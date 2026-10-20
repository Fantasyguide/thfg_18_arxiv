/* 腐化（id 70）纯逻辑单测：**「死亡消耗了 B」记进预算**。
 * 语义与一手对位见 `cards/corruption_core.h` 头注、因果链见 `cards/corruption.c` 头注。
 *
 *   cc -std=c11 -Wall -Wextra -Werror -o tests/build/test_corruption_core \
 *      tests/test_corruption_core.c cards/corruption_core.c && ./tests/build/test_corruption_core
 */
#include <stdio.h>

#include "../cards/corruption_core.h"

static int fails, checks;
#define CHECK(cond, ...) do { checks++; if (!(cond)) { printf("FAIL %s:%d ", __FILE__, __LINE__); \
    printf(__VA_ARGS__); printf("\n"); fails++; } } while (0)

/* ① 原生死亡回避一次花 2 发（`0x40a3ed` + `0x40a42c`）：MAX 7 → 5，CURRENT 跟着 = 5（预算 −2）*/
static void test_death_save_costs_two(void)
{
    corruption_budget_t b = { 7, 5 };   /* 引擎扣完 CURRENT、MAX 还没动（站点 0x40a44b 那一刻）*/

    CHECK(corruption_charge_death_save(&b, 2) == 2, "该记 2 发");
    CHECK(b.max_bombs == 5, "MAX 该 7−2=5，得到 %d", (int)b.max_bombs);
    CHECK(b.current_bombs == 5, "CURRENT 该跟着 MAX=5，得到 %d", (int)b.current_bombs);
}

/* ② 按 B 自救 / 六文钱 / 113 救场：**没有死亡回避事件**（spent 0 / 负数）⇒ 一个数都不动 */
static void test_no_event_keeps_invariant(void)
{
    corruption_budget_t b = { 7, 7 };

    CHECK(corruption_charge_death_save(&b, 0) == 0, "spent=0 不该记");
    CHECK(corruption_charge_death_save(&b, -3) == 0, "负数不该记");
    CHECK(b.max_bombs == 7 && b.current_bombs == 7, "预算被动了（%d/%d）",
          (int)b.current_bombs, (int)b.max_bombs);
}

/* ③ 真死：**根本不会有事件**（死亡回避没救成才扣命）⇒ 预算不变。
 *    这里直接验「不调用就不动」，并验「引擎在真死时把 CURRENT 重设成 3」不会被本函数误当成支出。*/
static void test_real_death_has_no_event(void)
{
    corruption_budget_t b = { 7, 3 };   /* 引擎 0x45d1a0 扣命 + 0x45d1e8 把 CURRENT 重设成 clamp(3,0,MAX) */

    CHECK(corruption_charge_death_save(&b, 0) == 0, "真死不产生事件");
    CHECK(b.max_bombs == 7 && b.current_bombs == 3, "真死不该动预算（%d/%d）",
          (int)b.current_bombs, (int)b.max_bombs);
}

/* ④ owner 要的「更好玩」的账：7 点预算 ⇒ 连撞 3 次死亡回避（2+2+2）到 1/1，
 *    第 4 次只剩最后 1 发（`0x420420` 只查 CURRENT > 0 ⇒ 仍放行，钳到 0）⇒ 0/0；再撞就真死。*/
static void test_three_saves_then_one(void)
{
    corruption_budget_t b = { 7, 7 };
    int i;

    for (i = 0; i < 3; i++) {
        b.current_bombs -= 2;                       /* 引擎两处 −1（这里 CURRENT 正好跟 MAX 同步）*/
        CHECK(corruption_charge_death_save(&b, 2) == 2, "第 %d 次该记 2", i + 1);
    }
    CHECK(b.max_bombs == 1 && b.current_bombs == 1, "3 次之后该是 1/1，得到 %d/%d",
          (int)b.current_bombs, (int)b.max_bombs);

    b.current_bombs -= 1;                           /* 最后 1 发被扣掉（钳 0）*/
    CHECK(corruption_charge_death_save(&b, 1) == 1, "第 4 次该记 1");
    CHECK(b.max_bombs == 0 && b.current_bombs == 0, "预算该用光（%d/%d）",
          (int)b.current_bombs, (int)b.max_bombs);

    /* 0/0 之后引擎不再放行（can_bomb 查 CURRENT > 0）⇒ 没有事件；预算也不会变负 */
    CHECK(corruption_charge_death_save(&b, 2) == 0, "0/0 之后无可扣");
    CHECK(b.max_bombs == 0 && b.current_bombs == 0, "预算变负了（%d/%d）",
          (int)b.current_bombs, (int)b.max_bombs);
}

/* ⑤ 108 的「有 106 ⇒ 多扣 1」：卡侧上报 1 发 ⇒ 预算 −1（owner 点名的 106+108 那条路的一半）*/
static void test_card_side_note(void)
{
    corruption_budget_t b = { 7, 6 };   /* 108 已经把 CURRENT 扣掉 1 */

    CHECK(corruption_charge_death_save(&b, 1) == 1, "卡侧上报 1 发该记");
    CHECK(b.max_bombs == 6 && b.current_bombs == 6, "该记成 6/6，得到 %d/%d",
          (int)b.current_bombs, (int)b.max_bombs);
}

/* ⑥ 钳位：异常大的 spent 也只扣到 0，绝不出现负预算（负 MAX 会让 consume 的 min(cur,max) 把当前数写负）*/
static void test_clamp(void)
{
    corruption_budget_t b = { 2, 0 };

    CHECK(corruption_charge_death_save(&b, 5) == 2, "只该记 2（钳到 MAX）");
    CHECK(b.max_bombs == 0 && b.current_bombs == 0, "该是 0/0，得到 %d/%d",
          (int)b.current_bombs, (int)b.max_bombs);
}

int main(void)
{
    test_death_save_costs_two();
    test_no_event_keeps_invariant();
    test_real_death_has_no_event();
    test_three_saves_then_one();
    test_card_side_note();
    test_clamp();

    if (fails) { printf("test_corruption_core: %d FAIL\n", fails); return 1; }
    printf("test_corruption_core: %d checks OK\n", checks);
    return 0;
}
