/* zenith_core.h —— 天顶剑（id 87）的纯逻辑：开火节奏 / 挑最近的敌人 / 剑刃池的运动。
 * 不碰引擎（ANM VM id 只当数字转手），主机 `make test-host` 直接测。
 *
 * 剑刃 = **卡自管的 ANM VM + 每帧一个小矩形伤害源**（炎魔火球 72 的写法，见 `cards/firelord.c`），
 * 不是引擎弹（`assets/sht` 那条链只在按住射击键时广播，而这张卡要跟班自己开火）。
 * ⚠ 仓里此前**没有**「多把在途的移动伤害弹」池（炎魔只有单发火球）⇒ 这个池是**新写法**；
 *   所有数值都是 `❓ 自定`（见 `TUNING.md` 的 87 条目），机制本身照抄既有卡。
 *
 * 为什么自带挑目标而不直接用 `broken_core_core.h` 的 `bc_aim_*`：那套的射程是破损核心的
 * `BC_RANGE`(512)，天顶剑的剑刃要飞 ZN_LIFE×ZN_SPEED ≈ 600 px ⇒ 射程得单独调（不影响那张卡）。
 * 角度仍然用 `bc_atan2f`（同一个确定性 SSE 多项式，不引 libm、零 x87）。 */
#pragma once
#include <stdint.h>

#define ZN_BLADE_MAX   8        /* 同时在途的剑刃上限（池满就攒着不发，不丢已有的）*/
#define ZN_PERIOD      20       /* 帧：两把之间隔 1/3 秒 ❓ 自定（对齐破损核心 120 DPS 的名义量级）*/
#define ZN_RANGE       640.0f   /* 锁敌半径（> 剑刃的射程，免得对着打不到的敌人发）❓ 自定 */
#define ZN_RANGE_SQ    (ZN_RANGE * ZN_RANGE)
#define ZN_SPEED       13.0f    /* px / 帧 ❓ 自定：46 帧飞约 600 px */
#define ZN_LIFE        46       /* 帧：剑刃的寿命（到点消失，不留在场上）❓ 自定 */
#define ZN_SPIN        0.42f    /* 弧度 / 帧：自旋（约 24°/帧，一趟转 ~3 圈）❓ 自定 */
#define ZN_HIT_LIFE    2        /* 伤害源寿命（帧）：一个源对同一敌人只结算一次 ⇒ 每帧放一个新源 */
#define ZN_HIT_W       26.0f    /* 判定框（剑刃 24 px 长，给一点余量）❓ 自定 */
#define ZN_HIT_H       26.0f
#define ZN_DMG         20       /* 每帧每个源的伤害；一趟大约罩住敌人 2 帧 ⇒ 一把约 40 ❓ 自定 */
#define ZN_CAP         200      /* 有剑刃在飞时把「每敌每帧」上限 player+0x47984 抬到它（豌豆 77 同款做法）*/
#define ZN_X_MIN       (-360.0f) /* 出界即收（实体坐标：x 以弹幕区中心为 0）*/
#define ZN_X_MAX       360.0f
#define ZN_Y_MIN       (-80.0f)
#define ZN_Y_MAX       520.0f

typedef struct {
    int32_t  active;      /* 1 = 这个槽在飞 */
    uint32_t id;          /* ANM VM id（引擎侧句柄；core 只存不碰）*/
    float    x, y;        /* 当前坐标 */
    float    vx, vy;      /* 每帧位移（开火那帧定死，直线飞行、不追踪）*/
    float    rot;         /* 自转角（弧度；C 每帧写进 VM 的 rotation.z）*/
    uint32_t life;        /* 剩余飞行帧；0 = 该收了 */
} zn_blade_t;

typedef struct {
    uint32_t  frames;     /* 子机存在以来的帧数 */
    uint32_t  charge;     /* 距上次开火的帧数（钳在 ZN_PERIOD）*/
    uint32_t  fired;      /* 已掷出几把（日志 / 单测用）*/
    zn_blade_t b[ZN_BLADE_MAX];
} zn_state_t;

/* 挑最近的（reset → 逐个 consider → ready）。距离平方比较，不开方。
 * 角度由调用方 `bc_atan2f(aim.dy, aim.dx)` 算（core 不引 libm）。 */
typedef struct {
    int   found;
    float best_d2;
    float dx, dy;         /* 目标 − 子机 */
} zn_aim_t;

void zn_reset(zn_state_t *s);                        /* 全部清零（不碰 VM：VM 由 C 侧先删）*/
int  zn_charge(zn_state_t *s);                       /* 每帧一次：frames++、充能；返回 1 = 蓄满 */
void zn_did_fire(zn_state_t *s);                     /* 蓄满后真发了一把：清充能、计数 */
zn_blade_t *zn_slot(zn_state_t *s);                  /* 找一个空槽；全满返回 0（调用方攒着）*/
void zn_launch(zn_blade_t *b, uint32_t id, float ox, float oy, float dx, float dy);
                                                     /* 朝 (dx, dy) 掷出：归一化 × ZN_SPEED */
void zn_advance(zn_blade_t *b);                      /* 推一帧；寿命尽 / 出界 ⇒ active = 0 */
int  zn_live(const zn_state_t *s);                   /* 在飞数量（> 0 时调用方抬每帧伤害上限）*/

void zn_aim_reset(zn_aim_t *a);
void zn_aim_consider(zn_aim_t *a, float ox, float oy, float ex, float ey);
int  zn_aim_ready(const zn_aim_t *a);
