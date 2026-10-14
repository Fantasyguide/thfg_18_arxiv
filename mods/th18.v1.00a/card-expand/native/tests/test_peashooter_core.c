/* 豌豆射手（id 77）纯逻辑单测：开火节奏状态机。
 *   cc -std=c11 -Wall -Wextra -Werror -o t tests/test_peashooter_core.c cards/peashooter_core.c && ./t */
#include <stdio.h>
#include <string.h>

#include "../cards/peashooter_core.h"

static int fails;
#define CHECK(cond, ...) do { if (!(cond)) { printf("FAIL %s:%d ", __FILE__, __LINE__); \
    printf(__VA_ARGS__); printf("\n"); fails++; } } while (0)

/* 跑 n 个射击帧，按需喂随机数（rnd 固定），统计。→ 平时颗数 / 开大颗数 / 开大次数 */
static void run(pea_state_t *s, int n, uint32_t rnd, int *normal, int *gat, int *starts, int *ends)
{
    pea_out_t o;
    for (int i = 0; i < n; i++) {
        if (s->pending_wait) pea_set_wait(s, rnd);
        pea_shoot_frame(s, &o);
        if (o.fires) { if (o.set == PEA_SET_GATLING) *gat += o.rows; else *normal += o.rows; }
        CHECK(o.rows == (o.fires ? (o.set == PEA_SET_GATLING ? PEA_GAT_ROWS : 1) : 0),
              "帧 %d：fires=%d rows=%d set=%d", i, o.fires, o.rows, o.set);
        *starts += o.burst_start;
        *ends += o.burst_end;
        pea_frame(s);
    }
}

static void test_wait_range(void)
{
    pea_state_t s;
    for (uint32_t r = 0; r < 4000; r += 7) {
        pea_clear(&s);
        CHECK(s.pending_wait, "pea_clear 之后该要一个随机数");
        pea_set_wait(&s, r);
        CHECK(!s.pending_wait, "喂过随机数之后不该还挂着");
        CHECK(s.wait >= PEA_WAIT_MIN && s.wait < PEA_WAIT_MIN + PEA_WAIT_SPAN,
              "rnd=%u → wait=%u 越界", r, s.wait);
    }
    /* 周期 = 等待 + 开大，落在 20–30 s（1200–1800 帧）*/
    CHECK(PEA_WAIT_MIN + PEA_GAT_FRAMES == 1200, "周期下界 = %d", PEA_WAIT_MIN + PEA_GAT_FRAMES);
    CHECK(PEA_WAIT_MIN + PEA_WAIT_SPAN - 1 + PEA_GAT_FRAMES == 1800,
          "周期上界 = %d", PEA_WAIT_MIN + PEA_WAIT_SPAN - 1 + PEA_GAT_FRAMES);
}

static void test_no_fire_before_seed(void)
{
    pea_state_t s;
    pea_out_t o;
    pea_clear(&s);
    pea_shoot_frame(&s, &o);                       /* 还没喂随机数 */
    CHECK(!o.fires && !o.burst_start && o.rows == 0, "没喂随机数就开火了");
    CHECK(s.peas == 0 && s.timer == 0, "没喂随机数却动了状态");
}

static void test_normal_cadence(void)
{
    pea_state_t s;
    int normal = 0, gat = 0, starts = 0, ends = 0;
    pea_clear(&s);
    pea_set_wait(&s, 0);                            /* wait = PEA_WAIT_MIN，最短的一轮 */
    CHECK(s.wait == PEA_WAIT_MIN, "wait=%u", s.wait);

    /* 第一个射击帧就该有一颗（timer 从 0 起）*/
    pea_out_t o;
    pea_shoot_frame(&s, &o);
    CHECK(o.set == PEA_SET_NORMAL && o.fires && o.rows == 1, "第一帧没打出豌豆");
    pea_frame(&s);
    /* 之后每 PEA_INTERVAL 帧一颗 */
    run(&s, PEA_INTERVAL - 1, 0, &normal, &gat, &starts, &ends);
    CHECK(normal == 0, "第 2..%d 帧不该开火，却打了 %d 颗", PEA_INTERVAL, normal);
    run(&s, 1, 0, &normal, &gat, &starts, &ends);
    CHECK(normal == 1, "第 %d 帧该打第二颗，normal=%d", PEA_INTERVAL + 1, normal);
    CHECK(starts == 0 && gat == 0, "这时不该开大");
}

static void test_gatling_shape(void)
{
    pea_state_t s;
    int normal = 0, gat = 0, starts = 0, ends = 0;
    pea_clear(&s);
    pea_set_wait(&s, 0);
    /* 把等待跑完：wait 每个射击帧 −1，跑满 PEA_WAIT_MIN 帧之后下一帧开大 */
    run(&s, PEA_WAIT_MIN, 0, &normal, &gat, &starts, &ends);
    CHECK(starts == 0, "等待还没走完就开大了");
    CHECK(s.wait == 0, "等待没走完，剩 %u", s.wait);
    int n0 = normal;
    CHECK(n0 == (PEA_WAIT_MIN + PEA_INTERVAL - 1) / PEA_INTERVAL,
          "等待期该打 %d 颗，实际 %d", (PEA_WAIT_MIN + PEA_INTERVAL - 1) / PEA_INTERVAL, n0);

    /* 开大那 PEA_GAT_FRAMES 帧 */
    run(&s, PEA_GAT_FRAMES, 0, &normal, &gat, &starts, &ends);
    CHECK(starts == 1 && ends == 1, "开大 start=%d end=%d", starts, ends);
    CHECK(normal == n0, "开大期间还在打平时的豌豆");
    CHECK(gat == PEA_GAT_FRAMES / PEA_GAT_INTERVAL * PEA_GAT_ROWS,
          "开大该打 %d 颗，实际 %d", PEA_GAT_FRAMES / PEA_GAT_INTERVAL * PEA_GAT_ROWS, gat);
    CHECK(gat * PEA_GAT_DMG == 980, "开大总伤 = %d", gat * PEA_GAT_DMG);
    CHECK(s.pending_wait, "开完大该要一个新的随机等待");
    CHECK(s.bursts == 1 && s.peas == (uint32_t)(normal + gat), "bursts=%u peas=%u", s.bursts, s.peas);

    /* 下一帧回平时形态，且 timer 已清 0 ⇒ 立刻补一颗 */
    pea_out_t o;
    pea_set_wait(&s, 0);
    pea_shoot_frame(&s, &o);
    CHECK(o.set == PEA_SET_NORMAL && o.fires, "开大结束后没回到平时形态");
}

static void test_two_cycles(void)
{
    pea_state_t s;
    int normal = 0, gat = 0, starts = 0, ends = 0;
    pea_clear(&s);
    /* rnd 固定 → 每轮 wait 都是 PEA_WAIT_MIN；两个完整周期 */
    run(&s, 2 * (PEA_WAIT_MIN + PEA_GAT_FRAMES), 0, &normal, &gat, &starts, &ends);
    CHECK(starts == 2 && ends == 2, "两个周期该开大 2 次，得到 start=%d end=%d", starts, ends);
    CHECK(gat == 2 * PEA_GAT_FRAMES / PEA_GAT_INTERVAL * PEA_GAT_ROWS, "开大颗数 %d", gat);
    CHECK(s.bursts == 2, "bursts=%u", s.bursts);
}

static void test_inflight_and_cap(void)
{
    pea_state_t s;
    pea_out_t o;
    pea_clear(&s);
    pea_set_wait(&s, 0);
    CHECK(!pea_needs_cap(&s), "还没开火就要抬上限");
    pea_shoot_frame(&s, &o);                       /* 第一颗 */
    CHECK(o.fires && pea_needs_cap(&s), "开火那帧没武装 inflight");
    for (int i = 0; i < PEA_INFLIGHT; i++) {
        CHECK(pea_needs_cap(&s), "第 %d 帧 inflight 提前掉了", i);
        pea_frame(&s);
    }
    CHECK(!pea_needs_cap(&s), "%d 帧之后 inflight 该归零", PEA_INFLIGHT);
    /* 一颗 90 塞满 Reimu 的 90（自机弹一开火就把它挤掉）、又超出 Sakuya 的 60，所以上限要抬；
     * 开大那排 7 × 7 = 49 本来就装得下 */
    CHECK(PEA_CAP >= PEA_DMG + 90, "PEA_CAP=%d 装不下一颗 %d + 自机弹的份", PEA_CAP, PEA_DMG);
    CHECK(PEA_INTERVAL > PEA_INFLIGHT, "两颗豌豆的 inflight 窗口会重叠（间隔 %d ≤ %d）", PEA_INTERVAL, PEA_INFLIGHT);
    CHECK(PEA_GAT_ROWS * PEA_GAT_DMG < 60, "一排 %d 撞得上 Sakuya 的 60", PEA_GAT_ROWS * PEA_GAT_DMG);
}

static void test_clear(void)
{
    pea_state_t s;
    int normal = 0, gat = 0, starts = 0, ends = 0;
    pea_clear(&s);
    run(&s, PEA_WAIT_MIN + 40, 0, &normal, &gat, &starts, &ends);
    CHECK(starts == 1, "该已经开过一次大");
    pea_clear(&s);
    CHECK(s.timer == 0 && s.wait == 0 && s.gat_left == 0 && s.inflight == 0 &&
          s.bursts == 0 && s.peas == 0 && s.pending_wait == 1, "pea_clear 没清干净");
}


/* ★ P40：电弹连锁的选目标（纯函数 ✓）—— 最近 / 排除去过的 / 半径外 / 空表 四种情形 ✓ */
static void test_chain_pick(void)
{
    static const float xy[6] = { 100.0f, 0.0f,  60.0f, 0.0f,  400.0f, 0.0f };
    static const uint32_t id[3] = { 7u, 8u, 9u };
    uint32_t vis[4];
    int k;

    k = pea_chain_pick(xy, id, 3, 0.0f, 0.0f, 150.0f, 0, 0);
    CHECK(k == 1, "半径内最近的应是 B(60px)，实际 %d", k);

    vis[0] = 8u;
    k = pea_chain_pick(xy, id, 3, 0.0f, 0.0f, 150.0f, vis, 1);
    CHECK(k == 0, "排除已连过的 B 之后应挑 A，实际 %d", k);

    vis[0] = 7u; vis[1] = 8u;
    k = pea_chain_pick(xy, id, 3, 0.0f, 0.0f, 150.0f, vis, 2);
    CHECK(k == -1, "只剩半径外的 C 时应返回 -1，实际 %d", k);

    k = pea_chain_pick(xy, id, 3, 0.0f, 0.0f, 500.0f, 0, 0);
    CHECK(k == 1, "半径放大后仍是最近的 B，实际 %d", k);

    k = pea_chain_pick(xy, id, 0, 0.0f, 0.0f, 150.0f, 0, 0);
    CHECK(k == -1, "没有候敌时应返回 -1，实际 %d", k);

    k = pea_chain_pick(xy, id, 3, 0.0f, 0.0f, 0.0f, 0, 0);
    CHECK(k == -1, "半径 0 应返回 -1，实际 %d", k);
}

int main(void)
{
    test_wait_range();
    test_no_fire_before_seed();
    test_normal_cadence();
    test_gatling_shape();
    test_two_cycles();
    test_inflight_and_cap();
    test_clear();
    test_chain_pick();
    printf(fails ? "peashooter_core: %d FAILED\n" : "peashooter_core: OK\n", fails);
    return fails != 0;
}
