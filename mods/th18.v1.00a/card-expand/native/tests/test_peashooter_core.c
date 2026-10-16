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
        /* ★★ 2026-10-15：爆发的**结束**现在由 `pea_burst_frame()`（**每帧** ✓）报 ⇒ 这里照卡侧
         *   的真实顺序同步驱动（`on_tick_2`：`pea_burst_frame` → 无敌声明 → 射击回调 ✓）。
         *   旧写法读 `o.burst_end` ✗ —— 那个字段已不再被设置（见 `peashooter_core.c` 的注释 ✓）。*/
        *ends += pea_burst_frame(s);
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
    /* 周期 = 等待 + 开大；P54（owner：爆发拉长）把开大从 100 帧加到 240 帧 ⇒ 22–32 s（1340–1940 帧）*/
    CHECK(PEA_WAIT_MIN + PEA_GAT_FRAMES == 1340, "周期下界 = %d", PEA_WAIT_MIN + PEA_GAT_FRAMES);
    CHECK(PEA_WAIT_MIN + PEA_WAIT_SPAN - 1 + PEA_GAT_FRAMES == 1940,
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
    CHECK(gat * PEA_GAT_DMG == 2400, "开大总伤 = %d（240 颗 × 10 ✓）", gat * PEA_GAT_DMG);
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

/* ★★★ 2026-10-15（owner 报「电大哥的**无敌一直存在**」✗ 的回归测试 ✓）：
 *   症状 = **松手**（此后不再有射击帧 ✗）之后爆发还挂着 ⇒ 卡侧 `on_tick_2` 里那句
 *   `ce_invuln_request(id, PS_BURST_INVULN)` 每帧照发 ⇒ 无敌一直存在 ✗。
 *   病根 = 爆发倒计时原来只在 `pea_shoot_frame()` 里减 ✗，而它只在按住射击键时被调。
 *   本测试**一枪都不打**、只按帧跑 ⇒ 爆发必须在 PEA_GAT_FRAMES 帧内走完 ✓。*/
static void test_burst_ends_without_firing(void)
{
    pea_state_t s;
    pea_out_t o;
    int ends = 0;
    pea_clear(&s);
    pea_set_wait(&s, 0);
    s.wait = 0;                                  /* 攒够了 ⇒ 下一帧开大 */
    pea_shoot_frame(&s, &o);                     /* 这一帧开大 */
    CHECK(o.burst_start == 1 && pea_burst_left(&s) > 0, "没开大");
    CHECK(s.gat_left == (uint32_t)PEA_GAT_FRAMES, "开大那一帧不该顺手倒计时：gat_left=%u", s.gat_left);
    /* 之后**只按帧**（不调 `pea_shoot_frame` ✗ = 松手 ✓）*/
    for (int i = 0; i < PEA_GAT_FRAMES; ++i) {
        ends += pea_burst_frame(&s);
        if (i < PEA_GAT_FRAMES - 1)
            CHECK(pea_burst_left(&s) > 0, "第 %d 帧就提前结束了", i + 1);
    }
    CHECK(ends == 1, "松手后爆发该正好结束 1 次，实际 %d", ends);
    CHECK(pea_burst_left(&s) == 0, "松手后爆发还没结束 ⇒ 无敌会一直挂着 ✗");
    CHECK(s.pending_wait, "爆发结束后该要一个新的随机等待");
}

/* ★★★ 2026-10-15（owner 报「**电大哥的弹幕把我主机的弹幕也弄成全方向随机的了**」✗ 的回归 ✓）：
 *   判据 = 弹上 packed id（`bullet+0xac` ✓）的**高 8 位 = shooterset 组号** ✓，要 ∈ 我们那两组
 *   （`native/sht_ids.h` 生成物：普攻 **0x17**、机枪 **0x18** ✓）。本测试钳三件事：
 *     ① 我们两组（**任意组内行号** 0..255 ✓）都判"是" ✓；
 *     ② **原生**主机的弹（其它组号 ✓，含 0x00 原生普攻 / 0x0a–0x15 原生装备卡子机组 /
 *        0x16 = 108 那组 ✓）一律判"不是" ✗ —— 这正是那个 bug 的边界 ✓；
 *     ③ 紧邻的两组（0x16 / 0x19 ✓）也判"不是" ✗。*/
static void test_bullet_is_ours(void)
{
    const uint32_t N = 0x17u, G = 0x18u;
    CHECK(pea_bullet_is_ours((N << 8) | 0u,   N, G) == 1, "普攻组第 0 行没认出来");
    CHECK(pea_bullet_is_ours((N << 8) | 7u,   N, G) == 1, "普攻组第 7 行没认出来");
    CHECK(pea_bullet_is_ours((G << 8) | 0u,   N, G) == 1, "机枪组第 0 行没认出来");
    CHECK(pea_bullet_is_ours((G << 8) | 255u, N, G) == 1, "机枪组第 255 行没认出来");
    for (uint32_t set = 0; set <= 0x2fu; ++set) {
        if (set == N || set == G) continue;
        CHECK(pea_bullet_is_ours((set << 8) | 3u, N, G) == 0,
              "★ 组 %#x 的弹被误认成我们的（主机弹会被改朝向 ✗）", set);
    }
    CHECK(pea_bullet_is_ours((0x16u << 8) | 0u, N, G) == 0, "0x16（108 那组）被误认");
    CHECK(pea_bullet_is_ours((0x19u << 8) | 0u, N, G) == 0, "0x19（116 季节那组）被误认");
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


/* ★ P42：电弧按敌人对去重（纯函数 ✓）—— key 无序 / 找同对 / 复用死槽 / 表满不新建 ✓ */
static void test_arc_dedupe(void)
{
    uint32_t keys[3] = { 0, 0, 0 };
    uint32_t vms[3] = { 0, 0, 0 };
    uint8_t  alive[3] = { 0, 0, 0 };
    uint32_t k1 = pea_arc_key(7u, 8u), k2 = pea_arc_key(8u, 7u);
    int i;

    CHECK(k1 == k2, "无序对：key(7,8) 应等于 key(8,7)（%u vs %u）", k1, k2);
    CHECK(pea_arc_find(keys, 3, k1) == -1, "空表里不该找到 ✓");

    i = pea_arc_put(keys, vms, alive, 3, k1, 111u);
    CHECK(i == 0 && vms[0] == 111u, "第一次应放进 0 号槽（i=%d vm=%u）", i, vms[0]);
    i = pea_arc_put(keys, vms, alive, 3, k2, 222u);
    CHECK(i == 0 && vms[0] == 222u, "★ 同一对（反向写）应复用 0 号槽、只换 VM（i=%d vm=%u）", i, vms[0]);
    CHECK(pea_arc_find(keys, 3, k1) == 0, "同对应能找到 0 号槽 ✓");

    alive[0] = 1;                                    /* 0 号还活着 ⇒ 新对放 1 号 ✓ */
    i = pea_arc_put(keys, vms, alive, 3, pea_arc_key(1u, 2u), 333u);
    CHECK(i == 1, "0 号活着 ⇒ 新对应放 1 号（i=%d）", i);
    alive[0] = 0;                                    /* 0 号死了 ⇒ 新对复用 0 号 ✓ */
    i = pea_arc_put(keys, vms, alive, 3, pea_arc_key(3u, 4u), 444u);
    CHECK(i == 0 && vms[0] == 444u, "0 号死后应被复用（i=%d vm=%u）", i, vms[0]);

    alive[0] = alive[1] = alive[2] = 1;              /* 全活 + 无同对 ⇒ 不新建 ✓ */
    i = pea_arc_put(keys, vms, alive, 3, pea_arc_key(9u, 9u), 555u);
    CHECK(i == -1, "表满且无同对 ⇒ 必须返回 -1（宁可不建 ✓），实际 %d", i);
    i = pea_arc_put(keys, vms, alive, 3, pea_arc_key(3u, 4u), 666u);
    CHECK(i == 0, "表满但**有同对** ⇒ 仍要复用（i=%d）", i);
}


/* ★ P43：卡侧电弹（纯逻辑 ✓）—— 两组数值 / 每 10 帧才结算 / 出界判定 / 满了不炸 ✓ */
static void test_proj(void)
{
    pe_proj_t a[3];
    int i;
    pe_proj_clear_all(a, 3);

    i = pe_proj_new(a, 3, 0.0f, 400.0f, PE_SET_NORMAL);
    CHECK(i == 0 && a[0].alive == 1, "起一颗正常弹应在 0 号（i=%d）", i);
    CHECK(a[0].dmg == PE_DMG_NORMAL && a[0].vy == -(PE_SPD_NORMAL / PE_ELEC_SLOW),
          "★ P47 普攻电球：伤害照 PEA_NORMAL、速度是它的一半（dmg=%d vy=%.1f 应为 %.1f）",
          a[0].dmg, a[0].vy, -(PE_SPD_NORMAL / PE_ELEC_SLOW));

    i = pe_proj_new(a, 3, 16.0f, 400.0f, PE_SET_GATLING);
    CHECK(i == 1 && a[1].dmg == PE_DMG_GATLING && a[1].vy == -(PE_SPD_GATLING / PE_ELEC_SLOW),
          "★ P47 机枪电弹同样半速（dmg=%d vy=%.1f 应为 %.1f）",
          a[1].dmg, a[1].vy, -(PE_SPD_GATLING / PE_ELEC_SLOW));

    CHECK(pe_proj_hit_now(&a[0]) == 1, "刚出生就该能结算 ✓");
    pe_proj_fired(&a[0]);
    CHECK(pe_proj_hit_now(&a[0]) == 0, "★ 出过源之后应等冷却（不许帧伤 ✓）");
    for (i = 0; i < PE_HIT_COOLDOWN; ++i) pe_proj_move(&a[0]);
    CHECK(pe_proj_hit_now(&a[0]) == 1, "★ 过了 %d 帧应能再结算一次（= 对同一敌每 10 帧 1 跳 ✓）", PE_HIT_COOLDOWN);

    a[0].x = 0.0f; a[0].y = 400.0f;
    CHECK(pe_proj_out(&a[0]) == 0, "画面内不该判出界 ✓");
    a[0].y = PE_VIEW_Y_MIN - 1.0f;
    CHECK(pe_proj_out(&a[0]) == 1, "越过上界就该收 ✓");

    CHECK(pe_proj_new(a, 1, 0.0f, 0.0f, PE_SET_NORMAL) == -1, "池满应返回 -1（不炸 ✓）");

    /* ★ P44：扇形弹幕 = 显式方向 ✓（x 也要动 ✓）*/
    pe_proj_clear_all(a, 3);
    i = pe_proj_spawn(a, 3, 100.0f, 400.0f, 7.0f, -18.0f, PE_SET_GATLING, PE_DMG_GATLING);
    CHECK(i == 0 && a[0].vx == 7.0f && a[0].vy == -18.0f, "显式方向应写进 vx/vy（vx=%.1f vy=%.1f）", a[0].vx, a[0].vy);
    pe_proj_move(&a[0]);
    CHECK(a[0].x == 107.0f && a[0].y == 382.0f, "一帧后应同时按 vx/vy 走（x=%.1f y=%.1f）", a[0].x, a[0].y);
    pe_proj_clear_all(a, 3);
    CHECK(pe_proj_hit_now(&a[0]) == 0 && pe_proj_out(&a[0]) == 0, "清空之后都不该有反应 ✓");
}

int main(void)
{
    test_wait_range();
    test_no_fire_before_seed();
    test_normal_cadence();
    test_gatling_shape();
    test_two_cycles();
    test_bullet_is_ours();                           /* ★ 2026-10-15：主机弹绝不能被误认 ✓ */
    test_burst_ends_without_firing();                /* ★ 2026-10-15：松手后爆发/无敌必须自己走完 ✓ */
    test_inflight_and_cap();
    test_clear();
    test_chain_pick();
    test_arc_dedupe();
    test_proj();
    printf(fails ? "peashooter_core: %d FAILED\n" : "peashooter_core: OK\n", fails);
    return fails != 0;
}
