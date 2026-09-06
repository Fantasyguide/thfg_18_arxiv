/* 黄金小丑（Balatro・Golden Joker）—— 被动：每关过关、商店开门那一刻，金钱 +20％（向下取整）。
 *
 * 时点是 shop.c 的 BP_ce_shop_reopen 里 MSG 请求开第一家店的那一帧（SDK 事件 on_shop_open）：商店对象这一帧稍后才建，
 * 所以店里显示与可花的就是加过利息的钱；重开的第二家不再算。无上限，加的钱同时记进收入统计（方片 2 同款）。
 * 只依赖 MONEY 本身，replay 里同一时点得到同一结果。AUDIT §W。 */
#include "sdk.h"

#define GJ_PERCENT 20

static void on_shop_open(ce_card_t *c)
{
    (void)c;
    int32_t m = CE_MONEY();
    int32_t gain = ce_interest(m, GJ_PERCENT);
    if (gain <= 0) { ce_log("golden: money %d, no interest", m); return; }
    CE_MONEY() = m + gain;
    CE_MONEY_TOTAL() += gain;
    ce_log("golden: interest %d%% -> money %d + %d = %d", GJ_PERCENT, m, gain, m + gain);
}

CE_CARD(73, .on_shop_open = on_shop_open);
