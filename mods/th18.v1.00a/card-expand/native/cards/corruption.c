/* 腐化（id 70）—— 放炸弹不再消耗炸弹数，而是消耗**上限**。
 *
 * 拿到卡时上限顶满（零售上限 7），此后每放一发上限 −1；当前数跟着上限走，
 * 所以 HUD 上那个数字**就是剩余预算**：一次给满七发，用一发少一发，过关也不回复。
 *
 * 【为什么挂断点而不是 on_tick_2】
 * consume_bomb `0x4574d0` 全库只有 `do_bomb` `0x4203b7` 一个调用方，所以断点挂在它**刚返回**
 * 的那一条（`0x4203bc`，`a1 c0 f2 4c 00`，5 字节绝对寻址、无相对量）就不多不少地覆盖每一次
 * 炸弹消耗，而且**同一帧**改完 —— 用 on_tick_2 的边沿要等到下一帧，中间会闪一格。
 *
 * 【为什么要接管每关开场】
 * `GameThread__thread_start` 在 `0x44274a`–`0x44278f` 把当前数刷成 `min(3, 上限)`，
 * 会把预算显示压回 3。`on_stage_start`（`+0x34`，同一函数 `0x442F98` 广播、在那之后）
 * 把当前数拉回 = 上限。**它不加东西，只是不让引擎的补给把预算显示改小。**
 *
 * 【撤销扣除为什么是精确的】
 * 进 do_bomb 前 can_bomb 已保证 `CURRENT >= 1`，且不变式 `CURRENT <= MAX` 成立，
 * 所以 consume 里的两次钳位（`< 0 → 0`、`min(cur,max)`）都不会触发 —— 扣完必然正好是
 * `CURRENT-1`，`+= 1` 就是精确还原。
 */
#include "sdk.h"

#define CORRUPTION_MAX_BOMBS 7          /* 零售上限（0x4428ed 等三处 mov [0x4ccd64],7）*/

static void sync_hud(void) { ce_gui_update_bombs(); }

static int ctor(ce_card_t *c)
{
    if (!ce_fresh_acquire(c)) return 0;             /* 每关开场引擎会再调一次 ctor，那次不算新获得 */
    CE_MAX_BOMBS() = CORRUPTION_MAX_BOMBS;
    CE_CURRENT_BOMBS() = CE_MAX_BOMBS();
    sync_hud();
    ce_log("corruption: acquired — bombs %d/%d (from now on a bomb spends the MAX, not the count)",
           CE_CURRENT_BOMBS(), CE_MAX_BOMBS());
    return 0;
}

static void on_bomb_spent(ce_card_t *c)
{
    (void)c;
    CE_CURRENT_BOMBS() += 1;                        /* 精确还原刚才那一下 */
    if (CE_MAX_BOMBS() > 0) CE_MAX_BOMBS() -= 1;    /* 改成扣上限，下限 0 */
    if (CE_CURRENT_BOMBS() > CE_MAX_BOMBS()) CE_CURRENT_BOMBS() = CE_MAX_BOMBS();
    sync_hud();
    ce_log("corruption: bomb spent the cap — bombs %d/%d left",
           CE_CURRENT_BOMBS(), CE_MAX_BOMBS());
}

/* 每关开场：不让引擎的 min(3, 上限) 补给把预算显示压小 */
static void on_stage_start(ce_card_t *c)
{
    (void)c;
    if (CE_CURRENT_BOMBS() != CE_MAX_BOMBS()) {
        CE_CURRENT_BOMBS() = CE_MAX_BOMBS();
        sync_hud();
    }
}

/* 撞了之后失效的根因（2026-09-09 用户实跑）：死亡/复活路径会写 CURRENT_BOMBS
 *（`0x40a3d0` 一带的碎片进位、`0x44278f` 的每关 min(3,上限) 补给），把不变式
 *「CURRENT == MAX = 剩余预算」打破 —— 之后 on_bomb_spent 的 +1 还原就落在错的基准上，
 * 预算显示与扣减全乱。每帧把不变式拉回来（这张卡的模型就是 CURRENT 恒等于 MAX）。*/
static int on_tick_2(ce_card_t *c)
{
    (void)c;
    if (CE_CURRENT_BOMBS() != CE_MAX_BOMBS()) {
        CE_CURRENT_BOMBS() = CE_MAX_BOMBS();
        sync_hud();
    }
    return 0;
}

CE_CARD(70, .ctor = ctor, .on_bomb_spent = on_bomb_spent, .on_stage_start = on_stage_start,
            .on_tick_2 = on_tick_2);
