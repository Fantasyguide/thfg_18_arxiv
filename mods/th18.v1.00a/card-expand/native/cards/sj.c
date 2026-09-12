/* 黑桃 J —— 移动速度 +10%。
 * player+0x477ec 是每帧的移速倍率：Player tick 末尾复位 1.0，移动函数读它。
 * AbilityManager tick（优先级 0x16）先于 Player tick（0x17），所以在 on_tick_2 里乘才生效（on_tick 在复位之前，白写）。
 * BombMarisa 用同一字段（0.5），多张同类卡相乘叠加。 */
#include "sdk.h"
#include "royal.h"   /* 皇家同花顺：五张黑桃共用 ctor */

static int on_tick_2(ce_card_t *c)
{
    ce_royal_tick(c);
    uint8_t *p = CE_PLAYER();
    if (p) *(float *)(p + CE_PLAYER_SPEED_MULT) *= 1.1f;
    return 0;
}

/* on_delete：删卡（倒吊人 / 原版空白卡）时收掉皇家同花顺演出的文件级倒计时（见 royal.c 的 ce_royal_dismiss）。
 * 移动速度那一项不用还：`player+0x477ec` 是**每帧**倍率，Player tick 末尾自己复位 1.0。*/
CE_CARD(59, .ctor = ce_royal_flush_ctor, .on_tick_2 = on_tick_2, .on_delete = ce_royal_dismiss);
