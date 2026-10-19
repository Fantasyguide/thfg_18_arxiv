/* 腐化（id 70）纯逻辑单测：**死亡回避漏记的炸弹支出**结算。
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

/* ① 死亡回避一次花 2 发（`0x40a3ed` + `0x40a42c`）：MAX 7 → 5，CURRENT 跟着 = 5（预算 −2） */
static void test_death_save_costs_two(void)
{
    corruption_budget_t b = { 7, 5, 3 };
    corruption_death_window_t w = { 0, 0 };
    int32_t spent = -1;
    corruption_result_t r;

    corruption_death_window_open(&w, 3, CORRUPTION_DEATH_WINDOW_FRAMES);
    CHECK(w.frames_left == 24 && w.lives_snapshot == 3, "开窗不对（%d, %d）",
          (int)w.frames_left, (int)w.lives_snapshot);

    r = corruption_death_window_tick(&b, &w, &spent);
    CHECK(r == CORRUPTION_SETTLED, "残机没变 + CURRENT 少了 2 ⇒ 该结算（r=%d）", (int)r);
    CHECK(spent == 2, "spent 该是 2，得到 %d", (int)spent);
    CHECK(b.max_bombs == 5, "MAX 该 7−2=5，得到 %d", (int)b.max_bombs);
    CHECK(b.current_bombs == 5, "CURRENT 该跟着 MAX=5，得到 %d", (int)b.current_bombs);
    CHECK(w.frames_left == 0, "结算完该关窗（一次中弹只结算一次），得到 %d", (int)w.frames_left);

    /* 结算完再 tick 一次：不许重复记（窗口已关）*/
    spent = -1;
    CHECK(corruption_death_window_tick(&b, &w, &spent) == CORRUPTION_IDLE, "关窗后不该再结算");
    CHECK(spent == 0 && b.max_bombs == 5 && b.current_bombs == 5, "重复结算了！（%d/%d, spent=%d）",
          (int)b.current_bombs, (int)b.max_bombs, (int)spent);
}

/* ② 按 B 自救 / 六文钱 / 113 救场：CURRENT 仍 == MAX ⇒ spent == 0 ⇒ 一个数都不动 */
static void test_rescue_keeps_invariant(void)
{
    corruption_budget_t b = { 7, 7, 3 };
    corruption_death_window_t w = { 0, 0 };
    int32_t spent = -1;

    corruption_death_window_open(&w, 3, CORRUPTION_DEATH_WINDOW_FRAMES);
    CHECK(corruption_death_window_tick(&b, &w, &spent) == CORRUPTION_IDLE, "spent=0 不该结算");
    CHECK(spent == 0, "spent 该 0，得到 %d", (int)spent);
    CHECK(b.max_bombs == 7 && b.current_bombs == 7, "按 B 自救不该动预算（%d/%d）",
          (int)b.current_bombs, (int)b.max_bombs);
    CHECK(w.frames_left == CORRUPTION_DEATH_WINDOW_FRAMES - 1, "窗口该照常倒数，得到 %d",
          (int)w.frames_left);
}

/* ③ 真死（残机 −1，引擎同时把 CURRENT 重设成 clamp(3,0,MAX)）⇒ 不结算、关窗
 *    —— 否则每次真死都会白扣预算（那个 MAX−CURRENT 是重生补给，不是支出）*/
static void test_real_death_does_not_settle(void)
{
    corruption_budget_t b = { 7, 3, 2 };        /* 引擎 0x45d1e8 刚把 CURRENT 写成 3 */
    corruption_death_window_t w = { 0, 0 };
    int32_t spent = -1;

    corruption_death_window_open(&w, 3, CORRUPTION_DEATH_WINDOW_FRAMES);
    CHECK(corruption_death_window_tick(&b, &w, &spent) == CORRUPTION_REAL_DEATH, "残机 3→2 该判真死");
    CHECK(spent == 0, "真死不该有 spent，得到 %d", (int)spent);
    CHECK(b.max_bombs == 7 && b.current_bombs == 3, "真死不该动预算（%d/%d）",
          (int)b.current_bombs, (int)b.max_bombs);
    CHECK(w.frames_left == 0, "真死该关窗，得到 %d", (int)w.frames_left);

    /* 关窗后 CURRENT 再被谁写小也不再结算 */
    b.current_bombs = 5;
    CHECK(corruption_death_window_tick(&b, &w, &spent) == CORRUPTION_IDLE, "真死之后不该再结算");
    CHECK(b.max_bombs == 7, "真死之后 MAX 被改了（%d）", (int)b.max_bombs);
}

/* ④ 窗口过期 ⇒ 不结算（陈旧快照不许把之后别人的写当成支出）*/
static void test_window_expiry(void)
{
    corruption_budget_t b = { 7, 7, 3 };
    corruption_death_window_t w = { 0, 0 };
    int32_t spent = -1;
    int i;

    corruption_death_window_open(&w, 3, CORRUPTION_DEATH_WINDOW_FRAMES);
    for (i = 0; i < CORRUPTION_DEATH_WINDOW_FRAMES; i++) {
        CHECK(corruption_death_window_tick(&b, &w, &spent) == CORRUPTION_IDLE,
              "第 %d 帧不该结算", i + 1);
        CHECK(spent == 0, "第 %d 帧 spent 该 0", i + 1);
    }
    CHECK(w.frames_left == 0, "24 帧之后窗口该过期，得到 %d", (int)w.frames_left);

    b.current_bombs = 5;                        /* 过期之后才出现"少了 2 发" */
    CHECK(corruption_death_window_tick(&b, &w, &spent) == CORRUPTION_IDLE, "窗口过期后不该结算");
    CHECK(b.max_bombs == 7 && b.current_bombs == 5, "窗口过期后不该动预算（%d/%d）",
          (int)b.current_bombs, (int)b.max_bombs);
    CHECK(spent == 0, "过期后 spent 该 0，得到 %d", (int)spent);
}

/* ⑤ 边界：只剩 1 发时死亡回避（`0x420420` 只查 CURRENT > 0 ⇒ 放行，钳到 0）⇒ spent = 1 */
static void test_last_bomb(void)
{
    corruption_budget_t b = { 1, 0, 3 };        /* MAX=1，唯一那发被扣掉 */
    corruption_death_window_t w = { 0, 0 };
    int32_t spent = 0;

    corruption_death_window_open(&w, 3, CORRUPTION_DEATH_WINDOW_FRAMES);
    CHECK(corruption_death_window_tick(&b, &w, &spent) == CORRUPTION_SETTLED, "spent=1 该结算");
    CHECK(spent == 1 && b.max_bombs == 0 && b.current_bombs == 0, "预算该用光（%d/%d, spent=%d）",
          (int)b.current_bombs, (int)b.max_bombs, (int)spent);
}

/* ⑥ 边界：每关 `min(3, MAX)` 补给（CURRENT 比 MAX 小 4）**不是**死亡回避的支出 ⇒ 不结算 */
static void test_stage_refill_is_not_a_charge(void)
{
    corruption_budget_t b = { 7, 3, 3 };
    corruption_death_window_t w = { 0, 0 };
    int32_t spent = -1;

    corruption_death_window_open(&w, 3, CORRUPTION_DEATH_WINDOW_FRAMES);
    CHECK(corruption_death_window_tick(&b, &w, &spent) == CORRUPTION_IDLE,
          "MAX−CURRENT = 4 不可能是死亡回避（最多 −2）⇒ 不该结算");
    CHECK(spent == 0 && b.max_bombs == 7 && b.current_bombs == 3, "不该动预算（%d/%d）",
          (int)b.current_bombs, (int)b.max_bombs);
}

/* ⑦ 没开窗（还没中弹）⇒ 一律不动 */
static void test_no_window(void)
{
    corruption_budget_t b = { 7, 5, 3 };
    corruption_death_window_t w = { 0, 0 };
    int32_t spent = -1;

    CHECK(corruption_death_window_tick(&b, &w, &spent) == CORRUPTION_IDLE, "没开窗不该结算");
    CHECK(spent == 0 && b.max_bombs == 7 && b.current_bombs == 5, "没开窗却动了预算");
}

int main(void)
{
    test_death_save_costs_two();
    test_rescue_keeps_invariant();
    test_real_death_does_not_settle();
    test_window_expiry();
    test_last_bomb();
    test_stage_refill_is_not_a_charge();
    test_no_window();

    if (fails) { printf("test_corruption_core: %d FAIL\n", fails); return 1; }
    printf("test_corruption_core: %d checks OK\n", checks);
    return 0;
}
