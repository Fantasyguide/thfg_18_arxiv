/* corruption_core.h —— 腐化（id 70）的纯逻辑：**「死亡消耗了 B」该怎么记进预算**。
 * 零引擎依赖 ⇒ 主机单测直接跑（`tests/test_corruption_core.c`）。
 *
 * ## 一手（th18 v1.00a，整条因果链见 `cards/corruption.c` 头注）
 * · `Card__death_save_bomb_revive` `0x40a2a0` 在 `0x40a3ed` 与 `0x40a42c` 各做一次
 *   「`CURRENT_BOMBS 0x4ccd58` − 1 → 钳 0 → 再钳 `MAX_BOMBS 0x4ccd64`」⇒ 一次死亡回避花 **1 或 2 发**；
 *   **全函数对 MAX 只有读，零写**（`0x40a3d7` / `0x40a40c`）；
 * · 它**不经过** `consume_bomb 0x4574d0` ⇒ `on_bomb_spent` 抓不到；
 * · 卡侧每帧把 `CURRENT` 钉回 `MAX` ⇒ 不记账的话那 1–2 发下一帧就被"补回"、MAX 一分不掉。
 *
 * ## 口径（owner 2026-10-19：「**切实记录死秽回避消耗的 B 数，腐化只动上限**」）
 * 事件里给的是 `spent` = 这一次**已经真的从 CURRENT 扣掉**的发数（1 或 2）⇒ 本函数只做两件事：
 *   ① `max_bombs -= spent`（钳 ≥ 0）；
 *   ② `current_bombs = max_bombs`（当前数 = 新预算 ⇒ 本卡「CURRENT 恒等于 MAX」的不变式立刻恢复）。
 * 返回**实际**记进预算的发数（钳位之后；0 = 什么都没动，调用方据此决定要不要刷 HUD / 记日志）。
 */
#pragma once
#include <stdint.h>

/* 三个引擎全局在本卡里的模型（调用方每次从 CE_* 读进来、结算完写回去）。*/
typedef struct {
    int32_t max_bombs;      /* MAX_BOMBS 0x4ccd64 = 本卡的「预算」   */
    int32_t current_bombs;  /* CURRENT_BOMBS 0x4ccd58 = HUD 上那个数 */
} corruption_budget_t;

/* 把一次「死亡消耗了 `spent` 发 B」记进预算。返回实际扣掉的发数（0 = 没动）。
 * `spent <= 0`（按 B 自救 / 六文钱 / 113 救场 —— 那些路径不产生本事件）⇒ 不动。*/
int32_t corruption_charge_death_save(corruption_budget_t *b, int32_t spent);
