/* 119「调试・定向删卡」—— **调试用主动卡**（临时工具；owner 2026-10-14 需求 A1）
 *
 * owner 原话（逐字）：「最好是能弄一张**主动卡**，类似于挑战：十二合一，效果是**删卡**，
 *   debug 期间的**定向删卡**」✓ —— 起因是 A1 那条实跑（"用倒吊人删指定卡"）做不到 ✗。
 * owner 追加的设计（逐字）：「添加 hud，按 **A S** 左右切换目标，**空格键删除**
 *   （这些按键都是 th18 不会读的，无冲突风险，但**需要提前摘除所有卡的调试空格键**）」✓
 *   ⇒ 前置条件已由 `CE_DEBUG_SPACE_KEYS 0`（`sdk.h`）统一完成 ✓（全仓 7 文件 13 处空格读点已惰性化 ✓）。
 *
 * ## 怎么用
 *   · `A` / `S` = 目标在本局**在场卡**里左右移动（按卡号升序成环 ✓；跳过本卡自己 ✓）；
 *   · **`SPACE`** = 删除当前目标 ✓（owner 指定的键 ✓）；
 *   · **`C`**（主动卡键，`D` 切到本卡后）= 同一个动作 ✓（既然是"主动卡"，它的"效果"就是删卡 ✓）。
 *   · 左下角一行 HUD 显示：目标卡号 / 卡名 / 位次 / 操作提示 ✓。
 *
 * ## 机制（**0 条新增站点** ✓ R5）
 *   · 目标 = 走**卡链表**（与 `ce_pick_random_card()` `sdk.c:3310` 同一条路 ✓），
 *     取 `ce_card_id()` 排序（本卡自己排除 ✗）；
 *   · 删除 = SDK 既有的 **`ce_destroy_card(ce_card_t *)`** ✓（`sdk/public.h:266`；倒吊人
 *     `cards/hanged.c:18` 同款 ✓）⇒ 它会摘空 `mgr+0x38` 槽、并让引擎 `0x408b00(-1)` 重选刷 HUD ✓。
 *   · 键 = `ce_key_down()`（`GetKeyboardState` ✓，**不进 replay 输入流** ✓）⇒ 调试用正合适 ✓；
 *     边沿自己记（电平 → 边沿 ✓，先例 `animal.c:3988` ✓）。
 *
 * ## 生命周期（METHOD 铁律二 ✓）
 *   本卡**不留跨帧资源** ✓（没有自建 VM / HUD 槽 / 池 ✓）⇒ `on_run_reset` / `on_delete`
 *   只需把**文件静态**（游标 + 三个键的上帧电平）清零 ✓ —— 按 P17/P21 的教训：文件静态是
 *   进程级的，retry/新局都不归零 ⇒ 必须自己清 ✓。
 *
 * ⚠ 这是**临时调试卡** —— owner 口径：「最终会删掉这张，包括其他测试卡」✓ ⇒ 收工时整文件 +
 *   卡表条目一起删 ✓（`CE_DEBUG_SPACE_KEYS` 也可以恢复成 1 ✓）。*/
#include "sdk.h"

#define DDBG_ID   119u
#define DDBG_MAX  255

static int32_t    s_cursor;          /* 当前目标下标（0..s_n−1）*/
static int32_t    s_n;               /* 本帧在场卡数 */
static int32_t    s_id[DDBG_MAX + 1];
static uint8_t   *s_card[DDBG_MAX + 1];
static int32_t    s_a_prev, s_s_prev, s_sp_prev;   /* 三个键的上帧电平（边沿检测 ✓）*/

/* 重建"本局在场卡"表（卡号升序；排除本卡自己 ✓）。每帧重建 —— 卡链最多 255 个节点 ✓，代价可忽略 ✓。*/
static void ddbg_rebuild(void)
{
    uint8_t *mgr = CE_ABILITY_MGR();
    unsigned guard = 0;
    const uint8_t *node;

    s_n = 0;
    if (!mgr) return;
    for (node = *(const uint8_t *const *)(mgr + CE_MGR_CARD_LIST_FIRST);
         node && guard < 256;
         node = *(const uint8_t *const *)(node + CE_NODE_NEXT), ++guard) {
        uint8_t *card = *(uint8_t *const *)(node + CE_NODE_CARD);
        uint32_t id;
        if (!card) continue;
        id = ce_card_id((ce_card_t *)card);
        if (id == DDBG_ID) continue;                     /* 不含自己 ✗（免得把自己删了）*/
        if (id == 0 || id > DDBG_MAX) continue;
        s_id[s_n] = (int32_t)id;
        s_card[s_n] = card;
        ++s_n;
    }
    /* 升序（插入排序；n ≤ 255 且链表本来近似有序 ⇒ 够用 ✓）*/
    {
        int32_t i, j;
        for (i = 1; i < s_n; ++i) {
            int32_t ki = s_id[i]; uint8_t *kc = s_card[i];
            for (j = i - 1; j >= 0 && s_id[j] > ki; --j) { s_id[j+1] = s_id[j]; s_card[j+1] = s_card[j]; }
            s_id[j+1] = ki; s_card[j+1] = kc;
        }
    }
    if (s_cursor >= s_n) s_cursor = s_n ? s_n - 1 : 0;
    if (s_cursor < 0) s_cursor = 0;
}

/* 删掉当前目标（`SPACE` 与 `C` 共用 ✓）。返回 1 = 真的删了 ✓。*/
static int ddbg_delete_current(const char *how)
{
    uint32_t victim;
    if (s_n <= 0) { ce_log("dbgdel: 没有可删的卡（本局只装了本卡？）"); return 0; }
    victim = (uint32_t)s_id[s_cursor];
    if (ce_destroy_card((ce_card_t *)s_card[s_cursor])) {
        ce_log("dbgdel: [%s] 已删除卡 %u（位次 %d/%d）", how, victim, (int)(s_cursor + 1), (int)s_n);
        ddbg_rebuild();
        return 1;
    }
    ce_log("dbgdel: [%s] 删除卡 %u 被拒（ce_destroy_card 返回 0）", how, victim);
    return 0;
}

static int ddbg_tick(ce_card_t *c)
{
    (void)c;
    int a  = ce_key_down(CE_VK_A)     ? 1 : 0;
    int s  = ce_key_down(CE_VK_S)     ? 1 : 0;
    int sp = ce_key_down(CE_VK_SPACE) ? 1 : 0;

    ddbg_rebuild();
    if (s_n > 0) {
        if (a && !s_a_prev) {
            s_cursor = (s_cursor + s_n - 1) % s_n;
            ce_log("dbgdel: 目标 <- %d（位次 %d/%d）", (int)s_id[s_cursor], (int)(s_cursor + 1), (int)s_n);
        }
        if (s && !s_s_prev) {
            s_cursor = (s_cursor + 1) % s_n;
            ce_log("dbgdel: 目标 -> %d（位次 %d/%d）", (int)s_id[s_cursor], (int)(s_cursor + 1), (int)s_n);
        }
        if (sp && !s_sp_prev) (void)ddbg_delete_current("SPACE");
    }
    s_a_prev = a; s_s_prev = s; s_sp_prev = sp;
    return 0;
}

static int ddbg_draw(ce_card_t *c)
{
    (void)c;
    if (!CE_GAME_THREAD()) return 0;                     /* 菜单/商店里不画 ✓ */
    if (s_n > 0) {
        ce_hud_text(8.0f, 404.0f, 0xFF80FFFFu,
                    "[dbg] 删除目标: %d  (位次 %d/%d)   A/S 切换   SPACE 删除",
                    (int)s_id[s_cursor], (int)(s_cursor + 1), (int)s_n);
    } else {
        ce_hud_text(8.0f, 404.0f, 0xFF808080u, "[dbg] 没有可删的卡（A/S/SPACE 无效）");
    }
    return 0;
}

static int on_activate(ce_card_t *c)
{
    (void)c;
    ddbg_rebuild();
    (void)ddbg_delete_current("C");
    return 1;                                            /* 视作发动成功（代价由 active_recharge 管 ✓）*/
}

static void ddbg_reset(void)
{
    s_cursor = 0; s_n = 0;
    s_a_prev = s_s_prev = s_sp_prev = 0;
}

static void on_run_reset(ce_card_t *c) { (void)c; ddbg_reset(); }
static void on_delete(ce_card_t *c)    { (void)c; ddbg_reset(); }

CE_CARD(119,                                /* ⚠ 必须写裸 119：宏做 token 拼接（`ce_hooks_##ID`）✗ */
        .active_recharge = 1,                 /* 主动卡（照 118）：D 切到本卡后按 C ✓ */
        .on_tick_2 = ddbg_tick,               /* 读 A/S/SPACE（`ce_key_down` 电平 + 自己记边沿 ✓）*/
        .on_draw = ddbg_draw,                 /* 左下角一行 HUD ✓（`ce_hud_text` 只许在 +0x48 里调 ✓）*/
        .on_activate = on_activate,
        .on_run_reset = on_run_reset,
        .on_delete = on_delete);
