/* test_faith_core.c —— `cards/faith_core.c` 的主机单测（`make test-host` 的口径：
 *   cc -std=c11 -Wall -Wextra -Werror -o /tmp/tf tests/test_faith_core.c cards/faith_core.c && /tmp/tf）。
 *
 * 覆盖（用户点名的那几项）：
 *   · 三档加值与上限夹取；
 *   · `Δ_110` 下限 0（= 「50000 扣不穿」）；
 *   · 连击槽增减（含满槽夹取；⚠ 这一套**只服务 110** —— th11 一手没有连击槽，见下）；
 *   · 扣火力的 **1.05 下限**（含门槛 ≥ 2.00 的拒绝）；
 *   · 消弹计数累加（每颗 +10；111 的每 100 个 +10 与余数保留）；
 *   · Miss / 非决死灵击 / 全避 的扣减（**110 侧**的名义值）；
 *   · ★ 信号强度（**2026-09-15 按 th11 一手重写**：线上未满 +50 且补满钳 100.00、
 *     已满按 y 分 +0.16/+0.12/+0.09、线下 −0.30、死亡态不动）+ `faith_slot_sub()`（111 的 miss）。
 *
 * 数值来源见 `cards/faith_core.h` 的头注释（th11/th10 一手 + spec §7.1/§7.2/§7.5 + review §3.2.1）。
 * ⚠ `FAITH_EMPTY_*`（连击槽空扣信仰）**没有一手出处**（th10/th11 都否）⇒ 这里只测「函数还在、
 *   口径未变」，110 与 111 都不调它。
 */
#include <stdio.h>
#include <string.h>

#include "../cards/faith_core.h"

static int s_fail, s_pass;
#define CHECK(cond) do { if (cond) ++s_pass; else { ++s_fail; printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } } while (0)

/* 原始单位 → 定点（工具）*/
#define FX(x) ((int32_t)(x) * FAITH_FIXED)

/* ============ ★★ 生成语义的**源码级不变式**（2026-09-15 收口；`make test-host` 从 `native/` 跑）============
 * 为什么放在这里：`cards/faith.c` 是引擎侧代码（`sdk.h` 里是 inline asm）⇒ **上不了主机**；
 *   而本次收口的几条硬约束（没有出生保护 / 满 P 转换的落点与顺序 / 不串档）**只能从源码形状上看**。
 * 口径：读不到文件就**跳过**（别处的 cwd 不该误报），读到就逐条断言。*/
static char *faith_slurp(const char *path)
{
    static char buf[4][262144];
    static int  slot;
    FILE *f = fopen(path, "rb");
    size_t n;
    char *b;

    if (!f) return 0;
    b = buf[slot = (slot + 1) & 3];
    n = fread(b, 1, sizeof buf[0] - 1, f);
    fclose(f);
    b[n] = 0;
    return b;
}
/* 去掉块注释与行注释：这次收口删掉的东西**在注释里被引用着**（那些「已删」的记录），
 * 所以源码级检查必须在**代码**上做，否则注释会把断言弄假。*/
static void faith_strip_comments(char *s)
{
    char *r = s, *w = s;
    int in_line = 0, in_block = 0, in_str = 0, in_chr = 0;

    while (*r) {
        if (in_line)  { if (*r == '\n') { in_line = 0; *w++ = *r; } ++r; continue; }
        if (in_block) { if (r[0] == '*' && r[1] == '/') { in_block = 0; r += 2; } else ++r; continue; }
        if (in_str)   { if (*r == '\\' && r[1]) { r += 2; continue; }
                        if (*r == '"') in_str = 0;
                        *w++ = *r++; continue; }
        if (in_chr)   { if (*r == '\\' && r[1]) { r += 2; continue; }
                        if (*r == '\'') in_chr = 0;
                        *w++ = *r++; continue; }
        if (r[0] == '/' && r[1] == '*') { in_block = 1; r += 2; continue; }
        if (r[0] == '/' && r[1] == '/') { in_line = 1; r += 2; continue; }
        if (*r == '"')  { in_str = 1; *w++ = *r++; continue; }
        if (*r == '\'') { in_chr = 1; *w++ = *r++; continue; }
        *w++ = *r++;
    }
    *w = 0;
}

static int faith_at(const char *src, const char *needle)
{
    const char *p = strstr(src, needle);
    return p ? (int)(p - src) : -1;
}
/* `needle` 之前的 `back` 字符里不许出现 `age >` / `age <`（= 这条判定不再按出生帧数挡）。
 * ⚠ 只认「比较」的写法：`++p->age;` 这种自增**不算**（它是被判定读的输入，不是门）。*/
static int faith_no_age_before(const char *src, const char *needle, int back)
{
    const char *p = strstr(src, needle);
    const char *q;
    if (!p) return 0;
    q = (p - src > back) ? p - back : src;
    while (q + 5 <= p) {
        if (q[0] == 'a' && q[1] == 'g' && q[2] == 'e' && q[3] == ' ' &&
            (q[4] == '>' || q[4] == '<')) return 0;
        ++q;
    }
    return 1;
}

/* `faith_spawn(` 之后紧跟的 200 字符里**不许**出现手写的 `FAITH_TIER_`（E-1：档位只从来源取）。*/
static int faith_spawn_sites_are_tierfree(const char *src)
{
    const char *p = src;
    while ((p = strstr(p, "faith_spawn(")) != 0) {
        const char *q = p + 12;
        size_t k;
        for (k = 0; k < 200 && q[k]; ++k) {
            if (strncmp(q + k, "FAITH_TIER_", 11) == 0) return 0;
        }
        p = q;
    }
    return 1;
}

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

    /* ============ 逐帧衰减的**速率两档**（2026-09-15：从「❓ 自定」改判）============
     * ★ 这一节原来测的是 `faith_slot_drain_empty()`（❓ 自定、110/111 都不许调），它**已删**：
     *   它的判据是「槽空 ∧ **场上有敌人**」，而一手（th10）的门是「槽空 ∧ 无 Boss ∧ 无对话 ∧ 关卡帧 ≥ 90」，
     *   速率侧则被 `th10:0x418a30`（`c7 40 10 12 00 00 00`）在场抬到下限 18。
     *   现役实现 = `mof_core.h` 的 `mof_core_faith_tick()`（门 + 槽 + 衰减，含「门不成立则槽一起冻结」），
     *   逻辑单测在 `tests/test_mof_core.c`；这里只钉**两个速率常量与它们的定点换算**。
     *   `10800` = ✅ 一手（−180 显示/帧 × 60）；`600` = 🟡 用户口径（一手不认，见 `faith_core.h` 长注）。*/
    CHECK(FAITH_EMPTY_NO_ENEMY_PER_SEC == 10800);
    CHECK(FAITH_EMPTY_ENEMY_PER_SEC == 600);
    CHECK(FAITH_FRAMES_PER_SEC == 60);
    CHECK((FAITH_EMPTY_NO_ENEMY_PER_SEC * FAITH_FIXED) / FAITH_FRAMES_PER_SEC == 18000);  /* −180 显示/帧 */
    CHECK((FAITH_EMPTY_ENEMY_PER_SEC    * FAITH_FIXED) / FAITH_FRAMES_PER_SEC == 1000);   /*  −10 显示/帧 */
    /* `faith_combo_empty()` 仍在（它是「槽空了吗」的只读判断，日志/诊断用）*/
    faith_slot_init_110(&a);
    a.combo = 0;
    CHECK(faith_combo_empty(&a) == 1);
    a.combo = 1;
    CHECK(faith_combo_empty(&a) == 0);

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

    /* ============ 信号强度（0..130.00 显示，千分点定点；★ 2026-09-15 按 th11 一手重写）============
     * 一手 `th11:0x420a70`：线上未满 ⇒ +50.00/f 并**补满钳 100.00**；已满按 y 分档
     * +0.16 / +0.12 / +0.09；线下 −0.30/f；死亡 / 复活态那一档（我们的 BAND_HOLD）不动。
     * 擦弹（`graze_gain`）是**独立通道**，由调用方按难度查表后传进来（一手 §3）。*/
    faith_slot_init_111(&b, FAITH_DIFF_NORMAL);
    CHECK(faith_signal_step(0, FAITH_SIGNAL_BAND_LOW, 0) == FAITH_SIGNAL_GAIN_LINE);
    CHECK(faith_signal_step(50000, FAITH_SIGNAL_BAND_LOW, 0) == FAITH_SIGNAL_GAIN_LINE);
    CHECK(faith_signal_step(60000, FAITH_SIGNAL_BAND_LOW, 0) == 40000);   /* ★ 只补到 100.00（钳）*/
    CHECK(faith_signal_step(FAITH_SIGNAL_HI_MARK, FAITH_SIGNAL_BAND_LOW, 0) == FAITH_SIGNAL_GAIN_TRICK);
    CHECK(faith_signal_step(FAITH_SIGNAL_HI_MARK, FAITH_SIGNAL_BAND_MID, 0) == FAITH_SIGNAL_GAIN_MID);
    CHECK(faith_signal_step(FAITH_SIGNAL_HI_MARK, FAITH_SIGNAL_BAND_TOP, 0) == FAITH_SIGNAL_GAIN_TOP);
    CHECK(faith_signal_step(50000, FAITH_SIGNAL_BAND_OFFLINE, 0) == -FAITH_SIGNAL_LOSS_OFFLINE);
    CHECK(faith_signal_step(50000, FAITH_SIGNAL_BAND_HOLD, 0) == 0);      /* 死亡 / 复活态：不动 */
    CHECK(faith_signal_step(50000, FAITH_SIGNAL_BAND_LOW, 4000) == FAITH_SIGNAL_GAIN_LINE + 4000);
    CHECK(faith_signal_step(50000, FAITH_SIGNAL_BAND_HOLD, 4000) == 4000);/* 擦弹照给 */
    b.signal = 0;
    faith_signal_add(&b, FAITH_SIGNAL_MAX + 999);
    CHECK(b.signal == FAITH_SIGNAL_MAX);                       /* 上限 130.00 显示 */
    faith_signal_add(&b, -FAITH_SIGNAL_MAX - 999);
    CHECK(b.signal == 0);                                      /* 下限 0 */
    b.signal = 1234;
    faith_signal_reset(&b);
    CHECK(b.signal == 0);                                      /* 被弹 / 关卡结束 / 一局开始归 0 */

    /* ============ 扣一笔指定量（`faith_slot_sub`；111 的 miss 用它）============ */
    faith_slot_init_111(&b, FAITH_DIFF_NORMAL);                /* base 50000 ⇒ Δ 上限 50000 */
    (void)faith_slot_add(&b, FAITH_EV_BIG, 5000);
    CHECK(faith_slot_sub(&b, 3000) == 3000);                   /* 原始单位进、原始单位出 */
    CHECK(b.delta == 2000 * FAITH_FIXED);
    CHECK(faith_slot_sub(&b, 99999) == 2000);                  /* 夹在 Δ 现有量上（下限 0）*/
    CHECK(b.delta == 0);
    CHECK(faith_slot_sub(&b, 0) == 0);
    CHECK(faith_slot_sub(&b, -5) == 0);
    CHECK(faith_slot_sub(0, 100) == 0);                        /* NULL 安全 */

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

    /* ============ ★★ 档位 ↔ 来源（E-1 硬约束：**不许串档**）============
     * 一手（两作同一套；地址在 `faith_core.h` 的表里）：
     *   击破掉落 → 大（type 3）、满 P 转换 → 中/小（type 9）、消弹 → 微（type 8）。
     * 这是共用池「一套实体 + 各自数值表」能成立的前提 ⇒ 卡侧只许从这里取 tier。*/
    CHECK(faith_core_tier_for_source(FAITH_SRC_BREAK)  == FAITH_TIER_BIG);
    CHECK(faith_core_tier_for_source(FAITH_SRC_FULL_P) == FAITH_TIER_SMALL);
    CHECK(faith_core_tier_for_source(FAITH_SRC_CANCEL) == FAITH_TIER_MICRO);
    /* ★ 硬约束本身：满 P 转换**只许** type 9 档、消弹**只许** type 8 档（串档 = 回归）*/
    CHECK(faith_core_tier_for_source(FAITH_SRC_FULL_P) != FAITH_TIER_MICRO);
    CHECK(faith_core_tier_for_source(FAITH_SRC_CANCEL) != FAITH_TIER_SMALL);
    CHECK(faith_core_tier_for_source(FAITH_SRC_CANCEL) != FAITH_TIER_BIG);
    CHECK(faith_core_tier_for_source(FAITH_SRC_BREAK)  != FAITH_TIER_SMALL);
    /* 越界来源 = 防御性给「中/小」档（枚举封闭，正常走不到）*/
    CHECK(faith_core_tier_for_source((faith_source_t)99) == FAITH_TIER_SMALL);
    /* type 号（两作同一套表）与档号 */
    CHECK(FAITH_TYPE_BIG == 3 && FAITH_TYPE_SMALL == 9 && FAITH_TYPE_MICRO == 8);
    CHECK(FAITH_TIER_BIG == 0 && FAITH_TIER_SMALL == 1 && FAITH_TIER_MICRO == 2);  /* 旧枚举值不许动 */

    /* ============ ★★ 生成运动学（E-2：固定角 −π/2 + 定速，**不是随机散**）============
     * 一手实参：击破 / 满 P 转换族 = `0xbfc90fdb` / `0x400ccccd`（`th10:0x41bac1` 等；
     * th11 侧 `0x49842c` / `0x498430` 同值）；消弹族 = `0x3f19999a` = 0.6（`th10:0x4081d9`）。*/
    CHECK(FAITH_THROW_ANGLE == -1.5707964f);                   /* −π/2 = 正上方 */
    CHECK(faith_core_throw_speed(FAITH_TIER_BIG)   == 2.2f);
    CHECK(faith_core_throw_speed(FAITH_TIER_SMALL) == 2.2f);   /* 满 P 转换与击破掉落同速（一手同参）*/
    CHECK(faith_core_throw_speed(FAITH_TIER_MICRO) == 0.6f);   /* 消弹族慢一些（一手）*/
    CHECK(FAITH_THROW_SPEED_MAIN == 2.2f && FAITH_THROW_SPEED_CANCEL == 0.6f);

    /* ============ ★★ 生成语义的源码级不变式（2026-09-15 收口；见 `faith.c` / `faith.h` / `TUNING.md` §2.5c）============
     * A/B = 满 P 转换的**落点与顺序**、C = **没有出生保护**（生成即被收）、E-1 = **不串档**、
     * E-2 = 固定角/定速（随机出生已删）。这些在主机上只能从源码形状上钉。*/
    {
        char *fc = faith_slurp("cards/faith.c");
        char *fh = faith_slurp("cards/faith.h");
        if (fc && fh) {
            /* 注释里保留着「原来是什么、为什么删」的记录 ⇒ 断言一律打在去注释后的**代码**上。*/
            faith_strip_comments(fc);
            faith_strip_comments(fh);
            /* C：出生保护（32 帧不吸/不收/不减速）与「出生错帧延时」都**整条删掉**，
             *    判定里不许再出现按 age 挡收点的写法（旧 `p->age >= FAITH_INTANGIBLE`）。*/
            CHECK(strstr(fc, "FAITH_INTANGIBLE") == 0);
            CHECK(strstr(fh, "FAITH_INTANGIBLE") == 0);
            /* 「生成即被收」= 收点/吸入/真空飞行三条判定都**不再**按出生帧数挡；
             * 物理上的减速也不再豁免（`sl` 恒等于 slow）。抖动那一条**故意保留**引擎的 `age > 31`。*/
            CHECK(strstr(fc, "FAITH_INTANGIBLE") == 0);
            CHECK(faith_no_age_before(fc, "d2 <= collect_r2", 200));
            CHECK(faith_no_age_before(fc, "faith_player_above_line() || CE_BOMB_COLLECTING()", 200));
            CHECK(strstr(fc, "const float sl = slow;") != 0);
            CHECK(strstr(fc, "FAITH_SLOW_JITTER_AGE") != 0);   /* 引擎自己的 `age > 31` 门（不是出生保护）*/
            /* E-2：随机出生角/速已删，改成一手实参（角 −π/2 + 逐档定速）。*/
            CHECK(strstr(fc, "FAITH_THROW_MIN") == 0);
            CHECK(strstr(fc, "FAITH_THROW_SPAN") == 0);
            CHECK(strstr(fc, "FAITH_THROW_ANGLE") != 0);
            CHECK(strstr(fc, "faith_core_throw_speed") != 0);
            /* A/B：转换在共用件里，按 **TH18 类型号** 筛道具、按 **来源** 取档（E-1）。*/
            CHECK(strstr(fc, "faith_convert_full_p_items") != 0);
            CHECK(strstr(fc, "faith_core_is_p_item_type") != 0);
            CHECK(strstr(fc, "faith_core_tier_for_source(FAITH_SRC_FULL_P)") != 0);
            /* 删原生道具的四步都在（state / anm VM / anm id / freelist 前插）*/
            CHECK(strstr(fc, "faith_item_delete") != 0);
            CHECK(strstr(fc, "CE_ITEM_STATE") != 0 && strstr(fc, "FAITH_ITEM_ANM_ID") != 0);
            /* B12（2026-09-18）：满 P 转换三条都在 ——
             *   ① **特效**：逐颗起一台 `CE_ANM_ABILITY_SCRIPT_MOF_CONV_BURST`
             *      （一手 TH10 `0x448db0(…, 0x189)`；0x189 = bullet.anm script393，**不是** SE id）；
             *   ② **音**：`FAITH_CONV_SE`（❓自定 = TH18 `0x28` = `se_kira02`，与 TH10 那颗逐字节同源）；
             *   ③ **上抛段**：`state == 2` 且 `age < FAITH_TOSS_FRAMES` 时豁免真空门
             *      （一手 TH10 顶点翻 3 `0x41b185`；帧数 ❓自定）。*/
            CHECK(strstr(fc, "CE_ANM_ABILITY_SCRIPT_MOF_CONV_BURST") != 0);
            CHECK(faith_at(fc, "conv_burst_spawn(px, py)") > faith_at(fc, "faith_convert_full_p_items"));
            /* ② 音：**事件级**（用户口径「只有满 P 转换的那一下有」）—— 判据 `conv_event`（刚满 P）
             *    + 只在 `n > 0` 时响一次；**不许**回到「逐颗响」的写法。*/
            CHECK(strstr(fc, "FAITH_CONV_SE") != 0);
            CHECK(strstr(fc, "conv_event") != 0);
            CHECK(strstr(fc, "&& conv_event") != 0);
            CHECK(strstr(fc, "ce_play_sound(FAITH_CONV_SE, px)") == 0);   /* 逐颗那版已撤 */
            CHECK(faith_at(fc, "ce_play_sound(FAITH_CONV_SE") > faith_at(fc, "faith_convert_full_p_items"));
            CHECK(strstr(fc, "FAITH_TOSS_FRAMES") != 0);
            CHECK(strstr(fc, "const int in_toss =") != 0);
            CHECK(strstr(fc, "&& !in_toss") != 0);        /* 豁免真的接在真空门那一句上 */
            /* B11（2026-09-18 用户裁决）：真空门**不许**含「自机 state 2/4 ⇒ 吸」那一条
             *   （那是 TH18 的死亡回收；实跑症状 = 一次死亡把 54 颗绿点一次吸进 114 的批）。
             *   注意：`faith_player_above_line()` 里那条 `pstate == 2 || pstate == 4` 是**反向**用的
             *   （死亡期不看线 ⇒ 返回 0），**必须留着**。*/
            CHECK(strstr(fc, "if (ps == 2 || ps == 4) return 1;") == 0);
            CHECK(strstr(fc, "if (pstate == 2 || pstate == 4) return 0;") != 0);
        } else {
            printf("  （跳过源码级检查：读不到 cards/faith.c —— 请从 native/ 目录跑本测试）\n");
        }
        {
            char *mo = faith_slurp("cards/mof.c");
            char *sg = faith_slurp("cards/signal.c");
            if (mo && sg) {
                int conv, pool;
                faith_strip_comments(mo);
                faith_strip_comments(sg);
                /* ★ A/B 的**顺序**约束：转换必须在池推进之前（否则本帧转换出来的点要等下一帧才受物理/被收）。*/
                conv = faith_at(mo, "faith_convert_full_p_items()");
                pool = faith_at(mo, "faith_pool_tick()");
                CHECK(conv >= 0 && pool >= 0 && conv < pool);
                conv = faith_at(sg, "faith_convert_full_p_items()");
                pool = faith_at(sg, "faith_pool_tick()");
                CHECK(conv >= 0 && pool >= 0 && conv < pool);
                /* 转换通道的值两个槽各按自己的数值表登记（0 = 不参与）*/
                CHECK(strstr(mo, "u.conv_value = FAITH_MOF_SMALL") != 0);
                CHECK(strstr(sg, "u.conv_value = SIGNAL_GREEN_MID") != 0);
                /* E-1：卡侧**所有**撒点都走 `faith_core_tier_for_source()`，不许在 spawn 调用点手写档位。*/
                CHECK(faith_spawn_sites_are_tierfree(mo));
                CHECK(faith_spawn_sites_are_tierfree(sg));
            } else {
                printf("  （跳过源码级检查：读不到 cards/mof.c / cards/signal.c —— 请从 native/ 目录跑本测试）\n");
            }
        }
    }

    /* ============ ★★ 2026-09-15h：P 点补值的**共享策略**（纯逻辑那一半）============
     * 为什么在这张测试里：这一层是 110 与 111 **共用**的落笔口径（`faith_p_topup_take()` 的判据与
     * 相位），而「一颗小 P 补多少」原来是两张卡各算一次 ⇒ 111 的「爱丽丝减半」被 110 的 +0.05 绕过。
     * 一手 / 口径：`pl01a.sht` 的 `+0x24`=12 / `+0x20`=8（爱丽丝一发占满槽 **1/8**，其余五机 20/4 ⇒ **1/4**
     * ⇒ **恰好一半** ⇒ 比例 ✅ 一手）；TH18 侧落成「1 档的一半 = 50 单位」是 ❓ 映射；
     * P 点减半到 **平均 2.5 单位 = 0.025 档** 是 🟡 用户口径（0.025 档无法用整数精确表示 ⇒ 2/3 交替）。
     * 这里钉四条不变式：
     *   ① 只在不生效时不推进相位、且恒返回常态 5（「只在爱丽丝生效时改变」）；
     *   ② 序列 2,3,2,3…、**长期平均恰好 2.5 单位**；
     *   ③ 相位**跨调用连续**（状态是唯一载体：两个独立实例互不干扰 ⇒ 没有隐藏全局量）；
     *   ④ 情态判据 `faith_core_p_alice_on(has_111, is_marisa)` **两条都要**（缺一不生效）。*/
    {
        faith_p_alt_t al, al2, al3;
        int32_t i, sum;
        int bad;

        /* --- ④ 判据：111 在场 ∧ 机体 == 魔理沙(1) --- */
        CHECK(faith_core_p_alice_on(1, 1) == 1);
        CHECK(faith_core_p_alice_on(1, 0) == 0);        /* 只 111、非魔理沙 ⇒ 不生效 */
        CHECK(faith_core_p_alice_on(0, 1) == 0);        /* 只魔理沙、111 不在场 ⇒ 不生效 */
        CHECK(faith_core_p_alice_on(0, 0) == 0);
        CHECK(FAITH_CHAR_MARISA == 1);
        CHECK(FAITH_P_TOPUP_NORMAL == 5);               /* = +0.05 档（`MOF_P_POWER_UNITS` 现在指向它）*/
        CHECK(FAITH_P_TOPUP_ALICE_LO == 2 && FAITH_P_TOPUP_ALICE_HI == 3);
        CHECK(FAITH_P_TOPUP_ALICE_LO + FAITH_P_TOPUP_ALICE_HI == 5);   /* 平均 = 2.5 单位 = 0.025 档 */

        /* --- ① 不生效：恒常态、**一个字节都不推进** --- */
        memset(&al, 0, sizeof al);
        CHECK(faith_core_p_topup_total(&al, 0, 1) == FAITH_P_TOPUP_NORMAL);
        CHECK(faith_core_p_topup_total(&al, 0, 1) == FAITH_P_TOPUP_NORMAL);
        CHECK(al.alt == 0 && al.items == 0 && al.lo == 0 && al.hi == 0);
        CHECK(faith_core_p_topup_total(0, 1, 1) == FAITH_P_TOPUP_NORMAL);  /* 没有状态 ⇒ 同样常态 */
        CHECK(faith_core_p_topup_total(0, 0, 1) == FAITH_P_TOPUP_NORMAL);

        /* --- ② 生效：序列 2,3,2,3… 且**平均恰好 2.5** --- */
        memset(&al, 0, sizeof al);
        CHECK(faith_core_p_topup_total(&al, 1, 1) == FAITH_P_TOPUP_ALICE_LO);
        CHECK(faith_core_p_topup_total(&al, 1, 1) == FAITH_P_TOPUP_ALICE_HI);
        CHECK(faith_core_p_topup_total(&al, 1, 1) == FAITH_P_TOPUP_ALICE_LO);
        CHECK(al.items == 3 && al.lo == 2 && al.hi == 1);
        sum = 0;
        for (i = 0; i < 10; ++i) sum += faith_core_p_topup_total(&al, 1, 1);   /* 同一个状态 ⇒ 相位连续 */
        CHECK(sum == 25);                                  /* 10 颗共 25 单位 */
        CHECK(sum * 4 == 10 * 10);                         /* ★ 平均 2.5 单位 = **0.025 档**（整数式）*/
        CHECK(al.items == 13 && al.lo == 7 && al.hi == 6);
        CHECK(al.lo - al.hi <= 1 && al.hi - al.lo <= 1);   /* 两侧之差恒 ≤ 1 */

        /* --- ③ 相位连续：两个独立实例互不干扰（⇒ 只能从状态来，没有静态量在背后偷偷记）--- */
        memset(&al2, 0, sizeof al2);
        memset(&al3, 0, sizeof al3);
        CHECK(faith_core_p_topup_total(&al2, 1, 1) == FAITH_P_TOPUP_ALICE_LO);
        CHECK(faith_core_p_topup_total(&al3, 1, 1) == FAITH_P_TOPUP_ALICE_LO);   /* al2 走了一格，al3 不受影响 */
        CHECK(faith_core_p_topup_total(&al2, 1, 1) == FAITH_P_TOPUP_ALICE_HI);
        CHECK(al3.alt == 1 && al3.items == 1);

        /* --- 「关掉再开」不丢相位：`alice_on = 0` 的那几次调用不动 `alt` --- */
        bad = 0;
        for (i = 0; i < 3; ++i) if (faith_core_p_topup_total(&al3, 0, 1) != FAITH_P_TOPUP_NORMAL) bad = 1;
        CHECK(bad == 0 && al3.alt == 1 && al3.items == 1);
        CHECK(faith_core_p_topup_total(&al3, 1, 1) == FAITH_P_TOPUP_ALICE_HI);   /* 接着上一相位往下走 */

        /* --- ★★ 满火力（`usable = 0`）：返回常态 **且不吃相位格** ---
         * 为什么重要：满火力那一颗 P 点**拿不到任何增量**（原生也不给）⇒ 若让它白吃一格相位，
         * 后面**真正拿到火力**的那些颗就会偏向另一侧（例：满格那颗占掉 LO ⇒ 下一颗恒 HI ⇒ 平均被拉到 3）。*/
        memset(&al, 0, sizeof al);
        CHECK(faith_core_p_topup_total(&al, 1, 0) == FAITH_P_TOPUP_NORMAL);
        CHECK(faith_core_p_topup_total(&al, 1, 0) == FAITH_P_TOPUP_NORMAL);
        CHECK(al.alt == 0 && al.items == 0 && al.lo == 0 && al.hi == 0);
        /* 之后第一颗**真**拿到火力的还是 LO（序列从没被打乱）*/
        CHECK(faith_core_p_topup_total(&al, 1, 1) == FAITH_P_TOPUP_ALICE_LO);
        CHECK(faith_core_p_topup_total(&al, 1, 1) == FAITH_P_TOPUP_ALICE_HI);
        /* 常态侧同样：`usable = 0` 只是个「不算数」的标记，返回值恒为常态 */
        CHECK(faith_core_p_topup_total(&al, 0, 0) == FAITH_P_TOPUP_NORMAL);
        CHECK(al.items == 2);
    }

    /* ============ ★★ 2026-09-18（B1）：114 收点批的**成员 / 在飞谓词** =「真空飞行」============
     * 用户规格（原话要点）：「**信仰点本身得做到：① 开批 ② 计数 +1 ③ 飞行中的信仰点会组织批次结算
     *   —— 也就是与 P 点和金钱完全一致**」。
     * 引擎对 P / 钱只有 `item+0xc74 == 3`（真空飞行）才算批成员 / 才算在飞（§Z22 成员门 +
     *   `ce_item_pool_any_attracting`）⇒ 我们的点必须**同一谓词**（`faith_core_state_is_vacuum_flight`）。
     * 这一节钉两件事：① 谓词本身「只认 3」；② `cards/faith.c` 的**两条通道都过它**（源码级不变式，
     *   因为 `faith.c` 是引擎侧代码、上不了主机 —— 见上面那段说明）。*/
    {
        int st;
        CHECK(FAITH_STATE_IDLE == 0 && FAITH_STATE_FALL == 1 && FAITH_STATE_SCATTER == 2);
        CHECK(FAITH_STATE_VACUUM == 3 && FAITH_STATE_RING == 4);
        CHECK(FAITH_STATE_VACUUM == 3);                 /* ★ = 引擎 `CE_ITEM_STATE_VACUUM`（同一语义）*/
        for (st = 0; st <= 8; ++st)
            CHECK(faith_core_state_is_vacuum_flight(st) == (st == FAITH_STATE_VACUUM ? 1 : 0));
    }
    {
        char *fc = faith_slurp("cards/faith.c");
        if (fc) {
            int member, fly;
            faith_strip_comments(fc);
            /* ① 旧开关（不看状态的那条路）**必须已删** —— 它一旦回来，普通捡点又会开批。*/
            CHECK(strstr(fc, "FAITH_VITEM_NOTE_VACUUM_ONLY") == 0);
            /* ② 两条通道都过同一个谓词（成员 = dot_collect 里的上报；在飞 = 声明时的计数）。*/
            member = faith_at(fc, "ce_vitem_note_collect");
            fly    = faith_at(fc, "ce_vitem_declare_flying");
            CHECK(member >= 0 && fly >= 0);
            CHECK(faith_at(fc, "faith_core_state_is_vacuum_flight") >= 0);
            CHECK(member > faith_at(fc, "faith_core_state_is_vacuum_flight"));  /* 谓词在成员上报**之前** */
            /* ③ presence 不许再报「活着的全算」（旧实现的 `s_n_big + s_n_small` 形状）；
             *    计数必须**在谓词之下**（`if (…is_vacuum_flight(p->state)) ++vacuum_in_flight;`）。*/
            CHECK(strstr(fc, "s_n_big + s_n_small") == 0);
            CHECK(strstr(fc, "faith_core_state_is_vacuum_flight(p->state)) ++vacuum_in_flight;") != 0);
            fly = faith_at(fc, "ce_vitem_declare_flying");
            CHECK(fly >= 0 && faith_at(fc, "vacuum_in_flight") > 0 && fly > faith_at(fc, "vacuum_in_flight"));
        } else {
            printf("  （跳过 B1 源码级检查：读不到 cards/faith.c —— 请从 native/ 目录跑本测试）\n");
        }
    }


    /* ★★ 2026-09-18c：ZUN 自己「一次爆多颗道具」的**落点分布**（一手 TH18 `0x430510` 的附加掉落循环）：
     *   `ang = rand± × π`（整圈随机角）、`k = rand01 × 0.5`（半径比例）、
     *   `off = (cos(ang)×rx×k, sin(ang)×ry×k)`（`rx/ry` = 掉落区半径，默认 32×32）。
     * 这里钉住**缩放**那一半（三角由调用方用引擎 FSINCOS 算 ⇒ 本仓 0 条 x87 的硬线）。*/
    {
        float ox = 9.0f, oy = 9.0f;
        faith_core_scatter_off(32.0f, 32.0f, 0.0f, 1.0f, 0.0f, &ox, &oy);   /* rand01=0 ⇒ 原点 */
        CHECK(ox == 0.0f && oy == 0.0f);
        faith_core_scatter_off(32.0f, 32.0f, 1.0f, 1.0f, 0.0f, &ox, &oy);   /* 最远 = 半径的一半 */
        CHECK(ox == 16.0f && oy == 0.0f);
        faith_core_scatter_off(32.0f, 32.0f, 1.0f, 0.0f, 1.0f, &ox, &oy);
        CHECK(ox == 0.0f && oy == 16.0f);
        faith_core_scatter_off(64.0f, 32.0f, 1.0f, 1.0f, 0.0f, &ox, &oy);   /* rx≠ry ⇒ 各按自己的半径 */
        CHECK(ox == 32.0f && oy == 0.0f);
        faith_core_scatter_off(32.0f, 32.0f, 0.5f, -1.0f, 0.0f, &ox, &oy);  /* 负方向也对称 */
        CHECK(ox == -8.0f && oy == 0.0f);
        faith_core_scatter_off(32.0f, 32.0f, 1.0f, 1.0f, 0.0f, 0, 0);       /* NULL 安全 */
        faith_core_scatter_off(32.0f, 32.0f, 1.0f, 1.0f, 1.0f, &ox, 0);
        CHECK(ox == 16.0f);
    }
    printf("test_faith_core: %d passed, %d failed\n", s_pass, s_fail);
    return s_fail ? 1 : 0;
}