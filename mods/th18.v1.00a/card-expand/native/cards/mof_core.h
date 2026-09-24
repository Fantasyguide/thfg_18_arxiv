/* mof_core.h —— 风神录（110）的**纯逻辑**：PV 公式 / 大信仰点判据 / 扣火力 / 卡内奖残。
 *
 * 只 <stdint.h> + `faith_core.h`，主机 gcc 直接测（`tests/test_mof_core.c`）。
 * 引擎访问在 `mof.c`。
 *
 * 依据：
 *   · `docs/superpowers/specs/2026-09-10-mof-faith-design.md` §2.1 / §2.4 / §2.6 / §3 / §8.2
 *   · `docs/superpowers/specs/2026-09-11-system-card-spec-review.md` §3.2 / §3.2.1 / §5.5.1 /
 *     §7.2 / §7.4 / §7.8 / §7.9 / §11 的 #47a #48 / §12
 *   · `mods/th18.v1.00a/card-expand/EVENTS-CONTRACT.md` §3（**计分层：声明 + 聚合 + 单一落笔**）
 *   · ★ **一手（th10 v1.00a 逐条指令）**：`local/_scratch/research-th10-faith-and-graze.md`
 *     —— 2026-09-12 的订正出自它（§3.1：连击表大信仰点 = **+120f**；§1.1：TH10 **无擦弹倍率**；
 *     §4.1：灵击原版门槛 ≥1.00 / 无下限）。
 *     **凡 spec 与它冲突处以它为准**（一手 > 转抄）；它没覆盖的（如高度系数的曲线形状）仍按 spec 实现，
 *     冲突逐条标在函数注释里（`mof_core_height_coeff` / `mof_core_graze_mult`）。
 *   · ★★ **2026-09-15 撤销上面那份文档的一条负结论**（它的 §3.2/§3.4「TH10 无逐帧信仰衰减」）：
 *     那是**绝对地址 grep** 的产物，而真正的衰减是 `eax`（= `0x474c40`）相对寻址 ⇒ 抓不到。
 *     新一手（逐指令 + 「速率字段只有 2 个专用写点、且只被这一处读」的活性证据）：
 *     `local/_scratch/research-th10-faith-units-and-drain.md` §3 / §5 第 9 条 / §6。
 *     ⇒ 110 的逐帧衰减**已恢复**（`mof_core_faith_tick()`），全文见下面那一大段。*/

#pragma once
#ifndef MOF_CORE_H
#define MOF_CORE_H
#include <stdint.h>
#include "faith_core.h"

/* ---- 灵击（§2.1 / §3.2.1；110 与 111 共用同一套门槛与消耗）----
 * 换算式：MoF 下限 0.00 ↔ TH18 下限 1.00 ⇒ TH18 1.00 = MoF 0.00、TH18 2.00 = MoF 1.00。
 * ⚠ 必须自写扣法（原生 `GlobalsInner__spend_power` `0x457480` 在 `0x4574a3` 判 `<= 一档` 就拒绝，
 *   且只能整档扣 ⇒ 落不到 1.05）—— 见 `mof_core_power_spend()`，扣完必须 `ce_repopulate_options()`。
 * ★★ **这是用户裁决，不是复刻**（2026-09-12 一手对照，任务 ④）：
 *   · **TH10 原版**（`research-th10-faith-and-graze.md` §4.1）：门槛 = `CURRENT_POWER/20 != 0`
 *     ⇒ **火力 ≥ 1.00**（`0x4259b0`/`0x4259b3`，恰好 1.00 可发动）；消耗 `add WORD ds:0x474c48,0xffec`
 *     = **−1.00**（`0x4259d4`）；**代码里没有任何下限钳位** ⇒ 可以花到 0.00
 *     （决死灵击 `0x425c43` 同门槛同消耗，但**不扣信仰**）。
 *   · **本卡按用户 2026-09-12 的 TH18 口径**：门槛 **≥ 2.00 档**、每次扣 1 档、**下限 1.05 档**
 *     （「加一个サービス」）。因为 TH18 的火力下限是 1.00 档（MoF 的 0.00），2.00/1.05 是换算+裁决的产物。
 *   ⇒ 「门槛 2.00 / 下限 1.05」**别当复刻写进文档**；要复刻 TH10 就该是 1.00 / 无下限。*/
#define MOF_BOMB_POWER_MIN      FAITH_POWER_MIN_LEVEL     /* 门槛 = TH18 ≥ 2.00 档（TH10 原版 ≥1.00）*/
#define MOF_BOMB_POWER_FLOOR    FAITH_POWER_FLOOR         /* 每次扣 1 档、下限 1.05 档（TH10 无下限）*/
#define MOF_BOMB_POWER_COST     FAITH_POWER_COST

/* 非决死灵击扣信仰（§7.5）。⚠ 「决死」的判据（死亡已发起、`+0x10` 救援层还没跑完）**仍待一手** ——
 * 现在按「放灵击时命数没有减少」的名义实现，登记为待实跑（报告「未解」第 2 条）。*/
#define MOF_SPELL_MISS_COST     FAITH_SPELL_MISS_COST     /* −3000 */

/* 全避 boss 非符 / 符卡 −30000（时符必减）—— 判据：本卡**没放灵击**且符卡正常收掉（§2.2）。
 * ⚠ 符卡边界用 `SPELLCARD+0x78`（`CE_SPELL_FLAGS`，115 先例）。*/
#define MOF_ALL_AVOID_COST      FAITH_ALL_AVOID_COST

/* ---- 大信仰点判据（§7.9 已定；§11 #48 明确「不问落到哪个原生 item type」）----
 * 判据 = 这只敌人**实际掉什么**，不按族名挑：
 *   ① `drop_main == 2`（TH18 的 type 2 = 金钱/点道具）→ 大信仰点；
 *   ② 或 **同时掉钱与 P**（ex_drop 里 type 1 与 type 2 都 > 0）→ 大信仰点。
 * ⚠ **boss 不掉** —— ❓ **自定 / TODO（用户未定）**：boss 判据现在用 `EnemyManager` 的 boss id 表
 *   （`em + CE_EM_BOSS_IDS`，season.c 已实跑的一手口径）当**占位**，已汇总给用户拍板
 *   ⇒ `mof_core_wants_big*` 显式收 `is_boss`，卡侧传 `mof_is_boss(ev->enemy)`。
 *
 * ★ 死亡事件的**门**（[`2026-09-12-shared-enemy-death-event-design.md`](../../../../docs/superpowers/specs/2026-09-12-shared-enemy-death-event-design.md)
 *   §2.4 / review §4.2 的 ⚠ killAll 坑）：`on_enemy_death` 是**广播**，各类死因都会来，必须自己筛：
 *   · **排除 `CE_EDEATH_KILL_ALL`**（`0x4307f1`/`0x430a00`/`0x430c08`）—— 该路径在调 `0x430240`
 *     **之前**已 `memset(enemy+0x6240,0,0xc0)` 并清主掉落 ⇒ 清场杀不掉道具，却仍会被死亡断点计到
 *     「击破」；不排除就会把计数刷满（review §4.2 / §12）；
 *   · **真敌人** = `flags & CE_EDEATH_F_REAL`（= `(flags_lo & 0x31) == 0`；极性见 review §11 #42：
 *     **置位 = 被排除在真敌人之外**）；
 *   · **有掉落表** = `flags & CE_EDEATH_F_HAS_DROP`（= `drop_main != 0`）。*/
#define MOF_DROP_TYPE_MONEY   2    /* zEnemyDrop 的 type 2 = 金钱（= TH18 的点道具）*/
#define MOF_DROP_TYPE_P       1    /* type 1 = P 点 */

/* 死亡事件的 `kind` / `flags` 常量直接用 `CE_EDEATH_*`（定义在 `native/sdk/public.h`）。
 * ★ 本纯逻辑层**不含任何 TH18 地址 / ABI 包装**；`sdk/public.h` 本身也只有 `<stdint.h>` + 类型 + 宏
 *   ⇒ `mof_core.c` 直接 include 它（相对路径 `../sdk/public.h`），主机 `cc` 一样能编。
 *   （不再自己镜像一份常量：两份定义迟早会漂。）*/

/* 死亡事件的门（纯逻辑；`kind` = `CE_EDEATH_*`、`flags` = `CE_EDEATH_F_*`）。返回 1 = 可以继续判掉落。*/
int mof_core_death_qualifies(uint32_t kind, uint32_t flags);

/* ---- 蓝点（自建）分值 = 信仰 × 高度系数（§2.4 / §7.6）----
 * 线上 = 1.0、板底 ≈ 0.54（初版，实跑调）。用**千分比**定点：1000 = ×1.0。
 * ⚠ 我们**不重造高度系数**（§7.6 用户确认）：TH18 原生「线上收 = 满额、线下收 = 打折」本来就在跑，
 *   所以这个系数只是「卡内计数（`mof_bonus`）用哪一种口径」的显式化。
 * ⚠ **一手冲突（2026-09-12 新研究，未按它改）**：`local/_scratch/research-th10-faith-and-graze.md`
 *   §2.1 从 th10 一手（`0x41b4aa`–`0x41b535`，本轮又复核过一次）读出 TH10 的线下蓝点**不是**这条
 *   「从线上 1.0 线性降到板底 0.54」的曲线，而是 —— 线上 = `10×信`（满额），线下一律**先砍一半**
 *   再加一个小线性项：`线下 = 10×信/2 − int((y−144)×(1/304)×(0.5×信 − 5000))`（TH10 线上线 = 144.0f、
 *   板底 − 线上 = 304；信 = 存储值）。⇒ **曲线形状与 spec 不一致**：一手是「**过线即半价**（恰 50%）
 *   + 幅度 <5% 的小修正」，且修正项 `(0.5×信 − 5000)` 的符号随信仰翻转 ⇒ 板底比值并不固定：
 *   初值 信=5000 时板底 = 55%（撞上 spec 的 ≈0.54，纯属巧合），信=10000 时恒 50%，
 *   信=99999（上限）时板底只到 **45.5%**。而 spec/本实现是「线上 1.0 起**平滑**线性降到板底 0.54」
 *   ⇒ 线下刚过线那一段差得最远（0.997 vs 一手 0.50）。端点值二者只在初值附近巧合一致。
 *   本条按 **spec §2.4 / review §7.6** 实现（用户裁决在先）—— 要改用一手式子只需改
 *   `mof_core_pickup_coeff()` 一个函数（并把 `collect_y` 换成**道具自己的 y**：一手用 `[edx+0x3c4]`）。*/
#define MOF_BLUE_COEF_TOP     1000   /* 线上（吃黄）= ×1.0 */
#define MOF_BLUE_COEF_BOTTOM   540   /* 板底 ≈ ×0.54（❓ 初版，实跑调；一手 ≈ 0.55）*/
#define MOF_BOARD_Y            448.0f  /* ❓ 板底世界 y（TH18 游玩区 384×448 的底边）；实跑标定 */

/* ---- 卡内分数奖残（§2.6 定案 ⑤）----
 * 蓝点分**只进卡内计数** `mof_bonus`（不写全局 score、不引入全局分数奖残）。
 * 阈值照 MoF 原版：**20m / 40m / 80m / 150m** ⇒ 越过一次发一次残。
 * ⚠ 量纲待核：MoF 原版的「20m」是得点口径，TH18 这边我们按**分**算。
 *   若实跑发现太快/太慢，只改这四个常量（报告「未解」第 3 条）。*/
#define MOF_BONUS_MAX        4
#define MOF_BONUS_M          20000000   /* 20m */
#define MOF_BONUS_40M        40000000   /* 40m = 20m × 2 */
#define MOF_BONUS_80M        80000000   /* 80m = 20m × 4 */
#define MOF_BONUS_150M      150000000   /* 150m = 20m × 7.5（照抄原版，不整除）*/

/* ---- 判决 ---- */
typedef enum {
    MOF_PICK_DROP = 0,   /* 保持原生掉落（不改写 ex_drop）*/
    MOF_PICK_ADD_BIG,    /* **额外再撒一颗**大信仰点实体（`drop_main == 2` 那条）*/
    MOF_PICK_ADD_SMALL   /* ★ 2026-09-15 起语义收窄为「补齐」：满 P 转换的**主路**是共用件的池扫描
                          *   （`faith_convert_full_p_items()`：原生 P 道具被删掉 + 原位撒点），本判决
                          *   只剩 `on_item_collect` 的**同帧竞态网**（那颗 P 道具抢在扫描前被吃掉）。*/
} mof_pick_t;

/* ---- 大信仰点的难度表（§7.1 / th10 一手 `0x41b57f`）---- */
int32_t mof_core_big_value(int difficulty);   /* E/N 5000 · H 8000 · L/EX 10000 */

/* ---- 大信仰点判据（纯函数：卡侧把 `ce_enemy_death_t` 现场读到的值喂进来）----
 * ▲ 推荐入口 = `mof_core_wants_big_death()`：它自带死亡事件的门（KILL_ALL / 真敌人 / 有掉落表），
 *   卡侧在 `on_enemy_death` 里直接用它，不要自己拼。*/
/* 主掉落是金钱（type 2）且不是 boss ⇒ 大信仰点。§7.9 的判据①。*/
int mof_core_wants_big_main(int32_t drop_main, int is_boss);
/* 主掉落不是金钱，但「同时掉钱与 P」（ex_drop 的 type 1 与 type 2 都 > 0）且不是 boss ⇒ 大信仰点。判据②。*/
int mof_core_wants_big_dual(int32_t drop_main, int32_t money_count, int32_t p_count, int is_boss);
/* 合起来：该不该为大信仰点额外撒一颗；`*value_out` 给本难度的分值。
 * `is_boss` 为真时一律返回 0（boss 不掉，§7.9）—— ❓ boss 判据本身仍是卡的占位（用户未定）。*/
int mof_core_wants_big(int32_t drop_main, int32_t money_count, int32_t p_count, int is_boss,
                       int difficulty, int32_t *value_out);

/* ★ `on_enemy_death` 专用入口：门（`mof_core_death_qualifies`）+ 判据一起做。
 * 判据只用事件里**已有**的字段：`drop_main`（尚未消费）+ `flags & CE_EDEATH_F_HAS_DROP`。
 * ⇒ 这就是「**不按族挑、按这敌实际掉什么触发**」（§7.9）在死亡钩子上的直译。
 * ⚠ `drop_main != 2` 的「同时掉钱与 P」那条**拿不到 ex_drop 计数**（事件不携带它）
 *   ⇒ 只能退到 `on_enemy_drop_pre`（那里 `counts` 就是 ex_drop）。见 `mof.c` 的双路说明。
 * ⚠★ **2026-09-15 起已退役**：上面这三个（含 `mof_core_wants_big_death`）都是**旧口径**
 *   （「这只敌掉的是不是金钱」），被用户 2026-09-15 的裁决取代（「金钱和绿点总之别重合」⇒ 走
 *   「判据收窄」）。**110 侧不再调用它们**（改走下面那三个 `*_host` 入口）；保留只为**回归断言**
 *   （`tests/test_mof_core.c` 仍在测），签名冻结、**新代码别用**。*/
int mof_core_wants_big_death(uint32_t kind, uint32_t flags, int32_t drop_main, int is_boss,
                             int difficulty, int32_t *value_out);

/* ============================ ★★ 大信仰点判据 = 宿主表（2026-09-15 换掉旧口径）============================
 *
 * **用户裁决（2026-09-15）**：「**金钱和绿点总之别重合**」 ⇒ 走**方案 1（判据收窄）**：
 *   把「`drop_main == 2` 就给一颗」换成「**只有原作真的声明了 type 3 的那批敌人的对应物才给**」。
 *   起点是用户实测到的现象：旧判据下**每只掉钱的敌人**都撒一颗大信仰点，位置又和金钱同点
 *   ⇒ 「大绿点与金钱完全重合」。
 *
 * **一手预研（TH10 v1.00a 全量穷举，全套在 `mof_big_faith_table.h` 的头部注释里）**：
 *   TH10 掉 type 3（大信仰点）的敌人 = **绿妖精族**（`G` 前缀，40 条 `dropMain(3)` 声明）
 *   + 大妖精 `Girl01/02/03`（13 个「创建期第 6 实参 = 3」且子程序自己不改写的站点）；
 *   **boss 一个都没有**；`dropExtra(3,*)` 全 exe **零条**。
 *   映射到 TH18 ⇒ 表里 4 个**视觉身份**（`(anm 源, 主脚本)`）：`(2,10)`、`(2,56)`、`(2,68)`、`(2,80)`。
 *
 * **为什么要这种键**：`ce_enemy_death_t` 里**没有**敌人类型号（`id` 是逐实例递增的），
 *   而 `zEnemyData+0x268/0x26c` 恰好记着「这敌画的是哪个 anm、哪条脚本」（一手
 *   `th18:0x436c85`/`th18:0x436c91`，只在槽号 K==0 时写；`anmSetMain` 的同两处在
 *   `th18:0x431659`/`th18:0x431675`）—— 这就是「绿妖精」的机器可读形式。
 *   运行时绝对偏移：`enemy+0x1494` = N、`enemy+0x1498` = S（= `zEnemy+0x122c`(`CE_ENEMY_DATA`)
 *   + `0x268`/`0x26c`），卡侧读它见 `mof.c` 的 `mof_enemy_anm_identity()`。
 *
 * **语义**：在表里 ⇒ 死亡时撒一颗大信仰点（值 = 本难度 5000/8000/10000，一手 `th10:0x41b8cc`）；
 *   不在表里 ⇒ 什么都不撒。**中绿点（满 P 转换）不走这条路**（`mof_core_full_p_replace()` /
 *   共用件池扫描），本表**不影响**它。*/

/* 死亡事件的**门**（不含「必须有掉落表」那一条；见 `mof_core.c` 里那段为什么去掉）。*/
int mof_core_death_is_real(uint32_t kind, uint32_t flags);
/* 查宿主表：`anm_src` = `enemy+0x1494`、`anm_script` = `enemy+0x1498`。命中 = 1。*/
int mof_core_big_faith_host(int32_t anm_src, int32_t anm_script);
/* 卡侧唯一入口：门（非 KILL_ALL / 真敌人）+ 宿主表 + 非 boss ⇒ 该不该撒 + 分值。
 * `is_boss` = `(enemy+0x635c & CE_ENEMY_FLAG_BOSS) != 0`（一手：ECL opcode 512 `setBoss` 置位）。*/
int mof_core_wants_big_host(uint32_t kind, uint32_t flags, int32_t anm_src, int32_t anm_script,
                            int is_boss, int difficulty, int32_t *value_out);

/* ★★ 2026-09-18：「**被我们变绿的**妖精」专用入口 —— 同一个门 + 非 boss，**但不查宿主表**
 *   （判据是「这张卡把它变绿了」，由 `cards/mof_green_core.h` 的表决定；键仍然是 `(N,S)`，
 *   只是那批妖精的 `(N,S)` 不在原始宿主表里）。
 *   用户口径把两件事绑成一个开关：「**变绿的妖精 = 掉大绿点的妖精**，不许分开」⇒
 *   因此这里**不做**任何「掉落类型」判断（`drop_main` 一律不参与），分值 = 本难度 5000/8000/10000。*/
int mof_core_wants_big_green(uint32_t kind, uint32_t flags, int is_boss, int difficulty,
                             int32_t *value_out);

/* 满 P 时原生 P 点 → 小信仰点（§7.9 / #48）：**旧口径的通用入口**。
 * ⚠★ 2026-09-15 收口后**两张卡都不再调它**（110 与 111 都换成下面那个 TH18 类型版
 *   `mof_core_full_p_replace()` + 共用件 `faith_convert_full_p_items()`）—— 它按**掉落表**判
 *   （`drop_main == 1` 或 `p_count > 0`），而「满 P 期间场上不该有 P 道具」这条不变式要看**场上**，
 *   只有池扫描做得到。**保留**（签名冻结、单测仍在跑）只为向后兼容；新代码别用它。*/
mof_pick_t mof_core_pick_p_item(int32_t drop_main, int32_t p_count, int32_t power,
                                int32_t max_power, int is_boss);

/* ---- ★★ A/B 满 P 转换：**替换，不是加撒**（用户 2026-09-15 定案；本轮收口）--------------
 * **一手 = 改写道具自己的 type**（`th10:0x41bbf7`–`0x41bc23`，`ItemManager::spawn_item` 内
 * **在道具字段被填之前**）：`cmp WORD ds:0x474c48,0x64`（火力 = 100 单位 = **5.00** ⇒ 满 P）→
 * `type−1 > 0xa` 出局 → 11 字节索引表 `th10:0x41be28` = `[00,02,02,00,02,02,02,02,02,01,01]`
 * + 3 项跳表 `th10:0x41be1c`：**type 1 / 4 → 9（小信仰点）**、**type 10 / 11 → 5（点道具）**、其余不变。
 * ⇒ 满 P 时那份 P 点**不再存在**（不是「P 点 + 信仰点」两份）。
 * 第二处一手 = **满的那一帧全场转换** `th10:0x41ba50`（`th11:0x4240f0` 同形：原位 + 角 −π/2 + 速 2.2）。
 * 两处合起来是**同一条不变式**：「满 P 期间场上不存在 P 类道具」。
 *
 * **我们的落点（★ 判断依据）**：TH18 **没有信仰点这个 item type**（type 1/3 是 P、9–0xe 是引擎自己的
 * 「分数转换点」族）⇒ 「把 type 改成 9」在 TH18 里改出来的**不是绿点**，而是另一个原生道具
 * （`th18:0x446547` 的加分 handler）⇒ 字面照抄做不到，**按语义照抄**：
 *   · **主路**（A + B 的实现）= 共用件 `faith_convert_full_p_items()`（`faith.c`）：满 P 时扫原生
 *     道具池，把 P 类道具**原位删掉**（照引擎自己的清尾），在**它自己的位置**撒一颗 **type 9 档**的
 *     自建点（角 −π/2、速 2.2，一手实参）。卡侧每帧 tick 调一次，放在 `faith_pool_tick()` **之前**
 *     （本帧转换出来的点当帧就建 VM / 受物理 / 被收）。
 *   · **兜底**（本判据 + `on_item_collect`）= **同一帧的竞态网**：引擎的道具 tick 有可能在我们扫到它
 *     之前就把那颗 P 道具喂给了玩家（玩家贴着敌人站），而那一颗**已经**拿过原生的满 P 身价分 ⇒
 *     仍然补一颗信仰点，否则「TH10 里它本来该是信仰点」这件事就丢了。**正常路径下它不触发**
 *     （道具已被主路删掉）⇒ 不会出现用户点名不许的「既算 P、又加撒一颗」。
 *
 * ⚠ **TH18 的类型号与 TH10 完全不同**（一手 `th18:0x44625f` 的分派：`[item+0xc78]` = type，
 *   `type−1` 索引 `th18:0x44685c` → 跳表 `th18:0x446830`）：
 *     | TH18 type | 语义（handlers） | 一手 |
 *     | --- | --- | --- |
 *     | 1 | **小 P**（`collect_power(1)`，`0x446a7b`；满 P 时**同一 handler 内**换成身价分） | `th18:0x446870` |
 *     | 2 | 钱（`0x446b00`） | `th18:0x4463ba` |
 *     | 3 | **大 P**（`collect_power(100 = 1.00 档)`，`0x446330`；满 P 时 +2000 分） | `th18:0x4462d8` |
 *     | 4 / 6 | 残机碎 / 符卡碎（`0x457570` / `0x457690`） | `th18:0x4463c6` / `0x446465` |
 *     | 5 | 1UP（命 +1，钳 `LIVES_STOCK`） | `th18:0x4463d6` |
 *     | 8 | **补满火力**（`collect_power(MAX_POWER)`，`0x44627f`；弹窗 = 淡黄 `+`） | `th18:0x44627f` |
 *     | 9–0xe | 点道具（20 分档，107 spec §4.2） | `th18:0x446547` |
 *     | 0x10–0x13 | 卡道具 | `th18:0x446531` |
 *   ⇒ 「P 类」在 TH18 = **{1（小 P）, 3（大 P）}** —— 它们对位 TH10 的 type 1（小 P）与 4（大 P）
 *     （两者都在 TH10 的表里 → 9）。TH10 的 type 10/11 是 **P 点的外观变体**（`spawn_item` 另写
 *     `[item+0x3e4]`），TH18 没有这一档 ⇒ 那半条表**无从对应**（登记在 `TUNING.md` §2.5）。
 *   ⚠ TH18 的 type 8（补满）**不接**：TH10 没有「补满」这一档（它的 type 4 是 +1.00 档），
 *     我们只接「按量加火力」的两种 ⇒ type 8 是否也该算「大 P」留给实跑/用户拍板（`TUNING.md` §2.5）。
 *   ⚠ 「P 类」的**类型号只有一份出处** = `faith_core.h` 的 `FAITH_TYPE18_P_SMALL/BIG`
 *     （共用件 `faith.c` 的池扫描也要用同一组号 ⇒ 这里只是别名，别再写字面量）。*/
#define MOF_FULLP_ITEM_SMALL_P   FAITH_TYPE18_P_SMALL    /* TH18 type 1 = 小 P（↔ TH10 type 1）*/
#define MOF_FULLP_ITEM_BIG_P     FAITH_TYPE18_P_BIG      /* TH18 type 3 = 大 P（↔ TH10 type 4）*/
/* 满 P 时这颗 TH18 P 道具**该被替换**（返回 1 = 该被替换）。
 * 主路 = `faith_convert_full_p_items()`；本判据只服务 `on_item_collect` 那张**同帧竞态网**（见上）。
 * `max_power <= 0` 或 `power < max_power` ⇒ 0（不满 P 不转化，照一手）。*/
int mof_core_full_p_replace(int32_t type18, int32_t power, int32_t max_power);

/* ==================== ★★ P 点火力 +0.05 档（110 / 111 共用；用户 2026-09-15 要求）====================
 *
 * 用户原话：「装了 110 或 111，P 点变成 +0.05」。
 *
 * ## 一、TH18 原生一颗小 P 给多少 —— **+1 单位 = +0.01 档**（一手，逐条可复跑）
 *   材料 = `local/_scratch/th18_full.asm`（`th18.exe` v1.00a，imagebase `0x400000`）：
 *     ① 类型分派 `th18:0x44625f`（`mov edx,[edi+0xc78]` = 读 `item+0xc78` 的 type）
 *        → 19 字节索引表 `th18:0x44685c` + 11 项跳表 `th18:0x446830`；
 *        `type 1`（小 P）落跳表第 0 项 **`0x4462cc`**（全表见本文件的 §满 P 改写表）。
 *     ② `th18:0x4462cc: mov ecx,edi; call 0x446870` —— P 例程（`ecx` = item）。
 *     ③ `0x446870` 的火力支：`0x446981: cmp eax,[0x4ccd3c]`（eax = `CURRENT_POWER`、
 *        另一侧 = `MAX_POWER`）→ `jl 0x446a7b`；那里 **`0x446a7b: push 0x1`** +
 *        `0x446a7d: mov ecx,0x4cccdc`（= `GLOBALS.inner`）+ `0x446a87: call 0x4573f0`
 *        ⇒ **`add_power(GlobalsInner*, 1)`**：**一颗小 P = 1 单位**。
 *        （`0x446a82: mov edi,0x64` 那个 100 是**分值**身价，走 `0x446ac0` 的 `SCORE += edi/10`，
 *          不是火力 —— 别把它当成 +100 单位。）
 *     ④ `0x4573f0` = **`GlobalsInner__add_power(amount)`**（★ 本仓首次命名，逐条读出的语义）：
 *        `[edi+0x5c] += amount` 并钳到 `[edi+0x60]`，返回「档数是否变了」：
 *          · `0x4573fb mov ecx,[edi+0x5c]` = `0x4ccd38` = `CURRENT_POWER`；
 *          · `0x4573fe mov eax,[edi+0x60]` = `0x4ccd3c` = `MAX_POWER`；
 *          · `0x457465 idiv [edi+0x64]` = `0x4ccd40` = `POWER_PER_LEVEL` = **100**。
 *        （`0x457480` = `spend_power` 是它的镜像，本仓 `engine.h` 的 `CE_FN_SPEND_POWER` 已有一手。）
 *     ⑤ `POWER_PER_LEVEL = 100` 的写入点：`th18:0x45a968`–`0x45a98b`（`Player__initialize`）
 *        `mov DWORD PTR [eax+0x24],0x64` →（`imul [eax+0x20]`）→ `MAX_POWER = 100 × [eax+0x20]`、
 *        `POWER_PER_LEVEL = 100` ⇒ **一档（1.00）= 100 单位**。
 *   ★ 旁证（本仓既有、独立于上面这条链）：`cards/th06rank_core.c` 的 `t06_power_small_gain()`
 *     一直按「TH18 小 P = th06 `Inc(1)` = +0.01」实现 —— 与一手同量纲。
 *
 * ## 二、目标值 = **+0.05 档 = 5 单位**（与 TH10 / TH11 的一手口径对齐）
 *   · TH11：P 点 = 火力 **+1 单位**（`engine/item/th11/01-item-types-and-popup.md`：
 *     type 1/10 → `th11:0x423b34 mov eax,1; call 0x420a00`），而 TH11 的 **1.00 档 = `BOMB_COST` = 20 单位**
 *     （`local/_scratch/research-th11-bomb-lifefrag-hud.md` §1.4）⇒ **+1 单位 = +0.05 档**；
 *   · TH10：`CURRENT_POWER` 的单位就是 0.05（`local/_scratch/research-th10-faith-and-graze.md` §3.5
 *     的 `th10:0x4259a6` `/20`）⇒ 小 P = +1 单位 = **+0.05**、大 P = +20 单位 = +1.00。
 *   ⇒ TH18 侧换算 = **5 单位**。
 *
 * ## 三、替换 vs 补差 —— **语义是替换（总 +5），实现是补差（只补 4）**
 *   原生已经给了 1 单位 ⇒ 我们只补 **5 − 1 = 4**（`MOF_P_TOPUP_UNITS`）。
 *   若写成「再给 +5」= 一颗 P 点共 **+6 单位 = 0.06 档**，正是用户点名不许的「叠加成双倍」。
 *
 * ## 四、落在哪个接缝：**`on_item_commit`（`th18:0x44658a`）**，不是 `on_item_collect`（`0x44625f`）
 *   两个钩子都是 SDK **现成**的（`native/sdk/public.h` 的 `ce_hooks_t`；断点表见 `native/sdk.c` 头表），
 *   所以**不用新开断点**。为什么必须是后者（这条不是风格问题，是行为正确性）：
 *     ① 原生那一发 +1 发生在**两个钩子之间**（collect 在 type 分派前、commit 在 handler 跑完之后）；
 *     ② 而原生「跨档了吗」是**拿被它改过的值**算的（`th18:0x4573f0` 尾：`ebx − amount` 与 `ebx`
 *        各 `idiv [edi+0x64]` 比一次）⇒ 在 collect 里先写 4 会让它的比较变成
 *        `floor((p+4)/100)` vs `floor((p+5)/100)`，**只在 `p ≡ 95 (mod 100)` 时报**，
 *        而真正跨档的是 `p ≡ 95..99` ⇒ **5 次里错 4 次**（`p = 2.00 → 2.05` 这种必报的升级会静默）
 *        ⇒ 原生的升级收尾（`repopulate_options` = 子机按新火力档重建 + `on_power_level_change` 广播
 *        + 升级弹窗 + 音效）就丢了 —— 子机档位对不上，是**看得见**的行为错；
 *     ③ 在 commit 里补 4：原生那一步逐字节不变（它自己那 1 单位的跨档判定照旧），
 *        我们这一补若跨了档、而原生没报，由 `mof_core_p_topup()` 的 `repop` 标志点名
 *        ⇒ 卡侧补一次 `ce_repopulate_options()`（**与 110/111 的灵击扣火力同款收尾**，
 *        见 `mof_core_power_spend()` 的用法与 `native/engine.h` 的 `CE_FN_PLAYER_REPOPULATE_OPTIONS`）。
 *
 * ## 五、满火力：**不越上限**
 *   原生在 `CURRENT_POWER >= MAX_POWER` 时**不给火力**（`th18:0x446981` 的 `jl` 不跳 ⇒ 走身价分那一支）
 *   ⇒ 我们同样不给（`delta = 0`）；未满时最多补到 `MAX_POWER`（`room` 夹取）。
 *
 * ## 六、谁落笔（**两张卡同装只算一次**）
 *   `mof_core_p_owner()` = 「**id 最小者落笔**」，与仓里既有的选举规则同形
 *   （`ce_optpose_*` 低速子机声明层 108 > 111、`ce_dmg_applier()` 的伤害层落笔者）
 *   ⇒ 110 在场就 110 落笔、111 让位；判据是 SDK 现成的 `ce_owned()` 存在性查询
 *   （spec-review §3.1 的那个例外条就是这条；`cards/th06rank.c` 同款用法）。
 *   ⚠ **不用按帧闩锁**（`ce_cancel_latch_*` 那种）：那个闩锁每帧在 `ce_item_frame` 里 reset，
 *   而本层是**每颗道具**一次 —— 用按帧闩锁的话，同一帧收到的第二颗 P 点就白给了
 *   （同帧收一撮 P 是常态，收点波尤其如此），所以闩锁的粒度根本不对，不是「哪种更好」的问题。
 *   ⚠ `type 3`（大 P，`th18:0x4462d8`，原生 +`POWER_PER_LEVEL` = +1.00 档）**不在本层**：
 *   用户说的是「**P 点**变成 +0.05」（TH10/TH11 的**小 P** 口径），把大 P 也改成 0.05 是**削弱**。
 *
 * ## 七、本层的 ❓ 自定项（逐条登记在 `TUNING.md` §2.5b）
 *   · ① **只管 `type 1`（小 P）**，`type 3`（大 P）/ `type 8`（补满）不动 —— **❓ 自定（口径解释）**。
 *     「P 点」按 TH10/TH11 的**小 P** 读（TH10 小 P = type 1、大 P 是另一个 type 4，两者原生都是
 *     +1 单位 / +20 单位）⇒ 大 P 保持 +1.00 档。若用户的「每个 P 点」本意含大 P，改 `MOF_P_ITEM_TYPE18`
 *     这一处不够（大 P 原生给的是整档），要动的是上面那段判据 —— 先问用户，别自行扩大。
 *   · ② **我们自己那一补跨档时只补第一步**：一手 `th18:0x446a90` 那三步是「`0x45d5e0` 重建子机 +
 *     `0x4645f0` 弹升级字 + `0x476c70` 播音 `0xd`」，本层只做第一步 `ce_repopulate_options()`
 *     —— **❓ 自定（纯表现层）**。要一并做就在两个 commit 回调的 `t.repop` 分支里加，实跑听感定。
 *   · 其余全部是**复刻**（落点 / 满火力不给 / 补差额 4 / 选举规则），逐条带一手地址，见上面一~六段。*/

#define MOF_P_ITEM_TYPE18      1   /* TH18 item type 1 = 小 P（本层只管它）*/
#define MOF_P_POWER_UNITS      FAITH_P_TOPUP_NORMAL
                                   /* 一颗 P 点的**总**火力增量 = +0.05 档 × 100 单位/档。
                                    * ★ 2026-09-15h：改成**共享策略的常态值**（`faith_core.h` 的
                                    *   `FAITH_P_TOPUP_NORMAL`）—— 一个数只有一个出处，改一处两边一起动。
                                    *   ⚠ 爱丽丝生效时**不走**这个常量：走 `faith_p_topup_take()` 给的
                                    *   2/3 交替值 + `mof_core_p_topup_units()`（见 `faith.h` 的共享策略段）。*/
#define MOF_P_NATIVE_UNITS     1   /* 原生那一发（`th18:0x446a7b push 1` → `0x4573f0` = add_power）*/
#define MOF_P_TOPUP_UNITS      (MOF_P_POWER_UNITS - MOF_P_NATIVE_UNITS)   /* 补差 = 4（**不是 5**）*/

typedef struct {
    int32_t delta;   /* 在 commit 断点该再补多少（单位 = TH18 的 1/100 档）；0 = 一个字节都不写 */
    int     repop;   /* 1 = 这次补差跨了一档、而原生那一步没报 ⇒ 调用方必须 `ce_repopulate_options()` */
} mof_p_topup_t;

/* `power_after_native` = **原生 +1 已经落地之后**从 `CE_CURRENT_POWER`（`0x4ccd38`）读到的值；
 * `max_power` = `CE_ADDR_MAX_POWER`（`0x4ccd3c`）。返回本颗 P 点该补多少 / 要不要补跨档收尾。*/
mof_p_topup_t mof_core_p_topup(int32_t power_after_native, int32_t max_power);

/* ★★ 2026-09-15h：**同一件事，但总量由共享策略给**（`total_units` = 本颗小 P 的**总**增量：
 * 常态 `FAITH_P_TOPUP_NORMAL`(5)，爱丽丝生效时 2/3 交替）。
 * 上面那个旧签名 = 本函数 + `MOF_P_POWER_UNITS`（**行为逐字节不变**，旧签名因此可以继续被单测钉着）。
 * ⚠ 卡侧**不许**自己算 2/3：`total_units` 只许来自 `faith_p_topup_take()`（`faith.h`）。*/
mof_p_topup_t mof_core_p_topup_units(int32_t power_after_native, int32_t max_power, int32_t total_units);

/* 谁落笔：110 < 111（id 最小者）⇒ 110 在场返回 110、否则有 111 返回 111、都不在返回 0。
 * 卡侧的用法（两边都传同一组输入，结论必然一致）：
 *   110：`mof_core_p_owner(1, ce_owned(111))`（自己就是那张 110）
 *   111：`mof_core_p_owner(ce_owned(110), 1)` */
int mof_core_p_owner(int have110, int have111);

/* ---- 蓝点分值（§2.4）---- */
/* 高度系数（千分比）：y <= 回收线 → TOP；线下一路插值到板底 BOTTOM（`board_y` = 板底世界 y）。*/
int32_t mof_core_height_coeff(float y, float poc_y, float board_y);
/* ★ **生产路径**用的那一支（2026-09-12 审计修，任务 1）：只吃「收集高度 + 当前回收线读数」，
 * 板底用 `MOF_BOARD_Y`。`mof.c` 的 `mof_on_pickup()` 每收到一颗点就调它一次
 * ⇒ 线下**真的走线性插值**（旧实现是 TOP/BOTTOM 二元：`faith_player_above_line() ? TOP : BOTTOM`，
 *   线下 1px 就吃 0.54 ⇒ `mof_core_height_coeff()` 当时是**死代码**）。
 * 边界（与 spec §2.4 / review §7.6 一致）：线上（含恰好过线）= TOP 1.0；板底 = BOTTOM 0.54；
 * 板底以下**夹在** BOTTOM（不作负数外推）；`poc_y >= board_y`（跨度非正）退化为 BOTTOM。
 * `collect_y` = 收集那一刻的高度（= 自机 y，理由与误差见 `mof.c` 的 `mof_on_pickup()` 注释）。*/
int32_t mof_core_pickup_coeff(float collect_y, float poc_y);
/* 一颗蓝点的分值 = 信仰 × 高度系数（向下取整）。`faith` 是 MoF 侧的信仰（= PV 的基础）。
 * ⚠ MoF 原版蓝点就是「点道具」，分值 = 最大得点 × 高度系数（§7.6）；我们**不写全局分数**，
 *   只把结果累进 `mof_bonus`（§2.6）。*/
int32_t mof_core_blue_value(int32_t faith, int32_t coeff_permille);

/* ---- 卡内奖残（§2.6）---- */
typedef struct {
    int64_t bonus;      /* 卡内累计（分）*/
    int32_t awarded;    /* 已经越过几档（0..MOF_BONUS_MAX）*/
} mof_bonus_t;

void    mof_bonus_init(mof_bonus_t *b);
void    mof_bonus_add(mof_bonus_t *b, int64_t delta);
int32_t mof_bonus_awarded(const mof_bonus_t *b);
/* 越过阈值时返回本次应发的残机数（0..MOF_BONUS_MAX），并把 awarded 推进。*/
int     mof_bonus_take(mof_bonus_t *b);
int32_t mof_bonus_threshold(int tier);   /* tier 0..3 → 阈值；越界返回最后一个 */

/* ---- 扣火力（§5.5.1 三个坑；110 与 111 共用）----
 * 门槛 ≥ 2 档；每次扣 1 档；**下限 1.05 档**（用户「加一个サービス，最后一个灵击只会扣到 1.05」）。
 * ⚠ 由于目标值 1.05 ≠ 整档，**原生 `spend_power` 做不到** ⇒ 这是自写扣法的纯逻辑部分：
 *   卡侧照 `new_power` 写 `CURRENT_POWER`（`0x4ccd38`）+ `ce_repopulate_options()`。*/
typedef struct {
    int     ok;          /* 1 = 放得出去 */
    int32_t new_power;   /* 扣完的火力（定点，一档 = 100）*/
} mof_power_t;

/* `power` 是**定点**火力（`CE_CURRENT_POWER()` 的原始值：一档 = 100）。*/
mof_power_t mof_core_power_spend(int32_t power);
int         mof_core_power_can(int32_t power);

/* ============================ ★★ 110 的逐帧信仰衰减（2026-09-15 恢复）============================
 *
 * **背景（必须知道，否则会再删一次）**：本机制在 2026-09-12 被当成「TH10 没有」整段删掉。
 * 那个结论出自**绝对地址 grep**，而真正的衰减全在 `eax` 相对寻址里（`eax = 0x474c40`，
 * `[eax+0xc]` = `0x474c4c`）⇒ `grep 'ds:0x474c4c'` 抓不到 ⇒ 被误判成「没有」。
 * 2026-09-15 的复核（`local/_scratch/research-th10-faith-units-and-drain.md` §3 / §5 第 9 条）推翻了它。
 *
 * **一手（th10 v1.00a，样本 md5 `7dc488d82c81dd4aee4ba098b8804d83`）**：
 *   · **门**（`th10:0x418740`，**三个条件全部满足才衰减**，任一不满足就整段跳到 `th10:0x418769`）：
 *       ① 无 Boss   `[ENEMY_MANAGER+0x10] == 0`  （`th10:0x418746`）
 *       ② 无对话   `[GUI+0x9eb8] == 0`          （`th10:0x418751`）
 *       ③ 关卡帧 ≥ 90  `[g_GameThread+0x14] >= 0x5a`（`th10:0x418759`）
 *     ⇒ 判据是 **Boss 槽**，不是「场上有没有敌人」（道中一堆妖精在飞时照样满速衰减）。
 *   · **动作**（`th10:0x418a00`，`eax = 0x474c40`；唯一调用点 `th10:0x418764`，在三道门之后）：
 *       槽 `[eax+0x18]` = `0x474c58`（`GLOBALS+0x14` 的 zTimer.current，上限 130）:
 *         · `> 0` ⇒ `zTimer::add(&GLOBALS+0x14, −1.0f)`（`th10:0x418a10`）—— **只 −1，不扣信仰**；
 *         · `<= 0` ⇒ `存储信仰([eax+0xc]) −= 速率([eax+0x10])`（`th10:0x418a26`），
 *                    速率**当场被抬到下限 18**（`th10:0x418a28 cmp edx,0x12` / `th10:0x418a30 mov [eax+0x10],0x12`），
 *                    结果**硬钳存储 5000**（`th10:0x418a1a mov esi,0x1388` / `th10:0x418a37` / `th10:0x418a3c`）。
 *     ⇒ 速率字段只有 2 个写点（击破 `th10:0x40e26b` = 1、开局 `th10:0x417a00` = 9，**都 < 18**
 *       ⇒ 实际恒为下限 18）；一帧 = **−18 存储 = −180 显示** ⇒ 60 fps = **−10800 显示/秒**。
 *   · ★ **槽与衰减在同一个函数、同一组门之后** ⇒ **Boss 战 / 对话中 / 关卡前 90 帧内，连击槽也一起冻结**
 *     （不只是「boss 战不衰减」—— 那一版的判据只有一条门，是**不完整**的）。
 *   · **下限不用另加**：我们的 `faith_slot_total() = base + delta`（`base` ≡ 一手存储 5000 的显示形态
 *     `FAITH_BASE_110` = 50000）⇒ 「`delta` 不越 0」就等价于一手的「钳在 5000 存储」。
 *
 * **量纲**：一手的速率是「显示/帧」；本仓 `delta` 是**显示单位 ×100 定点**（`FAITH_FIXED`）
 * ⇒ 一手 −180 显示/帧 = 我们 `delta` 的 **−18000/帧**。它与 `faith_core.h` 既有的
 * `FAITH_EMPTY_NO_ENEMY_PER_SEC = 10800`（显示/s）**同量纲且恰好同值**（10800 × 100 / 60 = 18000），
 * 所以速率**只有一个出处**：那个宏（`MOF_FAITH_DRAIN_*` 的静态断言把它钉死）。
 *
 * 🟡 **门③「关卡帧 ≥ 90」怎么取** 见 `mof.c` 的 `mof_stage_frames()` —— 一手那个字段是
 *   `g_GameThread+0x14`，TH18 的 `zGameThread` 字段图**未反**（`engine/chapter/th18/01` §1 的 ❓）
 *   ⇒ 本卡用**自己的按帧计数**（`on_tick_2` 每帧 +1，按关卡号/关卡计时回退重置），**不是复刻**。
 *   「怎么钉」：实跑看 `mof:` 那行的 `stage_frames` 是否每关从 0 起、进关后 ~90 帧开始掉信仰。
 *
 * ⚠ **不许改既有签名**（被单测钉着）：新门用**新结构**传，不往旧函数里塞参数。
 * =================================================================================*/
typedef struct {
    int no_boss;            /* ① 一手 `th10:0x418746`：`[ENEMY_MANAGER+0x10] == 0`（卡侧 `!mof_boss_alive()`）*/
    int no_dialogue;        /* ② 一手 `th10:0x418751`：`[GUI+0x9eb8] == 0`（卡侧 `!CE_GUI_MSG_ACTIVE()`）*/
    int stage_frames_ok;    /* ③ 一手 `th10:0x418759`：关卡帧 ≥ 90（卡侧自数帧，🟡 见上）*/
    int enemies_present;    /* ④ 屏幕上有没有敌人。**只作诊断**（日志 `near=`）：一手的速率**不按它**选
                             *   （真判据是 `killed_since_last`，见下）。*/
    int killed_since_last;  /* ⑤ ★★ **这一帧有没有击破敌机**（= 一手的真判据）：一手在敌机死亡路径里
                             *   把速率字段 `0x474c50` 写 **1**（`th10:0x40e26b`，紧跟 `or [enemy+0x1444],0x2000`），
                             *   而衰减帧读它时**用的是读到的原值**（`0x418a26 sub ecx,edx`）、读完才抬到 18
                             *   ⇒ **有击破的那一帧只扣 1 存储（−10 显示），没有击破则扣 18 存储（−180 显示）**。*/
} mof_faith_gate_t;

/* ---- ★★ 衰减速率：**两档，判据 = 这一帧有没有击破**（2026-09-18 逐条复核后的定案）----
 *
 * **一手机制（`th10:0x418a00` 逐条，见下面原注）**：速率字段 `0x474c50` 全 exe 两个写点 ——
 *   `0x417a00` 写 **9**（关卡开始一次）、**`0x40e26b` 写 1（每一次敌机死亡）**；衰减帧
 *   `0x418a23 mov edx,[eax+0x10]` → `0x418a26 sub ecx,edx`（**用读到的原值**）→ `0x418a30` 抬到 18。
 * ⇒ **有击破的那一帧扣 1 存储 = −10 显示；其余帧扣 18 存储 = −180 显示**。
 *   道中一直在击破 ⇒ 每帧都被刷成 1 ⇒ 观感就是「有敌人 ≈ −600/s」；
 *   一波清完没人死 ⇒ 恒 18 ⇒ 「无敌人 ≈ −10800/s」。**用户 2026-09-15 的两档口径数值是对的，
 *   只是机制是「击破」而不是「在场」**（见 `TUNING.md` §2.5j 的两次订正记录）。
 *
 * ⚠ 两次撤回记录（别再走回头路）：
 *   ① 2026-09-15：我们以为「一手只有一档（18）」，把用户口径当 🟡 保留 —— **错**，
 *      错因 = 以为「读后抬到 18」会当场生效（实际 `sub` 用的是读到的旧值）；
 *   ② 2026-09-18：用户说「掉得慢/不够狠」，我们据此**把 600 那档整个撤掉** —— **也错**，
 *      错因同 ①；用户今天的追问「一手是不是只有有敌人分支」把这段逼回逐条复核，才找出 `0x40e26b`。
 *
 * 以下是一手复核（保留原样，供后人核）：
 *
 * **用户口径（2026-09-15，逐字）**：「**连击槽为空时**：若屏幕上无敌人：减 **10800/s**，
 *   若屏幕上有敌人：减 **600/s**」。⇒ 两档都保留，速率由 `gate->enemies_present` 选（下一行的函数）。
 *
 * 🟡 **但一手（th10 v1.00a）在这一点上与用户口径冲突，必须如实登记**（原始字节实测，可复跑）：
 *   `0x474c50`（速率）在整支 exe 里**只有 3 个写点、1 个读点**，且读点前面紧跟一个**抬到下限 18** 的写：
 *     · 写 `th10:0x417a00` = **9**（关卡开始，`c7 05 50 4c 47 00 09 …`）
 *     · 写 `th10:0x40e26b` = **1**（敌机死亡处理路径，`c7 05 50 4c 47 00 01 …`）
 *     · 读 `th10:0x418a23`（`mov edx,[eax+0x10]`）→ `th10:0x418a2e cmp edx,0x12` → `th10:0x418a30
 *       mov [eax+0x10],0x12`（机器码 `c7 40 10 12 00 00 00`）⇒ **读完立刻把字段抬到 18**
 *   ⇒ 字段**单调不减**：首帧过后恒 ≥ 18 ⇒ 有效速率**恒为 18 存储/帧 = −10800 显示/s**，
 *     「9 / 1」两个写入值都**读不出来**（1 那一档尤其：它只有在「首帧就恰好是 1」时才生效，
 *     而那要求先死一只敌人、且这是本关第一次扣信仰 —— 实战几乎不可能）。
 *   ⇒ 一手层面**只有一档**；用户那两档可能是「版本差异 / 另一作 / 观测口径」。
 *   ⚠ 本条**未实跑验证**（我们改的是 TH18 的卡，不是 th10 本体）⇒ 标 🟡，登记在 `TUNING.md`。
 *   「怎么钉」：实跑看 `mof:` 行的 `drain=` 与 `rate=` 两项对账 —— 日志里每 600 帧会打
 *   「本段掉了多少 / 当前档位」（见 `mof.c` 的 `mof_faith_log_tick()`）。*/

/* 一帧的支出（**显示单位 ×100 定点**，与 `faith_slot_t.delta` 同量纲）。两档都由 `/s` 的宏推出 ——
 * **速率只有一个出处**（`faith_core.h` 的那两个宏），下面两条 `_Static_assert` 把「改一处忘另一处」
 * 变成编译期红。无敌人档 = 一手「−18 存储/帧 = −180 显示/帧」；有敌人档 = 用户口径的 −600 显示/s。*/
#define MOF_FAITH_RATE_FIXED(per_sec)   (((per_sec) * FAITH_FIXED) / FAITH_FRAMES_PER_SEC)
#define MOF_FAITH_DRAIN_NO_BOSS_FIXED   MOF_FAITH_RATE_FIXED(FAITH_EMPTY_NO_ENEMY_PER_SEC)  /* = 18000 */
#define MOF_FAITH_DRAIN_KILL_FIXED      MOF_FAITH_RATE_FIXED(FAITH_EMPTY_ENEMY_PER_SEC)     /* = 1000（−10 显示/帧）*/
_Static_assert(MOF_FAITH_DRAIN_NO_BOSS_FIXED == 18000,
               "一手 th10：没击破的帧扣 18 存储/帧 = −180 显示/帧 = 18000 定点");
_Static_assert(MOF_FAITH_DRAIN_KILL_FIXED == 1000,
               "一手 th10：**有击破的那一帧**只扣 1 存储/帧 = −10 显示/帧 = 1000 定点"
               "（`0x40e26b` 写 1、`0x418a26` 用读到的原值；数值上 = 用户口径的 −600 显示/s）");

/* 本帧该用哪一档速率（**纯函数，可单测**）。
 * `killed_since_last` = 这一帧有没有击破敌机（1/0）：
 *   1 ⇒ 慢档 `MOF_FAITH_DRAIN_KILL_FIXED`（−10 显示/帧）；0 ⇒ 快档 `MOF_FAITH_DRAIN_NO_BOSS_FIXED`（−180 显示/帧）。
 * 其它取值一律按「有击破」处理（保守：宁可少扣）。*/
int32_t mof_faith_rate_fixed(int killed_since_last);

/* 每帧推进（**连击槽与衰减只用这一条路**）：
 *   · 「连击槽为空」= `s->combo <= 0` 是本函数里的**分叉**（一手 `th10:0x418a06 jle`）：槽 > 0 只 −1、
 *     槽 ≤ 0 才扣信仰 —— **用户口径的「连击槽为空时」就是这条**（不是门）；
 *   · 三条门（无 Boss / 无对话 / 关卡帧 ≥ 90）**全部**成立才推进一步；任一门不成立
 *     ⇒ **槽与衰减一起冻结**（一手：两者在同一函数、同一组门之后）；
 *   · 速率为**一手单档** −180 显示/帧（★ 2026-09-18：原来的「有敌人 −600/s」两档已撤）。
 * `frames` 一般传 1（`on_tick_2` 每帧一次）；批量补跑时传 N —— 速率是常量，与逐帧等价。
 * 返回本帧真正扣掉的量（**定点**；被下限夹掉的、或门不成立时的 0 都如实反映）。
 * ⚠ 卡侧一律用本函数；`mof_core_combo_tick()`（旧签名）只是门 =「无 Boss」的**兼容包装**，
 *   它的门**不完备**，除既有调用点外不要再用。*/
int32_t mof_core_faith_tick(faith_slot_t *s, const mof_faith_gate_t *gate, int frames);

/* ---- 旧入口（2026-09-15 退役为薄包装）----
 * 它是「§2026-09-12 一手订正：TH10 无逐帧信仰衰减」的产物：**恒返回 0、只推进连击槽**。
 * 那条订正已被推翻（见上面那一大段），所以本函数**不再**恒 0：它把参数映射成门
 * （`no_boss = !in_boss_fight`，另两条门**按最宽松处理**）后转调 `mof_core_faith_tick()`。
 * ⚠ **保留它只为「不改既有签名」这条工程规矩**（`tests/test_mof_core.c` 与旧调用点钉着它）；
 *   ⚠ 它**不是**完整复刻：缺少「无对话 / 关卡帧 ≥ 90」两条门 ⇒ 真实卡侧**不许**再走这条路。*/
int32_t mof_core_combo_tick(faith_slot_t *s, int in_boss_fight);

/* ---- 收集事件档与连击帧数（§7.5 的表 + 2026-09-12 一手订正）★ 任务 2 ----
 * 一颗**自建点**被收走时走哪一档 / 加多少连击帧。表（一手 th10 的充能 push 立即数）：
 *   大信仰点（type 3）      +120f（`0x41b5c5 push 0x78`）  · 信仰 +5000/8000/10000
 *   蓝点（点道具 type 2）   +100f（`0x41b56e push 0x64`）  · ——（蓝点通道还没落地，§2.4）
 *   小信仰点（type 9）       +60f（`0x41b61d push 0x3c`）  · 信仰 +100
 *   小 P（type 1/10）        +60f（`0x41b46c push 0x3c`）  · ——
 *   大 P（type 4/11）        +20f（`0x41b6bd/0x41b6ef`）   · ——
 *   微信仰点（type 8）        +3f（`0x41b5e2 push 3`）     · 信仰 +10
 *   1UP（type 7）             +0f
 * ★ 为什么 110 自己带一张表而不用 `faith_core.h` 的 `FAITH_COMBO_*`：那张表是**共用默认**
 *   （111 的 `signal_core_pickup()` 也读它），而一手订正只改了 110 的大信仰点（130 → 120）
 *   ⇒ 改共用件会**同时改掉 111**（本轮不允许）⇒ 110 用自己这一份，`faith_core` 那边留原样。
 * ⚠ 与一手的差异（**只在报告里，不在本轮改**）：`FAITH_COMBO_BIG = 130` 现在只被 111 的
 *   大绿点通道用；一手 th10 里没有 130 这个充能值（130 是**上限**，`0x413035`）。 */
#define MOF_COMBO_BIG          120   /* 大信仰点 +120f（一手 `0x41b5c5 push 0x78`；spec 旧值 130 是错的）*/
#define MOF_COMBO_SMALL_FAITH   60   /* 小信仰点 +60f（一手 `0x41b61d push 0x3c`；= §7.5 的「小 P / 小信仰点」）*/
int32_t mof_core_pickup_combo(int tier);      /* tier = `faith_tier_t`：BIG 120 / SMALL 60 */

/* ---- ★★ 2026-09-16 补：**引擎侧道具与击杀**的连击充能（这条通道原来一条都没接）----------
 * 症状（用户 2026-09-16 实跑逐字）：「**连击值一点也不涨，击破妖精是涨连击值的啊**」。
 * 根因：一手 th10 的连击槽共有 **9 个充能调用点**（`engine/player/th10/02-hud-bottom-left.md` §2.2
 * 的立即数表），我们原来只接了其中两条 —— 「**自建点**被收走」（`mof_on_pickup`）、
 * 「消弹微档」（`mof_on_cancel`）—— **引擎自己的道具与敌人侧那几条一条都没接**
 * ⇒ 吃 P 点 / 吃钱（TH18 的蓝点生态位）/ 吃大 P / 击破小怪 全都不涨连击。
 *
 * 档位值（全部 = 一手 `th10:0x412ff0` 调用点前的 `push` 立即数）：
 *   TH18 **1（小 P）**  ← th10 type 1/10 ⇒ **+60f**（`0x41b46c push 0x3c`）
 *   TH18 **2（金钱）**  ← th10 type 2（蓝点）⇒ **+100f**（`0x41b56e push 0x64`）
 *                        ★ 对应关系：`AUDIT.md` §Z19「TH14 官方『红点与蓝点』→ **TH18 无蓝点生态位**」
 *                        ⇒ 18 的金钱就是「得点道具」那一路（批集 = {P, 钱}），拿它当蓝点档。
 *   TH18 **3（大 P）**  ← th10 type 4/11 ⇒ **+20f**（`0x41b6bd`/`0x41b6ef`）
 *   其余 type ⇒ **0f**：4 命碎 / 5 整命（一手 1UP = +0f）/ 6 符卡碎 / 8 补满火力（TH10 无此档）/
 *             9–0xe 分数转换点族（引擎强制收的产物，不是玩家收的道具）/ 0x10+ 卡道具。
 * 击杀：**非 boss** 一只 ⇒ **+10f**（一手 `th10:0x40e681 push 0xa`，坐标在敌人被移除/撒落那一段里，
 *   无条件给一次）。⚠ 用户 2026-09-15 的口径写的是「**大p、击杀非boss单位 +20f**」——一手是 **10**；
 *   而用户同表里的「大信仰点 +130f」= **拾取 120 + 击杀 10** 正好对上 10（`AUDIT.md` §BC3）
 *   ⇒ 取一手值，且**击杀 boss = +0f**（用户口径与一手一致）。
 *   要改成 20：只改 `MOF_COMBO_KILL` 一个常量（`tests/test_mof_core.c` 的钉子会一起红）。*/
#define MOF_COMBO_BLUE        100   /* 蓝点（TH18 = 金钱 type 2），一手 `th10:0x41b56e push 0x64` */
#define MOF_COMBO_SMALL_P      60   /* 小 P，一手 `th10:0x41b46c push 0x3c` */
#define MOF_COMBO_BIG_P        20   /* 大 P，一手 `th10:0x41b6bd`/`0x41b6ef push 0x14` */
#define MOF_COMBO_KILL         10   /* 击杀非 boss，一手 `th10:0x40e681 push 0xa` */
#define MOF_ITEM_TYPE18_MONEY   2   /* TH18 item type 2 = 金钱（= 蓝点生态位）*/
#define MOF_ITEM_TYPE18_BIG_P   3   /* TH18 item type 3 = 大 P（`mof_core.h` 的 P 类注释）*/
int32_t mof_core_item_combo(int32_t type18);   /* 引擎道具 → 连击帧数（0 = 这一档不充能）*/

/* 同一颗点该走哪个**信仰事件档**：BIG → `FAITH_EV_BIG`、其余实体档 → `FAITH_EV_SMALL_FAITH`。
 * ★ 任务 2 的修：原来两条通道**都**走 `FAITH_EV_GREEN`（+100f）——小信仰点因此多了 40f
 *   （§7.5 的表：小信仰点 +60f）。信仰**加值**由调用方给的 `value` 决定，事件只决定「哪一档」。
 * ⚠ 110 的**蓝点通道**（§2.4，落地后也该进这个池）**不能复用本映射** —— 蓝点走 `FAITH_EV_GREEN`
 *   （+100f）。实体层（`faith.h` 的 `faith_pickup_fn`）只带 `tier`、不带「这颗是什么」⇒
 *   那是「蓝点通道」落地时必须一并解决的事（报告「发现但不在本次范围」）。
 * `tier` 用数值而**不 include `faith.h`**（那是引擎侧头，会把 `CE_*` 地址拖进纯逻辑层）：
 * 0 = `FAITH_TIER_BIG`、1 = `FAITH_TIER_SMALL`（2 = MICRO 不建实体）。*/
faith_ev_t mof_core_pickup_ev(int tier);

/* ---- 符卡「全避」监视器（§2.2 / §7.5）★ 2026-09-12 审计加 ----
 * 「全避」= 这一张**拖到超时**（没打掉、时间到了）且本卡**没在这一张里放过灵击** ⇒ −30000。
 * 本来是写在卡侧 tick 里的四条语句，但**卡侧代码主机编不出来**（`sdk.h` 的 inline asm）⇒ 一条都测不到；
 * 而它恰恰是这次审计改掉的判据（旧实现「符卡结束且没放灵击」就扣 ⇒ **每张正常收掉的符卡也扣 30000**）。
 * ⇒ 把状态机搬到这里，卡侧只喂两个每帧读到的量：
 *   `spell_now`      = 本帧符卡进行中（`ce_spell_active()`）
 *   `timed_out_now`  = 本帧 flags 上**已经**有超时位（`ce_spell_flags() & CE_SPELL_FLAG_TIMED_OUT`；
 *                      引擎在 `0x42efc0` 置这一位，**耐久符卡不置** ⇒ 自动满足「时符必减 / 耐久按收符卡算」）
 * 返回 1 = **本帧该扣这一笔**（只在 active 的下降沿返回一次；扣完调用方自己清零连击槽等副作用）。
 * ⚠ 超时位必须**锁存**：`on_tick_2` 是每帧轮询，超时位与 active 位可能同帧翻转。读取的闸门是
 *   `w->was`（上一帧已经在符卡里）⇒ 「新符卡开始那一帧」不会把上一张的残留位读进来。*/
typedef struct {
    int32_t was;        /* 上一帧符卡进行中（调用方不用自己维护）*/
    int32_t bombed;     /* 本张符卡期间放过灵击 */
    int32_t timed_out;  /* 本张符卡期间读到过超时位 */
} mof_spell_watch_t;

int mof_core_spell_watch(mof_spell_watch_t *w, int spell_now, int timed_out_now);

/* ---- PV 公式（§7.4 / §7.8）----
 *   只装 110：`PV = 信仰_110`（MoF 里信仰**就是**最大得点，**无乘数**）
 *   只装 111：`PV = 信仰_111 × (擦弹倍率 + 信号强度)`
 *   两个都装：`PV = ( 基础均值 + Δ_110 + Δ_111 ) × (擦弹倍率 + 信号强度 )`
 *   ⇒ 这里只认「谁在场」，不读任何卡的状态（EVENTS-CONTRACT §3 的「不跨卡耦合」）。
 *
 * ⚠★ **2026-09-13 起本函数不再参与落笔**（计分层重做：各卡声明**自己那一族的加数**、
 * SDK 一次合成 `G = G_0 + Σ未耦合 + coupling × Σ耦合`，见 `../../SCORE-LAYER-REWORK.md`）。
 * 它保留为**旧口径的回归断言**（`tests/test_mof_core.c` / `tests/test_signal_core.c` 仍在测它）；
 * 新代码里 110 只声明「Δ_10」、基础由层算 `G_0` ⇒ **不要再拿它算分**。
 * `have110` / `have111` = 声明者的**存在性**；`aggregate` = `ce_score_aggregate()`
 * （旧口径下 = Σ 各卡的「基础 + Δ」）。*/
int32_t mof_core_pv(int have110, int have111, int32_t aggregate, int32_t graze_mult_permille,
                    int32_t signal_value);

/* 擦弹倍率（千分点定点）：★ 2026-09-15 按 **th11 一手**改成**整数阶梯**
 *   `B = min(⌊graze/100⌋, 899)`（单位 0.01 ⇒ 0.00..8.99；每满 100 擦弹 +0.01）⇒ 返回 `B × 10` 千分点。
 * ⚠ 旧注释写的「1 graze = 1 千分点、上限 8990 千分点」**是错的**（那是把浮点写法当成实现）：
 *   一手（`local/_scratch/research-th11-faith-and-points.md` §2.3）里 exe **没有** `0.0001f`/`8.99f`，
 *   真身是 `⌊graze/100⌋`（`th11:0x41f4e0` 的 `0x51eb851f` + `sar edx,5`）与整数 `0x383`(=899)
 *   （`th11:0x41f4e7` / `th11:0x41f4fe`）。⇒ 上限在 **89900 擦弹**才到，旧实现 8990 就封顶（提前 10 倍）。
 *
 * ★★ 归属订正（2026-09-12 一手，任务 ③）：**这是 th11（地灵殿 / SA）的东西，TH10 没有**。
 *   · 一手否定证据（`local/_scratch/research-th10-faith-and-graze.md` §1.1，🟡→✅）：
 *     th10 全 exe 的 `float 0.0001f` 只出现一次（`0x470c58`）且 4 个引用**全是 `fcomp`**
 *     （`fpatan` 的 epsilon，`0x4075f1` / `0x407606` / `0x41d337` / `0x41d349`）；
 *     全 exe **没有 `8.99f`**；13 个计分口（`add_to_score` 的全部调用点）**没有一处读擦弹**；
 *     `zGlobals` / `zPlayer` 两个结构里**都没有 graze 字段**。
 *   · ⇒ 「蓝点分 = 信仰 × (擦弹倍率 + 信号强度)」是 **SA 的式子**（它在 review §7.5 末与 §7.7 的
 *     **th11 行**、`IDEAS.md` 的 §111 小节；`2026-09-10-mof-faith-design.md` 全文没有这句）。
 *     TH10 的蓝点（线上）= **信仰本身**，无乘数（`0x41b4bd`→`0x41b569`）。
 *   · ⇒ **110 单装时不许用本函数**：`mof_core_pv()` 的「只装 110」那一支返回 `aggregate`，
 *     整项乘数都不参与（`mof.c` 的 `on_item_score()` 也只在**两卡同装**时把它交给
 *     `mof_core_pv()` 的乘数项 —— 那时它是 111 声明进来的那一半）。
 *   · 111 仍在用它（`signal.c` / `signal_core.*` 的 `(擦弹倍率 + 信号强度)`）⇒ **数值行为不动**。
 *   · 🟡 量纲本身也**未一手核实**：th11.exe 里同样扫不到 `8.99f` 与 `0.0001f`
 *     （research §6「未取到」第 2 条）⇒ 这条式子仍是转抄。
 *
 * ★★ **2026-09-15 追加的一手否定（`research-th10-bomb-boss-residuals.md` §6）**：TH10 v1.00a
 *   **连擦弹计数器都不存在** —— 擦弹判定 `th10:0x4266b0` 返回 2、擦弹火花 = `bullet.anm` script434
 *   （`th10:0x4064f7 push 0x1b2`）、音效 = `se_graze.wav`（id `0x1c`，`th10:0x406514`），
 *   而事件体（`th10:0x4064d9`–`0x406517`）**只置子弹自己的去重位**（`[bullet] |= 4`），
 *   全区间 `inc/dec/add/sub [mem]` 扫描**零命中**、`zGlobals`/`zPlayer` 无 graze 字段、
 *   replay 每帧块也没有该字段（9 条穷举见该文 §6.1(e)）
 *   ⇒ 「擦弹倍率」在 TH10 侧是**彻底不存在**的概念（连输入量都没有），本函数只是 111 那一半。*/
int32_t mof_core_graze_mult(int32_t graze);

/* ============================ 灵击的收尾（★ 2026-09-15 按 TH10 一手重写）============================
 *
 * **一手总依据**：`engine/player/th10/01-bomb-full-chain.md`（本轮定稿的一手文档）+ 它的两份底稿
 *   `local/_scratch/research-th10-bomb-boss-residuals.md` / `research-th10-faith-and-graze.md`。
 *   下面每一条都给了 `th10:0x…` 或 `th18:0x…` 的出处。
 *
 * ## 三件被**推翻**的旧说法（旧实现在这三处是错的）
 *   ① **「灵击不伤敌人」是错的**：`Bomb` 自己不调伤害，而是暴露一个伤害加成查询 `th10:0x4059f0`，
 *      由玩家→敌人伤害例程 `th10:0x428630` **主动回调**（`th10:0x428891`），返回值加进伤害累加器
 *      （`th10:0x42889a`），最后落到 `th10:0x40e1b6`（TH10 敌人 HP 的唯一扣减点）。
 *      ⇒ 语义 = **每敌每帧一笔额外伤害**（按「圈内/圈外 × 符卡/无符卡 × 有 boss/无 boss」分档），
 *      不是「独立的伤害源」也不是「自机弹的倍率」。**TH18 侧没有等价物** ⇒ 本卡用
 *      `ce_damage_rect`（引擎**独立伤害源**池）近似，逐条差异见 `mof.c` 的 `mof_bomb_tick()` 与
 *      `TUNING.md` §2.5（**❓ 自定，不许写成复刻**）。
 *   ② **没有震屏**：灵击可达闭包 186 个函数内**没有任何** `ScreenEffect` 创建（`th10:0x43c8b0` 不可达），
 *      而且 TH10 的 `ScreenEffect` 结构上做不出震屏（固定矩形 alpha 覆盖层、无平移字段）
 *      ⇒ `ce_screen_shake` 已从本卡删掉。
 *   ③ **消弹半径不是全屏**：`bomb+0x3c` 每帧被灵击 anm VM 的 `scale.y` 覆盖（`th10:0x405813`），
 *      曲线 = `bullet.anm` script400/407/441 的 `scale(16,16)` + `scaleTime(120, 6, 16.0 → 160/112/64)`
 *      ⇒ **16.0 起、120 帧线性长到目标、其后保持**。旧值 `4000.0f`（全屏）**一手无此物**，已删。
 *
 * ## 半径曲线（一手：`th10:0x405813` + `bullet.anm` 原始字节）
 *   帧 0 → 120：`r = 16.0 + (target − 16.0) × 帧/120`（线性，**整数化到 1/100 单位**：每帧 +1.20
 *   灵梦档 / +0.80 魔理沙档 / +0.40 符卡档），帧 ≥ 120 保持 target。
 *   ⚠ 目标值由 `Bomb::start` `th10:0x4058d9`–`0x405917` 选脚本决定：
 *     灵梦（`GLOBALS.CHARACTER 0x474c68 == 0`）script `0x190` → 160.0；魔理沙 script `0x197` → 112.0；
 *     符卡中（`[spellcard+0x378c] bit0` 且 `[spellcard+0x3788] ∈ 0x5d..0x60 ∪ {0x6d}`）script `0x1b9` → 64.0。 */
#define MOF_BOMB_R_START_CENTI    1600     /* 16.00（script 的 `scale(16,16)`）*/
#define MOF_BOMB_R_GROW_FRAMES     120     /* `scaleTime(120, 6, …)` 的时长 */
#define MOF_BOMB_R_REIMU_CENTI   16000     /* 灵梦 160.0（script400）*/
#define MOF_BOMB_R_MARISA_CENTI  11200     /* 魔理沙 112.0（script407）*/
#define MOF_BOMB_R_SPELL_CENTI    6400     /* 符卡中 64.0（script441）*/
#define MOF_BOMB_CENTI_ONE         100     /* 1/100 世界像素（曲线用定点，避免 core 里跑浮点）*/

/* 消弹/伤害中心每帧**上移**（一手 `th10:0x405793`–`th10:0x4057de`，三支互斥）：
 *   符卡中（同一条件）−0.5；否则 `CHARACTER == 0`（灵梦）−1.0、`!= 0`（魔理沙）−1.3。
 * 单位 = 1/1000 世界像素（整数化）。 */
#define MOF_BOMB_RISE_SPELL_MILLI   500    /* 0.5（常量 `th10:0x470b0c`）*/
#define MOF_BOMB_RISE_REIMU_MILLI  1000    /* 1.0（`th10:0x470afc`）*/
#define MOF_BOMB_RISE_MARISA_MILLI 1300    /* 1.3（`th10:0x470d34` = 1.29999995f）*/

/* 灵击的**持续帧数**：🟡 = 灵击 anm 脚本（`bullet.anm` script400）的 `time` 字段累加 = **296 帧**；
 * 脚本尾部还有 `ins_6($I0,80)` + `ins_5($I0,offset,30)` 组成的 80×30 帧计数循环，真实寿命可能更长
 * ⇒ 这条**只有 🟡**（半径在 120 帧就已顶满，所以曲线本身不受影响）。登记 `TUNING.md` §2.5。 */
#define MOF_BOMB_FRAMES            296

/* `Bomb::start` 的「大灵击」标志 `bomb+0x44`（一手 `th10:0x4059bb`–`0x4059e7`）：
 *   = 1 iff（符卡进行中）且（`[spellcard+0x3738]` = 本次攻击已过帧数 **≥ 60**）。
 * 它同时决定三件事：内圈伤害 3 vs 38（`th10:0x405a39`/`0x405a46`）、圈外是否有伤害（`th10:0x405a76`）、
 * 以及 `Bomb::start` 是否**作废符卡奖励**（`th10:0x405500` 的同一道 60 帧门）⇒ 三条共用这一个判据。*/
#define MOF_BOMB_BIG_BOMB_FRAMES    60

/* 灵击伤害表（**原始值**，一手 `th10:0x4059f0` 全文 + `engine/player/th10/01-bomb-full-chain.md` §2.2）
 * 与它的缩放链（一手 `th10:0x40e141`–`th10:0x40e19e`）：
 *     raw --(player_state ∈ {0,2} ⇒ /5)--> 0 则**整笔跳过** --(符卡中 且 该敌是 boss ⇒ 再 /5，下限 1)--> HP
 * 游玩中 `player_state` 恒 0（决死灵击把它置 1，`th10:0x425c84`）⇒ 我们一律走 /5。 */
#define MOF_BOMB_DMG_IN_SPELL_NOBIG       3   /* 圈内 + 符卡中 + `bomb+0x44==0`（符卡前 60 帧）*/
#define MOF_BOMB_DMG_IN_SPELL_BIG        38   /* 圈内 + 符卡中 + `bomb+0x44==1`（0x26）*/
#define MOF_BOMB_DMG_IN_BOSS             38   /* 圈内 + 无符卡 + 有 boss（0x26）*/
#define MOF_BOMB_DMG_IN_PLAIN             5   /* 圈内 + 无符卡 + 无 boss */
#define MOF_BOMB_DMG_OUT_SPELL_BIG_REIMU 26   /* 圈外 + 符卡中 + `bomb+0x44==1`：灵梦 0x1a */
#define MOF_BOMB_DMG_OUT_SPELL_BIG_MARISA 23  /*                                    魔理沙 0x17 */
#define MOF_BOMB_DMG_OUT_BOSS_REIMU      20   /* 圈外 + 无符卡 + 有 boss：灵梦 0x14 */
#define MOF_BOMB_DMG_OUT_BOSS_MARISA     16   /*                          魔理沙 0x10 */
#define MOF_BOMB_DMG_NONE                 0   /* 其余一律 0（圈外且既没 boss 也没符卡）*/
#define MOF_BOMB_DMG_DIV                  5   /* `th10:0x40e157`–`0x40e165`（魔数 0x66666667 + sar 1）*/

/* 圈的存在判据 = **有 boss 活着**（一手 `th10:0x405a58`/`0x405a95`：`[ENEMY_MANAGER+0x10] != 0`），
 * 与「这只敌是 boss」(`enemy+0x1444` bit `0x8000`) 是**两条**判据 —— 卡侧分别由 `mof_boss_alive()` /
 * `mof_is_boss()` 实现（TH18 的对应物见 `mof.c` 的注释）。 */

/* 本卡**自定**的伤害上限抬升值（`ce_raise_damage_cap`）：❓ 不是 TH10 的量。
 * 理由（TH18 侧）：`enm_compute_damage_sources` `th18:0x45f0f0` 把「每敌每帧」的总伤害钳到
 * `player+0x47984`，而它每帧被 `GameThread` 从 `sht+0x28`（`max_dmg`：灵梦 90 / 魔理沙 160 /
 * 咲夜 60 / 早苗 120）复位 ⇒ 自机弹与卡的伤害源**共用配额**。TH10 的灵击伤害是**加进累加器**的
 * （不与子弹抢配额）⇒ 这里照先例（Remilia `th18:0x40f48e` 写 300、本卡旧实现同值）抬到 300，
 * 让本帧的源不被自机弹的配额吃掉。⚠ 副作用：抬升的当帧**自机弹**也能多用配额（只抬不降、
 * GameThread 每帧复位）—— 与所有用 `ce_damage_rect` 的卡同款，登记在 `TUNING.md` §2.5。 */
#define MOF_BOMB_DMG_CAP           300
/* 半径消弹的颗数上限（不限）：一手 TH10 的 `cancel_circle` 也没有上限（2000 槽全扫）。*/
#define MOF_BOMB_CANCEL_MAX      99999

/* 半径曲线：`frame` = 灵击已过帧数（0 起），`target_centi` = 目标半径（1/100 px）。
 * 返回本帧半径（1/100 px）。线性、只增不减、帧 ≥ 120 保持目标值（一手曲线）。*/
int32_t mof_core_bomb_radius_centi(int32_t frame, int32_t target_centi);

/* 灵击消耗的**半档**（111 在场 + 魔理沙机体的额外特技）：50 单位 = 0.5 档。*/
#define MOF_BOMB_POWER_COST_HALF   (MOF_BOMB_POWER_COST / 2)
/* 半档那一支的**门槛** = 1.50 档（owner 2026-09-24：「111 魔理沙换成 power >= 1.50，下限不变
 *   —— 相当于最后一次灵击只花 0.45」）。下限仍是 `MOF_BOMB_POWER_FLOOR`（105）⇒ 150 那一发实扣 45。*/
#define MOF_BOMB_POWER_HALF_GATE   (150)

/* ★★ 灵击的**消耗规则**（110/111 **共用同一套系统**，见 `mof_core.c` 的实现注）：
 *   `alice_on` = 「111 在场 + 魔理沙机体」⇒ 半档（门槛 1.55 档 / 扣 50 单位 / 下限 1.05 档）；
 *   否则 = 常态（门槛 2.00 档 / 扣 1 档 / 下限 1.05 档）。*/
mof_power_t mof_core_bomb_power_spend(int32_t power, int alice_on);

/* 消弹/伤害中心每帧上移量（1/1000 px）。`spell_ish` = **小灵击档**（= `mof_core_bomb_suppressed()`）、
 * `reimu` = `CHARACTER == 0`。*/
int32_t mof_core_bomb_rise_milli(int spell_ish, int reimu);

/* ★★★ **小灵击档**：一手 TH10 里 `Bomb::start` 选主脚本的第三条分支不是「符卡中」，而是
 *   **特定几张符卡**（`[spellcard+0x3788] ∈ 0x5d..0x60 ∪ {0x6d}`，半径 64）——
 *   那正是风神录 6 面终符（灵击被神奈子压制）。owner 2026-09-24 口径（逐字）：
 *   「风神录 6 面终符，灵击会被神妈压制所以开一个小的，这个一般认为是**剧情和平衡意义**，
 *     **不能把这个当常态**」。
 *   ⇒ 本函数只在 `spell_id` 命中**白名单**时返回 1；白名单默认**空**（= TH18 侧不启用这条特例），
 *     要按 TH18 自己的某张符卡复刻，把那张符卡的 id 填进 `k_suppress_ids` 即可。
 *   ⚠ 这与「符卡中」**不是同一个判据**：伤害表 / `bomb+0x44` 走的是**通用**的
 *     `ce_spell_active()`（一手 `flags & 1`），那些地方**不要**换成这个谓词。*/
int mof_core_bomb_suppressed(int32_t spell_id);

/* 本帧**每敌**的伤害（已过 TH10 的 `/5`（+符卡 `/5`）缩放链；0 = 这一笔不存在）。
 *   `in_circle`   = 该敌在消弹圈内（圈外走 `*_OUT_*` 那几档）
 *   `boss_alive`  = 场上有 boss（= TH10 的 `[ENEMY_MANAGER+0x10] != 0`）
 *   `spell_active`= 符卡进行中（= `[spellcard+0x378c] bit0`）
 *   `big_bomb`    = 本题的 `bomb+0x44`（见 `MOF_BOMB_BIG_BOMB_FRAMES`）
 *   `reimu`       = `CHARACTER == 0`（圈外那一档分灵梦/魔理沙） */
int32_t mof_core_bomb_damage(int in_circle, int boss_alive, int spell_active, int big_bomb, int reimu);

/* ============ ★ 2026-09-15「灵击 → 决死」（**决死 B 形态**）：形态状态 + 两条代价口径 ============
 * 用户口径（2026-09-15 逐字）：「**基本语义上灵击==B**」「**所以灵击也得决死**」
 *   ⇒ 本卡的灵击（库存 == 0 时自干的那一支）在**决死窗口内按 B** 时，必须走「救命」那条路：
 *     照常发动（扣 1 档火力 / 消弹 / 伤害源 / 节奏不变）+ **调 `Player__cancel_impending_death`**。
 *
 * ## 一手（TH18 侧）：决死窗口 = `player+0x476ac == 4`；救命 = `th18:0x45d590`
 *   · `0x45d4e4: movl $0x4,0x476ac(%edi)`（`Player__die` `th18:0x45d3a0` 体内，`edi` = this = zPlayer）。
 *     本轮**自己在样本上复核**：对 `+0x476ac` 的 disp32 穷举（`th18.exe`）共 19 处，
 *     其中**写**点 7 处 —— `0x40da64`/`0x45abbd`/`0x45bff3`/`0x45d5cb` 写 1、`0x45c302` 写 0、
 *     `0x45d21e` 写 2、**只有 `0x45d4e4` 写 4** ⇒ 「状态 4 = 决死窗口」这条唯一。
 *   · `0x45d590`（`Player__cancel_impending_death`）：thiscall(**Player***)、**无栈参**、结尾裸 `ret`。
 *     体内只写 6 个字段：`+0x644 |= 1` / `+0x640 = 0` / `+0x638 = 0x3c` / `+0x63c = 60.0f` /
 *     `+0x634 = 0x3b` / **`+0x476ac = 1`**（回到存活）。
 *     先例（形状可照抄）：`cards/senro.c`（107）、`cards/trance.c`（113）、`cards/animal.c`（117）。
 *
 * ## 两条代价口径（**一手 TH10**，本轮逐条复核）
 *   ① **决死不扣信仰 −3000**（✅ 一手）：TH10 的 `Player::on_tick`（`th10:0x425730`–`0x425d80`）
 *      里 `call 0x412e70`（信仰减益助手）**只有一处** = `th10:0x425a1f`（`push 0xbb8` 在 `0x425a15`），
 *      位置在 **state 1（存活）** 那一支；**决死那一支（state 4 → `th10:0x425bd9`–`0x425c8e`）
 *      通篇没有它** ⇒ 与既有注释「**非决死**灵击 −3000」互为印证。⇒ 决死形态扣 `FAITH_REDUCE_NONE`。
 *   ② **消弹入不入账**（🟡 原引文**被本轮复核推翻**，故降级为 ❓ 自定 + 🟡）：
 *      原文引 `th10:0x4258d4` / `th10:0x4258f0` 为「决死清弹调用点」、两处第二刀都压 `0x0`
 *      ⇒「决死不撒道具 ⇒ 不记账」。本轮自己读 `th10.exe` 的结果分三段：
 *        (a) `th10:0x408100` 的三个栈参（`ret 0xc`）依次是 `[esp+0x28]` 半径 / **`[esp+0x2c]` 撒不撒** /
 *            `[esp+0x30]` 条件消；「撒」的判据在 `th10:0x4081cc`–`th10:0x4081e7`（≠ 0 ⇒ `call 0x41bb00`
 *            撒 **type 8** 微信仰点，位置 = 弹自己的位置）。两处 `0x4258d4` / `0x4258f0` 的
 *            第二刀**确实都压 `0x0`** ✔（引文这半是对的）。
 *        (b) ⚠ **但这两处不在决死分支里**：`Player::on_tick` 的状态跳表 `th10:0x426344` =
 *            `[0]=0x425751` / `[1]=0x425964` / `[2]=0x425aa9` / `[3]=0x425d25` / **`[4]=0x425a96`（决死）**；
 *            而 `0x425839` → `0x425846 jl 0x4258ab` → 那两处调用**落在 index 0 那一支**，
 *            该支末尾是 `0x425902: cmp [player+0x478],0x3c` → `mov [player+0x458],1`
 *            ⇒ **state 0 = 复活上升**（60 帧；前 30 帧扩圈消弹、之后删残留弹）= 它们是**复活清弹**、
 *            不是决死。真正的决死分支 `0x425bd9` **自己一句消弹都没有**，只调 `Bomb::start`
 *            （`th10:0x425c3e`）⇒ 决死的消弹由 `Bomb::on_tick` → `th10:0x405ac0` 做，
 *            那里第二刀压的是 **`(~[player+0x378c]) & 1`**（`th10:0x405af1`–`0x405b18`）
 *            = **与普通灵击同一句**（符卡中不撒、否则撒）⇒ **「决死不撒道具」不成立**。
 *        (c) ⇒ 「决死形态不记这笔账」**没有一手依据**（一手反而说与普通灵击一致）。本卡仍然按
 *            **用户口径/任务口径**取 0（不记账），但**标 ❓ 自定**（不是复刻、也不是「一手待查」），
 *            理由只有一条设计上的：决死形态已经免掉 −3000；若再照常按颗数入账，它就**严格优于**
 *            主动灵击（用户口径「不许超模」）。翻 `MOF_BOMB_DEATHBOMB_ACCOUNT` 即可回到
 *            「与普通灵击一致」。登记 `TUNING.md`。
 *            ⚠ 本仓纪律：**不许把这一条写成复刻**（写成"复刻"却其实是自定 = 明令禁止）。
 *        (d) ⚠⚠ **2026-09-15 同步（另一 agent 的 TH11 一手 + 本轮的 TH10 复核）**：TH11 那一侧
 *            **已被一手钉死 = 决死与普通灵击代价完全相同、两者都撒 type 8**（消弹 `th11:0x40af90` /
 *            撒点 `th11:0x40b0ac`；见 `signal_core.h` 的地址链）⇒ 111 取 **1（照常记账）**。
 *            而这与我上面的 TH10 复核**同向** ⇒ **「两作在这一条上相反」这个说法不成立**：
 *            两边的一手都说「照常」。110 之所以仍是 0，纯粹是**任务/用户口径的保守取值**
 *            （上面 (c) 的设计理由），**不是**「TH10 原作不撒」。⇒ 谁要改成与一手对齐，
 *            就翻 `MOF_BOMB_DEATHBOMB_ACCOUNT` 这一行（111 侧不用动）。
 *
 * ## 故意**不做**的事（结构性，别加）
 *   · **不挂 `on_death_before_deathbomb`（卡虚表 `+0x10`）自动救人** —— 按 `EVENTS-CONTRACT.md`
 *     §4.2/§5.2 的救援层顺序（**107 → 117 → 决死 B → 113**），本卡扮演的是「**决死 B**」，
 *     语义 = **玩家在窗口内按 B**；107/117 才是「中弹即自动爆」。只做 B 键这条路。
 *   · 不 `ce_rescue_note_claim()`：那只服务 113（它挂在 `+0x0c`）。我们这条走完之后窗口不会耗尽，
 *     引擎**不会**广播 `+0x0c`（111 的 miss 记账钩子因此天然不触发 —— 救下来就不该按 miss 扣）。
 *
 * ## 为什么这些在 core
 *   本仓硬要求「状态进 core 才可测」；`by_hit` 的先例 = `cards/animal_core.h` 的 `int32_t by_hit;`。
 *   卡侧只做「**检测窗口 → 传参**」，两条代价口径由下面这几个纯函数回答
 *   （`tests/test_mof_core.c` 在测）。⚠ 不新增/不改任何 `faith_*` 的签名。*/
#define MOF_BOMB_BY_PRESS   0    /* 按 B 主动发动（窗口外；或库存 > 0 ⇒ 交给原生 `do_bomb`，本卡不动）*/
#define MOF_BOMB_BY_HIT     1    /* **决死 B**：决死窗口内按 B 且库存 == 0 ⇒ 本卡自干 + 救命 */
#define MOF_DEATHBOMB_STATE 4    /* 一手 `th18:0x45d4e4`：`player+0x476ac`（= `CE_PLAYER_STATE`）= 4 */

/* ★ **复刻**：决死形态的消弹**照常入信仰账**（每颗 +10），与主动灵击同款。
 *
 * 一手链（2026-09-15 复核，**推翻了上一版"TH10 决死不撒"的说法**）：
 *   · `Player::on_tick` 的状态跳表 `th10:0x426344`：[0]=`0x425751`、[1]=`0x425964`、
 *     [4]=**`0x425a96`（决死）**；而先前被当成"决死两处调用点"的 `th10:0x4258d4`/`0x4258f0`
 *     **落在 [0] 那一支**（`0x425846 jl 0x4258ab`；该支末尾 `0x425902 cmp [player+0x478],0x3c`
 *     → `[player+0x458]=1`）= **state 0「复活上升清弹」**，与决死无关（**误归属**）。
 *   · 真决死 `0x425bd9` 只调 `Bomb::start`（`0x425c3e`）⇒ 消弹走 `Bomb::on_tick` → `th10:0x405ac0`，
 *     其撒不撒的第二刀 = `(~[player+0x378c]) & 1`（`0x405af1`–`0x405b18`）—— **与普通灵击同一句**。
 *   ⇒ **两作同向：TH10 / TH11 的决死都照常撒**（TH11 侧 = `SIGNAL_BOMB_BY_HIT_ACCOUNT 1`，同为一手）。
 *   ⚠ 「决死不扣信仰」另有一条独立一手：扣 −3000 的 `call 0x412e70` 只出现在 **state 1（主动灵击）**
 *     那一支（`0x425a1f`）⇒ 决死不扣，与本常量无关。*/
#define MOF_BOMB_DEATHBOMB_ACCOUNT   1

/* 一发灵击的**形态**（本卡私有标量，照 `cards/animal_core.h` 的 `by_hit` 先例进了 core ⇒ 可单测）。*/
typedef struct {
    int32_t by_hit;    /* 形态：`MOF_BOMB_BY_PRESS` / `MOF_BOMB_BY_HIT` */
    int32_t cancels;   /* 诊断：本发累计消弹颗数（**无论是否入账**都记）*/
} mof_bomb_form_t;

/* 决死窗口判据（`player_state == 4`，一手 `th18:0x45d4e4`）。返回 1/0（`<0` 之类的野值一律 0）。*/
int32_t mof_core_deathbomb_window(int32_t player_state);
/* 开一发：记形态（`by_hit` 由卡侧「检测窗口」后传入）、颗数清零。`f == NULL` 安全。*/
void    mof_bomb_form_start(mof_bomb_form_t *f, int32_t by_hit);
/* 本发又消掉 `bullets` 颗（诊断计数；`<= 0` 不动）。*/
void    mof_bomb_form_cancel_note(mof_bomb_form_t *f, int32_t bullets);
/* 发动时该扣哪一档信仰：主动 = `FAITH_REDUCE_BOMB`（−3000）；**决死 = `FAITH_REDUCE_NONE`**。
 * （`faith_slot_reduce()` 对 `FAITH_REDUCE_NONE` 返回 0 ⇒ 调用方可以无条件调它，不必分支。）*/
int32_t mof_bomb_form_faith_kind(const mof_bomb_form_t *f);
/* 消弹是否入信仰账（1 = 入，每颗 +10；0 = 不入）。口径 = `MOF_BOMB_DEATHBOMB_ACCOUNT`。*/
int32_t mof_bomb_form_cancel_accounts(const mof_bomb_form_t *f);
/* 本发是不是决死形态（诊断/日志用）。*/
int32_t mof_bomb_form_is_deathbomb(const mof_bomb_form_t *f);

#endif /* MOF_CORE_H */
