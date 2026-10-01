/* zenith_core.c —— 天顶剑的纯逻辑（头文件里写了每一条的出处 / ❓ 口径）。
 * 只用加减乘除与 `__builtin_sqrtf`（Makefile 的 `-fno-math-errno` ⇒ 一条 sqrtss）：不引 libm、零 x87。 */
#include <string.h>

#include "zenith_core.h"

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

void zn_launch(zn_blade_t *b, uint32_t id, float ox, float oy, float dx, float dy)
{
    float d = __builtin_sqrtf(dx * dx + dy * dy);

    b->active = 1;
    b->id = id;
    b->x = ox;
    b->y = oy;
    b->vx = d > 0.0f ? dx * (ZN_SPEED / d) : 0.0f;
    b->vy = d > 0.0f ? dy * (ZN_SPEED / d) : 0.0f;
    b->rot = 0.0f;
    b->life = ZN_LIFE;
}

void zn_advance(zn_blade_t *b)
{
    if (!b->active) return;
    b->x += b->vx;
    b->y += b->vy;
    b->rot += ZN_SPIN;
    if (b->life > 0) b->life--;
    if (b->life == 0 || b->x < ZN_X_MIN || b->x > ZN_X_MAX ||
        b->y < ZN_Y_MIN || b->y > ZN_Y_MAX)
        b->active = 0;
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
    a->best_d2 = ZN_RANGE_SQ;        /* 一开始就是射程：射程外的自然进不来 */
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
