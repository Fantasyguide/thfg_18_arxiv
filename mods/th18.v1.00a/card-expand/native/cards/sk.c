/* 黑桃 K —— 自机弹伤害 ×1.1。
 * 数值走 SDK 的自机弹伤害层（`.bullet_damage_permille`，AUDIT §AA）：聚合、单次取整、每帧伤害上限
 * 跟着抬，全都不用这张卡自己操心。2026-09-08 之前它自己写 `bullet+0x9c`、并且**漏了抬上限**——
 * 贴脸打 boss 时那 +10% 有相当一部分被每敌每帧配额吃掉，改到层里顺手修掉。 */
#include "sdk.h"
#include "royal.h"   /* 皇家同花顺：五张黑桃共用 ctor */

static int royal_tick(ce_card_t *c) { ce_royal_tick(c); return 0; }
/* on_delete：删卡（倒吊人 / 原版空白卡）时收掉皇家同花顺演出的文件级倒计时（见 royal.c 的 ce_royal_dismiss）。
 * 伤害倍率不用自己清：`.bullet_damage_permille` 是**声明层**的值，SDK 的 `opdelete` 桩会
 * `ce_dmg_set(ID, 0)` 让这张卡退出聚合（SDK.md §6.1）。*/
CE_CARD(61, .ctor = ce_royal_flush_ctor, .on_tick_2 = royal_tick, .bullet_damage_permille = 1100,
            .on_delete = ce_royal_dismiss);
