/* 策略的判别式测试：几何取值要能区分对错（圆心重合式测试是瞎的）。 */
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "../policy.h"   /* 与现行接口相同；单独编译见 README */
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
static void bullet(float x, float y, float vx, float vy, float r)
{
    ap_bullet_t *b = &W.bullets[W.nbullets++];
    b->x = x; b->y = y; b->vx = vx; b->vy = vy; b->radius = r; b->state = 1;
}

int main(void)
{
    ap_policy_t P;
    uint32_t a;
    float t;

    /* 1. 没弹：射击，不炸，不乱动（允许朝 home_y 微调，但不该按左右） */
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

    /* 3. 评分函数本身：站着不动的净距应为负（会被打中），向右走 4/帧 应为正 */
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

    /* 7. bomb 冷却：刚炸过（last_bomb_frame = 当前帧-1）就不再按 */
    W.player.bombs = 2; ap_policy_init(&P); P.last_bomb_frame = W.frame - 1;
    a = ap_policy_decide(&P, &W);
    assert(!(a & BTN_BOMB));

    /* 8. 出生/死亡态：什么都不按（连射击也不按，把控制权还给游戏） */
    reset(); ap_policy_init(&P); W.player.state = 1;
    assert(ap_policy_decide(&P, &W) == 0);

    /* 9. 敌人本体也是障碍：一个敌人贴在头顶正上方 30px、半宽半高 16，不动 —— 向上走必须被判危险 */
    reset(); ap_policy_init(&P);
    W.enemies[W.nenemies].x = 0.0f; W.enemies[W.nenemies].y = 384.0f - 30.0f;
    W.enemies[W.nenemies].hit_w = 16.0f; W.enemies[W.nenemies].hit_h = 16.0f;
    W.enemies[W.nenemies].collidable = 1; W.nenemies++;
    assert(ap_policy_score_move(&W, 0.0f, -4.0f, 24, &t) < 0.0f);
    assert(ap_policy_score_move(&W, 0.0f, 0.0f, 24, &t) > 0.0f);

    /* 10. 同一个敌人但引擎不拿它撞自机（collidable=0）：向上走不该被判危险 */
    W.enemies[0].collidable = 0;
    assert(ap_policy_score_move(&W, 0.0f, -4.0f, 24, &t) > 0.0f);

    puts("test_policy ok");
    return 0;
}
