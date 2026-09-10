/* lolk_core.c —— 绀珠传（id 118）纯逻辑实现。语义见 lolk_core.h 头注。 */
#include "lolk_core.h"

#include <string.h>

int32_t lolk_rate(int32_t killed, int32_t spawned)
{
    int32_t r;
    if (spawned <= 0) {
        return 100;                       /* 没有分母 → 不惩罚（用户定案：没敌人 = 100％）*/
    }
    if (killed < 0) {
        killed = 0;
    }
    r = (killed * 100) / spawned;
    if (r > 100) {
        r = 100;
    }
    return r;
}

int32_t lolk_rate_phase(int32_t is_survival, int32_t outcome)
{
    if (is_survival) {
        return 100;                       /* 时符：恒 100％（引擎也把它的超时当收卡）*/
    }
    return (outcome == LOLK_OUT_TIMEOUT) ? 0 : 100;
}

int32_t lolk_bonus(int32_t graze, int32_t rate)
{
    int64_t raw;
    if (graze <= 0 || rate <= 0) {
        return 0;
    }
    raw = (int64_t)graze * rate * LOLK_BONUS_COEF;   /* TH15：擦弹 × 击破率％ × 50 */
    return (int32_t)((raw / 10) * 10);               /* 向下取整到 10 */
}

static void wave_clear(lolk_state_t *s)
{
    s->graze = 0;
    s->spawned = 0;
    s->killed = 0;
    s->saw_spell = 0;
    s->wave_is_spell = 0;
    s->saw_survival = 0;
    s->saw_broken = 0;
    s->saw_timeout = 0;
}

void lolk_reset(lolk_state_t *s)
{
    memset(s, 0, sizeof *s);
    s->slow = LOLK_SLOW_FULL;
    s->last_chapter = -1;                 /* 首帧章号 0 也算「变」→ 能开第一波 */
    s->last_stage_sub = -1;               /* 首帧子程序号 0 也算「变」→ 能开道中第一波 */
}

/* 道中波边界：TH15 的「一章」= TH18 的「一个敌人组」。三条信号：
 *  ① 子程序换了 + 该子程序内第一次刷出（且在 LOLK_MID_SUB_WINDOW 帧内）→ 新波
 *     （`MainFront` 里 `@MainSubNN()` 一组一组的开头）；
 *  ② **同一子程序内，静默 ≥ LOLK_MID_BURST_GAP 帧后又刷出** → 新波。
 *     一个 MainSub 里常常排了几波，例如 st01 `MainSub00` 的三波阴阳玉：
 *     burst 之间 `wait(120)`、burst 内部 `wait(10)`（2026-09-09 用户实跑：原来这三波被并成一章）。
 *  ③ 认不出关卡脚本（stage_sub < 0）时退到更宽的 LOLK_MID_QUIET_GAP。*/
static int mid_boundary(const lolk_state_t *s, const lolk_in_t *in, int32_t quiet_before)
{
    if (in->spawn_delta <= 0) {
        return 0;
    }
    if (in->stage_sub >= 0) {
        if (!s->sub_fired && s->sub_age <= LOLK_MID_SUB_WINDOW) return 1;      /* ① */
        return (quiet_before >= LOLK_MID_BURST_GAP) ? 1 : 0;                    /* ② */
    }
    return (quiet_before >= LOLK_MID_QUIET_GAP) ? 1 : 0;
}

/* 本波击破率：道中按击破数/总数；boss 相位按击破/超时（时符恒 100）。*/
static int32_t wave_rate(const lolk_state_t *s)
{
    if (s->wave_kind != LOLK_KIND_BOSS) {
        return lolk_rate(s->killed, s->spawned);
    }
    if (s->saw_broken) {
        return lolk_rate_phase(s->saw_survival, LOLK_OUT_BROKEN);   /* 见过击破 → 100％ */
    }
    if (s->saw_timeout) {
        return lolk_rate_phase(s->saw_survival, LOLK_OUT_TIMEOUT);  /* 只有超时 → 0％（时符仍 100）*/
    }
    /* 两个信号都没抓到（boss 直接死了 / 采样错过）→ 当击破，不惩罚玩家。*/
    return lolk_rate_phase(s->saw_survival, LOLK_OUT_BROKEN);
}

static int close_wave(lolk_state_t *s, lolk_settle_t *out)
{
    int32_t bonus, rate;
    int32_t had = (s->graze || s->spawned || s->killed);

    if (!had) {
        wave_clear(s);
        return LOLK_EV_NONE;              /* 空波：不产出（TH15 的「没刷怪不结算」同义）*/
    }
    rate = wave_rate(s);
    bonus = lolk_bonus(s->graze, rate);
    s->waves_done = (s->waves_done < LOLK_WAVE_MAX) ? s->waves_done + 1 : LOLK_WAVE_MAX;
    if (out) {
        memset(out, 0, sizeof *out);
        out->wave_index = s->waves_done;
        out->kind = s->wave_kind;
        out->is_spell = s->wave_is_spell;
        out->graze = s->graze;
        out->killed = s->killed;
        out->spawned = s->spawned;
        out->rate = rate;
        out->bonus = bonus;
        out->score = bonus / LOLK_SCORE_DIV;
        if (bonus >= LOLK_FRAG_THRESHOLD) {
            /* TH15 的**普通（Legacy）模式**掉的是**残机碎片**（用户 2026-09-10 更正 + 一手汇编复核：
             * `th15:0x43ae5d TEST [0x4e7794],0x300` → `JZ th15:0x43af1d` = 普通模式 →
             * `CALL th15:0x44fa60`，那函数加的是 `CURRENT_LIFE_FRAGMENTS`(+0x64)、调 `Gui__update_lives`、
             * 凑满播 se_extend(0x11)；Pointdevice 才走 `0x44fbc0` = B 碎片。
             * 对应标签：普通模式保留的 VM 是脚本 0xf2 = sprite150（本卡面板第 6 条 = PANEL_6）。*/
            out->frag_type = LOLK_FRAG_LIFE;
            (void)LOLK_FRAG_BOMB;
        }
    }
    wave_clear(s);
    return LOLK_EV_WAVE_END;
}

static void open_wave(lolk_state_t *s, const lolk_in_t *in, int32_t kind)
{
    s->wave_open = 1;
    s->wave_kind = kind;
    wave_clear(s);
    s->debounce = LOLK_DEBOUNCE;
    s->flag_base = in->phase_timeout_flag;
    s->st_base = in->spell_timeout;
    if (in->spell_active) {
        s->saw_spell = 1;
        s->wave_is_spell = 1;
        if (in->survival) {
            s->saw_survival = 1;
        }
    }
}

int lolk_tick(lolk_state_t *s, const lolk_in_t *in, lolk_settle_t *out)
{
    int ev = LOLK_EV_NONE;
    int boss = in->boss_present ? 1 : 0;
    int want_new = 0;
    int chapter_changed = (in->chapter != s->last_chapter);
    int32_t quiet_before = s->quiet;
    int mid_new;

    /* ① 旧波的相位结果。**边界帧也要采**：击破 / 超时就发生在相位切换的那一帧，
     *    所以这一步必须在边界处理之前。符卡标记（saw_spell / wave_is_spell）
     *    不在这里采 —— 边界帧的符卡位属于**新波**（见 ④）。*/
    if (s->wave_open) {
        if (in->phase_broken) {
            s->saw_broken = 1;
        }
        if (in->phase_timeout) {
            s->saw_timeout = 1;                 /* 相位本地条件（超时那帧才成立）*/
        }
        if (!s->flag_base && in->phase_timeout_flag) {
            s->saw_timeout = 1;                 /* 引擎持续位的上升沿 = 本相位超时 */
        }
        if (in->spell_active && in->survival) {
            s->saw_survival = 1;
        }
        if ((in->spell_active && in->spell_timeout) || (!s->st_base && in->spell_timeout)) {
            s->saw_timeout = 1;                 /* 符卡位 bit7（本波起点之后才翻上来的）*/
        }
    }

    /* ② 道中信号跟踪（每帧一次，必须在边界判定之前）：
     *    关卡脚本子程序换了 → 重置窗口；本帧有刷出 → 清静默计数。*/
    if (in->stage_sub != s->last_stage_sub) {
        s->sub_age = 0;
        s->sub_fired = 0;
    } else if (s->sub_age < 0x7fffffff) {
        s->sub_age++;
    }
    if (in->spawn_delta > 0) {
        s->quiet = 0;
    } else if (s->quiet < 0x7fffffff) {
        s->quiet++;
    }
    mid_new = mid_boundary(s, in, quiet_before);

    /* ③ boss 消失：结算并关波（道中 boss 整场就是这一波）*/
    if (s->boss_present && !boss) {
        if (s->wave_open) {
            ev |= close_wave(s, out);
        }
        s->wave_open = 0;
    }

    /* ④ 边界。相位指纹只在「有活动中断槽」时更新 —— 槽被清空的那一帧（0）不算边界，
     *    于是「槽清空 → 新槽」这条链只报一次，且报到的是新相位。*/
    if (boss != s->boss_present) {
        want_new = boss;                                    /* 出现 = 新波；消失已在 ③ 处理 */
    } else if (s->wave_open && chapter_changed) {
        want_new = 1;                                       /* ECL 524 切章（道中 / boss 阶段）*/
    } else if (boss && s->wave_open && s->debounce == 0 && in->phase_sig && s->last_phase_sig &&
               in->phase_sig != s->last_phase_sig) {
        want_new = 1;                                       /* boss 相位换 */
    } else if (!boss && s->debounce == 0 &&
               (mid_new || (!s->wave_open && in->spawn_delta > 0))) {
        /* 道中：新敌人组（见 ② 与头注）；**还没开波时，第一个刷出就开第一波**
         * （不能靠「章号变了」开波 —— 那会在开局先开一个空波，见下面注释）*/
        want_new = 1;
        s->sub_fired = 1;                                   /* 这一刷已消化，同子程序内不再切 */
    }
    /* ⚠️ 2026-09-09 用户实跑：**不要**在「章号变了但还没有任何刷出/boss」时开波。
     * 否则开局那一帧先开一个空波、下一帧第一个刷出就把它结掉 → 屏幕上一上来弹一块
     * 「击破率 0%」的结算面板。开波只由「刷出（mid_new）」或「boss 出现」触发。*/

    if (want_new) {
        if (s->wave_open) {
            ev |= close_wave(s, out);
        }
        open_wave(s, in, boss ? LOLK_KIND_BOSS : LOLK_KIND_MID);
        ev |= LOLK_EV_WAVE_START;
    }

    /* ⑤ 新波自己的符卡标记（符卡通常在章开始后几帧才宣告，所以每帧都要补）*/
    if (s->wave_open && in->spell_active) {
        s->saw_spell = 1;
        s->wave_is_spell = 1;
        if (in->survival) {
            s->saw_survival = 1;
        }
    }

    if (s->debounce > 0) {
        s->debounce--;
    }
    s->boss_present = boss;
    s->spell_active = in->spell_active;
    if (in->phase_sig) {
        s->last_phase_sig = in->phase_sig;
    }
    s->last_chapter = in->chapter;
    s->last_stage_sub = in->stage_sub;
    return ev;
}

void lolk_add_graze(lolk_state_t *s, int32_t n)
{
    if (n <= 0) {
        return;
    }
    if (s->wave_open && s->graze < 0x7fffffff - n) {
        s->graze += n;
    }
}

void lolk_add_spawned(lolk_state_t *s, int32_t n)
{
    if (n <= 0) {
        return;
    }
    if (s->wave_open && s->spawned < 0x7fffffff - n) {
        s->spawned += n;
    }
}

void lolk_add_killed(lolk_state_t *s, int32_t n)
{
    if (n <= 0) {
        return;
    }
    if (s->wave_open && s->killed < 0x7fffffff - n) {
        s->killed += n;
    }
}

int32_t lolk_slowdown_step(lolk_state_t *s, int32_t grazed_this_frame)
{
    if (s->slow < LOLK_SLOW_START || s->slow > LOLK_SLOW_FULL) {
        s->slow = LOLK_SLOW_FULL;         /* 状态被外部写坏 → 自愈 */
    }
    if (grazed_this_frame) {
        s->slow = LOLK_SLOW_START;
        s->grazed_this_frame = 1;
    } else if (s->slow < LOLK_SLOW_FULL) {
        s->slow += LOLK_SLOW_STEP;
        if (s->slow > LOLK_SLOW_FULL) {
            s->slow = LOLK_SLOW_FULL;
        }
    }
    return s->slow;
}
