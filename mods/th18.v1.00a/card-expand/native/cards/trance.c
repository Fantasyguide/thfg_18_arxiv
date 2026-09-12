/* 神灵庙 / 灵界（card-expand id **113**，内部 TRANCE）—— **0x54 纯被动卡**：灵界槽 + 灵界窗口 +
 * 死亡后的强制灵界 + 神灵奖励。
 *
 * ## 一句话（用户定案，SPEC §0）
 *
 * **收神灵充灵界槽；满槽的灵界里中弹无伤、收神灵奖励翻倍；死亡时（死亡已确定）先强制开灵界，
 * 然后照常 biu。**
 *
 * ## 分工：本文件只做「接线」，全部判断在 `cards/trance_core.c`
 *
 *   | 件 | 在哪 | 管什么 |
 *   | --- | --- | --- |
 *   | 灵界槽 / 窗口状态机 / 奖励表 / 连锁 / 救援层记账 | `trance_core.c` | 纯逻辑、无引擎地址、有主机单测 |
 *   | **本文件** | — | 读引擎面 → 调 core → 落引擎副作用（无敌声明 / 音效 / 分数声明 / HUD）|
 *
 * 设计稿：`docs/superpowers/specs/2026-09-12-ten-desires-113-design.md`。
 * ★ **实装期的三条硬结论**（本文件存在的原因，逐条写清出处）：
 *
 *   1. **挂 `+0x14`（`on_death_frame2`），绝不挂 `+0x10`**（SPEC 用户定案 ③）。
 *      `+0x10` = `on_death_before_deathbomb` = **救援层**（调 `cancel_impending_death` `0x45d590`
 *      取消死亡，117 先例 `cards/animal.c`）；`+0x14` = 死亡结算**之后**（扣钱扣火力已发生，
 *      `SDK.md:71`）⇒ 113 的「死后照死、只是先开灵界」只可能落在这里。
 *      ⚠ 113 里**不许**出现 `cancel_impending_death`（SPEC §7 第一条：分层不许越界）。
 *   2. **`+0x14` 拿不到中弹点**：现场是 respawn 之后、`ce_player_pos()` 已经是复活点
 *      （`cards/animal.c:1777` 的一手注释）⇒ 本文件**一次都不读自机坐标**（连 `ce_player_pos()` 都不调）。
 *   3. **「中弹无伤」不写 `player+0x47774`**：那是 TH18 的**复活无敌** zTimer（`engine.h:277`），
 *      写它既不等于灵界、还会把 107/117 给的长无敌截短（SPEC §2.1 行 2 · `EC` §4.1）。
 *      本卡要无敌帧时**只声明** `ce_invuln_request(113, n)`（层取 max、SDK 统一落笔）。
 *
 * ## ★ 槽怎么被填（**2026-09-12 接通的那条链**，每步给代码位置）
 *
 * 修复前：`trance_collect()` 的**唯一**调用者 `trance_on_spirit_collected()` 没有人调
 * （挂 `__attribute__((unused))`）⇒ 灵界槽恒 0 ⇒ 灵界永不开启、`+0x14` 恒 SKIPPED，只剩 HUD 的「0/600」。现在：
 *
 *   来源 ─► 入槽 ─► 满槽/条件 ─► 开灵界 ─► 窗口 ─► 收尾
 *
 *   ① **来源 = 击破**（SPEC §3.3 候选 ①，`DEATH:263` 预定的那一条）：
 *      `on_enemy_death()`（本文件尾；事件钩子，非虚表槽）过滤 `kind == CE_EDEATH_KILLED`
 *      且 `flags & CE_EDEATH_F_REAL` ⇒ `trance_kill_note()` **只做 O(1) 入队**（`DEATH` §2.4）。
 *   ② **撒实体**（帧末 `on_item_frame` ⑤）：`trance_kill_take()` 取走 → 每颗按
 *      `TRANCE_KILL_SPIRIT_TYPE`（= **1**，SPEC §2.2：击破走通道 B）与
 *      `TRANCE_KILL_SPIRITS_PER_KILL`（❓ 自定 = 1）调 `trance_spirit_spawn()` **建一颗神灵 VM**
 *      （自建池，见下「神灵实体」段）——★ **不再**在帧末按「已收」入账。
 *   ③ **靠近才吃**（`on_item_frame` ⑥ = `trance_spirits_tick()`）：自机走到 ≤
 *      `TRANCE_SPIRIT_PICK_RADIUS`（❓ 自定 24 px）才调 **`trance_on_spirit_collected()`**
 *      → `trance_collect()`（core）→ `trance_gauge_add(+2)`。**每颗神灵 +2 槽**（一手 `th13:0x438b3d`）。
 *   ④ **满槽 + 8 条条件**：`trance_fill_cond()` 把 core 的条件表喂满（含第 8 条 `(GAME_FLAGS & 0x30) != 0x20`
 *      → `trance_game_flags_ok()`，TH18 全局 `0x4cccc8`，站址见该函数注释）⇒ `trance_can_activate()`。
 *   ⑤ **开灵界**：`trance_begin()`（一手两道门：已在灵界幂等 / 槽 < 200 失败）—— `on_item_frame` ②。
 *   ⑥ **窗口**：`trance_tick()` 每帧 −1，`on_death_frame2()`（虚表 `+0x14`）可强制开一遍（相位 2）。
 *   ⑦ **收尾**：归零 ⇒ `trance_end()`（非死亡结束 −200）；删卡 ⇒ `trance_clear()`（`+0x50`，含清池）。
 *
 *   ⇒ 速率账（一手）：1 颗 = +2 槽，满槽 600 ⇒ **300 颗**；满槽窗口 = 600 帧 = 10.00 s。
 *     击破一颗只算 1 颗（❓ 自定，见 §9 #4）⇒ 本卡一关的强弱直接由击破数决定，实跑首日先看汇总日志。
 *
 * ## 长持续效果的自查（`METHOD.md` 铁律二 / SPEC §3.5）
 *
 * 本卡跨帧留下的东西：① 灵界槽与窗口相位（卡私有状态 `ce_state`）；② 每帧的**无敌声明**；
 * ③ HUD 文字 VM；④ 击破待撒队列；⑤ ★ **神灵实体池**（自建 VM 池，文件级 static，见下）。
 * ⇒ `on_delete`（虚表 `+0x50`）与 `on_run_reset` **都走同一个 `trance_clear()`**：
 * **先退声明层**（`ce_invuln_request(113, 0)`）、**再收 HUD**、**再删光池里所有神灵 VM**、
 * 最后 `memset` 私有状态。删卡发生在**商店**里、那一刻 AbilityManager 冻住、tick 全停
 * ⇒ 只能靠 `on_delete`，等下一帧等于没有清理（`SDK.md:71` 那条）。
 *
 * ## ⛔ 计分声明：**已停用**（`SPEC §9 #5` 未拍 ⇒ 未定项不许悄悄生效）
 *
 * 113 原来把 type-1 的那份「最大得点 +100 显示单位」换算成 ×100 定点写进
 * `ce_score_declare(113, …)`（= **10000**）。但 `ce_score_aggregate()` 是**所有卡之和**，
 * 而 110 / 111 的落笔者把它当**信仰总量 / 被乘数**用 ⇒ 113 一在场就给 110/111 的 PV
 * **凭空加 10000**。裁决前**先停**：`trance_declare_score()` 现在只记本地账
 * （`s->decl_last` / `s->decl_blocked`）+ 打日志，**一个字节都不往计分层写**；
 * `trance_clear()` 里那行 `ce_score_declare(113, 0)` 也一并删了（本卡不进这层，就没得退）。
 * 拍板后怎么接（三条路径）见文件里「计分层」段与 `trance_declare_score()` 的注释。
 *
 * ## ★ 击破 → 神灵：**已接线**（事件钩子，不是虚表槽）
 *
 * `on_enemy_death` 是 `ce_hooks_t` 的**事件钩子**（`sdk/public.h`），由 `sdk.c` 的
 * `BP_ce_enemy_death`（断点 `0x430240`）沿卡链表直接 `h->on_enemy_death(card, &ev)` 派发
 * —— **不经过 21 槽虚表**，所以 `CE_CARD` 里没有它的桩是**正常的**（与 `on_enemy_drop_pre` /
 * `on_item_score` / `on_bomb_gate` 同一类）。本文件因此直接 `.on_enemy_death = on_enemy_death`。
 * （SDK 侧两处真问题已于 2026-09-12 由 owner 修掉：`sites.py` 按**名字**取址、`.def` 补导出了
 *   `BP_ce_enemy_death` —— 后者曾让 thcrap `GetProcAddress` 拿不到 handler、游戏里**静默不生效**。）
 *
 * 回调里的纪律（`DEATH` §2.4 / §4.1）：
 *   · **只做 O(1) 入队**（`trance_kill_note`：一次数组写 + 计数 ++）；清场帧一次会被调 100–300 次；
 *   · **禁止**析构敌人、**禁止**缓存 `ev->enemy`（同帧 sweep 就删）⇒ 队列里**只放标量**
 *     （x/y 浮点快照），帧末 `trance_kill_take()` 一次取走；那对坐标就是**神灵实体的出生点**
 *     （2026-09-12 第二轮起真的用它建 VM，见下）。
 *   · 帧末那一步（`trance_kill_take()` → `trance_spirit_spawn()`）在 `on_item_frame` 里，
 *     **不在回调里** ⇒ 建 VM / 记账都不在死亡栈上（`DEATH` §2.4 第三条禁令）。
 *
 * ## ★ 神灵实体：**已实装**（2026-09-12 第二轮，自建 VM 池 = 本文件 `s_spirit[]`）
 *
 * **用户 2026-09-12 裁决（binding）**：
 *   「113 神灵实体 ≈ P 点、蓝点等**下落道具**，但是：**1、不会下落；2、不会被全屏收集收集，
 *     必须跟过去吃；3、会随着时间不断变小而消失；它不是敌人。**」
 *
 * 落地形状（每一条都点名代码位置）：
 *   · **不是敌人**：不建敌人、不碰 `EnemyManager`（本文件只在 `trance_enemies_present()` 里**读**它）。
 *   · **不是引擎道具**：不进 `ItemManager` 的道具池 ⇒ 原生磁吸 / 回收线 / `ce_item_collect_all()`
 *     全屏收点**天然看不见它**（裁决 2 要的正是这个）。★ **本文件不调用任何全屏收点 API**
 *     （`ce_item_collect_all` / `ce_cancel_all_bullets` / `ce_cancel_radius` 一个都没有 —— 实测
 *     `grep -c` 为 0，见交付报告）⇒ 「全屏收点收不走神灵」不是靠 `if` 挡住的，是**结构上不可能**。
 *     回收完全由本文件自己算（`trance_spirits_tick()` 的距离判定）。
 *   · **不下落**（裁决 1）：`x/y` 是**出生点**，整个生命周期**只被原地微动**改变
 *     （`trance_spirit_bob_offset()`，±1 px 方波，❓ 自定）——**没有速度、没有重力、不出界删除**。
 *   · **靠近才吃**（裁决 2）：`trance_spirit_in_pickup_range()`（core，整数平方比较，
 *     半径 `TRANCE_SPIRIT_PICK_RADIUS` = ❓ 自定 24 px；不是引擎道具的 30 px 收点半径）。
 *   · **缩小 + 到点消失**（裁决 3）：`trance_spirit_scale_permille()`（core）每帧写
 *     `ce_anm_set_scale`；生命 `TRANCE_SPIRIT_LIFE_FRAMES`（一手 480 帧）归零 ⇒
 *     `trance_spirit_expired()` ⇒ 播载体脚本的 interrupt 1（消失段）并回收槽。
 *   · **容量**：`TRANCE_SPIRIT_POOL`（❓ 自定 128；一手 TH13 是 256 槽）⇒ 溢出**丢尾**并
 *     计 `s_stat_spirit_dropped`（诊断，进汇总日志）。
 *   · **生成时机**：帧末（`on_item_frame` ⑤），源 = 已有的击破队列（带 `x/y` 快照）
 *     ⇒ **不在死亡回调里建 VM**（`DEATH` §2.4 第三条禁令）。
 *
 * ## ⛔ 仍未接线（诚实登记，别当成已实现）
 *
 *   · **通道 E（玩家死亡每帧掉 type 3）/ F（连锁真撒一颗 type 3）**：SPEC §3.3 ② / §4 都是 ⏳ 待拍
 *     ⇒ 只记账 / 只打日志（`trance_chain_note` 的计数与 `chain_extra` 日志照跑），不撒。
 *   · **救援层（107 → 117）**：**不是未接线** —— 定案（`EC` §5.2）是「**全部同时爆发**」，
 *     107（`cards/senro.c`）与 117（`cards/animal.c`）**各自**在自己的 `+0x10` 里动作 + 救命，
 *     消弹归属走 `ce_cancel_latch_claim`、无敌走 `ce_invuln_request`（取 max）⇒ **卡间零互调**。
 *     本卡的定位是**事后层**（死亡已确定之后，挂 `+0x14`，即用户口径的「回光返照」）
 *     ⇒ 只记录、不代人执行、且**结构性拒绝进救援层**（本文件不出现 `cancel_impending_death`）。
 *   · **BGM 变体 / 灵界背景 anm / 神灵专属切图**：SPEC §5 全部 ⏳（资产未做）⇒ 不起背景 VM；
 *     神灵实体本身用**借来的现成脚本 + 贴图 + 染色**顶着（见下「载体脚本」段的 ❓ 自定登记）。
 */
#include <stddef.h>
#include <string.h>
#include <stdio.h>

#include "sdk.h"
#include "trance_core.h"

#define TRANCE_NUM  113          /* ⚠ `CE_CARD` 会对 ID 做 ## 拼接 ⇒ 里面必须写字面量 113 */
#define TRANCE_LOG_EVERY 600     /* 每 600 游玩帧一条汇总 */

/* ---- 音效：TH13 的 id 在 TH18 无意义 ⇒ **全部 ❓ 自定，实跑首日校准**（SPEC §5 末行）----
 * 只挑 TH18 现有表里语义最近的（SDK.md §7 的零售 se 表）。 */
#define TRANCE_SE_PICK      0x3f   /* ❓ 自定（TH13 收神灵也是 0x3f，但两作表不同源）—— 收集音 */
#define TRANCE_SE_LEVELUP   0x44   /* ❓ 自定（TH13 跨格 0x44）—— 跨格音 */
#define TRANCE_SE_ENTER     0x34   /* ❓ 自定（TH13 灵界进入 0x34）—— 进入灵界 */
#define TRANCE_SE_END       0x46   /* ❓ 自定（TH13 灵界结束 0x46）—— 结束灵界 */
#define TRANCE_SE_FORCED    0x2c   /* ❓ 自定（se_bomb 系）—— 死亡强制的灵界（TH13 无对应音）*/

/* ==================== ★ 神灵实体（自建 VM 池）：载体脚本 / 贴图 / 层 / 染色 ====================
 *
 * TH18 **没有「神灵」这个美术**（原版 `astral.anm` 是 TH13 的资产，SPEC §5 台账里是 ⏳ 未做）
 * ⇒ 按 `METHOD.md` 铁律「UI / 动画一律先从原代码找」的**退路条款**：借现成的可复用件 + 标 ❓ 自定，
 * **不包装成复刻**（这里逐条写清「借了什么、为什么借它、差在哪」）。
 *
 * ### 载体脚本：`ability.anm` script129（= `CE_ANM_ABILITY_SCRIPT_SEASON_BULLET_SUMMER`）
 * 先例 = 110/111 的绿点借季节点脚本（`cards/faith.c`：`CE_ANM_ABILITY_SCRIPT_SEASON_POINT` 115）。
 * **为什么不是 faith 那条 115**（照抄先例前先看脚本本体，这是本次唯一一处刻意不照抄的地方）：
 *   `assets/ability/scripts/115_season_point.anm.txt` 的**主路径在跑 `scaleTime`**
 *   （`-1: scale(2,2)` → `+16: scaleTime(300, 0, 0.7, 0.7)`）⇒ 卡侧每帧写 `ce_anm_set_scale`
 *   会被 VM 自己的插值器**覆盖**（`sdk.h` 的 `ce_anm_set_scale` 注释就是这条），
 *   裁决 3 的「不断变小」**做不出来**。
 *   129 的主路径是 `-1: layer(13); type(1); alpha(220); …` + `0: stop();`（**没有 scaleTime**）
 *   ⇒ 换贴图 / 写 scale / 写 color 都由卡说了算（同 `117_season_glyph`「脚本里只给默认值，
 *   卡每帧 `ce_anm_set_sprite` 换」的既有口径）；它的 `interruptLabel(1)` 是一条
 *   「layer 15 + 加算 + 20 帧放大到 2.0 + 换 3 张贴图 + 自删」的消失段 ⇒ 正好当**吃到 / 到点**的收束。
 *
 * ### 贴图：零售 P 点（`CE_ANM_ABILITY_SPRITE_SOUL_0`）
 * 裁决把神灵定义成「≈ **P 点**、蓝点等下落道具」⇒ 用**零售切图**里的小道具样贴图（16×16），
 * 不自己画（`METHOD.md`）。`SOUL_*`（TH17 `beast.anm` 的魂，16×16，**近白**）是我们手上唯一
 * 既能当「魂 / 神灵」又**能被染色染得动**的那一格（`ce_anm_set_color` 是**乘算**染色：
 * 纯红/纯金的道具贴图染成绿/蓝会变黑，近白的才染得出颜色）。
 * ⚠ 差在哪：这不是 TH13 `enemy/littlegods.png` 那张 30×30 的神灵图，也不是 P 点那张 16×16
 * 的道具图 —— 拿到真美术后**只改 `spirit_sprite_for()`**，其余（池 / 曲线 / 判定）一行不动。
 *
 * ### 层：世界列表 **13**（= 道具 / 子弹层，照 `cards/faith.c` 的 `FAITH_LAYER`、`season.c`）
 * ⚠ 与 `HUD-LAYOUT.md` §4.1 的「HUD 一律 UI 层 24/25」**不冲突**：那条规则管的是**UI 列表的 HUD**
 *   （24→绘制组 37、25→组 38，为了被暂停模糊罩盖住）；神灵是**世界列表的场上道具样实体**，
 *   位置来自敌人死亡坐标（`ev->x/y`，与 `cards/mof.c:238` 的 `faith_spawn(ev->x, ev->y, …)` 同空间），
 *   必须走**相机 3 的游玩区**（世界层 3–19）。放进 UI 列表（相机 2、无 originMode(1)）会整体偏位
 *   （游玩区 32…416 与整张表面的原点不同）⇒ 那是**画错地方**，不是「更安全」。*/
#define TRANCE_SPIRIT_ANM_SCRIPT  CE_ANM_ABILITY_SCRIPT_SEASON_BULLET_SUMMER   /* ❓ 自定：见上 */
#define TRANCE_SPIRIT_SPRITE      CE_ANM_ABILITY_SPRITE_SOUL_0                  /* ❓ 自定：零售魂贴图 */
#define TRANCE_SPIRIT_LAYER       13     /* 世界列表·道具层（faith.c 的 FAITH_LAYER 同值）*/

/* ---- 四类神灵的染色（**一手**：TH13 `astral.anm` 的 `color()` 值，`SPIRIT` §2.3 表）----
 * 原版一颗神灵 = **外环 + 内芯**两个子脚本（`scriptNew(2)/scriptNew(3)`），各有自己的 `color()`；
 * 我们只有**一个** VM、`ce_anm_set_color` 只写 `color_1` ⇒ 取**内芯**那一路当整体色
 * （外环值抄在注释里备查：type0 绿 (0,127,0) / type1 橄榄 (63,63,0) / type2 紫 (63,0,63) /
 *  type3 灰 (63,63,63)）。真美术到位后可按 type 换贴图 + 用 `color_2` 补外环。
 * ⚠ 乘法染色的后果：近白贴图 × 下列颜色 ≈ 原色；深色贴图会偏暗（这是占位期的已知差）。*/
#define TRANCE_TINT_TYPE0   0xffff20ffu   /* 一手：type 0 内芯 (255,32,255) 品红 */
#define TRANCE_TINT_TYPE1   0xff5050ffu   /* 一手：type 1 内芯 (80,80,255) 蓝（击破通道用的就是它）*/
#define TRANCE_TINT_TYPE2   0xff20ff20u   /* 一手：type 2 内芯 (32,255,32) 绿 */
#define TRANCE_TINT_TYPE3   0xff808080u   /* 一手：type 3 内芯 (128,128,128) 灰 */

/* ---- 「中弹无伤」：本卡自己这一层的闩锁（SPEC §3.2 是 ⏳ 阻塞项，见下）----
 * 一手的语义（`th13:0x444ca6`：`Player::on_hit` 第一行读 `GAME_FLAGS & 0x100` 直接 return）
 * 在 TH18 侧**没有对应点**（SPEC §3.2 表最后一行的 TODO：自机 AABB 判定 `0x443fd0` 系 +
 * `Player::on_hit` 一侧的帧流程）。本卡**不猜**、**不伪装 bomb / 对话状态**
 * （`EC` §4.1 第 3 条：那是已知雷区，会连带改道具吸附）。
 * ⇒ 现在只做「**每帧声明无敌帧**」这一件事：灵界进行中声明 TRANCE_INVULN_DECLARE 帧。
 * ⚠ 这与一手语义**不等价**（一手是「撞了但不算撞弹」：不记 miss、不进死亡流程；
 *   无敌是「子弹不判定」）—— 两者的差别在 SPEC §7 的实跑清单第 2 条里要逐条验。
 * `❓ 自定` 的帧数：取 TH13 灵界自己的最短可感窗口（30 帧）——**等一手原语落地后应当整个删掉**。*/
#define TRANCE_INVULN_DECLARE 30

/* ---- 最大得点的单位（SPEC §4 / §9 #5 ⏳）----
 * TH18 `0x4ccd24` 是 ×100 定点（一手：`max_point = (MONEY + 1) × K × 100`，
 * K = EASY 500 / 其余 1000；一手汇编见 `trance_core.c` 的 `trance_max_point_field_addends` 注释）。
 * ⚠ 本卡**不直接写这个全局**（会牵动原生最大得分显示与别的卡的读点，`IDEAS:180` 的订正段）
 * ⇒ 只把它当「单位口径」用：type 1 的 +10 / +100 在这里换算成定点加数，真正落笔走计分层。
 * TODO(实装期，待用户拍)：SPEC §9 #5 三选一（原生字段 / `on_item_score` 声明 / 卡内计数）。*/
#define TRANCE_MAXPOINT_FIELD_UNIT 100

/* ---- 计分层（`EC` §3）----
 * 形状（SDK owner 2026-09-12 澄清，订正我先前「层没接上」的误判）：
 *   · `ce_score_declare` / `ce_score_aggregate` / **`ce_score_applier`** 都在 `sdk_core.h`
 *     （**不在** `sdk.c`，所以我先前 grep 不到 —— 那是搜错了文件）；
 *   · `BP_ce_item_score` 的「逐张广播 `on_item_score(&value)`」是**故意保留**的形状：
 *     各卡只**声明**（`ce_score_declare`），**被选举的落笔者**（id 最小）在广播里自守
 *     `if (ce_score_applier() != 我的 id) return;` 然后落笔 —— 110/111 都是这么写的。
 *
 * ★★ 2026-09-12（本次）：**113 的声明已停用**（`trance_declare_score()` 不再写 `ce_score_declare`）。
 *   为什么**必须停**（不是「先放着无所谓」）：
 *     · 本卡原先声明的是 `trance_max_point_field_addends(...)` = **10000**（灵界里 type 1 的那份
 *       「最大得点 +100 显示单位」换算成 ×100 定点）。而 `ce_score_aggregate()` 是**所有声明卡之和**，
 *       **110 / 111 的落笔者把它当「信仰总量」用**（`mof_core_pv()` 的 `aggregate` 形参）⇒
 *       只要 113 在场且灵界开着，110 落笔的 PV 里就会**凭空多出 10000**（两卡同装那一支还算进
 *       `delta = agg − 基础_110 − 基础_111` ⇒ 直接进被乘数），**并且与「113 该不该算在
 *       on_item_score 这一层」这个尚未拍板的问题捆绑**（`SPEC §9 #5` 的载体三选一：
 *       原生 `0x4ccd24` / `on_item_score` 声明 / 卡内计数 —— **还没拍**）。
 *     · 按「**未定项不许悄悄生效**」的原则 ⇒ 停声明、保留本地计数与日志（下面 `s->decl_*`），
 *       等 §9 #5 拍板再接。
 *   拍板后**应该怎么接**（三条，届时只动本函数、**不动 core**）：
 *     · 若选「`on_item_score` 声明」：把下面那行 `ce_score_declare(TRANCE_NUM, decl)` **打开**即可；
 *       但要连带解决**两个数不同量纲**的问题 —— 113 声明的是「最大得点定点加数」，
 *       而 110/111 声明的是「信仰」，两者被 SDK **无条件求和**进同一个 `aggregate`
 *       ⇒ 要么给 SDK 加**第三个通道**（`ce_score_maxpoint_declare` 之类），要么 113 只在
 *       **没有 110/111 在场时**（`ce_score_applier() == 113`）才声明。**这一步是 owner 的决策，不是本卡能自决的**；
 *     · 若选「原生 `0x4ccd24`」：本函数整个删掉，改成写那个全局（⚠ 会牵动原生最大得分显示与别的卡
 *       的读点，`IDEAS:180` 的订正段）；
 *     · 若选「卡内计数」：本函数删掉，落点搬到 `trance_on_spirit_collected()` 里（那里已有
 *       `r.max_point` 的本地账）。
 *   ⇒ 三种都**不需要**改 `trance_core.*`（纯逻辑与载体无关，单测继续绿）。*/

typedef struct {
    trance_state_t core;
    int32_t frame;            /* 本卡的**游玩帧计数**（on_item_frame = 正常游玩帧，见下）*/
    int32_t last_gauge;       /* 上一帧的槽值（HUD 只在变了的时候重排）*/
    int32_t log_frame;
    int32_t stat_forced;      /* 诊断：死亡强制次数 */
    int32_t stat_levelups;    /* 诊断：跨格次数 */
    int32_t stat_kill_drop;   /* 诊断：击破记账（入队→帧末取走）的颗数 */
    int32_t stat_kill_spirit; /* 诊断：由击破**撒出去**的神灵实体颗数（★ 不等于收到的颗数 ——
                               * 2026-09-12 第二轮：只有走进去吃到的才入槽，没收到的会自然到期消失）*/
    /* ★ 2026-09-12：计分声明**停用**后的本地账（`SPEC §9 #5` 未拍 ⇒ 只记不发，见 trance_declare_score）。
     *   `decl_blocked` = 被挡下的（本会声明出去的）帧数；`decl_last` = 最后一帧本会声明的定点加数
     *   （0 = 那一刻在灵界外 / 没有该声明的量）。拍板后把声明打开时，这两个字段留着当对账用。*/
    int32_t decl_blocked;     /* 诊断：被挡下的声明次数（本卡还在灵界里的每一帧 +1）*/
    int32_t decl_last;        /* 诊断：最后一次本会写进 ce_score_declare(113, …) 的值（定点）*/
} trance_card_t;

_Static_assert(sizeof(trance_card_t) <= 496, "ce_state 上限 496 字节（超了会静默返回 NULL）");

/* HUD 的两行文字（DLL 静态区；113 同时只会有一张在场 —— 与 `cards/mof.c` 的三个 VM 同口径）。
 * ⚠ 用**贴图 VM 字形**（`ce_hud_vm_text_*`）而不是 ASCII：ASCII 盖不到暂停模糊罩下面
 *   （`sdk.h` 的 CE_HUD_VM_TEXT 段有实跑证伪记录）。只认 `0-9 . % + -` 与空格（3.7 s 打不出来）。*/
static ce_hud_vm_text_t s_txt_gauge;    /* 槽值（0..600）*/
static ce_hud_vm_text_t s_txt_time;     /* 剩余秒数（%.2fs ⇒ 位）：整数点后用 2 位小数 */
/* ---- HUD 落点（SPEC §9 #6 ⏳ 未定）----
 * 本卡**不用** `ce_hud_slot_claim`：那个分配器是「左下角系统卡货架」（`HUD-LAYOUT.md` §2），
 * 而灵界槽在原作是**游玩区左下、与季节/动物灵同一带**——113 与 116/117 同装时的排布未定
 * ⇒ 先按 `HUD-LAYOUT.md` §1.2 的那条锚点线（左 257 / 底 924，两张现有卡独立推出的同一条）
 *   手动摆两行，标 `❓ 自定`，等 §9 #6 拍板（届时换成 `ce_hud_slot_claim_card`）。
 * 坐标是 **640×480 字面量空间**（`ce_hud_vm_text_*` 的口径）。*/
#define TRANCE_HUD_X      132.0f   /* ❓ 自定：640 空间 ≈ 表面 264（= 锚点 257 一带）*/
#define TRANCE_HUD_GAUGE_Y 449.0f  /* ❓ 自定：640 空间 ≈ 表面 898（底边 924 上方两行）*/
#define TRANCE_HUD_TIME_Y  459.0f  /* ❓ 自定 */
#define TRANCE_HUD_COLOR   0xffffffffu
#define TRANCE_HUD_COLOR_HOT 0xff60c0ffu   /* ❓ 自定：灵界中染成暖色（原版灵界的观感未抄到）*/

static trance_card_t *st(ce_card_t *c) { return ce_state(c, trance_card_t); }

/* ★ 入槽函数（定义在下面「收集」段）：`on_item_frame` 的帧末要用它 ⇒ 先声明。
 * 它就是把「击破 → 神灵实体 → 靠近才有 +2」这条链接上的**最后一环**（见文件头「槽怎么被填」段）。
 * 第 3 参 = 神灵的**世界坐标 x**（收集音的声像用它；没有实体层的年代这里只能读自机 x，`❓ 自定` 已消）。*/
static void trance_on_spirit_collected(ce_card_t *c, int32_t type, float x);

/* ---- 引擎面读取（薄包装；缺宏就在本文件里补 `static inline` + 一手出处）---- */

/* 「场上有敌」：TH13 是 `ENEMY_MANAGER_PTR->+0xb8 != 0`（`th13:0x40582a`）。
 * TH18 侧同一语义的现成字段 = `zEnemyManager.enemy_count_real`（`engine.h:211`，一手）。*/
static inline int trance_enemies_present(void)
{
    uint8_t *em = CE_ENEMY_MGR();
    if (!em) return 0;
    return (*(int32_t *)(em + CE_EM_ENEMY_COUNT) > 0) ? 1 : 0;
}

/* ---- ★ 第 8 条激活条件：`(GAME_FLAGS & 0x30) != 0x20`（`th13:0x405859`）----
 *
 * **一手定位（本次做的，地址不是猜的）**：TH13 那个 `GAME_FLAGS` 是 `th13:0x4be830`；
 * TH18 v1.00a 里同形的全局 = **`0x4cccc8`**（本机 Capstone 线性扫描全 `.text` 的实证）：
 *   · `test byte ptr [0x4cccc8], 0x30` 这一形状在**全 exe 出现 9 次**
 *     （`0x417cf5` / `0x417d79` / `0x459b93` / `0x459d71` / `0x459f1f` / `0x46669e` /
 *       `0x46679a` / `0x4670d9` / `0x467154`），**其它地址一次都没有**
 *     （唯一另一处 `test …0x30` 是 `0x45eb14` 的输入位 `0x4ca428`，与状态无关）；
 *   · 位用法与 TH13 的 `0x4be830` **逐位同形**：位 0/1/2/3/6 由 `or`/`and` 写入
 *     （`0x44335b or 2` / `0x4433c8 or 8` / `0x443409 or 1` / `0x443aed or 4` / `0x4651f5 and ~0x40`），
 *     位 4–5 构成 2 bit 状态域 `(flags >> 4) & 3`（`local/_scratch/pause.c` 的反编译 `uVar7 = DAT_004cccc8 >> 4 & 3`）；
 *   · 本仓已有文档按 `DAT_004cccc8 & 0x30` 引用它并与「replay 回放 / 练习模式」并列：
 *     `engine/card/th18/05-shop-and-money.md:97`（商店 30 帧自动退的三个条件之一）。
 * **语义（🟡 一手站址 + 一手旁证，人类可读的那半句未完全坐实）**：该 2 bit 域**只被写成 0 或 2**
 *   （写点：`0x461a1a`–`0x461a27` 一带 —— replay/demo 装载路径里 `[0x4ccd14] >= 0` ⇒ `flags = flags&~0x10|0x20`，
 *   否则 `flags &= ~0x30`）⇒ `& 0x30 != 0x20` ≡ `& 0x30 == 0` ≡「**不在那个非正常游玩态**」。
 *   旁证三条：① 该态下不注册 top10（`local/_scratch/pause.c` 的 `if (uVar7 != 2) { if (uVar7 == 0) … }`）；
 *   ② 该态下不画「Pause」文字（`pause.c`：`if (((byte)DAT_004cccc8 & 0x30) != 0x20) FUN_00477a50(…,"Pause")`）；
 *   ③ 该态下商店 30 帧自动退（`05-shop-and-money.md:97`）。
 *   ⇒ 读作「回放 / 演示（非正常游玩）」，但**具体是哪一种**留给实跑（见下 TODO）。**取值一样照抄一手**。
 *
 * ⚠ 编码方式：**不是** `test byte [0x4cccc8],0x30` 的等价 C 写法问题 —— 一手就是「只取低字节的 0x30 位」，
 *   所以这里按 **uint8_t** 读（与 `0x46669e` 的 `test byte ptr` 同宽），不是按 dword 读。
 * TODO(实跑首日)：在回放 / 演示 / 练习各态下各打一条日志，确认 `(flags>>4)&3` 的值与「不能激活」是否吻合；
 *   若发现它在正常游玩里也是 2（即状态域取值不是这个意思），**立刻把这条门退化成恒 1 并在报告里记一笔**。*/
#define TRANCE_ADDR_TH18_GAME_FLAGS  0x4cccc8u
#define TRANCE_GAME_FLAGS_MASK       0x30u   /* 一手 `0x405859` 的掩码 */
#define TRANCE_GAME_FLAGS_EXCLUDED   0x20u   /* 一手 `0x405859` 的「不满足」值 */
static inline int trance_game_flags_ok(void)
{
    uint8_t f = *(volatile uint8_t *)TRANCE_ADDR_TH18_GAME_FLAGS;
    return ((f & TRANCE_GAME_FLAGS_MASK) != TRANCE_GAME_FLAGS_EXCLUDED) ? 1 : 0;
}

/* 把 core 的激活条件表喂满（**8 条一条不少**，每条都对应引擎上的一个现成读点，不猜）：
 *   · full          ← 我们自己的槽（core）
 *   · enemies       ← CE_EM_ENEMY_COUNT > 0
 *   · cooldown      ← 我们自己的冷却计数器（core 的 trance_tick 里涨）
 *   · dialogue      ← CE_GUI_MSG_ACTIVE()（`th13:0x40584d` 的同语义读点）
 *   · bombing       ← CE_BOMB_ACTIVE()（`th13:0x405806` 的同语义读点）
 *   · key           ← **SPEC §9 #1 ⏳ 未定** ⇒ 走 `auto_mode = 1`（「满槽自动进」兜底口径）
 *   · game_flags_ok ← ★ **第 8 条**：`(GAME_FLAGS & 0x30) != 0x20`（`th13:0x405859`）
 *                     ⇒ TH18 全局 `0x4cccc8`，见 trance_game_flags_ok() 的一手站址表
 * ⚠ 不读 `CE_ADDR_INPUT_*` 找「免费键位」：TH18 的配置表只认 6 个键（`SDK.md`），
 *   而 `ce_key_down`（GetKeyboardState）**不进 replay 输入流** ⇒ 用键发动的效果在回放里不重现。*/
static void trance_fill_cond(const trance_card_t *s, trance_cond_t *c)
{
    memset(c, 0, sizeof *c);
    c->full = (s->core.gauge.gauge >= TRANCE_GAUGE_MAX) ? 1 : 0;
    c->enemies = trance_enemies_present();
    c->cooldown = (s->core.cooldown >= TRANCE_COOLDOWN_FRAMES) ? 1 : 0;
    c->dialogue = CE_GUI_MSG_ACTIVE() ? 1 : 0;
    c->bombing = CE_BOMB_ACTIVE() ? 1 : 0;
    c->in_trance = (s->core.phase != TRANCE_PHASE_IDLE) ? 1 : 0;
    c->key = 0;                       /* ⏳ SPEC §9 #1：没有键 */
    c->auto_mode = 1;                 /* ⏳ 兜底口径：满槽自动进 */
    c->game_flags_ok = trance_game_flags_ok();   /* ★ 第 8 条（一手 `th13:0x405859`）*/
}

/* 计分声明 —— ★ **2026-09-12：已停用（不再写 `ce_score_declare`）**，只留本地计数 + 日志。
 *
 * 为什么停（**污染**实录）：本卡在灵界里本会声明 10000（type 1 的「最大得点 +100 显示单位」× 100 定点）。
 * `ce_score_aggregate()` 是**所有卡的声明之和**，而 110 / 111 的落笔者把它当「信仰总量 / 被乘数」用
 * （`mof_core_pv()` 的 `aggregate`；两卡同装时还要减去两份基础算 `delta`）⇒
 * 113 一在场，**110/111 每次吃金钱道具的 PV 就凭空多 10000**（`SPEC §9 #5` 的「最大得点载体」
 * 尚未拍板 ⇒ 这个效果属于「未定项悄悄生效」，必须停）。
 * 保留的东西：本函数照旧算 `decl`（量纲不变），写进 `s->decl_last` / `s->decl_blocked` 并在汇总日志里打出来，
 * 实跑时能看到「如果接线会声明多少」，对账 §9 #5 的三种载体口径。
 * 拍板后怎么接：见文件头「计分层」段的**三条**（打开这一行 / 加第三通道 / 换成本地计数）——
 * 无论选哪条都**不动 `trance_core.*`**，本卡的单测继续绿。*/
static void trance_declare_score(trance_card_t *s)
{
    trance_reward_t r;
    int32_t decl;

    if (!s) return;
    if (s->core.phase == TRANCE_PHASE_IDLE) {
        s->decl_last = 0;          /* 灵界外：本卡不改最大得点（口径不变，只是不再发出去）*/
        return;
    }
    r = trance_reward(1, 1, 0);
    decl = trance_max_point_field_addends(r.max_point, TRANCE_MAXPOINT_FIELD_UNIT);
    s->decl_last = decl;
    ++s->decl_blocked;
    /* ⛔ 停用（`SPEC §9 #5` 未拍）：下面这行是**唯一**会让 113 污染 110/111 PV 的路，先不写。
     *     ce_score_declare(TRANCE_NUM, decl);
     *  ⚠ 也**不许**改成写 `ce_score_mul_declare` —— 那条通道是「乘数附加项（千分点）」，
     *    113 这份是**加数**：10000 写进去 = 给乘数加 **10000‰（+10.0×）**，PV 直接飞掉。*/
}

/* ================================================================================
 * ★★ 神灵实体池（自建 VM 池）—— 2026-09-12 第二轮
 * ================================================================================
 *
 * 用户裁决（binding，文件头「神灵实体」段有全文）：
 *   ① 不会下落 ② 不被全屏收点收集、必须跟过去吃 ③ 随时间不断变小、到点消失 ④ 它不是敌人。
 *
 * ## 为什么池放**文件级 static**（而不是 `ce_state`）
 *
 * `ce_state` 有 **496 字节硬上限**（`SDK.md:116`，超了 `ce_state_user` 会**静默返回 NULL**）——
 * 池每颗至少要 {id, x, y, life, age, type} = 4+8+12 = 24 B ⇒ 128 颗 ≈ 3 KB，
 * **装不进去**（现有 `trance_card_t` 已经 ≈ 240 B，其中 128 B 是击破队列的 16 组 x/y）。
 * 所以池走文件级 static（与 `cards/faith.c` 的 `s_big/s_small`、`cards/season.c` 同口径）。
 * ★ 代价与对策：**删卡时必须显式清池**（文件级 static 不会随 `ce_state` 一起被 memset）
 *   ⇒ `trance_clear()` 里调 `trance_spirit_clear_all()`（删卡 / 局末 / 关卡开始三条路都走它）。
 *
 * ## 每颗的状态
 *
 *   · `x/y`  = **出生点**（世界坐标；敌人死亡位置快照）。★ 全生命周期**不变** ⇒ 裁决 1「不下落」。
 *   · `life` = 剩余生命帧（出生 = `TRANCE_SPIRIT_LIFE_FRAMES`，每帧 −1；≤ 0 ⇒ 到点消失）。
 *   · `age`  = 已活帧数（喂 core 的**缩小曲线**与**原地微动**）。
 *   · `type` = 神灵类型（击破通道 ⇒ `TRANCE_KILL_SPIRIT_TYPE`；决定染色与奖励）。
 *   · `id`   = anm VM id（**0 = 空槽**）。当前缩放是**每帧算出来的**（core 的
 *     `trance_spirit_scale_permille()`），不存字段 —— 少一个会失同步的状态。
 *
 * ## 生命周期五步（每步的代码位置）
 *
 *   撒（`trance_spirit_spawn`，帧末 `on_item_frame` ⑤）
 *     → 停着（`trance_spirits_tick` 只写 `x, y + bob`；**没有速度 / 重力 / 出界删除**）
 *     → 缩小（同函数：`ce_anm_set_scale` 每帧写 core 算出的千分点）
 *     → 消失（`trance_spirit_expired(life)` ⇒ 播 `interrupt 1` 消失段 + 回收槽）
 *     → 靠近才吃（`trance_spirit_in_pickup_range` 命中 ⇒ `trance_on_spirit_collected` 入账 + 回收槽）。
 *
 * ## ★ 为什么「消失段」期间**不立刻丢掉 id**（`dying` 标志）
 *
 * 吃到 / 到点都用 `ce_anm_interrupt(vm, 1)` 播载体脚本的消失段（TH13 到点也是这么做的，
 * `th13:0x438a61`），那段要 **20 帧**才自己 `delete()`。如果那一刻就把 id 从池里丢掉，这 20 帧里
 * 池**不再持有**这个 VM ⇒ `trance_clear()`（**删卡**那条路）就删不掉它 —— 而删卡发生在**商店**里、
 * AbilityManager 冻住、tick 全停 ⇒ 玩家会看见一张卡的残影挂在屏幕上（`METHOD.md` 铁律二要的正是
 * 「不许留下任何要等下一帧才消失的东西」）。
 * ⇒ 消失段期间槽**继续持有 id**（`dying = 1`）：不再写它的外观、不再判定收它，
 *   每帧用 `ce_anm_get_vm()` 看它有没有自删；自删了才回收槽。**池满时它也算占位**（20 帧内必回收）。*/
typedef struct {
    int32_t id;      /* anm VM id；0 = 空槽（脚本自删之后的下一次 tick 就会走到这一步）*/
    float   x, y;    /* 出生点（世界坐标）；不下落 ⇒ 只读，只有微动会临时偏移写回 VM */
    int32_t life;    /* 剩余生命（帧）；≤ 0 = 到点消失（`trance_spirit_expired`）*/
    int32_t age;     /* 已活帧数（缩小曲线 / 微动相位）*/
    int32_t type;    /* 神灵 type（0..3）；染色与奖励都按它 */
    int32_t dying;   /* 1 = 已在播消失段（interrupt 1）：不再收 / 不再写外观，等脚本自删后回收槽 */
} trance_spirit_t;

static trance_spirit_t s_spirit[TRANCE_SPIRIT_POOL];
static int             s_n_spirit;      /* 池里活着的颗数（0..TRANCE_SPIRIT_POOL，紧凑数组 + swap-remove）*/

/* 诊断（文件级；进 `on_item_frame` 的汇总日志，不占 `ce_state`——496 字节要留给真的状态）*/
static int32_t s_stat_spirit_spawned;   /* 撒出去几颗（含被后续丢掉的）*/
static int32_t s_stat_spirit_picked;    /* 被自机走过去吃掉的颗数（= 入槽的来源）*/
static int32_t s_stat_spirit_expired;   /* 到点（寿命 480 帧）自己消失的颗数 */
static int32_t s_stat_spirit_dropped;   /* 池满 / VM 建失败而**没能撒出去**的颗数（丢尾）*/

/* 贴图与染色：见文件头「载体脚本 / 贴图 / 层 / 染色」段的 ❓ 自定登记。
 * 贴图目前**四类共用一张**（`SOUL_0`）—— 原版是按 type 换贴图 + 换 `color()`，我们只做后者
 * （染色），因为手上没有神灵图；真美术到位后在这里按 type 返回不同 sprite 即可。*/
static int spirit_sprite_for(int32_t type)
{
    (void)type;
    return TRANCE_SPIRIT_SPRITE;
}

static uint32_t spirit_tint_for(int32_t type)
{
    switch (type) {
    case 0:  return TRANCE_TINT_TYPE0;
    case 1:  return TRANCE_TINT_TYPE1;
    case 2:  return TRANCE_TINT_TYPE2;
    case 3:  return TRANCE_TINT_TYPE3;
    default: return TRANCE_TINT_TYPE1;   /* 越界不该发生（type 来自 core 的常量）⇒ 退到击破通道那色 */
    }
}

/* swap-remove：把尾元素换到 i（**不 ++i**，尾元素也要过筛）。与 `faith.c` 的 `dot_remove` 同形。*/
static void spirit_remove(int i)
{
    s_spirit[i] = s_spirit[--s_n_spirit];
}

/* 收池：删光池里所有 VM（**含正在播消失段的 `dying` 那些** —— 它们一样由池持有 id）并清零。
 * ★ **删卡铁律**（`METHOD.md`）：本池是**文件级 static** ⇒ 必须由 `trance_clear()` 显式调；
 *   它同时是「删卡（商店里 AbilityManager 冻住、tick 全停）」与「局末 / 关卡开始」的唯一清理路。
 *   ⚠ 不依赖 tick，也要能**连续多次**执行（原版空白卡一次删光整副牌 ⇒ 可能连着调几遍）。*/
static void trance_spirit_clear_all(void)
{
    int i;

    for (i = 0; i < s_n_spirit; ++i) {
        if (s_spirit[i].id) ce_anm_delete((uint32_t)s_spirit[i].id);
        s_spirit[i].id = 0;
    }
    s_n_spirit = 0;
    /* ⚠ 诊断计数**刻意不清**（它们只增不减、只进日志；清掉反而丢掉「删卡前撒了多少」这条线索）。
     *   只有 `on_stage_start` 想从零对账时才会重置 —— 见那里的注释。*/
}

/* 撒一颗（帧末从击破队列里来；**绝不在死亡回调里调** —— `DEATH` §2.4 第三条禁令）。*/
static void trance_spirit_spawn(float x, float y, int32_t type)
{
    uint32_t id;
    trance_spirit_t *p;

    if (trance_spirit_pool_room(s_n_spirit) <= 0) {   /* 池满：丢尾 + 计数（诊断进汇总日志）*/
        ++s_stat_spirit_dropped;
        return;
    }
    id = ce_anm_spawn(CE_ABILITY_ANM(), TRANCE_SPIRIT_ANM_SCRIPT, TRANCE_SPIRIT_LAYER);
    if (!id) {                                        /* anm 还没装载 / 管理器没起：静默丢弃 + 计数 */
        ++s_stat_spirit_dropped;
        return;
    }

    p = &s_spirit[s_n_spirit];
    memset(p, 0, sizeof *p);
    p->id = (int32_t)id;
    p->x = x;
    p->y = y;
    p->life = TRANCE_SPIRIT_LIFE_FRAMES;              /* 一手 480 帧（core 的常量注释有出处）*/
    p->age = 0;
    p->type = type;
    ++s_n_spirit;
    ++s_stat_spirit_spawned;

    /* 出生即写一次全部外观量：位置（**不下落**，之后只有微动偏移）、贴图、颜色、缩放。
     * 缩放写的是**出生那一帧**的曲线值（= 1.5，一手），不然第一帧会是载体脚本的默认 scale。*/
    ce_anm_set_pos(id, x, y, 0.0f);
    ce_anm_set_sprite(CE_ABILITY_ANM(), id, spirit_sprite_for(type));
    ce_anm_set_color(id, spirit_tint_for(type));
    {
        float sc = (float)trance_spirit_scale_permille(0) / 1000.0f;
        ce_anm_set_scale(id, sc, sc);
    }
}

/* 每帧推进池（`on_item_frame` ⑥，**在撒点之后**）：缩小 → 微动 → 靠近判定 → 到期。
 * ⚠ 一个浮点运算都没有除了「喂给引擎的 int→float 转换」与平方距离的比较
 *   （`-mfpmath=sse` 下全是 SSE；本文件**没有任何返回 float 的函数** ⇒ `make dllx87` 门安全）。*/
static void trance_spirits_tick(ce_card_t *c)
{
    const float *pp = ce_player_pos();
    uint8_t *pl = CE_PLAYER();
    int pickup_ok = 1;
    int i;

    /* 一手的门（`SPIRIT` §4 末行 / TH13 `SpiritItem__on_tick` 各 state 分支）：
     * **自机死亡或复活中（`player+0x476ac` ∈ {2,4}）不被吸引、也不收**。
     * TH18 侧的同一读点 = `CE_PLAYER_STATE`（`engine.h:274`）；`cards/faith.c` 的
     * `faith_player_above_line()` 用的是同一条（那里只为回收线，这里是吃取）⇒ 口径一致。
     * ⚠ 只是「这一帧不吃」，**不**影响缩小 / 到期（神灵照常变小、照常到点消失）。*/
    if (pl) {
        int32_t pstate = *(int32_t *)(pl + CE_PLAYER_STATE);
        if (pstate == 2 || pstate == 4) pickup_ok = 0;
    }

    for (i = 0; i < s_n_spirit; ) {
        trance_spirit_t *p = &s_spirit[i];
        float sc;

        /* VM 已经不在（载体脚本的消失段自己 delete 了）⇒ 回收槽。放在最前面：任何写 VM 的调用
         * 之前都要确认它还活着（`ce_anm_get_vm` 返回 0 时那些写函数会自己 return 0）。*/
        if (!p->id || !ce_anm_get_vm((uint32_t)p->id)) {
            spirit_remove(i);
            continue;
        }
        /* 已在播消失段（吃过 / 到点）：**不动它**（外观交给脚本的消失段），只等它自删。
         * ★ 槽继续持有 id 的理由见上面「为什么消失段期间不立刻丢掉 id」（删卡不许留残影）。*/
        if (p->dying) { ++i; continue; }

        ++p->age;
        --p->life;

        /* ① **到点消失**（裁决 3）：生命归零 ⇒ 播载体脚本的消失段（interrupt 1 = 放大 + 换贴图后自删）
         *    并置 `dying`（**不再收、不再写外观**；脚本删掉自己之后的下一次 tick 才回收槽）。
         *    一手：TH13 也是到点调 `anm_set_interrupt_by_id(vm, 1)`（`th13:0x438a61`）。*/
        if (trance_spirit_expired(p->life)) {
            ce_anm_interrupt((uint32_t)p->id, 1);
            p->dying = 1;
            ++s_stat_spirit_expired;
            ++i;
            continue;
        }

        /* ② **靠近才吃**（裁决 2）：判定用**出生点**（不受微动影响 ⇒ 不会「抖出半径」）。
         *    半径 = `TRANCE_SPIRIT_PICK_RADIUS`（24 px，core）；**全屏收点在这里没有任何作用**
         *    （我们这颗不在引擎道具池里，`ce_item_collect_all()` 扫的是 `ItemManager` 的池）。*/
        if (pickup_ok && pp) {
            int32_t dx = (int32_t)(pp[0] - p->x);
            int32_t dy = (int32_t)(pp[1] - p->y);
            if (trance_spirit_in_pickup_range(dx, dy)) {
                trance_on_spirit_collected(c, p->type, p->x);   /* 入账（槽 +2 / 奖励 / 连锁）*/
                ce_anm_interrupt((uint32_t)p->id, 1);           /* 吃掉的那一下播放消失段 */
                p->dying = 1;
                ++s_stat_spirit_picked;
                ++i;
                continue;
            }
        }

        /* ③ **不下落**（裁决 1）：位置 = 出生点 + 原地微动（±1 px 方波，❓ 自定）。
         *    ★ 没有速度、没有重力、没有出界删除 —— 与 `cards/faith.c` 的飘落/吸引/出界那三段
         *      刻意完全不同（那三件都是「下落道具」的行为，用户明确说 113 的神灵**不要**）。*/
        ce_anm_set_pos((uint32_t)p->id, p->x,
                       p->y + (float)trance_spirit_bob_offset(p->age), 0.0f);

        /* ④ **不断变小**（裁决 3）：每帧写 core 算出的千分点（一手曲线 1.5 → 200 帧 → 0.4）。*/
        sc = (float)trance_spirit_scale_permille(p->age) / 1000.0f;
        ce_anm_set_scale((uint32_t)p->id, sc, sc);

        ++i;
    }
}

/* ============================ 每帧（游玩帧） ============================ */

/* `on_item_frame` = 断点 `0x446ee3`（ItemManager tick 的门卫位次，`sites.py:711` 一手 disasm）：
 * 正常游玩帧一次；菜单 / 商店 / 暂停 / 对话**冻结帧连 CMP 都到不了**（**这里不触发**）
 * ⇒ 它就是本卡要的帧源（与 `cards/collect2x.c` 的 `s_play_frame` 同口径，但**存在私有状态里**）。*/
static void on_item_frame(ce_card_t *c)
{
    trance_card_t *s = st(c);
    trance_tick_t t;
    trance_cond_t cond;
    float kx[TRANCE_KILL_QUEUE_MAX];
    float ky[TRANCE_KILL_QUEUE_MAX];
    int i;

    if (!s) return;
    ++s->frame;

    /* ① 窗口推进（相位 0：推冷却 + 连锁窗口；相位 ≠ 0：槽 −1/帧、归零收尾）*/
    t = trance_tick(&s->core, s->frame);

    /* ② 满槽自动进（唯一入口；SPEC §9 #1 的兜底口径）*/
    if (s->core.phase == TRANCE_PHASE_IDLE) {
        trance_fill_cond(s, &cond);
        if (trance_can_activate(&s->core, &cond) && trance_begin(&s->core, 0, &cond)) {
            ce_play_sound(TRANCE_SE_ENTER, 0.0f);   /* ❓ 声像：+0x14 与这里都不该读自机坐标，
                                                     * 但**进入**不是 +0x14 ⇒ 自机 x 可读 */
            ce_log("trance: enter (manual/auto), gauge %d, frame %d", (int)s->core.gauge.gauge, (int)s->frame);
        }
    }

    /* ③ 「中弹无伤」（SPEC §3.2 ⏳）：灵界进行中**只声明无敌帧**（层取 max）。
     * 每帧重新声明 ⇒ 窗口一结束声明自动掉回 0；`on_delete` 里也显式清一次。*/
    ce_invuln_request(TRANCE_NUM, (s->core.phase != TRANCE_PHASE_IDLE) ? TRANCE_INVULN_DECLARE : 0);

    /* ④ 结束反馈 */
    if (t.ended) {
        ce_play_sound(TRANCE_SE_END, 0.0f);
        ce_log("trance: end (%s), gauge %d, frame %d", t.ended_death_free ? "death-free" : "manual",
               (int)s->core.gauge.gauge, (int)s->frame);
    }

    /* ⑤ ★ **击破 → 神灵实体 → （靠近才）入槽**（2026-09-12 第二轮：实体层就是这一步接上的）。
     * 帧末一次性取走队列（`on_enemy_death` 只入队，**死亡栈上不建 VM**），然后每一条按 SPEC 的条款撒实体：
     *   · 类型 = `TRANCE_KILL_SPIRIT_TYPE`（**type 1**：SPEC §2.2 订正「击破走通道 B `spec1/spec2`」，
     *     一手 `SPIRIT` §3 通道表 B 行 `type=1`）；
     *   · 颗数 = `TRANCE_KILL_SPIRITS_PER_KILL`（❓ 自定 = 1；TH13 的颗数由 ECL `spec1/spec2` 决定，
     *     TH18 无对应 ⇒ SPEC §9 #4 未查，见 core 的常量注释）；
     *   · **位置 = 敌人死亡的 x/y 快照**（队列里带的就是它；`ev->enemy` 指针一律不碰）；
     *   · ★ **不再直接入账**：`TRANCE_KILL_AUTOCOLLECT` 那条「帧末按已收」的临时兜底已删除
     *     （实体层已实装 ⇒ 不再需要兜底）。入账现在只发生在 `trance_spirits_tick()` 的距离判定里。*/
    i = trance_kill_take(&s->core, s->frame, kx, ky, TRANCE_KILL_QUEUE_MAX);
    if (i > 0) {
        int j, k, n = i * TRANCE_KILL_SPIRITS_PER_KILL;
        s->stat_kill_drop += i;
        for (j = 0; j < i; ++j)                       /* 一次击破 = 一条队列项（带 x/y 快照）*/
            for (k = 0; k < TRANCE_KILL_SPIRITS_PER_KILL; ++k)
                trance_spirit_spawn(kx[j], ky[j], TRANCE_KILL_SPIRIT_TYPE);
        s->stat_kill_spirit += n;                 /* 撒出去的颗数（不等于收到的颗数，见汇总日志）*/
        ce_log("trance: %d kill(s) -> spawn %d spirit VM(s) of type %d at (%d,%d) "
               "(pool %d/%d) [queue dropped %d, pool dropped %d]",
               i, n, TRANCE_KILL_SPIRIT_TYPE, (int)kx[0], (int)ky[0],
               s_n_spirit, TRANCE_SPIRIT_POOL, (int)trance_kill_queue_dropped,
               (int)s_stat_spirit_dropped);
    }

    /* ⑥ ★ **神灵池每帧推进**（缩小 / 微动 / 靠近才吃 / 到点消失）—— 裁决四条都在这一个函数里。
     * ⚠ 顺序：**先撒后推**（本帧刚撒的也能吃到；`age` 从 1 开始，出生那帧的 scale 已在 spawn 里写过）。*/
    trance_spirits_tick(c);

    /* ⑦ 计分声明（模板 `EC` §3）：★ **已停用**（`SPEC §9 #5` 未拍 ⇒ 只记本地账，不发声明）。
     * 见 trance_declare_score()：那 10000 一旦发出去会污染 110/111 的 PV 聚合。*/
    trance_declare_score(s);

    /* ⑧ 汇总日志 */
    if (s->frame - s->log_frame >= TRANCE_LOG_EVERY) {
        s->log_frame = s->frame;
        ce_log("trance: gauge %d/%d tier %d phase %d, spirits %d/%d/%d/%d, chain %d(bonus %d), "
               "activations %d forced %d levelups %d killdrop %d killspirit %d "
               "| entity pool: %d/%d alive, spawned %d picked %d expired %d dropped %d "
               "(r=%d px, life %d f, scale %d->%d over %d f), "
               "score-decl BLOCKED (decl %d, %d frames)",
               (int)s->core.gauge.gauge, TRANCE_GAUGE_MAX, (int)s->core.gauge.tier, (int)s->core.phase,
               (int)s->core.spirits_seen[0], (int)s->core.spirits_seen[1],
               (int)s->core.spirits_seen[2], (int)s->core.spirits_seen[3],
               (int)s->core.chain_count, (int)s->core.chain_bonus_spawned,
               (int)s->core.activations, (int)s->stat_forced, (int)s->stat_levelups,
               (int)s->stat_kill_drop, (int)s->stat_kill_spirit,
               s_n_spirit, TRANCE_SPIRIT_POOL,
               (int)s_stat_spirit_spawned, (int)s_stat_spirit_picked,
               (int)s_stat_spirit_expired, (int)s_stat_spirit_dropped,
               TRANCE_SPIRIT_PICK_RADIUS, TRANCE_SPIRIT_LIFE_FRAMES,
               TRANCE_SPIRIT_SCALE_START_PERMILLE, TRANCE_SPIRIT_SCALE_END_PERMILLE,
               TRANCE_SPIRIT_SCALE_FRAMES,
               (int)s->decl_last, (int)s->decl_blocked);
    }
}

/* ============================ ★ 死亡后的强制灵界（虚表 +0x14） ============================ */

/* 挂 **`+0x14` `on_death_frame2`** —— 理由见文件头第 1 条（SPEC 用户定案 ③）：
 * 死亡**已经确定**（扣钱 / 扣火力已发生，`SDK.md:71`）⇒ 我们只做「先开灵界」这一件额外的事。
 * ★ 这个函数里**禁止**：调 `cancel_impending_death`（救援层）、读自机坐标（现场是 respawn 之后）、
 *   改生命 / 炸弹 / 死亡流程（用户定案 ②：死后照死、不延时、不免死）。
 * 返回值：0（与 117 先例一致 —— `+0x14` 不是「救下了」那条路）。*/
static int on_death_frame2(ce_card_t *c)
{
    trance_card_t *s = st(c);
    if (!s) return 0;
    if (trance_force_on_death(&s->core)) {
        ++s->stat_forced;
        ce_play_sound(TRANCE_SE_FORCED, ce_player_x());   /* ⚠ 这是**死亡之后**的一帧：
                                                           * 读 x 只为声像，不当「中弹位置」用 */
        ce_log("trance: death-forced trance (phase 2, gauge %d, frame %d)", (int)s->core.gauge.gauge, (int)s->frame);
    } else {
        /* 没开成 = 已在灵界（`begin` 幂等）或槽 < 200（一手 `0x405a40` 的同一道门）。
         * 两条都是**预期**路径，不是错误。*/
        ce_log("trance: +0x14 forced trance SKIPPED (phase %d, gauge %d)", (int)s->core.phase, (int)s->core.gauge.gauge);
    }
    return 0;
}

/* ============================ 收集（神灵 → 槽 / 奖励 / 连锁） ============================
 *
 * ★ **唯一调用者（2026-09-12 第二轮起）**：`trance_spirits_tick()` 的**距离判定命中那一刻** ——
 *   即「自机走过去吃到了一颗**场上看得见的神灵实体**」。
 *   ⛔ 旧的调用路（帧末把击破队列按「已收」直接入账，`TRANCE_KILL_AUTOCOLLECT`）**已删除**：
 *   实体层已实装 ⇒ 不再需要兜底；现在**只有真的走到 24 px 内才会加槽**（用户裁决 2）。
 *
 * 本函数只做「一颗神灵被吃掉之后该发生什么」：槽 +2（`trance_collect`）→ 收集音（**声像用实体 x**）
 * → 跨格音 → 奖励落地（命碎走引擎原生加法；符卡碎 / 最大得点仍是 ⏳，见下两条日志）。
 * 它**不碰 VM**（删 VM / 回收池槽是调用方的事）⇒ 池与记账解耦，单测能单独测 core 那一半。*/
static void trance_on_spirit_collected(ce_card_t *c, int32_t type, float x)
{
    trance_card_t *s = st(c);
    trance_collect_t col;
    trance_reward_t r;

    if (!s) return;
    if (!trance_collect(&s->core, type, &col)) return;

    r = trance_reward(type, (s->core.phase != TRANCE_PHASE_IDLE) ? 1 : 0, 0);
    /* 收集音的声像 = **神灵实体的 x**（`SPIRIT` §3 通道 B 的 `pos` 语义）。
     * ⚠ 旧版本这里读 `ce_player_x()` 并标 ❓ 自定（那时没有实体）—— **该 ❓ 已消**。*/
    ce_play_sound(TRANCE_SE_PICK, x);
    if (col.tier_crossed) {
        ++s->stat_levelups;
        ce_play_sound(TRANCE_SE_LEVELUP, 0.0f);
    }
    /* 连锁奖励（一手 `th13:0x4391a0`：**第 10 颗**当场额外送 1 颗 type 3，那颗就是场上的第 11 颗）
     * ——「要不要做」是 SPEC §4 的 ⏳ ⇒ 现在**只记账 + 打日志**。
     * ⚠ 注意：实体层已经在了，所以这条**不再是「撒不了」**，而是「**要不要**按一手加这条通道」
     *   还没拍板（SPEC §9 #4）⇒ 拍板后就在这一行调 `trance_spirit_spawn(自机位置, 3)`，别再等别的东西。*/
    if (col.chain_extra)
        ce_log("trance: chain bonus at count %d -> extra type 3 required "
               "(policy TODO: SPEC §9 #4 — entity layer is ready)",
               (int)s->core.chain_count);
    /* 奖励落点：命碎 / 符卡碎走引擎原生加法（各自自带进位与 HUD 刷新）*/
    if (r.life_frag > 0) {
        int k;
        for (k = 0; k < r.life_frag; ++k) ce_add_life_fragment();   /* `sdk.h`：自带进位 + 刷 HUD */
    }
    if (r.spell_frag > 0) {
        /* ⚠ 符卡碎的 helper **不存在**（SPEC §3.4：「`CE_BOMB_FRAGMENTS` 是全局，别裸写」）
         * ⇒ 这里**刻意不写**，只记日志。TODO(实装期)：等 SDK 出 `ce_add_bomb_fragment()`。*/
        ce_log("trance: spell fragment +%d NOT applied (no SDK helper — see SPEC §3.4)", (int)r.spell_frag);
    }
    if (r.max_point > 0) {
        int32_t add = trance_max_point_field_addends(r.max_point, TRANCE_MAXPOINT_FIELD_UNIT);
        /* ⚠ 口径：**没有落笔**（`SPEC §9 #5` 未拍 ⇒ `trance_declare_score()` 已停发声明）
         * ⇒ 这条日志里的 add 是「若接线会声明多少」，不是已经加过的最大得点。*/
        ce_log("trance: max point +%d display (=%d field units) NOT applied "
               "(score layer STOPPED — SPEC 9 #5)", (int)r.max_point, (int)add);
    }
    ce_log("trance: collect type %d (%s) gauge +%d -> %d, lifeFrag +%d spellFrag +%d maxPt +%d chainExtra %d",
           (int)type, (s->core.phase != TRANCE_PHASE_IDLE) ? "trance" : "normal",
           (int)col.gauge_add, (int)s->core.gauge.gauge, (int)r.life_frag, (int)r.spell_frag,
           (int)r.max_point, (int)col.chain_extra);
}

/* ============================ HUD（SPEC §9 #6 ⏳；落点见 TRANCE_HUD_* 注释） ============================ */

static int on_draw(ce_card_t *c)
{
    trance_card_t *s = st(c);
    char buf[24];
    int32_t cs;                 /* 剩余百分之几秒（整数，避免任何浮点 → dllx87 门）*/
    int32_t sec, frac;

    if (!s) return 0;

    /* 槽值行：`gauge/600` —— 只认数字与 `/`？⚠ `ce_hud_vm_text_set` 的字符集只有 `0-9 . % + -` 与空格
     * （`sdk.h` 的 CE_HUD_VM_TEXT 段）⇒ `/` **打不出来**，改用空格分隔。*/
    snprintf(buf, sizeof buf, "%d %d", (int)s->core.gauge.gauge, TRANCE_GAUGE_MAX);
    ce_hud_vm_text_set(&s_txt_gauge, TRANCE_HUD_X, TRANCE_HUD_GAUGE_Y,
                       (s->core.phase != TRANCE_PHASE_IDLE) ? TRANCE_HUD_COLOR_HOT : TRANCE_HUD_COLOR,
                       CE_ASCII_ALIGN_LEFT, 0.0f, buf, "trance");

    /* 剩余秒数行：一手是 `%.2fs`（`th13:0x405940`）⇒ 这里打成 `3.45`（`s` 打不出来）。
     * 槽 −1/帧 ⇒ 剩余帧数 = 槽值（一手：灵界时长 = 进入时的槽值帧数）。*/
    cs = s->core.gauge.gauge * 100 / 60;          /* 百分之几秒（整数）*/
    sec = cs / 100;
    frac = cs % 100;
    snprintf(buf, sizeof buf, "%d.%02d", (int)sec, (int)frac);
    if (s->core.phase != TRANCE_PHASE_IDLE)
        ce_hud_vm_text_set(&s_txt_time, TRANCE_HUD_X, TRANCE_HUD_TIME_Y,
                           (s->core.gauge.gauge < 30) ? TRANCE_HUD_COLOR_HOT : TRANCE_HUD_COLOR,
                           CE_ASCII_ALIGN_LEFT, 0.0f, buf, "trance");
    else
        ce_hud_vm_text_clear(&s_txt_time);

    s->last_gauge = s->core.gauge.gauge;
    return 0;
}

/* ============================ 生命周期 ============================ */

static void trance_clear(ce_card_t *c)
{
    trance_card_t *s = st(c);

    /* ★ 顺序要紧（照 `cards/mof.c` 的教训）：**先退声明层**（删掉的卡不许再往聚合里声明），
     * 再收**外部资源**（HUD VM + ★ 神灵实体池的 VM），最后才清私有状态。
     * ⚠ 2026-09-12：这里的 `ce_score_declare(TRANCE_NUM, 0)` **也一并删了** —— 本卡现在
     *   **根本不进计分层**（进入点是 `trance_declare_score()`，已停用）⇒ 没有要退的声明。
     *   ★ 将来 §9 #5 拍板、把声明打开时，**必须连这一行一起恢复**（METHOD 铁律二：
     *     长持续效果删卡要收干净；`ce_layer_drop_card()` 也会清，但别依赖它）。
     *
     * ★★ **删卡铁律**（`METHOD.md`：「长持续效果的卡必须交代被删掉会怎样」）：
     *   `trance_spirit_clear_all()` **必须在 `if (!s) return;` 之前** —— 池是**文件级 static**，
     *   就算本卡的 `ce_state` 拿不到（`st(c) == NULL`）池里的 VM 也还在场上飘着，
     *   而删卡发生在**商店**里（AbilityManager 冻住、tick 全停）⇒ 只有这一条路收得掉它们。
     *   同一个 `trance_clear()` 也是 `on_run_reset` / `on_stage_start` 的清理路 ⇒ 三种路径一份实现。*/
    ce_invuln_request(TRANCE_NUM, 0);
    ce_hud_vm_text_clear(&s_txt_gauge);
    ce_hud_vm_text_clear(&s_txt_time);
    trance_spirit_clear_all();          /* ★ 神灵实体：删光池里所有 VM + 清计数 */
    if (!s) return;
    memset(s, 0, sizeof *s);
}

static int ctor(ce_card_t *c)
{
    trance_card_t *s = st(c);
    if (!s) return 0;
    memset(s, 0, sizeof *s);
    trance_gauge_reset(&s->core.gauge);   /* 一手：关卡开始写 0（`th13:0x42bbfd`）*/
    s->core.cooldown = TRANCE_COOLDOWN_FRAMES;  /* 开局不必等 30 帧冷却（关卡开始槽是 0，进不去）*/
    s->last_gauge = -1;
    /* ⚠ ctor **不清神灵池**：ctor 会在「每关开始引擎对卡组里每张卡再调一次」时重进
     *   （`sdk.h:357` 的一手记录），而池的生命周期是**关卡**（由 `on_stage_start` / `on_run_reset` /
     *   `on_delete` 收）—— 在 ctor 里清会在换关那一瞬间把上一关残影删掉，也会误删刚撒的实体。
     *   ⇒ 这里只清**卡的私有状态**（上面那行 memset），池交给 trance_clear()。*/
    ce_log("trance: ctor (id %d) — trance gauge 0/%d, auto-enter mode (no key: SPEC §9 #1), "
           "spirit pool %d slots (static)", TRANCE_NUM, TRANCE_GAUGE_MAX, TRANCE_SPIRIT_POOL);
    return 0;
}

static int on_load(ce_card_t *c)
{
    trance_card_t *s = st(c);
    if (!s) return 0;
    /* ⚠ 一次性把「已接线 / 未接线」说清楚（免得实跑看见日志以为坏了）。
     * selfcheck 非 0 = 常量与语义脱钩（改了常量忘了改口径）⇒ 必须在日志里喊出来。*/
    ce_log("trance: load — selfcheck %d (0 = ok); WIRED: enemy-death hook -> kill queue -> "
           "spirit ENTITY pool (%d slots, script %d sprite %d layer %d) -> walk within %d px to collect "
           "(type %d, %d per kill) -> trance_collect -> gauge; "
           "PENDING: score CARRIER (SPEC 9 #5 — **declaration STOPPED**, see trance_declare_score), "
           "HUD slot (#6), channel E/F spawns (#4), astral art (SPEC 5)",
           trance_core_selfcheck(), TRANCE_SPIRIT_POOL, TRANCE_SPIRIT_ANM_SCRIPT, TRANCE_SPIRIT_SPRITE,
           TRANCE_SPIRIT_LAYER, TRANCE_SPIRIT_PICK_RADIUS,
           TRANCE_KILL_SPIRIT_TYPE, TRANCE_KILL_SPIRITS_PER_KILL);
    return 0;
}

static void on_stage_start(ce_card_t *c)
{
    trance_card_t *s = st(c);
    if (!s) return;
    /* 关卡开始：槽写 0（一手 `th13:0x42bbfd`）。相位一并回空闲（上一关末帧不该带进来）。
     * ⚠ `trance_clear()` 里已经把神灵池清空（并删光它们的 VM）⇒ 上一关的残影不会带进新关。*/
    trance_clear(c);
    trance_gauge_reset(&s->core.gauge);
    s->core.cooldown = TRANCE_COOLDOWN_FRAMES;
    s->last_gauge = -1;
    /* 关卡开始也把**池的诊断计数**归零（新关从零对账；`trance_spirit_clear_all()` 刻意不清它们，
     * 因为删卡那条路上我们想留着「删卡前撒了多少」）。*/
    s_stat_spirit_spawned = s_stat_spirit_picked = 0;
    s_stat_spirit_expired = s_stat_spirit_dropped = 0;
}

static void on_run_reset(ce_card_t *c) { trance_clear(c); }
static void on_delete(ce_card_t *c)    { trance_clear(c); }

/* ============================ 击破 → 神灵（事件钩子；断点 0x430240） ============================
 *
 * `on_enemy_death` 由 `sdk.c` 的 `BP_ce_enemy_death` 沿卡链表直接派发（**不经 21 槽虚表**），
 * 所以 `.on_enemy_death = …` 直接就能被调到（不是「等 SDK 补桩」）。
 *
 * 过滤规则（`DEATH` §1.3 / §4 表的 113 行）：
 *   kind == `CE_EDEATH_KILLED`（`retaddr == 0x42f3b7`，life ≤ 0 的**主路**）
 *   且 flags & `CE_EDEATH_F_REAL`（`flags_lo & 0x31 == 0` = 真敌人；`public.h` 的极性订正段）
 * ⇒ **自动排除** `CE_EDEATH_KILL_ALL`（过场/清场 killAll 不算击破，`DEATH:263`）、
 *   `BOMB_KILL` / `ECL_DIE` / `OTHER`（要不要算击破由各卡自己定；本卡**只认自机打死的主路**）。
 *
 * ★ 回调纪律（`DEATH` §2.4）：**只入队（O(1)）**、不许析构敌人、不许缓存 `ev->enemy`、
 *   不许在这里建 VM / 扫全场。位置只抄 `ev->x / ev->y` 两个标量进队列；
 *   帧末由 `on_item_frame` 的 `trance_kill_take()` 一次取走（`DEATH` §4.1 对 109 的 O(n²) 警告同款）。*/
static void on_enemy_death(ce_card_t *c, const ce_enemy_death_t *ev)
{
    trance_card_t *s = st(c);
    if (!s || !ev) return;
    if (ev->kind != CE_EDEATH_KILLED) return;                 /* 只要「被打死的主路」*/
    if (!(ev->flags & CE_EDEATH_F_REAL)) return;              /* 只要真敌人（极性见 public.h）*/
    trance_kill_note(&s->core, s->frame, ev->x, ev->y);       /* O(1)：标量入队，指针一律不碰 */
}

/* ★ 计分层：本卡**刻意不注册** `.on_item_score`（载体 `SPEC §9 #5` 未拍 —— 注册了也只是把
 *   声明值再写一遍、没有载体语义），并且 **2026-09-12 起连 `ce_score_declare(113, …)` 的写入也停了**
 *   （那 10000 会被 110/111 的落笔者当信仰算进 PV 聚合 ⇒ 污染）。完整理由与「拍板后怎么接」
 *   见文件头「计分声明：已停用」段 + `trance_declare_score()`。
 *   本卡的其它功能（灵界槽 / 满槽自动进 / `+0x14` 死亡强制灵界 / 无敌声明 / HUD / 击破记账）
 *   **一律照常**，与计分层无关。*/
CE_CARD(113,
        .ctor = ctor,
        .on_load = on_load,
        .on_item_frame = on_item_frame,
        .on_death_frame2 = on_death_frame2,     /* ★ 死后层 +0x14：见 on_death_frame2 的注释 */
        .on_enemy_death = on_enemy_death,       /* ★ 击破 → 神灵（事件钩子，非虚表槽）*/
        .on_stage_start = on_stage_start,
        .on_draw = on_draw,
        .on_run_reset = on_run_reset,
        .on_delete = on_delete);
