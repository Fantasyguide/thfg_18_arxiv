/* test_pofv_core.c —— pofv_core.c 的主机单测（`make test-host` 的第 20 条，跑法见 native/Makefile：
 *   cc -std=c11 -Wall -Wextra -Werror -o .../test_pofv_core tests/test_pofv_core.c cards/pofv_core.c
 * 语义来源（一手）：
 *   · 白弹阈值 T = 4.0：spawn spec review §5.4（`zBullet+0x658`；弹种表 `0x4c5f90` stride `0x118`）
 *   · 气槽 4 级 / CA 门槛与消耗（C1 0、C2 1、C3 2、C4 3）/ C1 无敌 8 帧 / QCA 清空：
 *     `mods/th18.v1.00a/card-expand/IDEAS.md` §「109 花映塚」
 *   · SP 三档 10 万 / 30 万 / 50 万：同上；活化灵自爆 3 颗小玉、HP 减半、只有连爆打得动普通幽灵：同上
 *   · 红圈消掉的白弹**不回复气**：同上（「红色消弹圈」段）
 *   · 爆风队列 / 每帧预算：shared-enemy-death spec §4.1（死亡回调里不许扫弹池）
 * 设计文档：`docs/superpowers/specs/2026-09-12-pofv-109-design.md`
 * 覆盖：白弹筛选 / 蓄力状态机与拦 Z 条件 / 气槽 4 级与消耗 / SP 阈值 /
 *       爆风与红圈的半径与计数调用点 / 爆风队列与帧预算 / 活化灵状态机。 */
#include <stdio.h>
#include <string.h>

#include "../cards/pofv_core.h"

static int s_fail, s_pass;
#define CHECK(cond) do { if (cond) ++s_pass; else { ++s_fail; printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } } while (0)

int main(void)
{
    pofv_state_t st;
    float r = 0.0f;
    int lvl = 0;
    int i;

    /* ==================== A. 白弹半径阈值筛选（一手 T = 4.0）==================== */
    CHECK(POFV_WHITE_R_MAX == 4.0f);
    /* 一手半径表（review §5.4）：2.0 / 2.4 / 2.8 / 3.2 / 4.0 全是白弹（可消）*/
    CHECK(pofv_classify_bullet_radius(2.0f) == POFV_BULLET_WHITE);
    CHECK(pofv_classify_bullet_radius(2.4f) == POFV_BULLET_WHITE);
    CHECK(pofv_classify_bullet_radius(2.8f) == POFV_BULLET_WHITE);
    CHECK(pofv_classify_bullet_radius(3.2f) == POFV_BULLET_WHITE);
    CHECK(pofv_classify_bullet_radius(4.0f) == POFV_BULLET_WHITE);        /* ★ 边界含 4.0 */
    /* 一手：30×30 起半径 ≥ 5.0 = 不可消（蓝圈消不掉、只有红圈能消）*/
    CHECK(pofv_classify_bullet_radius(5.0f) == POFV_BULLET_UNBREAKABLE);
    CHECK(pofv_classify_bullet_radius(6.0f) == POFV_BULLET_UNBREAKABLE);
    CHECK(pofv_classify_bullet_radius(8.0f) == POFV_BULLET_UNBREAKABLE);
    CHECK(pofv_classify_bullet_radius(4.0001f) == POFV_BULLET_UNBREAKABLE);
    /* 半径 0.0 的 7 个哨兵弹种（51 / 53 / 55–59）：既不是白弹也不是不可消弹 */
    CHECK(pofv_classify_bullet_radius(0.0f) == POFV_BULLET_SENTINEL);
    CHECK(pofv_classify_bullet_radius(-1.0f) == POFV_BULLET_SENTINEL);
    CHECK(pofv_is_white_bullet(4.0f) == 1);
    CHECK(pofv_is_white_bullet(0.0f) == 0);
    CHECK(pofv_is_white_bullet(5.0f) == 0);
    {
        /* 自扫池的计数判据（蓝圈调用点）：混合数组里数出的白弹数 */
        const float radii[8] = { 2.0f, 0.0f, 5.0f, 4.0f, 2.4f, 0.0f, 6.0f, 3.2f };
        CHECK(pofv_scan_count_white(radii, 8) == 4);
        CHECK(pofv_scan_count_white(radii, 0) == 0);
        CHECK(pofv_scan_count_white(NULL, 8) == 0);
    }

    /* ==================== B. 蓄力状态机与拦 Z 条件 ==================== */
    pofv_state_init(&st);
    CHECK(st.gauge == 0 && st.phase == POFV_CHARGE_IDLE && st.level == 0);
    CHECK(pofv_charge_max_level(0) == 0);
    CHECK(pofv_charge_max_level(2) == 2);
    CHECK(pofv_charge_max_level(9) == POFV_GAUGE_MAX);      /* 钳到 4 级 */

    /* 没气：蓄不动 ⇒ 不拦 Z（原生照发）*/
    CHECK(pofv_charge_begin(&st) == 1);
    for (i = 0; i < POFV_CHARGE_FRAMES_PER_LEVEL * 2; ++i) pofv_charge_hold(&st);
    CHECK(st.level == 0);
    CHECK(pofv_charge_suppress(&st) == 0);
    CHECK(pofv_charge_release(&st, &lvl) == 0);
    CHECK(lvl == 0 && st.gauge == 0);

    /* 有 4 格气：按住 → 升级 → 拦 Z；松开 → 发动 CA */
    st.gauge = POFV_GAUGE_MAX;
    CHECK(pofv_charge_begin(&st) == 1);
    CHECK(pofv_charge_begin(&st) == 0);                     /* 已在按住态：重复无副作用 */
    CHECK(st.max_level == POFV_GAUGE_MAX);                  /* b = min(气槽, 4) */
    for (i = 1; i < POFV_CHARGE_FRAMES_PER_LEVEL; ++i) {
        CHECK(pofv_charge_hold(&st) == 0);                  /* 还没升到 C1：这段就是「点射窗口」*/
        CHECK(pofv_charge_suppress(&st) == 0);
    }
    CHECK(pofv_charge_hold(&st) == 1);                      /* ★ 升到 C1：卡侧播「叮」*/
    CHECK(st.level == 1);
    CHECK(pofv_charge_suppress(&st) == 1);                  /* ★ 拦 Z 条件成立 */
    for (i = 0; i < POFV_CHARGE_FRAMES_PER_LEVEL; ++i) {
        pofv_charge_hold(&st);
        CHECK(pofv_charge_suppress(&st) == 1);
    }
    CHECK(st.level == 2 && st.max_level == 4);
    CHECK(pofv_charge_release(&st, &lvl) == 1);
    CHECK(lvl == 2 && st.gauge == POFV_GAUGE_MAX - 1);      /* C2 消耗 1 格 */
    CHECK(st.phase == POFV_CHARGE_IDLE && st.level == 0);

    /* 上限 = 气槽：只有 2 格气就只能蓄到 C2 */
    st.gauge = 2;
    CHECK(pofv_charge_begin(&st) == 1);
    for (i = 0; i < POFV_CHARGE_FRAMES_PER_LEVEL * 5; ++i) pofv_charge_hold(&st);
    CHECK(st.level == 2 && st.max_level == 2);

    /* C1：1 格气、**不消耗**（一手）*/
    CHECK(pofv_charge_release(&st, &lvl) == 1);
    CHECK(lvl == 2 && st.gauge == 1);
    CHECK(pofv_charge_begin(&st) == 1);
    for (i = 0; i < POFV_CHARGE_FRAMES_PER_LEVEL; ++i) pofv_charge_hold(&st);
    CHECK(st.level == 1);
    CHECK(pofv_charge_release(&st, &lvl) == 1);
    CHECK(lvl == 1 && st.gauge == 1);                       /* C1 消耗 0 格 ⇒ 气槽不动 */

    /* 点射：没蓄到 C1 就松手 ⇒ **什么都不发动**（原生那一发照走）*/
    st.gauge = 4;
    CHECK(pofv_charge_begin(&st) == 1);
    CHECK(pofv_charge_hold(&st) == 0);
    CHECK(pofv_charge_release(&st, &lvl) == 0);
    CHECK(lvl == 0 && st.gauge == 4);

    /* 打断（对话 / 暂停 / 死亡）：不扣气、不发动 */
    CHECK(pofv_charge_begin(&st) == 1);
    for (i = 0; i < POFV_CHARGE_FRAMES_PER_LEVEL; ++i) pofv_charge_hold(&st);
    CHECK(st.level == 1);
    CHECK(pofv_charge_cancel(&st) == 1);
    CHECK(st.phase == POFV_CHARGE_IDLE && st.level == 0 && st.gauge == 4);

    /* ★ QCA 已**放弃**（用户 2026-09-12）：API 已从 core 删除 ⇒ 这里只能记一条「不存在」的事实：
     *   109 的 B 侧只剩「长按 Z 蓄力」，X 键完全交还原生炸弹（不再拦 `on_bomb_gate`）。
     *   （`pofv_charge_cancel()` 仍然存在 —— 它服务蓄力打断，与 QCA 无关。）*/
    CHECK(pofv_charge_cancel(&st) == 0);                    /* IDLE 时打断 = 无事发生 */
    CHECK(pofv_charge_max_level(st.gauge) == 4);            /* 蓄力上限仍按气槽算 */

    /* CA 无敌：一手 C1 = 8 帧；更高等级更长（帧数本身 ❓ 自定）*/
    CHECK(pofv_ca_invuln_frames(1) == 8);
    for (i = 1; i < POFV_CA_LEVEL_MAX; ++i) CHECK(pofv_ca_invuln_frames(i) <= pofv_ca_invuln_frames(i + 1));
    CHECK(pofv_ca_invuln_frames(0) == 0 && pofv_ca_invuln_frames(5) == 0);

    /* ---- B.1 ★ 2026-09-12 审计修（任务 4）：声明层实际护住的帧数 = 档位值（off-by-one）----
     * 契约（`pofv_core.h` 的 `pofv_invuln_t`）：起窗帧 F 声明 N（= 档位值），之后每帧声明
     * N−1 … 1，**第 N+1 帧返回 0**（退声明）⇒ 「声明量 > 0」的帧数**恰好 N**。
     * 旧实现在起窗帧之外又声明 N 帧 ⇒ C1 护住 F..F+8 = 9 帧（审计的 off-by-one）。*/
    {
        pofv_invuln_t w;
        int32_t n;

        /* C1 = 8 帧：逐帧点名 */
        pofv_invuln_init(&w);
        CHECK(pofv_invuln_left(&w) == 0);
        n = pofv_invuln_grant(&w, 1);
        CHECK(n == 8);                                    /* 起窗帧（F）声明的量 */
        CHECK(pofv_invuln_left(&w) == 8);
        CHECK(pofv_invuln_tick(&w) == 7);                 /* F+1 */
        CHECK(pofv_invuln_tick(&w) == 6);                 /* F+2 */
        CHECK(pofv_invuln_tick(&w) == 5);                 /* F+3 */
        CHECK(pofv_invuln_tick(&w) == 4);                 /* F+4 */
        CHECK(pofv_invuln_tick(&w) == 3);                 /* F+5 */
        CHECK(pofv_invuln_tick(&w) == 2);                 /* F+6 */
        CHECK(pofv_invuln_tick(&w) == 1);                 /* F+7（最后一帧声明 1）*/
        CHECK(pofv_invuln_tick(&w) == 0);                 /* ★ F+8 = 第 N+1 帧：**不再声明** */
        CHECK(pofv_invuln_left(&w) == 0);
        CHECK(pofv_invuln_tick(&w) == 0);                 /* 窗口外继续轮询也是 0（不复活）*/
        CHECK(pofv_invuln_tick(0) == 0);                  /* NULL 安全 */
        CHECK(pofv_invuln_grant(0, 1) == 0);
        CHECK(pofv_invuln_left(0) == 0);

        /* 一般化：每个档位都恰好护住 N 帧，第 N+1 帧为 0 */
        for (lvl = POFV_CA_LEVEL_MIN; lvl <= POFV_CA_LEVEL_MAX; ++lvl) {
            int frames = pofv_ca_invuln_frames(lvl);
            int on_frames = 0;
            pofv_invuln_init(&w);
            if (pofv_invuln_grant(&w, lvl) > 0) ++on_frames;
            for (i = 0; i < 64; ++i) if (pofv_invuln_tick(&w) > 0) ++on_frames;
            CHECK(on_frames == frames);                   /* ★ 声明量 > 0 的帧数 = 档位值 */
            CHECK(pofv_invuln_left(&w) == 0);
        }
        /* 连开两次窗：第二次起窗按自己的档位重新计时（不会把两次叠成一串）*/
        pofv_invuln_init(&w);
        CHECK(pofv_invuln_grant(&w, 1) == 8);
        CHECK(pofv_invuln_tick(&w) == 7);
        CHECK(pofv_invuln_grant(&w, 2) == 16);            /* 覆盖起窗：F' = 本帧，声明 16 */
        CHECK(pofv_invuln_left(&w) == 16);
        CHECK(pofv_invuln_tick(&w) == 15);
        /* 等级非法：不开窗（卡侧据此什么都不做）*/
        pofv_invuln_init(&w);
        CHECK(pofv_invuln_grant(&w, 0) == 0);
        CHECK(pofv_invuln_grant(&w, 9) == 0);
        CHECK(pofv_invuln_left(&w) == 0 && pofv_invuln_tick(&w) == 0);
        /* 清窗（删卡 / 对话打断）*/
        pofv_invuln_init(&w);
        CHECK(pofv_invuln_grant(&w, 3) == 24);
        pofv_invuln_init(&w);
        CHECK(pofv_invuln_left(&w) == 0 && pofv_invuln_tick(&w) == 0);
    }

    /* ==================== C. 气槽 4 级与消耗 ==================== */
    pofv_state_init(&st);
    pofv_gauge_add_points(&st, 0);                          /* 0 点：无副作用 */
    pofv_gauge_add_points(&st, POFV_GAUGE_PTS_PER_LEVEL - 1);
    CHECK(st.gauge == 0 && st.gauge_pts == POFV_GAUGE_PTS_PER_LEVEL - 1);
    pofv_gauge_add_points(&st, 1);
    CHECK(st.gauge == 1 && st.gauge_pts == 0);              /* 满点进位 */
    pofv_gauge_add_points(&st, POFV_GAUGE_PTS_PER_LEVEL * 10);
    CHECK(st.gauge == POFV_GAUGE_MAX && st.gauge_pts == 0); /* 上限 4 级，溢出丢弃 */
    pofv_gauge_add_points(&st, 500);
    CHECK(st.gauge == POFV_GAUGE_MAX && st.gauge_pts == 0);

    CHECK(pofv_ca_cost(1) == 0 && pofv_ca_cost(2) == 1 && pofv_ca_cost(3) == 2 && pofv_ca_cost(4) == 3);
    CHECK(pofv_ca_cost(0) == 0 && pofv_ca_cost(5) == 0);     /* 非法等级：0（不扣）*/
    pofv_state_init(&st);
    st.gauge = 4;
    for (i = 1; i <= POFV_CA_LEVEL_MAX; ++i) {
        CHECK(pofv_ca_available(&st, i) == 1);
        st.gauge = i;                                       /* 恰好 L 格气：刚好发得动 L 级 */
        CHECK(pofv_ca_available(&st, i) == 1);
        CHECK(pofv_ca_available(&st, i + 1) == 0);
        st.gauge = i - 1;
        CHECK(pofv_ca_available(&st, i) == 0);
        st.gauge = 4;
    }
    /* 不变式：任何等级的 CA 放完都至少剩 1 格气（一手：C1 不消耗，其余消耗 L−1）*/
    pofv_state_init(&st);
    for (i = 1; i <= POFV_CA_LEVEL_MAX; ++i) {
        st.gauge = i;
        CHECK(pofv_ca_available(&st, i) == 1);
        st.gauge -= pofv_ca_cost(i);
        CHECK(st.gauge == 1);
    }

    /* ==================== D. SP 三档阈值（10 万 / 30 万 / 50 万，一手）==================== */
    CHECK(POFV_SP_STAGE_MAX == 3);
    CHECK(pofv_sp_threshold(0) == 100000);
    CHECK(pofv_sp_threshold(1) == 300000);
    CHECK(pofv_sp_threshold(2) == 500000);
    CHECK(pofv_sp_threshold(3) == 0 && pofv_sp_threshold(-1) == 0);
    CHECK(pofv_sp_attack_stage(0) == 0);
    CHECK(pofv_sp_attack_stage(99999) == 0);
    CHECK(pofv_sp_attack_stage(100000) == 1);
    CHECK(pofv_sp_attack_stage(299999) == 1);
    CHECK(pofv_sp_attack_stage(300000) == 2);
    CHECK(pofv_sp_attack_stage(500000) == 3);
    CHECK(pofv_sp_attack_stage(99999999) == 3);

    pofv_state_init(&st);
    st.sp = 100000;
    CHECK(pofv_sp_take_attack(&st) == 1);                   /* 越过第一档：发动 */
    CHECK(st.attacks == 1);
    CHECK(pofv_sp_take_attack(&st) == 0);                   /* 同档只报一次 */
    st.sp = 300000;
    CHECK(pofv_sp_take_attack(&st) == 1 && st.attacks == 2);
    st.sp = 500000;
    CHECK(pofv_sp_take_attack(&st) == 1 && st.attacks == 3);
    CHECK(pofv_sp_take_attack(&st) == 0 && st.attacks == 3);
    /* 一次跨两档（阈值同时越过）也只发动一次 */
    pofv_state_init(&st);
    st.sp = 600000;
    CHECK(pofv_sp_take_attack(&st) == 1 && st.sp_stage == 3 && st.attacks == 1);
    /* 击破填 SP + 充气 */
    pofv_state_init(&st);
    pofv_account_kill(&st);
    CHECK(st.kills == 1 && st.sp == POFV_SP_PER_KILL);
    CHECK(st.gauge_pts == POFV_GAUGE_PTS_PER_KILL);

    /* ==================== E. 爆风 / 红圈：半径与计数调用点 ==================== */
    pofv_blast_radius(1, &r);
    CHECK(r > 0.0f);
    {
        float prev = 0.0f;
        for (i = 1; i <= POFV_BLAST_CHAIN_MAX; ++i) {
            pofv_blast_radius(i, &r);
            CHECK(r > prev);                                /* 连爆质量越高 → 圈越大（单调）*/
            prev = r;
        }
        pofv_blast_radius(0, &r);                           /* 越界钳到 1..MAX */
        { float r1; pofv_blast_radius(1, &r1); CHECK(r == r1); }
        pofv_blast_radius(99, &r);
        { float rr; pofv_blast_radius(POFV_BLAST_CHAIN_MAX, &rr); CHECK(r == rr); }
    }
    {
        float prev = 0.0f;
        for (i = 1; i <= POFV_CA_LEVEL_MAX; ++i) {
            pofv_ca_circle_radius(i, &r);
            CHECK(r > prev);                                /* 等级越高 → 红圈越大（一手语义）*/
            prev = r;
        }
        pofv_ca_circle_radius(1, &r);
        { float r1; pofv_ca_circle_radius(0, &r1); CHECK(r == r1); }   /* 越界钳制 */
        pofv_ca_circle_radius(POFV_CA_LEVEL_MAX, &r);
        CHECK(r >= 400.0f);                                 /* C4 = 近乎全屏（一手语义）*/
    }

    /* 蓝圈记账：按消掉的白弹数充气 + 填 SP；连爆质量加权 */
    pofv_state_init(&st);
    pofv_account_blast(&st, 0, 1);                          /* 0 颗：什么也不记 */
    CHECK(st.white_cancelled == 0 && st.gauge_pts == 0 && st.sp == 0);
    pofv_account_blast(&st, 2, 1);
    CHECK(st.white_cancelled == 2 && st.any_cancelled == 2);
    CHECK(st.gauge_pts == 2 * POFV_GAUGE_PTS_PER_WHITE_CANCEL);
    CHECK(st.sp == 2 * POFV_SP_PER_WHITE_CANCEL);
    {
        int32_t base = st.gauge_pts;
        pofv_account_blast(&st, 1, 3);                      /* 同样的 1 颗，档高 → 充气更多 */
        CHECK(st.gauge_pts - base == 3 * POFV_GAUGE_PTS_PER_WHITE_CANCEL);
    }
    /* ★ 红圈记账：只填 SP、**不回复气**（一手：「红色消弹圈消掉的白弹…不回复气」）*/
    pofv_state_init(&st);
    st.gauge = 2; st.gauge_pts = 7;
    pofv_account_ca(&st, 5);
    CHECK(st.gauge == 2 && st.gauge_pts == 7);
    CHECK(st.any_cancelled == 5 && st.white_cancelled == 0);
    CHECK(st.sp == 5 * POFV_SP_PER_RED_CANCEL);

    /* 连爆质量档：窗口内累加、窗口外回 1 档、封顶 */
    pofv_state_init(&st);
    CHECK(pofv_chain_register_kill(&st, 100) == 1);
    CHECK(pofv_chain_register_kill(&st, 100 + POFV_CHAIN_WINDOW_FRAMES) == 2);
    CHECK(pofv_chain_register_kill(&st, 100 + POFV_CHAIN_WINDOW_FRAMES * 2) == 3);
    CHECK(pofv_chain_register_kill(&st, 100 + POFV_CHAIN_WINDOW_FRAMES * 3) == 4);
    CHECK(pofv_chain_register_kill(&st, 100 + POFV_CHAIN_WINDOW_FRAMES * 4) == POFV_BLAST_CHAIN_MAX);
    CHECK(pofv_chain_register_kill(&st, 10000) == 1);       /* 隔太久：回 1 档 */

    /* 爆风队列：死亡回调只入队（O(1)），帧末按预算出队 */
    {
        pofv_blast_queue_t q;
        pofv_blast_t b;
        int32_t budget;
        pofv_blast_queue_init(&q);
        CHECK(pofv_blast_take(&q, &b) == 0);
        CHECK(pofv_blast_enqueue(&q, 1.0f, 2.0f, 1) == 1);
        CHECK(pofv_blast_enqueue(&q, 3.0f, 4.0f, 2) == 1);
        CHECK(pofv_blast_take(&q, &b) == 1);
        CHECK(b.x == 1.0f && b.y == 2.0f && b.chain == 1);   /* FIFO */
        CHECK(pofv_blast_take(&q, &b) == 1);
        CHECK(b.x == 3.0f && b.chain == 2);
        CHECK(pofv_blast_take(&q, &b) == 0);

        /* 帧预算：一次清场帧入队 300 个（review §4.1 的最坏情形），本帧只做 POFV_BLASTS_PER_FRAME 个 */
        for (i = 0; i < 300; ++i) pofv_blast_enqueue(&q, (float)i, 0.0f, 1);
        CHECK(q.n == POFV_BLAST_QUEUE_MAX);                  /* 满即丢，不越界 */
        CHECK(q.dropped == 300 - POFV_BLAST_QUEUE_MAX);
        budget = POFV_BLASTS_PER_FRAME;
        for (i = 0; i < POFV_BLASTS_PER_FRAME; ++i) CHECK(pofv_blast_take_budget(&q, &b, &budget) == 1);
        CHECK(budget == 0);
        CHECK(pofv_blast_take_budget(&q, &b, &budget) == 0); /* 预算用完：本帧不再消弹 */
        CHECK(q.n == POFV_BLAST_QUEUE_MAX - POFV_BLASTS_PER_FRAME);
        budget = 2;
        CHECK(pofv_blast_take_budget(&q, &b, &budget) == 1);
        CHECK(pofv_blast_take_budget(&q, &b, &budget) == 1);
        CHECK(pofv_blast_take_budget(&q, &b, &budget) == 0);

        /* 环形回绕：填满两轮后仍按 FIFO 顺序 */
        pofv_blast_queue_init(&q);
        for (i = 0; i < POFV_BLAST_QUEUE_MAX; ++i) pofv_blast_enqueue(&q, (float)i, 0.0f, 1);
        for (i = 0; i < POFV_BLAST_QUEUE_MAX; ++i) { CHECK(pofv_blast_take(&q, &b) == 1); CHECK(b.x == (float)i); }
        pofv_blast_enqueue(&q, 42.0f, 0.0f, 4);
        CHECK(pofv_blast_take(&q, &b) == 1 && b.x == 42.0f && b.chain == 4);
        CHECK(q.dropped == 0);
    }

    /* ==================== F. 活化灵 ==================== */
    {
        pofv_spirit_pool_t pool;
        int32_t slots[POFV_SPIRIT_MAX];
        int32_t n_act = 0;
        int k, n;

        pofv_spirit_pool_init(&pool);
        CHECK(pool.live == 0);
        k = pofv_spirit_spawn(&pool, 10.0f, 100.0f, 7);
        CHECK(k == 0 && pool.live == 1);
        CHECK(pool.s[0].active == 1 && pool.s[0].activated == 0);
        CHECK(pool.s[0].hp == POFV_SPIRIT_HP_TOUGH);         /* 一手：普通幽灵很硬 */
        CHECK(pool.s[0].selfdestruct_in == -1);
        CHECK(pool.s[0].src_enemy_id == 7);

        /* 固灵：领域内的未活化灵魂 → 活化灵（HP **减半** + 缓慢上浮 + 自爆倒计时）*/
        CHECK(pofv_spirit_spawn(&pool, 400.0f, 400.0f, 8) == 1);   /* 领域外 */
        n = pofv_spirit_activate_in_domain(&pool, 10.0f, 100.0f, &n_act);
        CHECK(n == 1 && n_act == 1);
        CHECK(pool.s[0].activated == 1);
        CHECK(pool.s[0].hp == POFV_SPIRIT_HP_TOUGH / 2);     /* ★ 一手语义「HP 减半」*/
        CHECK(pool.s[0].selfdestruct_in == POFV_SPIRIT_SELFDESTRUCT_FRAMES);
        CHECK(pool.s[1].activated == 0);
        CHECK(pofv_spirit_activate_in_domain(&pool, 10.0f, 100.0f, &n_act) == 0);  /* 已活化：不重复 */

        /* 活化灵上浮 + 自爆到点 */
        {
            float y0 = pool.s[0].y;
            n = pofv_spirit_tick(&pool, slots, POFV_SPIRIT_MAX);
            CHECK(n == 0);
            CHECK(pool.s[0].y > y0);                         /* 一手：缓慢上浮 */
            CHECK(pool.s[0].selfdestruct_in == POFV_SPIRIT_SELFDESTRUCT_FRAMES - 1);
        }
        for (i = 0; i < POFV_SPIRIT_SELFDESTRUCT_FRAMES - 1; ++i)
            n = pofv_spirit_tick(&pool, slots, POFV_SPIRIT_MAX);
        CHECK(n == 1 && slots[0] == 0);                      /* 自爆：槽位报出来并清掉 */
        CHECK(pool.s[0].active == 0 && pool.live == 1);

        /* 自爆名额（cap）不会丢事件：满了就留在槽里下一帧再爆 */
        pofv_spirit_pool_init(&pool);
        pofv_spirit_spawn(&pool, 0.0f, 0.0f, 1);
        pofv_spirit_spawn(&pool, 5.0f, 0.0f, 2);
        pofv_spirit_activate_in_domain(&pool, 0.0f, 0.0f, &n_act);
        CHECK(n_act == 2);
        for (i = 0; i < POFV_SPIRIT_SELFDESTRUCT_FRAMES; ++i) pofv_spirit_tick(&pool, slots, 1);
        CHECK(pool.live == 1);                               /* 第一颗爆了，第二颗本帧没名额 */
        CHECK(pofv_spirit_tick(&pool, slots, POFV_SPIRIT_MAX) == 1);
        CHECK(pool.live == 0);

        /* 爆风波及：一手「普通幽灵…只有被连爆波及时才爆」*/
        pofv_spirit_pool_init(&pool);
        pofv_spirit_spawn(&pool, 0.0f, 0.0f, 1);
        pofv_spirit_spawn(&pool, 500.0f, 0.0f, 2);
        n = pofv_spirit_blast_hit(&pool, 0.0f, 0.0f, 64.0f, POFV_BLAST_SPIRIT_DMG, slots, POFV_SPIRIT_MAX);
        CHECK(n == 1 && slots[0] == 0);
        CHECK(pool.s[0].active == 0);                        /* 圈内：击破 */
        CHECK(pool.s[1].active == 1);                        /* 圈外：没事 */
        CHECK(pool.live == 1);
        CHECK(pofv_spirit_blast_hit(&pool, 0.0f, 0.0f, 0.0f, POFV_BLAST_SPIRIT_DMG, slots, POFV_SPIRIT_MAX) == 0);

        /* 池上限 + 手动移除 */
        pofv_spirit_pool_init(&pool);
        for (i = 0; i < POFV_SPIRIT_MAX; ++i) CHECK(pofv_spirit_spawn(&pool, (float)i, 0.0f, i) == i);
        CHECK(pool.live == POFV_SPIRIT_MAX);
        CHECK(pofv_spirit_spawn(&pool, 0.0f, 0.0f, 99) == -1);          /* 满：丢弃 */
        pofv_spirit_remove(&pool, 3);
        CHECK(pool.live == POFV_SPIRIT_MAX - 1);
        CHECK(pofv_spirit_spawn(&pool, 0.0f, 0.0f, 100) == 3);          /* 空出来的槽可复用 */
        pofv_spirit_remove(&pool, 3);
        pofv_spirit_remove(&pool, 3);                                    /* 重复移除：无副作用 */
        CHECK(pool.live == POFV_SPIRIT_MAX - 1);
        pofv_spirit_remove(&pool, -1);
        pofv_spirit_remove(&pool, POFV_SPIRIT_MAX);
        CHECK(pool.live == POFV_SPIRIT_MAX - 1);
        pofv_spirit_pool_init(&pool);
        CHECK(pool.live == 0);
    }

    printf("test_pofv_core: %d passed, %d failed\n", s_pass, s_fail);
    return s_fail ? 1 : 0;
}
