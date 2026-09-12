/* 策略的判别式测试：几何取值要能区分对错。前 10 条从采样版继承（行为契约不变），
 * 11–13 是势场版特有的。 */
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "../policy.h"
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static ap_world_t W;

static void reset(void)
{
    memset(&W, 0, sizeof W);
    W.frame = 100;
    W.phase = AP_PHASE_IN_GAME | AP_PHASE_PLAYER_CONTROLLABLE;
    W.player.x = 0.0f; W.player.y = 384.0f;
    W.player.hit_radius = 1.25f;
    W.player.speed = 4.0f; W.player.speed_focus = 2.0f;
    W.player.state = 0; W.player.lives = 3; W.player.bombs = 2;
    W.area.xmin = -184.0f; W.area.xmax = 184.0f; W.area.ymin = 16.0f; W.area.ymax = 432.0f;
}
static void laser(float x, float y, float angle, float start, float end, float half_h, float speed, int t_active)
{
    ap_laser_t *l = &W.lasers[W.nlasers++];
    l->x = x; l->y = y; l->angle = angle; l->start = start; l->end = end; l->start_len = 1e6f;
    l->speed = speed; l->half_h = half_h; l->t_active = t_active; l->state = t_active ? 0 : 1;
}
static void bullet(float x, float y, float vx, float vy, float r)
{
    ap_bullet_t *b = &W.bullets[W.nbullets++];
    b->x = x; b->y = y; b->vx = vx; b->vy = vy; b->radius = r; b->state = 1;
}
/* 把一次决策施加到世界上：自机按动作走一帧，弹按速度走一帧 */
static void step_world(uint32_t a)
{
    float sp = (a & BTN_SLOW) ? W.player.speed_focus : W.player.speed;
    float dx = ((a & BTN_RIGHT) ? 1.0f : 0.0f) - ((a & BTN_LEFT) ? 1.0f : 0.0f);
    float dy = ((a & BTN_DOWN) ? 1.0f : 0.0f) - ((a & BTN_UP) ? 1.0f : 0.0f);
    if (dx != 0.0f && dy != 0.0f) { dx *= 0.70710678f; dy *= 0.70710678f; }
    W.player.x += dx * sp; W.player.y += dy * sp;
    for (int i = 0; i < W.nbullets; i++) { W.bullets[i].x += W.bullets[i].vx; W.bullets[i].y += W.bullets[i].vy; }
    W.frame++;
}

int main(void)
{
    ap_policy_t P;
    uint32_t a;
    float t, fx, fy;

    /* 1. 没弹：射击，不炸，不乱动 */
    reset(); ap_policy_init(&P);
    a = ap_policy_decide(&P, &W);
    assert(a & BTN_SHOT);
    assert(!(a & BTN_BOMB));
    assert(!(a & (BTN_LEFT | BTN_RIGHT)));

    /* 2. 一颗弹正对着头顶直落（x=0），速度 4/帧，20 帧后到 —— 必须横向让开 */
    reset(); ap_policy_init(&P);
    bullet(0.0f, 384.0f - 80.0f, 0.0f, 4.0f, 6.0f);
    a = ap_policy_decide(&P, &W);
    assert(a & (BTN_LEFT | BTN_RIGHT));
    assert(!(a & BTN_BOMB));

    /* 3. 外推函数本身：站着不动的净距应为负（会被打中），向右走 4/帧 应为正 */
    assert(ap_policy_score_move(&W, 0.0f, 0.0f, 24, &t) < 0.0f);
    assert(t > 0.0f && t <= 24.0f);
    assert(ap_policy_score_move(&W, 4.0f, 0.0f, 24, &t) > 0.0f);

    /* 4. 左边贴墙（x = xmin），弹从右上斜着来 —— 不能选左（会撞墙），得选别的且不被打中 */
    reset(); ap_policy_init(&P);
    W.player.x = -184.0f;
    bullet(-184.0f + 60.0f, 384.0f - 60.0f, -3.0f, 3.0f, 6.0f);   /* 20 帧后到自机 */
    a = ap_policy_decide(&P, &W);
    assert(!(a & BTN_BOMB));
    assert(a & (BTN_UP | BTN_DOWN | BTN_RIGHT));

    /* 5. 四面围死：弹环从四周同时收拢、2 帧后全部到达，任何走法都躲不开 → 用炸弹（有炸弹时） */
    reset(); ap_policy_init(&P);
    for (int i = 0; i < 36; i++) {
        float ang = (float)i * (float)M_PI * 2.0f / 36.0f;
        bullet(cosf(ang) * 14.0f, 384.0f + sinf(ang) * 14.0f, -cosf(ang) * 6.0f, -sinf(ang) * 6.0f, 6.0f);
    }
    a = ap_policy_decide(&P, &W);
    assert(a & BTN_BOMB);
    /* 6. 同样局面但没炸弹：不能按 bomb，仍给出一个动作 */
    W.player.bombs = 0; ap_policy_init(&P);
    a = ap_policy_decide(&P, &W);
    assert(!(a & BTN_BOMB));
    /* 7. bomb 冷却：刚炸过就不再按 */
    W.player.bombs = 2; ap_policy_init(&P); P.last_bomb_frame = W.frame - 1;
    a = ap_policy_decide(&P, &W);
    assert(!(a & BTN_BOMB));

    /* 8. 出生/死亡态：什么都不按 */
    reset(); ap_policy_init(&P); W.player.state = 1;
    assert(ap_policy_decide(&P, &W) == 0);

    /* 9. 敌人本体也是障碍：头顶正上方 30px、半宽半高 16、可碰撞 —— 向上走必须被判危险 */
    reset(); ap_policy_init(&P);
    W.enemies[W.nenemies].x = 0.0f; W.enemies[W.nenemies].y = 384.0f - 30.0f;
    W.enemies[W.nenemies].hit_w = 16.0f; W.enemies[W.nenemies].hit_h = 16.0f;
    W.enemies[W.nenemies].collidable = 1; W.nenemies++;
    assert(ap_policy_score_move(&W, 0.0f, -4.0f, 24, &t) < 0.0f);
    assert(ap_policy_score_move(&W, 0.0f, 0.0f, 24, &t) > 0.0f);
    /* 10. 同一个敌人但引擎不拿它撞自机（collidable=0）：向上走不该被判危险 */
    W.enemies[0].collidable = 0;
    assert(ap_policy_score_move(&W, 0.0f, -4.0f, 24, &t) > 0.0f);

    /* 11. 势场特有：迎面弹在 100 px 外（25 帧后到）就已有明显的侧向推力，且决策已横移；
     *     而 400 px 外（100 帧后到）的同一颗弹，时间折扣后推力低于起动阈值，不动 */
    reset(); ap_policy_init(&P);
    bullet(0.0f, 384.0f - 100.0f, 0.0f, 4.0f, 6.0f);
    ap_policy_field(&P, &W, &fx, &fy);
    assert(fabsf(fx) > P.f_min);
    assert(fabsf(fx) > fabsf(fy));              /* 推力主要是横向的（切向项在起作用） */
    a = ap_policy_decide(&P, &W);
    assert(a & (BTN_LEFT | BTN_RIGHT));
    reset(); ap_policy_init(&P);
    bullet(0.0f, 384.0f - 400.0f, 0.0f, 4.0f, 6.0f);
    ap_policy_field(&P, &W, &fx, &fy);
    assert(sqrtf(fx * fx + fy * fy) < P.f_min);
    a = ap_policy_decide(&P, &W);
    assert(!(a & (BTN_LEFT | BTN_RIGHT | BTN_UP | BTN_DOWN)));

    /* 12. 擦身弹：从左往右横穿、最近距离 100 px（> safe_r），几乎无推力 */
    reset(); ap_policy_init(&P);
    bullet(-100.0f, 384.0f - 100.0f, 4.0f, 0.0f, 6.0f);
    ap_policy_field(&P, &W, &fx, &fy);
    assert(sqrtf(fx * fx + fy * fy) < P.f_min);

    /* 13. 弹流前不来回顶：正上方一列弹（每 40 px 一颗）持续下落，模拟 40 帧，
     *     横向方向最多改变一次（第一次选边后就顺着走），且全程不被打中 */
    reset(); ap_policy_init(&P);
    for (int i = 0; i < 8; i++) bullet(0.0f, 384.0f - 60.0f - 40.0f * i, 0.0f, 4.0f, 6.0f);
    {
        int flips = 0, last = 0, hit = 0;
        for (int f = 0; f < 40; f++) {
            a = ap_policy_decide(&P, &W);
            int h = (a & BTN_RIGHT) ? 1 : (a & BTN_LEFT) ? -1 : 0;
            if (h != 0 && last != 0 && h != last) flips++;
            if (h != 0) last = h;
            step_world(a);
            for (int i = 0; i < W.nbullets; i++) {
                float dx = W.bullets[i].x - W.player.x, dy = W.bullets[i].y - W.player.y;
                if (sqrtf(dx * dx + dy * dy) < W.bullets[i].radius + W.player.hit_radius) hit = 1;
            }
        }
        assert(flips <= 1);
        assert(!hit);
    }

    /* 14. 有效激光：水平（angle=0）从 (−100, 354) 向右伸到 x=+100，半高 4 —— 自机在 (0,384)，
     *     向上走 4/帧 到 y≈358 就进盒 → 危险；不动安全；势场把自机往下推 */
    reset(); ap_policy_init(&P);
    laser(-100.0f, 384.0f - 30.0f, 0.0f, 0.0f, 200.0f, 4.0f, 0.0f, 0);
    assert(ap_policy_score_move(&W, 0.0f, -4.0f, 24, &t) < 0.0f);
    assert(ap_policy_score_move(&W, 0.0f, 0.0f, 24, &t) > 0.0f);
    ap_policy_field(&P, &W, &fx, &fy);
    assert(fy > 0.0f && fabsf(fy) > fabsf(fx));
    /* 15. 同一条激光但只覆盖 x∈[−100,−20]（end=80）：自机在 x=0 不在盒的沿线范围内，向上走不该被判危险 */
    reset(); ap_policy_init(&P);
    laser(-100.0f, 384.0f - 30.0f, 0.0f, 0.0f, 80.0f, 4.0f, 0.0f, 0);
    assert(ap_policy_score_move(&W, 0.0f, -4.0f, 24, &t) > 0.0f);
    /* 16. 旋转：竖直激光（angle=π/2）从 (30, 200) 向下伸 300，半高 4 —— 向右走 4/帧 会进盒，向左不会 */
    reset(); ap_policy_init(&P);
    laser(30.0f, 200.0f, (float)M_PI / 2.0f, 0.0f, 300.0f, 4.0f, 0.0f, 0);
    assert(ap_policy_score_move(&W, 4.0f, 0.0f, 24, &t) < 0.0f);
    assert(ap_policy_score_move(&W, -4.0f, 0.0f, 24, &t) > 0.0f);
    /* 17. 预警激光正压在自机身上，10 帧后开始杀：现在不算撞（t_active 内），但势场已经推开、决策已移动；
     *     同一条激光 200 帧后才开始杀：时间折扣后不动 */
    reset(); ap_policy_init(&P);
    laser(-100.0f, 384.0f, 0.0f, 0.0f, 200.0f, 4.0f, 0.0f, 10);
    assert(ap_policy_score_move(&W, 0.0f, 0.0f, 5, &t) > 0.0f);     /* 5 帧内不杀 */
    assert(ap_policy_score_move(&W, 0.0f, 0.0f, 24, &t) < 0.0f);    /* 24 帧内会杀 */
    a = ap_policy_decide(&P, &W);
    assert(a & (BTN_UP | BTN_DOWN));
    reset(); ap_policy_init(&P);
    laser(-100.0f, 384.0f, 0.0f, 0.0f, 200.0f, 4.0f, 0.0f, 200);
    a = ap_policy_decide(&P, &W);
    assert(!(a & (BTN_UP | BTN_DOWN | BTN_LEFT | BTN_RIGHT)));
    /* 18. 伸长中的激光：从 (0, 200) 向下、现在只到 y=300、每帧 +10 —— 24 帧内会伸到自机（384）→ 不动危险，横移安全 */
    reset(); ap_policy_init(&P);
    laser(0.0f, 200.0f, (float)M_PI / 2.0f, 0.0f, 100.0f, 4.0f, 10.0f, 0);
    assert(ap_policy_score_move(&W, 0.0f, 0.0f, 24, &t) < 0.0f);
    assert(ap_policy_score_move(&W, 4.0f, 0.0f, 24, &t) > 0.0f);

    /* 19. 无弹时不振荡：自机从 (60, 410) 起，只有引力和墙，跑 120 帧——
     *     最后 30 帧一步都不走，且横向 / 纵向方向翻转各不超过 1 次；终点在中下（y 在 home 附近、x 靠中轴） */
    reset(); ap_policy_init(&P);
    W.player.x = 60.0f; W.player.y = 410.0f;
    {
        int fl_h = 0, fl_v = 0, lh = 0, lv = 0, moved_late = 0;
        for (int f = 0; f < 120; f++) {
            a = ap_policy_decide(&P, &W);
            int h = (a & BTN_RIGHT) ? 1 : (a & BTN_LEFT) ? -1 : 0;
            int v = (a & BTN_DOWN) ? 1 : (a & BTN_UP) ? -1 : 0;
            if (h && lh && h != lh) fl_h++;
            if (v && lv && v != lv) fl_v++;
            if (h) lh = h;
            if (v) lv = v;
            if (f >= 90 && (h || v)) moved_late = 1;
            step_world(a);
        }
        assert(!moved_late);
        assert(fl_h <= 1 && fl_v <= 1);
        assert(fabsf(W.player.x) < 20.0f);
        assert(fabsf(W.player.y - P.home_y) < 20.0f);
    }
    /* 20. 底边不把自机顶飞：站在 y=428（离底 4px）无弹 —— 底边斥力 + 回 home 的引力都向上，
     *     但合力必须小于左 / 右 / 上边同样贴 4px 时的斥力（底边单独减弱） */
    reset(); ap_policy_init(&P);
    W.player.y = 428.0f; ap_policy_field(&P, &W, &fx, &fy);
    {
        float fy_bottom = -fy, fx_side;
        reset(); W.player.x = -180.0f; ap_policy_field(&P, &W, &fx_side, &fy);
        assert(fy_bottom > 0.0f && fy_bottom < fx_side);
    }

    /* 21. 横向飞来、正穿过自机的弹（从左边 y=384 向右）：推力应向下（远离 boss 那一侧），不能向上 */
    reset(); ap_policy_init(&P);
    bullet(-80.0f, 384.0f, 4.0f, 0.0f, 6.0f);
    ap_policy_field(&P, &W, &fx, &fy);
    assert(fy > 0.0f);
    a = ap_policy_decide(&P, &W);
    assert(a & BTN_DOWN);
    assert(!(a & BTN_UP));
    /* 同样的弹但自机在 y=420（home 之下）：这时朝 home 那一侧是向上，允许向上让 */
    reset(); ap_policy_init(&P); W.player.y = 420.0f;
    bullet(-80.0f, 420.0f, 4.0f, 0.0f, 6.0f);
    ap_policy_field(&P, &W, &fx, &fy);
    assert(fy < 0.0f);

    /* 22. 去抖：弹从正上方偏右 2 px 直落（合力几乎正左，略带分量落在扇区边界附近），
     *     躲避阶段（前 30 帧）方向改变（含起停）不超过 3 次，且不被打中。
     *     只数前 30 帧：之后安静计数满、引力把自机拉回中轴是正常回位，不是抖动 */
    reset(); ap_policy_init(&P);
    bullet(2.0f, 384.0f - 80.0f, 0.0f, 4.0f, 6.0f);
    {
        int changes = 0, hit = 0;
        uint32_t last = 0xFFFFFFFFu;
        for (int f = 0; f < 30; f++) {
            uint32_t dir;
            a = ap_policy_decide(&P, &W);
            dir = a & (BTN_UP | BTN_DOWN | BTN_LEFT | BTN_RIGHT);
            if (last != 0xFFFFFFFFu && dir != last) changes++;
            last = dir;
            step_world(a);
            for (int i = 0; i < W.nbullets; i++) {
                float dx = W.bullets[i].x - W.player.x, dy = W.bullets[i].y - W.player.y;
                if (sqrtf(dx * dx + dy * dy) < W.bullets[i].radius + W.player.hit_radius) hit = 1;
            }
        }
        assert(changes <= 3);
        assert(!hit);
    }
    /* 23. 低速键不翻转：一颗弹在自机上方 45 px 处从左往右掠过（最近距离在 40 px 阈值附近来回），
     *     100 帧内 SLOW 位最多翻转 2 次 */
    reset(); ap_policy_init(&P);
    bullet(-200.0f, 384.0f - 45.0f, 4.0f, 0.0f, 6.0f);
    {
        int toggles = 0, last = -1;
        for (int f = 0; f < 100; f++) {
            int sl;
            a = ap_policy_decide(&P, &W);
            sl = (a & BTN_SLOW) ? 1 : 0;
            if (last >= 0 && sl != last) toggles++;
            last = sl;
            step_world(a);
        }
        assert(toggles <= 2);
    }
    /* 24. 涡旋选边不随中轴翻转：自机在 x=+3，一颗弹正对头顶直落 —— 连续 10 帧的横向方向一致 */
    reset(); ap_policy_init(&P);
    W.player.x = 3.0f;
    bullet(3.0f, 384.0f - 120.0f, 0.0f, 4.0f, 6.0f);
    {
        int first = 0, consistent = 1;
        for (int f = 0; f < 10; f++) {
            int h;
            a = ap_policy_decide(&P, &W);
            h = (a & BTN_RIGHT) ? 1 : (a & BTN_LEFT) ? -1 : 0;
            if (f == 0) first = h; else if (h != 0 && h != first) consistent = 0;
            step_world(a);
        }
        assert(first != 0 && consistent);
    }

    /* 25. 鼠标引导：无弹时立刻朝引导点走（不等安静 30 帧）；到了死区就停；有弹压顶时躲弹优先于引导 */
    reset(); ap_policy_init(&P);
    ap_policy_set_guide(&P, 1, 100.0f, 300.0f);
    a = ap_policy_decide(&P, &W);
    assert((a & BTN_RIGHT) && (a & BTN_UP));
    reset(); ap_policy_init(&P);
    ap_policy_set_guide(&P, 1, 3.0f, 386.0f);
    a = ap_policy_decide(&P, &W);
    assert(!(a & (BTN_UP | BTN_DOWN | BTN_LEFT | BTN_RIGHT)));
    reset(); ap_policy_init(&P);
    ap_policy_set_guide(&P, 1, 0.0f, 300.0f);                   /* 引导在正上方 */
    bullet(0.0f, 384.0f - 40.0f, 0.0f, 4.0f, 6.0f);              /* 弹也在正上方、10 帧后到 */
    {
        int hit = 0;
        a = ap_policy_decide(&P, &W);
        assert(a & (BTN_LEFT | BTN_RIGHT));          /* 不是纯朝引导点冲 */
        for (int f = 0; f < 15; f++) {
            step_world(a);
            for (int i = 0; i < W.nbullets; i++) {
                float dx = W.bullets[i].x - W.player.x, dy = W.bullets[i].y - W.player.y;
                if (sqrtf(dx * dx + dy * dy) < W.bullets[i].radius + W.player.hit_radius) hit = 1;
            }
            a = ap_policy_decide(&P, &W);
        }
        assert(!hit);
    }
    ap_policy_set_guide(&P, 0, 0.0f, 0.0f);

    /* 26. 引导死区：光标压在自机 5 px 处不动；从 60 px 外跟过去，40 帧内停在 release 距离内、
     *     最后 10 帧一步不走，且横向方向不翻转 */
    reset(); ap_policy_init(&P);
    ap_policy_set_guide(&P, 1, 5.0f, 384.0f);
    a = ap_policy_decide(&P, &W);
    assert(!(a & (BTN_UP | BTN_DOWN | BTN_LEFT | BTN_RIGHT)));
    reset(); ap_policy_init(&P);
    ap_policy_set_guide(&P, 1, 60.0f, 384.0f);
    {
        int fl = 0, lh = 0, late = 0;
        for (int f = 0; f < 40; f++) {
            int h;
            a = ap_policy_decide(&P, &W);
            h = (a & BTN_RIGHT) ? 1 : (a & BTN_LEFT) ? -1 : 0;
            if (h && lh && h != lh) fl++;
            if (h) lh = h;
            if (f >= 30 && (a & (BTN_UP | BTN_DOWN | BTN_LEFT | BTN_RIGHT))) late = 1;
            step_world(a);
        }
        assert(fl == 0 && !late);
        assert(fabsf(W.player.x - 60.0f) < P.guide_release);
    }
    ap_policy_set_guide(&P, 0, 0.0f, 0.0f);

    puts("test_policy ok");
    return 0;
}
