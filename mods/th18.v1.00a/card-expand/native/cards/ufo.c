/* 星莲船 / UFO（id 112）—— 令牌 → 召唤 → 吸收 → 击破开奖（th12 系）。
 *
 * 设计唯一真源：`docs/superpowers/specs/2026-09-12-ufo-112-design.md`（下文 `spec §n`）。
 * 一手依据：`docs/superpowers/plans/2026-09-10-ufo-card-feasibility.md`（下文 `PLAN`；召唤链 / spawn 表 / 磁吸模型）、
 *           `engine/ufo/th12/01-ufo-system.md`（下文 `UFO:行号`；TH12 原版全链）、
 *           `docs/superpowers/specs/2026-09-12-shared-enemy-death-event-design.md`（下文 `DEATH`）、
 *           `engine/card/th18/05-shop-and-money.md`（道具 type 语义）。
 *
 * **一句话**（spec §0）：集齐 **3 个同色**（或 **3 个互不相同**）令牌 ⇒ 召唤一个 ECL 真敌机 UFO ⇒
 * 它在上方框里飘、把全场 P / 金钱道具吸走 ⇒ 击破按吸收量开奖；超时飞走无奖。
 *
 * 纯逻辑（凑组状态机 / 奖励表 / 填充率 / 伪磁场几何）在 `ufo_core.{h,c}`，主机单测
 * `tests/test_ufo_core.c`；本文件只做「碰引擎的那部分」。
 *
 * ★★ 占位清单（需要 ECL / 资产 / 用户决策的，一律**不假装做完**）见文件尾 —— 取值一律是「不做」这个语义。
 *
 * ============================ ★ 用户 2026-09-12 的三条裁决（本次实装）============================
 * ① **金 UFO == 原本的蓝 UFO**（素材到时候 P 图替换）⇒ 颜色枚举加「金」（`UFO_COLOR_GOLD` = 4，
 *    ❓ 自定编号），**行为逐项照蓝**：吸收链 / 吸满不吐 / 击破无额外 / HP / 飘行参数全走同一段代码；
 *    贴图先借**蓝的那一套**（`ufo_color_art_index(GOLD) == 1` = 一手名字表 `th12:0x4b33e0` 的
 *    `UFO_Blue` 那一格；卡侧具名表 `UFO_ECL_BODY_BLUE` 上写了「金也用这一件，等 P 图」）。
 *    ⚠ 「金在凑组状态机里算不算独立一色」一手没有 ⇒ **没编**：`UFO_GOLD_COUNTS_AS_BLUE`（❓ 自定，
 *    默认 1 = 金 ≡ 蓝），改一个宏可翻面，两种取法都进了单测；⏳ 列在报告里待拍。
 * ② **「两同一异」= 一手会删掉第一个 token** ⇒ `ufo_group_put` 改成**照抄一手 KEEP2**
 *    （`th12:0x422e80` 的 `cnt1c = 4` 那条路）：第三格填上时不成组就**当场丢最旧、保留最近两个**，
 *    丢完**重判合成**。一手指令原文与 6 种排列的期望见 `ufo_group_put` 的注释 + 单测。
 * ③ **五边形框**（手上 2 枚、再来一枚即成组时框住场上「这类令牌」）⇒ ★ **2026-09-12（A7）已实装**：
 *    框 = **原版 TH12 `bullet.anm` script211 的逐条移植**（`assets/ability/scripts/223_ufo_pentagon.anm.txt`，
 *    `drawPolyBorder(1.0f, 5)` = 程序画的五边形、**无贴图**），触发条件 / 颜色标签 / 层号 / 生命周期
 *    全部一手（见下面那个「★★ 五边形框」常量块）；卡侧 `ufo_pentagon_hint_update()` 每帧驱动，
 *    **不用 `drawRect` 硬凑假的五边形**（用户明确要求）。
 * ④ **左下角飞碟槽 HUD** ⇒ ★ **2026-09-18 第三版：原版切图的三格令牌槽已接线**
 *    （`ability.anm` 脚本 302/303 ← TH12 `front.anm` script80/81 逐条移植；块认领改**自带锚点**
 *    钉在原版槽的位置 = 表面 (104, 952)，UI 层 25 ⇒ 会被暂停模糊罩盖住）。
 *    第一版那套 `drawRect` 占位（3 格 + 本体血条 + 一行读数）**整段删掉**；UFO 的"血量/进度"仍然由
 *    **实体身上**那两件表达（吸收环 228 + 血条 231，见 `AUDIT.md` §BO-12）。详见 `AUDIT.md` §DS。
 *
 * ★ 本次还复核了一条 **TH12 一手**（裁决 ② 的依据，不是 TH18）：
 *   `cd local/th12.v1.00b && objdump -d --start-address=0x422e80 --stop-address=0x422f20 -M intel th12.exe`
 *   ⇒ 令牌槽状态机 `th12:0x422e80` 的全部分支（见 `ufo_core.c` 的 `ufo_group_put` / `ufo_group_drop_oldest`
 *   逐指令对照）。★ 2026-09-12 **订正**：KEEP2 的条件有**三条**（`0x422ed4`/`0x422ed6` 的
 *   `slots[0]==color && slots[1]!=color` 也算一条，此前漏抄）⇒ `{A,B,A}`（首尾同色）**同样走 KEEP2**，
 *   不走 `0x422eeb or eax,-1` 的彩虹支 ⇒ **我们的实现与字面一手完全一致，没有差异**。
 *   （此前文件头这段与 `AUDIT.md` §BF4 都写着「有意偏离一格」—— 那条结论作废。）
 */

/* ============================ 本次实装新增的一手（objdump th18.exe，2026-09-12）============================
 * spec / PLAN 里标 🟡 的几处，本实装用 `objdump -d th18.exe` 逐条复核过（下面每条都给了指令原文地址）。
 * 复核手段可复现：`cd local/th18.v1.00a && objdump -d --start-address=0x… --stop-address=0x… -M intel th18.exe`
 * （PE 节映射：`.text` VA 0x1000 / raw 0x400 ⇒ 文件偏移 = VA − 0x400000 − 0x1000 + 0x400）。
 *
 * ① **道具 pos / vel 的偏移**（spec §3.4 引 `REVIEW` 而 `engine.h` 没收）
 *    `0x445bf5 movss xmm0,[edi+0xc38]` / `0x445bfd [edi+0xc3c]` / `0x445c05 [edi+0xc40]`
 *    ⇒ vel = float3 @ `+0xc38/+0xc3c/+0xc40`；同段 `0x445c25 addss xmm0,[edi+0xc2c]` 等三处
 *    ⇒ pos = float3 @ `+0xc2c/+0xc30/+0xc34`（与 `engine.h:428` 的 `CE_ITEM_POS` 一致）。
 *    位移式（±0x445bf5–0x445c55 段）：
 *        pos.x += vel.x × GAME_SPEED(`0x4ccbf0`) × slowdown(`mgr+0xe6bb14`)
 *        pos.y += vel.y × GAME_SPEED × slowdown       ← vel.y 另在 0x445c6d 段 += GAME_SPEED × 0.03
 *    ⇒ **清零 vel 之后每帧只剩 vy 的 +0.03 重力**（`PLAN` §2.4 的「可忽略」得到确认）。
 *
 * ② **出界即删的判定（围栏①）**：`0x445e38` 是删除点（`mov [edi+0xc74],0` + `push [edi+0xc28]` + `call 0x488cf0`
 *    + freelist 摘挂，与 `0x44658a` 的收集清尾**逐指令同构**）。跳它的两个条件：
 *        `0x445e1b comiss xmm0,[edi+0xc30]; ja 0x445e38`   ⇒ **y > 472.0f**（`0x4b93a4` = 472.0f，一手读出）
 *        `0x445e28 andps xmm0,[abs mask]; comiss xmm0,0x4b9340; jb 0x44620a`
 *                                                          ⇒ **|x| >= 200.0f**（`0x4b9340` = 200.0f，一手读出）
 *
 * ③ **自机收点半径**：`0x44624a movss xmm0,[esi+0x4798c]`（此时 esi = `PLAYER_PTR` `0x4cf410`）+
 *    `0x446252 mulss xmm0,xmm0` + `0x446256 comiss` ⇒ 半径 = `player+0x4798c`（`PLAN` §2.1：原生默认 30.0f）。
 *
 * ④ **道具清尾（吸收用）**：两条引擎路径（收集清尾 `0x446595` 起、出界删除 `0x445e38` 起）**逐指令同构**：
 *    `state = 0` → `push [item+0xc28]` + `call 0x488cf0`（删 anm VM）→ `[item+0xc28] = 0` →
 *    **freelist LRU 摘挂**（`0x4465b4`–`0x4465d2`，见 `ufo_item_relink`）。
 *    ⚠ 那个「摘挂」是 **normal_items freelist 的 LRU 移动，不是「把槽还回空闲表」**（槽仍在链表上，
 *      由下一个排到队头的槽复用）⇒ 「槽泄漏」的正确读法：**被清掉的槽要等整条池轮一遍才回来**，
 *      不是永久丢失。卡侧**照抄了这四步**（含 LRU 摘挂，带 NULL 守卫，见 `ufo_item_relink`）。
 *      这是「槽泄漏」记账口径的**一手依据**，见文件尾 §槽泄漏。
 *
 * ⑤ **type 语义（奖励表映射用）**：`0x446830` 的跳表（20 项，type 1..0x13）
 *    type 2 → `0x4463ba`、type 4 → `0x4463c6`、type 6 → `0x446465`；`0x4463c6` 体内
 *    `mov ecx,0x4cccdc; call 0x457570` ⇒ **type 4 = 命碎累加**（`0x457570` = `engine/card/th18/06-resource-economy.md`
 *    §93 一手闭合的那条）。⇒ spec §4 的候选映射「红 → 命碎片」在 TH18 侧落在 **type 4**。 */

#include <stdio.h>      /* snprintf —— HUD 读数那一行（字库只认 0-9 . % + - 与空格）*/
#include <string.h>

#include "sdk.h"
#include "ufo_core.h"
/* ★★ 2026-09-18（用户口径，`NEXT.md` §1x.9）：本卡要吸收 **110/111 的（大/中）信仰点** ——
 * 那是 `faith.c` 的**自建 VM 池**（TH18 没有信仰点这个 item type），走声明式通道
 * （`faith_absorb_list/take/move`，语义见 `faith.h` 那一段）。**只 include 头文件，不碰它的内部**。 */
#include "faith.h"
/* ★★ 2026-09-24（owner：「**得改谁能飞碟掉落，并且还得给掉的那个改帖图**」）：令牌**宿主表** ——
 * 生成物，由 `native/tools/gen_ufo_carrier_table.py` 从 TH18 的 7 关 ECL + enemy.anm 现扫出来
 * （键 = 敌人「长什么样」的 `(N,S)`，与 110 的大信仰点表同一套判据）。表的内容、一手出处与"换哪张皮"
 * 的分档理由全在 `cards/ufo_carrier_table.h` 与 `TUNING.md` §2.0zzm。 */
#include "ufo_carrier_table.h"

/* ============================ 裸引擎地址 / 偏移（本卡不改 sdk.h / engine.h，缺的写这里）============================
 * ⚠ 地址 = TH18 v1.00a。已有 SDK 宏的**不重复造**（`CE_ITEM_*` / `CE_SPAWN_*` / `CE_ENEMY_*` / `ce_spawn_enemy`）。*/

/* 道具 vel（float3）—— `engine.h` 没收，出处见文件头 ①。 */
#define UFO_ITEM_VEL        0xc38
#define UFO_ITEM_ANM_ID     0xc28     /* `item+0xc28` = 这颗道具的 anm VM id（清尾要删它；出处同 ④） */
/* 敌人**本体血量当前值**：`zEnemyData.life.current` = `enemy + 0x122c (CE_ENEMY_DATA) + 0x4ff4` = `+0x6220`
 * （一手交叉：`animal.c` 的 `ANIMAL_ENEMY_LIFE_CUR` 与 `lolk.c` 的 `CE_ENEMY_LIFE_CUR` 都写 0x6220，
 *  且 `engine.h` 的 `CE_ENEMY_DATA` 注里 0x6220 就是 `data.life`）⇒ HUD 血条第一版读它。 */
#define UFO_ENEMY_LIFE_CUR  0x6220
/* ★★ 2026-09-21（P2 #6「太脆」）：`data.life` 的**三格**都在这三行 —— 一手（objdump th18.exe）
 *   `EnemyManager__allocate_new_enemy` `0x42d815`–`0x42d82a`：把 `spawn+0x14` 同一个数**写进
 *   `+0x6220 / +0x6224 / +0x6228` 三格**（`data.life{current,…}`，基址 = `CE_ENEMY_DATA(0x122c)+0x4ff4`）。
 *   ⇒ 我们要在 spawn 之后改血量，就必须**照这三格一起写**（只写一格是与引擎的 spawn 形状不一致）。 */
/* ⚠ 命名刻意**不写 `MAX`/`SNAP`**：这两格的**语义没钉死**（引擎自己在 `0x42f371` 会把 `+0x6228`
 *   当"某个时刻的快照"重写一次；`engine.h:569` 又说 `+0x6230` 是 `life.max`）——
 *   我们**唯一**确定的是「spawn 把 `spawn+0x14` 同时写进这三格」（`0x42d815`–`0x42d82a`）
 *   ⇒ 回写真血量时按**同一个形状**写这三格即可，不去替它们起名字。 */
#define UFO_ENEMY_LIFE_2    0x6224   /* 一手：spawn 同写法（`0x42d81e`/`0x42d821`）*/
#define UFO_ENEMY_LIFE_3    0x6228   /* 一手：spawn 同写法（`0x42d827`/`0x42d82a`）*/
/* 为什么能这么写（本次一手复核）：`spawn+0x14 >= 1000` 的那条黑名单闸
 *   `0x42d8ed cmp [edx+0x14],0x3e8 / jl` + `0x42d8f6 or ecx,0x40000000` **只在 spawn 那一刻**跑；
 *   血量本身只是 `0x42d815` 起写的三个 int。⇒ 「先 spawn 一个 ≤999 的值，再回写真血量」=
 *   拿到一手的真血量，而位 30 不置（人偶/灵梦炸弹照样打得中）。扣血走的是 `0x42f314
 *   sub [ebx+0x4ff4],esi`（= `+0x6220`）⇒ 回写之后一切照常。 */
/* 敌人「静默撤退」位：置 `flags_lo` 位 25 ⇒ 同一帧的 EnemyManager sweep 直接析构它，
 * 不跑死亡处理 / 不掉落 / 不跑死亡 ECL。一手指令：`0x42d200 or [ecx+0x635c],0x2000000`（写）、
 * `0x42dfc5 test [ecx+0x635c],0x2000000` + `0x42dfcf jne 0x42dfe8` + `0x42dff2 call [eax+0x14]`（读/析构）。
 * ⚠ 不自己调析构（那是 UAF：我们可能正处在别人的链表遍历里，`DEATH` §2.5）。108 `youkai.c` 同款。
 * ★★ 2026-09-12 换池：这个位**现在只用于 UFO 本体**（它仍然是 ECL 真敌机）——
 *   令牌换成自建 VM 之后不再需要「静默撤退」（直接 `ce_anm_delete` 自己那只 VM 就行）。 */
#define UFO_FLAGS_DELETE_NOW 0x2000000u
/* ★ **已随真敌人壳一起删除**：`zEnemyData.abs_pos` 的偏移（`+0x12b4` / `+0x12b8`，float3）。
 *   它当年是为了「令牌是卡侧自己在动的**敌人**」才钉的（引擎每帧用 `abs_pos` 重算 `final_pos`，
 *   只写 `CE_ENEMY_POS_X/Y` 有「当帧被引擎 tick 覆盖」的风险）。**令牌不是敌人了** ⇒ 那套双写在
 *   `ufo_tokens_tick` 里整段消失，偏移也就没有用户了（`grep UFO_ENEMY_ABS_POS` = 0 命中）。 */

/* 音效（`engine/_shared/th18-sound-table.md` 一手表；用**具名宏**，不写裸数字）
 * ★★ 2026-09-20（P1「缺音效一批」）：**TH12 的 id → wav 表已逆出**（直读 `local/th12.v1.00b/th12.exe`：
 *   cfg 表 `th12:0x4ae5a4` 60 行 × 0x14（`+0x00` = wav 名表下标，与 TH13 同构）、名表 `th12:0x4aea50`、
 *   播放器 `th12:0x453d90`（居中）/ `th12:0x453e20`（带声像）。**按 wav 名对到 TH18**（两作同名的
 *   `se_*.wav` 逐字节相同，md5 已核）⇒ 下面每一格的 id 都有出处，不再是"借一个像的"。 */
#define UFO_SE_SPAWN    0x14    /* ✅ 一手：`Ufo::spawn`（`th12:0x44a9d9 mov edx,0x34`）→ TH12 `0x34` =
                                 * `se_lazer02.wav`（居中播放）；TH18 的同一枚 wav = **`0x14`**（md5 同）。*/
#define UFO_SE_ENTER    CE_VOICE_UFOALERT
                                /* ★★ 2026-09-20 补（P1 #th12-8「UFO 缺少入场音效」）：一手 = `Ufo::spawn`
                                 * 的**第二声** `th12:0x44a9fe mov eax,0x36` + `th12:0x44aa06 call 0x453e20`
                                 * ⇒ TH12 `0x36` = `se_ufoalert.wav`，**在飞碟自己的坐标**播
                                 * （`th12:0x44a9f7 fld [eax+0x1074]` = 敌机 x）。TH18 71 枚里没有这枚 wav
                                 * ⇒ 走音效表扩容通道（`CE_VOICE_UFOALERT` = `0x58`；源
                                 * `local/th12.v1.00b/dat/se_ufoalert.wav`，构建期逐字节现取，
                                 * 见 `assets/voice/derive_retail_se.py`）。
                                 * ⚠ 一手里它是**循环音**（th12 的 cfg 行 `+8` = 1；wav 只有 0.5 s）；
                                 *   本仓 SDK 没有"停循环音"的通路 ⇒ 出场那一帧放**一次**（不改循环位）。*/
#define UFO_SE_ABSORB   0x0c    /* ★★ 2026-09-23 **一手订正**（原来 0x0d = `se_powerup.wav` 是无出处的
                                 *   ❓自定占位）：**UFO 吞下一颗道具的那一刻，TH12 播的是 `0x26`**。
                                 *   一手 = state 6/7「飞向 UFO」通道的收尾 `th12:0x42696d`–`0x42698d`
                                 *   （逐指令读，`edi` = 道具、`esi` = `edi+0x96c` = 道具坐标）：
                                 *     `426958 fld [esp+0xc]` / `42695c fcomp [0x4a43e0]`（= 324.0 = 18²）
                                 *     `426964 test ah,5 ; jp 0x4260af`      ← 还没进 18 px 圈 ⇒ 继续飞
                                 *     `42696d fld [esi]` / `42696f push ecx`  ← 声像 = **道具自己的 x**
                                 *     `426970 mov eax,0x26`                   ← ★ se id
                                 *     `426978 mov [edi+0x9b0],0`              ← 道具 state = 0（被吃掉）
                                 *     `426982 call 0x453e20`                  ← play_sound(id, pan)
                                 *     `426987 mov eax,[edi+0x9b4]` / `42698d call 0x44aca0`  ← 才是入账
                                 *   TH12 `0x26` → **`se_timeout2.wav`**（我按 `th12:0x4ae5a4` 的 60 行表
                                 *   逐行读出：`+0x00` = 名表下标 29 → `se_timeout2.wav`；与 TH18 的同名 wav
                                 *   **逐字节相同**）⇒ TH18 的同一枚 = **`0x0c`**。
                                 *   ⚠ 于是它与 `UFO_SE_TOKEN` **同值** —— 一手里这两件事本来就是**同一个音**
                                 *   （令牌被收下走 `0x426da4` 的共同尾，也是 `0x26`），别以为是写错了。*/
#define UFO_SE_PACKAGE_OPEN 0x10
                                /* ★★ 2026-09-23 补（原来整条包被收下只播一个 ❓自定 0x0d）：**击破掉的
                                 *   「赏金压缩包」被自机收下时，一手里是两声**：
                                 *     ① `th12:0x426c76 mov eax,0x27` + `0x426c7e call 0x453e20`
                                 *        （`0x426c6f` 起的 type 0x10–0x12 支、声像 = `[edi+0x96c]` 道具 x）
                                 *        ⇒ TH12 `0x27` = **`se_invalid.wav`** ⇒ TH18 的同一枚 = **`0x10`**
                                 *        （我按两作的表逐行核过：TH18 `0x10` → `se_invalid.wav`）；
                                 *     ② 子分派算完分之后落回**共同尾** `0x426da4` ⇒ 再播一次 `0x26`
                                 *        = `se_timeout2.wav` ⇒ TH18 **`0x0c`**（= `UFO_SE_TOKEN`）。
                                 *   ⇒ 我们照抄两声：`UFO_SE_PACKAGE_OPEN` + `UFO_SE_TOKEN`。*/
#define UFO_SE_ITEM_BREAK 0x0d  /* ★ 2026-09-18 改：原占位借用 `0x11 = se_extend.wav` ⇒ 每掉一次赏金道具
                                 * 就放一次 **Extend 音**，用户实跑听成"一直在响的奇怪 Extend 音效"
                                 * （`CARD-118-12IN1-REPORT.md` B9）。改借 `0x0d = se_powerup.wav`（拾取感）。
                                 * ★ 2026-09-23：**压缩包被收下那一条已经不用它了**（见 `UFO_SE_PACKAGE_OPEN`）
                                 * —— 现在只剩两处：**整颗 Bomb 被收下**、**击破时加成道具 spawn**。
                                 * 前者一手 = 道具 7：`0x422dd0`（`collect_bomb` 体内 `0x422de9 mov edx,0x30`
                                 * = `se_bonus`）+ 共同尾 `0x426da4`（`0x26`）⇒ 我们保留引擎自己的
                                 * `ce_add_bomb()` 音 + 补 `UFO_SE_TOKEN`（0x0c）那一声（🟡 不再叠 `se_bonus`，
                                 * 免得同一个动作响三声 —— 记在 `TUNING.md`）。
                                 * 后者（`ufo_break` 里 spawn 加成道具时播）**一手 spawn_item 根本不播音** ⇒
                                 * 它是 ❓自定 的"这里该有声"补丁，**本轮不动**（删它要 owner 点头）。*/
#define UFO_SE_FULL     0x2e    /* ★★ 2026-09-23（owner 实跑第 7 条）：「**UFO 吸收满之后是有一个音效的**，
                                 *   好像是 **`se_cardget.wav`**」⇒ 用 TH18 自己的 `0x2e`（= `se_cardget.wav`，
                                 *   +0 dB / prio 100；同一枚 wav 在 `0x0f` 也有一行 = −8 dB，取响的那一行）。
                                 *   ⚠ **一手是 `se_bonus.wav`**：TH12 吸满那一支 `0x44ad7e` 的 id `0x30`
                                 *   → TH12 表第 0x30 行 = `se_bonus.wav`（vol 0 / prio 100），而 TH18 同名的
                                 *   `0x2f` = `se_bonus.wav` **逐字节相同**（md5 44b82b1d…）⇒ 原来那一格是**一手正确**的。
                                 *   ⇒ 本轮按 owner 口径改成 `se_cardget`，属**听感取舍**（一行可回退），登记 `TUNING.md`。*/
#define UFO_SE_UFO       CE_VOICE_UFO
                                /* ★★ 2026-09-23（owner 实跑第 8 条）：「UFO 召唤的时候…得有音效
                                 *   **`se_ufo.wav`**」⇒ 新扩表槽 `CE_VOICE_UFO`（`0x5f`，源 = TH12
                                 *   `se_ufo.wav` 逐字节现取，构建期由 `assets/voice/derive_retail_se.py` 取）。
                                 *   一手定位见该脚本的 TABLE 行：TH12 表第 **0x33** 行 ⇒ `se_ufo.wav`
                                 *   （vol −400 / prio 100 / loop 0）。
                                 *   ⚠ TH12 里这一枚**不是** `Ufo::spawn` 播的（那里只有 0x34 + 0x36）⇒ 本格 =
                                 *   **owner 指定的映射**，不是「一手召唤时就是这一声」。*/
#define UFO_SE_BREAK    0x06    /* ★★★ 2026-09-24（owner 报的正确音源）：「**击破 UFO 时候的音效应该是
                                 *   `se_enep02.wav`**」（并给了他那份 TH12 的 wav，本机 md5 `7d31b64f…`）。
                                 *   ⇒ 两张表都核过（本机直读）：
                                 *     · TH12 SE 表 `0x4ae5a4` 第 **0x32**(50) 行 = `se_enep02.wav`
                                 *       （名表 idx 39、vol −500 / prio 100）；
                                 *     · TH18 SE 表 `0x4c9b80` 第 6 行 = `se_enep02.wav`（名表 `0x4b47a0` idx 37、
                                 *       vol −500 / prio 100）⇒ **TH18 id = 0x06**。
                                 *   ⚠ 订正上一版：`0x44af65 lea eax,[edi+0x6]` 那一声（`edi` 在 `0x44af15 xor edi,edi`
                                 *     之后确实是 0）＝ TH12 **id 6 = `se_power1.wav`** —— 那是**击破代码里显式播的
                                 *     一声**（vol −700），不是 owner 耳朵里那声"碎"；**击杀那个真敌人本身**还会由引擎
                                 *     播它自己的敌人击破音 `se_enep02`（TH12 id 0x32）⇒ 一手实际是**两层**
                                 *     （显式 se_power1 + 引擎 se_enep02）。owner 要的是后者 ⇒ 我们直接播 0x06。*/
#define UFO_SE_TOKEN    0x0c    /* ★★ 2026-09-20 改（P1 #th12-3「收取 token 声音应该不是这个」）：
                                 *   **一手 = TH12 `0x26` = `se_timeout2.wav`** —— 令牌（道具 type 10..15）
                                 *   被收下的公共尾：`th12:0x426f7c` 跳表（索引 = type−1，`th12:0x42616a`–
                                 *   `0x426182`）把 type 10/13→`0x426c3f`、11/14→`0x426c4d`、12/15→`0x426c5e`，
                                 *   三条都是 `call th12:0x4270b0`（令牌入槽）后 `jmp th12:0x426da4`：
                                 *   `th12:0x426dab mov eax,0x26` + `th12:0x426dbd call 0x453e20`
                                 *   （声像 = `[edi+0x96c]` = 令牌自己的 x）。TH12 `0x26` → `se_timeout2.wav`
                                 *   （md5 与 TH18 的**相同**）⇒ TH18 的同一枚 = **`0x0c`**。
                                 *   ⚠ 原名 `0x39`（`se_pin00.wav`）只是"听着像钟声"的 ❓自定借用，已废。*/
#define UFO_SE_TOKEN_RECOLOR 0x4e
                                /* ★★ 2026-09-20 补（P1 #th12-2「缺少 token 变色声音」）：
                                 *   一手 = TH12 `0x35` = `se_changeitem.wav`，在**令牌自己的 x** 播
                                 *   （`th12:0x4264d7 lea eax,[ebx+0x35]` + `th12:0x4264dd call 0x453e20`，
                                 *   声像 `th12:0x4264d0 fld [edi+0x96c]`）—— 那一段只对**变色令牌**
                                 *   （type 13/14/15）跑，正是本卡的 `cycler` 令牌。TH12 `0x35` →
                                 *   `se_changeitem.wav`（md5 与 TH18 **相同**）⇒ TH18 的同一枚 = **`0x4e`**
                                 *   （与 117 `animal.c` 的 `ANIMAL_SE_CHANGE` 同一格，语义也同为"变色"）。*/

/* ==================== ★★ se 诊断：播 se 的**唯一出口**（2026-09-23，工单「收碟子音效不对」）========
 * 为什么包一层：owner 只能在实跑里**听**，而日志里此前**一个 se 都看不到** ⇒ 无法判定「响的是哪一枚」。
 * 日志策略（**刻意不刷屏**）：
 *   ① 每种事件**本关第一次**播时打一行 —— 事件名 + TH18 id + 声像 + 该行对应的一手站点；
 *   ② 关底 / 删卡时由 `ufo_se_tally()` 把「每种事件播了几次」汇总进收尾行。
 * ⇒ owner 实跑时只要 grep `ufo/se` 就能一眼看出那一刻响的是哪个 id。
 * ⚠ 计数在 `on_ctor`（每关开场）清零 ⇒ 一行汇总里的数字是**本关**的。
 * ⚠ 本函数只是 `ce_play_sound` 的薄包装（**不新增站点、不碰共享件**），行为与原调用完全一致。 */
#define UFO_SEV_TOKEN_TAKE  0    /* 令牌被自机收下并落槽（一手 `0x426da4` 共同尾 = `0x26`）*/
#define UFO_SEV_TOKEN_FULL  1    /* 槽里已有成组 ⇒ 令牌被吃掉换满槽补偿（同一条共同尾）*/
#define UFO_SEV_ABSORB      2    /* UFO 吞下一颗道具（一手 `0x426970` = `0x26`）*/
#define UFO_SEV_SPAWN       3    /* UFO 入场（一手 `0x44a9d9` = `0x34` + `0x44a9fe` = `0x36`）*/
#define UFO_SEV_RECOLOR     4    /* 彩虹令牌变色（一手 `0x4264d7` = `0x35`）*/
#define UFO_SEV_FULL        5    /* UFO 吸满吐奖励（一手 `0x44ad7e` = `0x30`）*/
#define UFO_SEV_BREAK       6    /* UFO 被击破（一手 `0x44af65` = `6+色`）*/
#define UFO_SEV_ETBREAK2    7    /* 击破的那一发大扩圈消弹音（一手同一条路，id 随色）*/
#define UFO_SEV_PACKAGE     8    /* 赏金压缩包被收下（一手 `0x426c76` = `0x27`）*/
#define UFO_SEV_BOMB_TAKE   9    /* 整颗 Bomb 被收下（一手 = 道具 7：`collect_bomb` + 共同尾）*/
#define UFO_SEV_DROP_SPAWN 10    /* ❓自定：击破时加成道具 spawn 那一声（一手 `spawn_item` 不播音）*/
#define UFO_SEV_TOKEN_SPAWN 11   /* ★ 2026-09-23（#18）：**令牌生成**那一声（一手 `0x42741b` = `0x37`）*/
#define UFO_SEV_ALERT_LOOP 12    /* ★ 2026-09-23（#11）：UFO 在场期间的**持续** `se_ufoalert`（循环音）*/
#define UFO_SEV_COUNT      13

static const char *const s_sev_name[UFO_SEV_COUNT] = {
    "token-take", "token-slot-full", "ufo-absorb", "ufo-spawn", "token-recolor",
    "ufo-fill-full", "ufo-break", "ufo-etbreak2", "package-open", "bomb-take", "drop-spawn",
    "token-spawn", "ufo-alert-loop"
};

/* ★★ 2026-09-23（owner 实跑第 4 条）：「**UFO（敌人）吸收道具不应该有音效**，我**收取飞碟 token 不应该有音效**」
 *   —— 这两件事一手确实各有一声（都是 TH12 `0x26` = `se_timeout2.wav`），但 owner 听感上明确要**静音**
 *   （同一天早些时候的口径也是「收 token 没声音，就当没声音处理」）。
 *   ⇒ 实现成**静音表**：事件照旧计数/照旧进日志（可对账），只是**不调 `ce_play_sound`**。
 *   ⚠ 要恢复一手：把下面几格改 0（一行）。
 *   ★ 2026-09-23 追加静音一格 `ufo-etbreak2`：一手那发扩圈**自己不播音**（它只是消弹；见 `UFO_SE_BREAK`
 *     的注）⇒ 原来在 `ufo-break` 与 `ufo-etbreak2` 各播一次会听成两声。登记见 `TUNING.md` §2.0y/§2.0z… 与 `SINGLE-CARD-TEST.md` #4。*/
static const uint8_t s_sev_mute[UFO_SEV_COUNT] = {
    /* token-take */ 1, /* token-slot-full */ 1, /* ufo-absorb */ 1,
    /* ufo-spawn */ 0, /* token-recolor */ 0, /* ufo-fill-full */ 0, /* ufo-break */ 0,
    /* ufo-etbreak2 */ 1, /* package-open */ 0, /* bomb-take */ 0, /* drop-spawn */ 0,
    /* token-spawn */ 0, /* ufo-alert-loop */ 0
};
static uint32_t s_sev_count[UFO_SEV_COUNT];
static uint8_t  s_sev_logged[UFO_SEV_COUNT];

/* 唯一的播放入口：计数 + 首播一行 + 转发 `ce_play_sound`（**静音事件只计数/记日志**）。 */
static void ufo_se(int ev, uint32_t id, float x, const char *one_hand)
{
    if (ev >= 0 && ev < UFO_SEV_COUNT) {
        ++s_sev_count[ev];
        if (!s_sev_logged[ev]) {
            s_sev_logged[ev] = 1;
            ce_log("ufo/se: \"%s\" -> th18 id 0x%02x (pan %.0f)%s - %s",
                   s_sev_name[ev], (unsigned)id, (double)x,
                   s_sev_mute[ev] ? " [MUTED by owner ruling - not played]" : "",
                   one_hand ? one_hand : "(no source recorded)");
        }
        if (s_sev_mute[ev]) return;                   /* ★ 静音：不播，但账照记 */
    }
    ce_play_sound(id, x);
}

/* 关底 / 删卡时的一行汇总（全 0 就不打）。 */
static void ufo_se_tally(const char *why)
{
    uint32_t n = 0;
    int i;
    for (i = 0; i < UFO_SEV_COUNT; ++i) n += s_sev_count[i];
    if (!n) return;
    ce_log("ufo/se tally (%s): token-take %u / token-slot-full %u / ufo-absorb %u / ufo-spawn %u / "
           "token-recolor %u / ufo-fill-full %u / ufo-break %u / ufo-etbreak2 %u / package-open %u / "
           "bomb-take %u / drop-spawn %u (本关累计；id 见上面各条 `ufo/se: \"...\" -> id` 行)",
           why, s_sev_count[UFO_SEV_TOKEN_TAKE], s_sev_count[UFO_SEV_TOKEN_FULL],
           s_sev_count[UFO_SEV_ABSORB], s_sev_count[UFO_SEV_SPAWN], s_sev_count[UFO_SEV_RECOLOR], s_sev_count[UFO_SEV_TOKEN_SPAWN],
           s_sev_count[UFO_SEV_FULL], s_sev_count[UFO_SEV_BREAK], s_sev_count[UFO_SEV_ETBREAK2],
           s_sev_count[UFO_SEV_PACKAGE], s_sev_count[UFO_SEV_BOMB_TAKE], s_sev_count[UFO_SEV_DROP_SPAWN]);
}

static void ufo_se_reset(void)
{
    int i;
    for (i = 0; i < UFO_SEV_COUNT; ++i) { s_sev_count[i] = 0; s_sev_logged[i] = 0; }
}

/* ---- ★ 占位：ECL 子程序名（spec §6 步 3，要资产管理线）----
 * `"NoEnemyDebug"` = 原版自带的惰性壳（`wait(1000)` 死循环 ⇒ 不跑 ECL、不动、不开火、不掉落、不自删），
 * 108 `youkai.c` 已在用同一招。真名待写：令牌 = spec §3.2 的第 ①②③ 三处细节也要先拍。
 *
 * ★★ 用户 2026-09-12 裁决 ① 落在这一段：**金 = 蓝的调色版** ⇒ 金那一趟**走蓝的整套贴图 / 子程序**。
 *    一手名字表只有 4 件（`th12:0x4b33e0` = `UFO_Red`/`UFO_Blue`/`UFO_Green`/`UFO_Rainbow`，
 *    索引 = 纯逻辑层 `ufo_color_art_index()`），**没有金** ⇒ 金借蓝那一格（`UFO_ECL_BODY_BLUE`，等 P 图）：
 *    资产线切图/调色之后，要么改 `ufo_color_art_index()` 给金单开一格，要么把下面这个名字换掉。
 *    ⚠ 5 个名字现在都是同一个惰性壳（没有 ECL 可用），但**选哪一件**这件事已经按色分好了 ——
 *    召唤日志里会打 `art idx`，实跑就能看出「金走的确实是蓝那一件」。
 *
 * ── ★★★ 2026-09-12 更新：**TH12 的真脚本已经移植好了**，等资产 ─────────────────────────
 * 真名 = 一手名字表 `th12:0x4b33e0`（4 项，全是本体的；`th12:0x44a909` 索引，`color-1`，`-1`→彩虹）：
 *
 *   ★★ 资产到位后要改的就是下面这 4 行（**只改值，不改名**）—— ★ 2026-09-12：**已经改过了**，
 *      现在这 4 行就是 `_TTL` 那一组真名（只对**本体**；令牌已换池，不走 ECL）：
 *   RED     "NoEnemyDebug" → "UFO_Red_TTL"
 *   BLUE    "NoEnemyDebug" → "UFO_Blue_TTL"       （金也走这一件，见上）
 *   GREEN   "NoEnemyDebug" → "UFO_Green_TTL"
 *   RAINBOW "NoEnemyDebug" → "UFO_Rainbow_TTL"
 *
 *   ⚠ 但**别急着改**：TH18 版的脚本文本已落在 `assets/ecl/ufo.ecl.txt`（9 个子程序：
 *     4 色原样移植 + 4 色自计时等价改写 + `UFO_EtBreak`），它必须先进 `default.ecl`
 *     并映射进 thcrap，名字才查得到（查不到 ⇒ `0x48b3a0` 干净退出 ⇒ 表现为「本体不出来」）。
 *     落地三件 —— ✅ ① `assets/ecl/` 的文本 → `native/build/ecl/default.ecl`（`make ecl-ufo`）；
 *     ✅ ② `release.py:33` 已把 `default.ecl` 映射进 thcrap（落 **patch-step3** = 常开那个）；
 *     ✅ ③ ★ 2026-09-12：贴图**不走 ECL 了** —— 已移植进 `ability.anm`（script 164..167），
 *        由本文件的**跟随 VM** 画（见下面「跟随 VM」注释块与 `UFO_ECL_HIDDEN_BODY_SCRIPT`）。
 *     ⏳ 在 ① 之前选 `_TTL` 后缀那一组（`UFO_Red_TTL` …），理由见 `ufo.ecl.txt` 文件头 ——
 *     原样那组靠引擎写 UFO 敌机的 `int_vars[0]`，而那个偏移**还没提炼进 `engine.h`**（spec §9 #10b）。
 *     ⚠ 现在这 4 个名字**真的会被 `ce_spawn_enemy` 查到**（default.ecl 已进 patch-step3）——
 *       若实跑仍打 `failed - placeholder ECL name only`，那就是名字没进包（查 release/dist 那一环）。
 *
 *   ⚠ **令牌不再走 ECL 了**（★ 2026-09-12 用户拍板换池）：令牌是**自建 VM 池**里的一只 VM，
 *     既没有子程序名也没有实体 ⇒ 下面这段只对**本体**有效（`UFO_ECL_BODY_*` 四件）。
 *   ⚠ 一手的令牌在 TH12 里是**道具**（type 10–15），行为全在 C 侧
 *     （`th12:0x4262f5` 每 150 帧换色 / 60 px 内暂停），ECL 名字表（`th12:0x4b33e0`）里**根本没有**
 *     令牌子程序 ⇒ 当年那个 `UFO_ECL_TOKEN = "NoEnemyDebug"` 的占位依赖**已整个删除**
 *     （见 `AUDIT.md` §BO-11 的 P3：真敌人壳的三条代价 + 用户裁决）。 */
#define UFO_ECL_BODY_RED     "UFO_Red_TTL"    /* ★ 真名 = `UFO_Red`（`th12:0x4b33e0[0]`）；**用 `_TTL` 那一组**：
                                               * 卡侧写不到 UFO 敌机的 `int_vars[0]`（A 组的离场靠引擎写它）⇒ 见 assets/ecl/ufo.ecl.txt */
#define UFO_ECL_BODY_BLUE    "UFO_Blue_TTL"   /* 真名 = `UFO_Blue`（[1]）；**金也用这一件**（金 = 蓝的调色版，等 P 图）*/
#define UFO_ECL_BODY_GREEN   "UFO_Green_TTL"  /* 真名 = `UFO_Green`（[2]）*/
#define UFO_ECL_BODY_RAINBOW "UFO_Rainbow_TTL"/* 真名 = `UFO_Rainbow`（[3]）*/

/* 颜色 → 本体 ECL 子程序名（**只有这一个出口**；索引由纯逻辑层 `ufo_color_art_index()` 给）。 */
static const char *ufo_ecl_body_name(int32_t color)
{
    switch (ufo_color_art_index(color)) {
    case 0:  return UFO_ECL_BODY_RED;
    case 1:  return UFO_ECL_BODY_BLUE;     /* ★ 蓝**与金**都落这一格 */
    case 2:  return UFO_ECL_BODY_GREEN;
    case 3:  return UFO_ECL_BODY_RAINBOW;
    default: return UFO_ECL_BODY_BLUE;     /* 非法色到不了（`ufo_group_take_ready` 只吐合法色）*/
    }
}

/* ============================ ★ 跟随 VM：本体与令牌的**画面**（2026-09-12 新增）============================
 * 背景：TH12 的四色飞碟动画（+ 六态令牌）**已经移植**进 `ability.anm`（脚本 164..167 / 168..173，
 * `make anm` 产出、号在 `native/anm_ids.h`），但 **ECL 画不出来** —— 见下面那个注释块。
 * 做法：ECL 侧只把 UFO 敌机刷成**隐形载体**（`assets/ecl/ufo.ecl.txt` 的 Δ2），画面由本文件
 * 起一个**跟随 VM**（world 列表、实体坐标每帧抄敌机的 `abs_pos`）来画。
 *
 * ★★ 2026-09-12（本轮，用户拍板「**token 得是自建 VM 池子**」）—— **令牌的那一半已经不是「跟随 VM」了**：
 *   令牌**不再是真敌人**（壳 / `UFO_TOKEN_LIFE` / 占位 ECL 名 `UFO_ECL_TOKEN` 全部删除），
 *   改成文件级静态池 `s_tokpool`（先例：117 `animal.c` 的 `s_tok[40]`、113 `trance.c` 的 `s_spirit[128]`）：
 *   **一只 token = 一个 `ce_anm_spawn` 出来的 VM**（脚本仍是 `CE_ANM_ABILITY_SCRIPT_UFO_TOKEN_*`、
 *   层仍是 `UFO_TOKEN_ANM_LAYER`），位置由**我们自己的坐标字段**（`ufo_tok_motion_t` 的 `x/y`）算出来、
 *   每帧 `ce_anm_set_pos` 写进去。
 *   ⚠ 所以令牌这里**没有**「贴图 VM + 跟随 VM」两份 —— 一只 token 就一个 VM，它既是画面也是本体。
 *   ⚠ **本体（UFO）不受影响**：它有血量 / 受击盒 / 要被子弹打死 ⇒ 仍然是 ECL 真敌机 + 跟随 VM
 *     （下面 `ufo_body_vm_*` 这一段**一个字没改**）。 */

/* ECL 把本体刷成隐形用的脚本号 —— **`-1` = `anmSetSprite` 的「拆槽」分支**：
 * `S < 0` ⇒ 先 `call 0x488cf0` 把该槽旧 VM 标删、槽写 0、**不建新 VM**（一手 `th18:0x436bf6`：
 * `test esi,esi; js → 收工`；opcode 303，独立函数 `th18:0x436bb0`）。
 * 原生真的这么用：**14 处 `anmSetSprite(K, -1)` 全在 boss 死亡 / 符卡收尾**
 * （`st02mbs:179 anmSetSprite(1,-1)`、`st07bs:2390 anmSetSprite(2,-1)` …）⇒ 原版「让这个槽不画东西」
 * 的**官方手法**。一手全文：`engine/anm/th18/04-anm-slot-selection.md` §4。
 * ★ 画面不走 ECL：TH12 的飞碟动画已移植进 ability.anm（script 164..167），
 *   由本文件起一个**跟随 VM** 画在敌机位置上 —— 因为 ECL 的 anm 索引表（th18:0x438d40）里没有 ability.anm
 *   （`N==0 → bullet.anm`、`N==1 → effect.anm`、`N==7 → 本关 st0Xwl.anm`、
 *    `N>=2 → 本关 ECL 头 anim{} 的第 N-2 条`；七关的 anim{} 里一个 ability.anm 都没有）。
 *   ⚠ 两边与 `assets/ecl/ufo.ecl.txt` 的那 8 个数必须一起改（同一条一手链写在那里）。 */
/* ★★ 击破时的**大扩圈消弹**载体（2026-09-18 补）：一手 = 在 UFO 位置生成一只 `UFO_EtBreak2`
 * 敌机（`life = 3000`、原地、无判定盒、40 帧后自己 `delete()`；见 `assets/ecl/ufo.ecl.txt` 那段注）。
 * ⚠ 它**会进 `enemy_count_real`**（与令牌不同）—— 一手也是真敌人，且只活 40 帧 ⇒ 照抄。 */
#define UFO_ECL_ETBREAK2_NAME   "UFO_EtBreak2"
#define UFO_ETBREAK2_LIFE       3000     /* ✅ 一手：`th12:0x44af46 mov [esp+0x4c],0xbb8` */
#define UFO_SE_ETBREAK2         0x1d     /* ✅ 一手 wav：TH12 的击破大消弹音 = se id 6 = `se_power1.wav`；
                                          * TH18 的同一枚 = `0x1d`（`engine/_shared/th18-sound-table.md`）
                                          * ⇒ **逐字节同名的 wav**，不是"借一个像的" */
#define UFO_ECL_HIDDEN_BODY_SCRIPT  (-1)  /* = `anmSetSprite(0, -1)`：拆槽，不建 VM（`assets/ecl/ufo.ecl.txt` 里那 8 个数）*/

/* 跟随 VM 的层（一手：`engine/anm/th18/02-render-stages.md` §1 的优先级表 —— 层 6/7/8/9/10/11 = 「敌机等」，
 * 都在相机 3 = 游玩区）。
 *   本体：**脚本自己写了 `layer(7)`**（`assets/ability/scripts/164_ufo_red.anm.txt`）⇒ 这里给同一个值
 *         （`ce_anm_spawn` 的 layer 只是实例化默认值，脚本里的 `layer(n)` 会覆盖）。
 *   令牌：脚本**没写** `layer()`（原生 bullet.anm 175..180 就没有）⇒ 这个参数是**唯一**的层来源。
 *         取 **13**：与原生道具同层（`faith.h` 的 `FAITH_LAYER` / `season.c` 的 `SEASON_POINT_LAYER`
 *         都是这个口径「世界层 13 = 相机 3 = 游玩区，与原生道具同层」）。⚠❓ 层号本身没在实机上验过。 */
#define UFO_BODY_ANM_LAYER   7
#define UFO_TOKEN_ANM_LAYER  13

/* 颜色 → 本体跟随 VM 的脚本号。★ 复用 `ufo_color_art_index()`（纯逻辑层，金 = 蓝那一格，用户裁决 ①）
 *   ⇒ **只有这一个出口**，与 `ufo_ecl_body_name()` 是同一张表（一个给 ECL 子程序名、一个给 anm 脚本号）。 */
static int ufo_body_anm_script(int32_t color)
{
    switch (ufo_color_art_index(color)) {
    case 0:  return CE_ANM_ABILITY_SCRIPT_UFO_RED;
    case 1:  return CE_ANM_ABILITY_SCRIPT_UFO_BLUE;     /* ★ 蓝**与金**都落这一格（金 = 蓝的调色版）*/
    case 2:  return CE_ANM_ABILITY_SCRIPT_UFO_GREEN;
    case 3:  return CE_ANM_ABILITY_SCRIPT_UFO_RAINBOW;
    default: return CE_ANM_ABILITY_SCRIPT_UFO_BLUE;     /* 非法色到不了（`ufo_color_valid` 守着）*/
    }
}

/* 颜色 → 令牌跟随 VM 的脚本号（**非彩虹**那一半；彩虹走 `ufo_tok_script()` 的三态）。
 * 贴图 = 原生 `bullet.anm` 175..180 的 6 态（`make_ufo_token_art.py`）。★ 2026-09-12（A8）：
 * 彩虹那三条（171..173）是**被截断**的版本（没有 `interruptLabel`）⇒ **这里不再给彩虹用**；
 * 彩虹改指 `ability.anm` **215..217**（全文版，三轮中断都在；★ 2026-09-18 订正：本行原写 219..221
 * 是 `anm_ids.h` 重新编号前的旧号 —— C 侧一律走宏名，所以行为从来没受影响）。 */
static int ufo_token_anm_script(int32_t color)
{
    switch (ufo_color_art_index(color)) {
    case 0:  return CE_ANM_ABILITY_SCRIPT_UFO_TOKEN_RED;
    case 1:  return CE_ANM_ABILITY_SCRIPT_UFO_TOKEN_BLUE;   /* ★ 金令牌用蓝那一套（用户裁决 ①）*/
    case 2:  return CE_ANM_ABILITY_SCRIPT_UFO_TOKEN_GREEN;
    default: return CE_ANM_ABILITY_SCRIPT_UFO_TOKEN_BLUE;   /* 彩虹到不了这里（`ufo_tok_script` 先截）*/
    }
}

/* ============================ ★★ 彩虹令牌：三段 interrupt + 每 150 帧换色（2026-09-12 A8）============================
 * 一手 = TH12 v1.00b `ItemManager__on_tick__body`（`th12:0x425c20`）的令牌分支
 * （`0x4262a0`–`0x4264f4`），全部用 `objdump -d` 逐条复核；下面每条都给了指令地址。
 * 复核手段：`cd local/th12.v1.00b && objdump -d --start-address=0x… --stop-address=0x… -M intel th12.exe`。
 *
 * **① 三段标签各是谁发的**（令牌的 anm VM **就嵌在道具对象里**；`vm+0x494` = on_interrupt 回调、
 * `vm+0x3c4` = `pending_interrupt`（u16），一手 `0x44a992`–`0x44a9a9` 的 Ufo 光环那段同构代码）：
 *
 *   | 标签 | 段正文（原生 `bullet.anm`） | 发送者 | 触发条件 |
 *   | --- | --- | --- | --- |
 *   | `3` 主循环 | `offset64:` 4 帧交替 + 32 帧 `rotateTime(±0.20943952)` | `FUN_004219e0(item)` `0x4219e0` | 玩家**离开** 60 px 圈 **且** 计时器 `< 90`（`0x426347`）|
 *   | `2` 快闪 | `offset340:` 同样的交替但**每 2 帧** | `FUN_0040d5d0(item)` `0x40d5d0` | ① 计时器 **== 90**（`0x426358`→`0x426363`）② 离开圈时计时器 `>= 90`（`0x426340`）|
 *   | `7` 固定色慢摆 | `sprite(<固定色>)` + 12 帧半周期慢摆 | `FUN_00421a10(item, 0)` `0x421a10`（函数体开头就是 `add eax,7`）| 玩家**进入** 60 px 圈那一帧（`0x42631c`）|
 *
 *   ⚠ 三条都是「**状态变了才发**」的闩，靠道具的 `+0x9c4` 标志（0 = 圈外 / 1 = 圈内）：
 *   进圈 `0x42630f`–`0x426321`（`cmp flag,0` / `jne` / 发 7 / `flag = 1`）、
 *   出圈 `0x42632d`–`0x42634c`（`cmp flag,0` / `je` / 按计时器发 3 或 2 / `flag = 0`）。
 *   `spawn_item` 把它清 0（`0x427595`）⇒ **出生时算「圈外」**，第一帧不会白发一次中断。
 *
 * **② 60 px 圈**：`0x4262b8 fld dist²` / `0x4262bc fcomp qword [0x4a4410]`（一手读出该 double = **3600.0**）
 *   / `0x4262c4 test ah,0x41` / `0x4262c7 jp 0x42632d`。
 *   ★ 这条 FPU idiom 的方向是**实测**过的（`test ah,0x41; jp` 在 `a > b` 时跳、`a <= b` 时落；
 *     用 32 位汇编 `flds a; fcomps b; fnstsw ax; testb $0x41,%ah; jp` 跑三种取值验证）：
 *     **jp 目标 = 圈外（dist² > 3600）、落下来 = 圈内（dist² ≤ 3600）**。
 *
 * **③ 计时器（`item+0x9a0`，= 计时器对象 `item+0x99c` 的 `cur`）**：
 *   圈**外**：只有公共那一次 `0x42636a Timer__increment(&item+0x99c)` ⇒ **每帧 +1**
 *            （`0x464a80`：`cur_f += 1.0`（`0x4a3ce0` = double 1.0）、`cur += 1`）。
 *   圈**内**：先 `0x4262db Timer__decrement(&item+0x99c, -1.0f)`（`0x4a3da8` 一手 = **−1.0f**；
 *            `0x464a20` 是 `cur_f += amt`）⇒ 与公共那次**抵消** ⇒ **冻结**；
 *            若 `cur == 90` 再多来一次 −1（`0x4262fe`/`0x42630a`，防止同帧的 2 号中断盖掉 7 号）。
 *            ⚠ 圈内**只有彩虹令牌**走这段（一手整段的门外是 `0x426276 mov eax,[edi+0x9b4]` /
 *            `cmp eax,0xd` / `0xe` / `0xf`，三命中跳 `0x42628f`；固定色令牌 10/11/12 不进）。
 *
 * **④ 150 帧**：`0x426483 cmp eax,0x96; jl`（`cur >= 150` 才换色）+ `0x426497 Timer__set_current(&t99c, 0)`
 *   + `0x4264a2 (type−0xc)%3` + `0x4264b6 type = 余数 + 0xd`（13→14→15→13）
 *   + `0x4264bc script = type + 0xa5`（= 178/179/180）。
 *   ★ 判阈值在 `0x426352`/`0x426483`，**推进在它之后**（`0x426368`/`0x42636a`），换色那一帧也照走一次推进
 *     ⇒ 稳定周期**正好 150 帧**（0→149 判、150 换、换完 cur = 1）；`90` 起开始快闪 ⇒ 最后 60 帧在闪。
 *
 * **⑤ 换色不是中断，是「重指脚本」**：`0x4264cb call 0x4061e0(vm = item, bullet_anm, script)`，该函数是
 *   `AnmVm__wipe`（`0x402520`）+ `0x454d10 AnmLoaded__load_script_into_vm`（**同一个 VM** 换脚本）
 *   —— 所以换色后 VM 从新脚本的主循环重开。
 *
 * ★ 本卡的对应物（**逐条照抄，差别只有下面这两条**）：
 *   ① 我们的令牌是**自造的敌机实体**（ECL 惰性壳）+ 一个跟随 VM ⇒ 「重指脚本」= **删旧 VM + 按新脚本
 *      再起一个**（SDK 没有「给已有 VM 换脚本」这条通路；见 `ufo_tok_rainbow_recolor`）。
 *   ② **不搬**「圈内连道具寿命（`item+0x988`，1200 帧）也冻结」那一条（`0x4262f0`）——
 *      那是 TH12 **道具**的寿命计时器，我们的 `ttl` 是自定值（`UFO_TOKEN_TTL_FRAMES`），换掉它等于
 *      顺手改一个跟本题无关的行为 ⇒ 留着，登记在文件尾。
 * ⚠ 一手的 60 px 圈**在实机上没验过**（我们照抄数值）；三段中断的实际观感同样待实跑（见文件尾）。 */
/* ★ 时序列自证（2026-09-12）：把 ②③④ 的指令逐条翻成一个状态机，与下面 `ufo_tok_rainbow_tick`
 *   的逐行实现做**逐事件对照**（13 种进出圈脚本：全程外 / 全程内 / 在第 5/89/90/91/148/149/150/151/300
 *   帧进圈再出圈 / 每 10 帧抖边界 / 出生即贴脸）⇒ 中断序列与换色帧**完全一致**
 *   （唯一差别是「同一帧里重复发同一个中断号」，而 `pending_interrupt` 是单寄存器、同值后写无影响）。
 *   全远场景下换色发生在第 **150 / 300 / 450** 帧（间隔恒 150），首次快闪在第 **90** 帧
 *   ⇒ 「90 起闪、150 换色、闪 60 帧」这条读法由指令顺序自洽。 */
#define UFO_TOK_INT_RESUME   3        /* 主循环（`interruptLabel(3)`）*/
#define UFO_TOK_INT_BLINK    2        /* 2 帧快闪（`interruptLabel(2)`）*/
#define UFO_TOK_INT_NEAR     7        /* 换固定色 sprite + 慢摆（`interruptLabel(7)`）*/
#define UFO_TOK_NEAR_R2      3600.0f  /* 60 px 的平方 —— 一手 `th12:0x4a4410` = double 3600.0 */
#define UFO_TOK_BLINK_AT     90       /* 一手 `0x5a`（`th12:0x426358`）：走到 90 起开始快闪 */
#define UFO_TOK_CYCLE_TICKS  150      /* 一手 `th12:0x426483`（`0x95 < cur`，即 `cur >= 150`）*/
#define UFO_TOK_RB_STATES    3        /* 三态：脚本 219/220/221（一手 type 13/14/15 → 178/179/180）
                                       * ⚠ 「219/220/221」是**落地当天的号**；并发开发期各人各拿一段靠后的号，
                                       *   收尾会跑 `assets/renumber_scripts.py --apply` 压紧 ⇒ 以
                                       *   `anm_ids.h` 的 `CE_ANM_ABILITY_SCRIPT_UFO_TOKEN_RAINBOW_*` 宏名为准。 */

/* ==================== ★★ 五边形框：提示 UI（2026-09-12 裁决 ③ 的**实装**；资产缺口 A7）====================
 * 用户口径（裁决 ③）：「玩家手上有**两个令牌**（= 再来一枚就能成组）时，屏幕上**这类令牌**外面
 * 会有一圈**五边形框**作提示」。本轮把「框本身」补齐（此前只有判据 + 钩子）。
 *
 * **一手全在这一段里**（TH12 v1.00b，全部 `objdump -d` 逐条复核，复核命令见 §A8 常量块的尾注）：
 *
 * ① **框 = `bullet.anm` script211**（扁平号 `0xd3`，脚本原文 `local/th12.v1.00b/anm/bullet/bullet.anm.txt`
 *    5511–5566 行）：
 *      `sprite(-1); layer(12); blendMode(1); drawPolyBorder(1.0f, 5); angleVel(0,0,0.034906585f);`
 *      `color(255,255,255); scale(0,0); scaleTime(10,4,20,20); stop();` 然后四段标签 +
 *      一段无名（`color(255,64,64); alphaTime(4,0,0); +4: delete()`）：
 *      `interruptLabel(7)` 红脉动 / `8` 蓝 / `9` 绿 / `10` 四色轮转。
 *    ⇒ **不用贴图**：`drawPolyBorder(1.0f, 5)` 的第二个参是**边数**（5 = 五边形），第一个是半径，
 *      多边形由引擎程序画（`sprite(-1)` = 不画精灵）⇒ 这项资产**没有 PNG 可切**，也没有 ORDER 条目。
 *      TH18 原生的同款写法（v8 opcode 605 可用）：`effect.anm` script101 `sprite(-1); drawPolyBorder(1.0f, 12);`、
 *      `ability.anm` script35 `sprite(-1); drawPolyBorder(112.0f, 64);`（都不写 `type()`，与 TH12 一致）。
 *    ⇒ 落地 = `assets/ability/scripts/223_ufo_pentagon.anm.txt`（正文**一字未改**地照抄 script211）。
 *
 * ② **触发点** = `ItemManager__on_tick__body`（`th12:0x425c20`）的令牌分支（`0x42658a`–`0x426843`）：
 *      · `0x42658a cmp dword [0x4b0c58],3 / jl 0x4265aa`（`0x4b0c58` = 已收枚数）
 *        ⇒ **已收满 3 枚（槽满）就一个框都不画**，并把已有的框删掉（落 `0x4260af` = 本条道具收工）。
 *      · `0x4265aa mov eax,[edi+0x9b4]`（道具类型）`add eax,-10 / cmp eax,5 / ja 0x4260af`
 *        ⇒ **只对令牌道具（类型 10..15）跑**。
 *      · `0x4265bc jmp dword [eax*4+0x426fc4]` —— 跳表（一手读出 `0x426fc4` 起的 6 个 dword）：
 *        `type10→0x4265c3`、`type11→0x426696`、`type12→0x426766`、
 *        **`type13→0x4265c3`（同红）、`type14→0x426696`（同蓝）、`type15→0x426766`（同绿）**。
 *        每条 case 的正文（三个 dword：`0x4b0c4c`、`0x4b0c50` = `COLLECTED_UFOS[0]/[1]`，颜色 1/2/3 = 红/蓝/绿）：
 *          `case 红(10/13)`：`slots[0]==1 && slots[1]==1` ⇒ 建框 + 标签 **7**；
 *                           否则 `{slots[0],slots[1]} == {2,3}`（两种顺序都算）⇒ 建框 + 标签 **10**；
 *                           其余 ⇒ 删框。`case 蓝(11/14)`：`2,2` ⇒ 8、`{1,3}` ⇒ 10。
 *                          `case 绿(12/15)`：`3,3` ⇒ 9、`{1,2}` ⇒ 10。
 *      ⇒ 一句话：**框的颜色 = 收了这一枚会得到的那只 UFO 的颜色**
 *        （同色补全 ⇒ 该色 7/8/9；三色各一 ⇒ 彩虹 10）。★ 这与本卡纯逻辑层的
 *        `ufo_group_peek_color()`（= `ufo_group_classify()`）**同一个含义**，所以标签直接由它算（见下）。
 *      ⇒ 原生彩虹令牌（type 13/14/15）走的是「**它当前显示的那一色**」那一条 case ——
 *        ~~我们的令牌颜色模型里彩虹是**通配**~~ ⇒ ★ 2026-09-18（新增 1「得是按色」）已按**原生字面口径**
 *        改掉：判据用 `ufo_tok_display_color()`（变色令牌按**当前显示的那一色**，`t->color` 只对
 *        非变色令牌等价）—— 原来那条「彩虹 = 通配」的偏离**作废**。
 *        （既有钩子 `ufo_pentagon_hint_for_color()`，已有单测），**有意不跟「当前态」那条**：
 *        否则会出现「框不亮、收下去却成了组」。⏳ 待用户看一眼（要改成原生字面口径 = 把这里的
 *        `t->color` 换成 `rb_state` 对应的色，一处）。
 *
 * ③ **生命周期**（`item+0x968` 存框的 VM id）：
 *      · 建框六处（`0x4265f0` / `0x426673` / `0x4266c3` / `0x426746` / `0x426792` / `0x426814`），
 *        每处都是 `push 0x0; push 0xd3; push &pos; push bullet_anm; mov eax,0x17; xor ecx,ecx;
 *        call 0x4615a0`（`AnmManager__create_vm`；`eax` = 层参数，`0x17` = 23 = **道具自己的层**，
 *        见 `spawn_item` `0x42771f lea eax,[edi+0x18]`（edi = −1）—— 脚本里的 `layer(12)` 随后覆盖它）
 *        + `mov [edi+0x968],id` + `call 0x461970`（发中断：标签在 **EBX**，函数体 `movsx edx,bx`
 *        → 写 `vm+0x3c4` = pending interrupt，并递归子树）。
 *      · **每帧只查 `cmp dword [edi+0x968],0 / jne`（`0x4265d5` 一族）⇒ 已经有框就什么都不做
 *        （不重建、也不重发标签）**。
 *      · 条件不成立 ⇒ `push [edi+0x968]; call 0x461a70`（`AnmManager__delete_vm`：置
 *        `vm+0x47c |= 0x10000000`，一手 `0x461a89`）+ `mov [edi+0x968],0`。
 *   ⚠ 「建完**立刻**发中断」不会跳掉脚本开头的初始化：一手 `AnmLoaded__load_script_into_vm`
 *     （`th12:0x454d10`）在末尾 `push vm; call 0x455630`（VM 的 update）**自己先跑了一帧**
 *     （`0x454db9`/`0x454dba`）⇒ `drawPolyBorder` / `layer(12)` 那时已经落地，下一帧 pending 中断
 *     才跳到标签段。TH18 侧同构：`AnmLoaded__instantiate_vm_to_world_list_back` `0x405bf0`
 *     的说明里也写着「**跑一帧**」（`engine/anm/th18/01-vm-instantiate.md` §0）
 *     ⇒ 本卡照抄「spawn → 立刻 interrupt」这个次序（先例：本文件 A8 的彩虹三段中断）。
 *
 * **本卡与原生的差别**（都是「载体不同」逼出来的，逐条登记）：
 *   ① 原生的框挂在**道具**上：位置在建框那一刻抄一次（`0x4615a0` 内部把 pos 拷进 VM），**之后不跟随**；
 *      我们的令牌是**敌机实体** + 跟随 VM（`ufo_tok_vm_spawn`）⇒ 框也每帧跟到实体坐标
 *      （`ufo_tok_penta_follow`）。令牌实体不动时两者等价；真 ECL 到位后会飘（`moveLimit` 活动框）、
 *      不跟随就会「框留在原地、令牌飘走」。
 *   ② 金在原生里不存在 ⇒ 金的框落在蓝那一格（`ufo_group_classify()` 内部过 `ufo_color_norm()`）——
 *      与用户裁决 ①（金 = 蓝的调色版）一致。
 *   ③ 原生的框在**框住的道具所在的那只 VM 之外另起一个 VM**（句柄存 `item+0x968`），我们也是：
 *      贴图那只在 `ufo_tok_t.anm_id`（卡私状态），框那只在 `s_penta_id[槽号]`（**文件静态** —— 私状态
 *      只剩 28 B，16 个槽 × 4 B = 64 B 放不下；理由与被 `_Static_assert` 拦下的过程写在那个数组的注里）。
 *
 * **层号**（★ 照抄原生）：脚本自己写 `layer(12)` —— 原生原值，且 12 落在世界层区间（3–19）里，所以
 *   **不是**「别作的层号」⇒ 照抄不改（对照先例：本体脚本 `164_ufo_red` 的 `layer(7)` 也是从 TH12
 *   `enemy.anm` script135 原样搬的）。我们的令牌跟随 VM 在 `UFO_TOKEN_ANM_LAYER` = **13** ⇒ 框正好
 *   在令牌**下面一层**，与原生「框 12 / 道具 VM 23」的相对关系同向。
 *   ⚠ 这不是 HUD：**场上实体的框**用世界层，**不**走 HUD 的 25（层 25 是「会被暂停模糊罩住」那一组，
 *   见 `HUD-LAYOUT.md` §4.1；框要跟着令牌在游玩区里动，进了 25 反而会被当 HUD 折到别组）。
 * ⚠ 待实跑：框的观感（2°/帧自转 + 10 帧从 0 胀到 20）与层 12 的压叠关系都没在实机上验过
 *   （文件尾 §未做/待跑 29d）。 */
#define UFO_PENTAGON_ANM_SCRIPT CE_ANM_ABILITY_SCRIPT_UFO_PENTAGON   /* 同号同名；`make anm` 生成（已重跑，= 223）*/
/* 框颜色 → 中断标签（一手：脚本 211 的 `interruptLabel(7/8/9/10)`；发送点 `mov ebx,7` 一族）。 */
#define UFO_PENTAGON_INT_RED     7
#define UFO_PENTAGON_INT_BLUE    8
#define UFO_PENTAGON_INT_GREEN   9
#define UFO_PENTAGON_INT_RAINBOW 10
/* 层（一手：脚本 211 自己写 `layer(12)`；理由见上面「层号」段）。 */
#define UFO_PENTAGON_ANM_LAYER   12

/* 彩虹令牌的态 → 脚本号。一手顺序 = `(type − 0xc) % 3`：13→14→15→13（`th12:0x4264a2`），
 * 脚本 = `178 + 余数`（`0x4264bc` 的 `+0xa5`）。
 * ⚠ **必须**用带 `interruptLabel` 的全文版（现号 215..217，`make_ufo_token_art.py` 生成）：
 *   168..173 里那三条彩虹（171..173）**没有标签** ⇒ 发中断进去也停在原地（`ce_anm_interrupt` 会静默无效）。 */
static int ufo_token_rainbow_script(int state)
{
    switch (state) {
    case 0:  return CE_ANM_ABILITY_SCRIPT_UFO_TOKEN_RAINBOW_RED;
    case 1:  return CE_ANM_ABILITY_SCRIPT_UFO_TOKEN_RAINBOW_BLUE;
    case 2:  return CE_ANM_ABILITY_SCRIPT_UFO_TOKEN_RAINBOW_GREEN;
    default: return CE_ANM_ABILITY_SCRIPT_UFO_TOKEN_RAINBOW_RED;
    }
}

/* 令牌跟随 VM 的脚本号 —— **唯一出口**（非彩虹 = 固定色那三条；彩虹 = 三态轮转）。 */
static int ufo_tok_script(int32_t color, int rb_state, int cycler)
{
    /* ★ 2026-09-18（新增 2）：变色令牌（一手 type 13/14/15）走**全文版**脚本（带三段 interruptLabel），
     * 普通令牌按色走那 6 态里的前 3 条。判据从「color == 彩虹」改成**显式的 cycler 标志** ——
     * 因为「彩虹」再也不是一个令牌颜色了（`color` 现在只可能是红/蓝/绿/金）。 */
    if (cycler) return ufo_token_rainbow_script(rb_state % UFO_TOK_RB_STATES);
    (void)rb_state;
    return ufo_token_anm_script(color);
}

/* ---- HUD 槽声明的块尺寸（表面 px）★★ 2026-09-18 第三版：**原版切图的三格令牌槽** ----------
 * ★★ 用户 2026-09-18 实跑 ⑤：「**飞碟槽 HUD 你没按照原版做**」—— 第一版是 `drawRect` 拼的占位
 *   （自己标了 ❓ 自定），第二版搬来了原版贴图/脚本但**只生成了资产、卡侧没接线**。
 *   这一版才把原版那两件真正画出来：
 *     · **三格令牌槽** = `ability.anm` 脚本 **302**（← TH12 `front.anm` **script80** 逐条移植）
 *     · **入槽闪光**   = `ability.anm` 脚本 **303**（← TH12 `front.anm` **script81**，31 帧自删）
 *   两件的贴图都是 `front/front00.png` 的 1:1 切图（生成器 `assets/ability/make_ufo_hud_art.py`）。
 * ★ 一手（三处互相印证，详见生成器文件头）：
 *     ① 槽只有**四态**：空 / 红 / 蓝 / 绿 —— 中断 10/11/12/13 → sprite59/60/61/62
 *        （`FUN_004214b0` `th12:0x4214b0`：`pending_interrupt = COLLECTED_UFOS[i] + 10`；
 *         逐像素量过原图：**sprite59 = 纯黑框 = 空槽**，60/61/62 = 红/蓝/绿）。
 *        ⇒ **原版没有彩虹色的槽**（与用户裁决「新增 2：没有彩虹色 token」吻合）。
 *     ② 「UFO 在场时槽闪烁」的真身 = `FUN_00421690` `th12:0x421690` 每帧**直接写 sprite 字段**：
 *        `sprite = COLLECTED_UFOS[i] + 59 + (闪烁 ? 4 : 0)` ⇒ **亮版那一组正好 +4**（59..62 / 63..66）。
 *        ⇒ ORDER 里槽的四态必须连续、亮版紧接其后 +4（生成器已 `assert`）。
 *     ③ 新令牌落进哪一格 = 对该格的 VM 发**中断 17**（出现：alpha 0→255 + scale 0→1，30 帧），
 *        同时起一个脚本 303 的闪光 VM：`interrupt(7 + 格号)` 摆位 + `interrupt(10 + 四态)` 上色
 *        （`FUN_004214b0` 的 0x20a02 段：`push 0x51` 建 VM、`lea edx,[ebx+7]`、`ax = [COLLECTED_UFOS[arg]] + 0xa`）。
 * ★ 块认领 = **自带锚点（`CE_HUD_SIDE_PINNED`）钉在原版槽的位置上**，理由两条：
 *   ① 槽/闪光的坐标**写在脚本里**（原版把三个槽位做成 `posTime` 中断 7/8/9 的目标点，那是原版配方，
 *      照抄；Δ7 起同一组数字在 C 侧也有一份 —— `k_ufo_slot_x[]`，摆位由 C 补差值）⇒ 块必须钉在与
 *      脚本数字一致的地方，走货架自动排布的话一旦被重排（别的卡先占位），
 *      块会和画面上的槽错开（"块在这、槽在那"是这类 HUD 最典型的坑）；
 *   ② ★★★ 2026-09-22 订正（P3 #1）：原版槽在 **游玩区的左下角**，不是屏幕左下角的黑边里 ——
 *      一手 `front.anm` script80 的 `pos(52/73/94, 456)` 是 **TH12 那套 anm 世界空间**的坐标，
 *      而那个空间里**游玩区占 x ∈ [32,416]、y ∈ [16,464]**（同文件 script82 的大横幅写
 *      `pos(224, 208)` = 场地中心；script79 的小横幅滑到 `x=32, y=467` = 场地左沿/下沿下 3 px）
 *      ⇒ 换算成**场地相对**坐标：x = 52/73/94 − 32 = **20/41/62**、y = 456 − 16 = **440**
 *      （槽高 16 ⇒ 底边 448 = **与场地下沿齐平**）。
 *      TH18 的场地在表面 px 里 = x 256..1024 / y 32..928（`engine/anm/th18/02-render-stages.md` §1、
 *      用户 1280 截图实测左边缘 257 / 底边 928）⇒ 槽心 = **表面 (296, 338, 380) × 912**。
 *      ⚠ 旧实现按「640 空间 52/73/94」直接 ×2 写 = 表面 104/146/188。
 *        ★ 2026-09-24 订正（owner）：「**你对渲染坐标的理解一直有问题，它真的没有到什么黑底部分**」
 *        —— 场地左沿以外那条带子是 **TH18 自己的 HUD 面板区**（原生分数/残机/Bomb/当前卡都在那儿），
 *        **不是黑底**；所以「直接 ×2」既不是"错在黑边"，也不该用"黑底看不见"去否掉它。
 *        当年那次「**缺少飞碟(token)槽位 UI**」的真因是 **Δ7 三格叠在同一点**（见下面 Δ7 那段日志证据），
 *        与底色无关。
 *   · ★★★★ 2026-09-23 末轮 **一手定案**（把②那套换算从"推断"升成"复刻"，并推翻"直接 ×2"）：
 *     反出 TH12 的 `front.anm` 原文（`python3 tooling/thtk/unpack.py th12.v1.00b` ⇒
 *     `local/th12.v1.00b/anm/front/front.anm.txt`，`front.err` 为 0 字节）逐条读，四条互相咬合：
 *       ① 一手**槽**脚本 = **script80**（`sprite(sprite59); ins_85(1); randMode(1); resampleMode(1);
 *          anchor(0, 0); layer(20); pos(52, 456)`；中断 **7/8/9** ⇒ `posTime(8, 0, 52/73/94, 456, 0)`
 *          = 三格摆位；中断 **10/11/12/13** ⇒ `sprite(59/60/61/62)` = **四态**；中断 **14** ⇒ 吸收
 *          （`posTime(8,0,31,456)` + `pos(82,464)`）；中断 **17** ⇒ 出现（`alpha(0)+alphaTime(30,0,255)
 *          + scale(0,0)+scaleTime(30,4,1,1)`））—— 与本仓端口**逐条同形** ✓（302 ← 80，303 ← 81 闪光）。
 *       ② 一手**小横幅** = script79：`pos(-96, 467)` → `posTime(60, 4, 32.0f, 467.0f)` ⇒ 滑到 **x = 32**、
 *          y = 467 = 场地**下沿(464)外 3 px** ⇒ 「贴场地下左角」的招牌位。
 *       ③ 同文件场地**四沿**：script3 `pos(416, 0)`、script4 `pos(32, 0)`、script5 `pos(32, 464)`
 *          （全 `anchor(1,1)`）⇒ 左右两沿 = **x 32 与 416**，下沿 = **464**；而 416 − 32 = **384**
 *          = TH12 场地**宽正好 384** ✓。
 *       ④ **判据（决定性一条）**：横幅族的 x 中心在文件里写作 **`pos(224, 240)`**（另有 `pos(224, 208)`、
 *          `pos(224, 144)`、`pos(224, 32)` 成串）= (32+416)/2 与 (16+464)/2 ⇒ 这个 anm 空间里
 *          **场地中心 = (224, 240)**。TH12 的场地在 640×480 屏幕里居中 ⇒ 屏幕空间下中心应写 **(320, 240)**，
 *          但**全文件没有 320**（x 最大 528）⇒ **该 anm 空间 = 640 屏幕空间在 x 方向整体 +96、y 方向不偏**。
 *       ⇒ 一手槽在**屏幕** = 52/73/94 + 96 = **148/169/190 单位 = 表面 296/338/380** —— 正是本文件的值，
 *         且 +96 与 Δ5 是**同一个数** ⇒ **Δ5 属复刻，不是 ❓自定**（原 ❓ 标签作废）。
 *       ⇒ 「直接 ×2」的 104/146/188 落在场地**外**的左侧黑边（TH18 场地左沿 256）**是错的**（与 owner
 *         实跑那次「缺少槽位 UI」互相印证）。**尺子自检**：槽心距场地左沿 40 表面 px = 场地宽 768 的
 *         **5.2%**，一手 20/384 也是 **5.2%** ✓（相对场地完全一致；owner 18:14 报的「偏右 1/3 个游玩区宽」
 *         是**相对屏幕左沿**的观感 —— 一手槽在屏幕 148/640 = 23%，与一手不符，要动即**故意偏离**）。
 *   ⚠ 实测过的相邻占位（`HUD-LAYOUT.md` §1.4）：110/111 组合 UI = 表面 x 64..252（本块在它右侧，
 *     不重叠 ✓）；**116（263..365 / 888..920）与 117（257..449 / 876..924）与本块（272..404 /
 *     896..928）横向重叠** —— 这是"按一手的场地左下角"与"116/117 也占着同一条带子"的**口径冲突**，
 *     已登记 `TUNING.md` §2.0v 待 owner 拍；**若 owner 决定故意左移**，改生成器 `X_SHIFT_640`
 *     （一行，重跑生成器 + `make anm`）并同步本文件 `k_ufo_slot_x[]` / `UFO_HUD_PIN_X`
 *     —— ⚠ 注意改回 `0.0` 就是回到已被用户报过「看不见槽」的那一版。
 * ★★★ Δ7（2026-09-23「槽位只有 1 个」工单）—— **摆位改由 C 侧接管**，理由是一手实跑证据：
 *   · 用户实跑：三格只看得见 1 个。日志（`ufo/slot.vm` 1539 行）里三格的**脚本 pos.x 恒 = 296**，
 *     即三只 VM 叠在同一点上（详见 `ufo_hud_vm_log()` 的注：旧诊断把 `+0x2c` 当 pos 读，
 *     真正的 pos 在 `+0x30`，那一列三格全是 296）。
 *   · 同一份日志证明**中断机制本身是好的**：三格的 alpha/scale 各自按 30 帧爬升（= 中断 17 的
 *     `alpha(0)+alphaTime(30,0,255)+scale(0,0)+scaleTime(30,4,2,2)` 真在跑），但**只有摆位那一条
 *     （一手 TH12 的 7/8/9 → `posTime`）从不生效** ⇒ 原版那套「脚本自己摆位」在 TH18 里没落地。
 *   · 绘制点 = `pos(+0x30) + entity_pos(+0x5f0) + __pos_2(+0x4e4)`（一手 `th18:0x48171f`）⇒
 *     直接写前两项都会和脚本打架。Δ7 的做法：**每帧把差值写进 entity_pos**，让三项之和**恰为**槽心：
 *         `entity_pos = 槽心 − (pos + __pos_2)`
 *     这个式子是**自校准**的：脚本那两项是什么值都不影响结果（将来若 7/8/9 生效了，pos 变成目标值，
 *     entity_pos 自动归零，画面不动）。
 *   · 中断 7/8/9 照旧发（一手配方原样保留，无害）；脚本 302/303 **一个字都没改**。 */
#define UFO_ID_NUM        112u      /* 本卡 id（HUD 槽 / 日志用；`CE_CARD(112, …)` 里的那个数）*/
/* 块 = 三格槽的外沿（表面 px）：槽心 **272/314/356**（间距 42 表面 px）、槽 24×16 单位 = 48×32 表面 px
 * ⇒ 块 x **248..380** / y 896..928。钉块用的锚点 = 块的**左下角**
 * （`ce_hud_slot_claim_pinned_card` 的口径，与 `faith_hud.c`、`trance.c` 同）。 */
/* ★★★ 2026-09-23（owner 第二次实跑，原话）：
 *   「HUD 的一手一直有问题，**令牌槽就应当往左移动 1/4 个游玩区半径**；然后剩余文字相对于 UFO
 *     也是得往左移动 1/4 个游玩区半径」
 *   ⇒ 令牌槽整排、剩余时间文字**都按这个量左移**。量纲（一手，见 `engine/anm/th18/01-vm-instantiate.md`
 *     §"游玩区 = ECL x ∈ [−192,192]、y ∈ [0,448]"）：游玩区**半径 = 192 单位**（x 半宽）
 *     ⇒ 1/4 = **48 单位 = 96 表面 px**（1 单位 = 2 表面 px；也是 640 空间里的 48 px）。
 *   ⚠ 这是 **owner 口径**、**不是复刻**：一手 TH12 槽在屏幕 148/169/190（640 空间，= 表面 296/338/380）、
 *     距场地左沿只有 20..62 单位。owner 看过两轮都判"偏右"（上一轮记的是「偏右 1/3 个游玩区宽」）
 *     ⇒ 按 owner 的量左移。**一个常量就能回退**：`UFO_HUD_OWNER_DX_SURF`（改 0 即回上一版的位置）。
 *   ★★ 2026-09-24 订正（owner 原话：「**你对渲染坐标的理解一直有问题，它真的没有到什么黑底部分**」）：
 *     场地左沿（表面 257）以外那条带子是 **TH18 自己的 HUD 面板区** —— 原生的分数 / 残机 / Bomb /
 *     「当前卡」UI 全都在那儿（原生当前卡 UI = 表面 x 68..116）⇒ **系统槽本来就该放这一带**。
 *     旧注里"落进黑边 ⇒ 空槽（纯黑框）看不见"那条推理**不成立**，已作废；空槽贴图保持原生、不动。
 *     历次左移的量都只是**位置口径**，与底色无关。*/
#define UFO_HUD_OWNER_DX_SURF  (-180.0f)    /* ★★ owner 2026-09-24 第 19 轮（在他两次"偏右"之后）：
                                             *   **槽心 92/134/176、块 x 68..200** —— 与**原生那张「当前卡」UI
                                             *   的左沿（68）对齐**（owner 从三个候选里选的 C）。
                                             *   ⚠⚠ 订正我自己一直搞错的一件事（owner 原话：「你对渲染坐标的理解
                                             *   一直有问题，它**真的没有到什么黑底部分**」）：场地左沿以外
                                             *   那条带子（表面 x < 257）**不是黑底**，那是 TH18 自己的
                                             *   **HUD 面板区**（原生的分数/残机/Bomb/当前卡都在那儿）——
                                             *   系统槽本来就该放这一带，所以"空槽是黑框 ⇒ 在黑边上看不见"
                                             *   那条担心**不成立**（当年那次「缺少槽位 UI」的真因是 Δ7 的三格
                                             *   叠在同一个点，不是底色）。空槽贴图**保持原生**、不动。
                                             *   ⇒ 累计左移 −180（一手场地左下角 296/338/380 起算）；
                                             *   回退/再调 = 只改这一个常量。*/
#define UFO_HUD_BLOCK_W   132.0f    /* = 三个槽的外沿宽（左移不改宽）*/
#define UFO_HUD_BLOCK_H    32.0f    /* = 928 − 896（槽本体高，底边与场地下沿齐平）*/
#define UFO_HUD_PIN_X     (248.0f + UFO_HUD_OWNER_DX_SURF)   /* 一手按 113 的口径 248，再按 owner 左移 */
#define UFO_HUD_PIN_Y     928.0f    /* = 640 空间 464 = **场地下沿**；槽本体落在 896..928*/
/* ★★ 三格的**槽心**（表面 px）= Δ7 的摆位目标（脚本里那一份是 296/338/380，差值由 entity_pos 补，
 * 见上「Δ7」注的 `entity_pos = 槽心 − (pos + __pos_2)` 自校准式 ⇒ 这张表是**唯一摆位源**）。
 * 相邻间距 21 单位 = **42 表面 px**。
 * ★★★ 2026-09-23 **owner 裁决 ②（故意左移）+「参考一下 113 的位置」**：一手值 = **296/338/380**
 *   （已在上面「定案」注里证死，+96 属复刻），但 owner 实跑观感「偏右、不在左下角」⇒ **改按 113 的口径对齐**：
 *     113（同在左下角带的系统卡）的块锚点是 `trance.c` 的 `TRANCE_HUD_BASE_X/Y = 248/928`
 *     （一手 TH13 根 x = −196 场地坐标 ⇒ 表面 640 + (−196)×2 = **248** = **场地左沿 256 外 8 px**；
 *      y 928 = **场地下沿** ⇒ 与「场地左下角」同一个口径，只是**允许探出左沿一点点**）。
 *   ⇒ 本卡取**块左沿 248**（= 113 同值）⇒ 槽心 = 248 + 24（半格）= **272/314/356**，块 x **248..380**、
 *     底边 928 与 113 **完全同一条基准线**；槽组左沿比一手左移 **24 表面 px**（= 场地宽的 3%）。
 *     · **红线**：再往左（含旧的 104/146/188）槽就深进**场外左侧黑边**，而空槽是**纯黑框** ⇒ 黑底画黑框，
 *       正是 owner 报过「缺少飞碟(token)槽位 UI」那一版 ⇒ **左移到此为止**。
 *     · 代价：与 116/117 的块重叠更多（`TUNING.md` §2.0v 的口径冲突，owner 已知）。
 *   ⇒ **偏离一手是 owner 明确裁决的**，不是我们的自定；要回一手就把这张表改回 296/338/380
 *     + `UFO_HUD_PIN_X` 改回 272（两行，行为改动 ⇒ 发 DLL）。
 * ⚠ 是**数组**不是 `X0 + i*DX`：本文件有"零 x87"硬门（`make dllx87`），`(float)i` 这类整→浮转换
 *   在本仓是明令避免的写法（同文件尾 `_Static_assert` 那一带的先例），常量表最稳。 */
static const float k_ufo_slot_x[UFO_SLOT_COUNT] = {
    272.0f + UFO_HUD_OWNER_DX_SURF,      /* 176 */
    314.0f + UFO_HUD_OWNER_DX_SURF,      /* 218 */
    356.0f + UFO_HUD_OWNER_DX_SURF,      /* 260 */
};
/* ★ 槽底那抹毛笔横幅（脚本 315）跟槽是**同一个视觉组** ⇒ 同量左移（写 entity_pos，脚本自己的
 *   `pos`/`posTime` 一个字不改 ⇒ 淡入动画照旧）。owner 只说了"槽"，但底衬不跟着走就成了错位。 */
#define UFO_HUD_BANNER_DX_SURF  (UFO_HUD_OWNER_DX_SURF)
#define UFO_HUD_SLOT_Y 912.0f       /* = 456 ×2（一手槽 y）；槽高 16 单位 = 32 表面 px ⇒ 896..928 */
_Static_assert(UFO_SLOT_COUNT == 3, "k_ufo_slot_x holds exactly the three TH12 slot columns");
/* 槽的 **sprite 基数**与**亮版偏移**：ORDER 里 457/458/459/460 = 空/红/蓝/绿，461..464 = 亮版
 * ⇒ 与一手 `FUN_00421690` 的 `59 + 四态 + (闪烁 ? 4 : 0)` **同一个算式**（`anm_ids.h` 生成）。 */
#ifdef CE_ANM_ABILITY_SPRITE_SLOT_EMPTY
#define UFO_HUD_SPR_SLOT_EMPTY   CE_ANM_ABILITY_SPRITE_SLOT_EMPTY   /* 457 */
#else
#define UFO_HUD_SPR_SLOT_EMPTY   457    /* `make anm` 后换宏（上面那支优先）*/
#endif
#define UFO_HUD_SLOT_BLINK_OFFSET 4     /* 一手：亮版那一组 = 基址 + 4（59..62 → 63..66）*/
#ifdef CE_ANM_ABILITY_SPRITE_SLOT_EMPTY_B
/* ★ 编译期断死生成器的约定：四态连续、亮版正好 +4（`make_ufo_hud_art.py` 侧也 assert 了 ORDER 顺序）*/
_Static_assert(CE_ANM_ABILITY_SPRITE_SLOT_GREEN == CE_ANM_ABILITY_SPRITE_SLOT_EMPTY + 3,
               "the four slot states must be four consecutive sprite ids (empty/red/blue/green)");
_Static_assert(CE_ANM_ABILITY_SPRITE_SLOT_EMPTY_B == CE_ANM_ABILITY_SPRITE_SLOT_EMPTY + 4,
               "the bright slot set must start exactly +4 after the normal set (one-hand: 59..62/63..66)");
#endif
/* 槽的四态（0 = 空，1 = 红，2 = 蓝，3 = 绿）—— 与中断号 `10 + 四态` 一一对应 */
#define UFO_HUD_STATE_MAX 3
#define UFO_HUD_INTERRUPT_STATE0 10     /* 一手：`FUN_004214b0` 的 `COLLECTED_UFOS[i] + 10` */
#define UFO_HUD_INTERRUPT_SLOT0   7     /* 一手：GUI 构建时给三个槽发的 7/8/9（x = 52/73/94）*/
#define UFO_HUD_INTERRUPT_APPEAR 17     /* 一手：新令牌入槽那一格的出现动画 */
#define UFO_HUD_FLASH_FRAMES     31     /* 一手：脚本 303（← script81）在 +31 帧 `delete()` */
#define UFO_HUD_BLINK_PERIOD     16     /* 一手：`FUN_00421690` 的 `(timer % 16 == 1)` —— 16 帧闪一下 */
/* ==================== ★★ 击破计时器（P3 #9「UFO 缺少击破计时器」）====================
 * 一手 = `Ufo::on_draw_16` `th12:0x44a6e3`–`0x44a7e8`（**逐指令读出**，不是猜「HUD 数字/条」）：
 *
 * ```
 * 44a6e3  mov ecx,[ebx+0x38]      ; zUfo+0x38 = 场上 UFO 的 enemy_id（0 = 没有 UFO）
 * 44a6ee  cmp [ebx+0x14],0x3c     ; zUfo+0x14 = 存活帧数；**< 60 帧不画**
 * 44a72c  ... 找到那只敌人（链表按 enemy_id 走）
 * 44a782  mov eax,[ebx+0x28]      ; zUfo+0x28 = 存活上限 **720**
 * 44a78d  sub eax,[ebx+0x14]      ; ★ remaining = 720 − 存活帧数
 * 44a796  mov [esp+0x2c],eax
 * 44a7ac  fild [esp+0x2c]         ; float(remaining)
 * 44a7b0  fdiv [0x4a3d20]         ; ÷ **60.0**          ⇒ 秒（double 直读 = 60.0）
 * 44a7be  fld [0x4a3ce8]          ;    **100.0**
 * 44a7c4  fmul st,st(1)           ; × 100
 * 44a7c6  call 0x4931e0           ; (int)
 * 44a7cc  mov ecx,0x64 / idiv     ; ÷ 100 ⇒ eax = 秒、edx = **百分秒**
 * 44a7d3  push edx / push eax
 * 44a7da  push 0x4a215c           ; ★ 格式串 = **"%2d:%.2d"**
 * 44a7e3  call 0x4020a0           ; AsciiManager__printf（= 我们的 ce_hud_text 同一条路）
 * ```
 *  · 颜色：`44a778 mov [AsciiMgr+0x18f80],0xc080ffc0` = **A=0xc0 / R=0x80 / G=0xff / B=0xc0**（淡绿）。
 *  · 字号：`44a66e fld1` ⇒ **1.0**（`[+0x18f84]/[+0x18f88]`）；字符集 `[+0x18f98] = 2`。
 *  · 位置：基座 = 「敌人的世界坐标」（`44a748`–`44a772` 的 `enemy+0x1074/78/7c` 再加那套空间换算项
 *    32+192 / 16，与环/血条同一个点）⇒ x 再 **−12.0**（`44a7a2 fsub [0x4a4128]` = double 12.0）、
 *    y 再 **+20.0**（`44a790 fadd [0x4a4170]` = double 20.0，那本是行距）⇒ 落在 UFO **正下方 20 单位**、
 *    x 左移 12（= 5 个字符手工居中；我们用 `align_h = 居中` 拿到同一个结果，见下面的实现）。
 *  · 单位：`%2d` 的秒最大 **12**（720/60）⇒ 显示形如 `12:00`、`11:91`、` 7:00`（秒:百分秒，**不是**分:秒）。
 *  · 消失条件同出现条件：UFO 不在场上 / 那 60 帧宽限内 / 敌人已不在 ⇒ 不画。
 * ⚠ 一手里它由 `AsciiManager` 画（TH12 整个 HUD 都是 ASCII）⇒ **我们照抄同一条路（`ce_hud_text*`）**，
 *   而不是自己另起贴图 VM：这正是 METHOD「能复刻就不要自己写」。
 *   代价（`SDK.md` §16 已声明）：ASCII 文字**结构上盖不进暂停模糊罩**，暂停时它会在罩子上面 ——
 *   原版的计时器同样是 ASCII（TH12 没有这层罩子），这条差异登记在 `TUNING.md` §2.0v。 */
#define UFO_TIMER_FIRST_FRAME  60u      /* 一手 0x44a6ee：存活帧数 >= 0x3c 才画 */
/* ★★ 2026-09-23（owner 原话）：「剩余文字相对于 UFO **也是得往左移动 1/4 个游玩区半径**」
 *   ⇒ 在「居中于 UFO」之上再左移 **48 单位**（= 192/4；640 空间里就是 48 px）。
 *   ⚠ owner 口径、不是复刻：一手 TH12 是 `fsub [0x4a4128]` = 12.0，而那 12 是**把 5 个字符手工居中**
 *     （TH12 的 ASCII 默认左对齐）—— 我们用 align_h = 居中已经等价 ⇒ 那一处**不复抄**，
 *     这里 -48 是 owner 看画面要的位移。回退 = 把这个数改回 0.0。*/
#define UFO_TIMER_OWNER_DX640  (-48.0f) /* = −(192 / 4)（owner 2026-09-23 口径）*/
#define UFO_TIMER_DX640        0.0f     /* 一手那 −12.0 是**手工居中**；我们用 align_h=居中 ⇒ 不再位移 */
#define UFO_TIMER_DY640        20.0f    /* 一手 0x44a790：`fadd [0x4a4170]` = 20.0（UFO 正下方）*/
#define UFO_TIMER_COLOR   0xc080ffc0u   /* 一手 0x44a778：D3DCOLOR 0xc080ffc0（淡绿）*/
#define UFO_TIMER_LOG_EVERY    60u      /* 诊断行节奏（跟环那一条一样，每 60 帧一行）*/
/* 2026-09-23: the 3-slot per-VM dump was also every 60 frames -> 1737 lines in a 10-minute session
 * (ufo/slot.vm + ufo/slot:), drowning everything else in the log => throttled to 600 frames (10 s).
 * The countdown line (ufo/timer:) stays at 60 frames (it is live state, not noise). */
#define UFO_SLOT_LOG_EVERY    600u      /* custom: per-VM slot dump cadence */
/* HUD 脚本号（`make anm` 生成的宏优先；没有宏时用字面量 —— 与 gauge 那一组同一写法）*/
#ifdef CE_ANM_ABILITY_SCRIPT_UFO_HUD_SLOT
#define UFO_HUD_SCRIPT_SLOT  CE_ANM_ABILITY_SCRIPT_UFO_HUD_SLOT     /* 302 */
#else
#define UFO_HUD_SCRIPT_SLOT  302
#endif
#ifdef CE_ANM_ABILITY_SCRIPT_UFO_HUD_FLASH
#define UFO_HUD_SCRIPT_FLASH CE_ANM_ABILITY_SCRIPT_UFO_HUD_FLASH    /* 303 */
#else
#define UFO_HUD_SCRIPT_FLASH 303
#endif
/* ★★ 2026-09-23（owner #17「飞碟槽**底下**还有一抹**毛笔**艺术化贴图」）：一手 = TH12 `front.anm`
 *   **script79**（`sprite67` = `front00.png` (336,368,96×16)，`anchor(1,2)`、`layer(19)`、`alpha(128)`、
 *   `pos(-96,467)` → `posTime(60,4, 32,467)` → `+30: alphaTime(20,0,255)`）。
 *   换算到表面（该 anm 空间 x 方向 +96、坐标 ×2）= **(256, 934)** ⇒ 正好压在那排槽
 *   （248..380 / 896..928）**下面** ✓ 与 owner 的原话一致。生成器 `make_ufo_hud_art.py` 把它
 *   逐条移植成 ability.anm 的 **script315**（`ufo_hud_banner`），本卡只负责起一支 + 收干净。*/
#ifdef CE_ANM_ABILITY_SCRIPT_UFO_HUD_BANNER
#define UFO_HUD_SCRIPT_BANNER CE_ANM_ABILITY_SCRIPT_UFO_HUD_BANNER  /* 315 */
#else
#define UFO_HUD_SCRIPT_BANNER 315
#endif
_Static_assert(UFO_HUD_BLOCK_W > 0.0f && UFO_HUD_BLOCK_H > 0.0f,
               "the UFO HUD block is claimed now (user ruling 4) - a zero-size claim would be a bug");

/* ============================ ★ 调试键（2026-09-18；★ 2026-09-23 改键位）================================
 * 形状**完全照抄 117（TH17 动物灵）`animal.c` 的那一套**（`ANIMAL_DEBUG_KEY` 那一块）：
 *   · `ce_key_down(CE_VK_SPACE)` + 卡状态里一个 `dbg_key_prev` 做**边沿检测**（拍一下出一批，按住不连发）；
 *   · 固定落点 + **一行日志**；
 *   · 一个宏开关：`UFO_DEBUG_KEY`（发布前改 0 即可全部停用 —— 117 的发布版就是 0）。
 * ⚠ 为什么能用空格：`ce_key_down` 走 `GetKeyboardState`（user32），空格**不在游戏配置里、也不进 replay
 *   输入流** ⇒ 只作调试用（117 的原话，同一只手一验过）。
 * ⚠ 本卡不吃 `CE_INPUT_*`（那是引擎的游戏输入），所以这几枚键与「按不按 shift/Z」完全不冲突。
 *
 * ★ 2026-09-23（用户）：「**本卡的 shift+空格调试换成：爆大量 P 点和金钱点，方便测试 UFO 吸收**」
 *   ⇒ 键位表如下（`UFO_DEBUG_*` 三个宏都是 ❓自定，只为调试）：
 *     · **空格**          = 生成 1 枚令牌（一手小怪那种**固定色**）—— 不变
 *     · **Shift+空格**    = **爆一批 P 点 + 金钱点**（按下表的网格铺开）—— ★ 本轮新行为
 *     · **Ctrl+空格**     = 生成 1 枚**变色**令牌（原 Shift+空格 那件；搬过来是**为了让「变色」那条链
 *       仍然有键可测**，`AUDIT` §BE 详报过「token 变色」是你报过的项；不想要就删这一个 `else if` 分支）
 */
#define UFO_DEBUG_KEY        1        /* ★ 调试期开着；**发布前改 0** */
#define UFO_DEBUG_X          0.0f     /* 令牌落点 = 画面中央（与 117 的 (0, 224) 同一口径：ECL 640 空间）*/
#define UFO_DEBUG_Y          224.0f   /* ⚠ 令牌的活动框是 y ∈ [160,370]、x ∈ [−176,176] ⇒ 这一带安全 */

/* ---- Shift+空格 那一批点的铺法（❓自定）------------------------------------------------------------
 * 为什么要「一批」：UFO 吸收链是**每帧扫全池**、18 单位内直接吞、之外靠累计速度拉（`ufo_absorb_tick`）
 * ⇒ 一次铺一大片才能同时压到「吞」与「拉」两条路，也才看得出 fill 涨得快不快。
 * 铺法：以**场上的 UFO 为圆心**（没有 UFO 就围着自机）铺一个 `COLS×ROWS` 的**固定偏移网格**——
 *   · **不用三角函数**：本仓硬门是「我们的 object 里 0 条 x87」，表驱动最省事也最稳（`make dllx87`）；
 *   · 两种点**各占一半的格子**（交错），这样一眼能看出 P 点与金钱点都被吸；
 *   · 网格半径刻意 > 18 单位（吸收圈）⇒ 看得见它们**一颗一颗流进去**，而不是一帧全没。
 * ❓ 数量/步长/半径全是调试口径：嫌少改 `ROWS`，嫌挤改步长。 */
#define UFO_DEBUG_ITEM_COLS   8        /* 每行 8 颗 */
#define UFO_DEBUG_ITEM_ROWS   6        /* 6 行 ⇒ 每色 24 颗、一次共 48 颗 */
#define UFO_DEBUG_ITEM_DX     12.0f    /* 网格步长（ECL 单位；8 列 ⇒ 横跨 84 单位 ≈ 168 表面 px）*/
#define UFO_DEBUG_ITEM_DY     14.0f    /* 行距（6 行 ⇒ 纵跨 70 单位）*/
#define UFO_DEBUG_ITEM_ANGLE (-1.5707964f)  /* 初速方向 = **正上方**（一手奖励/普通道具都用 −π/2 `th12:0x44aca0`）*/
#define UFO_DEBUG_ITEM_SPEED  1.4f     /* 初速大小（一手普通道具 2.2 / 令牌 1.4 ⇒ 调试取后者，慢一点好看）*/

/* 帧源：`on_item_frame`（`0x446ee3`）每次分派 = 一个**正常游玩帧**（冻结/商店帧连门卫 CMP 都到不了；
 * 114 `collect2x.c` 的一手推导）。本卡全程用它当帧计数，不用 `card+0x20` 那个钟。 */

/* ------------------------------------------------------------------ 卡状态
 * 私有状态预算 = `CE_STATE_BYTES`(512) − `CE_STATE_RESERVED`(16) = 496 B（sdk_core.h）。
 * ★★ 2026-09-12（第二轮换池）体积账：令牌从「真敌人壳 + 20 B/槽」变成「自建 VM + 位置/速度」，
 *   **每槽变大了** —— 实测 `sizeof(ufo_tok_t)` = **40 B**（20 → 40：`motion` 20 B 顶掉了原来那句
 *   「再加一个 12 B 的 `ufo_tok_motion_t`」的算法），池 `UFO_TOKEN_CAP` = 5 ⇒ 200 B，**再加**
 *   五边形框句柄表 5 × 4 B = 20 B ⇒ 220 B；而 `sizeof(ufo_card_t)` 换池前只剩 8 B 余量（488/496）
 *   ⇒ 照**先例**（117 `animal.c` 的 `s_tok[40]`、113 `trance.c` 的 `s_spirit[128]`）把**整张令牌池
 *   连同框句柄一起搬进文件级静态区**（`s_tokpool` / `s_penta_id`，见它们自己的注）。
 * ⚠ 卡私状态里**一丝 token 状态都不留**（实测 `sizeof(ufo_card_t)` = 144 B；换池前是 488 B）。
 *
 * ---- ★★ 令牌池：换池时**一起删掉**的四条旧状态（逐条给依据，别当成漏了）
 * 换池前这里有：令牌颜色**登记环**（`pending_color[]` + `pending_n` + `pending_token` + 环满记账
 *   `pending_drop` + 容量宏 + 一对 `put/take`）+ `on_tick_2` 里的一整段「出队 → 落槽」。
 * **依据（为什么它们只为旧载体而存在）**：那套环的**全部**用途是「**令牌实体被子弹/炸弹打掉时**，
 *   在死亡回调里把它的颜色登记下来、帧末再落进凑组槽」—— 即用户要删掉的那条
 *   「token 被子弹打掉也算收」。令牌换成自建 VM 之后**根本没有死亡事件**（VM 不会自己死，只有
 *   我们主动删它）⇒ 环里永远不会有人往里放东西。留着就是**死代码**；而且 `-Wunused-function`
 *   会当场把登记函数判出来 —— 实装时正是那条告警把这一整块暴露出来的（自证见报告）。
 * ⚠ 令牌的**来源**不受影响：小怪击破 → `on_enemy_death` 登记**落点**（`pend_drop` / `pend_drop_x/y`）
 *   → 帧末 `on_tick_2` ②b 建 VM。那一条路**一个字没改**（用户明确：来源不要动）。 */
/* ★❓ 自定（spec §9 #12「令牌掉落普查」：TH18 侧「谁产令牌、产多少」没有一手 ⇒ 自定）：
 * ★ 2026-09-12 **触发点已按一手改成「小怪被击破时掉」**（见 `on_enemy_death` ③：
 * 一手的令牌来自敌人 ECL 掉落表 `dropExtra(10..12,n)`，op 407 handler `th12:0x4179cc`）。
 * ★★ 2026-09-12（换池）**这一段不变** —— 令牌的**来源**与载体是两件事：
 *   来源仍是「小怪击破时登记、帧末生成」（`on_enemy_death` ③ → `on_tick_2` ②b），
 *   换掉的只是「登记之后用什么承载它」（真敌人壳 → 自建 VM）。
 * 下面三个数现在只剩**节流**一个作用（一手是「表里有就掉」，既没有概率也没有冷却）：
 *   `INTERVAL` = 两枚令牌之间的**最短**间隔；`RETRY` = 造失败（表满 / VM 建不出来）后的重试间隔；
 *   `FIRST` = 开局头 3 s 不掉落。⏳ 三个都是手感值，等实跑标定。 */
#define UFO_TOKEN_SPAWN_INTERVAL 300  /* 两枚令牌之间的最短间隔（300 游玩帧 = 5 s）*/
#define UFO_TOKEN_SPAWN_RETRY    120  /* 造失败（表满 / anm VM 建不出来）时的重试间隔 */
#define UFO_TOKEN_SPAWN_FIRST    180  /* 开局先等 3 s 再允许第一枚掉落 */

/* ★★ 令牌池的一只成员（2026-09-12 换池：**这就是那一枚 token 的全部状态**）-------------------
 * 一只 token = **一个** `ce_anm_spawn` 出来的 VM（`vm_id`）+ **我们自己的**位置与速度
 * （`ufo_tok_motion_t`，纯逻辑层算）。**没有实体、没有敌机字段、没有第二个 VM。**
 *
 * 逐字段交代（哪些是旧载体留下的、哪些是新搬进来的）：
 *   · `motion.x/y`  ← 以前读/写敌机的 `CE_ENEMY_POS_X/Y`（`enemy+0x12b4` 的 `abs_pos` 也是双写目标）
 *     ⇒ 换池后**唯一的坐标真相**（依据：用户拍板 + 真敌人壳的三条代价，`AUDIT.md` §BO-11 P3）。
 *   · `motion.vx/vy/age` ← 以前在文件静态 `s_tok_motion[16]` 里按槽号存（因为私状态放不下）；
 *     现在池本来就在静态区 ⇒ **并进同一个结构体**，少一层「槽号 → 运动表」的间接。
 *   · `ttl`  = 存活上限（★❓ 自定 `UFO_TOKEN_TTL_FRAMES`）。**换池后仍然需要**：VM 不会自己消失
 *     （一手是靠「出界即删」回收的，我们的 VM 出界那一步由 `ufo_tok_motion_step` 判、这里兜底）。
 *   · `vor`  = 保留名（`enemy_id` 的**替代**）：换池前它是「这只令牌是哪个敌人」⇒ 用来找实体、
 *     认死亡事件。**现在没有实体了** ⇒ 这个字段的语义降级为「本局第几个令牌」（= `motion_init` 用的
 *     `serial`，一手 `ItemManager+0x666fe0` 那个计数），日志对账用；**不参与任何判定**。
 *   · `anm_id` = 唯一的 VM 句柄（0 = 空槽）。铁律：句柄非 0 ⇒ 必须有一条删除路径
 *     （`ufo_tok_pool_clear_all` / `ufo_tok_kill` / 每帧自愈）。
 *   ⚠ 下面四个 uint8 字段是**照抄旧结构体**的（那时为了压住 20 B 的槽宽）：`pentagon_hint`
 *     （够不够格画五边形框，判据在纯逻辑层 `ufo_pentagon_hint_for_color()`）、
 *     `rb_state` / `rb_cycle` / `near`（彩虹令牌三段中断 + 每 150 帧换色，一手 `th12:0x42630f`–`0x4264b7`）。
 *     ⚠ 只有**彩虹**令牌用后三个（一手把整段关在 `if (type == 13||14||15)` 里），非彩虹恒 0。 */
typedef struct {
    int32_t           anm_id;        /* ★ 唯一 VM 句柄（0 = 空槽）；以前是「贴图跟随 VM」，现在它**就是**令牌 */
    int32_t           color;         /* `UFO_COLOR_*`；`UFO_COLOR_NONE` ⇒ 槽空（分配器只看它）*/
    uint32_t          ttl;           /* 剩余存活帧（到 0 ⇒ 静默撤：删 VM + 回收槽）*/
    ufo_tok_motion_t  motion;        /* ★ 位置 + 速度 + 年龄（我们自己的字段，见上）*/
    uint32_t          vor;           /* 本局第几个令牌（= 一手那个计数的对账值；不参与判定）*/
    uint8_t           pentagon_hint; /* 这一枚此刻该不该画五边形框（纯逻辑层判据的结果）*/
    uint8_t           cycler;        /* ★ 2026-09-18（新增 2）：1 = **变色令牌**（一手 type 13/14/15）。
                                      * 变色令牌的 `color` 是它的**起始显示色**（红/蓝/绿之一），
                                      * 显示与入槽一律走 `ufo_tok_display_color()`；没有"彩虹色令牌"。 */
    uint8_t           rb_state;      /* 变色令牌的当前显示态 0/1/2（红/蓝/绿）*/
    uint8_t           rb_cycle;      /* 彩虹令牌的换色计时器（一手 `item+0x9a0`，0..150）*/
    uint8_t           near;          /* 上一帧在不在 60 px 圈内（一手 `item+0x9c4` 的闩）*/
} ufo_tok_t;

/* ★★ 令牌池（**文件级静态**，先例：117 `animal.c` 的 `s_tok[40]` / 113 `trance.c` 的 `s_spirit[128]`）----
 * 为什么在这里而不是卡私状态：实测 `UFO_TOKEN_CAP` × `sizeof(ufo_tok_t)` = 5 × 40 = **200 B**
 *   （+ 框句柄表 20 B），而换池后私状态只剩 8 B 余量 —— 用户硬约束是「**不要为了省字节去砍功能**」
 *   ⇒ 整块搬到这里（卡私状态因此**反而变小**，见文件头体积账）。
 * 为什么**可以**在这里：本卡是**单例**（`CE_CARD(112, …)`）⇒ 没有串台问题（与 `s_penta_id` / HUD 那几组
 *   句柄同一个理由）。⚠ 换池前作者担心「卡对象局中重建 ⇒ 静态表串台」—— 本池的**分配器只看 `color`**、
 *   每次 `on_ctor` 都 `ufo_tok_pool_clear_all()`（清句柄 + 清表），所以重建只会「先收干净再重来」。
 * 收尾（长持续效果的铁律）：`on_delete` / `on_run_reset` 两条路汇到 `ufo_withdraw_all()` →
 *   `ufo_tok_pool_clear_all()`；删卡那一刻卡的 tick 全停、**只有 `on_delete` 跑得到**。
 *   ★★ 2026-09-23 订正：**换关不再走这条路**（owner：「换面数的时候 token 清零」= 经典 bug）——
 *     换关只收本体（引擎已经在 stage-end 销毁了那只真敌人），令牌池与三格槽原样跨关。
 *     详见 `on_stage_start` 与 `s_stage_seen` 的注。 */
static struct {
    ufo_tok_t tok[UFO_TOKEN_CAP];   /* 紧凑数组：`[0, n)` 全是活的；swap-remove 保持紧凑 */
    int32_t   n;                    /* 池里活着几只（0..UFO_TOKEN_CAP）*/
    int32_t   dropped;              /* 池满而丢掉的枚数（记账；`on_tick_2` / `on_item_frame` 里打日志）*/
} s_tokpool;

/* ---- ★ 五边形框的**每个令牌槽一个** VM 句柄（`s_tokpool` 的同一个下标；同样是文件静态）------------
 * 为什么与令牌池一样放静态：一个框 = 一个独立 VM（原生也是：框的句柄在 `item+0x968`、令牌自己的 VM
 *   在另一格）⇒ 池深 × 4 B 又放不进私状态（第一版塞进 `ufo_tok_t` 时被文件尾的 `_Static_assert`
 *   当场拦下，clang 原话 `532 <= 496`）。
 * 收尾：`ufo_tok_pool_clear_all()` 按**下标**逐个 `ce_anm_delete`（只看句柄、不看 `color`）⇒ 删卡也收得掉。 */
static uint32_t s_penta_id[UFO_TOKEN_CAP];

/* ==================== ★★ 掉落物实体池（压缩包 / 整颗 Bomb；用户 2026-09-18）====================
 * 与令牌池同一套口径：**文件静态**句柄表（`on_ctor` 的 memset 清不掉它 ⇒ 必须显式收尾），
 * 一只实体一个 VM。纯逻辑（运动 / 结算）在 `ufo_core` 的 `ufo_drop_t` 一族。
 * 容量 4：一手场上最多也就「一次击破 1 件赏金 + 1 枚令牌 + 颜色加成」几件 ⇒ 4 足够宽裕。 */
#define UFO_DROP_POOL 4
/* 层：一手压缩包脚本自己写 `layer(14)`（`bullet.anm` script181）、整颗 Bomb 那支不写层（道具由引擎排）
 * ⇒ 我们统一给**世界层**（3–19 的游玩区）：压缩包 14（= 一手原值）、Bomb 13（与令牌同层，观感一致）。 */
#define UFO_DROP_ANM_LAYER       14
#define UFO_DROP_BOMB_ANM_LAYER  13
/* ★★★ 2026-09-23（owner 第 5 条 / 新口径）：满 B 改成**原生道具**（一手逐条见 `ufo_drop_spawn`）。
 *   · `UFO_DROP_NATIVE_BOMB_TYPE` = **7**：一手 `th18:0x4464d5`（收集 handler）—— 加一颗弹 +
 *     满则清碎片 + SE `0x2e` + 刷 HUD；**原版自己就用它掉整颗 B**（`th18:0x40d2e0` 辉夜卡 `push 7`）。
 *     ⚠ 原生 type 15 会被 spawn **改写成 6**（`0x446ff7 cmovne`）、≥0x14 一律返回 0 ⇒ 只用 7。
 *   · 角度/速度 = 一手普通掉落实参（`0x43061e` 那条 `Enemy__drop_items_and_notify_cards`：
 *     角度恒 `−1.5707964`（−π/2）、速度恒 `2.2`）⇒ 与 B碎 掉落**逐字相同** = owner 要的"完全复刻"。*/
#define UFO_DROP_NATIVE_BOMB_TYPE   7u
#define UFO_DROP_NATIVE_BOMB_ANGLE  (-1.5707964f)
#define UFO_DROP_NATIVE_BOMB_SPEED  2.2f
/* 原生道具的只读字段（`zItem`；结构体一手 = `local/vendor/th-re-data` 的 `zItem`）：
 *   `+0xc2c` = 世界坐标 float3（一手：spawn 支 `th18:0x447090` 把 `lea ebx,[esi+0xc2c]` 当坐标指针
 *   交给 `0x488f70` 写副 VM 的 `entity_pos`）；`+0xc74` = state（0 = 池里已回收/不画）。*/
#define UFO_ITEM_POS        0xc2c
#define UFO_ITEM_STATE      0xc74
#define UFO_ITEM_STATE_OFF  0
/* ★★★ 2026-09-24（owner）：那颗「**五角星**」的来历查清了 —— 它**不是**我们的贴图，而是
 *   **引擎自己画的原生整 B 道具图标**（`bullet/item.png` sprite512，type 7 的 VM1 脚本 118 画的）
 *   —— ZUN 从早期作品层垒下来的**整 B 美术**（owner 原话「其他作品确实是用那个作为一颗 B 的」）。
 *   本卡的外观走「**卡面 + 原生光晕**」那一套（脚本 301/331/345）⇒ 把**引擎那颗图标藏起来**：
 *   每个游玩帧把道具 VM1 的 `color_1`（`item+0x534`，一手 `th18:0x447115` 出生写 0xffffffff）的
 *   **alpha 字节写成 0**（`0x00ffffff`）。
 *   ⚠ 为什么写在 **tick**（`ufo_drops_tick`，位次在道具 body **之前**）而不是 draw：这样整帧都是透明的；
 *     而引擎的「减速压暗」那支只写 `+0x535/+0x536`（G/R 两个字节，`0x445d42 mov WORD [edi+0x535],0xffa0`）
 *     ⇒ **碰不到 alpha 字节**，我们写的 0 不会被覆盖 ✓。
 *   ⇒ 想让「原生那颗星星」当整 B 外观（而不是我们的卡面）：把 `UFO_ITEM_HIDE_ICON` 改 0 即可。*/
/* ★★★ 2026-09-24 第 21 轮**修 bug**（owner：「**依旧画了老满B的五角星**」）：
 *   上一版写的是 `item+0x534` —— **那不是颜色**！一手逐指令：道具构造器 `th18:0x447115` 把
 *   `[item+0x534] = 0xffffffff`（道具自己的字段），而**图标 VM 的颜色**在
 *   `th18:0x445d0a`–`0x445d12`：`mov [item+0xc28], eax`（新 VM id）+ `mov [eax+0x524], 0xffa0ffff`
 *   ⇒ 颜色在 **VM + 0x524**（= `engine.h` 的 `CE_ANM_VM_COLOR1`），VM id 在 `item+0xc28`。
 *   ⇒ 现在的藏法 = 每帧 `ce_anm_get_vm(item+0xc28)` 取到 VM，再把 `vm+0x524` 的 **alpha 写 0**。 */
#define UFO_ITEM_HIDE_ICON  1
#define UFO_DROP_ANM_LOG_EVERY 60   /* 满 B 的诊断行节奏（0 = 关）；验证摇摆/隐藏星用 */
/* 引擎读数（照 110 `mof.c` 的同名宏）：满火力判据 = `CURRENT_POWER >= MAX_POWER` */
#define CE_MAX_POWER_VAL()       (*(int32_t *)CE_ADDR_MAX_POWER)
#define CE_MONEY_BASE_VAL()      (*(int32_t *)CE_ADDR_MONEY_BASE)
#define CE_DIFFICULTY_VAL()      (*(int32_t *)CE_ADDR_DIFFICULTY)   /* 0 EASY / 1..3 / 4 EXTRA */
typedef struct {
    ufo_drop_t core;                            /* 纯逻辑（坐标 / 速度 / 携带的账）*/
    uint32_t   anm_id;                          /* 它的 VM（0 = 空槽）*/
    uint8_t   *item;                            /* ★ 2026-09-23：**原生道具槽指针**（只有整颗 B 那一支有；
                                                 * = `CE_FN_SPAWN_ITEM` 的返回值，`zItem*`。它的运动/收集
                                                 * 全由引擎的道具池管 ⇒ 我们只读它的坐标、不写任何字段）*/
    uint8_t    logged;                          /* 「起来了 / 被收了」各只报一次 */
} ufo_drop_slot_t;
static ufo_drop_slot_t s_drops[UFO_DROP_POOL];

/* ★★ 2026-09-23（owner：「一面进二面，**换面数的时候 token（飞碟）清零**」）——
 *   引擎在**每关开始**会对卡组里每张卡再调一次 ctor（一手实跑 2026-09-04：`on_stage_start` 后紧跟 ctor，
 *   见 `sdk.h` §ctor 的注）；而本卡的 `on_ctor` 会 `memset` 卡私状态 + `ufo_tok_pool_clear_all()`
 *   ⇒ 三格令牌的账与场上令牌**都被清零** = owner 说的那个"经典 bug"。
 *   ⇒ 本文件记住"上一个见过的关卡号"，在 ctor 里就能把「**换关重入**」与「**刚拿到卡**」分开：
 *     关卡号变了 ⇒ 换关重入 ⇒ **令牌池与槽原样带过去**；关卡号没变 ⇒ 真的新拿卡 ⇒ 照旧全清。
 *   ⚠ 一手对照：TH12 里"每关重来"的是**敌人/弹幕**（引擎的 stage-end 清场），而三格令牌的账
 *     （`COLLECTED_UFOS` 一族）是**整局**的账 —— 它**唯一**的一手清除点是「**UFO 结束**那一刻」
 *     （`th12:0x44a326`，本卡 `ufo_teardown` 已照做）。把"换关时引擎强制销毁 UFO 敌机"也当成
 *     "UFO 正常结束"去清槽，就是 owner 反复报的那条。*/
#define UFO_STAGE_SEEN_NONE 0xffffffffu
static uint32_t s_stage_seen = UFO_STAGE_SEEN_NONE;

/* ==================== ★★ 拉拽速度是**累计量**（一手；2026-09-18 实跑反馈修）====================
 * 一手（`th12:0x426879` 段，逐条）：
 *     item+0x9bc  +=  (UFO.timer / 600) × 0.05 + 0.02      ← ★ **每帧往上加**（不是"每帧走这么多"）
 *     if (item+0x9bc > 6.0) item+0x9bc = 6.0
 *     位移 = 速度 × GAME_SPEED（引擎自己的道具物理用这个**累计速度**）
 * ⇒ 道具越拉越快：第 N 帧的速度 ≈ Σ_{k≤N}(k/600×0.05 + 0.02) —— 第 100 帧就有 **≈2.4 单位/帧**。
 * ★ 我们原来把它当成"**每帧的位移**"（`ufo_drag_step(..., ufo_drag_speed(frames))`）⇒ 第 100 帧只有
 *   0.028 单位/帧 ⇒ **慢了几十倍**，实跑就是用户报的「吸收速度极慢、可吸收道具定住了一样」。
 * ⇒ 修法：按**道具槽**记一个累计速度（同时被拉的极少 ⇒ 一张小表就够）。
 * ⚠ 表以**槽地址 + type** 为键：槽被回收后再发新道具时 type 多半不同 ⇒ 命中会重置（防串味）。 */
#define UFO_PULL_MAX 16
typedef struct {
    uint8_t *item;          /* 道具槽地址（NULL = 空）*/
    uint32_t type;          /* 该槽当时的 type（防"槽被复用"串到新道具）*/
    int32_t  spd_milli;     /* 累计速度（×1000）*/
} ufo_pull_t;
static ufo_pull_t s_pull[UFO_PULL_MAX];

/* 取该道具本帧的**累计速度**（不存在就先建一条）。`ufo_frames` = UFO 的存活帧数（一手公式里的 timer）。*/
static float ufo_pull_speed(uint8_t *it, uint32_t type, uint32_t ufo_frames)
{
    int i, free_i = -1;
#if UFO_DRAG_ACCEL_ENABLE
    int32_t inc;
#endif

    for (i = 0; i < UFO_PULL_MAX; ++i) {
        if (s_pull[i].item == it) {
            if (s_pull[i].type != type) { s_pull[i].spd_milli = 0; s_pull[i].type = type; }  /* 槽被复用 */
            break;
        }
        if (!s_pull[i].item && free_i < 0) free_i = i;
    }
    if (i == UFO_PULL_MAX) {                          /* 没这条 ⇒ 用空位（满了就复用第 0 条）*/
        i = (free_i >= 0) ? free_i : 0;
        s_pull[i].item = it;
        s_pull[i].type = type;
        s_pull[i].spd_milli = 0;
    }
#if !UFO_DRAG_ACCEL_ENABLE
    /* ★★★ 2026-09-24（owner：「是**匀速缓慢吸收**」）—— 常量速度，见 `ufo_core.h` 的常量注
     *   （一手 TH18 那条匀速吸附 = `player+0x47988 / 3` = 1.667 单位/帧；本卡取 1.5）。
     *   这一支不再需要那张逐颗增量表，但表与 `ufo_pull_forget*` 全部保留
     *   （回退 = `UFO_DRAG_ACCEL_ENABLE` 改回 1，一行）。*/
    (void)it; (void)type; (void)ufo_frames;
    return ufo_drag_speed_const();
#else
    /* 一手：`+ (timer/600)×0.05 + 0.02`（百万分比整数算，避免浮点误差累积）*/
    inc = (int32_t)((int64_t)ufo_frames * 50 / 600) + 20;       /* 0.05 → 50‰、0.02 → 20‰ */
    s_pull[i].spd_milli += inc;
    if (s_pull[i].spd_milli > 6000) s_pull[i].spd_milli = 6000;  /* 一手上限 6.0 */
    return (float)s_pull[i].spd_milli / 1000.0f;
#endif
}

/* 道具被吸掉 / 撤掉时把它的累计量丢掉（槽要被回收了）。*/
static void ufo_pull_forget(uint8_t *it)
{
    int i;
    if (!it) return;
    for (i = 0; i < UFO_PULL_MAX; ++i)
        if (s_pull[i].item == it) { s_pull[i].item = 0; s_pull[i].type = 0; s_pull[i].spd_milli = 0; }
}

static void ufo_pull_forget_all(void)
{
    memset(s_pull, 0, sizeof s_pull);
}

/* ★★ P2（用户 2026-09-18「随机吧」）的随机种子：**文件静态**（照 110 `mof.c` 的 `s_scatter_seed` 形状）。
 * 播种点 = `on_ctor`（用「关卡号 ⊕ 帧钟」混一次）⇒ 同一局内不会重复、重开一局会变。
 * ⚠ 用**我们自己的 LCG**（`ufo_rand_next`，纯整数）：引擎那两支 RNG（`0x402810`/`0x4027d0`）
 *   走 x87 ⇒ 会踩 `make dllx87` 的「0 条 x87」硬门（110 也是这个理由自建）。 */
static uint32_t s_rng_seed = 0x9e3779b9u;

/* ⚠ 全静态（无堆）：卡单例 + 无指针 ⇒ 与 108 的 12 KB 使魔表不是一回事，这里不需要第二个静态表。 */

/* ---- HUD 的 VM 句柄（★ 放**文件静态**而不是卡状态：`on_ctor` 会 `memset` 私有状态，
 *      而引擎可能在别处摧毁这些 UI VM ⇒ 句柄必须活得比一次 `memset` 长，靠每帧 `ufo_hud_ensure()`
 *      自愈（照 `signal.c` 的同一支写法）。**卡单例**（`CE_CARD(112, …)`）⇒ 静态没有串台问题。 */
static uint32_t s_slot_id[UFO_SLOT_COUNT];   /* 三格令牌槽（脚本 302 ← TH12 front.anm script80）*/
static uint32_t s_banner_id;   /* ★ #17 槽底毛笔横幅（ability.anm script315）的句柄 */
static int32_t  s_slot_state[UFO_SLOT_COUNT];/* 每格**当前显示的四态**（0 空 / 1 红 / 2 蓝 / 3 绿）；
                                              * −1 = 还没上过色（首帧会发一次中断）。改态才发中断 ⇒ 幂等。*/
static uint8_t  s_slot_blink;                /* 当前这一帧用的是不是亮版那一组（一手 `FUN_00421690`）*/
static uint8_t  s_slot_blink_prev;           /* 上一帧的闪烁位（**离场沿**检测：一手在 UFO 结束时会重置三格）*/
static uint32_t s_slot_log_tick;             /* ★ 2026-09-23 诊断节奏：本函数自己的帧计数（别用 `s->play_frame`，见那条注释）*/
static uint8_t  s_timer_logged;              /* ★ #9 击破计时器：本次召唤的诊断行是否已经打过（每次召唤一行 + 每 60 帧一行）*/
static uint32_t s_flash_id[UFO_SLOT_COUNT];  /* ★ Δ7：每格**最近的**闪光 VM（脚本 303，31 帧自删）—— 存活期内每帧摆位 */
static uint8_t  s_flash_left[UFO_SLOT_COUNT];/* 该闪光的剩余寿命（防句柄陈旧：过期就丢句柄，不再碰它）*/

/* ★★ 2026-09-23（「槽位 UI 完全看不见」工单）：把槽 VM 的**实况**打一行 —— 只读、不写。
 * 为什么需要：原来那条 `ufo/slot:` 只打我们自己的记账值（state / blink），**VM 真正画在哪、
 * 多大、alpha 多少一概看不到** ⇒ 判不出「根本没建 VM」「VM 建了但画在屏外」「scale 是 0」
 * 这三种完全不同的病。
 * ★★★ 2026-09-23 第二轮（「槽位只有 1 个」工单）**订正了字段**：第一版把脚本 `pos` 当成 `+0x2c`，
 *   那是**差 4 字节**的读法 —— `+0x2c` 是 `instr_offset`（脚本指令游标，恒 0），所以旧日志里那句
 *   `pos (0, 296)` 其实是 `(instr_offset=0, pos.x=296)`：**y 从来没被打出来过**，而"pos.x"那一列
 *   三格全是 296（= 脚本初始 `pos(296,912)`）⇒ 三格**叠在同一个点上**，正是用户看到的「只有 1 个」。
 *   一手（两处独立，见 `engine.h` 的 `CE_ANM_VM_*` 注）：`draw_vm 0x48171f` 的
 *   `x = [vm+0x30] + [vm+0x5f0] + [vm+0x4e4]` + ExpHP `zAnmVmPrefix`（`0x30 pos` / `0x4e4 __pos_2`）。
 * ⇒ 这一版把**三项 + 和**全打出来，"画在表面哪一点"一眼可算，不再靠猜。 */
static void ufo_hud_vm_log(int i, const char *tag, uint32_t id)
{
    const uint8_t *vm = id ? (const uint8_t *)ce_anm_get_vm(id) : 0;
    const float *sp, *p2, *ep;

    if (!vm) { ce_log("ufo/slot.vm %s: (no VM; id %u)", tag, id); return; }
    sp = (const float *)(vm + CE_ANM_VM_SCRIPT_POS);        /* +0x30 脚本 pos（pos()/posTime() 写的）*/
    p2 = (const float *)(vm + CE_ANM_VM_POS_2);             /* +0x4e4 第二份 pos */
    ep = (const float *)(vm + CE_ANM_VM_POS);               /* +0x5f0 实体坐标（C 侧摆位用这个）*/
    ce_log("ufo/slot.vm %s id %u script %d instr %d: draw = pos(0x30)(%.0f,%.0f) + pos2(0x4e4)(%.0f,%.0f) "
           "+ ent(0x5f0)(%.0f,%.0f) = 表面 (%.0f,%.0f) [本格槽心应 (%.0f,%.0f)]; "
           "sprite %d scale %.2f alpha %u layer %d flags_lo %08x",
           tag, id, (int)*(const int32_t *)(vm + CE_ANM_VM_SCRIPT_ID),
           (int)*(const int32_t *)(vm + 0x2c) /* instr_offset：脚本游标（诊断脚本停在哪）*/,
           (double)sp[0], (double)sp[1], (double)p2[0], (double)p2[1], (double)ep[0], (double)ep[1],
           (double)(sp[0] + p2[0] + ep[0]), (double)(sp[1] + p2[1] + ep[1]),
           (double)k_ufo_slot_x[i], (double)UFO_HUD_SLOT_Y,
           (int)*(const int32_t *)(vm + CE_ANM_VM_SPRITE_ID),
           (double)*(const float *)(vm + CE_ANM_VM_SCALE),
           (unsigned)((*(const uint32_t *)(vm + CE_ANM_VM_COLOR1) >> 24) & 0xffu),
           (int)*(const int32_t *)(vm + CE_ANM_VM_LAYER),
           (unsigned)*(const uint32_t *)(vm + CE_ANM_VM_FLAGS_LO));
}

/* ---- ★ A7（2026-09-12）：五边形框的**每个令牌槽一个** VM 句柄，同样放**文件静态** ----------------
 * 为什么不像别的跟随 VM 那样放卡私状态（`ufo_tok_t.anm_id` 就在那里）：**放不下** ——
 *   `sizeof(ufo_card_t)` 现在 **468 B**，上限 `CE_STATE_BYTES − CE_STATE_RESERVED` = **496 B**
 *   只剩 28 B，而 16 个令牌槽各要一个句柄 = 64 B。第一版正是塞进 `ufo_tok_t` 的，当场被
 *   本文件尾那条 `_Static_assert` 拦下（clang 的原话：`532 <= 496`）⇒ 换到这里，**零私状态开销**。
 * 代价与 HUD 那一组**完全相同**，所以处理方式也照抄：
 *   `on_ctor` 的 `memset` 清不掉它 ⇒ 必须有**显式**收尾路径。这一条已经就位 ——
 *   `ufo_tok_vm_kill_all()` 是**只看句柄、不看 `color`** 的（帧内还配合 `ce_anm_get_vm` 自愈），
 *   而 `on_load` 的防御支 / `ufo_withdraw_all`（`on_stage_start` / `on_run_reset` / `on_delete` 都汇到它）
 *   都调它 ⇒ 句柄不会漏。⚠ 与 HUD 的区别：框是**世界层** VM（跟着令牌在游玩区动），不是 UI VM。 */
static uint32_t s_penta_id[UFO_TOKEN_CAP];

/* ★★ 卡私状态（`ce_state()` 指的那块 496 B）---------------------------------------------------------
 * ★ 2026-09-12 换池：**令牌池整个移出这里**（`s_tokpool`），所以本结构体现在只剩三块：
 *   凑组槽 / 本体生命周期 / 帧计数与登记位。**没有 token 表、没有 token 坐标、没有 token 句柄。**
 * 三块令牌相关状态**刻意留在这里**（它们不是「每只 token 的状态」，是「卡的账」）：
 *   · `pend_drop*` —— 死亡回调里登记的**落点**（回调里禁止建 VM，`DEATH` §2.4）⇒ 帧末建。一格够：
 *     节流保证同一次 sweep 最多落一枚。⚠ 换池后这一步从「建敌人」变成「建自己的 VM」，
 *     但**纪律不变**：仍然只在**帧末**（`on_tick_2`）建 VM。**这就是「令牌的来源」那条路。**
 *   · `token_serial` —— 本局第几个令牌（**运动角度的奇偶** + 日志对账，一手 `ItemManager+0x666fe0`）。
 *     ★ 2026-09-18：颜色**不再**用它（P2 改随机，种子是文件静态的 `s_rng_seed`）。 */
typedef struct {
    ufo_group_t grp;                                   /* 3 个令牌槽（spec §3.3 末行）*/
    ufo_ufo_t   u;                                     /* UFO 生命周期 / 两个计数 / fill */
    float       last_x, last_y;                        /* 最近一次已知的 UFO 位置（击破时由死亡事件给；
                                                        * ⚠ 不是缓存指针 ⇒ 合规，`DEATH` §2.4 只禁指针）*/
    uint32_t    play_frame;                            /* 游玩帧计数 */
    uint32_t    next_token_at;                         /* 下一个令牌最早能在哪一帧出现（节流）*/
    uint32_t    token_serial;                          /* 本局第几个令牌（**运动角度奇偶** + 对账；颜色已改随机）*/
    float       pend_drop_x, pend_drop_y;              /* 落点 = 那只敌人的死亡坐标 */
    /* ★ **本体**的跟随 VM 句柄（`ability.anm` 164..167 之一）。换池**没动本体**（它仍是 ECL 真敌机）。
     * 生命周期与 `u` 绑死：`ufo_try_summon()` 建起来、`ufo_teardown()` / `ufo_withdraw_all()` 收掉
     * （⚠ 删卡走 `on_delete`，那一刻 tick 全停 ⇒ 必须在 `ufo_withdraw_all()` 里收，不能指望每帧跟随时收）。*/
    uint32_t    body_anm_id;
    uint32_t    summon_fx_for;          /* ★ 召唤横幅（脚本 324）已经为哪一只 UFO（`s->u.ufo_id`）起过
                                         * —— 每只 UFO 只起一次。原字段名 `ring_vm_for` 是紫环时代留下的：
                                         * owner 实跑第 ③ 条之后，召唤表现换成一手的 `front.anm` script82。*/
    int32_t     panel_last;             /* ★ #22：上一次看到的吸收数（变了就重弹记录面板）*/
    uint8_t     pend_drop;                             /* 1 = 有令牌等着落地（来源那条路）*/
    uint8_t     pend_drop_n;                           /* ★ 2026-09-24：这一批要落**几枚**（宿主表给：妖精 1 / 大蝴蝶 2 / boss 2）*/
    int32_t     pend_drop_color;                       /* ★ 2026-09-24：固定色（`UFO_COLOR_*`；`UFO_COLOR_NONE` = 走旧的随机/变色那条）*/
    uint8_t     pending_break;                         /* 1 = 帧末要按「击破」收尾开奖 */
    uint8_t     logged_no_hud;                         /* 保留名：HUD「画起来了」日志只报一次（第三版起 = 原版槽 VM 到位）*/
    uint8_t     logged_lifetime;                       /* 存活上限日志每局只报一次 */
    uint8_t     tokens_leave_logged;                   /* 2026-09-23 owner #9: dialogue -> leave flight, log once */
    uint8_t     pentagon_any;                          /* 本帧有没有令牌够格画五边形框（= 场上真有框）*/
    uint8_t     pentagon_logged;                       /* 五边形框条件第一次成立时只报一条日志 */
    /* —— 统计（只用于日志，不参与判定）—— */
    uint32_t    spawned_tokens;
    uint32_t    taken_tokens;                          /* 被自机吃掉的 */
    uint32_t    lost_tokens;                           /* 超时 / 出界 / VM 被引擎销毁 */
    uint32_t    ufos_spawned;
    uint32_t    ufos_broken;
    uint32_t    ufos_vanished;
    uint32_t    items_pulled;
    uint32_t    rewards_spit;
    uint32_t    bursts;                                /* 吸满吐奖的次数 */
    uint32_t    dropped_by_ufo;                        /* UFO 击破/吸满返还的令牌数（一手：每色 1 枚）*/
    uint32_t    dropped_tokens;                        /* 「两同一异」丢最旧丢掉的最旧令牌数（一手 KEEP2）*/
    uint32_t    failed_summons;                        /* 本体 `ce_spawn_enemy` 建不出来 */
    /* —— ★ P1（2026-09-18 用户裁决「按一手」）：满槽补偿的账 ——
     * 一手：UFO 在场期间槽是满的 ⇒ 再收令牌只给 `[0x4b0c78] += 100000`（PIV）+ 飘字 1000。
     * ⚠ TH18 **没有 PIV 这个状态量** ⇒ 载体见 `ufo_slot_full_bonus()` 的注（本轮默认给分）。*/
    uint32_t    slot_full_tokens;                      /* 槽满期间被消耗的令牌数 */
    /* —— ★★ 用户 2026-09-18（`NEXT.md` §1x.9）：被飞碟吸走的**信仰点**的账 ——
     * 口径：**每颗 +1 计数**（与道具同；用户「按 +1 实现」），归 **P 通道**（中信仰点是 P 点转化的、
     * 大信仰点对红 UFO 等价于 P 点）；击破时这些账由「压缩包」带回、收集时调 `faith_spawn()` 还回去。 */
    uint32_t    faith_pulled_big;                      /* 吸走的大信仰点数 */
    uint32_t    faith_pulled_mid;                      /* 吸走的中信仰点数 */
    int32_t     faith_pulled_mask;                     /* 这些点原属谁的槽（110/111；按位或）*/
    int32_t     faith_pulled_big_sum;                  /* 大点 value 之和（还回去时按均值还原）*/
    int32_t     faith_pulled_mid_sum;                  /* 中点 value 之和 */
    uint32_t    drops_spawned;                         /* 诊断：起了几个掉落物实体 */
    int64_t     slot_full_piv;                         /* 一手的原始加值累计（×100 定点；日志/对账用）*/
    /* —— ★ ④（2026-09-18）：Boss / 对话两条离场路的闩 —— */
    int32_t     dbg_key_prev;                          /* ★ 调试键（空格）上一帧按着没有 —— 边沿检测（照 117 同款）*/
    uint8_t     boss_leave_logged;                     /* 「Boss 在场 ⇒ 静默撤」只报一次 */
    uint8_t     dialogue_break_logged;                 /* 「对话 ⇒ 原地自爆发奖」只报一次 */
} ufo_card_t;

/* ---- ★ ④：Boss 在场判据（一手 `th12:0x449f20`：`bosses[0..7]` 任一非空 ⇒ 有 Boss）---------
 * TH18 的同位表 = `EnemyManager + CE_EM_BOSS_IDS + 4*slot`（存的是 enemy_id；一手写入点
 * `th18:0x42d490`，由 ECL `setBoss` 在 `th18:0x433c08` 调）—— 与 110 `mof.c` 的 `mof_boss_alive()`
 * 同一个判据（那一条已一手对账过，见 `mof.c` 的 `CE_EM_BOSS_IDS` 注）。
 * ⚠ 一手只判 `bosses[0]`（TH12 的 `FUN_00449f20` 扫 8 格，任一非空即真）⇒ 这里也扫全表，
 *   免得 TH18 用别的槽号放 Boss（道中 Boss 与关底 Boss 在一手里走同一个槽 = 两者一致）。 */
#define UFO_BOSS_SLOTS 8
static int ufo_boss_alive(void)
{
    uint8_t *em = CE_ENEMY_MGR();
    int i;
    if (!em) return 0;
    for (i = 0; i < UFO_BOSS_SLOTS; ++i)
        if (*(const int32_t *)(em + CE_EM_BOSS_IDS + 4 * i) != 0) return 1;
    return 0;
}

/* ★ ④：Boss 在场时 UFO 的「离场延迟」。一手 = ECL 轮询周期（`$B`/`$C` 两圈 60 帧 + `goto @60`
 * + `wait(1)`）⇒ 首轮轮询落在召唤后 **≈301 帧**（若 `moveCircleRelTime(60,…)` 阻塞脚本则 ≈361）
 * ⇒ 本卡取 **301**（🟡 两个读法的下界；改这一个常量即可对齐另一种读法）。 */
#define UFO_BOSS_LEAVE_FRAMES 301

/* ============================== 小工具 ============================== */
static uint8_t *ufo_enemy_by_id(uint32_t id)
{
    uint8_t *em = CE_ENEMY_MGR();
    return em && id ? ce_enemy_by_id(em, (int32_t)id) : 0;
}

/* 自机坐标（SDK 帮手；指针为空时返回 0）。 */
static void ufo_player_xy(float *px, float *py)
{
    const float *p = ce_player_pos();
    *px = p ? p[0] : 0.0f;
    *py = p ? p[1] : 0.0f;
}

/* ⚠ 这里删掉了一个 `ufo_hash_unit()`（确定性散列，用于**占位期按节流自己撒令牌的落点**）：
 * 令牌来源已按一手改成「小怪击破时掉」（落点 = 那只敌人的死亡坐标）⇒ 散列不再需要。
 * 留着它只会被 `-Wall -Wextra -Werror` 判「unused function」（留此一行说明它去哪了）。 */

/* 关卡号（填充率公式的入参）。
 * ★❓ 自定（spec §4：逐关阈值是**为 TH12 道具密度**算的，TH18 要重估）⇒ 现在取 **1**（= 一手公式的
 * 第 1 关增量 0.030），这是「照抄一手同一格」最保守的取法，不是本机关卡号。 ⏳ 待实跑标定。 */
#define UFO_PLACEHOLDER_STAGE 1

/* ==================== ★★ §3.2 令牌池：建 / 撤 / 找（2026-09-12 换池，自建 VM）====================
 * 一只 token = `s_tokpool.tok[i]` 的一条 + **它的那个 anm VM**。池是**紧凑数组**（`[0, n)` 全是活的），
 * 回收用 swap-remove（照 113 `trance.c` 的 `spirit_remove`）：槽号会变，所以**任何持有下标的循环
 * 都要用「不 ++i」的写法重跑当前位置**（池很小，最多 5 条）。 */
static ufo_tok_t *ufo_tok_at(int i)
{
    return (i >= 0 && i < s_tokpool.n) ? &s_tokpool.tok[i] : 0;
}

/* 池里还活着几只（= `n`）。分配器的**唯一**判据是 `color`：`UFO_COLOR_NONE` ⇒ 空。
 * ⚠ 上限判据走纯逻辑层的 `ufo_token_pool_room()`（单一真源），越界写表 = 内存写坏。 */
static int ufo_tok_live_count(void)
{
    return s_tokpool.n;
}

/* swap-remove：把尾元素换到 i（**不 ++i**，尾元素也要过筛）。`s_penta_id` 跟着一起搬
 * —— 框的句柄是**按下标**存的，不搬就会把 A 的框留给 B（框的颜色/寿命全错）。 */
static void ufo_tok_remove(int i)
{
    s_tokpool.tok[i] = s_tokpool.tok[--s_tokpool.n];
    s_penta_id[i] = s_penta_id[s_tokpool.n];
    s_penta_id[s_tokpool.n] = 0;
}

/* 收掉**某个令牌槽**的**五边形框 VM**（句柄清零 ⇒ 幂等）。★ 2026-09-12（A7）新增。
 * 一手对应物 = 原生条件不成立那一支 `push [edi+0x968]; call 0x461a70` + `mov [edi+0x968],0`
 *（`AnmManager__delete_vm`：只**标删**，真正的析构由 AnmManager 自己的 update 走 ⇒ 在死亡栈上叫也安全）。*/
static void ufo_tok_penta_kill(int slot)
{
    if (slot < 0 || slot >= UFO_TOKEN_CAP) return;
    if (s_penta_id[slot]) { ce_anm_delete(s_penta_id[slot]); s_penta_id[slot] = 0; }
}

/* 收掉一枚令牌的 **VM**（换池后这是它**唯一**的句柄：画面就是它本体，没有第二个 VM）。
 * ★ 为什么要与框分开：彩虹令牌每 150 帧换色时走「删旧 VM + 起新 VM」（`ufo_tok_rainbow_recolor`），
 *   而原生换色只重指**令牌自己的那只 VM**（`0x4061e0(vm = item, …)`），**不动**框那只
 *   （框在 `item+0x968`，是另一个 VM）。⇒ 换色不该把框也删掉 —— 否则每 150 帧框会重新
 *   `scaleTime(10,4,20,20)` 胀一次（原生没有这个动作）。 */
static void ufo_tok_art_vm_kill(ufo_tok_t *t)
{
    if (t->anm_id) { ce_anm_delete((uint32_t)t->anm_id); t->anm_id = 0; }
}

/* 收掉一个令牌槽的**两个 VM**（令牌自己 `anm_id` + 五边形框 `s_penta_id[slot]`；句柄清零 ⇒ 幂等；
 * `ce_anm_delete` 内部有 ANM 管理器门槛）。
 * ★ 铁律：令牌消失的**每一条路**都要过这里（被收 / 超时 / 出界 / VM 被引擎销毁 / 删卡），
 *   否则 VM 会留在场上画一个**幽灵令牌**（框也一样：一个没有令牌的空五边形）。 */
static void ufo_tok_vm_kill(int slot, ufo_tok_t *t)
{
    ufo_tok_art_vm_kill(t);
    ufo_tok_penta_kill(slot);
}

/* 收掉**整池**的令牌 VM + 五边形框 VM，并把池清空。⚠ **只看句柄、不看 `color`** ——
 * `on_ctor` 的 `memset`（卡私状态）与 `ufo_withdraw_all` 都需要这一支：句柄一旦被清掉，
 * 那些 VM 就永远留在场上了（删卡那一刻 tick 回调全停，没有任何「顺手收」的机会）。
 * ★ 换池后这里**更强**了：令牌的 VM 与框的句柄**都是文件静态**（不依赖卡私状态）⇒ 这一支
 *   在 `on_ctor` 的 `memset` **之后**调也照样收得掉（`s_penta_id` 旧版就已经是这个性质）。
 * ⚠ 池是「紧凑数组」⇒ 直接按 `[0, s_tokpool.n)` 走；`n` 之外的 `s_penta_id` 只可能残留 0。
 * ⚠ **不碰诊断计数**（与 113 `trance_spirit_clear_all` 同一口径：计数只进日志）。 */
static void ufo_tok_pool_clear_all(void)
{
    int i;
    for (i = 0; i < UFO_TOKEN_CAP; ++i) {
        if (i < s_tokpool.n && s_tokpool.tok[i].anm_id)
            ce_anm_delete((uint32_t)s_tokpool.tok[i].anm_id);
        s_tokpool.tok[i].anm_id = 0;
        s_tokpool.tok[i].color = UFO_COLOR_NONE;
        if (s_penta_id[i]) { ce_anm_delete(s_penta_id[i]); s_penta_id[i] = 0; }
    }
    s_tokpool.n = 0;
}

/* 撤一只令牌（下标）、回收槽位。**换池后这就是「撤实体」的全部**：
 *   · 以前这里要置敌机的「本帧删」位（`flags_lo` 位 25）让引擎 sweep 静默析构它 ——
 *     **现在没有敌人了** ⇒ 那一段连同 `UFO_FLAGS_DELETE_NOW` 的令牌用法一起删除。
 *   · 删一个 anm VM 走 `ce_anm_delete`（`0x488cf0` 只把 VM **标删**：`vm+0x538 |= 0x80`，
 *     一手见 `engine/anm/th18/01-vm-instantiate.md` §0 的表，真正的析构由 AnmManager 自己的 update 走）。
 *   ⚠ 换池后**任何调用点都不再是死亡栈**（令牌不产生死亡事件了）⇒ 「死亡栈上能不能删 VM」那条
 *     🟡 未验证的组合**整个消失了**（附带好处）。 */
static void ufo_tok_kill(int slot)
{
    ufo_tok_t *t = ufo_tok_at(slot);
    if (!t) return;
    ufo_tok_vm_kill(slot, t);            /* 令牌 VM + 五边形框，句柄都清零 */
    t->color = UFO_COLOR_NONE;           /* 分配器看它 ⇒ 之后这个槽就是空的（swap-remove 会覆盖它）*/
    t->ttl = 0;
    t->vor = 0;
    t->pentagon_hint = 0;
    t->rb_state = 0;
    t->near = 0;
    t->rb_cycle = 0;
    /* 运动状态一起清（swap-remove 之后尾元素会搬进来，别让它吃到上一枚的速度/年龄）*/
    t->motion.x = 0.0f;
    t->motion.y = 0.0f;
    t->motion.vx = 0.0f;
    t->motion.vy = 0.0f;
    t->motion.age = 0;
    ufo_tok_remove(slot);
}

/* 起一只令牌的 VM，并**立刻**摆到它的坐标上（世界层 VM 的实体坐标 (0,0,0) = 弹幕区上边框中点
 * ⇒ 不先摆位置的话会先在屏幕最上方闪一帧）。失败只报日志（画不出来 ≠ 卡坏）。
 * ★★ 换池（2026-09-12）之后**这就是令牌本体**：一只 token 一个 VM，位置来自 `t->motion.x/y`
 *   （我们自己的字段），**不再**从任何敌机字段抄。 */
static void ufo_tok_vm_spawn(ufo_tok_t *t)
{
    int script = ufo_tok_script(t->color, t->rb_state, t->cycler);
    uint32_t id = ce_anm_spawn(CE_ABILITY_ANM(), script, UFO_TOKEN_ANM_LAYER);
    if (!id) {
        ce_log("ufo: token VM spawn FAILED (color %d, script %d, layer %d) - the token exists in the "
               "pool but has nothing to draw with (1 token = 1 VM; script %d is a port of retail "
               "bullet.anm 175..180)", (int)t->color, script, UFO_TOKEN_ANM_LAYER, script);
        return;
    }
    t->anm_id = (int32_t)id;
    ce_anm_set_pos(id, t->motion.x, t->motion.y, 0.0f);
}

/* 每帧把 VM 摆到令牌自己的坐标上（**这是令牌唯一的写位置点**）。
 * ⚠ 句柄在别处被引擎销毁过（`ce_anm_get_vm` 找不到）⇒ **自愈**：丢掉句柄重新起一个
 *   （照 `ufo_hud_ensure()` 的同一支写法；令牌脚本不会自己 `delete()`，都是死循环）。
 * ⚠❓ 用户点名要求「自愈」：**`ce_anm_set_pos` / `ce_anm_interrupt` 返回 0 ⇒ 清句柄**
 *   （本函数里就是这一条：`ce_anm_set_pos` 返回 0 说明这个 id 已经查不到 VM 了 ⇒ 下一帧重建）。
 *   ⚠ `ce_anm_interrupt` 是 `void`（SDK 面），所以那条「返回 0」只落在 set_pos 这一支；
 *     中断发不出去的那一只（`anm_id` 已经为 0）在下一次 tick 会被这里重建。 */
static void ufo_tok_vm_follow(ufo_tok_t *t)
{
    if (!t->anm_id || !ce_anm_get_vm((uint32_t)t->anm_id)) {
        t->anm_id = 0;
        ufo_tok_vm_spawn(t);
        return;                                        /* spawn 里已经摆好位置 */
    }
    if (!ce_anm_set_pos((uint32_t)t->anm_id, t->motion.x, t->motion.y, 0.0f))
        t->anm_id = 0;                                 /* 句柄自愈：返回 0 ⇒ 下一帧重建 */
}

/* ============================ ★★ 五边形框（提示 UI）的建 / 跟随 / 收（2026-09-12 A7）============================
 * 一手（触发条件 + 六处建框点 + 删框点 + 层号）全部写在文件头那个常量块里，这里只有接法与理由。 */

/* 这一枚令牌（颜色 `color`）此刻该拿哪个标签建框 —— 返回 0 = **不该建框**。
 * 一手：标签 = **收了这一枚会得到的那只 UFO 的颜色**（`0x4265c3`/`0x426696`/`0x426766` 三条 case 的结果）：
 *   同色补全 ⇒ 红/蓝/绿 ⇒ 标签 `7/8/9`；三色各一 ⇒ 彩虹 ⇒ 标签 `10`。
 * 判据直接用纯逻辑层的两个**纯函数**（不改状态、已有单测）：
 *   `ufo_pentagon_hint_for_color()`（收了它成不成组）+ `ufo_group_peek_color()`（成了组会得到哪只）。
 * ⚠ `ufo_group_classify()` 内部会过 `ufo_color_norm()` ⇒ **金落在蓝那一格**（裁决 ①：金 = 蓝的调色版）。 */
static int ufo_pentagon_label_for_color(const ufo_group_t *g, int32_t color)
{
    ufo_group_t probe;
    int32_t made;

    if (!ufo_pentagon_hint_for_color(g, color)) return 0;
    probe = *g;
    if (!ufo_slot_set(&probe, color)) return 0;        /* 理论到不了（判据先过了一遍）*/
    made = ufo_group_peek_color(&probe);
    switch (made) {
    case UFO_COLOR_RED:     return UFO_PENTAGON_INT_RED;
    case UFO_COLOR_BLUE:    return UFO_PENTAGON_INT_BLUE;
    case UFO_COLOR_GREEN:   return UFO_PENTAGON_INT_GREEN;
    case UFO_COLOR_RAINBOW: return UFO_PENTAGON_INT_RAINBOW;
    default:                return 0;                  /* 不成组 / 坏色 ⇒ 不画（防御）*/
    }
}

/* 建框（**只在还没有框时建**：原版每帧只查 `cmp dword [edi+0x968],0 / jne`（`th12:0x4265d5`）
 * ⇒ 条件成立的后续帧既不重建、也不重发标签）；建完**立刻发标签**（原生六处建框点都是
 * `create_vm` 之后马上 `call 0x461970`，标签在 EBX）。
 * ⚠ 次序不必担心「中断跳掉脚本初始化」：`ce_anm_spawn` 走的那条引擎通路
 *   （`AnmLoaded__instantiate_vm_to_world_list_back` `0x405bf0`）**自己先跑一帧**，
 *   脚本开头的 `sprite(-1)/layer(12)/drawPolyBorder(1.0f,5)/scaleTime(...)` 那时已经落地，
 *   下一帧才轮到 pending 中断跳到标签段（TH12 侧同构：`0x454db9` 的 `call 0x455630`）。
 * ⚠ 本函数会**建 VM** ⇒ 只能从 tick / 帧末调（本卡的调用点 = `ufo_pentagon_hint_update()`，
 *   而它在 `ufo_tokens_tick()` 里 ⇒ 每游玩帧的门卫位次，不是死亡栈）。 */
static void ufo_tok_penta_spawn(int slot, const ufo_tok_t *t, int label)
{
    uint32_t id;

    if (slot < 0 || slot >= UFO_TOKEN_CAP) return;
    if (s_penta_id[slot] || !label || !CE_ABILITY_ANM() || !t) return;
    id = ce_anm_spawn(CE_ABILITY_ANM(), UFO_PENTAGON_ANM_SCRIPT, UFO_PENTAGON_ANM_LAYER);
    if (!id) {
        ce_log("ufo: pentagon hint-VM spawn FAILED (script %d, layer %d) - the frame around the "
               "completing token will NOT be drawn", UFO_PENTAGON_ANM_SCRIPT, UFO_PENTAGON_ANM_LAYER);
        return;
    }
    s_penta_id[slot] = id;
    ce_anm_set_pos(id, t->motion.x, t->motion.y, 0.0f);   /* ★ 换池：位置 = 我们自己的坐标字段 */
    /* 标签 = 框的颜色。SDK 面 = 既有的 `ce_anm_interrupt`（`AnmManager__interrupt_tree` `th18:0x488be0`，
     * 写 pending interrupt 并递归子树；TH12 侧是 `0x461970`）—— A8 的彩虹三段中断用的就是它，零新 SDK 面。 */
    ce_anm_interrupt(id, label);
}

/* 每帧跟随（框）：同一支写法（自愈 + 抄令牌自己的坐标）。
 * ⚠ 原生**不跟随**（框的位置在 `create_vm` 那一刻被拷进 VM，之后道具飘走它也不动）——
 *   我们的令牌会飞、会反弹（`ufo_tok_motion_step`）⇒ 不跟随就会「框留在原地、令牌飘走」。
 * ⚠ 句柄被引擎在别处销毁过 ⇒ 只**丢掉句柄**，重建交给紧接着的 `ufo_tok_penta_spawn()`
 *   （它带着本帧算出来的标签；这里自己重建就会造出一个「没有标签 = 停在白色」的框）。 */
static void ufo_tok_penta_follow(int slot, const ufo_tok_t *t)
{
    if (!s_penta_id[slot] || !ce_anm_get_vm(s_penta_id[slot])) {
        s_penta_id[slot] = 0;
        return;
    }
    if (!ce_anm_set_pos(s_penta_id[slot], t->motion.x, t->motion.y, 0.0f))
        s_penta_id[slot] = 0;                          /* 句柄自愈（与令牌那一支同款）*/
}

/* ---- ★ 彩虹令牌的换色（= TH12 的「重指脚本」，`th12:0x4061e0`；一手见常量块 ⑤）----
 * TH12 在**同一个 VM** 上 `AnmVm__wipe` + `load_script_into_vm`；SDK 没有这条通路
 * （`ce_anm_set_sprite` 只会被脚本下一帧的 `sprite()` 覆盖）⇒ 用**删旧 + 起新**等价达成，
 * 位置由 `ufo_tok_vm_spawn` 当场摆好（中间不产生「留在原点的 VM」）。
 * ⚠ 一手同时做了三件事，本函数逐条对应：
 *   ① `Timer__set_current(&t99c, 0)`（`0x426497`）⇒ `rb_cycle = 0`；
 *   ② `type = (type−0xc)%3 + 0xd`（`0x4264b6`）⇒ 态 +1 取模（13→14→15→13）；
 *   ③ 用 `type + 0xa5` 重新装载（`0x4264bc`）⇒ 换脚本重开。
 * ⚠ 一手还会播一个音：`th12:0x4264d0 fld [edi+0x96c]`（令牌自己的 x）+ `th12:0x4264d7 lea eax,[ebx+0x35]`
 *   + `th12:0x4264dd call 0x453e20` ⇒ TH12 `0x35` = **`se_changeitem.wav`**。
 *   ★★ 2026-09-20（P1 #th12-2）**已补上**：TH12 `0x35` 的那枚 wav 与 TH18 的 `se_changeitem.wav`
 *   **md5 相同** ⇒ TH18 的同一格 = `0x4e` = `UFO_SE_TOKEN_RECOLOR`，在**令牌自己的 x** 播
 *   （一手就是这么给的声像；旧版注释说"没有映射表所以不播"，本轮的 id 表推翻了那个前提）。
 * ★ 换池（2026-09-12）之后这一段**更贴一手了**：一手就是「在**同一个对象自己的** VM 上换脚本」，
 *   我们现在也是「在**同一只令牌自己的** VM 上换脚本」（删旧 + 起新是 SDK 面不足的等价物）。 */
static void ufo_tok_rainbow_recolor(ufo_tok_t *t)
{
    t->rb_state = (uint8_t)((t->rb_state + 1) % UFO_TOK_RB_STATES);
    t->rb_cycle = 0;
    /* 一手的第三个动作（同帧、在换脚本**之前**还是之后？）：
     * `0x4264dd` 在 `0x4264cb call 0x4061e0`（换脚本）**之后**、`0x4264e2` 之前
     * ⇒ 顺序 = ②改 type ③换脚本 ④播音。这里等价：先播音，再删旧/起新 VM
     * （VM 的先后不影响听感，只保证"换色那一帧一定响"）。*/
    ufo_se(UFO_SEV_RECOLOR, UFO_SE_TOKEN_RECOLOR, t->motion.x,
                   "一手 th12:0x4264d7 lea eax,[ebx+0x35] + 0x4264dd call 0x453e20, 声像 = 令牌 x");
    /* ★ 只收**令牌自己那只** VM（`ufo_tok_art_vm_kill`），**不碰五边形框**（A7）：原生换色重指的是
     *   令牌自己那只 VM，框在另一个槽（`item+0x968`）里、不受影响 ⇒ 框不该跟着重播胀开动画。 */
    ufo_tok_art_vm_kill(t);                            /* 句柄清零（幂等）*/
    ufo_tok_vm_spawn(t);                               /* 新脚本 + 当场摆位置；失败只报日志 */
}

/* ---- ★ 彩虹令牌每帧：60 px 闩 + 三段中断 + 换色计时器（一手见上面那个常量块）----
 * ⚠ 与 `ufo_tok_pick`（20 px 接触判定）是**两件事**：60 px 圈只影响画面与计时器，不是收集半径。
 * ⚠ 换池后 60 px 圈读的是**令牌自己的坐标**（`t->motion.x/y`）—— 以前读敌机的 `CE_ENEMY_POS_X/Y`，
 *   两边都是同一个 ECL 640 空间（§量纲那条注），所以数值**逐条不变**。 */
static void ufo_tok_rainbow_tick(const ufo_card_t *s, ufo_tok_t *t, float px, float py)
{
    float dx, dy;
    int near;

    if (!t->cycler) return;                            /* 一手整段只在 type 13/14/15 上跑（★ 新增2：判据从「色==彩虹」改成显式 cycler 标志）*/
    dx = t->motion.x - px;
    dy = t->motion.y - py;
    near = (dx * dx + dy * dy <= UFO_TOK_NEAR_R2);     /* 一手：距离² ≤ 3600（60 px）*/

    if (near != (int)t->near) {
        /* 进 / 出 60 px 圈的那一帧各发一次（一手两个「状态变了才发」的闩：`0x42630f` / `0x42632d`）。
         * 出圈时按计时器选 3（还没到 90 ⇒ 回主循环）或 2（已经在闪 ⇒ 接着闪）。 */
        if (near) ce_anm_interrupt((uint32_t)t->anm_id, UFO_TOK_INT_NEAR);
        else      ce_anm_interrupt((uint32_t)t->anm_id,
                                   t->rb_cycle < UFO_TOK_BLINK_AT ? UFO_TOK_INT_RESUME
                                                                  : UFO_TOK_INT_BLINK);
        t->near = (uint8_t)(near ? 1 : 0);
    }
    if (near) {
        /* 圈内：计时器冻结（一手那天 −1 由公共的 +1 抵消）。
         * ⚠ 一手**不是**纯粹的冻结：`0x4262f5` 在减完之后再看一眼「== 90 吗？是就再减一次」
         *   ⇒ 唯一残差是「`cur == 91` 会被摁回 90」（冻结在 90 而不是 91）。照抄它 ——
         *   不抄的话，在「91 帧那一刻进圈」这种情形下换色相位会差 1 帧（对照模型实测）。
         *   而 `cur == 90` 时它不动（90−1=89 → 不再减 → +1 回 90）⇒ 第 90 帧那次快闪中断
         *   在圈内**不会**被重复触发（这正是它存在的理由：别让 2 号中断盖掉同帧的 7 号）。 */
        if (t->rb_cycle == UFO_TOK_BLINK_AT + 1) t->rb_cycle = UFO_TOK_BLINK_AT;
        return;
    }

    /* ⚠ 顺序照一手：**先判阈值、后推进**（两个判在 `0x426352`/`0x426483`，推进在 `0x426368`）。
     * 换色那一帧也照走一次推进（一手换完 `jmp 0x42636a`）⇒ 稳定周期正好 `UFO_TOK_CYCLE_TICKS` 帧。
     * `rb_cycle` 是 uint8 ⇒ 推进前必须挡住 150（否则会加到 151/越界）——一手因为那个「换完再 +1」
     * 才让周期是整数 150：换色帧 0 → 1，此后每帧 +1，到 150 再换。 */
    if (t->rb_cycle == UFO_TOK_BLINK_AT) ce_anm_interrupt((uint32_t)t->anm_id, UFO_TOK_INT_BLINK);
    if (t->rb_cycle >= UFO_TOK_CYCLE_TICKS) {
        ufo_tok_rainbow_recolor(t);                    /* 内部把计时器清 0 */
        /* ★ 2026-09-18：换色打一行（用户实跑报「飞碟（token）没有变色」⇒ 这一行就是判据：
         *   有这行 = 链在跑（那问题在观感/贴图）；没有 = 变色令牌根本没生成（看下面那条日志）。 */
        ce_log("ufo: token RECOLOUR -> rb_state %d (cycler token #%u, script %d) at frame %u",
               (int)t->rb_state, t->vor, ufo_tok_script(t->color, t->rb_state, t->cycler),
               s ? s->play_frame : 0);
    }
    if (t->rb_cycle < UFO_TOK_CYCLE_TICKS) ++t->rb_cycle;
}

/* ★★ **造一只令牌**（2026-09-12 换池：从「建一个真敌人」变成「建一个 VM + 一条池记录」）----------------
 * 换池前这里要 `memset(spawn)` + 填 `CE_SPAWN_*` 一整张表 + `ce_spawn_enemy(UFO_ECL_TOKEN, …)`
 *   —— 那段**整段删掉**了（依据：用户拍板 + `AUDIT.md` §BO-11 的 P3 三条代价）。
 * 现在只有三步：① 池里有位置吗 ② 记下颜色 / 相位 / 位置 / 速度 ③ 起一个 VM 画它。
 * ⚠ **建 VM 仍然只能在 non-death-stack 上调**（要分配）—— 调用点两条都没变：
 *   `on_tick_2` ②b（帧末）与 `ufo_try_item` 的吸满支（`on_item_frame` 内）。
 *
 * ★ `color` / `rb_state` 两个**显式**入参（2026-09-12 那轮加的，换池后语义不变）：UFO 被打破 / 吸满
 *   时掉的那一枚是**指定颜色**的（一手 `th12:0x44af00` 的跳表 `0x44b590`：红→道具 13、蓝→14、
 *   绿→15、彩虹→按 `COLLECTED_UFOS[2]`）—— 不能走「小怪掉」那条路（那条是**随机**的，P2）。
 * 返回非 0 = 真的造出来了（池里多一只）；0 = 池满 / VM 建不出来（调用方记账 + 走重试节流）。 */
static int ufo_tok_spawn_ex(ufo_card_t *s, float x, float y, int32_t color, int rb_state, int cycler)
{
    ufo_tok_t *t;
    uint32_t serial;
    int slot;

    if (!s) return 0;
    if (ufo_token_pool_room(ufo_tok_live_count()) <= 0) {   /* ★ 上限判据在纯逻辑层（单一真源）*/
        ++s_tokpool.dropped;
        ce_log("ufo: token pool full (%d slots) - token dropped", UFO_TOKEN_CAP);
        return 0;
    }
    /* 出生点夹取（一手 `th12:0x4273f0`：x 夹 ±192、y 不小于 0）—— 令牌从敌人死亡点掉出来 */
    if (x < -UFO_TOK_SPAWN_CLAMP_X) x = -UFO_TOK_SPAWN_CLAMP_X;
    if (x >  UFO_TOK_SPAWN_CLAMP_X) x =  UFO_TOK_SPAWN_CLAMP_X;
    if (y < 0.0f) y = 0.0f;
    if (y > UFO_TOK_OUT_Y_BOTTOM) y = UFO_TOK_OUT_Y_BOTTOM;
    /* 槽 = 池尾（紧凑数组）⇒ `on_item_frame` 里记下的下标在整个生命周期里都指向同一只令牌。 */
    slot = s_tokpool.n;
    t = &s_tokpool.tok[slot];
    s_penta_id[slot] = 0;                              /* 防御性对账：池尾的框句柄必须是空的 */
    serial = s->token_serial;
    /* ★ 令牌的**颜色**：小怪掉的那条路 = **随机**（★ 2026-09-18 用户裁决 P2「随机吧」，
     *   见下面 `color == UFO_COLOR_NONE` 那一支）；UFO 掉的那条路由调用方指定。
     * ⚠⚠ 这里曾经写成 `(int32_t)ufo_hash_unit(0, serial)` —— `ufo_hash_unit` 返回 [0,1] 的小数，
     *   cast 成 int 几乎恒为 0（= `UFO_COLOR_NONE`）⇒ 令牌对所有代码路径**不可见**
     *   （池的循环直接跳过它 ⇒ 不收不撤；分配器永远发同一个槽 ⇒ 每 300 帧泄漏一只）。
     *   实测：serial 0..99999 里只有 1 个 cast 出非 0。⇒ **颜色必须是枚举值，不是散列**。
     * ★ 用户裁决 ①（金 = 蓝的调色版）：小怪掉的那条路颜色走 `ufo_token_color_for_serial_gold()` ——
     *   它在**随机取到的蓝**之上每隔 `UFO_GOLD_CYCLE` 次做成**金**（`ufo_token_color_gold_variant`）。
     *   金进槽即归一成蓝
     *   （`ufo_color_norm`）⇒ 除了贴图/颜色显示，**行为与蓝一字不差**。 */
    if (ufo_token_color_is_field(color)) {
        t->color = color;                              /* UFO 掉的那条路：一手掉落表指定的色 */
    } else {
        /* ★★ P2（用户 2026-09-18「随机吧」）：小怪掉的那条路**随机取红/蓝/绿**。
         *   一手那条「颜色写死在敌人的 ECL 掉落表里」在 TH18 不可移植（TH18 没有这张表）⇒ 用户裁决随机。
         *   ⚠ 用**我们自己的 LCG**（`ufo_rand_next`，纯整数）：引擎那两支 RNG 走 x87，会踩 dllx87 硬门。
         *   ⚠ 取 `>> 16` 那一段再取模 —— LCG 低位图案明显。 */
        uint32_t roll = (s_rng_seed = ufo_rand_next(s_rng_seed));
        t->color = ufo_token_color_gold_variant(ufo_token_color_for_roll(roll), roll);
    }
    /* ★ 2026-09-18（新增 2）：**变色令牌**（一手 type 13/14/15）—— 它的 `color` 是**起始显示色**，
     *   显示与入槽都走 `ufo_token_display_color()`；只有 UFO 击破/吸满返还的那一枚是变色令牌。 */
    t->cycler = (uint8_t)(cycler ? 1 : 0);
    t->ttl = UFO_TOKEN_TTL_FRAMES;
    t->vor = serial;                                   /* 对账用（一手 `ItemManager+0x666fe0` 那个计数）*/
    t->pentagon_hint = 0;
    t->anm_id = 0;                                     /* ★ 新的 VM 句柄从头开始（下面 spawn 里填）*/
    /* ★ 运动：一手 state 6 的直线飞行 + 反弹（角度按**本关第几个令牌**的奇偶交替）。
     *   ⚠ 一手那个计数把「UFO 掉的令牌」也算进去（`0x427411 inc [edi+0x666fe0]` 在两条路之前）
     *   ⇒ 我们用同一个 `token_serial`（两条路都自增）✓。
     *   ★★ 换池：位置/速度写进**我们自己的** `t->motion`（以前位置在敌机字段里）。 */
    ufo_tok_motion_init_at(&t->motion, serial, x, y);
    /* ★ A8：彩虹令牌的初始态。一手：`spawn_item` 把闩清 0（`0x427595`）⇒ 出生算「圈外」、
     * 第一帧不发中断；计时器从 0 起（换色后 `Timer__set_current(0)`）。
     * 起始**态**：小怪掉的统一从 0 = 红那一态起（= TH12 的道具类型 13 ⇒ 脚本 178）；
     * UFO 掉的由一手掉落表给（红 UFO → 13、蓝 → 14、绿 → 15、彩虹 → 按第 3 个令牌的颜色，
     * `th12:0x44af00`）⇒ 现在由调用方按 `ufo_break_token_state()` 传进来。 */
    t->rb_state = (uint8_t)((rb_state >= 0 && rb_state < UFO_TOK_RB_STATES) ? rb_state : 0);
    t->near = 0;
    t->rb_cycle = 0;
    ++s_tokpool.n;                                     /* ★ 先入池（VM 建失败也要在池里，好让 TTL 收掉它）*/
    /* ★ 画面 = 本体：起**那一个** VM（1 只 token = 1 个 VM，没有第二个） */
    ufo_tok_vm_spawn(t);
    ++s->spawned_tokens;
    ++s->token_serial;
    return 1;
}

/* 小怪掉的那一条 —— 调用点只有一处（`on_tick_2` ②b）。
 * ★★ 2026-09-18 **用户裁决**（逐字）：「可能得做固定表，这个不像鬼形兽怎么都能开出来
 *   —— **或者不费那个事情，就是掉会换色的**」⇒ 取**后一条**：小怪掉的令牌也做成**变色令牌**
 *   （一手 type 13/14/15 那种），**起始色随机**（P2 那条裁决不变）、之后每 150 帧换一色。
 * ⚠ 与一手的差别（登记 `TUNING.md` §2.0s）：一手小怪掉的是**固定色**（type 10/11/12，由敌人的
 *   掉落表写死），只有 UFO 返还的那一枚会变色；我们这张卡在 TH18 **没有敌人掉落表可抄**
 *   （`AUDIT.md` §BO-11 的 P2），所以"固定表"要么自己编一张、要么走用户给的第 2 条 —— 现在走第 2 条。
 * ⚠ 由此"入槽记哪一色"完全取决于**收到它的那一刻它显示哪一色**（= 一手对变色令牌的口径，新增 1/2）。*/
static int ufo_tok_spawn(ufo_card_t *s, float x, float y)
{
    uint32_t roll = (s_rng_seed = ufo_rand_next(s_rng_seed));
    int st = (int)(roll % UFO_TOK_RB_STATES);
    int32_t c = ufo_token_display_color(UFO_COLOR_RED, 1, st);
    int ok = ufo_tok_spawn_ex(s, x, y, c, st, 1);      /* cycler = 1（会换色）*/
    /* ★★★ 2026-09-23（owner #18：「生成飞碟==token 的时候有一个音效，得加进来」）：
     *   TH12 在**令牌生成**那一刻播一声：一手 `th12:0x42741b mov $0x37,%eax` + `0x427423 call 0x453e20`
     *   （= 同一支 SE 播放器，与 token-take 那条同一个）⇒ id `0x37` → SE 表 `th12:0x4ae5a4` 第 0x37 行
     *   （VA `0x4ae9f0`）`+0x00` = 名表下标 44 → 名表 `th12:0x4aea50` → **`se_piyo.wav`**。
     *   TH18 **没有**这一枚 ⇒ 走扩表新槽 `CE_VOICE_UFO_TOKEN_SPAWN`（`0x5e`，构建期从 TH12 逐字节现取；
     *   音量 −300 = TH12 该行 `+0x04` 的 −3.00 dB；priority 100 为 ❓ 自定、与其它扩表项一致）。*/
    if (ok) ufo_se(UFO_SEV_TOKEN_SPAWN, CE_VOICE_UFO_TOKEN_SPAWN, x,
                   "一手 th12:0x42741b mov $0x37 + 0x427423 call 0x453e20（令牌生成；se_piyo.wav）");
    return ok;
}

/* ★ 2026-09-18（新增 2）：令牌的**显示色** = 它会往槽里记的那一色（一手：变色令牌按「当时显示的那一色」）。
 * 唯一的出口 —— 入槽、五边形框判据、日志都走它。 */
static int32_t ufo_tok_display_color(const ufo_tok_t *t)
{
    if (!t) return UFO_COLOR_NONE;
    return ufo_token_display_color(t->color, t->cycler, t->rb_state);
}



/* ==================== §3.3 召唤：UFO 本体（ECL 真敌机）==================== */
/* 出生点：一手活动框 `moveLimit(0,128,280,128)` ⇒ x ∈ [−140,140]、y ∈ [64,192]（spec §1 末行）。
 * 出生在框中央（ECL 到位后由脚本自己在框里飘；占位惰性壳不动 ⇒ 恒在中央）。 */
/* ★★ 击破大消弹的载体：在 UFO 位置起一只 `UFO_EtBreak2`（一手 `0x44af00` 的第一件事）。
 * spawn 表逐条照抄：位置 = UFO 的 `final_pos`（**无偏移**）、`life = 3000`、
 * `main_drop = 0`、`score_reward = 0`，角度/速度 = 0（原地）。 */
static void ufo_etbreak2_spawn(float x, float y)
{
    uint8_t spawn[CE_SPAWN_SIZE];
    float *pos;

    if (!CE_ENEMY_MGR()) return;
    memset(spawn, 0, sizeof spawn);
    pos = (float *)(spawn + CE_SPAWN_POS);
    pos[0] = x; pos[1] = y; pos[2] = 0.0f;
    *(int32_t *)(spawn + CE_SPAWN_SCORE) = 0;
    *(int32_t *)(spawn + CE_SPAWN_MAIN_DROP) = 0;
    *(int32_t *)(spawn + CE_SPAWN_PARENT) = 0;
    *(int32_t *)(spawn + CE_SPAWN_LIFE) = UFO_ETBREAK2_LIFE;
    if (!ce_spawn_enemy(UFO_ECL_ETBREAK2_NAME, spawn))
        ce_log("ufo: ce_spawn_enemy(\"" UFO_ECL_ETBREAK2_NAME "\") failed - the break-time big cancel ring "
               "is missing this file (assets/ecl/ufo.ecl.txt / make ecl-ufo)");
}

static uint8_t *ufo_body_spawn(int32_t color, int32_t spawn_life)
{
    uint8_t spawn[CE_SPAWN_SIZE];
    float *pos;
    uint8_t *e;
    const char *name = ufo_ecl_body_name(color);       /* 按色选子程序：★ 金落蓝那一件（等 P 图）*/

    if (!CE_ENEMY_MGR()) return 0;
    memset(spawn, 0, sizeof spawn);
    pos = (float *)(spawn + CE_SPAWN_POS);
    pos[0] = 0.0f;
    pos[1] = (UFO_FRAME_Y_TOP + UFO_FRAME_Y_BOTTOM) * 0.5f;   /* 128.0f = 框中央 */
    pos[2] = 0.0f;
    *(int32_t *)(spawn + CE_SPAWN_SCORE) = 0;          /* ★ 击破分数：一手公式没有（spec §9 #5）⇒ 不给 */
    *(int32_t *)(spawn + CE_SPAWN_MAIN_DROP) = 0;      /* ★ 主掉落：奖励全走 `ce_spawn_item`，不靠它 */
    *(int32_t *)(spawn + CE_SPAWN_PARENT) = 0;
    /* ★★ P4 + P2 #6：这里写的是 **spawn 值**（≤ `UFO_LIFE_MAX_ALLOWED`，避开位 30 黑名单）——
     *   真血量在敌机建出来之后由 `ufo_body_apply_real_hp()` 回写（一手见那支函数的注）。 */
    *(int32_t *)(spawn + CE_SPAWN_LIFE) = spawn_life;
    e = (uint8_t *)ce_spawn_enemy(name, spawn);
    if (!e) {
        ce_log("ufo: ce_spawn_enemy(\"%s\", color %d, art idx %d) failed - placeholder ECL name only; "
               "the four colour subroutines (UFO_Red/Blue/Green/Rainbow, one-hand name table "
               "th12:0x4b33e0) are NOT written yet (spec §6 step 3)",
               name, (int)color, ufo_color_art_index(color));
        return 0;
    }
    return e;
}

/* ★★ 2026-09-21（P2 #6）：把**真血量**（一手表原值，可 > 999）写回敌机的三格 life。
 * 一手形状 = `th18:0x42d815`–`0x42d82a`（spawn 那一刻引擎自己就是往这三格写同一个数）。
 * ⚠ 时机：必须在 `ce_spawn_enemy()` **返回之后**（对象已经是活的），且**不要**回头再动
 *   `spawn+0x14` —— 那条路才会置位 30（见 `UFO_ENEMY_LIFE_2` 上面那段一手）。 */
static void ufo_body_apply_real_hp(uint8_t *e, int32_t hp)
{
    if (!e || hp < 1) return;
    *(int32_t *)(e + UFO_ENEMY_LIFE_CUR) = hp;
    *(int32_t *)(e + UFO_ENEMY_LIFE_2)   = hp;
    *(int32_t *)(e + UFO_ENEMY_LIFE_3)   = hp;
}

/* ============================ ★ 本体跟随 VM（画面）============================
 * 为什么需要：ECL 侧的本体是**隐形载体**（`anmSetSprite(0, -1)` = 原版的「拆槽」分支 ——
 * 槽直接拆掉、**不建 VM**，一手 `th18:0x436bf6`；见 `UFO_ECL_HIDDEN_BODY_SCRIPT` 上面那个注释块）
 * —— ECL 的 anm 索引表里没有 `ability.anm`，脚本 164..167 只能由卡侧起（完整一手链见文件头那个注释块）。
 * 生命周期：`ufo_try_summon()` 建成时起、`ufo_teardown()` / `ufo_withdraw_all()` 收
 * （⚠ 删卡走 `on_delete`，那一刻 tick 回调**全停** ⇒ 绝不能只靠每帧跟随那一支收尾）。 */

/* 收掉本体的跟随 VM（句柄清零 ⇒ 幂等）。 */
static void ufo_body_vm_kill(ufo_card_t *s)
{
    if (s->body_anm_id) { ce_anm_delete(s->body_anm_id); s->body_anm_id = 0; }
}

/* 起一个本体跟随 VM，并**立刻**摆到敌机位置上（世界层 (0,0,0) = 弹幕区上边框中点 ⇒ 不先摆会闪一帧）。
 * `e` = 本体敌机（取位置用；可为 NULL，那就停在原点等每帧跟随来摆）。 */
static void ufo_body_vm_spawn(ufo_card_t *s, int32_t color, const uint8_t *e)
{
    int script = ufo_body_anm_script(color);
    uint32_t id = ce_anm_spawn(CE_ABILITY_ANM(), script, UFO_BODY_ANM_LAYER);
    if (!id) {
        ce_log("ufo: body follow-VM spawn FAILED (color %d, script %d, layer %d) - the body enemy "
               "stays invisible (ECL side only tears slot 0 down: anmSetSprite(0, -1))",
               (int)color, script, UFO_BODY_ANM_LAYER);
        return;
    }
    s->body_anm_id = id;
    if (e) ce_anm_set_pos(id, *(const float *)(e + CE_ENEMY_POS_X),
                              *(const float *)(e + CE_ENEMY_POS_Y), 0.0f);
}

/* 每帧跟随（本体）：位置 = 敌机的 `abs_pos`（`CE_ENEMY_POS_X/Y` = `data.final_pos`，与 ECL 同一套
 * 640 空间的 ECL 坐标 —— 世界层 VM 的实体坐标就是这个空间，1 单位 = 2 表面 px，
 * 见 `engine/anm/th18/01-vm-instantiate.md` §3，所以不需要任何换算）。
 * ⚠ 敌机指针为 NULL（被静默撤掉 / 飞走 / 击破那一刻）⇒ **必须把 VM 收掉**，别让它留在场上。
 * ⚠ 句柄在别处被引擎销毁过 ⇒ 自愈：丢掉句柄重新起一个（与令牌那一支同款，照 `ufo_hud_ensure()`）。
 * 🟡 位置取自「本帧 `on_item_frame` 时刻」的敌机坐标；敌人 sweep 在同一帧更晚的位次跑 ⇒ 极端情况下
 *    VM 可能比敌机的**逻辑位置**晚一帧（敌机本体是隐形的，肉眼看不到这一帧差）。待实跑确认。 */
/* ★★★ 2026-09-23（owner 实跑第 ③ 条「召唤动画是紫色三角（不对）」）—— **换成一手那一套**。
 *
 * 一手 = `th12:0x44a8a0`（`Ufo::spawn`）：建完 UFO 敌机之后**紧接着**建的是
 *   `front.anm` **script 0x52(82)** 的 VM，并写 `pending_interrupt = (色 == -1 ? 3 : 色 - 1) + 7`
 *   （`engine/ufo/th12/01-ufo-system.md` §5，逐指令读出；同一段还有吸收环 0xcf 与两个音效）。
 * ⇒ 那支脚本才是"召唤动画"：**304×32 的大横幅**（`sprite68` = entry1 `front/front02.png` 的 (0,0)），
 *   `layer(6)`、`blendMode(1)`（加算）、`pos(224, 208)`（场地中心、场地上半）、
 *   `alpha(0) → alphaTime(20,0,128)`，`+30: scaleTime(30,2,2,2)`（边长大边淡出）、`+75` 自删；
 *   子脚本 **83** = `sprite69`（32×32 的窗口 + `scrollX`，从场地左侧扫到右侧 = 擦除）、
 *   **84 ×3**（`+35/+40/+45`） = 同图残影。红/蓝/绿/彩虹 = 中断 7/8/9/10。
 *   本卡脚本 = **324（82）/ 325（83）/ 326（84）**，逐条移植，改动（Δ）在脚本文件头。
 *
 * ⚠ **紫环（318/316/317）与青六边形（321/319/320）本批停用**：它们的一手
 *   （`enemy.anm` 141+139/140、103+102/104）是 owner #14 之后我们按「全 enemy.anm 里只有 4 支用
 *   `drawPoly`」挑出来的**候选**，而 `Ufo::spawn` 里根本没有它们 —— owner 实跑已判**不对**。
 *   脚本文件保留（脚本号一动就会顶掉 322/323 的破碟爆闪），只是不再被起。
 *
 * ⚠ **位置**：横幅的世界坐标 = **场地中心 x = 0、场地上沿往下 192** ——
 *   TH12 那个 anm 空间的场地是 x∈[32,416] / y∈[16,464]（x 以场地左沿为原点），TH18 世界层
 *   x **以场地中心为原点**（本卡脚本 228/231 的 `pos(0,-28)`、`scale(6,59)` 原值就能落在 UFO 上，
 *   实跑验过）⇒ Δx = 224 − 32 − 192 = 0、Δy = 208 − 16 = 192（推导同脚本 324 的 Δ1）。*/
/* ★★ 2026-09-23（owner 两轮实跑）：**横幅的整体尺寸系数** ——
 *   一手 `front.anm` script82/84 是 `scale 1.0 → scaleTime(30,2,2,2)`（304×32 的贴图 ⇒ 峰值 608 单位，
 *   比场地（384）还宽）。owner 先报「大概大了 3 倍」⇒ 缩到 1/3，随后又报「那个 UI 又小了」
 *   ⇒ 取中间 **2/3**（基准 0.667 / 峰值 1.333 ⇒ 峰值约 405 单位 ≈ 场地宽，"刚好横跨场地"）。
 *   ⚠ 这个宏**只用于日志**（真正生效的数是三支脚本里的 `scale` / `scaleTime` 目标 —— 脚本里读不到 C 宏）；
 *     改尺寸就改那两对数（324 基准+峰值、325 基准、326 基准+峰值），顺手把这个数改成同一个值。*/
#define UFO_SUMMON_FX_SCALE_PERMILLE 667u
#define UFO_SUMMON_FX_LAYER  6             /* 一手：script82/83/84 自己写的 `layer(6)`（游玩区世界层）*/
#define UFO_SUMMON_FX_DX     0.0f          /* 一手：(224 − 32 − 192) = 场地中心 */
#define UFO_SUMMON_FX_DY   192.0f          /* 一手：(208 − 16) = 场地上沿往下 192 */
static uint32_t s_summon_fx_id;            /* 大横幅发射器 VM（脚本 324 在 +75 帧自己 delete()）*/

/* ★★ 2026-09-24（owner：「召唤动画的**闪电**…它的样式就是**类闪电**，并且似乎是**动态**的 ——
 *   拿了一个**蓝色的会有残影的像素，随机地让它向外运动**画出来的」＋「并且**中间是洋红色**／
 *   **后面被 ufo 贴图盖住了**我没看出来」）—— 召唤那一刻的**闪电碎屑**。
 *
 * **一手 = TH18 自己的 `bullet.anm` etbreak 家族**（`local/th18.v1.00a/anm/bullet/bullet.anm.txt`
 * 的 `script160..313`）：那一族是**同形、只差颜色与尺寸档**的几十支脚本，本作引擎自己就在用
 * （敌人被击破时的碎屑）。本卡借其中**第二档 + 蓝/洋红两色**，五支脚本落在 `ability.anm` 346..350：
 *   346 = 驱动器（← 一手 script227）／347+348 = 蓝色碎屑的暗层 + 加算亮层（← 225/226）／
 *   349+350 = 中心洋红的暗层 + 加算亮层（← 219/220，第一帧撒一对 ⇒ owner 说的「中间是洋红色」）。
 *   切图 = `assets/ability/make_ufo_debris_art.py`（原生 `effect/etbreak.png` 的 sprite540..547）；
 *   移植改动（层号 20/21 → **5/6**、加「向外飞」的 `posTime`、撒 12 轮）= 那五支脚本的文件头 +
 *   `TUNING.md` §2.0zzf。
 *
 * 位置 = **UFO 本体那一刻的位置**（`entity_pos` 写敌机的 `final_pos`，与本体跟随 VM 同一套 ECL 空间）
 * ⇒ 子 VM 继承它、整批碎屑就洒在飞碟身上。驱动器在 346 里自己撒完、约 90 帧后自删，C 侧不跟随
 * （UFO 在这 ~1.5 秒里只挪几个单位，一手那些驱动器同样是一次性摆位）。
 * 层号在脚本里写死 **5/6**：**低于本体 VM 的 7** ⇒ 正中心那一颗洋红会被 UFO 贴图盖住 —— 正是 owner
 * 观察到的那件事（详见那五支脚本的 Δ1）。 */
#define UFO_SUMMON_DEBRIS_LAYER 6          /* 驱动器自己的层（脚本 346 里也写 6；子 VM 各自写 5/6）*/
static uint32_t s_summon_debris_id;        /* 闪电碎屑发射器 VM（脚本 346 自己 delete()）*/

static void ufo_summon_debris_spawn(const uint8_t *e)
{
    void *anm = CE_ABILITY_ANM();
    if (!anm) return;
    /* 陈旧句柄先作废（驱动器自删 ⇒ 句柄会被引擎复用，见 `ufo_summon_fx_try` 的同一段说明）。 */
    if (s_summon_debris_id && !ce_anm_get_vm(s_summon_debris_id)) s_summon_debris_id = 0;
    if (s_summon_debris_id && ce_anm_get_vm(s_summon_debris_id)) return;   /* 上一批还在飞 ⇒ 不叠 */
    s_summon_debris_id = ce_anm_spawn(anm, CE_ANM_ABILITY_SCRIPT_UFO_SUMMON_DEBRIS, UFO_SUMMON_DEBRIS_LAYER);
    if (!s_summon_debris_id) {
        ce_log("ufo: summon debris spawn FAILED (script %d, layers %d/5+6) - no lightning this time",
               CE_ANM_ABILITY_SCRIPT_UFO_SUMMON_DEBRIS, UFO_SUMMON_DEBRIS_LAYER);
        return;
    }
    if (e) ce_anm_set_pos(s_summon_debris_id, *(const float *)(e + CE_ENEMY_POS_X),
                                              *(const float *)(e + CE_ENEMY_POS_Y), 0.0f);
    ce_log("ufo: summon debris up (ability.anm script %d <= TH18 bullet.anm script227 driver + "
           "225/226 blue + 219/220 magenta core; world layers 5 (dim) + 6 (additive), i.e. BELOW the "
           "body VM's 7 so the centre hides behind the UFO sprite - owner observation; 12 rounds, "
           "self-deletes)",
           CE_ANM_ABILITY_SCRIPT_UFO_SUMMON_DEBRIS);
}

/* 收尾（**长持续效果铁律**）：横幅 / 碎屑都绑在 UFO 上，但两条路都必须收得掉 ——
 * `ufo_withdraw_all()`（`on_delete` / `on_run_reset`）与 `on_stage_start()` 各调一次（幂等）。
 * ⚠ 为什么不能只靠"它们自己会 delete()"：删卡那一刻卡的 tick 全停（`on_delete`），
 *   而这两只 VM 是**引擎**在跑的 —— 不收就会在场上留一条横幅 / 一批碎屑。 */
static void ufo_summon_fx_kill(void)
{
    if (s_summon_fx_id)      { ce_anm_delete(s_summon_fx_id);      s_summon_fx_id = 0; }
    if (s_summon_debris_id)  { ce_anm_delete(s_summon_debris_id);  s_summon_debris_id = 0; }
}

/* ==================== ★★ 令牌宿主：换皮 + 头顶旋转飞碟 + 每波只挑 1~2 只（owner 2026-09-24）==========
 *
 * **owner 口径（逐轮原话，合起来就是这一节的全部规则）**
 *   ①「th12 原版掉落飞碟的怪……只有**大蝴蝶**和**杂鱼妖精**（boss 我不清楚）；**得改谁能飞碟掉落**，
 *      并且还得给掉的那个**改帖图**」
 *   ②「掉飞碟的大蝴蝶的贴图是 `enemy.png` 的**第一二行**；掉飞碟的杂鱼妖精是 `enemy.png` 妖精贴图的
 *      **第 4 行**；并且她们头上会有 `enemy.png` 第二行的右半部分的**旋转飞碟特效**」
 *   ③「**杂鱼妖精是一碰就碎的**，但是我一般说的"**中**"妖精，血量一般**得打一会儿**」
 *   ④「th18 第一面，第一波是三波**红绿阴阳玉**，第二波是**一些杂鱼妖精外加一个中妖精**」
 *   ⑤「**杂鱼妖精原始精灵尺寸也得是 32*32 的**」
 *   ⑥「**同一波的那只中妖精不应该掉，整个中妖精家族就不应该掉**。你把换皮看成是，**它带着 token，
 *      所以换皮** ⇒ **必须原本是 64*64 是大蝴蝶才换皮掉**」
 *   ⑦「和目前 th10 th11 的卡（的设计/困境）类似，**一波（或者一个子波）一般只有 1 个妖精是特殊妖精**，
 *      考虑到对称性**可能是两个**；第一面第二波如果是手动设计的话，是**只会有对称位置的 2 个小妖精**给 UFO，
 *      且**排在队列最后**」
 *
 * ⇒ 三条硬规则（**都不是"整族换皮"，而是"这一只带着 UFO"的视觉标记**）：
 *   **A. 谁能带**：`UFO_CARRIER_KEYS[]`（生成器扫出来的宿主表）——
 *      · **32×32 且一碰就碎**（`hp ≤ 400`）的**杂鱼妖精** ⇒ 穿妖精皮（脚本 352 = TH12 `sprite36..39`）；
 *      · **64×64** 的**大蝴蝶** ⇒ 穿蝴蝶皮（脚本 351 = TH12 `sprite125..132`）；
 *      · **其余（48×32 / 48×48 的中妖精家族）不是宿主**：不换皮、一枚都不掉（owner ⑥）。
 *   **B. 一波只挑 1~2 只**（owner ⑦）：同一波（同 `(N,S)`、到达间隔 ≤ `UFO_CARRIER_WAVE_GAP` 帧）里
 *      只把**最后到达**的那一只 + 它的**镜像伙伴**（`|x1+x2| ≤ 16`、`|y1−y2| ≤ 24`）选成宿主 ——
 *      「排在队列最后」+"对称位置的 2 个"。其余同波同类妖精**保持原生贴图、不掉令牌**。
 *      （波号靠 **`enemy_id` 单调递增** = 创建顺序，所以"最后到达"就是"创建得最晚"那只。）
 *   **C. 换皮的画面**：皮 VM（`layer(8)`）+ 头顶飞碟 VM（脚本 353，`layer(9)`、脚本自带 `pos(0,-32)`），
 *      每帧跟着敌人的 `final_pos`；同时**把原生贴图隐掉**（每帧写宿主槽 0 的 VM `color_1` alpha = 0），
 *      收尾（删卡 / 换关 / 敌人消失）**还原**原来那一色。
 *
 * **一手依据**：表 = `native/tools/gen_ufo_carrier_table.py` 从 TH18 的 7 关 ECL + `enemy.anm` 现扫
 * （键 = `enemy+0x1494/0x1498` 的 `(N,S)`「它长什么样」，与 110 的大信仰点表同一套判据）；
 * 三张皮 = TH12 `enemy.anm` 的 `sprite125..132` / `36..39` / `167..172` 1:1 切图（`make_ufo_carrier_art.py`）。
 * 逐条推导与退役清单见 `TUNING.md` §2.0zzm / §2.0zzo / §2.0zzp / §2.0zzq，实跑判据见
 * `SINGLE-CARD-TEST.md` §本批14-1。 */
/* ---- ECL 波号（owner 2026-09-24 第 15 轮：「115 审计的时候看见了，**ecl 脚本里面是有波的计数的**」）----
 * 一手与 115/117 同源（`native/cards/lolk.c` 的 `enemy_ecl_sub` / `animal.c` 的 `animal_enemy_sub_name`）：
 *   · 关卡脚本 = 引擎建的那个 `main` 敌人（`parent_enemy_id == 0` 且当前子程序名是 `main` / `Main*`）；
 *   · 它的**当前子程序**（`zEnemy+0xc` → `ctx+0x4`，子程序名从 `zEnemy+0x1218` → `fm+0x8c` 的
 *     子程序表取，每项 8 B = {name, bytecode}）；
 *   · 名字里的 **`MainSubNN` 数字 = 波号**（"MainSub00"→0、"MainSub01b"→1、"MainSub01c"→1；
 *     `main` / `MainFront` / `MainBoss` 这种 → −1）。115 的注释把这件事写得很清楚：
 *     「章/波的天然键 —— 去掉尾部字母后的数字是一章」。
 * ⇒ **波 = MainSub 编号**（ECL 自己的计数），不再靠"到达间隔 ≤ N 帧"这种我们编的启发式；
 *   拿不到时（例如开发壳、或主上下文恰好不在 MainSub 段）才退回间隔启发式（`s_stage_wave = -1`）。 */
#define UFO_ENEMY_PARENT_ID    0x6834     /* zEnemy.parent_enemy_id（引擎建的关卡脚本 = 0）*/
#define UFO_ENEMY_ECL_CTX      0x0c       /* zEnemy.ecl.context.current_context */
#define UFO_ECL_CTX_SUB        0x04       /* zEclRunContext.cur_location.subroutine_index */
#define UFO_ENEMY_ECL_FILEMGR  0x1218     /* zEnemy.ecl.file_manager */
#define UFO_ECL_FM_SUB_TABLE   0x8c       /* zEclFileManager.subroutines */
#define UFO_ECL_FM_SUB_COUNT   0x08       /* zEclFileManager 子程序个数 */
#define UFO_MAINSUB_DIGITS_MAX 6

static int32_t s_stage_script_id;         /* 关卡脚本那只敌人的 id（指针不跨帧缓存）*/
static int32_t s_stage_wave = -1;         /* 本帧的 ECL 波号（MainSub 数字；-1 = 读不到）*/

static const char *ufo_enemy_sub_name(const uint8_t *e, int32_t *out_sub)
{
    uint8_t *ctx, *fm, **subs;
    int32_t sub;
    if (out_sub) *out_sub = -1;
    if (!e) return 0;
    ctx = *(uint8_t **)(e + UFO_ENEMY_ECL_CTX);
    fm  = *(uint8_t **)(e + UFO_ENEMY_ECL_FILEMGR);
    if (!ctx || !fm) return 0;
    sub = *(int32_t *)(ctx + UFO_ECL_CTX_SUB);
    if (sub < 0 || sub >= *(int32_t *)(fm + UFO_ECL_FM_SUB_COUNT)) return 0;
    subs = *(uint8_t ***)(fm + UFO_ECL_FM_SUB_TABLE);
    if (!subs) return 0;
    if (out_sub) *out_sub = sub;
    return (const char *)subs[sub * 2];
}

static int32_t ufo_parse_mainsub_number(const char *n)
{
    int32_t v = 0;
    int i;
    if (!n) return -1;
    if (strncmp(n, "MainSub", 7) != 0) return -1;
    n += 7;
    if (*n < '0' || *n > '9') return -1;
    for (i = 0; i < UFO_MAINSUB_DIGITS_MAX && n[i] >= '0' && n[i] <= '9'; ++i) {
        v = v * 10 + (n[i] - '0');
        if (v > 99999) { v = 99999; break; }
    }
    return v;
}

static uint8_t *ufo_stage_script_find(uint8_t *em)
{
    uint8_t *e;
    if (!em) return 0;
    if (s_stage_script_id) {
        e = ce_enemy_by_id(em, s_stage_script_id);
        if (e) return e;
        s_stage_script_id = 0;
    }
    CE_ENEMY_FOREACH(em, e) {
        const char *name;
        int32_t sub;
        if (!e) continue;
        if (*(const int32_t *)(e + UFO_ENEMY_PARENT_ID) != 0) continue;
        name = ufo_enemy_sub_name(e, &sub);
        if (name && (strncmp(name, "Main", 4) == 0 || strcmp(name, "main") == 0)) {
            s_stage_script_id = *(const int32_t *)(e + CE_ENEMY_ID);
            return e;
        }
    }
    return 0;
}

/* 本帧的 ECL 波号（读不到就 −1；读不到时上层退回"到达间隔"那条启发式）。 */
static int32_t ufo_stage_wave(void)
{
    uint8_t *em = (uint8_t *)CE_ENEMY_MGR();
    uint8_t *e;
    const char *name;
    int32_t sub;
    if (!em) return -1;
    e = ufo_stage_script_find(em);
    if (!e) return -1;
    name = ufo_enemy_sub_name(e, &sub);
    return ufo_parse_mainsub_number(name);
}

#define UFO_CARRIER_TRACK_MAX      24     /* 同时跟踪的"候选 + 宿主"上限（波里多出来的只跟丢不换皮）*/
#define UFO_CARRIER_WAVE_MAX       12     /* 一波里最多记这么多只候选 */
#define UFO_CARRIER_WAVE_GAP       20     /* 读不到 ECL 波号时：相邻两只到达间隔 > 它 ⇒ 算新的一波（帧）*/
#define UFO_CARRIER_WAVE_SETTLE    60     /* 有 ECL 波号时：一波静默 > 它 ⇒ 判定（防止最后那波一直不判）*/
#define UFO_CARRIER_PER_WAVE        2     /* 一波里最多几只宿主：owner ⑦「一般 1 只，考虑对称性 2 只」*/
#define UFO_CARRIER_SYM_XTOL       16.0f  /* 镜像判据（单位）：|x1 + x2| ≤ 它 */
#define UFO_CARRIER_SYM_YTOL       24.0f  /*                  |y1 − y2| ≤ 它 */
#define UFO_CARRIER_SKIN_ENABLE     1     /* 0 = 不换皮（只保留"谁能掉"那条规则）*/
#define UFO_CARRIER_HIDE_NATIVE     1     /* 1 = 隐掉宿主原生贴图（每帧写 alpha 0；收尾还原）*/
#define UFO_CARRIER_BODY_LAYER      8     /* 一手：`enemy.anm` script45 / script35 的 `layer(8)` */
#define UFO_CARRIER_MARK_LAYER      9     /* 一手：script98 的 `layer(9)`（压在皮之上）*/
#define UFO_CARRIER_ALPHA_HIDDEN    0x00ffffffu   /* D3DCOLOR：alpha 0、RGB 白（同整 B 藏原生星的手法）*/
#define UFO_CARRIER_SLOTS          16     /* 宿主自己的 anm 槽数（`zEnemyData.anm_vm_ids[0x10]`）*/
/* ★★ owner 2026-09-24 第 18 轮裁决：「**如果自带火焰那就别改了**」⇒ 默认 **0 = 火焰留着**
 *   （槽 1 是敌人自带的火焰/光环：TH18 `GirlD01` 的 ECL 写 `anmSetSprite(1, 105)`，
 *    脚本 105 = `layer(11)` + `加算` + 子脚本 106/107 = `sprite152..159` 的 8 帧火焰
 *    —— 那是**敌人自己的设计**，不是我们画的；换皮的宿主背后照样挂着自己的火）
 *   想连它一起隐掉（让宿主只剩 TH12 那层皮）就把这个宏改 1 —— 一行。）*/
#define UFO_CARRIER_HIDE_SLOT1      0     /* 1 = 连槽 1（敌自带的火焰/光环）一起隐掉；0 = 留着（owner 口径）*/
#define UFO_TOKEN_DROP_LEGACY       0     /* 1 = 旧行为：任意带掉落表的小怪也掉（owner 口径已废）*/

/* ★★ owner 2026-09-24 第 11 轮：「道中 boss 怎么是掉落一红一蓝啊，掉落两蓝的也有 ——
 *   **统一改成两个会变的随机吧**」⇒ **任何宿主 / boss 击破都掉 2 枚**，而且都是"会变的"令牌
 *   （登记时 `color = UFO_COLOR_NONE` ⇒ 走 `ufo_tok_spawn()`：随机起始色 + 每 150 帧换一色）。
 *   ⚠ 退役（历史口径留在 `TUNING.md` §2.0zzn / §2.0zzm-4）：妖精1/蝴蝶2 两个宏、逐关固定色掩码表、
 *     boss 的 1 红 + 1 绿（`pend_drop_boss`）。 */
#define UFO_CARRIER_TOKENS_DROP    1      /* ★ owner 2026-09-24 第 18 轮：「**一个非 boss 敌人只掉 1 个飞碟，不是 2 个**」*/
#define UFO_CARRIER_TOKENS_BOSS    2      /* ★ boss 仍是 2 枚（owner 第 11 轮：「道中 boss 一红一蓝 / 两蓝」⇒ 统一两个会变的随机）*/

typedef struct {
    uint8_t  used;
    uint8_t  carrier;              /* 1 = 这一只被选中（换皮 + 掉令牌）；0 = 只是波里的普通妖精 */
    uint8_t  logged;               /* 换皮日志每只只报一次 */
    uint8_t  facing;               /* ★ 第 17 轮：0 = 正面（走位皮）/ 1 = 向右（356）/ 2 = 向左（357）*/
    float    last_x;               /* 上一帧的 x（算水平速度方向）*/
    uint8_t  have_last_x;
    uint8_t  kind;                 /* `ufo_carrier_kind_t` */
    int16_t  n, s;                 /* 外观身份（`anmSelect` 的 N / 主脚本 S）*/
    int32_t  enemy_id;
    float    skin_scale;           /* 皮的缩放（表里的 `skin_scale_permille`）= 1.0（三条规则下恒等）*/
    uint32_t body_vm, mark_vm;     /* 皮 / 头顶飞碟 */
    /* ★★ 2026-09-24 第 18 轮：宿主**全部 16 个 anm 槽**的原色都要存 —— 原来只隐槽 0（本体），
     *   而 TH18 的大敌人把**火焰/光环挂在槽 1**（`GirlD01` 的 ECL 写 `anmSetSprite(1, 105)`，
     *   而脚本 105 = `layer(11)` + 加算 + 子脚本 106/107 = sprite152..159 的 8 帧火焰）
     *   ⇒ 只隐槽 0 时，"背后的火焰"照样在（owner 实跑：「怎么画出来都有火焰，包括没改的那一只」）。*/
    uint32_t native_vm[UFO_CARRIER_SLOTS];
    uint32_t native_color[UFO_CARRIER_SLOTS];
    uint32_t seen_tick;
} ufo_carrier_t;

static ufo_carrier_t s_carriers[UFO_CARRIER_TRACK_MAX];
/* ★★ 2026-09-24 第 17 轮：**选宿主改成"落地即选、不再等一波结束"**（owner 实跑两条：
 *   ①「队列中**前两个**妖精变了（考虑对称总共 4 个）」②「走到一半的路程，**倒数第三个突然变了**」）。
 *   真因：上一版是"一波结束时才判" ⇒ ①一波里**分几拍刷**的（或跨 MainSub 的）被拆成几组、每组各挑一次；
 *   ②判定发生在**飞行途中** ⇒ 妖精飞一半突然换皮。
 *   现在：**每一组的第一只到达时立刻成为宿主**（+ 它后来的**镜像**），组只在"波号变了 / 静默超时"时关闭；
 *   选择不再依赖"波结束"，所以没有"半途突变"。
 *   ⚠ 另一条实跑观察（owner ①）：用"**最后**到达的那只当宿主"时，画面上变的是**队列最前面**那只
 *   ⇒ **创建顺序与队列顺序相反** ⇒ 取"**第一只**到达的"才是 owner 要的「**排在队列最后**」。
 *   （ENEMY_ID 单调递增 = 创建顺序；这里用"第一次见到"的顺序近似它，同一帧内多只按遍历顺序。）*/
static int32_t s_grp_wave = -2;            /* 当前组的波号（−2 = 没开组、−1 = 读不到）*/
static int32_t s_grp_id;                   /* 组里第一只（= 宿主 #1）*/
static int16_t s_grp_n, s_grp_s;
static float   s_grp_x, s_grp_y;
static uint8_t s_grp_mirror;               /* 已经选到镜像了 */
static uint32_t s_grp_last_arrival;
static unsigned s_grp_count;               /* 组里见过几只（纯日志）*/
static uint32_t s_carrier_tick_no;
static unsigned s_carrier_live, s_carrier_peak, s_carrier_fail, s_carrier_hidden,
                s_carrier_waves, s_carrier_plain;      /* plain = 同波但没被选中的那些 */

/* 宿主判据：`(N,S)` 命中宿主表 ⇒ 返回档位；否则 −1（表本身已经把"中妖精家族"剔掉了）。*/
static int ufo_carrier_kind_of(const uint8_t *e)
{
    int32_t n, s;
    int i;
    if (!e) return -1;
    n = *(const int32_t *)(e + CE_ENEMY_ANM_SRC);
    s = *(const int32_t *)(e + CE_ENEMY_ANM_SCRIPT);
    for (i = 0; i < UFO_CARRIER_KEY_COUNT; ++i)
        if (UFO_CARRIER_KEYS[i].n == n && UFO_CARRIER_KEYS[i].s == s)
            return (int)UFO_CARRIER_KEYS[i].kind;
    return -1;
}

static float ufo_carrier_skin_scale(const uint8_t *e, int kind)
{
    int32_t n, s;
    int i;
    if (!e) return 1.0f;
    n = *(const int32_t *)(e + CE_ENEMY_ANM_SRC);
    s = *(const int32_t *)(e + CE_ENEMY_ANM_SCRIPT);
    for (i = 0; i < UFO_CARRIER_KEY_COUNT; ++i)
        if (UFO_CARRIER_KEYS[i].n == n && UFO_CARRIER_KEYS[i].s == s &&
            (int)UFO_CARRIER_KEYS[i].kind == kind)
            return (float)UFO_CARRIER_KEYS[i].skin_scale_permille / 1000.0f;
    return 1.0f;
}

/* 皮的脚本：蝴蝶那一档只有一支（一手 script45 本身就是左右两套 sprite 的表）；
 * 妖精那一档**按飞行方向挑**（owner 第 17 轮：「左右的动态没有复刻」）：
 *   0 = 正面走位（脚本 352 ← 一手 script15）／1 = 向右（脚本 356 ← 一手 script17）／2 = 向左（357 ← script16）。*/
static int ufo_carrier_body_script_k(int kind, int facing)
{
    if (kind == (int)UFO_CARRIER_KIND_BFLY) return CE_ANM_ABILITY_SCRIPT_UFO_CARRIER_BUTTERFLY;
    switch (facing) {
    case 1:  return CE_ANM_ABILITY_SCRIPT_UFO_CARRIER_FAIRY_SIDE_R;
    case 2:  return CE_ANM_ABILITY_SCRIPT_UFO_CARRIER_FAIRY_SIDE_L;
    default: return CE_ANM_ABILITY_SCRIPT_UFO_CARRIER_FAIRY;
    }
}

static ufo_carrier_t *ufo_carrier_find(int32_t id)
{
    int i;
    if (!id) return 0;
    for (i = 0; i < UFO_CARRIER_TRACK_MAX; ++i)
        if (s_carriers[i].used && s_carriers[i].enemy_id == id) return &s_carriers[i];
    return 0;
}

/* 还原宿主原生贴图的颜色（幂等；句柄/原值都清掉）。 */
static void ufo_carrier_unhide(ufo_carrier_t *c)
{
#if UFO_CARRIER_HIDE_NATIVE
    int k;
    for (k = 0; k < UFO_CARRIER_SLOTS; ++k) {
        if (c->native_vm[k] && c->native_color[k]) {
            uint8_t *vm = ce_anm_get_vm(c->native_vm[k]);
            if (vm) *(uint32_t *)(vm + CE_ANM_VM_COLOR1) = c->native_color[k];
            if (s_carrier_hidden) --s_carrier_hidden;
        }
        c->native_vm[k] = 0;
        c->native_color[k] = 0;
    }
#endif
}

/* 收掉一个跟踪槽（皮 + 标记 + 还原原生贴图）。幂等。 */
static void ufo_carrier_release(ufo_carrier_t *c)
{
    if (!c || !c->used) return;
    ufo_carrier_unhide(c);
    if (c->body_vm) { ce_anm_delete(c->body_vm); c->body_vm = 0; }
    if (c->mark_vm) { ce_anm_delete(c->mark_vm); c->mark_vm = 0; }
    if (c->carrier && s_carrier_live) --s_carrier_live;
    c->used = 0;
    c->carrier = 0;
    c->enemy_id = 0;
    c->kind = 0;
}

/* 全部收掉（删卡 / 换关 / run reset 那条路 —— 见 `ufo_withdraw_all`）。顺手报一条每批一次的对账。 */
static void ufo_carrier_kill_all(void)
{
    int i;
    for (i = 0; i < UFO_CARRIER_TRACK_MAX; ++i) ufo_carrier_release(&s_carriers[i]);
    s_grp_wave = -2;
    s_grp_id = 0;
    s_grp_mirror = 0;
    s_grp_count = 0;
    s_stage_script_id = 0;      /* 换关/删卡 ⇒ 关卡脚本的句柄与波号一起作废（下帧重新认）*/
    s_stage_wave = -1;
    if (s_carrier_peak || s_carrier_fail || s_carrier_hidden || s_carrier_waves)
        ce_log("ufo: carrier skins released - %u wave(s) judged, peak %u host(s) at once, %u same-wave "
               "fairy(ies) left plain, %u refused (track full, %d max), %u native sprite(s) still hidden "
               "after release (must be 0)",
               s_carrier_waves, s_carrier_peak, s_carrier_plain, s_carrier_fail,
               (int)UFO_CARRIER_TRACK_MAX, s_carrier_hidden);
    s_carrier_peak = 0;
    s_carrier_fail = 0;
    s_carrier_waves = 0;
    s_carrier_plain = 0;
}

/* 组关闭（只在日志里交代一声；**选宿主不在这里做** —— 已经"落地即选"了）。 */
static void ufo_carrier_group_close(const char *why)
{
    if (s_grp_wave == -2) return;
    ++s_carrier_waves;
    if (s_grp_count > 0)
        ce_log("ufo: carrier wave closed (%s) - MainSub %d, %u candidate(s) seen, host %d%s "
               "(owner: one per wave, two when symmetric, the LAST in the queue)",
               why, (int)s_grp_wave, s_grp_count, (int)s_grp_id, s_grp_mirror ? " + its mirror" : "");
    s_grp_wave = -2;
    s_grp_id = 0;
    s_grp_mirror = 0;
    s_grp_count = 0;
}

/* 新到一只候选：决定它是不是宿主。返回 1 = 它成了宿主（要起皮）。 */
static int ufo_carrier_arrival(ufo_carrier_t *c, float x, float y)
{
    int32_t w = (s_stage_wave >= 0) ? s_stage_wave : -1;
    int new_group;
    if (s_grp_wave == -2)                       new_group = 1;
    else if (w >= 0 && s_grp_wave >= 0)         new_group = (w != s_grp_wave);   /* 有波号：只看波号 */
    else                                        new_group = (s_carrier_tick_no - s_grp_last_arrival) > UFO_CARRIER_WAVE_GAP;
    ++s_grp_count;
    s_grp_last_arrival = s_carrier_tick_no;
    if (new_group) {
        if (s_grp_wave != -2) ufo_carrier_group_close("next wave");
        s_grp_wave = w;
        s_grp_id = c->enemy_id;
        s_grp_n = c->n;
        s_grp_s = c->s;
        s_grp_x = x;
        s_grp_y = y;
        s_grp_mirror = 0;
        s_grp_count = 1;
        return 1;                               /* ★ 组里第一只 = 宿主 #1（= 队列最后那只）*/
    }
    if (UFO_CARRIER_PER_WAVE >= 2 && !s_grp_mirror && c->n == s_grp_n && c->s == s_grp_s) {
        float dx = x + s_grp_x, dy = y - s_grp_y;
        if (dx < 0.0f) dx = -dx;
        if (dy < 0.0f) dy = -dy;
        if (dx <= UFO_CARRIER_SYM_XTOL && dy <= UFO_CARRIER_SYM_YTOL) {
            s_grp_mirror = 1;
            return 1;                           /* ★ 它的**镜像伙伴** = 宿主 #2 */
        }
    }
    return 0;                                   /* 同波其余同类妖精：原样、不掉 */
}

/* 死亡回调里的那一问：**这一只是不是宿主**。
 * ⚠ 两种情况要看死人的顺序：① 这一波还没结束（玩家把"队列最后那只"提前打死了 —— 一手里那只就是宿主）；
 * ② 刚出生就被打死（我们还没扫到过它）⇒ 把它所在的那一波**当场判掉**再问一次。
 * 这样"宿主 = 本波最后到达的那一只"这条规则不会因为死亡时机而漏掉。 */
static int ufo_carrier_host_now(int32_t id)
{
    ufo_carrier_t *c = ufo_carrier_find(id);
    return (c && c->carrier) ? 1 : 0;       /* 选宿主是"落地即定"的 ⇒ 死亡时只查一下记录 */
}

/* ★ 每游玩帧扫一遍敌机（调用点 = `on_item_frame`，与本体跟随 VM 同一拍）。
 * ⚠ 这里会 `ce_anm_spawn` ⇒ 绝不能放在死亡栈上（本调用点合规：`on_item_frame` 是每帧门卫位次）。*/
static void ufo_carrier_tick(void)
{
    uint8_t *em = (uint8_t *)CE_ENEMY_MGR();
    uint8_t *e;
    int i;
    ++s_carrier_tick_no;
#if !UFO_CARRIER_SKIN_ENABLE
    ufo_carrier_kill_all();
    return;
#else
    if (!em) return;
    {   /* ★ 本帧的 ECL 波号（MainSub 数字）。读得到就更新 sticky，读不到沿用上一次 —— 与 117 同法。 */
        int32_t w = ufo_stage_wave();
        if (w >= 0) s_stage_wave = w;
    }
    CE_ENEMY_FOREACH(em, e) {
        int kind = ufo_carrier_kind_of(e);
        int32_t id;
        ufo_carrier_t *c;
        if (kind < 0) continue;                        /* 不是宿主外观：一个字节都不碰 */
        /* ★★ 2026-09-24（110/111 ↔ 112 的妖精冲突护栏，owner 口径「**别同时操作同一只妖精的 VM**」）：
         *   110/111 的绿妖精换皮每帧重申 `ce_fairy_claim_group(card_id, N, S)`（**整组**换皮），
         *   我们在这里**同帧只读**：命中就**当场放弃这一只**（不登记、不发皮、不掉令牌）。
         *   ⚠ 必须在 `!c`（第一次见到）之前就跳过 —— 否则被认领的那只会**顶掉本波"第一只"的位置**，
         *     本波就少一个宿主。定义与语义见 `native/sdk.h`（他们登记在 `AUDIT.md` §BE 详录 #86）。*/
        if (ce_fairy_group_claimed(*(const int32_t *)(e + CE_ENEMY_ANM_SRC),
                                   *(const int32_t *)(e + CE_ENEMY_ANM_SCRIPT)))
            continue;
        id = *(const int32_t *)(e + CE_ENEMY_ID);
        c = ufo_carrier_find(id);
        if (!c) {                                      /* 第一次见到这一只 ⇒ 只登记为"本波候选" */
            for (i = 0; i < UFO_CARRIER_TRACK_MAX; ++i)
                if (!s_carriers[i].used) { c = &s_carriers[i]; break; }
            if (!c) { ++s_carrier_fail; continue; }
            memset(c, 0, sizeof(*c));
            c->used = 1;
            c->enemy_id = id;
            c->kind = (uint8_t)kind;
            c->n = (int16_t)*(const int32_t *)(e + CE_ENEMY_ANM_SRC);
            c->s = (int16_t)*(const int32_t *)(e + CE_ENEMY_ANM_SCRIPT);
            c->skin_scale = ufo_carrier_skin_scale(e, kind);
            /* ★ 分组键 = **ECL 自己的波号**（MainSub 数字），组里的**第一只立刻当宿主** + 后来的镜像。 */
            if (ufo_carrier_arrival(c, *(const float *)(e + CE_ENEMY_POS_X),
                                       *(const float *)(e + CE_ENEMY_POS_Y))) {
                c->carrier = 1;
                ++s_carrier_live;
                if (s_carrier_live > s_carrier_peak) s_carrier_peak = s_carrier_live;
            } else {
                ++s_carrier_plain;
            }
        }
        c->seen_tick = s_carrier_tick_no;
        if (!c->carrier) continue;                     /* 没被选中 ⇒ 保持原生贴图、不掉令牌 */
        /* ★ 万一我们在它落地时先选了它、其后 110/111 才把它那一组认领（护栏是帧内有效的通报）
         *   ⇒ **当场撤皮 + 撤掉宿主资格**（`carrier` 清掉 ⇒ 死亡回调也不会掉令牌）。 */
        if (ce_fairy_group_claimed(c->n, c->s)) {
            ce_log("ufo: carrier skin OFF - enemy_id %d (N %d, S %d) is claimed by card 110/111 "
                   "(ce_fairy_group_claimed) - skin released, no token drop (owner: never touch the "
                   "same fairy's VM from two cards)",
                   (int)c->enemy_id, (int)c->n, (int)c->s);
            ufo_carrier_release(c);
            continue;
        }
        /* —— 被选中的宿主：起皮（第一次进来时）+ 每帧跟随 + 隐掉原生贴图 —— */
        {   /* ★ 第 17 轮：按**飞行方向**挑皮的那一支（正面 352 / 向右 356 / 向左 357）——
             * owner 实跑：「左右的动态没有复刻」⇒ 妖精得跟着自己的飞行方向转身（一手 script15/16/17）。
             * 方向变了就**重起一次皮 VM**（走位那一段会在新脚本里从头播一遍，观感与原版一致）；
             * 死区 0.5 单位/帧，避免飘着飘着左右抖。 */
            float x_now = *(const float *)(e + CE_ENEMY_POS_X);
            int facing = c->facing;
            if (c->have_last_x) {
                float dx = x_now - c->last_x;
                if (dx > 0.5f)       facing = 1;
                else if (dx < -0.5f) facing = 2;
            }
            c->last_x = x_now;
            c->have_last_x = 1;
            if (!c->body_vm || facing != c->facing) {
                if (c->body_vm) { ce_anm_delete(c->body_vm); c->body_vm = 0; }
                c->facing = (uint8_t)facing;
                c->body_vm = ce_anm_spawn(CE_ABILITY_ANM(), ufo_carrier_body_script_k(c->kind, facing),
                                          UFO_CARRIER_BODY_LAYER);
                if (c->body_vm && c->skin_scale != 1.0f)
                    ce_anm_set_scale(c->body_vm, c->skin_scale, c->skin_scale);
            }
        }
        if (!c->mark_vm)
            c->mark_vm = ce_anm_spawn(CE_ABILITY_ANM(), CE_ANM_ABILITY_SCRIPT_UFO_CARRIER_MARK,
                                      UFO_CARRIER_MARK_LAYER);
        if (!c->logged) {
            c->logged = 1;
            ce_log("ufo: carrier skin ON - enemy_id %d (N %d, S %d) -> %s x%.2f; body script %d + mark script %d "
                   "(owner: the skin MEANS it carries a token)",
                   (int)c->enemy_id, (int)c->n, (int)c->s,
                   c->kind == (uint8_t)UFO_CARRIER_KIND_BFLY ? "BFLY (sprite125..132 蝴蝶)" : "FAIRY (sprite36..47 妖精)",
                   (double)c->skin_scale, ufo_carrier_body_script_k(c->kind, (int)c->facing),
                   CE_ANM_ABILITY_SCRIPT_UFO_CARRIER_MARK);
        }
        {
            float x = *(const float *)(e + CE_ENEMY_POS_X);
            float y = *(const float *)(e + CE_ENEMY_POS_Y);
            if (c->body_vm) {
                ce_anm_set_pos(c->body_vm, x, y, 0.0f);
                if (c->skin_scale != 1.0f)
                    ce_anm_set_scale(c->body_vm, c->skin_scale, c->skin_scale);
            }
            if (c->mark_vm) ce_anm_set_pos(c->mark_vm, x, y, 0.0f);
        }
#if UFO_CARRIER_HIDE_NATIVE
        {   /* 隐掉原生贴图：**每帧**写（宿主的 anm 脚本可能自己在跑 alpha/color 插值）。
             * ★ 第 18 轮：**槽 0 = 本体、槽 1 = 火焰/光环**（大敌人的 `anmSetSprite(1, 105)`），
             *   两槽一起隐 —— 否则换完皮背后还挂着一团原生火（owner 实跑点名的那件事）。*/
            int k;
            for (k = 0; k < UFO_CARRIER_SLOTS; ++k) {
                uint32_t nv;
                if (k > 0 && !UFO_CARRIER_HIDE_SLOT1) break;
                nv = *(const uint32_t *)(e + CE_ENEMY_DATA + 0x124 + k * 4);   /* `anm_vm_ids[k]` */
                if (nv != c->native_vm[k]) {
                    uint8_t *vm = nv ? ce_anm_get_vm(nv) : 0;
                    if (c->native_vm[k] && c->native_color[k]) {               /* 旧的那只还回去 */
                        uint8_t *old = ce_anm_get_vm(c->native_vm[k]);
                        if (old) *(uint32_t *)(old + CE_ANM_VM_COLOR1) = c->native_color[k];
                        if (s_carrier_hidden) --s_carrier_hidden;
                    }
                    c->native_vm[k] = nv;
                    c->native_color[k] = vm ? *(const uint32_t *)(vm + CE_ANM_VM_COLOR1) : 0;
                    if (vm) ++s_carrier_hidden;
                }
                if (c->native_vm[k]) {
                    uint8_t *vm = ce_anm_get_vm(c->native_vm[k]);
                    if (vm) *(uint32_t *)(vm + CE_ANM_VM_COLOR1) = UFO_CARRIER_ALPHA_HIDDEN;
                }
            }
        }
#endif
    }
    /* 组静默超时 ⇒ 关组（只为日志与状态复位；宿主早就选好了）。*/
    {
        uint32_t quiet = (s_grp_wave >= 0) ? UFO_CARRIER_WAVE_SETTLE : UFO_CARRIER_WAVE_GAP;
        if (s_grp_wave != -2 && (s_carrier_tick_no - s_grp_last_arrival) > quiet)
            ufo_carrier_group_close("quiet");
    }
    /* 这一帧没见到的（死了 / 飞走了 / 换关清了场）⇒ 收皮 + 还原颜色 */
    for (i = 0; i < UFO_CARRIER_TRACK_MAX; ++i)
        if (s_carriers[i].used && s_carriers[i].seen_tick != s_carrier_tick_no)
            ufo_carrier_release(&s_carriers[i]);
#endif  /* UFO_CARRIER_SKIN_ENABLE */
}

static void ufo_summon_fx_try(ufo_card_t *s, const uint8_t *e)
{
    void *anm = CE_ABILITY_ANM();
    if (!s || !e || !anm) return;
    /* VM id 会被引擎复用 ⇒ 陈旧句柄先作废（脚本 324 在 +75 帧自删；不清的话下一只 UFO 的 VM
     * 恰好复用同一个 id 时，「上一只还在放」那条门会误判成真 ⇒ 新 UFO 再也不出横幅）。*/
    if (s_summon_fx_id && !ce_anm_get_vm(s_summon_fx_id)) s_summon_fx_id = 0;
    if (s_summon_debris_id && !ce_anm_get_vm(s_summon_debris_id)) s_summon_debris_id = 0;
    /* ★ 2026-09-24：这一道门改成**只看「这只 UFO 起过没有」**（原来还要求"横幅 VM 还活着"）。
     *   为什么改：现在一次召唤要起**两件**东西（横幅 75 帧 / 碎屑 ~90 帧），若仍用"横幅还在放就
     *   整条返回"的写法，第二只 UFO 来得快时连碎屑也起不来 —— 而两件东西的时长并不相等。 */
    if (s->u.ufo_id && s->summon_fx_for == s->u.ufo_id) return;      /* 这只已经起过两件 */
    s->summon_fx_for = s->u.ufo_id;
    ufo_summon_debris_spawn(e);                                      /* ★ 闪电碎屑（脚本 346）*/
    if (s_summon_fx_id && ce_anm_get_vm(s_summon_fx_id)) return;     /* 上一只的横幅还在放 ⇒ 不叠 */
    s_summon_fx_id = ce_anm_spawn(anm, CE_ANM_ABILITY_SCRIPT_UFO_SUMMON_BANNER, UFO_SUMMON_FX_LAYER);
    if (!s_summon_fx_id) {
        ce_log("ufo: summon banner spawn FAILED (script %d, layer %d) - no summon animation this time",
               CE_ANM_ABILITY_SCRIPT_UFO_SUMMON_BANNER, UFO_SUMMON_FX_LAYER);
        return;
    }
    ce_anm_set_pos(s_summon_fx_id, UFO_SUMMON_FX_DX, UFO_SUMMON_FX_DY, 0.0f);
    /* 颜色 = 一手那一条 `pending_interrupt`；`ufo_color_art_index` 就是它的本仓实现
     * （红 0 / 蓝 1 / 金 1 / 绿 2 / 彩虹 3，`ufo_core.c`）⇒ 标签 = 7 + idx。 */
    {
        int idx = ufo_color_art_index(s->u.color);
        if (idx >= 0) ce_anm_interrupt(s_summon_fx_id, 7 + idx);
        ce_log("ufo: summon banner up (ability.anm script %d <= TH12 front.anm script82 + 83 + 84; "
               "color %d idx %d -> interrupt %d; world layer %d, pos (%.0f, %.0f), size x0.%03u "
               "(owner ruling; peak = x0.%03u), self-deletes at +75)",
               CE_ANM_ABILITY_SCRIPT_UFO_SUMMON_BANNER, (int)s->u.color, idx,
               idx >= 0 ? 7 + idx : -1, UFO_SUMMON_FX_LAYER,
               (double)UFO_SUMMON_FX_DX, (double)UFO_SUMMON_FX_DY,
               (unsigned)UFO_SUMMON_FX_SCALE_PERMILLE,
               (unsigned)(UFO_SUMMON_FX_SCALE_PERMILLE * 2u));
    }
}

static void ufo_body_vm_follow(ufo_card_t *s)
{
    uint8_t *e = ufo_enemy_by_id(s->u.ufo_id);
    if (!e) {                                          /* 本体不在了 ⇒ 收 VM（静默消失也要收）*/
        ufo_body_vm_kill(s);
        return;
    }
    if (!s->body_anm_id || !ce_anm_get_vm(s->body_anm_id)) {
        s->body_anm_id = 0;
        ufo_body_vm_spawn(s, s->u.color, e);
        ufo_summon_fx_try(s, e);                       /* ★ 召唤横幅（每只 UFO 一次，脚本 324）*/
        return;                                        /* spawn 里已经摆好位置 */
    }
    ce_anm_set_pos(s->body_anm_id, *(const float *)(e + CE_ENEMY_POS_X),
                                   *(const float *)(e + CE_ENEMY_POS_Y), 0.0f);
}

/* ==================== ★★ 实体上的两件 UI（用户点名：血量 UI + 照原版样式）====================
 * 需求 ① **环绕实体右侧的吸收进度环**（按 `fill` 双向填充）、需求 ② **实体正上方的血量条**
 * （居中、只有进度、**没有框**）。两件都是**世界层 VM**，每帧跟随 UFO 本体敌机。
 *
 * **一手（全部可复核；TH12 v1.00b）**
 *   ① 环 = `bullet.anm` **script206**（光环树 script207 的第三个子脚本），卡侧脚本 = `ability.anm` **228**
 *      （逐条照抄，只把 `sprite(sprite404)` 换成 `@UFO_ARC_STRIP@`，并补了「描边挂到弧下面」的两条
 *       `scriptNew`，登记见该脚本的注释）。两条静态描边 = script204/205 ⇒ 卡侧脚本 **229 / 230**。
 *      · 半径：`scale(6.0f,59.0f)` ⇒ 外 62 / 内 56。
 *      · 弧的模式位：`iset($I0,32); iset($I1,2); ins_101($I0)` —— 一手 opcode **101**，
 *        TH18 handler `th18:0x47b211` 是 `and eax,0xb7ffffff; or eax,0x34000000` ⇒ `flags_lo>>26` = **13**
 *        = v8 助记符 **`texArcEven`**（v8.anmm 的 601；`602 texArc` = mode 14 是 `or 0x38000000`）；
 *        参数 = 弧的**段数**（原生 32 ⇒ 观感粗糙，照抄不改）。
 *      · `rotation.z = 1.5707964` = π/2：屏幕 y 向下 ⇒ π/2 是**6 点**；mode 13 以 base 为中心向两侧展开
 *        ⇒ fill→1 时两端在 **12 点**闭合 —— 这就是「从 6 点双向填充」的来源。
 *   ② 进度每帧由引擎写 `rotation.x = fill × 2π`（`th12:0x44a499`–`0x44a4c7`：
 *      `fld [zUfo+0x50]` / `fmul qword [0x4a3cd0]` = 6.2831854820251465 / `fstp [vm+0x430]`，
 *      同一段还 `or [vm+0x47c],4` 置脏位）。TH18 的 `rotate` handler（`th18:0x47a13b`）不置脏位
 *      ⇒ 卡侧每帧写 `rotation.x` 同样即时生效（顶点在 draw 时按 live 字段重算）。
 *   ③ 条 = `front.anm` **script136**（卡侧脚本 **231**），子 **137** = 1 px 黑投影（卡侧脚本 **232**）；
 *      引擎每帧写 `scale.x = life.current / life.maximum`（`th12:0x44a801`–`0x44a814`，读
 *      `enemy+0x2648/+0x264c`）+ `entity_pos = 敌人坐标`（`th12:0x44a83e`–`0x44a849`，**不加偏移**）。
 *      ⚠ 原生那口「框」是**另一条 VM**（script134 = `0x86`、`sprite102` = `front_ufo.png` 的 96×96）——
 *      用户口径明确「只有进度、没有框」⇒ **本卡不起 134**。
 *
 * **层号（世界层；理由在每个脚本的注释里）**：环 / 两条描边 = **7**、条 = **10**、投影 = **9**
 *   —— 三条都是**原生原值**，且都落在 TH18 的世界层区间 3–19（相机 3 = 游玩区，
 *   `engine/anm/th18/02-render-stages.md` §0/§1）。**不是** HUD 的 24/25（外围 UI 列表 + 暂停模糊）：
 *   实体身边的物件进 24/25 就会丢掉「跟着实体在游玩区里动」这个空间关系。
 *   先例：脚本 223（五边形框）的 `layer(12)`、本体脚本 164..167 的 `layer(7)`，都是从原生原样搬的层号。
 *   ⚠ `ce_anm_spawn` 的 layer 参数会被脚本里的 `layer(n)` 覆盖，这里仍写同一个值（可读 + 与本体一致）。
 *
 * **收尾**（METHOD 铁律）：三件都在文件静态区（同 `s_penta_id` 的先例），
 *   `ufo_withdraw_all()` → `ufo_gauge_kill()` 一条路收干净 —— 删卡那一刻 tick 全停，
 *   只有这条路跑得到；`on_run_reset` / `on_delete` 汇到它（**换关不走这条路**，见 `on_stage_start`）。 */

/* ★★★ 2026-09-22 定案（P3 #7「吸收环明显偏移 UFO 中心，UFO 中心在吸收环的 10 点方向」）：
 * **环的偏移 = (0, 0) —— 它与 UFO 同心**。此前写 `(32, 16)` 是把**坐标空间换算**的两项当成了偏移。
 *
 * 一手（两条路径写的是同一个字段 `vm+0x430`，逐指令读出来）：
 *   · 召唤那一帧 `th12:0x44aa8e`–`0x44aab4`：`fld [enemy+0x1074]`（x）→ `fadd [0x4a3d70]`
 *     → `fadd [0x4a3d28]` → `fstp [vm+0x430]`；y 同理只加一项 `[0x4a3d68]`；z 原样。
 *   · 每帧跟随 `th12:0x44a42a`–`0x44a45f`：与上面**逐条同形**（同两个常量、同三格）。
 *   · 三个常量的**值**（PE 节映射后直读 double）：`0x4a3d70` = **32.0**、`0x4a3d28` = **192.0**、
 *     `0x4a3d68` = **16.0**。
 *
 * ⇒ `192` + `16` 是 **TH12 那套 anm 世界空间的换算项**，不是偏移：
 *   TH12 的 `front.anm` / `bullet.anm` 世界空间里，**游玩区占 x ∈ [32, 416]、y ∈ [16, 464]**
 *   （0 = 左边框再往左 32 px、上边框再往上 16 px），而敌人自己的 ECL 坐标是「x 以场地中心为原点、
 *   y 从场地上沿起算」。把敌人的 ECL 坐标折进那个空间**恰好**要 +192（半宽）+32（左内缩）、+16（上内缩）
 *   ⇒ `敌人 + (32+192, 16)` 与「环画在敌人身上」是**同一个点**。
 *
 * 三条独立印证（都不是"看着像"）：
 *  ① 同族脚本的**位置语义**：`front.anm` script82（UFO 出现大横幅，304×32）写 `pos(224, 208)`
 *     = 上面那个空间里的 **x 224** = 场地中心（32 + 384/2）✓、y 208 = 16 + 192 ✓（横幅居中）；
 *     script79（小横幅，96×16）`pos(-96, 467)` → `posTime(60,4, 32, 467)`：起点 = 那个空间的
 *     x 0…… 而 x=32 正是**场地左沿**、y=467 正好在**场地下沿（464）下 3 px** ⇒ 两处都对得上。
 *  ② **同一个敌人的另一件跟随物**：血条 VM（`front.anm` script136）由 `th12:0x44a83e`–`0x44a849`
 *     调 `0x461e30(&zUfo+0x44, &enemy.x)` 摆位 —— **与环同一条换算、同一个空间**，其脚本
 *     `pos(0,-28)` 让条落在敌人上方 28 单位。本卡的血条一直写的是 `(0, -28)`（`UFO_GAUGE_BAR_DY`），
 *     实跑从没被报过偏移 ⇒ 同一个空间里的环就该是 `(0, 0)`（差 32/16 的那一版**必然**偏）。
 *  ③ 用户实跑的方位与量级：`(32, 16)` 在世界单位下 = 表面 **(64, 32) px**（1 单位 = 2 表面 px），
 *     即环心在 UFO 的**右下** ⇒ 反过来说 UFO 落在环心的**左上 = 10 点方向** —— 与用户原话逐字吻合。
 *
 * 复查入口：`objdump -d --start-address=0x44a412 --stop-address=0x44a460 -M intel th12.exe`（每帧那一段）、
 * `0x44aa8e`–`0x44aab4`（召唤那一段）；常量用 PE 节映射直读（`.rdata` 文件偏移 = VA − 0x498000 + 0x97000）。
 * ⚠ 别再把它改成 `224`（那是"读法 A"）：224 是**换算项之和**，不是偏移 —— 写成 224 环会整个飘到右侧。 */
#define UFO_GAUGE_RING_DX       0.0f      /* ✅ 一手：与敌人同心（见上，32 + 192 都是空间换算项）*/
#define UFO_GAUGE_RING_DY       0.0f      /* ✅ 一手：同上（`0x4a3d68` = 16.0 也是空间换算项）*/
#define UFO_GAUGE_BAR_DY     (-28.0f)     /* 一手：脚本 231（TH12 `front.anm` script136）的 `pos(0,-28,0)` */
#define UFO_GAUGE_LAYER_RING   7          /* 一手：TH12 script206/204/205 自己写的 `layer(7)` */
#define UFO_GAUGE_LAYER_BAR   10          /* 一手：TH12 script136 自己写的 `layer(10)` */
#define UFO_GAUGE_BAR_FULL_W  32.0f       /* 一手：脚本 231 的 `drawRect(32.0f, 4.0f)` 的**宽**。
                                           * ★★ 注意：这是**脚本里已经写死的矩形尺寸**（世界单位），
                                           *   **不是** scale 值 —— 卡侧每帧写的 scale.x 是
                                           *   **比例 0..1**（`life.cur/life.max`，一手 `th12:0x44a801`）。
                                           *   2026-09-18 之前这里被当成 scale 乘了一次 ⇒ 条长 32 倍
                                           *   （用户实跑 B3「血条显示贼长」）。保留这个常量只为
                                           *   记录脚本原值，**不要再拿它去 set_scale**。 */
#define UFO_GAUGE_BAR_H        4.0f       /* 一手：同上（`4.0f`）。同一条警告：脚本尺寸 ≠ scale；
                                           *   scale.y 必须保持 1（引擎一个字节都不写它）。 */
#define UFO_GAUGE_BAR_COLOR 0xffff6060u   /* 一手：脚本 231 的 `color(255,96,96)` ⇒ D3DCOLOR 0xAARRGGBB */
/* π/2 = 环的 base 角（6 点方向）。**不许**在 .c 里算三角函数 / 跑浮点（x87 硬门）⇒ 写成常量。
 * 一手值 = 脚本 228 的 `rotate(3.1415927f, 0.0f, 1.5707964f)` 第三个参数（TH12 script206 原值）。 */
static const float k_ufo_gauge_pi_over_2 = 1.5707964f;

/* --- 脚本号：`make anm` 之前 `native/anm_ids.h` 里还没有这两条宏 ⇒ 走**字面量**；
 *     `make anm` 之后宏就存在了，下面的 `#ifdef` 自动切过去（**这一步不用手改**）。
 *     形状照抄同仓先例 `cards/youkai.c` 的 `YK_OPT_SCRIPT_*`（那里的 224..227 也是同一天落地的新脚本）。
 *     ⚠ 号 = 228..232 连续五条：228 环 / 229+230 环的两条描边（环的子 VM，C 不直接起）/
 *       231 血条 / 232 血条的 1 px 投影（条的子 VM，C 不直接起）。 */
#ifdef CE_ANM_ABILITY_SCRIPT_UFO_GAUGE_ARC
#define UFO_GAUGE_SCRIPT_ARC   CE_ANM_ABILITY_SCRIPT_UFO_GAUGE_ARC
#else
#define UFO_GAUGE_SCRIPT_ARC   228     /* `make anm` 后换宏（本行会被上面的 #ifdef 取代）*/
#endif
#ifdef CE_ANM_ABILITY_SCRIPT_UFO_GAUGE_BAR
#define UFO_GAUGE_SCRIPT_BAR   CE_ANM_ABILITY_SCRIPT_UFO_GAUGE_BAR
#else
#define UFO_GAUGE_SCRIPT_BAR   231     /* `make anm` 后换宏 */
#endif
#ifdef CE_ANM_ABILITY_SCRIPT_UFO_GAUGE_ARC_BORDER_OUT
#define UFO_GAUGE_SCRIPT_RING_OUT CE_ANM_ABILITY_SCRIPT_UFO_GAUGE_ARC_BORDER_OUT   /* 229 */
#else
#define UFO_GAUGE_SCRIPT_RING_OUT 229
#endif
#ifdef CE_ANM_ABILITY_SCRIPT_UFO_GAUGE_ARC_BORDER_IN
#define UFO_GAUGE_SCRIPT_RING_IN  CE_ANM_ABILITY_SCRIPT_UFO_GAUGE_ARC_BORDER_IN    /* 230 */
#else
#define UFO_GAUGE_SCRIPT_RING_IN  230
#endif

/* --- 四件 VM 的句柄（★ 文件静态，照 `s_penta_id` 的先例：`on_ctor` 的 memset 清不掉它 ⇒
 *     收尾**只能**走显式路径，这条纪律与本文件其它静态句柄完全一致）。
 * ★★ 2026-09-18 第二轮（用户实跑：「**环不是偏移，是贼 jb 大**，它确实是跟着飞碟动 —— 但是贼大，
 *   所以只在它快走的时候，我才看到露出的环」）⇒ 根因 = **父 scale 把子几何乘爆**：
 *   第二版把**填充弧**当父、让两条 `drawPolyBorder(62,48)` 描边当它的子（图省事没移植原生 207），
 *   而一手（`engine/anm/th16/09-drawing.md` §3.5 第 1 条，弧/圆/多边形三族共用）：
 *   **父非空且 `!(flags_hi & 0x10000)` 时 `r_out *= parent.scale.x`、`r_in *= parent.scale.y`**
 *   ⇒ 描边半径 = 62 × 父 scale(6 / 59) = **372 ～ 3658 世界单位**（≈744 ～ 7316 表面 px）
 *   ⇒ 屏幕上只剩「露出的」一小段巨大圆弧，而且**跟着敌人动**（父子位置继承）—— 与用户描述逐字吻合。
 *   ⇒ **修法 = 把原生那棵树摊平**：三条 C 侧各自起一只、各自摆在敌人身上、各自 `delete` ⇒ 没有父
 *   ⇒ 半径就是脚本原值 **62 / 55**（描边）与 **59**（填充弧）。原生树里 **207 的 scale 恰好是 1**，
 *   所以「没有父乘」正是原生效果 ✓（脚本 228 里那两行 `scriptNew` 已删，那边有同一份长注）。 */
static uint32_t s_ring_arc_id;                 /* 填充弧（脚本 228）*/
static uint32_t s_ring_border_id[2];           /* 环的两条静态描边（脚本 229 / 230，**独立 VM**）*/
static uint32_t s_gauge_bar_id;                /* 血条（脚本 231；它的 1 px 投影是它的 `scriptNew` 子 VM）*/

static void ufo_gauge_follow(ufo_card_t *s);   /* 定义在下面（spawn 里也调它摆一次位置）*/

/* 「满时的闪烁」（一手 `0x44ad94` 的 `anm_interrupt_by_id(vm3c, 2)`）—— 三条环各发一次。
 * 原生那一下是**递归子树**（`0x461970`）：204/205/206 全部进 `interruptLabel(2)` = 每 2 帧换色循环。
 * 树摊平之后必须显式发三条（少发哪一条，哪一条就不闪）。 */
#define UFO_GAUGE_INTERRUPT_FLASH 2
static void ufo_gauge_flash(void)
{
    int i;
    if (s_ring_arc_id) ce_anm_interrupt(s_ring_arc_id, UFO_GAUGE_INTERRUPT_FLASH);
    for (i = 0; i < 2; ++i)
        if (s_ring_border_id[i]) ce_anm_interrupt(s_ring_border_id[i], UFO_GAUGE_INTERRUPT_FLASH);
}

/* 收干净这几件（幂等：句柄清零）。**长持续效果铁律**：`ufo_withdraw_all()` 是唯一入口，
 * `on_run_reset` / `on_delete` 两条路汇到它（★ 2026-09-23：**换关不汇到它** —— 令牌跨关，见 `on_stage_start`）。
 * ⚠ **不用** `ce_anm_interrupt`：删卡那一刻 tick 全停，脚本里的 `interruptLabel(1)`（淡出 → `delete()`）
 *   根本跑不到 —— 直接 `ce_anm_delete`（血条的 `scriptNew` 子 VM 会随父一并清）。 */
static void ufo_gauge_kill(void)
{
    int i;
    if (s_ring_arc_id)  { ce_anm_delete(s_ring_arc_id);  s_ring_arc_id = 0; }
    for (i = 0; i < 2; ++i)
        if (s_ring_border_id[i]) { ce_anm_delete(s_ring_border_id[i]); s_ring_border_id[i] = 0; }
    if (s_gauge_bar_id) { ce_anm_delete(s_gauge_bar_id); s_gauge_bar_id = 0; }
}

/* 起这几件（在本体跟随 VM 之后调；建完立刻由 `ufo_gauge_follow` 摆一次位置 ⇒ 不闪一帧）。
 * `e` 只用于日志（位置统一由 follow 那一处算，避免「两处各算一次」的漂移）。
 * ★ 生成顺序 = 叠放顺序（同层后生成的画在上面）：两条描边先生成、填充弧最后（弧压在描边上，
 *   与原生树里 204 → 205 → 206 的次序一致 ✓）。 */
static void ufo_gauge_spawn(ufo_card_t *s, const uint8_t *e)
{
    void *anm = CE_ABILITY_ANM();

    if (!anm) return;
    ufo_gauge_kill();                                  /* 幂等：正常路径上已经是空的 */

    s_ring_border_id[0] = ce_anm_spawn(anm, UFO_GAUGE_SCRIPT_RING_OUT, UFO_GAUGE_LAYER_RING);
    s_ring_border_id[1] = ce_anm_spawn(anm, UFO_GAUGE_SCRIPT_RING_IN,  UFO_GAUGE_LAYER_RING);
    s_ring_arc_id       = ce_anm_spawn(anm, UFO_GAUGE_SCRIPT_ARC,      UFO_GAUGE_LAYER_RING);
    /* 血条：父 = 脚本 231 —— 1 px 投影（脚本 232）是它 `scriptNew` 出来的子 VM（脚本 231 的第一行）。 */
    s_gauge_bar_id      = ce_anm_spawn(anm, UFO_GAUGE_SCRIPT_BAR,      UFO_GAUGE_LAYER_BAR);

    if (!s_ring_arc_id || !s_ring_border_id[0] || !s_ring_border_id[1] || !s_gauge_bar_id)
        ce_log("ufo: gauge VM spawn FAILED (arc %u, borders %u/%u, bar %u; scripts %d/%d/%d/%d, "
               "layers %d/%d, enemy %p) - the entity gauges stay invisible this summon "
               "(assets/ability/scripts/228..231_*)",
               s_ring_arc_id, s_ring_border_id[0], s_ring_border_id[1], s_gauge_bar_id,
               UFO_GAUGE_SCRIPT_ARC, UFO_GAUGE_SCRIPT_RING_OUT, UFO_GAUGE_SCRIPT_RING_IN,
               UFO_GAUGE_SCRIPT_BAR, UFO_GAUGE_LAYER_RING, UFO_GAUGE_LAYER_BAR, (const void *)e);

    ufo_gauge_follow(s);                               /* 摆好位置 / 颜色 / 角度（唯一的摆位点）*/
}

/* ★★ 2026-09-23（owner 实跑第 11 条）：「**UFO 存在的时候一直有音效 `se_ufoalert.wav`**」。
 *   一手：TH12 `Ufo::spawn` 的第二声（`th12:0x44a9fe mov eax,0x36` + `0x44aa06 call 0x453e20`，在飞碟的 x），
 *   **且那一行是循环音**（TH12 cfg 第 0x36 行 `+0x0c` = 1；wav 本身只有 ~0.5 s）⇒ 原版是"一直响到 UFO 走"。
 *   ⚠ 本仓 SDK **没有停循环音的通路**（`ce_play_sound` 是唯一出口；TH18 的循环位只有原生 0x14/0x37 两行）
 *   ⇒ 用**手动循环**：UFO 在场期间每 `UFO_SE_ALERT_PERIOD` 帧重放一次（0.5 s 的 wav × 28 帧 ≈ 无缝），
 *   UFO 一消失就**自然停**（不再调用 = 没有下一声）。
 *   ❓自定：周期 28 帧（不改循环位、不新增断点；宁可听感上略有缝，也不要一个停不掉的循环）。 */
#define UFO_SE_ALERT_PERIOD 28u
static uint16_t s_alert_left;                  /* 距离下一声还有几帧（0 = 立刻）*/

static void ufo_alert_loop_tick(ufo_card_t *s)
{
    uint8_t *e;
    if (!s || !ufo_present(&s->u)) { s_alert_left = 0; return; }
    e = ufo_enemy_by_id(s->u.ufo_id);
    if (!e) { s_alert_left = 0; return; }
    if (s_alert_left) { --s_alert_left; return; }
    s_alert_left = (uint16_t)UFO_SE_ALERT_PERIOD;
    ufo_se(UFO_SEV_ALERT_LOOP, UFO_SE_ENTER, *(const float *)(e + CE_ENEMY_POS_X),
           "一手 th12:0x44a9fe mov eax,0x36 + 0x44aa06（循环音；本卡用手动循环重放，见函数注）");
}

/* 每游玩帧跟随这几件（位置 / 环角 / 条宽）。调用点 = `on_tick_2` 末尾（每帧一次，与 `fill` 同一拍）。 */
static void ufo_gauge_follow(ufo_card_t *s)
{
    uint8_t *e;
    float x, y;
    int32_t permille;
    int i;

    if (!s) return;
    if (!s_ring_arc_id && !s_gauge_bar_id) return;     /* 都没起（UFO 不在）⇒ 无操作 */
    e = ufo_enemy_by_id(s->u.ufo_id);
    if (!e) {                                          /* 本体不在了（飞走 / 击破 / 被清）⇒ 收干净 */
        ufo_gauge_kill();
        return;
    }
    /* 句柄在别处被引擎销毁过 ⇒ 自愈：丢掉句柄重新起（与本体跟随 VM / `ufo_hud_ensure()` 同款口径）。
     * ★ 四只一起查：环那三条是**同一棵摊平的树**，缺一只就整组重起（避免"只剩描边没有弧"的中间态）。 */
    if (!s_ring_arc_id || !ce_anm_get_vm(s_ring_arc_id)
        || !s_ring_border_id[0] || !ce_anm_get_vm(s_ring_border_id[0])
        || !s_ring_border_id[1] || !ce_anm_get_vm(s_ring_border_id[1])
        || !s_gauge_bar_id || !ce_anm_get_vm(s_gauge_bar_id)) {
        ufo_gauge_spawn(s, e);
        return;                                        /* spawn 里已经摆好位置 */
    }

    /* 位置 = 敌机坐标（`CE_ENEMY_POS_X/Y` = `data.final_pos`，ECL 640 空间；世界层 VM 的实体坐标就是
     * 这个空间，1 单位 = 2 表面 px —— 与本体跟随 VM 同一套口径，不需要任何换算）。 */
    x = *(const float *)(e + CE_ENEMY_POS_X);
    y = *(const float *)(e + CE_ENEMY_POS_Y);

    /* ① 环（三条各自独立，见 `s_ring_border_id[]` 的长注）：贴着实体右侧（🟡 读法 B，见
     *    `UFO_GAUGE_RING_DX`）+ 填充弧的角度 = `fill × 2π`、base = π/2（6 点）。
     *    比例来源 = 我们自己的 `fill`（`ufo_core` 的 `fill_millionths`；一手门 = 每面 34/36/39/42/46/51/56）。
     *    浮点只在**头文件**的 `static inline ufo_fill_ratio()` 里出（x87 硬门）。
     *    ⚠ 三条必须**逐帧**摆：它们没有父子关系了，谁也不会替谁继承位置。 */
    for (i = 0; i < 2; ++i)
        ce_anm_set_pos(s_ring_border_id[i], x + UFO_GAUGE_RING_DX, y + UFO_GAUGE_RING_DY, 0.0f);
    ce_anm_set_pos(s_ring_arc_id, x + UFO_GAUGE_RING_DX, y + UFO_GAUGE_RING_DY, 0.0f);
    ce_anm_set_rotation(s_ring_arc_id, ufo_fill_ratio(&s->u) * 6.2831855f, 0.0f,
                        k_ufo_gauge_pi_over_2);
    /* ★ 2026-09-18 诊断（第一轮实跑：「本体的很多倍…飞碟快飞走的时候，我才猛然在画面的**最底下**
     *   看到了一个很大的圆弧」）—— 每 60 帧把**引擎眼里的那几台 VM**现场打一行。
     *   ⚠ **第二轮实跑已经定位**（用户：「环不是偏移，是贼 jb 大，它确实是跟着飞碟动」）⇒ 根因 =
     *   父 scale 乘爆子几何（见 `s_ring_border_id[]` 的长注），本条诊断**保留**：
     *   它现在用来核对「三条环 VM 各自独立、位置一致、scale 是脚本原值（6/59）」，仍是回归的哨兵。 */
    {
        void *rv = ce_anm_get_vm(s_ring_arc_id);
        if ((s->play_frame % 60u) == 0u) {
            const float *rpos = rv ? (const float *)((const uint8_t *)rv + CE_ANM_VM_POS) : 0;
            const float *rsc  = rv ? (const float *)((const uint8_t *)rv + CE_ANM_VM_SCALE) : 0;
            const float *rrot = rv ? (const float *)((const uint8_t *)rv + CE_ANM_VM_ROTATION) : 0;
            void *o1 = ce_anm_get_vm(s_ring_border_id[0]);
            void *o2 = ce_anm_get_vm(s_ring_border_id[1]);
            const float *p1 = o1 ? (const float *)((const uint8_t *)o1 + CE_ANM_VM_POS) : 0;
            const float *p2 = o2 ? (const float *)((const uint8_t *)o2 + CE_ANM_VM_POS) : 0;
            const float *s1 = o1 ? (const float *)((const uint8_t *)o1 + CE_ANM_VM_SCALE) : 0;
            const float *s2 = o2 ? (const float *)((const uint8_t *)o2 + CE_ANM_VM_SCALE) : 0;
            void *bv = s->body_anm_id ? ce_anm_get_vm(s->body_anm_id) : 0;
            const float *bpos = bv ? (const float *)((const uint8_t *)bv + CE_ANM_VM_POS) : 0;
            const float *bsc  = bv ? (const float *)((const uint8_t *)bv + CE_ANM_VM_SCALE) : 0;
            const float *abs  = (const float *)(e + CE_SPAWN_POS);
            ce_log("ufo/gauge: arc %u pos (%.1f, %.1f) scale (%.2f, %.2f) rot (%.3f, %.2f, %.2f) | "
                   "border %u/%u pos (%.1f, %.1f)/(%.1f, %.1f) scale (%.2f, %.2f)/(%.2f, %.2f) | "
                   "bar %u | body %u pos (%.1f, %.1f) scale (%.2f, %.2f) | enemy final (%.1f, %.1f) "
                   "abs (%.1f, %.1f) | frame %u",
                   s_ring_arc_id, rpos ? (double)rpos[0] : 0.0, rpos ? (double)rpos[1] : 0.0,
                   rsc ? (double)rsc[0] : 0.0, rsc ? (double)rsc[1] : 0.0,
                   rrot ? (double)rrot[0] : 0.0, rrot ? (double)rrot[1] : 0.0,
                   rrot ? (double)rrot[2] : 0.0,
                   s_ring_border_id[0], s_ring_border_id[1],
                   p1 ? (double)p1[0] : 0.0, p1 ? (double)p1[1] : 0.0,
                   p2 ? (double)p2[0] : 0.0, p2 ? (double)p2[1] : 0.0,
                   s1 ? (double)s1[0] : 0.0, s1 ? (double)s1[1] : 0.0,
                   s2 ? (double)s2[0] : 0.0, s2 ? (double)s2[1] : 0.0,
                   s_gauge_bar_id, s->body_anm_id,
                   bpos ? (double)bpos[0] : 0.0, bpos ? (double)bpos[1] : 0.0,
                   bsc ? (double)bsc[0] : 0.0, bsc ? (double)bsc[1] : 0.0,
                   (double)x, (double)y, (double)abs[0], (double)abs[1], s->play_frame);
        }
    }

    /* ② 条：**scale = HP 比例**（不是像素宽！），高 = 1（= 脚本自己的 `drawRect(32,4)`）。
     *    HP 当前值 = `zEnemyData.life.current` = `enemy+0x6220`（同址先例：`animal.c` 的
     *    `ANIMAL_ENEMY_LIFE_CUR` / `lolk.c` 的 `CE_ENEMY_LIFE_CUR`），上限 = 我们 spawn 时写进去的 life。
     *    颜色每帧写一次（脚本里的 `color(255,96,96)` 是我们照抄的 ⇒ 幂等，同时保证「引擎重建过 VM
     *    之后颜色不丢」）。
     *    ★★ 2026-09-18 修 bug（用户实跑 B3「血条显示贼长」）：原来写的是
     *      `ce_anm_set_scale(条, UFO_GAUGE_BAR_FULL_W * permille / 1000.0f, UFO_GAUGE_BAR_H)`
     *      —— 把脚本里**已经写死的** `drawRect(32.0f, 4.0f)` 尺寸（32 / 4）又当成 scale 乘了一次
     *      ⇒ 满血时 scale.x = 32 ⇒ 条宽 32×32 = 1024 世界单位（≈2048 表面 px，横穿整个游玩区）。
     *    一手判据（`th12:0x44a801`–`0x44a814`，见 `AUDIT.md` §BO-12）：引擎每帧**只写**
     *      `scale.x = life.current / life.maximum`（**比值 0..1**），`scale.y` 一个字节都不写
     *      ⇒ 高度保持脚本默认的 1（= 4 单位）。同仓对照：HUD 那几条走脚本 104（`drawRect(1,1)`）
     *      时 scale 才是像素尺寸 —— **两个脚本的单位不是一回事**，这正是当初抄错的那一步。
     *    ⚠ 投影（子 VM 232）**只跟位置、不跟 scale** —— 照抄原生：引擎只缩放条那一只 VM
     *      （`th12:0x44a801` 写的就是 `zUfo+0x44` 那一只），影子恒长 32。别顺手也去 set_scale 它。 */
    /* ★ P4：分母 = `u->hp_max`（= 当初真写进 spawn 的那个数），**不在显示侧另算一次**。 */
    permille = ufo_hp_permille(*(const volatile int32_t *)(e + UFO_ENEMY_LIFE_CUR),
                               s->u.hp_max);
    ce_anm_set_color(s_gauge_bar_id, UFO_GAUGE_BAR_COLOR);
    ce_anm_set_pos(s_gauge_bar_id, x, y + UFO_GAUGE_BAR_DY, 0.0f);
    ce_anm_set_scale(s_gauge_bar_id, (float)permille / 1000.0f, 1.0f);
}

/* 复刻引擎清尾的 **freelist LRU 摘挂**（`0x4465b4`–`0x4465d2`，指令原文见文件头 ④）。
 * 逐指令对照（ecx = 头、eax = 头的 next、edi = 本槽）：
 *     ecx = *(item+0)                      ; 池的 list head（`zItemManager` 的 normal_items_freelist_head）
 *     eax = *(head+4)                      ; ★ 头的 next，**不是** `*(item+4)`
 *     if (eax) { *(item+4) = eax; *(eax+8) = item; }
 *     if (*(head+0xc)) *(head+0xc) = item;
 *     *(head+4) = item;  *(item+8) = head;
 * ⚠⚠ 实装时**踩过一次**：把 `eax` 误读成 `*(item+4)`（自赋值、无操作）⇒ 对「还在空闲表上」的槽
 *   （= 我们吸掉的那些）**完全没做前插**，旧头与新头之间的结点从正向链上掉出去 = **链表损坏**。
 *   判据就在这里：`0x4465b6 8b 41 04 mov eax,[ecx+0x4]` —— 基址是 **ecx（头）**，不是 edi（本槽）。
 * ⚠ 两个 NULL 守卫是**防御**，不是照抄（引擎那两条无守卫）：`head == NULL` 时引擎自己会崩，
 *   我们直接返回 = 「只可能少崩」；`head->next == NULL`（头是唯一的哨兵）时跳过前插那一对写字。
 * ⚠ 这是**直写引擎链表**的新用法、仓里无先例 ⇒ 已列进 impl.md 的未实跑清单。 */
static void ufo_item_relink(uint8_t *it)
{
    uint8_t *head = *(uint8_t **)it;
    uint8_t *next;
    if (!head) return;
    next = *(uint8_t **)(head + 4);          /* ★ 头的 next（不是 *(it+4)）*/
    if (next) {
        *(uint8_t **)(it + 4) = next;
        *(uint8_t **)(next + 8) = it;
    }
    if (*(uint8_t **)(head + 0xc)) *(uint8_t **)(head + 0xc) = it;
    *(uint8_t **)(head + 4) = it;
    *(uint8_t **)(it + 8) = head;
}

/* ★ 前置声明：定义在下面（自建掉落物实体 = 压缩包 / 整颗 Bomb，见那一段的长注）*/
static void ufo_drop_spawn_bomb(ufo_card_t *s, float x, float y);
static void ufo_drop_kill_all(void);

/* ================================ §3.4 伪磁场：一颗道具 ================================
 * 返回 1 = 本帧真的动了它（吸掉或拉了一步），0 = 没碰（栅栏 / 早停）。 */
/* ⚠ 不再收自机坐标：P2 #4 把「离自机 30 px」那条栅栏撤了 ⇒ 这一路不需要它（理由见调用点注）。 */
/* ★★ 2026-09-23（owner 实跑第 7 条）：「**点收满了但是没有进入收满状态，没掉 B 环没变成炫彩**」
 *   —— 真因：吸满那一段（吐奖励 + 环闪烁 + 音效）原来只挂在**道具吸收**那一支里
 *   （`ufo_try_item`），而**信仰点吸收**那一支（`ufo_absorb_faith_tick`）调 `ufo_absorb_item()`
 *   时**丢掉了返回值** ⇒ 若最后一口是被吸进来的信仰点喂满的，就只涨 fill、**不开奖也不闪**
 *   ⇒ 实跑正是「fill 1.000 但没掉 B、没变炫彩」（日志：那只 UFO 拉了 44 颗大信仰点）。
 *   ⇒ 把这一段抽成公共函数，两条吸收路都调。 */
static void ufo_fill_complete(ufo_card_t *s, float ux, float uy)
{
    int32_t reward = ufo_spit_reward(s->u.color);
    int32_t tok_state = ufo_spit_token_state(s->u.color, s->grp.last_slot2);
    ++s->bursts;
    /* ★★ 一手「满时的闪烁」（`0x44ad86` 音效 0x30 → `0x44ad8b`/`0x44ad94`
     *   `anm_interrupt_by_id(vm3c, 2)`）：**先发闪烁中断、同一帧再吐奖励**
     *   （`0x44ad94` 早于 `0x44adcd`/`0x44adf8`，与用户口述的「先掉碎片再闪烁」在一手里是同一帧）。
     *   原生那一下是**递归子树**（`0x461970`）⇒ 204/205/206 三条全部进 `interruptLabel(2)`
     *   = 每 2 帧换色循环（黄→红→绿→青）⇒ 我们把三条**各自**发一次（树已摊平，见
     *   `s_ring_border_id[]` 的长注）。 */
    ufo_gauge_flash();
    ufo_se(UFO_SEV_FULL, UFO_SE_FULL, ux,
               "一手 th12:0x44ad7e mov eax,0x30 + 0x44ad86 call 0x453e20（吸满吐奖励）");
    if (reward == UFO_REWARD_BOMB_FRAG) {
        /* ★★ 用户裁决 **P6**（2026-09-18）：「**整颗 Bomb 确实是得是整颗**，有没有可能额外建一个
         *   掉落 Bomb 整卡的 VM（会小题大做吗？）」⇒ 答复：不小题大做 —— 用本卡自己的
         *   「自建掉落物实体」（一手道具 7 的外观 + 普通道具运动），**不再映射到 TH18 type 6**。 */
        ufo_drop_spawn_bomb(s, ux, uy);
        ++s->rewards_spit;
    } else if (reward) {
        float rp[3];
        rp[0] = ux; rp[1] = uy; rp[2] = 0.0f;
        /* 一手参数：角度 −π/2、速度 2.2（`th12:0x44aca0` 的奖励 / `0x446f40` 的 main-drop 同参）*/
        if (ce_spawn_item((uint32_t)reward, rp, -1.5707964f, 2.2f)) ++s->rewards_spit;
        else ce_log("ufo: reward spit type %d failed (item pool full?)", reward);
    }
    if (tok_state != UFO_TOK_DROP_NONE) {
        /* ★ 彩虹 UFO 吸满 ⇒ 掉一枚**令牌**（一手 `0x44ae03`；TH18 没有令牌道具类型 ⇒ 掉令牌）。
         * 位置 = 本体位置（一手：`[esi+0x34]+0x1074` = 敌机 pos）。
         * ⚠ 换池后「掉令牌」= 池里多一只 VM（不再建敌人）⇒ 仍是**非死亡栈**（`on_item_frame`）。 */
        if (ufo_tok_spawn_ex(s, ux, uy, ufo_token_display_color(UFO_COLOR_RED, 1, tok_state),
                             tok_state, 1))
            ce_log("ufo: rainbow UFO filled up - spat one RAINBOW TOKEN (rb state %d, "
                   "one-hand th12:0x44ae03 by COLLECTED_UFOS[2])", (int)tok_state);
    }
    if (!reward && tok_state == UFO_TOK_DROP_NONE)
        ce_log("ufo: fill full (color %d) - nothing spat (blue has no case in TH12's switch)",
               (int)s->u.color);
}

static int ufo_try_item(ufo_card_t *s, uint8_t *it, float ux, float uy)
{
    float *ipos = (float *)(it + CE_ITEM_POS);
    float *ivel = (float *)(it + UFO_ITEM_VEL);
    float nx, ny, dx, dy;
    float speed;

    /* 捕获：到 UFO 中心 18 px 内（一手 `th12:0x426879`）⇒ 置空槽 + 删 anm VM + 计数。 */
    dx = ipos[0] - ux;
    dy = ipos[1] - uy;
    if (ufo_within_capture(dx, dy)) {
        /* 清尾照 `0x446595`/`0x44659f`/`0x4465aa`/`0x4465b4`–`0x4465d2`（见文件头 ④）：
         *   state = 0 → 删 anm VM → anm id 写 0 → **freelist LRU 摘挂**。 */
        uint32_t anm = *(uint32_t *)(it + UFO_ITEM_ANM_ID);
        *(int32_t *)(it + CE_ITEM_STATE) = 0;
        if (anm) ce_anm_delete(anm);
        *(uint32_t *)(it + UFO_ITEM_ANM_ID) = 0;
        ufo_item_relink(it);
        ufo_pull_forget(it);                           /* ★ 槽要回收了 ⇒ 丢掉它的累计拉拽速度 */
        /* vel 也清掉（同一个槽下帧可能被复用；残留 vel 会立刻把新道具甩走）*/
        ivel[0] = 0.0f; ivel[1] = 0.0f; ivel[2] = 0.0f;
        ++s->items_pulled;
        if (ufo_absorb_item(&s->u, *(uint32_t *)(it + CE_ITEM_TYPE), UFO_PLACEHOLDER_STAGE,
                            UFO_FILL_SCALE_PERMILLE)) {
            /* 吸满：按色吐**一件**（一手 `0x44ad99` 的 switch）——
             *   红 → 道具 4（残机碎片）；绿 → 道具 7（整颗 Bomb，我们映射到 TH18 type 6）；
             *   蓝 → switch 无 case ⇒ 什么都不吐；彩虹 → **一枚令牌**（`0x44ae03`，按 `COLLECTED_UFOS[2]`）。
             * ⚠ 现在这条**每次吸满只会走一次**（`ufo_absorb_item` 已按一手订正为「满着不再开奖」）*/
            ufo_fill_complete(s, ux, uy);
        } else {
            ufo_se(UFO_SEV_ABSORB, UFO_SE_ABSORB, ipos[0],
                         "一手 th12:0x426970 mov eax,0x26 + 0x426982 call 0x453e20（吞下一颗道具）");
        }
        return 1;
    }

    /* 拉拽：先算目标点。栅栏①②（屏内 / 离自机 > 30 px）是**针对目标点**的 ——
     * 一手依据见文件头 ②③：目标点出屏 ⇒ 引擎当帧删；离自机 30 px 内 ⇒ 引擎当帧收掉。
     * ⚠ 这里宁可「本帧不拉」（记账 `skipped_guard`）也不把道具钉在屏边：钉住反而会被删。 */
    /* ★★ 2026-09-18 修（用户实跑「吸收速度极慢 / 道具定住了一样」）：这里用的是**累计速度**
     *   （一手 `item+0x9bc += …`），不是 `ufo_drag_speed()` 那个"每帧位移"公式 —— 后者只是
     *   一手公式里的**单帧增量项**，当成位移就慢了几十倍。*/
    speed = ufo_pull_speed(it, *(uint32_t *)(it + CE_ITEM_TYPE), s->u.frames);
    nx = ipos[0];
    ny = ipos[1];
    ufo_drag_step(&nx, &ny, ux, uy, speed);
    /* ★★ 2026-09-21（P2 #4「吸收速度似乎还是慢了」）：这里原来还 AND 了一条
     *   `ufo_target_clear_of_player(nx, ny, px, py)`（"下一步别踏进自机 30 px 收点圈"）——
     *   **一手里没有这条栅栏**：TH12 的 state 7 每帧走完位移就落回共同尾巴 `th12:0x4260af`，
     *   而那条尾巴正是**自机收点矩形**的判定（`0x426100`–`0x426164`）⇒ 一手允许自机半途把
     *   被吸的道具收走。我们照抄出来的后果是：只要下一步会踏进自机 30 px 圈，就**整帧不动**；
     *   下一帧速度更大、目标点还在圈里 ⇒ **永久卡在圈边上**（自机不挪窝它就不动）。
     *   ⇒ 现在只判「目标点在屏内」（`ufo_step_allowed` 只剩这一条）。 */
    if (!ufo_step_allowed(nx, ny)) {
        ++s->u.skipped_guard;
        return 0;
    }
    /* 一手时序（`PLAN` §2.4）：本帧直写 pos 之后，引擎的 state-1 物理还会叠一点
     *（`0x445c25`/`0x445c35`/`0x445c45`）⇒ **写 pos 的同时必须清 vel**，否则残余 vel 被 body 用掉、
     * 道具被甩回原处。清干净之后每帧只剩 vy 的 +0.03 重力（文件头 ①）。 */
    ivel[0] = UFO_ITEM_VEL_DAMP;
    ivel[1] = UFO_ITEM_VEL_DAMP;
    ivel[2] = UFO_ITEM_VEL_DAMP;
    ipos[0] = nx;
    ipos[1] = ny;
    return 1;
}

/* 扫池（成本口径见文件尾 §性能）。 */
/* ★ 前置声明：定义在下面（自建实体 = 110/111 的信仰点，见那一段的长注）*/
static void ufo_absorb_faith_tick(ufo_card_t *s, float ux, float uy);

static void ufo_absorb_tick(ufo_card_t *s)
{
    uint8_t *mgr = CE_ITEM_MANAGER();
    uint8_t *em = CE_ENEMY_MGR();
    uint8_t *e;
    float ux, uy;
    unsigned i;

    if (!mgr || !em) return;
    e = ufo_enemy_by_id(s->u.ufo_id);
    if (!e) return;                                    /* 本体没了（飞走 / 被清）⇒ 交给 finish 分支 */
    ux = *(float *)(e + CE_ENEMY_POS_X);
    uy = *(float *)(e + CE_ENEMY_POS_Y);
    /* ★ 2026-09-21（P2 #4）：这里原来还取一次自机坐标（`ufo_player_xy`）给「离自机 30 px」
     *   那条栅栏用；栅栏撤掉之后拉拽只朝 **UFO** 走 ⇒ 自机坐标在这一路上不再需要。 */

    for (i = 0; i < CE_ITEM_SLOTS; ++i) {
        uint8_t *it = mgr + CE_ITEM_POOL_OFF + (size_t)i * CE_ITEM_STRIDE;
        int32_t st = *(int32_t *)(it + CE_ITEM_STATE);
        if (st == 0) continue;                         /* 早停：空槽（最常见）*/
        /* 早停：先读 type（一次访存），不是 1/2 立刻走 —— 满配时全场大部分槽都在这条上出局 */
        if (ufo_absorb_type_index(*(uint32_t *)(it + CE_ITEM_TYPE)) < 0) continue;
        /* ▼ 站点自检门 ③（2026-09-18，**fail-safe**）：吸收只对「引擎文档域里的道具状态」成立
         *   （一手：engine.h 的 CE_ITEM_STATE 段 —— 0 落地/1 吸引/2 归位/3 真空/4 被吸 …）。
         *   状态不在域里 ⇒ 别赌，**跳过吸收**（= 该道具继续走原版路径）+ 记一行。*/
        {
            const int32_t istate = *(const int32_t *)(it + CE_ITEM_STATE);
            if (istate < 0 || istate > 6) {
                ce_assume_fail("ufo absorb: 道具 state 不在文档域（跳过本次吸收）");
                continue;
            }
        }
        if (!ufo_absorbable(*(uint32_t *)(it + CE_ITEM_TYPE), st, s->u.ufo_id, 1)) continue;
        ufo_try_item(s, it, ux, uy);
    }
    /* ★★ 自建实体（110/111 的信仰点）走同一拍：**先扫道具池、再扫声明式列表** ——
     *   两边互不影响（信仰点不在道具池里），顺序只影响日志里"谁先"的观感。 */
    ufo_absorb_faith_tick(s, ux, uy);
}

/* ==================== ★★ 吸收**自建实体**：110/111 的（大/中）信仰点（用户 2026-09-18）====================
 * 用户口径（`NEXT.md` §1x.9）：「UFO 得能吸收：大中信仰点 … **进行计数**」+「**按 +1 实现**」。
 * 一手背景：TH12 的 UFO 吸的是**道具**（type 1/2），而本 mod 的信仰点是**自建 VM 池**
 *   （`faith.h`：TH18 没有信仰点这个 item type）⇒ 一手没有对位物，这是**跨卡耦合**，
 *   形状按 `NEXT.md` §1x.7 定的「声明式」：生产者（faith）只声明，消费者（本卡）只读。
 *
 * 计数口径（一条条写清，免得下一个人猜）：
 *   · **+1 / 颗**（用户 2026-09-18：「+1 不就是 *2 吗？不过按照 +1 实现」）—— 与道具逐值同规则；
 *   · **通道 = P 通道（cntA）**：中信仰点是满 P 转换来的 ⇒ 等价 P 点（用户原文）；
 *     大信仰点按 §1x.7 的等价表 = 「红 UFO 等价于 P 点」⇒ 同通道；
 *     ⚠ 彩虹 UFO（color == −1）时一手会把两条通道**互换**（`th12:0x44accb`）⇒ 这里照样走
 *     `ufo_absorb_item()` 的统一入口，互换逻辑只有一处。
 *   · **fill（吸收进度）也跟着涨**（与道具同：一手 `0x44acf5` 那支就是"只加 fill 不计数的 type"）——
 *     ❓ 自定：用户只说"计数"，但「吸一颗相当于吸一颗道具」是最小惊奇的读法；若不想要，删
 *     `ufo_absorb_item()` 那一行的调用即可（保留 `faith_pulled_*` 的记账）。
 *   · **拉拽目标是 UFO 本体**（`ux/uy`），不是自机 ⇒ 本函数**不收自机坐标**
 *     （2026-09-18：原来多两个没用到的 `px/py` 形参，编译器 `-Wunused-parameter` 会叫，删掉）；
 *   · **被吸走的点不再属于 110/111**：不 +Δ、不进 114 的批、不加连击（`faith_absorb_take()` 只删实体）
 *     —— 它们的价值记在 `faith_pulled_*` 里，**击破时由「压缩包」带回**（收集时调 `faith_spawn()`）。 */
static void ufo_absorb_faith_tick(ufo_card_t *s, float ux, float uy)
{
    faith_absorb_t list[FAITH_BIG_POOL];
    int n, i;

    n = faith_absorb_list(list, (int)FAITH_BIG_POOL);
    for (i = 0; i < n; ++i) {
        faith_absorb_t e = list[i];
        float dx = e.x - ux, dy = e.y - uy;
        float nx, ny, speed;

        /* ★★ 2026-09-24（owner 追认）：「**没有出生无敌是我们要复现的效果**（我们后建的点是我们自己搞的、
         *   无所谓，主要是对于**原生点**）」⇒ 飞碟这一路**不看** `in_toss`、也不给任何出生豁免：
         *   一手里道具的钩子门只要求 `state ∈ {1,2}`（`0x4271dc`），刚出生的道具就是 state 1 ⇒ **当场可吸** ✓。
         *   （`faith_absorb_t` 里的 `age/state/in_toss` 三个字段保留：生产者照旧声明，
         *     消费者**当前不用** —— 将来若要给自建点加豁免，判据已经在那儿了。）*/
        if (ufo_within_capture(dx, dy)) {              /* 18 px 内 ⇒ 吸掉（与道具同一判据）*/
            if (!faith_absorb_take(&e)) continue;      /* 这一颗已经被别人收走 / 匹配不上 */
            if (e.tier == FAITH_TIER_BIG) { ++s->faith_pulled_big; s->faith_pulled_big_sum += e.value; }
            else { ++s->faith_pulled_mid; s->faith_pulled_mid_sum += e.value; }
            s->faith_pulled_mask |= e.slot_mask;
            /* ★ 计数 +1（颜色通道由 `ufo_absorb_item` 一处决定：彩虹时互换）*/
            /* ★★ 2026-09-23（owner #7）：**必须接住返回值** —— 原来 `(void)` 丢掉它 ⇒
             *   被信仰点喂满的那一次不开奖（没 B、没闪烁）。见 `ufo_fill_complete()` 的注。*/
            if (ufo_absorb_item(&s->u, UFO_ITEM_TYPE_P, UFO_PLACEHOLDER_STAGE, UFO_FILL_SCALE_PERMILLE))
                ufo_fill_complete(s, ux, uy);
            ufo_se(UFO_SEV_ABSORB, UFO_SE_ABSORB, e.x,
                       "一手 th12:0x426970（同上；这一声是 110/111 的信仰点被吞）");
            continue;
        }
        /* 拉拽：与道具同一条步长公式（一手 `th12:0x426879`），走 `ufo_drag_step` 的**比例步进**
         * （不过冲、不引 sqrt）。信仰点是我们自己的实体 ⇒ 没有「出屏即删 / 离自机 30px 即收」
         * 那两条栅栏（那是一手对**道具**的规则；这里的点是 110/111 的池，由它们自己管生命周期）。
         * ★★ 2026-09-18：速度用**累计口径**（`ufo_drag_speed_accum`）—— 与道具那条同一个一手公式
         *   （原来误用单帧增量 ⇒ 慢了几十倍、点看着"定住"）。*/
        speed = ufo_drag_speed_accum(s->u.frames);
        nx = e.x; ny = e.y;
        ufo_drag_step(&nx, &ny, ux, uy, speed);
        /* ★★ 2026-09-24（owner：「我看到的只是顿住一下，**实测是顿住好长时间**」）——
         *   这里原来调 `faith_absorb_move()`（**只写坐标、不碰速度**）⇒ 点自己的上抛/下落每帧还在叠，
         *   与飞碟那一步互相抵消 ⇒ 点**悬在半空** = owner 看到的"顿住好长时间"。
         *   一手：TH12 的道具上钩那一刻（state 7）**把速度向量与吸引距离一起清零** ⇒ 之后位移只由
         *   飞碟那一步决定 ⇒ 换成 `faith_absorb_hold()`（写坐标 + 清 vx/vy + 飞散段清 lr）。*/
        if (nx != e.x || ny != e.y) faith_absorb_hold(&e, nx, ny);   /* 写回 + 按住它自己的运动*/
    }
}

/* ================================ §3.4 令牌 → 槽（吃 / 记）=============================== */
/* 令牌被自机「吃到」：spec §3.2 ⏳ ①「令牌自己就是实体（接触即入槽）还是莉莉白式携带者」**未拍板**
 * ⇒ 本实装取前者（接触即入槽：占位期实体不飘不爆，接触是唯一可靠的交互）。
 * 一手没有「玩家接触敌人的通用事件」（`EVENTS-CONTRACT.md` 的事件表里没有），所以这里用距离。
 * ★★ 2026-09-18 用户裁决 **P5**：「**令牌收取按照本作收取走**」⇒ 半径**读自机自己的字段**
 *   `player+0x4798c`（`CE_PLAYER_ITEM_COLLECT_R`）—— 那就是 **TH18 引擎自己的道具收集闸**
 *   （`th18:0x44624a movss xmm0,[esi+0x4798c]` → `mulss xmm0,xmm0` → `comiss` = `dist² < r²`，
 *   原生默认 30.0f）。与 110 的 `faith.c:40` 同一口径：**别自己发明半径，读引擎的**。
 * ⚠ 原来的 20 px 圆是 ❓ 自定占位（`AUDIT.md` §BO-11 的 P5），本轮**取消**。
 * ⚠ 半径来自玩家对象的字段 ⇒ 引擎在别处改它（例如某些卡改收集范围）我们**自动跟随**，这正是裁决要的。 */
static float ufo_tok_pick_radius(void)
{
    const uint8_t *p = (const uint8_t *)CE_PLAYER();
    float r;
    if (!p) return 0.0f;                               /* 菜单 / 无自机 ⇒ 不收（而不是用一个假半径）*/
    r = *(const volatile float *)(p + CE_PLAYER_ITEM_COLLECT_R);
    if (!(r > 0.0f)) return 0.0f;                      /* 引擎还没初始化 / 被写成 0 ⇒ 不收 */
    return r;
}

static int ufo_tok_pick(float tx, float ty, float px, float py, float radius)
{
    float dx = tx - px, dy = ty - py;
    if (!(radius > 0.0f)) return 0;
    return dx * dx + dy * dy <= radius * radius;       /* 与引擎同一条判据（严格 < 与 <= 在实体判定上等价）*/
}

/* 令牌落槽的结果码 = 纯逻辑层那三个（**转发**，不另立一套值；决策只有一处）。 */
#define UFO_PUT_NONE  UFO_GROUP_PUT_NONE
#define UFO_PUT_HELD  UFO_GROUP_PUT_HELD
#define UFO_PUT_READY UFO_GROUP_PUT_READY

/* 槽里有没有一个「已经成组、等着被兑现」的组合。 */
static int ufo_present_group_ready(const ufo_card_t *s)
{
    return ufo_group_has_match(&s->grp);
}

/* 把一个令牌颜色落进槽。**决策全在 `ufo_group_put`（纯逻辑、主机单测覆盖）**，
 * 这里只做接线：音效 + 统计 + 把「两同一异丢掉的最旧令牌」记进账。
 * （「槽满且不成组 ⇒ 丢最旧 + 丢完重判」的完整规则写在 `ufo_core.h` 的 `ufo_group_put` 上。）*/
static int ufo_token_to_slot(ufo_card_t *s, int32_t color, float x)
{
    int32_t dropped = 0;
    int put;

    if (!ufo_color_valid(color)) return UFO_PUT_NONE;
    /* ⚠ 槽里**已经**有一个干等着的组（上一帧被「场上已有 UFO」挡下、或者 UFO 刚走而没人叫召唤）：
     * 这一颗令牌**不消费**它（`ufo_group_put` 会原样报 READY）⇒ 不播音、不计数，纯粹当「再叫一次」。
     * 音效 / `taken_tokens` 只记**真的进了槽**的那些（否则每 5 秒白响一声、统计也虚高）。*/
    if (ufo_present_group_ready(s)) return UFO_PUT_READY;
    ++s->taken_tokens;
    /* 声像 = **令牌自己的 x**（一手 `th12:0x426d86 fld [edi+0x96c]` 就是这个实参）。
     * （旧版写死 0 = 屏心，只是"取 0 更稳"的保守解；P1 #th12-3 顺手改回一手口径。）*/
    ufo_se(UFO_SEV_TOKEN_TAKE, UFO_SE_TOKEN, x,
                   "一手 th12:0x426dab mov eax,0x26 + 0x426dbd call 0x453e20（令牌入槽共同尾）");
    put = ufo_group_put(&s->grp, color, &dropped);
    if (dropped) {
        /* ★ 用户 2026-09-12 裁决 ②：这就是一手 KEEP2（`cnt1c = 4`，`th12:0x422e80`）——
         * 「两同一异 ⇒ 丢最旧、保留最近两个」。`dropped` 现在是**个数**（0/1）。 */
        s->dropped_tokens += (uint32_t)dropped;
        ce_log("ufo: three in the slots but not a group (two same + one different) - dropped %d oldest "
               "token(s), keeping the newest two (one-hand KEEP2, th12:0x422e80 cnt1c=4; user ruling 2)",
               (int)dropped);
    }
    return put;
}

/* ★ `ufo_pending_put/_take`（死亡栈上的「令牌颜色」登记环）**已随旧载体一起删除** ——
 *   理由见文件头那段「令牌池的四条已删除的旧状态」：那套环只服务「令牌被子弹打掉也算收」。
 *   令牌的来源（小怪击破 ⇒ 登记落点 ⇒ 帧末建 VM）走的是 `pend_drop` / `pend_drop_x/y`，与它无关。 */

/* 读走并撤掉一只令牌（**先取值再清条目** —— 顺序反了就把颜色读成 NONE）。
 * ★ 换池后这里**不再有死亡栈调用点**（令牌不产生死亡事件）⇒ 「死亡栈上删 VM」那条 🟡 未验证的
 *   组合整个消失了。现在只在两处调：接触收取（`ufo_tokens_tick`）与出界/TTL 退休。
 * ★★ 2026-09-18（新增 2）：返回的是**显示色**（`ufo_tok_display_color`），不是 `t->color` ——
 *   一手把变色令牌按「当时显示的那一色」入账（`th12:0x426c3f/0x426c4d/0x426c5e` 的 case 13/14/15
 *   分别记红/蓝/绿）。⇒ 入槽的唯一的取值口就是这里。 */
static int32_t ufo_tok_take_and_kill(int slot)
{
    ufo_tok_t *t = ufo_tok_at(slot);
    int32_t color;
    if (!t) return UFO_COLOR_NONE;
    color = ufo_tok_display_color(t);
    ufo_tok_kill(slot);
    return color;
}

/* 召唤（**唯一入口** —— 只有这一处调 `ce_spawn_enemy` 建本体）。
 * ⚠ 两个调用点都必须是**非死亡栈**：`on_tick_2`（帧末）与 `ufo_tokens_tick`（`on_item_frame` 内）。
 *   `on_enemy_death` 里**只登记**（`pending_token`），由 `on_tick_2` 兑现 —— 见 `DEATH` §2.4。 */
static void ufo_try_summon(ufo_card_t *s)
{
    int32_t made, hp, spawn_life;
    uint8_t *body;

    if (!ufo_present_group_ready(s)) return;
    if (!ufo_can_spawn(&s->u)) {
        /* 场上已经有一个 UFO（单例，一手管理器 = 一个 `zUfo`）。组**留在槽里**，
         * 等它没了之后下一次入槽会用 `UFO_PUT_READY` 把它兑现出来。 */
        return;
    }
    /* ★★ P1（一手）：**取走"成组"这个结果，但不槽** —— 一手在 UFO 结束时才清
     *   （`0x44a326`-`0x44a344`），召唤只置自己的闩。槽留着 ⇒ 「UFO 在场期间槽是满的」
     *   （HUD 三格亮着、收不进新令牌）＝ 用户 2026-09-18 裁决 P1 要的那条可见行为。 */
    made = ufo_group_take_ready(&s->grp);
    if (made == UFO_COLOR_NONE) return;
    /* ★★ P4（用户 2026-09-18「HP 一手确实是按火力」）+ P2 #6（2026-09-21「感觉飞碟太脆了」）：
     *   `hp` = **一手真血量**（表 `{1600..2500}` × 火力档，原值不缩放）；
     *   `spawn_life` = 写进 `spawn+0x14` 的**钳制值**（≤999，避开位 30 黑名单闸）；
     *   敌机建出来后立刻 `ufo_body_apply_real_hp()` 把真血量写回三格 life。 */
    hp = ufo_hp_for_power(CE_CURRENT_POWER(), CE_POWER_PER_LEVEL());
    spawn_life = ufo_clamp_spawn_life(hp);
    body = ufo_body_spawn(made, spawn_life);
    if (!body) {
        /* 建不出来：组已经消费掉了（不还回去 —— 还回去会与「单例」打架），记账 + 让节流重试 */
        ++s->failed_summons;
        ce_log("ufo: summon FAILED (color %d, art idx %d) - ce_spawn_enemy(\"%s\") returned nothing; "
               "that ECL subroutine is a placeholder and does not exist in default.ecl yet "
               "(spec §6 step 3)", (int)made, ufo_color_art_index(made), ufo_ecl_body_name(made));
        return;
    }
    /* ⚠ 先自己读 `enemy_id` 再登记：万一 `ufo_spawn` 拒了（id == 0），刚建出来的那个敌人
     * **没有任何人记着它** ⇒ 永久孤儿（`ufo_withdraw_all` 只收 `ufo_present` 那个）。
     * 所以那一步用置「本帧删」位收掉它（**不析构**：UAF，见文件头的位 25 说明）。 */
    /* ★ P4 / P2 #6：`u->hp_max` 记的是**真血量**（一手原值，血条/HUD 的分母读它），
     *   而写进 `spawn+0x14` 的是它的钳制版 `spawn_life` —— 两者不再是同一个数。 */
    if (!ufo_spawn(&s->u, *(uint32_t *)(body + CE_ENEMY_ID), made, hp)) {
        uint32_t orphan = *(uint32_t *)(body + CE_ENEMY_ID);
        /* ⚠ **无条件**置位（不能写 `if (orphan)`）：`ufo_spawn` 被拒的两条路里，
         * `enemy_id == 0` 恰恰就是最需要收掉的那个 —— 带守卫的话正好把它漏掉（`body` 已保证非 NULL）。*/
        *(uint32_t *)(body + CE_ENEMY_FLAGS) |= UFO_FLAGS_DELETE_NOW;
        ++s->failed_summons;
        ce_log("ufo: summoned body (color %d) but ufo_spawn refused it (enemy_id %u) - that enemy is "
               "being removed this frame (orphan guard)", (int)made, orphan);
        return;
    }
    ++s->ufos_spawned;
    s->logged_lifetime = 0;                            /* 新一个 UFO：存活上限日志重新允许报一次 */
    /* ★★ P2 #6：**回写真血量**（> 999 也合法 —— 位 30 只在 spawn 那一刻按 `spawn+0x14` 置）。
     *   放在 `ufo_spawn()` 成功之后：所有失败 / 孤儿路径都在它上面返回 ⇒ 只对**真活的**那只写。 */
    ufo_body_apply_real_hp(body, hp);
    /* ★ 画面：起本体的**跟随 VM**（`ability.anm` 164..167；`made` 是合成色，金落蓝那一格）。
     * ⚠ 放在 `ufo_spawn()` **成功之后**：所有失败 / 孤儿路径都在它上面返回 ⇒
     *   绝不会出现「没有本体却有 VM」的幽灵。位置在 `ufo_body_vm_spawn` 里当场摆好。 */
    ufo_body_vm_spawn(s, made, body);
    /* ★★ 实体上的两件 UI（用户点名）：吸收环 + 血量条。与本体 VM **同一拍**起（位置在 spawn 里当场
     * 摆好），生命周期也绑在一起 —— 收尾走 `ufo_withdraw_all()` → `ufo_gauge_kill()`。 */
    ufo_gauge_spawn(s, body);
    /* ★★ 2026-09-23（owner 实跑第 8 条：「**UFO 召唤的时候目前看不见任何特效**」的真因）：
     *   召唤特效（紫环 318 + 六边形 321）原来**只在 `ufo_body_vm_follow()` 的"重建本体 VM"那一支**里起
     *   —— 而本体 VM 永远先由**本函数**（`ufo_body_vm_spawn`）建好 ⇒ follow 那一支**一次都进不去**
     *   ⇒ 特效从来没被创建过（日志铁证：整个会话 0 行 `ufo: summon purple ring up`）。
     *   ⇒ 改在**这里**（召唤成功那一刻）起，与本体 VM 同一拍。`ufo_summon_fx_try` 自带两道幂等门。
     *   ★ 2026-09-23 owner 第 ③ 条：起的东西从「紫环 + 六边形」换成**一手的大横幅**（脚本 324），
     *     理由与一手出处见 `ufo_summon_fx_try` 上面那一整段。 */
    ufo_summon_fx_try(s, body);
    /* ★ 日志带 `art idx`：用户裁决 ① 说「金 = 蓝的调色版」，这一格就是证据 ——
     *   金（4）打出来必须是 **1**（= 蓝那一格，`ufo_color_art_index`），实跑一眼可核。 */
    /* ★ P4/P2 #6 对账：把「火力 → 档位 → 一手 HP → 写进 spawn 的钳制值」四个数一次打全
     *   —— 实跑里核「HP 确实按火力 + 真血量 > 999」只用看这一行（一手表 {1600..2500}）。 */
    ufo_se(UFO_SEV_SPAWN, UFO_SE_SPAWN, 0.0f,
                       "一手 th12:0x44a9d9 mov edx,0x34 + 0x44a9ef call 0x453d90（居中）");                 /* 一手：`Ufo::spawn` 的出场音（se_lazer02，居中）*/
    /* ★★ 2026-09-23（owner 实跑第 8 条）：**召唤音 = `se_ufo.wav`**（新扩表槽 `CE_VOICE_UFO`）。
     *   一手的第二声（`0x36` = `se_ufoalert`，在飞碟自己的 x、且是**循环音**）不再在这里放一次 ——
     *   它改由 `ufo_alert_loop_tick()` 在 UFO 存活期间**持续**播（owner 第 11 条口径）。 */
    ufo_se(UFO_SEV_SPAWN, UFO_SE_UFO, *(const float *)(body + CE_ENEMY_POS_X),
                       "★ owner 2026-09-23 指定 se_ufo.wav（TH12 表第 0x33 行；一手那一枚不在 Ufo::spawn —— "
                       "见宏定义处的说明）");
    ce_log("ufo: summoned UFO (color %d, art idx %d%s) enemy_id %u at frame %u - REAL HP %d "
           "(one-hand tier %d = %d, applied to enemy+0x6220/24/28 after spawn; spawn life was %d "
           "to stay under the >=1000 flag-30 gate; power %d, per level %d)", (int)made,
           ufo_color_art_index(made), ufo_color_is_gold(made) ? " = gold, uses the BLUE art set" : "",
           s->u.ufo_id, s->play_frame, (int)hp, (int)ufo_hp_tier(CE_CURRENT_POWER(), CE_POWER_PER_LEVEL()),
           (int)ufo_hp_onehand_for_tier(ufo_hp_tier(CE_CURRENT_POWER(), CE_POWER_PER_LEVEL())),
           (int)spawn_life, CE_CURRENT_POWER(), CE_POWER_PER_LEVEL());
}

/* 每游玩帧跑一遍令牌池：① VM 还在不在 ② 走位（位移 + 反弹 + 出界）③ 摆 VM ④ 彩虹计时 ⑤ TTL ⑥ 自机接触。
 * ★ 换池（2026-09-12）：以前这里要「找敌机实体 → 写它的 `final_pos`/`abs_pos`」；现在**没有实体**了 ——
 *   令牌的坐标就是 `t->motion.x/y`，写画面就是 `ce_anm_set_pos`（一只 token 一个 VM）。
 * ⚠ 池是**紧凑数组**：回收用 swap-remove ⇒ 循环**不能 `++i`**（尾元素会搬进来，也要过一遍筛）。 */
static void ufo_pentagon_hint_update(ufo_card_t *s);   /* 定义在下面（算判据 + 驱动框 VM，见它的注释块）*/
static void ufo_slot_full_bonus(ufo_card_t *s, float x, float y);  /* 定义在下面（P1 的满槽补偿）*/
/* ★ 2026-09-23（owner 第 9 条）：把场上所有令牌切进"离场态"（对话那一刻调）。
 * 只改运动状态（纯逻辑层 `ufo_tok_motion_leave()`），**不删 VM** —— 一手的回收点只有"出界"这一条，
 * 让它们自己飞出去（几秒内）比当场消失更像原版。幂等 ⇒ 对话期间每帧调也无副作用。 */
static void ufo_tokens_leave(ufo_card_t *s)
{
    int i;
    for (i = 0; i < s_tokpool.n; ++i) ufo_tok_motion_leave(&s_tokpool.tok[i].motion);
    if (s_tokpool.n && !s->tokens_leave_logged) {
        s->tokens_leave_logged = 1;
        ce_log("ufo: DIALOGUE - %d token(s) switched to the leave flight (no more bounce, speed x%.1f; "
               "owner 2026-09-23 ruling #9) - they exit the frame and get recycled as usual",
               s_tokpool.n, (double)UFO_TOK_LEAVE_SPEED_MUL);
    }
}

static void ufo_tokens_tick(ufo_card_t *s)
{
    float px, py, pick_r;
    int i;
    ufo_player_xy(&px, &py);
    pick_r = ufo_tok_pick_radius();          /* ★ P5：半径每帧取一次（本作自己的收集半径）*/
    for (i = 0; i < s_tokpool.n; ) {
        ufo_tok_t *t = &s_tokpool.tok[i];
        int32_t color;
        float tx = 0.0f;                     /* 接触收取时用：令牌的 x（**撤之前**抄，见 ⑥ 的注）*/

        /* ① VM 不在了（**且自愈也建不出来**）⇒ 这一只令牌**没有画面了**，整套退休（不入槽、不算收）。
         * 走到这一条的三类情形：引擎在别处摧毁了这个 VM **并且** `CE_ABILITY_ANM()` 这时也取不到
         * （anm 还没装载 / 已卸载）；`on_delete` 那一刻引擎已经拆了 anm；本帧的 `ce_anm_spawn` 失败。
         * ⚠ 先给一次**自愈**机会（与令牌自己那一支同一个口径）：句柄没了就重建一个 —— 只有连
         *   `ufo_tok_vm_spawn` 也建不出来（它会把 `anm_id` 留 0）才算真没了。
         *   ★ 这条让「VM 被引擎在别处销毁」不再必然损失一枚令牌（换池前那条路是直接退休）。
         * ⚠ 这条分支**绕过了 `ufo_tok_kill()`**（句柄已经是 0）⇒ 直接 swap-remove 回收槽。 */
        if (!t->anm_id || !ce_anm_get_vm((uint32_t)t->anm_id)) {
            t->anm_id = 0;
            ufo_tok_vm_spawn(t);                       /* 自愈一次 */
            if (!t->anm_id) {
                ce_log("ufo: token VM is gone AND could not be respawned (color %d, serial %u) - "
                       "retiring the pool entry (no collection, no reward)", (int)t->color, t->vor);
                t->color = UFO_COLOR_NONE;
                ++s->lost_tokens;
                ufo_tok_penta_kill(i);                 /* 框跟着一起收（不然留一个空五边形）*/
                ufo_tok_remove(i);
                continue;                              /* ★ 不 ++i */
            }
        }
        /* ② ★★ 令牌**运动**（一手 `th12:0x425c20` 的 state 6；用户口述的「飞碟在场上…飞行」）：
         *   位移 + 四边反弹 + 1200 帧后不再反弹 + 出界即撤 —— 常量与指令地址全在
         *   `ufo_core.h` 的「令牌的运动」那一块，逻辑在 `ufo_tok_motion_step()`（有主机单测）。
         *   ★★ 换池：位置直接写**我们自己的** `t->motion`（不再双写敌机的 `final_pos` / `abs_pos`
         *     —— 那两个偏移是「真敌人壳」逼出来的，已经随壳一起删掉了）。 */
        if (ufo_tok_motion_step(&t->motion)) {
            /* 一手：飞出外框 = 该道具 state 0（回收），**不入槽**（`0x42655b`）*/
            ++s->lost_tokens;
            ufo_tok_kill(i);                           /* 内部 swap-remove ⇒ 不 ++i */
            continue;
        }
        /* ③ 画面：把**这一个** VM 摆到令牌坐标上（放在 TTL / 成组早停**之前** ——
         *   令牌只要还在场上就得跟着走，包括「槽里有成组、这一颗暂时不收」那段等待期）。 */
        ufo_tok_vm_follow(t);
        /* ④ ★ A8：彩虹令牌的三段中断 + 换色（60 px 闩 / 90 帧起闪 / 150 帧换色）。
         * ⚠ 也放在早停之前：计时器与玩家距离无关地每帧要走（一手在道具的 tick 里，不受「槽满了没」影响）。 */
        ufo_tok_rainbow_tick(s, t, px, py);
        /* ⑤ TTL 兜底（一手靠「出界即删」回收；我们的 VM 不会自己消失 ⇒ 这一条守着漏网）*/
        if (t->ttl) --t->ttl;
        if (!t->ttl) {
            ufo_tok_kill(i);
            ++s->lost_tokens;
            continue;
        }
        color = t->color;
        /* ⑥ **接触收取**：半径 = **本作（TH18）自己的收点半径**（P5 轮已落地 —— 读 `player+0x4798c`，
         *    不是自定值；见 `ufo_tok_pick_radius()` 的注）。
         * ★★ 2026-09-23（owner 实跑第 6 条）：「**目前依旧召唤 UFO 之后会把场上的所有飞碟 token 清空**」
         *   —— 真因就在这一段：P1 那条「槽满 ⇒ 令牌照收、只给满槽补偿」的支**原先排在接触闸之前**
         *   ⇒ UFO 一在场（或有一个组干等着），**全池**的令牌当帧都被 `ufo_tok_take_and_kill()` 吃掉
         *   （与距离无关）⇒ 场上所有 token 瞬间消失。一手不是这样：`FUN_004270b0` 是**被自机碰到**
         *   时才进的那条路（`0x4270b0` 的调用点在道具接触分派里）⇒ 闸必须在前面。
         *   ⇒ 本轮把「接触闸」提到 P1 支**之前**（下面 `ufo_tok_pick` 先判，没碰到就 `++i; continue`）。 */
        if (!ufo_tok_pick(t->motion.x, t->motion.y, px, py, pick_r)) { ++i; continue; }
        /* 接触即入槽：**先取值、再撤令牌**（顺序见 `ufo_tok_take_and_kill`）
         * ⚠ 令牌的 x 也要**在撤之前**抄下来：`ufo_tok_take_and_kill` 是 swap-remove，
         *   调用后 `t` 指向的那一格已经换了人（音效的声像要用**这一颗**令牌的 x）。*/
        tx = t->motion.x;
        /* ★★ P1（一手 `0x4270b0` 开头）：**槽满（= 已有一个干等着的组 / UFO 在场）⇒ 令牌照收，
         *   但给的是「满槽补偿」而不是进槽**：一手是 `[PIV] += 100000` + 飘字 1000，令牌**被消耗**。
         *   ⚠ 旧写法是"不收也不撤、留在场上等组被消费掉" —— 那是一手没有的行为（一手照收照消耗）。
         *   ⚠ **必须在接触闸之后**（见上面 ⑥ 的注；2026-09-23 修「召唤即清场」那一条）。 */
        if (ufo_present_group_ready(s)) {
            float bx = tx, by = t->motion.y;
            color = ufo_tok_take_and_kill(i);           /* 令牌被消耗（一手同款：state=0 + 音效 + 淡出）*/
            ufo_se(UFO_SEV_TOKEN_FULL, UFO_SE_TOKEN, bx,
                     "一手 th12:0x426da4 共同尾（槽满再收一枚 ⇒ 同一枚音）");            /* 一手的共同尾也播音效 0x26（= 我们的入槽音）*/
            ufo_slot_full_bonus(s, bx, by);
            continue;                                   /* swap-remove ⇒ 不 ++i */
        }
        color = ufo_tok_take_and_kill(i);              /* 内部 swap-remove ⇒ 不 ++i */
        /* ⚠ 这里不是死亡栈（`on_item_frame` 是每游玩帧的门卫位次）⇒ 可以立刻建本体；
         *   死亡事件那条路只登记、由 `on_tick_2` 兑现。 */
        if (ufo_token_to_slot(s, color, tx) == UFO_PUT_READY) ufo_try_summon(s);
    }
    ufo_pentagon_hint_update(s);
}

/* ★★ **五边形框**（用户 2026-09-12 裁决 ③）—— 每帧算条件 **并驱动框 VM**（2026-09-12 A7 实装）----
 * 触发条件（用户原话）：「玩家手上有**两个令牌**（= 再来一枚就能成组）时，屏幕上**这类令牌**外面
 * 会有一圈五边形框作提示」。⇒ 两级判据（都在纯逻辑层、有单测）：
 *   ① `ufo_group_pentagon_hint(&s->grp)`：槽里此刻是不是「差一枚就成组」（便宜的早停；
 *      当前规则下等价于「槽里正好 2 枚」—— 4 个颜色桶里任意两枚都补得出组，见 ufo_core.h 的注）。
 *      ★ 这一条同时**涵盖了原版那条「已收满 3 枚就不提示」**（`th12:0x42658a cmp [0x4b0c58],3`）：
 *      判据要求 `filled == 2`，槽满 3 枚必然为假。
 *   ② `ufo_pentagon_hint_for_color(&s->grp, t->color)`：**场上的这一枚**收了就能成组 ⇒ 它才该被画框
 *      （框的颜色 = 收了它会得到的那只 UFO 的颜色，见 `ufo_pentagon_label_for_color()`）。
 * 本函数把②的结果逐令牌记进 `ufo_tok_t.pentagon_hint`，并**照原生的生命周期**驱动框 VM：
 *   · 条件成立 + 还没有框 ⇒ **建框 + 发一次标签**（`ufo_tok_penta_spawn`）；
 *   · 条件成立 + 已经有框 ⇒ **什么都不做**（原生 `0x4265d5` 的 `jne`：不重建、不重发标签）；
 *   · 条件不成立 ⇒ **删框**（原生那六处 case 的 else 支：`call 0x461a70` + 句柄清 0）；
 *   · 每一帧都把框移到令牌实体坐标上（`ufo_tok_penta_follow`；原生不跟随，见它的注）。
 * ✅ 美术来源 = **原生**（`bullet.anm` script211 逐条照抄 ⇒ 脚本 223，程序画的五边形、无贴图）；
 *    **不用 `drawRect` 硬凑**（用户明确要求）。
 * ⚠ 调用点必须**不是死亡栈**（本函数会 `ce_anm_spawn`）—— 现在只有 `ufo_tokens_tick()` 一处，
 *   那是 `on_item_frame` 的门卫位次（每游玩帧），合规。 */
static void ufo_pentagon_hint_update(ufo_card_t *s)
{
    int i, any = 0;

    if (!ufo_group_pentagon_hint(&s->grp)) {
        /* 早停：槽里不是 2 枚（或某种颜色都补不出组）⇒ 全部清零 + **把框全收了**
         * （原生：条件一不成立就删框，不留一个孤零零的五边形在场上）。 */
        for (i = 0; i < s_tokpool.n; ++i) {
            s_tokpool.tok[i].pentagon_hint = 0;
            ufo_tok_penta_kill(i);
        }
        for (i = s_tokpool.n; i < UFO_TOKEN_CAP; ++i) ufo_tok_penta_kill(i);  /* 池外的残留句柄也收 */
        s->pentagon_any = 0;
        s->pentagon_logged = 0;                        /* 条件消失后允许再报一次 */
        return;
    }
    for (i = 0; i < s_tokpool.n; ++i) {
        ufo_tok_t *t = &s_tokpool.tok[i];
        int label;

        /* ★ 2026-09-18（新增 1「得是按色」）：判据用**当前显示色**（变色令牌按它此刻显示的那一色）
         *   —— 一手 `bullet.anm` script211 就是这个口径；原来的「彩虹 = 通配」模型作废。 */
        label = ufo_pentagon_label_for_color(&s->grp, ufo_tok_display_color(t));
        t->pentagon_hint = (uint8_t)(label ? 1 : 0);
        if (!label) { ufo_tok_penta_kill(i); continue; }
        /* ★ 换池：不再需要「先找敌机实体」那一步（以前实体没了就 `continue`、不建框）——
         *   现在令牌与它的 VM **同生共死**（VM 没了那一支在上一趟 `ufo_tokens_tick` 里
         *   已经把整条池记录 + 框一起收了）⇒ 走到这里必然有 VM 可跟。 */
        ufo_tok_penta_follow(i, t);                    /* 先跟随（顺带自愈掉死句柄）*/
        ufo_tok_penta_spawn(i, t, label);              /* 没框才建；有框则原样留着*/
        any = 1;
    }
    s->pentagon_any = (uint8_t)any;
    if (any && !s->pentagon_logged) {
        s->pentagon_logged = 1;
        ce_log("ufo: PENTAGON HINT ON (2 tokens in the slots and at least one field token would "
               "complete the group) - frame = ability.anm script %d layer %d (TH12 bullet.anm "
               "script211, ported verbatim)", UFO_PENTAGON_ANM_SCRIPT, UFO_PENTAGON_ANM_LAYER);
    }
}

/* ==================== ★★ 掉落物实体：起 / 跟 / 收 / 结算（用户 2026-09-18）====================
 * 用户两条口径都在这里落地：
 *   · 「击破还会掉落一个**被自动吸收的压缩包**（**应该是有实体的**，只是被自动吸收过来）」
 *     ⇒ `ufo_drop_spawn_bag()`：实体 = 一手赏金袋的外观（脚本 296/297/298），
 *        运动 = 一手 state 8（原地 60 帧 → 加速 0.2/帧 上限 12 归航到自机）；
 *   · 「P6 整颗 Bomb」⇒ `ufo_drop_spawn_bomb()`：★ 2026-09-23 改成**生成 TH18 原生道具 type 7**
 *     （`ce_spawn_item(7, …)`；TH18 本来就有"整颗 Bomb"这个 type，原版自己也用它掉整颗 B）
 *     ⇒ 运动/收点/结算全归引擎的道具池；我们只额外附一支**卡面 VM**（脚本 301）跟着它画。
 * 收集 = 压缩包走与令牌同一条判据（本作自己的收点半径 `player+0x4798c`，P5 轮已落地）；
 * 整颗 B 走**引擎自己的**三条 V 门 + 30/70px 收点（见 `ufo_drop_spawn` 那一整段）。 */
static void ufo_drop_kill(int i)
{
    if (i < 0 || i >= UFO_DROP_POOL) return;
    if (s_drops[i].anm_id) { ce_anm_delete(s_drops[i].anm_id); s_drops[i].anm_id = 0; }
    memset(&s_drops[i], 0, sizeof s_drops[i]);
}

static void ufo_drop_kill_all(void)
{
    int i;
    for (i = 0; i < UFO_DROP_POOL; ++i) ufo_drop_kill(i);
}

static int ufo_drop_slot_free(void)
{
    int i;
    for (i = 0; i < UFO_DROP_POOL; ++i)
        if (!s_drops[i].core.alive && !s_drops[i].anm_id) return i;
    return -1;
}

/* 挑脚本号：一手按**携带的账**选那一支（type 16 = 只有 cntA ⇒ 六个子图标全是 P；
 * 17 = 只有 cntB ⇒ 全是点；18 = 两者都有 ⇒ 交替）。与我们自建的同一件实体对上。 */
static int ufo_drop_bag_script(int32_t cnt_p, int32_t cnt_money)
{
#ifdef CE_ANM_ABILITY_SCRIPT_UFO_BOUNTY_BAG
    if (cnt_p > 0 && cnt_money > 0) return CE_ANM_ABILITY_SCRIPT_UFO_BOUNTY_BAG;
    if (cnt_p > 0)                  return CE_ANM_ABILITY_SCRIPT_UFO_BOUNTY_BAG_P;
    return CE_ANM_ABILITY_SCRIPT_UFO_BOUNTY_BAG_MONEY;
#else
    return 296;                                    /* fallback：`make anm` 之前的字面量 */
#endif
}

static int ufo_drop_spawn(ufo_card_t *s, int32_t kind, float x, float y,
                          int32_t color, int32_t cnt_p, int32_t cnt_money, int32_t mult)
{
    int slot = ufo_drop_slot_free();
    ufo_drop_slot_t *d;
    void *anm = CE_ABILITY_ANM();
    int script, layer = UFO_DROP_ANM_LAYER;

    if (!s || !anm || slot < 0) {
        ce_log("ufo: drop spawn FAILED (kind %d; pool free? %d, anm %p)", (int)kind, slot, anm);
        return 0;
    }
    d = &s_drops[slot];
    memset(d, 0, sizeof *d);
    d->core.kind = kind;
    d->core.alive = 1;
    d->core.x = x;
    d->core.y = y;
    d->core.ufo_color = color;
    d->core.cnt_p = cnt_p;
    d->core.cnt_money = cnt_money;
    d->core.mult_permille = mult;
    d->core.faith_big = s->faith_pulled_big;       /* ★ 被吸走的信仰点跟着压缩包回来 */
    d->core.faith_mid = s->faith_pulled_mid;
    d->core.faith_mask = s->faith_pulled_mask;
    if (kind == UFO_DROP_KIND_BOMB) {
        /* ★★★ 2026-09-23（owner 第 5 条 + 新口径「满 B 的运动模式应当**完全复刻 B碎**…参考**信仰点**的
         *   运动模式，完全复刻即可。主要是**上线收点**（以及其他全屏收点）以及 **th15 变黄变慢抖动**」）
         *   —— **改生成 TH18 原生的整颗 Bomb 道具（item type 7）**，不再自建 VM 走自己的物理：
         *     一手 `ItemManager__spawn_item` `th18:0x446f40`：type 7 ⇒ 收集 handler `0x4464d5`
         *     （`[0x4ccd58]++` 加一颗弹 + 满则清碎片 `0x4ccd5c` + SE `0x2e` + HUD `0x4420e0`）；
         *     **原版自己就用它掉整颗 B**：`0x40d2e0`（辉夜卡死亡帧）`push 7` → 同一入口。
         *     白送的一手行为（自建 VM 拿不到）：三条 V 门（①上线 `poc_height (player+0x47998) >
         *     自机 y` `0x445baa` ②放 B 后前 60 帧 ③对话 `GUI+0x1b0 != 0` ⇒ 共同落点 `0x445f63`
         *     翻 state 3 + 写磁速）、30px 直收、70px 环吸、出界删除、磁速爬升上限 12.0、
         *     **TH15 三件套**（减速期抖 + 压暗 `RGB(255,160,255)`，门 ①state==1 ②年纪≥32
         *     ③`mgr+0xe6bb14 < 1.0`）、暂停/菜单处理、池的分配回收、以及**收下时的引擎副作用**。
         *   ⚠ 角度/速度照一手普通掉落（`−π/2`、`2.2`：`0x43061e` 那条 `Enemy__drop_items` 的实参，
         *     与 B碎 掉落逐字相同 ⇒ 这就是 owner 要的"完全复刻 B碎 的运动"）。
         *   ⚠ 原生 script118 自己会画那颗**绿色整 B 图标**（32×32、自转 45°/帧 ×32 帧）——
         *     我们**不**重复画它，只在它**后面**叠一层自己的卡面（见下），观感 = owner 说的
         *     「微微 rotate + 背光」。*/
        float p3[3];
        p3[0] = x; p3[1] = y; p3[2] = 0.0f;
        d->item = ce_spawn_item(UFO_DROP_NATIVE_BOMB_TYPE, p3, UFO_DROP_NATIVE_BOMB_ANGLE,
                                UFO_DROP_NATIVE_BOMB_SPEED);
        if (!d->item) {
            ce_log("ufo: WHOLE BOMB native item spawn FAILED (type %u) - no reward this time",
                   (unsigned)UFO_DROP_NATIVE_BOMB_TYPE);
            ufo_drop_kill(slot);
            return 0;
        }
        /* 卡面 VM（我们自己的贴图，脚本 301 = TH18 `abcard.anm` script16 的配方：加算 + 呼吸缩放）：
         * 位置每帧跟随**原生道具**的坐标（`item+0xc2c` = 世界坐标 float3），见 `ufo_drops_draw_follow()`。*/
        script = CE_ANM_ABILITY_SCRIPT_UFO_BOMB_ITEM;
        layer = UFO_DROP_BOMB_ANM_LAYER;
    } else {
        script = ufo_drop_bag_script(cnt_p, cnt_money);
    }
    if (!d->item) {                                   /* 压缩包（自建 VM）那一支：照旧 */
        d->anm_id = ce_anm_spawn(anm, script, layer);
        if (!d->anm_id) {
            ce_log("ufo: drop VM spawn FAILED (kind %d, script %d) - the entity stays invisible",
                   (int)kind, script);
            ufo_drop_kill(slot);
            return 0;
        }
        ce_anm_set_pos(d->anm_id, x, y, 0.0f);
    } else {
        d->anm_id = ce_anm_spawn(anm, script, layer);  /* 卡面 VM：位置由跟随那一支每帧写 */
        if (d->anm_id) ce_anm_set_pos(d->anm_id, x, y, 0.0f);
        else ce_log("ufo: WHOLE BOMB card-face VM spawn FAILED (script %d) - the native green "
                    "whole-B icon shows alone", script);
    }
    ce_log("ufo: drop entity up (kind %s, vm %u, script %d, layer %d) at (%.0f, %.0f)%s",
           kind == UFO_DROP_KIND_BOMB ? "WHOLE BOMB (native item type 7)" : "bounty PACKAGE",
           d->anm_id, script, layer, (double)x, (double)y,
           kind == UFO_DROP_KIND_BOMB ?
           "  <- native item: POC/line collect, full-screen collect, TH15 slow+tint+shake are the "
           "ENGINE's now; our VM only draws the B card face on top of it" :
           "  <- carries P/money + the pulled faith dots back to the player");
    return 1;
}


/* ============================ ★★ #20 击破 UFO 的 **×N** 弹出（2026-09-23 重做：ASCII）=================
 * owner（2026-09-23）：「击破 UFO 时有个**大大的 ×一个数**的 ascii 贴图或特效」。
 * ★★ 2026-09-23 第二轮（owner 实跑第 2 条）：「**我能看见一个黑色的 *1 从画面的下方缓慢地往上飞，
 *   一直也不消失**」⇒ 上一版（字形 VM 通道）有**三个**毛病，这一轮全按一手重做：
 *   ① **位置错 448 表面 px**：上一版把世界坐标按 `480 + y×2` 折到表面 px，而 ASCII 空间是
 *      `x = 320 + wx`、`y = 16 + wy`（与 #9 倒计时同一条换算）⇒ 整条弹字被画在画面**下方**。
 *   ② **寿命错**：一手 `th12:0x44ab52 movl $0x2d0,0x28(%esi)` 写的是 **`zUfo+0x28` = UFO 自己的存活上限
 *      （720 帧）**，**不是**弹字寿命；弹字活的是 `zUfo+0x64`（「弹字剩余帧」，**击破时置 120**，
 *      `Ufo__on_draw_16` 的门 `cmp [ebx+0x64],0 … jle`）⇒ 旧版 720 帧所以"一直也不消失"。
 *   ③ **颜色发黑**：旧版走我们自己的字形通道（`MUL.png` = 深灰 (28,28,28) + `TS_DIGIT_*` = 灰 (101,101,101)），
 *      而 `ce_anm_set_color` 是**乘**色 ⇒ 红 × 深灰 = 近黑（用户看到的"黑色的 *1"就是它）。
 *   ⇒ 一手本来就是 **ASCII 文字**：`Ufo__on_draw_16`（`th12:0x44a5d0`）里
 *      `printf` 格式串 = `0x4a2154` = **`"*%1.1f"`**（我直读字符串）、`font 4`、**scale 2.0**
 *      （`fld [0x4a4014]` = 2.0）、颜色**每 `%10` 帧闪一次**：
 *      `edx = [ebx+0x64] % 10`，`setge edx,5` ⇒ **≥5 灰 `0xff808080` / <5 黄**（那两个 dword 的算式见
 *      `0x44a644`–`0x44a658`；一手里黄那一支算出来是 `0x00FFFF00`，alpha 位为 0 —— ASCII 管理器不看
 *      alpha，我们按显示意图取 **`0xffffff00`**，🟡）。
 *   ⇒ 本版：**改回 ASCII 通道**（`ce_hud_text_ex`，与 #9 倒计时同一条路），只留"值 + 位置 + 剩余帧"三个数。
 *   ⚠ ASCII 文字**盖不进暂停模糊罩**（`SDK.md` §16）—— 与 #9 倒计时同一个已知差异，`TUNING.md` 有登记。 */
/* ★ #12 击破爆炸的层号：一手 `bullet.anm` script208/209 自己写的 `layer(12)`（世界层、原值）*/
#define UFO_BREAK_FLASH_LAYER 12

#define UFO_XN_LIFETIME   120u           /* ✅ 一手 `zUfo+0x64` = "弹字剩余帧"，击破时置 120（2 秒）*/
#define UFO_XN_DY         24.0f          /* ❓自定：画在 UFO **下方** 24 单位（一手弹字位置存在 `+0x54`，写点未逐条钉死）*/
#define UFO_XN_SCALE      2.0f           /* ✅ 一手 `fld [0x4a4014]` = 2.0 */
#define UFO_XN_COLOR_GREY 0xff808080u    /* ✅ 一手 `rem%10 >= 5` 那一支（dword 直读 0xff808080）*/
#define UFO_XN_COLOR_YEL  0xffffff00u    /* 🟡 一手另一支算出 0x00ffff00（alpha=0，ASCII 不看 alpha）⇒ 补 255 */
#define UFO_XN_FLASH_MOD  10u            /* ✅ 一手 `idiv 0x0a`：按剩余帧的个位闪 */
/* ★ 2026-09-24（owner 指出原版有第二行）：得分那一行的两个一手数 —— 位置 = 同一点 **y + 32.0**
 *   （`th12:0x44a68f fadd [0x4a3d70]`，那个常量就是 **32.0**）、**scale 1.0**（倍率那行是 2.0）。*/
#define UFO_XN_SCORE_DY     32.0f        /* ✅ 一手 `[0x4a3d70]` = 32.0（y 向屏幕下方）*/
#define UFO_XN_SCORE_SCALE  1.0f         /* ✅ 一手第二条 printf 的 scale = 1.0 */

static uint16_t s_xn_left;               /* 剩余帧（0 = 不画）*/
static float    s_xn_wx, s_xn_wy;        /* 世界坐标（ECL 640 空间）*/
static int32_t  s_xn_permille;           /* 倍率（千分比；显示 `*%1.1f`）*/
/* ★★ 2026-09-23：**#20 ×N 弹字与 #9 剩余时间都从 ASCII 换成「贴图 VM 文字」**
 *   （owner：「剩余时间画在模糊层之后，暂停不会模糊」）—— ASCII 永远进不了暂停模糊罩，
 *   VM 放 UI 层 25 才折到 38 那一个绘制桩（`SDK.md` §16）。状态放 DLL 静态区（同 ×N 的其余几个数）。*/
static ce_hud_vm_text_t s_xn_text;       /* ×N 的那 4 个字形 VM */
static ce_hud_vm_text_t s_xn_score_text; /* ★ 它底下那行**得分**（一手 `on_draw_16` 的第二条 printf）*/
static int32_t s_xn_score;               /* 要显示的分（`zUfo+0x68`）*/
static ce_hud_vm_text_t s_timer_text;    /* `11:00` 的那 5 个字形 VM */

/* ★★ 2026-09-24（owner：「不知道哪儿冒出来一个 **14.0 的 UI 一直赖着不走**，并且**不被暂停模糊**」）：
 *   **暂停时 `on_draw` 照样会被调**（SDK 的菜单门注里写着），而本卡的两处 HUD 文字每帧都调
 *   `ce_hud_vm_text_set()` —— 只要那一帧有 VM 需要**新建**（引擎在别处收掉了句柄 / 串变长），
 *   新 VM 就落在**暂停模糊罩之后**（罩子是暂停时新起的 VM、尾插 = 画在后面）⇒ 那几个字形看起来
 *   "没被模糊"，而且会一直留在屏上。
 *   引擎自己判"这一帧是不是游玩帧"的判据就在 `GameThread+0xb0`（`th18:0x446ec0`，道具池 body 的门）：
 *   `flags & 3` 或 `flags & 0x400` 置位 ⇒ 那一帧不是正常游玩帧。
 *   ⇒ 本卡的 HUD 文字照同一条门：**非游玩帧什么都不画**（不建 VM、也不清 —— 解除后下一帧照旧）。
 *   ⚠ 不影响别的路径：令牌 / 掉落物是**世界层** VM（引擎自己的道具池用同一条门），位置与生命周期
 *     都不依赖这里。*/
static int ufo_play_frame(void)
{
    const uint8_t *gt = (const uint8_t *)CE_GAME_THREAD();
    uint32_t f;
    if (!gt) return 0;
    f = *(const uint32_t *)(gt + CE_GT_FLAGS);
    return (f & 0x3u) == 0u && (f & 0x400u) == 0u;
}

static void ufo_xn_kill(void)
{
    s_xn_left = 0;
    ce_hud_vm_text_clear(&s_xn_text);           /* ★ 长持续效果铁律：VM 也要一起收 */
    ce_hud_vm_text_clear(&s_xn_score_text);     /* ★ 得分那一行同命 */
    s_xn_score = 0;
}

/* 起一次（开奖那一刻调）。`wx/wy` = 掉落物的**世界坐标**（居中系）。 */
/* ★★★ 2026-09-24（owner 指出原版 ×N 下面还有一行"得分"）—— **这一击的得分**：
 *   一手把它算在**击破那一刻**（`th12:0x44af00` 里按色四条分支写 `zUfo+0x68`，
 *   公式 = 我们已经在用的 `ufo_drop_settle_value()`：`cntA×PV×mult + cntB×PV`（红）
 *   或 `trunc(PV×cntB×mult)` 再 `/10×10`（蓝/绿/彩虹））⇒ 与「赏金包」收集时给的分**同一条公式**。
 *   ⚠ 收集那一刻的 PV 可能变了（本作 PV 由金钱档推出来）⇒ 弹字显示的是**击破时那一刻**的值
 *     （一手也是击破时算的 ✓），与包里最终给的分可能有极小差异 —— 记在 `TUNING.md`。*/
static void ufo_xn_spawn(float wx, float wy, int32_t mult_permille, int32_t score)
{
    s_xn_wx = wx;
    s_xn_wy = wy;
    s_xn_permille = (mult_permille > 0) ? mult_permille : 1000;
    s_xn_score = (score > 0) ? score : 0;
    s_xn_left = (uint16_t)UFO_XN_LIFETIME;
    ce_log("ufo: xN popup up - mult %d.%03d + score %d -> vm-text \"*%%1.1f\" (+ score line at +32) "
           "at world (%.0f, %.0f) for %u frames "
           "(one-hand th12:0x44a5d0: format 0x4a2154 \"*%%1.1f\", scale 2.0, colour flashes "
           "grey/yellow every %%10 frames; lifetime = zUfo+0x64 = 120)",
           s_xn_permille / 1000, s_xn_permille % 1000, (int)s_xn_score, (double)wx, (double)wy,
           (unsigned)UFO_XN_LIFETIME);
}

/* 每帧（**draw 相位**：ASCII 只能在 `on_draw` 里画）：画字 + 倒数。
 * ⚠ 位置/寿命都在**卡状态之外**（文件静态）—— 与令牌池同一个口径（`on_ctor` 的 memset 清不到）。
 * ★ 长持续效果：`ufo_xn_kill()` 在 `on_delete`/换关时清掉（只清"剩余帧"这一个数 ⇒ 幂等）。 */
static void ufo_xn_tick(void)
{
    float x, y;
    uint32_t rem;
    char buf[8];
    if (!s_xn_left) return;                            /* 已经收过尾（见下）*/
    if (!ufo_play_frame()) return;                     /* ★ 暂停/非游玩帧：一个字都不画（见 `ufo_play_frame`）*/
    --s_xn_left;
    if (!s_xn_left) {                                  /* ★★ 到点**必须把字形 VM 收掉** —— 上一版在这里
                                                        * 只是下一帧 `return`，于是 ×N 的字**永远留在屏上**
                                                        * （owner：「一直赖着不走」）。*/
        ce_hud_vm_text_clear(&s_xn_text);
        ce_hud_vm_text_clear(&s_xn_score_text);
        s_xn_score = 0;
        return;
    }
    rem = (uint32_t)s_xn_left % UFO_XN_FLASH_MOD;
    /* 位置：640×480 空间（x 以场地中心 320 为 0、y 以场地上沿 16 起算，与 #9 倒计时同一条换算）*/
    x = 320.0f + s_xn_wx;
    y = 16.0f + s_xn_wy + UFO_XN_DY;
    /* ★★ 2026-09-23（owner：「剩余时间画在模糊层之后，暂停不会模糊」）—— 同一条毛病也在 ×N 上：
     *   ASCII **永远进不了暂停模糊罩**（`SDK.md` §16）⇒ 两处都改成**贴图 VM 文字**
     *   （`ce_hud_vm_text_*`，UI 层 25 折到 38、与罩子同一个绘制桩）。
     *   ⚠ 串是**手写**的（不为 4 个字符引 `snprintf` 的格式化机器，与上面 `tag[6]` 同一个口径）：
     *     一手格式 = `"*%1.1f"` ⇒ `*` + 整数位 + `.` + 一位小数（四舍五入，`%1.1f` 的行为）。*/
    {
        uint32_t p = (uint32_t)s_xn_permille;
        uint32_t tenths = (p + 50u) / 100u;            /* 四舍五入到 0.1 */
        uint32_t whole = tenths / 10u;
        /* ★ 2026-09-24：倍率域是 1.0..**10.0**（蓝/金那条 `2000 + 8000·fill` 在 fill→1 时逼近 10000）
         *   ⇒ 整数位**可能是两位**；上一版只打一位（`whole % 10`）⇒ 9.95 以上会显示成 `*0.0`。*/
        int k = 0;
        buf[k++] = '*';
        if (whole >= 10u) buf[k++] = (char)('0' + (whole / 10u) % 10u);
        buf[k++] = (char)('0' + (whole % 10u));
        buf[k++] = '.';
        buf[k++] = (char)('0' + (tenths % 10u));
        buf[k] = '\0';
    }
    /* ★ 2026-09-24：改用 **`CE_HUD_FONT_TH18_FONT2B`**（`*` 与 `:` 只在那套字形集里，
     *   而这个字形表绑的 entry 决定了 sprite 号往哪张表查 —— 详见脚本 332 的头注）。*/
    ce_hud_vm_text_set_font(&s_xn_text, x, y,
                            (rem >= UFO_XN_FLASH_MOD / 2u) ? UFO_XN_COLOR_GREY : UFO_XN_COLOR_YEL,
                            CE_ASCII_ALIGN_CENTER, UFO_XN_SCALE, buf, "ufo-xn",
                            CE_HUD_FONT_TH18_FONT2B);
    /* ★★★ 2026-09-24（owner：「爆炸之后会有一个 **×[数]** 的黄色大数字，**以及在其底下的一个量级
     *   应该是得分的数字**」）—— 一手 `Ufo::on_draw_16`（`th12:0x44a5d0`）里确实是**两条** ASCII：
     *     · 第 1 条（`0x44a664`）：`push 0x4a2154` = **`"*%1.1f"`**，参数 = `zUfo+0x60`（倍率）、
     *       颜色 = 每 `%10` 帧在灰 `0xff808080` / 黄 之间翻、**scale 2.0**（`fld [0x4a4014]`）、font 4 ✓
     *       —— 就是上面这一行。
     *     · 第 2 条（`0x44a6a9`，`test [ebx+0x68],…; jle` 才调）：位置 = 同一点 **y + 32.0**
     *       （`fld [esp+0x44]; fadd [0x4a3d70]`）、颜色 **white（`[esi+0x18f80] = -1`）**、
     *       **scale 1.0**、参数 = **`zUfo+0x68` = 这一击的得分**（`0x44af00` 里按色四条分支写进去）。
     *   ⇒ 本行就是它：白色、scale 1.0、画在 ×N 下方 **32 单位**，而且**只有得分 > 0 才画**（一手同款门）。
     *   ⚠ 一手那个数是按 `%d,%.3d,%.3d`（千分位）打的（`0x4021a0`，`<1000` 用 `%d`）——
     *     我们的字形集里**没有逗号** ⇒ 先按**纯数字**打；要千分位就得再补一个 `,` 字形（一处）。*/
    if (s_xn_score > 0) {
        char sbuf[16];
        uint32_t v = (uint32_t)s_xn_score;
        int k = 0, j;
        char tmp[12];
        do { tmp[k++] = (char)('0' + (v % 10u)); v /= 10u; } while (v && k < 11);
        for (j = 0; j < k; ++j) sbuf[j] = tmp[k - 1 - j];
        sbuf[k] = '\0';
        ce_hud_vm_text_set_font(&s_xn_score_text, x, y + UFO_XN_SCORE_DY, 0xffffffffu,
                                CE_ASCII_ALIGN_CENTER, UFO_XN_SCORE_SCALE, sbuf, "ufo-xn-score",
                                CE_HUD_FONT_TH18_FONT2B);
    }
}

/* ==================== ★★ #22 吸收数记录面板（TH12 `front.anm` script134/135）====================
 * 一手（`local/th12.v1.00b/anm/front/front.anm.txt`）：
 *   · 父 **script134** = `sprite(sprite102)`（= `front_ufo.png` 的 96×96）+ `scriptNew(script135)` +
 *     `resampleMode(1)` + `layer(23)` + `scale(0,0)` → `scaleTime(20, 0, 1.0, 1.0)` + **`pos(528, 288)`**；
 *   · 子 **script135** = 内容那半，+20 帧 `delete()`；**中断 1 = 缩回**（134 自己不 delete）。
 * 卡侧脚本 = `ability.anm` 的 **304（父）/ 305（子）**（生成器 `make_ufo_hud_art.py` 早就移植好了，
 * 但直到 2026-09-23 才接上 —— 那之前 `ufo.c` 一个字都没引用它）。
 * **内容（吸收数）**：TH12 用 ascii manager 画 ⇒ 我们走**自己的字形通道**（与 #20 的 ×N 同一套：
 * `SCRIPT_HUD_GLYPH` + `ATLAS_HUD_DIGIT_0..9`）。
 * **触发**：每帧看「吸收数」有没有变（= `cnt_p + cnt_money`，就是这只 UFO 吃进去的 token 数）
 *   ⇒ 变了就**重新弹一次**面板并刷新数字；静置 `UFO_PANEL_LIFETIME` 帧后发**中断 1 缩回**。*/
/* ==================== ★★ #12 击破 UFO 的**爆炸特效**（owner 2026-09-23）=========================
 * owner 原话：「击破 UFO 的时候是**类似于击破 boss 有一个爆炸特效**的」。
 * 一手 = TH12 `Ufo::break`（`0x44af00` 一带）建的那两支：`bullet.anm` **script208**（外圈，`drawPoly(1,32)`
 *   `scaleTime(40,4,192,192)`、`color(0,255,192)` + `color2(255,255,255)`、`layer(12)`、`blendMode(1)`）
 *   与它的 `scriptNew` 子 **script209**（内圈，同形、`scaleTime(40,4,320,320)`、`color2(255,255,0)`、
 *   `alpha(128)` / `alpha2(64)`）⇒ 逐条移植成 `ability.anm` 的 **322 / 323**（那边一个改动都没有）。
 * `drawPoly` = 程序画的多边形 ⇒ **不需要任何贴图 / 图集**（与召唤紫环/六边形同一族做法）。
 * 生命周期：脚本自己在 +40 帧 `delete()` ⇒ 卡侧只记一个句柄（供"消失时不再重复用"与诊断），
 *   `on_delete`/换关时 `ufo_break_flash_kill()` 一并收。 */
static uint32_t s_break_flash_id;

static void ufo_break_flash_kill(void)
{
    if (s_break_flash_id) { ce_anm_delete(s_break_flash_id); s_break_flash_id = 0; }
}

static void ufo_break_flash_spawn(float wx, float wy)
{
    void *anm = CE_ABILITY_ANM();
    if (!anm) return;
    ufo_break_flash_kill();                            /* 上一发还没消失 ⇒ 先收（不叠）*/
    s_break_flash_id = ce_anm_spawn(anm, CE_ANM_ABILITY_SCRIPT_UFO_BREAK_FLASH, UFO_BREAK_FLASH_LAYER);
    if (!s_break_flash_id) {
        ce_log("ufo: break flash VM spawn FAILED (ability.anm script %d, layer %d) - the explosion is "
               "missing this break (assets/ability/scripts/322_,323_*)",
               CE_ANM_ABILITY_SCRIPT_UFO_BREAK_FLASH, UFO_BREAK_FLASH_LAYER);
        return;
    }
    ce_anm_set_pos(s_break_flash_id, wx, wy, 0.0f);
    ce_log("ufo: break explosion up at (%.0f, %.0f) - ability.anm script %d (+inner %d), one-hand "
           "TH12 bullet.anm script208/209 (drawPoly(1,32), scaleTime(40,4,192 / 320), layer %d, "
           "no texture needed)",
           (double)wx, (double)wy, CE_ANM_ABILITY_SCRIPT_UFO_BREAK_FLASH,
           CE_ANM_ABILITY_SCRIPT_UFO_BREAK_FLASH_INNER, UFO_BREAK_FLASH_LAYER);
}

#if !UFO_PANEL_ENABLED
/* owner 2026-09-23：右侧收点面板与它上面的数字**整套关掉** ⇒ 这里只留两个空壳（不产生未用警告）。*/
static void ufo_panel_kill(void) { }
static void ufo_panel_tick(ufo_card_t *s) { (void)s; }
#else
/* ★★ 2026-09-23（owner 实跑第 1 + 3 条）：「**去掉右侧的收点 HUD，不需要了**」+
 *   「**然后吸收了多少点数的那个 ascii 也不要了**」⇒ 面板（脚本 304/305）与它上面那排数字**整套关掉**。
 *   做法 = 一个开关（**代码保留**：资产、脚本、字形通道都在，将来要开只需把这里改回 1）：
 *   关掉之后 `ufo_panel_tick()` 立刻返回 ⇒ 面板 VM 与数字 VM 都不会被创建。
 *   ⚠ 面板的资产（`ability.anm` 304/305）留在库里 —— 「移植了但没接」那条审计指的是**卡侧一个引用都没有**，
 *     这里是"接过了、按 owner 口径关掉"，注释里写明，不算那类问题。*/
#define UFO_PANEL_ENABLED  0               /* ★ owner 2026-09-23：0 = 不弹（一手确实有这件，但 owner 不要）*/
#define UFO_PANEL_GLYPHS   3               /* ❓自定：最多三位（99+ 只画后三位）*/
#define UFO_PANEL_LIFETIME 180u            /* ❓自定：静置 3 秒后缩回（一手只做了「中断 1 = 缩回」）*/
#define UFO_PANEL_DX       40.0f           /* ❓自定：数字在面板内的起点（一手 `pos(528,288)` 是面板本身）*/
#define UFO_PANEL_DY       44.0f
#define UFO_PANEL_ADV      22.0f           /* ❓自定：字距（同 #20）*/
#define UFO_PANEL_SCALE    2.0f
#define UFO_PANEL_COOLDOWN 20u             /* ★ 自审补：吸收数每变一次就整块重弹会抖 ⇒ 冷却期内只**原地刷数字**，
                                            *   不重建面板（快速吸收时尤其明显）。20 帧 ≈ 0.33 s。*/            /* ❓自定：UI 层 1 texel = 1 表面 px ⇒ ×2（先例：人妖槽四件）*/

static uint32_t s_panel_id;                /* 面板父 VM（304）*/
static uint32_t s_panel_glyph[UFO_PANEL_GLYPHS];
static uint16_t s_panel_left;              /* 静置倒计时（0 = 已经缩回）*/
static uint16_t s_panel_cool;              /* ★ 自审补：重弹冷却（>0 时只原地刷新数字）*/

static void ufo_panel_kill(void)
{
    int i;
    if (s_panel_id) { ce_anm_delete(s_panel_id); s_panel_id = 0; }
    for (i = 0; i < UFO_PANEL_GLYPHS; ++i)
        if (s_panel_glyph[i]) { ce_anm_delete(s_panel_glyph[i]); s_panel_glyph[i] = 0; }
    s_panel_left = 0;
}

/* 原地刷新数字（冷却期用）：只重建几位字形 VM，**不碰面板本体**。 */
static void ufo_panel_digits(int32_t count)
{
    void *anm = CE_ABILITY_ANM();
    const float px = 528.0f + UFO_PANEL_DX;
    const float py = 288.0f + UFO_PANEL_DY;
    float p[3];
    int i, n = 0, v;
    if (!anm) return;
    if (count < 0) count = 0;
    v = count % 1000;
    for (i = 0; i < UFO_PANEL_GLYPHS; ++i)
        if (s_panel_glyph[i]) { ce_anm_delete(s_panel_glyph[i]); s_panel_glyph[i] = 0; }
    do { ++n; v /= 10; } while (v > 0 && n < UFO_PANEL_GLYPHS);
    v = count % 1000;
    for (i = 0; i < n; ++i) {
        int div = 1, k;
        for (k = 0; k < (n - 1 - i); ++k) div *= 10;
        p[0] = px + (float)i * UFO_PANEL_ADV;
        p[1] = py; p[2] = 0.0f;
        s_panel_glyph[i] = ce_anm_spawn_ui(anm, CE_ANM_ABILITY_SCRIPT_HUD_GLYPH, p);
        if (s_panel_glyph[i]) {
            ce_anm_set_sprite(anm, s_panel_glyph[i],
                              CE_ANM_ABILITY_SPRITE_ATLAS_HUD_DIGIT_0 + (v / div) % 10);
            ce_anm_set_scale(s_panel_glyph[i], UFO_PANEL_SCALE, UFO_PANEL_SCALE);
            ce_anm_set_color(s_panel_glyph[i], 0xffffffffu);
        }
    }
    s_panel_left = (uint16_t)UFO_PANEL_LIFETIME;
}

/* 弹一次面板 + 把 `count` 画出来（数字从右往左取位）。 */
static void ufo_panel_pop(int32_t count)
{
    void *anm = CE_ABILITY_ANM();
    const float px = 528.0f + UFO_PANEL_DX;     /* 一手的面板位 = (528,288)（脚本里写着）*/
    const float py = 288.0f + UFO_PANEL_DY;
    float p[3];
    int i, n = 0, v;
    if (!anm) return;
    if (count < 0) count = 0;
    v = count % 1000;                            /* ❓自定：只画后三位 */
    ufo_panel_kill();                            /* 重弹：先收上一套（不会叠）*/
    p[0] = 0.0f; p[1] = 0.0f; p[2] = 0.0f;
    s_panel_id = ce_anm_spawn_ui(anm, CE_ANM_ABILITY_SCRIPT_UFO_HUD_PANEL, p);
    s_panel_cool = UFO_PANEL_COOLDOWN;
    do { ++n; v /= 10; } while (v > 0 && n < UFO_PANEL_GLYPHS);
    v = count % 1000;
    for (i = 0; i < n; ++i) {
        int div = 1, k;
        for (k = 0; k < (n - 1 - i); ++k) div *= 10;
        p[0] = px + (float)i * UFO_PANEL_ADV;
        p[1] = py;
        s_panel_glyph[i] = ce_anm_spawn_ui(anm, CE_ANM_ABILITY_SCRIPT_HUD_GLYPH, p);
        if (s_panel_glyph[i]) {
            ce_anm_set_sprite(anm, s_panel_glyph[i],
                              CE_ANM_ABILITY_SPRITE_ATLAS_HUD_DIGIT_0 + (v / div) % 10);
            ce_anm_set_scale(s_panel_glyph[i], UFO_PANEL_SCALE, UFO_PANEL_SCALE);
            ce_anm_set_color(s_panel_glyph[i], 0xffffffffu);
        }
    }
    s_panel_left = (uint16_t)UFO_PANEL_LIFETIME;
    ce_log("ufo: absorb-count panel popped (ability.anm script %d <= TH12 front.anm script134/135; count = %d, "
           "%d digit VM(s) on script %d) - one-hand panel pos (528,288), retracts via interrupt 1 after %u frames",
           CE_ANM_ABILITY_SCRIPT_UFO_HUD_PANEL, (int)count, n, CE_ANM_ABILITY_SCRIPT_HUD_GLYPH,
           (unsigned)UFO_PANEL_LIFETIME);
}

/* 每帧：看吸收数变没变（变了就重弹）+ 静置超时缩回。 */
static void ufo_panel_tick(ufo_card_t *s)
{
    int32_t now;
    if (!s) return;
    if (s_panel_cool) --s_panel_cool;
    now = s->u.cnt_p + s->u.cnt_money;
    if (now != s->panel_last) {
        s->panel_last = now;
        /* ★ 自审补（2026-09-23）：冷却期内**只刷数字**（面板与它的出现动画不重播）——
         *   否则快速吸收时每帧都重建 VM：画面抖 + VM churn。 */
        if (now > 0) {
            if (s_panel_cool && s_panel_id && ce_anm_get_vm(s_panel_id)) ufo_panel_digits(now);
            else ufo_panel_pop(now);
        }
        return;
    }
    if (!s_panel_id) return;
    if (!ce_anm_get_vm(s_panel_id)) { s_panel_id = 0; return; }   /* 引擎删了 ⇒ 清句柄 */
    if (s_panel_left && --s_panel_left == 0) {
        int i;
        ce_anm_interrupt(s_panel_id, 1);                          /* 一手：中断 1 = 缩回（子 135 在 +20 帧自删）*/
        for (i = 0; i < UFO_PANEL_GLYPHS; ++i)
            if (s_panel_glyph[i]) { ce_anm_delete(s_panel_glyph[i]); s_panel_glyph[i] = 0; }
    }
}

#endif  /* UFO_PANEL_ENABLED */

/* 击破那一刻的**倍率**（一手 `zUfo+0x60`）：赏金包按它算分、×N 弹字显示它
 * ⇒ 同一个数、同一处算（原来 `ufo_drop_spawn_bag` 里算一遍、×N 那边再用收包那一刻的 `c->mult_permille`）。 */
static int32_t ufo_break_mult_permille(const ufo_card_t *s)
{
    return ufo_drop_mult_permille(s->u.color, CE_CURRENT_POWER() >= CE_MAX_POWER_VAL(),
                                  s->u.fill_millionths);
}

/* ★★★ 2026-09-24（owner 指出原版 ×N 下面还有一行"得分"）—— **这一击的得分**：
 *   一手把它算在**击破那一刻**（`th12:0x44af00` 里按色四条分支写 `zUfo+0x68`，公式 =
 *   我们已经在用的 `ufo_drop_settle_value()`：红 `cntA×PV×mult + cntB×PV`、
 *   蓝/绿/彩虹 `trunc(PV×cntB×mult)` 再 `/10×10`）⇒ 与「赏金包」收集时给的分**同一条公式**。
 *   ⚠ 收集那一刻的 PV 可能变了（本作 PV 由金钱档推出来）⇒ 弹字显示的是**击破时那一刻**的值
 *     （一手也是击破时算的 ✓），与包里最终给的分可能有极小差异 —— 记在 `TUNING.md`。*/
static int32_t ufo_break_popup_score(const ufo_card_t *s)
{
    int32_t pv = ufo_drop_pv_from_base(CE_MONEY_BASE_VAL());
    return ufo_drop_settle_value(s->u.color, s->u.cnt_p, s->u.cnt_money,
                                 ufo_break_mult_permille(s), pv);
}

/* 击破时掉的**压缩包**（用户口径：有实体、被自动吸收、收集时结算）。 */
static void ufo_drop_spawn_bag(ufo_card_t *s, float x, float y)
{
    int32_t mult = ufo_break_mult_permille(s);
    (void)ufo_drop_spawn(s, UFO_DROP_KIND_BAG, x, y, s->u.color,
                         s->u.cnt_p, s->u.cnt_money, mult);
}

/* ★★ P6：绿 UFO 喂饱 ⇒ **整颗 Bomb**。
 * ★★ 2026-09-23（owner 第 5 条 + 新口径）：**改成生成 TH18 原生道具 `item type 7`**
 *   （一手 `th18:0x4464d5` = 加弹 / 清碎片 / SE 0x2e / 刷 HUD；原版自己就用它掉整颗 B：`th18:0x40d2e0`）
 *   ⇒ 上线收点 / 全屏收点 / TH15 变黄变慢抖动 / 暂停 / 池回收全部由引擎负责（逐条地址见 `ufo_drop_spawn`）。 */
static void ufo_drop_spawn_bomb(ufo_card_t *s, float x, float y)
{
    (void)ufo_drop_spawn(s, UFO_DROP_KIND_BOMB, x, y, s->u.color, 0, 0, 1000);
}

/* 收集结算（一手三条式子，纯逻辑在 `ufo_drop_settle_value()`）：
 *   · **压缩包**：给分（一手式子）+ **把被吸走的信仰点还回去**（`faith_spawn()`，用户 2026-09-18）
 *     + 一手那两个音效（结算 `0x27`、收尾 `0x26` —— TH18 侧用现成 id 代替，见 `UFO_SE_*`）；
 *   · **整颗 Bomb**：`ce_add_bomb()`（引擎自己的加法：钳上限 / 清碎片 / 音效 / 刷 HUD —— 与
 *     一手「吃到整颗 Bomb」同一条路；TH13 的符卡碎也是走它）。 */
static void ufo_drop_collect(ufo_card_t *s, int i)
{
    ufo_drop_slot_t *d = &s_drops[i];
    ufo_drop_t *c = &d->core;

    if (!d->anm_id || !c->alive) { ufo_drop_kill(i); return; }
    if (c->kind == UFO_DROP_KIND_BOMB) {
        /* ★★ 2026-09-23：**这一支已经不会走到了** —— 整颗 B 现在是原生道具，收下由引擎的
         *   type 7 handler（`0x4464d5`）结算（加弹 / 清碎片 / SE 0x2e / 刷 HUD）。
         *   保留它只为"万一还有自建那一支"（`d->item == 0`）时能自洽收尾。*/
        ce_add_bomb();
        /* ★★ 2026-09-23 一手订正：整颗 Bomb = 一手道具 7，收下那一刻是**两声** ——
         *   `0x422dd0`（`collect_bomb`，体内 `0x422de9 mov edx,0x30` = `se_bonus`）+
         *   共同尾 `0x426da4`（`0x26` = `se_timeout2`）。`ce_add_bomb()` 自带引擎那一套音效，
         *   所以我们只补共同尾那一声（🟡 不再叠 `se_bonus`，免得同一个动作响三声 —— 记在 TUNING）。*/
        ufo_se(UFO_SEV_BOMB_TAKE, UFO_SE_TOKEN, c->x,
               "一手 th12:0x426da4 共同尾（道具 7 的收下；前半声 se_bonus 由 ce_add_bomb 自带）");
        ce_log("ufo: WHOLE BOMB picked up (one-hand item 7; th18 has no such type) - ce_add_bomb()");
    } else {
        int32_t pv = ufo_drop_pv_from_base(CE_MONEY_BASE_VAL());
        int32_t gain = ufo_drop_settle_value(c->ufo_color, c->cnt_p, c->cnt_money,
                                             c->mult_permille, pv);
        /* ★★ 2026-09-23 一手订正（原来只播一个 ❓自定 0x0d）：一手是**两声** ——
         *   `0x426c76 mov eax,0x27` + `0x426c7e call 0x453e20`（`se_invalid`，声像 = 道具 x）
         *   ⇒ 先响这一声；随后子分派算分、落回共同尾 `0x426da4` 再响一次 `0x26`。*/
        ufo_se(UFO_SEV_PACKAGE, UFO_SE_PACKAGE_OPEN, c->x,
               "一手 th12:0x426c76 mov eax,0x27 + 0x426c7e call 0x453e20（赏金包收下第一声）");
        ufo_se(UFO_SEV_PACKAGE, UFO_SE_TOKEN, c->x,
               "一手 th12:0x426da4 共同尾（赏金包收下第二声）");
        ce_add_score_delta(gain);
        ce_log("ufo: PACKAGE opened - color %d, cntA %d + cntB %d, mult %d.%03d, PV %d -> score %d "
               "(one-hand formulas; th12:0x426c9a/0x426cd2/0x426d27)",
               (int)c->ufo_color, (int)c->cnt_p, (int)c->cnt_money,
               (int)c->mult_permille / 1000, (int)c->mult_permille % 1000, (int)pv, (int)gain);
        /* ★ 信仰点原样还回去（用户：「被吸走后爆出来还得按照信仰-樱点算 … 后面只调用收信仰樱点的函数即可吧」）
         *   —— 这里调的是**生产者自己的 spawn 入口**（`faith_spawn`），不碰它的记账：还回去的点
         *   会像正常撒出来的点一样被自机收走/过线/进批（= 用户要的"按信仰算"）。 */
        if (c->faith_big || c->faith_mid) {
            uint32_t n;
            int32_t mask = c->faith_mask ? c->faith_mask : (~0);
            /* 值 = 被吸走那批的**均值**（保住总分；同一批点的 value 在 110/111 内部按难度/档位
             * 各不相同，按均值还原是"总量不变"的最小失真读法 —— ❓ 自定，见 TUNING §2.0o）。 */
            int32_t vb = c->faith_big ? (s->faith_pulled_big_sum / (int32_t)c->faith_big) : 0;
            int32_t vm = c->faith_mid ? (s->faith_pulled_mid_sum / (int32_t)c->faith_mid) : 0;
            if (c->faith_big && vb <= 0) vb = 1;
            if (c->faith_mid && vm <= 0) vm = 1;
            for (n = 0; n < c->faith_big; ++n)
                (void)faith_spawn(c->x, c->y, FAITH_TIER_BIG, mask, vb);
            for (n = 0; n < c->faith_mid; ++n)
                (void)faith_spawn(c->x, c->y, FAITH_TIER_SMALL, mask, vm);
            ce_log("ufo: PACKAGE returned the pulled faith dots: %u big (value %d each) + %u mid "
                   "(value %d each), mask 0x%x - via the producer's own faith_spawn()",
                   c->faith_big, (int)vb, c->faith_mid, (int)vm, (unsigned)mask);
        }
        s->faith_pulled_big = 0;                  /* 账已经结清 ⇒ 清掉（下一只 UFO 从零开始）*/
        s->faith_pulled_mid = 0;
        s->faith_pulled_mask = 0;
    }
    ufo_drop_kill(i);
}

/* 每游玩帧：走一步 + 判定收集（与令牌同一套半径）。 */
static void ufo_drops_tick(ufo_card_t *s)
{
    float px, py, r;
    int i;

    ufo_player_xy(&px, &py);
    r = ufo_tok_pick_radius();                    /* P5：本作自己的收点半径（与令牌共用）*/
    for (i = 0; i < UFO_DROP_POOL; ++i) {
        ufo_drop_slot_t *d = &s_drops[i];
        if (!d->core.alive) continue;
        if (d->item) {
            /* ★★ 2026-09-23：**整颗 B = 原生道具** ⇒ 运动、收集、暂停、抖动全归引擎的道具池
             *   （`ItemManager__on_tick` `th18:0x446ec0` → body `0x445a80`）。
             *   我们只做两件事：① 池格被回收（state 0）⇒ 收掉自己的卡面 VM；② 卡面 VM 句柄自愈。
             *   ⚠ **不写** `d->core.x/y`（那是压缩包自建那一支的坐标）—— 卡面 VM 的位置由
             *     `ufo_drops_draw_follow()` 在**画之前**从 `item+0xc2c` 直接读，免得晚一帧。*/
            uint8_t *it = d->item;
            if (*(int32_t *)(it + UFO_ITEM_STATE) == UFO_ITEM_STATE_OFF) {
                ce_log("ufo: WHOLE BOMB native item left the pool (state 0) - collected or despawned; "
                       "card-face VM %u taken down", d->anm_id);
                ufo_drop_kill(i);
                continue;
            }
            if (!d->anm_id || !ce_anm_get_vm(d->anm_id)) {
                uint32_t id = ce_anm_spawn(CE_ABILITY_ANM(), CE_ANM_ABILITY_SCRIPT_UFO_BOMB_ITEM,
                                           UFO_DROP_BOMB_ANM_LAYER);
                if (id) d->anm_id = id;
            }
#if UFO_ITEM_HIDE_ICON
            /* ★ 藏掉**引擎画的那颗原生整 B 图标**（ZUN 的遗留星星美术）：每帧把**图标 VM** 的
             *   `color_1`（`vm+0x524`）alpha 写 0 —— VM id 在 `item+0xc28`（一手 `th18:0x445d0a/0x445d12`
             *   逐指令，见 `UFO_ITEM_HIDE_ICON` 上面的注）。每帧写一次（幂等）：引擎只在出生/重建那帧
             *   写白，压暗那支只碰 G/R 字节 ⇒ 我们写的 alpha 不会被覆盖。 */
            {
                uint32_t ivm = *(const uint32_t *)(it + UFO_ITEM_ANM_ID);
                uint8_t *vm = ivm ? ce_anm_get_vm(ivm) : 0;
                if (vm) *(uint32_t *)(vm + CE_ANM_VM_COLOR1) = 0x00ffffffu;
            }
#endif
            continue;
        }
        /* 句柄自愈（引擎在别处销毁过 ⇒ 丢了就重建；照令牌那一支同款口径）*/
        if (!d->anm_id || !ce_anm_get_vm(d->anm_id)) {
            int script = ufo_drop_bag_script(d->core.cnt_p, d->core.cnt_money);
            uint32_t id = ce_anm_spawn(CE_ABILITY_ANM(), script, UFO_DROP_ANM_LAYER);
            if (!id) { ufo_drop_kill(i); continue; }
            d->anm_id = id;
        }
        (void)ufo_drop_step(&d->core, px, py);
        ce_anm_set_pos(d->anm_id, d->core.x, d->core.y, 0.0f);
        if (ufo_drop_within_pick(&d->core, px, py, r)) ufo_drop_collect(s, i);
    }
}

/* ★★ 2026-09-23：把「卡面 VM」摆到**原生道具**身上 —— 在 `on_draw` 里做（画之前），
 *   所以不晚一帧（`on_item_frame` 在道具 body **之前**，那时坐标还是上一帧的）。
 * 一手：`ItemManager__on_draw__body` `th18:0x446d60` 自己也是"把道具的世界坐标写进两个 VM 的
 *   `entity_pos`"（`VM+0x5f0`）再 `call 0x481210` ⇒ 我们照同一条路走，只是 VM 换成自己的卡面。
 * ⚠ 道具已被收走 ⇒ 顺手把槽收干净（`ufo_drops_tick` 那条也会收，这里兜一拍）。*/
static uint32_t s_drop_log_tick;      /* 满 B 诊断行的节拍（本函数没有卡状态参数）*/
static void ufo_drops_draw_follow(void)
{
    int i;
    for (i = 0; i < UFO_DROP_POOL; ++i) {
        ufo_drop_slot_t *d = &s_drops[i];
        uint8_t *it;
        if (!d->core.alive || !d->item) continue;
        it = d->item;
        if (*(int32_t *)(it + UFO_ITEM_STATE) == UFO_ITEM_STATE_OFF) { ufo_drop_kill(i); continue; }
        if (!d->anm_id || !ce_anm_get_vm(d->anm_id)) { d->anm_id = 0; continue; }
        ce_anm_set_pos(d->anm_id, *(const float *)(it + UFO_ITEM_POS),
                                   *(const float *)(it + UFO_ITEM_POS + 4), 0.0f);
#if UFO_DROP_ANM_LOG_EVERY
        /* ★ 2026-09-24 第 21 轮诊断（owner：「**没有左右摇摆**」）：每 60 帧把**父 VM**的
         *   `rotation`（`+0x38/3c/40`）与 `scale`（`+0x54/58`）打一行 ——
         *   摇摆是脚本 301 的 `rotateTime(120,21,…)` 在跑，日志里看得见它在动就说明是**幅度太小**
         *   （一手 -2°↔+4° 只有 6°），而不是"动画没跑"。*/
        if ((++s_drop_log_tick % UFO_DROP_ANM_LOG_EVERY) == 0u) {
            float *rv = (float *)(ce_anm_get_vm(d->anm_id) + CE_ANM_VM_ROTATION);
            float *sv = (float *)(ce_anm_get_vm(d->anm_id) + CE_ANM_VM_SCALE);
            uint32_t ivm = *(const uint32_t *)(it + UFO_ITEM_ANM_ID);
            uint8_t *ivm_p = ivm ? ce_anm_get_vm(ivm) : 0;
            ce_log("ufo/drop.bomb: parent vm %u rot (%.4f, %.4f, %.4f) scale (%.3f, %.3f) | native icon vm %u "
                   "color 0x%08x (alpha must be 0 = hidden star)",
                   d->anm_id, (double)rv[0], (double)rv[1], (double)rv[2],
                   (double)sv[0], (double)sv[1], ivm,
                   ivm_p ? *(const uint32_t *)(ivm_p + CE_ANM_VM_COLOR1) : 0u);
        }
#endif
    }
}

/* ==================== ★★ P1：满槽补偿（槽满期间收到的令牌）====================
 * 一手（`th12:0x4270b0` 开头，逐条）：
 *   判据 `[0x4b0c58] >= 3`（已填槽数）⇒ `[0x4b0c78] += 100000`（夹到 `[0x4b0cf4]`）+ 飘字 **1000**
 *   （色 `0xff80ff80`）；令牌**被消耗**（走 `0x426da4` 那条共同尾：state=0 + 音效 0x26 + interrupt 1）。
 *
 * ⚠ **载体问题（必须说清，别包装成复刻）**：一手加的是 `[0x4b0c78]` = **PIV（点道具的价值基数，×100 定点）**，
 *   它是一个**跨帧的状态量**；而 **TH18 没有这个量** —— TH18 的点道具身价是**由金钱档推出来的**
 *   （`(MONEY+1) × K × 100`，`engine.h` 的 `CE_ADDR_MONEY_BASE`；K = EASY 500 / 其余 1000）。
 *   ⇒ 三种映射，本文件用宏 `UFO_SLOT_FULL_BONUS_MODE` 选（**默认 1 = 给分**，理由见下）：
 *     · `0` = **只记账 + 弹字**（不碰任何引擎状态；最保守，但玩家实际拿不到东西）
 *     · `1` = **给分 `UFO_SLOT_FULL_POPUP_VALUE`（1000）** ← **默认**：与一手**弹字的值逐值相同**，
 *           落点是 SDK 的未耦合加数（`ce_add_score_delta`），**不影响别的卡/经济/商店**；
 *     · `2` = **金钱 +1**（`CE_ADDR_MONEY` / `CE_ADDR_MONEY_TOTAL`）← **默认（2026-09-18 改）**：
 *           TH18 的**点道具价值基数就是金钱** —— 引擎每颗道具按 `身价 = MONEY × K` 现算
 *           （`th18:0x446b86 movd xmm1,[0x4ccd34]` → `mulss K` → `0x446b5c mov [0x4ccd24],ecx`，
 *           K = EASY `500.0f`（`0x4b93ac`）/ 其余 `1000.0f`（`0x4b93d4`），再夹到 `0x4ccd28/2c`）
 *           ⇒ **+1 金钱 = 之后每颗点道具多值 K（500~1000 显示点）**，
 *           与一手「PIV +1000 ⇒ 之后每颗点道具多值 1000」**同构同量级**。
 *           ★ 为什么不是 `CE_ADDR_MONEY_ITEMS`（`0x4ccd20`）：那只是"吃到的金钱道具个数"的**统计**
 *           （`0x446d22 inc`），**不参与身价公式**（身价读的是 `0x4ccd34`）。
 *   ⇒ 这是**❓ 自定（载体映射）**，不是复刻；登记在 `TUNING.md` §2.0l / §2.0q。
 *   ★★ 一手量级复核（2026-09-18 追问后重核，见 §2.0q）：常规的 PIV 增长是 **+100（显示 +1）/次**
 *     （`th12:0x4391ed add eax,0x64`）；令牌这条是 **+100000（显示 +1000）** ⇒ 一次 ≈ **1000 次常规增长**，
 *     而且 PIV 是**跨帧状态量**（与分数/火力/擦槽计数一起进 `th12:0x43b0xx` 的状态快照），
 *     **不是一次性加分** ⇒ 所以"一次性给 1000 分"（模式 1）是弱化的近似，模式 2 才是同构的那一档。 */
/* ★★ 2026-09-18（owner 拍板）：**2 → 0** —— 池那一笔 `ce_piv_add(UFO_ID_NUM, 1000)` **就是**原生的
 *   「飞碟满 3 ⇒ 最大得点 +1000 显示」（一手 `th12:0x4270d1` `add eax,0x186a0` / `th12:0x4270d8`，
 *   PIV 存储 = 显示 ×100；口径见 `sdk_core.h` 的「共享 PIV 池 + 擦弹族」块）。
 *   而模式 1/2（一次性给分数 / 给金钱）是**同一件事的弱化近似**（上面那段 ❓ 自定 载体映射）
 *   ⇒ 两个都开 = **同一机制双计**。user 2026-09-18：「改成 0」。
 *   1 / 2 的实现留在下面原处（想退回只改这一行），登记在 `AUDIT.md` §BE 详录 #48。*/
#define UFO_SLOT_FULL_BONUS_MODE 0
/* ★ 2026-09-18（共享 PIV 池落地后补记）：上面那三种映射是**池还不存在时**的载体方案。
 *   现在本事件的**权威载体 = 共享 PIV 池那一笔**（`ufo_slot_full_bonus()` 里的
 *   `ce_piv_add(UFO_ID_NUM, 1000)`，一手 `th12:0x4270d8` 逐值）。
 *   ⚠ 于是模式 `1`/`2` 与池那一笔**表达的是同一个 +1000**（一个走分数/金钱、一个走额外最大得点）
 *   ⇒ 同时开着是**两份**。本宏的取值本来就是待裁决项（`NEXT.md`「仍待：`UFO_SLOT_FULL_BONUS_MODE`
 *   的取值」/ `TUNING.md` §2.0l、§2.0q）⇒ **本处没有擅自改它**，留给 owner 拍；
 *   要「只留池那一份」就把宏改成 `0`（只记账 + 弹字）。 */

/* 引擎弹字环的**数字槽**写入（形状逐条照抄原生 adder `th18:0x4645f0` 的字段写序，
 * 与 110 `mof.c` 的 `mof_popup_number()` 同款 —— 那一支也是"照抄不抽共享件"的写法；
 * 抽成 `ce_popup_number()` 是 owner 的决策，本轮不动共享件）。 */
#define UFO_POPUP_COUNTER   0x10
#define UFO_POPUP_SLOTS     10
#define UFO_POPUP_DIGITS    8
#define UFO_POPUP_GLYPH_MINUS 0x0a

static int ufo_popup_number(const float *pos, int32_t value, uint32_t color)
{
    uint8_t *mgr = CE_POPUP_MGR();
    int32_t idx, v;
    uint8_t *sl;
    int n = 0;

    if (!mgr || !pos) return 0;
    idx = *(int32_t *)(mgr + UFO_POPUP_COUNTER);
    if (idx < 0 || idx >= UFO_POPUP_SLOTS) idx = 0;
    sl = mgr + CE_POPUP_RING + (size_t)idx * CE_POPUP_STRIDE;
    v = value;
    if (v < 0) {
        *(uint8_t *)sl = (uint8_t)UFO_POPUP_GLYPH_MINUS;
        n = 1;
    } else if (v == 0) {
        *(uint8_t *)sl = 0;
        n = 1;
    } else {
        while (v != 0 && n < UFO_POPUP_DIGITS) { *((uint8_t *)sl + n) = (uint8_t)(v % 10); v /= 10; ++n; }
    }
    *(uint8_t *)(sl + 0x3d) = (uint8_t)n;
    *(uint32_t *)(sl + CE_POPUP_COLOR) = color;
    if (!(*(uint32_t *)(sl + 0x30) & 1u)) *(uint32_t *)(sl + 0x30) |= 1u;
    *(float *)(sl + 0x18) = 1.0f;
    *(int32_t *)(sl + 0x24) = 0;
    *(int32_t *)(sl + 0x20) = -1;
    *(int32_t *)(sl + 0x28) = 0;
    ((float *)(sl + CE_POPUP_POS))[0] = pos[0];
    ((float *)(sl + CE_POPUP_POS))[1] = pos[1];
    ((float *)(sl + CE_POPUP_POS))[2] = pos[2];
    *(uint32_t *)(sl + 0x3c) = 1;                      /* busy（原生 adder 最后写它 + inc 计数器）*/
    *(int32_t *)(mgr + UFO_POPUP_COUNTER) = (idx + 1) % UFO_POPUP_SLOTS;
    return 1;
}

/* 槽满期间收到一枚令牌：记账 + 弹字 +（按 `UFO_SLOT_FULL_BONUS_MODE`）落笔。 */
static void ufo_slot_full_bonus(ufo_card_t *s, float x, float y)
{
    float p[3];

    if (!s) return;
    ++s->slot_full_tokens;
    s->slot_full_piv += UFO_SLOT_FULL_PIV_ADD;
    p[0] = x; p[1] = y; p[2] = 0.0f;
    (void)ufo_popup_number(p, UFO_SLOT_FULL_POPUP_VALUE, UFO_SLOT_FULL_POPUP_COLOR);
    /* ★★ 2026-09-18（用户口径）：**TH12 路径往共享 PIV 池写** —— 这一处就是一手为本作记的
     *   那条**唯一**「非擦弹」涨源（擦弹 +1 显示/次 由共享的擦弹写入者负责，本卡只声明在场、
     *   不在这里重复实现 ⇒ 这里只接飞碟链这一支）。
     *
     *   为什么落点是**本函数**：一手的判据在 `th12:0x4270b0` 开头 —— `th12:0x4270b5`
     *   `mov ebp,ds:0x4b0c58`（飞碟收集计数）+ `th12:0x4270bb` `cmp ebp,3` ⇒
     *   **计数已满 3（三格槽满）时再收一枚飞碟道具**（types `0x0a`–`0x0c`，调用者
     *   `th12:0x426c3f` / `th12:0x426c4d` / `th12:0x426c5e`）才走加值支：
     *   `th12:0x4270d1 add eax,0x186a0` ⇒ `th12:0x4270d8 mov ds:0x4b0c78,eax`（+100000 **存储**）
     *   ⇒ `th12:0x4270df` 夹 `MAXIMUM_PIV` ⇒ `th12:0x4270f6` 飘窗 **1000**；「不足 3」那一支
     *   （`th12:0x427105`）只做登记、**不碰 PIV**。
     *   本卡与它同构的事件 = **槽里已经有一个成组的组合时再消耗一枚令牌**
     *   （`ufo_tokens_tick()` 第 ⑤ 步判 `ufo_present_group_ready()`，然后
     *   `ufo_tok_take_and_kill()` 消耗令牌 + `ufo_popup_number()` 弹 1000）⇒ 唯一落点就是本函数。
     *
     *   量纲（别写错）：TH12 的 PIV **存储 = 显示 ×100**（取值 `th12:0x421880` 逐指令 =
     *   `trunc10(PIV/100)`）⇒ 一手 `0x186a0` = **+1000 显示** ⇒ 这里写 **1000**
     *   （池的单位 = 显示「点」；`ce_piv_claim_auto()` 用的难度两表 `k_piv_init/k_piv_cap`
     *    也是显示单位 ⇒ 单位自洽）。
     *   一手出处：`engine/faith/th12/01-point-item-value.md` §9（涨源表 + 飞碟奖励高度项的否定结论）、
     *   §4.2（本支的逐指令算式）；跨作口径：`engine/faith/OVERVIEW.md` 的跨作表与
     *   「结论：th12–th17 可以共用一个 PIV」。
     *
     *   「一次事件只加一次」：本函数**一次调用 = 一枚令牌被消耗**（调用点先把它从池里
     *   swap-remove 掉）⇒ 不存在「同一事件重入」，不需要额外的一次性闩；`s->slot_full_tokens`
     *   本来就是这件事的账本。⚠ 同一帧里**多枚**令牌各自被消耗 = 多次事件 ⇒ 各 +1000
     *   （与一手「逐枚收、逐枚 +1000」一致）。
     *
     *   闸（**本卡不夹**）：可写上限 = `cap_i − init_i`（本卡 th12 的难度两表）由**层**按难度表管 ——
     *   `ce_piv_claim_auto()` 在 `on_item_frame()` 开头每帧重申，`ce_piv_add()` 自己按该卡已写账截断
     *   （`sdk_core.h` 同名注释块 / `sdk_core.c:610`）；本卡只负责报出「一手这一笔的点数」。 */
    ce_piv_add(UFO_ID_NUM, 1000);
#if UFO_SLOT_FULL_BONUS_MODE == 1
    ce_add_score_delta(UFO_SLOT_FULL_POPUP_VALUE);      /* 弱化近似：一次性给弹字那个数 */
#elif UFO_SLOT_FULL_BONUS_MODE == 2
    /* ★★ 同构载体（见上面的长注）—— **用户 2026-09-18 确认口径：「所以是 +1000 最大得点！」**
     *   ⇒ 我们要的是**最大得点 +1000**（与难度无关；一手 PIV 那条也没有难度项）。
     *   TH18 的最大得点由金钱推：`身价 = (MONEY·k + k) × …`，k = **EASY 500 / 其余 1000**
     *   （`th18:0x446b30` 的 `500.0f`(`0x4b93ac`) / `0x446b86` 的 `1000.0f`(`0x4b93d4`)）
     *   ⇒ **+1 金钱 = +1000 最大得点（Normal/Hard/Lunatic）**，而 **EASY 只有 +500**
     *     ⇒ EASY 下补 **+2 金钱**，才逐值等于一手的 +1000。
     *   ⚠ Extra(4) 走的是另一支（`engine/card/th18/05-shop-and-money.md` §2 记 🟡 未 dump）
     *     ⇒ 本处按"非 EASY = 1000"处理，并把难度打进日志（实跑一眼可核）。 */
    {
        const int32_t diff = CE_DIFFICULTY_VAL();
        const int32_t step = (diff == 0) ? 2 : 1;     /* EASY 的 k=500 ⇒ 两步才够 1000 */
        int32_t n;
        for (n = 0; n < step; ++n) { CE_MONEY() += 1; CE_MONEY_TOTAL() += 1; }
        ce_log("ufo: slot-full bonus -> MAX POINT +1000 (via MONEY +%d; difficulty %d, k=%d)",
               (int)step, (int)diff, diff == 0 ? 500 : 1000);
    }
#endif
    ce_log("ufo: slots FULL (a UFO is on the field / a group is waiting) - token consumed for the "
           "one-hand bonus: PIV +%d (= +%d shown), popup %d (mode %d, %lld lifetime); %u tokens so far; "
           "shared PIV pool -> %d (this card wrote %d, its writable cap %d)",
           UFO_SLOT_FULL_PIV_ADD, UFO_SLOT_FULL_POPUP_VALUE / 100, UFO_SLOT_FULL_POPUP_VALUE,
           UFO_SLOT_FULL_BONUS_MODE, (long long)s->slot_full_piv, s->slot_full_tokens,
           ce_piv_pool(), ce_piv_written(UFO_ID_NUM), ce_piv_cap_of(UFO_ID_NUM));
}

/* ================================ §3.5 击破 / 飞离 / 共用 finish ================================ */
static void ufo_teardown(ufo_card_t *s, const char *why)
{
    if (!ufo_present(&s->u)) return;
    ce_log("ufo: finish (%s) - color %d, %d P + %d money absorbed (fill %d.%03d), frame %u; "
           "faith pulled: %u big + %u mid (mask 0x%x)",
           why ? why : "?", (int)s->u.color, s->u.cnt_p, s->u.cnt_money,
           s->u.fill_millionths / UFO_FILL_UNIT,
           (s->u.fill_millionths % UFO_FILL_UNIT) / 1000, s->play_frame,
           s->faith_pulled_big, s->faith_pulled_mid, (unsigned)s->faith_pulled_mask);
    /* 照 TH12 `Ufo::on_tick` 的结束分支（`UFO:104-107`）：清计数 / 停循环音 / 删光环 VM / 血条淡出。
     * ★★ 2026-09-18 订正这条注释（它曾是**过期**的）：吸收环与血量条**已经实装**（`ufo_gauge_*`，
     *   脚本 228/229/230/231/232，见 `AUDIT.md` §BO-12）——**但它们不在这里收**：
     *   收尾点在 `ufo_withdraw_all()` 的 `ufo_gauge_kill()`（击破 / 飞走 / 过场 / 删卡四条路都汇到那里）。
     * ⚠ 本支真正空着的是「停循环音」那半（`spec §9 #11`：TH18 侧没有停循环音的 API；本卡也**没起过**循环音）。
     * ★ **本体的跟随 VM**（`body_anm_id`）⇒ 这一支是它的收尾点（`ufo_hud_kill` 的同类）。 */
    ufo_body_vm_kill(s);
    /* ★★ P1（一手 `0x44a326`-`0x44a344`）：**槽在 UFO 结束这一刻才清** ——
     *   击破 / 飞走 / 撤场 / 删卡四条路都汇到本函数 ⇒ 这是唯一的清槽点。
     *   ⚠ 必须是"结束"而不是"召唤"：否则又回到旧口径（UFO 在场时槽是空的、还能继续收令牌）。 */
    if (s->grp.filled || s->grp.last_group != UFO_COLOR_NONE)
        ce_log("ufo: slots cleared (UFO ended: %s) - filled was %d, last group color %d",
               why ? why : "?", (int)s->grp.filled, (int)s->grp.last_group);
    ufo_group_clear(&s->grp);
    s->boss_leave_logged = 0;
    s->dialogue_break_logged = 0;
    ufo_finish(&s->u);                                 /* 纯逻辑侧：清 id / 计数 / fill */
}

/* 击破：开奖（赏金分 + 颜色加成道具 + **返还一枚令牌**）+ 收尾。 */
static void ufo_break(ufo_card_t *s)
{
    int64_t bounty = ufo_break_bounty_score(&s->u);
    int32_t bonus = ufo_break_color_bonus(s->u.color);
    /* ★★ 返还的令牌：一手 `th12:0x44af00` 的跳表（`0x44b590`，索引 = color+1）——
     *   红→道具 13、蓝→14、绿→15、彩虹→按 `COLLECTED_UFOS[2]`（`0x4b0c54`）。
     *   **每色恰好 1 枚**（用户口述里「好像是两个飞碟」= 不成立；另一件是加成道具 4/5）。
     *   ⚠ `last_slot2` 是 `ufo_group_take_ready()` 存下来的第 3 枚颜色（★ P1 起槽与一手一样
     *     留到 UFO 结束才清，但"召唤那一刻的第 3 枚"这个信息还是要快照一份）。 */
    int32_t tok_state = ufo_break_token_state(s->u.color, s->grp.last_slot2);
    float rp[3];
    int32_t color = s->u.color;

    ++s->ufos_broken;
    /* ★★ 2026-09-23（owner 实跑第 4 条）：「**消弹的爆炸特效怎么变成了在玩家处吃到压缩包释放，
     *   应该是 UFO 本身的爆炸**」⇒ 真因：`ufo_xn_spawn()`（×N 弹字）与 `ufo_break_flash_spawn()`
     *   （爆炸）上一轮被我挂在了**收压缩包**那一支（`ufo_drop_collect`）⇒ 它们跟着玩家的收包位置出。
     *   一手两件事都挂在**击破**（`0x44ab00` 建弹字 VM / `0x44af00` 那条路），位置 = **UFO 自己**。
     *   ⇒ 现在挪到这里（击破那一刻、UFO 位置 `s->last_x/y`）。
     *   ⚠ ×N 的倍率 = 一手 `zUfo+0x60`（击破时定下、赏金包收下时按它算分）⇒ 用 `s->break_mult_permille`
     *     这份快照（在下面算好），**不要**用收包那一刻的值。 */
    {
        int32_t mult = ufo_break_mult_permille(s);
        ufo_xn_spawn(s->last_x, s->last_y, mult, ufo_break_popup_score(s));
        ufo_break_flash_spawn(s->last_x, s->last_y);
    }
    ufo_se(UFO_SEV_BREAK, UFO_SE_BREAK, s->last_x,
                   "一手 th12:0x44af65 lea eax,[edi+6]（id 随色：se_power1/tan00/tan01/tan02）");
    /* ★★ 一手 `0x44af00` 的第一件事（顺序照抄）：在 UFO 位置生成 `UFO_EtBreak2`
     *   （扩圈消弹 16→640、每帧 +16、40 帧后自己 `delete()`）+ 播大消弹音（`se_power1.wav`）。
     *   ⚠ 与**召唤**那一圈（`UFO_EtBreak`，16→112、每帧 +6，写在 ECL 本体里）是**两条**，别混。 */
    ufo_etbreak2_spawn(s->last_x, s->last_y);
    ufo_se(UFO_SEV_ETBREAK2, UFO_SE_ETBREAK2, s->last_x,
                       "一手 同上（大扩圈消弹那一发；我们固定用 se_power1 那一档）");
    ce_log("ufo: break-time BIG cancel ring up (%s at (%.0f, %.0f), radius 16->640 +16/frame, 40 frames; "
           "se 0x%02x = se_power1.wav = the very wav one-hand uses)",
           UFO_ECL_ETBREAK2_NAME, (double)s->last_x, (double)s->last_y, (unsigned)UFO_SE_ETBREAK2);
    /* ★ 击破分数：一手公式**仓里没有**（spec §9 #5）⇒ `ufo_break_bounty_score` 现在恒 0
     *（占位权重 `UFO_BOUNTY_SCORE_PER_*` = 0 = 「不做」）。这里照实加，不为好看编数。
     * ⚠ 本轮已经**查到**一手的倍率与算式（见报告 §对账 8）：
     *   `0x44af00` 按色算 `+0x9d4`（红 1.0/2.0、蓝 8·fill+2→6→8、绿恒 2.0、彩虹 2·fill+2→3→4），
     *   赏金道具在收集时按 `cntA·PV·mult + cntB·PV`（红）或 `cntB·PV·mult`（蓝/绿/彩虹）给分
     *   （`0x426c9a`/`0x426cd2`/`0x426d27`）；但 TH18 侧**没有能承载它的道具 type**（16/17/18 全不可用，
     *   `UFO_BOUNTY_TYPES_USABLE == 0`）⇒ 分值与倍率仍不接线（要接就落在这两行之间）。 */
    if (bounty) ce_add_score_delta(bounty);
    if (bonus) {
        rp[0] = s->last_x; rp[1] = s->last_y; rp[2] = 0.0f;
        /* 一手击破掉落的参数：角度 −π/2、速度 2.2（`th12:0x44af00`；`UFO:256-259`）*/
        if (ce_spawn_item((uint32_t)bonus, rp, -1.5707964f, 2.2f)) {
            ++s->rewards_spit;
            ufo_se(UFO_SEV_DROP_SPAWN, UFO_SE_ITEM_BREAK, s->last_x,
                               "❓自定：一手 spawn_item 不播音（删它要 owner 点头）");
        } else {
            ce_log("ufo: break bonus type %d failed (item pool full?)", bonus);
        }
    }
    /* ★ 返还令牌（一手：每色 1 枚；出界/被收就没了 —— 与原生一致，风险收益的来源）
     * ⚠ 换池后失败原因只剩「池满 / anm VM 建不出来」（以前还有「ECL 名查不到」那条）。 */
    if (tok_state != UFO_TOK_DROP_NONE) {
        if (ufo_tok_spawn_ex(s, s->last_x, s->last_y,
                             ufo_token_display_color(UFO_COLOR_RED, 1, tok_state), tok_state, 1))
            ++s->dropped_by_ufo;
        else
            ce_log("ufo: break - the returned token could NOT be spawned (pool full / anm not loaded)");
    }
    /* ★★ 击破掉的**压缩包**（用户 2026-09-18：「击破还会掉落一个**被自动吸收的压缩包**（注意，
     *   **应该是有实体的**，只是被自动吸收过来）」）。
     * 一手 = 赏金道具 type 16/17/18（携带 `cntA/cntB/颜色/倍率`，玩家**吃到时**才结算）；
     * TH18 侧三个 type **全不可用**（`UFO_BOUNTY_TYPES_USABLE == 0`）⇒ 自建实体 + 我们自己的结算。 */
    if (ufo_bounty_item_type(&s->u)) {
        ufo_drop_spawn_bag(s, s->last_x, s->last_y);
        ++s->drops_spawned;
    }
    ce_log("ufo: BREAK color %d - %d P + %d money absorbed, bounty score %lld (placeholder weight 0), "
           "colour bonus item %d, returned token state %d; faith to return: %u big + %u mid",
           (int)color, s->u.cnt_p, s->u.cnt_money,
           (long long)bounty, (int)bonus, (int)tok_state,
           s->faith_pulled_big, s->faith_pulled_mid);
    ufo_teardown(s, "broken");
}

/* ==================== ★★ HUD 第三版：原版切图的**三格令牌槽**（用户 2026-09-18 实跑 ⑤）============
 * 用户原话：「**飞碟槽 HUD 你没按照原版做**」。第一版是 `drawRect` 拼的占位（自标 ❓ 自定），
 * 第二版只生成了原版资产（贴图 + 脚本 302..305）**却没接线** ⇒ 这一版把原版那两件真正画出来：
 *   ① **三格令牌槽** = 脚本 **302**（← TH12 `front.anm` **script80** 逐条移植）—— 三只 VM，
 *      槽位由**中断 7/8/9** 决定（原版把三个槽位做成 `posTime` 的目标点，x = 52/73/94），
 *      四态由**中断 10/11/12/13** 决定（空/红/蓝/绿，见下面 `ufo_hud_slot_state_of()`）。
 *   ② **入槽闪光** = 脚本 **303**（← `front.anm` **script81**）：新令牌落进第 i 格时起一只，
 *      发 `interrupt(7 + i)` 摆位 + `interrupt(10 + 四态)` 上色，31 帧后它自己 `delete()`。
 * 一手（三处互相印证，全文见 `assets/ability/make_ufo_hud_art.py` 文件头）：
 *   · `FUN_004214b0` `th12:0x4214b0`：每个槽 `pending_interrupt = COLLECTED_UFOS[i] + 10`；
 *     某一格刚收了令牌 ⇒ 对该格发 **17**（出现动画）+ 起一只脚本 0x51(81) 的闪光 VM
 *     （`lea edx,[ebx+7]` 摆位、`ax = [COLLECTED_UFOS[arg]] + 0xa` 上色）。
 *   · `FUN_00421690` `th12:0x421690`：**每帧**直接写 sprite 字段 = `COLLECTED_UFOS[i] + 59 + (闪烁 ? 4 : 0)`
 *     ⇒ 「UFO 在场时槽闪烁」的真身是**换到亮版那一组 sprite**（不是改 alpha）；闪烁相 = `timer % 16 == 1`。
 *   · 逐像素量原图：sprite59 = **纯黑框 = 空槽**，60/61/62 = 红/蓝/绿 ⇒ **原版没有彩虹色的槽**
 *     （与用户裁决「新增 2：没有彩虹色 token」吻合）。
 * ⚠ 层 25 = 与暂停模糊罩同一个绘制组（38）、罩子是暂停那一刻才生成的 ⇒ 我们的 HUD 会被罩住；
 *   **不许**换到 26+，也**不许**改用 ASCII 文字（`ce_hud_text*` 结构上不可能被罩住）。
 * ⏳ 还没接的（见文件尾清单）：出现横幅 79（sprite67，贴图已可切）、大横幅 82（front02.png 304×32）、
 *   吸收数记录面板 304/305（**TH18 的右列被原生卡组图标占着**，位置要另定）。 */

/* 起一只 HUD VM：脚本自己的坐标照抄一手（槽位/面板位都写在脚本里），实体坐标先传 0；
 * 摆位由 `ufo_hud_place_at()`（Δ7）每帧按"绘制点 = 三项之和"补差值 —— 见常量区的 Δ7 长注。 */
static uint32_t ufo_hud_vm_spawn(int script)
{
    static const float origin[3] = { 0.0f, 0.0f, 0.0f };
    return ce_anm_spawn_ui(CE_ABILITY_ANM(), script, origin);
}

/* ---- Δ7 的落点：把第 i 格的**绘制点**钉到槽心（表面 px）-----------------------------------------
 * 绘制点 = `pos(+0x30)` + `entity_pos(+0x5f0)` + `__pos_2(+0x4e4)`（一手 `th18:0x48171f`，见 `engine.h`）。
 * 前两项里 `pos`/`__pos_2` 是脚本/装载器的账（我们不动它们），差值全部由 **entity_pos** 吃掉：
 *     entity_pos = 槽心 − (pos + __pos_2)
 * ⇒ 无论脚本那两项是 296 还是 0 还是别的，画面上三格都落在 296/338/380 × 912。
 * ⚠ 必须**每帧**算（`pos` 若哪天被中断改了就自动跟随），只在数值变了时才写（省 D3D 无关的写）。
 * ⚠ 浮点全是 `-` 运算、没有整→浮转换（本文件的零 x87 硬门）。 */
static void ufo_hud_place_at(int i, uint32_t id)
{
    uint8_t *vm;
    const float *sp, *p2;
    float *ep, want_x, want_y;

    if (i < 0 || i >= UFO_SLOT_COUNT || !id) return;
    vm = ce_anm_get_vm(id);
    if (!vm) return;
    sp = (const float *)(vm + CE_ANM_VM_SCRIPT_POS);
    p2 = (const float *)(vm + CE_ANM_VM_POS_2);
    ep = (float *)(vm + CE_ANM_VM_POS);
    want_x = k_ufo_slot_x[i] - (sp[0] + p2[0]);
    want_y = UFO_HUD_SLOT_Y  - (sp[1] + p2[1]);
    if (ep[0] != want_x) ep[0] = want_x;
    if (ep[1] != want_y) ep[1] = want_y;
}

/* 槽里的颜色 → **四态**（0 空 / 1 红 / 2 蓝 / 3 绿）。
 * ★ 金显示成蓝那一档（用户裁决 ①：金 = 蓝的调色版，等 P 图）；彩虹令牌**不进槽**
 *   （用户裁决「新增 2」：没有彩虹色 token，彩虹 = 收齐三色那一刻直接开彩色 UFO）⇒ 落到空态。 */
static int32_t ufo_hud_slot_state_of(int32_t color)
{
    switch (color) {
    case UFO_COLOR_RED:   return 1;
    case UFO_COLOR_BLUE:  return 2;
    case UFO_COLOR_GOLD:  return 2;    /* ★ 与蓝同色（等 P 图）*/
    case UFO_COLOR_GREEN: return 3;
    default:              return 0;    /* 空 / 彩虹（不该出现，出现也当空）*/
    }
}

/* 收干净本卡的 HUD VM。**长持续效果铁律**：菜单门（`on_draw` 门关着时）、`on_run_reset`、
 * `on_delete` 三处都要调 —— 引擎会在别处摧毁这些 VM，所以这里按「有就删、删完清句柄」写，
 * 幂等（第二次进来句柄已经是 0）。 */
static void ufo_hud_kill(void)
{
    int i;
    ufo_xn_kill();                                     /* ★ #20：×N 也一并收干净 */
    ufo_break_flash_kill();                            /* ★ #12：击破爆炸也一并收干净 */
    ufo_panel_kill();                                  /* ★ #22：记录面板也一并收干净 */
    if (s_banner_id) { ce_anm_delete(s_banner_id); s_banner_id = 0; }   /* ★ #17 横幅（长持续效果铁律）*/
    for (i = 0; i < UFO_SLOT_COUNT; ++i) {
        if (s_slot_id[i]) { ce_anm_delete(s_slot_id[i]); s_slot_id[i] = 0; }
        s_slot_state[i] = -1;                 /* 下一帧从"没上过色"重来（新 VM 的 sprite 是脚本给的初始态）*/
        /* ★ Δ7：闪光 VM 会自己 `delete()`（一手 31 帧），但**句柄必须在这里作废** ——
         *   否则下一局的 VM 若复用了同一个 id，我们会去动别人的位置（长持续效果铁律的同类坑）。 */
        s_flash_id[i] = 0;
        s_flash_left[i] = 0;
    }
    s_slot_blink = 0;
    s_slot_blink_prev = 0;
    ce_hud_vm_text_clear(&s_timer_text);  /* ★ #9 剩余时间：换成 VM 之后要显式收（长持续效果铁律）*/
    s_timer_logged = 0;                  /* #9：计时器每次召唤只打一行「首次」诊断 */
}

/* 起/补 VM（幂等 + 自愈：引擎可能在别处摧毁它们；照 `signal.c` 的写法每帧查一次，开销可忽略）。
 * 生成顺序 = 叠放顺序（同层后生成的画在上面）：三格按 0→2 生成，与槽位从左到右一致。 */
static void ufo_hud_ensure(void)
{
    int i;
    /* ★ #17：**先**起槽底那抹毛笔横幅（同一层 25 ⇒ 先生成的画在下面 = 「槽底下」）*/
    if (!s_banner_id || !ce_anm_get_vm(s_banner_id)) {
        s_banner_id = ufo_hud_vm_spawn(UFO_HUD_SCRIPT_BANNER);
        if (s_banner_id) {
            static int s_logged_banner;
            /* ★ owner 2026-09-23：横幅与槽是同一个视觉组 ⇒ 同量左移。写的是 **entity_pos**
             *   （脚本自己的 `pos(0,934)` / `posTime(60,4,256,934)` 一个字不改 ⇒ 淡入动画照旧）。*/
            (void)ce_anm_set_pos(s_banner_id, UFO_HUD_BANNER_DX_SURF, 0.0f, 0.0f);
            if (!s_logged_banner) {
                s_logged_banner = 1;
                ce_log("ufo: brush banner under the slots up (ability.anm script %d <= front.anm script79, "
                       "sprite67 of front00.png; one-hand pos = surface (256, 934) = playfield bottom-left, "
                       "3 units below the field's bottom edge) + owner shift %.0f surface px",
                       UFO_HUD_SCRIPT_BANNER, (double)UFO_HUD_BANNER_DX_SURF);
            }
        }
    }
    for (i = 0; i < UFO_SLOT_COUNT; ++i)
        if (!s_slot_id[i] || !ce_anm_get_vm(s_slot_id[i])) {
            s_slot_id[i] = ufo_hud_vm_spawn(UFO_HUD_SCRIPT_SLOT);
            s_slot_state[i] = -1;             /* 新 VM：下一帧补一次四态中断 */
            if (s_slot_id[i]) ce_anm_interrupt(s_slot_id[i], UFO_HUD_INTERRUPT_SLOT0 + i);
        }
}

/* 新令牌落进第 i 格的动作：起一只闪光 VM（脚本 303）并给它摆位 + 上色，同时对本格发**出现**中断 17。
 * 一手出处：`FUN_004214b0` 的 0x20a02 段（`push 0x51` 建 VM → 摆位中断 + 四态中断）与
 * 紧随其后的「对该格发 17」（`mov edx,0x11`）。
 * ★ Δ7：闪光的**摆位**也归 C —— 一手同样把槽位做成 7/8/9 的 `posTime` 目标，而那条路在 TH18 里
 *   不落地（同槽本体，见常量区 Δ7）。闪光是 31 帧自删的一次性 VM ⇒ 句柄留在 `s_flash_id[i]` 里，
 *   由 `ufo_hud_slots_tick()` 在它的存活期内每帧摆一次（寿命计数 `s_flash_left[i]` 兜住"句柄变陈旧"）。 */
static void ufo_hud_slot_flash(int i, int32_t state)
{
    uint32_t id = ufo_hud_vm_spawn(UFO_HUD_SCRIPT_FLASH);
    if (!id) return;
    ce_anm_interrupt(id, UFO_HUD_INTERRUPT_SLOT0 + i);            /* 摆到第 i 格（7/8/9；Δ7 起只是配方保留）*/
    ce_anm_interrupt(id, UFO_HUD_INTERRUPT_STATE0 + state);       /* 上第 i 格的四态色（10..13）*/
    ufo_hud_place_at(i, id);                                     /* Δ7：当场摆一次（下一帧起由 tick 续）*/
    ufo_hud_vm_log(i, "flash", id);                              /* 闪光 VM 的实况（一次性，不刷屏）*/
    s_flash_id[i] = id;
    s_flash_left[i] = UFO_HUD_FLASH_FRAMES + 2;                  /* 一手 31 帧自删 ⇒ 多给 2 帧余量 */
    /* ★★★ 2026-09-23（owner：「飞碟槽的动画很奇怪，是先装进去了一个碟子，然后瞬间消失，然后才是
     *   一个放大缩小进入碟子的动画」）：**「放大缩小进槽」应当由这支 flash VM 自己演**。
     *   原来我们把这个出现动画发给了**槽位 VM**，而 flash VM 同时画着一枚**实心碟子**（alpha 255、
     *   盖在同一格上）⇒ 槽位 VM 的放大缩小被**盖住**；等 flash 在 31 帧自删后，那支动画才露出来
     *   ⇒ 观感正是「先有一个碟子 → 瞬间消失 → 才看到放大缩小」。
     *   改法：flash VM 演出现动画（它才是 owner 看到的那个碟子），槽位 VM 只**换色**（由 tick 的
     *   `interrupt(10+st)` 负责，这里再补一次保证顺序）。 */
    /* ★★ 2026-09-23（owner 实跑第 1 条）：「**收槽的动画还是不太对：收进去之后现在是空槽位本身会有一个
     *   放大再缩小的动作，一手显然不是**」⇒ 上一版把那支「出现动画」（脚本 302 的中断 17 = alpha 0→255 +
     *   scale 0→静态）发给了 **flash VM**，而它画的就是**槽位本身那张彩色框**（同一个 sprite）
     *   ⇒ 观感 = 槽自己放大缩小 ✗。本轮只保留四态换色（下一行）。
     *   ⚠ 诚实标注：一手 script81 的默认路径**确实有** scale 0.5→1.5→1.0 的爆闪，本轮的改法（303 去掉
     *     三段 scale + 这里不再发 17）是 **owner 口径**；要回退：303 加回那三行 + 这里加回发 17。 */
    if (s_slot_id[i]) ce_anm_interrupt(s_slot_id[i], UFO_HUD_INTERRUPT_STATE0 + state);
    ce_log("ufo: slot %d flash (state %d) - ability.anm script %d + slot interrupt %d/%d "
           "(one-hand th12:0x4214b0)", i + 1, (int)state, UFO_HUD_SCRIPT_FLASH,
           UFO_HUD_INTERRUPT_SLOT0 + i, UFO_HUD_INTERRUPT_STATE0 + state);
}

/* 每帧的槽刷新：四态变化 ⇒ 发一次中断（幂等）；UFO 在场 ⇒ 按一手那套**换 sprite** 做闪烁。 */
static void ufo_hud_slots_tick(ufo_card_t *s, int ufo_on_field)
{
    int i;

    ufo_hud_ensure();
    /* 闪烁相：一手 `FUN_00421690` 的调用侧（`th12:0x44a2e0`–`0x44a2fd`，2026-09-22 逐指令读出）：
     *   `mov eax,[zUfo+0x14]`（**UFO 自己的存活帧数**）→ `and eax,0x8000000f`（% 16）
     *   → `cmp eax,1; jne; push 1; call 0x421690`（**亮版**）/ `cmp eax,9; jne; push 0; call`（**暗版**）。
     * ⇒ 相位基准 = **UFO 的存活帧数**（本卡的 `s->u.frames`），不是全局游玩帧 ——
     *   旧实现用 `play_frame`（🟡 当时的猜测）会让「第一帧闪不闪」随机取决于你在第几帧召唤。
     * ⚠ 我们**每帧都写一次** sprite（一手里只在 %16 == 1/9 两帧写）：写法等价（中间帧的 sprite 本来
     *   就没人改），而且引擎若在别处重建过 VM，我们下一帧就自愈 —— 与 `ufo_hud_ensure()` 同一口径。 */
    s_slot_blink = (ufo_on_field && (s->u.frames % UFO_HUD_BLINK_PERIOD) == 1u) ? 1u : 0u;

    for (i = 0; i < UFO_SLOT_COUNT; ++i) {
        const int32_t st = ufo_hud_slot_state_of(s->grp.slot[i]);
        uint32_t id = s_slot_id[i];

        /* ③ ★ Δ7：闪光的摆位（在它 31 帧的寿命内每帧一次；过期就丢句柄，别再碰那个 id）。 */
        if (s_flash_id[i]) {
            if (s_flash_left[i]) {
                --s_flash_left[i];
                ufo_hud_place_at(i, s_flash_id[i]);
            } else {
                s_flash_id[i] = 0;                    /* 一手：脚本 303 已自删 ⇒ 句柄作废 */
            }
        }

        if (!id || !ce_anm_get_vm(id)) continue;      /* 引擎刚销毁过 ⇒ 下一帧 `ensure` 重建 */
        if (st != s_slot_state[i]) {                  /* ① 四态变了：发中断（新令牌还会带一只闪光 VM）*/
            const int32_t prev = s_slot_state[i];
            s_slot_state[i] = st;
            ce_anm_interrupt(id, UFO_HUD_INTERRUPT_STATE0 + st);
            if (prev >= 0 && st != 0)                 /* 从"有东西"变成"另一种东西"= 新令牌落进这一格 */
                ufo_hud_slot_flash(i, st);
        }
        /* ② ★★★ 2026-09-23（owner 实跑：「**收碟子之后碟子在 HUD 在有动画的时候出现一下然后消失了**」）：
         *   根因 = **持久配色原来只靠 `interrupt(10+st)`，而那一支在 TH18 里不落地**。
         *   一手走的是 `FUN_004214b0(gui, n)` = **直接写 sprite 号**（`COLLECTED_UFOS[i] + 10`，
         *   `th12:0x4214b0`），我们把它翻成了「发中断 10+n」；槽位 VM 的主脚本早已 `stop()`
         *   （`instr 140`），那支中断的 sprite 写不进画面 ⇒ **只有 UFO 在场时** C 侧每帧写 sprite
         *   才看得见颜色，UFO 一离场就退回脚本初值 `sprite 457`（空槽）⇒ 唯一还显示碟子的是
         *   那支 31 帧自删的 flash VM（脚本 303）⇒「出现一下然后消失」。
         *   日志铁证：`state 2/0/0` 那几帧的槽 VM `sprite 457 alpha 255`，而 UFO 在场（`frames 24`）
         *   那几帧正好是 `459/460/458`（= 457 + state）。
         *   ⇒ 改成**每帧直接写**（与一手同一个「直接写」口径）：亮版那一组 = 基址 + 4
         *   （`FUN_00421690` 的那套闪烁仍然只在 UFO 在场时成立 —— `s_slot_blink` 已经含这个门）。 */
        ce_anm_set_sprite(CE_ABILITY_ANM(), id,
                          UFO_HUD_SPR_SLOT_EMPTY + (int)st
                              + (s_slot_blink ? UFO_HUD_SLOT_BLINK_OFFSET : 0));
        /* ③ ★★ Δ7（本工单的根因修复）：每帧把三格钉回槽心 —— 一手那套"脚本自己摆位"
         *    （中断 7/8/9 → `posTime`）在 TH18 里不落地，三格会全部叠在脚本初始 x（= 296）上。 */
        ufo_hud_place_at(i, id);
    }
    /* 每 60 帧一行诊断（与环那条同款）：用户/我们只凭日志就能判「三格 VM 在不在、摆在哪、是哪个 sprite」。
     * ★★ 2026-09-23（「完全看不见」工单）：原来这条只打我们的**记账值**，看不到 VM 真正的
     *   位置/尺寸/alpha ⇒ 判不出「没画」还是「画在屏外」。现在把三格的**VM 实况**并进来：
     *     `draw` = `pos(+0x30)` + `__pos_2(+0x4e4)` + `ent(+0x5f0)`（三项全打） / `spr`(+0x24) /
     *     `scale`(+0x54) / `a`(color_1 `+0x524` 的 A 字节) / `L`(layer `+0x18`) / `flags_lo`(+0x534)。
     *   ⇒ 判据一句话：`draw` 必须 ≈ 本格槽心 (272/314/356, 912)、`scale 2.00`、`a 255`、`L 25`。
     * ★ 2026-09-23 第二轮（「槽位只有 1 个」）：第一版读错了字段（把 `+0x2c` instr_offset 当 pos），
     *   三格"pos.x"全是脚本值 296 却没打 y —— 这次三项 + 和全打，叠没叠一眼看得出。
     * ⚠ 节奏用**本函数自己的静态帧计数**，不用 `s->play_frame` —— 后者在关卡开头的若干帧是 0，
     *   会让 `% 60 == 0` 连打几十行（2026-09-22 的实跑日志里就是这样刷的）。 */
    if (++s_slot_log_tick >= UFO_SLOT_LOG_EVERY) {
        s_slot_log_tick = 0;
        ce_log("ufo/slot: vm %u/%u/%u state %d/%d/%d sprite-base %u blink %u/%u frames %u "
               "(one-hand th12:0x4214b0 + 0x421690; slot centres x = 272/314/356 surface)",
               s_slot_id[0], s_slot_id[1], s_slot_id[2],
               (int)s_slot_state[0], (int)s_slot_state[1], (int)s_slot_state[2],
               UFO_HUD_SPR_SLOT_EMPTY, s_slot_blink, s_slot_blink_prev, s->u.frames,
               UFO_HUD_SCRIPT_SLOT);
        for (i = 0; i < UFO_SLOT_COUNT; ++i) {
            char tag[8];
            tag[0] = 's'; tag[1] = 'l'; tag[2] = 'o'; tag[3] = 't';
            tag[4] = (char)('1' + i); tag[5] = '\0';       /* 手写 tag：不为一行日志引 snprintf */
            ufo_hud_vm_log(i, tag, s_slot_id[i]);
        }
    }
    s_slot_blink_prev = s_slot_blink;
}

/* ==================== ★★ 击破计时器（P3 #9）—— 一手全套见常量区的长注 ====================
 * 与 HUD 槽**无关**（原版那两条是各自独立的 VM/绘制点）⇒ 由 `on_draw` 直接调，不经过
 * `ufo_hud_draw()` 的「HUD 块已认领」闸（认领失败不该把计时器一起吞掉）。
 * 数值全整数（x87 硬门：本文件里任何 `(float)int` 都会带出 x87 指令）⇒ 秒/百分秒按一手那两条
 * 整数除法算，`remain*100/60 % 100` 与一手 `(int)(100 × remain/60) % 100` 同值。 */
static void ufo_timer_draw(const ufo_card_t *s)
{
    uint8_t *e;
    uint32_t remain, sec, hund;
    float x, y;

    /* ★ 三条"不画"的出口都必须**把贴图 VM 收掉**（ASCII 时代不用管：不调就不画；
     *   换成常驻 VM 之后留着就是"上一只的时间冻在屏幕上"）。*/
    if (!s || !ufo_present(&s->u)) { s_timer_logged = 0; ce_hud_vm_text_clear(&s_timer_text); return; }
    if (!ufo_play_frame()) return;                     /* ★ 非游玩帧（暂停 / 菜单）：一个字都不画 */
    if (s->u.frames < UFO_TIMER_FIRST_FRAME) { ce_hud_vm_text_clear(&s_timer_text); return; }  /* 一手 0x44a6ee：前 60 帧不画 */
    e = ufo_enemy_by_id(s->u.ufo_id);
    if (!e) { ce_hud_vm_text_clear(&s_timer_text); return; }   /* 一手 0x44a72c：敌人不在就不画 */

    remain = (s->u.frames < UFO_LIFE_MAX) ? (UFO_LIFE_MAX - s->u.frames) : 0u;
    sec    = remain / 60u;                                /* 一手：÷ [0x4a3d20] = 60.0 */
    hund   = (remain * 100u / 60u) % 100u;                /* 一手：× 100 → (int) → % 100 */

    /* 位置 = 敌人的**屏幕**坐标（TH18 的 ASCII 空间是 640×480：场地中心 x=320、上沿 y=16）+ 一手那 +20。
     * 一手还有一处 x −12（`fsub [0x4a4128]` = 12.0）—— 那是**把 5 个字符手工居中**（TH12 的 ASCII 默认左对齐）；
     * 我们用 `align_h = 居中`，同一个结果 ⇒ 不再重复那次位移。 */
    x = 320.0f + *(const float *)(e + CE_ENEMY_POS_X) + UFO_TIMER_DX640 + UFO_TIMER_OWNER_DX640;
    y =  16.0f + *(const float *)(e + CE_ENEMY_POS_Y) + UFO_TIMER_DY640;
    /* 格式串、颜色、字号逐条照抄一手（`"%2d:%.2d"` / 0xc080ffc0 / 1.0）。
     * ⚠ `font`/`group`/`scale` 传 -1/0 = 用**环境值**（TH12 明写字符集 2 与 scale 1.0；TH18 的
     *   「字体 id」与 TH12 的「字符集」不是同一个编号体系 ⇒ 不硬抄那个 2，见 TUNING §2.0v）。 */
    /* ★★ 2026-09-23（owner：「那个剩余时间画在模糊层之后，暂停不会模糊」）——
     *   ASCII 文字**永远进不了暂停模糊罩**（罩子 = `text.anm` script59 的 UI VM、层 38/绘制桩 0x3e；
     *   四个 ASCII 绘制趟里只有组 1 早于它，而那一趟在 surface 1 + 相机 0 上、实跑看不见 —— `SDK.md` §16）
     *   ⇒ 改成**贴图 VM 版文字**（`ce_hud_vm_text_*`：VM 放 UI 层 25，`render_layer` `0x4882b8` 折到 38
     *     ⇒ 与罩子同一个绘制桩，暂停时被罩住）。字形 = TH18 **font 2** 的原生切图（数字 + `:`）。
     *   ⚠ 串手写（同 ×N 那条口径）：一手格式 = `"%2d:%.2d"` ⇒ 秒两位（个位前补空格，`%2d` 的行为）、
     *     `:`、百分秒两位；`UFO_LIFE_MAX` = 720 ⇒ 秒恒 < 12 ⇒ 两位够。*/
    {
        unsigned s10 = sec / 10u, s1 = sec % 10u;
        unsigned h10 = hund / 10u, h1 = hund % 10u;
        char buf[8];
        buf[0] = (s10 > 0u) ? (char)('0' + s10) : ' ';    /* 一手 `%2d`：10 以下左补空格 */
        buf[1] = (char)('0' + s1);
        buf[2] = ':';
        buf[3] = (char)('0' + h10);
        buf[4] = (char)('0' + h1);
        buf[5] = '\0';
        ce_hud_vm_text_set_font(&s_timer_text, x, y, UFO_TIMER_COLOR, CE_ASCII_ALIGN_CENTER, 0.0f,
                                buf, "ufo-timer", CE_HUD_FONT_TH18_FONT2B);
    }
    if (!s_timer_logged || (s->play_frame % UFO_TIMER_LOG_EVERY) == 0u) {
        s_timer_logged = 1;
        ce_log("ufo/timer: %2u:%.2u left (%u = %u-frame cap - age %u) at screen 640 (%.0f, %.0f) "
               "= one-hand th12:0x44a6e3-0x44a7e8 (\"%%2d:%%.2d\", color 0xc080ffc0, +20 under UFO)",
               (unsigned)sec, (unsigned)hund, remain, (unsigned)UFO_LIFE_MAX, s->u.frames,
               (double)x, (double)y);
    }
}

static void ufo_hud_draw(ufo_card_t *s)
{
    uint8_t *body;

    if (!s) return;
    /* 认领还没成功（开场批次未跑完）⇒ 这一段什么也不做。VM 的坐标全在脚本里，不依赖槽的回读值。 */
    if (!ce_hud_slot_claimed(UFO_ID_NUM)) return;

    body = ufo_enemy_by_id(s->u.ufo_id);
    if (!s->logged_no_hud && s_slot_id[0]) {
        s->logged_no_hud = 1;
        ce_log("ufo: HUD up (TH12 original art) - 3 token slot VMs (ability.anm script %d <= front.anm "
               "script80) on UI layer 25, block %.0fx%.0f pinned at surface (%.0f, %.0f) = the "
               "PLAYFIELD bottom-left corner, aligned to card 113's block (248, 928); slots are positioned by "
               "interrupts 7/8/9 and coloured by 10/11/12/13 (empty/red/blue/green)",
               UFO_HUD_SCRIPT_SLOT, (double)UFO_HUD_BLOCK_W, (double)UFO_HUD_BLOCK_H,
               (double)UFO_HUD_PIN_X, (double)UFO_HUD_PIN_Y);
    }
    ufo_hud_slots_tick(s, body != 0);
}

/* ============================ ★★ 2026-09-18：HUD 槽的**认领**（两个 bug 一起修）============================
 * 用户 2026-09-18 实跑的 B4「**缺少 th12 UI 的飞碟槽 HUD**」有两个独立根因，逐条：
 *
 * ① **第三个实参传的是 NULL ⇒ 认领恒失败**（这是本轮真正让 HUD 一次都没出现过的那条）。
 *    `hud_register()` 的**第一句**就是 `if (card_id > … || !blk || !out) return 0;`（`sdk_core.c:933`）
 *    ⇒ `ce_hud_slot_claim_card(UFO_ID_NUM, &blk, 0)` **永远返回 0**、`s_hud[112].used` 恒 0
 *    ⇒ `ufo_hud_draw()` 的第一句 `ce_hud_slot_get()` 立刻返回（`ufo.c` 的 HUD 段）
 *    ⇒ 三格令牌槽 / 本体血条 / 一行读数**一次都没 spawn 过**（`ufo_hud_ensure()` 在它的后面）。
 *    ⚠ 同一个坑 111 吃过一次：`signal.c:3165-3168` 留着那条一手记录（「认领从来没成功过」）。
 *
 * ② **中盘拿卡不调 `on_load`** ⇒ 光修 ① 还不够。一手：卡 118 的「十二合一」走
 *    `ce_give_card()` → 引擎 `allocate_new_card(mode 2)` ⇒ **只调 `ctor`**（trace 里没有 `on_load`）
 *    ⇒ 107 / 110 / 111 后来都补了「每帧再认领一次」（`senro.c:654` / `mof.c:1788` / `signal.c:2813`），
 *    本卡当时没补。⇒ 这一支就是那次补齐，**同一形状**。
 *
 * 幂等性：`ce_hud_slot_claim_pinned_card()` 自带**关卡纪元**（`ce_hud_stage_begin()`：关卡号变了才先清
 * 旧占位）；同一关内重复认领只是把同一块矩形再写一遍（pinned ⇒ 不触发 `hud_relayout()`）；
 * 稳态下这一支每帧只花「一次 `ce_hud_slot_claimed()` 比较」。日志各只打一次（成功/失败各一行）。
 * ★ 2026-09-18 第三版：入口从 `_card` 换成 **`_pinned_card`**（块钉在原版槽的位置，理由见常量区）。 */
static uint8_t s_hud_claimed;          /* 1 = 本关已认领成功（幂等闸；删卡/换局时清）*/
static uint8_t s_hud_logged_ok;        /* 成功那一行只打一次 */
static uint8_t s_hud_logged_fail;      /* 失败那一行只打一次（并写明原因）*/

static void ufo_hud_claim_forget(void)
{
    s_hud_claimed = 0;                 /* 只清自己的闸；槽位的账归 on_load / on_delete / 纪元 */
}

static void ufo_hud_ensure_registered(void)
{
    ce_hud_block_t blk;
    ce_hud_slot_t  got;

    if (s_hud_claimed && ce_hud_slot_claimed(UFO_ID_NUM)) return;   /* 常态：一次比较就回去 */
    memset(&blk, 0, sizeof blk);
    blk.w = UFO_HUD_BLOCK_W;
    blk.h = UFO_HUD_BLOCK_H;
    /* ★ 第三版起 = **自带锚点**（`CE_HUD_SIDE_PINNED`）：钉在原版槽的位置上（理由见常量区那一段 ——
     *   槽的坐标写在脚本里，块必须与它同源；走货架排布一旦被重排就会"块在这、槽在那"）。
     *   ★ 入口必须是 `_pinned_card` 版（照 `faith_hud.c` 的同一口径）：它才是带**关卡纪元**的那一个。 */
    blk.side = CE_HUD_SIDE_PINNED;
    if (!ce_hud_slot_claim_pinned_card(UFO_ID_NUM, &blk, UFO_HUD_PIN_X, UFO_HUD_PIN_Y, &got)) {
        if (!s_hud_logged_fail) {                      /* ★ 真 out（null 就是 2026-09-18 那个 bug①）*/
            s_hud_logged_fail = 1;
            ce_log("ufo: ce_hud_slot_claim_pinned_card(%u, %.0f, %.0f) FAILED - HUD block %.0fx%.0f "
                   "not claimed (HUD-LAYOUT §3/§4.2：块越界 / 锚点+块宽 > 1040 / out 传空都会走到这里)",
                   UFO_ID_NUM, (double)UFO_HUD_PIN_X, (double)UFO_HUD_PIN_Y,
                   (double)blk.w, (double)blk.h);
        }
        return;
    }
    s_hud_claimed = 1;
    if (!s_hud_logged_ok) {
        s_hud_logged_ok = 1;
        ce_log("ufo: HUD block %.0fx%.0f claimed (pinned) at surface (%.0f, %.0f) (= 640 space "
               "(%.1f, %.1f)); the slot VMs are pinned to surface (272/314/356, 912) by ufo_hud_place_at "
               "(script 302 writes `scale(2.0,2.0)` + `resolutionMode(2)`; ★ 2026-09-23 订正: "
               "`resolutionMode(2)` 在本机 1280x960 下与「不写」等价（都是 x1，一手 `th18:0x4063d0`）"
               "—— 当初让槽从看不见变看得见的是 Δ4b 的 `scale(2.0)`)",
               (double)blk.w, (double)blk.h, (double)got.x, (double)got.y,
               (double)(got.x * CE_HUD_SURFACE_TO_640), (double)(got.y * CE_HUD_SURFACE_TO_640));
    }
}

/* ================================ 回调 ================================ */
static int on_ctor(ce_card_t *c)
{
    ufo_card_t *s = ce_state(c, ufo_card_t);
    if (!s) return 0;
    /* ★ 先收本体的跟随 VM **再** memset：句柄就存在这块状态里 ⇒
     * 只 memset 的话句柄没了、VM 却还留在场上（长持续效果铁律里最典型的那种漏）。
     * ★★ 换池后令牌与它那一堆 VM **不在这块状态里**（都在文件静态 `s_tokpool` / `s_penta_id`）
     *   ⇒ memset 清不到它们，所以**必须显式**收：`ufo_tok_pool_clear_all()`（只看句柄、不看 `color`）。
     *   这一步是**幂等**的（池空 = 无操作），而且**不依赖 tick** —— 删卡那一刻 tick 全停也照样收。
     *   正常路径上它已经是无操作（`ufo_withdraw_all` 先收过一遍），这里是防御，不是主路径。 */
    /* ★★ 换关重入（关卡号变了）⇒ **令牌池与三格槽原样带过去**（owner 口径，见 `s_stage_seen` 的注）：
     *   池不清（VM 一个都不删，句柄留在文件静态区）、槽不清。其余每关该重来的照旧重来。*/
    {
        uint32_t stage = (uint32_t)CE_CURRENT_STAGE();
        int per_stage = (s_stage_seen != UFO_STAGE_SEEN_NONE && stage != s_stage_seen);
        ufo_group_t keep_grp;
        uint16_t    keep_filled = 0;
        if (per_stage) { keep_grp = s->grp; keep_filled = s_tokpool.n; }
        else ufo_tok_pool_clear_all();
        ufo_body_vm_kill(s);            /* ⚠ 必须**在 memset 之前**：句柄就在这块状态里 */
        memset(s, 0, sizeof *s);
        ufo_group_reset(&s->grp);
        ufo_reset(&s->u);
        if (per_stage) {
            s->grp = keep_grp;
            /* 令牌池的 `n` 与各条记录本来就留在 `s_tokpool`（没被 memset 也没被 clear）⇒ 只报一行日志 */
            if (keep_filled || keep_grp.filled)
                ce_log("ufo: stage %u start - KEPT %u field token(s) + %d collected token(s) in the "
                       "slots (slots are a per-RUN tally; the only one-hand clear point is 'the UFO "
                       "ended', th12:0x44a326)", (unsigned)stage, (unsigned)keep_filled,
                       (int)keep_grp.filled);
        }
        s_stage_seen = stage;
    }
    s->pend_drop = 0;
    s_tokpool.dropped = 0;                             /* 诊断计数每关从零（与 113 的口径一致）*/
    s->next_token_at = UFO_TOKEN_SPAWN_FIRST;          /* 开局先等一会儿再出第一个令牌 */
    /* ★ P2：给令牌颜色的随机数播种（文件静态的 LCG）。用「关卡号 ⊕ 帧钟 ⊕ 一个奇常数」混一次 ——
     *   同一局内不重复、重开一局会变；**不调引擎 RNG**（那两支走 x87，见 `s_rng_seed` 的注）。 */
    s_rng_seed = ufo_rand_next(0x9e3779b9u ^ (uint32_t)CE_CURRENT_STAGE() ^ ce_sdk_frame());
    ufo_se_reset();                                    /* ★ se 诊断：本关的「哪一枚响了几次」从零起 */
    return 0;
}

static int on_load(ce_card_t *c)
{
    on_ctor(c);
    /* ★ HUD 槽**认领**（用户 2026-09-12 裁决 ④）：走带关卡纪元的入口
     * `ce_hud_slot_claim_pinned_card()`（第三版起；块 = 自带锚点）⇒ 关卡号一变就先清掉上一关的
     * 占位再认领（HUD-LAYOUT §4.4），
     * 同一关内（局中删卡）一笔都不重排；认领**幂等**（`on_load` 每关都会再来一次）。
     * ★★ 2026-09-18：真身搬进 `ufo_hud_ensure_registered()`（拖两个 bug 一起修 —— NULL out 恒失败 +
     *   中盘拿卡不调 `on_load`）。`on_load` 只是**第一次**叫它；每游玩帧还会补叫一次
     *   （`on_item_frame` 顶部），幂等。 */
    ufo_hud_claim_forget();                 /* 换关/重进：闸复位，让下面那一支重新认领一次 */
    ufo_hud_ensure_registered();
    return 0;
}

/* 把本卡留在场上的东西全部收干净：**本体**（真敌人：静默撤，不析构）+ **令牌池**（VM，直接删）。
 * ★ 换关 / 局末 / 删卡三条路都要走它（METHOD 铁律：长持续效果必须交代「被删掉会怎样」）。
 * ★ 令牌的 VM 与五边形框 **都在文件静态区**（换池的结果）⇒ 删卡那一刻 tick 全停也收得掉 ——
 *   这正是用户点名的那条（「只有 `on_delete` 跑得到」）：`ufo_tok_pool_clear_all()` 只看句柄。 */
static void ufo_withdraw_all(ufo_card_t *s, const char *why)
{
    int n = 0;
    ufo_body_vm_kill(s);
    /* ★ 2026-09-24：召唤特效（横幅 324 / 闪电碎屑 346）也在这里收 —— 它们由**引擎**跑，
     * 卡的 tick 停了它们照样会画完那几十帧（METHOD 长持续效果铁律：删卡那一刻只有本函数跑得到）。 */
    ufo_summon_fx_kill();
    /* ★★ 2026-09-24：**宿主皮**（脚本 351/352 + 头顶飞碟 353）也在这里收 —— 而且必须**还原**宿主
     * 原生贴图的颜色（我们是每帧把它的 alpha 写成 0 来做"换皮"的，不收就会留下隐形敌人）。*/
    ufo_carrier_kill_all();
    /* ★★ 实体上的两件 UI（吸收环 / 血量条）：**必须在这里收** —— 删卡那一刻 tick 全停
     * （`on_delete`），漏了就会在场上留一圈弧 + 一条血条（METHOD 长持续效果铁律）。 */
    ufo_gauge_kill();
    n += ufo_tok_live_count();                         /* 池里有几只 ⇒ 就是一个日志数字 */
    ufo_tok_pool_clear_all();                          /* ★ 令牌 VM + 五边形框，全部只看句柄 */
    ufo_drop_kill_all();                               /* ★ 掉落物实体（压缩包 / Bomb）也在静态区 */
    ufo_pull_forget_all();                             /* ★ 拉拽累计速度表（静态区）*/
    if (s_tokpool.dropped)
        ce_log("ufo: token pool dropped %d token(s) while full (cap %d)", s_tokpool.dropped,
               UFO_TOKEN_CAP);
    if (ufo_present(&s->u)) {
        uint8_t *e = ufo_enemy_by_id(s->u.ufo_id);
        if (e) *(uint32_t *)(e + CE_ENEMY_FLAGS) |= UFO_FLAGS_DELETE_NOW;
        ufo_teardown(s, why);
        ++n;
    }
    if (n && why) ce_log("ufo: withdrew %d entity/entities (%s)", n, why);
}

static void on_stage_start(ce_card_t *c)
{
    ufo_card_t *s = ce_state(c, ufo_card_t);
    if (!s) return;
    if (s->spawned_tokens || s->ufos_spawned)
        ce_log("ufo: stage change - %u tokens spawned, %u UFOs (%u broken / %u vanished / %u failed), "
               "%u items pulled, %u rewards, %u tokens dropped", s->spawned_tokens, s->ufos_spawned,
               s->ufos_broken, s->ufos_vanished, s->failed_summons, s->items_pulled, s->rewards_spit,
               s->dropped_tokens);
    /* ★★★ 2026-09-23（owner：「一面进二面，换关数的时候 token（飞碟）清零」= 经典 bug）——
     *   这里以前是 `ufo_withdraw_all(s, "stage start")` + `on_ctor(c)`，等于**换关就把令牌全清**：
     *     · `ufo_withdraw_all` 会 `ufo_teardown` ⇒ **清三格槽**；还会清掉令牌池（场上令牌）；
     *     · `on_ctor` 再 memset 一遍卡私状态（槽又一次归零）。
     *   ⇒ 现在换关**只做两件必须做的事**，令牌（池 + 槽）原样跨关：
     *     ① 本体：换关时引擎已经在 stage-end 静默销毁那只真敌人（`DEATH` §1.4：不报事件）
     *        ⇒ 本体 VM 与 `u` 状态收干净。若这一刻 UFO 居然还活着（时序边界），也当场结束它，
     *        但**槽不清**（那是玩家的账，不是"UFO 正常结束"）。环 / 血条 / **召唤特效（横幅 + 闪电碎屑）**
     *        都跟着本体收（`ufo_summon_fx_kill()`，2026-09-24 补 —— 那两只 VM 由引擎跑，卡停了也会画完）。
     *     ② HUD 认领闸复位 + 当场补一次登记（新关卡按新布局重新登记；槽位 VM 的账在关卡纪元里）。*/
    if (ufo_present(&s->u)) {
        ufo_group_t keep = s->grp;                     /* ★ owner 口径：换关不清槽 */
        ufo_gauge_kill();                              /* 环 + 血条（跟着本体）*/
        ufo_summon_fx_kill();                          /* ★ 召唤特效（横幅 + 闪电碎屑）也跟着本体 */
        ufo_carrier_kill_all();                        /* ★ 宿主皮（换关敌人全清 ⇒ 皮也要收 + 还原色）*/
        ufo_body_vm_kill(s);
        ufo_teardown(s, "stage start");
        s->grp = keep;
    } else {
        ufo_body_vm_kill(s);                           /* 幂等：敌人已经不在也不会出错 */
        ufo_gauge_kill();
        ufo_summon_fx_kill();                          /* ★ 幂等（正常情况下它们早自删了）*/
        ufo_carrier_kill_all();                        /* ★ 幂等 */
    }
    {
        int i;
        /* 五边形框的句柄在文件静态区：换关后要按**新关卡**的条件重判 ⇒ 全部收掉重来（令牌一个没动）。
         * 本函数不是死亡栈（`on_stage_start` 在引擎的事件位次上），`ce_anm_spawn` 合规。*/
        for (i = 0; i < UFO_TOKEN_CAP; ++i) {
            s_tokpool.tok[i].pentagon_hint = 0;
            ufo_tok_penta_kill(i);
        }
        s->pentagon_any = 0;
        s->pentagon_logged = 0;
    }
    /* ★ 2026-09-18：换关 ⇒ 让 HUD 认领的闸复位（新关卡要按新布局重新登记一次；
     *   关卡纪元本身也会清旧占位，这里是同一条纪律的第二道）。 */
    ufo_hud_claim_forget();
    ufo_hud_ensure_registered();                       /* 当场补一次，别等第一帧游玩帧 */
}

/* ★ 共享死亡事件（spec §7 / DEATH §4 表）：只认 `KILLED` / `BOMB_KILL`，**必须排除 `KILL_ALL`**
 *（过场清场会把计数器刷满）。回调里**禁止**析构任何敌人、**禁止**缓存 `ev->enemy`。
 *
 * ★★ 2026-09-12 换池：这里**只剩两件事** ——
 *   ① 本体被打死 ⇒ 记位置（帧末开奖）；
 *   ② **令牌的来源**：小怪被击破 ⇒ 登记一个落点（帧末建令牌）。
 *   删掉的那一件是「**令牌自己被子弹打掉 ⇒ 也算收**」（`ufo_token_kill_counts` + `ufo_tok_slot`）：
 *   它是**真敌人载体**的产物（令牌有 HP 才会死）。一手 TH12 的令牌是**道具**（打不掉、只能接触收）
 *   ⇒ 用户拍板的换池把那条路整个去掉了（`AUDIT.md` §BO-11 的 P3 代价①）。
 *   ⚠ 顺带的好处：`on_enemy_death` 里**不再有「撤掉我们的东西」**这种动作（只登记两个数）⇒
 *     `DEATH` §2.4 的两条禁令现在天然满足（不改任何敌人对象、不建 VM）。 */
static void on_enemy_death(ce_card_t *c, const ce_enemy_death_t *ev)
{
    ufo_card_t *s = ce_state(c, ufo_card_t);
    if (!s || !ev) return;
    (void)c;
    /* ① UFO 本体被打死 ⇒ 击破（记下位置；开奖放帧末 —— 与飞离分支共用同一段，顺序也更清楚）*/
    if (ev->id == s->u.ufo_id && ufo_body_kill_counts(ev->kind)) {
        s->last_x = ev->x;
        s->last_y = ev->y;
        s->pending_break = 1;
        return;
    }
    /* ★★ ② **令牌从哪来**（2026-09-12 订正；用户口述「会有小怪爆飞碟」）----------------
     * 一手 = TH12 的敌人**掉落表**：`dropExtra(10..12, n)`（ECL op 407，handler `th12:0x4179cc`）
     * 由 `EnemyDrop__spawn_ex_drops` `th12:0x4121e0` 生成为**道具**（type 10..15）⇒ 令牌是
     * 「敌人被击破时按其掉落表掉出来的」。**TH18 的敌人没有这张表**（type 10..15 在 TH18 是别的
     * 东西/不存在）⇒ 只剩两件事能照一手：**触发点**（击破时）与**落点**（那只敌人的位置）。
     * 本节此前是「卡侧按 300 帧节流自己撒」，与一手/用户口述都不符 ⇒ 已删（见 `on_item_frame`）。
     * 门（全是 ❓ 自定，一手没有对应物）：
     *   ① 死因：`ufo_body_kill_counts()`（= 打死 / 炸弹秒杀；`KILL_ALL` 过场清场**一律不算**）；
     *   ② **有掉落表的真敌人**：`CE_EDEATH_F_HAS_DROP` + `CE_EDEATH_F_REAL` —— 一手那边令牌来自
     *      某个**具体敌人子程序**的掉落表，TH18 没有表可比 ⇒ 用「这只敌人本来就会掉东西」当代理；
     *      ⚠ 顺带天然排除**我们自己的东西**（本体 spawn 时 `main_drop = 0` ⇒ 没有这个位）。
     *      ⚠ 令牌**不再有** `main_drop` 这回事（它已经不是敌人了，永远进不了这个回调）。
     *   ③ 节流 `next_token_at`（一手是「表里有就掉、没有不掉」，没有概率也没有冷却）；
     *   ④ 场上上限 `UFO_TOKEN_MAX`；⑤ 落点先到先得（`pend_drop` 只存一格）。
     * ⚠ 回调里**只登记**（建 VM 是禁止的 —— 要分配，`DEATH` §2.4）⇒ 帧末 `on_tick_2` 才建。
     * ==========================================================================================
     * ★★★ 2026-09-24（owner：「th12 原版掉落飞碟的怪……只有**大蝴蝶**和**杂鱼妖精**（+ boss）；
     *   **得改谁能飞碟掉落**」）—— 判据换成**宿主表**（`UFO_CARRIER_KEYS[]`，见上面的注释块）：
     *   · 命中宿主表 ⇒ 掉**这一档的枚数**（妖精 1 / 大蝴蝶 2），**颜色按关固定**
     *     （一手 `dropExtra(10/11/12, n)` = 红/蓝/绿，写死在敌人的掉落表里）；
     *   · 是 boss（`CE_ENEMY_FLAG_BOSS`）⇒ 1 红（+ 1 绿，见 `UFO_CARRIER_TOKENS_BOSS_COLOR2`）
     *     —— 一手：TH12 每个 boss 相位都写 `dropExtra(10,1) + dropExtra(12,1)`；
     *     道中 boss 死那一笔 10~15 蓝**不采用**（owner 2026-09-24 判"不可能掉这么多"，
     *     与社区全局账（整轮约 55 个红+虹令牌）也矛盾）—— 理由与证据见 `ufo_carrier_table.h` 头注。
     *   · 其它敌人 ⇒ **不掉**（旧行为在 `UFO_TOKEN_DROP_LEGACY` 后面，默认关）。
     * ⚠ 一手没有"冷却"（表里有就掉），但宿主可能连着死一片 ⇒ 保留 `next_token_at` 当**上限保险**；
     *   场上的硬上限仍是 `UFO_TOKEN_MAX`。 */
    {
        const uint8_t *ee = (const uint8_t *)ev->enemy;
        ufo_carrier_t *cc = ufo_carrier_find(ev->id);
        int is_carrier = ufo_carrier_host_now(ev->id);   /* ★ 只有"每波挑选"选中的那一只掉 */
        /* ★ 110/111 的护栏（帧内有效）：整组被认领的那一只**不算宿主**（防御性复核一遍）。 */
        if (is_carrier && ee && ce_fairy_group_claimed(*(const int32_t *)(ee + CE_ENEMY_ANM_SRC),
                                                      *(const int32_t *)(ee + CE_ENEMY_ANM_SCRIPT)))
            is_carrier = 0;
        cc = ufo_carrier_find(ev->id);                   /* 判定后可能新建/更新过 ⇒ 重新取一次 */
        int is_boss = ee && (*(const uint32_t *)(ee + CE_ENEMY_FLAGS) & CE_ENEMY_FLAG_BOSS);
        int want = 0;
        int32_t col = UFO_COLOR_NONE;
        if (!(ev->flags & CE_EDEATH_F_REAL) || !ufo_body_kill_counts(ev->kind) || s->pend_drop ||
            ufo_tok_live_count() >= UFO_TOKEN_MAX) {
            /* 清场 / 假敌人 / 上一枚还没兑现 / 场上满了 ⇒ 这一只不算 */
        } else if (is_carrier || is_boss) {
            /* ★ owner 第 11 轮：掉落一律是"**会变的、随机起始色**"（`col` 留 `UFO_COLOR_NONE`
             * ⇒ 兑现时走 `ufo_tok_spawn()`）。
             * ★ owner 第 18 轮再收一格：「**一个非 boss 敌人只掉 1 个飞碟，不是 2 个**」
             * ⇒ **宿主（妖精/蝴蝶）1 枚、boss 2 枚**。
             * ⚠ 一手**没有冷却**（表里有就掉）⇒ 宿主/boss 这条**不吃 `next_token_at` 节流**
             * （一波里那 2 只宿主都该掉；节流只留给 `UFO_TOKEN_DROP_LEGACY` 那条旧路）。 */
            want = is_boss ? UFO_CARRIER_TOKENS_BOSS : UFO_CARRIER_TOKENS_DROP;
        }
#if UFO_TOKEN_DROP_LEGACY
        else if ((ev->flags & CE_EDEATH_F_HAS_DROP) && s->play_frame >= s->next_token_at) {
            want = 1;                                   /* 旧行为：任意带掉落表的小怪，一枚、随机色 */
        }
#endif
        if (want > 0) {
            s->pend_drop = 1;
            s->pend_drop_n = (uint8_t)want;
            s->pend_drop_color = col;
            s->pend_drop_x = ev->x;
            s->pend_drop_y = ev->y;
            if (is_carrier || is_boss)
                ce_log("ufo: token drop queued - %s (N %d, S %d)%s -> %d token(s), cycler + random "
                       "start colour (owner r11: two colour-changing random ones)",
                       is_carrier ? (cc && cc->kind == (uint8_t)UFO_CARRIER_KIND_BFLY ? "BUTTERFLY host"
                                                                                             : "FAIRY host")
                                  : "boss",
                       ee ? (int)*(const int32_t *)(ee + CE_ENEMY_ANM_SRC) : -1,
                       ee ? (int)*(const int32_t *)(ee + CE_ENEMY_ANM_SCRIPT) : -1,
                       is_boss && !is_carrier ? " [boss]" : "", want);
        }
    }
}

#if UFO_DEBUG_KEY
/* ★ 2026-09-23（用户原话）：「**本卡的 shift+空格调试换成：爆大量 P 点和金钱点，方便测试 UFO 吸收**」。
 *   · 圆心 = **场上的那台 UFO**（`ufo_enemy_by_id` ⇒ 吸收链的目标就是它）；没有 UFO 时围着自机
 *     （吸收链照样会把它们拉向本体，只是要先等本体出来）。
 *   · 铺法 = `COLS × ROWS` 的**固定偏移网格**（两种点交错），半径刻意大于 18 单位的「吞」圈
 *     ⇒ 能看到它们**一颗一颗被拉进去**，同时压到「吞」与「拉」两条路。
 *   · ⚠ **不用三角函数**：本仓硬门 = 我们的 object 里 0 条 x87（`make dllx87`），表/网格最稳。
 *   · 落点在同一帧内不重叠：偏移是整数倍的步长（`DX/DY`），每颗再给同一组固定初速让引擎物理散开。 */
static void ufo_debug_spawn_items(ufo_card_t *s)
{
    uint8_t *e = ufo_enemy_by_id(s->u.ufo_id);
    float cx, cy;
    int made_p = 0, made_m = 0, refused = 0;
    int row, col;

    if (e) {
        cx = *(const float *)(e + CE_ENEMY_POS_X);     /* 吸收目标 = UFO 本体 */
        cy = *(const float *)(e + CE_ENEMY_POS_Y);
    } else {
        ufo_player_xy(&cx, &cy);                       /* 没 UFO ⇒ 围着自机（等本体现身再吸）*/
    }
    for (row = 0; row < UFO_DEBUG_ITEM_ROWS; ++row) {
        for (col = 0; col < UFO_DEBUG_ITEM_COLS; ++col) {
            float pos[3];
            /* 交错：奇偶格各出一种点 ⇒ 一眼能看出 P 点与金钱点**都在**被吸 */
            uint32_t type = (((row * UFO_DEBUG_ITEM_COLS + col) & 1) ? UFO_ITEM_TYPE_MONEY
                                                                     : UFO_ITEM_TYPE_P);
            pos[0] = cx + ((float)col - (float)(UFO_DEBUG_ITEM_COLS - 1) * 0.5f) * UFO_DEBUG_ITEM_DX;
            pos[1] = cy + ((float)row - (float)(UFO_DEBUG_ITEM_ROWS - 1) * 0.5f) * UFO_DEBUG_ITEM_DY;
            pos[2] = 0.0f;
            if (ce_spawn_item(type, pos, UFO_DEBUG_ITEM_ANGLE, UFO_DEBUG_ITEM_SPEED)) {
                if (type == UFO_ITEM_TYPE_MONEY) ++made_m; else ++made_p;
            } else {
                ++refused;
            }
        }
    }
    ce_log("ufo: DEBUG item burst by SHIFT+SPACE - %d P + %d money around %s (%.0f, %.0f); "
           "grid %dx%d step %.0fx%.0f, %d refused (item pool full?)",
           made_p, made_m, e ? "the UFO" : "the player (no UFO on field)", (double)cx, (double)cy,
           UFO_DEBUG_ITEM_COLS, UFO_DEBUG_ITEM_ROWS,
           (double)UFO_DEBUG_ITEM_DX, (double)UFO_DEBUG_ITEM_DY, refused);
}
#endif  /* UFO_DEBUG_KEY */

static void on_item_frame(ce_card_t *c)
{
    /* ★ 2026-09-18（用户口径）：向**共享 PIV 池**声明本卡在场 + 本卡的可写上限（`cap_i − init_i`，按难度查表）。
     *   一行、幂等（每帧重申，难度一变立刻跟上）；退场由 `ce_layer_drop_card()` 兜底。
     *   擦弹那**一项**是**共用写入者**（用户口径：别加一张卡擦弹数的作用就 +1 次）⇒ 这里只声明「我在这一族」。
     *   口径与表的一手出处：`engine/faith/OVERVIEW.md`「结论：th12–th17 可以共用一个 PIV」+ `sdk_core.h` 同名注释块。*/
    ce_piv_claim_auto(ce_card_id(c), ce_difficulty());
    ce_graze_family_claim(ce_card_id(c));

    ufo_card_t *s = ce_state(c, ufo_card_t);
    if (!s) return;
    ++s->play_frame;
#if UFO_DEBUG_KEY
    {   /* ★ 调试（照 117 `animal.c` 同款）：边沿检测 + 一行日志。
         *   ★ 2026-09-24 第 21 轮键位（owner）：「**调试改成：Ctrl+空格，掉落一个满 B**」
         *   ⇒ **空格 = 1 枚固定色令牌** / **Shift+空格 = 爆一批 P 点+金钱点** /
         *      **Ctrl+空格 = 掉一颗满 B**（原来那件"1 枚变色令牌"退役；要验证变色链就直接看满 B 那条路）。*/
        int down  = ce_key_down(CE_VK_SPACE) ? 1 : 0;
        int shift = ce_key_down(CE_VK_SHIFT) ? 1 : 0;
        int ctrl  = ce_key_down(CE_VK_CONTROL) ? 1 : 0;
        if (down && !s->dbg_key_prev && shift) {
            ufo_debug_spawn_items(s);                  /* ★ 本轮新行为（用户 2026-09-23 口径）*/
        } else if (down && !s->dbg_key_prev && ctrl) {
            /* ★★ owner 2026-09-24 第 21 轮：「**Ctrl+空格，掉落一个满 B**」—— 直接走 P6 那条
             *   「绿 UFO 喂饱」的生产函数（原生 `item type 7` + 我们的卡面/背光两层 VM）。
             *   用途：不用真去打 UFO，一眼就能验"满 B"的**摇摆 / 尺寸 / 那颗原生星有没有藏掉 / 光晕**。*/
            ufo_drop_spawn_bomb(s, UFO_DEBUG_X, UFO_DEBUG_Y);
        } else if (down && !s->dbg_key_prev) {
            if (ufo_tok_spawn(s, UFO_DEBUG_X, UFO_DEBUG_Y))
                ce_log("ufo: DEBUG spawn token #%u at (%.0f, %.0f) by SPACE (color %d, %s)",
                       s->token_serial - 1u, (double)UFO_DEBUG_X, (double)UFO_DEBUG_Y,
                       (int)s_tokpool.tok[s_tokpool.n - 1].color,
                       s_tokpool.tok[s_tokpool.n - 1].cycler ? "cycler" : "plain");
            else
                ce_log("ufo: DEBUG spawn token by SPACE failed (pool full? %d slots)", UFO_TOKEN_CAP);
        }
        s->dbg_key_prev = down;
    }
#endif

    /* ★★ 2026-09-18：每游玩帧补一次 HUD 槽认领（幂等，稳态一次比较）。
     *   为什么必须在这里：中盘拿到卡（卡 118 的十二合一）时引擎**只调 `ctor`、不调 `on_load`**
     *   ⇒ 认领从没发生过 ⇒ HUD 不出现，要等换面才补上（用户实跑 B4）。
     *   放在 `on_item_frame`：它 = 每个**正常游玩帧**恰好一次（暂停/商店/对话帧到不了这里），
     *   与 107（`senro.c` 的 tick）/ 110·111（`mof.c`/`signal.c` 的 `on_tick_2`）同一口径。 */
    ufo_hud_ensure_registered();
    /* ★ 存活帧推进放在这里：`on_item_frame` 是**每个正常游玩帧恰好一次**的分派点
     * （暂停 / 商店 / 对话帧连门卫 CMP 都到不了）⇒ 只有这里调，720 帧的上限才名符其实。 */
    if (ufo_present(&s->u) && ufo_tick_advance(&s->u) && !s->logged_lifetime) {
        s->logged_lifetime = 1;
        /* 到一手存活上限 720 帧。⚠ **这不算飞走**：TH18 的飞走由 ECL `delete()` 驱动、
         * 不经过 `Enemy__die`（`UFO:238-239`），卡侧只认「id 不在了」那个信号（spec §9 #10）。
         * ⇒ 上限到点只报一条日志（占位惰性壳根本不会动，飞走这件事**现在做不到**）。 */
        ce_log("ufo: lifetime cap %d reached but the enemy is still alive - the ECL side has not flown "
               "away (placeholder ECL does not move at all; spec §6 step 3)", UFO_LIFE_MAX);
    }
    /* ★ 本体跟随 VM：每帧把画面搬到敌机的 `abs_pos`（敌机本体在 ECL 侧是**隐形载体**，
     * 画面全靠这个 VM —— 一手链见文件头「跟随 VM」注释块）。
     * 放在**召唤之前**：这一帧新召唤出来的本体由 `ufo_try_summon` 里的
     * `ufo_body_vm_spawn` 自己摆位置（不会漏）；而「敌机被静默撤掉」那一帧也由这里收 VM。 */
    ufo_body_vm_follow(s);
    /* ★★ 2026-09-24（owner「还得给掉的那个改帖图」）：每游玩帧给**宿主敌机**挂皮 + 头顶旋转飞碟
     * （扫 `(N,S)` 命中 `UFO_CARRIER_KEYS[]` 的敌机；皮/标记/还原原生色全在那一支里，见它的注释块）。*/
    ufo_carrier_tick();
    /* 令牌出现在这一帧？—— ★ 2026-09-12 本轮订正：**触发点改成「小怪被击破」**
     * （令牌实体由 `on_enemy_death` 登记落点、`on_tick_2` 建出来），**不再**按固定帧自己撒。
     * 一手的令牌是敌人掉落表的产物（`dropExtra(10..12,n)`，见 `on_enemy_death` ③）。
     * ⚠ 这同时解掉了旧写法的一个副作用：**空场也会定时冒出令牌**（与「小怪爆飞碟」不符）。 */
    /* ★★ ④ 两条离场路（2026-09-18，一手逐条见 `AUDIT.md` §DQ）--------------------------------
     * 一手（`Ufo::on_tick` `0x44a395`–`0x44a3df`）：
     *   · **对话中**（`Gui.msg`(`Gui+0x6d30`) != 0）⇒ `life.is_spell |= 2`；下一帧
     *     `EnemyData::on_tick`(`0x413f5d`) 看到位 2 ⇒ 调 `Enemy::die`(`0x414af0`) ⇒ 走
     *     `on_death_callback`(`0x44a890`) ⇒ **`FUN_0044af00` = 击破那条开奖路**。
     *     ⇒ 一手的「对话期间」= **原地自爆 + 全额发奖**（不是不发奖的飞走）。用户 2026-09-18 的记忆
     *     （「对话期间应该也会直接飞走」）与一手矛盾 ⇒ **本实装按一手**，差异登记在
     *     `AUDIT.md` §DQ / `TUNING.md` §2.0l，等你裁决（要改成记忆版只需把这一段换掉）。
     *   · **Boss 在场**（一手判据 = `FUN_00449f20`：`bosses[0..7]` 任一非空；道中 Boss 与关底 Boss
     *     走同一个 `setBoss(0)` ⇒ 两者一致）⇒ 引擎每帧写 `UFO.int_vars[0] = 999`；ECL **轮询**
     *     （首轮 ≈301 帧）看到就 `movePosTime(300,1,…,−64)` 飞走 + `delete()` ⇒ **不发奖**
     *     （`ECL delete()` 不经过 `Enemy::die`，一手有两条独立的负向证明）。
     *     ⇒ 本卡按「活了 `UFO_BOSS_LEAVE_FRAMES` 帧且 Boss 在场 ⇒ **静默撤退、不发奖**」实现
     *     （延迟取一手首轮轮询帧数；`UFO_FLAGS_DELETE_NOW` 与 ECL `delete()` 同效：不跑死亡处理）。 */
    if (ufo_present(&s->u)) {
        uint8_t *be = ufo_enemy_by_id(s->u.ufo_id);
        if (CE_GUI_MSG_ACTIVE()) {
            /* ★★ 2026-09-23（owner 实跑第 9 条）：「**进入对话后，场上的 token 应当不再反弹，直接飞走**」
             *   —— 在自爆之前先把场上的令牌切进"离场态"（不再反弹 + 速度 ×2；纯逻辑层的
             *   `ufo_tok_motion_leave()`），它们会在随后几秒里自己飞出外框被回收
             *   （不主动删：一手的回收点只有"出界即 state 0"这一条）。 */
            ufo_tokens_leave(s);
            /* 对话 ⇒ 原地自爆 = 击破开奖路（一手）。位置 = UFO 当前坐标（一手 `0x44af25` 读 final_pos）。 */
            if (!s->dialogue_break_logged) {
                s->dialogue_break_logged = 1;
                ce_log("ufo: DIALOGUE active - the one-hand behaviour is a self-destruct (= the BREAK "
                       "path, full reward), not a silent fly-away (th12:0x44a3ab sets life.is_spell bit 1 "
                       "-> EnemyData::on_tick 0x413f5d -> Enemy::die -> 0x44af00)");
            }
            if (be) { s->last_x = *(float *)(be + CE_ENEMY_POS_X); s->last_y = *(float *)(be + CE_ENEMY_POS_Y); }
            s->pending_break = 1;                      /* 帧末按击破兑现（不在本处建 VM / 开奖）*/
        } else if (ufo_boss_alive() && s->u.frames >= UFO_BOSS_LEAVE_FRAMES && be) {
            /* Boss 在场 ⇒ 静默撤退（一手：ECL 飞走 + delete，**不发奖**）。 */
            if (!s->boss_leave_logged) {
                s->boss_leave_logged = 1;
                ce_log("ufo: a BOSS is on the field - silent withdrawal after %d frames (one-hand: the "
                       "engine writes int_vars[0]=999 and the ECL flies off + delete()s, no reward)",
                       UFO_BOSS_LEAVE_FRAMES);
            }
            ufo_teardown(s, "boss on field (silent leave, no reward)");
            ++s->ufos_vanished;
            *(uint32_t *)(be + CE_ENEMY_FLAGS) |= UFO_FLAGS_DELETE_NOW;
        }
    }
    /* 伪磁场（只在 UFO 在场且过了 60 帧暂停时扫池）*/
    if (ufo_can_absorb(&s->u)) ufo_absorb_tick(s);
    ufo_tokens_tick(s);
    ufo_drops_tick(s);                                 /* ★ 掉落物实体（压缩包 / 整颗 Bomb）*/
    /* 最后再叫一次召唤：覆盖「组早就齐了、但这一刻没有新令牌进来」的两条路 ——
     * ① 上一次召唤被「场上已有 UFO」挡下，现在那个 UFO 已经走了；
     * ② 组是在**死亡事件**里落成的（那条路只在 `on_tick_2` 里落槽，落完当下没有召唤机会）。
     * `ufo_try_summon` 自己三门全查（成组 / 单例 / 建得出来）⇒ 常态下这只是两次比较。 */
    ufo_try_summon(s);
    /* ★★ 实体上的两件 UI：每帧跟随（位置 = 敌机坐标；环角 = `fill × 2π`；条宽 = HP 千分比 × 32）。
     * 放在**召唤之后**：这一帧刚召唤出来的本体与两件 UI 由 `ufo_gauge_spawn` 当场摆好，
     * 这里再跟一次是幂等的；而「敌机被静默撤掉 / 击破」那一帧由 `ufo_gauge_follow` 自己收干净。 */
    ufo_gauge_follow(s);
    /* ★ 2026-09-23（owner 第 11 条）：UFO 在场期间的**持续** `se_ufoalert`（手动循环，见它的注）。
     * 放在最后：UFO 这一帧走掉的话 `ufo_present()` 已经是假 ⇒ 不再补下一声。 */
    ufo_alert_loop_tick(s);
}

/* 帧末统一判定（对齐 109 的教训：`DEATH` §4.1「回调里只登记、帧末动手」）。
 * ⚠ **顺序是刻意的**：先 ②b 建令牌（可能要**建** VM）与 ② 击破开奖，最后 ③ 存亡差分。
 *   反过来的话，① 刚建出来的 UFO 会被 ③ 立刻判成「不在了」—— 它这一帧还没挂上链表尾呢。
 * ★★ 2026-09-12 换池：原来的 ① 段（「死亡栈里登记的令牌颜色」出队 → 落槽 → 可能召唤）**整段删除** ——
 *   那是「令牌被子弹打掉也算收」的兑现点，令牌换成自建 VM 之后环里永远不会有人放东西。 */
static int on_tick_2(ce_card_t *c)
{
    ufo_card_t *s = ce_state(c, ufo_card_t);
    if (!s) return 0;

    if (s->pending_break) {                            /* ② 击破（死亡栈里只记了位置）*/
        s->pending_break = 0;
        ufo_break(s);
    }
    ufo_panel_tick(s);                                 /* ★ #22：吸收数记录面板（变了就弹；owner 已要求关掉）*/
    /* ★ #20 的 ×N 弹字是 **ASCII**（2026-09-23 重做）⇒ 只能在 draw 相位画 ⇒ 它的 tick 在 `on_draw` 里 */
    /* ②b ★ 令牌落地（死亡栈里只登记了落点；这一刻已经离开死亡栈 ⇒ 可以安全建 VM）。
     *   一手：令牌是「敌人掉落表」的产物（`dropExtra`）⇒ 触发点 = 那一只敌人的死亡位置。
     *   ⚠ 换池后这一步建的是**我们自己的 VM**（不是敌人）—— 「不在死亡栈上建」这条纪律不变。 */
    if (s->pend_drop) {
        int n = s->pend_drop_n ? (int)s->pend_drop_n : 1;
        int32_t col = s->pend_drop_color;
        int spawned = 0;
        s->pend_drop = 0;
        s->pend_drop_n = 0;
        s->pend_drop_color = 0;
        /* ★★ 2026-09-24（owner「得改谁能飞碟掉落」）：宿主表那条路一次可能落**多枚**
         * （一手：`dropExtra(10,3)` 这种一台敌人一次掉 2-3 枚），并且**颜色按关固定**
         * （一手 type 10/11/12 = 红/蓝/绿，写死在敌人的掉落表里）。
         * 旧的"任意带掉落表的小怪"那条路保持原样（一枚、随机/变色）—— 见 `UFO_TOKEN_DROP_LEGACY`。 */
        while (n-- > 0) {
            int32_t c = col;                    /* 恒为 `UFO_COLOR_NONE` ⇒ 随机起始色 + 会变色 */
            int ok = ufo_token_color_is_field(c)
                         ? ufo_tok_spawn_ex(s, s->pend_drop_x, s->pend_drop_y, c, 0, 0)
                         : ufo_tok_spawn(s, s->pend_drop_x, s->pend_drop_y);
            if (!ok) break;
            ++spawned;
        }
        if (spawned)
            s->next_token_at = s->play_frame + UFO_TOKEN_SPAWN_INTERVAL;
        else
            s->next_token_at = s->play_frame + UFO_TOKEN_SPAWN_RETRY;   /* 失败别每帧重试 */
    }
    /* ③ 本体存亡差分（飞离 ⇒ 收尾，**不发奖**：一手 ECL `delete()` 不经过 `Enemy__die`）。
     * ⚠ 能力管理器 tick（优先级 `0x16`）**早于**敌人 sweep（`0x1b`）⇒ 被打死的那一帧这里
     *    还可能看到它活着；但 ② 已经把 `ufo_id` 清 0 ⇒ 击破不会被这一格误判成飞走。 */
    if (ufo_present(&s->u) && !ufo_enemy_by_id(s->u.ufo_id)) {
        ++s->ufos_vanished;
        ufo_teardown(s, "vanished (ECL delete - flew away / stage clear) - no reward, per TH12 UFO:238-239");
    }
    /* ④ 场地腾出来了：如果槽里还压着一个成好组的组合（它一直等不到下一次令牌入槽就会干等几百帧），
     * 在这里兑现。`ufo_try_summon` 三门自检 ⇒ 常态下两次比较。 */
    ufo_try_summon(s);
    return 0;
}

/* +0x44：引擎建好本卡的 HUD 图标时经 `+0x44` 交来图标根 VM 的 anm id（图标重建时会再交一次）。
 * ★ 第一版 HUD **不用**图标根 VM（三格槽 / 血条 / 读数都走自己起的 UI VM，见 HUD 段）——
 *   所以这里只记一条日志（保留这个钩子：以后要在**卡自己的图标**上叠数字时，
 *   `ce_hud_digit_attach(&d, icon_id, 值, "ufo112")` 就在这里接）。 */
static int on_hud_anm(ce_card_t *c, uint32_t icon_id)
{
    (void)c;
    ce_log("ufo: on_hud_anm(icon %u) - the v1 HUD does not use the card icon VM "
           "(its own UI VMs sit on the left-bottom shelf slot %u)", icon_id, UFO_ID_NUM);
    return 0;
}

/* ★ 菜单门（用户裁决 ④ 的硬要求；先例 `animal.c` / `lolk.c` / `signal.c`）：
 * `on_draw`（虚表 `+0x48`）在**主菜单 / 标题**里也会被调，而 `on_tick_2`（`+0x2c`）不会
 * ⇒ 不设门会在菜单上留一整套 HUD，而且 `ufo_hud_ensure()` 会在菜单里反复重建那 5 个 VM。
 * 门关着时把 VM 收掉；**HUD 槽位不动**（槽位的账归 `on_load` / `on_delete`）。
 * ★ #9 起这里还负责画**击破计时器**：一手它是 `Ufo::on_draw_16` 里的 `AsciiManager__printf`
 *   （`th12:0x44a6e3`），而本仓的 ASCII 原语（`ce_hud_text*`）**只允许在 `on_draw` 里调**
 *   （`SDK.md` §14）⇒ 位置与闸门天然对上。 */
static int on_draw(ce_card_t *c)
{
    ufo_card_t *s = ce_state(c, ufo_card_t);
    if (!s) return 0;
    if (!CE_PLAYER() || !CE_GAME_THREAD()) { ufo_hud_kill(); s_timer_logged = 0; return 0; }
    ufo_timer_draw(s);                                 /* 一手 th12:0x44a6e3-0x44a7e8（与 HUD 块认领无关）*/
    ufo_xn_tick();                                     /* ★ #20：×N 弹字（ASCII ⇒ 必须在 draw 相位，见它的注）*/
    ufo_drops_draw_follow();                           /* ★ 满 B 的卡面 VM 跟随**原生道具**（画之前写，不晚帧）*/
    ufo_hud_draw(s);
    return 0;
}

static void on_run_reset(ce_card_t *c)
{
    ufo_card_t *s = ce_state(c, ufo_card_t);
    if (s && (s->spawned_tokens || s->ufos_spawned))
        ce_log("ufo: run reset - %u tokens spawned (%u taken / %u lost / %u dropped by UFO / "
               "%u slot-dropped / %d pool-overflow), %u UFOs (%u broken / %u vanished / %u failed), "
               "%u items pulled, %u rewards, %u bursts",
               s->spawned_tokens, s->taken_tokens, s->lost_tokens, s->dropped_by_ufo, s->dropped_tokens,
               s_tokpool.dropped, s->ufos_spawned, s->ufos_broken, s->ufos_vanished, s->failed_summons,
               s->items_pulled, s->rewards_spit, s->bursts);
    ufo_se_tally("run reset");                         /* ★ se 诊断：本关「哪一枚响了几次」*/
    ufo_hud_kill();                                    /* ★ 局末收 HUD VM（长持续效果铁律）*/
    if (s) ufo_withdraw_all(s, "run reset");
    /* ★ 2026-09-23：**一整局重来** ⇒ 把「上一个见过的关卡号」也清掉，让紧接着的 `on_ctor` 走
     *   **全清**那一支（否则"从第 3 关退到标题、再开新局第 1 关"会因为关卡号变了而被当成换关，
     *   把上一局的令牌带到新局里去）。*/
    s_stage_seen = UFO_STAGE_SEEN_NONE;
    on_ctor(c);
    /* ★ 2026-09-18：局末把「已认领」的闸复位 ⇒ 下一局的每帧补认领会重新登记一次
     *   （槽位本身的账归关卡纪元；这里只是让 `ufo_hud_ensure_registered()` 不再被自己的闸挡住）。*/
    ufo_hud_claim_forget();
}

/* ★ 删卡（METHOD 铁律 / spec §7）：商店里 AbilityManager 冻着、tick 回调全停 ⇒ 只有这里收得到。
 * ★★ 换池之后这条路的**分工变了**（说清楚，免得下一个人以为漏了）：
 *   ① **令牌 + 五边形框** —— **自建 VM**，句柄在**文件静态区**（`s_tokpool` / `s_penta_id`）
 *      ⇒ `ufo_withdraw_all()` → `ufo_tok_pool_clear_all()` 直接 `ce_anm_delete` 收干净。
 *      ⚠ 这是**最关键**的一条：静态区的句柄不会被 SDK 释放私有状态时清掉，只能靠这里显式收。
 *   ② **UFO 本体** —— 仍然是**真敌人** ⇒ 这里**不能**析构它（UAF：可能正在别人的链表遍历里，
 *      `DEATH` §2.5）⇒ 跟 108 一样走「置 `flags_lo` 位 25」的静默撤退，由引擎同帧 sweep。
 *   ③ **HUD 二件**（用户裁决 ④ 的必答项，别漏）：
 *      `ufo_hud_kill()` —— 3 个格子矩形 + 血条底 / 填充 + 那行读数的（最多 10 个）字形 VM；
 *      `ce_hud_slot_release()` —— 退出左下角货架的占位（`CE_CARD` 的 opdelete 里
 *      `ce_layer_drop_card()` 也会调一次，这里显式写出来是为了让「谁退了槽」一眼可查，
 *      幂等 ⇒ 调两次无害，与 `signal.c` 同款）。
 * ⚠ 换池后「删卡那一刻只有 `on_delete` 跑得到」这条**更容易满足**了：令牌再也没有「要等下一帧才消失」
 *   的东西（以前真敌人壳还得靠引擎 sweep 析构，现在 VM 直接标删、AnmManager 当帧就清）。
 *
 * ★★ 2026-09-18 **修了一个形状缺陷**（`CARD-RESOURCE-INTERACTION-AUDIT.md` §… 标为「已验证必错（代码形状），
 *   可达性未证」）：原来第一句是 `if (!s) return;`，而本卡**所有**长持续资源（令牌池 / 五边形框 /
 *   吸收环 / 血量条 / HUD 那一组）的句柄都在**文件静态区**，唯一收尾路径就是这一段
 *   ⇒ `ce_state()` 拿不到（私有块被提前释放 / 未分配）时，这些 VM **永久留场**。
 *   现在把「与 `s` 无关的静态资源」提到 `if (!s)` **之前**无条件收；需要 `s` 的那两件
 *   （本体跟随 VM 的句柄在卡状态里、真敌机的 id 也在）留在后面。 */
static void on_delete(ce_card_t *c)
{
    ufo_card_t *s = ce_state(c, ufo_card_t);

    /* ---- ① 与 `s` 无关的静态资源：**无条件**收（不受下面那个早退影响）-------------------
     * 顺序照 `ufo_withdraw_all()` 的原顺序（实体上的两件 UI → 令牌池 + 五边形框 → HUD VM → 槽位），
     * 三支都是幂等的 ⇒ 下面 `ufo_withdraw_all()` 再调一次是无操作。 */
    ufo_gauge_kill();                                  /* 吸收环 + 血量条（实体上的两件 UI）*/
    ufo_tok_pool_clear_all();                          /* 令牌 VM + 五边形框（只看句柄）*/
    ufo_hud_kill();                                    /* 3 格 + 血条底/填充 + 一行读数 */
    ce_hud_slot_release(UFO_ID_NUM);                   /* 退出左下角货架的占位（幂等）*/
    ufo_hud_claim_forget();                            /* 认领闸复位（卡若中途回来要能重认领）*/

    if (!s) {
        ce_log("ufo: card deleted - private state unavailable; the static VM pools / HUD were still "
               "released unconditionally (METHOD 长持续效果铁律)");
        ufo_se_tally("card deleted");
        return;
    }
    /* ---- ② 需要 `s` 的：本体跟随 VM 的句柄 + 真敌机的静默撤退 ---------------------------- */
    ce_log("ufo: card deleted - %u tokens spawned (%u taken / %u lost / %u dropped by UFO / "
           "%u slot-dropped / %d pool-overflow), %u UFOs summoned (%u broken / %u failed), "
           "%u items pulled, %u rewards",
           s->spawned_tokens, s->taken_tokens, s->lost_tokens, s->dropped_by_ufo, s->dropped_tokens,
           s_tokpool.dropped, s->ufos_spawned, s->ufos_broken, s->failed_summons, s->items_pulled,
           s->rewards_spit);
    ufo_se_tally("card deleted");
    ufo_withdraw_all(s, NULL);                         /* 私有状态随后由 SDK 释放（`ce_state_free`）*/
}

CE_CARD(112,
        .on_load = on_load,
        .on_stage_start = on_stage_start,
        .on_item_frame = on_item_frame,
        .on_tick_2 = on_tick_2,
        .on_hud_anm = on_hud_anm,
        .on_draw = on_draw,
        .on_enemy_death = on_enemy_death,
        .on_run_reset = on_run_reset,
        .on_delete = on_delete);

/* ============================== 编译期对账 ============================== */
_Static_assert(sizeof(ufo_card_t) <= CE_STATE_BYTES - CE_STATE_RESERVED,
               "ufo card private state exceeds the SDK budget");
/* ECL 侧那个隐形用的数**必须是负数**（= `anmSetSprite` 的拆槽分支，一手 `th18:0x436bf6`）：
 * 写成 >= 0 就又变成「让 effect.anm 的某个脚本去画」了 —— 那正是本轮要修掉的毛病。
 * ⚠ 这条断言只守 C 侧那个同名常量；真正的来源是 `assets/ecl/ufo.ecl.txt` 里那 8 个数（手工同步）。 */
_Static_assert(UFO_ECL_HIDDEN_BODY_SCRIPT < 0,
               "the ECL body slot must be torn down (S < 0), not pointed at some script");
/* 纯逻辑层自己钉了一遍 death kind 常量（它不 include sdk/public.h）⇒ 在这里强制对账。 */
_Static_assert(UFO_EDEATH_KILLED == CE_EDEATH_KILLED, "death kind table drifted");
_Static_assert(UFO_EDEATH_BOMB_KILL == CE_EDEATH_BOMB_KILL, "death kind table drifted");
_Static_assert(UFO_EDEATH_ECL_DIE == CE_EDEATH_ECL_DIE, "death kind table drifted");
_Static_assert(UFO_EDEATH_OTHER == CE_EDEATH_OTHER, "death kind table drifted");
_Static_assert(UFO_EDEATH_KILL_ALL == CE_EDEATH_KILL_ALL, "death kind table drifted");
/* 令牌 / 本体的 life 硬约束（`spawn+0x14 >= 1000` ⇒ `0x42d8f6` 置锁定黑名单位）。
 * ⚠ 只断言**输入常量**是不够的（把 `ufo_clamp_spawn_life` 改成返回 >= 1000 时它们照样过）——
 *   所以真正守不变量的那条是 **钳制边界本身**：`UFO_LIFE_MAX_ALLOWED < UFO_LIFE_LIMIT`。
 *   再加上「`ufo_clamp_spawn_life` 的返回值永远 < 1000」这条**运行期**检查（主机单测里覆盖，
 *   见 `tests/test_ufo_core.c` 的 `ufo_clamp_spawn_life(UFO_LIFE_LIMIT) == UFO_LIFE_MAX_ALLOWED`）。
 * ★ 2026-09-12 换池：原来这里还有一条「令牌 life < 1000」的断言 ——
 *   **连同那个常量一起删了**：令牌不是敌人 ⇒ 没有 `spawn.life` 这回事
 *   （以前那个常量 = 1 的后果正是「任何一发子弹都能收走令牌」，即用户要修掉的那条）。 */
_Static_assert(UFO_LIFE_MAX_ALLOWED < UFO_LIFE_LIMIT, "the clamp ceiling itself must stay below 1000");
/* ★★ P4 + P2 #6（2026-09-21）：HP **不再有"缩放上界"这个量** ——
 *   真血量 = 一手表原值（可 > 999；spawn 之后回写 `enemy+0x6220/24/28`，见 `ufo_body_apply_real_hp`）；
 *   必须 < 1000 的只是**写进 `spawn+0x14` 的那个值**（= `ufo_clamp_spawn_life()` 的输出）⇒
 *   守它的是下面的 `UFO_CLAMP_OUTPUT_MAX` 断言 + 单测里 `ufo_clamp_spawn_life(1600/2500) == 999`。 */
_Static_assert(UFO_HP_ONEHAND_SCALE_PERMILLE > 0 && UFO_HP_MAX_ONEHAND > 0 && UFO_HP_TABLE_N == 5,
               "the one-hand HP table is {{1600,1900,2100,2300,2500}} - five tiers, positive");
/* ⚠ 下面这条**单独**不够（两边都是常量，钳制函数改了它也不会红）⇒ 真正兜底的是两张网：
 *   ① `UFO_CLAMP_OUTPUT_MAX`（= `UFO_LIFE_MAX_ALLOWED`）**就是** `ufo_clamp_spawn_life` 的返回上界
 *      这条约定（纯逻辑层那个 `static inline` 尾注）；要机械保证就把钳制也搬成 `static inline` 并
 *      在头里对**它的返回值**做 `_Static_assert` —— 那样又要让 core 依赖 `UFO_LIFE_LIMIT`，得不偿失。
 *   ② 主机单测 `tests/test_ufo_core.c` 的 `ufo_clamp_spawn_life(1000/1600/2500) == 999` 那一组
 *      （钳制函数返回 ≥1000 时会红）。⇒ 改钳制函数必须动单测，这条链是有人的。 */
_Static_assert(UFO_CLAMP_OUTPUT_MAX < UFO_LIFE_LIMIT, "clamped spawn life must stay below 1000");
/* ★ A8：彩虹令牌的换色计时器是 **uint8**（为了不撑大 `ufo_tok_t`）⇒ 150 必须装得下，
 *   而且「到 150 就换色并清 0」这条不变量必须由 `ufo_tok_rainbow_tick` 自己守住
 *   （推进写成「先判 ≥150 换色、再只在 <150 时 ++」）。 */
_Static_assert(UFO_TOK_CYCLE_TICKS <= 255, "the 150-frame recolour tick must fit in a uint8_t");
_Static_assert(UFO_TOK_BLINK_AT < UFO_TOK_CYCLE_TICKS,
               "the 90-frame blink must start before the 150-frame recolour (one-hand order)");
_Static_assert(UFO_TOK_RB_STATES == 3,
               "the rainbow script table must stay 3 entries (one-hand type 13/14/15 -> 178/179/180)");
/* ★★ 2026-09-12 换池：令牌池的**两条**结构不变量（池是文件静态定长数组，越界就是内存写坏）。
 *   ① 池容量必须 >= 「同时活几个」的上限（否则 `UFO_TOKEN_MAX` 这条手感上限根本到不了）；
 *   ② 五边形框的句柄表与池**同下标**（`ufo_tok_remove` 的 swap-remove 要一起搬）⇒ 两张表必须同长。
 *   ⚠ 纯逻辑层的 `ufo_token_pool_room()` 就是①的判据入口（分配前先问它，越界不可达）。 */
_Static_assert(UFO_TOKEN_CAP >= UFO_TOKEN_MAX,
               "the token pool must be able to hold UFO_TOKEN_MAX live tokens");
_Static_assert(UFO_TOKEN_CAP == UFO_TOKEN_MAX + UFO_TOKEN_POOL_SLACK,
               "UFO_TOKEN_CAP must come from the pure-logic pool room rule (single source of truth)");

/* ============================== ★ 占位 / TODO 清单 ==============================
 * 每条都是「需要 ECL / 资产 / 用户决策」，取值一律是**不做**这个语义：
 *   §6 步 3 —— ECL（**剩下的只有本体**）
 *     1. ★★ **令牌已不需要 ECL**（2026-09-12 换池）：它不再是真敌人、没有子程序、没有惰性壳。
 *        当年那条占位 ECL 名依赖（`"NoEnemyDebug"`）**连同整个常量删掉了**
 *        （依据见文件头「跟随 VM」那一段 + `AUDIT.md` §BO-11 的 P3）。
 *        ⚠ 一手的活动框 `moveLimit(0,128,280,128)` 现在**没有意义**：令牌的位置由我们自己的
 *        `ufo_tok_motion_t` 算（一手 state 6 的直线飞行 + 反弹，`ufo_tok_motion_step`）——
 *        这正是「一手令牌是道具、运动在 C 侧」的忠实对应物。
 *     2. ★ **UFO 本体四色子程序 —— ✅ 已实装**（真名 = `UFO_*_TTL`，卡侧宏 `UFO_ECL_BODY_*`，
 *        见上面那个常量块；ECL 侧 `assets/ecl/ufo.ecl.txt`，一手名字表 `th12:0x4b33e0`）。
 *        它自己会飘（`moveLimit` ×2）、**420 帧起飞走**（`_TTL` 变体自带计时、到点 `delete()`）、
 *        召唤瞬间自己消弹（`UFO_EtBreak`）。⇒ spec §9 #10b「飞走由谁驱动」**已解**（走 B 组 `_TTL`；
 *        A 组「原样移植 + 引擎写 `$I0`」那条路仍在等 `int_vars` 偏移进 `engine.h`）。
 *        ⚠ **仍未做**的是**击破时**那一圈大消弹（一手 = 另生成一只 `UFO_EtBreak2` 敌机，半径 16→640）——
 *        见文件尾 §占位清单与 `NEXT.md` §1z-2。
 *     3. `default.ecl` 的 thcrap 文件映射 —— ✅ **已做**（`native/release.py:33`，落 patch-step3 常开那个）。
 *    3b. ★ 本体的**跟随 VM** —— ✅ **本轮实装**（`ufo_body_vm_spawn/follow/kill`）：
 *        ECL 侧只能当隐形载体（`anmSetSprite(0, -1)` = 拆槽），画面由本文件起 `ability.anm` 164..167 画。
 *   §5 资产（8 件里 4 件有主了；`assets/ability/ufo/` 现在是 112 的切图目录）
 *     4. 三格令牌槽 / 收集闪光（`front.anm` 80/81）—— **第一版已经用 `drawRect` 拼出来了**
 *        （脚本 104，层 25，见 HUD 段）；**原生切图**仍然待做（这一版不冒充复刻）
 *     5. 「UFO 出现」横幅 / 大横幅 4 色染色（79/82）—— 未做
 *     6. ★ **UFO 本体 4 色贴图 —— ✅ 本轮实装**（`ability.anm` script 164..167 + 本文件的跟随 VM；
 *        原生来源 `enemy.anm` script135..138 / `enemy/ufo_anim.png`，切图器 `make_ufo_art.py`）。
 *        仍然**没做**的是：光环 / 击破特效 / 收集爆环（`bullet.anm` 207/208/211）—— 那三件还是空
 *     7. 血条外框 / 填充（`front.anm` 134/136）—— 第一版血条是 `drawRect` 拼的；**原生血条**不会出
 *        （本卡刻意把 life 压到 < 1000，spec §4 的取舍）
 *     8. ★ **令牌贴图 6 态 —— ✅ 实装**（`ability.anm` script 168..173 + `ufo_tok_vm_spawn/follow`；
 *        原生来源 `bullet.anm` 175..180 / `etama8.png`，切图器 `make_ufo_token_art.py`）。
 *        ★★ 2026-09-12 换池：`ufo_tok_vm_spawn/follow` 现在起的是**令牌本体那只 VM**
 *        （一只 token = 一个 VM；以前是「惰性壳 + 跟随 VM」两份）。
 *        ★ 2026-09-12（A8）：**彩虹令牌的每 150 帧换色 + 三段 `interruptLabel` 已实装** ——
 *        彩虹改用 `ability.anm` **215..217**（全文版，`make_ufo_token_art.py` 生成；号码见 `anm_ids.h`）+
 *        `ufo_tok_rainbow_tick` 按一手发 3/2/7 号中断（发送者 / 触发条件 / 地址见文件头那个常量块）。
 *        ⚠ 由此 171..173（被截断的那三条彩虹）**现在没有调用者**了 ⇒ 留着没删（等统一压号时一起处理）。
 *        ⚠ 仍然**没做**的是：**金令牌与蓝令牌长得一模一样**（金借蓝那一套贴图，用户裁决 ①「等 P 图」）
 *     9. ★ 2026-09-18 更新：`UFO_SE_ITEM_BREAK` **已由 0x11 改成 0x0d**（原占位借的是
 *        `0x11 = se_extend.wav` ⇒ 每掉一次赏金道具响一声 Extend 音，用户实跑听成"一直在响的怪 Extend 音"，
 *        `CARD-118-12IN1-REPORT.md` B9；见 `TUNING.md` §2.0f）。**仍是 ❓自定占位**（TH12 的专用音未逆到）
 *    8b. ⚠ A8 两处**故意没搬**（一手里有、本卡不做，别当成漏了）：
 *        ① 一手在 60 px 圈内会**连道具寿命也一起冻结**（`th12:0x4262f0` 的第二个 timer = 道具 1200 帧
 *           的寿命计时器）——我们的 `ttl`（`UFO_TOKEN_TTL_FRAMES`）是自定值、语义不同 ⇒ 不跟着冻
 *           （跟了就等于顺手换掉一个与本题无关的行为）。
 *        ② 一手换色时会播一个音（`th12:0x4264dd` → se id **`0x35`**）——那是 **TH12** 的 id，
 *           本仓只有 TH18 的音效表（`engine/_shared/th18-sound-table.md`）⇒ **不播**：宁可没声音，
 *           也不借一个 TH18 的 id 冒充（要补得先查 TH12 `0x4aea50` 那张 se 名表对到 TH18 的哪一条）。
 *           ★★ 2026-09-20（P1 #th12-2）**这条已经做掉了**：TH12 的 `0x4aea50` 名表与全 60 行 cfg 已逆出，
 *           `0x35` → `se_changeitem.wav`（与 TH18 同名文件 **md5 相同**）⇒ 现在播 TH18 的 **`0x4e`**
 *           （`UFO_SE_TOKEN_RECOLOR`，在令牌自己的 x）。上面那个"不播"的决定作废。
 *    8c. ❓ A8 待实跑：① 三段中断的观感（进圈换固定色慢摆 / 90 帧起快闪 / 出圈回主循环）；
 *        ② 60 px 圈读的是**令牌自己的坐标**（`t->motion.x/y` = 世界层 VM 的实体坐标；换池前读的是
 *           敌机 `CE_ENEMY_POS_X/Y`，两边同属 ECL 640 空间 ⇒ 数值逐条不变，但「60」的**量纲**
 *           仍**没验过**（1 单位 = 2 表面 px ⇒ 约 120 px）；
 *        ③ 换色走「删旧 VM + 起新 VM」（TH12 是同一个 VM 原地换脚本）⇒ 理论上可能有一帧叠影/空档，
 *           实跑若看到换色那一帧闪一下，就是它（要消掉得给 SDK 加「重指脚本」）。
 *    8d. 脚本号对账（A8）：彩虹全文版由 `make_ufo_token_art.py` 的 `SCRIPT_FULL_PORTS` 生成，
 *        落地当天用的号 = `ability.anm` **215..217**（`CE_ANM_ABILITY_SCRIPT_UFO_TOKEN_RAINBOW_{RED,BLUE,GREEN}`；
 *        ★ 2026-09-18 订正：原写 219..221 是重编号前的旧号）。
 *        ⚠ 生成器要求脚本号从 68 起**连续**（`build_ability.py` 的硬门）⇒ 并发开发期中间有洞时
 *        `make anm` 会直接 raise（这跟本卡无关，是抢号协议）；收尾压号 = `assets/renumber_scripts.py --apply`：
 *        它按文件名 + 首行改号、**不会**改生成器那张表 ⇒ 压完**要回头把 `SCRIPT_FULL_PORTS` 的三个号改掉**
 *        （C 侧不受影响：只认宏名）。
 *    9b. ★★ **五边形框 —— ✅ 2026-09-12（A7）本轮实装**（用户 2026-09-12 裁决 ③；`TUNING.md` 资产缺口 A7）：
 *        框 = `ability.anm` **script223**（= 原版 TH12 `bullet.anm` **script211** 的逐条移植：
 *        `sprite(-1); layer(12); blendMode(1); drawPolyBorder(1.0f, 5); angleVel(0,0,0.034906585f);`
 *        `scaleTime(10,4,20,20)` + 四个颜色标签 7/8/9/10），触发条件 / 颜色 / 层号 / 生命周期的一手
 *        全部写在文件头「★★ 五边形框」常量块里；卡侧驱动 = `ufo_pentagon_hint_update()`
 *        （算判据 + 建框 / 每帧跟随 / 条件不成立就收）。**没有用 `drawRect` 硬凑** ✓。
 *        ⚠ **这项资产没有贴图可切**：`drawPolyBorder` 是引擎程序画的多边形（`sprite(-1)`）⇒
 *        无 PNG、无 ORDER 片段（TH18 原生同款写法见 `effect.anm` script101 / `ability.anm` script35）。
 *        ⚠ 三处与原生的差别（常量块里逐条写了理由）：
 *        ① 框**每帧跟随**令牌坐标（原生建完就不动）—— ★ 换池后更必要了：令牌会飞会反弹；
 *        ② 金走 `ufo_color_norm()` 落蓝那一格；
 *        ③ ~~判据用卡自己的「彩虹 = 通配」模型~~ ⇒ ★ 2026-09-18「新增 1 得是按色」**已改成原生字面口径**：
 *        `ufo_pentagon_hint_update()` 现在传 `ufo_tok_display_color(t)`（变色令牌按当前显示态）。
 *        ⇒ 这一格**已收口**，那两条「有意偏离」只剩 ①②。
 *        ⚠ 脚本号 223 在本文件里是**字面量**（`UFO_PENTAGON_ANM_SCRIPT`）：`native/anm_ids.h` 是
 *        `make anm` 的生成物、本轮的写入范围不含它 ⇒ 先字面量 + 这条注释；`make anm` 跑过之后
 *        它会多出 `CE_ANM_ABILITY_SCRIPT_UFO_PENTAGON 223`（同号同名），届时换成那个宏即可。
 *   §9 待用户拍板（本实装取「不做」这一支，全部集中在 ufo_core.h 的对应常量注里）
 *    10. #1 金 UFO —— ★ **已按用户 2026-09-12 裁决 ① 落地**：金是合法色（`UFO_COLOR_GOLD` = 4），
 *        **全部行为与蓝逐项相同**（吸收链 / 吸满不吐 / 击破无额外 / HP / 飘行参数都照蓝），
 *        贴图先借**蓝那一件**（`ufo_color_art_index(GOLD) == 1`、`UFO_ECL_BODY_BLUE`，等 P 图）。
 *        ⏳ **金算不算凑组里的独立一色** = `UFO_GOLD_COUNTS_AS_BLUE`（❓ 自定，默认 1 = 金 ≡ 蓝；
 *        一手 `th12:0x422e80` 的颜色域里没有金 ⇒ 这一格**没有一手**，只能等用户拍板）
 *    11. #2「两同一异 ⇒ 丢最旧」—— ★ **已按用户 2026-09-12 裁决 ② 照抄一手 KEEP2**
 *        （`th12:0x422e80` 的 `cnt1c = 4` 那条路）：第三格填上时不成组就**当场丢最旧、保留最近两个**，
 *        丢完**重判合成**；槽里不再存在「满 3 格却不成组」的稳态。完整规则与 6 种排列的期望
 *        写在 `ufo_group_put`（`ufo_core.h`）+ 单测。★ 2026-09-12 订正：KEEP2 的**三条**条件抄全之后
 *        （`0x422ed4`/`0x422ed6` 的 `slots[0]==color && slots[1]!=color` 也算），`{A,B,A}` 与
 *        另外两种排列一样走 KEEP2 ⇒ **「与字面一手有一格差异」这条作废**（行为本来就没变）。
 *    12. #3 蓝 UFO 吸满不吐 —— **复刻**（一手 switch 无 case 2）；★ 金**照蓝**（同样不吐）
 *    13. #4 HP 缩法 —— ★★ 2026-09-18 按用户裁决 P4 落地（「HP 一手确实是按火力」）；
 *        ★★★ **2026-09-21 改口径（P2 #6「感觉飞碟太脆了」）：缩放取消** ——
 *        一手表 `{1600,1900,2100,2300,2500}`（`th12:0x4b33f0`，索引 = `CURRENT_POWER / POWER_PER_LEVEL`）
 *        **原值当真血量**（`ufo_hp_for_power()`），spawn 时只写钳制值（≤999），敌机建出来后
 *        `ufo_body_apply_real_hp()` 回写 `enemy+0x6220/24/28`（一手：位 30 的黑名单闸只在 spawn
 *        那一刻按 `spawn+0x14` 置，`th18:0x42d8ed`–`0x42d8fc`；血量三格是 `0x42d815`–`0x42d82a`）。
 *        血条/HUD 的分母 = `s->u.hp_max`（= 真血量，**不在显示侧另算**）。
 *        🟡 唯一旋钮 = `UFO_HP_ONEHAND_SCALE_PERMILLE`（默认 1000 = 一手原值；TH18 每帧伤害上限
 *        比 TH12 大这一层**没有**一手 ⇒ 要缩放得 owner 拍，见 `TUNING.md` §2.0r）。
 *    14. #5 击破得分公式 —— **不编**：`UFO_BOUNTY_SCORE_PER_*` = 0 ⇒ 击破加 0 分。
 *        ★ 2026-09-21（P2 #5）：**碎片载体改成 `0x10`/`0x11`**（TH18 的卡道具 = 残碎卡 / 符卡碎片卡），
 *        原来的 4/6 是别作的遗留槽（TH18 道具分派里连 case 都没有）——见 `ufo_core.h` 的奖励段。
 *    15. #6 扩圈消弹（`0x429250` vs `0x429370`）—— **不选**，一次都没调（`ce_cancel_radius` 有现成面）
 *    16. #7 吸掉金钱的经济影响 —— 照吸（用户定案 ⑦），但把 `type 2` 吸走 = 每颗少 1 金钱
 *        ⇒ **记账在日志里**（`items_pulled` / `cnt_money`），⏳ 待用户看完再决定要不要改成别种道具
 *    17. #8 HUD 落点 / 认领 —— ★★ **2026-09-18 第三版：原版切图的三格令牌槽已接线**
 *        （脚本 302/303 ← `front.anm` script80/81；块 = **自带锚点**钉在原版位置 表面 (104, 952)，层 25）；
 *        ★ 认领历史：第一版认领**恒失败**（NULL out）+ 中盘拿卡不调 `on_load` ⇒ HUD 一次都没出现过
 *        （用户实跑 B4）⇒ §DO 修掉；第二版只生成资产没接线 ⇒ 第三版接线（用户实跑 ⑤）。详见 §DS。
 *        ⏳ 待实跑对的：槽的落点（与 110/111 块底擦 6 个单位）/ 闪烁相 / 中断 17 与闪光的时序
 *    17b. ★ HUD 的「整摞一起淡」（玩家贴近左下角，`ce_hud_near_corner()`）**没做** ——
 *        HUD-LAYOUT §4.5 要求多块一起淡，本版先不淡（形制未定，不自己发明淡出曲线）
 *    18. #9 令牌实体三处细节 —— ★★ **已按用户 2026-09-12 的拍板改掉**：令牌**不是实体了**，
 *        是**自建 VM 池**里的一只 VM（`s_tokpool`）⇒ 「自己就是实体 + 接触即入槽」这两条保留，
 *        「与本体共用惰性壳」那条**作废**（没有壳了）。依据：`AUDIT.md` §BO-11 的 P3 三条代价。
 *    19. #10 / #10b 离场信号通道 + 飞走由谁驱动 —— 卡侧只认「id 不在了」（可以工作，但**飞不起来**）
 *    20. #11 循环音停止 API —— 本卡没起循环音（`on_enemy_death` 里不播音）⇒ 这一条**目前不适用**；
 *        真 ECL 到位、加了循环音之后必须补
 *    21. #12 令牌掉落普查 —— ★ 2026-09-12 **订正**：触发点不再「按节流自己撒」，改成
 *        **小怪被击破时掉**（一手：令牌 = 敌人 ECL 掉落表的产物 `dropExtra(10..12,n)`，op 407
 *        handler `th12:0x4179cc` → `EnemyDrop__spawn_ex_drops` `th12:0x4121e0`）；「谁掉」用
 *        `CE_EDEATH_F_HAS_DROP + F_REAL`（有掉落表的真敌人）当代理，节流见 `UFO_TOKEN_SPAWN_*`。
 *        ⏳ 仍是 ❓ 自定：TH18 的敌人**没有**令牌掉落表（type 10..15 在 TH18 不是令牌）⇒ 「哪只掉、
 *        掉几个、什么颜色」三格没有一手可抄（一手只有「掉落表里写了就有」这个结构）。
 *        ★ 换池**没动这条**（来源与载体是两件事）—— 用户明确要求过。
 *    22. #13 `BOMB_KILL` 算不算击破 —— 取「算」（现在只剩 `ufo_body_kill_counts` 一处；
 *        ★ 换池删掉了 `ufo_token_kill_counts`，令牌没有死因这回事了）
 *   ★ 自定参数（spec 没有、一手也没有 ⇒ 具名常量 + ❓）
 *    23. ★ `UFO_TOKEN_MAX` = **3**（同时活几只；⚠ 原为 1，理由见常量注 —— 上限 1 时场上永远只有
 *        一个、很容易错过 ⇒ 那一局基本凑不齐）、`UFO_TOKEN_TTL_FRAMES` = 2400、
 *        `UFO_TOKEN_SPAWN_INTERVAL` = 300 / `UFO_TOKEN_SPAWN_RETRY` = 120 / `UFO_TOKEN_SPAWN_FIRST` = 180、
 *        ~~`UFO_TOKEN_PICK_DIST` = 20 px~~ ★ 2026-09-18 **P5 已落地**：半径**读自机字段**
 *        `player+0x4798c`（`CE_PLAYER_ITEM_COLLECT_R`，`th18:0x44624a` 用的就是它）= **本作自己的收点半径**。
 *        （一手 TH12 是**自机收点矩形**：`th12:0x426100` 四次比较读
 *        `player+0xc444..`，半尺寸来自玩家表 `0x4b31b4` = {60,70,60} ×0.5×0.5 = **15 单位**；
 *        我们的令牌不是道具、走不到那条通路 ⇒ 尚用自算距离；⏳ P5，**本次刻意没有改半径**）、
 *        `UFO_PLACEHOLDER_STAGE` = 1（填充率公式的关卡号）、
 *        `UFO_TOKEN_POOL_SLACK` = 2（池容量 = `UFO_TOKEN_MAX` + 它，判据在纯逻辑层 `ufo_token_pool_room`）。
 *        ★ 换池**删除**的两个自定常量：令牌 HP（令牌不是敌人，没有 HP 这回事）、
 *        死亡登记环容量（令牌不再有死亡事件 ⇒ 那整套登记环一起删）。
 *    24. 令牌颜色：★ 2026-09-18 **裁决 P2「随机吧」+ 新增 2** 之后 = **两个来源**：
 *        ① **小怪掉**的那条路 = **随机**取红/蓝/绿（我们自己的 LCG，种子见 `s_rng_seed`）；
 *        ② **UFO 掉**的那条路 = 一手掉落表指定（`ufo_break_token_state()`）—— 它掉的是**变色令牌**
 *           （`cycler = 1`，起始显示色 = 那个 state 对应的色）。
 *        ⚠ 一手的事实是「**颜色固定**：哪只敌人掉、掉什么颜色都写在敌人的 ECL 掉落表里，没有随机、
 *        也没有轮转」（`dropExtra(10/11/12,n)` 三种固定色；`13/14/15` = 彩虹令牌的三种起始态，
 *        由 UFO 击破时按色/`COLLECTED_UFOS[2]` 指定）⇒ 轮转表是 ❓ 自定的占位（TH18 无掉落表）。
 *        ⚠ 换池前这一条写的是「真 ECL 到位后应改成**实体携带**颜色 `$I0`」—— **现在没有实体了**
 *        ⇒ 那条路线作废；要改只能改成「按掉落的**那只敌人**选色」（需要一个 TH18 侧的映射表，同样没有一手）。
 *    29c. ★ 「跟随 VM」的**待实跑**（原三条，换池后第二条对**令牌**已经不适用、对**本体**仍适用）：
 *        ① 令牌层的层号 `UFO_TOKEN_ANM_LAYER` = 13（与原生道具同层的二手口径，没实测过；
 *           小了会被别的层压住、大了会压住子弹）；
 *        ② **本体** VM 的位置可能比敌机**晚一帧**（跟随读的是 `on_item_frame` 时刻的坐标，敌人 sweep
 *           在同一帧更晚的位次）—— 敌机本体是隐形的，肉眼看不到这一帧差，但「吸收判定」用的坐标
 *           与画面若差一帧，贴脸时可能有 1 px 级的观感错位；★ **令牌没有这一条**了（它的坐标就是
 *           我们自己的字段，同一帧写、同一帧画）；
 *        ③ 自愈重建（VM 被引擎在别处销毁时）会从头播一遍淡入（`alphaTime(60,0,255)`）⇒ 若实跑看到
 *           飞碟/令牌周期性闪一下，就是它。★ 换池后令牌自愈还有一个新观察点：自愈会**重建整个
 *           令牌 VM**（位置由 `ufo_tok_vm_spawn` 当场摆好，但脚本从第 0 帧重开 ⇒ 会重播一次
 *           `scaleTime` 之类）。
 *    29d. ★ **五边形框（A7）的待实跑**（三条，全是❓）：① 观感 —— 2°/帧自转 + 10 帧从 0 胀到 20
 *        （`drawPolyBorder(1.0f, 5)` 的第一个参 = 半径 1.0，乘 `scaleTime` 到 20；世界层 1 单位 = 2 表面 px
 *        ⇒ 成品半径约 40 表面 px）与令牌（32×32 画布）的大小关系是**照原生数值推的**，没在实机上看过；
 *        ② 层号 `UFO_PENTAGON_ANM_LAYER` = 12（原生原值）压在令牌的 13 下面 —— 「框在令牌后面」这个
 *        相对关系照抄了原版（原版：框 12 / 道具自己的 VM 23），但 **TH12↔TH18 的层轴是否逐档等价没实测**；
 *        ③ 「建完立刻发标签」这一步在 TH18 侧的**中断语义没实跑过**（判定依据是一手的装载器「跑一帧」
 *        + `th18:0x488be0` 写 pending interrupt；若实跑看到框**根本不出现**、或出现却一直不换色，
 *        第一个要查的就是这里 —— 退路是把标签改成**脚本自己的默认段**（不带标签也有颜色），但那样就
 *        偏离原生的「一个 VM + 四种标签」结构了）。
 *   🟡 待实跑
 *    25. 扫池成本（没有任何实测帧耗；口径见文件尾 §性能）
 *    26. 槽泄漏量（口径见文件尾 §槽泄漏）
 *    27. `ufo_drag_speed` 的步长（一手公式照抄，但 TH18 没有「飞向 UFO」状态 ⇒ 手感要重标）
 *    28. 围栏三件的边界（|x| >= 200 / y > 472 / 自机 30 px）—— 一手值已复核，但「引擎当帧删」的
 *        时序在实跑里没验过
 *    29. `on_item_frame` / `on_tick_2` 里建**令牌 VM**（`ufo_tok_spawn`）—— 这两条都不是死亡栈
 *        （前者是每游玩帧的门卫位次、后者是能力管理器 tick），但「断点里 `new`」这件事本身没有先例
 *        ⇒ AUDIT 待补。★ 换池后**新的**待验点：令牌那一只 VM 是**我们唯一**的画面载体
 *        （以前还有敌机壳兜着）⇒ 若 `ce_anm_spawn` 在某个时点返回 0，令牌就「看不见但仍然存在」，
 *        只能等 TTL 收掉（日志会打 `token VM spawn FAILED`）。
 *    29b. ★ **HUD 第三版**（★ 2026-09-18 更新：原版切图的三格令牌槽**已接线**；第一版的 `drawRect`
 *        占位（3 格 + 血条 + 一行读数）连同那几个 ❓ 常量**整段删掉**）。**待实跑首验**的：
 *        ① 槽的落点（脚本里写的是 `pos(296/338/380, 912)` = 640 空间 (148/169/190, 456)，
 *           已按「TH12 场地左边缘 32 → TH18 128」+96 换算；若实跑发现**与 110/111 的组合 UI 撞车**
 *           或位置不受待见，改生成器的 `X_SHIFT_640` 一个数重跑即可）；
 *        ② 尺寸 `scale ×2`（Δ4）与 Δ3 坐标 ×2 是否真的对上原版视觉大小；
 *        ③ 闪烁相（`play_frame % 16 == 1`，🟡 一手那个 timer 的来源没钉死）；
 *        ④ 中断 17（出现动画）与闪光 VM（脚本 303）在 TH18 的中断/自删时序。
 *        ⚠ 仍**没接**的原版件（贴图/脚本已备好或可切）：出现横幅 79（sprite67 96×16，`anchor(1,2)`，
 *        `pos(-96,467) → posTime(60,4,32,467)`）· 大横幅 82（front02.png 304×32，`type(2)`）·
 *        吸收数记录面板 304/305（`pos(528,288)` 在 TH18 会与**原生卡组图标列**（根 528、行 y
 *        248/313/378）撞车 ⇒ 位置要另定）。跳过的理由都写在生成器文件头与常量区。
 *   §槽泄漏（记账口径）
 *    30. 卡侧 `state = 0` 与引擎自己的出界删除（`0x445e38`）**逐条同构**，**都不把槽还回空闲表**
 *        ⇒ 泄漏量 = 本局吸掉的道具数（日志 `items_pulled`）。4696 槽（`CE_ITEM_SLOTS`）：
 *        按一手填充率 stage 1 要 34 颗 / 个 UFO，满 720 帧最多召唤几次也要 34×N 槽 ⇒
 *        需要跑一关看 `items_pulled` 与「道具不再生成」的临界点。spec §3.4 的 `ce_item_despawn()`
 *        仍未实装（SDK 侧），但**「四步清尾」卡侧自己写了** —— `ufo_item_relink()`（文件头 ④）
 *        照抄引擎那四步：`state = 0` → 删 anm VM（`0x488cf0`）→ anm id 写 0 → **freelist 摘挂**，
 *        只多两个 NULL 守卫。所以这里**不是**「卡侧不碰 freelist」；关键是那一步摘挂是
 *        **LRU 移动**（把该槽前插到 `normal_items` freelist 队头，槽仍在链表上、等整池轮一遍才复用），
 *        **不是把槽还回空闲表** ⇒ 「泄漏」要按「轮回一圈」读，不是永久丢失。写错 = 链表损坏，
 *        `PLAN` §4 未解 3 记的是**实跑未验**（不是没实装）。
 *
 * ============================== ★ 2026-09-12「逐条对账」这一轮的改动 ==============================
 * 全部来自 TH12 一手（`objdump -d local/th12.v1.00b/th12.exe`，地址都在注释里），逐条：
 *   31. **吸满只开一次奖**（`ufo_absorb_item`）：一手 `0x44acd8 fld1; fcom fill; test ah,0x41;
 *       jnp 0x44ae98` ⇒ fill 已经 >= 1.0 就整个收工（计数照加、fill 不加、不吐奖励）。
 *       旧版「满着期间每颗都吐」会让红 UFO 的**命碎片**无限刷 ⇒ 这是本轮修的**唯一一个漏洞**。
 *   32. **击破/吸满返还一枚令牌**（一手 `0x44af00` 的跳表 `0x44b590` + `0x44aca0` 的 `0x44ae03`）：
 *       红→道具 13、蓝→14、绿→15、彩虹→按 `COLLECTED_UFOS[2]`（`0x4b0c54`）；**每色恰好 1 枚**
 *       （「掉两个飞碟」不成立）。落地 = `ufo_break_token_state()` / `ufo_spit_token_state()`
 *       + `ufo_tok_spawn_ex()`（显式颜色/相位）。
 *   33. **令牌来源 = 小怪击破**（一手：敌人 ECL 掉落表 `dropExtra(10..12,n)`，op 407 handler
 *       `th12:0x4179cc`）⇒ 删掉「卡侧按 300 帧自己撒」，改成死亡事件登记落点（帧末建）。
 *   34. **令牌运动会飞、会反弹**（一手 `0x425c20` 的 state 6 全段）：初速 1.5（`0x4a4270`）、
 *       角度按奇偶取 π/4 / 3π/4（`0x4a4274`/`0x4a4278`）、四边反弹 ×(−1.0)（`0x4a4168`，
 *       边界 176 / −176 / 160 / 370）、1200 帧后不再反弹、出界即撤（208 / −208 / −16 / 464）。
 *       落地 = 纯逻辑 `ufo_tok_motion_init_at/step()`（有单测）+ 卡侧每帧 `ce_anm_set_pos`。
 *   35. **KEEP2 的第三条分支**（`0x422ed4`/`0x422ed6`）⇒ `{A,B,A}` 也走 KEEP2 ⇒ 我们与字面一手
 *       **完全一致**（订正 `ufo_group_keep2_literal_th12()` 与单测；行为本来就没变）。
 *   36. ⏳ **没改但要用户拍**（一手已给数值，改法不明确/有争议）：HP 一手是 {1600,1900,2100,2300,2500}
 *       按火力档（`th12:0x4b33f0`，照抄会触发 TH18 的 `0x42d8f6` 位）；令牌收取一手是自机收点矩形
 *       （半尺寸 15 单位，来自玩家表 `0x4b31b4`）—— ★ 已按用户 **P5** 改成「读本作自己的收点半径」
 *       （`player+0x4798c`）；血条一手是 `front.anm` 134/136
 *       （`0x44a5d0` 每帧写 `scale.x = life.cur/max`）而我们用 `drawRect` 占位。见报告。
 *
 * ============== ★★ 2026-09-12（第二轮）**令牌从真敌人载体 → 自建 VM 池**（用户拍板）==============
 * 一句话：**一只 token = 一个 `ce_anm_spawn` 出来的 VM**（脚本不变、层不变），位置与运动跑在
 * **我们自己的** `ufo_tok_motion_t` 上，池是文件静态的 `s_tokpool`（先例 117 `animal.c` / 113 `trance.c`）。
 *   37. **删掉的东西**（逐条 + 它们**只为旧载体而存在**的依据）：
 *       · `ce_spawn_enemy(UFO_ECL_TOKEN, spawn)` 那条路 + 整个 `CE_SPAWN_*` 填表（令牌不是敌人了）；
 *       · `UFO_ECL_TOKEN` / `UFO_ECL_TOKEN_NAME`（`"NoEnemyDebug"` 占位 ECL 名依赖，纯属载体）；
 *       · `UFO_TOKEN_LIFE`（令牌 HP）—— 没有实体就没有 HP，它的存在只是为了「能被子弹打掉」；
 *       · **「token 被子弹打掉也算收」那条路**：`ufo_token_kill_counts()` + `ufo_tok_slot()`（按
 *         `enemy_id` 找实体的分配器）+ `on_enemy_death` 里的令牌死亡支 + 死亡栈登记环
 *         （`pending_color/pending_n/pending_token/pending_drop` + `ufo_pending_put/take` +
 *         `on_tick_2` 的出队段）—— 这一整套的唯一用途就是那条路；
 *       · `UFO_ENEMY_ABS_POS/_Y`（`enemy+0x12b4/+0x12b8` 的 `abs_pos` 双写）—— 只为「令牌是敌人」而钉；
 *       · `UFO_FLAGS_DELETE_NOW` 的**令牌用法**（位 25 静默撤退；常量本身留着，本体仍要用）。
 *       自证（只数**代码行**，注释与日志字符串不算）：对 `cards/ufo.c` 跑一条「只看非注释行」的
 *              `grep -E`，令牌相关的符号（旧壳名 / 旧 HP 常量 / 旧死亡判据 / 旧实体分配器 /
 *              旧的 `pending_*` 环）命中为 **0**；本体那一个建敌入口命中 **1**。
 *              ⚠ 那条 grep 的**原文刻意不写进这里**（免得它自己命中自己，把判据污染掉）；
 *              报告里有可复制的原句与输出。
 *   38. **换来的好处**（用户点名的第一条副作用）：令牌不再是真敌人 ⇒ **`enemy_count_real` 不再把它
 *       算进去** ⇒ 炸弹门 `0x420453` / 决死门 `0x45c07b` 不会再因为「场上有令牌」而放行
 *       （`AUDIT.md` §BO-3 的那条副作用随之消失）。同时令牌也**打不掉了** —— 与一手（道具、只能接触收）
 *       一致，「碰到才收」的手感回来了。
 *   39. **保持不变的**：变色（150 帧一轮、红→蓝→绿）、快闪（90 帧起）、**接触收取**（半径现在读自机字段，
 *       半径本次刻意不改）、五边形提示框跟着令牌走、`UFO_TOKEN_MAX` 上限、`ufo_core.*` 的槽逻辑。
 *       ★ 换色仍是「删旧 VM + 起新 VM」（SDK 没有「给已有 VM 重指脚本」这条通路；原生是同一个 VM
 *       原地换脚本）—— 行为与换池前一致，差别只在那一个理论上的「换色帧空档」。 */

/* ==========================================================================================
 * ★★ 待实装的卡间耦合约定（用户 2026-09-15 定，**机制尚未设计**）—— 110/111 的绿点对飞碟的等价性
 * ==========================================================================================
 * 背景：110/111 的「绿点」（信仰点）是 `cards/faith.c` 的**自建 VM 池**，
 *       而本卡的飞碟吸收走的是**原生道具链** ⇒ 现在**看不到它们**（绿点既不会被吸、也不计数）。
 * 用户口径（原话）：
 *   · 「绿点**也会被飞碟吸走**」；
 *   · 「因为绿点是绿的，所以 **1 个大/中绿点会给绿色 UFO 计 2 个计数**」；
 *   · 「具体的语义**等价于 P 点对于红 UFO**；特别地，由于**中绿点是从 P 点转换过来的**，
 *      所以它**对于红 UFO 和彩虹 UFO 都等价于 P 点**」。
 *
 * ★ 等价表（先钉语义，机制后设计）：
 *
 *   | 我们的点                       | 绿 UFO | 红 UFO        | 彩虹 UFO      |
 *   | ---                            | ---    | ---           | ---           |
 *   | 大绿点（type 3，击破掉落）     | 计 2   | 等价于 P 点   | 未定          |
 *   | 中绿点（type 9，**由 P 点转换**）| 计 2  | 等价于 P 点   | ★ 也等价于 P 点 |
 *   | 微绿点（type 8，消弹）         | —（不入批、也不参与本耦合）        |
 *
 * 语义要点：**等价性来自「它原本是什么」** —— 中绿点是满 P 转换的产物 ⇒ 继承 P 点在 UFO 侧的待遇（红 + 彩虹）；
 *          大绿点是击破掉落 ⇒ 只按「绿」这条走。
 *
 * ⚠ 机制要设计时必须先定（写在这里免得丢）：
 *   1. 「1 个计数」在**本卡**是「1 颗道具」还是「1 点吸收进度」？（决定「计 2」是算两颗还是进度 ×2）
 *   2. 绿点被吸走之后**还算不算 110/111 的信仰**（纯吃 vs 信仰也让渡）—— 不定这条同一颗点会被两处记账；
 *   3. 对**异色** UFO（蓝/绿/红/彩虹 之外）的行为未定；
 *   4. 通道形状：与 `NEXT.md` §1x.2 同形 ⇒ **优先做声明式**（自建实体声明「我在飞 / 我被收」，
 *      本卡在吸收判定里读它），这样 116 季节点 / 117 动物灵将来也能复用。
 *   5. 排期依据与完整背景见 `NEXT.md` **§1x.7**；本卡改动按 `CLAUDE.md` 的协作纪律登记 `AUDIT.md` §BE。
 * ========================================================================================== */
