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

/* ★★ 每把剑的**贴图基准朝向**（引擎 `rotation = 0` 时剑尖的指向，弧度）：
 *   owner 2026-10-03：「刀的甩出去的**旋转和运动方向不太一致**」「**得翻转贴图**的，**种子弯刀是刀**」。
 *   原版 `AI_182` 的 `rotation = radians + heading + π/2` 是按**弹幕贴图（正上朝，base = −π/2）**推的；
 *   我们用的是 Terraria **物品贴图** —— 21 张逐张量主轴（协方差最大特征向量）**全是斜的**（剑尖在右上）。
 *   ⇒ `rot = ang + heading − base`；单张的 base 让**弯刀/异形剑**（种子弯刀 −60.8°、村正 −50.3°……）也各归各位。
 *   量法：`assets/ability/zenith/_src/sword_*.png` 的 alpha>40 像素做 PCA 取主轴角（负 = 右上）。*/
static const float s_zn_base[ZN_PROFILE_N] = {
    -0.7976f,  /*    65 星怒  -45.7° */
    -0.7854f,  /*  1123 养蜂人  -45.0° */
    -0.7941f,  /*    46 光之灾  -45.5° */
    -0.7854f,  /*   121 炽焰巨剑  -45.0° */
    -0.7906f,  /*   190 草剑  -45.3° */
    -0.7941f,  /*   368 断钢剑  -45.5° */
    -0.7837f,  /*   674 真断钢剑  -44.9° */
    -0.8622f,  /*   273 暗影焰刃  -49.4° */
    -0.9320f,  /*   675 真暗影焰刃  -53.4° */
    -0.7854f,  /*  2880 波涌之刃  -45.0° */
    -0.7854f,  /*   989 附魔剑  -45.0° */
    -0.7854f,  /*  1826 无头骑士剑  -45.0° */
    -0.8325f,  /*  3063 喵刀  -47.7° */
    -0.8570f,  /*  3065 狂星之怒  -49.1° */
    -0.8395f,  /*   757 泰拉刃  -48.1° */
    -0.8779f,  /*   155 村正  -50.3° */
    -0.8308f,  /*   795 嗜血者  -47.6° */
    -1.0612f,  /*  3018 种子弯刀  -60.8° */
    -0.7854f,  /*  4144 大地之刃  -45.0° */
    -0.7854f,  /*  3507 铜短剑  -45.0° */
    -0.7854f,  /*  4956 天顶剑本体  -45.0° */
};

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
    b->hit_timer = (uint8_t)ZN_HIT_COOLDOWN;   
/* 出生那一帧就能命中（原版第一帧就在判定）*/
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

    /* ★ 2026-10-03（owner：「天顶剑是**既有逆时针也有顺时针**的」）：原版 `ai0 < 0` 那一支走的是
     *   `rotation = π + num4·L·(−2π) + …` ⇒ **反向扫**；扫向 = ai0 的符号 × sign(vx)。
     *   ⚠ `fade_x` 仍用**不翻**的 `sign(vx)`（原版 `vector2_2` 那行没翻）。*/
    out->ang = BC_PI + ((b->ai0 >= 0.0f) ? zn_sign(b) : -zn_sign(b)) * L * (2.0f * BC_PI);   /* num9 */
    {
        float vlen = zn_vlen(b);                                       /* |velocity| */
        out->R = vlen + clamp01((L - 0.5f) / 0.5f) * ZN_RADIUS_ADD;    /* |v| + GetLerpValue(0.5,1,L)·40 */
        if (out->R < ZN_RADIUS_MIN) out->R = ZN_RADIUS_MIN;            /* max(60, ·) */
    }
    out->A = b->ai0 * lerp2(2.0f, 1.0f, zn_fast(b));            /* ai[0] · Lerp(2,1,fast) */
    k = 1.0f - clamp01(L / 0.5f);                               /* 1 − GetLerpValue(0,0.5,L) */
    out->fade_x = k * (zn_sign(b) * -out->R * 0.1f);
    out->fade_y = k * (-b->ai0 * 0.3f);
    /* rotation = num9 + heading + π/2（原版），再**减掉这把剑的贴图基准朝向**：
     *   原版弹幕贴图正上朝（base = −π/2）而我们用物品贴图（每张斜得不一样，见 `s_zn_base[]`）
     *   ⇒ `rot = ang + heading − base`。`tip` = 剑尖指向（与贴图无关的那一项）。*/
    {
        float base = (b->prof >= 0 && b->prof < ZN_PROFILE_N) ? s_zn_base[b->prof] : ZN_SPRITE_BASE;
        out->rot = out->ang + b->heading - base;
        out->tip = out->ang + b->heading;
    }
    /* 原版 `spriteDirection`：`ai0 >= 0` 时 = sign(vx)、`ai0 < 0` 时 = −sign(vx)；= −1 就翻转贴图。*/
    out->mirror = ((b->ai0 >= 0.0f) ? !b->sign_pos : (b->sign_pos != 0u)) ? 1 : 0;

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
    a->best_d2 = 0.0f;                    /* ★ 取最远（见 consider）：从 0 往上比；射程由调用方先滤 */
    a->dx = 0.0f;
    a->dy = 0.0f;
}

/* ★★ 2026-10-03 owner：「把追踪改成**最远的敌人** —— 天顶剑的一般攻击方式，这样能**打更多敌人**」。
 *   ⇒ 从"取最近"改成"取**射程内最远**"：剑朝最远的那个敌人甩过去，飞行路径上会**扫过**沿途的敌人
 *     （近处的反而更容易吃到），这是原版那把剑清场的观感。
 *   实现：`best_d2` 初值 = 射程平方，判据反过来取**更大**的 d2（射程外的仍旧进不来）。*/
void zn_aim_consider(zn_aim_t *a, float ox, float oy, float ex, float ey)
{
    float dx = ex - ox, dy = ey - oy;
    float d2 = dx * dx + dy * dy;
    if (d2 > ZN_RANGE_SQ) return;          /* ⚠ 射程外不要（原来靠 best_d2 的初值当射程，取最远后必须显式滤）*/
    if (d2 > a->best_d2) {
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
