/* zenith_core.c —— 天顶剑的纯逻辑。运动逐行照 Terraria `Projectile.AI_182_FinalFractal`
 *（公式对照表在 zenith_core.h 的文件头）。
 * 只用加减乘除与 `__builtin_sqrtf`（Makefile 的 `-fno-math-errno` ⇒ 一条 sqrtss）与 `bc_atan2f`；
 * **一次 sin/cos 都没有** —— 椭圆那两步旋转由调用方走引擎的 `ce_polar_to_vel()`（FSINCOS）。 */
#include <string.h>

#include "zenith_core.h"
#include "broken_core_core.h"      /* bc_atan2f / BC_PI / BC_HALF_PI */

static float clamp01(float v)
{
    if (v < 0.0f) return 0.0f;
    if (v > 1.0f) return 1.0f;
    return v;
}

static float lerp2(float a, float b, float t)   /* MathHelper.Lerp(a, b, clamp01(t)) */
{
    return a + (b - a) * clamp01(t);
}

void zn_reset(zn_state_t *s)
{
    memset(s, 0, sizeof *s);
}

int zn_charge(zn_state_t *s)
{
    s->frames++;
    if (s->charge < ZN_PERIOD) s->charge++;
    return s->charge >= ZN_PERIOD;
}

void zn_did_fire(zn_state_t *s)
{
    s->charge = 0;
    s->fired++;
}

zn_blade_t *zn_slot(zn_state_t *s)
{
    int i;
    for (i = 0; i < ZN_BLADE_MAX; ++i)
        if (!s->b[i].active) return &s->b[i];
    return 0;
}

void zn_launch(zn_blade_t *b, uint32_t id, int32_t prof,
               float ox, float oy, float tx, float ty, float ai0)
{
    float dx = tx - ox, dy = ty - oy;
    float d = __builtin_sqrtf(dx * dx + dy * dy);

    if (d > ZN_ARC_MAX) {                 /* 太远就截到最大弧长（原版由玩家瞄准距离天然限住）*/
        float k = ZN_ARC_MAX / d;
        dx *= k;
        dy *= k;
        d = ZN_ARC_MAX;
    }
    b->active = 1;
    b->id = id;
    b->trail_id = 0;
    b->ov_id = 0;
    b->sp_id = 0;
    b->prof = (int16_t)prof;
    b->vx = dx * 0.5f;                    /* 原版 velocity = (目标 − 自机) / 2（**偏移**，不是位移）*/
    b->vy = dy * 0.5f;
    b->heading = bc_atan2f(dy, dx);
    b->sign_pos = (dx > 0.0f) ? 1u : 0u;
    b->ai0 = ai0;
    if (b->ai0 > ZN_AI0_MAX) b->ai0 = ZN_AI0_MAX;
    if (b->ai0 < -ZN_AI0_MAX) b->ai0 = -ZN_AI0_MAX;
    b->phase = 0.0f;
    b->frames = 0;
    b->trail_timer = 0;
    b->hit_timer = (uint8_t)ZN_HIT_COOLDOWN;   /* 出生那一帧就能命中（原版第一帧就在判定）*/
}

void zn_step(zn_blade_t *b, zn_pose_t *out)
{
    float L, k, op;

    memset(out, 0, sizeof *out);
    if (!b->active) { out->done = 1; return; }

    b->frames++;
    b->phase += zn_rate(b);
    if (b->phase >= ZN_PHASE_END) {       /* 原版：>= 120 ⇒ Kill() */
        b->active = 0;
        out->done = 1;
        return;
    }

    L = b->phase / ZN_PHASE_SWING;
    if (L > 1.0f) L = 1.0f;               /* GetLerpValue(0,1,phase/60) */

    out->ang = BC_PI + zn_sign(b) * L * (2.0f * BC_PI);         /* num9 */
    {
        float vlen = zn_vlen(b);                                       /* |velocity| */
        out->R = vlen + clamp01((L - 0.5f) / 0.5f) * ZN_RADIUS_ADD;    /* |v| + GetLerpValue(0.5,1,L)·40 */
        if (out->R < ZN_RADIUS_MIN) out->R = ZN_RADIUS_MIN;            /* max(60, ·) */
    }
    out->A = b->ai0 * lerp2(2.0f, 1.0f, zn_fast(b));            /* ai[0] · Lerp(2,1,fast) */
    k = 1.0f - clamp01(L / 0.5f);                               /* 1 − GetLerpValue(0,0.5,L) */
    out->fade_x = k * (zn_sign(b) * -out->R * 0.1f);
    out->fade_y = k * (-b->ai0 * 0.3f);
    out->rot = out->ang + b->heading + BC_HALF_PI;              /* rotation = num9 + heading + π/2 */

    op = clamp01(b->phase / ZN_FADE_IN) * clamp01((ZN_PHASE_END - b->phase) / ZN_FADE_OUT);
    out->alpha = (int)(op * 255.0f + 0.5f);

    b->trail_timer++;
    if (b->trail_timer >= ZN_TRAIL_EVERY) { b->trail_timer = 0; out->emit_trail = 1; }
    b->hit_timer++;
    if (b->hit_timer >= ZN_HIT_COOLDOWN) { b->hit_timer = 0; out->hit = 1; }
}

int zn_live(const zn_state_t *s)
{
    int i, n = 0;
    for (i = 0; i < ZN_BLADE_MAX; ++i)
        if (s->b[i].active) n++;
    return n;
}

void zn_aim_reset(zn_aim_t *a)
{
    a->found = 0;
    a->best_d2 = ZN_RANGE_SQ;             /* 一开始就是射程：射程外的自然进不来 */
    a->dx = 0.0f;
    a->dy = 0.0f;
}

void zn_aim_consider(zn_aim_t *a, float ox, float oy, float ex, float ey)
{
    float dx = ex - ox, dy = ey - oy;
    float d2 = dx * dx + dy * dy;
    if (d2 < a->best_d2) {
        a->found = 1;
        a->best_d2 = d2;
        a->dx = dx;
        a->dy = dy;
    }
}

int zn_aim_ready(const zn_aim_t *a)
{
    return a->found;
}
