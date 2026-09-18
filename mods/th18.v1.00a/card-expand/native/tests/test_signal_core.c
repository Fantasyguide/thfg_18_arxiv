/* test_signal_core.c —— `cards/signal_core.c` 的主机单测（`make test-host` 的口径：
 *   cc -std=c11 -Wall -Wextra -Werror -o /tmp/t111 tests/test_signal_core.c \
 *      cards/signal_core.c cards/faith_core.c cards/mof_core.c && /tmp/t111）。
 *
 * ★ 2026-09-15：**全篇按 th11 v1.00a 一手重写断言**（旧值凡与一手冲突处一律换掉，逐条在行内注明）。
 *   新增/改写的钉点：
 *     · 信号槽**每帧四档**（线上未满 +50 且**补满钳 100.00** / 已满 +0.16·+0.12·+0.09 / 线下 −0.30）
 *       ——旧实现的「≥100 一律 +0.09、未满无条件 +50」是错的；
 *     · 擦弹**按难度查表**（子弹 {8,5,5,5,5} 显示 / 激光 {5,4,3,2,2}）——旧实现「一律 +8」只对 Easy 成立；
 *     · 蓝点分 = 一手整数式 `trunc10((A+B)×Y/100)`，`S ≤ 0 ⇒ 10`；
 *     · 擦弹倍率 `min(⌊graze/100⌋,899)×10`（**89900 才封顶**，不是 8990）；
 *     · **111 侧无连击槽**（th11 一手没有 ⇒ `signal_core_pickup()` 不再动 combo）；
 *     · miss 扣信仰（显示 −100,000，夹 0）；得分跨阈值奖残；命碎道具的类型映射；
 *     · HUD 条宽 = `9 + 4×level`（一手），**5 格那套已删**；
 *     · ★ P 点火力 +0.05 档的 **111 侧那一半**（2026-09-15）：`mof_core_p_owner(ce_owned(110), 1)`
 *       ⇒ 110 在场就**让位**（两张同装只算一次）；算式与穷举在 `test_mof_core.c`。
 *     · ★★ **爱丽丝（§18）**：**111 私有**的那一条 = 灵击 **别人的一半**（门槛「能扣就扣」= ≥1.55 档、
 *       下限 1.05 照旧、每次恒扣 50 单位）。**比例 ✅ 一手**（`pl01a.sht` 的 `+0x24`=12 / `+0x20`=8
 *       ⇒ 一发占满槽 1/8 vs 其余五机 20/4 ⇒ 1/4）、**绝对档位 ❓ 我们的映射**。依据见 `signal_core.h` ④。
 *     · ⚠ **爱丽丝的另一条（P 点平均 +0.025 档）不在本文件的覆盖里**：它 2026-09-15h 搬进了共享件
 *       （落笔者恒为 110 ⇒ 相位必须两张卡都够得到）⇒ 单测在 `test_faith_core.c`（纯逻辑）与
 *       `test_mof_core.c`（接线模型：**同一颗道具只推进一次相位**）。
 *
 * 一手出处：
 *   `local/_scratch/research-th11-signal.md`（§1 载体量纲 / §2 每帧四档 / §3 擦弹表 / §4 归零 / §6 HUD）
 *   `local/_scratch/research-th11-faith-and-points.md`（§2.1 擦弹倍率 / §2.2-2.3 蓝点分 / §4 绿点 / §6 写入点）
 *   `local/_scratch/research-th11-bomb-lifefrag-hud.md`（§4 命碎 —— 其地址名由本卡的复核订正）
 *
 * ⚠ 链接**三个**共用件：`signal_core.c`（本卡纯逻辑）+ `faith_core.c`（绿点记账 / 信号表）+
 *   `mof_core.c`（擦弹倍率 / 灵击扣法 —— 111 **只借不重写**，见 signal.c 的分工表）。
 */
#include <stdio.h>
#include <string.h>

#include "../cards/signal_core.h"
#include "../cards/mof_core.h"      /* 共用件：擦弹倍率 / 灵击扣法（111 侧只借不重写）*/

static int s_fail, s_pass;
#define CHECK(cond) do { if (cond) ++s_pass; else { ++s_fail; printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } } while (0)

#define G       FAITH_SIGNAL_GAIN_LINE      /* +50.00/f（线上、未满 100.00）*/
#define GTOP    FAITH_SIGNAL_GAIN_TOP       /* +0.16/f（已满、离线上 > 64 px）*/
#define GMID    FAITH_SIGNAL_GAIN_MID       /* +0.12/f（已满、离线上 48…64 px）*/
#define GTRICK  FAITH_SIGNAL_GAIN_TRICK     /* +0.09/f（已满、离线上 0…48 px）*/
#define L       FAITH_SIGNAL_LOSS_OFFLINE   /* −0.30/f（线下）*/
#define ZE      FAITH_SIGNAL_GRAZE_BULLET_EASY      /* 擦弹（子弹，Easy）= +8.00 */
#define ZN      FAITH_SIGNAL_GRAZE_BULLET_NORMAL    /* 擦弹（子弹，其余）= +5.00 */

/* ============ ★★ 「灵击 → 决死」：`cards/signal.c` 的 `signal_bomb()` 里那几步的**可执行模型** ====
 * 与信仰/信号账有关的三步（其余步骤与账无关，由 `cards/signal.c` 自己保证）：
 *   ① `signal_bomb_form_start(&s->bomb_form, by_hit)`（记形态）；
 *   ② 消 `bullets` 颗：入不入账问 `signal_bomb_form_cancel_accounts()`，入账那一支走
 *      `signal_on_cancel()` 里那一句 `faith_slot_add_cancel(0, &s->slot, n, 0, 0)`（111 每 100 个 +10）；
 *   ③ 决死形态**没有任何信仰扣除**（111 的信仰惩罚只走 miss 那条路，见文件头与 `signal_core.h`）。
 * 返回该发之后的信仰总值（显示口径）。*/
static int32_t sim_signal_bomb(faith_slot_t *slot, signal_bomb_form_t *f, int32_t by_hit, int32_t bullets)
{
    signal_bomb_form_start(f, by_hit);
    if (bullets > 0) {
        signal_bomb_form_cancel_note(f, bullets);
        if (signal_bomb_form_cancel_accounts(f)) (void)faith_slot_add_cancel(0, slot, bullets, 0, 0);
    }
    return faith_slot_total(slot);
}

int main(void)
{
    faith_slot_t slot;
    int32_t v;

    /* ==================== 0. 常数自洽（防共用件被改而这边不知道）==================== */
    CHECK(SIGNAL_MAX == 130000);            /* 130.00 显示（一手：槽存 ×100、上限 13000）*/
    CHECK(SIGNAL_HI_MARK == 100000);        /* 满 = 100.00（一手 `th11:0x42364a` 的 0x2710）*/
    CHECK(SIGNAL_FULL_AT == 100000);
    CHECK(G == 50000);                      /* +50.00/f */
    CHECK(GTOP == 160);                     /* +0.16/f（一手 `add eax,0x10`）*/
    CHECK(GMID == 120);                     /* +0.12/f（一手 `add eax,0xc`）*/
    CHECK(GTRICK == 90);                    /* +0.09/f（一手 `add eax,0x9`）*/
    /* ★★ 2026-09-15f 订正：这里原来写的是 `L == 3000` —— 它把**错的**值也一起钉住了
     *   （注释却写着「−0.30/f」）。本文件的槽单位是千分点（上面 `SIGNAL_MAX == 130000` = 显示 130.00、
     *   `GTOP == 160` = +0.16/f 都是这个刻度）⇒ −0.30/f = **300**。一手见 `faith_core.h` 那一大段
     *   （`th11:0x420a85 lea esi,[edx-0x1e]` = −30 **存储** = −0.30 显示；存储 = 显示 ×100）。*/
    CHECK(L == 300);                        /* −0.30/f（一手 −30 存储；千分点 ⇒ 300） */
    CHECK(ZE == 8000 && ZN == 5000);        /* 擦弹表（一手 `th11:0x4a7950` = {800,500,…} 存储）*/
    CHECK(FAITH_SIGNAL_BAND_TOP_PX == 64);  /* 一手 64.0f（线 128 − 64）*/
    CHECK(FAITH_SIGNAL_BAND_MID_PX == 48);  /* 一手 80.0f（线 128 − 48）*/
    CHECK(SIGNAL_UNIT == 1000);
    CHECK(SIGNAL_GREEN_BIG == 100);         /* 大绿点 +100 */
    CHECK(SIGNAL_GREEN_MID == 10);          /* 中绿点 +10 */
    CHECK(SIGNAL_GREEN_SMALL_N == 100);     /* 小绿点每 100 个 */
    CHECK(SIGNAL_GREEN_SMALL_V == 10);      /*   … +10 */
    CHECK(FAITH_SA_MISS_PIV_LOSS == 100000);/* 一手 `sub eax,0x989680` = 显示 −100,000 */
    CHECK(FAITH_POWER_MIN_LEVEL == 2);      /* 灵击门槛 TH18 ≥ 2.00 档（§3.2.1，用户拍板）*/
    CHECK(FAITH_POWER_COST == 100);         /* 每次扣 1 档 */
    CHECK(FAITH_POWER_FLOOR == 105);        /* 下限 1.05 档（用户「サービス」）*/
    CHECK(FAITH_GRAZE_MULT_MAX == 8990);    /* 擦弹倍率上限 8.99（千分点）*/
    CHECK(FAITH_GRAZE_STEP == 100 && FAITH_GRAZE_STEP_MAX == 899);
    CHECK(SIGNAL_ITEM_TYPE_LIFEFRAG == 0x10);   /* ★ 2026-09-15e 订正：TH18 命碎道具 = **type 0x10**
                                                 *   （EXTEND2 卡道具 → `CardLifeFragment` id 3）。
                                                 *   旧值 4 是错的 —— 一手三条见 `signal_core.h` 的载体段。*/
    CHECK(SIGNAL_SEG_DROP_EVERY == 2);          /* ★ 用户口径（不是复刻）：每 2 段 ⇒ 1 颗 */
    CHECK(SIGNAL_FRAG_ITEM_TYPE == SIGNAL_ITEM_TYPE_LIFEFRAG);
    CHECK(SIGNAL_LIFEFRAG_PER_LIFE_TH11 == 5);
    CHECK(SIGNAL_BAR_BASE_PX == 9 && SIGNAL_BAR_STEP_PX == 4);   /* 一手 `9.0 + 4.0×level` */

    /* ==================== 1. 每帧档位：线上未满 ⇒ +50 并**补满钳 100.00** ====================
     * 一手 `th11:0x420a99: add eax,0x1388` + `0x420aa8: mov [edi+0x14],0x2710`
     * ⇒ 「未满时一帧直接补满」（不是「每帧 +50 直到满」；数值上等价，**钳位**不等价）。*/
    CHECK(signal_core_step(0, FAITH_SIGNAL_BAND_LOW, 0, FAITH_DIFF_NORMAL) == G);
    CHECK(signal_core_step(49999, FAITH_SIGNAL_BAND_LOW, 0, FAITH_DIFF_NORMAL) == G);
    CHECK(signal_core_step(50000, FAITH_SIGNAL_BAND_LOW, 0, FAITH_DIFF_NORMAL) == G);
    CHECK(signal_core_step(60000, FAITH_SIGNAL_BAND_LOW, 0, FAITH_DIFF_NORMAL) == 40000);   /* ★ 只补到 100.00 */
    CHECK(signal_core_step(99999, FAITH_SIGNAL_BAND_LOW, 0, FAITH_DIFF_NORMAL) == 1);       /* ★ 只差 0.001 */
    CHECK(signal_core_step(100000, FAITH_SIGNAL_BAND_LOW, 0, FAITH_DIFF_NORMAL) == GTRICK); /* 已满 ⇒ 走档位表 */
    /* 三档（已满时按 y 分档；<64 / 64..80 / 80..128）*/
    CHECK(signal_core_step(100000, FAITH_SIGNAL_BAND_TOP, 0, FAITH_DIFF_NORMAL) == GTOP);
    CHECK(signal_core_step(100000, FAITH_SIGNAL_BAND_MID, 0, FAITH_DIFF_NORMAL) == GMID);
    CHECK(signal_core_step(129999, FAITH_SIGNAL_BAND_TOP, 0, FAITH_DIFF_NORMAL) == GTOP);
    /* 两帧从 0 到满（+50 +50）——原版节奏（补满那条路的等价观测）*/
    v = 0; v += signal_core_step(v, FAITH_SIGNAL_BAND_LOW, 0, FAITH_DIFF_NORMAL); CHECK(v == 50000);
    v += signal_core_step(v, FAITH_SIGNAL_BAND_LOW, 0, FAITH_DIFF_NORMAL); CHECK(v == 100000);
    CHECK(signal_core_is_full(v) == 1);
    /* 顶到 130 也不越界（`apply` 会夹）*/
    slot.signal = 130000;
    (void)signal_core_apply(&slot, signal_core_step(slot.signal, FAITH_SIGNAL_BAND_TOP, 0, FAITH_DIFF_NORMAL));
    CHECK(slot.signal == SIGNAL_MAX);

    /* ==================== 2. 擦弹：按难度查表（一手 §3）====================
     * 子弹 / 敌体擦弹表 `th11:0x4a7950` = {800,500,500,500,500} 存储 = {8,5,5,5,5} 显示。*/
    CHECK(signal_core_graze_gain(FAITH_DIFF_EASY, 0) == ZE);
    CHECK(signal_core_graze_gain(FAITH_DIFF_NORMAL, 0) == ZN);
    CHECK(signal_core_graze_gain(FAITH_DIFF_HARD, 0) == ZN);
    CHECK(signal_core_graze_gain(FAITH_DIFF_LUNATIC, 0) == ZN);
    CHECK(signal_core_graze_gain(FAITH_DIFF_EXTRA, 0) == ZN);
    CHECK(signal_core_graze_gain(99, 0) == ZN);                    /* 越界按 NORMAL 夹 */
    /* 激光擦弹表 `th11:0x4a7a68` = {500,400,300,200,200} 存储（⚠ 本卡恒传 is_laser=0，见 TUNING）*/
    CHECK(signal_core_graze_gain(FAITH_DIFF_EASY, 1) == 5000);
    CHECK(signal_core_graze_gain(FAITH_DIFF_NORMAL, 1) == 4000);
    CHECK(signal_core_graze_gain(FAITH_DIFF_HARD, 1) == 3000);
    CHECK(signal_core_graze_gain(FAITH_DIFF_LUNATIC, 1) == 2000);
    CHECK(signal_core_graze_gain(FAITH_DIFF_EXTRA, 1) == 2000);
    /* 与「线上/线下」**无关**（一手：4 个擦弹站点都不读玩家 y）*/
    CHECK(signal_core_step(0, FAITH_SIGNAL_BAND_HOLD, 1, FAITH_DIFF_NORMAL) == ZN);
    CHECK(signal_core_step(0, FAITH_SIGNAL_BAND_OFFLINE, 1, FAITH_DIFF_NORMAL) == ZN - L);
    CHECK(signal_core_step(0, FAITH_SIGNAL_BAND_LOW, 3, FAITH_DIFF_EASY) == G + 3 * ZE);
    CHECK(signal_core_step(100000, FAITH_SIGNAL_BAND_TOP, 2, FAITH_DIFF_NORMAL) == GTOP + 2 * ZN);
    {
        /* 帧差口径（原生 `GRAZE 0x4ccd0c`）：卡侧只吃帧差；回绕 / 重开时帧差为负 ⇒ 传 0。*/
        int32_t graze_counter = 100, prev = graze_counter, dg;
        graze_counter += 1;
        dg = graze_counter - prev;
        CHECK(dg == 1);
        CHECK(signal_core_step(0, FAITH_SIGNAL_BAND_HOLD, dg, FAITH_DIFF_NORMAL) == ZN);
        prev = 5; graze_counter = 0;
        dg = graze_counter - prev;
        CHECK(dg < 0);
        CHECK(signal_core_step(0, FAITH_SIGNAL_BAND_HOLD, dg > 0 ? dg : 0, FAITH_DIFF_NORMAL) == 0);
    }

    /* ==================== 3. 档位判据（离回收线多少 px → band）====================
     * 一手分界（绝对 y 64/80/128，线 = 128）⇔ 线上偏移 >64 / 48…64 / 0…48 / ≤0。*/
    CHECK(signal_core_band(6401) == FAITH_SIGNAL_BAND_TOP);
    CHECK(signal_core_band(6400) == FAITH_SIGNAL_BAND_MID);     /* y == 64 ⇒ 第二档 */
    CHECK(signal_core_band(4801) == FAITH_SIGNAL_BAND_MID);
    CHECK(signal_core_band(4800) == FAITH_SIGNAL_BAND_LOW);     /* y == 80 ⇒ 第三档 */
    CHECK(signal_core_band(1) == FAITH_SIGNAL_BAND_LOW);
    CHECK(signal_core_band(0) == FAITH_SIGNAL_BAND_OFFLINE);    /* y == 128 ⇒ 线下 */
    CHECK(signal_core_band(-1) == FAITH_SIGNAL_BAND_OFFLINE);

    /* ==================== 4. 线下：−0.30/帧 ==================== */
    /* ★★ 2026-09-15f **数值硬钉**（不只是符号）：线上那几条断言写的是 `-L`（符号），所以
     *    `FAITH_SIGNAL_LOSS_OFFLINE` 被写成 3000（= −3.00/帧，**快 10 倍**）时**测试照样绿**
     *    —— 实跑里就是「擦弹一点也不涨信号强度」（用户 2026-09-15 原话：槽恒被压在 0）。
     *    这里把两个数都钉成字面量：
     *      · 常量本身 = **300 千分点**（= −0.30 显示，一手 `th11:0x420a85 lea esi,[edx-0x1e]` = −30 存储、
     *        存储 = 显示 ×100 ⇒ −0.30 显示；同段的量纲自洽检查见 `research-th11-signal.md` §4）；
     *      · 一帧的实际扣量 = **−300**（本文件的槽单位是千分点 `FAITH_SIGNAL_MAX = 130000`）。*/
    CHECK(L == 300);
    CHECK(signal_core_step(50000, FAITH_SIGNAL_BAND_OFFLINE, 0, FAITH_DIFF_NORMAL) == -300);
    CHECK(signal_core_step(50000, FAITH_SIGNAL_BAND_OFFLINE, 0, FAITH_DIFF_NORMAL) == -L);
    CHECK(signal_core_step(100000, FAITH_SIGNAL_BAND_OFFLINE, 0, FAITH_DIFF_NORMAL) == -L);  /* ★ 不叠加档位 */
    CHECK(signal_core_step(60000, FAITH_SIGNAL_BAND_OFFLINE, 5, FAITH_DIFF_NORMAL) == 5 * ZN - L);
    /* 擦弹必须**顶得住**线下衰减（这是「擦弹能涨信号」的最低不变式：一颗擦弹 > 一帧的扣量）*/
    CHECK(ZN > L);
    /* 夹到 0 底（★ 2026-09-15f：一帧只扣 0.30 ⇒ 要试下钳必须把值放到 0.30 以下；
     *   订正前那条用的是 1000（1.00），靠的是「一帧 −3.00」这个**错值**才成立）*/
    slot.signal = 200;                      /* 0.20 显示 < 一帧的 0.30 ⇒ 应当夹到 0 */
    (void)signal_core_apply_step(&slot, FAITH_SIGNAL_BAND_OFFLINE, 0, FAITH_DIFF_NORMAL);
    CHECK(slot.signal == 0);
    slot.signal = 1000;                     /* 1.00 显示：一帧之后应当还剩 0.70（= 700） */
    (void)signal_core_apply_step(&slot, FAITH_SIGNAL_BAND_OFFLINE, 0, FAITH_DIFF_NORMAL);
    CHECK(slot.signal == 700);
    slot.signal = 0;
    (void)signal_core_apply_step(&slot, FAITH_SIGNAL_BAND_OFFLINE, 0, FAITH_DIFF_NORMAL);
    CHECK(slot.signal == 0);
    /* HOLD（自机死亡 / 复活态）：既不加也不减 —— 只有擦弹照给 */
    CHECK(signal_core_step(50000, FAITH_SIGNAL_BAND_HOLD, 0, FAITH_DIFF_NORMAL) == 0);
    CHECK(signal_core_step(50000, FAITH_SIGNAL_BAND_HOLD, 2, FAITH_DIFF_NORMAL) == 2 * ZN);

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
    CHECK(signal_core_apply_step(0, FAITH_SIGNAL_BAND_LOW, 3, FAITH_DIFF_NORMAL) == 0);
    CHECK(signal_core_step(0, FAITH_SIGNAL_BAND_HOLD, 0, FAITH_DIFF_NORMAL) == 0);

    /* 关卡结束归零 + 每帧从 0 重新涨 */
    slot.signal = 90000;
    CHECK(signal_core_reset(&slot) == 1);
    CHECK(signal_core_is_full(slot.signal) == 0);
    (void)signal_core_apply_step(&slot, FAITH_SIGNAL_BAND_LOW, 0, FAITH_DIFF_NORMAL);
    CHECK(slot.signal == 50000);
    (void)signal_core_apply_step(&slot, FAITH_SIGNAL_BAND_LOW, 0, FAITH_DIFF_NORMAL);
    CHECK(slot.signal == 100000);
    CHECK(signal_core_is_full(slot.signal) == 1);

    /* ---- 5.1 「归零的是什么 / 保留的是什么」逐项钉住 ----
     * 卡侧的 `on_stage_start`（虚表 `+0x34` = **每关开场**）与 `on_run_reset`（`+0x4c` = 局末）
     * 都只调本函数 ⇒ 契约就是「**只有信号槽归零**」，其余跨关保留（一手：信仰跨关保留、
     * 小绿点余数跨关保留；一手 `th11:0x431097` 只在玩家登场期把槽写 0）。*/
    {
        faith_slot_t s2;
        faith_slot_init_111(&s2, FAITH_DIFF_NORMAL);        /* 基础 50000、Δ 上限 = 50000 */
        CHECK(signal_core_pickup(&s2, FAITH_EV_BIG, SIGNAL_GREEN_BIG) == 100 * FAITH_FIXED);
        s2.small_rem = 42;                                  /* 小绿点余数（一手：÷100 口径的余数）*/
        s2.signal = 88000;
        CHECK(s2.delta == 100 * FAITH_FIXED && faith_slot_total(&s2) == 50100);
        CHECK(signal_core_reset(&s2) == 1);
        CHECK(s2.signal == 0);                              /* ★ 归零的：信号槽 */
        CHECK(faith_slot_total(&s2) == 50100);              /* ★ 保留的：信仰（基础 + Δ_111）*/
        CHECK(s2.delta == 100 * FAITH_FIXED);
        CHECK(s2.base == 50000 && s2.delta_cap == 50000);   /* 基础 / Δ 上限不动 */
        CHECK(s2.small_rem == 42);                          /* ★ 保留的：小绿点余数 */
        CHECK(s2.combo == 0);                               /* ★ 111 侧从不碰连击槽（见 §13）*/
        CHECK(signal_core_reset(&s2) == 0);
        CHECK(s2.small_rem == 42 && faith_slot_total(&s2) == 50100);
    }

    /* ==================== 6. 满槽阈值（≥100.00 ⇒ 全收）==================== */
    CHECK(signal_core_is_full(0) == 0);
    CHECK(signal_core_is_full(99999) == 0);
    CHECK(signal_core_is_full(100000) == 1);                /* ★ 恰好 100.00 = 满 */
    CHECK(signal_core_is_full(100001) == 1);
    CHECK(signal_core_is_full(130000) == 1);

    /* ==================== 7. 信号强度项 A（一手 `th11:0x41f640`）====================
     * `A = min(⌊槽存储/100⌋, 100)`，单位 0.01 ⇒ 千分点 = `A × 10`（0…1000）。
     * ⚠ 与旧实现不同：**整数阶梯**（floor 到显示值），不是按槽比例连续。*/
    CHECK(signal_core_strength(0) == 0);
    CHECK(signal_core_strength(-5) == 0);
    CHECK(signal_core_strength(1000) == 10);                /* 显示 1.00 ⇒ A=1 ⇒ 0.01 */
    CHECK(signal_core_strength(50000) == 500);              /* 显示 50 ⇒ 0.50 */
    CHECK(signal_core_strength(99999) == 990);              /* 显示 99.999 ⇒ ⌊99⌋ = 0.99（floor！）*/
    CHECK(signal_core_strength(100000) == 1000);            /* 满 ⇒ 1.00 */
    CHECK(signal_core_strength(100001) == 1000);            /* ★ ≥100 一律 1.00（`0x41f64c`）*/
    CHECK(signal_core_strength(130000) == 1000);            /* ★ 130 不改公式（只体现在格数上）*/

    /* ==================== 8. 蓝点分 = 一手整数式 `trunc10((A+B)×Y/100)` ====================
     * 一手 `th11:0x423b7c`：`Y = trunc10(⌊PIV/100⌋)`、`A = min(⌊交信度/100⌋,100)`、
     * `B = min(⌊graze/100⌋,899)`、`S = trunc10((A+B)×Y/100)`、`S ≤ 0 ⇒ 10`。*/
    /* 8.1 擦弹倍率的共用件口径：整数阶梯、每 100 擦弹一步、上限 8.99 = 8990 千分点 */
    CHECK(mof_core_graze_mult(0) == 0);
    CHECK(mof_core_graze_mult(-3) == 0);
    CHECK(mof_core_graze_mult(1) == 0);
    CHECK(mof_core_graze_mult(99) == 0);
    CHECK(mof_core_graze_mult(5000) == 500);                /* 5000 擦弹 = 50 步 = 0.50 */
    CHECK(mof_core_graze_mult(89899) == 8980);
    CHECK(mof_core_graze_mult(89900) == 8990);              /* ★ 89900 才封顶（不是 8990）*/
    CHECK(mof_core_graze_mult(99999) == 8990);
    /* 8.2 「一手文档里的三个点」（研究文档 §2.3，Normal Y=50000、交信度满）*/
    CHECK(signal_core_bonus(50000, mof_core_graze_mult(0), 100000) == 50000);        /* B=0 → 1.00× */
    CHECK(signal_core_bonus(50000, mof_core_graze_mult(100), 100000) == 50500);      /* B=1 → 1.01× */
    CHECK(signal_core_bonus(50000, mof_core_graze_mult(8900), 100000) == 94500);     /* B=89 → 1.89× */
    CHECK(signal_core_bonus(50000, mof_core_graze_mult(89900), 100000) == 499500);   /* 9.99× 封顶 */
    /* 8.3 ★ 下限：S ≤ 0 ⇒ 10（一手 `th11:0x423bce: mov ebx,0xa`，即社区说的「白回収」）*/
    CHECK(signal_core_bonus(50000, 0, 0) == 10);
    CHECK(signal_core_bonus(25000, 0, 0) == 10);
    CHECK(signal_core_bonus(200000, 0, 0) == 10);
    CHECK(signal_core_bonus(0, 8990, 130000) == 10);        /* 信仰 0（我们槽里到不了，防御）*/
    CHECK(signal_core_bonus(-100, 5000, 100000) == 10);     /* 负信仰也不再给负数 */
    /* 8.4 只有信号、没有擦弹 */
    CHECK(signal_core_bonus(50000, 0, 100000) == 50000);
    CHECK(signal_core_bonus(50000, 0, 130000) == 50000);    /* 130 也是 1.00 */
    CHECK(signal_core_bonus(50000, 0, 50000) == 25000);     /* A=50 ⇒ 0.50 */
    CHECK(signal_core_bonus(25000, 0, 50000) == 12500);
    CHECK(signal_core_bonus(50000, 0, 99999) == 49500);     /* ★ A=99（floor）⇒ 0.99 */
    /* 8.5 擦弹倍率当主角 */
    CHECK(signal_core_bonus(50000, 5000, 100000) == 300000);   /* 5.00 + 1.00 = 6.00 */
    CHECK(signal_core_bonus(1000, 5000, 0) == 5000);           /* B=500 ⇒ 5.00 × 1000 */
    /* 8.6 上限进公式：8990 + 1000 = 9990 千分点 ⇒ 9.99 × Y */
    CHECK(signal_core_bonus(50000, 8990, 130000) == 499500);
    CHECK(signal_core_bonus(100000, 8990, 130000) == 999000);
    CHECK(signal_core_bonus(200000, 8990, 130000) == 1998000);
    /* 8.7 难度初值 × 满乘数 */
    CHECK(signal_core_bonus(FAITH_BASE_111_EASY, 8990, 130000) == 249750);
    CHECK(signal_core_bonus(FAITH_BASE_111_HARD, 0, 100000) == 100000);
    /* 8.8 ★ `Y` 与 `S` **两次 trunc10**（一手 `0x41f510` 与 `0x423bc5/0x423bc8`）*/
    CHECK(signal_core_bonus(33333, 1000, 0) == 33330);      /* Y = trunc10(33333) = 33330、B=100 ⇒ 1.00× */
    CHECK(signal_core_bonus(50015, 1000, 0) == 50010);      /* Y = 50010 */
    CHECK(signal_core_bonus(50000, 0, 1000) == 500);        /* A=1,B=0 ⇒ 0.01×50000 = 500 */
    /* 8.9 溢出安全：Y 上限量级 × 9.99（用 int64 中间量，结果仍 ≤ INT32_MAX）*/
    v = signal_core_bonus(0x7fffffff / 10, 8990, 130000);
    CHECK(v > 0);
    CHECK(v == 2145336110);                                 /* = trunc10(999×214748360/100) */
    /* 8.10 定点版与原始单位版同刻度（返回值都是 ×FAITH_FIXED）*/
    CHECK(signal_core_bonus_fixed(50000 * FAITH_FIXED, 0, 100000) == 50000 * FAITH_FIXED);
    CHECK(signal_core_bonus_fixed(1000 * FAITH_FIXED, 8990, 130000) == 9990 * FAITH_FIXED);
    CHECK(signal_core_bonus_fixed(0, 8990, 130000) == 10 * FAITH_FIXED);   /* ★ 下限 10 也走定点 */

    /* ==================== 9. HUD 填充条宽度（一手 `th11:0x41b679`–`0x41b711`）====================
     * `level = min(⌊槽/100⌋, 100)/10 ∈ 0..10`、`宽 = 9 + 4×level` px（槽 0 → 9、≥100.00 → 49）。*/
    CHECK(signal_core_bar_level(0) == 0);
    CHECK(signal_core_bar_level(-1) == 0);
    CHECK(signal_core_bar_level(9999) == 0);                /* 显示 9.999 ⇒ level 0 */
    CHECK(signal_core_bar_level(10000) == 1);               /* 显示 10 ⇒ level 1 */
    CHECK(signal_core_bar_level(50000) == 5);
    CHECK(signal_core_bar_level(100000) == 10);             /* 满 ⇒ level 10 */
    CHECK(signal_core_bar_level(130000) == 10);             /* 130 也被钳到 100（一手 `0x41b687`）*/
    CHECK(signal_core_bar_width_px(0) == 9);
    CHECK(signal_core_bar_width_px(10000) == 13);
    CHECK(signal_core_bar_width_px(50000) == 29);
    CHECK(signal_core_bar_width_px(100000) == 49);
    CHECK(signal_core_bar_width_px(130000) == 49);          /* ★ 49 px 封顶 */

    /* ==================== 10. 与共用件的接缝（111 槽）==================== */
    /* 10.1 初值随难度（一手：表 `th11:0x4a79e8` = 25000/50000/100000/200000/200000）*/
    faith_slot_init_111(&slot, FAITH_DIFF_EASY);    CHECK(slot.base == 25000);
    faith_slot_init_111(&slot, FAITH_DIFF_NORMAL);  CHECK(slot.base == 50000);
    faith_slot_init_111(&slot, FAITH_DIFF_HARD);    CHECK(slot.base == 100000);
    faith_slot_init_111(&slot, FAITH_DIFF_LUNATIC); CHECK(slot.base == 200000);
    faith_slot_init_111(&slot, FAITH_DIFF_EXTRA);   CHECK(slot.base == 200000);
    faith_slot_init_111(&slot, 99);                 CHECK(slot.base == 200000);   /* 越界钳 LUNATIC */
    /* 10.2 Δ 上限 = 原版基础（= 一手「上限 = 初值 ×2」，表 `th11:0x4a79fc`）*/
    faith_slot_init_111(&slot, FAITH_DIFF_HARD);
    CHECK(slot.delta_cap == 100000);
    CHECK(slot.delta == 0);
    CHECK(slot.signal == 0);
    CHECK(slot.small_rem == 0);
    CHECK(slot.combo == 0);
    CHECK(faith_slot_total(&slot) == 100000);
    /* 10.3 大绿点 +100（= 一手 type 3），能加到上限 */
    CHECK(faith_slot_add(&slot, FAITH_EV_BIG, SIGNAL_GREEN_BIG) == SIGNAL_GREEN_BIG * FAITH_FIXED);
    CHECK(faith_slot_total(&slot) == 100100);
    CHECK(faith_slot_add(&slot, FAITH_EV_BIG, SIGNAL_GREEN_BIG) == SIGNAL_GREEN_BIG * FAITH_FIXED);
    CHECK(faith_slot_total(&slot) == 100200);
    CHECK(faith_slot_add(&slot, FAITH_EV_BIG, 999999) == 99800 * FAITH_FIXED);
    CHECK(slot.delta == slot.delta_cap * FAITH_FIXED);
    CHECK(faith_slot_total(&slot) == 200000);               /* = 基础 ×2 */
    CHECK(faith_slot_add(&slot, FAITH_EV_BIG, 100) == 0);
    CHECK(faith_slot_total(&slot) == 200000);
    /* 10.4 中绿点 +10（= 一手 type 9）*/
    faith_slot_init_111(&slot, FAITH_DIFF_EASY);
    CHECK(faith_slot_add(&slot, FAITH_EV_GREEN, SIGNAL_GREEN_MID) == 10 * FAITH_FIXED);
    CHECK(faith_slot_total(&slot) == 25010);
    /* 10.5 小绿点：一手「每颗显示 +0.1」⇔ 本仓「每 100 颗 +10」，余数跨关保留。
     * ⚠ `faith_slot_add_cancel()` 的 `out_111_add` 回的是**定点**量（10 → 1000）。*/
    faith_slot_init_111(&slot, FAITH_DIFF_NORMAL);
    v = -1;
    faith_slot_add_cancel(0, &slot, 99, 0, &v);
    CHECK(v == 0);
    CHECK(slot.small_rem == 99);
    CHECK(slot.delta == 0);
    faith_slot_add_cancel(0, &slot, 1, 0, &v);
    CHECK(v == SIGNAL_GREEN_SMALL_V * FAITH_FIXED);
    CHECK(v == slot.delta);
    CHECK(slot.small_rem == 0);
    CHECK(faith_slot_total(&slot) == 50010);
    faith_slot_add_cancel(0, &slot, 250, 0, &v);
    CHECK(v == 2 * SIGNAL_GREEN_SMALL_V * FAITH_FIXED);
    CHECK(slot.small_rem == 50);
    CHECK(slot.delta == 3 * SIGNAL_GREEN_SMALL_V * FAITH_FIXED);
    CHECK(faith_slot_total(&slot) == 50030);
    faith_slot_reset(&slot);
    CHECK(slot.small_rem == 50);
    CHECK(slot.delta == 0);
    CHECK(slot.signal == 0);
    /* 10.6 分槽记账：动信号**不动**信仰、动信仰**不动**信号 */
    faith_slot_init_111(&slot, FAITH_DIFF_NORMAL);
    slot.signal = 60000;
    CHECK(faith_slot_add(&slot, FAITH_EV_BIG, SIGNAL_GREEN_BIG) == 100 * FAITH_FIXED);
    CHECK(slot.signal == 60000);
    faith_slot_add_cancel(0, &slot, 100, 0, &v);
    CHECK(slot.signal == 60000);
    (void)signal_core_apply_step(&slot, FAITH_SIGNAL_BAND_TOP, 0, FAITH_DIFF_NORMAL);
    CHECK(slot.delta == 110 * FAITH_FIXED);
    CHECK(slot.small_rem == 0);
    CHECK(faith_slot_total(&slot) == 50110);

    /* ==================== 11. 灵击：门槛 / 扣 1 档 / 下限 1.05 ====================
     * ⚠ 这一组是**用户拍板的 TH18 映射**，不是复刻：一手 th11 是
     *   `CURRENT_POWER / BOMB_COST != 0`、扣 `BOMB_COST`（5 机 = 20 = 1.00、魔理沙A = 12 = 0.60）、
     *   **无下限**（研究文档 §1）。我们的 2.00 / 1.05 是换算 + 「サービス」。*/
    CHECK(mof_core_power_can(0) == 0);
    CHECK(mof_core_power_can(199) == 0);                    /* 1.99 档 ⇒ 放不出（门槛 ≥2.00）*/
    CHECK(mof_core_power_can(200) == 1);                    /* ★ 恰好 2.00 档 ⇒ 放得出 */
    CHECK(mof_core_power_can(400) == 1);
    v = mof_core_power_spend(400).new_power;
    CHECK(v == 300);
    CHECK(mof_core_power_spend(300).new_power == 200);
    CHECK(mof_core_power_spend(299).new_power == 199);
    CHECK(mof_core_power_spend(200).new_power == 105);      /* ★ 2.00 → 1.05（不是 1.00！）*/
    CHECK(mof_core_power_spend(200).ok == 1);
    CHECK(mof_core_power_spend(199).ok == 0);
    CHECK(mof_core_power_spend(199).new_power == 199);
    CHECK(mof_core_power_spend(105).ok == 0);
    CHECK(mof_core_power_spend(105).new_power == 105);

    /* ==================== 12. 契约 PV 总式（旧口径的回归面）==================== */
    /* 12.1 只装 111：**不含乘数**的那一半 = `信仰_111` */
    faith_slot_init_111(&slot, FAITH_DIFF_NORMAL);
    CHECK(signal_core_pv_111(&slot, 50000, 0) == 50000);
    /* 12.2 ★ 带乘数的 PV = 一手那个 S（2026-09-15 改；旧值是「乘完不取整」）*/
    CHECK(signal_core_pv_111_scaled(&slot, 50000, 0, 0, 0) == 10);            /* 没擦弹没信号 ⇒ 下限 10 */
    CHECK(signal_core_pv_111_scaled(&slot, 50000, 0, 0, 100000) == 50000);    /* 信号 1.00 */
    CHECK(signal_core_pv_111_scaled(&slot, 50000, 0, 5000, 100000) == 300000);/* 5.00 + 1.00 */
    CHECK(signal_core_pv_111_scaled(&slot, 50000, 0, 8990, 130000) == 499500);/* 9.99 上限 */
    (void)faith_slot_add(&slot, FAITH_EV_BIG, SIGNAL_GREEN_BIG);              /* 大绿点 +100 */
    CHECK(faith_slot_total(&slot) == 50100);
    CHECK(signal_core_pv_111(&slot, 50100, 0) == 50100);
    faith_slot_init_111(&slot, FAITH_DIFF_LUNATIC);
    CHECK(signal_core_pv_111(&slot, 200000, 0) == 200000);
    CHECK(signal_core_pv_111_scaled(&slot, 200000, 0, 8990, 130000) == 1998000);
    /* 12.3 两卡同装：`PV = 基础均值 + Δ_110 + Δ_111`（**基础只取一份**）*/
    faith_slot_init_111(&slot, FAITH_DIFF_NORMAL);
    CHECK(signal_core_pv_111(&slot, 100000, 1) == 50000);
    (void)faith_slot_add(&slot, FAITH_EV_BIG, SIGNAL_GREEN_BIG);
    CHECK(faith_slot_total(&slot) == 50100);
    CHECK(signal_core_pv_111(&slot, 100100 + 2000, 1) == 52100);
    CHECK(signal_core_pv_111_scaled(&slot, 100100 + 2000, 1, 8990, 130000) == 520470);  /* ★ S 式带 trunc10 */
    faith_slot_init_111(&slot, FAITH_DIFF_EASY);
    CHECK(signal_core_pv_111(&slot, 75000, 1) == 37500);
    CHECK(signal_core_pv_111(&slot, 75000, 1) != 75000);
    /* 12.4 防御：槽指针为空 / 聚合值为负 */
    CHECK(signal_core_pv_111(0, 100000, 1) == 0);
    faith_slot_init_111(&slot, FAITH_DIFF_NORMAL);
    CHECK(signal_core_pv_111(&slot, -1000, 1) >= 0);
    /* 12.5 共用件的「只装 110」那一支仍是对的（110 侧不参与乘数）*/
    CHECK(mof_core_pv(1, 0, 50000, 0, 0) == 50000);
    CHECK(mof_core_pv(1, 0, 52000, 8990, 1000) == 52000);
    /* 12.6 ★★ 2026-09-15：**跨卡等式撤下**（原来断言 `signal_core_pv_111_scaled() == mof_core_pv()`）。
     *   一手把 S 式钉成 `trunc10((A+B)×Y/100)` + 下限 10，而共用件 `mof_core_pv()` 仍是旧乘式
     *   （`Y × (A+B)/100` 不取整、无下限）⇒ 两者在**一般输入上不等**。
     *   `mof_core.c` 不归本卡（110 共享件）⇒ 这里**只断言 111 侧的一手值**，
     *   「谁去把 `mof_core_pv()` 对齐」列在报告里。下面的三条把差异**显式**钉住（防有人悄悄改一边）。*/
    {
        int32_t y = 52100, gm = 8990, sig = 130000;         /* A=100, B=899 ⇒ X=999 */
        int32_t mine = signal_core_bonus(y, gm, sig);
        int32_t theirs = (int32_t)(((int64_t)y * (int64_t)(gm + signal_core_strength(sig))) / 1000);
        CHECK(mine == 520470);                              /* 一手：trunc10(999×52100/100) */
        CHECK(theirs == 520479);                            /* 共用件旧式：不取整 */
        CHECK(mine != theirs);                              /* ★ 差异是有意的、见上 */
    }

    /* ==================== 13. 绿点收集入账 `signal_core_pickup()` ====================
     * 一手：大绿点 = type 3（显示 +100）、中 = type 9（+10）、小 = type 8（每颗 +0.1 显示 + 10 分）。
     * ★ 2026-09-15：**111 侧不碰连击槽**（th11 一手没有连击槽 ⇒ 旧断言 `combo == 130/100` 已删）。*/
    faith_slot_init_111(&slot, FAITH_DIFF_NORMAL);
    CHECK(signal_core_pickup(&slot, FAITH_EV_BIG, SIGNAL_GREEN_BIG) == 100 * FAITH_FIXED);
    CHECK(faith_slot_total(&slot) == 50100);
    CHECK(slot.combo == 0);                                 /* ★ 连击槽纹丝不动 */
    CHECK(signal_core_pickup(&slot, FAITH_EV_GREEN, SIGNAL_GREEN_MID) == 10 * FAITH_FIXED);
    CHECK(faith_slot_total(&slot) == 50110);
    CHECK(slot.combo == 0);
    CHECK(SIGNAL_GREEN_BIG == 100 && SIGNAL_GREEN_MID == 10);
    /* 13.1 `value <= 0`：不认；NULL 安全 */
    v = slot.delta;
    CHECK(signal_core_pickup(&slot, FAITH_EV_BIG, 0) == 0);
    CHECK(signal_core_pickup(&slot, FAITH_EV_BIG, -5) == 0);
    CHECK(slot.delta == v && slot.combo == 0);
    CHECK(signal_core_pickup(0, FAITH_EV_BIG, 100) == 0);
    /* 13.2 Δ_111 上限 = 原版基础：加到满之后一颗都进不去 */
    {
        faith_slot_init_111(&slot, FAITH_DIFF_NORMAL);
        CHECK(signal_core_pickup(&slot, FAITH_EV_BIG, 60000) == 50000 * FAITH_FIXED);
        CHECK(faith_slot_total(&slot) == 100000);
        CHECK(signal_core_pickup(&slot, FAITH_EV_BIG, SIGNAL_GREEN_BIG) == 0);
        CHECK(faith_slot_total(&slot) == 100000);
        CHECK(slot.combo == 0);
    }
    /* 13.3 110 的槽一点都不会被碰 */
    {
        faith_slot_t s110, s111;
        faith_slot_init_110(&s110);
        faith_slot_init_111(&s111, FAITH_DIFF_NORMAL);
        CHECK(signal_core_pickup(&s111, FAITH_EV_BIG, SIGNAL_GREEN_BIG) == 100 * FAITH_FIXED);
        CHECK(s110.delta == 0 && faith_slot_total(&s110) == 50000);
        CHECK(faith_slot_total(&s111) == 50100);
    }

    /* ==================== 14. Miss 扣信仰（一手 `th11:0x4327d0`）====================
     * `Δ -= 显示 100,000`、夹 0（原版地板 = 该难度初值 = 我们的 base）。*/
    faith_slot_init_111(&slot, FAITH_DIFF_NORMAL);          /* base 50000 ⇒ Δ 最多 50000 */
    (void)faith_slot_add(&slot, FAITH_EV_BIG, 5000);
    CHECK(signal_core_miss_penalty(&slot) == 5000);         /* 只有 5000 ⇒ 全扣 */
    CHECK(slot.delta == 0);
    CHECK(faith_slot_total(&slot) == 50000);                /* = 初值（一手的地板）*/
    CHECK(signal_core_miss_penalty(&slot) == 0);            /* Δ=0 ⇒ 扣不动 */
    faith_slot_init_111(&slot, FAITH_DIFF_LUNATIC);         /* base 200000 ⇒ Δ 上限 200000 */
    (void)faith_slot_add(&slot, FAITH_EV_BIG, 150000);
    CHECK(signal_core_miss_penalty(&slot) == 100000);       /* 一手值：显示 −100,000 */
    CHECK(slot.delta == 50000 * FAITH_FIXED);
    CHECK(faith_slot_total(&slot) == 250000);
    CHECK(signal_core_miss_penalty(&slot) == 50000);
    CHECK(faith_slot_total(&slot) == 200000);
    CHECK(signal_core_miss_penalty(0) == 0);                /* NULL 安全 */

    /* ==================== 15. 得分跨阈值奖残 + 命碎道具（一手路径 ①②）====================
     * ① 得分跨阈值 → `th11:0x41f001: call 0x420950` = **残机 +1**（钳 9）；
     * ② 捡命碎道具：一手是 **type 5**（`th11:0x423c3e` → `0x4209b0`，5 碎 = 1 残）；
     *    它在 TH18 侧的对应物 = **item type 4**（`engine/card/th18/06-resource-economy.md` §5）。*/
    CHECK(signal_core_lifefrag_table_len() == 1);           /* ❓ 表只取到第 0 项 */
    CHECK(signal_core_lifefrag_threshold(0, 0) == 1000000000LL);
    CHECK(signal_core_lifefrag_threshold(0, 1) == 1000000000LL);   /* EX 表 `th11:0x4a3d60` 同值 */
    CHECK(signal_core_lifefrag_threshold(1, 0) == -1);      /* 越界 = 表里没有这一项 */
    CHECK(signal_core_lifefrag_threshold(-1, 0) == -1);
    {
        int32_t idx = 0;
        CHECK(signal_core_lifefrag_score(999999999LL, &idx, 0) == 0);   /* ★ TH18 分上限也打不到 */
        CHECK(idx == 0);
        CHECK(signal_core_lifefrag_score(1000000000LL, &idx, 0) == 1);  /* 恰好跨过 ⇒ 发 */
        CHECK(idx == 1);
        CHECK(signal_core_lifefrag_score(2000000000LL, &idx, 0) == 0);  /* 越界 ⇒ 不再发 */
        CHECK(idx == 1);
        CHECK(signal_core_lifefrag_score(1000000000LL, 0, 0) == 0);     /* NULL 安全 */
        CHECK(signal_core_lifefrag_score(2000000000LL, &idx, 1) == 0);  /* EX 表也越界 */
    }
    /* 「现值为不可达」这条显式钉住（❓ 表未取到；登记 TUNING）*/
    CHECK(signal_core_lifefrag_threshold(0, 0) > 999999999LL);          /* TH18 分数上限 */
    CHECK(SIGNAL_LIFEFRAG_PER_LIFE_TH11 == 5 && SIGNAL_LIFEFRAG_PER_LIFE_TH18 == 3);
    CHECK(signal_core_is_lifefrag_item(SIGNAL_ITEM_TYPE_LIFEFRAG) == 1);
    CHECK(signal_core_is_lifefrag_item(4) == 0);            /* ★ 旧值（type 4）现在必须**不是**命碎 */
    CHECK(signal_core_is_lifefrag_item(6) == 0);            /* 同上：type 6 = 符卡碎（原生另一条路）*/
    CHECK(signal_core_is_lifefrag_item(5) == 0);            /* ★ th11 的 type 7(1UP)/th18 的 type 5 都不是 */
    CHECK(signal_core_is_lifefrag_item(7) == 0);
    CHECK(signal_core_is_lifefrag_item(0) == 0);
    CHECK(signal_core_is_lifefrag_item(1) == 0);

    /* ==================== 16. ★ 六条「支援特技」的纯逻辑（2026-09-15 本轮实装）====================
     * 判据一手见 `cards/signal_core.h` 的「支援特技」整段（含**本轮订正过的 TH11 输入位表**）。
     * 这里**逐条钉**状态机与阈值，特别是三条反直觉的形状：
     *   ① 紫必须**松手再按**（按住不放永远不发动）；
     *   ② 紫的取消条件是 `INPUT & 0x9` = **射击|低速**（不是上下）；
     *   ③ 萃香/文的计数器**触发时不清零** ⇒ 过阈值之后是**持续态**。*/

    /* --- 16.1 输入位（★ 本轮订正：0x9 = 射击|低速）--- */
    CHECK(SIGNAL_IN_SHOT == 0x1 && SIGNAL_IN_FOCUS == 0x8);
    CHECK(SIGNAL_IN_Z_OR_SHIFT == 0x9);
    CHECK(SIGNAL_IN_UP == 0x10 && SIGNAL_IN_DOWN == 0x20 && SIGNAL_IN_LEFT == 0x40 && SIGNAL_IN_RIGHT == 0x80);
    CHECK(signal_core_pressing_atk(0x1) == 1);          /* 射击 */
    CHECK(signal_core_pressing_atk(0x8) == 1);          /* 低速（一手 `th11:0x43036d: shr eax,3`）*/
    CHECK(signal_core_pressing_atk(0x9) == 1);
    CHECK(signal_core_pressing_atk(0x10) == 0);         /* ★ 上**不是**这一位（旧读法作废）*/
    CHECK(signal_core_pressing_atk(0x20) == 0);         /* ★ 下也不是 */
    CHECK(signal_core_pressing_atk(0x0) == 0);
    CHECK(signal_core_pressing_dir(0x40) == 1 && signal_core_pressing_dir(0x0) == 0);
    CHECK(signal_core_dir_of(0x40) == SIGNAL_GAP_DIR_LEFT);
    CHECK(signal_core_dir_of(0x80) == SIGNAL_GAP_DIR_RIGHT);
    CHECK(signal_core_dir_of(0x0) == 0);
    CHECK(signal_core_dir_of(0x10) == 0);               /* 上/下不是左右（一手解码器把它们分成 1/2）*/
    CHECK(signal_core_dir_of(0xC0) == SIGNAL_GAP_DIR_LEFT);  /* 左右同按 ⇒ 左（照抄一手优先级）*/
    /* 帕秋莉：`0x400` 的忠实读法 = 「射击 + 低速位同帧」（TH18 没有这一位 ⇒ 卡侧合成）*/
    CHECK(signal_core_patchouli_pressed(0x1 | 0x8) == 1);
    CHECK(signal_core_patchouli_pressed(0x1) == 0);
    CHECK(signal_core_patchouli_pressed(0x8) == 0);
    CHECK(signal_core_patchouli_pressed(0x1 | 0x8 | 0x40) == 1);   /* 方向键不干扰 */

    /* ★★ 2026-09-16（用户实跑纠错）：「应当是**同时按下 shift+Z**，而不是按着 Z 再按 shift 就切换」。
     * 判据 = `signal_core_patchouli_fresh()`：两键各自都要在「刚按下」的窗口内
     * （`SIGNAL_PATCHOULI_FRESH_FRAMES` = 5 = 一手 `th11:0x43605e`/`0x43606d` 的两个 `cmp …,5` 的形状）。*/
    CHECK(SIGNAL_PATCHOULI_FRESH_FRAMES == 5);
    /* ① 同时按下（两键都在第 1 帧）⇒ 算 */
    CHECK(signal_core_patchouli_fresh(0x1 | 0x8, 1, 1) == 1);
    CHECK(signal_core_patchouli_fresh(0x1 | 0x8, 5, 5) == 1);          /* 窗口边界：5 帧内都算 */
    /* ② 「按着 Z 很久，再按 Shift」⇒ **不算**（Z 的保持计数已经超出窗口）*/
    CHECK(signal_core_patchouli_fresh(0x1 | 0x8, 6, 1) == 0);
    CHECK(signal_core_patchouli_fresh(0x1 | 0x8, 100, 1) == 0);
    /* ③ 反向同理（先按 Shift 很久再按 Z）*/
    CHECK(signal_core_patchouli_fresh(0x1 | 0x8, 1, 6) == 0);
    /* ④ 两键都按了很久 ⇒ 不算（闩锁之外的第二道保护：不会因为按住不放而反复切档）*/
    CHECK(signal_core_patchouli_fresh(0x1 | 0x8, 30, 30) == 0);
    /* ⑤ 缺一个键 / 计数为 0（这一帧没按）⇒ 不算 */
    CHECK(signal_core_patchouli_fresh(0x1, 1, 0) == 0);
    CHECK(signal_core_patchouli_fresh(0x8, 0, 1) == 0);
    CHECK(signal_core_patchouli_fresh(0x0, 0, 0) == 0);

    /* --- 16.2 紫·缺口瞬移：认边门槛 = TH18 的 x 钳位 0x5c00（不是文档里的 0x4800）--- */
    CHECK(SIGNAL_GAP_EDGE_FIXED == 0x5c00);
    CHECK(SIGNAL_GAP_WINDOW_VERDICT == 8);
    CHECK(signal_gap_edge(0x40, -0x5c00) == SIGNAL_GAP_DIR_LEFT);
    CHECK(signal_gap_edge(0x80,  0x5c00) == SIGNAL_GAP_DIR_RIGHT);
    CHECK(signal_gap_edge(0x40, -0x5bff) == 0);         /* 差一格就不算贴边 */
    CHECK(signal_gap_edge(0x80, -0x5c00) == 0);         /* 贴左边界但按的是右键 ⇒ 不认（卡 41 同款）*/
    CHECK(signal_gap_edge(0x40,  0x5c00) == 0);
    CHECK(signal_gap_edge(0x40, 0) == 0);

    {
        signal_gap_t g;
        int dir, i;

        /* (a) 不贴边 ⇒ 永远不武装、不发动 */
        memset(&g, 0, sizeof g);
        for (i = 0; i < 40; ++i) CHECK(signal_gap_tick(&g, 0x40, 0, 0, &dir) == 0);
        CHECK(g.phase == SIGNAL_GAP_IDLE && g.wins == 0);

        /* (b) ★ 贴边后**一直按住**同方向 ⇒ 永远不发动（一手 state 1/3 里 `dir==同侧` 只 frames++）*/
        memset(&g, 0, sizeof g);
        for (i = 0; i < 40; ++i) CHECK(signal_gap_tick(&g, 0x40, -0x5c00, 0, &dir) == 0);
        CHECK(g.phase == SIGNAL_GAP_ARMED && g.wins == 0);
        CHECK(g.frames == 39);                          /* 每帧 ++，就是不进 WAIT */

        /* (c) ★ 松手再按 ⇒ 第 2 帧发动（一手 `0x430ea9`→state 2 → `0x430eff`→state 99）*/
        memset(&g, 0, sizeof g);
        CHECK(signal_gap_tick(&g, 0x40, -0x5c00, 0, &dir) == 0);   /* 帧1：武装 */
        CHECK(g.phase == SIGNAL_GAP_ARMED && g.frames == 0);
        CHECK(signal_gap_tick(&g, 0x00, -0x5c00, 0, &dir) == 0);   /* 帧2：松手 ⇒ 开窗 */
        CHECK(g.phase == SIGNAL_GAP_WAIT && g.frames == 1);
        CHECK(signal_gap_tick(&g, 0x40, -0x5c00, 0, &dir) == 1);   /* 帧3：再按 ⇒ 发动 */
        CHECK(dir == SIGNAL_GAP_DIR_LEFT && g.phase == SIGNAL_GAP_ACTIVE && g.wins == 1);
        /* 原生接手期间（`native_active`）⇒ 压回 IDLE、不重复触发 */
        CHECK(signal_gap_tick(&g, 0x40, -0x5c00, 1, &dir) == 0);
        CHECK(g.phase == SIGNAL_GAP_IDLE && g.wins == 1 && g.blocked == 1);
        /* 原生收尾（active 归 0）⇒ 停在 IDLE，且**必须重新松手再按**才会再发 */
        CHECK(signal_gap_tick(&g, 0x40, -0x5c00, 0, &dir) == 0);
        CHECK(g.phase == SIGNAL_GAP_ARMED);

        /* (d) 窗口边界：松手后第 8 帧仍可发动、第 9 帧起超时取消 */
        memset(&g, 0, sizeof g);
        (void)signal_gap_tick(&g, 0x40, -0x5c00, 0, &dir);          /* 武装 */
        (void)signal_gap_tick(&g, 0x00, -0x5c00, 0, &dir);          /* 开窗 frames=1 */
        for (i = 0; i < 7; ++i) CHECK(signal_gap_tick(&g, 0x00, -0x5c00, 0, &dir) == 0);
        CHECK(g.frames == 8);                                        /* 松手后第 8 帧 */
        CHECK(signal_gap_tick(&g, 0x40, -0x5c00, 0, &dir) == 1);     /* ★ 还能发动 */

        memset(&g, 0, sizeof g);
        (void)signal_gap_tick(&g, 0x40, -0x5c00, 0, &dir);
        (void)signal_gap_tick(&g, 0x00, -0x5c00, 0, &dir);
        for (i = 0; i < 8; ++i) (void)signal_gap_tick(&g, 0x00, -0x5c00, 0, &dir);
        CHECK(g.frames == 9);
        CHECK(signal_gap_tick(&g, 0x40, -0x5c00, 0, &dir) == 0);     /* ★ 第 9 帧超时 ⇒ 不发动 */
        CHECK(g.phase == SIGNAL_GAP_IDLE && g.wins == 0);

        /* (e) 武装中按了**另一侧** ⇒ 取消（一手 `th11:0x430ed0`）*/
        memset(&g, 0, sizeof g);
        (void)signal_gap_tick(&g, 0x40, -0x5c00, 0, &dir);
        CHECK(g.phase == SIGNAL_GAP_ARMED);
        (void)signal_gap_tick(&g, 0x80, -0x5c00, 0, &dir);
        CHECK(g.phase == SIGNAL_GAP_IDLE);

        /* (f) ★★ 按**射击**或**低速**就取消（一手 `th11:0x430e79` 的 `test BYTE [0x4c93c0],0x9`；
         *      旧读法说是「上/下」——这里把两种读法都钉住，谁改回去就红）*/
        memset(&g, 0, sizeof g);
        (void)signal_gap_tick(&g, 0x40, -0x5c00, 0, &dir);           /* 武装 */
        (void)signal_gap_tick(&g, 0x00, -0x5c00, 0, &dir);           /* 开窗 */
        CHECK(g.phase == SIGNAL_GAP_WAIT);
        CHECK(signal_gap_tick(&g, 0x01, -0x5c00, 0, &dir) == 0);     /* 射击 ⇒ 取消 */
        CHECK(g.phase == SIGNAL_GAP_IDLE && g.frames == 0);
        memset(&g, 0, sizeof g);
        (void)signal_gap_tick(&g, 0x40, -0x5c00, 0, &dir);
        (void)signal_gap_tick(&g, 0x00, -0x5c00, 0, &dir);
        CHECK(signal_gap_tick(&g, 0x08, -0x5c00, 0, &dir) == 0);     /* 低速 ⇒ 取消 */
        CHECK(g.phase == SIGNAL_GAP_IDLE);
        memset(&g, 0, sizeof g);
        (void)signal_gap_tick(&g, 0x40, -0x5c00, 0, &dir);
        (void)signal_gap_tick(&g, 0x00, -0x5c00, 0, &dir);
        CHECK(signal_gap_tick(&g, 0x10, -0x5c00, 0, &dir) == 0);     /* 上 ⇒ **不**取消（窗口继续）*/
        CHECK(g.phase == SIGNAL_GAP_WAIT && g.frames == 2);

        /* (g) 右侧对称：贴右边界 + 松手 + 再按右 ⇒ dir=2（自右向左）*/
        memset(&g, 0, sizeof g);
        (void)signal_gap_tick(&g, 0x80, 0x5c00, 0, &dir);
        (void)signal_gap_tick(&g, 0x00, 0x5c00, 0, &dir);
        CHECK(signal_gap_tick(&g, 0x80, 0x5c00, 0, &dir) == 1);
        CHECK(dir == SIGNAL_GAP_DIR_RIGHT);

        CHECK(signal_gap_tick(0, 0x40, 0, 0, &dir) == 0);            /* NULL 安全 */
        CHECK(signal_gap_tick(&g, 0x40, 0, 0, 0) == 0);              /* out_dir 可空 */
    }

    /* --- 16.3 萃香 / 文：>10 帧、**触发不清零**、`rising` 只在过阈值那一帧 --- */
    {
        signal_hold_t h;
        int rising, i, on = -1;

        memset(&h, 0, sizeof h);
        CHECK(SIGNAL_SUPPORT_HOLD_FRAMES == 10);
        /* 第 1..10 帧：未过阈值（一手 `cmp …,0xa` + `jle` ⇒ 必须**严格大于 10**）*/
        for (i = 1; i <= 10; ++i) {
            rising = -1;
            on = signal_hold_tick(&h, 1, &rising);
            CHECK(on == 0 && rising == 0);
        }
        CHECK(h.frames == 10 && h.fires == 0);
        rising = -1;
        CHECK(signal_hold_tick(&h, 1, &rising) == 1);                /* 第 11 帧：过阈值 */
        CHECK(rising == 1 && h.fires == 1 && h.frames == 11);
        /* ★★ 一手**不清零**（`th11:0x430666` 之后直接落到 0x4306ce）⇒ 持续态、每帧都成立，
         *    但 `rising` 只有第一次是 1（卡侧只按上升沿收一次费）*/
        for (i = 0; i < 20; ++i) {
            rising = -1;
            CHECK(signal_hold_tick(&h, 1, &rising) == 1);
            CHECK(rising == 0);
        }
        CHECK(h.frames == 11 && h.fires == 1);
        /* 判据一断 ⇒ 归零；再攒一轮要重新数 11 帧 */
        CHECK(signal_hold_tick(&h, 0, &rising) == 0);
        CHECK(h.frames == 0 && h.active == 0 && h.fires == 1);
        for (i = 1; i <= 10; ++i) CHECK(signal_hold_tick(&h, 1, &rising) == 0);
        CHECK(signal_hold_tick(&h, 1, &rising) == 1 && h.fires == 2);
        CHECK(signal_hold_tick(0, 1, &rising) == 0);                 /* NULL 安全 */
        CHECK(signal_hold_tick(&h, 1, 0) == 1);                      /* rising 可空 */
    }

    /* --- 16.4 帕秋莉五档（formation）：归一化 / 槽号映射 / 四格偏移表 --- */
    {
        signal_cell_t c;
        int32_t f, i;

        CHECK(SIGNAL_FORMATION_COUNT == 5);
        CHECK(SIGNAL_FORMATION_CELLS == 4);      /* = `CE_OPTION_POSE_CELLS`（sdk_core.h，不改共享件）*/
        CHECK(signal_formation_wrap(0) == 0);
        CHECK(signal_formation_wrap(4) == 4);
        CHECK(signal_formation_wrap(5) == 0);    /* 一手的 `(n+1)%5` 循环 */
        CHECK(signal_formation_wrap(-1) == 4);   /* 负数安全 */
        CHECK(signal_formation_wrap(-6) == 4);
        for (f = 0; f < SIGNAL_FORMATION_COUNT; ++f) {
            CHECK(signal_formation_sht_set(f) == f);          /* 相对序号 0..4（卡侧 + CE_SHT_SET_MARISA_FORMATION_FIRE）*/
            for (i = 0; i < SIGNAL_FORMATION_CELLS; ++i) CHECK(signal_formation_cell(f, i, &c) == 1);
            CHECK(signal_formation_cell(f, SIGNAL_FORMATION_CELLS, &c) == 0);   /* 越界 */
        }
        CHECK(signal_formation_sht_set(5) == 0);
        CHECK(signal_formation_cell(-1, 0, &c) == 0);
        CHECK(signal_formation_cell(0, 0, 0) == 0);           /* NULL 安全 */
        /* 五档必须**互不相同**（否则「切档」在画面上看不出任何变化 = 假功能）*/
        {
            signal_cell_t a, b;
            int32_t g2, same;
            for (f = 0; f < SIGNAL_FORMATION_COUNT; ++f) {
                for (g2 = f + 1; g2 < SIGNAL_FORMATION_COUNT; ++g2) {
                    same = 0;
                    for (i = 0; i < SIGNAL_FORMATION_CELLS; ++i) {
                        (void)signal_formation_cell(f, i, &a);
                        (void)signal_formation_cell(g2, i, &b);
                        if (a.x10 == b.x10 && a.y10 == b.y10) ++same;
                    }
                    CHECK(same < SIGNAL_FORMATION_CELLS);      /* 至少一格不同 */
                }
            }
        }
    }

    /* --- 16.5 ★★ 2026-09-16：子机格数 = 原生火力档（用户口径 ②；一手 `th18:0x45d604` 的 lvl）---
     * 这是「火力随 P 完全依赖原生关系」的那一半落点：档 0 = **0 格**（不是 1 格）、档 N = N 格、上限 = 声明层上限。
     * 一手逐条：`0x45d604 mov eax,[0x4ccd38]` → `0x45d60a idiv [0x4ccd40]` → `0x45d85f test eax,eax; jle`
     * （0 档连循环都不进）→ 建 `lvl` 格，数组上限 4（`0x45d81e mov edi,4`）。*/
    {
        int32_t lvl;

        for (lvl = 0; lvl <= 4; ++lvl) {
            CHECK(signal_power_cells(lvl * 100, 100, 4) == lvl);        /* 一档 = 100 单位（`0x4ccd40`）*/
            CHECK(signal_power_cells(lvl * 100 + 99, 100, 4) == lvl);   /* 档内没跨过去 */
            CHECK(signal_power_cells(lvl * 40, 40, 4) == lvl);          /* 单位换了也一样（TH13 是 40）*/
        }
        /* 钳位与边界 */
        CHECK(signal_power_cells(-1, 100, 4) == 0);                     /* 负火力 ⇒ 0（不是负数格）*/
        CHECK(signal_power_cells(0, 100, 4) == 0);
        CHECK(signal_power_cells(999, 100, 4) == 4);                    /* 超过满档 ⇒ 钳到上限 */
        CHECK(signal_power_cells(400, 100, 4) == 4);
        CHECK(signal_power_cells(500, 100, 2) == 2);                    /* cells_max 自己也是钳位（声明层上限）*/
        CHECK(signal_power_cells(100, 0, 4) == 0);                      /* 除零守卫 */
        CHECK(signal_power_cells(100, -100, 4) == 0);
        CHECK(signal_power_cells(100, 100, 0) == 0);
    }

    /* --- 16.5 荷取：**河童罩子**（2026-09-16q 按一手重写：泡 920 帧、没被撞退**半档**）---
     * 一手出处全在 `cards/signal_core.h` 的 ⑥ 号块（`th11:0x407680`/`0x407720` 逐条 +
     * `pl01.anm` script58 原文）。这里钉五件事：
     *   ① 寿命 = 920 帧（脚本 `+900: delete()`），不是旧的 75（那是**破罩伤害源**的寿命）；
     *   ② 退款额度 = 半档（`BOMB_COST` 20 / 退款 10 ⇒ 扣 100 退 50）；
     *   ③ 判据 = 「泡的 VM 没了」⇒ `expire()`（正常路），`step()` 只是句柄丢了时的兜底；
     *   ④ 破罩（`break()`）⇒ **不退**（一手：破罩那条路没有 `collect_power`）；
     *   ⑤ 破罩消弹圈半径 = 32 + 10×(k-1)，k = 1..30（一手：先消弹、帧末才 `+= 10.0`）。*/
    {
        signal_nitori_t n;

        CHECK(SIGNAL_NITORI_SHIELD_FRAMES == 920);      /* 一手：script58 第 920 帧 delete() */
        CHECK(SIGNAL_NITORI_BREAK_DMG_LIFE == 75);      /* 一手 `th11:0x40790c: push 0x4b`（**伤害源**寿命）*/
        CHECK(SIGNAL_NITORI_BREAK_FRAMES == 30);        /* 一手 `cmp [bomb+0x2c],0x1e` */
        CHECK(SIGNAL_NITORI_BREAK_IFRAMES == 40);       /* 一手 `0x407961 push 0x28` */

        /* ② 额度：半档（一手 10 / 20）*/
        CHECK(signal_nitori_refund_of(100) == 50);
        CHECK(signal_nitori_refund_of(1) == 0);         /* 1 单位的一半 ⇒ 0（不四舍五入）*/
        CHECK(signal_nitori_refund_of(3) == 1);
        CHECK(signal_nitori_refund_of(0) == 0 && signal_nitori_refund_of(-5) == 0);

        memset(&n, 0, sizeof n);
        CHECK(signal_nitori_busy(&n) == 0);
        CHECK(signal_nitori_expire(&n) == 0);           /* 没开窗 ⇒ 什么都不发生 */
        CHECK(signal_nitori_expire(&n) == 0);
        signal_nitori_open(&n, 100);
        CHECK(signal_nitori_busy(&n) == 1 && n.pending == 50);

        /* ③ 正常路：泡的 VM 自己没了 ⇒ 结算（**不等 920 帧走完** —— 一手判据就是「VM 在不在」）*/
        CHECK(signal_nitori_expire(&n) == 50);
        CHECK(n.refunds == 1 && n.denials == 0 && n.pending == 0);
        CHECK(signal_nitori_busy(&n) == 0);
        CHECK(signal_nitori_expire(&n) == 0);           /* 幂等 */
        CHECK(n.refunds == 1);

        /* ④ 破罩 ⇒ 不退（并且把这一发记成 denial）*/
        signal_nitori_open(&n, 100);
        signal_nitori_break(&n);
        CHECK(n.broken == 1 && signal_nitori_busy(&n) == 1);
        CHECK(signal_nitori_expire(&n) == 0);
        CHECK(n.denials == 1 && n.refunds == 1);

        /* ③b 账本里**没有寿命**（2026-09-16u 订正）：同一发只结算一次 */
        signal_nitori_open(&n, 100);
        CHECK(n.active == 1 && n.pending == 50);
        CHECK(signal_nitori_expire(&n) == 50);
        CHECK(n.refunds == 2 && n.denials == 1);
        CHECK(signal_nitori_expire(&n) == 0 && n.active == 0);        /* 幂等 */

        /* 本来就没扣到火力（cost 0）⇒ 不退，但照样进 denial（诊断可分辨）*/
        signal_nitori_open(&n, 0);
        CHECK(n.pending == 0);
        CHECK(signal_nitori_expire(&n) == 0);
        CHECK(n.refunds == 2 && n.denials == 2);

        /* ⑤ 破罩消弹圈：32 → 322（+10/帧），越界钳位 */
        CHECK(signal_nitori_break_radius_px(1) == 32);
        CHECK(signal_nitori_break_radius_px(2) == 42);
        CHECK(signal_nitori_break_radius_px(30) == 322);
        CHECK(signal_nitori_break_radius_px(31) == 322);   /* 钳到 30 帧 */
        CHECK(signal_nitori_break_radius_px(0) == 32);
        CHECK(signal_nitori_break_radius_px(-9) == 32);
        {   /* 单调递增（每帧都往外长）*/
            int32_t k, ok = 1;
            for (k = 2; k <= SIGNAL_NITORI_BREAK_FRAMES; ++k)
                if (signal_nitori_break_radius_px(k) <= signal_nitori_break_radius_px(k - 1)) ok = 0;
            CHECK(ok);
        }

        /* 提前作废（换泡 / 删卡 / 换关）：不退款、也不进诊断计数 */
        signal_nitori_open(&n, 100);
        signal_nitori_close(&n);
        CHECK(signal_nitori_busy(&n) == 0 && n.pending == 0);
        CHECK(n.refunds == 2 && n.denials == 2);
        CHECK(signal_nitori_expire(&n) == 0);

        signal_nitori_open(0, 100);                     /* NULL 安全 */
        signal_nitori_break(0);
        signal_nitori_close(0);
        CHECK(signal_nitori_expire(0) == 0);
        CHECK(signal_nitori_refund_of(0) == 0);
   
        /* --- 16.6 ★★ 2026-09-16u：罩子的**时钟**与**可见曲线**（用户口径）---
         * 口径（逐字）：「**B 的无敌时间不算罩子的持续时间**，此时撞任何弹幕都应该认为是没撞；
         * 其他原因的无敌**依旧不算**在罩子的持续时间，但撞弹也不能就把罩子破了」。
         * 钉三件事：
         *   ① `clock_runs` 的三个输入各自都能单独把时钟停住（无敌 / 无伤声明 / 自机状态 2·3·4）；
         *   ② 只认**活着的状态 1**：0（未初始化）/5+ 之类不许被当成「类无敌」而白停（口径收窄）；
         *   ③ 曲线 = 一手那三条指令的等价式（两端精确）。
         */
        CHECK(signal_nitori_clock_runs(0, 0, 1) == 1);      /* 干净的一帧 ⇒ 推进 */
        CHECK(signal_nitori_clock_runs(40, 0, 1) == 0);     /* 无敌计时器 > 0（原生 B / 救援无敌 / 复活）*/
        CHECK(signal_nitori_clock_runs(0, 1, 1) == 0);      /* 无伤声明（113 的灵界闸门等）*/
        CHECK(signal_nitori_clock_runs(0, 0, 4) == 0);      /* 决死窗口 */
        CHECK(signal_nitori_clock_runs(0, 0, 2) == 0);      /* 死亡 */
        CHECK(signal_nitori_clock_runs(0, 0, 3) == 0);      /* 复活 */
        CHECK(signal_nitori_clock_runs(0, 0, 0) == 1);      /* 0（未初始化）不算类无敌 */
        {   int32_t ps, ok = 1;
            for (ps = -3; ps <= 8; ++ps)
                if (signal_nitori_clock_runs(0, 0, ps) != (ps == 2 || ps == 3 || ps == 4 ? 0 : 1)) ok = 0;
            CHECK(ok);
        }
        CHECK(SIGNAL_NITORI_FADE_IN_FRAMES == 10 && SIGNAL_NITORI_GROW_FRAMES == 20);
        CHECK(SIGNAL_NITORI_SHRINK_FRAMES == 900);
        CHECK(SIGNAL_NITORI_GROW_FRAMES + SIGNAL_NITORI_SHRINK_FRAMES == SIGNAL_NITORI_SHIELD_FRAMES);

        /* alpha：0..10 帧 0 → 255，其后恒 255 */
        CHECK(signal_nitori_bubble_alpha(0) == 0);
        CHECK(signal_nitori_bubble_alpha(5) == 127);
        CHECK(signal_nitori_bubble_alpha(10) == 255);
        CHECK(signal_nitori_bubble_alpha(919) == 255);
        CHECK(signal_nitori_bubble_alpha(-7) == 0);

        /* scale：0..20 帧 0 → 1.0；20..920 帧 1.0 → 0.1（一手那 900 帧缩到 0.1） */
        CHECK(signal_nitori_bubble_scale_milli(0) == 0);
        CHECK(signal_nitori_bubble_scale_milli(10) == 500);
        CHECK(signal_nitori_bubble_scale_milli(20) == 1000);
        CHECK(signal_nitori_bubble_scale_milli(21) == 999);   /* 1000 − (1000−100)×1/900 的整数式 */
        CHECK(signal_nitori_bubble_scale_milli(920) == 100);
        CHECK(signal_nitori_bubble_scale_milli(SIGNAL_NITORI_SHIELD_FRAMES + 500) == 100);   /* 越界也钳到终点 */
        CHECK(signal_nitori_bubble_scale_milli(-7) == 0);
        {   /* 两段各自单调：0..20 只涨、20..920 只落（一手是两条 `scaleTime`）*/
            int32_t a, ok = 1;
            int32_t prev = signal_nitori_bubble_scale_milli(1);
            for (a = 2; a <= SIGNAL_NITORI_GROW_FRAMES; ++a) {
                int32_t v = signal_nitori_bubble_scale_milli(a);
                if (v < prev) ok = 0;
                prev = v;
            }
            for (a = SIGNAL_NITORI_GROW_FRAMES + 1; a <= SIGNAL_NITORI_SHIELD_FRAMES; a += 7) {
                int32_t v = signal_nitori_bubble_scale_milli(a);   /* 20..920 只落 */
                if (v > prev) ok = 0;
                prev = v;
            }
            CHECK(ok);
        }
        /* --- 16.7 ★★ 文·超高速的残像上色（TH11 `pl00.anm` script19 的等价式）---
         * 一手：`color(255,64,64); colorTime(15, 0, 64,64,255); alphaTime(15,0,0); +15: delete();`
         * 用户口径：原版**更贴洋红**、而且有「时间渐变（= 不同残像的位置渐变）」。*/
        CHECK(SIGNAL_TRAIL_RAMP_FRAMES == 15);
        CHECK(signal_trail_rgb(0) == 0xff4040);         /* 起手 (255,64,64) */
        CHECK(signal_trail_rgb(15) == 0x4040ff);        /* 15 帧后 (64,64,255) */
        CHECK(signal_trail_rgb(-4) == 0xff4040);        /* 负数钳到起点 */
        CHECK(signal_trail_rgb(99) == 0x4040ff);        /* 越界钳到终点 */
        {   /* 中段应当「更贴洋红」：R 与 B 都明显高于 G，且两端之间单调过渡 */
            int32_t rgb = signal_trail_rgb(7);
            int32_t r = (rgb >> 16) & 0xff, g = (rgb >> 8) & 0xff, b = rgb & 0xff;
            CHECK(g == 64);
            CHECK(r > 64 && r < 255 && b > 64 && b < 255);
            CHECK(r - g > 60 && b - g > 60);            /* 洋红感 = 红蓝高、绿低 */
            {   int32_t a, ok = 1, prevr = 256, prevb = -1;
                for (a = 0; a <= SIGNAL_TRAIL_RAMP_FRAMES; ++a) {
                    int32_t v = signal_trail_rgb(a);
                    int32_t rr = (v >> 16) & 0xff, bb = v & 0xff;
                    if (rr > prevr || bb < prevb) ok = 0;   /* R 只降、B 只升 */
                    prevr = rr; prevb = bb;
                }
                CHECK(ok);
            }
        }

        /* 「换泡 = 旧泡按自己的记录结算」用的就是 expire()：破罩的旧泡不退、没破的退一半，
         * 而且**都不碰 `breaks`**（卡侧的「被撞次数」）—— 这才是「自动破掉不影响 no miss 判定」。 */
        CHECK(signal_nitori_expire(&n) == 0);
    }

    /* ============ ★★ P 点火力 +0.05 档（111 侧；用户 2026-09-15）============
     * 完整算式与穷举在 `test_mof_core.c`（那边的 `sim_p_commit()` 把两张卡的接线一起模型了）。
     * 这里只钉 **111 自己的那一半**：让位判据 —— 与 `cards/signal.c` 的
     * `signal_on_item_commit()` 里那一行 `mof_core_p_owner(ce_owned(110), 1) != 111` 同形。 */
    CHECK(mof_core_p_owner(1, 1) == 110);      /* 110 在场 ⇒ 111 **不落笔**（两张同装只算一次）*/
    CHECK(mof_core_p_owner(0, 1) == 111);      /* 110 不在 ⇒ 111 落笔（「110 **或** 111」即生效）。
                                                * ⚠ 入参是 `ce_owned(110)` 与**自己的 1**：111 的在场
                                                * 由「它的回调正在跑」保证，不靠 `ce_owned(111)`。*/
    {
        /* 111 侧补差与 110 侧逐值相同（同一份纯逻辑、同一组输入）—— 「任一张在场即生效」的实质。 */
        mof_p_topup_t a = mof_core_p_topup(101, 400);
        CHECK(a.delta == MOF_P_TOPUP_UNITS);
        CHECK(101 + a.delta == 105);                   /* = 1.05 档（原生 1.01 + 补 0.04）*/
        CHECK(mof_core_p_topup(400, 400).delta == 0);  /* 满火力不越上限（111 侧同一条）*/
    }

    /* ============ ★★ 满 P 转换 / 档位 ↔ 来源（111 侧；E-1 硬约束，2026-09-15 收口）============
     * 主路 = 共用件 `faith_convert_full_p_items()`（`faith.c`；110 与 111 是**同一台机器**，
     * `cards/signal.c` 的 `on_tick_2` 里每帧调，放在 `faith_pool_tick()` 之前）⇒ 111 侧钉两件事：
     *   ① 判据用 **TH18 类型版**的 `mof_core_full_p_replace()`（满 P 且 type ∈ {小 P(1), 大 P(3)}）
     *      —— 旧实现只认 type 1，**漏了 TH18 的大 P**；
     *   ② 档位走 `faith_core_tier_for_source(FAITH_SRC_FULL_P)` = **type 9 档**（中绿点）——
     *      111 的中绿点值 `SIGNAL_GREEN_MID` = 10 就挂在这一档上，不许串成大/小档。*/
    CHECK(SIGNAL_GREEN_BIG == FAITH_SA_BIG);     /* +100 = th11 type 3（大绿点 / 击破掉落）*/
    CHECK(SIGNAL_GREEN_MID == FAITH_SA_MID);     /* +10  = th11 type 9（中绿点 / 满 P 转换）*/
    CHECK(faith_core_tier_for_source(FAITH_SRC_FULL_P) == FAITH_TIER_SMALL);  /* 中绿点档 */
    CHECK(faith_core_tier_for_source(FAITH_SRC_BREAK)  == FAITH_TIER_BIG);    /* 大绿点档 */
    CHECK(faith_core_tier_for_source(FAITH_SRC_CANCEL) == FAITH_TIER_MICRO);  /* 小绿点档（消弹）*/
    CHECK(mof_core_full_p_replace(FAITH_TYPE18_P_SMALL, 400, 400) == 1);
    CHECK(mof_core_full_p_replace(FAITH_TYPE18_P_BIG,   400, 400) == 1);      /* ★ TH18 大 P 也转 */
    CHECK(mof_core_full_p_replace(2, 400, 400) == 0);                         /* 钱不转 */
    CHECK(mof_core_full_p_replace(FAITH_TYPE18_P_SMALL, 399, 400) == 0);      /* 不满 P 不转 */

    /* ============ ★★ 2026-09-15c「灵击 → 决死」（决死 B 形态；111 侧）============
     * 一手链（TH11 v1.00a，md5 `6af1e3b37f28293f4fc127ee01d2632c`）见 `signal_core.h` 的
     * 「灵击 → 决死」块。这一块钉四件事：
     *   ① **窗口判据** = `player_state == 4`（一手 `th18:0x45d4e4`：全 exe 唯一写 4 的点）；
     *   ② **消弹照常记账**（✅ 一手：TH11 的决死与普通灵击「代价完全相同、两者都撒 type 8」）
     *      ⇒ 两种形态的信仰账**逐弹数完全一致**（差 0）；
     *   ③ **决死不扣信仰是结构性的**（111 的信仰惩罚只走 miss 的 `+0x0c`；救下来 ⇒ 窗口不耗尽
     *      ⇒ 引擎不广播它）⇒ 模型里两种形态都没有信仰扣除（一手同向：TH11 决死不触发 `$MISS_COUNT++`）；
     *   ④ **不开荷取退款窗口**（❓ 自定：那条特技的前提是「没被打中」，决死的前提是「刚被打中」）。*/
    {
        signal_bomb_form_t f;
        faith_slot_t        a, b;
        int32_t             bullets, ok;

        /* ① 窗口判据（与 110 同一批一手事实，两边各自实现、各自单测）*/
        CHECK(SIGNAL_DEATHBOMB_STATE == 4);
        CHECK(signal_core_deathbomb_window(SIGNAL_DEATHBOMB_STATE) == 1);
        for (int32_t ps = -2; ps <= 10; ++ps)
            if (ps != SIGNAL_DEATHBOMB_STATE) CHECK(signal_core_deathbomb_window(ps) == 0);

        /* ② 形态字段（二值；野值当主动）*/
        signal_bomb_form_start(&f, SIGNAL_BOMB_BY_PRESS);
        CHECK(signal_bomb_form_is_deathbomb(&f) == 0);
        CHECK(f.cancels == 0);
        signal_bomb_form_start(&f, SIGNAL_BOMB_BY_HIT);
        CHECK(signal_bomb_form_is_deathbomb(&f) == 1);
        CHECK(f.cancels == 0);
        signal_bomb_form_start(&f, 99);
        CHECK(signal_bomb_form_is_deathbomb(&f) == 0);

        /* ③ 消弹记账：**一手 = 照常**（当前口径钉死；开关若被改，这一行会先红）*/
        CHECK(SIGNAL_BOMB_BY_HIT_ACCOUNT == 1);                       /* ✅ 一手：不跳过 */
        CHECK(signal_bomb_form_cancel_accounts(&f) == 1);             /* 主动 = 照常 */
        signal_bomb_form_start(&f, SIGNAL_BOMB_BY_HIT);
        CHECK(signal_bomb_form_cancel_accounts(&f) == 1);             /* ★ 决死也照常（一手）*/

        /* ④ 两种形态逐弹数跑一遍：信仰账**完全一致**（差恒 0）*/
        ok = 1;
        for (bullets = 0; bullets <= 300; bullets += 7) {
            faith_slot_init_111(&a, FAITH_DIFF_NORMAL);
            faith_slot_init_111(&b, FAITH_DIFF_NORMAL);
            if (sim_signal_bomb(&a, &f, SIGNAL_BOMB_BY_PRESS, bullets) !=
                sim_signal_bomb(&b, &f, SIGNAL_BOMB_BY_HIT,   bullets)) ok = 0;
        }
        CHECK(ok);
        /* 具体值：250 颗 = 每 100 个 +10 ⇒ +20（余 50 颗留在槽里跨帧/跨关）*/
        faith_slot_init_111(&a, FAITH_DIFF_NORMAL);
        CHECK(sim_signal_bomb(&a, &f, SIGNAL_BOMB_BY_HIT, 250) == 50000 + 20);
        CHECK(f.cancels == 250);                                      /* 颗数照记（诊断）*/

        /* ⑤ 荷取退款窗口：主动开、决死不开（❓ 自定）*/
        signal_bomb_form_start(&f, SIGNAL_BOMB_BY_PRESS);
        CHECK(signal_bomb_form_opens_refund(&f) == 1);
        signal_bomb_form_start(&f, SIGNAL_BOMB_BY_HIT);
        CHECK(signal_bomb_form_opens_refund(&f) == (SIGNAL_BOMB_BY_HIT_REFUND ? 1 : 0));

        /* ⑥ NULL 安全 + 诊断计数 */
        signal_bomb_form_start(0, SIGNAL_BOMB_BY_HIT);
        signal_bomb_form_cancel_note(0, 10);
        CHECK(signal_bomb_form_is_deathbomb(0) == 0);
        CHECK(signal_bomb_form_cancel_accounts(0) == 1);
        CHECK(signal_bomb_form_opens_refund(0) == 1);
        signal_bomb_form_start(&f, SIGNAL_BOMB_BY_HIT);
        signal_bomb_form_cancel_note(&f, 60);
        signal_bomb_form_cancel_note(&f, 40);
        signal_bomb_form_cancel_note(&f, -3);                         /* 负数不动 */
        CHECK(f.cancels == 100);
    }

    /* ============ 16.9 ★★ 2026-09-16za 106 的协同效应：「**除非满 P，不加信号强度**」============
     * 用户逐字：「协同效应：除非满P，不加信号强度」；选定口径 = **只停「线上补满」那一条**
     * （`faith_signal_step_gated(..., allow_line_topup = 0)`）：
     *   · 未满 + 线上 ⇒ **不再**给「+50 补满到 100.00」；
     *   · 已满 + 线上 ⇒ 档位小额加值（+0.16/+0.12/+0.09）**照给**；
     *   · 线下 ⇒ −0.30 衰减**照给**；HOLD ⇒ 0 **照旧**；
     *   · 擦弹是独立通道 ⇒ **照给**。
     * `faith_signal_step(...)`（旧签名）= `..._gated(..., 1)` ⇒ 既有调用方与单测零变化。*/
    {
        const int32_t online = FAITH_SIGNAL_BAND_TOP;
        /* 未满 + 线上：旧行为 **+50.00 并钳到 100.00**（从 0 出发一帧只到 50.00）、带闸则 0 */
        CHECK(faith_signal_step(0, online, 0) == FAITH_SIGNAL_GAIN_LINE);
        CHECK(faith_signal_step_gated(0, online, 0, 1) == FAITH_SIGNAL_GAIN_LINE);
        CHECK(faith_signal_step(FAITH_SIGNAL_HI_MARK - 100, online, 0) == 100);   /* 快满 ⇒ 只补差值 */
        CHECK(faith_signal_step_gated(0, online, 0, 0) == 0);
        CHECK(faith_signal_step_gated(FAITH_SIGNAL_HI_MARK - 100, online, 0, 0) == 0);
        /* 已满 + 线上：三档小额加值照给（带不带闸一样）*/
        for (int b = FAITH_SIGNAL_BAND_TOP; b <= FAITH_SIGNAL_BAND_LOW; ++b) {
            if (b == FAITH_SIGNAL_BAND_OFFLINE || b == FAITH_SIGNAL_BAND_HOLD) continue;
            CHECK(faith_signal_step_gated(FAITH_SIGNAL_HI_MARK, b, 0, 0) ==
                  faith_signal_step_gated(FAITH_SIGNAL_HI_MARK, b, 0, 1));
        }
        /* 线下衰减 / HOLD：带闸也一样 */
        CHECK(faith_signal_step_gated(FAITH_SIGNAL_HI_MARK, FAITH_SIGNAL_BAND_OFFLINE, 0, 0) ==
              -FAITH_SIGNAL_LOSS_OFFLINE);
        CHECK(faith_signal_step_gated(FAITH_SIGNAL_HI_MARK, FAITH_SIGNAL_BAND_HOLD, 0, 0) == 0);
        /* 擦弹独立通道：带闸照给（线上未满时也照给）*/
        CHECK(faith_signal_step_gated(0, online, 123, 0) == 123);
        CHECK(faith_signal_step_gated(FAITH_SIGNAL_HI_MARK, online, 123, 0) ==
              faith_signal_step_gated(FAITH_SIGNAL_HI_MARK, online, 123, 1));
        /* 卡侧接线：`signal_core_step_mod()` 的 `no_line_topup` 位（`m == NULL` ⇒ 不带闸）*/
        {
            signal_step_mod_t m;
            __builtin_memset(&m, 0, sizeof m);
            CHECK(signal_core_step_mod(0, online, 0, 0, 0) == FAITH_SIGNAL_GAIN_LINE);
            CHECK(signal_core_step_mod(0, online, 0, 0, &m) == FAITH_SIGNAL_GAIN_LINE);
            m.no_line_topup = 1;
            CHECK(signal_core_step_mod(0, online, 0, 0, &m) == 0);          /* ★ 闸开 ⇒ 不补满 */
            CHECK(signal_core_step_mod(0, online, 5, 0, &m) == 5 * signal_core_graze_gain(0, 0)); /* 擦弹照给 */
        }
    }

    /* ============ 17. ★★ 2026-09-15e 命碎**段机**（用户口径「每两次这样掉 1 个命碎」）============
     * 判据 / 粒度 / 近似 / 一手出处全在 `cards/signal_core.h` 的「命碎段机」段。这里逐条钉：
     *   ① **满 `SIGNAL_SEG_DROP_EVERY`（= 2）才出账**，且**余数跨段保留**（满 2 清零）；
     *   ② **本段死过 ⇒ 不计**（与「灵击/决死救回 ⇒ 命数不减 ⇒ 算没死」同一条口径）；
     *   ③ 三个边界信号（相位指纹 / 符卡位 / boss 消失）都能切段，同一次切换的**多个边沿只算一段**；
     *   ④ boss 不在场（妖精波）**不计段**；
     *   ⑤ 死亡归属 = 「本帧处理完仍开着的那个段」（边界帧的死亡算**新段**）；
     *   ⑥ `signal_seg_rebase()` 换关只重挂记忆（**余数保留**）、`signal_seg_init()` 整台归零。
     * ⚠ 「**改成一手 1:1 只要把 `SIGNAL_SEG_DROP_EVERY` 改成 1**」—— 那时本段里所有
     *   `pending == 1` 的断言会先红（它们就是「2 段 1 颗」这条用户口径的钉子）。*/
    {
        signal_seg_t    g;
        int32_t         drop, drops;
        signal_seg_in_t in;

        /* —— 帧推进的小工具：一次 tick + 累计「撒了几颗」—— */
        #define SEG_STEP(BOSS, SPELL, SIG, DIED)                                            \
            do {                                                                            \
                in.boss_present = (BOSS); in.spell_active = (SPELL); in.phase_sig = (SIG);   \
                in.dialogue = 0;                                                             \
                drop = 0;                                                                    \
                (void)signal_seg_tick(&g, &in, (DIED), &drop);                               \
                if (drop) ++drops;                                                           \
            } while (0)
        /* 边沿合并窗口 = `SIGNAL_SEG_DEBOUNCE` 帧 ⇒ 两次切换之间至少隔这么多「安静帧」。*/
        /* ★ 2026-09-16m：段要活够 `SIGNAL_SEG_MIN_FRAMES` 才记账 ⇒ 这里的「安静帧」
         *   必须**长于**它（顺便仍然 ≥ 去抖窗口），否则每一段都会被新的「段太短」门吞掉。*/
        #define SEG_QUIET(BOSS, SPELL, SIG)                                                 \
            do { int i_; int nq_ = SIGNAL_SEG_MIN_FRAMES + 10;                              \
                 if (nq_ < SIGNAL_SEG_DEBOUNCE + 1) nq_ = SIGNAL_SEG_DEBOUNCE + 1;          \
                 for (i_ = 0; i_ < nq_; ++i_) SEG_STEP(BOSS, SPELL, SIG, 0); } while (0)

        CHECK(SIGNAL_SEG_DEBOUNCE > 0);
        CHECK(SIGNAL_SEG_EV_START == 1 && SIGNAL_SEG_EV_END == 2);

        /* --- ① 两段没死过 ⇒ 1 颗；第 1 段收尾时只有余数 --- */
        signal_seg_init(&g);
        drops = 0;
        SEG_STEP(1, 0, 1001, 0);                 /* boss 出现 ⇒ 开第 1 段 */
        CHECK((drop ? 1 : 0) == 0);
        CHECK(g.open == 1 && g.segs == 0);
        SEG_QUIET(1, 0, 1001);                   /* 段内安静帧（★ 活够最短段长）*/
        SEG_STEP(1, 0, 1002, 0);                 /* 相位换 ⇒ 第 1 段收尾（没死）*/
        CHECK(g.segs == 1 && g.segs_clean == 1);
        CHECK(g.pending == 1 && drops == 0);     /* ★ 余数：还差一段 */
        CHECK(g.open == 1);                      /* 新段已经开着 */
        SEG_QUIET(1, 0, 1002);
        SEG_STEP(1, 1, 1003, 0);                 /* 符卡段开始 ⇒ 第 2 段收尾 ⇒ **满 2 出账** */
        CHECK(drops == 1 && g.drops == 1);
        CHECK(g.segs == 2 && g.segs_clean == 2);
        CHECK(g.pending == 0);                   /* ★ 满 2 清零 */
        CHECK(g.open == 1 && g.is_spell == 1);   /* 当前这段是符卡段 */

        /* --- ② 余数跨段保留：再攒 3 段没死 ⇒ 再出 1 颗，且余 1 --- */
        SEG_QUIET(1, 1, 1003);
        SEG_STEP(1, 0, 1004, 0);                 /* 第 3 段收尾 ⇒ pending 1 */
        CHECK(g.pending == 1 && drops == 1);
        SEG_QUIET(1, 0, 1004);
        SEG_STEP(1, 0, 1005, 0);                 /* 第 4 段收尾 ⇒ pending 2 ⇒ 出账 */
        CHECK(drops == 2 && g.pending == 0);
        SEG_QUIET(1, 0, 1005);
        SEG_STEP(1, 0, 1006, 0);                 /* 第 5 段收尾 ⇒ pending 1（跨段保留）*/
        CHECK(g.pending == 1 && drops == 2);
        CHECK(g.segs == 5 && g.segs_clean == 5);
        CHECK(g.drops == 2);                     /* 5 段 ⇒ 2 颗（2:1 的余数行为）*/

        /* --- ③ 本段死过 ⇒ **不计**（段数照算，`pending` 不动）--- */
        {
            int32_t before_pending = g.pending, before_drops = drops;
            SEG_QUIET(1, 0, 1006);               /* 收尾后新段已开着（sig 仍是 1006） */
            SEG_STEP(1, 0, 1006, 1);             /* ★ 这一段里死了一次 */
            CHECK(g.open == 1 && g.died == 1);
            SEG_QUIET(1, 0, 1006);
            SEG_STEP(1, 0, 1007, 0);             /* 相位换 ⇒ 收尾：死过 ⇒ 不攒 */
            CHECK(g.segs == 6 && g.segs_clean == 5);
            CHECK(g.pending == before_pending && drops == before_drops);
            CHECK(g.died == 0);                  /* 新段从「没死」重新起算 */
        }

        /* --- ④ 死亡归属：**边界那一帧**的死亡算**新段**的（旧段已按「没死」结算）--- */
        {
            int32_t clean_before = g.segs_clean;
            SEG_QUIET(1, 0, 1007);
            SEG_STEP(1, 0, 1008, 1);             /* 相位换 + 同帧死亡 ⇒ 旧段 clean、新段 died */
            CHECK(g.segs_clean == clean_before + 1);
            CHECK(g.died == 1 && g.open == 1);
        }

        /* --- ⑤ 同一次切换的多个边沿**只算一段**（去抖）：相位换 + 符卡位翻转连着来 --- */
        {
            signal_seg_t a, b;
            int32_t      da, db, i;
            signal_seg_in_t ia, ib;

            signal_seg_init(&a);
            signal_seg_init(&b);
            da = db = 0;
            /* a：一次切换里「相位换」与「符卡位起」**同一帧**；b：相位换之后再隔 2 帧才是符卡位。*/
            for (i = 0; i < SIGNAL_SEG_MIN_FRAMES + 2; ++i) {   /* ★ 段要活够最短段长才算账 */
                ia.boss_present = 1; ia.spell_active = 0; ia.phase_sig = 2001; ia.dialogue = 0;
                (void)signal_seg_tick(&a, &ia, 0, &da);
                ib.boss_present = 1; ib.spell_active = 0; ib.phase_sig = 2001; ib.dialogue = 0;
                (void)signal_seg_tick(&b, &ib, 0, &db);
            }
            ia.boss_present = 1; ia.spell_active = 1; ia.phase_sig = 2002; ia.dialogue = 0;   /* 两个边沿同帧 */
            (void)signal_seg_tick(&a, &ia, 0, &da);
            ib.boss_present = 1; ib.spell_active = 0; ib.phase_sig = 2002; ib.dialogue = 0;   /* 只有相位换 */
            (void)signal_seg_tick(&b, &ib, 0, &db);
            ib.boss_present = 1; ib.spell_active = 1; ib.phase_sig = 2002; ib.dialogue = 0;   /* 2 帧后符卡位起 */
            (void)signal_seg_tick(&b, &ib, 0, &db);
            CHECK(a.segs == 1 && b.segs == 1);   /* ★ 都只收了 1 段（不是 2 段）*/
            CHECK(a.pending == 1 && b.pending == 1);
        }

        /* --- ★★ 2026-09-16：**对话中只吸收、不结算**（用户实跑：「一面结束对话后进关底直接就掉了
         *   一个残碎片」）。场景：关底 boss 被击破（段收尾）→ 对话（dlg=1）→ boss 对象被释放。
         *   修之前：boss 1→0 会被算成又一次段收尾 ⇒ pending 从 1 变 2 ⇒ 撒一颗 ✗。--- */
        {
            signal_seg_t d;
            int32_t      dd = 0, i;
            signal_seg_in_t id;

            signal_seg_init(&d);
            /* 第 1 段：boss 在场一段 → 相位换 ⇒ 收尾（没死）⇒ pending = 1 */
            for (i = 0; i < SIGNAL_SEG_MIN_FRAMES + 2; ++i) {   /* ★ 第 1 段活够最短段长 */
                id.boss_present = 1; id.spell_active = 0; id.phase_sig = 4001; id.dialogue = 0;
                (void)signal_seg_tick(&d, &id, 0, &dd);
            }
            id.boss_present = 1; id.spell_active = 0; id.phase_sig = 4002; id.dialogue = 0;
            (void)signal_seg_tick(&d, &id, 0, &dd);
            CHECK(d.segs == 1 && d.pending == 1 && dd == 0);
            /* 对话开始（这一帧 boss 还在）→ 对话期间 boss 被释放、相位槽清空、符卡位翻转 */
            for (i = 0; i < 60; ++i) {
                id.boss_present = (i < 10) ? 1 : 0;
                id.spell_active = 0; id.phase_sig = (i < 10) ? 4002 : 0; id.dialogue = 1;
                (void)signal_seg_tick(&d, &id, 0, &dd);
            }
            CHECK(d.segs == 1);                  /* ★ 对话里**一段都没多算** */
            CHECK(d.pending == 1 && dd == 0);    /* ★ 也没撒 */
            CHECK(d.open == 0);
            /* 对话结束后 boss 已不在 ⇒ 不补边沿、不补段 */
            for (i = 0; i < SIGNAL_SEG_DEBOUNCE + 2; ++i) {
                id.boss_present = 0; id.spell_active = 0; id.phase_sig = 0; id.dialogue = 0;
                (void)signal_seg_tick(&d, &id, 0, &dd);
            }
            CHECK(d.segs == 1 && d.pending == 1 && dd == 0);
        }

        /* --- ★★ 2026-09-16m：**太短的段不算一段**（用户第二次实跑「依旧对话结束后发残碎」）。
         *   场景：有一段真相位（结算 ⇒ pending 1）→ 对话 → **对话刚结束**时 boss/相位重新出现，
         *   机器按设计开一段，可几帧内又出现一次边沿（新 boss 登记相位槽）⇒ 这个「几帧的段」
         *   原来会被当成一整段结算、而且必然「没死过」⇒ pending 2 ⇒ 白撒一颗 ✗。
         *   修法 = 段长 < `SIGNAL_SEG_MIN_FRAMES` 的只记诊断不记账。--- */
        {
            signal_seg_t q;
            int32_t      dq = 0, i;
            signal_seg_in_t iq;

            signal_seg_init(&q);
            /* 第 1 段：活够 ⇒ pending 1（用「boss 消失」这条边沿收尾）*/
            for (i = 0; i < SIGNAL_SEG_MIN_FRAMES + 2; ++i) {
                iq.boss_present = 1; iq.spell_active = 0; iq.phase_sig = 5001; iq.dialogue = 0;
                (void)signal_seg_tick(&q, &iq, 0, &dq);
            }
            iq.boss_present = 0; iq.spell_active = 0; iq.phase_sig = 0; iq.dialogue = 0;
            (void)signal_seg_tick(&q, &iq, 0, &dq);
            CHECK(q.segs == 1 && q.segs_clean == 1 && q.pending == 1 && dq == 0);
            CHECK(q.last_len >= SIGNAL_SEG_MIN_FRAMES);
            /* 对话（只吸收）*/
            for (i = 0; i < 40; ++i) {
                iq.boss_present = 0; iq.spell_active = 0; iq.phase_sig = 0; iq.dialogue = 1;
                (void)signal_seg_tick(&q, &iq, 0, &dq);
            }
            CHECK(q.open == 0 && dq == 0);
            /* 对话刚结束：boss 回来（开段；这一帧的「指纹变了」边沿被去抖窗口吃掉）
             * → 等去抖过完 → 相位指纹再一变（收尾）⇒ **假段（十几帧），不许记账** */
            for (i = 0; i < SIGNAL_SEG_DEBOUNCE + 2; ++i) {
                iq.boss_present = 1; iq.spell_active = 0; iq.phase_sig = 5002; iq.dialogue = 0;
                (void)signal_seg_tick(&q, &iq, 0, &dq);
            }
            CHECK(q.open == 1);
            iq.boss_present = 1; iq.spell_active = 0; iq.phase_sig = 5003; iq.dialogue = 0;
            (void)signal_seg_tick(&q, &iq, 0, &dq);
            CHECK(q.segs == 2 && q.open == 1);   /* 段数照记（诊断口径）+ 新段已经开着 */
            CHECK(q.segs_clean == 1);            /* ★ 但**没死过**那本账没动 */
            CHECK(q.pending == 1 && dq == 0);    /* ★ 也没撒 */
            CHECK(q.last_len < SIGNAL_SEG_MIN_FRAMES);
        }

        /* --- ⑥ boss 不在场 ⇒ 不计段；boss 消失 ⇒ 最后一段照收 --- */
        {
            signal_seg_t h;
            int32_t      dh = 0, i;
            signal_seg_in_t ih;

            signal_seg_init(&h);
            for (i = 0; i < 10; ++i) { ih.boss_present = 0; ih.spell_active = 0; ih.phase_sig = 0; ih.dialogue = 0;
                                       (void)signal_seg_tick(&h, &ih, 1, &dh); }   /* 妖精波里的死亡 */
            CHECK(h.open == 0 && h.segs == 0 && dh == 0);                      /* 一段都没开 */
            ih.boss_present = 1; ih.spell_active = 0; ih.phase_sig = 3001; ih.dialogue = 0;
            (void)signal_seg_tick(&h, &ih, 0, &dh);
            CHECK(h.open == 1);
            for (i = 0; i < SIGNAL_SEG_MIN_FRAMES + 2; ++i) {                   /* ★ 活够最短段长 */
                ih.boss_present = 1; ih.spell_active = 0; ih.phase_sig = 3001; ih.dialogue = 0;
                (void)signal_seg_tick(&h, &ih, 0, &dh);
            }
            ih.boss_present = 0; ih.spell_active = 0; ih.phase_sig = 0;        /* boss 被打死 */
            (void)signal_seg_tick(&h, &ih, 0, &dh);
            CHECK(h.segs == 1 && h.segs_clean == 1 && h.pending == 1);
            CHECK(h.open == 0);                                                /* 不再开新段 */
        }

        /* --- ⑦ `rebase`（换关）保留余数 / `init`（局末）整台归零 --- */
        signal_seg_init(&g);
        drops = 0;
        SEG_STEP(1, 0, 4001, 0);
        SEG_QUIET(1, 0, 4001);                   /* ★ 活够最短段长 */
        SEG_STEP(1, 0, 4002, 0);                 /* ⇒ pending 1 */
        CHECK(g.pending == 1 && g.segs == 1);
        signal_seg_rebase(&g);                   /* 换关 */
        CHECK(g.pending == 1 && g.segs == 1 && g.open == 0 && g.last_sig == 0);
        SEG_STEP(1, 0, 5001, 0);                 /* 新关第一段：不会因为「旧指纹」误报收尾 */
        CHECK(g.segs == 1 && g.open == 1);
        SEG_QUIET(1, 0, 5001);                   /* ★ 活够最短段长 */
        SEG_STEP(1, 0, 5002, 0);                 /* ⇒ pending 2 ⇒ 出账（余数真的跨了关）*/
        CHECK(drops == 1 && g.pending == 0);
        signal_seg_init(&g);
        CHECK(g.pending == 0 && g.segs == 0 && g.drops == 0 && g.open == 0);

        /* --- ⑧ NULL 安全 --- */
        drop = 7;
        CHECK(signal_seg_tick(0, &in, 0, &drop) == SIGNAL_SEG_EV_NONE && drop == 0);
        CHECK(signal_seg_tick(&g, 0, 0, &drop) == SIGNAL_SEG_EV_NONE && drop == 0);
        in.boss_present = 1; in.spell_active = 0; in.phase_sig = 6001;
        CHECK(signal_seg_tick(&g, &in, 0, 0) == SIGNAL_SEG_EV_START);   /* out_drop = NULL 也安全 */
        signal_seg_init(0);
        signal_seg_rebase(0);

        #undef SEG_STEP
        #undef SEG_QUIET
    }

    /* ============ 18. ★★ 爱丽丝（魔理沙×爱丽丝）在**本文件**里的那一半 ============
     * 口径与一手核对（`pl01a.sht` 的 `+0x24`=12 / `+0x20`=8 ⇒ 一发占满槽 **1/8**、其余五机 20/4 ⇒ **1/4**
     * ⇒ **恰好一半**；「绝对数 0.50」不在表里）全在 `cards/signal_core.h` 的 ④ 段。这里只钉
     * **111 私有**的那一条（灵击），钉三件事：
     *   ① **一手登记值**与两条口径常量自洽（`SIGNAL_ALICE_BOMB_COST_TH11` = 12；50 单位 = 半个常态价）；
     *   ② 门槛 / 下限 / 每次扣款：**恒扣 50 单位**、`power ≥ 155`（= 50 + 下限 105）才放得出、
     *      扣完**永不越 1.05 档**、`power ∈ [50,105]` 一律拒绝（否则会被下限反向抬起来 = 白嫖灵击）；
     *   ③ `alice_on == 0` ⇒ **逐值等于共用件** `mof_core_power_spend()` ⇒ 「只在爱丽丝生效时改变」可证。
     * ★★ **P 点那一条（+0.02/+0.03 交替）2026-09-15h 已搬进共享件**（`faith_core.c` / `faith.c`）——
     *   因为它的落笔者恒为 110（`mof_core_p_owner()` 选举），相位住在 111 的卡状态里两边够不到。
     *   ⇒ 它的单测在 `tests/test_faith_core.c`（纯逻辑：序列 / 平均值 / 只在不生效时不推进）与
     *   `tests/test_mof_core.c`（**接线模型**：同一颗道具只推进一次相位 + 总量→补值）。
     *   ⛔ 本文件**不许**再出现第二份相位（`signal_alice_t` 已退役，见 `signal_core.h`）。*/
    {
        int32_t p;
        int     bad;
        mof_power_t pa, pb;

        /* --- ① 一手登记值 + 两条口径常量自洽 --- */
        CHECK(SIGNAL_ALICE_BOMB_COST_TH11 == 12);                 /* ✅ 一手 pl01a.sht +0x24（登记）*/
        CHECK(SIGNAL_ALICE_BOMB_COST_UNITS == 50);                /* ❓ 映射：1 档的一半 */
        CHECK(SIGNAL_ALICE_BOMB_COST_UNITS * 2 == FAITH_POWER_COST);      /* = 半个常态价（1 档）*/

        /* --- ② 灵击：扣款恒 50、门槛「能扣就扣」= ≥1.55 档、下限 1.05 照旧 --- */
        pa = signal_core_alice_power_spend(400, 1);
        CHECK(pa.ok == 1 && pa.new_power == 350);                 /* 4.00 档 → 3.50 档 */
        pa = signal_core_alice_power_spend(200, 1);
        CHECK(pa.ok == 1 && pa.new_power == 150);                 /* ★ 常态在 2.00 档只扣到 1.05（95 单位）；
                                                                   *   爱丽丝扣满 50 单位 */
        pa = signal_core_alice_power_spend(155, 1);               /* 恰好 1.55 档 ⇒ 放得出 */
        CHECK(pa.ok == 1 && pa.new_power == FAITH_POWER_FLOOR);   /* 落到下限 1.05，**正好**不越 */
        pa = signal_core_alice_power_spend(154, 1);               /* 1.54 档 ⇒ 放不出（下限吃不下）*/
        CHECK(pa.ok == 0 && pa.new_power == 154);                 /* 失败时火力**一个字节都不动** */
        CHECK(signal_core_alice_power_spend(105, 1).ok == 0);
        CHECK(signal_core_alice_power_spend(50, 1).ok == 0);      /* ★ 「能扣就扣」**不许**白嫖：
                                                                   *   50 单位时会扣成 0 再被下限抬回来 ⇒ 拒绝 */
        CHECK(signal_core_alice_power_spend(0, 1).ok == 0);
        bad = 0;
        for (p = 155; p <= 400; ++p) {                             /* 全域：恒扣 50、永不越下限 */
            pa = signal_core_alice_power_spend(p, 1);
            if (!pa.ok || p - pa.new_power != SIGNAL_ALICE_BOMB_COST_UNITS
                || pa.new_power < FAITH_POWER_FLOOR) bad = 1;
        }
        CHECK(bad == 0);

        /* --- ③ `alice_on == 0` ⇒ 逐值等于共用件（「只在爱丽丝生效时改变」） --- */
        bad = 0;
        for (p = 0; p <= 500; ++p) {
            pa = signal_core_alice_power_spend(p, 0);
            pb = mof_core_power_spend(p);
            if (pa.ok != pb.ok || pa.new_power != pb.new_power) bad = 1;
        }
        CHECK(bad == 0);
    }

    /* ==================== 17. ★★ 咲夜 / 早苗 的六条支援特技（2026-09-15i 本轮实装）====================
     * 判据 / 一手地址 / 用户口径的分层见 `cards/signal_core.h` 的「咲夜 / 早苗」那一段。
     * 这里逐条钉：逐弹 3 帧循环、蓄力 300 帧与半径公式、贴板底的判据与「不减 ≠ 减到 0」、
     * 红美铃的 ×1.5 与下限、神奈子的快照/重取、恋恋的两条触发路（原生条件 ∪ 用户口径）。*/

    /* --- 17.1 大小姐：逐弹 1 计入 + 2 不计（用户口径 2026-09-15）--- */
    {
        signal_remilia_t r;
        uint16_t k1[1], k2[2];
        int f;

        /* ★★ 2026-09-16 用户改口径：「**目前擦弹频率有点超模，改成一颗弹幕每 6 帧擦一次**」
         *   ⇒ 周期从 3 改成 6（有效仍是 1 帧、计不计的分支一字未改）。*/
        CHECK(SIGNAL_REMILIA_GRAZE_ON_FRAMES == 1);
        CHECK(SIGNAL_REMILIA_GRAZE_OFF_FRAMES == 5);
        CHECK(SIGNAL_REMILIA_GRAZE_PERIOD == 6);

        k1[0] = 7; k2[0] = 7; k2[1] = 9;
        memset(&r, 0, sizeof r);
        /* 一颗弹连着 13 帧在圈里 ⇒ 在它的第 1 / 7 / 13 帧各计一次（周期 6、中间 5 帧不计）*/
        CHECK(signal_remilia_tick(&r, k1, 1, 0, 0) == 1);      /* 第 1 帧：计入 */
        for (f = 2; f <= 6; ++f) CHECK(signal_remilia_tick(&r, k1, 1, 0, 0) == 0);   /* 第 2..6 帧：不计 */
        CHECK(signal_remilia_tick(&r, k1, 1, 0, 0) == 1);      /* 第 7 帧：再计入 */
        for (f = 8; f <= 12; ++f) CHECK(signal_remilia_tick(&r, k1, 1, 0, 0) == 0);
        CHECK(signal_remilia_tick(&r, k1, 1, 0, 0) == 1);      /* 第 13 帧 */
        CHECK(r.total == 3 && r.n == 1 && r.frames == 13);

        /* ★ 逐弹独立：第 3 帧才进圈的那颗有自己的相位（当帧就计入）*/
        memset(&r, 0, sizeof r);
        CHECK(signal_remilia_tick(&r, k1, 1, 0, 0) == 1);      /* 帧1：7 计入（cd = 5）*/
        CHECK(signal_remilia_tick(&r, k1, 1, 0, 0) == 0);      /* 帧2：7 冷 */
        CHECK(signal_remilia_tick(&r, k2, 2, 0, 0) == 1);      /* 帧3：9 是新弹 ⇒ 计入；7 还在冷（不计）*/
        CHECK(r.n == 2);
        CHECK(signal_remilia_tick(&r, k2, 2, 0, 0) == 0);      /* 帧4：7 冷（剩 3）、9 冷（剩 4）*/
        CHECK(r.n == 2 && r.total == 2);   /* 到这一帧为止：7 计 1 次、9 计 1 次 */

        /* 取不到弹池 ⇒ 安全返回 0，且**表与冷却一个都不动**（见 .h）*/
        memset(&r, 0, sizeof r);
        CHECK(signal_remilia_tick(&r, k1, 1, 0, 0) == 1);      /* 帧1：计入（cd 置 5）*/
        f = signal_remilia_tick(&r, NULL, 0, 0, 0);
        CHECK(f == 0 && r.n == 1 && r.frames == 1);
        for (f = 2; f <= 6; ++f) CHECK(signal_remilia_tick(&r, k1, 1, 0, 0) == 0);   /* 帧2..6：冷 */
        CHECK(signal_remilia_tick(&r, k1, 1, 0, 0) == 1);      /* 帧7：再计入 */

        /* 表满 ⇒ 记 dropped、当帧不计（也不进表）*/
        memset(&r, 0, sizeof r);
        {
            uint16_t many[SIGNAL_REMILIA_TRACK_MAX + 5];
            int i, cnt = 0;
            for (i = 0; i < SIGNAL_REMILIA_TRACK_MAX + 5; ++i) many[i] = (uint16_t)(100 + i);
            cnt = signal_remilia_tick(&r, many, SIGNAL_REMILIA_TRACK_MAX + 5, 0, 0);
            CHECK(cnt == SIGNAL_REMILIA_TRACK_MAX);      /* 满表以内全计入 */
            CHECK(r.n == SIGNAL_REMILIA_TRACK_MAX);
            CHECK(r.dropped == 5);
        }
        /* ★★ 2026-09-16m：**出参** = 本帧计入的槽号（卡侧要对每一颗调引擎的 `ce_do_graze()`）
         *   —— 第 1 帧两颗都计入（顺序 = 输入顺序）、冷却期间一个都不给、第 4 帧再给回来。*/
        {
            signal_remilia_t r2;
            uint16_t      kk[3], out[3];
            int32_t       cnt;

            signal_remilia_reset(&r2);
            kk[0] = 7; kk[1] = 9; kk[2] = 11;
            out[0] = out[1] = out[2] = 0xffff;
            cnt = signal_remilia_tick(&r2, kk, 3, out, 3);
            CHECK(cnt == 3 && out[0] == 7 && out[1] == 9 && out[2] == 11);
            out[0] = out[1] = out[2] = 0xffff;
            cnt = signal_remilia_tick(&r2, kk, 3, out, 3);           /* 冷却中 */
            CHECK(cnt == 0 && out[0] == 0xffff);                     /* 一颗都不给 */
            {
                int f2;
                /* 冷却 = `OFF_FRAMES` 帧：上面那一帧已经吃掉 1 帧 ⇒ 再走 `OFF_FRAMES - 1` 帧，
                 * 下一帧才是「冷却走完、重新计入」那一帧。*/
                for (f2 = 0; f2 < SIGNAL_REMILIA_GRAZE_OFF_FRAMES - 1; ++f2) {
                    (void)signal_remilia_tick(&r2, kk, 3, 0, 0);
                }
            }
            out[0] = 0xffff;
            cnt = signal_remilia_tick(&r2, kk, 3, out, 3);           /* 冷却走完 ⇒ 又全给 */
            CHECK(cnt == 3 && out[0] == 7);
            /* 缓冲比颗数小 ⇒ 只写前 `out_max` 个，但返回的仍是**真实颗数** */
            signal_remilia_reset(&r2);
            out[0] = out[1] = 0xffff;
            cnt = signal_remilia_tick(&r2, kk, 3, out, 1);
            CHECK(cnt == 3 && out[0] == 7 && out[1] == 0xffff);
            /* NULL 出参 = 老行为（不写）*/
            signal_remilia_reset(&r2);
            CHECK(signal_remilia_tick(&r2, kk, 3, 0, 0) == 3);
        }

        signal_remilia_reset(&r);
        CHECK(r.n == 0 && r.total == 0 && r.dropped == 0);
        CHECK(signal_remilia_tick(0, k1, 1, 0, 0) == 0);
        CHECK(signal_remilia_tick(&r, k1, 0, 0, 0) == 0);
    }

    /* --- 17.2 二小姐：蓄力 300 帧 ⇒ 松手清屏（半径 = 2×擦弹圈）--- */
    {
        signal_flandre_t f;
        int i;

        CHECK(SIGNAL_FLANDRE_CHARGE_FRAMES == 300);       /* 5 s @ 60 fps */
        CHECK(SIGNAL_FLANDRE_CIRCLE_MIN == 40);           /* 一手 `0x4b92c4` */
        CHECK(SIGNAL_FLANDRE_RADIUS_MUL == 2);

        /* ★ 2026-09-18（用户口径「咲开一个逐渐变大的圆，最大了就知道满了」）：环的比例 ——
         * 只用于画，判定半径不变；下限 0.12（蓄力一开始就得看得见）、满值 = 1.00（= 清屏半径本身）、
         * 中间线性、越界/负数安全。*/
        /* 千分比口径（120…1000）；★ 不许返回 float —— 32 位 ABI 下 float 返回值走 st(0)，
         *   会让目标文件带 x87、撞 `make dllx87` 的 0 条硬线。*/
        CHECK(SIGNAL_FLAN_RING_MIN_PERMILLE == 120);
        CHECK(signal_flandre_ring_ratio_permille(0) == SIGNAL_FLAN_RING_MIN_PERMILLE);
        CHECK(signal_flandre_ring_ratio_permille(-5) == SIGNAL_FLAN_RING_MIN_PERMILLE);
        CHECK(signal_flandre_ring_ratio_permille(SIGNAL_FLANDRE_CHARGE_FRAMES) == 1000);
        CHECK(signal_flandre_ring_ratio_permille(SIGNAL_FLANDRE_CHARGE_FRAMES + 999) == 1000);
        CHECK(signal_flandre_ring_ratio_permille(1) > SIGNAL_FLAN_RING_MIN_PERMILLE);   /* 一蓄就长大 */
        CHECK(signal_flandre_ring_ratio_permille(1) < signal_flandre_ring_ratio_permille(2));
        CHECK(signal_flandre_ring_ratio_permille(150) == 560);                          /* 半程 = 120+440 */
        CHECK(signal_flandre_ring_ratio_permille(299) == 997);                          /* 差一帧就没满 */
        {   /* 单调不减 + 全程落在 [120, 1000] */
            int q, prev = signal_flandre_ring_ratio_permille(0);
            for (q = 1; q <= SIGNAL_FLANDRE_CHARGE_FRAMES; ++q) {
                int r = signal_flandre_ring_ratio_permille(q);
                CHECK(r >= prev);
                CHECK(r >= SIGNAL_FLAN_RING_MIN_PERMILLE && r <= 1000);
                prev = r;
            }
        }


        /* 半径：2 ×(自机判定半径 3.0 + 40.0) = 86.00 px；入参取不到时按原生 3.0 兜底 */
        CHECK(signal_flandre_cancel_radius_x100(300) == 8600);
        CHECK(signal_flandre_cancel_radius_x100(100) == 8200);      /* 判定点被缩到 1.0（原生 Aya）*/
        CHECK(signal_flandre_cancel_radius_x100(0) == 8600);        /* 兜底 = 原生 3.0 */
        CHECK(signal_flandre_cancel_radius_x100(-5) == 8600);

        /* (a) 蓄满 300 帧后松手 ⇒ 本帧清屏 */
        memset(&f, 0, sizeof f);
        for (i = 0; i < SIGNAL_FLANDRE_CHARGE_FRAMES - 1; ++i)
            CHECK(signal_flandre_tick(&f, 1, 0) == 0);
        CHECK(f.charge == 299 && f.armed == 0);
        CHECK(signal_flandre_tick(&f, 1, 0) == 0);        /* 第 300 帧：满，但还没松手 */
        CHECK(f.charge == 300 && f.armed == 1);
        CHECK(signal_flandre_tick(&f, 1, 0) == 0);        /* 继续按：仍不发动（不是自动放）*/
        CHECK(signal_flandre_tick(&f, 0, 0) == 1);        /* ★ 松手那一下 */
        CHECK(f.fires == 1 && f.charge == 0 && f.armed == 0);
        CHECK(signal_flandre_tick(&f, 0, 0) == 0);        /* 松着不会连发 */

        /* (b) 差一帧就松手 ⇒ 什么都不发生（记 aborted）*/
        memset(&f, 0, sizeof f);
        for (i = 0; i < SIGNAL_FLANDRE_CHARGE_FRAMES - 1; ++i) (void)signal_flandre_tick(&f, 1, 0);
        CHECK(signal_flandre_tick(&f, 0, 0) == 0);
        CHECK(f.fires == 0 && f.aborted == 1 && f.charge == 0);

        /* (c) 蓄力中松一下再按 ⇒ 重新数（连续蓄力才作数）*/
        memset(&f, 0, sizeof f);
        for (i = 0; i < 100; ++i) (void)signal_flandre_tick(&f, 1, 0);
        (void)signal_flandre_tick(&f, 0, 0);
        CHECK(f.charge == 0 && f.aborted == 1);
        (void)signal_flandre_tick(&f, 1, 0);
        CHECK(f.charge == 1);

        /* (d) 中弹 ⇒ 打断（❓ 自定）*/
        memset(&f, 0, sizeof f);
        for (i = 0; i < 200; ++i) (void)signal_flandre_tick(&f, 1, 0);
        CHECK(signal_flandre_tick(&f, 1, 1) == 0);
        CHECK(f.charge == 0 && f.armed == 0 && f.breaks == 1);
        CHECK(signal_flandre_tick(&f, 0, 0) == 0 && f.fires == 0);

        /* (e) 满了之后被打中 ⇒ 蓄力也丢（不会留到下一次松手）*/
        memset(&f, 0, sizeof f);
        for (i = 0; i < SIGNAL_FLANDRE_CHARGE_FRAMES; ++i) (void)signal_flandre_tick(&f, 1, 0);
        CHECK(f.armed == 1);
        (void)signal_flandre_tick(&f, 1, 1);
        CHECK(f.armed == 0 && f.charge == 0);
        CHECK(signal_flandre_tick(&f, 0, 0) == 0 && f.fires == 0);

        signal_flandre_reset(&f);
        CHECK(f.charge == 0 && f.fires == 0);
        CHECK(signal_flandre_tick(NULL, 1, 0) == 0);
    }

    /* --- 17.3 诹访子：板底 = 引擎自己的自机 y 下界 `0xd800`（432.0 px）--- */
    CHECK(SIGNAL_BOARD_BOTTOM_FIXED == 0xd800);
    CHECK(signal_core_at_board_bottom(0xd7ff) == 0);      /* 差一格就不算贴底 */
    CHECK(signal_core_at_board_bottom(0xd800) == 1);
    CHECK(signal_core_at_board_bottom(0xe000) == 1);
    CHECK(signal_core_at_board_bottom(0x1000) == 0);      /* 上界 32.0 px 那一头不是板底 */
    CHECK(signal_core_at_board_bottom(0) == 0);
    CHECK(signal_core_at_board_bottom(-1) == 0);

    /* --- 17.4 ③④ 的落点：`faith_signal_step()` / `faith_signal_add()` 的**上游**修饰 --- */
    {
        faith_slot_t a, b;
        signal_step_mod_t m;

        /* (a) `m == NULL` ⇒ 与旧路逐值相同（既有行为不变的**可执行证明**）*/
        {
            int bad = 0, band, gz, d;
            int32_t nows[5] = { 0, 100, 40000, 100000, 129000 };
            int bi, gi, ni;
            for (bi = 0; bi <= 4; ++bi) {
                for (gi = 0; gi <= 2; ++gi) {
                    for (ni = 0; ni < 5; ++ni) {
                        band = bi; gz = gi;
                        memset(&a, 0, sizeof a); memset(&b, 0, sizeof b);
                        a.signal = nows[ni]; b.signal = nows[ni];
                        d = signal_core_step(a.signal, band, gz, 1);
                        if (d != signal_core_step_mod(a.signal, band, gz, 1, NULL)) bad = 1;
                        if (signal_core_apply_step(&a, band, gz, 1) !=
                            signal_core_apply_step_mod(&b, band, gz, 1, NULL)) bad = 1;
                        if (a.signal != b.signal) bad = 1;
                    }
                }
            }
            CHECK(bad == 0);
        }
        /* (b) 诹访子：「不减」≠「减到 0」 —— 线下那一档**整个不施加**，同帧擦弹的加值一点不少 */
        memset(&m, 0, sizeof m);
        m.at_bottom = 1;
        CHECK(signal_core_step_mod(200, FAITH_SIGNAL_BAND_OFFLINE, 0, 1, &m) == 0);
        CHECK(signal_core_step_mod(200, FAITH_SIGNAL_BAND_OFFLINE, 0, 1, NULL) == -L);
        memset(&a, 0, sizeof a); memset(&b, 0, sizeof b);
        a.signal = 100; b.signal = 100;
        (void)signal_core_apply_step_mod(&a, FAITH_SIGNAL_BAND_OFFLINE, 2, 1, &m);   /* 贴板底：2 颗弹 */
        (void)signal_core_apply_step_mod(&b, FAITH_SIGNAL_BAND_OFFLINE, 2, 1, NULL); /* 不贴：照扣 −0.30 */
        CHECK(a.signal == 100 + 2 * ZN);                 /* ★ 一分不少（0 + 擦弹增量）*/
        CHECK(b.signal == 100 + 2 * ZN - L);             /* 对照：正好少一帧的 −0.30 */
        CHECK(a.signal - b.signal == L);
        /* 「减到 0」那种写法会把加值吃掉：0 起算时 −0.30 先夹到 0、再加擦弹增量 ⇒ 差一帧的损失 */
        memset(&a, 0, sizeof a);
        (void)faith_signal_add(&a, 2 * ZN - L);
        CHECK(a.signal == 2 * ZN - L);
        /* 贴板底只在**线下**那一档改（线上三档一字节不动）*/
        CHECK(signal_core_step_mod(100000, FAITH_SIGNAL_BAND_TOP, 0, 1, &m) ==
              signal_core_step_mod(100000, FAITH_SIGNAL_BAND_TOP, 0, 1, NULL));
        CHECK(signal_core_step_mod(100000, FAITH_SIGNAL_BAND_HOLD, 0, 1, &m) == 0);

        /* (c) 红美铃：正增量 ×1.5、负增量不乘、下限 = 满值一半（显示 50.00）*/
        CHECK(SIGNAL_MEILING_FLOOR == 50000);
        CHECK(SIGNAL_MEILING_FLOOR == FAITH_SIGNAL_HI_MARK / 2);
        memset(&m, 0, sizeof m);
        m.meiling_on = 1;
        /* 已满（100.00）时档位给的那一份：+0.16 → ×1.5 = +0.24 */
        CHECK(signal_core_step_mod(100000, FAITH_SIGNAL_BAND_TOP, 0, 1, &m) == GTOP + GTOP / 2);
        CHECK(signal_core_step_mod(100000, FAITH_SIGNAL_BAND_MID, 0, 1, &m) == GMID + GMID / 2);
        CHECK(signal_core_step_mod(100000, FAITH_SIGNAL_BAND_LOW, 0, 1, &m) == GTRICK + GTRICK / 2);
        /* 擦弹增量也 ×1.5：已满时 1 颗弹（普通）= 5000 + 160(档位) 再加半 */
        CHECK(signal_core_step_mod(100000, FAITH_SIGNAL_BAND_TOP, 1, 1, &m) ==
              (GTOP + ZN) + (GTOP + ZN) / 2);
        /* ★「线上未满 ⇒ 补满并钳到 100.00」这条一手的钳**保住**：×1.5 之后仍不会越过 100.00 */
        CHECK(signal_core_step_mod(90000, FAITH_SIGNAL_BAND_TOP, 0, 1, &m) == 10000);
        CHECK(signal_core_step_mod(90000, FAITH_SIGNAL_BAND_TOP, 0, 1, NULL) == 10000);
        /* ★ 低于 50.00 时「一帧补满」也被 ×1.5：+50.00 → +75.00（仍不越 100.00 那条一手的钳）*/
        CHECK(signal_core_step_mod(0, FAITH_SIGNAL_BAND_TOP, 0, 1, &m) == 75000);
        CHECK(signal_core_step_mod(40000, FAITH_SIGNAL_BAND_TOP, 0, 1, &m) == 60000);  /* 75000 撞 100.00 的钳 ⇒ 补到 100.00 */
        CHECK(signal_core_step_mod(50000, FAITH_SIGNAL_BAND_TOP, 0, 1, &m) == 50000);  /* 再往上就撞 100.00 的钳 */
        CHECK(signal_core_step_mod(60000, FAITH_SIGNAL_BAND_TOP, 0, 1, &m) == 40000);  /* d=40000×1.5=60000 → 钳回 40000 */
        /* 负增量不乘：线下的 −0.30 原样（只是被下限接住）*/
        CHECK(signal_core_step_mod(60000, FAITH_SIGNAL_BAND_OFFLINE, 0, 1, &m) == -L);
        /* 下限：槽低于 50.00 时，这一帧的增量被抬到「正好补到 50.00」*/
        CHECK(signal_core_step_mod(40000, FAITH_SIGNAL_BAND_OFFLINE, 0, 1, &m) == 10000);
        /* ★★ 2026-09-16 加（用户实跑：「你实现 >0.5 的方式很烂，导致它的 UI 在 1 帧 1 帧跳」）：
         *   槽**恰好等于**下限、且这一帧是线下（会扣）时，增量必须是 **0** —— 不能扣下去再抬回来。
         *   旧写法带 `signal_now < FLOOR` 前置条件 ⇒ 恰好等于 FLOOR 的那一帧照扣 ⇒
         *   在 FLOOR 与 FLOOR−Δ 之间**逐帧振荡**（HUD 的条/数值看起来在 1 帧 1 帧跳）。*/
        {
            int32_t s0 = SIGNAL_MEILING_FLOOR;
            int i;
            CHECK(signal_core_step_mod(s0, FAITH_SIGNAL_BAND_OFFLINE, 0, 1, &m) == 0);
            memset(&a, 0, sizeof a);
            a.signal = SIGNAL_MEILING_FLOOR;
            for (i = 0; i < 10; ++i) {
                (void)signal_core_apply_step_mod(&a, FAITH_SIGNAL_BAND_OFFLINE, 0, 1, &m);
                CHECK(a.signal == SIGNAL_MEILING_FLOOR);       /* ★ 10 帧都纹丝不动（不振荡）*/
            }
        }
        memset(&a, 0, sizeof a);
        a.signal = 40000;
        (void)signal_core_apply_step_mod(&a, FAITH_SIGNAL_BAND_OFFLINE, 0, 1, &m);
        CHECK(a.signal == SIGNAL_MEILING_FLOOR);
        memset(&a, 0, sizeof a);                          /* 被弹归零之后：下一帧的下限把它抬回来 */
        (void)signal_core_apply_step_mod(&a, FAITH_SIGNAL_BAND_TOP, 0, 1, &m);
        CHECK(a.signal >= SIGNAL_MEILING_FLOOR);
        /* 已经在下限之上 ⇒ 下限**不动**它（不会把槽往下拉）*/
        CHECK(signal_core_step_mod(60000, FAITH_SIGNAL_BAND_OFFLINE, 0, 1, &m) == -L);
        /* `meiling_on == 0` ⇒ 逐值等于旧路（对拍）*/
        {
            int bad = 0, ni;
            int32_t nows[4] = { 0, 40000, 100000, 129000 };
            for (ni = 0; ni < 4; ++ni) {
                memset(&m, 0, sizeof m);
                if (signal_core_step_mod(nows[ni], FAITH_SIGNAL_BAND_OFFLINE, 1, 1, &m) !=
                    signal_core_step(nows[ni], FAITH_SIGNAL_BAND_OFFLINE, 1, 1)) bad = 1;
                if (signal_core_step_mod(nows[ni], FAITH_SIGNAL_BAND_TOP, 0, 1, &m) !=
                    signal_core_step(nows[ni], FAITH_SIGNAL_BAND_TOP, 0, 1)) bad = 1;
            }
            CHECK(bad == 0);
        }
        /* 落值仍夹在 [0, SIGNAL_MAX] */
        memset(&a, 0, sizeof a);
        memset(&m, 0, sizeof m);
        m.meiling_on = 1;
        a.signal = SIGNAL_MAX - 1;
        (void)signal_core_apply_step_mod(&a, FAITH_SIGNAL_BAND_TOP, 5, 1, &m);
        CHECK(a.signal == SIGNAL_MAX);
        CHECK(signal_core_apply_step_mod(NULL, 0, 0, 1, &m) == 0);
    }

    /* --- 17.5 神奈子：收点线「快照基准 + 相对下移」（⏳ 待迁移到共享 PoC 层）--- */
    {
        signal_kanako_t k;
        int32_t drop = SIGNAL_KANAKO_POC_DROP_PX * 100;

        /* ❓ 自定（用户 2026-09-16x：「下降幅度有点弱」⇒ 32 → 96；
         * 锚点 = TH18 原生 22 号卡把同一字段写成 224.0，而默认 128.0 ⇒ 那一档 = +96）*/
        CHECK(SIGNAL_KANAKO_POC_DROP_PX == 96);
        signal_kanako_reset(&k);
        CHECK(k.base_x100 == 0 && k.applied_x100 == 0);
        /* 第一帧：现值 = 基准（引擎的 128.0）⇒ 往下移 96 px（= 128 → 224，与原生卡同值）*/
        CHECK(signal_kanako_tick(&k, 12800, drop) == 22400);
        CHECK(k.base_x100 == 12800 && k.resnaps == 0);
        /* 第二帧：现值仍是**我们写的那个值** ⇒ 基准不变、不再重取 */
        CHECK(signal_kanako_tick(&k, 22400, drop) == 22400);
        CHECK(k.resnaps == 0 && k.writes == 2);
        /* 被引擎复位回 128.0 ⇒ 重取基准（不与自己打架）*/
        CHECK(signal_kanako_tick(&k, 12800, drop) == 22400);
        CHECK(k.base_x100 == 12800 && k.resnaps == 1);
        /* ★★ 2026-09-16y（用户口径「**同装只多加 0.5 吧**」）：**基准 ≠ 默认 128.0 ⇒ 追加量减半**。
         *   最典型的同装对象 = **原生 22 号卡**（把同一字段写成 224.0，`th18:0x40c275`）
         *   ⇒ 224 + 96/2 = **272**（而不是 320）。*/
        CHECK(SIGNAL_KANAKO_POC_BASE_DEFAULT_PX == 128);
        signal_kanako_reset(&k);
        CHECK(signal_kanako_tick(&k, 22400, drop) == 27200);      /* ★ 与原生 22 号卡同装 */
        CHECK(k.base_x100 == 22400 && k.resnaps == 0);
        CHECK(signal_kanako_tick(&k, 27200, drop) == 27200);      /* 基线稳住，不再变 */
        /* 别的写者把线改成**不是我们写过的值** ⇒ 重取基准、并且同样按「非默认」减半 */
        CHECK(signal_kanako_tick(&k, 25600, drop) == 30400);
        CHECK(k.base_x100 == 25600 && k.resnaps == 1);
        /* drop = 0 ⇒ 还原成基准值（本卡不生效时那一支）*/
        CHECK(signal_kanako_tick(&k, 30400, 0) == 25600);
        CHECK(signal_kanako_restore(&k) == 25600);
        /* 现值读不到（0）⇒ 这一帧别写、也不动基准 */
        CHECK(signal_kanako_tick(&k, 0, drop) == 0);
        CHECK(k.base_x100 == 25600);
        signal_kanako_reset(&k);
        CHECK(signal_kanako_restore(&k) == 0);            /* 从没写过 ⇒ 什么都没得还原 */
        CHECK(signal_kanako_tick(NULL, 12800, drop) == 0);
        CHECK(signal_kanako_restore(NULL) == 0);
    }

    /* --- 17.6 恋恋：放开键 ⇒ 4 帧无敌（载体/条件/量全部照抄原生 29 Aya 卡）--- */
    {
        signal_koishi_t k;
        /* 原生条件（`0x40ca95`–`0x40cab2` 化简）：`last_dir == 0 && dir != 0 && !focus`，
         * 而射击键按住时 `last_dir` 每帧被强制 0（`0x40c853`）。*/
        memset(&k, 0, sizeof k);
        CHECK(signal_koishi_tick(&k, SIGNAL_IN_SHOT, 3, 0) == 0);        /* 按着射击 + 方向：不发动 */
        CHECK(k.last_dir == 0);                                          /* ★ 被强制 0（照抄原生）*/
        CHECK(signal_koishi_tick(&k, SIGNAL_IN_SHOT, 3, 0) == 0);
        CHECK(signal_koishi_tick(&k, SIGNAL_IN_LEFT, 3, 0) == 1);        /* ★ 放开射击那一帧 */
        CHECK(k.fires_native == 1 && k.fires_user == 1);                 /* 两路同时成立也只算「本帧发动」*/
        CHECK(signal_koishi_tick(&k, SIGNAL_IN_LEFT, 3, 0) == 0);        /* 一直按着方向：不再发动 */
        CHECK(k.fires_native == 1);
        /* 没按方向 ⇒ 原生那一路不发动 */
        CHECK(signal_koishi_tick(&k, SIGNAL_IN_SHOT, 0, 0) == 0);
        CHECK(signal_koishi_tick(&k, 0, 0, 0) == 1);                     /* 但用户口径那一路（放开）会 */
        CHECK(k.fires_native == 1 && k.fires_user == 2);
        /* 聚焦时不发动（原生门 `0x40cab2`）*/
        memset(&k, 0, sizeof k);
        CHECK(signal_koishi_tick(&k, SIGNAL_IN_SHOT, 3, 0) == 0);
        CHECK(signal_koishi_tick(&k, SIGNAL_IN_SHOT | SIGNAL_IN_FOCUS, 3, 1) == 0);
        CHECK(k.fires_native == 0 && k.fires_user == 0);
        /* 站着不动：方向 0 ⇒ 两路都不发动 */
        memset(&k, 0, sizeof k);
        CHECK(signal_koishi_tick(&k, 0, 0, 0) == 0);
        CHECK(signal_koishi_tick(&k, 0, 0, 0) == 0);
        CHECK(signal_koishi_tick(&k, SIGNAL_IN_UP, 0, 0) == 0);          /* 只按移动键不算「全键位」*/
        /* 用户口径那一路 —— ★★ **2026-09-16 改口径（用户逐字：「恋恋改成放开 shift 和 Z 吧」）**：
         * 掩码从「六个动作键」收窄成 **Z | Shift**。下面按新口径逐条钉（孤立用户那一路时先吃掉
         * 原生那一次的发动：第一帧先按方向把 `last_dir` 顶成非 0）。*/
        /* 为了**只测用户那一路**，下面全部用 `move_dir = 0`（原生那一路要求 `dir != 0` ⇒ 不会混进来）。*/
        /* ① 放开 **Z** ⇒ 发动 */
        memset(&k, 0, sizeof k);
        CHECK(signal_koishi_tick(&k, SIGNAL_IN_SHOT, 0, 0) == 0);
        CHECK(signal_koishi_tick(&k, 0, 0, 0) == 1);
        CHECK(k.fires_user == 1 && k.fires_native == 0);                 /* ★ 只有用户那一路动 */
        /* ② 放开 **Shift** ⇒ 也发动 */
        memset(&k, 0, sizeof k);
        CHECK(signal_koishi_tick(&k, SIGNAL_IN_FOCUS, 0, 0) == 0);
        CHECK(signal_koishi_tick(&k, 0, 0, 0) == 1);
        CHECK(k.fires_user == 1 && k.fires_native == 0);
        /* ③ Z 与 Shift 都按着 ⇒ 两个都放开才发动**一次**（不是两次）*/
        memset(&k, 0, sizeof k);
        CHECK(signal_koishi_tick(&k, SIGNAL_IN_SHOT | SIGNAL_IN_FOCUS, 0, 0) == 0);
        CHECK(signal_koishi_tick(&k, 0, 0, 0) == 1);
        CHECK(k.fires_user == 1);
        /* ④ **只剩一个**没放开 ⇒ 不发动（用户口径：「不是 or，是……这一帧不按 shift and 不按着 Z」）
         *    —— 两个方向都测：只松 Shift / 只松 Z。*/
        memset(&k, 0, sizeof k);
        (void)signal_koishi_tick(&k, SIGNAL_IN_SHOT | SIGNAL_IN_FOCUS, 0, 0);
        CHECK(signal_koishi_tick(&k, SIGNAL_IN_SHOT, 0, 0) == 0);        /* 只松 Shift、Z 还按着 ⇒ 不发动 */
        CHECK(k.fires_user == 0);
        memset(&k, 0, sizeof k);
        (void)signal_koishi_tick(&k, SIGNAL_IN_SHOT | SIGNAL_IN_FOCUS, 0, 0);
        CHECK(signal_koishi_tick(&k, SIGNAL_IN_FOCUS, 0, 0) == 0);       /* 只松 Z、Shift 还按着 ⇒ 不发动 */
        CHECK(k.fires_user == 0);
        CHECK(signal_koishi_tick(&k, 0, 0, 0) == 1);                     /* 两个都松开的那一帧才发动 ✓ */
        CHECK(k.fires_user == 1);
        /* ⑤ **不再看别的键**（炸弹 X / 发动卡 / 切卡 / 暂停都退出判据）*/
        memset(&k, 0, sizeof k);
        (void)signal_koishi_tick(&k, SIGNAL_IN_BOMB | SIGNAL_IN_CARD | SIGNAL_IN_CHANGE, 0, 0);
        CHECK(signal_koishi_tick(&k, 0, 0, 0) == 0);                     /* 放开 X/卡键 ⇒ **不**算 */
        CHECK(k.fires_user == 0);
        /* ⑥ 只按移动键 ⇒ 不在掩码里（松开方向键也不发动）*/
        memset(&k, 0, sizeof k);
        (void)signal_koishi_tick(&k, SIGNAL_IN_UP | SIGNAL_IN_RIGHT, 0, 0);
        CHECK(signal_koishi_tick(&k, 0, 0, 0) == 0);
        CHECK(k.fires_user == 0);
        /* ⑦ 与**原生那一路的并集**：按着 Z 走一步、松 Z 那一帧两路同时成立 ⇒ 本帧仍只发动一次 */
        memset(&k, 0, sizeof k);
        (void)signal_koishi_tick(&k, SIGNAL_IN_SHOT, 3, 0);              /* Z 按住：last_dir 被强制 0 */
        CHECK(k.last_dir == 0);
        CHECK(signal_koishi_tick(&k, SIGNAL_IN_LEFT, 3, 0) == 1);        /* 松 Z：原生（起步）+ 用户（放开）都成立 */
        CHECK(k.fires_native == 1 && k.fires_user == 1);
        /* 掩码本体：新口径 = Z | Shift；旧口径的六个动作键宏保留（文档用，恋恋不再读它）*/
        CHECK(SIGNAL_IN_Z_OR_SHIFT == (SIGNAL_IN_SHOT | SIGNAL_IN_FOCUS));
        CHECK(SIGNAL_IN_ACTION_MASK == (0x1u | 0x2u | 0x8u | 0x100u | 0x400u | 0x800u));
        CHECK((SIGNAL_IN_ACTION_MASK & SIGNAL_IN_DIRMASK) == 0);
        /* ★ 量 = **原生 Aya 卡的量**（`0x40cb16` 的 `addss …,0x4b9248` = +4.0 帧）；用户 2026-09-15 拍板照原生 */
        CHECK(SIGNAL_KOISHI_INVULN_FRAMES == 4);
        signal_koishi_reset(&k);
        CHECK(k.fires_native == 0 && k.fires_user == 0 && k.last_dir == 0);
        CHECK(signal_koishi_tick(NULL, 0, 0, 0) == 0);
    }


    /* ==================================================================================
     * ★★ 2026-09-16zb：**一手阵型几何**（TH11 `pl01b.sht + 0x28 + formation*0x50`，
     *   条目 = `T(档-1)+序号`）—— 这里把**逐值**钉住：改表就会红。
     *   一手链条的唯一出处 = `cards/signal_core.c` 那段注释 +
     *   `engine/player/th11/04-marisa-b-option-geometry.md`。
     * ================================================================================== */
    {
        int f, n, i;
        /* ① 三角数前缀：档 1..4 的块起点 = 0/1/3/6（每档块长 = 档号）*/
        CHECK(signal_formation_tri(1) == 0);
        CHECK(signal_formation_tri(2) == 1);
        CHECK(signal_formation_tri(3) == 3);
        CHECK(signal_formation_tri(4) == 6);
        CHECK(signal_formation_tri(0) == 0);            /* 档 0（零子机）→ 0，不当负 */
        CHECK(signal_formation_tri(9) == 6);            /* 越界钳到档 4（T(4-1) = 6 = 最后一块的起点）*/
        for (f = 0; f < SIGNAL_FORMATION_COUNT; ++f) {
            signal_cell_t cell;
            int32_t sum = 0;

            /* ② 每一档的第 i 格都必须能取到（i < 档），且档外/档后越界一律拒绝 */
            for (n = 1; n <= SIGNAL_FORMATION_LEVELS; ++n) {
                sum = 0;
                for (i = 0; i < n; ++i) {
                    CHECK(signal_formation_cell_lv(f, n, i, &cell));
                    sum += cell.x10;
                }
                CHECK(sum == 0);                        /* 每档的块 x 左右对称（一手数据本身对称）*/
                CHECK(signal_formation_cell_lv(f, n, n, &cell) == 0);      /* 序号 = 档 ⇒ 越界 */
                CHECK(signal_formation_cell_lv(f, n, -1, &cell) == 0);
            }
            CHECK(signal_formation_cell_lv(f, 0, 0, &cell) == 0);          /* 档 0 没有格子 */
            CHECK(signal_formation_cell_lv(f, 1, 0, &cell) && cell.x10 == 0);   /* 档 1 = 轴上一点 */
            CHECK(signal_formation_cell_lv(-1, 1, 0, &cell) == 0);
            CHECK(signal_formation_cell_lv(f, 1, 0, NULL) == 0);

            /* ③ 档 4 的四格与「档 4 = 表里第 6..9 条」逐值一致（= 老接口同值）*/
            for (i = 0; i < SIGNAL_FORMATION_CELLS; ++i) {
                signal_cell_t a, b;
                CHECK(signal_formation_cell_lv(f, SIGNAL_FORMATION_LEVELS, i, &a));
                CHECK(signal_formation_cell(f, i, &b));
                CHECK(a.x10 == b.x10 && a.y10 == b.y10);
            }
        }
        /* ④ 逐值钉住四个代表点（一手：火档1 = (0,-32)、水档2 = (-16,24)/(16,24)、
         *    金档4 的第 4 格 = (0,80)、土档4 的第 3 格 = (-20,30)）——单位 = 1/10 px */
        {
            signal_cell_t c;
            CHECK(signal_formation_cell_lv(0, 1, 0, &c) && c.x10 == 0 && c.y10 == -320);
            CHECK(signal_formation_cell_lv(1, 2, 0, &c) && c.x10 == -160 && c.y10 == 240);
            CHECK(signal_formation_cell_lv(1, 2, 1, &c) && c.x10 == 160 && c.y10 == 240);
            CHECK(signal_formation_cell_lv(3, 4, 3, &c) && c.x10 == 0 && c.y10 == 800);
            CHECK(signal_formation_cell_lv(4, 4, 2, &c) && c.x10 == 200 && c.y10 == 300);
            /* 与 pl01b.sht 的 50 条实条目**同量纲**：全部落在 ±1000（±100 px）内 */
            for (f = 0; f < SIGNAL_FORMATION_COUNT; ++f)
                for (i = 0; i < 10; ++i) {
                    signal_cell_t g;
                    int32_t lv = (i < 1) ? 1 : (i < 3) ? 2 : (i < 6) ? 3 : 4;
                    int32_t idx = i - signal_formation_tri(lv);
                    CHECK(signal_formation_cell_lv(f, lv, idx, &g));
                    CHECK(g.x10 >= -1000 && g.x10 <= 1000 && g.y10 >= -1000 && g.y10 <= 1000);
                }
        }
        /* ⑤ 兼容壳：`_sym` 现在只是 `_lv(档 = n)` 的委托（旧的自定"按对称轴凑格"已退役）*/
        {
            signal_cell_t a, b;
            CHECK(signal_formation_cell_sym(2, 1, 3, &a));
            CHECK(signal_formation_cell_lv(2, 3, 1, &b));
            CHECK(a.x10 == b.x10 && a.y10 == b.y10);
            CHECK(signal_formation_cell_sym(2, 3, 3, &a) == 0);   /* n=3 只有 0..2 */
        }
    }

    printf("test_signal_core: %d passed, %d failed\n", s_pass, s_fail);
    return s_fail ? 1 : 0;
}
