/* truesilver_core.h —— 真银圣剑（id 75）的纯逻辑：每关耐久 + 炸弹后的伤害爆发帧。纯 C11，主机单测 tests/test_truesilver_core.c。 */
#pragma once
#include <stdint.h>

#define TS_DURABILITY     2        /* 每关开场的耐久（炉石 4/2 的「2」）*/
#define TS_BURST_FRAMES   4        /* 挥砍：炸弹放出后的下一帧起连续几帧出伤害源 */
#define TS_BURST_DMG      100      /* 每帧；4 × 100 = 400（炉石 4/2 的「4」× 100）*/
#define TS_BURST_CAP      400      /* 爆发帧把每敌每帧伤害上限抬到它：炸弹自己的源也在同一帧结算，比炎魔的 300 再高 100 留给它们（AUDIT X4）*/
#define TS_FIELD_W        384.0f   /* 盖满弹幕区：中心 (0, 224)，384×448 */
#define TS_FIELD_H        448.0f
#define TS_FIELD_CY       224.0f

typedef struct {
    int      durability;    /* 本关剩余挥砍次数 */
    uint32_t burst_left;    /* 还要出几帧伤害源 */
    uint32_t swings;        /* 统计：本局挥砍总数 */
    /* HUD 数字（引擎侧，core 不碰）*/
    uint32_t icon_id;       /* 引擎交来的 HUD 图标根 VM id（on_hud_anm）*/
    uint32_t digit_id;      /* 我们的数字 VM id */
    int      digit_shown;   /* digit VM 当前显示的数字（-1 = 还没设）*/
} ts_state_t;

void ts_stage_start(ts_state_t *s);       /* 每关开场：耐久回满、清爆发 */
int  ts_on_bomb(ts_state_t *s);           /* 放了一发炸弹：耐久 > 0 → 扣 1、武装爆发、返回 1；否则 0 */
int  ts_tick(ts_state_t *s);              /* 每帧：爆发中返回本帧伤害（TS_BURST_DMG），否则 0 */
