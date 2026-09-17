/* faith_core.h —— 绿点 / 信仰系统（风神录 110 + 地灵殿 111 共用）的**纯逻辑**层。
 *
 * 只 <stdint.h>，主机 gcc 直接测（`tests/test_faith_core.c`）。引擎访问 / VM 池在 `faith.c`。
 *
 * 用户定案（2026-09-12，`docs/superpowers/specs/2026-09-11-system-card-spec-review.md` §7.1 / §3.2.1）：
 *   「111 的绿点和 110 的绿点**完完全全就是同一个东西**，它们加的都是分，只是加的数量不一样」
 *   ⇒ **一套实现 + 两个记账槽**（不是各做一套、也不是共用一个槽）。
 *   本文件就是那「一套」：**按槽（slot）记账**的加值 / 夹取 / 衰减 / 连击 / 收集判定。
 *
 * 三个槽的量纲（§7.1）：
 *   SLOT_110（风神录）：单位 = 信仰点（会消失的最大得点），基础恒 50000，Δ 下限 0。
 *   SLOT_111（地灵殿）：单位 = SA 绿点（不会消失），基础按难度，Δ 上限 = 原版基础。
 *   SLOT_SIGNAL（信号，111 侧）：0..130 的强度，**单列**（它是乘数的一半，不是得分）。
 *
 * 「单位换算」在这里的实现方式：**槽的原始单位 ×100 定点**存 `delta`（大信仰点 5000 → 500000），
 * 出槽外统一 `/100`（见 `faith_score_unit()`）—— 定点只是为了 111 的「小绿点每 100 个 +10」（§3.2.1
 * 的卡私有余数计数器）能不丢余数，110 这边数值全是 100 的倍数，等价于整数。
 */
#pragma once
#ifndef FAITH_CORE_H
#define FAITH_CORE_H
#include <stdint.h>

/* ---- 槽 ---- */
enum {
    FAITH_SLOT_110 = 0,     /* 风神录（信仰）*/
    FAITH_SLOT_111 = 1,     /* 地灵殿（SA 绿点）*/
    FAITH_SLOT_SIGNAL = 2,  /* 信号强度（111；0..130，不是得分）*/
    FAITH_SLOT_COUNT = 3
};

/* ---- 定点 ---- */
#define FAITH_FIXED         100   /* 槽内原始单位 ×100 定点 */

/* ---- 基础值（§7.2）---- */
#define FAITH_BASE_110      50000  /* 恒（所有难度）*/
/* 基础_111 按难度：E 25000 / N 50000 / H 100000 / L 200000 / EX 200000。用难度下标查（见下）。*/
#define FAITH_BASE_111_EASY    25000
#define FAITH_BASE_111_NORMAL  50000
#define FAITH_BASE_111_HARD   100000
#define FAITH_BASE_111_LUNATIC 200000
#define FAITH_BASE_111_EXTRA  200000

/* 难度下标（TH18 的 difficulty 值，与 `cards.js` / ECL 同口径：0 EASY … 3 LUNATIC、4 EXTRA）。
 * `faith_base_111()` 对越界入参钳到 LUNATIC。*/
enum { FAITH_DIFF_EASY = 0, FAITH_DIFF_NORMAL = 1, FAITH_DIFF_HARD = 2,
       FAITH_DIFF_LUNATIC = 3, FAITH_DIFF_EXTRA = 4 };

/* ==== ★★ 道具 type 编号 ↔ 档 ↔ 两作别名（D：术语统一，用户 2026-09-15 定案）============
 *
 * **两作是同一套 type 号，只是名字不同**（一手：type → 分派跳表两作同形 —— `th10:0x41b8a0`
 * （索引 = type−1，11 项）/ `th11:0x423f00`；数值落点见下表）：
 *
 *   | 道具 type | 本仓档 | TH10《风神录》(110) | TH11《地灵殿》(111) | 一手落点 |
 *   | --- | --- | --- | --- | --- |
 *   | **3** | `FAITH_TIER_BIG` | **大**信仰点 +5000/8000/10000 | **大**绿点 +100 | `th10:0x41b57f`（难度表 `0x41b8cc`）/ `th11:0x423c82` |
 *   | **9** | `FAITH_TIER_SMALL` | **小**信仰点 +100 | **中**绿点 +10 | `th10:0x41b5fc` / `th11:0x423cd7` |
 *   | **8** | `FAITH_TIER_MICRO` | **微**信仰点 +10 | **小**绿点 **+0.1** | `th10:0x41b5d6` / `th11:0x423cc4` |
 *
 * ★ **单位（2026-09-15 复核，防 100 倍误读）**：TH11 的绿点计数器 `th11:0x4a56f0` 是 **×100 定点**
 *   —— 初始化 `th11:0x41f9f5`/`th11:0x41fa0f` 都 `imul …,0x64`（×100）后写 `th11:0x41fa2b`；
 *   HUD 读它时在 `th11:0x41ce04` 用魔术数 `0x51eb851f`+`sar edx,5` = **÷100** 还原显示值。
 *   所以 type 8 收一颗时 `push 0xa; call th11:0x424640` 的 **+10 = 显示 +0.1**（不是 +10！）；
 *   type 9 的 +1000 = 显示 +10、type 3 的 +10000 = 显示 +100 —— 与本表三档**逐条对上**。
 *   ⚠ 同一 handler 里另有一条 `th11:0x40ceb0` 是**分数**通道（+10/10 = 显示 +10），别与计数器混为一谈。
 *
 * ⇒ 「110 的**小**信仰点」与「111 的**中**绿点」**是同一个槽（type 9）**；
 *   「110 的**微**」与「111 的**小**」同槽（type 8）—— 只是名字与量纲不同。
 *
 * ⚠⚠ **两作量纲不同，别跨作搬数**（一手）：
 *   · **TH10：信仰显示 = 存储 × 10**（`add_to_piv` `0x41be80` 内部 `/10`；初值 `push 0xc350 (50000)`
 *     喂 `set_faith` `0x418b80`，而它内部再 `/10`；回放存内部值、载入 ×10 再喂 —— 三重证据见
 *     `local/_scratch/research-th10-faith-and-graze.md` §2）；
 *   · **TH11：信仰显示 = 存储 ÷ 100**（`add_to_piv` `th11:0x424640` **不除 10**；HUD = `trunc10(⌊存储/100⌋)`）。
 *   ⇒ 同一句「+100」在 TH10 是**显示** +100、在 TH11 是**存储** 100 = 显示 +1 —— **只有显示值可以互相对照**。
 *   本文件所有数值常量一律写**显示单位**（`FAITH_MOF_*` = 110 的显示值、`FAITH_SA_*` = 111 的显示值）。
 *
 * ---- ★ E-1 硬约束：**档位 ↔ 来源不许串档**（两作一致；用户 2026-09-15 口径）----
 *   | 档（type）| 来源 | TH10 一手 | TH11 一手 |
 *   | --- | --- | --- | --- |
 *   | 大（3）| **击破掉落** | 大信仰点难度表 `0x41b8cc` | `th11:0x423c82` 系 |
 *   | 中 / 小（9）| ★ **满 P 转换**（P 道具 → 信仰点）| `0x41bbf8` 跳表（type 1/4 → 9）+ 全场转换 `0x41ba50` | 表 `0x424578` / 全场转换 `0x4240f0`（1/4/10/11 → 9）|
 *   | 微 / 小（8）| ★ **消弹**（每颗被消的弹）| `0x4081e7 push 8` | `th11:0x40b0a2`（`mov ecx,0x8` → `call 0x424230`）|
 *   ⇒ **满 P 转换只许产出 `FAITH_TIER_SMALL`、消弹只许产出 `FAITH_TIER_MICRO`**。
 *   `faith_core_tier_for_source()` 是这条约束的**唯一出处**（卡侧不要再手写 tier 字面量）。
 *   这也是共用池「一套实体 + 各自数值表」能成立的前提：两作的对应关系相同。
 *
 * ---- ★ E-2 生成运动学：**固定角 −π/2 + 定速**（旧实现的「随机散」是 ❓ 自定，2026-09-15 删）----
 * 一手（实参立即数，逐条可复跑）：
 *   · **击破 / 满 P 转换族**（大 / 中）：角 **−π/2** = `0xbfc90fdb`、速 **2.2** = `0x400ccccd`
 *     —— `th10:0x41bac1`（满 P 全场转换的每一颗）、`0x40c9b6` / `0x40e661`（掉落表撒落）；
 *       TH11 侧 = `th11:0x42415a`/`0x424184` 的 `0x49842c`（−π/2）与 `0x498430`（2.2）；
 *       TH18 自己的 main-drop 同参（`th18:0x430481` / `0x4305d9`，与 `engine.h` 的 `CE_FN_SPAWN_ITEM` 注一致）；
 *   · **消弹族**（微）：角 −π/2、速 **0.6** = `0x3f19999a` —— `th10:0x4081d9`（TH11 侧由弹自己的速度给）。
 *   ⇒ 两族的**角都是 −π/2（直上）**，只有速度不同；且**撒在实体自己的位置**（弹 / 道具原位）。
 * ⚠ 浮点只在本 header 的 `static inline` 里出：i686 的 float 返回值走 x87 st(0)，独立函数会让
 *   `make dllx87`（本仓不变式「我们的 object 里 0 条 x87 指令」）变红。*/
typedef enum {
    FAITH_TIER_BIG = 0,          /* 道具 type 3（大信仰点 / 大绿点）*/
    FAITH_TIER_SMALL = 1,        /* 道具 type 9（小信仰点 / 中绿点）*/
    FAITH_TIER_MICRO = 2         /* 道具 type 8（微信仰点 / 小绿点）*/
} faith_tier_t;

#define FAITH_TYPE_BIG    3      /* 一手 type 号（两作同表；见上表）*/
#define FAITH_TYPE_SMALL  9
#define FAITH_TYPE_MICRO  8

/* 这颗点**从哪来**（E-1 的来源枚举）。`faith_core_tier_for_source()` 给出它**必须**产出的那一档。*/
typedef enum {
    FAITH_SRC_BREAK = 0,   /* 击破掉落（绿妖精 / 同时掉钱与 P 的敌人）→ 大 */
    FAITH_SRC_FULL_P = 1,  /* 满 P 转换（原生 P 道具被替换掉）→ 中 / 小（type 9）*/
    FAITH_SRC_CANCEL = 2   /* 消弹（每一颗被消的弹）→ 微（type 8）*/
} faith_source_t;

faith_tier_t faith_core_tier_for_source(faith_source_t src);

/* TH18 的「P 类」道具 type（一手 `th18:0x44625f` 分派表：`[item+0xc78]` = type，
 * `type−1` 索引 byte 表 `0x44685c` → 跳表 `0x446830`）：
 *   type 1 = **小 P**（`0x4462cc` → `0x446870`，原生 `collect_power(1)`）、
 *   type 3 = **大 P**（`0x4462d8`，原生 `collect_power(100 = 1.00 档)`）。
 * ⚠ 这是 **TH18 的类型号**，跟 TH10 的 1/4（小 P / 大 P）只是「同角色、不同编号」。
 * ⚠ 原生 type 8（补满火力）**不算 P 类**（TH10 没有这一档 ⇒ 无从对应；`TUNING.md` §2.5）。*/
#define FAITH_TYPE18_P_SMALL  1
#define FAITH_TYPE18_P_BIG    3

/* 这颗 TH18 道具是不是「P 类」（满 P 时该被替换的那一族）。*/
static inline int faith_core_is_p_item_type(int32_t type18)
{ return type18 == FAITH_TYPE18_P_SMALL || type18 == FAITH_TYPE18_P_BIG; }

/* ======================================================================================
 * ★★ 2026-09-18（B1 对齐）：本池的**道具状态**编号 ↔ 引擎道具 state（`engine.h` 的 `CE_ITEM_STATE`）
 * ======================================================================================
 * 本池（`cards/faith.c` 的 `faith_dot_t.state`）的编号是当初「照抄引擎道具运动学」时定下的，
 * 与引擎的 `item+0xc74` **不可逐值对照**，只有**真空飞行那一档是语义对位**：
 *
 *   | 本池 state | 含义 | 引擎侧对位（`item+0xc74`）|
 *   | --- | --- | --- |
 *   | 0 | 待发射（还没建 VM）| 0（空槽）|
 *   | 2 | 飞散 / 上抛（出生那一下）| 1（抛物 / 坠落体）|
 *   | 1 | 飘落（重力在跑）| 1 的后半段 |
 *   | **3** | **过回收线 / 放 B / 对话 ⇒ 朝自机飞** | ★ **3（真空飞行）** |
 *   | 4 | 70px 吸引半径内 ⇒ 朝自机飞 | 4（环吸附）|
 *
 * ## 为什么单把 3 这一档摘出来（`NEXT.md` §1x.2 / B1；用户 2026-09-18 定的验收规格）
 * 114（辉针城）的收点批**只认「真空飞行」**：引擎侧谓词 = `type ∈ {1,2}` **且** `item+0xc74 == 3`
 * （成员门 `cards/collect2x.c` 的 `on_item_collect` 与 presence 谓词 `ce_item_pool_any_attracting`，
 * 一手见 `AUDIT.md` §Z22）⇒ 坠落中的道具（state 1）与环吸中的道具（state 4）**既不计数也不续批**。
 * 用户 2026-09-18 的规格原话要点 =「**信仰点本身得做到：① 开批 ② 计数 +1 ③ 飞行中的信仰点会组织
 * 批次结算 —— 也就是与 P 点和金钱完全一致**」⇒ 我们的点在 114 那两条通道上必须**用同一谓词**：
 *   · **算批成员 / 算在飞** = 只有 `state == 3`（= 引擎 state 3 的同一语义：过回收线 / 放 B / 对话）；
 *   · 飘落 / 飞散（`state ∈ {1,2}`）与 70px 环吸（`state == 4`）**都不算** —— 引擎对 P / 钱正是这样。
 * ★ 这一条同时把「普通游玩顺手捡绿点」与「收点」分开了：前者（触收 / 环收）根本不开批、不续批，
 *   与引擎 §Z22 的收窄**同一条**；旧实现（2026-09-15 §1x.2，不看状态）会让普通捡点开批/续批
 *   ⇒ 实跑症状「绿点一直触发收点」「boss 战期间 bonus 对着自己掉的 P 点触发」。
 * ⚠ 微档（`FAITH_TIER_MICRO`）不进批是**另一条**门（`CE_VITEM_HALF`，§1x.3「半道具」），别混。*/
typedef enum {
    FAITH_STATE_IDLE    = 0,   /* 待发射（还没建 VM）*/
    FAITH_STATE_FALL    = 1,   /* 飘落（重力在跑）*/
    FAITH_STATE_SCATTER = 2,   /* 飞散 / 上抛（出生那一下）*/
    FAITH_STATE_VACUUM  = 3,   /* ★ 过回收线 / 放 B / 对话 ⇒ 朝自机飞（= 引擎 state 3 的对位）*/
    FAITH_STATE_RING    = 4    /* 70px 吸引半径内 ⇒ 朝自机飞（= 引擎 state 4 的对位）*/
} faith_state_t;

/* 这颗点**此刻**算不算「收点批的成员 / 在飞体」（= 引擎 state 3 的同一谓词，见上表）。
 * ⚠ 114 那两条通道（成员 / presence）**都**必须过这一个判据；不许在别处另写一份状态比较。*/
static inline int faith_core_state_is_vacuum_flight(int32_t state)
{ return state == (int32_t)FAITH_STATE_VACUUM; }

/* 本粒子的出生速度（E-2 一手；角度一律 `FAITH_THROW_ANGLE`）。*/
#define FAITH_THROW_ANGLE         (-1.5707964f)   /* −π/2（`0xbfc90fdb` / `th11:0x49842c`）*/
#define FAITH_THROW_SPEED_MAIN    2.2f            /* 击破 / 满 P 转换族（`0x400ccccd` / `th11:0x498430`）*/
#define FAITH_THROW_SPEED_CANCEL  0.6f            /* 消弹族（`th10:0x3f19999a`）*/
static inline float faith_core_throw_speed(faith_tier_t tier)
{ return (tier == FAITH_TIER_MICRO) ? FAITH_THROW_SPEED_CANCEL : FAITH_THROW_SPEED_MAIN; }

/* ======================================================================================
 * ★★ 2026-09-15h：P 点补值的**共享策略**（110 + 111 两张卡**同一个**落笔口径）
 * ======================================================================================
 * ## 为什么要有这一层（否则爱丽丝的减益会被绕过）
 * 「一颗小 P 补到 +0.05 档」这一层的落点 = `on_item_commit`（`th18:0x44658a`，推导见 `mof_core.h`
 * 的 §P 点火力），而它是「**每颗道具全局只补一次**」：落笔者由 `mof_core_p_owner()` 选举
 * （id 最小者胜）⇒ **110 在场时落笔者恒为 110**。
 * 111 的支援特技「爱丽丝」按用户口径包含**两件**（① 灵击减半 ② **P 点收益变成平均 +0.025 档**），
 * 而 ② 原来只写在 111 的卡侧 ⇒ 双卡同装时被 110 的 +0.05 绕过。
 * ⇒ 把「**本颗该补多少**」做成**共享策略**：两张卡的 `on_item_commit` 都只做
 *    「**问策略 → 用它当补值**」，**谁都不许自己算 2/3 交替**。
 *
 * ## 判据（**不许用槽值当判据**）
 *   · **爱丽丝生效** = 「**111 在场**」∧「**TH18 机体 == 魔理沙(1)**」；
 *   · 在场判据 = `faith_registered_mask()` 的 111 位（在 `faith.h`；**不是**槽值、也**不是**本卡自己猜）；
 *   · 机体门 = `CE_ADDR_CHARACTER`（`0x4cccf4`）== `FAITH_CHAR_MARISA` —— 这是**用户拍板的 Σ 映射**
 *     （`signal_core.h` 的「支援特技」整段：「TH18 魔理沙(1) = Σ 爱丽丝 + 帕秋莉 + 荷取」），**不是复刻**。
 *
 * ## 相位（2/3 交替）**只有一份**
 *   `faith_p_alt_t` 是**纯数据**：放在共享件的静态区（`faith.c` 的 `s_p_alt`），由**落笔者**在每次
 *   补值时推进一次 ⇒ 不变式 = 「**同一颗道具只推进一次相位**」（落笔本来每颗只发生一次，
 *   见 `mof_core_p_owner()` 的选举）。⚠ 双卡各持一份相位会打架（两边的 `alt` 会分叉成两个序列）。
 *
 * ## 诊断
 *   `items` / `lo` / `hi` 三个计数只在诊断行里看（实跑对账 `lo − hi ∈ {0,1}`）。
 * ==================================================================================== */
#define FAITH_P_TOPUP_NORMAL   5   /* 常态：一颗小 P 总共 **+5 单位 = +0.05 档**（= `MOF_P_POWER_UNITS`）*/
#define FAITH_P_TOPUP_ALICE_LO 2   /* 爱丽丝生效：第 1 颗总 **+2 单位（0.02 档）** */
#define FAITH_P_TOPUP_ALICE_HI 3   /* 爱丽丝生效：第 2 颗总 **+3 单位（0.03 档）** ⇒ 平均 **2.5 单位 = 0.025 档**
                                    *   （🟡 用户口径；0.025 档 = 2.5 单位**无法用整数精确表示** ⇒ 交替）*/
#define FAITH_CHAR_MARISA      1   /* `CE_ADDR_CHARACTER` 的魔理沙（0 灵梦 / 1 魔理沙 / 2 咲夜 / 3 早苗）*/

typedef struct {
    int32_t alt;      /* 交替相位：0 = 下一颗走 LO(+2)、1 = 下一颗走 HI(+3)（首分配清零 ⇒ 第 1 颗走 LO）*/
    int32_t items;    /* 诊断：爱丽丝生效时推进过的颗数 */
    int32_t lo, hi;   /* 诊断：LO / HI 两侧各发了几颗 */
} faith_p_alt_t;

/* 爱丽丝这条特技此刻生不生效（**纯判据**；两个入参由共享件从引擎读出来喂进来）。返回 1/0。*/
static inline int faith_core_p_alice_on(int has_111, int is_marisa)
{ return (has_111 != 0) && (is_marisa != 0); }

/* 本颗小 P 的**总**增量（单位 = TH18 的 1/100 档：常态 5 / 爱丽丝 2 或 3），**并推进相位**。
 *   · `alice_on == 0` ⇒ 返回 `FAITH_P_TOPUP_NORMAL` **且不推进相位**（⇒「只在爱丽丝生效时改变」可证）；
 *   · `usable == 0`（**满火力 / max 未初始化** ⇒ 原生也不给火力，这一颗拿不到任何增量）⇒ 同样返回常态
 *     **且不推进相位**：否则「白吃掉一个相位格」会让**真正拿到火力的那些颗**偏离 2.5 的平均
 *     （例：满火力那颗占掉 LO，下一颗就恒拿 HI ⇒ 平均值被拉到 3）；
 *   · `alt == NULL`（没有状态）⇒ 返回常态、不推进。*/
int32_t faith_core_p_topup_total(faith_p_alt_t *alt, int alice_on, int usable);

/* ---- 绿点三档单颗值（§7.1 的表；111 侧单位 = **显示点**，与 th11 一手对得上）----
 * ★ 2026-09-15 一手复核（`local/_scratch/research-th11-faith-and-points.md` §4.1，地址在 th11 v1.00a）：
 *   大 = item type 3（`th11:0x423c82`，难度跳表 `th11:0x423f2c` 全指 `0x423c98`）⇒ `add_to_piv(+10000 存储)`
 *        = **显示 +100**（弹窗数字 = `+100`，绿 `0xff00ff00`）；
 *   中 = type 9（`th11:0x423cd7`）⇒ `+1000 存储` = **显示 +10**（弹窗 `+10`）；
 *   小 = type 8（`th11:0x423cc4`）⇒ `+10 存储` = **显示 +0.1**，**另 +10 分**（`th11:0x423ccd`）。
 *   量纲：th11 的信仰存储 = 显示 ×100（`add_to_piv` = `0x424640` **不除 10**），HUD 显示 `trunc10(⌊存储/100⌋)`。
 *   ⇒ 社区那句「小绿点每 100 个 +10」是 **÷100 量纲的观测结果**（100 × 0.1 = 10），
 *     不是「有个计数器数到 100 再给分」—— 代码里没有任何这种计数器（`add_to_piv` 只有 3 个调用点）。 */
#define FAITH_MOF_BIG_EN   5000    /* 大信仰点 E/N（th10 一手：0x41b57f 的难度表）*/
#define FAITH_MOF_BIG_H    8000    /* 大信仰点 HARD */
#define FAITH_MOF_BIG_L   10000    /* 大信仰点 LUNATIC（/EX）*/
#define FAITH_MOF_WEI       10     /* 微信仰点：**消弹每颗 +10**（§2.2）*/
#define FAITH_MOF_SMALL    100     /* 小信仰点（满 P 时 P 点转化）*/
#define FAITH_SA_BIG       100     /* 大绿点（111）= th11 type 3 的「显示 +100」*/
#define FAITH_SA_MID        10     /* 中绿点（111）= th11 type 9 的「显示 +10」*/
/* 小绿点的**本仓口径**：一手是「每颗 = 显示 +0.1 + 10 分」；我们的槽定点只能到 1/100 显示
 * （`FAITH_FIXED = 100`）⇒ 按「每 `FAITH_SA_SMALL_N` 颗进一笔 `FAITH_SA_SMALL_V`」结算，
 * **行为等价**（100 颗 = 显示 +10，余数跨帧 / 跨关保留，见 `faith_slot_t.small_rem`）。
 * ⚠ 一手的「每颗 +10 分」我们**没有**做（111 的分数只走蓝点分那条通道）—— 见 `signal.c` 的说明。*/
#define FAITH_SA_SMALL_N   100     /* 小绿点：**每 100 个** +10（= 一手 ÷100 的观测口径）*/
#define FAITH_SA_SMALL_V    10
/* Miss 对信仰（111）：一手 `th11:0x4327e7: sub eax,0x989680`（−10,000,000 **存储** = **显示 −100,000**），
 * 下限写 `MIN_PIV`（`th11:0x4327fd`，= 该难度初值 = 我们的 `base`）⇒ 对 Δ 而言就是**夹到 0**（§6.1 #3）。
 * 同一函数还把交信度清零（`th11:0x432807`）—— 那一条走 `signal_core_reset()`。*/
#define FAITH_SA_MISS_PIV_LOSS  100000

/* ---- 信仰减少（§7.5 / §2.2）---- */
#define FAITH_MISS_DIV      3      /* miss −(信仰−50000)/3（对 Δ 而言 = −Δ/3）—— **110 侧**（th10 一手）*/
#define FAITH_SPELL_MISS_COST   3000   /* 非决死灵击 −3000 —— **110 侧**（th10 一手）*/
#define FAITH_ALL_AVOID_COST   30000   /* 全避 boss 非符 / 符卡 −30000（时符必减）—— **110 侧** */
/* ---- 逐帧信仰衰减（**110 侧，一手**）----
 * ★★ 2026-09-15 复核订正（`local/_scratch/research-th10-faith-units-and-drain.md` §3 / §6.2）：
 *   这一节原来写着「**没有一手出处**、110/111 都不许调」——**那条是错的**，本轮撤销。真值：
 *   · **TH10 有**逐帧衰减（`th10:0x418a17`–`0x418a3c`）：槽 `0x474c58` ≤ 0 时
 *     `存储信仰(0x474c4c) −= 速率(0x474c50)`，速率当场抬到下限 18 ⇒ **−18 存储/帧 = −180 显示/帧**，
 *     结果硬钳存储 5000（= 显示 50000 = 我们的 `FAITH_BASE_110`）；
 *   · **门**（`th10:0x418740`–`th10:0x41875d`，三条全满足才衰减）：
 *     无 Boss（`ENEMY_MANAGER+0x10 == 0`）**∧** 无对话（`GUI+0x9eb8 == 0`）**∧** 关卡帧 ≥ 90；
 *   · **TH11 没有**（`research-th11-faith-and-graze.md` §6.1 的写入点穷举里没有这一条）⇒
 *     **111 侧一律不调**（111 连连击槽都没有，门都凑不出来）。
 *
 * **两档速率**（选档在 `mof_core.h` 的 `mof_faith_rate_fixed()`，判据 = `enemies_present`）：
 *   · 无敌人 `10800` —— ✅ **一手**：速率字段 `0x474c50` 被 `th10:0x418a30`
 *     （机器码 `c7 40 10 12 00 00 00` = `mov [eax+0x10],0x12`）**读完立刻抬到下限 18**
 *     ⇒ 有效速率恒 **18 存储/帧 = 180 显示/帧** = **10800 显示/s**；
 *   · 有敌人 `600` —— 🟡 **用户口径（2026-09-15）**，一手证据**不支持**：`0x474c50` 只有 2 个写点
 *     （击破 `th10:0x40e26b` = **1**、开局 `th10:0x417a00` = **9**），而读点前那个抬到 18 的写让字段
 *     **单调不减** ⇒ 「1 存储/帧 = 600 显示/s」这档在一手里**读不出来**。
 *     ⚠ 用户是 TH10 老玩家、这条是他的游戏事实 ⇒ **这档保留并接上判据**，同时如实标 🟡；
 *     冲突明细与「怎么钉」见 `mof_core.h` 的「衰减速率：两档」块 + `TUNING.md` §2.5h。*/
#define FAITH_EMPTY_NO_ENEMY_PER_SEC  10800   /* ✅ 复刻（th10:0x418a26/0x418a30）：−180 显示/帧 × 60 */
#define FAITH_EMPTY_ENEMY_PER_SEC       600   /* 🟡 用户口径 2026-09-15（一手不认，见上）：−10 显示/帧 × 60 */
#define FAITH_FRAMES_PER_SEC  60   /* zTimer 口径：60 帧 = 1 s */

/* ---- 连击条（§7.5）**只服务 110（th10）**；111 侧一律不调（见上面「连击条」块的说明）----
 * ★★ 2026-09-15 复核订正（一手 th10）：连击计时器 `0x474c54`（`= GLOBALS+0x14`，`current` = `0x474c58`，
 *   上限 130）**每帧 −1 在 `th10:0x418a10`**，而那一步与信仰衰减**在同一个函数、同一组门之后**
 *   （`th10:0x418a00` 的唯一调用点 `th10:0x418764` 在「无 Boss ∧ 无对话 ∧ 关卡帧 ≥ 90」之后）
 *   ⇒ 「Boss 战 / 对话中 / 关卡前 90 帧」三种情况下**槽与信仰一起冻结**；
 *   ⇒ 槽**确实**驱动信仰衰减（槽 ≤ 0 才扣）。旧注释里「只被 HUD / 回放读」是被推翻的旧结论。
 *   ⚠ TH11（111）一手**没有连击槽**（`research-th11-faith-and-graze.md` §6.1）⇒ 111 侧不调本节。*/
#define FAITH_COMBO_FULL  130      /* 满槽 = 130 帧（一手 `th10:0x413035 mov [esi+0x4],0x82`）*/
#define FAITH_COMBO_BIG       130  /* 大信仰点（⚠ 110 侧用 `MOF_COMBO_BIG` = 120f，一手；这里保留旧值）*/
#define FAITH_COMBO_GREEN     100  /* 蓝点（= 我们的绿点收集）*/
#define FAITH_COMBO_SMALL_P    60  /* 小 P / 小信仰点 */
#define FAITH_COMBO_BIG_P      20  /* 大 P / 击杀非 boss */
#define FAITH_COMBO_WEI         3  /* 微信仰点（消弹每颗）*/
#define FAITH_COMBO_KILL_BOSS   0  /* 1UP / 击杀 boss = +0 */

/* ---- 灵击：门槛 / 消耗 / 下限（§3.2 + §3.2.1，110 与 111 共用同一套）----
 * 换算式：MoF 下限 0.00 ↔ TH18 下限 1.00 ⇒ TH18 1.00 = MoF 0.00、TH18 2.00 = MoF 1.00。
 * 门槛 = TH18 ≥ 2.00 档；每次扣 1 档；★ 用户「加一个サービス」⇒ **下限 1.05 档**。
 *
 * ⚠ 为什么必须自写扣法（不要用 `ce_spend_power` / 原生 `GlobalsInner__spend_power` `0x457480`）：
 *   ① 它在 `0x4574a3` 判 `CURRENT_POWER <= 一档` 就**直接拒绝**（返回 0、不扣）；
 *   ② 它**只能整档扣**（档数语义），落不到 1.05 这种非整档值。
 *   ⇒ 见 `faith_power_spend()`：自己写 `CURRENT_POWER`（`0x4ccd38` = `GlobalsInner+0x5c`）+ 夹下限，
 *      扣完**必须** `ce_repopulate_options()`。 */
#define FAITH_POWER_PER_LEVEL  100     /* 一档 = CE_ADDR_POWER_PER_LEVEL 0x4ccd40 = 100（0x45a98b）*/
#define FAITH_POWER_MIN_LEVEL  2       /* 门槛：TH18 ≥ 2.00 档 */
#define FAITH_POWER_FLOOR      105     /* 下限 = 1.05 档（定点 105）*/
#define FAITH_POWER_COST       100     /* 每次扣 1 档 */

/* ---- 信号强度 = TH11「通信强度」槽（111 侧）★★★★ 2026-09-15 整段按一手重写 ----
 *
 * **一手出处**：`local/_scratch/research-th11-signal.md`（th11 v1.00a 逐条反汇编）+ 本文自己的复核。
 *
 * 载体与量纲（一手 §1）：
 *   · 槽 = `th11:0x4a56f4`（`zGlobals+0x14`），**int32 定点 ×100**：存 0…13000 = **显示 0.00…130.00**；
 *   · 「满」判据 = 存 10000 = 显示 100.00（`th11:0x42364a: cmp DWORD PTR ds:0x4a56f4,0x2710`）；
 *   · 硬上限 = 存 13000 = 显示 130.00（`th11:0x40bab5: mov DWORD PTR ds:0x4a56f4,0x32c8`；每帧函数尾再钳一次）。
 *   ⇒ 本仓用**千分点**（1 显示点 = 1000）：130.00 显示 = `130000`、100.00 显示 = `100000`。**一一对应**。
 *
 * 每帧增减（一手 §2，函数 `th11:0x420a70`，唯一调用点 `th11:0x4207e6`，每帧一次）：
 *   | 玩家 y | 槽 < 100.00 | 槽 ≥ 100.00 |
 *   | --- | --- | --- |
 *   | `< 64`（离回收线 > 64 px） | **+50.00/帧**，并把槽**钳到 100.00** | **+0.16/帧** |
 *   | `64…80`（离线上 48…64 px） | +50.00/帧，钳 100.00 | **+0.12/帧** |
 *   | `80…128`（线上） | +50.00/帧，钳 100.00 | **+0.09/帧** |
 *   | `≥ 128`（线下） | **−0.30/帧**，下限 0 | 同左 |
 *   ⚠ 一手的分界是**绝对 y**（`th11:0x497f8c`=64.0f、`th11:0x498080`=80.0f、`th11:0x497f30`=128.0f，
 *   128.0f 同时是 TH11 的**道具回收线**）。TH18 的回收线是引擎字段 `player+0x47998`
 *   （`faith_player_poc_height()`）⇒ 本仓把三档表达成**相对回收线的偏移**（>64 / 48…64 / 0…48 px）。
 *   🟡 这假设「两作的线与屏高同量级」（未实跑验证）⇒ 见 `TUNING.md` §2 的 111 行。
 *   ⚠ 「线上未满 +50/帧」在原版是**直接补满**（`add eax,0x1388` 后 `cmp eax,0x2710` ⇒ 钳到 100.00），
 *     不是「每帧 +50 直到满」—— 对我们数值上等价（见 `faith_signal_step()` 的实现）。
 *   ⚠ 原版这条每帧函数在**对白中**与**一局前 90 帧**不跑（`th11:0x4207d9`/`0x4207db`）⇒ 本仓未实现（❓）。
 *
 * 擦弹增减（一手 §3，4 个站点：`th11:0x408c49` / `0x411eaa` / `0x4260ec` / `0x4275c4`）：
 *   槽 += **按难度查表**的值，与「线上/线下」**无关**（4 个站点都不读玩家 y）：
 *     · 子弹 / 敌体擦弹：`th11:0x4a7950` = `th11:0x4a79d4` = **{800,500,500,500,500}** 存储
 *       = **{+8.00, +5.00 ×4}**（⇒ 社区那句「擦弹 +8」只对 **Easy** 成立）；
 *     · 激光擦弹：`th11:0x4a7a68` = `th11:0x4a7a7c` = **{500,400,300,200,200}** 存储
 *       = **{+5.00, +4.00, +3.00, +2.00, +2.00}**（且原版每 3 帧才算 1 擦弹）。
 *   ⇒ 表在这里具名；`signal_core_graze_gain()` 做查表（111 私有）。
 *   ⚠ TH18 的原生擦弹计数（`0x4ccd0c`）**不区分来源**（子弹 / 激光）⇒ 本卡只用「子弹 / 敌体」表，
 *     激光表具名备用（❓ 未接，见 `TUNING.md`）。
 *
 * 归零（一手 §4）：被弹（`th11:0x432807` + 玩家状态 2/4 每帧写 0）、一局开始（`th11:0x41fb24`）、
 *   玩家登场 60 帧（状态 0，`th11:0x431097` 每帧写 0）。
 *   ⚠ 一手**没有**「放灵击清零」这条写入点（7 个写点穷举见 §1.2）—— 旧注释里那句是错的（已删）。
 *
 * 满槽效果（一手 §5）：道具进回收框时若槽 ≥ 100.00 ⇒ 把该道具切 `state 3`（吸附自机 = 被收走）。
 *   ⇒ 我们的 `ce_item_collect_all()`（把活道具切 state 3）与它**同一条路**（`EVENTS-CONTRACT.md` §6）。 */
#define FAITH_SIGNAL_MAX        130000   /* 130.00 显示 = 一手硬上限 13000 存储（`th11:0x40bab5`）*/
#define FAITH_SIGNAL_HI_MARK    100000   /* 100.00 显示 = 「满」（`th11:0x42364a` 的 0x2710）*/
#define FAITH_SIGNAL_GAIN_LINE     50000 /* +50.00/帧（线上且未满；结果钳到 `FAITH_SIGNAL_HI_MARK`）*/
#define FAITH_SIGNAL_GAIN_TOP        160 /* +0.16/帧（已满，离线上 > 64 px；`add eax,0x10`）*/
#define FAITH_SIGNAL_GAIN_MID        120 /* +0.12/帧（已满，离线上 48…64 px；`add eax,0xc`）*/
#define FAITH_SIGNAL_GAIN_TRICK       90 /* +0.09/帧（已满，离线上 0…48 px；`add eax,0x9`）—— 即常见的「≥100 后 +0.09」*/
/* ★★★ 2026-09-15f **订正一个 10× 的量纲错**（用户实跑「擦弹一点也不涨信号强度」的根因）。
 *   原值 `3000` 与紧邻的注释「−0.30/帧」**自相矛盾**：本文件的槽单位是**千分点**
 *   （`FAITH_SIGNAL_MAX = 130000` = 显示 130.00、`GAIN_TOP = 160` = +0.16/帧、`GAIN_LINE = 50000` = +50.00/帧
 *   —— 三条都与注释逐字对上），所以「−0.30/帧」必须是 **300**，`3000` 是 **−3.00/帧（快 10 倍）**。
 *   一手三条（互为独立，全在 `local/_scratch/research-th11-signal.md`）：
 *     · §「跌源只有一条」：每帧玩家在回收线下方时 **−0.30/帧**（下限 0）；
 *     · 反汇编原文 `th11:0x420a85 lea esi,[edx-0x1e]` = **−30**（存储单位）+ `0x420b03 add [edi+0x14],esi`；
 *     · 同文件的量纲自洽检查：「−30/帧 = −0.30/帧 ✓；+9/帧 = +0.09/帧 ✓；+0x1388 = +5000 = +50.00/帧 ✓」。
 *   TH11 的槽存储 = 显示 ×100（`th11:0x41f9f5` 的 `imul …,0x64`）⇒ −30 存储 = −0.30 显示 = **−300 千分点** ✓。
 *   影响（订正前）：线下每帧扣 −3.00（= −180/s）远超擦弹的 +5.00/颗 ⇒ **槽恒被压在 0**，
 *   于是「擦弹一点也不涨信号强度」（用户 2026-09-15 实跑原话）。订正后：线下 100.00 → 0 约 5.5 s，
 *   与一手的 +5.00/颗 擦弹增益同量级 ⇒ 擦弹会把槽顶起来 ✓（TH11 的真实手感）。
 *   ⚠ 本条**只改这一个常量的值**：`faith_signal_step()` 的分支、门的判据、卡侧的调用次序一个字节没动；
 *     单测里那两条断言写的是符号（`== -FAITH_SIGNAL_LOSS_OFFLINE`）⇒ 不因换值而失效。
 *     登记：`AUDIT.md` §BE（共享件改动）+ `TUNING.md` §2.6f。谁改这条请连同两处登记一起改。*/
#define FAITH_SIGNAL_LOSS_OFFLINE    300 /* −0.30/帧（线下；一手 `th11:0x420a85 lea esi,[edx-0x1e]` = −30 存储），下限 0 */
/* 档位分界（px，**相对回收线**；一手绝对 y 64/80/128 − 线 128） */
#define FAITH_SIGNAL_BAND_TOP_PX      64 /* y < 64  ⇔ 线上 > 64 px */
#define FAITH_SIGNAL_BAND_MID_PX      48 /* y < 80  ⇔ 线上 > 48 px */

/* 擦弹增益表（千分点）。索引顺序 = `FAITH_DIFF_*`（E/N/H/L/EX）。一手见上面的注释。*/
#define FAITH_SIGNAL_GRAZE_BULLET_EASY     8000  /* +8.00（`th11:0x4a7950[0]` = 800 存储）*/
#define FAITH_SIGNAL_GRAZE_BULLET_NORMAL   5000  /* +5.00 */
#define FAITH_SIGNAL_GRAZE_BULLET_HARD     5000  /* +5.00 */
#define FAITH_SIGNAL_GRAZE_BULLET_LUNATIC  5000  /* +5.00 */
#define FAITH_SIGNAL_GRAZE_BULLET_EXTRA    5000  /* +5.00 */
#define FAITH_SIGNAL_GRAZE_LASER_EASY      5000  /* +5.00（`th11:0x4a7a68[0]` = 500 存储）*/
#define FAITH_SIGNAL_GRAZE_LASER_NORMAL    4000  /* +4.00 */
#define FAITH_SIGNAL_GRAZE_LASER_HARD      3000  /* +3.00 */
#define FAITH_SIGNAL_GRAZE_LASER_LUNATIC   2000  /* +2.00 */
#define FAITH_SIGNAL_GRAZE_LASER_EXTRA     2000  /* +2.00 */

/* 每帧档位（一手 `th11:0x420a70` 的四档 + 本仓的「不动」档）。
 * 卡侧用 `signal_core_band()`（纯整数）从「自机离回收线多少 px」算出来。*/
enum {
    FAITH_SIGNAL_BAND_TOP = 0,      /* 线上 > 64 px：未满 +50 / 已满 +0.16 */
    FAITH_SIGNAL_BAND_MID = 1,      /* 线上 48…64 px：未满 +50 / 已满 +0.12 */
    FAITH_SIGNAL_BAND_LOW = 2,      /* 线上 0…48 px：未满 +50 / 已满 +0.09 */
    FAITH_SIGNAL_BAND_OFFLINE = 3,  /* 线下：−0.30 */
    FAITH_SIGNAL_BAND_HOLD = 4      /* 不推进（自机死亡 / 复活态：原版那两帧不看回收线；两参都 0）*/
};

/* ---- 擦弹倍率（§7.5）----
 * ★★ 2026-09-15 一手订正（`local/_scratch/research-th11-faith-and-points.md` §2.3，来源 `th11:0x41f4e0`/`0x41f4e7`）：
 *   原版是**整数阶梯**：`B = min(⌊graze/100⌋, 899)`，单位 0.01 ⇒ **每满 100 擦弹 +0.01**、上限 8.99。
 *   exe 里**没有** `0.0001f` / `8.99f` 两个浮点（那是扫错了靶子：真身是 ÷100 魔数与整数 `0x383`）。
 *   ⇒ 本仓一律用「以 100 擦弹为一步」的定点：步数上限 `FAITH_GRAZE_STEP_MAX`，
 *     千分点 = `min(⌊graze/100⌋, 899) × 10`（见 `mof_core_graze_mult()`）。*/
#define FAITH_GRAZE_STEP        100    /* 一步 = 100 擦弹（原版 ⌊graze/100⌋ 的分母）*/
#define FAITH_GRAZE_STEP_MAX    899    /* 步数上限 = 8.99（一手 `0x383`）⇒ 触发点 = 89900 擦弹 */
#define FAITH_GRAZE_MULT_MAX    (FAITH_GRAZE_STEP_MAX * 10)   /* 千分点上限 = 8990（**别用它当 graze 阈值**）*/

/* ---- 逐帧衰减的实现口径（★ 2026-09-15 订正）----
 * 上面那一节原来还有一组「每 15 / 30 帧结算一次」的整数分解常量（`FAITH_EMPTY_*_EVERY/STEP`），
 * 是为旧的 `faith_slot_drain_empty()` 准备的、且其中一半喂的是**没有一手出处**的「有敌人 −600/s」档。
 * 现按一手重做：速率是常量（下限 18 存储/帧）⇒ **每帧直接减，没有除不尽的问题**，那组常量**已删**；
 * 实现见 `mof_core.h` 的 `mof_core_faith_tick()` + `MOF_FAITH_DRAIN_NO_BOSS_FIXED`。*/

/* ---- 收集判定（照原生道具的几何：圆圈收 + 矩形吸引环给坐标）---- */
#define FAITH_COLLECT_R_DEFAULT   30.0f   /* player+0x4798c 的初值（引擎 `Player__reset`）*/
#define FAITH_ATTRACT_R_DEFAULT   70.0f   /* player+0x47990/94 的初值 */

/* ---- 绿点事件（绿点是从哪来的）---- */
typedef enum {
    FAITH_EV_BIG = 0,        /* 大信仰点 ⊕（+5000/8000/10000）· 大绿点（+100）*/
    FAITH_EV_GREEN,          /* 绿点收集（蓝点通道；连击 +100f）*/
    FAITH_EV_SMALL_P,        /* 小 P（连击 +60f）*/
    FAITH_EV_SMALL_FAITH,    /* 小信仰点（+100 / 连击 +60f）*/
    FAITH_EV_BIG_P,          /* 大 P（连击 +20f）*/
    FAITH_EV_KILL_NONBOSS,   /* 击杀非 boss（连击 +20f）*/
    FAITH_EV_MICRO,          /* 微信仰点 = 消弹每颗（+10 / 连击 +3f）*/
    FAITH_EV_KILL_BOSS,      /* 击杀 boss（连击 +0f）*/
    FAITH_EV_ONEUP,          /* 1UP（连击 +0f）*/
    FAITH_EV_COUNT
} faith_ev_t;

/* ---- 槽状态（纯数据；卡可以整块拷进自己的 ce_state）---- */
typedef struct {
    int32_t delta;      /* 累积增量，**原始单位 ×FAITH_FIXED 定点**（110：5000 → 500000）*/
    int32_t combo;      /* 连击槽 0..130（帧）*/
    int32_t signal;     /* 信号强度 ×1000（0..130000）；只有 111 会动它 */
    int32_t small_rem;  /* 111 的小绿点余数 0..99（**跨关保留**，§3.2.1）*/
    int32_t base;       /* 该槽的基础值，**原始单位**（110 = 50000 常数；111 = 按难度）*/
    int32_t delta_cap;  /* Δ 上限，**原始单位**（0 = 无上限）。111 = 原版基础，110 = 0（无上限）*/
} faith_slot_t;

/* 只读快照（给「落笔者」算 PV 用：落笔者**不读别的卡的状态**，只读自己与聚合值）。*/
typedef struct {
    int32_t faith_total;   /* 原始单位：基础 + Δ（110 侧这就是 MoF 的「信仰」）*/
    int32_t delta;
    int32_t base;
    int32_t combo;
} faith_view_t;

/* ---- 初始化 ---- */
/* 110 槽：基础恒 50000、Δ 从 0 起算、下限 0（= 「50000 扣不穿」）、无上限。*/
void faith_slot_init_110(faith_slot_t *s);
/* 111 槽：基础按难度、Δ 上限 = 原版基础（§7.2）。*/
void faith_slot_init_111(faith_slot_t *s, int difficulty);
void faith_slot_reset(faith_slot_t *s);          /* 清 delta/combo/signal，保留 base/cap */

int32_t faith_base_111(int difficulty);          /* 越界钳 LUNATIC */
int32_t faith_big_mof(int difficulty);           /* 大信仰点难度表：E/N 5000 · H 8000 · L/EX 10000 */

/* ---- 加值 ---- */
/* 落一颗绿点 / 信仰点到某个槽。delta 是**原始单位**（5000 传 5000）。返回真正加进去的量（原始单位）。*/
int32_t faith_slot_add(faith_slot_t *s, faith_ev_t ev, int32_t delta);
/* 定点版（微信仰点 / 小绿点这类 1/100 单位的通道用）。返回真正加进去的定点量。*/
int32_t faith_slot_add_fixed(faith_slot_t *s, int32_t delta_fixed);
/* 消弹累加器 → 110 的微信仰点（每颗 +10）与 111 的小绿点（每 100 个 +10，余数跨帧/跨关保留）。
 * n = 本次消掉的弹数（直接吃 `ce_cancel_radius()` 的返回值，§5.5 订正 3）。
 * 返回值写进 *out_110_add / *out_111_add，可为 NULL。
 * ⚠ **单位是定点（×100），不是原始单位** —— 交出去的是 `faith_slot_add()` 的返回值（定点量），
 *   与 `faith_slot_add()` 的「原始单位」不同（2026-09-12 订正：原来这句写成「原始单位」是错的）。
 *   调用方若只要「这一步有没有进位」，判 `> 0` 即可；要原始单位就 `/ FAITH_FIXED`。*/
void faith_slot_add_cancel(faith_slot_t *s110, faith_slot_t *s111, int32_t n,
                           int32_t *out_110_add, int32_t *out_111_add);

/* ---- 减益 ---- */
#define FAITH_REDUCE_NONE    0
#define FAITH_REDUCE_MISS    1   /* miss −(信仰−50000)/3；对 Δ 而言 = −(Δ/3)（向下取整）—— **110 侧** */
#define FAITH_REDUCE_BOMB    2   /* 非决死灵击 −3000 —— **110 侧** */
#define FAITH_REDUCE_AVOID   3   /* 全避 boss 非符 / 符卡 −30000 —— **110 侧** */
/* 施加一条减益到槽；返回真正的扣减量（正数 = 扣掉多少，原始单位）。Δ 夹在 [0, ∞)。*/
int32_t faith_slot_reduce(faith_slot_t *s, int kind);
/* 扣一笔**指定量**（原始单位；Δ 夹在 [0, ∞)）。返回真正扣掉的量。
 * 111 的 miss 用它（`FAITH_SA_MISS_PIV_LOSS`）—— 与 110 的三档名义值不同，故不塞进 `faith_slot_reduce` 的 kind 表。*/
int32_t faith_slot_sub(faith_slot_t *s, int32_t delta);
/* ★ 2026-09-15 **删除** `faith_slot_drain_empty(s, frames, enemies_present)`（原「❓ 自定」）：
 *   · 它的判据是「槽空 **且场上有敌人**」（−10800/s / −600/s 两档）——一手**两档都不对**：
 *     真值是「槽空 ∧ **无 Boss** ∧ 无对话 ∧ 关卡帧 ≥ 90」下的**固定**速率 −180 显示/帧；
 *     「场上有敌人」根本不是判据（道中一堆妖精在飞时照样满速衰减），而「有敌人 −600/s」那一档
 *     在一手里**完全没有对应物**；
 *   · 一手实现在 **110 专属**（TH11 侧连衰减带连击槽都没有） ⇒ 它没有第二个可能的调用者；
 *   · 「没有敌人时那份 10800 恰好是一手值」只说明**速率数值**对上了，不是机制对上 ——
 *     按 METHOD 的规矩，机制不对就不能包装成复刻，所以整条删掉（不留向后兼容壳）。
 *   ⇒ 一手速率的**唯一出处** = `FAITH_EMPTY_NO_ENEMY_PER_SEC`；实现 = `mof_core.h` 的
 *     `mof_core_faith_tick()`（门 + 槽 + 衰减，含「门不成立则槽一起冻结」）。*/
/* ❓ 「连击槽空」在本题里不再是独立前置条件（它就在 `mof_core_faith_tick()` 的分叉里）——
 *   这里只留「槽空了吗」这个只读判断，供日志/诊断用。*/
int     faith_combo_empty(const faith_slot_t *s);

/* ---- 连击槽（**只服务 110 / th10**；111 侧一律不调，见上面「连击条」块的说明）---- */
int32_t faith_combo_value(faith_ev_t ev);         /* 该事件的 +f 值（§7.5 的表）*/
int32_t faith_combo_add(faith_slot_t *s, faith_ev_t ev);   /* 加到满槽 130 为止；返回真正加了多少 f */
int32_t faith_combo_set(faith_slot_t *s, int32_t frames);
int     faith_combo_full(const faith_slot_t *s);  /* == 130 */

/* ---- 信号强度（111）---- */
/* 本帧信号增量（千分点）。`band` = 上面那组 `FAITH_SIGNAL_BAND_*`（卡侧用 `signal_core_band()` 从
 * 「自机离回收线多少 px」算出来；原版读的是玩家绝对 y，我们按相对偏移表达 —— 见常量块的 🟡）。
 * `graze_gain` = 本帧擦弹带来的增量（**已按难度查表**后的总量，0 = 本帧没擦弹；查表在
 * `signal_core_graze_gain()`）。返回可正可负；调用方用 `faith_signal_add()` 落值。
 * ⚠ 「线上未满 ⇒ +50/帧」在原版是「**补满并钳到 100.00**」⇒ 这里返回的增量会**被夹到刚好补满**
 *   （`FAITH_SIGNAL_HI_MARK - signal_now`），不是无条件 +50000。*/
int32_t faith_signal_step(int32_t signal_now, int band, int32_t graze_gain);
/* ★★ 2026-09-16za：带**「线上补满」闸**的版本（106 红魔乡的协同效应，用户口径逐字：
 *   「**协同效应：除非满P，不加信号强度**」，且用户选定**只停「线上补满」那一条**）。
 *   `allow_line_topup == 0` ⇒ 未满的线上档**不给** `FAITH_SIGNAL_GAIN_LINE`（那一支的 d 变 0）；
 *   其余**一字不改**：擦弹是独立通道（照给）、已满档的 +0.16/+0.12/+0.09 照给、线下的 −0.30 衰减照给、
 *   `HOLD` 档照旧 0。
 *   一手背景（TH06 `ItemManager.cpp`）：满 P（128）才是过线收点的充要条件 ⇒ 「站在线上把信号槽补满」
 *   这件事在不满 P 时本来就不该发生（点收不到，槽自然也不该被线上补满）。
 *   `faith_signal_step(...)` == `faith_signal_step_gated(..., 1)`（旧调用方与既有单测零变化）。*/
int32_t faith_signal_step_gated(int32_t signal_now, int band, int32_t graze_gain, int allow_line_topup);
void    faith_signal_add(faith_slot_t *s, int32_t d);   /* 夹 [0, FAITH_SIGNAL_MAX] */
void    faith_signal_reset(faith_slot_t *s);            /* 被弹 / 关卡结束 / 一局开始归 0 */

/* ---- 单位换算 / 快照 ---- */
int32_t faith_slot_total(const faith_slot_t *s);          /* 基础 + Δ（原始单位）*/
int32_t faith_score_unit(const faith_slot_t *s);          /* 出槽：原始单位（= total，定点 /100 后）*/
void    faith_slot_view(const faith_slot_t *s, faith_view_t *out);

#endif /* FAITH_CORE_H */
