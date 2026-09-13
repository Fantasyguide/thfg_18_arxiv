/* broken_core_core.h —— 破损核心（id 71）的纯逻辑：开火节奏 + 挑最近的敌人 + 角度。
 * 不碰引擎，主机 make test-host 直接测。设计：docs/superpowers/specs/2026-09-05-broken-core-design.md。
 *
 * 为什么自带 atan2：DLL 只链 kernel32 + msvcrt，且有 x87 自检（make dllx87）——不引 libm，
 * 也不调游戏自己那个 x87 CRT atan2（`0x4a94a0`）。这里是纯 SSE 的多项式近似，跨机器逐位一致 = replay 安全。 */
#pragma once
#include <stdint.h>

#define BC_PERIOD     60        /* 帧：蓄满一发要 1 秒（2026-09-06 平衡：120 → 60，名义 DPS 40 → 120，原生子机卡中位 120–135）*/
#define BC_HIT_FRAMES 4         /* 一道闪电的伤害拆成连续 4 帧、每帧一个新伤害源（一个源对同一敌人只算一次）：单帧 30 才不撞每帧上限（Sakuya 60）*/
#define BC_DMG_FRAME  30        /* 每帧 30 × 4 = 一道 120 */
#define BC_RANGE      512.0f    /* 锁敌半径（抄 CardAlice 的搜索半径 `0x4b93b0`）*/
#define BC_RANGE_SQ   (BC_RANGE * BC_RANGE)

typedef struct {
    uint32_t frames;      /* 子机存在以来的帧数 */
    uint32_t charge;      /* 距上次开火的帧数；>= BC_PERIOD = 蓄满 */
    uint32_t shots;       /* 已打出去几发 */
    uint32_t hit_left;    /* 这一道还剩几帧要放伤害源（bc_did_fire 置 BC_HIT_FRAMES，bc_hit_frame 每帧取一）*/
    float    tx, ty;      /* 这一道钉住的目标坐标（开火那帧记下）*/
} bc_state_t;

/* 每帧一次：计时。返回 1 = 蓄满了（可以打）。蓄满后若没目标会一直保持蓄满，
 * 直到 bc_did_fire 才清零——「攒着，敌人一进射程就劈」。*/
int  bc_tick(bc_state_t *s);
void bc_did_fire(bc_state_t *s, float tx, float ty);   /* 记目标、清充能、开始 BC_HIT_FRAMES 帧的伤害 */
int  bc_hit_frame(bc_state_t *s);                        /* 每帧一次（含开火那帧）：返回 1 = 本帧在 (tx, ty) 放一个 BC_DMG_FRAME 的源 */

/* 挑最近的：reset → 逐个 consider → angle。距离平方比较，不开方。 */
typedef struct {
    int   found;
    float best_d2;
    float dx, dy;         /* 目标 − 子机 */
} bc_aim_t;

void bc_aim_reset(bc_aim_t *a);
void bc_aim_consider(bc_aim_t *a, float ox, float oy, float ex, float ey);
/* 有目标且在 BC_RANGE 内 → 写出角度（弧度，与引擎同向：0 = +x，−π/2 = 正上方）并返回 1 */
int  bc_aim_angle(const bc_aim_t *a, float *out_angle);
/* 到目标的距离（没目标时 0）。电弧的长度用它。**static inline** 的理由同 bc_atan2f：
 * i386 ABI 返回 float 要走 st0，内联掉才守得住 make dllx87 的「零 x87」。
 * `-fno-math-errno`（Makefile）让 __builtin_sqrtf 编成一条 sqrtss，不掉进 libm。*/
static inline float bc_aim_dist(const bc_aim_t *a)
{
    return a->found ? __builtin_sqrtf(a->best_d2) : 0.0f;
}

/* atan2 近似（最大误差 < 2e-5 rad）。**故意定义在头里的 static inline**：i386 ABI 里返回 float 要走 st0
 * （`flds`），而本仓的 make dllx87 不许我们的目标文件出现任何 x87 指令。内联掉就没有那次返回。 */
#define BC_PI      3.14159265358979f
#define BC_HALF_PI 1.57079632679490f

static inline float bc_atan_unit(float z)      /* atan(z)，|z| <= 1：Hastings 奇次多项式 */
{
    float z2 = z * z;
    return z * (0.99986600f + z2 * (-0.33029950f + z2 * (0.18014100f
              + z2 * (-0.08513300f + z2 * 0.02083510f))));
}

static inline float bc_atan2f(float y, float x)
{
    float ax = x < 0.0f ? -x : x;
    float ay = y < 0.0f ? -y : y;
    float ang;

    if (ax == 0.0f && ay == 0.0f) return 0.0f;
    /* 先算第一象限的角，再按 x / y 的符号折回四个象限 */
    ang = (ay <= ax) ? bc_atan_unit(ay / ax) : BC_HALF_PI - bc_atan_unit(ax / ay);
    if (x < 0.0f) ang = BC_PI - ang;
    return y < 0.0f ? -ang : ang;
}
