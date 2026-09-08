/* 旋风斩（id 79）纯逻辑单测：X 的算法 + 接续状态机。
 *   cc -std=c11 -Wall -Wextra -Werror -o t tests/test_whirlwind_core.c cards/whirlwind_core.c && ./t */
#include <stdio.h>

#include "../cards/whirlwind_core.h"

#define PER 100                      /* CE_POWER_PER_LEVEL */

static int fails;
#define CHECK(cond, ...) do { if (!(cond)) { printf("FAIL %s:%d ", __FILE__, __LINE__); \
    printf(__VA_ARGS__); printf("\n"); fails++; } } while (0)

/* X = ⌊火力⌋ − 1，钳进 [0, 3] */
static void test_extra(void)
{
    struct { int power, want; } cases[] = {
        {   0, 0 }, {   1, 0 }, {  99, 0 },      /* 不足一档 */
        { 100, 0 }, { 150, 0 }, { 199, 0 },      /* 正好 / 刚过一档：没得接 */
        { 200, 1 }, { 250, 1 }, { 299, 1 },
        { 300, 2 }, { 350, 2 },
        { 400, 3 },
        { 500, 3 }, { 9999, 3 },                 /* 异常大的火力也钳在 WW_MAX_EXTRA */
        {  -5, 0 },                              /* 负数不炸 */
    };
    for (unsigned i = 0; i < sizeof cases / sizeof cases[0]; ++i)
        CHECK(ww_extra(cases[i].power, PER) == cases[i].want,
              "ww_extra(%d) = %d，应为 %d", cases[i].power, ww_extra(cases[i].power, PER), cases[i].want);
    CHECK(ww_extra(400, 0) == 0, "per_level 为 0 时应返回 0，不该除零");
    CHECK(ww_extra(400, -1) == 0, "per_level 为负时应返回 0");
}

/* 扣完之后必然剩 [一档, 两档)：引擎 spend_power 的拒绝分支永远碰不到 */
static void test_cost_never_refused(void)
{
    for (int power = 0; power <= 400; ++power) {
        int x = ww_extra(power, PER);
        if (!x) continue;
        int left = power - x * PER;
        CHECK(left >= PER, "火力 %d 扣 %d 档后只剩 %d，低于一档（引擎会拒绝）", power, x, left);
        CHECK(left < 2 * PER, "火力 %d 扣 %d 档后剩 %d，还够再接一次（X 算小了）", power, x, left);
    }
}

/* 一次完整的四连：4.00 火力 → 玩家那发 + 接 3 发 */
static void test_full_chain(void)
{
    ww_state_t s = { 0, 0, 0, 0, 0 };
    int x = ww_on_bomb(&s, 400, PER);
    CHECK(x == 3 && s.left == 3, "4.00 火力应接 3 发，得到 x=%d left=%u", x, s.left);

    /* 玩家那发正在放：do_bomb 先置「正在放」再扣炸弹，所以算 X 那一刻 bombing 已是 1 */
    CHECK(ww_tick(&s, 1) == 0, "上一发还在放时不该抢跑");

    for (int i = 0; i < 3; ++i) {
        CHECK(ww_tick(&s, 0) == 1, "第 %d 发该放了", i + 1);
        CHECK(s.chaining == 1, "调 do_bomb 之前 chaining 必须已经置好（on_bomb_spent 在它内部触发）");
        CHECK(ww_on_bomb(&s, 100, PER) == 0, "接出来的这发不该再算 X");
        ww_fired(&s, 1);
        CHECK(ww_tick(&s, 1) == 0, "这发正在放，不该再放");
    }
    CHECK(s.left == 0 && s.chained == 3, "应正好接 3 发，得到 left=%u chained=%u", s.left, s.chained);
    CHECK(ww_tick(&s, 0) == 0, "放完了不该继续");
    CHECK(s.chaining == 0, "放完之后 chaining 要清掉，否则下一发真炸弹算不出 X");

    /* 下一发真炸弹：火力已经被扣到 1.00，X = 0 —— 自带终止 */
    CHECK(ww_on_bomb(&s, 100, PER) == 0, "1.00 火力不该再接");
}

/* 引擎守卫拦下时：欠账留着重试，但不会无限试 */
static void test_refused_then_give_up(void)
{
    ww_state_t s = { 0, 0, 0, 0, 0 };
    ww_on_bomb(&s, 300, PER);
    CHECK(s.left == 2, "3.00 火力应接 2 发");

    CHECK(ww_tick(&s, 0) == 1, "该放第 1 发");
    ww_fired(&s, 0);                                   /* 被拦下 */
    CHECK(s.left == 2 && s.chained == 0, "拦下时欠账不该减");
    CHECK(ww_tick(&s, 0) == 1, "下一帧该重试");
    ww_fired(&s, 1);
    CHECK(s.left == 1 && s.chained == 1, "成功后欠账 −1");

    /* 上一发成功把 waited 清了零，所以要再攒满 WW_GIVE_UP 帧、下一帧才作废 */
    for (int i = 0; i < WW_GIVE_UP; ++i) { ww_tick(&s, 0); ww_fired(&s, 0); }
    CHECK(s.left == 1, "正好第 WW_GIVE_UP 帧还不该作废（边界），left=%u", s.left);
    ww_tick(&s, 0);
    CHECK(s.left == 0, "等满 %d 帧的下一帧就该作废，left=%u", WW_GIVE_UP, s.left);
    CHECK(s.chaining == 0, "作废时 chaining 要一起清");
}

/* 成功一发就把等待计时清零：长链不会被前面攒的帧数误判成「放不出去」 */
static void test_waited_resets_on_success(void)
{
    ww_state_t s = { 0, 0, 0, 0, 0 };
    ww_on_bomb(&s, 400, PER);
    for (int i = 0; i < WW_GIVE_UP - 1; ++i) { ww_tick(&s, 0); ww_fired(&s, 0); }
    CHECK(s.left == 3, "还没到止损点，欠账应该还在");
    ww_tick(&s, 0);
    ww_fired(&s, 1);
    CHECK(s.waited == 0, "成功一发之后等待计时要清零，得到 %u", s.waited);
    CHECK(s.left == 2, "成功一发欠账应为 2，得到 %u", s.left);
}

/* 关卡开场 / 局末的清理：欠账与 chaining 都不许带过去 */
static void test_clear(void)
{
    ww_state_t s = { 0, 0, 0, 0, 0 };
    ww_on_bomb(&s, 400, PER);
    ww_tick(&s, 0);
    CHECK(s.left == 3 && s.chaining == 1, "先造出一个「欠着 3 发、正在接」的状态");
    ww_clear(&s);
    CHECK(s.left == 0 && s.chaining == 0 && s.waited == 0, "ww_clear 应把状态机清干净");
    CHECK(ww_tick(&s, 0) == 0, "清完不该再放");
}

int main(void)
{
    test_extra();
    test_cost_never_refused();
    test_full_chain();
    test_refused_then_give_up();
    test_waited_resets_on_success();
    test_clear();
    printf("test_whirlwind_core: %s\n", fails ? "FAILED" : "OK");
    return fails ? 1 : 0;
}
