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
 * 覆盖：白弹筛选 / 蓄力状态机（按住 C 的电平）/ 气槽 4 级与消耗 / SP 阈值 /
 *       爆风与红圈的半径与计数调用点 / 爆风队列与帧预算 / 活化灵状态机 /
 *       **灵力道具池（口径 5：打包面值守恒 / 磁吸收取 / 寿命 / 池满）**。
 * ⚠ 2026-10-01（合成卡 owner 口径 2）：**拦 Z 已整条删除**（`pofv_charge_suppress` 不再存在）
 *   ⇒ 本文件不再有它的断言；一手 10 帧闸的事实留在核心注释与 `TUNING.md` §2.0v #1。 */
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

    /* ==================== B. 蓄力状态机（按住 C 的电平）==================== */
    pofv_state_init(&st);
    CHECK(st.gauge == 0 && st.phase == POFV_CHARGE_IDLE && st.level == 0);
    CHECK(pofv_charge_max_level(0) == 0);
    CHECK(pofv_charge_max_level(2) == 2);
    CHECK(pofv_charge_max_level(9) == POFV_GAUGE_MAX);      /* 钳到 4 级 */

    /* ★★★ 2026-10-01（合成卡 owner 口径 2「不拦截 Z 了」）：**拦 Z 整条删除**
     *   —— `pofv_charge_suppress()` / `POFV_SUPPRESS_AFTER_FRAMES` 已不存在，
     *   这里能断言的事实只剩「气槽空也照样蓄力」（蓄力只由按住 C 的帧数决定）。
     *   一手（TH09 `th09:0x41fa17` 的 10 帧闸）仍然成立，是**本卡刻意不复刻**
     *   （登记 `TUNING.md` §2.0v #1）。*/
    CHECK(pofv_charge_begin(&st) == 1);
    for (i = 1; i < POFV_CHARGE_FRAMES_PER_LEVEL; ++i) {
        CHECK(pofv_charge_hold(&st) == 0);                  /* 还没升到 C1（等级随时间涨）*/
        CHECK(st.level == 0);
    }
    CHECK(pofv_charge_hold(&st) == 1);                      /* ★ 升到 C1：卡侧播「叮」*/
    CHECK(st.hold_frames == POFV_CHARGE_FRAMES_PER_LEVEL);
    CHECK(st.level == 1 && st.gauge == 0);                  /* ★ 气槽空也涨到 C1 */
    CHECK(pofv_charge_release(&st, &lvl) == 0);             /* 没气 ⇒ 放不出 CA（蓄力量被气槽钳）*/
    CHECK(lvl == 0 && st.gauge == 0);

    /* 有 4 格气：按住 → 升级 → 松开 → 发动 CA */
    st.gauge = POFV_GAUGE_MAX;
    CHECK(pofv_charge_begin(&st) == 1);
    CHECK(pofv_charge_begin(&st) == 0);                     /* 已在按住态：重复无副作用 */
    CHECK(st.max_level == POFV_GAUGE_MAX);                  /* b = min(气槽, 4) */
    for (i = 1; i < POFV_CHARGE_FRAMES_PER_LEVEL; ++i) {
        CHECK(pofv_charge_hold(&st) == 0);                  /* 还没升到 C1（等级仍随时间涨）*/
        CHECK(st.level == 0);
    }
    CHECK(pofv_charge_hold(&st) == 1);                      /* ★ 升到 C1：卡侧播「叮」*/
    CHECK(st.level == 1);
    for (i = 0; i < POFV_CHARGE_FRAMES_PER_LEVEL; ++i) pofv_charge_hold(&st);
    CHECK(st.level == 2 && st.max_level == 4);
    CHECK(pofv_charge_release(&st, &lvl) == 1);
    CHECK(lvl == 2 && st.gauge == POFV_GAUGE_MAX - 1);      /* C2 消耗 1 格 */
    CHECK(st.phase == POFV_CHARGE_IDLE && st.level == 0);

    /* 上限 = 气槽：只有 2 格气 ⇒ 蓄力量 a 照涨（一手：蓄力量只被气槽**钳上界**），
     * 但**放得出的档** = min(a, b) = 2（`pofv_charge_usable_level`）。*/
    st.gauge = 2;
    CHECK(pofv_charge_begin(&st) == 1);
    for (i = 0; i < POFV_CHARGE_FRAMES_PER_LEVEL * 5; ++i) pofv_charge_hold(&st);
    CHECK(st.level == POFV_CA_LEVEL_MAX && st.max_level == 2);
    CHECK(pofv_charge_usable_level(&st) == 2);

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
     *   109 的 B 侧只剩「按住 C 蓄力」，X 键完全交还原生炸弹（不再拦 `on_bomb_gate`）。
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
    /* 击破填 SP（★ 2026-10-01 口径 5：**不再直接加气** ⇒ 充能走灵力道具）*/
    pofv_state_init(&st);
    pofv_account_kill(&st);
    CHECK(st.kills == 1 && st.sp == POFV_SP_PER_KILL);
    CHECK(st.gauge == 0 && st.gauge_pts == 0);              /* ★ 直加点那条路已删除 */

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

    /* 蓝圈记账（★ 2026-10-01 口径 5：只记 counters + 填 SP，**不直接加气**）；
     * 充能的那笔账改由 `pofv_power_value_cancel()` 给出，由卡侧打包成灵力道具实体。*/
    pofv_state_init(&st);
    pofv_account_blast(&st, 0, 1);
    CHECK(st.white_cancelled == 0 && st.gauge_pts == 0 && st.sp == 0);
    CHECK(pofv_power_value_cancel(0, 1) == 0);
    pofv_account_blast(&st, 2, 1);
    CHECK(st.white_cancelled == 2 && st.any_cancelled == 2);
    CHECK(st.gauge == 0 && st.gauge_pts == 0);              /* ★ 直加点那条路已删除 */
    CHECK(st.sp == 2 * POFV_SP_PER_WHITE_CANCEL);
    /* ★ 连爆质量加权**挪进面值公式**：同样的 1 颗，档 3 ⇒ 3 倍面值（保留原来的味道）*/
    CHECK(pofv_power_value_cancel(1, 1) == POFV_POWER_PTS_CANCEL);
    CHECK(pofv_power_value_cancel(1, 3) == 3 * POFV_POWER_PTS_CANCEL);
    CHECK(pofv_power_value_cancel(2, 2) == 2 * 2 * POFV_POWER_PTS_CANCEL);
    CHECK(pofv_power_value_cancel(1, 0) == POFV_POWER_PTS_CANCEL);      /* 越界档钳到 1 */
    CHECK(pofv_power_value_cancel(1, 99) == POFV_BLAST_CHAIN_MAX * POFV_POWER_PTS_CANCEL);
    /* ★ 红圈记账：只填 SP、**不回复气**（一手：「红色消弹圈消掉的白弹…不回复气」）*/
    pofv_state_init(&st);
    st.gauge = 2; st.gauge_pts = 7;
    pofv_account_ca(&st, 5);
    CHECK(st.gauge == 2 && st.gauge_pts == 7);
    CHECK(st.any_cancelled == 5 && st.white_cancelled == 0);
    CHECK(st.sp == 5 * POFV_SP_PER_RED_CANCEL);

    /* 连爆档的来源（★★ 2026-09-22 P5 #4 重做）：**档 = 点着它的那个暴风场的档 + 1**
     *   —— 帧窗口的节拍器已删除（用户：「暴风的连锁似乎不该这么快」）。*/
    {
        pofv_storm_pool_t p;
        pofv_storm_t out;
        pofv_storm_pool_init(&p);
        CHECK(p.live == 0 && p.dropped == 0);
        CHECK(pofv_storm_chain_at(&p, 0.0f, 0.0f) == 0);          /* 没有场 ⇒ 0（自机弹打死的）*/

        CHECK(pofv_storm_spawn(&p, 0.0f, 0.0f, 1) == 0);
        CHECK(p.live == 1);
        /* 出生帧 age = 0 ⇒ 半径 = r₀ = 16（★ 一手：etama 脚本的 `pos(16.0f,…)`）*/
        CHECK(pofv_storm_slot(&p, 0) != NULL);
        out = *pofv_storm_slot(&p, 0);
        pofv_storm_radius(&out, &r);
        CHECK(r == POFV_STORM_R0);
        CHECK(pofv_storm_chain_at(&p, 0.0f, 0.0f) == 1);          /* 圆心在圈里 */
        CHECK(pofv_storm_chain_at(&p, 1000.0f, 0.0f) == 0);       /* 圈外 */

        /* 半径逐帧长，到寿命那一刻 = 该档终点半径（旧值 64 是「缺乏范围」的根因）*/
        for (i = 0; i < POFV_STORM_FRAMES - 1; ++i) {
            pofv_storm_tick(&p);
            out = *pofv_storm_slot(&p, 0);
            pofv_storm_radius(&out, &r);
            CHECK(r > POFV_STORM_R0 && r <= 176.0f);
        }
        pofv_storm_tick(&p);                                      /* 满寿命：释放 */
        CHECK(p.live == 0);
        CHECK(pofv_storm_chain_at(&p, 0.0f, 0.0f) == 0);          /* 场没了 ⇒ 连锁也就断了 */
        pofv_storm_radius(pofv_storm_slot(&p, 0), &r);
        CHECK(r == 0.0f);                                         /* 空槽 ⇒ 0 */

        /* 池满：丢（不越界、不崩），并记账 */
        for (i = 0; i < POFV_STORM_MAX; ++i) CHECK(pofv_storm_spawn(&p, (float)i, 0.0f, 1) == i);
        CHECK(pofv_storm_spawn(&p, 0.0f, 0.0f, 1) == -1);
        CHECK(p.dropped == 1 && p.live == POFV_STORM_MAX);
        CHECK(pofv_storm_slot(&p, POFV_STORM_MAX) == NULL);
        CHECK(pofv_storm_slot(NULL, 0) == NULL);
    }
    /* 档越高 → 终点半径越大（单调），且档 1 = 176（TH09 一手：CA 释放节点 16+20×8）*/
    {
        float prev = 0.0f;
        for (i = 1; i <= POFV_BLAST_CHAIN_MAX; ++i) {
            pofv_storm_final_radius(i, &r);
            CHECK(r > prev);
            prev = r;
        }
        pofv_storm_final_radius(1, &r);
        CHECK(r == 176.0f);
    }

    /* 消弹的帧预算：**场模型下不再是「队列出队」，而是「每帧最多几次整池扫弹」**
     *   （`POFV_STORM_SCANS_PER_FRAME`）。这里只钉住这个预算数存在且 > 0 ——
     *   真正的闸在卡侧 `on_tick_2` 的 `scan_budget` 里。*/
    CHECK(POFV_STORM_SCANS_PER_FRAME > 0);
    CHECK(POFV_STORM_SCANS_PER_FRAME < POFV_STORM_MAX * POFV_STORM_FRAMES);
    CHECK(POFV_BLAST_CHAIN_MAX >= 2);

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

    /* ==================== G. 灵力道具（自建实体池；口径 5）====================
     * ★★ 2026-10-01：充能不再直接加点，而是「消弹 / 击破 / 结界碰弹 → 掉实体 → 拾取 → 涨槽」。
     *   本段钉住：池不变式 / **打包面值守恒** / 磁吸与收取 / 圈外只飘落 / 寿命 / 池满与上限。 */
    {
        pofv_power_pool_t pool;
        float pp[3] = { 100.0f, 200.0f, 0.0f };
        int32_t pts = 0;
        int k, n, j;

        pofv_power_pool_init(&pool);
        CHECK(pool.live == 0 && pool.spawned == 0 && pool.dropped == 0);

        /* 池的基本不变式 */
        CHECK(pofv_power_spawn(&pool, 10.0f, 20.0f, 10, POFV_POWER_SRC_KILL) == 0);
        CHECK(pool.live == 1 && pool.s[0].active == 1);
        CHECK(pool.s[0].x == 10.0f && pool.s[0].y == 20.0f);
        CHECK(pool.s[0].value == 10 && pool.s[0].life == POFV_POWER_LIFE);
        CHECK(pool.s[0].src == POFV_POWER_SRC_KILL);
        CHECK(pofv_power_spawn(&pool, 0.0f, 0.0f, 5, POFV_POWER_SRC_AURA) == 1);
        CHECK(pool.live == 2);
        CHECK(pofv_power_clear(&pool) == 2);
        CHECK(pool.live == 0);

        /* ★ 打包撒：颗数被 `max_spawn` 截断，但**总面值守恒**（多的摊到已撒的那几颗上）*/
        pofv_power_pool_init(&pool);
        k = pofv_power_spawn_packed(&pool, 50.0f, 60.0f, 10, 10, 4, POFV_POWER_SRC_CANCEL, &pts);
        CHECK(k == 4 && pool.live == 4);
        CHECK(pts == 100);                                  /* 10 颗 × 10 分全给出去 */
        {
            int32_t sum = 0;
            for (j = 0; j < POFV_POWER_MAX; ++j) if (pool.s[j].active) sum += pool.s[j].value;
            CHECK(sum == 100);
        }
        /* 只有一颗时**在原地**（不铺开）*/
        pofv_power_pool_init(&pool);
        CHECK(pofv_power_spawn_packed(&pool, 7.0f, 8.0f, 1, 20, 4, POFV_POWER_SRC_KILL, &pts) == 1);
        CHECK(pts == 20 && pool.s[0].x == 7.0f && pool.s[0].y == 8.0f && pool.s[0].value == 20);
        /* 非法入参：什么都不做 */
        pofv_power_pool_init(&pool);
        CHECK(pofv_power_spawn_packed(&pool, 0.0f, 0.0f, 0, 10, 4, 0, &pts) == 0 && pts == 0);
        CHECK(pofv_power_spawn_packed(&pool, 0.0f, 0.0f, 5, 0, 4, 0, &pts) == 0 && pts == 0);
        CHECK(pofv_power_spawn_packed(&pool, 0.0f, 0.0f, 5, 10, 0, 0, &pts) == 0 && pts == 0);
        CHECK(pool.live == 0);

        /* 池余量参与截断：只剩 2 格 ⇒ 只撒 2 颗，面值仍然全给 */
        pofv_power_pool_init(&pool);
        for (j = 0; j < POFV_POWER_MAX - 2; ++j) pofv_power_spawn(&pool, 0.0f, 0.0f, 1, 0);
        CHECK(pool.live == POFV_POWER_MAX - 2);
        k = pofv_power_spawn_packed(&pool, 0.0f, 0.0f, 10, 10, 4, POFV_POWER_SRC_CANCEL, &pts);
        CHECK(k == 2 && pool.live == POFV_POWER_MAX);
        CHECK(pts == 100);                                  /* ★ 面值守恒（颗数少了、分没少）*/
        /* 池全满：一颗都撒不出 ⇒ 报 0 并记账（卡侧只少一次掉落，不崩） */
        CHECK(pofv_power_spawn_packed(&pool, 0.0f, 0.0f, 10, 10, 4, POFV_POWER_SRC_CANCEL, &pts) == 0);
        CHECK(pts == 0 && pool.dropped >= 10);

        /* 收取：在收取半径内 ⇒ 点入账、槽当场释放 */
        pofv_power_pool_init(&pool);
        pofv_power_spawn(&pool, 100.0f, 200.0f, 30, POFV_POWER_SRC_KILL);
        CHECK(pofv_power_tick(&pool, pp, &pts) == 1);
        CHECK(pts == 30 && pool.live == 0 && pool.absorbed == 1);

        /* 磁吸：圈内（MAG_R 内、ABSORB_R 外）朝自机走 ⇒ 有限帧内必被收 */
        pofv_power_pool_init(&pool);
        pofv_power_spawn(&pool, 100.0f + POFV_POWER_MAG_R, 200.0f, 10, POFV_POWER_SRC_AURA);
        { float x0 = pool.s[0].x; pofv_power_tick(&pool, pp, &pts); CHECK(pool.s[0].x < x0); }
        n = 0;
        while (pool.live > 0 && n < 200) { pofv_power_tick(&pool, pp, &pts); ++n; }
        CHECK(pool.live == 0 && pts >= 10);

        /* 圈外：只飘落（y 增），不磁吸、不收取 */
        pofv_power_pool_init(&pool);
        pofv_power_spawn(&pool, 100.0f + POFV_POWER_MAG_R * 4.0f, 200.0f, 10, POFV_POWER_SRC_AURA);
        {
            float y0 = pool.s[0].y;
            pofv_power_tick(&pool, pp, &pts);
            CHECK(pool.s[0].y > y0);
            CHECK(pts == 0 && pool.live == 1);
        }

        /* 没有自机（过场 / 单测）：只飘落，不磁吸不收取 */
        pofv_power_pool_init(&pool);
        pofv_power_spawn(&pool, 0.0f, 0.0f, 10, POFV_POWER_SRC_AURA);
        CHECK(pofv_power_tick(&pool, 0, &pts) == 0 && pts == 0 && pool.s[0].y > 0.0f);

        /* 寿命：到点自行消失（不刷屏），并记账 */
        pofv_power_pool_init(&pool);
        pofv_power_spawn(&pool, 0.0f, 0.0f, 10, POFV_POWER_SRC_AURA);
        for (n = 0; n < POFV_POWER_LIFE; ++n) pofv_power_tick(&pool, 0, &pts);
        CHECK(pool.live == 0 && pool.expired == 1 && pts == 0);

        /* 池满：丢弃不越界；NULL 安全 */
        pofv_power_pool_init(&pool);
        for (j = 0; j < POFV_POWER_MAX; ++j) CHECK(pofv_power_spawn(&pool, (float)j, 0.0f, 1, 0) == j);
        CHECK(pofv_power_spawn(&pool, 0.0f, 0.0f, 1, 0) == -1);
        CHECK(pool.live == POFV_POWER_MAX);
        CHECK(pofv_power_clear(&pool) == POFV_POWER_MAX);
        CHECK(pool.live == 0);
        CHECK(pofv_power_spawn(0, 0.0f, 0.0f, 1, 0) == -1);
        CHECK(pofv_power_spawn_packed(0, 0.0f, 0.0f, 1, 1, 1, 0, &pts) == 0);
        CHECK(pofv_power_tick(0, pp, &pts) == 0 && pts == 0);
        CHECK(pofv_power_clear(0) == 0);
        pofv_power_pool_init(0);                            /* NULL 安全 */
    }

    /* ==================== H. 固灵结界（几何；一手 TH19 Reimu 圆）==================== */
    CHECK(POFV_AURA_R > 0.0f);
    CHECK(POFV_AURA_ITEMS_PER_FRAME > 0);
    CHECK(POFV_AURA_R >= 14.0f);                            /* 一手 TH19 逐类 K = 14..600 的下界 */

    /* 圆心必中；边界 `r` 处**不含**（一手是严格 `<`）；半径外不中 */
    CHECK(pofv_aura_hit(0.0f, 0.0f, 96.0f, 0.0f) == 1);
    CHECK(pofv_aura_hit(95.9f, 0.0f, 96.0f, 0.0f) == 1);
    CHECK(pofv_aura_hit(96.0f, 0.0f, 96.0f, 0.0f) == 0);    /* 96² == 96² ⇒ 不中（严格 <）*/
    CHECK(pofv_aura_hit(96.1f, 0.0f, 96.0f, 0.0f) == 0);
    CHECK(pofv_aura_hit(57.0f, 76.0f, 96.0f, 0.0f) == 1);   /* 斜向：57²+76² = 9025 < 9216 */
    CHECK(pofv_aura_hit(-95.0f, 0.0f, 96.0f, 0.0f) == 1);   /* 方向无关 */

    /* ★ 一手：pad（目标半径）以**平方和**并入（Reimu 支），不是并进半径（Sanae 支）。
     *   r=96 / pad=20 ⇒ 有效半径 = √(96²+20²) ≈ 98.06；线性并法会给出 116。 */
    CHECK(pofv_aura_hit(98.0f, 0.0f, 96.0f, 20.0f) == 1);
    CHECK(pofv_aura_hit(98.1f, 0.0f, 96.0f, 20.0f) == 0);
    CHECK(pofv_aura_hit(110.0f, 0.0f, 96.0f, 20.0f) == 0);  /* ★ 线性并法会误判为命中 */

    /* 半径 ≤ 0（结界没展开）⇒ 永不命中；NaN 安全 */
    CHECK(pofv_aura_hit(0.0f, 0.0f, 0.0f, 0.0f) == 0);
    CHECK(pofv_aura_hit(0.0f, 0.0f, -1.0f, 0.0f) == 0);

    /* 多位置打包撒（结界碰弹：位置 = 被碰到的弹的原位，颗数受每帧上限钳、面值守恒） */
    {
        pofv_power_pool_t pool;
        const float ox[4] = { 1.0f, 2.0f, 3.0f, 4.0f };
        const float oy[4] = { 10.0f, 20.0f, 30.0f, 40.0f };
        int32_t pts = 0;
        int j;

        pofv_power_pool_init(&pool);
        CHECK(pofv_power_spawn_packed_at(&pool, ox, oy, 8, 5, 4, POFV_POWER_SRC_AURA, &pts) == 4);
        CHECK(pts == 40 && pool.live == 4);                 /* 8 颗 × 5 分，颗数被截到 4 */
        {
            int32_t sum = 0;
            for (j = 0; j < 4; ++j) sum += pool.s[j].value;
            CHECK(sum == 40);
        }
        /* 位置逐个用上（原位掉落） */
        CHECK(pool.s[0].x == 1.0f && pool.s[1].x == 2.0f && pool.s[3].y == 40.0f);
        /* 非法入参 / NULL 安全 */
        CHECK(pofv_power_spawn_packed_at(&pool, ox, oy, 0, 5, 4, 0, &pts) == 0 && pts == 0);
        CHECK(pofv_power_spawn_packed_at(0, ox, oy, 4, 5, 4, 0, &pts) == 0);
        CHECK(pofv_power_spawn_packed_at(&pool, 0, oy, 4, 5, 4, 0, &pts) == 0);
        CHECK(pofv_power_spawn_packed_at(&pool, ox, oy, 4, 0, 4, 0, &pts) == 0);
    }

    /* ==================== I. 灵（自制真敌人）的句柄池与循环撒节拍 ====================
     * owner 口径 8/9/10：灵都自建、按周期循环撒、任何 boss 在场就不撒、同屏上限 4（一手 TH09）。 */
    CHECK(POFV_GHOST_MAX == 4);                              /* ★ 一手 TH09 的槽数 */
    CHECK(POFV_GHOST_CYCLE_FRAMES > 0);
    CHECK(POFV_GHOST_CALM_FRAMES > 0);
    CHECK(POFV_GHOST_HP > POFV_GHOST_HP_CALM);               /* 沈静化 = 耐久骤降（一手 TH19） */
    CHECK(POFV_GHOST_KIND_MAX == 4);                         /* 狼 / 獺 / 鷲 / TH09 的霊 */
    {
        pofv_ghost_pool_t g;
        int32_t kind = -1, slots[POFV_GHOST_MAX];
        int n, j, hits;

        pofv_ghost_pool_init(&g);
        CHECK(g.live == 0 && g.cycle == 0 && g.next_kind == POFV_GHOST_KIND_WOLF);

        /* 循环撒的节拍 + 四族轮转（狼 → 獺 → 鷲 → 霊 → 狼）：每撒一只后要等满一个周期。
         * 前 3 只登记进池（留一个空位给下面的满员测试）。 */
        {
            const int32_t expect[4] = { POFV_GHOST_KIND_WOLF, POFV_GHOST_KIND_OTTER,
                                        POFV_GHOST_KIND_EAGLE, POFV_GHOST_KIND_HYZ };
            for (j = 0; j < 4; ++j) {
                n = 0;
                while (pofv_ghost_cycle_tick(&g, 0, &kind) == 0 && n < POFV_GHOST_CYCLE_FRAMES * 2) ++n;
                CHECK(kind == expect[j]);
                CHECK(n == (j == 0 ? 0 : POFV_GHOST_CYCLE_FRAMES - 1));   /* 首帧即撒；之后满一周期 */
                if (j < 3) CHECK(pofv_ghost_register(&g, 100 + j, kind) == j);
                else CHECK(g.next_kind == POFV_GHOST_KIND_WOLF);          /* 轮转回第一族 */
            }
        }
        CHECK(g.live == 3);

        /* ★ 口径 9（方案甲）：任何 boss 在场就不撒，且**不重置**倒计时 */
        g.cycle = 0;
        CHECK(pofv_ghost_cycle_tick(&g, 1, &kind) == 0);
        CHECK(g.cycle == 0);                                 /* boss 一走立刻就能撒（不被罚一个周期）*/
        CHECK(pofv_ghost_cycle_tick(&g, 0, &kind) == 1 && g.cycle == POFV_GHOST_CYCLE_FRAMES);
        CHECK(pofv_ghost_register(&g, 103, kind) == 3);      /* 第 4 只 ⇒ 满员 */

        /* 同屏上限（一手 TH09 = 4）：满员时即使节拍到点也不撒 */
        CHECK(g.live == POFV_GHOST_MAX);
        g.cycle = 0;
        CHECK(pofv_ghost_register(&g, 999, 0) == -1);        /* 池满 */
        CHECK(pofv_ghost_cycle_tick(&g, 0, &kind) == 0);
        CHECK(pofv_ghost_cycle_tick(&g, 1, &kind) == 0);     /* boss 与满员两条门都挡住 */

        /* 查找 / 释放 / 槽复用 */
        CHECK(pofv_ghost_find(&g, 100) == 0 && g.s[0].enemy_id == 100);
        CHECK(pofv_ghost_find(&g, 404) == -1);
        CHECK(pofv_ghost_find(&g, 0) == -1 && pofv_ghost_find(0, 1) == -1);
        CHECK(pofv_ghost_release(&g, 0) == 1 && g.live == POFV_GHOST_MAX - 1);
        CHECK(pofv_ghost_release(&g, 0) == 0);               /* 重复释放：无副作用 */
        CHECK(pofv_ghost_release(&g, -1) == 0 && pofv_ghost_release(&g, POFV_GHOST_MAX) == 0);
        CHECK(pofv_ghost_find(&g, 100) == -1);
        CHECK(pofv_ghost_register(&g, 301, 0) == 0);         /* 空出来的槽可复用 */
        CHECK(pofv_ghost_find(&g, 301) == 0);
        pofv_ghost_pool_init(&g);

        /* 沈静化：幂等（不重置计时）+ 到 POFV_GHOST_CALM_FRAMES 自然成佛 */
        pofv_ghost_register(&g, 500, POFV_GHOST_KIND_EAGLE);
        CHECK(g.s[0].calmed == 0 && g.s[0].calm_age == 0);
        CHECK(pofv_ghost_calm(&g, 0) == 1);
        CHECK(g.s[0].calmed == 1 && g.calmed_total == 1);
        for (n = 0; n < 30; ++n) pofv_ghost_tick(&g, slots, POFV_GHOST_MAX);
        CHECK(pofv_ghost_calm(&g, 0) == 0);                  /* ★ 幂等 */
        CHECK(g.s[0].calm_age == 30);
        hits = 0;
        for (n = 0; n < POFV_GHOST_CALM_FRAMES + 4; ++n) hits += pofv_ghost_tick(&g, slots, POFV_GHOST_MAX);
        CHECK(hits == 1 && g.live == 0 && g.expired == 1);
        CHECK(pofv_ghost_calm(&g, 0) == 0);                  /* 槽已空 */
        CHECK(pofv_ghost_calm(&g, -1) == 0 && pofv_ghost_calm(0, 0) == 0);

        /* 未沈静化的灵**不会**自然成佛（一直挂在场上，等玩家打） */
        pofv_ghost_pool_init(&g);
        pofv_ghost_register(&g, 600, 0);
        hits = 0;
        for (n = 0; n < POFV_GHOST_CALM_FRAMES * 3; ++n) hits += pofv_ghost_tick(&g, slots, POFV_GHOST_MAX);
        CHECK(hits == 0 && g.live == 1);

        /* 成佛名额 cap：满了留在槽里、下一帧再报（不漏事件） */
        pofv_ghost_pool_init(&g);
        for (j = 0; j < POFV_GHOST_MAX; ++j) { pofv_ghost_register(&g, 700 + j, 0); pofv_ghost_calm(&g, j); }
        for (n = 0; n < POFV_GHOST_CALM_FRAMES; ++n) pofv_ghost_tick(&g, slots, 1);
        CHECK(g.live == POFV_GHOST_MAX - 1);                 /* 本帧只报 1 只 */
        hits = 0;
        for (n = 0; n < POFV_GHOST_MAX + 4; ++n) hits += pofv_ghost_tick(&g, slots, POFV_GHOST_MAX);
        CHECK(hits == POFV_GHOST_MAX - 1 && g.live == 0);    /* 剩下的全部报到 */

        pofv_ghost_register(&g, 800, 0);
        CHECK(pofv_ghost_clear(&g) == 1 && g.live == 0);
        CHECK(pofv_ghost_clear(0) == 0);
        pofv_ghost_pool_init(0);
        CHECK(pofv_ghost_register(0, 1, 0) == -1);
        CHECK(pofv_ghost_cycle_tick(0, 0, &kind) == 0);
        CHECK(pofv_ghost_tick(0, slots, 1) == 0 && pofv_ghost_release(0, 0) == 0);
    }

    printf("test_pofv_core: %d passed, %d failed\n", s_pass, s_fail);
    return s_fail ? 1 : 0;
}
