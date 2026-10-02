/* star_cannon_core.c —— 星星炮的纯逻辑。数值来源与对位表在 star_cannon_core.h 的文件头
 *（一手 = 反编译 1.4.4.5 的 `Item.cs` / `Projectile.cs`）。只用加减乘除，零 sin/cos、零 x87。 */
#include <string.h>

#include <math.h>

#include "star_cannon_core.h"

void sc_reset(sc_state_t *s)
{
    memset(s, 0, sizeof *s);
}

int sc_charge(sc_state_t *s)
{
    s->frames++;
    if (s->charge < SC_PERIOD) s->charge++;
    return s->charge >= SC_PERIOD;
}

void sc_did_fire(sc_state_t *s)
{
    s->charge = 0;
    s->fired++;
}

sc_shot_t *sc_slot(sc_state_t *s)
{
    int i;
    for (i = 0; i < SC_SHOT_MAX; ++i)
        if (!s->s[i].active) return &s->s[i];
    return 0;
}

int sc_live(const sc_state_t *s)
{
    int i, n = 0;
    for (i = 0; i < SC_SHOT_MAX; ++i)
        if (s->s[i].active) n++;
    return n;
}

void sc_launch(sc_shot_t *sh, uint32_t id, float x, float y, float vx, float vy)
{
    sh->active = 1;
    sh->id = id;
    sh->x = x;
    sh->y = y;
    sh->vx = vx;
    sh->vy = vy;
    sh->rot = 0.0f;
    sh->alpha = 0;                 /* 一手 `SetDefaults` 的 `alpha = 50` 是**贴图自带**的那一档；
                                    * aiStyle 5 每帧从 0 起往上加（localAI[0] 一开始是 0 ⇒ 先被置 1）*/
    sh->alpha_dir = 1;             /* `if (localAI[0] == 0f) localAI[0] = 1f` */
    sh->frames = 0;
    sh->hit = 1;                   /* 出生那一帧就能命中（原版第一帧就在判定）*/
    sh->hit_timer = SC_HIT_COOLDOWN;
}

int sc_step(sc_shot_t *sh)
{
    const float dir = (sh->vx >= 0.0f) ? 1.0f : -1.0f;   /* 原版 `direction` = sign(velocity.X) */

    if (!sh->active) return 1;
    sh->frames++;
    if (sh->frames > 0xF000u) sh->frames = 0xF000u;      /* uint16 不会绕回（一颗星活不了那么久）*/
    sh->x += sh->vx;                                     /* aiStyle 5 不加速度、不加重力 */
    sh->y += sh->vy;
    /* ★ 原版公式本来就是「按速度自转」：`rotation += (|vx|+|vy|)·0.01·direction` ⇒ 弹速减半时
     *   自转自动跟着减半（owner 2026-10-03「弹速慢一倍」），不用另配常量。*/
    sh->rot += (fabsf(sh->vx) + fabsf(sh->vy)) * 0.01f * dir;
    if ((uint8_t)(sh->hit_timer + 1u) >= (uint8_t)SC_HIT_COOLDOWN) {
        sh->hit_timer = 0;
        sh->hit = 1;
    } else {
        sh->hit_timer = (uint8_t)(sh->hit_timer + 1u);
        sh->hit = 0;
    }
    sh->alpha = (int16_t)(sh->alpha + SC_ALPHA_STEP * sh->alpha_dir);
    if (sh->alpha > SC_ALPHA_MAX) { sh->alpha = SC_ALPHA_MAX; sh->alpha_dir = -1; }
    if (sh->alpha < 0) { sh->alpha = 0; sh->alpha_dir = 1; }
    if (sh->x < -SC_VIEW_X || sh->x > SC_VIEW_X ||
        sh->y < SC_VIEW_Y_MIN || sh->y > SC_VIEW_Y_MAX) {
        sh->active = 0;
        return 1;                                        /* 出界即收（原版撞方块 / timeLeft 用尽）*/
    }
    return 0;
}

int sc_draw_alpha(const sc_shot_t *sh)
{
    int a = 255 - sh->alpha;                             /* 原版把 alpha 加到**不透明度**的反面 */
    if (a < 0) a = 0;
    if (a > 255) a = 255;
    return a;
}

void sc_aim_reset(sc_aim_t *a)
{
    a->found = 0;
    a->best_d2 = SC_RANGE_SQ;
    a->dx = 0.0f;
    a->dy = 0.0f;
}

void sc_aim_consider(sc_aim_t *a, float ox, float oy, float ex, float ey)
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

int sc_aim_ready(const sc_aim_t *a)
{
    return a->found;
}
