/* faith_hud.h —— ★★ 110（风神录）/ 111（地灵殿）**组合 UI** 的共享实现（引擎侧）。
 *
 * 用户口径（2026-09-15 逐字，`NEXT.md` §2A）：
 *   ②「**组合 UI** 就是：一个**稍大的蓝点**，右侧分为**三行**，从上到下：**信号强度槽**、
 *      **最大得点×信号强度数值**、**连击槽**；**连击槽长一些以托住最大得点×信号强度数值**。」
 *   ③「只有 110 或只有 111 **都显示这一套 UI**，只是对应的那部分**不起作用**。
 *      只有 110 时：信号强度显示 **1.00**、信号**满格（不闪烁）**；只有 111 时：**连击条填满**。」
 *   ④「这里**只显示风神录 + 地灵殿的额外最大得点**……**所以这个地方是可以显示 0 的**。」
 *
 * ## 为什么是**共用件**（而不是 110 画一块、111 画一块）
 *
 * 用户定案（2026-09-12）：「111 的绿点和 110 的绿点**完完全全就是同一个东西**」⇒ 两块 HUD 也是
 * **同一块**：一个蓝点、一条信号槽、一行最大得点、一条连击槽。⇒ 这套东西必须由**一个**模块持有，
 * 谁在场谁驱动（`NEXT.md` §2A 第 3 条）。本模块就是那个「谁」：
 *   · **在场判据** = `faith_registered_mask()`（`faith.{h,c}` 的注册表 —— **不是**「槽值恰好为 0」，
 *     用户明令）；两张卡各自在 `on_load` 里 `faith_register()`、在 `on_delete`/`on_run_reset` 里
 *     `faith_unregister()`（两张卡原来就这么做）⇒ 本模块只**读**那张表。
 *   · **读数** = 两张卡每帧 `faith_hud_declare()` 自己那一槽的三个数（见下）。**卡之间不读对方状态**
 *     （`EVENTS-CONTRACT.md` §3 的硬要求）：各自只声明**自己**的 Δ / 连击槽 / 信号槽。
 *   · **VM 回收**：16 支 VM 全是本模块起的 ⇒ 收在 `faith_hud_departed()`（两张卡的 `on_delete`
 *     与 `on_run_reset` 都调它）。**最后一张卡走了才真收**（另一张还在时块与占位都不动 —— 用户 ③
 *     要求「只有一张也照常显示」）。
 *
 * ## 调用点（两张卡的接线，逐条见 `cards/mof.c` / `cards/signal.c` 的注释）
 *
 * | 时机 | 调用 |
 * | --- | --- |
 * | `on_load`（每关都会再跑一次）| `faith_hud_on_load()` —— 纪元推进时重新认领块（`HUD-LAYOUT.md` §4.4）|
 * | `on_tick_2`（每帧）| `faith_hud_declare(自己那一槽, …)` |
 * | `on_draw`（每帧；虚表 `+0x48`）| `faith_hud_draw()` —— 自带「菜单/标题门」与「一帧只画一遍」的守卫 |
 * | `on_delete` / `on_run_reset` | `faith_hud_departed()`（**在** `faith_unregister()` 之后调）|
 *
 * ⚠ **不许**在 `on_draw` 之外调 `faith_hud_draw()`（引擎的 UI VM / ASCII 环境在 `on_draw` 里才对，
 *   见 `SDK.md` §14）；也不许两张卡各画一半 —— 分工会立刻错位。
 *
 * ## 布局（640×480 空间，块左下角 = 原点，y **向上**；表面 px = ×2）
 *
 * ```
 *   y30 ┌──────────────────────────────────────────────┐
 *       │ 信号槽底/条 48×16 @(18,14)（TH11 裁切填充）      │
 *   y14 │ 6 位数字 @x18 步进 7 ｜ × @61 ｜ 倍率 @69..94   │ ← 墨迹上沿 y14
 *    y3 │ 连击槽：亮条 (18,1)+75×2 / 影 (19,0)+75×2       │
 *     0 └──────────────────────────────────────────────┘
 *        ↑ 蓝点 16×16 框 y1..17 / x0..16，本块放大到 18×18（❓「稍大」）
 * ```
 *
 * 块 = **94×30（640）= 188×60（表面）**（`engine/player/th11/04-merged-hud-spec.md` §1 的坐标表）。
 * 逐元素的**复刻 / ❓ 自定**判定与一手出处写在 `faith_hud.c` 的常量块与
 * `engine/player/th11/04-merged-hud-spec.md` §5 的表里。
 */
#pragma once
#ifndef FAITH_HUD_H
#define FAITH_HUD_H
#include <stdint.h>
#include "faith_core.h"
#include "faith_hud_core.h"

/* ---- 每张在场卡每帧**声明自己那一槽**的三个读数（只读共享；不改任何槽语义）----
 * ⚠ 本结构是**快照**，不是账本：`delta_fixed` / `combo` / `signal` 的换算与夹取全在
 *   `faith_core.*`（110/111 各自那份槽里），本模块一个字节都不改它们（用户的槽语义归卡自己）。*/
typedef struct {
    int32_t delta_fixed;   /* 本槽 Δ（`faith_slot_t.delta`，`FAITH_FIXED` 定点）—— 行 2 的加数 */
    int32_t combo;         /* 连击槽帧数 0..130（**只有 110 会动它**，一手 th10；行 3）*/
    int32_t signal;        /* 信号强度千分点 0..130000（**只有 111 会动它**；行 1 的条与数字）*/
} faith_hud_slot_t;

/* 声明自己那一槽的读数（`slot` = `FAITH_SLOT_110` / `FAITH_SLOT_111`；`v` = NULL 视作全 0）。
 * 每帧在 `on_tick_2` 里调一次。⚠ 它**不代表在场** —— 在场判据永远是 `faith_registered_mask()`。*/
void faith_hud_declare(int slot, const faith_hud_slot_t *v);

/* 关卡开场（`on_load`）：推进纪元时**重新认领**这一块（其它卡的认领也在这里发生 ⇒ 位置由货架定）。*/
void faith_hud_on_load(void);

/* 每帧画一次（任一在场卡从 `on_draw` 调；自带菜单门 + 帧守卫）。*/
void faith_hud_draw(void);

/* 一张卡退场（`on_delete` / `on_run_reset`；**必须在 `faith_unregister()` 之后调**）：
 * 还有另一张在场 ⇒ 只清它那一槽的声明（块与占位不动）；两张都走了 ⇒ 收掉 16 支 VM + 放占位。*/
void faith_hud_departed(void);

#endif /* FAITH_HUD_H */
