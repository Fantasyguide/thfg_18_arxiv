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
 *    框 = **零售 TH12 `bullet.anm` script211 的逐条移植**（`assets/ability/scripts/223_ufo_pentagon.anm.txt`，
 *    `drawPolyBorder(1.0f, 5)` = 程序画的五边形、**无贴图**），触发条件 / 颜色标签 / 层号 / 生命周期
 *    全部一手（见下面那个「★★ 五边形框」常量块）；卡侧 `ufo_pentagon_hint_update()` 每帧驱动，
 *    **不用 `drawRect` 硬凑假的五边形**（用户明确要求）。
 * ④ **左下角飞碟槽 HUD 第一版** ⇒ 真认领货架槽（`ce_hud_slot_claim_card(112, …)`）并画
 *    三格令牌槽 + 本体血条 + 一行贴图 VM 读数，全部在 **UI 层 25**（会被暂停模糊罩盖住）。
 *
 * ★ 本次还复核了一条 **TH12 一手**（裁决 ② 的依据，不是 TH18）：
 *   `cd local/th12.v1.00b && objdump -d --start-address=0x422e80 --stop-address=0x422f20 -M intel th12.exe`
 *   ⇒ 令牌槽状态机 `th12:0x422e80` 的全部分支（见 `ufo_core.c` 的 `ufo_group_put` / `ufo_group_drop_oldest`
 *   逐指令对照）。⚠ 与**字面一手**有一格差异：`{A,B,A}`（首尾同色）在字面一手里会掉到
 *   `0x422eeb or eax,-1` 出**彩虹**，不走 KEEP2；用户口径是「槽满且不成组 ⇒ 丢最旧」（三种排列一致）
 *   ⇒ 我们按用户口径实现，差异钉在 `ufo_group_keep2_literal_th12()` + 单测里，⏳ 待实机复现一次。
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
 *    `0x446252 mulss xmm0,xmm0` + `0x446256 comiss` ⇒ 半径 = `player+0x4798c`（`PLAN` §2.1：零售默认 30.0f）。
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

/* ============================ 裸引擎地址 / 偏移（本卡不改 sdk.h / engine.h，缺的写这里）============================
 * ⚠ 地址 = TH18 v1.00a。已有 SDK 宏的**不重复造**（`CE_ITEM_*` / `CE_SPAWN_*` / `CE_ENEMY_*` / `ce_spawn_enemy`）。*/

/* 道具 vel（float3）—— `engine.h` 没收，出处见文件头 ①。 */
#define UFO_ITEM_VEL        0xc38
#define UFO_ITEM_ANM_ID     0xc28     /* `item+0xc28` = 这颗道具的 anm VM id（清尾要删它；出处同 ④） */
/* 敌人**本体血量当前值**：`zEnemyData.life.current` = `enemy + 0x122c (CE_ENEMY_DATA) + 0x4ff4` = `+0x6220`
 * （一手交叉：`animal.c` 的 `ANIMAL_ENEMY_LIFE_CUR` 与 `lolk.c` 的 `CE_ENEMY_LIFE_CUR` 都写 0x6220，
 *  且 `engine.h` 的 `CE_ENEMY_DATA` 注里 0x6220 就是 `data.life`）⇒ HUD 血条第一版读它。 */
#define UFO_ENEMY_LIFE_CUR  0x6220
/* 敌人「静默撤退」位：置 `flags_lo` 位 25 ⇒ 同一帧的 EnemyManager sweep 直接析构它，
 * 不跑死亡处理 / 不掉落 / 不跑死亡 ECL。一手指令：`0x42d200 or [ecx+0x635c],0x2000000`（写）、
 * `0x42dfc5 test [ecx+0x635c],0x2000000` + `0x42dfcf jne 0x42dfe8` + `0x42dff2 call [eax+0x14]`（读/析构）。
 * ⚠ 不自己调析构（那是 UAF：我们可能正处在别人的链表遍历里，`DEATH` §2.5）。108 `youkai.c` 同款。 */
#define UFO_FLAGS_DELETE_NOW 0x2000000u

/* 音效（`engine/_shared/th18-sound-table.md` 一手表；用**具名宏**，不写裸数字）*/
#define UFO_SE_ABSORB   0x0d    /* `se_powerup.wav`（-1 dB / prio 90）：每吸一颗 */
#define UFO_SE_FULL     0x2f    /* `se_bonus.wav`（+0 dB / prio 100）：吸满吐奖励 */
#define UFO_SE_BREAK    0x38    /* `se_extend2.wav`（+0 dB / prio 100）：击破 */
#define UFO_SE_TOKEN    0x39    /* `se_pin00.wav`：令牌入槽（与 114 的收点结算钟声同 id）*/
#define UFO_SE_ITEM_BREAK 0x11  /* `se_extend.wav`：赏金道具落地（**占位**：TH12 的是专用音，未查）*/

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
 *   ★★ 资产到位后要改的就是下面这 4 行（**只改值，不改名**）：
 *   RED     "NoEnemyDebug" → "UFO_Red"
 *   BLUE    "NoEnemyDebug" → "UFO_Blue"       （金也走这一件，见上）
 *   GREEN   "NoEnemyDebug" → "UFO_Green"
 *   RAINBOW "NoEnemyDebug" → "UFO_Rainbow"
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
 *   ⚠ **令牌（`UFO_ECL_TOKEN`）没有可移植的真名** —— 一手名字表里**只有那 4 项**，没有令牌子程序。
 *     TH12 的令牌是**道具**（type 10–15），行为在 C 侧（`th12:0x4262f5` 每 150 帧换色 / 60 px 内暂停），
 *     不在 ECL 里 ⇒ 令牌子程序只能**新写**（`assets/ecl/ufo.ecl.txt` 也还没写它）。 */
#define UFO_ECL_TOKEN   UFO_ECL_TOKEN_NAME     /* ★ TODO(资产/ECL §6 步 3)：令牌自己的子程序名待写 */
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
    default: return UFO_ECL_BODY_BLUE;     /* 非法色到不了（`ufo_group_consume` 只吐合法色）*/
    }
}

/* ============================ ★ 跟随 VM：本体与令牌的**画面**（2026-09-12 本轮新增）============================
 * 背景：TH12 的四色飞碟动画（+ 六态令牌）**已经移植**进 `ability.anm`（脚本 164..167 / 168..173，
 * `make anm` 产出、号在 `native/anm_ids.h`），但 **ECL 画不出来** —— 见下面那个注释块。
 * 做法：ECL 侧只把 UFO 敌机刷成**隐形载体**（`assets/ecl/ufo.ecl.txt` 的 Δ2），画面由本文件
 * 起一个**跟随 VM**（world 列表、实体坐标每帧抄敌机的 `abs_pos`）来画。
 * 令牌**不需要新 ECL**：它现在挂 `"NoEnemyDebug"`（零售 `default.ecl` 里的死循环空壳 `wait(1000)`，
 * 不移动不开火）—— 正是我们要的惰性载体，本文件补上跟随 VM 即可。 */

/* ECL 把本体刷成隐形用的脚本号 —— **`-1` = `anmSetSprite` 的「拆槽」分支**：
 * `S < 0` ⇒ 先 `call 0x488cf0` 把该槽旧 VM 标删、槽写 0、**不建新 VM**（一手 `th18:0x436bf6`：
 * `test esi,esi; js → 收工`；opcode 303，独立函数 `th18:0x436bb0`）。
 * 零售真的这么用：**14 处 `anmSetSprite(K, -1)` 全在 boss 死亡 / 符卡收尾**
 * （`st02mbs:179 anmSetSprite(1,-1)`、`st07bs:2390 anmSetSprite(2,-1)` …）⇒ 原版「让这个槽不画东西」
 * 的**官方手法**。一手全文：`engine/anm/th18/04-anm-slot-selection.md` §4。
 * ★ 画面不走 ECL：TH12 的飞碟动画已移植进 ability.anm（script 164..167），
 *   由本文件起一个**跟随 VM** 画在敌机位置上 —— 因为 ECL 的 anm 索引表（th18:0x438d40）里没有 ability.anm
 *   （`N==0 → bullet.anm`、`N==1 → effect.anm`、`N==7 → 本关 st0Xwl.anm`、
 *    `N>=2 → 本关 ECL 头 anim{} 的第 N-2 条`；七关的 anim{} 里一个 ability.anm 都没有）。
 *   ⚠ 两边与 `assets/ecl/ufo.ecl.txt` 的那 8 个数必须一起改（同一条一手链写在那里）。 */
#define UFO_ECL_HIDDEN_BODY_SCRIPT  (-1)  /* = `anmSetSprite(0, -1)`：拆槽，不建 VM（`assets/ecl/ufo.ecl.txt` 里那 8 个数）*/

/* 跟随 VM 的层（一手：`engine/anm/th18/02-render-stages.md` §1 的优先级表 —— 层 6/7/8/9/10/11 = 「敌机等」，
 * 都在相机 3 = 游玩区）。
 *   本体：**脚本自己写了 `layer(7)`**（`assets/ability/scripts/164_ufo_red.anm.txt`）⇒ 这里给同一个值
 *         （`ce_anm_spawn` 的 layer 只是实例化默认值，脚本里的 `layer(n)` 会覆盖）。
 *   令牌：脚本**没写** `layer()`（零售 bullet.anm 175..180 就没有）⇒ 这个参数是**唯一**的层来源。
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
 * 贴图 = 零售 `bullet.anm` 175..180 的 6 态（`make_ufo_token_art.py`）。★ 2026-09-12（A8）：
 * 彩虹那三条（171..173）是**被截断**的版本（没有 `interruptLabel`）⇒ **这里不再给彩虹用**；
 * 彩虹改指 `ability.anm` 219..221（全文版，三轮中断都在），见下面那个常量块。 */
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
 *   | 标签 | 段正文（零售 `bullet.anm`） | 发送者 | 触发条件 |
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
 *      TH18 零售的同款写法（v8 opcode 605 可用）：`effect.anm` script101 `sprite(-1); drawPolyBorder(1.0f, 12);`、
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
 *      ⇒ 零售彩虹令牌（type 13/14/15）走的是「**它当前显示的那一色**」那一条 case ——
 *        我们的令牌颜色模型里彩虹是**通配**（`ufo_core.h` 的凑组规则）⇒ 判据用 `t->color`
 *        （既有钩子 `ufo_pentagon_hint_for_color()`，已有单测），**有意不跟「当前态」那条**：
 *        否则会出现「框不亮、收下去却成了组」。⏳ 待用户看一眼（要改成零售字面口径 = 把这里的
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
 * **本卡与零售的差别**（都是「载体不同」逼出来的，逐条登记）：
 *   ① 零售的框挂在**道具**上：位置在建框那一刻抄一次（`0x4615a0` 内部把 pos 拷进 VM），**之后不跟随**；
 *      我们的令牌是**敌机实体** + 跟随 VM（`ufo_tok_vm_spawn`）⇒ 框也每帧跟到实体坐标
 *      （`ufo_tok_penta_follow`）。令牌实体不动时两者等价；真 ECL 到位后会飘（`moveLimit` 活动框）、
 *      不跟随就会「框留在原地、令牌飘走」。
 *   ② 金在零售里不存在 ⇒ 金的框落在蓝那一格（`ufo_group_classify()` 内部过 `ufo_color_norm()`）——
 *      与用户裁决 ①（金 = 蓝的调色版）一致。
 *   ③ 零售的框在**框住的道具所在的那只 VM 之外另起一个 VM**（句柄存 `item+0x968`），我们也是：
 *      贴图那只在 `ufo_tok_t.anm_id`（卡私状态），框那只在 `s_penta_id[槽号]`（**文件静态** —— 私状态
 *      只剩 28 B，16 个槽 × 4 B = 64 B 放不下；理由与被 `_Static_assert` 拦下的过程写在那个数组的注里）。
 *
 * **层号**（★ 照抄零售）：脚本自己写 `layer(12)` —— 零售原值，且 12 落在世界层区间（3–19）里，所以
 *   **不是**「别作的层号」⇒ 照抄不改（对照先例：本体脚本 `164_ufo_red` 的 `layer(7)` 也是从 TH12
 *   `enemy.anm` script135 原样搬的）。我们的令牌跟随 VM 在 `UFO_TOKEN_ANM_LAYER` = **13** ⇒ 框正好
 *   在令牌**下面一层**，与零售「框 12 / 道具 VM 23」的相对关系同向。
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
 * ⚠ **必须**用带 `interruptLabel` 的全文版（219..221，`make_ufo_token_art.py` 生成）：
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
static int ufo_tok_script(int32_t color, int rb_state)
{
    if (ufo_color_art_index(color) == 3)                 /* 3 = 彩虹那一格（`ufo_color_art_index`）*/
        return ufo_token_rainbow_script(rb_state % UFO_TOK_RB_STATES);
    return ufo_token_anm_script(color);
}

/* ---- HUD 槽声明的块尺寸（表面 px）与第一版元素常量（用户 2026-09-12 裁决 ④）------------
 * ★ 本卡现在**真的认领**左下角货架的槽并画第一版（之前是「故意不认领」）。
 * 块 = 3 格令牌槽（上）+ 一条本体血条（下）+ 一行贴图 VM 读数（右侧）。
 * ★ 层：**全部在 UI 层 25**（用户 2026-09-12 硬规则：只有 layer 24/25 会被暂停模糊罩盖住）——
 *   矩形用 `ability.anm` 脚本 104（1×1 `drawRect` 模板，脚本里写的是 `layer(25)`），
 *   读数用 `ce_hud_vm_text_*`（脚本 136 字形，`layer(25)`）。⚠ **不用** `ce_hud_text*`（ASCII）：
 *   那几趟绘制结构上不可能被暂停罩盖住（`engine/anm/th18/03-ascii-text.md` §2f）。
 * ★ 锚点：脚本 104 **没有写 `anchor`** ⇒ 按 `AUDIT.md` §AL20（2026-09-10 两次实跑）**矩形默认锚 =
 *   **左上角**（⚠ `signal.c` / `stock.c` 的注释写「图心」，与 AL20 冲突；这里按 AL20 的实跑结论算左上角，
 *   一个宏 `UFO_HUD_RECT_ANCHOR_CENTER` 就能整体翻过去）。
 * ❓ 自定：块尺寸 / 格尺寸 / 间距 / 配色 / 字号**全是初版**（用户会一点一点对）。
 *   原版 TH12 的三格槽与血条是**零售切图**（`front.anm` 80/81 与 134/136，spec §5）——
 *   我们一张都没切 ⇒ 用 `drawRect` 拼，**不冒充复刻**（照 `signal.c` 的先例）。 */
#define UFO_ID_NUM        112u      /* 本卡 id（HUD 槽 / 日志用；`CE_CARD(112, …)` 里的那个数）*/
#define UFO_HUD_BLOCK_W   112.0f    /* ❓ 自定：块宽 = 68（三格）+ 6（间隙）+ ~32（读数 3 个字）*/
#define UFO_HUD_BLOCK_H    30.0f    /* ❓ 自定：块高 = 20（格）+ 4（空档）+ 6（血条）*/
#define UFO_HUD_CELL_W     20.0f    /* ❓ 自定：一格令牌槽 */
#define UFO_HUD_CELL_H     20.0f    /* ❓ 自定 */
#define UFO_HUD_CELL_GAP    4.0f    /* ❓ 自定：格子间距 */
#define UFO_HUD_BAR_H       6.0f    /* ❓ 自定：血条高（贴着块底边）*/
#define UFO_HUD_TEXT_DX     6.0f    /* ❓ 自定：读数与三格的水平间距 */
#define UFO_HUD_TEXT_SCALE  0.75f   /* ❓ 自定：读数字号（1.0 = 引擎 ASCII 的常态大小）*/
#define UFO_HUD_RECT_ANCHOR_CENTER 0   /* ❓ 自定：0 = 左上角锚（AUDIT §AL20）；1 = 图心锚 */
/* 颜色一律 D3DCOLOR `0xAARRGGBB`（一手：`AUDIT.md` §O29k —— Tenshi 的 `0xff0080ff` = 蓝 (0,128,255)）。*/
#define UFO_HUD_COLOR_RED      0xffff4040u  /* ❓ 自定 */
#define UFO_HUD_COLOR_BLUE     0xff4080ffu  /* ❓ 自定 */
#define UFO_HUD_COLOR_GREEN    0xff40ff40u  /* ❓ 自定 */
#define UFO_HUD_COLOR_RAINBOW  0xffffffffu  /* ❓ 自定：彩虹令牌先用白（四色都要能分辨）*/
#define UFO_HUD_COLOR_GOLD     UFO_HUD_COLOR_BLUE   /* ★ 金 = 蓝的调色版 ⇒ 先用蓝那一档（等 P 图）*/
#define UFO_HUD_COLOR_EMPTY    0xff202020u  /* ❓ 自定：空格子的底色 */
#define UFO_HUD_COLOR_BAR_BG   0xff303030u  /* ❓ 自定：血条底 */
#define UFO_HUD_COLOR_BAR_FILL 0xff60ff90u  /* ❓ 自定：血条填充 */
#define UFO_HUD_COLOR_TEXT     0xffffffffu  /* ❓ 自定：读数颜色 */
_Static_assert(UFO_HUD_BLOCK_W > 0.0f && UFO_HUD_BLOCK_H > 0.0f,
               "the UFO HUD block is claimed now (user ruling 4) - a zero-size claim would be a bug");
_Static_assert(UFO_HUD_CELL_W * 3.0f + UFO_HUD_CELL_GAP * 2.0f <= UFO_HUD_BLOCK_W,
               "three token cells must fit inside the claimed block");
_Static_assert(UFO_HUD_CELL_H + UFO_HUD_BAR_H <= UFO_HUD_BLOCK_H,
               "cells + HP bar must fit inside the claimed block");

/* 帧源：`on_item_frame`（`0x446ee3`）每次分派 = 一个**正常游玩帧**（冻结/商店帧连门卫 CMP 都到不了；
 * 114 `collect2x.c` 的一手推导）。本卡全程用它当帧计数，不用 `card+0x20` 那个钟。 */

/* ------------------------------------------------------------------ 卡状态
 * 私有状态预算 = `CE_STATE_BYTES`(512) − `CE_STATE_RESERVED`(16) = 496 B（sdk_core.h）。
 * 令牌表 16 × 16 B = 256 B 进私有状态（比照 108/117 的静态表，但本表**必须**随卡对象走：
 * 卡对象可能在局中被重建 ⇒ 静态表会串台）。当前 `sizeof(ufo_card_t)` = **468 B**
 * （2026-09-12 **实测**，编译期断言守着 ≤ 496 B；A8 加的彩虹三字段填进了 `ufo_tok_t` 原来的尾巴，
 * `sizeof(ufo_tok_t)` 仍是 20 B ⇒ 净增 0）。 */
#define UFO_TOKEN_SLOTS 16      /* ★❓ 自定：同时最多记几个令牌实体（`UFO_TOKEN_MAX` 是「同时活几个」，
                                 * 这里是「能记几个」的分配护栏）*/
/* 令牌颜色的**登记环**容量（死亡回调里入队、帧末出队落槽）。
 * 为什么要环：死亡事件是**广播**、一次 `Enemy__die` 扫描里可能死好几只（炸弹 / 清场），
 * 单寄存器会互相覆盖 ⇒ 丢掉的颜色随实体一起没了（玩家看到「令牌没了、什么都没发生」）。
 * ★ **唯一真源就是这一个宏**（结构体数组与 `ufo_pending_put` 的边界判据都用它，别再各写一遍表达式）。 */
#define UFO_PENDING_CAP (UFO_TOKEN_MAX + 2)
/* ★❓ 自定（spec §9 #12「令牌掉落普查」：TH18 侧「谁产令牌、产多少」没有一手 ⇒ 自定）：
 * 令牌实体**由本卡按节流自己撒**（占位期最简做法，不改 ECL / 不改掉率）。
 * ⏳ 用户拍板「改成打谁掉」之后，这里整段换成死亡事件里的落点即可（接口已经就位）。 */
#define UFO_TOKEN_SPAWN_INTERVAL 300  /* 每 300 游玩帧（5 s）尝试出一个 */
#define UFO_TOKEN_SPAWN_RETRY    120  /* 造失败（ECL 名查不到 / 表满）时的重试间隔 */
#define UFO_TOKEN_SPAWN_FIRST    180  /* 开局先等 3 s 再出第一个 */

typedef struct {
    int32_t  color;      /* UFO_COLOR_*；UFO_COLOR_NONE = 空条目 */
    /* ★ 这里曾经有个 `int32_t armed`（「已算进令牌槽」），但它**恒为 0** ⇒ 让
     * `ufo_tok_live_count` 恒返回 0、`UFO_TOKEN_MAX` 的上限形同不存在（每一帧都能再撒一个令牌）。
     * 实装后**整个删掉**（不留占位格）：没有实体需要「已武装」这个中间态（入槽即撤，
     * `ufo_tok_take_and_kill`），去重靠 `enemy_id`（撤掉之后 `ufo_tok_slot` 自然找不到）。 */
    uint32_t enemy_id;   /* 该令牌实体的 `enemy_id`（⚠ 不存指针） */
    uint32_t ttl;        /* 剩余存活帧（到 0 ⇒ 静默撤退；占位期惰性壳不会自己消失，靠它兜底） */
    /* ★ 本轮新增：这一枚令牌的**跟随 VM**（`ability.anm` 168..173 之一）的 anm id。
     * 为什么要有它：ECL 侧那个 `"NoEnemyDebug"` 惰性壳**什么都不画**（它只是载体），
     * 令牌的贴图由本文件起一个跟随 VM 画在实体位置上（见 `ufo_token_anm_script()` 上面的注释块）。
     * ⚠ 铁律：句柄非 0 ⇒ 必须有一条删除路径（`ufo_tok_kill()` / 退休分支 / `ufo_withdraw_all()`）。*/
    uint32_t anm_id;
    /* ---- 下面四个字节字段（★ 全是 uint8，为的是 `ufo_tok_t` 不长胖；`ufo_tok_t` = 20 B）---- */
    /* ★ 用户 2026-09-12 裁决 ③：这一枚令牌够不够格被画一圈**五边形框**
     *（= 槽里正好 2 枚、收了它就能成组）。判据在纯逻辑层 `ufo_pentagon_hint_for_color()`，
     * 每帧由 `ufo_pentagon_hint_update()` 刷新；**框本身自 2026-09-12（A7）起已实装**（脚本 223）。
     * ⚠ 框的 **VM 句柄不在这个结构体里**（16 个令牌槽各要 4 B = 64 B，私状态放不下）⇒
     *   在 `s_penta_id[]`（文件静态），理由与收尾路径见那个数组的注。 */
    uint8_t  pentagon_hint;
    /* ★ 2026-09-12（A8）彩虹令牌的三段中断 + 换色（一手见上面的常量块）。**三个字段与
     *   `pentagon_hint` 挤在同一段尾巴里**（`ufo_tok_t` 一直是 20 B）：
     *   `rb_state` = 当前态 0/1/2 ⇒ 脚本 219/220/221（TH12 的道具类型 13/14/15，`th12:0x4264b6`）；
     *   `rb_cycle` = 换色计时器（TH12 的 `item+0x9a0`）—— 一手只在 0..150 之间取值（到 150 就换色并
     *                清 0）⇒ **uint8 够**（断言在下面）；
     *   `near`     = 上一帧在不在 60 px 圈内（TH12 的 `item+0x9c4` 闩，0 = 圈外 —— 出生也清 0）。
     * ⚠ 只有**彩虹**令牌用这三个字段（一手把整段关在 `if (type == 13||14||15)` 里）；
     *   非彩虹令牌恒为 0（换来的是「一个结构体一套语义」，不做 union）。 */
    uint8_t  rb_state;
    uint8_t  rb_cycle;
    uint8_t  near;
} ufo_tok_t;

typedef struct {
    ufo_group_t grp;                                   /* 3 个令牌槽（spec §3.3 末行）*/
    ufo_ufo_t   u;                                     /* UFO 生命周期 / 两个计数 / fill */
    ufo_tok_t   tok[UFO_TOKEN_SLOTS];
    float       last_x, last_y;                        /* 最近一次已知的 UFO 位置（击破时由死亡事件给；
                                                        * ⚠ 不是缓存指针 ⇒ 合规，`DEATH` §2.4 只禁指针）*/
    uint32_t    play_frame;                            /* 游玩帧计数 */
    uint32_t    next_token_at;                         /* 下一个令牌最早能在哪一帧出现（节流）*/
    uint32_t    token_serial;                          /* 本局第几个令牌（散列出生点用）*/
    /* ★ 本轮新增：**本体**的跟随 VM 句柄（`ability.anm` 164..167 之一）。
     * 生命周期与 `u` 绑死：`ufo_try_summon()` 建起来、`ufo_teardown()` / `ufo_withdraw_all()` 收掉
     * （⚠ 删卡走 `on_delete`，那一刻 tick 全停 ⇒ 必须在 `ufo_withdraw_all()` 里收，不能指望每帧跟随时收）。*/
    uint32_t    body_anm_id;
    /* 死亡回调里登记的令牌颜色（帧末落槽）。★ 是一个**小环**而不是单个寄存器：
     * 同一次 `Enemy__die` 扫描里可能有**多只**令牌一起死（炸弹 / 清场），单寄存器会互相覆盖
     * ⇒ 丢掉的颜色随实体一起没了（玩家看到「令牌没了、什么都没发生」）。 */
    int32_t     pending_color[UFO_PENDING_CAP];
    uint8_t     pending_n;                             /* 环里存了几个（<= 容量）*/
    uint8_t     pending_drop;                          /* 环满而丢掉的令牌数（记账；日志里打）*/
    uint8_t     pending_token;                         /* 兼容位：环非空 == 1（帧末处理）*/
    uint8_t     pending_break;                         /* 1 = 帧末要按「击破」收尾开奖 */
    uint8_t     logged_no_hud;                         /* 保留名：HUD 第一版「画起来了」日志只报一次 */
    uint8_t     logged_lifetime;                       /* 存活上限日志每局只报一次 */
    uint8_t     pentagon_any;                          /* 本帧有没有令牌够格画五边形框（= 场上真有框）*/
    uint8_t     pentagon_logged;                       /* 五边形框条件第一次成立时只报一条日志 */
    /* —— 统计（只用于日志，不参与判定）—— */
    uint32_t    spawned_tokens;
    uint32_t    taken_tokens;                          /* 被自机吃掉的 */
    uint32_t    lost_tokens;                           /* 超时 / 被打死 / 过场清掉 */
    uint32_t    ufos_spawned;
    uint32_t    ufos_broken;
    uint32_t    ufos_vanished;
    uint32_t    items_pulled;
    uint32_t    rewards_spit;
    uint32_t    bursts;                                /* 吸满吐奖的次数 */
    uint32_t    dropped_tokens;                        /* 「两同一异」丢最旧丢掉的最旧令牌数（一手 KEEP2）*/
    uint32_t    failed_summons;                        /* `ce_spawn_enemy` 建不出来（占位 ECL 名）*/
} ufo_card_t;

/* ⚠ 全静态（无堆）：卡单例 + 无指针 ⇒ 与 108 的 12 KB 使魔表不是一回事，这里不需要第二个静态表。 */

/* ---- HUD 的 VM 句柄（★ 放**文件静态**而不是卡状态：`on_ctor` 会 `memset` 私有状态，
 *      而引擎可能在别处摧毁这些 UI VM ⇒ 句柄必须活得比一次 `memset` 长，靠每帧 `ufo_hud_ensure()`
 *      自愈（照 `signal.c` 的同一支写法）。**卡单例**（`CE_CARD(112, …)`）⇒ 静态没有串台问题。 */
static uint32_t s_cell_id[UFO_SLOT_COUNT];   /* 三格令牌槽（脚本 104 矩形）*/
static uint32_t s_bar_bg_id;                 /* 血条底 */
static uint32_t s_bar_fill_id;               /* 血条填充（宽度 = 本体 HP 千分比）*/
static ce_hud_vm_text_t s_hud_text;          /* 一行读数（脚本 136 字形，层 25）*/

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
static uint32_t s_penta_id[UFO_TOKEN_SLOTS];

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

/* 确定性散列（**不调 `ce_rand()`**：死亡回调里不引入随机流，`spec §7` 的 replay 安全）。
 * 用 `enemy_id`（单调递增、同局不复用，`engine.h:389`）+ 本局序号做一次整数混合。 */
static float ufo_hash_unit(uint32_t a, uint32_t b)
{
    uint32_t h = a * 2654435761u + b * 40503u + 0x9e3779b9u;
    h ^= h >> 15;
    h *= 2246822519u;
    h ^= h >> 13;
    return (float)(h & 0xffffu) / 65535.0f;            /* 0..1 */
}

/* 关卡号（填充率公式的入参）。
 * ★❓ 自定（spec §4：逐关阈值是**为 TH12 道具密度**算的，TH18 要重估）⇒ 现在取 **1**（= 一手公式的
 * 第 1 关增量 0.030），这是「照抄一手同一格」最保守的取法，不是本机关卡号。 ⏳ 待实跑标定。 */
#define UFO_PLACEHOLDER_STAGE 1

/* ========================= §3.2 令牌实体：建 / 撤 / 找 ========================= */
static ufo_tok_t *ufo_tok_slot(ufo_card_t *s, uint32_t id)
{
    int i;
    if (!id) return 0;
    for (i = 0; i < UFO_TOKEN_SLOTS; ++i)
        if (s->tok[i].color != UFO_COLOR_NONE && s->tok[i].enemy_id == id) return &s->tok[i];
    return 0;
}

static ufo_tok_t *ufo_tok_free(ufo_card_t *s)
{
    int i;
    for (i = 0; i < UFO_TOKEN_SLOTS; ++i)
        if (s->tok[i].color == UFO_COLOR_NONE) return &s->tok[i];
    return 0;
}

/* 场上还挂着几个令牌实体。⚠ **只看颜色**（原来多读了一个恒为 0 的 `armed`，
 * 导致恒返回 0、`UFO_TOKEN_MAX` 的上限完全失效）。 */
static int ufo_tok_live_count(const ufo_card_t *s)
{
    int i, n = 0;
    for (i = 0; i < UFO_TOKEN_SLOTS; ++i)
        if (s->tok[i].color != UFO_COLOR_NONE) ++n;
    return n;
}

/* 收掉**某个令牌槽**的**五边形框 VM**（句柄清零 ⇒ 幂等）。★ 2026-09-12（A7）新增。
 * 一手对应物 = 零售条件不成立那一支 `push [edi+0x968]; call 0x461a70` + `mov [edi+0x968],0`
 *（`AnmManager__delete_vm`：只**标删**，真正的析构由 AnmManager 自己的 update 走 ⇒ 在死亡栈上叫也安全）。*/
static void ufo_tok_penta_kill(int slot)
{
    if (s_penta_id[slot]) { ce_anm_delete(s_penta_id[slot]); s_penta_id[slot] = 0; }
}

/* 收掉一枚令牌的**贴图跟随 VM**（只这一个句柄）。
 * ★ 为什么要与框分开：彩虹令牌每 150 帧换色时走「删旧 VM + 起新 VM」（`ufo_tok_rainbow_recolor`），
 *   而零售换色只重指**令牌自己的那只 VM**（`0x4061e0(vm = item, …)`），**不动**框那只
 *   （框在 `item+0x968`，是另一个 VM）。⇒ 换色不该把框也删掉 —— 否则每 150 帧框会重新
 *   `scaleTime(10,4,20,20)` 胀一次（零售没有这个动作）。 */
static void ufo_tok_art_vm_kill(ufo_tok_t *t)
{
    if (t->anm_id) { ce_anm_delete(t->anm_id); t->anm_id = 0; }
}

/* 收掉一个令牌槽的**两个跟随 VM**（贴图 `anm_id` + 五边形框 `s_penta_id[slot]`；句柄清零 ⇒ 幂等；
 * `ce_anm_delete` 内部有 ANM 管理器门槛）。
 * ★ 铁律：令牌实体消失的**每一条路**都要过这里（撤实体 / 超时退休 / 静默消失 / 删卡），
 *   否则 VM 会留在场上画一个**看不见实体的幽灵令牌**（框也一样：一个没有令牌的空五边形）。 */
static void ufo_tok_vm_kill(int slot, ufo_tok_t *t)
{
    ufo_tok_art_vm_kill(t);
    ufo_tok_penta_kill(slot);
}

/* 收掉**整张表**的令牌跟随 VM。⚠ **只看句柄、不看 `color`**：`on_ctor` 的 `memset` 与
 * `ufo_withdraw_all` 都需要这一支 —— 句柄一旦被 memset 掉，那些 VM 就永远留在场上了
 * （删卡那一刻 tick 回调全停，没有任何「顺手收」的机会）。
 * ★ 框那 16 个句柄是**文件静态**（理由见它的定义处）⇒ 这一支**不依赖令牌表**，memset 之后照样收得掉。 */
static void ufo_tok_vm_kill_all(ufo_card_t *s)
{
    int i;
    for (i = 0; i < UFO_TOKEN_SLOTS; ++i) ufo_tok_vm_kill(i, &s->tok[i]);
}

/* 撤一个令牌实体：置「本帧删」位 ⇒ 同帧 sweep 静默析构（不掉落、不跑死亡处理）+ 收跟随 VM。
 * ⚠ 调用点有一半在**死亡栈**上（`on_enemy_death` → `ufo_tok_take_and_kill`）⇒ 必须守住 `DEATH` §2.4：
 *   **不析构敌人、不缓存 `ev->enemy`**（置位那一步正合纪律）。
 *   而「删一个 anm VM」**可以**在死亡栈上做 —— `0x488cf0` 只是把 VM **标删**
 *   （`vm+0x538 |= 0x80`，一手见 `engine/anm/th18/01-vm-instantiate.md` §0 的表），真正的析构由
 *   AnmManager 自己的 update 走；引擎在敌人析构那条路上也同样标自己的 `anm_vm_ids` 槽。
 *   ⚠❓ 但「从共享死亡断点直接进 AnmManager」这条组合**实跑没验过**（本卡第一次这么写）。
 *   对比：**建** VM 是绝对不行的（要分配）—— 本卡所有 `ce_anm_spawn` 都在 tick / 帧末，不在死亡栈。 */
static void ufo_tok_kill(int slot, ufo_tok_t *t)
{
    uint8_t *e = ufo_enemy_by_id(t->enemy_id);
    if (e) *(uint32_t *)(e + CE_ENEMY_FLAGS) |= UFO_FLAGS_DELETE_NOW;
    ufo_tok_vm_kill(slot, t);
    t->color = UFO_COLOR_NONE;
    t->enemy_id = 0;
    t->ttl = 0;
    /* ★ A8 的三个字段一起清（`color = NONE` 已经让槽变「空」，这里是防御性对账：
     *   下一次 `ufo_tok_spawn` 复用同一个槽时不该吃到上一枚令牌的计时器）。 */
    t->rb_state = 0;
    t->near = 0;
    t->rb_cycle = 0;
}

/* 起一个令牌跟随 VM，并**立刻**摆到实体位置上（世界层 VM 的实体坐标 (0,0,0) = 弹幕区上边框中点
 * ⇒ 不先摆位置的话会先在屏幕最上方闪一帧）。`e` = 令牌敌机（用来取位置）。失败只报日志（画不出来 ≠ 卡坏）。 */
static void ufo_tok_vm_spawn(ufo_tok_t *t, const uint8_t *e)
{
    int script = ufo_tok_script(t->color, t->rb_state);
    uint32_t id = ce_anm_spawn(CE_ABILITY_ANM(), script, UFO_TOKEN_ANM_LAYER);
    if (!id) {
        ce_log("ufo: token follow-VM spawn FAILED (color %d, script %d, layer %d) - the token entity "
               "is alive but invisible (`%s` is a lazy shell and paints nothing)",
               (int)t->color, script, UFO_TOKEN_ANM_LAYER, UFO_ECL_TOKEN);
        return;
    }
    t->anm_id = id;
    if (e) ce_anm_set_pos(id, *(const float *)(e + CE_ENEMY_POS_X),
                              *(const float *)(e + CE_ENEMY_POS_Y), 0.0f);
}

/* 每帧跟随（令牌）：把 VM 的位置抄成实体的 `abs_pos`。
 * ⚠ 句柄在别处被引擎销毁过（`ce_anm_get_vm` 找不到）⇒ **自愈**：丢掉句柄重新起一个
 *   （照 `ufo_hud_ensure()` 的同一支写法；一次性特效的脚本不会自己 `delete()`，都是死循环）。 */
static void ufo_tok_vm_follow(ufo_tok_t *t, const uint8_t *e)
{
    if (!t->anm_id || !ce_anm_get_vm(t->anm_id)) {
        t->anm_id = 0;
        ufo_tok_vm_spawn(t, e);
        return;                                        /* spawn 里已经摆好位置 */
    }
    ce_anm_set_pos(t->anm_id, *(const float *)(e + CE_ENEMY_POS_X),
                              *(const float *)(e + CE_ENEMY_POS_Y), 0.0f);
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

/* 建框（**只在还没有框时建**：零售每帧只查 `cmp dword [edi+0x968],0 / jne`（`th12:0x4265d5`）
 * ⇒ 条件成立的后续帧既不重建、也不重发标签）；建完**立刻发标签**（零售六处建框点都是
 * `create_vm` 之后马上 `call 0x461970`，标签在 EBX）。
 * ⚠ 次序不必担心「中断跳掉脚本初始化」：`ce_anm_spawn` 走的那条引擎通路
 *   （`AnmLoaded__instantiate_vm_to_world_list_back` `0x405bf0`）**自己先跑一帧**，
 *   脚本开头的 `sprite(-1)/layer(12)/drawPolyBorder(1.0f,5)/scaleTime(...)` 那时已经落地，
 *   下一帧才轮到 pending 中断跳到标签段（TH12 侧同构：`0x454db9` 的 `call 0x455630`）。
 * ⚠ 本函数会**建 VM** ⇒ 只能从 tick / 帧末调（本卡的调用点 = `ufo_pentagon_hint_update()`，
 *   而它在 `ufo_tokens_tick()` 里 ⇒ 每游玩帧的门卫位次，不是死亡栈）。 */
static void ufo_tok_penta_spawn(int slot, const uint8_t *e, int label)
{
    uint32_t id;

    if (s_penta_id[slot] || !label || !CE_ABILITY_ANM()) return;
    id = ce_anm_spawn(CE_ABILITY_ANM(), UFO_PENTAGON_ANM_SCRIPT, UFO_PENTAGON_ANM_LAYER);
    if (!id) {
        ce_log("ufo: pentagon hint-VM spawn FAILED (script %d, layer %d) - the frame around the "
               "completing token will NOT be drawn", UFO_PENTAGON_ANM_SCRIPT, UFO_PENTAGON_ANM_LAYER);
        return;
    }
    s_penta_id[slot] = id;
    if (e) ce_anm_set_pos(id, *(const float *)(e + CE_ENEMY_POS_X),
                              *(const float *)(e + CE_ENEMY_POS_Y), 0.0f);
    /* 标签 = 框的颜色。SDK 面 = 既有的 `ce_anm_interrupt`（`AnmManager__interrupt_tree` `th18:0x488be0`，
     * 写 pending interrupt 并递归子树；TH12 侧是 `0x461970`）—— A8 的彩虹三段中断用的就是它，零新 SDK 面。 */
    ce_anm_interrupt(id, label);
}

/* 每帧跟随（框）：与令牌贴图 VM 同一支写法（自愈 + 抄实体 `abs_pos`）。
 * ⚠ 零售**不跟随**（框的位置在 `create_vm` 那一刻被拷进 VM，之后道具飘走它也不动）——
 *   我们的令牌是**敌机实体**，真 ECL 到位后会飘（`moveLimit(0,128,280,128)`）⇒ 不跟随就会
 *   「框留在原地、令牌飘走」。实体不动时两者等价（占位期 `NoEnemyDebug` 就是不动）。
 * ⚠ 句柄被引擎在别处销毁过 ⇒ 只**丢掉句柄**，重建交给紧接着的 `ufo_tok_penta_spawn()`
 *   （它带着本帧算出来的标签；这里自己重建就会造出一个「没有标签 = 停在白色」的框）。 */
static void ufo_tok_penta_follow(int slot, const uint8_t *e)
{
    if (!s_penta_id[slot] || !ce_anm_get_vm(s_penta_id[slot])) {
        s_penta_id[slot] = 0;
        return;
    }
    ce_anm_set_pos(s_penta_id[slot], *(const float *)(e + CE_ENEMY_POS_X),
                                      *(const float *)(e + CE_ENEMY_POS_Y), 0.0f);
}

/* ---- ★ 彩虹令牌的换色（= TH12 的「重指脚本」，`th12:0x4061e0`；一手见常量块 ⑤）----
 * TH12 在**同一个 VM** 上 `AnmVm__wipe` + `load_script_into_vm`；SDK 没有这条通路
 * （`ce_anm_set_sprite` 只会被脚本下一帧的 `sprite()` 覆盖）⇒ 用**删旧 + 起新**等价达成，
 * 位置由 `ufo_tok_vm_spawn` 当场摆好（中间不产生「留在原点的 VM」）。
 * ⚠ 一手同时做了三件事，本函数逐条对应：
 *   ① `Timer__set_current(&t99c, 0)`（`0x426497`）⇒ `rb_cycle = 0`；
 *   ② `type = (type−0xc)%3 + 0xd`（`0x4264b6`）⇒ 态 +1 取模（13→14→15→13）；
 *   ③ 用 `type + 0xa5` 重新装载（`0x4264bc`）⇒ 换脚本重开。
 * ⚠ 一手还会播一个音（`0x4264dd` `0x453e20(x, 0x35)`）——`0x35` 是 **TH12 的 se id**，
 *   本仓没有 TH12→TH18 的音效映射表（`engine/_shared/th18-sound-table.md` 只有 TH18 那张）
 *   ⇒ **不播**，宁可没声音也不随便借一个 TH18 的 id 冒充。登记在文件尾。 */
static void ufo_tok_rainbow_recolor(ufo_tok_t *t, const uint8_t *e)
{
    t->rb_state = (uint8_t)((t->rb_state + 1) % UFO_TOK_RB_STATES);
    t->rb_cycle = 0;
    /* ★ 只收**贴图**那只（`ufo_tok_art_vm_kill`），**不碰五边形框**（A7）：零售换色重指的是
     *   令牌自己那只 VM，框在另一个槽（`item+0x968`）里、不受影响 ⇒ 框不该跟着重播胀开动画。 */
    ufo_tok_art_vm_kill(t);                            /* 句柄清零（幂等）*/
    ufo_tok_vm_spawn(t, e);                            /* 新脚本 + 当场摆位置；失败只报日志 */
}

/* ---- ★ 彩虹令牌每帧：60 px 闩 + 三段中断 + 换色计时器（一手见上面那个常量块）----
 * ⚠ 与 `ufo_tok_pick`（20 px 接触判定）是**两件事**：60 px 圈只影响画面与计时器，不是收集半径。 */
static void ufo_tok_rainbow_tick(ufo_tok_t *t, const uint8_t *e, float px, float py)
{
    float dx, dy;
    int near;

    if (t->color != UFO_COLOR_RAINBOW) return;         /* 一手整段只在 type 13/14/15 上跑 */
    dx = *(const float *)(e + CE_ENEMY_POS_X) - px;
    dy = *(const float *)(e + CE_ENEMY_POS_Y) - py;
    near = (dx * dx + dy * dy <= UFO_TOK_NEAR_R2);     /* 一手：距离² ≤ 3600（60 px）*/

    if (near != (int)t->near) {
        /* 进 / 出 60 px 圈的那一帧各发一次（一手两个「状态变了才发」的闩：`0x42630f` / `0x42632d`）。
         * 出圈时按计时器选 3（还没到 90 ⇒ 回主循环）或 2（已经在闪 ⇒ 接着闪）。 */
        if (near) ce_anm_interrupt(t->anm_id, UFO_TOK_INT_NEAR);
        else      ce_anm_interrupt(t->anm_id, t->rb_cycle < UFO_TOK_BLINK_AT ? UFO_TOK_INT_RESUME
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
    if (t->rb_cycle == UFO_TOK_BLINK_AT) ce_anm_interrupt(t->anm_id, UFO_TOK_INT_BLINK);
    if (t->rb_cycle >= UFO_TOK_CYCLE_TICKS) ufo_tok_rainbow_recolor(t, e);   /* 内部把计时器清 0 */
    if (t->rb_cycle < UFO_TOK_CYCLE_TICKS) ++t->rb_cycle;
}

/* 造一个令牌实体。颜色 = 实体自己的事（一手：彩虹令牌每 150 帧换色、60 px 内暂停 ——
 * spec §3.2/§1 末行「落在令牌实体上」）⇒ **占位期不写颜色**，由卡侧的合成规则决定它算哪一色。
 * ⚠ `spawn+0x14`（life）必须 < 1000（`0x42d8f6` 会置锁定黑名单位）。 */
static uint8_t *ufo_tok_spawn(ufo_card_t *s, float x, float y)
{
    uint8_t spawn[CE_SPAWN_SIZE];
    float *pos;
    ufo_tok_t *t;

    if (!CE_ENEMY_MGR()) return 0;
    t = ufo_tok_free(s);
    if (!t) {
        ce_log("ufo: token table full (%d slots) - token dropped", UFO_TOKEN_SLOTS);
        return 0;
    }
    memset(spawn, 0, sizeof spawn);
    pos = (float *)(spawn + CE_SPAWN_POS);
    pos[0] = x;
    pos[1] = y;
    pos[2] = 0.0f;
    *(int32_t *)(spawn + CE_SPAWN_SCORE) = 0;          /* ★ 打死令牌不给分（占位期取「不做」）*/
    *(int32_t *)(spawn + CE_SPAWN_MAIN_DROP) = 0;      /* ★ 令牌不打掉落（它自己就是令牌）*/
    *(int32_t *)(spawn + CE_SPAWN_PARENT) = 0;
    /* `CE_SPAWN_FLAG_A/B` 一律不设：`flags_lo & 0x31` 置位 = **伤害免疫**（`DEATH:163` 一手订正）
     * ⇒ 置了就收不了令牌。life 钳在 < 1000。 */
    *(int32_t *)(spawn + CE_SPAWN_LIFE) = ufo_clamp_spawn_life(UFO_TOKEN_LIFE);
    {
        uint8_t *e = (uint8_t *)ce_spawn_enemy(UFO_ECL_TOKEN, spawn);
        if (!e) {
            ce_log("ufo: ce_spawn_enemy(\"%s\") returned nothing - that ECL subroutine is a "
                   "placeholder and does not exist in default.ecl yet (spec §6 step 3)", UFO_ECL_TOKEN);
            return 0;
        }
        /* ★ 令牌的**颜色**：占位期由卡侧按确定性轮转给（真 ECL 到位后应改成「实体携带颜色 = `$I0`」
         * 并由卡侧读它，见 impl.md §2 第 8 条）。
         * ⚠⚠ 这里曾经写成 `(int32_t)ufo_hash_unit(0, serial)` —— `ufo_hash_unit` 返回 [0,1] 的小数，
         *   cast 成 int 几乎恒为 0（= `UFO_COLOR_NONE`）⇒ 令牌对所有代码路径**不可见**
         *   （`ufo_tok_slot` 找不到它 ⇒ 死亡事件不认账；`ufo_tokens_tick` 直接 continue ⇒ 不收不撤；
         *   `ufo_tok_free` 永远发同一个槽 ⇒ 每 300 帧泄漏一个真敌人；`ufo_color_valid(0)` 挡住召唤）。
         *   实测：serial 0..99999 里只有 1 个 cast 出非 0。⇒ **颜色必须是枚举值，不是散列**。
         * ★ 用户裁决 ①（金 = 蓝的调色版）：颜色走 `ufo_token_color_for_serial_gold()` —— 它在轮转表
         *   之上把「轮到蓝」的那些令牌每隔 `UFO_GOLD_CYCLE` 个周期做成**金**。金进槽即归一成蓝
         *   （`ufo_color_norm`）⇒ 除了贴图/颜色显示，**行为与蓝一字不差**。 */
        t->color = ufo_token_color_for_serial_gold(s->token_serial);
        t->enemy_id = *(uint32_t *)(e + CE_ENEMY_ID);
        t->ttl = UFO_TOKEN_TTL_FRAMES;
        t->pentagon_hint = 0;
        t->anm_id = 0;                                 /* ★ 新槽先清句柄（`ufo_tok_free` 只查 `color`）*/
        /* ★ A7：同一个槽的**五边形框**句柄一起清（防御性对账 —— 正常路径上 `ufo_tok_kill()` /
         *   退休分支已经把它收成 0 了；句柄在文件静态数组里，所以这里按**槽号**清）。
         *   ⚠ `t` 一定来自 `s->tok[]`（`ufo_tok_free(s)` 返回的指针）⇒ 指针相减就是槽号。 */
        s_penta_id[(int)(t - s->tok)] = 0;
        /* ★ A8：彩虹令牌的初始态。一手：`spawn_item` 把闩清 0（`0x427595`）⇒ 出生算「圈外」、
         * 第一帧不发中断；计时器从 0 起（换色后 `Timer__set_current(0)`）。起始**态**取 0 = 红那一态
         * （= TH12 的道具类型 13 ⇒ 脚本 178）。⚠❓ 一手里真正的起始态由「哪个 UFO 掉的」决定
         * （红 UFO → 13、蓝 → 14、绿 → 15、彩虹 → 按第 3 个令牌的颜色，`th12:0x44af00`）；
         * 我们的令牌实体**不携带颜色**（卡侧轮转表给，见文件尾 §自定 24）⇒ 统一从红起。 */
        t->rb_state = 0;
        t->near = 0;
        t->rb_cycle = 0;
        /* ★ 画面：起跟随 VM（ECL 侧是惰性壳，什么都不画 —— 见文件头「跟随 VM」注释块）*/
        ufo_tok_vm_spawn(t, e);
        ++s->spawned_tokens;
        ++s->token_serial;
        return e;
    }
}

/* ==================== §3.3 召唤：UFO 本体（ECL 真敌机）==================== */
/* 出生点：一手活动框 `moveLimit(0,128,280,128)` ⇒ x ∈ [−140,140]、y ∈ [64,192]（spec §1 末行）。
 * 出生在框中央（ECL 到位后由脚本自己在框里飘；占位惰性壳不动 ⇒ 恒在中央）。 */
static uint8_t *ufo_body_spawn(int32_t color)
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
    *(int32_t *)(spawn + CE_SPAWN_LIFE) = ufo_clamp_spawn_life(UFO_HP_PLACEHOLDER);
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
        return;                                        /* spawn 里已经摆好位置 */
    }
    ce_anm_set_pos(s->body_anm_id, *(const float *)(e + CE_ENEMY_POS_X),
                                   *(const float *)(e + CE_ENEMY_POS_Y), 0.0f);
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

/* ================================ §3.4 伪磁场：一颗道具 ================================
 * 返回 1 = 本帧真的动了它（吸掉或拉了一步），0 = 没碰（栅栏 / 早停）。 */
static int ufo_try_item(ufo_card_t *s, uint8_t *it, float ux, float uy, float px, float py)
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
        /* vel 也清掉（同一个槽下帧可能被复用；残留 vel 会立刻把新道具甩走）*/
        ivel[0] = 0.0f; ivel[1] = 0.0f; ivel[2] = 0.0f;
        ++s->items_pulled;
        if (ufo_absorb_item(&s->u, *(uint32_t *)(it + CE_ITEM_TYPE), UFO_PLACEHOLDER_STAGE,
                            UFO_FILL_SCALE_PERMILLE)) {
            /* 吸满：吐一件奖励（一手颜色 → 一件；蓝不吐；彩虹吐令牌 ⇒ TH18 无对应 type） */
            int32_t reward = ufo_spit_reward(s->u.color);
            ++s->bursts;
            ce_play_sound(UFO_SE_FULL, ipos[0]);
            if (reward) {
                float rp[3];
                rp[0] = ux; rp[1] = uy; rp[2] = 0.0f;
                /* 一手参数：角度 −π/2、速度 2.2（`th12:0x44aca0` 的奖励 / `0x446f40` 的 main-drop 同参）*/
                if (ce_spawn_item((uint32_t)reward, rp, -1.5707964f, 2.2f)) ++s->rewards_spit;
                else ce_log("ufo: reward spit type %d failed (item pool full?)", reward);
            } else {
                ce_log("ufo: fill full (color %d) - nothing spat (blue has no case in TH12's switch; "
                       "rainbow spits a token which TH18 has no item type for)", (int)s->u.color);
            }
        } else {
            ce_play_sound(UFO_SE_ABSORB, ipos[0]);
        }
        return 1;
    }

    /* 拉拽：先算目标点。栅栏①②（屏内 / 离自机 > 30 px）是**针对目标点**的 ——
     * 一手依据见文件头 ②③：目标点出屏 ⇒ 引擎当帧删；离自机 30 px 内 ⇒ 引擎当帧收掉。
     * ⚠ 这里宁可「本帧不拉」（记账 `skipped_guard`）也不把道具钉在屏边：钉住反而会被删。 */
    speed = ufo_drag_speed(s->u.frames);
    nx = ipos[0];
    ny = ipos[1];
    ufo_drag_step(&nx, &ny, ux, uy, speed);
    if (!ufo_step_allowed(nx, ny, px, py)) {
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
static void ufo_absorb_tick(ufo_card_t *s)
{
    uint8_t *mgr = CE_ITEM_MANAGER();
    uint8_t *em = CE_ENEMY_MGR();
    uint8_t *e;
    float ux, uy, px, py;
    unsigned i;

    if (!mgr || !em) return;
    e = ufo_enemy_by_id(s->u.ufo_id);
    if (!e) return;                                    /* 本体没了（飞走 / 被清）⇒ 交给 finish 分支 */
    ux = *(float *)(e + CE_ENEMY_POS_X);
    uy = *(float *)(e + CE_ENEMY_POS_Y);
    ufo_player_xy(&px, &py);

    for (i = 0; i < CE_ITEM_SLOTS; ++i) {
        uint8_t *it = mgr + CE_ITEM_POOL_OFF + (size_t)i * CE_ITEM_STRIDE;
        int32_t st = *(int32_t *)(it + CE_ITEM_STATE);
        if (st == 0) continue;                         /* 早停：空槽（最常见）*/
        /* 早停：先读 type（一次访存），不是 1/2 立刻走 —— 满配时全场大部分槽都在这条上出局 */
        if (ufo_absorb_type_index(*(uint32_t *)(it + CE_ITEM_TYPE)) < 0) continue;
        if (!ufo_absorbable(*(uint32_t *)(it + CE_ITEM_TYPE), st, s->u.ufo_id, 1)) continue;
        ufo_try_item(s, it, ux, uy, px, py);
    }
}

/* ================================ §3.4 令牌 → 槽（吃 / 记）=============================== */
/* 令牌被自机「吃到」：spec §3.2 ⏳ ①「令牌自己就是实体（接触即入槽）还是莉莉白式携带者」**未拍板**
 * ⇒ 本实装取前者（接触即入槽：占位期实体不飘不爆，接触是唯一可靠的交互）。
 * 一手没有「玩家接触敌人的通用事件」（`EVENTS-CONTRACT.md` 的事件表里没有），所以这里用距离：
 * 20 px（★❓ 自定；一手只有「收点 30 px」与「吸收 18 px」两个半径，令牌不是道具、不该用它们）。 */
#define UFO_TOKEN_PICK_DIST 20.0f

static int ufo_tok_pick(ufo_card_t *s, float tx, float ty, float px, float py)
{
    float dx = tx - px, dy = ty - py;
    (void)s;
    return dx * dx + dy * dy <= UFO_TOKEN_PICK_DIST * UFO_TOKEN_PICK_DIST;
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
static int ufo_token_to_slot(ufo_card_t *s, int32_t color)
{
    int32_t dropped = 0;
    int put;

    if (!ufo_color_valid(color)) return UFO_PUT_NONE;
    /* ⚠ 槽里**已经**有一个干等着的组（上一帧被「场上已有 UFO」挡下、或者 UFO 刚走而没人叫召唤）：
     * 这一颗令牌**不消费**它（`ufo_group_put` 会原样报 READY）⇒ 不播音、不计数，纯粹当「再叫一次」。
     * 音效 / `taken_tokens` 只记**真的进了槽**的那些（否则每 5 秒白响一声、统计也虚高）。*/
    if (ufo_present_group_ready(s)) return UFO_PUT_READY;
    ++s->taken_tokens;
    ce_play_sound(UFO_SE_TOKEN, 0.0f);                 /* 声像 = 屏心（实体坐标在接触点，取 0 更稳）*/
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

/* 入队。返回 0 = 环满（调用方记账；此时那个令牌的颜色只能丢）。**死亡栈里可安全调**（只动我们的表）。 */
static int ufo_pending_put(ufo_card_t *s, int32_t color)
{
    if (s->pending_n >= UFO_PENDING_CAP) return 0;
    s->pending_color[s->pending_n++] = color;
    s->pending_token = 1;
    return 1;
}

/* 出队（空 ⇒ 返回 `UFO_COLOR_NONE`）。 */
static int32_t ufo_pending_take(ufo_card_t *s)
{
    int32_t c;
    int i;
    if (s->pending_n == 0) {
        s->pending_token = 0;
        return UFO_COLOR_NONE;
    }
    c = s->pending_color[0];
    for (i = 0; i + 1 < s->pending_n; ++i) s->pending_color[i] = s->pending_color[i + 1];
    --s->pending_n;
    if (s->pending_n == 0) s->pending_token = 0;
    return c;
}

/* 读走并撤掉一个令牌实体（**先取值再清条目** —— 顺序反了就把颜色读成 NONE）。
 * 死亡栈里也能安全调（只置 `flags_lo` 位 25 + 改我们自己的表，不析构任何东西）。 */
static int32_t ufo_tok_take_and_kill(int slot, ufo_tok_t *t)
{
    int32_t color = t->color;
    ufo_tok_kill(slot, t);
    return color;
}

/* 召唤（**唯一入口** —— 只有这一处调 `ce_spawn_enemy` 建本体）。
 * ⚠ 两个调用点都必须是**非死亡栈**：`on_tick_2`（帧末）与 `ufo_tokens_tick`（`on_item_frame` 内）。
 *   `on_enemy_death` 里**只登记**（`pending_token`），由 `on_tick_2` 兑现 —— 见 `DEATH` §2.4。 */
static void ufo_try_summon(ufo_card_t *s)
{
    int32_t made;
    uint8_t *body;

    if (!ufo_present_group_ready(s)) return;
    if (!ufo_can_spawn(&s->u)) {
        /* 场上已经有一个 UFO（单例，一手管理器 = 一个 `zUfo`）。组**留在槽里**，
         * 等它没了之后下一次入槽会用 `UFO_PUT_READY` 把它兑现出来。 */
        return;
    }
    made = ufo_group_consume(&s->grp);                 /* 成组才返回非 NONE，组就此消费掉 */
    if (made == UFO_COLOR_NONE) return;
    body = ufo_body_spawn(made);
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
    if (!ufo_spawn(&s->u, *(uint32_t *)(body + CE_ENEMY_ID), made)) {
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
    /* ★ 画面：起本体的**跟随 VM**（`ability.anm` 164..167；`made` 是合成色，金落蓝那一格）。
     * ⚠ 放在 `ufo_spawn()` **成功之后**：所有失败 / 孤儿路径都在它上面返回 ⇒
     *   绝不会出现「没有本体却有 VM」的幽灵。位置在 `ufo_body_vm_spawn` 里当场摆好。 */
    ufo_body_vm_spawn(s, made, body);
    /* ★ 日志带 `art idx`：用户裁决 ① 说「金 = 蓝的调色版」，这一格就是证据 ——
     *   金（4）打出来必须是 **1**（= 蓝那一格，`ufo_color_art_index`），实跑一眼可核。 */
    ce_log("ufo: summoned UFO (color %d, art idx %d%s) enemy_id %u at frame %u", (int)made,
           ufo_color_art_index(made), ufo_color_is_gold(made) ? " = gold, uses the BLUE art set" : "",
           s->u.ufo_id, s->play_frame);
}

/* 帧末跑一遍令牌表：① 实体还在不在 ② 死亡事件解到的颜色 ③ TTL ④ 自机接触。 */
static void ufo_pentagon_hint_update(ufo_card_t *s);   /* 定义在下面（算判据 + 驱动框 VM，见它的注释块）*/
static void ufo_tokens_tick(ufo_card_t *s)
{
    float px, py;
    int i;
    ufo_player_xy(&px, &py);
    for (i = 0; i < UFO_TOKEN_SLOTS; ++i) {
        ufo_tok_t *t = &s->tok[i];
        uint8_t *e;
        int32_t color;

        if (t->color == UFO_COLOR_NONE) continue;
        e = ufo_enemy_by_id(t->enemy_id);
        if (!e) {
            /* 实体没了，却没走过死亡事件 ⇒ 过场全清 / 重开（`DEATH` §1.4 的「静默消失」那三类）
             * ⇒ 直接退休，**不入槽**。★ 这条分支**绕过了 `ufo_tok_kill()`**（直接清 `color`）
             * ⇒ 跟随 VM 必须在这里**显式**收掉，否则场上来一个没有实体的幽灵令牌。 */
            ufo_tok_vm_kill(i, t);
            ++s->lost_tokens;
            t->color = UFO_COLOR_NONE;
            t->enemy_id = 0;
            t->ttl = 0;
            continue;
        }
        /* ★ 画面：跟随 VM 每帧抄实体的 `abs_pos`（放在 TTL / 成组早停**之前** ——
         *   令牌只要还在场上就得跟着走，包括「槽里有成组、这一颗暂时不收」那段等待期）。 */
        ufo_tok_vm_follow(t, e);
        /* ★ A8：彩虹令牌的三段中断 + 换色（60 px 闩 / 90 帧起闪 / 150 帧换色）。
         * ⚠ 也放在早停之前：计时器与玩家距离无关地每帧要走（一手在道具的 tick 里，不受「槽满了没」影响）。 */
        ufo_tok_rainbow_tick(t, e, px, py);
        if (t->ttl) --t->ttl;
        if (!t->ttl) {                                 /* 占位期惰性壳不会自己消失 ⇒ TTL 兜底 */
            ufo_tok_kill(i, t);
            ++s->lost_tokens;
            continue;
        }
        color = t->color;
        /* ⚠ 槽里**已经**有一个干等着的组（上一帧被「场上已有 UFO」挡下、或刚被打死一个本体）：
         * 这一颗**不收也不撤** —— 收了也只是白吃一颗 + 白响一声（`ufo_group_put` 会原样报 READY）。
         * 让它留在场上，等组被消费掉之后再接触入槽。 */
        if (ufo_present_group_ready(s)) continue;
        if (!ufo_tok_pick(s, *(float *)(e + CE_ENEMY_POS_X), *(float *)(e + CE_ENEMY_POS_Y), px, py))
            continue;
        /* 接触即入槽（spec §3.2 ⏳① 取这支）：**先取值、再撤实体**（顺序见 `ufo_tok_take_and_kill`）*/
        color = ufo_tok_take_and_kill(i, t);
        /* ⚠ 这里不是死亡栈（`on_item_frame` 是每游玩帧的门卫位次）⇒ 可以立刻建本体；
         *   死亡事件那条路只登记、由 `on_tick_2` 兑现。 */
        if (ufo_token_to_slot(s, color) == UFO_PUT_READY) ufo_try_summon(s);
    }
    ufo_pentagon_hint_update(s);
}

/* ★★ **五边形框**（用户 2026-09-12 裁决 ③）—— 每帧算条件 **并驱动框 VM**（2026-09-12 A7 实装）----
 * 触发条件（用户原话）：「玩家手上有**两个令牌**（= 再来一枚就能成组）时，屏幕上**这类令牌**外面
 * 会有一圈五边形框作提示」。⇒ 两级判据（都在纯逻辑层、有单测）：
 *   ① `ufo_group_pentagon_hint(&s->grp)`：槽里此刻是不是「差一枚就成组」（便宜的早停；
 *      当前规则下等价于「槽里正好 2 枚」—— 4 个颜色桶里任意两枚都补得出组，见 ufo_core.h 的注）。
 *      ★ 这一条同时**涵盖了零售那条「已收满 3 枚就不提示」**（`th12:0x42658a cmp [0x4b0c58],3`）：
 *      判据要求 `filled == 2`，槽满 3 枚必然为假。
 *   ② `ufo_pentagon_hint_for_color(&s->grp, t->color)`：**场上的这一枚**收了就能成组 ⇒ 它才该被画框
 *      （框的颜色 = 收了它会得到的那只 UFO 的颜色，见 `ufo_pentagon_label_for_color()`）。
 * 本函数把②的结果逐令牌记进 `ufo_tok_t.pentagon_hint`，并**照零售的生命周期**驱动框 VM：
 *   · 条件成立 + 还没有框 ⇒ **建框 + 发一次标签**（`ufo_tok_penta_spawn`）；
 *   · 条件成立 + 已经有框 ⇒ **什么都不做**（零售 `0x4265d5` 的 `jne`：不重建、不重发标签）；
 *   · 条件不成立 ⇒ **删框**（零售那六处 case 的 else 支：`call 0x461a70` + 句柄清 0）；
 *   · 每一帧都把框移到令牌实体坐标上（`ufo_tok_penta_follow`；零售不跟随，见它的注）。
 * ✅ 美术来源 = **零售**（`bullet.anm` script211 逐条照抄 ⇒ 脚本 223，程序画的五边形、无贴图）；
 *    **不用 `drawRect` 硬凑**（用户明确要求）。
 * ⚠ 调用点必须**不是死亡栈**（本函数会 `ce_anm_spawn`）—— 现在只有 `ufo_tokens_tick()` 一处，
 *   那是 `on_item_frame` 的门卫位次（每游玩帧），合规。 */
static void ufo_pentagon_hint_update(ufo_card_t *s)
{
    int i, any = 0;

    if (!ufo_group_pentagon_hint(&s->grp)) {
        /* 早停：槽里不是 2 枚（或某种颜色都补不出组）⇒ 全部清零 + **把框全收了**
         * （零售：条件一不成立就删框，不留一个孤零零的五边形在场上）。 */
        for (i = 0; i < UFO_TOKEN_SLOTS; ++i) {
            s->tok[i].pentagon_hint = 0;
            ufo_tok_penta_kill(i);
        }
        s->pentagon_any = 0;
        s->pentagon_logged = 0;                        /* 条件消失后允许再报一次 */
        return;
    }
    for (i = 0; i < UFO_TOKEN_SLOTS; ++i) {
        ufo_tok_t *t = &s->tok[i];
        uint8_t *e;
        int label;

        if (t->color == UFO_COLOR_NONE) { t->pentagon_hint = 0; ufo_tok_penta_kill(i); continue; }
        label = ufo_pentagon_label_for_color(&s->grp, t->color);
        t->pentagon_hint = (uint8_t)(label ? 1 : 0);
        if (!label) { ufo_tok_penta_kill(i); continue; }
        e = ufo_enemy_by_id(t->enemy_id);
        /* 实体没了：**不建框**（实体退休那两条路在上一趟已经 `ufo_tok_vm_kill()` 收过句柄了）*/
        if (!e) continue;
        ufo_tok_penta_follow(i, e);                    /* 先跟随（顺带自愈掉死句柄）*/
        ufo_tok_penta_spawn(i, e, label);              /* 没框才建；有框则原样留着*/
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

/* ================================ §3.5 击破 / 飞离 / 共用 finish ================================ */
static void ufo_teardown(ufo_card_t *s, const char *why)
{
    if (!ufo_present(&s->u)) return;
    ce_log("ufo: finish (%s) - color %d, %d P + %d money absorbed (fill %d.%03d), frame %u",
           why ? why : "?", (int)s->u.color, s->u.cnt_p, s->u.cnt_money,
           s->u.fill_millionths / UFO_FILL_UNIT,
           (s->u.fill_millionths % UFO_FILL_UNIT) / 1000, s->play_frame);
    /* 照 TH12 `Ufo::on_tick` 的结束分支（`UFO:104-107`）：清计数 / 停循环音 / 删光环 VM / 血条淡出。
     * ⚠ 本卡现在**没有**光环 VM、**没有**血条 VM、**没有**循环音（资产与那两条 API 都还没到位，
     *   spec §3.5 ⏳ / §9 #11）⇒ 这一半是**空实现**，不是「忘了写」。清单见文件尾。
     * ★ 但**本体的跟随 VM 现在有了**（`body_anm_id`）⇒ 这一支就是它的收尾点（`ufo_hud_kill` 的同类）：
     *   击破 / 飞走 / 过场收场 / 删卡都汇到 `ufo_withdraw_all` → 这里。 */
    ufo_body_vm_kill(s);
    ufo_finish(&s->u);                                 /* 纯逻辑侧：清 id / 计数 / fill */
}

/* 击破：开奖（赏金分 + 颜色加成道具）+ 收尾。 */
static void ufo_break(ufo_card_t *s)
{
    int64_t bounty = ufo_break_bounty_score(&s->u);
    int32_t bonus = ufo_break_color_bonus(s->u.color);
    float rp[3];
    int32_t color = s->u.color;

    ++s->ufos_broken;
    ce_play_sound(UFO_SE_BREAK, s->last_x);
    /* ★ 击破分数：一手公式**仓里没有**（spec §9 #5）⇒ `ufo_break_bounty_score` 现在恒 0
     *（占位权重 `UFO_BOUNTY_SCORE_PER_*` = 0 = 「不做」）。这里照实加，不为好看编数。 */
    if (bounty) ce_add_score_delta(bounty);
    if (bonus) {
        rp[0] = s->last_x; rp[1] = s->last_y; rp[2] = 0.0f;
        /* 一手击破掉落的参数：角度 −π/2、速度 2.2（`th12:0x44af00`；`UFO:256-259`）*/
        if (ce_spawn_item((uint32_t)bonus, rp, -1.5707964f, 2.2f)) {
            ++s->rewards_spit;
            ce_play_sound(UFO_SE_ITEM_BREAK, s->last_x);
        } else {
            ce_log("ufo: break bonus type %d failed (item pool full?)", bonus);
        }
    }
    /* ★ 赏金道具（一手 16/17/18 三个 type）：TH18 侧**一个都不能用**（16 = 0x10 是 EXTEND2 卡道具、
     * 17/18 超出 type 上限 0x13；`UFO_BOUNTY_TYPES_USABLE == 0`）⇒ 不生成，只留日志。 */
    if (ufo_bounty_item_type(&s->u))
        ce_log("ufo: bounty item type %d skipped - no usable TH18 item type (one-hand types are "
               "16/17/18; 16 is the EXTEND2 card item, 17/18 are above the 0x13 limit)",
               (int)ufo_bounty_item_type(&s->u));
    ce_log("ufo: BREAK color %d - %d P + %d money absorbed, bounty score %lld (placeholder weight 0), "
           "colour bonus item %d", (int)color, s->u.cnt_p, s->u.cnt_money, (long long)bounty, (int)bonus);
    ufo_teardown(s, "broken");
}

/* ==================== HUD 第一版（spec §3.1 末行 / §5 / §9 #8；★ 用户 2026-09-12 裁决 ④）============
 * **真的认领左下角货架的槽并画出来**（之前是「故意不认领」—— 那条纪律已被用户裁决 ④ 取代）。
 * 三块元素 + 一行读数，全部落在**用户 2026-09-12 的 HUD 硬规则**上（见文件头常量区的长注）：
 *   ① 三格令牌槽：脚本 104（1×1 `drawRect` 模板，`layer(25)`），每格颜色 = 该槽的颜色（金显示成蓝）
 *   ② 本体血条：同脚本 104 两条（底 + 填充），宽度 = 本体 HP 的千分比（`ufo_hp_permille`）
 *   ③ 一行读数：`ce_hud_vm_text_*`（脚本 136 字形，`layer(25)`）—— 有 UFO 时显示填充率 `NN%`，
 *      没有时显示 `-`（字库只认 `0-9 . % + -` 与空格，见 `sdk.h` 的 `CE_HUD_VM_TEXT_MAX` 注）
 * ★ 为什么不画原版那三件（横幅 79 / 大横幅 82 / 收集闪光 81）：**一张都没切图**（spec §5）⇒
 *   这一版是 `drawRect` 拼的**占位**，**不冒充复刻**（照 `signal.c` 的先例）。⏳ 见文件尾清单。
 * ⚠ 层 25 = 与暂停模糊罩同一个绘制组（38）、罩子是暂停那一刻才生成的 ⇒ 我们的 HUD 会被罩住；
 *   **不许**换到 26+，也**不许**改用 ASCII 文字（`ce_hud_text*` 结构上不可能被罩住）。 */

static uint32_t ufo_hud_rect_spawn(void)
{
    static const float origin[3] = { 0.0f, 0.0f, 0.0f };
    return ce_anm_spawn_ui(CE_ABILITY_ANM(), CE_ANM_ABILITY_SCRIPT_STOCK_RECT, origin);
}

/* 摆一个矩形：入参是**左上角**（表面 px）。脚本 104 没写 `anchor` ⇒ 按 `AUDIT.md` §AL20
 * （2026-09-10 两次实跑）**矩形默认锚 = 左上角**；`UFO_HUD_RECT_ANCHOR_CENTER` = 1 时改成图心锚
 * （⚠ `signal.c` / `stock.c` 是按图心算的；实跑若发现整体偏半个尺寸，翻这一个宏）。 */
static void ufo_hud_rect(uint32_t id, float x0, float y0, float w, float h, uint32_t bgra)
{
    if (!id) return;
    if (w < 0.0f) w = 0.0f;
    if (h < 0.0f) h = 0.0f;
    ce_anm_set_scale(id, w, h);
#if UFO_HUD_RECT_ANCHOR_CENTER
    ce_anm_set_pos(id, x0 + w * 0.5f, y0 + h * 0.5f, 0.0f);
#else
    ce_anm_set_pos(id, x0, y0, 0.0f);
#endif
    ce_anm_set_color(id, bgra);
}

/* 槽里的颜色 → 显示色。★ 金显示成蓝那一档（用户裁决 ①：金 = 蓝的调色版，等 P 图）。 */
static uint32_t ufo_hud_color_of(int32_t color)
{
    switch (color) {
    case UFO_COLOR_RED:     return UFO_HUD_COLOR_RED;
    case UFO_COLOR_BLUE:    return UFO_HUD_COLOR_BLUE;
    case UFO_COLOR_GREEN:   return UFO_HUD_COLOR_GREEN;
    case UFO_COLOR_RAINBOW: return UFO_HUD_COLOR_RAINBOW;
    case UFO_COLOR_GOLD:    return UFO_HUD_COLOR_GOLD;    /* ★ 与蓝同色 */
    default:                return UFO_HUD_COLOR_EMPTY;
    }
}

/* 收干净本卡的 HUD VM。**长持续效果铁律**：菜单门（`on_draw` 门关着时）、`on_run_reset`、
 * `on_delete` 三处都要调 —— 引擎会在别处摧毁这些 VM，所以这里按「有就删、删完清句柄」写，
 * 幂等（第二次进来句柄已经是 0）。 */
static void ufo_hud_kill(void)
{
    int i;
    for (i = 0; i < UFO_SLOT_COUNT; ++i) {
        if (s_cell_id[i]) { ce_anm_delete(s_cell_id[i]); s_cell_id[i] = 0; }
    }
    if (s_bar_bg_id) { ce_anm_delete(s_bar_bg_id); s_bar_bg_id = 0; }
    if (s_bar_fill_id) { ce_anm_delete(s_bar_fill_id); s_bar_fill_id = 0; }
    ce_hud_vm_text_clear(&s_hud_text);
}

/* 起/补 VM（幂等 + 自愈：引擎可能在别处摧毁它们；照 `signal.c` 的写法每帧查一次，
 * 5 次 `ce_anm_get_vm` 的开销可忽略）。生成顺序 = 叠放顺序（同层后生成的画在上面）：
 * 三格先生成、血条底与填充后生成 ⇒ 血条压在格子上方（两块本来不重叠，只是为了确定性）。 */
static void ufo_hud_ensure(void)
{
    int i;
    for (i = 0; i < UFO_SLOT_COUNT; ++i)
        if (!s_cell_id[i] || !ce_anm_get_vm(s_cell_id[i])) s_cell_id[i] = ufo_hud_rect_spawn();
    if (!s_bar_bg_id || !ce_anm_get_vm(s_bar_bg_id)) s_bar_bg_id = ufo_hud_rect_spawn();
    if (!s_bar_fill_id || !ce_anm_get_vm(s_bar_fill_id)) s_bar_fill_id = ufo_hud_rect_spawn();
}

static void ufo_hud_draw(ufo_card_t *s)
{
    ce_hud_slot_t slot;
    uint8_t *body;
    char buf[CE_HUD_VM_TEXT_MAX + 1];
    float x0, y0, x640, text_x;
    int i, permille;

    if (!s) return;
    /* 槽坐标：`get` 给的是**认领那一刻**的值 ⇒ 每帧取（同一开场批次里后来的认领会重排）。
     * ⚠ `ce_hud_slot_get()` 只填 x/y/w/h/side、**不填 x640/y640**（只有认领时才填）⇒
     *   直接读 `slot.x640` 是未初始化的栈值；这里按 SDK 的口径自己换算（`signal.c` 同一处教训）。 */
    if (!ce_hud_slot_get(UFO_ID_NUM, &slot)) return;   /* 还没认领到（开场批次未跑完）*/
    x640 = slot.x * CE_HUD_SURFACE_TO_640;

    ufo_hud_ensure();

    if (!s->logged_no_hud) {
        s->logged_no_hud = 1;
        ce_log("ufo: HUD v1 up - 3 token cells %0.fx%0.f + HP bar %0.fx%0.f + one vm-text line, block "
               "%.0fx%.0f at surface (%.0f, %.0f), all on UI layer 25 (script 104 rect / 136 glyph)",
               (double)UFO_HUD_CELL_W, (double)UFO_HUD_CELL_H, (double)UFO_HUD_BLOCK_W,
               (double)UFO_HUD_BAR_H, (double)UFO_HUD_BLOCK_W, (double)UFO_HUD_BLOCK_H,
               (double)slot.x, (double)slot.y);
    }

    /* 块的**左上角**（表面 px）—— 槽给的是块左下角（y 是下边缘，TH 坐标 y 向下增长）。 */
    x0 = slot.x;
    y0 = slot.y - UFO_HUD_BLOCK_H;

    /* ① 三格令牌槽：每格画成纯色方块 ⇒ 「哪一格是什么色」一眼可见（空格子 = 暗底）*/
    for (i = 0; i < UFO_SLOT_COUNT; ++i) {
        float cx = x0 + (UFO_HUD_CELL_W + UFO_HUD_CELL_GAP) * (float)i;
        ufo_hud_rect(s_cell_id[i], cx, y0, UFO_HUD_CELL_W, UFO_HUD_CELL_H,
                     ufo_hud_color_of(s->grp.slot[i]));
    }

    /* ② 本体血条：底（整块宽）+ 填充（宽 = HP 千分比 × 块宽）。
     *    HP 当前值 = `zEnemyData.life.current` = `enemy+0x6220`（一手：`animal.c` 的
     *    `ANIMAL_ENEMY_LIFE_CUR` / `lolk.c` 的 `CE_ENEMY_LIFE_CUR` 同址），
     *    上限 = 我们 spawn 时写进去的那个 life（`ufo_clamp_spawn_life(UFO_HP_PLACEHOLDER)`）。 */
    permille = 0;
    body = ufo_enemy_by_id(s->u.ufo_id);
    if (body)
        permille = ufo_hp_permille(*(const volatile int32_t *)(body + UFO_ENEMY_LIFE_CUR),
                                   ufo_clamp_spawn_life(UFO_HP_PLACEHOLDER));
    ufo_hud_rect(s_bar_bg_id, x0, slot.y - UFO_HUD_BAR_H, UFO_HUD_BLOCK_W, UFO_HUD_BAR_H,
                 UFO_HUD_COLOR_BAR_BG);
    ufo_hud_rect(s_bar_fill_id, x0, slot.y - UFO_HUD_BAR_H,
                 UFO_HUD_BLOCK_W * (float)permille / 1000.0f, UFO_HUD_BAR_H,
                 UFO_HUD_COLOR_BAR_FILL);

    /* ③ 一行读数：有 UFO ⇒ 吸满进度 `NN%`；没有 ⇒ `-`（字库只认 0-9 . % + - 与空格）。
     *    坐标 = 640 空间（内部 × `DAT_0056aca0`），y 取**块顶**（垂直对齐还受引擎 ASCII 环境的
     *    `align_v` 影响 ⇒ 实跑可能要挪一两 px，改 `UFO_HUD_TEXT_SCALE` / 这个 y 即可）。 */
    if (body) snprintf(buf, sizeof buf, "%d%%", permille / 10);
    else { buf[0] = '-'; buf[1] = '\0'; }
    text_x = x640 + (UFO_HUD_CELL_W * (float)UFO_SLOT_COUNT
                     + UFO_HUD_CELL_GAP * (float)(UFO_SLOT_COUNT - 1) + UFO_HUD_TEXT_DX)
                    * CE_HUD_SURFACE_TO_640;
    ce_hud_vm_text_set(&s_hud_text, text_x, y0 * CE_HUD_SURFACE_TO_640, UFO_HUD_COLOR_TEXT,
                       CE_ASCII_ALIGN_LEFT, UFO_HUD_TEXT_SCALE, buf, "ufo112");
}

/* ================================ 回调 ================================ */
static int on_ctor(ce_card_t *c)
{
    ufo_card_t *s = ce_state(c, ufo_card_t);
    if (!s) return 0;
    /* ★ 先收本体的跟随 VM **再** memset（本轮新增）：句柄就存在这块状态里 ⇒
     * 只 memset 的话句柄没了、VM 却还留在场上（长持续效果铁律里最典型的那种漏）。
     * ★ 令牌的 16 个句柄同理 —— 一起收（`ufo_tok_vm_kill_all` 只看句柄、不看 `color`）。
     * 正常路径上这一步已经是无操作（`ufo_withdraw_all` 先收过一遍），这是防御，不是主路径。 */
    ufo_body_vm_kill(s);
    ufo_tok_vm_kill_all(s);
    memset(s, 0, sizeof *s);
    ufo_group_reset(&s->grp);
    ufo_reset(&s->u);
    s->pending_n = 0;                                  /* memset 已经把环清空，这里只明确语义 */
    s->pending_drop = 0;
    s->pending_token = 0;
    s->next_token_at = UFO_TOKEN_SPAWN_FIRST;          /* 开局先等一会儿再出第一个令牌 */
    return 0;
}

static int on_load(ce_card_t *c)
{
    ce_hud_block_t blk;

    on_ctor(c);
    /* ★ HUD 槽**认领**（用户 2026-09-12 裁决 ④）：用**带关卡纪元**的入口
     * `ce_hud_slot_claim_card()` ⇒ 关卡号一变就先清掉上一关的占位再认领（HUD-LAYOUT §4.4），
     * 同一关内（局中删卡）一笔都不重排；认领**幂等**（`on_load` 每关都会再来一次）。
     * 块尺寸自定（❓ 见文件头常量区），`side = 左下`（底部货架，按 card_id 排序 ⇒ 确定性）。 */
    memset(&blk, 0, sizeof blk);
    blk.w = UFO_HUD_BLOCK_W;
    blk.h = UFO_HUD_BLOCK_H;
    blk.side = CE_HUD_SIDE_LEFT;
    if (!ce_hud_slot_claim_card(UFO_ID_NUM, &blk, 0)) {
        ce_log("ufo: ce_hud_slot_claim_card(%u) FAILED - HUD block %.0fx%.0f not claimed "
               "(HUD-LAYOUT §3)", UFO_ID_NUM, (double)blk.w, (double)blk.h);
    } else {
        ce_hud_slot_t probe;
        if (ce_hud_slot_get(UFO_ID_NUM, &probe))
            ce_log("ufo: HUD block %.0fx%.0f claimed at surface (%.0f, %.0f) "
                   "(= 640 space (%.1f, %.1f))", (double)blk.w, (double)blk.h,
                   (double)probe.x, (double)probe.y,
                   (double)(probe.x * CE_HUD_SURFACE_TO_640),
                   (double)(probe.y * CE_HUD_SURFACE_TO_640));
    }
    return 0;
}

/* 把本卡留在场上的真敌人全部静默撤掉（**不析构**：可能正在别人的链表遍历里 ⇒ 只置位，sweep 收）。
 * 令牌与 UFO 都是真敌人 ⇒ 换关 / 局末 / 删卡三条路都要走它（METHOD 铁律）。
 * ★ 跟随 VM **也在这里收**（本轮新增）：删卡走的是 `on_delete`（虚表 `+0x50`），
 *   那一刻 tick 回调**全停** ⇒ 「每帧跟随时顺手收」这条路根本不会跑。
 *   ⇒ 这里**无条件**收两遍句柄（本体一个 + 令牌 16 个，都只看句柄、不看 `color`）：句柄非 0
 *     就是还欠引擎一个 VM（`ufo_tok_kill` 里也收一份，这里那遍对它是无操作的兜底）。 */
static void ufo_withdraw_all(ufo_card_t *s, const char *why)
{
    int i, n = 0;
    ufo_body_vm_kill(s);
    ufo_tok_vm_kill_all(s);                            /* ★ 只看句柄（`color` 还没清）*/
    for (i = 0; i < UFO_TOKEN_SLOTS; ++i)
        if (s->tok[i].color != UFO_COLOR_NONE) { ufo_tok_kill(i, &s->tok[i]); ++n; }
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
    /* 换关：令牌与 UFO 都不跨关（一手 TH12 的管理器每关重来）。引擎自己也会在换关时
     * `EnemyManager__destroy_all` 清掉真敌人（`DEATH` §1.4：那条路**不报事件** —— 对我们正好），
     * 但那发生在关卡结束时点；这里**主动置位**撤干净，免得在本关末尾多留几个实体。 */
    ufo_withdraw_all(s, "stage start");
    on_ctor(c);
}

/* ★ 共享死亡事件（spec §7 / DEATH §4 表）：只认 `KILLED` / `BOMB_KILL`，**必须排除 `KILL_ALL`**
 *（过场清场会把计数器刷满）。回调里**禁止**析构任何敌人、**禁止**缓存 `ev->enemy`。 */
static void on_enemy_death(ce_card_t *c, const ce_enemy_death_t *ev)
{
    ufo_card_t *s = ce_state(c, ufo_card_t);
    ufo_tok_t *t;
    if (!s || !ev) return;
    (void)c;
    /* ① 令牌实体被打死 ⇒ 入槽（令牌「自己就是实体」那一支）。
     * ⚠ `KILL_ALL`（过场清场）一律不算：`ufo_token_kill_counts` 已经挡掉（spec §7 AUDIT 硬要求）。
     * ⚠ 回调体里**禁止析构任何敌人**、**禁止缓存 `ev->enemy`**（`DEATH` §2.4）——
     *   这里只改我们自己的表 + 登记，不动任何敌人对象。 */
    t = ufo_tok_slot(s, ev->id);
    if (t && ufo_token_kill_counts(ev->kind)) {
        /* **只登记**：出槽这件事本身是纯逻辑（改我们的表），可以当场做；
         * 但「出槽后要不要召唤」必须先登记、帧末才动手 —— 死亡栈里**禁止新建敌人**（重入）。
         * ⚠ `t` 来自 `s->tok[]` ⇒ 槽号 = 指针相减（五边形框句柄在**文件静态数组**里、按槽号存，
         *   所以这里要多带一个槽号 —— A7；收 VM 只标删、不析构，死亡栈上做是安全的）。 */
        if (!ufo_pending_put(s, ufo_tok_take_and_kill((int)(t - s->tok), t))) {
            ++s->pending_drop;
            ce_log("ufo: pending token ring full (cap %d) - a token colour was dropped "
                   "(expected never to happen: at most UFO_TOKEN_MAX = %d tokens are alive at once, "
                   "so at most that many can die in one sweep)", UFO_PENDING_CAP, UFO_TOKEN_MAX);
        }
        return;
    }
    /* ② UFO 本体被打死 ⇒ 击破（记下位置；开奖放帧末 —— 与飞离分支共用同一段，顺序也更清楚）*/
    if (ev->id == s->u.ufo_id && ufo_body_kill_counts(ev->kind)) {
        s->last_x = ev->x;
        s->last_y = ev->y;
        s->pending_break = 1;
        return;
    }
}

static void on_item_frame(ce_card_t *c)
{
    ufo_card_t *s = ce_state(c, ufo_card_t);
    if (!s) return;
    ++s->play_frame;
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
    /* 令牌出现在这一帧？（节流 + 上限；占位期惰性壳不会自己消失 ⇒ 上限 + TTL 都得有）*/
    if (s->play_frame >= s->next_token_at &&
        ufo_tok_live_count(s) < UFO_TOKEN_MAX) {
        /* 落点：一手活动框（`UFO_FRAME_*`）× 确定性散列（不调 `ce_rand()`）*/
        float x = (ufo_hash_unit(s->token_serial, 0x112u) * 2.0f - 1.0f) * UFO_FRAME_HALF_W;
        float y = UFO_FRAME_Y_TOP +
                  ufo_hash_unit(0x112u, s->token_serial) * (UFO_FRAME_Y_BOTTOM - UFO_FRAME_Y_TOP);
        if (ufo_tok_spawn(s, x, y))
            s->next_token_at = s->play_frame + UFO_TOKEN_SPAWN_INTERVAL;
        else
            s->next_token_at = s->play_frame + UFO_TOKEN_SPAWN_RETRY;   /* 失败别每帧重试 */
    }
    /* 伪磁场（只在 UFO 在场且过了 60 帧暂停时扫池）*/
    if (ufo_can_absorb(&s->u)) ufo_absorb_tick(s);
    ufo_tokens_tick(s);
    /* 最后再叫一次召唤：覆盖「组早就齐了、但这一刻没有新令牌进来」的两条路 ——
     * ① 上一次召唤被「场上已有 UFO」挡下，现在那个 UFO 已经走了；
     * ② 组是在**死亡事件**里落成的（那条路只在 `on_tick_2` 里落槽，落完当下没有召唤机会）。
     * `ufo_try_summon` 自己三门全查（成组 / 单例 / 建得出来）⇒ 常态下这只是两次比较。 */
    ufo_try_summon(s);
}

/* 帧末统一判定（对齐 109 的教训：`DEATH` §4.1「回调里只登记、帧末动手」）。
 * 三段**都要跑**（不许早 return：① 与 ③ 之间没有互斥关系，早退会让飞走判定晚一帧）。
 * ⚠ **顺序是刻意的**：先 ① 落槽（可能要**建**一个新 UFO），再 ② 击破开奖，最后 ③ 存亡差分。
 *   反过来的话，① 刚建出来的 UFO 会被 ③ 立刻判成「不在了」—— 它这一帧还没挂上链表尾呢。 */
static int on_tick_2(ce_card_t *c)
{
    ufo_card_t *s = ce_state(c, ufo_card_t);
    if (!s) return 0;

    if (s->pending_token) {                            /* ① 死亡栈里只登记了「若干令牌没了」*/
        /* 这一刻**已经离开死亡栈**（`on_tick_2` 是能力管理器 tick）⇒ 可以安全建本体。
         * 环里可能有多个（同一次扫描里死了好几只）⇒ 全出队。 */
        while (s->pending_n) {
            int32_t color = ufo_pending_take(s);
            if (ufo_token_to_slot(s, color) == UFO_PUT_READY) ufo_try_summon(s);
        }
        s->pending_token = 0;
    }
    if (s->pending_break) {                            /* ② 击破（死亡栈里只记了位置）*/
        s->pending_break = 0;
        ufo_break(s);
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
 * 门关着时把 VM 收掉；**HUD 槽位不动**（槽位的账归 `on_load` / `on_delete`）。 */
static int on_draw(ce_card_t *c)
{
    ufo_card_t *s = ce_state(c, ufo_card_t);
    if (!s) return 0;
    if (!CE_PLAYER() || !CE_GAME_THREAD()) { ufo_hud_kill(); return 0; }
    ufo_hud_draw(s);
    return 0;
}

static void on_run_reset(ce_card_t *c)
{
    ufo_card_t *s = ce_state(c, ufo_card_t);
    if (s && (s->spawned_tokens || s->ufos_spawned))
        ce_log("ufo: run reset - %u tokens (%u taken / %u lost / %u dropped / %u pending-dropped), "
               "%u UFOs (%u broken / %u vanished / %u failed), %u items pulled, %u rewards, %u bursts",
               s->spawned_tokens, s->taken_tokens, s->lost_tokens, s->dropped_tokens, s->pending_drop,
               s->ufos_spawned, s->ufos_broken, s->ufos_vanished, s->failed_summons, s->items_pulled,
               s->rewards_spit, s->bursts);
    ufo_hud_kill();                                    /* ★ 局末收 HUD VM（长持续效果铁律）*/
    if (s) ufo_withdraw_all(s, "run reset");
    on_ctor(c);
}

/* ★ 删卡（METHOD 铁律 / spec §7）：商店里 AbilityManager 冻着、tick 回调全停 ⇒ 只有这里收得到。
 * 该收的：令牌实体（**真敌人**，会留在场上）+ UFO 本体 + 计数 + **本卡自己的 HUD VM**。
 * ⚠ 这里**不能**析构敌人（UAF：可能正在别人的链表遍历里，`DEATH` §2.5）⇒ 跟 108 一样走
 *   「置 `flags_lo` 位 25」的静默撤退，由引擎同帧 sweep。
 * ★ HUD 二件（用户裁决 ④ 的必答项，别漏）：
 *   ① `ufo_hud_kill()` —— 3 个格子矩形 + 血条底 / 填充 + 那行读数的（最多 10 个）字形 VM；
 *   ② `ce_hud_slot_release()` —— 退出左下角货架的占位（`CE_CARD` 的 opdelete 里
 *      `ce_layer_drop_card()` 也会调一次，这里显式写出来是为了让「谁退了槽」一眼可查，
 *      幂等 ⇒ 调两次无害，与 `signal.c` 同款）。 */
static void on_delete(ce_card_t *c)
{
    ufo_card_t *s = ce_state(c, ufo_card_t);
    if (!s) return;
    ce_log("ufo: card deleted - %u tokens spawned (%u taken / %u lost / %u dropped), %u UFOs summoned "
           "(%u broken / %u failed), %u items pulled, %u rewards",
           s->spawned_tokens, s->taken_tokens, s->lost_tokens, s->dropped_tokens, s->ufos_spawned,
           s->ufos_broken, s->failed_summons, s->items_pulled, s->rewards_spit);
    ufo_withdraw_all(s, NULL);                         /* 私有状态随后由 SDK 释放（`ce_state_free`）*/
    ufo_hud_kill();
    ce_hud_slot_release(UFO_ID_NUM);
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
 *   见 `tests/test_ufo_core.c` 的 `ufo_clamp_spawn_life(UFO_LIFE_LIMIT) == UFO_LIFE_MAX_ALLOWED`）。 */
_Static_assert(UFO_LIFE_MAX_ALLOWED < UFO_LIFE_LIMIT, "the clamp ceiling itself must stay below 1000");
_Static_assert(UFO_TOKEN_LIFE < UFO_LIFE_LIMIT, "token life must stay below 1000");
_Static_assert(UFO_HP_PLACEHOLDER < UFO_LIFE_LIMIT, "UFO life must stay below 1000");
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

/* ============================== ★ 占位 / TODO 清单 ==============================
 * 每条都是「需要 ECL / 资产 / 用户决策」，取值一律是**不做**这个语义：
 *   §6 步 3 —— ECL（**最大的缺口，本卡现在只有壳**）
 *     1. `UFO_ECL_TOKEN` = `"NoEnemyDebug"`：令牌自己的 ECL 子程序**待写**（飘行 / 换色 / 接触）。
 *        一手活动框 = `moveLimit(0,128,280,128)`（spec §1 末行），换色 = 每 150 帧、60 px 内暂停。
 *     2. `UFO_ECL_BODY` = `"NoEnemyDebug"`：UFO 本体四色子程序待写（一手名字表 `th12:0x4b33e0`
 *        = `UFO_Red`/`UFO_Blue`/`UFO_Green`/`UFO_Rainbow`）。**没有它 UFO 不会飘、不会飞走、
 *        不会自己消弹**（spec §9 #10b 的「飞走由谁驱动」也一起没解）。
 *     3. `default.ecl` 的 thcrap 文件映射 —— ✅ **已做**（`native/release.py:33`，落 patch-step3 常开那个）。
 *    3b. ★ 本体的**跟随 VM** —— ✅ **本轮实装**（`ufo_body_vm_spawn/follow/kill`）：
 *        ECL 侧只能当隐形载体（`anmSetSprite(0, -1)` = 拆槽），画面由本文件起 `ability.anm` 164..167 画。
 *   §5 资产（8 件里 4 件有主了；`assets/ability/ufo/` 现在是 112 的切图目录）
 *     4. 三格令牌槽 / 收集闪光（`front.anm` 80/81）—— **第一版已经用 `drawRect` 拼出来了**
 *        （脚本 104，层 25，见 HUD 段）；**零售切图**仍然待做（这一版不冒充复刻）
 *     5. 「UFO 出现」横幅 / 大横幅 4 色染色（79/82）—— 未做
 *     6. ★ **UFO 本体 4 色贴图 —— ✅ 本轮实装**（`ability.anm` script 164..167 + 本文件的跟随 VM；
 *        零售来源 `enemy.anm` script135..138 / `enemy/ufo_anim.png`，切图器 `make_ufo_art.py`）。
 *        仍然**没做**的是：光环 / 击破特效 / 收集爆环（`bullet.anm` 207/208/211）—— 那三件还是空
 *     7. 血条外框 / 填充（`front.anm` 134/136）—— 第一版血条是 `drawRect` 拼的；**原生血条**不会出
 *        （本卡刻意把 life 压到 < 1000，spec §4 的取舍）
 *     8. ★ **令牌贴图 6 态 —— ✅ 本轮实装**（`ability.anm` script 168..173 + `ufo_tok_vm_spawn/follow`；
 *        零售来源 `bullet.anm` 175..180 / `etama8.png`，切图器 `make_ufo_token_art.py`）。
 *        ★ 2026-09-12（A8）：**彩虹令牌的每 150 帧换色 + 三段 `interruptLabel` 已实装** ——
 *        彩虹改用 `ability.anm` **219..221**（全文版，`make_ufo_token_art.py` 生成）+
 *        `ufo_tok_rainbow_tick` 按一手发 3/2/7 号中断（发送者 / 触发条件 / 地址见文件头那个常量块）。
 *        ⚠ 由此 171..173（被截断的那三条彩虹）**现在没有调用者**了 ⇒ 留着没删（等统一压号时一起处理）。
 *        ⚠ 仍然**没做**的是：**金令牌与蓝令牌长得一模一样**（金借蓝那一套贴图，用户裁决 ①「等 P 图」）
 *     9. `UFO_SE_ITEM_BREAK` = 0x11 是**占位**（TH12 的击破/赏金音未查一手）
 *    8b. ⚠ A8 两处**故意没搬**（一手里有、本卡不做，别当成漏了）：
 *        ① 一手在 60 px 圈内会**连道具寿命也一起冻结**（`th12:0x4262f0` 的第二个 timer = 道具 1200 帧
 *           的寿命计时器）——我们的 `ttl`（`UFO_TOKEN_TTL_FRAMES`）是自定值、语义不同 ⇒ 不跟着冻
 *           （跟了就等于顺手换掉一个与本题无关的行为）。
 *        ② 一手换色时会播一个音（`th12:0x4264dd` → se id **`0x35`**）——那是 **TH12** 的 id，
 *           本仓只有 TH18 的音效表（`engine/_shared/th18-sound-table.md`）⇒ **不播**：宁可没声音，
 *           也不借一个 TH18 的 id 冒充（要补得先查 TH12 `0x4aea50` 那张 se 名表对到 TH18 的哪一条）。
 *    8c. ❓ A8 待实跑：① 三段中断的观感（进圈换固定色慢摆 / 90 帧起快闪 / 出圈回主循环）；
 *        ② 60 px 圈读的是**实体坐标**（`CE_ENEMY_POS_X/Y`，与 `ufo_tok_pick` 同一套）；一手是 TH12 的
 *           道具坐标 —— 两边都是 ECL 640 空间，但「60」的量纲**没验过**（1 单位 = 2 表面 px ⇒ 约 120 px）；
 *        ③ 换色走「删旧 VM + 起新 VM」（TH12 是同一个 VM 原地换脚本）⇒ 理论上可能有一帧叠影/空档，
 *           实跑若看到换色那一帧闪一下，就是它（要消掉得给 SDK 加「重指脚本」）。
 *    8d. 脚本号对账（A8）：彩虹全文版由 `make_ufo_token_art.py` 的 `SCRIPT_FULL_PORTS` 生成，
 *        落地当天用的号 = `ability.anm` **219..221**（`CE_ANM_ABILITY_SCRIPT_UFO_TOKEN_RAINBOW_{RED,BLUE,GREEN}`）。
 *        ⚠ 生成器要求脚本号从 68 起**连续**（`build_ability.py` 的硬门）⇒ 并发开发期中间有洞时
 *        `make anm` 会直接 raise（这跟本卡无关，是抢号协议）；收尾压号 = `assets/renumber_scripts.py --apply`：
 *        它按文件名 + 首行改号、**不会**改生成器那张表 ⇒ 压完**要回头把 `SCRIPT_FULL_PORTS` 的三个号改掉**
 *        （C 侧不受影响：只认宏名）。
 *    9b. ★★ **五边形框 —— ✅ 2026-09-12（A7）本轮实装**（用户 2026-09-12 裁决 ③；`TUNING.md` 资产缺口 A7）：
 *        框 = `ability.anm` **script223**（= 零售 TH12 `bullet.anm` **script211** 的逐条移植：
 *        `sprite(-1); layer(12); blendMode(1); drawPolyBorder(1.0f, 5); angleVel(0,0,0.034906585f);`
 *        `scaleTime(10,4,20,20)` + 四个颜色标签 7/8/9/10），触发条件 / 颜色 / 层号 / 生命周期的一手
 *        全部写在文件头「★★ 五边形框」常量块里；卡侧驱动 = `ufo_pentagon_hint_update()`
 *        （算判据 + 建框 / 每帧跟随 / 条件不成立就收）。**没有用 `drawRect` 硬凑** ✓。
 *        ⚠ **这项资产没有贴图可切**：`drawPolyBorder` 是引擎程序画的多边形（`sprite(-1)`）⇒
 *        无 PNG、无 ORDER 片段（TH18 零售同款写法见 `effect.anm` script101 / `ability.anm` script35）。
 *        ⚠ 三处与零售的差别（都是「我们的令牌是敌机实体」逼出来的，常量块里逐条写了理由）：
 *        ① 框**每帧跟随**实体坐标（零售建完就不动）；② 金走 `ufo_color_norm()` 落蓝那一格；
 *        ③ 判据用**卡自己的「彩虹 = 通配」**模型（零售的彩虹令牌按**当前显示态**算）——
 *        ⏳ **这一格待用户拍板**（要改成零售字面口径 = `ufo_pentagon_hint_update()` 里把 `t->color`
 *        换成 `rb_state` 对应的色，就一处）。
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
 *        写在 `ufo_group_put`（`ufo_core.h`）+ 单测；⚠ 与**字面一手**的差异（`{A,B,A}` 首尾同色：
 *        字面一手出彩虹、我们按用户口径丢最旧）记在 `ufo_group_keep2_literal_th12()` 上，
 *        ⏳ TODO(实机复现一次)
 *    12. #3 蓝 UFO 吸满不吐 —— **复刻**（一手 switch 无 case 2）；★ 金**照蓝**（同样不吐）
 *    13. #4 HP 缩法 —— `UFO_HP_PLACEHOLDER` = 100（小值占位，不是复刻）
 *    14. #5 击破得分公式 —— **不编**：`UFO_BOUNTY_SCORE_PER_*` = 0 ⇒ 击破加 0 分
 *    15. #6 扩圈消弹（`0x429250` vs `0x429370`）—— **不选**，一次都没调（`ce_cancel_radius` 有现成面）
 *    16. #7 吸掉金钱的经济影响 —— 照吸（用户定案 ⑦），但把 `type 2` 吸走 = 每颗少 1 金钱
 *        ⇒ **记账在日志里**（`items_pulled` / `cnt_money`），⏳ 待用户看完再决定要不要改成别种道具
 *    17. #8 HUD 落点 / 认领 —— ★ **已按用户 2026-09-12 裁决 ④ 认领并画第一版**
 *        （`ce_hud_slot_claim_card(112, …)` + 三格令牌槽 + 本体血条 + 一行贴图 VM 读数，层 25）；
 *        ⏳ 坐标 / 配色 / 字号全是 ❓ 初版，等用户逐个对
 *    17b. ★ HUD 的「整摞一起淡」（玩家贴近左下角，`ce_hud_near_corner()`）**没做** ——
 *        HUD-LAYOUT §4.5 要求多块一起淡，本版先不淡（形制未定，不自己发明淡出曲线）
 *    18. #9 令牌实体三处细节 —— 取「自己就是实体 + 接触即入槽 + 与本体共用惰性壳」
 *    19. #10 / #10b 离场信号通道 + 飞走由谁驱动 —— 卡侧只认「id 不在了」（可以工作，但**飞不起来**）
 *    20. #11 循环音停止 API —— 本卡没起循环音（`on_enemy_death` 里不播音）⇒ 这一条**目前不适用**；
 *        真 ECL 到位、加了循环音之后必须补
 *    21. #12 令牌掉落普查 —— TH18 侧「谁产令牌」是自定：本实现按**节流**自己撒（见下 §自定参数）
 *    22. #13 `BOMB_KILL` 算不算击破 —— 取「算」（`ufo_token_kill_counts` / `ufo_body_kill_counts`）
 *   ★ 自定参数（spec 没有、一手也没有 ⇒ 具名常量 + ❓）
 *    23. `UFO_TOKEN_LIFE` = 1（令牌 HP）、`UFO_TOKEN_MAX` = **3**（同时几个；⚠ 原为 1，理由见常量注 ——
 *        上限 1 时场上永远只有一个、容易被误伤或超时撤掉 ⇒ 那一局基本凑不齐）、`UFO_TOKEN_TTL_FRAMES` = 2400、
 *        `UFO_TOKEN_SPAWN_INTERVAL` = 300 / `UFO_TOKEN_SPAWN_RETRY` = 120 / `UFO_TOKEN_SPAWN_FIRST` = 180、
 *        `UFO_TOKEN_PICK_DIST` = 20 px、`UFO_PLACEHOLDER_STAGE` = 1（填充率公式的关卡号）、
 *        `UFO_PENDING_CAP` = `UFO_TOKEN_MAX + 2`（死亡登记环容量）
 *    24. 令牌颜色由**卡侧的轮转表**给（实体自己不携带颜色；表在纯逻辑层 `k_ufo_token_color_rotation`）
 *        —— 真 ECL 到位后应改成「实体携带颜色 = `$I0`」并让卡侧读它（一手：`item+0x9a0` 那套是物品侧）
 *    29c. ★ 本轮「跟随 VM」的**待实跑**（三条，全是❓）：① 令牌层的层号 `UFO_TOKEN_ANM_LAYER` = 13
 *        （与原生道具同层的二手口径，没实测过；小了会被别的层压住、大了会压住子弹）；
 *        ② 本体 VM 的位置可能比敌机**晚一帧**（跟随读的是 `on_item_frame` 时刻的坐标，敌人 sweep
 *           在同一帧更晚的位次）—— 敌机本体是隐形的，肉眼看不到这一帧差，但「吸收判定」用的坐标
 *           与画面若差一帧，贴脸时可能有 1 px 级的观感错位；③ 自愈重建（VM 被引擎在别处销毁时）
 *           会从头播一遍淡入（`alphaTime(60,0,255)`）⇒ 若实跑看到飞碟周期性闪一下，就是它。
 *    29d. ★ **五边形框（A7）的待实跑**（三条，全是❓）：① 观感 —— 2°/帧自转 + 10 帧从 0 胀到 20
 *        （`drawPolyBorder(1.0f, 5)` 的第一个参 = 半径 1.0，乘 `scaleTime` 到 20；世界层 1 单位 = 2 表面 px
 *        ⇒ 成品半径约 40 表面 px）与令牌（32×32 画布）的大小关系是**照零售数值推的**，没在实机上看过；
 *        ② 层号 `UFO_PENTAGON_ANM_LAYER` = 12（零售原值）压在令牌的 13 下面 —— 「框在令牌后面」这个
 *        相对关系照抄了零售（零售：框 12 / 道具自己的 VM 23），但 **TH12↔TH18 的层轴是否逐档等价没实测**；
 *        ③ 「建完立刻发标签」这一步在 TH18 侧的**中断语义没实跑过**（判定依据是一手的装载器「跑一帧」
 *        + `th18:0x488be0` 写 pending interrupt；若实跑看到框**根本不出现**、或出现却一直不换色，
 *        第一个要查的就是这里 —— 退路是把标签改成**脚本自己的默认段**（不带标签也有颜色），但那样就
 *        偏离零售的「一个 VM + 四种标签」结构了）。
 *   🟡 待实跑
 *    25. 扫池成本（没有任何实测帧耗；口径见文件尾 §性能）
 *    26. 槽泄漏量（口径见文件尾 §槽泄漏）
 *    27. `ufo_drag_speed` 的步长（一手公式照抄，但 TH18 没有「飞向 UFO」状态 ⇒ 手感要重标）
 *    28. 围栏三件的边界（|x| >= 200 / y > 472 / 自机 30 px）—— 一手值已复核，但「引擎当帧删」的
 *        时序在实跑里没验过
 *    29. `on_item_frame` 里建敌人（`ufo_tok_spawn`）—— 这条不是死亡栈（是每游玩帧的门卫位次），
 *        但「断点里 `new`」这件事本身没有先例 ⇒ AUDIT 待补
 *    29b. ★ **HUD 第一版全都没在实机上跑过**：三格槽 / 血条的矩形锚点（`AUDIT.md` §AL20 说矩形默认
 *        左上角锚，`signal.c` 的注释说图心 —— 两者冲突，本卡按 AL20 的实跑结论写，留了
 *        `UFO_HUD_RECT_ANCHOR_CENTER` 一个宏）· 块落点（货架给的槽坐标）· 读数那一行的垂直对齐
 *        （受引擎 ASCII 环境的 `align_v` 影响）· 血条读到的 life 值 全是待实跑首验的
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
 *        `PLAN` §4 未解 3 记的是**实跑未验**（不是没实装）。 */
