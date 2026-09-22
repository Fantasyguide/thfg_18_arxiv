/* test_trance_core.c —— `cards/trance_core.c` 的主机单测（口径与 `make test-host` 相同：
 *   cc -std=c11 -Wall -Wextra -Werror -o /tmp/t113 tests/test_trance_core.c cards/trance_core.c && /tmp/t113）。
 *
 * 覆盖（用户点名的部分加粗）：
 *   · **灵界槽**：上限 600 / 每 200 一格 / 收神灵 +2 / type3 再 +1 / 灵界中 −1 帧 / 非死亡结束 −200
 *     / 夹取 / 跨格反馈；
 *   · **灵界窗口状态机**：相位 0/1/2、`begin` 的两道门（已在灵界 = 幂等成功、槽 < 200 = 失败）、
 *     `end` 只在**非死亡**路上补扣 200、−1/帧 到 0 收尾；
 *   · **激活条件全 8 条**（逐条单点放开；`auto_mode` 兜底；第 8 条 = `(GAME_FLAGS & 0x30) != 0x20`）；
 *   · **死亡强制**（`trance_force_on_death`）：不消耗生命、不碰死亡流程、槽不足时不开；
 *   · **蓝白灵 type 0–3 的映射与计数**（不按「蓝 / 白」二分）+ 奖励**普通 / 灵界**两套值；
 *   · **奖励倍率**落到 `max_point` 的**单位换算**（3 种单位参数 × 0/负/越界）；
 *   · **连锁**：60 帧窗口、**第 10 颗触发**（一手 `+0x8838++ ; if (+0x8838 > 9)`）、type 3/4 不推进、窗口归零清计数；
 *   · **救援层记账**：顺序 **107 → 117**、一次中弹**全烧**、消弹闩锁只给第一家、
 *     无敌**取 max 不求和**、**113 传进来必须被硬拒**（分层越界）；
 *   · **type 3 弹窗分**（TH13 一手整数链 `piv/100` 再取整到 10 的倍数）；
 *   · `astral.anm` 脚本号（`type*6+1` / `+4`）；
 *   · 击破队列（上限丢弃 / 取走清零）；
 *   · ★ **神灵实体池的策略**（2026-09-12 第二轮；用户裁决「不下落 / 全屏收点收不走 / 必须跟过去吃 /
 *     随时间变小到点消失 / 不是敌人」）：缩小曲线（一手 1.5 → 200 帧 → 0.4）、寿命 480 帧的到期判定、
 *     吃取半径 24 px 的**边界**（**含**端点）、池容量与剩余位、原地微动方波、**入槽值换算**，
 *     外加一段「击破 → 撒实体 → 走近才收」的整数模拟（**关键**：只击破、不走近 ⇒ 槽**不涨**）；
 *   · 边界：NULL 入参、越界 type、负槽值。
 *
 * 一手出处全部在 `cards/trance_core.h` 的文件头（那张表）；本文件只测行为、不重复论证。
 */
#include <stdio.h>
#include <string.h>

#include "../cards/trance_core.h"

static int s_fail, s_pass;
#define CHECK(cond) do { if (cond) ++s_pass; else { ++s_fail; printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } } while (0)

static trance_cond_t cond_all_ok(void)
{
    trance_cond_t c;
    memset(&c, 0, sizeof c);
    c.full = 1; c.enemies = 1; c.cooldown = 1; c.key = 1;   /* 其余（dialogue/bombing/in_trance）= 0 */
    c.game_flags_ok = 1;                                     /* ★ 第 8 条（`th13:0x405859`）；TH18 全局 0x4cccc8 */
    return c;
}

int main(void)
{
    trance_state_t s;
    trance_gauge_t g;
    trance_gauge_add_t ga;
    trance_gauge_tick_t gt;
    trance_tick_t tk;
    trance_collect_t col;
    trance_reward_t r;
    trance_cond_t c;
    trance_rescue_report_t rep;
    const int32_t *ord;
    int i;

    /* ================= 0. 自检与常量（改常量忘改语义 ⇒ 立刻红）================= */
    CHECK(trance_core_selfcheck() == 0);
    CHECK(TRANCE_GAUGE_MAX == 600);
    CHECK(TRANCE_GAUGE_TIER == 200);
    CHECK(TRANCE_GAUGE_TIERS == 3);
    CHECK(TRANCE_GAUGE_PER_SPIRIT == 2);
    CHECK(TRANCE_GAUGE_TYPE3_BONUS == 1);
    CHECK(TRANCE_GAUGE_DRAIN == 1);
    CHECK(TRANCE_GAUGE_END_PENALTY == 200);
    CHECK(TRANCE_GAUGE_MIN_ENTER == 200);
    CHECK(TRANCE_COOLDOWN_FRAMES == 30);
    CHECK(TRANCE_CHAIN_WINDOW == 60);
    CHECK(TRANCE_CHAIN_THRESHOLD == 10);
    /* 击破 → 神灵的映射常量（SPEC §2.2：击破走通道 B ⇒ type 1；
     * ★ 2026-09-13：颗数**不再是** ❓ 自定 = 1 —— 走 TH13 通道 B 的衰减公式（见第 12b 节）*/
    CHECK(TRANCE_KILL_SPIRIT_TYPE == 1);
    CHECK(TRANCE_KILL_SPIRIT_MIN == 1);
    CHECK(TRANCE_KILL_SPIRIT_MAX == 64);
    CHECK(TRANCE_DURATION_FRAMES_MAX == 600);    /* 满槽 600 帧 = 10.00 s */
    CHECK(TRANCE_TYPE_COUNT == 4);
    CHECK(TRANCE_ID != TRANCE_RESCUE_107 && TRANCE_ID != TRANCE_RESCUE_117);

    /* ================= 1. 格（每 200 一格，共 3 格）================= */
    CHECK(trance_gauge_tier(0) == 0);
    CHECK(trance_gauge_tier(1) == 0);
    CHECK(trance_gauge_tier(199) == 0);
    CHECK(trance_gauge_tier(200) == 1);
    CHECK(trance_gauge_tier(399) == 1);
    CHECK(trance_gauge_tier(400) == 2);
    CHECK(trance_gauge_tier(599) == 2);
    CHECK(trance_gauge_tier(600) == 3);
    CHECK(trance_gauge_tier(601) == 3);          /* 越界也夹到 3 格 */
    CHECK(trance_gauge_tier(-5) == 0);

    trance_gauge_reset(&g);
    CHECK(g.gauge == 0 && g.tier == 0);
    trance_gauge_set(&g, 350);
    CHECK(g.gauge == 350 && g.tier == 1);
    trance_gauge_set(&g, 9999);
    CHECK(g.gauge == 600 && g.tier == 3);        /* 一手：`0x43d530` 夹到 600 */
    trance_gauge_set(&g, -7);
    CHECK(g.gauge == 0 && g.tier == 0);

    /* ================= 2. 收神灵加槽（+2；type3 再 +1）与跨格反馈 ================= */
    trance_gauge_reset(&g);
    ga = trance_gauge_add(&g, TRANCE_GAUGE_PER_SPIRIT);
    CHECK(ga.delta == 2 && ga.gauge == 2 && ga.tier == 0 && ga.crossed_tier == 0);
    ga = trance_gauge_add(&g, TRANCE_GAUGE_PER_SPIRIT);
    CHECK(ga.gauge == 4 && ga.crossed_tier == 0);

    trance_gauge_set(&g, 198);
    ga = trance_gauge_add(&g, TRANCE_GAUGE_PER_SPIRIT);
    CHECK(ga.gauge == 200 && ga.tier == 1 && ga.crossed_tier == 1);   /* 跨第 1 格 */
    ga = trance_gauge_add(&g, TRANCE_GAUGE_TYPE3_BONUS);
    CHECK(ga.gauge == 201 && ga.crossed_tier == 0);                   /* 已在本格内，不重复报 */

    trance_gauge_set(&g, 599);
    ga = trance_gauge_add(&g, TRANCE_GAUGE_TYPE3_BONUS);
    CHECK(ga.gauge == 600 && ga.tier == 3 && ga.crossed_tier == 1);   /* 跨第 3 格 */
    ga = trance_gauge_add(&g, TRANCE_GAUGE_PER_SPIRIT);
    CHECK(ga.delta == 0 && ga.gauge == 600 && ga.crossed_tier == 0);  /* 满槽：再加也不动 */
    ga = trance_gauge_add(&g, -700);
    CHECK(ga.gauge == 0 && ga.delta == -600 && ga.crossed_tier == 0);  /* 减到 0 夹住，不报跨格 */

    /* 非死亡结束补扣 200（`th13:0x405cdd`） */
    trance_gauge_set(&g, 600);
    ga = trance_gauge_sub(&g, TRANCE_GAUGE_END_PENALTY);
    CHECK(ga.gauge == 400 && ga.tier == 2 && ga.delta == -200);
    trance_gauge_set(&g, 100);
    ga = trance_gauge_sub(&g, TRANCE_GAUGE_END_PENALTY);
    CHECK(ga.gauge == 0 && ga.delta == -100);     /* 夹到 0（一手：`if (<0) = 0`）*/

    /* 灵界中 −1/帧（不是一次性扣 200） */
    trance_gauge_set(&g, 600);
    for (i = 0; i < 10; ++i) gt = trance_gauge_drain(&g);
    CHECK(gt.gauge == 590 && gt.floor == 0);
    trance_gauge_set(&g, 1);
    gt = trance_gauge_drain(&g);
    CHECK(gt.gauge == 0 && gt.floor == 1);       /* 一手 `0x43d4f0`：`if (GAUGE < 1) end()` */
    gt = trance_gauge_drain(&g);
    CHECK(gt.gauge == 0 && gt.floor == 1);       /* 已 0：仍报归零（幂等）*/

    /* ================= 3. 激活条件（一手八条，逐条单点放开）=================
     * ⚠ 契约：`trance_cond_t.full` / `.cooldown` 是**调用方**从槽与计时器算出来的（卡侧的
     * `trance_fill_cond` 就是这么做的，`core` 自己不看表）⇒ 本测里两者必须**自洽**。*/
    memset(&s, 0, sizeof s);
    trance_gauge_reset(&s.gauge);
    s.phase = TRANCE_PHASE_IDLE;
    s.cooldown = TRANCE_COOLDOWN_FRAMES;
    c = cond_all_ok();
    c.full = (s.gauge.gauge >= TRANCE_GAUGE_MAX) ? 1 : 0;
    CHECK(s.gauge.gauge == 0 && c.full == 0);     /* 前提：槽是空的（下面这条才有意义）*/
    CHECK(trance_can_activate(&s, &c) == 0);      /* ★ 槽没满 ⇒ 不行（`0x405815` **必须满槽 600**）*/
    trance_gauge_set(&s.gauge, TRANCE_GAUGE_MAX);
    c.full = 1;
    CHECK(trance_can_activate(&s, &c) == 1);
    CHECK(trance_can_activate(&s, NULL) == 0);
    CHECK(trance_can_activate(NULL, &c) == 0);

    c.enemies = 0;    CHECK(trance_can_activate(&s, &c) == 0); c.enemies = 1;   /* `0x40582a` */
    c.cooldown = 0;   CHECK(trance_can_activate(&s, &c) == 0); c.cooldown = 1;  /* `0x405842` */
    c.dialogue = 1;   CHECK(trance_can_activate(&s, &c) == 0); c.dialogue = 0;  /* `0x40584d` */
    c.bombing = 1;    CHECK(trance_can_activate(&s, &c) == 0); c.bombing = 0;   /* `0x405806` */
    c.in_trance = 1;  CHECK(trance_can_activate(&s, &c) == 0); c.in_trance = 0; /* `0x4057fa` */
    /* ★ 第 8 条（`0x405859`）：`(GAME_FLAGS & 0x30) != 0x20`。
     * 卡侧 trance_game_flags_ok() 把 `(byte[0x4cccc8] & 0x30) != 0x20` 折成这个 bool ⇒ 这里只测 bool 那一侧。*/
    c.game_flags_ok = 0; CHECK(trance_can_activate(&s, &c) == 0);   /* 状态域 == 2 ⇒ 不满足 */
    c.game_flags_ok = 1; CHECK(trance_can_activate(&s, &c) == 1);
    c.key = 0;        CHECK(trance_can_activate(&s, &c) == 0);                  /* `0x405833` */
    c.auto_mode = 1;  CHECK(trance_can_activate(&s, &c) == 1);                  /* ⏳ SPEC §9 #1 兜底 */
    c.key = 1;        CHECK(trance_can_activate(&s, &c) == 1);
    c.auto_mode = 0;  CHECK(trance_can_activate(&s, &c) == 1);
    /* 第 8 条与其它条**同时**成立才放行（单点放开法：这里是「它独自失败」那一格）*/
    c.game_flags_ok = 0;
    CHECK(trance_can_activate(&s, &c) == 0);
    c.game_flags_ok = 1;
    s.phase = TRANCE_PHASE_MANUAL;
    CHECK(trance_can_activate(&s, &c) == 0);      /* 已在灵界 ⇒ 不行 */
    s.phase = TRANCE_PHASE_IDLE;

    /* ================= 4. begin / end 的门与收尾 ================= */
    memset(&s, 0, sizeof s);
    trance_gauge_reset(&s.gauge);
    trance_gauge_set(&s.gauge, 600);
    c = cond_all_ok();
    CHECK(trance_begin(&s, 0, &c) == 1);
    CHECK(s.phase == TRANCE_PHASE_MANUAL);
    CHECK(s.activations == 1 && s.deaths_rescued == 0);
    CHECK(trance_begin(&s, 0, &c) == 1);          /* 一手：已在灵界 = 视为成功（幂等）*/
    CHECK(s.activations == 1);                    /* 但没有第二次激活 */

    tk.ended = 0; tk.ended_death_free = 0;
    trance_end(&s, &tk);
    CHECK(tk.ended == 1 && tk.ended_death_free == 0);
    CHECK(s.phase == TRANCE_PHASE_IDLE);
    CHECK(s.gauge.gauge == 400);                  /* ★ 手动/自动结束：−200（`0x405cdd`）*/
    tk.ended = 0;
    trance_end(&s, &tk);                          /* 已空闲：空操作 */
    CHECK(tk.ended == 0 && s.gauge.gauge == 400);

    /* 死亡强制：结束**不**补扣 200（一手 `end` 里 `if (!(flags & 2)) GAUGE -= 200`）*/
    memset(&s, 0, sizeof s);
    trance_gauge_reset(&s.gauge);
    trance_gauge_set(&s.gauge, 600);
    CHECK(trance_begin(&s, 1, &c) == 1);
    CHECK(s.phase == TRANCE_PHASE_DEATH_FREE && s.deaths_rescued == 1);
    tk.ended = 0; tk.ended_death_free = 0;
    trance_end(&s, &tk);
    CHECK(tk.ended == 1 && tk.ended_death_free == 1);
    CHECK(s.gauge.gauge == 600);                  /* ★ 不补扣（也**不**重走 die —— 我们不做延时）*/
    CHECK(s.ended_death_free == 1);

    /* 槽 < 200 ⇒ begin 失败（一手 `0x405a40` 的硬门） */
    memset(&s, 0, sizeof s);
    trance_gauge_reset(&s.gauge);
    trance_gauge_set(&s.gauge, TRANCE_GAUGE_MIN_ENTER - 1);
    CHECK(trance_begin(&s, 0, &c) == 0);
    CHECK(s.phase == TRANCE_PHASE_IDLE && s.activations == 0);
    trance_gauge_set(&s.gauge, TRANCE_GAUGE_MIN_ENTER);
    CHECK(trance_begin(&s, 0, &c) == 1);          /* 恰好 200 ⇒ 成功 */
    trance_end(&s, NULL);
    CHECK(s.gauge.gauge == 0);                    /* 200 − 200 */

    /* ================= 5. 每帧推进（冷却 / −1 帧 / 归零收尾）================= */
    memset(&s, 0, sizeof s);
    trance_gauge_reset(&s.gauge);
    for (i = 0; i < TRANCE_COOLDOWN_FRAMES - 1; ++i) tk = trance_tick(&s, i);
    CHECK(s.cooldown == TRANCE_COOLDOWN_FRAMES - 1);
    CHECK(tk.phase == TRANCE_PHASE_IDLE && tk.ended == 0 && tk.drain_floor == 0);
    for (i = 0; i < 100; ++i) tk = trance_tick(&s, 1000 + i);
    CHECK(s.cooldown == TRANCE_COOLDOWN_FRAMES);  /* 涨到 30 就停 */

    trance_gauge_set(&s.gauge, 3);
    s.phase = TRANCE_PHASE_MANUAL;
    tk = trance_tick(&s, 2000);
    CHECK(tk.phase == TRANCE_PHASE_MANUAL && tk.drain_floor == 0 && s.gauge.gauge == 2);
    tk = trance_tick(&s, 2001);
    CHECK(s.gauge.gauge == 1 && tk.drain_floor == 0);
    tk = trance_tick(&s, 2002);
    CHECK(tk.drain_floor == 1 && tk.ended == 1 && tk.ended_death_free == 0);
    CHECK(s.phase == TRANCE_PHASE_IDLE);
    CHECK(s.gauge.gauge == 0);
    tk = trance_tick(&s, 2003);
    CHECK(tk.ended == 0 && tk.drain_floor == 0);

    /* 相位非 0 而槽已是 0（卡侧手工改过槽）⇒ 与一手同形：`GAUGE < 1` 恒真 ⇒ 当帧收尾（幂等）*/
    memset(&s, 0, sizeof s);
    trance_gauge_reset(&s.gauge);
    s.phase = TRANCE_PHASE_DEATH_FREE;
    tk = trance_tick(&s, 3000);
    CHECK(tk.drain_floor == 1 && tk.ended == 1 && s.phase == TRANCE_PHASE_IDLE);
    CHECK(s.gauge.gauge == 0);
    CHECK(tk.ended_death_free == 1);                       /* 相位 2 的收尾不带 −200 */
    CHECK(trance_tick(NULL, 0).phase == TRANCE_PHASE_IDLE);   /* NULL 安全 */

    /* ================= 6. 死亡强制（+0x14 的唯一动作）================= */
    memset(&s, 0, sizeof s);
    trance_gauge_reset(&s.gauge);
    trance_gauge_set(&s.gauge, TRANCE_GAUGE_MAX);
    CHECK(trance_force_on_death(&s) == 1);
    CHECK(s.phase == TRANCE_PHASE_DEATH_FREE);
    CHECK(s.deaths_rescued == 1 && s.activations == 1);
    CHECK(s.gauge.gauge == TRANCE_GAUGE_MAX);     /* ★ 不消耗槽：开窗口 ≠ 扣 200 */
    /* ★ 第二版：**已在灵界**时 `begin` 走「幂等成功」那条路 —— 一手在那里**仍然**把 bit1 设成
     * `param_1`（`0x405a4c`–`0x405a50`）⇒ 我们的语义是「返回 1 = 这次中弹被吃下了」，
     * 但**不**重复计数（`deaths_rescued` 不涨：本来就是死亡强制档，没有第二次欠账）。*/
    CHECK(trance_force_on_death(&s) == 1);
    CHECK(s.phase == TRANCE_PHASE_DEATH_FREE);
    CHECK(s.deaths_rescued == 1);
    CHECK(s.activations == 1);
    trance_end(&s, NULL);
    CHECK(s.gauge.gauge == TRANCE_GAUGE_MAX);     /* 死亡强制的结束不补扣 */

    memset(&s, 0, sizeof s);
    trance_gauge_reset(&s.gauge);
    trance_gauge_set(&s.gauge, TRANCE_GAUGE_MIN_ENTER - 1);
    CHECK(trance_force_on_death(&s) == 0);        /* 同一道门：槽 < 200 开不成 */
    CHECK(s.phase == TRANCE_PHASE_IDLE && s.deaths_rescued == 0);
    CHECK(trance_force_on_death(NULL) == 0);

    /* ================= 7. 四类神灵：奖励表（普通 / 灵界两套值）================= */
    /* type 0 = 命碎 */
    r = trance_reward(0, 0, 0);
    CHECK(r.life_frag == 1 && r.max_point == 0 && r.spell_frag == 0);
    CHECK(r.gauge == 2 && r.gauge_type3 == 0);
    r = trance_reward(0, 1, 0);
    CHECK(r.life_frag == 2);                      /* 灵界中翻倍 */
    CHECK(r.gauge == 0);                          /* ★ 灵界中收神灵**不再加槽** */
    /* type 1 = 最大得点 */
    r = trance_reward(1, 0, 0);
    CHECK(r.max_point == 10 && r.life_frag == 0 && r.spell_frag == 0);
    CHECK(r.gauge == 2);
    r = trance_reward(1, 1, 0);
    CHECK(r.max_point == 100);                    /* +10 → +100 */
    CHECK(r.gauge == 0);
    /* type 2 = 符卡碎 */
    r = trance_reward(2, 0, 0);
    CHECK(r.spell_frag == 1 && r.life_frag == 0 && r.max_point == 0);
    CHECK(r.gauge == 2);
    r = trance_reward(2, 1, 0);
    CHECK(r.spell_frag == 2 && r.gauge == 0);
    /* type 3 = 槽 +1 且加分（灵界中只加分） */
    r = trance_reward(3, 0, 0);
    CHECK(r.gauge == 2 && r.gauge_type3 == 1);
    CHECK(r.life_frag == 0 && r.max_point == 0 && r.spell_frag == 0);
    r = trance_reward(3, 1, 0);
    CHECK(r.gauge == 0 && r.gauge_type3 == 0);    /* 灵界中不加槽 */
    /* type 4 = item 14 的别名（一手：`0x438ed8` 与 1 同奖）*/
    r = trance_reward(4, 0, 0);
    CHECK(r.max_point == 10 && r.gauge == 2);
    r = trance_reward(4, 1, 0);
    CHECK(r.max_point == 100 && r.gauge == 0);
    /* 越界 */
    r = trance_reward(-1, 0, 0);
    CHECK(r.life_frag == 0 && r.max_point == 0 && r.spell_frag == 0 && r.gauge == 0 && r.gauge_type3 == 0);
    r = trance_reward(5, 1, 0);
    CHECK(r.life_frag == 0 && r.max_point == 0 && r.gauge == 0);
    /* 早苗特例（`th13:0x438bea`）：★ 2026-09-13 用户口径「其他三个人原样进去就行」⇒ **已打开**。
     * 一手语义：TH13 的 `CHARACTER == 2`（= 早苗）收 type 1 时**额外 +1 槽**；灵界中不加槽 ⇒ 也不加这一笔。*/
    CHECK(TRANCE_ENABLE_SANAE_TYPE1_BONUS == 1);
    r = trance_reward(1, 0, 1);
    CHECK(r.gauge == 2 + TRANCE_GAUGE_SANAE_TYPE1);
    r = trance_reward(1, 0, 0);                            /* 非早苗：不带这一笔 */
    CHECK(r.gauge == 2);
    r = trance_reward(0, 0, 1);                            /* 早苗但**不是 type 1**：不带 */
    CHECK(r.gauge == 2);
    r = trance_reward(1, 1, 1);                            /* 灵界中：一分槽都不加 */
    CHECK(r.gauge == 0);

    /* ================= 8. 收集：槽账 + 按 type 计数 + 连锁 ================= */
    memset(&s, 0, sizeof s);
    trance_gauge_reset(&s.gauge);
    CHECK(trance_collect(&s, 0, &col) == 1);
    CHECK(col.gauge_add == 2 && col.tier_crossed == 0);
    CHECK(s.spirits_seen[0] == 1 && s.spirits_seen[1] == 0);
    /* ★★ 2026-09-14：**收集不再推进连锁**（一手连锁长在 `spawn_spirit` 尾部 `th13:0x4391a0`，
     *   见 `trance_core.h` 的粒度订正段）⇒ 收多少颗，`chain_count` 都必须纹丝不动。*/
    CHECK(s.chain_count == 0 && s.chain_timer == 0);

    CHECK(trance_collect(&s, 9, &col) == 0);               /* 越界 type：不动任何东西 */
    CHECK(col.gauge_add == 0 && s.gauge.gauge == 2);
    CHECK(trance_collect(NULL, 0, &col) == 0);

    trance_collect(&s, 1, NULL);                            /* out 可为 NULL */
    CHECK(s.spirits_seen[1] == 1 && s.gauge.gauge == 4 && s.chain_count == 0);

    /* type 3 收一颗：槽 +2 +1 = 3；灵界中 0 */
    trance_collect(&s, 3, &col);
    CHECK(col.gauge_add == 3 && s.gauge.gauge == 7);
    CHECK(s.spirits_seen[3] == 1);
    s.phase = TRANCE_PHASE_MANUAL;
    trance_collect(&s, 3, &col);
    CHECK(col.gauge_add == 0 && s.gauge.gauge == 7);
    CHECK(s.spirits_seen[3] == 2);
    s.phase = TRANCE_PHASE_IDLE;

    /* 连锁：**在"掉出一颗灵"的那一刻**数到第 10 颗就额外送 type 3
     * （一手 `th13:0x4391a0` 尾段逐条，长在 `spawn_spirit` 里：
     *     `+0x8828 = 60; +0x8838++ ; if (+0x8838 > 9) spawn_spirit(this, 3, pos, 0.5, 1);`
     *   ⇒ 判据是「加完之后 ≥ 10」= 本次这颗就是窗口内第 10 颗。
     *   ★ 2026-09-12 修的是比较符（旧 `> 10` = 第 11 颗才触发）；
     *   ★★ 2026-09-14 修的是**粒度**（旧版挂在"收集"上 ⇒ 玩家连续吸灵时窗口永不过期 ⇒ 白灵刷屏）
     *     ⇒ 本节改成直接驱动 `trance_chain_note()`（= 卡侧 `trance_spirit_spawn()` 的调用形状）。*/
    memset(&s, 0, sizeof s);
    trance_gauge_reset(&s.gauge);
    for (i = 1; i < TRANCE_CHAIN_THRESHOLD; ++i) {          /* 第 1..9 颗 */
        CHECK(trance_collect(&s, 1, &col) == 1);
        CHECK(trance_chain_note(&s, 1) == 0);   /* 前 9 颗都不触发 */
        CHECK(s.chain_count == i && s.chain_timer == TRANCE_CHAIN_WINDOW);
    }
    CHECK(i == 10);                             /* 手算：循环跑完 i = 10 = TRANCE_CHAIN_THRESHOLD */
    CHECK(trance_collect(&s, 1, &col) == 1);    /* 第 10 颗照样只有"收集"效果 */
    CHECK(trance_chain_note(&s, 1) == 1);       /* ★ 第 10 颗 ⇒ 触发（那份额外的 type 3 就是场上的第 11 颗）*/
    CHECK(s.chain_count == 10 && s.chain_bonus_spawned == 1);
    CHECK(trance_chain_note(&s, 1) == 1);       /* 第 11 颗继续触发（一手：计数 > 9 之后每颗都触发）*/
    CHECK(s.chain_bonus_spawned == 2);
    CHECK(s.chain_count == 11);

    /* type 3 / 4 自身不推进连锁（一手：`if (type != 3 && type != 4)`）*/
    {
        int32_t before = s.chain_count;
        CHECK(trance_chain_note(&s, 3) == 0);
        CHECK(s.chain_count == before);
        CHECK(trance_chain_note(&s, 4) == 0);
        CHECK(s.chain_count == before);
        CHECK(trance_chain_note(NULL, 1) == 0);
    }

    /* 窗口每帧 −1、归零清计数 */
    memset(&s, 0, sizeof s);
    trance_gauge_reset(&s.gauge);
    trance_chain_note(&s, 1);
    CHECK(s.chain_timer == TRANCE_CHAIN_WINDOW && s.chain_count == 1);
    for (i = 0; i < TRANCE_CHAIN_WINDOW - 1; ++i) CHECK(trance_chain_tick(&s, i) == 0);
    CHECK(s.chain_timer == 1 && s.chain_count == 1);
    CHECK(trance_chain_tick(&s, 100) == 1);
    CHECK(s.chain_timer == 0 && s.chain_count == 0);         /* 窗口归零 ⇒ `+0x8838 = 0` */
    CHECK(trance_chain_tick(&s, 101) == 0);                  /* 已归零：不再报 */
    CHECK(trance_chain_tick(NULL, 0) == 0);

    /* ================= 9. 奖励单位换算（SPEC §9 #5 ⏳ 做成参数）================= */
    CHECK(trance_max_point_field_addends(10, 100) == 1000);
    CHECK(trance_max_point_field_addends(100, 100) == 10000);
    CHECK(trance_max_point_field_addends(10, 1) == 10);
    CHECK(trance_max_point_field_addends(0, 100) == 0);
    CHECK(trance_max_point_field_addends(-10, 100) == -1000);
    CHECK(trance_max_point_field_addends(10, 0) == 10);      /* 单位未知 ⇒ 原样返回 */
    CHECK(trance_max_point_field_addends(10, -1) == 10);
    CHECK(trance_max_point_field_addends(2147483647, 100) == 2147483647);   /* 防溢出 */
    CHECK(trance_max_point_field_addends(-2147483647, 100) == (-2147483647 - 1));

    /* type 3 弹窗分：一手整数链 `(piv/100)` 再向下取整到 10 的倍数（`th13:0x438c3d`）*/
    CHECK(trance_type3_popup_from_piv(0) == 0);
    CHECK(trance_type3_popup_from_piv(-5) == 0);
    CHECK(trance_type3_popup_from_piv(99) == 0);          /* 0/10 = 0 */
    CHECK(trance_type3_popup_from_piv(100) == 0);         /* 1 → 取整到 10 的倍数 = 0 */
    CHECK(trance_type3_popup_from_piv(999) == 0);         /* 9 → 0 */
    CHECK(trance_type3_popup_from_piv(1000) == 10);       /* 10 → 10 */
    CHECK(trance_type3_popup_from_piv(1009) == 10);
    CHECK(trance_type3_popup_from_piv(30000) == 300);     /* 300 → 300 */
    CHECK(trance_type3_popup_from_piv(30099) == 300);
    CHECK(trance_type3_popup_from_piv(1234567) == 12340); /* 12345 → 12340 */
    CHECK(trance_type3_popup_from_piv(50000) == 500);
    /* 入账口径（一手 `0x413bf0`：`SCORE += 值/10`）—— 落笔由计分层做，这里只钉住上游值 */
    CHECK(trance_type3_popup_from_piv(30000) / 10 == 30);

    /* ================= 10. astral.anm 脚本号（一手 `th13:0x43907e`）================= */
    CHECK(trance_anm_script(0, 0) == 1);
    CHECK(trance_anm_script(1, 0) == 7);
    CHECK(trance_anm_script(2, 0) == 13);
    CHECK(trance_anm_script(3, 0) == 19);
    CHECK(trance_anm_script(0, 1) == 4);
    CHECK(trance_anm_script(1, 1) == 10);
    CHECK(trance_anm_script(2, 1) == 16);
    CHECK(trance_anm_script(3, 1) == 22);
    CHECK(trance_anm_script(4, 0) == 7);        /* type 4 与 1 同形 */
    CHECK(trance_anm_script(4, 1) == 10);
    CHECK(trance_anm_script(-1, 0) == 0);
    CHECK(trance_anm_script(5, 1) == 0);

    /* ================= 11. 救援层（EC §5.2：一次中弹全烧、顺序 107 → 117）================= */
    ord = trance_rescue_order();
    CHECK(ord != NULL);
    CHECK(ord[0] == 107);
    CHECK(ord[1] == 117);
    CHECK(TRANCE_RESCUE_COUNT == 2);
    CHECK(trance_rescue_invuln_frames(107) == 40);
    CHECK(trance_rescue_invuln_frames(117) == 40);
    CHECK(trance_rescue_invuln_frames(113) == 0);      /* 113 不在这层 */
    CHECK(trance_rescue_invuln_frames(0) == 0);

    /* 两个都在场：**全烧**、顺序 107 → 117、消弹只给 107、无敌取 max = 40 */
    CHECK(trance_rescue_resolve(0x3, 117, &rep) == 1);
    CHECK(rep.slots[0].id == 107 && rep.slots[1].id == 117);
    CHECK(rep.slots[0].was_active == 1 && rep.slots[1].was_active == 1);
    CHECK(rep.slots[0].consumed == 1 && rep.slots[1].consumed == 1);   /* ★ 一次中弹把救援者全部烧掉 */
    CHECK(rep.slots[0].burst == 1 && rep.slots[1].burst == 1);         /* 两家的动作都要执行 */
    CHECK(rep.slots[0].cancel_all == 1 && rep.slots[1].cancel_all == 0); /* 细则 2：只有第一家消弹 */
    CHECK(rep.cancel_taken == 107);
    CHECK(rep.invuln_frames == 40);                                    /* 细则 3：max 不是 80 */
    CHECK(rep.any == 1 && rep.death_cancelled == 1);

    /* 只有 117 在场 */
    CHECK(trance_rescue_resolve(0x2, 117, &rep) == 1);
    CHECK(rep.slots[0].was_active == 0 && rep.slots[0].consumed == 0 && rep.slots[0].cancel_all == 0);
    CHECK(rep.slots[1].consumed == 1 && rep.slots[1].cancel_all == 1);
    CHECK(rep.cancel_taken == 117 && rep.invuln_frames == 40 && rep.death_cancelled == 1);

    /* 只有 107 在场 */
    CHECK(trance_rescue_resolve(0x1, 117, &rep) == 1);
    CHECK(rep.slots[0].consumed == 1 && rep.slots[0].cancel_all == 1);
    CHECK(rep.slots[1].consumed == 0);
    CHECK(rep.cancel_taken == 107 && rep.any == 1);

    /* 一家都不在 ⇒ 死亡照走（之后才轮到 113 的 +0x14）*/
    CHECK(trance_rescue_resolve(0x0, 117, &rep) == 1);
    CHECK(rep.any == 0 && rep.death_cancelled == 0 && rep.invuln_frames == 0);
    CHECK(rep.cancel_taken == -1);
    CHECK(rep.slots[0].consumed == 0 && rep.slots[1].consumed == 0);

    /* ★★ 113 不许进这一层（SPEC §7 第一条：分层不许越界）—— 两种入口都硬拒 */
    CHECK(trance_rescue_resolve(0x3, 113, &rep) == 0);
    CHECK(trance_rescue_resolve(0x0, 113, &rep) == 0);
    CHECK(trance_rescue_resolve(0x3, 110, &rep) == 1);   /* 别的卡照样能问 */
    CHECK(trance_rescue_resolve(0x3, 117, NULL) == 0);   /* NULL 安全 */
    /* 掩码越界位不影响结果（只认低 2 位）*/
    CHECK(trance_rescue_resolve(0xff, 117, &rep) == 1);
    CHECK(rep.any == 1 && rep.cancel_taken == 107);

    /* ================= 12. 击破队列（事件钩子 → O(1) 入队 → 帧末 take）=================
     * 一手：`on_enemy_death` 由断点 `0x430240` 沿卡链表派发（**不经 21 槽虚表**），
     * 清场帧一次 100–300 次（`DEATH` §1.7）⇒ 只入队、只存标量（`ev->enemy` 禁止缓存）。
     * ★ 2026-09-13：**每条再带一个颗数**（TH13 通道 B 的衰减结果）——
     *   ⚠ 一次击破 = **一条**（绝不按颗数展开：16 格队列 + 清场帧 100–300 次 ⇒ 展开必溢出）。*/
    {
        float ox[TRANCE_KILL_QUEUE_MAX], oy[TRANCE_KILL_QUEUE_MAX];
        int32_t on[TRANCE_KILL_QUEUE_MAX];
        memset(&s, 0, sizeof s);
        trance_gauge_reset(&s.gauge);
        CHECK(trance_kill_take(&s, 0, ox, oy, on, TRANCE_KILL_QUEUE_MAX) == 0);

        trance_kill_note(&s, 10, 100.5f, -20.25f, 6);
        CHECK(s.kill_pending == 1 && s.kill_last_frame == 10);
        CHECK(s.kill_x[0] == 100.5f && s.kill_y[0] == -20.25f);      /* 位置快照进队 */
        CHECK(s.kill_n[0] == 6);                                     /* 颗数进队 */
        trance_kill_note(&s, 11, -3.0f, 444.0f, 1);
        CHECK(s.kill_pending == 2 && s.kill_last_frame == 11);
        CHECK(s.kill_x[1] == -3.0f && s.kill_y[1] == 444.0f);
        /* 脏数据保底：n ≤ 0 按 1 记（正常路径不会出现；见 trance_kill_note 的注释）*/
        trance_kill_note(&s, 11, 5.0f, 5.0f, 0);
        CHECK(s.kill_n[2] == TRANCE_KILL_SPIRIT_MIN);
        CHECK(trance_kill_take(&s, 11, NULL, NULL, NULL, 0) == 3);   /* 清干净，别影响下面 */

        /* 帧末取走：返回**击破次数** + 按入队顺序抄位置/颗数；取走即清零 */
        trance_kill_note(&s, 10, 100.5f, -20.25f, 6);
        trance_kill_note(&s, 11, -3.0f, 444.0f, 1);
        CHECK(trance_kill_take(&s, 12, ox, oy, on, TRANCE_KILL_QUEUE_MAX) == 2);
        CHECK(ox[0] == 100.5f && oy[0] == -20.25f && on[0] == 6);
        CHECK(ox[1] == -3.0f && oy[1] == 444.0f && on[1] == 1);
        CHECK(s.kill_pending == 0);
        CHECK(trance_kill_take(&s, 13, ox, oy, on, TRANCE_KILL_QUEUE_MAX) == 0);

        /* 只要次数、不要位置/颗数（out 传 NULL / cap = 0 都可以）*/
        trance_kill_note(&s, 14, 1.0f, 2.0f, 3);
        CHECK(trance_kill_take(&s, 15, NULL, NULL, NULL, 0) == 1);
        CHECK(s.kill_pending == 0);
        trance_kill_note(&s, 16, 3.0f, 4.0f, 3);
        CHECK(trance_kill_take(&s, 17, ox, oy, on, 0) == 1);        /* cap = 0：不抄但报次数 */
        CHECK(ox[0] == 100.5f);                                      /* 缓冲没被写（还是上一次的值）*/
        /* ★ 只给位置不给颗数（out_n = NULL）也要能跑：卡侧老调用点就是这么退化的 */
        trance_kill_note(&s, 18, 7.0f, 8.0f, 12);
        CHECK(trance_kill_take(&s, 19, ox, oy, NULL, TRANCE_KILL_QUEUE_MAX) == 1);
        CHECK(ox[0] == 7.0f && oy[0] == 8.0f);

        /* cap 小于次数：只抄前 cap 个，次数照实报 */
        memset(&s, 0, sizeof s);
        trance_kill_note(&s, 20, 10.0f, 11.0f, 2);
        trance_kill_note(&s, 21, 12.0f, 13.0f, 5);
        trance_kill_note(&s, 22, 14.0f, 15.0f, 1);
        CHECK(trance_kill_take(&s, 23, ox, oy, on, 2) == 3);
        CHECK(ox[0] == 10.0f && oy[0] == 11.0f && on[0] == 2);
        CHECK(ox[1] == 12.0f && oy[1] == 13.0f && on[1] == 5);

        /* 上限夹住 + 溢出记账；NULL 安全 */
        trance_kill_note(NULL, 0, 0.0f, 0.0f, 1);
        CHECK(trance_kill_take(NULL, 0, ox, oy, on, 4) == 0);
        {
            int32_t dropped_before = trance_kill_queue_dropped;
            for (i = 0; i < TRANCE_KILL_QUEUE_MAX + 5; ++i)
                trance_kill_note(&s, i, (float)i, (float)(i * 2), i + 1);
            CHECK(s.kill_pending == TRANCE_KILL_QUEUE_MAX);          /* 上限夹住 */
            CHECK(trance_kill_queue_dropped == dropped_before + 5);  /* 丢的**次数**有记账 */
            CHECK(s.kill_last_frame == TRANCE_KILL_QUEUE_MAX - 1);   /* 被丢的不改 last_frame */
            CHECK(trance_kill_take(&s, 0, ox, oy, on, TRANCE_KILL_QUEUE_MAX) == TRANCE_KILL_QUEUE_MAX);
            for (i = 0; i < TRANCE_KILL_QUEUE_MAX; ++i) CHECK(on[i] == i + 1);   /* 颗数跟着各自的条目 */
        }
    }

    /* ============ 12b. ★★ 击破颗数 = TH13 通道 B 的衰减（2026-09-13 新增）============
     *
     * 一手（`th13:0x419f41`，`engine/spirit/th13/01-spirit-and-trance.md` §3.2）：
     *     max = spec1(T，窗口) ; div = spec2(N，上限颗数)
     *     if (enemy_timer <= max) { n = (max − timer)/(max/div) + 1 ;
     *                               if (DIFFICULTY == 2 || DIFFICULTY == 3) n += 1 ; }
     * ⇒ 瞬杀 = N+1、拖满窗口 = 1、H/L 各 +1。
     * 期望值直接取一手普查（`thecl -d 13` 全 8 关）里的真实敌人，**不自己编数**。
     * TH18 侧的入参来源（T/N/alive）在 `cards/trance.c` 的 `on_enemy_death()`（原生同形通道）。*/
    {
        /* —— 瞬杀（alive = 0）= N + 1 —— */
        CHECK(trance_kill_spirit_count(0, 120, 5, 1) == 6);      /* st01 GirlB01 (5,120) ⇒ 6 */
        CHECK(trance_kill_spirit_count(0, 140, 5, 1) == 6);      /* st01 GirlB02 (5,140) ⇒ 6：140/5=28 ⇒ 5+1 */
        CHECK(trance_kill_spirit_count(0, 40, 1, 1) == 2);       /* st01 Phantom01 (1,40) ⇒ 2 */
        CHECK(trance_kill_spirit_count(0, 120, 6, 1) == 7);      /* st02 GirlC01func (6,120) ⇒ 7 */
        CHECK(trance_kill_spirit_count(0, 90, 3, 1) == 4);       /* st02 ECir00 (3,90) ⇒ 4 */
        CHECK(trance_kill_spirit_count(0, 350, 16, 1) == 17);    /* st05 GirlD01 (16,350) ⇒ 17 */
        CHECK(trance_kill_spirit_count(0, 400, 40, 1) == 41);    /* st07 GirlD02 (40,400) ⇒ 41 */

        /* —— 难度（一手只在 `== 2 || == 3` 上加；EXTRA(4) 不加）—— */
        CHECK(trance_kill_spirit_count(0, 120, 5, 0) == 6);      /* EASY */
        CHECK(trance_kill_spirit_count(0, 120, 5, 1) == 6);      /* NORMAL */
        CHECK(trance_kill_spirit_count(0, 120, 5, 2) == 7);      /* HARD +1 */
        CHECK(trance_kill_spirit_count(0, 120, 5, 3) == 7);      /* LUNATIC +1 */
        CHECK(trance_kill_spirit_count(0, 120, 5, 4) == 6);      /* EXTRA：一手没这一支 ⇒ 不加 */
        CHECK(trance_kill_spirit_count(-7, 120, 5, 1) == 6);     /* 负存活帧当 0（入参防御）*/

        /* —— 衰减：**单调不增**，且拖满窗口正好落到 1 ——
         * 步长 = T/N = 120/5 = 24 ⇒ 每多活 24 帧少 1 颗（整数除 ⇒ 台阶每 24 帧一级）。*/
        CHECK(trance_kill_spirit_count(1, 120, 5, 1) == 5);      /* (120−1)/24 = 4 ⇒ 5 */
        CHECK(trance_kill_spirit_count(24, 120, 5, 1) == 5);     /* 96/24 = 4 ⇒ 5（台阶上沿）*/
        CHECK(trance_kill_spirit_count(25, 120, 5, 1) == 4);     /* 95/24 = 3 ⇒ 4（下一级）*/
        CHECK(trance_kill_spirit_count(48, 120, 5, 1) == 4);     /* 72/24 = 3 ⇒ 4 */
        CHECK(trance_kill_spirit_count(96, 120, 5, 1) == 2);     /* 24/24 = 1 ⇒ 2 */
        CHECK(trance_kill_spirit_count(97, 120, 5, 1) == 1);     /* 23/24 = 0 ⇒ 1 */
        CHECK(trance_kill_spirit_count(119, 120, 5, 1) == 1);    /* 1/24 = 0 ⇒ 1（拖到最后）*/
        CHECK(trance_kill_spirit_count(120, 120, 5, 1) == 1);    /* 窗口用尽 ⇒ 0 + 1 = 1 */
        /* 逐帧扫一遍：**永远 ≥ 1，且不增**（这条是"越慢越少"的可执行表述）*/
        {
            int32_t alive, prev = trance_kill_spirit_count(0, 120, 5, 1);
            CHECK(prev == 6);
            for (alive = 1; alive <= 200; ++alive) {
                int32_t n = trance_kill_spirit_count(alive, 120, 5, 1);
                CHECK(n >= TRANCE_KILL_SPIRIT_MIN);
                CHECK(n <= prev);
                prev = n;
            }
            CHECK(prev == 1);                                    /* 过期之后恒 1（❓ 自定保底）*/
        }

        /* —— 窗口过期（alive > T）：一手 `if` 之外 = **0 颗**；本卡保底 1 颗（❓ 自定，见 .h）—— */
        CHECK(trance_kill_spirit_count(121, 120, 5, 1) == 1);
        CHECK(trance_kill_spirit_count(100000, 120, 5, 1) == 1);
        /* 过期这一档**不加**难度奖励（一手把 `+1` 写在 `if` 里面 ⇒ 照抄控制流）*/
        CHECK(trance_kill_spirit_count(121, 120, 5, 3) == 1);
        CHECK(trance_kill_spirit_count(120, 120, 5, 3) == 2);    /* 还在窗口内 ⇒ +1 照给 */

        /* —— 防呆：没配窗口 / 脏数据 ⇒ ≥ 1，且**不除零**（一手 `max/div` 在 div > max 时会炸）—— */
        CHECK(trance_kill_spirit_count(0, 0, 0, 1) == TRANCE_KILL_SPIRIT_MIN);
        CHECK(trance_kill_spirit_count(0, -5, 5, 1) == TRANCE_KILL_SPIRIT_MIN);
        CHECK(trance_kill_spirit_count(0, 120, 0, 1) == TRANCE_KILL_SPIRIT_MIN);
        CHECK(trance_kill_spirit_count(0, 120, -3, 1) == TRANCE_KILL_SPIRIT_MIN);
        CHECK(trance_kill_spirit_count(0, 1, 999, 1) == 2);      /* N > T ⇒ 步长按 1（不除零），T+1 = 2 */

        /* —— 上界夹取（卡侧调它 + 打日志；见 cards/trance.c 的 on_enemy_death）—— */
        CHECK(trance_kill_spirit_clamp(0) == TRANCE_KILL_SPIRIT_MIN);
        CHECK(trance_kill_spirit_clamp(-9) == TRANCE_KILL_SPIRIT_MIN);
        CHECK(trance_kill_spirit_clamp(1) == 1);
        CHECK(trance_kill_spirit_clamp(41) == 41);               /* 一手普查里最大的一笔（40+1）不被夹 */
        CHECK(trance_kill_spirit_clamp(TRANCE_KILL_SPIRIT_MAX) == TRANCE_KILL_SPIRIT_MAX);
        CHECK(trance_kill_spirit_clamp(TRANCE_KILL_SPIRIT_MAX + 1) == TRANCE_KILL_SPIRIT_MAX);
        CHECK(trance_kill_spirit_clamp(100000) == TRANCE_KILL_SPIRIT_MAX);
    }

    /* ============ 12b′. ★★ 兜底档 = 按敌人 HP 上限分两档（2026-09-23 重做）============
     *
     * 背景（owner 2026-09-19 实跑「神灵掉的**相对**数量偏多」）：旧兜底是写死的 `(120,5)`
     * ⇒ 瞬杀 6 颗，而同一张卡里配了原生窗口的敌人只有 2 颗（TH18 原生 `N` 众数 = 1）
     * ⇒ 观感上"没配窗口的反而掉得多"。新版把兜底钉在**TH18 原生窗口在同一 HP 档里的众数**上。
     *
     * 一手锚（全在 `cards/trance_core.h` 的 `TRANCE_KILL_FB_*` 注释里，本次重跑 `thecl -d 18`
     * ＋ `thecl -d 13` 得出）：<1000 三档的 `N` 众数 = 1（66/72、44/52、42/65）、`T` 众数 = 120；
     * ≥1000 档 `N` 众数 = 10（23/57）、`T` 众数 = 300（31/57）。TH13 侧同向（小妖精 1..4、GirlC/D 10..40）。*/
    {
        int32_t T = 0, N = 0;

        /* —— 小怪档（TH18 的实战主体：216 个未配窗口的创建点里 178 个 HP<500）—— */
        trance_kill_fallback(200, &T, &N);                       /* st04 Phantom01 那一档 */
        CHECK(T == 120 && N == 1);
        CHECK(trance_kill_spirit_count(0, T, N, 1) == 2);        /* 瞬杀 2 颗（= 原生小怪同档）*/
        CHECK(trance_kill_spirit_count(0, T, N, 2) == 3);        /* Hard +1 */
        CHECK(trance_kill_spirit_count(T + 1, T, N, 1) == 1);    /* 拖过窗口 ⇒ 保底 1 */

        /* —— 大怪档（TH18 侧只有 `GirlD01` 那 3 个点落在这一档）—— */
        trance_kill_fallback(5000, &T, &N);
        CHECK(T == 300 && N == 10);
        CHECK(trance_kill_spirit_count(0, T, N, 1) == 11);       /* 瞬杀 11（TH13 GirlC01 (10,400) 同量级）*/

        /* —— 分档线的两条边界（`>=` 归大档）—— */
        trance_kill_fallback(TRANCE_KILL_FB_HP_BIG - 1, &T, &N);
        CHECK(T == TRANCE_KILL_FB_T_SMALL && N == TRANCE_KILL_FB_N_SMALL);
        trance_kill_fallback(TRANCE_KILL_FB_HP_BIG, &T, &N);
        CHECK(T == TRANCE_KILL_FB_T_BIG && N == TRANCE_KILL_FB_N_BIG);

        /* —— 读不到 HP（`ev->enemy == NULL` / 脏数据）⇒ **小怪档**（保守：宁可少掉）—— */
        trance_kill_fallback(0, &T, &N);
        CHECK(T == TRANCE_KILL_FB_T_SMALL && N == TRANCE_KILL_FB_N_SMALL);
        trance_kill_fallback(-12345, &T, &N);
        CHECK(T == TRANCE_KILL_FB_T_SMALL && N == TRANCE_KILL_FB_N_SMALL);

        /* —— 不变式：大档一定不比小档小（同 alive 下的颗数单调）—— */
        {
            int32_t alive;
            CHECK(TRANCE_KILL_FB_T_BIG >= TRANCE_KILL_FB_T_SMALL);
            CHECK(TRANCE_KILL_FB_N_BIG >= TRANCE_KILL_FB_N_SMALL);
            for (alive = 0; alive <= 400; ++alive) {
                const int32_t big = trance_kill_spirit_count(alive, TRANCE_KILL_FB_T_BIG,
                                                             TRANCE_KILL_FB_N_BIG, 1);
                const int32_t small = trance_kill_spirit_count(alive, TRANCE_KILL_FB_T_SMALL,
                                                               TRANCE_KILL_FB_N_SMALL, 1);
                CHECK(big >= small);
                CHECK(small >= TRANCE_KILL_SPIRIT_MIN);
            }
        }
        /* —— 新兜底档一定落在夹取上界内（不会再像旧 `(120,5)` 那样贴到 64 附近）—— */
        CHECK(trance_kill_spirit_clamp(TRANCE_KILL_FB_N_BIG + 1) == TRANCE_KILL_FB_N_BIG + 1);
    }

    /* ============ 12c. ★★ boss 相位击破的**灵批次**（2026-09-13 第二轮）============
     *
     * 用户口径（#3/#4）：「**收/击破 boss 的符卡**会掉红灵（可能带绿灵），而**非符不掉**」
     *   ＋「道中 boss 击破符卡会掉**一大堆**灵」。
     * 一手（TH13，本机 `thecl -d 13` 全量重跑；`engine/spirit/th13/01` §3）：
     *   · 批次写在**符卡刚结束**的那个相位的序言里（`spellEnd();` 之后几行），且被
     *     `unless ($TIMEOUT == 0) goto skip;` 包着 ⇒ 超时一颗不掉、**非符相位的结束一颗不掉**；
     *   · stage boss：`dropExtra(10,1)+dropExtra(12,1)+dropExtra(13,20)` = **22 颗**（逐关 22..47）；
     *   · 道中 boss（`MBossDead`）：**5..25 颗**。
     * ★ 2026-09-14 起颗数**逐关查表**（不再用 TH18 的件数按比例换算），`phase_items` 只当去重判据。*/
    {
        /* —— 0 = 事件无效（计数全零：重复回调 / 没配掉落）—— 卡侧的去重判据就是这个 0 —— */
        CHECK(trance_boss_phase_spirit_count(1, 0, 0) == 0);
        CHECK(trance_boss_phase_spirit_count(4, 1, -1) == 0);
        CHECK(trance_boss_phase_spirit_count(7, 0, -9999) == 0);

        /* —— ★★ 2026-09-14：**逐关固定档**（一手 `thecl -d 13` 普查的颗数，
         *    见 `trance_core.h` 的那张表）；`phase_items` 只当"这是一次真掉落"的去重判据 —— */
        int32_t st;
        for (st = 1; st <= 7; ++st) {
            /* stage boss：一手 `Boss2` 序言那一批（22/22/22/32/37/32/47）*/
            const int32_t boss_expect[7]    = { 22, 22, 22, 32, 37, 32, 47 };
            /* 道中 boss：一手 `MBossDead`（5/9/24/19/19/19/25）*/
            const int32_t midboss_expect[7] = {  5,  9, 24, 19, 19, 19, 25 };
            CHECK(trance_boss_phase_spirit_count(st, 0, 21) == boss_expect[st - 1]);
            CHECK(trance_boss_phase_spirit_count(st, 1, 21) == midboss_expect[st - 1]);
            /* 件数完全不影响颗数（同一关同一类 ⇒ 同一个数），只影响"是不是一次真掉落" */
            CHECK(trance_boss_phase_spirit_count(st, 0, 1) == boss_expect[st - 1]);
            CHECK(trance_boss_phase_spirit_count(st, 0, 110) == boss_expect[st - 1]);
            CHECK(trance_boss_phase_spirit_count(st, 0, 100000) == boss_expect[st - 1]);
        }
        /* 去重判据：计数全零 / 负数 ⇒ 0（调用方直接返回），与关卡、类型无关 */
        CHECK(trance_boss_phase_spirit_count(1, 0, 0) == 0);
        CHECK(trance_boss_phase_spirit_count(4, 1, -5) == 0);
        /* 关卡号越界一律夹取（菜单/商店里 `CE_CURRENT_STAGE()` 可能是 0 或垃圾）*/
        CHECK(trance_boss_phase_spirit_count(0, 0, 21) == 22);
        CHECK(trance_boss_phase_spirit_count(-3, 1, 21) == 5);
        CHECK(trance_boss_phase_spirit_count(99, 0, 21) == 47);
        CHECK(trance_boss_phase_spirit_count(99, 1, 21) == 25);
        /* 一手区间：stage boss 22..47、道中 boss 5..25（用户口径 #4「一大堆」的观感就靠这条）*/
        for (st = 1; st <= 7; ++st) {
            CHECK(trance_boss_phase_spirit_count(st, 0, 21) >= 22);
            CHECK(trance_boss_phase_spirit_count(st, 1, 21) >= 5);
        }

        /* —— 构成（一手：1×type0 + 1×type2 + 其余 type3）—— */
        CHECK(trance_boss_phase_spirit_type(0, 22) == TRANCE_PHASE_TYPE_A);
        CHECK(trance_boss_phase_spirit_type(0, 22) == 0);
        CHECK(trance_boss_phase_spirit_type(1, 22) == TRANCE_PHASE_TYPE_B);
        CHECK(trance_boss_phase_spirit_type(1, 22) == 2);
        CHECK(trance_boss_phase_spirit_type(2, 22) == TRANCE_PHASE_TYPE_REST);
        CHECK(trance_boss_phase_spirit_type(2, 22) == 3);
        CHECK(trance_boss_phase_spirit_type(21, 22) == TRANCE_PHASE_TYPE_REST);
        CHECK(trance_boss_phase_spirit_type(0, 2) == TRANCE_PHASE_TYPE_REST);     /* 防呆：<3 颗全 type3 */
        CHECK(trance_boss_phase_spirit_type(1, 2) == TRANCE_PHASE_TYPE_REST);
        CHECK(trance_boss_phase_spirit_type(0, 1) == TRANCE_PHASE_TYPE_REST);
        /* 一批 22 颗的**槽值**：1×2 + 1×2 + 20×3 = 64 —— 与一手 TH13 每个相位的槽收益逐值相同 */
        {
            int32_t k, gauge = 0;
            for (k = 0; k < 22; ++k)
                gauge += trance_spirit_gauge_value(trance_boss_phase_spirit_type(k, 22), 0);
            CHECK(gauge == 64);
        }
    }

    /* ================= 13. 一台完整流程的端到端（槽 → 进 → 耗 → 收尾）================= */
    memset(&s, 0, sizeof s);
    trance_gauge_reset(&s.gauge);
    s.cooldown = TRANCE_COOLDOWN_FRAMES;
    for (i = 0; i < 300; ++i) trance_collect(&s, 0, NULL);      /* 300 颗：+2 each = 600（满槽）*/
    CHECK(s.gauge.gauge == TRANCE_GAUGE_MAX);
    CHECK(s.gauge.tier == TRANCE_GAUGE_TIERS);
    CHECK(s.spirits_seen[0] == 300);
    c = cond_all_ok();
    CHECK(trance_can_activate(&s, &c) == 1);
    CHECK(trance_begin(&s, 0, &c) == 1);
    for (i = 0; i < 600; ++i) tk = trance_tick(&s, i);           /* 600 帧 = 10.00 s */
    CHECK(tk.ended == 1 && tk.drain_floor == 1);
    CHECK(s.phase == TRANCE_PHASE_IDLE && s.gauge.gauge == 0);
    CHECK(s.activations == 1);
    /* 灵界里收的 3 颗不加槽（一手 `SPIRIT:326`）*/
    CHECK(trance_collect(&s, 0, &col) == 1);
    CHECK(col.gauge_add == 2 && s.gauge.gauge == 2);

    /* ================= 14. ★ 击破 → 神灵 → 槽（2026-09-12 接通的那条链的纯逻辑一半）=================
     * 卡侧的实际接线在 `cards/trance.c`：`on_enemy_death`（读敌人自己的 (T,N,alive) → 算颗数 →
     * 帧内 O(1) 入队）→ `on_item_frame` 帧末 `trance_kill_take()` → **每条按自己的颗数撒实体**
     * （`trance_spirit_spawn`）→ 自机走近才 `trance_on_spirit_collected()`。
     * ⚠ 本节测的是**记账那一半**（`trance_collect` 的槽账），**不是**「击破直接给槽」——
     *   2026-09-12 第二轮起击破**只撒实体**，槽要走近才涨（那条行为的不变式在下面 N.8 里钉）。
     * 本测把「收到一颗」照抄一遍（不碰引擎面）：**每颗 type 1 ⇒ 槽 +2**。
     * ★ 2026-09-13：**被击破的敌人不是每只都掉 1 颗了** —— 这里按一手普查最典型的
     *   「瞬杀小妖精 = 6 颗」(T=120,N=5) 走一遍，核对「一次次击破 = 6 颗 = +12 槽」这条乘法关系。
     * 手算：50 次击破 × 6 颗 × 2 槽 = 600 = 满槽（`TRANCE_GAUGE_MAX`）。*/
    {
        float ox[TRANCE_KILL_QUEUE_MAX], oy[TRANCE_KILL_QUEUE_MAX];
        int32_t on[TRANCE_KILL_QUEUE_MAX];
        int32_t kills = 0, spirits = 0;
        memset(&s, 0, sizeof s);
        trance_gauge_reset(&s.gauge);
        s.cooldown = TRANCE_COOLDOWN_FRAMES;
        while (kills < 50) {
            int32_t got, j, k, n;
            /* 卡侧那一步：敌人在出场第 0 帧被打死（T=120、N=5 ⇒ 6 颗）*/
            n = trance_kill_spirit_count(0, 120, 5, 1);
            CHECK(n == 6);
            trance_kill_note(&s, kills, (float)kills, 0.0f, n);       /* 一次击破（回调里只入队）*/
            ++kills;
            got = trance_kill_take(&s, kills, ox, oy, on, TRANCE_KILL_QUEUE_MAX);
            CHECK(got == 1);                                          /* 返回的是**击破次数** */
            for (j = 0; j < got; ++j) {
                for (k = 0; k < on[j]; ++k) {                         /* 帧末：每条按自己的颗数撒 */
                    CHECK(trance_collect(&s, TRANCE_KILL_SPIRIT_TYPE, &col) == 1);
                    ++spirits;
                }
            }
        }
        CHECK(spirits == 50 * 6);                        /* 300 颗（修复前是 50 颗：❓ 自定 1 颗/击破）*/
        CHECK(s.gauge.gauge == TRANCE_GAUGE_MAX);        /* ★ 槽真的被填满了（修复前这里恒 0）*/
        CHECK(s.gauge.tier == TRANCE_GAUGE_TIERS);
        CHECK(s.spirits_seen[TRANCE_KILL_SPIRIT_TYPE] == 300);   /* 都记在 type 1 上（不按「蓝/白」二分）*/
        CHECK(s.spirits_seen[0] == 0 && s.spirits_seen[2] == 0 && s.spirits_seen[3] == 0);
        c = cond_all_ok();
        CHECK(trance_can_activate(&s, &c) == 1);         /* ★ 满槽 + 8 条 ⇒ 可开灵界（修复前永远不行）*/
        CHECK(trance_begin(&s, 0, &c) == 1);
        CHECK(s.phase == TRANCE_PHASE_MANUAL);
        /* 灵界里继续击破：**不再加槽**（一手 `SPIRIT:326`），但照旧计数 ⇒ 槽只减不增 */
        CHECK(trance_collect(&s, TRANCE_KILL_SPIRIT_TYPE, &col) == 1);
        CHECK(col.gauge_add == 0 && s.gauge.gauge == TRANCE_GAUGE_MAX);
        tk = trance_tick(&s, 9999);
        CHECK(s.gauge.gauge == TRANCE_GAUGE_MAX - TRANCE_GAUGE_DRAIN);
    }

    /* ================= M. ★★ 中弹救援（**救援层的最后一家**，2026-09-12 第二版·落点订正版）=================
     *
     * 用户口径（唯一权威）：
     *   「灵界有两个触发方式，一个是手动触发；一个是**在确定中弹后，取消掉死亡，进入灵界状态；
     *     灵界状态退出后，重新死亡**。」
     *   ＋ 「113 这条机制能够确定说，它一定在**救援层的最后**出发，**甚至比决死 B 还靠后**」
     * ⇒ 顺序 107 → 117 → 决死 B → 113，落点 = **卡虚表 `+0x0c`**（`on_death_after_deathbomb`）。
     * 卡侧（`cards/trance.c`）只做「读引擎面 + 调那两个地址 + `return 1`」，
     * **能算错的判断全在这里** ⇒ 本节逐个钉门、钉窗口、钉链式。
     *
     * 用户点名的四条覆盖：
     *   ① 槽 < 200 ⇒ 不救；② 槽 ≥ 200 ⇒ 救且进 `TRANCE_PHASE_DEATH_FREE`；
     *   ③ 死亡强制窗口结束 ⇒ **报出「需要再死一次」**；④ 链式（用户定案允许）。
     *   ⚠ 两条**第一版**的写法已被推翻，这里改成正面断言：
     *     · 「正在重新死亡时不许再救」⇒ **允许链式**（原话「就这样，**允许链式**」）；
     *     · 「等帧点（`TRANCE_PENDING_FRAMES`）」⇒ **不可达**（`+0x0c` 与 `commit` 在同一 tick，
     *       `on_item_frame` 跑到时命已扣），那套常量/相位/函数已整体删除（`AUDIT` §BM7）。*/

    /* ---- M.1 相位只有三个取值（多一个就是第一版的排队相位没删干净）---- */
    CHECK(TRANCE_PHASE_IDLE == 0);
    CHECK(TRANCE_PHASE_MANUAL == 1);
    CHECK(TRANCE_PHASE_DEATH_FREE == 2);

    /* ---- M.2 `trance_state_init`：一张全新的卡（槽 0、空闲、无残留）---- */
    trance_state_init(&s);
    CHECK(s.phase == TRANCE_PHASE_IDLE);
    CHECK(s.gauge.gauge == 0 && s.gauge.tier == 0);
    CHECK(s.activations == 0 && s.deaths_rescued == 0);
    CHECK(s.chain_count == 0 && s.chain_timer == 0);
    CHECK(s.kill_pending == 0);
    trance_gauge_set(&s.gauge, 600);
    trance_state_init(&s);
    CHECK(s.gauge.gauge == 0);                         /* ★ 它连槽一起清零（= gauge_reset 的效果）*/

    /* ---- M.3 ① 槽 < 200 ⇒ **不救**（一手 `th13:0x405a40` 的门）---- */
    trance_state_init(&s);
    trance_gauge_set(&s.gauge, TRANCE_GAUGE_MIN_ENTER - 1);
    CHECK(trance_force_on_death(&s) == 0);             /* `+0x0c` 里拿到 0 ⇒ return 0 ⇒ 照常死 */
    CHECK(s.phase == TRANCE_PHASE_IDLE);
    CHECK(s.deaths_rescued == 0 && s.activations == 0);
    trance_gauge_set(&s.gauge, 0);
    CHECK(trance_force_on_death(&s) == 0);             /* 槽 0（窗口自然结束时就是它）*/

    /* ---- M.4 ② 槽 ≥ 200 ⇒ 救，且进 `TRANCE_PHASE_DEATH_FREE` ---- */
    trance_state_init(&s);
    trance_gauge_set(&s.gauge, TRANCE_GAUGE_MIN_ENTER);
    CHECK(trance_force_on_death(&s) == 1);             /* 恰好 200 ⇒ 成功（边界含）*/
    CHECK(s.phase == TRANCE_PHASE_DEATH_FREE);
    CHECK(s.deaths_rescued == 1 && s.activations == 1);
    CHECK(s.gauge.gauge == TRANCE_GAUGE_MIN_ENTER);    /* ★ 拿窗口**不消耗槽**（−1/帧 才是消耗）*/

    /* ---- M.5 ③ 窗口结束 ⇒ 报出「需要再死一次」（一手 `th13:0x405c3e`）----
     * 用户点名的那条：**保命是延时，不是免死** ⇒ 结束时必须给出「再死一次」的信号。
     * ⚠ 窗口长度 = **进入时的槽值**（一手：槽 −1/帧）⇒ 想造短窗口就把槽调到刚过门。*/
    trance_state_init(&s);
    trance_gauge_set(&s.gauge, TRANCE_GAUGE_MIN_ENTER);
    CHECK(trance_force_on_death(&s) == 1);
    for (i = 0; i < TRANCE_GAUGE_MIN_ENTER - 1; ++i) {
        tk = trance_tick(&s, i);
        if (tk.ended || tk.re_die) break;              /* 还没到点：不许提前报收尾 */
    }
    CHECK(i == TRANCE_GAUGE_MIN_ENTER - 1);
    CHECK(tk.ended == 0 && tk.re_die == 0);
    CHECK(s.gauge.gauge == 1);
    tk = trance_tick(&s, TRANCE_GAUGE_MIN_ENTER);
    CHECK(tk.ended == 1 && tk.ended_death_free == 1);
    CHECK(tk.re_die == 1);                             /* ★ 欠一次死亡 */
    CHECK(trance_should_redie(&tk) == 1);               /* ← 卡侧据此调 `Player__die` */
    CHECK(s.phase == TRANCE_PHASE_IDLE);
    CHECK(s.gauge.gauge == 0);
    /* 手动窗口结束**不**欠死亡（`re_die` 恒 0）⇒ 不会无缘无故再死一次 */
    trance_state_init(&s);
    trance_gauge_set(&s.gauge, TRANCE_GAUGE_MIN_ENTER);
    CHECK(trance_begin(&s, 0, &c) == 1);
    for (i = 0; i < TRANCE_GAUGE_MIN_ENTER; ++i) tk = trance_tick(&s, i);
    CHECK(tk.ended == 1 && tk.ended_death_free == 0);
    CHECK(tk.re_die == 0);
    CHECK(trance_should_redie(&tk) == 0);

    /* ---- M.6 ③b 一手链的另一半：**第二次 `die` 必然救不成**（槽已被扣到 0）----
     * 一手 `SPIRIT` §7：死亡触发的灵界结束时槽已逐帧扣到 0 ⇒ 第二次 `begin` 必然撞 `0x405a40`。
     * ⇒ 「延时」在纯逻辑层就该闭环：窗口走完 ⇒ 槽 0 ⇒ 想救也救不了 ⇒ 正常扣命。*/
    trance_state_init(&s);
    trance_gauge_set(&s.gauge, TRANCE_GAUGE_MAX);
    CHECK(trance_force_on_death(&s) == 1);
    for (i = 0; i < TRANCE_GAUGE_MAX; ++i) tk = trance_tick(&s, 1000 + i);
    CHECK(tk.re_die == 1);
    CHECK(s.gauge.gauge == 0);                         /* 槽被扣光 */
    CHECK(trance_force_on_death(&s) == 0);             /* 补死那次救不成（一手同一形状）*/

    /* ---- M.7 ★ **链式救援是允许的**（用户 2026-09-12 定案：「就这样，允许链式」）----
     * 唯一的天然上限 = 槽门：窗口里又攒回 ≥ 200 ⇒ 补死那次**照样能救**（再开一个窗口）。
     * ⚠ 第一版那条「re-dying 期间不许再救」是**错的**，这里正面钉死它已被删：
     *   本卡**没有任何**「已经救过」的状态位。*/
    trance_state_init(&s);
    trance_gauge_set(&s.gauge, TRANCE_GAUGE_MAX);
    CHECK(trance_force_on_death(&s) == 1);
    CHECK(s.deaths_rescued == 1);
    trance_end(&s, &tk);
    CHECK(tk.re_die == 1);                             /* 补死的欠账 */
    trance_gauge_set(&s.gauge, TRANCE_GAUGE_MAX);      /* 窗口里又攒回 ≥ 200 */
    CHECK(trance_force_on_death(&s) == 1);             /* ★ 照样能救（链式）*/
    CHECK(s.phase == TRANCE_PHASE_DEATH_FREE);
    CHECK(s.deaths_rescued == 2);
    /* 兜底：槽不够时链式也就到不了 —— 这证明「上限只有槽门」，不是靠标志 */
    trance_state_init(&s);
    trance_gauge_set(&s.gauge, TRANCE_GAUGE_MIN_ENTER - 1);
    CHECK(trance_force_on_death(&s) == 0);

    /* ---- M.8 已在灵界时再 `begin` = **成功且什么都不改**（★ 2026-09-22 一手复核后改写）----
     * 一手 `0x405a36 test byte ptr [esi+0x14],1` / `0x405a3a jne 0x405b64` ⇒ `0x405b64` 只有
     * `xor eax,eax; pop esi; …; ret 4`（返回 0 = 成功，**不碰 `this+0x14`**）。
     * 上一版读成「手动窗被中弹升格成死亡强制档」是把 `0x405a4c`（其实是「槽 < 200 ⇒ return −1」）
     * 误当位写 ⇒ owner 实跑看到「灵界里中弹 ⇒ 结束时死」（th13 #6）。*/
    trance_state_init(&s);
    trance_gauge_set(&s.gauge, TRANCE_GAUGE_MAX);
    CHECK(trance_begin(&s, 0, &c) == 1);
    CHECK(s.phase == TRANCE_PHASE_MANUAL && s.deaths_rescued == 0 && s.activations == 1);
    CHECK(trance_force_on_death(&s) == 1);             /* 窗里中弹 ⇒ 成功（被吃下）*/
    CHECK(s.phase == TRANCE_PHASE_MANUAL);             /* ★ 不升格 */
    CHECK(s.deaths_rescued == 0 && s.activations == 1);/* ★ 不记账、不另算一次 begin */
    /* 已经在死亡强制档里再被中弹（同一帧第二次撞弹）⇒ 同样「成功 + 原样不动」。*/
    trance_state_init(&s);
    trance_gauge_set(&s.gauge, TRANCE_GAUGE_MAX);
    CHECK(trance_force_on_death(&s) == 1);
    CHECK(s.phase == TRANCE_PHASE_DEATH_FREE && s.deaths_rescued == 1 && s.activations == 1);
    CHECK(trance_force_on_death(&s) == 1);
    CHECK(s.deaths_rescued == 1 && s.activations == 1);
    /* 手动窗结束 ⇒ 一手 `end`：非死亡档 ⇒ 补扣 200、**不欠死**（owner th13 #6 的口径）*/
    trance_state_init(&s);
    trance_gauge_set(&s.gauge, TRANCE_GAUGE_MAX);
    trance_begin(&s, 0, &c);
    trance_force_on_death(&s);
    trance_end(&s, &tk);
    CHECK(tk.re_die == 0 && tk.ended_death_free == 0);
    CHECK(s.gauge.gauge == TRANCE_GAUGE_MAX - TRANCE_GAUGE_END_PENALTY);
    /* 槽不够**且本来不在灵界** ⇒ 救不成（一手 `0x405a4c` 的 `return −1` 那条路）*/
    trance_state_init(&s);
    trance_gauge_set(&s.gauge, TRANCE_GAUGE_MIN_ENTER - 1);
    CHECK(trance_force_on_death(&s) == 0);
    CHECK(s.phase == TRANCE_PHASE_IDLE && s.deaths_rescued == 0);

    /* ---- M.9 一条完整的「中弹 → 救 → 补死」账（卡侧 `+0x0c` 回调的纯逻辑镜像）----
     * 卡侧的序列（`cards/trance.c` 的 `on_death_after_deathbomb` / `on_item_frame` ③）：
     *   门① `ce_rescue_note_owner()`（引擎面，本文件测不了）⇒ 门② `trance_force_on_death()`
     *   ⇒ `cancel_impending_death()` + **`return 1`**
     *   窗口走完 ⇒ `trance_tick` 报 `re_die` ⇒ `trance_should_redie()` ⇒ 卡调 `Player__die`*/
    trance_state_init(&s);
    trance_gauge_set(&s.gauge, 300);                   /* 槽 300 ⇒ 窗口 300 帧 */
    CHECK(trance_force_on_death(&s) == 1);             /* ← 卡在这里 `cancel` + `return 1` */
    CHECK(s.phase == TRANCE_PHASE_DEATH_FREE);
    CHECK(s.gauge.gauge == 300);
    for (i = 0; i < 300; ++i) tk = trance_tick(&s, 600 + i);
    CHECK(tk.ended == 1 && tk.re_die == 1);
    CHECK(s.gauge.gauge == 0);                         /* 300 帧窗口走完 ⇒ 槽 0 */
    CHECK(trance_should_redie(&tk) == 1);               /* ← 卡在这里调 `Player__die` */
    CHECK(trance_force_on_death(&s) == 0);             /* 补死那次：槽 0 ⇒ 救不成（链式到此为止）*/

    /* ---- M.10 NULL / 越界入参（纯逻辑层的防御面）---- */
    CHECK(trance_should_redie(NULL) == 0);
    CHECK(trance_force_on_death(NULL) == 0);
    trance_state_init(NULL);
    tk.re_die = 0;
    CHECK(trance_should_redie(&tk) == 0);
    tk.re_die = 1;
    CHECK(trance_should_redie(&tk) == 1);

    /* ================= N. ★ 神灵实体池（自建 VM 池）的纯策略 =================
     *
     * 用户 2026-09-12 裁决：神灵 ≈ 下落道具，但**1 不下落 2 全屏收点收不走、必须跟过去吃
     * 3 随时间不断变小到点消失 4 不是敌人**。
     * 卡侧（`cards/trance.c` 的 `s_spirit[]`）只做「建 / 删 anm VM + 把整数喂给引擎」，
     * **能算错的全在这几个 core 函数里** ⇒ 本节逐个钉端点与边界。 */

    /* ---- N.1 常量（改常量忘改曲线 ⇒ 立刻红）---- */
    CHECK(TRANCE_SPIRIT_POOL == 128);                 /* ❓ 自定（一手 TH13 是 256 槽）*/
    CHECK(TRANCE_SPIRIT_LIFE_FRAMES == 480);          /* 一手 `th13:0x438a61`：CMP [EBX+0x60],0x1e0 */
    CHECK(TRANCE_SPIRIT_SCALE_START_PERMILLE == 1500);/* 一手 `-1: scale(1.5,1.5)`（脚本原文）*/
    CHECK(TRANCE_SPIRIT_SCALE_PERMILLE_BASE == 1000); /* ❓ 我们写的基数（实跑订正 #4：不是 1.5）*/
    CHECK(TRANCE_SPIRIT_SCALE_END_PERMILLE == 400);   /* 一手 `+200: scaleTime(200, 0, 0.4, 0.4)` */
    CHECK(TRANCE_SPIRIT_SCALE_DELAY == 200);          /* ★ 一手 `+200:`：**前 200 帧不缩** */
    CHECK(TRANCE_SPIRIT_SCALE_FRAMES == 200);
    CHECK(TRANCE_SPIRIT_SCRIPT_TAIL_FRAMES == 40);    /* 一手 `interruptLabel(1)` → `+40: delete()` */
    CHECK(TRANCE_SPIRIT_PICK_RADIUS == 24);           /* ❓ 自定（一手 TH13 收集半径 = 20 px）*/
    CHECK(TRANCE_SPIRIT_PICK_RADIUS < 30);            /* ★ 比引擎道具的 30 px 收点半径更小 */
    CHECK(TRANCE_SPIRIT_BOB_PX == 1);
    CHECK(TRANCE_SPIRIT_BOB_PERIOD == 32);
    CHECK(TRANCE_SPIRIT_SCALE_FRAMES < TRANCE_SPIRIT_LIFE_FRAMES);  /* 缩完还有一段时间是 0.4 */

    /* ---- N.2 缩小曲线（裁决 3：不断变小；★ 2026-09-14 相位按一手订正）----
     * 手算：BASE 1000、END 400、**DELAY 200**、FRAMES 200 ⇒
     *   前 200 帧恒 1000（一手 `+200:` 之前不缩）；
     *   之后每帧 −600/200 = −3 千分点（向零截断）：
     *     age 200 ⇒ 1000（起点，仍最大）· age 201 ⇒ 997 · age 300 ⇒ 700 ·
     *     age 399 ⇒ 403 · age 400 ⇒ 400（端点照抄一手）· age > 400 ⇒ 保持 400。
     * ⚠ 上一版从 age 0 就开始缩（age 200 就已经 400）—— 那正是用户口径 #5「消失得快」的根因。*/
    CHECK(trance_spirit_scale_permille(0) == 1000);
    CHECK(trance_spirit_scale_permille(1) == 1000);                           /* ★ 前 200 帧不许变小 */
    CHECK(trance_spirit_scale_permille(100) == 1000);
    CHECK(trance_spirit_scale_permille(199) == 1000);
    CHECK(trance_spirit_scale_permille(TRANCE_SPIRIT_SCALE_DELAY) == 1000);   /* age 200 = 一手 `+200:` */
    CHECK(trance_spirit_scale_permille(201) == 997);
    CHECK(trance_spirit_scale_permille(300) == 700);                          /* 走到插值中段 */
    CHECK(trance_spirit_scale_permille(399) == 403);
    CHECK(trance_spirit_scale_permille(400) == 400);                          /* age 400 = 一手缩完那一刻 */
    CHECK(trance_spirit_scale_permille(401) == 400);
    CHECK(trance_spirit_scale_permille(TRANCE_SPIRIT_LIFE_FRAMES) == 400);   /* 到期那一刻仍是 0.4 */
    CHECK(trance_spirit_scale_permille(100000) == 400);
    CHECK(trance_spirit_scale_permille(-1) == 1000);                          /* 负 age 当出生那一刻 */
    {   /* 单调不增（裁决 3 的「不断变小」= 这条不变式），且**前 200 帧持平** */
        int32_t a2, prev = trance_spirit_scale_permille(0);
        int mono = 1, held = 1;
        for (a2 = 1; a2 <= TRANCE_SPIRIT_SCALE_DELAY + TRANCE_SPIRIT_SCALE_FRAMES; ++a2) {
            int32_t cur = trance_spirit_scale_permille(a2);
            if (cur > prev) mono = 0;
            if (a2 <= TRANCE_SPIRIT_SCALE_DELAY && cur != TRANCE_SPIRIT_SCALE_PERMILLE_BASE) held = 0;
            prev = cur;
        }
        CHECK(mono == 1);
        CHECK(held == 1);
    }

    /* ---- N.3 原地微动（裁决 1：不下落；微动是我们定的 ❓ 自定 ±1 px 方波）----
     * 周期 32、一季 8 帧：0..7 → 0 · 8..15 → −1 · 16..23 → 0 · 24..31 → +1 */
    CHECK(trance_spirit_bob_offset(0) == 0);
    CHECK(trance_spirit_bob_offset(7) == 0);
    CHECK(trance_spirit_bob_offset(8) == -1);
    CHECK(trance_spirit_bob_offset(15) == -1);
    CHECK(trance_spirit_bob_offset(16) == 0);
    CHECK(trance_spirit_bob_offset(23) == 0);
    CHECK(trance_spirit_bob_offset(24) == 1);
    CHECK(trance_spirit_bob_offset(31) == 1);
    CHECK(trance_spirit_bob_offset(32) == 0);          /* 整周期回到起点 */
    CHECK(trance_spirit_bob_offset(63) == 1);
    CHECK(trance_spirit_bob_offset(64) == 0);
    CHECK(trance_spirit_bob_offset(-3) == 0);
    {   /* 微动幅度恒 ≤ BOB_PX（不许把实体「抖」到别的地方去）*/
        int32_t a2, ok = 1;
        for (a2 = 0; a2 < TRANCE_SPIRIT_BOB_PERIOD * 3; ++a2) {
            int32_t o = trance_spirit_bob_offset(a2);
            if (o > TRANCE_SPIRIT_BOB_PX || o < -TRANCE_SPIRIT_BOB_PX) ok = 0;
        }
        CHECK(ok == 1);
    }

    /* ---- N.4 到期判定（裁决 3：到点消失）---- */
    CHECK(trance_spirit_expired(TRANCE_SPIRIT_LIFE_FRAMES) == 0);
    CHECK(trance_spirit_expired(2) == 0);
    CHECK(trance_spirit_expired(1) == 0);          /* 还剩 1 帧 ⇒ 还活着 */
    CHECK(trance_spirit_expired(0) == 1);          /* ← 边界：0 就是到点 */
    CHECK(trance_spirit_expired(-1) == 1);

    /* ---- N.5 吃取半径（裁决 2：必须跟过去吃）----
     * 手算（r = 24、r² = 576）：
     *   (24,0) ⇒ 576 ≤ 576 **命中**（边界含）· (25,0) ⇒ 625 > 576 不命中
     *   (16,16) ⇒ 512 命中 · (17,17) ⇒ 578 不命中 · (20,13) ⇒ 569 命中 · (20,14) ⇒ 596 不命中 */
    CHECK(trance_spirit_in_pickup_range(0, 0) == 1);
    CHECK(trance_spirit_in_pickup_range(TRANCE_SPIRIT_PICK_RADIUS, 0) == 1);
    CHECK(trance_spirit_in_pickup_range(-TRANCE_SPIRIT_PICK_RADIUS, 0) == 1);
    CHECK(trance_spirit_in_pickup_range(0, TRANCE_SPIRIT_PICK_RADIUS) == 1);
    CHECK(trance_spirit_in_pickup_range(TRANCE_SPIRIT_PICK_RADIUS + 1, 0) == 0);
    CHECK(trance_spirit_in_pickup_range(0, -(TRANCE_SPIRIT_PICK_RADIUS + 1)) == 0);
    CHECK(trance_spirit_in_pickup_range(16, 16) == 1);
    CHECK(trance_spirit_in_pickup_range(17, 17) == 0);
    CHECK(trance_spirit_in_pickup_range(20, 13) == 1);
    CHECK(trance_spirit_in_pickup_range(20, 14) == 0);
    CHECK(trance_spirit_in_pickup_range(-17, -17) == 0);
    CHECK(trance_spirit_in_pickup_range(1000, 1000) == 0);

    /* ---- N.6 容量（❓ 自定 128）---- */
    CHECK(trance_spirit_pool_room(0) == TRANCE_SPIRIT_POOL);
    CHECK(trance_spirit_pool_room(1) == TRANCE_SPIRIT_POOL - 1);
    CHECK(trance_spirit_pool_room(TRANCE_SPIRIT_POOL - 1) == 1);
    CHECK(trance_spirit_pool_room(TRANCE_SPIRIT_POOL) == 0);        /* 满了 ⇒ 卡侧丢尾 + 计数 */
    CHECK(trance_spirit_pool_room(TRANCE_SPIRIT_POOL + 99) == 0);
    CHECK(trance_spirit_pool_room(-5) == TRANCE_SPIRIT_POOL);       /* 负值当 0 */

    /* ---- N.7 入槽值换算（一颗值多少槽；真实入账仍走 trance_collect）---- */
    CHECK(trance_spirit_gauge_value(0, 0) == 2);
    CHECK(trance_spirit_gauge_value(1, 0) == 2);
    CHECK(trance_spirit_gauge_value(2, 0) == 2);
    CHECK(trance_spirit_gauge_value(3, 0) == 3);                    /* type 3 再 +1（一手 `0x438c3d`）*/
    CHECK(trance_spirit_gauge_value(4, 0) == 2);                    /* type 4 = item 14 ⇒ 与 1 同奖 */
    CHECK(trance_spirit_gauge_value(0, 1) == 0);                    /* 灵界中不加槽（`SPIRIT:326`）*/
    CHECK(trance_spirit_gauge_value(3, 1) == 0);
    CHECK(trance_spirit_gauge_value(99, 0) == 0);                   /* 越界 */
    CHECK(trance_spirit_gauge_value(-1, 0) == 0);
    CHECK(trance_spirit_gauge_value(TRANCE_KILL_SPIRIT_TYPE, 0)
          == TRANCE_REWARD_GAUGE_ALWAYS);                           /* 击破通道那一颗 = +2 */

    /* ---- N.8 「击破 → 撒实体 → 走近才收」的整数模拟（照抄卡侧 on_item_frame ⑤⑥ 的循环）----
     * 模型：4 次击破 ⇒ 帧末 take ⇒ 池里 4 颗（x = 100/200/300/400，y = 0，自机从 x = 0 出发）。
     * ★ **本节的中心断言**：只击破、自机离得远 ⇒ **槽一点不涨**（旧版「帧末直接入账」在这里会 +8，
     *   那正是本次要拆掉的那条兜底）——必须走过去才涨。 */
    {
        float   ex[4], ey[4];       /* 每颗的出生点（世界坐标）*/
        int32_t eage[4], elife[4];
        int32_t alive = 0, picked = 0, expired = 0, spawned = 0;
        int32_t px = 0, py = 0;     /* 自机（整数像素）*/
        int32_t k, i2;

        memset(&s, 0, sizeof s);
        trance_gauge_reset(&s.gauge);
        s.cooldown = TRANCE_COOLDOWN_FRAMES;

        for (k = 0; k < 4; ++k) {                       /* 四次击破（回调里只入队）*/
            /* ★ 2026-09-13：这里刻意让每次击破**只掉 1 颗**（拖满窗口那一档）——
             *   本节要钉的是「只击破不走近 ⇒ 槽不涨」，颗数那一条在第 12b 节测，
             *   本节的池/距离模型按 1 颗跑最简单（帧末的循环形状与卡侧逐字相同）。*/
            int32_t on1[1];
            trance_kill_note(&s, k, (float)(100 + k * 100), 0.0f, 1);
            CHECK(trance_kill_take(&s, k, ex + alive, ey + alive, on1, 4) == 1);
            CHECK(on1[0] == 1);                         /* 该条自己的颗数被抄回来了 */
            CHECK(trance_spirit_pool_room(alive) > 0);  /* 撒之前先看有没有位（卡侧就是这么判的）*/
            eage[alive] = 0;
            elife[alive] = TRANCE_SPIRIT_LIFE_FRAMES;
            ++alive;
            ++spawned;
        }
        CHECK(spawned == 4 && alive == 4);
        CHECK(s.gauge.gauge == 0);                      /* ★ 还没走过去 ⇒ 槽 0（击破本身不给槽）*/
        CHECK(s.spirits_seen[TRANCE_KILL_SPIRIT_TYPE] == 0);

        /* 自机一帧走 20 px、走到第 1 颗跟前（x = 100）就走了 5 帧；那一帧 dist² = 0 ⇒ 吃到 */
        for (k = 0; k < 5; ++k) {
            px += 20;
            for (i2 = 0; i2 < alive; ) {
                ++eage[i2];
                --elife[i2];
                if (trance_spirit_expired(elife[i2])) {          /* 到点：卡侧在这里 interrupt + 回收 */
                    ex[i2] = ex[alive - 1]; ey[i2] = ey[alive - 1];
                    eage[i2] = eage[alive - 1]; elife[i2] = elife[alive - 1];
                    --alive; ++expired;
                    continue;
                }
                if (trance_spirit_in_pickup_range(px - (int32_t)ex[i2], py - (int32_t)ey[i2])) {
                    CHECK(trance_collect(&s, TRANCE_KILL_SPIRIT_TYPE, &col) == 1);   /* = 卡侧入账那一步 */
                    CHECK(col.gauge_add == TRANCE_GAUGE_PER_SPIRIT);                /* 每颗 +2 */
                    ex[i2] = ex[alive - 1]; ey[i2] = ey[alive - 1];
                    eage[i2] = eage[alive - 1]; elife[i2] = elife[alive - 1];
                    --alive; ++picked;
                    continue;
                }
                ++i2;
            }
        }
        CHECK(picked == 1);                     /* 只吃到走到跟前的那一颗（其余在 200/300/400）*/
        CHECK(s.gauge.gauge == TRANCE_GAUGE_PER_SPIRIT);        /* 槽 = 2（不是 8）*/
        CHECK(s.spirits_seen[TRANCE_KILL_SPIRIT_TYPE] == 1);
        CHECK(alive == 3);

        /* 自机站着不动，把那 3 颗放到期（寿命 480 帧）⇒ 全部消失，且**一颗都不入账** */
        for (k = 0; k < TRANCE_SPIRIT_LIFE_FRAMES; ++k) {
            int32_t alive_prev = alive;
            for (i2 = 0; i2 < alive; ) {
                ++eage[i2];
                --elife[i2];
                if (trance_spirit_expired(elife[i2])) {
                    ex[i2] = ex[alive - 1]; ey[i2] = ey[alive - 1];
                    eage[i2] = eage[alive - 1]; elife[i2] = elife[alive - 1];
                    --alive; ++expired;
                    continue;
                }
                ++i2;
            }
            if (alive == 0 && alive_prev > 0) break;
        }
        CHECK(alive == 0);
        CHECK(expired == 3);
        CHECK(picked == 1);                                     /* 到期**不等于**被收 */
        CHECK(s.gauge.gauge == TRANCE_GAUGE_PER_SPIRIT);        /* 槽没再涨 */
    }

    /* ================= ★ 神灵运动 state 机（2026-09-13 新增；一手 th13:0x438730）=================
     * 三条边界逐条钉：96² 进 / 128² 出（迟滞）/ 112² 灵梦慢吸转快吸；灵界态只额外使能 state 4。*/
    {
        const int32_t IN = TRANCE_ATTRACT_IN2, OUT = TRANCE_ATTRACT_OUT2, RM = TRANCE_ATTRACT_REIMU_IN2;

        /* 常量自洽：三个半径就是 96²/128²/112²，且 进 < 灵梦 < 出（迟滞区间非空）*/
        CHECK(IN == 96 * 96 && OUT == 128 * 128 && RM == 112 * 112);
        CHECK(IN < RM && RM < OUT);

        /* 漂移 → 被吸：**严格小于** 96²；正好 96² 不吸（一手 fcomp 的方向）*/
        CHECK(trance_spirit_state_step(TRANCE_STATE_DRIFT, IN - 1, 0, 0) == TRANCE_STATE_ATTRACT);
        CHECK(trance_spirit_state_step(TRANCE_STATE_DRIFT, IN, 0, 0) == TRANCE_STATE_DRIFT);
        CHECK(trance_spirit_state_step(TRANCE_STATE_DRIFT, IN + 1, 0, 0) == TRANCE_STATE_DRIFT);

        /* 被吸 → 漂移：**大于等于** 128² 才脱离（迟滞：96²..128² 之间保持被吸）*/
        CHECK(trance_spirit_state_step(TRANCE_STATE_ATTRACT, OUT - 1, 0, 0) == TRANCE_STATE_ATTRACT);
        CHECK(trance_spirit_state_step(TRANCE_STATE_ATTRACT, OUT, 0, 0) == TRANCE_STATE_DRIFT);
        /* ★ 迟滞的实质：把"进"的半径从 96 放宽到 128 —— 同一距离在两种 state 下结论不同 */
        CHECK(trance_spirit_state_step(TRANCE_STATE_DRIFT, 100 * 100, 0, 0) == TRANCE_STATE_DRIFT);
        CHECK(trance_spirit_state_step(TRANCE_STATE_ATTRACT, 100 * 100, 0, 0) == TRANCE_STATE_ATTRACT);

        /* 灵梦 + 灵界态：96² 之外进 state 4（任意距离慢吸）；**缺一个条件都不行** */
        CHECK(trance_spirit_state_step(TRANCE_STATE_DRIFT, IN + 1, 1, 1) == TRANCE_STATE_REIMU_SLOW);
        CHECK(trance_spirit_state_step(TRANCE_STATE_DRIFT, IN + 1, 1, 0) == TRANCE_STATE_DRIFT); /* 非灵梦 */
        CHECK(trance_spirit_state_step(TRANCE_STATE_DRIFT, IN + 1, 0, 1) == TRANCE_STATE_DRIFT); /* 非灵界态 */
        /* 灵梦慢吸**不会**因为太远而放弃（一手：state 4 里没有"太远就退出"那条）*/
        CHECK(trance_spirit_state_step(TRANCE_STATE_REIMU_SLOW, OUT * 4, 1, 1) == TRANCE_STATE_REIMU_SLOW);
        CHECK(trance_spirit_state_step(TRANCE_STATE_REIMU_SLOW, RM - 1, 1, 1) == TRANCE_STATE_ATTRACT);
        CHECK(trance_spirit_state_step(TRANCE_STATE_REIMU_SLOW, RM, 1, 1) == TRANCE_STATE_REIMU_SLOW);

        /* 异常/越界 state 一律归一到漂移（我们的池不走 state 3/5）*/
        CHECK(trance_spirit_state_step(0, IN - 1, 0, 0) == TRANCE_STATE_ATTRACT);
        CHECK(trance_spirit_state_step(3, IN + 1, 0, 0) == TRANCE_STATE_DRIFT);
        CHECK(trance_spirit_state_step(5, IN + 1, 0, 0) == TRANCE_STATE_DRIFT);
        CHECK(trance_spirit_state_step(99, IN + 1, 0, 0) == TRANCE_STATE_DRIFT);
        /* 负的平方距离（入参错误）**钳成 0** ⇒ 当"贴脸"处理（宁可吸过来，也不要卡在漂移态）*/
        CHECK(trance_spirit_state_step(TRANCE_STATE_DRIFT, -1, 0, 0) == TRANCE_STATE_ATTRACT);

        /* 出界框（一手 x∈[−224,224]、y∈[−32,480]）：边界含在界内 */
        CHECK(trance_spirit_out_of_field(0, 224) == 0);
        CHECK(trance_spirit_out_of_field(-224, 0) == 0);
        CHECK(trance_spirit_out_of_field(224, 480) == 0);
        CHECK(trance_spirit_out_of_field(0, -32) == 0);
        CHECK(trance_spirit_out_of_field(-225, 0) == 1);
        CHECK(trance_spirit_out_of_field(225, 0) == 1);
        CHECK(trance_spirit_out_of_field(0, -33) == 1);
        CHECK(trance_spirit_out_of_field(0, 481) == 1);
    }

    /* ================= ★ 机体特技（2026-09-13 用户口径）=================
     * 三行照一手（灵梦慢吸 / 魔理沙无 / 早苗 type1 +1）、一行是**本仓合成规则**（咲夜：时间 ×2）。*/
    {
        trance_state_t st;
        const trance_trait_t *t;

        /* 表本身：四行、角色号 0..3、名字不空；越界回落到"全中性"那一行（不是 NULL）*/
        t = trance_traits(0);  CHECK(t && t->character == 0 && t->reimu_slow_attract == 1);
        t = trance_traits(1);  CHECK(t && t->character == 1 && t->reimu_slow_attract == 0);
        t = trance_traits(2);  CHECK(t && t->character == 2 && t->drain_div == 2);
        t = trance_traits(3);  CHECK(t && t->character == 3 && t->sanae_type1_bonus == 1);
        t = trance_traits(99); CHECK(t && t->drain_div == 1 && t->spirit_life_permille == 1000);
        t = trance_traits(-5); CHECK(t && t->sanae_type1_bonus == 0);

        /* ★ 四行各自**只**点亮自己那一条特技（防止手抄串行 —— 这类表最容易复制粘贴错）*/
        t = trance_traits(0); CHECK(t->reimu_slow_attract == 1 && t->marisa_trance_buff == 0 && t->drain_div == 1);
        t = trance_traits(1); CHECK(t->marisa_trance_buff == 1 && t->reimu_slow_attract == 0 && t->drain_div == 1);
        t = trance_traits(2); CHECK(t->drain_div == 2 && t->marisa_trance_buff == 0 && t->reimu_slow_attract == 0);
        t = trance_traits(3); CHECK(t->sanae_type1_bonus == 1 && t->marisa_trance_buff == 0 && t->drain_div == 1);
        /* 判定点：只有灵梦那条为 1（TH18 原生四机体统一 ⇒ 这一位是"由我们缩小"的唯一开关）*/
        CHECK(trance_traits(0)->small_hitbox == 1);
        CHECK(trance_traits(1)->small_hitbox == 0);
        CHECK(trance_traits(2)->small_hitbox == 0);
        CHECK(trance_traits(3)->small_hitbox == 0);
        CHECK(trance_traits(99)->small_hitbox == 0);   /* 越界回落行也不许缩 */

        /* 灵梦 ≠ 早苗 ≠ 咲夜：三行互不相同（防止手抄串行）*/
        CHECK(trance_traits(0)->reimu_slow_attract == 1 && trance_traits(0)->sanae_type1_bonus == 0);
        CHECK(trance_traits(3)->reimu_slow_attract == 0 && trance_traits(3)->drain_div == 1);

        /* ★ 咲夜的时间 ×2 —— **两个后果都要成立**：灵界窗口翻倍 + 神灵寿命翻倍 */
        memset(&st, 0, sizeof st);
        trance_set_character(&st, 2);
        CHECK(trance_spirit_life_frames(&st) == TRANCE_SPIRIT_LIFE_FRAMES * 2);
        /* 槽为 0 ⇒ 剩余时间 0（空槽不是"负时间"）*/
        CHECK(trance_remaining_centisec(&st) == 0);
        st.gauge.gauge = TRANCE_GAUGE_MAX;                        /* 满槽 600 */
        CHECK(trance_remaining_centisec(&st) == 2000);            /* 咲夜：1200 帧 = 20.00 s */
        trance_set_character(&st, 0);
        CHECK(trance_remaining_centisec(&st) == 1000);            /* 一手：600 帧 = 10.00 s */
        /* 真跑一遍：满槽开灵界，数到"槽归零"用了多少帧 —— 灵梦 600、咲夜 1200 */
        {
            int32_t ch;
            for (ch = 0; ch < 4; ++ch) {
                int32_t frames = 0;
                trance_state_t run;
                trance_cond_t cond;
                memset(&run, 0, sizeof run);
                trance_set_character(&run, ch);
                trance_gauge_reset(&run.gauge);
                trance_gauge_add(&run.gauge, TRANCE_GAUGE_MAX);   /* 直接填满 */
                memset(&cond, 0, sizeof cond);
                cond.full = 1; cond.enemies = 1; cond.cooldown = 1; cond.game_flags_ok = 1;
                cond.auto_mode = 1;
                CHECK(trance_begin(&run, 0, &cond) == 1);
                while (run.phase != TRANCE_PHASE_IDLE && frames < 5000) {
                    (void)trance_tick(&run, frames);
                    ++frames;
                }
                CHECK(frames == (ch == 2 ? TRANCE_DURATION_FRAMES_MAX * 2 : TRANCE_DURATION_FRAMES_MAX));
            }
        }
        /* 一手三行：角色 0/1/3 的寿命与窗口都不许被改到 */
        memset(&st, 0, sizeof st);
        trance_set_character(&st, 0); CHECK(trance_spirit_life_frames(&st) == TRANCE_SPIRIT_LIFE_FRAMES);
        trance_set_character(&st, 3); CHECK(trance_spirit_life_frames(&st) == TRANCE_SPIRIT_LIFE_FRAMES);
        /* 幂等 + 换人会清相位累加器 */
        st.drain_phase = 1;
        trance_set_character(&st, 3);
        CHECK(st.drain_phase == 1);                                 /* 同一个号：不动任何东西 */
        trance_set_character(&st, 2);
        CHECK(st.drain_phase == 0);
        /* NULL 安全 */
        trance_set_character(NULL, 2);
        CHECK(trance_spirit_life_frames(NULL) == TRANCE_SPIRIT_LIFE_FRAMES);
        CHECK(trance_remaining_centisec(NULL) == 0);
    }

    /* ================= 汇总 ================= */
    printf("test_trance_core: %d passed, %d failed (%d assertions)\n", s_pass, s_fail, s_pass + s_fail);
    return s_fail ? 1 : 0;
}
