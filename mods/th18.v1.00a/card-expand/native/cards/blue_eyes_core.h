/* blue_eyes_core.h —— 青眼白龙的纯逻辑：生命 / 跟随 / 发波。不碰引擎，主机 make test-host 直接测。
 * 设计：docs/superpowers/specs/2026-09-04-blue-eyes-design.md §2。 */
#pragma once
#include <stdint.h>

#define BE_HP             1500     /* 2026-09-04 平衡：2500 → 1500 */
#define BE_WAVE_PERIOD    300      /* 帧：每 5 s 一波；第一波在召唤后第 300 帧 */
#define BE_WAVE_FRAMES    45       /* 2026-09-04 平衡：30 → 45（×1.5）；脚本 79–85 的保持段同步 */
#define BE_WAVE_DMG       100      /* 每帧伤害；45 × 100 = 4500。2026-09-06 起配合 BE_WAVE_CAP 是实数（此前不抬上限，自机开火时被自机弹吃满配额）*/
#define BE_WAVE_CAP       300      /* 光束帧把每敌每帧伤害上限 player+0x47984 抬到它（炎魔 FL_BLAST_CAP / Remilia 同款；AUDIT V20、O29e′）*/
#define BE_RECHARGE       600
#define BE_RADIUS         48.0f
#define BE_FOLLOW_DY      (-80.0f)
#define BE_SUMMON_DY      (-100.0f)
#define BE_FOLLOW_LERP    0.04f
#define BE_BEAM_WIDTH     96.0f    /* 判定宽（用户 2026-09-04：大幅增宽；画面上的 Master Spark 贴图约 126 px 宽）*/
#define BE_MOUTH_DY       (-52.0f) /* 龙头（贴图顶端）相对龙中心的 y：256 × 0.45 / 2 ≈ 58，头尖略进一点 */
#define BE_BAR_DY         66.0f    /* 血条中心相对龙中心的 y（龙尾尖约 +58）*/
#define BE_BAR_W          56.0f    /* 血条填充满宽（底槽各多 2 px）*/
#define BE_BAR_H          4.0f
#define BE_BOB_PERIOD     96       /* 帧：呼吸浮动一个来回（2026-09-06 用户：与炎魔之王同款）*/
#define BE_BOB_AMP        5.0f     /* px：浮动半幅（龙约 115 px 高）*/

typedef struct {
    int32_t  hp;
    uint32_t frames;      /* 召唤以来的帧数 */
    uint32_t wave_left;   /* 本波剩余帧 */
    uint32_t waves;       /* 已发波数 */
    uint32_t blocked;     /* 累计挡弹 */
    float    x, y, z;     /* 龙坐标（玩家坐标系）*/
    uint32_t anm_id;      /* 龙 VM；0 = 没有 */
    uint32_t beam_id;     /* 当前光束 VM；0 = 没有 */
    uint32_t bar_bg_id;   /* 血条底槽 VM */
    uint32_t bar_id;      /* 血条填充 VM（C 每帧写 scale.x = 满宽 × hp/BE_HP）*/
} be_state_t;

typedef struct { int wave_start; int beam_dmg; int died; } be_step_t;

void      be_summon(be_state_t *s, float px, float py, float pz);   /* 清零、hp = BE_HP、坐标 = (px, py + BE_SUMMON_DY, pz) */
void      be_follow(be_state_t *s, float px, float py, float pz);   /* 目标 (px, py + BE_FOLLOW_DY, pz)，lerp BE_FOLLOW_LERP */
be_step_t be_step(be_state_t *s, int blocked);                       /* 每帧：扣血、计帧、发波状态机 */

/* 呼吸浮动：画面 y = s->y + 它（挡弹判定 / 光束起点 / 血条一起跟）。三角相位过 smoothstep 折成来回 = C¹ 连续的近似正弦，
 * 不引 libm。frames = 0 在最低点。static inline：i386 返回 float 走 st0，内联掉才守得住 make dllx87 的零 x87（炎魔之王同款）。*/
static inline float be_bob_dy(const be_state_t *s)
{
    uint32_t ph = s->frames % BE_BOB_PERIOD;
    float t = (float)ph / ((float)BE_BOB_PERIOD * 0.5f);
    if (t > 1.0f) t = 2.0f - t;
    float k = t * t * (3.0f - 2.0f * t);
    return (k * 2.0f - 1.0f) * BE_BOB_AMP;
}
