/* signal_core.h —— 地灵殿（card-expand id 111）的**纯逻辑**：信号槽（= TH11「通信强度」）的
 * 档位 / 强度映射 / 蓝点分公式 / 命碎与残机 / HUD 条的宽度。只 <stdint.h>，主机 gcc 直接测
 * （`tests/test_signal_core.c`）。
 *
 * ## 数值的**唯一**出处（2026-09-15 起全部换成 th11 v1.00a 一手）
 *
 *   | 面 | 一手出处 |
 *   | --- | --- |
 *   | 槽的载体 / 量纲 / 上下限 / 每帧四档 | `local/_scratch/research-th11-signal.md` §1 / §2 |
 *   | 擦弹按难度查表 | 同上 §3（4 个站点 + 4 张表原值） |
 *   | 归零（被弹 / 一局开始 / 登场 60 帧）| 同上 §4 |
 *   | 满槽全收（切 `state 3`）| 同上 §5.1 |
 *   | 蓝点分 = `trunc10((A+B)×Y/100)` | `local/_scratch/research-th11-faith-and-points.md` §2.2 / §2.3 |
 *   | 绿点三档 / 信仰初值上限 / miss | 同上 §3 / §4 / §6 |
 *   | 擦弹倍率 `min(⌊graze/100⌋,899)` | 同上 §2.1（`th11:0x41f4e0`），实现共用件 `mof_core_graze_mult()` |
 *   | 命碎 / 残机 / 得分跨阈值 | `local/_scratch/research-th11-bomb-lifefrag-hud.md` §4 **+ 本轮一手复核（订正了它）** |
 *   | 灵击伤害 / 帧数 / 判定框 | 同上 §2（`th11:0x433a90` 的 6 个调用点） |
 *
 * ## 本文件与共用件的边界（别把功劳算错）
 *
 *   | 量 | 谁实现 | 为什么 |
 *   | --- | --- | --- |
 *   | 信号槽的**按帧增减/擦弹增减** | 表在共用件 `faith_core.h`，**查表与档位在本文件** | 111 私有 |
 *   | 满槽阈值 / 强度映射 / 蓝点分 / 命碎 / 条宽 | **本文件** | 111 私有（SA 独有） |
 *   | 槽内加值 / 上限夹取 / 小绿点余数 / 信号落值 | `faith_core.c`（共用件，**不重写**） | 110 + 111 共用 |
 *   | 灵击门槛 / 扣档 / 下限 | `mof_core.c`（共用件，**不重写**） | 110 + 111 共用 |
 *   | ★ 魔理沙(1) 的爱丽丝变体（灵击 = 别人的一半 / P 点 +0.02+0.03 交替） | **本文件 ④** | 111 私有（**比例 ✅一手 + 档位 ❓映射**；不生效时原样转发上面那一行）|
 *   | 擦弹倍率 `min(⌊graze/100⌋,899)×10` 千分点 | `mof_core.c` 的 `mof_core_graze_mult()` | **111 用**（它是 th11 的式子；110 单装时不参与 PV）|
 */
#pragma once
#ifndef SIGNAL_CORE_H
#define SIGNAL_CORE_H
#include <stdint.h>
#include "faith_core.h"
#include "mof_core.h"   /* ★ 2026-09-15g：**只借类型**（`mof_power_t` / `mof_p_topup_t`）与那两个共用函数。
                         *   爱丽丝那条（④）的两个入口在 `alice_on == 0` 时**原样转发**给
                         *   `mof_core_power_spend()` / `mof_core_p_topup()` ⇒ 用同一个类型，卡侧一条路写下来、
                         *   单测也能逐个 `power` 与共用件对拍「非 Alice 时逐值不变」。
                         *   ⚠ **不重写**共用件的逻辑（本文件的地盘没有扩大：灵击扣法/门槛/下限仍归 `mof_core.c`）。*/

/* ---- 槽的量纲（表在 `faith_core.h`，这里只给别名，方便 111 侧读起来自洽）----
 * `signal` 是**千分点**（1 显示点 = 1000），上限 130.00 显示 = `SIGNAL_MAX`。
 * 一手：th11 的槽存的是 ×100 定点（0…13000 = 显示 0.00…130.00）⇒ 我们是它的 10 倍刻度、显示值一致。*/
#define SIGNAL_UNIT          1000          /* 1.00 显示 = 千分点 1000 */
#define SIGNAL_MAX           FAITH_SIGNAL_MAX      /* 130000 = 显示 130.00（一手硬上限 13000 存储）*/
#define SIGNAL_HI_MARK       FAITH_SIGNAL_HI_MARK  /* 100000 = 显示 100.00（一手「满」判据 0x2710）*/
#define SIGNAL_FULL_AT       FAITH_SIGNAL_HI_MARK  /* ★ 满（≥100.00）⇒ 把落下的道具切 state 3（全收）*/

/* ---- 绿点三档（SA 表；出处 §7.1 + th11 一手 §4.1）---- */
#define SIGNAL_GREEN_BIG     FAITH_SA_BIG        /* 大绿点 +100（= th11 item type 3：显示 +100）*/
#define SIGNAL_GREEN_MID     FAITH_SA_MID        /* 中绿点 +10（= th11 type 9：显示 +10）*/
#define SIGNAL_GREEN_SMALL_N FAITH_SA_SMALL_N    /* 小绿点**每 100 个** +10（= th11 type 8 的 ÷100 口径）*/
#define SIGNAL_GREEN_SMALL_V FAITH_SA_SMALL_V

/* ---- 槽推进（一手 §2 / §3）---- */
/* 本帧擦弹 → 槽的增量（千分点）。`difficulty` = TH18 的 `0x4ccd00`（0 EASY … 4 EXTRA）；
 * `is_laser` = 激光擦弹档（一手更慢的表）。⚠ TH18 的原生擦弹计数**不区分来源** ⇒ 卡侧恒传 0
 * （激光表具名备用，见 `faith_core.h` 与 `TUNING.md`）。⚠ 越界难度：子弹表按 NORMAL、激光表按 LUNATIC。*/
int32_t signal_core_graze_gain(int difficulty, int is_laser);
/* 「自机离回收线多少 px」→ 每帧档位（`FAITH_SIGNAL_BAND_*`）。
 * 入参 = 偏移的 **1/100 px 整数**（卡侧写 `(int32_t)((poc_height − player.y) * 100.0f)`；
 * 非负 ⇒ 截断即向下取整）。一手分界（线上 > 64 px / 48…64 px / 0…48 px / 线下）见 §2.2 的
 * `64.0f`、`80.0f`、`128.0f`（线 = 128.0f），边界比较逐条对齐（恰好 64/48/0 ⇒ 落在下一档）。*/
int     signal_core_band(int32_t above_line_px_x100);
/* 本帧信号增量（千分点；可正可负）。`graze` = 本帧**新增**的擦弹数（原生 `GRAZE 0x4ccd0c` 的帧差）。
 * = `faith_signal_step(signal_now, band, graze × signal_core_graze_gain(difficulty, 0))`。*/
int32_t signal_core_step(int32_t signal_now, int band, int32_t graze, int difficulty);

/* 落到槽里（夹 [0, SIGNAL_MAX]），返回落值后的 signal。 */
int32_t signal_core_apply(faith_slot_t *s, int32_t d);
int32_t signal_core_apply_step(faith_slot_t *s, int band, int32_t graze, int difficulty);
/* 被弹 / 关卡结束 / 一局开始归 0 ⇒ 返回 1（让调用方知道发生了复位，便于日志）。 */
int     signal_core_reset(faith_slot_t *s);

/* ---- 绿点收集入账（§2.2 / §7.1 / §7.3）—— 卡侧 `signal_on_pickup` 的**判定体** ----
 * 一颗**实体**绿点被收走时调用（`faith.c` 的 `dot_collect()` 带 `tier` 与 `value` 回调过来）：
 *   · 大绿点（击破通道，+100）⇒ 卡侧传 `FAITH_EV_BIG`；
 *   · 中绿点（满 P 后吃 P 点，+10）⇒ 卡侧传 `FAITH_EV_GREEN`。
 * 加值与上限夹取（Δ_111 上限 = 原版基础）在 `faith_slot_add()` 里，本函数不重写。
 * ★ 2026-09-15：**不再动连击槽**（`faith_combo_add`）—— th11 一手里没有连击槽
 *   （信仰写入点穷举 = 初值 / 三档绿点 / miss / 读档，`research-th11-faith-and-points.md` §6.1）。
 * 返回**定点（×FAITH_FIXED）**的入账量（= `faith_slot_add()` 的返回值；夹到上限时返回 0）。*/
int32_t signal_core_pickup(faith_slot_t *s, faith_ev_t ev, int32_t value);

/* 夹取（纯函数，便于单测边界）。 */
int32_t signal_core_clamp(int32_t v);
/* 满槽（≥100.00）⇒ 触发「上线回收式全收」（一手 §5.1：道具切 state 3）。 */
int     signal_core_is_full(int32_t signal_now);

/* ---- 信号强度 → 蓝点分公式里的那一项 `A`（一手 §5.2 / §2.3）----
 * `A = min(⌊槽存储/100⌋, 100)`（`th11:0x41f640`），单位 0.01 ⇒ 0.00…1.00，满 = 100。
 * 本函数返回**千分点**（`A × 10`，0…1000）—— 与 `mof_core_graze_mult()` 的 `B × 10` 同刻度。
 * ⇒ 乘数项 `A + B`（单位 0.01）在千分点下就是 `strength + graze_mult`。
 * ⚠ 槽到 130.00 仍是 1.00（一手 `th11:0x41f64c: mov eax,0x64`），130 只体现在 HUD 的格数上。*/
int32_t signal_core_strength(int32_t signal_now);

/* ---- 蓝点分（★ 一手整数式）----
 * 一手（`th11:0x423b7c` 未满分支 / `th11:0x423be8` 已满分支，研究文档 §2.2/§2.3）：
 *
 *     Y = trunc10(⌊CURRENT_PIV / 100⌋)          ← 信仰**显示值**（HUD 上那个 6 位数；`th11:0x41f510`）
 *     A = min(⌊交信度存储 / 100⌋, 100)           ← 信号项，单位 0.01（`th11:0x41f640`）
 *     B = min(⌊GRAZE / 100⌋, 899)               ← 擦弹项，单位 0.01（`th11:0x41f4e0`）
 *     S = trunc10( (A + B) × Y / 100 )          ← 弹窗数字 = 显示得分增量（`th11:0x423bc5/0x423bc8`）
 *     若 S ≤ 0 ⇒ **S = 10**（`th11:0x423bca: test ebx,ebx / jg` + `0x423bce: mov ebx,0xa`）
 *
 * ⇒ **每一步都是整数**（`0x51eb851f`+`sar 5` = ÷100、`0x66666667`+`sar 2` = ÷10、
 *   `lea [eax+eax*4]; add ebx,ebx` = trunc10）。exe 里**没有** `0.0001f` / `8.99f` —— 谁再提浮点写法，
 *   就是又在扫错靶子（研究文档 §2.3 的「真身表」）。
 *
 * 我们槽里的单位：`faith_total` 是**显示值**（base = 25000 这种、Δ = 绿点的显示增量）
 * ⇒ `Y` 直接由它 trunc10 得到（不需要 ÷100 那一层）。
 * 入参 `graze_mult_permille` = `mof_core_graze_mult(graze)`（= `B × 10`）；
 * `signal_now` = 槽的千分点（内部换成 `A`）。返回值 = S（**显示分**）。*/
int32_t signal_core_bonus(int32_t faith_total, int32_t graze_mult_permille, int32_t signal_now);
/* 定点版：`faith_fixed` = ×FAITH_FIXED，返回**同样定点**的 S（= 显示分 × FAITH_FIXED）。*/
int32_t signal_core_bonus_fixed(int32_t faith_fixed, int32_t graze_mult_permille, int32_t signal_now);

/* ---- 契约 PV 总式（EVENTS-CONTRACT.md §3）—— 旧口径的回归面 ----
 * ⚠★ **2026-09-13 起不参与落笔**：计分层改成「各卡声明自己那一族的加数 + 一次合成」
 *   （`../../SCORE-LAYER-REWORK.md`），111 只声明「Δ_11」与信号强度。两个函数保留 = 回归断言。
 * ⚠★ **2026-09-15**：`signal_core_pv_111_scaled()` 已按一手改成**同一个 S 式**（trunc10 + 下限 10），
 *   而共用件 `mof_core_pv()` 仍是旧乘式（`base × mult / 1000`，**没有** trunc10 / 下限 10）
 *   ⇒ 两者**不再逐字相等**（`tests/test_signal_core.c` §12.6 已把跨卡等式换成一手断言）。
 *   `mof_core_pv()` 不归本轮（110 共享件）⇒ 需要它的 owner 对齐（报告里列了）。*/
int32_t signal_core_pv_111(const faith_slot_t *s111, int32_t aggregate, int have110);
/* 同上 + 乘数 ⇒ 完整 PV（= 一手那个 S）。*/
int32_t signal_core_pv_111_scaled(const faith_slot_t *s111, int32_t aggregate, int have110,
                                  int32_t graze_mult_permille, int32_t signal_now);

/* ---- Miss（一手 `th11:0x4327d0` 段）----
 * `Δ -= 显示 100,000`（一手 `sub eax,0x989680` = −10^7 存储 = 显示 −100,000），**下限 0**
 * （原版把地板写成该难度初值 `MIN_PIV`；我们的 Δ 从 0 起算 ⇒ 夹 0 就是同一件事）。
 * 返回值 = 真正扣掉的**显示值**（0..100,000）。同一时刻卡侧还要 `signal_core_reset()` 归零信号槽
 * （一手 `th11:0x432807` 同一个 miss 函数里写槽 = 0）。*/
int32_t signal_core_miss_penalty(faith_slot_t *s);

/* ---- 命碎 / 残机（★ 2026-09-15 一手复核，**订正了研究文档 §4**）----
 * 一手（`objdump -d -M intel local/th11.v1.00a/th11.exe`，本轮自己读了这三段）：
 *
 *   `th11:0x420950` = **残机 +1（1UP）**：`mov eax,ds:0x4a5718 / inc eax / cmp eax,0x9 /
 *                     mov ds:0x4a5718,eax`（钳 9）+ 音效 + `call 0x41a060(gui, …)` 刷 HUD。
 *                     ⇒ **`0x4a5718` = 残机数**（它被当「要画的残机数」传给 HUD —— 残机图标 9 格）。
 *   `th11:0x4209b0` = **命碎 +1**：`[0x4a571c]++`；`th11:0x4209ca: cmp eax,0x5` ⇒ **5 碎 = 1 残**
 *                     （满 5 归零 + `call 0x420950`）；残机已满（`0x4a5718 >= 9`）⇒ 碎清零、不加。
 *                     ⇒ **`0x4a571c` = 命碎数**。研究文档 §4.1 把这两个地址的名字**写反了**。
 *   两条发放路径（研究文档 §4.2 的路径判定对，**道具类型写错**）：
 *     ① **得分跨阈值**：`th11:0x41efd8`/`0x41efe3`（表 `th11:0x4a3b1c`）/ EX 表 `th11:0x41eff8`
 *        （表 `th11:0x4a3d60`）→ `th11:0x41f001: call 0x420950` = **发 1 条残机（extend）**，并把索引 `0x4a5750` +1。
 *        ⚠ 阈值表**没能取到**：`0x4a3b1c` 起只有第 0 项可读 = `0x3b9aca00`(10^9)，其后是颜色/函数指针；
 *        而按「1e9, 0, …」照读会得到「第 2 项 = 0 ⇒ 每帧都发残机」这种荒谬行为 ⇒ **那张表不在那儿**。
 *        ⇒ 本仓只落**结构 + 已知那一项**，标 ❓（`TUNING.md`）。**它同时是「表未取到」的自证**。
 *     ② **捡命碎道具**：一手是 **item type 5**（跳表 `th11:0x423f00[4] = th11:0x423c3e`）→ `call 0x4209b0`；
 *        而 **type 7**（`0x423f00[6] = th11:0x423d97`）调的是 `0x420950` = **1UP**（不是命碎）。
 *
 * 所以本卡对「命碎」的落地：
 *   · 路径 ① 由**我们**实现（TH18 原生没有得分奖残）⇒ `signal_core_lifefrag_score()` → `ce_add_life()`；
 *   · 路径 ②（**捡命碎道具**）也由**我们负责撒** —— 见下一段（★ 2026-09-15e 订正）。*/

/* ---- ★★ 2026-09-15e：命碎的**载体**与**撒道具**（订正本文上一版把载体写成 `item type 4`）----
 *
 * ## TH18 侧的载体 = **item type `0x10`**（三条一手，本轮全部自己复核过）
 *   ① `engine/card/th18/05-shop-and-money.md` §8 + §4.1：道具 type `0x10`–`0x13` 走
 *      `allocate_new_card(ABILITY_MANAGER_PTR, ITEM_TYPE_TABLE_sprite_and_card[type].card_id, 0)`
 *      （一手 dump 表 `0x4b4020`：`0x10 → card_id 3` = **EXTEND2 残机碎片**）；
 *      而 **EXTEND2/4 的 `entry+0x14 == 6` ⇒ 永远不进商店随机池** ⇒ **「整」只能买、「碎」只能掉**。
 *   ② `native/cards/collect2x_core.h` 的既有结论：`C2X_FRAG_LIFE = 0x10`，并把 **type 4/6 判为
 *      「旧引擎血统残留」**（114 那张卡当初实测踩过）。
 *   ③ 本轮**自己复核**（`local/_scratch/th18_full.asm` 全量）：
 *      · `0x446f40`（`ItemManager::spawn_item`，`sdk.h` 的 `CE_FN_SPAWN_ITEM`）**全 exe 9 个调用点**
 *        逐个看过，压进去的 type 只有 **立即数 7 / 2 / 9 / 2 / 4 + 两种变量**
 *        （`enemy+0x623c` 主掉落、`ex_drop[i]+1`）⇒ **exe 里没有任何一处写死撒 `0x10`**；
 *        `0x10` 只能从 **ECL 掉落表**进来（`0x43061f` 的 `lea eax,[ebx+1]` 那条循环 + `dropMain`）。
 *      · 把 TH18 全部 21 个 `st*.ecl` 解出来（`thecl -d 18 -x`）扫 `dropMain`/`dropExtra`：
 *        **`dropMain(0x10)` 全游戏只有一处** —— `st04mbs.ecl` 的 **`MBossEscape()`**
 *        （`0x15a4: ins_510(16)`，紧跟 `dropExtra(1,20)`/`dropExtra(2,20)`/`dropArea`/`dropItems()`）。
 *        ⚠ 顺带一个漂亮的同形证据：TH11 的 `MBossEscape` 也在收尾 `dropMain(5)`（stage02/03/04/05）。
 *      ⇒ **「TH18 的原生命中碎制已经在承担 TH11 那条路」不成立**：原生**只在关卡 4 道中 boss 逃脱**
 *        这一处掉命碎道具，boss 的每个攻击段一个都没有（TH18 的 `st0Xbs.ecl` 里 `ins_510` 出现 **0 次**）。
 *        ⇒ 每段撒道具这件事**必须由本卡做**（旧注释按此改写，见 `signal.c`）。
 *
 * ## 吃到之后的一切**全走原生**（卡不碰碎片、不碰残机）
 *   吃到 type `0x10` → `allocate_new_card(…, 3, 0)`（mode 0 只调 ctor）→ **`CardLifeFragment` ctor
 *   `th18:0x457570`**：`[card+0x70]++`，难度 `0x4ccd00`（`== 4` 走 EX 表）选阈值表
 *   `0x4b40c8`（通常）/ `0x4b3ff4`（EX），满阈值 → `call 0x4575f0` 加一命 + `0x441f10` 刷 HUD。
 *   ★ 阈值表本轮 read 实测：**通常表 12 项全是 3**；EX 表前 8 项是 3（其后是哨兵值）
 *   ⇒ 与既有结论「3 个碎片 = 1 残」一致。**⇒ 卡只负责撒道具**，别的什么都不写。
 *
 * ## 比例（★ **用户口径（2026-09-15 逐字），不是复刻**）
 *   用户：「**算了，数值平衡一下，每两次这样掉 1 个命碎**」—— 一手是 **1:1**（每段一次机会）。
 *   ⇒ 本卡按 **每 2 段**：`SIGNAL_SEG_DROP_EVERY`。**想改回一手 1:1 只要把那个宏改成 1**
 *   （`signal_seg_tick()` 里没有任何别的假设；余数跨段保留）。
 *   ⚠ 一手还要求「掉的是**道具**、捡到才 +1」—— 这一条我们**照做**（原生链自己管捡到之后）。 */
#define SIGNAL_ITEM_TYPE_LIFEFRAG   0x10 /* ★ TH18 item type 0x10 = 命碎（EXTEND2 卡道具 → CardLifeFragment id 3）
                                          *   ↔ th11 的 item type 5。⚠ 旧值 4 是错的（type 4 连 TH18 的道具
                                          *   分派 switch 里都没有：05-shop-and-money §8 的表没有它）。*/
#define SIGNAL_ITEM_TYPE_1UP        5    /* TH18 item type 5 = 整命（↔ th11 的 type 7）*/
/* TH11 的碎片换算（一手 `th11:0x4209ca`）；TH18 侧是 3（原生），列在这里只为把两条口径摆在一起。*/
#define SIGNAL_LIFEFRAG_PER_LIFE_TH11  5
#define SIGNAL_LIFEFRAG_PER_LIFE_TH18  3
/* 得分阈值表（一手 `th11:0x4a3b1c` / EX `th11:0x4a3d60`）——**只取到第 0 项**，❓ 见上。*/
#define SIGNAL_LIFEFRAG_TABLE_LEN   1
int32_t signal_core_lifefrag_table_len(void);
/* 第 idx 项的阈值（`ex` = 1 用 EX 表）；越界返回 −1（= 表中没有这一项 ⇒ 不再发）。*/
int64_t signal_core_lifefrag_threshold(int32_t idx, int ex);
/* 得分状态机：`score` = 本帧分数、`*idx` = 已发到第几项（卡侧持有，**一局内不重置**：
 * 一手在一局开始时才清 `0x4a5750`（`th11:0x41faf7`），而我们的 ctor 每关都会跑 ⇒ 刻意不在 ctor 里清）。
 * 返回 1 = 本帧跨过下一级阈值（调用方发 1 条残机，并把 `*idx` 推进一格）。越界后恒返回 0。*/
int     signal_core_lifefrag_score(int64_t score, int32_t *idx, int ex);
/* 这颗道具是不是「命碎道具」（TH18 item type `0x10`；见上面那一段的一手）。*/
int     signal_core_is_lifefrag_item(uint32_t type);

/* ======================================================================================
 * ★★ 2026-09-15e：命碎的**段机**（「每 N 段没死过 ⇒ 撒 1 颗」）
 * ======================================================================================
 * ## 一手要的形状（TH11 v1.00a，md5 `6af1e3b37f28293f4fc127ee01d2632c`）
 *   · **条件** = 本段攻击内**玩家没死过**：ECL `unless ($CAPTURE == 1) goto 跳过; dropMain(5);`
 *     `$CAPTURE` = `enemy_manager+0x18`（`th11:0x4173fa`），**引擎只在「决死窗口过期、真死」时清 0**
 *     （`th11:0x43124a`，同处 `$MISS_COUNT++`）；**灵击不清、决死救回也不清** ⇒ 我们的判据 = 「这一段内**命数减少过**」。
 *   · **时点** = **每段攻击的收尾**（不在段首、不是每帧）。
 *   · **粒度** = **boss 的一个攻击相位**：★ 本轮用 `thecl -d 11 -x` 把 11 的 ECL 全量解出来、
 *     把 46 处 `dropMain(5)`（+14 处 `dropExtra(*,5)`）按**所在子程序**归类，结果是
 *     **全部落在 boss / 道中 boss 的子程序里**：`Boss2/Boss3/…`（非符）、`BossCardN`（符卡）、
 *     `BossDead`、`MBossN/MBossCardN/MBossDead/MBossEscape`（另 stage04 `Girl3Dead`），
 *     **普通妖精波一个都没有**。⇒ 「段」= boss 的一个攻击相位（一个非符一段、一张符卡一段）。
 *     ⚠ 这同时说明：**道中不刷 boss 的妖精波本来就不掉**（不是我们漏做）。
 *
 * ## 段边界怎么观测（❓ 自定：TH18 没有「段」这一级对象，边界是**我们重建**的）
 *   上游事实（一手）：TH18 的 ECL 524 `setChapter` **粒度只到「整场 boss 战 / 关卡段」**
 *   （`engine/chapter/th18/01-chapter-residuals.md` §3：关底 boss 全程就 `43` 一个值），
 *   所以**章号**切不出非符/符卡这一级。可用的原生信号有三个（全部只读）：
 *
 *   | 信号 | 一手出处 | 变化时意味着 |
 *   | --- | --- | --- |
 *   | boss 在场（`em+CE_EM_BOSS_IDS` 命中）| 同 `lolk.c` 的 `boss_enemy()`（`season.c` 起的老口径）| 1→0 = 最后一段收尾（TH11 的 `BossDead` 块也掉）|
 *   | 符卡位 `CE_SPELL_FLAGS` bit0 | `engine/card/th18/02-wave-signals.md` §1（置 `0x42a320` / 清 `0x42a780`）| 边沿 = 符卡段的起 / 收 |
 *   | 相位槽指纹（活动 `zEnemyInterrupt`）| `engine/chapter/th18/02-wave-signals.md` §2/§3；`zEnemy+0x6374`、stride `0x88`、`{hp_value,time,sub_life,…}` | 变了 = 换相位（非符 ↔ 符卡 ↔ 下一段）|
 *
 *   ⇒ **段边界 = 这三个信号的任一跳变**（用 `SIGNAL_SEG_DEBOUNCE` 帧窗口把「槽清空 → 新槽 → 符卡位翻转」
 *   这一串同一次切换的边沿合并成一次）。判据形状照 `cards/lolk_core.c` 的 `lolk_tick()` 的波边界
 *   （同一批一手信号的既有用法，⚠ 本文件**不 include 也不改** 115 的 core）。
 *   ⚠ **相位指纹只在「有活动槽」时更新**（同 lolk）：槽被清空那一帧不算边界 ⇒ 「清空 → 新槽」只报一次。
 *
 *   ### 这是**近似**，代价写在这里（❓ 自定，已登记 `TUNING.md` §2.1e）
 *     ① **同一次切换的多余边沿被吞 5 帧**：真 boss 的相邻相位间隔远大于 5 帧 ⇒ 不会误吞两段；
 *        但「相位 < 5 帧」的极端脚本会少算一段（原生 8 关的相位都 ≥ 数百帧）。
 *     ② **指纹逐位相同**的两个相邻相位认不出来（`st01bs` 的 4 个相位指纹互不相同；8 关未穷举）。
 *     ③ **非 boss 的妖精波不算段** —— 这一条与一手**同粒度**（见上），不是近似。
 *     ④ 道中 boss 的「逃脱（Escape）」在我们这里是「boss 消失 ⇒ 段收尾」；TH11 的 `MBossEscape`
 *        也掉（stage02/03/04/05 各一处）⇒ 同向。
 *
 *   ### ★★ 2026-09-16 补：**对话中不结算、只吸收**（用户实跑：「一面结束对话后进关底直接就掉了一个残碎片」）
 *   症状机制（实跑日志可复现）：关底 boss 被击破 → **对话**（`mof/faith: … boss=1 dlg=1`）→
 *   boss 对象随后被释放 ⇒ 我们原来的三个信号里「boss 1→0」被判成**又一次段收尾**
 *   （日志 `signal/frag: … open 1 … pending 1/2` → `signal: 命碎 -> … 已结算 2 段`）
 *   ⇒ 多算一段 ⇒ 攒够 2 段撒出一颗。**一手里那个「收尾」是 `BossDead` 子程序自己的掉点**
 *   （ECL `dropMain(5)`，在对话之前就跑完了），对话期间没有任何 `dropMain` ⇒ 我们也必须在对话里闭嘴。
 *   ⇒ 判据：`in->dialogue != 0` 时**只更新记忆（`last_*`）、不结算也不开新段**（并把当前段直接作废、
 *     不置 `pending`）。这样「对话里发生的一切信号变化」被**吸收**掉：对话结束后不会补一次段收尾。
 *
 * ## 「本段没死过」怎么判（卡侧供给，判据进 core 只为可测）
 *   `died_this_frame` = **本帧命数（`CE_CURRENT_LIVES`）比上一帧少**（`signal.c` 既有的 `last_lives` 差分）。
 *   救援层语义天然对上：**107 / 117 / 113 救你一命 ⇒ 命数不减 ⇒ 记为「没死」**
 *   （`EVENTS-CONTRACT.md` §5.2：救援发生在同一帧的死亡确定之前；TH11 一手同向 —— **决死不触发
 *   `$MISS_COUNT++` / 不清 `$CAPTURE`**，见 `local/_scratch/research-th11-fragments-and-type8.md` §Q3）。
 * ==================================================================================== */
#define SIGNAL_SEG_DROP_EVERY   2     /* ★ **用户口径（不是复刻）**：每 2 段「没死过」⇒ 1 颗。
                                       *   一手是 1:1 ⇒ **改成 1 就是复刻一手**（只改这一个宏）。*/
#define SIGNAL_SEG_DEBOUNCE     5     /* ❓ 自定：相位切换的多余边沿合并窗口（帧；值取 115 的同名常量）*/
#define SIGNAL_FRAG_ITEM_TYPE   SIGNAL_ITEM_TYPE_LIFEFRAG   /* 撒出去的道具 type（= 0x10）*/

/* 接线层每帧填进来的原生只读信号（判据本体在 core ⇒ 主机单测钉得住）。*/
typedef struct {
    int32_t boss_present;  /* 场上有 boss（boss id 表命中）*/
    int32_t spell_active;  /* `CE_SPELL_FLAGS` bit0：符卡进行中 */
    int32_t phase_sig;     /* 活动相位槽指纹；**0 = 没有活动槽**（不是「指纹 0」）*/
    int32_t dialogue;      /* ★ 2026-09-16：对话 VM 非 0（`CE_GUI_MSG_ACTIVE()`）。见下面「对话中」那一段 */
} signal_seg_in_t;

/* ★★ 2026-09-16m：**段太短就不算一段**（用户两次实跑「对话结束后发残碎片」）。
 * 真段（攻击相位）都是几百帧量级；对话结束/进出场会在几帧里造出一次「开段 → 立刻收尾」的假边沿，
 * 而假段必然「没死过」⇒ 每两次就白撒一颗命碎。段长 < 本值的**只记诊断、不记账**。
 * ❓ 自定（100 帧 ≈ 1.7 s）：远大于任何边沿抖动（去抖窗口 8 帧），远小于最短的真相位。*/
#define SIGNAL_SEG_MIN_FRAMES 100

/* 段机状态（一局内累计；`on_delete` / `on_run_reset` 收干净）。*/typedef struct {
    int32_t open;        /* 本段开着（boss 在场 / 相位有效）*/
    int32_t is_spell;    /* 本段是符卡段（诊断 / 日志）*/
    int32_t died;        /* 本段内自机掉过命 */
    int32_t pending;     /* 已攒够的「没死段」数（0..`SIGNAL_SEG_DROP_EVERY`−1）—— **余数跨段保留** */
    int32_t segs;        /* 诊断：结算过的段数 */
    int32_t segs_clean;  /* 诊断：其中「没死过」的段数 */
    int32_t drops;       /* 诊断：因满 `SIGNAL_SEG_DROP_EVERY` 而应该撒的颗数 */
    int32_t last_sig;    /* 上一帧相位指纹（只在非 0 时更新，见上）*/
    int32_t last_spell;  /* 上一帧符卡位 */
    int32_t last_boss;   /* 上一帧 boss 在场 */
    int32_t debounce;    /* 边沿合并倒计时 */
    int32_t frame;       /* 已跑帧数（诊断）*/
    int32_t start_frame; /* ★★ 2026-09-16m：本段开始时的 `frame`（用来算段长）*/
    int32_t last_len;    /* ★ 诊断：上一段的长度（帧）；-1 = 还没结算过*/
} signal_seg_t;

/* 事件位掩码：一次 tick 可能同时「收一段 + 开一段」。*/
#define SIGNAL_SEG_EV_NONE   0
#define SIGNAL_SEG_EV_START  1
#define SIGNAL_SEG_EV_END    2

/* 每帧推进。`died_this_frame` = 本帧自机命数减少（非 0 = 本帧死了一次）。
 * `out_drop`（可为 NULL）= **本帧该撒一颗 `SIGNAL_FRAG_ITEM_TYPE` 道具**（1 = 撒）。
 * 位置 / 角度 / 速度**不在这里**（core 不碰引擎）：它们由卡侧按一手取（见 `signal.c` 的 `signal_seg_drop()`）。
 * ⚠ `g == NULL` / `in == NULL` 安全（返回 `SIGNAL_SEG_EV_NONE`、`*out_drop = 0`）。*/
int32_t signal_seg_tick(signal_seg_t *g, const signal_seg_in_t *in, int died_this_frame, int32_t *out_drop);
/* 归零段机（一局开始时调；`g == NULL` 安全）。⚠ **余数 `pending` 也一起清**——
 * 一手没有「跨局保留」这回事（命碎计数 `th11:0x4a571c` 是每局一份）。*/
void    signal_seg_init(signal_seg_t *g);
/* 换关开场用：**只重挂边沿记忆**（清 `open` / `died` / `is_spell` / `last_*` / `debounce`），
 * **保留** `pending` 余数与全部诊断计数。理由：换关时上一关的 boss 与相位槽都没了，
 * 留着旧指纹会在本关第一个相位上误报一次「段收尾」（并可能白送一颗命碎）；而余数按**段**保留。
 * `g == NULL` 安全。*/
void    signal_seg_rebase(signal_seg_t *g);

/* ---- HUD：连续填充条的宽度（一手 §6.1(b)，`th11:0x41b679`–`0x41b711`）----
 * `level = min(⌊槽存储/100⌋, 100) / 10` ∈ 0..10；**`宽(px) = 9.0 + 4.0 × level`**
 * （一手常量 `th11:0x498230` = 4.0、`th11:0x498098` = 9.0；槽 0 → 9 px、槽 ≥100.00 → 49 px）。
 * ⚠ 这是**贴图宽度**（不是屏幕 scale）：卡侧把它除以贴图内容宽得到 `scale.x`。
 * ⚠ 档位图标那三个阈值（100 / 110 / 120 → `front.anm` script 7/8/9/10）在卡侧
 *   （`signal.c` 的 `signal_gauge_level_int()`）—— 它管的是**中断**、不是宽度。*/
#define SIGNAL_BAR_BASE_PX    9
#define SIGNAL_BAR_STEP_PX    4
#define SIGNAL_BAR_LEVEL_MAX  10
int32_t signal_core_bar_level(int32_t signal_now);      /* 0..10 */
int32_t signal_core_bar_width_px(int32_t signal_now);   /* 9..49 */

/* ======================================================================================
 * 「支援特技」六条（TH11 组合号 = `3×CHARACTER + SUBSHOT`，`th11:0x43064c`–`0x43064f`）
 * ======================================================================================
 * ## 判据/数值的唯一出处（**全部一手**，别在这里发明）
 *   · 输入面（位掩码 / `0x400` 组合位 / 边沿工具）：`engine/input/th11/01-special-detection.md`
 *   · 效果侧（缺口状态机 / 萃香全收 / 文的速度链路 / 帕秋莉 formation / 子机数组）：
 *     `engine/player/th11/02-support-special-effects.md`
 *   · 逐条反汇编（含否定证据）：`local/_scratch/research-th11-support-special-input.md`
 *   · TH18 侧的缺口原生入口：`engine/player/th18/04-warp-yukari-gap.md`（`th18:0x409a60`）
 *   · TH18 侧的速度旋钮：`CE_PLAYER_SPEED_MULT`（engine.h；`th18:0x45c702` 每帧复位 1.0）
 *   · TH18 侧的输入面位掩码：`engine/input/th18/01-key-mapping.md` §2 + 本仓逐条复核
 *     （`0x40a1d9 test [0x4ca428],0x40` = 左 / `0x40a1f1 …,0x80` = 右 / `0x45eac6 …,0x10` = 上 /
 *      `0x45ea7d …,0x20` = 下 / `0x45ea06 …,0x1` = 射击）
 *
 * ## 用户拍板的映射（**照做，不是复刻**）
 *   TH18 灵梦（机体 0） = Σ 三只 TH11 灵梦伙伴（紫 + 萃香 + 文），三条同时生效；
 *   TH18 魔理沙（机体 1）= Σ 三只 TH11 魔理沙伙伴（爱丽丝 + 帕秋莉 + 荷取），三条同时生效。
 *   ⚠ 所以下面每条特技都带自己的「机体门」（`SIGNAL_CHAR_*`），**不是**「TH18 的机体 = TH11 的机体」。
 *   咲夜（2）/ 早苗（3）本轮不做。
 * ==================================================================================== */

/* ---- TH18 机体号（`CE_ADDR_CHARACTER` 的取值域）---- */
#define SIGNAL_CHAR_REIMU     0
#define SIGNAL_CHAR_MARISA    1
#define SIGNAL_CHAR_SAKUYA    2
#define SIGNAL_CHAR_SANAE     3

/* ---- TH18 输入位（`CE_ADDR_INPUT_HELD` / `CE_ADDR_INPUT_PRESSED`；见上面的出处）----
 * ⚠ 方向位与 TH11 的 `0x4c93c0` **布局相同**（TH10/TH11/TH18 三作同形），但 `0x400` 那个
 *   「射击+↑」组合位是 **TH11 特有的合成位**，TH18 **没有** ⇒ 帕秋莉那条必须自己在卡侧合成（见下）。*/
#define SIGNAL_IN_SHOT       0x001u   /* Z（射击）*/
#define SIGNAL_IN_BOMB       0x002u   /* X（灵击）*/
#define SIGNAL_IN_FOCUS      0x008u   /* Shift（低速）*/
#define SIGNAL_IN_UP         0x010u
#define SIGNAL_IN_DOWN       0x020u
#define SIGNAL_IN_LEFT       0x040u
#define SIGNAL_IN_RIGHT      0x080u
/* 四个方向位（`attempted_direction != 0` 的同义物）*/
#define SIGNAL_IN_DIRMASK    (SIGNAL_IN_UP | SIGNAL_IN_DOWN | SIGNAL_IN_LEFT | SIGNAL_IN_RIGHT)

/* ★★ TH11 判据里那个反复出现的 `INPUT & 0x9` = **射击(bit0) | 低速(bit3)** ——
 *   ⚠ **本轮（2026-09-15）订正**：`local/_scratch/research-th11-support-special-input.md` §1.2 与
 *   `engine/input/th11/01-special-detection.md` 的位表把 `0x9` 读成了「上|下」、把低速写成 `0x4`
 *   （`0x43036d: shr eax,3; and eax,1` 的 `shr 3` 取的是 **bit3 = 0x8**，不是 bit2）。三条一手复核：
 *     · `th11:0x43036d` 的 `shr eax,3 / and eax,1` 写的 `player+0x8d20` 就是 `is_focused`；
 *     · `th11:0x43603f: or eax,0x8` —— bit3 由「**射击键按住 ≥ 8 帧**」派生
 *       （计数器 `th11:0x4c93bc`，`0x436034`/`0x436042` 钳到 8）⇒ bit3 就是低速位；
 *     · 方向位是 `0x10/0x20/0x40/0x80`（`th11:0x430310`–`0x430340` 的解码器，本轮逐条重读），
 *       `0x9` 里**没有任何方向位**；
 *     · 独立佐证（名字与偏移双源）：ExpHP 把两个计数器命名为 `reimu_b_frames_without_input` /
 *       **`reimu_c_frames_without_z_or_shift`** —— 后者直译「**没按 Z 也没按 Shift**」。
 *   ⇒ 紫的**取消**条件 = 「按了射击或低速」（不是「按了上/下」）；萃香/文的判据 = 「**没按射击、也没按低速**」。
 *   ⚠ 与用户口径「紫与射击键无关」相反（一手证伪）——报告里单列。*/
#define SIGNAL_IN_Z_OR_SHIFT (SIGNAL_IN_SHOT | SIGNAL_IN_FOCUS)

static inline int signal_core_pressing_atk(uint32_t input) { return (input & SIGNAL_IN_Z_OR_SHIFT) != 0u; }
static inline int signal_core_pressing_dir(uint32_t input) { return (input & SIGNAL_IN_DIRMASK) != 0u; }
/* 方向号：**照抄 TH11 `attempted_direction` 的 3/4 两支**（`th11:0x430330` 的左 0x40 先于
 * `th11:0x430340` 的右 0x80）⇒ 左右同按时算「左」，与一手一致。返回 `SIGNAL_GAP_DIR_*` 或 0。
 * 上/下与四个对角在 TH11 里是 1/2/5/6/7/8 —— 但紫的判据只认 3/4（上/下会先取消掉状态），
 * 所以这里只区分左右。*/
static inline int32_t signal_core_dir_of(uint32_t input)
{
    if (input & SIGNAL_IN_LEFT)  return 1;    /* = SIGNAL_GAP_DIR_LEFT  */
    if (input & SIGNAL_IN_RIGHT) return 2;    /* = SIGNAL_GAP_DIR_RIGHT */
    return 0;
}
/* ★ 帕秋莉的判据合成：TH11 的 `0x400` = 「射击(bit0) 与 ↑(bit3) **同一帧都按住**」 ——
 *   ⚠ 这里的 `bit3` 是 **TH11 `INPUT` 的 bit3 = 低速位**（见上），不是「↑」！
 *   `local/_scratch/research-th11-support-special-input.md` §3.5①(b) 把 `test al,0x8` 读成「↑」，
 *   与本轮复核矛盾（`th11:0x43601b` 的 `al` 是 `HARDWARE_INPUT` 的低字节，bit3 与低速同位；
 *   而且 §3.5 自己引的 `0x436069: test al,0x8` 与 `0x43601b` 是**同一个位**，
 *   不可能一处在测「↑」、另一处在测「低速」）。
 *   ⇒ **忠实读法 = 「同一帧既按射击、又处于低速位」**。TH18 没有 `0x400` 这一位 ⇒ 卡侧合成。
 *
 * ★★ **2026-09-16 补完：`0x400` 的完整产生式**（用户实跑纠错：「应当是**同时按下 shift+Z**，
 *   而不是**按着 Z，再按 shift** 就切换了」）。产生点全在每帧输入函数 `th11:0x435fe0`，
 *   两支**互斥**（`0x435ff9 test [0x4c3480],0x200`）：
 *   · **配置位 = 0（默认那一支）** `0x43605a`–`0x43607b`：
 *       `test al,0x1`（射击）∧ `cmp [0x4c933c],5 / jae 跳过` ∧ `test al,0x8`（低速）∧
 *       `cmp [0x4c9348],5 / jae 跳过` ⇒ `or eax,0x400`
 *     ⚠ 那两个槽 `0x4c933c` / `0x4c9348` 在本 build 里是**零初始化的 BSS、全 exe 无写点**
 *       （本轮用 PE 节表逐节解析确认，值 = 0）⇒ 那两个 `< 5` 门**恒过**，该支化简成
 *       「同一帧按着射击 + 按着低速」。但**形状**是「两个各自的值都要 < 5」= 「**两键都刚按下**」。
 *   · 配置位 = 1 那一支 `0x43601b`–`0x43604c`：只看低速位（电平）就置 `0x400`，另有
 *     「射击按住 ≥ 8 帧 ⇒ 派生 bit3」的计数器（`0x4c93bc`，`0x43602d`–`0x436042`）。
 *   ⇒ 按**用户口径 + 默认支的形状**落地：两键都要在「刚按下」的窗口内（= 同时按下）。
 *   ⚠ 读取端仍是 🟡：帕秋莉那条 `0x4308fe test [0x4c93cc],0x400` 读的是**另一个字** `0x4c93cc`，
 *     它在回放路径里由 `0x40d65d`–`0x40d662` 从 `0x4c92b4` 抄来；**实时游玩**下 `0x4c92b4`
 *     的写入者仍未找到（`engine/input/th11/01-special-detection.md` §4 的 ❓ 同一条）。*/
#define SIGNAL_PATCHOULI_FRESH_FRAMES   5   /* 一手形状：`0x43605e` / `0x43606d` 的两个 `cmp …,5` */
static inline int signal_core_patchouli_pressed(uint32_t input)
{
    return (input & SIGNAL_IN_SHOT) != 0u && (input & SIGNAL_IN_FOCUS) != 0u;
}

/* ★ 2026-09-16：**换档判据（含新鲜度）** —— `z_held` / `shift_held` = 两个键各自**已连续按住的帧数**
 * （0 = 这一帧没按）。两键都在 `SIGNAL_PATCHOULI_FRESH_FRAMES` 帧以内 ⇒ 「同时按下」。
 * ⚠ 与上面那个纯电平版的关系：电平版仍被单测与旧调用点钉着；卡侧实际用本函数（用户口径）。*/
static inline int signal_core_patchouli_fresh(uint32_t input, int32_t z_held, int32_t shift_held)
{
    if (!(input & SIGNAL_IN_SHOT) || !(input & SIGNAL_IN_FOCUS)) return 0;
    if (z_held <= 0 || shift_held <= 0) return 0;
    return z_held <= SIGNAL_PATCHOULI_FRESH_FRAMES && shift_held <= SIGNAL_PATCHOULI_FRESH_FRAMES;
}

/* ---- ① 霊夢×八雲紫：缺口瞬移（`th11:0x430e50` 状态机 + `0x4307b0` 起手/钳位）----
 *
 * ★★ **本轮（2026-09-15）按一手逐条重排的状态机**（上一版把「第二次按下」实现了**电平**，
 *    并写成「按住方向键走到边缘再继续按住**也会**触发」——那是**错的**，见下面 state 1 的分支）：
 *
 *   | TH11 state | 含义 | 一手判据 | 出口 |
 *   | --- | --- | --- | --- |
 *   | 0 空闲 | 可以被起手段武装 | `th11:0x4307de`（左）/ `0x43080f`（右）：x **已被钳到该侧** 且 state==0 | → 1 / 3 |
 *   | 1 / 3 已武装 | 「第一次按键」记下了 | `0x8ba8…` 不，是 `th11:0x430ea3`：`dir==0`（**松手**）⇒ state 2/4、frames=1；`dir==同侧` ⇒ 只 `frames++`（`0x430f74`）；`dir==别的` ⇒ state=0（`0x430ed0`）| → 2 / 4 或取消 |
 *   | 2 / 4 窗口 | 等第二次同方向 | `frames > 8` ⇒ 取消（`0x430ee5` / `0x430f8a`）；`dir==同侧` ⇒ **发动**（`0x430eff`）；否则 `frames++`（`0x430f62`）| → 99 / 100 |
 *   | 99 / 100 | 演出 45 帧 + 位移 ±208 px | `0x430ff9` / `0x431012`；`frames >= 0x2d` 收尾（`0x43104d`）| → 0 |
 *
 *   ⇒ **必须「松手再按」才可能发动**：state 1/3 里 `dir==同侧` 只 `frames++`，
 *     永远不会自己走进 state 2/4 —— 所以「贴边后一直按住」**不会**触发（上一版的电平读法作废）。
 *   ⇒ 取消：`state != 0` 时按到 `INPUT & 0x9`（= **射击或低速**，见上）、GUI 对话、Boss 条件
 *     （`th11:0x430e79`–`0x430e99`）；`state == 0` 时**不判**（`0x430e5d` 早退到只 `frames++`）。
 *   ⚠ GUI 对话 / `[ENEMY_MANAGER+0x70]!=0` 两条**本卡没有对应物**（TH18 侧的等价物是「原生瞬移正在跑」，
 *     已作为我们自己的安全门写进 `native_active` 那一支）；不假装复刻。
 *
 * ★ 一手缺口量纲与 TH18 的差别（**必须用 TH18 的量纲**）：
 *   · TH11 的 x 钳位 = `±0x5c00` @ 1/256 px = ±92 px；瞬移位移 = `±0xD000` = ±208 px（`th11:0x430ff9`）。
 *   · TH18 的 x 钳位 = `±0x5c00` @ **1/128 px** = ±184 px（`th18:0x45b735`/`0x45b7ad`），
 *     换边位移 = `±0xc000` = ±384 px（`th18:0x45b25e` 段）。
 *   ⇒ **「贴边」在 TH18 侧的判据就是 `|x| >= 0x5c00`（= 引擎的钳位值本身）**：
 *     TH11 的起手段读的是**钳位前**的 x（`x < -0x5c00`，即「还在往墙里推」），
 *     而我们的 `on_tick_2` 跑在 Player tick **之前**（`engine.h` 的一手：AbilityManager 0x16 先于 Player 0x17）
 *     ⇒ 读到的 `player+0x62c` 是**上一帧钳完**的值，永远是 `±0x5c00` 而不是越界值。
 *     两者合起来 = 「自机已被引擎钉在该侧边界」——语义等价，量纲用 **1/128 px**。
 *   ⚠ **订正 `engine/player/th18/04-warp-yukari-gap.md` §4/§5 的「门槛 0x4800」**：卡 41 的
 *     `th18:0x40a1c8: lea eax,[edx+0x5bff] / cmp eax,0xb7fe / jbe 退出` 是**区间判定**，
 *     实际门槛 = **`|x| >= 0x5c00`**（`0x4800` 只是那条 `lea` 的加数的一半，被误读成了门槛）。
 *     本卡按 `0x5c00` 判（= 与卡 41 真正一致）。*/
#define SIGNAL_GAP_EDGE_FIXED    0x5c00   /* ★ TH18 的 x 钳位值本身（th18:0x45b735/0x45b7ad 的一手） */
#define SIGNAL_GAP_WINDOW_VERDICT 8       /* ★ TH11 的 8 帧窗口（th11:0x430ee5）—— 用户拍板保留 */
#define SIGNAL_GAP_DIR_LEFT      1        /* `th18:0x409a60(player, 1)` = 自左向右 */
#define SIGNAL_GAP_DIR_RIGHT     2        /* `th18:0x409a60(player, 2)` = 自右向左 */
/* 一手：TH11 音效 `0x30`（`th11:0x430f1c` 的 `3+0x2d` / `0x430fc0` 的 `4+0x2c`）、
 * TH11 演出 45 帧（`th11:0x43104d` 的 `cmp …,0x2d`）。
 * ⚠ TH18 侧走的是**原生 16 帧**动画（`th18:0x45b1a0` 段，且它自己在第 1 步播音效 0x51/0x50），
 *   所以这两个数只是**登记**，卡侧一个都不写（把 TH11 的 45 帧硬塞进 TH18 的 16 帧状态机 = 两套状态机打架）。*/
#define SIGNAL_GAP_SE_TH11       0x30
#define SIGNAL_GAP_FRAMES_TH11   45

typedef enum {
    SIGNAL_GAP_IDLE  = 0,     /* = TH11 state 0 */
    SIGNAL_GAP_ARMED = 1,     /* = TH11 state 1/3：已贴边，等**松手** */
    SIGNAL_GAP_WAIT  = 2,     /* = TH11 state 2/4：松过手了，窗口内等第二次同方向 */
    SIGNAL_GAP_ACTIVE = 3     /* 已把方向位交给原生瞬移，等它自己收尾 */
} signal_gap_phase_t;

typedef struct {
    signal_gap_phase_t phase;
    int32_t dir;              /* 认到的边（SIGNAL_GAP_DIR_*）；0 = 无 */
    int32_t frames;           /* = TH11 的 `reimu_a_frames_in_gapping_state`（`th11:0x924`）*/
    int32_t warp_dir;         /* 交出去的方向（1/2）；ACTIVE 期间用它认领原生状态位 */
    int32_t wins, blocked;    /* 诊断：触发次数 / 因「原生瞬移还在跑」被挡下的次数 */
} signal_gap_t;

/* 认边：`x_fixed` = 自机 x（**1/128 px** 定点，TH18 `player+0x62c`），`input` = TH18 输入位。
 * 返回 1/2 = 已贴到该侧边界（与 TH18 卡 41 `0x40a1c1`–`0x40a1f2` 的门槛**同一个数**）。
 * ⚠ `input` 只用于「方向键」那一半，与卡 41 一致（它要求「朝那一侧的方向键」）。*/
int signal_gap_edge(uint32_t input, int32_t x_fixed);

/* 每帧推进。返回 1 = **本帧要发动**（调用方去调 `th18:0x409a60(player, out_dir)`）。
 * `native_active` = 原生瞬移动画正在跑（`(*(uint32_t*)(player+0x4779c) & 0x180) != 0`）
 * ⇒ 真则把状态机压回 IDLE 并返回 0（**不许二次触发**：`th18:0x409a60` 每次都把步数清零，
 *   叠着调会让 16 帧动画永远走不完）。*/
int signal_gap_tick(signal_gap_t *g, uint32_t input, int32_t x_fixed, int native_active, int *out_dir);

/* ---- ② 霊夢×伊吹萃香 / ③ 霊夢×射命丸文：两个「连续不按键」计数器 ----
 * 一手 `th11:0x43077a`（萃香 `+0x8ba4`）/ `0x430651`（文 `+0x8ba8`），**同形**：
 *
 *     if (!(INPUT & 0x9)) { if (++n > 10) <效果>; } else n = 0;
 *
 *   · `INPUT & 0x9` = **射击(bit0) | 低速(bit3)**（一手订正，见文件上半部分的位表）——
 *     **不是**「上/下」。两条判据都不含任何方向位。
 *   · 萃香那条**另**要求 `attempted_direction == 0`（`th11:0x43077e`；与 `&0x9` 不冗余：
 *     一个查「有没有按方向」、一个查「有没有按射击/低速」）。
 *   · ★ **触发那一下不清零**（`0x430666`/`0x430795` 之后直接落到 `0x4306ce`/`0x4307ab`，
 *     归零只在**判据不成立**的 `0x4306c4`/`0x4307a5`）⇒ 过阈值之后是**持续态**，
 *     每帧都成立，直到松手。阈值 = **严格大于 10**（`cmp …,0xa` + `jle`）。
 *   · 效果：萃香的 = **全屏收道具**（`th11:0x4245e0`，把整池 state 1 的道具切成 3）；
 *     文的 = 只驱动**视觉**（`0x8ba8` 全 exe 只有 inc/cmp/清零三个访问点，**不参与任何速度计算**）。*/
#define SIGNAL_SUPPORT_HOLD_FRAMES  10
typedef struct {
    int32_t frames;           /* 连续满足判据的帧数（钳在 `SIGNAL_SUPPORT_HOLD_FRAMES + 1`；一手是无界自增，判据只看 `> 10`）*/
    int32_t active;           /* 上一帧是否已过阈值（用来产出**上升沿**）*/
    int32_t fires;            /* 诊断：**过阈值**的次数（进入持续态的次数）*/
} signal_hold_t;

/* 每帧推进：`ok` = 本帧满足判据。返回 1 = **本帧处于「已过阈值」的持续态**（不是一次性触发）。
 * `rising`（可为 NULL）= 本帧是不是**刚过阈值**的那一帧 —— 卡侧拿它当「只记一次账/只打一行」的边沿
 * （⚠ 2026-09-16：这个边沿**曾经**是「收费边沿」，价那一层已按用户拍板整体删除，见 `signal.c` 的常量块）。*/
int signal_hold_tick(signal_hold_t *h, int ok, int *rising);

/* ---- ③ 文的「超高速」：❓ 自定 ----
 * 一手缺口①**本轮已取到**（`local/vendor/thtk/build/thdat/thdat -x 11 local/th11.v1.00a/th11.dat`）：
 *   `pl00a/b/c.sht` 的 `+0x10..0x1c` 四个速度字段**逐字节相同**（`4.5 / 2.0 / 3.18198 / 1.41421`）
 *   ⇒ 「文 = 速度天生高」**证伪**；而 `+0x8ba8` 又明确不参与速度计算（见上）
 *   ⇒ **一手里根本不存在这个倍率**（文那条在原版只驱动贴图/拖尾）。
 * 用户口径要求「超高速移动」可被感知 ⇒ 我们用 TH18 引擎自己的移速旋钮
 * `CE_PLAYER_SPEED_MULT`（`th18:0x477ec`；`0x45c702` 每帧写 1.0、`0x45b5b6` 移动时 `mulss` 读；
 * 先例 `trance.c` / `season.c`）补一个**保守**倍率，**登记为 ❓ 自定**，
 * 并且**不当成文的表现**（原版不加速）。*/
#define SIGNAL_HYPER_SPEED_PERMILLE 1500   /* ❓ 自定：1.50×（理由与取舍见 TUNING.md 的 111 行）*/
/* 持续态的门槛：`signal_hold_tick` 的 `frames > SIGNAL_SUPPORT_HOLD_FRAMES`；这里给个具名。*/
#define SIGNAL_HYPER_MIN_FRAMES (SIGNAL_SUPPORT_HOLD_FRAMES + 1)


/* ======================================================================================
 * ④ 魔理沙×爱丽丝（TH11 组合号 **3** = `3×CHARACTER(1) + SUBSHOT(0)`）
 * ======================================================================================
 * ## (a) 「子机数上限 8」—— **仍不实现**（TH18 原生上限 4 < 8；用户 2026-09-15 口径亦确认「还是 4 个」）
 * 一手（`th11:0x432cc0` 段）：TH11 **不存在「上限常量」**，上限就是 `zPlayer.options[0x8]`
 * 这个数组本身（元素 `0xe4`、已启用数量 `+0x7c90`，全部 12 个访问点里**没有任何 8 之外的钳位**）；
 * 「只有爱丽丝带 8 个」是**她的 SHT 在满火力时填 8 格**的结果（🟡 该推测未证）。
 * ★ 2026-09-15g **补一个一手数据点**（本轮读 `.sht` 时顺手对到的，**仍不是证明**）：
 *   `pl01a.sht` 的 `+0x20 = 8`，其余五个自机 `+0x20 = 4`；而 `MAX_POWER = +0x20 × +0x24`
 *   ⇒ 爱丽丝的火力**刻度**本来就是 8 档（HUD 打「现/满」时分母 = `MAX_POWER / +0x24` = **8**）
 *   ⇒ 「8」不只是脚本事故，**是她的火力档数**。⚠ 但「子机数组按档数填」那一步在中段代码里
 *   本轮仍未逐条读死 ⇒ 这一条继续挂 🟡，**不要当结论用**。
 * TH18 侧：原生子机数 = **火力档**（`Player__repopulate_options_and_notify_cards` `th18:0x45d5e0`：
 * `lvl = CURRENT_POWER / 100` ∈ 0..4 ⇒ **满火力 NORMAL 4 个**，5 档才有第 5 个；
 * 卡申请的子机走**另一个 12 格池** `inner+0x410`（`th18:0x40a790`），但**数量由卡自己声明**
 * （`ce_option_pose_t.count`），而声明层的上限是 `CE_OPTION_POSE_CELLS` = 4
 * （`sdk_core.h`，一手：原生数组 4 格 `0x45b8ea push 4`）。
 * ⇒ TH18 原生的上限（4）**已经低于 8**，「提到 8」在本卡里**是空操作**；
 *   真要提到 8 得改**共享件** `sdk_core.h` 的 `CE_OPTION_POSE_CELLS`（影响所有卡）⇒
 *   **登记进报告，不在本轮动**。
 *
 * ## (b) ★★ 2026-09-15g：**两条真·效果**（**比例 ✅ 复刻 + 数值 ❓ 映射**，本轮实装）
 *
 * 用户口径（2026-09-15 逐字）：「**还是 4 个子机**，只是**灵击改成 0.5p 一次**；但此时需要**把火力
 * 变成 +0.025**，其中 .5 需要额外存储，或者按照 **+0.02 +0.03 交替**」；追加：「不知道 P 点能不能
 * 改成 +0.025 一颗，能自然行，**不能的话 3 最干净**」。
 * ⇒ 引擎的火力字段是 **1/100 档的整数**（`CE_ADDR_POWER_PER_LEVEL` = 100 = 一档、`CURRENT_POWER` 是 int）
 *   ⇒ **+0.025 档 = 2.5 单位 无法精确表示** ⇒ 走用户给的**交替**那条路：**+0.02 / +0.03 交替**
 *   （平均 2.5 单位 = 0.025 档，**不需要「额外存储 .5」、也不需要任何新引擎字段**）。
 *
 * ### ① 灵击 = 0.5 档（50 单位）—— **比例是 ✅ 复刻，绝对档位是 ❓ 映射**（★ 父 agent 2026-09-15g 裁定）
 *   本轮自己把 TH11 的 `.sht` 读了一遍（`local/vendor/thtk/build/thdat/thdat -x 11 …/th11.dat`）：
 *     · 组合号 → 文件：`th11:0x42f911`–`0x42f916`（`edx = 3×CHARACTER + SUBSHOT`，`[edx*4 + 0x4a7a90]`）
 *       ⇒ 该表 `th11:0x4a7a90[3] = 0x4953bc` = 字符串 **`"pl01a.sht"`**
 *       ⇒ **爱丽丝 = 魔理沙 A = `pl01a.sht`**（`0x4a7a90[4]/[5]` = `pl01b/pl01c` = 帕秋莉 / 荷取 ✓）。
 *     · `pl01a.sht` 头：`+0x20 = 8`、**`+0x24 = 12`**（`0x42fa7c: mov ds:0x4a574c,[sht+0x24]` = `BOMB_COST`；
 *       `0x42fa69: imul` 出 `MAX_POWER = 8 × 12 = 96`）。**其余五个自机 `+0x20 = 4`、`+0x24 = 20`**。
 *   ⇒ **这一条分两层看，别再写成「整条都是用户口径」**：
 *     ① **比例那层 = ✅ 复刻（一手，两条式子相除）**：一发占**满槽**的比例
 *        `爱丽丝 12/96 = 1/8` vs `其余五机 20/80 = 1/4` ⇒ **恰好是别人的一半**
 *        （等价说法：满槽能放 **8 发** vs 4 发）。这是 `+0x24` 与 `+0x20` 两个字段**相除**的直接结果，
 *        **没有任何我们的换算**。
 *     ② **绝对档位那层 = ❓ 我们的映射**（用户口径）：TH11 的消耗是 **per-shot 字段**、HUD 自己按
 *        `+0x24` 归一（`th11:0x41cc1d` 的 `idiv ds:0x4a574c` ⇒ 爱丽丝自己的刻度里一发就是 **1.00**，
 *        满火力显示 **8.00**；格式串 `0x494180`/`0x494184`/`0x49418c`）；
 *        而 TH18 的一档是**跨机体统一**的 100 单位、没有 per-shot 刻度 ⇒ 只能把它**落成
 *        「1 档的一半」= 50 单位**（`FAITH_POWER_COST / 2`）。
 *   ⚠ 收窄后的说法（别再扩大）：**「绝对数 0.50」不在表里**（表里是 12；与别机的绝对单位比是 12/20 = 0.60，
 *     那是**跨机体**比值、不是爱丽丝自己的刻度）—— 但 **「一半」这个比例是一手** ⇒ 落地值 50 单位
 *     是「**复刻的比例 × 我们的档位映射**」，注释与文档都按这两层写。
 *   ⚠ 门槛 / 下限的落地选择（**❓ 自定：与既有普通灵击门槛「同构」**）：
 *     · **门槛 = 「能扣就扣」**：`power − 50 ≥ 下限(105)` ⇔ **`power ≥ 155`（1.55 档）**。
 *       这就是本卡**既有**的那条规则的同一个形状 —— 普通灵击是「门槛 ≥2.00 档 = 扣款 100 + 下限 105」，
 *       即门槛**恒等于「扣款 + 下限」**（所以 105 那个下限在常态下永远不被触发）；
 *       爱丽丝换成 50 的扣款，同一套口径给出 `50 + 105 = 155` ⇒ **规则同构、只有扣款这一个数变了**。
 *       * 不取 `power ≥ 50`（= 只按「成本」判，字面「能扣就扣」）：那样 `power ∈ [50,105]` 时
 *         `max(power−50,105)` 会**把火力加上去**（净收益）⇒ 白嫖灵击 = 超模；
 *       * 也不取「门槛照旧 2.00 档」：那样下限**永远不生效**（死代码），且与「能扣就扣」相反。
 *     · **下限照旧** = `FAITH_POWER_FLOOR`（105 = 1.05 档，用户 2026-09-12 拍的那个「サービス」）；
 *       门槛已经保证 `power − 50 ≥ 105` ⇒ 这一夹**永不触发**（只在越界入参上兜底）。
 *     ⚠ 与常态那套（`mof_core_power_spend()`：门槛 2.00 / 扣 1 档 / 下限 1.05）**并存**：
 *       `alice_on == 0` ⇒ `signal_core_alice_power_spend()` **原样转发**给共用件
 *       ⇒ 110 与「非魔理沙机体」的行为**逐字节不变**（单测按 `power` 逐个对拍）。
 *
 * ### ② P 点（火力道具）收益 +0.05 → **+0.025 档**（+0.02 / +0.03 交替）—— 🟡 **用户口径**
 *   现状（110/111 共用，见 `mof_core.h` 的 §P 点火力）：原生小 P = **+1 单位**，我们在
 *   `on_item_commit` 补差到 **+5 单位**（+0.05 档）。Alice 生效时改成「平均 2.5 单位」：
 *   第 1 颗 **+2（0.02）**、第 2 颗 **+3（0.03）**、第 3 颗 **+2** …（顺序不影响平均值）。
 *   · **交替相位跨帧 / 跨关保留** ⇒ 它住在**共享件**的静态区（`faith.c` 的 `s_p_alt`），
 *     **没有新增任何引擎字段**、也不写 `MONEY`、不动道具本身。
 *   · `alice_on == 0` ⇒ `faith_core_p_topup_total()` 返回常态 `FAITH_P_TOPUP_NORMAL`（5 单位）
 *     **且不推进相位**（⇒ 「只在 Alice 生效时改变」在宿主单测里就能钉住）。
 *   · **只管 TH18 `type 1`（小 P）** —— 与共用件那一层同口径（大 P `type 3` 仍 +1.00 档，见
 *     `mof_core.h` §七①）；Alice 只改这一层的**数值**，不改它的**范围**。
 *   · 想改成用户追加口径里的「平 +0.03」：把 `faith_core.h` 的 `FAITH_P_TOPUP_ALICE_LO`/`_HI`
 *     **都改成 3**（一处两行）；⚠ 那就**不是** +0.025 了（登记在 `TUNING.md` §2.1f）。
 *
 * ### ③ 谁落笔（**110 在场时的双倍风险**）
 *   见 `cards/signal.c` 的 `signal_on_item_commit()`：这一层是「**每颗道具全局只补一次**」——
 *   110 与 111 各有一个 `on_item_commit`，但 111 在 `mof_core_p_owner(ce_owned(110), 1) != 111` 时
 *   **先让位**（id 最小者落笔）⇒ 两颗同装时**只有 110 补一次、111 一个字节都不写** ⇒ **不会双倍**。
 *   ★★ **2026-09-15h 订正**：原来这里写着「代价 = 双卡同装时爱丽丝的 P 点减益不生效」——**已修**：
 *     补值现在由**共享策略**回答（`faith_p_topup_take()`），落笔者 110 问的是**同一个**策略
 *     ⇒ 判据（111 在场 ∧ 机体 == 魔理沙(1)）在 110 那一侧同样成立 ⇒ 双卡同装也照样减益。
 * ==================================================================================== */

#define SIGNAL_ALICE_OPTION_CAP_TH11 8
#define SIGNAL_ALICE_OPTION_CAP_TH18 4   /* = CE_OPTION_POSE_CELLS（sdk_core.h）*/

/* ✅ 一手（**只登记**，不参与计算）：`pl01a.sht` 的 `+0x24 = 12` / `+0x20 = 8`（⇒ 满槽 96 = 8 发；
 *   其余五机 20/4 ⇒ 满槽 80 = 4 发）⇒ **一发占满槽的比例恰好是别人的一半 = ✅ 复刻**。见 (b)①。*/
#define SIGNAL_ALICE_BOMB_COST_TH11   12
/* ❓ **映射**（比例 ✅ 一手 + 档位 ❓ 我们的换算）：一发灵击 = **1 档的一半** = 50 单位。
 *   ⚠ 不是「表里有 0.50」—— 表里是 12；0.50 来自「一手确认的**一半**比例」×「TH18 一档 = 100 单位」。*/
#define SIGNAL_ALICE_BOMB_COST_UNITS  (FAITH_POWER_COST / 2)   /* = 50 */

/* 一发灵击的扣法（`alice_on` = 机体门：TH18 魔理沙(1)）。返回类型与 `mof_core_power_spend()` **同一个**
 * （`mof_power_t`），`ok = 0` ⇒ 卡侧什么都不做（照原作的「放不出」）。`alice_on == 0` ⇒ 原样转发共用件。
 * ⚠ 门槛/下限的形状 = **❓ 自定（与既有普通灵击门槛同构）**：门槛恒等于「扣款 + 下限」⇒ `50 + 105 = 155`。
 *   扣款那个数**不是自由变量**：它是「一手确认的**一半**比例 × TH18 一档(100 单位)」（见 (b)①）。*/
mof_power_t signal_core_alice_power_spend(int32_t power, int alice_on);

/* ⛔ **退役（2026-09-15h，别再复活）**：这里原来有 `SIGNAL_ALICE_P_TOTAL_LO/HI`、`signal_alice_t`
 *   （含交替相位 `alt`）与 `signal_alice_p_total()` / `signal_alice_p_topup()` 两支。
 *   全部**搬进共享件** —— 理由只有一个，但是硬的：**P 点补值的落笔者恒为 110**（选举），
 *   相位若住在 111 的卡状态里，双卡同装时 110 够不到它（卡之间不许读对方状态）⇒ 减益被绕过，
 *   而「两张卡各持一份相位」又会分叉成两条序列。
 *   ⇒ 现在：纯逻辑 `faith_core.h` 的 `faith_core_p_topup_total()`、相位与策略 `faith.h` 的
 *     `faith_p_topup_take()` / `faith_p_topup_stats()`、补值 `mof_core_p_topup_units()`。*/

/* ---- ⑤ 魔理沙×帕秋莉：子机配置切换（五档）----
 * 一手 `th11:0x4308fe`：`if (INPUT_B & 0x400) formation = (formation + 1) % 5`
 *   （`th11:0x43090a`–`0x430921`），ANM script = `0x22 + formation`（`th11:0x430949`），
 *   目标角/距从 `zPlayerOption+0x74` 的 `marisa_b_preferred_offsets_by_formation[formation]` 取
 *   （`th11:0x430970` 的 `[ebp+eax*8+0x75e4]`；`0x75e4 = 0x7570 + 0x74` ✓，5 档 × zInt2）。
 *   **无帧冷却**（电平判定）⇒ 长按会每帧切一档。卡侧照抄该电平判定，但加一条
 *   **按键沿闩锁**（见 `signal.c`）以免长按把五档转成走马灯 —— 那是 ❓ 自定，登记 TUNING。*/
#define SIGNAL_FORMATION_COUNT  5
/* 边沿闩锁要等按键松开；这本是 TH11 没有的（它只有电平）⇒ 单独给出、写成可关的常量。*/
#define SIGNAL_FORMATION_EDGE_LATCH 1

/* ❓ 自定：五档的子机阵型（单位 = **十分之一像素**，与 `signal_core` 的整数纪律一致）。
 * ⚠ **一手缺口**：TH11 的五张表在 `zPlayerOption+0x74`（`th11:0x75e4`），但全 exe 里对它的
 *   引用**只有 `0x430970`/`0x43098a` 两处读**、**没有任何写点**，也没有一块 `75 e4 00 00` 的
 *   数据镜像 ⇒ 那 5 档 × 8 格的数值**没能取到**。这里给一套**可用、可测、不超模**的几何，
 *   逐档只差「摊开的宽窄 + 前后的远近」，**登记为 ❓ 自定**（TUNING.md）。
 * ⚠ 只填**前 4 格**：TH18 的声明层上限是 `CE_OPTION_POSE_CELLS` = 4（不改共享件）。*/
#define SIGNAL_FORMATION_CELLS  4
/* ★ 2026-09-16zb：**有格子的档数**（TH11 `pl01b.sht` 头 `+0x20 = 4` ⇒ 档 1..4；档 0 = 零子机）。一手阵型表按
 * 「档 L 的块 = `T(L-1) .. T(L)-1`（T = 三角数）」排布 ⇒ 取格要用 `(档, 序号)` 两个维度。*/
#define SIGNAL_FORMATION_LEVELS 4
typedef struct { int32_t x10; int32_t y10; } signal_cell_t;   /* 十分之一像素；y 负 = 在自机上方 */

/* ======================================================================================
 * ★★ 2026-09-16：魔理沙侧的**原生子机抑制** + 「子机格数随 P」+ **DPS 归一**（用户口径）
 * ======================================================================================
 * 用户逐字（2026-09-16）：
 *   ① 「**魔理沙原生子机需要弄掉**」——装了 111（且机体 = 魔理沙）时，自机**原生那 4 格子机**不许再出现；
 *   ② 「火力随 P 完全依赖原生关系，但是，得根据这一作的具体 dps 进行调整。保证平均 dps 一致即可，
 *      比如可以相邻两次发射的弹幕攻击力不一样，以符合平均 dps 关系。
 *      大概是 18魔理沙/13魔理沙 = 18灵梦/13灵梦」。
 *
 * ## A. 一手：两作的「魔理沙子机」各是多少（本轮自己读 `.sht`，不外推）
 *   · 数据：`thdat -x 18` 的 **`pl01.sht`**（TH18）与 `thdat -x 13` 的 **`pl01a.sht`**（TH13）；
 *   · 结构（一手）：TH18 stride `0x5c`（`engine/sht/th18/02`），档数在头 `+0x20`、每档单位在 `+0x24`
 *     —— TH18 `+0x24 = 40`（**引擎全局** `0x4ccd40 = 100` 才是运行时除的那个数），
 *     TH13 `zShtShooter` stride `0x38`，且 `th13:0x441f04 imul [sht+0x24],[sht+0x20]` 定 `MAX_POWER = 4×40 = 160`；
 *   · 选组规则**两作同形**：非同聚焦 = `档`，聚焦 = `档 + 1 + sht[0x20]`
 *     （TH18 一手 `0x45ebcb`–`0x45ebf8`；TH13 一手 `th13:0x446013`–`0x446034`，那里还多一条 `idx = 0x0a` 的特例）；
 *   · 「每帧输出」按**本仓既有口径** Σ(damage / fire_rate)（`append_shooterset.py` 的 `_dps()` 就是它）。
 *
 * 满档（4 档 = 4 格子机）实测，**伤害单位/frame**（unfocused / focused）：
 * ```
 *                body            option          合计
 *   18 魔理沙   6.000 / 6.000   6.667 / 5.250   12.667 / 11.250
 *   13 魔理沙  10.000 / 9.333   3.067 / 2.000   13.067 / 11.333
 *   18 灵梦     9.333 / 9.333  16.000 /20.000   25.333 / 29.333
 *   13 灵梦     9.333 / 9.333   5.250 / 8.000   14.583 / 17.333
 * ```
 * ## B. 比例 k = 18 / 13（用户要的那个比值）
 *   · **子机层**（本轮归一真正用的那个量）：`k_opt = 6.667 / 3.067 = **2.174**`（unfocused）/ `2.625`（focused）。
 *     ★ 这一格**与伤害模型无关**：两作的子机行都是**激光**（`speed = 0` + `mode = 2`、
 *     `func_on_tick = 2`、`fire_rate` 都是 15）⇒ 「激光按每帧 `damage` 连续给伤」的算法
 *     在分子分母上同时生效、约掉 ⇒ 两种算法都出 2.174（`100/46` = `6.667/3.067`）。
 *   · **整条自机弹**（用户的等式左边）：`k_all = 12.667 / 13.067 = **0.969**`（unfocused）。
 *     ⚠ 用同一口径横向比四个机体（满档 unfocused）：魔理沙 **0.969** / 灵梦 **1.737** /
 *     咲夜 **1.675** / 早苗 **1.970** ⇒ **用户的等式在这个口径下不成立，魔理沙是唯一的例外**。
 *     根因 = 魔理沙的子机行**是激光**：把激光按「每帧给 `damage`」算（其余行仍按 damage/rate），
 *     魔理沙变成 **1.893**，与灵梦/咲夜/早苗的 1.674–1.970 落在同一条带上
 *     （咲夜 1.675、灵梦 1.737、魔理沙 1.893、早苗 1.970）⇒ **用户的「≈」成立，但它要求
 *     把激光按连续伤害算**。两侧的**完整表**（4 机体 × 2 聚焦 × 4 档 + 两种模型）登记在 `TUNING.md`。
 *     ⚠ 一个未收口的离群：TH13 早苗的**聚焦**组总量只有 3.88（其余机体 11–19）⇒ 那一格 7.079 不可信，
 *     本轮没查它（早苗聚焦那两组的行为函数本轮没逐条读）⇒ **不作为任何结论的依据**。
 *
 * ## C. 落点：我们的子机层 = 「格数随档 + 单发伤害按 k 折算」
 *   · **格数** = 火力档（0..4）= 原生关系（`signal_power_cells()`，纯逻辑、单测）：
 *     档 0 一格都不申请、档 N 申请 N 格（`ce_option_pose_t.count`），位置取本档阵型的前 N 格。
 *     ⇒ 原版「5 档阵型只用一个格子表达」的取舍**改成**「N 格 + 阵型的前 N 格」：
 *     满档第一次能看到**整个阵型**（旧版任何档都只有 1 格）；档位低时格数少 = 输出低（与原生同向）。
 *   · **单发伤害**：让「N 格的合计每帧输出」= **本作原生子机层**的每帧输出（= `13 档 × k_opt`）：
 *     `D = fire_rate × V18_opt(4) / 4 = 3 × 6.667 / 4 = **5**`
 *     （`fire_rate = 3`、每格 1 行 —— ★ 2026-09-16zb 按 TH11 `pl01b.sht` 的元素行重定，旧版是 12×…/4 = 20
 *     配「每格 4 行、rate 60」；发弹数的对账见 `assets/sht/append_shooterset.py` 的 `MARISA_FORMATION_*`）。
 *     4 格 × 5/3 = 6.667/frame = 原生满档子机层，**一格不多**（本卡把原生子机层**替换**掉，
 *     自机总输出因此与原生**逐值相同**：6.667 + 主炮 6.000 = 12.667 ✓）。
 *   · 「**平均 dps 一致**」逐档核对（原生 vs 我们，unfocused）：
 *     `1.867/2.400/5.600/6.667` vs `1.667/3.333/5.000/6.667` ⇒ 档 1/3 各 −11%、档 2 **+39%**、档 4 精确 0%；
 *     **四档均值** `4.133` vs `4.167` = **+0.8%**（这就是用户说的「平均 dps 一致即可」）。
 *     偏差来源是原生自己的非单调（档 2 的四条激光每条只有 18、比档 1 的单条 28 还低）
 *     —— 要用单发伤害去逐档拟合它就得**每档一个 damage**（或用户说的「相邻两发交替」），
 *     生成器已经按 `(lo, hi)` 对留好了那个形状（`MARISA_FORMATION_ROW_DAMAGE`），
 *     本轮取 `(20, 20)`（= 不需要交替）；要逐档拟合的改法与代价写在 `TUNING.md` 的 111 行。
 *   · ⚠ **残留偏差（明写）**：① 上面那三档 −11%/+39%/−11%；② 聚焦/非聚焦给的是**同一套**
 *     （格数与伤害都不分聚焦）⇒ 相对原生聚焦档（5.250）高 +27%；③ **伤害模型**：原生子机行是
 *     **激光**（对同一条束每帧给 `damage`），我们是一发一发飞出去的**平弹**
 *     ⇒ 同一本 Σdmg/rate 账下等价，但打一个具体敌人时「束一直贴着」与「弹一颗颗飞」并不等价，
 *     这一层没有一手能对齐 ⇒ 登记为已知近似。
 * ==================================================================================== */

/* 取第 `formation`（0..4）档、第 `idx`（0..3）格的**目标偏移**（返回 0 = 越界，`out` 不动）。*/
int signal_formation_cell(int32_t formation, int32_t idx, signal_cell_t *out);
/* ★★ 2026-09-16zb：**一手**阵型取格（TH11 `pl01b.sht + 0x28 + formation*0x50`，条目 = `T(档-1)+序号`）。
 * 一手链条（写者/读者/索引/轴序）的唯一出处 = `signal_core.c` 那段注释 +
 * `engine/player/th11/04-marisa-b-option-geometry.md`。`level` = 火力档 1..4、`idx` = 0..level-1。*/
int signal_formation_cell_lv(int32_t formation, int32_t level, int32_t idx, signal_cell_t *out);
int32_t signal_formation_tri(int32_t level);
/* 归一化档号 = `((n % 5) + 5) % 5`（一手的 `(n+1)%5` 循环的等价式；负数也安全）。*/
/* ★★ 2026-09-16m：按**对称轴**取格（n = 本次声明几格；idx = 第几格 0..n-1）。
 * 表本身左右对称，但「取前 n 格」在 n<4 时会偏左 ⇒ 这个入口保证任何 n 都左右对称。
 * 语义与 ❓ 自定的理由见 `signal_core.c` 里的定义处。*/
int signal_formation_cell_sym(int32_t formation, int32_t idx, int32_t n, signal_cell_t *out);

int32_t signal_formation_wrap(int32_t n);
/* 本档对应的 shooterset 槽号（`CE_SHT_SET_FORMATION_*` 由 `assets/sht/append_shooterset.py` 生成
 * 到 `sht_ids.h`；这里是**纯逻辑的映射表**，不 include 生成物 —— 映射本身单测。）*/
int32_t signal_formation_sht_set(int32_t formation);   /* 返回 1..5（**槽号的相对序号**，见 .c）*/

/* ★★ 2026-09-16（上面 B/C 两段的落地）：**子机格数 = 原生火力档**（纯逻辑，主机单测钉得住）。
 * `cells_max` = 声明层的上限（`CE_OPTION_POSE_CELLS` = 4，由卡侧传进来 —— 本文件不 include 共享件）。
 * 返回 `clamp(power / per_level, 0, cells_max)`；`per_level <= 0` ⇒ 0（守好除零）。
 * 一手：原生就是 `lvl = CURRENT_POWER / POWER_PER_LEVEL`（`th18:0x45d604`–`0x45d60a`）再建 `lvl` 格
 * （`0x45d81e mov edi,4` 是数组上限、`0x45d85f test eax,eax; jle` = 0 档一格都不建）。*/
int32_t signal_power_cells(int32_t power, int32_t per_level, int32_t cells_max);

/* ---- ⑥ 魔理沙×河城荷取：**河童罩子**（放 B 起一个水泡；没被撞 ⇒ 退**半档**火力）----
 * ★★ 2026-09-16q 本轮**按一手重写**（`local/th11.v1.00a/th11.exe` 逐条反汇编 +
 *   `thanm -l 11 pl01.anm -m v4.anmm` 读脚本原文）。上一版那套「75 帧窗口 + 扣多少退多少」
 *   是**证据不足时的自定**，本次逐条订正：
 *
 *   | 事项 | 一手（TH11） | 上一版（❌ 本轮订正） |
 *   | --- | --- | --- |
 *   | 罩子本体 | `pl01.anm` **script58** 的青色水泡，**920 帧**自删（脚本 `+900: delete()`）| 无（只有窗口）|
 *   | 破罩 | 被撞 ⇒ `th11:0x40793d call 0x4070e0`（`anm_trigger_leave_by_id`）播 `case(1)` | 无 |
 *   | 没被撞 | 泡的 VM 自己走完（`th11:0x407828 test esi,esi` 的「VM 没了」支）⇒ **`collect_power(10)`**（`th11:0x40784d`）| 退**全额**（100）|
 *   | 判据 | 「**泡的 VM 还在不在**」 | 「75 帧内 `player_state ∈ {2,4}`」|
 *   | 破罩的代价 | 30 帧外扩消弹圈（32 → 322，+10/帧）+ **每帧刷 40 帧无敌** + 一次性 75 帧伤害源 | 无 |
 *
 *   · **额度 = 半档**（不是照抄 10 这个数）：TH11 的 `BOMB_COST` 是 per-shot 字段（`pl*.sht+0x24`）
 *     = **20**（= 1.00 火力），退款 10 ⇒ **退一半**。我们 111 的灵击扣 1 档 = 100 单位 ⇒ 退 **50**。
 *   · ⚠ `SIGNAL_NITORI_REFUND_FRAMES`(=75) 那个数**从来不是罩子寿命**：75 是**破罩那一瞬间**
 *     才建的那个伤害源的存活帧数（`th11:0x40790c push 0x4b`）⇒ 本轮拆成两个名字，免得下一个人再混。
 *   · 破罩的完整一手（逐条）：
 *     `th11:0x4078c4 mov [player+0x928],1`（**把这一下中弹撤销**，不扣命）→
 *     `th11:0x4078f3` SE 0x26 → `0x40790c push 0x4b`(75 帧) / `push 0x1e`(半高 30) /
 *     `0x4984c8`(半宽 20) / `0x497ef0`(伤害 32.0) → `0x407923 call 0x433a90`（伤害源）→
 *     之后每帧：消弹（半径 `[bomb+0x490]`，初 32.0 / 每帧 +10.0）+
 *     `0x407961 push 0x28` → `0x406860`（= `Player::set_iframes(40)`，每帧刷）+
 *     `0x407968 cmp [bomb+0x2c],0x1e`（计时 > 30 ⇒ 删泡 VM、bomb 结束）。*/
#define SIGNAL_NITORI_SHIELD_FRAMES   920   /* 一手：script58 第 920 帧 `delete()`（泡的寿命）*/
/* ★★ 2026-09-16u 泡的三条可见曲线常量（**全部一手**，出处 = `pl01.anm` script58 正文）：
 *   `alphaTime(10, 0, 255)` / `scaleTime(20, 0, 1.0f, 1.0f)` / `scaleTime(900, 0, 0.1f, 0.1f)`。*/
#define SIGNAL_NITORI_FADE_IN_FRAMES  10    /* 一手：10 帧 alpha 0 → 255 */
#define SIGNAL_NITORI_GROW_FRAMES     20    /* 一手：20 帧 scale 0 → 1.0 */
#define SIGNAL_NITORI_SHRINK_FRAMES   900   /* 一手：其后 900 帧 scale 1.0 → 0.1（20+900 = 920 = 寿命）*/
#define SIGNAL_NITORI_END_SCALE_MILLI 100   /* 一手：`scaleTime(900, 0, 0.1f, 0.1f)` 的终值（千分点）*/
/* ★ 2026-09-16u：文·超高速残像的**上色斜坡长度**（一手 `pl00.anm` script19 的 `colorTime(15, …)`）*/
#define SIGNAL_TRAIL_RAMP_FRAMES      15
#define SIGNAL_NITORI_REFUND_NUM      1     /* 退款比例 = NUM/DEN（一手 `collect_power(10)` / `BOMB_COST` 20）*/
#define SIGNAL_NITORI_REFUND_DEN      2
#define SIGNAL_NITORI_BREAK_FRAMES    30    /* 一手：破罩的 30 帧（`cmp [bomb+0x2c],0x1e`）*/
#define SIGNAL_NITORI_BREAK_R0_PX     32    /* 一手：破罩消弹圈的初值（`0x497ef0` = 32.0f 也用在伤害上）*/
#define SIGNAL_NITORI_BREAK_DR_PX     10    /* 一手：每帧 +10.0（`0x49822c`）*/
#define SIGNAL_NITORI_BREAK_IFRAMES   40    /* 一手：每帧刷 40 帧无敌（`0x407961 push 0x28`）*/
#define SIGNAL_NITORI_BREAK_DMG       32    /* 一手：伤害源每帧伤害 32.0（`0x497ef0`）*/
#define SIGNAL_NITORI_BREAK_DMG_LIFE  75    /* 一手：伤害源存活 75 帧（`0x40790c push 0x4b`）—— **不是**罩子寿命 */
#define SIGNAL_NITORI_BREAK_HALF_W    20    /* 一手：伤害源半宽 20.0（`0x4984c8`）*/
#define SIGNAL_NITORI_BREAK_HALF_H    30    /* 一手：伤害源半高 30（`push 0x1e`）*/
typedef struct {
    int32_t active;           /* 这一发**开没开退款窗**（原生 B / 决死形态 = 0 ⇒ 只起泡不记账）*/
    int32_t broken;           /* 泡被撞过 ⇒ **不退款**（一手：破罩那条路不退款）*/
    int32_t pending;          /* 待退还的**火力单位**（开窗时按 NUM/DEN 算好）*/
    int32_t refunds, denials; /* 诊断：退还次数 / 被留下（破罩 或 本来没扣到）次数 */
} signal_nitori_t;
/* ⚠ **寿命不在这个结构里**（2026-09-16u 订正）：寿命是卡侧 `s_shield.life_left` 的**暴露帧**计数 ——
 *   账本的 `active` 只表示「这一发要不要退款」（原生 B 那发是 0），拿它当寿命会让原生 B 的泡
 *   在无敌一结束就被判死（实跑日志：`暴露 1 帧 / 暂停 381 帧 … 罩子结束（寿命尽头）`）。*/

/* 退款额度（纯比例；`cost` = 真正被扣掉的**火力单位**）。*/
int32_t signal_nitori_refund_of(int32_t cost);
/* 起泡那一刻开窗：`cost` = 这一发真扣掉的火力单位（正数）。*/
void   signal_nitori_open(signal_nitori_t *n, int32_t cost);
/* 泡被撞（破罩）⇒ 这一发不再退款。*/
void   signal_nitori_break(signal_nitori_t *n);
/* 结算这一发（寿命尽头 / 泡的 VM 没了 / 被新的一发 B 顶掉 —— 三条入口共用）：
 * 返回该退还给火力的单位数（0 = 不退）。**一手判据**：`th11:0x407828 test esi,esi` 的「VM 没了」那一支。*/
int32_t signal_nitori_expire(signal_nitori_t *n);
/* 提前作废（同一发又按了一次 B 把泡换掉 / 换关 / 删卡）：**不退款、也不进诊断计数**。*/
void   signal_nitori_close(signal_nitori_t *n);
/* 破罩第 `k` 帧（1-based）的消弹半径 px = 32 + 10×(k-1)（一手：先消弹后 `+= 10.0`）。*/
int32_t signal_nitori_break_radius_px(int32_t k);

/* ★★ 2026-09-16u（用户口径）：「**B 的无敌时间不算做罩子的持续时间**；此时撞任何弹幕都应该
 *   认为是没撞 —— **包括其他的无敌、类无敌情况**」。
 *   ⇒ 罩子的寿命按**暴露帧**计（无敌的那一帧不推进），三个输入就是「类无敌」的全集：
 *     · `invuln_cur`     = `player+0x47778`（无敌 zTimer 的 current，`> 0` = 有无敌；
 *       原生 B（`BombMarisa__begin` 120 帧 + 每帧刷 40）、107/117 的救援无敌、复活无敌都写它）；
 *     · `no_hit`         = 「中弹无伤」声明层（113 的灵界闸门等）⇒ 弹被吃掉、`Player__die` 整个被跳过；
 *     · `player_state`   = `player+0x476ac`：引擎自己的中弹判定在 `∈ {2,3,4}` 时就早退
 *       （`th18:0x45cd15`–`0x45cd1a` / `0x45ce71`–`0x45ce76` / `0x45d04e`–`0x45d053` 三处同形；
 *       4 = 决死窗口、2/3 = 死亡与复活）⇒ 那三个状态同样算「类无敌」。
 *   返回 1 = 本帧推进寿命；0 = 本帧暂停（泡**冻结**在原处，跟着自机走但不缩小、不淡出）。*/
int32_t signal_nitori_clock_runs(int32_t invuln_cur, int32_t no_hit, int32_t player_state);

/* 泡的可见曲线 = 一手那三条指令的**逐条等价式**（卡侧每帧按**暴露年龄** `age` 求值）：
 *   · `alphaTime(10, 0, 255)` ⇒ 0..10 帧线性 0 → 255，其后恒 255；
 *   · `scaleTime(20, 0, 1.0f, 1.0f)` + `scaleTime(900, 0, 0.1f, 0.1f)`
 *     ⇒ 0..20 帧线性 0 → 1.0；20..920 帧线性 1.0 → 0.1；920 之后 = 寿命尽头（卡侧删 VM）。
 *   ⚠ 为什么把曲线搬进卡侧：**VM 自己的时间线没法暂停**（脚本的 `+900: delete()` 是绝对时间标签），
 *     而用户口径（2026-09-16u）要求「**B 的无敌时间不算罩子的寿命**（其它无敌同理）」⇒ 只能由卡侧按
 *     暴露帧重算（`❓ 自定` 的只是**落点**，三个数（10 / 20 / 900、终点 1.0→0.1）全是一手）。
 *   ⚠ 返回**千分点整数**（1000 = 一手 scale 1.0），调用方自己除 —— 本仓的**零 x87 不变式**
 *     （`Makefile` 的 `dllx87`）：32 位 x86 的 `float` 返回值走 st(0)，会在我们自己的 `.o` 里留下
 *     `flds`/`fstp` ⇒ **任何返回 `float` 的函数都不许进 DLL**（先例 = `mof_core_bomb_radius_centi()`）。*/
int32_t signal_nitori_bubble_scale_milli(int32_t age);   /* 返回 0..1000（1000 = 一手 scale 1.0）*/
int32_t signal_nitori_bubble_alpha(int32_t age);         /* 返回 0..255 */
/* 这一发还开着退款窗吗（诊断用；寿命看卡侧的 `s_shield.life_left`）。*/
static inline int signal_nitori_busy(const signal_nitori_t *n) { return n && n->active; }

/* ---- ★★ 2026-09-16u：文（超高速）的**残像上色曲线** = TH11 `pl00.anm` script19 的逐条等价式 ----
 * 一手（原文，TH11 `pl00.anm` script19 = 灵梦的残像）：
 *   `color(255, 64, 64); colorTime(15, 0, 64, 64, 255); alphaTime(15, 0, 0); +15: delete();`
 * ⇒ 每一支残像在**它自己的 15 帧寿命**里从 `(255,64,64)` 线性跑到 `(64,64,255)`；
 *   15 支残像各处在不同年龄 ⇒ 整条拖影呈现「**红 → 洋红 → 蓝**」的**时间/位置渐变**。
 * 用户口径（2026-09-16u）：「灵梦的残影看起来是红色，原版更贴洋红**并且有时间渐变**
 *   （等价于不同残影的位置渐变），目前缺乏渐变」。
 * 返回 **0xRRGGBB**（不含 A：alpha 由 TH18 原生残像脚本自己的 `alphaTime` 掌管，卡侧只换 RGB）。*/
int32_t signal_trail_rgb(int32_t age);

/* ============ ★ 2026-09-15c「灵击 → 决死」（**决死 B 形态**）：形态状态 + 代价口径 ============
 * 用户口径（2026-09-15 逐字）：「**基本语义上灵击==B**」「**所以灵击也得决死**」
 *   ⇒ 111 的灵击（库存 == 0 时自干那一支）在**决死窗口内按 B** 时也要救命（与 110 同一件事，
 *     两张卡各自一份状态 —— 两卡的**数值表与账本本来就是分家的**，见 `EVENTS-CONTRACT.md` §7）。
 *
 * ## 一手（TH18 侧，与 110 共用同一批事实；卡侧本轮自己在样本上复核过）
 *   · 决死窗口 = `player+0x476ac`（= `CE_PLAYER_STATE`）**== 4**：`th18:0x45d4e4` 是**全 exe 唯一**
 *     写 4 的点（写点穷举：`0x40da64`/`0x45abbd`/`0x45bff3`/`0x45d5cb` 写 1、`0x45c302` 写 0、
 *     `0x45d21e` 写 2、`0x45d4e4` 写 4）。
 *   · 救命 = `Player__cancel_impending_death` **`th18:0x45d590`**（thiscall(Player*)、无栈参）——
 *     原生自己的「决死 B」就是它：`0x45c2a9` 那一支（窗口没走完）`can_bomb 0x420420` →
 *     `do_bomb 0x420360` → **`0x45c2d7 call 0x45d590`**。
 *   · ⇒ 111 走**同一套** `signal_core_deathbomb_window()`。
 *
 * ## 代价口径（★ 111 这一侧**已由一手钉死**，与 110 那侧**故意取不同值**）
 *   ① **消弹**：`SIGNAL_BOMB_BY_HIT_ACCOUNT` = **1 = 照常按颗数记账**（✅ 一手，见下）。
 *      **一手链（TH11 v1.00a，md5 `6af1e3b37f28293f4fc127ee01d2632c`；另一 agent 钉死、本轮采用）**：
 *        · 真决死 = `Player::on_tick` 的 **state 4 路**：被弹 `Player::sub_432a90` → `th11:0x432a9e`
 *          置 `player+0x928 = 4`；窗口 = `[player+0x948] < 8`（`th11:0x43122d`）；
 *          门控 = `[bomb+0x3c] == 0` + 火力够 + `INPUT & 2` → `Timer(…, 60)`（`th11:0x431286`）+
 *          `Player::do_bomb()`（`th11:0x431293`）+ 火力 −1.00（`th11:0x431298`）→ 回 state 1。
 *        · **决死与普通灵击的代价完全相同**（都只扣火力 1.00、都不扣信仰），而且
 *          **两者都撒 type 8**：消弹入口 `th11:0x40af90`、撒点 `th11:0x40b0ac`。
 *        · ⚠ 先前给的三个候选 **`0x406fbd` / `0x43113f` / `0x43115e` 全被否掉**（`0x406fbd` 是
 *          `Bomb::on_tick__marisa_a` 的消弹，后两个是 **state 0（复活起身）** 的消弹）
 *          —— **这些地址绝不能再当「决死」引用**。
 *      ⚠ **不要把这条推广到 110**：110 侧（TH10）我们**仍取 0（不记账）**，但那是**❓ 自定**、
 *        不是一手结论 —— 它引的那对地址（`th10:0x4258d4`/`0x4258f0`）在 110 的复核里**也被推翻**
 *        （同样是 `Player::on_tick` **state 0（复活上升）** 的清弹；真决死走 `Bomb::start` →
 *        `Bomb::on_tick` → `th10:0x405ac0`，那里第二刀 = `(~[player+0x378c])&1` = **与普通灵击同款**）。
 *        全过程与地址在 `mof_core.h` 的「灵击 → 决死」块。**两张卡在这一条上取值不同，但理由不是
 *        「两作相反」——TH10 那一侧的「不撒」没有一手依据**（用户/任务口径决定的保守取值）。
 *   ② **不扣信仰**：111 的信仰惩罚只走 miss 那条路（见 `signal.c` 的 `on_death_after_deathbomb` +
 *      `signal_core_miss_penalty`）⇒ 决死形态**天然不扣**，且**一手同向**：TH11 的决死
 *      **不触发 `$MISS_COUNT++`、也不清 `$CAPTURE`**（⇒ 不取消命碎）。救下来 ⇒ 窗口不耗尽 ⇒
 *      引擎不广播 `+0x0c` ⇒ 那条 miss 账根本不触发 ⇒ **不需要任何额外开关**。
 *   ③ **荷取退款窗口**：❓ 自定 —— 决死形态**不开**（`SIGNAL_BOMB_BY_HIT_REFUND`）。理由见 `signal.c`
 *      的 `signal_bomb()`（这条特技的前提是「没被打中」，而决死的前提就是「刚被打中」）。登记 `TUNING.md`。
 *
 * ## 故意**不做**的事
 *   · 不挂 `on_death_before_deathbomb`（`+0x10`）自动救人 —— 本卡扮演的是救援层里的「**决死 B**」
 *     （`EVENTS-CONTRACT.md` §5.2 的顺序 **107 → 117 → 决死 B → 113**），语义 = **玩家在窗口内按 B**。
 *   · 不 `ce_rescue_note_claim()`（那只服务 113 的 `+0x0c`，我们这条不走 `+0x0c`）。
 *
 * ⚠ 与 110 的那一份（`mof_core.h` 的 `mof_bomb_form_t`）**故意不共用类型**：两卡的账本/数值表分开，
 *   而且两边的 🟡 依据不同 ⇒ 合成一个会让「翻哪一行」变得含糊。逻辑只有十几行，重复可接受。*/
#define SIGNAL_BOMB_BY_PRESS   0    /* 按 B 主动发动（窗口外；或库存 > 0 ⇒ 交给原生，本卡不动）*/
#define SIGNAL_BOMB_BY_HIT     1    /* **决死 B**：决死窗口内按 B 且库存 == 0 ⇒ 本卡自干 + 救命 */
#define SIGNAL_DEATHBOMB_STATE 4    /* 一手 `th18:0x45d4e4`：`player+0x476ac` = 4 */

/* ✅ 一手：决死形态的消弹**照常入信号账**（1 = 与主动灵击同款按颗数记账；0 = 跳过）。
 * 依据 = TH11 的决死与普通灵击「代价完全相同、都撒 type 8」（消弹 `th11:0x40af90` / 撒点 `th11:0x40b0ac`），
 * 完整地址链与「那三个被否掉的候选」见上面。⚠ **别把这一行与 110 的 `MOF_BOMB_DEATHBOMB_ACCOUNT`
 * 当成同一个旋钮**：110 侧取 0 是 ❓ 自定（它引的地址在复核里被推翻），两边的**理由不同**。*/
#define SIGNAL_BOMB_BY_HIT_ACCOUNT   1
/* ❓ 自定：决死形态是否照常开荷取的退款窗口（0 = 不开）。理由见 `signal.c` 的 `signal_bomb()`。*/
#define SIGNAL_BOMB_BY_HIT_REFUND    0

/* 一发灵击的形态（照 `cards/animal_core.h` 的 `by_hit` 先例进 core ⇒ 可单测）。*/
typedef struct {
    int32_t by_hit;    /* 形态：`SIGNAL_BOMB_BY_PRESS` / `SIGNAL_BOMB_BY_HIT` */
    int32_t cancels;   /* 诊断：本发累计消弹颗数（**无论是否入账**都记）*/
} signal_bomb_form_t;

/* 决死窗口判据（`player_state == 4`，一手 `th18:0x45d4e4`）。返回 1/0。*/
int32_t signal_core_deathbomb_window(int32_t player_state);
/* 开一发：记形态（`by_hit` 由卡侧「检测窗口」后传入）、颗数清零。`f == NULL` 安全。*/
void    signal_bomb_form_start(signal_bomb_form_t *f, int32_t by_hit);
/* 本发又消掉 `bullets` 颗（诊断；`<= 0` 不动）。*/
void    signal_bomb_form_cancel_note(signal_bomb_form_t *f, int32_t bullets);
/* 消弹是否入信号账（1 = 入；0 = 不入）。口径 = `SIGNAL_BOMB_BY_HIT_ACCOUNT`。*/
int32_t signal_bomb_form_cancel_accounts(const signal_bomb_form_t *f);
/* 本发该不该开荷取的退款窗口（1 = 开）。口径 = `SIGNAL_BOMB_BY_HIT_REFUND`。*/
int32_t signal_bomb_form_opens_refund(const signal_bomb_form_t *f);
/* 本发是不是决死形态（诊断/日志用）。*/
int32_t signal_bomb_form_is_deathbomb(const signal_bomb_form_t *f);

/* ======================================================================================
 * ★★ 2026-09-15i：**咲夜(2) 三条 + 早苗(3) 三条**（用户拍板的伙伴映射，**不是复刻**）
 * ======================================================================================
 * ## 用户拍板的映射（2026-09-15 逐字；照做）
 *   TH18 **咲夜(2) = Σ 大小姐（蕾米莉亚）+ 二小姐（芙兰朵露）+ 红美铃**，三条**同时**生效；
 *   TH18 **早苗(3) = Σ 诹访子 + 神奈子 + 恋恋**，三条**同时**生效。
 *   ⚠ 与灵梦/魔理沙那六条**同一条纪律**：机体门由卡侧判 `CE_ADDR_CHARACTER`（`0x4cccf4`），
 *     **不是**「TH18 的机体 = TH11 的机体」。
 *   ⚠ **这六条是用户自定的伙伴映射，不是 TH11 一手**（TH11 的组合号是 `3×CHARACTER + SUBSHOT`，
 *     只有四位自机 × 三种子机 = 12 个组合；咲夜/早苗这两组伙伴**在 TH11 里不存在**）。
 *     ⇒ 除了明确标「一手 / 复刻」的那几处（⑥ 的载体与加法、① 的擦弹判定式、④ 的板底、⑤ 的字段），
 *       **一律标 ❓ 用户口径**，不许包装成复刻。
 *
 * ## 逐条的「一手 / 用户口径」分层（每条函数注释里再写一次，别在这里省略）
 *   | # | 伙伴 | 一手能定的部分 | ❓ 用户口径 / 🟡 的部分 |
 *   | --- | --- | --- | --- |
 *   | ① | 大小姐 · 擦弹持续触发 | 擦弹判定式（`0x4248a0` → `0x45cd90`/`0x45cc20`）、一生一次的位（`0x424984`）、激光不在弹池 | 「持续触发」这个机制本身、3 帧逐弹循环 |
 *   | ② | 二小姐 · 蓄力清屏 | 擦弹圈半径的两半（自机判定半径 + 40.0 边距）、消弹原语与闩锁 | 5 秒 / 2× 半径 / 中断条件 |
 *   | ③ | 红美铃 · 下限 + 增速 | ——（父子 agent 裁定的读法）| 整条（×0.5 的意义、×1.5 的范围）|
 *   | ④ | 诹访子 · 贴板底不扣信号 | 板底 = 引擎自己的自机 y 下界 `0xd800`、线下档的 −0.30 路径 | 「不扣」这个效果 |
 *   | ⑤ | 神奈子 · 收点线下降 | 字段 / 写点穷举 / 没有声明层 | 下降量、相对快照的写法 |
 *   | ⑥ | 恋恋 · 放开键 ⇒ **4 帧**无敌 | 载体 `player+0x47774`、加法语义、原生卡 Aya 的**条件与 +4 帧**（量照抄原生，用户 2026-09-15 拍板）| **多**覆盖了一条「六个动作键全放开」的下降沿（原生只在起步沿触发）|
 *
 * ## 状态进 core 才可测（本仓硬要求）
 *   这六条的新状态**全部**在这一段里（`signal_remilia_t` / `signal_flandre_t` /
 *   `signal_step_mod_t` / `signal_kanako_t` / `signal_koishi_t`），卡侧只做
 *   「读引擎 → 调 core → 落效果」⇒ 主机单测（`tests/test_signal_core.c` §17）钉得住。
 *   ⚠ **既有签名一个都没改**（`signal_core_step` / `signal_core_apply_step` / `faith_signal_*` 冻结）：
 *     ③④ 走的是**新的** `signal_core_step_mod()` / `signal_core_apply_step_mod()`，
 *     `m == NULL` 时与旧路**逐值相同**（单测对拍）。
 * ==================================================================================== */

/* ---- TH18 的其余三个动作键位（`engine/input/th18/01-key-mapping.md` §2 的一手位表；
 *      方向 4 位与射击/低速/炸弹已在上面给过）---- */
#define SIGNAL_IN_ESC        0x100u   /* Esc（暂停）*/
#define SIGNAL_IN_CARD       0x400u   /* C（发动选中的主动卡，`0x45c084`）*/
#define SIGNAL_IN_CHANGE     0x800u   /* D（切卡，`0x45c09c`）*/
/* 「全键位」= **游戏自己配置的六个动作键**（射击/炸弹/低速/暂停/发动卡/切卡）。
 * ⚠ 掩码里还有别的位（`0x40000` 手柄按钮 / `0x200000` 对话推进）—— 那些**不是「键位」**
 *   （`engine/input/th18/01-key-mapping.md` §2 的位表把来源标了）⇒ **不**计进这一组。
 * ❓ 自定：用户口径只说「放开全键位（除移动键）」，没给掩码；这里把「全键位」落成这六个。*/
#define SIGNAL_IN_ACTION_MASK  (SIGNAL_IN_SHOT | SIGNAL_IN_BOMB | SIGNAL_IN_FOCUS | \
                                SIGNAL_IN_ESC | SIGNAL_IN_CARD | SIGNAL_IN_CHANGE)

/* ======================================================================================
 * ① 咲夜×蕾米莉亚（大小姐）：**擦弹持续触发**
 * ======================================================================================
 * ## 为什么必须卡侧自己扫弹池（一手，三条）
 *   ① TH18 的擦弹**每颗弹一生只算一次**：`0x4248a0` 的擦弹分派器在 `0x42496f`
 *      `test BYTE [esi+0x20],0x4 / jne 退出` —— 擦过就置这一位（`0x424984 or DWORD [esi+0x20],0x4`），
 *      此后同一颗弹**永远**不再进擦弹（对照 TH15 的 `bullet+0x1464` 每弹计时器 = 能反复擦）。
 *   ② 擦弹判定式是引擎自己的、两种模式（`0x4248a0` 按 `flags` 选）：
 *        · 圆（`flags & 0x10`）→ `0x45cd90`：`d² < (r + max(br/2.5, 40.0))² + br²`
 *          （常量 `0x4b921c` = 2.5、`0x4b92c4` = **40.0**）；
 *        · 矩形（其余）→ `0x45cc20`：弹心 vs 自机判定框外扩 **24.0**（`0x4b92a4`）。
 *      门：`flags & 0x2`（`0x4248cf`）、弹半径 `br > 0`（`0x4248e9`）、
 *      `flags & 0x40` 时 `br *= bullet+0xf38`（`0x424917`）。
 *   ③ **激光不在弹池里** ⇒ 用户那句「激光本来就持续触发」**结构上自动满足**：
 *      激光走自己的擦弹调用点（`0x44ab35` / `0x44cfef` / `0x44fa53`），本函数只扫子弹池
 *      （`zBulletManager`：`mgr+0xec` 起 2000 槽、stride `0xfa0` —— `engine.h` 的一手 + `pofv.c` / `lolk.c` 两个先例）。
 *
 * ## ❓ 用户口径（2026-09-15 逐字，父 agent 2026-09-15 转达）
 *   「**擦弹 1 帧，然后不计算擦弹 2 帧吧，对于每个子弹执行这个操作**」
 *   ⇒ **逐弹**的 3 帧循环：第 1 帧计入、第 2–3 帧不计、第 4 帧再计入 …
 *   ⚠ 是**逐弹独立**的循环，**不是**全局节流（全局节流会让所有弹同拍，收益随弹数不线性）。
 *   ⚠ 第一帧也算 ⇒ 那颗弹**刚进圈的那一帧会与原生擦弹各记一次**（原生记「一生一次」的那一次）
 *     ⇒ 入圈帧翻倍。这是我们按用户口径逐字落地的**已知后果**，登记 `TUNING.md` §2.1g。
 */
#define SIGNAL_REMILIA_GRAZE_ON_FRAMES   1   /* 用户口径：每颗弹 1 帧计入 */
/* ★★ 2026-09-16 用户改口径（逐字）：「**目前擦弹频率有点超模，改成一颗弹幕每 6 帧擦一次**」——
 *   原口径是「擦弹 1 帧、然后不计算擦弹 2 帧」（周期 3）⇒ 现在周期 **6**（有效仍是 1 帧）。
 *   ⇒ 计不计的分支一字未改，只把这个常数从 2 改成 5。*/
#define SIGNAL_REMILIA_GRAZE_OFF_FRAMES  5   /* 用户口径：每颗弹 5 帧不计 ⇒ 周期 6（每 6 帧擦 1 次）*/
#define SIGNAL_REMILIA_GRAZE_PERIOD \
        (SIGNAL_REMILIA_GRAZE_ON_FRAMES + SIGNAL_REMILIA_GRAZE_OFF_FRAMES)   /* = 6 */
#define SIGNAL_REMILIA_TRACK_MAX         36  /* ❓ 自定：同时跟踪的弹数上限（超出的当帧**不计**、也不进表）。
                                              *   理由三条：① 43 px 圈内的弹数按 TH18 的弹幕密度通常 <10、
                                              *   极密时才上 30；② 每颗弹要在圈里连待 3 帧才吃满收益，
                                              *   漏掉最密那一档的少数弹比「让卡状态爆掉」划算；
                                              *   ③ **硬约束**：`ce_state` 上限 496 字节，本卡已用 288
                                              *   （见 `signal.c` 的 `_Static_assert`）⇒ 这张表最多吃 ~130 字节。*/

typedef struct {
    uint16_t key[SIGNAL_REMILIA_TRACK_MAX];   /* 子弹标识 = **弹池槽号**（0..2000；卡侧 `(b - pool)/stride`）
                                               *   —— 用槽号而不是指针，把这张表压到 16 位/项。*/
    uint8_t  cd [SIGNAL_REMILIA_TRACK_MAX];   /* 冷却剩余帧：>0 = 本帧不计（每帧 −1）；0 = 本帧计入 */
    int32_t  n;        /* 本帧在表的颗数（诊断）*/
    int32_t  total;    /* 诊断：累计**额外**计入的擦弹次数（= 喂给信号槽那一本账的那一份）*/
    int32_t  frames;   /* 诊断：跑过的帧数 */
    int32_t  dropped;  /* 诊断：因表满被丢弃的（弹, 帧）次数 */
} signal_remilia_t;

/* 每帧推进。`keys` = 本帧**进入擦弹半径**的子弹槽号（卡侧扫池填），`n` = 颗数。
 * 返回**本帧额外计入的擦弹次数**（0..n）。`r == NULL` / `keys == NULL` / `n <= 0` 都安全（返回 0，
 * 且**不**清表 —— 卡侧取不到弹池时不该把冷却全部归零）。*/
/* ★★ 2026-09-16m：**多一个出参**（`out_keys` / `out_max` 可为 NULL）—— 本帧**计入**的那些弹的槽号
 * 按顺序写进去（最多 `out_max` 个）。用途：卡侧要对每一个「真的算了一次擦弹」的弹调
 * `ce_do_graze()`（引擎自己的擦弹函数 ⇒ 计数 + 火花 + `se_graze.wav`），见 `signal.c` 的 ⓪ 段。
 * 返回值和以前一样 = 本帧计入的颗数。*/
int32_t signal_remilia_tick(signal_remilia_t *r, const uint16_t *keys, int32_t n,
                            uint16_t *out_keys, int32_t out_max);
/* 归零（删卡 / 换关 / 局末）。`r == NULL` 安全。*/
void    signal_remilia_reset(signal_remilia_t *r);

/* ======================================================================================
 * ② 咲夜×芙兰朵露（二小姐）：**按住 Shift 蓄力 5 秒，松手清掉 2×擦弹圈**
 * ======================================================================================
 * ❓ 用户口径（2026-09-15 逐字）：「按住 shift 时会蓄力，连续蓄力 5s 后放开 shift 可消除
 *   2×擦弹圈半径范围的屏幕」。
 *   · **5s @ 60 fps = 300 帧**（本仓帧率口径：`1 s = 60 帧`，与 `SIGNAL_NITORI_REFUND_FRAMES`
 *     的 75 帧 / `faith_hud` 的 20 帧门同一套）。
 *   · **擦弹圈半径**要去一手找 —— 它就是引擎擦弹判定里**自机那一侧**的那一项：
 *     `r + max(br/2.5, 40.0)`（`0x45cd90`）。`br/2.5 ≥ 40.0` 恒成立（TH18 的弹半径 ≤ 100），
 *     ⇒ 圈的**最小值** = `r + 40.0`，`r` = `player+0x4799c`（非聚焦）/ `+0x479a0`（聚焦），原生 **3.0**
 *     ⇒ 擦弹圈 = **43.0 px**，清屏半径 = `2 × 43.0` = **86.0 px**。
 *     ⚠ 用**最小值**（而不是「每颗弹各算一个圈」）：用户要的是**一个**屏幕范围，
 *       而 `br/2.5` 那部分是**逐弹**的 ⇒ 只有 40.0 这一半是共有的（也正好是「额外圈」的名字来源）。
 *   · 清屏口径 **照本卡既有消弹**：`ce_cancel_radius(pos, r, MAX, mode 0)`（纯删除、不撒道具、
 *     不引入 type 9 / 钱）+ **消弹闩锁**（同帧别的卡已消过 ⇒ 认领失败就不做；**一颗都没消到 ⇒ 交回**）。
 */
#define SIGNAL_FLANDRE_CHARGE_FRAMES  300   /* 用户口径：5 s @ 60 fps */
#define SIGNAL_FLANDRE_CIRCLE_MIN     40    /* 一手 `0x4b92c4`（px；擦弹圈的「额外圈」那一半）*/
#define SIGNAL_FLANDRE_RADIUS_MUL     2     /* 用户口径：2× */

typedef struct {
    int32_t charge;   /* 连续按住 Shift 的帧数（钳在 `SIGNAL_FLANDRE_CHARGE_FRAMES`）*/
    int32_t armed;    /* 已充满（`charge >= 300`）⇒ 等松手 */
    int32_t last;     /* 上一帧 Shift 的**电平**（认边沿用）*/
    int32_t fires;    /* 诊断：成功清屏次数 */
    int32_t breaks;   /* 诊断：被中弹打断的次数 */
    int32_t aborted;  /* 诊断：没充满就松手的次数 */
} signal_flandre_t;

/* 每帧推进：`shift_held` = 本帧 Shift 电平；`interrupted` = 本帧中弹（卡侧传「自机 state ∈ {2,4}」）。
 * 返回 1 = **本帧要清屏**（松开那一下）。`f == NULL` 安全。
 * ⚠ **中断条件（❓ 自定）**：
 *   · 中弹（`interrupted != 0`）⇒ 蓄力清零（这是用户口径外我们加的一条：挨打了还留着蓄力不合理）；
 *   · 松开 Shift 而**没充满** ⇒ 清零（"没充满就松手"）；
 *   · **方向键不参与**（用户口径里没提，且聚焦移动时方向键恒按着 ⇒ 把方向键当中断会让蓄力
 *     在移动中永远充不满）。**登记 `TUNING.md` §2.1g**。*/
int  signal_flandre_tick(signal_flandre_t *f, int shift_held, int interrupted);
/* 归零。`f == NULL` 安全。*/
void signal_flandre_reset(signal_flandre_t *f);
/* 清屏半径（**1/100 px** 定点出入口，与 `signal_core_band()` 的整数纪律一致）：
 * `= 2 × (player_r_x100 + 4000)`。`player_r_x100 <= 0` ⇒ 按原生 3.0 px（300）兜底。*/
int32_t signal_flandre_cancel_radius_x100(int32_t player_r_x100);

/* ======================================================================================
 * ③ 咲夜×红美铃：**信号下限 = 满值的一半 + 增速 ×1.5**
 * ======================================================================================
 * ❓ 用户口径（2026-09-15 逐字）：「信号强度不会低于 ×0.5，信号强度增长速度 ×1.5」。
 * ★ **父 agent 2026-09-15 裁定的读法（本卡照它落地，不自己改口径）**：
 *   · 「×0.5」= **信号槽的下限 = 满值的 50%** = `FAITH_SIGNAL_HI_MARK / 2` = **50000** = 显示 **50.00**
 *     （**不是**「擦弹倍率项 ×0.5」—— 那个读法若成立请先回报，见 `signal.c` 的注释）；
 *   · 「增速 ×1.5」= **所有正增量 ×1.5**（档位给的那一份 + 擦弹给的那一份）。
 * ## 落点：**共用件 `faith_signal_step()` / `faith_signal_add()` 的上游**（只对本卡生效）
 *   `faith_core.c` 的两个函数**一个字节都不改**（110 与所有默认行为不变）——
 *   新入口 `signal_core_step_mod()` 先把增量算出来，再在**本卡私有**的这一层做两件事：
 *     ① `d > 0` ⇒ `d = d + d/2`（= ×1.5，整数向下取整；本仓没有浮点纪律问题）；
 *     ② 下限：`signal_now < FLOOR` 且 `signal_now + d < FLOOR` ⇒ `d = FLOOR − signal_now`。
 *   ⚠ **「档位补满」那一条的 ×1.5 有两档表现**（不是漏做、也不是全生效）：
 *     · `slot < 50.00` 时一手那句 `d = min(+50.00, 100.00 − slot)` **够不着 100.00**
 *       ⇒ 放大后真的更快（`slot = 0` ⇒ 一帧到 **75.00**，原版是 50.00）；
 *     · `slot ≥ 50.00` 时那个 `min` 已经把它压到「正好补满」⇒ 放大后再**钳回** `100.00`
 *       （`slot = 60.00` 时原版 +40.00、我们 +60.00 也会被钳成 +40.00）⇒ 观察不到。
 *     ⇒ 钳必须保住（一手 `th11:0x420aa8` 的 `mov 0x2710`）——**不**为了「让 ×1.5 看得见」去掉它。
 *   ⚠ 负增量（线下 −0.30/帧）**不乘**（用户说的是「增长速度」）。
 *   ⚠ 下限是**逐帧**生效的：被弹 / 换关把槽写 0 之后，**下一帧**的这一步就会把它抬回 50.00
 *     （也就是在咲夜机体上，信号槽实际上「永远不会停在 50.00 以下」）。
 *     这是「信号强度不会低于 ×0.5」的直接后果，登记 `TUNING.md` §2.1g。
 */
#define SIGNAL_MEILING_FLOOR     (FAITH_SIGNAL_HI_MARK / 2)   /* 50000 = 显示 50.00（用户口径）*/
#define SIGNAL_MEILING_GAIN_NUM  3                            /* ×1.5 = 3/2 */
#define SIGNAL_MEILING_GAIN_DEN  2

/* ======================================================================================
 * ④ 早苗×洩矢诹访子：**贴紧板底时线下不扣信号**
 * ======================================================================================
 * ❓ 用户口径（2026-09-15 逐字）：「贴紧板底的时候不会扣除信号强度」。
 * ## 「板底」= **引擎自己的自机 y 下界**（一手，不用魔法常数）
 *   TH18 自机移动段 `0x45b6xx`–`0x45b7f6` 在写回定点位置之前做钳位：
 *     · y（`player+0x630`，**1/128 px 定点**）：`0x45b7c4 cmp edx,0x1000 / jge` ⇒ 下界 **`0x1000` = 32.0 px**；
 *       `0x45b7df cmp edx,0xd800 / jle` ⇒ 上界 **`0xd800` = 432.0 px**（`0x45b7e7 mov [edi+0x630],0xd800`）。
 *     · x（`player+0x62c`，同刻度）：`0x45b735 cmp ecx,-0xa400` / `0x45b7ad cmp ecx,0x5c00`
 *       ⇒ `[-0xa400, 0x5c00]`（`0x5c00` 就是魔理沙那条瞬移用的同一个值）。
 *   ⇒ **「贴紧板底」= `y_fixed >= 0xd800`**（引擎钉住的那个值本身，一格不多不少）。
 *     这就是任务口径要的「用引擎自己的自机 y 界」，不是我们挑的常数。
 * ## 「不扣」的落法（别把「减到 0」与「不减」混为一谈）
 *   线下的那一档是 `FAITH_SIGNAL_BAND_OFFLINE` ⇒ `faith_signal_step()` 给 `−0.30/帧`。
 *   贴板底时我们**把档位换成 `FAITH_SIGNAL_BAND_HOLD`**（那一支的线项 = **0**，不是负数）
 *   ⇒ 这一帧**根本不发生扣减**，而同帧的擦弹加值照旧加上去。
 *   ⚠ **不能**「先减 −0.30，再把槽夹到 0」：`faith_signal_add()` 夹 0 会把**同帧**擦弹挣来的加值
 *     一起吃掉（`faith_signal_step()` 里 `d = −300 + graze_gain` 本来就可能是负的）⇒ 那是另一回事。
 */
#define SIGNAL_BOARD_BOTTOM_FIXED  0xd800   /* 一手 `th18:0x45b7e7`（= 432.0 px @1/128）*/
/* `y_fixed` = 自机 y（`player+0x630`，1/128 px 定点）。返回 1 = 已贴住板底。*/
int signal_core_at_board_bottom(int32_t y_fixed);

/* ======================================================================================
 * ⑤ 早苗×八坂神奈子：**收点线下降**
 * ======================================================================================
 * ❓ 用户口径（2026-09-15 逐字）：「收点线下降」。
 * ## 字段与写点（一手穷举；`CE_PLAYER_POC_HEIGHT` = `player+0x47998`，float）
 *   · `0x45ae8f`（自机初始化段）：`[player+0x47998] = 0x43000000` = **128.0**（同一段还写
 *     `+0x47988`=5.0 / `+0x4798c`=30.0 / `+0x47990`=`+0x47994`=70.0 / `+0x4799c`=`+0x479a0`=3.0）；
 *   · `0x40c275`：`[player+0x47998] = 0x43600000` = **224.0**（`engine.h` 的注释早写着「Kanako 置 224.0」）
 *     —— 这是**原生卡**的一次性写（`0x40c270` 段，`mov eax,ds:0x4cf410` 取自机）。
 *   · **没有任何每帧写点**（全 exe 只有这两个写立刻数的点）。
 * ## ✅ **已迁移到共享 PoC 高度层**（2026-09-15；本节保留为来龙去脉）
 *   用户当天追加：「有一张**原生卡直接是降低 PoC 高度**…**th14 卡也得追这个高度，包括信号系统**…
 *   **PoC 高度有一个指示线，在开局显示，如果开局 PoC 高度变了，显示也得显示变**」
 *   ⇒ 共享的「PoC 高度层」（声明 → 聚合 → 落笔 + 只读访问器）**已落地**在
 *     `sdk_core.h` 的 PoC 块 + `sdk.h` 的 `ce_poc_declare_px*()` / `ce_poc_height()` / `ce_poc_apply()`；
 *     一手考证（原生卡 = id 22 ITEM_LINE / CardKanako、指示线 = `front.anm` script 0x39 + `Gui+0x194` bit 0x2000）
 *     见 `engine/player/th18/05-poc-height-and-indicator.md`。
 *   ⚠ 卡侧原来的**三个迁移点全部改完**（`signal.c` 的 `signal_support_kanako()` / `signal_support_reset()`）：
 *     读现值 → `ce_poc_height()`；落生效值 → `ce_poc_declare_px_id()` + `ce_poc_apply()`；删卡/换关写回 → `ce_poc_release()`。
 *     本卡的 `signal_kanako_restore()` **不再被卡侧调用**（层的基准记账比它更准：原生卡 22 中途把线抬到 224
 *     也不会被我们顶掉），但**保留**为纯逻辑 + 单测（`tests/test_signal_core.c`）。
 *   ⚠ `signal_kanako_tick()` 的接口形状一个字没动 —— 它是「快照基准 + 相对下移」的纯逻辑，与「用谁的方法写引擎」正交。
 * ## 历史（写这一段时的现状，留作对账）
 *   ★ 用户口径要求先查 SDK 有没有**声明式**的 PoC 层（照 `ce_option_pose_*` / 伤害层 / 无敌层那种
 *     「各卡声明 → 层选举/合成」的形状）：**当时查过，没有**（`grep -n "ce_poc\|CE_POC" sdk*.h sdk*.c`
 *     零命中；写这一段时仓里还没有那个件）。全仓对这个字段的引用只有四处，**全是读**：
 *       `sdk.c:950`（收取高度系数 `0x446bf9` 现场，只读）、`cards/collect2x.c:148`（注释）、
 *       `cards/season.c:605` / `cards/th06rank.c:265`（各自只读）；`ce_layer_drop_card()` 里也没有 PoC 一项。
 *     ⇒ 当时只能在本卡里直写。**登记 🟡**（用户 2026-09-15 口径允许这条路）。
 *   ★ **谁会同时写它**：运行时只有两处 —— 引擎的 `Player__reset`（每关一次，写 128.0）与
 *     **原生卡 `0x40c275`**（装上那张卡时一次，写 224.0）。因此本卡用
 *     **「快照基准 + 相对下移」**：每帧读一次现值；若它仍等于我们上一帧写的值 ⇒ 基准不变；
 *     否则（引擎/原生卡刚改过）⇒ **重取基准**再下移。删卡 / 换关 ⇒ **写回基准**。
 *     这样「原生卡把线抬到 224」不会被我们长期顶回 128（我们只是从它那里再往下移一点）。
 * ❓ 下降量：**自定**，登记 `TUNING.md` §2.1g（`SIGNAL_KANAKO_POC_DROP_PX`，一处改一行）。
 *   ★ 2026-09-16x 用户实跑「**早苗回收线的下降幅度有点弱**」⇒ 32 → **96**（128 → 224）。
 *     取 96 的**锚点 = TH18 自己那张「降低收点线」的原生卡**：原生 22 号卡（ITEM_LINE / CardKanako，
 *     `th18:0x40c275`）把同一字段写成 **224.0**，而线的默认值是 **128.0**（`th18:0x45ae8f`；
 *     游玩区高 448）⇒ 那一档就是 **+96**。我们取同一作里既有的那一档，而不是继续拍一个数。
 *   ⚠ 实跑日志（`kanako <写过的帧数>` 每 600 帧 +600、`resnap 0`）已证「每帧都在写、基准没被别人改」，
 *     所以这次纯粹是**幅度**问题，不是机制问题。
 *   ⚠ 与原生 22 号卡同装时我们是在它的 224 **之上**再加 96 ⇒ 320（≈ 游玩区 71%）。
 *     要改档位就改这一个数（48 / 96 / 128 是三档参考）。
 */
#define SIGNAL_KANAKO_POC_DROP_PX    96     /* ❓ 自定（锚点 = 原生 22 号卡的 128→224，= +96）*/
/* ★★ 2026-09-16y（用户口径逐字）：「**同装只多加 0.5 吧**」
 *   ⇒ 追加量按**基准**分两档（基准 = 引擎给这条线的现值快照）：
 *     · 基准 == **默认线高 128.0**（`th18:0x45ae8f`，本卡单独生效）⇒ 追加**全额** 96；
 *     · 基准 **> 默认**（= 已经有别人动过这条线，典型就是**原生 22 号卡把它写成 224.0**，
 *       见 `th18:0x40c275`）⇒ 追加**减半** 48（224 → 272，而不是 320）。
 *   ⚠ 为什么减半必须发生在**同一帧内**（不能由卡侧先读基准再决定）：基准是在 `tick()` 里
 *     用「现值 != 我们上一帧写的值」这一条**当场认下来**的（见函数注释），卡侧在第一帧还不知道
 *     原生卡在不在 ⇒ 判定只能放在 tick 内部。*/
#define SIGNAL_KANAKO_POC_BASE_DEFAULT_PX 128   /* 一手：线的默认值（`th18:0x45ae8f` 写 128.0）*/

typedef struct {
    int32_t base_x100;    /* 引擎基准（1/100 px；0 = 还没取到）*/
    int32_t applied_x100; /* 我们上一帧写进去的值（0 = 没写过）*/
    int32_t writes;       /* 诊断：写过的帧数 */
    int32_t resnaps;      /* 诊断：发现基准被别人改过 ⇒ 重取次数 */
} signal_kanako_t;

/* 每帧：`poc_now_x100` = 引擎当前的收点线（1/100 px；卡侧 `*(float*)(pl+0x47998) × 100`）。
 * `drop_x100` = 本卡要下移的**全额**（1/100 px；`<= 0` ⇒ 只维护基准并返回基准值 = 还原）；
 *   ⚠ 实际追加量由**基准**决定：基准 == 默认 128.0 ⇒ 全额；基准 > 默认（原生 22 号卡那种）⇒ **减半**
 *     （用户口径「同装只多加 0.5」，见上面的常量块）。
 * 返回**本帧该写进去的值**（1/100 px；返回 0 = 现值读不到，卡侧别写）。
 * `k == NULL` 安全（返回 0）。*/
int32_t signal_kanako_tick(signal_kanako_t *k, int32_t poc_now_x100, int32_t drop_x100);
/* 归零（删卡 / 换关 / 局末）。`k == NULL` 安全。⚠ **不**负责写回引擎（那是卡侧 `ce` 的事）。*/
void    signal_kanako_reset(signal_kanako_t *k);
/* 删卡 / 换关时该**写回**引擎的值（1/100 px；0 = 本卡从没写过 ⇒ 什么都不用做）。*/
int32_t signal_kanako_restore(const signal_kanako_t *k);

/* ======================================================================================
 * ⑥ 早苗×古明地恋：**放开全键位（除移动键）⇒ 4 帧无敌**（**量照抄原生**）
 * ======================================================================================
 * ## ★ 参考 = **TH18 原生 29 号卡 Aya（SPEEDQUEEN）** —— 本轮自己取到的一手
 *   `CardAya__on_tick` **`th18:0x40c7f0`**，其中 `0x40ca95`–`0x40cb2a` 这一段：
 *     · `0x40ca95 mov eax,[edx+0x58]` / `0x40ca98 cmp eax,[ebx+0x4793c]` / `0x40ca9e je 跳过`
 *       —— `card+0x58` 是**上一帧**记下的 `player+0x4793c`（`0x40cb3b mov [edx+0x58],eax`）；
 *     · `0x40caa4 test eax,eax` / `0x40caa6 jne 跳过` ⇒ 上一帧必须是 **0**；
 *     · `0x40caac cmp DWORD [ebx+0x476cc],0` / `0x40cab2 jne 跳过` ⇒ **没按 Shift**
 *       （`0x476cc` = `is_focused`：写点 `0x45b3fa mov edx,[0x4ca428] / shr edx,3 / and edx,1`）；
 *     · `player+0x4793c` = **自机方向号**（0 = 站住、1..8 = 八向；写点 `0x45b2cc`–`0x45b3ca`）
 *       ⇒ 合起来 = 「**起步**那一帧（方向号 0 → 非 0），且没按住 Shift」；
 *     · 落点 = **自机无敌计时器 `player+0x47774`**（zTimer `{prev@+0, cur@+4, cur_f@+8}`，
 *       `engine.h` 的 `CE_PLAYER_INVULN_TIMER`）：
 *       `0x40cad2 mov eax,[ebx+0x47778]`（cur）→ `0x40cad8 mov [ebx+0x47774],eax`（prev = cur）
 *       → `0x40cb16 addss xmm1,[0x4b9248]`（`0x4b9248` = **4.0**）→ `0x40cb2a mov [ebx+0x47778],eax`。
 *     ⇒ 原生 = **给无敌计时器 +4 帧**（加法，不是置值）。
 *   ⚠ `engine/card/th18/03-hooks.md` 与任务口径提到的 `0x40D2E0` 那一族**不是文文卡**：
 *     `0x40d2e0` 是原生 **31 Kaguya** 的 `spawn_item(type 7, 玩家位, −π/2, 3.2, 3)`
 *     （该文 §「原生卡的技能目录」自己写着）⇒ 这一条按**`0x40c7f0` 的 Aya 卡**落地。
 *
 * ## 我们照抄了什么 / 换了什么（**必须分开写**）
 *   | 面 | 原生 Aya | 本卡 | 性质 |
 *   | --- | --- | --- | --- |
 *   | 载体 | `player+0x47774` 的 zTimer，**加法** | 同一个字段（经 SDK 声明层写**同一个** zTimer）| ✅ 复刻 |
 *   | 帧数 | **+4** | **4**（`SIGNAL_KOISHI_INVULN_FRAMES`）| ✅ 复刻（用户 2026-09-15 追加口径：「**原生量是 4 帧吗？那就按照原生量**」）|
 *   | 触发 | 起步沿（方向 0→非 0）且**没按 Shift** | **原生条件 ∪ 用户口径的放开沿**（见下）| ✅ 复刻 ∪ ❓ 用户口径 |
 *   ⚠ 为什么落点走 **SDK 的无敌声明层**（`ce_invuln_request(id, frames)` → `ce_invuln_apply()`
 *     在 `ce_item_frame` 里统一写 `player+0x47774`，`sdk.h` ②）：那一层就是为「多张卡都要给无敌」建的
 *     （107 结界结束 / 109 爆风 / 117 霊撃），层取 **max**、不截短别人 ⇒ **本卡不自己直写那个字段**
 *     （自己写会与 107/109/117 抢，且商店里删卡时收不回来）。
 *     ⚠ 语义差别（登记）：层的语义是「**声明剩余帧数**」= 置值语义（`if (t->cur < frames) 写`），
 *       原生 Aya 是「**加 4 帧**」的加法语义。量取 4 帧（= 原生）时两者在**第一次**触发上等价；
 *       差别只在「计时器本来还有剩」时（原生再 +4 ⇒ **可刷成永久无敌**；层是取 max 不叠加）
 *       ——**用户 2026-09-15 拍板保持不累加**，完整推演见下面那一段「刻意差异」。
 *
 * ## ★★ 本轮一手把「按键释放 → 无敌」这一句**对上了原生卡的真条件**（两条，缺一不可）
 *   原生判据（`0x40ca95`–`0x40cab2`，逐条见上）化简后**恰好是**：
 *       `last_dir == 0 && move_dir != 0 && !focusing`
 *     —— 而 `last_dir` 在**射击键按住**时被每帧强制 0：`0x40c80a` 读
 *     `player+0x4779c` bit `0x200`（= 「这一帧按着 Z」，`Player::on_tick` 每帧先清 `0x45c921`、
 *     `Player::move` 按输入 bit0 再置 `0x45ea17`），位置着时卡片走 `0x40c84c/0x40c853`
 *     （`card+0x58 = 0`）后**直接返回、本帧不加无敌** ⇒ 松 Z 的那一帧就满足 `last == 0`。
 *     ★ 完整推导（含「量 = 加 4 帧、无上限闸 ⇒ 可用点 Z / 抖方向刷成永久无敌」与
 *     无敌计时器的写点普查）已回流到 `engine/card/th18/12-card29-aya-speedqueen.md`。
 *   ⇒ 玩家侧读到的就是用户那句话：「**放开 Z / Shift（= 非移动键），按着方向键**」那一下给无敌。
 *     所以**用户口径与原生条件是一回事**，本卡按它落地（**复刻**）；**量也照抄原生（4 帧）**
 *     ⇒ 这条的 (a) 路从条件到量**全是 ✅ 复刻**，只剩 (b) 那一路（见下）是 ❓ 用户口径。
 *   ⚠ 原生那一道门**只认 Z**（X / C / D 不在门里，Shift 单独松开也不给 —— 见那篇 §2 的附录）。
 *     本卡两个都收（**并集**），两个诊断计数分开印（`fires_native` / `fires_user`）：
 *       · (a) 原生条件（复刻）：`last_dir == 0 && dir != 0 && !focus`；
 *       · (b) 用户口径：**「放开 Shift 和 Z」那一下的下降沿**（掩码 = `SIGNAL_IN_Z_OR_SHIFT`）——
 *         ★★ **2026-09-16 用户改口径（逐字）：「恋恋改成放开 shift 和 Z 吧」**。
 *         原口径是「放开**全**键位（除移动键）」（六个动作键 `SIGNAL_IN_ACTION_MASK`），
 *         现在收窄成 **Z | Shift 两个键**：判据 = 「上一帧按着 Z 或 Shift，这一帧两个都松开了」。
 *     (b) 覆盖「站着不动把（Z/Shift）全放开」这种原生不触发的情形；(a)(b) 的代价都是 4 帧无敌 ⇒ 不担心超模。
 *
 * ## ★★ 与原生的一处**刻意差异**：原生可「累加」（能刷成永久无敌），我们**不累加**（用户拍板）
 *   原生那两条写计时器的路**全是 `addss`（加法）**，而且三个门（`0x40ca9e`–`0x40cab2`）之后
 *   **没有**「已在无敌中就不再累加」的闸：
 *     ```
 *     0x40cafc  mulss xmm1,[0x4b9248]      ; ×4.0f（分支 A：`player+0x47780` 那个"类型"非 0 时）
 *     0x40cb04  addss xmm1,[ebx+0x4777c]    ; + 现有计时器
 *     0x40cb16  addss xmm1,[0x4b9248]       ; + 4.0f（分支 B；`0x4b9248` = 4.0f）
 *     0x40cb1e/0x40cb22/0x40cb2a            ; cvttss2si → 写回 `+0x47778`(cur) / `+0x4777c`(cur_f)
 *     ```
 *   ⇒ **原生的无敌可累加**：它的触发条件是「方向输入出现（起步）/ 非聚焦」这一类判定，
 *     **只要能以 1 帧精度反复制造「起步」，就每 2 帧净增 +2 帧** ⇒ 计时器单调上升
 *     ⇒ 实际等于**永久无敌**。
 *     ✅ **2026-09-16 补完（原来这里是 🟡）**：两条配方都坐实了 ——
 *       · **点 Z**（按住方向、每 2 帧按一下 Z）：按 Z 的帧卡片走 `0x40c80a` 的 early-return
 *         支（钉 `card+0x58 = 0` + 不加），松 Z 的帧 ⇒ `上一帧==0 ∧ 方向≠0 ∧ !聚焦` ⇒ **+4**；
 *       · **抖方向**（不碰 Z）：松的帧刷缓存为 0、按的帧 +4。
 *     「谁置 `0x200`」也取到了：`0x45ea17`（`Player::move`，门 = 输入 `0x4ca428` bit0 = Z），
 *     每帧由 `0x45c921` 先清 ⇒ 那个位就是「这一帧按着 Z」。
 *     完整一手（含计时器写点普查、社区解明对照）见 `engine/card/th18/12-card29-aya-speedqueen.md`。
 *   ★ **我们的取舍 = 走 SDK 的声明层**（`ce_invuln_request`，聚合 = **取 max**、**置值**语义）
 *     ⇒ 只有 4 帧，**抖 1 帧也刷不出永久无敌**。
 *     理由：那一层是与 **107 / 109 / 117** 共用的**取 max 契约**（`sdk.h` ②），
 *     本卡直写 `player+0x4777c` / `+0x47778` 会破坏它（谁后写谁赢 ⇒ 别人的无敌被截短，且删卡收不回来）。
 *     **用户 2026-09-15 选定「保持现状（不累加，安全）」** ⇒ 这是一条**刻意偏差**（比原生安全），
 *     **不是 bug**，登记在 `TUNING.md` §2.1g / `AUDIT.md` §DE-3b / `NEXT.md` §2F。
 *   ⚠ **若要改成可累加**：绕开声明层、自己 `addss` 到 `player+0x4777c` 并回写 `+0x47778`
 *     （= 逐字复刻原生那四行）—— 代价是与 107/109/117 的取 max 层**打架**（会被本卡顶回 4 帧基准，
 *     或者本卡被别人覆盖），且删卡路径要自己收。**不推荐**。
 */
/* ✅ **复刻**：量 = 原生 Aya 卡的量 —— `CardAya__on_tick` **`th18:0x40cb16`** 的
 *   `addss xmm1,DWORD PTR ds:0x4b9248`（`0x4b9248` = **4.0f**）→ `0x40cb2a mov [ebx+0x47778],eax`
 *   ⇒ 给 `player+0x47774` 的无敌计时器 **+4 帧**。
 *   ⚠ 用户 2026-09-15 追加口径：「**原生量是 4 帧吗？那就按照原生量**」⇒ 从 1 帧改成 4 帧。*/
#define SIGNAL_KOISHI_INVULN_FRAMES 4

typedef struct {
    int32_t last_dir;     /* ✅ 复刻：本卡记的 `player+0x4793c`（射击键按住时每帧强制 0，照抄 `0x40c853`）*/
    int32_t prev_action;  /* ❓ 用户口径：上一帧是否有非移动键按住（认下降沿）*/
    int32_t fires_native; /* 诊断：(a) 原生条件那一路给了几次 */
    int32_t fires_user;   /* 诊断：(b) 用户口径那一路给了几次 */
    int32_t frames;       /* 诊断：跑过的帧数 */
} signal_koishi_t;

/* 每帧推进。`input` = `CE_INPUT_HELD()`；`move_dir` = `player+0x4793c`（0 = 没按方向、1..8 = 八向）；
 * `focusing` = `player+0x476cc != 0`（**Shift 按住**；⚠ 不是 `CE_PLAYER_IS_FOCUSING 0x4779c`，那个是错登记）。
 * 返回 1 = **本帧要声明 `SIGNAL_KOISHI_INVULN_FRAMES` 帧无敌**（卡侧 `ce_invuln_request(111, …)`）。
 * ⚠ 移动键（`SIGNAL_IN_DIRMASK`）不进 (b) 的判据（用户口径「除移动键」）。`k == NULL` 安全。*/
int signal_koishi_tick(signal_koishi_t *k, uint32_t input, int32_t move_dir, int focusing);
/* 归零。`k == NULL` 安全。*/
void signal_koishi_reset(signal_koishi_t *k);

/* ======================================================================================
 * ③④ 的落点：`faith_signal_step()` / `faith_signal_add()` 的**上游**（本卡私有的修饰）
 * ======================================================================================
 * `m == NULL` ⇒ 与既有的 `signal_core_step()` / `signal_core_apply_step()` **逐值相同**
 * （全域对拍，见 `tests/test_signal_core.c` §17.4(a)）⇒ 111 单卡在灵梦/魔理沙机体上的行为、
 * 以及 110 的一切，都**一个字节不变**。*/
typedef struct {
    int at_bottom;      /* ④ 诹访子：贴板底 ⇒ 线下那一档换成「不推进」（`FAITH_SIGNAL_BAND_HOLD`）*/
    int meiling_on;     /* ③ 红美铃：正增量 ×1.5 + 下限 `SIGNAL_MEILING_FLOOR` */
    int no_line_topup;  /* ★★ 2026-09-16za（106 的协同效应）：「除非满 P，不加信号强度」——
                         *   卡侧从 `ce_full_p_gate_active()` 填；置 1 ⇒ 未满的线上档**不再补满**
                         *   （`faith_signal_step_gated(..., 0)`）。擦弹 / 已满档 / 线下衰减都不变。*/
} signal_step_mod_t;

/* 本帧增量（千分点；语义与 `signal_core_step()` 完全一致，多两个修饰）。*/
int32_t signal_core_step_mod(int32_t signal_now, int band, int32_t graze, int difficulty,
                             const signal_step_mod_t *m);
/* 落值（夹 [0, SIGNAL_MAX]），返回落值后的 signal。`m == NULL` ⇒ 等价于 `signal_core_apply_step()`。*/
int32_t signal_core_apply_step_mod(faith_slot_t *s, int band, int32_t graze, int difficulty,
                                   const signal_step_mod_t *m);

#endif /* SIGNAL_CORE_H */
