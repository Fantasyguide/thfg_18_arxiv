/* faith_hud_core.h —— ★★ 110（风神录）/ 111（地灵殿）**组合 UI** 的**纯逻辑**层。
 *
 * 只 <stdint.h>，主机 gcc 直接测（`tests/test_faith_hud_core.c`）。
 * 引擎访问 / VM 池在 `faith_hud.c`（共用件的「画」那一半）。
 *
 * ## 这块 UI 是什么（用户 2026-09-15 口径，逐字见 `NEXT.md` §2A）
 *
 * > ①「采用**合体 UI**。**th10 的 UI** 是，一个蓝点，右侧是分为**两行**，**最大得点**，下面是**连击槽**。
 * >    **th11 的 UI** 是，一个蓝点，右侧直接是**最大得点 6 位「×信号强度」**，而**最大得点上面是信号强度**。」
 * > ②「**组合 UI** 就是：一个**稍大的蓝点**，右侧分为**三行**，从上到下：**信号强度槽**、
 * >    **最大得点×信号强度数值**、**连击槽**；**连击槽长一些以托住最大得点×信号强度数值**。」
 * > ③「只有 110 或只有 111 **都显示这一套 UI**，只是对应的那部分**不起作用**。
 * >    只有 110 时：信号强度显示 **1.00**、信号**满格（不闪烁）**；只有 111 时：**连击条填满**。」
 * > ④「这里**只显示风神录 + 地灵殿的额外最大得点**……系统卡只加额外最大得点 ⇒ 这个地方**可以显示 0**。」
 *
 * ⇒ ★ **行 2 的数字 = Δ_10 + Δ_11（两份「额外」之和），不是「基础 + Δ」**（基础是宿主自己那份
 *   最大得点的事，不进这块）；一局开头显示 **0** 是**正确显示**。
 *   ⚠ 实现上**别**用 `faith_slot_total()`（那是「基础 + Δ」，`faith_core.h` §单位换算）。
 *
 * ## 逐项出处（`复刻` vs `❓ 自定` —— 本仓铁律：不许把自定包装成复刻）
 *
 * | 本文件的函数 / 常量 | 判定 | 一手出处 |
 * | --- | --- | --- |
 * | `faith_hud_delta_display()` = 两槽 Δ 各出定点再相加 | 复刻（用户 ④）| `NEXT.md` §2A ④ |
 * | `faith_hud_trunc10()`（值截到 10 的倍数）| **复刻** | `th11:0x41ce37` `idiv 10` + `th11:0x41ce43` `sub edi,edx` |
 * | `faith_hud_digits6()`（6 位零填充）| **复刻** | TH11 `"%.6d"`（`th11:0x494198`）+ TH10 前导零照画（`th10:0x414a7d` 循环无跳零分支）|
 * | `faith_hud_signal_pct/level/width_px()` | **复刻** | `th11:0x41b6a2`（档 = `min(⌊槽/100⌋,100)/10`）+ `th11:0x41b707`–`0x41b711`（宽 = `9+4×档`）|
 * | `faith_hud_signal_flash()`（档位闪色 4 档）| **复刻** | `th11:0x41b593`–`0x41b66e`（<100 → 7 / <110 → 8 / <120 → 9 / ≥120 → 10）|
 * | `faith_hud_signal_strength()`（`A = min(⌊槽⌋,100)`，千分点）| **复刻** | `th11:0x41f640`（`signal_core_strength()` 的同一个式子，单测里逐值对账）|
 * | `faith_hud_mult_permille()` = **擦弹项 + 信号项** | **复刻**（用户指定按 TH11 一手）| `th11:0x41ce1f` `add edx,ebx`（擦弹项 + 交信度项）/ `th11:0x41f4e0` / `th11:0x41f640` |
 * | `faith_hud_mult_text()`（`"%1.2f"` 4 字符）| **复刻** | `th11:0x494198` 的 `%.2f`；值域 `≤ 9.99` ⇒ 恒 4 字符 |
 * | `faith_hud_combo_width_milli()`（满槽宽 = **75 px**）| ❓ **自定** | 原版 TH10 满槽 = **38 px**（`th10:0x470cb8` = 38/130）；用户 ② 要「连击槽长一些以托住行 2」⇒ 拉到行 2 的整宽 75 px |
 * | `FAITH_HUD_SIGNAL_NEUTRAL`（110 单装 ⇒ 1.00 + 满格）| 复刻（用户 ③）| 「满」= 显示 100.00（`th11:0x42364a` `cmp …,0x2710`）⇒ `A = 100` ⇒ `1.00`、档 10 ⇒ 宽 49 |
 * | `FAITH_HUD_COMBO_NEUTRAL`（111 单装 ⇒ 连击条满）| 复刻（用户 ③）| 满槽 = 130 帧（`th10:0x413035` `0x82`）|
 *
 * ⚠ **多卡口径**（用户 2026-09-15）：擦弹项与信号项**相加**（TH11 一手就是这么写的，
 *   `add edx,ebx`），**不是**「110 那份 + 111 那份各自算一套」。110 单装时「信号项」取中性 1.00
 *   （= 用户 ③ 的「不起作用」= 乘数为 1 的恒等），因此行 2 的倍率 = `擦弹项 + 1.00`。
 */
#pragma once
#ifndef FAITH_HUD_CORE_H
#define FAITH_HUD_CORE_H
#include <stdint.h>
#include "faith_core.h"

/* ---- 行 2：最大得点（= Δ_10 + Δ_11）---- */

/* 两槽的 Δ → **显示单位**的整数（各自 `÷FAITH_FIXED` 出定点再相加，用户 ④ 的口径）。
 * 入参是两个槽自己的 `faith_slot_t.delta`（`FAITH_FIXED = 100` 定点）。
 * ⚠ 传 `faith_slot_total()`（基础 + Δ）是**错的** —— 基础不进这块（用户 ④）。*/
int32_t faith_hud_delta_display(int32_t delta_110_fixed, int32_t delta_111_fixed);

/* 值截到 10 的倍数（**复刻**：`th11:0x41ce37` 的 `idiv 10` + `0x41ce43` 的 `sub edi,edx`）。
 * 例：50012 → 50010（TH11 原版 `"%.6d"` 打出来就是 `050010`）。*/
int32_t faith_hud_trunc10(int32_t v);

/* 行 2 要画的那 6 位数 = `trunc10(Δ_10 + Δ_11)`，夹 [0, `FAITH_HUD_MAXPOINT_MAX`]。
 * 上限的来历：TH11 是 `%.6d`（位数不够就往后写、不钳），而本仓行 2 是 **6 个固定 VM**
 * ⇒ 超 6 位必须钳（❓ 自定：钳在 `999990` = 6 位里最大的 10 的倍数）。*/
#define FAITH_HUD_MAXPOINT_MAX  999990
int32_t faith_hud_maxpoint(int32_t delta_110_fixed, int32_t delta_111_fixed);

/* 6 位数字（**复刻**：前导零照画），`d[0]` = 最高位。value 已被钳在 0..999990。*/
void faith_hud_digits6(int32_t value, int d[6]);

/* ---- 行 1：信号强度槽 ---- */

/* 显示值（= 一手 `⌊槽存储/100⌋`，0..130）。本仓槽是**千分点**（1 显示点 = 1000）。*/
int32_t faith_hud_signal_pct(int32_t signal_permille);

/* 档 0..10（**复刻** `th11:0x41b6a2`：`min(⌊槽存储/100⌋, 100) / 10`）。*/
int32_t faith_hud_signal_level(int32_t signal_permille);

/* 条的**像素宽** = `9 + 4×档` ∈ 9..49（**复刻** `th11:0x41b707`–`0x41b711`；单位 = 640 空间 px）。*/
int32_t faith_hud_signal_width_px(int32_t signal_permille);

/* 信号强度 `A`（**复刻** `th11:0x41f640`：`min(⌊槽存储/100⌋, 100)`），返回**千分点** 0..1000
 * （= 与 `signal_core_strength()` 同一个式子；单测里对 0..130000 逐值断言两者相等）。*/
int32_t faith_hud_signal_strength(int32_t signal_permille);

/* 档位闪色（**复刻** `th11:0x41b593`–`0x41b66e`）：0 = 常态白（中断 7）；
 * 1/2/3 = 中断 8/9/10（槽越满闪得越快）。分界是**显示值** 100 / 110 / 120。
 * ⚠ 只有 111 在场时才会用到（用户 ③：110 单装时**不闪烁**）。*/
int faith_hud_signal_flash(int32_t signal_permille);

/* ---- 行 2 的倍率（`×` 后面那 4 个字符）---- */

/* 倍率（千分点）= 擦弹项 + 信号项，夹 [0, `FAITH_HUD_MULT_MAX`]。
 * 两项都是千分点（`mof_core_graze_mult()` 给擦弹项、`faith_hud_signal_strength()` 给信号项）
 * ⇒ 相加即一手 `add edx,ebx`；上限 8990 + 1000 = 9990 = **9.99**（TH11 值域）。*/
#define FAITH_HUD_MULT_MAX  9990
int32_t faith_hud_mult_permille(int32_t graze_mult_permille, int32_t signal_strength_permille);

/* 倍率 → 4 个字符的文本 `"M.MM"`（`out` 至少 5 字节）。
 * **复刻** TH11 的 `"%1.2f"`；因为两项都是 10 千分点的整数倍 ⇒ 第三位小数恒 0，
 * 所以「截断」与 printf 的「四舍五入」逐值等价（单测钉住）。*/
void faith_hud_mult_text(int32_t mult_permille, char out[5]);

/* ---- 行 3：连击槽 ---- */

/* 条的宽度（**千分之一 px**，640 空间）= `combo × 75000 / 130`。
 * ❓ **自定**（唯一一处）：满槽 75 px 而不是 TH10 原版的 38 px —— 用户 ②「连击槽长一些以托住
 * 最大得点×信号强度数值」，而行 2 的整宽就是 75 px（6 位 42 + 间隔 1 + `×` 7 + 倍率 25）。
 * 比例仍是**线性**的（TH10 一手：`width = current × k`，`k = 38/130`），只是 k 变大。*/
#define FAITH_HUD_BAR_W_MILLI_FULL  75000
int32_t faith_hud_combo_width_milli(int32_t combo);

/* ---- 缺卡时的**中性值**（用户 ③）----
 * ⚠ 「卡在不在场」**不许**用「槽值恰好为 0」判定 —— 判据走 `faith.{h,c}` 的注册表
 * （`faith_registered_mask()`，见 `faith.h`）；本头只提供那几个**中性读数**。*/
#define FAITH_HUD_SIGNAL_NEUTRAL   100000  /* 显示 100.00 = TH11 的「满」⇒ 强度 1.00、条满 49 px */
#define FAITH_HUD_COMBO_NEUTRAL    FAITH_COMBO_FULL   /* 130 = 连击槽满（`th10:0x413035`）*/

#endif /* FAITH_HUD_CORE_H */
