/* 风神录（TH10，card-expand id 110）—— **装备卡**：火力经济 + 信仰 + 连击条 + 灵击。
 *
 * 一手/定案依据（**先读这两篇，本文的注释只指路、不重复推理**）：
 *   · `docs/superpowers/specs/2026-09-10-mof-faith-design.md`（110 的设计原始稿）
 *   · `docs/superpowers/specs/2026-09-11-system-card-spec-review.md` §3.2 / §3.2.1 / §4.6(R9) /
 *     §5.5 + §5.5.1（**三处事实订正 + 扣火力三个坑**）/ §7 全章（绿点 = 信仰，110/111 共用）/
 *     §7.9（大信仰点判据）/ §11 的 #47a #48 / §12 —— **订正覆盖 spec 的旧说法**
 *   · `mods/th18.v1.00a/card-expand/EVENTS-CONTRACT.md` §3（计分层：**声明 → 聚合 → id 最小者落笔**）
 *   · ★ **一手（th10 v1.00a 逐条指令）**：`local/_scratch/research-th10-faith-and-graze.md`
 *     —— 2026-09-12 的三条订正都出自它（§3.2/§3.4 无逐帧信仰衰减、§3.1 连击表的大信仰点 = +120f、
 *     §1.1 TH10 无擦弹倍率、§4.1 灵击门槛/消耗的原版值）。**它覆盖 spec 里被证伪的说法。**
 *   · ★★ **2026-09-15 本轮的一手（灵击全链）**：`engine/player/th10/01-bomb-full-chain.md`
 *     （已回流的一手文档：门槛/消耗/伤害链/半径曲线/震屏否定/boss 判据）+ 底稿
 *     `local/_scratch/research-th10-bomb-boss-residuals.md`（§1 伤害链、§2 半径曲线、§3 震屏否定、
 *     §5 boss 判据、§7 收集弹窗全表、§9 给 TH18 的映射要点）+ `research-th10-faith-and-graze.md`
 *     （§3 信仰增减、§4 灵击门槛/消耗）。
 *     **本轮据此改掉的三件事**：① 「灵击不伤敌人」是错的（经 `th10:0x4059f0` 回调贡献伤害）；
 *     ② 消弹半径是**会长到 160/112/64 的圈**（不是全屏常量）；③ 灵击**没有震屏**（`ce_screen_shake` 已删）。
 *     另加：满 P 改写表按 TH10 的（含**大 P**）、绿点收集的绿色 `+N` 弹窗、boss 判据改用「就是 boss」的字段。
 *  · ★★ **2026-09-15b 本轮（NEXT.md §1x.5）：大信仰点判据从「掉钱」换成「宿主表」** ——
 *    用户实测「大绿点与金钱完全重合」+ 裁决「**金钱和绿点总之别重合**」⇒ 走「判据收窄」：
 *    判据 = **TH18 侧「本来就是绿的那批敌人」**（表在 `mof_big_faith_table.h`，键 = 敌人的
 *    `(anm 源, 主脚本)` = 它画的是哪一张贴图），**不再看 `drop_main`**；`on_enemy_drop_pre`
 *    （旧判据②）整条删除。全套一手预研（TH10 逐关清单 / 计数 / 映射 / 对不上的清单）在那个头文件里。
 *  · ★★ **2026-09-15c 本轮：「灵击 → 决死」（决死 B 形态）** —— 用户口径（逐字）
 *    「**基本语义上灵击==B**」「**所以灵击也得决死**」⇒ 库存 == 0 且**自机在决死窗口里**
 *    （`player+0x476ac == 4`，一手 `th18:0x45d4e4`）时按 B ⇒ 本卡灵击以**决死形态**发动：
 *    照常扣 1 档火力 / 消弹 / 伤害源（节奏一字不改）+ 另调 `Player__cancel_impending_death`
 *    **`th18:0x45d590`** 救命 + **不扣 −3000 信仰**（一手：TH10 决死那一支没有 `call 0x412e70`）。
 *    全部一手依据、两个 🟡（消弹入不入账）与「为什么不做 `+0x10` 自动救人」写在 `mof_core.h`
 *    的「灵击 → 决死」块；本文件只做「检测窗口 → 传参」+ 照答案落笔。
 *
 * ## 分工（这张卡只是「一具身体」，逻辑都在共用件里）
 *
 *   | 件 | 在哪 | 管什么 |
 *   | --- | --- | --- |
 *   | 绿点 / 信仰**纯逻辑** | `faith_core.c` | 按槽加值、上限/下限夹取、连击、信号、擦弹倍率、单位换算 |
 *   | 绿点**实体池**（110/111 共用） | `faith.c` | 撒点、飞散/飘落/吸附、收集判定、消弹入账回调 |
 *   | 110 的**纯逻辑** | `mof_core.c` | 大信仰点判据、蓝点分值、卡内奖残、扣火力、PV 公式 |
 *   | **本文件** | — | 读引擎、调上面三层、`ce_score_declare_coupled` |
 *
 * ## 本卡在计分层里的角色（★ 2026-09-13 重做；见 `../SCORE-LAYER-REWORK.md`）
 *
 * **没有「落笔者」这个概念了**：每张卡只**声明自己那一份 `G_i`**，SDK 顶层一次合成
 *   `G = G_0 + Σ未耦合 + coupling × Σ耦合 / 1000`，然后写 `*value = G × 10`
 *   （引擎随后 `SCORE += value / 10` ⇒ 精确得到 G 分、天然不双计分）。
 *
 * 110 声明的是 **耦合族**（用户规格唯一带乘数的那一项）：
 *     `G_10 = 信仰_110 = 50000 + Δ_110`（`faith_score_unit()`）
 *     `coupling = (擦弹倍率 + 信号强度)`，两者都由层合成
 *       · 擦弹倍率 = `min(原生擦弹计数 × 0.0001, 8.99)`（层从 `th18:0x4ccd0c` 读，110 不必自己算）
 *       · 信号强度 = 111 声明的千分点（层求和）
 *     ⇒ `Δ_110` 与 `Δ_111` **一起**被耦合系数缩放（这正是用户公式的写法：`(Δ_10+Δ_11) × (…)`）。
 *
 * ★ 为什么不是「基础均值」那一套（旧口径）：旧公式 `(基础均值 + Δ110 + Δ111) × …` 里那个「均值」
 *   是为了「107 与 110 各自声明了完整值 ⇒ 得把两份 base 减掉」而发明的补丁；现在基础只由层算一次
 *   （`G_0`），110/111 只交 Δ ⇒ **不需要减法**，也不会因为多了第三张卡就整条不跑。
 * ⚠ 本卡**不读别的卡的状态**（只读层的耦合聚合），契约的硬要求仍然成立。
 *
 * ## 明确不做（spec §6）
 *
 * 不写 `MONEY` / 不写 max point 全局 / 不做自机 spell card 系统 / 不做续关改动 / 不做原版机体差异。
 *
 * ## 长持续效果的自查（`METHOD.md` 铁律二）
 *
 * 本卡跨帧留下的东西：① 绿点 VM 池（共用件，静态区）；② HUD 的 **9 个 VM**
 * （图标 1 + 数字位 6 + 连击条 2 —— 2026-09-12 从「两行贴图字数字」换成原生那一整块）；
 * ③ ★ 2026-09-15 新增 / ★★ 2026-09-21 改归共用件：**一发灵击的状态**（剩余帧数 / 中心 / 目标半径）
 * + 它每帧撒的 **引擎伤害源**（`player+0x20574` 池，寿命 2 帧自灭）。
 * 状态本身现在住在 `cards/bomb10.c` 的文件静态里（110/111 **共用一份**，为的是「同一套灵击系统」）；
 * **`on_delete` 与 `on_run_reset` 都走同一个 `mof_clear()`**：收池 + 收 HUD + 从计分层撤销声明
 * （`ce_score_declare_coupled(id, 0)`）+ `bomb10_reset()`（删两支环 VM + 清那一发状态 ⇒
 * 下一帧不再续撒）；删卡发生在**商店**里、那一刻 AbilityManager 冻住 ⇒ 只能靠 `on_delete`
 * （虚表 `+0x50`），不能等下一帧。
 * ⚠ 「共用一份」的代价与必然性：本卡被删掉时**111 发的那一发也一起收**（反之亦然）——
 * 这正是「同一套系统」的意思（见 `bomb10.h` 的硬语义 ①②③）。
 * 伤害源的残留与「为什么不手动清池槽」见 `mof_clear()` 里的那段注释。
 *
 * ## 明确不做 / 未做（本轮）
 *   · **消激光**：TH10 的灵击用同一个半径消激光（`th10:0x41c800`），TH18 没有半径消激光的原语
 *     （`ce_cancel_all_bullets` 是全屏消激光，语义不符）⇒ 不消激光（登记）。
 *   · **灵击的视觉**：TH10 的扩散圈是 anm 手册里的贴图（`bullet.anm` script400），TH18 无对应资产
 *     ⇒ 本卡只有效果（消弹 + 伤害 + 音效），没有画面（登记，属资产层）。
 *   · **微档实体**（一手的 type 8 微信仰点是可见的星形碎片）——照旧不建实体，只记 +10 信仰。
 */
#include <stddef.h>          /* sdk.h 的 ce_item_collect_all 用了 size_t（内置 Linux 头不带）*/
#include <string.h>
#include <stdio.h>
#include "sdk.h"
#include "faith.h"
#include "faith_hud.h"        /* ★★ 2026-09-15：110/111 共用的「组合 UI」（共享件）*/
#include "mof_core.h"
#include "mof_green_core.h"   /* ★★ 2026-09-18：变绿表（改精灵 + 改掉落，跟着卡走）*/
#include "bomb10.h"           /* ★★★ 2026-09-21：**110 型灵击的唯一一份实现**（110/111 共用）——
                               * 本卡原来那套 `mof_bomb_start/tick` 已整段搬进去（行为逐条不变），
                               * 本卡只交「自己那一半」：对话门 / 整档扣火力 / 发动代价 / 消弹记账。
                               * 为什么搬：owner 口径「只要有 110 or 111 就有灵击，**它们完全走同一套
                               * 灵击系统**」⇒ 两卡同装时不许各发一发（见 `bomb10.h` 的硬语义 ①②③）。*/

/* ---- 本卡常量 ---- */
#define MOF_ID_NUM            110      /* ⚠ `CE_CARD` 会对 ID 做 ## 拼接 ⇒ 这里必须给**字面量**，
                                         * 不能把 NUM 再套一层宏（`CE_CARD(MOF_ID_NUM, …)` 会拼出
                                         * `ce_hooks_MOF_ID_NUM`）。所以下面 CE_CARD 里写 110。*/
/* engine.h 给了 `CE_CURRENT_POWER()` 但**没有** max power 的读取宏 ⇒ 就地补一个（只读）。
 * `CE_ADDR_MAX_POWER` = `0x4ccd3c` = `GlobalsInner+0x60`（engine.h 一手）。*/
#define CE_MAX_POWER_VAL()     (*(int32_t *)CE_ADDR_MAX_POWER)
#define MOF_GRAZE_ADDR        0x4ccd0c   /* 原生擦弹计数（GLOBALS.inner+0x30；与 lolk/season 同址）*/
#define MOF_ADDR_DIFFICULTY   0x4ccd00   /* 难度（0 EASY / 1 NORMAL / 2 HARD / 3 LUNATIC / **4 EXTRA**；
                                          * 一手：`collect_money_item` 拿它当 switch 选择子，`cmp eax,4` +
                                          * 5 项跳转表 0x446d40；th06rank.c 同址，**只读**）*/
#define MOF_LOG_EVERY         600        /* 每 600 帧一条汇总（实跑对账）*/

/* ---- ★★ 逐帧信仰衰减的门③「关卡帧 ≥ 90」（🟡 **自定源**，不是复刻）----
 * **一手要什么**：`th10:0x418759 cmp [edi+0x14],0x5a`，`edi` = `g_GameThread` ⇒ 一手读的是
 *   **GameThread 里的一个关卡帧计数**（同字段上还比过 0/30/90/2940/3000，`edi` 的来源链是
 *   `GAME_THREAD_PTR 0x477810` → `0x4179fb` 注册每帧任务 → `0x4187c0` → `0x418190`）。
 *   ⚠ 那个字段的**起点/清零点**在 th10 侧也没查到（`research-th10-faith-units-and-drain.md` §9 的 ❓）。
 *
 * **TH18 侧为什么不用引擎的**：本仓**没有** `zGameThread` 的字段图
 *   （`engine/chapter/th18/01-chapter-residuals.md` §1 明写「❓ 未证，别拿 TH17 的布局当结论」）。
 *   引擎里唯一现成的「关卡帧」是 `CE_ADDR_TIME_IN_STAGE`（`0x4ccce8`），但本轮的写点穷举证明它
 *   **不是「每关从 0 起」**：它的 inc 在 `th18:0x443d16`（`GameThread__on_tick` 尾），三个清零点
 *   分别在 `th18:0x4425ca`（一局初始化）、`th18:0x4441e5`（`GameThread` 启动）、`th18:0x44442f`
 *   （**关卡拆机/换关**，同函数里紧跟 `Stage` 与三个管理器的析构）—— 换关确实归零，但同一函数里
 *   `CURRENT_STAGE` 与另两个计数也一起清 ⇒ 它语义上是**「本局/本关的计时」**，与一手那个字段
 *   是否同物**没有证据**（🟡）。
 *   ⚠ **且本仓已有明令**：`CARD-NUMERIC-INTERACTION-AUDIT.md` 记过「114 刻意弃用 `TIME_IN_STAGE`、
 *     卡一律自数帧」⇒ 跟既有口径走，别为一个门去改全仓的帧源。
 * ⇒ **本卡的取法**：`s->stage_frames` = 自己的按帧计数（`on_tick_2` 每帧 +1），
 *   按「关卡计时回退 / 关卡号变化」重置（`mof_stage_frames()`，用 `CE_TIME_IN_STAGE()` 与
 *   `CE_CURRENT_STAGE()` 两个**只读**信号做重置判据 —— 计时回退只可能在换关/重开那一帧发生，
 *   比只看关卡号更可靠：练习模式重开同一关时关卡号不变）。
 *
 * 🟡 **怎么钉**：实跑看 `mof:` 每 600 帧那行的 `gate=` / `sf=` 两栏 ——
 *   ① `sf` 应当在**每次换关后从 0 重新爬**（不是一局一路涨）；
 *   ② 进关后 ~90 帧（1.5 s）那段，`gate=101`/`gate=111`（对应位）里「关卡帧」那一位应当是 **0**，
 *      之后变 1，且这段时间信仰**不动**；③ `gate=` 三位依次 = `无Boss / 无对话 / 关卡帧≥90`。*/
#define MOF_STAGE_FRAME_GATE   90        /* 一手 `th10:0x418759` 的 `0x5a`；判据 = 「≥ 90 算过」 */

/* ---- ★★ 2026-09-15：「组合 UI」把本卡的 HUD 整块搬进了**共享件**（`cards/faith_hud.{h,c}`）----
 *
 * 本节原来那一整批 `MOF_HUD_*` 几何常量（图标/数字位/连击条的位置与尺寸、`U640_TO_SURF`、
 * TH10 的压暗门限 416/400 与 −128/−112、连击条两个四边形的坐标、满槽宽 38 px、`MOF_COMBO_MAX`）
 * **全部搬到 `cards/faith_hud.c` 的常量块**，本文件一个都不留 —— 免得两处各一套几何、改一处漏一处。
 * 它们的**一手出处与逐条判定（复刻 / ❓ 自定）**也一并搬过去（那边逐条给了 `th10:0x…` / `th11:0x…`
 * / 脚本号）；块的尺寸与三行的堆叠见 `engine/player/th11/04-merged-hud-spec.md` 与 `NEXT.md` §2A。
 * ⚠ 本卡侧**不再持有任何 HUD VM**（`s_hud_*` 也一起删了）：起 VM / 摆位 / 上色 / 压暗 / 回收全在共享件；
 *   `on_delete` 那条路径由 `faith_hud_departed()` 收（见 `mof_clear()` 的注释）。
 * ⚠ **仍然有效的一手事实**（合并块用的还是 TH10 那套数字位与连击条几何，只是执行者换了模块）：
 *   TH10 的信仰读数在 `Gui::on_tick` th10:0x414a60 起被逐位画进 `GUI+0x8094` 的 7 个 VM
 *   （`Gui::initialize` th10:0x413d5e–0x413d7e 跑 `front.anm` script51..57）；`anchor(1,1)` = 左上
 *   （一手证法：`front.anm` script2..5 的边框条四条 `pos` 只有「pos = 矩形左上角」才正好铺满四边）；
 *   连击槽 = `GLOBALS+0x14` 的 zTimer 被 `th10:0x4158b6`–0x415937` 直画的两条实心矩形
 *   （宽 = `current × 38/130`、A 借最左数字 VM 的 `color_1.A`）。*/

/* ---- ★★ 2026-09-15：灵击（按 X）整套按 TH10 一手重写（依据见 `mof_core.h` 的「灵击的收尾」块）----
 *
 * 与旧实现的三个差别（旧值全部作废）：
 *   ① 消弹**不是全屏一次性**：半径 = `16.0 → 160/112/64`（120 帧线性），**逐帧推进**、中心每帧上移；
 *   ② **有伤害**（TH10 的灵击经 `0x4059f0` 回调贡献伤害）⇒ 用 `ce_damage_rect` 近似（❓ 自定）；
 *   ③ **没有震屏**（一手否定）⇒ `ce_screen_shake` 整段删掉。
 *
 * 引擎字段（一手）：
 *   · `CE_ADDR_CHARACTER` `th18:0x4cccf4`（0 灵梦 / 1 魔理沙 / 2 咲夜 / 3 早苗）；
 *   · `CE_ENEMY_FLAGS` `th18:0x635c` 的 bit23 = 「这敌是 boss」（ECL opcode 512 `setBoss`）；
 *   · `CE_EM_BOSS_IDS` `th18:0x48` = boss 表（存 enemy_id，写入点 `th18:0x42d490`）；
 *   · `CE_SPELL_BONUS` `th18:0x7c` / `CE_SPELL_FLAG_BONUS` `th18:0x78` bit1（= TH10 的 `+0x3790` / `+0x378c` bit1）；
 *   · `CE_ADDR_RANK` —— engine.h 里**没有**这个宏（只有 106 卡自己写死过）⇒ 本卡就地定义。*/
#define MOF_ADDR_CHARACTER    0x4cccf4   /* int32 角色号 0..3（engine.h 一手：`Player__initialize` `0x45a7a3`）*/
/* ★★ 2026-09-21：灵击的那批常量（RANK / 音效 id / 伤害源寿命 / 游玩区 / 环淡出与火星节流）
 * 与**救命调用** `th18:0x45d590` 全部搬进共用件 `cards/bomb10.h`（两张卡原来各抄一份，现在只有一份）。
 * 一手出处逐条跟着搬过去（文件头那张表），本文件不再重复。*/

/* 弹字环的数字槽（`engine.h` 的 `CE_POPUP_*` 一手；**照原生 adder `th18:0x4645f0` 的字段写序**）：
 *   mgr+0x10 = 槽号计数器（原生 adder 读它、≥0xa 归零、写完 inc）、18 槽 @ mgr+0x624、stride 0x48、
 *   k0..12 = 数字槽（adder 只用前 10 个 ⇒ 计数器在 10 处回卷）、k13..17 = 倍率槽（114 的 `ce_popup_mult`）。
 *   slot：+0x00 起 = **裸 digit 字节 0..9 逆序**（负值写 0x0a）、+0x3d = 位数、+0x1c = 颜色、
 *   +0x18 = scale 初值 1.0、+0x24 = age 0、+0x20 = −1、+0x28 = 0、+0x30 bit0 = 动画类、+0x3c = busy。
 * 生命周期全原生：tick `th18:0x464020`（每帧 age+1、`> 0x3c` 清 busy ⇒ **寿命 60 帧**）、draw `th18:0x464180`。*/
#define MOF_POPUP_COUNTER     0x10
#define MOF_POPUP_SLOTS       10         /* 原生 adder 的回卷点（`th18:0x4645ff cmp eax,0xa`）*/
#define MOF_POPUP_DIGITS      8          /* 数字串可用字节数（+0x00..+0x0b；+0x0c 起是 pos）*/
#define MOF_POPUP_GLYPH_MINUS 0x0a       /* 负数 → `mov BYTE PTR [esi],0xa`（`th18:0x46466d`）*/
#define MOF_POPUP_GREEN       0xff00ff00u /* ★ 复刻：大信仰点/小信仰点的弹窗色 = 绿（一手 TH10
                                           * `th10:0x41b5ae push 0xff00ff00`（type 3 大信仰点）/
                                           * `th10:0x41b602 push 0xff00ff00`（type 9 小信仰点），
                                           * 两句后面各跟 `call 0x42b9c0`（PopupManager::add）；
                                           * 与 TH11 `th11:0x423cae` 逐字节同色）*/
#define MOF_POPUP_FIELD_SCALE 0x18
#define MOF_POPUP_FLAG        0x30
#define MOF_POPUP_ANIM        0x2c

/* ★★★ 2026-09-21：这里原来有 `mof_bomb_t`（一发灵击的全部跨帧状态：剩余帧数 / 中心 / 目标半径 /
 * 环与竖条环 VM / 火星节流 / 形态），**整段搬进共用件 `cards/bomb10.{h,c}`** —— 那一份是 110 与 111
 * **共用的一份状态**（这是「同一套灵击系统」在代码上的意思，见 `bomb10.h` 的硬语义 ①②③）。
 * 本卡只留**形态账本**（`mof_bomb_form_t form`，在下面那个结构体里）：它是本卡自己的代价口径
 * （决死扣 0 信仰那条 0/1 开关），与「这一发走到第几帧」无关。*/

typedef struct {
    faith_slot_t slot;              /* Δ_110 / 连击槽（130f）/ 基础 50000（共用件）*/
    mof_bonus_t  bonus;             /* 卡内奖残计数（§2.6）*/
    int32_t      last_graze;        /* 上一帧原生擦弹计数；-1 = 未初始化 */
    int32_t      graze_mult;        /* 擦弹倍率（千分点定点）*/
    int32_t      last_lives;        /* 上一帧残机（miss 检测：命数减少）*/
    mof_spell_watch_t spell_watch;  /* 符卡监视器（状态机在 mof_core.c 的 mof_core_spell_watch()）*/
    /* ⚠ `spell_frames`（本次符卡已过帧数）与 `bomb`（一发灵击的全部状态）**都不在这里了**
     *   （2026-09-21）：两者都搬进共用件 `cards/bomb10.*`（`bomb10_spell_frames()` 查询）。
     *   理由 = 「同一套灵击系统」：符卡帧数是那一发的自变量（`big_bomb` 判据），必须与状态同住；
     *   而状态必须是**唯一的一份**（两张卡同装不许各发一发）。*/
    mof_bomb_form_t form;           /* ★ 2026-09-15c 本发的**形态**（按 B / **决死 B**）+ 累计消弹颗数。
                                     *   语义与两条代价口径全在 `mof_core.h` 的「灵击 → 决死」块
                                     *   （状态在 core ⇒ 主机单测能钉住；卡侧只「检测窗口 → 传参」）。
                                     *   ⚠ 逐帧状态搬走之后它是本卡**唯一**残留的灵击字段：纯诊断 + 代价口径。*/
    int32_t      spell_avoided;     /* 诊断：全避次数 */
    int32_t      frame;
    /* ★ 2026-09-12 审计（本次，任务 1 的收尾）：诊断口径拆成「**收到** / **撒出**」两分法 ——
     *   与 `cards/signal.c:125-128`（111）同款。起因：原来 `stat_big` / `stat_small` **一个字段担两件事**
     *   （撒点处 ++ 一次、`mof_on_pickup` 里又 ++ 一次）⇒ 一颗点记两次，实跑对账时数字永远是两倍。
     *   拆开之后「收到」只由 `mof_on_pickup` 记、「撒出」只由 `faith_spawn` 成功那一支记。*/
    int32_t      stat_big;          /* 诊断：**收到**的大信仰点颗数（`mof_on_pickup`，tier == BIG）*/
    int32_t      stat_small;        /* 诊断：**收到**的小信仰点颗数（`mof_on_pickup`，tier != BIG）*/
    int32_t      stat_big_spawn;    /* 诊断：**撒出**的大信仰点颗数（判据① death + 判据② drop_pre）*/
    int32_t      stat_small_spawn;  /* 诊断：**撒出**的小信仰点颗数（满 P 吃 P 点，**只此一路**）*/
    int32_t      stat_spawn_failed; /* 诊断：共用池满 ⇒ `faith_spawn()` 返回 0 的次数（大小点合计）*/
    int32_t      stat_cancel;       /* 诊断：消弹总数 */
    int32_t      stat_p_topup;      /* 诊断：P 点火力补差成功的颗数（见 `on_item_commit`；每 MOF_LOG_EVERY 帧的诊断行走 `Ptop`）*/
    int32_t      stat_miss;         /* 诊断：miss 次数 */
    int32_t      stat_bomb;         /* 诊断：灵击次数 */
    int32_t      stat_bomb_dbf;     /* ★ 2026-09-15c 诊断：**决死形态**发动的次数（其中一支口径，
                                     * 与 `stat_bomb` 并列看：`dbf ≤ bomb`，且 `bomb − dbf` = 主动次数）*/
    /* ★★ 2026-09-15：逐帧信仰衰减（装回）的诊断与状态。字段全在本卡私有块里 ⇒ 删卡随 `mof_clear()`
     * 的 `memset` 一起归零，不留跨帧痕迹（`METHOD.md` 铁律二）。*/
    int32_t      stage_frames;      /* ③ 门用的**关卡帧计数**（🟡 自定源，见 `MOF_STAGE_FRAME_GATE`）*/
    int32_t      stage_frames_ref;  /* 上一帧的 `CE_TIME_IN_STAGE()`；**变小 = 换关/重开** ⇒ 重置上面那个 */
    int32_t      stage_id_ref;      /* 上一帧的 `CE_CURRENT_STAGE()`；**变化 = 换关** ⇒ 重置。初值 -1 */
    int32_t      drain_lost;        /* 诊断：本段（自上次日志起）**逐帧衰减**累计扣掉的定点量 */
    int32_t      gate_no_boss;      /* 诊断：本帧三条门 */
    int32_t      gate_no_dlg;
    int32_t      gate_frames;
    int32_t      gate_enemies;      /* 诊断：本帧走的是哪一档（1 = 有敌人 ⇒ 慢档 600/s）*/
    int32_t      combo_before;      /* ★ 2026-09-15f 诊断：本帧 `mof_core_faith_tick()` 之前的连击槽 */
    int32_t      combo_after;       /*   之后的连击槽（两手之差 = 1 ⇒ 一手「每帧 −1」；0 ⇒ 门冻结）*/
    int32_t      stat_combo_item;   /* ★ 2026-09-16 诊断：**引擎道具**给连击槽充的帧数累计（P/钱/大P）*/
    int32_t      stat_combo_kill;   /*   诊断：**击破非 boss** 给连击槽充的帧数累计（= 只数 × 10）*/
    int32_t      log_frame;
} mof_card_t;

/* ★ 2026-09-16r：跨关账本的**权威副本**（见 `ctor` 的说明）。`ctor` 恢复它、每帧同步它。*/
static faith_slot_t s_slot_keep;
static int32_t      s_slot_stage = -1;

_Static_assert(sizeof(mof_card_t) <= 496, "ce_state 上限 496 字节（超了会静默返回 NULL）");

/* ★★ 2026-09-15：本卡**不再持有任何 HUD VM**（原来那 9 支：图标 1 + 数字位 6 + 连击条 2）。
 * 整块 HUD 搬进共享件 `cards/faith_hud.{h,c}`（16 支 VM，含本卡原来那 9 支的全部角色）——
 * 因为用户 2026-09-15 口径 ①② 把 110 与 111 的两块 UI 并成了**一块**，共用件才是它唯一的持有者。
 * 于是这里的 `s_hud_icon` / `s_hud_digit[]` / `s_hud_digit_spr[]` / `s_hud_bar[]` / `s_hud_dim` /
 * `s_hud_warned` / `s_hud_digit_tbl[]` 与整套 `mof_hud_*` 一起删掉（静态符号留着不用会 `-Wunused`）。*/

/* ★★ 2026-09-15：P 点火力那一层的**每颗道具闩**（`on_item_collect` 置、`on_item_commit` 清）。
 * `on_item_commit` 不带 payload ⇒ 只能靠断点对的背靠背关系记住「这一颗是不是小 P」
 * （同 `collect2x.c` 的 `s_stash` / `s_stash_valid` 形状）。放 DLL 静态区、不进 `ce_state`。*/
static int      s_p_item;

/* ★★ 2026-09-18：**击破闩锁** —— 一手的信仰衰减速率按「这一帧有没有击破」选档
 * （一手 `th10:0x40e26b` 在敌机死亡路径里把速率字段写 1 ⇒ 那一帧只扣 −10 显示）。
 * 置位在 `on_enemy_death`、读走并在 `on_tick_2` 里清（`gate.killed_since_last`）。
 * ⚠ 2026-09-22：它**留在本文件**（不属于搬进共用件的绿妖精那一段 —— 那是 110 自己的衰减速率判据）。*/
static int32_t s_mof_killed;

/* ============================ HUD 诊断 / HUD 本体（★ 2026-09-15 全部搬走）============================
 *
 * 这一段原来是两样东西，现在**都在共享件** `cards/faith_hud.{h,c}` 里：
 *   · `mof_hud_diag_build/row/digit/tick`（那套「起来了但看不见」逐支实况诊断，
 *     `MOF_VM_LAYER/FLAGS_LO/MIXED/POS_SCRIPT/RECT_*` 那批字段偏移也一起搬）；
 *   · `mof_hud_spawn/warn_once/alive/dual_color/kill/digit_of/dim_tick/draw`（9 支 VM 的整套驱动）。
 * 现在的对应物：`faith_hud.c` 的 `fh_diag_row()`（**16 支**逐支一行，起全了打一次）+ 每 600 帧一行汇总、
 * `fh_spawn()` / `fh_place_gauge/icon/digits/star/mult/combo()` / `fh_dim_tick()` / `fh_kill()`。
 *
 * ⚠ 为什么整段删而不是留着不调：静态函数/变量留着不引用会触发 `-Wunused`（`-Werror` ⇒ 直接红），
 *   而且留着会让人以为还有第二条通往这块 UI 的路。
 * ⚠ 那段诊断的**方法论仍然有效**、也已搬过去（`flags_lo` 的绘制门槛 / colorMode / 渲染模式三组位、
 *   `mixed_inherited_color` 才是真正画出去的顶点色、`entity_pos + 脚本 pos` 相加）——
 *   它是 2026-09-15 那次「110 的 HUD 看不见」排查留下的最值钱的东西，别丢。*/

static mof_card_t *st(ce_card_t *c) { return ce_state(c, mof_card_t); }


static int mof_difficulty(void) { return *(volatile int32_t *)MOF_ADDR_DIFFICULTY; }
static int32_t mof_graze(void)  { return *(volatile int32_t *)MOF_GRAZE_ADDR; }

/* 这只敌人**是不是 boss** —— ★ 2026-09-15 换成「就是 boss」的那个字段（一手）。
 *
 * 一手（TH18）：`zEnemy.flags_lo`（`CE_ENEMY_FLAGS` = `th18:0x635c`）的 **bit23 `0x800000`**，
 * 由 **ECL opcode 512 `setBoss`** 置/清：
 *   `th18:0x433bf5: or ecx,0x800000` + `th18:0x433bfb: mov [edi+0x5130],ecx`（注册；`0x5130 + 0x122c
 *   (zEnemyData 在 zEnemy 里的偏移) = 0x635c` ✅ 同格）
 *   `th18:0x433be2: and ecx,0xff7fffff`（反注册）
 *   引擎自己也读它：`th18:0x42eed0: test DWORD PTR [ecx+0x635c],0x800000`（boss 血条那一段）。
 * ⇒ 这与 **TH10 的判据逐条同形**：TH10 是 `enemy+0x1444` 的 bit `0x8000`，同样由 `setBoss` 置/清
 *   （`th10:0x40ffec or ecx,0x8000` / `th10:0x40ffbf and ...,0xffff7fff`）⇒ **复刻**。
 *
 * ⚠ 旧实现借的是 **boss id 表**（`CE_EM_BOSS_IDS`）—— 那张表**是 TH10 `bosses[]` 槽表在 TH18 的对应物**
 *   （见 `mof_boss_alive()`），但「这敌是不是 boss」在 TH10 用的是**它自己的位**（`th10:0x40e184`），
 *   两者在「同一个槽被后来的 boss 顶掉」时会分叉 ⇒ 各就各位：**这一问用位、有 boss 那一问用表**。
 * ⚠ 不用 `Spellcard` 状态代替（一手明令）：boss 的非符阶段照样是 boss（`th10:0x405a53` 的
 *   「无符卡 + 有 boss」分支就是证据）。*/
static int mof_is_boss(const uint8_t *e)
{
    if (!e) return 0;
    return (*(const uint32_t *)(e + CE_ENEMY_FLAGS) & CE_ENEMY_FLAG_BOSS) != 0;
}

/* 「场上有没有 boss 活着」—— ★ TH10 的判据是 `[ENEMY_MANAGER + 0x10] != 0`（`bosses[0]`，
 * `th10:0x405a58` / `th10:0x405a95` 两处消费点），TH18 的同位表 = `EnemyManager + 0x48 + 4*slot`
 * （**存的是 enemy_id**，一手写入点 `th18:0x42d490`：`[edx+eax*4+0x48] = [ecx+0x6830]`，由 ECL `setBoss`
 * 在 `th18:0x433c08` 调）⇒ 「表首格非 0」与 TH10 的 `bosses[0] != 0` 逐条对位（TH18 是 id、TH10 是指针，
 * 零/非零的语义相同）。灵击的「圈外只有有 boss 时才给伤害」就用这一条（复刻）。*/
static int mof_boss_alive(void)
{
    uint8_t *em = CE_ENEMY_MGR();
    if (!em) return 0;
    return *(const int32_t *)(em + CE_EM_BOSS_IDS) != 0;
}

/* 「屏幕上还有没有敌人」—— ★ **只用它选衰减速率档**（用户口径 2026-09-15）。
 *
 * 一手（TH18）：`[[0x4cf2d0]+0x198]` = `zEnemyManager.enemy_count_real`（ExpHP 名）。
 * ⚠ **两个 `+0x198` 长得一样、意义无关**（一手踩坑记录，见 `engine/enemy/th18/` 的那条 ⚠）：
 *   `GUI_PTR → +0x1b0 → +0x198` 是**对话期的敌人出场计数**，不是这个；真读 count 必须走
 *   `ENEMY_MANAGER_PTR`（`0x4cf2d0`）—— 下面这行就是那条。
 *
 * ⚠ **这不是「一手 th10 的门」**：th10 的门①是 `[ENEMY_MANAGER+0x10] == 0`，我在本轮逐条复核了它 ——
 *   它是**当前 boss 的指针**（非 0 时被当对象解引用：`th10:0x414adc` 读它的 `+0x23fc` 符卡计时画奖励条、
 *   `th10:0x41217b`/`th10:0x416142`/`th10:0x41e760` 读它的 `+0x1068` 坐标 / `+0x2480` 标志），
 *   **不是敌人计数**。这里之所以要「敌人计数」，是因为用户口径把速率分成两档、判据是「屏幕上有没有敌人」；
 *   它**与那三条门是两件事**（门决定「冻不冻」，这一条只决定「冻不着时用哪一档」）。🟡 见 `TUNING.md` §2.5h。*/
static int mof_enemy_present(void)
{
    uint8_t *em = CE_ENEMY_MGR();
    if (!em) return 0;
    return *(const int32_t *)(em + CE_EM_ENEMY_COUNT) > 0;
}

/* 「本关已经过了几帧」—— ★ 🟡 **自定源**（一手那个字段在 TH18 没有对位，理由全文在
 *   `MOF_STAGE_FRAME_GATE` 的块注里）。**两个重置判据都只用引擎的只读全局**：
 *     · `CE_TIME_IN_STAGE()`（`0x4ccce8`）**回退** ⇒ 换关 / 练习重开（`th18:0x44442f` 那个清零点）；
 *     · `CE_CURRENT_STAGE()`（`0x4cccdc`）**变化** ⇒ 换关（练习重开同一关时它不变 ⇒ 需要上面那条）。
 *   ⚠ 不在 `on_stage_start` 里重置：那个回调是引擎在换关时对卡组每张卡调的（`sdk.h` 有先例），
 *     但它**不覆盖**「练习模式重开同一关」与「中途 retry」两种路径，而这里两条都覆盖 ⇒ 单一出处。
 *   ⚠ 每帧只推一次（`on_tick_2` 里调一次），所以 `stage_frames` 的语义 = 「本卡看到的游玩帧」；
 *     返回的是**自增之后**的值 ⇒ 「本帧的帧号」，门③直接用它与 90 比（第 1 帧 = 1 ⇒ 90 帧后才过）。*/
static int32_t mof_stage_frames(mof_card_t *s)
{
    int32_t ts, st;

    if (!s) return 0;
    ts = CE_TIME_IN_STAGE();
    st = CE_CURRENT_STAGE();
    if (s->stage_frames_ref < 0 || ts < s->stage_frames_ref || st != s->stage_id_ref) {
        s->stage_frames = 0;                 /* 换关 / 重开 ⇒ 从 0 起（本帧 = 1）*/
        s->stage_id_ref = st;
    }
    s->stage_frames_ref = ts;
    return ++s->stage_frames;
}

/* ---- 计分层：声明自己的那一份 **Δ_10**（粘性；删卡时清 0 退出聚合）----
 * 属**耦合族** ⇒ 走 `ce_score_declare_coupled()`（会被 `(擦弹倍率 + 信号强度)` 缩放）。
 * ⚠ **不含基础**：基础项 `G_0` 由层按用户公式算一次（TH18 原生身价 `(1+资金力)×难度修正`，
 *   下限 10000 / 上限按难度）。旧实现声明「基础 50000 + Δ」是为了配合落笔者的
 *   「减两份 base」补丁；重做后那套补丁没了 ⇒ 这里必须减掉，否则基础被双计
 *   （`SCORE-LAYER-REWORK.md` §5 第 1 条坑）。单卡时两者等值（有单测）。*/
static void mof_declare_score(mof_card_t *s)
{
    int32_t delta;

    if (!s) return;
    delta = faith_score_unit(&s->slot) - FAITH_BASE_110;      /* = Δ_110（基础由层出）*/
    if (delta < 0) delta = 0;
    ce_score_declare_coupled(MOF_ID_NUM, delta);
    /* ★ 2026-09-18（换版）：本卡是 **th10** 那一族 —— 信仰**整个坐在高度公式里面**
     *   （一手 `th10:0x41b4d5`–`th10:0x41b52b`：`F/2 − d(F−100000)/6080`）⇒ `coeff_110` 是它自己那条曲线。
     *   曲线要 `F` = **显示信仰**（层据此按 `500 − d×(F−100000)×1000/(6080·F)` 算千分点，
     *   `d` 取**道具 y** —— 一手：th10 判的是 `item+0x3c4` 与 144 线；见 `engine/faith/th10/01-faith-and-points.md`）。*/
    ce_score_family_declare(MOF_ID_NUM, CE_COEFF_FAMILY_10);
    ce_score_family10_faith(faith_score_unit(&s->slot));      /* ← 就是上面那个 `F`（显示单位）*/
}

/* ---- 绿色 `+N` 弹窗（★ 用户要求复刻「吃绿点有绿色 +N」）----------------------------------
 *
 * **一手（TH10 侧）**：`th10:0x42b9c0` = `PopupManager::add(value, &pos, color)`（`esi` = 全局
 * `0x477840`、`eax` = 数值、`edi` = &zFloat3 位置、栈 1 参 = 颜色、`ret 4`），720 槽 × stride 0x40；
 * **寿命 60 帧**（`th10:0x42b75e cmp [esi-0x4],0x3c`）、**上飘 0.5 px/帧**（`th10:0x42b713 fsubr`）、
 * 第 56 帧换字形组；**位置 = 道具自身坐标**（`th10:0x41b360 lea edi,[ebp-0x4]`，全 type 共用）。
 * 绿点那两条调用点：**大信仰点** `th10:0x41b5ae`（绿 `0xff00ff00`，值 = 难度表 5000/8000/10000）、
 * **小信仰点** `th10:0x41b602`（绿 `0xff00ff00`，值 = 100）；**微信仰点（type 8）没有弹窗**
 * （`th10:0x41b5d6` 的 handler 全程没有 `0x42b9c0` 调用）—— 我们的微档本来就不建实体，正好一致。
 *
 * **TH18 侧的载体 = 引擎自己的弹字环**（不是我们自造）：写序**逐条照抄原生 adder `th18:0x4645f0`**
 *   `mov edi,ds:0x4cf420`（POPUP_MGR）→ 读 `[edi+0x10]` 槽号、`>= 0xa` 归零 → 槽 = `edi+0x624+0x48*idx`
 *   → busy(`+0x3c`)=1 → 逐位取十进制写 `slot+0`（**裸 digit 字节、逆序**；负值写 `0x0a`；0 写 digit 0）
 *   → 位数(`+0x3d`)、颜色(`+0x1c`)、动画位(`+0x30` bit0 + `+0x2c`)、age(`+0x24`)=0、`+0x20`=−1、`+0x28`=0
 *   → pos(`+0xc`)=道具坐标、scale(`+0x18`)=1.0 → `inc [edi+0x10]`。
 *   生命周期（60 帧回收、上飘、按离自机距离取 alpha 80/208）**全原生**，我们只写一次。
 * ⚠ 与 `cards/collect2x.c` 的 `ce_popup_mult()` 是**同一族**的第二个用户：那个写**倍率槽**（k13–17），
 *   本函数写**数字槽**（k0–9，计数器与原生 adder 共用 ⇒ 与原生弹字一起轮转，不会互相覆盖）。
 * ⚠ 形状照抄、**没有抽进共享件**（`sdk.h` / `faith.h` 是共享件，本轮不动；111 若也要数字弹窗，
 *   该由 owner 决定是否提炼成 `ce_popup_number()`，见报告）。*/
static int mof_popup_number(const float *pos, int32_t value, uint32_t color)
{
    uint8_t *mgr = CE_POPUP_MGR();
    int32_t idx, v;
    uint8_t *s;
    int n = 0;

    if (!mgr || !pos) return 0;
    idx = *(int32_t *)(mgr + MOF_POPUP_COUNTER);
    if (idx < 0 || idx >= MOF_POPUP_SLOTS) idx = 0;        /* 原生 adder：`cmp eax,0xa; jl` 否则归零 */
    s = mgr + CE_POPUP_RING + (size_t)idx * CE_POPUP_STRIDE;

    v = value;
    if (v < 0) {
        *(uint8_t *)s = (uint8_t)MOF_POPUP_GLYPH_MINUS;   /* 负数：原生写 0x0a、位数 1 */
        n = 1;
    } else if (v == 0) {
        *(uint8_t *)s = 0;                                 /* 0：原生写 digit '0'、位数 1 */
        n = 1;
    } else {
        while (v != 0 && n < MOF_POPUP_DIGITS) {           /* 逆序写个位（原生 `idiv 10` 同序）*/
            *(uint8_t *)(s + n) = (uint8_t)(v % 10);
            v /= 10;
            ++n;
        }
    }
    *(uint8_t *)(s + 0x3d) = (uint8_t)n;                   /* 位数 */
    *(uint32_t *)(s + CE_POPUP_COLOR) = color;             /* 颜色（TH10 用的是 D3DCOLOR 0xAARRGGBB）*/
    if (!(*(uint32_t *)(s + MOF_POPUP_FLAG) & 1u)) {       /* 动画类标志 bit0（原生 adder 同款复位）*/
        *(uint32_t *)(s + MOF_POPUP_FLAG) |= 1u;
        *(int32_t *)(s + MOF_POPUP_ANIM) = 0;
    }
    *(int32_t *)(s + CE_POPUP_AGE) = 0;                    /* age 归零（原生回收只清 busy ⇒ 必须自己复位）*/
    *(int32_t *)(s + CE_POPUP_AGE + 4) = 0;                /* +0x28 位移动画 float */
    *(int32_t *)(s + CE_POPUP_AGE - 4) = -1;               /* +0x20 上一帧 age */
    *(float *)(s + CE_POPUP_POS) = pos[0];
    *(float *)(s + CE_POPUP_POS + 4) = pos[1];
    *(float *)(s + CE_POPUP_POS + 8) = pos[2];
    *(float *)(s + MOF_POPUP_FIELD_SCALE) = 1.0f;          /* scale（tick 的弹出动画起点）*/
    *(uint8_t *)(s + CE_POPUP_BUSY) = 1;                   /* busy **最后置**（防同帧别人读到半初始化槽）*/
    *(int32_t *)(mgr + MOF_POPUP_COUNTER) = (idx + 1 >= MOF_POPUP_SLOTS) ? 0 : idx + 1;
    return 1;
}

/* ---- 收集回调（faith.c 发出来的）----
 * ★ 2026-09-15：改成**带位置**的那一支（`faith_user_t.on_pickup_at`，本卡专属）—— TH10 的弹窗
 *   画在**道具自己的坐标**上（`th10:0x41b360`），而旧回调签名不带位置 ⇒ 只能拿自机坐标近似
 *   （收集半径 30 px 的误差）。带位置的那一支一到位，弹窗就与一手一致。
 * 逻辑与 `mof_on_pickup()` 完全相同，只多一个弹窗；两处都保留（旧入口仍是 faith.c 的默认路径）。*/
static void mof_on_pickup(void *user, int slot_mask, int tier, int32_t value)
{
    ce_card_t *c = (ce_card_t *)user;
    mof_card_t *s = c ? st(c) : 0;
    const float *pp;
    int32_t coeff;
    faith_ev_t ev;

    if (!s || !(slot_mask & (1 << FAITH_SLOT_110))) return;

    /* 事件档（§7.5 的表）：大信仰点 `FAITH_EV_BIG`、**小信仰点 `FAITH_EV_SMALL_FAITH`**。
     * ★ 2026-09-12 审计修（任务 2）：原来两条通道**都**走 `FAITH_EV_GREEN`（连击 +100f）
     *   ⇒ 小信仰点比 §7.5 多 40f（review §7.5 的连击表第 864 行：**小 P / 小信仰点 +60f**；
     *   `faith_core.h` 的 `FAITH_EV_SMALL_FAITH` 本来就是为这一档留的）。
     *   ⚠ 映射本体在 `mof_core_pickup_ev()`（纯逻辑层 ⇒ **有单测**，卡侧这段主机编不出来）。*/
    ev = mof_core_pickup_ev(tier);

    /* 信仰：加值由 `value` 给（大 5000/8000/10000、小 100），事件档只决定「哪一档」。*/
    (void)faith_slot_add(&s->slot, ev, value);

    /* 连击槽：大信仰点 +120f、小信仰点 +60f（§7.5 + 一手 th10 的充能立即数；表在 `mof_core.h`）。
     * ⚠ 这里**不用** `faith_combo_add(ev)`：共用件那张表把「大」档写成 130f（一手实测 120f），
     *   而它同时被 111 的大绿点通道读 ⇒ 110 用自己的表（`mof_core_pickup_combo()`），别改共用件。
     * `faith_combo_set()` 自带 [0, 130] 夹取（130 = 槽满，一手 `0x413035`）。*/
    (void)faith_combo_set(&s->slot, s->slot.combo + mof_core_pickup_combo(tier));

    /* 卡内奖残计数（§2.6）：蓝点分 = 信仰 × 高度系数。
     * ★ 2026-09-12 审计修（任务 1）：高度系数改成**真正的线性插值**（§2.4 / review §7.6
     *   「按到收点线（`player+0x47998`）的高度线性打折：线上 = 1.0，板底 ≈ 0.54」）。
     *   旧实现是二元 —— `faith_player_above_line() ? TOP : BOTTOM` ⇒ **线下 1px 就吃 0.54**
     *   （`mof_core_height_coeff()` 当时是死代码）。
     * ⚠ 高度取**收集那一刻的自机 y**：实体层的回调签名（`faith.h` 的 `faith_pickup_fn`）
     *   **不带这颗点的位置**，而收集判定本身就是「与自机的距离² ≤ 收集半径²（默认 30px）」
     *   ⇒ 收的那一帧自机 y 就是收集高度（误差 ≤ 收集半径 ≈ 30px，占 224px 跨度的 13%）。
     *   ★ 2026-09-15：`faith.h` 的 `faith_user_t` 尾部加了**带位置**的回调（`on_pickup_at`），
     *     110 现在走那一支 ⇒ 弹窗不受这一误差影响；但**高度系数仍用自机 y**
     *     （那是 §7.6 的用户口径、不是一手的式子）。换成「点自己的 y」现在只需一行
     *     （`mof_on_pickup_at()` 里把 `pp[1]` 换成回调给的 `y`）—— **故意留作独立小改**，
     *     因为本轮的任务是灵击/弹窗/判据，不顺手改计分行为。登记在报告「未做」里。
     *   ⚠ 一手 th10 用的确实是**道具自己的 y**（`0x41b4aa` `fld [edx+0x3c4]` 比 144.0f）⇒ 这是已知偏差。
     * 自机指针为空时退回旧的保守档 BOTTOM（本回调从 `on_tick_2` → `faith_pool_tick()` 里发出来，
     * 那条路已经挡过 `!CE_PLAYER()`，所以这是纯防御）。*/
    pp = ce_player_pos();
    coeff = pp ? mof_core_pickup_coeff(pp[1], faith_player_poc_height()) : MOF_BLUE_COEF_BOTTOM;
    mof_bonus_add(&s->bonus, mof_core_blue_value(faith_slot_total(&s->slot), coeff));

    /* 「收到」计数（`stat_big` / `stat_small`）：**只在这里**记 —— 撒点处记的是「撒出」
     * （`stat_*_spawn`）。两者分开的理由见结构体注释（旧实现一个字段担两件事 ⇒ 一颗点记两次）。*/
    if (tier == (int)FAITH_TIER_BIG) ++s->stat_big; else ++s->stat_small;
    mof_declare_score(s);
}

/* ---- ★ 带位置的收集回调（`faith_user_t.on_pickup_at`）：同一套记账 **+ 绿色 `+N` 弹窗** ----
 * 值/色 = **复刻**（TH10：大信仰点 = 本难度的 5000/8000/10000、小信仰点 = 100，都是绿 `0xff00ff00`，
 * 见 `mof_popup_number()` 的注释）；位置 = 道具自身坐标（一手 `th10:0x41b360`）。
 * ⚠ 微档（消弹的 +10）**不弹**：一手 TH10 的 type 8 微信仰点没有弹窗（`th10:0x41b5d6` 全程不调
 *   `0x42b9c0`），而且它在我们这里根本不建实体（`faith_spawn()` 直接返回 0）⇒ 天然一致。*/
static void mof_on_pickup_at(void *user, int slot_mask, int tier, int32_t value, float x, float y)
{
    float pos[3];

    mof_on_pickup(user, slot_mask, tier, value);              /* 记账先做（弹窗失败也不影响数值）*/
    if (!(slot_mask & (1 << FAITH_SLOT_110))) return;
    pos[0] = x;
    pos[1] = y;
    pos[2] = 0.0f;
    if (!mof_popup_number(pos, value, MOF_POPUP_GREEN))
        ce_log("mof: green popup dropped (popup ring busy?) value %d", (int)value);
}

/* ---- 消弹回调（faith.c 在 `faith_cancel_account()` 里发）----
 * 「微信仰点：**消弹每颗 +10**」（§2.2 / §7.5）。
 * ★ 一手（TH10）：灵击的消弹函数 `th10:0x408100` 在每颗被消掉的弹上**撒一颗 type 8 微信仰点**：
 *   `th10:0x4081e5: push 0x8`（type）→ `th10:0x4081e7: call 0x41bb00`（`ItemManager::spawn_item`），
 *   角度 `0xbfc90fdb`（−π/2 = 向上）、速度 `0x3f19999a`（0.6f）⇒ 本卡的「每颗 +10 信仰」照它接账
 *   （我们**不建实体** —— 一手的微档是可见的星形碎片，这一档的实体版见 `faith.c` 头注的 ❓ 自定）。
 * 颗数**直接吃 `ce_cancel_radius()` 的返回值**（§5.5 订正 3：`ce_cancel_all_bullets()` 是 void、数不出来）。*/
static void mof_on_cancel(void *user, int32_t bullets)
{
    ce_card_t *c = (ce_card_t *)user;
    mof_card_t *s = c ? st(c) : 0;

    if (!s || bullets <= 0) return;
    s->stat_cancel += bullets;
    /* ★ E-1 硬约束（2026-09-15）：这一档是 **type 8（微）**，只许走 `FAITH_TIER_MICRO`。
     *   ⚠ 微档今天**不建实体**（`faith_cancel_account` 只记账）⇒ 将来补实体版时，tier 必须取
     *   `faith_core_tier_for_source(FAITH_SRC_CANCEL)`（= MICRO），不许写成 SMALL/大档
     *   （一手：每颗被消的弹 → `th10:0x4081e7 push 8` / `th11:0x40b0a2 mov ecx,8`；
     *    角 −π/2、速 **0.6**，撒在**弹自己的位置** —— 但 `ce_cancel_radius()` 只给颗数、给不了位置，
     *    这是微档实体版还差的那一条一手，登记在报告与 `TUNING.md`）。*/
    faith_slot_add_cancel(&s->slot, 0, bullets, 0, 0);        /* 110：每颗 +10，落 Δ_110 */
    /* 连击槽：微信仰点 +3f/颗（§7.5）。⚠ 原作是「每颗 +3f」，没有每帧上限 —— 照抄。*/
    {
        int32_t k = bullets;
        while (k-- > 0) faith_combo_add(&s->slot, FAITH_EV_MICRO);
    }
    mof_declare_score(s);
}

/* 这只敌人**长什么样** —— ★★ 2026-09-15 新增（大信仰点判据换成宿主表后，卡侧要读它）。
 *
 * 一手（TH18）：`zEnemyData` 里有两个字段记着「这敌画的是哪个 anm、哪条脚本」：
 *   · `+0x268` = **anm 源选择子 N**（= `anmSelect(N)` 写进 `+0x264` 的那个值，在这里留档）
 *   · `+0x26c` = **主脚本号 S**（该 anm 文件的**扁平脚本号**；`S` = 画的是哪一套动画）
 * 写入点（两处，都**只在槽号 K==0 时**执行 —— 而原生 153/153 次 `anmSetMain` 全用 K=0）：
 *   · `th18:0x436c85 mov [edi+0x26c],eax`（`eax` = 第 2 实参 S）与
 *     `th18:0x436c91 mov [edi+0x268],eax`（`eax` = `[edi+0x264]` = N）—— **`anmSetSprite`**（303）
 *   · `th18:0x431659 mov [edi+0x270],eax` / `th18:0x43165f mov [edi+0x26c],eax` /
 *     `th18:0x431675 mov [edi+0x268],eax` —— **`anmSetMain`**（306）
 *   `edi` = `zEnemyData` = `zEnemy + 0x122c`（`engine.h` 的 `CE_ENEMY_DATA`）
 *   ⇒ **绝对偏移 = `enemy+0x1494`（N）/ `enemy+0x1498`（S）**。
 * ⚠ 为什么用它当判据：`ce_enemy_death_t` 里**没有**敌人类型号（`ev->id` = `enemy+0x6830`，
 *   是**逐实例递增**的流水号，不是类型），而这一对 `(N,S)` = 「它画的是哪一张贴图」，
 *   正是用户说的**视觉语言**（绿妖精 = 大信仰点宿主）。全套预研见 `mof_big_faith_table.h`。
 * ★ 2026-09-22（P6）：那两个偏移**已提到 `engine.h`**（`CE_ENEMY_ANM_SRC` / `CE_ENEMY_ANM_SCRIPT`）——
 *   111（`cards/signal.c` 的宿主表判据）与绿妖精载体（`cards/faith.c`）读的是同一对字段
 *   ⇒ 常量只留一处出处（本文件原先的两条 `#define` 已删）。*/

static void mof_enemy_anm_identity(const uint8_t *e, int32_t *src, int32_t *script)
{
    if (src)    *src    = e ? *(const int32_t *)(e + CE_ENEMY_ANM_SRC)    : -1;
    if (script) *script = e ? *(const int32_t *)(e + CE_ENEMY_ANM_SCRIPT) : -1;
}

/* ============================================================================================
 * ★★ 2026-09-18：把「一部分妖精」改成绿妖精 —— 改精灵 + 改掉落，**跟着卡走**
 *
 * 用户口径（原话）：「**更改部分妖精到绿妖精**的可能性：**改其精灵和掉落物**，并且**跟着卡走**
 *   （没有卡的时候不改）；卡只会在商店增减，可以认为**不会出现存在妖精的时候**要求这只妖精变成绿妖精」
 *   ＋「我预期改是**一波改一两只** —— 似乎 110 111 原作就是这样，**纯信仰点掉落的没那么多**」
 *   ＋（掉落那半）「**替换**：清掉原生 P/金钱，只撒大信仰点」。
 * 全量预研（一手：色相 / 同形判据 / 三条落点路线 / 站点预算）见 `REPORT-110-GREEN-FAIRY.md`；
 * 名单与纯逻辑（表 + 每行两个开关）见 `cards/mof_green_core.h`。
 *
 * ## 怎么改精灵（本文件的落笔）
 *
 * **图像层，零新增断点、零 VM 重建、与 ECL 零交互**：`enemy.anm` 的 sprite 表是**整文件一张**、
 * 按全局 sprite 号索引（一手 `AnmLoaded__set_sprite` `th18:0x477b00`：`anm+0x11c` 是表指针、
 * `ecx = id*0x11` 个 dword ⇒ 每项 `0x44` 字节）⇒ 把**源变体的 12 项整项覆盖**成**绿色变体的 12 项**
 * 就等于「这一支妖精换上了绿贴图」（同形异色：逐 pose 掩码 IoU 中位 1.000、alpha 像素数逐条相等）。
 * 因为脚本每条 `ins_300(spriteN)` 都会重写 `vm+0x24` 并重取 UV（`0x477b24`），**新生妖精从第一帧起
 * 就是绿的**；卡不在场时一个字节都不写，删卡时按备份精确还原。
 *
 * ## 为什么要按关收窄
 *
 * 同一支妖精的外观**跨关共用**（`S=184` 同时被 st02/st03/st07 用）⇒ 不加关掩码就会「一次改掉三关」，
 * 与「一波一两只」冲突。关号取 `CE_CURRENT_STAGE()`（**1-based**），掩码在表里（`stage_mask`）。
 *
 * ## 掉落那半（`mof_drop_wipe()`）
 *
 * 用户口径「变绿 = 掉绿点」是**同一个开关** ⇒ 判据 = 「**此刻真的改上了**（`faith_green_applied()` 的位）
 * ∧ 这一敌的 `(N,S)` 落在该行的源变体里」（`mof_green_lookup()`）。看着还是蓝的（体检没过 /
 * 不在这一关）**一律不算**，否则会出现「蓝妖精掉绿点」。
 * 撒点成功后**清掉原生掉落**（替换语义）——落笔点是死亡断点 `0x430240`（**在引擎撒主掉落
 * `0x430493` 之前**，同函数体内），三块一起清：主掉落 `+0x00`、附加表 `+0x04`（20 项）、
 * 限时表 `+0x54` 与它的窗口 `+0xa4`。撒点失败（池满）时**不清**（宁可保留原生掉落，不给玩家空手）。
 * ============================================================================================ */
/* ★ 用户 2026-09-18 的「替换」裁决落在**谁**身上 —— 一个开关：
 *   · 0（默认）= 只替换「**被我们变绿的**」那批（他问的就是这批：「变绿之后原本掉的 P/金钱怎么处理」）；
 *   · 1 = 连**本来就绿**的宿主（宿主表 `(2,10)/(2,56)/(2,68)/(2,80)`，例如用户点名的
 *     「一面开头的**绿阴阳玉** `Cir00m`」）也一并替换成「只给绿点」。
 *   ⚠ 打开它 = 改**既有**行为（那些站点现在同时掉原生 P/金钱 + 绿点），会动火力/金钱经济 ⇒ 默认关着。*/

/* 清掉这只敌人的**原生掉落**（用户口径「替换」）。一手：死亡断点 `0x430240` 是 `FUN_00430240`
 * 的**函数入口**，引擎撒主掉落在同一函数体内的 `0x430469`（读 `[edi+0x623c]`）→ `0x430493`；
 * 附加表 `ex_drop` 在更后面（`0x43049f` 才进 `0x430510`），限时表由它开头 `0x430544`–`0x430573`
 * 并入 ⇒ 三块 + 窗口一起清才干净（`zEnemyDrop` 的字段图：`+0x00` 主掉落 / `+0x04` 起 20 项 /
 * `+0x54` 起限时 20 项 / `+0xa4` 窗口帧数 —— 见 `engine/ecl/th18/02-item-drops.md` §5.1）。*/
static void mof_drop_wipe(uint8_t *e)
{
    uint8_t *d;
    if (!e) return;
    d = e + CE_ENEMY_DROP;
    memset(d, 0, CE_ENEMY_DROP_TIMED_TIME);              /* 主掉落 + ex_drop + 限时表 */
    *(int32_t *)(d + CE_ENEMY_DROP_TIMED_TIME) = 0;      /* 限时窗口（不清它，下一帧还会并回来）*/
}

/* ★★★ 2026-09-22（P6）：绿妖精载体（`FAITH_GF_*` / `gf_*`，原在本文件）**整段搬进共用件**
 *   `cards/faith.c` 的 `faith_green_fairy_tick()` —— 111（地灵殿）在它自己的 `on_tick_2` 里也要调它
 *   （用户报的 th11 #4「全程未见绿妖精」的根因就是「载体只被 110 调」）。
 *   ⚠ 110 侧**行为一字不变**：同一个入口、同一个帧内位置（`on_tick_2` 一开头），
 *     段内所有比较都是相对帧差 ⇒ 帧号换成引擎帧号（`ce_sdk_frame()`）等价。
 *   ⚠ 收尾（`faith_green_fairy_depart_all()`）在本文件的 `mof_clear()` 里调（铁律二）。*/
/* ★★ 2026-09-18c / 2026-09-22：大信仰点的**落点散布** = ZUN 自己「一次爆多颗道具」的分布
 * （一手公式见 `faith_core.h` 的 `faith_core_scatter_off()`）。
 * ★ 2026-09-22：实现**搬进共用件** `faith_scatter_from_enemy()`（`cards/faith.{h,c}`）——
 *   111 那边（th11 #1「绿点和金钱重合」）需要**同一条**公式，留两份迟早漂；
 *   本卡这里只剩一次调用，参数与语义逐字不变（`enemy` = 死掉的那只、偏移加到 `ev->x/ev->y` 上）。
 *   ⚠ 那次搬家的随机源也从本文件的 `s_scatter_seed` 挪进了共用件（同一个 LCG、同一调用顺序）。*/

/* ---- 击破 → 大信仰点（§7.9）★ **正式钩子 = `on_enemy_death`**（共享死亡断点 `0x430240`）----
 *
 * 为什么是死亡钩子（2026-09-12 SDK owner 修完断点后的正式路径）：
 *   · 断点 `0x430240`（`cavesize 6`、期望字节 `558bec83ec14`）**在引擎撒主掉落之前**
 *     （体内顺序：死亡特效 → `0x430469` 读表 → `0x430493` 撒 → `0x43049f` 触发
 *     `on_enemy_drop_pre` → `0x4304aa` 清表）⇒ 只有这里能拿到**未经消费**的 `drop_main`；
 *   · `ce_hooks_t.on_enemy_death` 是**事件钩子**（不是 21 槽虚表成员），`sdk.c` 沿卡链表直接
 *     `h->on_enemy_death(card, &ev)` 派发 ⇒ 卡侧 `.on_enemy_death =` 本来就能被调到
 *     （`0x84` 只是 trace 槽号，`CE_CARD` 不需要为它生成桩）。
 *     ⚠ 2026-09-12 之前挂不上**不是桩的问题，是断点没装**（`sites.py` 下标取址错位 / 对账表缺条目 /
 *     patch 未重生成）—— 三处已由 SDK owner 修好，现在 `CE_BP_ENEMY_DEATH_RVA = 0x030240`。
 *
 * 本回调的两条**硬禁令**（设计 §2.4 的重入风险）：
 *   · **禁止析构任何敌人**（断点必在敌人链表遍历栈里 ⇒ UAF：`sweep` 先存 next、同帧会删）；
 *   · **禁止缓存 `ev->enemy`**（只在本次回调内有效；要记账只用 `ev->id`）。
 *   本实现两条都遵守：只读 `ev` 的字段与敌人自己的两个只读身份字段、只撒自建 VM、不碰任何敌人对象。
 *
 * ★★ **判据（2026-09-15 用户裁决后换掉）：从「这敌掉的是不是金钱（`drop_main == 2`）」
 *   换成「这敌在不在**宿主表**里」**（`mof_core_wants_big_host()`）：
 *   · 表 = `mof_big_faith_table.h`（TH18 侧实测为「绿」的那批敌人，4 个视觉身份 `(N,S)`）；
 *   · 键 = `(enemy+0x1494, enemy+0x1498)`（见上面 `mof_enemy_anm_identity()` 的一手）；
 *   · 门 = 非 KILL_ALL（过场全清会把计数刷满）+ 真敌人（`CE_EDEATH_F_REAL`）+ **非 boss**；
 *     ⚠ 旧门里「必须有掉落表」那一条**去掉了**（宿主按外观定，TH18 里确有原生 item = 0 的宿主，
 *     例如 st04 的 `Cir02/Cir03`）—— 理由与一手在 `mof_core.c` 的 `mof_core_death_is_real()`。
 *   · **`drop_main` 不再参与判据**（旧口径下「每只掉钱的敌人」都给 ⇒ 与金钱同点同生，
 *     正是用户观察到的问题）；它仍被打进日志，方便实跑对账「宿主 ↔ 它掉了什么」。
 *   · 旧的**判据②**（「同时掉钱与 P」= `on_enemy_drop_pre` 的那条路）随之作废 ——
 *     它服务的正是「掉钱的敌人都给」这个已被推翻的前提 ⇒ **本卡不再挂 `on_enemy_drop_pre`**
 *     （2026-09-15 删；`mof_core_wants_big_dual()` 保留给回归单测）。*/
static void on_enemy_death(ce_card_t *c, const ce_enemy_death_t *ev)
{
    mof_card_t *s = st(c);
    int32_t v = 0;
    int32_t anm_src = -1, anm_script = -1;
    int is_boss, want_big, is_green;
    float sc[2];                            /* ★ 落点散布（ZUN 的爆道具分布公式）*/

    if (!s || !ev) return;
    if (!mof_core_death_is_real(ev->kind, ev->flags)) return;
    /* 「这是不是 boss」用 `mof_is_boss()`（= `CE_ENEMY_FLAG_BOSS`，一手：ECL opcode 512 `setBoss` 置位），
     * 与 TH10 `enemy+0x1444` bit15 逐条同形（见那个函数的注释）。
     * ⚠ 2026-09-15 起它**不是**「掉钱那一路的额外收窄」，而是新判据的一部分（表 + 非 boss）；
     * `mof_core_wants_big_host()` 内部先判 `is_boss` ⇒ 这里传进去即可，别再自己加条件。
     * 为什么必须有这道门：TH18 有 boss 的贴图**正好也发绿**（st02 `Boss*`/`MBoss` 用 `st02enm` script0，
     * 实测 hue 134），不加门就会被表外的键挡不住（表只收 N=2，那支是 N=3，属双保险）。*/
    is_boss = mof_is_boss((const uint8_t *)ev->enemy);

    /* ★★ 2026-09-18：**击破闩锁** —— 一手的信仰衰减速率按「这一帧有没有击破」选档
     *   （一手 `th10:0x40e26b` 在敌机死亡路径里把速率字段写 1 ⇒ 那一帧只扣 −10 显示）。
     *   置位点选在这里（过了 `mof_core_death_is_real()` 门）= 与一手同一语境：**真击破**才写。
     *   `on_tick_2` 里读走并清（见 `gate.killed_since_last`）。*/
    s_mof_killed = 1;

    /* ★★ 2026-09-16 补：**击破一只非 boss ⇒ 连击槽 +10f**（一手 `th10:0x40e681 push 0xa`，
     *   坐标在 TH10 敌人被移除/撒落那一段里、无条件给一次；表与口径见 `mof_core.h`）。
     *   ⚠ 位置：在 `want_big` 的早退**之前** —— 宿主表只决定「要不要撒大信仰点」，
     *     与「这只被击破了」是两件事（普通妖精也要充能）。
     *   ⚠ 门沿用上面那条 `mof_core_death_is_real()`（过场 KILL_ALL 全清不算击破 —— 一手那一段
     *     在敌人**自然结算**里，不是「被脚本清场」）。boss ⇒ +0f（用户口径与一手一致）。*/
    if (!is_boss) {
        (void)faith_combo_set(&s->slot, s->slot.combo + MOF_COMBO_KILL);
        s->stat_combo_kill += MOF_COMBO_KILL;
    }

    mof_enemy_anm_identity((const uint8_t *)ev->enemy, &anm_src, &anm_script);
    want_big = mof_core_wants_big_host(ev->kind, ev->flags, anm_src, anm_script, is_boss,
                                       mof_difficulty(), &v);
    /* ★★ 2026-09-18：**被我们变绿的**妖精同样算宿主（用户口径「变绿 = 掉绿点」是同一个开关）。
     * ⚠ 判据只看 `faith_green_applied()`（**此刻真的改上了**）—— 体检没过 / 不在这一关的妖精
     *   看着仍是原色 ⇒ 不许按宿主给点（否则就是「蓝妖精掉绿点」）。*/
    is_green = 0;
    if (!want_big && mof_green_lookup(faith_green_applied(), anm_src, anm_script)) {
        want_big = mof_core_wants_big_green(ev->kind, ev->flags, is_boss, mof_difficulty(), &v);
        is_green = want_big;
    }
    if (!want_big) return;
    faith_scatter_from_enemy((const uint8_t *)ev->enemy, &sc[0], &sc[1]);
    /* ★★ 2026-09-18c：落点按 **ZUN 自己「一次爆多颗道具」的分布公式**撒开（用户口径逐字：
     * 「不是扇形，而是**参考 zun 的爆多个道具是如何分布的那个代码**」）—— 出生角仍是**一手**的
     * −π/2 直上，只有**落点**按那条公式偏移：`pos = 敌机坐标 + polar(rand±×π, rx, ry) × (rand01 × 0.5)`。
     * 一手逐条与默认半径见 `cards/faith_core.h` 的 `faith_core_scatter_off()` / `engine.h` 的
     * `CE_ENEMY_DROP_AREA_*`。⚠ 一波绿阴阳玉同时死时，它们的点就会各偏一处、不再叠成一条竖线。*/
    /* 打开它 = 连**原生就在掉点**的那些站点也改（会动火力/金钱经济）⇒ 默认关。换皮驱动本身在
     * 共享件（`faith.c` 的 `faith_green_swap_tick()`）。*/
#ifndef MOF_GREEN_WIPE_HOSTS
#define MOF_GREEN_WIPE_HOSTS 0
#endif
    if (faith_spawn_big(ev->x + sc[0], ev->y + sc[1], v)) {
        /* ★ 用户 2026-09-18 裁决：**替换**（不是追加）—— 变绿的妖精只给绿点，
         *   原生 P/金钱整体清掉（「金钱和绿点总之别重合」那条也一并满足）。
         *   顺序：先撒我们的点，成功才清原生掉落（池满时宁可保留原生掉落，不给玩家空手）。*/
        if (is_green || (MOF_GREEN_WIPE_HOSTS && !is_green)) mof_drop_wipe((uint8_t *)ev->enemy);
        ++s->stat_big_spawn;
        ce_log("mof: big faith +%d via %s @(%.0f,%.0f) (anm %d/%d, kind %u, drop_main %d, boss %d, enemy %u)",
               (int)v, is_green ? "GREEN fairy" : "host table",
               (double)ev->x, (double)ev->y, (int)anm_src, (int)anm_script,
               (unsigned)ev->kind, (int)ev->drop_main, is_boss, (unsigned)ev->id);
    } else {
        ++s->stat_spawn_failed;
    }
}


/* 打开它 = 连**原生就在掉点**的那些站点也改（会动火力/金钱经济）⇒ 默认关。定义留在这里，
 * 换皮驱动本身在共享件（`faith.c` 的 `faith_green_swap_tick()`）。*/


/* ---- ★★ 2026-09-15：`on_enemy_drop_pre` 整条**已删**（旧判据②随之作废）----
 *
 * 删掉的理由（一句话）：**它服务的那个前提被用户裁决推翻了**。
 *   · 它原来的唯一职责 = 判据②「主掉落不是钱、但 ex_drop 里钱与 P 同时 > 0 ⇒ 也给一颗大信仰点」
 *     —— 同属「**按这敌掉什么**给绿点」的旧口径（§7.9）。
 *   · 2026-09-15 用户裁决：「**金钱和绿点总之别重合**」⇒ 走「判据收窄」：大信仰点只给
 *     **原作（TH10）真的声明了 type 3 的那批敌人的对应物**（宿主表 `mof_big_faith_table.h`）
 *     ⇒ 「掉什么」这个输入**整个退出判据**，判据② 没有存在余地。
 *   · 判据① 与判据② 曾**互斥**（`drop_main == 2` 走死亡钩子、否则走本钩子）；现在只剩死亡钩子一条路，
 *     且它连 `drop_main` 都不读 ⇒ 本钩子**零职责**（留着就是一个每只敌人死亡都要跑一遍的空函数）。
 *   · `mof_core_wants_big_dual()` 仍在 `mof_core.c` 里（签名冻结、**回归单测还在测**），只是卡不再调。
 *
 * ⚠ 历史（两轮注释，删函数时保留要点，免得下一个人重犯）：
 *   · **2026-09-12 审计裁决**：小信仰点（满 P 的 P 点转化）**从本钩子删掉了** —— 理由是同一颗 P 点
 *     会被记两次（本钩子看到「这一波要掉什么」，`on_item_collect` 看到玩家真吃到的那一颗）
 *     ⇒ 撒两颗实体 ⇒ `Δ_110` 与连击槽各双份。
 *   · **2026-09-15 收口（用户定案 A）**：那条裁决的**前提**也被换掉了 —— 现在根本不会有「满 P 吃 P 点」：
 *     共用件 `faith_convert_full_p_items()`（`on_tick_2` 每帧调）在满 P 时就把原生 P 类道具**原位删掉
 *     并换成自建的 type 9 档点**，`on_item_collect` 只剩**同帧竞态网**。一手与译文见那个函数的长注。*/

/* ---- 满 P 时 P 类道具被**替换**掉之后补的那一颗（`on_item_collect`）★ 同帧竞态网 ----
 *
 * ★★ 2026-09-15 收口（用户定案 A）：**主路**已经不在这个函数里了 —— 见
 *   `faith.c` 的 `faith_convert_full_p_items()`（卡侧每帧在 `on_tick_2` 里调）：满 P 时
 *   **扫原生道具池、把 P 类道具原位删掉、在它自己的位置撒一颗 type 9 档的点**。
 *   一手 = `th10:0x41bbf8`（满 P 时 spawn 就把 type 1/4 改写成 9）+ `th10:0x41ba50`
 *   （满的那一帧全场原位重生 + 上抛）⇒ 原版那份 P 点**根本不存在**（不是「P 点 + 信仰点」两份）。
 *
 * 那么这里还留着做什么？**同一帧的竞态网**：引擎的道具 tick（`0x445a80`）有可能在我们扫到那颗
 * P 道具之前就把它喂给了玩家（玩家贴着敌人站，30 px 内）—— 那一颗**已经**拿过原生的满 P 身价分
 * （`th18:0x446981` 的分支），如果不再补一颗点，「TH10 里它本来该是信仰点」这件事就丢了。
 * ⇒ 判据 `mof_core_full_p_replace()`（满 P 且 type ∈ {小 P, 大 P}）在这里再判一次。
 * ⚠ **正常路径下它不触发**（那颗道具已经被主路删了）⇒ 不会出现「同一颗 P 点既算 P、又加撒一颗」。
 *
 * 两处一手细节沿用：
 *   · 类型号：TH18 的「P 类」= **{1 小 P, 3 大 P}**（一手 `th18:0x44625f` 分派表 + `0x446981` 的满 P 分支），
 *     对位 TH10 的 type 1 / 4（两者都在 `th10:0x41be28` 的表里 → 9）；TH18 的 type 8（补满火力）
 *     TH10 没有对应档 ⇒ 不接（`TUNING.md` §2.5）。
 *   · 档位：**只许 type 9 档**（E-1 硬约束）—— 走 `faith_core_tier_for_source(FAITH_SRC_FULL_P)`，
 *     不再手写 `FAITH_TIER_*`。
 * ⚠ 撒出来的**一律是我们自建 VM**（TH18 没有信仰点 type）—— 我们不去改原生 type（改不出绿点，
 *   只会变成引擎自己的「分数转换点」），而是把原道具删掉（主路）+ 自建点顶上来。
 * ★ 2026-09-12 审计裁决：`on_enemy_drop_pre` 里那条**同判据**的撒点已删（理由见那个函数的注释）。*/
static void on_item_collect(ce_card_t *c, uint32_t type, const void *item)
{
    mof_card_t *s = st(c);
    const float *ip;

    /* ★★ 2026-09-15：**先无条件记下本颗道具的身份**（P 点火力那一层要它，见下面的 `on_item_commit`）。
     * `on_item_commit`（`0x44658a`）**不带 payload**，所以只能靠「collect → handler → commit」
     * 这对断点背靠背的关系自己闩一下（**同 `collect2x.c` 的 `s_stash` 形状**）。
     * ⚠ 必须写在**任何 return 之前**：早退就等于让上一颗道具的身份漏到这一颗的 commit 上。 */
    s_p_item = (type == (uint32_t)MOF_P_ITEM_TYPE18);
    if (!s || !item) return;

    /* ★★ 2026-09-16 补：**引擎道具也充连击槽**（原来这条通道一条都没接 —— 用户实跑
     *   「连击值一点也不涨，击破妖精是涨连击值的啊」）。
     *   表与一手（th10 的 `push` 立即数）全在 `mof_core.h`；这里只接线。
     *   ⚠ 必须写在下面那个 `mof_core_full_p_replace()` 的早退**之前** —— 满 P 时吃到的
     *     P 点同样要充能（一手那颗 P 点已经被改写转档，但「收到一颗」这件事仍然发生）。
     *   ⚠ 不设任何门：一手那 9 个充能点都不看 boss/对话/关卡帧（那三条门只挡**衰减**）。*/
    {
        int32_t add = mof_core_item_combo((int32_t)type);
        if (add > 0) {
            (void)faith_combo_set(&s->slot, s->slot.combo + add);
            s->stat_combo_item += add;
        }
    }
    /* 判据全在 core（`mof_core_full_p_replace`）：满 P 且 type ∈ {小 P, 大 P} ⇒ 补一颗。
     * （`power < max_power` 时它自己返回 0 ⇒ 这里不必再挡一次。）*/
    if (!mof_core_full_p_replace((int32_t)type, CE_CURRENT_POWER(), CE_MAX_POWER_VAL())) return;
    ip = (const float *)((const uint8_t *)item + CE_ITEM_POS);
    if (faith_spawn(ip[0], ip[1], faith_core_tier_for_source(FAITH_SRC_FULL_P),
                    1 << FAITH_SLOT_110, FAITH_MOF_SMALL)) {
        ++s->stat_small_spawn;
        ce_log("mof: 满 P 吃 P 类道具 (type %u) → 小信仰点 +%d（同帧竞态网：主路没来得及删）",
               (unsigned)type, (int)FAITH_MOF_SMALL);
    } else {
        ++s->stat_spawn_failed;
    }
}

/* ==================== ★★ 装了 110（或 111）⇒ 一颗 P 点 = +0.05 档火力 ====================
 * 用户 2026-09-15：「装了 110 或 111，P 点变成 +0.05」。全部推导（一手链、替换 vs 补差、
 * 为什么落在这个断点、满火力怎么夹、谁落笔）在 `mof_core.h` 的 §P 点火力 那一大段，这里只接线。
 *
 * 一句话版：**落点 = `on_item_commit`（`th18:0x44658a`），不是 `on_item_collect`（`0x44625f`）** ——
 * 原生那一发 `add_power(1)`（`th18:0x446a7b` → `0x4573f0`）发生在两个断点之间，且在 collect 里
 * 先写会**打乱原生自己的跨档判定**（它拿被改过的值做 `idiv [0x4ccd40]` 比较）⇒ 先写会让升级静默
 * （子机不按新档重建）。在 commit 里补差：原生那一步逐字节不变，我们这 4 单位的跨档由 core 的
 * `repop` 标志点名、补一次 `ce_repopulate_options()`（与灵击扣火力同款收尾）。
 *
 * ⚠ 两张卡同装只算一次：`mof_core_p_owner()` = **id 最小者落笔** ⇒ 110 恒落笔、111 让位
 *   （111 侧读 `ce_owned(110)`；本卡自己就是在场的那张 110）。**不是**按帧闩锁：
 *   `ce_cancel_latch_*` 那种闩锁每帧 reset，而同帧收一撮 P 点是常态 ⇒ 会漏掉同帧的第 2 颗以后。
 * ⚠ 只碰火力：不写 `MONEY`、不动道具数值 / 不删道具 / 不改分派表。
 *
 * ## ★★ 2026-09-15h：补多少 = **共享策略**（`faith_p_topup_take()`），本卡不再自己算
 *   为什么：111 的支援特技「爱丽丝」按用户口径把 P 点收益改成**平均 +0.025 档**，而**落笔者恒为 110**
 *   ⇒ 那条减益若只写在 111 的卡侧就会被绕过。现在两张卡都只做「**问策略 → 用它当补值**」：
 *     · 策略判据 = `faith_registered_mask()` 的 111 位（在场）+ `CE_ADDR_CHARACTER == 魔理沙(1)`（机体门）
 *       ⇒ 常态 5 单位（+0.05 档）、爱丽丝生效时 **2 / 3 交替**（平均 2.5 单位 = +0.025 档）；
 *     · 相位只有**一份**（共享件静态区），**只有落笔者推进** ⇒ 本卡在下面 `owner` 检查之后才调它，
 *       「同一颗道具只推进一次」是既有选举保证的（双卡同装时 111 早退、一个字节都不写）。
 *   ⚠ 纪律：**本卡不许自己实现 2/3 交替**（两处实现必然漂移）；要改数值就改 `faith_core.h` 的那三个常量。*/
static void on_item_commit(ce_card_t *c)
{
    mof_card_t *s = st(c);
    const int is_p = s_p_item;
    int32_t total;
    mof_p_topup_t t;

    s_p_item = 0;                                  /* 本卡自己的闩：一颗道具只认一次 */
    if (!is_p) return;                             /* 不是小 P（type 1）⇒ 本层不管 */
    if (mof_core_p_owner(1, ce_owned(111)) != 110) return;   /* id 最小者落笔（111 在场也归 110）*/

    /* ★★ 问共享策略（**会推进共享相位** ⇒ 只有落笔者可以调，见上面的纪律）。*/
    total = faith_p_topup_take();                   /* 常态 5 / 爱丽丝生效时 2、3 交替 */
    /* 此刻原生那 +1 已经落地（handler 跑完了）⇒ 读到的就是 `power_after_native`。*/
    t = mof_core_p_topup_units(CE_CURRENT_POWER(), CE_MAX_POWER_VAL(), total);
    if (t.delta <= 0) return;                      /* 满火力 / 引擎没初始化 ⇒ 一个字节都不写 */

    CE_CURRENT_POWER() = CE_CURRENT_POWER() + t.delta;
    if (t.repop) {
        /* 原生那一步没报的那次跨档 —— 我们自己补收尾（子机按新火力档重建 + `on_power_level_change` 广播）。
         * 一手先例：原生 P 例程自己就是「跨档 ⇒ `0x45d5e0`」= `th18:0x446a90`。*/
        ce_repopulate_options();
        ce_log("mof: P 点补差跨档 → 火力 %d 单位（%d 档）· ce_repopulate_options()",
               (int)CE_CURRENT_POWER(), (int)(CE_CURRENT_POWER() / 100));
    }
    if (s) ++s->stat_p_topup;                      /* 诊断：补差成功的颗数（不逐颗刷日志；随每 MOF_LOG_EVERY 帧的 `Ptop` 一行）*/
}

/* ---- ★ 计分层（2026-09-13 重做：声明自己的那一份 G_i）----
 * 本卡是 TH10 那一族：**信仰**（`mof_core.h` 的信仰槽）。对用户公式而言
 *   `G_10 = 基础(50000) + Δ_10`（`faith_score_unit(&s->slot)`），
 * 而且它是**耦合族**——用户规格里唯一带乘数的那一项：
 *   `(Δ_10 + Δ_11) × (擦弹倍率 + 信号强度)`。
 *
 * ★ 重做前这里的问题（`CARD-NUMERIC-INTERACTION-AUDIT.md` §1.6）：
 *   110 与 111 各自把「**基础 + Δ**」一起声明，而落笔者（id 最小）再用 `mof_core_pv()` 的
 *   「两卡同装」那一支把两份 base 减掉 —— 那套减法是为了绕开「谁声明了什么」的不确定性，
 *   一旦 107（id 更小）在场就整条不跑。
 *   现在通道本身带语义，不需要减法：
 *     · `ce_score_declare_coupled(id, Δ)` —— **交出去的只是 Δ，基础 G_0 由层出**
 *       （用户公式 `G = G_0 + Σ G_i`：基础只算一次，落在层里，谁也 dupe 不了）；
 *     · 耦合系数 `ce_score_coupling_permille()` 由层合成（擦弹倍率来自原生擦弹计数、
 *       信号强度来自 111 的声明），110 **不读别人的卡状态**。
 *   ⇒ 本卡不再自守 `ce_score_applier()`，也不再写 `*value`。
 *
 * ⚠ `coeff_06`（收取高度）不在这里：引擎自己乘（一手 `th18:0x446bf9`–`0x446cf3`）。
 * ⚠ `mof_core_pv()` 仍然在，但**本卡不再用它算分**（它的「两卡同装」支是旧口径的补丁）；
 *   主机单测仍测它，避免删函数把别人的回归面拆了。*/
static void on_item_score(ce_card_t *c, int32_t *value)
{
    mof_card_t *s = st(c);

    (void)value;                       /* `*value` 由 SDK 统一写（`ce_score_apply()`）*/
    if (!s) return;
    mof_declare_score(s);              /* 每颗道具当场声明一次（值随信仰变，粘性但便宜）*/
}


/* ============================ 灵击（★ 2026-09-15 按 TH10 一手重写）============================
 *
 * ★★★ **2026-09-21：本节正文（触发之外的「怎么做」）整段搬进共用件 `cards/bomb10.{h,c}`。**
 *   下面这一整段**保留原样**——它是这套实现的**一手依据底稿**（TH10 的门槛 / 消耗 / 伤害链 /
 *   半径曲线 / 载体取舍），`bomb10.c` 的实现逐条对应它；函数名里的 `mof_bomb_start()` /
 *   `mof_bomb_tick()` 现在叫 `bomb10_start()` / `bomb10_tick()`（在共用件里，110/111 同一份）。
 *   本卡留在自己文件里的只有四项（对话门 / 整档扣火力 / 发动代价 / 消弹记账）——
 *   见本节末端那一段与 `s_bomb_host`。
 *
 * 触发：`on_tick_2` 读炸弹键**上升沿**（`INPUT_PRESSED & 0x2`）—— **不是 `on_bomb_spent`**
 * （它在 `0x4203bc`，炸弹**成功花掉之后**才触发，承载不了「库存 == 0」这条分支，§5.5 订正 2）。
 * 库存 == 0 → 本卡自干；库存 > 0 → 交给原生 `do_bomb`（本卡不动）。
 * ⚠ 扣完**必须** `ce_repopulate_options()`（Tsukasa / 商店先例）。
 *
 * ## ★★ 2026-09-15c **决死形态**（「决死 B」；用户口径「灵击==B」「所以灵击也得决死」）
 *   库存 == 0 且**自机在决死窗口里**（`player+0x476ac == 4`，一手 `th18:0x45d4e4`）时按 B
 *   ⇒ 本发灵击走**决死形态**：**照常发动**（扣 1 档火力 / 消弹 / 伤害源 / 节奏一字不改）+
 *   **调 `Player__cancel_impending_death` `th18:0x45d590` 救命**（现役实现 = 共用件的
 *   `bomb10_cancel_impending_death()`，由 `bomb10_start()` 在决死形态下调用）。
 *   两条代价口径（一手复核 + 判定依据全部在 `mof_core.h` 的「灵击 → 决死」块，**别在这里重复推理**）：
 *     · **不扣 −3000 信仰** ✅ 一手（TH10 决死那一支没有 `call 0x412e70`）；
 *     · **消弹不记「每颗 +10」** 🟡/❓ 自定（引文被本轮复核推翻）—— 开关 = `MOF_BOMB_DEATHBOMB_ACCOUNT`。
 *   ⚠ **不挂 `on_death_before_deathbomb`（卡虚表 `+0x10`）自动救人**：按 `EVENTS-CONTRACT.md`
 *     §4.2/§5.2 的救援层顺序（**107 → 117 → 决死 B → 113**），本卡扮演的是「**决死 B**」——
 *     语义就是**玩家在窗口内按 B**，不是 107/117 那种「中弹即自动爆」。**只做 B 键这条路。**
 *   ⚠ 一手对照（本轮复核 `th18:0x45c2a9` 那一支）：原生自己的「决死 B」= 窗口没走完时
 *     `can_bomb`（`0x420420`）→ `do_bomb`（`0x420360`）→ **`0x45c2d7 call 0x45d590`** ⇒
 *     「按 B 救命」在原生就是「调同一个救命函数」；库存 == 0 时 `can_bomb` 返回 0 ⇒ 原生这条路
 *     整条不走（`0x45c2ca je 0x45c3d9` 落到扣命）—— 这正是本卡补上的缺口。
 *   ★ **帧内时序（一手登记号；这决定了「不会太晚」）**：卡的 tick 在 **AbilityManager = 更新线 `0x16`**，
 *     而 `Player__on_tick`（决死窗口那条路 + `+0x0c` 广播）在 **Player 线 `0x17`** ⇒ **卡的 tick 先跑**
 *     （两个优先级都由仓库里的一手注释登记：`sdk.c` 的 `optpose` 段与 `trance_core.h` §5.4）。
 *     两个后果：① 中弹那一帧（Player 线里 state → 4）我们**看不到**（我们的 tick 已经跑过了）⇒
 *     最早在**下一帧**才认得出窗口 —— 窗口有 8 帧，够用；而且两边读的是**同一个**
 *     `INPUT_PRESSED`（`0x4ca434` = `(held ^ prev) & held` = 本帧刚按下）⇒ 原生那支要的也是
 *     「**窗口内的新按下**」，我们的「上升沿」触发与原生**同义**（不是「按住就行」）；
 *     ② 救命的调用**早于**同一帧 Player 线的扣命判定 ⇒ 被救的那一帧引擎走 case 1（存活），
 *     **不会**扣命、**不会**广播 `+0x0c`（113 与 111 的 miss 记账因此天然不触发）。
 *
 * ## 发动时（`mof_bomb_start`）抄一手的四件事
 *   ① 扣 1 档火力（`mof_core_power_spend`；门槛/下限是**用户裁决**，不是 TH10 原版，见 `mof_core.h`）；
 *   ② **非决死**灵击 **信仰 −3000**（一手 `th10:0x425a15`，`FAITH_REDUCE_BOMB`）；
 *      ★ 决死形态扣 0（`mof_bomb_form_faith_kind()` 给 `FAITH_REDUCE_NONE`，一手依据见上）；
 *   ③ **作废当前符卡奖励**（一手 `th10:0x405500`，被 `Bomb::start` 在 `th10:0x405982` 调）：
 *      门 = `[spellcard+0x3738] >= 60`（本次攻击已过 60 帧）⇒ 命中则
 *      `[spellcard+0x3790] = 0`（奖励值）+ 清 `[spellcard+0x378c]` 的 **bit1**（奖励存活位）。
 *      TH18 的同位字段/同位：`CE_SPELL_BONUS`（`th18:0x7c`）与 `CE_SPELL_FLAG_BONUS`
 *      （`th18:0x78` bit1，一手 `th18:0x42a7ed` 收符卡时读它 + `0x42a801 mul [edi+0x7c]` 计分）
 *      ⇒ **同一件事、同一位**，先例 = `cards/judgment.c`（神之宣告）的同一对写。**这是复刻。**
 *   ④ **RANK −= 0x80、钳 [−0x400, 0x400]**（一手 `th10:0x405987`–`th10:0x4059b8`）。
 *      ⚠ TH18 的 `0x4ccd08` 是**死字段**（只有 2 条读、都是 ECL global getter、没人写、没有 ECL 用它）
 *      ⇒ **写了没有实际效果**；仍照一手写（复刻旋钮），理由与出处见常量块的注释 + `TUNING.md` §2.5。
 *   ⑤ 音效：`se_slash.wav`（一手 `th10:0x405942` 播 id `0x26` = `se_slash.wav`）—— TH18 的同一 wav
 *      是 id `0x2c`/`0x2d` ⇒ 取 `0x2c`（★ 复刻同一 wav；❓ 两个 id 的取舍登记 `TUNING.md` §2.5）。
 *
 * ## 之后每帧（`mof_bomb_tick`）：消弹圈 + 中心上移 + 每敌每帧一笔伤害
 *   半径 = `mof_core_bomb_radius_centi(已过帧)`（16.0 → 目标、120 帧线性、其后保持）；
 *   中心 = 发动瞬间的自机坐标，**y 每帧上移** 0.5/1.0/1.3（一手 `th10:0x4057b6`–`0x4057de`）；
 *   消弹 = `ce_cancel_radius(中心, 半径, 不限, mode 0)`（**引擎自己就带「敌弹自身半径 × 0.5」那一条**：
 *     `th18:0x429370` 的命中式 = `dist² ≤ (弹半径(th18:0x658)×0.5 + R)²`，与 TH10 `th10:0x40818d` 的
 *     `fadd` 同义 ⇒ 我们**不需要**自己加 ⇒ 这一条是复刻，不是自定）；
 *   伤害 = 每帧**新撒一个伤害源**（`ce_damage_rect`），数值 = `mof_core_bomb_damage(...)`。
 *
 * ## ★ 伤害为什么是「独立伤害源」而不是「自机弹增伤」（**❓ 自定，必须写清**）
 *   · **TH10 的语义**：灵击**不加**自机弹的倍率，而是往「玩家→敌人」的伤害累加器里**加一笔数**
 *     （`th10:0x428891` 回调 `0x4059f0` → `th10:0x42889a add`），**每敌每帧一次**、**按位置分档**
 *     （圈内/圈外）、且**不开火也照样掉血**。⇒ 它是「每敌每帧的额外伤害」，不是倍率、也不是独立源。
 *   · **TH18 侧两个候选通道**：
 *       ① `bullet_damage_permille`（自机弹伤害层）：**全局连乘、不区分位置、且只在开火时有效**
 *          ⇒ 表达不了「圈内/圈外」、更表达不了「不开火也掉血」（一手最显眼的那半边）。
 *       ② `ce_damage_rect`（`th18:0x45dfa0` 的**独立伤害源**池）：**每帧每敌一笔、按位置（OBB）命中**、
 *          与开火无关 ⇒ **形状上更接近**一手的「每敌每帧一笔额外伤害」。
 *     ⇒ 选 ②。**但它不是复刻**，三处差异逐条登记（`TUNING.md` §2.5）：
 *       (a) 载体不同：TH10 是「加进伤害累加器的一笔」，TH18 是**独立伤害源**（走伤害源池 →
 *           每帧每敌 OBB 命中 ⇒ 与自机弹**共用**每帧上限 `player+0x47984`，TH10 不共用）；
 *       (b) 形状：TH10 的圈是**圆**（`d² < r²`），TH18 的伤害源是**矩形** ⇒ 我们用边长 `2r` 的外接方形，
 *           角落最多多覆盖 41%（消弹那一侧仍是引擎的真圆，不受影响）；
 *       (c) 数值：TH10 的 raw 表经 `/5`（+符卡 `/5`）后是 **1..7/帧**（见 `mof_core_bomb_damage()`），
 *           我们**原样搬**（两作的 boss 阶段 HP 同量级：TH18 `st06bs.ecl` 的非符 2300–3100、
 *           符卡 13300–17800；游玩区同为 384×448）—— 但两作的伤害量纲没有逐条对账 ⇒ 🟡。
 *       (d) 「圈外」那一档在 TH10 是**互斥**的（圈内 38 / 圈外 20），而两个伤害源叠加是**相加**的
 *           ⇒ 我们把内圈源的数值写成 `内圈 − 圈外`，于是「圈内 = 内 + (内−外 = 0 时靠外圈)」……
 *           精确说：**圈内总伤害 = 内圈档、圈外总伤害 = 圈外档**（见 `mof_bomb_tick()` 的实现），
 *           从而**逐条复现互斥分档**（当圈外档为 0 时只撒内圈源）。
 *       (e) 激光：TH10 的 `sub_405ac0` 用**同一个半径**消激光（`th10:0x41c800`）——TH18 侧**没有**
 *           半径消激光的原语（`ce_cancel_all_bullets` 是「全屏消激光」`th18:0x449090`，语义不符）
 *           ⇒ **本卡不消激光**（未做，登记）。
 *       (f) 视觉：TH10 的灵击 VM 自己有贴图（`bullet.anm` script400 的扩散圈），本卡**没有**视觉
 *           （TH18 无对应贴图；要做需要资产层，登记为未做）。
 *   · 频率与寿命：一手 = 「灵击 anm VM 存活期」🟡（脚本 time 累加 296 帧，尾部还有 80×30 帧的计数循环）
 *     ⇒ `MOF_BOMB_FRAMES` = 296（🟡，登记）。*/

/* ============================ 灵击（★ 2026-09-21：实现整段搬进共用件 `cards/bomb10.*`）============
 * 本文原来的 `mof_bomb_start()` / `mof_bomb_tick()` / `mof_bomb_fx_dump()` / `mof_bomb_fx_kill()` /
 * `mof_cancel_impending_death()`（TH10 一手的逐帧状态机）**整段搬进 `cards/bomb10.{h,c}`** ——
 * owner 口径（逐字）：「**110也得是主动卡，因为只要有110 or 111就有灵击，当然，它们完全走同一套
 * 灵击系统**」⇒ 那份状态必须**只有一份**，否则两张卡同装时按 X（库存 == 0）会**各发一发**
 * （双倍扣火力 / 双倍伤害 / 两份环 VM），而且改一处必须记得同步另一处。
 *
 * 搬走的是**公共那一半**：扣火力之后的写 `CURRENT_POWER` + `ce_repopulate_options()`、决死救命、
 * 曲线参数（半径目标 / 上移量 / 角色档）、范围环与电流边 VM、音效、RANK、每帧消弹 + 伤害源 +
 * 环推进 + 火星 + 收尾日志；连带那批常量（`MOF_RANK_*` / `MOF_BOMB_SE` / `MOF_PLAYFIELD_*` /
 * `MOF_BOMB_RING_FADE_AT` / `MOF_BOMB_SPARK_EVERY` / 救命地址）也都搬进 `bomb10.h`（只有一份）。
 *
 * 留在本文件的是**本卡那一半**（四项，逐条见 `bomb10_host_t` 的定义）：
 *   ① `mof_bomb_gate()`        —— 对话 VM 忙 ⇒ 不放（一手 `th10:0x425971`）；
 *   ② `mof_bomb_power_spend()` —— **整档**扣火力（`mof_core_power_spend()`）；
 *   ③ `mof_bomb_on_start()`    —— 记形态 + 信仰 −3000（非决死）+ 作废符卡奖励 + 全避标记 + 诊断 + 日志；
 *   ④ `mof_bomb_on_cancel()`   —— 消弹记账。
 * **门序与改动前逐条相同**：进行中（现在由机器把守）→ 对话门 → 火力门槛；只有这三道，一个都没多。
 * ⚠ 「进行中不放」那一道**不是新闸**：一手 `th10:0x42598d mov eax,[esi+0x28] / test / jne 0x425a24`
 *   （`esi` = `BOMB_PTR`），同时它就是「两张卡同装不叠加」的机制（`bomb10.h` 硬语义 ②）。*/

/* ① 卡私有前置闸 = **对话 VM 忙**（一手 `th10:0x425971 mov ecx,[eax+0x9eb8] / test / jne 0x425a24`，
 * `eax` = `GUI_PTR` `0x47770c`）。`CE_GUI_MSG_ACTIVE()` 是 engine.h 的一手宏（同一个 `+0x9eb8`）。
 * ⚠ 一手还有一道「没有 GUI 就不干」（`th10:0x425964 test eax,eax / je`）—— 在 TH18 里由
 *   `on_tick_2` 的调用条件天然满足（没有 GUI 就没有卡机器），所以这里只判对话那一条。*/
static int32_t mof_bomb_gate(void *user)
{
    (void)user;
    return CE_GUI_MSG_ACTIVE() ? 1 : 0;
}

/* ② 火力扣法 = **整档**（共用件 `mof_core_power_spend()`：≥2.00 档 / 扣 1 档 / 下限 1.05；
 * 门槛与下限是用户裁决的 TH18 侧映射、**不是** TH10 原版，一手差别见 `mof_core.h`）。
 * 「决死形态没发动」那一行日志与改动前**逐字相同**（它原来在 `mof_bomb_start()` 里）。*/
static int32_t mof_bomb_power_spend(void *user, int32_t by_hit, int32_t power_now, int32_t *power_after)
{
    /* ★★★ 2026-09-24（owner：「110 和 111 的灵击应当是完全一套系统，而魔理沙机体 111 会有一些额外
     *   灵击特技」）：**消耗规则与卡号无关** —— 111 在场 + 魔理沙机体时，**谁放的灵击都吃半档**
     *   （原来只有 111 自己放的那一发吃，110 放照样扣整档 ⇒ owner 实跑「试图发动 110 灵击，
     *   并没有以 0.5p 释放」）。判据与 111 侧、与 P 点补值那条（`faith_p_topup_take()`）**同源**：
     *   `faith_registered_mask()` 的 111 位 + `CHARACTER == 魔理沙`。*/
    int alice = ((faith_registered_mask() & (1 << FAITH_SLOT_111)) != 0
                 && *(const volatile int32_t *)CE_ADDR_CHARACTER == FAITH_CHAR_MARISA) ? 1 : 0;
    mof_power_t pw = mof_core_bomb_power_spend(power_now, alice);

    (void)user;
    if (alice) {
        static int s_logged;
        if (!s_logged) {
            s_logged = 1;
            ce_log("mof: 灵击消耗走**半档**（111 在场 + 魔理沙机体；门槛 1.55 档 / 扣 50 单位 / 下限 1.05）"
                   "—— 与 111 放的那一发同一条规则");
        }
    }
    if (!pw.ok) {
        if (by_hit) ce_log("mof: 灵击发动（决死）**没发动**：火力 %d 不够门槛（power_ok=0）⇒ 本次不救命",
                           (int)power_now);
        return 0;
    }
    *power_after = pw.new_power;
    return 1;
}

/* ③ 发动瞬间的卡私有那一半（机器已经把火力扣完、子机重建完、救命叫完、环 VM 起完、音效与 RANK 落完）。
 * 逐条的一手依据（= 搬走前 `mof_bomb_start()` 里那段注释，**理由一字未改**）：
 *   · 信仰 **−3000**（一手 `th10:0x425a15 push 0xbb8 / call 0x412e70`，`FAITH_REDUCE_BOMB`）；
 *     **决死形态扣 0**（一手：决死那一支没有 `call 0x412e70`）—— `faith_slot_reduce()` 对
 *     `FAITH_REDUCE_NONE` 恒返回 0 ⇒ 这里不必分支；
 *   · **作废当前符卡奖励**（一手 `th10:0x405500`，`Bomb::start` 在 `th10:0x405982` **无条件**调它）：
 *     一手的门只有一条 `cmp [spellcard+0x3738],0x3c`（本次攻击已过 ≥60 帧）⇒ 命中则奖励值归零 +
 *     清 bit1（奖励存活位）。TH18 的同位字段 = `CE_SPELL_BONUS`（`th18:0x7c`）与
 *     `CE_SPELL_FLAG_BONUS`（`th18:0x78` bit1，一手 `th18:0x42a7ed` 读它 / `0x42a801 mul [edi+0x7c]` 计分）
 *     ⇒ **同一件事、同一位**（先例 = `cards/judgment.c` 的同一对写）。**这是复刻。**
 *     我们额外要求「符卡进行中」——差别只在「非符攻击里也去清一遍奖励」，而那时奖励本来就是 0
 *     ⇒ **无可观察差异**（更保守：绝不在非符阶段动奖励位）；
 *   · `spell_watch.bombed = 1`（本张符卡期间放过灵击 ⇒ 不算全避，见 `mof_core_spell_watch()`）；
 *   · 诊断计数 `stat_bomb` / `stat_bomb_dbf` + 计分声明（信仰变了，`mof_declare_score()`）。
 * ⚠ 符卡帧数改读 `bomb10_spell_frames()`（同一个一手量 `[spellcard+0x3738]`，现在由机器逐帧推进）——
 *   **与改动前差 1 帧相位**：原来本卡在 ④ 处先自增、再进 ⑤ 发动（含当帧），现在机器在 ⑧ 处推进
 *   （发动机上不含当帧，= 111 一直以来的那个相位）⇒ 「符卡已过 ≥60 帧」这条门的边界整体后移一帧。
 *   影响面 = 作废符卡奖励与 `big_bomb` 伤害档各差一帧，登记在 `AUDIT.md` §BE-EY。*/
static void mof_bomb_on_start(void *user, int32_t by_hit, int32_t cost, int32_t power_before, int32_t power_after)
{
    mof_card_t *s = (mof_card_t *)user;
    int32_t spell_now = ce_spell_active();
    int32_t reimu = (*(const volatile int32_t *)MOF_ADDR_CHARACTER == 0);

    (void)cost;
    if (!s) return;
    mof_bomb_form_start(&s->form, by_hit);               /* 记形态（by_hit + 消弹颗数归零）*/
    ++s->stat_bomb;
    if (by_hit) ++s->stat_bomb_dbf;                      /* 诊断（**救命本身由机器做**）*/
    /* ③ 作废当前符卡奖励（复刻一手 `th10:0x405500`；门 = 本次攻击已过 ≥60 帧）——
     *   ★★★ 2026-09-24 owner 口径：**河童特技 = 按 Bomb 不 bonus failed，除非你真撞了**
     *   ⇒ 罩子形态（111+魔理沙）这一发**跳过**作废；真被弹那一下由 111 的罩子路径调
     *   `bomb10_void_spell_bonus()` 补上。实现搬进 `bomb10.c`（两条路共用一份）。*/
    if (!bomb10_nitori_armed()) bomb10_void_spell_bonus();
    s->spell_watch.bombed = 1;                           /* 本张符卡期间放过灵击 ⇒ 不算全避 */
    faith_slot_reduce(&s->slot, mof_bomb_form_faith_kind(&s->form));
    mof_declare_score(s);
    /* ★ 日志（纯诊断）。**唯一一处措辞变化**：`power 前 -> 后` —— 改动前那一行两个数**都是扣完的值**
     *   （首字段读的是 `CE_CURRENT_POWER()`，而它刚被写过），现在用机器传进来的真值，
     *   与 111 那一行同口径。其余字段逐字未变。*/
    ce_log("mof: 灵击发动%s (power %d -> %d, %s, target r=%.1f, rise=%.2f/f, frames %d, faith %d, 变体 %d)",
           mof_bomb_form_is_deathbomb(&s->form) ? "（决死）" : "",
           (int)power_before, (int)power_after,
           spell_now ? "spell" : (reimu ? "reimu" : "marisa"),
           (double)((float)bomb10_target_centi() * 0.01f),
           (double)((float)mof_core_bomb_rise_milli(bomb10_variant() == 2, reimu) * 0.001f),
           (int)bomb10_left(), (int)faith_slot_total(&s->slot),
           (int)bomb10_variant());
}

/* ④ 每帧消弹的卡私有记账（机器只报「本帧消掉几颗」）。**口径与改动前逐字相同**：
 *   形态账本照记（`mof_bomb_form_cancel_note()` —— 无论入不入账都记），入不入账由
 *   `mof_bomb_form_cancel_accounts()` 回答（决死形态那条 🟡 开关 = `MOF_BOMB_DEATHBOMB_ACCOUNT`，
 *   全论证在 `mof_core.h`）。
 * ⚠ 本卡的 `stat_cancel` 是「**有消弹的帧数**」（`++`），111 那边是「颗数」（`+= n`）——
 *   这个不一致是**既有的**（两边旧注都只写「诊断」），本次**不合并**：机器只把颗数原样转给卡。*/
static void mof_bomb_on_cancel(void *user, int32_t bullets)
{
    mof_card_t *s = (mof_card_t *)user;

    if (!s || bullets <= 0) return;
    mof_bomb_form_cancel_note(&s->form, bullets);
    if (!mof_bomb_form_cancel_accounts(&s->form)) ++s->stat_cancel;
}

/* 本卡交给共用机器的那四项（静态常量表；机器不认识卡，卡也不持有那份状态）*/
static const bomb10_host_t s_bomb_host = {
    .card_id     = MOF_ID_NUM,
    .log_tag     = "mof",
    .gate        = mof_bomb_gate,
    .power_spend = mof_bomb_power_spend,
    .on_start    = mof_bomb_on_start,
    .on_cancel   = mof_bomb_on_cancel,
};

/* ---- 每帧 ---- */
/* ★★ 2026-09-18（用户实跑）：**中盘拿到卡（卡 118 的十二合一）时 `on_load` 不会被调** ——
 *   一手：`ce_give_card(mode 0/2)` 只走 `ctor`（trace 日志里只有 `ctor (+0x00)` / `on_hud_anm (+0x44)`，
 *   没有 `on_load`）⇒ `faith_register()` 与 `faith_hud_on_load()` 都没跑 ⇒ **共享池没人认领、
 *   组合 UI 不出现**，要等下一次换面（`on_load`）才补上（用户原话：「是切换面数重置才出现的」）。
 *   ⇒ 每帧补一次「注册 + 认领」：两者都幂等（`faith_register` 同 id 覆盖、`faith_hud_on_load`
 *     只在换关时重置淡入），代价 = 一个 ≤ 4 项的循环 + 一次关卡号比较。*/
static void mof_ensure_registered(ce_card_t *c);

static int on_tick_2(ce_card_t *c)
{
    mof_card_t *s = st(c);
    int32_t g, dg;
    int lives, spell_now;

    if (!s || !CE_PLAYER()) return 0;
    ++s->frame;
    mof_ensure_registered(c);       /* ★ 中盘拿卡（give12）时 on_load 不会被调 ⇒ 每帧补一次（幂等）*/
    /* ★ 2026-09-24：换皮驱动已搬进共享件（`faith_green_swap_tick()`，由 `faith_pool_tick()`
     *   每帧调一次）⇒ 本卡不再自己驱动；这里什么都不用做（装 110 **或** 111 都生效）。*/               /* ★★ 2026-09-18：变绿（换 sprite 表项；按关收窄、幂等、可还原）*/
    /* ★★ 2026-09-18b / 2026-09-22：绿妖精载体（原版妖精不动）。★ 2026-09-22 起实现搬进共用件
     *   `cards/faith.c` 的 `faith_green_fairy_tick()` —— 111 单装时也要有它（th11 #4 的根因是
     *   「载体只被 110 调」）⇒ 不能再只挂在 110 这一边。
     *   ⚠ 调用位置**一字未变**（仍是 `on_tick_2` 一开头、`mof_green_tick()` 之后）⇒ 110 行为不变；
     *   函数自带帧守卫 ⇒ 110+111 同装时一帧只跑一遍（谁先跑到谁执行）。*/
    faith_green_fairy_tick();

    /* ① 擦弹 → 擦弹倍率（§7.5：`min(graze × 0.0001, 8.99)`）
     * ★ 2026-09-12 一手订正（任务 ③）：**这是 th11（SA）的式子，TH10 没有擦弹倍率**
     *   （`research-th10-faith-and-graze.md` §1.1：`0.0001f` 只作 `fpatan` 的 epsilon、
     *   全 exe 无 `8.99f`、13 个计分口无一处读擦弹；§6 更进一步：TH10 **连擦弹计数器都不存在**）。
     * ⇒ 这里算出来的值**只服务「110 与 111 同装」那一支**（那时乘数 = 擦弹倍率 + 111 声明进来的
     *   信号强度，是契约 §7.4 的式子）；**110 单装时它整项不参与**
     *   （`mof_core_pv()` 的「只装 110」那一支 = `aggregate`，单测钉着：`test_mof_core.c` 的
     *   `mof_core_pv(1, 0, 55000, 8990, 130000) == 55000`）。
     * ★ 2026-09-15 核对（本轮）：`s->graze_mult` 在**本文件里唯一的读点**就是下面⑧的诊断日志
     *   ⇒ 它**不参与任何计分/PRV**（计分走 SDK 的耦合层，见 `on_item_score()`）。
     *   实跑对账时看日志里的这条：它代表**111 那一半**，不是 110 的机制。*/
    g = mof_graze();
    if (s->last_graze < 0 || g < s->last_graze) s->last_graze = g;
    dg = g - s->last_graze;
    s->last_graze = g;
    s->graze_mult = mof_core_graze_mult(g);
    if (dg > 0) faith_combo_add(&s->slot, FAITH_EV_MICRO);   /* ❓ 口径待核：擦弹是否续连击（见报告）*/

    /* ② 连击槽推进 + ★★ **逐帧信仰衰减**（2026-09-15 **装回**；2026-09-12 那次删除是误判）。
     *
     * **误删的原因（必须留痕，否则会再删一次）**：当时的结论是「TH10 没有逐帧信仰衰减」，
     *   依据是对**绝对地址** `0x474c4c` 的写入点穷举 —— 而真正的衰减全在 `eax`（= `0x474c40`）
     *   **相对寻址**里（`[eax+0xc]` / `[eax+0x10]` / `[eax+0x18]`）⇒ grep 抓不到 ⇒ 被误判成「没有」。
     *   一手见 `local/_scratch/research-th10-faith-units-and-drain.md` §3 / §5 第 9 条。
     *
     * **门（`th10:0x418740`–`0x41875d`，三条全满足才走衰减；任一不满足跳到 skip）**：
     *   ① 无 Boss   `th10:0x418746` `[ENEMY_MANAGER+0x10] == 0`（那个字段是**当前 boss 指针**，
     *      本轮逐条复核：`th10:0x414adc` 拿它读符卡计时画奖励条、`th10:0x41217b` 拿它读 `+0x1068` 坐标）
     *   ② 无对话   `th10:0x418751` `[GUI+0x9eb8] == 0`      → `CE_GUI_MSG_ACTIVE()`
     *   ③ 关卡帧 ≥ 90 `th10:0x418759` `[g_GameThread+0x14] >= 0x5a` → `mof_stage_frames()`（🟡 **自定**，
     *      见那个函数的注释：TH18 的 `zGameThread` 字段图未反 ⇒ 用卡自己的按帧计数）
     * **用户口径（2026-09-15）**把「连击槽为空」也点名为前提 —— 那一条**已经在**本函数里：
     *   `mof_core_faith_tick()` 的分叉（一手 `th10:0x418a06 jle`）= 「槽 > 0 只 −1、槽 ≤ 0 才扣信仰」，
     *   所以「槽空」是**分叉**不是门；`enemies_present` 只用来**选速率档**（无敌人 10800/s、有敌人 600/s）。
     * ★★ 2026-09-18 **结案**：两档都在一手里，判据 = **这一帧有没有击破**（`gate.killed_since_last`）——
     *   一手 `th10:0x40e26b`（敌机死亡路径）把速率字段写 1，而衰减帧 `0x418a26 sub ecx,edx` 用的是
     *   **读到的原值**、读完才抬到 18 ⇒ 有击破扣 1 存储（−10 显示）、没击破扣 18（−180 显示）。
     *   详见 `mof_core.h` 的「衰减速率：两档，判据 = 这一帧有没有击破」。*/
    {
        mof_faith_gate_t gate;
        gate.no_boss         = !mof_boss_alive();       /* ①  */
        gate.no_dialogue     = !CE_GUI_MSG_ACTIVE();    /* ②  */
        gate.stage_frames_ok = (mof_stage_frames(s) >= MOF_STAGE_FRAME_GATE);   /* ③ 🟡 自定源 */
        gate.enemies_present = mof_enemy_present();     /* 只作诊断（日志 `near=`）—— 不选档 */
        gate.killed_since_last = s_mof_killed;          /* ★★ 一手的真判据：这一帧有没有击破 */
        s_mof_killed = 0;                               /* 读走即清（下一帧重新判定）*/
        /* ★★ 2026-09-15f：**连击槽的前后值**（用户实跑「击破条的衰减速度肯定有问题」的对账依据）——
         *   一手是「槽 > 0 ⇒ 每帧只 −1」（`th10:0x418a10`），所以**在没有道具补槽的帧里**，
         *   `combo_before - combo_after` 应当恰好 = 1（门全成立）或 = 0（任一不成立 ⇒ 冻结）。
         *   日志把它打进 `mof/faith:` 那行（每 600 帧），见下面 ⑨。*/
        s->combo_before = s->slot.combo;
        s->drain_lost += mof_core_faith_tick(&s->slot, &gate, 1);
        s->combo_after = s->slot.combo;
        /* 诊断用的门快照（每 600 帧那条日志要打「门/boss/关卡帧」）*/
        s->gate_no_boss = gate.no_boss;
        s->gate_no_dlg  = gate.no_dialogue;
        s->gate_frames  = gate.stage_frames_ok;
        s->gate_enemies = gate.killed_since_last;       /* 日志 `near=` 改打「本帧有没有击破」（1/0）*/
    }

    /* ③ miss 检测（命数减少）→ `−(信仰−50000)/3`（§7.5）。
     * ⚠ 引擎没有「miss」回调 ⇒ 这里用**命数**差分（比 state 机可靠：决死救回不改命数）。*/
    lives = CE_CURRENT_LIVES();
    if (s->last_lives >= 0 && lives < s->last_lives) {
        faith_slot_reduce(&s->slot, FAITH_REDUCE_MISS);
        faith_combo_set(&s->slot, 0);                    /* miss 清连击槽（§2.3）*/
        ++s->stat_miss;
        mof_declare_score(s);
    }
    s->last_lives = lives;

    /* ④ 符卡监视（★ 2026-09-15 新增）
     * `spell_now` 是「符卡进行中」（`[spellcard+0x378c] bit0`，一手 `th10:0x405798` 那一支用的同一字段）。
     * **「本次符卡已过帧数」这个计数搬走了**（2026-09-21，`s->spell_frames` 已删）：它是一手
     *   `[spellcard+0x3738]`（**本次攻击已过帧数**）的对位，一手有两处读它 ——
     *   · `bomb+0x44` = 1 iff（符卡中 且 该计数 ≥ 60）（`th10:0x4059ca`）⇒ 内圈 3 vs 38 等分档；
     *   · 作废符卡奖励的门（`th10:0x405500` 的第一条 `cmp [eax+0x3738],0x3c`）。
     *   两处**都是那一发灵击的自变量** ⇒ 与那一发的状态同住（`bomb10_spell_frames()`，
     *   由共用机器在每帧的灵击钩子里推进；本卡在 `mof_bomb_on_start()` 里读它）。
     * ⚠ 一手那个计数器是**引擎的字段**（按攻击走）；我们只能自己数「连续 active 的帧数」——两者在
     *   「符卡刚开始就炸弹」与「符卡中途炸弹」这两种情形下等价（差异只可能出现在引擎把计时器
     *   提前重置的怪情况）⇒ 记为近似，登记 `TUNING.md` §2.5。
     *
     * 「全避 −30000」的判据（§2.2 / §7.5，★ 2026-09-12 审计修）：= 这一张**拖到超时**（没打掉、时间到了）
     *   **且本卡没在这一张里放过灵击** ⇒ 才扣；**打掉的（收了符卡）不扣**。
     *   · 超时 = `CE_SPELL_FLAG_TIMED_OUT`（`0x80`）——引擎在 `0x42efc0` 置位；**耐久符卡不置**
     *     （那条走 `0x42efb3 test cl,0x8` 的「按收符卡算」）⇒ 「时符必减 / 耐久按收符卡算」自动成立；
     *   · 只在 active 的**下降沿**结算一次；
     *   · 状态机在 `mof_core_spell_watch()`（`mof_core.c`）——**故意搬进 core**：卡侧代码在主机上
     *     编不出来（`sdk.h` 的 inline asm）⇒ 留在 tick 里一条断言都写不了，而这次修的正是这段判据。
     *   ⚠ 旧实现只判「没放灵击」⇒ **每张符卡正常收掉也扣 30000**（一局 boss 战就能把 Δ_110
     *     打到下限 0、信仰永久钉在 50000）。这是审计发现并修掉的行为差异。*/
    spell_now = ce_spell_active();
    if (mof_core_spell_watch(&s->spell_watch, spell_now,
                             (ce_spell_flags() & CE_SPELL_FLAG_TIMED_OUT) != 0)) {
        faith_slot_reduce(&s->slot, FAITH_REDUCE_AVOID);
        ++s->spell_avoided;
        mof_declare_score(s);
    }

    /* ⑤ 灵击（库存 == 0 且炸弹键上升沿）→ 发动；然后**每帧推进**已经发动的那一发。
     * ★★ 2026-09-21：实现是**共用件** `bomb10_start()` / `bomb10_tick()`（110 与 111 的**同一套**，
     *   见本文件上面「灵击」那一段与本卡那四项 `s_bomb_host`）。**X 路径的触发条件一字未动。**
     * ★ 2026-09-15c：**决死窗口内**按 B ⇒ 同一支路走**决死形态**（发动 + 救命 + 不扣信仰）。
     *   窗口判据 = `player+0x476ac == 4`（一手 `th18:0x45d4e4`，全 exe 唯一写 4 的点），
     *   判据本体在 core（`mof_core_deathbomb_window()`）⇒ 主机单测钉得住；
     *   卡侧在这里只做「检测窗口 → 传参」这一件事（本仓硬要求：状态/语义进 core）。
     * ⚠ **不做**别的救人事（不挂 `+0x10`）—— 见上面「决死形态」那段与 `EVENTS-CONTRACT.md` §5.2。
     * ⚠ 推进那一行传的 `driver` = 「**本卡负责驱动这一帧**」：两张同装时那一发状态只有一份，
     *   只能有一张来推（否则一发灵击每帧前进两帧：半径 / 伤害 / 时长全错）。规则 = **id 小的那张推**
     *   （先例 = 低速子机声明层「id 小者落笔」（108 < 111）、组合 UI「110 优先」）
     *   ⇒ **110 恒推**（它是两张里 id 小的那张；111 那边只在 `!ce_owned(110)` 时才推）。
     *   ⚠ 别写成「另一张在场就让位」—— 两边都让位 = 谁都不推（这一发彻底不动）。*/
    if (CE_CURRENT_BOMBS() <= 0 && (*(uint32_t *)CE_ADDR_INPUT_PRESSED & 0x002u)) {
        uint8_t *pl = CE_PLAYER();
        int32_t pstate = pl ? *(int32_t *)(pl + CE_PLAYER_STATE) : -1;
        int32_t by_hit = mof_core_deathbomb_window(pstate);
        if (by_hit)
            ce_log("mof: 决死窗口内按 B（state %d == 4，库存 0）⇒ 本发走决死形态", (int)pstate);
        (void)bomb10_start(&s_bomb_host, s, by_hit);
    }
    bomb10_tick(1);                 /* 110 = id 小的那张 ⇒ 恒推（111 在场时由它让位，见上）*/

    /* ⑥ 满 P 全场转换（★★ 2026-09-15，A + B 的主路；**必须在池推进之前**）——
     *    满 P 时把原生 P 类道具原位删掉、在它自己的位置撒一颗 type 9 档的点（角 −π/2、速 2.2）。
     *    一手 `th10:0x41bbf8`（出生即改写 type）+ `th10:0x41ba50`（满的那一帧全场原位重生），
     *    全文与「为什么落点不是改 type」见 `faith.h` 的 `faith_convert_full_p_items()`。
     *    ⚠ 顺序：放在 `faith_pool_tick()` **之前** ⇒ 本帧转换出来的点当帧就建 VM、就受物理/收集判定
     *    （用户验收「线上生成即被吃 / 满 P 那刻原地变绿点」都靠这个顺序）。*/
    faith_convert_full_p_items();

    /* ⑦ 绿点池推进（共用件：撒点 / 吸附 / 收集 → 回调 `mof_on_pickup`）*/
    faith_pool_tick();

    /* ⑧ 卡内奖残（§2.6：20m / 40m / 80m / 150m → `ce_add_life()`）*/
    {
        int n = mof_bonus_take(&s->bonus);
        while (n-- > 0) {
            ce_add_life();
            ce_log("mof: bonus threshold reached (%d/%d) -> +1 life (bonus %d)",
                   (int)mof_bonus_awarded(&s->bonus), MOF_BONUS_MAX, (int)s->bonus.bonus);
        }
    }

    /* ⑨ 诊断汇总 */
    if (s->frame - s->log_frame >= MOF_LOG_EVERY) {
        s->log_frame = s->frame;
        /* ★ 「收到 / 撒出」两两并排（照 `cards/signal.c:437-438` 的口径）：
         *   big/small = 玩家真吃到的，bigSp/smallSp = 撒出去的。实跑时前者 ≤ 后者；
         *   满 P 吃一颗 P 点 ⇒ `smallSp +1`，吃到后 `small +1`（**各一次**，不再像旧实现那样一颗点两边都 +1）。
         *   ★ 2026-09-15 收口后：满 P 的这一颗**主路**是「原生 P 道具被删 + 池里原地撒点」
         *   （共用件那条 `faith: 满 P 转换 N 颗…` 的日志），本行只在**同帧竞态**里才会多一次 `smallSp`。
         * ⚠ `x%d` 那项 = **th11 的擦弹倍率**（110 单装时不参与 PV，见 `on_item_score` 的注释）。
         * ★ `Ptop` = **本卡落笔的 P 点补差颗数**（本卡在场即由它落笔，见 `on_item_commit`）：
         *   它与「吃到的 P 点颗数」对照着看 —— 本卡在场时 `Ptop` 应当**颗颗不落**（每颗 +4 单位）；
         *   111 单装时这里是 0（那颗由 111 落笔，见 `cards/signal.c` 的同一行）。*/
        ce_log("mof: faith %d (Δ %d), combo %d, graze %d (th11 x%d), bonus %d, "
               "big %d/%d small %d/%d (got/spawn), Ptop %d, cancel %d miss %d bomb %d (dbf %d) avoid %d, "
               "bomb_left %d (target r %.0f), comboIn %d (pickup %d + item %d + kill %d + cancel %d)",
               (int)faith_slot_total(&s->slot), (int)(s->slot.delta / FAITH_FIXED),
               (int)s->slot.combo, (int)s->last_graze, (int)s->graze_mult,
               (int)s->bonus.bonus,
               (int)s->stat_big, (int)s->stat_big_spawn,
               (int)s->stat_small, (int)s->stat_small_spawn,
               (int)s->stat_p_topup,
               /* ★ 2026-09-15c：`dbf` = **决死形态**发动的次数（`bomb − dbf` = 主动次数）； */
               (int)s->stat_cancel, (int)s->stat_miss, (int)s->stat_bomb, (int)s->stat_bomb_dbf,
               (int)s->spell_avoided,
               (int)bomb10_left(), (double)((float)bomb10_target_centi() * 0.01f),
               /* ★ 2026-09-16：「连击槽**进账**」的四条通道对账（未夹取；某一项恒 0 ⇒ 那条断了）。
                * `pickup` = **自建信仰点**被收（大 +120 / 小 +60，`mof_core_pickup_combo()`）——
                *   用 `stat_big/stat_small`×值**算**出来，不另开累计字段（省 `ce_state` 的 496 字节上限）；
                * `cancel` = 消弹微档每颗 +3（`FAITH_EV_MICRO`）同理由 `stat_cancel`×3 算；
                * `item` / `kill` = 本轮新接的引擎道具与击破（各自有累计字段）。*/
               (int)(120 * s->stat_big + 60 * s->stat_small
                     + s->stat_combo_item + s->stat_combo_kill + 3 * s->stat_cancel),
               (int)(120 * s->stat_big + 60 * s->stat_small),
               (int)s->stat_combo_item, (int)s->stat_combo_kill,
               (int)(3 * s->stat_cancel));
        /* ★ 决死形态的「不记账」那一档要看得见：本发的颗数记在 `form.cancels`（诊断，不入数值）。*/
        if (bomb10_busy() && mof_bomb_form_is_deathbomb(&s->form))
            ce_log("mof: 决死形态进行中（已消 %d 颗，本形态入账=%d）",
                   (int)s->form.cancels, (int)mof_bomb_form_cancel_accounts(&s->form));
        if (s->stat_spawn_failed)
            ce_log("mof: %d faith point(s) dropped (pool full)", (int)s->stat_spawn_failed);
        /* ★★ 2026-09-15 逐帧衰减的对账行（`TUNING.md` §2.5h 的「怎么钉」就靠它）。
         *   `gate=` 三位依次 = **无Boss / 无对话 / 关卡帧≥90**（1 = 该门成立）；
         *   `boss` = `mof_boss_alive()` 的原值、`sf` = 本卡自数的关卡帧（🟡 自定源）、
         *   `near` = 场上敌人计数是否 > 0（**只选速率档**）、`rate` = 本段实际用的档（定点/帧）、
         *   `drain` = 本段（600 帧）累计被逐帧衰减扣掉的**显示**单位。
         *   对账口径：`drain / 600 × 60` 应当 ≈ 10800（无敌人档）或 600（有敌人档），
         *   且 miss / 灵击 / 关卡结算**都不算在 `drain` 里**（那些走 `faith_slot_reduce`，另有日志行）。*/
        ce_log("mof/faith: gate=%d%d%d boss=%d dlg=%d sf=%d/%d near=%d rate=%d drain=%d (display, %d frames) combo=%d",
               s->gate_no_boss ? 1 : 0, s->gate_no_dlg ? 1 : 0, s->gate_frames ? 1 : 0,
               mof_boss_alive(), CE_GUI_MSG_ACTIVE() ? 1 : 0,
               (int)s->stage_frames, (int)MOF_STAGE_FRAME_GATE, s->gate_enemies ? 1 : 0,
               (int)mof_faith_rate_fixed(s->gate_enemies),
               (int)(s->drain_lost / FAITH_FIXED),
               (int)MOF_LOG_EVERY, (int)s->slot.combo);
        /* ★★ 2026-09-15f（用户「击破条衰减速度肯定有问题」的对账行）：把**本帧连击槽的前后值**与
         *   「这一帧门全不全」并排打出来。判读口径（一手 `th10:0x418a10`「每帧 −1」）：
         *     · `cstep=1`（`before − after` = 1）= 门全成立、照一手 −1 ✓ 正常；
         *     · `cstep=0` = 三条门任一不成立（boss 战 / 对话中 / 关卡前 90 帧）⇒ **槽冻结** ✓ 正常；
         *     · `cstep>1` = 有别的路径在减（如 miss 清槽 `faith_combo_set(0)`、或批量补跑）⇒ 要查；
         *     · `cstep<0` = 本帧有**补槽**（吃道具）比 −1 多 ⇒ 正常（补槽在 `mof_on_pickup`）。
         *   ⚠ 这条只覆盖 `on_tick_2` 的**最后一帧**（不是 600 帧的累计）—— 它是「稳态取一帧」的抽样；
         *     要看累计请对照 600 帧的 `drain`（信仰）与 combo 的绝对值。*/
        ce_log("mof/combo: before=%d after=%d cstep=%d gates=%d%d%d (boss=%d dlg=%d sfOK=%d) → HUD 行3 画的是 %s",
               (int)s->combo_before, (int)s->combo_after,
               (int)(s->combo_before - s->combo_after),
               s->gate_no_boss ? 1 : 0, s->gate_no_dlg ? 1 : 0, s->gate_frames ? 1 : 0,
               mof_boss_alive(), CE_GUI_MSG_ACTIVE() ? 1 : 0, s->gate_frames ? 1 : 0,
               (faith_registered_mask() & (1 << FAITH_SLOT_110)) ? "真值（110 在场）" : "中性满格（110 不在场）");
        s->drain_lost = 0;
    }

    /* ⑩ ★★ 2026-09-15：向**共享的组合 UI**声明本卡这一槽的读数（`cards/faith_hud.{h,c}`）。
     *   为什么要「声明」而不是让那块 UI 读本卡状态：卡之间**不许读对方状态**（`EVENTS-CONTRACT.md` §3），
     *   而这块 UI 两卡共用、行 2 要画 `Δ_10 + Δ_11`（用户 ④）⇒ 只能由**各自**把自己那一份交出来，
     *   由共享件相加（与计分层的「声明 → 聚合」是同一个形状）。
     *   ⚠ 交出去的是**本槽的三个数**（Δ 定点 / 连击槽 / 信号槽），**不含基础值** ——
     *     「基础不进这块 UI」是用户 ④ 的原话（别把 `faith_slot_total()` 那种「基础 + Δ」送进来）。
     *   ⚠ 它**不代表在场**：在场判据是 `faith_registered_mask()`（`faith.h`）⇒ 本卡被删掉之后，
     *     共享件立刻改走「只有 111」的显示（连击条填满），而**不是**因为「连击槽恰好是 0」——
     *     用户明令不许拿槽值当判据。*/
    {
        faith_hud_slot_t v;

        v.delta_fixed = s->slot.delta;
        v.combo       = s->slot.combo;      /* 连击槽（TH10 的 zTimer 对位，0..130）—— 行 3 */
        v.signal      = 0;                  /* 110 侧**没有**信号槽（th11 的量），行 1 由 111 声明 */
        faith_hud_declare(FAITH_SLOT_110, &v);
    }
    s_slot_keep = s->slot;   /* ★ 2026-09-16r：账本同步进静态（跨关 / 卡对象重建都不丢）*/
    return 0;
}

/* ============================ HUD（★ 2026-09-15 已搬到共享件）============================
 *
 * 这一段原来就是**「原生那一整块」**的驱动：9 支 VM（图标 1 + 6 位数字 + 连击条 2），
 * 几何全在 TH10 一手（`front.anm` script51..57 + `Gui::on_draw` th10:0x4158b6 的两个四边形），
 * 层 25（`resolutionMode(2)` ⇒ 组 38 ⇒ 被暂停模糊罩盖住 + 进 P 键截图）。
 * 现在它连同 `mof_hud_spawn/warn_once/alive/dual_color/kill/digit_of/dim_tick/draw` 一起**删掉**，
 * 由 `cards/faith_hud.{h,c}` 接管 —— 因为用户 2026-09-15 口径 ①② 把 110 与 111 的两块 UI
 * 并成了**一块**（一个蓝点 + 信号强度槽 / 最大得点×信号强度 / 连击槽），共用块只能有一个持有者。
 *
 * 逐项去向（要改这块 UI 请直接改 `faith_hud.c`，别再往本文件加）：
 *   · 图标 / 数字位 / 连击条的位置与缩放 ⇒ `faith_hud.c` 的 `FH_*` 常量块 + `fh_place_*()`；
 *   · 两路顶点色镜像 ⇒ 共享件对**全部 16 支**每帧 `ce_anm_mirror_color()`（那里有完整的一手注）；
 *   · 整块压暗 / 淡回 ⇒ `fh_dim_tick()`（❓ 用户口径 = UI 统一：**门限每帧由本块矩形推**
 *     （进 = 自机落在块内 / 出 = 出到外扩 16 px 之外）+ 终点 32 + C 每帧 ±56（4 帧）；TH10 那套
 *     416/400 与 −128/−112、TH11 那套 416/400 与 −64 都是**各自块的沿**，已不再被使用、
 *     只在脚本注释里留档 —— 详见 `faith_hud.c` 常量块的长注与 `TUNING.md` §2.6f-3）；
 *   · 收尾 ⇒ `fh_kill()`，由 `faith_hud_departed()` 调（`on_delete` / `on_run_reset` 都走它）。
 *
 * ⚠ 仍然有效、值得记住的两条（合并块照旧受它们约束）：
 *   ① **层只能是 24/25**（`HUD-LAYOUT.md` §4.1 的硬规则：别的层折不到模糊罩那一组）；
 *   ② 连击条原生**不是 anm 精灵**（引擎直画 D3D 四边形 `0x43bda0`）⇒ 我们只能用 `drawRect`
 *      脚本近似（`❓ 自定（载体差异）`）；**alpha 借最左数字 VM 的 `color_1.A`** 那条一手
 *      （`Gui+0x8393`）在共享件里**照抄了**（`fh_place_combo()` 还顺带把影条画成黑色 ——
 *      旧实现在这里两条都是白的）。*/

static int on_draw(ce_card_t *c)
{
    /* ★★ 2026-09-15：整块 HUD 搬到共享件（`cards/faith_hud.{h,c}`）⇒ 本卡只剩这一行转发。
     * 三件原来由本卡自己做的事现在都在共享件里（行为一字不改）：
     *   ① **菜单 / 标题门**（`on_draw` 虚表 `+0x48` 在**主菜单 / 标题**里也会被调，而 `on_tick_2`
     *      不会 ⇒ 不设门就会把整块 HUD 画到菜单上；门关着时要把 VM 收掉、别只是不画）——
     *      这是 `faith_hud_draw()` 的第一句（117 的 bug3 就是这个坑，115/116/117 同款门）；
     *   ② **一帧只画一遍** —— 110 与 111 都会调它（共享件用 `ce_sdk_frame()` 做帧守卫；
     *      16 支 VM 是**共享件的静态状态**，只有一份 ⇒ 两边不会各自 spawn 一套）；
     *   ③ 起 VM / 摆位 / 上色 / 压暗 / 回收 —— 全在共享件。
     * ⚠ 本函数**不吃 `c`**（在场判据 = `faith_registered_mask()`，读数来自 `faith_hud_declare()`）
     *   ⇒ 不留未使用的局部变量。HUD 的**槽位**账也归共享件（认领在 `faith_hud_on_load()`）。*/
    (void)c;
    faith_hud_draw();
    return 0;
}

/* ---- 生命周期 ---- */
static void mof_clear(ce_card_t *c)
{
    mof_card_t *s = st(c);

    /* ★ 顺序要紧：先退出计分层（否则删掉的卡还在往聚合里声明），再收实体与 HUD。*/
    ce_score_declare_coupled(MOF_ID_NUM, 0);
    faith_unregister(MOF_ID_NUM);
    /* ★ 只收**自己槽**的绿点（`1 << FAITH_SLOT_110`）。这里原来是 `faith_clear_entities()`
     * （清整个共用池）⇒ 110 在商店里被倒吊人删掉时，**111 同行还飘着的绿点会跟着凭空消失**
     * （两张卡共用一个实体池，`faith.h` 头注）。按 `slot_mask` 筛的那一支是 2026-09-12 审计补的，
     * 下面这一行就是「删卡路径漏在哪」的补丁。*/
    faith_clear_entities_mask(1 << FAITH_SLOT_110);
    /* ★★ 2026-09-15：HUD 搬到共享件 ⇒ 收尾从 `mof_hud_kill()`（本卡那 9 支 VM）换成
     * `faith_hud_departed()`（共享件那 16 支 VM）。
     * ⚠ **顺序是硬的**：必须在 `faith_unregister(MOF_ID_NUM)`（上面那一行）**之后** —— 共享件的
     *   「还有没有卡在场」判据就是那张注册表（`faith_registered_mask()`）⇒ 先退注册它才知道
     *   「110 走了」；若 111 还在场，它**只清 110 那一槽的声明**、块与 VM 一个都不动（用户 ③
     *   「只有 111 也显示这一套 UI，只是对应的那部分不起作用」）。
     * ⚠ 这里**必须**收干净：删卡（倒吊人 / 原版空白卡）发生在商店里，那一刻 `on_draw` 不会再来
     *   —— 只有 `on_delete`（虚表 `+0x50`）跑得到；最后一张卡走时共享件在同一支里收 VM + 放占位。*/
    faith_hud_departed();
    /* ★ 2026-09-15：P 点火力那一层的「每颗道具闩」也退掉。它是**同一颗道具内**的瞬时状态
     * （`on_item_collect` 置、`on_item_commit` 清），不是跨帧效果；这里清只是为了
     * 「删卡后又以同一个 id 回来」时状态绝对干净（ctor 也清一次，两条路都堵）。*/
    s_p_item = 0;
    /* ★★ 2026-09-18：变绿也要在这里交代（`METHOD.md` 铁律二）—— 它改的是**引擎内存里的
     * sprite 表**，是彻头彻尾的「跨帧留下的东西」；`on_run_reset` 与 `on_delete` 都走本函数。
     * ⚠ 放在 `if (!s) return;` **之前**：还原不依赖卡私有状态（备份在文件静态区）。*/
    faith_green_restore_all();
    ce_fairy_claim_clear(110);      /* ★ 护栏位也要收干净（删卡/换局）*/
    /* ★★ 2026-09-22（`METHOD.md` 铁律二，th10 #2 的 ride-along）：**场上那几只补刷妖精也要收掉**——
     *   它们的位置由共用件每帧写 `%F2/%F3`、离场由它写 `$I0 = 999`，两条通道都只在 `on_tick_2` 里；
     *   删卡发生在**商店**里（那一刻 tick 全停）⇒ 不收就会钉在关卡里到本局结束。
     *   ⚠ 实现已搬进共用件（`faith_green_fairy_depart_all()`，幂等）—— **110 与 111 都要调它**
     *     （谁被删掉，那一侧的 tick 就没了；两卡同装时先跑到的那个把表清空、后一个什么都不做）。
     *   ⚠ 放在 `if (!s) return;` 之前：收尾不依赖卡私有状态（表在共用件的静态区）。*/
    faith_green_fairy_depart_all();
    if (!s) return;
    /* ★★ 2026-09-16k / 2026-09-21：灵击的**范围环 + 电流边**要先删掉再 `memset` —— 它们的 VM 是
     *   **跨帧**留下的东西（脚本 279/293 自己不会死）⇒ 不显式删就会「删了卡还挂一圈在场上」。
     *   火星不用管（一次性 VM，脚本自己 `wait` + `delete()`）。
     *   ⚠ 现在收的是**共用件** `bomb10_reset()`：那一发状态是 110/111 **共用的一份** ⇒ 本卡被删掉时
     *     111 发的那一发也必须一起收（否则那两支环 VM 永远留在场上）。反之亦然。*/
    bomb10_reset();
    /* ★ 2026-09-15（灵击改成跨帧之后）自查一遍「删卡会留下什么」（`METHOD.md` 铁律二）：
     *   · 那一发的状态（剩余帧数 / 中心 / 目标半径）**随 `bomb10_reset()` 一起清零** ⇒ 下一帧不再
     *     续撒消弹圈与伤害源；删卡发生在商店里、`on_tick_2` 本来也不再跑，两条路都断得干净。
     *   · **伤害源**是引擎池（`player+0x20574`）里的对象、寿命只有 `BOMB10_DMG_SRC_LIFE` 帧 ⇒
     *     最多再存活 1–2 帧自己回收。引擎**没有**「按键删一条伤害源」的接口，而按槽位去清
     *     `flags` 会在池槽已被别人复用时误伤别人的源 ⇒ **故意不碰它**（残留 ≤2 帧、无副作用）。
     *   · 绿点实体 / HUD VM 在上面几行收；计分声明在上面退。*/
    memset(s, 0, sizeof *s);
    s->last_graze = -1;
    s->last_lives = -1;
    /* ★ 2026-09-15：门③的两个「参照帧」也要复位成 -1（0 是**合法值**⇒ 不复位就会把
     * 重新装卡后的第一帧误判成「没换关」而带着旧计数走）。`mof_stage_frames()` 首帧见 -1 ⇒ 归零。*/
    s->stage_frames_ref = -1;
    s->stage_id_ref = -1;
}

/* ★★ 2026-09-18：把「注册共享池 + 认领组合 UI 块」抽出来 —— `on_load` 与 `on_tick_2` 都调它。
 *   为什么每帧也要调：**中盘拿到卡**（卡 118 的十二合一）时引擎**只调 `ctor`**、不调 `on_load`
 *   （一手：trace 日志里只有 `ctor (+0x00)` / `on_hud_anm (+0x44)`）⇒ 注册与认领都没发生
 *   ⇒ 共享池没有消费者、组合 UI 不出现，要等下一次换面才补上（用户实跑原话）。两者都幂等。*/
static void mof_ensure_registered(ce_card_t *c)
{
    mof_card_t *s = st(c);
    faith_user_t u;

    if (!s) return;
    memset(&u, 0, sizeof u);
    u.card_id = MOF_ID_NUM;
    u.slot_mask = 1 << FAITH_SLOT_110;       /* 只消费 110 槽 */
    /* ★ 2026-09-15：改走**带位置**的收集回调（`on_pickup_at`）—— 弹窗要画在道具自己的坐标上
     * （一手 `th10:0x41b360`）。`on_pickup` 仍设着（**兜底**：万一 `on_pickup_at` 那一支将来被
     * 摘掉/改名，faith.c 的默认路径还能把账记上；faith.c 的语义是**二选一**，不会双计）。*/
    u.on_pickup = mof_on_pickup;
    u.on_pickup_at = mof_on_pickup_at;
    u.on_cancel = mof_on_cancel;
    /* ★★ 2026-09-15：满 P 转换通道的单颗值（主路 `faith_convert_full_p_items()` 用它）——
     * 110 的小信仰点 = 100（`FAITH_MOF_SMALL`）。不设（0）= 不参与转换。*/
    u.conv_value = FAITH_MOF_SMALL;
    u.user = c;
    if (!faith_register(&u)) ce_log("mof: faith_register failed (table full?)");
    /* ★★ 2026-09-15：**块的认领搬到共享件**（`faith_hud_on_load()`）—— 110/111 现在共用**一块**
     * 组合 UI（用户 2026-09-15 口径 ①②），由共享件按「在场的、id 较小的那张」认领（110 优先；
     * 两张都不在就不认领），块 = **188×60 表面 px**（640 空间 94×30）。
     * ⚠ 顺序：必须在 `faith_register()` **之后** —— 共享件的在场判据就是这张注册表。*/
    faith_hud_on_load();
}

static int on_load(ce_card_t *c)
{
    mof_card_t *s = st(c);

    if (!s) return 0;
    s->last_graze = -1;
    s->last_lives = -1;
    s->stage_frames_ref = -1;             /* ★ 见 `mof_stage_frames()` 的初值语义 */
    s->stage_id_ref = -1;
    mof_ensure_registered(c);
    mof_declare_score(s);
    return 0;
}

static int ctor(ce_card_t *c)
{
    mof_card_t *s = st(c);

    if (!s) return 0;
    /* ★★★ 2026-09-16r **跨关保留**（用户实跑：「切换面数的时候最大得点会被清空」→「**是 Delta 变 0 了**」）。
     * 一手口径（本仓已有先例 = 卡 113 的同一条 bug 与同一条修法，`AUDIT.md` §DK-2）：
     *   **PIV / 最大得点在一局里是一直升、跨关不清**，只有「一局开始」才归零。
     * 机制：`on_run_reset` / 卡对象重建在**关卡切换**时也会跑（`mof_clear()` 那个 `memset` 会把
     *   Δ / 连击 / 信号一起抹掉）⇒ 账本**搬进文件静态**（卡是单例；与 `s_opt[]` / `s_tint_*` 同一层做法），
     *   `ctor` 从这里**恢复**，只有「关卡号变小」（回到第 1 关 / 重开一局 / 练习重选关）才重新初始化。*/
    if (s_slot_stage > 0 && (int32_t)CE_CURRENT_STAGE() < s_slot_stage) {
        ce_log("mof: 新一局（stage %d -> %d）⇒ 信仰账本归零（原 Δ %d）",
               (int)s_slot_stage, (int)CE_CURRENT_STAGE(), (int)(s_slot_keep.delta / FAITH_FIXED));
        faith_slot_init_110(&s->slot);        /* 基础恒 50000、Δ 从 0 起算、下限 0 */
    } else {
        s->slot = s_slot_keep;                /* ★ 一面→二面：**不清**（跨关保留 Δ 与连击）
                                               * ⚠ 「信号槽每关归零」那条只归 111（110 没有信号槽，
                                               *   见 `faith_hud.c` 的声明：110 的 `v.signal` 恒 0）*/
    }
    s_slot_stage = (int32_t)CE_CURRENT_STAGE();
    mof_bonus_init(&s->bonus);
    s->last_graze = -1;
    s->last_lives = -1;
    s->stage_frames_ref = -1;             /* ★ 门③的关卡帧计数：首帧起算（见 `mof_stage_frames()`）*/
    s->stage_id_ref = -1;
    s->log_frame = 0;
    s_p_item = 0;                             /* ★ 2026-09-15：P 点火力那层的每颗道具闩（见 `on_item_commit`）*/
    mof_declare_score(s);
    return 0;
}

static void on_run_reset(ce_card_t *c) { mof_clear(c); }   /* ⚠ 不再清账本：`ctor` 会从 s_slot_keep 恢复 */
static void on_delete(ce_card_t *c)    { mof_clear(c); }

/* ★ 2026-09-15：换关时把**灵击**收掉（一手：TH10 的灵击是一个 per-stage 的对象 ——
 *   `Bomb` 的 anm VM 归关卡所有，换关时 bullet.anm 的 VM 被清 ⇒ VM 没了 `Bomb::on_tick` 就把
 *   `[bomb+0x28]` 置 0（`th10:0x405781`–`th10:0x405786`），半径查询随即返回 0）。
 *   不做这一步的话，一发 296 帧的灵击会**跨关**继续消弹/撒伤害源（打到下一关刚出场的敌人）。
 *   顺带把「本次符卡帧数」与符卡监视器归零（换关本来也不该带着上一关的符卡状态）。
 * ★★ 2026-09-21：收的是共用件 `bomb10_reset()`（那一发状态 110/111 共用 ⇒ 两张卡**都**有权收，
 *   谁先到都算、幂等）。⇒ 111 单装时也顺带得到这条「灵击不跨关」的保证（它原来没有，
 *   见 `AUDIT.md` §BE-EY 的差异表）。「符卡帧数」现在也归共用件（`bomb10_reset()` 一起清）。*/
static void on_stage_start(ce_card_t *c)
{
    mof_card_t *s = st(c);

    if (!s) return;
    bomb10_reset();
    s->spell_watch.was = 0;
    s->spell_watch.bombed = 0;
    s->spell_watch.timed_out = 0;
}

/* ============================ ★★ 主动卡入口（`on_activate` / 用户的 C 键）============================
 * 2026-09-21（owner 口径逐字）：
 *   「**110也得是主动卡，因为只要有110 or 111就有灵击，当然，它们完全走同一套灵击系统**」
 *   （逐字前半句来自 `WORK-PACKAGES.md` §10 的第 4 行回答；111 侧的同一条口径见 §9 与 §BE-EV。）
 *
 * ## 这一层做了什么（只有「多一个入口」这一件事）
 *   · `.active_recharge = 1` ⇒ SDK 在卡绑定时走**主动卡**那一支（`sdk.c` 的 `active_init`）：
 *     `flags = (flags & ~0x46) | 0x08`（`CE_FLAG_ACTIVE`，一手 `th18:0x411d83` 的主动 case）
 *     + 写 `card+0x48` = 充能时长 + 按 Tenshi 初始化两组 zTimer ⇒ **引擎**据此把本卡放进主动卡组、
 *     画进 HUD 主动行、可被 D 选中，并在 C 键上升沿（一手 `th18:0x45c084 test ds:0x4ca434,0x400`
 *     → `0x45c090 mov ecx,[ecx+0x38]`（**被选中的那张**）→ `0x45c099 call [eax+0x8]`）
 *     调到 `vtable+0x08` = SDK 的 `ce_sdk_c_press`（它做门控：state 空闲 + 充能到底 ⇒ 再调这里）。
 *     ⇒ 与 111 的实现**逐项同形**（111 是 `WORK-PACKAGES.md` §9 的先例；值 `= 1` 照本 mod 惯例）。
 *   · 本函数**只调 `bomb10_start()`**（X 路径用的**同一个入口 / 同一份实现**），
 *     **不复制、不另写一套**，也不在这里做任何灵击侧的计算。
 *
 * ## 与 X 路径（`on_tick_2` 的 ⑤）逐项对照 —— 只差「触发条件」，灵击侧**逐项相同**
 *   | 项 | X 路径（⑤）| C 路径（本函数）|
 *   | --- | --- | --- |
 *   | 触发 | `CE_CURRENT_BOMBS() <= 0` 且 `INPUT_PRESSED & 0x2`（X 上升沿）| 引擎的主动卡分派：C 上升沿 `0x400` + 无对话 + 关卡在跑 + **本卡被 D 选中**（四重门控在引擎/SDK 侧，见 `engine/card/th18/04-active-cards.md` §2）|
 *   | 形态判据 | `mof_core_deathbomb_window(player_state)` | **同一个判据**（下面逐字照写：同一函数、同一入参来源）|
 *   | 火力门槛 / 消耗 | `mof_bomb_power_spend()` → `mof_core_power_spend()` | 同（都在 `bomb10_start()` 里）|
 *   | 伤害 / 帧数 / 判定框 / 消弹 / 音效 / 视觉 | `bomb10_start()` + `bomb10_tick()` | 同（**同一个函数**）|
 *   | 决死（B 形态）语义 | `by_hit` ⇒ 救命 + 消弹照常记账 | 同 |
 *   | 火力不足时 | **静默什么都不做**（不扣 P / 不消弹 / 不响音效）| 同（这里**不**加 `ce_refuse()` —— 拒绝音 + 日志是 X 路径没有的行为）|
 *
 * ## 三条口径说明
 *   ① **没有新增闸**：这里**不**加「库存 == 0」。那一句描述的是 **X 那条路与原生炸弹的仲裁**
 *      （库存 > 0 ⇒ X 交给原生 `do_bomb`），**不是灵击的规则**；C 走的是完全独立的主动卡分派
 *      ⇒ 加这个闸就是「新增闸」（与 §9 的口径相反，111 侧同款判断）。
 *   ② **不加冷却数值**：`active_recharge = 1` 是 SDK 主动卡机器的**入场券**（先例 116 / 118 / 111），
 *      不是我们新定的冷却；灵击的真代价仍是「扣 1 档火力」。
 *   ③ **返回值 0 = 瞬发**：`ce_sdk_c_press` 拿到 0 会把状态置 2（收尾），本卡**不声明**
 *      `on_active_tick` ⇒ 主动卡机器不会替我们推进任何东西；灵击照旧由 `on_tick_2` 的 ⑤
 *      （`bomb10_tick`）推进 —— **与从哪个入口发动无关**。
 *
 * ## 🟡 已知的一帧相位差（与 111 逐字相同，诚实记一笔）
 *   一手帧序（`engine.h` 的 `CE_PLAYER_SPEED_MULT` 注）：**AbilityManager tick（`0x16`）先于 Player（`0x17`）**，
 *   而 C 的分派在 `Player__on_tick__body`（`0x17`）里 ⇒ C 入口发动的那一帧，`on_tick_2` 的 ⑤
 *   **已经跑过**了 ⇒ 灵击的**第一次 tick 落在下一帧**；X 入口是「⑤ 发动 → 当帧推进一次」。
 *   **总帧数（`MOF_BOMB_FRAMES` = 296）、每帧伤害、半径曲线、消耗、消弹全部逐项相同**，
 *   差的只是「起手那一帧要不要立刻推进一次」⇒ owner 2026-09-21 拍「不动 X（不并进 ⑤）」。*/
static int on_activate(ce_card_t *c)
{
    mof_card_t *s = st(c);
    uint8_t *pl;
    int32_t ps, by_hit;

    if (!s) return 0;
    /* 形态判据 = **逐字照 X 路径**（`on_tick_2` ⑤）：决死窗口内发动 ⇒ 决死 B 形态（救命 + 消弹照常记账）。
     * ⚠ 这两行是**同一判据的同一写法**（不是另写一套规则）：`mof_core_deathbomb_window()` 是
     *   `mof_core.h` 里唯一的窗口判据，两个入口共用它。*/
    pl = CE_PLAYER();
    ps = pl ? *(int32_t *)(pl + CE_PLAYER_STATE) : -1;
    by_hit = mof_core_deathbomb_window(ps);
    ce_log("mof: **C（主动卡发动）** ⇒ 调 bomb10_start()（与 X 路径同一入口；库存 %d、"
           "state %d%s）", (int)CE_CURRENT_BOMBS(), (int)ps, by_hit ? " = 决死窗口" : "");
    (void)bomb10_start(&s_bomb_host, s, by_hit);
    return 0;                      /* 瞬发：不持续、不接管 `on_active_tick`（理由见上 ③）*/
}

/* 钩子分工（★ 2026-09-15：**只剩一条路**）：
 *   · `on_enemy_death`（断点 `0x430240`，**撒落之前**）→ 大信仰点的**唯一**入口：过死亡事件的门
 *     （非 KILL_ALL / 真敌人）+ **宿主表**（`mof_big_faith_table.h`，键 = `(anm 源, 主脚本)`）
 *     + 非 boss。旧判据「`drop_main == 2` 就给」已被用户 2026-09-15 的裁决取代
 *     （「金钱和绿点总之别重合」）。
 *   · `on_enemy_drop_pre`（`0x430510`，撒落之后）★ **已不挂**（2026-09-15 删）：它原来的唯一职责
 *     是旧判据②「同时掉钱与 P」，而「这敌掉什么」已整个退出判据 ⇒ 零职责。历史见那个函数位置的长注。
 *   · 本卡**不改写掉落表**：撒的是自建 VM ⇒ 不需要死亡钩子「在撒之前」的那个特权，
 *     但那里仍是**唯一**能拿到「未经消费的 `drop_main`」的位置（现在只用它记日志）。
 *   · **满 P 转换**（A/B）改的**不是掉落表、也不是原生 type**，而是**已在场**的原生 P 道具本身
 *     （共用件扫池 → 原位删 + 原位撒）—— 落点选它的理由是「一手讲的是**不变式**（满 P 期间场上
 *     没有 P 类道具），只有看场上的那条路做得到」，全文见 `faith.h`。
 * 「不做」清单（spec §6）与长持续效果自查（METHOD 铁律二）见文件头。*/
CE_CARD(110,
        /* ★★ 2026-09-21（owner 口径逐字：「**110也得是主动卡，因为只要有110 or 111就有灵击，
         *   当然，它们完全走同一套灵击系统**」）：**主动卡** —— 灵击多一个 C 入口。
         *   `.active_recharge = 1` = 先例取值（111 / 116 / 118 同值）；`.on_activate` 里
         *   **只调共用件的 `bomb10_start()`**（X 路径用的同一个入口）；**X 路径一字未动**。
         *   ⚠ 配套：`patch/th18/cards.js` 的 110 条目必须 `"category": 0`（契约 reason 2：
         *     有主动行为 ⟺ `category == 0`；不一致 = **整局新卡禁用**，先例见 `AUDIT.md` §BE-EJ）。*/
        .active_recharge = 1,
        .on_activate = on_activate,
        .ctor = ctor,
        .on_load = on_load,
        .on_tick_2 = on_tick_2,
        .on_draw = on_draw,
        .on_stage_start = on_stage_start,
        .on_enemy_death = on_enemy_death,
        .on_item_collect = on_item_collect,
        .on_item_commit = on_item_commit,        /* ★ 2026-09-15 P 点 +0.05 档（补差 4 单位；见该函数）*/
        .on_item_score = on_item_score,
        .on_run_reset = on_run_reset,
        .on_delete = on_delete);
