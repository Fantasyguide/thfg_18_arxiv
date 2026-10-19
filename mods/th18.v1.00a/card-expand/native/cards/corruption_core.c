/* corruption_core.c —— 腐化（id 70）的纯逻辑。语义与一手对位表在 `corruption_core.h` 头注。 */
#include "corruption_core.h"

void corruption_death_window_open(corruption_death_window_t *w, int32_t lives, int32_t frames)
{
    if (!w) return;
    w->lives_snapshot = lives;
    w->frames_left = (frames > 0) ? frames : 0;
}

corruption_result_t corruption_death_window_tick(corruption_budget_t *b,
                                                 corruption_death_window_t *w,
                                                 int32_t *out_spent)
{
    int32_t spent;

    if (out_spent) *out_spent = 0;
    if (!b || !w || w->frames_left <= 0) return CORRUPTION_IDLE;

    /* ① 真死（残机变小）：`0x45d1a0 sub [0x4ccd48],1` 扣命，紧接着 `0x45d1e8` 把 CURRENT 重设成
     *    `clamp(BOMBS_PER_LIFE 0x4ccd60 = 3, 0, MAX)` ⇒ 那个 `MAX − CURRENT` 是**重生补给**，
     *    不是支出 ⇒ 关窗、不结算（否则每次真死都会白扣预算）。*/
    if (b->lives < w->lives_snapshot) {
        w->frames_left = 0;
        return CORRUPTION_REAL_DEATH;
    }

    /* ② 死亡回避的漏记支出：两处 `−1`（各钳 0 / MAX）⇒ 1 或 2。
     *    `0` = 按 B 自救 / 六文钱 / 113 救场 —— 那些路径 CURRENT 仍等于 MAX ⇒ 自动不重复记。
     *    `≥3` 不可能来自 `0x40a2a0`（它最多 −2）⇒ 一律不当支出（例如每关 `min(3,MAX)` 补给）。*/
    spent = b->max_bombs - b->current_bombs;
    if (spent == 1 || spent == 2) {
        b->max_bombs -= spent;      /* ★ 把漏记的支出记进预算 */
        b->current_bombs = b->max_bombs;
        /* 一次中弹只结算一次，结算完就关窗：
         * 之后的每一发都是本卡 `on_bomb_spent` 的正路（CURRENT 恒等于 MAX），
         * 下一次中弹 `+0x10`（`0x45d574`）必然重开一个新窗 —— 留着旧窗只会
         * 把别人的写者（如 79 旋风斩的 CURRENT 还原）误当成支出。*/
        w->frames_left = 0;
        if (out_spent) *out_spent = spent;
        return CORRUPTION_SETTLED;
    }

    /* ③ 还看不出支出：只数窗口，过期自动关（兜底，防跨命/跨关的陈旧快照）。*/
    w->frames_left--;
    return CORRUPTION_IDLE;
}
