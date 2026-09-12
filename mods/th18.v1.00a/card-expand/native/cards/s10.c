/* 黑桃 10 —— 从道具获得的金钱 +10％：每吃第 10 个金钱道具，那一个给 2。
 * 事件 on_item_money：collect_money_item 里 MONEY += 1 之前（sdk.c BP_ce_item_money），bonus 由断点同时加进
 * MONEY 与 MONEY_TOTAL_COLLECTED。用确定性计数而不是随机：replay 靠输入重放，自带随机会失同步。
 * 计数在卡的私有状态里，随卡对象一局一建。 */
#include "sdk.h"
#include "royal.h"   /* 皇家同花顺：五张黑桃共用 ctor */

typedef struct { uint32_t n; } s10_state_t;

static void on_item_money(ce_card_t *c, int32_t *bonus)
{
    s10_state_t *st = ce_state(c, s10_state_t);
    if (!st) return;
    if (++st->n >= 10) { st->n = 0; *bonus += 1; }
}

static int royal_tick(ce_card_t *c) { ce_royal_tick(c); return 0; }
/* on_delete：这张卡在商店里被删掉（倒吊人 / 原版空白卡）时，把皇家同花顺演出的文件级倒计时收掉
 * —— 倒计时靠本卡的 on_tick_2 走，卡没了就永远挂着。理由与做法见 royal.c 的 ce_royal_dismiss。*/
CE_CARD(58, .ctor = ce_royal_flush_ctor, .on_tick_2 = royal_tick, .on_item_money = on_item_money,
            .on_delete = ce_royal_dismiss);
