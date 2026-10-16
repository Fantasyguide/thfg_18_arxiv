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
#include "mof_green_core.h"   /* ★ 2026-09-22：绿妖精载体的三个纯函数 */

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
 * ## 一、★ 订正：TH10 的转换**没有音效**，它有的是**特效**（一手，2026-09-18 复核）
 * 我们旧注写「一手确实逐颗播音（`th10:0x448db0` 参 `0x189`）」——**那是误读**：
 *   · `0x448db0` 用的是 **`ANM_MANAGER_PTR`**（TH10 `[0x491c10]`，th-re-data statics 命名）：
 *     它 alloc 一个 VM（`0x449950`）、写 `+0x340` 位置 float3 / `+0x38c`+`+0x390` 脚本指针 /
 *     `+0x35c` 标志，并用第 3 个实参去查**脚本表** `[anmgr+0x11c][id*4]`（该表 ~400 项）
 *     ⇒ 第 3 个实参 `0x189` 是 **anm 脚本号**，不是 SE id。
 *   · TH10 的 SE 播放器是 `0x43dc90`，`ecx = SOUND_MANAGER`（`0x492590`）—— 而**转换循环
 *     （`0x41ba50`–`0x41baef`）与信仰道具的 spawn helper（`0x41bb00`）里一次都没调它**
 *     （全 exe 该函数 96 个调用点，没有一个落在这两段里）。
 *   · TH11 侧同样：转换（`th11:0x4240f0`）只调 spawn helper 与 `0x455b10`，两者函数体都**不**引用
 *     `SOUND_MANAGER`（`0x4c3e80`）。
 * ⇒ **原版转换静音**；我们原来借 `se_powerup`（TH18 `0x0d`）当转换音是 ❓ 自定，**已撤**。
 *   ★★★ 2026-09-20（P1 th10 #1）**第三版，也是最终版**：`FAITH_CONV_SE = CE_VOICE_MOFPOWERUP`（`0x5b`）
 *     = **TH10 自己那一枚 `se_powerup.wav`** —— 满 P 那一刻原版真的会响的就是它
 *     （一手 `th10:0x41b446` 小 P / `th10:0x41b68c` 大 P：`mov ebx,0x1d` + `call 0x43dd10`；
 *     TH10 行 `0x1d` = `se_powerup.wav`）。它与 TH18 的同名文件**不是同一次录音**
 *     （`4cb28b08…` 77278 B vs `b9e28720…` 367106 B）⇒ **走扩表导入**（源
 *     `local/th10.v1.00a/dat/se_powerup.wav`，构建期逐字节现取，见 `assets/voice/derive_retail_se.py`）。
 *     ⚠ 历史（别照旧稿办事）：第二版曾用 `0x25 = se_item00`（"吃到一颗道具"的语义对位），
 *     第一版是 `0x28 = se_kira02`（闪光音，本行旧文里的 ③「−15 dB / prio 50」说的是那一版）——
 *     两版都是 ❓ 自定替身，已被一手音色取代。
 *     ⚠ 「只在事件那一下响一声」仍是 ❓ 自定（原版响的是「吃到 P」那一声，而本卡的转换是引擎外的
 *     一次批量删+撒，没有对应的「吃到」事件）；判据与事件闩见 `faith_convert_full_p_items()` 头部。
 *     ★ 2026-09-22（P6 复核，补一条**直接证据**）：TH10 那一声与「满 P 那一帧的全场转换」是
 *     **同一条代码**里发生的 —— `0x41b446 call 0x43dd10`（播 `0x1d`）后面紧跟着
 *     `0x41b44b cmp WORD PTR ds:0x474c48,0x64` → `0x41b455 mov DWORD PTR [esp+0x14],0x1`
 *     （置「刚满 P」标志，正是转换循环 `0x41ba50` 的触发条件）⇒ 「满 P 那一刻」与这一声
 *     在原作里**逐帧同源**（这也解释了用户的记忆为什么把两者连在一起）。
 *     换音 = 改这一个宏；要换 TH18 没有的 wav 就走 `assets/voice/ORDER.txt` 扩容（`AUDIT` §Q/§DK-7）。
 *
 * ## 二、★ 原版有的那颗特效（一手配方，逐条抄得下来）
 * TH10 转换循环对**每一颗**被转换的 P 道具：
 *   `0x448db0(anm_mgr_obj, &item.pos, 0x189)` ⇒ 在**该道具的位置**起一个脚本 VM。
 * 全局脚本号 `0x189` = `bullet.anm` 的 **script393**（`local/vendor/thtk/build/thanm/thanm -l 10
 * local/th10.v1.00a/dat/bullet.anm` 可复跑）：
 *   ```text
 *   script393 { ins_3(sprite332); ins_66(1); ins_68(11); ins_67(0);
 *               ins_52(128,255,128);      ; ★ 顶点色 = 淡绿（转换出来的正是绿点）
 *               ins_50(0.5f, 0.5f);       ; 起始 scale 0.5
 *               ins_60(20, 4, 1.5f, 1.5f) ; 20 帧缩到 1.5（= 弹出）
 *               +10: ins_58(10, 4, 0);    ; 再 10 帧 alpha → 0
 *               +10: ins_1(); }           ; 删
 *   ```
 *   `sprite332` = `bullet/etama2.png` **(64,224,32,32)**（32×32；本仓的信仰点贴图来自**同一张**图的
 *   y=208 行 —— 见 `assets/ability/make_mof_art.py`）。
 * ⇒ **下一步（资产活）**：把这支脚本 + 这块 32×32 切图搬进 `ability.anm`（走 272/273 那条老流水线：
 *   `assets/ability/ORDER.txt` + `make_mof_art.py` + `build_ability.py`），转换时按 `faith_spawn()`
 *   成功的那几颗在 `(px,py)` 各起一台 —— 就与 TH10 逐条一致了。**今天先不硬塞**（要动 atlas 号段与
 *   `CAPACITY.md` 台账，属于独立一步）。*/

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
    /* ★★ 2026-10-14（P14a）：改用 **I1 词汇** ✓ —— `ce_anm_set_color_rgb()` 内部就是
     *   「读 color_1、保 alpha、只换 RGB」✓（与原来那两行手写表达式**逐位等价** ✓，
     *   等价性由 `tests/test_sdk_color.c` 的 400 条断言钉死 ✓）。
     *   ⇒ 原来那两个局部量（`vm` / `cur`）不再需要，一起删掉 ✓（否则 `-Werror=unused` ✗）。*/
    ce_anm_set_color_rgb(anm_id, tinted ? FAITH_SLOW_TINT : FAITH_TINT_NONE_RGB);
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
    /* ★★ 2026-09-18：**自带外观**（出生时从注册者抄过来；0 = 按档的默认点外观）——
     *   见 `faith_user_t` 尾部那两个字段的注（唯一使用者 = 107 的樱点）。*/
    int32_t art_sprite;
    int32_t art_script;
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

/* ★★ 2026-10-03d（92 伟大之作的**清扫**通道②；owner：「110-111 得响应清扫，107 是樱点，也得响应清扫」）：
 *   本池是 **107 樱点 / 110-111 信仰点·绿点共用**的（见 `faith.h` 文件头）⇒ 清扫只做这一份，
 *   三张卡一起响应（不需要各自在本卡 tick 里再挂一次）。
 *
 *   形态与那六张私有池的卡逐条一致（见 `AUDIT.md` §BE 详录 #221/#222）：
 *     · 每颗在**原位**撒 `FAITH_SWEEP_MONEY_PER_ITEM` 枚 type 2 金钱道具；
 *       道具池满 ⇒ 走 `ce_money_pocket(1)`（与 92 通道①同一条钱包记账）。
 *     · **消失走本层自己的收尾**：`ce_anm_delete(VM)` + `dot_remove()`（swap-remove）
 *       ⇒ 不留幽灵 VM（铁律二）。⚠ `state 0`（待发射、还没建 VM）的颗**也算在场上**
 *       —— 它已经在池里、会被 `faith_absorb_list()` 看见 ⇒ 一并换钱，只是没有 VM 可删。
 *     · 倒序遍历：`dot_remove()` 会把尾元素换到 `i`，正序会跳项。
 *   ❓ 面值自定（1 颗 = 1 枚）—— 见 `TUNING.md` §2.11 的跨卡面值表。*/
#define FAITH_SWEEP_MONEY_PER_ITEM 1

int faith_sweep_to_money(int32_t *out_money)
{
    int t, n = 0;
    int32_t money = 0;
    for (t = 0; t < 2; ++t) {
        const faith_tier_t tier = (t == 0) ? FAITH_TIER_BIG : FAITH_TIER_SMALL;
        int i = dot_n(tier);
        while (--i >= 0) {
            faith_dot_t *p = dot_at(tier, i);
            float pos[3];
            int k;
            pos[0] = p->x;
            pos[1] = p->y;
            pos[2] = 0.0f;
            for (k = 0; k < FAITH_SWEEP_MONEY_PER_ITEM; ++k) {
                if (ce_spawn_item(2u, pos, -1.5707964f, 1.7f)) ++money;
                else { ce_money_pocket(1); ++money; }
            }
            if (p->id) ce_anm_delete((uint32_t)p->id);
            dot_remove(tier, i);
            ++n;
        }
    }
    if (out_money) *out_money = money;
    if (n) ce_log("faith: 92 清扫 —— %d 颗点换成金钱（每颗 %d 枚，共 %d 枚）",
                  n, FAITH_SWEEP_MONEY_PER_ITEM, (int)money);
    return n;
}

/* ============================ ★★ 声明式「外部消费者」（112 的飞碟）====================================
 * 语义与为什么用「快照 + 坐标取回」写在 `faith.h` 那一段；这里只做实现。
 * ⚠ 本支**不动记账**：取走的点不进 Δ / 不进 114 的批 / 不加连击 —— 一手的道具被 UFO 吸走也是
 *   「道具消失、不进自机的账」；「算谁的账」由调用方（112）决定，见 `NEXT.md` §1x.9。 */
int faith_absorb_list(faith_absorb_t *out, int max)
{
    int n = 0, t;
    if (!out || max <= 0) return 0;
    for (t = 0; t < 2; ++t) {
        const faith_tier_t tier = (t == 0) ? FAITH_TIER_BIG : FAITH_TIER_SMALL;
        int i, cnt = dot_n(tier);
        for (i = 0; i < cnt && n < max; ++i) {
            const faith_dot_t *p = dot_at(tier, i);
            if (!p->id) continue;                    /* 还没建 VM（state 0 待发射）⇒ 看不见也吸不到 */
            out[n].x = p->x;
            out[n].y = p->y;
            out[n].tier = (int32_t)tier;
            out[n].slot_mask = p->slot_mask;
            out[n].value = p->value;
            /* ★ 2026-09-24：把"出生多久 / 在哪个段 / 在不在上抛豁免期"一并声明给消费者
             *   （112 的飞碟要按它决定"刚撒出来的点先别吸"）。判据与真空门那一处**同一条**。*/
            out[n].age = (int32_t)p->age;
            out[n].state = (int32_t)p->state;
            out[n].in_toss = (p->state == (uint8_t)FAITH_STATE_SCATTER &&
                              p->age < FAITH_TOSS_FRAMES) ? 1 : 0;
            ++n;
        }
    }
    return n;
}

/* 与 `faith_absorb_take` 同一个匹配规则（坐标 + 槽）的一支**定位器** ——
 * 两处不能各写一份匹配（迟早漂移）⇒ 抽成一个 static 帮手。 */
static faith_dot_t *faith_absorb_find(const faith_absorb_t *e, int *out_index)
{
    faith_tier_t tier;
    int i, cnt;
    if (out_index) *out_index = -1;
    if (!e) return 0;
    if (e->tier != FAITH_TIER_BIG && e->tier != FAITH_TIER_SMALL) return 0;
    tier = (faith_tier_t)e->tier;
    cnt = dot_n(tier);
    for (i = 0; i < cnt; ++i) {
        faith_dot_t *p = dot_at(tier, i);
        if (!p->id) continue;
        if (p->x < e->x - 0.5f || p->x > e->x + 0.5f) continue;
        if (p->y < e->y - 0.5f || p->y > e->y + 0.5f) continue;
        if (p->slot_mask != e->slot_mask) continue;
        if (out_index) *out_index = i;
        return p;
    }
    return 0;
}

int faith_absorb_move(const faith_absorb_t *e, float nx, float ny)
{
    faith_dot_t *p = faith_absorb_find(e, 0);
    if (!p) return 0;
    p->x = nx;
    p->y = ny;
    /* VM 位置同步（世界层 VM 的实体坐标就是本坐标空间；照本文件别处写位置的同一支）*/
    if (p->id) {
        void *anm = CE_ABILITY_ANM();
        if (anm) ce_anm_set_pos((uint32_t)p->id, nx, ny, 0.0f);
    }
    return 1;
}

int faith_absorb_hold(const faith_absorb_t *e, float nx, float ny)
{
    faith_dot_t *p = faith_absorb_find(e, 0);
    if (!p) return 0;
    p->x = nx;
    p->y = ny;
    /* ★ 一手：TH12 的道具上钩那一刻把速度清零（state 7，`0x4271ec`–`0x427204` 三条 FLDZ/FST）
     *   ⇒ 位移只由飞碟那一步决定。这里照做：清零本点的 vx/vy；若还在飞散段（state 2），
     *   把 `lr` 也清零 ⇒ 下一帧自然落回"飘落"（state 1），不会再从 `lang/lr` 重算出速度。*/
    p->vx = 0.0f;
    p->vy = 0.0f;
    if (p->state == (uint8_t)FAITH_STATE_SCATTER) p->lr = 0.0f;
    if (p->id) {
        void *anm = CE_ABILITY_ANM();
        if (anm) ce_anm_set_pos((uint32_t)p->id, nx, ny, 0.0f);
    }
    return 1;
}

int faith_absorb_take(const faith_absorb_t *e)
{
    int idx = -1;
    faith_dot_t *p = faith_absorb_find(e, &idx);
    if (!p || idx < 0) return 0;
    ce_anm_delete((uint32_t)p->id);                  /* 与 dot_collect 同款：收下就删 VM */
    dot_remove((faith_tier_t)e->tier, idx);
    return 1;
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
    /* ★ 2026-09-18：自带外观优先（0 = 按档默认）—— 107 的樱点走这两个字段；
     *   逐颗覆盖由 `faith_spawn_art()` 在**出生时**写进结构（见 `dot_spawn_at()` 尾），本函数不必知道。*/
    const int script = p->art_script ? (int)p->art_script : faith_script_for(tier);
    const int sprite = p->art_sprite ? (int)p->art_sprite : faith_sprite_for(tier);
    uint32_t id = ce_anm_spawn(CE_ABILITY_ANM(), script, FAITH_LAYER);

    if (!id) return 0;
    ce_anm_set_pos(id, p->x, p->y, 0.0f);
    ce_anm_set_sprite(CE_ABILITY_ANM(), id, sprite);
    ce_anm_set_color(id, argb);          /* color_1（`+0x524`）*/
    ce_anm_set_color2(id, argb);         /* color_2（`+0x528`）—— 两路都写才与 colorMode 无关 */
    p->id = (int32_t)id;
    return 1;
}

/* 公共体：`ang` = 上抛角（`faith_spawn()` 传一手的 −π/2；`faith_spawn_angled()` 传调用方给的角）。
 * ⚠ 必须保持「只在这一支里出现一次 memset/入池」——两个入口不能各写一遍（会漂移）。*/
static int dot_spawn_at(float x, float y, faith_tier_t tier, int slot_mask, int32_t value,
                        float ang, int32_t force_sprite, int32_t force_script)
{
    faith_dot_t *p;
    float spd, vel[3];
    int i;

    if (!slot_mask || value <= 0) return 0;
    if (tier == FAITH_TIER_MICRO) return 0;      /* 微档不建实体（见文件头 ❓ 自定）*/
    if (tier != FAITH_TIER_BIG && tier != FAITH_TIER_SMALL) return 0;
    if (dot_n(tier) >= (tier == FAITH_TIER_BIG ? FAITH_BIG_POOL : FAITH_SMALL_POOL)) {
        ++s_stat_dropped;                        /* 池满：丢弃（不做待撒队列 —— 量级不需要）*/
        return 0;
    }

    /* ★ E-2（2026-09-15）：出生运动学 = **一手实参**，不再随机 ——
     * 角见上面公共体的说明（默认 −π/2 = `0xbfc90fdb`，直上；只有满 P 转换的**第 2 组外观起**才给偏角）、
     * 速按档（`faith_core_throw_speed()`：2.2 / 2.2 / 0.6）。
     * 撒点位置由调用方给，**一律是实体自己的位置**（道具 / 弹原位；见 `faith_core.h` §E-2）。*/
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
    /* ★ 2026-09-18：把**注册者的自带外观**抄进这一颗（0 = 默认按档外观）。
     *   按 `slot_mask` 找回注册者 —— 「一个消费者占自己一位槽」是 `faith_register()` 的既有约定。*/
    for (i = 0; i < s_n_user; ++i) {
        if (s_user[i].slot_mask != slot_mask) continue;
        p->art_sprite = (int32_t)s_user[i].art_sprite;
        p->art_script = (int32_t)s_user[i].art_script;
        break;
    }
    /* ★ 2026-09-18：调用方的**逐颗覆盖**（`faith_spawn_art()`；0 = 不覆盖）—— 107 的两种樱点用。*/
    if (force_sprite) p->art_sprite = force_sprite;
    if (force_script) p->art_script = force_script;
    if (tier == FAITH_TIER_BIG) ++s_n_big; else ++s_n_small;
    return 1;
}

int faith_spawn(float x, float y, faith_tier_t tier, int slot_mask, int32_t value)
{
    return dot_spawn_at(x, y, tier, slot_mask, value, FAITH_THROW_ANGLE, 0, 0);
}

int faith_spawn_angled(float x, float y, faith_tier_t tier, int slot_mask, int32_t value,
                       float angle)
{
    return dot_spawn_at(x, y, tier, slot_mask, value, angle, 0, 0);
}

int faith_spawn_art(float x, float y, faith_tier_t tier, int slot_mask, int32_t value,
                    float angle, int32_t sprite, int32_t script)
{
    return dot_spawn_at(x, y, tier, slot_mask, value, angle, sprite, script);
}

int faith_spawn_big(float x, float y, int32_t value_110)
{
    return faith_spawn(x, y, FAITH_TIER_BIG, 1 << FAITH_SLOT_110, value_110);
}

/* ★★ 2026-09-22（th11 #1「绿点和金钱重合问题需解决」）：ZUN 自己那条「一次爆多颗道具」的分布，
 * 做成**共用入口** —— 110 的 `on_enemy_death` 与 111 的 `on_enemy_drop_pre` 用同一份公式，
 * 才不会出现「110 撒开了、111 还压在金钱正中」。理由、一手出处与随机源口径全在 `faith.h` 的同名声明上。*/
int faith_scatter_from_enemy(const uint8_t *enemy, float *ox, float *oy)
{
    static uint32_t s_seed = 0x9e3779b9u;   /* ❓ 自定：不用引擎那两支走 x87 的 RNG（见 `faith.h`）*/
    const uint8_t *d;
    float rx, ry, ang, cs_sn[3] = { 0.0f, 0.0f, 0.0f };

    if (ox) *ox = 0.0f;
    if (oy) *oy = 0.0f;
    if (!enemy) return 0;
    d = enemy + CE_ENEMY_DROP;
    rx = *(const float *)(d + CE_ENEMY_DROP_AREA_X);
    ry = *(const float *)(d + CE_ENEMY_DROP_AREA_Y);
    if (!(rx > 0.5f)) rx = CE_ENEMY_DROP_AREA_DEFAULT;   /* 没调 dropArea ⇒ 引擎自己的默认 32×32 */
    if (!(ry > 0.5f)) ry = CE_ENEMY_DROP_AREA_DEFAULT;
    s_seed = s_seed * 1664525u + 1013904223u;            /* Numerical Recipes LCG */
    ang = (((float)(s_seed >> 8) * (1.0f / 16777216.0f)) * 2.0f - 1.0f) * 3.1415927f;  /* rand± × π */
    ce_polar_to_vel(cs_sn, ang, 1.0f);                   /* 引擎 FSINCOS ⇒ (cos, sin)，不引 libm */
    s_seed = s_seed * 1664525u + 1013904223u;
    faith_core_scatter_off(rx, ry, (float)(s_seed >> 8) * (1.0f / 16777216.0f),
                           cs_sn[0], cs_sn[1], ox, oy);
    return 1;
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

/* ★★ 2026-09-18（B12）：**满 P 转换特效** —— TH10 `bullet.anm` **script393** 的移植
 * （脚本正文 = `assets/ability/scripts/295_mof_conv_burst.anm.txt`，逐条一手见那里的长注）。
 * 一手触发点：TH10 的全场转换循环对**每一颗**被转换的 P 道具调
 * `0x448db0(anm_obj, &item.pos, 0x189)`——`0x448db0` 用 `ANM_MANAGER_PTR`、参 3 查**脚本表**
 * ⇒ `0x189` 是**脚本号**（**不是 SE id**；TH10 的 SE 播放器 `0x43dc90` 在转换里零调用
 * ⇒ **原版转换静音**，只有这台特效）。
 * 形态（逐值照抄）：贴图 = TH10 sprite332 = `etama2.png (64,224,32,32)`（本仓 `CONV_BURST`）、
 * 顶点色 `(128,255,128)` 淡绿、起手 scale 0.5、20 帧缩到 1.5、第 10 帧起淡出 10 帧、20 帧自删。
 * ⚠ 层 / 颜色 / 混合模式由**脚本自己**写（`layer(11)` / `color(128,255,128)` / `blendMode(1)`），
 *   C 侧只给位置（照 280/281 火星的接线口径；**不要** `ce_anm_set_scale()`）。*/
#define FAITH_CONV_LAYER      11   /* = TH10 script393 的 `layer(11)`（数值一字未动）*/
/* ★★ 2026-09-18（B12 第二版，按用户实跑反馈改）：**一次转换事件只响一声**（不是逐颗）。
 *   用户原话：「原版的音效似乎是**只有满 P 转换的那一下有**，否则没有。也有可能是我跟满 P 的音效本身混了」。
 *   一手对得上：TH10 的转换循环本身**不播音**（`th10:0x41ba50` 函数体只有 2 个 call：
 *   `0x41bb00` spawn_item / `0x448db0` 建 anm VM，**一次 `play_sound` 都没有**；
 *   `0x41afd0–0x41be16` 全段只有 3 个 SE 站点，全在 `ItemManager::on_tick__body` 里），
 *   那一刻能听到的是 **P 道具被吃掉时**的那两声：
 *   · `th10:0x41b446`（小 P）/ `th10:0x41b68c`（大 P）：`mov ebx,0x1d` + `call 0x43dd10`
 *     ⇒ **TH10 `0x1d` = `se_powerup.wav`**（⚠ 旧注写「= se_graze」**是错的**：`0x1c` 才是 se_graze）；
 *   · `th10:0x41b72d`（所有道具收集的公共尾）：`mov ebx,0x14` ⇒ `0x14` = `se_item00.wav`。
 *   ★★ 2026-09-20（P1 th10 #1）：**改成直接用 TH10 那一枚** —— TH10 的 `se_powerup.wav`
 *     与 TH18 的同名文件**不是同一次录音**（77278 B vs 367106 B，md5 不同）⇒ 借 TH18 `0x0d` 听起来
 *     就是另一个音。现在走音效表扩容通道 `CE_VOICE_MOFPOWERUP`（`0x5b`，构建期从
 *     `local/th10.v1.00a/dat/se_powerup.wav` 逐字节现取，见 `assets/voice/derive_retail_se.py`）。
 *   ⇒ 性质说明：**音色 = ✅ 一手（TH10 那一枚）**；**「只在事件那一下响一声」= ❓ 自定**（原版转换静音、
 *     它响的是「吃到 P」那一声，而本卡的转换是引擎外的一次批量删+撒，没有对应的「吃到」事件）。
 *     `TUNING.md` §4/§4.1 已登记。*/
#define FAITH_CONV_SE         CE_VOICE_MOFPOWERUP

/* ★ 2026-09-18：满 P 转换里「不同外观的消费者」之间的上抛偏角步长 = **10°** =
 * `0.1745329`（`th18:0x429168` 那枚 —— TH18 自己消弹撒点用的抖动角，一手见
 * `engine/bullet/th18/01-cancel-to-item.md` §2.1）。用户口径：「不要让两种点重合；有角度的上抛？」*/
#define FAITH_CONV_SPREAD_STEP 0.1745329f

static void conv_burst_spawn(float x, float y)
{
    uint32_t id = ce_anm_spawn(CE_ABILITY_ANM(), CE_ANM_ABILITY_SCRIPT_MOF_CONV_BURST, FAITH_CONV_LAYER);
    if (!id) return;
    ce_anm_set_pos(id, x, y, 0.0f);
}

/* 诊断（静态区）：累计转换过的原生道具颗数 + 撒失败（池满）次数。*/
static int32_t s_stat_converted, s_stat_conv_dropped;

int faith_convert_full_p_items(void)
{
    static uint32_t s_last_frame;
    static int      s_have_last;
    static int      s_was_full;         /* ★ B12 第二版：上一帧是否已经满 P（事件判据用）*/
    uint8_t *mgr;
    uint32_t now;
    int i, u, n = 0;
    const float *pp_conv;

    /* ① 只在满 P 时干活（一手：`th10:0x41bbf7 cmp WORD ds:0x474c48,0x64` 的同一判据在 TH18 侧
     *    = `CURRENT_POWER >= MAX_POWER`；`max_power <= 0` = 引擎还没初始化 ⇒ 不碰）。*/
    if (FAITH_MAX_POWER() <= 0 || CE_CURRENT_POWER() < FAITH_MAX_POWER()) {
        s_was_full = 0;                                  /* 不在满 P ⇒ 下一次满 P 算「新事件」*/
        return 0;
    }
    /* ★★ 2026-09-20（P1「缺音效一批」th10 #1「缺少满 P 那一刻的滞留 P 点转换音效」）：
     *   事件闩**不再在这里消费** —— 旧写法在扫描之前就 `s_was_full = 1`，于是
     *   「**刚满 P 那一帧场上恰好没有 P 道具**（`n == 0`）」会把事件帧白白吃掉 ⇒
     *   **此后满 P 期间的每一次转换都无声**（道具照样被转换，只是 `conv_event` 已经是 0）。
     *   改成在拿到 `n` 之后才消费（见转换循环尾）。`s_was_full` 的语义因此精确化为
     *   「**上一次真的转换过**、且现在仍然是满 P」。*/

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
        /* ★★ 2026-09-18（用户口径「不要让两种点重合；有角度的上抛？」）：**按外观分组**给上抛角 ——
         *   同外观的消费者共用同一个角（⇒ 仍然完全重合、视觉一颗，110+111 的行为不变），
         *   不同外观的组依次 ±10°（⇒ 107 的樱点不会藏在信仰点底下）。
         *   第 0 组 = 一手的 −π/2 ⇒ **单独装任何一张时逐字节不变**。
         *   偏角常数 = `0.1745329`（10°）= `th18:0x429168` 那枚（TH18 自己消弹撒点用的抖动角，一手见
         *   `engine/bullet/th18/01-cancel-to-item.md` §2.1）。**不抽随机、不推 RNG**。*/
        {
            uint32_t seen_spr[FAITH_MAX_USERS], seen_scr[FAITH_MAX_USERS];
            int n_seen = 0;

            for (u = 0; u < s_n_user; ++u) {
                int g = -1, v;
                float ang;

                if (s_user[u].conv_value <= 0) continue;
                for (v = 0; v < n_seen; ++v) {
                    if (seen_spr[v] == s_user[u].art_sprite && seen_scr[v] == s_user[u].art_script) {
                        g = v;
                        break;
                    }
                }
                if (g < 0) {                        /* 新外观 ⇒ 新组 */
                    g = n_seen;
                    seen_spr[n_seen] = s_user[u].art_sprite;
                    seen_scr[n_seen] = s_user[u].art_script;
                    ++n_seen;
                }
                ang = FAITH_THROW_ANGLE;
                if (g > 0) {
                    const float k = (float)((g + 1) / 2);            /* 组 1→+1、2→−1、3→+2、4→−2 …（左右分）*/
                    ang += ((g & 1) ? k : -k) * FAITH_CONV_SPREAD_STEP;
                }
                if (!faith_spawn_angled(px, py, faith_core_tier_for_source(FAITH_SRC_FULL_P),
                                        s_user[u].slot_mask, s_user[u].conv_value, ang)) {
                    ++s_stat_conv_dropped;
                }
            }
        }
        /* ★★ 2026-09-18（B12）：**逐颗起一台转换特效**（TH10 `0x448db0(…, 0x189)` 的对位）——
         * 位置 = **该 P 道具原位**（一手：`0x448db0` 的第 2 参就是道具自己的 pos float3）。*/
        conv_burst_spawn(px, py);
    }
    /* ★★ 2026-09-18（B12 第二版）/ 2026-09-20（P1 修事件闩）：音是**事件**级的 ——
     * 「满 P 之后**第一次真的转换**」那一帧响一声（用户口径「只有满 P 转换的那一下有」；
     * 一手见 `FAITH_CONV_SE` 的注释）。⚠ 判据必须是 `n > 0 && !s_was_full`（**不是**在函数开头
     * 就算好的 `conv_event`）—— 否则 n == 0 的那一帧会把事件吃掉，此后全程无声。
     * 声像取自机 x（那一刻道具散在场上，取任何一颗都不对；自机是这一事件的中心）。*/
    if (n > 0 && !s_was_full) {
        pp_conv = ce_player_pos();
        ce_play_sound(FAITH_CONV_SE, pp_conv ? pp_conv[0] : 0.0f);
    }
    if (n > 0) {
        s_was_full = 1;                                          /* ★ 事件闩在这里消费（见函数头那条注）*/
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

/* ============================================================================================
 * ★★★ 2026-09-22（P6）：**绿妖精载体 —— 110 / 111 共用**（原在 `cards/mof.c`，整段搬来）
 *
 * 「**每波多刷一只（或一对）绿的**」（2026-09-18b）—— 不碰原版妖精。
 *
 * ## 为什么搬进共用件（一手 + 现状）
 *   · 111（地灵殿）的设计**本来就要求「让绿妖精出现」**（`docs/superpowers/specs/2026-09-11-system-card-spec-review.md`
 *     §805，用户 2026-09-12 口径）—— 而这段代码原来只被 110 的 `on_tick_2` 调
 *     ⇒ 只装 111 时绿妖精**一次都不会出现**（owner 实跑 th11 #4「全程未见绿妖精」）。
 *   · 它天生是「两作共用的一件事」：载体掉的 `(N,S) = (2,10)` 是**同一张宿主表**的键
 *     （110 判据用它、111 现在也判它），所以载体、跟随、收尾都该只有**一份**实现。
 *   · 搬到共用件的代价最小：本段只用 SDK 原语 + `mof_green_core.h` 的三个纯函数
 *     （`mof_green_is_fairy` / `mof_green_extra_slots` / `mof_green_extra_follow_offset`）。
 *
 * ## 谁驱动它（**一帧只跑一遍**）
 *   110（`cards/mof.c`）与 111（`cards/signal.c`）各在自己的 `on_tick_2` 里调一次
 *   `faith_green_fairy_tick()`；本函数自带**帧守卫**（与 `faith_pool_tick()` / `faith_convert_full_p_items()`
 *   同一口径，帧号 = `ce_sdk_frame()`）⇒ 两卡同装时只有先跑到的那一次真的执行。
 *   ⚠ 帧号从「卡自己的 `s->frame`」换成**引擎帧号**：本段所有比较都是**相对差**
 *     （`frame - s_gf_last` / `frame < s_gf_frame + DELAY`）⇒ 行为等价（110 侧不变）。
 *   ⚠ 卡被删掉（倒吊人 / 空白卡）那一刻两卡都不再调它 ⇒ 场上那几只必须由
 *     `faith_green_fairy_depart_all()` 收尾（由**每张卡的** `on_clear` 调，幂等）。
 *
 * ## 读了哪两个引擎字段
 *   `CE_ENEMY_ANM_SRC`(`+0x1494`) / `CE_ENEMY_ANM_SCRIPT`(`+0x1498`) = 这只敌机的 `(N,S)`
 *   （一手与出处写在 `engine.h` 那两个宏上）—— 判「这一只算不算妖精」与「它是不是我们自己刷的」。
 *
 * 用户原话：「**有没有可能你给循环 +1？**」+「按照**几何形状**，每波多**一对**，或者多**一只**，你懂我意思」。
 * 为什么不是「改 ECL 循环计数 +1」：循环体只有**一处** `enmCreate("GirlBlueA01", …)`，所有迭代共用同一
 * 调用点 ⇒ 多刷出来的是**原色**妖精，引擎里没有任何字段能区分「我是多出来的那只」（全量论证见
 * `REPORT-110-GREEN-FAIRY.md` §1.6）⇒ 改成「**我们自己多刷一只**」：
 *
 *   · 载体 = **我们自己的 ECL 子程序** `CEGreenFairy`（`assets/ecl/green_extra.ecl.txt`；
 *     `make ecl-ufo` 把它合进原生 `default.ecl`，`release.py:33` 映射进 thcrap 常开的 patch-step3）
 *     —— 先例 = 108 使魔壳 / 112 的四个 UFO 令牌（都走 `ce_spawn_enemy()`，`sdk.h:980`）。
 *   · 它**天生就是绿的**：子程序里 `anmSelect(2); anmSetMain(0, 10);` ⇒ `enemy.anm` 绿色变体。
 *     ★ 顺带：`(N,S) = (2,10)` **本来就是宿主表的键** ⇒ 它死亡时自动撒一颗大信仰点，
 *       本文件一个字都不用加；`main_drop = 0` ⇒ **纯**信仰点（原生什么都不掉）。
 *   · **几何**：卡侧每帧扫敌人，把本波已经刷出的小妖精**出生点**记下来（最多 8 个），
 *     隔 `FAITH_GF_DELAY` 帧后用 `mof_green_extra_slots()` 算出**队形外面那一格**（左右人少的一侧优先；
 *     一对则左右各一），**并且采样本波第一只的入场向量**（同帧位移 → 角/速）传给子程序
 *     ⇒ 它沿着同一队形方向入场，看起来就是这一波的一员。
 *   · **一波的边界**：用「超过 `FAITH_GF_GAP` 帧没有新的小妖精」当判据（不需要读 ECL 调用栈）。
 *   · 卡不在场 ⇒ 本函数根本不会被调（只有 `on_tick_2` 调它）；已刷出的自己会到点离场（子程序里 `delete()`）。
 *
 * ⚠ 影响面（写在这里免得后人踩）：**多一只敌人** ⇒ 章节击破数、117 的波次普查
 *   （`animal_wave_group_totals`，它以**静态表**为准、运行期 census 兜底）、以及本卡
 *   「非 boss 击破 +10f 连击」（`on_enemy_death`）都会看到它 —— 用户口径下这是**要的**
 *   （它就该是一颗能打的信仰点），但数值口径变了要知道。*/
#define FAITH_GF_ECL_NAME  "CEGreenFairy"   /* = `assets/ecl/green_extra.ecl.txt` 里的子程序名 */
#define FAITH_GF_ENABLE    1                /* 总开关（❓自定）*/
/* ★★ 两个常量 = 「一波几只 / 每几波补」，默认值照 **TH10（MoF，本卡的正牌原型）** 实测的分布定：
 *   · TH10 全作 177 只掉大信仰点（type 3）的敌人，**每一处都只刷 1 只（177/177 = 100%）** —— ZUN 从不批量刷绿妖精；
 *   · 逐波：54 波里 **33 波是 0 只**，非零波里 **9 个是 1–2 只**，只有 3 个是密集波（EX 36 / 五面 boss 段 21 / 四面 boss 段 18）；
 *   · 一站点的占比 = 175/1774 = **9.9%**；一关累计 14–37 只（均值 25.3）。
 *   一手：`engine/faith/th10/02-faith-drop-distribution.md` §3/§5（用户口径「一波改一两只」与这条**实测一致**）。
 * ⚠ 姊妹作 TH11（卡 111）**不是**这个形状 —— 它是双峰「一波要么 0 只、要么 5~80 只」
 *   （`engine/faith/th11/03-faith-drop-distribution.md` §3）。要让 110 走 TH11 的形状：
 *   `FAITH_GF_COUNT = 5..8` + `FAITH_GF_EVERY = 2..3`（**那是另一个游戏的手感，默认不采用**）。*/
/* ★★ 2026-09-24 owner 口径：「**绿妖精我放弃了，走改妖精路线**（只有杂鱼绿色妖精、一只/一对）」
 *   ⇒ 「每波多刷一只」这条路**停用**（= 0），改由 `mof_green_pairs` 的**换皮**那一行进位
 *   （`mof_green_core.c` 的 A 组蓝妖精 · 只 st01）。机理与判据一行都没删，要回去把它改回 1 即可。*/
#define FAITH_GF_COUNT     0                /* 每波补刷**几只**：1 = 一只（TH10 的形状）/ 2 = 一对 / 5–8 = 整群 */
#define FAITH_GF_EVERY     1                /* 每**几**波补一次：**1 = 每波**（2026-09-23 从 2 改成 1）。
                                              * 改的依据 = 用户实跑「**绿妖精看到了，但是只看到一只**」+ 他当初的口径
                                              * 「我预期改是**一波改一两只**」：隔一波补一只 ⇒ 一整关只见到 1–3 只，
                                              * 与他预期不符。0 = 关（一个都不补）。
                                              * ⚠ 一手参考：TH10 全作 177 个掉大信仰点的敌人位**每处都只刷 1 只**，
                                              *   而 54 波里只有 21 波有（39%）—— 那是**原作**的密度；我们现在取「每波 1 只」，
                                              *   比原作密，**这是用户口径优先**（登记为 ❓ 自定，见 TUNING §2.0r）。*/
#define FAITH_GF_GAP       75               /* 帧：这么久没有新的小妖精 ⇒ 认为换了一波（❓自定）*/
#define FAITH_GF_DELAY     12               /* 帧：本波第一只出现后隔这么久再补刷（攒几个出生点认队形，
                                           *      但**要趁妖精还在刷** —— 2026-09-18c 从 18 收到 12）*/
#define FAITH_GF_WINDOW    24               /* 帧：只在「上一只妖精还是在这么久内刷出的」时才补 —— 避免
                                           *      补刷落到**下一波**的画面里（用户实跑：大蝴蝶/阴阳玉）*/
#define FAITH_GF_SAMPLES   8                /* 记几个出生点（队形采样上限）*/
#define FAITH_GF_LIFE      40               /* 血量（TH18 小妖精量级；`CE_SPAWN_LIFE` 必须 < 1000）*/
#define FAITH_GF_SCORE     0                /* 击破分：给 0 ⇒ 不靠它刷分（❓自定）*/
/* ★ 2026-09-24：**入场段帧数** —— 出生后这么多帧里 y 从入场线插值到目标那一格
 *   （≈0.75 s @60fps，与原生小妖精从 y=−32 走进场地的节奏同量级）。见 `gf_follow_tick()`。*/
#define FAITH_GF_SPAWN_Y_MIN 24.0f        /* ★ 补刷妖精的**出生 y 下限**（夹进场地内，理由见出生点那段：
                                           *   场地之上的敌机会被引擎当帧回收 —— 探针实测 age=2 就没了）*/
#define FAITH_GF_ENTER_FRAMES 45
#define FAITH_GF_TTL       690              /* ★ 存活上限帧数（≈11.5 s @60fps）。2026-09-22 订正：
                                           *   旧注释写「只传给 ECL 记日志、子程序到点自己 delete」——
                                           *   **ECL 里没有这段**（它只认 `$I0 == 999`）⇒ 现在由卡侧的
                                           *   `gf_follow_tick()` 按 `age` 到点写 `$I0 = 999` 收尾。
                                           *   ❓自定：TH18 一波妖精的本体运动 ≈450 帧（`GirlA01` 体：
                                           *   30 + 120 + 300），690 给它留了余量又不至于永久占位。*/
#define FAITH_GF_ANGLE_DOWN 1.5707964f      /* π/2：ECL 的角度（0 = +x，π/2 = 向下）*/
/* ★★ 2026-09-18c（用户实跑后修的两条）：实跑日志里三只补刷有**两只撒到屏幕外**
 *   （`@(384,96)` / `@(-264,96)` —— 游玩区只有 ECL x ∈ [−192,192]，见
 *   `engine/anm/th18/01-vm-instantiate.md` §3），第三只 `@(104,-32)` 正常。
 *   还有一只的 `y` 是观测到的 96（屏幕中段）⇒ 看着"凭空出现在半空"。
 *   ⇒ ① 槽位 x **夹进游玩区**（留 `FAITH_GF_X_LIMIT` 边距）；② 只用**看起来像入场线**的 y
 *      （`|y| ≤ FAITH_GF_ENTRY_BAND`），否则一律走标准入场线 `FAITH_GF_ENTRY_Y`。*/
#define FAITH_GF_X_LIMIT    176.0f          /* |x| 上限（游玩区 192 − 16 边距）*/
#define FAITH_GF_ENTRY_Y    (-32.0f)        /* 标准入场线（实跑日志里原生 `enmCreate` 的 y 就是 −32）*/
#define FAITH_GF_ENTRY_BAND 48.0f           /* 观测到的 y 超过这个绝对值 ⇒ 认为它已经进场，不用它*/

static int32_t s_gf_frame;                /* 本波第一只出现的那一帧 */
static int32_t s_gf_last;                 /* 上一只小妖精出现的帧（波次边界的判据）*/
static int32_t s_gf_max_id;               /* 见过的小妖精里最大的 `enemy_id`（单调递增 ⇒ 用它认「新」）*/
static float   s_gf_first_y;              /* 本波第一只的出生 **y**（队形的深度）*/
static float   s_gf_tgt_x, s_gf_tgt_y;    /* ★ 最近刷出的那只妖精的出生点（跟随偏移的基准）*/
static float   s_gf_x[FAITH_GF_SAMPLES];
static int32_t s_gf_id[FAITH_GF_SAMPLES];   /* ★★ 2026-09-22：被采样的那几只妖精的 `enemy_id`。
                                           * 用途 = **跟随目标死了之后换一只**（同波任一只的轨迹都一样，
                                           * 见下面 `gf_pick_target()`）—— 只有 x/y 不够，要有身份。*/
static int32_t s_gf_n;
static int32_t s_gf_done;                 /* 本波已补刷 */
static int32_t s_gf_stage = -1;

/* ★★ 2026-09-23（用户实跑：「绿妖精一直有问题，没有出现」）—— **诊断**（只打日志、不改行为）。
 *   判据链上一共有四个**静默**早退（`s_gf_n <= 0` / 隔波门 / 延迟 / 窗口），日志里一条 `faithgf:` 都没有
 *   ⇒ 分不清是「没看见妖精」还是「看见了但被门挡住」。这一行把本帧的原始观测摊开：
 *   敌机数、被判成「妖精」的只数、波次账（wave/n/done/cancel），以及**见过的外观 (src,script)**
 *   —— 外观对不上的话，照着它把 `mof_green_is_fairy()` 的区间改对（那份区间当年只按 st01 系普查过）。
 *   节流：每 `FAITH_GF_DBG_EVERY` 帧一行；只在「这一段里有敌机」时才打。*/
#define FAITH_GF_DBG_EVERY 300
static int32_t s_gf_dbg_frame;
static int32_t s_gf_dbg_enemy, s_gf_dbg_fairy;
static int32_t s_gf_dbg_pair[8][2];
static int32_t s_gf_dbg_np;
static void gf_dbg_pair(int32_t src, int32_t script)
{
    int i;
    for (i = 0; i < s_gf_dbg_np; ++i)
        if (s_gf_dbg_pair[i][0] == src && s_gf_dbg_pair[i][1] == script) return;
    if (s_gf_dbg_np < 8) {
        s_gf_dbg_pair[s_gf_dbg_np][0] = src;
        s_gf_dbg_pair[s_gf_dbg_np][1] = script;
        ++s_gf_dbg_np;
    }
}
static int32_t s_gf_wave;                 /* 本关第几波（1 起；用来实现 `FAITH_GF_EVERY`）*/
static int32_t s_gf_trig_src, s_gf_trig_script;   /* 触发这一波的那只妖精的外观（诊断用）*/
/* ★★ 2026-09-18c（用户实跑口径「**妖精应当跟随那个队列才对**」）：补刷出来的绿妖精要**跟着同一波的妖精
 * 一起动**。通道 = 它自己的 ECL float 槽 `%F2/%F3`（= `enemy+0x14d0/+0x14d4`，一手
 * `Enemy__ecl_get_float_global_ptr 0x4387e0`）：卡侧每帧写「目标妖精坐标 + 出生时定下的队形偏移」，
 * 我们那支子程序每帧 `movePosTime(1,1,%F2,%F3)` 走过去（它自己**不设**任何运动）。
 * `tgt_id` = 它跟着的那只妖精（本波最近刷出的一只）；目标没了就换 `s_gf_tgt_id`，再没了就喊它走。*/
typedef struct {
    int32_t id;          /* 这只绿妖精的 `enemy_id`（0 = 空槽）*/
    int32_t tgt_id;      /* 跟着的那只妖精的 `enemy_id`（0 = 还没定）*/
    float   off_x, off_y;/* 出生时定下的队形偏移（相对目标，常数 ⇒ 与目标同轨迹）*/
    float   last_x, last_y; /* 诊断：最后一帧写进 `%F2/%F3` 的点 */
    float   y0;          /* ★ 2026-09-24：**出生时的 y**（= 入场线）—— 入场那一段从它插值下来，
                          *   见 `gf_follow_tick()`（owner：「有一个绿色妖精瞬间出现了一帧
                          *   然后直接消失了」）。*/
    int32_t lost;        /* 目标找不到的连续帧数 */
    int32_t wave;        /* ★★ 2026-09-22：它属于第几波（`s_gf_wave`）—— **换目标不许跨波**：
                          *   否则上一波那只绿妖精会挂到下一波的妖精身上，永远有人可跟 ⇒ 既不 `lost`
                          *   也不离场 ⇒ 占着 8 个跟随槽位（槽满 = 后面的绿妖精没人写位置 = 又站住不动）。*/
    int32_t age;         /* ★★ 2026-09-22：存活帧数 —— 兜底寿命（见 `FAITH_GF_TTL`）。
                          *   没有它的话「目标一直活着」的那只可以永远留在场上。*/
} gf_fol_t;

#define FAITH_GF_LOST_FRAMES 90        /* 目标没了这么久 ⇒ 喊它飞离自删（❓自定：1.5 s @60fps）*/

static gf_fol_t s_gf_fol[MOF_GREEN_EXTRA_SLOT_MAX];
static int32_t s_gf_tgt_id;              /* 本波**最近刷出**的那只妖精（新妖精出生时更新）*/
static int32_t s_gf_wave_id;              /* 本波第一只妖精的 `enemy_id`（判「别的种类是这波之后才来的」）*/
static int32_t s_gf_other_id;             /* 见过的「**非妖精**」外观里最大的 id（水位线）*/
static int32_t s_gf_cancel;               /* ★ 本波出现过大妖精/阴阳玉/幽灵/boss ⇒ 撤销补刷 */
static int32_t s_gf_stat;                 /* 诊断：一共刷了几只 */

/* 「**妖精**」的外观（`enemy.anm` 的扁平脚本号；一手 = `native/tools/measure_enemy_variants.py`
 * 的变体表 + `local/_scratch/th18_wave_density.py` 的波次实例普查）：
 *   A 组 0–19（GirlRedA\* / GirlBlueA\* / GirlA0x）、GirlB/GirlC 20–39、
 *   B 组 184–203（st02/03/07 的蓝/红妖精）；C 组 159–178 也收（ECL 零站点，收了也无害）。
 *
 * ★★ 2026-09-18c **用户口径（逐字）**：「我忘说了，不是**所有 for 循环**都加绿妖精，而是**妖精循环**
 *   加绿妖精」⇒ **只有刷出「妖精」的那些波**才补刷，而且判据是**这只敌机长什么样**（脚本号），
 *   不是"哪段 ECL 循环"（引擎里拿不到循环身份；外观是唯一稳的机器可读判据）。
 * **不含**（因此这些波永远不会被补刷）：
 *   · 大妖精 40–44 / 224–228（64×64，剪影没有绿色版）—— 实跑里用户叫它「大蝴蝶」；
 *   · **阴阳玉 `Cir*` 45–80**（`Cir00m/p` 等；绿的那批本来就是宿主，红的不补）；
 *   · `Phantom*` 91–110（幽灵）、以及所有 boss/中 boss 的脚本。*/
static int gf_is_fairy(int32_t src, int32_t script)
{
    return mof_green_is_fairy(src, script);           /* 判据在 core（可离线单测）*/
}

/* 这只敌机的 `(N,S)` 外观身份（一手与出处见 `engine.h` 的 `CE_ENEMY_ANM_SRC/_SCRIPT`）：
 * `N` = `anmSelect` 的选择子、`S` = 槽 0 的主脚本号。绿妖精载体与宿主表都靠它认人。*/
static void gf_anm_identity(const uint8_t *e, int32_t *src, int32_t *script)
{
    if (src)    *src    = e ? *(const int32_t *)(e + CE_ENEMY_ANM_SRC)    : -1;
    if (script) *script = e ? *(const int32_t *)(e + CE_ENEMY_ANM_SCRIPT) : -1;
}

/* ★★ 2026-09-22：**这一只敌机是不是我们自己补刷的绿妖精**。
 * 为什么必须有这条判据（否则会自噬）：我们刷出来的那只外观是 `(2,10)` —— 它**也是「妖精」**，
 * 于是每帧的扫敌会把它当成「新出生的妖精」：① 混进队形采样（`s_gf_x[]` / `s_gf_n`），
 * ② 刷新 `s_gf_last`（波次边界的判据），③ 抢走 `s_gf_tgt_id`（下一波的跟随目标会变成上一波的绿妖精，
 * 而它自己的位置又是别人给的 ⇒ 跟随链**套娃**）。
 * 判据 = 「在不在本卡的补刷表 `s_gf_fol[]` 里」。*/
static int gf_is_ours(int32_t id)
{
    int i;
    if (!id) return 0;
    for (i = 0; i < MOF_GREEN_EXTRA_SLOT_MAX; ++i)
        if (s_gf_fol[i].id == id) return 1;
    return 0;
}

/* ★★ 2026-09-22：给一只补刷妖精挑**当下还活着**的跟随目标。
 * 规则（同一波内任一成员的轨迹都一样 ⇒ 换谁都能保持队形）：
 *   ① 现在的目标还活着 ⇒ 不变（避免每帧换人造成抖动）；
 *   ② `wave != s_gf_wave`（本波已经翻页）⇒ 直接返回 0，**不许跨波换目标**；
 *   ③ 否则本波**最近刷出**的那只（`s_gf_tgt_id`）；
 *   ④ 否则从本波采样表里**从后往前**（最近的优先）挑第一只还活着的；
 *   ⑤ 都没有 ⇒ 返回 0（调用方累计 `lost`，到点喊它飞离）。
 * ⚠ 只在**这一帧**用 `ce_enemy_by_id()` 判活；不缓存指针（`METHOD.md`：「禁止缓存 ev->enemy」同源）。*/
static int32_t gf_pick_target(uint8_t *em, int32_t cur, int32_t wave)
{
    int i;
    if (cur && ce_enemy_by_id(em, cur)) return cur;
    if (wave != s_gf_wave) return 0;                     /* ★ 跨波不换（见 `gf_fol_t.wave`）*/
    if (s_gf_tgt_id && s_gf_tgt_id != cur && ce_enemy_by_id(em, s_gf_tgt_id)) return s_gf_tgt_id;
    for (i = s_gf_n - 1; i >= 0; --i) {
        const int32_t id = s_gf_id[i];
        if (id && id != cur && ce_enemy_by_id(em, id)) return id;
    }
    return 0;
}

static void gf_reset(int32_t stage)
{
    s_gf_stage = stage;
    s_gf_frame = 0;
    s_gf_last = 0;
    s_gf_first_y = 0.0f;
    s_gf_n = 0;
    s_gf_done = 0;
    s_gf_wave = 0;
    s_gf_cancel = 0;
    s_gf_tgt_id = 0;
    s_gf_tgt_x = s_gf_tgt_y = 0.0f;
    memset(s_gf_id, 0, sizeof s_gf_id);
    memset(s_gf_fol, 0, sizeof s_gf_fol);      /* ★ 跟随表：换关/换局就断了跟踪（场上的自己会飞走）*/
    /* ⚠ `s_gf_max_id` / `s_gf_other_id` **不重置**：它们只是「我见过哪些实例」的水位线，跨关继续单调递增。*/
}

static void gf_spawn_one(float x, float y, float tgt_x, float tgt_y)
{
    uint8_t sp[CE_SPAWN_SIZE];
    uint8_t *e;
    uint8_t *mgr = CE_ENEMY_MGR();
    float off_x = 0.0f, off_y = 0.0f;

    /* ★★ 2026-09-18c：**自己判敌人数上限** —— 一手：ECL 的 `enmCreate` 那条路有这道门
     *   （`th18:0x4369ec mov eax,[edx+0x198]` → `0x4369f5 cmp eax,[edx+0x8c]` → `jge` 跳过），
     *   而**卡直接调 `allocate_new_enemy` 时引擎不给**（这条也写在同事那份
     *   `engine/enemy/th18/01-spawn-api-and-card-summons.md` 里）⇒ 我们自己补上，
     *   免得在敌机拥挤的波次上把敌人池顶爆。*/
    if (!mgr) return;
    if (*(const int32_t *)(mgr + CE_EM_ENEMY_COUNT) >= *(const int32_t *)(mgr + CE_EM_ENEMY_MAX)) {
        ce_log("faithgf: green extra fairy skipped —— 敌人数到上限（%d >= %d；ECL 那条路同款门）",
               (int)*(const int32_t *)(mgr + CE_EM_ENEMY_COUNT),
               (int)*(const int32_t *)(mgr + CE_EM_ENEMY_MAX));
        return;
    }
    memset(sp, 0, sizeof sp);
    *(float   *)(sp + CE_SPAWN_POS + 0) = x;
    *(float   *)(sp + CE_SPAWN_POS + 4) = y;
    *(float   *)(sp + CE_SPAWN_POS + 8) = 0.0f;
    *(int32_t *)(sp + CE_SPAWN_SCORE)     = FAITH_GF_SCORE;
    *(int32_t *)(sp + CE_SPAWN_MAIN_DROP) = 0;              /* 纯信仰点：原生什么都不掉 */
    *(int32_t *)(sp + CE_SPAWN_LIFE)      = FAITH_GF_LIFE;
    *(int32_t *)(sp + CE_SPAWN_ECL_I + 0) = 0;              /* `$I0` = 999 时才飞离自删（卡侧喊走）*/
    *(float   *)(sp + CE_SPAWN_ECL_F + 0) = 0.0f;           /* `%F0/%F1` 现在不用（子程序自己不设运动）*/
    *(float   *)(sp + CE_SPAWN_ECL_F + 4) = 0.0f;
    /* ★ 跟随通道的初值：`%F2/%F3` = 出生点（= 队形槽位）*/
    *(float   *)(sp + CE_SPAWN_ECL_F + 8)  = x;
    *(float   *)(sp + CE_SPAWN_ECL_F + 12) = y;
    e = (uint8_t *)ce_spawn_enemy(FAITH_GF_ECL_NAME, sp);
    if (e) {
        int32_t id = *(const int32_t *)(e + CE_ENEMY_ID);
        int i;
        ++s_gf_stat;
        /* ★ 记进跟随表：偏移 = 队形槽位 − 目标出生点（常数）⇒ 之后它与目标**同轨迹**。*/
        mof_green_extra_follow_offset(x, y, tgt_x, tgt_y, &off_x, &off_y);
        for (i = 0; i < MOF_GREEN_EXTRA_SLOT_MAX; ++i) {
            if (s_gf_fol[i].id == 0) {
                s_gf_fol[i].id = id;
                s_gf_fol[i].tgt_id = s_gf_tgt_id;
                s_gf_fol[i].off_x = off_x;
                s_gf_fol[i].y0    = (y < 0.0f) ? y : -32.0f;   /* 入场线（出生点通常已在场地之上）*/
                s_gf_fol[i].off_y = off_y;
                s_gf_fol[i].lost = 0;
                s_gf_fol[i].wave = s_gf_wave;      /* ★ 归属本波：换目标不许跨波（见 `gf_fol_t`）*/
                s_gf_fol[i].age = 0;
                break;
            }
        }
        ce_log("faithgf: green extra fairy @(%.0f,%.0f) 跟随 #%d 偏移 (%+.0f,%+.0f)"
               " (wave frame %d, %d obs, trigger anm %d/%d, wave#%d, total %d)",
               (double)x, (double)y, (int)s_gf_fol[i < MOF_GREEN_EXTRA_SLOT_MAX ? i : 0].tgt_id,
               (double)off_x, (double)off_y,
               (int)s_gf_frame, (int)s_gf_n, (int)s_gf_trig_src, (int)s_gf_trig_script,
               (int)s_gf_wave, (int)s_gf_stat);
    } else {
        ce_log("faithgf: green extra fairy spawn FAILED —— `%s` 不在已加载的 default.ecl 里？"
               "（`make ecl-ufo` + `make dist` + 发布那一步）", FAITH_GF_ECL_NAME);
    }
}

/* ★★ 2026-09-18c：每帧把「目标妖精坐标 + 出生时的队形偏移」写进每只补刷妖精的 `%F2/%F3`
 * （= `enemy+0x14d0/+0x14d4`）—— 子程序那边只是 `movePosTime(1,1,%F2,%F3)`，所以它**跟着同一波的
 * 妖精同轨迹运动**（用户口径：「妖精应当跟随那个队列才对」）。
 * 目标丢了的处理：`gf_pick_target()` 先换成本波最近那只、再换采样表里最近一只还活着的；
 * 都没了就累计 `lost`，超过 `FAITH_GF_LOST_FRAMES` 就写 `$I0 = 999` 喊它飞离自删（不留在场上当孤儿）。
 *
 * ★★ 2026-09-22（th10 #2「绿色妖精队列错误」的根因修复）：**本函数必须每帧都跑**。
 *   旧写法把它放在 `faith_green_fairy_tick()` 的**补刷窗口之后**（`if (s_gf_done || …) return;` 那段的下方），
 *   而补刷成功的那一帧就把 `s_gf_done = 1` ⇒ 此后的每一帧都在那三个早退里返回 ⇒
 *   本函数**一次都没为刚刷出来的那只跑过**（补刷那一帧它还没进表，下一帧已经 `s_gf_done`）。
 *   后果：`%F2/%F3` 永远是 `gf_spawn_one()` 写进去的**出生点**，子程序的
 *   `movePosTime(1,1,%F2,%F3)` 于是每帧都把它搬回出生点 ⇒ 它**钉在入场线上不动**、
 *   也不会收到 `$I0 = 999`（那行也在这里）⇒ 既不入队、也永不离场。用户看到的「队列错误」
 *   就是这一条。（2026-09-18c 的「跟随」机制本身是对的，只是调用点被早退挡住了。）*/
static void gf_follow_tick(uint8_t *em)
{
    int i;
    for (i = 0; i < MOF_GREEN_EXTRA_SLOT_MAX; ++i) {
        gf_fol_t *f = &s_gf_fol[i];
        uint8_t *me, *tg;
        const float *tp;
        int32_t tid;

        if (f->id == 0) continue;
        me = ce_enemy_by_id(em, f->id);
        if (!me) {
            ce_log("faithgf: 绿妖精 #%d 不在了（被打死 / 自删；age=%d wave#%d，最后一次写的 F2/F3="
                   "(%.1f,%.1f)、偏移(%+.0f,%+.0f)）",
                   (int)f->id, (int)f->age, (int)f->wave,
                   (double)f->last_x, (double)f->last_y, (double)f->off_x, (double)f->off_y);
            f->id = 0;
            continue;
        }
        /* ★ 2026-09-22：**寿命兜底** —— 目标一直活着、也没被打死的那种，不能永远留在场上。*/
        /* ★★ 2026-09-24 诊断（owner：「**依旧没有绿色妖精**」）：偏移已经修好（日志里 `(+0,+0)`/`(+40,+0)`），
         *   那就得看**引擎里那支敌机自己**的状态 —— 出生后 +30 / +150 帧各读一次：
         *   位置 / 外观 (N,S) / 目标与偏移 / 我们写进 `%F2/%F3` 的点。
         *   三种可能的读法：① 已经不在敌人表里（被打死 / 自删）；② 位置在场地外；
         *   ③ 外观不是 `(2,10)`（那我们写错了 `anmSetMain` 的参数）。*/
        /* ★★ 2026-09-24：探针挪到**出生后 1/2/3/10 帧** —— 实跑日志里 7 次补刷**全部在 `age=2`
         *   就消失**（那时入场段还没让它进场地、火力够不到）⇒ 必须在消失前把状态打出来。*/
        if (f->age == 1 || f->age == 2 || f->age == 3 || f->age == 10) {
            const float *mp = ce_enemy_pos(me);
            ce_log("faithgf/probe: 绿妖精 #%d age=%d pos=(%.0f,%.0f) anm=(%d,%d) 跟随 #%d 偏移(%+.0f,%+.0f) "
                   "写F2/F3=(%.0f,%.0f) wave#%d",
                   (int)f->id, (int)f->age,
                   mp ? (double)mp[0] : 0.0, mp ? (double)mp[1] : 0.0,
                   (int)*(const int32_t *)(me + CE_ENEMY_ANM_SRC),
                   (int)*(const int32_t *)(me + CE_ENEMY_ANM_SCRIPT),
                   (int)f->tgt_id, (double)f->off_x, (double)f->off_y,
                   (double)*(const float *)(me + CE_ENEMY_ECL_F2),
                   (double)*(const float *)(me + CE_ENEMY_ECL_F3),
                   (int)f->wave);
        }
        if (++f->age > FAITH_GF_TTL) {
            *(int32_t *)(me + CE_ENEMY_ECL_I0) = 999;
            ce_log("faithgf: 绿妖精 #%d 到寿命上限（%d 帧）⇒ 喊它飞离", (int)f->id, (int)FAITH_GF_TTL);
            f->id = 0;
            continue;
        }
        tid = gf_pick_target(em, f->tgt_id, f->wave);
        if (tid != f->tgt_id) f->tgt_id = tid;
        tg = tid ? ce_enemy_by_id(em, tid) : 0;
        if (!tg) {
            if (++f->lost > FAITH_GF_LOST_FRAMES) {
                *(int32_t *)(me + CE_ENEMY_ECL_I0) = 999;   /* 喊它飞离自删 */
                ce_log("faithgf: 绿妖精 #%d 的目标丢了 %d 帧 ⇒ 喊它飞离（wave#%d）",
                       (int)f->id, (int)FAITH_GF_LOST_FRAMES, (int)f->wave);
                f->id = 0;
            }
            continue;
        }
        f->lost = 0;
        tp = ce_enemy_pos(tg);
        if (!tp) continue;
        {   /* ★ 2026-09-24：**最终位置夹进游玩区**。换目标（`gf_pick_target`）之后偏移不变，
             *   而新目标可能在另一头 ⇒ 不夹的话这支补刷妖精又会被带出屏幕（owner 报「看不见」）。
             *   ★★ 同一轮：**入场段**（前 `FAITH_GF_ENTER_FRAMES` 帧）y 从**出生时的入场线**
             *   插值到目标那一格 —— owner 实跑「有一个绿色妖精**瞬间出现了一帧然后直接消失**」：
             *   原来第一帧就把 y 写成目标的 y（可能已在场地深处、正贴着火线）⇒ 它是**瞬移进场**
             *   然后当帧被打掉；按同波妖精的入场节奏从上面飞下来才看得见
             *   （也正是「补一只同队形成员」的原意）。*/
            float fx = tp[0] + f->off_x;
            float fy = tp[1] + f->off_y;
            if (f->age < FAITH_GF_ENTER_FRAMES) {
                const float u = (float)f->age / (float)FAITH_GF_ENTER_FRAMES;
                fy = f->y0 + (fy - f->y0) * u;
            }
            if (fx >  FAITH_GF_X_LIMIT) fx =  FAITH_GF_X_LIMIT;
            if (fx < -FAITH_GF_X_LIMIT) fx = -FAITH_GF_X_LIMIT;
            f->last_x = fx; f->last_y = fy;
            *(float *)(me + CE_ENEMY_ECL_F2) = fx;
            *(float *)(me + CE_ENEMY_ECL_F3) = fy;
        }
    }
}

/* ★★ 2026-09-22：**删卡 / 换局时把场上的补刷妖精收掉**（公共入口 `faith_green_fairy_depart_all()`）。
 * `METHOD.md` 铁律二：任何跨帧留下东西的效果都要交代「被删掉会怎样」（删卡发生在**商店**里，
 * 那一刻 `on_tick_2` 已经不再跑 ⇒ 上面那条跟随/离场通道**断得干干净净**）。
 * 留在这条路上的后果（旧版就是这样）：那几只绿妖精的 `%F2/%F3` 冻在最后一次写入值上、
 * `$I0` 永远是 0 ⇒ 子程序既不跟人走、也永不 `delete()` ⇒ **它们会钉在关卡里直到本局结束**。
 * 做法 = 与跟随丢失时同一条口令：写 `$I0 = 999`（子程序 `moveVel(-π/2, 3.0)` 飞离 + `delete()`）。
 * ⚠ 只碰**我们自己的**实例（`s_gf_fol[]` 里的 id，`ce_enemy_by_id()` 当帧复核）；写之前必须
 *   校验指针还在（商店里 `ENEMY_MGR` 可能是空的 / 那只已经不在了）。
 * ⚠ **110 与 111 都要调它**（各自的 `on_clear`）—— 谁被删掉，那一侧的 tick 就没了；幂等，
 *   两卡同装时先跑到的那个清空表、后一个什么都不做。*/
static void gf_depart_all(void)
{
    uint8_t *em = CE_ENEMY_MGR();
    int i, n = 0;
    if (em) {
        for (i = 0; i < MOF_GREEN_EXTRA_SLOT_MAX; ++i) {
            uint8_t *me;
            if (s_gf_fol[i].id == 0) continue;
            me = ce_enemy_by_id(em, s_gf_fol[i].id);
            if (me) {
                *(int32_t *)(me + CE_ENEMY_ECL_I0) = 999;
                ++n;
            }
        }
    }
    if (n) ce_log("faithgf: green fairy depart x%d（删卡/换局 ⇒ $I0 = 999，子程序飞离自删）", n);
    memset(s_gf_fol, 0, sizeof s_gf_fol);
    /* ★ 与搬家前的 `mof_clear()` **逐行等价**：那一版清的是 `s_ge_stage/s_ge_n/s_ge_done/s_ge_last`
     *   （同一组值，现在住在本文件的静态区）⇒ 110 的行为不变。
     *   `s_gf_max_id` / `s_gf_other_id` 是「我见过哪些实例」的水位线，**不重置**。*/
    s_gf_stage = -1;        /* 下一局重新划波次（下一个 tick 会走 `gf_reset()`）*/
    s_gf_n = 0;
    s_gf_done = 0;
    s_gf_last = 0;
}

/* 公共收尾入口（`faith.h`）：110 与 111 的 `on_clear` 都调它。*/
void faith_green_fairy_depart_all(void)
{
    gf_depart_all();
}

/* 每帧：认新妖精 → 划波次 → 隔 N 帧补刷 + 跟随（只由卡的 `on_tick_2` 调 ⇒ 卡不在场时一次都不跑）。
 * ★★ 2026-09-22（P6）：**公共入口**（`faith.h`），110 与 111 各调一次；自带帧守卫 ⇒ 一帧只跑一遍。*/
void faith_green_fairy_tick(void)
{
    const int32_t frame = (int32_t)ce_sdk_frame();   /* ★ 引擎帧号（本段所有比较都是相对差，见文件头）*/
    uint8_t *em;
    int32_t stage;
    /* ★ 帧守卫（与 `faith_pool_tick()` / `faith_convert_full_p_items()` 同一口径）：
     *   110 与 111 同装时两卡各调一次 ⇒ 不加这道门就会**一帧推两遍**（波次计数/采样/补刷全部翻倍）。*/
    {
        static uint32_t s_last_frame;
        static int      s_have_last;
        const uint32_t now = ce_sdk_frame();
        if (s_have_last && now == s_last_frame) return;
        s_last_frame = now;
        s_have_last = 1;
    }
    em = CE_ENEMY_MGR();
    stage = CE_CURRENT_STAGE();

#if !FAITH_GF_ENABLE
    (void)frame; (void)em; (void)stage;
    return;
#else
    if (!em) return;
    if (stage != s_gf_stage) gf_reset(stage);

    {
        uint8_t *e;
        CE_ENEMY_FOREACH(em, e) {
            int32_t src = 0, script = 0, id;
            gf_anm_identity(e, &src, &script);
            id = *(const int32_t *)(e + CE_ENEMY_ID);
            ++s_gf_dbg_enemy;                          /* ★ 诊断：本帧敌机总数 */
            gf_dbg_pair(src, script);                  /* ★ 诊断：见过的外观（不管是不是妖精）*/
            /* ★★ 2026-09-22：**我们自己补刷的那几只不算「本波的妖精」**（`gf_is_ours()` 的长注：
             *   不排除它们就会混进队形采样、刷新波次边界、并抢走下一波的跟随目标 —— 跟随链套娃）。*/
            if (gf_is_ours(id)) continue;
            if (!gf_is_fairy(src, script)) {
                /* ★★ 2026-09-18c（用户：「**三只大蝴蝶那里不应该有绿妖精！**」）：
                 * 这一波只要出现**别的种类**（大妖精 40–44/224–228、阴阳玉 `Cir*`、`Phantom*`、boss…），
                 * 就**撤销**本波的补刷 —— 否则「上一波妖精触发、延迟十几帧才落地」的那只绿妖精
                 * 会正好落在它们的画面上（实跑就是这么被看到的）。*/
                if (id > s_gf_other_id) {
                    s_gf_other_id = id;
                    if (s_gf_n > 0 && s_gf_wave_id != 0 && id > s_gf_wave_id) s_gf_cancel = 1;
                }
                continue;
            }
            ++s_gf_dbg_fairy;                          /* ★ 诊断：被判成「妖精」的只数 */
            if (id <= s_gf_max_id) continue;                  /* 老面孔 */
            s_gf_max_id = id;
            if (s_gf_last != 0 && frame - s_gf_last > FAITH_GF_GAP) {   /* 隔太久 ⇒ 新的一波 */
                s_gf_n = 0;
                s_gf_done = 0;
                s_gf_cancel = 0;
                ++s_gf_wave;
            } else if (s_gf_wave == 0) {
                s_gf_wave = 1;                                        /* 本关第一波 */
            }
            if (s_gf_n == 0) {
                const float *p = ce_enemy_pos(e);
                s_gf_frame = frame;
                s_gf_first_y = p ? p[1] : 0.0f;      /* 队形的深度（补刷的那只与它同深）*/
                s_gf_trig_src = src;                 /* 诊断：触发这一波的外观 */
                s_gf_trig_script = script;
                s_gf_wave_id = id;                   /* 本波第一只的 id（判「别的种类是这波之后才来的」）*/
            }
            if (s_gf_n < FAITH_GF_SAMPLES) {
                const float *p = ce_enemy_pos(e);
                s_gf_x[s_gf_n] = p ? p[0] : 0.0f;
                s_gf_id[s_gf_n] = id;                /* ★ 记身份：目标死了要换一只（见 `gf_pick_target()`）*/
                ++s_gf_n;
            }
            {   /* ★ 跟随目标 = 本波**最近刷出**的那只妖精（记下它的出生点，偏移以它为基准）*/
                const float *p = ce_enemy_pos(e);
                s_gf_tgt_id = id;
                s_gf_tgt_x = p ? p[0] : 0.0f;
                s_gf_tgt_y = p ? p[1] : 0.0f;
            }
            s_gf_last = frame;
        }
    }

    /* ★★ 2026-09-22：**跟随通道每帧都跑**（在下面三个「不补刷」的早退之前）——
     *   已经刷出去的那几只要一直有 `%F2/%F3` 才会跟着队列走，而补刷一旦发生就 `s_gf_done = 1`
     *   ⇒ 放在早退之后等于「只为 0 只服务」。根因全文见 `gf_follow_tick()`。*/
    gf_follow_tick(em);

    /* ★ 诊断（节流）：一行说清「这一段引擎给了什么、我们判成什么、这一波的账卡在哪一步」。
     *   ⚠ 不用 `snprintf`（本 TU 没链 stdio）⇒ 最多 6 对外观用固定字段打出来。*/
    if (frame - s_gf_dbg_frame >= FAITH_GF_DBG_EVERY && s_gf_dbg_enemy > 0) {
        int32_t ps[6], pt[6];
        int i;
        s_gf_dbg_frame = frame;
        for (i = 0; i < 6; ++i) {
            ps[i] = (i < s_gf_dbg_np) ? s_gf_dbg_pair[i][0] : -1;
            pt[i] = (i < s_gf_dbg_np) ? s_gf_dbg_pair[i][1] : -1;
        }
        ce_log("faithgf/dbg: 帧 %d 敌 %d 妖精 %d | wave#%d n=%d done=%d cancel=%d last=%d "
               "trig=(%d,%d) | 外观 (%d,%d)(%d,%d)(%d,%d)(%d,%d)(%d,%d)(%d,%d)",
               (int)frame, (int)s_gf_dbg_enemy, (int)s_gf_dbg_fairy,
               (int)s_gf_wave, (int)s_gf_n, (int)s_gf_done, (int)s_gf_cancel, (int)s_gf_last,
               (int)s_gf_trig_src, (int)s_gf_trig_script,
               (int)ps[0], (int)pt[0], (int)ps[1], (int)pt[1], (int)ps[2], (int)pt[2],
               (int)ps[3], (int)pt[3], (int)ps[4], (int)pt[4], (int)ps[5], (int)pt[5]);
        s_gf_dbg_enemy = s_gf_dbg_fairy = s_gf_dbg_np = 0;
    }
    if (s_gf_done || s_gf_n <= 0) return;
    if (frame < s_gf_frame + FAITH_GF_DELAY) return;            /* 先攒几个出生点（认队形）*/
    /* ★ 这一波**还在刷**才补：否则画面已经是下一波了（用户实跑「绿妖精跟阴阳玉/大蝴蝶混在一起」）*/
    if (frame - s_gf_last > FAITH_GF_WINDOW) { s_gf_done = 1; return; }
    /* ★★ 这一波出现过**别的种类**（大妖精=大蝴蝶 / 阴阳玉 / 幽灵 / boss）⇒ 本波**不补**
     *   （用户 2026-09-18c 原话：「三只大蝴蝶那里不应该有绿妖精！」）*/
    if (s_gf_cancel) {
        s_gf_done = 1;
        ce_log("faithgf: green extra fairy CANCELLED for wave#%d —— 这一波出现了非妖精外观"
               "（大妖精/Cir*/Phantom*/boss；见 `mof_green_is_fairy()`）", (int)s_gf_wave);
        return;
    }

    {
        float slots[MOF_GREEN_EXTRA_SLOT_MAX];
        int i, n;
        /* ⚠ **不采样同波妖精的入场向量**（v1）：算角要用 `atan2f`，而 32 位目标上 float 返回走
         * x87（`make dllx87`：本仓 0 条 x87 的硬线）⇒ 这里的入场向量就用**原生小妖精那一套**
         * （`GirlA01` 体：`%F0 = π/2 向下`、`%F1 = 1.0`），队形一致性靠**出生点**给（见
         * `mof_green_extra_slots()`）。要真对齐同波妖精的向量，得改用整数近似或直接读它的
         * 速度字段（记为后续可选优化，`TUNING.md` §2.0h）。
         * ★ 2026-09-18：删掉两个**未用**的局部（`angle` / `speed`）—— 出生已经走 `gf_spawn_one()`
         *   （它的入场上抛角/速度是自己的常量），这两个是搬走之后留下的壳
         *   （`faith.h` 一改，mof.c 被重编才暴露出来）。*/
        float ey = s_gf_first_y;
        if (FAITH_GF_EVERY > 0 && (s_gf_wave % FAITH_GF_EVERY) != 0) {   /* 这一波不补（例如隔波）*/
            s_gf_done = 1;
            return;
        }
        /* 观测到的 y 只在「像入场线」时才用（否则它已经进场了，用标准入场线让它从上面下来）*/
        if (ey > FAITH_GF_ENTRY_BAND || ey < -FAITH_GF_ENTRY_BAND) ey = FAITH_GF_ENTRY_Y;
        /* ★★★ 2026-09-24（决定性探针数据）：`faithgf/probe: 绿妖精 #27 age=1 pos=(0,0) anm=(2,10)` →
         *   下一行就「不在了（age=2）」。外观 `(2,10)` 是对的、位置却是 `(0,0)`，而且它出生在
         *   **y = −32（场地之上）** ⇒ 结论：**它一出生就踩了「不在场地内」的回收** —— ECL 自己
         *   `enmCreate` 出来的妖精带入场状态（引擎不回收），而**卡侧 `allocate_new_enemy()` 出来的
         *   没有那一层**（112 的 UFO 令牌之所以没踩到，是因为它一直在场地内出生）。
         *   ⇒ 出生 y **夹进场地**（≥ `FAITH_GF_SPAWN_Y_MIN`）；「从上面飞下来」那一段仍由入场段
         *   （`FAITH_GF_ENTER_FRAMES`）从**这个** y 插值到队形槽位 ⇒ 观感不变，但不会被回收。*/
        if (ey < FAITH_GF_SPAWN_Y_MIN) ey = FAITH_GF_SPAWN_Y_MIN;
        n = mof_green_extra_slots(s_gf_x, s_gf_n, FAITH_GF_COUNT, slots, MOF_GREEN_EXTRA_SLOT_MAX);
        for (i = 0; i < n; ++i) {
            float sx = slots[i];
            float tx = s_gf_tgt_x, ty = s_gf_tgt_y;                  /* 缺省 = 本波最后那只 */
            int j;
            /* ★★★ 2026-09-24（owner：「**我依旧没有看见绿妖精**」）：跟随目标改成
             *   **离本槽位最近的那只同波妖精**，而不是「本波最后刷出的那只」。
             *   根因（实跑日志）：`@(176,-32) 跟随 #26 偏移 (+360,+0)` —— 队形横向铺开时
             *   「最后刷出的那只」可能在另一头（x=−184），而偏移 = 槽位 − 目标 ⇒ **+360 px**；
             *   跟随通道每帧写 `%F2/%F3 = 目标 + 偏移` ⇒ 那支妖精一出生就被**恒定偏移**带出
             *   游玩区（x ∈ −192..192）⇒ 永远看不见。取最近的那只 ⇒ 偏移只有「一格队形」那么宽。*/
            {
                float best = 1.0e9f;
                for (j = 0; j < s_gf_n && j < FAITH_GF_SAMPLES; ++j) {
                    float d = s_gf_x[j] - sx;
                    if (d < 0.0f) d = -d;
                    if (d < best) {
                        best = d;
                        tx = s_gf_x[j];
                        ty = ey;                    /* 同一条入场线（y 用本波入场线）*/
                    }
                }
            }
            if (tx >  FAITH_GF_X_LIMIT) tx =  FAITH_GF_X_LIMIT;
            if (tx < -FAITH_GF_X_LIMIT) tx = -FAITH_GF_X_LIMIT;
            if (sx >  FAITH_GF_X_LIMIT) sx =  FAITH_GF_X_LIMIT;      /* ★ 夹进游玩区 */
            if (sx < -FAITH_GF_X_LIMIT) sx = -FAITH_GF_X_LIMIT;
            gf_spawn_one(sx, ey, tx, ty);
        }
        s_gf_done = 1;
    }
#endif
}
/* ★★★ 2026-09-30（owner 实跑：「**107 卡会使得有些妖精变成绿妖精**」）：**换皮驱动只在
 *   「变绿这件事的主家」在场时才跑**。那个点池**不是 110/111 专用的** —— **107 也注册在里面**
 *   （它的「大樱点」实体走同一个池，`senro.c` 的 `faith_register`），而 `faith_pool_tick()` 原来
 *   是**无条件**调 `faith_green_swap_tick()` ⇒ **107 单装时每帧都会把场上的妖精改成绿妖精**
 *   （`mof.c` 那边「变绿 ⇒ 掉绿点」的判据只在 110 在场时才跑 ⇒ 观感正好是「无缘无故变绿」）。
 *   判据 = **池子里有 110 或 111 注册** —— 正是 owner 2026-09-24 那句「110 或 111 只要装一张就改」。
 *   ⚠ 没做成 `faith_user_t` 的新字段，是为了**不逼 `mof.c` / `signal.c` 跟着改**（共享件最小改动；
 *     要改成显式能力位随时可以，那时这两张卡各加一行 `u.xxx = 1`）。*/
static int faith_green_owner_present(void)
{
    int i;

    for (i = 0; i < s_n_user; ++i) {
        uint32_t id = s_user[i].card_id;
        if (id == 110u || id == 111u) return 1;
    }
    return 0;
}

int faith_pool_tick(void)
{
    /* ★ 换皮驱动（110/111 任一在场才跑；幂等）——判据见上面 `faith_green_owner_present()` 的注。*/
    if (faith_green_owner_present()) faith_green_swap_tick();
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
#define MOF_GREEN_WIPE_HOSTS 0

static uint8_t  s_fgreen_bak[MOF_GREEN_PAIR_MAX * MOF_GREEN_PAIR_BYTES];
static uint32_t s_fgreen_applied;     /* ★ 「此刻真的改上了」位掩码（判据只看它，不看表里的开关）*/
static int32_t  s_fgreen_logged;      /* 汇总日志只打一次（应用/还原各一次）*/
static int32_t  s_fgreen_warned;      /* 运行期同形体检失败只报一次（免得每帧刷屏）*/

/* `zAnmLoaded*`（`enemy.anm`）的 sprite 表基址；拿不到（关卡还没载入 / 没装载）返回 NULL。*/
static uint8_t *faith_enemy_sprite_table(void)
{
    uint8_t *mgr = CE_ENEMY_MGR();
    uint8_t *anm, *tbl;

    if (!mgr) return 0;
    anm = *(uint8_t **)(mgr + CE_EM_ANM_ENEMY);
    if (!anm) return 0;
    if (*(const uint32_t *)(anm + CE_ANM_LOADED_READY) == 0) return 0;
    tbl = *(uint8_t **)(anm + CE_ANM_LOADED_SPRITES);
    return tbl;
}


/* ======================================================================================
 * ★★★ 2026-09-25：**实例级「改妖精」**（owner 实跑否掉整组换皮之后重写的那一套）
 * ======================================================================================
 * owner 口径（逐字）：「绿妖精我放弃了，**走改妖精路线**吧，改的数量就是**只有杂鱼绿色妖精**，
 *   记得和 th12 卡得**错开改，别改成同一只**」+「数量就按照**改一只（对）**就行，和 th12 基本一致」；
 *   实跑又补一刀：「**依旧蓝妖精全绿**」—— 那是旧路线（整组覆盖 sprite 表项）的必然结果
 *   （表是**全局**的：改一行 ⇒ 那一支妖精**全部**变绿）⇒ 本条路线把它换成**按实例**：
 *
 *   · **选**：按 **ECL 波号**（`MainSubNN` 的数字，与 112 星莲船载体同一个键）分组，
 *     每组取**最先到的那一只** = 宿主 #1；同组里与它**镜像对称**的那只 = 宿主 #2
 *     （`|x1+x2| ≤ 16` ∧ `|y1−y2| ≤ 24`，112 同值）⇒ **一波 1~2 只**。
 *   · **改**：只把这一只的外观换成**同一形状的绿色变体** —— 原生槽 0 的 VM 每帧把
 *     `color_1` 的 alpha 写 0（隐掉原生贴图，112 同法），另起一支自己的 VM 播**绿脚本**
 *     （`mof_green_green_script()`：蓝 pose k → 绿 pose k），每帧跟随它的坐标。
 *     ⇒ 其余同类妖精**原样**（这就是 owner 要的「只有那一只（对）是绿的」）。
 *   · **掉**：这一只死时给**绿点**（判据 = `faith_green_node_owns(enemy_id)`，见 `mof.c` 的死亡回调）。
 *   · **错开 112**：每帧对「本关启用的那一行」的 **5 个 pose 全量**重申护栏位
 *     （`ce_fairy_claim_group(110, 2, src_script + k)`）—— 112 读到就**整组避开**（他们只读组级位，
 *     所以我们必须按组通报；实例位也一并通报，将来他们想精确避开时可以直接读）。
 *
 * ⚠ 与旧路线的关系：旧的「整组覆盖 sprite 表项」的机制**一行都没删**（`mof_green_pair_*` 与
 *   `mof_green_plan()` 仍被单测钉住，`faith_green_applied()` 仍是那条路线的「改上了」位），
 *   但**驱动的这一处**现在不再走它 ⇒ `faith_green_applied()` 恒 0 ⇒ `mof.c` 里那条按
 *   「组」判绿的掉点路径自然静默（改由实例判据接管）。
 * ⚠ 建 VM 只在**帧门卫**位次（`faith_pool_tick()` 每帧一次，自带帧守卫）⇒ 不在死亡栈上。*/
#define FAITH_GF_NODE_MAX      6          /* 同时改的实例上限（一波 2 只 + 换波残影 + 余量）*/
#define FAITH_GF_PER_WAVE      2          /* 一波最多改几只：owner「改一只（对）」*/
#define FAITH_GF_WAVE_GAP      20         /* 读不到 ECL 波号时：相邻两只到达间隔 > 它 ⇒ 算新的一波（帧）*/
#define FAITH_GF_WAVE_SETTLE   60         /* 有波号时：一波静默 > 它 ⇒ 关组（防最后一波永远不判）*/
#define FAITH_GF_STALE_TICKS   3          /* 连着这么多帧没在敌人表里见到 ⇒ 收掉这一条的跟皮 */
/* 跟随探针（每这么多 tick 打一行「敌机坐标 vs 我们的皮的坐标」）—— 2026-09-25 加的：
 * owner 实跑报「绿妖精**定住在画面外**」⇒ 这一行让「皮有没有跟着敌人走」变成**日志可判**的事，
 * 不必再靠肉眼（上一次的根因就是皮每帧被重起、永远停在 (0,0)，但日志里只看到一堆「起皮」）。*/
#define FAITH_GF_PROBE_EVERY  60
#define FAITH_GF_HIDE_COLOR    0x00ffffffu/* VM `color_1` 写它 = alpha 0、RGB 白 ⇒ 原生贴图不可见（112 同值）*/

/* ECL 侧字段（与 112 的 `ufo.c` 同源：`lolk.c` / `animal.c` 也是这一套 —— 关卡脚本那只敌人的
 * 当前子程序名，名字里的 `MainSubNN` 数字就是波号）。两处各留一份：112 那条线不共享本文件。*/
#define FAITH_GF_ENEMY_PARENT_ID  0x6834   /* zEnemy.parent_enemy_id（引擎建的关卡脚本 = 0）*/
#define FAITH_GF_ENEMY_ECL_CTX    0x0c     /* zEnemy.ecl.context.current_context */
#define FAITH_GF_ECL_CTX_SUB      0x04     /* zEclRunContext.cur_location.subroutine_index */
#define FAITH_GF_ENEMY_ECL_FM     0x1218   /* zEnemy.ecl.file_manager */
#define FAITH_GF_ECL_FM_SUB_TABLE 0x8c     /* zEclFileManager.subroutines */
#define FAITH_GF_ECL_FM_SUB_COUNT 0x08     /* zEclFileManager 子程序个数 */
#define FAITH_GF_ANM_VM0          0x124    /* zEnemyData.anm_vm_ids[0]（`CE_ENEMY_DATA + 0x124`）*/
#define FAITH_GF_ANM_MAIN         0x270    /* zEnemyData.anm_main（ExpHP；`anmSetMain` K==0 写它）*/
#define FAITH_GF_ANM_SUB          0x274    /* `anmSetMain` K==0 一并清 0 */
#define FAITH_GF_ANM_FLAGS        0x5130   /* flags_lo：`anmSetMain` K==0 置 bit 0x100000（=`anmReset` 的判据）*/
#define FAITH_GF_ANM_FLAG_MAIN    0x100000u
#define FAITH_GF_ANM_LAYERS       0x27c    /* zEnemyData.anm_layers（引擎的 layer 表达式 = `[+0x27c] + 7`）*/

typedef struct {
    int32_t  id;            /* 敌机 id（`CE_ENEMY_ID`，每个实例唯一、单调递增）*/
    int32_t  row;           /* 命中的 `mof_green_pairs` 行号 */
    int32_t  blue_script;   /* 被换掉的蓝脚本（ECL 每换一次动画就更新一次）*/
    int32_t  green_script;  /* 我们的皮 VM 正在播的绿脚本 */
    uint32_t vm;            /* 我们起的皮 VM（`enemy.anm` 的绿脚本）*/
    uint32_t native_vm;     /* 原生槽 0 的 VM（隐它用；ECL 重起 VM 时会变）*/
    uint32_t native_color;  /* ⛔ 旧做法（隐原生贴图）的残留字段，现在不用了 */
    int32_t  skin_count;    /* 诊断：这条起过几次皮（>1 = ECL 中途换过动画）*/
    int32_t  seen_tick;     /* 最后一次在敌人表里见到的 tick（mark & sweep）*/
    int32_t  born_tick;     /* 起皮时的 tick（探针的节流基准）*/
    float    native_sc_x, native_sc_y;   /* ⛔ 同上：旧做法抄 scale 用的残留字段 */
    float    x, y;          /* 最后一次见到的坐标（诊断 + 镜像判据的组锚点）*/
} faith_gf_node_t;

static faith_gf_node_t s_gf_node[FAITH_GF_NODE_MAX];
static int32_t s_gf_tick_no;             /* 跟皮的 tick 计数（与关卡无关，单调）*/
static int32_t s_gf_cur_wave = -2;       /* 当前组的波号（−2 = 没开组；−1 = 读不到波号）*/
static int32_t s_gf_grp_id;              /* 组锚点（宿主 #1 的 id）*/
static int32_t s_gf_grp_n, s_gf_grp_s;   /* 组锚点的 (N,S)：镜像伙伴必须同一支变体 */
static float   s_gf_grp_x, s_gf_grp_y;
static int32_t s_gf_grp_last;            /* 组里最后一只到达的 tick（静默超时用）*/
static int32_t s_gf_grp_mirror;          /* 已经选到镜像伙伴了 */
static int32_t s_gf_grp_count;           /* 本组见过的候选数（诊断）*/

/* 关卡脚本那只敌人（引擎建的：`parent_enemy_id == 0` 且子程序名以 `Main` 开头）。指针不跨帧缓存。*/
static int32_t s_gf_stage_id;

static const char *faith_gf_sub_name(const uint8_t *e, int32_t *out_sub)
{
    uint8_t *ctx, *fm, **subs;
    int32_t sub;
    if (out_sub) *out_sub = -1;
    if (!e) return 0;
    ctx = *(uint8_t **)(e + FAITH_GF_ENEMY_ECL_CTX);
    fm  = *(uint8_t **)(e + FAITH_GF_ENEMY_ECL_FM);
    if (!ctx || !fm) return 0;
    sub = *(int32_t *)(ctx + FAITH_GF_ECL_CTX_SUB);
    if (sub < 0 || sub >= *(int32_t *)(fm + FAITH_GF_ECL_FM_SUB_COUNT)) return 0;
    subs = *(uint8_t ***)(fm + FAITH_GF_ECL_FM_SUB_TABLE);
    if (!subs) return 0;
    if (out_sub) *out_sub = sub;
    return (const char *)subs[sub * 2];
}

static int32_t faith_gf_parse_mainsub(const char *n)
{
    int32_t v = 0;
    int i;
    if (!n || strncmp(n, "MainSub", 7) != 0) return -1;
    n += 7;
    if (*n < '0' || *n > '9') return -1;
    for (i = 0; i < 6 && n[i] >= '0' && n[i] <= '9'; ++i) {
        v = v * 10 + (n[i] - '0');
        if (v > 99999) { v = 99999; break; }
    }
    return v;
}

/* 本帧的 ECL 波号（`MainSubNN` 的数字；读不到 = −1 ⇒ 上层退回「到达间隔」启发式）。*/
static int32_t faith_gf_wave_now(uint8_t *em)
{
    uint8_t *e;
    const char *name;
    if (!em) return -1;
    if (s_gf_stage_id) {
        e = ce_enemy_by_id(em, s_gf_stage_id);
        if (e) name = faith_gf_sub_name(e, 0);
        else { s_gf_stage_id = 0; name = 0; }
        if (name) return faith_gf_parse_mainsub(name);
    }
    CE_ENEMY_FOREACH(em, e) {
        if (!e) continue;
        if (*(const int32_t *)(e + FAITH_GF_ENEMY_PARENT_ID) != 0) continue;
        name = faith_gf_sub_name(e, 0);
        if (name && (strncmp(name, "Main", 4) == 0 || strcmp(name, "main") == 0)) {
            s_gf_stage_id = *(const int32_t *)(e + CE_ENEMY_ID);
            return faith_gf_parse_mainsub(name);
        }
    }
    return -1;
}

static faith_gf_node_t *faith_gf_node_find(int32_t id)
{
    int i;
    for (i = 0; i < FAITH_GF_NODE_MAX; ++i)
        if (s_gf_node[i].id == id) return &s_gf_node[i];
    return 0;
}

/* 收掉一条跟皮：删我们的 VM + 把原生 VM 的 `color_1` 还回去（`restore` = 0 时只删 VM，
 * 因为那只敌人已经不在了）。幂等。*/
/* 把敌人自己的主 anm 换到 `green_script`（一手 `anmSetMain(0,S)` 的 K==0 复刻）；定义在下面。*/
static int faith_gf_repoint(faith_gf_node_t *nd, uint8_t *e, int32_t green_script);

static void faith_gf_node_release(faith_gf_node_t *nd, int restore)
{
    /* ★ 2026-09-25：新做法下**我们自己没有 VM**（那支 VM 本来就是敌人的）⇒ 撤离 = 把敌人的主 anm
     *   **换回它原来的蓝脚本**（`restore` = 1 且那只还活着时）。不还回去的话，那只妖精会以绿的样子
     *   留在场上（删卡/换关后「改过的妖精没变回来」）。*/
    if (!nd || !nd->id) return;
    if (restore && nd->green_script >= 0) {
        uint8_t *e = ce_enemy_by_id((uint8_t *)CE_ENEMY_MGR(), nd->id);
        if (e) (void)faith_gf_repoint(nd, e, nd->blue_script);
    }
    memset(nd, 0, sizeof *nd);
}

/* 把敌人**自己的**主 anm 换到 `green_script`（= 一手 `anmSetMain(0, S)` 的 K==0 复刻，逐条见下）。
 * 返回 1 = 换上了。这是**唯一**改外观的地方（没有第二支 VM、没有隐原生贴图）。*/
static int faith_gf_repoint(faith_gf_node_t *nd, uint8_t *e, int32_t green_script)
{
    uint8_t *em_mgr = (uint8_t *)CE_ENEMY_MGR();
    uint8_t *enemy_anm = em_mgr ? *(uint8_t **)(em_mgr + CE_EM_ANM_ENEMY) : 0;   /* N=2 那一份 */
    uint8_t *data = e + CE_ENEMY_DATA;
    int32_t layer = *(const int32_t *)(data + FAITH_GF_ANM_LAYERS) + 7;          /* 引擎自己的 layer 表达式 */
    uint32_t *slot = (uint32_t *)(data + FAITH_GF_ANM_VM0);
    uint32_t old = *slot;
    uint32_t nv;

    float keep_pos[3]   = {0.0f, 0.0f, 0.0f};
    float keep_pos2[3]  = {0.0f, 0.0f, 0.0f};
    float keep_scale[2] = {1.0f, 1.0f};
    int   have_keep = 0;

    if (!enemy_anm || !nd) return 0;
    if (layer < 1 || layer > 30) layer = 8;      /* 兜底 = 一手妖精脚本自己的 `layer(8)`（112 同值）*/
    /* ★★★ 2026-09-25：**换 VM 之前先把旧 VM 的"姿态"记下来** ——
     *   一手（`local/vendor/thtk` 反编译 st01.ecl）：同一只妖精的左/右版本是**两个不同的生成变体**
     *   （`enmCreateM("GirlBlueA01", +184, …)` vs `enmCreate("GirlBlueA01", −184, …)`），
     *   而妖精的行为子程序 `GirlA01` 里**一个 anm 指令都没有** ⇒ 左右朝向**不可能**来自 ECL 的换脚本，
     *   只能是**生成时/引擎侧写在 VM 上**的东西（镜像 = `scale.x = −1` 那一类），
     *   再加上引擎每帧维护的坐标/`__pos_2`。⇒ 我们替换 VM 时**必须把这些原样继承过去**，
     *   否则新的那支 VM 就是「没镜像、坐标还在原点」的样子（用户看到的「不转向 / 半截在边缘外」正是它）。*/
    if (old) {
        uint8_t *ovm = ce_anm_get_vm(old);
        if (ovm) {
            memcpy(keep_pos,   ovm + CE_ANM_VM_POS,   12);
            memcpy(keep_pos2,  ovm + CE_ANM_VM_POS_2, 12);
            memcpy(keep_scale, ovm + CE_ANM_VM_SCALE,  8);
            have_keep = 1;
            ce_anm_delete(old);                  /* 引擎 `anmSetSprite`/`anmSetMain` 同款：先 kill 旧 VM */
        }
    }
    *slot = 0;
    nv = ce_anm_spawn(enemy_anm, (int)green_script, (int)layer);
    if (!nv) {
        ce_log("faithgf/skin: 换绿失败（enemy.anm 脚本 %d，layer %d）—— VM 池满？", (int)green_script, (int)layer);
        return 0;
    }
    *slot = nv;
    if (have_keep) {                             /* 继承旧 VM 的姿态（镜像/坐标）—— 见上面那段 */
        uint8_t *nvm = ce_anm_get_vm(nv);
        if (nvm) {
            memcpy(nvm + CE_ANM_VM_POS,   keep_pos,   12);
            memcpy(nvm + CE_ANM_VM_POS_2, keep_pos2,  12);
            memcpy(nvm + CE_ANM_VM_SCALE, keep_scale,  8);
        }
    }
    /* ★ 逐条复刻引擎 `anmSetMain` 的 K==0 收尾（一手 `th18:0x431640` 那一段）：
     *   `[+0x270] = S`（主脚本记忆，`anmReset`/`anmSelectedPlay` 回放用）、`[+0x26c] = S`、
     *   `[+0x274] = 0`、`[+0x268] = N`、`flags_lo |= 0x100000`。
     *   ⚠ `+0x4fd8/+0x4fdc`（final_sprite_size = vm 尺寸 × scale）**不写**：蓝/绿两支同形
     *     （`mof_green_pair_sane()` 在运行期逐条比过 12 个 sprite 的宽高）⇒ 值本来就一样。*/
    *(int32_t *)(data + FAITH_GF_ANM_MAIN)    = green_script;   /* +0x270 */
    *(int32_t *)(data + 0x26c)                = green_script;   /* +0x26c（= `CE_ENEMY_ANM_SCRIPT` 的相对值）*/
    *(int32_t *)(data + FAITH_GF_ANM_SUB)     = 0;              /* +0x274 */
    *(int32_t *)(data + CE_ENEMY_ANM_SRC)     = 2;              /* +0x268 = N */
    *(uint32_t *)(data + FAITH_GF_ANM_FLAGS) |= FAITH_GF_ANM_FLAG_MAIN;
    nd->green_script = green_script;
    nd->native_vm = nv;
    ++nd->skin_count;
    return 1;
}

/* 给这一只换绿（首换或 ECL 换了动画后的跟随）。`blue_script` = 被换掉的那一支蓝 pose。*/
static int faith_gf_skin_up(faith_gf_node_t *nd, uint8_t *e, int32_t green_script,
                            float x, float y)
{
    (void)x; (void)y;
    if (!faith_gf_repoint(nd, e, green_script)) return 0;
    /* ★★★ 2026-09-25（owner：「**绿妖精没有转向动画**」+ 一条关键线索：「它精灵表 12 帧，
     *   其中有 2/3 的帧分别是左右」）⇒ 这一版**换掉整套做法**：
     *
     *   **旧做法（拆掉）**：「隐掉原生贴图（`vm+0x524` alpha=0）+ 自己起一支 VM 播绿脚本 + 每帧跟随坐标」。
     *   它有三个天生的坑，实跑全撞上了：① 自己的 VM 出生在 (0,0)（世界原点）；② 原生 VM 上的
     *   **一切 VM 级效果都不继承** —— 镜像/翻转（左右朝向）、VM 上跑的 pose 切换，全丢；
     *   ③ 「跟随」要我们自己每帧写坐标，任何一次早退/重起都会让它定格。
     *
     *   **新做法**：**把敌人自己的主 anm 指到绿脚本**（`faith_gf_repoint()` = 一手 `anmSetMain(0, S)`
     *   的 K==0 逐条复刻）⇒ 从这一刻起**原生的那支 VM 自己就是绿妖精**：
     *     · 坐标/镜像/翻转/pose 切换/死亡动画**全部由引擎自己驱动**，我们一个字节都不用跟；
     *     · 精灵表 12 帧里的左右两套（`script1/2` 与 `3/4`：同一批 sprite 4..11，其中一支带
     *       `ins_308()` = **flipX**）本来就是**脚本级**的 ⇒ 引擎/ECL 一切换，切到的就是**绿的那一支**
     *       同名 pose（我们只把蓝 pose k 映射到绿 pose k，`mof_green_green_script()`）。
     *   ⚠ 「精灵表 12 帧 = 4 帧 idle（sprite0..3）+ 8 帧转向/飞行（sprite4..11）」这条一手事实
     *     直接来自 `thanm -l 18 enemy.anm`（script0 = sprite0..3；script1/2 = sprite4..11 正向、
     *     script3/4 = 同一批反向；script1/3 带 `ins_308()`）—— 与 owner 的描述逐条吻合。*/
    ce_fairy_claim_instance(110, nd->id);       /* 实例护栏（每帧重申；112 读得到）*/
    ce_log("faithgf/skin: #%d **改为绿妖精**（(N,S)=(2,%d) → 绿脚本 %d；第 %d 次换）"
           "（owner：一波改一只/一对，别和 112 改同一只）",
           (int)nd->id, (int)nd->blue_script, (int)green_script, (int)nd->skin_count);
    return 1;
}

/* 新到一只候选（本波第一只 / 它的镜像伙伴）⇒ 记进跟皮表并起皮。
 * 返回 1 = 这一只成了宿主。`wave` = 本帧波号（−1 = 读不到）。*/
static int faith_gf_host_take(int32_t id, int32_t row, int32_t n, int32_t s,
                              float x, float y, int32_t wave)
{
    faith_gf_node_t *nd = 0;
    uint8_t *e;
    int32_t green;
    int i;

    green = mof_green_green_script(&mof_green_pairs[row], s);
    if (green < 0) return 0;                     /* 不在这一行的源窗口里（不该发生：调用方已判过）*/
    e = ce_enemy_by_id((uint8_t *)CE_ENEMY_MGR(), id);
    if (!e) return 0;
    for (i = 0; i < FAITH_GF_NODE_MAX; ++i)      /* 空槽（或被回收的槽）*/
        if (!s_gf_node[i].id) { nd = &s_gf_node[i]; break; }
    if (!nd) {
        ce_log("faithgf/skin: 跟皮表满（%d 条）⇒ 这一波多出来的宿主不改（#%d）", (int)FAITH_GF_NODE_MAX, (int)id);
        return 0;
    }
    memset(nd, 0, sizeof *nd);
    nd->id = id;
    nd->row = row;
    nd->blue_script = s;
    nd->seen_tick = s_gf_tick_no;
    nd->born_tick = s_gf_tick_no;
    nd->x = x;
    nd->y = y;
    nd->native_vm = *(const uint32_t *)(e + CE_ENEMY_DATA + FAITH_GF_ANM_VM0);
    if (nd->native_vm) {
        uint8_t *vm = ce_anm_get_vm(nd->native_vm);
        nd->native_color = vm ? *(const uint32_t *)(vm + CE_ANM_VM_COLOR1) : 0;   /* 残留：不再写 */
    }
    if (!faith_gf_skin_up(nd, e, green, x, y)) { memset(nd, 0, sizeof *nd); return 0; }
    (void)n; (void)wave;
    return 1;
}

/* 组闭合（换波 / 静默超时）：只为诊断与状态复位 —— 宿主早就选好了。*/
static void faith_gf_group_close(const char *why)
{
    if (s_gf_cur_wave == -2) return;
    ce_log("faithgf/skin: 波关闭（%s）—— 波号 %d、候选 %d 只、宿主 #%d%s",
           why, (int)s_gf_cur_wave, (int)s_gf_grp_count, (int)s_gf_grp_id,
           s_gf_grp_mirror ? " + 镜像伙伴" : "");
    s_gf_cur_wave = -2;
    s_gf_grp_mirror = 0;
    s_gf_grp_count = 0;
}

/* 一只候选妖精到达：决定它是不是宿主（= 112 的 `ufo_carrier_arrival()` 的同一套规则）。
 * 返回 1 = 它是宿主（要起皮）。*/
static int faith_gf_arrival(int32_t id, int32_t n, int32_t s, float x, float y, int32_t wave)
{
    int new_group;

    if (s_gf_cur_wave == -2)                          new_group = 1;
    else if (wave >= 0 && s_gf_cur_wave >= 0)         new_group = (wave != s_gf_cur_wave);
    else                                              new_group = (s_gf_tick_no - s_gf_grp_last) > FAITH_GF_WAVE_GAP;
    ++s_gf_grp_count;
    s_gf_grp_last = s_gf_tick_no;
    if (new_group) {
        if (s_gf_cur_wave != -2) faith_gf_group_close("next wave");
        s_gf_cur_wave = wave;
        s_gf_grp_id = id;
        s_gf_grp_n = n;
        s_gf_grp_s = s;
        s_gf_grp_x = x;
        s_gf_grp_y = y;
        s_gf_grp_mirror = 0;
        s_gf_grp_count = 1;
        return 1;                                     /* ★ 组里第一只 = 宿主 #1 */
    }
    if (FAITH_GF_PER_WAVE >= 2 && !s_gf_grp_mirror && n == s_gf_grp_n && s == s_gf_grp_s
        && mof_green_mirror_ok(x, y, s_gf_grp_x, s_gf_grp_y)) {
        s_gf_grp_mirror = 1;
        return 1;                                     /* ★ 它的**镜像伙伴** = 宿主 #2 */
    }
    return 0;                                         /* 同波其余同类：原样、不掉绿点 */
}

/* 「这一只（id）是我们改绿的那只吗」—— 给 `mof.c` 的死亡回调判掉落用（**实例级**，
 * 取代原来那条按「组」判的 `mof_green_lookup()`）。*/
int faith_green_node_owns(int32_t enemy_id)
{
    return faith_gf_node_find(enemy_id) ? 1 : 0;
}

/* 撤离全部跟皮（删卡 / 换关 / 换局）：**把改过的妖精还回原色**，VM 一并删掉。
 * 删卡发生在商店里（帧钩子不跑）⇒ 这里必须当场做完，不能等 tick。*/
void faith_green_nodes_release_all(void)
{
    int i, n = 0;
    for (i = 0; i < FAITH_GF_NODE_MAX; ++i) {
        if (!s_gf_node[i].id) continue;
        faith_gf_node_release(&s_gf_node[i], 1);
        ++n;
    }
    s_gf_cur_wave = -2;
    s_gf_grp_mirror = 0;
    s_gf_grp_count = 0;
    if (n) ce_log("faithgf/skin: 撤离 %d 条跟皮（改过的妖精已还回原色）", n);
}

/* 每帧：① 波簿记；② 扫敌人选宿主/换皮/跟随；③ mark & sweep 收掉死掉的。*/
static void faith_gf_skin_tick(uint8_t *em)
{
    int32_t wave, stage = CE_CURRENT_STAGE();
    uint32_t armed = 0;                 /* 本关启用的行位掩码（同一个 `mof_green_plan()` 口径）*/
    uint8_t *e;
    int i;

    ++s_gf_tick_no;
    for (i = 0; i < MOF_GREEN_PAIR_MAX; ++i)
        if (mof_green_row_armed(&mof_green_pairs[i], stage)) armed |= (1u << i);
    if (!armed) {                        /* 本关一行都没启用 ⇒ 把上一条路改过的收干净 */
        faith_green_nodes_release_all();
        return;
    }
    wave = faith_gf_wave_now(em);
    /* 护栏位：对启用的每一行的 **5 个 pose 全量**重申（112 读的是「这只妖精的当前 (N,S)」，
     * 所以只报基址会漏掉 pose 1..4 ⇒ 他们可能仍然挑到同一只）。*/
    for (i = 0; i < MOF_GREEN_PAIR_MAX; ++i) {
        int k;
        if (!(armed & (1u << i))) continue;
        for (k = 0; k < MOF_GREEN_POSE_MAX; ++k)
            ce_fairy_claim_group(110, 2, mof_green_pairs[i].src_script + k);
    }
    /* 静默超时 ⇒ 关组（只为日志）。*/
    if (s_gf_cur_wave != -2) {
        uint32_t quiet = (s_gf_cur_wave >= 0) ? FAITH_GF_WAVE_SETTLE : FAITH_GF_WAVE_GAP;
        if ((uint32_t)(s_gf_tick_no - s_gf_grp_last) > quiet) faith_gf_group_close("quiet");
    }
    /* 敌人扫描：只认「杂鱼妖精 ∧ 落在启用的行里」。*/
    CE_ENEMY_FOREACH(em, e) {
        faith_gf_node_t *nd;
        int32_t id, n, s;
        float x, y;
        int32_t row = -1;
        if (!e) continue;
        n = *(const int32_t *)(e + CE_ENEMY_ANM_SRC);
        s = *(const int32_t *)(e + CE_ENEMY_ANM_SCRIPT);
        if (!mof_green_is_fairy(n, s)) continue;                  /* 中妖精/大妖精/阴阳玉一律不碰 */
        for (i = 0; i < MOF_GREEN_PAIR_MAX; ++i) {
            if (!(armed & (1u << i))) continue;
            if (mof_green_green_script(&mof_green_pairs[i], s) >= 0) { row = i; break; }
        }
        if (row < 0) continue;                                    /* 不是那一支蓝妖精（例如绿/红/橄榄）*/
        id = *(const int32_t *)(e + CE_ENEMY_ID);
        x = *(const float *)(e + CE_ENEMY_POS_X);
        y = *(const float *)(e + CE_ENEMY_POS_Y);
        nd = faith_gf_node_find(id);
        if (!nd) {
            if (faith_gf_arrival(id, n, s, x, y, wave))
                (void)faith_gf_host_take(id, row, n, s, x, y, wave);
            continue;
        }
        /* 已经在跟皮：① 脚本变了就跟着换绿（蓝 pose k → 绿 pose k）；② 探针。
         * ⚠ 新做法下**不需要**跟随坐标、也不需要隐原生贴图、更不需要抄 scale ——
         *   那支 VM 就是敌人自己的，引擎会把坐标/朝向/翻转/pose 全都驱动好。*/
        nd->seen_tick = s_gf_tick_no;
        nd->x = x; nd->y = y;
        nd->native_vm = *(const uint32_t *)(e + CE_ENEMY_DATA + FAITH_GF_ANM_VM0);
        if (s != nd->green_script) {          /* 引擎/ECL 改了主脚本（换 pose / 转向 / 死亡动画…）*/
            int32_t green = mof_green_green_script(&mof_green_pairs[nd->row], s);
            if (green >= 0) {                 /* 还是那一支蓝妖精的某个 pose ⇒ 跟到对应的绿 pose */
                nd->blue_script = s;
                (void)faith_gf_skin_up(nd, e, green, x, y);
            } else if (!mof_green_is_green_script(&mof_green_pairs[nd->row], s)) {
                ce_log("faithgf/skin: #%d 的主脚本变成了 (N,S)=(%d,%d)（既不是那一行的蓝 pose、也不是绿）"
                       "⇒ 不再改它（保留它自己的外观）", (int)id, (int)n, (int)s);
                faith_gf_node_release(nd, 0);
                continue;
            }
        }
        ce_fairy_claim_instance(110, id);                          /* 实例护栏每帧重申 */
        if (FAITH_GF_PROBE_EVERY > 0 && (s_gf_tick_no % FAITH_GF_PROBE_EVERY) == 0) {
            uint8_t *vm = nd->native_vm ? ce_anm_get_vm(nd->native_vm) : 0;
            const float *sp = vm ? (const float *)(vm + CE_ANM_VM_POS) : 0;
            const float *sc = vm ? (const float *)(vm + CE_ANM_VM_SCALE) : 0;
            ce_log("faithgf/skin/probe: #%d 敌(%.0f,%.0f) VM %s(%.0f,%.0f) scale(%.2f,%.2f) "
                   "主脚本 %d（我们的绿脚本 %d）换绿 %d 次",
                   (int)id, (double)x, (double)y, vm ? "" : "**没了**",
                   sp ? (double)sp[0] : -999.0, sp ? (double)sp[1] : -999.0,
                   sc ? (double)sc[0] : -999.0, sc ? (double)sc[1] : -999.0,
                   (int)s, (int)nd->green_script, (int)nd->skin_count);
        }
    }
    /* mark & sweep：连着几帧没见到 ⇒ 那只已经不在了（被打死/自删）⇒ 收掉这一条。*/
    for (i = 0; i < FAITH_GF_NODE_MAX; ++i) {
        faith_gf_node_t *nd = &s_gf_node[i];
        if (!nd->id) continue;
        if (s_gf_tick_no - nd->seen_tick > FAITH_GF_STALE_TICKS) {
            ce_log("faithgf/skin: #%d 不在了（改绿的那只被打死/自删；起皮 %d 次）⇒ 收掉跟皮",
                   (int)nd->id, (int)nd->skin_count);
            faith_gf_node_release(nd, 0);
        }
    }
}

/* 每帧幂等：按「当前关 ⇒ 该改哪些行」逐行 apply / restore（`mof_green_plan()` 是纯函数，单测钉住）。
 * ⚠ 只由 `on_tick_2` 调 ⇒ **卡不在场时根本不会跑**（用户「没有卡的时候不改」）。*/
/* ⛔ 旧路线（整组换 sprite 表项）—— 见上面那一节，**已不再被驱动**（owner 实跑否掉：整组换 ⇒ 全变绿）。
 *   机制本身一行没删（`mof_green_pair_*` 仍被单测钉住、随时可以切回来）⇒ 用 `unused` 标注压掉告警，
 *   而不是删掉它（删了就没人记得这条路存在过）。*/
static void faith_green_group_swap_tick(void) __attribute__((unused));
static void faith_green_group_swap_tick(void)
{
    uint8_t *tbl = faith_enemy_sprite_table();
    uint32_t want_apply = 0, want_restore = 0;
    int32_t stage = CE_CURRENT_STAGE();
    int i, applied_now = 0;

    if (!tbl) return;
    mof_green_plan(stage, s_fgreen_applied, &want_apply, &want_restore);

    for (i = 0; i < MOF_GREEN_PAIR_MAX; ++i) {
        const mof_green_pair_t *p = &mof_green_pairs[i];
        uint8_t *bak = s_fgreen_bak + (size_t)i * MOF_GREEN_PAIR_BYTES;
        uint32_t bit = 1u << i;

        if (want_restore & bit) {                       /* 换关 ⇒ 这一行不该在 */
            mof_green_pair_restore(tbl, p, bak);
            s_fgreen_applied &= ~bit;
            continue;
        }
        if (!(want_apply & bit)) {
            /* 已经改上的行：`anm` 若被重载（换关先还原再重载的另一条路）会把表项打回原色 ⇒ 自愈。
             * **不重取备份**（备份里是原色，此时表里也是原色；重取只会多一次无谓写）。*/
            if ((s_fgreen_applied & bit) && !mof_green_pair_is_green(tbl, p))
                mof_green_pair_apply(tbl, p);
            if (s_fgreen_applied & bit) ce_fairy_claim_group(110, 2, p->src_script);   /* 护栏位每帧重申 */
            continue;
        }
        /* 第一次接管这一行：先做运行期同形体检（源/绿两支的宽高逐条相等），不过就**不改**。*/
        if (!mof_green_pair_sane(tbl, p)) {
            if (!s_fgreen_warned) {
                s_fgreen_warned = 1;
                ce_log("faith: green pair %d REJECTED (sprite %d->%d 宽高不一致；常量与真表对不上，本行不改)",
                       i, (int)p->src_sprite, (int)p->dst_sprite);
            }
            continue;
        }
        mof_green_pair_swap(tbl, p, bak);
        s_fgreen_applied |= bit;
        /* ★★★ 2026-09-24 实例/组级护栏（owner 口径「**别同时操作同一只妖精的 VM，否则爆了**」，
         *   112 那条线确认他们的宿主池**含 `(2,0)`** = 我们换皮这一组）⇒ 我们每帧对自己**真的改上的**
         *   每一行重申一次「(N=2, S=src_script) 这组妖精归我」（`sdk.h` 的 `ce_fairy_claim_group()`）。
         *   112 那边读到就**当场放弃这只宿主**（不发皮、不掉令牌）；我们这侧是整组换皮，按组通报足够。*/
        ce_fairy_claim_group(110, 2, p->src_script);
        ++applied_now;
        if (!s_fgreen_logged)
            ce_log("faith: green fairy swap #%d '%s' (stage %d, sprite %d->%d, script %d->%d)",
                   i, p->who ? p->who : "?", (int)stage,
                   (int)p->src_sprite, (int)p->dst_sprite, (int)p->src_script, (int)p->dst_script);
    }
    if (applied_now) s_fgreen_logged = 1;
}

/* ★★★ 2026-09-25：**换皮驱动的入口**（名字保留 —— `mof.c` / `signal.c` 的调用点不动）。
 *   现在走的是**实例级**那一套（`faith_gf_skin_tick()`）：按波挑 1~2 只改成绿妖精，其余原样。*/
void faith_green_swap_tick(void)
{
    uint8_t *em = (uint8_t *)CE_ENEMY_MGR();
    /* ★★ 帧守卫：本函数由 `faith_pool_tick()` 调，而**两张卡都会调那张池子**
     *   （110/111 同装 ⇒ 同一帧两次）⇒ 这里必须自己防重，否则波簿记一帧走两步
     *   （`s_gf_tick_no` 双倍 ⇒ 「到达间隔」启发式的阈值等效减半、诊断计数翻倍）。
     *   与 `faith_pool_tick()` 里那道守卫同一个口径（帧号取引擎的当前帧计数）。*/
    {
        static uint32_t s_last;
        static int      s_have;
        uint32_t now = ce_sdk_frame();
        if (s_have && now == s_last) return;
        s_last = now;
        s_have = 1;
    }
    if (!em) return;
    faith_gf_skin_tick(em);
}

/* 还原全部（删卡 / 换局 / 换关清场）。`mof_clear()` 调它 —— 删卡发生在商店里，那一刻只有
 * `on_delete`（虚表 `+0x50`）跑得到（`METHOD.md` 铁律二：长持续效果必须交代「被删掉会怎样」）。*/
void faith_green_restore_all(void)
{
    uint8_t *tbl = faith_enemy_sprite_table();
    int i, n = 0;

    if (!tbl) { s_fgreen_applied = 0; return; }
    for (i = 0; i < MOF_GREEN_PAIR_MAX; ++i) {
        if (!(s_fgreen_applied & (1u << i))) continue;
        mof_green_pair_restore(tbl, &mof_green_pairs[i],
                               s_fgreen_bak + (size_t)i * MOF_GREEN_PAIR_BYTES);
        ++n;
    }
    if (n) ce_log("faith: green fairy restore (%d 行，sprite 表已还原)", n);
    s_fgreen_applied = 0;
    faith_green_nodes_release_all();   /* ★ 2026-09-25：实例级的跟皮也一并撤（还回原色 + 删我们的 VM）*/
    ce_fairy_claim_clear(110);   /* ★ 护栏位一并撤（删卡/换局）*/
}

/* ★★★ 2026-09-24 owner 口径：「**你得把改妖精抽离出来，是 110 或 111 只要装一张就改**」
 *   ⇒ 换皮驱动从 `mof.c` 搬到这里（共享件），由 `faith_pool_tick()` 每帧驱动一次 ——
 *     那张池子 110/111 **都会调**，所以「装任意一张就生效」。`mof.c` 只留两个只读查询。*/
uint32_t faith_green_applied(void) { return s_fgreen_applied; }

/* 某一行**此刻真的改上了**且表里仍是绿（给 110 的「变绿 ⇒ 掉绿点」判据用）。*/
int faith_green_row_green(int i)
{
    uint8_t *tbl = faith_enemy_sprite_table();
    if (!tbl || i < 0 || i >= MOF_GREEN_PAIR_MAX) return 0;
    if (!(s_fgreen_applied & (1u << i))) return 0;
    return mof_green_pair_is_green(tbl, &mof_green_pairs[i]);
}

/* ★★★ 2026-09-26：112 的路 —— 满 P 时「一颗 P 被转换」的产出（见 faith.h 的声明注）。
 *   逐条照抄 `faith_convert_full_p_items()` 里那段产出循环（含「按外观分组给分角」），
 *   只把位置换成调用者给的 (x, y)、并且**不碰原生道具池**。*/
int faith_payout_full_p_conversion(float x, float y)
{
    uint32_t seen_spr[FAITH_MAX_USERS], seen_scr[FAITH_MAX_USERS];
    int n_seen = 0, u, n = 0;
    for (u = 0; u < s_n_user; ++u) {
        int g = -1, v;
        float ang;
        if (s_user[u].conv_value <= 0) continue;
        for (v = 0; v < n_seen; ++v) {
            if (seen_spr[v] == s_user[u].art_sprite && seen_scr[v] == s_user[u].art_script) {
                g = v; break;
            }
        }
        if (g < 0) { g = n_seen; seen_spr[n_seen] = s_user[u].art_sprite;
                     seen_scr[n_seen] = s_user[u].art_script; ++n_seen; }
        ang = FAITH_THROW_ANGLE;
        if (g > 0) {
            const float k = (float)((g + 1) / 2);
            ang += ((g & 1) ? k : -k) * FAITH_CONV_SPREAD_STEP;
        }
        if (faith_spawn_angled(x, y, faith_core_tier_for_source(FAITH_SRC_FULL_P),
                               s_user[u].slot_mask, s_user[u].conv_value, ang)) ++n;
        else ++s_stat_conv_dropped;
    }
    return n;
}
