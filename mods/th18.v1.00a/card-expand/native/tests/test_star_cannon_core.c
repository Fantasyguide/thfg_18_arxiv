/* 星星炮（id 89）纯逻辑单测：开火节奏（复刻原版白板 useTime = 12）/ 瞄准 / 星星弹幕的
 * 运动（定速直线 + 自转 + 闪烁 + 出界）与命中免疫计时（复刻默认 localNPCHitCooldown = 10）。
 *   cc -std=c11 -Wall -Wextra -Werror -o t tests/test_star_cannon_core.c cards/star_cannon_core.c && ./t
 * 一手数值与出处见 `cards/star_cannon_core.h` 的文件头。 */
#include <assert.h>
#include <math.h>
#include <stdio.h>

#include "../cards/star_cannon_core.h"

static int fails;
#define CHECK(cond, ...) do { if (!(cond)) { printf("FAIL %s:%d ", __FILE__, __LINE__); \
    printf(__VA_ARGS__); printf("\n"); fails++; } } while (0)

static void test_charge(void)
{
    sc_state_t s;
    int i;

    sc_reset(&s);
    for (i = 1; i < SC_PERIOD; ++i)
        CHECK(!sc_charge(&s), "第 %d 帧就开火了（原版 useTime = %d）", i, SC_PERIOD);
    CHECK(sc_charge(&s), "第 %d 帧该开火（白板 5 发/秒）", SC_PERIOD);
    sc_did_fire(&s);
    CHECK(s.fired == 1 && s.charge == 0, "fired=%u charge=%u", s.fired, s.charge);
    /* 开火后要**重新蓄**（原版 autoReuse 的节奏）：第 1..11 帧不该开火、第 12 帧开火 */
    for (i = 1; i < SC_PERIOD; ++i) CHECK(!sc_charge(&s), "重蓄时第 %d 帧就开火了", i);
    CHECK(sc_charge(&s), "重蓄满之后该开火");
}

static void test_aim(void)
{
    sc_aim_t a;

    sc_aim_reset(&a);
    CHECK(!sc_aim_ready(&a), "没有候选却给出了目标");
    sc_aim_consider(&a, 0.0f, 0.0f, SC_RANGE + 1.0f, 0.0f);
    CHECK(!sc_aim_ready(&a), "射程外的敌人被选中了");
    sc_aim_reset(&a);
    sc_aim_consider(&a, 0.0f, 0.0f, 0.0f, -(SC_RANGE - 1.0f));
    CHECK(sc_aim_ready(&a), "正上方的敌人没选中");

    sc_aim_reset(&a);
    sc_aim_consider(&a, 0.0f, 0.0f, 10.0f, 10.0f);      /* 距 ~14 */
    sc_aim_consider(&a, 0.0f, 0.0f, 100.0f, 0.0f);      /* 距 100 */
    CHECK(sc_aim_ready(&a) && fabsf(a.dx - 10.0f) < 1e-6f && fabsf(a.dy - 10.0f) < 1e-6f,
          "该选最近的那个");
}

static void test_launch_and_step(void)
{
    sc_state_t s;
    sc_shot_t *sh;
    float x0, y0;
    int i;

    sc_reset(&s);
    sh = sc_slot(&s);
    CHECK(sh != 0, "空池该给槽");
    sc_launch(sh, 0x1234u, 5.0f, 6.0f, 0.0f, -SC_SPEED);   /* 朝正上，速度 = shootSpeed */
    CHECK(sh->active == 1 && sh->id == 0x1234u, "发射没生效");
    CHECK(sh->x == 5.0f && sh->y == 6.0f, "出膛位置该原样");
    CHECK(sh->hit == 1, "出生那一帧就该能命中");
    CHECK(sc_draw_alpha(sh) == 255, "出生时不透明度该是满的（原版 alpha 从 0 起）");

    x0 = sh->x; y0 = sh->y;
    CHECK(!sc_step(sh), "第一帧不该出界");
    CHECK(sh->y == y0 - SC_SPEED, "位移该是**定速直线**（原版 aiStyle 5 不加速、不重力）：%g",
          (double)(y0 - sh->y));
    CHECK(sh->x == x0, "朝正上时 x 不该动");
    /* 自转方向 = sign(velocity.X)（原版那个 `direction`）：vx == 0 时按 +1 处理（一手是"保持上一帧"）*/
    /* 自转按**速度**算：`(|vx|+|vy|)·0.01`（原版公式）⇒ 弹速 7 时 = 0.07 */
    CHECK(fabsf(sh->rot - (fabsf(sh->vx) + fabsf(sh->vy)) * 0.01f) < 1e-6f,
          "vx == 0 ⇒ direction = +1 ⇒ rot = %g，得到 %g",
          (double)((fabsf(sh->vx) + fabsf(sh->vy)) * 0.01f), (double)sh->rot);

    /* 自转与闪烁都按**水平飞行**测（竖直飞 6 帧就出界了；这里每帧把位置拉回原地，只看时间维）*/
    sc_reset(&s);
    sh = sc_slot(&s);
    sc_launch(sh, 0x99u, 0.0f, 0.0f, -SC_SPEED, 0.0f);     /* 朝左 ⇒ direction = −1 */
    for (i = 0; i < 10; ++i) { sh->x = 0.0f; sh->y = 0.0f; sc_step(sh); }
    CHECK(fabsf(sh->rot + (fabsf(sh->vx) + fabsf(sh->vy)) * 0.01f * 10.0f) < 1e-4f,
          "朝左该往负向自转：rot = %g", (double)sh->rot);

    /* 闪烁：alpha 在 0..SC_ALPHA_MAX 之间来回（原版 `alpha += 25·localAI[0]`）*/
    {
        int i, lo = 9999, hi = -1;
        for (i = 0; i < 200; ++i) {
            sh->x = 0.0f; sh->y = 0.0f;                     /* 拉回原地，专测闪烁 */
            sc_step(sh);
            if (sh->alpha < lo) lo = sh->alpha;
            if (sh->alpha > hi) hi = sh->alpha;
        }
        CHECK(lo >= 0 && hi <= SC_ALPHA_MAX, "alpha 越界 [%d, %d]", lo, hi);
        CHECK(hi == SC_ALPHA_MAX && lo <= SC_ALPHA_STEP, "alpha 该走到两端（实测 [%d, %d]）", lo, hi);
        CHECK(sc_draw_alpha(sh) == 255 - sh->alpha / SC_ALPHA_DRAW_DIV,
              "画出来的 alpha 该是 255 − 原版 alpha/SC_ALPHA_DRAW_DIV（覆盖混合下压摆动幅度）");
    }

    /* 命中免疫：每 SC_HIT_COOLDOWN 帧正好一次 */
    {
        int i, hits = 0;
        sc_reset(&s);
        sh = sc_slot(&s);
        sc_launch(sh, 1u, 0.0f, 0.0f, SC_SPEED, 0.0f);
        for (i = 0; i < SC_HIT_COOLDOWN * 6; ++i) {
            sh->x = 0.0f; sh->y = 0.0f;                     /* 同上：只看免疫计时 */
            if (sc_step(sh)) break;
            if (sh->hit) ++hits;
        }
        CHECK(hits >= 5 && hits <= 7, "%d 帧里该命中 5..7 次，得到 %d", SC_HIT_COOLDOWN * 6, hits);
    }

    /* ★ 2026-10-03 owner 口径：「星星炮是**无限穿透**的」+「务必**不要做成帧伤**」——
     *   这两条一起钉：① 命中**不消耗**（星不消失、照飞）；② 伤害源只每 `SC_HIT_COOLDOWN` 帧出一次
     *   （= 原版 `localNPCHitCooldown = 10`），所以是"每 10 帧 1 次"而不是每帧。
     *   一手：`enemy+0x80` = 命中间隔（`engine/player/th18/02-damage-sources.md` 的字段表）。*/
    {
        int i, hits = 0;
        const int N = SC_HIT_COOLDOWN * 5;
        sc_reset(&s);
        sh = sc_slot(&s);
        sc_launch(sh, 1u, -200.0f, 0.0f, SC_SPEED, 0.0f);
        for (i = 0; i < N; ++i) {
            sh->x = 0.0f; sh->y = 0.0f;                     /* 钉在"敌人身上"，只数伤害与存亡 */
            if (sc_step(sh)) break;
            CHECK(sh->active == 1, "第 %d 帧：命中不该让星星消失（原版 penetrate = -1）", i);
            if (sh->hit) ++hits;
        }
        CHECK(hits == 5, "%d 帧里该正好 5 次伤害（每 10 帧一次、**不是帧伤**），得到 %d", N, hits);
    }

    /* 出界即收（原版撞方块 / timeLeft 用尽）*/
    sc_reset(&s);
    sh = sc_slot(&s);
    sc_launch(sh, 2u, 0.0f, 0.0f, 0.0f, -SC_SPEED);
    {
        int i, done = 0;
        /* 出膛 y = 0、往上 14 px/帧、上沿 −80 ⇒ 约 6 帧出界（原版撞方块 / timeLeft 用尽那一条）*/
        for (i = 0; i < 1000; ++i) if (sc_step(sh)) { done = 1; CHECK(i < 60, "出界太慢：第 %d 帧", i); break; }
        CHECK(done, "往上飞了一千帧还没出界");
        CHECK(!sh->active, "出界后该自己关掉 active");
    }
}

static void test_pool(void)
{
    sc_state_t s;
    int i;

    sc_reset(&s);
    CHECK(sc_live(&s) == 0, "空池 live=%d", sc_live(&s));
    for (i = 0; i < SC_SHOT_MAX; ++i) {
        sc_shot_t *sh = sc_slot(&s);
        CHECK(sh != 0, "第 %d 个该拿得到槽", i);
        sc_launch(sh, (uint32_t)(i + 1), 0.0f, 0.0f, 0.0f, -SC_SPEED);
    }
    CHECK(sc_slot(&s) == 0, "池满了还给槽");
    CHECK(sc_live(&s) == SC_SHOT_MAX, "live=%d", sc_live(&s));
    CHECK(sizeof(sc_state_t) <= 496 - 16, "sc_state_t 太大了：%u 字节", (unsigned)sizeof(sc_state_t));
    s.s[3].active = 0;
    CHECK(sc_slot(&s) == &s.s[3], "没复用刚放掉的槽");
}

int main(void)
{
    test_charge();
    test_aim();
    test_launch_and_step();
    test_pool();
    printf(fails ? "star_cannon_core: %d FAILED\n" : "star_cannon_core: ok\n", fails);
    return fails ? 1 : 0;
}
