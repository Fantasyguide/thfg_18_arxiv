/* faith.c —— 绿点 / 信仰系统（110 风神录 + 111 地灵殿）**共用**的引擎侧实现：VM 池 + 收集判定。
 *
 * ## 为什么共用一个池（用户要的说明）
 *
 * 用户 2026-09-12 定案：「111 的绿点和 110 的绿点**完完全全就是同一个东西**，它们加的都是分，
 * 只是加的数量不一样」「**一套系统 + 分槽记账**」。所以：
 *   · **实体层共用一个池**（本文件）—— 撒点、飞散、飘落、吸附、收集判定写一遍；
 *   · **记账层按槽分**（`faith_core.c` 的 `faith_slot_t`）—— 110 只写 `Δ_110`、111 只写 `Δ_111`，
 *     两者**互不可见**（「不跨卡耦合」，review §3.1 第 1 条）。
 *   · 池里的每颗点带 `slot_mask`，收集时**只回调认领该 mask 的卡** ⇒ 一张卡装的时候另一槽根本没人写。
 *
 * 代价（已知、用户已知悉）：自建实体**不入原生道具池**（review §7.3）⇒ 原生磁吸 / 回收线
 * **看不到它**。所以吸引与收集判定在这里自己做，判据照抄原生（读自机字段）。
 * ★ 2026-09-15（`NEXT.md` §1x.2）：「114 的批集计数看不到它」**这一条已经接上了** ——
 *   本文件在收点那一刻向 SDK 的「虚拟收集 / 虚拟在飞」层报一颗（`ce_vitem_note_collect`），
 *   并每帧声明在飞数（`ce_vitem_declare_flying`）；114（辉针城）在它自己的 `on_item_frame` 里读。
 *   ⇒ 大 / 中两档**进 114 的批计数**（**不进 SCORE、不进批分**，用户口径「只 +1」）、
 *   在飞期间**续批**（不提前结算）；**微档不入批**（§1x.3，它是半道具）。
 * ★★ 2026-09-18（B1）：两条通道**都只认「真空飞行」（本池 `state == 3`）** —— 与引擎对 P 点 /
 *   金钱的谓词（`type ∈ {1,2}` 且 `item+0xc74 == 3`，§Z22）**同一条**，即用户 2026-09-18 的规格
 *   「信仰点与 P 点 / 金钱完全一致：开批 / 计数 +1 / 飞行中组织批次结算」。判据全文（含两套
 *   state 编号的对照表）在 `faith_core.h`；本文件的落点见 `dot_collect()` 与 `faith_pool_tick()` 尾部。
 *   层的契约与「为什么用声明式而不是卡间直连」见 `sdk_core.h`。
 *
 * ## 运动学：照原生道具逐条抄（不自己发明）
 *
 * 一手：TH18 `ItemManager__on_tick__body` `0x445a80`（与 TH16 `th16:0x42f4e0` 同款代码同一组常量）——
 * 完整推导与地址见 `native/cards/season.c` 文件头（33/34 轮实跑修出来的那份）。本文件沿用同一套：
 *   · 出生：**角恒 −π/2（直上）+ 定速**，逐档一手（★ 2026-09-15 E-2 订正，见 `faith_core.h` 的 §E-2）：
 *       击破掉落 / 满 P 转换（大 / 中）= `0xbfc90fdb` / `0x400ccccd`（th10:0x41bac1 等）、
 *       消弹（微）= `0xbfc90fdb` / `0x3f19999a`（th10:0x4081d9）；
 *       TH11 侧同参（`th11:0x49842c` / `th11:0x498430`）；TH18 自己的撒落同参（`th18:0x430481`）。
 *       ⚠ **旧实现是「角度与速度由调用方随机给（0..2π、0.2..2.1）」= ❓ 自定**（自述「随机散得更自然」），
 *       2026-09-15 按一手删掉：原版撒点**不随机**，而且撒在**实体自己的位置**（弹 / 道具原位）。
 *       ⇒ 同帧同源的一撮会完全重叠 —— 这正是原版的行为（我们不再用随机角去「修」它）。
 *   · 飞散：`pos += vel`、`r -= 0.03`、`vel = polar(角, r)` ⇒ 沿固定方向减速直线飞行；
 *   · 飘落：`vy += 0.03`（`0x4b90a4`）、`vy > 2.0 → 2.0`、`vy >= 0` 时水平速度清零；
 *   · 吸引：进吸引半径 → 朝自机飞，初速 = 磁速/3，每帧 +0.2 到上限 12.0；过回收线（`player+0x47998`）
 *     → 朝自机飞，初速 = 磁速本身；
 *   · 收集半径 / 吸引半径 / 磁速**读自机自己的字段**（`player+0x4798c` / `+0x47990|94` / `+0x47988`）；
 *   · 出界删：`y > 472`、`|x| >= 200`。
 *   ⇒ 与 season.c 是**同一份代码的第二次出现**（SDK.md §5 的「出现第二次就提炼」还没做，
 *      因为季节点是「按副季节换贴图 + 释放期全收」，绿点是「三档 + 按槽记账」；
 *      两者的**差异**都在调用侧，共同部分已尽量收进本文件的常量里）。⚠ 待办：抽成一个
 *      `item_fly.c` 让 116 / 110 / 111 共用（登记在报告里）。
 *
 * ## ❓ 自定（原版确实没有 / 我们没找到）
 *
 *   · **微档不建实体**：§2.2 的微信仰点有 4 个来源（道中/boss 非符消弹、击破消弹、全避消弹、
 *     登场消弹），其中只有「我们自己放灵击」这条我们现在接得到；消弹每颗 +10 走
 *     `faith_cancel_account()` 直接进账，不撒实体（原作那边是可见的星形碎片）。
 *     ★ **差别到底有多大（2026-09-15 一手订正，`local/_scratch/research-th10-item-motion.md`）**：
 *     TH10 道具的 state 机是 `state 5`（**刚生成的待机倒计时**，`th10:0x41b01b`）→ `state 2`（上升）
 *     → **约 20 帧后 `th10:0x41b185`–`0x41b1a4` 无条件转 `state 3` = 飞向自机** ⇒
 *     原版**不需要玩家走过去收**（旧研究稿「`state 5` ⇒ 要走过去收」是误读：它引的 `th10:0x41ba67`
 *     比的是 `item+0x3e0` = **type**，那个函数是满 P 转换）。⇒ 我们与原版的差别只剩
 *     **① 画面上那颗星形碎片 ② 约 20 帧的入账延迟**，**没有"满地要捡"的手感差异**。
 *     登记为待实跑：拿到断点后再补实体版（`faith_spawn(..., FAITH_TIER_MICRO, ...)`）。
 *     ★ 2026-09-15b：微档的**贴图与脚本都已经备好且与小档不再相同**（`faith_sprite_for()`：
 *     TH10 类型 8 的第二套绿点 `(240,208)` → `FAITH_DOT_MICRO.png`；`faith_script_for()`：
 *     脚本 273 = TH10 `script382` 的 `blendMode(1)` + `alpha(16)` + `scale(0→1)` 加算淡光球）
 *     ⇒ 哪天补实体，三档外观**开箱即是三样**，不用再找图/找脚本。⚠️ 但在那之前这一档
 *     **在实跑里看不见**（池里根本没有微档点）—— 别把「代码里已经是三样」当成「画面上是三样」。
 *   · 收集动画**照播**（脚本 272 末尾的 `interruptLabel(1)`）：TH10 的道具被吃到时是引擎直接删 VM，
 *     没有脚本级收尾；本仓实体池固定发中断 1 ⇒ 照本仓既有实体脚本（115/114/101）的形状补。
 *     登记在 `TUNING.md` §2.6。
 *   · ~~贴图染色~~（**2026-09-15 已删**）：原来借季节点 + `ce_anm_set_color` 染绿，
 *     现在换成 TH10 原生信仰点贴图（本来就是绿的）⇒ 顶点色写白。
 *   · ~~出生保护 32 帧~~（**2026-09-15 已删**，见 `faith.h`）：一手 + 实测，TH18 原生 P 点 / 金钱
 *     **没有**出生无敌（道具 tick 的收点检查从出生第一帧就跑）⇒ 我们那道 32 帧闸门是自定。
 *     出生动画（脚本 272 / 273）**一个字没改** —— 它只是画成什么样，不该挡住吸与收。
 *     ⚠ 同一个 32 当年还兼作「减速抖动」的门 —— 那一条**是引擎自己的条件**（`age > 31`），
 *     所以它按 `FAITH_SLOW_JITTER_AGE` 留下来了（**删掉的只有收点 / 吸入 / 物理三条**）。
 *   · ~~随机出生角 / 速度~~（**2026-09-15 已删**，E-2）：原版是固定角 −π/2 + 定速（一手实参）。
 *   · **满 P 全场转换的「音」**：一手每次转换播一个 SE（`th10:0x448db0` 参 `0x189`；th11 是
 *     `0x455b10` 参 `0x16`/`0x73`），**我们没播** —— 那是**两作各自的 SE 表**里的编号，TH18 没有
 *     同一份音源，挑哪一个要用户拍板 ⇒ 记 `❓ 自定（不播）`，登记 `TUNING.md` §2.5c。
 *     其余（原位、角 −π/2、速 2.2、type 9 档、一颗一颗转）全是**复刻**。
 */
#include <stddef.h>          /* sdk.h 的 ce_item_collect_all 用了 size_t（内置 Linux 头不带）*/
#include <string.h>
#include "sdk.h"
#include "faith.h"

/* —— 运动学常量（出处见文件头；与 season.c 同值，别单独调）—— */
#define FAITH_GRAVITY         0.03f    /* 每帧重力增量（`0x4b90a4`）*/
#define FAITH_VFALL_MAX       2.0f    /* 下落速度上限（`0x494558`）*/
#define FAITH_MAG_MAX         12.0f   /* 吸引速度上限（`0x4945d4`）*/
#define FAITH_MAG_STEP        0.2f    /* 吸引速度每帧增量 */
#define FAITH_OOB_Y           472.0f
#define FAITH_OOB_X           200.0f
/* ★ E-2：出生角/速**不再由调用方随机给**，改成一手实参（角度与逐档速度见 `faith_core.h`）。
 * 池侧的出生参数只有一条：`faith_spawn()` 里用 `FAITH_THROW_ANGLE` + `faith_core_throw_speed(tier)`。
 * ⇒ 下面这两个旧的随机区间常量**已删**（`FAITH_THROW_MIN` / `FAITH_THROW_SPAN`）。*/
/* MAX_POWER（`engine.h` 的 `CE_ADDR_MAX_POWER` = 0x4ccd3c = GlobalsInner+0x60）。
 * ⚠ 卡侧各有一个同址宏（`mof.c` 的 `CE_MAX_POWER_VAL()` / `signal.c` 的 `SIGNAL_MAX_POWER_VAL()`），
 *   但本文件是**共用件**、不许反向依赖卡的头 ⇒ 就地读同一个地址。*/
#define FAITH_MAX_POWER()     (*(const int32_t *)CE_ADDR_MAX_POWER)

/* ★ 减速时的「抖动 / 染色」门：`age >= 32`（= 引擎自己的 `age > 31`）。
 * 一手：`th18:0x445a80` 的同段（`th16:0x42fad2`–`0x42fb35`）—— 引擎的道具 tick 在
 * **`slowdown_factor < 1.0` 且道具年龄 > 31 帧**时才把 VM 位置写成 `randf(−1,1)`（`season.c` 同款）。
 * ⚠ 这一条**不是**出生保护（C 条删掉的那个）：引擎自己的判定就长这样，且只有「减速中」才相关
 * （绀传珠卡的 `ITEM_SLOWDOWN`）。收点 / 吸入 / 物理三条路**没有**这个门。*/
#define FAITH_SLOW_JITTER_AGE  32

/* ★★ 2026-09-18（B12，用户报「满 P 的 P 点转化缺特效和音效」）：
 *
 * ## 一、转换音（`faith_convert_full_p_items()` 里逐颗播）
 * 一手：TH10 的转换循环里**每颗播一次**（`th10:0x41bad2 push 0x189` → `call 0x448db0`(SoundManager 带位置)），
 * TH11 同款（`th11:0x455b10` 参 `0x16`/`0x73`）。⇒ 语义 = **每转换一颗 P 道具响一声**、声像用该道具的 x。
 * ⚠ **id 是 ❓自定**：`0x189`(TH10) / `0x16`、`0x73`(TH11) 是**两作各自的 SE 表编号**，TH18 的表只有
 *   84 行（`engine/_shared/th18-sound-table.md` §0）⇒ 拿不到同一份音源，只能挑一支语义最近的。
 *   本轮取 **`0x0d = se_powerup.wav`**（TH18「火力上升」的音；「满 P ⇒ P 点转信仰点」正是 P 系语义）。
 *   替换 = 改这一个数字（候选：`0x1c/0x1d = se_power0/1`、`0x25 = se_item00`、`0x4c = se_item01`）。
 *   登记 `TUNING.md` §2.5g。*/
#define FAITH_CONV_SE          0x0d

/* ## 二、「上抛」必须看得见 —— 抛完才许被真空门吸走
 * 用户实跑口径：「应该是从**当前 P 点的位置上抛**转化中信仰点」。
 * 一手 TH10（`th10:0x41b173`–`0x41b1a4`，state-2「上升」那一段）：
 *   `[item+0xc] += 1.0([0x476f78]) × 0.03([0x470cdc])`（每帧 +0.03 = 我们同一支重力常量）
 *   → `fcomp [0x470b04] = 0.0` → **一旦升到顶点（vy ≥ 0）就 `mov [item+0x2c],3`**（转「飞向自机」）。
 *   ⇒ 原版的道具**先把自己的上抛走完**，之后才飞向自机（另见 `faith.c` 文件头对 TH10 state 机的记录）。
 * ⚠ 我们的池是 TH18 宿主的道具运动学（飘落 → 过回收线/放 B/对话 ⇒ 真空），**出生第一帧就能被真空门
 *   翻成 state 3**（`faith_vacuum_gate()`）⇒ 玩家满 P 且在高处时，转换出来的点在**画面上第一帧就已经
 *   朝自机飞**，看起来像「点不是从 P 点位置出来的」（这正是用户报的观感）。
 * ⇒ 本轮让**上抛期（`state == 2` 且 `age < FAITH_TOSS_FRAMES`）豁免真空门**：点从 P 点位置上抛、
 *   抛完（或过了这段）才受回收线/放 B/对话那三条门支配。
 * ⚠ 只豁免**真空门**，**不**豁免收点判定：触收（30px）/ 70px 环吸 / `faith_collect_all()`（全屏收点）
 *   从出生第一帧就照旧生效（那三条是「出生保护已删」的既有结论，不许被这一条带回来）。
 * ❓ 自定：`FAITH_TOSS_FRAMES = 20` —— TH10 顶点判定按 `vy: −2.2 → 0` 每帧 +0.03 算是 73 帧，
 *   但本池的「飞散」段（`lr -= 0.03`）本来就是 73 帧、豁免整段会让「放 B / 进对话立刻吸」变钝
 *   ⇒ 取 20 帧（0.33 s）当「看得见上抛」的下限。体感不对就改这一个数（登记 `TUNING.md` §2.5g）。*/
#define FAITH_TOSS_FRAMES      20

/* ==================== ★★ §1x.4：绀珠传（115）擦弹期「三件套」—— 变慢 / 变黄 / 抖动 ====================
 *
 * 用户 2026-09-15 口径：「th15 擦弹时候**大中绿点也得变黄、变慢、抖动**」。
 * 绿点是**本池自建的 VM** ⇒ 引擎不会替它做这三件事（引擎只替**原生道具**做，见下面的逐指令一手）
 * ⇒ 三件套都得在本池里复刻一层。「变慢 + 抖动」这套的**源头是 TH16 的季节点，而它复刻的是 TH15**
 * （★ 用户 2026-09-15 定案「th16 的季节点的抖动复刻了 th15」⇒ 不必再逆 th15），
 * 所以下面按「116 季节点已经做好的那套」同口径实现（`season.c` 的 `points_tick` 尾部那一段）。
 *
 * ## 一、引擎自己怎么做（**一手：th18 本地 exe 逐指令**，2026-09-15d 复核）
 *
 * 卡在哪一段：`ItemManager__on_tick__body` `th18:0x445a80`，**在「重力在跑」的那一段里**
 * （`item+0xc74 == 2`，段尾 `0x445c85` 起；道具池循环 `edi = mgr+0x14`、步长 `0xc94`、`cmp eax,0x1258`）。
 * （段号是引擎的 `item+0xc74`；本池的 `faith_dot_t.state` 用的是**照抄运动学**的那套编号 ——
 *   本池 `state 2` = 飞散 / 上抛、`state 1` = 飘落，与引擎编号**不逐值对应**，别混读。）
 *   ```
 *   00445c6d  MULSS XMM0,[ECX+0xe6bb14]    ; ★ 重力增量 ← ×factor（本段就是「下落」那一段）
 *   00445c85  JL  0x00445de6              ; age（item+0xc50）> 31 才继续（否则整段跳过）
 *   00445c8b  COMISS XMM5,[ECX+0xe6bb14]  ; XMM5 = 1.0f（`fVar22`）；factor >= 1.0 ⇒ 跳去「还原」
 *   —— 减速中（factor < 1.0）——
 *   00445c9d  CALL 0x402810               ; Rng__randf_neg_1_to_1 ×2 ⇒ 两个独立随机
 *   00445cd4  CALL 0x488b40               ; AnmManager__get_vm_with_id(item+0xc28) = 抖动用
 *   00445ce7  MOVQ  [EAX+0x5f0],XMM0      ; ⇒ VM 的 **pos.x/pos.y = randf(−1,1)**（z = 0）
 *   00445d01  CALL 0x488b40               ; 再取一次 = 染色用
 *   00445d12  MOV   dword [EAX+0x524],0xffa0ffff            ; ⇒ **color_1 = 0xffa0ffff（淡黄）**
 *   00445d42  MOV   word  [EDI+0x535],0xffa0                ; color_1 的另一路（A/B）同色
 *   00445ddf  MOV   byte  [EDI+0x534],0xff
 *   —— 关掉（factor >= 1.0）——
 *   00445ebe  MOV   dword [ECX+0x524],0xffffffff            ; 还原：color_1 写回白
 *   00445dcf  MOV   word  [EDI+0x535],0xffff / 0x445dd8 byte [EDI+0x537]=0xff
 *   ```
 *   另一段（`item+0xc74 == 1`，上抛 / 初速，`0x445bed`–`0x445c55`）**不乘** factor：
 *   它只做 `vel × GAME_SPEED`（`0x445c15`/`0x445c19`/`0x445c21`）—— 引擎对「刚出生那一下上抛」不减速。
 *   ⇒ 三条一手事实：① **门 = `age > 31` 且 `factor < 1.0`**；② 抖动是
 *   **VM 的 pos 加 `randf(−1,1)`**；③ 染色写的是
 *   **`color_1`**，值 **`0xffa0ffff`**；关掉时**无条件还原成 `0xffffffff`**（白 = 原色）。
 *   ★ 单条指令的染色常量 `0xffa0ffff` 是本轮新查到的 —— 它同时是「引擎确实会染色」的**一手**，
 *     以及「引擎染成什么颜色」的**一手**。`0xffa0ffff` = BGRA，byte 序 = ff ff a0 ff
 *     ⇒ **B=255 / G=160 / R=255 / A=255** = 偏白的淡黄。
 *     ⚠ **我们不用这个值** —— 用户 2026-09-15 裁决统一成 **TH15 给弹的黄 `0xffffd080`**
 *     （见下面 §三）；这里留着是作为「已知差异」的一手凭据。
 *   ⚠ 同段里 `0x445dcf`/`0x445dd8`/`0x445ddf` 那三条 `+0x534`/`+0x535`/`+0x537` 的写点与
 *     `engine/anm/th18/07-blendmode-rgb-only-alpha-is-sa.md` 的「另一路顶点色（SA 分量）」一致，
 *     两条路一起写才是引擎「整条 color_1 同色」的效果 —— 我们这边走 SDK 的 `ce_anm_set_color()`
 *     （写 `color_1` + `color_2`，见 `dot_spawn_vm()` 的口径），行为等价。
 *
 * ## 二、本池照抄成什么（= `faith_dot_slow_style()`）
 *
 * | 层 | 引擎（原生道具）| 本池（自建绿点）| 出处 |
 * | --- | --- | --- | --- |
 * | 门 | `age > 31 && factor < 1.0` | `age >= FAITH_SLOW_JITTER_AGE && slowed` | 一手 `0x445c85`/`0x445c8b` |
 * | 变慢 | 位移与重力增量都乘 factor（`0x445c6d`、`0x445c7d`）| 同（作用在**飞散态 2 / 飘落态 1**）| 一手 + `season.c` 同口径 |
 * | 抖动 | 画出来的位置 = 逻辑位置 ± 1 px 随机 | 同（`ce_anm_set_pos` 写 `x+jx, y+jy`）| 一手 `0x445ce7`；TH16 季节点同款 |
 * | 变黄 | `color_1 = 0xffa0ffff`（关掉写回白）| **`0xffffd080`**（**TH15 那支**，见 §三）| 引擎侧一手 `0x445d12`/`0x445ebe`；我们取的那支一手 = TH15 `th15:0x419d3d` |
 *
 * ⚠ **只改 RGB、保留 alpha** 这条沿用本仓既有的两处教训（`season.c` 第三十三轮：季节点脚本自己有
 * `alphaTime(300,0,128)` 的淡出，写死 alpha 会把它按掉；`lolk.c` 的弹染色同坑「出圈变透明」）
 * —— 引擎写那条 `0xffa0ffff` 时是「刚出生、alpha 还是 255」的样子，**不能把 alpha 一起写死**。
 * ⇒ 我们读回当前 `color_1` 的 A，与 `FAITH_SLOW_TINT` 的 RGB 拼起来写回去。
 *
 * ## 三、颜色取值的**用户口径**（★ 不是复刻，别写成复刻）
 *
 * 两处「黄」摆在一起，最后由**用户拍板**选了一支：
 *   · **引擎给原生道具的**：`0xffa0ffff`（一手 `th18:0x445d12`，淡黄偏白）；
 *   · **TH15 给被擦到的弹的 / 116 季节点在用的**：`0xffffd080`
 *     （一手 `th15:0x419d3d`，`lolk.c` 的 `LOLK_GRAZED_COLOR`）。
 * ⇒ ★★ **用户 2026-09-15 裁决：统一成 TH15 那支**，原话「**肯定得统一成 th15，包括到时候
 *   th07 的樱点**」⇒ 本层 `FAITH_SLOW_TINT = 0xffffd080`，与 116 季节点**同色**，
 *   将来 107（妖妖梦）的樱点做减速视觉时**也必须复用同一常量**（待办见 `TUNING.md` §2.5d 末尾）。
 *   ⚠ **已知差异（刻意保留，不修）**：引擎自己在减速中给**原生道具**写的是 `0xffa0ffff`
 *   （一手 `0x445d12`）⇒ 同屏时**我们的绿点与原生 P 点 / 金钱会差一点点色**（G 通道 a0 vs d0）。
 *   我们**刻意不用引擎那支**，理由是**用户要求与 TH15 / 季节点统一**（不是「引擎那条不对」）。
 *   ⇒ 这一条的性质是 **❓ 用户口径（自定）**，**不许包装成复刻**；引擎那条一手事实照旧留着备查。
 *
 * ## 四、与 `season.c` 的关系（陈账，别人接手时看）
 *
 * `season.c` 的 `points_tick` 里有**同源代码的第二次出现**（同样的门、同样的 ±1 px 抖动、
 * 颜色用 TH15 那支黄）。本仓惯例「出现第二次就提炼」，但 **116 不在本卡的文件所有权内**
 * （`CLAUDE.md` 的协作纪律）⇒ 本轮**没有动它**。下面两支是提炼后的**唯一实现处**，
 * 116 的 owner 若要接：`slowed = faith_item_slowdown_permille() < 1000` + 一次 `faith_dot_slow_style(...)`。
 * ★ 2026-09-15 用户裁决之后，**两支的颜色已经一致**（都是 `0xffffd080`）⇒ 接过去**观感零变化**。*/
#define FAITH_SLOW_TINT        0xffffd080u  /* ★ 用户口径 = TH15 给弹的黄（一手 `th15:0x419d3d`，
                                             *   = 116 季节点 `LOLK_GRAZED_COLOR`）。
                                             *   ⚠ 引擎给**道具**的那支是 `0xffa0ffff`（一手
                                             *   `th18:0x445d12`）—— 刻意不用，见上面 §三。*/
#define FAITH_TINT_NONE_RGB    0x00ffffffu  /* 还原用的 RGB：白 = 不染色（A 另取当前的）*/
/* 本池**未染色**时的整条顶点色（三档出生就写它；`faith_tint_for()` 会给微档 0x10ffffff）——
 * 只在本函数**读不到 VM** 的兜底分支里当「当前色」用（正常路径总会读到活 VM）。*/
#define FAITH_TINT_NONE        0xffffffffu
#define FAITH_SLOW_NONE        1000        /* 「没在减速」的千分比（= 1.0f）*/

/* 读引擎的道具减速因子（`itemmgr+0xe6bb14`，`engine.h` 的 `CE_ITEM_SLOWDOWN`）的千分比。
 * 一手：绀珠传卡（115）每帧写 0.3，引擎自己 +0.1/帧 回 1.0（`th18:0x446807`–`0x446821`）。
 * （引擎恒 ≥ 0；> 1 的中间态按 1.0 处理 —— 我们只接受「变慢」。NaN 由 `!(f > 0.0f)` 挡住。）
 *
 * ★★ **为什么长这样**（`make dllx87` 的不变式，别「顺手优化」掉）：本仓要求我们的 object 里
 *   **0 条 x87 指令**，而 i686 Windows ABI 的 **float 返回值 / 按值传 float 走 x87 `st(0)`**
 *   ⇒ 只要写成「`static float faith_item_slowdown_permille(void)` + 调用方接 float」或
 *   「`int` 按值传 float 再乘」，编译器就会在边界上插一条 `flds (%esp)`（本轮实测踩到：
 *   `faith_item_slowdown_permille` 起手一条 `flds`）。
 *   ⇒ 读法**直接写成 `static inline`**：模块内部调用点与卡侧调用点都在**同一个编译单元**里
 *   就地展开（纯 SSE），**不产生任何 ABI 边界** ⇒ 零 x87。语义与「返回千分比」完全一致。
 *   ⚠ 卡侧要用它：`#include "cards/faith.h"`（`static inline` 随头走；`faith.h` 已是共用件）。*/
static inline int faith_item_slowdown_permille(void)
{
    uint8_t *im = CE_ITEM_MANAGER();
    float f;

    if (!im) return FAITH_SLOW_NONE;
    f = *(float *)(im + CE_ITEM_SLOWDOWN);
    if (!(f > 0.0f) || f >= 1.0f) return FAITH_SLOW_NONE;
    return (int)(f * 1000.0f + 0.5f);
}

/* 卡侧（别的编译单元）读减速因子的入口 —— 必须是**独立函数**：`static inline` 的那支随头走时，
 * 卡侧编译单元里会多出一份展开体（本仓规矩是「共用件只留一份实现」）。这里只是转一手，
 * float 全部在**同一个 TU 内**内联展开 ⇒ 没有 ABI 边界、没有 x87（见上面那段 ★★）。*/
int faith_slowdown_permille(void)
{
    return faith_item_slowdown_permille();
}

void faith_dot_slow_style(uint32_t anm_id, uint8_t age, int slowed, float x, float y)
{
    /* ★ 诊断（只读 + 一条日志，实跑对账 §1x.4 的三层用）：累计「有多少颗点被染过黄」，
     * 每 600 帧打一行。门 = 「减速中」（不看年龄门），因此它与 600 帧汇总里
     * `pickups/dropped` 的节奏能对上。**不参与任何逻辑**：减速结束时计数清零，
     * 下一段减速从 0 重新数（不跨段累加）。*/
    static int32_t s_stat_slow_frames, s_stat_slow_dots;
    uint8_t *vm;
    uint32_t cur;
    const int tinted = (slowed && age >= FAITH_SLOW_JITTER_AGE);

    if (slowed) {
        if (++s_stat_slow_frames >= 600) {
            ce_log("faith: 减速三件套 —— 近 %d 帧有 %d 颗点被染黄/抖动（绀珠传 115 的 slowdown 因子）",
                   (int)s_stat_slow_frames, (int)s_stat_slow_dots);
            s_stat_slow_frames = 0;
            s_stat_slow_dots = 0;
        }
    } else {
        s_stat_slow_frames = s_stat_slow_dots = 0;
    }
    if (tinted) ++s_stat_slow_dots;
    if (!anm_id) return;
    vm = ce_anm_get_vm(anm_id);
    cur = vm ? *(uint32_t *)(vm + CE_ANM_VM_COLOR1) : FAITH_TINT_NONE;
    /* 只改 RGB、保留当前 alpha（见上 §二 的 ⚠）。 */
    ce_anm_set_color(anm_id, (cur & 0xff000000u) |
                             (tinted ? (FAITH_SLOW_TINT & 0x00ffffffu) : FAITH_TINT_NONE_RGB));
    if (tinted) {
        /* 抖动 = 画出来的位置加 ±1 px（一手 `0x445ce7`：引擎写 VM 的 entity_pos，
         * 我们的实体没有「逻辑坐标 vs 画坐标」两层 ⇒ 直接写 pos）。取值与 116 季节点同分布。*/
        float jx = (float)((int32_t)(ce_rand() % 3) - 1);
        float jy = (float)((int32_t)(ce_rand() % 3) - 1);
        ce_anm_set_pos(anm_id, x + jx, y + jy, 0.0f);
    } else {
        ce_anm_set_pos(anm_id, x, y, 0.0f);       /* 关掉时位置写回逻辑坐标（引擎同：不再叠随机）*/
    }
}

#define FAITH_COLLECT_R_DEF   30.0f
#define FAITH_ATTRACT_R_DEF   70.0f
#define FAITH_MAG_SPEED_DEF   5.0f

/* 大点的运动学稍微「重」一点（下落更快、吸引起点更远）—— ❓ 自定：原版大/小信仰点的
 * 运动学差异**未查**（th10 那边是同一套 item tick，差异只在半径/分值）。留一个系数方便实跑调。*/
#define FAITH_BIG_MAG_BONUS   1.0f

typedef struct {
    int32_t id;            /* anm VM id；0 = 空槽 */
    float   x, y;
    float   vx, vy;
    float   lang;          /* 飞散角（整个飞散期不变）*/
    float   lr;            /* 飞散剩余速度（每帧 −0.03）*/
    uint8_t state;         /* 0 = 待发射，1 = 飘落，2 = 飞散，3 = 过回收线飞向自机，4 = 吸引半径内飞向自机 */
    uint8_t mag20;         /* 吸引速度 ×20 */
    uint8_t age;
    uint8_t delay;
    uint8_t tier;          /* faith_tier_t */
    int32_t slot_mask;     /* 落进谁的槽 */
    int32_t value;         /* 原始单位分值 */
} faith_dot_t;

/* ★ 池放 DLL 静态区（ce_state 只有 496 字节，装不下几百颗点；同 season.c 的口径）。*/
static faith_user_t s_user[FAITH_MAX_USERS];
static int          s_n_user;

/* 诊断（静态区）：每 600 帧一条汇总，实跑对账用 */
static int32_t s_stat_pickups, s_stat_points, s_stat_frame, s_stat_dropped;
/* ★★ 2026-09-18（B1）：被收的点里，「真空飞行中 ⇒ 报进 114 的批」与「普通游玩顺手捡 ⇒ 不报」
 * 各有多少颗（判据 = `faith_core_state_is_vacuum_flight()`；同一 600 帧窗口内清零）。
 * 实跑读法：两列都能读到数才说明门在动；若 `skip` 恒 0 而 `pickups` 在涨 ⇒ 门没生效。*/
static int32_t s_stat_vitem_member, s_stat_vitem_skip;

/* ★★ 2026-09-15h：P 点补值共享策略的**相位**（权威副本，只有这一份）。
 * 谁推进 / 什么时候推进 / 「被删掉会怎样」—— 全在 `faith.h` 那段；这里只放数据与三支入口。*/
static faith_p_alt_t s_p_alt;

int32_t faith_p_topup_take(void)
{
    /* 判据（现成件）：111 在场 = 注册表的 111 位；机体门 = 魔理沙（用户拍板的 Σ 映射）。*/
    int has_111   = (faith_registered_mask() & (1 << FAITH_SLOT_111)) ? 1 : 0;
    int is_marisa = (*(const int32_t *)CE_ADDR_CHARACTER == FAITH_CHAR_MARISA) ? 1 : 0;
    /* ★ 「这一颗拿不拿得到火力」也在这里判（**不交给卡侧**，否则两张卡各写一遍、必然漂移）：
     *   满火力 / `MAX_POWER` 还没初始化 ⇒ 原生也不给（`th18:0x446981` 的 `jl` 不跳）⇒ 这一颗
     *   **不吃相位格**。⚠ 本函数读的 `CURRENT_POWER` 与卡侧紧接着读的是**同一时刻**的值
     *   （两者之间没有任何写）。*/
    int usable    = (*(const int32_t *)CE_ADDR_MAX_POWER > 0
                     && *(const int32_t *)CE_ADDR_CURRENT_POWER < *(const int32_t *)CE_ADDR_MAX_POWER) ? 1 : 0;

    return faith_core_p_topup_total(&s_p_alt, faith_core_p_alice_on(has_111, is_marisa), usable);
}

void faith_p_topup_stats(int32_t *alt, int32_t *items, int32_t *lo, int32_t *hi)
{
    if (alt)   *alt   = s_p_alt.alt;
    if (items) *items = s_p_alt.items;
    if (lo)    *lo    = s_p_alt.lo;
    if (hi)    *hi    = s_p_alt.hi;
}

void faith_p_topup_reset(void) { memset(&s_p_alt, 0, sizeof s_p_alt); }

/* ============================ 注册 ============================ */
void faith_module_reset(void)
{
    faith_clear_entities();
    memset(s_user, 0, sizeof s_user);
    s_n_user = 0;
    faith_p_topup_reset();          /* 模块整体归零 ⇒ 共享相位也跟着清 */
    s_stat_pickups = s_stat_points = s_stat_frame = s_stat_dropped = 0;
}

int faith_register(const faith_user_t *u)
{
    int i;

    if (!u || !u->slot_mask || u->card_id > (uint32_t)CE_SDK_CARD_ID_MAX) return 0;
    for (i = 0; i < s_n_user; ++i) {                    /* 同 id = 更新（幂等：ctor / 每关重进都会调）*/
        if (s_user[i].card_id == u->card_id) { s_user[i] = *u; return 1; }
    }
    if (s_n_user >= FAITH_MAX_USERS) return 0;
    s_user[s_n_user++] = *u;
    return 1;
}

void faith_unregister(uint32_t card_id)
{
    int i;

    for (i = 0; i < s_n_user; ++i) {
        if (s_user[i].card_id != card_id) continue;
        s_user[i] = s_user[--s_n_user];
        /* ★ 2026-09-15h：**最后一张消费本模块的卡也走了** ⇒ 共享相位归零（`METHOD.md` 铁律二：
         *   跨帧状态要在删卡那条路上交代清楚。「删卡后又装回来」不该带着上一局的相位）。
         * ⚠ 只在 `s_n_user == 0` 时清：110 与 111 是两张卡，删掉其中一张时另一张还在用这份相位。*/
        if (s_n_user == 0) faith_p_topup_reset();
        return;
    }
}

/* ★★ 2026-09-15（组合 UI）：在场卡的 `slot_mask` 并集（= 「哪些槽有卡」的唯一判据）。
 * 声明与理由（含「不许用槽值 0 当判据」）写在 `faith.h`。本函数**只读**注册表、不改任何状态。*/
int faith_registered_mask(void)
{
    int i, mask = 0;

    for (i = 0; i < s_n_user; ++i) mask |= s_user[i].slot_mask;
    return mask;
}

/* ============================ 池 ============================ */
/* 池**静态分区**（两个区各自紧凑，区内 swap-remove）：
 *   大档 = `s_big[0 .. s_n_big)`，容量 `FAITH_BIG_POOL`；
 *   小档 = `s_small[0 .. s_n_small)`，容量 `FAITH_SMALL_POOL`。
 * 为什么分区：大信仰点一次清场几十颗，不能把小点的位置吃掉（两张卡同装时更是如此）；
 * 而且两个区各自有独立上限 ⇒ 「大点把池挤满导致小点全丢」这种耦合不存在。
 * 统一视图（tick 里顺序推进两个区）用 `dot_at()/dot_n()` 取。*/
static faith_dot_t s_big[FAITH_BIG_POOL];
static faith_dot_t s_small[FAITH_SMALL_POOL];
static int         s_n_big;
static int         s_n_small;

static int dot_n(faith_tier_t tier) { return (tier == FAITH_TIER_BIG) ? s_n_big : s_n_small; }
static faith_dot_t *dot_at(faith_tier_t tier, int i)
{
    return (tier == FAITH_TIER_BIG) ? &s_big[i] : &s_small[i];
}

static void dot_remove(faith_tier_t tier, int i)
{
    if (tier == FAITH_TIER_BIG) s_big[i] = s_big[--s_n_big];
    else                        s_small[i] = s_small[--s_n_small];
}

void faith_clear_entities(void)
{
    faith_clear_entities_mask(~0);          /* 所有槽（模块复位；卡侧删卡请用带 mask 的那一支）*/
}

/* ★★ 2026-09-16：**把整池活着的点切成「朝自机飞」**（本池 state 3 = 引擎道具 state 3 的对位）。
 *
 * ## 为什么必须有这一支（用户 2026-09-16 实跑报的 bug）
 *   「**信仰点没有被信号满格全屏吸收（擦弹）和灵梦伊吹特技全屏吸收**」——
 *   那两条（111 的满槽全收 `faith_signal_full` 路径 / 灵梦·萃香 `th11:0x4245e0` 的对位）
 *   都只调 SDK 的 `ce_item_collect_all()`，而**那个函数只扫原生道具池**
 *   （`ItemManager` @ `0x4cf2ec`，池 +0x14、stride 0xc94 —— 见 `sdk.h` ①）。
 *   本池的点是**自建 VM**（`th18` 没有信仰点这个 item type，见 `faith.h` 的「为什么自建」）
 *   ⇒ 它们不在原生池里 ⇒ 那两条根本看不见它们 ⇒ 实跑里「绿点纹丝不动」。
 *
 * ## 一手依据（TH10 / TH11 里这两条**天然**包含信仰点）
 *   TH10 的信仰点**就是道具**（type 3 大 / type 9 小，`th10:0x41b3ce` 的 11 项跳转表），
 *   TH11 同理 ⇒ 满槽那一条（`th11:0x42364a` → `0x423805`）与萃香那一条（`th11:0x4245e0`，
 *   `ItemManager::reimu_b_autocollect`）扫的都是**同一个道具池**，信仰点当然在里面
 *   （TH11 的 `0x4245e0` 循环里判的就是 `state == 1 → 3`，与 type 无关）。
 *   ⇒ 我们补这一支**不是加机制，是把「自建实体的池」接进同一条语义**。
 *
 * `slot_mask`：收哪些槽的点（`~0u` = 全收）。一手那两条都是**全屏收**、与哪张卡无关
 *   ⇒ 三个调用点都传 `~0u`（111 满槽 / 111 萃香 / 107 森罗结界）。
 * 返回被切的数量（诊断与日志用）。⚠ 与 SDK 那个函数同口径：**只动 state，不动 pos/vel**；
 *   已经在飞（state 3）与还没建 VM（state 0）的**跳过**（= `ce_item_collect_all()` 的 `st == 0 || st == 3` 同规则）。*/
int faith_collect_all(uint32_t slot_mask)
{
    const float *pp = ce_player_pos();
    float mag_speed = FAITH_MAG_SPEED_DEF;
    int n = 0, t, i;

    if (pp) {
        mag_speed = *(const float *)(CE_PLAYER() + CE_PLAYER_ITEM_ATTRACT_SPD);
        if (mag_speed <= 0.0f) mag_speed = FAITH_MAG_SPEED_DEF;
    }

    for (t = 0; t < 2; ++t) {
        faith_tier_t tier = t ? FAITH_TIER_SMALL : FAITH_TIER_BIG;

        for (i = 0; i < dot_n(tier); ++i) {
            faith_dot_t *p = dot_at(tier, i);

            if (!(p->slot_mask & slot_mask)) continue;
            if (p->state == 0 || p->state == 3) continue;
            p->state = 3;                            /* ★ 与「过回收线 / 放 B / 对话」同一条（`faith_pool_tick` 里那一句）*/
            p->mag20 = (uint8_t)(mag_speed * 20.0f);
            if (p->mag20 > 240) p->mag20 = 240;
            if (p->mag20 == 0) p->mag20 = 1;
            ++n;
        }
    }
    return n;
}

/* 只收「认领了 `slot_mask` 里任意一位」的实体（`dot.slot_mask & slot_mask != 0`）。
 * ★ 为什么必须有这一支（2026-09-12 审计）：本池是 110（风神录）与 111（地灵殿）**共用**的，
 *   而 `on_delete` 只代表**一张卡**没了 —— 用 `faith_clear_entities()` 会把另一张卡同屏还飘着的
 *   绿点一起删掉（`signal.c` / `mof.c` 原来都这么调，两边注释里都登记了这个偏差）。
 *   每颗点在 `faith_spawn()` 里就带上了 `slot_mask`（进谁的槽），按它筛选即可只收自己家的。
 * 语义与 `dot_remove()` 的 swap-remove 一致：命中就删 VM 并把尾元素换到 `i`（**不 ++i**，尾元素也要过筛）。*/
void faith_clear_entities_mask(int slot_mask)
{
    int i;

    if (!slot_mask) return;
    for (i = 0; i < s_n_big; ) {
        if (!(s_big[i].slot_mask & slot_mask)) { ++i; continue; }
        if (s_big[i].id) ce_anm_delete((uint32_t)s_big[i].id);
        s_big[i] = s_big[--s_n_big];
    }
    for (i = 0; i < s_n_small; ) {
        if (!(s_small[i].slot_mask & slot_mask)) { ++i; continue; }
        if (s_small[i].id) ce_anm_delete((uint32_t)s_small[i].id);
        s_small[i] = s_small[--s_n_small];
    }
    /* ★★ 2026-09-15（`NEXT.md` §1x.2，删卡铁律）：**池被清空 ⇒ 立刻撤掉在飞声明**。
     * 不撤也不会错（声明按 SDK 帧号自过期，见 `sdk_core.h`），但「删卡那一刻马上停」比
     * 「2 帧后才停」更贴 `METHOD.md` 的「长持续效果必须在删卡路径里收干净」。
     * ⚠ 只在**两个区都空**时撤：110 被删而 111 的点还在飘时，声明必须继续（同一台机器两台卡共用）。*/
    if (s_n_big == 0 && s_n_small == 0) ce_vitem_drop(CE_VITEM_SRC_SHARED);
}

/* 贴图：**三档三张图**（全部 TH10 原生切图、零自绘）。不再借天空璋的季节点。
 *
 * 三条一手链（完整推导 + 可复跑命令见 `assets/ability/make_mof_art.py` 文件头 §2026-09-15 定案）：
 *   ① 类型 → 信仰值：TH10 `ItemManager::on_tick__body` th10:0x41b3ce 的 11 项跳转表
 *      `[eax*4 + 0x41b8a0]`（eax = 道具类型 − 1，类型读自 `item+0x3e0`）：
 *      **类型 3 → 0x41b57f**（+5000/8000/10000、弹窗色 `0xff00ff00`）= 大信仰点；
 *      **类型 9 → 0x41b5fc**（+100、同色）= 小信仰点；**类型 8 → 0x41b5d6**（+10）= 微信仰点。
 *   ② 类型 → 脚本号：TH10 `ItemManager::spawn_item` th10:0x41bca8 `lea eax,[ebp+0x176]`
 *      ⇒ 脚本 = 类型 + 374（0xA→375 / 0xB→378 两条特例）。
 *   ③ 脚本 → sprite 矩形：`thanm -l 10 bullet.anm -m v4.anmm` 的正文
 *      `script377{sprite(sprite346)}` / `script383{sprite(sprite352)}` / `script382{sprite(sprite351)}`，
 *      与 `on_draw__body` th10:0x41b8e0 的 `lea edx,[eax+0x157]`（343 + 视觉类型）两条独立对上。
 *   像素：`bullet/etama2.png`（`bullet.anm` entry2）—— 大 `sprite346 (32,208,16,16)`、
 *      小 `sprite352 (112,208,16,16)`、微 `sprite351 (112,208,16,16)`。
 *      ⚠️ **小 / 微 在 TH10 里共用同一个矩形**（两个 sprite 号字面同值 = ZUN 自己的重号），
 *      TH11 也一样（`script99/105/104` → `sprite347/353/352`）⇒ **原版没有任何一作给微档单独画图**。
 *
 * ★ 2026-09-15b：**微档换成它自己的第三张图**（`FAITH_DOT_MICRO.png` = `(240,208,16,16)`）。
 *   取的是同一张 `etama2.png` 上 ZUN 画的**第二套绿点**：`on_draw__body` th10:0x41b8e0 对新出的
 *   sprite 有**两族** —— `vtype+343`（画用 y ≥ 8.0，可见档）与 `vtype+352`（画用 y < 8.0 那一支）；
 *   后者里 **vtype 8 / 9（微 / 小信仰点的视觉类型）= `sprite360` / `sprite361` = `(240,208)`**
 *   （同族 vtype 3 那格 `sprite355 (160,208)` 与它**逐像素只差 2 个像素** = 同一张素色绿点）。
 *   ⇒ **像素 100% 原版**、**sprite 号也是一手的**；❓**自定**的只有「把它指给微档」这一步
 *   （原版可见分支给 type 8 的仍是 `(112,208)`，即与小档同图 —— 原版没有三档图，这层映射只能我们定）。
 *   登记 `TUNING.md` §2.6。三档因此**三图各不相同**：16px 大绿花 / 12px 绿珠 / 10px 素绿点。
 *   ⚠️ **微档不建实体**（见文件头 §❓ 自定）⇒ 这一支今天是**备用**的，等微档实体的 TODO 落地才显形。*/
static int faith_sprite_for(faith_tier_t tier)
{
    switch (tier) {
    case FAITH_TIER_BIG:   return CE_ANM_ABILITY_SPRITE_FAITH_DOT_BIG;    /* 类型 3（script377 / sprite346）*/
    case FAITH_TIER_MICRO: return CE_ANM_ABILITY_SPRITE_FAITH_DOT_MICRO;  /* 类型 8（script382 / sprite360/361）*/
    case FAITH_TIER_SMALL:
    default:               return CE_ANM_ABILITY_SPRITE_FAITH_DOT;        /* 类型 9（script383 / sprite352）*/
    }
}

/* 每档的出生脚本：**微档与另两档不同**（照 TH10 原文）。
 *   大 / 小 = 脚本 272（TH10 script377；TH10 的 `script383` 与它**逐字相同** ⇒ 一条就够）；
 *   微     = 脚本 273（TH10 script382：多 `blendMode(1)` + `alpha(16)`，且 `scale(0→1)` 而不是 0.5→1）。
 * ⇒ 这是三档的**第二层**区分度（第一层是贴图），对照表见
 *   `assets/ability/scripts/273_mof_faith_dot_micro.anm.txt` 的文件头。*/
static int faith_script_for(faith_tier_t tier)
{
    return (tier == FAITH_TIER_MICRO) ? CE_ANM_ABILITY_SCRIPT_MOF_FAITH_DOT_MICRO
                                      : CE_ANM_ABILITY_SCRIPT_MOF_FAITH_DOT;
}

/* 顶点色：**白 = 不染色**（alpha 255）。
 * ★ 2026-09-15：原来的 `❓ 自定` 染色（亮绿 / 浅绿，为了把季节点染成绿点）**整段删掉** ——
 *   TH10 的信仰点贴图自己就是绿的（`(32,208)` 的那一格平均色 ≈ RGB(118,192,124)），
 *   再乘一层绿会把原生美术压暗、也失去「大/小两格本来的差别」。
 *   显式写 0xffffffff 是为了确定性（不依赖模板 VM 的初值），不是染色。
 * ★ 2026-09-15b：**微档 alpha = 16**（= TH10 script382 的 `alpha(16)`，那个「加算淡光球」）。
 *   为什么 C 侧也写一遍：TH18 的顶点色有**两路**寄存器、由 `colorMode` 决定哪一路生效
 *   （`th18:0x47e202`：mode 0 → color_1、mode 1 → color_2），而 `color()` / `alpha()` 分散写在两路上
 *   （`alpha()` 只写 color_2 的 A）⇒ **只靠脚本定不住最终 alpha**。本仓既有口径是「卡侧把两路写成
 *   同一个值」（`trance.c` 的 `wipe_set_dual_color`，2026-09-14 实跑逼出来的）⇒ 这里照同一口径
 *   写两路：大/小 = `0xffffffff`（不透明，与旧行为**逐字节等价**），微 = `0x10ffffff`。
 * ⚠ `FAITH_TINT_NONE` 的定义在**文件上方**（§1x.4 那一块，`faith_dot_slow_style()` 的兜底分支也用
 *   它）—— 语义只有一份：「白 = 不染色」。*/
#define FAITH_TINT_MICRO  0x10ffffffu   /* A = 16/255 ≈ 0.06（= TH10 script382 的 alpha(16)）*/

static uint32_t faith_tint_for(faith_tier_t tier)
{
    return (tier == FAITH_TIER_MICRO) ? FAITH_TINT_MICRO : FAITH_TINT_NONE;
}

static int dot_spawn_vm(faith_dot_t *p)
{
    /* ★ 2026-09-15：**从「季节点脚本」换成信仰点自己的脚本**。
     *   旧代码借的是 `SEASON_POINT`（116 的脚本 115 = TH16 `bullet.anm` script129）⇒ 贴图也是
     *   季节点那格（用户实跑报「拿的是天空璋的季节点」）。现在**按档**取：大/小 → 脚本 272
     *   （= TH10 `bullet.anm` script377 的逐条转写）、微 → 脚本 273（= script382 的逐条转写）；
     *   sprite 与顶点色同样按档给（`faith_sprite_for()` / `faith_tint_for()`）。*/
    const faith_tier_t tier = (faith_tier_t)p->tier;
    const uint32_t argb = faith_tint_for(tier);
    uint32_t id = ce_anm_spawn(CE_ABILITY_ANM(), faith_script_for(tier), FAITH_LAYER);

    if (!id) return 0;
    ce_anm_set_pos(id, p->x, p->y, 0.0f);
    ce_anm_set_sprite(CE_ABILITY_ANM(), id, faith_sprite_for(tier));
    ce_anm_set_color(id, argb);          /* color_1（`+0x524`）*/
    ce_anm_set_color2(id, argb);         /* color_2（`+0x528`）—— 两路都写才与 colorMode 无关 */
    p->id = (int32_t)id;
    return 1;
}

int faith_spawn(float x, float y, faith_tier_t tier, int slot_mask, int32_t value)
{
    faith_dot_t *p;
    float ang, spd, vel[3];

    if (!slot_mask || value <= 0) return 0;
    if (tier == FAITH_TIER_MICRO) return 0;      /* 微档不建实体（见文件头 ❓ 自定）*/
    if (tier != FAITH_TIER_BIG && tier != FAITH_TIER_SMALL) return 0;
    if (dot_n(tier) >= (tier == FAITH_TIER_BIG ? FAITH_BIG_POOL : FAITH_SMALL_POOL)) {
        ++s_stat_dropped;                        /* 池满：丢弃（不做待撒队列 —— 量级不需要）*/
        return 0;
    }

    /* ★ E-2（2026-09-15）：出生运动学 = **一手实参**，不再随机 ——
     * 角恒 −π/2（`0xbfc90fdb`，直上）、速按档（`faith_core_throw_speed()`：2.2 / 2.2 / 0.6）。
     * 撒点位置由调用方给，**一律是实体自己的位置**（道具 / 弹原位；见 `faith_core.h` §E-2）。*/
    ang = FAITH_THROW_ANGLE;
    spd = faith_core_throw_speed(tier);

    p = dot_at(tier, dot_n(tier));
    memset(p, 0, sizeof *p);
    p->id = 0;                                   /* VM 在本帧的 `faith_pool_tick()` 里建（不在别人的回调栈里建）*/
    p->x = x;
    p->y = y;
    p->lang = ang;
    p->lr = spd;
    ce_polar_to_vel(vel, ang, spd);              /* 引擎自己的 FSINCOS，不引 libm */
    p->vx = vel[0];
    p->vy = vel[1];
    p->state = 0;
    p->mag20 = 0;
    p->age = 0;
    /* ★ 2026-09-15（同 E-2）：**不再错帧**（旧值是 `(s_spawn_seq & 3) + 1` = 出生后 1..4 帧才建 VM /
     * 才受物理 —— 那等于又造了一道「出生保护」）。一手：spawn 出来的道具出生即在 state 1
     * （`th18:0x446fac`）立刻参与收点判定 ⇒ 我们同样 **0 帧等待**：
     * 出生 → 本帧的池 tick 就建 VM、就受物理与收集判定（`faith_pool_tick()` 里那一支会**落到**物理段）。*/
    p->delay = 0;
    p->tier = (uint8_t)tier;
    p->slot_mask = slot_mask;
    p->value = value;
    if (tier == FAITH_TIER_BIG) ++s_n_big; else ++s_n_small;
    return 1;
}

int faith_spawn_big(float x, float y, int32_t value_110)
{
    return faith_spawn(x, y, FAITH_TIER_BIG, 1 << FAITH_SLOT_110, value_110);
}

/* ==================== ★★ 满 P 全场转换（A + B 的同一台机器；一手见 faith.h 的长注）====================
 *
 * 一手两个落点（TH10 + TH11 各一份）讲的是同一条不变式 —— **满 P 期间场上不存在 P 类道具**：
 *   · 出生即替换：`th10:0x41bbf8`（spawn_item 内，满 P ⇒ type 1/4 → 9）；
 *   · 满的那一帧全场转换：`th10:0x41ba50` / `th11:0x4240f0`（原位重生 + 角 −π/2 + 速 2.2）。
 * 我们照这条不变式实现成**一条每帧规则**（满 P 就扫一遍道具池）。
 *
 * ⚠ 与一手的**唯一**已知差异：原版在 spawn_item 里改 type ⇒ 转换点**不经过任何一帧的 P 状态**；
 *   我们是「出生后在本帧/下一帧的池 tick 被扫到」⇒ 那颗 P 道具可能**存在不到一帧**
 *   （视觉上看不见、玩家也吃不掉 —— 引擎的道具 tick 与我们的池 tick 之间的窗口）。
 *   真要一样就得插一个新的 spawn_item 断点（`patch` 那一层不归本卡改）⇒ 登记在报告与 `TUNING.md`。
 *
 * 扫描口径**与引擎自己的道具 tick 完全一致**（`th18:0x445a80` 的循环：`edi = mgr+0x14`、
 * 步长 `0xc94`、`cmp eax,0x1258`）⇒ 4696 槽**一次连续走完**（前 600 槽 = 普通道具池
 * 自由表 `mgr+0xe6baf4`、后 4096 槽 = 状态 5 族（type 9–0xe）自由表 `mgr+0xe6bb04` —— 见构造函数
 * `th18:0x42aa3c` 的 0x258 与 `0x42aade` 的 0x1000；两池结构体同布局、state/type 同偏移）。*/

/* 道具的 anm VM id —— `engine.h` 没收这个偏移（本卡不许改 engine.h）⇒ 写在这里，出处：
 * 收集清尾 `th18:0x44659f push [edi+0xc28]` + `0x4465aa mov [edi+0xc28],0`（出界删 `0x445e42`/`0x445e4d` 同构）；
 * spawn 里 `0x44710b mov [esi+0xc28],0`。同款先例 = `cards/ufo.c` 的 `UFO_ITEM_ANM_ID`。*/
#define FAITH_ITEM_ANM_ID   0xc28

/* 删一颗原生道具：**照引擎自己的清尾**（收集清尾 `th18:0x446595` 起 / 出界删 `th18:0x445e38` 起，
 * 两条路径逐指令同构）：
 *     [item+0xc74] = 0                    ; state = 0（= 空槽；道具 tick 的循环头 `0x445ae8 je` 靠它跳槽）
 *     push [item+0xc28]; call 0x488cf0     ; 删这只 anm VM（= `engine.h` 的 CE_FN_ANM_DELETE_BY_ID / `ce_anm_delete`）
 *     [item+0xc28] = 0
 *     ＋ freelist LRU 前插（`0x4465b4`–`0x4465d2`）
 * ⚠ 最后那步**不能省**：spawn（`th18:0x446f95` `mov esi,[ecx+0xe6baf8]`）是从这条链上取槽的，
 *   而且取走时用 `0x404f20`（通用双链表摘挂）把它摘下来 ⇒ 被删的槽若不前插，就**回不到分配链**
 *   （普通池只有 600 槽 ⇒ 池会越用越干）。
 * ⚠ 逐指令写法与 `cards/ufo.c` 的 `ufo_item_relink()` 同形（那份文件头 ④ 有完整对照）；
 *   两个 NULL 守卫是**防御**（引擎那两条无守卫，head 为空时它自己会崩）。*/
static void faith_item_delete(uint8_t *it)
{
    uint8_t *owner, *next;
    uint32_t anm;

    if (!it) return;
    anm = *(uint32_t *)(it + FAITH_ITEM_ANM_ID);
    *(int32_t *)(it + CE_ITEM_STATE) = 0;
    if (anm) ce_anm_delete(anm);
    *(uint32_t *)(it + FAITH_ITEM_ANM_ID) = 0;

    owner = *(uint8_t **)it;                 /* item+0 = 池的自由表所有者结点（普通池 = mgr+0xe6baf4）*/
    if (!owner) return;
    next = *(uint8_t **)(owner + 4);         /* ★ 是**头的** next，不是 *(it+4)（ufo.c 踩过一次的坑）*/
    if (next) {
        *(uint8_t **)(it + 4) = next;
        *(uint8_t **)(next + 8) = it;
    }
    if (*(uint8_t **)(owner + 0xc)) *(uint8_t **)(owner + 0xc) = it;
    *(uint8_t **)(owner + 4) = it;
    *(uint8_t **)(it + 8) = owner;
}

/* 诊断（静态区）：累计转换过的原生道具颗数 + 撒失败（池满）次数。*/
static int32_t s_stat_converted, s_stat_conv_dropped;

int faith_convert_full_p_items(void)
{
    static uint32_t s_last_frame;
    static int      s_have_last;
    uint8_t *mgr;
    uint32_t now;
    int i, u, n = 0;

    /* ① 只在满 P 时干活（一手：`th10:0x41bbf7 cmp WORD ds:0x474c48,0x64` 的同一判据在 TH18 侧
     *    = `CURRENT_POWER >= MAX_POWER`；`max_power <= 0` = 引擎还没初始化 ⇒ 不碰）。*/
    if (FAITH_MAX_POWER() <= 0 || CE_CURRENT_POWER() < FAITH_MAX_POWER()) return 0;

    /* ② 帧守卫（与 `faith_pool_tick()` 同款）：110 与 111 同装时各调一次 ⇒ 本函数一帧只跑一遍。*/
    now = ce_sdk_frame();
    if (s_have_last && now == s_last_frame) return 0;
    s_last_frame = now;
    s_have_last = 1;

    mgr = CE_ITEM_MANAGER();
    if (!mgr) return 0;

    for (i = 0; i < CE_ITEM_SLOTS; ++i) {
        uint8_t *it = mgr + CE_ITEM_POOL_OFF + (size_t)i * CE_ITEM_STRIDE;
        float px, py;

        if (*(const int32_t *)(it + CE_ITEM_STATE) == 0) continue;              /* 空槽 */
        if (!faith_core_is_p_item_type(*(const int32_t *)(it + CE_ITEM_TYPE))) continue;
        /* ③ 原位：先读位置再删（一手 `th10:0x41bac1` 的 `ecx` = 道具自己的 pos float3）*/
        px = *(const float *)(it + CE_ITEM_POS);
        py = *(const float *)(it + CE_ITEM_POS + 4);
        faith_item_delete(it);
        ++n;
        /* ④ 给**每个**认领了转换通道的注册者在本槽撒一颗（E-1 硬约束：满 P 转换只许 type 9 档）。
         *    值由注册表给（110 = 100 / 111 = 10）⇒ 本模块不需要知道是谁。*/
        for (u = 0; u < s_n_user; ++u) {
            if (s_user[u].conv_value <= 0) continue;
            if (!faith_spawn(px, py, faith_core_tier_for_source(FAITH_SRC_FULL_P),
                             s_user[u].slot_mask, s_user[u].conv_value)) {
                ++s_stat_conv_dropped;
            }
        }
        /* ★★ 2026-09-18（B12）：**每转换一颗 P 道具响一声**、声像 = 该道具的 x ——
         * 一手 TH10 `0x41bad2`（转换循环内 `push 0x189` → `call 0x448db0`）；TH11 同款。
         * ⚠ 只播**每颗道具一次**（不是每个注册者一次）：两卡同装时是一颗道具被转一次。
         * id 是 ❓自定，见 `FAITH_CONV_SE` 的长注。*/
        ce_play_sound(FAITH_CONV_SE, px);
    }
    if (n > 0) {
        s_stat_converted += n;
        if (n >= 4) {                                            /* 限流：一颗一颗的转换不逐条刷日志 */
            ce_log("faith: 满 P 转换 %d 颗原生 P 道具（累计 %d，撒失败 %d）",
                   n, (int)s_stat_converted, (int)s_stat_conv_dropped);
        }
    }
    return n;
}

/* ============================ 收集 ============================ */

/* ★★ 2026-09-15（`NEXT.md` §1x.2）：本池的档 → SDK「虚拟收集 / 虚拟在飞」层的**类别**
 * （`sdk_core.h` 的 `ce_vitem_kind_t`）。**只有微档是半道具** ——
 * 用户 2026-09-15 定案（§1x.3）：「微档不计入，因为它是消弹点，算是某种**半道具**，
 * 所谓的实体 ≈ **只是为了动画**」⇒ 它不入 114 的批、也不算「有东西在飞」。
 * ⚠ 微档今天**根本不建实体**（`faith_spawn()` 对微档返回 0，见文件头 ❓ 自定）⇒ 这道门此刻是
 *   **双保险**：将来按 §2.5c #10 补上微档实体时，它天然不入批，不用再改这里。*/
static int faith_vitem_kind(faith_tier_t tier)
{
    return (tier == FAITH_TIER_MICRO) ? CE_VITEM_HALF : CE_VITEM_FULL;
}

/* ★★ 2026-09-15（§1x.2，与 114 的两条通道）：**大 / 中档**（`CE_VITEM_FULL`）——
 *   ① 被收的那一刻报一颗（成员通道 → 114 的批计数 +1，**不进 SCORE / 不进批分**）；
 *   ② 池里还有活着的点时每帧声明在飞数（presence 通道 → 114 的批不提前结算）。
 * 两条都在下面：①在 `dot_collect()`、②在 `faith_pool_tick()` 尾部。
 *
 * ★★ 2026-09-18（B1）：**两条通道都收窄到「真空飞行」（`state == 3`）** —— 判据统一走
 * `faith_core_state_is_vacuum_flight()`（`faith_core.h` 那张「本池 state ↔ 引擎 state」对照表 +
 * 用户 2026-09-18 的规格：信仰点必须与 P 点 / 金钱**完全一致** = 开批 / 计数 +1 / 飞行中组织批次结算）。
 * 引擎对 P / 钱只认 `item+0xc74 == 3`（§Z22）⇒ 飘落（本池 1）/ 飞散（本池 2）/ 70px 环吸（本池 4）
 * 被收**不入批、也不续批** —— 那是「普通游玩顺手捡一颗」，引擎那边同样不开批。
 * ⚠ 旧实现（2026-09-15）**不看状态**（那时的开关 `FAITH_VITEM_NOTE_VACUUM_ONLY = 0`）⇒ 实跑两症状：
 *   「绿点一直触发收点」「boss 战期间 bonus 对着自己掉的 P 点触发」。该开关**已删**（只剩一条路）。*/

/* ★★ 2026-09-16：**收尾改成「一手形状」= 原位播 `se_item00` + 立刻删 VM，没有收尾动画**。
 * 用户实跑反馈逐字：「拾取到大信仰点，吸收那一刻动画是一个放大减淡，或许 th10 原生是这个，
 *  但是和 th18 的吸收 p 点金钱的动画不符」——**用户是对的，而且一手连 TH10 都是「直接删」**：
 *   · TH18 一手（收集分派的**共享尾**，所有 type 都走：`th18:0x446584`–`0x4465a5`）：
 *       `lea esi,[edi+0xc2c]`（道具坐标）→ `movss xmm2,[esi]` → `push 0x25` →
 *       `call 0x476c70`（**带位置播音** = `se_item00`，`engine/_shared/th18-sound-table.md` 0x25 行）
 *       → `mov [edi+0xc74],0`（**state 立刻归 0**）→ `call 0x488cf0`（**释放道具的 anm VM**）。
 *       ⇒ 原生**没有**脚本级收尾动画（也不给道具 VM 发任何中断 —— 本函数 0x44625f–0x446584 里
 *         对 `[edi+0xc28]` 只有那一处释放）。
 *   · TH10 同形（`assets/ability/scripts/272_mof_faith_dot.anm.txt` 的文件头原本就写着
 *     「TH10 的道具被吃到时是**引擎直接删 VM**」）⇒ 我们原来写的那段「加算 + 放大 1.8× +
 *     淡出 8 帧」是从**别的卡的实体脚本**（115 季节点 / 114 种子 / 101 豌豆）借形状借来的
 *     ❓ 自定，不是任何一作的原样 —— 已删（脚本 272/273 的 `interruptLabel(1)` 段一起去掉）。
 * ⚠ 换成「立刻删」之后本池的记账不受影响：调用点 `faith_pool_tick()` 里
 *   `dot_collect(p); dot_remove(tier, i);` 本来就是**当场回收槽**（不依赖脚本 8 帧后自删）。*/
#define FAITH_PICKUP_SE   0x25   /* TH18 道具收集音 `se_item00`（一手 `th18:0x44658e push 0x25`）*/

static void dot_collect(faith_dot_t *p)
{
    int i;

    ce_play_sound(FAITH_PICKUP_SE, p->x);        /* ★ 一手：`xmm2 = pos.x`、原位播（`th18:0x44658a`）*/
    if (p->id) {
        ce_anm_delete((uint32_t)p->id);          /* ★ 一手：收下就删（`th18:0x446595` + `0x44659f`）*/
        p->id = 0;
    }
    for (i = 0; i < s_n_user; ++i) {
        if (!(s_user[i].slot_mask & p->slot_mask)) continue;      /* 只回调认领该槽的卡 */
        /* ★ 2026-09-15：`on_pickup_at`（带位置）**优先**，两者是二选一（不是叠加）
         *   ⇒ 旧注册者（111）不设它 ⇒ 走的还是 `on_pickup`，行为逐字节不变。*/
        if (s_user[i].on_pickup_at) {
            s_user[i].on_pickup_at(s_user[i].user, p->slot_mask, (int)p->tier, p->value, p->x, p->y);
        } else if (s_user[i].on_pickup) {
            s_user[i].on_pickup(s_user[i].user, p->slot_mask, (int)p->tier, p->value);
        }
    }
    /* ★★ 成员通道（**一台机器只报一次**：不是每个注册者各报一次 ⇒ 110 与 111 同装时同一颗共用池
     * 的点天然去重，`sdk_core.h` 契约 ⑤。位置在「谁被回调」之外 —— 就算池里一颗点都没被卡认领
     * （`s_n_user == 0`），它仍然是「被收了一颗点」）。
     * ★★ 2026-09-18（B1）**门**：只有**真空飞行中被收**（`state == 3` = 过回收线 / 放 B / 对话 ⇒
     *   朝自机飞，引擎 `item+0xc74 == 3` 的同一谓词、§Z22）才报 —— 与 P 点 / 金钱**完全一致**：
     *   飘落 / 飞散中顺手捡（state 1/2）与 70px 环吸被收（state 4）**不开批、不计数**
     *   （引擎那边这两条路同样不进批，见 `collect2x.c` 的成员门）。判据全文见 `faith_core.h` 的对照表。
     *   诊断：两类各自的累计数每 600 帧随 `faith:` 汇总行打一次（实跑对账「哪些颗进了批」用）。*/
    if (faith_core_state_is_vacuum_flight(p->state)) {
        /* ★★ 2026-09-18（B11）：**只报颗数、不报值** —— 用户口径：「绿点的面值不进 `batch_score`，
         * 它加的不是分数」（绿点加的是信仰、不是 SCORE）⇒ 114 把它算作**批成员**（计数 +1），
         * 批分里只有原生 P/钱的小分；纯绿点批的补差恒 0 是设计。*/
        ce_vitem_note_collect(faith_vitem_kind((faith_tier_t)p->tier), 1);
        ++s_stat_vitem_member;
    } else {
        ++s_stat_vitem_skip;
    }
    ++s_stat_pickups;
    s_stat_points += p->value;
}

void faith_cancel_account(int32_t bullets_cancelled)
{
    int i;

    if (bullets_cancelled <= 0) return;
    for (i = 0; i < s_n_user; ++i) {
        if (s_user[i].on_cancel) s_user[i].on_cancel(s_user[i].user, bullets_cancelled);
    }
}

/* ============================ 每帧 ============================ */
/* faith_player_poc_height() 在 faith.h 里（static inline，见那里的注释）。*/

int faith_player_above_line(void)
{
    const float *pp = ce_player_pos();
    uint8_t *pl = CE_PLAYER();
    int pstate;

    if (!pp || !pl) return 0;
    /* 原版的门：自机 state ∈ {2,4}（死亡 / 复活）时不看回收线（`0x445ba3`/`0x445ba8`）。*/
    pstate = *(int32_t *)(pl + CE_PLAYER_STATE);
    if (pstate == 2 || pstate == 4) return 0;
    return faith_player_poc_height() > pp[1];
}

/* ★★ 2026-09-16：**引擎道具「真空飞行」门的完整对位**（一手：`th18:0x445b9a`–`0x445be0` 是状态 1 的那处、
 * `0x445fe4`–`0x44602a` 是状态 4 的那处 —— **两处逐条同形**，所以本池只写一处、两种态共用）：
 *   ① 自机 `state == 2 || state == 4`（死亡中 / 被判中弹）`0x445ba0`/`0x445ba5`
 *   ② 自机 y < 收点线（`player+0x47998`）`0x445baa`–`0x445bb9`
 *   ③ 放 B 且帧钟 < 60（bomb_mgr+0x30 == 1 ∧ +0x38 < 0x3c）`0x445bc4`–`0x445bce`
 *   ④ 对话 VM 非 0（`Gui+0x1b0`）`0x445bd4`–`0x445be0`
 * 命中 ⇒ 道具 state → 3（`0x445f63` = 真空飞行的入口）。
 * ⚠ 本卡原来少了 ①（`faith_player_above_line()` 里那两条是**反向**用的：state 2/4 时它返回 0，
 *   也就是「不看线」—— 正是 ① 的另一半），而且只在 `state == 1` 判 ⇒ 2026-09-16 一并补全。
 * ★★ 2026-09-18（B11）：① **又去掉了**（用户裁决）—— 引擎含它是 TH18 的死亡回收，
 *   而本池的点在死亡 / 复活期不该被吸走（实跑：一次死亡把 54 颗绿点一次吸进 114 的批）。
 *   判据与理由见函数体上方那段注释。*/
/* ★★ 2026-09-18（B11，用户裁决）：**去掉「自机 state 2/4」那一条** ——
 *   引擎（TH18）的道具真空门确实含它（一手 `0x445ba0`/`0x445ba5`：「死亡中 / 复活中 ⇒ 当在线上」），
 *   那是 TH18 自己的**死亡回收**机制；但用户口径是：**绿点（TH10/TH11 的信仰点）在那一刻不该被
 *   这台机器吸走** —— 实跑症状 = 一次死亡把全场 54 颗绿点一次吸进 114 的批（`×1.6、分数 0`）。
 *   ⇒ 本池只保留另外三条（过回收线 / 放 B 前 60 帧 / 对话管理器）；死亡与复活期**不吸**。
 *   ⚠ 有意的**原版口径差**：TH18 原生 P/钱在死亡时照旧被引擎吸走（那是引擎自己的行为，我们不干预，
 *     它也因此照旧入批、批分非零）；本条只作用于**本池的自建点**。
 *   ⚠ 另外三条的**全屏收点**路径不受影响：`faith_collect_all()`（111 满槽 / 萃香 / 107 森罗结界）
 *     是显式调用，与这道门无关。*/
static int faith_vacuum_gate(void)
{
    return faith_player_above_line() || CE_BOMB_COLLECTING() || CE_GUI_MSG_ACTIVE();
}

int faith_enemies_present(void)
{
    uint8_t *em = CE_ENEMY_MGR();
    return (em && *(int32_t *)(em + CE_EM_ENEMY_COUNT) > 0) ? 1 : 0;
}

int faith_pool_tick(void)
{
    uint8_t *pl = CE_PLAYER();
    /* ★★ 2026-09-13 补（跨卡互作审计发现 #3）：**帧守卫**。
     *   这个池是 110/111 **共用**的（`faith_register` 按 slot_mask 分账），而两个卡各自在自己的
     *   tick 里调一次 `faith_pool_tick()` ⇒ 两卡同装时**同一帧推进两次**：
     *   `age++` 两次（出生帧数被算重）、`delay--` 两次、物理积分两次（下落/磁吸翻倍）、
     *   吸引与收集判定各多跑一遍。
     *   ★ 同款守卫也长在 `faith_convert_full_p_items()` 上（否则两卡各自扫一遍池 = 同一帧转两轮）。
     *   ⇒ 用「本帧已跑过就返回」的守卫；帧号取引擎的当前帧计数（与卡侧 `frame` 同源，见 `sdk.h`）。
     *   ⚠ 守卫必须**先于一切副作用**（含下面那段读 `ITEM_SLOWDOWN` 的只读代码之后的写动作）。*/
    {
        static uint32_t s_last_frame;
        static int      s_have_last;
        const uint32_t now = ce_sdk_frame();
        if (s_have_last && now == s_last_frame) return 0;
        s_last_frame = now;
        s_have_last = 1;
    }
    const float *pp = ce_player_pos();
    float mag_speed = FAITH_MAG_SPEED_DEF;
    float collect_r2 = FAITH_COLLECT_R_DEF * FAITH_COLLECT_R_DEF;
    float ring_r2 = FAITH_ATTRACT_R_DEF * FAITH_ATTRACT_R_DEF;
    float slow = 1.0f;
    int collected = 0, t, i;
    int vacuum_in_flight = 0;   /* ★★ 2026-09-18（B1）：本帧处于真空飞行的点数（presence 通道的真值）*/

    {   /* 与季节点同一口径：引擎的道具减速因子（绀珠传卡 115 会写它）⇒ 同一屏上绿点跟着一起慢。
         * ★ 2026-09-15d（§1x.4）：读法提炼进 `faith_item_slowdown_permille()`（同一个只读函数也供
         *   `faith_dot_slow_style()` 的调用方用；语义与「为什么返回整数」见那一支的注释）。*/
        const int pm = faith_item_slowdown_permille();
        if (pm < FAITH_SLOW_NONE) slow = (float)pm * 0.001f;
    }
    if (pl) {
        float r;
        mag_speed = *(float *)(pl + CE_PLAYER_ITEM_ATTRACT_SPD);
        if (mag_speed <= 0.0f) mag_speed = FAITH_MAG_SPEED_DEF;
        r = *(float *)(pl + CE_PLAYER_ITEM_COLLECT_R);
        if (r > 0.0f) collect_r2 = r * r;
        r = *(float *)(pl + (CE_PLAYER_FOCUSED() ? CE_PLAYER_ITEM_ATTRACT_RF
                                                 : CE_PLAYER_ITEM_ATTRACT_RU));
        if (r > 0.0f) ring_r2 = r * r;
    }

    /* 每 600 帧一条汇总（诊断；实跑时对「哪些敌人掉了、丢了多少」用）。
     * ★ 2026-09-15 起多一项 `conv` = 满 P 转换掉的原生 P 道具颗数（A/B 的实跑对账用：
     *   满 P 打怪时它应该**颗颗不落**，而场上不该再出现 P 点）。*/
    if (s_stat_frame % 600 == 599 &&
        (s_stat_pickups || s_stat_dropped || s_stat_converted || s_stat_vitem_skip)) {
        /* ★★ 2026-09-16 加：**吸引速度对账**（用户实跑：「伊吹特技的信仰点和 P 点金钱吸收动态不一致，
         *   信仰点快了」）。我们的三个常量**已经与一手逐值相同**（`faith.c` 的
         *   `FAITH_MAG_STEP` = 0.2 = `th18:0x4b90f0`、`FAITH_MAG_MAX` = 12.0 = `th18:0x4b927c`、
         *   翻 3 时 `item+0xc80 = player+0x47988` = `th18:0x445f63`、环吸 = 该值 ÷3 = `0x44663b`-`0x44651`）
         *   ⇒ 剩下的差异只可能在**入场时机/当时的 mag 值**上 ⇒ 把同一帧的「自建点速度」与
         *   「原生池里某个 state-3 道具的速度」并排打出来，一次实跑就能看出谁快、差多少。*/
        int it_speed10 = -1, it_state = -1;
        {
            uint8_t *mgr = CE_ITEM_MANAGER();
            unsigned k;
            for (k = 0; mgr && k < 256; ++k) {          /* 只扫前 256 槽（诊断用；池 4696 槽）*/
                uint8_t *it = mgr + CE_ITEM_POOL_OFF + (size_t)k * CE_ITEM_STRIDE;
                int32_t st = *(int32_t *)(it + CE_ITEM_STATE);
                if (st == 3) {
                    it_state = (int)st;
                    it_speed10 = (int)(*(const float *)(it + CE_ITEM_MAG_SPEED) * 10.0f);
                    break;
                }
            }
        }
        ce_log("faith: pickups %d, points %d, dropped %d, conv %d (big %d / small %d alive) | "
               "vitem member %d skip %d | speed x10: dot_big %d dot_small %d native3 %d(state %d)",
               (int)s_stat_pickups, (int)s_stat_points, (int)s_stat_dropped, (int)s_stat_converted,
               s_n_big, s_n_small,
               (int)s_stat_vitem_member, (int)s_stat_vitem_skip,
               s_n_big   ? (int)((float)s_big[0].mag20)   : -1,
               s_n_small ? (int)((float)s_small[0].mag20) : -1,
               it_speed10, it_state);
        s_stat_pickups = s_stat_points = s_stat_dropped = 0;
        s_stat_converted = 0;
        s_stat_vitem_member = s_stat_vitem_skip = 0;
    }
    ++s_stat_frame;

    for (t = 0; t < 2; ++t) {
        faith_tier_t tier = t ? FAITH_TIER_SMALL : FAITH_TIER_BIG;

        for (i = 0; i < dot_n(tier); ) {
            faith_dot_t *p = dot_at(tier, i);
            float x, y, vx, vy;

            if (p->state == 0) {                             /* 出生帧：建 VM，然后**落到**下面的物理/收集段 */
                if (p->delay > 0) { --p->delay; ++i; continue; }
                if (!dot_spawn_vm(p)) { dot_remove(tier, i); continue; }
                p->state = 2;                                /* → 飞散（一手：出生的道具立刻参与收点判定）*/
                /* ★ 2026-09-15：这里原来 `++i; continue;`（建完 VM 那一帧不动 = 又一道 1 帧的「出生保护」）。
                 * 现在**落到**下面：本帧就上物理、就被收（用户验收「站在回收线上撒点：新点当帧就被吸走」）。*/
            }
            x = p->x; y = p->y; vx = p->vx; vy = p->vy;

            if (!p->id || !ce_anm_get_vm((uint32_t)p->id)) {  /* 脚本已 delete（超时）→ 回收槽 */
                dot_remove(tier, i);
                continue;
            }
            ++p->age;

            /* 回收线 / 放 B / 对话 ⇒ 真空飞行（初速 = 磁速）。
             * ★ 2026-09-15：原来这里（以及下面两处）还有 `p->age >= FAITH_INTANGIBLE`（32 帧）——
             *   整条删掉（见文件头与 `faith.h`：TH18 原生道具没有出生保护）。
             * ★★ 2026-09-16（用户「检查一下进对话的全屏吸收」的复核结果）：**门与作用态都补全了**：
             *   · 门少第一条（自机 state 2/4）⇒ 见 `faith_vacuum_gate()` 的一手；
             *   · 作用态只有 `state == 1` —— 一手两处（`0x445b9a` 状态 1、`0x445fe4` 状态 4）
             *     都是「非 0 非 3 就查」，而本池的两套段号**不可逐值对照**（本池 state 2 = 引擎 state 1
             *     的「出生上抛」，见文件上方 §一）⇒ 这里改成 `state ∉ {0, 3}`（= 与 `faith_collect_all()`
             *     同一条规则），否则「刚击破撒出的点在上抛期进对话/上线」不会被吸。*/
            /* ★★ 2026-09-18（B12）：**上抛期豁免真空门**（见 `FAITH_TOSS_FRAMES` 的一手说明）——
             * 让「从 P 点位置上抛」这一段在画面上真的存在，抛完才受回收线 / 放 B / 对话支配。*/
            const int in_toss = (p->state == (uint8_t)FAITH_STATE_SCATTER && p->age < FAITH_TOSS_FRAMES);
            if (p->state != 0 && p->state != 3 && pl && !in_toss) {
                if (faith_vacuum_gate()) {
                    p->state = 3;
                    p->mag20 = (uint8_t)(mag_speed * 20.0f);
                    if (p->mag20 > 240) p->mag20 = 240;
                }
            }

            if (p->state == 2) {                             /* 飞散：直线 + 速度每帧 −0.03 */
                /* ★ 2026-09-15d（§1x.4 的「变慢」层）：**飞散态也吃减速因子** ——
                 * 与 `season.c` 的季节点同口径（用户口径是「同一屏上的东西一起慢」）。
                 * ⚠ 与引擎自己的**唯一**一处差：一手只对**「重力在跑」的那一段**乘因子
                 *   （`item+0xc74 == 2`，段内 `0x445c6d` 乘重力增量、`0x445c2d`/`0x445c31` 乘位移），
                 *   前一段（`item+0xc74 == 1` = 出生的上抛，`0x445bed`–`0x445c55`）是**裸**
                 *   `vel × GAME_SPEED`（`0x445c15`/`0x445c19`/`0x445c21`）⇒ 原生道具的「刚出生那一下上抛」不减慢。
                 *   ⚠ 注意两套段号不可逐值对照：引擎的上抛 = `item+0xc74 == 1`，而本池的
                 *     飞散（= 上抛）是 `p->state == 2`（见文件上方 §一 的说明）。
                 *   116 季节点从第三十三轮起就一并减慢了（`season.c` 的注释写了这条），
                 *   用户口径要的是「绿点跟季节点一样」⇒ 这里**跟 116**（照原生则上抛不减）。
                 *   想回到一手口径：把 `sl` 改成 1.0f 即可（一个局部常量，见 `TUNING.md` §2.5d）。*/
                const float sl = slow;
                x += vx * sl;
                y += vy * sl;
                p->lr -= 0.03f;
                if (p->lr <= 0.0f) {
                    p->lr = 0.0f;
                    vx = 0.0f;
                    vy = 0.0f;
                    p->state = 1;
                } else {
                    float vel[3];
                    ce_polar_to_vel(vel, p->lang, p->lr);
                    vx = vel[0];
                    vy = vel[1];
                }
            } else if (p->state == 1) {                      /* 飘落 */
                const float sl = slow;
                vy += FAITH_GRAVITY * sl;
                if (vy > FAITH_VFALL_MAX) vy = FAITH_VFALL_MAX;
                if (vy >= 0.0f) vx = 0.0f;                   /* 原版：下落时水平速度清零 */
                x += vx * sl;
                y += vy * sl;
            } else if (pp) {                                 /* 3/4：朝自机飞 */
                float mag = (float)p->mag20 / 20.0f;
                float dx = pp[0] - x, dy = pp[1] - y;
                float len = __builtin_sqrtf(dx * dx + dy * dy);

                if (tier == FAITH_TIER_BIG) mag *= FAITH_BIG_MAG_BONUS;
                if (len > 0.001f) { vx = dx / len * mag; vy = dy / len * mag; }
                else { vx = 0.0f; vy = mag; }
                x += vx;
                y += vy;
                if (mag < FAITH_MAG_MAX) {
                    int m = (int)p->mag20 + (int)(FAITH_MAG_STEP * 20.0f);
                    p->mag20 = (uint8_t)(m > 240 ? 240 : m);
                }
            }

            p->x = x;
            p->y = y;
            p->vx = vx;
            p->vy = vy;

            if (y > FAITH_OOB_Y || x >= FAITH_OOB_X || x <= -FAITH_OOB_X) {
                if (p->id) ce_anm_delete((uint32_t)p->id);
                dot_remove(tier, i);
                continue;
            }

            if (pp) {                                        /* 收点判定（**出生帧就跑** = 无出生保护）*/
                float dx = pp[0] - x, dy = pp[1] - y;
                float d2 = dx * dx + dy * dy;

                if (d2 <= collect_r2) {                      /* 收 */
                    dot_collect(p);
                    dot_remove(tier, i);
                    ++collected;
                    continue;
                }
                if (p->state == 1 && d2 <= ring_r2) {        /* 只有飘落态才进吸引半径 */
                    p->state = 4;
                    p->mag20 = (uint8_t)(mag_speed * 20.0f / 3.0f);
                    if (p->mag20 == 0) p->mag20 = 1;
                }
            }

            /* ★★ §1x.4 「变黄 + 抖动」两层：一次调用写掉（条件、颜色、抖动分布与引擎逐条同形，
             * 详见 `faith_dot_slow_style()` 上面那个注释块；**只改 RGB、保留当前 alpha**）。
             * 门 = `slow < 1.0 && age >= 32`（= 引擎的 `factor < 1.0 && age > 31`）。
             * ⚠ 关掉时同一支函数把颜色写回原色 + 位置写回逻辑坐标 —— 「变黄不回来」那个坑
             *   （116 在第三十三轮踩过）在这条路上不存在。*/
            faith_dot_slow_style((uint32_t)p->id, p->age, slow < 1.0f, x, y);
            /* ★★ 2026-09-18（B1）：本帧**存活**且处于真空飞行的点 ⇒ 计入 presence（判定见函数尾）。*/
            if (faith_core_state_is_vacuum_flight(p->state)) ++vacuum_in_flight;
            ++i;
        }
    }

    /* ★★ 2026-09-15（§1x.2 **presence 通道**）：本帧还有几颗大 / 中点在飞 —— 报给 SDK 的
     * 「虚拟在飞」层，由 114 在它的 `on_item_frame` 里读（`ce_vitem_flying() > 0` ⇒ 续批 idle）。
     * ★★ 2026-09-18（B1）：**只数「真空飞行」（`state == 3`）的那些** —— 与引擎对 P / 钱的
     *   presence 谓词（`ce_item_pool_any_attracting`：`type ∈ {1,2}` 且 `item+0xc74 == 3`）**同一谓词**。
     *   ⇒ 飘落（1）/ 飞散（2）中的点**不续批**（引擎那边的坠落道具同样不续批：它们多半会被
     *     普通游玩触收/环收拿走，而那条路本来就不进批 ⇒ 让它们续批只会把批凭空吊住）。
     *   ⚠ 旧实现（2026-09-15，用户那时口径「绿点也得当作正在飞行的道具」）报的是
     *     `s_n_big + s_n_small` =「活着的都算在飞」⇒ 一批绿点飘着就能把批一直吊开、期间收到的
     *     原生道具全并进来 ⇒ 实跑症状「绿点一直触发收点」。现已按用户 2026-09-18 的规格
     *     （「与 P 点 / 金钱**完全一致**」）收窄到真空飞行那一档；判据见 `faith_core.h` 的对照表。
     * ⚠ 微档不进这个数（`faith_vitem_kind` → HALF ⇒ 层内恒 0），今天也没有微档实体。
     * ⚠ 声明是**每帧**的（层按 SDK 帧号判时效）⇒ 池空了、两卡都没在 tick 了、卡被删了，
     *   都会在 `CE_VITEM_HOLD_FRAMES` 帧内自然失效，不需要额外的收尾。*/
    ce_vitem_declare_flying(CE_VITEM_SRC_SHARED, CE_VITEM_FULL, vacuum_in_flight);

    return collected;
}
