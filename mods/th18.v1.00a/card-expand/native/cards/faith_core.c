/* faith_core.c —— 绿点 / 信仰系统（110 + 111 共用）的纯逻辑。接口与依据见 faith_core.h。
 *
 * 数值来源（**一条都没自己发明**）：
 *   `docs/superpowers/specs/2026-09-10-mof-faith-design.md` §2.2 / §2.3 / §3（THBWiki 一手 +
 *   th10 反编译 §8.2 的难度表）
 *   `docs/superpowers/specs/2026-09-11-system-card-spec-review.md` §7.1 / §7.2 / §7.5 / §3.2.1（用户定案）
 * 主机单测：`tests/test_faith_core.c`。
 */
#include "faith_core.h"

/* ---- 难度表 ---- */
int32_t faith_base_111(int difficulty)
{
    switch (difficulty) {
    case FAITH_DIFF_EASY:    return FAITH_BASE_111_EASY;
    case FAITH_DIFF_NORMAL:  return FAITH_BASE_111_NORMAL;
    case FAITH_DIFF_HARD:    return FAITH_BASE_111_HARD;
    case FAITH_DIFF_EXTRA:   return FAITH_BASE_111_EXTRA;
    case FAITH_DIFF_LUNATIC:
    default:                 return FAITH_BASE_111_LUNATIC;
    }
}

int32_t faith_big_mof(int difficulty)
{
    switch (difficulty) {
    case FAITH_DIFF_HARD:    return FAITH_MOF_BIG_H;
    case FAITH_DIFF_LUNATIC:
    case FAITH_DIFF_EXTRA:   return FAITH_MOF_BIG_L;
    case FAITH_DIFF_EASY:
    case FAITH_DIFF_NORMAL:
    default:                 return FAITH_MOF_BIG_EN;
    }
}

/* ---- 初始化 ---- */
void faith_slot_init_110(faith_slot_t *s)
{
    if (!s) return;
    s->delta = 0;
    s->combo = 0;
    s->signal = 0;
    s->small_rem = 0;
    s->base = FAITH_BASE_110;
    s->delta_cap = 0;                 /* 无上限（110 侧不设上限，只有下限 0）*/
}

void faith_slot_init_111(faith_slot_t *s, int difficulty)
{
    if (!s) return;
    s->delta = 0;
    s->combo = 0;
    s->signal = 0;
    s->small_rem = 0;
    s->base = faith_base_111(difficulty);
    s->delta_cap = s->base;           /* 上限 = 原版基础（原版「上限 = 初值 ×2」的等价写法，§7.2）*/
}

void faith_slot_reset(faith_slot_t *s)
{
    if (!s) return;
    s->delta = 0;
    s->combo = 0;
    s->signal = 0;
    /* small_rem 是「跨关保留」的私有余数（§3.2.1）⇒ 这里**不清**。
     * 真要清（换局）走 faith_slot_init_*。*/
}

/* ---- 加值 ---- */
int32_t faith_slot_add_fixed(faith_slot_t *s, int32_t delta_fixed)
{
    int32_t next, applied;

    if (!s || delta_fixed <= 0) return 0;
    next = s->delta + delta_fixed;
    /* ⚠ 量纲：`base` / `delta_cap` 是**原始单位**（50000 这种给人看的数），
     * `delta` 是**定点**（×100）⇒ 比较前把上限换成定点。*/
    if (s->delta_cap > 0 && next > s->delta_cap * FAITH_FIXED) next = s->delta_cap * FAITH_FIXED;
    applied = next - s->delta;
    s->delta = next;
    return applied;                   /* 夹取后真正进去的量（满上限时 = 0）*/
}

int32_t faith_slot_add(faith_slot_t *s, faith_ev_t ev, int32_t delta)
{
    /* ev 决定「这一档对本槽合不合法」—— 110 只认自己的档、111 只认自己的，避免一张卡
     * 误把另一张的档位值加进自己的槽（这正是「分槽记账」要防的事）。*/
    if (!s || delta <= 0) return 0;
    switch (ev) {
    case FAITH_EV_BIG:        /* 两个槽都有「大」档，值由调用方按自己的表给 */
    case FAITH_EV_GREEN:      /* 绿点收集（蓝点通道）*/
    case FAITH_EV_SMALL_P:
    case FAITH_EV_SMALL_FAITH:
    case FAITH_EV_BIG_P:
    case FAITH_EV_KILL_NONBOSS:
    case FAITH_EV_MICRO:
    case FAITH_EV_KILL_BOSS:
    case FAITH_EV_ONEUP:
        return faith_slot_add_fixed(s, delta * FAITH_FIXED);
    default:
        return 0;
    }
}

void faith_slot_add_cancel(faith_slot_t *s110, faith_slot_t *s111, int32_t n,
                           int32_t *out_110_add, int32_t *out_111_add)
{
    int32_t a110 = 0, a111 = 0;

    if (n > 0) {
        /* 110：微信仰点，**每颗 +10**（§2.2 / §7.5）*/
        if (s110) a110 = faith_slot_add(s110, FAITH_EV_MICRO, n * FAITH_MOF_WEI);
        /* 111：小绿点「每 100 个 +10」，余数用卡私有整数计数器（0–99，满 100 进位，**跨关保留**）*/
        if (s111) {
            int32_t total = s111->small_rem + n;
            int32_t carry = total / FAITH_SA_SMALL_N;
            s111->small_rem = total % FAITH_SA_SMALL_N;
            if (carry > 0) a111 = faith_slot_add(s111, FAITH_EV_MICRO, carry * FAITH_SA_SMALL_V);
        }
    }
    if (out_110_add) *out_110_add = a110;
    if (out_111_add) *out_111_add = a111;
}

/* ---- 减益 ---- */
int32_t faith_slot_reduce(faith_slot_t *s, int kind)
{
    int32_t amount = 0, before;

    if (!s) return 0;
    switch (kind) {
    case FAITH_REDUCE_MISS:
        /* miss −(信仰 − 50000)/3。因为 信仰 = 50000 + Δ ⇒ (信仰 − 50000) = Δ，
         * 而 Δ 的**下限就是 0** ⇒ 这正是「50000 扣不穿」的实现（§7.2 用户原话）。
         * ⚠ 先回到**原始单位**再除（原作是在信仰这个整数上做整数除法、向下取整）——
         *   直接除定点会多出两次取整，量纲也不对。*/
        amount = ((s->delta / FAITH_FIXED) / FAITH_MISS_DIV) * FAITH_FIXED;
        break;
    case FAITH_REDUCE_BOMB:
        amount = FAITH_SPELL_MISS_COST * FAITH_FIXED;
        break;
    case FAITH_REDUCE_AVOID:
        amount = FAITH_ALL_AVOID_COST * FAITH_FIXED;
        break;
    default:
        return 0;
    }
    if (amount <= 0) return 0;
    before = s->delta;
    s->delta -= amount;
    if (s->delta < 0) s->delta = 0;      /* Δ 下限 0（110）· 111 侧同样不为负 */
    return before - s->delta;
}

int32_t faith_slot_drain_empty(faith_slot_t *s, int frames, int enemies_present)
{
    int32_t loss = 0, before;

    if (!s || frames <= 0) return 0;
    if (s->combo > 0) return 0;                       /* 连击槽没空 → 不衰减 */
    loss = (frames * (enemies_present ? FAITH_EMPTY_ENEMY_PER_SEC : FAITH_EMPTY_NO_ENEMY_PER_SEC)
            * FAITH_FIXED) / FAITH_FRAMES_PER_SEC;
    /* 除法在最后一步 ⇒ 60 帧恰好 −10800 / −600（无余数丢失；`FAITH_EMPTY_*_EVERY` 那组常量
     * 是这条的精确整数分解，留给需要一个结算周期的实现参考）。*/
    if (loss <= 0) return 0;
    before = s->delta;
    s->delta -= loss;
    if (s->delta < 0) s->delta = 0;
    return before - s->delta;
}

int faith_combo_empty(const faith_slot_t *s) { return s ? (s->combo <= 0) : 0; }

/* ---- 连击槽 ---- */
int32_t faith_combo_value(faith_ev_t ev)
{
    switch (ev) {
    case FAITH_EV_BIG:         return FAITH_COMBO_BIG;        /* 大信仰点 +130f */
    case FAITH_EV_GREEN:       return FAITH_COMBO_GREEN;      /* 蓝点 +100f */
    case FAITH_EV_SMALL_P:     return FAITH_COMBO_SMALL_P;    /* 小 P +60f */
    case FAITH_EV_SMALL_FAITH: return FAITH_COMBO_SMALL_P;    /* 小信仰点 +60f */
    case FAITH_EV_BIG_P:       return FAITH_COMBO_BIG_P;      /* 大 P +20f */
    case FAITH_EV_KILL_NONBOSS:return FAITH_COMBO_BIG_P;      /* 击杀非 boss +20f */
    case FAITH_EV_MICRO:       return FAITH_COMBO_WEI;        /* 微信仰点 +3f */
    case FAITH_EV_KILL_BOSS:   return FAITH_COMBO_KILL_BOSS;  /* 击杀 boss +0f */
    case FAITH_EV_ONEUP:       return FAITH_COMBO_KILL_BOSS;  /* 1UP +0f */
    default:                   return 0;
    }
}

int32_t faith_combo_set(faith_slot_t *s, int32_t frames)
{
    if (!s) return 0;
    if (frames < 0) frames = 0;
    if (frames > FAITH_COMBO_FULL) frames = FAITH_COMBO_FULL;
    s->combo = frames;
    return s->combo;
}

int32_t faith_combo_add(faith_slot_t *s, faith_ev_t ev)
{
    int32_t before;

    if (!s) return 0;
    before = s->combo;
    faith_combo_set(s, s->combo + faith_combo_value(ev));
    return s->combo - before;
}

int faith_combo_full(const faith_slot_t *s) { return s && s->combo >= FAITH_COMBO_FULL; }

/* ---- 信号强度 ---- */
int32_t faith_signal_step(int32_t signal_now, int above_line, int offline, int32_t graze)
{
    int32_t d = 0;

    if (signal_now < 0) signal_now = 0;
    if (signal_now > FAITH_SIGNAL_MAX) signal_now = FAITH_SIGNAL_MAX;

    if (above_line && signal_now < FAITH_SIGNAL_HI_MARK) d += FAITH_SIGNAL_GAIN_LINE;
    else if (signal_now >= FAITH_SIGNAL_HI_MARK)        d += FAITH_SIGNAL_GAIN_TRICK;
    /* ⚠ 「线上且已超 100」这一档：§7.5 只给了「未满 100 时 +50/帧」与「超 100 后 +0.09/帧」
     * ⇒ 超 100 之后不管是线上还是线下都走 +0.09（上面的 else-if），线下另加下面的 −0.3。*/

    if (graze > 0) d += graze * FAITH_SIGNAL_GAIN_GRAZE;
    if (offline)   d -= FAITH_SIGNAL_LOSS_OFFLINE;
    return d;
}

void faith_signal_add(faith_slot_t *s, int32_t d)
{
    if (!s) return;
    s->signal += d;
    if (s->signal < 0) s->signal = 0;
    if (s->signal > FAITH_SIGNAL_MAX) s->signal = FAITH_SIGNAL_MAX;
}

void faith_signal_reset(faith_slot_t *s) { if (s) s->signal = 0; }

/* ---- 单位换算 / 快照 ---- */
int32_t faith_slot_total(const faith_slot_t *s)
{
    int32_t delta;
    if (!s) return 0;
    delta = s->delta / FAITH_FIXED;      /* 出槽：定点 → 原始单位（整数除法 = 向下取整；110 的加值都是 100 的倍数，无余数）*/
    return s->base + delta;
}

int32_t faith_score_unit(const faith_slot_t *s) { return faith_slot_total(s); }

void faith_slot_view(const faith_slot_t *s, faith_view_t *out)
{
    if (!out) return;
    if (!s) {
        out->faith_total = 0; out->delta = 0; out->base = 0; out->combo = 0;
        return;
    }
    out->faith_total = faith_slot_total(s);
    out->delta = s->delta / FAITH_FIXED;
    out->base = s->base;
    out->combo = s->combo;
}
