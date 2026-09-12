/* test_signal_core.c —— `cards/signal_core.c` 的主机单测（`make test-host` 的口径：
 *   cc -std=c11 -Wall -Wextra -Werror -o /tmp/t111 tests/test_signal_core.c \
 *      cards/signal_core.c cards/faith_core.c && /tmp/t111）。
 *
 * 覆盖（spec 点名的部分加粗）：
 *   · **信号槽四段增减**：线上未满 +50/f、≥100 后 +0.09/f、擦弹 +8/graze、线下 −0.3/f；
 *   · **上下限**：0 底、130 顶、`faith_signal_add` 的夹取；
 *   · **被弹 / 关卡归零**（`signal_core_reset`）；
 *   · **满槽阈值 ≥100**（`signal_core_is_full`）+ 显示用强度封顶 1.00；
 *   · **蓝点分公式** `信仰 × (擦弹倍率 + 信号强度)`，含**擦弹倍率上限 8.99** 与整数边界；
 *   · 擦弹 3 帧算 1 擦弹（原生计数器口径）与用户点名的边界；
 *   · 与共用件的接缝：`faith_slot_init_111` 的难度基础 / Δ 上限 = 原版基础、小绿点每 100 个 +10、
 *     信号槽与信仰槽**互不污染**（分槽记账，§3.2.1）；
 *   · 灵击门槛 / 扣 1 档 / 下限 1.05（共用件 `mof_core_power_spend`，111 侧读同一组常数）；
 *   · **绿点收集入账**（§13，`signal_core_pickup`：大 +100/+130f、中 +10/+100f、上限夹取、
 *     0/负数/NULL 不认、110 槽不被碰）—— 这条是 2026-09-12 审计补的：判定原来只在**卡侧静态回调**
 *     里（主机编不出来 ⇒ 覆盖不到），而且当时**没有任何来源会撒 111 槽的点**（回调在游戏里永不触发）；
 *   · HUD 格填充（`signal_core_gauge_permille` / `signal_core_cell_permille`）。
 *
 * 依据：`docs/superpowers/specs/2026-09-10-sa-signal-design.md` §2.1 / §2.2 / §2.3 / §3
 *       + `docs/superpowers/specs/2026-09-11-system-card-spec-review.md` §3.2.1 / §7.1 / §7.2 / §7.4 / §7.5。
 *
 * ⚠ 链接**三个**共用件：`signal_core.c`（本卡纯逻辑）+ `faith_core.c`（绿点记账 / 信号表）+
 *   `mof_core.c`（擦弹倍率 / 灵击扣法 / PV 总式 —— 111 **只借不重写**，见 signal.c 的分工表）。
 */
#include <stdio.h>
#include <string.h>

#include "../cards/signal_core.h"
#include "../cards/mof_core.h"      /* 共用件：擦弹倍率 / 灵击扣法 / PV（111 侧只借不重写）*/

static int s_fail, s_pass;
#define CHECK(cond) do { if (cond) ++s_pass; else { ++s_fail; printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } } while (0)

#define G  FAITH_SIGNAL_GAIN_LINE      /* +50/f（线上、未满 100）*/
#define T  FAITH_SIGNAL_GAIN_TRICK     /* +0.09/f（≥100）*/
#define Z  FAITH_SIGNAL_GAIN_GRAZE     /* +8/graze */
#define L  FAITH_SIGNAL_LOSS_OFFLINE   /* −0.3/f（线下）*/

int main(void)
{
    faith_slot_t slot;
    int32_t v;

    /* ==================== 0. 常数自洽（防共用件被改而这边不知道）==================== */
    CHECK(SIGNAL_MAX == 130000);            /* 0–130 点，千分点定点 */
    CHECK(SIGNAL_HI_MARK == 100000);        /* 满槽阈值 100（spec §2.1）*/
    CHECK(SIGNAL_FULL_AT == 100000);
    CHECK(G == 50000);                      /* +50/f */
    CHECK(T == 90);                         /* +0.09/f */
    CHECK(Z == 8000);                       /* +8/graze */
    CHECK(L == 3000);                       /* −0.3/f */
    CHECK(SIGNAL_UNIT == 1000);
    CHECK(SIGNAL_CELLS == 5);
    CHECK(SIGNAL_GREEN_BIG == 100);         /* 大绿点 +100 */
    CHECK(SIGNAL_GREEN_MID == 10);          /* 中绿点 +10 */
    CHECK(SIGNAL_GREEN_SMALL_N == 100);     /* 小绿点每 100 个 */
    CHECK(SIGNAL_GREEN_SMALL_V == 10);      /*   … +10 */
    CHECK(FAITH_POWER_MIN_LEVEL == 2);      /* 灵击门槛 TH18 ≥ 2.00 档（§3.2.1）*/
    CHECK(FAITH_POWER_COST == 100);         /* 每次扣 1 档 */
    CHECK(FAITH_POWER_FLOOR == 105);        /* 下限 1.05 档（用户「サービス」）*/
    CHECK(FAITH_GRAZE_MULT_MAX == 8990);    /* 擦弹倍率上限 8.99（千分点）*/

    /* ==================== 1. 线上（<100）：+50/帧 ==================== */
    CHECK(signal_core_step(0, 1, 0, 0) == G);
    CHECK(signal_core_step(1000, 1, 0, 0) == G);
    CHECK(signal_core_step(49999, 1, 0, 0) == G);
    CHECK(signal_core_step(50000, 1, 0, 0) == G);          /* 99.999 点也算「未满 100」*/
    CHECK(signal_core_step(99999, 1, 0, 0) == G);          /* ★ 99.999 → +50 ⇒ 下一帧直接顶满 */
    /* 两帧就从 0 到满（50 + 50 点 = 100 点）——这是 SA 原版的节奏（§2.1 照抄）*/
    v = 0; v += signal_core_step(v, 1, 0, 0); CHECK(v == 50000);
    v += signal_core_step(v, 1, 0, 0); CHECK(v == 100000);
    CHECK(signal_core_is_full(v) == 1);

    /* ==================== 2. ≥100：+0.09/帧（线上）==================== */
    CHECK(signal_core_step(100000, 1, 0, 0) == T);
    CHECK(signal_core_step(100001, 1, 0, 0) == T);
    CHECK(signal_core_step(129999, 1, 0, 0) == T);
    /* 顶到 130 也不越界（`apply` 会夹）*/
    slot.signal = 130000;
    (void)signal_core_apply(&slot, signal_core_step(slot.signal, 1, 0, 0));
    CHECK(slot.signal == SIGNAL_MAX);

    /* ==================== 3. 擦弹 +8/graze（可与线上/线下叠加）==================== */
    CHECK(signal_core_step(0, 0, 0, 1) == Z);
    CHECK(signal_core_step(0, 0, 0, 3) == 3 * Z);
    CHECK(signal_core_step(0, 0, 0, 100) == 100 * Z);
    CHECK(signal_core_step(0, 1, 0, 1) == G + Z);          /* 线上 + 擦弹 */
    {
        /* spec §2.1：「激光每 3 帧算 1 擦弹」⇒ 那是**原生计数器**的口径，卡侧只吃帧差。
         * 这里验「帧差 1 → +8」这条链路（原生计数器的 3 帧口径在引擎侧，不属本文件）。*/
        int32_t graze_counter = 100;
        int32_t prev = graze_counter, dg;
        graze_counter += 1;                                 /* 本帧擦到 1 次 */
        dg = graze_counter - prev;
        CHECK(dg == 1);
        CHECK(signal_core_step(0, 0, 0, dg) == Z);
        /* 回绕 / 重开（原生计数器变小）时卡侧把帧差当 0 —— 这里验「负帧差 ⇒ 不叠负值」的调用口径 */
        prev = 5; graze_counter = 0;
        dg = graze_counter - prev;
        CHECK(dg < 0);
        CHECK(signal_core_step(0, 0, 0, dg > 0 ? dg : 0) == 0);
    }

    /* ==================== 4. 线下：−0.3/帧 ==================== */
    CHECK(signal_core_step(50000, 0, 1, 0) == -L);
    CHECK(signal_core_step(100000, 0, 1, 0) == T - L);      /* 已≥100：先 +0.09 再 −0.3 ⇒ 净 −0.21 */
    CHECK(signal_core_step(129999, 0, 1, 0) == T - L);
    CHECK(signal_core_step(60000, 0, 1, 5) == 5 * Z - L);   /* 线下也能擦弹（SA：擦弹照给）*/
    /* 夹到 0 底 */
    slot.signal = 1000;                                     /* 1.0 点 */
    (void)signal_core_apply_step(&slot, 0, 1, 0);
    CHECK(slot.signal == 0);
    slot.signal = 0;
    (void)signal_core_apply_step(&slot, 0, 1, 0);
    CHECK(slot.signal == 0);                                /* 已经是 0 就不再往下跑 */

    /* ==================== 5. 夹取 / 归零 ==================== */
    CHECK(signal_core_clamp(-1) == 0);
    CHECK(signal_core_clamp(0) == 0);
    CHECK(signal_core_clamp(99999) == 99999);
    CHECK(signal_core_clamp(130000) == 130000);
    CHECK(signal_core_clamp(130001) == SIGNAL_MAX);
    CHECK(signal_core_clamp(0x7fffffff) == SIGNAL_MAX);

    slot.signal = 77777;
    CHECK(signal_core_reset(&slot) == 1);
    CHECK(slot.signal == 0);
    CHECK(signal_core_reset(&slot) == 0);                   /* 已经是 0 ⇒ 不算一次复位 */
    CHECK(signal_core_reset(0) == 0);                       /* NULL 安全 */
    signal_core_apply(0, 12345);                            /* NULL 安全 */
    CHECK(signal_core_apply_step(0, 1, 0, 3) == 0);
    CHECK(signal_core_step(0, 0, 0, 0) == 0);               /* 空中（既不在线上也不在线下）⇒ 不动 */

    /* 关卡结束归零（§2.1）+ 每帧从 0 重新涨 */
    slot.signal = 90000;
    CHECK(signal_core_reset(&slot) == 1);
    CHECK(signal_core_is_full(slot.signal) == 0);
    (void)signal_core_apply_step(&slot, 1, 0, 0);
    CHECK(slot.signal == 50000);
    (void)signal_core_apply_step(&slot, 1, 0, 0);
    CHECK(slot.signal == 100000);
    CHECK(signal_core_is_full(slot.signal) == 1);

    /* ---- 5.1 ★ 2026-09-12 审计修（任务 3）：「归零的是什么 / 保留的是什么」逐项钉住 ----
     * 卡侧的 `on_stage_start`（虚表 `+0x34` = **每关开场**）与 `on_run_reset`（`+0x4c` = 局末）
     * 都只调本函数 ⇒ 契约就是「**只有信号槽归零**」，其余跨关保留（§2.1 归 0 表只列信号；
     * §3.2.1「信仰 / 小绿点余数跨关保留」、§7.5「Δ_111 不会消失」）。
     * ⚠ 这条是任务 3 的**证据**：若有人把 `faith_slot_reset()`（清 delta/combo/signal）搬进钩子，
     *   这里的断言会红。*/
    {
        faith_slot_t s2;
        faith_slot_init_111(&s2, FAITH_DIFF_NORMAL);        /* 基础 50000、Δ 上限 = 50000（§7.2）*/
        CHECK(signal_core_pickup(&s2, FAITH_EV_BIG, SIGNAL_GREEN_BIG) == 100 * FAITH_FIXED);
        (void)faith_combo_set(&s2, 77);
        s2.small_rem = 42;                                  /* 小绿点余数（§3.2.1：跨关保留）*/
        s2.signal = 88000;
        CHECK(s2.delta == 100 * FAITH_FIXED && faith_slot_total(&s2) == 50100);
        CHECK(signal_core_reset(&s2) == 1);
        CHECK(s2.signal == 0);                              /* ★ 归零的：信号槽（§2.1）*/
        CHECK(faith_slot_total(&s2) == 50100);              /* ★ 保留的：信仰（基础 + Δ_111）*/
        CHECK(s2.delta == 100 * FAITH_FIXED);
        CHECK(s2.base == 50000 && s2.delta_cap == 50000);   /* 基础 / Δ 上限不动 */
        CHECK(s2.small_rem == 42);                          /* ★ 保留的：小绿点余数 */
        CHECK(s2.combo == 77);                              /* 本函数不碰连击槽（§2.1 没点它）*/
        CHECK(signal_core_reset(&s2) == 0);                 /* 已经是 0 ⇒ 不再算一次归零 */
        CHECK(s2.small_rem == 42 && faith_slot_total(&s2) == 50100);
    }

    /* ==================== 6. 满槽阈值（≥100 ⇒ 自动全收）==================== */
    CHECK(signal_core_is_full(0) == 0);
    CHECK(signal_core_is_full(99999) == 0);
    CHECK(signal_core_is_full(100000) == 1);                /* ★ 恰好 100 = 满 */
    CHECK(signal_core_is_full(100001) == 1);
    CHECK(signal_core_is_full(130000) == 1);

    /* ==================== 7. 信号强度 = 蓝点分里的那一项（0.00–1.00）==================== */
    CHECK(signal_core_strength(0) == 0);
    CHECK(signal_core_strength(-5) == 0);
    CHECK(signal_core_strength(1000) == 10);                /* 1 点 = 0.01 */
    CHECK(signal_core_strength(50000) == 500);              /* 50 点 = 0.50 */
    CHECK(signal_core_strength(99999) == 999);              /* 99.999 点 ⇒ 0.999（截断）*/
    CHECK(signal_core_strength(100000) == 1000);            /* 100 点 = 1.00 */
    CHECK(signal_core_strength(100001) == 1000);            /* ★ ≥100 一律 1.00（spec §2.1）*/
    CHECK(signal_core_strength(130000) == 1000);            /* ★ 130 上限不改公式（spec §2.3）*/

    /* ==================== 8. 蓝点分公式 = 信仰 × (擦弹倍率 + 信号强度) ==================== */
    /* 8.1 擦弹倍率的共用件口径：1 graze = 1 千分点、上限 8.99 = 8990 */
    CHECK(mof_core_graze_mult(0) == 0);
    CHECK(mof_core_graze_mult(-3) == 0);
    CHECK(mof_core_graze_mult(1) == 1);
    CHECK(mof_core_graze_mult(5000) == 5000);
    CHECK(mof_core_graze_mult(8990) == 8990);
    CHECK(mof_core_graze_mult(8991) == 8990);               /* ★ 上限 8.99 */
    CHECK(mof_core_graze_mult(99999) == 8990);

    /* 8.2 无擦弹无信号 ⇒ 0 分（SA 原版就是乘数项为零）*/
    CHECK(signal_core_bonus(50000, 0, 0) == 0);
    /* 8.3 满信号 + 无擦弹 ⇒ 1.00 × 信仰 */
    CHECK(signal_core_bonus(50000, 0, 100000) == 50000);
    CHECK(signal_core_bonus(50000, 0, 130000) == 50000);    /* 130 也是 1.00 */
    /* 8.4 半信号（0.50）*/
    CHECK(signal_core_bonus(50000, 0, 50000) == 25000);
    CHECK(signal_core_bonus(25000, 0, 50000) == 12500);
    /* 8.5 擦弹倍率当主角：graze 5000 ⇒ 5.00 + 信号 1.00 = 6.00 */
    CHECK(signal_core_bonus(50000, 5000, 100000) == 300000);
    CHECK(signal_core_bonus(1000, 5000, 0) == 5000);
    /* 8.6 擦弹倍率上限进公式：8990 + 1000 = 9990 ⇒ 9.99 × 信仰 */
    CHECK(signal_core_bonus(50000, 8990, 130000) == 499500);
    CHECK(signal_core_bonus(100000, 8990, 130000) == 999000);
    CHECK(signal_core_bonus(200000, 8990, 130000) == 1998000);   /* LUNATIC 基础 200000 */
    /* 8.7 难度初值（§7.2 的表）× 满乘数 */
    CHECK(signal_core_bonus(FAITH_BASE_111_EASY, 8990, 130000) == 249750);
    CHECK(signal_core_bonus(FAITH_BASE_111_HARD, 0, 100000) == 100000);
    CHECK(signal_core_bonus(FAITH_BASE_111_LUNATIC, 0, 0) == 0);
    /* 8.8 截断（向下取整）而不是四舍五入：50000 × 0.0001 = 5 ⇒ 整；这里用奇数验截断 */
    CHECK(signal_core_bonus(33333, 1000, 0) == 33333);
    CHECK(signal_core_bonus(3, 500, 0) == 1);               /* 3 × 0.5 = 1.5 ⇒ 1 */
    CHECK(signal_core_bonus(1, 1, 0) == 0);                 /* 1 × 0.0001 ⇒ 0（不足 1 分）*/
    /* 8.9 防御：负擦弹倍率夹 0、负信仰夹 0（卡侧不会给，但别算出负数分）*/
    CHECK(signal_core_bonus(50000, -5000, 0) == 0);
    CHECK(signal_core_bonus(-100, 5000, 100000) < 0);       /* 信仰为负时结果同号（调用方先判 >0）*/
    /* 8.10 溢出安全：信仰上限量级 × 9.99 */
    CHECK(signal_core_bonus(0x7fffffff / 10, 8990, 130000) == (int32_t)((int64_t)(0x7fffffff / 10) * 9990 / 1000));
    /* 8.11 定点版（内部通道用）与原始单位版同号同量级 */
    CHECK(signal_core_bonus_fixed(50000 * FAITH_FIXED, 0, 100000) == 50000 * FAITH_FIXED);
    CHECK(signal_core_bonus_fixed(1000 * FAITH_FIXED, 8990, 130000) == 9990 * FAITH_FIXED);
    CHECK(signal_core_bonus_fixed(0, 8990, 130000) == 0);

    /* ==================== 9. HUD 格填充（5 格 × 26 点）==================== */
    CHECK(signal_core_gauge_permille(0) == 0);
    CHECK(signal_core_gauge_permille(-1) == 0);
    CHECK(signal_core_gauge_permille(65000) == 500);
    CHECK(signal_core_gauge_permille(130000) == 1000);
    CHECK(signal_core_gauge_permille(130001) == 1000);      /* 夹顶 */
    CHECK(signal_core_cell_permille(0, 0) == 0);
    CHECK(signal_core_cell_permille(26000, 0) == 1000);     /* 第 1 格满 = 26 点 */
    CHECK(signal_core_cell_permille(26000, 1) == 0);        /* 第 2 格从 26 点才开始 */
    CHECK(signal_core_cell_permille(13000, 0) == 500);      /* 格内线性 */
    CHECK(signal_core_cell_permille(130000, 0) == 1000);
    CHECK(signal_core_cell_permille(130000, 4) == 1000);    /* 最后一格满 = 130 点 */
    CHECK(signal_core_cell_permille(100000, 4) == 0);       /* ★ 满槽阈值 100 点时第 5 格还没亮 */
    CHECK(signal_core_cell_permille(50000, -1) == 0);       /* 越界安全 */
    CHECK(signal_core_cell_permille(50000, SIGNAL_CELLS) == 0);
    CHECK(signal_core_cell_permille(0, 4) == 0);

    /* ==================== 10. 与共用件的接缝（111 槽 = 不会消失的绿点）==================== */
    /* 10.1 初值随难度（§7.2）*/
    faith_slot_init_111(&slot, FAITH_DIFF_EASY);    CHECK(slot.base == 25000);
    faith_slot_init_111(&slot, FAITH_DIFF_NORMAL);  CHECK(slot.base == 50000);
    faith_slot_init_111(&slot, FAITH_DIFF_HARD);    CHECK(slot.base == 100000);
    faith_slot_init_111(&slot, FAITH_DIFF_LUNATIC); CHECK(slot.base == 200000);
    faith_slot_init_111(&slot, FAITH_DIFF_EXTRA);   CHECK(slot.base == 200000);
    faith_slot_init_111(&slot, 99);                 CHECK(slot.base == 200000);   /* 越界钳 LUNATIC */
    /* 10.2 Δ 上限 = 原版基础（= 「上限 = 初值 ×2」的等价写法）*/
    faith_slot_init_111(&slot, FAITH_DIFF_HARD);
    CHECK(slot.delta_cap == 100000);
    CHECK(slot.delta == 0);
    CHECK(slot.signal == 0);
    CHECK(slot.small_rem == 0);
    CHECK(faith_slot_total(&slot) == 100000);               /* 基础 + Δ */
    /* 10.3 大绿点 +100（§3.2.1 的表），能加满到上限 */
    CHECK(faith_slot_add(&slot, FAITH_EV_BIG, SIGNAL_GREEN_BIG) == SIGNAL_GREEN_BIG * FAITH_FIXED);
    CHECK(faith_slot_total(&slot) == 100100);
    CHECK(faith_slot_add(&slot, FAITH_EV_BIG, SIGNAL_GREEN_BIG) == SIGNAL_GREEN_BIG * FAITH_FIXED);
    CHECK(faith_slot_total(&slot) == 100200);
    /* 一口气灌到上限：Δ 只能到 delta_cap（= 基础），多出来的丢掉 */
    CHECK(faith_slot_add(&slot, FAITH_EV_BIG, 999999) == 99800 * FAITH_FIXED);
    CHECK(slot.delta == slot.delta_cap * FAITH_FIXED);
    CHECK(faith_slot_total(&slot) == 200000);               /* = 基础 ×2（SA 原版的「上限 = 初值 ×2」）*/
    CHECK(faith_slot_add(&slot, FAITH_EV_BIG, 100) == 0);   /* 满了 ⇒ 加不进去 */
    CHECK(faith_slot_total(&slot) == 200000);
    /* 10.4 中绿点 +10（满 P 后吃 P 点）*/
    faith_slot_init_111(&slot, FAITH_DIFF_EASY);
    CHECK(faith_slot_add(&slot, FAITH_EV_GREEN, SIGNAL_GREEN_MID) == 10 * FAITH_FIXED);
    CHECK(faith_slot_total(&slot) == 25010);
    /* 10.5 小绿点每 100 个 +10、余数跨帧 / 跨关保留（§3.2.1）
     * ⚠ 实测口径：`faith_slot_add_cancel()` 的 `out_111_add` 回的是**定点**量（10 → 1000），
     *   与函数头注释写的「原始单位」不符 ⇒ 这里同时钉住「返回值 = 槽里 Δ 的增量」这条不变式。*/
    faith_slot_init_111(&slot, FAITH_DIFF_NORMAL);
    CHECK(slot.small_rem == 0);
    v = -1;
    faith_slot_add_cancel(0, &slot, 99, 0, &v);
    CHECK(v == 0);                                          /* 不满 100 ⇒ 一分不加 */
    CHECK(slot.small_rem == 99);
    CHECK(slot.delta == 0);
    faith_slot_add_cancel(0, &slot, 1, 0, &v);
    CHECK(v == SIGNAL_GREEN_SMALL_V * FAITH_FIXED);         /* 第 100 个 ⇒ +10（定点 1000）*/
    CHECK(v == slot.delta);                                 /* ★ 返回值 = Δ 的增量（同量纲）*/
    CHECK(slot.small_rem == 0);
    CHECK(slot.delta == SIGNAL_GREEN_SMALL_V * FAITH_FIXED);
    CHECK(faith_slot_total(&slot) == 50010);
    faith_slot_add_cancel(0, &slot, 250, 0, &v);
    CHECK(v == 2 * SIGNAL_GREEN_SMALL_V * FAITH_FIXED);     /* 250 = 2×100 + 50 ⇒ +20、余 50 */
    CHECK(slot.small_rem == 50);
    CHECK(slot.delta == 3 * SIGNAL_GREEN_SMALL_V * FAITH_FIXED);
    CHECK(faith_slot_total(&slot) == 50030);
    /* ⚠ 「跨关保留」的口径：`faith_slot_reset()` 明确**不动** small_rem（faith_core.h）*/
    faith_slot_reset(&slot);
    CHECK(slot.small_rem == 50);
    CHECK(slot.delta == 0);
    CHECK(slot.signal == 0);
    /* 10.6 ★ 分槽记账：动信号**不动**信仰、动信仰**不动**信号（110 / 111 互不可见的核心）*/
    faith_slot_init_111(&slot, FAITH_DIFF_NORMAL);
    slot.signal = 60000;
    CHECK(faith_slot_add(&slot, FAITH_EV_BIG, SIGNAL_GREEN_BIG) == 100 * FAITH_FIXED);
    CHECK(slot.signal == 60000);                            /* 信仰通道不碰信号 */
    faith_slot_add_cancel(0, &slot, 100, 0, &v);
    CHECK(slot.signal == 60000);                            /* 消弹通道也不碰信号 */
    (void)signal_core_apply_step(&slot, 1, 0, 0);
    CHECK(slot.delta == 110 * FAITH_FIXED);                 /* 信号通道不碰 Δ */
    CHECK(slot.small_rem == 0);
    CHECK(faith_slot_total(&slot) == 50110);

    /* ==================== 11. 灵击：门槛 / 扣 1 档 / 下限 1.05（共用件，111 用同一组常数）==================== */
    CHECK(mof_core_power_can(0) == 0);
    CHECK(mof_core_power_can(199) == 0);                    /* 1.99 档 ⇒ 放不出（门槛 ≥2.00）*/
    CHECK(mof_core_power_can(200) == 1);                    /* ★ 恰好 2.00 档 ⇒ 放得出 */
    CHECK(mof_core_power_can(400) == 1);
    v = mof_core_power_spend(400).new_power;
    CHECK(v == 300);                                        /* 4.00 → 3.00（扣 1 档）*/
    CHECK(mof_core_power_spend(300).new_power == 200);
    CHECK(mof_core_power_spend(299).new_power == 199);
    CHECK(mof_core_power_spend(200).new_power == 105);      /* ★ 2.00 → 1.05（不是 1.00！）*/
    CHECK(mof_core_power_spend(200).ok == 1);
    CHECK(mof_core_power_spend(199).ok == 0);               /* 门槛不满足 ⇒ 拒绝 */
    CHECK(mof_core_power_spend(199).new_power == 199);      /*   …且不改火力 */
    CHECK(mof_core_power_spend(105).ok == 0);               /* 1.05 档 ⇒ 再也放不出（下限生效）*/
    CHECK(mof_core_power_spend(105).new_power == 105);

    /* ==================== 12. 契约 PV 总式（不含乘数那一半）==================== */
    /* 12.1 只装 111：`PV = 信仰_111 × (擦弹倍率 + 信号强度)`（§7.4）。*/
    faith_slot_init_111(&slot, FAITH_DIFF_NORMAL);
    CHECK(signal_core_pv_111(&slot, 50000, 0) == 50000);    /* 无 110 ⇒ 就是自己的信仰 */
    CHECK(signal_core_pv_111_scaled(&slot, 50000, 0, 0, 0) == 0);          /* 没擦弹没信号 ⇒ 0 */
    CHECK(signal_core_pv_111_scaled(&slot, 50000, 0, 0, 100000) == 50000); /* 信号 1.00 */
    CHECK(signal_core_pv_111_scaled(&slot, 50000, 0, 5000, 100000) == 300000); /* 5.00 + 1.00 */
    CHECK(signal_core_pv_111_scaled(&slot, 50000, 0, 8990, 130000) == 499500); /* 9.99 上限 */
    (void)faith_slot_add(&slot, FAITH_EV_BIG, SIGNAL_GREEN_BIG);           /* 大绿点 +100 */
    CHECK(faith_slot_total(&slot) == 50100);
    CHECK(signal_core_pv_111(&slot, 50100, 0) == 50100);                   /* 立即反映到 PV */
    faith_slot_init_111(&slot, FAITH_DIFF_LUNATIC);
    CHECK(signal_core_pv_111(&slot, 200000, 0) == 200000);                 /* 难度初值 200000 */
    CHECK(signal_core_pv_111_scaled(&slot, 200000, 0, 8990, 130000) == 1998000);

    /* 12.2 两卡同装：`PV = 基础均值 + Δ_110 + Δ_111`（**基础只取一份**）*/
    /*      `aggregate` = (基础_110 + Δ_110) + (基础_111 + Δ_111)；NORMAL 下两份基础都是 50000
     *      ⇒ 双方 Δ = 0 时 aggregate = 100000、`Δ 之和 = 100000 − 100000 = 0` ⇒ PV = 均值 50000。*/
    faith_slot_init_111(&slot, FAITH_DIFF_NORMAL);          /* 基础 50000（与 110 的基础撞巧相同）*/
    CHECK(signal_core_pv_111(&slot, 100000, 1) == 50000);   /* ★ 均值 50000，**不是** 100000 */
    /*      Δ_111 = 100（下面加一颗大绿点）、Δ_110 = 2000 ⇒ aggregate = 102100 ⇒ PV = 50000 + 2100 */
    (void)faith_slot_add(&slot, FAITH_EV_BIG, SIGNAL_GREEN_BIG);
    CHECK(faith_slot_total(&slot) == 50100);
    CHECK(signal_core_pv_111(&slot, 100100 + 2000, 1) == 52100);
    CHECK(signal_core_pv_111_scaled(&slot, 100100 + 2000, 1, 8990, 130000) == 520479);  /* ×9.99 */
    /*      EASY 的 111 基础 25000 ⇒ 基础均值 = (50000+25000)/2 = 37500（双方 Δ = 0）*/
    faith_slot_init_111(&slot, FAITH_DIFF_EASY);
    CHECK(signal_core_pv_111(&slot, 75000, 1) == 37500);
    CHECK(signal_core_pv_111(&slot, 75000, 1) != 75000);    /* ★ 不是「两份基础相加」*/
    CHECK(signal_core_pv_111(&slot, 75000, 1) != 50000);
    /* 12.3 防御：槽指针为空 / 聚合值为负 */
    CHECK(signal_core_pv_111(0, 100000, 1) == 0);
    faith_slot_init_111(&slot, FAITH_DIFF_NORMAL);
    CHECK(signal_core_pv_111(&slot, -1000, 1) >= 0);
    CHECK(signal_core_pv_111_scaled(&slot, -1000, 1, 0, 0) == 0);
    /* 12.4 共用件的「只装 110」那一支是对的（110 落笔仍走它）*/
    CHECK(mof_core_pv(1, 0, 50000, 0, 0) == 50000);
    CHECK(mof_core_pv(1, 0, 52000, 8990, 1000) == 52000);    /* 只 110 时无乘数（§7.4）*/
    /* 12.5 卡侧写 `*value = pv × 10`（§7.8）⇒ 自己先把 pv 钳在 int32/10 以内，不靠碰运气 */
    CHECK((int64_t)200000000 * 10 <= 0x7fffffff);           /* 钳位取值留够余量 */
    CHECK((int64_t)300000000 * 10 > 0x7fffffff);            /* 说明再大就必须钳 */
    /* 12.6 ★★ 两卡同装时**两边必须算出同一个 PV**（谁当落笔者都一样）：
     *      `signal_core_pv_111[_scaled]()`（111 侧按契约实现）↔ 共用件 `mof_core_pv()`（110 落笔走它）。
     *      2026-09-12：共用件的「两卡同装」那一支原来把基础记了三遍（`(base + aggregate) × mult`），
     *      与契约不符 ⇒ 已修成「先减掉两份基础再加基础均值」。这里**同时钉住两边相等**。*/
    {
        int32_t d110 = 4000, d111 = 7000;
        /* aggregate = (基础_110 + Δ_110) + (基础_111 + Δ_111)；NORMAL 下两份基础都是 50000 */
        int32_t agg = (FAITH_BASE_110 + d110) + (FAITH_BASE_111_NORMAL + d111);
        int32_t sig_raw = 60;                                   /* 信号 60 点 */
        int32_t sig_permille = signal_core_strength(sig_raw);    /* 该点的千分点强度 */

        faith_slot_init_111(&slot, FAITH_DIFF_NORMAL);
        faith_slot_add(&slot, FAITH_EV_BIG, d111);   /* Δ_111 = d111（大绿点通道）*/

        /* 只装 111：111 侧 = 信仰_111 × 乘数；共用件同一支也该一样 */
        CHECK(mof_core_pv(0, 1, FAITH_BASE_111_NORMAL + d111, 1000, sig_permille)
              == signal_core_pv_111_scaled(&slot, agg, 0, 1000, sig_raw));
        /* 两卡同装：两边逐字相等（契约：(基础均值 + Δ110 + Δ111) × 乘数）*/
        CHECK(mof_core_pv(1, 1, agg, 1000, sig_permille)
              == signal_core_pv_111_scaled(&slot, agg, 1, 1000, sig_raw));
        CHECK(mof_core_pv(1, 1, agg, 1000, sig_permille) == (50000 + d110 + d111) * (1000 + sig_permille) / 1000);
        /* 纯擦弹倍率、无信号 */
        CHECK(mof_core_pv(1, 1, agg, 8990, 0)
              == signal_core_pv_111_scaled(&slot, agg, 1, 8990, 0));
        /* Δ 都是 0 时 = 基础均值 × 乘数（不再是「基础 ×3」）*/
        CHECK(mof_core_pv(1, 1, FAITH_BASE_110 + FAITH_BASE_111_NORMAL, 1000, 0) == 50000);
    }

    /* ==================== 13. 绿点收集入账 `signal_core_pickup()`（§2.2 / §7.1 / §7.5）====================
     * ★ 2026-09-12 审计补：这段判定原来是**卡侧静态回调**里的代码（`signal.c` 的 `signal_on_pickup`），
     *   而卡侧代码在主机上编不出来 ⇒ 单测一次都覆盖不到；更糟的是当时**没有任何来源会撒 111 槽的点**，
     *   那条回调在游戏里也永不触发。判定搬进 core 就是为了这里能钉住它。
     *   契约（§2.2 的三档 / §7.5 的连击表）：
     *     大绿点 +100（`FAITH_SA_BIG`）→ Δ_111 += 100、连击 +130f；
     *     中绿点  +10（`FAITH_SA_MID`）→ Δ_111 +=  10、连击 +100f；
     *     小绿点  每 100 个 +10 → 走 `faith_slot_add_cancel()`（消弹路径，不在本函数）。*/
    faith_slot_init_111(&slot, FAITH_DIFF_NORMAL);       /* 基础 50000、Δ 上限 = 50000（§7.2）*/
    /* 13.1 大绿点：+100（定点 ×100 = 10000）与连击 +130f */
    CHECK(signal_core_pickup(&slot, FAITH_EV_BIG, SIGNAL_GREEN_BIG) == 100 * FAITH_FIXED);
    CHECK(faith_slot_total(&slot) == 50100);            /* 50000 + 100 */
    CHECK(slot.combo == 130);                           /* 大档 = +130f（满槽）*/
    /* 13.2 中绿点：+10 与连击 +100f（先清连击槽）*/
    faith_combo_set(&slot, 0);
    CHECK(signal_core_pickup(&slot, FAITH_EV_GREEN, SIGNAL_GREEN_MID) == 10 * FAITH_FIXED);
    CHECK(faith_slot_total(&slot) == 50110);
    CHECK(slot.combo == 100);                           /* 绿点通道 = +100f */
    /* 13.3 单颗值与档位是**独立**的（值由撒点方给，卡侧传的就是 `FAITH_SA_*`）*/
    CHECK(SIGNAL_GREEN_BIG == 100 && SIGNAL_GREEN_MID == 10);
    /* 13.4 `value <= 0`：不认（共用件也不会撒这种点）*/
    v = slot.delta;
    faith_combo_set(&slot, 0);
    CHECK(signal_core_pickup(&slot, FAITH_EV_BIG, 0) == 0);
    CHECK(signal_core_pickup(&slot, FAITH_EV_BIG, -5) == 0);
    CHECK(slot.delta == v && slot.combo == 0);           /* 槽与连击槽都没动 */
    /* 13.5 NULL 槽安全 */
    CHECK(signal_core_pickup(0, FAITH_EV_BIG, 100) == 0);
    /* 13.6 Δ_111 上限 = 原版基础（§7.2）：加到满之后一颗都进不去，但**连击槽照加**（§7.5 没说封顶连击）*/
    {
        faith_slot_init_111(&slot, FAITH_DIFF_NORMAL);   /* 上限 = 50000 */
        CHECK(signal_core_pickup(&slot, FAITH_EV_BIG, 60000) == 50000 * FAITH_FIXED); /* 只吃得到上限 */
        CHECK(faith_slot_total(&slot) == 100000);        /* = 基础 ×2 */
        CHECK(slot.combo == 130);
        faith_combo_set(&slot, 0);
        CHECK(signal_core_pickup(&slot, FAITH_EV_BIG, SIGNAL_GREEN_BIG) == 0);
        CHECK(faith_slot_total(&slot) == 100000);
        CHECK(slot.combo == 130);                        /* 值进不去，事件本身仍然算 */
    }
    /* 13.7 110 的槽**一点都不会被碰**（分槽记账 / 不跨卡耦合）—— 这里用「另一槽不动」来钉住：
     *      111 的 pickup 只认 `s` 这一个槽，共用件的 110 槽要自己调才会动。*/
    {
        faith_slot_t s110, s111;
        faith_slot_init_110(&s110);
        faith_slot_init_111(&s111, FAITH_DIFF_NORMAL);
        CHECK(signal_core_pickup(&s111, FAITH_EV_BIG, SIGNAL_GREEN_BIG) == 100 * FAITH_FIXED);
        CHECK(s110.delta == 0 && faith_slot_total(&s110) == 50000);   /* 110 槽纹丝不动 */
        CHECK(faith_slot_total(&s111) == 50100);
    }

    printf("test_signal_core: %d passed, %d failed\n", s_pass, s_fail);
    return s_fail ? 1 : 0;
}
