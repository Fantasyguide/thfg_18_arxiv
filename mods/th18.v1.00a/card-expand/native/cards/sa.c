/* 黑桃 A —— Miss 后无敌时间 +50%。
 * 复活时 Player tick 在 0x45c35e 把无敌计时器置 {prev 0x117, cur 0x118, 280.0}。这个 (280, 279) 的组合只在
 * 刚置好的那一帧出现（之后 prev = 上一帧 cur），炸弹 / 决死救回写的是别的值。AbilityManager tick 先于 Player tick，
 * 所以 on_tick_2 在下一帧 Player 递减之前看到它 → 放大到 420。 */
#include "sdk.h"
#include "royal.h"   /* 皇家同花顺：五张黑桃共用 ctor */

static int on_tick_2(ce_card_t *c)
{
    ce_royal_tick(c);
    if (!CE_PLAYER()) return 0;
    ce_timer_t *t = CE_PLAYER_INVULN();
    if (t->cur == CE_RESPAWN_INVULN_FRAMES && t->prev == CE_RESPAWN_INVULN_FRAMES - 1) {
        int32_t n = CE_RESPAWN_INVULN_FRAMES * 3 / 2;
        t->cur = n; t->prev = n - 1; t->cur_f = (float)n;
    }
    return 0;
}

/* on_delete：删卡（倒吊人 / 原版空白卡）时收掉皇家同花顺演出的文件级倒计时（见 royal.c 的 ce_royal_dismiss）。
 * 无敌计时器不用还：那是玩家自己的 zTimer，逐帧自减，本卡只是在复活那一帧把它改长。*/
CE_CARD(62, .ctor = ce_royal_flush_ctor, .on_tick_2 = on_tick_2, .on_delete = ce_royal_dismiss);
