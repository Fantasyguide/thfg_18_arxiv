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
 *     —— 2026-09-12 的三条订正出自它（§3.2/§3.4：无逐帧信仰衰减；§3.1：连击表大信仰点 = **+120f**；
 *     §1.1：TH10 **无擦弹倍率**；§4.1：灵击原版门槛 ≥1.00 / 无下限）。
 *     **凡 spec 与它冲突处以它为准**（一手 > 转抄）；它没覆盖的（如高度系数的曲线形状）仍按 spec 实现，
 *     冲突逐条标在函数注释里（`mof_core_height_coeff` / `mof_core_graze_mult`）。*/

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
    MOF_PICK_ADD_SMALL   /* 额外撒小信仰点实体（满 P 时 P 点转化，§7.9 / #48）*/
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
 *   ⇒ 只能退到 `on_enemy_drop_pre`（那里 `counts` 就是 ex_drop）。见 `mof.c` 的双路说明。*/
int mof_core_wants_big_death(uint32_t kind, uint32_t flags, int32_t drop_main, int is_boss,
                             int difficulty, int32_t *value_out);

/* 满 P 时原生 P 点 → 小信仰点（§7.9 / #48）：
 * `drop_main == 1` 或 `p_count > 0` 才谈转化；`power >= max_power`（满 P）才算。
 * 返回 `MOF_PICK_ADD_SMALL` 或 `MOF_PICK_DROP`。*/
mof_pick_t mof_core_pick_p_item(int32_t drop_main, int32_t p_count, int32_t power,
                                int32_t max_power, int is_boss);

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

/* ---- 连击槽（§7.5）：110 的那一套（130f 满）在 `faith_core` 里 ----
 * 这里只做「按帧的推进」这类**与引擎无关**的决策：
 *   · 连击槽 > 0 → 每帧 −1（充满 130f ⇒ 「充能值 = 能撑多少帧」）；
 *   · **boss 战中连击槽不衰减**（§2.3）⇒ 卡侧把 `in_boss_fight` 传进来；
 *   · ★ **槽空时什么都不做**（2026-09-12 一手订正，任务 ①）：**TH10 没有逐帧信仰衰减**。
 *     一手（`local/_scratch/research-th10-faith-and-graze.md` §3.1 / §3.2 / §3.4、§1.1-C）：对信仰
 *     `0x474c4c` 的写入点**已穷举** —— 增只有 3 类道具（大信仰 +5000/8000/10000、小 +100、微 +10），
 *     减只有 3 条（miss −(信仰−50000)/3、非决死灵击 −3000、Boss 阶段窗口到时 −30000）；
 *     连击计时器 `0x474c54` 只被 `0x412ff0` 充能、只被 HUD / 回放读 ⇒
 *     旧实现的「无敌人 −10800/s、有敌人 −600/s」在 TH10 里**没有任何对应**
 *     （`faith_core.h` 的那对数字是 SA 侧的机制，被错记到 MoF 头上）。
 *     共用件 `faith_slot_drain_empty()` **保留**（111 / 以后的卡可能用），但 **110 不再调它**。
 * ⚠ 本函数**恒返回 0**（返回值的原语义是「本帧扣掉多少信仰」）；保留 int32_t 只为调用点形状稳定。*/
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
 * `have110` / `have111` = 声明者的**存在性**（`ce_owned(id)` 那类查询，§3.1 允许的例外）；
 * `aggregate` = `ce_score_aggregate()`（SDK 聚合出来的 Σ 声明，**落笔者只读聚合值**）——
 * ⚠ 各卡声明的是**自己那份信仰**（110：基础 50000 + Δ_110；111：基础(难度) + Δ_111），
 *   所以 110 单装时 `aggregate` 就是「信仰_110」、两卡同装时它 = 两份信仰之和。*/
int32_t mof_core_pv(int have110, int have111, int32_t aggregate, int32_t graze_mult_permille,
                    int32_t signal_value);

/* 擦弹倍率（千分比定点）：`min(graze × 0.0001, 8.99)`。
 * ⚠ 单位说明：`0.0001/graze` 意味着 1 graze = 1 千分点，**8.99 的上限在千分点里是 8990**。
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
 *     （research §6「未取到」第 2 条）⇒ 这条式子仍是转抄。*/
int32_t mof_core_graze_mult(int32_t graze);

/* ---- 灵击的收尾（§2.1）---- */
/* 灵击 = 全屏消弹 + 全场伤害 + 震屏。常量是**照抄既有先例**的：
 *   · 消弹半径：照 Tenshi 要石（`0x40eb0c..`）与 season.c 解放半径的量级；
 *   · 伤害 / 震屏：❓ 原版 MoF 的灵击伤害与震屏参数**未抄到** ⇒ 具名占位常量 + TODO，实跑调。*/
#define MOF_BOMB_CANCEL_R     4000.0f   /* 全屏量级（season.c 同款上界）*/
#define MOF_BOMB_CANCEL_MAX   99999     /* 数量上限（不限）*/
#define MOF_BOMB_DMG          300       /* ❓ 占位：伤害源每帧伤害（TODO：抄 th10 灵击）*/
#define MOF_BOMB_DMG_LIFE     2         /* ❓ 占位：伤害源存活帧数 */
#define MOF_BOMB_SHAKE_TIME   20        /* ❓ 占位：震屏帧数 */
#define MOF_BOMB_SHAKE_START  12        /* ❓ 占位：震屏起始强度 */
#define MOF_BOMB_SHAKE_END    0         /* ❓ 占位：震屏结束强度 */

#endif /* MOF_CORE_H */
