/* shop.c —— 「商店走两遍」：每关过关后商店开 CE_SHOP_VISITS 次（默认 2），每次仍是原生流程。
 *
 * 两个断点（AUDIT §P；引擎链见 engine/card/th18/05-shop-and-money.md §3.5）：
 *   BP_ce_shop_bought  0x4183ea  AbilityShop__on_tick 成交分支：状态已置 5，紧接着给背景 VM 发中断 6 与发卡扣钱。
 *                      盖住 push 6 / push 0 / push 6 / lea ecx,[edi+0x228]（12 字节，无相对寻址），放行；只记「本次进店成交了」。
 *   BP_ce_shop_reopen  0x443b05  GameThread__on_tick：test eax,0x20000（eax = GameThread+0xb0，esi = GameThread）。
 *                      eax 带位 = 关末 MSG opcode 36 刚要求开店 → 名额从头算；
 *                      否则若「店刚关（0x4cf2a4 == 0）且本次成交过且名额没用完」→ eax |= 0x20000，原 test 用改过的 eax 算 flags，
 *                      GameThread 这一帧就再 new 一家（0x443b10..0x443c17，与原生同一段代码）。
 *                      flag_set 那一支（MSG 开第一家店）顺带调 ce_sdk_on_shop_open()：过关结算类的卡（黄金小丑）在这里改钱。
 *   ce_shop_setup      门里：两个断点已挂；记一行配置。
 *
 * 为什么无空档帧：商店 on_tick 优先级 0xc，在自己 tick 里析构（state 5 → 0x4176e0 → 0x417857 清指针）；
 * GameThread 0x10 随后跑到这里重开；敌人 0x1b / GUI(MSG) 0x21 看到的指针始终非 0。
 * 例外（AUDIT P4′）：GameThread 在 0x443b05 之前有两条提前返回（0x44387b、0x4439ed），恰在那一帧命中就晚一帧重开——日志标 late。
 * 练习模式（0x4c5f8c >= 0）与 replay 回放（GameThread+0xd0 != 0）里商店 30 帧自动退、不走成交分支，天然不重开；再显式挡一道。
 */
#include "card_expand.h"
#include "thcrap_bp.h"
#include "engine.h"
#include "shop_core.h"
#include "sdk.h"

static ce_shop_state_t s_shop;

static int bp_applied(const uint8_t *p, unsigned len)
{
    if (p[0] != 0xe8) return 0;
    for (unsigned i = 5; i < len; ++i) if (p[i] != 0x90) return 0;
    return 1;
}

int __cdecl BP_ce_shop_bought(x86_reg_t *regs, void *bp_info)
{
    (void)regs; (void)bp_info;
    ce_shop_on_bought(&s_shop, CE_CURRENT_STAGE(), CE_TIME_IN_STAGE());
    ce_log("shop: bought (stage %d, frame %d, money now %d, visits left after this %d)",
           CE_CURRENT_STAGE(), CE_TIME_IN_STAGE(), CE_MONEY(), s_shop.visits_left);
    return BP_EXEC_ORIGINAL;
}

int __cdecl BP_ce_shop_reopen(x86_reg_t *regs, void *bp_info)
{
    (void)bp_info;
    uint8_t *gt = (uint8_t *)regs->esi;                       /* GameThread this（0x443874 mov esi,ecx）*/
    int flag_set  = (regs->eax & CE_GT_FLAG_OPEN_SHOP) != 0;
    int shop_open = CE_SHOP_PTR() != 0;
    int blocked   = CE_PRACTICE_STAGE() >= 0 || (gt && *(int32_t *)(gt + CE_GT_REPLAY_PLAYING) != 0);
    if (ce_shop_on_gamethread(&s_shop, flag_set, shop_open, CE_CURRENT_STAGE(), CE_TIME_IN_STAGE(), blocked)) {
        regs->eax |= CE_GT_FLAG_OPEN_SHOP;
        int gap = CE_TIME_IN_STAGE() - s_shop.last_bought_frame;          /* 原生退场 30 帧：成交帧 + 31 = 重开帧；更晚 = 中间有空档帧 */
        ce_log("shop: reopen%s (visit %d/%d, stage %d, frame %d = bought + %d, money %d)", gap > 31 ? " (late, gap!)" : "",
               s_shop.visits - s_shop.visits_left, s_shop.visits, CE_CURRENT_STAGE(), CE_TIME_IN_STAGE(), gap, CE_MONEY());
    } else if (flag_set) {
        ce_log("shop: opened by msg (visit 1/%d, stage %d, frame %d, money %d)",
               s_shop.visits, CE_CURRENT_STAGE(), CE_TIME_IN_STAGE(), CE_MONEY());
        ce_sdk_on_shop_open();                                    /* 过关结算类的卡（黄金小丑）在商店建起来之前改钱 */
    }
    return BP_EXEC_ORIGINAL;
}

int ce_shop_setup(uint8_t *base)
{
    ce_shop_reset(&s_shop, CE_SHOP_VISITS_DEFAULT);
    if (!bp_applied(base + CE_BP_SHOP_BOUGHT_RVA, 12)) { ce_verdict("FAIL: shop: breakpoint ce_shop_bought not applied"); return 0; }
    if (!bp_applied(base + CE_BP_SHOP_REOPEN_RVA, 5))  { ce_verdict("FAIL: shop: breakpoint ce_shop_reopen not applied"); return 0; }
    ce_log("shop: %d visits per stage (retail 1); reopen window %d frames", s_shop.visits, CE_SHOP_REOPEN_WINDOW);
    return 1;
}
