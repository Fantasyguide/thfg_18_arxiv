/* signal_core.h —— 地灵殿（card-expand id 111）的**纯逻辑**：信号槽的阈值 / 强度映射 /
 * 蓝点分乘数。只 <stdint.h>，主机 gcc 直接测（`tests/test_signal_core.c`）。
 *
 * 设计（**照它实装，别自己发明**）：`docs/superpowers/specs/2026-09-10-sa-signal-design.md`
 *   §2.1 信号槽 0–130（<100 线上 +50/f · ≥100 线上 +0.09/f · 擦弹 +8/graze · 线下 −0.3/f ·
 *        被弹 / 关卡归 0 · **满（≥100）像上线回收一样自动收道具**）
 *   §2.3 蓝点分 = `信仰 × (擦弹倍率 + 信号强度)`
 * 数值与常数的**唯一出处**是共用件 `faith_core.h`（`FAITH_SIGNAL_*` /
 * `FAITH_GRAZE_MULT_MAX` / `FAITH_SA_*` / `FAITH_POWER_*`）—— 本文件只写「怎么用」，
 * **一个数都不另立**（用户 2026-09-12：「110/111 走同一个东西」，见
 * `docs/superpowers/specs/2026-09-11-system-card-spec-review.md` §3.2.1 / §7.1 / §7.5）。
 *
 * ## 本文件与 `faith_core` 的边界（别把功劳算错）
 *
 *   | 量 | 谁实现 | 为什么 |
 *   | --- | --- | --- |
 *   | 信号槽的**按帧增减** | **本文件**（`signal_core_step` = `faith_signal_step` 的一层薄封装，见 .c 头注）| 111 私有 |
 *   | 满槽阈值 / 强度映射 / 蓝点分乘数 | **本文件** | 111 私有（SA 独有） |
 *   | 槽内加值 / 上限夹取 / 小绿点余数 | `faith_core.c`（共用件，**不重写**） | 110 + 111 共用 |
 *   | 灵击门槛 / 扣 1 档 / 下限 1.05 | `mof_core.c`（共用件，**不重写**） | 110 + 111 共用 |
 *   | 擦弹倍率 `min(graze×0.0001, 8.99)` | `mof_core.c` 的 `mof_core_graze_mult()` | **111 用**（它是 th11 的式子；110 单装时不参与 PV，见 `mof_core.h` 的归属订正）|
 */
#pragma once
#ifndef SIGNAL_CORE_H
#define SIGNAL_CORE_H
#include <stdint.h>
#include "faith_core.h"

/* ---- 槽的量纲（照 `faith_core.h`，这里只给别名，方便 111 侧读起来自洽）----
 * `signal` 是**千分点定点**（1.00 = 100000），上限 130 点 = `FAITH_SIGNAL_MAX`。*/
#define SIGNAL_UNIT          1000          /* 1.00 定量 = 千分点 1000 */
#define SIGNAL_MAX           FAITH_SIGNAL_MAX      /* 130000 = 130 点 */
#define SIGNAL_HI_MARK       FAITH_SIGNAL_HI_MARK  /* 100000 = 100 点（满槽阈值）*/
#define SIGNAL_FULL_AT       FAITH_SIGNAL_HI_MARK  /* ★ 满（≥100）⇒ 像上线回收一样自动收道具 */

/* ---- 绿点三档（SA 表；出处 §7.1 的表）---- */
#define SIGNAL_GREEN_BIG     FAITH_SA_BIG        /* 大绿点 +100（击破通道）*/
#define SIGNAL_GREEN_MID     FAITH_SA_MID        /* 中绿点 +10（满 P 后吃 P 点）*/
#define SIGNAL_GREEN_SMALL_N FAITH_SA_SMALL_N    /* 小绿点**每 100 个** +10（余数跨关保留）*/
#define SIGNAL_GREEN_SMALL_V FAITH_SA_SMALL_V

/* ---- 信号槽推进（§2.1 的四段。⚠ 只**转发**共用件的 `faith_signal_step()`）----
 * `line_reading` = 本帧读到的**回收线读数**（`faith_player_above_line()`：线上 1 / 线下 0）；
 * `offline` = **线下**（= `!line_reading` 且自机不在死亡 / 复活态）。
 * ⚠ 两参是分开的、不是互补：自机 state ∈ {2,4}（死亡 / 复活）时 `faith_player_above_line()`
 *   返回 0，但那时**既不加也不减**（原版那两帧同样不看回收线）⇒ 卡侧传 `(0, 0)`。
 * `graze` = 本帧**新增**的擦弹数（原生 `GRAZE` 计数器的帧差，0x4ccd0c）。
 * 返回值可正可负（千分点）；调用方喂 `signal_core_apply()`。*/
int32_t signal_core_step(int32_t signal_now, int line_reading, int offline, int32_t graze);

/* 落到槽里（夹 [0, SIGNAL_MAX]），返回落值后的 signal。 */
int32_t signal_core_apply(faith_slot_t *s, int32_t d);
int32_t signal_core_apply_step(faith_slot_t *s, int line_reading, int offline, int32_t graze);
/* 被弹 / 关卡结束归 0 ⇒ 返回 1（让调用方知道发生了复位，便于日志）。 */
int     signal_core_reset(faith_slot_t *s);

/* ---- 绿点收集入账（§2.2 / §7.1 / §7.3）—— 卡侧 `signal_on_pickup` 的**判定体** ----
 * 一颗**实体**绿点被收走时调用（`faith.c` 的 `dot_collect()` 带 `tier` 与 `value` 回调过来）：
 *   · 大绿点（击破通道，+100）⇒ 卡侧传 `FAITH_EV_BIG`：Δ_111 += 100、连击槽 +130f（§7.5）；
 *   · 中绿点（满 P 后吃 P 点，+10）⇒ 卡侧传 `FAITH_EV_GREEN`：Δ_111 += 10、连击槽 +100f。
 * 加值与上限夹取（Δ_111 上限 = 原版基础，§7.2）在 `faith_slot_add()` 里，本函数不重写。
 * `value` 是这次实体的**单颗值**（大 100 / 中 10，来源见 `signal.c` 的两个撒点钩子）。
 * 返回**定点（×FAITH_FIXED）**的入账量（= `faith_slot_add()` 的返回值；夹到上限时返回 0）。
 * ⚠ 2026-09-12 审计加：原来这段判定**只写在卡侧静态回调里**，而卡侧代码主机编不出来
 *   ⇒ 单测一次都覆盖不到（而且当时根本没人撒 111 槽的点）。判定搬到这里就是为了能测。*/
int32_t signal_core_pickup(faith_slot_t *s, faith_ev_t ev, int32_t value);

/* 夹取（纯函数，便于单测边界）。 */
int32_t signal_core_clamp(int32_t v);
/* 满槽（≥100）⇒ 触发「上线回收式全收」（§2.1）。 */
int     signal_core_is_full(int32_t signal_now);

/* ---- 信号强度 → 蓝点分公式里的那一项（§2.3：`0.00–1.00`）----
 * 映射 = `min(signal, 100) / 100`，用**千分点**表示（1000 = 1.00）。
 * 依据：spec §2.1「**显示上 ≥100 一律显示 1.00**」+ §2.3 明写 `信号强度 = 0.00–1.00`
 * ⇒ 乘数项封顶 1.00（130 上限只体现在**槽的显示格数**上，不改公式）。*/
int32_t signal_core_strength(int32_t signal_now);

/* ---- 蓝点分（§2.3）----
 * `bonus = 信仰 × (擦弹倍率 + 信号强度)`，三个入参**全是千分点定点**，结果向下取整。
 * 只装 111 时 `faith_total` 就是 SA 信仰（`faith_slot_total()`）；
 * 两卡同装时它是计分层聚合出来的总式（见 .c 的 `signal_core_bonus_fixed()`）。 */
int32_t signal_core_bonus(int32_t faith_total, int32_t graze_mult_permille, int32_t signal_now);
/* 定点版：`faith_fixed` = ×FAITH_FIXED；返回**同样定点**的 bonus（不取整、留给调用方 /100）。*/
int32_t signal_core_bonus_fixed(int32_t faith_fixed, int32_t graze_mult_permille, int32_t signal_now);

/* ---- 契约 PV 总式（EVENTS-CONTRACT.md §3 / review §7.4）—— **不含乘数**的那一半 ----
 * 落笔者在 `on_item_score` 里要算的「基础 PV」：
 *   只装 111：`PV = 信仰_111`（= `faith_score_unit(s)` = 基础(难度) + Δ_111）
 *   两卡同装：`PV = 基础均值 + Δ_110 + Δ_111`
 *            `= (FAITH_BASE_110 + 基础_111)/2 + (aggregate − 基础_110 − 基础_111)`
 *   两者都带上「**基础只取一份**」这条（§7.2：绝不能把两份基础相加）。
 * 拿到基础 PV 后**再乘** `(擦弹倍率 + 信号强度)`（用 `signal_core_bonus()` 的乘数项）。
 *
 * ⚠ 为什么不直接用共用件的 `mof_core_pv()`：起初它的「两卡同装」那一支把基础记了三遍
 *   （`(基础均值 + aggregate) × 乘数`，而 `aggregate` 里已经含两份基础），与契约不符 ⇒
 *   111 侧先按契约原文实现了自己这一份（`mof_core_pv()` 的**只装 110 / 只装 111** 两支本来就是对的）。
 *   **2026-09-12 已把共用件修成契约口径**，现在两边逐字相等 ⇒ `tests/test_signal_core.c` §12.6 直接
 *   断言「两卡同装时 `signal_core_pv_111_scaled()` == `mof_core_pv()`」；本函数保留 = 111 落笔时的自足实现。
 * `aggregate` = `ce_score_aggregate()`。 */
int32_t signal_core_pv_111(const faith_slot_t *s111, int32_t aggregate, int have110);
/* 同上 + 乘数（擦弹倍率与信号都传千分点）⇒ 完整 PV，与卡侧 `on_item_score` 同式。*/
int32_t signal_core_pv_111_scaled(const faith_slot_t *s111, int32_t aggregate, int have110,
                                  int32_t graze_mult_permille, int32_t signal_now);

/* ---- HUD / 诊断用的读数（纯整数，便于单测）---- */
/* 信号格的填充比例（千分点 0..1000）：`signal / 130 点`。 */
int32_t signal_core_gauge_permille(int32_t signal_now);
/* 手机信号样式：第 `cell` 格（0 起）点亮到几分（千分点 0..1000）。
 * 每格 = 130/`SIGNAL_CELLS` 点，格内线性。`cell` 越界 → 0。 */
#define SIGNAL_CELLS        5      /* 手机信号样式的格子数（spec §2.5「3–5 格」⇒ 取 5）*/
int32_t signal_core_cell_permille(int32_t signal_now, int cell);

#endif /* SIGNAL_CORE_H */
