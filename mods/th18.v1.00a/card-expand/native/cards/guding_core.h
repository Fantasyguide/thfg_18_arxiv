/* guding_core.h —— 古锭刀（id 76）的纯逻辑：「终符」判定 + 翻倍。纯 C11，主机单测 tests/test_guding_core.c。
 *
 * 终符 = boss 的最后一张符卡。ZUN 每段攻击用 setInterrupt(0, hp, time, "下一段子程序") 衔接，th18 七关 boss + Extra 道中
 * 最后一张符卡的中断槽子程序名一律是 "BossDead" / "MBossDead"，其余是 "Boss2"…"Boss10"（engine/ecl/th18/01 §6）。
 * 所以：符卡进行中 && boss 当前活动中断槽（第一个 hp_value > -1 && time > 0）的子程序名含 "Dead" ⇒ 终符。 */
#pragma once
#include <stdint.h>

#define GD_MULT              2        /* 伤害倍率 */
#define GD_SLOT_HP           0x0      /* zEnemyInterrupt 布局（ExpHP + 一手，engine.h CE_ED_INTERRUPTS）*/
#define GD_SLOT_TIME         0x4
#define GD_SLOT_SUB_LIFE     0x8      /* char[0x40]：血量到阈值走它 */
#define GD_SLOT_SUB_TIMEOUT  0x48     /* char[0x40]：计时到阈值走它 */
#define GD_SLOT_SUB_LEN      0x40
#define GD_SLOT_STRIDE       0x88
#define GD_FINAL_MARK        "Dead"   /* 子程序名里含它 = 这段攻击结束即 boss 死亡 */

typedef struct {
    int active;         /* 当前帧是否终符（on_bullet_created 读它）*/
    uint32_t frames;    /* 统计：本局终符总帧数 */
    uint32_t bullets;   /* 统计：本局翻倍过的自机弹数 */
} gd_state_t;

/* 在 interrupts[n]（stride GD_SLOT_STRIDE）里找活动槽：第一个 hp_value > -1 && time > 0；没有返回 -1 */
int gd_find_active_slot(const uint8_t *interrupts, unsigned n);
/* 槽的子程序名（sub_life 或 sub_timeout，任一）含 "Dead" → 1。名字按 0x40 截断，不依赖终止符 */
int gd_slot_is_final(const uint8_t *slot);
/* 每帧：写入本帧判定，返回 +1 = 刚进入终符、-1 = 刚离开、0 = 没变 */
int gd_update(gd_state_t *s, int final_now);
/* 自机弹伤害：终符中 ×GD_MULT，否则原样 */
int gd_bullet_damage(gd_state_t *s, int dmg);
/* 每敌每帧上限：终符中 base×GD_MULT，否则 0（= 不抬）*/
int gd_damage_cap(const gd_state_t *s, int base);
