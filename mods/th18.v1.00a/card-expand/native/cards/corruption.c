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
 * 【删卡】它改的是全局上限（一局内不重置）⇒ on_delete 里必须把上限还原，见文件末的 on_delete 注释。
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

/* ★ 2026-09-12（删卡收尾审计，铁律 METHOD.md「长持续效果的卡必须交代『被删掉会怎样』」）：**本卡留下的
 * 全局旋钮必须在 on_delete（虚表 +0x50）里还原**。它是那七步清单里的第 7 类（全局旋钮）唯一漏网的一张 ——
 * 既不每帧重写、也没走 SDK 的按卡聚合，删卡路径上就没人收。
 *
 * 【为什么删卡路径会漏】`MAX_BOMBS`（`0x4ccd64`）是**一局内持续累积**的量：三处 `mov [0x4ccd64],7`
 * （`0x4428ed` / `0x442920` / `0x442957`，AUDIT §T8）只在**开局初始化**跑，每关开场 `0x442745`–`0x44278f`
 * 写的是 `CURRENT = min(3, MAX)`、**不碰 MAX**。而本卡把「放一发炸弹」记成 `MAX −= 1`。
 * 倒吊人（74）/ 原版空白卡那次删卡都发生在**商店里**，那一刻 `AbilityManager__on_tick` 被
 * `ABILITY_SHOP_PTR` 冻住 ⇒ `on_bomb_spent` / `on_tick_2` 当场停摆，`on_run_reset` 要等局末，
 * **只有这里跑得到**。不收的后果：扣掉的那几格上限**再也没人还**，整局剩下的关卡上限一直偏低
 * （例：用掉 3 发后删卡 ⇒ 后面每关都是 `3/4` 而不是 `3/7`），HUD 也一直是错的。
 *
 * 【为什么写回 7 就是精确撤销】本卡 ctor 在**取得那一刻**把 MAX 写成 7（`CORRUPTION_MAX_BOMBS`），
 * 此后同一局内只有本卡会减它 —— 引擎侧只有 `CardBomb__destructor` `0x409C20` 的 +1 与那三处开局 =7，
 * 皇家同花顺（58–62）的 +1/+2 也只是往上加且被 `< 7` 钳住 ⇒ MAX 恒 ≤ 7，写回 7 不会超过任何别的卡
 * 给过的上限，也不需要额外记账。**当前数不动**：那是玩家已经用掉的预算，不退还；删卡时不变式
 * `CURRENT == MAX` 仍成立（商店里没人动炸弹），下一个关卡开场引擎自己会把它压到 `min(3, 7)`。
 * `on_run_reset` 不补：局末之后引擎在每局开头的初始化里自己把 MAX 写回 7，这里再写一次没有意义。*/
static void on_delete(ce_card_t *c)
{
    (void)c;
    if (CE_MAX_BOMBS() == CORRUPTION_MAX_BOMBS) return;
    ce_log("corruption: dismissed (card destroyed) — bomb cap %d -> %d (engine default), count %d kept",
           CE_MAX_BOMBS(), CORRUPTION_MAX_BOMBS, CE_CURRENT_BOMBS());
    CE_MAX_BOMBS() = CORRUPTION_MAX_BOMBS;
    if (CE_CURRENT_BOMBS() > CE_MAX_BOMBS()) CE_CURRENT_BOMBS() = CE_MAX_BOMBS();
    sync_hud();
}

CE_CARD(70, .ctor = ctor, .on_bomb_spent = on_bomb_spent, .on_stage_start = on_stage_start,
            .on_tick_2 = on_tick_2, .on_delete = on_delete);
