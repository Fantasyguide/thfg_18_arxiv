/* 火球术（id 83）的纯逻辑。见 fireball_core.h。 */
#include "fireball_core.h"

void fb_launch(fb_state_t *s, float px, float py)
{
    uint32_t keep = s->anm_id;            /* VM id 由引擎侧管，清状态时别顺手抹掉 */
    fb_state_t z = { 0, 0.0f, 0.0f, 0, 0.0f, 0.0f, 0, 0, 0 };
    *s = z;
    s->anm_id = keep;
    s->active = 1;
    s->x = px;
    s->y = py + FB_SPAWN_DY;
}

int fb_in_range(float bx, float by, float ex, float ey)
{
    float dx = ex - bx, dy = ey - by;
    return dx * dx + dy * dy < FB_HIT_R * FB_HIT_R;
}

int fb_offscreen(float x, float y)
{
    return x < -FB_FIELD_X_HALF - FB_EXIT_MARGIN || x > FB_FIELD_X_HALF + FB_EXIT_MARGIN
        || y < -FB_EXIT_MARGIN                   || y > FB_FIELD_H + FB_EXIT_MARGIN;
}

fb_step_t fb_step(fb_state_t *s, int hit)
{
    fb_step_t out = { 0, 0, 0, 0, 0 };

    if (s->blast_left > 0) {              /* 爆炸中：每帧一个新伤害源（一个源对同一敌人只结算一次，AUDIT U9）*/
        out.blast_dmg = FL_BLAST_DMG;
        if (--s->blast_left == 0) out.done = 1;
        return out;
    }
    if (!s->active) { out.done = 1; return out; }

    s->frames++;
    s->y -= FB_SPEED;                     /* 正前方 = 画面上方；弹幕区 y 向下为正 */

    if (hit) {                            /* 碰到第一个敌人：就地爆炸 */
        s->active = 0;
        s->ex = s->x; s->ey = s->y;
        s->blast_left = FL_BLAST_FRAMES;
        out.hit = 1;
        return out;
    }
    if (fb_offscreen(s->x, s->y) || s->frames >= FB_MAX_FRAMES) {   /* 空了：什么都不炸 */
        s->active = 0;
        out.missed = 1;
        out.done = 1;
        return out;
    }
    out.trail = (s->frames % FL_TRAIL_EVERY) == 0;
    return out;
}
