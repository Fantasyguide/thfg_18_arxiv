/* 鞘翅重锤（id 91）纯逻辑单测：时机条状态机（激活帧丢 C / 没敌人不动 / 卡准=HIT / 卡歪=MISS /
 * 3s 没敌人=TIMEOUT / 3s 总预算=TIMEOUT / 砸完 RETURN / 缓冲走完 DONE / 无敌帧声明单调）。
 *   cc -std=c11 -Wall -Wextra -Werror -o t tests/test_elytra_mace_core.c cards/elytra_mace_core.c && ./t
 * 规则与一手口径见 `cards/elytra_mace_core.h`。 */
#include <assert.h>
#include <math.h>
#include <stdio.h>

#include "../cards/elytra_mace_core.h"

static int fails;
#define CHECK(cond, ...) do { if (!(cond)) { printf("FAIL %s:%d ", __FILE__, __LINE__); \
    printf(__VA_ARGS__); printf("\n"); fails++; } } while (0)

/* 跑到「光标进入窗口」那一帧（返回 1 = 成功等到）；`c_edge` 一直 0（不按 C）。*/
static int run_until_in_window(em_state_t *s, int has_target)
{
    int i;
    for (i = 0; i < EM_AIM_FRAMES; ++i) {
        int ev = em_tick(s, has_target, 0);
        CHECK(ev == EM_EV_NONE, "没按 C 却出了事件 %d", ev);
        if (em_in_window(s)) return 1;
    }
    return 0;
}

static void test_begin_and_first_frame(void)
{
    em_state_t s;

    em_reset(&s);
    CHECK(s.state == EM_IDLE, "初始该是 IDLE");
    CHECK(em_invuln_left(&s) == 0, "IDLE 不该声明无敌");

    em_begin(&s);
    CHECK(s.state == EM_AIM, "begin 之后该进 AIM");
    CHECK(s.cursor == -EM_BAR_HALF && s.dir > 0.0f, "光标该从左端往右出发");
    CHECK(em_invuln_left(&s) > EM_AIM_FRAMES, "AIM 里声明的无敌该覆盖 3s + 砸击 + 缓冲");

    /* ★ 激活那一帧的 C 边沿必须被丢掉（否则同一次按键自己确认自己）*/
    CHECK(em_tick(&s, 1, 1) == EM_EV_NONE, "激活帧的 C 不该触发确认");
    CHECK(s.state == EM_AIM, "激活帧还是 AIM");
}

static void test_cursor_only_moves_with_target(void)
{
    em_state_t s;
    float before;

    em_reset(&s);
    em_begin(&s);
    em_tick(&s, 0, 0);                     /* 解锁 */
    before = s.cursor;
    for (int i = 0; i < 30; ++i) em_tick(&s, 0, 0);
    CHECK(s.cursor == before, "没有可攻击敌人时光标该停着（%g -> %g）", (double)before, (double)s.cursor);
    CHECK(s.no_target >= 30, "「没有敌人」的计时该在跑，得到 %u", s.no_target);

    em_tick(&s, 1, 0);
    CHECK(s.cursor > before, "有敌人了光标该动起来");
    CHECK(s.no_target == 0, "有敌人该把 no_target 清零");
}

static void test_hit_and_miss(void)
{
    em_state_t s;

    /* 卡准：跑到窗口里再按 */
    em_reset(&s);
    em_begin(&s);
    em_tick(&s, 1, 0);                     /* 解锁帧 */
    CHECK(run_until_in_window(&s, 1), "光标一直没进过窗口（参数被改了？）");
    CHECK(em_tick(&s, 1, 1) == EM_EV_HIT, "窗口内按 C 该是 HIT");
    CHECK(s.state == EM_SMASH && s.hits == 1, "HIT 之后该进 SMASH");

    /* 砸完 ⇒ RETURN ⇒ 缓冲 ⇒ DONE */
    {
        int ev = EM_EV_NONE, i, saw_return = 0;
        for (i = 1; i < EM_SMASH_FRAMES; ++i) {
            ev = em_tick(&s, 0, 0);
            CHECK(ev == EM_EV_NONE, "砸击第 %d 帧不该出事件（得到 %d）", i, ev);
        }
        ev = em_tick(&s, 0, 0);
        CHECK(ev == EM_EV_RETURN, "砸完那一帧该 RETURN，得到 %d", ev);
        saw_return = 1;
        CHECK(s.state == EM_EXIT, "RETURN 之后该进 EXIT");
        for (i = 1; i < EM_EXIT_INVULN; ++i) em_tick(&s, 0, 0);
        ev = em_tick(&s, 0, 0);
        CHECK(ev == EM_EV_DONE && s.state == EM_IDLE, "缓冲走完该 DONE + IDLE（得到 %d）", ev);
        CHECK(saw_return, "（占位）");
        CHECK(em_invuln_left(&s) == 0, "回 IDLE 后不该再声明无敌");
    }

    /* 卡歪：解锁后立刻按（光标还在最左端 ⇒ 窗口外）*/
    em_reset(&s);
    em_begin(&s);
    em_tick(&s, 1, 0);
    CHECK(em_tick(&s, 1, 1) == EM_EV_MISS, "窗口外按 C 该 MISS");
    CHECK(s.state == EM_EXIT && s.misses == 1, "MISS 之后该进 EXIT");
    CHECK(!em_in_window(&s), "最左端当然不在窗口里（这条同时验证窗口判据）");
}

static void test_timeouts(void)
{
    em_state_t s;
    int i, ev = EM_EV_NONE;

    /* 3s 没有可攻击的敌人 ⇒ TIMEOUT */
    em_reset(&s);
    em_begin(&s);
    for (i = 0; i < EM_NO_TARGET_FRAMES; ++i) {
        ev = em_tick(&s, 0, 0);
        if (ev) break;
    }
    CHECK(ev == EM_EV_TIMEOUT, "3s 没敌人该 TIMEOUT，第 %d 帧得到 %d", i, ev);
    CHECK(s.state == EM_EXIT && s.timeouts == 1, "TIMEOUT 之后该进 EXIT");

    /* 一直有敌人但不按 ⇒ 总预算到（也是 3s）*/
    em_reset(&s);
    em_begin(&s);
    for (i = 0; i < EM_AIM_FRAMES + 5; ++i) {
        ev = em_tick(&s, 1, 0);
        if (ev) break;
    }
    CHECK(ev == EM_EV_TIMEOUT, "总预算到该 TIMEOUT（第 %d 帧得到 %d）", i, ev);
    CHECK(s.timeouts == 1, "计数该 +1");

    /* 有敌人时 no_target 会清零 ⇒ 不能靠它误退出 */
    em_reset(&s);
    em_begin(&s);
    for (i = 0; i < EM_NO_TARGET_FRAMES * 2 && i < EM_AIM_FRAMES - 1; ++i) {
        ev = em_tick(&s, (i % 3) ? 1 : 0, 0);      /* 断断续续有敌人 */
        CHECK(ev == EM_EV_NONE, "不该退出（第 %d 帧得到 %d）", i, ev);
    }
}

static void test_invuln_monotone(void)
{
    em_state_t s;
    int prev, now, i;

    em_reset(&s);
    em_begin(&s);
    em_tick(&s, 1, 0);
    prev = em_invuln_left(&s);
    for (i = 0; i < 60; ++i) {
        em_tick(&s, 1, 0);
        now = em_invuln_left(&s);
        CHECK(now <= prev, "AIM 里声明的无敌帧该单调不增（%d -> %d）", prev, now);
        CHECK(now > 0, "AIM 里不该出现 0");
        prev = now;
    }
    /* 卡准 ⇒ SMASH 期间还要有砸击 + 缓冲 */
    while (!em_in_window(&s)) em_tick(&s, 1, 0);
    CHECK(em_tick(&s, 1, 1) == EM_EV_HIT, "该 HIT");
    CHECK(em_invuln_left(&s) >= EM_EXIT_INVULN, "SMASH 第一帧的声明该含缓冲（得到 %d）",
          em_invuln_left(&s));
    for (i = 0; i < EM_SMASH_FRAMES + EM_EXIT_INVULN + 4; ++i) em_tick(&s, 0, 0);
    CHECK(em_invuln_left(&s) == 0, "全走完该是 0");
}

int main(void)
{
    test_begin_and_first_frame();
    test_cursor_only_moves_with_target();
    test_hit_and_miss();
    test_timeouts();
    test_invuln_monotone();
    printf(fails ? "elytra_mace_core: %d FAILED\n" : "elytra_mace_core: ok\n", fails);
    return fails ? 1 : 0;
}
