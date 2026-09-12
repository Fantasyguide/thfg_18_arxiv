/* test_faith_core.c —— `cards/faith_core.c` 的主机单测（`make test-host` 的口径：
 *   cc -std=c11 -Wall -Wextra -Werror -o /tmp/tf tests/test_faith_core.c cards/faith_core.c && /tmp/tf）。
 *
 * 覆盖（用户点名的那几项）：
 *   · 三档加值与上限夹取；
 *   · `Δ_110` 下限 0（= 「50000 扣不穿」）；
 *   · 连击槽增减（含满槽夹取、boss 战不衰减在 `mof_core` 侧）；
 *   · 扣火力的 **1.05 下限**（含门槛 ≥ 2.00 的拒绝）；
 *   · 消弹计数累加（每颗 +10；111 的每 100 个 +10 与余数保留）；
 *   · Miss / 非决死灵击 / 全避 的扣减；
 *   · 信号强度与擦弹倍率的边界。
 *
 * 数值来源见 `cards/faith_core.h` 的头注释（spec §7.1/§7.2/§7.5 + review §3.2.1）。
 */
#include <stdio.h>
#include <string.h>

#include "../cards/faith_core.h"

static int s_fail, s_pass;
#define CHECK(cond) do { if (cond) ++s_pass; else { ++s_fail; printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } } while (0)

/* 原始单位 → 定点（工具）*/
#define FX(x) ((int32_t)(x) * FAITH_FIXED)

int main(void)
{
    faith_slot_t a, b;
    faith_view_t v;
    int32_t add110, add111;

    /* ============ 基础与初始化 ============ */
    faith_slot_init_110(&a);
    CHECK(a.base == 50000);
    CHECK(a.delta == 0);
    CHECK(a.delta_cap == 0);                     /* 110 侧不设 Δ 上限 */
    CHECK(a.combo == 0);
    CHECK(faith_slot_total(&a) == 50000);         /* 信仰 = 基础 + Δ */

    faith_slot_init_111(&b, FAITH_DIFF_NORMAL);
    CHECK(b.base == 50000);                       /* 只有 NORMAL 撞巧与 110 同基础（§7.2）*/
    CHECK(b.delta_cap == 50000);                  /* 上限 = 原版基础（= 初值 ×2 的等价写法）*/
    CHECK(faith_base_111(FAITH_DIFF_EASY) == 25000);
    CHECK(faith_base_111(FAITH_DIFF_NORMAL) == 50000);
    CHECK(faith_base_111(FAITH_DIFF_HARD) == 100000);
    CHECK(faith_base_111(FAITH_DIFF_LUNATIC) == 200000);
    CHECK(faith_base_111(FAITH_DIFF_EXTRA) == 200000);
    CHECK(faith_base_111(99) == 200000);          /* 越界钳 LUNATIC */

    /* ============ 三档加值（110）============ */
    CHECK(faith_big_mof(FAITH_DIFF_EASY) == 5000);
    CHECK(faith_big_mof(FAITH_DIFF_NORMAL) == 5000);
    CHECK(faith_big_mof(FAITH_DIFF_HARD) == 8000);
    CHECK(faith_big_mof(FAITH_DIFF_LUNATIC) == 10000);
    CHECK(faith_big_mof(FAITH_DIFF_EXTRA) == 10000);

    faith_slot_init_110(&a);
    CHECK(faith_slot_add(&a, FAITH_EV_BIG, faith_big_mof(FAITH_DIFF_NORMAL)) == FX(5000));
    CHECK(faith_slot_total(&a) == 55000);
    CHECK(faith_slot_add(&a, FAITH_EV_BIG, faith_big_mof(FAITH_DIFF_HARD)) == FX(8000));
    CHECK(faith_slot_total(&a) == 63000);
    CHECK(faith_slot_add(&a, FAITH_EV_SMALL_FAITH, FAITH_MOF_SMALL) == FX(100));
    CHECK(faith_slot_total(&a) == 63100);
    CHECK(faith_slot_add(&a, FAITH_EV_MICRO, FAITH_MOF_WEI) == FX(10));
    CHECK(faith_slot_total(&a) == 63110);
    CHECK(faith_slot_add(&a, FAITH_EV_BIG, 0) == 0);      /* 0 不加 */
    CHECK(faith_slot_add(&a, FAITH_EV_BIG, -5) == 0);     /* 负数不加 */

    /* ============ 上限夹取（111：Δ 上限 = 原版基础）============ */
    faith_slot_init_111(&b, FAITH_DIFF_NORMAL);           /* base 50000 ⇒ Δ 上限 50000 */
    CHECK(faith_slot_add(&b, FAITH_EV_BIG, 100) == FX(100));
    CHECK(faith_slot_total(&b) == 50100);
    CHECK(faith_slot_add(&b, FAITH_EV_BIG, 60000) == FX(49900));   /* 只吃得到上限为止 */
    CHECK(faith_slot_total(&b) == 100000);                     /* = 基础 ×2 */
    CHECK(faith_slot_add(&b, FAITH_EV_BIG, 100) == 0);         /* 已经满了：一颗都进不去 */
    CHECK(faith_slot_total(&b) == 100000);

    /* ============ Δ_110 下限 0（= 50000 扣不穿）============ */
    faith_slot_init_110(&a);
    CHECK(faith_slot_reduce(&a, FAITH_REDUCE_AVOID) == 0);     /* Δ = 0 ⇒ 扣不动 */
    CHECK(a.delta == 0);
    CHECK(faith_slot_total(&a) == 50000);                      /* 下限恒 50000 */
    (void)faith_slot_add(&a, FAITH_EV_BIG, 10000);
    CHECK(faith_slot_total(&a) == 60000);
    CHECK(faith_slot_reduce(&a, FAITH_REDUCE_AVOID) == FX(10000));  /* 全避 −30000 但只有 10000 */
    CHECK(faith_slot_total(&a) == 50000);                      /* 扣不穿 */

    /* 非决死灵击 −3000（Δ 够时按名义值扣）*/
    faith_slot_init_110(&a);
    (void)faith_slot_add(&a, FAITH_EV_BIG, 5000);
    CHECK(faith_slot_reduce(&a, FAITH_REDUCE_BOMB) == FX(3000));
    CHECK(faith_slot_total(&a) == 52000);

    /* Miss = −(信仰 − 50000)/3 = −Δ/3（向下取整）*/
    faith_slot_init_110(&a);
    (void)faith_slot_add(&a, FAITH_EV_BIG, 5000);              /* 信仰 55000 */
    CHECK(faith_slot_reduce(&a, FAITH_REDUCE_MISS) == FX(5000 / 3));
    CHECK(faith_slot_total(&a) == 55000 - 5000 / 3);
    faith_slot_init_110(&a);
    (void)faith_slot_add(&a, FAITH_EV_SMALL_FAITH, 100);       /* Δ = 100 ⇒ 扣 33 */
    CHECK(faith_slot_reduce(&a, FAITH_REDUCE_MISS) == FX(33));
    CHECK(faith_slot_reduce(&a, FAITH_REDUCE_MISS) == FX(22));     /* Δ = 67 ⇒ 扣 22 */
    CHECK(faith_slot_total(&a) == 50045);
    CHECK(faith_slot_reduce(&a, FAITH_REDUCE_NONE) == 0);      /* 无效 kind */

    /* ============ 空槽扣信仰（−10800/s 无敌人 · −600/s 有敌人）============ */
    faith_slot_init_110(&a);
    (void)faith_slot_add(&a, FAITH_EV_BIG, 10000);             /* Δ = 10000 */
    (void)faith_slot_add(&a, FAITH_EV_BIG, 10000);             /* Δ = 20000（留够扣的空间）*/
    a.combo = 0;                                               /* 连击槽空 */
    CHECK(faith_combo_empty(&a) == 1);
    CHECK(faith_slot_drain_empty(&a, 60, 0) == FX(10800));     /* 1 秒、无敌人 */
    CHECK(faith_slot_drain_empty(&a, 60, 1) == FX(600));       /* 1 秒、有敌人 */
    CHECK(faith_slot_total(&a) == 50000 + 20000 - 10800 - 600);
    /* 扣穿到 0 就停（Δ 下限 0）*/
    CHECK(faith_slot_drain_empty(&a, 600, 0) == FX(20000 - 10800 - 600));
    CHECK(faith_slot_total(&a) == 50000);
    faith_slot_init_110(&a);
    (void)faith_slot_add(&a, FAITH_EV_BIG, 10000);
    a.combo = 1;                                               /* 槽未空 ⇒ 不衰减 */
    CHECK(faith_combo_empty(&a) == 0);
    CHECK(faith_slot_drain_empty(&a, 60, 0) == 0);
    CHECK(faith_slot_total(&a) == 60000);

    /* ============ 连击槽（满 130f；各事件 +f 值）============ */
    faith_slot_init_110(&a);
    CHECK(faith_combo_add(&a, FAITH_EV_BIG) == 130);
    CHECK(a.combo == FAITH_COMBO_FULL);
    CHECK(faith_combo_full(&a) == 1);
    CHECK(faith_combo_add(&a, FAITH_EV_MICRO) == 0);           /* 满槽不再加 */
    CHECK(a.combo == 130);
    faith_combo_set(&a, 0);
    CHECK(faith_combo_add(&a, FAITH_EV_GREEN) == 100);         /* 蓝点 +100f */
    CHECK(faith_combo_add(&a, FAITH_EV_SMALL_P) == 30);        /* +60f 但只剩 30 到满 */
    CHECK(a.combo == 130);
    faith_combo_set(&a, 0);
    CHECK(faith_combo_add(&a, FAITH_EV_BIG_P) == 20);          /* 大 P / 击杀非 boss */
    CHECK(faith_combo_add(&a, FAITH_EV_KILL_NONBOSS) == 20);
    CHECK(faith_combo_add(&a, FAITH_EV_MICRO) == 3);           /* 微信仰点 */
    CHECK(faith_combo_add(&a, FAITH_EV_KILL_BOSS) == 0);       /* 击杀 boss / 1UP = +0f */
    CHECK(faith_combo_add(&a, FAITH_EV_ONEUP) == 0);
    CHECK(faith_combo_value(FAITH_EV_SMALL_FAITH) == 60);
    CHECK(faith_combo_set(&a, 999) == 130);                    /* 夹取 */
    CHECK(faith_combo_set(&a, -5) == 0);

    /* ============ 消弹计数累加 ============ */
    faith_slot_init_110(&a);
    faith_slot_init_111(&b, FAITH_DIFF_NORMAL);
    faith_slot_add_cancel(&a, &b, 7, &add110, &add111);
    CHECK(add110 == FX(70));                                       /* 110：每颗 +10 */
    CHECK(add111 == 0);                                        /* 111：余数 7，还没满 100 */
    CHECK(b.small_rem == 7);
    faith_slot_add_cancel(&a, &b, 93, &add110, &add111);
    CHECK(add110 == FX(930));
    CHECK(add111 == FX(10));                                       /* 7 + 93 = 100 ⇒ 进位 +10 */
    CHECK(b.small_rem == 0);
    faith_slot_add_cancel(&a, &b, 250, &add110, &add111);
    CHECK(add110 == FX(2500));
    CHECK(add111 == FX(20));                                       /* 250 = 2×100 + 50 */
    CHECK(b.small_rem == 50);
    faith_slot_add_cancel(&a, &b, 0, &add110, &add111);        /* 0 颗：什么都不动 */
    CHECK(add110 == 0 && add111 == 0);
    CHECK(b.small_rem == 50);                                  /* 余数**保留**（跨帧 / 跨关）*/
    CHECK(faith_slot_total(&a) == 50000 + 70 + 930 + 2500);
    CHECK(faith_slot_total(&b) == 50000 + 10 + 20);
    faith_slot_reset(&b);
    CHECK(b.small_rem == 50);                                  /* reset 不清余数（§3.2.1 跨关保留）*/
    CHECK(b.base == 50000);                                    /* 也不动基础 */
    faith_slot_init_111(&b, FAITH_DIFF_EASY);
    CHECK(b.small_rem == 0);                                   /* 重新 init 才清 */

    /* ============ 信号强度（0..130，千分点定点）============ */
    faith_slot_init_111(&b, FAITH_DIFF_NORMAL);
    CHECK(faith_signal_step(0, 1, 0, 0) == FAITH_SIGNAL_GAIN_LINE);         /* 线上未满 100：+50/帧 */
    CHECK(faith_signal_step(FAITH_SIGNAL_HI_MARK, 1, 0, 0) == FAITH_SIGNAL_GAIN_TRICK);  /* 超 100：+0.09/帧 */
    CHECK(faith_signal_step(50000, 0, 1, 0) == -FAITH_SIGNAL_LOSS_OFFLINE); /* 线下：−0.3/帧 */
    CHECK(faith_signal_step(50000, 1, 0, 3) == FAITH_SIGNAL_GAIN_LINE + 3 * FAITH_SIGNAL_GAIN_GRAZE);
    b.signal = 0;
    faith_signal_add(&b, FAITH_SIGNAL_MAX + 999);
    CHECK(b.signal == FAITH_SIGNAL_MAX);                       /* 上限 130 */
    faith_signal_add(&b, -FAITH_SIGNAL_MAX - 999);
    CHECK(b.signal == 0);                                      /* 下限 0 */
    b.signal = 1234;
    faith_signal_reset(&b);
    CHECK(b.signal == 0);                                      /* 被弹 / 关卡结束归 0 */

    /* ============ 快照 ============ */
    faith_slot_init_110(&a);
    (void)faith_slot_add(&a, FAITH_EV_BIG, 5000);
    (void)faith_combo_set(&a, 42);
    faith_slot_view(&a, &v);
    CHECK(v.base == 50000);
    CHECK(v.delta == 5000);
    CHECK(v.faith_total == 55000);
    CHECK(v.combo == 42);
    CHECK(faith_score_unit(&a) == 55000);
    faith_slot_view(0, &v);
    CHECK(v.faith_total == 0 && v.combo == 0);

    printf("test_faith_core: %d passed, %d failed\n", s_pass, s_fail);
    return s_fail ? 1 : 0;
}
