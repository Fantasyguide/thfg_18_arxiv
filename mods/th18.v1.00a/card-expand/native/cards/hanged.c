/* 倒吊人（Balatro・The Hanged Man）—— 商店一次性购买：随机摧毁卡组里的另一张卡，获得 300 金钱。本身不进卡组。
 *
 * 即时卡（强欲之壶 / 方片 2 同款）：ctor 里做完 return 1，分配尾段当场删掉自己。ctor 跑在自己入链之前，
 * 所以链表里没有自己；候选 = 链表里除当前选中的主动卡以外的全部（选中的那张 mgr+0x38 指着它，删了要重选 + 重建
 * HUD 图标 VM，ce_destroy_card 拒绝这种删法）。没有候选就什么都不给——买了个寂寞，日志记一行。
 * 随机走游戏 replay 安全流（ce_rand）。固定 300：摧毁垃圾牌赚、摧毁关键牌亏，这就是赌。
 * 钱在 ctor 里加、价钱由商店随后扣（05-shop-and-money.md「成交」），落地 = M + 300 − price。AUDIT §W。 */
#include "sdk.h"

#define HM_REWARD 300

static int ctor(ce_card_t *c)
{
    ce_card_t *victim = ce_pick_random_card(c);
    if (!victim) { ce_log("hanged: no other card to destroy, nothing gained"); return 1; }
    uint32_t vid = ce_card_id(victim);
    if (!ce_destroy_card(victim)) { ce_log("hanged: destroy of card %u refused, nothing gained", vid); return 1; }
    int32_t m = CE_MONEY();
    CE_MONEY() = m + HM_REWARD;
    CE_MONEY_TOTAL() += HM_REWARD;
    ce_log("hanged: destroyed card %u, money %d -> %d (shop still deducts the price)", vid, m, m + HM_REWARD);
    return 1;                                              /* 即时卡：当场销毁，不进卡组 */
}

CE_CARD(74, .ctor = ctor);
