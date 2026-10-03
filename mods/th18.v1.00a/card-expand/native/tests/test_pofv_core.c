/* test_pofv_core.c —— pofv_core.c 的主机单测（`make test-host` 的第 20 条，跑法见 native/Makefile：
 *   cc -std=c11 -Wall -Wextra -Werror -o .../test_pofv_core tests/test_pofv_core.c cards/pofv_core.c
 * 语义来源（一手）：
 *   · 白弹阈值 T = 4.0：spawn spec review §5.4（`zBullet+0x658`；弹种表 `0x4c5f90` stride `0x118`）
 *   · 气槽 4 级 / CA 门槛与消耗（C1 0、C2 1、C3 2、C4 3）/ C1 无敌 8 帧 / QCA 清空：
 *     `mods/th18.v1.00a/card-expand/IDEAS.md` §「109 花映塚」
 *   · SP 三档 10 万 / 30 万 / 50 万：同上
 *   · 灵力道具 / 固灵结界 / 灵（真敌人）：`SPEC-109-FUSION.md` §8 + 一手 `engine/versus/th19/06-aura.md`
 *   · 红圈消掉的白弹**不回复气**：同上（「红色消弹圈」段）
 *   · 爆风队列 / 每帧预算：shared-enemy-death spec §4.1（死亡回调里不许扫弹池）
 * 设计文档：`docs/superpowers/specs/2026-09-12-pofv-109-design.md`
 * 覆盖：白弹筛选 / 蓄力状态机（按住 C 的电平）/ 气槽 4 级与消耗 / SP 阈值 /
 *       爆风与红圈的半径与计数调用点 / 爆风队列与帧预算 /
 *       **灵力道具池（口径 5：打包面值守恒 / 磁吸收取 / 寿命 / 池满）** /
 *       **固灵结界几何（一手 TH19 Reimu 圆 + pad 平方和并入）** /
 *       **灵句柄池（600 帧循环撒 / 四族轮转 / boss 门 / 同屏 4 / 幂等沈静化 / 成佛名额）**。
 * ⚠ 2026-10-01（口径 10）：**旧的「活化灵」自建池整条撤掉** ⇒ 它的断言（HP 减半 / 上浮 /
 *   自爆 3 颗 / Shift 领域吸收）已随之下线；那条 TH09 一手事实记在 `TUNING.md` §2.0v-1 #19。
 * ⚠ 2026-10-01（合成卡 owner 口径 2）：**拦 Z 已整条删除**（`pofv_charge_suppress` 不再存在）
 *   ⇒ 本文件不再有它的断言；一手 10 帧闸的事实留在核心注释与 `TUNING.md` §2.0v #1。 */
#include <stdio.h>
#include <string.h>
/* ★ 2026-10-01f（统筹修构建）：M 段（`pofv_atan2` 的黄金样本对账）用了 `fabsf`，但本文件
 *   只 include 了 stdio/string + `pofv_core.h`（后者只有 stdint.h）⇒ `-Werror`
 *   报「implicit declaration of fabsf」把整条 `make check` 弄红（不是本次文案改动引入的）。
 *   宿主单测允许用 libm（卡侧才禁 —— 见 `sdk.h` 里 `pofv_atan2` 的那段注释），所以补这一行。
 *   登记：`AUDIT.md` §BE 详录 #207。*/
#include <math.h>

#include "../cards/pofv_core.h"

static int s_fail, s_pass;
#define CHECK(cond) do { if (cond) ++s_pass; else { ++s_fail; printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } } while (0)

/* ★ 2026-10-01（owner：「动态这块按照季节点做就行」）：一手 TH16 的消弹道具出生时
 *   `item+0xc50 = 6` ⇒ **待发射 6 帧、这 6 帧里什么都不做**。所有「出生后应立刻动」的老断言
 *   都要先跳过这段延时 —— 这个 helper 就是干这个的（顺带把「延时里不动」本身检查一遍）。*/
static void power_wait_launch(pofv_power_pool_t *q, const float *pp, int32_t *pts)
{
    int k;
    for (k = 0; k < POFV_POWER_LAUNCH_DELAY; ++k) pofv_power_tick(q, pp, pts);
}

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
        /* ★★ 2026-10-01：改用一手 TH09「自机弹命中」节点（r0 = 0 / dr = 1.6 / 寿命 6）
         *    ⇒ 出生帧 age = 0 ⇒ 半径 = r₀ = **0**（旧版是一手 etama 的 16）。*/
        CHECK(POFV_STORM_R0 == 0.0f);
        CHECK(POFV_STORM_DR == 1.6f);                             /* ★ 一手 = 连锁前沿速度 */
        CHECK(pofv_storm_slot(&p, 0) != NULL);
        out = *pofv_storm_slot(&p, 0);
        pofv_storm_radius(&out, &r);
        CHECK(r == POFV_STORM_R0 && r == 0.0f);
        CHECK(pofv_storm_chain_at(&p, 0.0f, 0.0f) == 1);          /* r=0 时只有圆心自己在圈里 */
        CHECK(pofv_storm_chain_at(&p, 1000.0f, 0.0f) == 0);       /* 圈外 */

        /* 寿命按档给（`life = R_k / dr`）⇒ 半径**每帧只长 dr**，到寿命那一刻 = 该档终点半径 176 */
        {
            int32_t life1 = pofv_storm_life_for_chain(1);
            CHECK(life1 == (int32_t)(176.0f / POFV_STORM_DR + 0.5f));   /* = 110 帧 */
            CHECK(out.life == life1);
            for (i = 0; i < life1 - 1; ++i) {
                pofv_storm_tick(&p);
                out = *pofv_storm_slot(&p, 0);
                pofv_storm_radius(&out, &r);
                /* 前沿速度恒为 dr（档不改速度）—— 这就是 owner 说的「链跟着敌人走」*/
                CHECK(r == POFV_STORM_DR * (float)out.age || (r > 0.0f && r < 176.0f));
            }
            out = *pofv_storm_slot(&p, 0);
            pofv_storm_radius(&out, &r);
            CHECK(r > 170.0f && r <= 176.0f);                     /* 到寿命 ≈ 该档终点半径 */
        }
        while (p.live > 0) pofv_storm_tick(&p);                   /* 满寿命：释放 */
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
        int32_t prevlife = 0;
        for (i = 1; i <= POFV_BLAST_CHAIN_MAX; ++i) {
            pofv_storm_final_radius(i, &r);
            CHECK(r > prev);
            prev = r;
            /* ★ owner「连爆档只放大范围」：dr 恒定 ⇒ 档只把**寿命**拉长 */
            CHECK(pofv_storm_life_for_chain(i) > prevlife);
            prevlife = pofv_storm_life_for_chain(i);
        }
        CHECK(pofv_storm_life_for_chain(0) == pofv_storm_life_for_chain(1));      /* 越界钳 */
        CHECK(pofv_storm_life_for_chain(99) == pofv_storm_life_for_chain(POFV_BLAST_CHAIN_MAX));
        pofv_storm_final_radius(1, &r);
        CHECK(r == 176.0f);
    }

    /* 消弹的帧预算：**场模型下不再是「队列出队」，而是「每帧最多几次整池扫弹」**
     *   （`POFV_STORM_SCANS_PER_FRAME`）。这里只钉住这个预算数存在且 > 0 ——
     *   真正的闸在卡侧 `on_tick_2` 的 `scan_budget` 里。*/
    CHECK(POFV_STORM_SCANS_PER_FRAME > 0);
    CHECK(POFV_STORM_SCANS_PER_FRAME < POFV_STORM_MAX * pofv_storm_life_for_chain(1));
    CHECK(POFV_BLAST_CHAIN_MAX >= 2);

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
        power_wait_launch(&pool, pp, &pts);              /* 一手：待发射 6 帧 */
        CHECK(pofv_power_tick(&pool, pp, &pts) == 1);
        CHECK(pts == 30 && pool.live == 0 && pool.absorbed == 1);

        /* 磁吸：圈内（MAG_R 内、ABSORB_R 外）朝自机走 ⇒ 有限帧内必被收 */
        pofv_power_pool_init(&pool);
        pofv_power_spawn(&pool, 100.0f + POFV_POWER_BOX_HALF_W * 0.5f, 200.0f, 10, POFV_POWER_SRC_AURA);
        { float x0;
          power_wait_launch(&pool, pp, &pts);
          x0 = pool.s[0].x; pofv_power_tick(&pool, pp, &pts); CHECK(pool.s[0].x < x0); }
        n = 0;
        while (pool.live > 0 && n < 200) { pofv_power_tick(&pool, pp, &pts); ++n; }
        CHECK(pool.live == 0 && pts >= 10);

        /* 圈外：只飘落（y 增），不磁吸、不收取 */
        pofv_power_pool_init(&pool);
        /* ★ 2026-10-01：站位要**在场地内**（一手出界判据 |x| ≤ 200）—— 旧站位 `100 + MAG_R*4 = 324`
         *   一出生就被出界释放了；改成「刚出吸引盒、仍在场内」。*/
        pofv_power_spawn(&pool, 100.0f + (POFV_POWER_BOX_HALF_W + 20.0f), 200.0f, 10, POFV_POWER_SRC_AURA);
        {
            float x0, y0;
            /* ★ 2026-10-01：出生先**待发射 6 帧**、再飞散（方向随机）⇒ 只断言「动了」，
             *   不再断言「一定往下」—— 旧断言是「圈外只飘落」，那是**改造前**的动态。*/
            power_wait_launch(&pool, pp, &pts);
            x0 = pool.s[0].x; y0 = pool.s[0].y;
            pofv_power_tick(&pool, pp, &pts);
            CHECK(pool.s[0].x != x0 || pool.s[0].y != y0);
            CHECK(pts == 0 && pool.live == 1);
        }

        /* 没有自机（过场 / 单测）：只飘落，不磁吸不收取 */
        pofv_power_pool_init(&pool);
        pofv_power_spawn(&pool, 0.0f, 0.0f, 10, POFV_POWER_SRC_AURA);
        /* 同上：出生那几帧是**飞散**（待发射 + 直线飞散），断言「动了」而不是「往下」*/
        {
            float x0, y0;
            power_wait_launch(&pool, 0, &pts);
            x0 = pool.s[0].x; y0 = pool.s[0].y;
            CHECK(pofv_power_tick(&pool, 0, &pts) == 0 && pts == 0);
            CHECK(pool.s[0].x != x0 || pool.s[0].y != y0);
        }
        /* ★ 新动态：**待发射**的错帧里不动（`delay = seq & 3`）；第 2 颗起才有延时 */
        {
            pofv_power_pool_t q;
            pofv_power_pool_init(&q);
            CHECK(q.seq == 0);
            /* ★ 2026-10-01（owner：「动态这块按照季节点做就行」）：待发射 = **统一 6 帧**
             *   （一手 TH16 消弹池出生 `item+0xc50 = 6`）—— 不再是旧的 `& 3` 错帧。*/
            pofv_power_spawn(&q, 0.0f, 0.0f, 1, POFV_POWER_SRC_AURA);
            CHECK(q.s[0].delay == POFV_POWER_LAUNCH_DELAY);
            CHECK(q.s[0].st == 0);                    /* 出生在「飞散」态 */
            pofv_power_spawn(&q, 0.0f, 0.0f, 1, POFV_POWER_SRC_AURA);
            CHECK(q.s[1].delay == POFV_POWER_LAUNCH_DELAY);
            /* ★ 一手初速 = `rand×1.9 + 0.2` ⇒ 必须落在 0.2..2.1；方向 = 64 向表的单位向量 */
            {
                int k; int seen_lo = 0, seen_hi = 0;
                for (k = 0; k < 64; ++k) {
                    float sp = q.s[k < 2 ? k : 0].lr;   /* 只看已生成的这两颗 */
                    (void)sp;
                }
                (void)seen_lo; (void)seen_hi;
                CHECK(q.s[0].lr >= POFV_POWER_SPEED_MIN - 1e-6f
                      && q.s[0].lr <= POFV_POWER_SPEED_MIN + POFV_POWER_SPEED_SCALE + 1e-6f);
                CHECK(q.s[1].lr >= POFV_POWER_SPEED_MIN - 1e-6f
                      && q.s[1].lr <= POFV_POWER_SPEED_MIN + POFV_POWER_SPEED_SCALE + 1e-6f);
                { float L = q.s[0].dx * q.s[0].dx + q.s[0].dy * q.s[0].dy;
                  CHECK(L > 0.98f && L < 1.02f); }     /* 方向是单位向量 */
            }
            /* 待发射那一帧不动 */
            { float x0 = q.s[1].x, y0 = q.s[1].y;
              pofv_power_tick(&q, 0, &pts);
              CHECK(q.s[1].x == x0 && q.s[1].y == y0); }   /* delay 6 → 本帧还在等 */
            /* 飞散：剩余速度每帧 −0.03（一手 TH16 `item+0xc20`）——先过待发射 */
            { float l0;
              power_wait_launch(&q, 0, &pts);
              l0 = q.s[0].lr;
              pofv_power_tick(&q, 0, &pts);
              CHECK(q.s[0].lr < l0);
              { float d = l0 - q.s[0].lr;
                CHECK(d > POFV_POWER_DECEL - 1e-5f && d < POFV_POWER_DECEL + 1e-5f); } }
        }

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

    /* ---- H.1 ★★ 2026-10-01 owner 追加口径：**结界按自机角色取不同游戏的版本** ----
     *   「固灵结界 **咲夜取 th09，早苗取 th19**，灵梦和魔理沙**待定**」＋
     *   「（早苗的半径）**把 shift 时长当作某种蓄力**」。 */
    {
        pofv_aura_profile_t pr;
        float r = 0.0f;

        /* 咲夜（TH18 角色号 2）→ TH09 的「消弹圈 = 锚定场」（一手「CA 释放」节点 16 / 20 / 8） */
        pofv_aura_profile_for(2, &pr);
        CHECK(pr.mode == POFV_AURA_FIELD);
        CHECK(pr.r0 == POFV_AURA_TH09_R0 && pr.r0 == 16.0f);      /* ★ 一手 th09:0x41e875 */
        CHECK(pr.dr == POFV_AURA_TH09_DR && pr.dr == 20.0f);      /* ★ 一手 */
        CHECK(pr.life == POFV_AURA_TH09_LIFE && pr.life == 8);    /* ★ 一手；终点 16+20×8 = 176 */
        pofv_aura_radius(&pr, 0, 0, &r);
        CHECK(r == 16.0f);                                        /* 出生帧 = r₀ */
        pofv_aura_radius(&pr, 4, 0, &r);
        CHECK(r == 96.0f);                                        /* 逐帧长 */
        pofv_aura_radius(&pr, 8, 0, &r);
        CHECK(r == 176.0f);                                       /* 寿命到点 = 终点 */
        pofv_aura_radius(&pr, 999, 0, &r);
        CHECK(r == 176.0f);                                       /* 过寿命钳住（不无限长）*/
        pofv_aura_radius(&pr, -5, 0, &r);
        CHECK(r == 16.0f);                                        /* 负 age 钳到 0 */
        CHECK(pr.pad_linear == 0);                                /* 场形态沿用 Reimu 支几何 */

        /* 早苗（TH18 角色号 3）→ TH19 `AuraSanaeInf`：圆 + `<= (r+pad)²` + K = 128 */
        pofv_aura_profile_for(3, &pr);
        CHECK(pr.mode == POFV_AURA_STATIC);
        CHECK(pr.pad_linear == 1);                                /* ★ 一手 §7.3 的第二支 */
        CHECK(pr.k == POFV_AURA_SANAE_K && pr.k == 128.0f);       /* ★ 一手 §7.2 */
        /* ★ 「同一个东西」：早苗的半径也是**按 px/帧 长**（`dr = 满档 256 / 60 帧 ≈ 4.267`），
         *   只是**长到蓄力给的上限就停** —— 不再瞬间跳到目标值。 */
        CHECK(pr.grow_frames == POFV_AURA_SANAE_GROW_FRAMES && pr.grow_frames == 60);   /* 一手 +0x94 */
        CHECK(pr.dr > 4.26f && pr.dr < 4.28f);                    /* 256 / 60 */
        pofv_aura_radius(&pr, 0, 0, &r);
        CHECK(r == 0.0f);                                         /* 没按 Shift ⇒ 没有结界 */
        pofv_aura_radius(&pr, 0, 200, &r);
        CHECK(r == 0.0f);                                         /* 起手那一帧还是 0（要长）*/
        pofv_aura_radius(&pr, 15, 200, &r);
        CHECK(r > 63.9f && r <= 64.0f);                           /* 长了 15 帧 ⇒ ≈64 */
        pofv_aura_radius(&pr, 60, 200, &r);
        CHECK(r > 255.9f && r <= 256.0f);                         /* 60 帧到满档 ⇒ 256 (K=128) */
        pofv_aura_radius(&pr, 30, 100, &r);
        CHECK(r > 127.9f && r <= 128.0f);                         /* 半蓄力（上限 128）30 帧就够到 */
        pofv_aura_radius(&pr, 999, 200, &r);
        CHECK(r > 255.9f && r <= 256.0f);                         /* 过了上限就停住（不无限长）*/
        pofv_aura_radius(&pr, 60, 9999, &r);
        CHECK(r > 255.9f && r <= 256.0f);                         /* 蓄力钳到 200 */
        pofv_aura_radius(&pr, 60, -5, &r);
        CHECK(r == 0.0f);                                         /* 负蓄力钳到 0 */

        /* 「Shift 时长 → 蓄力」的换算（❓ 自定：每帧 2） */
        CHECK(pofv_aura_shift_charge(0) == 0);
        CHECK(pofv_aura_shift_charge(-3) == 0);
        CHECK(pofv_aura_shift_charge(1) == POFV_AURA_SHIFT_CHARGE_PER_FRAME);
        CHECK(pofv_aura_shift_charge(50) == 100);
        CHECK(pofv_aura_shift_charge(100) == POFV_AURA_CHARGE_MAX);      /* 100 帧涨满 */
        CHECK(pofv_aura_shift_charge(100000) == POFV_AURA_CHARGE_MAX);   /* 钳住 */
        /* 端到端：按住 Shift 100 帧 ⇒ 早苗的结界长到 256 */
        pofv_aura_profile_for(3, &pr);
        pofv_aura_radius(&pr, 60, pofv_aura_shift_charge(100), &r);
        CHECK(r > 255.9f && r <= 256.0f);

        /* 灵梦（0）/ 魔理沙（1）→ ❓ owner 待定 ⇒ 占位 = 常驻圆 96（= 改造前行为）*/
        pofv_aura_profile_for(0, &pr);
        CHECK(pr.mode == POFV_AURA_STATIC && pr.pad_linear == 0);
        CHECK(pr.r_fixed == POFV_AURA_R && pr.k == 0.0f);
        pofv_aura_radius(&pr, 0, 200, &r);
        CHECK(r == POFV_AURA_R);                                  /* 不随蓄力变 */
        pofv_aura_profile_for(1, &pr);
        CHECK(pr.r_fixed == POFV_AURA_R);
        pofv_aura_profile_for(99, &pr);                           /* 未识别 ⇒ 同一档 */
        CHECK(pr.mode == POFV_AURA_STATIC && pr.r_fixed == POFV_AURA_R);

        /* 两支几何：同参数下 Sanae 支（线性并 pad）比 Reimu 支（平方和并 pad）命中得远 */
        CHECK(pofv_aura_hit_ex(110.0f, 0.0f, 96.0f, 20.0f, 0) == 0);   /* Reimu：96²+20² = 9616 < 110² */
        CHECK(pofv_aura_hit_ex(110.0f, 0.0f, 96.0f, 20.0f, 1) == 1);   /* Sanae：(96+20)² = 13456 > 110² */
        CHECK(pofv_aura_hit_ex(115.9f, 0.0f, 96.0f, 20.0f, 1) == 1);
        CHECK(pofv_aura_hit_ex(116.1f, 0.0f, 96.0f, 20.0f, 1) == 0);
        CHECK(pofv_aura_hit_ex(116.0f, 0.0f, 96.0f, 20.0f, 1) == 1);   /* Sanae 支是 `<=` */
        CHECK(pofv_aura_hit_ex(0.0f, 0.0f, 0.0f, 0.0f, 1) == 0);       /* r ≤ 0 ⇒ 永不命中 */
        CHECK(pofv_aura_hit_ex(0.0f, 0.0f, -1.0f, 0.0f, 1) == 0);
        /* 旧签名 = Reimu 支的包装（改造前就有；钉住不回归） */
        CHECK(pofv_aura_hit(98.0f, 0.0f, 96.0f, 20.0f) == pofv_aura_hit_ex(98.0f, 0.0f, 96.0f, 20.0f, 0));

        /* NULL 安全 */
        pofv_aura_profile_for(3, 0);
        pofv_aura_radius(0, 0, 0, &r);
        CHECK(r == 0.0f);
        pofv_aura_radius(&pr, 0, 0, 0);
    }

    /* ---- H.2 ★★ 2026-10-01 owner 第四 / 第五条：**暴风也按自机角色取哪一边** ----
     *   「th09 暴风**可以直接打死 th09 幽灵**」「不过似乎 th19 **没有暴风连锁，只是暴风消弹**」 */
    {
        pofv_storm_profile_t sp;

        pofv_storm_profile_for(2, &sp);        /* 咲夜 → TH09：连爆 + 能直接打死幽灵 */
        CHECK(sp.chain == 1 && sp.kills_ghosts == 1);

        pofv_storm_profile_for(3, &sp);        /* 早苗 → TH19：**不连锁、只消弹**（不给敌人伤害）*/
        CHECK(sp.chain == 0 && sp.kills_ghosts == 0);

        pofv_storm_profile_for(0, &sp);        /* 灵梦 / 魔理沙 → ❓ 待定 ⇒ 占位 = TH09 档 */
        CHECK(sp.chain == 1 && sp.kills_ghosts == 1);
        pofv_storm_profile_for(1, &sp);
        CHECK(sp.chain == 1 && sp.kills_ghosts == 1);
        pofv_storm_profile_for(99, &sp);       /* 未识别 ⇒ 同一档 */
        CHECK(sp.chain == 1 && sp.kills_ghosts == 1);

        pofv_storm_profile_for(3, 0);          /* NULL 安全 */
    }

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

    /* ==================== J. オートパリア（抵挡护盾；owner 2026-10-01 甜头）====================
     * ★ 一手 TH19：バリア由 `FUN_0040b420` 从「破壊中」复活，而它的**唯二调用点 = 阶段 3 / 阶段 4**
     *   ⇒ 「蓄力到 C3 / C4 发动就给盾」；`FUN_00544c90` 的 `if (*p != 1) return 0` ⇒ **挡 1 发**。 */
    {
        pofv_state_t sh;

        CHECK(POFV_SHIELD_ARM_LEVEL == 3);                        /* ★ 一手：阶段 3 / 4 */
        CHECK(POFV_SHIELD_INVULN_FRAMES > 0);                     /* ❓ 自定（与 107/117 同档 40）*/

        pofv_state_init(&sh);
        CHECK(pofv_shield_up(&sh) == 0);                          /* 开局没有盾 */
        CHECK(pofv_shield_arm(&sh, 0) == 0);                      /* 没蓄力 */
        CHECK(pofv_shield_arm(&sh, 1) == 0);                      /* C1 不给（一手：阶段 1/2 不回复）*/
        CHECK(pofv_shield_arm(&sh, 2) == 0);                      /* C2 不给 */
        CHECK(pofv_shield_up(&sh) == 0);
        CHECK(pofv_shield_arm(&sh, 3) == 1);                      /* ★ C3 发动 ⇒ 架上 */
        CHECK(pofv_shield_up(&sh) == 1);
        CHECK(pofv_shield_arm(&sh, 3) == 0);                      /* 已有盾：不重复、也不刷新 */
        CHECK(pofv_shield_arm(&sh, 4) == 0);
        CHECK(pofv_shield_up(&sh) == 1);

        CHECK(pofv_shield_take(&sh) == 1);                        /* ★ 挡 1 发 */
        CHECK(pofv_shield_up(&sh) == 0);
        CHECK(pofv_shield_take(&sh) == 0);                        /* 第二发直接放行（一手 `*p != 1`）*/

        CHECK(pofv_shield_arm(&sh, 4) == 1);                      /* 破了以后 C4 发动 ⇒ 再架上 */
        CHECK(pofv_shield_up(&sh) == 1);
        pofv_state_init(&sh);                                     /* 换关 / 重开：清掉 */
        CHECK(pofv_shield_up(&sh) == 0);
        CHECK(pofv_shield_take(&sh) == 0);                        /* 没盾：吃不掉 */

        CHECK(pofv_shield_arm(0, 4) == 0);                        /* NULL 安全 */
        CHECK(pofv_shield_take(0) == 0);
        CHECK(pofv_shield_up(0) == 0);
    }

    /* ==================== K. ボスアタック（C3/C4 的「送到对面的攻击」；owner 2026-10-01）========
     * ★ 一手 TH09 `0x41f999`：门槛 = 蓄力量 ≥400 且场上没有 kind-3 boss；
     *   `0x403cf0` = CA L3 的**不断扩张**的消弹圈（0 / 3.5 / 128 → 448）。 */
    {
        pofv_bossatk_t ba;

        CHECK(POFV_BOSSATK_MIN_LEVEL == 1);        /* ★ owner：C1 起就有（打自己场上的敌人）*/
        CHECK(POFV_BOSSATK_C1_FRAMES < POFV_BOSSATK_C2_FRAMES);
        CHECK(POFV_BOSSATK_C2_FRAMES < POFV_BOSSATK_C3_FRAMES);
        CHECK(POFV_BOSSATK_C3_FRAMES < POFV_BOSSATK_C4_FRAMES);   /* 档越高打得越久 */

        memset(&ba, 0, sizeof ba);
        CHECK(pofv_bossatk_is_boss(&ba) == 0);
        CHECK(pofv_bossatk_tick(&ba) == 0);        /* 没起手 ⇒ 不打 */

        CHECK(pofv_bossatk_start(&ba, 1) == 1);    /* ★ C1 起手 */
        CHECK(ba.active == 1 && ba.level == 1 && ba.left == POFV_BOSSATK_C1_FRAMES);
        CHECK(pofv_bossatk_is_boss(&ba) == 0);
        pofv_bossatk_stop(&ba);
        CHECK(pofv_bossatk_start(&ba, 2) == 1);    /* ★ C2 起手 */
        CHECK(ba.level == 2 && ba.left == POFV_BOSSATK_C2_FRAMES);
        CHECK(pofv_bossatk_is_boss(&ba) == 0);
        pofv_bossatk_stop(&ba);

        CHECK(pofv_bossatk_start(&ba, 3) == 1);    /* ★ C3 起手 */
        CHECK(ba.active == 1 && ba.level == 3 && ba.left == POFV_BOSSATK_C3_FRAMES);
        CHECK(pofv_bossatk_is_boss(&ba) == 0);     /* ★ C3「自己不是 boss」*/
        {   int i, n = 0;
            for (i = 0; i < POFV_BOSSATK_C3_FRAMES + 5; ++i) n += pofv_bossatk_tick(&ba);
            CHECK(n == POFV_BOSSATK_C3_FRAMES);    /* 正好打这么多帧 */
        }
        CHECK(ba.active == 0 && pofv_bossatk_tick(&ba) == 0);

        CHECK(pofv_bossatk_start(&ba, 4) == 1);    /* ★ C4 起手 */
        CHECK(ba.level == 4 && ba.left == POFV_BOSSATK_C4_FRAMES);
        CHECK(pofv_bossatk_is_boss(&ba) == 1);     /* ★ C4 = 自己是 boss */
        pofv_bossatk_stop(&ba);                    /* 删卡 / 换关：收手 */
        CHECK(ba.active == 0 && ba.left == 0 && ba.level == 0);
        CHECK(pofv_bossatk_is_boss(&ba) == 0);
        CHECK(pofv_bossatk_start(&ba, 99) == 1 && ba.level == 4);   /* 越界钳到 4 */

        CHECK(pofv_bossatk_start(0, 4) == 0);      /* NULL 安全 */
        CHECK(pofv_bossatk_tick(0) == 0);
        pofv_bossatk_stop(0);
        CHECK(pofv_bossatk_is_boss(0) == 0);
    }

    /* ============ L. 蓄力攻击的消弹圈 = **逐帧扩张的场**（owner 2026-10-01：「消弹圈是逐帧扩张的场」）====
     * ★ 一手：L1 `0x403c30` = 0 / 4.0 / 48 → 192；L2 `0x403c90` = 0 / 4.0 / 64 → 256；
     *   L3 `0x403cf0` = 0 / 3.5 / 128 → 448；「松手那一下」`0x41e875` = **16 / 20 / 8 → 176**。 */
    {
        pofv_storm_pool_t pool;
        const pofv_storm_t *f;
        float r0, dr, r;
        int32_t life;
        int i, slot;

        pofv_ca_field_params(1, &r0, &dr, &life);
        CHECK(r0 == 0.0f && dr == 4.0f && life == 48);           /* ★ 一手 L1 */
        pofv_ca_field_params(2, &r0, &dr, &life);
        CHECK(r0 == 0.0f && dr == 4.0f && life == 64);           /* ★ 一手 L2 */
        pofv_ca_field_params(3, &r0, &dr, &life);
        CHECK(r0 == 0.0f && dr == 3.5f && life == 128);          /* ★ 一手 L3 */
        pofv_ca_field_params(4, &r0, &dr, &life);
        CHECK(life == 128);                                      /* ★ L≥3 同一支 */
        pofv_ca_field_params(0, &r0, &dr, &life);
        CHECK(life == 48);                                       /* 越界按 L1 */
        CHECK(POFV_CA_BURST_R0 == 16.0f && POFV_CA_BURST_DR == 20.0f && POFV_CA_BURST_LIFE == 8);

        pofv_storm_pool_init(&pool);
        slot = pofv_ca_field_start(&pool, 10.0f, 20.0f, 1);
        CHECK(slot >= 0 && pool.live == 1);
        f = pofv_storm_slot(&pool, slot);
        CHECK(f && f->kind == POFV_STORM_KIND_CA);
        CHECK(pofv_storm_is_storm(f) == 0);                      /* ★ CA 场**不是**暴风 */
        pofv_storm_radius(f, &r);
        CHECK(r == 0.0f);                                        /* 出生帧 = r0 = 0 */
        /* 逐帧扩张：每帧 +dr（**不是**一次性按终点半径）*/
        for (i = 1; i <= 10; ++i) {
            pofv_storm_tick(&pool);
            f = pofv_storm_slot(&pool, slot);
            pofv_storm_radius(f, &r);
            CHECK(r == 4.0f * (float)i);
        }
        /* 打满寿命：终点 = 4.0 × 48 = 192（★ 一手 L1 的终点）*/
        while (pool.live > 0) pofv_storm_tick(&pool);
        CHECK(pool.live == 0);

        /* 「松手那一下」（level 0）：16 / 20 / 8 → 176，**任何一次松手都起** */
        slot = pofv_ca_field_start(&pool, 0.0f, 0.0f, 0);
        f = pofv_storm_slot(&pool, slot);
        pofv_storm_radius(f, &r);
        CHECK(r == POFV_CA_BURST_R0);                            /* 出生就是 16（不是 0）*/
        for (i = 0; i < POFV_CA_BURST_LIFE; ++i) pofv_storm_tick(&pool);
        CHECK(pool.live == 0);

        /* ★ CA 场**不参与连爆**：盖着点也不算档 */
        slot = pofv_ca_field_start(&pool, 100.0f, 100.0f, 3);
        CHECK(slot >= 0);
        for (i = 0; i < 200; ++i) pofv_storm_tick(&pool);        /* 长到 448 以上 */
        CHECK(pofv_storm_chain_at(&pool, 100.0f, 100.0f) == 0);
        while (pool.live > 0) pofv_storm_tick(&pool);

        /* 暴风仍是暴风（kind 默认 + 参与连爆）*/
        CHECK(pofv_storm_spawn(&pool, 5.0f, 5.0f, 1) == 0);
        f = pofv_storm_slot(&pool, 0);
        CHECK(pofv_storm_is_storm(f) == 1);
        CHECK(f->r0 == POFV_STORM_R0 && f->dr == POFV_STORM_DR);
        CHECK(pofv_storm_chain_at(&pool, 5.0f, 5.0f) == 1);
        pofv_storm_pool_init(&pool);
        CHECK(pofv_ca_field_start(&pool, 0.0f, 0.0f, 3) == 0);   /* 空池能起 */
        pofv_storm_pool_init(&pool);
        CHECK(pofv_ca_field_start(0, 0.0f, 0.0f, 1) == -1);      /* NULL 安全 */
        pofv_storm_spawn_field(0, 0.0f, 0.0f, 0.0f, 1.0f, 1, 0);
        pofv_storm_is_storm(0);
        pofv_ca_field_params(1, 0, 0, 0);
    }

    
    /* ================= M. 方位角（自机狙用）=================
     * ❓ 自定（数学近似）：`pofv_atan2` 是纯 SSE 的多项式近似（见 `pofv_core.c`）。
     * 这里**拿 libm 的 `atan2` 对账**（主机单测允许用 libm；卡侧才是禁的）——
     * 扫 0..2π 全周 + 四象限边界，断言误差 < 1e-4 rad。*/
    /* ==================== M. 方位角 pofv_atan2（自机狙用；纯 SSE 近似）==================== */
    {
        /* ⚠ 本文件**不引 libm**（host 编译行没有 `-lm`）⇒ 用**黄金样本对账**：
         *   每个角度给一对 (y, x) 与**期望角**（值来自 `tools/` 之外的常识角 / 手算），
         *   覆盖四个象限 + 轴 + 非对角线（`atan(1/2)` / `atan(2)`）。*/
        static const struct { float y, x, want; } g[] = {
            {  1.0f,  1.0f,  0.7853982f },   /* 45°   */
            {  1.0f, -1.0f,  2.3561945f },   /* 135°  */
            { -1.0f, -1.0f, -2.3561945f },   /* −135° */
            { -1.0f,  1.0f, -0.7853982f },   /* −45°  */
            {  1.0f,  2.0f,  0.4636476f },   /* atan(1/2) */
            {  2.0f,  1.0f,  1.1071487f },   /* atan(2)   */
            {  0.1f,  1.0f,  0.0996687f },   /* atan(0.1) */
            {  1.0f,  0.1f,  1.4711277f },   /* atan(10)  */
            {  3.0f,  4.0f,  0.6435011f },   /* 3-4-5 三角 */
            { -3.0f,  4.0f, -0.6435011f },
            {  3.0f, -4.0f,  2.4980915f },
            { -3.0f, -4.0f, -2.4980915f },
        };
        size_t gi;
        float worst = 0.0f;
        for (gi = 0; gi < sizeof g / sizeof g[0]; ++gi) {
            float got = 0.0f, err;
            pofv_atan2(g[gi].y, g[gi].x, &got);
            err = got - g[gi].want; if (err < 0.0f) err = -err;
            if (err > worst) worst = err;
            CHECK(err < 1e-4f);
        }
        printf("    pofv_atan2 黄金样本最大误差 = %.2e rad\n", (double)worst);
    }
    {
        float r = 123.0f;
        pofv_atan2(0.0f, 0.0f, &r);
        CHECK(r == 0.0f);                          /* 原点：定 0 */
        pofv_atan2(1.0f, 0.0f, &r);  CHECK(r - 1.5707964f < 1e-4f && 1.5707964f - r < 1e-4f);   /* 正上 = +π/2 */
        pofv_atan2(-1.0f, 0.0f, &r); CHECK(r + 1.5707964f < 1e-4f && -1.5707964f - r < 1e-4f);   /* 正下 = −π/2 */
        pofv_atan2(0.0f, -1.0f, &r); CHECK(r > 3.1415927f - 1e-4f || r < -3.1415927f + 1e-4f); /* 正左 = ±π */
        pofv_atan2(0.0f, 1.0f, &r);  CHECK(r == 0.0f);                       /* 正右 = 0 */
        pofv_atan2(1.0f, 1.0f, &r);  CHECK(r - 0.7853982f < 1e-4f && 0.7853982f - r < 1e-4f);   /* 45° */
        pofv_atan2(1.0f, -1.0f, &r); CHECK(r - 2.3561945f < 1e-4f && 2.3561945f - r < 1e-4f);   /* 135° */
        pofv_atan2(-1.0f, -1.0f, &r);CHECK(r + 2.3561945f < 1e-4f && -2.3561945f - r < 1e-4f);   /* −135° */
        pofv_atan2(1.0f, 2.0f, &r);  CHECK(r - 0.4636476f < 1e-4f && 0.4636476f - r < 1e-4f);   /* 非对角：atan(1/2) */
        pofv_atan2(2.0f, 1.0f, &r);  CHECK(r - 1.1071487f < 1e-4f && 1.1071487f - r < 1e-4f);   /* 非对角：atan(2) */
        pofv_atan2(1.0f, 1.0f, 0);   /* NULL 不许崩 */
    }

    
    /* ==================== P. 击中数 hits（TH19 的 hits UI）====================
     * 一手：① `ce_bullet_hit`（`0x45f466`）=「一发自机弹 × 一个敌机」的命中；
     *       ② `engine/versus/th19/04-manual-spec.md` 第 7 行「combo 满 100 → kind 9」（霊力アイテム）。*/
    {
        pofv_state_t st;
        int k, drops = 0;
        pofv_state_init(&st);
        CHECK(st.hits == 0 && st.hits_drops == 0);
        CHECK(pofv_hits_add(&st, 0) == 0);                    /* 0 不加 */
        CHECK(pofv_hits_add(&st, -5) == 0);                   /* 负数忽略 */
        CHECK(pofv_hits_add(&st, 1) == 0 && st.hits == 1);
        /* ★ 打满 100 次命中 ⇒ **正好掉一颗**（这就是那个 UI 的意义：看得见攒到多少）*/
        for (k = 1; k < POFV_HITS_PER_DROP; ++k) drops += pofv_hits_add(&st, 1);
        CHECK(drops == 1);
        CHECK(st.hits == POFV_HITS_PER_DROP);
        CHECK(st.hits_drops == 1);
        /* 累计：再 100 次 ⇒ 第二颗；hits **不清零**（TH19 那个计数是一直往上加的）*/
        drops = 0;
        for (k = 0; k < POFV_HITS_PER_DROP; ++k) drops += pofv_hits_add(&st, 1);
        CHECK(drops == 1 && st.hits == 2 * POFV_HITS_PER_DROP && st.hits_drops == 2);
        /* 一次跨两格（某个大伤害事件一次算 250）⇒ 掉两颗 */
        CHECK(pofv_hits_add(&st, 250) == 2);
        CHECK(st.hits_drops == 4);
    }

    /* ==================== O. 灵力道具的**下落动态**（一手 TH16 季节道具）====================
     * owner 实跑：「**下落动态不对**」＋「动态这块按照季节点做就行」⇒ 逐条对一手：
     * 重力 0.03/`vy` 上限 2.0/`vy≥0 ⇒ 只动 y`；回收线 128 ⇒ 速度 6.0；吸引矩形 ⇒ 起步 2.0、+0.2、上限 12。*/
    {
        pofv_power_pool_t q;
        pofv_power_t *it;
        float pp2[2] = { 0.0f, 400.0f };              /* 自机在下方很远 ⇒ 不触发回收线/吸引 */
        int32_t pts = 0, k;
        pofv_power_pool_init(&q);
        pofv_power_spawn(&q, 0.0f, 0.0f, 10, POFV_POWER_SRC_KILL);
        it = &q.s[0];
        /* 待发射 6 帧 */
        power_wait_launch(&q, pp2, &pts);
        CHECK(it->delay == 0);
        /* 飞散期：速度每帧 −0.03、**竖直速度 vy 保持 0（无重力）** */
        CHECK(it->st == 0 && it->vy == 0.0f);
        pofv_power_tick(&q, pp2, &pts);
        CHECK(it->vy == 0.0f);
        /* 飞到速度耗尽 ⇒ 转「飘落」 */
        for (k = 0; k < 200 && it->st == 0; ++k) pofv_power_tick(&q, pp2, &pts);
        CHECK(it->st == 1);
        /* ★★ 一手飘落：**每帧 vy += 0.03**、**上限 2.0**、**只有 y 在动** */
        {
            float x0 = it->x, v0 = it->vy;
            pofv_power_tick(&q, pp2, &pts);
            CHECK(it->x == x0);                                  /* `vy ≥ 0 ⇒ vx = 0` */
            CHECK(it->vy > v0);
            CHECK(it->vy < v0 + POFV_POWER_GRAVITY + 1e-5f);
        }
        for (k = 0; k < 400; ++k) pofv_power_tick(&q, pp2, &pts);
        CHECK(it->vy <= POFV_POWER_FALL_MAX + 1e-6f);             /* 封顶 2.0 */

        /* 过回收线（一手：`player.y < 128`）⇒ 朝自机飞、速度 = `sht+8` = 6.0 */
        pofv_power_pool_init(&q);
        pofv_power_spawn(&q, 0.0f, 300.0f, 10, POFV_POWER_SRC_KILL);
        { float pu[2] = { 0.0f, 100.0f };                        /* y=100 < 128 ⇒ 回收线内 */
          power_wait_launch(&q, pu, &pts);
          pofv_power_tick(&q, pu, &pts);
          CHECK(q.s[0].st == 2);
          { float y0 = q.s[0].y;
            pofv_power_tick(&q, pu, &pts);
            CHECK(q.s[0].y < y0); } }                            /* 朝自机（上）走 */

        /* 吸引矩形（一手 `item_attract_box_*`）⇒ 起步 2.0、每帧 +0.2、上限 12.0 */
        pofv_power_pool_init(&q);
        pofv_power_spawn(&q, POFV_POWER_BOX_HALF_W * 0.5f, 300.0f, 10, POFV_POWER_SRC_KILL);
        { float pu[2] = { 0.0f, 300.0f };                        /* 同高、横向在盒内 ⇒ state 5 */
          power_wait_launch(&q, pu, &pts);
          pofv_power_tick(&q, pu, &pts);
          CHECK(q.s[0].st == 3);
          CHECK(q.s[0].mspd >= POFV_POWER_MAG_SPEED0 - 1e-6f);
          { float m0 = q.s[0].mspd;
            pofv_power_tick(&q, pu, &pts);
            CHECK(q.s[0].mspd > m0); }                           /* 每帧 +0.2 */
          for (k = 0; k < 200; ++k) pofv_power_tick(&q, pu, &pts);
          CHECK(q.s[0].mspd <= POFV_POWER_MAG_SPEED_MAX + 1e-6f); }

        /* 出界（一手：y 472 / |x| 200）⇒ 释放而不是等到寿命 */
        pofv_power_pool_init(&q);
        pofv_power_spawn(&q, 0.0f, POFV_POWER_OUT_Y - 1.0f, 10, POFV_POWER_SRC_KILL);
        /* ⚠ 自机必须**远离**（否则会被吸引盒抓住、朝上飞走而不是坠出界）：
         *   y = 200 ⇒ 既 ≥ 回收线 128，又离道具 (471) 有 271 px > 盒半高 120 ✓ */
        { float far[2] = { 0.0f, 200.0f };
          for (k = 0; k < 400 && q.live > 0; ++k) pofv_power_tick(&q, far, &pts); }
        CHECK(q.live == 0);
        CHECK(q.expired >= 1);
    }

    /* ==================== N. 逐弹归属（C3/C4 的自伤源用）====================
     * 集合逐条来自 `assets/ecl/pofv109_boss.ecl.txt` 的 `etSprite` 调用。*/
    {
        static const uint32_t ours[] = { 0u, 1u, 2u, 11u, 12u, 13u, 18u, 20u };
        static const uint32_t theirs[] = { 3u, 4u, 5u, 6u, 10u, 14u, 19u, 21u, 255u, 0xffffffffu };
        size_t k;
        for (k = 0; k < sizeof ours / sizeof ours[0]; ++k)
            CHECK(pofv_bossatk_bullet_type_ours(ours[k]) == 1);
        for (k = 0; k < sizeof theirs / sizeof theirs[0]; ++k)
            CHECK(pofv_bossatk_bullet_type_ours(theirs[k]) == 0);
    }

    printf("test_pofv_core: %d passed, %d failed\n", s_pass, s_fail);
    return s_fail ? 1 : 0;
}
