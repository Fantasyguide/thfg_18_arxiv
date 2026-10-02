/* 天顶剑（id 87）纯逻辑单测：开火节奏 / 挑最近的敌人 / **剑刃的泰拉瑞亚运动参数**
 *（椭圆半轴、相位寿命、透明度、命中冷却、残影节奏）。
 *   cc -std=c11 -Wall -Wextra -Werror -o t tests/test_zenith_core.c cards/zenith_core.c -lm && ./t
 * 公式的一手对照表在 `zenith_core.h` 文件头（Terraria `Projectile.AI_182_FinalFractal`）。 */
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#include "../cards/zenith_core.h"
#include "../cards/broken_core_core.h"     /* bc_atan2f：与 zenith.c / zenith_core.c 同一份 */

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
    zn_aim_consider(&a, 0.0f, 0.0f, ZN_RANGE + 1.0f, 0.0f);
    CHECK(!zn_aim_ready(&a), "射程外的敌人被选中了");
    zn_aim_reset(&a);
    zn_aim_consider(&a, 0.0f, 0.0f, ZN_RANGE - 1.0f, 0.0f);
    CHECK(zn_aim_ready(&a), "射程内的敌人没选中");

    /* ★ owner 2026-10-03：追踪取**最远**的敌人（天顶剑的一般攻击方式，路径上能扫到更多敌人）。
     *   ⚠ 射程外的照样不要（取最远之后，射程不能再靠 best_d2 的初值兜）。*/
    zn_aim_reset(&a);
    zn_aim_consider(&a, 10.0f, 10.0f, 10.0f, -90.0f);            /* 距 100，正上方 */
    zn_aim_consider(&a, 10.0f, 10.0f, 60.0f, 10.0f);             /* 距 50，正右方 */
    zn_aim_consider(&a, 10.0f, 10.0f, 10.0f, 210.0f);            /* 距 200 ⇒ 该选这个 */
    CHECK(zn_aim_ready(&a) && a.dx == 0.0f && a.dy == 200.0f, "该选最远的那个（距 200）");
    zn_aim_consider(&a, 10.0f, 10.0f, 10.0f, 10.0f + ZN_RANGE + 5.0f);   /* 更远但出射程 ⇒ 不该抢 */
    CHECK(a.dy == 200.0f, "射程外的更远目标不该被选中");
}

static void test_launch(void)
{
    zn_state_t s;
    zn_blade_t *b;

    /* 目标 (300,400)：两点距 500 > ZN_ARC_MAX ⇒ 截到 200 ⇒ velocity = 偏移 (60,80)（= 目标方向 × 100）*/
    zn_reset(&s);
    b = zn_slot(&s);
    CHECK(b == &s.b[0], "第一个空槽该是 [0]");
    zn_launch(b, 0x1234u, 7, 0.0f, 0.0f, 300.0f, 400.0f, 50.0f);
    CHECK(b->active == 1 && b->id == 0x1234u && b->prof == 7,
          "发射没生效");
    CHECK(fabsf(b->vx - 60.0f) < 1e-4f && fabsf(b->vy - 80.0f) < 1e-4f,
          "velocity 偏移该是 (60, 80)，得到 (%g, %g)", (double)b->vx, (double)b->vy);
    CHECK(fabsf(b->heading - bc_atan2f(160.0f, 120.0f)) < 1e-5f, "heading 该指向目标");
    CHECK(zn_sign(b) == 1.0f, "向右飞 sign 该是 +1");
    CHECK(fabsf(zn_fast(b) - (700.0f / 900.0f)) < 1e-4f, "fast=%g（|v|·2 = 200）", (double)zn_fast(b));
    CHECK(fabsf(zn_rate(b) - (0.7f + 1.3f * (700.0f / 900.0f))) < 1e-4f, "rate=%g", (double)zn_rate(b));
    CHECK(fabsf(b->ai0 - 50.0f) < 1e-4f, "ai0 该原样存下");
    CHECK(b->phase == 0.0f, "发射该把相位清零");

    /* 向左飞 ⇒ sign = −1；目标是原地 ⇒ |v| = 0 ⇒ fast = 1（最慢的相增 = 0.7）*/
    zn_reset(&s);
    b = zn_slot(&s);
    zn_launch(b, 1u, 0, 0.0f, 0.0f, -80.0f, 60.0f, 0.0f);
    CHECK(zn_sign(b) == -1.0f, "向左飞 sign 该是 −1");
    zn_reset(&s);
    b = zn_slot(&s);
    zn_launch(b, 2u, 0, 5.0f, 6.0f, 5.0f, 6.0f, 0.0f);
    CHECK(b->vx == 0.0f && b->vy == 0.0f && zn_fast(b) == 1.0f, "原地：v = 0、fast = 1");
    /* 原版 `Lerp(0.7, 2, GetLerpValue(900,0,|v|·2,true))`：**|v| 越小 fast 越接近 1 ⇒ 相增越大（寿命越短）** */
    CHECK(fabsf(zn_rate(b) - ZN_SPIN_MAX) < 1e-5f, "原地该用最快的相增 %g，得到 %g", (double)ZN_SPIN_MAX, (double)zn_rate(b));

    /* |v| 很大 ⇒ fast 接近 0 ⇒ 相增接近 2.0（原版：飞得越快转得越快）*/
    zn_reset(&s);
    b = zn_slot(&s);
    zn_launch(b, 3u, 0, 0.0f, 0.0f, ZN_ARC_MAX, 0.0f, 0.0f);   /* |v| = 100、|v|·2 = 200 → fast = 700/900 */
    {
        zn_state_t s2;
        zn_blade_t *b2;
        zn_reset(&s2);
        b2 = zn_slot(&s2);
        /* 极小距离 ⇒ fast 接近 1（慢）——与上一行对照，方向正确即可 */
        zn_launch(b2, 4u, 0, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f);
        CHECK(zn_rate(b2) > zn_rate(b), "越近相增越大（原版：|v| 小 ⇒ GetLerpValue 大 ⇒ Lerp 靠 2.0）");
    }

    /* ai0 超界要夹到 [−100, 100]（原版 ai[0] 只可能是这个范围）*/
    zn_reset(&s);
    b = zn_slot(&s);
    zn_launch(b, 5u, 0, 0.0f, 0.0f, 10.0f, 0.0f, 9999.0f);
    CHECK(b->ai0 == ZN_AI0_MAX, "ai0 该夹到 %g，得到 %g", (double)ZN_AI0_MAX, (double)b->ai0);
    zn_launch(b, 5u, 0, 0.0f, 0.0f, 10.0f, 0.0f, -9999.0f);
    CHECK(b->ai0 == -ZN_AI0_MAX, "ai0 该夹到 −%g", (double)ZN_AI0_MAX);
}

/* ★ owner 2026-10-03：「天顶剑**既有逆时针也有顺时针**」「**得翻转贴图**的，种子弯刀是刀」。 */
static void test_sweep_and_mirror(void)
{
    zn_blade_t bl, br;
    zn_pose_t pl, pr;
    int i;

    /* ai0 < 0 ⇒ **反向扫**（原版那一支用 −2π），且镜像标志与 ai0 ≥ 0 相反 */
    zn_launch(&bl, 1u, 0, 0.0f, 0.0f, 100.0f, 0.0f, 30.0f);     /* ai0 = +30、vx > 0 */
    zn_launch(&br, 2u, 0, 0.0f, 0.0f, 100.0f, 0.0f, -30.0f);    /* ai0 = −30、vx > 0 */
    for (i = 0; i < 11; ++i) { zn_step(&bl, &pl); zn_step(&br, &pr); }
    CHECK(pl.ang > 3.14159265f, "ai0 > 0：该朝 + 方向扫（ang=%g）", (double)pl.ang);
    CHECK(pr.ang < 3.14159265f, "ai0 < 0：该**反向**扫（ang=%g）", (double)pr.ang);
    CHECK(pl.mirror == 0, "ai0 > 0 且 vx > 0 ⇒ spriteDirection = +1，不翻");
    CHECK(pr.mirror == 1, "ai0 < 0 且 vx > 0 ⇒ spriteDirection = −1，要翻");
}

static void test_orbit_params(void)
{
    zn_state_t s;
    zn_blade_t *b;
    zn_pose_t p;
    int i, n_trail = 0, n_hit = 0;
    float prev_ang;
    float vlen;

    /* 出膛点 (0,0) → 目标 (120,160)：d = 200 = ZN_ARC_MAX ⇒ |v| = 100、heading = atan2(160,120) */
    zn_reset(&s);
    b = zn_slot(&s);
    zn_launch(b, 3u, 0, 0.0f, 0.0f, 120.0f, 160.0f, 100.0f);
    vlen = __builtin_sqrtf(b->vx * b->vx + b->vy * b->vy);

    /* 第一帧：L = rate/60、ang = π + 1·L·2π、R = max(60, |v| + 0)·1、A = ai0·Lerp(2,1,fast) */
    zn_step(b, &p);
    CHECK(!p.done, "第一帧不该就结束");
    {
        float L = zn_rate(b) / ZN_PHASE_SWING;
        float want_ang = (float)M_PI + L * 2.0f * (float)M_PI;
        float want_A = 100.0f * (2.0f + (1.0f - 2.0f) * zn_fast(b));
        CHECK(fabsf(p.ang - want_ang) < 1e-4f, "ang=%g 该是 %g", (double)p.ang, (double)want_ang);
        CHECK(p.ang > (float)M_PI, "第一帧该从 π 往正方向转（sign=+1）");
        CHECK(fabsf(p.A - want_A) < 1e-3f, "A=%g 该是 %g", (double)p.A, (double)want_A);
    }
    /* R：|v| = 100 > 60 ⇒ L 小的时候 R = 100（还没加 40）*/
    CHECK(fabsf(p.R - vlen) < 1e-3f || p.R >= ZN_RADIUS_MIN, "R=%g 不该小于 |v|", (double)p.R);
    /* rot = ang + heading − base（原版 rotation 那一行 + **这把剑的贴图基准**）；
     *   `tip` = ang + heading 与贴图无关；`mirror` = 原版 spriteDirection == −1。
     *   ⚠ 单测里 zn_launch 传的 prof = 0/3/5，对应表里第 0/3/5 项（65 / 121 / 368）。*/
    {
        const float bases[21] = {
            -0.7976f, -0.7854f, -0.7941f, -0.7854f, -0.7906f, -0.7941f, -0.7836f, -0.8622f,
            -0.9320f, -0.7854f, -0.7854f, -0.7854f, -0.8325f, -0.8570f, -0.8395f, -0.8779f,
            -0.8308f, -1.0612f, -0.7854f, -0.7854f, -0.7854f
        };
        float base = bases[(b->prof >= 0 && b->prof < 21) ? b->prof : 0];
        CHECK(fabsf(p.rot - (p.ang + b->heading - base)) < 1e-5f, "rot 公式不对");
        CHECK(fabsf(p.tip - (p.ang + b->heading)) < 1e-5f, "tip 该是 ang + heading");
    }
    /* 透明度：前 5 个相位单位淡入 */
    CHECK(p.alpha > 0 && p.alpha < 255, "淡入第一帧 alpha=%d", p.alpha);
    /* fade 起始偏移：k = 1 − GetLerpValue(0,0.5,L)；fade_x = k·sign·−R·0.1、fade_y = k·−ai0·0.3 */
    {
        float L0 = zn_rate(b) / ZN_PHASE_SWING;
        float k0 = 1.0f - (L0 < 0.0f ? 0.0f : (L0 > 0.5f ? 1.0f : L0 / 0.5f));
        CHECK(k0 > 0.9f, "第一帧 k 该接近 1，得到 %g", (double)k0);
        CHECK(fabsf(p.fade_x - (k0 * zn_sign(b) * -p.R * 0.1f)) < 1e-3f, "fade_x=%g", (double)p.fade_x);
        CHECK(fabsf(p.fade_y - (k0 * -100.0f * 0.3f)) < 1e-3f, "fade_y=%g", (double)p.fade_y);
    }

    /* 相位走到 ZN_PHASE_SWING：L 夹到 1、ang 停在 π + 2π、R 长到 |v| + 40、fade 衰减到 0 */
    prev_ang = p.ang;
    for (i = 0; b->phase < ZN_PHASE_SWING && i < 1000; ++i) {
        zn_step(b, &p);
        CHECK(p.ang + 1e-4f >= prev_ang, "ang 该单调增（sign=+1）");
        prev_ang = p.ang;
        n_trail += p.emit_trail;
        n_hit += p.hit;
    }
    CHECK(fabsf(p.ang - ((float)M_PI + 2.0f * (float)M_PI)) < 1e-3f,
          "满相位时 ang 该停在 3π，得到 %g", (double)p.ang);
    CHECK(fabsf(p.R - (vlen + ZN_RADIUS_ADD)) < 1e-3f, "满相位时 R 该是 |v| + 40，得到 %g", (double)p.R);
    CHECK(fabsf(p.fade_x) < 1e-3f && fabsf(p.fade_y) < 1e-3f, "满相位时 fade 该衰减到 0");
    /* 残影 / 命中节奏：走完 60 相位大约 60/rate 帧 */
    CHECK(n_trail >= 4, "60 相位里该给几次残影机会（帧门 ZN_TRAIL_EVERY ⇒ 约 30 次），得到 %d", n_trail);

    /* ★ 2026-10-03 owner：「它是因为在收回的时候 v 小，所以虚影密」⇒ 残影改成**按距离**发。
     *   这里钉住打包/距离判定的语义（世界坐标那一侧的比较在卡里）。*/
    {
        zn_blade_t tb;
        memset(&tb, 0, sizeof tb);
        tb.trail_last = 0xffffffffu;                       /* 还没落点 ⇒ 第一枚必发 */
        CHECK(zn_trail_far_enough(&tb, 0.0f, 0.0f) == 1, "第一枚残影该发");
        tb.trail_last = zn_trail_pack(100.0f, -50.0f);
        CHECK(zn_trail_far_enough(&tb, 100.0f + ZN_TRAIL_SPACING - 1.0f, -50.0f) == 0,
              "没走到 ZN_TRAIL_SPACING 就不该再发（v 小的时候正是这段）");
        CHECK(zn_trail_far_enough(&tb, 100.0f + ZN_TRAIL_SPACING + 1.0f, -50.0f) == 1,
              "走够距离该发");
        /* 定点回读的量化误差 ≤ 0.5 px（1/2 px 定点）*/
        CHECK(fabsf((float)(int16_t)(zn_trail_pack(100.0f, -50.0f) >> 16) / 2.0f - 100.0f) <= 0.5f,
              "打包回读误差该 ≤ 0.5 px");
    }
    CHECK(n_hit >= 5, "60 相位里该命中若干次，得到 %d", n_hit);

    /* 寿命：相位到 ZN_PHASE_END 那一帧 done=1；帧数落在 [120/2.0, 120/0.7] 内 */
    zn_reset(&s);
    b = zn_slot(&s);
    zn_launch(b, 5u, 0, 0.0f, 0.0f, 120.0f, 160.0f, 0.0f);
    for (i = 0; i < 10000; ++i) {
        zn_step(b, &p);
        if (p.done) break;
        CHECK(b->active, "还没到相位上限就断了");
        CHECK(p.alpha > 0, "中途 alpha 掉到 0（第 %d 帧）", i);
    }
    CHECK(p.done == 1 && !b->active, "相位到 %g 该结束", (double)ZN_PHASE_END);
    CHECK(b->phase >= ZN_PHASE_END, "结束时相位该到上限");
    CHECK(i + 1 >= (int)(ZN_PHASE_END / ZN_SPIN_MAX) && i + 1 <= (int)(ZN_PHASE_END / ZN_SPIN_MIN) + 1,
          "寿命 %d 帧超出 [%d, %d]", i + 1, (int)(ZN_PHASE_END / ZN_SPIN_MAX),
          (int)(ZN_PHASE_END / ZN_SPIN_MIN) + 1);
}

static void test_pool(void)
{
    zn_state_t s;
    int i;

    zn_reset(&s);
    CHECK(zn_live(&s) == 0, "空池 live=%d", zn_live(&s));
    for (i = 0; i < ZN_BLADE_MAX; i++) {
        zn_blade_t *b = zn_slot(&s);
        CHECK(b != 0, "第 %d 个该拿得到槽", i);
        zn_launch(b, (uint32_t)(i + 1), 0, 0.0f, 0.0f, 60.0f, 80.0f, 0.0f);
    }
    CHECK(zn_slot(&s) == 0, "池满了还给槽");
    CHECK(zn_live(&s) == ZN_BLADE_MAX, "live=%d", zn_live(&s));
    s.b[3].active = 0;
    CHECK(zn_slot(&s) == &s.b[3], "没复用刚放掉的槽");
    zn_reset(&s);
    CHECK(zn_live(&s) == 0 && zn_slot(&s) == &s.b[0], "reset 没清干净");
}

int main(void)
{
    test_charge();
    test_aim();
    test_launch();
    test_sweep_and_mirror();
    test_orbit_params();
    test_pool();
    printf(fails ? "zenith_core: %d FAILED\n" : "zenith_core: ok\n", fails);
    return fails ? 1 : 0;
}
