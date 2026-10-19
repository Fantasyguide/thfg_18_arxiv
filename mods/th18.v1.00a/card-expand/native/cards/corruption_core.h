/* corruption_core.h —— 腐化（id 70）的纯逻辑：**死亡回避漏记的炸弹支出**该记多少。
 * 零引擎依赖 ⇒ 主机单测直接跑（`tests/test_corruption_core.c`）。
 *
 * ## 一手：这条支出现在为什么必须单独记账（th18 v1.00a，整条因果链见 `cards/corruption.c` 头注）
 * · `Card__death_save_bomb_revive` `0x40a2a0` 在 `0x40a3ed` 与 `0x40a42c` 各做一次
 *   「`CURRENT_BOMBS 0x4ccd58` − 1 → 钳 0 → 再钳 `MAX_BOMBS 0x4ccd64`」⇒ 一次死亡回避花 **1 或 2 发当前数**
 *   （`0x40a2a0` 全函数对 MAX **只有读**：`0x40a3d7` / `0x40a40c`，零写）；
 * · 它**不经过** `consume_bomb 0x4574d0` ⇒ 本卡挂在 `0x4203bc` 的 `on_bomb_spent` 抓不到；
 * · 卡侧每帧把 `CURRENT` 钉回 `MAX` ⇒ 不结算的话那 1–2 发下一帧就被"补回"、MAX 一分不掉。
 *
 * ## 口径（纯算术，三条）
 * ① 中弹那一刻用 `+0x10`（`Player__die` 末尾 `0x45d574`）拍**残机快照**并开窗；
 * ② 窗口内 `MAX − CURRENT ∈ {1,2}` ⇒ 那就是死亡回避的支出 ⇒ `MAX −= spent; CURRENT = MAX`。
 *    `0` = 按 B 自救 / 六文钱 / 113 救场等「CURRENT 仍等于 MAX」的路径 ⇒ **不重复记**（不加额外判据）；
 * ③ 残机**变小** = 真死（`0x45d1a0` 扣命、`0x45d1e8` 把 CURRENT 重设成
 *    `clamp(每条命炸弹数 0x4ccd60 = 3, 0, MAX)`）⇒ **不结算**，别把重生补给当成支出。
 */
#pragma once
#include <stdint.h>

/* 中弹窗口的帧数。原生决死窗 = 8 帧（`0x45d557` 写 `player+0x47908 = 8`），扣命在窗口耗尽之后
 * ⇒ 24 帧 = 3 倍余量（覆盖决死窗被延长 / 卡顿跳帧的场合）。 */
#define CORRUPTION_DEATH_WINDOW_FRAMES 24

/* 三个引擎全局的一份快照（调用方每帧从 CE_* 读进来、结算完写回去）。*/
typedef struct {
    int32_t max_bombs;      /* MAX_BOMBS 0x4ccd64 = 本卡的「预算」          */
    int32_t current_bombs;  /* CURRENT_BOMBS 0x4ccd58 = HUD 上那个数        */
    int32_t lives;          /* CURRENT_LIVES 0x4ccd48（只读：用来判真死）    */
} corruption_budget_t;

/* 一次中弹的窗口（调用方那三个 static）。*/
typedef struct {
    int32_t frames_left;    /* > 0 = 在一个中弹窗里；0 = 不在窗里 */
    int32_t lives_snapshot; /* 中弹那一刻（`+0x10`）的残机 */
} corruption_death_window_t;

typedef enum {
    CORRUPTION_IDLE = 0,        /* 窗口外 / 窗口内还看不出支出（按 B 自救、救援、还没决死）*/
    CORRUPTION_SETTLED = 1,     /* ★ 结算了 1 或 2 发：`b` 已被更新、`out_spent` 有效 */
    CORRUPTION_REAL_DEATH = 2   /* 残机减少 = 真死 ⇒ 关窗、不结算 */
} corruption_result_t;

/* 中弹瞬间（`+0x10`）：拍快照 + 开窗。只记数，不动任何引擎值。*/
void corruption_death_window_open(corruption_death_window_t *w, int32_t lives, int32_t frames);

/* 每帧一次（`on_tick_2` 里，**在钉不变式之前**调）。
 * 返回 CORRUPTION_SETTLED 时 `b->max_bombs` / `b->current_bombs` 已是结算后的值，
 * 调用方要把它们写回引擎、刷 HUD、记一行日志（`out_spent` = 1 或 2）。*/
corruption_result_t corruption_death_window_tick(corruption_budget_t *b,
                                                 corruption_death_window_t *w,
                                                 int32_t *out_spent);
