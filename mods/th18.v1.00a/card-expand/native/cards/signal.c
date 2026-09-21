/* 地灵殿（TH11，card-expand id 111）—— **装备卡**：信号槽 + 绿点信仰 + 蓝点分 + 灵击 + 命碎/残机。
 *
 * ## 设计依据（**先读这几篇，本文注释只指路、不重复推理**）
 *   · ★★ **一手（2026-09-15，全部换到 th11 v1.00a 逐条反汇编）**：
 *     `local/_scratch/research-th11-signal.md`（通信强度槽：载体/量纲/每帧四档/擦弹按难度查表/
 *       归零/满槽全收/HUD 两层）
 *     `local/_scratch/research-th11-faith-and-points.md`（信仰初值上限 / 绿点三档 / **蓝点分整数式** /
 *       交信度 / 擦弹倍率的整数阶梯 / 信仰写入点穷举）
 *     `local/_scratch/research-th11-bomb-lifefrag-hud.md`（灵击伤害·帧数·判定框 / 命碎 / HUD 信号格配方；
 *       ⚠ 它的 §4 命碎地址由本文的一手复核**订正**，见 `signal_core.h` 的说明块）
 *   · 设计稿（**非一手**，与一手冲突处以一手为准）：`docs/superpowers/specs/2026-09-10-sa-signal-design.md`、
 *     `…2026-09-11-system-card-spec-review.md`
 *   · `mods/th18.v1.00a/card-expand/EVENTS-CONTRACT.md` §3（计分层：声明 → 聚合 → 合成）
 *   · 实装期新结论与偏差：`docs/superpowers/specs/2026-09-12-signal-111-impl.md`
 *
 * ## 分工：本文件只是「一具身体」，逻辑都在共用件里（**一行都没重写**）
 *
 *   | 件 | 在哪 | 管什么 |
 *   | --- | --- | --- |
 *   | 绿点实体池（110/111 **共用**） | `faith.c` | 撒点、飞散 / 飘落 / 吸附、收集判定、按 `slot_mask` 回调 |
 *   | 绿点记账（按槽） | `faith_core.c` | `Δ_111` 加值 / 上限 = 基础 / 小绿点余数 / 信号落值夹取 |
 *   | 灵击门槛与扣法（110/111 **共用**） | `mof_core.c` | `mof_core_power_spend()`（≥2.00 档、扣 1 档、下限 1.05） |
 *   | ★ 魔理沙(1) 的爱丽丝变体（灵击 = 别人的一半） | `signal_core.c` | `signal_core_alice_power_spend()`（**比例 ✅一手 / 档位 ❓映射**；不生效时原样转发上面那一行）|
 *   | ★ 爱丽丝的 **P 点平均 +0.025 档**（两张卡同一份策略） | `faith_core.c` + `faith.c` | `faith_core_p_topup_total()`（纯逻辑）/ `faith_p_topup_take()`（判据 + **唯一**相位）⇒ 补值走 `mof_core_p_topup_units()` |
 *   | 擦弹倍率（**111 用**；110 单装时不参与 PV）| `mof_core.c` | `mof_core_graze_mult()` = `min(⌊graze/100⌋,899)×10` 千分点 |
 *   | PV 总式（110/111 共用，含三态） | `mof_core.c` | `mof_core_pv()` = 计分层契约 §3 的那条旧式（**不参与落笔**） |
 *   | 信号槽的 111 私有逻辑 | `signal_core.c` | 擦弹查表、回收线档位、蓝点分整数式、miss、命碎、条宽 |
 *   | **本文件** | — | 读引擎、调上面几层、`ce_score_declare_coupled` / HUD / 灵击 / 得分奖残 |
 *
 * ## 本卡在计分层里的角色（★ 2026-09-13 重做；见 `../SCORE-LAYER-REWORK.md`）
 *
 * **没有「落笔者」了**：111 只声明两样东西 —— 自己的 `G_11 = 基础_111 + Δ_111`（耦合族）
 * 与自己的**信号强度**（千分点）。层合成
 *   `G = G_0 + Σ未耦合 + (擦弹倍率 + 信号强度) × Σ耦合 / 1000`，
 * ⇒ 与一手 `信仰显示 × (交信度 + 擦弹倍率)` 同构（一手的 `A`/`B` 单位 0.01、千分点刻度一致）。
 * ⚠ `mof_core_pv()` / `signal_core_pv_111()` 仍是纯逻辑单测的回归面，但**都不再参与落笔**。
 *
 * ## 明确不做（spec §6 + 一手复核的取舍）
 *   · ★★ **2026-09-15e 订正**：原来这里写着「不让 111 自己发命碎 —— TH11 的『捡命碎道具』一路在 TH18 里
 *     由**原生命中碎制**承担（item type 4 → `0x457570`）⇒ 我们只观测、不重发」。**这条不成立**，两处错：
 *       ① **载体错了**：命碎道具是 item type **`0x10`**（EXTEND2 卡道具 → `CardLifeFragment` id 3），
 *          不是 type 4（type 4 连 TH18 的道具分派 switch 里都没有）。一手三条见 `signal_core.h` 的
 *          「命碎的载体」段（表 `0x4b4020` / `collect2x_core.h:55` / 本轮 9 个 `0x446f40` 调用点穷举）。
 *       ② **「原生已经在发」也是错的**：把 TH18 全部 21 个 `st*.ecl` 解出来扫掉落指令（本轮自己跑
 *          `thecl -d 18 -x`），**`dropMain(0x10)` 全游戏只有一处** —— `st04mbs.ecl` 的 `MBossEscape()`
 *          （`0x15a4`），而 **8 个 `st0Xbs.ecl`（关底 boss）里一次都没有**。
 *      ⇒ **撒命碎道具这件事由本卡负责**（用户口径：每 2 段「没死过」的攻击段撒 1 颗）。
 *        判据 / 粒度 / 近似全部写在 `signal_core.h` 的「命碎段机」段，卡侧只做
 *        「读引擎信号 → 调 `signal_seg_tick()` → 撒道具」。**卡不碰碎片、不碰残机**：
 *        吃到之后走原生链（`0x457570` 累加，阈值 3 个 = 1 残，本轮 read 复核 = 全 3）。
 *   · **得分跨阈值那一路** TH18 没有 ⇒ 由本卡实现（`ce_add_life()`，一手 `th11:0x41f950` 段）。
 *   · ★★ **2026-09-15c 新增：「灵击 → 决死」（决死 B 形态）** —— 用户口径（逐字）
 *     「**基本语义上灵击==B**」「**所以灵击也得决死**」⇒ 库存 == 0 且自机在**决死窗口**里
 *     （`player+0x476ac == 4`，一手 `th18:0x45d4e4`）时按 B ⇒ 本卡灵击以**决死形态**发动：
 *     照常扣 1 档火力 / 消弹 / 伤害源 + 另调 `Player__cancel_impending_death` **`th18:0x45d590`** 救命。
 *     三条口径（消弹**照常记账** ✅一手 / 不扣信仰 = 结构上不会发生 / 不开荷取退款窗 ❓自定）与**全部一手依据**
 *     写在 `signal_core.h` 的「灵击 → 决死」块 + `signal_bomb()` 的注释里；🟡 项见 `TUNING.md`。
 *   · 写 `MONEY`（蓝点分只加分数、不加钱）；
 *   · 伙伴主动技能 / 子机层（spec §9.1：子机位置走「低速子机声明层」，属另一批活）。
 *     ⚠ **2026-09-12 更新（108 落地的声明层，别再走老路）**：本卡今天**没有任何子机代码**
 *     （全文件不写 `option+0x64/+0x68/+0x6c/+0x70`；唯一相关调用是 `signal_bomb()` 里的
 *     `ce_repopulate_options()` —— 那是「扣了 1 档火力 ⇒ 池按新档重建」的原生要求，不是子机阵型写入
 *     ⇒ 本次「111 迁移」**没有可迁移的直写**）。将来做 §9.1 那个子机层时**只许声明**：
 *       `ce_option_pose_declare(c, &pose)`（每帧，逐格偏移）+ `on_option_pose_apply` 落笔
 *       （spec §3.3.1 / sdk.h 那一段）；**108(108) < 111(111)** ⇒ 同装时 108 落笔，
 *       本卡会在「退位通知」里被叫一次（此时 `ce_option_pose_winner() != 111`）⇒ 只收子机、不写字段。
 *       ⚠ 重建池之后本卡的槽指针会作废（`option+0x00` 被清）⇒ 落笔里用 `ce_option_get` 重新认领即可
 *       （门式：每帧重写偏移，不依赖任何「池刚重建」的通知）。
 *
 * ## 长持续效果的自查（`METHOD.md` 铁律二）
 * 本卡跨帧留下的东西：① 绿点 VM 池（共用件，静态区）；② HUD 槽位认领；③ 3 个 UI 层 VM。
 * **`on_delete` 与 `on_run_reset` 都走同一个 `signal_clear()`**：收池 + 退声明 + 收 VM + 释放槽位。
 * 删卡发生在**商店**里、那一刻 AbilityManager 冻住 ⇒ 只能靠 `on_delete`（虚表 `+0x50`），不能等下一帧。
 * ⚠ 共用池的收法（2026-09-12 审计改）：`faith_clear_entities()` 会清**整个**池（110 的绿点也在里面）
 *   ⇒ 改成 `faith_clear_entities_mask(1 << FAITH_SLOT_111)`（只收本卡槽里的点）。
 * ★ `on_stage_start`（`+0x34`，每关开场）**只归零信号槽**（一手 `th11:0x431097`：玩家登场 60 帧每帧写 0
 *   —— 而玩家对象随场景重建 ⇒ 每关一次）；不动信仰、不动 VM、不动槽位。
 */
#include <stddef.h>          /* sdk.h 的 ce_item_collect_all 用了 size_t（内置 Linux 头不带）*/
#include <string.h>
#include <stdio.h>
#include "sdk.h"
#include "faith.h"
#include "faith_hud.h"        /* ★★ 2026-09-15：110/111 共用的「组合 UI」（共享件）*/
#include "signal_core.h"
#include "sht_ids.h"         /* ★ 生成物：111 五组 formation shooterset 的槽号（0x29–0x2d，本轮新增）。
                              * ⚠ 只在**卡侧** include（`signal_core.c` 保持纯逻辑、不 include 它，见那里的注释）。*/
#include "mof_core.h"        /* ★ 共用件：借 `mof_core_power_spend`（灵击）与 `mof_core_graze_mult`（擦弹倍率），
                              * **不改它**。⚠ 它的 `mof_core_pv()` 仍是旧乘式（没有一手的 trunc10 与下限 10，
                              * 见 `signal_core.h` 的 PV 说明）⇒ 本卡不用它算 PV。*/

/* ---- 本卡常量 ---- */
#define SIGNAL_ID_NUM          111      /* ⚠ `CE_CARD` 对 ID 做 ## 拼接 ⇒ 必须给**字面量**,
                                         * 不能写 `CE_CARD(SIGNAL_ID_NUM, …)`（会拼出 ce_hooks_SIGNAL_ID_NUM）*/

/* ★★★ 2026-09-16r **跨关账本**（用户实跑：「切换面数的时候最大得点会被清空」→「**是 Delta 变 0 了**」）。
 * 一手口径 + 本仓先例 = 卡 113 的同一条 bug / 同一条修法（`AUDIT.md` §DK-2）：**PIV / 最大得点在一局里一直涨、
 * 跨关不清**，只有「一局开始」才归零。机制：`on_run_reset` / 卡对象重建在**关卡切换**时也会跑，
 * `signal_clear()` 的 `memset` 会把 Δ / 连击 / 信号一起抹掉 ⇒ 账本搬进文件静态、`ctor` 恢复它、
 * 每帧（HUD 声明那一处）同步它，只有**关卡号变小**（回第 1 关 / 重开一局 / 练习重选关）才重新初始化。*/
static faith_slot_t s_slot_keep111;
static int32_t      s_slot_stage111 = -1;
/* engine.h 没给这两个宏 ⇒ 就地补（**只读**，与 `mof.c` / `lolk.c` / `season.c` 同址同口径）。*/
#define SIGNAL_ADDR_GRAZE      0x4ccd0c   /* 原生擦弹计数（GLOBALS.inner+0x30）*/
#define SIGNAL_ADDR_DIFFICULTY 0x4ccd00   /* 难度（0 EASY..3 LUNATIC、4 EXTRA）*/
/* `CE_ADDR_MAX_POWER` 0x4ccd3c = GlobalsInner+0x60（engine.h 一手；`mof.c` 的 `CE_MAX_POWER_VAL()` 同址）。*/
#define SIGNAL_MAX_POWER_VAL() (*(int32_t *)CE_ADDR_MAX_POWER)

/* ---- 绿点的**三档来源**（§2.2 / §7.3 的表；实体层共用 `faith.c`，这里只给值与自己那一槽）----
 * ★ 2026-09-12 审计补：原来本卡**只有「小绿点（每 100 个消弹 +10）」这一条**通道 ——
 *   全卡没有任何 `faith_spawn()` 调用 ⇒ 撒出来的点永远不带 `1 << FAITH_SLOT_111`
 *   ⇒ `signal_on_pickup()` 是个**永不触发的死回调**。下面两条钩子就是补齐缺口：
 *     · 大绿点（+100 = `SIGNAL_GREEN_BIG`）= **击破掉落**（判据与 110 的大信仰点共用
 *       `mof_core_wants_big_*` —— §7.1 定案「绿点就是同一个东西，只是数值表不同」；**boss 不掉**，§7.3）；
 *     · 中绿点（+10 = `SIGNAL_GREEN_MID`）= ★ **满 P 转换**（E-1 的 type 9 档）：
 *       主路 = 共用件 `faith_convert_full_p_items()`（`on_tick_2` 每帧调，满 P 时把原生 P 类道具
 *       **原位删掉** + 原位撒点；一手 `th11:0x4240f0`）；`on_item_collect` 那条只剩**同帧竞态网**
 *       （判据 `mof_core_full_p_replace()`：满 P 且 type ∈ {小 P(1), 大 P(3)}）；
 *     · 小绿点（每 100 个 +10）= 消弹通道（`signal_on_cancel`，**不建实体**）。
 * ⚠ `value` 传的是**本卡的值**，且 `slot_mask` 必须是 `1 << FAITH_SLOT_111`；**不要**用
 *   `faith_spawn_big()` —— 那是 110 的便捷包装（写死 110 的槽与值）。*/

/* ---- 灵击（★ 2026-09-15 一手对齐：伤害 / 帧数 / 判定框全部换成 th11 原值）------------------
 * 触发（spec §2.4「B 双选」）：X 键上升沿 + 炸弹库存 == 0 ⇒ 本卡的灵击（库存 > 0 交给原生）。
 *
 * **一手（研究文档 §2，`th11:0x433a90` = 伤害源原语 `(pos, 每帧伤害, 存活帧数, 半宽, 半高)`）**：
 *
 *   | Bomb | 每帧伤害 | 存活帧数 | 半宽 × 半高 | 单目标吃满 | 一手调用点 |
 *   | --- | --- | --- | --- | --- | --- |
 *   | 灵梦A | 32.0 | **30** | **10 × 50** | 960 | `th11:0x45dec0` |
 *   | 灵梦B/C・魔理沙B/C | 32.0 | **75** | **20 × 30** | 2400 | `th11:0x407923` |
 *   | 被弹（复仇）bomb | 32.0 | **150** | **30 × 30** | 4800 | `th11:0x4314dd` |
 *   | 魔理沙A | ——（不走伤害源：弹幕 + 碰撞回调返回 50 / 特例 8）| | | | `th11:0x406ebf` |
 *
 *   · 一手常量：**每帧伤害 32.0f**（`th11:0x497ef0`，4 个调用点共用）。
 *   · **六套里 4 套同值**（灵梦B/C + 魔理沙B/C）⇒ 本卡取**众数那一套 75f / 20×30**（TH18 侧认不出
 *     TH11 的机体；另两套的数值也具名列出，见下 —— 想换一行就够）。
 *   · ❌ **不是全屏**：判定框是 20×100 / 40×60 / 60×60 px 的**小矩形**（研究文档 §2.5），
 *     占位 `300×2 帧` 把伤害放大了 ~9 倍、帧数压到 1/15 ⇒ 本轮的订正就是把这两个数换掉。
 *   · **震屏：一手否定（本卡已删）** —— 研究文档 §3 穷举了 4 条 bomb 路径（① `call 0x433a90` 的 4 个
 *     调用点 ② `0x438440` 的弹窗 ③ 相机/屏幕偏移字段 `SUPERVISOR+0x434`/`ANM_MANAGER+0xb0/b4` 的 xref
 *     ④ 全 exe float 扫描找 `(20,12,0)` 三参常量对）**都没有震屏**；同族物只有**关卡演出旋转**
 *     （`th11:0x42aa2d` 的 `1800.0f`/`30.0f`）。⇒ 原来的 `ce_screen_shake(20, 12, 0)` 无一手出处，
 *     **已删**（`ce_screen_shake` 本身是 SDK 的共享原语，110 / 炎魔 / 妖妖梦还在用 ⇒ 不动共享件）。
 *   · 清弹（消弹圈）：原版每个 bomb tick 调 `th11:0x424fe0`，半径取自 **bomb 自己的字段**：
 *     灵梦A 初值 = 32.0f（`th11:0x45dd70: fld ds:0x497ef0; … fstp [edi+0x490]`）、每帧
 *     `[bomb+0x490] += [bomb+0x494]`（`th11:0x45ded6`）⇒ **是个随时间长大的圈**，不是全屏。
 *     我们是一次性灵击（没有逐帧 bomb 主体）⇒ 仍用**全屏量级**（❓ 自定，登记 `TUNING.md`）。
 *
 * 门槛 / 消耗（★ 用户已拍板的 TH18 侧映射，**不是复刻**）：
 *   · 本卡用共用件的 `FAITH_POWER_MIN_LEVEL`（≥2.00 档）/ `FAITH_POWER_COST`（扣 1 档）/
 *     `FAITH_POWER_FLOOR`（下限 1.05 档）—— 依据 = TH18 火力下限 1.00 ↔ 原作 0.00 的换算 + 用户
 *     「加一个サービス」（spec-review §3.2.1）。
 *   · ⚠ **一手差异（务必写在这里，别当成复刻）**：TH11 原版门槛 = `CURRENT_POWER / BOMB_COST != 0`
 *     （`th11:0x4311d3` 的 `idiv ds:0x4a574c`），消耗 = `CURRENT_POWER -= BOMB_COST`（`th11:0x4311f1`），
 *     **代码里没有任何下限钳位**（可花到 0.00）；而 `BOMB_COST` 是 per-shot 字段（`pl*.sht` 记录 `+0x24`）：
 *     **5 个自机 = 20（= 1.00 火力）、魔理沙A = 12（= 0.60 火力）**（研究文档 §1.2 的表）。
 *     ⇒ 「魔理沙A 只扣 0.60」「无下限」这两条**我们没有复刻**（TH18 侧没有 per-shot 的 bomb 消耗表）。*/
#define SIGNAL_INPUT_BOMB      0x002u     /* `CE_ADDR_INPUT_PRESSED` bit 1 = X（炸弹）*/
#define SIGNAL_BOMB_CANCEL_R   4000.0f    /* ❓ 自定：消弹半径（一手是增长圈，初值 32.0f —— 见上）*/
#define SIGNAL_BOMB_CANCEL_MAX 99999      /* 数量上限（不限）*/
/* ★★★ 2026-09-16m **灵击 = 110 的灵击**（用户口径，逐字、说了两遍）：
 *   「**我才发现 th11 的灵击每个机体都完全不一样** ⇒ **都改成 th10 的样式吧，装 th11 的时候也是 th10 灵击**」
 *   「**明确：除了魔理沙河童特技的额外效果，其他的灵击就 == 110 灵击 即可**」
 *   ⇒ 本卡不再自己发明灵击：**半径曲线 / 逐帧消弹圈 / 每敌每帧伤害 / 时长 / 消耗 / 音效 / 视觉**
 *     全部与卡 110（`mof.c` 的 `mof_bomb_start()` + `mof_bomb_tick()`）**逐条同形**；
 *     纯数学那一半**直接调 110 的共用件** `mof_core_*`（`mof_core.h` 本来就被本卡 include）。
 *   ⇒ **唯一保留的额外效果** = 魔理沙×荷取（河童）的 **河童罩子**（`signal_shield_*`：
 *     水泡贴图 + 「没被撞 ⇒ 退半档火力」；★★★★ 2026-09-16q 按一手重写，见下面那一段）。
 *   ⚠ 被这条口径**取代**的旧口径：爱丽丝（魔理沙×爱丽丝）的「灵击 = 0.5 档」。
 *     `signal_core_alice_power_spend()` 仍留在 `signal_core.c`（主机单测还在），但**本卡不再调它**。
 *   ⚠ 与 `mof.c` 的关系：**逐帧状态机这里是第二份**（卡之间不互相 include）⇒ 改一边必须同步另一边，
 *     `TUNING.md` §2.5 记着这件事。常量一律取 `mof_core.h` 里的共用值（不另抄一份）。*/
#define SIGNAL_BOMB10_SE            0x2c   /* = 110 的 `MOF_BOMB_SE`（`se_slash.wav`；一手 `th10:0x405942`）*/
#define SIGNAL_BOMB10_DMG_SRC_LIFE  2      /* = 110 的 `MOF_BOMB_DMG_SRC_LIFE`（每帧一个新源 ⇒ 每帧一笔）*/
#define SIGNAL_BOMB10_RANK_ADDR     0x4ccd08  /* = 110 的 `MOF_RANK_ADDR`（TH18 里是死字段，照抄写法）*/
#define SIGNAL_BOMB10_RANK_DROP     0x80
#define SIGNAL_BOMB10_RANK_MIN      (-0x400)
#define SIGNAL_BOMB10_RANK_MAX      0x400
#define SIGNAL_BOMB10_FIELD_W       384.0f /* = 110 的 `MOF_PLAYFIELD_W`（游玩区，世界坐标 −192..192）*/
#define SIGNAL_BOMB10_FIELD_H       448.0f
#define SIGNAL_BOMB10_FIELD_CX      0.0f
#define SIGNAL_BOMB10_FIELD_CY      224.0f
#define SIGNAL_BOMB10_SPARK_EVERY   6      /* 火星出生节流（❓ 自定，与 110 的 `MOF_BOMB_SPARK_EVERY` 同值）*/

/* 本发灵击的跨帧状态。**放在文件静态里**（不是 `signal_card_t`）—— 卡的 `ce_state` 有 **496 字节硬上限**，
 * 而这张卡的状态已经接近上限（同款做法 = 本文件的 `s_opt[]` / `s_tint_*`；卡是单例，安全）。*/
typedef struct {
    int32_t  left;          /* 还剩几帧（0 = 没在灵击）*/
    int32_t  elapsed;       /* 已经过几帧（半径曲线的自变量）*/
    float    x, y;          /* 消弹/伤害中心（x = 发动瞬间的自机 x，y 每帧上移）*/
    int32_t  target_centi;  /* 本发的目标半径（1/100 px）：灵梦 16000 / 其他 11200 */
    int32_t  rise_milli;    /* 每帧上移量（1/1000 px）*/
    int32_t  reimu;         /* 发动瞬间的 `CHARACTER == 0`（伤害表的圈外档分灵梦/魔理沙）*/
    int32_t  frames;        /* 诊断：本发实际走了几帧 */
    int32_t  dmg_frames;    /* 诊断：本发实际撒出伤害源的帧数 */
    int32_t  spell_frames;  /* 符卡已过帧数（对位 110 的 `s->spell_frames`；≥ 60 = `big_bomb` 那一档）*/
    uint32_t ring_vm;       /* 范围环的 VM id（脚本 279；0 = 没起）*/
    int16_t  ring_frames;   /* 环已驱动了几帧 */
    int8_t   ring_spark;    /* 火星节流计数 */
    int8_t   ring_fading;   /* 环已发过淡出中断 */
    int32_t  faith_dots;    /* ★ 2026-09-16p 诊断：本发一共撒出几颗信仰点（与 110 同口径）*/
    uint32_t strip_vm;      /* ★ 2026-09-16p：**电流边**竖条环（脚本 293，与 110 同一份）*/
} signal_bomb10_t;

static signal_bomb10_t s_b10;
/* 一手伤害源三元组（复刻：`th11:0x433a90` 的调用点；见上面那张表）。默认取 4/6 机体的众数那一套。*/
#define SIGNAL_BOMB_DMG         32        /* 每帧伤害（一手 `th11:0x497ef0` = 32.0f；原占位 300）*/
#define SIGNAL_BOMB_DMG_LIFE    75        /* 存活帧数（一手 `push 0x4b`；原占位 2）*/
#define SIGNAL_BOMB_HALF_W      20        /* 半宽（一手 `0x4984c8` = 20.0f）*/
#define SIGNAL_BOMB_HALF_H      30        /* 半高（一手 `push 0x1e`）*/
/* 另两套（具名备用；换机体只要改上面四行 + 把这里注释掉的口径搬过来）：
 *   灵梦A：`frames 30`（`push 0x1e`）、`半宽 10 × 半高 50`（`0x49822c` = 10.0f、`push 0x32`）；
 *   被弹复仇（决死）：`frames 150`（`push 0x96`）、`半宽 30 × 半高 30`（`0x497ff0` = 30.0f、`push 0x1e`）。*/

/* ---- ★★ 2026-09-15c：灵击 → **决死**（「决死 B 形态」）的救命调用 ----
 * `Player__cancel_impending_death` **`th18:0x45d590`**：thiscall(**zPlayer***)、**无栈参**、裸 `ret`。
 * 本轮**自己在样本上复核**（`objdump -d --start-address=0x45d590 th18.exe`）：体内 6 个写 ——
 * `+0x644 |= 1` / `+0x640 = 0` / `+0x638 = 0x3c` / `+0x63c = 60.0f` / `+0x634 = 0x3b` /
 * **`+0x476ac = 1`**（状态回存活）。
 * 与 110（`cards/mof.c`）各持一份：两卡的账本/数值表分家（`EVENTS-CONTRACT.md` §7），
 * 连那条 🟡 的依据都不同（111 侧等 TH11 一手，见 `signal_core.h`）。
 * ⚠ 一手对照：原生自己的「决死 B」= `th18:0x45c2a9` 那一支 `can_bomb 0x420420` → `do_bomb 0x420360`
 *   → **`0x45c2d7 call 0x45d590`**；库存 == 0 时 `can_bomb` 返回 0 ⇒ 原生整条不走 —— 本卡补的就是这个缺口。*/
#define SIGNAL_FN_CANCEL_IMPENDING_DEATH 0x45d590u

/* 取消本次死亡（形状照 `cards/senro.c` 的 `senro_cancel_impending_death()`）。*/
static void signal_cancel_impending_death(void)
{
    void *p = CE_PLAYER();
    if (p) ((void (__attribute__((thiscall)) *)(void *))(uintptr_t)SIGNAL_FN_CANCEL_IMPENDING_DEATH)(p);
}

/* ---- HUD（**原版 TH11 信号槽那一块**；几何逐条给一手出处）--------------------------------
 * ★ 2026-09-12 本轮（审计 A3 的 111 部分）：**原来的 5 个 `drawRect` 格子整段删掉**，
 *   改成 TH11 `front.anm` script32..35 那 **4 个 VM** 里的 3 个（第 4 个原版自己把它写成零宽）。
 *
 * ★ **2026-09-15 逐字段核对**（依据：`research-th11-bomb-lifefrag-hud.md` §5.1/§5.2 + 新研究
 *   `research-th11-signal.md` §6.1；「一手」= 逐条指令 / anm 原文）：
 *
 *   | 字段 | 一手 | 本卡 | 裁决 |
 *   | --- | --- | --- | --- |
 *   | 槽底 | `sprite57`、`pos(42,456)`、`anchor(1,2)`、`layer 19` | script **212**（切图 sprite57，TH11 script32 逐条搬）@ `(42,456)` | ✅ |
 *   | 增长条 | `sprite58`、同 pos、`layer 18` | script **213**（切图 sprite58，TH11 script33 逐条搬）@ 同 pos | ✅ |
 *   | 图标 | `sprite26`、`pos(30,468)`、`anchor(1,2)`、`layer 18` | script **214**（切图 sprite26，TH11 script35 逐条搬）@ `(30,468)` | ✅ |
 *   | 条宽 | **`9.0 + 4.0 × level`**，`level = min(⌊槽/100⌋,100)/10`（`th11:0x41b679`–`0x41b711`）| `signal_core_bar_width_px()` | ✅ |
 *   | 档位中断 | 100/110/120 → 写 `VM+0x37c`（label/cmd）7/8/9/10；**只在档变时发**；`Gui+0x43cc < 0x14` 前 20 帧不发 | `signal_gauge_level_int()` + `SIGNAL_WARMUP_FRAMES` | ✅ |
 *   | 淡回 / 压暗 | label **2** / **3**（`alphaTime(30,0,255)` / `alphaTime(10,0,64)`）| `SIGNAL_INT_SHOW` / `SIGNAL_INT_DIM` | ✅ |
 *   | 第 4 个 VM（script34/sprite59）| 引擎每帧把它的宽度写 0（`th11:0x41b71d: fldz; fst [ebx+0x3700]`）| 不建 | ✅ |
 *
 *   · 槽底与图标的位置**相对块左下角**（= 原生图标的左下角 (30,468)）：
 *     图标 (0,0)、槽 (+12,+12)；块的**外沿** = 640 空间 `x 30..90`、`y 440..468` = **60×28**。
 *   · **增长条**：`level = min(⌊显示值⌋,100)/10 ∈ 0..10` ⇒ 每 +10 显示点亮一根 4 px
 *     （贴图内容宽 48 px ⇒ 卡侧写成 `scale.x = 宽/48`，左端因 `anchor(1,2)` 钉住）。
 *   · **档位闪色**（原版 `th11:0x41b5a0`–`0x41b673` 写 `VM+0x37c`）：`pct<100 → 7`（常态白）、
 *     `<110 → 8`（5 帧闪）、`<120 → 9`（3 帧闪）、`≥120 → 10`（2 帧闪）。**只在档位变化时发**
 *     （原生那边也是靠 `gui+0x4420` 的 bit6..8 做边沿）。
 *   · **整块压暗**（原版 `th11:0x41b4a1`–`0x41b55a`）：显示态遇到「自机 `y > 416` 且 `x < −64`」⇒
 *     发中断 3（`alphaTime(10,0,64)`）；压暗态遇到「`y < 400` 或 `x > −64`」⇒ 发中断 2（淡回）。
 *     ⚠ 这套门限是 **TH11 自己的**（`416.0f`@`0x498010`、`400.0f`@`0x4982d8`、`−64.0f`@`0x4982e0`），
 *     与 SDK 的 `ce_hud_near_corner()`（TH16 栅格 400/384）**不是同一组** ⇒ 本卡自算（共享件不动）。
 *   · ❓ **文字读数三行是我们自己加的**（原生 SA 的读数就是那排竖条，没有数字）—— 商品化成
 *     「信号 0.00–1.30 / 信仰 / 蓝点分预览」，摆在原生那一块的右边（块因此宽到 104，640 空间）。
 *     这一条从 2026-09-10 起就在 `TUNING.md` §2 的 111 行登记为 ❓ 自定。
 *   · ⚠ **「5 格手机信号」那套（`SIGNAL_CELLS` / `drawRect` 拼格）已删**：一手是
 *     「**4 档图标 + 一条连续填充条**」（`research-th11-signal.md` §6.1 与 §0 的第 7 条），
 *     不是 5 个离散格子 ⇒ 旧口径作废（`signal_core_gauge_permille()` / `_cell_permille()` 一并删除）。
 *
 * ⚠ 块尺寸（表面 px）= 原生外沿 + 读数列：`104 × 28`（640 空间）= **208 × 56**。
 *   块一变宽变高，货架会把它后面的块（116/117 …）往右推 —— 这是「按原生尺寸报块」的必然结果，
 *   报告里登记了影响面。*/

/* ---- ★★ 2026-09-15：「组合 UI」把本卡的 HUD 整块搬进了**共享件**（`cards/faith_hud.{h,c}`）----
 *
 * 本节原来那一整批常量（`SIGNAL_U640_TO_SURF`、块尺寸 208×56、元素相对块左下角的偏移、
 * 条宽的归一化分母 64、档位中断号 7/8/9/10 与 20 帧升温门、压暗门限 416/400/−64、
 * 三行文字读数的位置与颜色 `SIGNAL_TEXT_*` / `SIGNAL_C_TEXT*`）**全部搬到 `cards/faith_hud.c`
 * 的常量块**（那边逐条给了 `th11:0x…` 的出处与本轮的一手订正）；本文件一个都不留 ——
 * 免得两处各一套几何、日后改一处漏一处。
 *
 * 为什么搬（用户 2026-09-15 口径 ①②③，`NEXT.md` §2A）：两块 HUD 合并成**一块**
 *   （一个蓝点 + 信号强度槽 / 最大得点×信号强度 / 连击槽三行）⇒ 必须由**一个**模块持有，
 *   否则「只有 110 在场时信号中性 1.00 / 只有 111 时连击条填满」没有落点。
 *   合并后的块 = **188×60 表面 px**（640 空间 94×30）= `engine/player/th11/04-merged-hud-spec.md`。
 * ⚠ 本卡侧**不再持有任何 HUD VM**：原来那 3 支原版 VM（脚本 212/213/214 —— **脚本本身复用**，
 *   合并块照样用它们画槽底 / 增长条 / 图标）+ 3 行 `ce_hud_vm_text_*` 读数一起退役；
 *   起 VM / 摆位 / 上色 / 闪色 / 压暗 / 回收全在共享件，`on_delete` 那条路径由
 *   `faith_hud_departed()` 收（见 `signal_clear()` 的注释）。
 * ⚠ **块认领也搬走了**（原来在 `on_load` 里 `ce_hud_slot_claim_card(111, …)`）：两张卡共用一块
 *   ⇒ 由共享件按「在场的、id 较小的那张」认领（110 优先），本卡不再认领。
 *
 * `SIGNAL_LOG_EVERY` 仍在下面（它是本卡诊断汇总行的周期，与 HUD 无关）。*/

#define SIGNAL_LOG_EVERY       600        /* 每 600 帧一条汇总（实跑对账）*/

/* ★★ 2026-09-16（用户口径 ②）：帕秋莉的子机格子数 = **原生火力档**（0..4 格），
 *   不再固定一格。判据/算式/一手表全在 `signal_core.h` 的「原生子机抑制 + 格数随 P + DPS 归一」那一段
 *   （本轮把三件事一起改了：格数随档、伤害按 18/13 的比例折算到「每档合计 = 原生子机层」、
 *   原生那 4 格子机整层关掉）。这一步同时**取代**了旧注释里那条「只申请一格以免超模」的取舍：
 *   超模闸现在由**伤害**那一侧守（4 格 × 20/12 = 6.667/帧 = 原生满档子机层的逐值），不是靠压格数。
 *   格数上限 = 声明层上限 `CE_OPTION_POSE_CELLS`（4，共享件；原生数组也是 4 格）。
 *   ⚠ 这个常量必须在卡状态结构体**之前**定义（结构体里拿它开数组）。*/
#define SIGNAL_OPTION_MAX          CE_OPTION_POSE_CELLS

/* ★★ 2026-09-16：**本卡申请的 4 个子机槽**（原来是卡状态里的 `opt[1]`）。
 * 为什么搬到静态区：格数随火力档（0..4）之后 `ce_option_t opt[4]` 会把 `signal_card_t` 顶到 516 字节，
 *   越过 SDK 的私有状态上限 496（`CE_STATE_BYTES` 512 − `CE_STATE_RESERVED` 16；`sdk_core.h`）。
 * 为什么静态区**安全**：卡 id 在一局里唯一（`CE_MGR_OWNED` `0xd70` 的 int[255] = 「这张卡拿到了没有」），
 *   而本卡的行为只注册一次 ⇒ 进程里最多一个 111 实例（本文件本来就有别的静态态：`s_p_item` / 色调那几支）。
 * ⚠ 生命周期照旧：`signal_support_reset()` 与 `on_option_pose_apply` 的退位分支负责
 *   `ce_option_drop` 并清零（静态区不再被 `memset(s, 0, …)` 顺手清掉，必须显式清）。*/
static ce_option_t s_opt[SIGNAL_OPTION_MAX];

/* ---- 卡私有状态（`ce_state`，含 `CE_CARD` 的 16 字节保留头）---- */
typedef struct {
    faith_slot_t slot;            /* Δ_111（定点 ×100）/ **signal（千分点 0..130000）** / 小绿点余数
                                   * / base（按难度）/ delta_cap（= 原版基础）—— 全在共用件里。
                                   * ⚠ `slot.combo` **111 侧不用**（th11 一手没有连击槽，见 faith_core.h）*/
    int32_t      last_graze;      /* 上一帧原生擦弹计数；-1 = 未初始化 */
    int32_t      graze_mult;      /* 擦弹倍率（千分点）= `min(⌊graze/100⌋,899)×10`（一手整数阶梯）*/
    int32_t      last_lives;      /* 上一帧残机（被弹检测的兜底：命数减少 ⇒ 归零）。-1 = 未初始化 */
    int32_t      frag_idx;        /* 得分奖残已发到第几项（一手 `th11:0x4a5750`；一局内不重置）*/
    int32_t      frame;
    int32_t      stat_big;        /* 诊断：**收到**的大绿点颗数 */
    int32_t      stat_small;      /* 诊断：**收到**的中绿点颗数 */
    int32_t      stat_big_spawn;  /* 诊断：**撒出**的大绿点颗数（§2.2 击破通道）*/
    int32_t      stat_mid_spawn;  /* 诊断：**撒出**的中绿点颗数（满 P 吃 P 点）*/
    int32_t      stat_pool_full;  /* 诊断：共用池满导致撒点失败的次数（`faith_spawn()` 返回 0）*/
    int32_t      stat_cancel;     /* 诊断：消弹总数（小绿点来源）*/
    int32_t      stat_sweep;      /* 诊断：满槽全收触发次数 */
    int32_t      stat_bomb;       /* 诊断：灵击次数 */
    int32_t      stat_bomb_dbf;   /* ★ 2026-09-15c 诊断：**决死形态**发动的次数（`dbf ≤ bomb`；
                                   * `bomb − dbf` = 主动次数）*/
    signal_bomb_form_t bomb_form; /* ★ 2026-09-15c 本发的**形态**（按 B / **决死 B**）+ 累计消弹颗数。
                                   * 语义与代价口径全在 `signal_core.h` 的「灵击 → 决死」块
                                   * （状态在 core ⇒ 主机单测钉得住；卡侧只「检测窗口 → 传参」）。*/
    int32_t      stat_reset;      /* 诊断：被弹归零次数 */
    int32_t      stat_frag_item;  /* 诊断：**捡到**的命碎道具颗数（TH18 item type 0x10）*/
    signal_seg_t seg;             /* ★★ 2026-09-15e 命碎段机（判据在 `signal_core.c`；卡侧只喂信号）*/
    int32_t      stat_frag_spawn; /* 诊断：**撒出**的命碎道具颗数（与 `seg.drops` 同步，便于日志对账）*/
    int32_t      stat_extend;     /* 诊断：得分跨阈值发的残机数（一手 `th11:0x41f001` 那条路）*/
    /* ---- ★ 支援特技（2026-09-15；六条，本轮实装。判据/代价见上面那一段）---- */
    signal_gap_t  gap;            /* 紫：缺口瞬移状态机（`th11:0x430e50`）*/
    signal_hold_t suika;          /* 萃香：`attempted_direction==0 && !(INPUT&0x9)`（`th11:0x8ba4`）*/
    signal_hold_t aya;            /* 文：`!(INPUT&0x9)`（`th11:0x8ba8`）*/
    int32_t       aya_on;         /* 1 = 本帧超高速生效（只在进入时记一次 + 打一行）*/
    int32_t       suika_on;       /* 1 = 本帧处于萃香的持续态（诊断）*/
    int32_t       stat_gap;       /* 诊断：紫发动次数 */
    int32_t       stat_suika;     /* 诊断：萃香**进入持续态**的次数 */
    int32_t       stat_aya;       /* 诊断：文**进入持续态**的次数 */
    int32_t       stat_alice;     /* ★ 2026-09-15g 诊断：爱丽丝「**减半的灵击**」发动次数
                                   * （旧语义「本卡不实现 ⇒ 恒 0」作废：用户口径把这一条改成两条真·效果，
                                   *  见 `signal_core.h` ④(b)；「子机上限 8」那条**仍不实现**）*/
    int32_t       stat_patchouli; /* 诊断：帕秋莉换档次数 */
    uint32_t      flan_ring_vm;   /* ★ 2026-09-16：二小姐蓄力光环的 VM id（脚本 277；0 = 没起）*/
    /* ★★ 2026-09-16m：灵击的跨帧状态**不在这里**了 —— 它搬进了本文件的文件静态
     *   `s_b10`（`signal_bomb10_t`）。理由：卡的 `ce_state` 有 **496 字节硬上限**
     *   （`static_assert`，超了会静默拿不到状态），而 110 型灵击要 11 个字段。
     *   同款做法 = 本文件的 `s_opt[]` / `s_tint_*`（卡是单例，安全）。*/
    int32_t       stat_opt_tick;  /* ★ 2026-09-16 诊断：本卡调过几次 `ce_tick_shooters_for_card()`
                                   * （0 = 子机开火那条路根本没跑 ⇒ 先查 `opt_on` / `ce_option_claim`）*/
    int32_t       stat_nitori;    /* 诊断：荷取成功退还火力的次数 */
    /* ---- ★ 魔理沙侧的运行态（帕秋莉的子机层 + 荷取的退款窗口）----
     * ⚠ 子机格子的账**不在这里**（2026-09-16 起）：`ce_option_t opt[4]` 塞进来会让本结构体
     *   516 字节 > 私有状态上限 496（`CE_STATE_BYTES` 512 − `CE_STATE_RESERVED` 16，`sdk_core.h`；
     *   `_Static_assert` 当场拦下）。⇒ 挪到**本文件的静态区** `s_opt[]`（见它自己的注释）。*/
    int32_t       opt_script;     /* 当前子机贴图脚本（-1 = 没申请）*/
    int32_t       opt_on;         /* 1 = 本帧本卡是子机声明层的落笔者（开火门的条件）*/
    int32_t       formation;      /* = `th11:0x8bac`，0..4；一手的循环是 `(n+1)%5` */
    int32_t       patch_latch;    /* ❓ 自定：换档的按下沿闩锁（一手是电平，会长按走马灯）*/
    int32_t       z_held;         /* ★ 2026-09-16：Z 已连续按住的帧数（0 = 这一帧没按）—— 换档的新鲜度门用 */
    int32_t       shift_held;     /*   同上，Shift。判据见 `signal_core_patchouli_fresh()` */
    /* ⚠ 荷取的退款账**不在卡状态里**了（2026-09-16q）：`signal_nitori_t` 搬进文件静态
     *   `s_nitori`（泡 + 窗是同一件事，见上面「河童罩子」那一段的 `s_shield`）。
     *   理由 = 老问题：卡的 `ce_state` 有 496 字节硬上限（本行原来就顶着它）。*/
    /* ⚠ 这里原来有一个 `cost_spent`（诊断：六条特技一共花掉的信号）—— 2026-09-16「全免费」
     *   之后它恒为 0，随价那一层一起删了。*/
    /* ---- ★ 2026-09-15g 爱丽丝（魔理沙×爱丽丝）：灵击 = 别人的一半 + P 点平均 +0.025 档 ----
     * 口径、一手核对（`pl01a.sht` 的 `+0x24`=12 / `+0x20`=8 ⇒ 一发占满槽 1/8 vs 别人 1/4 = **一半**）、
     * 门槛与下限的选择 —— 全在 `signal_core.h` 的 ④ 段；卡侧只有两个门 + 两处调用。
     * ⚠ **卡状态里没有爱丽丝的字段了**（2026-09-15h）：P 点那条的**交替相位搬进了共享件**
     *   （`faith.c` 的 `s_p_alt`，因为 P 点补值的落笔者恒为 110 ⇒ 相位必须两边都够得到）；
     *   灵击那条是**无状态**的纯函数。⇒ 本卡的 `signal_support_reset()` 也不再需要为它留例外。*/
    int32_t       stat_p_topup;   /* ★ 诊断（2026-09-15）：本卡落笔的 P 点火力补差颗数 */
    /* ---- ★★ 2026-09-15i：**咲夜(2) 三条 + 早苗(3) 三条**（用户拍板的伙伴映射，不是复刻）----
     * 判据 / 一手 / 用户口径的分层全在 `signal_core.h` 的「咲夜 / 早苗」那一段；
     * 卡侧只做「读引擎 → 调 core → 落效果」。诊断数一律从 core 结构里取（不另开标量）。*/
    signal_remilia_t remilia;     /* 咲夜×大小姐：逐弹 3 帧循环（1 帧计入 + 2 帧不计）的状态 + 累计 */
    signal_flandre_t flandre;     /* 咲夜×二小姐：蓄力 300 帧 ⇒ 松手清屏 */
    signal_kanako_t  kanako;      /* 早苗×神奈子：收点线的「快照基准 + 相对下移」（✅ 已接共享 PoC 高度层）*/
    signal_koishi_t  koishi;      /* 早苗×恋恋：放开键 ⇒ 4 帧无敌（载体与量都照抄原生 29 Aya 卡）*/
    int32_t       extra_graze;    /* ★ 2026-09-16m 起**只作诊断**：本帧大小姐额外计入的擦弹次数
                                   *   （喂槽已经不靠它了 —— 那本账走原生计数 `0x4ccd0c` 的帧差）*/
    int32_t       extra_graze_fired; /* 诊断：上面那些里**真的调成功**引擎 `do_graze` 的次数*/
    int32_t       suwako_on;      /* 诊断：本帧贴板底（诹访子生效 ⇒ 线下不扣）*/
    int32_t      log_frame;
} signal_card_t;

_Static_assert(sizeof(signal_card_t) <= 496, "ce_state 上限 496 字节（超了会静默返回 NULL）");

static signal_card_t *st(ce_card_t *c) { return ce_state(c, signal_card_t); }

/* ★★ 2026-09-15：P 点火力那一层的**每颗道具闩**（与 `mof.c` 的 `s_p_item` 同形，各卡各一份）。
 * `on_item_commit`（`th18:0x44658a`）不带 payload ⇒ 靠它与 `on_item_collect`（`0x44625f`）
 * 在一颗道具上背靠背的关系记住「这一颗是不是小 P」（同 `collect2x.c` 的 `s_stash` 形状）。*/
static int      s_p_item;

static int     signal_difficulty(void) { return *(volatile int32_t *)SIGNAL_ADDR_DIFFICULTY; }
static int32_t signal_graze(void)      { return *(volatile int32_t *)SIGNAL_ADDR_GRAZE; }

/* ---- 计分层：声明「本卡这一份 G_11」+ 自己的信号强度（★ 2026-09-13 重做）----
 * 111 在用户公式里是**耦合族**的一员：`G_11 = 基础_111(难度) + Δ_111`，
 * 与 `G_10` 一起被 `(擦弹倍率 + 信号强度)` 缩放（`(Δ_10 + Δ_11) × (…)`）。
 * 两条声明，各管一件事：
 *   · `ce_score_declare_coupled(id, faith_score_unit(&s->slot))` = 我这份**得点**（点）；
 *   · `ce_score_mul_declare(id, signal_core_strength(s->slot.signal))` = 我这份**信号强度**
 *     （**千分点**，0..1000；`signal_core_strength()` 已经把 0..130 点换成 0..1000‰，
 *      **不能**声明裸的 `s->slot.signal`——那是 0..130000 的定点，差 100 倍）。
 * ⚠ 110 也声明同一个耦合族，但**谁都不是落笔者**了：两条声明由层合成，
 *   谁的 id 小都不影响结果（这正是重做要修的「107/110 在场 ⇒ 111 静默失效」）。
 * 粘性语义：信号掉到 0 时要**照样声明 0**（否则旧值留在层里）；删卡由 `ce_layer_drop_card()` 清。*/
static void signal_declare_score(signal_card_t *s)
{
    if (!s) return;
    /* ★ 声明的是 **Δ_11，不含基础**（`faith_score_unit()` = `base_111(难度) + Δ_111`，
     * 这里把 `s->slot.base` 减掉）：用户公式的基础项 `G_0` 由层算一次。
     * 旧实现声明完整值是为了配合落笔者的「减两份 base」补丁；重做后那套补丁没了
     * （见 `../SCORE-LAYER-REWORK.md` §5 第 1 条坑）。单卡时两者等值（有单测）。*/
    int32_t delta = faith_score_unit(&s->slot) - s->slot.base;
    if (delta < 0) delta = 0;
    ce_score_declare_coupled(SIGNAL_ID_NUM, delta);
    /* ★ 2026-09-18（换版）：本卡是 **th11** 那一族 —— 最大得点在原生里**没有高度项**
     *   （一手：蓝点两支不读 `item.y`，`piv_display()` 全 exe 只有 4 个消费点）⇒ `coeff_111 ≡ 1`。*/
    ce_score_family_declare(SIGNAL_ID_NUM, CE_COEFF_FAMILY_1);
    ce_score_mul_declare(SIGNAL_ID_NUM, signal_core_strength(s->slot.signal));
}

/* ---- 绿点收集回调（`faith.c` 的 `dot_collect()` 发出来；只回调认领了本掩码的卡）----
 * 111 的槽 = `1 << FAITH_SLOT_111`。三档里：
 *   · BIG（大绿点，击破通道）= +100（`FAITH_SA_BIG` → 由 `signal_on_enemy_drop_pre` 撒）；
 *   · SMALL 档实体（本卡用作「中绿点」= 满 P 后吃 P 点）= +10（`FAITH_SA_MID` → `signal_on_item_collect` 撒）；
 *   · 小绿点（每 100 个 +10）**不建实体** ⇒ 走 `on_cancel`（消弹计数），不在这里。
 * 记账（加值 / Δ_111 上限）**全在 `signal_core_pickup()`**（纯逻辑，单测覆盖），
 * 本回调只做「这颗粒子是不是我的槽 + 诊断 + 重新声明」这三件卡侧的事。
 * ⚠ 2026-09-15：**不再有连击槽那一笔**（th11 一手没有连击槽）。*/
static void signal_on_pickup(void *user, int slot_mask, int tier, int32_t value)
{
    ce_card_t *c = (ce_card_t *)user;
    signal_card_t *s = c ? st(c) : 0;
    faith_ev_t ev;

    if (!s || !(slot_mask & (1 << FAITH_SLOT_111))) return;   /* 不是本卡的槽（110 的点不碰）*/
    if (value <= 0) return;

    /* 档位只决定「走哪个事件档」（事件档是共用件收下加值的前提，见 `faith_slot_add()`）。*/
    ev = (tier == (int)FAITH_TIER_BIG) ? FAITH_EV_BIG : FAITH_EV_GREEN;
    (void)signal_core_pickup(&s->slot, ev, value);
    if (tier == (int)FAITH_TIER_BIG) ++s->stat_big; else ++s->stat_small;
    signal_declare_score(s);
}

/* ---- 消弹回调（`faith.c` 的 `faith_cancel_account()` 发；颗数 = `ce_cancel_radius()` 的返回值）----
 * 111 侧 = **小绿点「每 100 个 +10」**（一手：小绿点 = 显示 +0.1/颗，÷100 量纲就是这条），
 * 余数走 `faith_slot_t.small_rem`（**跨关保留**）。
 * 110 侧同一入口按自己的表记微信仰点 —— **各槽自己算自己的**，不做跨卡耦合（共用件负责）。
 * ⚠ 2026-09-15：**不再加连击槽**（th11 一手没有连击槽）。
 * ❓ 一手的「每颗小绿点另给 +10 分」（`th11:0x423ccd`）我们**没做** —— 111 的分数只走蓝点分那条通道
 *   （登记在报告「未做」里）。*/
static void signal_on_cancel(void *user, int32_t bullets)
{
    ce_card_t *c = (ce_card_t *)user;
    signal_card_t *s = c ? st(c) : 0;
    int32_t add111 = 0;

    if (!s || bullets <= 0) return;
    s->stat_cancel += bullets;
    /* ★ E-1 硬约束（2026-09-15）：消弹这一档是 **type 8（小绿点）**，只许 `FAITH_TIER_MICRO`
     *   （一手 `th11:0x40b0a2 mov ecx,8` → `call 0x424230`；TH10 侧 `0x4081e7 push 8`）。
     *   ⚠ 微档今天不建实体（只记账，每 100 颗 +10）；补实体版时 tier 必须取
     *   `faith_core_tier_for_source(FAITH_SRC_CANCEL)`，且位置要从**弹自己**来（见 `mof.c` 同一处的注）。*/
    faith_slot_add_cancel(0, &s->slot, bullets, 0, &add111);   /* s110 传 0：110 的槽不归本卡管 */
    if (add111 > 0) signal_declare_score(s);
}

/* ---- 这只敌人是不是 boss（§7.3「大绿点 boss 不掉」的判据）----
 * 与 `mof.c` 的 `mof_is_boss()` **同口径**：读 `EnemyManager` 的 boss id 表（`em + CE_EM_BOSS_IDS`，
 * stride 4；一手 = `0x4237ff` 按它走链表、写点 `0x42d4aa`，先例 `season.c` 的 `enemy_is_boss`）。
 * ⚠ 槽数 4 来自 `engine.h:82`（🟡 asm 里证不了表大小：没有边界检查，只有一个调用者探过 0/1/2）
 *   —— 读的是管理器结构体内的偏移，多读一格不会越界。⚠ 两个文件的这份判据是**有意重复**的：
 *   它是 12 行只读代码，而共用的落点是 `faith.*`（那是实体池，不该塞引擎判据）。*/
static int signal_is_boss(const uint8_t *e)
{
    uint8_t *em = CE_ENEMY_MGR();
    int32_t id;
    int i;

    if (!e || !em) return 0;
    id = *(const int32_t *)(e + CE_ENEMY_ID);
    for (i = 0; i < CE_EM_BOSS_SLOTS; ++i) {
        if (*(const int32_t *)(em + CE_EM_BOSS_IDS + i * 4) == id) return 1;
    }
    return 0;
}

/* ======================================================================================
 * ★★ 2026-09-15e：命碎（残机のかけら）—— 「每 2 段没死过的攻击段 ⇒ 撒 1 颗 item type 0x10」
 * ======================================================================================
 * **判据 / 粒度 / 近似 / 一手出处全部在 `cards/signal_core.h` 的「命碎段机」那一段**（别在这里重复推理）。
 * 本文只做三件事：① 读引擎（boss 指针 + 相位槽指纹 + 符卡位）；② 喂命数差分；③ 撒道具。
 *
 * | 件 | 在哪 | 管什么 |
 * | --- | --- | --- |
 * | 段状态机（边界去抖 / 死亡归属 / 余数 / 满 N 出账）| `signal_core.c` 的 `signal_seg_tick()` | **纯逻辑、主机单测** |
 * | 读引擎 + 撒道具 | 本文件这一段 | 卡侧 |
 *
 * ## 相位槽（只读；一手 = 本轮自己在 `local/_scratch/th18_full.asm` 上复核）
 *   · 槽数组 = `zEnemy+0x6374` 起 `SIGNAL_ED_INT_COUNT`（8）个，stride `SIGNAL_ED_INT_STRIDE`（`0x88`）
 *     —— 超时判定段 `0x42ef33: imul esi,edx,0x88` + `0x42ef46: mov eax,[esi+ecx*1+0x6374]`（清槽写 `-1`）；
 *   · 槽内 `+0x00 = hp_value`（`-1` = 空）、`+0x04 = time`（`0x42ef39` 比的正是 `+0x6378`）、
 *     `+0x08 = sub_life[0x40]`；
 *   · **活动槽 = 第一个 `hp_value > -1 && time > 0`**（同 `cards/lolk.c` 的 `phase_read()`；
 *     出处 `engine/chapter/th18/02-wave-signals.md` §2/§3）。
 *   ⚠ 关底 boss 的每个相位各 `setInterrupt`（`ins_514`）一次：`st01bs.ecl` 的
 *     `Boss1`/`BossCard1`/`Boss2`/`BossCard2` 各一条 ⇒ **一个非符一段、一张符卡一段**
 *     （本轮自己用 `thecl -d 18 -x` 复核；与 115 的既有结论一致）。
 *
 * ⚠ 与 `cards/lolk.c` 的 `boss_enemy()` / 指纹算法是**有意重复**的一小段只读代码：
 *   115 的共用落点是它自己的**波次面板**（core + UI），不是引擎判据 ⇒ 不把 115 的 core 拖进 111
 *   （那等于改别人的卡）。地址常量与它逐字相同、**engine.h 里没有 ⇒ 就地补**。
 * ==================================================================================== */
#define SIGNAL_ENEMY_INTERRUPTS  0x6374   /* zEnemyData.interrupts（zEnemy+0x6374，一手见上）*/
#define SIGNAL_ED_INT_STRIDE     0x88     /* zEnemyInterrupt，一手 `0x42ef33` 的 `imul …,0x88` */
#define SIGNAL_ED_INT_COUNT      8        /* 8 格（`engine/chapter/th18/02-wave-signals.md` §2）*/

/* 场上的 boss（boss id 表命中；取第一个）。取不到返回 0。
 * ⚠ 每帧重新取、**不缓存指针**（敌人对象会被回收）——与 `lolk.c` / `season.c` 同纪律。*/
static uint8_t *signal_boss_enemy(void)
{
    uint8_t *em = CE_ENEMY_MGR();
    int i;

    if (!em) return 0;
    for (i = 0; i < CE_EM_BOSS_SLOTS; ++i) {
        int32_t id = *(const int32_t *)(em + CE_EM_BOSS_IDS + i * 4);
        uint8_t *e;
        if (!id) continue;
        e = ce_enemy_by_id(em, id);
        if (e) return e;
    }
    return 0;
}

/* 活动相位槽的指纹（0 = 没有活动槽）。指纹 = (槽号, hp_value, time, sub_life 前 4 字节) 的混合
 * —— 形状与 `cards/lolk.c` 的 `phase_read()` 完全一致（同一批一手字段，两边各自的卡用它）。
 * ⚠ 引擎只保证「槽被清空 → 新相位登记新槽」（一手 `0x42ef53` 写 `-1`、`0x42f36b` 段登记）；
 *   指纹只是**我们**把这个跳变读出来的手段（🟡 本仓用法，不是引擎语义）。*/
static int32_t signal_phase_sig(const uint8_t *e)
{
    int i;

    if (!e) return 0;
    for (i = 0; i < SIGNAL_ED_INT_COUNT; ++i) {
        const uint8_t *slot = e + SIGNAL_ENEMY_INTERRUPTS + i * SIGNAL_ED_INT_STRIDE;
        int32_t hp = *(const int32_t *)slot;
        int32_t tm = *(const int32_t *)(slot + 4);
        if (hp > -1 && tm > 0) {
            return (int32_t)((i + 1) * 1000003 ^ hp * 31 ^ tm * 131 ^
                             *(const uint32_t *)(slot + 8));      /* sub_life 的前 4 字节 */
        }
    }
    return 0;
}

/* 撒一颗命碎道具（`SIGNAL_FRAG_ITEM_TYPE` = **item type 0x10** → EXTEND2 卡道具 → `CardLifeFragment`）。
 * · **位置 = 掉落源（boss）**；boss 取不到（刚被打死 / 对象已回收）⇒ **退回自机位置**并如实记一行日志。
 *   一手是「在敌机自己的位置掉」（`th11:0x4101b7` 的 `spawn_item(pos = &enemy+0x34, …)`）⇒ 优先 boss。
 * · **角度 −π/2、速度 2.2 = 一手**：TH18 原生 main-drop 的实参就是这一对
 *   （`0x430479` 那一段：`[esp+8] = 0x400ccccd` = 2.2、`[esp+4] = 0xbfc90fdb` = −π/2，`sdk.h` 的
 *   `ce_spawn_item` 注释也这么记），而 TH11 掉命碎道具走的 `EnemyDrop::drop_items` 同值
 *   （`th11:0x498430` = 2.2 / `th11:0x49842c` = −π/2）⇒ 两侧同口径。
 *   ⚠ 115 的**碎片卡**用的是 1.2 + 自机上方 76 px —— 那是它自己的表现约定，不是原生 main-drop。*/
static void signal_seg_drop(signal_card_t *s, const uint8_t *boss)
{
    const float *bp = boss ? ce_enemy_pos(boss) : 0;
    const float *pp = ce_player_pos();
    float pos[3];

    if (bp) {
        pos[0] = bp[0]; pos[1] = bp[1]; pos[2] = bp[2];
    } else if (pp) {
        pos[0] = pp[0]; pos[1] = pp[1]; pos[2] = pp[2];
    } else {
        return;                                   /* 两处都取不到（极端）⇒ 这一颗不发，不写引擎 */
    }
    if (!ce_spawn_item((uint32_t)SIGNAL_FRAG_ITEM_TYPE, pos, -1.5707964f, 2.2f)) {
        ce_log("signal: 命碎道具撒失败（道具池满？）type 0x%x", (unsigned)SIGNAL_FRAG_ITEM_TYPE);
        return;
    }
    ++s->stat_frag_spawn;
    ce_log("signal: 命碎 -> item type 0x%x @ %s（第 %d 段没死过 / 已结算 %d 段 / 本颗是第 %d 颗，余 %d）",
           (unsigned)SIGNAL_FRAG_ITEM_TYPE,
           bp ? "boss 位置" : "**自机位置（boss 指针取不到）**",
           (int)s->seg.segs_clean, (int)s->seg.segs, (int)s->seg.drops, (int)s->seg.pending);
}

/* ---- 大绿点（§2.2 / §7.3「击破掉落」= +100）：撒点钩子 ----
 * 判据与 110 的大信仰点**完全一样**（§7.1 定案「绿点就是同一个东西，只是数值表不同」⇒ 触发点共享）：
 *   · `drop_main == 2`（TH18 的金钱 = 本作的点道具）⇒ `mof_core_wants_big_main()`（判据①）；
 *   · 或 ex_drop 里 type 1 与 type 2 **都 > 0**（「同时掉钱与 P」）⇒ `mof_core_wants_big_dual()`（判据②）；
 *   · **boss 不掉**（§7.3 / §3.2.1）。
 * ⚠ 为什么挂 `on_enemy_drop_pre` 而不是 `on_enemy_death`：本卡既不改掉落表、也不抢先机，
 *   而这个钩子的入参 `counts` **同时**给出 `main_drop` 与 ex_drop 计数 ⇒ 两条判据一次拿全
 *   （110 分两个钩子是因为它还要在死亡回调里做别的账）。
 * ⚠ 回调里只 `faith_spawn()`：O(1)、只写 `faith.c` 的静态池；**ANM VM 由 `faith_pool_tick()`
 *   隔 1–4 帧再建**（`faith.c` 的 `delay`）⇒ 不在敌人的死亡栈里建任何引擎对象、不碰敌人。*/
static void signal_on_enemy_drop_pre(ce_card_t *c, int32_t *counts)
{
    signal_card_t *s = st(c);
    uint8_t *e;
    const float *ep;
    int32_t main_drop, drops = 0, p_count = 0;
    int is_boss, want;
    int i;

    if (!s || !counts) return;
    /* `counts` = `zEnemyDrop + 0x04`（`CE_ENEMY_DROP_COUNTS`）；`zEnemyDrop` = `enemy + 0x623c`。
     * 一手（asm `0x430499`：`lea ecx,[edi+0x623c]` 之后 `call 0x430510`）⇒ 反推敌人指针成立。*/
    e = (uint8_t *)counts - CE_ENEMY_DROP_COUNTS - CE_ENEMY_DROP;
    is_boss = signal_is_boss(e);
    main_drop = *ce_drop_main(counts);
    for (i = 0; i < CE_ENEMY_DROP_TYPES; ++i) {          /* ex_drop[(type-1)]（engine.h:44）*/
        if (i + 1 == MOF_DROP_TYPE_MONEY) drops = counts[i];
        if (i + 1 == MOF_DROP_TYPE_P)     p_count = counts[i];
    }
    want = mof_core_wants_big_main(main_drop, is_boss)
        || mof_core_wants_big_dual(main_drop, drops, p_count, is_boss);
    if (!want) return;
    ep = ce_enemy_pos(e);
    if (!ep) return;

    /* 值用**本卡的**「大」档（`SIGNAL_GREEN_BIG` = 100），槽写本卡自己的掩码。
     * 与 110 同一次死亡各撒各的（池按 `slot_mask` 分账、回调只发认领该掩码的卡）⇒ 互不干扰。*/
    if (faith_spawn(ep[0], ep[1], faith_core_tier_for_source(FAITH_SRC_BREAK),
                    1 << FAITH_SLOT_111, SIGNAL_GREEN_BIG)) {
        ++s->stat_big_spawn;
    } else {
        ++s->stat_pool_full;
    }
}

/* ---- `on_item_collect`：两件事都挂在这里（命碎道具的观测 + 中绿点的撒点）----
 * ① **命碎道具**（TH18 **type `0x10`** ↔ th11 的 type 5）：**只观测**（手数 / 记日志）——
 *    道具由**本卡**在段收尾时撒（见 `signal_seg_drop()`），捡到之后的一切走原生链
 *    （`0x457570` 累加 → 阈值 3 个 = 1 残）。★ 2026-09-15e：这条观测**现在真能数到东西**
 *    （以前它盯的是 type 4，而全游戏只有 `st04mbs` 一处掉 `0x10` ⇒ 恒 0；订正见文件头）。
 * ② **中绿点**（§2.2「满 power 后红点转换」= +10）：★ 2026-09-15 起这是**同帧竞态网**，不是主路。
 *    **主路** = 共用件 `faith_convert_full_p_items()`（本卡 `on_tick_2` 里每帧调，`faith_pool_tick()` 之前）：
 *    满 P 时把原生 P 类道具**原位删掉**、在它自己的位置撒 type 9 档的点 —— 与 110 同一台机器。
 *    一手 TH11 侧 = `th11:0x4240f0`（全场转换）+ `th11:0x424578` 的表（命中 → `type 9`），
 *    全文与「为什么落点不是改 type」见 `faith.h` 里那个函数的长注。
 *    **这里**只兜「引擎的道具 tick 抢在我们扫到之前就把那颗 P 道具喂给了玩家」这一种情况
 *    （那一颗已经拿过原生的满 P 身价分，仍要补一颗中绿点）。正常路径下不触发。
 * ⚠ 判据用 `mof_core_full_p_replace()`（**TH18 类型版**：满 P 且 type ∈ {小 P(1), 大 P(3)}）——
 *   旧实现只认 `type 1`（`mof_core_pick_p_item()` 那条通用口），**漏了 TH18 的大 P**。
 * ⚠ 档位走 `faith_core_tier_for_source(FAITH_SRC_FULL_P)`（E-1 硬约束：满 P 转换**只许** type 9 档）。
 * ⚠ 不动原生道具：`type` 不改、分派表不碰，撒出来的**一律是自建 VM**。*/
static void signal_on_item_collect(ce_card_t *c, uint32_t type, const void *item)
{
    signal_card_t *s = st(c);
    const float *ip;

    /* ★★ 2026-09-15：**先无条件记下本颗道具的身份**（P 点火力那一层要它，见 `signal_on_item_commit`）。
     * 必须写在任何 return 之前（下面两条早退），否则上一颗的身份会漏到这一颗的 commit 上。 */
    s_p_item = (type == (uint32_t)MOF_P_ITEM_TYPE18);
    if (!s || !item) return;

    /* ---- ① 命碎道具（★ 2026-09-15 一手复核的映射，见 `signal_core.h` 的命碎块）----
     * TH11 的命碎是 **item type 5**（`th11:0x423f00[4] = 0x423c3e` → `call 0x4209b0` = 命碎 +1，5 碎 = 1 残）；
     * 它在 TH18 侧的对应物是 **item type `0x10`**（EXTEND2 卡道具 → `CardLifeFragment` id 3 →
     * `0x457570` 累加，阈值 3 个 = 1 残）；**不是** type 4（见文件头的订正）。
     * ⚠ 这一支**只观测**：道具是**本卡**撒的（`signal_seg_drop()`），捡到之后的碎片/残机**全是原生**的事
     *   ⇒ 我们**不重复发放**、也不自己加碎片（否则双倍）。
     * （顺带：TH11 的 type 7 = 1UP 道具 ↔ TH18 的 type 5 = 整命，同样是原生。研究文档 §4 把这两个
     *   type 号写反了，一手复核的指令原文在 `signal_core.h`。）*/
    if (signal_core_is_lifefrag_item(type)) {
        ++s->stat_frag_item;
        ce_log("signal: 捡到命碎道具 (th18 type 0x%x = th11 type 5) —— 原生已入碎片账（3 个 = 1 残），本卡只计数",
               (unsigned)type);
        return;
    }

    /* ---- ② 中绿点（§2.2「满 power 后红点转换」= +10）：同帧竞态网（见函数头）----*/
    if (!mof_core_full_p_replace((int32_t)type, CE_CURRENT_POWER(), SIGNAL_MAX_POWER_VAL())) return;
    ip = (const float *)((const uint8_t *)item + CE_ITEM_POS);
    if (faith_spawn(ip[0], ip[1], faith_core_tier_for_source(FAITH_SRC_FULL_P),
                    1 << FAITH_SLOT_111, SIGNAL_GREEN_MID)) {
        ++s->stat_mid_spawn;
    } else {
        ++s->stat_pool_full;
    }
}

/* ==================== ★★ 装了 110（或 111）⇒ 一颗 P 点 = +0.05 档火力 ====================
 * 与 `cards/mof.c` 的 `on_item_commit()` **同一套逻辑、同一份纯逻辑**（`mof_core.h` 的
 * `mof_core_p_topup()` / `mof_core_p_owner()`），两侧各接一次线 —— 用户的要求是
 * 「**装了 110 或 111**，P 点变成 +0.05」⇒ **任一张在场即生效**，不是两张同时在才生效。
 *
 * ⚠ **两张卡同装只算一次**：`mof_core_p_owner()` 的规则是「**id 最小者落笔**」（与
 *   `ce_optpose_*` 低速子机声明层 108 > 111、`ce_dmg_applier()` 的伤害层落笔者同形）
 *   ⇒ **110 在场就 110 落笔、本卡让位**（判据 = SDK 现成的 `ce_owned()` 存在性查询，
 *   spec-review §3.1 的那个例外条）。两侧都调同一个纯函数、喂同一组输入 ⇒ 结论必然一致，
 *   不会出现「都没写」或「都写了」。
 *   ⚠ **不用按帧闩锁**：`ce_cancel_latch_*` 那种闩锁每帧在 `ce_item_frame` 里 reset，
 *     而同帧收一撮 P 点是常态（收点波 / 全屏收点）⇒ 会漏掉同帧的第 2 颗以后。
 *
 * ⚠ 落点是 `on_item_commit`（`th18:0x44658a`）而不是 `on_item_collect`：原生那一发
 *   `add_power(1)`（`th18:0x446a7b` → `0x4573f0`）发生在两个断点之间，且在 collect 里先写会
 *   打乱原生自己的跨档判定（它拿被改过的值做 `idiv [0x4ccd40]` 比较）⇒ 升级会静默
 *   （子机不按新火力档重建）。全套推导在 `mof_core.h` 的 §P 点火力。
 * ⚠ 只碰火力：不写 `MONEY`、不动道具数值 / 不删道具 / 不改分派表。
 *
 * ## ★★ 2026-09-15h 爱丽丝那一支：一颗 P 点 = **平均 +0.025 档**（🟡 用户口径）——
 *    **改走共享策略**（`faith.h` 的 `faith_p_topup_take()`），本卡**不再自己算 2/3 交替**
 *    （原来那份相位 `signal_alice_t.alt` 已**退役**：相位只有共享件那一份，见 `faith.h` 的不变式）。
 *    ★ **110 在场时会不会「两张卡各补一次 ⇒ 双倍」？—— 不会，而且这一点是结构保证的**：
 *      本层是「**每颗道具全局只补一次**」（不是每帧），落笔者由 `mof_core_p_owner()` 选举；
 *      110 在场 ⇒ `ce_owned(110)` 为真 ⇒ 本卡在**上面那一行 `!= 111` 处直接 return**，
 *      一个字节都不写 ⇒ 双倍在结构上不可能发生；**策略也因此在每颗道具上只被问一次**（相位只推进一次）。
 *    ★ 双卡同装时减益**照样生效**（2026-09-15h 修）：落笔者 110 的 `on_item_commit` 问的是**同一个**策略
 *      ⇒ 判据（111 在场 ∧ 机体 == 魔理沙(1)）在 110 那一侧同样成立。*/
static void signal_on_item_commit(ce_card_t *c)
{
    signal_card_t *s = st(c);
    const int is_p = s_p_item;
    mof_p_topup_t t;
    int32_t before, total;

    s_p_item = 0;                                  /* 本卡自己的闩：一颗道具只认一次 */
    if (!is_p) return;
    if (mof_core_p_owner(ce_owned(110), 1) != 111) return;   /* 110 在场 ⇒ 让位（id 最小者落笔）*/

    /* ★★ 问共享策略（**会推进共享相位** ⇒ 只有落笔者可以调）。判据（111 在场 ∧ 机体 == 魔理沙(1)）
     * 与一手核对全在 `faith.h` / `signal_core.h` ④(b)② —— 本卡一个数都不自己算。*/
    total = faith_p_topup_take();                  /* 常态 5 / 爱丽丝生效时 2、3 交替 */
    /* 此刻原生那 +1 已经落地 ⇒ 读到的是 `power_after_native`。 */
    before = CE_CURRENT_POWER();
    t = mof_core_p_topup_units(before, SIGNAL_MAX_POWER_VAL(), total);
    if (t.delta <= 0) return;                      /* 满火力 / 引擎没初始化 ⇒ 一个字节都不写 */

    CE_CURRENT_POWER() = before + t.delta;
    if (t.repop) {
        /* 原生那一步没报的那次跨档 —— 自己补收尾（原生 P 例程自己就是「跨档 ⇒ `0x45d5e0`」，
         * 一手 `th18:0x446a90`）。与灵击扣火力的收尾同款。*/
        ce_repopulate_options();
        ce_log("signal: P 点补差跨档 → 火力 %d 单位（%d 档）· ce_repopulate_options()",
               (int)CE_CURRENT_POWER(), (int)(CE_CURRENT_POWER() / 100));
    }
    if (s) ++s->stat_p_topup;
}

/* ---- 蓝点分声明（★ 2026-09-13 重做；★ 2026-09-15 换到一手口径）----
 * 一手（`th11:0x423b7c`）：蓝点分 `S = trunc10((A + B) × Y / 100)`、`S ≤ 0 ⇒ 10`
 *   （`Y` = 信仰显示值、`A` = 交信度项、`B` = 擦弹倍率项）—— 逐项与常数见 `signal_core.h`。
 * 本卡**不自己算 S**：层合成 `G = G_0 + Σ未耦合 + (擦弹倍率 + 信号强度) × Σ耦合 / 1000`
 * 与一手的 `(A + B) × Y` 同构（千分点刻度一致），本卡只声明两笔：
 *   · `ce_score_declare_coupled(Δ_11)`（信仰那一族的**增量**，基础由层算一次）；
 *   · `ce_score_mul_declare(signal_core_strength())`（信号强度 `A × 10` 千分点）。
 * ⚠ 旧实现里本卡自己乘 `(擦弹倍率 + 信号强度)`、自己守 `ce_score_applier()`；重做后**两条都撤了**
 *   —— 自己乘一遍、层再乘一遍就是双乘（`SCORE-LAYER-REWORK.md` §5 的第 3 条坑）。
 * ⚠ 层**不做**一手的 `trunc10` 与「≤ 0 ⇒ 10」那两步（那是弹窗/入账口径）⇒ 实得分数与一手在这两个
 *   边角上不完全一致（差 ≤9 分；未满槽 + 零擦弹时一手给 10、层给 0）。**层不归本卡**（共享件），
 *   只有 HUD 预览那一行走完整一手式（`signal_core_bonus()`）。
 * ⚠ `signal_core_pv_111_scaled()` 仍留在 `signal_core.c` 里当纯逻辑单测的被测对象，本卡不再调它。*/
static void on_item_score(ce_card_t *c, int32_t *value)
{
    signal_card_t *s = st(c);

    (void)value;                       /* ★ 重做后 `*value` 由 SDK 统一写（`ce_score_apply()`）*/
    if (!s) return;
    signal_declare_score(s);           /* 每颗道具当场声明一次（粘性但便宜）*/
}

/* ---- 满槽全收（§2.1 / §5.6 订正）----
 * ★ 「像上线回收一样自动收道具」= 把道具池里所有活道具的 state 置 3（真空飞行），
 *   引擎自己把它们飞向自机、走**原生**收集（计分 / HUD / 计数 / 磁吸全原生）——
 *   用 SDK 的共享件 `ce_item_collect_all()`（107 森罗结界同一件）。
 * ⚠ 不是 `0x445ffc`（那是 `comiss` 指令，spec §5.6 已订正）。*/
static int signal_sweep_items(signal_card_t *s)
{
    int n = ce_item_collect_all();
    /* ★★ 2026-09-16：**自建的信仰点也要一起收**（用户实跑：「信仰点没有被信号满格全屏吸收（擦弹）
     *   和灵梦伊吹特技全屏吸收」）。根因 = `ce_item_collect_all()` 只扫**原生道具池**，而本池的点是
     *   自建 VM；一手（TH10/TH11）里信仰点**就是道具**（type 3/9），这两条全屏收**天然包含它们**
     *   ⇒ 补 `faith_collect_all(~0u)`（全收，与哪张卡无关）。详见 `faith.h` / `faith.c` 的注释。*/
    int m = faith_collect_all(~0u);

    n += m;
    if (n > 0) {
        ++s->stat_sweep;                    /* 账：**本帧收到过东西**的帧数（三个调用方共用）*/
        /* ⚠ 消息里**不要**再写「gauge full（信号 X >= 满槽）」—— 调用方有三个
         * （满槽的 ⑤、萃香的持续态、以及将来别的），萃香那次打印的是**扣费之后**的槽值，
         * 旧消息因此出现过「signal 54900 >= 100000」这种自相矛盾的行（实跑日志 1055 行）。*/
        ce_log("signal: 全屏收点 -> 原生道具 %d 颗 + 自建信仰点 %d 颗（当前信号 %d）",
               n - m, m, (int)s->slot.signal);
    }
    return n;
}

/* ---- 灵击（§2.4 「B 双选」/ §3.2.1；与 110 **同一套**门槛与消耗）----
 * 触发：`on_tick_2` 读炸弹键**上升沿**（不是 `on_bomb_spent` —— 它在炸弹**成功花掉之后**才触发，
 *      承载不了「库存 == 0」这条分支，review §5.5 订正 2 / 110 同款）。
 * 库存 == 0 → 本卡自干：自写扣火力（≥2 档、扣 1 档、**下限 1.05**）+ 消弹 + 伤害源；
 * 库存 > 0  → 交给原生 `do_bomb`（本卡不动）。
 * ★ 2026-09-15g：**魔理沙(1) 机体**上这一套换成**爱丽丝的 0.5 档**（门槛「能扣就扣」= ≥1.55 档、
 *   扣 50 单位、下限 1.05 照旧；🟡 用户口径，不是复刻）—— 走 `signal_core_alice_power_spend()`，
 *   其余（消弹 / 伤害源 / 决死 / 荷取退款窗）**一个字都不变**，退款额取**真实扣款**（见下）。
 * ⚠ 扣完**必须** `ce_repopulate_options()`（Tsukasa / 商店先例）。
 * ★ 2026-09-15 一手对齐（常量块有全表）：伤害 = 32/帧 × 75 帧 × 半 20×30（th11 灵梦B/C・魔理沙B/C）；
 *   **震屏已删**（th11 一手找不到 —— 见常量块的说明）；清弹半径仍是我们自定的全屏量级（❓ TUNING）。
 *
 * ## ★★ 2026-09-15c **决死形态**（「决死 B」；用户口径「灵击==B」「所以灵击也得决死」）
 *   库存 == 0 且**自机在决死窗口里**（`player+0x476ac == 4`，一手 `th18:0x45d4e4`）时按 B
 *   ⇒ 本发灵击走**决死形态**：**照常发动**（扣 1 档火力 / 消弹 / 伤害源，节奏一字不改）+
 *   **调 `Player__cancel_impending_death` `th18:0x45d590` 救命**（`signal_cancel_impending_death()`）。
 *   三件事与主动灵击不同（依据全在 `signal_core.h` 的「灵击 → 决死」块，**别在这里重复推理**）：
 *     · **消弹照常记账** ✅ 一手（开关 = `SIGNAL_BOMB_BY_HIT_ACCOUNT` = 1；地址链见 `signal_core.h`）；
 *     · **不扣信仰** —— TH11 的信仰惩罚只走 miss 那条路（`+0x0c`）；救下来 ⇒ 窗口不耗尽 ⇒
 *       引擎不广播 `+0x0c` ⇒ 这条 miss 账**天然不触发**（所以不是「额外免一笔」，是结构上不会发生）；
 *     · **决死形态照样起泡、但不开退款窗**（2026-09-16q 拆开：泡 = 一手
 *       `Bomb::start__marisa_c` 无条件做的事；退款窗 = ❓ 自定，仍只在主动那一支开）。
 *   ★★ 2026-09-16q **本卡现在挂 `on_death_before_deathbomb`（`+0x10`）了，但只为「河童罩子」**：
 *     罩子活着时挨的那一下由**泡**吃掉（一手 `th11:0x4078c4 mov [player+0x928],1` = 撤销中弹），
 *     与「决死 B」那套（玩家在窗口内**按 B**）是两件事 —— 详见下面「河童罩子」那一段。
 *     救援闩锁照 107/117 的约定**无条件 claim**（返回值丢弃）。
 *   ⚠ 🟡 **未采用**（留档，别当成已实现）：TH11 另有一条 `th11:0x4314a9` 的伤害源三元组
 *     `(32.0f, 150 帧, 半 30×30)`（`push 0x96` / `0x497ff0` / `push 0x1e`，同处还写 `GAME_SPEED = 1.0`），
 *     研究稿标注为「被弹（复仇）bomb」；**它属于哪条路本轮没钉死** ⇒ 决死形态**仍用**普通那套
 *     `(32, 75, 20×30)`（= 「节奏不变」）。钉死之后再决定要不要按形态换参数。
 *   ★ **帧内时序（一手登记号）**：卡的 tick 在 **AbilityManager = 更新线 `0x16`**，`Player__on_tick`
 *     （决死窗口 + `+0x0c` 广播）在 **Player 线 `0x17`** ⇒ **卡的 tick 先跑**（与 110 同一批事实）。
 *     两个后果：① 中弹那一帧认不出窗口（tick 已经跑过）⇒ 最早下一帧生效，窗口 8 帧够用；
 *     ② 救命**早于**同一帧 Player 线的扣命判定 ⇒ 引擎走 case 1 ⇒ 不扣命、不广播 `+0x0c`
 *     （= 本卡 `on_death_after_deathbomb` 的 miss 记账天然不触发，正是我们要的）。*/

/* ======================================================================================
 * ★★★ 2026-09-16q：**河童罩子**（魔理沙的荷取特技）—— 一手全链在 `signal_core.h` 的 ⑥ 号块
 * ======================================================================================
 * 用户口径（2026-09-16 逐字）：
 *   「放B是消弹，给一个**防护罩（有贴图特效）**，只要防护罩不撞认为是 no miss。
 *     然后加强一下吧，对于**原生的B**，河童特效也会引入**不撞破罩子 no miss 和一个罩子**」
 *
 * **一手（TH11，本轮逐条复核过，与 `signal_core.h` 的对照表同一批事实）**：
 *   · 放 B 那一帧（`Bomb::start__marisa_c` `th11:0x407680`）起一支 **`pl01.anm` script58 的水泡**
 *     （`th11:0x4076e7 push 0x16` / `0x4076ec push 0x3a` / `0x407705 call 0x455b10`），
 *     并置 `player+0x8bc4` bit2（`th11:0x407714`）；
 *   · **每帧把泡钉在自机身上**（`th11:0x407866`–`0x407892`）⇒ 卡侧 `ce_anm_set_pos` 每帧写；
 *   · **一手不给无敌**：全 exe 的 `set_iframes`（`0x406860`）调用点里，荷取这支只在**破罩之后**
 *     （`th11:0x407963`）出现 ⇒ 罩子活着时是「**挡一次**」，不是无敌；
 *   · **被撞** ⇒ 撤销这一下中弹（`th11:0x4078c4 mov [player+0x928],1`，不扣命）+
 *     泡播破罩动画（`anm_trigger_leave_by_id`）+ 30 帧外扩消弹圈（32→322，+10/帧）+
 *     每帧刷 40 帧无敌 + 一次性 75 帧伤害源（32/帧，半宽 20 × 半高 30）；
 *   · **没被撞** ⇒ 泡的 VM 走完 920 帧自删（`th11:0x407828 test esi,esi` 那一支）⇒
 *     `collect_power(10)`（`th11:0x40784d`）= **退半档**（`BOMB_COST` 20 的一半）+ SE 0x33。
 *
 * **我们在 TH18 的接法（❓ 自定项逐条给理由）**：
 *   · **泡的贴图/曲线/寿命/破罩动画 = 一手**（脚本 294 = `pl01.anm script58` 逐条移植；
 *     层 `layer(5)`→`13` 是本仓实跑过的世界层，见脚本头的 ❓ 段）；
 *   · **判据 = 「泡的 VM 还在不在」**（一手同款）⇒ 卡侧每帧 `ce_anm_get_vm()`，
 *     没了就 `signal_nitori_expire()`（顺带 `step()` 作寿命兜底）；
 *   · **「被撞」的见证点 = `on_death_before_deathbomb`（卡虚表 `+0x10`）**：一手那句
 *     `mov [player+0x928],1` 的前提正是「玩家已经被判中弹（state 4）」；
 *     我们**不给罩子期间的持续无敌**（照一手）⇒ 中弹会真的走到 `Player__die` ⇒ `+0x10` 一定被叫到。
 *     ⚠ 这里**不能**用 `ce_no_hit_declare` 那一层（113 的灵界闸门）：它跳过整个 `Player__die` 体
 *     ⇒ 一手那条「照样进 state 4、但这一下被撤销」的语义会丢掉。
 *   · **原生 B 那条**（用户点名要的）：`on_bomb_spent`（站点 `th18:0x4203bc`，扣完弹、`begin` 之前）
 *     起同一支泡 + 同一个窗 ⇒ 原生 B 期间自机本来无敌（一手 `BombMarisa__begin` 给 120 帧、
 *     之后每帧刷 40），罩子在**无敌结束后**继续挡一次，判据与退款与主动灵击完全同形。
 *   · **额度 = 半档**（不是照抄 10 这个数）：一手 `BOMB_COST` = 20 单位（`pl*.sht+0x24`）⇒ 退 10
 *     = **退一半**；我们 111 的主动灵击扣 1 档 = 100 ⇒ 退 **50**。
 *     ⚠ 原生 B 走的是**炸弹库存**（不是火力）⇒ 这里退 50 火力是**我们自己的口径**
 *     （用户「不撞破罩子 no miss」的落点），登记 `TUNING.md`。
 *   · **建 VM 只能在 tick / 帧末**（`senro.c` 的死亡栈纪律）：`+0x10` 与 `on_bomb_spent` 都只置
 *     `pending` 位，真正的 `ce_anm_spawn` 一律发生在 `signal_shield_tick()`（`on_tick_2` 里）。
 *   · **删卡 / 换关 / 局末**：`signal_shield_kill()` 收干净（长持续效果铁律 —— 删卡发生在商店里，
 *     那一刻只有 `on_delete` 跑得到）。
 *
 * ★★ 2026-09-16u（用户口径逐字）：「**B 的无敌时间不算做罩子的持续时间**，此时撞任何弹幕都应该
 *   认为是没撞，**包括其他的无敌、类无敌情况**」：
 *   · **寿命按暴露帧计**：`signal_nitori_clock_runs(无敌计时器 / 无伤声明 / 自机状态)` 为 0 的那些帧
 *     **不推进** `s_nitori.frames_left`（泡冻结在原地、但仍跟着自机走）；
 *   · 「此时撞任何弹幕都算没撞」在结构上已经成立：无敌期子弹**根本不判定**（引擎的判定函数在
 *     `state ∈ {2,3,4}` / 无敌计时器 > 0 时就早退），无伤声明期 `Player__die` 整个被跳过
 *     ⇒ 我们的 `+0x10` 钩子（破罩的唯一入口）**一次都不会被叫到**；
 *   · 曲线（alpha / scale）也因此**从脚本搬到卡侧**：VM 的时间标签是绝对的、没法暂停
 *     （理由写在脚本 294 的头注释里），卡侧按暴露年龄求一手那三条指令的等价式。*/
/* ★ 破罩那一帧要不要把**信仰槽清零**（一手：TH11 的 `0x4a56f4` 被 state-4 分支清零 ⇒ **要**）。
 *   ⚠ 只清槽、**不写信仰 −100000**（那是真扣命那条路 `Player::sub_4327d0` 里的事）。
 *   标 ❓：这是「一手有对应物、但我们把它的落点选在罩子这条路」的一次判断 —— 见 `TUNING.md` §2.5。*/
#define SIGNAL_NITORI_BREAK_RESETS_GAUGE 1

typedef struct {
    uint32_t vm;            /* 水泡的 VM（脚本 294）；0 = 没起 / 已经没了 */
    uint32_t break_vm;      /* 破罩中的那支 VM（一手：破罩 30 帧后引擎显式删它，`th11:0x407980`）*/
    int32_t  spawn_pending; /* 1 = 本帧要起泡（建 VM 只在 tick 里干）*/
    int32_t  break_pending; /* 1 = 刚刚被撞（在 `+0x10` 里置；tick 里真播破罩 + 建伤害源）*/
    int32_t  break_left;    /* 破罩特写还剩几帧（一手 30）；>0 = 正在破 */
    int32_t  life_left;     /* ★ 寿命（**暴露帧**）：无敌的帧不推进；0 = 该收尾了 */
    int32_t  intro;         /* ★ 出场动画（real 帧，封顶 20）：泡必须**立刻看得见**，见下 */
    float    rot;           /* ★ 自转角 rz（每 real 帧 −3°，一手 `angleVel(0,0,-0.05235988f)`）*/
    int32_t  frames;        /* 诊断：泡活了多久（**暴露帧**：无敌的帧不推进）*/
    int32_t  paused;        /* 诊断：因为无敌 / 类无敌而被跳过的帧数（不进寿命）*/
    int32_t  breaks;        /* 诊断：破罩（被撞）次数 */
} signal_shield_t;

/* ★★ 2026-09-16u 三个常量（都在这一处，别散开写）：
 *   · `SHIELD_ROT_STEP` = 一手 `pl01.anm` script58 的 `angleVel(0, 0, -0.05235988f)`（= −3°/帧）；
 *     **由卡侧驱动**（`ce_anm_set_rotation`）：脚本那一条被 `stop()` 停掉时间线之后是否还生效
 *     没有实证，而用户口径点名「持续中的罩子应当是**逆时针旋转**」⇒ 卡侧写死最稳（负角 = 逆时针）。
 *   · `SHIELD_INTRO_FRAMES` = 一手的出场段（`scaleTime(20, 0, 1.0f, 1.0f)` 那 20 帧）**按 real 帧**走：
 *     否则「原生 B 起泡、B 的无敌期里寿命不推进」会让泡永远停在 scale 0 / alpha 0 ⇒ **看不见**
 *     （实跑日志实证：`暴露 0 帧、暂停 300+ 帧` 而 scale/alpha 还是第 0 帧的值）。
 *   · 视觉年龄 = `intro + 暴露帧`（`intro` 封顶 20）⇒ 无敌期里泡是**长大到 1.0 后冻住**，不是没出现。*/
#define SIGNAL_SHIELD_REFUND_SE      0x30   /* `se_bonus2.wav`（一手 TH11 id 0x33 ↔ TH18 0x30）*/
#define SIGNAL_SHIELD_BREAK_SE       0x2c   /* `se_slash.wav`（一手 TH11 id 0x26 ↔ TH18 0x2c，与灵击同 wav）*/
#define SIGNAL_SHIELD_ROT_STEP       (-0.05235988f)
#define SIGNAL_SHIELD_INTRO_FRAMES   20

static signal_shield_t s_shield;

/* ★★ 2026-09-16u：**文·超高速残像的池**（为了「时间渐变 = 位置渐变」）。
 * 用户口径：「灵梦的残影看起来是红色，原版更贴洋红**并且有时间渐变**（等价于不同残影的位置渐变），
 * 目前缺乏渐变」。一手（TH11 `pl00.anm` script19）：每支残像自己 `color(255,64,64)` →
 * `colorTime(15, 0, 64,64,255)`（在**它自己的 15 帧**里红→蓝）+ `alphaTime(15,0,0)` + `+15: delete()`。
 * ⇒ 每支残像的颜色只跟**它自己的年龄**有关 ⇒ 卡侧必须记住每支的年龄（原来只写一次起手色 ⇒ 整条全红）。
 * ⚠ 我们用的是 **TH18 原生**残像脚本（`pl0X.anm` script17/19/14/14，寿命 20 帧），它的淡出是脚本自己的
 *   `alphaTime` 写 color_1.A ⇒ 卡侧每帧**只换 RGB、保留 A**（`& 0xff000000`），否则会把淡出按掉。*/
#define SIGNAL_TRAIL_POOL   40          /* 同时在场最多约 20 支（20 帧寿命 × 每帧 1 支）⇒ 40 格够翻倍余量 */
typedef struct {
    uint32_t id;
    int32_t  age;
} signal_trail_t;
static signal_trail_t s_trail[SIGNAL_TRAIL_POOL];
static int32_t        s_trail_n;
/* 荷取的退款账（泡的窗）：与 `s_shield` 一起放在文件静态（卡的 496 字节上限）。*/
static signal_nitori_t  s_nitori;

/* 结算这一发罩子（定义在下面；`arm` 换泡时也要用 ⇒ 先声明）。*/
static void signal_shield_settle(signal_card_t *s, int32_t back, const char *why);

/* 收干净（删卡 / 换关 / 局末 / 换机体）。幂等。*/
static void signal_shield_kill(void)
{
    if (s_shield.vm) {
        ce_anm_delete(s_shield.vm);
        s_shield.vm = 0;
    }
    if (s_shield.break_vm) {                  /* 破罩中的那支也要收（它可能还在演那 30 帧）*/
        ce_anm_delete(s_shield.break_vm);
        s_shield.break_vm = 0;
    }
    memset(&s_shield, 0, sizeof s_shield);
}

/* 起泡 + 开窗（`cost` = 这一发真扣掉的**火力单位**，原生 B 那条传 0；
 * `with_refund` = 要不要给这一发开「没被撞就退一半」的窗）。
 * ⚠ 真正的 VM 出生在 `signal_shield_tick()` —— 两个调用点（`+0x10` / `on_bomb_spent`）
 *   都可能落在引擎的**死亡栈 / 炸弹栈**里（`senro.c` 的纪律）。*/
static void signal_shield_arm(signal_card_t *s, int32_t cost, int with_refund)
{
    /* ★★ 2026-09-16u/v（用户口径）：「如果是能放 B，那么罩子**自动破掉换新的**，当然，得**追踪这个
     *   罩子是消耗没消耗 P**，看情况是否返还，**这个破掉不应影响 no miss 判定**」。
     *   ⚠ 这条路**只有原生 B 那条走得到**：灵击那条在罩子没破时被上面那个闸挡掉了
     *     （用户 2026-09-16v：「如果放的是灵击，那么罩子没破不让放」）。
     *   ⇒ 旧泡**按它自己的记录结算**：它没被撞过、且当时真扣过 P ⇒ 退那一半；否则什么都不退。
     *   ⚠ `expire()` 就是这个语义（它只看 `broken` / `pending`），而且**不**碰 `breaks`
     *     ⇒「自动破掉」不会被算成「被撞」（no miss 判定不受影响）。*/
    if (s_shield.vm || s_shield.spawn_pending) {
        int32_t back = signal_nitori_expire(&s_nitori);
        ce_log("signal/shield: 旧罩子被新的一发 B **自动破掉换新**（不是被撞）");
        signal_shield_settle(s, back, "被新的 B 顶掉（自动换新）");
        if (s_shield.vm) {
            ce_anm_interrupt(s_shield.vm, 1);        /* 让它自己演 30 帧破罩动画后自删 */
            s_shield.vm = 0;
        }
    }
    /* 旧罩子留下的东西一律收干净（**在起新泡之前**）：破罩中的 VM 句柄、还没演完的破罩演出。
     *   ⚠ 为什么要掐掉剩下的破罩帧：那些帧里 tick 会在 ③ 就 `return`（消弹圈/无敌/演出），
     *     新泡那几帧就没人驱动（会停在出生点、scale 还是 0）⇒ 看起来「新泡没出现」。
     *     新的一发 B 自己带无敌，掐掉这几帧不亏。*/
    if (s_shield.break_vm) {
        ce_anm_delete(s_shield.break_vm);
        s_shield.break_vm = 0;
    }
    if (s_shield.break_left > 0) {
        ce_log("signal/shield: 旧罩子还没演完的破罩演出（剩 %d 帧）被新的一发 B 提前结束",
               (int)s_shield.break_left);
        s_shield.break_left = 0;
    }
    s_shield.spawn_pending = 1;
    s_shield.life_left     = SIGNAL_NITORI_SHIELD_FRAMES;   /* 寿命 = 暴露帧（920）*/
    s_shield.intro         = 0;
    s_shield.rot           = 0.0f;
    s_shield.frames        = 0;
    s_shield.paused        = 0;
    if (with_refund) signal_nitori_open(&s_nitori, cost);
    else             signal_nitori_close(&s_nitori);  /* 决死形态：起泡但不退款（❓ 自定，见上）*/
    ce_log("signal: 荷取·起泡（水泡脚本 294，寿命 %d 帧；退款窗 %s，本发扣了 %d 单位 = 半档 %d）",
           (int)SIGNAL_NITORI_SHIELD_FRAMES, with_refund ? "开" : "不开",
           (int)cost, (int)signal_nitori_refund_of(cost));
}

/* ★ 罩子**这一发还在进行中**吗（= 一手 `[bomb+0x3c] != 0`）。
 *   判据（三条任一为真）：泡还在（`vm`）/ 本帧要起泡（`spawn_pending`）/ 正在破罩演出或刚破（`break_*`）。
 *   ★★ 2026-09-16v：**破罩演出那 30 帧也算「进行中」** —— 用户 2026-09-16v「这个按照一手的来」
 *   （一手 `Bomb::on_tick__marisa_c` 要等 `[bomb+0x2c] > 30` 才删 VM 并返回 −1，`[bomb+0x3c]` 才清
 *   ⇒ 演出没演完放不出第二发）。⇒ 灵击那条**整段罩子生命周期内都放不出来**（含被撞之后那 30 帧）。
 *   ⚠ 原生 B 那条**不走这个闸**（用户前一条口径：「能放 B ⇒ 罩子自动破掉换新的」）。*/
static int signal_shield_busy(void)
{
    if (s_shield.vm || s_shield.spawn_pending) return 1;
    if (s_shield.break_pending || s_shield.break_left > 0) return 1;
    return 0;
}

/* 泡吃掉这一下中弹（在 `+0x10` 里调）。返回 1 = 这一下被泡吃了（卡侧已撤销中弹）。*/
static int signal_shield_eat_hit(void)
{
    if (!s_shield.vm && !s_shield.spawn_pending) return 0;   /* 没有泡 ⇒ 本卡不接这一下 */
    if (s_shield.break_pending || s_shield.break_left > 0) return 0; /* 正在破/已经破 ⇒ 不再吃 */
    /* ★★ 2026-09-16u（用户口径）：「这个罩子原则上是**撞弹**的保护，所以**不应当能抵消
     *   **中弹灵界的结束灵界击杀**」⇒ 卡自己造的死（113 的补死等）**不吃、也不破罩**
     *   （通报位 = `ce_forced_death_active()`，见 `sdk_core.h` 的说明块）。*/
    if (ce_forced_death_active()) {
        ce_log("signal/shield: 这一下是**卡自己造的死**（不是撞弹）⇒ 罩子让位（不吃、不破）");
        return 0;
    }
    s_shield.break_pending = 1;
    signal_nitori_break(&s_nitori);                  /* 破罩 ⇒ 这一发不退款（一手同上）*/
    (void)ce_rescue_note_claim(SIGNAL_ID_NUM);        /* 救援闩锁照 107/117 的约定（返回值丢弃）*/
    signal_cancel_impending_death();                  /* ★ 一手 `th11:0x4078c4`：把这一下中弹撤销 */
    return 1;
}

/* 结算这一发罩子（寿命尽头 / VM 消失两条入口共用）：
 * `back` = 该退还给火力的单位数（0 = 不退）；一手额度 = `collect_power(10)` = `BOMB_COST` 的一半。*/
static void signal_shield_settle(signal_card_t *s, int32_t back, const char *why)
{
    int32_t maxp, now, want;

    s->stat_nitori = s_nitori.refunds;
    maxp = SIGNAL_MAX_POWER_VAL();
    now  = CE_CURRENT_POWER();
    want = now + back;
    if (want > maxp) want = maxp;
    if (back > 0) {
        const float *pp = ce_player_pos();
        /* ★★ 2026-09-16u：**退款有音效**（用户口径：「返还东西的罩子返还是有音效的」）。
         *   一手：`th11:0x407845 call 0x44a260`（`play_sound_at_position`），id = `esi+0x33` = **0x33**
         *   = `se_bonus2.wav`（TH11 的 SE 表 `0x4a34f6` 8 字节/项 {WORD 音量, WORD wav 下标}：
         *   id 0x33 → wav 42 → 名字表 `0x4a36b0[42]` = `se_bonus2.wav`）⇒ **同一个 wav 在 TH18 是 0x30**
         *   （`engine/_shared/th18-sound-table.md` 第 208 行）。*/
        ce_play_sound(SIGNAL_SHIELD_REFUND_SE, pp ? pp[0] : 0.0f);
        *(int32_t *)CE_ADDR_CURRENT_POWER = want;
        ce_repopulate_options();                  /* 子机按新档重建（与灵击扣档同一条要求）*/
        ce_log("signal/shield: 罩子结束（%s；暴露 %d 帧 / 暂停 %d 帧 / 被撞 %d 次）**没被撞** ⇒ "
               "退 %d 单位火力（%d → %d，半档；一手 `collect_power(10)` = `BOMB_COST` 的一半）",
               why, (int)s_shield.frames, (int)s_shield.paused, (int)s_shield.breaks,
               (int)back, (int)now, (int)want);
    } else {
        ce_log("signal/shield: 罩子结束（%s；暴露 %d 帧 / 暂停 %d 帧）但**不退款**"
               "（被撞 %d 次 / 本来没扣到）",
               why, (int)s_shield.frames, (int)s_shield.paused, (int)s_shield.breaks);
    }
    s_shield.frames = 0;
    s_shield.paused = 0;
}

/* 每帧推进（`on_tick_2` 里、与机体无关地跑 —— 换机体后也要能把泡收干净）。*/
static void signal_shield_tick(signal_card_t *s)
{
    const float *pp = ce_player_pos();
    float center[3];

    /* ① 出生（延迟到 tick 里做）*/
    if (s_shield.spawn_pending) {
        s_shield.spawn_pending = 0;
        s_shield.frames        = 0;
        s_shield.paused        = 0;
        s_shield.intro         = 0;
        s_shield.rot           = 0.0f;
        s_shield.life_left     = SIGNAL_NITORI_SHIELD_FRAMES;
        s_shield.break_left    = 0;
        s_shield.vm = ce_anm_spawn(CE_ABILITY_ANM(), CE_ANM_ABILITY_SCRIPT_NITORI_BUBBLE, 13);
        if (s_shield.vm && pp) ce_anm_set_pos(s_shield.vm, pp[0], pp[1], 0.0f);
        ce_log("signal/shield: 泡 vm=%u（脚本 %d，层 13）pos=(%.1f,%.1f) —— 曲线（alpha/scale）由卡侧"
               "按**暴露年龄**驱动（无敌的帧不算寿命）",
               (unsigned)s_shield.vm, (int)CE_ANM_ABILITY_SCRIPT_NITORI_BUBBLE,
               pp ? (double)pp[0] : -999.0, pp ? (double)pp[1] : -999.0);
        if (!s_shield.vm) {                           /* 池满 ⇒ 窗也作废，别留下一个永远不退的账 */
            signal_nitori_close(&s_nitori);
            return;
        }
    }
    if (!s_shield.vm) return;

    /* ② 被撞 ⇒ 播破罩动画 + 建那一发伤害源（一手：破罩那一帧只做这两件，消弹圈从本帧开始长）*/
    if (s_shield.break_pending) {
        s_shield.break_pending = 0;
        s_shield.break_left    = SIGNAL_NITORI_BREAK_FRAMES;
        ++s_shield.breaks;
        if (ce_anm_get_vm(s_shield.vm)) ce_anm_interrupt(s_shield.vm, 1);
        /* 泡已经交了：句柄**搬到 `break_vm`**（不能丢）——
         * ⚠ 脚本的 `interruptLabel(1)` 后面那条 `+30: delete()` 是**绝对时间标签**：泡要是在第
         *   200 帧被打掉、它得等到第 950 帧才自删（中间 alpha 已经是 0、看不见，但 VM 槽一直占着）。
         *   一手正是因此**在 30 帧后显式删掉它**（`th11:0x407968 cmp [edi+0x2c],0x1e` →
         *   `th11:0x407980 call 0x4562e0`）⇒ 我们也照做（见下面 ③ 的收尾）。*/
        s_shield.break_vm = s_shield.vm;
        s_shield.vm = 0;
        if (pp) {
            center[0] = pp[0]; center[1] = pp[1]; center[2] = 0.0f;
            /* 一手 `th11:0x40790c`–`0x407923`：伤害 32.0 / 存活 75 帧 / 半宽 20 × 半高 30。
             * ⚠ 一手那两个是**半径**（半宽半高），`ce_damage_rect` 吃的是**全宽全高**（本仓既有
             *   调用点一律 `r * 2.0f`，例 `mof.c:1186`）⇒ 这里 ×2。*/
            if (ce_damage_rect(center, 0.0f, SIGNAL_NITORI_BREAK_DMG_LIFE, SIGNAL_NITORI_BREAK_DMG,
                               (float)SIGNAL_NITORI_BREAK_HALF_W * 2.0f,
                               (float)SIGNAL_NITORI_BREAK_HALF_H * 2.0f)) {
                ce_raise_damage_cap(MOF_BOMB_DMG_CAP);   /* 与灵击同一套上限（同一张卡的两条伤害源）*/
            }
        }
        /* ★ 2026-09-16u：破罩也有音效（一手 `th11:0x407901 call 0x44a1e0`，id **0x26**
         *   = `se_slash.wav`（TH11 表：0x26 → wav 30 → 名字表 = `se_slash.wav`）
         *   ⇒ TH18 同 wav = **0x2c**，与我们的灵击 SE 同一个 wav）。*/
        ce_play_sound(SIGNAL_SHIELD_BREAK_SE, pp ? pp[0] : 0.0f);
        ce_log("signal/shield: **破罩**（被撞 %d 次）⇒ 撤销中弹 + 泡播破罩动画 + %d 帧外扩消弹圈"
               "（%.0f→%.0f px）+ 每帧 %d 帧无敌 + 伤害源 %d/帧 × %d 帧",
               (int)s_shield.breaks, (int)SIGNAL_NITORI_BREAK_FRAMES,
               (double)signal_nitori_break_radius_px(1),
               (double)signal_nitori_break_radius_px(SIGNAL_NITORI_BREAK_FRAMES),
               (int)SIGNAL_NITORI_BREAK_IFRAMES, (int)SIGNAL_NITORI_BREAK_DMG,
               (int)SIGNAL_NITORI_BREAK_DMG_LIFE);
    }

    /* ③ 破罩的 30 帧：消弹圈逐帧外扩 + 每帧刷无敌（一手 `0x407942` / `0x407961`）*/
    if (s_shield.break_left > 0) {
        if (s_shield.break_vm && pp) {              /* 破罩中泡也**跟着自机**（一手 follow 在 VM 还在时照跑）*/
            ce_anm_set_pos(s_shield.break_vm, pp[0], pp[1], 0.0f);
        }
        int32_t k = SIGNAL_NITORI_BREAK_FRAMES - s_shield.break_left + 1;
        float r = (float)signal_nitori_break_radius_px(k);
        center[0] = pp ? pp[0] : 0.0f;
        center[1] = pp ? pp[1] : 0.0f;
        center[2] = 0.0f;
        /* ⚠ **不走 `ce_cancel_latch_*`**：那把闩锁仲裁的是「**不同卡**之间谁来做那一次全屏消弹」
         *   （`EVENTS-CONTRACT.md` §5.2 细则 2 / `sdk_core.h`）。这里的破罩圈与**本卡自己**的灵击圈
         *   是同一张卡的两个同心效果（一手：TH11 的破罩圈半径能从 32 长到 322 px，远大于灵击圈），
         *   拿闩锁当门会出现「灵击还在跑 ⇒ 破罩圈这 30 帧一颗都消不掉」的静默失效。
         *   重复扫描是安全的（第二趟扫到空池，记 0 颗），颗数照 `signal_bomb_form_cancel_note()` 入账。*/
        {
            int n = ce_cancel_radius(center, r, (int)MOF_BOMB_CANCEL_MAX, 1);
            if (n > 0) signal_bomb_form_cancel_note(&s->bomb_form, n);
        }
        ce_invuln_request((uint32_t)SIGNAL_ID_NUM, SIGNAL_NITORI_BREAK_IFRAMES);
        if (--s_shield.break_left == 0) {
            /* ⚠ 这里**不写 0**：一手的 iframes 是「每帧刷 40」，bomb 结束后那 40 帧由引擎自己跑完
             *   （`ce_invuln_apply()` 只在更长时才改计时器，声明消失就等于不再续期）。
             *   写 0 会撤掉**同一张卡**另一条路（恋恋）本帧的声明，得不偿失。*/
            if (s_shield.break_vm) {                  /* ★ 一手的收尾：30 帧后显式删泡的 VM */
                ce_anm_delete(s_shield.break_vm);
                s_shield.break_vm = 0;
            }
            ce_log("signal/shield: 破罩收尾（30 帧走完；泡的 VM 已按一手显式删；无敌计时器自然跑完）");
        }
        return;
    }

    /* ④ 泡活着：每帧钉在自机身上（一手 `th11:0x407866`–`0x407892`）+ 按**暴露年龄**驱动曲线。
     *   ★★ 2026-09-16u/v：三件事分开算（实跑实证过各自为什么必须这么算）：
     *     · **旋转**：每 real 帧 −3°（一手 `angleVel`），卡侧写 `ce_anm_set_rotation`
     *       —— 用户口径「持续中的罩子应当是**逆时针旋转**」；无敌期也照样转。
     *     · **出路（出场动画）**：`intro` 走 real 帧、封顶 20 帧。原生 B 起泡时自机**正处在无敌期**
     *       ⇒ 寿命不推进；若出场也绑寿命，泡会永远停在 scale 0 / alpha 0（实跑：「没有罩子贴图」）。
     *     · **收缩（寿命）**：只走暴露帧 ⇒ 视觉年龄 = `intro + 暴露帧`。*/
    {
        uint8_t *pl = CE_PLAYER();
        ce_timer_t *inv = pl ? CE_PLAYER_INVULN() : 0;
        int32_t invuln_cur = inv ? inv->cur : 0;
        int32_t pstate     = pl ? *(const int32_t *)(pl + CE_PLAYER_STATE) : 1;
        int32_t no_hit     = ce_no_hit_active() ? 1 : 0;
        int32_t runs       = signal_nitori_clock_runs(invuln_cur, no_hit, pstate);
        int32_t vis_age;

        if (s_shield.intro < SIGNAL_SHIELD_INTRO_FRAMES) ++s_shield.intro;
        vis_age = s_shield.intro
                + (SIGNAL_NITORI_SHIELD_FRAMES - s_shield.life_left);   /* 出场 + 暴露 */
        if (!ce_anm_get_vm(s_shield.vm)) {          /* 一手判据：泡的 VM 没了（异常路）*/
            int32_t back;
            ce_log("signal/shield: 泡的 VM 提前没了（暴露 %d 帧 / 暂停 %d 帧）⇒ 按一手判据结算",
                   (int)s_shield.frames, (int)s_shield.paused);
            s_shield.vm = 0;
            back = signal_nitori_expire(&s_nitori);
            signal_shield_settle(s, back, "VM 消失");
            return;
        }
        if (pp) ce_anm_set_pos(s_shield.vm, pp[0], pp[1], 0.0f);   /* 位置**每帧**都跟（不死）*/
        s_shield.rot += SIGNAL_SHIELD_ROT_STEP;                    /* ★ 逆时针自转（每 real 帧）*/
        (void)ce_anm_set_rotation(s_shield.vm, 0.0f, 0.0f, s_shield.rot);
        {   /* 曲线：看得见的形状（露 age）/ 收缩（暴露 age 一起算在 vis_age 里）*/
            float   sc = (float)signal_nitori_bubble_scale_milli(vis_age) / 1000.0f;
            int32_t a  = signal_nitori_bubble_alpha(vis_age);
            (void)ce_anm_set_scale(s_shield.vm, sc, sc);
            /* alpha 落在 color_1 的 A 字节上（`sdk.h` 的顶点色两路说明）：RGB 保持白 ⇒ 贴图原色 */
            (void)ce_anm_set_color(s_shield.vm, 0xffffffu | ((uint32_t)a << 24));
        }
        if (!runs) {
            if (s_shield.paused == 0 || (s_shield.paused % 300) == 299) {
                ce_log("signal/shield: 罩子**暂停计时**（暴露 %d 帧、暂停 %d 帧、寿命剩 %d）：%s%s自机状态 %d",
                       (int)s_shield.frames, (int)(s_shield.paused + 1), (int)s_shield.life_left,
                       invuln_cur > 0 ? "无敌计时器 " : "", no_hit ? "无伤声明 " : "", (int)pstate);
            }
            ++s_shield.paused;
            return;
        }
        ++s_shield.frames;
        if (--s_shield.life_left > 0) return;       /* 暴露帧还剩 ⇒ 继续 */
        {   /* 寿命尽头（暴露年龄满 920 帧）= 一手的「泡自己走完」那一刻 */
            int32_t back = signal_nitori_expire(&s_nitori);
            ce_anm_delete(s_shield.vm);
            s_shield.vm = 0;
            signal_shield_settle(s, back, "寿命尽头");
        }
    }
}

/* 诊断一行（实跑对账用；`signal/fx:` 那行的姊妹行）。*/
static void signal_shield_dump(const char *tag)
{
    uint8_t *vm = s_shield.vm ? ce_anm_get_vm(s_shield.vm) : 0;
    ce_log("signal/shield: %s vm=%u alive=%d break_vm=%u 寿命剩=%d intro=%d 暴露=%d 暂停=%d break_left=%d breaks=%d | "
           "账 active=%d broken=%d pending=%d refunds=%d denials=%d",
           tag, (unsigned)s_shield.vm, vm ? 1 : 0, (unsigned)s_shield.break_vm,
           (int)s_shield.life_left, (int)s_shield.intro, (int)s_shield.frames, (int)s_shield.paused,
           (int)s_shield.break_left, (int)s_shield.breaks,
           (int)s_nitori.active, (int)s_nitori.broken, (int)s_nitori.pending,
           (int)s_nitori.refunds, (int)s_nitori.denials);
}

/* ======================================================================================
 * ★★★ 2026-09-16m：**110 型灵击**（逐帧消弹圈 + 每敌每帧伤害 + 范围环 + 火星）
 * ======================================================================================
 * 用户口径见上面常量块。**每一条都对位 `mof.c` 的 `mof_bomb_start()` / `mof_bomb_tick()`**：
 *   · 消耗 = `mof_core_power_spend()`（≥ 2.00 档 / 扣 1 档 / 下限 1.05 —— 与 110 同一个共用件）
 *   · 半径 = `mof_core_bomb_radius_centi()`（16.00 → 160/112、120 帧线性；一手 TH10 `bullet.anm` script400/407）
 *   · 中心上升 = `mof_core_bomb_rise_milli()`（0.5 / 1.0 / 1.3 px 每帧）
 *   · 伤害 = `mof_core_bomb_damage()`（圈内档 / 圈外档，互斥；每帧一个新源 ⇒ 每敌每帧一笔）
 *   · 时长 = `MOF_BOMB_FRAMES`（296）；消弹 = 每帧 `ce_cancel_radius(中心, r, 上限, mode 0)`
 *   · 音效 = `SIGNAL_BOMB10_SE`（`se_slash.wav`）；视觉 = 脚本 279（环）+ 280/281（火星）*/

/* 收干净：删卡 / 换关 / 局末都要（脚本 279 自己不会死）。*/
static void signal_bomb10_kill(void)
{
    if (s_b10.ring_vm) {
        ce_anm_delete(s_b10.ring_vm);
        s_b10.ring_vm = 0;
    }
    if (s_b10.strip_vm) {                    /* ★ 2026-09-16p：竖条环一起收 */
        ce_anm_delete(s_b10.strip_vm);
        s_b10.strip_vm = 0;
    }
    memset(&s_b10, 0, sizeof s_b10);
}

/* ★ 诊断（2026-09-16m 新增）：把环 VM 的关键字段打一行 —— 用户两次报「起了 VM 却看不见」，
 * 下次实跑一眼就能分辨「字段不对（我们的 bug）」还是「字段都对但不画（图元/层的问题）」。
 * 对照物 = `signal/opt:` 那行（同一批字段，那支 VM **实跑看得见**）。*/
static void signal_bomb10_fx_dump(const char *tag)
{
    uint8_t *vm = s_b10.ring_vm ? ce_anm_get_vm(s_b10.ring_vm) : 0;

    if (!vm) {
        ce_log("signal/fx: %s ring vm=%u **alive=0**（VM 已经不在了）",
               tag, (unsigned)s_b10.ring_vm);
        return;
    }
    ce_log("signal/fx: %s ring vm=%u sprite=%d scale=(%.2f,%.2f) pos=(%.1f,%.1f) "
           "c1=%08x c2=%08x frames=%d",
           tag, (unsigned)s_b10.ring_vm,
           (int)*(const int32_t *)(vm + CE_ANM_VM_SPRITE_ID),
           (double)((const float *)(vm + CE_ANM_VM_SCALE))[0],
           (double)((const float *)(vm + CE_ANM_VM_SCALE))[1],
           (double)((const float *)(vm + CE_ANM_VM_POS))[0],
           (double)((const float *)(vm + CE_ANM_VM_POS))[1],
           *(const uint32_t *)(vm + CE_ANM_VM_COLOR1),
           *(const uint32_t *)(vm + CE_ANM_VM_COLOR2),
           (int)s_b10.ring_frames);
}

/* 灵击发动（= 110 的 `mof_bomb_start()`）。`by_hit` = 决死形态（救命）。*/
static void signal_bomb10_start(signal_card_t *s, int32_t by_hit)
{
    const float *pp = ce_player_pos();
    /* ★★ 2026-09-16m（用户追加口径）：「**爱丽丝也得是 0.5P**，这个我忘说了」
     *   ⇒ 魔理沙机体的灵击消耗 = **0.5 档（50 单位）**（爱丽丝那条，🟡 用户口径）；
     *     其余机体与其他一切（半径 / 消弹 / 伤害 / 时长 / 音效 / 视觉）**都是 110 的**。
     *   `signal_core_alice_power_spend(power, alice_on)` 在 `alice_on = 0` 时**原样转发**
     *   `mof_core_power_spend()`（≥2.00 档 / 扣 1 档 / 下限 1.05）⇒ 别的机体与 110 一字不变。
     *   ⚠ 这条与「河童退款」是**两条不同的**额外效果，别混：一个是**扣多少**、一个是**退不退**。*/
    const int alice = (*(const int32_t *)CE_ADDR_CHARACTER == SIGNAL_CHAR_MARISA);
    int32_t power_before = CE_CURRENT_POWER();
    mof_power_t pw = signal_core_alice_power_spend(power_before, alice);
    int32_t cost;
    int32_t r;
    int spell_now = ce_spell_active();
    int reimu = (*(const int32_t *)CE_ADDR_CHARACTER == SIGNAL_CHAR_REIMU);

    /* ★★ 2026-09-16v（用户口径逐字）：「**如果放的是灵击，那么罩子没破不让放**」。
     *   一手：TH11 的 `Player::do_bomb` `th11:0x40651b` 在 `[bomb+0x3c] != 0`（这一发 bomb 还在进行中）
     *   时直接 `or eax,-1` 返回 −1，调用侧 `th11:0x4311c7 cmp [bomb+0x3c],0; jne 跳过` ——
     *   而荷取的「bomb 进行中」**就是那支水泡还在**（泡走完 920 帧或被打掉之前，
     *   `Bomb::on_tick__marisa_c` 一直返回 0）⇒ 罩子没破时**放不出第二发**。
     *   ⚠ 本闸**只管灵击这一条**（库存 == 0 时本卡自干）：原生 B 是 TH18 的库存系统，
     *     按用户前一条口径走「**能放 B ⇒ 罩子自动破掉换新的**」（见 `signal_shield_arm()`）。
     *   ⚠ 拒绝 = **什么都不做**：不扣火力、不消弹、不落 RANK、不响音效（放在扣火力之前）。*/
    if (alice && signal_shield_busy()) {
        ce_log("signal: 灵击**被罩子挡住**（河童泡还在进行中：暴露 %d 帧 / 暂停 %d 帧 / 破罩演出剩 %d 帧）"
               "⇒ 这一下不放（一手 `th11:0x40651b` 的 `[bomb+0x3c] != 0` ⇒ `do_bomb` 返回 −1）",
               (int)s_shield.frames, (int)s_shield.paused, (int)s_shield.break_left);
        return;
    }

    if (!pp || !pw.ok) {                                 /* 火力不够 ⇒ 什么都不做（照原作）*/
        if (by_hit) ce_log("signal: 灵击（决死）**没发动**：火力 %d 不够门槛（power_ok=0）⇒ 本次不救命",
                           (int)CE_CURRENT_POWER());
        return;
    }
    cost = power_before - pw.new_power;

    signal_bomb_form_start(&s->bomb_form, by_hit);       /* 记形态（by_hit + 消弹颗数归零）*/
    *(int32_t *)CE_ADDR_CURRENT_POWER = pw.new_power;    /* 自写扣法（原生会拒绝、且只能整档扣）*/
    ce_repopulate_options();                             /* 子机按新档重建（必须）*/

    /* 决死形态：**救命**（`th18:0x45d590`）—— 位置与 110 同一处（门槛过了、火力也扣了）。*/
    if (signal_bomb_form_is_deathbomb(&s->bomb_form)) {
        signal_cancel_impending_death();
        ++s->stat_bomb_dbf;
    }

    /* ⑤ 音效（= 110 的 `MOF_BOMB_SE`：TH10 `se_slash.wav` ↔ TH18 同 wav `0x2c`）*/
    ce_play_sound(SIGNAL_BOMB10_SE, pp[0]);

    /* RANK −0x80（钳 ±0x400）—— 照抄 110 的写法（TH18 里该字段是死字段，写了没有实际效果）*/
    {
        int32_t rk = *(volatile int32_t *)SIGNAL_BOMB10_RANK_ADDR - SIGNAL_BOMB10_RANK_DROP;
        if (rk > SIGNAL_BOMB10_RANK_MAX) rk = SIGNAL_BOMB10_RANK_MAX;
        if (rk < SIGNAL_BOMB10_RANK_MIN) rk = SIGNAL_BOMB10_RANK_MIN;
        *(volatile int32_t *)SIGNAL_BOMB10_RANK_ADDR = rk;
    }

    /* 本发的曲线参数（= 110：符卡中 64.0；否则灵梦 160.0、其他 112.0）*/
    s_b10.target_centi = spell_now ? MOF_BOMB_R_SPELL_CENTI
                                   : (reimu ? MOF_BOMB_R_REIMU_CENTI : MOF_BOMB_R_MARISA_CENTI);
    s_b10.rise_milli  = mof_core_bomb_rise_milli(spell_now, reimu);
    s_b10.reimu       = reimu;
    s_b10.x           = pp[0];
    s_b10.y           = pp[1];
    s_b10.left        = MOF_BOMB_FRAMES;
    s_b10.elapsed     = 0;
    s_b10.frames      = 0;
    s_b10.dmg_frames  = 0;
    s_b10.ring_fading = 0;
    s_b10.ring_spark  = 0;
    s_b10.ring_frames = 0;

    /* 范围环（脚本 279）：半径逐帧给 px（起点 = TH10 的 16.00 px）*/
    r = mof_core_bomb_radius_centi(0, s_b10.target_centi);
    s_b10.ring_vm = ce_anm_spawn(CE_ABILITY_ANM(), CE_ANM_ABILITY_SCRIPT_BOMB_CLEAR_RING, 13);
    if (s_b10.ring_vm) {
        ce_anm_set_pos(s_b10.ring_vm, s_b10.x, s_b10.y, 0.0f);
        ce_anm_set_scale(s_b10.ring_vm, (float)r * 0.01f, (float)r * 0.01f);
    }
    /* ★ 2026-09-16p：**电流边**（与 110 同一条脚本 293；用户：「th10 的灵击…边缘有类似电流的特效」
     *   —— 原版是 `sprite(sprite362)` + `texCircle(32)` 把 14×128 竖条弯成圈）。🟡 texCircle 无先例。*/
    s_b10.strip_vm = ce_anm_spawn(CE_ABILITY_ANM(), CE_ANM_ABILITY_SCRIPT_MOF_BOMB_RING_STRIP, 13);
    if (s_b10.strip_vm) {
        ce_anm_set_pos(s_b10.strip_vm, s_b10.x, s_b10.y, 0.0f);
        ce_anm_set_scale(s_b10.strip_vm, (float)r * 0.01f, (float)r * 0.01f);
    }
    signal_bomb10_fx_dump("起");

    ++s->stat_bomb;
    if (alice) ++s->stat_alice;                          /* 诊断：本发走的是**爱丽丝 0.5 档**那条 */
    /* ★★ 2026-09-16q：魔理沙侧多一个**河童罩子**（用户口径「放B是消弹，给一个防护罩」）——
     *   一手（`Bomb::start__marisa_c` `th11:0x407680`）里泡是**无条件**起的（`0x407705 call 0x455b10`），
     *   退款窗则是「没被撞才退」（`0x40784d collect_power(10)`）⇒ 我们拆成两件事：
     *   **泡**：魔理沙一定起；**窗**：只有主动那一支开（决死形态那条 ❓ 自定维持原样）。*/
    if (alice) {
        signal_shield_arm(s, cost, signal_bomb_form_opens_refund(&s->bomb_form));
    }
    signal_declare_score(s);
    ce_log("signal: 灵击%s -%d.%02d 档 (power %d -> %d), 半径 16.0 -> %.1f px / %d 帧, 时长 %d 帧",
           signal_bomb_form_is_deathbomb(&s->bomb_form) ? "（决死）" : "",
           (int)(cost / 100), (int)(cost % 100),
           (int)power_before, (int)pw.new_power,
           (double)s_b10.target_centi * 0.01, (int)MOF_BOMB_R_GROW_FRAMES, (int)MOF_BOMB_FRAMES);
}

/* 每帧推进（= 110 的 `mof_bomb_tick()`）。*/
static void signal_bomb10_tick(signal_card_t *s)
{
    float center[3];
    float r;
    int32_t r_centi, dmg_in, dmg_out, dmg_src, n;
    int boss_alive, spell_now, big_bomb, reimu;

    /* 符卡帧计数（对位 110 的 `s->spell_frames`：`big_bomb` = 符卡中且已过 60 帧）*/
    if (ce_spell_active()) {
        if (s_b10.spell_frames < 1000000) ++s_b10.spell_frames;
    } else {
        s_b10.spell_frames = 0;
    }

    if (s_b10.left <= 0) return;

    /* 中心先上移，再拿它消弹/放伤害源（= 110 的顺序，一手 `th10:0x4057b6`–`0x405816`）*/
    s_b10.y -= (float)s_b10.rise_milli * 0.001f;
    r_centi = mof_core_bomb_radius_centi(s_b10.elapsed, s_b10.target_centi);
    r = (float)r_centi * 0.01f;
    center[0] = s_b10.x;
    center[1] = s_b10.y;
    center[2] = 0.0f;

    /* ① 消弹（半径 = 本帧的 r；mode 0 = 纯删除；闩锁口径与 110 相同）
     * ★★ 2026-09-16p：与 110 一字同形 —— **先在圈内每颗弹自己的位置撒一颗信仰点并自动收集**
     *   （用户口径「th10 灵击消弹应当是掉落中信仰点并自动收集的」；共享件
     *   `faith_cancel_drop_dots()`，一手的地址链写在它上面），再删弹。
     *   ⚠ 不再 `faith_cancel_account(n)` 直接进账（那些信仰由吸进来的点在拾取回调里入账）。*/
    if (ce_cancel_latch_claim((uint32_t)SIGNAL_ID_NUM)) {
        /* ★ 2026-09-16q：与 110 一字同形 —— **mode 1**（用户口径：「微档点可以完全认为是 th18 消弹点吧」），
         *   消弹点由引擎撒；不再自建撒点、也不再直接记账（信仰走那些点的拾取回调）。*/
        n = ce_cancel_radius(center, r, (int)MOF_BOMB_CANCEL_MAX, 1);
        if (n > 0) {
            signal_bomb_form_cancel_note(&s->bomb_form, n);
            if (!signal_bomb_form_cancel_accounts(&s->bomb_form)) s->stat_cancel += n;
        } else {
            (void)ce_cancel_latch_release((uint32_t)SIGNAL_ID_NUM);
        }
        if ((s_b10.elapsed % 30) == 0) {
            ce_log("signal: 灵击消弹圈 r=%.2f 本帧 cancelled %d（mode 1 = 引擎撒消弹点，与 110 同口径）",
                   (double)r, (int)n);
        }
    }

    /* ② 伤害：每帧一个新源（圈内档 / 圈外档互斥，与 110 逐条同形）*/
    boss_alive = 0;
    {
        uint8_t *em = CE_ENEMY_MGR();
        if (em) boss_alive = *(const int32_t *)(em + CE_EM_BOSS_IDS) != 0;
    }
    spell_now = ce_spell_active();
    big_bomb  = spell_now && s_b10.spell_frames >= (int32_t)MOF_BOMB_BIG_BOMB_FRAMES;
    reimu     = s_b10.reimu;

    dmg_in  = mof_core_bomb_damage(1, boss_alive, spell_now, big_bomb, reimu);
    dmg_out = (boss_alive || spell_now) ? mof_core_bomb_damage(0, boss_alive, spell_now, big_bomb, reimu) : 0;
    if (dmg_in > 0 || dmg_out > 0) ce_raise_damage_cap(MOF_BOMB_DMG_CAP);

    dmg_src = dmg_in - dmg_out;
    if (dmg_src > 0) {
        float side = r * 2.0f;
        if (ce_damage_rect(center, 0.0f, SIGNAL_BOMB10_DMG_SRC_LIFE, dmg_src, side, side)) ++s_b10.dmg_frames;
    }
    if (dmg_out > 0) {
        float pc[3];
        pc[0] = SIGNAL_BOMB10_FIELD_CX; pc[1] = SIGNAL_BOMB10_FIELD_CY; pc[2] = center[2];
        if (ce_damage_rect(pc, 0.0f, SIGNAL_BOMB10_DMG_SRC_LIFE, dmg_out,
                           SIGNAL_BOMB10_FIELD_W, SIGNAL_BOMB10_FIELD_H)) ++s_b10.dmg_frames;
    }

    /* ③ 范围环：逐帧跟着「本帧的真半径」长；长满那一帧起 20 帧淡出（= TH10 的 120 帧成长）*/
    if (s_b10.ring_vm) {
        if (!ce_anm_get_vm(s_b10.ring_vm)) {
            s_b10.ring_vm = 0;
        } else if (s_b10.ring_fading) {
            if (s_b10.ring_frames > MOF_BOMB_R_GROW_FRAMES + 30) {
                ce_anm_delete(s_b10.ring_vm);
                s_b10.ring_vm = 0;
            } else {
                ++s_b10.ring_frames;
            }
        } else {
            if (s_b10.elapsed >= MOF_BOMB_R_GROW_FRAMES) {
                ce_anm_interrupt(s_b10.ring_vm, 1);
                if (s_b10.strip_vm && ce_anm_get_vm(s_b10.strip_vm)) {
                    ce_anm_interrupt(s_b10.strip_vm, 1);                 /* ★ 竖条环一起淡出 */
                }
                s_b10.ring_fading = 1;
            }
            ce_anm_set_pos(s_b10.ring_vm, center[0], center[1], center[2]);
            (void)ce_anm_set_scale(s_b10.ring_vm, r, r);
            if (s_b10.strip_vm && ce_anm_get_vm(s_b10.strip_vm)) {           /* ★ 竖条环同驱 */
                ce_anm_set_pos(s_b10.strip_vm, center[0], center[1], center[2]);
                (void)ce_anm_set_scale(s_b10.strip_vm, r, r);
            }
            ++s_b10.ring_frames;
            if (s_b10.elapsed == MOF_BOMB_R_GROW_FRAMES + 2) signal_bomb10_fx_dump("长满后");
            if (++s_b10.ring_spark >= SIGNAL_BOMB10_SPARK_EVERY) {
                uint32_t red  = ce_anm_spawn(CE_ABILITY_ANM(), CE_ANM_ABILITY_SCRIPT_MOF_BOMB_SPARK, 13);
                uint32_t blue = ce_anm_spawn(CE_ABILITY_ANM(), CE_ANM_ABILITY_SCRIPT_MOF_BOMB_SPARK_BLUE, 13);
                s_b10.ring_spark = 0;
                if (red)  ce_anm_set_pos(red,  center[0], center[1], 0.0f);
                if (blue) ce_anm_set_pos(blue, center[0], center[1], 0.0f);
            }
        }
    }

    ++s_b10.elapsed;
    ++s_b10.frames;
    if (--s_b10.left <= 0) {
        signal_bomb10_fx_dump("结束前");
        if (s_b10.ring_vm) { ce_anm_delete(s_b10.ring_vm); s_b10.ring_vm = 0; }
        ce_log("signal: 灵击结束（%d 帧，伤害源 %d 帧，半径 %.1f，中心 y %.1f，撒信仰点 %d 颗）",
               (int)s_b10.frames, (int)s_b10.dmg_frames, (double)r, (double)s_b10.y,
               (int)s_b10.faith_dots);
        s_b10.left = 0;
    }
}

/* ======================================================================================
 * ★★ 支援特技（TH11 六个伙伴）—— **本卡的新功能块（2026-09-15）**
 * ======================================================================================
 * 判据/数值/量纲的**唯一**出处（本文只写「怎么落地」，不重复推理）：
 *   · `native/cards/signal_core.h` 的「支援特技」整段（含**本轮订正过的输入位表**）
 *   · `local/_scratch/research-th11-support-special-input.md`（判据一手；⚠ §1.2 的位表有错，见 core.h）
 *   · `engine/player/th11/02-support-special-effects.md`（效果侧）
 *   · `engine/player/th18/04-warp-yukari-gap.md`（TH18 原生缺口入口 `0x409a60`）
 *
 * ## 用户拍板的映射（**照做，不是复刻**）
 *   TH18 灵梦（机体 0）= Σ TH11 灵梦伙伴 **紫 + 萃香 + 文**，三条同时生效；
 *   TH18 魔理沙（机体 1）= Σ TH11 魔理沙伙伴 **爱丽丝 + 帕秋莉 + 荷取**，三条同时生效。
 *   咲夜(2)/早苗(3) 本轮不做。机体判据 = `CE_ADDR_CHARACTER`（`0x4cccf4`，0..3）。
 *
 * ## ★★ 2026-09-16 **特技全部免费**（用户拍板；本块曾经的五条价**已删**）
 *   历史：本卡一开始按用户口径「每条特技都要花掉本卡已有的资源」给五条特技各配了价
 *   （紫 20.00 / 萃香 10.00 / 文 0.25-每帧 / 帕秋莉 15.00 / 荷取 5.00），并加了「信号不够就不发动」的硬门。
 *   实跑（2026-09-16 09:55 那局）暴露两件事 ⇒ 当天拆掉整层：
 *     · 硬门让特技在信号低时**整个失灵**（用户反馈「灵梦机体的松开全部按键全收集没有生效」；
 *       日志行 1667/1672/1675/1691/1694/1695 连打六行 `被门槛挡下（信号 0 < 10000）`）；
 *     · 更根本的是**一手没有这一层**：TH11 交信度槽 `0x4a56f4` 的写点穷举（本轮在
 *       `local/th11.v1.00a/th11.exe` 的完整反汇编上重查过一遍）= **加值入口 `0x40baa0`**
 *       （5 个调用点全在推**擦弹收益表** `0x4a7950`/`0x4a79d4`/`0x4a7a68`/`0x4a7a7c`，无负参数）+
 *       **每帧函数 `0x420a70`**（线上 +5000 补满 / +16 / +12 / +9，线下 `0x420b03 add [edi+0x14],−30`）+ 
 *       **清零**（`0x41fb24` 一局开始、`0x431097`/`0x431227`/`0x4312c0` 玩家状态 0/2/4、`0x432807` miss）
 *       ⇒ **全系统唯一的减法就是线下 −0.30/帧**，没有任何「消费」入口。
 *   ⇒ 用户 2026-09-16 选定「**全免费 = 完全一手**」：五条价与 `signal_spend()` 全部删除，
 *     特技只按一手判据与频率发动。信号槽的语义因此 **100% 等于一手**（擦弹加 / 线上补 +
 *     线下 −0.30 衰减 / 死亡与登场清零 / 满槽把落下道具切成被收走 / 计分公式里的交信度项）。
 *   ⚠ 唯一剩下的 ❓ 自定旋钮是**文那条的倍率本身**（`SIGNAL_HYPER_SPEED_PERMILLE`）——
 *     一手那条只播拖尾特效、不存在速度倍率（见 ③ 的注释），倍率是用户口径要的表现。*/

/* ---- TH18 自机字段（engine.h 没有 → 就地补，**只读 / 只写这两个**，同址同口径）---- */
#define SIGNAL_OFF_PLAYER_POS_X     0x62c    /* int32 定点（1/128 px）。一手 `th18:0x40a1c2`（卡 41 的门）
                                              * 与 `0x45b705`（位置累加器）都读它；`0x45b735/0x45b7ad` 钳 ±0x5c00。*/
#define SIGNAL_OFF_PLAYER_WARP      0x4779c  /* dword 位域：bit7(0x80)/bit8(0x100) = 瞬移方向（`th18:0x409a60` 写、
                                              * `0x45b725`/`0x45b195` 读）。⚠ **不是**「聚焦位」：`engine.h` 的
                                              * `CE_PLAYER_IS_FOCUSING` 也写在这个偏移上，但全 exe 没有任何一处把它当
                                              * 整 dword 读成「聚焦」（本轮 `grep 0x4779c` 的 26 个站点逐个看过，
                                              * 全是 `test …,<bit>`）⇒ **那一行是错的登记**（正确候选是 `0x476cc`，
                                              * 见 `sdk.h` 的低速子机声明层注释）。本卡不依赖它，只登记。*/
#define SIGNAL_WARP_DIR_MASK        0x180u   /* bit7 | bit8 */
/* 原生缺口入口：`thiscall(player; dir) ret 4`（一手逐条：`0x409a63 mov eax,[ecx+0x477b0]` 用 ecx 取 player、
 * `0x409a80 mov eax,[ebp+0x8]` 取 dir、`0x409ac2 ret 0x4`）。
 * dir：1 = 自左向右、2 = 自右向左、0 = 清方向位（引擎自己在第 16 帧调）。*/
typedef int (__attribute__((thiscall)) *signal_fn_warp_t)(void *player, int dir);
#define SIGNAL_FN_WARP              0x409a60

/* ---- ① 紫 · 缺口瞬移 -----------------------------------------------------------------
 * 判据与状态机全在 `signal_gap_tick()`（纯逻辑、主机单测）；这里只做三件卡侧的事：
 * 读引擎（x 定点 / 输入 / 原生是否在跑）、**收代价**、调原生入口。
 * ⚠ **效果侧一行都不用写**：`0x409a60` 之后引擎自己跑 16 帧动画（子机收起 → 第 8 帧换边 →
 *   展开）并在收尾时用 dir=0 清位（`th18:0x45b2a4`–`0x45b2b8`）⇒ 我们**不管收尾**。*/
static void signal_support_gap(signal_card_t *s)
{
    uint8_t *pl = CE_PLAYER();
    int32_t x_fixed, dir = 0;
    int native_active;

    x_fixed = *(const int32_t *)(pl + SIGNAL_OFF_PLAYER_POS_X);
    native_active = (*(const uint32_t *)(pl + SIGNAL_OFF_PLAYER_WARP) & SIGNAL_WARP_DIR_MASK) != 0u;

    if (!signal_gap_tick(&s->gap, CE_INPUT_HELD(), x_fixed, native_active, &dir)) return;

    /* ★ 交出去：`th18:0x409a60(CE_PLAYER(), dir)` —— 原生卡 41 WARP 的入口。
     * ⚠ **不收费**（2026-09-16 用户拍板「全免费 = 完全一手」，见常量块）：一手这三条特技没有价。*/
    ((signal_fn_warp_t)SIGNAL_FN_WARP)(pl, dir);
    ++s->stat_gap;
    ce_log("signal: 紫·缺口瞬移 dir=%d (x=%d, 原生在跑=%d)", dir, (int)x_fixed, native_active);
}

/* ---- ② 萃香 · 全屏收道具 --------------------------------------------------------------
 * 一手 `th11:0x43077a`：`attempted_direction == 0` 且 `(INPUT & 0x9) == 0` 连续 > 10 帧
 * ⇒ 调 `0x4245e0`（把整池 state 1 的道具切成 state 3）。
 * ★ 落地**必须**走 `ce_item_collect_all()`（`EVENTS-CONTRACT.md` §6：114 的批结算只认 state 3）——
 *   它是 SDK 的共享件（107 森罗结界同一件），跟「满槽全收」是**同一个**原语（见 `signal_sweep_items`），
 *   所以两条路的效果完全一致，只有触发与代价不同。
 * ⚠ 一手是**持续态**（计数器在触发时不清零）⇒ 计数器过 10 之后**每一帧**都跑到 `0x43079e`，
 *   也就是「松手期间**连续**全收」，不是「进入的那一帧收一次」。
 *   ★★ 2026-09-16 订正（实跑反馈「松开全部按键全收集没有生效」，两条根因都是本卡自己加的）：
 *     ① **硬门**：原来信号 < 10.00 就整条不发动 —— 一手不收费，硬门与价**整层已删**（见常量块）；
 *     ② **只收一帧**：原来只在 `rising` 那一帧收 —— 改成**持续期内每帧都调** `ce_item_collect_all()`。
 *   效果本身幂等（已经 state 3 的道具再切一次没有额外收益）⇒ 频率不设限。*/
static void signal_support_suika(signal_card_t *s)
{
    uint32_t in = CE_INPUT_HELD();
    int rising = 0, on;

    /* 判据：没有方向输入 **且** 没按射击/低速（`0x9` = 射击|低速，见 core.h 的订正）。*/
    on = signal_hold_tick(&s->suika, !signal_core_pressing_dir(in) && !signal_core_pressing_atk(in),
                          &rising);
    s->suika_on = on;
    if (!on) return;                          /* 松开判据 ⇒ 状态机自己归零，本帧不动道具 */

    if (rising) {                             /* 进入持续态那一帧：记一笔 + 打一行 */
        ++s->stat_suika;
        ce_log("signal: 萃香·全屏回收 持续态开始（无方向无射击 >%d 帧）；"
               "持续期内**每帧**都收（复刻 `th11:0x43079e` 的到达频率）",
               SIGNAL_SUPPORT_HOLD_FRAMES);
    }
    (void)signal_sweep_items(s);              /* ★ 持续态：每帧都收（一手就是这个频率）*/
}

/* ---- ③ 文 · 超高速（❓ 自定倍率 + 残像拖影）-------------------------------------------
 * 一手 `th11:0x430651`：`(INPUT & 0x9) == 0` 连续 > 10 帧 ⇒ **造一个残像 VM**
 * （`0x8ba8` 全 exe 只有 inc/cmp/清零，**不参与速度**；`pl00a/b/c.sht` 的速度字段逐字节相同
 *  ⇒ 原版**不存在**这个倍率 —— 缺口①本轮已用 `thdat -x 11` 取到并证伪）。
 * 用户口径要求「超高速移动」可被感知 ⇒ 我们用 TH18 自己的旋钮 `CE_PLAYER_SPEED_MULT`
 * （`0x45c702` 每帧复位 1.0、`0x45b5b6` 移动时 `mulss` 读）补一个**保守**倍率，**登记 ❓ 自定**。
 * ⚠ 判据 = 与一手同一条；一手那条只播拖尾特效、**不收费**（2026-09-16 全免费，见常量块）。
 *
 * ## ★★ 2026-09-16l 用户口径：**拖影，不是闪黄**
 *   「**超高速移动不是闪黄，而是背后有玩家的拖影，参考 th16 的实现方式，以及反汇编资料**」
 *   ⇒ **不**给自机本体染黄（TH16 那支的「加速态」做法），改成**每帧在自机位置留一个残像**。
 *
 * ## 一手（两条独立链，机制逐条对上）
 *   · **TH11**（本卡对着的那一作）：`th11:0x43066e`–`0x4306c2` —— 计数 > 10 之后**每帧**
 *     `call 0x4561e0`（按 id 取/建 VM）→ `[vm+0x3b0] != 0` 时用 `[player+0x3b0]`（**自机当下的 sprite**）
 *     调 `0x44ab40`（`set_sprite`）→ `call 0x456430`（摆到 `[player+0x87c]` = 自机坐标）。
 *     它用的脚本 = **`pl00.anm` script19**（`0x430673 push 0x13`）：
 *       `layer(5); type(0); sprite(sprite0); blendMode(1); color(255,64,64);
 *        colorTime(15, 0, 64,64,255); alphaTime(15, 0, 0); +15: delete();`
 *       ⇒ **加算、15 帧里从红跑到蓝、然后淡出**（原文用 `v4.anmm` 打，见 `AUDIT` §BE 详录 #31 那条坑）。
 *   · **TH16**（用户点名的参考实现）：`Player__on_tick` `0x442c76` / `0x442d33` —— 加速态下**每帧**
 *     `AnmLoaded__create_40e5c0([player+0xc], &out, 脚本表[CHARACTER], 自机坐标, 0, -1)`
 *     → `AnmLoaded__set_sprite(自机 anm, vm, [player+0x34])`（照自机当下那帧的 sprite）。
 *     **本仓已实跑过的同款** = 卡 116（`cards/season.c` 的 `afterimage_tick()`，见
 *     `engine/player/th16/02-season-release-and-bombs.md` §5h）。
 *
 * ## 我们怎么落（**复用 TH18 原生脚本，不新增资产**）
 *   TH18 的 `pl0X.anm` 里**原生就留着**这一族残像脚本（TH16 那支被引擎删了、脚本还在；
 *   脚本号按 CHARACTER：pl00 = **17**、pl01 = **19**、pl02 = **14**、pl03 = **14** —— 与 116 用的是同一批，
 *   见 `season.c` 的 `k_script[]`）。⇒ 每帧 `ce_anm_spawn(自机 anm, 脚本[机体], 13)` +
 *   `ce_anm_set_sprite(自机 anm, id, 自机当下 sprite)` + `ce_anm_set_pos(自机坐标)`，脚本自己 20 帧淡出。
 *   ⚠ **颜色差异（登记 `TUNING.md` §2.5）**：TH11 的残像脚本是「红→蓝、15 帧」（`pl00.anm` script19），
 *     TH16/TH18 那支是「蓝紫 `color(60,0,192)`、20 帧」⇒ 我们用**原生脚本**（颜色即原生值），
 *     没有把 TH11 的 colorTime 搬过来（搬它要新脚本 + 每帧改色，会把脚本自己的 alphaTime 按掉）。*/
/* 每帧：把场上每支残像按**它自己的年龄**上色（一手 `pl00.anm` script19 的斜坡），并清掉没了的。*/
static void signal_trail_tick(void)
{
    int32_t i, w = 0;

    for (i = 0; i < s_trail_n; ++i) {
        uint32_t id = s_trail[i].id;
        uint8_t *vm = id ? ce_anm_get_vm(id) : 0;
        int32_t  age = s_trail[i].age;

        if (!vm) continue;                              /* VM 自己删了 ⇒ 出池 */
        {
            int32_t  rgb = signal_trail_rgb(age);
            uint32_t a   = *(const uint32_t *)(vm + CE_ANM_VM_COLOR1) & 0xff000000u;   /* 保留脚本自己的淡出 */
            (void)ce_anm_set_color(id, a | (uint32_t)rgb);
        }
        s_trail[w].id  = id;
        s_trail[w].age = age + 1;
        ++w;
    }
    s_trail_n = w;
}

static void signal_aya_trail(signal_card_t *s, const float *pp)
{
    static const int32_t k_script[4] = { 17, 19, 14, 14 };   /* TH18 `pl0X.anm` 的原生残像脚本 */
    uint8_t *pl;
    void *anm;
    uint32_t id;
    int32_t ch, sprite;

    (void)s;
    if (!pp) return;
    pl = CE_PLAYER();
    if (!pl) return;
    anm = *(void **)(pl + CE_PLAYER_ANM);
    if (!anm) return;
    ch = *(volatile int32_t *)CE_ADDR_CHARACTER;
    if (ch < 0 || ch > 3) ch = 0;
    id = ce_anm_spawn(anm, k_script[ch], 13);        /* 脚本自己写 layer(13)，这里只是给实例化的默认层 */
    if (!id) return;
    sprite = *(int32_t *)(pl + CE_PLAYER_VM + CE_ANM_VM_SPRITE_ID);
    if (sprite > 0) ce_anm_set_sprite(anm, id, sprite);
    ce_anm_set_pos(id, pp[0], pp[1], 0.0f);
    /* ★★ 2026-09-16o：**颜色改成 TH11 那一支的起手色（粉红）**（用户：「**我记得 th11 的虚影好像是粉色？**」）。
     *   一手：TH11 的残像脚本 = `pl00.anm` **script19**：
     *     `layer(5); type(0); sprite(sprite0); blendMode(1); color(255, 64, 64);
     *      colorTime(15, 0, 64, 64, 255); alphaTime(15, 0, 0); +15: delete();`
     *   ⇒ 原版是「**起手 (255,64,64) 粉红 → 15 帧里跑到 (64,64,255) 蓝**、同时淡出」。
     *   我们用的是 **TH18 原生**那支残像脚本（`pl0X.anm` script17/19/14/14，`color(60,0,192)` 蓝紫），
     *   它的 `alphaTime(20,0,0)` 淡出是脚本自己的 ⇒ **卡侧只写一次颜色**（每帧重写会把淡出按掉，
     *   同 `cards/season.c` 的教训）。这里写 TH11 的**起手色**（粉红），观感最接近原版那 15 帧的前半段。
     *   ⚠ 没有搬 TH11 的 `colorTime` 渐变（那要新脚本 + 每帧改色）—— 登记 `TUNING.md`。*/
    /* ★★ 2026-09-16u：**入池**并交给 `signal_trail_tick()` 每帧按年龄上色（一手那条颜色斜坡）。
     *   这里只写第 0 帧的颜色（`signal_trail_rgb(0)` = (255,64,64)），A 用 255。*/
    if (s_trail_n < SIGNAL_TRAIL_POOL) {
        s_trail[s_trail_n].id  = id;
        s_trail[s_trail_n].age = 0;
        ++s_trail_n;
    }
    ce_anm_set_color(id, 0xff000000u | (uint32_t)signal_trail_rgb(0));

    /* ★ 2026-09-16o 诊断（用户：「**超高速移动…灵梦依旧在闪黄**」）：把**自机本体 VM** 与**残像 VM**
     *   的关键字段各打一份 —— 若「闪黄」是自机本体被染色，`player_c1` 那一栏会露出来；
     *   若是残像本身画错贴图，`trail_sprite` 与 `player_vm_sprite` 的差会露出来。每次进入持续态打一行
     *   （`rising` 由调用方给，见 `signal_support_aya()`）。*/
    {
        const uint8_t *pvm = (const uint8_t *)(pl + CE_PLAYER_VM);
        uint8_t *tvm = ce_anm_get_vm(id);
        ce_log("signal/aya: 残像 sprite=%d(自机本体 %d) player_c1=%08x trail_c1=%08x trail_scale=(%.2f,%.2f)",
               (int)sprite, (int)*(const int32_t *)(pvm + CE_ANM_VM_SPRITE_ID),
               *(const uint32_t *)(pvm + CE_ANM_VM_COLOR1),
               tvm ? *(const uint32_t *)(tvm + CE_ANM_VM_COLOR1) : 0u,
               tvm ? (double)((const float *)(tvm + CE_ANM_VM_SCALE))[0] : 0.0,
               tvm ? (double)((const float *)(tvm + CE_ANM_VM_SCALE))[1] : 0.0);
    }
}

static void signal_support_aya(signal_card_t *s)
{
    uint32_t in = CE_INPUT_HELD();
    const float *pp;
    int rising = 0, on;

    on = signal_hold_tick(&s->aya, !signal_core_pressing_atk(in), &rising);
    s->aya_on = on;
    if (!on) return;                          /* 不写旋钮：引擎每帧自己复位 1.0，什么都不做就是常态 */
    *(float *)(CE_PLAYER() + CE_PLAYER_SPEED_MULT) =
        (float)SIGNAL_HYPER_SPEED_PERMILLE / 1000.0f;
    /* ★ 2026-09-16l：**残像拖影**（每帧一个，脚本 20 帧自灭 ⇒ 场上约 20 个）。
     *   一手 = TH11 `0x430688` 那一支 / TH16 `0x442c76`（两条独立链，见上面那段注释）。*/
    pp = ce_player_pos();
    signal_aya_trail(s, pp);
    if (rising) {                             /* 只在**进入**持续态的那一帧记一次 + 打一行 */
        ++s->stat_aya;
        ce_log("signal: 文·超高速 生效（未按射击/低速 >%d 帧）x%d/1000 + 残像拖影（每帧 1 个，脚本 20 帧）",
               SIGNAL_SUPPORT_HOLD_FRAMES, SIGNAL_HYPER_SPEED_PERMILLE);
    }
}

/* 灵梦三条（机体 0）。★ 三条**同时**生效 = 用户拍板的 Σ 映射。*/
static void signal_support_reimu(signal_card_t *s)
{
    signal_support_gap(s);
    signal_support_suika(s);
    signal_support_aya(s);
}

/* ======================================================================================
 * ★★ 2026-09-15i：**咲夜(2) = 大小姐 + 二小姐 + 红美铃**、**早苗(3) = 诹访子 + 神奈子 + 恋恋**
 * ======================================================================================
 * 判据 / 一手地址 / 用户口径的分层**全部**在 `signal_core.h` 的「咲夜 / 早苗」那一段
 * （含弹池常量的一手、原生 Aya 卡的 `0x40c7f0` 逐条、板底 `0xd800`、PoC 的写点穷举）——
 * 本文只写「卡侧怎么接线」，不重复推理。
 *
 * | 条 | core 里的那一半（可单测）| 卡侧这一段做的三件事 |
 * | --- | --- | --- |
 * | ① 大小姐 · 擦弹持续触发 | `signal_remilia_t` + `signal_remilia_tick()` | **扫弹池**（唯一的卡侧活）→ 喂槽号 |
 * | ② 二小姐 · 蓄力清屏 | `signal_flandre_t` + `signal_flandre_tick()` + 半径公式 | 读 Shift / 自机态 → 消弹（闩锁 + 记账）|
 * | ③ 红美铃 · 下限 + 增速 | `signal_core_step_mod()` 的 `meiling_on` | 在**信号推进那一句**（③）传门 |
 * | ④ 诹访子 · 贴板底不扣 | `signal_core_at_board_bottom()` + `at_bottom` | 同上，读 `player+0x630` |
 * | ⑤ 神奈子 · 收点线下降 | `ce_poc_declare_px_id()` 声明 + `ce_poc_apply()` 落笔（层在 `sdk_core.h`）| `signal_kanako_t` 算「快照 + 相对下移」的**目标值**（✅ 2026-09-15 已迁移）|
 * | ⑥ 恋恋 · 放开键 ⇒ 4 帧无敌 | `signal_koishi_t` + `signal_koishi_tick()` | 声明无敌层 `ce_invuln_request()`（**量 = 原生 4 帧**）|
 *
 * ⚠ ③④ 的落点在 `on_tick_2` 的 **③ 信号槽推进**那一句（早于 ⑥b 的分派）——
 *   原因：那是本帧信号槽唯一的一次推进，修饰必须在**它之前**算出来；两条的「读引擎 + 传门」
 *   因此写在那一句旁边（`signal_step_mod_fill()`），而**有每帧动作**的二小姐/神奈子/恋恋
 *   仍然在 ⑥b 的机体门里跑（与灵梦/魔理沙那六条的排法一致）。*/

/* ---- TH18 弹池（大小姐那条要自己扫）--------------------------------------------------
 * 常量与 `cards/lolk.c` / `cards/pofv.c` **同源**（`engine.h` 的一手 + 本轮复核），
 * 弹池那三个（`CE_BULLET_MGR` / `CE_BM_BULLETS` / `CE_BM_BULLET_COUNT` / `CE_BULLET_STRIDE` /
 * `CE_BULLET_FLAGS` / `CE_BULLET_POS` / `CE_BULLET_STATE`）直接用 `engine.h` 的。
 * ⚠ 本卡**不**往 `engine.h` 补这几个弹字段（那是共享件，改动要登记 §BE）—— 就地补，同址同口径。*/
#define SIGNAL_BULLET_RADIUS      0x658      /* float：弹自己的碰撞半径（= 圆判定里的 `br`；`0x4248d8`）*/
#define SIGNAL_BULLET_RADIUS2     0x65c      /* float：矩形判定用的第二个半宽（`0x424923`）*/
#define SIGNAL_BULLET_SIZE_SCALE  0xf38      /* float：`flags & 0x40` 时乘到两个半径上（`0x424917`）*/
#define SIGNAL_BULLET_FLAG_COLLIDE 0x2u      /* `0x4248cf`：不参与碰撞的弹连分派器都进不去 */
#define SIGNAL_BULLET_FLAG_CIRCLE  0x10u     /* `0x4248fd`：走圆判定 `0x45cd90`，否则矩形 `0x45cc20` */
#define SIGNAL_BULLET_FLAG_SCALED  0x40u     /* `0x424900`：先按 `+0xf38` 缩放半径 */
/* ⚠ `flags & 0x4` = 引擎的「这颗弹一生擦过了一次」（`0x42496f` 判 / `0x424984` 置）——
 *   **本卡故意不看这一位**：不看它才是「持续触发」（看它就退回成 TH18 原生的一发一次）。
 *   编译期留一个具名常量把这件事说出来，免得日后有人「顺手补上」。*/
#define SIGNAL_BULLET_FLAG_GRAZED  0x4u      /* 我们**不**用它当门（见上）*/
#define SIGNAL_GRAZE_CIRCLE_DIV    2.5f      /* 一手 `0x4b921c` */
#define SIGNAL_GRAZE_CIRCLE_MIN    40.0f     /* 一手 `0x4b92c4`（「额外圈」下限）*/
#define SIGNAL_GRAZE_RECT_MARGIN   24.0f     /* 一手 `0x4b92a4`（矩形模式外扩）*/
#define SIGNAL_OFF_PLAYER_FOCUS    0x476cc   /* int32：聚焦标志（一手 `0x45b3fa` 写；⚠ **不是** engine.h 的
                                              *   `CE_PLAYER_IS_FOCUSING 0x4779c` —— 那一行是错登记，见支援特技块的注）*/
#define SIGNAL_OFF_PLAYER_POS_Y_FIXED 0x630  /* int32 定点（1/128 px）；一手 `0x45b717`/`0x45b7e7` 的钳位对象 */

/* 这颗弹本帧是否落在**擦弹圈**里（逐字照抄引擎的两种判定；见 `signal_core.h` ①的一手）。
 * `focus` = 是否聚焦（决定读哪一组自机判定尺寸）。纯只读、不写弹上任何字段。*/
static int signal_bullet_in_graze(const uint8_t *b, const uint8_t *pl, uint32_t flags, float br, int focus)
{
    float dx = *(const float *)(b + CE_BULLET_POS)     - *(const float *)(pl + CE_PLAYER_X);
    float dy = *(const float *)(b + CE_BULLET_POS + 4) - *(const float *)(pl + CE_PLAYER_Y);

    if (flags & SIGNAL_BULLET_FLAG_CIRCLE) {
        /* 圆（`0x45cd90`）：`d² < (r + max(br/2.5, 40.0))² + br²`。
         * `r` = 自机判定圆半径（聚焦 `+0x479a0` / 非聚焦 `+0x4799c`，原生都 3.0）。*/
        float pr = *(const float *)(pl + (focus ? CE_PLAYER_HITBOX_R_F : CE_PLAYER_HITBOX_R_NF));
        float extra = br / SIGNAL_GRAZE_CIRCLE_DIV;
        float rr;
        if (extra < SIGNAL_GRAZE_CIRCLE_MIN) extra = SIGNAL_GRAZE_CIRCLE_MIN;
        rr = pr + extra;
        return (dx * dx + dy * dy < rr * rr + br * br) ? 1 : 0;
    }
    /* 矩形（`0x45cc20`）：自机判定框 vs **弹的方框**（半宽 = 0x658/2、半高 = 0x65c/2）**外扩 24.0**。
     * ⚠ 与 `cards/lolk.c` 的写法有一点不同：那边把 24.0 加到**自机框**上、且**丢掉**弹的两个半宽
     *   （差 br/2 ≈ 2–5 px）。这里按引擎原式写（弹框外扩 + 自机框）。*/
    {
        float hw = br * 0.5f;
        float hh = *(const float *)(b + SIGNAL_BULLET_RADIUS2) * 0.5f;
        float pw = *(const float *)(pl + (focus ? CE_PLAYER_HITBOX_HW_F : CE_PLAYER_HITBOX_HW_NF));
        float ph = *(const float *)(pl + (focus ? CE_PLAYER_HITBOX_HH_F : CE_PLAYER_HITBOX_HH_NF));
        float ew = hw + SIGNAL_GRAZE_RECT_MARGIN + pw;
        float eh = hh + SIGNAL_GRAZE_RECT_MARGIN + ph;
        return (dx <= ew && -dx <= ew && dy <= eh && -dy <= eh) ? 1 : 0;
    }
}

/* ---- ① 大小姐：扫弹池 → 本帧额外计入的擦弹次数 ----------------------------------------
 * 只扫**子弹池**（激光不在里面 ⇒ 用户那句「激光本来就持续触发」结构上自动满足，见 `signal_core.h` ①③）。
 * ⚠ 循环上界用 `CE_BM_BULLET_COUNT + 1`：引擎自己的两条平扫（`destroy_all 0x423a62` / `cancel_all 0x4299fc`）
 *   只跑 `0x7d0` 次、**漏掉最后一张真弹**（`mgr+0x108c + 1999*0xfa0`；池共 2001 块）——
 *   那是 ZUN 的 off-by-one，`lolk.c` 照抄了它。本卡多扫一格（`mgr+0xec + 2000*0xfa0` 仍在池内，
 *   池尾 = `mgr+0x7a228c`）⇒ 不跟着漏。（登记在报告里，未改共享件。）*/
static int32_t signal_graze_scan(signal_card_t *s)
{
    uint8_t *mgr = CE_BULLET_MGR();
    uint8_t *pl  = CE_PLAYER();
    uint16_t keys[SIGNAL_REMILIA_TRACK_MAX];
    int focus, n = 0;
    unsigned i;

    if (!mgr || !pl) return 0;
    focus = *(const int32_t *)(pl + SIGNAL_OFF_PLAYER_FOCUS) != 0;

    for (i = 0; i <= (unsigned)CE_BM_BULLET_COUNT && n < SIGNAL_REMILIA_TRACK_MAX; ++i) {
        uint8_t *b = mgr + CE_BM_BULLETS + (size_t)i * CE_BULLET_STRIDE;
        uint16_t st = *(const uint16_t *)(b + CE_BULLET_STATE);
        uint32_t flags;
        float br;

        /* `state`：0 = 空槽、3 = 被自机打掉、4 = 消弹中 ⇒ 都跳过（一手 `0x4249c8` / `0x42901d`）；
         * 1 / 2（出现中）才可能被擦（`0x4248a0` 的分派器只对活弹跑）。*/
        if (st == 0 || st == 3 || st == 4) continue;
        flags = *(const uint32_t *)(b + CE_BULLET_FLAGS);
        if (!(flags & SIGNAL_BULLET_FLAG_COLLIDE)) continue;      /* 引擎在这一行早退（`0x4248cf`）*/
        br = *(const float *)(b + SIGNAL_BULLET_RADIUS);
        if (!(br > 0.0f)) continue;                               /* `0x4248e9` */
        if (flags & SIGNAL_BULLET_FLAG_SCALED) br *= *(const float *)(b + SIGNAL_BULLET_SIZE_SCALE);
        if (signal_bullet_in_graze(b, pl, flags, br, focus)) keys[n++] = (uint16_t)i;
    }

    /* ★★ 2026-09-16m 用户口径：「**逐帧的擦弹应当完全调用擦弹函数，包括发音效，掉点之类的**」
     *   ⇒ 本帧**真的算了一次擦弹**的那些弹，逐颗调引擎自己的 `Player__do_graze`（`0x45e160`）：
     *     它做三件事 —— GRAZE 计数 +1（`0x4ccd0c`）、起一颗 `effect.anm` 火花（13 帧）、
     *     播 `se_graze.wav`（id `0x2a`，声像 = 子弹 x）。全套一手见 `engine.h` 的 `CE_FN_DO_GRAZE`。
     *   ⚠ **本卡不再自己记「额外擦弹」那一本账**（`extra_graze` 只留诊断）：喂信号槽的那笔增量走
     *     原生计数 `0x4ccd0c` 的帧差 `dg`（本函数被提到 `on_tick_2` 的 ⓪ 段、在读数**之前**跑）
     *     ⇒ **一本账**，不会「一次擦弹喂两遍」。
     *   ⚠ 音效会**逐帧**重播（只要本帧有计入的弹）—— 这是 `do_graze` 内部行为，去掉它就只能不调它
     *     （那与用户口径相悖）；同帧同 id 只响 1 次（音效池把声像取平均），不是每颗一声。*/
    {
        uint16_t counted[SIGNAL_REMILIA_TRACK_MAX];
        int32_t  c = signal_remilia_tick(&s->remilia, keys, n, counted, SIGNAL_REMILIA_TRACK_MAX);
        int32_t  k;
        int32_t  fired = 0;

        for (k = 0; k < c; ++k) {
            uint8_t *b = mgr + CE_BM_BULLETS + (size_t)counted[k] * CE_BULLET_STRIDE;
            if (ce_do_graze((const float *)(b + CE_BULLET_POS))) ++fired;
        }
        s->extra_graze_fired = fired;            /* 诊断：真正调成功的次数 */
        return c;
    }
}

/* ---- ⑥ 恋恋：读方向号 + 聚焦 + 输入 → 声明无敌 -----------------------------------------
 * 方向号 `player+0x4793c` 由引擎在自机移动段写（`0x45b2cc`–`0x45b3ca`，0 = 没按方向、1..8 = 八向）；
 * 我们的 `on_tick_2` 在 AbilityManager 线（`0x16`）、它在 Player 线（`0x17`）⇒ 读到的是**上一帧**的值
 * —— 原生 Aya 卡也是卡 tick，处在同一条线上 ⇒ 两边同相位（差别只在「+4 帧还是 +3 帧有效」，见 `signal_core.h` ⑥）。*/
/* ★★ 2026-09-16：**大小姐（咲夜×蕾米莉亚）· 擦弹圈内的子弹变红**（用户口径，⑧-3）。
 *
 * ## 落点：**弹 VM 的 `color_1`**（与 115 那条「TH15 靠近变橙」同一个字段）
 *   `bullet+0x28` = `zAnmVm`，颜色在 `VM + CE_ANM_VM_COLOR1`（`0x524`，D3DCOLOR 0xAARRGGBB）。
 *   先例 = `cards/lolk.c`（115）的 `LOLK_GRAZED_COLOR = 0xffffd080`：**只改 RGB、保留 alpha**
 *   （弹的 alpha 是脚本每帧插值的 ⇒ 连 alpha 一起写会把脚本的淡入淡出按掉）。
 *
 * ## 为什么要在 `on_draw` 里写（用户拍板方案 (a)）
 *   卡按 id 顺序 tick、111 在 115 前面 ⇒ 我若在 `on_tick_2` 里写，115 会**后写覆盖**。
 *   `on_draw`（虚表 `+0x48`）在所有卡的 `on_tick_2` 之后跑 ⇒ 红在那一帧**最后落笔**、压得住橙。
 *   ⚠ 代价：颜色比判定晚一帧生效（进出圈各差 1 帧），肉眼不可见；用户口径说「没有别的染色在场」，
 *     所以不做共享的「弹染色层」，本卡自带还原（见下）。
 *
 * ## 还原（必须有，否则弹出圈后永远红）
 *   小表记「被我染过的弹指针 + 它原来的 color_1」；每帧：
 *     · 圈内且不在表里 ⇒ 记下原色 + 写红；
 *     · 表里但已不在圈内（或槽已空 / 已消）⇒ 写回原色 + 移出表。
 *   ⚠ 指针是**池槽地址**（稳定），不是弹对象；槽被复用时 state 会变，靠上面的判据先还原。
 *   `on_delete` / `on_run_reset` 走 `signal_tint_restore_all()` 兜底。*/
/* ★ 2026-09-16 用户实跑：「**红染有点太红，参考染橙的力度**」——
 *   115 那条橙是 `0xffffd080`（R255 G208 B128：**掺白**的暖色，不是饱和色）⇒ 这里取同样「掺白」的力度：
 *   R 满、G/B 抬到一半 ⇒ `0xffff8080`。想更淡/更艳就改这一个常量（`TUNING.md` §2.1h）。*/
#define SIGNAL_REMILIA_TINT_COLOR   0xffff8080u   /* ❓ 自定：掺白红（只写 RGB，alpha 保留）*/
#define SIGNAL_REMILIA_TINT_MAX     64            /* 表容量（同屏圈内弹的常见上限）*/
#define SIGNAL_BULLET_VM_OFF        0x28          /* `zBullet.vm`（= `lolk.c` 的 `CE_BULLET_VM`）*/
static uint8_t  *s_tint_b[SIGNAL_REMILIA_TINT_MAX];
static uint32_t  s_tint_orig[SIGNAL_REMILIA_TINT_MAX];
static int       s_tint_n;

/* 把这一颗的 color_1 写成红色（只改 RGB）*/
static void signal_tint_apply(uint8_t *b)
{
    volatile uint32_t *col = (volatile uint32_t *)(b + SIGNAL_BULLET_VM_OFF + CE_ANM_VM_COLOR1);
    *col = (*col & 0xff000000u) | (SIGNAL_REMILIA_TINT_COLOR & 0x00ffffffu);
}

/* 还原整张表（删卡 / 换关 / 换机体时走）*/
void signal_tint_restore_all(void)
{
    int i;
    for (i = 0; i < s_tint_n; ++i) {
        volatile uint32_t *col = (volatile uint32_t *)(s_tint_b[i] + SIGNAL_BULLET_VM_OFF + CE_ANM_VM_COLOR1);
        *col = s_tint_orig[i];
    }
    s_tint_n = 0;
}

/* 每帧（在 `on_draw` 里跑）：圈内染红、出圈还原。`mgr`/`pl` 为空 ⇒ 只做还原。*/
static void signal_tint_tick(void)
{
    uint8_t *mgr = CE_BULLET_MGR();
    uint8_t *pl  = CE_PLAYER();
    int focus = 0, i, w = 0;
    unsigned k;

    if (pl) focus = *(const int32_t *)(pl + SIGNAL_OFF_PLAYER_FOCUS) != 0;

    /* ① 先把「我染过、但现在不该红」的还原掉（并把还该红的收进表的前 w 格）*/
    for (i = 0; i < s_tint_n; ++i) {
        uint8_t *b = s_tint_b[i];
        int keep = 0;

        if (mgr && pl) {
            uint16_t st = *(const uint16_t *)(b + CE_BULLET_STATE);
            uint32_t flags = *(const uint32_t *)(b + CE_BULLET_FLAGS);
            float br = *(const float *)(b + SIGNAL_BULLET_RADIUS);
            if (st != 0 && st != 3 && st != 4 && (flags & SIGNAL_BULLET_FLAG_COLLIDE) && br > 0.0f) {
                if (flags & SIGNAL_BULLET_FLAG_SCALED) br *= *(const float *)(b + SIGNAL_BULLET_SIZE_SCALE);
                keep = signal_bullet_in_graze(b, pl, flags, br, focus);
            }
        }
        if (keep) {
            s_tint_b[w] = b; s_tint_orig[w] = s_tint_orig[i]; ++w;
        } else {
            volatile uint32_t *col = (volatile uint32_t *)(b + SIGNAL_BULLET_VM_OFF + CE_ANM_VM_COLOR1);
            *col = s_tint_orig[i];
        }
    }
    s_tint_n = w;

    if (!mgr || !pl) return;

    /* ★★ 2026-09-16m：**机体门** —— 红染是「咲夜×大小姐」那条支援特技的视觉，只有咲夜(2) 生效。
     *   ⚠ 上面①（还原）已经跑完 ⇒ 别的机体上不会留下我们染过的红弹，只是**不再染新的**
     *   （用户实跑：「**魔理沙还是有染红**」）。*/
    if (*(const volatile int32_t *)CE_ADDR_CHARACTER != SIGNAL_CHAR_SAKUYA) return;

    /* ② 新进圈的弹：记原色 + 染红（已在表里的由①保持，重复写也幂等）*/
    for (k = 0; k <= (unsigned)CE_BM_BULLET_COUNT && s_tint_n < SIGNAL_REMILIA_TINT_MAX; ++k) {
        uint8_t *b = mgr + CE_BM_BULLETS + (size_t)k * CE_BULLET_STRIDE;
        uint16_t st = *(const uint16_t *)(b + CE_BULLET_STATE);
        uint32_t flags;
        float br;
        int j, dup = 0;

        if (st == 0 || st == 3 || st == 4) continue;
        flags = *(const uint32_t *)(b + CE_BULLET_FLAGS);
        if (!(flags & SIGNAL_BULLET_FLAG_COLLIDE)) continue;
        br = *(const float *)(b + SIGNAL_BULLET_RADIUS);
        if (!(br > 0.0f)) continue;
        if (flags & SIGNAL_BULLET_FLAG_SCALED) br *= *(const float *)(b + SIGNAL_BULLET_SIZE_SCALE);
        if (!signal_bullet_in_graze(b, pl, flags, br, focus)) continue;
        for (j = 0; j < s_tint_n; ++j) if (s_tint_b[j] == b) { dup = 1; break; }
        if (dup) continue;
        s_tint_b[s_tint_n] = b;
        s_tint_orig[s_tint_n] = *(const uint32_t *)(b + SIGNAL_BULLET_VM_OFF + CE_ANM_VM_COLOR1);
        ++s_tint_n;
        signal_tint_apply(b);
    }
}

#define SIGNAL_OFF_PLAYER_MOVE_DIR  0x4793c

/* ---- ③④ 的修饰（在 on_tick_2 的 ③ 那一句之前填）--------------------------------------
 * 读引擎 + 传门，**只**为本卡生效（共享件 `faith_signal_step()` / `faith_signal_add()` 一字不改）。*/
static void signal_step_mod_fill(signal_card_t *s, signal_step_mod_t *mod)
{
    uint8_t *pl = CE_PLAYER();
    int chara = *(const int32_t *)CE_ADDR_CHARACTER;

    memset(mod, 0, sizeof *mod);
    s->extra_graze = 0;
    s->suwako_on = 0;
    /* ★★ 2026-09-16za（106 红魔乡的协同效应，用户口径逐字：「**除非满P，不加信号强度**」）：
     *   106 每帧声明「现在不是满 P」（共享层 `ce_full_p_gate_*`）；本卡读到就关掉
     *   「线上未满 ⇒ 补满并钳到 100.00」那一支（用户选定口径 = **只停这一条**：
     *   擦弹是独立通道照给、已满档的 +0.16/+0.12/+0.09 照给、线下的 −0.30 衰减照给）。
     *   ⚠ 110 不读这一位（110 没有信号槽；`faith_signal_step` 的旧签名 = `..._gated(..., 1)`）。*/
    mod->no_line_topup = ce_full_p_gate_active() ? 1 : 0;
    if (!pl) return;

    if (chara == SIGNAL_CHAR_SAKUYA) {
        /* ★ 2026-09-16m：**擦弹扫描已经搬走**（`on_tick_2` 的 ⓪ 段、在原生计数读数之前跑）——
         *   它在这儿跑就晚了：那笔 `do_graze` 的 +N 会落在 `last_graze` 之后、下一帧被当成原生擦弹
         *   再喂一遍。本函数只剩红美铃那条修饰。*/
        mod->meiling_on = 1;
    }
    if (chara == SIGNAL_CHAR_SANAE) {
        /* ④ 诹访子：贴板底（板底 = 引擎自己的自机 y 下界 `0xd800` @1/128 px = 432.0 px）。*/
        mod->at_bottom = signal_core_at_board_bottom(
            *(const int32_t *)(pl + SIGNAL_OFF_PLAYER_POS_Y_FIXED));
        s->suwako_on = mod->at_bottom;
    }
}

/* ======================================================================================
 * 魔理沙三条（机体 1）= 爱丽丝 + 帕秋莉 + 荷取（用户拍板的 Σ 映射）
 * ====================================================================================== */

/* ---- ④ 爱丽丝 · ★ 2026-09-15g：**两条真·效果**（🟡 用户口径）--------------------------------
 * 「子机上限 8」那条**仍不实现**（TH18 原生上限 4 < 8，用户口径也确认「还是 4 个子机」）——
 * 一手与 TH18 侧的全套对账在 `signal_core.h` 的 ④(a)，**别在这里重复推理**。
 *
 * 本轮实装的是用户口径给的两条（同段 ④(b)）—— **两条的性质不同，别混着写**：
 *   ① **灵击 = 别人的一半**（TH18 侧 = 50 单位）：**比例 = ✅ 一手**（`pl01a.sht` 的 `+0x24`=12 / `+0x20`=8
 *      ⇒ 一发占满槽 1/8，其余五机 20/4 ⇒ 1/4 ⇒ **恰好一半**），**绝对档位 = ❓ 我们的映射**
 *      （TH11 是 per-shot 刻度、TH18 的一档是跨机体统一的 100 单位）。落点 = `signal_bomb()`
 *      （门槛「能扣就扣」= ≥1.55 档、下限 1.05 照旧）。
 *   ② **P 点收益 = 平均 +0.025 档**（🟡 用户口径；**+0.02 / +0.03 交替**）：相位与判据在**共享件**
 *      （`faith.h` 的 `faith_p_topup_take()`）—— **本卡不自己算 2/3**，落点 = `signal_on_item_commit()`。
 * ⇒ **爱丽丝这条没有「每帧做什么」**，所以它**不出现在 `signal_support_marisa()` 里**：
 *   它挂在上面那两个**事件钩子**上（机体门各自就地判 `CE_ADDR_CHARACTER == SIGNAL_CHAR_MARISA`）。
 *   ⚠ 与其余五条不同，这条从来没收过信号价（用户口径给的是「火力/收益」两条换算，不是本卡资源）
 *     —— 2026-09-16 起**六条全免费**，这一条只是早了一步。*/
#define SIGNAL_ALICE_OPTION_CAP_TH11 8
#define SIGNAL_ALICE_OPTION_CAP_TH18 4

/* ---- ⑤ 帕秋莉 · 五档子机配置切换（`th11:0x4308fe`）-----------------------------------
 * 一手判据：`INPUT_B & 0x400` ⇒ `formation = (formation + 1) % 5`（`th11:0x43090a`–`0x430921`）。
 *   而 `0x400` 是「`INPUT` 的 **bit0（射击）+ bit3（低速）** 同帧都按住」的合成位
 *   （`th11:0x43601b`/`0x43605a`/`0x436069` → `0x43601f`/`0x436076`；**bit3 = 低速**，见 `signal_core.h`
 *   的位表订正）⇒ TH18 侧由 `signal_core_patchouli_pressed()` 合成。
 * 落点：五档 → `assets/sht/append_shooterset.py` 的 **`MARISA_FORMATION_*` 五组（槽 0x29–0x2d）**，
 *   由**本卡自己申请的那一格子机**用 `ce_tick_shooters_for_card(opt, timer, 0, set)` 发射。
 *
 * ⚠ **与一手的三处已知偏差**（都写在这里，别当成复刻）：
 *   ① **档位闩锁**：一手是**电平**（长按会每帧切一档）；我们用「按下沿」闩锁，一次按住只切一档
 *      ⇒ ❓ 自定（电平读法在 TH18 的「射击+低速」判据下就是走马灯，见 `SIGNAL_FORMATION_EDGE_LATCH`）。
 *   ② **一格子机**：原版五档改的是**全部 8 格子机**的角/距与贴图；我们只申请**一格**
 *      （多格会直接放大输出 = 超模 —— 五组的每帧伤害完全相等（144/12 = 12.0），一格就够表达「换配置」）。
 *   ③ **贴图不换**：原版五档各有一支 `front.anm` script（`0x22 + formation` = 火/水/木/金/土）；
 *      我们的子机脚本是**借** `ability.anm` 的 `CE_ANM_ABILITY_SCRIPT_SEASON_OPT`（116 的子机本体脚本，
 *      三个中断号 1/2/3 齐备）⇒ 五档在画面上靠**位置**区分，不靠贴图。要真贴图得新增 ability.anm 脚本
 *      （要动 `assets/ability/` + `ORDER.txt`）⇒ **登记进报告，不在本轮文件范围内**。*/
/* ★★ 2026-09-16：二小姐蓄力的三条音效（**按 wav 名搬、不按 id 搬**；名表出处见
 *   `engine/card/th18/13-mallet-clear-and-th17-charge.md` 与 `engine/_shared/th18-sound-table.md`）。
 *   · `SIGNAL_SE_FLAN_CHARGE` = TH18 `0x21` = `se_cat00.wav`（TH17 那边是它的 `0x1e`）
 *   · `SIGNAL_SE_FLAN_LOOP`   = TH18 `0x4d` = `se_release.wav`（TH17 那边是 `0x37`）
 *   · `SIGNAL_SE_FLAN_FULL`   = TH18 `0x29`（**原生卡 42 的挥击音**；⚠ 名字两说，见调用点注释）
 *   `SIGNAL_SE_FLAN_LOOP_EVERY` = 循环音节流（❓ 自定：一手每帧一声，我们 30 帧一次）。*/
#define SIGNAL_SE_FLAN_CHARGE       0x21u
#define SIGNAL_SE_FLAN_LOOP         0x4du
#define SIGNAL_SE_FLAN_FULL         0x29u
#define SIGNAL_SE_FLAN_LOOP_EVERY   30
#define SIGNAL_OPTION_OFFSET_ALLOC  0   /* `ce_allocate_option` 的 off 参数：原生装备卡都用 0 */
#define SIGNAL_OPTION_RETRY        30   /* 认领失败时的补申请间隔（帧）*/
/* 子机贴图脚本（**按机体**选；见下面 `signal_option_script_for()` 的说明）。
 * ★★ 2026-09-16m：原来是写死的 `SIGNAL_OPTION_SCRIPT = CE_ANM_ABILITY_SCRIPT_SEASON_OPT`
 *   （= 借 116 的「季节点」那格图）⇒ 用户实跑「**魔理沙子机缺乏贴图**」⇒ 改成按机体给。*/
static int signal_option_script_for_chara(int32_t ch);

static int signal_option_script_for(int32_t ch, int32_t formation)
{
    /* ★★ 2026-09-16n：**魔理沙那一支按「当前档」选**（用户口径：「**切换子机的部分是帕秋莉的特效啊！**」
     *   ⇒ 子机 = 五个元素球，切档就是换一颗球）。一手：TH11 `repopulate_options` `th11:0x432cc0`
     *   对组合 4（MarisaB）用的就是 `0x22 + formation` = **`pl01.anm` script34..38**，
     *   而 34..38 **只差一个 `sprite()`**（38..42 = 五个 16×16 纯色球：红蓝绿黄褐 = 火水木金土）。
     *   ⚠ 这里与 108（妖机体）那批 `youkai_opt_*`（224–227）是**两回事**：那批是「伙伴本体」，
     *     111 的魔理沙要的是**帕秋莉的五元素球**。*/
    static const int k_elem[5] = {
        CE_ANM_ABILITY_SCRIPT_MARISA_ELEM_FIRE_OPT,
        CE_ANM_ABILITY_SCRIPT_MARISA_ELEM_WATER_OPT,
        CE_ANM_ABILITY_SCRIPT_MARISA_ELEM_WOOD_OPT,
        CE_ANM_ABILITY_SCRIPT_MARISA_ELEM_METAL_OPT,
        CE_ANM_ABILITY_SCRIPT_MARISA_ELEM_EARTH_OPT,
    };

    if (ch == SIGNAL_CHAR_MARISA) {
        int f = formation % 5;
        if (f < 0) f += 5;
        return k_elem[f];
    }
    return signal_option_script_for_chara(ch);
}

static int signal_option_script_for_chara(int32_t ch)
{
    /* 224–227 = 从原版**伙伴**切图的那四条子机脚本（卡 108 在用同一批）：
     *   灵梦 → 紫 / 魔理沙 → 爱丽丝 / 咲夜 → 大小姐 / 早苗 → 文。
     * ⚠ **❓ 自定**两处：① TH11 的子机画的是**伙伴本人**（12 组合各一套），我们只切了 4 位
     *   ⇒ 早苗侧没有她的伙伴（神奈子/诹访子/恋恋）就借文；② 111 的 Σ 映射是「三条特技同时生效」，
     *   没有「当前是哪一位伙伴」⇒ 「魔理沙用爱丽丝那一套」是按 Σ 里第一位取的。
     * 要完全对上得把剩下 8 位伙伴的子机图也切进来（管线 = `assets/ability/make_youkai_option_art.py`）。*/
    switch (ch) {
    case SIGNAL_CHAR_REIMU:  return CE_ANM_ABILITY_SCRIPT_YOUKAI_OPT_YUKARI;
    case SIGNAL_CHAR_MARISA: return CE_ANM_ABILITY_SCRIPT_YOUKAI_OPT_ALICE;
    case SIGNAL_CHAR_SAKUYA: return CE_ANM_ABILITY_SCRIPT_YOUKAI_OPT_REMILIA;
    default:                 return CE_ANM_ABILITY_SCRIPT_YOUKAI_OPT_AYA;
    }
}

/* 火力档 = `CURRENT_POWER / POWER_PER_LEVEL`（一手 `th18:0x45d604`；`0x4ccd40` = 100），
 * 钳到 [0, `SIGNAL_FORMATION_CELLS`]。⚠ 本卡**不**借 108 的 `yk_shot_tier()`（那是别人卡的核心）。
 * ★ 2026-09-16：判据本体搬进纯逻辑 `signal_power_cells()`（`signal_core.c`，主机单测钉得住），
 *   这里只剩「读引擎 → 调 core」。格数上限取 **`SIGNAL_OPTION_MAX`**（= 声明层上限 4），
 *   与阵型表的格数上限 `SIGNAL_FORMATION_CELLS`（也是 4）在本轮**同值** —— 两个名字都留着，
 *   因为一个管「申请几格子机」、一个管「阵型表能表达几格」，将来真要分开改。*/
static int signal_power_level(void)
{
    return (int)signal_power_cells(CE_CURRENT_POWER(), CE_POWER_PER_LEVEL(), SIGNAL_OPTION_MAX);
}

/* ★★★ 2026-09-16zb：TH11 `pl01b.sht` 的**逐字段复刻映射**（生成物 `sht_ids.h`，别手改）。
 *   · `s_marisa_opt[元素][档][格]` = **子机组号**（−1 = 该档没有这一格）；
 *   · `s_marisa_main[元素][档]`    = **主炮组号**（由 `on_shot_set_index` 整组覆盖）。
 *   元素顺序 = 火/水/木/金/土（= `s->formation` 0..4）；档 0..4（档 0 = 零子机，与 TH11 同）。
 *   一手（25 组 = 5 元素 × 5 档；主炮每发伤害逐元素逐档不同；子机行的率/角度逐格不同）
 *   与"为什么是 23 + 8 组"的推导：`assets/sht/append_shooterset.py` 的 `MARISA_*` 那一段（唯一出处）。*/
static const int16_t s_marisa_opt[SIGNAL_FORMATION_COUNT][CE_SHT_MARISA_LEVELS][CE_SHT_MARISA_CELLS]
    = CE_SHT_MARISA_OPT_INIT;
static const int16_t s_marisa_main[SIGNAL_FORMATION_COUNT][CE_SHT_MARISA_LEVELS]
    = CE_SHT_MARISA_MAIN_INIT;

/* 本帧要用的元素（0..4）与火力档（0..4，钳进 TH11 的档域）。*/
static int signal_marisa_elem(const signal_card_t *s)
{
    return (int)signal_formation_wrap(s->formation);
}

static int signal_marisa_level(void)
{
    int lv = signal_power_level();
    if (lv < 0) lv = 0;
    if (lv >= CE_SHT_MARISA_LEVELS) lv = CE_SHT_MARISA_LEVELS - 1;
    return lv;
}

/* 档位切换（在 `on_tick_2` 里跑）。❓ 自定：带按下沿闩锁（一手是流平，会长按走马灯）。
 * ★★ 2026-09-16（用户实跑纠错：「应当是**同时按下 shift+Z**，而不是**按着 Z，再按 shift** 就切换了」）：
 *   判据从**纯电平**换成**带新鲜度的电平** —— 两键各自都要在「刚按下」的窗口内才算「同时按下」
 *   （`signal_core_patchouli_fresh()` + `SIGNAL_PATCHOULI_FRESH_FRAMES` = 5；一手 `th11:0x43605e`/
 *   `0x43606d` 那两个 `cmp …,5` 的门，推导见 `signal_core.h`）。
 *   两个保持计数在这里维护（卡状态；哪个键松开哪个归零）。*/
static void signal_support_patchouli(signal_card_t *s)
{
    uint32_t in = CE_INPUT_HELD();

    s->z_held     = (in & SIGNAL_IN_SHOT)  ? (s->z_held + 1)     : 0;
    s->shift_held = (in & SIGNAL_IN_FOCUS) ? (s->shift_held + 1) : 0;
    if (!signal_core_patchouli_fresh(in, s->z_held, s->shift_held)) { s->patch_latch = 0; return; }
    if (s->patch_latch) return;
    s->patch_latch = 1;
    s->formation = signal_formation_wrap(s->formation + 1);
    ++s->stat_patchouli;
    ce_log("signal: 帕秋莉·子机配置 -> 元素 %d（档 %d 的子机组 0x%02x / 主炮组 0x%02x）",
           (int)s->formation, signal_marisa_level(),
           (int)s_marisa_opt[signal_marisa_elem(s)][signal_marisa_level()][0],
           (int)s_marisa_main[signal_marisa_elem(s)][signal_marisa_level()]);
}

/* 本帧声明（**每帧都要调**：声明层在 `BP_ce_item_frame` 每帧清集合，不声明 = 本帧退出）。
 * ★★ 2026-09-16：**格数随档**（用户口径 ②）——档 0 一格不申请，档 N 申请 N 格，
 *   第 i 格的位置 = 本档阵型的第 i 格（`signal_formation_cell`）；满档第一次看到**整个阵型**。
 *   合计输出由 `*_core.h` 的 DPS 归一守住（4 格 × 20/12 = 原生满档子机层）。*/
static void signal_option_declare(ce_card_t *c, signal_card_t *s)
{
    ce_option_pose_t pose;
    signal_cell_t cell;
    int i, n = signal_power_level();

    if (n <= 0) {                       /* 档 0（或取不到档）⇒ 本帧退出声明 */
        (void)ce_option_pose_declare(c, NULL);
        return;
    }
    if (n > SIGNAL_OPTION_MAX) n = SIGNAL_OPTION_MAX;
    memset(&pose, 0, sizeof pose);
    pose.anm_script = signal_option_script_for(*(const int32_t *)CE_ADDR_CHARACTER, s->formation);
    for (i = 0; i < n; ++i) {
        /* ★★ 2026-09-16m：**格数 < 4 时取对称的那些格**（用户实跑：「子机的位置摆放不对，
         *   **左倾严重，低 p 不对称**」）。原来的写法是「取阵型表的前 n 格」——而表是**左右成对**
         *   排的（0=最左 … 3=最右）⇒ 2 格时拿到的是**左边那两格**（整体偏左）、3 格时偏左一列。
         *   `signal_formation_cell_sym()` 改成按对称轴取：1 格 = 轴上、2 格 = 最内那一对、
         *   3 格 = 最内一对 + 轴上、4 格 = 整张表（表本身左右对称）。*/
        /* ★ 2026-09-16zb：一手阵型 —— 取「本档块」的第 i 格（`T(档-1)+i`）。
         *   一手表每档的块本身就是该档的 n 个格子且 x 对称 ⇒ 旧的自定"按对称轴凑格"整条退役。*/
        if (!signal_formation_cell_lv(s->formation, n, i, &cell)) break;
        /* 单位 = **像素**（引擎自己 ×128 变原生定点；`ce_optpose` 的契约见 `sdk.h`）。
         * 我们的表是**十分之一像素**（整数纪律）⇒ 这里 ÷10。两套（聚焦/非聚焦）给同值
         * （原版五档也只看一套；给同值 ⇒ Shift 切换没有位置跳变）。*/
        pose.cell[i].fx = pose.cell[i].ux = (float)cell.x10 / 10.0f;
        pose.cell[i].fy = pose.cell[i].uy = (float)cell.y10 / 10.0f;
    }
    pose.count = i;                     /* 阵型表给不出那么多格时按实际填到的格数声明 */
    (void)ce_option_pose_declare(c, (pose.count > 0) ? &pose : NULL);
}

/* 落笔（声明层每帧只调**被选举者**一次；被顶掉时再补叫一次「退位通知」）。
 * ★ 纪律（`sdk.h` 的低速子机声明层）：**只在这里**申请子机 / 写偏移字段；
 *   108（id 108 < 111）同装时它赢 ⇒ 我们在退位通知里**只收子机、不写字段**。*/
static int on_option_pose_apply(ce_card_t *c)
{
    signal_card_t *s = st(c);
    const ce_option_pose_t *pose = ce_option_pose_pose();
    int i, want;

    if (!s) return 0;
    if (ce_option_pose_winner() != (int)ce_card_id(c) || !pose || pose->count <= 0) {
        for (i = 0; i < SIGNAL_OPTION_MAX; ++i) {
            if (s_opt[i].ptr) ce_option_drop(c, &s_opt[i]);
            s_opt[i].ptr = 0;
            s_opt[i].retry = 0;
        }
        s->opt_script = -1;
        s->opt_on = 0;
        return 0;
    }
    if (s->opt_script >= 0 && s->opt_script != pose->anm_script) {
        /* 子机贴图脚本是**申请时**定死的（`Player__allocate_option` `th18:0x40a790` 第 5 个栈参）
         * ⇒ 换脚本必须先收再要。（本卡当前只有一个脚本 ⇒ 这条现在走不到，留着防将来换图。）*/
        for (i = 0; i < SIGNAL_OPTION_MAX; ++i) {
            if (s_opt[i].ptr) ce_option_drop(c, &s_opt[i]);
            s_opt[i].ptr = 0;
        }
        s->opt_script = -1;
    }
    want = pose->count;
    if (want > SIGNAL_OPTION_MAX) want = SIGNAL_OPTION_MAX;
    for (i = 0; i < want; ++i) {
        uint8_t *o = ce_option_get(c, &s_opt[i], SIGNAL_OPTION_OFFSET_ALLOC, pose->anm_script,
                                   SIGNAL_OPTION_RETRY, "signal");
        if (!o) continue;
        s->opt_script = pose->anm_script;
        s->opt_on = 1;
        *(int32_t *)(o + CE_OPT_OFF_NF_X) = (int32_t)(pose->cell[i].ux * 128.0f);
        *(int32_t *)(o + CE_OPT_OFF_NF_Y) = (int32_t)(pose->cell[i].uy * 128.0f);
        *(int32_t *)(o + CE_OPT_OFF_F_X)  = (int32_t)(pose->cell[i].fx * 128.0f);
        *(int32_t *)(o + CE_OPT_OFF_F_Y)  = (int32_t)(pose->cell[i].fy * 128.0f);
    }
    return 0;
}

/* 开火（虚表 `+0x1c`；一手：引擎在 `th18:0x45eca0` 沿卡链对**每张卡**调它）。
 * 先例 = `season.c` 的 `on_tick_shooters`（同一件事：卡侧自己 tick 自己那一组 shooterset）。
 *
 * ★★ 2026-09-16zb：**喂给行的计时器改成引擎给的那一个**（一手，用户口径「发弹数得大致对上」）。
 *   一手链条（TH18，逐条反汇编）：
 *     · `0x45eba5` 取 `player+0x476d4`（短计时器 = 射击计时器）与 `+0x476d0`（上一帧值）比，
 *       相等就跳过整段（`0x45ebb3 je`）⇒ 「这一帧计时器动过」才往下；
 *     · `0x45ecb8 call [eax+0x1c]` 推的两个参数就是 **`(short = +0x476d4, long = +0x476e8)`**
 *       ⇒ **本钩子拿到的 `short_timer` 正是引擎的开火计时器本身**；
 *     · 生命周期：`0x45a550` = `set(arg)`（`cur = arg`、`prev = arg-1`、`cur_f = arg`；按 Z 起手时
 *       `push 0` ⇒ 从 **0** 起算，`0x45eb9c`）；`0x45eccb` `cmp cur,0xe` ⇒ `>= 14` 走回绕那条
 *       （`push 0xe` / `call 0x452bf0`），`< 14` 走 `call 0x405990` 正常 +1 ⇒ **取值域 0..14、周期 15**。
 *   ⇒ TH11 是同一套（`th11:0x4343d1 mov ebx,[edi+0x934]` → `0x4343d7 call 0x4342f0` 判开火；
 *     `0x459270` increment；`0x4343f2 fld [0x498300]` = **-14.0f** + `0x459210` add ⇒ 同样 0..14 / 周期 15）
 *     ⇒ 行判据 `timer % fire_rate == start_delay`（`0x40aa47`–`0x40aa67`）在 `rate = 3` 时
 *     **15 帧正好 5 发/格**，与 TH11 元素子机行（`pl01b.sht` 一手：火/木/土 rate 3）逐值相同。
 *   ⚠ 旧版喂的是**本卡自己的帧计数**（`s->frame` + `rate = 60`）= 每 60 帧 1 发/格，比原版慢 4 倍；
 *     `s->frame` 现在只用于诊断（`stat_opt_tick` 那一路），不再参与开火相位。*/
/* ★ 2026-09-16u 诊断：本卡喂给引擎的最后一组参数（给 `signal/opt:` 那行用）+ 一次「引擎给了什么」的对照。*/
static int32_t s_opt_last_set;
static int     s_opt_timer_logged;

static int on_tick_shooters(ce_card_t *c, uint32_t short_timer, uint32_t long_timer)
{
    signal_card_t *s = st(c);
    int elem, lv, i;

    if (!s || !s->opt_on) return 0;
    if (!s_opt_timer_logged) {                     /* 只打一次（引擎给的两个计时器 vs 我们传的） */
        s_opt_timer_logged = 1;
        ce_log("signal/opt2: 引擎调用 on_tick_shooters(+0x1c) 时给的计时器 short=%u long=%u；"
               "本卡**原样转给** `ce_tick_shooters_for_card()`（一手：行判据 = `timer %% fire_rate == start_delay`，"
               "`th18:0x40aa47`–`0x40aa67`；短计时器取值域 0..14 / 周期 15，见 `th18:0x45eccb` 与 "
               "`th11:0x4343f2` 的 -14.0f）",
               (unsigned)short_timer, (unsigned)long_timer);
    }
    elem = signal_marisa_elem(s);
    lv   = signal_marisa_level();
    if (lv <= 0) return 0;                         /* 档 0 = 零子机（TH11 的档 0 组里没有子机行）*/
    for (i = 0; i < SIGNAL_OPTION_MAX && i < CE_SHT_MARISA_CELLS; ++i) {
        int set = (int)s_marisa_opt[elem][lv][i];
        uint8_t *o;

        if (set < 0) continue;                     /* 该档没有这一格（一手：档 N 只有前 N 格有行）*/
        o = ce_option_claim(c, &s_opt[i]);
        if (!o) continue;
        s_opt_last_set = set;
        ce_tick_shooters_for_card(o, (int)short_timer, 0, set);
        ++s->stat_opt_tick;
    }
    return 0;
}

/* ★★ 2026-09-16zb：**主炮组覆盖**（TH11 的主炮每发伤害逐元素逐档不同：16→8..16 ⇒ 8.00~10.67/帧）。
 *   一手：断点 `th18:0x45ebfb`（`Player__tick_shooting_state` 查主炮 set 前）—— 返回 >= 0 就换掉
 *   本帧的 set index（`sdk.c` 的 `BP_ce_shot_set_index` 同时改 edx 与 `[ebp-8]`）。
 *   我们返回的那一组**只有 2 行 `slot 0` 的主炮行**（数值 = TH11 一手值，弹/音效 = **本作原生** ⇒
 *   用户 2026-09-16zb 口径「主炮别换弹幕了」）。原生子机行（`slot ≥ 1`）本来就不会发：
 *   `Player__shoot_one_bullet`（`0x45e930`）在 128 格池里找不到活子机时**直接 `return 0`**，
 *   而原生那 4 格子机被本卡整层关掉（`ce_native_option_hide_declare`）⇒ 不会误发、不会重复计伤。
 *   ⚠ 优先级 = `CE_SHT_PRIO_111`（= **−1**，全场最低）：108（低速妖机体，默认 0）/113（10）/117（20）
 *     任一生效都能盖住它 —— 这就是用户 2026-09-16z 定的阶梯 **117 > 113 > 108 > 111**。
 *   ⚠ 机体门：只有当前机体 == 魔理沙（`SIGNAL_CHAR_MARISA`）才覆盖（与子机层同一道门）。*/
static int on_shot_set_index(ce_card_t *c, int power_level)
{
    signal_card_t *s = st(c);
    int elem, lv;

    if (!s) return -1;
    if (*(const int32_t *)CE_ADDR_CHARACTER != SIGNAL_CHAR_MARISA) return -1;
    elem = signal_marisa_elem(s);
    lv = power_level;
    if (lv < 0) lv = 0;
    if (lv >= CE_SHT_MARISA_LEVELS) lv = CE_SHT_MARISA_LEVELS - 1;
    return (int)s_marisa_main[elem][lv];
}

/* ---- ⑥ 荷取 · **河童罩子**（每帧推进）------------------------------------------------
 * ★★ 2026-09-16q：这一块**整段重写过** —— 一手全链（逐条地址 + 脚本原文 + 我们的接法）
 *   只在**一处**：本文件上面「★★★ 2026-09-16q：河童罩子」那一段（`signal_shield_*` 一族）。
 *   旧版那套「75 帧窗口 + 扣多少退多少 + 用 `player_state ∈ {2,4}` 判被撞」的证据链已经作废：
 *   · 75 是**破罩伤害源**的寿命，不是罩子寿命（罩子 = 920 帧）；
 *   · 退款额度一手 = **半档**（`collect_power(10)` vs `BOMB_COST` 20），不是全额；
 *   · 判据一手 = 「**泡的 VM 还在不在**」，不是「玩家状态」。
 *   ⚠ 这一发的退款账在文件静态 `s_nitori`（`pending` / `broken` / `refunds` / `denials`）、
 *     泡的 VM 句柄在 `s_shield` —— 都不在卡状态里（496 字节上限）。
 *   ⚠ 每帧推进**不在这里**：见 `on_tick_2` 的 ⑧-5 段（`signal_shield_tick()`）。*/

/* 魔理沙三条（机体 1）。★ 2026-09-15g：爱丽丝那条**现在已实装**，但它**没有每帧动作**
 * （两条都挂在事件钩子上：灵击 → `signal_bomb()`、P 点 → `signal_on_item_commit()`，机体门各自就地判）
 * ⇒ 这里只有一条「每帧」的活（帕秋莉换档）+ 子机声明。
 * ⚠ 荷取那条（河童罩子）**已经搬出这里**：它的每帧推进在 `on_tick_2` 的 ⑧-5 段
 *   （`signal_shield_tick()`）—— 与机体无关地跑，这样**换机体 / 换卡的残余泡也能被收干净**。*/
static void signal_support_marisa(ce_card_t *c, signal_card_t *s)
{
    signal_support_patchouli(s);
    /* ★★ 2026-09-16（用户口径 ①）：「魔理沙原生子机需要弄掉」——自机**原生那 4 格子机**整层关掉。
     *   机制/断点/为什么两条 + 为什么还要一支自愈 sweep：`sdk.h` 与 `sdk.c` 的那一大段（唯一出处）。
     *   这里只做两件事：**声明**（粘性；机体门在层里 ⇒ 换机体立刻恢复，不靠本卡改口）
     *   + **每帧扫一遍**（卡刚装上时场上可能已有原生子机，档位短路缓存会让引擎不重建也不清）。
     *   ⚠ 本函数只在 `CE_ADDR_CHARACTER == 魔理沙` 时被调用（`on_tick_2` 的分派），
     *     所以声明里给的机体号恒等于「当前机体」；真实屏障仍是层里的机体门。*/
    ce_native_option_hide_declare((uint32_t)ce_card_id(c), 1, SIGNAL_CHAR_MARISA);
    (void)ce_native_option_sweep();
    /* ★★ 2026-09-18（用户实跑报）：**隐藏子机 ≠ 收掉它已经在打的激光** ——
     *   「魔理沙原子机激光没有被恰当抑制：在生成子机的那一个位置固定，之后不随自机移动，
     *     但会随着 Z 发射/消失；尤其是 power up 的时候，会换一个位置」。
     *   一手（`sdk.c` 的 `ce_option_kill_bullets` / `ce_kill_retail_option_lasers` 那两段注释）：
     *   `mode=2` 激光的寿命**只跟射击短计时（Z）走**（`0x4601e0 cmp [player+0x476d4],0`），
     *   它的位置每帧从**持有它的那个子机**读 ⇒ 我们把原生子机的 VM/状态清掉之后，束**不会自己消失**、
     *   而且因为持有者不再更新，它会**钉在**子机被清掉时(或 power up 后重建时)的那个位置 ✓ 正是症状。
     *   ⇒ 每帧补一刀：把 set ≤ 0x15（0..9 主炮 + 0x0a..0x15 原生装备子机组）里 `row+0x21 == 2`
     *     的活动弹按引擎自杀段收掉（`state=2` → 束 VM `interrupt 1` → 清 `player+0x476fc` 的
     *     mode=2 去重槽）。**不会碰我们自己的子机激光**（111 追加的组是 `0x29..0x3c`）。
     *   ⚠ 每帧叫是**故意**的：power up 会让引擎 `repopulate` 重建子机 ⇒ 总有那么一帧
     *     「子机刚复活」而我们的 sweep 还没轮到 ⇒ 那一帧发出来的束就是新的一条（用户看到的「换位置」）。
     *     代价 = 128 槽扫描（无命中时不打日志）。*/
    (void)ce_kill_retail_option_lasers();
    signal_option_declare(c, s);
}

/* ======================================================================================
 * 咲夜三条（机体 2）= 大小姐 + 二小姐 + 红美铃（用户拍板的 Σ 映射）
 * ====================================================================================== */

/* ---- ② 二小姐 · 蓄力 5 秒 ⇒ 松手清屏（半径 = 2×擦弹圈）
 * 用户口径：「按住 shift 时会蓄力，连续蓄力 5s 后放开 shift 可消除 2×擦弹圈半径范围的屏幕」。
 * 判据与帧数全在 `signal_flandre_tick()`（纯逻辑、单测）；卡侧只做三件事：
 *   读引擎（Shift 电平 / 自机态）→ 松手那一下**清屏** → 打一行日志。
 * ## 清屏口径 = **照本卡既有的灵击那一套**（一字不改）
 *   · `ce_cancel_radius(pos, r, MAX, mode 0)` = **纯删除**（不撒道具、不引 type 9 / 钱）；
 *   · **消弹闩锁**（`EVENTS-CONTRACT.md` §5.2 细则 2）：先 `ce_cancel_latch_claim()`，
 *     一颗都没消到 ⇒ `ce_cancel_latch_release()` **交回**（同帧别的卡还能做）；
 *   · 记账走同一个入口 `faith_cancel_account(n)` → `signal_on_cancel` → 小绿点每 100 个 +10。
 * ## 没有视觉/音效（**一手找不到就不做**）
 *   任务口径明写「蓄力期间要不要有视觉/音效：没有一手就不做，只打日志」——本轮没找到任何一手
 *   （TH11 里根本没有这条特技：咲夜/早苗这两组伙伴是用户自定的映射）⇒ **只打日志**。
 * ## ❓ 代价
 *   这条**不另收信号价**：它自带的代价就是「连续 5 秒按着 Shift」（低速、压制机动）。
 *   与上一轮那六条的价目表分开——用户 2026-09-15 给这一轮的口径里没有价（见 `TUNING.md` §2.1g）。*/
static void signal_support_flandre(signal_card_t *s)
{
    uint8_t *pl = CE_PLAYER();
    uint32_t in = CE_INPUT_HELD();
    int32_t pstate;
    int32_t charged_before;

    {
        int interrupted;

        if (!pl) return;
        pstate = *(const int32_t *)(pl + CE_PLAYER_STATE);
        interrupted = (pstate == 2 || pstate == 4) ? 1 : 0;
        charged_before = s->flandre.charge;

        if (!signal_flandre_tick(&s->flandre, (in & SIGNAL_IN_FOCUS) != 0u, interrupted)) {
            /* 只在**中弹**打断且真的丢掉了蓄力时打一行（「没充满就松手」是另一回事，由 core 记 aborted）。*/
            if (interrupted && charged_before > 0 && s->flandre.charge == 0) {
                ce_log("signal: 二小姐·蓄力被中弹打断（蓄到 %d/%d 帧，state %d）",
                       (int)charged_before, SIGNAL_FLANDRE_CHARGE_FRAMES, (int)pstate);
            }
            /* ★★ 2026-09-16（⑧-1 动画，**重做**）：**消弹范围环**（脚本 277）。
             *   用户口径（逐字）：「消弹范围应该是特效显示出来，类似于 th09 的固灵结界」。
             *   ⚠ 上一版**实跑看不见**（用户报「shift蓄力依然啥都没有，只有消弹结果」），两处错：
             *     ① 半径口径错：`scale` 是**倍率**不是半径 —— `drawPoly(R,n)` 写 `sprite_size.x = R`、
             *        绘制时 `最终尺寸 = sprite_size × scale`（`th16:0x4626eb` / `th16:0x468722`），
             *        上一版给 0.4→2.0 ⇒ 环只有 0.4–2.0 **px**。现在 `R` = **清屏半径本身（px）**。
             *     ② 图元错：`drawPoly` 是**实心盘**；界线要用 `drawPolyBorder`（脚本 277 已改成它，
             *        底本 = TH18 原生 `ability.anm` script35）。
             *   ★ 半径**不另算一份**：与清屏判定共用 `signal_flandre_cancel_radius_x100()` ⇒
             *     环 = 真的会被消掉的那一圈（口径见 `engine/card/th18/14-clear-range-barrier.md` §4.3）。
             *   层 13 = 脚本自己的 `layer(13)`（也是本卡爆闪 278 的层）。*/
            if (s->flandre.charge > 0) {
                int32_t ring_x100 = signal_flandre_cancel_radius_x100(
                    (int32_t)(*(const float *)(pl + CE_PLAYER_HITBOX_R_NF) * 100.0f));
                float ring_r = (float)ring_x100 / 100.0f;

                if (!s->flan_ring_vm || !ce_anm_get_vm(s->flan_ring_vm)) {
                    const float *pp0 = ce_player_pos();
                    s->flan_ring_vm = ce_anm_spawn(CE_ABILITY_ANM(),
                                                   CE_ANM_ABILITY_SCRIPT_FLAN_CHARGE_RING, 13);
                    if (s->flan_ring_vm && pp0) {
                        ce_anm_set_pos(s->flan_ring_vm, pp0[0], pp0[1], pp0[2]);
                    }
                    ce_log("signal: 二小姐·范围环起 VM（id %u，脚本 %d，层 13，满半径 %.2f px，"
                           "比例 %.2f→1.00 —— 跟着蓄力长大）",
                           (unsigned)s->flan_ring_vm, (int)CE_ANM_ABILITY_SCRIPT_FLAN_CHARGE_RING,
                           (double)ring_r,
                           (double)((float)signal_flandre_ring_ratio_permille(s->flandre.charge) * 0.001f));
                }
                if (s->flan_ring_vm) {
                    /* ★★ 2026-09-18（用户口径）：「咲夜蓄力最好张开一个**逐渐变大的圆**，
                     *   这样**最大了就知道满了**」⇒ 半径 = 清屏半径 × `signal_flandre_ring_ratio(charge)`
                     *   （0.12 → 1.0 线性）：长满那一刻既报"满"，又正好等于松手会消掉的那一圈。
                     *   ⚠ 判定半径**不跟着变小**（`r_x100` 仍是满值）—— 比例只用于画。*/
                    const float *pp0 = ce_player_pos();
                    /* 千分比 → 倍率（本仓口径：浮点**只在卡里算**，core 一律走定点）*/
                    float k = (float)signal_flandre_ring_ratio_permille(s->flandre.charge) * 0.001f;
                    if (pp0) ce_anm_set_pos(s->flan_ring_vm, pp0[0], pp0[1], pp0[2]);
                    (void)ce_anm_set_scale(s->flan_ring_vm, ring_r * k, ring_r * k);
                }
            } else if (s->flan_ring_vm) {
                /* 蓄力掉了（中弹打断 / 没满就松手）⇒ 收光环（脚本 277 的中断 1 = 20 帧淡出后自删）。*/
                if (ce_anm_get_vm(s->flan_ring_vm)) ce_anm_interrupt(s->flan_ring_vm, 1);
                s->flan_ring_vm = 0;
            }
            return;
        }
        /* 松手且蓄满 ⇒ 收光环（淡出），随后由下面那一段放爆闪 */
        if (s->flan_ring_vm) {
            if (ce_anm_get_vm(s->flan_ring_vm)) ce_anm_interrupt(s->flan_ring_vm, 1);
            s->flan_ring_vm = 0;
        }

        /* ★★ 2026-09-16 加：**蓄力的音效**（用户口径「二小姐特技的蓄力得有音效和动画」）。
         *   一手（子代理取到，全链见 `engine/card/th18/13-mallet-clear-and-th17-charge.md`）：
         *   · **TH17 妖梦的蓄力**（`th17:0x44a8d0`、计时器 `player+0x19084`、阈值 60 帧）：
         *     起手 + 每帧 `0x1e` = `se_cat00.wav`；蓄力中再叠一层 `0x37` = `se_release.wav`；
         *     **蓄满不加新音**（只停 `se_cat00` + 重起 `se_release`）。
         *   · **TH18 原生卡 42（小槌）的挥击音** = `0x29`（我们借它当「蓄满」那一声）。
         *   ⚠ **按 wav 名搬、不按 id 搬**（两作的 id→wav 不是同一套）：TH18 名表里
         *     `se_cat00` = `0x21`、`se_release` = `0x4d`。
         *     ⚠ `0x29` 这一格有**名字分歧**：本仓音效表写 `se_kira00.wav`、子代理直接读 TH18 名表
         *       （`0x4b47a0`）得 `se_ok00.wav` ⇒ 这里只搬 **id**（卡 42 真正播的那一格），名字待核。
         *   ⚠ 循环音的**节流 = ❓ 自定**：一手每帧起一声，我们 30 帧一次（`TUNING.md` §2.6g）。*/
        {
            const float *px = ce_player_pos();
            const float sx = px ? px[0] : 0.0f;

            if (charged_before == 0 && s->flandre.charge > 0) {
                ce_play_sound(SIGNAL_SE_FLAN_CHARGE, sx);          /* 起手：se_cat00 (0x21) */
            }
            if (s->flandre.charge > 0 && (s->flandre.charge % SIGNAL_SE_FLAN_LOOP_EVERY) == 0) {
                ce_play_sound(SIGNAL_SE_FLAN_LOOP, sx);            /* 循环：se_release (0x4d)，节流 ❓ 自定 */
            }
            if (charged_before < SIGNAL_FLANDRE_CHARGE_FRAMES &&
                s->flandre.charge >= SIGNAL_FLANDRE_CHARGE_FRAMES) {
                ce_play_sound(SIGNAL_SE_FLAN_FULL, sx);            /* 蓄满：0x29（借卡 42 的挥击音）*/
            }
        }
    }

    /* ★ 松手 ⇒ 清屏。半径 = `2 × (自机判定半径 + 40.0)`（一手：擦弹圈里自机那一半 + 额外圈下限）。*/
    {
        const float *pp = ce_player_pos();
        int32_t r_x100 = signal_flandre_cancel_radius_x100(
            (int32_t)(*(const float *)(pl + CE_PLAYER_HITBOX_R_NF) * 100.0f));
        float r = (float)r_x100 / 100.0f;
        float pos[3];
        int n = 0;

        if (!pp) return;
        pos[0] = pp[0]; pos[1] = pp[1]; pos[2] = pp[2];
        /* ★★ 2026-09-16（⑧-2 消弹特效）：**爆闪** —— 脚本 278 = 原生卡 42（小槌）挥击的配方
         *   （sprite 431→432→433→434 四格推进 7 帧 + 一手 8 帧淡出，VM 存活 15 帧，自己 delete）。
         *   ⚠ **不要**对它调 `ce_anm_set_scale()`：脚本自己在跑两段 `scaleTime`，卡侧写会被插值器盖掉
         *     （`sdk.h:330`）；出生点写一次 entity_pos 就够。不用记 id（它自删）。*/
        {
            uint32_t bv = ce_anm_spawn(CE_ABILITY_ANM(), CE_ANM_ABILITY_SCRIPT_FLAN_CLEAR_BURST, 13);
            if (bv) ce_anm_set_pos(bv, pos[0], pos[1], pos[2]);
        }
        if (ce_cancel_latch_claim((uint32_t)SIGNAL_ID_NUM)) {
            n = ce_cancel_radius(pos, r, SIGNAL_BOMB_CANCEL_MAX, 0);
            if (n > 0) {
                faith_cancel_account(n);          /* 与灵击同一个记账入口（小绿点每 100 个 +10）*/
            } else {
                (void)ce_cancel_latch_release((uint32_t)SIGNAL_ID_NUM);   /* 空认领 ⇒ 交回（同 §5.2）*/
                ce_log("signal: 二小姐·清屏消掉 0 颗（池内已无弹 / 原生卡先清）⇒ 交回消弹闩锁");
            }
        } else {
            ce_log("signal: 二小姐·清屏被消弹闩锁挡下（本帧已有别的卡消过弹）");
        }
        ce_log("signal: 二小姐·蓄力满 %d 帧后松手 ⇒ 清屏 r=%.2f px (2×擦弹圈) cancelled %d",
               SIGNAL_FLANDRE_CHARGE_FRAMES, (double)r, n);
    }
}

/* ---- 咲夜三条的每帧入口（机体 2）。⭐ ①（大小姐）与 ③（红美铃）没有每帧动作 ——
 *   ① 在 `signal_step_mod_fill()` 里扫池（它的收益要进本帧唯一的槽推进那一句）；
 *   ③ 是那一句的一个门。这里只剩二小姐的蓄力。*/
static void signal_support_sakuya(signal_card_t *s)
{
    signal_support_flandre(s);
}

/* ======================================================================================
 * 早苗三条（机体 3）= 诹访子 + 神奈子 + 恋恋（用户拍板的 Σ 映射）
 * ====================================================================================== */

/* ---- ⑤ 神奈子 · 收点线下降（★ **已迁移到共享 PoC 高度层**，2026-09-15）
 * 口径与一手（字段 `player+0x47998`、两个写点 `0x45ae8f`=128.0 / `0x40c275`=224.0、
 * 以及「为什么用快照基准 + 相对下移」）**全在 `signal_core.h` ⑤**。
 * ## ✅ 迁移结果（原来标 ⏳ 的三处，全部改走共用件）
 *   ① 读现值 → `ce_poc_height()`（`sdk.h` 的 PoC 高度层访问器；与字段同值、空指针安全）；
 *   ② 落生效值 → **不再直写**：`ce_poc_declare_px_id()` 声明 + `ce_poc_apply()` 让它当场生效；
 *   ③ 删卡 / 换关的写回 → `ce_poc_release()`（层负责把「我们动手之前」的字段原值写回去，
 *      比本卡原来写回自己的快照基准更准 —— 原生卡 22 中途把线抬到 224 时也不会被我们顶掉）。
 * core 的 `signal_kanako_t`（快照 + 相对下移）**原样保留**：它是可测的那一半，与「谁去写引擎」正交。
 * ⚠ 本函数**不**自己判「别人改过」——快照刷新是 core 的活（它比 `applied_x100`），
 *   而「谁最后落笔」是层的事（多条声明取最大，见 `sdk_core.h` 的 PoC 层）。*/
static void signal_support_kanako(signal_card_t *s)
{
    int32_t want;

    if (!CE_PLAYER()) return;
    /* ★★ 2026-09-20：**改读语义访问器**（`ce_poc_height_semantic()`）。
     *   理由：106 在不满 P 时把引擎字段「遮」到 `min(语义高度, 自机 y)`（那是给引擎两条判据看的），
     *   而本卡要的基准是**没有 106 时**那条线 —— 否则「快照 + 相对下移」的快照会跟着自机 y 抖，
     *   声明值每帧乱跳（`signal_kanako_t` 的快照刷新判据就废了）。
     *   ⚠ 副作用（owner 已拍，`WORK-PACKAGES.md` §7-A2）：106 在场时「遮」是**覆盖位** ⇒
     *   本卡这条「收点线下降」对**引擎**不生效（106 的语义优先）—— 111 的全屏吸收不再靠这条线；
     *   但本卡自己的信号档仍按**语义高度**算（= 没 106 时的语义）。*/
    want = signal_kanako_tick(&s->kanako,
                              (int32_t)(ce_poc_height_semantic() * 100.0f),
                              SIGNAL_KANAKO_POC_DROP_PX * 100);
    if (want <= 0) return;
    /* ★ 已迁移到共享层：只声明、不直写。落笔（写字段 + 置 `Gui+0x194` bit 0x2000 开那条开局指示线）
     *   由层做；这里额外调一次 `ce_poc_apply()` 是为了**开局那一刻就已经生效**
     *   （引擎那条线是开局建的，见 `engine/player/th18/05-poc-height-and-indicator.md` §3），幂等。*/
    ce_poc_declare_px_id((uint32_t)SIGNAL_ID_NUM, (float)want / 100.0f);
    ce_poc_apply();
}

/* ---- ⑥ 恋恋 · 放开全键位（除移动键）⇒ 4 帧无敌（**载体、条件与量全部照抄原生 29 Aya 卡**）
 * 原生实现（`CardAya__on_tick` `th18:0x40c7f0` 的 `0x40ca95`–`0x40cb2a`）逐条在 `signal_core.h` ⑥；
 * 判据与量在 `signal_koishi_tick()`（纯逻辑、单测）。
 * ★ 落点 = **SDK 的无敌声明层**（`ce_invuln_request` / `ce_invuln_apply()` 在帧界写 `player+0x47774`）——
 *   本卡**不**自己直写那个字段（107 结界结束 / 109 爆风 / 117 霊撃 也在声明它，层取 max、互不截短）。
 * ⚠ **声明是粘性的**（`s_invuln[id]` 一直到改口为止）⇒ 必须**每帧**声明：触发帧声明
 *   `SIGNAL_KOISHI_INVULN_FRAMES`（= **4**，原生量）、其余帧声明 0。
 *   落笔在**帧界**（卡 tick 之后的那次 `ce_invuln_apply()`）⇒ 实际生效到「下一帧的自机判定」上，
 *   时长仍是 4 帧（见 `sdk.h` ②的层语义）。*/
static void signal_support_koishi(signal_card_t *s)
{
    uint8_t *pl = CE_PLAYER();
    uint32_t in = CE_INPUT_HELD();
    int32_t dir = 0, focus = 0, fire;

    if (pl) {
        dir   = *(const int32_t *)(pl + SIGNAL_OFF_PLAYER_MOVE_DIR);
        focus = *(const int32_t *)(pl + SIGNAL_OFF_PLAYER_FOCUS) != 0;
    }
    fire = signal_koishi_tick(&s->koishi, in, dir, focus);
    ce_invuln_request((uint32_t)SIGNAL_ID_NUM, fire ? SIGNAL_KOISHI_INVULN_FRAMES : 0);
    if (fire) {
        ce_log("signal: 恋恋·放开键 ⇒ %d 帧无敌（dir=%d focus=%d，量 = 原生 Aya 卡 +4 帧）",
               SIGNAL_KOISHI_INVULN_FRAMES, (int)dir, (int)focus);
    }
}

/* ---- 早苗三条的每帧入口（机体 3）。④（诹访子）与③无关、也没有每帧动作：
 *   它只在本帧的信号推进那一句里把档位换成「不推进」（`signal_step_mod_fill()`）。*/
static void signal_support_sanae(signal_card_t *s)
{
    signal_support_kanako(s);
    signal_support_koishi(s);
}

/* 收干净三条特技的跨帧状态（`on_delete` / `on_run_reset` / 每关开场都走这里）。
 * ⚠ 只有一处**卡外**的痕迹：`CE_PLAYER_SPEED_MULT` —— 引擎每帧自己复位 1.0
 *   （`th18:0x45c702`），但删卡发生在**商店**里（那一刻 Player tick 冻住）⇒ 显式写回 1.0。
 *   ⚠ 商店里玩家也不会移动，这只是「不留半格状态」的纪律，不是必须。*/
static void signal_support_reset(ce_card_t *c, signal_card_t *s)
{
    uint8_t *pl = CE_PLAYER();
    int i;

    /* ★ 子机按声明层的纪律收回：`ce_option_drop` 会删 ANM VM 并松开池槽
     *  （删卡发生在商店里 ⇒ 只能在这里收，不能等 tick）。*/
    if (c) {
        for (i = 0; i < SIGNAL_OPTION_MAX; ++i) {
            if (s_opt[i].ptr) ce_option_drop(c, &s_opt[i]);
            s_opt[i].ptr = 0;
            s_opt[i].retry = 0;
        }
        ce_option_pose_drop(c);      /* 退出子机声明层（退位/删卡都要）*/
    }
    s->opt_script = -1;
    s->opt_on = 0;
    memset(&s->gap, 0, sizeof s->gap);
    memset(&s->suika, 0, sizeof s->suika);
    memset(&s->aya, 0, sizeof s->aya);
    s->aya_on = 0;
    s->suika_on = 0;
    /* ★ 2026-09-15h：爱丽丝那条的叠加相位**已经不归本卡**（搬进共享件 `faith.c` 的 `s_p_alt`，
     *   因为 P 点补值的落笔者恒为 110 ⇒ 相位必须放在两张卡都够得到的地方）。所以这里**没有任何
     *   需要为它留的例外**：本函数照旧把本卡自己的三条特技状态清干净即可。
     *   ⚠ 共享相位的「被删掉会怎样」由共享件负责（最后一张卡退注册时归零，见 `faith.h`）。*/
    /* ★★ 2026-09-15i：咲夜 / 早苗 那六条的跨帧状态。三条**卡外**痕迹要显式收拾：
     *   ① 收点线（神奈子）—— ★ 2026-09-15 **已迁移到共享 PoC 高度层**：`ce_poc_release()` 撤声明，
     *      并在**没人再声明**时由层把「我们动手之前」的字段原值**立刻**写回（删卡发生在商店里、
     *      那一刻帧钩子不跑 ⇒ 层是在本调用里就地写的）。本卡原来那套「写回自己的快照基准」已删。
     *   ② 无敌声明（恋恋）—— 声明层是**粘性**的 ⇒ 必须显式撤（`ce_invuln_request(id, 0)`）。
     *      ⚠ `ce_layer_drop_card()` 在 `on_run_reset` / `on_delete` 里**已经**会撤一次，
     *        但 `on_stage_start` **不走**那条路 ⇒ 这里无条件撤一次（幂等，且「换关不残留无敌」是硬要求）。
     *   ③ 二小姐那条没有卡外痕迹（蓄力值在卡里；消弹是瞬时的）。*/
    {
        /* ★ 2026-09-15 迁移：撤声明 + 「没人再声明 ⇒ 立刻写回字段原值」全归共享层（幂等）。*/
        ce_poc_release((uint32_t)SIGNAL_ID_NUM);
    }
    ce_invuln_request((uint32_t)SIGNAL_ID_NUM, 0);
    signal_remilia_reset(&s->remilia);
    signal_flandre_reset(&s->flandre);
    signal_kanako_reset(&s->kanako);
    signal_koishi_reset(&s->koishi);
    /* ★ 2026-09-16：二小姐蓄力光环（长持续效果铁律：删卡/换关都得把它收干净）。*/
    if (s->flan_ring_vm) {
        if (ce_anm_get_vm(s->flan_ring_vm)) ce_anm_interrupt(s->flan_ring_vm, 1);
        s->flan_ring_vm = 0;
    }
    /* ★★ 2026-09-16k/m：**灵击范围环**（脚本 279）—— 它自己不会死（`stop()` 之后只画）
     *   ⇒ 删卡 / 换关 / 局末三条路都必须显式删（`METHOD.md` 的长持续效果铁律）。*/
    signal_bomb10_kill();
    /* ★★ 2026-09-16q：**河童罩子的水泡**（脚本 294）—— 同上（它活的比灵击长得多：920 帧）。*/
    signal_shield_kill();
    s_trail_n = 0;                       /* ★ 残像池（VM 自己 20 帧寿命，池只清记账）*/
    signal_nitori_close(&s_nitori);     /* 退款窗一并作废（不退款、不计入诊断）*/
    ce_invuln_request((uint32_t)SIGNAL_ID_NUM, 0);   /* 破罩那 30 帧刷的无敌也撤回 */
    s->extra_graze = 0;
    s->suwako_on = 0;
    /* ★ 2026-09-16：换档的新鲜度计数（Z / Shift 各按住多少帧）—— 换关/复位时归零，
     *   否则「上一关按着的 Z」会让下一关第一帧就算「同时按下」。*/
    s->z_held = 0;
    s->shift_held = 0;
    s->patch_latch = 0;
    s->flan_ring_vm = 0;
    signal_tint_restore_all();     /* ★ 2026-09-16：把自己染红的弹还回原色（删卡/换关/换机体都走这里）*/
    if (pl) *(float *)(pl + CE_PLAYER_SPEED_MULT) = 1.0f;
}

/* ---- ★★ 2026-09-16q 中弹瞬间（虚表 `+0x10`：`Player__die` `th18:0x45d574`）----
 * **本卡挂这一位只为「河童罩子」**（不是 107/117 那种「中弹即自动爆」的救援层卡片语义）。
 * 一手（TH11，逐条）：罩子活着时挨的那一下由**泡**吃掉 ——
 * `Bomb::on_tick__marisa_c` 的 restart 块 `th11:0x4078c4 mov [player+0x928],1`
 * **把 state 从 4 拉回 1**（不扣命），同一块里播 SE 0x26、建那个 75 帧伤害源。
 * ⇒ 我们的 `+0x10` 做同一件事：`signal_cancel_impending_death()` + 泡破罩。
 *
 * 三处**逐条对照**（都写在这里，别在别处再推一遍）：
 *   ① **撤销中弹**：`th11:0x4078c4`（state←1）↔ `signal_cancel_impending_death()`
 *      （`th18:0x45d590` 体内 `+0x476ac = 1`）。不扣命、不进决死窗口。
 *   ② **信仰槽清零**：一手那条路的代价是 `0x4a56f4`（TH11 的通信强度/信仰槽）**被 state-4 分支清零**
 *      —— 而**不扣命**。我们的对应物 = `signal_core_reset(&s->slot)`（**只清槽，不写信仰 −100000**：
 *      那是 `Player::sub_4327d0`（真扣命那条路）里的事，罩子这条路走不到）。开关 =
 *      `SIGNAL_NITORI_BREAK_RESETS_GAUGE`，理由与「用户口径『不撞破罩子 = no miss』」的边界写在
 *      `TUNING.md` §2.5 那一段。
 *   ③ **不返值**：返回值被 `Player__die` 忽略（照 107/117 的约定，见 `senro.c` 的同款注释）；
 *      救命靠 `cancel_impending_death` 的副作用。救援闩锁照约定**无条件 claim 且丢弃返回值**
 *      （唯一消费者是 113）。
 * ⚠ **只置 pending、不在这里建 VM**（死亡栈纪律，见 `senro.c` 的文件头）——
 *   泡的破罩动画与伤害源都在 `signal_shield_tick()` 里做。*/
static int on_death_before_deathbomb(ce_card_t *c)
{
    signal_card_t *s = st(c);

    if (!s) return 0;
    if (!signal_shield_eat_hit()) return 0;         /* 没有泡 ⇒ 本卡不接这一下 */
    if (SIGNAL_NITORI_BREAK_RESETS_GAUGE) {
        int reset = signal_core_reset(&s->slot);     /* 一手：state-4 分支把信仰槽清零（不扣命）*/
        if (reset) ++s->stat_reset;
        signal_declare_score(s);
        ce_log("signal/shield: 罩子吃掉这一下中弹（不扣命）⇒ 信仰槽清零 %d（一手 `0x4a56f4`；"
               "**不**写信仰 −100000，那是真扣命那条路的事）", reset);
    } else {
        ce_log("signal/shield: 罩子吃掉这一下中弹（不扣命；槽不清零 = 开关关掉了）");
    }
    return 0;
}

/* ---- 被弹归零 + 信仰惩罚（★ 2026-09-15 补上信仰那一半）----
 * 挂虚表 `+0x0c`（`on_death_after_deathbomb`）：死亡序列里**决死窗口耗尽之后、引擎扣命之前**
 * 的广播（一手：`double.c` 的文件头 + `engine/card/th18/03-hooks.md` §4）。
 * 为什么不用 `+0x14`：那是 respawn 之后的钩子，那时归零已经太晚（SA 原版是**被弹那一刻**归零）。
 * 一手（`research-th11-signal.md` §4 + `research-th11-faith-and-points.md` §6.1 的 Miss #1/#2）：
 *   **同一个函数 `th11:0x4327d0` 里做两件事** —— ① `th11:0x432807` 写通信强度 = 0；
 *   ② `th11:0x4327e7: sub eax,0x989680` ⇒ 信仰 **显示 −100,000**，下限写 `MIN_PIV`（= 初值）。
 *   ⇒ 本卡两件都做（`signal_core_reset()` + `signal_core_miss_penalty()`）。
 * ⚠ 它覆盖的是「真被弹 / 真死」这条路径。★ 2026-09-16q 起「**河童罩子吃掉的那一下**」有了自己的
 *   路（上面 `on_death_before_deathbomb`）：罩子撤销中弹 ⇒ 引擎不进 case 4 ⇒ **本函数不会被广播**
 *   ⇒ 不会写信仰 −100000，槽由罩子那条路清（= 一手的行为）。*/
static int on_death_after_deathbomb(ce_card_t *c, uint32_t acc)
{
    signal_card_t *s = st(c);

    if (!s) return 0;
    /* ★★ 2026-09-13（原生卡冲突审计 A 通道）：**这一位不是「真 miss」就别记账**。
     *   `+0x0c` 的形参 = 引擎的累加器 `acc |= 前一张卡的返回值`（一手 `th18:0x45c116 push edi` /
     *   `0x45c121 or edi,eax`）：**非 0 = 前面已经有卡把这次死亡取消了**。原生
     *   `CardEirin__on_player_death_after_deathbomb` `th18:0x40a4f3`（第 1 条指令 `cmp [ebp+8],0; jnz 让位`）
     *   就是这条契约的样板 —— 我们以前忽略它，导致原生 23/35 救下时 111 也照样把信号槽清零。
     *   ⚠ 残余：**排在我们后面**的救援者（原生 35 不看 acc）救下时我们仍会记一笔；那一档要等
     *   「死亡结果闩锁」这个共享件（登记在 CARD-NATIVE-CONFLICT-AUDIT 的待拍表）。*/
    if (acc != 0) {
        ce_log("signal: +0x0c 死亡已被前面的卡救下（acc=%u）⇒ 不按 miss 记账", (unsigned)acc);
        return 0;
    }
    {
        int32_t lost = signal_core_miss_penalty(&s->slot);      /* 一手：显示 −100,000，下限 = 初值 */
        int reset = signal_core_reset(&s->slot);                /* 一手：同一个 miss 函数把槽清零 */
        if (reset) ++s->stat_reset;
        signal_declare_score(s);                                /* 信仰 / 信号都变了 ⇒ 重新声明 */
        ce_log("signal: miss -> gauge reset %d, faith -%d (now %d, lives %d)",
               reset, (int)lost, (int)faith_slot_total(&s->slot), (int)CE_CURRENT_LIVES());
    }
    return 0;                                            /* 不救命，只记账 */
}

/* ---- 得分跨阈值 ⇒ +1 残机（★ 2026-09-15 新增：一手路径 ①）----
 * 一手 `th11:0x41efd8`/`0x41efe3`（表 `th11:0x4a3b1c`）/ EX 表 `th11:0x41eff8`（表 `th11:0x4a3d60`）
 * → `th11:0x41f001: call 0x420950` = **残机 +1**（`0x4a5718`，钳 9），并把索引 `0x4a5750` +1。
 * ⚠ 阈值表**没能取到**（只有第 0 项可读 = 10^9；按它照读会得出「第 2 项 = 0 ⇒ 每帧发残机」这种荒谬
 *   结果 ⇒ 那张表不在那儿）⇒ 表里现在只有那一项、**且 TH18 的分数上限 999,999,999 低于它**
 *   ⇒ 现状**打不到**（不是 bug，是「表未取到 + 照抄已知项」的直接后果）。登记 `TUNING.md`。
 * ⚠ 索引一局内不重置（一手 `0x4a5750` 只在 `th11:0x41faf7` 一局开始时清 0；而我们的 `ctor` 每关都跑
 *   ⇒ **刻意不在 ctor 里清**，只在 `on_run_reset`（局末）清）。*/
static void signal_lifefrag_tick(signal_card_t *s)
{
    int ex = (signal_difficulty() == FAITH_DIFF_EXTRA);

    if (signal_core_lifefrag_score((int64_t)CE_SCORE(), &s->frag_idx, ex)) {
        ce_add_life();                                   /* 引擎自己的加法（钳上限 / 音效 / 特效）*/
        ce_gui_update_lives();                           /* 原生 add_life 后照例刷 HUD 残机行 */
        ++s->stat_extend;
        ce_log("signal: 得分跨阈值 -> +1 残机 (idx %d, score %d, lives %d)",
               (int)s->frag_idx, (int)CE_SCORE(), (int)CE_CURRENT_LIVES());
    }
}

/* ---- 每帧 ---- */
/* ★ 2026-09-18：本函数每帧补调一次（见 `signal_ensure_registered()` 的注释）。*/
static void signal_ensure_registered(ce_card_t *c);

static int on_tick_2(ce_card_t *c)
{
    signal_card_t *s = st(c);
    signal_ensure_registered(c);   /* ★ 中盘拿卡（give12）时 on_load 不会被调 ⇒ 每帧补一次（幂等）*/
    const float *pp;
    int32_t g, dg;
    int lives, pstate, band, died_frame = 0;
    /* ★★ 2026-09-15f 诊断（用户实跑「擦弹一点也不涨信号强度」）：记下**这一帧**信号槽推进的前后值，
     * 供 600 帧那条 `signal/sig:` 行对账（判读口径写在那一行）。*/
    int32_t sig_before = 0, sig_after = 0;
    /* ★ 2026-09-15h 诊断：P 点交替的**共享**相位（只读，见下面 `signal/support:` 那一行）。*/
    int32_t palt_items = 0, palt_lo = 0, palt_hi = 0;

    if (!s || !CE_PLAYER()) return 0;
    ++s->frame;
    pp = ce_player_pos();

    /* ⓪ ★★ 2026-09-16m：**先跑大小姐的擦弹扫描**（它会调引擎的 `do_graze` ⇒ 原生计数 `0x4ccd0c`
     *    在本帧就已经包含我们的那几颗）—— 必须在 ① 读 `g` **之前**，否则那笔增量会落到
     *    `last_graze` 之后、下一帧被当成「原生擦弹」再喂一遍（一次擦弹进两次槽）。
     *    ⚠ 机体门就在这里：只有咲夜(2) 有这条特技（`signal_graze_scan()` 里还会再判一次机体）。*/
    s->extra_graze = 0;
    if (*(const int32_t *)CE_ADDR_CHARACTER == SIGNAL_CHAR_SAKUYA) {
        s->extra_graze = signal_graze_scan(s);
    }

    /* ① 擦弹帧差 → 擦弹倍率（**整数阶梯** `min(⌊graze/100⌋,899)×10`，一手 `th11:0x41f4e0`）
     *    与「本帧擦弹给槽的增量」（一手按难度查表，见 `signal_core_graze_gain()`）。*/
    g = signal_graze();
    if (s->last_graze < 0 || g < s->last_graze) s->last_graze = g;
    dg = g - s->last_graze;
    s->last_graze = g;
    s->graze_mult = mof_core_graze_mult(g);

    /* ② 回收线档位（一手 `th11:0x420a70` 的四档；线上/线下用**引擎自己的**回收线 `player+0x47998`）。
     *    ⚠ 死亡 / 复活（state 2 / 4）时**既不加也不减**（原版那两帧同样不看回收线）⇒ `BAND_HOLD`。*/
    pstate = *(int32_t *)(CE_PLAYER() + CE_PLAYER_STATE);
    if (!pp || pstate == 2 || pstate == 4) {
        band = FAITH_SIGNAL_BAND_HOLD;
    } else {
        /* 离回收线多少 px（**1/100 px 整数**交给纯逻辑层，浮点只在这里出现一次）。
         * ★★ 2026-09-20：**改读 `ce_poc_height_semantic()`**（= 不含 106「遮」的生效值）——
         *   信号档的语义必须 = **没有 106 时**（用户口径「106 只做过线不收点，其它语义零变化」）。
         *   原来那一支是 `faith_player_poc_height()`（= `ce_poc_height()` = 引擎字段真值），
         *   106 在场时它被压在自机 y 上 ⇒ 档位会随自机位置乱跳。
         *   ⚠ 110（`cards/mof.c`）仍在用 `faith_player_poc_height()`：那条是 110 的活、跨卡不许动，
         *     已登记在报告与 `AUDIT.md` §BE 的「残余」一行（106+110 同场时 110 的蓝色系数会读到遮值）。*/
        band = signal_core_band((int32_t)((ce_poc_height_semantic() - pp[1]) * 100.0f));
    }

    /* ③ 信号槽推进（档位 + 擦弹，全在 `signal_core` / `faith_signal_step` 里算）
     * ★★ 2026-09-15f：前后值各记一份给诊断（下一帧再读就晚了 —— 同帧里 ④ 的 miss 归零会盖掉）。
     * ★★ 2026-09-15i：**咲夜 / 早苗 的三条修饰**在这一句之前算出来（就本卡私有，共享件一字不改）：
     *   · 大小姐(2)：`signal_graze_scan()` 本帧**额外**计入的擦弹 → 直接加进喂槽的擦弹数
     *     （与原生帧差 `dg` **同一条路、同一本账** —— 不再开第二本账；`0x4ccd0c` 那个共享计数**不写**，
     *      理由与「实跑怎么分辨」写在 `signal_core.h` ① 与 `TUNING.md` §2.1g）；
     *   · 红美铃(2)：正增量 ×1.5 + 下限（`mod.meiling_on`）；
     *   · 诹访子(3)：贴板底 ⇒ 线下那一档换成「不推进」（`mod.at_bottom`）。*/
    {
        signal_step_mod_t mod;

        signal_step_mod_fill(s, &mod);
        sig_before = s->slot.signal;
        /* ★ 2026-09-16m：**不再加 `extra_graze`** —— 那几颗已经进过原生计数（见 ⓪ 段），
         *   加第二次就是「一次擦弹喂两遍」。*/
        (void)signal_core_apply_step_mod(&s->slot, band, (dg > 0 ? dg : 0),
                                        signal_difficulty(), &mod);
        sig_after = s->slot.signal;
        /* ★ 擦弹倍率也走**同一条**（`mof_core_graze_mult()`）：大小姐的额外擦弹把它当成
         * 「原生计数 + 本卡累计的额外次数」—— 累计值在 `remilia.total` 里（跨关保留到 `reset`）。
         * ⚠ 它今天只进诊断 / HUD 预览（计分层的耦合项用的是信号强度那条声明），
         *   将来若把擦弹倍率接回计分，这里**不需要**再改（口径已经在同一条路上）。*/
        if (s->remilia.total > 0) s->graze_mult = mof_core_graze_mult(g + s->remilia.total);
    }

    /* ④ 被弹归零的**兜底**：命数减少 ⇒ 重置 + 信仰惩罚（`+0x0c` 钩子没抓到的路径，
     *    例如某个非死亡流程扣命）。一手同源：miss 那一个函数里两件事一起做。
     * ★ 2026-09-15e：命数差分同时也是**命碎段机**的「这一段死没死」判据（见 ④b）。*/
    lives = CE_CURRENT_LIVES();
    died_frame = (s->last_lives >= 0 && lives < s->last_lives);
    if (died_frame) {
        (void)signal_core_miss_penalty(&s->slot);
        if (signal_core_reset(&s->slot)) ++s->stat_reset;
    }
    s->last_lives = lives;

    /* ④b ★★ 2026-09-15e **命碎段机**（用户口径「每两次这样掉 1 个命碎」；一手是 1:1）——
     *   段 = boss 的一个攻击相位（判据/粒度/近似全在 `signal_core.h` 的「命碎段机」段）。
     *   卡侧只有三件事：读引擎（boss + 相位指纹 + 符卡位）→ 喂命数差分 → 满 N 时撒道具。
     *   ⚠ `died_frame` 的语义 = **真死**（命数减少）；**被 107 / 117 / 113 救下 ⇒ 命数不减 ⇒ 视为没死**
     *     —— 与 TH11 一手同向（`$CAPTURE` 只在决死窗口过期的真死那一路被清，灵击/决死救回都不清）。
     *   ⚠ 位置：放在本卡自己的账（④）之后、灵击（⑥）之前 —— 与本帧谁消弹/谁灵击无关。*/
    {
        signal_seg_in_t sin;
        uint8_t *boss_e = signal_boss_enemy();
        int32_t drop = 0;

        sin.boss_present = boss_e ? 1 : 0;
        sin.spell_active = ce_spell_active();
        sin.phase_sig    = signal_phase_sig(boss_e);
        /* ★ 2026-09-16：对话中段机闭嘴（见 `signal_core.c` 里那一段：否则「关底 boss 被击破 →
         *   对话 → boss 对象释放」会被当成又一次段收尾 ⇒ 实跑多掉一颗命碎）。*/
        sin.dialogue     = CE_GUI_MSG_ACTIVE() ? 1 : 0;
        (void)signal_seg_tick(&s->seg, &sin, died_frame, &drop);
        if (drop) signal_seg_drop(s, boss_e);
    }

    /* ⑤ 满槽（≥100.00）⇒ 把落下的道具切 state 3（一手 `th11:0x42364a` → `0x423805`）*/
    if (signal_core_is_full(s->slot.signal)) signal_sweep_items(s);

    /* ⑥ 灵击（库存 == 0 且炸弹键上升沿）
     * ★ 2026-09-15c：**决死窗口内**按 B ⇒ 同一支路走**决死形态**（发动 + 救命；消弹照常记账 = 一手）。
     *   窗口判据 = `player+0x476ac == 4`（一手 `th18:0x45d4e4`，全 exe 唯一写 4 的点）；
     *   判据本体在 core（`signal_core_deathbomb_window()`）⇒ 主机单测钉得住，
     *   卡侧在这里只做「检测窗口 → 传参」（本仓硬要求：状态/语义进 core）。
     * ⚠ **不做**别的救人事（不挂 `+0x10`）—— 见上面「决死形态」那段与 `EVENTS-CONTRACT.md` §5.2。*/
    if (CE_CURRENT_BOMBS() <= 0 && (*(uint32_t *)CE_ADDR_INPUT_PRESSED & SIGNAL_INPUT_BOMB)) {
        uint8_t *pl = CE_PLAYER();
        int32_t ps = pl ? *(int32_t *)(pl + CE_PLAYER_STATE) : -1;
        int32_t by_hit = signal_core_deathbomb_window(ps);
        if (by_hit)
            ce_log("signal: 决死窗口内按 B（state %d == 4，库存 0）⇒ 本发走决死形态", (int)ps);
        signal_bomb10_start(s, by_hit);
    }

    /* ⑥b ★ 支援特技（十二个伙伴）—— **按机体门分派**（用户拍板的 Σ 映射，不是复刻）：
     *      Th18 灵梦(0) = 紫 + 萃香 + 文；魔理沙(1) = 爱丽丝 + 帕秋莉 + 荷取；
     *      ★ 2026-09-15i：**咲夜(2) = 大小姐 + 二小姐 + 红美铃**、**早苗(3) = 诹访子 + 神奈子 + 恋恋**。
     *      ⚠ 判据读的是 `CE_ADDR_CHARACTER`（`0x4cccf4`）—— **不是** TH11 的 `3×CHARACTER+SUBSHOT`
     *        （咲夜/早苗这两组伙伴在 TH11 里**不存在**，是用户自定的映射）。
     *      ⚠ 位置：放在灵击之后、`faith_pool_tick()` 之前 —— 萃香的全收要在本帧的池推进**之前**
     *        把道具切成 state 3（原版 `0x4245e0` 也是在 `Player::move` 里、道具 tick 之前跑）。
     *      ⚠ 咲夜/早苗 里**有每帧动作**的只有三条（二小姐蓄力 / 神奈子写收点线 / 恋恋声明无敌）；
     *        大小姐（扫弹池）与红美铃 / 诹访子（修饰信号推进）在 ③ 那一句里已经跑过
     *        （`signal_step_mod_fill()`，理由见那一段的注释）。*/
    {
        int chara = *(const int32_t *)CE_ADDR_CHARACTER;
        if (chara == SIGNAL_CHAR_REIMU)        signal_support_reimu(s);
        else if (chara == SIGNAL_CHAR_MARISA)  signal_support_marisa(c, s);
        else if (chara == SIGNAL_CHAR_SAKUYA)  signal_support_sakuya(s);
        else if (chara == SIGNAL_CHAR_SANAE)   signal_support_sanae(s);
    }

    /* ⑦ 满 P 全场转换（★★ 2026-09-15，A + B 的主路；**必须在池推进之前**）——
     *    与 110（`mof.c` 的 `on_tick_2`）调的是**同一个**共用件：满 P 时把原生 P 类道具原位删掉、
     *    在它自己的位置给**每个**认领了转换通道的槽撒一颗 type 9 档的点（角 −π/2、速 2.2）。
     *    一手 TH11 侧 = `th11:0x4240f0`（与 th10:0x41ba50 同形）：`[item+type] ∈ {1,4,10,11}` → `type 9`、
     *    `pos = 道具自己的位置`、角 `th11:0x49842c` = −π/2、速 `th11:0x498430` = 2.2。
     *    ⚠ 帧守卫在共用件里 ⇒ 110 与 111 同装时也只转一遍（本卡这一调不会重复删/重复撒）。
     *    ⚠ 顺序：`faith_pool_tick()` 之前 ⇒ 本帧转换出来的点当帧就建 VM / 受物理 / 被收。*/
    (void)faith_convert_full_p_items();

    /* ⑧ 绿点池推进（共用件：撒点 / 吸附 / 收集 → 回调 `signal_on_pickup`）*/
    (void)faith_pool_tick();

    /* ⑧ 得分跨阈值 ⇒ 残机（一手路径 ①；路径 ②「捡命碎道具」由原生承担，见 `on_item_collect`）*/
    signal_lifefrag_tick(s);

    /* ⑧-4 ★★★ 2026-09-16m：**110 型灵击**逐帧推进（消弹圈 / 每敌每帧伤害 / 范围环 / 火星）。
     *   与机体无关（哪台机都能按 X 放灵击）⇒ 放在支援特技分派**之外**。*/
    signal_bomb10_tick(s);

    /* ⑧-5 ★★★ 2026-09-16q：**河童罩子**逐帧推进（跟人 / 破罩 30 帧 / 泡没了 ⇒ 退半档）。
     *   同样与机体无关地跑（机体门只在「起泡」那一处）——换机体后残余的泡也能被收干净。*/
    signal_shield_tick(s);

    /* ⑧-6 ★★ 2026-09-16u：**文·超高速残像**的年龄推进 + 颜色斜坡（与机体无关地跑：
     *   残像的寿命是 20 帧，换机体/停特技之后那几支也要把颜色演完、自然出池）。*/
    signal_trail_tick();

    /* ⑨ 计分层：每帧重新声明（信仰会因绿点 / 小绿点 / miss 变化）*/
    signal_declare_score(s);

    /* ⑨ 诊断汇总 */
    if (s->frame - s->log_frame >= SIGNAL_LOG_EVERY) {
        s->log_frame = s->frame;
        /* ★ `ptop` = **本卡落笔的 P 点补差颗数**（`signal_on_item_commit`）。110 在场时本卡让位
         *   ⇒ 这里恒 0，那颗由 110 落笔（对照 `mof:` 那行的 `Ptop`）；111 单装时应颗颗不落。*/
        ce_log("signal: sig %d (str %d), faith %d (Δ %d), graze %d (x%d), big %d/%d small %d/%d full %d cancel %d sweep %d bomb %d (dbf %d) reset %d frag %d ext %d ptop %d",
               (int)s->slot.signal, (int)signal_core_strength(s->slot.signal),
               (int)faith_slot_total(&s->slot), (int)(s->slot.delta / FAITH_FIXED),
               (int)s->last_graze, (int)s->graze_mult,
               (int)s->stat_big, (int)s->stat_big_spawn,      /* 收到 / 撒出（大绿点）*/
               (int)s->stat_small, (int)s->stat_mid_spawn,    /* 收到 / 撒出（中绿点）*/
               (int)s->stat_pool_full, (int)s->stat_cancel,
               /* ★ 2026-09-15c：`dbf` = **决死形态**发动的次数（`bomb − dbf` = 主动次数）*/
               (int)s->stat_sweep, (int)s->stat_bomb, (int)s->stat_bomb_dbf, (int)s->stat_reset,
               (int)s->stat_frag_item, (int)s->stat_extend,   /* 命碎道具 / 得分奖残 */
               (int)s->stat_p_topup);                         /* P 点火力补差（110 在场 ⇒ 0）*/
        /* ★ 决死形态的消弹颗数与同款开关要看得见（颗数记在 `bomb_form.cancels`，诊断、不入数值）。
         *   形态是**每发**的状态 ⇒ 只在「本发就是决死形态」时才有意义（`cancels` 保留上一发的值）。*/
        if (signal_bomb_form_is_deathbomb(&s->bomb_form))
            ce_log("signal: 上一发为决死形态（已消 %d 颗，本形态入账=%d，开退款窗=%d）",
                   (int)s->bomb_form.cancels, (int)signal_bomb_form_cancel_accounts(&s->bomb_form),
                   (int)signal_bomb_form_opens_refund(&s->bomb_form));
        /* ★★ 2026-09-15e 命碎段机单开一行（实跑对账靠它）：结算过的段数 / 其中没死过的段数 /
         *   已撒的道具数 / 当前余数 / 本卡当前的段状态（开着？是不是符卡段？本段死过？）。
         *   ⚠ 用户口径「每 2 段 ⇐ 1 颗」⇒ 正常情况下 `drops ≈ segs_clean / SIGNAL_SEG_DROP_EVERY`。*/
        ce_log("signal/frag: segs %d (clean %d) drops %d spawn %d picked %d | open %d spell %d died %d "
               "pending %d/%d lastlen %d",
               (int)s->seg.segs, (int)s->seg.segs_clean,
               (int)s->seg.drops, (int)s->stat_frag_spawn, (int)s->stat_frag_item,
               (int)s->seg.open, (int)s->seg.is_spell, (int)s->seg.died,
               (int)s->seg.pending, (int)SIGNAL_SEG_DROP_EVERY, (int)s->seg.last_len);
        /* ★★ 2026-09-15f **信号强度那条链的逐段打点**（用户实跑「擦弹一点也不涨信号强度」；
         *   判读口径 —— 五段依次是「读擦弹 → 差分 → 档位 → 推进 → 共享件画的是谁」）：
         *     · `mask`：`faith_registered_mask()` —— **第 111 位（bit1 = 2）有没有置上**？
         *        它没置 ⇒ 组合 UI 走「只有 110」的中性显示（恒 1.00 / 满格），擦弹当然看不出变化。
         *     · `graze` = 原生擦弹计数（`0x4ccd0c`）累计值、`dg` = 本帧增量（0 = 本帧没擦到）。
         *     · `band`：0 = 线上 >64px / 1 = 48..64 / 2 = 0..48 / 3 = **线下** / 4 = 冻结。
         *        ★ 一手：**只要不是线下**，槽会被「+50 补满并钳到 100.00」⇒ 显示**恒 1.00**、
         *        擦弹的加值被吞掉 —— 这是 TH11 原版行为（`A = min(⌊槽⌋,100)` 封顶），不是 bug；
         *        而**线下**才是靠擦弹顶着的那一段。
         *     · `sig 前后`：本帧推进前后（千分点，1000 = 显示 1.00）。`dg>0` 时前后差里含
         *        `dg × 5000`（普通难度每擦 1 颗 +5.00）⇒ 这一栏能直接看出擦弹有没有进槽。
         *     · `neutral`：组合 UI 行 1 画的是**真值**还是**中性 1.00**（= 111 不在场）。
         *   ⚠ 若 `band=3` 且 `dg>0` 而 `sig 前后` 几乎不变 ⇒ 看 `FAITH_SIGNAL_LOSS_OFFLINE`
         *     （线下每帧的扣量）是不是把擦弹的加值吃掉了 —— 一手是 **−0.30/帧 = 300 千分点**。*/
        ce_log("signal/sig: mask=0x%02x(bit111=%d) graze=%d dg=%d band=%d sig %d->%d str=%d neutral=%d mult=%d "
               "grazeExtra %d(do_graze %d)",
               faith_registered_mask(), (faith_registered_mask() & (1 << FAITH_SLOT_111)) ? 1 : 0,
               (int)s->last_graze, (int)(dg > 0 ? dg : 0), (int)band,
               (int)sig_before, (int)sig_after, (int)signal_core_strength(s->slot.signal),
               (faith_registered_mask() & (1 << FAITH_SLOT_111)) ? 0 : 1, (int)s->graze_mult,
               (int)s->extra_graze, (int)s->extra_graze_fired);
        /* ★ 支援特技单开一行（六条各自的发动次数 + 一共花掉多少信号 + 各条当前是否在持续态）：
         *   实跑对账就靠它（`CARD-CHECKLIST.md` 的「按出来 / 看什么日志」）。
         *   ★ 2026-09-15g/h：爱丽丝那两个数分开印 —— `alice` = **减半的灵击**发动次数（本卡私有）、
         *   `palt items lo/hi` = P 点交替的**共享**相位与两侧分布（`faith_p_topup_stats()`，只读；
         *   实跑对账：`lo − hi ∈ {0,1}`、`items ≈ lo + hi`；**双卡同装时它由 110 推进**，
         *   所以这一栏在两张卡的日志里是**同一组数** —— 这正是「相位只有一个持有者」的可见证据）。*/
        faith_p_topup_stats(0, &palt_items, &palt_lo, &palt_hi);
        ce_log("signal/support: gap %d suika %d aya %d alice %d (palt %d: %d/%d) patchouli %d nitori %d | sig %d | on: suika %d aya %d hyper x%d",
               (int)s->stat_gap, (int)s->stat_suika, (int)s->stat_aya,
               (int)s->stat_alice, (int)palt_items, (int)palt_lo, (int)palt_hi,
               (int)s->stat_patchouli, (int)s->stat_nitori,
               (int)s->slot.signal,
               (int)s->suika_on, (int)s->aya_on, s->aya_on ? SIGNAL_HYPER_SPEED_PERMILLE : 1000);
        /* ★★ 2026-09-16q：**河童罩子**单开一行（实跑对账靠它）—— 泡在不在 / 活了多久 /
         *   破罩几次 / 退款账（`refunds` 涨 = 退过半档、`denials` 涨 = 破罩或本来没扣到）。*/
        signal_shield_dump("tick");
        /* ★★ 2026-09-15i：**咲夜 / 早苗 六条**单开一行（实跑对账靠它）。
         * 判读口径：
         *   · `remilia`：**累计额外计入**的擦弹次数（本帧的瞬时值在 `signal/sig:` 那行的 `dg` 里
         *     已经与原生帧差合在一起）—— 若装了 111 却没在涨 ⇒ 弹池扫描/机体门不对。
         *   · `flandre`：清屏次数；`charge`：当前蓄力帧数（0..300）。
         *   · `meiling`：本帧增速/下限门是否生效（0/1）；`floor`：下限值（千分点，恒 50000）。
         *   · `suwako`：本帧是否贴板底（1 = 线下不扣生效）。
         *   · `kanako`：写过的帧数 + 重取基准次数（`resnap > 0` = 有别人在写 `player+0x47998`）。
         *   · `koishi`：给过无敌的次数（`nat` = 原生 Aya 条件那一路、`usr` = 用户口径的放开那一路）。*/
        ce_log("signal/support2: chara=%d remilia %d (tracked %d) flandre %d (charge %d/%d aborted %d broken %d) meiling %d floor %d suwako %d kanako %d (resnap %d) koishi %d (nat %d usr %d)",
               *(const int32_t *)CE_ADDR_CHARACTER,
               (int)s->remilia.total, (int)s->remilia.n,
               (int)s->flandre.fires, (int)s->flandre.charge, SIGNAL_FLANDRE_CHARGE_FRAMES,
               (int)s->flandre.aborted, (int)s->flandre.breaks,
               (*(const int32_t *)CE_ADDR_CHARACTER == SIGNAL_CHAR_SAKUYA) ? 1 : 0,
               SIGNAL_MEILING_FLOOR, (int)s->suwako_on,
               (int)s->kanako.writes, (int)s->kanako.resnaps,
               (int)s->koishi.fires_native + (int)s->koishi.fires_user,
               (int)s->koishi.fires_native, (int)s->koishi.fires_user);

        /* ★★ 2026-09-16 新增：**子机（帕秋莉阵型）一行诊断** —— 用户实跑报「①子机缺乏贴图
         *   ②子机不发弹」，这一行一次运行就能把范围缩到一段：
         *   · `on` = 本帧是否算作子机声明层的落笔者（0 ⇒ 声明没赢/没声明，什么都别指望）；
         *   · `win` = 本帧声明层选举的赢家 id（111 = 我们）；
         *   · `ptr` = 子机槽指针（0 = 没申请到）；`vm_id` = 它的 anm VM id；`alive` = VM 还在吗；
         *     `sprite` / `scale` / `pos` = **现读那个 VM**（sprite = 0 或 scale 0 ⇒ 贴图那一半的问题）；
         *   · `set` = 本档喂给引擎的 shooterset（0x29–0x2d）；`tick` = 累计调过几次
         *     `ce_tick_shooters_for_card()`（0 ⇒ 开火那条路根本没跑）。*/
        if (ce_option_pose_winner() == (int)ce_card_id(c)) {
            uint8_t *o = s_opt[0].ptr;
            const uint32_t aid = o ? *(const uint32_t *)(o + CE_OPT_ANM_ID) : 0u;
            const uint8_t *vm = aid ? ce_anm_get_vm(aid) : 0;

            /* ★★ 2026-09-16u：**本组在飞的弹数**（用户实跑「魔理沙子机没有发弹」）。
             *   一手：`PlayerBullet__create` `th18:0x45e35c` 把 `(set<<8)|row` 写进 `bullet+0xac`，
             *   状态在 `+0x8c`（1 = 活动）⇒ 扫一遍自机弹池就能分辨：
             *     · `blt=0/0` ⇒ 引擎**一颗都没建**（问题在「喂参数 / 引擎那条路」）；
             *     · `blt=N/0`（活动弹里有、但没有本组的）⇒ 建了别的、没建我们的；
             *     · `blt=N/M`（M>0）⇒ 弹**建出来了**（那「看不见」就是贴图 / 脚本那一半的问题）。*/
            {
                uint8_t *pl = CE_PLAYER();
                int32_t bi, blive = 0, bmine = 0, blayer = -1;
                if (pl) {
                    for (bi = 0; bi < CE_PB_COUNT; ++bi) {
                        const uint8_t *b = pl + CE_PB_POOL + (size_t)bi * CE_PB_STRIDE;
                        if (*(const int32_t *)(b + CE_PB_STATE) == 0) continue;
                        ++blive;
                        if ((int32_t)((*(const uint32_t *)(b + CE_PB_PACKED_ID) >> 8) & 0xffu) != s_opt_last_set)
                            continue;
                        ++bmine;
                        if (blayer < 0) {          /* 只看第一颗本组弹的 VM 层 */
                            uint8_t *vm = (uint8_t *)ce_anm_get_vm(*(const uint32_t *)(b + CE_PB_VM_ID));
                            if (vm) blayer = *(const int32_t *)(vm + CE_ANM_VM_LAYER);
                        }
                    }
                }
                /* ★★ 2026-09-16w（子 agent 诊断的订正）：**「在飞几颗」不是可见性的判据** ——
                 *   `layer` 只影响绘制，弹对象/伤害源在 `PlayerBullet__create` 里就建好了。
                 *   真正的硬读数 = 弹 VM 的**层**（`vm+0x18`）：引擎的 `render_layer` 只在
                 *   「本趟桶号 == `vm+0x18`」时画世界列表成员，而**卡子机弹的 VM 没人设层**
                 *   （`0x407420` 两参、无层参数，实例化还显式保留 `vm+0x18`；空闲池是 memset 0）
                 *   ⇒ 脚本里没写 `layer()` 就是**层 0**（= 3D 背景那一趟，游玩区里一颗都看不见），
                 *   这**正是**用户实跑「魔理沙子机没有发弹」的根因（脚本 288..292 已补 `layer(13)`）。
                 *   ⇒ `blayer` 应当是 **13**（TH18 自机弹层）；读 0 就是层没进 anm。*/
                ce_log("signal/opt2: 自机弹池 在飞 %d 颗，其中**本组**(set 0x%02x) %d 颗，"
                       "本组弹 VM 的**层** = %d（应当 13；0 = 脚本没写 layer ⇒ 画在背景那一趟）",
                       (int)blive, (unsigned)s_opt_last_set, (int)bmine, (int)blayer);
            }
            ce_log("signal/opt: on=%d win=%d ptr=%p vm_id=%u alive=%d sprite=%d scale=%.2f pos(%.0f,%.0f) "
                   "set=0x%02x tick=%d power=%d/%d cells=%d native_hidden=%d",
                   (int)s->opt_on, ce_option_pose_winner(), (void *)o, (unsigned)aid, vm ? 1 : 0,
                   vm ? (int)*(const int32_t *)(vm + CE_ANM_VM_SPRITE_ID) : -1,
                   vm ? (double)((const float *)(vm + CE_ANM_VM_SCALE))[0] : 0.0,
                   vm ? (double)((const float *)(vm + CE_ANM_VM_POS))[0] : 0.0,
                   vm ? (double)((const float *)(vm + CE_ANM_VM_POS))[1] : 0.0,
                   (unsigned)s_opt_last_set,
                   (int)s->stat_opt_tick, signal_power_level(), (int)CE_POWER_PER_LEVEL(),
                   /* ★ 2026-09-16：本档声明了几格（= 原生火力档）+ 原生子机抑制这一层是否生效
                    *   （0 = 那一层没声明/机体不匹配 ⇒ 场上会有原生子机，先查这里）。*/
                   signal_power_level(),
                   ce_native_option_hidden(*(const int32_t *)CE_ADDR_CHARACTER));
        }
    }

    /* ⑩ ★★ 2026-09-15：向**共享的组合 UI**声明本卡这一槽的读数（`cards/faith_hud.{h,c}`）。
     *   为什么要「声明」而不是让那块 UI 读本卡状态：卡之间**不许读对方状态**（`EVENTS-CONTRACT.md` §3）,
     *   而这块 UI 两卡共用、行 2 要画 `Δ_10 + Δ_11`（用户 ④）⇒ 只能由**各自**把自己那一份交出来，
     *   由共享件相加（与计分层的「声明 → 聚合」是同一个形状）。
     *   ⚠ 声明的是**本槽**的三个数（Δ 定点 / 连击槽 / 信号槽），**不含基础值** ——
     *     「基础不进这块 UI」是用户 ④ 的原话（`faith_slot_total()` 那种「基础 + Δ」的值别往这里送）。
     *   ⚠ 它**不代表在场**：在场判据是 `faith_registered_mask()`（`faith.h`），
     *     所以本卡被删掉之后，共享件立刻改走「只有 110」的中性显示（信号 1.00 / 满格不闪），
     *     而**不是**因为「信号槽恰好是 0」——用户明令不许拿槽值当判据。*/
    {
        faith_hud_slot_t v;

        v.delta_fixed = s->slot.delta;
        v.combo       = s->slot.combo;      /* 111 侧**没有**连击槽（th11 一手），这里恒 0；
                                             * 行 3 显示什么由共享件按在场集合决定（只有 111 ⇒ 画满）*/
        v.signal      = s->slot.signal;
        faith_hud_declare(FAITH_SLOT_111, &v);
        s_slot_keep111 = s->slot;   /* ★ 2026-09-16r：账本同步进静态（跨关 / 卡对象重建都不丢）*/
    }
    return 0;
}

/* ============================ HUD（左下角：**原版 TH11 信号槽**）============================
 * 走 **`ce_hud_slot_claim(111, …)`**（`HUD-LAYOUT.md` §3）：底部货架按 card_id 升序排布 ⇒
 * 与季节 116 / 动物灵 117 / 信仰 110 同装也不会重叠。
 * ★ `ce_hud_slot_claim()` 给的是「认领那一刻」的坐标，同一开场批次里后来的认领会重排
 *   ⇒ 每帧用 `ce_hud_slot_get()` 取最新值，VM 每帧按它摆（`sdk_core.h` 的注释这么要求的）。
 *
 * 三个**原版 VM**（脚本 **212/213/214** = `CE_ANM_ABILITY_SCRIPT_SIGNAL_GAUGE_*`，`layer(25)`；
 *   贴图是 TH11 `front.anm` entry0 的切图；脚本逐条搬 TH11 `front.anm` script32/33/35）：
 *   槽底（sprite57）+ 增长条（sprite58）+ 图标（sprite26）—— 位置/宽度/闪色/压暗的出处见
 *   文件上半部分常量块的注释块。**我们只加了三行文字读数**（❓ 自定）。
 * ❓ 原生那第 4 个 VM（script34 / sprite59）**不出**：与 sprite58 同矩形，且原生每帧把它的
 *   宽度写 0（`th11:0x41b71f` `fldz`）⇒ 正常游玩不可见（报告里登记）。
 *
 * ★★ 2026-09-15c：用户口径「看不见信号 UI」的**两条**根因（都在本轮实测的日志里对上了）：
 *   ① **认领从来没成功过** —— `on_load` 里 `ce_hud_slot_claim_card(111, &blk, 0)` 传了 NULL `out`，
 *      而 `hud_register()` 对 NULL out 直接 `return 0`（`sdk_core.c`）⇒ `s_hud[111].used` 恒 0
 *      ⇒ `signal_hud_draw()` 第一句 `ce_hud_slot_get()` 就返回 ⇒ **3 支 VM 一次都没 spawn、读数一行没画**。
 *      日志判据：只有 `signal: ce_hud_slot_claim failed`、**一行 `claimed at surface` 都没有**。
 *   ② **顶点色只写了一半** —— 三支脚本只写 `color_1`，`color_2` 恒 0（一手 `th18:0x47e202` 的
 *      `colorMode` 分派 + 脚本 `alpha()` 写 `+0x527` 的订正见 `sdk.h` 的 `ce_anm_mirror_color()`）
 *      ⇒ 与 110 同一处理：每帧把两路镜像成同色（`signal_hud_dual_color()`），
 *      **基色/闪色/淡入全部照脚本**、不被覆盖。
 *   诊断：`signal: hud up`（一次）+ 每 600 帧一组 `signal: hud …`（sprite/layer/flags/c1/c2/eff/落点）*/

/* ★★ 2026-09-15：**这段代码已退役**（组合 UI 的 16 支 VM 现在全部由共享件
 * `cards/faith_hud.{h,c}` 持有）。原来这里的东西与它们的去向：
 *
 * | 原来的 | 现在在哪 |
 * | --- | --- |
 * | `s_gauge_bg/bar/icon` 三支 VM 的持有 + 每帧自愈重建 | `faith_hud.c` 的 `fh_place_gauge()` / `fh_place_icon()` |
 * | `signal_hud_dual_color()`（两路顶点色镜像）| 共享件每帧对**全部 16 支**调 `ce_anm_mirror_color()` |
 * | `s_sig_txt` / `s_faith_txt` / `s_bonus_txt` 三行 `ce_hud_vm_text_*` 读数（❓ 自定）| 合并块的行 1 / 行 2（`faith_hud_core.*` 的格式 + `faith_hud.c` 的摆位）|
 * | `signal_hud_diag_row/tick`（逐支实况诊断）| `faith_hud.c` 的 `fh_diag_row()` / 那块的一次性 16 行 + 每 600 帧汇总 |
 * | `signal_gauge_level_int()` + 档位中断 7/8/9/10 | `faith_hud_core.c` 的 `faith_hud_signal_flash()` + `faith_hud.c` 的 `fh_flash_tick()` |
 * | `signal_gauge_dim_tick()`（压暗 416/400/−64 边沿触发）| `faith_hud.c` 的 `fh_dim_tick()`（❓ 用户口径 = UI 统一：**门限每帧由本块矩形推**，不再取 TH11 那三条 —— 见那边的注释）|
 * | `s_gauge_state/age/dim`、`s_hud_up_logged`、`s_hud_diag_frame` | 共享件里同名角色的静态状态（`s_flash` / `s_age` / `s_dim` / `s_up_logged` / `s_diag_frame`）|
 *
 * ⚠ 退役方式：**整段删掉**（不是留着不调）—— 静态函数/变量留着不引用会触发 `-Wunused`，
 *   而且留着会让人以为还有第二条通往这块 UI 的路。
 * ⚠ 上面那一段长的历史注释**保留**：它记的是 TH11 原版那 4 个 VM 的一手对照表与 2026-09-15c
 *   那次「认领从来没成功过」的排查结论 —— 合并块用的**就是**他们那几个脚本（212/213/214），
 *   所以那些一手事实仍然有效，只是**执行者换了模块**。*/

/* ★★ 2026-09-15：`signal_hud_diag_row/tick`、`signal_gauge_level_int()`、`signal_gauge_dim_tick()`、
 * `signal_hud_draw()`（连同 `SIGNAL_VM_*` 那几个诊断字段偏移）**全部退役**，整段删掉。
 * 逐项去向见上面那张表；其中两条**语义**上的要点已经在共享件里落地，这里留一句免得日后找不到：
 *   · 档位闪色**只在档位变化时发**（每帧重发会把脚本 213 里 `jmp(…,20/30/36/40)` 的闪色循环
 *     反复重头开始）⇒ `faith_hud.c` 的 `fh_flash_tick()` 仍按边沿发，且带一手的**前 20 帧门**。
 *   · 压暗/淡回是**边沿**触发（同一支里 `s_dim` 位）；门限**原来**是 TH11 的 416/400/−64，
 *     ★ 2026-09-15f 起改成 ❓ 用户口径（UI 统一）：**由合并块自己的矩形每帧推**（见 `faith_hud.c`）。*/

static int on_draw(ce_card_t *c)
{
    /* ★★ 2026-09-15：整块 HUD 搬到共享件（`cards/faith_hud.{h,c}`）⇒ 本卡只剩这一行转发。
     * 三件原来由本卡自己做的事现在都在共享件里（行为一字不改）：
     *   ① **菜单 / 标题门**（`on_draw` 虚表 `+0x48` 在**主菜单 / 标题**里也会被调，而 `on_tick_2`
     *      不会 ⇒ 不设门会在菜单上留下那一整块 VM；门关着时要把 VM 收掉、别只是不画）——
     *      这是 `faith_hud_draw()` 的第一句（117 的 bug3 就是这个坑，115/116/117 同款门）；
     *   ② **一帧只画一遍** —— 110 与 111 都会调它（共享件用 `ce_sdk_frame()` 做帧守卫；
     *      16 支 VM 是**共享件的静态状态**，只有一份 ⇒ 两边不会各自 spawn 一套）；
     *   ③ 起 VM / 摆位 / 截断填充 / 档位闪色 / 压暗 / 回收 —— 全在共享件。
     * ⚠ 本函数**不吃 `c`**（在场判据 = `faith_registered_mask()`，读数来自 `faith_hud_declare()`）
     *   ⇒ 不留未使用的局部变量。HUD 的**槽位**账也归共享件（认领在 `faith_hud_on_load()`）。*/
    (void)c;
    faith_hud_draw();
    /* ★ 2026-09-16（⑧-3，方案 (a)）：**大小姐的擦弹圈红染**在这里落笔 —— `on_draw` 在所有卡的
     *   `on_tick_2` 之后跑 ⇒ 红压得住 115 那条「TH15 靠近变橙」（同一条路 id 序的问题）。
     *   ⚠ 帧守卫：110 与 111 的 `on_draw` 都会到这儿（本函数不吃 `c`），同帧只能跑一次，
     *     否则第二张卡会把第一张刚染好的弹当成「原有颜色」记进快照（还原时写回红色）。
     *   ★★ 2026-09-16m **补机体门（用户实跑「**魔理沙还是有染红**」）**：
     *     这条红染是**咲夜×大小姐**那条支援特技的视觉，**只能**在机体 == 咲夜(2) 时生效。
     *     原来这里**只**在 `signal_graze_scan()`（`signal_step_mod_fill()` 那一支）里判机体，
     *     而红染的落笔点（本处）**没有门** ⇒ 换任何机体、只要子弹进了擦弹圈就被染红
     *     —— 与「大小姐这条只在咲夜机体上生效」的整层口径矛盾（`TUNING.md` §2.1g）。
     *     ⚠ 注意：机体不匹配时**也要跑** `signal_tint_tick()` —— 它负责把**我染过的弹还原**
     *     （换机体/删卡那几条路都靠它；不跑就会留下永久的红弹）。所以是「进门 = 只染、
     *     不进门 = 只还原」，不是「整条跳过」。*/
    {
        static uint32_t s_tint_frame = 0xffffffffu;
        const uint32_t now = ce_sdk_frame();
        if (now != s_tint_frame) {
            s_tint_frame = now;
            signal_tint_tick();
        }
    }
    return 0;
}

/* ---- 生命周期 ---- */
static void signal_clear(ce_card_t *c)
{
    signal_card_t *s = st(c);

    /* ★ 顺序要紧：先退出计分层（否则删掉的卡还在往聚合里声明），再收实体 / VM / 槽位。*/
    ce_score_declare_coupled(SIGNAL_ID_NUM, 0);
    faith_unregister(SIGNAL_ID_NUM);
    /* ★ 只收**自己槽**的绿点（`1 << FAITH_SLOT_111`）。原来是 `faith_clear_entities()`
     * （清整个共用池，文件头「自查」里登记过这个偏差）⇒ 111 被删时会把 110 同行绿点一起收掉。
     * 2026-09-12 审计改成按 `slot_mask` 筛的一支 —— 这就是那条偏差的补丁。*/
    faith_clear_entities_mask(1 << FAITH_SLOT_111);
    /* ★★ 2026-09-15：HUD 搬到共享件 ⇒ 收尾从 `signal_hud_kill()`（本卡那 3 支原版 VM + 3 行读数）
     * 换成 `faith_hud_departed()`（共享件那 16 支 VM）。
     * ⚠ **顺序是硬的**：必须在 `faith_unregister(SIGNAL_ID_NUM)`（上面那行）**之后** —— 共享件的
     *   「还有没有卡在场」判据就是那张注册表（`faith_registered_mask()`）⇒ 先退注册，它才知道
     *   「111 走了」；若 110 还在场，它**只清 111 那一槽的声明**、块与 VM 一个都不动（用户 ③
     *   「只有 110 也显示这一套 UI」）。
     * ⚠ 删卡发生在**商店**里、那一刻 `on_draw` / `on_tick_2` 都不再跑 ⇒ 只有 `on_delete`（虚表 `+0x50`）
     *   跑得到；最后一张卡走时共享件在同一支里收 VM + 放占位（**块认领也归它**，所以本卡不再
     *   自己 `ce_hud_slot_release(111)` —— 那一行已删，免得释放一个不属于本卡的占位）。*/
    faith_hud_departed();
    /* ★ 2026-09-15：P 点火力那一层的「每颗道具闩」也退掉（同 `mof.c` 的 `mof_clear()`）：
     * 它是同一颗道具内的瞬时状态（collect 置 / commit 清），清它只是为了「删卡后又以同 id 回来」
     * 时绝对干净（ctor 也清一次）。*/
    s_p_item = 0;
    if (!s) return;
    /* ★ 支援特技的跨帧状态与**卡外**的那一个旋钮（`CE_PLAYER_SPEED_MULT`）先收掉：
     *   删卡发生在**商店**里（AbilityManager 冻住、Player tick 也停了）⇒ 只能靠这里，不能等下一帧。
     *   ⚠ 顺序：`signal_support_reset()` 要用 `s`，必须在 `memset` **之前**调。*/
    signal_support_reset(c, s);
    memset(s, 0, sizeof *s);
    s->last_graze = -1;
    s->last_lives = -1;
}

/* ★★ 2026-09-18：抽出「注册共享池 + 认领组合 UI 块」—— `on_load` 与 `on_tick_2` 都调它。
 *   中盘拿卡（卡 118 的十二合一）时引擎**只调 `ctor`**（trace 里没有 `on_load`）⇒ 注册与认领都没发生
 *   ⇒ 共享池没有消费者、组合 UI 不出现（用户实跑：「是切换面数重置才出现的」）。两者都幂等。*/
static void signal_ensure_registered(ce_card_t *c)
{
    signal_card_t *s = st(c);
    faith_user_t u;

    if (!s) return;
    memset(&u, 0, sizeof u);
    u.card_id = SIGNAL_ID_NUM;
    u.slot_mask = 1 << FAITH_SLOT_111;    /* 只消费 111 槽（实体层零改动）*/
    u.on_pickup = signal_on_pickup;
    u.on_cancel = signal_on_cancel;
    /* ★★ 2026-09-15：满 P 转换通道的单颗值（主路 `faith_convert_full_p_items()` 用它）——
     * 111 的中绿点 = 10（`SIGNAL_GREEN_MID`，= th11 type 9 的「显示 +10」）。不设（0）= 不参与。*/
    u.conv_value = SIGNAL_GREEN_MID;
    u.user = c;
    if (!faith_register(&u)) ce_log("signal: faith_register failed (table full?)");

    /* ★★ 2026-09-15：**块的认领搬到共享件**（`faith_hud_on_load()`）。
     *   原来这里自己 `ce_hud_slot_claim_card(111, 208×56, &got)` —— 那是「111 单独一块」时代的账；
     *   现在 110/111 共用**一块**（用户 2026-09-15 口径 ①②），由共享件按「在场的、id 较小的那张」
     *   认领（110 优先；两张都不在就不认领），块 = **188×60 表面 px**（640 空间 94×30）。
     *   ⚠ 顺序：必须在 `faith_register()` **之后** —— 共享件的在场判据就是这张注册表。
     *   ⚠ 历史（仍然有效的一手，别删）：`ce_hud_slot_claim_card()` 的第三个实参曾传 `0`（NULL out），
     *     而 `hud_register()` 对 NULL out 直接 `return 0` ⇒ 111 的 HUD **一次都没起过**
     *     （`NEXT.md` §1y.1 / `AUDIT` §… 的那次排查）。共享件那边同样传的是真 out（`&s_blk`），
     *     并且把「认领失败」打成了一行日志。*/
    faith_hud_on_load();
}

static int on_load(ce_card_t *c)
{
    signal_card_t *s = st(c);

    if (!s) return 0;
    s->last_graze = -1;
    s->last_lives = -1;

    signal_ensure_registered(c);

    signal_declare_score(s);
    /* ★ 2026-09-15（PoC 高度层迁移）：**开局那一刻**就把神奈子那条收点线的声明发出去。
     *   理由（一手）：引擎那条开局指示线是「关卡第 0 帧 / 第 30 帧」建的，**只在建的时候读一次字段**
     *   （`engine/player/th18/05-poc-height-and-indicator.md` §3），而 `on_load` 正是原生卡 22
     *   写这个字段的同一个时点 ⇒ 照它同形，线的位置才是开局就对的。
     *   门与机体一致（只有早苗机体这六条特技才生效）；`signal_support_kanako()` 幂等（声明 + `ce_poc_apply()`）。*/
    if (*(const int32_t *)CE_ADDR_CHARACTER == SIGNAL_CHAR_SANAE) signal_support_kanako(s);
    return 0;
}

static int ctor(ce_card_t *c)
{
    signal_card_t *s = st(c);

    if (!s) return 0;
    if (s_slot_stage111 > 0 && (int32_t)CE_CURRENT_STAGE() < s_slot_stage111) {
        ce_log("signal: 新一局（stage %d -> %d）⇒ 信号账本归零（原 Δ %d）",
               (int)s_slot_stage111, (int)CE_CURRENT_STAGE(), (int)(s_slot_keep111.delta / FAITH_FIXED));
        faith_slot_init_111(&s->slot, signal_difficulty());   /* 基础按难度、Δ 上限 = 原版基础（§7.2）*/
    } else {
        s->slot = s_slot_keep111;        /* ★ 一面→二面：**账本不清**（Δ / 连击 / 小绿点余数跨关保留）*/
        /* ★★ 2026-09-16s 用户口径：「**信号槽仍然每关归零（一手那条）**」
         *   ⇒ 只把**信号槽**按关卡归零（一手 `th11:0x431097`：玩家状态机那一支每帧写
         *     `[0x4a56f4] = 0`；玩家对象随场景重建 ⇒ 每关一次），Δ 与连击照旧保留。
         *   ⚠ 双保险：`on_stage_start` 里那句 `signal_core_reset()` 也做同一件事，
         *     这里再写一次是为了「ctor 排在 on_stage_start 之后」那种顺序也稳。*/
        if ((int32_t)CE_CURRENT_STAGE() != s_slot_stage111) faith_signal_reset(&s->slot);
    }
    s_slot_stage111 = (int32_t)CE_CURRENT_STAGE();
    s->last_graze = -1;
    s->last_lives = -1;
    s->log_frame = 0;
    /* ★ 2026-09-16：原生子机抑制**也从这里起步**（`ctor` 每关都跑一次，早于本关第一次 `on_tick_2`）
     *   —— 声明是粘性的，早一步声明就少一帧「原生子机被建出来」的窗口。机体门在层里，
     *   所以非魔理沙机体这里声明也不会有任何效果。*/
    ce_native_option_hide_declare((uint32_t)ce_card_id(c), 1, SIGNAL_CHAR_MARISA);
    s_p_item = 0;                                         /* ★ 2026-09-15：P 点火力那层的每颗道具闩 */
    /* ★ 2026-09-15e：命碎段机也重挂一次边沿记忆（`ctor` 每关都跑，且实跑口径是
     *   「`on_stage_start` 之后紧跟 `ctor`」⇒ 两个钩子都做一次，幂等、不依赖顺序）。
     *   ⚠ 用 `signal_seg_rebase()` 而**不是** `signal_seg_init()`：余数与诊断计数要跨关保留。*/
    signal_seg_rebase(&s->seg);
    /* ⚠ **不动 `frag_idx`**（得分奖残的索引）：一手 `0x4a5750` 只在**一局开始**时清 0
     *   （`th11:0x41faf7`），而我们的 `ctor` **每关都会跑一次**（sdk.h 的一手注释）
     *   ⇒ 在这里清会让「同一局里每关都能重拿同一批阈值」。留给 `on_run_reset`（局末）清。*/
    signal_declare_score(s);
    return 0;
}

/* ---- 关卡开场归零（§2.1「被弹（含河童 bomb） / **关卡结束** → 归 0」的第二半）
 * ★ 2026-09-12 审计修（任务 3）：spec 明写「关卡结束归 0」，而本卡原来**只有** `on_run_reset`
 *   ⇒ 跨关不归零（`on_run_reset` 是**局末**，不是每关）。两个钩子的语义差别（`SDK.md` §2 的 21 槽虚表）：
 *     · `+0x34` **`on_stage_start`** = **每关开场**（一手：`GameThread__thread_start 0x442F98` 广播，
 *       `engine/card/th18/03-hooks.md` §「+0x34」；`CARD-CHECKLIST.md` 也把它列为「每关开始（含换关）」）；
 *     · `+0x4c` **`on_run_reset`** = **局末重置**（「局中被倒吊人摧毁只走 `+0x50 on_delete`，
 *       `on_run_reset` 不会来」）⇒ 它一个局只来一次。
 *   ⇒ spec 的「关卡结束」在引擎里没有独立钩子，落地点就是**下一关开场**这一钩子；
 *     `on_run_reset` 那边的归零**保留**（局末也归零没坏处，且两条路语义上不冲突）。
 *
 * ⚠ 归零的**只有信号槽**（`slot.signal` 这一个量）。**保留**（跨关）：
 *   · `Δ_111` 绿点信仰（`slot.delta`）与 `base` / `delta_cap` —— §3.2.1「信仰跨关保留」、
 *     §7.5「Δ_111 不会消失」（信仰只有局末/换局才回初值）；
 *   · `small_rem` 小绿点余数（§3.2.1 明写「跨关保留」）；
 *   · 得分奖残的索引（`frag_idx`）—— 一手的索引只在**一局开始**清 0 ⇒ 跨关保留（见 `ctor` 的注释）。
 *   ⚠ `slot.combo`（连击槽）**111 侧根本不用**（th11 一手没有连击槽）⇒ 这里也不碰它。
 *   实现上这由 `signal_core_reset()` → 共用件 `faith_signal_reset()` 保证（它只写 `s->signal`），
 *   单测 `tests/test_signal_core.c` §5.1 逐项钉住「归零什么 / 保留什么」。
 *
 * ⚠ 这里**不动 HUD**（`on_run_reset` 会 `signal_hud_kill()`）：`ce_hud_vm_text_set()` 自带
 *   「VM 没了就重建」的自愈（`sdk.c`），三个原生 VM 有 `signal_hud_ensure()` 每帧自愈
 *   ⇒ 不需要像青眼 / 绀珠传那样在开场杀 VM（少一次无谓的 VM 重建）。
 *   ⚠ 但**压暗态**（`s_gauge_dim`）在开场要归零：原生的 flag 在 `zGui` 上、随局重置，
 *   我们放在静态区 ⇒ 留给 `signal_hud_kill()` / `signal_hud_ensure()` 那条路
 *   （开场不杀 VM 时它不会被复位，最多是「上一关压暗着、这一关第一帧立刻淡回」，无副作用）。
 * ⚠ 也**不碰**共用绿点池（残留的点由 `faith_pool_tick()` 自然出界）。*/
static void on_stage_start(ce_card_t *c)
{
    signal_card_t *s = st(c);

    if (!s) return;
    /* ★ 支援特技的跨帧状态**每关开场归零**（❓ 自定：一手的计数器挂在 zPlayer 上、随场景重建
     *   ⇒ 换关时原版也会归零；我们的卡对象是每关重建 state 但**不清零**，所以显式一次）。
     *   ⚠ 也顺手把 `CE_PLAYER_SPEED_MULT` 写回 1.0 —— 上一关的残留不能让这一关开场就加速。*/
    signal_support_reset(c, s);
    /* ★★ 2026-09-15e：命碎段机**只重挂边沿记忆**（`signal_seg_rebase()`）——
     *   换关时上一关的 boss 对象与相位槽都没了，留着旧指纹会在本关第一个相位上误报一次「段收尾」；
     *   **余数 `pending` 与诊断计数保留**（用户口径的「余数跨段保留」按段算，不按关切）。*/
    signal_seg_rebase(&s->seg);
    if (signal_core_reset(&s->slot)) {
        ++s->stat_reset;
        ce_log("signal: stage start -> gauge 0 (kept: faith %d, Δ %d, small_rem %d)",
               (int)faith_slot_total(&s->slot), (int)(s->slot.delta / FAITH_FIXED),
               (int)s->slot.small_rem);
    }
    /* 信号强度是**声明层**的一项（乘数附加通道）⇒ 归零后必须重新声明（粘性语义：
     * 信号掉到 0 时要照样声明 0，否则旧值留在层里；见 `signal_declare_score()`）。*/
    signal_declare_score(s);
}

static void on_run_reset(ce_card_t *c)
{
    /* 信号归 0（一手「一局开始归 0」`th11:0x41fb24`；**每关**那一半是 `on_stage_start`，
     * 见上面那个函数的注释）+ 收 HUD VM + 清得分奖残的索引。
     * ⚠ `on_run_reset`（虚表 `+0x4c`）的语义是**局末重置**（`SDK.md` §2 / §6：
     *   「局中被倒吊人摧毁只走 `on_delete`，`on_run_reset` 不会来」）。
     * ⚠ 这里**不用 signal_clear()**：那会把「信仰槽位注册 / HUD 槽位认领」也退掉，而 `on_load`
     *   每关都会再跑一次（幂等）；信仰本身**不清**（跨关保留）。
     * ⚠ 也**不碰**共用池：残留的点由 `faith_pool_tick()` 自然出界，比硬删温和（`on_delete` 那条路
     *   才硬收，而且现在只收 `1 << FAITH_SLOT_111` 自己的）。*/
    signal_card_t *s = st(c);

    if (!s) return;
    faith_signal_reset(&s->slot);
    s->frag_idx = 0;        /* 一手 `th11:0x4a5750` 在一局开始时清 0（`th11:0x41faf7`）*/
    signal_seg_init(&s->seg);    /* ★ 2026-09-15e：命碎段机整台归零（含**余数** —— 一手的命碎是一局一份）*/
    signal_support_reset(c, s);  /* 同上：三条支援特技的状态与卡外的移速旋钮 */
    /* ★★ 2026-09-15：HUD 的收尾改走共享件 —— `on_run_reset`（局末）与 `on_delete`（删卡）**都**会到
     *   `faith_hud_departed()`；它只在**两张卡都不在场**时才真收那 16 支 VM（只 111 走了、110 还在
     *   ⇒ 块与 VM 一个都不动，用户 ③）。⚠ 上面那行注释里说的「不用 signal_clear()」现在只针对
     *   **信仰槽的注册**（那个仍归 `on_delete`），HUD 这一支两者都可以调（幂等）。*/
    faith_hud_departed();
    signal_declare_score(s);
}

static void on_delete(ce_card_t *c) { signal_clear(c); }

/* ---- ★★ 2026-09-16q：**原生 B**（引擎自己那发炸弹花掉之后）⇒ 同一支河童罩子 ----
 * 用户口径（逐字）：「对于**原生的B**，河童特效也会引入**不撞破罩子 no miss 和一个罩子**」。
 * 一手对照（为什么位置对）：TH11 的泡是在 `Bomb::start__marisa_c` 里起的，也就是**引擎那一发
 * bomb 的起点**；TH18 侧与它同槽位的事件钩子 = `on_bomb_spent`（断点 `th18:0x4203bc`，
 * `0x4203b7 call 0x4574d0` 扣完弹、`0x4203ec call [vtable+4]` = `Bomb<Char>__begin` 之前）
 * ⇒ 在这一刻起泡，与一手「bomb 起点起泡」同相。
 * ⚠ 三条边界：
 *   ① **机体门 = 魔理沙**（`SIGNAL_CHAR_MARISA`；Σ 映射里荷取只挂在魔理沙上）；
 *   ② **只置 pending**（`senro.c` 的死亡栈 / 炸弹栈纪律：建 VM 一律在 tick 里）；
 *   ③ ★★ **不开退款窗**（2026-09-16t 用户裁决「A：什么都不退」）：一手那条退款的**前提是
 *      「这次花了火力」**（TH11 的 B 就是 `CURRENT_POWER -= BOMB_COST`，`th11:0x4311f1`），
 *      而原生 B 花的是**炸弹库存**、**一点 P 都不扣**（`th18:0x4203b7 call 0x4574d0`）
 *      ⇒ 没有可退的东西。罩子本身（挡一次中弹 = 不扣命）就是这条路的全部收益。*/
static void on_bomb_spent(ce_card_t *c)
{
    signal_card_t *s = st(c);
    int32_t ch;

    if (!s) return;
    ch = *(const int32_t *)CE_ADDR_CHARACTER;
    if (ch != SIGNAL_CHAR_MARISA) return;
    /* `with_refund = 0`：泡照起（一手无条件起泡），但不记账、不退火力（理由见上）。*/
    signal_shield_arm(s, 0, 0);
    ce_log("signal/shield: **原生 B** 起泡（弹库存 %d）—— 罩子照给（挡一次中弹 = 不扣命），"
           "**不退款**（原生 B 花的是库存、一点 P 都不扣；一手那条退款的前提是花了火力）",
           (int)CE_CURRENT_BOMBS());
}

CE_CARD(111,
        .ctor = ctor,
        .on_load = on_load,
        .on_tick_2 = on_tick_2,
        .on_draw = on_draw,
        /* ⚗ 2026-09-16zp 二分（**临时**）：111 的三个道具钩子全部停用 —— 它们是「只有崩溃配置里才会跑」
         * 的那一层（无 mod / 只有 patch 的两次正常里，卡的钩子从来没跑过）。停用后若不崩 ⇒ 元凶在这三个里；
         * 若照旧崩 ⇒ 元凶在我们「每帧都跑」的那一层（闸/家务/其它站点）。副作用：本卡不再处理道具。*/
        /* .on_item_score = on_item_score, */
        /* ★★ 2026-09-16q：**原生 B 的河童罩子**（用户点名要的那一条）。*/
        .on_bomb_spent = on_bomb_spent,
        /* ★★ 2026-09-16q：**中弹瞬间**（`Player__die` `0x45d574`）—— 本卡挂它**只为罩子**
         *   （「泡吃掉这一下」），不是 107/117 那种「中弹即自动爆」的救援语义；
         *   理由、一手对照、返回值约定全在 `on_death_before_deathbomb()` 上面那一段。*/
        .on_death_before_deathbomb = on_death_before_deathbomb,
        /* ★ 2026-09-12 审计补：绿点的两条**来源**（大 = 击破掉落 +100 / 中 = 满 P 吃 P 点 +10）。
         *   补之前本卡一条来源都没有 ⇒ `signal_on_pickup()` 永不触发。小绿点走 `on_cancel`。
         * ★ 2026-09-15e：同一个钩子还**观测命碎道具**（TH18 type 0x10 ↔ th11 type 5）——
         *   道具由本卡在段收尾撒（`signal_seg_drop()`），这里只计数、不重发。*/
        .on_enemy_drop_pre = signal_on_enemy_drop_pre,
        /* .on_item_collect = signal_on_item_collect, */
        /* .on_item_commit = signal_on_item_commit,  —— 二分临时停用 */
        /* ★ 2026-09-15：这一个钩子现在做**两件事**（一手 `th11:0x4327d0` 就是同一个函数里两件）：
         *   信号槽归零（`0x432807`）+ 信仰 −显示 100,000（`0x4327e7`，下限 = 初值）。*/
        .on_death_after_deathbomb = on_death_after_deathbomb,
        /* ★ 任务 3：`+0x34` = **每关开场**（§2.1 的「关卡结束归 0」落在关卡边界上）。*/
        .on_stage_start = on_stage_start,
        .on_run_reset = on_run_reset,
        .on_delete = on_delete,
        /* ★ 2026-09-15：帕秋莉那条的两个新钩子。
         *   · `+0x1c on_tick_shooters`：卡侧自己 tick 自己那一组 shooterset（116 是同款先例）；
         *   · `on_option_pose_apply`（ce_hooks_t 尾槽）：低速子机声明层的**落笔**回调 ——
         *     每帧恰好一次、只有被选举者（id 最小者胜；108 < 111）⇒ 子机的申请与逐格偏移只在那里写。*/
        .on_tick_shooters = on_tick_shooters,
        /* ★★ 2026-09-16zb：主炮组覆盖（TH11 的主炮伤害逐元素逐档不同 ⇒ 不覆盖就抄不了）。
         *   优先级 −1 = 阶梯最底（117 > 113 > 108 > 111），见 `sdk/public.h` 的 `CE_SHT_PRIO_111`。*/
        .on_shot_set_index = on_shot_set_index,
        .shot_set_index_priority = CE_SHT_PRIO_111,
        .on_option_pose_apply = on_option_pose_apply);
