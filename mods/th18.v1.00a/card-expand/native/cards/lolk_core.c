/* lolk_core.c —— 绀珠传（id 115）纯逻辑实现。语义见 lolk_core.h 头注。 */
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

const char *lolk_mid_rule_name(int32_t rule)
{
    switch (rule) {
    case LOLK_MID_RULE_MAINSUB: return "mainsub";
    case LOLK_MID_RULE_QUIET:   return "quiet-fallback";
    case LOLK_MID_RULE_BOSS:    return "boss";
    default:                    return "none";
    }
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
    s->last_chapter = -1;                 /* 首帧章号 0 也算「变」→ boss 兜底能认第一波 */
    s->last_stage_sub = -1;
    s->wave_chapter = -1;                 /* 还没开波 → 首个 MainSub 编号都算「变」*/
    s->mid_rule = LOLK_MID_RULE_NONE;
}

/* 道中波边界。**2026-09-13 用户洞察：章 = MainSub 编号（去掉尾部字母）**。
 *
 * TH15 用 ECL 524 在每段 `MainSubNN` 开头宣告一章；TH18 道中没有 524，但
 * `MainSub00 / 01 / 01b / 02 …` 的**编号本身**就是天然键，字母后缀是基号的异步子程序。
 * 所以本卡只用接线层解析出的 `stage_chapter`：
 *
 *   ① `stage_chapter >= 0`（在 MainSub 里）：编号变了（或当前没开波）+ 本帧有刷出 → 新章。
 *      同一编号里的多段 burst（st01 `MainSub00` 的三波阴阳玉）与异步子程序
 *      （`MainSubNNb`）**都不切**。旧实现靠 `LOLK_MID_BURST_GAP` 静默窗口猜，已删除。
 *   ② `stage_chapter < 0` 且 `stage_sub >= 0`（`main` / `MainFront` / `MainLatter` 段）：
 *      不是道中章 → 这些段里的刷出（开场 `MapleEnemy`）不开波。
 *   ③ `stage_sub < 0`（认不出关卡脚本）= 退路：静默 ≥ `LOLK_MID_QUIET_GAP` 帧后再刷出 → 新章。 */
static int mid_boundary(const lolk_state_t *s, const lolk_in_t *in, int32_t quiet_before)
{
    if (in->spawn_delta <= 0) {
        return 0;
    }
    if (in->stage_chapter >= 0) {
        return (!s->wave_open || in->stage_chapter != s->wave_chapter) ? 1 : 0;   /* ① */
    }
    if (in->stage_sub >= 0) {
        return 0;                                                              /* ② */
    }
    return (quiet_before >= LOLK_MID_QUIET_GAP) ? 1 : 0;                       /* ③ */
}

/* 开波时用的规则（写进 state，日志里区分「MainSub 主信号」还是退路）。*/
static int mid_rule_of(const lolk_in_t *in)
{
    if (in->stage_chapter >= 0) return LOLK_MID_RULE_MAINSUB;
    if (in->stage_sub >= 0) return LOLK_MID_RULE_NONE;   /* 非 MainSub 段：不该开波 */
    return LOLK_MID_RULE_QUIET;
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
    s->wave_chapter = (kind == LOLK_KIND_MID) ? in->stage_chapter : -1;   /* boss 波不认章 */
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
    int new_kind = LOLK_KIND_MID;
    int new_rule = LOLK_MID_RULE_NONE;
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

    /* ② 道中信号跟踪（每帧一次）：本帧有刷出 → 清静默计数（只在认不出关卡脚本的退路用）。*/
    if (in->spawn_delta > 0) {
        s->quiet = 0;
    } else if (s->quiet < 0x7fffffff) {
        s->quiet++;
    }

    /* ③ boss 消失：只有**当前开的是 boss 波**才结算并关波。
     *    ⚠️ 2026-09-13：道中 boss（st01 `MBoss`）与 `MainSub05/06` 时间重叠 ——
     *    若玩家在 MainSub05 刷怪途中打死道中 boss，此时开着的是**道中章 05**；
     *    那种情况不能关 05（关了之后 MainSub05 的后续刷出会把它再开一次 → 一章裂成两章）。
     *    道中章的边界只认下一个 MainSub 编号。*/
    if (s->boss_present && !boss) {
        if (s->wave_open && s->wave_kind == LOLK_KIND_BOSS) {
            ev |= close_wave(s, out);
            s->wave_open = 0;
        }
    }

    /* ④ 边界。相位指纹只在「有活动中断槽」时更新 —— 槽被清空的那一帧（0）不算边界，
     *    于是「槽清空 → 新槽」这条链只报一次，且报到的是新相位。
     *
     * ⚠️ 2026-09-13：道中波**不再**用 ECL 524 的 `chapter_changed`（TH18 道中没有 524，
     * 轮询它只会在 boss 入场 `setChapter(41→43)` 那一帧多开一个空道中波 —— 旧 §5 的 ❓）。
     * 道中一律走 `mid_boundary` 的 MainSub 编号。*/
    mid_new = mid_boundary(s, in, quiet_before);

    if (mid_new) {
        /* 道中章优先：MainSub 编号变了 / 退路静默够 → 即使道中 boss 在场也照切。*/
        want_new = 1;
        new_kind = LOLK_KIND_MID;
        new_rule = mid_rule_of(in);
    } else if (boss != s->boss_present) {
        want_new = boss;                                    /* 出现 = 新波；消失已在 ③ 处理 */
        new_kind = LOLK_KIND_BOSS;
        new_rule = LOLK_MID_RULE_BOSS;
    } else if (boss && s->wave_open && s->debounce == 0 && in->phase_sig && s->last_phase_sig &&
               in->phase_sig != s->last_phase_sig) {
        want_new = 1;                                       /* boss 相位换（指纹）*/
        new_kind = LOLK_KIND_BOSS;
        new_rule = LOLK_MID_RULE_BOSS;
    } else if (!boss && !s->wave_open && in->spawn_delta > 0 && in->stage_sub < 0) {
        /* 退路的**第一波**：认不出关卡脚本时静默计数还没攒够，第一个刷出就得开波
         * （否则永远开不出来）。*/
        want_new = 1;
        new_kind = LOLK_KIND_MID;
        new_rule = LOLK_MID_RULE_QUIET;
    }
    /* ⚠️ 2026-09-09 用户实跑：**不要**在「章号变了但还没有任何刷出/boss」时开波。
     * 否则开局那一帧先开一个空波、下一帧第一个刷出就把它结掉 → 屏幕上一上来弹一块
     * 「击破率 0%」的结算面板。开波只由「刷出（mid_new / 退路）」或「boss 出现」触发。*/

    if (want_new) {
        if (s->wave_open) {
            ev |= close_wave(s, out);
        }
        open_wave(s, in, new_kind);
        s->mid_rule = new_rule;
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
