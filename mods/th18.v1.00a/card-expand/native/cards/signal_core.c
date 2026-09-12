/* signal_core.c —— 地灵殿（111）纯逻辑的实现。接口与依据见 signal_core.h。
 *
 * ★ 本文件**不重写共用件**：信号槽的四段增减直接把共用件 `faith_signal_step()` 转出来，
 *   擦弹倍率在卡侧用 `mof_core_graze_mult()`，灵击用 `mof_core_power_spend()`，
 *   槽内加值 / 上限 / 小绿点余数在 `faith_core.c`。这里只做**111 私有**的三件事：
 *     ① 满槽阈值（≥100）；② 信号强度 → 0.00–1.00 的乘数项；③ 蓝点分乘数。
 *
 * 数值出处（一条都没自己发明）：
 *   `docs/superpowers/specs/2026-09-10-sa-signal-design.md` §2.1 / §2.3 / §3
 *   `docs/superpowers/specs/2026-09-11-system-card-spec-review.md` §7.5（Δ_111 明细 + 信号强度）
 * 主机单测：`tests/test_signal_core.c`。
 */
#include "signal_core.h"

int32_t signal_core_step(int32_t signal_now, int line_reading, int offline, int32_t graze)
{
    /* 转发到共用件（EVENTS-CONTRACT 的「同一件事只有一份实现」；110 侧不消费信号，
     * 但信号这张表的**定义权**在 `faith_core.h`，111 只是它的第一个用户）。 */
    return faith_signal_step(signal_now, line_reading, offline, graze);
}

int32_t signal_core_clamp(int32_t v)
{
    if (v < 0) return 0;
    if (v > SIGNAL_MAX) return SIGNAL_MAX;
    return v;
}

int32_t signal_core_pickup(faith_slot_t *s, faith_ev_t ev, int32_t value)
{
    int32_t added;

    /* `value` = 这颗实体的单颗值（大绿点 100 / 中绿点 10，由卡侧 `faith_spawn()` 给）。
     * 加值 / Δ_111 上限（= 原版基础，§7.2）都由共用件夹；本函数只决定「走哪个档」。*/
    if (!s || value <= 0) return 0;
    added = faith_slot_add(s, ev, value);
    (void)faith_combo_add(s, ev);      /* 连击槽 +f 表见 §7.5（大 = +130f、绿点通道 = +100f）*/
    return added;                      /* 定点 ×FAITH_FIXED；夹到上限时 = 0 */
}

int32_t signal_core_apply(faith_slot_t *s, int32_t d)
{
    if (!s) return 0;
    faith_signal_add(s, d);          /* 共用件里已经夹了 [0, FAITH_SIGNAL_MAX] */
    return s->signal;
}

int32_t signal_core_apply_step(faith_slot_t *s, int line_reading, int offline, int32_t graze)
{
    if (!s) return 0;
    return signal_core_apply(s, signal_core_step(s->signal, line_reading, offline, graze));
}

int signal_core_reset(faith_slot_t *s)
{
    if (!s) return 0;
    if (s->signal == 0) return 0;
    faith_signal_reset(s);
    return 1;
}

int signal_core_is_full(int32_t signal_now)
{
    return signal_now >= SIGNAL_FULL_AT;
}

int32_t signal_core_strength(int32_t signal_now)
{
    /* 0.00–1.00（千分点）⇒ 封顶在 HI_MARK（100 点），130 点仍是 1.00。
     * 依据：spec §2.1「显示上 ≥100 一律显示 1.00」+ §2.3「信号强度 = 0.00–1.00」。*/
    if (signal_now <= 0) return 0;
    if (signal_now >= SIGNAL_HI_MARK) return SIGNAL_UNIT;
    return signal_now * SIGNAL_UNIT / SIGNAL_HI_MARK;
}

int32_t signal_core_bonus(int32_t faith_total, int32_t graze_mult_permille, int32_t signal_now)
{
    int32_t mult = graze_mult_permille + signal_core_strength(signal_now);

    /* 负的擦弹倍率在数学上无意义（`mof_core_graze_mult` 不会给负数）⇒ 防御性夹 0。 */
    if (mult < 0) mult = 0;
    /* ⚠ 量纲：`faith_total` 是**原始单位**、`mult` 是千分点 ⇒ 乘完除 1000 才是分。
     *   先乘后除（而不是先除）—— 不然 `信仰 × 0.0001` 这种小数会整片被吃成 0。 */
    return (int32_t)(((int64_t)faith_total * (int64_t)mult) / SIGNAL_UNIT);
}

int32_t signal_core_bonus_fixed(int32_t faith_fixed, int32_t graze_mult_permille, int32_t signal_now)
{
    int32_t mult = graze_mult_permille + signal_core_strength(signal_now);

    if (mult < 0) mult = 0;
    /* 返回值仍是 ×FAITH_FIXED 的定点（调用方自己 /FAITH_FIXED 出槽）。 */
    return (int32_t)(((int64_t)faith_fixed * (int64_t)mult) / SIGNAL_UNIT);
}

/* ---- 契约 PV 总式（不含乘数的那一半；见 signal_core.h 的说明与偏差记录）---- */
int32_t signal_core_pv_111(const faith_slot_t *s111, int32_t aggregate, int have110)
{
    int64_t pv;

    if (!s111) return 0;
    if (!have110) {
        /* 只装 111：`PV = 信仰_111`（基础按难度 + Δ_111）。⚠ 不读聚合值 ——
         * 111 单装时聚合值也是它自己，但直接读槽更少一处约定（且与 §7.2 的基础口径绑定）。*/
        return faith_score_unit(s111);
    }
    /* 两卡同装：`PV = 基础均值 + Δ_110 + Δ_111`，而
     *   `aggregate` = 两卡各自的「基础 + Δ」之和（每张只声明一份自己的基础）
     * ⇒ `Δ 之和 = aggregate − 基础_110 − 基础_111` ⇒ 基础只取一份（§7.2 的铁律）。*/
    pv = (int64_t)(FAITH_BASE_110 + s111->base) / 2
         + (int64_t)aggregate - (int64_t)FAITH_BASE_110 - (int64_t)s111->base;
    if (pv < 0) pv = 0;
    if (pv > 0x7fffffff) pv = 0x7fffffff;
    return (int32_t)pv;
}

int32_t signal_core_pv_111_scaled(const faith_slot_t *s111, int32_t aggregate, int have110,
                                  int32_t graze_mult_permille, int32_t signal_now)
{
    int32_t base_pv = signal_core_pv_111(s111, aggregate, have110);
    int32_t mult = graze_mult_permille + signal_core_strength(signal_now);

    if (base_pv <= 0 || mult <= 0) return 0;
    return (int32_t)(((int64_t)base_pv * (int64_t)mult) / SIGNAL_UNIT);
}

int32_t signal_core_gauge_permille(int32_t signal_now)
{
    if (signal_now <= 0) return 0;
    if (signal_now >= SIGNAL_MAX) return SIGNAL_UNIT;
    return signal_now * SIGNAL_UNIT / SIGNAL_MAX;
}

int32_t signal_core_cell_permille(int32_t signal_now, int cell)
{
    int32_t lo, hi;      /* 本格的信号区间（千分点定点）*/

    if (cell < 0 || cell >= SIGNAL_CELLS) return 0;
    if (signal_now <= 0) return 0;
    /* 每格 = 130 / 5 = 26 点（= 26000 千分点）。用整数除，保证 5 格恰好铺满 0..130。 */
    lo = SIGNAL_MAX / SIGNAL_CELLS * cell;
    hi = SIGNAL_MAX / SIGNAL_CELLS * (cell + 1);
    if (hi <= lo) return 0;
    if (signal_now <= lo) return 0;
    if (signal_now >= hi) return SIGNAL_UNIT;
    return (signal_now - lo) * SIGNAL_UNIT / (hi - lo);
}
