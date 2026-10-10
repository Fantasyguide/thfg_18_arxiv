/* 季节（TH16，card-expand id 116）—— **装备卡**：季节点 → 季节子机 → 季节解放。
 *
 * 一手依据：[`engine/season/th16/01-season-data-and-tables.md`]（TH16 v1.00a 脱壳版 exe 反编译）
 *   · 档位阈值 100/230/390/590/840/1140（`th16:0x43deb0` + seed 字节）
 *   · 五副季节 10 张释放档位表（`.rdata` `th16:0x491e70`..`0x491fd4`）
 *   · 季节点三条来源：按累计伤害（`seasonItemDamage`）、死亡计时衰减（`dropSeason`/`dropItems`）、
 *     **擦弹**（`Player__do_graze` `th16:0x444cf0` 每颗擦到的弹撒一颗）、**消弹**（`gen_items_from_cancel` `th16:0x416a00`）
 *   · 季节点外观 = `bullet.anm` script129..133（`bullet/item.png` 右上 2×2 的 春/夏/秋/冬 +
 *     `(224,32)` 土用，各 16×16，与 P 点/蓝点同尺寸；TH16 按 `脚本号 = 129 + SUBSEASON` 选）：
 *     加算混合、10 帧淡入、16 帧从 2 倍缩到 1 倍、**随机自转**（`rotate(%RANDRAD)` +
 *     `fsetMul(%F0,%RANDF2,0.08726646)` + `angleVel(%F0)` = 0..5°/帧 随机方向）、
 *     之后 300 帧缩到 0.7 并淡到 alpha128、第 36 帧关加算
 *   · 消耗：春/秋/冬 清零，夏/土用 只扣一档（`th16:0x40db20`）
 * 纯逻辑（档位/表/消耗）在 `cards/season_core.c`，主机单测 `tests/test_season_core.c`。
 *
 *   ① 季节点 = **本卡自己的 anm VM**（`ability.anm` script 115 + 5 张原生切图，按副季节换 sprite）。
 *      运动学**逐条照抄原版道具**（TH16 `ItemManager__on_tick_1d__body` `th16:0x42f4e0` /
 *      TH18 `ItemManager__on_tick__body` `0x445a80`，两作同款代码同一组常量）：
 *        · ★ **出生先「待发射」再「直线飞散」**（TH16 `item+0xc50` = 6 → 3，`0x42f54e`/`0x430c90`）：
 *          延时 `total_items_created & 3` 帧（错帧，`0x430a0e` 段），然后
 *          **速度 = 出生给的 0.2..2.1、角度 = 出生给的随机角**，每帧
 *          `pos += vel`、`r -= 0.03`、`vel = polar(角度, r)`（`0x42f82c`–`0x42f83f`）——
 *          即**沿固定方向减速直线飞行**，飞完 `r` 就停（0.2..2.1 速 ⇒ 飞 0.7..73 px）⇒ 落点天然散开。
 *          ⚠️ **这一相位没有重力、也不清 vx** —— 早前版本一出生就套「飘落」规则（`vy >= 0 → vx = 0`），
 *          于是朝下的那半点全钉在敌人的 x 上、排成一列（用户 2026-09-10：「竖着一列同 x 坐标」）。
 *        · 飞散结束后转**飘落**（TH16：`r <= 0` → state 1，或放 B/消弹时 state 4）：
 *          `vy += 0.03`（`th16:0x49440c` = `th18:0x4b90a4`）、
 *          `vy > 2.0 → 2.0`（`th16:0x494558`）、`vy >= 0` 时水平速度清零
 *        · 出界删：`y > 472`（`th16:0x4946b8`）/ `|x| >= 200`（`th16:0x494664`）
 *        · 吸引：进「吸引半径」→ 朝自机飞，初速 = 磁速/3（`0x446647` 段 / `th16:0x42fd1d` 段），
 *          每帧 +0.2 到上限 12.0（`th16:0x4945d4`）；**过回收线**（`player+0x47998`，TH16 是 128/纱苗 224）
 *          → 朝自机飞，初速 = 磁速本身
 *        · 收集半径 / 吸引半径 / 磁速**读自机自己的字段**（`player+0x47988..0x47994`，初值 5.0/30/70/70）
 *          —— 原生卡 ITEM_CATCH(21) / ITEM_LINE(22) 改的就是它们，我们的季节点跟着一起变
 *      ⚠️ 不用 TH18 的原生道具：季节点没有原生 type（TH18 type≥0x14 直接返回），而借分数转换点
 *      的贴图外观不对。代价：季节点不进 TH18 的得分/道具系统（它只是本卡的资源）。
 *   ② 季节槽：累计季节点按 TH16 阈值分 0..6 档（`season_core.c`）。
 *   ③ 季节子机：档位 = 子机数（最多 3 个），**阵位直接照 TH16 副 sht 的 option_pos**（每档一套，
 *      秋最宽 ±48 / 冬竖线 / 其余小扇），走 `shooterset SEASON`；**切副季节时贴图与阵位一起换**。
 *   ④ 季节解放：**C 键**（本卡是主动卡：D 切到本卡、C 发动，见 `on_activate`）；按副季节查表出
 *      「半径 / 持续 / 每帧伤害 / 无敌」，持续期间每帧半径清弹 + 一个伤害源；
 *      **切副季节 = 完整双击 Z（春→夏→秋→冬→土用）** —— ❓自定（用户 2026-09-29 口径），
 *      TH16 没有切季节的键（见 `SEASON_INPUT_Z` 的注释）。
 *      ⚠️ 原始键不进 replay 输入流（回放只录 `th18:0x4ca428` 的掩码）→ 用 A/S 触发的效果回放里不重现。
 *   ⑤ HUD：**游玩区左下角**（照 TH16 `front.anm` script113–118 的季节计量槽）：季节字 PNG（32×32，
 *      `front00.png` (832,80)+32k 的 5 个单元，TH16 用 `sprite = 82 + SUBSEASON`）+ 长方形进度条
 *      （外框 `drawRectBorder(102,10)` / 填充 `drawRect(100,8)`，同 TH16 的两张脚本）+ 档位数字贴在条右端。
 *      玩家贴到左下角（`x < -64 && y > 400`）时 5 帧淡到 alpha 64，离开淡回 255
 *      —— 照 TH16 `Gui::on_tick_20` `th16:0x427cf0` 的闩锁 + 迟滞。
 *      ★ 五个 VM 画在 **UI 表面的层 24**（组 37 = 优先级 `0x3c`，早于暂停模糊罩的 `0x3e`）
 *      —— 平时全分辨率、暂停时被罩住；档位数字仍是 ASCII 文字，**罩不住**（见 `ce_hud_text_ex` 段）。
 *
 * 不做（下一步）：绿分数道具（TH18 无此概念）、土用「消弹圈向后移动并缩小」、
 * 子机按档位换弹型（现在只提速）。
 */
#include "sdk.h"
#include "season_core.h"
#include "broken_core_core.h"          /* bc_atan2f：纯 SSE 的确定性 atan2（擦弹点的飞散角）*/
#include "hud_dim.h"                    /* ★ 跨卡统一件：左下角 HUD 压暗（❓ 用户口径，见文件头）*/

#include <string.h>
#include <stdio.h>      /* snprintf：档位数字的字符串 */

#define SEASON_OPTS          6        /* 最多几颗子机 —— ★ TH16 就是「档位 = 子机数」：
                                           `season_option_pos` 的表按 1+2+3+4+5+6 = 21 个阵位排
                                           （`k_opt_start` = {0,1,3,6,10,15}），所以档 6 = 6 颗 */
#define SEASON_OPT_DX        24       /* 子机横向间距 */
#define SEASON_RETRY         60       /* 拿不到子机槽时的重试间隔 */
#define SEASON_SE_GET        0x25     /* se_item00（收道具）*/
#define SEASON_SE_LEVEL      0x3f     /* ★ TH16 `item_collect_season` 升档时播 0x3f（一手），不是 se_extend */
#define SEASON_SE_RELEASE    0x2c     /* se_bomb（解放）*/
#define SEASON_SE_SWITCH     0x4e     /* se_changeitem（切副季节）*/
/* ★★ 2026-09-29 用户口径（原话：「算了，改成**双击 Z** 吧，**必须是完整的双击**：
 *   **不按 Z，按 Z，放开，再按**」）⇒ **副季节切换 = 完整双击 Z**（❓自定，**不是复刻**；状态机在
 *   `season_core.c` 的 `season_dtap_tick()`，纯逻辑可离线单测）。
 *   Z 的位 = `INPUT_HELD`（`th18:0x4ca428`）**bit0 `0x001`** —— 一手：
 *   `engine/input/th18/01-key-mapping.md` §2.1（`0x401ca6`/`0x401cb1` 把 Shot 键表 `0x4ca470` 的位挪到 bit0）、
 *   §2.3 位表（`0x001` = 键盘 Z / 手柄 Shot = 射击）、§2.4 的逐键仿真；
 *   同仓先例 `cards/pofv.c:63` `POFV_INPUT_SHOT = 0x001`、`cards/animal.c:2608` `CE_INPUT_HELD() & 1u`。
 *   ⚠ 只读 `INPUT_HELD`（不写任何掩码位）⇒ ① 不进 replay 的问题不存在（held 的低 16 位就是
 *   `ReplayManager__on_tick__record_replay` `th18:0x462961` 录的那一份，回放里双击会一模一样地重现）；
 *   ② **不再需要**「按 Shift 抹掉 D 边沿」那段（已删，见 `on_tick_2` 的退休说明）。
 *   ⚠ 背景（为什么以前是 Shift+D）：TH16 **没有**切季节的键 —— 副季节在选关/菜单里选
 *   （`MainMenu__do_subseason_select` `th16:0x450af0` 的四处写点 `0x450d95`/`0x450df1`/`0x450df8`/`0x450e9b`
 *    + 关卡配置拷贝 `0x447b53`/`0x44b083`），游戏里唯一的季节键是「季节释放」`INPUT_RISING_EDGE & 0x800`。
 *   所以键位一直是**用户/owner 口径**（旧 `Shift+C` → 2026-09-21 `Shift+D` → 本次 **双击 Z**），
 *   没有一手可复刻。*/
#define SEASON_INPUT_Z        0x001u  /* Z（射击）—— 只读，绝不写 */

/* ★ 2026-09-29 **已退休**：`SEASON_INPUT_CHANGE 0x800u`（D）、`(held & 0x008) && (INPUT_PRESSED & 0x800)`
 *   这条 Shift+D 门，以及紧随的 `if (held & 0x008) INPUT_PRESSED &= ~0x800;` 阻断 —— 见 `on_tick_2`。
 *   拆掉阻断那一段同时修掉一个真问题：**按住 Shift 时引擎的 D（切换主动卡）被我们吃掉了**。*/
#define SEASON_GRAZE_ADDR    0x4ccd0c /* 原生擦弹计数（GLOBALS.inner+0x30）；与 lolk.c 同址 */
#define SEASON_GRAZE_PER_FRAME 2      /* 一帧最多因擦弹撒几颗 */
#define SEASON_CANCEL_PER_POINT 1     /* 消弹 → 季节点：**每 1 颗消掉的弹给 1 点**
                                       * ★ 2026-09-15 用户裁决改成 1（复刻 TH16）。
                                       * 一手 `th16:0x416a00` 的 mode 4 = 季节道具（type 0x10）
                                       * **每颗弹 1 个**（mode 5 才是每 3 颗 1 个，那是主炸弹）；
                                       * 我们照 mode 4 的档，所以比例 = 1。原值 8 是 ❓ 自定（已撤销）。
                                       * ★ 2026-09-29：**这个比例现在直接体现在代码里**
                                       * （`cancel_spawn_points` 的「一颗弹一颗点」循环），
                                       * 不再是累加器口径。*/
/* ★ 2026-09-29 **已退休**：`SEASON_CANCEL_POINT_MAX 4`（每帧最多撒几颗）与 `rel_acc` 累加器
 * —— 那是「我们没有弹的位置通道」时代的 ❓ 自定闸。现在有了位置（`cancel_spawn_points`），
 * 照 TH16 的做法**一颗被消的弹就给一颗点**，改由池上限（`SEASON_POINT_POOL`）自然兜底。*/
#define SEASON_CANCEL_MAX    9999     /* 半径清弹的数量上限（不限）*/
#define SEASON_DMG_CAP       300      /* 释放帧把每敌每帧伤害上限抬到它（Remilia / 火主同款值）*/
#define SEASON_DMG_WINTER    1500     /* 冬：释放期间自机弹伤害 ×1.5（TH16 `player+0x2c7cc=1.5f`）*/
#define SEASON_SPEED_FALL    1.5f     /* 秋：释放期间移速 ×1.5（TH16 `zPlayerInner+0x16078`）*/

/* —— 季节点：本卡自己的 VM；**运动学照原版道具逐条抄**（地址见文件头）—— */
/* ★★ 第三十三轮：96 → **512**。池只有 96 而 boss 一次掉 200 ⇒ 多出来的 104 颗进队列、
 * 要等玩家把场上收空才一颗颗补出来（`burst_tick` 8/帧但**池满时生不出来**），实跑看着像
 * 「boss 又掉了一次」（一秒钟后又冒出一批）。TH16 的原版道具池是 4696 槽（`0x1258`），
 * 200 颗是一口气全出来的 ⇒ 池子给够，boss 那 200 颗当场全出。VM 池上限 0x7fff（`get_vm` 的
 * 掩码），512 颗点占 512 个 VM，安全。*/
#define SEASON_POINT_POOL       512
#define SEASON_POINT_LAYER      13      /* 世界层 13 = 相机 3（游玩区），与原生道具同层 */
#define SEASON_POINT_GRAVITY    0.03f   /* 每帧重力增量 */
#define SEASON_POINT_VFALL_MAX  2.0f    /* 下落速度上限 */
#define SEASON_POINT_MAG_MAX    12.0f   /* 吸引速度上限 */
#define SEASON_POINT_MAG_STEP   0.2f    /* 吸引速度每帧增量 */
#define SEASON_POINT_OOB_Y      472.0f  /* 出界删（y）*/
#define SEASON_POINT_OOB_X      200.0f  /* 出界删（|x|）*/
#define SEASON_POINT_THROW_MIN  0.2f    /* 击破掉落（extra-drop 通道）的初速下限：TH16
                                           * `EnemyDrop__eject_extra_drops` `0x41d700` 里 type 0x10 那一支是
                                           * `speed = randf(0..1) × 1.9 + 0.2` = **0.2..2.1**（一手反编译）。
                                           * ⚠️ 早前写的 1.2..2.2 是**另一条通道**（按累计伤害掉的
                                           * `ItemManager__spawn_item` 调用方，TH16 `[0x494510]` = 1.2f）——
                                           * 用错了 ⇒ 点飞得太远，朝下的那批在同一个 x 上拉成一长条
                                           *（用户 2026-09-10：「竖着一列同 x 坐标」）。*/
#define SEASON_POINT_SCORE      10      /* TH16 收集 case 0x10 尾：`add_to_score(10)` */

/* 每个敌人掉落的季节点数 —— TH16 一手：`dropSeason(60,8,1)` 小妖精 8 个、大妖精 12~100、
 * boss `(1200,200,10)` / `(2400,400,400)`；ECL 里逐个敌人写，这里按「小怪 8 / boss 200」对齐。*/
#define SEASON_PI            3.14159265358979f
#define SEASON_POINT_INTANGIBLE 32      /* ★ 出生后的「闪白 / 无敌」期（帧）：不吸、不收、不减速、不抖，
                                           * 点按出生抛速自由飞（TH16 的出场白色阶段就是这个意思；
                                           * 引擎自己的道具 tick 也是 `age > 31` 才开始抖/染色）。*/
#define SEASON_POINTS_PER_ITEM 1        /* 一颗点 = 1 个季节点（TH16 一个季节点就是 1 点）*/
#define SEASON_DROP_MID      8          /* TH16 小妖精 `dropSeason(60,8,1)` 的 N（出场 T 帧内击破给满）*/
#define SEASON_DROP_MID_TIME 60         /* 上面的 T：计时器从出场开始走 60 帧 */
#define SEASON_DROP_MID_MIN  1          /* 上面的 M：计时走完只剩 1 个 */
#define SEASON_DROP_BOSS     200        /* TH16 boss `dropSeason(1200,200,10)` */
#define SEASON_DROP_CAP      512        /* 待撒队列上限（防溢出；池子给到 512 后基本用不上）*/
#define SEASON_BURST_PER_FRAME 8        /* 每帧最多撒几颗（boss 那 200 个摊成 ~0.4 s 的一阵雨）*/
/* 掉落去重环长度 / 窗口。窗口要盖住「同一个击破节点」的两次回调
 * （引擎死亡路径先调一次、ECL 的 `dropItems` 再调一次，中间可能隔着几十帧的死亡演出），
 * 又不能长到把下一个相位的掉落并进来 —— TH18 boss 相位之间至少 `wait(120)`（st01bs ECL 实证）。*/

/* —— HUD（照 TH16 `front.anm` script113–118 的季节计量槽；玩家贴近就淡到 64/255）——
 * ★ 2026-09-10 第三十一轮（用户选了「搬进游玩区」这条，实测后改成**UI 表面 + 组 37**）：
 * 位置放在**游玩区左下角**（TH16 真正的槽位：`front.anm` script114 的 `pos(-377,888)` +
 * `anchor(1,2)`；`Gui::on_tick_20` `th16:0x427cf0` 的淡出触发「玩家 x < −64 且 y > 400」
 * 正说明槽在游玩区里），但**画在 UI 表面**（层 24 → 绘制组 37 → 优先级 `0x39`）：
 *   - 暂停的模糊罩是 `text.anm` script59 的 UI VM（**层 38 → 组 38 → 优先级 `0x3e`**），
 *     只盖游玩区矩形（768×896 表面 px）。组 37 比它**早画** ⇒ 暂停时槽被罩住（视觉上就是「一起模糊」）；
 *   - 平时槽在 UI 表面上 = **全分辨率**（游玩区那张 RT 只有 408×472 再放大到屏幕，
 *     画在那边会**一直**偏软 —— 用户 2026-09-10 实测「放进来的 PNG 都变模糊了」）。
 * ⚠️ 代价：槽不是「自己变模糊」，而是被模糊罩**盖住**（和游玩区里的东西一样）；
 * 档位数字是 ASCII 文字，另见 `ce_hud_text_ex` 那段的组号说明。*/
#define SEASON_GAUGE_BX    308.0f     /* 外框中心 x（表面 px）：游玩区左边缘内侧，框宽 102 ⇒ 257..359 */
#define SEASON_GAUGE_BY    916.0f     /* 外框中心 y：游玩区底边（928）上方 12 */
/* ★★ 2026-09-12 第三十七轮：**迁进 `HUD-LAYOUT.md` §2 的左下角货架**（用户 2026-09-12 裁决
 * 「116 可以直接迁移」）。原来 `SEASON_GAUGE_BX/BY` 是**写死的绝对坐标** ⇒ 与 117 / 107 / 111
 * 各画各的、必然压在同一片地方。现在：
 *   · `on_load` 用 `ce_hud_slot_claim_card(116, ...)` 认领一块（带关卡纪元 ⇒ 每关开场重排）；
 *   · `on_draw` 每帧 `ce_hud_slot_get(116, ...)` 取**最新**锚点（同批后来的认领会按 id 重排）；
 *   · 所有元素的绝对坐标一律换成「**槽锚点 + 下面的相对偏移**」，`SEASON_GAUGE_BX/BY` 退居为
 *     「锚点恰在货架第一格 (257,924) 时」的等价写法（257+51 = 308 / 924−8 = 916，
 *     所以上表每个像素与迁移前**逐字节相同**，只是当锚点被挪走时会跟着走）。
 * 块尺寸口径照 §2：**视觉最高元素 → 最低元素**（不是外框）：
 *   最高 = 季节字贴图的可见上沿 **888**（VM 中心 904、32×32），最低 = 外框下边 **920** ⇒ 32；
 *   上下各留 4 px ⇒ **36**（§6.1 说的 `CE_HUD_BLOCK_PAD` 口径，先在这张卡上落地）。
 *   ❓ 自定：36 这个 h（含 4 px pad）与 104 这个 w（外沿 263..365 的 102 + 左右各 1 px 余量）
 *   都是我拍的，实跑看一眼货架行高/与 117 的间距对不对再说（§6.1 待定项）。*/
#define SEASON_HUD_BLOCK_W  130.0f   /* ★ z7（审计 P2-1）：104 是 257 锚点时代推的 ⇒ 墨迹右沿 177 > 172，改 110 刚好包住 */    /* ❓ 自定：块宽（表面 px，报给货架分配器）*/
#define SEASON_HUD_BLOCK_H   36.0f    /* ❓ 自定：块高 = 视觉 888..920（32）+ 上下各 4 */

/* —— 块内布局：**每个元素相对「块左下角」的偏移**（表面 px）——§2「块内部怎么摆仍归卡自己」。
 * 每个偏移 = **迁移前源码里的绝对表达式 − 锚点 (257, 924)**（锚点 = 货架第一格的块左下角）：
 *   外框中心 = (`SEASON_GAUGE_BX`, `SEASON_GAUGE_BY`) = (308, 916) ⇒ **(+51, −8)**，
 *              四条边再各加 `k_season_frame[]` 的 (dx, dy)（那组数一个没动）；
 *   长条（进度 + 背景）= (`BX + 7`, `BY`) = (315, 916) ⇒ **(+58, −8)**；
 *   季节字 = (`BX − 28`, `BY − 12`) = (280, 904) ⇒ **(+23, −20)**；
 *   releaseable 条 = (`BX + 20`, `BY − 8`) = (328, 908) ⇒ **(+71, −16)**；
 *   档位数字（贴图 VM，**640×480 字面量空间**，走 `ce_hud_vm_text_set`）= 迁移前的写死值
 *   (95.75, 451.75) − 锚点的 640 空间值 (128.5, 462) = **(−32.75, −10.25)**。
 * ⇒ 锚点落在 (257, 924) 时，下面每一个偏移都把元素摆回**迁移前的同一个像素**（逐项对账见报告）。*/
#define SEASON_HUD_BAR_DX       58.0f    /* 长条中心 x = 锚点 + 它（308+7−257）*/
#define SEASON_HUD_BAR_DY       (-8.0f)  /* 长条中心 y = 锚点 + 它（916−924）*/
#define SEASON_HUD_GLYPH_DX     23.0f    /* 季节字中心 x = 锚点 + 它（308−28−257）*/
#define SEASON_HUD_GLYPH_DY    (-20.0f)  /* 季节字中心 y = 锚点 + 它（916−12−924）*/
#define SEASON_HUD_STRIP_DX     71.0f    /* releaseable 条中心 x = 锚点 + 它（308+20−257）*/
#define SEASON_HUD_STRIP_DY    (-16.0f)  /* releaseable 条中心 y = 锚点 + 它（916−8−924）*/
/* ★★ 2026-09-13 用户口径：**HUD 放回左下角、暂不参与货架排布**（"目前暂时不搞货架排布"）。
 *   ★ 左沿 **68**（用户 2026-09-13 拍板）= TH18 **原生「当前卡」UI 的左沿**（112 的 owner 口径同值
 *   `UFO_HUD_PIN_X = 248 + (−180) = 68`）。**曾一度改成 124 是因为误判**：
 *   中间那版先取 68 时 HUD **整块看不见**，我先归因成"位置撞车"、又改成 124 —— 其实**真因是层号**
 *   （116 的五支 HUD 脚本写的是 `layer(24)` = 画进离屏表面 B，**只有游玩区矩形内才看得见**；
 *   已改 `layer(25)` 对齐全 mod 其它 HUD，见 `AUDIT-116.md` §10.3）⇒ 换层之后 **68 本来就能看见**。
 *   （`HUD-LAYOUT.md` §1.4：原生那块 = `pos(68,924)` + `anchor(1,2)` + `scale(0.75)` ⇒ 块 `x 68..116`；
 *   同节原文「左边那块 68..116 归原生的当前卡」、货架锚点定 257 就是为了躲开这一带）。
 *   ⚠ **附带事实**（实跑日志）：107（`senro`，`@(68,928)`）与 108（`youkai`，`anchor (68,898)`、
 *   youkai 字 `(196,892)`）**也在这一带** ⇒ 三张卡同时装备时 116 会与它们重叠。
 *   用户 2026-09-13 明确：**这一轮只修 116 自己，107/108 的位置不动**（先接受重叠）。
 *   y 保持 **924**（= 117 量过的那条底线，也 = `CE_HUD_ANCHOR_Y`）
 *   ⇒ 外框仍落 910..920、竖直观感一个像素不变；块 = `(68, 924)` 起 104×36 ⇒ **x 68..172**。
 *   ⚠ 要再左/右移**只改这一个数**。*/
#define SEASON_HUD_PIN_X       68.0f
#define SEASON_HUD_PIN_Y       924.0f
/* ★★★ 2026-10-03z8（owner 实跑：「**季节等级指示数字依然在季节主体的左侧**」；
 *   实跑日志一行定案：`season: hud geom … frame_cx 761.0 … digit_sx 644.5 | **digit−frame −116.5 px**`）——
 *   旧常量（−32.75 / −10.25）是 257 锚点时代按**另一套基准**标定的 ⇒ 数字比外框心**左 116.5 px**。
 *   现改成**跟着外框心**（外框心 = 槽 + (51, −8)，见上面那一族常量）⇒ 640 空间就是其一半。*/
#define SEASON_HUD_DIGIT_X640    25.5f   /* = 外框心 x 偏移 51 / 2（`resolutionMode(2)` ⇒ 640 空间 = 表面/2）*/
#define SEASON_HUD_DIGIT_Y640  (-10.25f)  /* ★★★ 2026-10-03z33（owner：「修 th16 的数字，你 git diff 一下，
                                           *   以前肯定调好过」）—— **恢复当年调好的值**：
                                           *   `git log -G` 的取值史：`f3a8e6eb`（原、长期未动）= **−10.25** ⇒
                                           *   我 `9332bed0` 改 −4.0、`ec596cc5` 改 −6.5、`f6a882b4` 又回 −4.0 ✗（都是没依据的试）。
                                           *   原注释：数字是 `CE_ASCII_ALIGN_CENTER`、位置 = `槽锚点.y640 + Y640`；
                                           *   当初锚点 (257,924) ⇒ 640 空间 (128.5,462) ⇒ 数字绝对位 (95.75, **451.75**)
                                           *   ⇒ 表面 y = **903.5**（= 槽底 924 − 20.5 ✓ 即 −10.25 × 2）。
                                           *   ⚠ X640 当年是 **−32.75**（= 锚点左边 65.5px），那是锚点在 **257** 时的形态；
                                           *     现在 116 的块在 682 ⇒ 照搬会把数字放到 616（压邻居）✗ ⇒ **x 保留 25.5（外框心）**。*/
/* ★★ 2026-09-10 第三十二轮：外框改成**四条矩形**拼出来（TH16 `front.anm` script114 的
 * `drawRectBorder(102,10)` 等价画法；脚本 123 = 1×1 模板，实际尺寸全靠 VM `scale`）。
 * 像素口径（用户 1280 档 100% 截图逐轮标定）：
 *   内部进度条/背景条 = 100×8，像素 **x 265..364 / y 912..919**（中心 (BX+7, BY)）—— **全程未动**；
 *   外框四条边**都是 1px 粗**（复刻原版：填充在框里不居中 ⇒ 上/左那圈会**露出 1px 背景**）：
 *     上 行 910、下 行 920（各 103 宽，x 263..365）；左 列 263、右 列 365（各 9 高，y 911..919）
 *   ⇒ 外沿 **x 263..365 / y 910..920**（103×11）。
 *   ⚠️ 露出来的那 1px：行 911（x 264..364）与 列 264（y 912..919）—— 里面条是 100×8 @ (265..364, 912..919)，
 *      上/左各差 1px，右边/下边贴着（原版就是这种「差 1px」的观感）。
 * 四条边互不重叠（横条整宽、竖条只占中间 9 行），四角由横条覆盖 ⇒ 淡到 alpha 64 时角上不叠色。
 * 要再调位置/粗细，改下表对应行的 (dx, dy, sx, sy) 即可。*/
#define SEASON_FRAME_N      4
static const struct { float dx, dy, sx, sy; } k_season_frame[SEASON_FRAME_N] = {
    {  +6.5f,  -5.5f, 103.0f, 1.0f },   /* 上边：103×1 ⇒ 像素 x 263..365 / y 910 */
    {  +6.5f,  +4.5f, 103.0f, 1.0f },   /* 下边：103×1 ⇒ 像素 x 263..365 / y 920 */
    { -44.5f,  -0.5f,   1.0f, 9.0f },   /* 左边：1×9   ⇒ 像素 x 263 / y 911..919 */
    { +57.5f,  -0.5f,   1.0f, 9.0f },   /* 右边：1×9   ⇒ 像素 x 365 / y 911..919 */
};
/* ★★ 2026-09-15g：**压暗这一路改走跨卡统一件 `cards/hud_dim.{h,c}`**（唯一实现）。
 * 性质 = ❓ **用户口径（UI 统一），不是复刻**（用户 2026-09-15：「压暗这个涉及到 UI 统一，
 * 这个不能复刻。是那个靠近透明吧」）⇒ **动态**统一成：终点 alpha、起始 255、过渡帧数、滞回带 **16 px**；
 * **区域**则按**本卡自己那块 HUD 的矩形**推（见下）。
 * ★★ 2026-09-22：**取值再换一档，这次是 TH18 原生一手** —— owner 拍「th18左下角原生应该有动态吧，照着那个填」
 * ⇒ 现在 = 终点 **64**（25% 不透明）/ **8 帧线性**（= 原生那块「当前卡」UI 的 `interruptLabel(5)`，
 * 一手见 `engine/card/th18/15-selected-card-hud.md` §4.2）。历史：32 / ±56（4 帧）→ 128 / 20 帧（09-21）→ 本值。
 * 权威口径见 `hud_dim.h` 文件头。
 *
 * ★★ 2026-09-15h：**进/出的口径订正**（父 agent：2026-09-15g 那版「进 = 矩形**之内**」裁错了）——
 *   **进 = 自机落在「块矩形外扩 `HUD_DIM_MARGIN`(28)」之内**、**出 = 离开「矩形 + 44」（28 + 滞回 16）之外**。
 *   理由：本块的**下沿 446 在自机 y 钳位 432 之下** ⇒ 「站进矩形」只剩 **4 px 高**的一条（几乎点不亮）；
 *   一手各作的门本来就落在它们自己那块上沿之上 24–32 px（TH16 的 `400` 在它那块沿之上 28）⇒ 边距是**原本就有的语义**。
 *
 * 本卡的块矩形 → 门限（**全部自机坐标**；换算见 `hud_dim.h` 的「表面 px → 自机坐标」定式：
 * `自机 = 表面/2 − (320, 16)`）：
 *   · 块 = **自带锚点的固定位**（`SEASON_HUD_PIN_X/Y` = `68, 924`，2026-09-13 用户口径）：表面 `(68, 924)` 起 **104 × 36**
 *     （`SEASON_HUD_BLOCK_W/H`；实测元素 x 263..365 / y 888..920 ⇒ 块 x 257..361、y 888..924）
 *   · ⇒ 自机坐标矩形 = x ∈ [−191.5, −139.5]、y ∈ [428, 446]（= 上沿 428 / 下沿 446）
 *   · ⇒ **进** = 自机落在「矩形 + 28」之内：x ∈ [−219.5, −111.5] ∧ y ∈ [400, 474]
 *        ⚠ 自机钳位是 x ∈ [−184, 184]、y ∈ [32, **432**]，而块底在 446 ⇒ 实际可触发区
 *          x ∈ [−184, −111.5]、y ∈ [400, 432]（≈ 72 × 32 px）
 *   · ⇒ **出** = 离开「矩形 + 44」：x < −235.5 ∨ x > −95.5 ∨ y < 384 ∨ y > 490（**16 px 滞回带**）
 *   ⚠ 旧值 `x < −64 ∧ y > 400` / 淡回 `y < 384` 是**照抄 TH16 `th16:0x427f41` / `0x427f6c`**
 *     （本卡注释原话）——那是 TH16 自己那块槽的几何推出来的，**不是**我们的块；统一后按本卡几何重推
 *     ⇒ 门限数字与旧值不同（y 上沿 412 而不是 400、x 也从"左三分之一"收成"块宽"+16），这是**几何差异**，
 *     不是新的不统一。原一手值留档如下：
 *       `SEASON_HUD_DIM  64`      —— TH16 `interruptLabel(5)` 的 `alphaTime(5,0,64)`（脚本 123–127；C 侧原来也写 64）
 *       `SEASON_HUD_STEP 51`      —— 步长（照 TH16 那套时间序列）
 *       `SEASON_HUD_HIDE_X (-64) / HIDE_Y 400 / SHOW_Y 384` —— `th16:0x427f41`（进）/ `th16:0x427f6c`（迟滞）
 * ★★ 2026-09-15i：**脚本侧的 64 已经改掉**（脚本 123–127 的 `interruptLabel(5): alphaTime(4,0,32)`、
 *   `interruptLabel(4): alphaTime(4,0,255)`；父 agent 裁定「过渡帧根治 = 改脚本」）⇒ 上一轮那段
 *   C 侧 `min(脚本态, 统一 alpha)` 的**上限钳制已删**（不留两套同时生效）。
 *   现在这块 HUD 的 alpha 分两路、但**深度与时长都统一**：
 *     · 外框 ×4 / 进度 / 背景 / 档位数字 —— **C 每帧写** `s->hud.alpha`（`hud_dim_tick()`：255 ↔ **64**、**8 帧线性**）；
 *     · 季节字 / 可解放指示条 —— **脚本的 `alphaTime`**（脚本 123–127 的 label 4/5 = `alphaTime(8, 0, 64)` / `alphaTime(8, 0, 255)`）。
 *   不变式与机器判据见 `native/tools/anm_hud_alpha_check.py`（`make check` 里跑）。*/

/* ★ 层号 = **24**（UI 表面，绘制组 37，优先级 `0x39`）——理由一手（`engine/anm/th18/02` §1 的桩表）：
 *   - 组 37（`0x39`）在**暂停模糊罩之前**（罩在组 38 = `0x3e`）⇒ 暂停时被罩住；
 *   - 又在**对话立绘之前**（立绘在组 44 = `0x57`）⇒ 立绘盖住槽（第二十九轮的用户口径）；
 *   - UI 列表 VM 的层 24–32 会折到 37–45，24 → 37 是这串里最早的一组。
 * 同层内**后生成者在上**（`insert_in_ui_list_back` 尾插 + `render_layer` 表头→表尾），
 * 生成顺序照 TH16 script113 的 `scriptNew`：外框 → 实心背景 → 渐变进度 → 字 → 条。*/

/* 副季节贴图：**直接从 TH16 原版 `bullet/item.png` 切的 5 个 16×16 单元**
 * （右上 2×2 = 春/夏、秋/冬，再一行 (224,32) = 土用；TH16 用「脚本号 = 129 + SUBSEASON」选，
 * 见 `th16:0x430cb7`）。切片尺寸与 P 点 / 蓝点一致，脚本 115 的 scale 也照 TH16 那张。*/

typedef struct {
    int32_t id;                            /* anm VM id；0 = 空槽 */
    float   x, y;                          /* 位置（px）—— ★ 用 float：原版是 float 积分，量化到整数会抖 */
    float   vx, vy;                        /* 速度（px/帧）*/
    float   lang;                          /* ★ 飞散角（TH16 `item+0xc24`，整个飞散期不变）*/
    float   lr;                            /* ★ 飞散剩余速度（TH16 `item+0xc20`，每帧 −0.03）*/
    uint8_t state;                         /* 0 = 待发射，1 = 飘落，2 = 飞散，3 = 过回收线飞向自机，4 = 吸引半径内飞向自机 */
    uint8_t mag20;                         /* 吸引速度 ×20（0 = 未进入飞行；100 = 5.0 px/帧；240 = 12.0 上限）*/
    uint8_t age;                           /* 出生帧数（日志用）*/
    uint8_t delay;                         /* 待发射的错帧延时（TH16：`total_items_created & 3`）*/
} season_point_t;

/* ★ 池是 DLL 静态区：ce_state 只有 496 字节，装不下几十颗点（见 AUDIT AF12 的同款静态池）*/
static season_point_t s_pt[SEASON_POINT_POOL];

/* ★ 掉落统计（诊断用，静态区）：每 600 帧打一条汇总，方便对「哪些敌人没掉」对账 */
static int32_t s_stat_mid, s_stat_boss, s_stat_skip, s_stat_points;
static int32_t s_stat_frame;

/* ★ 档位数字的贴图 VM（同上：状态放 DLL 静态区；本卡同时只会有一张在场）*/
static ce_hud_vm_text_t s_hud_digit;

/* ★★ 2026-09-12：HUD 块在货架上的**锚点**（块左下角，表面 px）。放在 DLL 静态区
 * （`ce_state` 只有 496 字节，见 AUDIT AF12 的同款静态池；本卡同时只会有一张在场）。
 * 与 117 那种「自带锚点」不同，本卡走**货架**：认领时给的是「块多大」，坐标由分配器按
 * card_id 升序算 ⇒ 必须每帧用 `ce_hud_slot_get()` 取最新（见 `hud_slot_sync`）。*/
static int32_t s_geom_last_level = -1;   /* ★ 诊断：最近一次 geometry 日志里的档位 */

static ce_hud_slot_t s_slot;
static int32_t s_slot_ok;               /* 1 = 本关已认领到槽（拿不到就退回原绝对坐标兜底）*/


typedef struct {
    ce_option_t opt[SEASON_OPTS];          /* 子机槽 */
    int32_t     n_pt;                      /* 场上几颗点（池在 s_pt）*/
    int32_t     points;                    /* 累计季节点 */
    int32_t     subseason;                 /* 副季节 0..4 */
    int32_t     frame;                     /* 射击帧计数 */
    int32_t     sub_prev;                  /* 【已退休，保留占位】历史 = 更早的「Shift+C」原始键盘读法的上一帧电平；
                                            * 2026-09-21 改用引擎边沿、2026-09-29 改成「双击 Z」的状态机
                                            * （`season_dtap_t dtap`，见下）⇒ 本字段不再读写
                                            *（保留是为了不动 `season_card_t` 布局：
                                            * 上面那条 `_Static_assert(sizeof ≤ 496)` 与存档影子都按这个大小）。*/
    int32_t     last_graze;                /* 上一帧原生擦弹计数；-1 = 未初始化 */
    int32_t     releases;                  /* 解放次数（日志）*/
    /* —— 解放进行态 —— */
    int32_t     rel_frames;                /* 剩余帧 */
    int32_t     rel_total;                 /* 总帧（日志）*/
    float       rel_radius;
    int32_t     rel_dmg;
    int32_t     rel_sub;                   /* 释放时的副季节（快照）*/
    int32_t     rel_cancelled;             /* 本次消弹总数 */
    int32_t     rel_acc;                   /* 【已退休】消弹 → 季节点 的累加器（2026-09-29 起改由
                                            * `cancel_spawn_points()` 一颗弹一颗点直接撒；字段保留不动布局）*/
    season_dtap_t dtap;                    /* ★ 2026-09-29：**双击 Z** 的状态机（❓自定，用户口径）
                                            * —— 跨帧、无句柄；`clear_card()` 里 `season_dtap_init()` 清零。
                                            * 一手/口径见 `season_core.h` 的 `season_dtap_t` 头注 */
    hud_dim_t   hud;                       /* ★ 2026-09-15g：压暗状态（闩锁 + alpha）—— 跨卡统一件
                                            * `cards/hud_dim.{h,c}`（原来这里是 `hud_dim`/`hud_alpha`
                                            * 两个 int32：闩锁同 `TH16 gui+0x1ac bit0`，值 **64**；现在统一 32）*/
    int32_t     hud_frame[4];              /* ★ 外框 = 四条 1px 矩形（上/下/左/右），见 k_season_frame[] */
    int32_t     hud_grad;                  /* script115 渐变**进度条**（宽 = 档内进度；蓝 / 黄→白）*/
    int32_t     hud_solid;                 /* script116 实心**背景条**（全长，档位亮度；档 0 透明）*/
    int32_t     hud_glyph;                 /* script117 季节字 */
    int32_t     hud_strip;                 /* script118 releaseable / 等级条 */
    int32_t     hud_last_state;            /* 上次发过的 interrupt 组合（2/3 + 档 + 4/5），变了才重发 */
    int32_t     hud_slot_ok;               /* ★ 2026-09-12：本刻已认领到货架槽（`s_slot` 在静态区）*/
    int32_t     spawn_seq;                 /* ★ 季节点出生序号（TH16 的 `total_items_created`，给待发射错帧用）*/
    int32_t     burst_left;                /* 待撒的季节点数（boss 死亡那一下排 200 个，逐帧撒）*/
    int16_t     burst_x, burst_y;          /* 爆发原点（敌人坐标）*/
    int32_t     ring_out;                  /* 解放特效 VM：外圈（TH16 `plXXsub` 释放脚本 1/2）*/
    int32_t     ring_in;                   /* 解放特效 VM：内圈（32 边形 = 春；12 边形 = 其余）*/
    float       ring_angle;                /* 两圈的自转累计角（rad）——外圈 +spin、内圈 −spin */
    /* 掉落钩子去重环：同一个敌人的死亡序列里 `Enemy__drop_items_and_notify_cards` 会被调多次
     * （引擎死亡路径 + ECL 自己的 `dropItems` 指令），只认第一次。键 = 指针 + `enemy_id`
     * （后者每实例唯一，内存复用不撞号，见 engine.h `CE_ENEMY_ID`）。*/
    float       rel_x, rel_y;              /* 释放原点（只有秋跟着自机走 —— 照 TH16 `0x40eeb0`）*/
} season_card_t;

_Static_assert(sizeof(season_card_t) <= 496, "ce_state 上限 496 字节（超了会静默返回 NULL）");

static season_card_t *st(ce_card_t *c) { return ce_state(c, season_card_t); }
static int level_of(season_card_t *s) { return (int)season_level_for_points(s->points); }
static int opts_for(int level) { return level > SEASON_OPTS ? SEASON_OPTS : level; }

/* 拿不到 TH16 阵位时的退路（也是 allocate_option 唯一支持的「竖排」参数：非聚焦 y / 聚焦 y 同一个值）*/
static int opt_dx(int i)
{
    int k = (i + 1) / 2;                     /* 1,1,2,2,3,3 */
    return ((i & 1) ? -1 : 1) * k * SEASON_OPT_DX;
}

/* ★ 子机阵位：把 TH16 副 sht 的 option_pos 写进 option 记录的位置偏移字段（定点 ×128）。
 * 一手：`Player::allocate_option` `th18:0x40a7f6`..`0x40a811` 只写 y（x 写 0），
 * 原生卡 `th18:0x40b8c1` 四个字段都写（非聚焦 x/y、聚焦 x/y）→ 卡可以自己摆 2D 阵型。*/
static void set_option_offsets(uint8_t *o, const season_opt_pos_t *pos)
{
    int32_t x, y;

    if (!o || !pos) return;
    x = (int32_t)(pos->x * 128.0f);
    y = (int32_t)(pos->y * 128.0f);
    *(int32_t *)(o + CE_OPT_OFF_NF_X) = x;
    *(int32_t *)(o + CE_OPT_OFF_NF_Y) = y;
    *(int32_t *)(o + CE_OPT_OFF_F_X)  = x;
    *(int32_t *)(o + CE_OPT_OFF_F_Y)  = y;
}

/* ★★ 2026-09-29：**五副季节各一条自己的子机脚本**（原来五季共用 script113）——
 * 一手 = TH16 `PlayerInner__repopulate_options` 的季节段：脚本号按副季节选
 *   `th16:0x492be0[SUBSEASON]` = **{1, 1, 1, 0x12, 1}**（`th16:0x44477b`），
 *   副 anm 文件按 `th16:0x492cdc[SUBSEASON]` = {pl00sub, pl02sub, pl03sub, pl01sub, pl04sub} 选。
 * 每条的**尺寸 / 帧序 / 关键指令**都不同（春=自转、夏=4 帧 sprite 循环、秋=8 帧 sprite 循环、
 * 冬=0.5↔0.6 缩放呼吸、土用=同春），这正是用户报的「应当有各自的动画」；
 * 尺寸也随脚本回到原版（0.42 → 0.8/1.0，见 `364_…spring.anm.txt` 的注）。
 * 我们把这五条配方逐行搬成 ability.anm 的新脚本
 * （`assets/ability/scripts/36{4..8}_season_opt_*.anm.txt`，脚本本体与 TH16 原文逐行同形，
 * 只把 sprite 换成 ORDER 的占位名）：
 *   春 364 = `pl00sub` script1 / 夏 365 = `pl02sub` script1 / 秋 366 = `pl03sub` script1 /
 *   冬 367 = `pl01sub` **script18** / 土用 368 = `pl04sub` script1。
 * ⚠ 号段：`build_ability.py` 按「原生脚本数 68 + 目录内序号」编号（文件号即脚本号），
 *   本目录原来最大 363 ⇒ 新脚本 364..368 ⇒ `anm_ids.h` 给出下面这几个宏。
 *   ⚠ `anm_ids.h` 是**生成物**（`make anm` 产）；`#ifdef` 回退是为了「本文件在生成物还没刷新时
 *   也能离线过 `-fsyntax-only`」，形状照抄 `youkai.c` / `ufo.c` 的同类写法。*/
#ifdef CE_ANM_ABILITY_SCRIPT_SEASON_OPT_SPRING
#define SEASON_OPT_SCRIPT_SPRING  CE_ANM_ABILITY_SCRIPT_SEASON_OPT_SPRING
#else
#define SEASON_OPT_SCRIPT_SPRING  364      /* `make anm` 后由上面的宏取代 */
#endif
#ifdef CE_ANM_ABILITY_SCRIPT_SEASON_OPT_SUMMER
#define SEASON_OPT_SCRIPT_SUMMER  CE_ANM_ABILITY_SCRIPT_SEASON_OPT_SUMMER
#else
#define SEASON_OPT_SCRIPT_SUMMER  365
#endif
#ifdef CE_ANM_ABILITY_SCRIPT_SEASON_OPT_FALL
#define SEASON_OPT_SCRIPT_FALL    CE_ANM_ABILITY_SCRIPT_SEASON_OPT_FALL
#else
#define SEASON_OPT_SCRIPT_FALL    366
#endif
#ifdef CE_ANM_ABILITY_SCRIPT_SEASON_OPT_WINTER
#define SEASON_OPT_SCRIPT_WINTER  CE_ANM_ABILITY_SCRIPT_SEASON_OPT_WINTER
#else
#define SEASON_OPT_SCRIPT_WINTER  367
#endif
#ifdef CE_ANM_ABILITY_SCRIPT_SEASON_OPT_DOYOU
#define SEASON_OPT_SCRIPT_DOYOU   CE_ANM_ABILITY_SCRIPT_SEASON_OPT_DOYOU
#else
#define SEASON_OPT_SCRIPT_DOYOU   368
#endif

/* 给一颗子机换 TH16 原版贴图（option+0xb0 = 它的 anm VM id）*/
static int season_opt_sprite(int subseason);      /* 定义见下（TH16 原版子机贴图）*/
static int season_opt_script(int subseason);      /* 定义见下（TH16 各副季节的子机脚本号）*/
static int alloc_options(ce_card_t *c);           /* 定义见下（换季时要整批重建，先声明）*/

static void set_option_art(uint8_t *o, int subseason)
{
    uint32_t vm;
    if (!o) return;
    vm = *(uint32_t *)(o + CE_OPT_ANM_ID);
    if (vm) ce_anm_set_sprite(CE_ABILITY_ANM(), vm, season_opt_sprite(subseason));
}

/* 申请一颗子机 + 摆到 TH16 阵位上 + 换原版贴图 */
static uint8_t *alloc_one(ce_card_t *c, season_card_t *s, int i)
{
    season_opt_pos_t pos;
    /* ★ 脚本号按副季节选（原来恒为 `CE_ANM_ABILITY_SCRIPT_SEASON_OPT` = script113）——
     * 一手：TH16 的季节子机脚本号 = `th16:0x492be0[SUBSEASON]`。 */
    uint8_t *o = ce_option_alloc(c, &s->opt[i], opt_dx(i), season_opt_script(s->subseason), "season");

    if (o) {
        if (season_option_pos(s->subseason, level_of(s), i, &pos)) set_option_offsets(o, &pos);
        set_option_art(o, s->subseason);
    }
    return o;
}

/* ★★ 2026-09-29：换副季节 = **收掉重申请**（不是只换贴图/阵位）。
 * 为什么必须重建：五副季节各有自己的**脚本**（TH16 就是「副 anm 文件 + 脚本号都按 SUBSEASON 选」），
 * 而 `Player::allocate_option` 的脚本号只在**申请那一刻**传进去 ⇒ 在役 VM 换不了脚本。
 * ⚠ 这**不是**新语义：TH16 的副季节是在**季节解放**时切的，而 `Bomb__activate_bomb` 扣完槽
 *   **接着就调 `PlayerInner__repopulate_options`**（`th16:0x40db20` 的调用点清单）⇒
 *   原版换季时子机本来也是**整批重建**（会重放出场动画）。*/
static void refresh_options(ce_card_t *c, season_card_t *s)
{
    int i;

    for (i = 0; i < SEASON_OPTS; ++i) if (s->opt[i].ptr) ce_option_drop(c, &s->opt[i]);
    alloc_options(c);
}

/* 季节子机的贴图（TH16 原版：自机贴图里切出来的 16×16 小球）*/
static int season_opt_sprite(int subseason)
{
    switch (subseason) {
    case SEASON_SUB_SPRING: return CE_ANM_ABILITY_SPRITE_OPT_SPRING;
    case SEASON_SUB_SUMMER: return CE_ANM_ABILITY_SPRITE_OPT_SUMMER;
    case SEASON_SUB_FALL:   return CE_ANM_ABILITY_SPRITE_OPT_FALL;
    case SEASON_SUB_WINTER: return CE_ANM_ABILITY_SPRITE_OPT_WINTER;
    default:                return CE_ANM_ABILITY_SPRITE_OPT_DOYOU;
    }
}

/* 季节子机的脚本（TH16 `th16:0x492be0[SUBSEASON]` = {1,1,1,0x12,1} 的五条配方，
 * 逐行搬到 ability.anm 的 364..368；见文件上部那段注释）*/
static int season_opt_script(int subseason)
{
    switch (subseason) {
    case SEASON_SUB_SPRING: return SEASON_OPT_SCRIPT_SPRING;
    case SEASON_SUB_SUMMER: return SEASON_OPT_SCRIPT_SUMMER;
    case SEASON_SUB_FALL:   return SEASON_OPT_SCRIPT_FALL;
    case SEASON_SUB_WINTER: return SEASON_OPT_SCRIPT_WINTER;
    default:                return SEASON_OPT_SCRIPT_DOYOU;
    }
}

/* 季节点本体的贴图（TH16 原版切图）*/
static int season_sprite(int subseason)
{
    switch (subseason) {
    case SEASON_SUB_SPRING: return CE_ANM_ABILITY_SPRITE_PT_SPRING;
    case SEASON_SUB_SUMMER: return CE_ANM_ABILITY_SPRITE_PT_SUMMER;
    case SEASON_SUB_FALL:   return CE_ANM_ABILITY_SPRITE_PT_FALL;
    case SEASON_SUB_WINTER: return CE_ANM_ABILITY_SPRITE_PT_WINTER;
    default:                return CE_ANM_ABILITY_SPRITE_PT_DOYOU;
    }
}

/* HUD 季节字（TH16 `front00.png` (832,80)+32k 的 5 个 32×32 单元；TH16 用 `sprite = 82 + SUBSEASON`）*/
static int season_glyph_sprite(int subseason)
{
    switch (subseason) {
    case SEASON_SUB_SPRING: return CE_ANM_ABILITY_SPRITE_GLYPH_SPRING;
    case SEASON_SUB_SUMMER: return CE_ANM_ABILITY_SPRITE_GLYPH_SUMMER;
    case SEASON_SUB_FALL:   return CE_ANM_ABILITY_SPRITE_GLYPH_FALL;
    case SEASON_SUB_WINTER: return CE_ANM_ABILITY_SPRITE_GLYPH_WINTER;
    default:                return CE_ANM_ABILITY_SPRITE_GLYPH_DOYOU;
    }
}

/* ③ 子机申请：照原生装备卡 —— `on_power_level_change` 时池刚重建，按档位重来一遍。*/
static int alloc_options(ce_card_t *c)
{
    season_card_t *s = st(c);
    int i, want;

    if (!s) return 0;
    want = opts_for(level_of(s));
    for (i = 0; i < SEASON_OPTS; ++i) {
        if (i < want) {
            uint8_t *o = ce_option_claim(c, &s->opt[i]);
            if (!o) {
                alloc_one(c, s, i);                     /* 还没有 → 申请 */
            } else {
                season_opt_pos_t pos;
                if (season_option_pos(s->subseason, level_of(s), i, &pos)) set_option_offsets(o, &pos);
                set_option_art(o, s->subseason);        /* 已有 → 只更新阵位/贴图（★ 不重新申请，避免泄漏）*/
            }
        } else if (s->opt[i].ptr) {
            ce_option_drop(c, &s->opt[i]);
        }
    }
    return 0;
}

static int on_power_level_change(ce_card_t *c) { return alloc_options(c); }

/* —— ① 季节点：撒 / 飞 / 收 —— */
static void point_remove(season_card_t *s, int i)
{
    s_pt[i] = s_pt[--s->n_pt];
}

static void point_clear_all(season_card_t *s)
{
    int i;
    for (i = 0; i < s->n_pt; ++i) {
        if (s_pt[i].id) ce_anm_delete((uint32_t)s_pt[i].id);
    }
    s->n_pt = 0;
}

/* ★★ 2026-10-03b（92 伟大之作的**清扫闸**；统筹接 `HANDOFF-86-92-AND-SWEEP.md` §3 的 T2）：
 *   本卡这一批 = **季节道具（季节点 / `season_point_t`）** —— 自建实体、不在引擎道具池里 ⇒ 只能本卡自己清。
 *   做法：**倒序**扫（`point_remove` 是 swap-remove，倒序才不跳项）⇒ 每颗在原位撒金钱（池满走
 *   `ce_money_pocket()` 的同一条记账）⇒ `ce_anm_delete` 删它自己的 VM ⇒ `point_remove` 出池。
 *   ⚠ 消失必须过 `point_remove`（池的紧凑性）+ 删 VM 句柄两件事，否则留幽灵 VM / 池里留死槽（铁律二）。
 *   ❓ 面值自定（1 颗季节点 = `SEASON_SWEEP_MONEY_PER_POINT` 枚金钱）—— 见 `TUNING.md` 的 116 段。*/
#define SEASON_SWEEP_MONEY_PER_POINT 1
static uint32_t s_sweep_seen;

static void season_sweep_to_money(season_card_t *s)
{
    int i, n = 0, k;
    for (i = s->n_pt - 1; i >= 0; --i) {
        season_point_t *p = &s_pt[i];
        float pos[3];
        pos[0] = p->x;
        pos[1] = p->y;
        pos[2] = 0.0f;
        for (k = 0; k < SEASON_SWEEP_MONEY_PER_POINT; ++k) {
            if (!ce_spawn_item(2u, pos, -1.5707964f, 1.7f)) ce_money_pocket(1);
        }
        if (p->id) ce_anm_delete((uint32_t)p->id);
        point_remove(s, i);
        ++n;
    }
    if (n) ce_log("season: 92 清扫 —— %d 颗季节道具换成金钱（每颗 %d 枚）", n, SEASON_SWEEP_MONEY_PER_POINT);
}

/* ★ 建 VM（「待发射」延时结束时才建 —— TH16 就是这么干的：延时里 `item+0xc50 = 6`、
 * VM 还没出生，`Item__init_anm_for_piv_or_season` `th16:0x430c90` 才 `copy_vm` + `run`）。
 * color 出生就是 `0xffffffff`（**无染色**，贴图自带颜色；`th16:0x430960` 尾
 * `mov [item+0x530], 0xffffffff`）—— 出场观感全靠脚本自己的 10 帧淡入。*/
static int point_spawn_vm(season_card_t *s, season_point_t *p)
{
    uint32_t id = ce_anm_spawn(CE_ABILITY_ANM(), CE_ANM_ABILITY_SCRIPT_SEASON_POINT,
                               SEASON_POINT_LAYER);

    if (!id) return 0;
    ce_anm_set_pos(id, p->x, p->y, 0.0f);
    ce_anm_set_sprite(CE_ABILITY_ANM(), id, season_sprite(s->subseason));   /* 按副季节换 TH16 原贴图 */
    ce_anm_set_color(id, 0xffffffffu);
    p->id = (int32_t)id;
    return 1;
}

/* 撒一颗季节点：位置 (x,y)、**飞散角 ang、飞散初速 spd**（照 TH16 `ItemManager__spawn_item`
 * `th16:0x430960`：速度/角度都只在这里给一次，之后靠 `item+0xc20` 递减）。
 * 池满 → 返回 0（调用方决定丢还是排队）。出生先进「待发射」（state 0），延时到了才建 VM 并开始飞散。*/
static int spawn_point_at(season_card_t *s, float x, float y, float ang, float spd, int from_graze)
{
    season_point_t *p;
    float vel[3];

    if (s->n_pt >= SEASON_POINT_POOL) return 0;
    p = &s_pt[s->n_pt];
    p->id = 0;                                     /* VM 等延时结束再建 */
    p->x = x;
    p->y = y;
    p->lang = ang;
    p->lr = spd;
    ce_polar_to_vel(vel, ang, spd);                /* 引擎自己的 FSINCOS（`0x447430`），不引 libm */
    p->vx = vel[0];
    p->vy = vel[1];
    p->state = 0;
    p->mag20 = 0;
    p->age = 0;
    /* TH16 的错帧：`item+0xc60 = total_items_created & 3`（小批量时就是这个环；
     * 大批量另加 4/8/16 的偏移，见 `0x430a0e` 段）—— 让同一帧出生的一撮不叠在一起。*/
    p->delay = (uint8_t)((s->spawn_seq & 3) + 1);   /* TH16 是「减到 −1 才发射」⇒ 实际等 1..4 帧 */
    ++s->spawn_seq;
    ++s->n_pt;
    (void)from_graze;
    return 1;
}

/* 在某个坐标随机弹出（TH16 `ItemManager__spawn_item` 的调用方：角度 randf(−π..π)、
 * 速度 randf(0..1) + 1.2 → 随机方向、1.2..2.2 px/帧）。*/
static int spawn_point_burst(season_card_t *s, float x, float y)
{
    float ang = (float)((int32_t)(ce_rand() % 2001) - 1000) * (SEASON_PI / 1000.0f);   /* −π..π */
    float spd = SEASON_POINT_THROW_MIN + (float)(ce_rand() % 1001) * 0.0019f;        /* 0.2..2.1 */

    return spawn_point_at(s, x, y, ang, spd, 0);
}

/* ★ 擦弹专用：TH16 `Player__do_graze` `th16:0x444e03` 的实参 = 出生在**弹的位置**、
 * 角度 = atan2(弹 − 自机)、速度 = **1.9**（一手：`fVar4 = 1.9`）→ 沿外抛方向飞出去。
 * （敌人掉落那条通道才是「随机角度 + 1.2..2.2 速度」，见 `spawn_point_burst`。）
 * ★ 2026-09-10 第二十六轮：**去掉「推到收集圈外 6 px」**——用户口径「初始角度应该更直上直下」，
 * 而原版就是**从弹的位置**起飞的；当初推出去是为了躲「当帧被收」，现在有
 * `SEASON_POINT_INTANGIBLE`（32 帧不吸不收）兜着，可以照原样从弹上起飞了。
 * 起飞方向仍是 `弹 − 自机`（一手反汇编：`MOVSS [ESI+4]/SUBSS [EDI+0x614]` → `FLD y; FLD x;
 * CALL math_atan2` ⇒ atan2(Δy, Δx)，即**从自机指向弹**）。*/
static void spawn_point_graze(season_card_t *s, float bx, float by, float px, float py)
{
    float dx = bx - px, dy = by - py;
    float len = __builtin_sqrtf(dx * dx + dy * dy);
    float ux, uy;

    if (len < 0.001f) { dx = 0.0f; dy = -1.0f; len = 1.0f; }
    ux = dx / len;
    uy = dy / len;
    spawn_point_at(s, bx, by, bc_atan2f(uy, ux), 1.9f, 1);
}

/* ★★ 2026-09-29：**季节解放产生的季节点 = 每颗被消的弹「在弹的位置竖着向上飞」** ——
 * 用户第 1 条「产生 —— 无敌上升 —— 自动回收」里的「产生 + 上升」。
 * 一手（TH16 `gen_items_from_cancel` `th16:0x416a00` 的 **kind 4** 分支，`0x416bbb` 一处 call）：
 *     `ItemManager__spawn_item(type = 0x10 = ITEM_SEASON, pos = 被消弹的坐标,
 *                              angle = −π/2 + rand[0, π/18], speed = 2.2)`
 *   实参逐条（反汇编，`0x430960` 的栈序 = type,pos,junk,angle,speed）：
 *     `0x416b91 fmul ds:0x49444c`（= **0.17453292 = 10°**）→ `0x416ba4 subss ds:0x494534`（= **π/2**）
 *     ⇒ **角度 = −π/2 .. −π/2+10°**（几乎正上方，±10°）；`0x416bac [esp+8] = 0x400ccccd` = **2.2f** 速度。
 *   之后走原版道具 tick 的「飞散」段（我们已照抄为 `state 2`/`p->lr`）：每帧 `r -= 0.03`
 *   ⇒ **竖直上升约 2.2²/(2×0.03) ≈ 80 px、约 73 帧**，速度耗尽后转飘落（state 1）。
 *   ⇒ 「上升」是**季节点自己**上升（不是自机）；自机那一边只有「无敌」（`release()` 按档位表写
 *     `CE_PLAYER_INVULN()`，一手 = `BombSub*::begin` 的 `player+0x1663c`，见 `release()` 注释）。
 * 判据（与引擎自己的半径消弹**逐条同形**，`CE_FN_BULLET_CANCEL_RADIUS_AS_BOMB` `0x429370` 的命中段）：
 *   `state ∈ {1,2}` 且 `bullet+0x24 == 0` 且 `dist² ≤ (bullet+0x658 × 0.5 + R)²`。
 * ⚠ 必须**先扫后消**：`ce_cancel_radius()` 会把弹删掉，那时的坐标已经取不到了
 *   （旧实现在没有位置通道时只能围着自机撒，还配了个 `SEASON_CANCEL_POINT_MAX 4` 的 ❓自定闸；
 *    现在有位置了 ⇒ **闸与 `spawn_point_around_player()` 一起删**，照原版「一颗弹一颗点」）。*/
static int cancel_spawn_points(season_card_t *s, const float *pos, float radius)
{
    uint8_t *mgr = CE_BULLET_MGR();
    int n = 0;
    unsigned i;

    if (!mgr || !pos) return 0;
    for (i = 0; i < CE_BM_BULLET_COUNT; ++i) {
        uint8_t *b = mgr + CE_BM_BULLETS + i * CE_BULLET_STRIDE;
        uint16_t state = *(uint16_t *)(b + CE_BULLET_STATE);
        float x, y, dx, dy, rr, ang;

        if (state != 1 && state != 2) continue;                      /* 引擎命中段：state ∈ {1,2} */
        if (*(uint32_t *)(b + CE_BULLET_FLAGS) != 0) continue;       /* 且 bullet+0x24 == 0 */
        x = *(float *)(b + CE_BULLET_POS);
        y = *(float *)(b + CE_BULLET_POS + 4);
        dx = x - pos[0];
        dy = y - pos[1];
        rr = *(float *)(b + 0x658) * 0.5f + radius;                  /* 弹半径的一半 + 释放半径 */
        if (dx * dx + dy * dy > rr * rr) continue;
        /* 角度 = −π/2 + rand[0, 10°]（一手 `0x49444c` = 0.17453292）、速度 = 2.2（一手 `0x400ccccd`）*/
        ang = -1.5707964f + (float)(ce_rand() % 1001) * (0.17453292f / 1000.0f);
        if (spawn_point_at(s, x, y, ang, 2.2f, 0)) ++n;
    }
    return n;
}

/* ============================ 升档弹字（`+…`） ============================
 * 用户第 3 条：「季节解放之后，收齐产生的季节道具之后，会在收齐的那个点有一个 `+....` 的 UI 显示」
 * —— 本卡原来**一条弹字都没有**（改前 `grep -n "popup" native/cards/season.c` 零命中）。
 *
 * 一手（TH16，`ItemManager__on_tick` 的 **type 0x10 = ITEM_SEASON** 分支，`th16:0x42fe58`）：
 *     eax = item_collect_season();            // 0x43de50：季节槽 +1（封顶），返回「档位跨了吗」
 *     if (eax) {                              // ★ 只在**跨档**那一颗上：
 *         PlayerInner__repopulate_options();  //   0x4440e0（子机按新档重排）
 *         PopupManager::add(item.pos, −1, 0xffffff40);  // ★ 0x42fe6d..0x42fe7b（这就是那个 `+…`）
 *         play_sound(0x3f, item.pos.x);       //   0x42fe84（= 我们的 SEASON_SE_LEVEL）
 *     }
 *     add_to_score(10); Gui__update_season_gauge();     // 0x42fe8b / 0x42fe92
 *     play_sound(0x25, item.pos.x); item 收尾           // 0x42fea5 / 0x42feae（= SEASON_SE_GET）
 *   · 位置 = **道具自己的坐标**（`lea esi,[edi+0xc08]`，edi = 该 zItem）—— 「收齐的那个点」✓
 *   · 值 = **−1**：TH16 的 adder `0x44a460` 对负值只写一个字节 `0x0a`（`0x44a4dd`），
 *     而弹字走字符串系统 `font_id = 2`（`0x44a31e`）
 *   · 颜色 = **0xffffff40**（D3DCOLOR = A8R8G8B8 ⇒ α=255, R=255, G=255, B=64 = 亮黄）
 *   · 生命周期/上飘/淡出**全在引擎里**（ring 槽、约 60 帧回收；本卡不持有它的状态）
 *
 * ★ TH18 侧的**同一支**：`PopupManager__sub_4645f0` `0x4645f0` —— 与 TH16 `0x44a460` **逐条同形**
 *   （ring 基址 `mgr+0x624` vs TH16 `+0x614`，计数器 `+0x10` wrap at 10，负值同样写 `0x0a`）。
 *   一手交叉验证：TH18 自己的**力量升档**分支（`ItemManager` 的 type 3 case）就是
 *   `PopupManager__sub_4645f0(&item.pos, −1, 0xffffff40)` —— 与 TH16 季节升档**同一配方、同一颜色**
 *   ⇒ **借 TH18 原生 adder，不自己写弹字**（不是 ❓自定）。
 *   ⚠ 本仓现成的封装有两个都**不适用**：`cards/collect2x.c` 的 `ce_popup_mult()` 写的是**倍率槽**
 *     （k13–17，且只能画 ×倍率 / NO BONUS），`cards/mof.c` 的 `mof_popup_number()` 是**别人卡里的 static**
 *     （拷一份就是「两套弹字」）。⇒ 直接调原生 adder。
 *   ★ 需要的新共享面（**报告里已提**）：`sdk.h` 里加一个 `ce_popup_number(pos, value, color)`
 *     （mof.c 也早就登记了同一个需求）—— 本卡先在文件内本地声明，不动 `sdk*`。
 * ⚠ ABI：adder **自己清栈**（`0x4646c8 c2 0c 00` = `ret 0xc`）⇒ **stdcall**，
 *   三个 dword 栈参；写成 cdecl 会双重清栈（崩）。 */
typedef void (__attribute__((stdcall)) *season_popup_add_t)(const float *pos, int32_t value, uint32_t color);
#define CE_FN_POPUP_ADD_TH18  0x4645f0      /* `PopupManager__sub_4645f0`（值 = −1 就是 TH16 那个 `+…`）*/

static void popup_level_up(const float *pos)
{
    if (!pos) return;
    ((season_popup_add_t)(uintptr_t)CE_FN_POPUP_ADD_TH18)(pos, -1, 0xffffff40u);
}

/* 收一颗：加 1 点、升档检查、放音、让 VM 走 interruptLabel(1)（放大淡出后自删）*/
static void point_collect(ce_card_t *c, season_card_t *s, season_point_t *p)
{
    int old_level, new_level;

    ce_anm_interrupt((uint32_t)p->id, 1);
    ce_add_score_delta(SEASON_POINT_SCORE);              /* TH16 case 0x10 尾：add_to_score(10) */
    if (s->points >= SEASON_POINTS_MAX) return;

    old_level = level_of(s);
    s->points += SEASON_POINTS_PER_ITEM;                 /* 一颗点 = 1 个季节点（TH16 原义）*/
    if (s->points > SEASON_POINTS_MAX) s->points = SEASON_POINTS_MAX;
    new_level = level_of(s);

    if (new_level == old_level) {
        ce_play_sound(SEASON_SE_GET, ce_player_x());
        return;
    }
    ce_play_sound(SEASON_SE_LEVEL, ce_player_x());
    alloc_options(c);
    /* ★ 升档弹字：TH16 的顺序是「重排子机 → 弹字 → 升档音」；位置 = **这颗道具自己的坐标** */
    {
        float at[3];
        at[0] = p->x; at[1] = p->y; at[2] = 0.0f;
        popup_level_up(at);
    }
    ce_log("season: level up -> %d/%d (%d options, %d points)",
           new_level, SEASON_LEVEL_MAX, opts_for(new_level), (int)s->points);
}

/* 每帧推进所有季节点：飘落 / 吸引 / 收集 / 出界。
 * ★ 逐条照抄 `th16:0x42f4e0`（= TH18 `0x445a80`）的 state 1 / 3 / 4 三段。*/
static void points_tick(ce_card_t *c, season_card_t *s)
{
    /* 每 600 帧（10 s）一条掉落汇总：小怪 / boss / 被跳过（计数已清零的重复回调）*/
    if (s->frame - s_stat_frame >= 600) {
        if (s_stat_mid || s_stat_boss || s_stat_skip)
            ce_log("season: drops in %d frames: mid %d, boss %d, skipped-dup %d, points %d",
                   (int)(s->frame - s_stat_frame), (int)s_stat_mid, (int)s_stat_boss,
                   (int)s_stat_skip, (int)s_stat_points);
        s_stat_mid = s_stat_boss = s_stat_skip = s_stat_points = 0;
        s_stat_frame = s->frame;
    }
    uint8_t *pl = CE_PLAYER();
    const float *pp = ce_player_pos();
    float mag_speed = 5.0f, collect_r2 = 30.0f * 30.0f, ring_r2 = 70.0f * 70.0f, poc = 128.0f;
    float slow = 1.0f;                      /* ★ TH15/TH18 的「道具减速因子」：只作用在**飘落态** */
    int i = 0;

    {
        uint8_t *im = CE_ITEM_MANAGER();
        if (im) {
            float f = *(float *)(im + CE_ITEM_SLOWDOWN);
            if (f > 0.0f && f < 1.0f) slow = f;         /* 引擎恒 ≥0；只接受「变慢」 */
        }
    }
    if (pl) {
        float r;
        mag_speed = *(float *)(pl + CE_PLAYER_ITEM_ATTRACT_SPD);
        if (mag_speed <= 0.0f) mag_speed = 5.0f;
        r = *(float *)(pl + CE_PLAYER_ITEM_COLLECT_R);
        if (r > 0.0f) collect_r2 = r * r;
        r = *(float *)(pl + (CE_PLAYER_FOCUSED() ? CE_PLAYER_ITEM_ATTRACT_RF
                                                 : CE_PLAYER_ITEM_ATTRACT_RU));
        if (r > 0.0f) ring_r2 = r * r;
        poc = *(float *)(pl + CE_PLAYER_POC_HEIGHT);
    }

    while (i < s->n_pt) {
        season_point_t *p = &s_pt[i];
        float x, y, vx, vy;

        /* ★「待发射」：TH16 `item+0xc50 == 6` —— 只数延时、不动、VM 都还没建（`0x42f54e` 段）*/
        if (p->state == 0) {
            if (p->delay > 0) { --p->delay; ++i; continue; }
            if (!point_spawn_vm(s, p)) { point_remove(s, i); continue; }
            p->state = 2;                              /* → 飞散（TH16 state 3）*/
            ++i; continue;                             /* TH16 建完 VM 那一帧也 `goto next_item`（不动）*/
        }
        x = p->x; y = p->y; vx = p->vx; vy = p->vy;

        if (!p->id || !ce_anm_get_vm((uint32_t)p->id)) {   /* 脚本已 delete（超时）→ 回收槽 */
            point_remove(s, i);
            continue;
        }
        ++p->age;

        /* —— 回收线 / 放 B / 对话：一律真空飞行（state 3），初速 = 磁速 —— */
        /* ★ 出生后 32 帧「闪白/无敌」期内：连回收线/放B/对话都不吸 —— 让它先飞出去 */
        if (p->state == 1 && pl && p->age >= SEASON_POINT_INTANGIBLE) {
            /* 原版的门：自机 state ∈ {2,4}（死亡 / 复活）时不看回收线（`0x445ba3`/`0x445ba8`）*/
            int pstate = *(int32_t *)(pl + CE_PLAYER_STATE);
            int above_line = 0;
            if (pp && pstate != 2 && pstate != 4 && poc > pp[1]) above_line = 1;
            /* 放 B（原版门：+0x30 == 1 且帧钟 < 60）· 对话中 · **本卡自己的季节解放期间**
             * （TH16 的 subseason bomb 那条门是 `timer < 10000`，即整个解放期都收道具）*/
            if (above_line || CE_BOMB_COLLECTING() || CE_GUI_MSG_ACTIVE() || s->rel_frames > 0) {
                p->state = 3;
                p->mag20 = (uint8_t)(mag_speed * 20.0f);
                if (p->mag20 > 240) p->mag20 = 240;
            }
        }

        if (p->state == 2) {                               /* ★ 飞散（TH16 state 3）：直线 + 速度每帧 −0.03 */
            float sl = (p->age < SEASON_POINT_INTANGIBLE) ? 1.0f : slow;
            x += vx * sl;
            y += vy * sl;
            p->lr -= 0.03f;                                /* TH16 `0x42f82c`：`r -= 0.03`（不乘减速因子）*/
            if (p->lr <= 0.0f) {                           /* 飞完 → 转飘落（TH16：state 1；放 B 时 4）*/
                p->lr = 0.0f;
                vx = 0.0f;
                vy = 0.0f;
                p->state = 1;
            } else {
                float vel[3];
                ce_polar_to_vel(vel, p->lang, p->lr);      /* TH16 `cartesian_from_polar(角度, r)` */
                vx = vel[0];
                vy = vel[1];
            }
        } else if (p->state == 1) {                        /* 飘落（★ 位移与重力增量都乘减速因子，与原生同；
                                                            *   出生后 32 帧的「无敌期」不减速）*/
            float sl = (p->age < SEASON_POINT_INTANGIBLE) ? 1.0f : slow;
            vy += SEASON_POINT_GRAVITY * sl;
            if (vy > SEASON_POINT_VFALL_MAX) vy = SEASON_POINT_VFALL_MAX;
            if (vy >= 0.0f) vx = 0.0f;                     /* 原版：下落时水平速度清零 */
            x += vx * sl;
            y += vy * sl;
        } else if (pp) {                                   /* state 3/4：朝自机飞（原版 atan2 + polar）*/
            float mag = (float)p->mag20 / 20.0f;
            float dx = pp[0] - x, dy = pp[1] - y;
            float len = __builtin_sqrtf(dx * dx + dy * dy);

            if (len > 0.001f) { vx = dx / len * mag; vy = dy / len * mag; }
            else { vx = 0.0f; vy = mag; }                  /* 重合：原版给 π/2 → 朝下 */
            x += vx;
            y += vy;
            if (mag < SEASON_POINT_MAG_MAX) {
                int m = (int)p->mag20 + (int)(SEASON_POINT_MAG_STEP * 20.0f);
                p->mag20 = (uint8_t)(m > 240 ? 240 : m);
            }
        }

        p->x = x;
        p->y = y;
        p->vx = vx;
        p->vy = vy;

        /* 出界删（原版：y > 472 或 |x| >= 200）*/
        if (y > SEASON_POINT_OOB_Y || x >= SEASON_POINT_OOB_X || x <= -SEASON_POINT_OOB_X) {
            if (p->id) ce_anm_delete((uint32_t)p->id);
            point_remove(s, i);
            continue;
        }

        if (pp && p->age >= SEASON_POINT_INTANGIBLE) {
            float dx = pp[0] - x, dy = pp[1] - y;
            float d2 = dx * dx + dy * dy;

            if (d2 <= collect_r2) {                        /* 30px（读自机字段）→ 收 */
                point_collect(c, s, p);
                point_remove(s, i);
                continue;
            }
            /* ★ 2026-09-10 第二十九轮：擦弹掉的点**也要被吸**（用户实跑：正常碎片靠近时会朝你
             * 靠过来，擦弹生成的不会）。原版 TH16 里它就是普通 item（type 16），走同一套 magnet；
             * 当初加 `!p->pad` 是为了躲「一出生就被收」，现在有 `SEASON_POINT_INTANGIBLE`（32 帧
             * 不吸不收）兜着，可以照原版吸了。*/
            /* ★ 只有「飘落」才进吸引半径 —— TH16 state 3（飞散）里没有这条检查，飞完才谈吸引 */
            if (p->state == 1 && d2 <= ring_r2) {
                p->state = 4;
                p->mag20 = (uint8_t)(mag_speed * 20.0f / 3.0f);
                if (p->mag20 == 0) p->mag20 = 1;
            }
        }

        /* ★ 减速时的**抖动**：TH16/TH18 道具 tick 在 `factor < 1.0` 且道具年龄 > 31 帧时
         * 把 VM 的位置写成 `randf(−1,1)`（`th16:0x42fad2`–`0x42fb35` / `0x445a80` 同段），
         * 也就是「擦弹慢下来时道具在原地上下一像素地抖」。这里给画出来的位置加同样的 ±1 px。*/
        /* ★ 2026-09-10：绀珠传卡生效时的「可吸收道具三件套」—— **变黄 + 抖 ±1 px + 减速**。
         * 引擎自己的道具 tick 就是这么写的（`factor < 1.0` 时给道具 VM 染色 + 位置写 randf(−1,1)，
         * `factor >= 1.0` 时写回无染色；`th16:0x42fa8c`–`0x42fb35` = `th18:0x445a80` 同段），
         * 所以原生 P 点/金钱是引擎替我们做的；季节点是本卡自建的 VM，三件套得自己复刻。
         * 颜色用绀珠传卡给弹用的同一支黄（`LOLK_GRAZED_COLOR` = 0xffffd080），
         * 减速退出（slow 回 1.0）时写回 `0xffffffff` —— ★ 这一步原来漏了，所以「变黄不回来」。*/
        {
            /* 染色与抖动的门与引擎一致：**减速中 且 道具年龄 > 31 帧**。
             * ★ 第三十三轮：**只改 RGB、保留当前 alpha** —— 脚本自己有 `alphaTime(300,0,128)` 的淡出，
             * 每帧写死 alpha 255 会把它按掉（同 LoLK 弹染色那个「出圈变透明」的坑，见 lolk.c）。*/
            int tinted = (slow < 1.0f && p->age >= SEASON_POINT_INTANGIBLE);
            {
                uint8_t *vm = ce_anm_get_vm((uint32_t)p->id);
                uint32_t cur = vm ? *(uint32_t *)(vm + CE_ANM_VM_COLOR1) : 0xffffffffu;
                ce_anm_set_color((uint32_t)p->id,
                                 (cur & 0xff000000u) | (tinted ? 0x00ffd080u : 0x00ffffffu));
            }
            if (tinted) {
                float jx = (float)((int32_t)(ce_rand() % 3) - 1);
                float jy = (float)((int32_t)(ce_rand() % 3) - 1);
                ce_anm_set_pos((uint32_t)p->id, x + jx, y + jy, 0.0f);
            } else {
                ce_anm_set_pos((uint32_t)p->id, x, y, 0.0f);
            }
        }
        ++i;
    }
}

/* 爆发队列：boss 死亡排的 200 个逐帧撒出来（池满就等着）*/
static void burst_tick(season_card_t *s)
{
    int k = 0;

    while (s->burst_left > 0 && k < SEASON_BURST_PER_FRAME) {
        if (!spawn_point_burst(s, (float)s->burst_x, (float)s->burst_y)) break;
        --s->burst_left;
        ++k;
    }
}

/* 这个敌人是不是 boss / 道中 boss —— 照引擎自己的判据：EnemyManager 的 boss id 表
 * （`get_boss_enemy_full` `0x4237f0` 就是按它走链表；道中 boss 也在表里）。*/
static int enemy_is_boss(const uint8_t *e)
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

/* ★★ 第三十四轮（用户：「小怪是每次掉 8 点吗？我记得不是有一个什么小怪的出场时间推移打死
 * 掉得会更少？」—— 用户记忆正确，TH16 就是衰减的）：
 * TH16 的 `dropItems`（opcode 509，handler `th16:0x420585`）在**结算那一刻**按计时器余量线性衰减：
 *     if (bonus_timer.current < 1) n = M;
 *     else n = (N − M) * bonus_timer.current / T + M;
 * （配套 `dropSeason(T,N,M)` 的 handler `th16:0x422a26`：`Timer__set_value(&bonus_timer, T)` +
 *  `extra_counts[15] = N` + `min_count = M`。）⇒ 出场后 T 帧内击破给满 N 个，拖到计时走完只剩 M 个。
 * 一手见 engine/season/th16/01 §5.2 / §5.3。
 * ★★ 第三十五轮（用户：「th16 有不同档位吗？」→ 选「原版 T + 原版计数映射到 TH16 量级」）：
 * TH18 里拿不到 TH16 那张逐敌人的 (T,N,M) 表，但 **TH18 自己有一套同形的逐敌人档位** ——
 * `zEnemyDrop.timed_ex_drop_time`（窗口 T）+ `timed_ex_drop[type]`（数量），由 ECL opcode 573/574
 * 写在每个敌人的子程序开头，字段就在敌人身上，卡侧直接读：
 *   `n = ex_drop[i] + timed_ex_drop[i] × (T − timer)/T`（`0x430510` 一手，timer 由 `FUN_0042ff80`
 *   的 `0x430161` 段每帧 +1）—— **和 TH16 的 dropSeason 同形同向**。
 * 所以这里：**T 用敌人自己的原生 `timed_ex_drop_time`**，数量用「原生 timed 计数」当档位键映射回
 * TH16 的量级（`season_drop_native_tier`），M 恒 1。没配原版窗口的敌人（T = 0，含所有 boss）
 * 退回 TH16 小妖精那套 `(60,8,1)`。一手普查见 engine/season/th18/01 §1b′。
 * 计时器用引擎自己的 `zEnemyData.time_alive`（**zEnemyData+0x2d0**，`current` 在 +0x2d4 ⇒
 * 绝对偏移 `enemy+0x1500`，见 engine.h 的 `CE_ED_TIME_ALIVE`）——
 * 一手：敌人每帧入口 `FUN_0042ff80`（`this` = zEnemyData 基址）尾部 `0x430209` / `0x430215` 两条
 * `LEA ECX,[ESI+0x2d0]; CALL 0x405990 (Timer::increment)`（每帧 +1，随游戏速度缩放；
 * 同段 `[ESI+0x50f0]`/`[ESI+0x5104]` 就是 invuln / no_hitbox 计时器）。TH16 的计时器由 ECL 的
 * `dropSeason` 设在**小怪子程序开头** = 出场，所以用 time_alive 对齐。
 * `out_T` / `out_native` 回传原生窗口与计数（只给日志用，可为 NULL）。*/
static int mid_drop_count(const uint8_t *e, int32_t *out_T, int32_t *out_native)
{
    const uint8_t *drop = e + CE_ENEMY_DROP;
    int32_t alive = *(const int32_t *)(e + CE_ENEMY_DATA + CE_ED_TIME_ALIVE);
    int32_t T = *(const int32_t *)(drop + CE_ENEMY_DROP_TIMED_TIME);
    int32_t native = 0, i;

    for (i = 0; i < CE_ENEMY_DROP_TYPES; ++i) {            /* 取各 type 里的最大值（原生每次只配一种）*/
        int32_t v = *(const int32_t *)(drop + CE_ENEMY_DROP_TIMED_COUNTS + i * 4);
        if (v > native) native = v;
    }
    if (T <= 0 || native <= 0) {                           /* 没配原版档位 → TH16 小妖精 (60,8,1) */
        if (out_T) *out_T = SEASON_DROP_MID_TIME;
        if (out_native) *out_native = 0;                   /* 日志里 native 0 = 这个敌人没配原生窗口 */
        return (int)season_drop_count_decay(alive, SEASON_DROP_MID_TIME,
                                            SEASON_DROP_MID, SEASON_DROP_MID_MIN);
    }
    if (out_T) *out_T = T;
    if (out_native) *out_native = native;
    return (int)season_drop_count_decay(alive, T, season_drop_native_tier(native),
                                        SEASON_DROP_MID_MIN);
}

/* ① 敌人死亡 → 在**敌人自己的位置**撒季节点（TH16：`dropSeason` 写在每个敌人的 ECL 子程序里，
 * 掉落点 = `enemy->abs_pos`；数量见 SEASON_DROP_*）。
 * 池还有空位就立刻撒；装不下的进队列，由 burst_tick 逐帧补（boss 的 200 个就是这么摊开的）。*/
static void on_enemy_drop_pre(ce_card_t *c, int32_t *counts)
{
    season_card_t *s = st(c);
    const float *ep;
    uint8_t *e;
    uint32_t id;
    int n, k = 0, is_boss, alive;
    int32_t mt = 0, mn = 0;                                /* 原生窗口 T / 原生 timed 计数（日志用）*/

    if (!s || !counts) return;
    e = (uint8_t *)counts - CE_ENEMY_DROP_COUNTS - CE_ENEMY_DROP;
    id = *(const uint32_t *)(e + CE_ENEMY_ID);
    is_boss = enemy_is_boss(e);
    alive = *(const int32_t *)(e + CE_ENEMY_DATA + CE_ED_TIME_ALIVE);
    /* ★★ 第三十三轮：**用引擎自己的「计数表已清零」+ 每个敌人的闩锁当判据，替掉去重环**
     * （用户：「除了去重没有别的方法吗？」→ 后来「小怪掉得变少了、boss 还是掉两次」再订正）。
     * 一手：`Enemy__drop_items_and_notify_cards` `0x430510` 的**结尾**就是
     *   `for (卡链表) card->vt[0x30](pos, this+4); _memset(this+4, 0, 0x50);`（反编译尾段一手）——
     * 也就是**它自己把 20 个 type 的计数表清零**。所以同一个「掉落事件」的第二次回调
     * （ECL 的 `dropItems` → `0x42d0c0`）看到的计数表**必然全零**（除非 ECL 又显式配了掉落，
     * 那就是一次真的新掉落，该掉）。这比「时间窗 + 位置」的去重干净得多：不猜、不会漏、不会误吃。
     * 代价：ECL 完全没配掉落的敌人不再给季节点（TH16 那边也是「ECL 里的 `dropSeason` 决定掉不掉」✓）。*/
    {
        int32_t total = 0, ci;
        uint8_t *flag = e + CE_ENEMY_DDC_UNUSED;
        for (ci = 0; ci < CE_ENEMY_DROP_TYPES; ++ci) total += counts[ci];
        if (total > 0) {
            *flag = 1;                               /* 有掉落配置 = 这一次是真掉落 */
        } else if (!is_boss && !*flag) {
            /* ★★ 小怪的兜底：TH18 有些敌人压根没配掉落表（计数全零），但它们和 TH16 的小妖精
             * 一样该掉 8 个 ⇒ 「这个敌人还没掉过」就掉一次。闩锁 = `enemy+0x6838`
             * （zEnemy 的死字段，只有初始化写 0，见 engine.h）。
             * ⚠️ boss **不走**这条兜底：boss 的掉落是 ECL 按攻击配的（计数非零才掉），
             * 兜底会让它在没配掉落的死亡上多掉一次 = 用户看到的「boss 掉两次」。*/
            *flag = 1;
        } else {
            s_stat_skip++;
            return;                                  /* 计数已清零 且 已掉过 = 同一次事件的重复回调 */
        }
    }
    /* 背景：同一个「击破/结算事件」引擎会调这个钩子两次 —— 一次来自死亡路径
     * （`FUN_00430240`），一次来自 ECL 自己的 `dropItems` 指令（opcode 509 → `0x433abf`
     * → `0x42d0c0`）。上面的「计数表全零就跳过」就是用来吃掉第二次的（引擎在通知完卡之后
     * 自己 `_memset(this+4, 0, 0x50)`）。*/
    ep = ce_enemy_pos(e);
    if (!ep) return;
    /* ★ 小怪的数量按 TH16 的计时衰减结算（`mid_drop_count`，T/档位取敌人自己的原版
     * `timed_ex_drop_time` / `timed_ex_drop`）；boss 每个相位恒 200
     *（TH16 boss ECL 每个相位各调一次 `dropSeason(1200,200,10)`，见 NEXT.md 第 29 轮）。*/
    n = is_boss ? SEASON_DROP_BOSS : mid_drop_count(e, &mt, &mn);

    s->burst_x = (int16_t)ep[0];
    s->burst_y = (int16_t)ep[1];

    while (k < n) {
        if (!spawn_point_burst(s, ep[0], ep[1])) break;
        ++k;
    }
    if (k < n) {                                           /* 池满：剩下的排队（上限兜底）*/
        s->burst_left += n - k;
        if (s->burst_left > SEASON_DROP_CAP) s->burst_left = SEASON_DROP_CAP;
    }
    if (is_boss) s_stat_boss++; else s_stat_mid++;
    s_stat_points += n;
    if (is_boss) {                                         /* boss / 道中 boss：一行日志，方便实跑对账 */
        ce_log("season: boss drop %d points @(%.0f, %.0f) (enemy id %u, spawned %d, queued %d, t=%d)",
               n, (double)ep[0], (double)ep[1], (unsigned)id, k, (int)s->burst_left, (int)s->frame);
    } else {
        /* 小怪：把出场帧数 + 原生档位（窗口 T / 计数）一起记下来，方便实跑核对衰减
         *（`n` 从 `season_drop_native_tier(计数)` 线性降到 1，窗口 = 该敌人自己的 T）。*/
        ce_log("season: mid drop %d points @(%.0f, %.0f) (enemy id %u, alive %d, T %d, native %d, t=%d)",
               n, (double)ep[0], (double)ep[1], (unsigned)id, alive, (int)mt, (int)mn, (int)s->frame);
    }
}

/* —— 解放特效（两圈）——
 * TH16 在 `BombSub*__begin` 里建两个 VM、把半径/时长写进 VM 的脚本变量（`+0x4b0/+0x4b4/+0x4ac`），
 * 脚本 `scaleTime(10, 4, %F0, %F1)` 自己扩、`wait($I3)` 等时长、`interruptLabel(1)` 淡出。
 * 我们这边不能写脚本变量，改成**卡每帧写 scale + color_1/color_2 + 自转角**，时间轴照抄：
 *   前 10 帧：scale 0 → 半径（TH16 的 scaleTime(10,…)）
 *   之后：alpha 255 → 64（TH16 是 250 帧的 alphaTime）
 *   最后 20 帧：alpha → 0（TH16 的 interruptLabel(1) + alphaTime(20,0,0)）
 * ★ 2026-09-10（用户实测：冬天和春天一样是粉色）：**配色/边数/自转/alpha 全部按副季节查表**
 *   （`season_ring()`，一手 = 各副季节 anm 的释放脚本；冬在 `pl01sub` 的 20/21，不是 3/4）。
 *   冬 = 外粉→橙 + **内蓝→青**（用户口径「冬天是蓝色的」），土用额外上浮 48 px。
 * ⚠️ 只有**秋**的释放跟着自机走（TH16 `BombSubFall__on_tick` `0x40eeb0` 把两个 VM 同步到自机坐标）。*/
static void ring_spawn(season_card_t *s, float x, float y)
{
    float pos[3];
    season_ring_t stl;

    season_ring(s->rel_sub, &stl);
    pos[0] = x; pos[1] = y; pos[2] = 0.0f;
    s->ring_angle = 0.0f;
    s->ring_out = (int32_t)ce_anm_spawn(CE_ABILITY_ANM(), CE_ANM_ABILITY_SCRIPT_SEASON_RING_OUT, 13);
    s->ring_in  = (int32_t)ce_anm_spawn(CE_ABILITY_ANM(),
                                        stl.inner_poly == 12 ? CE_ANM_ABILITY_SCRIPT_SEASON_RING_IN12
                                                             : CE_ANM_ABILITY_SCRIPT_SEASON_RING_IN, 12);
    if (s->ring_out) ce_anm_set_pos((uint32_t)s->ring_out, pos[0], pos[1], 0.0f);
    if (s->ring_in)  ce_anm_set_pos((uint32_t)s->ring_in, pos[0], pos[1], 0.0f);
}

static void ring_hide(season_card_t *s, int fade)
{
    if (s->ring_out) {
        if (fade) ce_anm_interrupt((uint32_t)s->ring_out, 1);
        else      ce_anm_delete((uint32_t)s->ring_out);
        s->ring_out = 0;
    }
    if (s->ring_in) {
        if (fade) ce_anm_interrupt((uint32_t)s->ring_in, 1);
        else      ce_anm_delete((uint32_t)s->ring_in);
        s->ring_in = 0;
    }
}

static void ring_tick(season_card_t *s, const float *pp)
{
    float grow, a;
    int elapsed, remain;

    if (!s->ring_out && !s->ring_in) return;
    if (s->rel_frames <= 0) return;

    if (s->rel_sub == SEASON_SUB_FALL && pp) {          /* 秋：圆心跟着自机 */
        s->rel_x = pp[0];
        s->rel_y = pp[1];
    }
    elapsed = s->rel_total - s->rel_frames;             /* 已经放了多久 */
    remain = s->rel_frames;
    grow = (elapsed >= 10) ? 1.0f : (float)elapsed / 10.0f;
    a = 255.0f - 191.0f * ((float)elapsed / 250.0f);
    if (a < 64.0f) a = 64.0f;
    if (remain <= 20) a = a * (float)remain / 20.0f;    /* 最后 20 帧淡出 */
    if (a < 0.0f) a = 0.0f;
    {
        season_ring_t stl;
        uint32_t ai = (uint32_t)a & 0xffu;
        float sc = s->rel_radius * grow;
        /* 土用 `posTime(60, 1, 0, 48, 0)`：60 帧线性上浮 48 px（其余季节 drift_y = 0）*/
        float dy = 0.0f;

        season_ring(s->rel_sub, &stl);
        if (stl.drift_y != 0.0f) {
            float t = (elapsed < 60) ? (float)elapsed : 60.0f;
            dy = stl.drift_y * (t / 60.0f);
        }
        /* `angleVel(±0.17453292)`：外圈 +、内圈 −；春的 spin = 0（不自转）*/
        s->ring_angle += stl.spin;

        if (s->ring_out) {
            uint32_t ao = ((ai * (uint32_t)stl.alpha_out) / 255u) << 24;
            ce_anm_set_pos((uint32_t)s->ring_out, s->rel_x, s->rel_y + dy, 0.0f);
            ce_anm_set_scale((uint32_t)s->ring_out, sc, sc);
            ce_anm_set_rotation((uint32_t)s->ring_out, 0.0f, 0.0f, s->ring_angle);
            ce_anm_set_color((uint32_t)s->ring_out, ao | (stl.outer_c1 & 0x00ffffffu));
            ce_anm_set_color2((uint32_t)s->ring_out, ao | (stl.outer_c2 & 0x00ffffffu));
        }
        if (s->ring_in) {
            uint32_t ai2 = ((ai * (uint32_t)stl.alpha_in) / 255u) << 24;
            /* ★★ 第三十三轮：**第二个圆比第一个大 16**（一手 `BombSubSpring__begin` 反编译：
             *   `%F0/%F1 = RELEASE_LEVEL_RADIUS_<季节>[level]` 给脚本 3 的 VM，
             *   脚本 4 的 VM 拿 `RELEASE_LEVEL_RADIUS_<季节>[level] + 16.0`）——
             *   所以 TH16 是**两个同心圆**（外圈亮/加算那个在半径 r+16、暗的那个在 r），
             *   不是「一个沿 r 渐变的圆」（用户实跑口径）。我们原来两圈同尺寸 ✗。*/
            float si = sc + 16.0f;
            ce_anm_set_pos((uint32_t)s->ring_in, s->rel_x, s->rel_y + dy, 0.0f);
            ce_anm_set_scale((uint32_t)s->ring_in, si, si);
            ce_anm_set_rotation((uint32_t)s->ring_in, 0.0f, 0.0f, -s->ring_angle);
            ce_anm_set_color((uint32_t)s->ring_in, ai2 | (stl.inner_c1 & 0x00ffffffu));
            ce_anm_set_color2((uint32_t)s->ring_in, ai2 | (stl.inner_c2 & 0x00ffffffu));
        }
    }
}

/* —— 解放残像（TH16 `Player__on_tick` `0x442c76` / `0x442d33` 一手）——
 * 天空璋的「加速态」下，自机**每帧**在自己位置建一个残像 VM：
 *   `AnmLoaded__create_40e5c0([player+0xc], &out, 脚本表[CHARACTER], 自机坐标, 0, -1)`
 *   → 脚本表 `0x494800` = {10, 9, 13, 16, 0}（按**角色**，不是副季节！`0x4a57a4` = `CHARACTER`），
 *     再 `AnmLoaded__set_sprite(自机 anm, vm, [player+0x34])`（照自机当下那帧的 sprite）+ 写 color_1：
 *     **加速态**（秋：`BombSubFall__on_tick` 每帧写 `player+0x16688 = 1.5`）→ 黄 `0xffffff00`；
 *     **速度缩放态**（冬：`BombSubWinter__on_tick` 每帧写 `player+0x2c7cc = 1.5` → `EnemyManager` 置
 *     `player+0x1664c` bit0x20）→ 蓝 `0xffff0000`。无敌中走另一支（只闪色、**不**建残像）。
 * 脚本本体（TH18 原生 `pl0X.anm`，逐条对上 TH16 的同一支）：
 *   `layer(13); type(1); sprite(sprite0); color(60,0,192); alphaTime(20,0,0); +20: delete()`
 *   —— 一张自机本体、20 帧淡出。脚本号按角色：pl00 = 17、pl01 = 19、pl02 = 14、pl03 = 14。
 * ⚠️ TH18 的引擎把这段删了（`Player__on_tick__body` `0x45c6e4` 直接跳到 `0x45c702`，没有建 VM 的调用），
 *    但原版脚本还在 ⇒ 这里补的是**天空璋的行为**，数值/条件全部照 TH16 的地址。*/
static void afterimage_tick(season_card_t *s, const float *pp)
{
    static const int32_t k_script[4] = { 17, 19, 14, 14 };   /* pl00/pl01/pl02/pl03 的残像脚本 */
    ce_timer_t *inv;
    uint8_t *pl;
    void *anm;
    uint32_t id;
    int32_t ch, sprite;

    if (!pp) return;
    /* 只有「加速态」的两个副季节（秋 / 冬）会建残像 —— 春/夏/土用的释放不写速度字段 */
    if (s->rel_sub != SEASON_SUB_FALL && s->rel_sub != SEASON_SUB_WINTER) return;
    inv = CE_PLAYER_INVULN();
    if (inv && inv->cur > 0) return;               /* 无敌中：TH16 走只闪色的那一支 */
    pl = CE_PLAYER();
    if (!pl) return;
    anm = *(void **)(pl + CE_PLAYER_ANM);
    if (!anm) return;
    ch = *(volatile int32_t *)CE_ADDR_CHARACTER;
    if (ch < 0 || ch > 3) ch = 0;
    id = ce_anm_spawn(anm, k_script[ch], 13);      /* 脚本自己写 layer(13)，这里只是给实例化的默认层 */
    if (!id) return;
    sprite = *(int32_t *)(pl + CE_PLAYER_VM + CE_ANM_VM_SPRITE_ID);
    if (sprite > 0) ce_anm_set_sprite(anm, id, sprite);
    ce_anm_set_pos(id, pp[0], pp[1], 0.0f);
    /* ★ 第三十三轮（用户录屏对账 + 一手反汇编 `0x442ccc`–`0x442d33`）：
     *   · **冬**：TH16 在残影 VM 出生后写一次 `color_1 = 0xffff0000`（红）✓ 照抄；
     *   · **秋**：TH16 **不写残影的颜色**，只把**自机本体**染黄（`player+0x538 = 0xffffff00`）——
     *     残影保持脚本自带的 `color(60,0,192)`（= 蓝紫，RGB 顺序，见 season_core.c 的 RGB888 注释），
     *     这正是用户视频里看到的「秋天蓝色虚影」。我们原来把黄写到了残影上 ✗。
     *   · 颜色只在出生写一次：每帧重写会按掉脚本 `alphaTime(20,0,0)` 的淡出（残影变成一整块不透明）。*/
    if (s->rel_sub == SEASON_SUB_WINTER) ce_anm_set_color(id, 0xffff0000u);
}

/* ④ 季节解放：按副季节查表，开始一段持续清弹 + 伤害 + 无敌 */
static void release(ce_card_t *c, season_card_t *s);   /* 定义在下面（`on_activate` 要用）*/

/* ============================ 主动卡发动（`on_activate` / 用户的 C 键） ============================
 * 用户 2026-09-12 定案：**116 是主动卡** —— 「用 D 键切换到该卡，然后按 C」触发**季节解放**
 * （原先是读 A 键的边沿）。⇒ 本卡声明 `active_recharge`，门槛留在 `on_activate` 里：
 * **档位 0 / 正在解放 ⇒ `ce_refuse`**（SDK 会把刚装填的充能退回、**不算发动**）。
 *
 * ⚠ `active_recharge = 1`（不是真冷却）：先例 = 117 咆哮（`cards/animal.c`）—— 它的真代价是**槽**
 *   而不是时间 ⇒ 充能只作「SDK 主动卡机器的入场券」。本卡的真代价是**季节点档位**
 *   （春/秋/冬 清空槽、夏/土用 只扣一档，见 `season_points_after_release`）⇒ 同一形状。 */
static int on_activate(ce_card_t *c)
{
    season_card_t *s = st(c);
    if (!s) return ce_refuse("SEASON", "no state slot");
    if (level_of(s) <= 0) return ce_refuse("SEASON", "level 0");
    if (s->rel_frames > 0) return ce_refuse("SEASON", "release in progress");
    release(c, s);
    return 1;          /* 持续态：释放期间保持「正在释放」；`on_active_tick` 返回 0 时由 SDK 收尾 */
}

static int on_active_tick(ce_card_t *c, uint32_t elapsed)
{
    season_card_t *s = st(c);
    (void)elapsed;                          /* 帧数由 `release_tick` 自己推（`s->rel_frames`）*/
    return s && s->rel_frames > 0;
}

static void release(ce_card_t *c, season_card_t *s)
{
    season_profile_t pr;
    ce_timer_t *inv;
    int level = level_of(s);
    int32_t before;

    if (level <= 0) return;                      /* 没档不能放（TH16 `can_bomb` 同）*/
    if (s->rel_frames > 0) return;               /* 正在放：忽略（TH16 有 45 帧冷却）*/

    season_profile(s->subseason, level, &pr);
    s->rel_sub = s->subseason;
    s->rel_frames = pr.duration;
    s->rel_total = pr.duration;
    s->rel_radius = pr.radius;
    s->rel_dmg = pr.damage;
    s->rel_cancelled = 0;
    s->rel_acc = 0;
    {
        const float *pp = ce_player_pos();
        s->rel_x = pp ? pp[0] : 0.0f;
        s->rel_y = pp ? pp[1] : 0.0f;
    }
    ring_hide(s, 0);                                   /* 上一次的残余（正常不会走到）*/
    ring_spawn(s, s->rel_x, s->rel_y);

    inv = CE_PLAYER_INVULN();
    if (inv && inv->cur < pr.invuln) {
        inv->prev = inv->cur;
        inv->cur = pr.invuln;
        inv->cur_f = (float)pr.invuln;
    }
    if (s->rel_sub == SEASON_SUB_WINTER) ce_set_bullet_damage_permille(c, SEASON_DMG_WINTER);

    ce_play_sound(SEASON_SE_RELEASE, ce_player_x());

    before = s->points;
    s->points = season_points_after_release(s->subseason, s->points);
    /* ★ 子机数 = 档位（TH16 `PlayerInner__repopulate_options` 的 `for (i = 0; i < level; i++)`，
     * 余下的槽清空 —— `engine/player/th16/04` §3b），而 `Bomb__activate_bomb` **在扣槽之后**
     * 调它（`th16:0x40db20` 的调用点清单）⇒ 春/秋/冬 释放后档位 0 = **一颗子机都不留**，
     * 夏/土用 只扣一档 ⇒ 子机少一颗。这里**无条件**重排（不只在档位变化时），保证和档位一致。*/
    alloc_options(c);

    ce_log("season: release #%d (%s L%d: r=%.0f, %d frames, %d dmg/frame, invuln %d; points %d -> %d, level -> %d, options %d)",
           ++s->releases, season_name(s->rel_sub), level, (double)pr.radius, (int)pr.duration,
           (int)pr.damage, (int)pr.invuln, (int)before, (int)s->points,
           level_of(s), opts_for(level_of(s)));
}

/* 解放逐帧：半径清弹（在弹的位置撒季节点）+ 伤害源 + 秋加速 */
static void release_tick(ce_card_t *c, season_card_t *s)
{
    const float *pp;
    float pos[3];
    int n;

    if (s->rel_frames <= 0) return;
    pp = ce_player_pos();
    if (!pp) return;
    ring_tick(s, pp);
    afterimage_tick(s, pp);                    /* 秋/冬 的加速态：每帧一个自机残像（TH16 同款）*/
    /* ★ 清弹/伤害的圆心 = 释放原点（TH16 的 `on_tick` 用**特效 VM 的位置**；只有秋跟着自机）*/
    pos[0] = s->rel_x;
    pos[1] = s->rel_y;
    pos[2] = pp[2];

    /* ★★ 2026-09-29：**先撒季节点、再消弹**（顺序不能反：`ce_cancel_radius` 会把弹删掉，
     * 之后就取不到「弹的位置」了）。撒的点照 TH16 kind 4：在**弹的位置**、角度 −π/2 ±10°、
     * 速度 2.2 ⇒ 竖直上升（见 `cancel_spawn_points()` 的一手注）。 */
    cancel_spawn_points(s, pos, s->rel_radius);

    /* mode = **4**：用户定案（2026-09-10）——「相当于一个耦合：同时掉落季节点和钱」。
     * 引擎的 `Bullet__cancel` 末尾调 `gen_items_from_et_cancel(pos, mode)`（一手 `th18:0x428e90`
     * → `th18:0x4290e0`）：`1` → type 9、`4` → 多数 type 9 + 少数 type 2（金钱）、`0` → 直接 return；
     * 所以传 4 ⇒ 引擎撒钱，本卡再按消弹数自己撒季节点 ⇒ 两者同时掉。
     * ⚠ **TH16 的 mode 4 = 季节道具（type 0x10）**（一手 `th16:0x416a00`），
     *   TH18 里没有 type 0x10 这个道具 ⇒ 无法 1:1，「引擎撒钱 + 自制季节点」是**刻意的耦合，不是复刻**
     *   （2026-09-15 六作消弹口径一手核查后仍保留此取舍）。*/
    n = ce_cancel_radius(pos, s->rel_radius, SEASON_CANCEL_MAX, 4);
    if (n > 0) s->rel_cancelled += n;

    if (s->rel_dmg > 0) {
        float d = s->rel_radius * 2.0f;
        ce_raise_damage_cap(SEASON_DMG_CAP);
        ce_damage_rect(pos, 0.0f, 2, s->rel_dmg, d, d);
    }

    if (s->rel_sub == SEASON_SUB_FALL) {
        uint8_t *pl = CE_PLAYER();
        /* 乘而不是赋值：别的卡（`sj.c`）也在乘同一字段，引擎每帧把它复位 1.0（engine.h `0x477ec`）*/
        if (pl) *(float *)(pl + CE_PLAYER_SPEED_MULT) *= SEASON_SPEED_FALL;
    }

    if (--s->rel_frames == 0) {
        ring_hide(s, 1);                                   /* 特效淡出（脚本 interruptLabel(1)）*/
        if (s->rel_sub == SEASON_SUB_WINTER) ce_set_bullet_damage_permille(c, 0);   /* 0 = 从伤害聚合里摘掉 */
        ce_log("season: release done (%s, %d frames, %d bullets cancelled)",
               season_name(s->rel_sub), (int)s->rel_total, (int)s->rel_cancelled);
    }
}

/* 擦弹 → 季节点（TH16 `Player__do_graze` 每颗擦到的弹都撒一颗）。
 * ★ 2026-09-10：改读**引擎自己**的擦弹断点（`0x42497f`，SDK `ce_graze_take`）——
 * 它给出「刚被擦到的那颗弹」的坐标，比扫弹池猜可靠得多（TH18 的 `bullet+0x20 |= 4` 一生一次，
 * 「最近的已擦弹」可能是自机背后的老弹）。断点没命中（比如只擦了激光/敌机）就退回自机周围。*/
/* ★ 2026-09-10：断点 `ce_graze` 会让**游戏开不了机**（C0000005，堆被写坏）→ 摘掉不用。
 * 改成**只读**的读法：每帧扫弹池，按引擎自己的擦弹判据（`engine/player/th18/02` §1b：
 * 圆弹 d² < (r+max(br/2.5,40))²+br²；方弹判定框 +24 px）挑出**在圈里且最靠近边界**的那颗
 * （d²/zone 最大 = 刚跨进来），拿它的坐标撒点。全程只读内存，零开机风险。*/
#define SEASON_HITBOX_HW       0x479a4
#define SEASON_HITBOX_HH       0x479a8
#define SEASON_HIT_RADIUS      0x4799c
#define SEASON_GRAZE_RECT_MARGIN  24.0f
#define SEASON_GRAZE_CIRCLE_MIN   40.0f
#define SEASON_GRAZE_CIRCLE_DIV   2.5f

static int graze_pick(float px, float py, float *bx, float *by)
{
    uint8_t *mgr = CE_BULLET_MGR();
    uint8_t *pl = CE_PLAYER();
    float collect_r = 30.0f, rect_hw = 24.0f, rect_hh = 24.0f, pr = 3.0f;
    float best = -1.0f;
    unsigned i;

    if (!mgr) return 0;
    if (pl) {
        float r = *(float *)(pl + CE_PLAYER_ITEM_COLLECT_R);
        if (r > 0.0f) collect_r = r;
        rect_hw = *(float *)(pl + SEASON_HITBOX_HW) + SEASON_GRAZE_RECT_MARGIN;
        rect_hh = *(float *)(pl + SEASON_HITBOX_HH) + SEASON_GRAZE_RECT_MARGIN;
        pr = *(float *)(pl + SEASON_HIT_RADIUS);
    }
    for (i = 0; i < CE_BM_BULLET_COUNT; ++i) {
        uint8_t *b = mgr + CE_BM_BULLETS + i * CE_BULLET_STRIDE;
        uint32_t flags;
        float x, y, dx, dy, d2, zone, ratio;

        if (*(uint16_t *)(b + CE_BULLET_STATE) == 0) continue;
        flags = *(uint32_t *)(b + CE_BULLET_FLAGS);
        if (!(flags & 0x2)) continue;
        x = *(float *)(b + CE_BULLET_POS);
        y = *(float *)(b + CE_BULLET_POS + 4);
        dx = x - px;
        dy = y - py;
        d2 = dx * dx + dy * dy;
        if (d2 < (collect_r + 6.0f) * (collect_r + 6.0f)) continue;   /* 圈内：撒在这儿会被当帧收走 */
        if (flags & 0x10) {
            float br = *(float *)(b + 0x658);
            float extra = br / SEASON_GRAZE_CIRCLE_DIV;
            if (extra < SEASON_GRAZE_CIRCLE_MIN) extra = SEASON_GRAZE_CIRCLE_MIN;
            zone = (pr + extra) * (pr + extra) + br * br;
        } else {
            if (dx > rect_hw || -dx > rect_hw || dy > rect_hh || -dy > rect_hh) continue;
            zone = rect_hw * rect_hw + rect_hh * rect_hh;
        }
        if (d2 >= zone) continue;
        ratio = d2 / zone;
        if (ratio > best) { best = ratio; *bx = x; *by = y; }
    }
    return best >= 0.0f;
}

static void graze_tick(season_card_t *s)
{
    const float *pp = ce_player_pos();
    int32_t g = *(volatile int32_t *)SEASON_GRAZE_ADDR;
    int32_t delta, i;
    float gx = 0.0f, gy = 0.0f;

    if (!pp) return;
    if (s->last_graze < 0 || g < s->last_graze) { s->last_graze = g; return; }
    delta = g - s->last_graze;
    s->last_graze = g;
    if (delta <= 0) return;
    if (delta > SEASON_GRAZE_PER_FRAME) delta = SEASON_GRAZE_PER_FRAME;
    if (!graze_pick(pp[0], pp[1], &gx, &gy)) { gx = pp[0]; gy = pp[1] - 24.0f; }
    for (i = 0; i < delta; ++i) spawn_point_graze(s, gx, gy, pp[0], pp[1]);
}

/* ⑤ 每帧 */
/* ★★ 2026-10-02（Jera 88「杰拉」）：本卡场上的**季节点**交给 SDK 的「复制代数闸」——
 * 每帧上报可见数，代数一变就照原档各重撒一颗（`spawn_point_at` 用**同一角度 / 同一起始速度**，
 * 与它出生的那一次逐字同源）。语义见 `sdk_core.h` 末节与 `cards/jera.c` 文件头。
 * ⚠ 先存 `n0` 再循环：`spawn_point_at` 是**追加**到池尾，边遍历边追加会把复制品再复制一遍。*/
static uint32_t s_dup_seen;

static int on_tick_2(ce_card_t *c)
{
    season_card_t *s = st(c);
    int i;

    if (!s || !CE_PLAYER()) return 0;
    ++s->frame;

    ce_dup_publish((uint32_t)ce_card_id(c), (int)s->n_pt);
    if (ce_dup_should_run(&s_dup_seen)) {
        int32_t n0 = s->n_pt, k;
        for (k = 0; k < n0; ++k)
            (void)spawn_point_at(s, s_pt[k].x, s_pt[k].y, s_pt[k].lang, s_pt[k].lr, 0);
        ce_log("season: jera dup %d/%d points", (int)(s->n_pt - n0), (int)s->n_pt);
    }

    graze_tick(s);
    if (ce_sweep_should_run(&s_sweep_seen)) season_sweep_to_money(s);   /* ★ 92 清扫：季节道具 → 金钱 */
    points_tick(c, s);
    burst_tick(s);

    /* ★ 2026-09-12 用户定案：季节解放**不再读 A 键** —— 本卡是**主动卡**，
     *   「用 D 键切换到该卡，然后按 C」触发（实现在下面的 `on_activate()`）。
     *   ⚠ 原来这里是 `ce_key_down(CE_VK_A)` 的边沿。 */

    /* ★★ 2026-09-29 用户口径：副季节切换 = **完整双击 Z**（「不按 Z，按 Z，放开，再按」）。
     *   ❓**自定、不是复刻**（TH16 根本没有切季节的键，见 `SEASON_INPUT_Z` 的注释）；
     *   状态机在 `season_core.c` 的 `season_dtap_tick()`：**只在第二次按下那一帧返回 1**
     *   （一次手势只切一档、不连发），中间那一下松手必须真的发生，整个手势不超过
     *   `SEASON_DTAP_WINDOW`（20 帧 ≈ 330 ms）。窗口与去重规则都是 ❓自定，见 `season_core.h` 头注。
     *
     *   替换掉的东西（2026-09-21 owner 口径的 Shift+D）：
     *     · 旧门 `(held & 0x008) && (INPUT_PRESSED & 0x800)`；
     *     · 旧阻断 `if (held & 0x008) INPUT_PRESSED &= ~0x800;` —— 它让**按住 Shift 时引擎的 D
     *       （切换主动卡）按不动**（用户的 D 就是「切到本卡」），本次一并删掉。
     *   ⇒ 本卡现在**一个输入掩码位都不写**（只读 `INPUT_HELD` 的 Z 位），引擎自己的 C/D 语义完整保留。
     *   ⚠ Z 同时也是开火键 ⇒ 双击那两下**会各打一帧**（用户明确要这个）；本卡不吞任何输入帧。*/
    if (season_dtap_tick(&s->dtap, (CE_INPUT_HELD() & SEASON_INPUT_Z) != 0)) {
        s->subseason = season_next(s->subseason);
        ce_play_sound(SEASON_SE_SWITCH, ce_player_x());
        refresh_options(c, s);                       /* ★ 子机**整批重建**：脚本/贴图/阵位都跟着换 */
        ce_log("season: sub-season -> %s (double-tap Z)", season_name(s->subseason));
    }

    release_tick(c, s);

    /* ★ 2026-09-15g：HUD 压暗的每帧推进**移到 `on_draw`**（`hud_slot_sync()` 之后）——
     * 门限现在由**本卡的块矩形**推，而块锚点是每帧从货架取的（`s_slot`）⇒ 必须在取完锚点之后算，
     * 用上一帧的锚点会晚一帧。原来这里那一份（TH16 `Gui::on_tick_20` `th16:0x427cf0` 的闩锁 + 迟滞 +
     * C 侧 64/±51）已按用户口径「UI 统一」收进共享件，原一手值见上面常量区的留档注释。*/

    for (i = 0; i < SEASON_OPTS; ++i) {
        if (i < opts_for(level_of(s))) {
            if (!ce_option_claim(c, &s->opt[i])) {
                uint8_t *o = ce_option_retry(c, &s->opt[i], opt_dx(i),
                                             season_opt_script(s->subseason),
                                             SEASON_RETRY, "season");
                if (o) {
                    season_opt_pos_t pos;
                    if (season_option_pos(s->subseason, level_of(s), i, &pos)) set_option_offsets(o, &pos);
                    set_option_art(o, s->subseason);
                }
            }
        } else if (s->opt[i].ptr) {
            /* 兜底：档位掉下来但 `on_power_level_change` / 释放路径没跑到时，每帧也把多余的收掉
             *（TH16 的子机数恒 = 档位；用户 2026-09-10：解放归零后不该还有季节子机）。*/
            ce_option_drop(c, &s->opt[i]);
        }
    }
    return 0;
}

/* 副季节 → shooterset（五组，数值照 TH16 `plXXsub.sht`；见 assets/sht/append_shooterset.py）*/
static int season_shooterset(int subseason)
{
    switch (subseason) {
    case SEASON_SUB_SPRING: return CE_SHT_SET_SEASON_SPRING;
    case SEASON_SUB_SUMMER: return CE_SHT_SET_SEASON_SUMMER;
    case SEASON_SUB_FALL:   return CE_SHT_SET_SEASON_FALL;
    case SEASON_SUB_WINTER: return CE_SHT_SET_SEASON_WINTER;
    default:                return CE_SHT_SET_SEASON_DOYOU;
    }
}

/* ③ 射击帧：**每颗子机各发自己那发**，节奏用引擎的 short_timer（= 本卡的帧计数）。
 * ★ 2026-09-10 改：原来传 `frame × 档位`（越攒越快）+ 只有一组「直飞种子」——
 * 与 TH16 不符（TH16 的 fire_rate 固定：春/夏/秋 5 帧、冬 15 帧、土用 3 帧；档位只决定子机数与伤害）。
 * 现在按副季节选 set：春 = 寻的（func init1/tick1）、夏 = 直飞 10、秋 = 匀加速（tick3）、
 * 冬 = 激光（init2/tick2/hit2）、土用 = 向后 24 速。*/
static int on_tick_shooters(ce_card_t *c, uint32_t short_timer, uint32_t long_timer)
{
    season_card_t *s = st(c);
    uint8_t *o;
    int i, level, set;

    (void)short_timer;
    (void)long_timer;
    if (!s) return 0;
    level = level_of(s);
    if (level <= 0) return 0;
    set = season_shooterset(s->subseason);
    for (i = 0; i < opts_for(level); ++i) {
        o = ce_option_claim(c, &s->opt[i]);
        if (!o) continue;
        ce_tick_shooters_for_card(o, (int)s->frame, 0, set);
    }
    return 0;
}

/* —— ⑤ HUD —— */

/* ★★ 2026-09-12：**在 `on_load`（每关开场）认领货架槽**（`HUD-LAYOUT.md` §3/§4.4）。
 * `ce_hud_slot_claim_card()` = 先把关卡号推进纪元（关卡号变了 ⇒ 上一关的占位先清空）再认领，
 * 认领**幂等**（`on_load` 每关都会再调一次，重复认领返回同一个槽）。
 * 释放不在这里做：删卡时 `on_delete` 走 `ce_layer_drop_card()` → `ce_hud_slot_release()`
 * （§4.4：局中删卡只「保留占位」，不当场重排，下关开场自然收敛）。*/
static void hud_slot_claim(season_card_t *s)
{
    ce_hud_block_t blk;
    int32_t ok;

    memset(&blk, 0, sizeof blk);
    blk.w = SEASON_HUD_BLOCK_W;
    blk.h = SEASON_HUD_BLOCK_H;
    /* ★ 2026-09-13 用户口径：改用**自带锚点**认领（照 117 的写法）——
     * 登记进槽位表（`ce_hud_slot_get()` 取得到）、但**货架不会动它**、别的块也不会为它让位；
     * 关卡纪元照样清 ⇒ 每关开场（`on_load`）重新认领。*/
    blk.side = CE_HUD_SIDE_PINNED;
    ok = ce_hud_slot_claim_pinned_card(116, &blk, SEASON_HUD_PIN_X, SEASON_HUD_PIN_Y, &s_slot);
    /* ★★ 2026-09-13 修 bug（实跑日志 7419 行 + HUD 位置错）：认领结果**必须写进卡字段**
     * `s->hud_slot_ok` —— 因为 `hud_slot_sync()` 与 `hud_ensure()` 读的都是**它**。
     * 原来只写文件静态 `s_slot_ok` ⇒ 两个后果：
     *   ① `hud_ensure()` 每帧都以为「本卡还没认领到」⇒ **每帧重认领**（日志刷屏）；
     *   ② `hud_slot_sync()` 永远走 `!ok` 那条 ⇒ 退回默认锚点 `CE_HUD_ANCHOR_*`，
     *      **从不 `ce_hud_slot_get()`** ⇒ 拿不到货架按 card_id 排给我们的槽位（**HUD 位置错**）。
     * 现在两处同源：`s->hud_slot_ok` 是唯一真值；`s_slot_ok` 只留作「上一次的值」用于日志去重。*/
    if (ok != s_slot_ok) {                         /* 只在结果**变化**时打日志（重认领不再刷屏）*/
        if (ok) {
            ce_log("season: HUD block %.0fx%.0f claimed at surface (%.0f, %.0f)",
                   (double)blk.w, (double)blk.h, (double)s_slot.x, (double)s_slot.y);
        } else {
            ce_log("season: ce_hud_slot_claim_card FAILED");
        }
    }
    s_slot_ok = ok;
    if (s) s->hud_slot_ok = ok;
}

/* 每帧取**最新**锚点。⚠ 两个坑（`sdk_core.c` 一手）：
 *   ① `ce_hud_slot_claim_card()` 给的是「认领那一刻」的坐标，同一开场批次里 id 更大的卡
 *      后认领会重排 ⇒ 必须每帧 `get`（开销可忽略）；取不到（还没认领 / 本关被 reset）就退回默认锚点；
 *   ② `ce_hud_slot_get()` **只填 x/y/w/h/side，不填 x640/y640**（认领时填、get 时不填）
 *      ⇒ 直接读 `s_slot.x640` 是上一关的陈旧值，这里按 SDK 口径自己换算（`CE_HUD_SURFACE_TO_640` = 0.5）。*/
static void hud_slot_sync(season_card_t *s)
{
    (void)s;
    /* ★★★ 2026-10-03s（owner 十二合一实跑：「112 和 117 在左下角重叠，甚至和主动卡重叠……依旧有大量重叠」
     *   ⇒ 查出**真凶就是这里**）：本函数原来**无条件**把槽覆盖回写死的 `SEASON_HUD_PIN_*`
     *   （注释记的是 2026-09-13 的口径「本卡不走货架」）—— 于是**分配器怎么排 116 都不听**：
     *   别人撞了会回退让位，116 却永远钉在 68..172 ⇒ 它和谁都能叠。
     *
     *   现行口径（owner 2026-10-03 三条，覆盖 09-13 那条）：
     *     ·「**包含左下角 HUD 的都应当进这个系统**」；
     *     ·「**所有左下角 HUD 应当是"一块 HUD"**」；
     *     · 本卡认领走 `ce_hud_slot_claim_pinned_card()` ⇒ 会被分配器**判碰撞 + 必要时回退货架**。
     *   ⇒ 现在**每帧从分配器取最新槽**（与 110/111/117 同款）；只有**取不到**（还没认领 / 本关被 reset）
     *     才退回声明锚点兜底 —— 那份兜底**不再覆盖**分配器给出的位置。
     *   ⚠ 取不到时用 `SEASON_HUD_PIN_*`（声明的左下角）而不是 0：首次认领前的那几帧不该把 UI 画到左上角。*/
    if (ce_hud_slot_get(116u, &s_slot)) {
        s_slot.x640 = s_slot.x * CE_HUD_SURFACE_TO_640;
        s_slot.y640 = s_slot.y * CE_HUD_SURFACE_TO_640;
        return;
    }
    s_slot.x = SEASON_HUD_PIN_X;
    s_slot.y = SEASON_HUD_PIN_Y;
    s_slot.x640 = s_slot.x * CE_HUD_SURFACE_TO_640;
    s_slot.y640 = s_slot.y * CE_HUD_SURFACE_TO_640;
}

static void hud_ensure(season_card_t *s)
{
    float pos[3];

    /* ★ 2026-09-12：万一 `on_load` 那次认领失败（或本关的槽被 SDK 的纪元重置清掉了），
     * 这里补一次 —— 认领是幂等的，正常路径只有开场那一次真正落账。
     * ⚠ 只在「本卡状态说没认领到」时补：不看 `ce_hud_slot_claimed()`，因为局中删卡时 SDK
     * 只把占位标记为「保留」、`claimed` 仍为真，那种情况不该重新认领（§4.4 不当场重排）。*/
    /* ★ 2026-10-03z7（审计 P1-1）：补上 `ce_hud_slot_claimed()` 判据 ——
     *   纪元清表后 `hud_slot_ok` 仍是 1、表里却没有我们 ⇒ 原来会整关停在兜底常量、不进系统。
     *   （`reserved`（局中删卡的保留位）在 SDK 里 `claimed` 仍为真 ⇒ 不会被误判重认领 ✓）*/
    if (!s->hud_slot_ok || !ce_hud_slot_claimed(116u)) hud_slot_claim(s);   /* ★ 传卡指针：认领结果会写回 `s->hud_slot_ok`（见 hud_slot_claim 的注释）*/

    pos[2] = 0.0f;
    /* ★ 2026-09-10 第二十九轮：任何 VM 被**重新生成**（换关时引擎清过 anm，或 VM 被删）都要把
     * `hud_last_state` 打回 -1 —— 否则 interrupt（2/3 = 进度色、8..13 = 档位色、4/5 = 淡入淡出）
     * 不会再发，新 VM 就停在脚本初始态。实跑症状：**进第二面后 releaseable 的 UI 消失**
     * （条 script127 的初始态是 `interruptLabel(3)` 里的 alpha 0，靠 interrupt 2 才淡入）。*/
    int fi, frame_missing = 0;
    for (fi = 0; fi < SEASON_FRAME_N; ++fi)
        if (!s->hud_frame[fi] || !ce_anm_get_vm((uint32_t)s->hud_frame[fi])) frame_missing = 1;
    if (frame_missing ||
        (!s->hud_solid || !ce_anm_get_vm((uint32_t)s->hud_solid)) ||
        (!s->hud_grad  || !ce_anm_get_vm((uint32_t)s->hud_grad))  ||
        (!s->hud_glyph || !ce_anm_get_vm((uint32_t)s->hud_glyph)) ||
        (!s->hud_strip || !ce_anm_get_vm((uint32_t)s->hud_strip)))
        s->hud_last_state = -1;
    /* 外框：四条边各一个 1×1 矩形 VM（脚本 123），尺寸用 scale 给 */
    for (fi = 0; fi < SEASON_FRAME_N; ++fi) {
        if (!s->hud_frame[fi] || !ce_anm_get_vm((uint32_t)s->hud_frame[fi])) {
            s->hud_frame[fi] = (int32_t)ce_anm_spawn_ui(CE_ABILITY_ANM(), CE_ANM_ABILITY_SCRIPT_SEASON_GAUGE_FRAME, 0);
            if (s->hud_frame[fi])
                ce_anm_set_scale((uint32_t)s->hud_frame[fi], k_season_frame[fi].sx, k_season_frame[fi].sy);
        }
    }
    /* ★ 生成顺序照 TH16 `front.anm` script113 的 `scriptNew` 次序：
     *   script114(外框) → **script116(实心背景)** → **script115(渐变进度)** → 117/118。
     *   后生成的画在上面 ⇒ **渐变进度条盖在实心背景条之上**（进度可见）。*/
    if (!s->hud_solid || !ce_anm_get_vm((uint32_t)s->hud_solid))
        s->hud_solid = (int32_t)ce_anm_spawn_ui(CE_ABILITY_ANM(), CE_ANM_ABILITY_SCRIPT_SEASON_GAUGE_SOLID, 0);
    if (!s->hud_grad || !ce_anm_get_vm((uint32_t)s->hud_grad))
        s->hud_grad = (int32_t)ce_anm_spawn_ui(CE_ABILITY_ANM(), CE_ANM_ABILITY_SCRIPT_SEASON_GAUGE_GRAD, 0);
    if (!s->hud_glyph || !ce_anm_get_vm((uint32_t)s->hud_glyph))
        s->hud_glyph = (int32_t)ce_anm_spawn_ui(CE_ABILITY_ANM(), CE_ANM_ABILITY_SCRIPT_SEASON_GAUGE_GLYPH, 0);
    if (!s->hud_strip || !ce_anm_get_vm((uint32_t)s->hud_strip))
        s->hud_strip = (int32_t)ce_anm_spawn_ui(CE_ABILITY_ANM(), CE_ANM_ABILITY_SCRIPT_SEASON_GAUGE_STRIP, 0);
    /* 位置：**表面 px**（UI 列表 VM 的坐标空间 = 相机 2，原点在表面左上角）。
     * ★★ 2026-09-12：全部由**槽锚点 + 块内偏移**推导（偏移见 `SEASON_HUD_*_DX/DY` 那组宏）——
     *   锚点落在货架第一格 (257,924) 时逐像素等于迁移前的 `SEASON_GAUGE_BX/BY` 写法。
     * 相对关系照 TH16 脚本 + 用户逐条微调（第二十二/二十五/三十二轮，单位就是表面 px）：
     *   进度条整体 **+8 x**（外框/进度条/背景条一起走）、季节字 **+7 x**、
     *   releaseable 条 **+37 x / +8 y**；数字单独在 `ce_hud_vm_text_set` 那处改。
     * ★ 第三十二轮：外框 = **四条矩形**（脚本 123 = 1×1 模板 + 逐边 scale，见 `k_season_frame[]`），
     * 不再是「实心矩形打底」。上/左各 2px、下/右各 1px ⇒ 外沿像素 x 263..365 / y 910..920。*/
    for (fi = 0; fi < SEASON_FRAME_N; ++fi) {
        if (!s->hud_frame[fi]) continue;
        ce_anm_set_pos((uint32_t)s->hud_frame[fi],
                       (s_slot.x + 51.0f) + k_season_frame[fi].dx,      /* 51 = 308−257，外框中心 */
                       (s_slot.y - 8.0f) + k_season_frame[fi].dy,       /* −8 = 916−924 */
                       0.0f);
    }
    if (s->hud_grad)  { pos[0] = s_slot.x + SEASON_HUD_BAR_DX;   pos[1] = s_slot.y + SEASON_HUD_BAR_DY;
                        ce_anm_set_pos((uint32_t)s->hud_grad, pos[0], pos[1], 0.0f); }
    if (s->hud_solid) { pos[0] = s_slot.x + SEASON_HUD_BAR_DX;   pos[1] = s_slot.y + SEASON_HUD_BAR_DY;
                        ce_anm_set_pos((uint32_t)s->hud_solid, pos[0], pos[1], 0.0f); }
    if (s->hud_glyph) { pos[0] = s_slot.x + SEASON_HUD_GLYPH_DX; pos[1] = s_slot.y + SEASON_HUD_GLYPH_DY;
                        ce_anm_set_pos((uint32_t)s->hud_glyph, pos[0], pos[1], 0.0f); }
    if (s->hud_strip) { pos[0] = s_slot.x + SEASON_HUD_STRIP_DX; pos[1] = s_slot.y + SEASON_HUD_STRIP_DY;
                        ce_anm_set_pos((uint32_t)s->hud_strip, pos[0], pos[1], 0.0f); }

    /* ★★★ 2026-10-03z6（owner 实跑：「**季节指示数字又和季节槽错位了**」）——
     *   **几何自述**：把「槽 → 框心 → 字心 → 数字（640 空间换算回表面）」一次打全，
     *   下次实跑就能直接读出**数字与框差多少 px**（本卡的元素各用一套偏移常量，
     *   错位只可能是常量之间的基准不一致 ⇒ 这条日志是判据）。
     *   节流：槽位或档位变化时各打一行（不刷屏）。*/
    {
        static float s_log_x = -1.0f, s_log_y = -1.0f;
        static int32_t s_log_level = -1;
        if (s_log_x != s_slot.x || s_log_y != s_slot.y || s_log_level != s_geom_last_level) {
            const float frame_cx = s_slot.x + 51.0f;                    /* 外框中心（表面）*/
            const float glyph_sx = s_slot.x + SEASON_HUD_GLYPH_DX;      /* 季节字心（表面）*/
            const float digit_sx = (s_slot.x640 + SEASON_HUD_DIGIT_X640) * 2.0f;   /* 数字（640→表面）*/
            const float frame_cy = s_slot.y - 8.0f;                     /* ★ z14：外框心 y（表面）*/
            const float digit_sy = (s_slot.y640 + SEASON_HUD_DIGIT_Y640) * 2.0f;   /* ★ z14：数字 y */

            s_log_x = s_slot.x; s_log_y = s_slot.y; s_log_level = s_geom_last_level;
            ce_log("season: hud geom slot (%.0f,%.0f) ok=%d | frame (±%.1f,±%.1f) glyph_sx %.1f "
                   "digit (%.1f,%.1f) | dx %.1f dy %.1f px | pts %d",
                   (double)s_slot.x, (double)s_slot.y, (int)s_slot_ok,
                   (double)frame_cx, (double)frame_cy, (double)glyph_sx,
                   (double)digit_sx, (double)digit_sy,
                   (double)(digit_sx - frame_cx), (double)(digit_sy - frame_cy),
                   (int)s_geom_last_level);
        }
    }
}

static void hud_hide(season_card_t *s)
{
    int fi;
    for (fi = 0; fi < SEASON_FRAME_N; ++fi) {
        if (s->hud_frame[fi]) { ce_anm_delete((uint32_t)s->hud_frame[fi]); s->hud_frame[fi] = 0; }
    }
    if (s->hud_grad)  { ce_anm_delete((uint32_t)s->hud_grad);  s->hud_grad = 0; }
    if (s->hud_solid) { ce_anm_delete((uint32_t)s->hud_solid); s->hud_solid = 0; }
    if (s->hud_glyph) { ce_anm_delete((uint32_t)s->hud_glyph); s->hud_glyph = 0; }
    if (s->hud_strip) { ce_anm_delete((uint32_t)s->hud_strip); s->hud_strip = 0; }
    ce_hud_vm_text_clear(&s_hud_digit);          /* ★ 档位数字（贴图 VM）跟着槽一起收 */
}

static int on_draw(ce_card_t *c)
{
    season_card_t *s = st(c);
    int level, next;
    float ratio;
    int fi;

    if (!s) return 0;
    /* ★ 2026-09-12：`on_draw`（虚表 +0x48）在**主菜单 / 标题 / 商店**里也会被调，而 `on_tick_2`（+0x2c）
     * 不会 ⇒ 不设门会在菜单上留着一整块季节槽。两道门照 111 `signal.c:556` / 115 `lolk.c:706` 的先例：
     *   ① 没有游戏线程 = 标题/主菜单退出路径；② 没有自机 = 菜单/商店/结算里 `CE_PLAYER()` 为 NULL。
     * 门关着时把 5 个 VM + 数字收掉，**槽位的账不动**（那归 `on_load` / `on_delete`，见 `hud_slot_claim`）。*/
    if (!CE_GAME_THREAD() || !CE_PLAYER()) { hud_hide(s); return 0; }

    hud_slot_sync(s);                    /* ★ 每帧取最新锚点（同批后来的认领会按 id 重排）*/

    /* ★★ 2026-09-15g：压暗 = 「自机的体量碰到本卡 HUD 块」——**每帧现算块矩形**（锚点会重排）。
     * 矩形/门限的推法与依据见文件上部那段常量注（块 104×36 @ s_slot ⇒ 矩形 x∈[−191.5,−139.5] ∧ y∈[428,446]；
     * **进 = 矩形 + 28**（外扩）⇒ x∈[−219.5,−111.5] ∧ y∈[400,474]、**出 = 矩形 + 44**）。
     * 每帧只调一次（这里已经过了主菜单/自机两道门）。*/
    {
        hud_dim_rect_t r;
        float bx, by, bw, bh;                                  /* ★ 2026-10-03q：**一块 HUD** —— 喂「带」的矩形 */
        ce_hud_band_rect(&bx, &by, &bw, &bh);
        if (bw <= 0.0f || bh <= 0.0f) {                        /* ★ z7（审计 P3-3）：空带兜底回本块 */
            bx = s_slot.x; by = s_slot.y; bw = SEASON_HUD_BLOCK_W; bh = SEASON_HUD_BLOCK_H;
        }
        hud_dim_rect_surface(&r, bx, by, bw, bh);
        (void)hud_dim_tick(&s->hud, &r, ce_player_pos());
    }

    level = level_of(s);
    next = (int)season_points_for_level(level + 1);
    /* 填充比 = 当档内的进度（照 TH16 `get_season_gauge_fill_ratio` 的口径）*/
    {
        int32_t base = season_points_for_level(level);
        int32_t span = next - base;
        ratio = (level >= SEASON_LEVEL_MAX) ? 1.0f
              : (span > 0 ? (float)(s->points - base) / (float)span : 0.0f);
        if (ratio < 0.0f) ratio = 0.0f;
        if (ratio > 1.0f) ratio = 1.0f;
    }

    hud_ensure(s);

    /* 状态 → interrupt 组合（TH16 `season_gauge_render_42c600` `0x42c600` 一手口径）：
     *   **渐变条（script115 = 进度）**：档 ≥1 → 2（黄→白）；档 0 → 3（蓝）
     *   **实心条（script116 = 背景）**：档 ≥1 → **档 + 7**（8..13 逐档变亮）；档 0 不发（条透明）
     *   releaseable 条（script118）：同上 2 / 3
     *   淡出闩锁：4 = 亮、5 = 暗（TH16 `Gui::on_tick_20` 的 `interrupt_tree(5/4)`）
     * ★ 状态色与淡出淡回**分开**发 —— 一个 VM 只有一份 `pending_interrupt`，
     *   混在同一帧发会让 4/5 把 2/3/8..13 顶掉。*/
    {
        int lvl_up = (level >= 1);                          /* 档 ≥1 = 进度黄白 + 背景上色 */
        int can_release = (lvl_up && s->rel_frames <= 0);   /* 可解放指示（script118）*/
        int st = (lvl_up ? 2 : 3) | ((lvl_up ? level + 7 : 3) << 4)
               | ((s->hud.dim ? 5 : 4) << 8) | (can_release << 12);
        if (st != s->hud_last_state) {
            int diff = st ^ s->hud_last_state;
            s->hud_last_state = st;
            if (diff & 0x0f) {                      /* 进度条状态色 */
                if (s->hud_grad) ce_anm_interrupt((uint32_t)s->hud_grad, lvl_up ? 2 : 3);
            }
            if (diff & 0xf0) {                      /* 背景条档位色 */
                if (s->hud_solid) ce_anm_interrupt((uint32_t)s->hud_solid, lvl_up ? level + 7 : 3);
            }
            if (diff & 0x1000) {                    /* 可解放指示条 */
                if (s->hud_strip) ce_anm_interrupt((uint32_t)s->hud_strip, can_release ? 2 : 3);
            }
            if (diff & 0xf00) {                     /* 淡出 / 淡回（TH16 是 interrupt_tree 整棵树）*/
                int dim = s->hud.dim ? 5 : 4;      /* 闩锁读共享件（`hud_dim.h`）*/
                for (fi = 0; fi < SEASON_FRAME_N; ++fi)
                    if (s->hud_frame[fi]) ce_anm_interrupt((uint32_t)s->hud_frame[fi], dim);
                if (s->hud_glyph) ce_anm_interrupt((uint32_t)s->hud_glyph, dim);
                if (s->hud_grad)  ce_anm_interrupt((uint32_t)s->hud_grad,  dim);
                if (s->hud_solid) ce_anm_interrupt((uint32_t)s->hud_solid, dim);
                /* ★★★ 2026-10-03z34（owner：「对于 **0 级的 th16 HUD，不应当出现那个 releaseable**，
                 *   可能是 HUD 统一之后的 opacity 刷写问题？」—— **判断正确** ✓）：
                 *   script127（releaseable 条）的 label 表：**3 = `alpha(0)`（隐藏）**、
                 *   **2 = `alphaTime(4,0,255)`（显示）**、**4 = `alphaTime(8,0,255)`（淡到满亮）**、
                 *   5 = `alphaTime(8,0,64)`（压暗）。而我们的**压暗广播**（TH16 一手是
                 *   `interrupt_tree(5/4)` 整棵树）**把它也算进去** ⇒ 0 级时刚发的"隐藏(3)"
                 *   立刻被这条广播用"亮(4)"**点亮** ✗ ⇒ owner 看到 0 级也有 releaseable 条。
                 *   ⇒ 只在**它本身该显示**时（`can_release`）才给这条发 4/5 ✓：
                 *     · 0 级 / 不可解放 ⇒ 一个 interrupt 都不发 ⇒ 停在 label 3 的 alpha 0 ✓
                 *     · 可解放 ⇒ 照旧跟着"带"一起淡（与原生一致 ✓）。*/
                if (can_release && s->hud_strip)
                    ce_anm_interrupt((uint32_t)s->hud_strip, dim);
            }
        }
    }

    if (s->hud_glyph) {
        ce_anm_set_sprite(CE_ABILITY_ANM(), (uint32_t)s->hud_glyph, season_glyph_sprite(s->subseason));
    }
    /* ★ 颜色由卡每帧写（TH16 是脚本 interrupt 2/3 + 8..13 上色，但 TH18 的渐变图元只认
     *   color_1 + color_2 两个色，靠脚本设不稳）——用户 2026-09-10 口径：
     *   **会动的进度矩形**：档 ≥1 → 黄(255,255,0) 白(255,255,255) 相间；**唯一例外** = 档 0 → 蓝(64,64,255)；
     *   **不会动的背景矩形**：蓝随档位变白（TH16 interrupt 8..13：64/96/128/144/192/224 的 (v,v,255)）；
     *   档 0 → **透明**（TH16 那一段连档位中断都不发）。*/
    {
        /* ★ 2026-09-15g：alpha 取**跨卡统一件**那一份（★ 2026-09-22 起 255 ↔ **64**、**8 帧线性** = 原生一手；
         * 旧口径 32 / ±56（4 帧）；更早这里写 64）。
         * 这四支（外框 ×4 / 进度 / 背景）的 alpha **由卡每帧写**（见 123 脚本头：「脚本里的 alphaTime
         * 只是初始态」）⇒ 用户看到的压暗深度就是这里这个值。*/
        uint32_t A = (uint32_t)(s->hud.alpha & 0xff) << 24;
        int lvl_up = (level >= 1);
        uint32_t c1 = lvl_up ? 0x00ffff00u : 0x004040ffu;
        uint32_t c2 = lvl_up ? 0x00ffffffu : 0x004040ffu;
        static const uint32_t lvl_c[7] = { 0x004040ffu, 0x004040ffu, 0x006060ffu, 0x008080ffu,
                                           0x009090ffu, 0x00c0c0ffu, 0x00e0e0ffu };
        if (level < 0) level = 0;
        if (level > SEASON_LEVEL_MAX) level = SEASON_LEVEL_MAX;
        if (s->hud_grad)  { ce_anm_set_color((uint32_t)s->hud_grad,  A | c1);
                            ce_anm_set_color2((uint32_t)s->hud_grad, A | c2); }
        if (s->hud_solid) ce_anm_set_color((uint32_t)s->hud_solid, (lvl_up ? A : 0u) | lvl_c[level]);
        for (fi = 0; fi < SEASON_FRAME_N; ++fi)
            if (s->hud_frame[fi]) ce_anm_set_color((uint32_t)s->hud_frame[fi], A | 0x00505050u);
        /* ★★ 2026-09-15i：**C 侧那层 `min(脚本 α, 32)` 兜底已撤**（脚本已统一，见下）。
         *   · 2026-09-15g 那一轮这里做过一段「上限钳制」：只对**由脚本管 alpha 的两支**
         *     （季节字 `hud_glyph` = 脚本 126 / 可解放指示条 `hud_strip` = 脚本 127）写
         *     `min(脚本当前 alpha, 统一 alpha)`，用来把脚本里的 **64** 压到 **32**；
         *     理由当时成立：脚本是**一手值**（TH16 `alphaTime(5,0,64)`），而 assets 归另一会话。
         *   · 2026-09-15i 起脚本侧已经**统一**（`AUDIT.md` §DD-9 / `TUNING.md` §7.1）：
         *     两支的 `interruptLabel(5): alphaTime(8, 0, 64)`（压暗）、
         *     `interruptLabel(4): alphaTime(8, 0, 255)`（淡回）⇒ 脚本给的就是统一值，
         *     这一层再留着就是**同一件事的两套实现**（父 agent 明令撤掉）⇒ 代码删除，只留本注。
         *   · **不变式（这就是那段代码的替身）**：126/127 的 label 4/5 `alphaTime` 必须保持
         *     `(8,0,255)` / `(8,0,64)`（★ 2026-09-22 换成原生一手；旧值 `(4,0,·)`/32、(20,0,·)/128 已退役）；
         *     `interruptLabel(2/3)` 那两条（淡入 4 帧 / 隐藏 0）
         *     是**另一条路**、不许被这次统一波及。谁把脚本改回 64，症状 = 这两支只淡到 64、
         *     而外框/进度/背景（下面那段 C 每帧写 `A`）淡到 64 ⇒ 一块 HUD 两种深度。
         *     ⇒ `native/tools/anm_hud_alpha_check.py`（`make check` 里）把这条不变式做成机器判据。
         *   · 这两支的 alpha **仍然只由脚本管**（C 不写它们的 color）—— 这是刻意的：
         *     脚本的 `alpha(0)` / `alphaTime` 负责「不可解放时条不该在场」的隐藏态。*/
    }
    /* ★ 2026-09-10 定案（用户口径 + TH16 `0x42c600` 一手复核）：
     *   **渐变条（drawRectGrad）= 进度**：TH16 每帧把 `get_season_gauge_fill_ratio() × 100`
     *     写进 **script115 那个 VM** 的矩形宽度（`+0x70`；满槽写常量 `0x42c80000` = 100.0），
     *     它正是 `search_children(gauge, 0x73)` 找到的渐变条 ⇒ **会动的条是渐变条**；
     *   **实心条（drawRect）= 背景**：全长 100，颜色随档位（script116 的 interrupt 8..13），档 0 透明；
     *   绘制顺序照 TH16 script113 的 `scriptNew(114→116→115)`：**进度画在背景之上**。*/
    if (s->hud_grad) {
        ce_anm_set_scale((uint32_t)s->hud_grad, ratio, 1.0f);       /* 进度：宽 = 档内进度 */
        /* 条宽 100 表面 px ⇒ 左端固定、宽度按 ratio 收缩（中心随之左移 (1−ratio)×50）*/
        ce_anm_set_pos((uint32_t)s->hud_grad,
                       s_slot.x + SEASON_HUD_BAR_DX - (1.0f - ratio) * 50.0f,
                       s_slot.y + SEASON_HUD_BAR_DY, 0.0f);
    }
    if (s->hud_solid) {
        ce_anm_set_scale((uint32_t)s->hud_solid, 1.0f, 1.0f);       /* 背景：全长 100 */
        ce_anm_set_pos((uint32_t)s->hud_solid,
                       s_slot.x + SEASON_HUD_BAR_DX, s_slot.y + SEASON_HUD_BAR_DY, 0.0f);
    }

    /* ★★ 等级数字定案（2026-09-10 一手：`th16:0x428e70` `Gui::on_draw_2` 尾段）：
     * TH16 的档位数字**不是贴图**，是 **ASCII 文字** ——
     *   `group = 1; font_id = 2; alignment_mode_h = 0; alignment_mode_v = 2;`
     * ⚠️ **group 不能照抄 1**：TH18 的字符串是「当帧画、下一帧 tick 掉」
     *   （`AsciiManager__on_tick` `0x4198b0` 每帧把记录 `+0x12c` 减 1、≤0 就压缩掉，
     *   环境 duration 默认 0），而 group 1 的绘制趟（`AsciiManager__on_draw_37` → `draw_group(1)`）
     *   跑在卡的 `on_draw` **之前** ⇒ 我们的串当帧没人画、下帧被清 = **数字消失**（实跑踩过）。
     *   group 0 的趟（`on_draw_58`）在卡之后 ⇒ 用 0。
     *   颜色取 7 档表 `local_60[]`（0x60606060 / 0xa0b0b080 / 0xb0b8b880 / 0xc0c0c080 /
     *   0xd0d0d080 / 0xe0e0e080 / 0xffffff30，D3DCOLOR = 0xAARRGGBB），
     *   玩家贴近左下角时把 alpha 字节压到 **0x40**（= 64，一手值；2026-09-15g 起按用户口径统一，
     *   ★ 2026-09-21 起统一值 = **128**）。
     *   数字本身走 `FUN_004082b0` = `create_string("%d")`。
     * ⇒ 照抄：字体 2 + group 1 + 对齐 (0,2) + 该档位色（缩放不动 = 环境 1.0）。*/
    {
        static const uint32_t lvl_digit_c[7] = {
            0x60606060u, 0xa0b0b080u, 0xb0b8b880u, 0xc0c0c080u,
            0xd0d0d080u, 0xe0e0e080u, 0xffffff30u,
        };
        uint32_t col = lvl_digit_c[level];
        /* ★★ 2026-09-15g（跨卡统一）：原来是「压暗时**硬切**到 0x40」，现在改成
         *   **`min(档位色自带的 alpha, 统一件 alpha)`** —— 理由两条：
         *   ① 深度按统一件走（`HUD_DIM_ALPHA_DIM`；★ 2026-09-22 起 = **64** = 原生一手），不再各写各的 0x40；
         *   ② 让数字跟**同一块**的外框/进度/背景**同步渐变**（原来条在 255↔64 逐帧走、数字一帧跳过去）。
         *   亮着时是恒等（统一 alpha = 255 ⇒ min 取档位色自带的那一档），所以「档位越高的数字越实」
         *   这条一手行为一个字节没变。*/
        uint32_t lvl_a = col >> 24;
        uint32_t alpha = lvl_a < (uint32_t)s->hud.alpha ? lvl_a : (uint32_t)s->hud.alpha;
        /* ★★ 2026-09-10 第三十二轮：**从 ASCII 文字改成贴图 VM**（用户定案「那就改：只能把数字改成贴图 VM」）。
         * 为什么必须换：ASCII 文字**永远进不了暂停模糊罩下面**（用户实跑：改成早于罩子的 group 1
         * 之后数字全看不见 —— 那一趟 `0x419950` 跑在 surface 1 + 相机 0 上，见下面 31 轮的记录）。
         * 现在走 `ce_hud_vm_text_set`（sdk.c）：字形 = TH18 font 2 的**原生切图**（`ascii1280` 变体，
         * `make_font2_art.py`），VM 挂脚本 136（UI 层 25 → 折到组 38 → 与模糊罩同桩 `0x3e`，
         * 罩子暂停时后起 ⇒ 数字被罩住、平时全分辨率）。
         * 排版与位置**与原来的 ASCII 完全一致**（同一套 640 空间坐标 + font 2 的字距/对齐）：
         *   x = 320 + BX + 28 = 182（外框右端再往右 2 ECL）
         *   y = 16 + BY + 6 = 464 → 再上移一点到条的垂直中线（455）
         * 颜色仍取 7 档表 `local_60[]`（`th16:0x428e70` 尾段，D3DCOLOR = 0xAARRGGBB），
         * 玩家贴近左下角时 alpha 改 0x40。
         * —— 下面这段是 31 轮的 ASCII 组号考古，留着当「为什么不能走 ASCII」的凭据：
         *   ⚠️ group 1 = `on_draw_37` `0x419950`（**先切到相机 0**，且跑在 surface 1 上 ⇒ 看不见）
         *   group 3 = `on_draw_3f` `0x419b10`（切相机 2，全表面）；group 0 = `on_draw_58` `0x419940`
         *   （优先级 0x58 = 最后 ⇒ 盖在立绘之上）；立绘在 UI 组 44（0x57），卡的 `on_draw` 在 0x33。*/
        {
            char buf[8];
            snprintf(buf, sizeof buf, "%d", level);
            /* ★ 2026-09-10 第三十二轮：用户截图量的「最终渲染还要挪多少」= **左 172 / 上 6**（表面 px）
             * ⇒ ASCII 空间（÷DAT）−86 / −3：182−86 = 96、455−3 = 452。
             * ★ 第三十二轮追加亚像素：再 **左 0.5 / 上 0.5**（表面 px）⇒ ASCII 各 −0.25 ⇒ (95.75, 451.75)。
             * ⚠️ 目标落点 = 表面 x≈185（槽在 257..359）—— 数字会跑到**游玩区左边界 256 之外的黑边里**，
             * 与槽分开；若这不是你要的（比如想让它贴条左端 / 或量的是别的元素），说一声再挪。
             * ★★ 2026-09-12 迁移：这两个数是**绝对**的 640 空间坐标，现在改成「槽锚点（640 空间）
             *   + 块内偏移」：锚点 (257,924) ⇒ (128.5, 462)，于是
             *   95.75 − 128.5 = −32.75、451.75 − 462 = −10.25（= `SEASON_HUD_DIGIT_X640/Y640`）。*/
            ce_hud_vm_text_set(&s_hud_digit,
                               s_slot.x640 + SEASON_HUD_DIGIT_X640,
                               s_slot.y640 + SEASON_HUD_DIGIT_Y640,
                               (alpha << 24) | (col & 0xffffffu),
                               CE_ASCII_ALIGN_CENTER, 0.0f /* 缩放用 ASCII 环境值 */,
                               buf, "season");
        }
    }
    return 0;
}

static void clear_card(ce_card_t *c)
{
    season_card_t *s = st(c);
    int i;
    if (!s) return;
    for (i = 0; i < SEASON_OPTS; ++i) if (s->opt[i].ptr) ce_option_drop(c, &s->opt[i]);
    point_clear_all(s);
    ring_hide(s, 0);
    hud_hide(s);
    ce_hud_vm_text_clear(&s_hud_digit);        /* 数字 VM 跟着卡一起收 */
    /* ★ 2026-09-12：HUD **槽位**也在这里退（`CE_CARD` 的 `on_run_reset`/`opdelete` 会先调
     * `ce_layer_drop_card()` → `ce_hud_slot_release()`，这里显式再来一次是幂等的；
     * 好处是 `on_load` 之后即使没删卡、状态被 `memset` 掉，下一个 `on_load` 也会重认领）。*/
    ce_hud_slot_release(116);
    s_slot_ok = 0;
    memset(s, 0, sizeof *s);
    s->last_graze = -1;
    s->hud_last_state = -1;
    hud_dim_init(&s->hud);                     /* ★ 2026-09-15g：压暗状态复位 = **满亮 255**（不是 0）*/
    season_dtap_init(&s->dtap);                /* ★ 2026-09-29：双击状态机清零（memset 已覆盖，显式再写一次）*/
}

/* ★ 2026-09-10 第二十六轮（用户实跑：切关后面板/子机被打回春）：
 * `on_load`（+0x20）**不是**「开局一次」——它的唯一调用点 `AbilityManager__notify_cards_on_load`
 * 由 `AbilityShop__sub_417880` 触发，而那个函数在**每关结束的商店/结算**里都会跑
 * （`AbilityShop::on_tick`、`GameThread__teardown_and_recount_cards`）。
 * 原来在这里 `clear_card` ⇒ 每过一关 `memset` 把 `subseason` 清零（= 春）、季节点也清零。
 * 现在 `on_load` 只做**认领 HUD 槽**这一件事（状态只在 **局末（+0x4c on_run_reset）** 与
 * **销毁（+0x50 on_delete）** 清）；新一局是**新卡对象**，`ce_state_user` 的槽是 `memset` 过的
 * ⇒ 天然从 0 开始。
 * ★ 2026-09-12：认领**必须**放在这里 —— 槽位带「关卡纪元」（`ce_hud_slot_claim_card` 把
 * `CE_CURRENT_STAGE()` 推进去），关卡号一变 SDK 就清空整张表 ⇒ 不重认领的话本关就没有槽，
 * `hud_slot_sync` 只能退回默认锚点（虽然像素相同，但账就不对了）。*/
static int on_load(ce_card_t *c)
{
    season_card_t *s = st(c);

    /* ⚠ 这里**不**再把 `s_slot_ok` 清 0：它是「上一次认领结果」，留着才能让
     * `hud_slot_claim()` 只在**结果变化**时打日志（否则每关都会再打一行）。*/
    hud_slot_claim(s);
    return 0;
}
static void on_run_reset(ce_card_t *c) { clear_card(c); }
static void on_delete(ce_card_t *c) { clear_card(c); }

CE_CARD(116, .active_recharge = 1,          /* ★ 主动卡（D 切换 + C 发动；代价 = 季节点档位）*/
         .on_activate = on_activate, .on_active_tick = on_active_tick,
         .on_power_level_change = on_power_level_change, .on_tick_shooters = on_tick_shooters,
         .on_tick_2 = on_tick_2, .on_enemy_drop_pre = on_enemy_drop_pre, .on_draw = on_draw,
         .on_load = on_load, .on_run_reset = on_run_reset, .on_delete = on_delete);
