/* 黑桃 Q —— 道具自动回收范围大幅增加。
 * 抄 CardNitori（id 21）的 on_load：写玩家四个道具参数。Player__reset 的默认 {5,30,70,70}，Nitori {10,30,110,110}。
 * on_load 由 AbilityManager__notify_cards_on_load 在关卡开场（Player__reset 之后）调；商店买到后下一关生效，与 Nitori 同。 */
#include "sdk.h"
#include "royal.h"   /* 皇家同花顺：五张黑桃共用 ctor */

static int on_load(ce_card_t *c)
{
    (void)c;
    uint8_t *p = CE_PLAYER();
    if (!p) return 0;
    /* 原生默认 {5, 30, 70, 70}，Nitori(21) {10, 30, 110, 110}；这里「略大」：半径 70 → 95、吸速 5 → 7，比 Nitori 弱一档 */
    *(float *)(p + CE_PLAYER_ITEM_ATTRACT_SPD) = 7.0f;
    *(float *)(p + CE_PLAYER_ITEM_COLLECT_R)   = 30.0f;
    *(float *)(p + CE_PLAYER_ITEM_ATTRACT_RF)  = 95.0f;
    *(float *)(p + CE_PLAYER_ITEM_ATTRACT_RU)  = 95.0f;
    return 0;
}

static int royal_tick(ce_card_t *c) { ce_royal_tick(c); return 0; }
/* on_delete：删卡（倒吊人 / 原版空白卡）时收掉皇家同花顺演出的文件级倒计时（见 royal.c 的 ce_royal_dismiss）。
 * 上面那四个道具参数**刻意不在这里还原**：它们是「每关开场重设」的量 —— `Player::reset` 在每个关卡开场
 * 先铺回默认值 {5,30,70,70}，之后才是各卡的 `on_load`；而删卡的四条路（倒吊人 ctor / 空白卡的
 * `reset_cards(mgr,0)` / 局末 recount / replay 复原的 `reset_cards`）全发生在**商店或整段装卸**的时刻
 * （`engine/card/th18/02-lifecycle.md` §1/§6/§7、`05-shop-and-money.md` §7；关末这两店是连着开的，
 * 见 native/shop.c 头注）⇒ 残留最多撑到下一关开场就被冲掉，进不了任何一关的游玩帧。
 * 在这里写默认值反而会把 Nitori（21）等同写这四个字段的卡的设置一起打掉。*/
CE_CARD(60, .ctor = ce_royal_flush_ctor, .on_tick_2 = royal_tick, .on_load = on_load,
            .on_delete = ce_royal_dismiss);
