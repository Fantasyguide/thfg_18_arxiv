/* 天顶剑（id 87）纯逻辑单测：开火节奏 / 挑最近的敌人 / 剑刃池的发射与推进。
 *   cc -std=c11 -Wall -Wextra -Werror -o t tests/test_zenith_core.c cards/zenith_core.c -lm && ./t
 * 角度那一段与 libm 对拍（bc_atan2f 本身的误差界在 test_broken_core_core.c 里测过，这里只查用对了没有）。 */
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#include "../cards/zenith_core.h"
#include "../cards/broken_core_core.h"     /* bc_atan2f：角度与 zenith.c 用的是同一个 */

static int fails;
#define CHECK(cond, ...) do { if (!(cond)) { printf("FAIL %s:%d ", __FILE__, __LINE__); \
    printf(__VA_ARGS__); printf("\n"); fails++; } } while (0)

static void test_charge(void)
{
    zn_state_t s;
    int i;

    zn_reset(&s);
    for (i = 1; i < ZN_PERIOD; i++)
        CHECK(!zn_charge(&s), "帧 %d 就蓄满了", i);
    CHECK(zn_charge(&s), "第 %d 帧该蓄满", ZN_PERIOD);
    CHECK(s.frames == (uint32_t)ZN_PERIOD, "frames=%u", s.frames);

    /* 蓄满后没目标 / 池满 ⇒ 一直保持蓄满（敌人一进射程就出手）*/
    for (i = 0; i < 300; i++) CHECK(zn_charge(&s), "蓄满后第 %d 帧掉了", i);
    CHECK(s.fired == 0, "还没开火 fired=%u", s.fired);

    zn_did_fire(&s);
    CHECK(s.fired == 1 && s.charge == 0, "fired=%u charge=%u", s.fired, s.charge);
    CHECK(!zn_charge(&s), "刚开完火不该马上又蓄满");
    for (i = 1; i < ZN_PERIOD; i++) zn_charge(&s);
    CHECK(zn_charge(&s), "第二把该蓄满了");
}

static void test_aim(void)
{
    zn_aim_t a;

    zn_aim_reset(&a);
    CHECK(!zn_aim_ready(&a), "没有候选却给出了目标");

    /* 射程外的不算 */
    zn_aim_reset(&a);
    zn_aim_consider(&a, 0.0f, 0.0f, ZN_RANGE + 1.0f, 0.0f);
    CHECK(!zn_aim_ready(&a), "射程外的敌人被选中了");

    /* 边界：正好射程内 */
    zn_aim_reset(&a);
    zn_aim_consider(&a, 0.0f, 0.0f, ZN_RANGE - 1.0f, 0.0f);
    CHECK(zn_aim_ready(&a), "射程内的敌人没选中");
    CHECK(fabsf(bc_atan2f(a.dy, a.dx)) < 1e-4f, "正右方角度应为 0");

    /* 取最近的那个，与加入顺序无关 */
    zn_aim_reset(&a);
    zn_aim_consider(&a, 10.0f, 10.0f, 10.0f, -90.0f);   /* 距 100，正上方 */
    zn_aim_consider(&a, 10.0f, 10.0f, 60.0f, 10.0f);    /* 距 50，正右方 */
    zn_aim_consider(&a, 10.0f, 10.0f, 10.0f, 210.0f);   /* 距 200 */
    CHECK(zn_aim_ready(&a), "该有目标");
    CHECK(a.dx == 50.0f && a.dy == 0.0f, "该选正右方那个 (%.1f, %.1f)", a.dx, a.dy);
    CHECK(fabsf(bc_atan2f(a.dy, a.dx)) < 1e-4f, "该选正右方那个");
}

static void test_launch_and_advance(void)
{
    zn_state_t s;
    zn_blade_t *b;
    float x0, y0;

    zn_reset(&s);
    b = zn_slot(&s);
    CHECK(b == &s.b[0], "第一个空槽该是 [0]");

    /* 朝正右方（dx=+100, dy=0）：速度 = ZN_SPEED，方向 +x */
    zn_launch(b, 0x1234u, 10.0f, 20.0f, 100.0f, 0.0f);
    CHECK(b->active == 1 && b->id == 0x1234u, "发射没生效");
    CHECK(fabsf(b->vx - ZN_SPEED) < 1e-4f && fabsf(b->vy) < 1e-4f,
          "速度该是 (+%g, 0)，得到 (%g, %g)", (double)ZN_SPEED, (double)b->vx, (double)b->vy);
    CHECK(b->life == ZN_LIFE, "寿命该是 %d，得到 %u", ZN_LIFE, b->life);
    CHECK(b->rot == 0.0f, "初角该是 0");

    /* 斜向：归一化后速度模长仍是 ZN_SPEED（3-4-5 三角形，方向要保对）*/
    zn_reset(&s);
    b = zn_slot(&s);
    zn_launch(b, 1u, 0.0f, 0.0f, 300.0f, 400.0f);
    CHECK(fabsf(b->vx - ZN_SPEED * 0.6f) < 1e-4f && fabsf(b->vy - ZN_SPEED * 0.8f) < 1e-4f,
          "3-4-5 方向错了：(%g, %g)", (double)b->vx, (double)b->vy);

    /* 原地（dx=dy=0）：不产生 NaN，速度 0 */
    zn_reset(&s);
    b = zn_slot(&s);
    zn_launch(b, 2u, 5.0f, 6.0f, 0.0f, 0.0f);
    CHECK(b->vx == 0.0f && b->vy == 0.0f, "零向量该给零速度");

    /* 推进：每帧走一步、寿命每帧减一、自旋单调累加 */
    zn_reset(&s);
    b = zn_slot(&s);
    zn_launch(b, 3u, 0.0f, 0.0f, 1.0f, 0.0f);
    x0 = b->x; y0 = b->y;
    zn_advance(b);
    CHECK(fabsf(b->x - (x0 + ZN_SPEED)) < 1e-4f && b->y == y0, "推进位移不对");
    CHECK(b->life == ZN_LIFE - 1, "寿命该减一，得到 %u", b->life);
    CHECK(fabsf(b->rot - ZN_SPIN) < 1e-4f, "自旋该累加，得到 %g", (double)b->rot);

    /* 寿命尽 ⇒ active 清 0（调用方据此收 VM）。起点放在左边界内，免得先撞出界那条。
     * 一趟 = ZN_LIFE × ZN_SPEED ≈ 598 px，正好与射程 ZN_RANGE 同量级（见 zenith_core.h 的注释）。*/
    zn_reset(&s);
    b = zn_slot(&s);
    zn_launch(b, 4u, ZN_X_MIN + 10.0f, 0.0f, 1.0f, 0.0f);
    for (int i = 0; i < ZN_LIFE; i++) {
        CHECK(b->active, "第 %d 帧就没了", i);
        zn_advance(b);
    }
    CHECK(!b->active, "寿命尽了还活着");

    /* 出界（往正下方飞，y 越过 ZN_Y_MAX）⇒ 早于寿命结束就收 */
    zn_reset(&s);
    b = zn_slot(&s);
    zn_launch(b, 5u, 0.0f, ZN_Y_MAX - 1.0f, 0.0f, 1.0f);
    zn_advance(b);
    CHECK(!b->active, "出界了还活着");
}

static void test_pool(void)
{
    zn_state_t s;
    int i;

    zn_reset(&s);
    CHECK(zn_live(&s) == 0, "空池 live=%d", zn_live(&s));

    /* 池满：第 ZN_BLADE_MAX+1 个拿不到槽 */
    for (i = 0; i < ZN_BLADE_MAX; i++) {
        zn_blade_t *b = zn_slot(&s);
        CHECK(b != 0, "第 %d 个该拿得到槽", i);
        zn_launch(b, (uint32_t)(i + 1), 0.0f, 0.0f, 1.0f, 0.0f);
    }
    CHECK(zn_slot(&s) == 0, "池满了还给槽");
    CHECK(zn_live(&s) == ZN_BLADE_MAX, "live=%d", zn_live(&s));

    /* 放掉一个 ⇒ 槽能复用 */
    s.b[3].active = 0;
    CHECK(zn_live(&s) == ZN_BLADE_MAX - 1, "live=%d", zn_live(&s));
    CHECK(zn_slot(&s) == &s.b[3], "没复用刚放掉的槽");

    /* zn_reset 全清 */
    zn_reset(&s);
    CHECK(zn_live(&s) == 0 && zn_slot(&s) == &s.b[0], "reset 没清干净");
}

int main(void)
{
    test_charge();
    test_aim();
    test_launch_and_advance();
    test_pool();
    printf(fails ? "zenith_core: %d FAILED\n" : "zenith_core: ok\n", fails);
    return fails ? 1 : 0;
}
