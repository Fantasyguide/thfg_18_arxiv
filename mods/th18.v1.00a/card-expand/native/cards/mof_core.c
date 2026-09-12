/* mof_core.c —— 风神录（110）的纯逻辑实现。接口与依据见 `mof_core.h`。
 * 主机单测：`tests/test_mof_core.c`。
 *
 * ⚠ 本文件包含 `../sdk/public.h` 只为拿 `CE_EDEATH_*`（死亡事件的 `kind` / `flags` 常量）。
 *   那个头**是纯 C**（只有 `<stdint.h>` + 类型 + 宏，**不含任何 TH18 地址或 ABI 包装**）
 *   ⇒ 不破坏「core 层无引擎地址」的规矩，也不需要 `-I.`（相对路径对
 *   `cc tests/test_mof_core.c cards/mof_core.c` 同样成立）。**别再自己镜像一份常量**（会漂）。*/
#include <stdint.h>
#include "../sdk/public.h"
#include "mof_core.h"

/* ---- 大信仰点的难度表（§7.1 / th10 一手 `0x41b57f`：E/N 5000 · H 8000 · L/EX 10000）---- */
int32_t mof_core_big_value(int difficulty) { return faith_big_mof(difficulty); }

/* ---- 大信仰点判据（§7.9：不按族挑，按「这敌实际掉什么」）---- */
int mof_core_death_qualifies(uint32_t kind, uint32_t flags)
{
    /* ★ killAll 一族**不算击破**（review §4.2 的 ⚠ killAll 坑）：那条路在调 `0x430240` 之前
     * 已经 `memset(enemy+0x6240,0,0xc0)` 并清主掉落 ⇒ 清场**杀不掉道具**，却仍会被死亡断点
     * 计到「击破」。过场全清一次就把计数刷满 ⇒ 必须排除。*/
    if (kind == CE_EDEATH_KILL_ALL) return 0;
    /* 只认真敌人（`(flags_lo & 0x31) == 0`；**置位 = 被排除在真敌人之外**，极性见 review §11 #42）*/
    if (!(flags & CE_EDEATH_F_REAL)) return 0;
    /* 没有掉落表就谈不上「掉钱」⇒ 直接出局（`CE_EDEATH_F_HAS_DROP` ⇔ `drop_main != 0`）*/
    if (!(flags & CE_EDEATH_F_HAS_DROP)) return 0;
    return 1;
}

int mof_core_wants_big_main(int32_t drop_main, int is_boss)
{
    if (is_boss) return 0;                       /* boss 不掉（§7.9）*/
    return (drop_main == MOF_DROP_TYPE_MONEY) ? 1 : 0;
}

int mof_core_wants_big_dual(int32_t drop_main, int32_t money_count, int32_t p_count, int is_boss)
{
    if (is_boss) return 0;
    if (drop_main == MOF_DROP_TYPE_MONEY) return 0;   /* 走判据①，别重复计 */
    return (money_count > 0 && p_count > 0) ? 1 : 0;
}

int mof_core_wants_big(int32_t drop_main, int32_t money_count, int32_t p_count, int is_boss,
                       int difficulty, int32_t *value_out)
{
    int want = mof_core_wants_big_main(drop_main, is_boss)
            || mof_core_wants_big_dual(drop_main, money_count, p_count, is_boss);

    if (value_out) *value_out = want ? mof_core_big_value(difficulty) : 0;
    return want;
}

int mof_core_wants_big_death(uint32_t kind, uint32_t flags, int32_t drop_main, int is_boss,
                             int difficulty, int32_t *value_out)
{
    int want;

    if (value_out) *value_out = 0;
    if (!mof_core_death_qualifies(kind, flags)) return 0;
    /* 死亡事件只带 `drop_main`，不带 ex_drop 计数 ⇒ 这里只能走判据①（`drop_main == 2`）。
     * 判据②（「同时掉钱与 P」）在 `on_enemy_drop_pre` 里用 `counts` 做（`mof.c` 的双路说明）。*/
    want = mof_core_wants_big_main(drop_main, is_boss);
    if (value_out && want) *value_out = mof_core_big_value(difficulty);
    return want;
}

mof_pick_t mof_core_pick_p_item(int32_t drop_main, int32_t p_count, int32_t power,
                                int32_t max_power, int is_boss)
{
    /* ⚠ TH18 没有「信仰点」这个 item type（§11 #48）⇒ 这里只判「这颗是不是 P 点」，
     * 撒出来的**一律是我们自建 VM**，不去改原生 type。*/
    if (is_boss) return MOF_PICK_DROP;
    if (p_count <= 0 && drop_main != MOF_DROP_TYPE_P) return MOF_PICK_DROP;
    if (max_power <= 0 || power < max_power) return MOF_PICK_DROP;   /* 不满 P 不转化 */
    return MOF_PICK_ADD_SMALL;
}

/* ---- 蓝点分值（§2.4 / §7.6）---- */
int32_t mof_core_height_coeff(float y, float poc_y, float board_y)
{
    float span, t;

    if (y <= poc_y) return MOF_BLUE_COEF_TOP;         /* 回收线以上（吃黄）= 满额 */
    span = board_y - poc_y;
    if (span <= 0.0f) return MOF_BLUE_COEF_BOTTOM;
    t = (y - poc_y) / span;                           /* 0 = 线上，1 = 板底 */
    if (t > 1.0f) t = 1.0f;                           /* 板底以下夹到 BOTTOM（不外推）*/
    return (int32_t)((float)MOF_BLUE_COEF_TOP
                     + ((float)MOF_BLUE_COEF_BOTTOM - (float)MOF_BLUE_COEF_TOP) * t);
}

/* ★ 生产路径（`mof.c` 的 `mof_on_pickup()`）用的那一支：板底固定 `MOF_BOARD_Y`。
 * 为什么单独一层：把「板底常量 + 收集高度」这一个口径钉在一个**有单测**的函数里，
 * 卡侧只负责把「收集那一刻的高度」喂进来（旧实现把口径写在卡侧 ⇒ 主机编不出来、也测不到，
 * 于是二元 TOP/BOTTOM 一路留到现在，插值函数成了死代码）。*/
int32_t mof_core_pickup_coeff(float collect_y, float poc_y)
{
    return mof_core_height_coeff(collect_y, poc_y, MOF_BOARD_Y);
}

int32_t mof_core_blue_value(int32_t faith, int32_t coeff_permille)
{
    int64_t v;

    if (faith <= 0 || coeff_permille <= 0) return 0;
    v = ((int64_t)faith * (int64_t)coeff_permille) / (int64_t)MOF_BLUE_COEF_TOP;
    if (v > 0x7fffffff) v = 0x7fffffff;
    return (int32_t)v;
}

/* ---- 卡内奖残（§2.6）---- */
void mof_bonus_init(mof_bonus_t *b)
{
    if (!b) return;
    b->bonus = 0;
    b->awarded = 0;
}

void mof_bonus_add(mof_bonus_t *b, int64_t delta)
{
    if (!b || delta <= 0) return;
    b->bonus += delta;
    if (b->bonus > (int64_t)0x7fffffffffffffffLL / 2) b->bonus = (int64_t)0x7fffffffffffffffLL / 2;
}

int32_t mof_bonus_awarded(const mof_bonus_t *b) { return b ? b->awarded : 0; }

int32_t mof_bonus_threshold(int tier)
{
    switch (tier) {
    case 0:  return MOF_BONUS_M;
    case 1:  return MOF_BONUS_40M;
    case 2:  return MOF_BONUS_80M;
    case 3:  return MOF_BONUS_150M;
    default: return MOF_BONUS_150M;
    }
}

int mof_bonus_take(mof_bonus_t *b)
{
    int n = 0;

    if (!b) return 0;
    while (b->awarded < MOF_BONUS_MAX && b->bonus >= mof_bonus_threshold(b->awarded)) {
        ++b->awarded;
        ++n;
    }
    return n;
}

/* ---- 扣火力（§5.5.1 三个坑 + §3.2.1 的 1.05 下限）---- */
int mof_core_power_can(int32_t power)
{
    return power >= MOF_BOMB_POWER_MIN * FAITH_POWER_PER_LEVEL;
}

mof_power_t mof_core_power_spend(int32_t power)
{
    mof_power_t r;
    int32_t floor_p = MOF_BOMB_POWER_FLOOR;          /* 1.05 档（定点 105）*/
    int32_t min_p = MOF_BOMB_POWER_MIN * FAITH_POWER_PER_LEVEL;   /* 2.00 档 = 200 */

    r.ok = 0;
    r.new_power = power;
    if (power < min_p) return r;                     /* 门槛：TH18 ≥ 2.00 档 */
    r.ok = 1;
    r.new_power = power - MOF_BOMB_POWER_COST;       /* 每次扣 1 档 */
    if (r.new_power < floor_p) r.new_power = floor_p; /* ★ 下限 1.05（用户「サービス」）*/
    return r;
}

/* ---- 连击槽的按帧推进（★ 2026-09-12 一手订正：TH10 无逐帧信仰衰减）----
 * 只做两件事：① 有连击 ⇒ 每帧 −1（boss 战里不减）；② 其余情况**什么都不做**。
 * ⚠ 旧实现在「连击槽空」时调 `faith_slot_drain_empty(s, 1, enemies_present)` 扣信仰
 *   （无敌人 −10800/s、有敌人 −600/s）—— 一手已证 TH10 没有这个机制（见 `mof_core.h` 的注释：
 *   信仰写入点穷举只有 5 类、连击计时器只被充能/只被 HUD 读）⇒ 整段删掉，`*_lost` 恒 0。*/
int32_t mof_core_combo_tick(faith_slot_t *s, int in_boss_fight)
{
    if (!s) return 0;
    /* ★ 连击槽自己每帧 −1（充满 = 130f，所以「充能值」就是「能撑多少帧」，§2.3）。
     * ⚠ **boss 战中连击槽不衰减**（§2.3）⇒ boss 战里 tick 不减。
     * ⚠ 🟡 「不衰减」这条仍只有 spec（THBWiki）来源：一手研究只钉了「上限 130 + 各来源充能帧数」，
     *   没有记 `0x474c54` 这个 zTimer 的按帧递减与 boss 战豁免（research §3.4）。*/
    if (s->combo > 0 && !in_boss_fight) --s->combo;
    return 0;                                   /* 恒 0：不再有「槽空扣信仰」这条通路 */
}

/* ---- 收集事件档 / 连击帧数（表与依据见 `mof_core.h`）---- */
int32_t mof_core_pickup_combo(int tier)
{
    /* 0 = `FAITH_TIER_BIG`（大信仰点 +120f，一手 `0x41b5c5 push 0x78`）；
     * 其余实体档 = `FAITH_TIER_SMALL`（110 只有小信仰点 +60f，一手 `0x41b61d push 0x3c`）。
     * ⚠ 数值**不取 `faith_core.h` 的 `FAITH_COMBO_*`**：那张表是共用默认（111 也读），
     *   而一手订正的只有 110 的大信仰点（130 → 120）。见头注。*/
    return (tier == 0) ? MOF_COMBO_BIG : MOF_COMBO_SMALL_FAITH;
}

faith_ev_t mof_core_pickup_ev(int tier)
{
    /* tier 用数值（0 = BIG / 1 = SMALL）—— 不 include `faith.h`（引擎侧头）。 */
    return (tier == 0) ? FAITH_EV_BIG : FAITH_EV_SMALL_FAITH;
}

/* ---- 符卡「全避」监视器（接口与依据见 mof_core.h）---- */
int mof_core_spell_watch(mof_spell_watch_t *w, int spell_now, int timed_out_now)
{
    int fire = 0;

    if (!w) return 0;
    if (spell_now && !w->was) {                 /* 新符卡开始：两个标志都清 */
        w->bombed = 0;
        w->timed_out = 0;
    }
    /* 只在「上一帧已经在符卡里」时读超时位 ⇒ 新符卡第一帧的残留位不会被读进来 */
    if (w->was && timed_out_now) w->timed_out = 1;
    if (!spell_now && w->was) {                 /* 下降沿：结算这一张 */
        fire = (w->timed_out && !w->bombed);
        w->timed_out = 0;
    }
    w->was = spell_now;
    return fire;
}

/* ---- PV 公式（§7.4 / §7.8）---- */
int32_t mof_core_graze_mult(int32_t graze)
{
    if (graze <= 0) return 0;
    return graze > FAITH_GRAZE_MULT_MAX ? FAITH_GRAZE_MULT_MAX : graze;
}

int32_t mof_core_pv(int have110, int have111, int32_t aggregate, int32_t graze_mult_permille,
                    int32_t signal_value)
{
    int64_t base, pv;
    int64_t mult = (int64_t)graze_mult_permille + (int64_t)signal_value;
    int64_t agg = aggregate;

    if (agg < 0) agg = 0;
    if (mult < 0) mult = 0;

    if (!have110 && !have111) {
        /* 两张都不在：这个断点本来也不该有人落笔。返回 0 只是防御
         *（卡侧先问 `ce_score_applier() == ce_card_id(c)` 才动手）。*/
        return 0;
    }

    if (have110 && !have111) {
        /* 只装 110：`PV = 信仰_110`（**无乘数** —— MoF 里信仰就是最大得点，§7.4）。
         * 而 `aggregate` = 110 声明的信仰（基础 + Δ）⇒ 直接就是答案。*/
        pv = agg;
    } else if (!have110 && have111) {
        /* 只装 111：`PV = 信仰_111 × (擦弹倍率 + 信号强度)`。
         * ⚠ 111 单装时**落笔者是 111 自己**（id 111 > 110，但 110 不在场 ⇒ 聚合里只有 111），
         *   它用自己那份 `base(难度) + Δ_111` 作 `aggregate` 传进来（§7.2：基础按难度）。*/
        pv = agg * mult / 1000;
    } else {
        /* 两个都装：`PV = (基础均值 + Δ_110 + Δ_111) × (擦弹倍率 + 信号强度)`。
         * ⚠ **基础只取一份**（§7.2：绝不能再相加）；而 `aggregate` 是两张卡的**完整**声明之和
         *   = (基础_110 + Δ_110) + (基础_111 + Δ_111) ⇒ 要先把两份基础减掉，才是 Δ 之和。
         *   （2026-09-12 修：原实现写成 `(base + agg) × mult`，把基础记了三遍 ——
         *     NORMAL 下 (50000 + 100000) × 1.0 = 150000，而契约是 50000。111 侧的
         *     `signal_core_pv_111()` 一直是按契约实现的 ⇒ 两卡同装时两边结果不一致，这里对齐到契约。）
         * 🟡 `FAITH_BASE_111_*` 随难度变（EASY 25000 … LUNATIC 200000），本函数拿不到难度 ⇒ 按 NORMAL 反推；
         *    EASY/LUNATIC 下会差 `(基础_111_真实 − 50000)`。需要精确时给本函数加 difficulty 参数（改调用方）。*/
        int64_t delta;
        base  = ((int64_t)FAITH_BASE_110 + FAITH_BASE_111_NORMAL) / 2;
        delta = agg - (int64_t)FAITH_BASE_110 - (int64_t)FAITH_BASE_111_NORMAL;
        if (delta < 0) delta = 0;
        pv = (base + delta) * mult / 1000;
    }
    if (pv > 0x7fffffff) pv = 0x7fffffff;
    return (int32_t)pv;
}
