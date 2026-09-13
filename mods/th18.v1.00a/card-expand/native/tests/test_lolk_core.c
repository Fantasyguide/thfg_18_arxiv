/* test_lolk_core.c —— lolk_core.c 的主机单测（make test-host）。
 * 语义：**一段攻击 = 一波**（非符一波、符卡一波、进 boss 一波）；道中波边界 =
 * 关卡脚本 ECL 子程序边界 + 刷出（TH15 用 524 切章声明，TH18 退到子程序号，见 th18/03）。
 * 击破率三来源（用户 2026-09-09 定案）：道中 = 击破/总数（分母 0 → 100％）；
 * boss 非符/符卡 = 击破 100％ / 超时 0％；时符恒 100％。
 * 覆盖：开波/关波/相位边沿/道中切章/道中子程序边界/退路/超时与击破/时符/空波/公式/碎片/减速/重置。 */
#include <stdio.h>
#include <string.h>

#include "../cards/lolk_core.h"

static int s_fail, s_pass;
#define CHECK(cond) do { if (cond) ++s_pass; else { ++s_fail; printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } } while (0)

/* 相位信号的默认值：道中、没有 boss、关卡脚本子程序 0、没有刷出 */
static void in_mid(lolk_in_t *in, int32_t chapter)
{
    memset(in, 0, sizeof *in);
    in->chapter = chapter;
}

/* boss 相位：boss_present + 相位指纹 + 符卡位。
 * `stage_chapter = -1`：真实 boss 战里关卡脚本停在 `MainBoss`（非 MainSub），
 * 道中章判定必须不参与。*/
static void in_boss(lolk_in_t *in, int32_t sig, int32_t spell, int32_t survival, int32_t chapter)
{
    memset(in, 0, sizeof *in);
    in->boss_present = 1;
    in->phase_sig = sig;
    in->spell_active = spell;
    in->survival = survival;
    in->chapter = chapter;
    in->stage_chapter = -1;
}

/* 静默推进 n 帧：波边沿有 LOLK_DEBOUNCE 帧去抖，换相位前先让它过期 */
static void advance(lolk_state_t *s, lolk_in_t *in, lolk_settle_t *out, int n)
{
    for (int i = 0; i < n; ++i) {
        lolk_tick(s, in, out);
    }
}

int main(void)
{
    lolk_state_t s;
    lolk_settle_t out;
    lolk_in_t in;
    int32_t f;

    /* 道中：第一个刷出开第一波（不再靠「章号变」开波，见 lolk_core.c 的注释） */
    lolk_reset(&s);
    CHECK(s.slow == LOLK_SLOW_FULL);
    in_mid(&in, 0);
    in.stage_sub = 0;
    in.spawn_delta = 1;
    CHECK(lolk_tick(&s, &in, &out) == LOLK_EV_WAVE_START);
    in.spawn_delta = 0;
    CHECK(s.wave_open == 1 && s.wave_kind == LOLK_KIND_MID);
    CHECK(lolk_tick(&s, &in, &out) == LOLK_EV_NONE);          /* 同章不切波 */

    /* 道中计数：击破/总数 */
    lolk_add_graze(&s, 5);
    lolk_add_spawned(&s, 4);
    lolk_add_killed(&s, 3);
    CHECK(s.graze == 5 && s.spawned == 4 && s.killed == 3);

    /* 道中切章：MainSub 编号变（0 → 1）+ 刷出 → 结算旧波 + 开新波；rate = 3/4 = 75 */
    advance(&s, &in, &out, LOLK_DEBOUNCE + 1);
    in.stage_chapter = 1;
    in.spawn_delta = 1;
    memset(&out, 0xcc, sizeof out);
    CHECK(lolk_tick(&s, &in, &out) == (LOLK_EV_WAVE_END | LOLK_EV_WAVE_START));
    CHECK(out.wave_index == 1 && out.kind == LOLK_KIND_MID);
    CHECK(out.graze == 5 && out.killed == 3 && out.spawned == 4 && out.rate == 75);
    CHECK(out.bonus == lolk_bonus(5, 75));
    CHECK(s.wave_open == 1 && s.graze == 0 && s.spawned == 0 && s.killed == 0);

    /* 道中分母 0 → 100％（用户定的边缘情况）；编号 1 → 2 + 刷出 */
    lolk_add_graze(&s, 200);
    advance(&s, &in, &out, LOLK_DEBOUNCE + 1);
    in.stage_chapter = 2;
    in.spawn_delta = 1;
    memset(&out, 0, sizeof out);
    CHECK(lolk_tick(&s, &in, &out) == (LOLK_EV_WAVE_END | LOLK_EV_WAVE_START));
    CHECK(out.rate == 100);
    CHECK(out.bonus == 1000000 && out.score == 100000);
    CHECK(out.frag_type == LOLK_FRAG_LIFE);                   /* bonus 够线 → 残机碎片（普通模式）*/

    /* 进 boss：boss 出现 = 新波（旧道中波结算）*/
    lolk_add_graze(&s, 1);
    advance(&s, &in, &out, LOLK_DEBOUNCE + 1);
    in_boss(&in, 100, 0, 0, 41);
    memset(&out, 0, sizeof out);
    CHECK(lolk_tick(&s, &in, &out) == (LOLK_EV_WAVE_END | LOLK_EV_WAVE_START));
    CHECK(out.kind == LOLK_KIND_MID);                         /* 刚结算的是道中波 */
    CHECK(s.wave_kind == LOLK_KIND_BOSS);

    /* boss 非符：相位指纹换 → 结算；没超时 → 100％ */
    lolk_add_graze(&s, 100);
    advance(&s, &in, &out, LOLK_DEBOUNCE + 1);
    in_boss(&in, 200, 0, 0, 41);
    memset(&out, 0, sizeof out);
    CHECK(lolk_tick(&s, &in, &out) == (LOLK_EV_WAVE_END | LOLK_EV_WAVE_START));
    CHECK(out.kind == LOLK_KIND_BOSS && out.is_spell == 0);
    CHECK(out.rate == 100 && out.bonus == 100 * 100 * LOLK_BONUS_COEF);

    /* boss 非符超时：引擎持续位上升沿 → 0％ */
    lolk_add_graze(&s, 100);
    advance(&s, &in, &out, LOLK_DEBOUNCE + 1);
    in_boss(&in, 300, 0, 0, 41);
    in.phase_timeout_flag = 1;                                /* 本相位超时 */
    memset(&out, 0, sizeof out);
    CHECK(lolk_tick(&s, &in, &out) == (LOLK_EV_WAVE_END | LOLK_EV_WAVE_START));
    CHECK(out.rate == 0 && out.bonus == 0 && out.score == 0 && out.frag_type == 0);
    CHECK(s.saw_timeout == 0);                                /* 新波重新开始记 */

    /* 新波起点已带着超时位（残留）→ 不误判；本相位击破条件成立 → 100％ */
    lolk_add_graze(&s, 50);
    advance(&s, &in, &out, LOLK_DEBOUNCE + 1);                /* 让去抖过期（本相位仍 300）*/
    in_boss(&in, 400, 0, 0, 41);
    in.phase_timeout_flag = 1;                                /* 起点就是 1（残留）*/
    CHECK(lolk_tick(&s, &in, &out) == (LOLK_EV_WAVE_END | LOLK_EV_WAVE_START));
    CHECK(s.saw_timeout == 0);                                /* 残留位不算超时 */
    lolk_add_graze(&s, 50);
    advance(&s, &in, &out, LOLK_DEBOUNCE + 1);
    in.phase_broken = 1;                                      /* 本相位打空 */
    CHECK(lolk_tick(&s, &in, &out) == LOLK_EV_NONE);          /* 只是记下，不结波 */
    CHECK(s.saw_broken == 1);
    in.phase_broken = 0;
    in.phase_sig = 450;                                       /* 下一相位登记新槽 */
    memset(&out, 0, sizeof out);
    CHECK(lolk_tick(&s, &in, &out) == (LOLK_EV_WAVE_END | LOLK_EV_WAVE_START));
    CHECK(out.rate == 100);

    /* 符卡超时（bit7）→ 0％ */
    lolk_add_graze(&s, 100);
    advance(&s, &in, &out, LOLK_DEBOUNCE + 1);
    in_boss(&in, 500, 1, 0, 41);                              /* 进符卡 */
    CHECK(lolk_tick(&s, &in, &out) == (LOLK_EV_WAVE_END | LOLK_EV_WAVE_START));
    CHECK(out.is_spell == 0);                                 /* 刚结算的是非符波 */
    CHECK(s.wave_is_spell == 1);
    lolk_add_graze(&s, 100);
    advance(&s, &in, &out, LOLK_DEBOUNCE + 1);
    in.spell_timeout = 1;
    in.phase_sig = 600;
    memset(&out, 0, sizeof out);
    CHECK(lolk_tick(&s, &in, &out) == (LOLK_EV_WAVE_END | LOLK_EV_WAVE_START));
    CHECK(out.is_spell == 1 && out.rate == 0);

    /* 时符（耐久符卡）：即使超时也 100％ */
    lolk_add_graze(&s, 100);
    advance(&s, &in, &out, LOLK_DEBOUNCE + 1);
    in_boss(&in, 700, 1, 1, 41);                              /* 进时符 */
    CHECK(lolk_tick(&s, &in, &out) == (LOLK_EV_WAVE_END | LOLK_EV_WAVE_START));
    CHECK(s.saw_survival == 1);
    lolk_add_graze(&s, 100);
    advance(&s, &in, &out, LOLK_DEBOUNCE + 1);
    in.spell_timeout = 1;                                     /* 超时 */
    in.phase_sig = 800;
    memset(&out, 0, sizeof out);
    CHECK(lolk_tick(&s, &in, &out) == (LOLK_EV_WAVE_END | LOLK_EV_WAVE_START));
    CHECK(out.rate == 100);
    CHECK(out.bonus == 100 * 100 * LOLK_BONUS_COEF);

    /* boss 消失：结算并关波 */
    lolk_add_graze(&s, 10);
    advance(&s, &in, &out, LOLK_DEBOUNCE + 1);
    in_mid(&in, 41);                                          /* boss 没了 */
    memset(&out, 0, sizeof out);
    CHECK(lolk_tick(&s, &in, &out) == LOLK_EV_WAVE_END);
    CHECK(out.wave_index == s.waves_done && out.graze == 10);
    CHECK(s.wave_open == 0);
    CHECK(lolk_tick(&s, &in, &out) == LOLK_EV_NONE);          /* 已关波不再结 */

    /* 纯函数：道中率 / 相位率 */
    CHECK(lolk_rate(0, 0) == 100);
    CHECK(lolk_rate(7, 3) == 100);
    CHECK(lolk_rate(0, 10) == 0);
    CHECK(lolk_rate_phase(0, LOLK_OUT_BROKEN) == 100);
    CHECK(lolk_rate_phase(0, LOLK_OUT_TIMEOUT) == 0);
    CHECK(lolk_rate_phase(1, LOLK_OUT_TIMEOUT) == 100);       /* 时符 */
    CHECK(lolk_rate_phase(1, LOLK_OUT_NONE) == 100);
    CHECK(lolk_bonus(200, 100) == 1000000);
    CHECK(lolk_bonus(1, 3) == 150);
    CHECK(lolk_bonus(0, 100) == 0);
    CHECK(lolk_bonus(100, 0) == 0);

    /* 空波（没有计数）不产出 END */
    lolk_reset(&s);
    in_boss(&in, 1, 0, 0, 0);
    CHECK(lolk_tick(&s, &in, &out) == LOLK_EV_WAVE_START);
    advance(&s, &in, &out, LOLK_DEBOUNCE + 1);
    in_boss(&in, 2, 0, 0, 0);
    CHECK(lolk_tick(&s, &in, &out) == LOLK_EV_WAVE_START);    /* 空波：只有 START */

    /* 碎片：每波够阈值都掉残机碎片（TH15 普通模式口径）*/
    lolk_reset(&s);
    in_boss(&in, 1, 0, 0, 0);
    CHECK(lolk_tick(&s, &in, &out) & LOLK_EV_WAVE_START);
    for (int w = 1; w <= LOLK_FRAG_CYCLE; ++w) {
        lolk_add_graze(&s, 200);
        advance(&s, &in, &out, LOLK_DEBOUNCE + 1);
        if (w < LOLK_FRAG_CYCLE) {
            in_boss(&in, 100 + w, (w & 1), 0, 0);
            CHECK(lolk_tick(&s, &in, &out) == (LOLK_EV_WAVE_END | LOLK_EV_WAVE_START));
            CHECK(out.wave_index == w);
            CHECK(out.bonus >= LOLK_FRAG_THRESHOLD);
            CHECK(out.frag_type == LOLK_FRAG_LIFE);
        } else {
            in_mid(&in, 0);                                   /* boss 消失关波 */
            CHECK(lolk_tick(&s, &in, &out) == LOLK_EV_WAVE_END);
            CHECK(out.wave_index == LOLK_FRAG_CYCLE);
            CHECK(out.frag_type == LOLK_FRAG_LIFE);           /* TH15 普通模式恒掉残机碎片 */
        }
    }

    /* 减速曲线：擦弹 → 30；随后每帧 +10，7 帧回满；再擦弹 → 回 30 */
    lolk_reset(&s);
    CHECK(lolk_slowdown_step(&s, 0) == 100);
    CHECK(lolk_slowdown_step(&s, 1) == 30);
    f = 30;
    for (int i = 0; i < 6; ++i) {
        f += LOLK_SLOW_STEP;
        CHECK(lolk_slowdown_step(&s, 0) == f);
    }
    CHECK(s.slow == 90);
    CHECK(lolk_slowdown_step(&s, 0) == 100);
    CHECK(lolk_slowdown_step(&s, 0) == 100);
    CHECK(lolk_slowdown_step(&s, 1) == 30);
    s.slow = 0;
    CHECK(lolk_slowdown_step(&s, 0) == 100);   /* 状态写坏 → 自愈 */

    /* 负增量 / 0 不计数 */
    lolk_reset(&s);
    in_mid(&in, 0);
    lolk_tick(&s, &in, &out);
    lolk_add_spawned(&s, 0);
    lolk_add_killed(&s, -5);
    lolk_add_graze(&s, -1);
    CHECK(s.spawned == 0 && s.killed == 0 && s.graze == 0);

    /* ══ 道中章（2026-09-13 用户洞察）：**章 = MainSub 编号（去掉尾部字母）** ══════
     * `MainSub00 / 01 / 01b / 02 …` 的编号是天然键；`01`+`01b`(+`01c`) 同章；
     * 同一编号里的多段 burst 不切（旧 `LOLK_MID_BURST_GAP` 规则已删）。
     * 一手结构：所有字母后缀都是基号的 `@…() async`，主上下文子程序名天然是基号。 */

    /* ① 编号变了但还没刷出 → 不开波；第一次刷出才开第一波 */
    lolk_reset(&s);
    in_mid(&in, 0);
    in.stage_chapter = 2;                                /* 进 MainSub02，还没刷出 */
    CHECK(lolk_tick(&s, &in, &out) == LOLK_EV_NONE);
    CHECK(s.wave_open == 0 && s.waves_done == 0);
    in.spawn_delta = 1;
    CHECK(lolk_tick(&s, &in, &out) == LOLK_EV_WAVE_START);
    CHECK(s.wave_open == 1 && s.wave_kind == LOLK_KIND_MID);
    CHECK(s.wave_chapter == 2 && s.mid_rule == LOLK_MID_RULE_MAINSUB);
    lolk_add_spawned(&s, 2);
    lolk_add_graze(&s, 10);
    lolk_add_killed(&s, 1);

    /* ② 同一编号里换到字母后缀子程序（stage_sub 变、stage_chapter 不变）→ 不切 */
    advance(&s, &in, &out, LOLK_DEBOUNCE + 1);
    in.spawn_delta = 1;
    in.stage_sub = 11;                                   /* MainSub01b/…：字母后缀 */
    CHECK(lolk_tick(&s, &in, &out) == LOLK_EV_NONE);
    CHECK(s.wave_open == 1 && s.waves_done == 0 && s.wave_chapter == 2);

    /* ③ 编号变了（2 → 3）+ 刷出 → 结算旧章 + 开新章（rate = 1/2 = 50） */
    in.spawn_delta = 3;
    in.stage_chapter = 3;
    memset(&out, 0, sizeof out);
    CHECK(lolk_tick(&s, &in, &out) == (LOLK_EV_WAVE_END | LOLK_EV_WAVE_START));
    CHECK(out.wave_index == 1 && out.kind == LOLK_KIND_MID);
    CHECK(out.graze == 10 && out.killed == 1 && out.spawned == 2 && out.rate == 50);
    CHECK(s.wave_chapter == 3 && s.wave_open == 1 && s.graze == 0);

    /* ④ 同一编号里静默很久后再刷（st01 MainSub00 的「三波」阴阳玉，burst 间 wait(120)）
     *    → **不再切**：MainSub00 是一章（这正是 2026-09-13 要修的点）*/
    lolk_reset(&s);
    in_mid(&in, 0);
    in.stage_chapter = 0;
    in.spawn_delta = 2;
    CHECK(lolk_tick(&s, &in, &out) == LOLK_EV_WAVE_START);
    lolk_add_spawned(&s, 2);
    advance(&s, &in, &out, LOLK_DEBOUNCE + 1);
    in.spawn_delta = 0;
    advance(&s, &in, &out, 120);                         /* burst 间 wait(120) */
    in.spawn_delta = 2;
    CHECK(lolk_tick(&s, &in, &out) == LOLK_EV_NONE);      /* MainSub00 是一章 → 不切 */
    CHECK(s.waves_done == 0 && s.wave_chapter == 0);
    lolk_add_spawned(&s, 2);
    /* 编号 0 → 1 + 刷出 → 这时才切 */
    in.spawn_delta = 2;
    in.stage_chapter = 1;
    memset(&out, 0, sizeof out);
    CHECK(lolk_tick(&s, &in, &out) == (LOLK_EV_WAVE_END | LOLK_EV_WAVE_START));
    CHECK(out.spawned == 4 && s.waves_done == 1 && s.wave_chapter == 1);

    /* ⑤ `main` / `MainFront` / `MainLatter` 段（stage_chapter < 0、stage_sub >= 0）里刷出
     *    （开场 MapleEnemy）→ 不开波，避免多出一个空章 */
    lolk_reset(&s);
    in_mid(&in, 0);
    in.stage_chapter = -1;
    in.stage_sub = 0;                                    /* sub = "main" */
    in.spawn_delta = 1;
    CHECK(lolk_tick(&s, &in, &out) == LOLK_EV_NONE);
    CHECK(s.wave_open == 0 && s.waves_done == 0);

    /* ⑥ 道中 boss 与 MainSub 并存：boss 出现先开 boss 波；随后 MainSub05 开刷 → 道中章照切
     *    （这是「章 = MainSub 编号」必须压过 boss 抑制的关键场景）*/
    lolk_reset(&s);
    in_boss(&in, 1, 0, 0, 0);
    in.stage_chapter = -1;                               /* boss 出现时关卡脚本在 main/MainFront */
    in.stage_sub = 0;
    in.spawn_delta = 1;
    CHECK(lolk_tick(&s, &in, &out) & LOLK_EV_WAVE_START);
    CHECK(s.wave_kind == LOLK_KIND_BOSS && s.mid_rule == LOLK_MID_RULE_BOSS);
    lolk_add_spawned(&s, 1);                             /* boss 自己也占一刷，别当空波 */
    advance(&s, &in, &out, LOLK_DEBOUNCE + 1);
    in.stage_chapter = 5;                                /* MainSub05 在道中 boss 期间开始刷 */
    in.spawn_delta = 1;
    memset(&out, 0, sizeof out);
    CHECK(lolk_tick(&s, &in, &out) == (LOLK_EV_WAVE_END | LOLK_EV_WAVE_START));
    CHECK(out.kind == LOLK_KIND_BOSS);                   /* 刚结算的是 boss 波 */
    CHECK(s.wave_kind == LOLK_KIND_MID && s.wave_chapter == 5);
    CHECK(s.mid_rule == LOLK_MID_RULE_MAINSUB);

    /* ⑥b 道中 boss 在 MainSub05 途中死掉 → **不能**关掉正开着的道中章 05 */
    lolk_add_spawned(&s, 1);
    advance(&s, &in, &out, LOLK_DEBOUNCE + 1);
    in.boss_present = 0;                                 /* 道中 boss 消失 */
    in.spawn_delta = 0;
    CHECK(lolk_tick(&s, &in, &out) == LOLK_EV_NONE);
    CHECK(s.wave_open == 1 && s.wave_chapter == 5);
    in.stage_chapter = 6;                                /* 接着 MainSub06 开刷 → 才切 05 → 06 */
    in.spawn_delta = 1;
    memset(&out, 0, sizeof out);
    CHECK(lolk_tick(&s, &in, &out) == (LOLK_EV_WAVE_END | LOLK_EV_WAVE_START));
    CHECK(out.kind == LOLK_KIND_MID && s.wave_chapter == 6);

    /* ⑦ boss 相位指纹换（道中不干扰）→ 仍按 boss 相位切 */
    lolk_reset(&s);
    in_boss(&in, 1, 0, 0, 41);
    in.spawn_delta = 1;
    CHECK(lolk_tick(&s, &in, &out) & LOLK_EV_WAVE_START);
    CHECK(s.wave_kind == LOLK_KIND_BOSS);
    lolk_add_spawned(&s, 1);
    advance(&s, &in, &out, LOLK_DEBOUNCE + 1);
    in.phase_sig = 2;
    in.spawn_delta = 0;
    CHECK(lolk_tick(&s, &in, &out) == (LOLK_EV_WAVE_END | LOLK_EV_WAVE_START));

    /* ⑧ 退路：认不出关卡脚本（stage_sub = -1、stage_chapter = -1）→ 静默间隔。
     *    第一次刷出先开第一波；静默 239 帧（差 1）再刷出不切；够 240 → 切 */
    lolk_reset(&s);
    in_mid(&in, 0);
    in.stage_chapter = -1;
    in.stage_sub = -1;
    in.spawn_delta = 1;
    CHECK(lolk_tick(&s, &in, &out) == LOLK_EV_WAVE_START);
    CHECK(s.mid_rule == LOLK_MID_RULE_QUIET);
    lolk_add_graze(&s, 3);
    in.spawn_delta = 0;
    advance(&s, &in, &out, LOLK_DEBOUNCE + 1);
    advance(&s, &in, &out, LOLK_MID_QUIET_GAP - 1 - (LOLK_DEBOUNCE + 1));
    CHECK(s.quiet == LOLK_MID_QUIET_GAP - 1);
    in.spawn_delta = 1;
    CHECK(lolk_tick(&s, &in, &out) == LOLK_EV_NONE);

    /* ⑨ 静默够 LOLK_MID_QUIET_GAP 帧再刷出 → 结算旧波 + 开新波 */
    lolk_reset(&s);
    in_mid(&in, 0);
    in.stage_chapter = -1;
    in.stage_sub = -1;
    in.spawn_delta = 1;
    CHECK(lolk_tick(&s, &in, &out) == LOLK_EV_WAVE_START);
    lolk_add_graze(&s, 3);
    in.spawn_delta = 0;
    advance(&s, &in, &out, LOLK_DEBOUNCE + 1);
    advance(&s, &in, &out, LOLK_MID_QUIET_GAP - (LOLK_DEBOUNCE + 1));
    CHECK(s.quiet == LOLK_MID_QUIET_GAP);
    in.spawn_delta = 1;
    memset(&out, 0, sizeof out);
    CHECK(lolk_tick(&s, &in, &out) == (LOLK_EV_WAVE_END | LOLK_EV_WAVE_START));
    CHECK(out.kind == LOLK_KIND_MID && out.graze == 3);
    CHECK(s.quiet == 0);

    /* 规则名（日志用）*/
    CHECK(strcmp(lolk_mid_rule_name(LOLK_MID_RULE_MAINSUB), "mainsub") == 0);
    CHECK(strcmp(lolk_mid_rule_name(LOLK_MID_RULE_QUIET), "quiet-fallback") == 0);
    CHECK(strcmp(lolk_mid_rule_name(LOLK_MID_RULE_BOSS), "boss") == 0);
    CHECK(strcmp(lolk_mid_rule_name(LOLK_MID_RULE_NONE), "none") == 0);

    /* reset 清干净 */
    lolk_reset(&s);
    CHECK(s.wave_open == 0 && s.spell_active == 0 && s.wave_is_spell == 0 &&
          s.graze == 0 && s.spawned == 0 && s.killed == 0 &&
          s.waves_done == 0 && s.slow == LOLK_SLOW_FULL &&
          s.last_chapter == -1 && s.saw_timeout == 0 && s.saw_broken == 0);
    CHECK(s.last_stage_sub == -1 && s.wave_chapter == -1 &&
          s.mid_rule == LOLK_MID_RULE_NONE && s.quiet == 0);

    printf("test_lolk_core: %d passed, %d failed\n", s_pass, s_fail);
    return s_fail ? 1 : 0;
}
