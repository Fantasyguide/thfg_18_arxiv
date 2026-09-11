/* policy.c —— 最朴素的「一直射击 + 躲弹」。
 *
 * 每帧枚举 18 个候选（八向 + 不动，各配常速 / 低速），把每颗飞行中的弹按
 * pos + vel × t 线性外推 horizon 帧，自机按候选速度走（撞可动区边界就停），
 * 取整个时间窗里「自机到最近弹的净距」作为该候选的安全度；敌人本体按静止盒算
 * （判定式与 Player__CalcKillBoxCollision 0x14006a980 两条分支同式）。
 * 选安全度最高的；安全度都为正时按次要目标（对齐 boss x、回到 home_y、少动）打破平手。
 * 每个候选都躲不开且马上要中弹 → 用炸弹（有炸弹、且冷却过了）。
 *
 * 不做的：激光、道具、符卡时间、擦弹。这是验证「读内存 → 决策 → 写输入」整条链路的原型。 */
#include <math.h>
#include <string.h>
#include "policy.h"

#define MARGIN_CAP 40.0f      /* 净距超过它就不再更「安全」，让次要目标起作用 */

void ap_policy_init(ap_policy_t *p)
{
    memset(p, 0, sizeof *p);
    p->horizon = 24;
    p->safe_margin = 12.0f;
    p->home_y = 384.0f;
    p->bomb_cooldown = 90;
    p->last_bomb_frame = 0xFFFFFFFFu;   /* 从未炸过 */
}

static float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

float ap_policy_score_move(const ap_world_t *w, float vx, float vy, int horizon, float *first_hit_t)
{
    float px = w->player.x, py = w->player.y, r = w->player.hit_radius;
    float worst = 1e9f;
    int t, i;
    *first_hit_t = 0.0f;
    for (t = 1; t <= horizon; t++) {
        float ft = (float)t;
        px = clampf(px + vx, w->area.xmin, w->area.xmax);
        py = clampf(py + vy, w->area.ymin, w->area.ymax);
        for (i = 0; i < w->nbullets; i++) {
            const ap_bullet_t *b = &w->bullets[i];
            float bx = b->x + b->vx * ft, by = b->y + b->vy * ft;
            float dx = bx - px, dy = by - py;
            float m = sqrtf(dx * dx + dy * dy) - (r + b->radius);
            if (m < worst) { worst = m; if (m < 0.0f && *first_hit_t == 0.0f) *first_hit_t = ft; }
        }
        for (i = 0; i < w->nenemies; i++) {
            /* 与引擎同式：圆对轴对齐盒 —— 自机圆心钳到盒内取最近点，再比半径 */
            const ap_enemy_t *e = &w->enemies[i];
            float cx, cy, dx, dy, m;
            if (!e->collidable) continue;
            cx = clampf(px, e->x - e->hit_w, e->x + e->hit_w);
            cy = clampf(py, e->y - e->hit_h, e->y + e->hit_h);
            dx = px - cx; dy = py - cy;
            m = sqrtf(dx * dx + dy * dy) - r;
            if (m < worst) { worst = m; if (m < 0.0f && *first_hit_t == 0.0f) *first_hit_t = ft; }
        }
    }
    return worst;
}

static const int DIRS[9][2] = { {0,0}, {0,-1}, {0,1}, {-1,0}, {1,0}, {-1,-1}, {1,-1}, {-1,1}, {1,1} };
static const uint32_t DIR_BITS[9] = { 0, BTN_UP, BTN_DOWN, BTN_LEFT, BTN_RIGHT,
                                      BTN_UP|BTN_LEFT, BTN_UP|BTN_RIGHT, BTN_DOWN|BTN_LEFT, BTN_DOWN|BTN_RIGHT };

uint32_t ap_policy_decide(ap_policy_t *p, const ap_world_t *w)
{
    int d, f, i, best = -1;
    float best_score = -1e9f, best_margin = 0.0f, best_hit_t = 0.0f;
    float target_x = 0.0f;
    uint32_t out;

    if (w->player.state != 0 && w->player.state != 3) return 0;   /* 出生 / 死亡：不插手 */

    /* 次要目标的 x：有 boss 对齐 boss，否则对齐第一个敌人，否则中轴 */
    for (i = 0; i < w->nenemies; i++) if (w->enemies[i].boss) { target_x = w->enemies[i].x; break; }
    if (i == w->nenemies && w->nenemies > 0) target_x = w->enemies[0].x;

    for (f = 0; f < 2; f++) {
        float sp = f ? w->player.speed_focus : w->player.speed;
        if (sp <= 0.0f) sp = f ? 2.0f : 4.0f;
        for (d = 0; d < 9; d++) {
            float vx = (float)DIRS[d][0] * sp, vy = (float)DIRS[d][1] * sp, hit_t, m, score, ex, ey;
            if (DIRS[d][0] && DIRS[d][1]) { vx *= 0.70710678f; vy *= 0.70710678f; }
            m = ap_policy_score_move(w, vx, vy, p->horizon, &hit_t);
            score = m < MARGIN_CAP ? m : MARGIN_CAP;
            ex = clampf(w->player.x + vx * (float)p->horizon, w->area.xmin, w->area.xmax);
            ey = clampf(w->player.y + vy * (float)p->horizon, w->area.ymin, w->area.ymax);
            score -= 0.02f * (fabsf(ex - target_x) + fabsf(ey - p->home_y));
            score -= 0.001f * sp * (float)(DIRS[d][0] != 0 || DIRS[d][1] != 0);
            if (f) score -= 0.0005f;              /* 同分时偏好常速 */
            if (score > best_score) { best_score = score; best = f * 9 + d; best_margin = m; best_hit_t = hit_t; }
        }
    }

    out = BTN_SHOT | DIR_BITS[best % 9] | (best >= 9 ? BTN_SLOW : 0);

    if (best_margin < 0.0f && best_hit_t > 0.0f && best_hit_t <= 6.0f && w->player.bombs > 0) {
        int cooled = p->last_bomb_frame == 0xFFFFFFFFu
                  || (w->frame - p->last_bomb_frame) > (uint32_t)p->bomb_cooldown;
        if (cooled) { out |= BTN_BOMB; p->last_bomb_frame = w->frame; }
    }
    p->prev_buttons = out;
    return out;
}
