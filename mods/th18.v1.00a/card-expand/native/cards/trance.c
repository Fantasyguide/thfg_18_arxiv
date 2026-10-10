/* 神灵庙 / 灵界（card-expand id **113**，内部 TRANCE）—— 灵界槽 + 灵界窗口 + **中弹救援（延时）** +
 * 神灵奖励。
 *
 * ## 一句话（用户 2026-09-12 **第二版**定案）
 *
 * **收神灵充灵界槽；灵界有两条触发方式 —— ① 手动（D 切到本卡 + 按 C，槽满才可）；
 * ② 中弹：取消掉这次死亡、进灵界（死亡强制档），灵界状态退出后重新死亡。**
 * ⇒ 第 ② 条是**延时**而不是免死（用户原话：「已确定死了之后的回光返照」）。
 *
 * ## 分工：本文件只做「接线」，全部判断在 `cards/trance_core.c`
 *
 *   | 件 | 在哪 | 管什么 |
 *   | --- | --- | --- |
 *   | 灵界槽 / 窗口状态机 / 奖励表 / 连锁 / 救援层记账 / **排队与补死记账** | `trance_core.c` | 纯逻辑、无引擎地址、有主机单测 |
 *   | **本文件** | — | 读引擎面 → 调 core → 落引擎副作用（取消死亡 / 补死 / 无敌声明 / 音效 / HUD）|
 *
 * 设计稿：`docs/superpowers/specs/2026-09-12-ten-desires-113-design.md`（**末尾有订正块**）。
 * ★ **实装期的四条硬结论**（本文件存在的原因，逐条写清出处）：
 *
 *   1. ★★ **113 在救援层的最后一家**（2026-09-12 第二版，用户原话「它一定在救援层的最后出发，
 *      **甚至比决死 B 还靠后**」）⇒ 层内顺序 **107 → 117 → 决死 B → 113**。
 *      ⇒ 落点 = 卡虚表 **`+0x0c` `on_death_after_deathbomb`**：引擎只在**决死窗口与 B 键那条路
 *      都走完之后**才广播它（一手 `0x45c114`，判据在 `0x45c100` 的 `jl 0x45c2a9`）
 *      ⇒ 用它就**结构性**排在 107/117（`+0x10`，在 `Player__die` 里就出手）**与决死 B 之后**。
 *      ⚠ 「挂在 `+0x10` 上只登记、再等帧点动手」那条**第一版实现是错的**（`on_item_frame` 不可达，
 *        见 `AUDIT` §BN7）—— 窗口关掉的那一帧**没有**「命还没扣」的中间态。
 *      ⚠「113 里**不许**出现 `cancel_impending_death`」/「挂 `+0x14` 的死后层」这些**第一版旧口径**
 *        **已被用户推翻**：现在 113 **就是要**调 `Player__cancel_impending_death` `th18:0x45d590`，
 *        并且**必须同时 `return 1`**（引擎拿它的返回值决定扣不扣命）。
 *   2. **`+0x14`（`on_death_frame2`）只做日志**：它现在是「补死真的发生了」的**唯一回执**，
 *      除此之外本卡不需要它做任何事（不再开灵界、也没有记账要清）。
 *   3. **「中弹无伤」不写 `player+0x47774`**：那是 TH18 的**复活无敌** zTimer（`engine.h:277`），
 *      写它既不等于灵界、还会把 107/117 给的长无敌截短（SPEC §2.1 行 2 · `EC` §4.1）。
 *      本卡要无敌帧时**只声明** `ce_invuln_request(113, n)`（层取 max、SDK 统一落笔）。
 *   4. **补死是一次「普通死亡」，允许链式**（用户 2026-09-12 定案原话：「**就这样，允许链式**
 *      （灵界期间攒到咆哮 / 结界开着 ⇒ 补死被救）」）⇒ 本卡**不给补死加任何禁止救援的闸**，
 *      也**没有**任何「已经救过」的状态位：补死之后引擎照常广播 `+0x10` / `+0x0c`，
 *      107/117 与 113 自己都能按各自的门再救一次（113 的门 = 让位检查 + 槽 ≥ 200）。
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
 *      且 `flags & CE_EDEATH_F_REAL`，**读敌人自己的掉落表**算出这一次掉几颗
 *      （TH13 通道 B 的衰减公式，见下 §「★★ 击破掉灵几颗」），然后 `trance_kill_note()`
 *      **只做 O(1) 入队**（`DEATH` §2.4；**一次击破 = 一条队列项**，带颗数）。
 *   ② **撒实体**（帧末 `on_item_frame` ⑥）：`trance_kill_take()` 取走 → **每条按自己的颗数**
 *      调 `trance_spirit_spawn()` 建 N 颗神灵 VM（type = `TRANCE_KILL_SPIRIT_TYPE` = **1**，
 *      SPEC §2.2：击破走通道 B；自建池，见下「神灵实体」段）——★ **不再**在帧末按「已收」入账。
 *   ③ **靠近才吃**（`on_item_frame` ⑧ = `trance_spirits_tick()`）：自机走到 ≤
 *      `TRANCE_SPIRIT_PICK_RADIUS`（❓ 自定 24 px）才调 **`trance_on_spirit_collected()`**
 *      → `trance_collect()`（core）→ `trance_gauge_add(+2)`。**每颗神灵 +2 槽**（一手 `th13:0x438b3d`）。
 *   ④ **满槽 + 8 条条件**：`trance_fill_cond()` 把 core 的条件表喂满（含第 8 条 `(GAME_FLAGS & 0x30) != 0x20`
 *      → `trance_game_flags_ok()`，TH18 全局 `0x4cccc8`，站址见该函数注释）⇒ `trance_can_activate()`。
 *   ⑤ **开灵界（两条路）**：
 *      **① 手动**：`on_activate`（D 切到本卡 + 按 C；2026-09-12 用户定案，原「满槽自动进」已删）。
 *      **② 中弹**：`on_death_after_deathbomb`（**`+0x0c`**）里当场做
 *        「让位检查 → `trance_force_on_death()` → `Player__cancel_impending_death` `0x45d590`
 *        → **`return 1`**」（详见下面「中弹救援」段）。
 *   ⑥ **窗口**：`trance_tick()` 每帧 −1；到点 ⇒ `trance_end()`。
 *   ⑦ **收尾**：手动结束 −200；**中弹救下的窗口结束 ⇒ `t.re_die` ⇒ 本卡补一次 `Player__die` `0x45d3a0`**。
 *      删卡 ⇒ `trance_clear()`（`+0x50`，含清池）。
 *
 *   ⇒ 速率账（一手）：1 颗 = +2 槽，满槽 600 ⇒ **300 颗**；满槽窗口 = 600 帧 = 10.00 s。
 *     ★★ **2026-09-13 起「一次击破 = 1 颗」不再是常数**：走 TH13 通道 B 的衰减
 *     （`n = (T − alive)/(T/N) + 1`，瞬杀 = `N+1`、拖满窗口 = 1，H/L +1；一手 `th13:0x419f41`）
 *     ⇒ **速杀流**：TH18 st01 的小妖精瞬杀给 2~11 颗（原生档位）、没配窗口的敌人走兜底 6 颗；
 *     拖到窗口末尾只剩 1 颗。一关的强弱从此同时由**击破数**与**击杀速度**决定 ——
 *     这是本卡第一次真正复刻「杀得越快掉得越多」这条 TH13 手感（落地记录 `AUDIT` §CK）。
 *
 * ## ★ 中弹救援：落点与地址链（**本段是这次改动的核心**）
 *
 *   ```
 *   自机碰撞（th18:0x45cd27 / 0x45ce81 / 0x45d060 三处 thiscall）
 *     └─► Player__die th18:0x45d3a0：state(+0x476ac) = 4、死亡特效、num_deathbomb_frames(+0x47908) = 8
 *           └─► 最后沿卡链表广播虚表 +0x10（0x45d570–0x45d57c）← 107 / 117 在这里当场取消死亡
 *   之后每帧 Player__on_tick__body th18:0x45be90 的 case 4（入口 0x45c0f4）：
 *     · state4_timer(+0x638) <  8（0x45c0fa / 0x45c100 jl）⇒ 查 B 键那条路 0x45c2a9：
 *         can_bomb 0x420420 → do_bomb 0x420360 → cancel_impending_death 0x45d590（0x45c2d7）→ 跳过扣命
 *     · state4_timer >= 8 ⇒ ① 循卡链表广播虚表 +0x0c（0x45c114，**参数 = 累加器，初值 0**）
 *                          ② 累加器 == 0 ⇒ commit_death_and_enter_state2 0x45d090（0x45c135，**扣命**）
 *                             累加器 != 0 ⇒ 跳 0x45c3d1（0x45c12d），**不扣命**
 *   ★ 113 的轮次 = **就是那个 +0x0c 回调**：让位检查 → 槽门 → cancel_impending_death → return 1。
 *     （「等帧点」那条路不可达：+0x0c 广播在 Player 线 0x17 里，on_item_frame 在 ItemManager 线
 *       0x1e 里 ⇒ 后者跑到时命已经扣了。这是第一版的错，见 AUDIT §BN7。）
 *   ```
 *
 * `cancel_impending_death` `0x45d590` 只写 6 个字段（`+0x644` / `+0x640` / `+0x638` /
 * `+0x63c` / `+0x634` / **`+0x476ac = 1`**），**读零个计时器**；真正的不归点是
 * `commit_death_and_enter_state2` `0x45d090`（全 exe 唯一调用点 `0x45c135`，扣命）。
 * ⇒ 在 `+0x0c` 里「取消 + `return 1`」两件都做到，就一定不会扣命
 *   （★ 一手原生先例：`CardEirin__on_player_death_after_deathbomb` `0x40a4f0`
 *    尾段 `0x40a53f call 0x45d590` → **`0x40a544 mov eax,1`** → `0x40a54a ret 4`）。
 *
 * ## 长持续效果的自查（`METHOD.md` 铁律二 / SPEC §3.5）
 *
 * 本卡跨帧留下的东西：① 灵界槽与窗口相位（卡私有状态 `ce_state`）；② 每帧的**无敌声明**；
 * ③ HUD 文字 VM；④ 击破待撒队列；⑤ ★ **神灵实体池**（自建 VM 池，文件级 static，见下）。
 * ★ **本卡不再有「跨帧排队的死亡」**（第一版那套 `pending_*` 已删）：救援在 `+0x0c` 回调里
 *   一次做完，那一刻要么取消死亡、要么照常死 ⇒ 没有需要跨帧收尾的中途状态。
 * ⇒ `on_delete`（虚表 `+0x50`）与 `on_run_reset` **都走同一个 `trance_clear()`**：
 * **先退声明层**（`ce_invuln_request(113, 0)`）、**再收 HUD**、**再删光池里所有神灵 VM**、
 * 最后 `trance_state_init()` 清私有状态（删卡之后本卡不该再有任何残留）。
 * 删卡发生在**商店**里、那一刻 AbilityManager 冻住、tick 全停 ⇒ 只能靠 `on_delete`，
 * 等下一帧等于没有清理（`SDK.md:71` 那条）。
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
 *   · ★ **`+0x10` 也在死亡栈上**（`Player__die` 的调用点里）⇒ 它的纪律与 `on_enemy_death` 同级：
 *     **不许建 VM / 不许 `ce_spawn_*` / 不许扫全场**（`METHOD.md` 铁律 + `AUDIT` §AL/§BL 的先例）。
 *     本卡的 `+0x10` 只做「写两个 int + 可能一行日志」。
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
 *   · **不下落**（裁决 1）：**没有重力**（一手位置积分走引擎通用 `PosVel`）—— 但**会动**：
 *     ★ 2026-09-13 起落地了原版那套**漂移 / 被吸**（`th13:0x438730` 的 state 机 + 96/128 迟滞 +
 *     `+0.3/帧` 上限 12.0；灵梦 + 灵界态另有任意距离的慢吸 state 4）与**出界删除**；
 *     当年那个「停在原地 + ±1 px 方波」的占位已停用（`trance_spirit_bob_offset()` 不再被调用）。
 *   · **靠近才吃**（裁决 2）：`trance_spirit_in_pickup_range()`（core，整数平方比较，
 *     半径 `TRANCE_SPIRIT_PICK_RADIUS` = **24 px**，2026-09-13 用户口径"这一项不动"）。
 *   · **缩小 + 到点消失**（裁决 3）：`trance_spirit_scale_permille()`（core）每帧写
 *     `ce_anm_set_scale`；生命 `TRANCE_SPIRIT_LIFE_FRAMES`（一手 480 帧）归零 ⇒
 *     `trance_spirit_expired()` ⇒ 播载体脚本的 interrupt 1（消失段）并回收槽。
 *   · **容量**：`TRANCE_SPIRIT_POOL`（❓ 自定 128；一手 TH13 是 256 槽）⇒ 溢出**丢尾**并
 *     计 `s_stat_spirit_dropped`（诊断，进汇总日志）。
 *   · **生成时机**：帧末（`on_item_frame` ⑦），源 = 已有的击破队列（带 `x/y` 快照）
 *     ⇒ **不在死亡回调里建 VM**（`DEATH` §2.4 第三条禁令）。
 *
 * ## ⛔ 仍未接线（诚实登记，别当成已实现）
 *
 *   · **通道 E（玩家死亡：死亡动画期间每帧一颗 type 3，一手 `th13:0x4434d8`）** —— **仍未接线**
 *     （一手那条通道静音 `play_sound=0`，`SPIRIT` §3 通道表 E 行）。
 *   · **通道 F（连锁）** 已接线（2026-09-13 实撒、2026-09-14 把粒度订正到"掉落时刻"）：
 *     见 `trance_spirit_spawn()` 尾部那一段（一手 `th13:0x4391a0`）。
 *   · **救援层（107 → 117 → 决死 B → ★113）**：**已接线**（2026-09-12 第二版）。定案是
 *     107/117「**全部同时爆发**」（各自在自己的 `+0x10` 里动作 + 救命，消弹归属走
 *     `ce_cancel_latch_claim`、无敌走 `ce_invuln_request` 取 max）⇒ 那两家**卡间零互调**，
 *     本卡**不代它们执行**；本卡自己是**最后一家**：在 `+0x0c` 里先查
 *     `ce_rescue_note_owner()`（`>= 0` 且不是 113 ⇒ 这次死亡已经有人救了 ⇒ 什么都不做）。
 *     ⚠ 旧口径「本卡**结构性拒绝进救援层**（不出现 `cancel_impending_death`）」**已作废**
 *       —— 那是第一版的「死后层」设计，用户 2026-09-12 第二版**推翻**了它。
 *   · **链式救援**：补死（灵界窗口结束那一次）是一次普通死亡 ⇒ 107/117/113 都还能再救
 *     （用户定案允许；上限 = 槽 ≥ 200 那道门）。本卡**没有任何**「禁止再救」的闸。
 *   · **BGM 变体 / 灵界背景 anm / 神灵专属切图**：SPEC §5 全部 ⏳（资产未做）⇒ 不起背景 VM；
 *     神灵实体用的是**原版切图 + 原版脚本**（2026-09-13 起；见下「神灵实体」段的表）。
 */
#include <stddef.h>
#include <string.h>
#include <stdio.h>

#include "sdk.h"
#include "trance_core.h"
#include "hud_dim.h"       /* ★ 跨卡统一件：HUD 压暗的门（❓ 用户口径，见文件头）*/
#include "trance_bgm.h"
#include "pl_anm_ids.h"
#include "voice_ids.h"     /* ★ 生成的音效 id：CE_VOICE_ASTRALUP = TH13 原版 se_astralup（见 §DK-7）*/     /* ★ 生成物：pl0X.anm 追加脚本号 + **灵界形态 sprite 加数**（CE_PL_ANM_FORM_BASE_*）*/

#define TRANCE_NUM  113          /* ⚠ `CE_CARD` 会对 ID 做 ## 拼接 ⇒ 里面必须写字面量 113 */
#define TRANCE_LOG_EVERY 600
#define TRANCE_STAGE_INIT_WINDOW 60   /* 进关后多少帧内允许自愈式补写槽初值（衬一次关卡加载的时序差）*/     /* 每 600 游玩帧一条汇总 */

/* ---- ★ 中弹救援用到的两个引擎函数（第二版新增；**地址不是猜的**，逐条给一手证据）----
 *
 * `Player__cancel_impending_death` `th18:0x45d590` —— thiscall(**zPlayer***)，**无栈参**，返回值 **void**。
 *   一手（本机 Capstone 反汇编 `local/th18.v1.00a/th18.exe`，70 字节整函数）：
 *     `mov eax,[ecx+0x644] / test al,1 / … or eax,1 / mov [ecx+0x640],0 / mov [ecx+0x644],eax`
 *     `mov [ecx+0x638],0x3c / mov [ecx+0x63c],0x42700000(60.0f) / mov [ecx+0x634],0x3b`
 *     `mov [ecx+0x476ac],1 / ret`
 *   ⇒ 它**只写 6 个字段、读零个计时器**：`state(+0x476ac) → 1`（活）、**状态/动画计时器 1**
 *     `zTimer{prev 0x3b, cur 0x3c, 60.0f}`（= `player+0x634/0x638/0x63c`，见
 *     [`engine/player/th18/01-position-and-state-timers.md`] 的布局表）。
 *     ⚠ **订正（2026-09-22）**：本行旧注释写「无敌计时器 = 60 帧」是**错的** ——
 *     无敌计时器是 `player+0x47774`（判据读 `+0x47778`），**这个函数一个字都不碰它**
 *     （穷举：`+0x47774/78/7c` 的全 exe 写点表里没有 `0x45d590`）。⇒ **救下之后自机没有无敌帧**。
 *     一手 TH13 同样如此：`Player::die` `th13:0x444a10` 的「免费灵界」支（`th13:0x444a32`
 *     `call TranceManager::begin` → `th13:0x444a3f test eax,eax` → `th13:0x444a43
 *     mov dword [edi+0x65c],1` → `ret`）**也只写一个 state**，不写无敌计时器。
 *     ⇒ 两作都靠「灵界里不会再中弹」来保护，而不是靠救下时给无敌（见下面 ④-2 的体术那一段）。
 *   ⇒ 「决死窗口关掉之后还管不管用」的答案：**管用**（它不看窗口计时器）；
 *    真正的不归点是 `Player__commit_death_and_enter_state2` `th18:0x45d090`（**扣命**）。
 *   先例：`cards/senro.c:51`（107）、`cards/animal.c:268`（117）已经照同一形状在用；
 *   引擎自己也在 `CardEirin__on_player_death_after_deathbomb`（`0x40a53f`，卡的回调里）
 *   与 `Player__on_tick__body` 的决死 B 分支（`0x45c2d7`）调它。
 *
 * `Player__die` `th18:0x45d3a0` —— thiscall(**zPlayer***)，**无栈参**，返回值 **void**（尾 `c3`，不是 `ret N`）。
 *   一手：① 全 exe 的 `call 0x45d3a0` 只有 **3 处**（`0x45cd27` / `0x45ce81` / `0x45d060`），
 *   全是「自机与弹/激光/敌体碰撞」例程里 `mov ecx,<player>; call` 的 thiscall 形状；
 *   ② 它做的事：`+0x4779c` 无位 8 时播音效 2（miss 音）、建死亡特效 VM（script 0x1d）、
 *   **`+0x476ac = 4`**（`0x45d4e4`）、决死窗口计时器清零、`num_deathbomb_frames = 8`（`0x45d557`）、
 *   最后循卡链表广播虚表 `+0x10`（`0x45d570`–`0x45d57c`）。
 *   ③ 「能不能从帧点调」的**实证**（不是推断）：它自己就被 `Player__on_tick__body` 的 state-4 分支
 *   邻域调用（`0x45cd27` 在同一个 tick 例程家族里），而**卡的回调里调「玩家侧大事」有现成先例** ——
 *   `CardEirin__on_player_death_after_deathbomb` `0x40a4f0` 里 `call 0x45d590`（`0x40a53f`）。
 *   ⇒ 结论：**它只看 `ecx` 一个指针**，与调用它的线程无关；从 `on_item_frame` 调**结构上安全**
 *     （本卡的用法见 `on_death_after_deathbomb`：补死时**只在 `state == 1`** 时调）。
 *   ⚠ 可信度：调用约定与副作用 = **✅ 一手**；「从帧点调不会有别的时序副作用」= **🟡 推断**
 *     （依据是「同族调用 + 卡回调先例」，实跑首日要按下面那条实跑清单核）。*/
#define TRANCE_FN_CANCEL_IMPENDING_DEATH 0x45d590u   /* Player__cancel_impending_death（thiscall(Player*)）*/
#define TRANCE_FN_PLAYER_DIE             0x45d3a0u   /* Player__die（thiscall(Player*)；广播 +0x10）*/

/* ★ 2026-09-13（原生卡冲突审计 A 通道）：救援层的**原生**邻居 + 判据（一手出处逐条见
 * `trance_native_rescue_would_fire()` 的头注）。
 *   0x420420 = `Bomb__can_bomb_and_deathbomb_check`（原生 23 在 `th18:0x40a52b` 就是这么调的）：
 *              无参、返回 int、**纯读**（`CURRENT_BOMBS > 0` && 炸弹管理器在 && 不在放炸弹中 &&
 *              GUI/自机状态允许）。engine.h 已经把它当「do_bomb 的守卫」记过（`CE_ADDR_BOMB_MGR` 那一行）。*/
#define TRANCE_FN_CAN_BOMB               0x420420u
#define TRANCE_NATIVE_AUTOBOMB           23          /* Eirin：花炸弹救，看 acc，且在有 35 + 金≥200 时让位 */
#define TRANCE_NATIVE_ROKUMON            35          /* ShikiEiki：金 ≥ 200 扣钱救，**不看 acc** */
#define TRANCE_NATIVE_ROKUMON_COST       200         /* 一手 `th18:0x40da10` 的 `cmp [0x4ccd34],0xc8` */

/* ---- 音效（★ 2026-09-13 第四轮：**按 wav 名字逐枚对齐 TH13 ↔ TH18**）----
 *
 * 一手（两台 exe 直读 + 反汇编；全套表与可复现命令见 `AUDIT.md` **§CP**）：
 *   **两作的音效库同源**：`th13.dat` 59 枚 / `th18.dat` 71 枚 `se_*.wav`，**58 枚同名文件逐字节相同**
 *   （md5 全等），只有 `se_astralup.wav` 是 TH13 独有。⇒ **按 wav 名字对齐是可靠的**。
 *   为此先把两边的 id → wav 表都直读出来（TH13 的映射此前本仓没有）：
 *     · TH13：cfg 表 `th13:0x4bad94`（71 行 × 0x14，`+0x00` = **名表下标**）+ 名表 `th13:0x4bb320`（60 项）
 *       ⇒ `id → wav = 名表[ cfg13[id] . +0x00 ]`（行号就是 id：loader `th13:0x460090` 起 slot/行 同步推进）。
 *     · TH18：cfg 表 `th18:0x4c9b80`（84 行 × 0x14，`+0x00` = **槽号=id 的置换**、`+0x04` = 名表下标）
 *       + 名表 `th18:0x4b47a0`（72 项）⇒ `id → wav = 名表[ 行(+0x00 == id) . +0x04 ]`。
 *
 * 六条语义的落点（TH13 一手 → 同枚 wav 在 TH18 的 id）：
 *   · **收集** `th13:0x438b38 MOV EDI,0x3f` → `0x3f` = `se_lgodsget` → **TH18 `0x3f`**（同号同字节）。
 *   · **出生** `th13:0x438ef0 ADD EDI,0x3b`（**只在 `spawn_spirit` 的 play_sound 参数 ≠ 0 时播**）
 *     → `0x3b..0x3e` = `se_lgods1..4` → **TH18 `0x3b..0x3e`**（同号同字节）。
 *   · **灵界进入** `th13:0x40586d MOV EDI,0x34` → `se_boon01` → **TH18 `0x34`**（同名重复行还有 `0x35`）。
 *   · **跨格（槽升一格）** `th13:0x43d574 MOV EDX,0x44`（走无参版 `th13:0x460bc0`，**不带声像**）
 *     → `0x44` = **`se_astralup`** —— ⚠ **TH18 里没有这枚 wav**（TH13 独有；滑窗相关 < 0.1，无波形替身）
 *     ⇒ 这是全卡**唯一**必须标 ❓ 的替身。
 *   · **灵界结束**：`th13:0x405b80` 的**非死亡分支里一个 play_sound 都没有**（手动结束 = **静音**）；
 *     死亡触发的那条在 `th13:0x405bf8 MOV EDI,0x42` 播 `se_pldead01`（然后才 `Player::die` `th13:0x405c3e`）。
 *     ⇒ TH18 `0x42` 同号同字节，且**只在死亡触发那条路上播**（见 `on_item_frame` ⑤）。
 *   · **死亡触发的进入**（= 我们的救援）：TH13 **不播音** —— 那一帧玩家听到的"呜"是 `Player::on_hit`
 *     `th13:0x444ca0` 自己播的 id 2（`se_enep00`）；TH18 侧同位置的 `Player__die` `th18:0x45d3a0`
 *     也已在 `0x45d3b7` 播过它自己的 id 2（`se_pldead00`）⇒ **我们不再补音**（旧 `TRANCE_SE_FORCED` 已删）。
 *
 * ⚠ **旧注释（`AUDIT` §BS-1 那一版）把 TH18 的 `0x40` 当成 `se_lgodsget` —— 那是错的**：
 *   `0x40` 在 TH18 是 **`se_msl`**（导弹音），`se_lgodsget` 是 **`0x3f`**。这正是实跑听到
 *   「拾取音不对」的原因（用户实跑反馈 2026-09-13）。根因是 `engine/_shared/th18-sound-table.md`
 *   顶部一条 2026-09-11 的错订正（"`+0x00` 才是 wav 名表下标"）——它**在数据上不可能成立**：
 *   TH18 那张表 84 行的 `+0x00` 是 **0..83 的置换**，而名表只有 72 项，置换不可能是名表下标；
 *   机器码侧也反过来：loader 循环 1（`th18:0x476450`）扫出 `行(+0x00 == k)` 写进 `slot[k].+8`，
 *   而 `0x4776f0` 取 blob 用的是 `[slot.+8 + 4]`（`th18:0x477752`/`0x477788`）⇒ 见 `AUDIT` §CP-2/§CP-3。*/
#define TRANCE_SE_PICK      0x3f   /* ✅ `se_lgodsget`（TH13 `0x3f` / TH18 `0x3f`，同名同字节）*/
#define TRANCE_SE_SPAWN_BASE 0x3b  /* ✅ `se_lgods1..4`（type 0..3 → `0x3b..0x3e`；两作同号同字节）*/
#define TRANCE_SE_ENTER     0x34   /* ✅ `se_boon01`（TH13 灵界进入 `0x34` / TH18 `0x34`，同名同字节）*/
#define TRANCE_SE_LEVELUP   CE_VOICE_ASTRALUP  /* ✅ 复刻（方案②，用户 2026-09-16n 定案）：一手 `th13:0x43d574 MOV EDX,0x44`
                                              *   = **`se_astralup.wav`**（TH13 独有的那一枚：两作同名 58/59 枚逐字节相同，
                                              *   唯一多出来的就是它）。现在走本仓**已有的音效表扩容**通道
                                              *   （`AUDIT` §Q：id `0x54..0x73`）：新 id **0x57**，所以不需要自己写播放器。
                                              *   声源 = TH13 原版 wav，构建期从 `local/` 现取（不入库）；
                                              *   ⌀ 旧版的 `0x0d`（`se_powerup`）是 ❓自定替身，已废。*/
#define TRANCE_SE_END       0x42   /* ✅ `se_pldead01`（两作同号同字节）—— **只给死亡触发的那条结束路**用*/

/* ==================== ★★ 击破掉灵**几颗**（TH13 通道 B 的衰减，2026-09-13）====================
 *
 * 一手（TH13，`engine/spirit/th13/01-spirit-and-trance.md` §3.2，`th13:0x419f41`）：
 *
 * ```c
 * max = enemy->+0x3ff4;   // spec1(N) ⇒ 计时窗口 T（帧）
 * div = enemy->+0x3ff8;   // spec2(N) ⇒ 上限颗数 N
 * if (enemy_timer <= max) {
 *     n = (max - enemy_timer) / (max / div) + 1 ;
 *     if (DIFFICULTY == 2 || DIFFICULTY == 3) n += 1 ;      // Hard / Lunatic 多 1
 * }
 * ```
 * ⇒ **杀得越快掉得越多**：瞬杀 = `N+1` 颗、拖满窗口 = 1 颗、H/L 各 +1。
 * ★ 公式本体在 `trance_core.c` 的 `trance_kill_spirit_count()`（**有主机单测**：第 12b 节，
 *   期望值直接取 TH13 `thecl -d 13` 全量普查里的真实敌人）。**本文件只做两件事**：
 *   ① 把引擎面读成它的入参；② 夹上界并把这几个数写进日志（实跑要对账的正是这一段）。
 *
 * ## 入参：**全是敌人自己的字段**（TH13 的 spec1/spec2 在 TH18 的同形对应物）
 *
 * | 入参 | TH18 读点 | 一手 |
 * | --- | --- | --- |
 * | `T` | `enemy + CE_ENEMY_DROP + CE_ENEMY_DROP_TIMED_TIME`（`+0xa4`）| ECL **opcode 574** → `0x433a70` → `FUN_0042d040` 写它并把计时器复位 |
 * | `N` | `max( enemy + CE_ENEMY_DROP + CE_ENEMY_DROP_TIMED_COUNTS[i] )`，i = 0..19（`+0x54`）| ECL **opcode 573** → `0x433a4a` → `FUN_0042d020` 写 `drop[type*4+0x50]` |
 * | `alive` | `enemy + CE_ENEMY_DATA + CE_ED_TIME_ALIVE`（`zEnemyData.time_alive`）| 敌人每帧入口 `0x42ff80` 尾部 `Timer::increment`；**与 116 用的是同一个计时器** |
 *
 * 为什么这就是 spec1/spec2 的对应物（一手，`engine/season/th18/01-season-residuals.md` §1b′）：
 * TH18 的**原生消费式**是 `n = ex_drop[i] + timed_ex_drop[i] × (T − timer)/T`（`0x430510`）
 * ⇒ **出场 T 帧内击破就多掉、拖久了只剩基础量**，方向与 TH13 通道 B、TH16 `dropSeason` 一致。
 * ⇒ 卡侧读敌人自己的字段即可，**不解析 ECL、不自造数字**。
 *
 * ⚠ **实测分布（本会话重跑 `thecl -d 18`，22 个 `*.ecl` / 749 个子程序）**：
 *   只有 **64 个**子程序配了 (573, 574)。`T` 分布 = 40×4 / 60×2 / 70×5 / 80×3 / 90×7 / **120×21** /
 *   150×2 / 200×4 / 300×8 / 360×3 / 400×2 / 600×2 / 900×1；计数 1..20（type 2「金钱」60 处、type 1「P 点」4 处）；
 *   **所有 `*bs`（boss）与多数小怪一个都没有** ⇒ 「没配窗口」是常态 ⇒ 兜底档是**必需的**。
 * --------------------------------------------------------------------------------------- */
/* ★★ 2026-09-23 重做（owner 2026-09-19 实跑：「神灵掉的**相对**数量偏多」）——
 *   兜底档从写死的 `(T,N) = (120,5)（瞬杀 6 颗）`换成**按敌人自己的 HP 上限分两档查表**：
 *
 * ```c
 * hp = enemy + CE_ENEMY_DATA + TRANCE_ENEMY_LIFE_MAX;   // zEnemyData.life.maximum
 * if (hp >= TRANCE_KILL_FB_HP_BIG) { T = 300; N = 10; } // 大妖精档（TH18 原生 ≥1000 档的众数）
 * else                             { T = 120; N =  1; } // 小妖精档（TH18 原生 <1000 三档的众数）
 * ```
 *
 * **为什么**（完整普查与三条一手锚写在 `trance_core.h` 的 `TRANCE_KILL_FB_*` 注释里，那里是唯一口径源）：
 *   旧兜底 6 颗 > **同一张卡**里配了窗口的敌人的 2 颗（TH18 原生 `N` 众数 = 1）⇒ 观感上
 *   「没配窗口的敌人反而掉得多」= owner 说的「**相对**数量偏多」。新档位把 `N` 钉在
 *   **TH18 原生窗口在同一个 HP 档里的众数**上（<1000 ⇒ 1；≥1000 ⇒ 10）⇒ 兜底与原生档**同档同量**，
 *   TH13 侧同向（小妖精 `N` = 1..4、`GirlC/GirlD` = 10..40）。
 *   实测生效面：216 个未配窗口的创建点里 178 个 HP<500（→ 瞬杀 2 颗）、500–999 档 **0** 个、
 *    ≥1000 的 38 个里 35 个是 `Ecl_EtBreak*`（相位辅助对象，本卡两条过滤本来就不收）⇒ 真正走大档的只有
 *   3 个 `GirlD01`（HP 5000/7000）。
 * ⇒ **映射（按 HP 分档 + 抄原生众数）本身是 ❓ 自定**（TH13 没有这条规则），档位线 `1000` 与两组
 *   `(T,N)` 全是普查值。实跑要更密/更稀**改 `trance_core.h` 的四个档位常数**（`TUNING.md` §2.0c）。
 * ⚠ **与原生档的语义差仍在**（不是这次引入的）：TH18 的 `timed_ex_drop` 是"**额外**多掉几颗"（加数），
 *   TH13 的 `spec2` 是"**上限颗数**"（论证与取法见 `AUDIT` §CK-4）。*/
/* 每帧最多往日志里写几条「这次击破的 (T,N,alive,hp) → n」（诊断环，文件级 static，不占 `ce_state`）。
 * 4 条够对账（一帧通常 0~2 次击破），多出来的只进计数。*/
#define TRANCE_KILL_DIAG_MAX 4

typedef struct {
    int32_t T, N, alive, hp, n;   /* 这一次击破的入参与结果（`hp` = 敌人 HP 上限；走原生档时也记，便于对账）*/
    int32_t flag;             /* bit0 = 走兜底（敌人没配原生窗口）；bit1 = 被上界夹过；bit2 = 队列满丢掉了 */
} trance_kill_diag_t;

static trance_kill_diag_t s_kd[TRANCE_KILL_DIAG_MAX];
static int32_t s_kd_count;              /* 本帧记了几条（≤ TRANCE_KILL_DIAG_MAX）*/
static int32_t s_kd_total;              /* 本帧一共几次击破（含没记进环的）*/
static int32_t s_stat_kill_native;      /* 走原生档位的击破次数 */
static int32_t s_stat_kill_fallback;    /* 走兜底 (T,N) 的击破次数 */
static int32_t s_stat_kill_clamped;     /* 被 TRANCE_KILL_SPIRIT_MAX 夹过的击破次数（★ 夹了要看得见）*/
static int32_t s_stat_kill_maxn;        /* 见过的单次最大颗数（诊断：兜底/原生档位量级对不对比它最快）*/

/* ==================== ★★ boss **相位击破**的灵批次（2026-09-13 第二轮）====================
 *
 * **用户口径**：「击破 boss 的**符卡**或者**非符**应当也会掉**大量**灵」。
 * 与上面通道 B（逐敌衰减）**并列的第二条掉灵通道**：通道 B 管"小怪被打死掉几颗"，
 * 这一段管"**boss 相位被击破时掉一大批**"。
 *
 * ## 一手（TH13）：批次长什么样、什么时候掉
 *
 * 每个相位结束处都有一段被 `unless ($TIMEOUT == 0) goto skip;` 包住的批次（st01bs `Boss2` 原文）：
 *   `dropExtra(10,1) + dropExtra(12,1) + dropExtra(13,20)` ⇒ **22 颗灵**（type0×1 / type2×1 / type3×20）
 *   ＋ P×20、金钱×10（普通道具）。item 10..14 → 神灵 type 0..4（`ItemManager__spawn_item` `th13:0x42f020`）。
 * ⇒ **击破才掉、超时一颗不掉**（`$TIMEOUT != 0` 时整段跳过）；**stage boss 的最后一个相位不掉灵**
 *   （`BossDead` 只有 P/金钱）；中 boss 死亡掉 9~25 颗。完整普查表在 `trance_core.h` 的那一段。
 *
 * ## TH18 落点：**引擎自己的那次掉落事件**（`on_enemy_drop_pre`）
 *
 * TH18 的 boss ECL 在每个相位结束处调 `default.ecl` 的 `@BossItem(main, power, money)` /
 * `@BossItemPhase(power, money, main, area)`（本机重跑 `thecl -d 18`：**44 处可达调用**），
 * 模板体里**同样**有 `unless ($TIMEOUT == 0) goto end;` ⇒ 与 TH13 逐字同款：
 * **超时 → 不调 `dropItems()` → 引擎不消费掉落表 → 本钩子根本不会被调到**。
 * ⇒ 所以我们**不需要**自己判「击破 vs 超时」，也不需要判「符卡 vs 非符」：
 *   **跟着 TH18 自己的相位掉落走**就是逐字复刻（一手审计：`engine/ecl/th18/02-item-drops.md` §3.4/§4.2）。
 *
 * ★★ **2026-09-16i 订正（用户口径「最终符卡击破不应该有爆灵」）**：上面那条偏差**已修**（`AUDIT` §DJ）——
 *   TH13 的 stage boss 最后一个相位（`BossDead`）**一颗灵都不掉**（全 7 关普查 = 0），
 *   而 TH18 的 `BossDead` 自己会调一次 `@BossItem`（逐关：st01 `(16,10,10)` / st02 `(17,20,20)` /
 *   st03 `(16,30,30)` / st04 `(16,80,30)` / st05 `(0,80,20)` / st06 `(0,0,60)`，都在死亡子程序 `+60` 帧处）。
 *   现在按**掉落构成**判掉：整批没有卡道具（`0x10..0x13` 全 0）或卡道具是 `0x10` EXTEND2 ⇒ 不掉
 *   （st01/03/04/05/06 的终符都命中）。🟡 残留：st02 的终符用 `0x11`（与它自己的普通相位同型）⇒
 *   构成上看不出来，只能靠"`+60` 帧 > 符卡门 3 帧"挡（按 ECL 结构那条路本来也到不了钩子）。
 *
 * ## 尺寸与队列
 *
 * 尺寸 = **TH18 自己那批道具的件数**（`counts[]` 求和，普查范围 20..110）× 一手标定的比例
 * （2 件 = 1 颗，TH13 实测 1.4~2.7）⇒ `trance_boss_phase_spirit_count()`（core，有单测）。
 * 构成 = 一手 TH13 的形状（1×type0 + 1×type2 + 其余 type3）。
 * ⚠ 批次**可能大于池的余量**（一件最大 48 颗 + 池 128）⇒ **排队**、每帧喂 `BURST_PER_FRAME` 颗
 *   （照 116 的 `burst_left` / `SEASON_BURST_PER_FRAME` 同一个模式）；**池满就停在那一帧、余量留着**
 *   （不丢批、也不空转）。回调里**只入队**（不建 VM）——与击破那条链同一条纪律。*/
/* ★★ 2026-09-14 符卡门（用户口径 #3）——一手地址与判据见 `trance_core.h` 的"符卡 vs 非符"段。
 *   全局指针 `th18:0x4cf2c0` **指向 zSpellcard 对象本体**（`spellEnd` 就是这么解引用的），
 *   `+0x78` 的 bit0 = 符卡进行中。采样放在 `on_item_frame`（每帧一次），事件在敌人 tick 里 ⇒
 *   留 `TRANCE_SPELL_SEEN_FRAMES` 帧容差把"谁先谁后"两个方向都覆盖掉。*/
#define TRANCE_SPELLCARD_PTR         0x4cf2c0u
#define TRANCE_SPELLCARD_FLAGS_OFF   0x78
#define TRANCE_SPELLCARD_FLAG_ACTIVE 0x1u
#define TRANCE_SPELL_SEEN_FRAMES     3
/* 道中 boss 判据：`dropArea` 写进 zEnemyDrop `+0xbc / +0xc0`（`th18:0x42d090 movss [ecx+0xbc],xmm1`；
 *   ecx = zEnemyDrop = enemy+0x623c、即 `on_enemy_drop_pre` 的 `counts` 基址 −0x04）。
 *   一手：`BossItem` 恒 `dropArea(64.0f,64.0f)`、`BossItemPhase`（道中 boss）恒传 `48.0f`
 *   ⇒ 用 **56.0f** 当分界（两边各留 8 的余量，将来出现别的半径也不会静默串档）。*/
#define TRANCE_ENEMY_DROP_AREA       0xbc
/* 诊断 + **击破兜底档**（`on_enemy_death`）用：`zEnemyData.life` 的 current / maximum
 * （`zEnemyData+0x4ff4` + `{current, maximum, …}` ⇒ 绝对 `enemy+0x6220 / +0x6224`；见 `engine/enemy` 的字段图）。
 * ★ 2026-09-23 一手复核这两个偏移（兜底档按 HP 分档必须先确认"哪个才是 max"）：
 *   出生拷贝 `th18:0x42d818/0x42d821/0x42d82a` 把 ECL `enmCreate` 的 `life` 实参**一次写三格**
 *   （`+0x6220` cur / `+0x6224` **max** / `+0x6228` remaining），构造函数 `0x42e468/0x42e472/0x42e47c` 三格一起清 0
 *   ⇒ `+0x6224` 全 exe 只有这两处写点 ⇒ 击破那一刻读到的是出生值 ✅
 *   ⚠ `engine.h` 的 `CE_ENEMY_HP_MAX` 曾经写成 `0x6230`（= `zEnemyData+0x5004`，符卡扣血那段当除数基准的**别**字段）
 *   —— 已订正为 `0x6224`（`AUDIT.md` §BE 详录 #71）；本文件这两个宏与它同值，别让它们漂开。
 * 打它的用途：下一次实跑就能看出「相位序言那一次 life 已经是新相位的血（>0）」还是「还是 0」——
 * 前者可以拿它当"终符 BossDead 不该掉灵"的判据（TH13 的 `BossDead` 一颗灵都不掉），
 * 后者说明 `lifeSet` 只写 next_attack 那一格、这条判据要用别的字段（见 `AUDIT` 的本轮登记）。*/
#define TRANCE_ENEMY_LIFE_CUR        0x4ff4
#define TRANCE_ENEMY_LIFE_MAX        0x4ff8
#define TRANCE_MIDBOSS_AREA_CUT      56.0f
/* ★★ 2026-09-16i（用户口径「最终符卡击破不应该有爆灵」+ 一手核实，见 `AUDIT` §DJ）：
 *   一手 TH13 的灵批次写在**"符卡刚结束"那个相位的序言**里；**终符 `BossDead` 一颗灵都不掉**
 *   （全 7 关普查 = 0，只有 P/金钱）。TH18 的 `BossDead` **也调一次 `@BossItem(...)`**
 *   （逐关核过：st01 `(16,10,10)`／st02 `(17,20,20)`／st03 `(16,30,30)`／st04 `(16,80,30)`／
 *     st05 `(0,80,20)`／st06 `(0,0,60)`，都在死亡子程序的 `+60` 帧处）⇒ "跟着引擎的掉落事件走"
 *   必然多一批。这里按**掉落构成**判掉（TH18 的一手数据，不是启发式）：
 *     ① 整批**没有卡道具**（`counts[]` 里 item type `0x10..0x13` 全 0）⇒ 一手这一类相位本就不接口灵
 *        （TH13 的灵来自 `dropExtra(10..14)`；st05/st06 的终符 `main = 0` 正是这种）⇒ 不掉；
 *     ② 卡道具是 **`0x10` = EXTEND2** ⇒ 一手普查里 EXTEND2 在 **stage boss 身上只出现在终符**
 *        （st01/03/04；其余 EXTEND2 站点是大妖精与道中 boss，不走这条门）⇒ 不掉；
 *     ③ 其余（`0x11` BOMB2 / `0x12` / `0x13`）⇒ 正常批次。
 *   🟡 **已知残留**：st02 的终符用的是 `0x11`（与它自己的普通相位同型）⇒ 构成上看不出来，
 *      只能靠"`+60` 帧 > 符卡门 3 帧"挡（按 ECL 结构那条路本来就到不了钩子）。要绝对判死需要读
 *      ECL 的相位名（`BossDead`）——本仓还没有那条一手路径（§DJ 待办）。
 *   ⚠ 只对 **stage boss 档**（`dropArea >= 56`）生效：道中 boss / 大妖精那两路不进这条门。*/
#define TRANCE_CARD_ITEM_LO          0x10      /* 卡道具 type 下界（EXTEND2；`engine/ecl/th18/02-item-drops.md` §2）*/
#define TRANCE_CARD_ITEM_HI          0x13      /* 卡道具 type 上界（0x13 = card id 6）*/
#define TRANCE_CARD_ITEM_EXTEND2     0x10      /* 残机卡：stage boss 身上只有终符掉它（一手普查）*/
static int32_t s_spell_active_seen;   /* >0 = 前几帧内见过"符卡进行中" */
static int32_t s_stat_phase_nospell;  /* 被符卡门挡掉的掉落事件数（用户口径 #3 的对账计数）*/
static int32_t s_stat_phase_final;    /* 诊断：被判为"终符不该掉灵"而挡掉的相位批次（AUDIT §DJ）*/
#define TRANCE_PHASE_DROP_ENABLE     1   /* 闪退二分用的总开关（置 0 = 本通道整段不跑）*/
#define TRANCE_PHASE_QUEUE_MAX       4   /* 同时在排队的「相位批次」事件数（一帧最多几次相位结束）*/
#define TRANCE_PHASE_BURST_PER_FRAME 8   /* 每帧最多喂几颗（衬 116 的 SEASON_BURST_PER_FRAME = 8）*/

static float   s_phase_qx[TRANCE_PHASE_QUEUE_MAX];   /* 各待喂批次的出生点（boss 世界坐标快照）*/
static float   s_phase_qy[TRANCE_PHASE_QUEUE_MAX];
static int32_t s_phase_qn[TRANCE_PHASE_QUEUE_MAX];   /* 该批次**还剩**几颗没喂出去 */
static int32_t s_phase_qt[TRANCE_PHASE_QUEUE_MAX];   /* 该批次的**总颗数**（选 type 用：index = total - left）*/
static int32_t s_phase_q_head;                       /* 环头（消费位置）*/
static int32_t s_phase_q_count;                      /* 环里有几个批次 */
static int32_t s_stat_phase_events;                  /* 诊断：相位批次事件数（击破 boss 相位的次数）*/
static int32_t s_stat_phase_spirits;                 /* 诊断：这些批次一共喂出去几颗灵 */
static int32_t s_stat_phase_full;                    /* 诊断：环满而**被丢掉**的批次事件数（正常恒 0）*/

/* ==================== ★ 神灵实体（自建 VM 池）：载体脚本 / 贴图 / 层 ====================
 *
 * ★ 2026-09-13：**载体已经是原版切图 + 原版脚本**（上一版"借 script129 + 魂贴图 + 染色"整段删除）。
 *   一手（`engine/spirit/th13/01-spirit-and-trance.md` §2.3，`th13:0x43907e`）：
 *   **引擎按 `script = type*6 + 1`（普通态）/ `type*6 + 4`（灵界态）选脚本** ⇒ 四型各一组父 + 两个子。
 *
 * | 件 | 在 mod 里是谁 | 一手出处 |
 * | --- | --- | --- |
 * | 贴图 | `assets/ability/trance/SPIRIT_*_CELL_C{0..3}R{0,1}.png`（30×30 → 居中进 32×32）| TH13 `astral.anm` entry1 `enemy/littlegods.png`（10 个 30×30，矩形全带 1px 边距）|
 * | 脚本 | `assets/ability/scripts/179–184`（type 0 手写）+ `233–250`（type 1–3，生成）| TH13 `astral.anm` script1–24 逐条抄 |
 * | 切图器 | `assets/ability/make_trance_art.py` | 原生切图（`local/th13.v1.00c/anm/astral/`，构建期取）|
 * | 脚本生成器 | `assets/ability/make_trance_spirit_scripts.py`（表驱动，含"冻结脚本对账"）| 同上 |
 *
 * **普通态**：四型**共用**一对贴图（`SPIRIT_PLAIN_0/1`），靠子脚本的 `color()` 分四色（一手值见下）；
 *           父 `-1: alpha(0); scale(1.5,1.5)` → `+200: scaleTime(200,0,0.4,0.4)` → `+240: delete()`
 *           ⇒ 用户口径 ③「不断变小而消失」**是脚本自己做的**，卡侧不写 scale。
 * **灵界态**：各型各一对贴图（白环 sprite `3+type`、glow sprite `7+type`），**不染色、不缩不淡**；
 *           父只有 `layer(13); scriptNew×2; stop();` ⇒ 收尾必须靠卡侧 `ce_anm_delete`（那组没有 `interruptLabel`）。
 * ⇒ 进 / 出灵界要**整批重建 VM**（换脚本不能靠改字段），见 `trance_spirit_restyle()`。
 *
 * ⚠ **层**：一手普通态 `layer(6)`、灵界态 `layer(13)` —— 两个都属**世界列表**（TH18 世界层 3–19
 *   走相机 3 的游玩区），**照抄没改**。与 `HUD-LAYOUT.md` §4.1 的「HUD 一律 UI 层 24/25」不冲突：
 *   那条规则管的是 **UI 列表的 HUD**；神灵是场上实体，放进 UI 列表（相机 2）会整体偏位。
 *
 * ❓ **仍然自定**（实跑首日校准，记在 `TUNING.md`）：池容量 128（一手 TH13 是 256）、
 *   收点半径 24 px、原地 micro-bob ±1 px / 32 帧方波。**贴图与脚本已不是自定**。
 * ---------------------------------------------------------------------------------------------- */
#define TRANCE_SPIRIT_LAYER       13     /* 世界列表·道具层（faith.c 的 FAITH_LAYER 同值）。
                                          * ⚠ 脚本自己还会 `layer(6)`（普通态）/ `layer(13)`（灵界态），
                                          *   这一项只是**出生那一刻**的初值。*/

/* ★ 2026-09-13：载体换成**原版切图 + 逐型脚本**（上一版的「借 script129 + 魂贴图 + 染色」已删）。
 * 一手（`engine/spirit/th13/01-spirit-and-trance.md` §2.3，`th13:0x43907e`）：
 *   引擎按 **`script = type*6 + 1`（普通态）/ `type*6 + 4`（灵界态）** 选脚本 ⇒ 四型各一组父+两子。
 *   脚本与贴图在 `assets/ability/`（`make_trance_art.py` 切图、`make_trance_spirit_scripts.py` 出版本）。
 * ⚠ 本卡**不再自己写 sprite / color / scale**：那三样都在脚本里（普通态四型靠 `color()` 分、
 *   灵界态各型各一对贴图；缩小是父脚本的 `scaleTime(200, 0, 0.4, 0.4)`）。
 *   卡侧每帧只写**位置**（`ce_anm_set_pos`）—— 写 scale 反而会被脚本的插值器盖掉（`sdk.h` 的注释）。*/
/* 当前机体号（TH18：0 灵梦 / 1 魔理沙 / 2 咲夜 / 3 早苗）。整张特技表都按它索引。*/
static int32_t trance_character(void)
{
    return *(volatile int32_t *)(uintptr_t)CE_ADDR_CHARACTER;
}

static int spirit_script_for(int32_t type, int in_trance)
{
    if (in_trance) {
        switch (type) {
        case 0:  return CE_ANM_ABILITY_SCRIPT_SPIRIT_TRANCE;
        case 1:  return CE_ANM_ABILITY_SCRIPT_SPIRIT_TRANCE1;
        case 2:  return CE_ANM_ABILITY_SCRIPT_SPIRIT_TRANCE2;
        default: return CE_ANM_ABILITY_SCRIPT_SPIRIT_TRANCE3;
        }
    }
    switch (type) {
    case 0:  return CE_ANM_ABILITY_SCRIPT_SPIRIT_PLAIN;
    case 1:  return CE_ANM_ABILITY_SCRIPT_SPIRIT_PLAIN1;
    case 2:  return CE_ANM_ABILITY_SCRIPT_SPIRIT_PLAIN2;
    default: return CE_ANM_ABILITY_SCRIPT_SPIRIT_PLAIN3;
    }
}

/* ---- 四类神灵的**一手配色**（TH13 `astral.anm` 的 `color()` 值，`SPIRIT` §2.3 表）----
 * ★ 2026-09-13 起这些值**不再由卡写**：它们已经写进了普通态的子脚本
 *   （`assets/ability/make_trance_spirit_scripts.py` 的表 = 唯一真源；生成器会与冻结的 type 0 手写脚本对账）。
 *   留在这里是为了「一眼看到一手值 + 单测/对账时不用翻两个文件」：
 *     外环 0 绿(0,127,0) / 1 橄榄(63,63,0) / 2 紫(63,0,63) / 3 灰(63,63,63)、blendMode 2；
 *     内芯 0 品红(255,32,255) / 1 蓝(80,80,255) / 2 绿(32,255,32) / 3 灰(128,128,128)、blendMode 1。
 *   ⚠ 灵界态（`type*6+4`）**不染色** —— 贴图自带白环 + 彩色 glow（一手原文无 `color()`）。*/
#define TRANCE_TINT_TYPE0   0xffff20ffu   /* 一手：type 0 内芯（生成器表里那一行的同一个值）*/
#define TRANCE_TINT_TYPE1   0xff5050ffu   /* 一手：type 1 内芯（击破通道用的就是它）*/
#define TRANCE_TINT_TYPE2   0xff20ff20u   /* 一手：type 2 内芯 */
#define TRANCE_TINT_TYPE3   0xff808080u   /* 一手：type 3 内芯 */

/* ---- ★ 对话判据（用户口径 ④「对话强制退出灵界」；一手 `engine/spirit/th13/04-dialogue-and-gauge-refund.md`）----
 * 一手（TH13）：`Gui::start_dialogue` `th13:0x428820` 的**尾部** `th13:0x4288f9 call 0x405b80`
 *   = `TranceManager::end` —— **只在对话开始那一帧退一次**，全 exe 没有「对话期间每帧检查再退」。
 * 一手（返还公式，`th13:0x405cd7`–`0x405ce9`）：**固定扣 200（= 一整格 = 3.33 s）**、
 *   夹下界 0 ⇒ `槽_退出后 = max(0, T − 200)`（不是用户感觉的"等比"）；
 *   死亡触发那一档（`this+0x14` bit1）**完全不扣**，但当场补死。
 *   ⇒ 这两档 `trance_end()` 已经分别实现了（`re_die` / 不扣 200），本卡只需**在正确的时刻调它**。
 * TH18 侧判据（一手，同篇 §4 判据 A）：`GUI_PTR`(0x4cf2e0) `-> +0x1b0` 非 0 = 活动 `GuiMsgVm*`。
 *   ⚠ 别用 `Gui+0x1b4`（那是本关 .msg 入口表、整关常驻 ⇒ 会把整关都判成"对话中"）。*/
#define TRANCE_GUI_MSG_OFF 0x1b0

static int trance_dialogue_active(void)
{
    uint8_t *gui = *(uint8_t **)(uintptr_t)CE_ADDR_GUI_PTR;
    return gui && *(void **)(gui + TRANCE_GUI_MSG_OFF) != NULL;
}

/* ---- 「中弹无伤」：**已换成一手语义**（2026-09-13 第四轮结案；旧近似留痕见下面的常量）----
 * 一手（`th13:0x444ca6`：`Player::on_hit` 第一行读 `GAME_FLAGS & 0x100` 直接 return）。
 * TH18 侧的对应点**找到了**（2026-09-13 一手复核）：`th18:0x45d3a0` —— th-re-data 把它命名成
 * `Player__die` 是**误称**，按内容它就是 `Player::on_hit` 本体（音效 `0x45d3b4` / 特效 `0x45d3f7` /
 * 状态→4 `0x45d4e4` / 无敌 6 `0x45d50d` / 决死窗口 8 `0x45d557` 全在里面；扣命与 miss 在更下游的
 * `0x45d090`，只能从状态 4 走到）⇒ 现有断点 `BP_ce_player_die` 的闸门**已经与一手同槽位**。
 * ⇒ 灵界进行中**只声明闸门**（下面 ④-2 的 `ce_no_hit_declare`），**不再声明无敌帧**；
 *   于是观感 = 一手：撞上弹会被吃掉、但不记 miss / 不播音 / 不出特效 / 不开决死窗口、也不扣命。*/
#define TRANCE_INVULN_DECLARE 30   /* ⛔ **已停用**（2026-09-13 第四轮）：灵界中改走一手闸门
                                     *    `BP_ce_player_die`（= TH13 `Player::on_hit` 的 return）。
                                     *    保留常量只为留痕；真要回到"无敌"那套就把 tick 里那个 0 改回它。*/

/* ★ **主动灵界结束后的无敌**：**70 帧**（≈1.17 s）—— ★ 2026-09-13 第五轮**从 wiki 的 60 帧改成一手值**。
 * 一手（`engine/spirit/th13/05-trance-attack.md` §4）：`TranceManager::end` `th13:0x405d29 push 0x46`
 *   → `th13:0x405d37 call 0x405440`。★ **`0x405440` 不是音效函数，就是 `Player::set_invulnerability(frames)`**
 *   （把 `player+0x14684` 那个 zTimer 设成 prev=值−1 / cur=值）⇒ 0x46 = 70 帧。
 *   ⚠ `engine/spirit/th13/01-spirit-and-trance.md` §6.2 旧文把它记成「音效 `0x46`」，**已订正**。
 * ⚠ **只有"主动"（手动开的那档）给**：被动（中弹救援）那档结束时走的是**死亡流程**，
 *   引擎自己会给 miss 无敌（一手 `Player__die` 里那段），我们**不许再叠一层**去盖它。
 * ⚠ 每次结束**重新计时**（连续两次灵界各给一次）；删卡/局末清零。*/
#define TRANCE_END_INVULN_FRAMES 70

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

#define TRANCE_ITEM_TYPE18_POINT  2u     /* TH18 item type 2 = 金钱（= 本 mod 的「点道具/蓝点」对位）*/
#define TRANCE_PIV_POINT_ITEM     100    /* TH13 每收一颗点道具 ⇒ 最大得点 +100 显示（一手 th13:0x42e8ca）*/

typedef struct {
    trance_state_t core;
    int32_t frame;            /* 本卡的**游玩帧计数**（on_item_frame = 正常游玩帧，见下）*/
    int32_t death_echo;       /* ★ 通道 E：死亡后还剩几帧要撒白灵（`+0x14` 里置 30，帧末每帧 −1）*/
    int32_t form_biased;      /* ★ 灵界形态：我们上次写进本体 VM 的 sprite 号（0 = 没写/已还原）*/
    int32_t form_base;        /* ★ 同上：那一次用的加数（还原时要按它减回去）*/
    int32_t form_on;          /* ★ 诊断：形态当前是不是接着（只用于 ON/OFF 各打一行日志）*/
    int32_t last_gauge;       /* 上一帧的槽值（HUD 只在变了的时候重排）*/
    int32_t dlg_prev;         /* 上一帧「在放对话」吗（用户口径 ④ 要求**边沿触发**，不是每帧退）*/
    int32_t dbg_key_prev;     /* 调试键（空格）上一帧按着没有 —— 边沿检测（照 `cards/animal.c` 同款）*/
    int32_t dmg_decl;         /* 本卡当前往伤害层里声明了多少（边沿判定用；初值 = CE_DMG_UNIT）*/
    int32_t end_invuln;       /* 主动灵界结束后的 1 秒无敌剩余帧数（被动那档不走这里）*/
    int32_t log_frame;
    int32_t stat_rescued;     /* 诊断：中弹救援成功几次（帧点里取消死亡 + 开死亡强制窗）*/
    int32_t stat_redie;       /* 诊断：灵界窗口结束「补死」几次（调 Player__die）*/
    int32_t redie_pending;    /* ★ 补死欠账：道具帧只挂账，玩家帧（on_tick_2）才真调 Player__die*/
    int32_t stat_rescue_lost; /* 诊断：在 `+0x0c` 里**没救成**几次（别人救了 / 槽不够）*/
    int32_t stat_levelups;    /* 诊断：跨格次数 */
    int32_t stat_kill_drop;   /* 诊断：击破**次数**（入队→帧末取走的条数；★ 2026-09-13 之前它=颗数，
                               * 因为当时一次击破恒 1 颗 —— 现在颗数看 `stat_kill_spirit` 与
                               * `s_stat_kill_maxn`）*/
    int32_t stat_kill_spirit; /* 诊断：由击破**撒出去**的神灵实体颗数（★ 不等于收到的颗数 ——
                               * 2026-09-12 第二轮：只有走进去吃到的才入槽，没收到的会自然到期消失）*/
    /* ★ 2026-09-12：计分声明**停用**后的本地账（`SPEC §9 #5` 未拍 ⇒ 只记不发，见 trance_declare_score）。
     *   `decl_blocked` = 被挡下的（本会声明出去的）帧数；`decl_last` = 最后一帧本会声明的定点加数
     *   （0 = 那一刻在灵界外 / 没有该声明的量）。拍板后把声明打开时，这两个字段留着当对账用。*/
    int32_t decl_blocked;     /* 诊断：被挡下的声明次数（本卡还在灵界里的每一帧 +1）*/
    int32_t decl_last;        /* 诊断：最后一次写进 ce_score_declare_delta(113, …) 的值（点）*/
    /* ★★ 2026-09-16g（清单第 ② 条落地）：**蓝灵（type 1）给的最大得点加成**的累计值（**点**）。
     *   一手：收一颗 type 1 ⇒ 最大得点 +10；**在灵界里收 ⇒ +100**（常量 `th13:0x4aeea4` / `0x4aeaf0`，
     *   档位由 `trance_reward()` 按出生旗标给，见 `trance_core.c` 的 `max_point`）。
     *   机制：TH13 抬的是**最大得点**（PIV）= 之后每颗点道具都更值钱 ⇒ 我们这边等价地把它当成
     *   「**每颗点道具的加数**」持续声明出去（`ce_score_declare_delta`，落笔 `th18:0x446bf9` 的 `ecx`）。
     *   ⇒ 累计值跨关保留（一手的 PIV 是一局里一直涨的），只在**一局开始**（`on_run_reset`）清零。*/
    int32_t max_point_bonus;  /* 累计的最大得点加成（点；= Σ 收取时 trance_reward().max_point）*/
    /* ★★ 2026-09-16j（用户实跑「目前初始值并非 200」）：进关槽初值的**卡侧自证**。
     *   只靠 `on_stage_start` 不可靠 —— 那个钩子与 `on_run_reset` 都会调 `trance_clear()`，
     *   而它结尾的 `trance_state_init()` 会把整个 core 清零（含槽）⇒ 谁后跑就把 200 冲掉。
     *   ⇒ 这里记「上一次看到的关卡号」+ 一个**进关窗口**：窗口内只要还在空闲相位且槽为 0，
     *   就按一手（进关初始化写 200，`th13:0x42d3c0` 的 `[esi+0x50]`）再写一次。 */
    int32_t last_stage;       /* 上次看到的关卡号（0 = 还没见过）*/
    int32_t stage_init_frames;/* 本关还剩几帧处于「进关窗口」（窗口内自愈式写初值）*/
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
/* ★ 2026-09-13 实跑订正：用户说"计时在屏幕下边缘，不在游玩区中间" —— 那正是我们手摆的 (132,449)。
 *   **一手的位置常量是 `(224, 112)`**（`th13:0x405946`/`0x405957`，经 EBX 传进
 *   `AsciiManager__create_string`）：**x = 224 正好是游戏区（32..416）的水平中心**、y = 112 ≈ 480 的 23%。
 *   ⇒ 改成一手值。⚠ 一手 ASCII 坐标空间的原点没钉死（`engine/spirit/th13/02-…` §1 的 ❓）
 *     ⇒ 这里是同一套 640 空间里的等价值，实跑若还偏只调这两个数。*/
#define TRANCE_HUD_X      224.0f   /* ✅ 一手：游戏区水平中心 */
/* ★ 2026-09-15：**锚点残差**旋钮（640 空间）。用户按 1280 截图逐字比对，给的是"每个字形还需往右多少"；
 *   其中"步进"部分已由 `HUD_FONTS[1]` 的 adv/adv_dot 吃掉，剩下的是**整串**的常量位移
 *   （用户表里最左字形 A = 53 px(1280)，扣掉"收窄后居中左缘自然右移 12"还剩 ≈41 ⇒ 640 空间 ≈20.5）。
 *   等下一张表（照完新构建再量一次：若各字形差值已相等、只是整体还偏）就把那个差值填这里。*/
#define TRANCE_HUD_X_OFFSET  19.5f /* ✅ 实测：用户在 1280 档量到"整体还需往右 39 px" ⇒ 640 空间 39/2。
                                    *    步进部分已由 `HUD_FONTS[1]` 的 adv/adv_dot 吃掉（各段差值已归零），
                                    *    这一个是**整串常量位移**（居中的左缘 + 我们与 TH13 的锚点差）。*/
#define TRANCE_HUD_GAUGE_Y 449.0f  /* （槽已改图形 HUD，这一行不再使用）*/
#define TRANCE_HUD_TIME_Y  112.0f  /* ✅ 一手：`th13:0x4aeb50` 的 y */
#define TRANCE_HUD_COLOR     0xffffffffu   /* ✅ 一手：原版倒计时默认色 = 白 `0xffffffff` */
/* ★ 2026-09-13 **订正**（一手 `engine/spirit/th13/02-trance-hud-and-popup.md` §1）：
 *   原版的"闪烁"是**不透明的红** `0xffff0000`（**不是蓝** —— 旧注释写的"暖色/未抄到"是错的），
 *   而且**没有半透明**（两条色的 alpha 都是 `0xff`，代码里也没有 alpha 动画）；
 *   触发条件有**两条**（`th13:0x405988`–`0x4059a6`）：
 *     · 槽 < 30 且**帧号是奇数**；· 槽 30..89 且**帧号 % 8 == 0**；槽 ≥ 90 不闪。
 *   用户口径 ⑧ 说的"半透明"与一手不符 ⇒ 按一手实现（不透明白/红），差异记在 `TUNING.md` §2.0。*/
#define TRANCE_HUD_COLOR_FLASH 0xffff0000u
/* ★ 2026-09-16 用户口径（咲夜那档的显示）：**按实际剩余时间显示，但封顶 9.99s**，
 *   且**封顶期间在"黄"与"不黄"之间闪烁**（提示"还在走，但显示到头了"）。
 *   ❓自定：黄 = `0xffffff00`（本引擎的颜色是 `0xAARRGGBB` —— 佐证 = 一手的那条红是 `0xffff0000`，
 *   若按 `0xRRGGBBAA` 读会变成黄色，与 `02-trance-hud-and-popup.md` §1 的实测不符）；
 *   闪烁节奏见下面 `TRANCE_HUD_CAP_FLASH_FRAMES` 的注释（2026-09-16d 用户口径）。*/
#define TRANCE_HUD_TIME_CAP_CS   999            /* 显示封顶 = 9.99 s（超过就停在这个数并闪黄）*/

/* ★★ 2026-09-16（用户「不满槽撞死时没爆白灵」）：**通道 E** = 死亡动画期间每帧在自机位置撒一颗白灵。
 *   一手 `th13:0x4434d8`：`spawn_spirit(mgr, type=3, pos=player+0x5b8, speed=[0x4aed90], sound=0)`，
 *   进入条件 = 死亡计时 `[player+0x668] < 0x1e` ⇒ **30 帧**；`[0x4aed90]` 读出来正是 **2.0**。
 *   ⚠ 与剩余槽值**无关**（不按槽数撒）—— 用户看到的"把剩余槽爆出来"是观感。*/
#define TRANCE_DEATH_ECHO_FRAMES 30      /* 一手：`[player+0x668] < 0x1e`（30 帧）*/
#define TRANCE_DEATH_ECHO_TYPE   3       /* 一手：type 3 = 白灵 */
#define TRANCE_DEATH_ECHO_SPEED  2.0f    /* 一手：`th13:0x4aed90` = 2.0f */
#define TRANCE_HUD_COLOR_CAPPED  0xffffff00u    /* 封顶期的"黄"那一半 */
/* 封顶闪烁的**翻转间隔**（帧）= 1 ⇒ **隔帧翻转**（周期 2 帧；60 fps 下 30 Hz 的频闪）。
 *   ★★ 2026-09-16d 用户口径：「咲夜 >9.99s 的闪烁频率换成**这个时间很小的时候**的那个闪烁频率」
 *     ⇒ 就是 TH13 那两条规则里的**第一条**（`trance_time_flash`：槽 < 30 = 剩余 < 0.5 s）：
 *       `gauge & 1` ⇒ **每帧都在红/白之间翻转**。另一条（槽 30..89）是"每 8 帧闪一帧"的中速档
 *       —— 用户先前否掉过 8 帧（"太快了"），这次点名要的是最急促的那一档（本仓 TH12 `ufo.c` 的
 *       `UFO_TOK_INT_BLINK` 也是 2 帧一翻，同族）。
 *   ⚠ **不能用槽值驱动**：封顶时显示值被冻结在 999（奇数）⇒ 照 TH13 用槽值会定死在一个状态；
 *     所以这里用**帧号**驱动同一个节奏（相位与槽值无关，但频率逐帧对齐）。
 *   要调档：1 = 隔帧翻（现在，= 剩余 <0.5 s 档）／8 = 每 8 帧闪一帧（= 剩余 0.5–1.5 s 档）／
 *           15 = 0.5 s 一翻（先前那版"慢速档"）。*/
#define TRANCE_HUD_CAP_FLASH_FRAMES 1

/* 一手的两条闪烁条件（槽值 + 帧号）。`flash` 的判据逐条照抄，不自己发明节律。*/
/* ★ 2026-09-15 订正（对齐 TH13 `th13:0x405983`–`0x4059a4` 的逐条规则）：
 *     槽 < 30  ⇒ 红（当且仅当**槽值为奇数**）
 *     30 ≤ 槽 < 90 ⇒ 红（当且仅当 **槽值 % 8 == 0**）
 *     ≥ 90 ⇒ 白
 *   上一版把第一条写成"帧号奇数"—— 闪的**频率**一样、**相位**不一样（实跑对比就能看出来），
 *   而第二条用的是帧号 ⇒ 同一条规则两个来源，干脆都改回 TH13 的判据。*/
static int trance_time_flash(int32_t gauge, int32_t frame)
{
    (void)frame;                                       /* 判据只用槽值（与 TH13 一致）*/
    if (gauge < 30)  return (gauge & 1) != 0;
    if (gauge < 90)  return (gauge % 8) == 0;
    return 0;
}

/* ==================== ★ 神灵**运动**的一手常量（2026-09-13；`th13:0x438730` 逐条）====================
 * 状态机与半径在 core（`trance_spirit_state_step` / `TRANCE_ATTRACT_*`）；这里只放**积分用的数**，
 * 因为它们是 float，而 core 不许出现 float 形参/返回值（i686 上会引 x87、撞 `make dllx87` 门）。
 * 一手出处：`engine/spirit/th13/03-spirit-motion-and-spawn.md` §1.3（每条都带地址与常量池地址）。*/
#define TRANCE_SPIRIT_ACCEL          0.3f        /* 一手 `0x4ae998` = 0.3（double），被吸时每帧加 */
#define TRANCE_SPIRIT_SPEED_MAX     12.0f        /* 一手 `0x4aecdc` = 12.0（float），被吸速度上限 */
#define TRANCE_SPIRIT_DRIFT_DECAY    0.12666667f /* 一手 `0x4aefc0`（double）：漂移每帧减 */
#define TRANCE_SPIRIT_DRIFT_MIN      0.13f       /* 一手 `0x4aefc8`（float）：漂移速度地板 */
#define TRANCE_SPIRIT_REIMU_ACCEL    0.05f       /* 一手 state 4：`+0.05/帧` */
#define TRANCE_SPIRIT_REIMU_MAX      4.0f        /* 一手 state 4：上限 4.0 */
/* ★★ 自由运动模型开关（2026-09-13 第三轮，用户口径「灵的移动按原版 TH17 做」）：
 *   1 = **TH17 動物霊**那套（定速直线 + 撞框反弹 + 速度下限重掷，一手 `th17:0x410af0`／
 *       `engine/animal/th17/01-animal-spirits.md` §4.2）；0 = TH13 原作那套（每帧衰减到地板 0.13）。
 *   两者都留着可 A/B：改这一个数即可（`TUNING.md` §2.0 记着）。*/
#define TRANCE_SPIRIT_MOTION_TH17      0
/* ★ 反弹框 = **TH13 的游玩区**（一手 `th13:0x438800`：x∈[−192,192]、y∈[0,448]）。
 *   ⚠ 2026-09-13 订正：上一版这里写的是 TH17 的 ±180/128..384，用户更正「th17 是动物灵、
 *     th13 是神灵，不是一个东西」⇒ 已改回 TH13 的框（TH17 那套代码一并删掉了）。*/
#define TRANCE_SPIRIT_BOUNCE_X        192.0f
#define TRANCE_SPIRIT_BOUNCE_Y_LO       0.0f
#define TRANCE_SPIRIT_BOUNCE_Y_HI     448.0f
/* TH17 出场初速 = **单位圆向量**（`th17:0x410570`：`cartesian_from_polar(..., 1.0)`）⇒ 速度 1.0/帧，
 *   而 TH13 是 `0.5 + randf()*3.5`（最快 4 px/帧，看着像"窜出去"）。*/
#define TRANCE_SPIRIT_SPEED_TH17       1.0f
#define TRANCE_SPIRIT_SPAWN_SPEED    0.5f        /* 通道 B（死亡计时 spec1/spec2 ⇒ 击破掉灵）参数 = 0.5（`th13:0x419f8e`）*/
#define TRANCE_SPIRIT_SPAWN_RAND     3.5f        /* 一手 `0x4aef00` = 3.5：初速 = 参数 + randf()*3.5 */
/* ★★ 2026-09-16e（用户「生的灵初始速度太快 / 最终散布比 TH13 大」）—— 把**六条通道各自的参数**
 *   补全（一手 `engine/spirit/th13/01-spirit-and-trance.md` §3 的表，逐条抄）：
 *     通道 A 伤害阈值 `0x419de9` **2.0** ／ B 死亡计时 `0x419f8e` **0.5** ×N ／ C ECL spec0 `0x41f6c1` **2.0**
 *     通道 D item 10..14 `0x42f04b` **0.5** ／ E 玩家死亡 `0x4434f3` **2.0** ／ F 连锁奖励 `0x439250` **0.5**
 *   而**六条都要再叠 `+ randf()*3.5`**（`0x43919a` 在 `spawn_spirit` 里，属于公共尾巴）。
 *   ⚠ 旧实现用「`speed > 0` ⇒ 按最终速度**原样**用」的约定传值 ⇒ 显式给值的两条（E 白灵 2.0、
 *     F 连锁 0.5）**丢掉了随机项**（散布比原版窄），而 boss 那两条（A/C）又在用 B 的 0.5
 *     （初速只有原版的一半）⇒ 现在统一成"**传参 = 通道参数，随机项一律在 spawn 里叠**"。*/
#define TRANCE_SPIRIT_PARAM_BOSS     2.0f        /* 通道 A / C（boss 掉灵两条都走这条）*/
#define TRANCE_SPIRIT_PARAM_DEATH    2.0f        /* 通道 E（= `TRANCE_DEATH_ECHO_SPEED` 同一个一手值）*/

/* ============================ ★ 调试键（2026-09-13，用户要求） ============================
 *
 * 「**空格爆大量灵，以蓝色灵为主，少量白色灵，一个绿色灵和一个红色灵**」（用户原话）。
 * 用途 = 肉眼验收**逐型配色**与两种样式（普通态 / 灵界态）—— 这批美术刚换成原版切图，
 * 而这四种颜色只有当场看才知道对不对。
 *
 * 机制与 117 的调试键**同一套**（`cards/animal.c`）：`ce_key_down(CE_VK_SPACE)` 走
 * `GetKeyboardState`（user32）—— **空格不在游戏键位表里、也不进 replay 输入流**，所以只作调试用。
 * 发布版把 `TRANCE_DEBUG_KEY` 置 0 即可整段消失（`#if` 包着）。
 *
 * ## ★ 颜色 ↔ type 的对账（**我按像素量的**，不是猜的）
 *
 * 一手源图 `local/th13.v1.00c/anm/astral/enemy/littlegods.png`（10 个 30×30）。
 * **灵界态**（= 灵界中用户看到的那一态：白环 + 彩色 glow，贴图自带色、不染色）
 * 的 glow（sprite7..10）最亮 1/4 像素均值实测：
 *
 * | type | 实测 RGB | 读作 |
 * | --- | --- | --- |
 * | 0 | `(255,124,255)` | **品红 / 粉**（用户口径里的「红」）|
 * | 1 | `(113,132,255)` | **蓝** ✓ |
 * | 2 | `(156,248,156)` | **绿** ✓ |
 * | 3 | `(226,255,255)` | **近白 / 青白**（用户口径里的「白」）|
 *
 * **普通态**（四型共用一对白贴图，靠 `color()` 染）内芯一手值是
 * type0 品红 `(255,32,255)` / type1 蓝 `(80,80,255)` / type2 绿 `(32,255,32)` / type3 灰 `(128,128,128)`
 * ⇒ **两个态的对应关系一致**：蓝=1、白=3、绿=2、红（品红）=0。
 * ⚠ 顺带对上了一条既有一手：**击破通道产的正是 type 1**（`TRANCE_KILL_SPIRIT_TYPE`）
 *   ⇒ 平时场上「以蓝灵为主」本来就是这样，不是你记错。
 * ⚠ 用户口径里的「红」我按**品红**处理（源图里没有纯红的神灵）。若你看到的红是别的型，
 *   改 `TRANCE_DEBUG_RED_TYPE` 一个数即可。*/
/* ---- 魔理沙的灵界加成（★ 用户 2026-09-13 给的官方机体说明：「**在灵界攻击力变高且移动速度变快**」）----
 *
 * ★★ **必须挂到既有通道上，不许自己直接写引擎字段**（否则就是新造一个"谁后写谁赢"）：
 *   · **攻击力** ⇒ `ce_set_bullet_damage_permille()`（SDK 的自机弹伤害层）：
 *     该层是**千分比连乘 + id 最小者单一落笔**（`sdk_core.h` 的 `CE_DMG_UNIT` 段），
 *     已有 5 家参与（61 黑桃K / 76 古锭刀 / 78 衔尾蛇 / 108 妖机体 / 116 冬季）⇒
 *     113 只是**第 6 个乘数**，与谁都不互相覆盖，也不受卡的链表顺序影响。
 *     ⚠ 117 咆哮**不在这层**（它走 `on_shot_set_index` 换 shot 组）⇒ 两者正交：
 *     咆哮换出来的兽灵弹照样会被这里的倍率乘上（这正是想要的效果）。
 *   · **移速** ⇒ `player+0x477ec`（`CE_PLAYER_SPEED_MULT`，引擎**每帧复位 1.0**、移动时读）：
 *     **没有声明层**，现行约定是"每帧 `*=` 一次"（先例：`cards/season.c` 的 `SEASON_SPEED_FALL`、
 *     `cards/sj.c` 的 1.1）。用 `*=` ⇒ 乘法可交换 ⇒ **多张卡同装时顺序无关**；
 *     谁要是用 `=` 赋值就会把别人拍掉（这条已写进 `AUDIT` §BW 的检查项）。
 *     ⚠ 必须写在 `on_tick_2`（AbilityManager tick 在 Player tick 之前 —— `engine.h:286` 的一手注）。
 *
 * ❓❓ **两个数值都是自定**：官方说明只写"变高/变快"，没给数字；TH13 侧的一手数值**还没挖到**
 *   （`engine/player/th13/01-character-specials.md` 那条线在做）。⇒ 先给保守值、名字明确、
 *   **实跑由用户 A/B 定**（`TUNING.md` §2.0）。找到一手后只改这两行。*/
/* ---- 灵梦的「**自机判定点较小**」（★ 用户 2026-09-13：TH18 原生**四个机体统一**，得我们自己缩）----
 *
 * 一手（TH13）：判定点半径表 `th13:0x4c0ba8` = **[2.0, 3.0, 3.0, 3.0]**（灵梦 2.0、其余 3.0），
 *   由 `Player__initialize` `th13:0x441f77` 读出来（`FLD [EBX*4 + 0x4c0ba8]`）后写进自机的判定字段
 *   （`+0x620/+0x624` = 半径 × 0.5 ⇒ 灵梦 **1.0** vs 他人 **1.5**）。
 * 一手（TH18）：判定点**四个机体统一** —— `Player__reset` `th18:0x45ae99`–`0x45aed5` 写死
 *   圆半径 `3.0`、AABB 半宽高 `1.5`（六个字段，见 `engine.h` 的 `CE_PLAYER_HITBOX_*` 段）。
 *   ★★ **能改的证明** = 原生卡 `CardAya__on_load` `th18:0x40c7b5` 就写
 *   `[player+0x4799c]=1.0 / [+0x479a4]=[+0x479a8]=0.5` —— 引擎自己的卡就用这几个字段缩判定点。
 *
 * ⇒ 我们把 TH13 的比例（2/3）搬到 TH18 的基准上：**3.0 → 2.0**、**1.5 → 1.0**。
 * ⚠ 与 `CardAya` 只写"非聚焦"那三个不同，我们**六个都写**（聚焦/非聚焦都缩）：
 *   TH13 的判定点不分聚焦，只缩一半会在按住 Shift 时**变回原大小**（那就不是"判定点较小"了）。
 * ⚠ 「判定点指示」（聚焦时那个红点）有自己的绘制尺寸、不吃这些字段 ⇒ **不改**（用户口径）。*/
#define TRANCE_HITBOX_R_NATIVE    3.0f   /* TH18 原生圆半径（`0x40400000`）*/
#define TRANCE_HITBOX_H_NATIVE    1.5f   /* TH18 原生 AABB 半宽/半高（`0x3fc00000`）*/
#define TRANCE_HITBOX_R_SMALL     2.0f   /* 一手 TH13：灵梦 2.0（= 原生 × 2/3）*/
#define TRANCE_HITBOX_H_SMALL     1.0f   /* 一手 TH13：半宽 = 半径 × 0.5 = 1.0 */
/* ★ 判定点「比例相乘」用的状态（2026-09-13 用户拍板方案②；用法见 `on_tick_2` 里那段注释）：
 *   `s_hb_last[i]` = 我们上次写进去的值（用来探测是否有别的卡插了一脚）；
 *   `s_hb_ext[i]`  = 别的卡留下的倍率（探测到就粘住；别人把值改回原生时自动回到 1.0）。*/
static float s_hb_last[6];
static float s_hb_ext[6] = { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f };
static int   s_hb_have[6];
static int   s_hb_log_n;   /* 诊断限流（前 12 次变化各打一行，见 on_tick_2 里那段）*/

/* ★ 2026-09-13（卡 113）：`TRANCE_MARISA_DMG_PERMILLE 1250` **已删除** —— 它是自定值，
 *   且被一手否证为「不该有」（TH13 灵界没有伤害倍率，`th13:0x445c93` 直接取 `row->dmg`）。
 *   灵界主炮组（见下面「灵界主炮组」段）实装后，增伤完全来自那组 row 的 dmg/rate ⇒ 同一份
 *   增伤不能再算一遍。历史与裁决见 `AUDIT` §CJ / §CI-6、`TUNING.md` §2.0。
 *   魔理沙的**移速**保留（下面那条，一手 ×3/2）。*/
#define TRANCE_MARISA_SPEED_MUL    1.5f   /* ✅ 一手：**×3/2（向零取整）**、水平与垂直都乘 —— `th13:0x442bcb cmp [0x4be7b8],1`（只有魔理沙）+ `test ebx,0x100` → `th13:0x442be5`。
                                            *   ★ 2026-09-13 第五轮从 ❓自定 1.15 订正成一手 1.5*/

#define TRANCE_DEBUG_KEY        1     /* 发布版置 0（与 117 的 ANIMAL_DEBUG_KEY 同款开关）*/
/* ★ Shift+空格 = 往槽里灌 **一整格（200）** —— 只为测「满格那一刻」的那套反馈：
 *   音效 / 满格图形提示（脚本 252–254）/ 左下角槽的游标跨格 / 框的亮暗两态 / 以及 C 键能不能进灵界。
 *   满槽 600 要 300 颗神灵，不灌的话测一次太慢。**发布前置 0**（与上面同一个开关管着）。*/
#define TRANCE_DEBUG_GAUGE_STEP 200
#define TRANCE_DEBUG_BLUE_TYPE  1
#define TRANCE_DEBUG_WHITE_TYPE 3
#define TRANCE_DEBUG_GREEN_TYPE 2
#define TRANCE_DEBUG_RED_TYPE   0
#define TRANCE_DEBUG_BLUE_N     32    /* 「大量」*/
#define TRANCE_DEBUG_WHITE_N    6     /* 「少量」*/
#define TRANCE_DEBUG_GREEN_N    1
#define TRANCE_DEBUG_RED_N      1
/* ★★ 2026-09-13 第五轮（用户实跑第 3/4 条「按空格生成的灵似乎被灵梦直接吸收」）：
 *   旧值 96 px + 三圈 ×1.35 的散布 ⇒ 40 颗全落在**灵梦的"任意距离慢吸"**范围内
 *   （`TRANCE_TRAIT_REIMU` 的慢吸不分距离，见 `trance_core.c` 的机体特技表）⇒ 一按空格整批立刻
 *   被吸走，**看不出"撒出去了"这件事**，也看不出收集反馈的节奏（+N 弹窗一颗一闪，40 颗挤在一起）。
 *   撒到 200 px（≈半个游玩区）后：灵会先自己走/反弹一段、再被慢慢吸进来，弹窗一颗一颗看得清。*/
/* ★ 2026-09-14（用户口径：空格撒的灵"被甩到整个屏幕形成一个圆环，不方便我吃测试"）：
 *   200 px 是**第五轮为"看得清撒出去"故意调大的**；现在要的是**方便吃**，所以默认改小到 60 px
 *   （三环 60/81/102 px，都在灵梦"任意距离慢吸"的范围里，按一下就能一颗颗吸进来）。
 *   想看大环 / 检查美术时按 **CTRL+SPACE**（用 `TRANCE_DEBUG_RADIUS_WIDE`）。*/
#define TRANCE_DEBUG_RADIUS     60.0f
#define TRANCE_DEBUG_RADIUS_WIDE 200.0f

static trance_card_t *st(ce_card_t *c) { return ce_state(c, trance_card_t); }

/* ★ 入槽函数（定义在下面「收集」段）：`on_item_frame` 的帧末要用它 ⇒ 先声明。
 * 它就是把「击破 → 神灵实体 → 靠近才有 +2」这条链接上的**最后一环**（见文件头「槽怎么被填」段）。
 * 第 3 参 = 神灵的**世界坐标 x**（收集音的声像用它；没有实体层的年代这里只能读自机 x，`❓ 自定` 已消）。*/
static void trance_on_spirit_collected(ce_card_t *c, int32_t type, float x, float y,
                                        int32_t born_trance);
static void trance_hud_tick(const trance_card_t *s);   /* 左下角指示槽（定义在 on_draw 前）*/
static void trance_player_die(void);                     /* 补死（定义在救援段；on_tick_2 要用）*/

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

/* 计分声明 —— ★★ **2026-09-16g：已接线**（清单第 ② 条，用户口径「灵界中吃蓝灵是 +100」）。
 *
 * 历史：2026-09-12 停用（当时 `ce_score_aggregate()` 是所有卡之和，110/111 把它当"信仰总量"用
 * ⇒ 113 一在场就把它们的 PV 顶高 10000）。**2026-09-13 计分层重做后那个机制已经不存在**：
 * 加数按**族**分通道（`ce_score_declare_delta` 不带耦合 / `_coupled` 会被(擦弹倍率+信号强度)缩放），
 * 基础 `G_0` 只在层里出现一次（见 `../../SCORE-LAYER-REWORK.md` §5）。
 * 而 `SPEC §9 #5` 问的"落在哪个载体"也已由既成事实回答：**107（妖妖梦）的「最大得点」走的就是
 * `ce_score_declare_delta()`**（`cards/senro.c:489`）⇒ 113 照同一条路走，不需要新通道、不动共享件。
 *
 * 量纲（**点**）：一手收一颗 type 1 灵 ⇒ 最大得点 **+10**，**在灵界里收 ⇒ +100**
 * （常量 `th13:0x4aeea4` / `0x4aeaf0`；档位由 `trance_reward()` 按出生旗标给，见 `trance_core.c`）。
 * TH13 抬的是 PIV（之后每颗点道具都更值钱）⇒ 这里等价地声明成"**每颗点道具的加数**"，
 * 由层在站点 `th18:0x446bf9` 写 `ecx = 1000 × G`（`G = G_0 + Σ未耦合 + …`）。
 * ⚠ 不要写 `ce_score_declare_coupled`（那一族会被擦弹倍率/信号强度缩放；113 这份不在耦合项里），
 *   更不许写 `ce_score_mul_declare`（那是信号强度千分点）。
 * ⚠ **粘性声明**：删卡 / 局末必须显式声明 0（见 `trance_clear()`），否则层会一直替我们加。*/
static void trance_declare_score(trance_card_t *s)
{
    int32_t decl;

    if (!s) return;
    decl = s->max_point_bonus;                 /* 累计加成（点）；0 = 不参与（层不写一个字）*/
    ce_score_declare_delta(TRANCE_NUM, decl);
    if (decl != s->decl_last) {                /* 只在值变了的时候打一行（限流：诊断用）*/
        s->decl_last = decl;
        if (decl > 0)
            ce_log("trance: 最大得点加成 %d 点/颗（type 1 蓝灵累计：普通 +10 / 灵界内 +100）"
                   "-> ce_score_declare_delta(113, %d)", (int)decl, (int)decl);
    }
    if (decl > 0) ++s->decl_blocked;           /* 诊断字段改义：声明**生效**的帧数 */
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
 * `th13:0x438a61`），那段要 **40 帧**才自己 `delete()`（一手 `astral.anm` script1：`interruptLabel(1)`
 * 之后的 `+40: delete()`，thanm 打印的绝对时间是 240；**不是** 20 帧 —— 2026-09-14 按一手重读订正）。如果那一刻就把 id 从池里丢掉，这 20 帧里
 * 池**不再持有**这个 VM ⇒ `trance_clear()`（**删卡**那条路）就删不掉它 —— 而删卡发生在**商店**里、
 * AbilityManager 冻住、tick 全停 ⇒ 玩家会看见一张卡的残影挂在屏幕上（`METHOD.md` 铁律二要的正是
 * 「不许留下任何要等下一帧才消失的东西」）。
 * ⇒ 消失段期间槽**继续持有 id**（`dying = 1`）：不再写它的外观、不再判定收它，
 *   每帧用 `ce_anm_get_vm()` 看它有没有自删；自删了才回收槽。**池满时它也算占位**（20 帧内必回收）。*/
typedef struct {
    int32_t id;      /* anm VM id；0 = 空槽（脚本自删之后的下一次 tick 就会走到这一步）*/
    float   x, y;    /* ★ **当前**坐标（世界坐标）—— 会动：漂移 / 被吸（2026-09-13 起）*/
    float   dx, dy;  /* 单位方向：漂移态保持它；被吸时每帧重算成"指向自机" */
    float   speed;   /* 标量速度（一手：被吸 `+0.3/帧` 上限 `12.0`、漂移 `−0.12666667/帧` 地板 `0.13`）*/
    int32_t state;   /* core 的 `TRANCE_STATE_*`（1 漂移 / 2 被吸 / 4 灵梦专属慢吸）*/
    int32_t size_age;/* **大小**的年龄（与 `age` 分开！）—— 见 `trance_spirit_scale_now()` 的注释：
                      * 一手的大小由引擎按计时器写，且**进/出灵界时全场恢复到最大**（THBWiki 口径 #19）
                      * ⇒ 它必须能被单独重置，不能拿 `age`（那是寿命的年龄）兼职。*/
    int32_t life;    /* 剩余生命（帧）；≤ 0 = 到点消失（`trance_spirit_expired`）*/
    int32_t age;     /* 已活帧数（缩小曲线 / 微动相位）*/
    int32_t type;    /* 神灵 type（0..3）；染色与奖励都按它 */
    int32_t dying;   /* 1 = 已在播消失段（interrupt 1）：不再收 / 不再写外观，等脚本自删后回收槽 */
    int32_t style;   /* 0 = 普通态 VM（脚本 type*6+1）、1 = **灵界态** VM（type*6+4）。
                      * 一手：灵界态是**另一组脚本**（各型各一对贴图、不缩不淡）⇒ 开关灵界时
                      * 整颗 VM 要**重建**（换脚本不能靠改字段），见 `trance_spirit_restyle()`。*/
    int32_t born_trance; /* ★★ **出生时**在不在灵界 = 一手 `spirit+0x84` bit0（`th13:0x438fde`）。
                      * 一手用它决定**两件事**，而且**只看出生那一刻**、之后永不改：
                      *   ① 奖励档（type0 命碎 +1/+2、type1 点 +10/+100、type2 符卡碎 +1/+2）；
                      *   ② 槽：**普通态出生 +2、灵界态出生 0**（`th13:0x438b45` 的
                      *      `FUN_0043d530` 只在"非灵界态"那条路上有；灵界态那条路
                      *      `th13:0x438d09` 只 `FUN_0043d330`/`FUN_0042f510`/`…(…,2)` 直接给奖励）。
                      * ⚠ 与 `style` **分开存**：`style` 表示"现在长什么样"（用户口径 ⑥ 要在开关灵界时
                      *   整批换样式），而奖励/槽按出生旗标走 —— 两者在"普通态出生、灵界里收到"时才分叉，
                      *   那正是原版的行为（收的是普通档奖励 + 加槽），旧版用相位代替 ⇒ 这一档少给 2 点槽。*/
    uint8_t probe;   /* ★ 诊断（2026-09-14）：1 = 这颗被选为"寿命取证"对象，前 6 帧逐帧打 age/scale
                      *   日志（用户口径 #5 要求"每帧 age 增量可核对"）。**不参与任何玩法判定**。*/
} trance_spirit_t;

static trance_spirit_t s_spirit[TRANCE_SPIRIT_POOL];
static int             s_n_spirit;      /* 池里活着的颗数（0..TRANCE_SPIRIT_POOL，紧凑数组 + swap-remove）*/
static int             s_spirit_style;  /* 池里这批 VM 是哪一态（0 普通 / 1 灵界）；换态那一帧整批重建 */
static int32_t         s_spirit_spawn_index;  /* 出生序号（一手 `SpiritItemManager+0x8820` 的等价物）：
                                               * 一手用它选抛出象限（`spawn_index % 4`）⇒ 见 `trance_spirit_spawn()`。*/
static int32_t         s_spirit_life = TRANCE_SPIRIT_LIFE_FRAMES;   /* 本机体的神灵寿命
                                         * （一手 480；咲夜 960）。每帧由特技表刷新 —— 放在文件级
                                         * 是因为 `trance_spirit_spawn()` 拿不到卡状态（与池同口径）。*/
static int32_t         s_bg_vm;         /* 灵界背景 VM 的 anm id（0 = 没起）。文件级 static —— 与神灵池同口径，
                                         * 因为 `ce_state` 只有 496 字节，而它必须能被 `trance_clear()` 收掉 */

/* ★ **灵界背景**（用户 2026-09-13 口径 ⑧ 的后半：「整体背景也会变」）。
 * 一手（`local/th13.v1.00c/anm/astral/astral.anm.txt` script0 + `th13:0x4032d0`）：
 *   贴图 = `effect/eff_astral.png`（512×512，**已原样切进 `ability.anm`**，脚本 251）；
 *   参数 = `angleVel(0,0,0.6°/帧)` / `scale(1.15)` / `blendMode(7)`
 *   （mode 7 = `DESTALPHA/INVDESTALPHA` 混合，表见 `engine/anm/th15/01-blend-modes.md`）。
 * ⚠★ **为什么默认关**（不是没做，是**不许把近似包装成复刻**）：
 *   原版这张图**不是**按 anm sprite 画的 —— 它由 TH13 的 **EffectManager effect 回调**画成
 *   **自定义 D3D 全屏四边形**（`th13:0x4032d0` 自己设一组 render state + `DrawPrimitiveUP` 画 4 个顶点，
 *   并额外再画 4 个 astral VM），而 script0 里的 `pos/scale/alpha` 在运行期**被那个回调覆盖**
 *   （`th13:0x403656` case 7/8/9 把四个 VM 的 scale 写成 320/240，还带一个 26/20/6 帧的状态机
 *    —— 这正是"一手写着 `alpha(1)` 却仍然看得见"的原因）。
 *   ⇒ TH18 侧没有那个回调，我们只能拿**普通 VM** 近似；而 `blendMode(7)` 取的是**目标帧缓冲的 alpha**
 *     （不是我们 VM 的 alpha）⇒ 观感**静态推不出来**，只能实跑看一眼。
 *   ⇒ 置 1 打开；打开后要调只改 `assets/ability/scripts/251_trance_astral.anm.txt` 里的 alpha（旋钮见 `TUNING.md` §2.0）。*/
/* ---- ★ 攒满一格的**图形提示**（THBWiki 口径 #7：「每攒满一个灵体都会有音效提示和图形提示」）----
 *
 * 一手（`th13:0x43d530`）：`old/200 != new/200` 时起 **`front.anm` script = `new/200 + 0x6b`**
 *   = **108 / 109 / 110**（满 1 格 / 2 格 / 3 格 = MAX），脚本逐条相同、只有 `sprite` 不同；
 *   位置 = **自机位置**、脚本自己上浮 16 px、寿命 100 帧。
 * 落地：贴图（`front00.png` sprite 78/79/80）已切进 `ability.anm`（`TRANCE_TIERUP_*`），
 *   脚本 **252/253/254** 逐条照抄（只把 `layer(11)` 改成 13，两者都是世界层）。
 * ⚠ 这三支 VM **自己 delete**（100 帧），不进我们的池；但**删卡那一刻**必须由 `trance_clear()`
 *   把在飞的那一支收掉（`METHOD.md` 铁律二：商店里 tick 全停，等不到它自己删）。*/
/* ==================== ★★ 左下角**指示槽**（THBWiki 口径 #4「左下灵界槽」；2026-09-13 用户点名）====================
 *
 * 一手（`local/th13.v1.00c/anm/front/front.anm.txt` script171 起的 5 支 + `th13:0x42b030` 每帧写）：
 *   根 script171 `layer(17); pos(-196, 448)`（640×480 空间 = **左下角**）起 4 个子：
 *     · 172 **框**（`front/agauge1.png` sprite121 **亮** / sprite122 **暗**）——
 *       引擎在 **槽 < 200** 时发 `中断 3` 换到暗的那张；
 *     · 173 **总条**：`scale.x = uv_scale.x = (槽/200)/3`；
 *     · 174 **第二条**：引擎不写（只靠中断 4/5 淡入淡出）；
 *     · 175 **游标**：`scale.x = uv_scale.x = (槽%200)/200`、`entity_pos.x = (槽/200) × 16`
 *       —— ★ 2026-09-13 第六轮订正：旧写「比例 × 16」（比例 = 格内比例）是**误读**，
 *       一手是**满格数**（x87 栈收支 + 几何自洽两条判据，见 `TRANCE_HUD_MARK_SLIDE` 的注释）。
 *   淡出：自机 `x < -64 && y > 384`（640 空间）⇒ `中断 5`（alpha→32）；恢复 ⇒ `中断 4`（→255）。
 *
 * ## 我们这版为什么**逐个起 4 支**、而不是起根脚本让 `scriptNew` 带子
 *
 * 引擎是**按 id 直接写子 VM 的 `scale` / `entity_pos`** 的；而卡侧只拿得到**自己起的那个 id**，
 * 拿不到 `scriptNew` 出来的子 VM（SDK 没有"枚举子 VM"的接口）。
 * ⇒ 把 4 支**叶子脚本**（175/176/177/178，都不 `scriptNew`）各自 `ce_anm_spawn_ui()` 起一份，
 *   把引擎写子的那几个值改成**卡侧写它自己起的那一支** —— 逐字段一一对应，不新增任何引擎接口。
 * ⚠ 因此**不要**再用根脚本 174（`CE_ANM_ABILITY_SCRIPT_TRANCE_GAUGE`）：那是"父 + 4 子"的版本，
 *   两套一起用会在屏幕上画两份。它保留在 `ability.anm` 里作为**原版结构的记录**。
 *
 * ## 坐标（全是**表面 px**：脚本里 `resolutionMode(2)`，= 640 空间 × 2）
 *
 * 4 支的出生位置都给**同一个基点**，各自脚本里的 `pos()` 就是一手给子的相对偏移
 * （175 是 `(0,8)`、其余 `(48,0)`）⇒ 合成即一手布局 ✓（引擎的绘制位置 = `pos + entity_pos`，
 * 一手 `th18:0x48171f`：`[EDI+0x30] + [EDI+0x5f0] + [EDI+0x4e4]`）。
 *
 * ★★ **2026-09-13 第三轮订正（实跑第 2 条「看不见左下角指示槽」的根因）**：
 *   旧值 `(-392, 896)` 是把一手根的 `pos(-196,448)` **当成"以场地中心为原点"**直接 ×2 —— 错了。
 *   一手的根坐标是**场地坐标**（TH13 的 gauge 叶子在 `layer(17)` = 游玩区层），而**表面 px 的原点
 *   是屏幕左上角**，两者之间还差一个**场地原点 (640, 32)**（一手换算：表面 = `640 + x×2, 32 + y×2`，
 *   `th18:0x4817d8` 一带的 `DAT_0056aca0` + `DAT_0056acb8`）。用户的 1280 截图实测把这条钉死了：
 *   **游玩区左边缘 = 257、底边 = 928**（`cards/animal.c` 的 117 号卡注释，2026-09-10 量的）
 *   ⇐ 与 `640 − 192×2 = 256`、`32 + 448×2 = 928` 逐像素吻合 ✅。
 *   ⇒ 正确基点 = `(640 + (−196)×2, 32 + 448×2)` = **(248, 928)**。旧值的 x = −392 在**屏幕左边之外**
 *     （x < 0）⇒ 4 支 VM 全在屏幕外 ⇒ 用户**一支都看不到** ✅ 根因确认。
 *
 * 游标那支每帧被卡侧改写 `pos`（**相对**基点的偏移，别把基点再加一遍！）。*/
#define TRANCE_HUD_BASE_X   248.0f     /* = 640 + 一手根 x(−196)×2 —— 场地原点 640，见上 */
#define TRANCE_HUD_BASE_Y   928.0f     /* = 32 + 一手根 y(448)×2 —— 场地原点 32，见上 */
/* ★★ 2026-09-13 第六轮订正（用户实跑：「扭曲程度减小了，但还是在左右动」）——**游标为什么会左右动**
 *
 * 一手原文（`thanm -l 13 local/th13.v1.00c/dat/front.anm` script175 的 `0:` 段）与引擎的每帧写
 * （`Gui__sub_42b030_spirit_related` `th13:0x42b030` 的 `0x42b1b7`–`0x42b1e2`）逐条对下来是：
 *
 *     槽   = [0x4be808]（0..600）
 *     满格 = 槽/200（`0x42b144 f7ef imul` + `0x42b146 c1fa06 sar edx,6` + `0x42b149` 符号修正 = /200）
 *     格内 = 槽 − 满格×200（`0x42b1b7 imul esi,esi,0xc8` + `0x42b1bd sub ecx,esi`）
 *     scale.x = uv_scale.x = 格内/200.0（`0x42b1ca fdiv [0x4aece8]` → `0x42b1d6`/`0x42b1d9`）
 *     entity_pos.x = ??? × 16.0（`0x42b1dc fmul [0x4ae8e0]` → `0x42b1e2 fstp [eax+0x574]`；
 *                    `+0x574` = `zAnmVmSuffix.entity_pos.x`，一手：th-re-data `zAnmVmSuffix` 0x44
 *                    + `zAnmVm` 的 suffix 基址 0x530）
 *
 * ★ 那个 `???` 不是"格内比例"（旧注释与 `engine/spirit/th13/01` §8 都这么写）—— 是 **满格**。
 *   两条独立判据：
 *   ① **x87 栈收支**：`0x42b167 fld 格内比例`… 一路数下来，`0x42b1dc` 乘的那个值是**上一段（173 总条）
 *      留在 ST 上的 `(float)(槽/200)`**：`fld [ebp-4]`(推) → `0x42b170 fdivr st(1)`(商落 ST0、被除数
 *      留在 ST1) → `0x42b172 fstp`(弹 ST0) ⇒ ST0 剩下的就是 `(float)(槽/200)`；173 段结尾
 *      `0x42b17b fstp [ecx+0x78]` 只弹掉商，于是它一路留到 `0x42b1dc fmul`，在函数出口
 *      `0x42b1e2 fstp` 弹空 —— **栈收支恰好为零**（不是笔误：漏一个值的话每帧漏一层 x87 栈，
 *      几帧就溢出；这也是"商的方向"的旁证：另一读法 3/(槽/200) 在槽<200 时除零 = 巨大条）。
 *   ② **几何自洽**：`entity_pos.x = 满格×16` ⇒ 游标左缘 = 基点 + `24 + 16×满格`；
 *      而总条（173）右缘 = `24 + 48×(槽/600)` = `24 + 16×满格 + 16×格内`；游标宽度 = `16×格内`
 *      ⇒ **游标右缘 ≡ 总条右缘**（逐格逐帧对齐）✅。反过来按"格内比例×16"算，游标会比总条
 *      多伸 `16×格内`、且在格内一边变长一边右移 —— 那正是用户看到的「左右动」。
 *   ⇒ 结论：**游标 = 当前格的填充段：起点贴当前格左缘（按整格跳 16 px / 此处 32 表面 px），
 *     宽度 = 格内比例，不随进度平移。**
 *   ⚠ 这条同时订正 `engine/spirit/th13/01-spirit-and-trance.md` §8 的同一格（该文档已就地加注）。*/
#define TRANCE_HUD_MARK_SLIDE 32.0f    /* 一手：游标**每满一格**跳 `16.0`（640 空间）×2 = 表面 px */
/* ★ 尺寸 ×2（2026-09-13 第三轮）：这 4 支挂在 **UI 层**（`layer(24)`），而 UI 层的坐标/尺寸空间是
 *   **整屏表面 px（1280×960）**；TH18 自己的 UI 贴图**整体是 TH13 的 2 倍**（一手对账：TH18
 *   `front/lifebar.png` 的 sprite 是 8×32 / 4×32 / 6×12，TH13 同族是 4×16 / 2×16 / 3×6，**逐项 2 倍**），
 *   因为 TH13 的 UI 贴图是按 640 宽的屏画的。⇒ 把 TH13 的 `agauge1/2` 按 1:1 贴进 TH18 的 UI 层
 *   只有原版**一半**的屏占 ⇒ 尺寸统一 ×2（本仓先例：`cards/mof.c`/`cards/signal.c` 的
 *   `U640_TO_SURF = 2.0f` 也是把 640 空间的量乘到表面 px）。
 *   ⚠ 只需 ×2 的**尺寸**：位置已经在上面按"表面 px"算过了，别再乘第二遍。*/
#define TRANCE_HUD_ART_SCALE 2.0f
/* ★★ 2026-09-15g：压暗的**门**改走跨卡统一件 `cards/hud_dim.{h,c}`（区域按本卡块的几何推）。
 * 性质 = ❓ **用户口径（UI 统一），不是复刻**（用户 2026-09-15：「压暗这个涉及到 UI 统一，
 * 这个不能复刻。是那个靠近透明吧」；口径 = **动态统一**：终点 alpha、起始 255、过渡帧数、滞回带 16 px；
 * **区域按各卡自己那块 HUD 的矩形推**）。★ 2026-09-22 起统一值 = 终点 **64**（25%）/ **8 帧线性** = TH18 原生一手
 * （旧口径 32 / ±56 = 4 帧）—— 本卡的 alpha 归**脚本**（175–178），已同步改成 `alphaTime(20, 0, 128/255)`。
 *
 * ★★ 2026-09-15h：**进/出的口径订正**（父 agent：15g 那版「进 = 矩形**之内**」裁错了）——
 *   **进 = 自机落在「块矩形外扩 `HUD_DIM_MARGIN`(28)」之内**、**出 = 离开「矩形 + 44」（28 + 滞回 16）之外**。
 *   理由：一手各作的门本来就落在它们自己那块上沿之上 24–32 px（TH11 的 `416` = 它那块沿 + 24、TH10 约 32、
 *   TH16 的 `400` 在它那块沿之上 28）⇒ 边距是**原本就有的语义**，不是我们发明的。
 *
 * 本卡**指示槽的块矩形**（表面 px，全部从 `TRANCE_HUD_BASE_*` 与脚本的 `pos/anchor/尺寸` 推）：
 *   · 基点（父 VM 的 entity_pos）= `TRANCE_HUD_BASE_X/Y` = **(248, 928)**
 *   · 外框（脚本 175）：`pos(0, 8)` + `anchor(1,2)`（左·下）+ 32×48 ×`TRANCE_HUD_ART_SCALE`(2)
 *       ⇒ 表面 x 248..312、y 840..936
 *   · 总条（176）/ 底衬（177）：`pos(48, 0)` + 同锚 + 48×32 ×2 ⇒ x 296..392、y 864..928
 *   · 游标（178）：`pos(48, 0)` + 16×32 ×2，且每满一格右移 32（`TRANCE_HUD_MARK_SLIDE`）
 *       ⇒ 静止 x 296..328；满格时右缘最多到 392（本块的右沿仍取**总条**那 392）
 *   ⇒ **块 = 表面 x 248..392、y 840..936**（= 144 × 96）
 *   ⇒ 自机坐标（`自机 = 表面/2 − (320, 16)`，见 `hud_dim.h`）= x ∈ [−196, −124]、y ∈ [404, 452]
 *   ⇒ **进** = 自机落在「矩形 + 28」之内：x ∈ [−224, −96] ∧ y ∈ [376, 480]
 *        （自机钳位 x ∈ [−184, 184]、y ∈ [32, **432**] ⇒ 实际可触发区 x ∈ [−184, −96]、y ∈ [376, 432]）
 *   ⇒ **出** = 离开「矩形 + 44」：x < −240 ∨ x > −80 ∨ y < 360 ∨ y > 496（**16 px 滞回带**）
 *
 * ⚠️ 原来的样子（留档，不再生效）：
 *   `TRANCE_HUD_HIDE_X (−64)` / `TRANCE_HUD_HIDE_Y (384)` —— 一手 TH13 的栅格被**同时**当成了进与出
 *   （所以原来**没有滞回**：进/出同一个值）。本轮核出来：**384 ≈ 本块上沿 404 再往外 20**（⇒ 一手那 384
 *   是它那块上沿减 16 的**淡回沿**，被借来当进门限了）。现在按本卡的块推：**进 = 矩形 + 28（y ≥ 376）**、
 *   淡回 = 出到矩形外扩 44 之外（y < 360）。−64 那个 x 门是「左三分之一」的近似（TH13 的块与我们的块不同宽），
 *   现在按块宽推（可触发区 x ∈ [−184, −96]）。
 *   ⚠ 2026-09-13 第三轮订正（−128 是 ×2 误换算）那条**仍然成立**：`ce_player_pos()` 是场地坐标，
 *     与一手的字面量同量纲。*/
#define TRANCE_HUD_BLOCK_W   144.0f    /* 指示槽的块宽（表面 px）：外框左沿 248 → 总条右沿 392 */
#define TRANCE_HUD_BLOCK_H    96.0f    /* ★★★ 2026-10-03z7（子智能体审计 P0-1，**订正我 z3 那次的误判**）——
                                        *   我按 atlas 里 `TRANCE_GAUGE_TOP`（64×64 ×2 = 128×128）把块高改成了 128，
                                        *   但审计给出反证：**那张贴图全仓没有任何脚本/C 引用**
                                        *   （`assets/ability/make_trance_art.py` 自述「该成员眼下没有任何
                                        *   脚本/C 代码引用（脚本 175 已改用 GAUGE_FRAME_A/B）」；
                                        *   `scripts/175..178` 只用 FRAME_A(32×48) / BAR(48×32) / BAR2 / MARK(16×32)）
                                        *   ⇒ 真实墨迹 y **840..936**（= 外框 32×48 ×2），块高就是 **96**。
                                        *   ⚠ 128 会**多占 32 px**（虚假碰撞：把 108 顶走的正是它）⇒ 改回 96。
                                        *   若将来真把顶层元素画出来，再加高（届时同步 `make_trance_art.py` 的注）。*/
#define TRANCE_HUD_BLOCK_TOP  (TRANCE_HUD_BLOCK_BOTTOM - TRANCE_HUD_BLOCK_H)
#define TRANCE_HUD_BLOCK_BOTTOM (TRANCE_HUD_BASE_Y + 8.0f)   /* 936：外框脚本 `pos.y = 8`（2× 空间）*/
                                                             /* ⇒ 块的**左下角**（`hud_dim_rect_surface` 的入口）*/
#define TRANCE_TIERUP_LIFT 32.0f   /* 自机上方多少 px 起（一手：engine 在自机位置起、脚本再上浮 16）*/
static int32_t s_tierup_vm;         /* 正在飞的那支 VM（0 = 没有）；删卡时收 */
/* 左下角指示槽的 4 支 VM（顺序固定：0 框 / 1 总条 / 2 第二条 / 3 游标；0 = 没起）。
 * ★ 文件级 static（与神灵池、背景、满格提示同口径）：`ce_state` 只有 496 B，且它们必须被
 *   `trance_clear()` 一条路收干净（删卡发生在商店里、tick 全停 —— `METHOD.md` 铁律二）。*/
#define TRANCE_HUD_N 4
static int32_t s_hud_vm[TRANCE_HUD_N];
/* ★★★ 2026-10-03x（goal「所有左下角 HUD 进同一个分配器」第 ② 步；owner 十二合一实跑
 *   「112 117 108 **113** 的槽位全都堆在左下角」）——
 *   本卡原来**完全不认领**（本文件旧注：反复"手动摆两行，等拍板"）⇒ 它在分配器眼里**不存在**：
 *   既不会被判碰撞、也不会让位、更不参与"整条带一起淡"。
 *   ⇒ 现在按 112 的样板接入：**pin 声明值 + (槽 − pin)** —— 内部布局一个像素不变，
 *     整块**跟着分配器走**（撞了就回退货架、和主动卡面冲突也一样让开）。*/
static ce_hud_slot_t s_hud_slot;
static uint8_t s_hud_slot_ok;
/* ★★★ 2026-10-03z（owner 实跑：「113 的 UI 也有奇怪的**分离**问题」）——
 *   面板是 4 支**按需分别 spawn** 的脚本 VM：先建的那批带**旧基点**、后建的那批带**新基点**
 *   ⇒ 分配器一动（认领落定 / 碰撞回退 / 换关重排）整组就**裂开**。
 *   ⇒ 改成**增量式**：`s_hud_applied_*` 记「已经加进所有活 VM 的差值」；
 *     每次只把 **(目标差值 − 已加)** 补到每一支活 VM 上（新 spawn 的当场补上当前差值）⇒ 永远同步。*/
static float s_hud_applied_dx, s_hud_applied_dy;
/* ★★★ 2026-10-03z16（**墨迹探针抓到的硬证据**：113 的贴图实际画在 (388..484, 892..940)，
 *   而声明块在 (470..614, 724..820) —— 两者根本不是同一个地方 ⇒ owner 看到「113 和 117 重合」）。
 *   根因：原来是**增量差值**（只往 `ep` 上加差额），而脚本自己也会改 `pos` ⇒ 一旦不同步就漂。
 *   ⇒ 改成 **112 那种绝对定位**（样板见 `ufo.c` 的 `ufo_hud_place_at()`）：
 *     · VM 出生时记下它的**设计位置** `(sp+p2+ep)`（= 脚本按基点算出来的那个点）；
 *     · 之后每帧 `ep = (设计位置 + 槽位移) − (sp + p2)` ⇒ 有效落点**恰好等于**设计位置 + 槽位移 ✓
 *       （脚本再怎么改 `pos`，我们这一帧都会把它算回去）。*/
static float s_hud_design_x[TRANCE_HUD_N], s_hud_design_y[TRANCE_HUD_N];
static uint8_t s_hud_design_ok[TRANCE_HUD_N];
#define TRANCE_HUD_DX ((s_hud_slot.x) - (TRANCE_HUD_BASE_X))
#define TRANCE_HUD_DY ((s_hud_slot.y) - (TRANCE_HUD_BLOCK_BOTTOM))

/* 认领（幂等；`_pinned_card` 版带**关卡纪元**）——取不到就退回声明锚点兜底（首次认领前那几帧）。*/
static void trance_hud_claim(void)
{
    ce_hud_block_t blk;
    ce_hud_slot_t got;
    if (s_hud_slot_ok && ce_hud_slot_claimed(TRANCE_NUM)) return;   /* 常态：一次比较就回去 */
    memset(&blk, 0, sizeof blk);
    blk.w = TRANCE_HUD_BLOCK_W;
    blk.h = TRANCE_HUD_BLOCK_H;
    blk.side = CE_HUD_SIDE_PINNED;                 /* 非 pinned 会被 pinned 入口拒掉 */
    s_hud_slot_ok = (uint8_t)(ce_hud_slot_claim_pinned_card(TRANCE_NUM, &blk,
                          TRANCE_HUD_BASE_X, TRANCE_HUD_BLOCK_BOTTOM, &got) ? 1 : 0);
    if (s_hud_slot_ok) {
        s_hud_slot = got;
    } else {                                        /* 兜底：声明锚点（SDK 失败时不写 out）*/
        s_hud_slot.x = TRANCE_HUD_BASE_X;
        s_hud_slot.y = TRANCE_HUD_BLOCK_BOTTOM;
        s_hud_slot.w = blk.w; s_hud_slot.h = blk.h;
        s_hud_slot.side = CE_HUD_SIDE_PINNED;
    }
}
static int32_t s_hud_frame_dark;    /* 框当前是不是"暗"那张（边沿换图，别每帧换）*/
static int32_t s_hud_hidden;        /* 当前是不是淡出态（边沿发中断 4/5）*/
static int32_t s_hud_dim;           /* ★ 2026-09-15g：压暗闩锁（共享件 `hud_dim_gate` 的一半；
                                     * alpha 那一半在 anm 脚本里 —— 见 `trance_hud_tick` ⑤）*/
/* ★ 2026-09-13 实跑诊断：用户报「看不见指示槽」+「几秒后闪退」⇒ 必须能从上位日志判断
 *   这 4 支 VM 到底**起没起起来、是不是在被反复重建**（反复重建 = 每帧泄漏 ⇒ 会崩）。
 *   `s_hud_tries`：连起来之前试了几帧；`s_hud_respawn`：**建好之后又被判失效的次数**（>0 就是危险信号）。*/
static int32_t s_hud_tries;
static int32_t s_hud_respawn;
static int32_t s_hud_up_logged;
#define TRANCE_HUD_SPAWN_TRIES 240  /* 兜底重试上限（4 s）；到点还没起来就放弃，别再泄漏 */
static int32_t s_hud_gave_up;       /* 1 = 本关放弃起指示槽（见上面那条上限的注释） */
/* ★ 2026-09-13 第六轮：几何诊断（用户报「还是在左右动」）。60 帧 ≈ 1 s 一行，
 *   只打数值，判据写在 `trance_hud_tick` ⑤ 的注释里。*/
#define TRANCE_HUD_DIAG_PERIOD 60
static int32_t s_hud_diag_frame;

static int trance_hud_script(int i)
{
    switch (i) {
    case 0:  return CE_ANM_ABILITY_SCRIPT_TRANCE_GAUGE_FRAME;
    case 1:  return CE_ANM_ABILITY_SCRIPT_TRANCE_GAUGE_FILL;
    case 2:  return CE_ANM_ABILITY_SCRIPT_TRANCE_GAUGE_FRAME_B;
    default: return CE_ANM_ABILITY_SCRIPT_TRANCE_GAUGE_MARK;
    }
}

/* 从`第 n 格`选脚本（1/2/3 ⇒ 252/253/254；越界夹到 3）。*/
static int trance_tierup_script(int32_t tier)
{
    if (tier <= 1) return CE_ANM_ABILITY_SCRIPT_TRANCE_TIERUP1;
    if (tier == 2) return CE_ANM_ABILITY_SCRIPT_TRANCE_TIERUP2;
    return CE_ANM_ABILITY_SCRIPT_TRANCE_TIERUP_MAX;
}

/* ==================== ★ 吃到神灵的**数字弹窗**（2026-09-13 第三轮；用户实跑第 1+3a 条）====================
 *
 * 一手（`th13:0x438730` `SpiritItem__on_tick` 的收集分支 + 跳转表 `th13:0x438e3c`）：
 *   `FUN_0046f820(&out, script, spirit, 0, -1, 0)` —— 脚本号按 **type**（`spirit[1]`）分派，
 *   每个 type 两条（`spirit+0x84` bit0 = 出生时在不在灵界）：
 *
 * | type | 普通态 | 灵界态 | 我们切的贴图 | 内容 |
 * | --- | --- | --- | --- | --- |
 * | 0 命碎 | 脚本 96 | 97 | `TRANCE_POPUP_LIFE1/2` | 命碎图标 + 1 / 2 |
 * | 1 最大得点 | 102 | 103 | `TRANCE_POPUP_POINT10/100` | 最大得点图标 + 10 / 100 |
 * | 2 符卡碎 | 99 | 100 | `TRANCE_POPUP_SPELL1/2` | 符卡碎图标 + 1 / 2 |
 * | 3 槽/分数 | — | — | 无 | 原版**不起 VM**（走 `0x44ad60` 改分数显示）|
 *
 * 脚本正文逐字节照抄（`assets/ability/scripts/255..260_popup_*.anm.txt`），只改两处：
 *   · `layer(11)` → **13**（TH18 自己那份同形脚本的层）；
 *   · `scale(1.0)` → **2.0**（TH18 同类弹窗的贴图是 TH13 的 2 倍：24×8 → 48×16）——
 *     改的是 scale，**贴图还是 TH13 的原矩形**，没有重画。
 * 位置 = **神灵自己的坐标**（一手第 3 参是 `spirit`）；54 帧后脚本自己 `delete()`。*/
/* VM 上由 `sprite()` 指令写下的**有效贴图矩形**（一手：`AnmLoaded__set_sprite` `th18:0x477b8f`
 * 把 sprite 的宽写进 `vm+0x74`、高写进 `+0x78`；`draw_vm` `th18:0x481703` 再乘 `+0x54` 的 scale.x）。
 * ⇒ 拿它和「我们切图时的 w×h」比，就能判「引擎到底认到了哪张图」（用户实跑反馈的贴图错位问题）。*/
#define TRANCE_VM_SPRITE_W 0x74
#define TRANCE_VM_SPRITE_H 0x78
/* ★★ 2026-09-13 第五轮（用户实跑第 2 条「指示槽的进度本身还是在奇怪地伸缩和移动」）：
 *   进度的正确做法是**裁切贴图**，不是**拉伸贴图** —— 两个字段都要写：
 *     · `zAnmVmPrefix.uv_scale` `+0x6c`（zFloat2 u/v；ExpHP th18 `type-structs-own.json`）；
 *       `uv_scale.u = f` ⇒ 只采样 sprite 自己 UV 矩形左边 f 那一段
 *       （公式与一手证据：`engine/anm/th16/10-sprite-geometry.md` §4 —— `U(TR) = su + U[0] + (U[1]−U[0])·ku`，
 *        **收缩恒以左上角为锚**，与 `anchor` 无关；它是 sprite 矩形内的比例，**与画布大小无关**）；
 *     · `flags_lo` `+0x534` 的 `|= 0x18` —— 原生 HUD 充能条就是这么干的（一手 `th18:0x408a43`
 *       `or dword ptr [esi+0x534], 0x18`，同一处还写 `+0x70` uv_scale.v / `+0x58` scale.y / `+0x80` uv_scroll.v；
 *       `AUDIT.md` §O29k 有整段）。
 *   ⚠️ 只写 `scale.x`（旧写法）⇒ 整张美术被横向压扁/拉长（= 用户看到的"伸缩"），
 *      再加上 178 的 `entity_pos.x` ⇒ 观感就是"一边伸缩一边移动"。TH13 一手的写法是
 *      `scale.x = uv_scale.x = 比例`（本仓一手记录：`engine/spirit/th13/01` §8、`AUDIT.md` §7554 那行）。*/
#define TRANCE_VM_LAYER    0x18    /* int32：VM 的层（ExpHP zAnmVmPrefix.layer；`ce_anm_spawn` 的第三参只在 <0x18 时写进去）*/
#define TRANCE_VM_UV_SCALE 0x6c
#define TRANCE_VM_FLAGS_LO 0x534
#define TRANCE_POPUP_MAX 24            /* 同时在飞的弹窗上限（一颗灵一支；爆量调试键一次 40 颗 ⇒ 会顶掉最老的）*/
#define TRANCE_POPUP_LOG_LIMIT 8       /* 前几支打日志（爆量时别把日志刷满）*/
static int32_t s_popup_vm[TRANCE_POPUP_MAX];
static int32_t s_popup_next;           /* 环形游标（顶掉最老的那支）*/
static int32_t s_popup_overflow;       /* 顶掉过几支（诊断）*/

static int trance_popup_script(int32_t type, int32_t style)
{
    if (type == 4) type = 1;           /* 一手 `th13:0x438ed8`：type 4 被改写成 1（奖励同 type 1）*/
    switch (type) {
    case 0:  return style ? CE_ANM_ABILITY_SCRIPT_POPUP_LIFE2   : CE_ANM_ABILITY_SCRIPT_POPUP_LIFE1;
    case 1:  return style ? CE_ANM_ABILITY_SCRIPT_POPUP_POINT100: CE_ANM_ABILITY_SCRIPT_POPUP_POINT10;
    case 2:  return style ? CE_ANM_ABILITY_SCRIPT_POPUP_SPELL2  : CE_ANM_ABILITY_SCRIPT_POPUP_SPELL1;
    default: return 0;                 /* type 3：一手不起弹窗 */
    }
}

/* 记账：弹窗是"自己会死"的 VM，正常情况不用管；但**删卡那一刻**卡回调全停
 * （商店里的倒吊人 / 原版空白卡），等不到它自己 delete ⇒ 必须有一份名单能一条路收干净
 * （`METHOD.md` 铁律二）。名单满时顶掉最老的那支（不 delete 它，只是不再为它记账）。*/
static void trance_popup_remember(int32_t id)
{
    int i;
    for (i = 0; i < TRANCE_POPUP_MAX; ++i) {
        const int32_t j = (s_popup_next + i) % TRANCE_POPUP_MAX;
        if (!s_popup_vm[j] || !ce_anm_get_vm((uint32_t)s_popup_vm[j])) {
            s_popup_vm[j] = id;
            s_popup_next = (j + 1) % TRANCE_POPUP_MAX;
            return;
        }
    }
    s_popup_vm[s_popup_next] = id;
    s_popup_next = (s_popup_next + 1) % TRANCE_POPUP_MAX;
    ++s_popup_overflow;
}

static void trance_popup_clear_all(void)
{
    int i;
    for (i = 0; i < TRANCE_POPUP_MAX; ++i) {
        if (s_popup_vm[i]) ce_anm_delete((uint32_t)s_popup_vm[i]);
        s_popup_vm[i] = 0;
    }
    s_popup_next = 0;
}

/* ★ 灵界遮罩（**旧版矩形近似**）：色值抄 TH13 `effect.anm` 幕布的 `color(32,16,96)`。
 *   ★★ 2026-09-13 第六轮**关掉**（`TRANCE_MASK_ENABLE 0`）—— 理由见下面那段的注释。*/
/* ★ 灵界 BGM（路线 A）的开关在 `trance_bgm.h`：**现在是 1**（2026-09-13 第二轮：真素材 + 接线重做后打开）。
 *   历史：实跑首日曾置 0 做二分，但日志里 `bgm … started 0` 证明它当时**根本没跑** ⇒ 闪退不是它
 *   （`AUDIT` §CA-5b 第 6 行）。出问题时的回退顺序见 `AUDIT` §CO-7：① 开关改 0；
 *   ② 把 DLL 旁边的 `bgm/` 目录改名（素材缺失 ⇒ 本模块**不接管**，主 BGM 照常）。*/
/* ==================== ★★ 灵界背景 / 转场（2026-09-13 第八轮：改成**目的地 alpha 遮罩**） ====================
 *
 * 一手全貌（`engine/spirit/th13/02-trance-hud-and-popup.md` §2.2–§2.4、§2.6）：
 *   灵界开始 ⇒ `TranceManager::begin` 起 **EffectManager effect 0**（`th13:0x415650`），
 *   它的 on_draw `th13:0x4032d0` **仅当状态字 `+0x1c34 == 2`** 每帧按**这个顺序**做三件事：
 *     ① `0x4032f5`–`0x4034d6`：**一块"只写 alpha"的游玩区四边形** ——
 *        `SetRenderState(0xf←0, 0xce←1, 0x13←ZERO, 0x14←ONE, 0xab←ADD, 0xcf←ONE, 0xd0←ZERO, 0xd1←ADD)`
 *        + `DrawPrimitiveUP` 画 (128,16)-(512,464)（= 384×448 游戏区）、**顶点色 = `0x00000000`**
 *        ⇒ **游戏区 `alpha := 0`、RGB 原样保留**（`0·S + 1·D`；alpha 侧 `1·Sa + 0·Da`，Sa=0）。
 *        ★ 第六轮把它读成"没有半透明遮罩可以复刻"是**读反了**：它正是**遮罩的居零步**。
 *     ② `0x403570` 循环 4 次 `AnmManager__draw_vm` 画**幕布**子 VM（`effect.anm` 3–6 进 / 7–10 出：
 *        `eff_switch.png` 的**笔触 alpha**（RGB 全白）+ `scale(3,12)` + 斜置 + 扫屏）
 *        —— `blendMode` 缺省 = 0 ⇒ `a_out = Sa² + Da(1−Sa)` ⇒ **它们把笔触写进目的地 alpha**；
 *     ③ `0x403591` 画第 5 台子 VM = **`astral.anm` script0**：`blendMode(7)`（DESTALPHA/INVDESTALPHA）
 *        ⇒ `rgb = astral·Da + 后台·(1−Da)` —— **只在 Da ≠ 0 的地方盖住后台**。
 *   而 script3（进场的第 1 台）在 **t=45** 变成一块 `sprite(-1); drawRect(640,480)`（alpha 255）
 *   并 `stop()` ⇒ **整屏 Da := 1** ⇒ astral 全显（它就是"填充幕"，不是"暗幕"）。
 *   ⇒ 合成出来就是用户口径：「**有一个会动的透明度遮罩（转场）罩在了那张 astral 图上**；
 *      进来也是一模一样的反向转场」。
 *
 * ⇒ 本卡的分工（脚本侧逐条复刻，D3D 那一步译成 anm）：
 *   · **目的地 alpha 清零幕** = 脚本 **271**（`blendMode(2)` = `SRCALPHA/ONE/REVSUBTRACT`，
 *     一手 `th18:0x47da20`；配 `color(0,0,0)` + `alpha(255)` ⇒ `rgb := D`、`a := Da−1→0`）
 *     —— **逐语义等价**于 ① 那块 `0x00000000` 四边形（推导见 271 文件头）；
 *   · **常驻背景** = astral（脚本 251，层 3）—— `blendMode(7)`，**恢复严格一手**（第八轮回改）；
 *   · **进入幕布** = 4 台（脚本 266–269 = 一手 `effect.anm` **3/4/5/6**）——灵界开始那一帧起，45 帧自灭；
 *   · **alpha 填充幕** = 脚本 **270**（= 一手 script3 的 t=45 那段）——第 45 帧起把 Da 填满；
 *   · **退出擦除** = 4 台（脚本 262–265 = 一手 `effect.anm` **7/8/9/10**，`end` 发中断 1 时重建的）
 *     ——灵界结束那一帧起 40/45/50/55 帧自灭；**底色先删**（一手：script3 那台 VM 被重建成 script7
 *     ⇒ t=45 的矩形随之消失）⇒ astral 被反向擦掉。
 *   · **旧矩形遮罩（脚本 261）仍然关着**：一手没有"深蓝半透明方罩"这一件东西（`AUDIT` §CB-3）。
 *   ⚠ 副作用声明：现在 astral 能否看见**取决于 Da** ⇒ 清零幕/填充幕哪一支没起，都会立刻表现为
 *     "astral 不显示"；卡的日志把这三支 VM 的 id 都打出来了（见 `on_item_frame`）。
 *   ⚠ 染色：幕布与填充幕取 **TH18 自己那份配方的 `color(64,64,56)`**（`local/th18.v1.00a/anm/effect/
 *     effect.anm.txt` script3..10 —— 与 TH13 script3..10 同形），不是 TH13 的 `color(32,16,96)`
 *     ⇒ 顺带去掉用户看到的蓝紫（`AUDIT` §CN-9）。*/
#define TRANCE_MASK_ENABLE        0
#define TRANCE_MASK_LAYER        19     /* 游玩区最上层（HUD 在 24+；渲染阶段表见 engine/anm/th18/02）*/
#define TRANCE_MASK_RGB          0x00201060u  /* BGR: 96,16,32 —— D3DCOLOR 的低三位是 BGR */
#define TRANCE_MASK_ALPHA        110    /* ❓ 自定：稳态不透明度（0..255）*/
#define TRANCE_MASK_FADE_STEP    6      /* ❓ 自定：每帧步长（≈20 帧淡完）*/
static int32_t s_mask_vm;
static int32_t s_mask_alpha;
/* ★ 灵界常驻背景（astral）：开。层 = 3 = 世界列表「背景组」（3–5；画进 surface 0、由 `0xf` 那次
 *   `arcade_vm` 拷进游玩区 —— `engine/anm/th18/02-render-stages.md` §1）。
 *   ⚠ 与脚本 251 的 `layer(3)` **必须一致**（`ce_anm_spawn` 的第三参只在 <0x18 时写进去）。*/
/* ★★★ 2026-09-13 第十轮（`AUDIT` §CT-8）：**灵界那一叠从层 3 挪到层 5**。
 *   一手：TH18 的 boss 符卡背景（`st0Xenm.anm` script10–14 = `cardbg0/1.png`）**全写 `layer(4)`**
 *   （`thanm -l 18 st01enm.anm` 里 `ins_304(4)`）⇒ 层 4 那趟（优先级 0xb）在层 3（0xa）**之后**
 *   ⇒ 整块盖掉我们的幕布 = 用户口径「灵界幕布被 boss 符卡幕布取代」。
 *   一手相对关系：**TH13 自己的符卡背景是 `layer(3)`**，而 TH13 的 astral 画在宿主层 **6**
 *   （`0x4035e0` case 9）⇒ 原版里 **astral 赢**。TH18 把符卡背景挪到 4 ⇒ 我们挪到 **5** 才保住同一关系
 *   （5 = 背景组 3–5 的顶，仍在敌机/子弹层 6–19 之下；3–5 同相机 3、同 surface 0 ⇒ 几何不变）。*/
#define TRANCE_BG_ENABLE 1
#define TRANCE_BG_LAYER  5
/* ★ 第八轮：astral 脚本里的 `angleVel(0,0,0.010471975)` —— 卡侧**按同一速率记账**，
 *   只为在"退出时重建 astral"（见下落沿那段）之后把相位写回去，别让转场首帧旋转跳变。
 *   ⚠ 改脚本的 `angleVel` 就要一起改这个数。*/
#define TRANCE_BG_ANGLE_VEL 0.010471975f
static float s_bg_angle;            /* astral 自转的累计角度（rad），与 VM 里的 rz 同步 */
/* ★★ 进出幕布（4+4 台；脚本 266–269 = 一手 `effect.anm` 3/4/5/6「进」、
 *   262–265 = 一手 `effect.anm` 7/8/9/10「出」）。
 *   一手宿主层由状态机在 **6 / 20 / 26** 之间切（`0x4035e0` 的 case 7/8/9 = `state 0/1/2`
 *   + `layer 26/20/6`），其中 **转场那两档 20/26 比 TH13 的 HUD（17/19/23/25）还高**
 *   ⇒ 在 TH13 里意思是"盖在最上面"。
 *   ⚠ **本卡不吃这一条**（第八轮）：我们的 astral 必须**读到**幕布写下的目的地 alpha，
 *   而只有"同层且在本机之前"才读得到 ⇒ 进出两批**都取 3**（= astral 那一层）。
 *   层号自定，`TUNING.md` §2.0；一手依据见 `AUDIT` §CN-9-1 ④⑤。*/
#define TRANCE_WIPE_LAYER 5             /* ★ 第十轮：**进出都取 5**（= astral 那一层；3→5 的理由见 `TRANCE_BG_ENABLE` 上面那段）。
                                         *   旧值 19（= 游玩区最上层）在这一轮**必须废掉**：这批幕布是
                                         *   **目的地 alpha 的写手**，而 astral（`blendMode(7)`）只认
                                         *   **同一个 surface 里、且在自己之前**写下的 `Da`——
                                         *   层 19 画在 astral **之后**（`engine/anm/th18/02-render-stages.md` §1
                                         *   的优先级 0x2b vs 0xa）⇒ 遮罩读不到 ⇒ 转场完全失效。
                                         *   同层内前后 = **生成顺序**（尾插）⇒ 卡侧在"下落沿"里
                                         *   **先删 astral、起完幕布后再重起它**（见下面那段注释）。
                                         *   ⚠ 与脚本 262–269 的 `layer(3)` **必须一致**。*/
#define TRANCE_WIPE_IN_LAYER 5          /* 进场幕布/填充幕/清零幕：**与 astral 同层 5，但生成在它之前**
                                         * （一手画序：清 alpha → 4 台幕布 → astral；同层叠放 = 生成顺序
                                         *  ⇒ astral 压住它们 = 原版画序）。
                                         * ⚠ 与脚本 262–269 / 270 / 271 的 `layer(3)` **必须一致**；
                                         *   想让进场幕布也盖在 astral 上（看得见，但会挡住 astral 约 45 帧）
                                         *   ⇒ 把这些脚本改成 `layer(19)` 并把这里改成 19（`TUNING.md` §2.0）。*/
#define TRANCE_WIPE_IN_COVER 1          /* **alpha 填充幕**（脚本 270 = 一手 script3 的 t=45 那段）
                                         * 置 0 = 第 45 帧没人把 Da 填回 1 ⇒ astral 在灵界期间**看不见**
                                         * （除非同层还有 alpha=1 的东西）；**别关**。*/
#define TRANCE_WIPE_N    4
/* 进场幕布的开关：**1**（一手有这一套：`0x402ff0` 初始化时挂 script 3/4/5/6）。
 *   置 0 = 灵界开始时不做"幕布扫进来"（背景直接是 astral），其余不受影响。
 *   ⚠ 与 `TRANCE_ALPHA_MASK` 的联动见下面那段（mask 开着而 enter 关掉 = 前 45 帧 astral 看不见）。*/
#define TRANCE_WIPE_ENTER 1
/* ★★ 2026-09-13 **第八轮**：**目的地 alpha 遮罩**（= TH13 转场的真机制）。
 *   一手 `th13:0x4032d0` 每帧（仅灵界中）先用 `SRCBLEND=ZERO/DESTBLEND=ONE` + 分离 alpha
 *   （`SRCBLENDALPHA=ONE/DESTBLENDALPHA=ZERO`）画一块游戏区四边形（顶点色 `0x00000000`）
 *   ⇒ **游戏区 alpha := 0、RGB 保留**；4 台幕布把 `eff_switch.png` 的笔触 alpha 写进 Da；
 *   `astral.anm` script0 的 **`blendMode(7)`（`rgb = astral·Da + 后台·(1−Da)`）**只在 Da≠0 处盖住后台；
 *   最后 script3 的 t=45 `drawRect(640,480)`（alpha 255）把 Da 填满 ⇒ astral 全显。
 *   ⇒ 用户口径「有一个**会动的透明度遮罩（转场）**罩在那张 astral 图上」= 这一套。
 *   本卡把它译成纯 anm（TH18 的 anm 改不了 0xce 那组分离 alpha 状态）：
 *     · 脚本 **271**（清零幕）：`blendMode(2)` = `SRCALPHA/ONE/REVSUBTRACT`
 *       （一手 `th18:0x47da20`）⇒ `rgb = D − Sa·S`、`a = Da − Sa·Sa`；
 *       配 `color(0,0,0)`（S.rgb=0 ⇒ **rgb := D 保留**）+ `alpha(255)`（Sa=1 ⇒ **a := Da−1 → 0**）
 *       = **逐语义等价于 TH13 那块 `0x00000000` 四边形**（`AUDIT` §CN-9）。
 *     · 脚本 **251** 恢复一手的 `blendMode(7)`。
 *   置 0 = **不起清零幕**（脚本 271 留着）：Da 就只由引擎每帧的 Clear（`0xffffffff`，一手
 *   `th18:0x4553dc`）= 1 决定 ⇒ astral 常显、幕布只是压暗一下 —— 即第六轮那套观感（一键回滚）。
 *   ⚠ 关掉 `TRANCE_WIPE_ENTER` 而留着 mask 会让前 45 帧"Da 一直是 0"（没幕布写 alpha、底色还没亮）
 *   ⇒ astral 那 45 帧看不见；两个开关要一起改。*/
#define TRANCE_ALPHA_MASK 1
static int32_t s_wipe_vm[TRANCE_WIPE_N];    /* 当前在飞的 4 台（进/出共用这 4 个槽）*/
static int32_t s_wipe_cover_vm;             /* alpha 填充幕（脚本 270；0 = 没起）*/
static int32_t s_alpha_zero_vm;             /* ★ 目的地 alpha 清零幕（脚本 271；0 = 没起）*/
static int32_t s_wipe_phase_prev;   /* 上一帧的"在不在灵界里"（0/1）：抓上升沿/下落沿 */
static int32_t s_wipe_bursts;       /* 起过几次进场幕布（诊断，只增）*/
static int32_t s_wipe_bursts_out;   /* 起过几次退出幕布（诊断，只增）*/
/* ★★ 2026-09-13 第九轮：**每帧写回去的 z 旋转**（= 一手 `effect.anm` script3..10 的 `rotate(0,0,θ)` 原值）。
 *   进 = script3..6 = ±1.9198622 / ±1.7453293；出 = script7..10 = ±1.2217305 / ±1.3962634。
 *   ⚠ 与那 8 个脚本里的 `rotate()` **必须一致**（改一处就要改两处；`AUDIT` §CT-1 有对照表）。*/
#define TRANCE_WIPE_THETA_IN  { 1.9198622f, -1.9198622f, 1.7453293f, -1.7453293f }
#define TRANCE_WIPE_THETA_OUT { 1.2217305f, -1.2217305f, 1.3962634f, -1.3962634f }
#define TRANCE_WIPE_PROBE_N   4     /* 一寸诊断：每批打前 4 条（4 台各一条）；进/出**各一轮**（共 8 行）*/
static const float s_wipe_theta_in[TRANCE_WIPE_N]  = TRANCE_WIPE_THETA_IN;
static const float s_wipe_theta_out[TRANCE_WIPE_N] = TRANCE_WIPE_THETA_OUT;
static int32_t s_wipe_probe;        /* 已打过的探针条数（每条 VM 各一次；进/出各一轮）*/
static int32_t s_wipe_frame;        /* 本批幕布起飞后过了几帧（驱动填充幕的 alpha）*/

/* ★★★ 2026-09-13 第九轮（`AUDIT` §CT-7）：**把"目的地 alpha"从 C 侧钉死**。
 *
 * 一手事实（`local/th18.v1.00a/th18.exe`）：TH18 的 anm `blendMode` 落地函数 `0x47d937`–`0x47d9a8`
 * 在每条模式分支**之前**统一设了 `SEPARATEALPHABLENDENABLE(0xce)←1`、`SRCBLENDALPHA(0xcf)←ONE`、
 * `DESTBLENDALPHA(0xd0)←ZERO`、`BLENDOPALPHA(0xd1)←ADD` ⇒ **任何 anm sprite 都满足 `Da' := Sa`**
 * （`blendMode(n)` 只管 RGB）。而**顶点色取自哪一路**由 `colorMode` 决定（`0x47e202` 的分派：
 * mode 0 → `+0x524` color_1、mode 1 → `+0x528` color_2；渲染器把选中的那一路写进 `+0x52c`）。
 * 脚本的 `color()` 只写 color_1 的 RGB、`alpha()` 只写 color_2 的 A ⇒ **只靠脚本无法保证
 * "被选中的那一路的 A" 是我们想要的值**（本轮"遮罩不生效、astral 一直全显"的成因就是这个）。
 * ⇒ 卡侧**每帧把两路颜色都写一遍**（`ce_anm_set_color` + `ce_anm_set_color2`）⇒ **与 colorMode 无关**。*/
#define TRANCE_WIPE_ARGB   0xFF404038u  /* = color(64,64,56) + A=255（D3DCOLOR 是 BGRA 序 ⇒ 0x38,0x40,0x40）*/
#define TRANCE_ZERO_ARGB   0x00000000u  /* 清零幕：color_1 = (0,0,0,0) ⇒ Sa = 0 且 S.rgb = 0（RGB 保留）*/
#define TRANCE_ZERO_ARGB2  0x01000000u  /* 清零幕：color_2 = (0,0,0,1) ⇒ 若被选中是 color_2，Sa = 1/255 ≈ 0；
                                         * A 取 1 而不是 0 是为了**不被绘制路径的早退**（`0x4812e8`：
                                         * `color_1.A == 0 && color_2.A == 0` ⇒ 整台 VM 不画）挡掉。*/
#define TRANCE_FILL_OFF    0x01404038u  /* 填充幕未到第 45 帧：A=1（≈透明，但仍会被画）*/
static void wipe_set_dual_color(uint32_t id, uint32_t argb)
{
    if (!id) return;
    (void)ce_anm_set_color(id, argb);        /* color_1（`+0x524`；colorMode 0 用的顶点色）*/
    (void)ce_anm_set_color2(id, argb);       /* color_2（`+0x528`；colorMode 1 用的顶点色）*/
}

/* 诊断（文件级；进 `on_item_frame` 的汇总日志，不占 `ce_state`——496 字节要留给真的状态）*/
static int32_t s_stat_spirit_spawned;   /* 撒出去几颗（含被后续丢掉的）*/
static int32_t s_stat_spirit_picked;    /* 被自机走过去吃掉的颗数（= 入槽的来源）*/
static int32_t s_stat_spirit_expired;   /* 到点（寿命 480 帧）自己消失的颗数 */
static int32_t s_stat_spirit_dropped;   /* 池满 / VM 建失败而**没能撒出去**的颗数（丢尾）*/
static int32_t s_spirit_log_n;          /* 神灵 VM 字段诊断只打前 3 颗（第五轮）*/
static int32_t s_stat_spirit_out;       /* 出界被删的颗数（一手框 x∈[−224,224] / y∈[−32,480]）*/
/* ★ 2026-09-14：连锁奖励（一手 `spawn_spirit` 尾部）撒出去几颗 + 日志限流计数。
 *   限流的理由：旧版在**收集**那一刻逐颗打日志，一次实跑连打 160 行（用户口径 #1 的证据）。*/
#define TRANCE_CHAIN_LOG_LIMIT 6
#define TRANCE_MAXPOINT_LOG_LIMIT 6   /* type 1 加成日志限流（6 行足够对账）*/
static int32_t s_stat_chain_bonus;
/* ★★ 2026-09-14 寿命取证（口径 #5）的四个诊断静态量；语义见 `trance_age_probe()` 的注释。
 *   全部只读只写自己的计数，**不参与任何玩法判定**。*/
#define TRANCE_AGE_PROBE_SPIRITS 2      /* 逐帧打 age 的"探测颗"数量 */
static int32_t s_stat_tick_dup;         /* 帧守卫挡掉的重复推进次数（正常恒 0；非 0 就是"每帧两次"）*/
static int32_t s_stat_age_probe_lines;  /* 已打出的关键帧行数（上限 = 关键帧数）*/
static int32_t s_stat_expire_log;       /* 到期取证日志已打几条（限 6）*/
static int32_t s_age_probe_used;        /* 已选中几颗探测颗 */
static int32_t s_age_probe_done;        /* 关键帧已打过几行（上限 = 关键帧数）*/

/* ★ **重建 ≠ 出生**（2026-09-13 第四轮，音效对齐时发现）：`trance_spirit_restyle()` 会把池里每一颗
 *   活着的神灵**删了重撒**（普通态/灵界态是两组脚本，VM 起过就没法换），那一刻 `trance_spirit_spawn()`
 *   会被连着调 N 次 —— 照搬出生音就会在**进/出灵界的瞬间连响 N 声**（最多 128 声）。
 *   一手 TH13 换样式**根本不重建 VM**（`spawn_spirit` 只在真的生成时跑），所以那批音在原版不存在
 *   ⇒ 用这个闩锁让重建静音。**它不是"可选的音效开关"**：只允许 restyle 那一小段置位。*/
static int s_spirit_spawn_silent;
static int32_t s_stat_spirit_vm_gone;   /* VM 在寿命之前就没了（诊断；见 tick 里的检查）*/
static int32_t s_stat_state[8];          /* 当前各 state 的颗数（诊断：1 漂移 / 2 被吸 / 4 灵梦慢吸）*/
static int32_t s_stat_maxpoint_log;       /* 诊断：type 1 加成打过几行日志（限流）*/
static int32_t s_stat_popup;             /* 起过几支数字弹窗（收集反馈 ③）*/
static int32_t s_stat_popup_failed;      /* 其中 VM 没建起来的次数（anm 没装载时会涨）*/

/* ★★ 2026-09-22 工单（owner th13 #4「灵界状态下被 boss 体术 = 不能移动」）的**现场取证**。
 *   一手结论（`engine/player/th18/03-player-hit-gate.md` §1.6）：TH18 里**唯一**的「不能移动」=
 *   自机状态机 `player+0x476ac` 离开 `1`（移动只从 `Player__on_tick__body` 的 case 1 调，
 *   `th18:0x45c0c1`→`0x45c0cc call 0x45b170`；全 exe 只有 7 处写状态，值 4 只由 `0x45d4e4` 写在
 *   `Player__die` 体内 = 我们闸门之后；值 2 只由 `0x45d21c` 写在 `0x45d090`，而它只能从状态 4 走到）。
 *   ⇒ 灵界期间只要闸门在跑，状态就该**恒 1**、自机可自由移动。
 *   这两个计数就是那句话的可观测化（**只读** `+0x476ac`、不写任何引擎字段）：
 *     · `s_stat_locked_frames` = 灵界期间读到状态 ≠ 1 的帧数（正常恒 0）；
 *     · 每次**状态值变化**各打一行（限 `TRANCE_LOCK_LOG_LIMIT` 行），带当时的状态值，
 *       方便 owner 回报的是"哪个状态把自机锁住了"而不是"感觉动不了"。
 *   ⚠ 已知会出 1 行的正常情形：死亡触发档（DEATH_FREE）窗口结束那一帧自机本来就要死
 *     （状态会走 4 → 2），那一条是**预期**，不是回归。*/
#define TRANCE_LOCK_LOG_LIMIT 6
static int32_t s_stat_locked_frames;     /* 灵界里状态 ≠ 1 的帧数（诊断；正常恒 0）*/
static int32_t s_lock_log_lines;         /* 已打过的"被锁"日志行数（限流）*/
static int32_t s_lock_state_prev = 1;    /* 上一帧读到的状态（边沿检测用）*/

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
    s_spirit_style = 0;
    /* ⚠ 诊断计数**刻意不清**（它们只增不减、只进日志；清掉反而丢掉「删卡前撒了多少」这条线索）。
     *   只有 `on_stage_start` 想从零对账时才会重置 —— 见那里的注释。*/
}

/* 撒一颗（帧末从击破队列里来；**绝不在死亡回调里调** —— `DEATH` §2.4 第三条禁令）。
 * `in_trance` = 撒的这一刻在不在灵界里 ⇒ 选普通态 / 灵界态那一组脚本（一手 `type*6+1` / `type*6+4`）。
 * `core` = 卡状态；**非 NULL 时本函数在"真的撒出去了"之后推进神霊连锁**（一手 `spawn_spirit` 尾部
 *   `th13:0x4391a0`：`inc [ecx+0x8838]` / `cmp …,0xa` / `jl` / 第 10 颗起额外撒一颗 type 3）。
 *   ⚠ 传 NULL 的三处：**换样式的整批重建**（`trance_spirit_restyle`，那不是原版的"出生"事件）、
 *     连锁奖励自己那一颗（递归会自激）、以及**调试环**（SPACE 爆一圈不是原版的掉落，见那里的注释）。
 * `speed` = 初速；**≤ 0 ⇒ 用一手出生速度**（`参数 + randf()*3.5`，`th13:0x43919a`）；
 *   连锁奖励那一颗一手写死 **0.5**（`th13:0x4aedf8`）⇒ 显式传值。*/
static void trance_spirit_spawn(float x, float y, int32_t type, int in_trance,
                                trance_state_t *core, float speed)
{
    uint32_t id;
    trance_spirit_t *p;

    if (trance_spirit_pool_room(s_n_spirit) <= 0) {   /* 池满：丢尾 + 计数（诊断进汇总日志）*/
        ++s_stat_spirit_dropped;
        return;
    }
    /* ★★ 2026-09-13 第三轮补：**出生点必须夹进原版的框**（一手 `spawn_spirit` `0x438ec0`：
     *   `pos` 被夹到 x∈[−192,192]、y∈[0,384]，`engine/spirit/th13/01` §3）。
     *   用户反馈「按空格之后灵爆的全屏幕都是，甚至有部分移动出边界」= 我们的爆量调试环
     *   直接按半径撒、没夹 ⇒ 一出生就在游玩区外（出界删除框 ±224/512 之外还会飞一会儿）。*/
    if (x < -192.0f) x = -192.0f; else if (x > 192.0f) x = 192.0f;
    if (y <    0.0f) y =    0.0f; else if (y > 384.0f) y = 384.0f;
    id = ce_anm_spawn(CE_ABILITY_ANM(), spirit_script_for(type, in_trance), TRANCE_SPIRIT_LAYER);
    if (!id) {                                        /* anm 还没装载 / 管理器没起：静默丢弃 + 计数 */
        ++s_stat_spirit_dropped;
        return;
    }

    p = &s_spirit[s_n_spirit];
    memset(p, 0, sizeof *p);
    p->id = (int32_t)id;
    /* ★★ 2026-09-13 第五轮（用户实跑「按空格只有灵界槽涨、看不见灵」）：
     *   TH13 的父脚本是 `alpha(0)`（父是**不带 sprite 的容器**）。在 TH13 上两个子各有自己的
     *   `color()`/`alpha(255)` ⇒ 看得见；TH18 侧子会吃到父的那一档（父子颜色继承）⇒ **整支透明**：
     *   神灵看不见、但收集逻辑照跑（槽照涨、日志照记）= 用户看到的现象。
     *   这里在 spawn 之后**给父写一次不透明白**（父不画东西 ⇒ 不改变观感，只保证不拖累子）；
     *   脚本侧也把父的 `alpha(0)` 改成了 `alpha(255)`（生成器 `make_trance_spirit_scripts.py`）。
     *   ⚠ 只给**父**写：子的配色是**每型不同**的一手值（写白会把染色抹掉）。*/
    (void)ce_anm_set_color(id, 0xffffffffu);
    /* ★ 同轮诊断：前 3 颗把引擎**当时认到的字段**打出来（layer / sprite / color / rect / scale）。
     *   用户报「看不见灵」时，这一行能区分三种根因：① VM 没建起来（上面 dropped 会涨）；
     *   ② 建起来了但 color 的 alpha 是 0（= 透明）；③ sprite 矩形不是我们切的那张（贴图错位）。*/
    if (s_spirit_log_n < 3) {
        const uint8_t *vm = ce_anm_get_vm(id);
        ++s_spirit_log_n;
        ce_log("trance:   spirit VM %d (script %d, type %d, %s) layer %d sprite %d color 0x%08x "
               "rect %.0fx%.0f scale (%.2f,%.2f)",
               (int)id, spirit_script_for(type, in_trance), (int)type, in_trance ? "TRANCE" : "plain",
               vm ? (int)*(const int32_t *)(vm + TRANCE_VM_LAYER) : -1,
               vm ? *(const int32_t *)(vm + CE_ANM_VM_SPRITE_ID) : -1,
               vm ? (unsigned)*(const uint32_t *)(vm + CE_ANM_VM_COLOR1) : 0u,
               vm ? (double)*(const float *)(vm + TRANCE_VM_SPRITE_W) : 0.0,
               vm ? (double)*(const float *)(vm + TRANCE_VM_SPRITE_H) : 0.0,
               vm ? (double)((const float *)(vm + CE_ANM_VM_SCALE))[0] : 0.0,
               vm ? (double)((const float *)(vm + CE_ANM_VM_SCALE))[1] : 0.0);
    }
    p->x = x;
    p->y = y;
    /* 诊断（口径 #5 的取证）：最早的两颗当"寿命探测颗"（`trance_age_probe` 只读它、不判玩法）。*/
    p->probe = (s_age_probe_used < TRANCE_AGE_PROBE_SPIRITS) ? 1u : 0u;
    /* 寿命按**机体特技**取（一手 480；咲夜 = 960 —— 用户口径「灵消失的时间增加」）*/
    p->life = s_spirit_life;
    p->age = 0;
    p->type = type;
    p->style = in_trance ? 1 : 0;
    p->born_trance = in_trance ? 1 : 0;   /* ★ 出生旗标（一手 `+0x84` bit0）：奖励/槽按它算，见结构体注释 */
    /* ★ 初速与抛向（一手 `th13:0x43919a`：`speed = 参数 + randf()*3.5`；方向 = 象限 + 随机抖动）。
     * ★★ 2026-09-13 第三轮订正（用户实跑：「即使是灵梦，灵会随机地移动吗？我记得原版不会移动」）：
     *   一手**不是**纯随机方向 —— `0x4390e1` 的 switch 按 **`spirit+0xc`（= 出生序号）% 4** 选**象限**，
     *   四个象限是**对角线**方向（−3π/4 / +π/4 / +3π/4 / −π/4），再叠一个 ±π/8 的抖动。
     *   ⇒ 所以原版观感是"一批灵按出生次序朝四个斜角散开"（有秩序、可预期），而不是每颗各自乱飞。
     *   我们旧版用 `ce_rand() & 15` 抽方向 ⇒ 看上去就是"随机乱走"。现在照一手：
     *   象限 = `s_spirit_spawn_index % 4`、抖动 = ±1 格（16 分度表 = 22.5° = 一手的 π/8 ✅）。
     *   ⚠ **不引三角函数**（i686 上 `sinf/cosf` 的 float 返回值走 st(0) ⇒ 会把 x87 带进我们的目标文件），
     *     用 16 分度的单位圆表。*/
    {
        static const float c[16] = {  1.000f,  0.924f,  0.707f,  0.383f,
                                      0.000f, -0.383f, -0.707f, -0.924f,
                                     -1.000f, -0.924f, -0.707f, -0.383f,
                                      0.000f,  0.383f,  0.707f,  0.924f };
        static const float s16[16] = { 0.000f,  0.383f,  0.707f,  0.924f,
                                       1.000f,  0.924f,  0.707f,  0.383f,
                                       0.000f, -0.383f, -0.707f, -0.924f,
                                      -1.000f, -0.924f, -0.707f, -0.383f };
#if TRANCE_SPIRIT_MOTION_TH17
        /* ★★ TH17 的**出场散射**（一手 `th17:0x410570`）：角度**任意**（不是象限），
         *   但要避开"与 24 px 内另一颗同向（点积 ≥ 0.96）"——撞上就把角度 +222.5° 重试，最多 10 次。
         *   我们用的是 16 分度表（22.5°/格）⇒ 重试步长取 **10 格 = 225°**（与一手差 2.5°，可忽略；
         *   点积判据照一手 0.96，距离判据照一手 24 px ⇒ 同向后转 = 视觉上一批灵**散开**而不是叠着飞）。*/
        {
            int32_t k = (int32_t)(ce_rand() & 15u);
            int32_t tries;
            for (tries = 0; tries < 10; ++tries) {
                int32_t j, clash = 0;
                const float vx = c[k], vy = s16[k];
                for (j = 0; j < s_n_spirit; ++j) {
                    const float ox = s_spirit[j].x - x, oy = s_spirit[j].y - y;
                    if (ox * ox + oy * oy < 576.0f) {                  /* 一手：距离² < 576（24 px）*/
                        if (vx * s_spirit[j].dx + vy * s_spirit[j].dy >= 0.96f) { clash = 1; break; }
                    }
                }
                if (!clash) break;
                k = (k + 10) & 15;                                     /* 一手：+3.883222 rad ≈ 222.5° */
            }
            ++s_spirit_spawn_index;                                    /* 一手 `+0x8820` 每生成一颗 ++ */
            p->dx = c[k];
            p->dy = s16[k];
            p->speed = (speed > 0.0f) ? speed : TRANCE_SPIRIT_SPEED_TH17;  /* 一手：单位圆向量 ⇒ 1.0/帧 */
        }
#else
        /* 象限基点：+π/4=2 / +3π/4=6 / −3π/4=10 / −π/4=14（一手那四个斜角，索引每次 +4）*/
        const int32_t jit = (int32_t)(ce_rand() % 3u) - 1;          /* ±1 格 = ±22.5° = 一手的 ±π/8 */
        const int32_t k   = (2 + 4 * (s_spirit_spawn_index & 3) + jit) & 15;
        ++s_spirit_spawn_index;                                     /* 一手 `+0x8820` 每生成一颗 ++ */
        p->dx = c[k];
        p->dy = s16[k];
        /* 一手 `th13:0x43919a`：`speed = 参数 + randf()*3.5` —— **六条通道都这样**，
         * 调用方传进来的就是**通道参数**（0.5 或 2.0，见上面那组常量的注释），随机项一律在这里叠。*/
        p->speed = speed +
                   (float)(ce_rand() % 1000u) * (TRANCE_SPIRIT_SPAWN_RAND / 1000.0f);
#endif
        p->state = TRANCE_STATE_DRIFT;
    }
    ++s_n_spirit;
    ++s_stat_spirit_spawned;

    /* 出生只写一次**位置**（**不下落**，之后每帧只有微动偏移）。
     * ★ 贴图 / 颜色 / 缩放**不写**：它们由脚本自己给（普通态父 `-1: scale(1.5,1.5)`、
     *   子脚本各自的 `color()`；灵界态父不带 scale）。卡侧写 scale 会被脚本的插值器盖掉。*/
    ce_anm_set_pos(id, x, y, 0.0f);

    /* 出生音（一手 `th13:0x438ef0 ADD EDI,0x3b` = `type + 0x3b`：TH13 的 `0x3b..0x3e` 就是 `se_lgods1..4`，
     * TH18 同号的 `0x3b..0x3e` 是**同名同字节的同一批文件**）。
     * 声像用**出生点 x**（一手 `th13:0x438eea fld [pos]` 取的就是 spawn 位置的 x）。
     * ★ **播放条件**：一手只在 `spawn_spirit` 的 `play_sound` 参数 ≠ 0 时播（`th13:0x438ee0` 判 `[ebp+0x14]`）。
     *   六条通道的取值（`SPIRIT` §3 通道表）：A 伤害阈值 `1` / B 死亡计时 `1` / C ECL spec0 `1` /
     *   D item 10..14 `1`（`dropItems`、`dropMain(14)` 都走这里 ⇒ **boss 相位批次同此**）/
     *   E **玩家死亡 `0`（静音）** / F 连锁奖励 `1`。
     *   ⇒ 我们实现的那几条（击破掉灵 = B、boss 掉灵 = A、相位批次 = D、连锁白灵 = F）**全是 `1`**，
     *     所以这里无条件播是**对的**；⚠ 将来若补 TH13 的通道 E（死亡动画期间每帧一颗 type 3），
     *     那一颗**必须**传 0 不播音 —— 本函数目前没有 switch，加的时候记得带上。
     * ⚠ 唯一的例外是**换样式的重建**（`s_spirit_spawn_silent`，见它的声明）：那不是一手里的 spawn 事件。*/
    if (!s_spirit_spawn_silent)
        ce_play_sound(TRANCE_SE_SPAWN_BASE + (uint32_t)(type & 3), x);

    /* ★★★ 神霊连锁 —— **一手就在这个位置**（`spawn_spirit` 的尾部，`th13:0x4391a0`）：
     *   `0x4391c5 cmp edi,3 / je` → `0x4391ce cmp edi,4 / je`（type 3/4 直接返回，**不推进**）
     *   → `0x439212 mov [ecx+0x8828],0x3c`（窗口 = 60，**每颗重置**）
     *   → `0x439232 inc [ecx+0x8838]` / `0x439238 cmp …,0xa` / `0x43923f jl`（第 10 颗起每颗都触发）
     *   → `0x439250 call 0x438ec0`（`spawn_spirit(this, 3, pos, 0.5f, 1)`）
     * ⇒ 「白灵」是**掉灵**的奖励，跟吃没吃到无关（这就是用户口径 #1：原版"小怪有时会掉白灵"）。
     *   上一版把它挂在**收集**上 ⇒ 玩家连续吸灵时 60 帧窗口永远不过期 ⇒ 第 10 颗之后**每颗**都白
     *   （实跑 `chain 80(bonus 71)`）—— 已订正，详见 `trance_core.h` 的粒度订正段。
     * ⚠ 递归安全：奖励那一颗传 `core = NULL`（不推进），而且它是 type 3（一手也跳过）。*/
    if (core && type != TRANCE_CHAIN_BONUS_TYPE && type != 4) {
        if (trance_chain_note(core, type)) {
            ++s_stat_chain_bonus;
            /* 一手 `spawn_spirit(this, 3, pos, 0.5f, 1)`：**同一个 pos**、**速度写死 0.5**、有声。
             * ⚠ 日志**限流**（旧版每颗一行 ⇒ 上一次实跑连打 160 行）：只打前
             *   `TRANCE_CHAIN_LOG_LIMIT` 次，之后靠汇总行的 `chain N(bonus M)` 对账。*/
            if (s_stat_chain_bonus <= TRANCE_CHAIN_LOG_LIMIT)
                ce_log("trance: chain bonus (spawn-time, TH13 0x4391a0) #%d: window-internal count %d "
                       ">= %d -> extra type %d at (%.0f,%.0f) speed %.1f",
                       (int)s_stat_chain_bonus, (int)core->chain_count, TRANCE_CHAIN_THRESHOLD,
                       TRANCE_CHAIN_BONUS_TYPE, (double)x, (double)y, (double)TRANCE_CHAIN_BONUS_SPEED);
            trance_spirit_spawn(x, y, TRANCE_CHAIN_BONUS_TYPE, in_trance, NULL, TRANCE_CHAIN_BONUS_SPEED);
        }
    }
}

/* ★★ 神灵的**大小**（2026-09-13 实跑订正 #4：用户说"最大大小比 TH13 大了 1.5 倍"）
 *
 * 一手（THBWiki 口径 #19）：「**灵界结束后，所有灵体的大小均恢复到最大**。因此可利用此特性
 *   加速下一个灵界的开启（灵梦因为自动收集灵体所以不行）」⇒ **大小是一个独立的计时器**，
 *   不是"一辈子只缩一次"：它必须能被灵界边界重置。
 * 我们上一版把 `astral.anm script1` 的 `-1: scale(1.5,1.5)` 当成了最终大小 ⇒ 全程比原版大 1.5 倍。
 * ⇒ 现在**由卡侧每帧写大小**（曲线本体在 core 的 `trance_spirit_scale_permille()`，唯一口径）：
 *   · 基准 **1.0**（不是 1.5）；**前 200 帧保持最大**、之后 200 帧线性缩到 0.4 —— 相位照抄一手
 *     `+200: scaleTime(200,0,0.4,0.4)`（★ 2026-09-14 订正：上一版从 age 0 就开始缩 ⇒ 早 200 帧变最小）；
 *   · `size_age` 每帧 +1；**进/出灵界时全部清零**（= 口径 #19 的"恢复最大"）。
 * ⚠ 脚本里那句 `scale(1.5)` 仍在（它是**一手原文**，不改），但我们的每帧写**在后**、盖住它。
 *   若将来发现两条打架（画面抖动），把脚本里那句删掉即可（`make_trance_spirit_scripts.py` 的表里改）。
 * ★ 本函数上一版是 core 那个曲线的**副本**（基准 1000 vs core 的 1500，两套值两处维护）——
 *   已删：卡侧直接调 core 的 `trance_spirit_scale_permille()`（见 `trance_spirits_tick()`）。*/

/* ★ 调试键：绕自机撒一圈神灵（用户口径：蓝为主 / 白少量 / 绿 1 / 红 1）。
 * 半径分三环，避免全叠在同一个圆上；**位置全部写死成字面量常量表**（不用三角函数 ⇒ 零浮点库依赖）。*/
#if TRANCE_DEBUG_KEY
static const float s_dbg_ring_dx[16] = {
     1.000f,  0.924f,  0.707f,  0.383f,  0.000f, -0.383f, -0.707f, -0.924f,
    -1.000f, -0.924f, -0.707f, -0.383f,  0.000f,  0.383f,  0.707f,  0.924f };
static const float s_dbg_ring_dy[16] = {
     0.000f,  0.383f,  0.707f,  0.924f,  1.000f,  0.924f,  0.707f,  0.383f,
     0.000f, -0.383f, -0.707f, -0.924f, -1.000f, -0.924f, -0.707f, -0.383f };

static void trance_debug_burst(ce_card_t *c)
{
    trance_card_t *s = st(c);
    const float *pp = ce_player_pos();
    const float cx = pp ? pp[0] : 0.0f;
    const float cy = pp ? pp[1] : 224.0f;
    /* CTRL+SPACE = 大环（200 px，看美术用）；SPACE = 小环（60 px，吃测试用）。*/
    const float base_r = ce_key_down(CE_VK_CONTROL) ? TRANCE_DEBUG_RADIUS_WIDE : TRANCE_DEBUG_RADIUS;
    const int in_trance = s && s->core.phase != TRANCE_PHASE_IDLE;
    int32_t placed = 0, dropped_before = s_stat_spirit_dropped;
    int g;

    for (g = 0; g < 4; ++g) {
        static const int32_t types[4] = { TRANCE_DEBUG_BLUE_TYPE, TRANCE_DEBUG_WHITE_TYPE,
                                          TRANCE_DEBUG_GREEN_TYPE, TRANCE_DEBUG_RED_TYPE };
        static const int32_t counts[4] = { TRANCE_DEBUG_BLUE_N, TRANCE_DEBUG_WHITE_N,
                                           TRANCE_DEBUG_GREEN_N, TRANCE_DEBUG_RED_N };
        int32_t k;
        for (k = 0; k < counts[g]; ++k, ++placed) {
            float r = base_r * (1.0f + 0.35f * (float)((placed / 16) % 3));
            /* ⚠ `core` 传 **NULL**：调试环**不推进连锁** —— 它不是原版的掉落事件，
             *   按 40 颗/次推进的话每按一次空格就会当场多出 25 颗白灵，会把 #1 的修复看反。*/
            trance_spirit_spawn(cx + s_dbg_ring_dx[placed % 16] * r,
                                cy + s_dbg_ring_dy[placed % 16] * r,
                                types[g], in_trance, NULL, TRANCE_SPIRIT_SPAWN_SPEED);   /* 调试环 */
        }
    }
    ce_log("trance: DEBUG BURST by %s - %d spirits around the player (%.0f,%.0f) 半径 %.0f px: "
           "blue(type %d) x%d, white(type %d) x%d, green(type %d) x%d, red(type %d) x%d "
           "[style=%s, pool %d/%d, dropped %d]",
           base_r > TRANCE_DEBUG_RADIUS ? "CTRL+SPACE(大环)" : "SPACE(小环)",
           (int)placed, (double)cx, (double)cy, (double)base_r,
           TRANCE_DEBUG_BLUE_TYPE, TRANCE_DEBUG_BLUE_N,
           TRANCE_DEBUG_WHITE_TYPE, TRANCE_DEBUG_WHITE_N,
           TRANCE_DEBUG_GREEN_TYPE, TRANCE_DEBUG_GREEN_N,
           TRANCE_DEBUG_RED_TYPE, TRANCE_DEBUG_RED_N,
           in_trance ? "TRANCE" : "plain", s_n_spirit, TRANCE_SPIRIT_POOL,
           (int)(s_stat_spirit_dropped - dropped_before));
}
#endif /* TRANCE_DEBUG_KEY */

/* ★ **换样式**（用户 2026-09-13 口径 ⑥「灵界状态下所有神灵会变成另一个样式被吸过来」）：
 * 灵界进 / 出那一刻，把池里每一颗**活着**的神灵换成对应态的脚本。
 * ⚠ 为什么是"重建 VM"而不是改字段：普通态与灵界态是**两组不同的脚本**（父/子都不同、
 *   缩放曲线也不同），VM 一旦起脚本就没法换 —— 只能删了重撒。
 *   `x/y/type/life/age` 全部**照搬**（神灵不因为换了样式而重新计时、也不会跳位置）。*/
static void trance_spirit_restyle(int in_trance)
{
    trance_spirit_t snap[TRANCE_SPIRIT_POOL];
    int n = s_n_spirit, i;

    if (n == 0) return;
    memcpy(snap, s_spirit, sizeof snap[0] * (size_t)n);
    trance_spirit_clear_all();
    /* ★ 这一段是**重建**不是出生 ⇒ 静音（`s_spirit_spawn_silent` 的声明里有理由：
     *   否则进出灵界那一瞬间会把池里每颗的出生音连着播一遍）。*/
    s_spirit_spawn_silent = 1;
    for (i = 0; i < n; ++i) {
        /* 正在播消失段的那些（`dying`）**不重建**：它们下一刻就自删了，
         * 重建只会让"吃到的那颗"在灵界开关的一瞬间诈尸。*/
        if (snap[i].dying) continue;
        /* ⚠ `core` 传 **NULL**：这是**换样式的重建**，不是原版的"出生"事件（推进连锁会凭空造白灵）。*/
        trance_spirit_spawn(snap[i].x, snap[i].y, snap[i].type, in_trance, NULL,
                            TRANCE_SPIRIT_SPAWN_SPEED);        /* 重建：按通道 B 的参数重掷 */
        if (s_n_spirit > 0) {                       /* 把生命周期原样接回去 */
            s_spirit[s_n_spirit - 1].life = snap[i].life;
            s_spirit[s_n_spirit - 1].age  = snap[i].age;
            /* ★ **出生旗标不能跟着重建走**（一手 `+0x84` bit0 是出生那一刻定的）——
             *   重建传的是 in_trance，会把它改写成"当前态"，那样奖励档会在开关灵界时跳档。*/
            s_spirit[s_n_spirit - 1].born_trance = snap[i].born_trance;
            /* ★ 口径 #19：**大小恢复到最大**（`size_age = 0`）—— 换个样式就"重新长满"，
             *   原版就是靠这条让玩家"下个灵界来得更快"。*/
            s_spirit[s_n_spirit - 1].size_age = 0;
        }
    }
    s_spirit_spawn_silent = 0;
}

/* ★★ 寿命取证（2026-09-14；用户口径 #5「灵消失的速度似乎是 1.5 倍速」要的是**可核对的行**）。
 *
 * 打三样东西，对应三个必须钉死的数字：
 *   ① **每帧的 age 增量**：被选中的探测颗（`p->probe`，全局最多 `TRANCE_AGE_PROBE_SPIRITS` 颗）
 *      前 6 帧**逐帧**打一行，带 `ce_sdk_frame()`（= SDK 自己数的游玩帧）⇒ 相邻两行的
 *      `sdk_frame` 差必须 == `age` 差 == 1；出现 +2 就是"每帧被推两次"（本函数另有帧守卫兜着）。
 *   ② **缩小曲线的相位**：`scale_permille` 在 age ≤ 200 必须恒为 `TRANCE_SPIRIT_SCALE_PERMILLE_BASE`
 *      （一手 `+200:` 之前不缩），age == 200 + 200 = 400 起必须是 `…_END_PERMILLE`。
 *      用一张"关键帧各打一次"的全局位图 ⇒ 一共 10 行，日志不会被刷。
 *   ③ **整段寿命**：到期那一刻另打一行（见 `trance_spirits_tick()` 的到期分支）⇒
 *      "480 帧到期 + 脚本尾巴 40 帧 = 520 帧" 能不能对上，一眼就能算。*/
static void trance_age_probe(const trance_spirit_t *p)
{
    /* 关键帧：1/2（逐帧增量）、199/200/201（缩小的起点）、300（插值中段）、400（终点）、479/480（到期）*/
    static const int32_t keys[] = { 1, 2, 199, 200, 201, 300, 400, 479, 480 };
    int32_t k;
    const int32_t scale = trance_spirit_scale_permille(p->size_age);

    if (p->probe && p->age <= 6) {
        ce_log("trance: AGE PROBE id %d sdk_frame %u age %d life %d size_age %d scale_permille %d "
               "(每帧 age 必须 +1；life 每帧 −1；前 200 帧 scale 恒 %d)",
               (int)p->id, (unsigned)ce_sdk_frame(), (int)p->age, (int)p->life, (int)p->size_age,
               (int)scale, TRANCE_SPIRIT_SCALE_PERMILLE_BASE);
        if (p->age == 6) ++s_age_probe_used;   /* 这颗打完了，换下一颗（避免刷屏）*/
    }
    for (k = 0; k < (int32_t)(sizeof keys / sizeof keys[0]); ++k) {
        if (p->age != keys[k]) continue;
        /* 每个关键帧**全局只打一行**（哪一颗先到就记它）⇒ 总数固定 9 行。*/
        if (s_age_probe_done & (1 << k)) return;
        s_age_probe_done |= (1 << k);
        ++s_stat_age_probe_lines;
        ce_log("trance: AGE KEY id %d sdk_frame %u age %d life %d size_age %d scale_permille %d "
               "(一手 astral.anm script1：前 200 帧不缩、200→400 缩到 400‰、480 帧到期)",
               (int)p->id, (unsigned)ce_sdk_frame(), (int)p->age, (int)p->life, (int)p->size_age,
               (int)scale);
        return;
    }
}

/* 每帧推进池（`on_item_frame` ⑥，**在撒点之后**）：缩小 → 微动 → 靠近判定 → 到期。
 * ⚠ 一个浮点运算都没有除了「喂给引擎的 int→float 转换」与平方距离的比较
 *   （`-mfpmath=sse` 下全是 SSE；本文件**没有任何返回 float 的函数** ⇒ `make dllx87` 门安全）。
 *
 * ★★ 2026-09-14 **帧守卫 + 每帧增量自检**（用户口径 #5 要求"给出可核对的行"）：
 *   ① **一道 `ce_sdk_frame()` 守卫**（照 `faith_pool_tick()` 的同一套，见 `sdk_core.h` 的 `ce_sdk_frame`）：
 *      本函数是**每帧只许走一次**的推进器（`age++ / size_age++ / life--` 都是帧率刻度）；
 *      万一将来有第二条调用路（或卡实例重复），一帧走两次就会让寿命**凭空减半**。
 *      守卫的代价是两次比较，收益是"寿命不再依赖调用次数"。
 *   ② **头 `TRANCE_AGE_LOG_MAX` 颗**在 `age ∈ {1, 2, 60, 199, 200, 201, 300, 400, 479, 480}` 这些
 *      关键帧各打一行 `age / life / size_age / scale‰` ⇒ 实跑日志里能直接核对
 *      「每帧 age +1（不是 +2/+3）」「前 200 帧 scale 恒 1000」「400 帧才到 400‰」。
 *      行数 = 颗数 × 10，**只在卡加载后最早的那几颗**上打 ⇒ 日志不会被刷。*/
static void trance_spirits_tick(ce_card_t *c)
{
    const float *pp = ce_player_pos();
    uint8_t *pl = CE_PLAYER();
    int pickup_ok = 1;
    /* ★ 灵梦的"任意距离慢吸"（一手 state 4）现在走**特技表**：`trance_traits(角色).reimu_slow_attract`。
     * （一手只对 TH13 的 `CHARACTER == 0` 生效 —— 而 TH13/TH18 的灵梦都是 0 号；
     *   🟡 见 `engine/spirit/th13/03-spirit-motion-and-spawn.md` §5.2 的推断链。）*/
    const int is_reimu = trance_traits(trance_character())->reimu_slow_attract;
    int i;

    /* ① 帧守卫：本帧已经推过一次就直接返回（`ce_sdk_frame()` 在 `BP_ce_item_frame` 里 ++，
     *   与 `faith_pool_tick()` 的守卫同源）。放在**一切副作用之前**。*/
    {
        static uint32_t s_last_frame;
        static int      s_have_last;
        const uint32_t now = ce_sdk_frame();
        if (s_have_last && now == s_last_frame) {
            ++s_stat_tick_dup;
            return;
        }
        s_last_frame = now;
        s_have_last = 1;
    }

    /* 一手的门（`SPIRIT` §4 末行 / TH13 `SpiritItem__on_tick` 各 state 分支）：
     * **自机死亡或复活中（`player+0x476ac` ∈ {2,4}）不被吸引、也不收**。
     * TH18 侧的同一读点 = `CE_PLAYER_STATE`（`engine.h:274`）；`cards/faith.c` 的
     * `faith_player_above_line()` 用的是同一条（那里只为回收线，这里是吃取）⇒ 口径一致。
     * ⚠ 只是「这一帧不吃」，**不**影响缩小 / 到期（神灵照常变小、照常到点消失）。*/
    if (pl) {
        int32_t pstate = *(int32_t *)(pl + CE_PLAYER_STATE);
        if (pstate == 2 || pstate == 4) pickup_ok = 0;
    }

    memset(s_stat_state, 0, sizeof s_stat_state);
    for (i = 0; i < s_n_spirit; ) {
        trance_spirit_t *p = &s_spirit[i];

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
        ++p->size_age;      /* 大小自己的年龄（进/出灵界会被清零）*/
        --p->life;
        /* ② 每帧增量自检（只在头 `TRANCE_AGE_LOG_MAX` 颗、且落在关键帧上时打一行）。*/
        trance_age_probe(p);
        if (p->state > 0 && p->state < 8) ++s_stat_state[p->state];   /* 诊断：本帧 state 分布 */

        /* ① **到点消失**（裁决 3）：生命归零 ⇒ 播载体脚本的消失段（interrupt 1 = 放大 + 换贴图后自删）
         *    并置 `dying`（**不再收、不再写外观**；脚本删掉自己之后的下一次 tick 才回收槽）。
         *    一手：TH13 也是到点调 `anm_set_interrupt_by_id(vm, 1)`（`th13:0x438a61`）。*/
        if (trance_spirit_expired(p->life)) {
            /* ★ 两种态**收尾方式不同**（一手 anm 原文）：
             *   普通态父脚本有 `interruptLabel(1)` = 40 帧缩小淡出后自删 ⇒ 发中断 1、置 `dying` 等它自删；
             *   灵界态那组**根本没有 interruptLabel**（一手：灵界态不缩不淡）⇒ 发中断等于什么都不做，
             *   必须**直接删 VM**（否则它永远挂在那儿）。*/
            if (p->style) {
                ce_anm_delete((uint32_t)p->id);
                ++s_stat_spirit_expired;
                spirit_remove(i);
                continue;
            }
            ce_anm_interrupt((uint32_t)p->id, 1);
            p->dying = 1;
            ++s_stat_spirit_expired;
            /* ★ 寿命取证（口径 #5）：到期那一帧把 **age / life / 帧号** 打出来（限前 6 次）。
             *   判据：`age` 必须 == `TRANCE_SPIRIT_LIFE_FRAMES`（480，一手 `th13:0x438a61` 的 `0x1e0`），
             *   之后脚本自己再走 40 帧的消失段（一手 `astral.anm` script1 的 `+40: delete()`）
             *   ⇒ 「出生 → 看不见」= **520 帧**（8.67 s @60fps）。*/
            if (s_stat_expire_log < 6) {
                ++s_stat_expire_log;
                ce_log("trance: spirit EXPIRED at age %d (life %d, sdk_frame %u) -> interrupt 1; "
                       "script tail +%d f => visible life about %d f (一手 480 + 40)",
                       (int)p->age, (int)p->life, (unsigned)ce_sdk_frame(),
                       TRANCE_SPIRIT_SCRIPT_TAIL_FRAMES,
                       (int)(TRANCE_SPIRIT_LIFE_FRAMES + TRANCE_SPIRIT_SCRIPT_TAIL_FRAMES));
            }
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
                trance_on_spirit_collected(c, p->type, p->x, p->y, p->born_trance);
                /* ★★ 2026-09-13 实跑订正：**收集那一刻神灵本体没有任何动画** —— 一手收集分支
                 *   （`th13:0x438b17` 一带逐条）只做四件事：
                 *     ① 播音（`SoundManager__play_sound_at_position`，`th13:0x438b38`）
                 *     ② 槽 +2（`FUN_0043d530(&CURRENT_SPIRIT_GAUGE)`；早苗 type 1 再 +1）
                 *     ③ 起一颗 **`front.anm` 数字弹窗 VM**（`FUN_0046f820(&out, script, spirit, 0, -1, 0)`，
                 *        script = type0 `0x60/0x61`、type1 `0x66/0x67`、type2 `0x63/0x64`）
                 *     ④ **`return -1`** ⇒ 上层**立刻释放这个槽**（VM 当场销毁，`FUN_00418330`：
                 *        置 `vm+0x1c` bit5 ⇒ 下一帧 `AnmManager__sub_46f690` 摘链回收）
                 *   ⇒ **"不是硬消失"指的是 ③**：玩家看到的反馈是"数字弹窗 + 音效 + 槽涨"，
                 *     而不是神灵自己播一段被吸进去的动画（**原版没有那一段**，别再加）。
                 *   ⚠ 旧实现拿**到期**用的 `interrupt 1`（40 帧 `scaleTime(40,1,0,0)` 缩小）当收集动画
                 *     ⇒ 用户看到的是"缩小"（实跑第 1 条）；改成"直接删 VM"后用户又指出少了反馈
                 *     ⇒ 本轮补上 ③（弹窗在 `trance_on_spirit_collected` 里起）。
                 *   VM 自己 54 帧后 `delete()`，不进池（删卡那一刻的收尾见 `trance_clear()` 的弹窗段）。*/
                ce_anm_delete((uint32_t)p->id);
                ++s_stat_spirit_picked;
                spirit_remove(i);
                continue;
            }
        }

        /* ③ ★★ **运动**（2026-09-13 落地；一手 `th13:0x438730` 的 state 机 + `th13:0x43919a` 的初速）
         *    —— 这一段把当年的占位行为（"停在原地 + ±1 px 方波"）换成了原版那套：
         *      · **漂移**：沿当前方向走，速度每帧 `−0.12666667`、地板 `0.13`；
         *      · **被吸**：方向改成**指向自机**，速度每帧 `+0.3`、上限 `12.0`；
         *      · **灵梦 + 灵界态**：任意距离 `+0.05/帧`、上限 `4.0` 的慢吸（一手 state 4）；
         *      · **出界删除**：出框 x∈[−224,224]、y∈[−32,480] 就删（一手 `th13:0x438a8a`）。
         *    ★ **没有重力**（一手位置积分走引擎通用 `PosVel`）⇒ 用户裁决 1「不下落」仍然成立。
         *    ⚠ 位置写在**父** VM 上：环 / 内芯是它的子 VM（`scriptNew`），child 出生时 `entity_pos` 清 0、
         *      靠父指针在渲染期挂进父的坐标系（一手 TH16 字段图 `engine/anm/th16/08-child.md` §3）
         *      ⇒ 移动父就整组跟着走。🟡 TH18 未逐字节复核（TH13 原版就是这么动的）。*/
        {
            const float px = pp ? pp[0] : 0.0f;
            const float py = pp ? pp[1] : 0.0f;
            const float ddx = px - p->x;
            const float ddy = py - p->y;
            const int32_t dist2 = (int32_t)(ddx * ddx + ddy * ddy);   /* 只需量级：与 96²/128²/112² 比 */
            const int32_t st_new = trance_spirit_state_step(p->state, dist2, p->style, is_reimu);

            p->state = st_new;
            if (st_new == TRANCE_STATE_ATTRACT || st_new == TRANCE_STATE_REIMU_SLOW) {
                /* 方向 = 指向自机（一手：角度指向自机；这里存**单位向量**，避免三角函数）*/
                float len = ddx * ddx + ddy * ddy;
                if (len > 0.0001f) {
                    /* `__builtin_sqrtf` 在 `-msse2 -fno-math-errno` 下就是一条 `sqrtss`
                     * （不引 libm、不走 x87）—— 这里只要单位向量，不需要角度。*/
                    const float inv = 1.0f / __builtin_sqrtf(len);
                    p->dx = ddx * inv;
                    p->dy = ddy * inv;
                }
                if (st_new == TRANCE_STATE_ATTRACT) {
                    p->speed += TRANCE_SPIRIT_ACCEL;
                    if (p->speed > TRANCE_SPIRIT_SPEED_MAX) p->speed = TRANCE_SPIRIT_SPEED_MAX;
                } else {
                    p->speed += TRANCE_SPIRIT_REIMU_ACCEL;
                    if (p->speed > TRANCE_SPIRIT_REIMU_MAX) p->speed = TRANCE_SPIRIT_REIMU_MAX;
                }
            } else {
#if TRANCE_SPIRIT_MOTION_TH17
                /* ★★ 2026-09-13 第三轮（用户口径：「灵的移动按原版 **TH17** 做，我的印象也不一定对」）：
                 *   自由运动改用 **TH17 動物霊**那套（一手 `engine/animal/th17/01-animal-spirits.md` §4.2，
                 *   `th17:0x410af0`）：
                 *     · **不衰减**（TH13 是每帧 −0.12666667、地板 0.13 ⇒ 越飘越慢、最后像在原地蠕动，
                 *       用户"灵会随机移动吗"的观感就来自这里）；
                 *     · **定速直线飞**，速度下限 `|vel| < 0.2` 时重掷 `±(0.5 + rand×0.3)`（一手 `0x4a3b04`）；
                 *     · **在框上反弹**：x ≤ −180 / x ≥ 180 / y ≤ 128 / y ≥ 384 翻对应分量
                 *       （一手 `0x4a3e8c` / `0x4a3d48` / `0x4a3d38` / `0x4a3d88`）。
                 *   ⚠ TH17 的 60 px 减速（`vel × 0.5`）在这里用不上：我们保留 TH13 的吸引（96 px 起吸，
                 *     那是用户 2026-09-13 早先点名要的）⇒ 60 px 时早就进"被吸"态了。
                 *   要回到 TH13 的衰减模型 ⇒ `TRANCE_SPIRIT_MOTION_TH17` 置 0（两套都留着，可 A/B）。*/
                if (p->speed < 0.2f)
                    p->speed = 0.5f + (float)(ce_rand() % 300u) * 0.001f;
#else
                p->speed -= TRANCE_SPIRIT_DRIFT_DECAY;
                if (p->speed < TRANCE_SPIRIT_DRIFT_MIN) p->speed = TRANCE_SPIRIT_DRIFT_MIN;
#endif
            }

            p->x += p->dx * p->speed;
            p->y += p->dy * p->speed;

#if !TRANCE_SPIRIT_MOTION_TH17
            /* ★★ **TH13 的墙反弹**（一手 `th13:0x438800` 一带 + `FUN_0040bb70(pos,0,224,384,448)`）：
             *   出 **x∈[−192,192] / y∈[0,448]**（= 游玩区）就翻对应速度分量。
             *   ⚠ 一手注：这条只对 `flags2` bit1 置位的神灵生效，而 **bit1 是生成时置位的**
             *     ⇒ 实际等价于"所有神灵都在游玩区里弹"。**不是 TH17 的 ±180/128..384**（那是上一版
             *     我照 TH17 写的，用户 2026-09-13 更正：「th17 是动物灵、th13 是神灵，不是一个东西」）。*/
            /* ⚠ 符号别写反：左墙是 −X、右墙是 +X（上一版我把两条都写反了 ⇒ 每帧都贴边 + 抽搐，
             *   用户实跑就是这么看到的："生成一瞬间贴到左右边界上，也在抽搐"）。*/
            if (p->x <= -TRANCE_SPIRIT_BOUNCE_X)       { p->x = -TRANCE_SPIRIT_BOUNCE_X;       p->dx = -p->dx; }
            else if (p->x >= TRANCE_SPIRIT_BOUNCE_X)   { p->x =  TRANCE_SPIRIT_BOUNCE_X;       p->dx = -p->dx; }
            if (p->y <= TRANCE_SPIRIT_BOUNCE_Y_LO)     { p->y =  TRANCE_SPIRIT_BOUNCE_Y_LO;    p->dy = -p->dy; }
            else if (p->y >= TRANCE_SPIRIT_BOUNCE_Y_HI){ p->y =  TRANCE_SPIRIT_BOUNCE_Y_HI;    p->dy = -p->dy; }
#endif

            /* 出界删除（一手框）—— 与"到期"同样回收槽，但不播消失段（它已经飞出去了）*/
            /* ★ 2026-09-13 第五轮（用户实跑：「神灵消失的速度似乎有点快」）：**VM 先没了**要留证据 ——
         *   我们的池按 `TRANCE_SPIRIT_LIFE_FRAMES`（一手引擎侧 480 帧）走，但**anm 脚本自己也可能
         *   提前 `delete()`**（一手 script1 的 `+200: scaleTime(...) ... +40: delete()`）。
         *   两者哪一个先到，实跑日志的这行会直接回答（`age` = 第几帧没的）。*/
        if (!ce_anm_get_vm((uint32_t)p->id)) {
            if (s_stat_spirit_vm_gone < 4) {
                ++s_stat_spirit_vm_gone;
                ce_log("trance: spirit VM %d gone EARLY at age %d f (pool life %d f) - "
                       "anm 脚本自己 delete 了？见 AUDIT §CI-8", (int)p->id, (int)p->age,
                       (int)p->life);
            }
            spirit_remove(i);
            ++s_stat_spirit_expired;
            continue;
        }
        if (trance_spirit_out_of_field((int32_t)p->x, (int32_t)p->y)) {
                ce_anm_delete((uint32_t)p->id);
                ++s_stat_spirit_out;
                spirit_remove(i);
                continue;
            }
            ce_anm_set_pos((uint32_t)p->id, p->x, p->y, 0.0f);

            /* ★ 大小（口径 #19；基准 1.0 —— 实跑订正 #4）：每帧写，脚本那句 1.5 被我们盖住。*/
            {
                float sc = (float)trance_spirit_scale_permille(p->size_age) / 1000.0f;
                ce_anm_set_scale((uint32_t)p->id, sc, sc);
            }
        }

        /* ④ **不断变小**（裁决 3）—— 现在由**脚本自己**做（普通态父 `+200: scaleTime(200,0,0.4,0.4)`，
         *    一手 `astral.anm` script1 原文）⇒ 卡侧**不再每帧写 scale**（写了也会被插值器盖掉）。
         *    core 里的 `trance_spirit_scale_permille()` 保留为**一手曲线的记录 + 单测对象**。*/

        ++i;
    }
}

/* ★ 魔理沙的灵界加成之二：**移速**。
 * 为什么必须是 `on_tick_2`：`CE_PLAYER_SPEED_MULT` 由引擎在 Player tick 末尾复位成 1.0，
 * 而 AbilityManager tick（`on_tick_2` 所在那条线）**先于** Player tick ⇒ 只有在这里乘才生效
 * （`engine.h:286` 的一手注：「AbilityManager tick(0x16) 先于 Player(0x17) → on_tick_2 里写生效」）。
 * 写法照先例 `cards/season.c` / `cards/sj.c`：**`*=`，不赋值** ⇒ 与别的卡可交换。
 *
 * ★ 2026-09-13（原生卡冲突审计 C 通道）**与原生 29 Aya（SPEEDQUEEN）不冲突**，一手为证：
 *   `CardAya__on_tick` `th18:0x40c7f0` 写的是**方向速度**两格 —— `player+0x477b4`（直线，`0x40ca81`
 *   写 `0x400` = 1024/256 = 4.00 px/帧）与 `player+0x477bc`（斜向，`0x40ca8b` 写 `0x2d4` = 724/256），
 *   以及 `player+0x4779c` bit 0x200 那条分支里从 `sht+0x10/+0x18` 取的速度（`0x40c816`–`0x40c844`）。
 *   **全函数没有一处碰 `player+0x477ec`**（grep 过：`0x477ec` 只出现在本卡的这行与 59/116）。
 *   而引擎的移动函数在最后一步把它当倍率乘上去（`th18:0x45b170` 末段 `* *(float*)(p+0x477ec)`；
 *   见 `mods/th18.v1.00a/mouse-control/TARGET.md` §目标字段 6/8）。
 *   ⇒ 两家**结构上相乘**（Aya 给基础步长、我们给倍率），谁也吃不掉谁 —— 不需要任何改动。*/
/* ★★ 2026-10-02（Jera 88「杰拉」）：本卡的**神灵**交给 SDK 的「复制代数闸」——
 * 每帧上报可见数，代数一变就照原档重撒（`trance_spirit_spawn` 是它唯一的出生口）。
 * ⚠ 两处口径：
 *   ① `core` 传 **NULL**（照 `trance_spirit_restyle()` 的先例）—— 复制**不是**原版的"出生事件"，
 *      传 core 会推进连锁（凭空造白灵）；这是 ❓ 口径，登记在 `TUNING.md` §2.10。
 *   ② 方向不用管：`trance_spirit_spawn()` 自带避让（TH17 那条 24 px 同向 +225° 重掷）⇒ 复制品自动散开。
 * 语义见 `sdk_core.h` 末节与 `cards/jera.c` 文件头。*/
static uint32_t s_dup_seen;

static int trance_dup_spirit_count(void)
{
    int i, n = 0;
    for (i = 0; i < s_n_spirit; ++i) if (s_spirit[i].id && !s_spirit[i].dying) ++n;
    return n;
}

static void trance_dup_run(void)
{
    trance_spirit_t snap[TRANCE_SPIRIT_POOL];
    int n = s_n_spirit, i, made = 0;
    if (n <= 0) return;
    memcpy(snap, s_spirit, sizeof snap[0] * (size_t)n);
    s_spirit_spawn_silent = 1;               /* 复制不该再响一遍出生音（照 restyle 的先例）*/
    for (i = 0; i < n; ++i) {
        if (snap[i].dying || !snap[i].id) continue;
        trance_spirit_spawn(snap[i].x, snap[i].y, snap[i].type, snap[i].born_trance, NULL,
                            TRANCE_SPIRIT_SPAWN_SPEED);
        ++made;
    }
    s_spirit_spawn_silent = 0;
    ce_log("trance: jera dup %d/%d spirits", made, n);
}

static int on_tick_2(ce_card_t *c)
{
    trance_card_t *s = st(c);
    uint8_t *pl;

    if (!s) return 0;
    ce_dup_publish((uint32_t)ce_card_id(c), trance_dup_spirit_count());
    if (ce_dup_should_run(&s_dup_seen)) trance_dup_run();
    pl = CE_PLAYER();
    if (!pl) return 0;

    /* ★★ 2026-09-13 第五轮：**补死在这里落地**（道具帧只挂 `redie_pending`）。
     *   一手：引擎自己的三处 `Player__die` 调用点全在 `Player::on_tick` 里
     *   （`0x45cd0d`/`0x45ce69`/`0x45d046`），而本钩子就在玩家 tick 这条线上；
     *   `Player__die` 会建 VM（死亡特效）⇒ 不能在道具帧里调（`AUDIT` §AL55 纪律）。*/
    if (s->redie_pending) {
        const int32_t pstate = *(int32_t *)(pl + CE_PLAYER_STATE);
        if (pstate == 1) {
            s->redie_pending = 0;
            trance_player_die();
            ++s->stat_redie;
            ce_log("trance: re-die EXECUTED at the player tick (frame %d, state %d) — Player__die called; "
                   "chaining is allowed (107/117/113 may rescue this death again)",
                   (int)s->frame, (int)pstate);
        }
    }

    /* ★ 灵梦的判定点（每帧写 —— `Player__reset` 会在关卡开始/复活时覆盖回原生值，写一次留不住）
     *   ⇒ 用**目标值**写，不用"乘一次"：不管现在是原生还是已被缩过，每帧都回到目标值（自愈）。
     *
     * ★★ 2026-09-13（原生卡冲突审计 D 通道）**这六个字段不只有"判定点"一个身份**，一手两条：
     *   ① **引擎的擦弹判定读同一批字段**（逐条反汇编，不是推断）：`FUN_004248a0`（子弹更新里的擦弹分派）
     *      按 `bullet.flags` 选两条路 ——
     *        · 圆模式 → `th18:0x45cd90`：`0x45cdd0 MOVSS XMM1,[ECX+0x479a0]`（聚焦）/
     *          `0x45cdda MOVSS XMM1,[ECX+0x4799c]`（非聚焦）= 判定圆半径 `r`（`ECX` = zPlayer）；
     *          随后 `0x45ce13 DIVSS XMM3,[0x4b921c]`（÷2.5）、`0x45ce1b …[0x4b92c4]`（40.0）、`MAXSS`、
     *          `ADDSS XMM0,XMM1` ⇒ **判据 `d² < (r + max(br/2.5, 40))² + br²`**（另有近档 `d² < r² + br²`）。
     *        · 矩形模式 → `th18:0x45cc20`：`0x45cc48/0x45cc50` 读 `[EDX+0x479b0]/[EDX+0x479b4]`（聚焦）、
     *          `0x45cc5a/0x45cc62` 读 `[EDX+0x479a4]/[EDX+0x479a8]`（非聚焦），`0x45cd37` 取外扩常量
     *          `[0x4b92a4]` = **24.0**。
     *      ⇒ 灵梦把 `r` 从 3.0 缩到 2.0 时，**有效擦弹半径也跟着小 1.0**（br = 4–8 时 43 → 42，
     *      面积 ≈ **−4.6%**；矩形模式同理）⇒ 106/107/110/111/115/116 六张读擦弹计数的卡一起掉收益。
     *      这是复刻 TH13 机体特技的**对称后果**（不是 bug），但它跨了 6 张卡。
     *   ② **原生 29 Aya 也写这三个字段**（`CardAya__on_load` `th18:0x40c7b5`：`0x4799c = 1.0`、
     *      `0x479a4 = 0x479a8 = 0.5`，只写非聚焦那一套，`on_load` 一次）。两家都是"变小"，
     *      而**本卡每帧写目标值** ⇒ Aya 那一次被顶回去（灵梦时 1.0 → 2.0 = **判定点变大**）。
     *   ★★ 2026-09-13 **用户拍板：方案②「比例相乘」**（同类效果叠加，见 `CLAUDE.md` 的六问与
     *      `CARD-NATIVE-CONFLICT-AUDIT.md` §D）⇒ 本卡改成**相对写**：
     *        ① 记下"我们上次写进去的值" `s_hb_last_*`；
     *        ② 每帧读当前值：**若与上次不同 ⇒ 说明有别的卡插了一脚**，算出它留下的倍率
     *           `ext = 当前值 / 原生值`（Aya 就是 1.0/3.0 = 1/3）并**粘住**；
     *        ③ 我们写 `原生值 × ext × 我们的比例`（灵梦 2/3）⇒ 与 Aya 同时在场 = `3.0 × 1/3 × 2/3 = 0.667`
     *           （判定点比任何单装都小 —— 这就是"比例相乘"的语义）；
     *        ④ 别的卡把值改回原生（`3.0`）时 `ext` 自动回到 1 ⇒ 我们回到 2.0（自愈仍然保留）。
     *      ⚠ 不是"每帧乘一次"（那会滚到 0）：比例作用在**原生基线**上，外部倍率只**探测一次**再粘住。*/
    {
        const trance_trait_t *tr = trance_traits(trance_character());
        /* 六个字段各自的（原生基线, 我们的比例）—— 一手：TH13 灵梦 2.0/3.0、半宽 1.0/1.5 */
        static const struct { uint32_t off; float base; } fld[6] = {
            { CE_PLAYER_HITBOX_R_NF,  TRANCE_HITBOX_R_NATIVE },
            { CE_PLAYER_HITBOX_R_F,   TRANCE_HITBOX_R_NATIVE },
            { CE_PLAYER_HITBOX_HW_NF, TRANCE_HITBOX_H_NATIVE },
            { CE_PLAYER_HITBOX_HH_NF, TRANCE_HITBOX_H_NATIVE },
            { CE_PLAYER_HITBOX_HW_F,  TRANCE_HITBOX_H_NATIVE },
            { CE_PLAYER_HITBOX_HH_F,  TRANCE_HITBOX_H_NATIVE },
        };
        const float k_r = tr->small_hitbox ? (TRANCE_HITBOX_R_SMALL / TRANCE_HITBOX_R_NATIVE) : 1.0f;
        const float k_h = tr->small_hitbox ? (TRANCE_HITBOX_H_SMALL / TRANCE_HITBOX_H_NATIVE) : 1.0f;
        int i;
        for (i = 0; i < 6; ++i) {
            float *p = (float *)(pl + fld[i].off);
            const float k = (i < 2) ? k_r : k_h;
            const float was = *p;
            if (k >= 1.0f) continue;                  /* 不是"变小"的机体 ⇒ 一个字都不写（别跟原生/别人抢）*/
            if (!s_hb_have[i] || *p != s_hb_last[i])  /* 有人插了一脚（或第一次）⇒ 认它留下的倍率 */
                s_hb_ext[i] = *p / fld[i].base;
            *p = fld[i].base * s_hb_ext[i] * k;
            s_hb_last[i] = *p;
            s_hb_have[i] = 1;
            /* ★ 诊断（用户口径："比例相乘"怎么验）：只在**有效值变了**的时候打，且限流。
             *   期望：灵梦单装 ⇒ base 3.0 × ext 1.0 × k 2/3 = **2.0**；
             *         与原生 29 Aya 同装（它写 1.0）⇒ ext 1/3 ⇒ **0.667**（比单装更小）； */
            if (was != *p && s_hb_log_n < 12) {
                ++s_hb_log_n;
                ce_log("trance: hitbox fld%d base %.2f ext %.3f k %.3f -> %.3f (was %.3f)",
                       i, (double)fld[i].base, (double)s_hb_ext[i], (double)k, (double)*p, (double)was);
            }
        }
    }

    if (!trance_traits(trance_character())->marisa_trance_buff) return 0;   /* 下面只有魔理沙那条 */
    if (s->core.phase == TRANCE_PHASE_IDLE) return 0;                       /* 「在灵界」才算 */
    *(float *)(pl + CE_PLAYER_SPEED_MULT) *= TRANCE_MARISA_SPEED_MUL;
    return 0;
}

/* ============================ 每帧（游玩帧） ============================ */

/* ============================ 主动卡发动（`on_activate` / 用户的 C 键） ============================
 * 用户 2026-09-12 定案：**灵界是主动触发的** —— 「用 D 键切换到该卡，然后按 C」，
 * 与 116（天空璋）同一套契约。⇒ 本卡声明 `active_recharge`（= 主动卡），
 * 真正的门槛留在 `on_activate` 里：**灵界槽不满 / 已在灵界 ⇒ `ce_refuse`**
 * （SDK 看到 `CE_ACTIVATE_REFUSED` 会把刚装填的充能退回、**不算发动**，卡侧自己放无效音）。
 *
 * ⚠ `active_recharge = 1`（不是真冷却）：先例 = 117 咆哮（`cards/animal.c` 的 `hyper_begin`），
 *   它的代价是**动物灵槽**而不是时间 ⇒ 充能只做「SDK 主动卡机器的入场券」。
 *   本卡的代价同样是槽（600 = 满）⇒ 同一形状。 */
static int on_activate(ce_card_t *c)
{
    trance_card_t *s = st(c);
    trance_cond_t cond;

    if (!s) return ce_refuse("TD_SPIRIT", "no state slot");
    if (s->core.phase != TRANCE_PHASE_IDLE) return ce_refuse("TD_SPIRIT", "already in trance");
    trance_fill_cond(s, &cond);
    if (!trance_can_activate(&s->core, &cond)) {
        ce_log("trance: refuse (C) - gauge %d/%d, conditions not met (SPEC §9 #1 的 8 条)",
               (int)s->core.gauge.gauge, TRANCE_GAUGE_MAX);
        return ce_refuse("TD_SPIRIT", "gauge not full");
    }
    if (!trance_begin(&s->core, 0, &cond)) return ce_refuse("TD_SPIRIT", "begin refused");
    /* ★ 当场开「中弹无伤」闸门 —— **不能等下一帧的按相位声明**：
     *   一手 TH13 是在 `TranceManager::begin` **里面**同步置的标志，所以同一帧的后续命中当场就被挡住。
     *   我们要是不在这里开，同帧的第二枪会走完整条死亡流程（声明那一步在下一个 `on_item_frame`）。*/
    ce_no_hit_declare(TRANCE_NUM, 1);
    /* 灵界进入音（一手 `th13:0x40586d MOV EDI,0x34` = `se_boon01`，`th13:0x40587a` 调
     * `SoundManager__play_sound_at_position`）。★ 声像 = **自机 x**：一手在 `th13:0x405867`
     * 就是 `fld [PLAYER+0x5b8]` 之后 `fstp [esp]` 当 x 传进去的（旧注释里那个 ❓ 已消）。*/
    ce_play_sound(TRANCE_SE_ENTER, ce_player_x());
    ce_log("trance: enter (C, manual), gauge %d, frame %d", (int)s->core.gauge.gauge, (int)s->frame);
    return 0;                               /* 瞬发：窗口由 `trance_tick` 自己推 */
}

/* ============================ ★ 中弹救援（救援层的**最后一家**，2026-09-12 第二版） ============================
 *
 * 用户口径（唯一权威）：
 *   「灵界有两个触发方式，一个是手动触发；一个是**在确定中弹后，取消掉死亡，进入灵界状态；
 *     灵界状态退出后，重新死亡**。」
 *   ＋ 「113 这条机制能够确定说，它一定在**救援层的最后**出发，**甚至比决死 B 还靠后**」
 * ⇒ 层内顺序 **107 → 117 → 决死 B → 113**。
 *
 * ## 落点 = 卡虚表 **`+0x0c`** `on_death_after_deathbomb`（引擎自带的那条救援钩子）
 *
 * 一手（`Player__on_tick__body` `th18:0x45be90` 的 case 4，入口 `th18:0x45c0f4`）：
 *
 * ```c
 * 0x45c0f4: eax = player->state4_timer(+0x638)
 * 0x45c0fa: cmp eax, player->num_deathbomb_frames(+0x47908)   // = 8（0x45d557 置）
 * 0x45c100: jl  0x45c2a9      // ★ 窗口没过 ⇒ 决死 B 那条路（can_bomb 0x420420 → do_bomb 0x420360
 *                             //   → cancel_impending_death 0x45d590，0x45c2d7）
 * 0x45c112: edi = 0           // ★ edi 从「player」改成「累加器」
 * 0x45c114: 循环 push 0(acc); call [vtbl+0xc]; or edi, eax     // ★★ 就是本条路（参数 = 累加器）
 * 0x45c12d: jne 0x45c3d1      // ★★ 任一卡返回非 0 ⇒ **跳过扣命**
 * 0x45c135: call 0x45d090     // 否则**同一 tick 内立刻 commit_death**（扣命）
 * ```
 *
 * ⇒ 这条一起给出了两件事：
 *   ① **顺序**：`+0x0c` 只在「决死窗口走完 + B 键那条路没救成」之后才广播 ⇒ 用它就**结构性**
 *      排在 107/117（走 `+0x10`，在 `Player__die` 里就出手）与决死 B **之后**；
 *      不需要数帧、也不需要猜时间点。
 *   ② **落点**：窗口关掉的那一帧**没有**「窗口已关但命还没扣」的中间态（同一条分支里
 *      要么被某个 `+0x0c` 挡住、要么当场扣命）⇒ **「帧点等 9 帧」那条路不可达**：
 *      `+0x0c` 在 Player 线 `0x17` 里广播，而 `on_item_frame` 在 ItemManager 线 `0x1e` 里跑
 *      ⇒ 后者跑到时 state 早不是 4。**第一版实现就是错在这里**（`AUDIT` §BN7 留痕）。
 *
 * ## 两道门 + 两个必做动作（**顺序不能改**）
 *
 *   ⓪ ★★ **2026-09-13 新增（原生卡冲突审计 A 通道）：两条「别抢别人的救」门** ——
 *      ① `acc != 0` ⇒ 这次死亡**已经被前面的卡取消**（`+0x0c` 的形参就是引擎的累加器，
 *         一手 `th18:0x45c116 push edi` / `0x45c121 or edi,eax`）。这是**引擎自己定的契约**：
 *         原生 `CardEirin__on_player_death_after_deathbomb` `th18:0x40a4f0` 的**第一条指令**
 *         就是 `cmp dword [ebp+8],0 / jnz 0x40a54d`（让位、返回 0）。我们以前**不看它** ⇒
 *         原生 23/35 已经救下时 113 还会再烧一次灵界槽（一次死亡两份代价）。
 *      ② 原生救援者**还会不会出手**（`trance_native_rescue_would_fire()`，判据逐条照抄原生）⇒
 *         会让位。为什么必须让：**原生 35 `CardShikiEiki__on_player_death_after_deathbomb`
 *         `th18:0x40da10` 不看 `acc`**（它只查金 ≥ 200 就扣钱返回 1）⇒ 我们若抢先救，
 *         它照样扣掉 200 金（代价白付）。23 虽然看 acc，但按「113 是救援层最后一家」的口径
 *         也一并让位。
 *   （下面两道是原有的）
 *
 *   ① `ce_rescue_note_owner()`：`>= 0` 且不是 113 ⇒ **这次死亡已经有人救了** ⇒ `return 0`。
 *      ⚠ 这一档**不需要帧闩锁**：`+0x10`（`Player__die` 内，`0x45d570`）**先于** `+0x0c`（`0x45c114`）
 *        在同一 tick 里跑完 ⇒ 107/117 写下的 owner 在本刻仍是本帧的值。
 *   ② `trance_force_on_death()`：槽 < 200 ⇒ 失败（一手 `th13:0x405a40` 同一道门）⇒ `return 0`（照常死）。
 *      ★ 它的返回值**同时**是两件事：① 这次死亡被不被吃下；② 补死的欠账记不记
 *        —— 返回 1 ⇒ 相位变 `TRANCE_PHASE_DEATH_FREE`（或手动窗被**升格**成它）⇒ 窗口结束时
 *        `trance_tick` 就会报 `re_die`。
 *   ③ **`Player__cancel_impending_death()`**（`th18:0x45d590`）—— 取消这次死亡。
 *   ④ ★ **`return 1`** —— 让引擎跳过 `0x45c135` 的扣命。**这两件缺一不可**：
 *      只调 `0x45d590` 不返回 1 ⇒ 引擎照样 `commit`（state 被拉回 1 也没用，命已经扣了）；
 *      只返回 1 不调 `0x45d590` ⇒ 引擎不扣命，但 state 仍是 4 ⇒ 下一帧照样进死亡流程。
 *      一手原生先例：`CardEirin__on_player_death_after_deathbomb` `0x40a4f0`
 *      （`0x40a53f call 0x45d590` → **`0x40a544 mov eax,1`** → `0x40a54a ret 4`，stdcall 1 个 dword）。
 *
 * ## 为什么不在 `+0x0c` 里判「自机 state 是否仍是 4」
 *
 * 引擎广播 `+0x0c` 时 state 本来就是 4（它自己那一帧就是这么进来的），而 `0x45c114` 的循环
 * **不看**前一张卡的返回值就继续广播 ⇒ 若前面已有卡取消过死亡，state 会变成 1 ——
 * 那种情形已经被**门 ①**（`ce_rescue_note_owner()`）挡住（那张卡会 claim），
 * 所以本卡不需要再自己读 state（少一次引擎面读取，也就少一个会漂的判据）。
 *
 * ## 死亡栈纪律
 *
 * `+0x0c` 是**死亡栈**（在 `Player__die` 的调用链里）：本函数只做
 * 「读几个字段 + 调两个引擎函数 + 写私有状态 + 一行日志」。**不建 anm VM、不 `ce_spawn_*`、不扫链表**；
 * `ce_play_sound` 可以（107/117 在 `+0x10` 里就播）。
 * ---------------------------------------------------------------------------------------------- */

/* ⚠★ **必须带 `thiscall` 属性**：这两个函数都是 `ecx = zPlayer*` 的成员函数。
 * 写成裸函数指针（默认 cdecl）会把参数压栈、`ecx` 留着上一次的残值 ——
 * 那是一处**只在实跑时才炸**的 ABI 错（2026-09-13 复核时发现本行原来漏了属性；
 * 本仓同类调用全都带属性，见 `sdk.h` 的 `ce_fn_timer_t` / `cards/senro.c:243`）。*/
typedef void (__attribute__((thiscall)) *trance_fn_player_t)(void *player);

/* 取消这次死亡（`Player__cancel_impending_death` `th18:0x45d590`）。照 `cards/senro.c` 的
 * `senro_cancel_impending_death()` 的形状（同样是 `CE_PLAYER()` 判空后再调）。
 * ⚠ 它是 **thiscall**：`ecx` = `zPlayer*`，无栈参，返回 void ⇒ 只能用内联汇编传 `ecx`。*/
static void trance_cancel_impending_death(void)
{
    void *p = (void *)CE_PLAYER();
    if (!p) return;
    ((trance_fn_player_t)(uintptr_t)TRANCE_FN_CANCEL_IMPENDING_DEATH)(p);
}

/* ★ **补死**：灵界窗口结束时把「欠的那次死亡」补上（一手 `th13:0x405c3e` 的同一动作）。
 * 同 ABI（thiscall(Player*)、无栈参、void）。调用点只有一处：`on_item_frame` ③。*/
static void trance_player_die(void)
{
    void *p = (void *)CE_PLAYER();
    if (!p) return;
    /* ★★ **先撤闸门再补死**（否则补死会被我们自己的闸门挡掉）：
     *   灵界里的「中弹无伤」= 在 `Player__die` 入口挂了一个断点（`BP_ce_player_die`），
     *   闸门开着就整段 die 体不跑 —— 而**补死**正是要 die 真的跑一遍。
     *   撤在这里而不是靠"相位已经回 IDLE"：声明是**粘性**的（见 `sdk_core.h` 的 `ce_no_hit_declare`），
     *   本帧稍后 `on_item_frame` 才会按相位把它写回 0 ⇒ 现在必须显式撤。
     *   一手依据：`engine/player/th18/03-player-hit-gate.md` §6 (a) 的风险 2。*/
    ce_no_hit_declare(TRANCE_NUM, 0);
    /* ★★ 2026-09-16u（用户口径）：「罩子原则上是**撞弹**的保护 ⇒ 不应当能抵消中弹灵界的
     *   **结束灵界击杀**」⇒ 这次补死要在 `+0x10` 广播里**通报一声「这是卡自己造的死」**，
     *   让 111 的河童罩子（以及将来任何「中弹瞬间」类钩子）让位。
     *   ⚠ 通报位是**同帧一次性**的（不是粘性）：`begin` / `end` 必须成对包住这一调。*/
    ce_forced_death_begin();
    ((trance_fn_player_t)(uintptr_t)TRANCE_FN_PLAYER_DIE)(p);
    ce_forced_death_end();
}

/* ★★ **原生救援者还会不会出手**（2026-09-13，原生卡冲突审计 A 通道）。
 *
 * 一手（逐条照抄，别自己发明判据）：
 *   · **35 ROKUMON**（`CardShikiEiki__on_player_death_after_deathbomb` `th18:0x40da10`）：
 *     第 1 条指令 `cmp dword [0x4ccd34],0xc8 / jl`（金 < 200 ⇒ 不救）⇒ **金 ≥ 200 就一定救**，
 *     而且**完全不看 `acc`**（所以它排在我们后面时会白扣 200 金 —— 这就是我们要让位的原因）。
 *     它开 `+0x0c` 那一帧还顺手写 `player+0x476ac = 1`（自机状态机）与 `+0x638 = 0x3c` 决死窗。
 *   · **23 AUTOBOMB**（`CardEirin__on_player_death_after_deathbomb` `th18:0x40a4f0`）：
 *     `acc != 0` ⇒ 让位 / 有 35 且金 ≥ 200 ⇒ 让位 / `[0x4cf2b8] != 0` 且 `0x420420()`（= `can_bomb`，
 *     一手 `Bomb__can_bomb_and_deathbomb_check`）⇒ 花炸弹救。⇒ 它出手的条件 =
 *     `!（有35 且 金>=200）&& can_bomb()`，且 `acc == 0`（此刻必然，因为门 ⓪-① 已经挡掉 acc != 0）。
 *
 * 返回：会出手的原生卡 id（35 / 23）；**0 = 原生不会出手**（那就轮到 113）。
 * ⚠ 判据**宁可偏保守**：只有当原生**确实会救**时才让位 —— 反过来（误让位）会让本来能救的一命直接死掉。
 * ⚠ `0x420420` 是纯读（查 `CURRENT_BOMBS` / 炸弹管理器 / 自机状态），无副作用，原生自己也是这么调的。*/
static int trance_native_rescue_would_fire(void)
{
    const uint8_t *mgr = CE_ABILITY_MGR();
    int has_rokumon = 0;
    int has_eirin = 0;
    unsigned guard = 0;

    if (!mgr) return 0;
    for (const uint8_t *node = *(const uint8_t *const *)(mgr + CE_MGR_CARD_LIST_FIRST);
         node && guard < 256;
         node = *(const uint8_t *const *)(node + CE_NODE_NEXT), ++guard) {
        const uint8_t *card = *(const uint8_t *const *)(node + CE_NODE_CARD);
        int id;
        if (!card) continue;
        id = *(const int32_t *)(card + CE_CARD_ID);      /* card+0x04 = card_id（原生 0x40a507 同一读法）*/
        if (id == TRANCE_NATIVE_ROKUMON) has_rokumon = 1;
        else if (id == TRANCE_NATIVE_AUTOBOMB) has_eirin = 1;
    }
    /* 35 优先（原生 23 自己也让位给它；且它不看 acc ⇒ 让位是唯一能保住那 200 金的办法）*/
    if (has_rokumon && CE_MONEY() >= TRANCE_NATIVE_ROKUMON_COST) return TRANCE_NATIVE_ROKUMON;
    if (has_eirin && ((int (*)(void))(uintptr_t)TRANCE_FN_CAN_BOMB)() != 0) return TRANCE_NATIVE_AUTOBOMB;
    return 0;
}

/* ★★ **`+0x0c` `on_death_after_deathbomb` —— 113 的中弹救援就是它**（第二版·落点订正版）。
 * 返回值：**1 = 救下了**（引擎据此跳过扣命），0 = 不救、照常死。
 * `acc` = 引擎传进来的累加器（前面各卡返回值 OR 起来，初值 0；一手 `0x45c114` 的 `push edi`）。
 * ★ 2026-09-13：`acc` 从「必须接收但不看」改成**门 ⓪**（非 0 ⇒ 别人已经救过，让位）。*/
static int on_death_after_deathbomb(ce_card_t *c, uint32_t acc)
{
    trance_card_t *s = st(c);
    int owner;
    int native;

    if (!s) return 0;

    /* ★ 门 ⓪-① 别人已经救过（引擎自己的契约：`acc` 是累加器；原生 23 的第一条指令就是查它）。
     *   ⇒ 我们什么都不做：**既不烧灵界槽、也不排补死**（否则一次死亡两份代价）。*/
    if (acc != 0) {
        ++s->stat_rescue_lost;
        ce_log("trance: +0x0c rescue SKIPPED - death already CANCELLED by an earlier card (acc=%u); "
               "113 spends nothing (native contract: CardEirin th18:0x40a4f3 tests the same arg)",
               (unsigned)acc);
        return 0;
    }

    /* ★ 门 ⓪-② 原生救援者还会出手 ⇒ 让位（**113 是救援层的最后一家**）。
     *   为什么不能让：原生 35 不看 `acc` ⇒ 我们抢先救之后它照样扣 200 金（代价白付）。*/
    native = trance_native_rescue_would_fire();
    if (native) {
        ++s->stat_rescue_lost;
        ce_log("trance: +0x0c rescue SKIPPED - native card %d will handle this death "
               "(35: money>=200 and it does NOT check acc; 23: can_bomb) => 113 yields, gauge kept",
               native);
        return 0;
    }

    /* 门 ① 让位：本帧已经有别家认领过这次死亡 ⇒ 本卡什么都不做。
     * ⚠ 口径（2026-09-12 复核，见 AUDIT §BO-4）：107/117 的 claim 发生在**中弹帧 N** 的 `+0x10` 里，
     *    而本回调是**窗口关闭的 N+8 帧**的 `+0x0c`；`ce_rescue_note_*` 又**每帧 reset**
     *    （`sdk.c` 的 `BP_ce_item_frame`，在 ItemManager 线 0x1e 跑）⇒ 帧 N 的 claim **到不了**本帧，
     *    **所以这道门在现行路线下不会触发**（`stat_rescue_lost` 那条分支同理）。
     *    真正保证「113 是救援层最后一家」的是**引擎结构**：107/117 若真救了，死亡当场被取消、
     *    窗口根本不会开 ⇒ 引擎**根本不会叫 113**。
     *    保留这道门的理由：兜住「有人 claim 却没 cancel」的异常路径（那时让位比抢着救更对）。*/
    owner = ce_rescue_note_owner();
    if (owner >= 0 && owner != TRANCE_NUM) {
        ++s->stat_rescue_lost;
        ce_log("trance: +0x0c rescue SKIPPED - death already claimed by card %d "
               "(113 is the LAST in the rescue layer: 107 -> 117 -> deathbomb -> 113)", owner);
        return 0;                    /* 不救命：让救过的那一家生效（它已经 cancel 过了）*/
    }

    /* 门 ② 拿窗口（`trance_begin(core, death_free=1, &cond)`；槽 < 200 ⇒ 失败，一手 `0x405a40`）。*/
    if (!trance_force_on_death(&s->core)) {
        ++s->stat_rescue_lost;
        ce_log("trance: +0x0c rescue FAILED - gauge %d < %d (一手 0x405a40 的门) => death proceeds",
               (int)s->core.gauge.gauge, TRANCE_GAUGE_MIN_ENTER);
        return 0;
    }

    /* 动作 ③ 取消死亡 + 动作 ④ 告诉引擎「我救下了」。顺序：**先确认救得成才 claim**，
     * 再 cancel，最后 return 1（返回 1 是引擎那一侧的唯一判据，见文件头）。*/
    (void)ce_rescue_note_claim(TRANCE_NUM);
    trance_cancel_impending_death();
    ++s->stat_rescued;
    /* ★ 这里**不播音**（2026-09-13 第四轮删掉旧的 `TRANCE_SE_FORCED` = `0x2c` `se_slash`）：
     *   一手 TH13 在"死亡触发的灵界**进入**"处一个音都不播 —— `Player::die` `th13:0x444a10`
     *   只在 `0x444a1d` 调 `TranceManager::begin(…, 1)`，播音 0x34 的是**手动**那条路
     *   （`th13:0x40586d` 在 `TranceManager::on_tick` 里，不在 `begin` 里面）。
     *   玩家此刻听到的"呜"= `Player::on_hit` `th13:0x444ca0` 自己播的 id 2；TH18 侧同位置的
     *   `Player__die` `th18:0x45d3a0` 也已在 `0x45d3b7` 播过它自己的 id 2。
     *   ⚠ 救援广播（卡虚表 `+0x10`）发生在 `th18:0x45d574`，**在那条播音之后** ⇒ 到我们这里时
     *   "呜"已经响过了，再补一个音只会叠。*/
    ce_no_hit_declare(TRANCE_NUM, 1);   /* ★ 同上：救下之后当场开闸门（同帧后续命中不该再走死亡流程）*/
    ce_log("trance: +0x0c RESCUED (last in the rescue layer) - death CANCELLED + return 1, "
           "death-free window opened (phase %d, gauge %d, frame %d); will re-die when it ends",
           (int)s->core.phase, (int)s->core.gauge.gauge, (int)s->frame);
    return 1;                        /* ★ 实质动作：引擎在 `0x45c12d` 用累加器决定扣不扣命 */
}

/* `on_item_frame` = 断点 `0x446ee3`（ItemManager tick 的门卫位次，`sites.py` 一手 disasm）：
 * 正常游玩帧一次；**菜单 / 暂停 / 商店冻结帧不经过**（门卫查的是商店指针 `0x4cf2a4`，
 * 而冻结帧连这条 CMP 都到不了）。
 * ★ 2026-09-13 订正一处**旧注释的过度概括**：原话把「对话」也列进了冻结帧 —— 一手复核
 *   （`engine/spirit/th13/04-dialogue-and-gauge-refund.md` §4）显示**对话期间世界照常 tick**
 *   （`AbilityManager__on_tick__stub` `th18:0x408a90` 只挡商店；引擎自己的中弹判定另有对话门
 *   `th18:0x45ce49`）⇒ 本回调在对话里**照样跑**，这正是第 ④ 条（对话强制退出灵界）能在这里做边沿检测的前提。
 * ⇒ 它就是本卡要的帧源（与 `cards/collect2x.c` 的 `s_play_frame` 同口径，但**存在私有状态里**）。
 * ⚠ 中弹救援**不在**这里（那是卡虚表 `+0x0c` 的事，见上）；本帧点只做：
 * 推窗口 / **补死** / 无敌声明 / 撒神灵 / 收神灵 / HUD / 日志。*/
/* ============================ ★ Boss「随血量减小掉灵」= TH13 通道 A（2026-09-13 第五轮） ============================
 *
 * 用户实跑第 6 条：「Boss（含道中boss）随血量减小没有掉灵」—— 本卡原来只有「击破掉灵」，
 * 而一手 TH13 的**通道 A**（`engine/spirit/th13/01-spirit-and-trance.md` §3.1）走的正是
 * **伤害累计阈值**：
 *
 *   ```c
 *   enemy->+0x3f60 += damage;                       // 累计伤害
 *   if (enemy->+0x3ffc <= enemy->+0x3f60) {         // 越过阈值
 *       if (enemy->+0x4004 > 0x13 && !(+0x4034 & 2)) spawn_spirit(mgr, 1, pos, 2.0, 1);
 *       enemy->+0x3ffc += {200,400,600,1000};       // ★ 阈值**加**增量（不是重置成某个数）
 *   }
 *   enemy->+0x4004 = 30;                            // 冷却
 *   ```
 *   距离分档（`th13:0x419e3a`）：`d² < 48² → +200` / `< 96² → +400` / `< 192² → +600` / 否则 `+1000`
 *   ⇒ **越靠近敌人打，掉得越密**；掉的是 **type 1**（= 我们的 `TRANCE_KILL_SPIRIT_TYPE`）。
 *
 * ⚠️ **两处必须声明的偏差**（不许包装成「复刻」）：
 *   ① 🟡 **有意窄化**：一手对**所有敌人**生效；本卡按用户口径**只对 boss / 道中 boss** 生效
 *      （判据 = `flags_lo` bit23 = ECL opcode 512 `setBoss`，一手普查 22 个 .ecl：
 *        只有 `st0Xbs`/`st0Xmbs` 各调一次 ⇒ 该位 ⟺ boss 或道中 boss）。小怪的"打着打着掉一颗"
 *      用户没要，且会让一关的神灵数量爆炸 ⇒ 不开。
 *   ② ❓ 自定：TH13 的 `+0x4004 = 30`（冷却）与 `> 0x13` 的组合语义在 TH18 无从照抄，
 *      这里换成「同一敌人的两次掉落至少间隔 `TRANCE_BOSS_DROP_COOLDOWN` 帧」（自定 20，进 TUNING）。
 *
 * 落点（一手，`REPORT-BOSS-SPIRIT-DROP.md` §2）：`0x42f2cb` `add [ebx+0x5008],esi`
 *   —— `ebx` = zEnemyData*、`esi` = 本帧最终伤害；它在两道免伤闸之后 ⇒ 语义 = "HP 即将减少"；
 *   `0x42ed40` 每敌每帧恰好一次。SDK 把这次事件派发成 `.on_enemy_damage`（见 `sdk/public.h`）。
 *
 * 为什么**只入队、帧末才撒**：回调在敌人的逻辑循环里（同一帧可能删敌人），建 VM 一律留到帧末
 *   —— 与击破那条链同一套纪律（`DEATH` §2.4 / `AUDIT` §AL55）。*/
#define TRANCE_BOSS_SLOTS          8      /* ≤4 个 boss 同时在场（`boss_ids[4]`）⇒ 8 槽永不够时就 LRU */
#define TRANCE_BOSS_DROP_COOLDOWN  20     /* ❓ 自定：同一敌人两次掉落的帧间隔下限（一手冷却常量的等价物）*/
#define TRANCE_BOSS_DROP_ENABLE    1      /* 2026-09-13：闪退二分用的总开关（置 0 = 整段不跑，连站点事件都不理会）*/
typedef struct {
    int32_t  used;            /* ★ 2026-09-13 第五轮实跑修：**必须显式标记"这一格用没用"** ——
                               *   旧写法用 `id == 0` 判断空槽，而 **enemy_id 真的会从 0 开始**
                               *   （构造 `id = mgr[0x90]; mgr[0x90] = id + 1`）⇒ 第一颗挨打的敌人就把
                               *   `s_boss[0]` 当"已存在"命中，`threshold` 还是 0 ⇒ **每 20 帧白掉一颗**
                               *   （实跑日志 `boss hits 1` / `total drops 1`、而 `accum` 只有 14）。*/
    uint32_t id;              /* `enemy+0x6830`（单调递增 ⇒ 槽复用也不会撞号，别用指针）*/
    int32_t  accum;           /* 累计伤害（一手 `+0x3f60`）*/
    int32_t  threshold;       /* 下一颗的阈值（一手 `+0x3ffc`，**累加**式）*/
    int32_t  last_drop;       /* 上一次掉落的帧号（冷却）*/
    int32_t  last_seen;       /* 最后一次受伤的帧号（LRU 淘汰用）*/
} trance_boss_slot_t;
static trance_boss_slot_t s_boss[TRANCE_BOSS_SLOTS];
static int32_t s_boss_queue;              /* 本帧待撒的颗数（帧末由 on_item_frame 取走）*/
static float   s_boss_qx[TRANCE_BOSS_SLOTS];
static float   s_boss_qy[TRANCE_BOSS_SLOTS];
static int32_t s_stat_boss_drop;          /* 累计掉了几颗（诊断进汇总日志）*/
static int32_t s_stat_boss_hits;          /* 累计"boss 吃到伤害"的事件次数（诊断：核对站点真的在跳）*/
static int32_t s_stat_boss_bad;           /* 被合理性闸挡掉的次数（诊断；见 on_enemy_damage）*/

/* 距离分档（一手 `th13:0x419e3a`）：返回阈值增量。用平方距离比，别开根号。*/
static int32_t trance_boss_bucket(float dx, float dy)
{
    const float d2 = dx * dx + dy * dy;
    if (d2 <  48.0f *  48.0f) return 200;
    if (d2 <  96.0f *  96.0f) return 400;
    if (d2 < 192.0f * 192.0f) return 600;
    return 1000;
}

static trance_boss_slot_t *trance_boss_slot(uint32_t id, int32_t frame)
{
    trance_boss_slot_t *free_slot = 0, *lru = &s_boss[0];
    int i;

    for (i = 0; i < TRANCE_BOSS_SLOTS; ++i) {
        if (s_boss[i].used && s_boss[i].id == id) { s_boss[i].last_seen = frame; return &s_boss[i]; }
        if (!s_boss[i].used && !free_slot) free_slot = &s_boss[i];
        if (s_boss[i].last_seen < lru->last_seen) lru = &s_boss[i];
    }
    if (!free_slot) free_slot = lru;           /* 满：淘汰最久没受伤的那个 */
    memset(free_slot, 0, sizeof *free_slot);
    free_slot->used       = 1;
    free_slot->id         = id;
    free_slot->threshold  = 200;               /* 第一颗的门槛 = 最近档（一手增量表的最小值）*/
    free_slot->last_drop  = frame - TRANCE_BOSS_DROP_COOLDOWN;
    free_slot->last_seen  = frame;
    return free_slot;
}

static void trance_boss_reset(void)
{
    memset(s_boss, 0, sizeof s_boss);
    s_boss_queue = 0;
    s_stat_boss_drop = 0;
    s_stat_boss_hits = 0;
    s_stat_boss_bad = 0;
}

/* ============================ ★ 灵界主炮组（TH13 灵界 set10 复刻，2026-09-13）============================
 * 用户口径（唯一权威）：「113 要**改变自机主炮**，复刻 TH13 灵界」。
 *
 * ## 一、原版怎么做（一手，`engine/spirit/th13/05-trance-attack.md` §0/§1.3）
 *   TH13 **不改任何一发弹的伤害或速度公式**：`FUN_00446000` 每帧算主炮组号时，
 *   `th13:0x446006 test [0x4be830],0x100` / `0x446036 mov eax,0xa` ⇒ **灵界 = 组号硬编码 10**，
 *   四个角色各在 `pl0Xa.sht` 的第 10 组放一套自己的 row（条数/角度/速/增伤/贴图全在数据里）。
 *   TH18 的同一段骨架（`th18:0x45ea00`）已经被 SDK 挂了 `on_shot_set_index`（`0x45ebfb`）——
 *   「组号 = 卡说了算」就是本回调 ⇒ **本卡只返回一个组号**，与 117 咆哮同形状。
 *
 * ## 二、TH18 这边的落点（2026-09-15 收尾：**永久专属槽 `0x28`**，NEXT.md §1w 第 5 步）
 *   零售的组指针数组**只有 40 格**（原生 23 + 本 mod 17，一格不剩），而原生 0x0a–0x15 十二格全是
 *   活的装备卡子机组 ⇒ 早先只能"借一格 + 把偏移项临时指到文件尾的孤儿段"。扩容到 96 格
 *   （数组区预留 256 格，AUDIT §CQ）后，`0x28` 成了本卡的**永久专属槽**：
 *     · 行数据 = `append_shooterset.py` 正常追加的一组（`TRANCE_ROWS`，名字 `TRANCE`）⇒ 偏移项常驻指着它；
 *     · 卡侧**只做一件事**：灵界窗口内 `on_shot_set_index` 返回 `CE_SHT_SET_TRANCE`（= 0x28），
 *       窗口外返回 −1（原生主炮，一个字节不动）。**不写任何 sht 内存** ⇒ 不需要借用/账本/还原三件套，
 *       也不存在"删卡后那一格永远指着灵界行"的收尾问题（`on_delete` 因此只收 BGM/无敌/神灵池）。
 *   ★ 顺带消掉一整类崩溃：旧设计的行指针是"影像基址 + 硬编码偏移"，**sht 资产与 DLL 不同批**
 *     时会指到影像之外，引擎把越界字节当子弹回调 `call`（AUDIT §CV）。现在最坏情况 = 读到未分配的
 *     set 0 别名 ⇒ **主炮没变、不崩**（下面 `trance_sht_group_ok()` 还会提前发现并记一行）。
 *   ★ 为什么窗口内这一格没有别的读者：装备子机路径 `0x40a9c0` 由本卡自己的 `on_option_shot_gate`
 *     掐掉（TH13 灵界组**一条子机 row 都没有**）⇒ 组号只由本卡的 `on_shot_set_index` 给出。
 *     失败方向：若哪天这个返回值失效，最坏 = 「灵界期间主炮没换」（看得见、不崩、日志可查）。
 *
 * ## 三、行数据 = TH13 一手逐字节（`pl0Xa.sht` set 10 → `pl0X.sht` 孤儿段）
 *   灵梦 2 row dmg22 dx±16 rate2 速24（贴图 `pl00f.png` 78×30）/ 魔理沙 4 row dmg 22/22/11/11
 *   dx ±16/±30 rate2（`pl01d.png` 62×30）/ 早苗 2 row dmg22 dx±16 rate2（`pl02c.png` 62×30 双条）。
 *   逐字段映射表、以及「TH13 有 TH18 没有 / TH18 有 TH13 没有」的字段怎么处理，
 *   全部写在 `append_shooterset.py` 的 `TRANSIENT_SLOT` 段注释里（那份是唯一出处，这里不重复）。
 *   ⚠ **咲夜不变**（用户 2026-09-13 定案）：TH13 没有咲夜 ⇒ 她这一档直接 `return -1`（走原生主炮），
 *     文件侧她的孤儿段也是空组（只有终止符）。
 *
 * ## 四、与 117 咆哮的仲裁
 *   两张卡争同一个断点。规则（用户 2026-09-13 定）：**灵界赢**（它是 113 自己的短窗口）。
 *   实现 = `sdk/public.h` 的 `shot_set_index_priority`（本卡 = `CE_SHT_PRIO_113` = 10；
 *   117 咆哮 = `CE_SHT_PRIO_117` = 20 ⇒ 两家同时要换 set 时**咆哮赢**，见那里的阶梯表）+
 *   `sdk.c` 的 `BP_ce_shot_set_index` 改成「取优先级最大者」，同帧多张卡都要覆盖时打一条
 *   `shot set contested` 的日志（现场一眼看出谁赢了）。AUDIT §CJ / §BE。
 * -------------------------------------------------------------------------------------------------- */

/* ★ 只读防御（2026-09-15，AUDIT §CW/CV 的教训）：进灵界前确认**这一格真的指着灵界行**。
 *   判据三连（全在 sht 影像内读，**不写**）：
 *     ① 影像在、槽位偏移项非 0（0 = 未分配 ⇒ 引擎会解析成 set 0）；
 *     ② 偏移项 + 数据区基点落在文件影像内，且该处**首行**起 4 个 func 字段（`+0x2c..+0x38`）全 0
 *        —— 这正是生成器对追加组的约定（`append_shooterset.py` 的 `check_shape` 反向断言），
 *        同时排除"偏移项指的是别的组"和"资产与 DLL 不同批"。
 *     ③ 咲夜的空组（只有终止符）也认：首 dword == 0xffffffff（终止符）。
 *   结果缓存成三态（未判 / 可用 / 不可用），**只在跨越时打一行日志**（每帧都打会淹掉现场日志）。
 *   ❓ 自定：这不是复刻（TH13 是硬编码组号、不存在资产错配），是给"我们自己的资产管线"上的保险。*/
enum { TRANCE_SHT_UNKNOWN = -1, TRANCE_SHT_BAD = 0, TRANCE_SHT_OK = 1 };
static int s_sht_state = TRANCE_SHT_UNKNOWN;
static uint8_t *s_sht_last;                 /* 诊断：最近一次体检的影像 */
static const uint8_t *s_sht_row;            /* 诊断：最近一次体检解出来的行指针 */

static const char *trance_sht_group_probe(void)
{
    uint8_t *pl = CE_PLAYER();
    uint8_t *sht = pl ? *(uint8_t **)(pl + CE_PLAYER_SHT_PTR) : 0;
    uint32_t ptr, base;
    if (!sht) return "影像不在（菜单/换场）";
    /* ★★ 2026-09-15 实跑订正（「灵界火力似乎没有生效」的真因）：
     *   文件里那一格存的是**文件内偏移**，但加载器会把它**重定位成绝对指针**
     *   （一手：`th18:0x45a836 add [esi+edx],eax`，`eax = lea [edx+CE_SHT_TRANCE_DBASE]`）
     *   ⇒ **运行时读到的是指针，不能再加基点**。上一版按"偏移"判（`>= 0x40000` 就非法）
     *   ⇒ 绝对指针必然被判非法 ⇒ `on_shot_set_index` 恒返回 −1 ⇒ 灵界里主炮一点没变。
     *   判据改成"这个指针落在影像内"：`base <= ptr < base + 1 MiB`（影像只有十几 KB）。*/
    ptr  = *(uint32_t *)(void *)(sht + CE_SHT_SET_ARRAY + CE_SHT_SET_TRANCE * 4);
    base = (uint32_t)(uintptr_t)sht;
    s_sht_last = sht;
    if (!ptr) return "槽位指针是 0（资产没这一格？）";
    if (ptr < base || ptr - base > 0x100000u) return "槽位指针不在影像内（资产与 DLL 不同批？）";
    {
        const uint8_t *row = (const uint8_t *)(uintptr_t)ptr;
        uint32_t first;
        s_sht_row = row;
        first = *(const uint32_t *)(const void *)row;
        if (first == 0xffffffffu) return 0;                 /* 空组（咲夜）⇒ 合法，但本卡不会切过去 */
        if (*(const uint32_t *)(const void *)(row + 0x2c) || *(const uint32_t *)(const void *)(row + 0x30)
         || *(const uint32_t *)(const void *)(row + 0x34) || *(const uint32_t *)(const void *)(row + 0x38))
            return "首行的 func 字段非 0（不像追加组的行）";
    }
    return 0;
}

/* 每帧（tick 里）判一次；只在**跨越**时打日志。→ 1 = 可以切过去。*/
static int trance_sht_group_ok(void)
{
    const char *why = trance_sht_group_probe();
    int want = why ? TRANCE_SHT_BAD : TRANCE_SHT_OK;
    if (want != s_sht_state) {
        s_sht_state = want;
        if (want == TRANCE_SHT_OK)
            ce_log("trance: shot group OK - set %#x (permanent slot) -> row %p in sht %p",
                   (unsigned)CE_SHT_SET_TRANCE, (const void *)s_sht_row, (void *)s_sht_last);
        else
            ce_log("trance: shot group UNAVAILABLE - set %#x: %s; main shot stays native",
                   (unsigned)CE_SHT_SET_TRANCE, why);
    }
    return want == TRANCE_SHT_OK;
}

/* 本帧该不该借：**在灵界** + **本机体有 TH13 对应**（咲夜没有）。*/
static int trance_sht_want(const trance_card_t *s)
{
    if (!s) return 0;
    if (s->core.phase == TRANCE_PHASE_IDLE) return 0;
    /* ★ 2026-09-16 用户新定案：**咲夜也参与**（数值照抄灵梦那份，贴图用她自己的刀弹
     *   `anm=7`）⇒ 旧的「咲夜不变」闸撤掉；她的差别只剩机体特技（走时 ×2 / 神灵寿命 ×2）。*/
    (void)TRANCE_CHAR_SAKUYA;
    return 1;
}

/* `Player__tick_shooting_state` 查主炮组号前的那个断点（`th18:0x45ebfb`）。
 * 返回 >= 0 = 覆盖本帧的 fire-time set index；-1 = 不覆盖（原版主炮）。
 * ★ 2026-09-15 收尾后本函数**不再写任何内存**（旧版在这里改偏移项）⇒ 只返回组号。
 *   引擎随后执行 `mov edi,[edi+edx*4+0xe0]` 拿到的就是生成器写好的那一格。
 *   `s_sht_state` 由每帧的 tick 刷新（`trance_sht_group_ok`）⇒ 这里只读。*/
static int on_shot_set_index(ce_card_t *c, int power_level)
{
    trance_card_t *s = st(c);
    (void)power_level;
    if (!trance_sht_want(s)) return -1;          /* 不在灵界（或咲夜）⇒ 原生主炮 */
    if (s_sht_state != TRANCE_SHT_OK) return -1; /* 这一格没验通过 ⇒ 退回原生主炮（不崩、可查）*/
    return CE_SHT_SET_TRANCE;
}

/* 装备卡子机路径（`th18:0x40a9c0`）：灵界期间**一律停火**——
 * 一手：TH13 灵界组**一条子机 row 都没有**（`pl0Xa.sht` set10 全是 slot=0），
 * 所以「子机弹照旧打」就不是复刻。SDK 只会对两组 set 问这个回调
 * （原生装备子机组 0x0a..0x15 与 117 的咆哮组 0x1e..0x27）⇒ 两条都拦。
 * ⚠ 这条同时保证了借用格 0x0e 在窗口内**没有别的读者**（见本段 §二）。*/
static int on_option_shot_gate(ce_card_t *c, int sht_set)
{
    trance_card_t *s = st(c);
    static int logged;
    (void)sht_set;
    if (!trance_sht_want(s)) { logged = 0; return 0; }
    if (!logged) {
        logged = 1;
        ce_log("trance: option sub-shots suppressed while in trance "
               "(TH13 set10 has no option row; first set asked = %#x)", (unsigned)sht_set);
    }
    return 1;
}

/* 队列里**还剩几颗**没喂出去（只给汇总日志用；环最多 4 项 ⇒ O(1) 的 4 次加法）。*/
static int32_t trance_phase_queued(void)
{
    int32_t i, sum = 0;
    for (i = 0; i < s_phase_q_count; ++i)
        sum += s_phase_qn[(s_phase_q_head + i) % TRANCE_PHASE_QUEUE_MAX];
    return sum;
}

/* ---- ★★ boss 相位批次的**帧末喂出**（回调只入队，VM 一律在这里建；见上面那段常量注释）----
 * 每帧最多喂 `TRANCE_PHASE_BURST_PER_FRAME` 颗；**池满就停在这一帧、余量留在队列里**
 * （照 116 的 burst 模式：宁可排队也不丢一大批）。
 * 类型按一手的构成逐颗选（`trance_boss_phase_spirit_type`，index = total − 剩余）。
 * ⚠ 池满时**不调** `trance_spirit_spawn`（它会把"撒不出去"记进 `s_stat_spirit_dropped` ——
 *   而我们这里是"排队"不是"丢"，记进去会污染那条诊断）。*/
static void trance_phase_burst_tick(trance_card_t *s)
{
    int k = 0;
    const int in_trance = (s->core.phase != TRANCE_PHASE_IDLE) ? 1 : 0;

    while (s_phase_q_count > 0 && k < TRANCE_PHASE_BURST_PER_FRAME) {
        const int32_t h = s_phase_q_head;
        const int32_t before = s_n_spirit;

        if (trance_spirit_pool_room(s_n_spirit) <= 0) break;      /* 池满：整批留着，下一帧再喂 */
        trance_spirit_spawn(s_phase_qx[h], s_phase_qy[h],
                            trance_boss_phase_spirit_type(s_phase_qt[h] - s_phase_qn[h], s_phase_qt[h]),
                            in_trance, &s->core, TRANCE_SPIRIT_PARAM_BOSS);   /* 通道 A/C = 2.0 */
        if (s_n_spirit == before) break;        /* VM 没建起来（anm 没装载）：留着，下一帧重试一次 */
        --s_phase_qn[h];
        ++k;
        ++s_stat_phase_spirits;
        if (s_phase_qn[h] <= 0) {               /* 这一批喂完 ⇒ 出队 */
            s_phase_q_head = (h + 1) % TRANCE_PHASE_QUEUE_MAX;
            --s_phase_q_count;
        }
    }
}

static void on_item_frame(ce_card_t *c)
{
    /* ★ 2026-09-18（用户口径）：向**共享 PIV 池**声明本卡在场 + 本卡的可写上限（`cap_i − init_i`，按难度查表）。
     *   一行、幂等（每帧重申，难度一变立刻跟上）；退场由 `ce_layer_drop_card()` 兜底。
     *   擦弹那**一项**是**共用写入者**（用户口径：别加一张卡擦弹数的作用就 +1 次）⇒ 这里只声明「我在这一族」。
     *   口径与表的一手出处：`engine/faith/OVERVIEW.md`「结论：th12–th17 可以共用一个 PIV」+ `sdk_core.h` 同名注释块。*/
    ce_piv_claim_auto(ce_card_id(c), ce_difficulty());
    ce_graze_family_claim(ce_card_id(c));

    trance_card_t *s = st(c);
    trance_tick_t t;
    float kx[TRANCE_KILL_QUEUE_MAX];
    float ky[TRANCE_KILL_QUEUE_MAX];
    int32_t kn[TRANCE_KILL_QUEUE_MAX];   /* ★ 每条击破自带的颗数（TH13 通道 B 的衰减结果）*/
    int i;

    if (!s) return;
    ++s->frame;

    /* ★★ 2026-09-16n：**跨面不清零 / 不充值**（用户口径：一手的灵界槽与最大得点跨面都不归零）。
     *   这里只做两件事：① 关卡号变了就记下（不再碰槽）；
     *   ② **关卡号变小 / 第一次看到**（= 新一局 / 练习重选关）才写一格 200 并把最大得点加成归零。
     *   （旧版那个“60 帧窗口自愈补 200”已删：它会把合法的空槽顶回一格。）*/
    {
        const int32_t st_now = (int32_t)CE_CURRENT_STAGE();
        if (st_now != s->last_stage) {
            const int new_run = (s->last_stage == 0 || st_now < s->last_stage);
            if (new_run) {
                if (s->max_point_bonus > 0)
                    ce_log("trance: 新一局（stage %d -> %d）⇒ 收灵最大得点加成归零（原值 %d 点）",
                           (int)s->last_stage, (int)st_now, (int)s->max_point_bonus);
                s->max_point_bonus = 0;
                s->decl_last = 0;
                trance_gauge_set(&s->core.gauge, TRANCE_GAUGE_INITIAL);
                ce_log("trance: 新一局（stage %d）⇒ 灵界槽写一格 %d（一手：进关初始化就写 200）",
                       (int)st_now, (int)TRANCE_GAUGE_INITIAL);
            } else {
                ce_log("trance: 跨面（stage %d -> %d）⇒ 灵界槽与最大得点加成**原样保留**"
                       "（槽 %d，加成 %d 点；一手跨面不归零/不充值）",
                       (int)s->last_stage, (int)st_now,
                       (int)s->core.gauge.gauge, (int)s->max_point_bonus);
            }
            s->last_stage = st_now;
        }
    }

    /* ① 窗口推进（空闲：推冷却 + 连锁窗口；在灵界里：槽 −1/帧、归零收尾并报 `re_die`）*/
    t = trance_tick(&s->core, s->frame);

    /* ★ 2026-10-01（跨卡只读标志；交接 = `HANDOFF-108-CROSSCARD-SHIFT.md`）：
     *   向层声明「灵界进行中」—— 108（永夜抄）靠它决定 shift 切不切侧（owner 口径：
     *   「处于灵界状态 or 暴走咆哮状态的时候，可以改主机贴图，但是 shift 别把妖子机放出来」）。
     *   判据 = `core.phase`（**不是** `p->state` —— 那是神灵实体的运动 state 机，
     *   见交接 §2.2 的订正）；粘性，撤位由下一帧的 `: 0u` 或删卡时的 `ce_layer_drop_card()` 完成。*/
    ce_card_flag_set(ce_card_id(c), (s->core.phase != TRANCE_PHASE_IDLE) ? CE_CARD_FLAG_TRANCE : 0u);

    /* ①′ ★ **对话强制退出**（用户口径 ④；一手 `th13:0x4288f9` = 起对话那一帧调 `end`）。
     *   **边沿触发**：只在"上一帧没对话、这一帧有对话"时动手（一手没有每帧检查那条路）。
     *   把它的结果并进 `t` ⇒ 下面的「结束反馈」与「补死」两条逻辑**共用一份**，不另抄一遍：
     *     · 手动档（非死亡）：`trance_end` 照一手补扣 **200**（`max(0, T−200)`）；
     *     · 中弹救下档（死亡强制）：**不扣 200**，但报 `re_die` ⇒ 本帧就补死（一手同款）。*/
    {
        const int dlg = trance_dialogue_active();
        if (dlg && !s->dlg_prev && s->core.phase != TRANCE_PHASE_IDLE) {
            trance_tick_t e;
            const int32_t gauge_before = s->core.gauge.gauge;
            memset(&e, 0, sizeof e);
            trance_end(&s->core, &e);
            ce_log("trance: DIALOGUE started -> force end (one-hand th13:0x4288f9); "
                   "gauge %d -> %d (non-death path keeps the fixed -200, NOT proportional), re_die=%d",
                   (int)gauge_before, (int)s->core.gauge.gauge, (int)e.re_die);
            t.ended            = t.ended || e.ended;
            t.ended_death_free = t.ended_death_free || e.ended_death_free;
            t.re_die           = t.re_die || e.re_die;
        }
        s->dlg_prev = dlg;
    }

    /* ② ★ 2026-09-12 用户定案：**灵界改成主动触发**（原来这里有一条「满槽自动进」，
     *   已删）—— 与 116 同一套契约：「**用 D 键切换到该卡，然后按 C 发动**」。
     *   发动实现在上面的 `on_activate()`（`active_recharge = 1`：真实代价是**灵界槽**，
     *   槽不满就 `ce_refuse` ⇒ SDK 把刚装填的充能退回、不算发动）。*/

    /* ③ ★ **补死**：中弹救下的窗口结束时**欠一次死亡**（一手 `th13:0x405c3e`：`end` 里再调一次
     *   `Player::die`）⇒ 这里真的去调 `th18:0x45d3a0`。
     *   ⚠ 只在自机**活着**（state 1）时补：死在别的 state 里（2/3/4）说明死亡流程已经在跑，
     *     再调一次只会把死亡动画 / 计时器重来一遍（`Player__die` 会重新置 `num_deathbomb_frames = 8`）。
     *     证据与不确定性：`Player__die` 的 3 个一手调用点都在「state ∉ {2,3,4}」的门之后
     *     （`0x45cd0d` / `0x45ce69` / `0x45d046`）⇒ 状态门本来就是引擎自己的调用前提，这里照抄。
     *   ★ 补死 = **普通死亡**，允许链式（用户定案）：引擎照常广播 `+0x10`，107/117/113 都还能再救。*/
    {
        uint8_t *pl = CE_PLAYER();
        const int32_t pstate = pl ? *(int32_t *)(pl + CE_PLAYER_STATE) : -1;
        if (trance_should_redie(&t)) {
            if (pstate == 1) {
                /* ★★ 2026-09-13 第五轮：**不在道具帧里调 `Player__die`** —— 改成"挂个欠账"，
                 *   到 `on_tick_2`（玩家 tick 前后那一档）再调。理由是一手 + 本仓纪律两条：
                 *     ① 引擎自己的三个 `Player__die` 调用点（`0x45cd0d`/`0x45ce69`/`0x45d046`）
                 *        **全在 `Player::on_tick` 里** —— 从道具帧（ItemManager 线）里调等于跨线程式的乱入；
                 *     ② `Player__die` 会**建 VM（死亡特效）**，而本仓纪律明确"别在错误的栈里建 VM"
                 *        （`AUDIT` §AL55 那一条）。
                 *   ⚠ 时机没变坏：`on_tick_2` 每帧都跑、且在玩家 tick 之前 ⇒ 同一个帧边界内落地。*/
                s->redie_pending = 1;
                ce_log("trance: re-die owed (death-free window ended, frame %d) — Player__die deferred "
                       "to the player-tick hook (see on_tick_2); chaining stays allowed", (int)s->frame);
            } else {
                ce_log("trance: re-die SKIPPED (player state %d, not 1) — death is already in flight",
                       (int)pstate);
            }
        }
    }

    /* ④ 「中弹无伤」：**灵界进行中不再声明无敌帧**（2026-09-13 第四轮，见下 ④-2）。
     * ★ 为什么删：这条当初是"一手原语还没落地"时的近似（当时的注释就写着"等一手原语落地后应当整个删掉"）。
     *   现在一手原语已经落地（断点闸门 `BP_ce_player_die`，站点 = `th18:0x45d3a0`，
     *   一手核对：**它就是 TH13 的 `Player::on_hit`**，th-re-data 把它叫 `Player__die` 是误称 ——
     *   音效 `0x45d3b4` / 特效 `0x45d3f7` / 状态→4 `0x45d4e4` / 无敌 6 `0x45d50d` / 窗口 8 `0x45d557`
     *   全在它体内；扣命与 miss 在更下游的 `0x45d090`，只能从状态 4 走到 ⇒ 闸门一开全部落空）。
     *   ⚠ **两者不等价，而且差在用户能看见的地方**：无敌 = 子弹**根本不判定**（用户实跑原话
     *     「撞上后无敌，但会使得这个弹被消掉」——那正是"判定被无敌挡掉"+"上一帧的弹被吃"的混合观感）；
     *     闸门 = 判定照跑、**弹被吃掉**、但不记 miss / 不播音 / 不进死亡流程 = **一手 TH13 的语义**。
     *   ⇒ 灵界期间**只留闸门**。唯一保留的无敌是「主动灵界结束后的 1 秒」（wiki 口径，见下）。*/
    ce_invuln_request(TRANCE_NUM,
                      (s->core.phase == TRANCE_PHASE_MANUAL
                       || s->core.phase == TRANCE_PHASE_DEATH_FREE)
                      ? 0                                            /* ★ 灵界中：不声明无敌（靠闸门） */
                      : (s->end_invuln > 0 ? s->end_invuln : 0));    /* ★ 主动结束后的 1 秒 */

    /* ⑤′ 主动结束后的 1 秒无敌倒计时（声明在 ④ 里；这里只递减）*/
    if (s->end_invuln > 0) --s->end_invuln;

    /* ★★ 2026-09-22 **删掉原来这里的一行直写**（owner 口径：「删掉那一行，完全交给无敌层」）。
     *
     * 历史（2026-09-16e）：用户报「灵界贴图被无敌的黄色闪给覆盖了」⇒ 当时在灵界期间**直写**
     *   `player+0x47774.cur = 0`（引擎自己的无敌计时；空判定早退读的就是它
     *   `th18:0x45cd23`/`0x45ce7f`/`0x45d05c`），目的 ① 与一手 TH13 一致（灵界里本就不该有无敌）
     *   ② 顺手去掉盖在白剪影上的黄闪。
     *
     * **为什么删**：那一行会**截短同一帧别的卡声明的长无敌** —— `ce_invuln_*` 是共享层
     *   （取最大、只写更长值、**谁都不许直写引擎字段**），107 结界（0.67 s）/ 109 暴风 / 117 霊撃 /
     *   110·111 的灵击 270 帧都声明在它上面。113 一清，那些卡的保命当场没了，而它们**无从知晓**。
     *   ⇒ 现在完全交给无敌层：谁声明的无敌照旧生效，黄闪也就是它该有的观感。
     *
     * **代价（owner 已确认接受）**：113 与上面那些卡**同装**时灵界里会看到无敌黄闪（113 单装没有，
     *   因为无人声明无敌 ⇒ 层写 0）。与一手 TH13 的差异变成「不主动清除，只是自己不声明」。
     *   ⚠ 出灵界那一秒的无敌仍是**我们自己的声明**（`end_invuln`，走 ④ 的 `ce_invuln_request`），不受影响。*/

    /* ④-2 ★ **中弹无伤闸门**（2026-09-13：从"近似"换成一手语义）。
     *   一手 TH13：`Player::on_hit` 第一行读到灵界标志就 return（`th13:0x444ca6`）
     *   ⇒ **不扣命 / 不记 miss / 不播中弹音，但弹照样被吃掉**（判定函数仍返回 1，调用侧置弹状态 3）。
     *   TH18 那个槽位是空的 ⇒ 本仓在 `Player__die` 入口插了断点（`BP_ce_player_die`），
     *   这里按相位**粘性声明**（断点落在两个 tick 之间也要挡得住）。
     *   ⚠ 与上面那条 `ce_invuln_request` **不是一回事**，两条都留着：
     *     无敌层 = 子弹**根本不判定**；本闸门 = 判定照跑、**弹被吃掉**（= 原版）。
     *     实跑首日要按 `AUDIT` §BP-6 的判据确认：灵界中撞弹 ⇒ 不掉命、不记 miss、**弹消失**。
     *
     * ★★ 2026-09-22（工单 owner th13 #4「灵界里被 boss 体术 ⇒ 不能移动，持续被体术」）**结案备注**。
     *   一手全文 = [`engine/player/th18/03-player-hit-gate.md`] §1.6（体术路径）/ §4.2（TH13 判定层闸门）/
     *   §4.3（TH13 体术路径）。四条要点（都过指令边界，逐条可复跑）：
     *
     *   ① **「boss 体术」没有自己的路径**：它是 `EnemyData__step_game_logic`（`0x42ed40`）里的接触判定
     *      —— 圆分支 `0x42f580 call 0x45cd90`、OBB 分支 `0x42f667 call 0x45cea0`，两处第 3 参都传 0
     *      （= 可致死）；命中后调用侧**只对 `eax==2`（擦弹）做事**（`0x42f66c`–`0x42f69a` 每 6 帧一次
     *      `Player__do_graze`），**`eax==1` 什么都不做**。⇒ 体术的全部后果只可能来自
     *      这两条判定函数体内的 `call 0x45d3a0`（= 本站点），没有别的分支。
     *
     *   ② **TH18 里「不能移动」只有一种形态 = 自机状态机离开 1**：移动只在
     *      `Player__on_tick__body` 的 **case 1** 里被调（`0x45c0c1` 判 `[0x4cf2a4]==0` 后
     *      `0x45c0cc call 0x45b170`，全 exe 唯一调用者）；case 0/2/4 都不调它。
     *      而 `player+0x476ac` 全 exe 只有 7 个写点，**值 4（决死窗口 = 那条"停止移动"）只有
     *      `0x45d4e4` 一处，就在 `Player__die` 体内（= 本闸门之后）**；值 2 只有 `0x45d21c`
     *      一处（`0x45d090` 的 commit），而它只能从状态 4 走到（唯一调用者 `0x45c135` 在 case 4 里）。
     *
     *   ③ **命中链里「判中 → call」之间一条玩家字段写都没有**：三条判定函数
     *      （`0x45cc20` / `0x45cd90` / `0x45cea0`）逐个筛「写内存」指令，命中的只有各自的栈局部
     *      （`0x45cd90` 更是**一条内存写都没有**）；区间 `0x423e10`–`0x45d588`（62035 条）按
     *      `+0x476ac`/`+0x47774/78`/`+0x4779c`/`+0x477a4`/`+0x620/624/62c/630`/`+0x47908`/`0x4cf2a4`
     *      过滤出 49 条写，**判定函数体内 0 条**；玩家位置/速度的 26 条写点**全部**属于玩家自己那一族
     *      ⇒ **TH18 没有「体术推挤/位移写回」**（对 `+0x47778` 的直写也没有）。
     *      ⇒ **本站点已经早到极限**：再往前（判定函数里返回 0）弹就**不被吃掉**了，
     *        与 owner 口径「弹照样被吃掉 + 金光」冲突 ⇒ **不加站点、不挪站点**（`AUDIT` §BE-FE）。
     *
     *   ④ **不用无敌层**（owner 2026-09-22 否决：「无敌 = 会删金光」）：灵界期间继续保持
     *      `ce_invuln_request(113, 0)`（上面 ④），只留本闸门 —— 判定照跑（碰撞发生）、弹被吃掉、
     *      hit 音被拦（`0x45d3b4` 在闸门之后）、状态 4 / 决死窗口 / 停止移动一笔都不写。
     *      一手对照：TH13 那道 `on_hit` 闸门（`th13:0x444ca4` → `th13:0x444de4` 函数尾）之前
     *      同样**没有任何玩家字段写**；而 TH13 的**四个判定函数一个内存写都没有**
     *      （所以「TH13 早退就够」这条成立）。
     *      ⚠ 一手差异（已知、按 owner 口径不复刻）：TH13 的圆/OBB 判定函数**自己**也过灵界旗标
     *      （`th13:0x444146` / `th13:0x444269` / `th13:0x444759` → `xor eax,eax; ret`）⇒
     *      原版灵界里「圆弹 / 激光 / 体术」是「连判定都不成立 = 穿过去」，我们这版是「判定成立但被吃掉」。
     *
     *   ⇒ **无回归可修的结论**：owner 2026-09-18 那次实跑（《单卡测试》）时本站点**还 park 着没进 patch**
     *     （09-19 `7647511` 才放出）⇒ 那时体术命中真的走 `Player__die` ⇒ **状态 4 = 决死窗口 = 不能移动**，
     *     窗口结束被本卡的 `+0x0c` 救援取消 ⇒ 下一帧再中 ⇒ 那个「持续不能动」的循环。
     *     09-19 之后闸门在跑，这一笔在代码上已经不存在；实跑判据见 §1.6.4（含下面 ④-2b 的诊断行）。*/
    ce_no_hit_declare(TRANCE_NUM,
                      (s->core.phase == TRANCE_PHASE_MANUAL
                       || s->core.phase == TRANCE_PHASE_DEATH_FREE) ? 1 : 0);

    /* ④-2b ★★ **灵界期间「自机被锁」现场取证**（2026-09-22 工单 owner th13 #4）。
     *   为什么需要它：owner 的观察原话是「灵界状态下被 boss 体术 = 不能移动，持续被体术」——
     *   「不能移动」是一句**观感**，而引擎里它只有一种可判定的形态：自机状态机离开 `1`
     *   （一手：移动只从 `Player__on_tick__body` 的 case 1 调，`0x45c0c1` 判 `[0x4cf2a4]==0` 后
     *   `0x45c0cc call 0x45b170`；状态 4/2/0 那三支都不调它。全 exe 于 `+0x476ac` 的 7 个写点见
     *   `engine/player/th18/03-player-hit-gate.md` §1.6）。
     *   与本卡的关系：灵界里我们只声明**闸门**（上面 ④-2），闸门开着的那些命中路径**进不了状态 4**
     *   ⇒ 灵界期间状态应当**恒 1**。所以"读到 ≠1"要么是**闸门没生效**（站点没挂上 / 声明没到），
     *   要么是**另一条我没找到的锁**（那就是新证据，必须带状态值回报）。
     *   ⚠ 只读、只记数、不写任何引擎字段；限流 6 行。预期会各出 1 行的正常情形：死亡触发档收尾
     *   （自机本来就要死：状态 4 → 2）。*/
    if (s->core.phase != TRANCE_PHASE_IDLE) {
        uint8_t *wpl = CE_PLAYER();
        const int32_t ps = wpl ? *(int32_t *)(wpl + CE_PLAYER_STATE) : -1;
        if (ps != 1) {
            ++s_stat_locked_frames;
            if (ps != s_lock_state_prev && s_lock_log_lines < TRANCE_LOCK_LOG_LIMIT) {
                ++s_lock_log_lines;
                ce_log("trance: ★ player LOCKED-in-trance: state %d -> %d while the no-hit gate is "
                       "declared for card %d (frame %d, phase %d) — 移动只从 state 1 那一支调 "
                       "(0x45c0cc)，所以 state != 1 就是「不能移动」；state 4 的唯一写点是 "
                       "0x45d4e4（在 Player__die 体内 = 本卡闸门之后）⇒ 若非死亡触发档收尾看到这一行，"
                       "请连同这行回报", (int)s_lock_state_prev, (int)ps, TRANCE_NUM, (int)s->frame,
                       (int)s->core.phase);
            }
        }
        s_lock_state_prev = ps;
    } else {
        s_lock_state_prev = 1;   /* 出灵界复位（下一次进灵界从 1 起算，边沿检测不误报）*/
    }

    /* ④′ ★ **灵界版 BGM（路线 A：DLL 侧第二路 + 交叉淡化）** —— 只在**帧点**动手。
     *   为什么不在 `on_activate` / `on_death_after_deathbomb` 里起：
     *   · 后者是**死亡栈**（`Player__die` 的调用链）⇒ 那里不建 DirectSound 对象（本卡纪律）；
     *   · 前者虽然安全，但两条入口各写一遍不如"看相位"统一 —— 相位一进灵界就起、一回 IDLE 就淡出，
     *     两个入口（手动 / 中弹救援）自动都覆盖到。
     *   `ce_trance_bgm_start()` **幂等**且**自检失败时返回 0**（不产生任何副作用）⇒ 起不来就是没 BGM。*/
    if (s->core.phase != TRANCE_PHASE_IDLE) {
        if (!ce_trance_bgm_active()) (void)ce_trance_bgm_start();
    } else if (ce_trance_bgm_active()) {
        ce_trance_bgm_release(0);        /* 从当前位置往回淡；真正的停+释放在淡化走完那一帧 */
    }

    /* ④‴ **灵界常驻背景（astral）**：相位一进就起、一回 IDLE 就删（一手：那台子 VM 由 effect 持有，
     *   灵界结束即随之销毁）。
     *   ★ 2026-09-13 第六轮：**位置不再靠出生点** —— 一手 `astral.anm` script0 自己写 `pos(320,224)`
     *     （640×480 屏的正中）⇒ 脚本 251 里已换成世界层的 `pos(0,224)`（游玩区正中）。
     *     旧注释「出生点 (0,0) + originMode(1) = 游玩区正中」是**错的**：那是**上边框中点**。
     *   ★★ 2026-09-13 第七轮：**这一段被搬到 ④‴′ 之后**（见下面）—— 一手画序是
     *     "先 4 台幕布 → 再画 astral"（`th13:0x4032d0` 的 `0x403570` 循环 vs `0x403591`），
     *     而**同一层内的叠放顺序 = 生成顺序**（`AnmManager__insert_in_world_list_back` 尾插，
     *     `AUDIT` §AL/`01-vm-instantiate.md` §2b-4）⇒ 要让 astral 压在幕布上，
     *     就必须**先起幕布、后起 astral**。*/

    /* ④‴′ ★★ **进/出幕布**（用户口径「退出是一张有形状的不透明 mask」）——
     *   · **进**（上升沿：上一帧不在灵界、这一帧在）= 脚本 266–269 = 一手 `effect.anm` 3/4/5/6
     *     （`0x402ff0` 初始化时挂的那 4 条）：4 块斜置笔触幕布**扫进来**，45 帧后自灭；
     *   · **出**（下落沿）= 脚本 262–265 = 一手 `effect.anm` 7/8/9/10（`end` 发中断 1 时重建）：
     *     同样 4 块**扫出去**，40/45/50/55 帧各自自灭。
     *   ★★ 2026-09-13 第七轮两处订正（上一版实跑 = 用户说的「两个蓝色的矩形移动」）：
     *     ① **一手 `pos` 要加 effect 宿主的 `entity_pos (320,240)`**（`0x402ff0` 尾段把 4 台子 VM 的
     *        `+0x574/+0x578` 写成 320/240）⇒ 换算到世界层 = **一手字面量 + (0,224)**；
     *        漏掉它整块幕布高 240 px，游玩区里只剩"被 CLAMP 拉实的那半边" = 蓝矩形。
     *     ② **`posTime` 的 (x,y,z) 是绝对目标**（一手 thpages：`initial = 当前值`、`goal = 参数`），
     *        第二参 1 是缓动曲线 `easeIn2`，不是"增量"⇒ 目标值同样要 +(0,224)。
     *   · 只在**边沿**起一次；每帧扫名单把已自灭的 id 归 0（不清就会拿着死 id 去删）；
     *   · 名单是文件级 static ⇒ 删卡/局末由 `trance_clear()` 收（`METHOD.md` 铁律二）。*/
    {
        const int32_t in_trance = (s->core.phase != TRANCE_PHASE_IDLE) ? 1 : 0;
        int k, wipe_alive = 0;
        /* ★★ 2026-09-13 第八轮：**先起"目的地 alpha 清零幕"**（脚本 271）。
         *   它是 TH13 `0x4032d0` 那段自定义四边形（游戏区 `alpha := 0`、RGB 保留）的 anm 译法，
         *   必须**每帧、在幕布与 astral 之前**画 ⇒ 同一层里生成顺序最靠前。
         *   它一旦没起，`astral`（`blendMode(7)`，只认 Da）在灵界期间就可能整块看不见
         *   ⇒ 这里**每帧**做一次"还活着吗"的补起（不是只在边沿起一次），见下面 `TRANCE_ALPHA_MASK` 那段。*/
        if (in_trance && !s_wipe_phase_prev) {          /* ★ 上升沿：灵界刚开始 = 幕布扫进来 */
            s_wipe_frame = 0;                           /* 本批的帧计数（驱动填充幕的 alpha）*/
#if TRANCE_ALPHA_MASK && TRANCE_WIPE_ENTER
            /* ⚠ 只在"进场幕布也会起"时才起清零幕：没有幕布写 alpha 的话，清零幕会让
             *   前 45 帧（底色还没亮）Da 一直是 0 ⇒ astral 那 45 帧看不见。两个开关要一起改。*/
            if (s_alpha_zero_vm && !ce_anm_get_vm((uint32_t)s_alpha_zero_vm)) s_alpha_zero_vm = 0;
            if (!s_alpha_zero_vm) {
                s_alpha_zero_vm = (int32_t)ce_anm_spawn(CE_ABILITY_ANM(),
                                                        CE_ANM_ABILITY_SCRIPT_TRANCE_ALPHA_ZERO,
                                                        TRANCE_WIPE_IN_LAYER);
                ce_log("trance: alpha 清零幕 VM %d (脚本 271 = 一手 th13:0x4032d0 那块「RGB 保留 / "
                       "alpha := 0」的游戏区四边形；blendMode(2) + color(0,0,0) + alpha(255) ⇒ "
                       "rgb := D、a := Da−1→0；层 %d)", (int)s_alpha_zero_vm, TRANCE_WIPE_IN_LAYER);
            }
#endif
        /* ★★★ **alpha 填充幕**（脚本 270 = 一手 script3 的 t=45 那段：一块 640×480 的 `drawRect`）。
         * ★ 它不是"暗幕"：`astral` 用 `blendMode(7)`（`rgb = astral·Da + 后台·(1−Da)`）只认 Da。
         * ★★ 第九轮**顺序订正**：TH18 的 alpha 是 `Da := Sa`（一手 `th18:0x47d961`）⇒
         *   **同一层里"后画的赢"** ⇒ 这块全屏矩形必须画在**4 台幕布之前**，否则每帧都会把幕布
         *   刚写好的 `Da` 再抹掉（`TRANCE_FILL_OFF` 的 A=1/255 ≈ 0）⇒ 进场永远揭不开 astral。
         *   一手同构：`effect.anm` script3 那台 VM 是 4 台里的**第 1 台**（`0x403570` 循环按 0..3 画），
         *   所以它的 t=45 `drawRect` 天然在另外 3 台之前 ⇒ 我们按 **[清零 271] → [填充 270] → [4 台幕布] → [astral]**
         *   生成（`on_item_frame` 里的 spawn 顺序）。*/
#if TRANCE_WIPE_IN_COVER
        if (in_trance && !s_wipe_phase_prev) {
            if (s_wipe_cover_vm && !ce_anm_get_vm((uint32_t)s_wipe_cover_vm)) s_wipe_cover_vm = 0;
            if (!s_wipe_cover_vm) {
                s_wipe_cover_vm = (int32_t)ce_anm_spawn(CE_ABILITY_ANM(),
                                                        CE_ANM_ABILITY_SCRIPT_TRANCE_WIPE_COVER,
                                                        TRANCE_WIPE_IN_LAYER);
                ce_log("trance: alpha 填充幕 VM %d (脚本 270 = 一手 effect.anm script3 的 t=45 常驻"
                       "drawRect(640,480), alpha(255) ⇒ 第 45 帧起整屏 Da := 1 ⇒ astral 全显；"
                       "起在 astral 之前 ⇒ 被 astral 用 mode 7 整块替换、只在左右边条露出) 层 %d",
                       (int)s_wipe_cover_vm, TRANCE_WIPE_IN_LAYER);
            }
            /* ★★★ 第九轮（`AUDIT` §CT-7）：**填充幕的 alpha 也由 C 驱动**（不再依赖脚本的 `alphaTime`）。
             *   第 45 帧之前写 `TRANCE_FILL_OFF`（A = 1/255 ≈ 0 ⇒ `Da := ≈0`，astral 不被填满）；
             *   第 45 帧起写 `TRANCE_WIPE_ARGB`（A = 255 ⇒ `Da := 1` ⇒ astral 全显）。
             *   一手依据：`effect.anm` script3 的 t=45 段（`AUDIT` §CN-9-1 ⑥）；
             *   "由 C 写而不是靠脚本的 `alpha()`" = ❓自定（同 §CT-3 的理由：`alpha()` 落在哪一路
             *   取决于 `colorMode`，只有 C 写两路才是确定的）。*/
            if (s_wipe_cover_vm) {
                wipe_set_dual_color((uint32_t)s_wipe_cover_vm,
                                    (s_wipe_frame >= 45) ? TRANCE_WIPE_ARGB : TRANCE_FILL_OFF);
            }
        }
#endif
#if TRANCE_WIPE_ENTER
            int up = 0;
            for (k = 0; k < TRANCE_WIPE_N; ++k) {
                if (s_wipe_vm[k]) { ce_anm_delete((uint32_t)s_wipe_vm[k]); s_wipe_vm[k] = 0; }
                s_wipe_vm[k] = (int32_t)ce_anm_spawn(CE_ABILITY_ANM(),
                                                     CE_ANM_ABILITY_SCRIPT_TRANCE_WIPE_IN1 + k,
                                                     TRANCE_WIPE_IN_LAYER);
                if (s_wipe_vm[k]) ++up;
            }
            ++s_wipe_bursts;
            ce_log("trance: 进入幕布 %d/4 起 (脚本 %d..%d, 层 %d, 一手 effect.anm script3..6 + "
                   "TH18 自己那份 (effect.anm script3..6, color(64,64,56)): scale(3,12) 斜置扫入, "
                   "45 帧自灭；它们把 eff_switch 的笔触 alpha **写进目的地 alpha** = 那张「会动的遮罩」)"
                   " — burst #%d",
                   up, CE_ANM_ABILITY_SCRIPT_TRANCE_WIPE_IN1,
                   CE_ANM_ABILITY_SCRIPT_TRANCE_WIPE_IN1 + TRANCE_WIPE_N - 1,
                   TRANCE_WIPE_IN_LAYER, (int)s_wipe_bursts);
#endif
        }
        if (!in_trance && s_wipe_phase_prev) {    /* ★ 下落沿：灵界刚结束 = 幕布扫出去 */
            int up = 0;
            /* ★ 一手在 `end`（中断 1）里把 4 台子 VM **重建**成 script 7..10 —— 而 script3 的
             *   那台 VM 一被重建，t=45 那块常驻 `drawRect` 就**没了** ⇒ 退出时 Da 不再被填满。
             *   本卡里底色是独立的一支 VM（脚本 270）⇒ 必须在这里**显式删掉**，否则 Da 永远是 1、
             *   astral 永远显着，退出转场就看不见了。*/
            if (s_wipe_cover_vm) { ce_anm_delete((uint32_t)s_wipe_cover_vm); s_wipe_cover_vm = 0; }
            /* ★ 清零幕在**退出窗口里继续活着**：退出幕布靠它每帧把 Da 清 0，笔触才能"擦掉" astral
             *   （一手同理：状态字在退出期间仍是 2 ⇒ `0x4032d0` 那块四边形照画）。*/
            for (k = 0; k < TRANCE_WIPE_N; ++k) {
                if (s_wipe_vm[k]) { ce_anm_delete((uint32_t)s_wipe_vm[k]); s_wipe_vm[k] = 0; }
                s_wipe_vm[k] = (int32_t)ce_anm_spawn(CE_ABILITY_ANM(),
                                                     CE_ANM_ABILITY_SCRIPT_TRANCE_WIPE1 + k,
                                                     TRANCE_WIPE_LAYER);
                if (s_wipe_vm[k]) ++up;
            }
            ++s_wipe_bursts_out;
            /* ★★ 第八轮：**退出时必须让 astral 排在这 4 台之后**。
             *   同层内前后 = 生成顺序（尾插），而 astral 是**灵界开始时**起的（排在前面）
             *   ⇒ 它会在退出幕布**之前**画 ⇒ 读到的 `Da` 里没有退出的笔触 ⇒ 反向转场失效。
             *   一手的做法是"幕布占了子 VM 的 0..3 号槽、astral 永远在第 5 槽"（`0x403570` 循环 +
             *   `0x403591`）⇒ 本机用**重建 astral** 达到同一效果：这里先删掉，
             *   下面那段"astral 寿命"逻辑会在**同一帧**把它重起（排在幕布之后）✓。
             *   ⚠ 重建会把 `angleVel` 累积的角度归零 ⇒ 用 `s_bg_angle` 追回来（见下）。*/
            if (s_bg_vm) { ce_anm_delete((uint32_t)s_bg_vm); s_bg_vm = 0; }
            ce_log("trance: 退出擦除幕布 %d/4 起 (脚本 %d..%d, 层 %d, 一手 effect.anm script7..10: "
                   "scale(3,12) 斜置扫出, 40/45/50/55 帧自灭；同一时间填充幕已删、清零幕仍在 ⇒ "
                   "astral 被反向擦掉；astral 已删待重起 = 排在幕布之后) — burst #%d",
                   up, CE_ANM_ABILITY_SCRIPT_TRANCE_WIPE1,
                   CE_ANM_ABILITY_SCRIPT_TRANCE_WIPE1 + TRANCE_WIPE_N - 1,
                   TRANCE_WIPE_LAYER, (int)s_wipe_bursts_out);
        }
        s_wipe_phase_prev = in_trance;
        ++s_wipe_frame;
        for (k = 0; k < TRANCE_WIPE_N; ++k) {           /* 自灭的 VM 归 0（脚本自带 delete）*/
            if (s_wipe_vm[k] && !ce_anm_get_vm((uint32_t)s_wipe_vm[k])) s_wipe_vm[k] = 0;
            if (s_wipe_vm[k]) ++wipe_alive;
        }
        /* ★★★ 2026-09-13 第九轮（`AUDIT` §CT-7）：**幕布的两路顶点色每帧都写**（`TRANCE_WIPE_ARGB`）。
         *   为什么必须从 C 写：TH18 恒开分离 alpha 混合 ⇒ `Da' := Sa`（一手 `th18:0x47d961`），
         *   而顶点色取自 `colorMode` 选中的那一路（`th18:0x47e202`：0 → color_1、1 → color_2），
         *   脚本的 `color()` 只碰 color_1 的 RGB、`alpha()` 只碰 color_2 的 A ⇒ **只靠脚本无法保证
         *   被选中的那一路的 A = 255**（那才是"`Sa` 等于贴图笔触 alpha = 那张遮罩"的前提）。
         *   两路都写 ⇒ 与 colorMode 无关、也不再依赖 `alpha()` 落在哪一路。*/
        for (k = 0; k < TRANCE_WIPE_N; ++k)
            if (s_wipe_vm[k]) wipe_set_dual_color((uint32_t)s_wipe_vm[k], TRANCE_WIPE_ARGB);
        /* ★★ 2026-09-13 第九轮（`AUDIT` §CT）：**每帧把幕布的 z 旋转写回去**。
         *   用户实跑说「透明度蒙版的**纤维方向反了**（正常是左右方向，看到的是竖直且很细）」——
         *   而一手（`th13:0x4032d0` 调 `draw_vm` 画的那 4 台）的 `rotate(0,0,±θ)` 恰好把贴图里
         *   **竖直的笔触**转到**离水平只差 10–20°**（θ 是 ±70/±80 出、±100/±110 进；见下面的表）。
         *   本机脚本里那 4 条 `rotate()` 与一手**逐字相同**（`AUDIT` §CT-1 的对照表），
         *   但实跑的画面却是"未旋转"的笔触 ⇒ 我们**不再依赖脚本里的 `rotate` 是否落地**，
         *   由卡侧每帧直接写 `vm+0x44`（rz）。**这正是本仓另一张卡（117 的鹫束 script163）用的写法**
         *   （`ce_anm_set_rotation` 逐帧改朝向），所以是既有先例而不是新发明。
         *   θ 值本身 = **一手的原值**（复刻）；"由 C 写而不是靠脚本指令" = ❓自定（给理由+出处）。
         *   ⚠ 脚本里**没有** `rotateTime` ⇒ 不会被插值器盖掉（`sdk.h` 的注释）。*/
        {
            const float *th = in_trance ? s_wipe_theta_in : s_wipe_theta_out;
            for (k = 0; k < TRANCE_WIPE_N; ++k) {
                uint8_t *vm;
                if (!s_wipe_vm[k]) continue;
                /* ★ 探针在**写之前**读：`rz_before` 才是"引擎自己留下的那个值"。
                 *   · 若 `rz_before ≈ 要的值` ⇒ 脚本的 `rotate()` 本来就落地了（那本轮的写入是幂等的）；
                 *   · 若 `rz_before = 0`（尤其**第一帧就是 0**）⇒ 脚本的 `rotate()` 没落地
                 *     （= 上一版实跑那幅"竖直且很细"的笔触就是这个），本轮的写入是**真修**；
                 *   · 若第一帧 ≈ 要的值、后面变 0 ⇒ 有插值器在每帧盖它，本轮的写入同样是真修。*/
                vm = ce_anm_get_vm((uint32_t)s_wipe_vm[k]);
                if (vm && s_wipe_probe < TRANCE_WIPE_PROBE_N * 2) {
                    const uint32_t fl = *(uint32_t *)(vm + 0x534);
                    const float *rz = (const float *)(vm + CE_ANM_VM_ROTATION);
                    const float *sc = (const float *)(vm + CE_ANM_VM_SCALE);
                    const float *pp = (const float *)(vm + 0x30);
                    ce_log("trance: 幕布探针 #%d %s k=%d id=%d **rz_before=%.6f**(要 %.6f) mode=%u "
                           "layer=%d scale=(%.2f,%.2f) pos=(%.1f,%.1f) spr=%d c1=%08x c2=%08x eff=%08x",
                           (int)s_wipe_probe, in_trance ? "进" : "出", k, (int)s_wipe_vm[k],
                           (double)rz[2], (double)th[k], (unsigned)((fl >> 26) & 0x1f),
                           (int)*(uint32_t *)(vm + 0x18), (double)sc[0], (double)sc[1],
                           (double)pp[0], (double)pp[1], (int)*(uint32_t *)(vm + CE_ANM_VM_SPRITE_ID),
                           *(uint32_t *)(vm + CE_ANM_VM_COLOR1), *(uint32_t *)(vm + CE_ANM_VM_COLOR2),
                           *(uint32_t *)(vm + 0x52c));
                    ++s_wipe_probe;
                }
                (void)ce_anm_set_rotation((uint32_t)s_wipe_vm[k], 0.0f, 0.0f, th[k]);
            }
        }
#if TRANCE_BG_ENABLE
        /* ★ astral 的寿命 = "在灵界里 **或** 退出幕布还在飞"（一手：effect 回收时两者一起消失）。
         *   ⇒ 退出时**不立刻删背景**，幕布扫完（≈55 帧）才删 ⇒ 用户口径的"mask 盖住**这张图**"
         *   才是真的（否则幕布盖的是已经空掉的场地）。*/
        if (s_bg_vm && !in_trance && wipe_alive == 0) {
            ce_anm_delete((uint32_t)s_bg_vm);
            s_bg_vm = 0;
            if (s_wipe_cover_vm) { ce_anm_delete((uint32_t)s_wipe_cover_vm); s_wipe_cover_vm = 0; }
        }
        /* ★★ 一手画序（`th13:0x4032d0`）：**清 alpha（`0x4032f5`–`0x4034d6`）→ 4 台幕布
         *   （`0x403570` 的循环）→ 画 astral（`0x403591`）**。同层叠放 = 生成顺序（尾插）
         *   ⇒ astral **必须在这段之后**起，才会压在清零幕/幕布/底色之上（否则 astral 在列表里更靠前 = 被压住 ✗）。
         *   ★★★ 第九轮修正（实跑「退出时背景变成灰色纯色」的**真因**）：这里的门原来是
         *   `s->core.phase != TRANCE_PHASE_IDLE`，而**下落沿那一帧相位已经是 IDLE**
         *   （`in_trance = 0`）⇒ 上面刚落沿把 astral 删掉，这里又因为相位是 IDLE **不重起**
         *   ⇒ **整个退出窗口（40–55 帧）都没有 astral** ⇒ 幕布那层 `color(64,64,56)` 的
         *   不透明灰**原样留在画面上**（本该被 mode 7 的 astral 整块替换）＝ 用户看到的"灰色纯色"，
         *   而"在一定透明度蒙版下消失"就是幕布自己的 alpha 形状在扫出去。
         *   ⇒ 门必须与上面那句"寿命"逻辑**互补**：`in_trance || wipe_alive != 0`。
         *   （`AUDIT` §CT-2；这条与一手一致：`end` 只发中断 1、状态字仍是 2，astral 一直在画。）*/
        if (s->core.phase != TRANCE_PHASE_IDLE || wipe_alive != 0) {
            if (!s_bg_vm || !ce_anm_get_vm((uint32_t)s_bg_vm)) {
                if (s_bg_vm) { ce_anm_delete((uint32_t)s_bg_vm); s_bg_vm = 0; }  /* 追不回 ⇒ 记 0 再起 */
                s_bg_vm = (int32_t)ce_anm_spawn(CE_ABILITY_ANM(),
                                                CE_ANM_ABILITY_SCRIPT_TRANCE_ASTRAL,
                                                TRANCE_BG_LAYER);
                /* ★★ 自转相位的连续性：astral 脚本用 `angleVel(0,0,0.010471975)`（0.6°/帧）自转，
                 *   角度**存在 VM 里** ⇒ 重建就会归零（= 一次最多 180° 的跳变，退出转场第一帧
                 *   正好是 astral 全显的那一帧 ⇒ 看得出来）。本卡自己按同一速率记账，
                 *   起完就把账本上的角度写回去（`ce_anm_set_rotation` 写 `vm+0x3c` 的 rz，
                 *   之后 `angleVel` 继续在这个值上加 = 无缝）。*/
                if (s_bg_angle != 0.0f) {
                    (void)ce_anm_set_rotation((uint32_t)s_bg_vm, 0.0f, 0.0f, s_bg_angle);
                    ce_log("trance: astral 重建 ⇒ 自转相位写回 %.4f rad（避免退出首帧的旋转跳变）",
                           (double)s_bg_angle);
                }
                ce_log("trance: astral 背景 VM %d (script 251, layer %d, pos(0,224) 场地正中, "
                       "scale 1.15 = 588 单位 ⇒ 盖满游玩区, 0.6°/帧 自转；起在清零幕/幕布/填充幕**之后**"
                       "= 压住它们) - 复刻 astral.anm script0 含 blendMode(7)（第八轮回改）",
                       (int)s_bg_vm, TRANCE_BG_LAYER);
            }
            s_bg_angle += TRANCE_BG_ANGLE_VEL;          /* 与脚本里的 angleVel 同步记账 */
        } else {
            s_bg_angle = 0.0f;                          /* 出灵界窗口后再进来 ⇒ 从头转 */
        }
#endif
#if TRANCE_ALPHA_MASK
        /* ★ 清零幕（脚本 271）的寿命 = **"在灵界里 且 进场幕布开着"** 或 **"退出幕布还在飞"**。
         *   它自己 `stop()` 不会消失；`ce_anm_get_vm` 查得到就说明还活着 ⇒ 每帧对账一次。
         *   ⚠ `TRANCE_WIPE_ENTER 0` 时**进场窗口不起它**（没有幕布写 alpha，Da 会一直是 0、
         *     前 45 帧 astral 看不见）—— 只用它覆盖退出窗口。见 `TRANCE_ALPHA_MASK` 的注释。
         *   ★★ 这一段**必须排在 astral 的删/起之后**（删序 = 先 astral、后清零幕）：
         *     清零幕一停，Da 就回到引擎每帧 Clear 的 1（`0xffffffff`，一手 `th18:0x4553dc`）
         *     ⇒ 若 astral 还活着，那一帧它会**整块闪一下**。先删 astral 就没有这一帧。*/
        {
            const int want_zero = in_trance ? (TRANCE_WIPE_ENTER != 0) : (wipe_alive != 0);
            if (s_alpha_zero_vm && !ce_anm_get_vm((uint32_t)s_alpha_zero_vm)) s_alpha_zero_vm = 0;
            if (want_zero && !s_alpha_zero_vm) {
                s_alpha_zero_vm = (int32_t)ce_anm_spawn(CE_ABILITY_ANM(),
                                                        CE_ANM_ABILITY_SCRIPT_TRANCE_ALPHA_ZERO,
                                                        TRANCE_WIPE_IN_LAYER);
                if (s_alpha_zero_vm)
                    ce_log("trance: alpha 清零幕补起 VM %d (层 %d) — 它中途没了 ⇒ astral(mode 7) 会失真",
                           (int)s_alpha_zero_vm, TRANCE_WIPE_IN_LAYER);
            } else if (!want_zero && s_alpha_zero_vm) {
                ce_anm_delete((uint32_t)s_alpha_zero_vm);
                s_alpha_zero_vm = 0;
            }
            /* ★★★ 第九轮（`AUDIT` §CT-7）：**清零幕的两路顶点色每帧写死**。
             *   `color_1 = 0x00000000`（Sa = 0 且 S.rgb = 0）、`color_2 = 0x01000000`（A = 1/255 ≈ 0，
             *   但**不为 0** ⇒ 不会被绘制早退挡掉）。这样无论 `colorMode` 选哪一路，
             *   顶点 alpha 都 ≈ 0 ⇒ `Da := Sa ≈ 0`（一手 `0x4032d0` 那块 `0x00000000` 四边形的语义），
             *   而 `blendMode(2)` 的 `rgb = D − Sa·S ≈ D` ⇒ **RGB 原样保留** ✓。*/
            if (s_alpha_zero_vm) {
                wipe_set_dual_color((uint32_t)s_alpha_zero_vm, TRANCE_ZERO_ARGB);
                (void)ce_anm_set_color2((uint32_t)s_alpha_zero_vm, TRANCE_ZERO_ARGB2);
            }
        }
#endif
    }

    /* ④⁗ ★★ **灵界遮罩层（过渡近似，2026-09-13 第三轮；用户口径「至少得有个遮罩」）**
     *   为什么用**矩形图元**而不是贴图：`drawRect` 不吃贴图 ⇒ 在 §CB-1（贴图错位）修好之前也能看见，
     *   而"变暗"这件事本身不需要图案。一手真身见 `engine/spirit/th13/02-trance-hud-and-popup.md` §2.4
     *   （EffectManager 回调 + 全屏四边形 + 4 支 astral VM，我们没有那条路）。
     *   淡入 20 帧 → 保持 → 淡出 20 帧：alpha 由**这里**写（`ce_anm_set_color` 的 0xAARRGGBB 带 alpha），
     *   脚本只画一张静止矩形 ⇒ 调参只改 `TRANCE_MASK_ALPHA`。*/
#if TRANCE_MASK_ENABLE
    {
        const float *pp = ce_player_pos();   (void)pp;
        if (s->core.phase != TRANCE_PHASE_IDLE) {
            if (!s_mask_vm) {
                s_mask_vm = (int32_t)ce_anm_spawn(CE_ABILITY_ANM(), CE_ANM_ABILITY_SCRIPT_TRANCE_MASK,
                                                  TRANCE_MASK_LAYER);
                /* ★ 第五轮诊断：用户报「灵界没有任何特效」时，这一行要能回答三个问题 ——
                 *   ① VM 到底建起来没有；② 引擎认到的 layer / pos 是多少（脚本没写活时看这里）；
                 *   ③ 贴图矩形是多少（`sprite(-1)` 缺失时 drawRect 不画，可矩形字段仍是 0——判据见脚本注释）。*/
                if (s_mask_vm) {
                    const uint8_t *mv = ce_anm_get_vm((uint32_t)s_mask_vm);
                    ce_log("trance: astral MASK VM %d up (script %d, layer asked %d%s) - "
                           "sprite(-1)+drawRect 近似，见 261_trance_mask.anm.txt",
                           (int)s_mask_vm, CE_ANM_ABILITY_SCRIPT_TRANCE_MASK, TRANCE_MASK_LAYER,
                           mv ? "" : " (VM 追不回！)");
                } else {
                    ce_log("trance: astral MASK **FAILED** (script %d) - ce_anm_spawn 返回 0",
                           CE_ANM_ABILITY_SCRIPT_TRANCE_MASK);
                }
            }
            if (s_mask_vm) {
                if (s_mask_alpha < TRANCE_MASK_ALPHA) {
                    s_mask_alpha += TRANCE_MASK_FADE_STEP;
                    if (s_mask_alpha > TRANCE_MASK_ALPHA) s_mask_alpha = TRANCE_MASK_ALPHA;
                }
                (void)ce_anm_set_color((uint32_t)s_mask_vm,
                                       TRANCE_MASK_RGB | ((uint32_t)s_mask_alpha << 24));
            }
        } else if (s_mask_vm) {                 /* 退出：先淡出，alpha 到 0 再删 */
            if (s_mask_alpha > 0) {
                s_mask_alpha -= TRANCE_MASK_FADE_STEP;
                if (s_mask_alpha < 0) s_mask_alpha = 0;
                (void)ce_anm_set_color((uint32_t)s_mask_vm,
                                       TRANCE_MASK_RGB | ((uint32_t)s_mask_alpha << 24));
            } else {
                ce_anm_delete((uint32_t)s_mask_vm);
                s_mask_vm = 0;
            }
        }
    }
#endif

    /* ④″ ★ **神灵换样式**（用户口径 ⑥）：灵界进 / 出各换一次（只在**真的变**的那一帧动手）。
     *   池是文件级 static ⇒ 用一个文件级闩锁记住"现在池里那批是哪一态"。
     *   ★★ **2026-09-13 第三轮修 bug**：闩锁必须在 `trance_spirit_restyle()` **之后**写 ——
     *     restyle 内部调 `trance_spirit_clear_all()`，而那个函数会把 `s_spirit_style` 归 0
     *     ⇒ 旧写法（先写闩锁再 restyle）等于每帧都判定"样式变了"，
     *     **每帧把池里所有神灵 VM 全部销毁重建一次**（实跑日志 103 行 `restyled` 连打 + 1500 帧内
     *     上千次 VM 建删 ⇒ 头号闪退嫌疑）。*/
    {
        const int want_style = (s->core.phase != TRANCE_PHASE_IDLE) ? 1 : 0;
        if (want_style != s_spirit_style) {
            const int had = s_spirit_style;
            trance_spirit_restyle(want_style);      /* ⚠ 内部会 clear_all ⇒ 把闩锁清 0 */
            s_spirit_style = want_style;            /* ★ 所以只能在这里写 */
            ce_log("trance: spirits restyled to %s (%d alive, latch %d->%d) - one-hand: engine picks "
                   "script type*6+%d", want_style ? "TRANCE" : "plain", s_n_spirit, had, want_style,
                   want_style ? 4 : 1);
        }
    }
    ce_trance_bgm_tick();                /* 未接管时是空操作 */

    /* ⑤ 结束反馈 */
    if (t.ended) {
        if (!t.ended_death_free) {
            /* ★ 主动档结束 ⇒ **1 秒无敌**（wiki 口径；被动档走 miss 无敌，不叠）*/
            s->end_invuln = TRANCE_END_INVULN_FRAMES;
            ce_log("trance: manual trance ended -> %d frames of end-invulnerability (THBWiki: 1 s, "
                   "sprite blinks)", TRANCE_END_INVULN_FRAMES);
        }
        if (t.ended_death_free) {
            /* ★ 只有**死亡触发**那条结束路播音（一手 `th13:0x405b80` 的 `test byte [esi+0x14],2` 分支
             *   → `th13:0x405bf8 MOV EDI,0x42` = `se_pldead01`，`th13:0x405c05` 播；声像取 `[PLAYER+0x5b8]`
             *   —— 就是"这一档灵界结束 = 真正的死亡"，播完才走 `Player::die` `th13:0x405c3e`）。
             * ⚠ 手动结束一手**不播音**（同一函数的非死亡分支里没有任何 play_sound）⇒ 我们也不播。*/
            ce_play_sound(TRANCE_SE_END, ce_player_x());
        }
        ce_log("trance: end (%s), gauge %d, frame %d%s", t.ended_death_free ? "death-free" : "manual",
               (int)s->core.gauge.gauge, (int)s->frame,
               t.re_die ? " — re-die owed (延时不是免死)" : "");
        ce_log("trance:   end sound %s (一手: 死亡触发那条 = id 0x%x se_pldead01; 手动那条 TH13 不播音)",
               t.ended_death_free ? "played" : "none", (unsigned)TRANCE_SE_END);
    }

    /* ⑥ ★ **击破 → 神灵实体 → （靠近才）入槽**（2026-09-12 第二轮：实体层就是这一步接上的）。
     * 帧末一次性取走队列（`on_enemy_death` 只入队，**死亡栈上不建 VM**），然后每一条按自己的颗数撒实体：
     *   · 类型 = `TRANCE_KILL_SPIRIT_TYPE`（**type 1**：SPEC §2.2 订正「击破走通道 B `spec1/spec2`」，
     *     一手 `SPIRIT` §3 通道表 B 行 `type=1`）；
     *   · ★★ **颗数 = 每条队列项自带的 `kn[j]`**（2026-09-13 起）—— 由 `on_enemy_death()` 按
     *     TH13 通道 B 的衰减公式算好（`trance_kill_spirit_count()`），**不再是常数 1**。
     *     ⚠ 撒的时候**逐颗调** `trance_spirit_spawn()`：池满/VM 建失败时那一颗由它自己记账
     *     （`s_stat_spirit_dropped`）⇒ **不在这里提前 break**，否则"池满丢了几颗"就统计不到了。
     *   · **位置 = 敌人死亡的 x/y 快照**（队列里带的就是它；`ev->enemy` 指针一律不碰）；
     *   · ★ **不再直接入账**：`TRANCE_KILL_AUTOCOLLECT` 那条「帧末按已收」的临时兜底已删除
     *     （实体层已实装 ⇒ 不再需要兜底）。入账现在只发生在 `trance_spirits_tick()` 的距离判定里。*/
    /* ★ 符卡状态采样（每帧一次；本帧稍后的敌人 tick 里的掉落事件要用它，见常量段）。*/
    {
        const uint8_t *sc = *(uint8_t *const *)(uintptr_t)TRANCE_SPELLCARD_PTR;
        if (sc && (*(const uint32_t *)(sc + TRANCE_SPELLCARD_FLAGS_OFF) & TRANCE_SPELLCARD_FLAG_ACTIVE))
            s_spell_active_seen = TRANCE_SPELL_SEEN_FRAMES;
        else if (s_spell_active_seen > 0)
            --s_spell_active_seen;
    }

    i = trance_kill_take(&s->core, s->frame, kx, ky, kn, TRANCE_KILL_QUEUE_MAX);
    if (i > 0) {
        int j, k, n = 0;
        s->stat_kill_drop += i;                       /* **击破次数**（不是一个击破算一颗）*/
        for (j = 0; j < i; ++j) {                     /* 一次击破 = 一条队列项（带 x/y 与颗数）*/
            const int cb = (int)s->core.phase != TRANCE_PHASE_IDLE;   /* 出生那一刻在不在灵界 */
            for (k = 0; k < kn[j]; ++k)
                /* 通道 B（死亡计时 spec1/spec2）= 0.5：击破掉灵走的就是这条。*/
                trance_spirit_spawn(kx[j], ky[j], TRANCE_KILL_SPIRIT_TYPE, cb, &s->core,
                                    TRANCE_SPIRIT_SPAWN_SPEED);
            n += kn[j];
        }
        s->stat_kill_spirit += n;                 /* 撒出去的颗数（不等于收到的颗数，见汇总日志）*/
        ce_log("trance: %d kill(s) -> spawn %d spirit VM(s) of type %d at (%d,%d) "
               "(pool %d/%d) [queue dropped %d, pool dropped %d]",
               i, n, TRANCE_KILL_SPIRIT_TYPE, (int)kx[0], (int)ky[0],
               s_n_spirit, TRANCE_SPIRIT_POOL, (int)trance_kill_queue_dropped,
               (int)s_stat_spirit_dropped);
        /* ★★ 实跑对账用：**逐条**把这一次击破的 (T, N, alive) 与算出来的 n 打出来
         *   （最多 4 条；`s_kd_total` 是这一帧的总次数）。判据见 `AUDIT` §CK-7：
         *   同一个敌人「刚出场就打死」与「拖 2 秒再打死」两条日志的 n 必须明显不同（前者大）。*/
        {
            const int shown = (s_kd_count < i) ? s_kd_count : i;
            ce_log("trance:   kill-count inputs (TH13 channel B): %d kill event(s) this frame, "
                   "%d queued, %d shown below; n = (T-alive)/(T/N)+1 (+1 on Hard/Lunatic); difficulty %d",
                   s_kd_total, i, shown, (int)*(volatile int32_t *)(uintptr_t)CE_ADDR_DIFFICULTY);
            for (j = 0; j < shown; ++j)
                ce_log("trance:   kill #%d: T=%d N=%d alive=%d hp=%d -> n=%d%s%s%s",
                       j, (int)s_kd[j].T, (int)s_kd[j].N, (int)s_kd[j].alive, (int)s_kd[j].hp,
                       (int)s_kd[j].n,
                       (s_kd[j].flag & 1) ? " [fallback (T,N) by HP band]" : " [native (T,N)]",
                       (s_kd[j].flag & 2) ? " [CLAMPED to TRANCE_KILL_SPIRIT_MAX]" : "",
                       (s_kd[j].flag & 4) ? " [dropped: kill queue full]" : "");
        }
    }
    /* 诊断环每帧清零（无论本帧有没有击破）——它只服务"这一帧那几条日志"。*/
    s_kd_count = 0;
    s_kd_total = 0;

    /* ⑦‴ ★ **通道 E**（TH13 `0x4434d8`）：死亡动画期间**每帧**在自机位置撒一颗白灵。
     *   位置取 `CE_PLAYER_X/Y`（`zPlayer.inner` 首字段 = 0x620/0x624 的 float3）；样式跟随相位
     *   （灵界里死 = 用灵界态脚本，与其它通道同口径）；速度/类型都是一手值。*/
    if (s->death_echo > 0) {
        uint8_t *pl = CE_PLAYER();
        if (pl) {
            const float px = *(float *)(pl + CE_PLAYER_X);
            const float py = *(float *)(pl + CE_PLAYER_Y);
            trance_spirit_spawn(px, py, TRANCE_DEATH_ECHO_TYPE,
                                s->core.phase != TRANCE_PHASE_IDLE, &s->core,
                                TRANCE_DEATH_ECHO_SPEED);
        }
        if (--s->death_echo == 0)
            ce_log("trance: channel E 窗口结束（共撒 %d 帧白灵）", TRANCE_DEATH_ECHO_FRAMES);
    }

    /* ⑦′ ★ **Boss 掉灵**（TH13 通道 A）的帧末落地：回调里只入队标量，这里才建 VM
     *   （理由与击破那条链完全一样：回调在敌人逻辑循环里，同帧可能删敌人）。
     *   位置 = `ev->x/y`（敌人世界坐标快照）、type = `TRANCE_KILL_SPIRIT_TYPE`（一手通道 A 就是 type 1）。*/
    if (s_boss_queue > 0) {
        int j;
        const int n = s_boss_queue;
        s_boss_queue = 0;
        for (j = 0; j < n; ++j)
            trance_spirit_spawn(s_boss_qx[j], s_boss_qy[j], TRANCE_KILL_SPIRIT_TYPE,
                                s->core.phase != TRANCE_PHASE_IDLE, &s->core,
                                TRANCE_SPIRIT_PARAM_BOSS);                        /* 通道 A = 2.0 */
        ce_log("trance: boss drop -> spawn %d spirit VM(s) of type %d (pool %d/%d) "
               "[boss hits %d, total drops %d, pool dropped %d]",
               n, TRANCE_KILL_SPIRIT_TYPE, s_n_spirit, TRANCE_SPIRIT_POOL,
               (int)s_stat_boss_hits, (int)s_stat_boss_drop, (int)s_stat_spirit_dropped);
    }

    /* ⑦″ ★★ **boss 相位批次的帧末喂出**（2026-09-13 第二轮；用户口径「击破符卡/非符也掉大量灵」）。
     *   回调（`on_enemy_drop_pre`）只把 (x, y, 颗数) 入队，VM 在这里建；每帧喂 8 颗、池满就等。
     *   ★ **顺序**在 `trance_spirits_tick()` 之前 ⇒ 本帧喂出去的那些当场就参与推进（与击破那条链一致）。*/
#if TRANCE_PHASE_DROP_ENABLE
    trance_phase_burst_tick(s);
#endif

    /* ⑦ ★ **神灵池每帧推进**（缩小 / 微动 / 靠近才吃 / 到点消失）—— 裁决四条都在这一个函数里。
     * ⚠ 顺序：**先撒后推**（本帧刚撒的也能吃到；`age` 从 1 开始，出生那帧的 scale 已在 spawn 里写过）。*/
    trance_spirits_tick(c);

    /* ⑧ 计分声明（模板 `EC` §3）：★ **已停用**（`SPEC §9 #5` 未拍 ⇒ 只记本地账，不发声明）。
     * 见 trance_declare_score()：那 10000 一旦发出去会污染 110/111 的 PV 聚合。*/
    trance_declare_score(s);

    /* ⑦′ ★ 魔理沙的灵界加成之一：**自机弹伤害**（挂 SDK 的伤害声明层，见文件头的长注释）。
     *   ★★ 2026-09-13：**1250‰ 已撤**（`TRANCE_MARISA_DMG_PERMILLE` 删除）——
     *   一手 TH13 灵界**没有伤害倍率**（伤害直接取 `row->dmg`，`th13:0x445c93`），
     *   增伤**全在 shot 组数据里**（dmg 14→22、rate 3→2）。灵界组已实装
     *   （见上面「灵界主炮组」段）⇒ 再留 1250 就是同一份增伤算两遍。
     *   这里只在**发现声明不是中性**时拉回 1000（旧存档 / 热重载不会把脏值留下）。*/
    if (s->dmg_decl != CE_DMG_UNIT) {
        s->dmg_decl = CE_DMG_UNIT;
        ce_set_bullet_damage_permille(c, CE_DMG_UNIT);
        ce_log("trance: bullet damage declaration -> %d permille (marisa 1250 permille buff removed)",
               (int)CE_DMG_UNIT);
    }

    /* ⑦″ ★ **灵界主炮组**（TH13 灵界 set10 复刻）—— 每帧只做一次**只读**体检：
     *   这一格（`CE_SHT_SET_TRANCE`）是否真的指着生成器写的那组行。跨越时打一行日志。
     *   旧版这里负责"每帧把偏移项指过去/还原"，收尾后不再需要（槽是我们自己的，见本节 §二）。*/
    (void)trance_sht_group_ok();

    /* ⑧′ ★ **调试键**（用户 2026-09-13）：拍空格爆一批神灵（见 `TRANCE_DEBUG_KEY` 的注释）。
     *   边沿触发（`dbg_key_prev`），只在游玩帧跑 ⇒ 不会在菜单/商店里误触发。*/
#if TRANCE_DEBUG_KEY
    {
        const int down = ce_key_down(CE_VK_SPACE) ? 1 : 0;
        if (down && !s->dbg_key_prev) {
            if (ce_key_down(CE_VK_SHIFT)) {
                /* Shift+空格：灌一格（200）。走 core 的加法 ⇒ 跨格判定与一手同一条路。*/
                trance_gauge_add_t ga = trance_gauge_add(&s->core.gauge, TRANCE_DEBUG_GAUGE_STEP);
                ce_log("trance: DEBUG gauge +%d by SHIFT+SPACE -> %d/%d (tier %d, crossed=%d, "
                       "phase %d) - press C to enter trance when full",
                       TRANCE_DEBUG_GAUGE_STEP, (int)s->core.gauge.gauge, TRANCE_GAUGE_MAX,
                       (int)s->core.gauge.tier, (int)ga.crossed_tier, (int)s->core.phase);
            } else {
                trance_debug_burst(c);
            }
        }
        s->dbg_key_prev = down;
    }
#endif

    /* ⑧″ ★ **左下角指示槽**（口径 #4）：每帧驱动 4 支 VM（见 `trance_hud_tick` 的逐条注释）。*/
    trance_hud_tick(s);

    /* ⑨ 汇总日志 */
    if (s->frame - s->log_frame >= TRANCE_LOG_EVERY) {
        s->log_frame = s->frame;
        ce_log("trance: gauge %d/%d tier %d phase %d, spirits %d/%d/%d/%d, "
               "chain %d(bonus %d spawned at DROP time), "
               "activations %d rescued %d redie %d resclost %d levelups %d killdrop %d killspirit %d "
               "(per-kill count, TH13 channel B: native %d fallback %d clamped %d max n %d) "
               "boss hits %d(drops %d, rejected %d) "
               "(boss PHASE batch: %d event(s) %d spirit(s) fed, %d queued now, %d event(s) lost, "
               "%d ignored: no spell card, %d skipped: final phase) "
               "| entity pool: %d/%d alive, spawned %d picked %d expired %d dropped %d out %d "
               "popup %d(fail %d, overflow %d) spawn# %d "
               "(motion: drift %d / attract %d / reimu-slow %d, character %d) "
               "(r=%d px, life %d f + script tail %d f = %d f, scale %d held %d f then ->%d over %d f; "
               "tick guard dup %d, age-probe lines %d), "
               "score-decl BLOCKED (decl %d, %d frames), %s "
               "| gauge-hud vms [%d %d %d %d] respawn %d tries %d "
               "| player-lock watch: %d frame(s) with state != 1 while in trance (0 = 灵界期间自机没被锁；"
               "见本文件 on_item_frame 的 ④-2b)",
               (int)s->core.gauge.gauge, TRANCE_GAUGE_MAX, (int)s->core.gauge.tier, (int)s->core.phase,
               (int)s->core.spirits_seen[0], (int)s->core.spirits_seen[1],
               (int)s->core.spirits_seen[2], (int)s->core.spirits_seen[3],
               (int)s->core.chain_count, (int)s->core.chain_bonus_spawned,
               (int)s->core.activations, (int)s->stat_rescued, (int)s->stat_redie,
               (int)s->stat_rescue_lost, (int)s->stat_levelups,
               (int)s->stat_kill_drop, (int)s->stat_kill_spirit,
               (int)s_stat_kill_native, (int)s_stat_kill_fallback, (int)s_stat_kill_clamped,
               (int)s_stat_kill_maxn,
               (int)s_stat_boss_hits, (int)s_stat_boss_drop, (int)s_stat_boss_bad,
               (int)s_stat_phase_events, (int)s_stat_phase_spirits,
               (int)trance_phase_queued(),
               (int)s_stat_phase_full, (int)s_stat_phase_nospell,
               (int)s_stat_phase_final,
               s_n_spirit, TRANCE_SPIRIT_POOL,
               (int)s_stat_spirit_spawned, (int)s_stat_spirit_picked,
               (int)s_stat_spirit_expired, (int)s_stat_spirit_dropped, (int)s_stat_spirit_out,
               (int)s_stat_popup, (int)s_stat_popup_failed, (int)s_popup_overflow,
               (int)s_spirit_spawn_index,
               (int)s_stat_state[TRANCE_STATE_DRIFT], (int)s_stat_state[TRANCE_STATE_ATTRACT],
               (int)s_stat_state[TRANCE_STATE_REIMU_SLOW],
               (int)*(volatile int32_t *)(uintptr_t)CE_ADDR_CHARACTER,
               TRANCE_SPIRIT_PICK_RADIUS, TRANCE_SPIRIT_LIFE_FRAMES,
               TRANCE_SPIRIT_SCRIPT_TAIL_FRAMES,
               (int)(TRANCE_SPIRIT_LIFE_FRAMES + TRANCE_SPIRIT_SCRIPT_TAIL_FRAMES),
               TRANCE_SPIRIT_SCALE_PERMILLE_BASE, TRANCE_SPIRIT_SCALE_DELAY,
               TRANCE_SPIRIT_SCALE_END_PERMILLE, TRANCE_SPIRIT_SCALE_FRAMES,
               (int)s_stat_tick_dup, (int)s_stat_age_probe_lines,
               (int)s->decl_last, (int)s->decl_blocked, ce_trance_bgm_state_name(),
               (int)s_hud_vm[0], (int)s_hud_vm[1], (int)s_hud_vm[2], (int)s_hud_vm[3],
               (int)s_hud_respawn, (int)s_hud_tries,
               (int)s_stat_locked_frames);
    }
}

/* ============================ ★ 死亡结算之后（虚表 +0x14）：**只记一行日志** ============================
 *
 * ★ **2026-09-12 第二版（含落点订正）：本回调不做任何动作。**
 *   它现在唯一的用途是**回执**：补死（`Player__die`）真的走完了死亡结算 ⇒ 实跑时能在日志里
 *   对出「补死落地了几次」。本卡**没有任何跨帧状态**需要在这里收（第一版的排队 / 记账位已删）。
 *   ⚠ 中弹救援**不在**这里：那一档走 `+0x0c`（见上），而且救下之后这次死亡根本不会被结算。
 *   ⚠ 「玩家死在灵界里」（灵界窗口还开着就死）这一档也不在这里做事：窗口会自然 −1/帧 走到 0，
 *     相位 2 ⇒ `trance_tick` 报 `re_die` ⇒ 交给帧点补死。
 * 返回值：0（一手：`Player__die` 的三个调用点都直接 `ret`，`+0x14` 的返回值无人读）。*/
static int on_death_frame2(ce_card_t *c)
{
    trance_card_t *s = st(c);
    if (!s) return 0;
    /* ★ 通道 E（一手 `th13:0x4434d8`）：这个钩子 = **"补死真的发生了"**（救下来的那次不会走到这里，
     *   见本节顶部的说明）⇒ 从这里开一个 30 帧的窗口，帧末每帧在自机位置撒一颗白灵。
     * ★★ 2026-09-16e 用户口径：「**撞弹掉灵应该仅在不足一格（< 200）的时候** —— 目前只要撞了都会爆」
     *   核实结论：一手那边**没有**一条 `gauge < 200` 的字面判断长在这一段里，但**效果完全等价** ——
     *   一手 `Player::die` `0x444a10` 会先调 `TranceManager::begin(…, 1)`（免费灵界）：槽 ≥ 200 时
     *   **成功 ⇒ 根本不进死亡流程**（`0x444a41` 直接 return）⇒ 也就走不到 `+0x14` 这一段；
     *   只有槽 < 200（`0x405a40` 的门）才会真的死 ⇒ 才爆灵。⇒ 这里按同一判据**显式挡一道**
     *   （我们的救援层可能因"别的卡先 claim / 已经在灵界"而没出手，那时引擎仍会走到 `+0x14`）。*/
    if (s->core.gauge.gauge >= TRANCE_GAUGE_MIN_ENTER) {
        s->death_echo = 0;
        ce_log("trance: +0x14 (frame %d, phase %d) - death settled **but gauge %d >= %d** "
               "⇒ channel E SKIPPED（一手：槽够就会先被免费灵界救下、进不了死亡分支；"
               "用户口径「仅不足一格才爆白灵」）",
               (int)s->frame, (int)s->core.phase,
               (int)s->core.gauge.gauge, TRANCE_GAUGE_MIN_ENTER);
        return 0;
    }
    s->death_echo = TRANCE_DEATH_ECHO_FRAMES;
    ce_log("trance: +0x14 (frame %d, phase %d, gauge %d) - death settled"
           " ⇒ channel E 开窗 %d 帧（每帧一颗 type %d 白灵，speed %.1f）",
           (int)s->frame, (int)s->core.phase, (int)s->core.gauge.gauge,
           TRANCE_DEATH_ECHO_FRAMES, TRANCE_DEATH_ECHO_TYPE, (double)TRANCE_DEATH_ECHO_SPEED);
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
 * → 跨格音 → **数字弹窗** → 奖励落地（命碎走引擎原生加法；符卡碎 / 最大得点仍是 ⏳，见下两条日志）。
 * 它**不碰 VM**（删 VM / 回收池槽是调用方的事）⇒ 池与记账解耦，单测能单独测 core 那一半。
 * ⚠ 弹窗 VM 是**本函数自己起的**（世界列表、层 13、54 帧后自己 delete）—— 池里没它的位置，
 *   但**删卡时必须收**（`trance_clear()` 的 `s_popup_vm[]` 段，`METHOD.md` 铁律二）。*/
/* ★ 2026-09-18（owner 口径）：TH13 的**点道具（蓝点）被收**也往共享 PIV 池写 **+100 显示/颗**
 *   （一手 `th13:0x42e8ca` 实参 100.0 → `PIV += ftol(100×100)` = +10000 存储 = +100 显示；
 *   见 `engine/faith/th13/01-point-item-value.md` 的四调用点表）。
 *   TH18 侧对位 = **type 2（金钱 = 点道具）**（见 `cards/mof.c` 的满 P 转换与 `IDEAS.md` §1.5 一手表）。
 *   接缝用**现成的**共享站点 `ce_item_collect`（`th18:0x44625f`，本来就在站点表里）⇒ 不新增断点/入場時刻。*/
static void trance_on_item_collect(ce_card_t *c, uint32_t type, const void *item)
{
    (void)item;
    if (type != TRANCE_ITEM_TYPE18_POINT) return;      /* type 2 = 金钱 / 点道具 */
    ce_piv_add(ce_card_id(c), TRANCE_PIV_POINT_ITEM);  /* +100 显示（一手 th13:0x42e8ca）*/
}

static void trance_on_spirit_collected(ce_card_t *c, int32_t type, float x, float y,
                                        int32_t born_trance)
{
    trance_card_t *s = st(c);
    trance_collect_t col;
    trance_reward_t r;
    int32_t popup;

    if (!s) return;
    if (!trance_collect(&s->core, type, &col)) return;

    /* ★ 奖励档按**出生旗标**（一手 `+0x84` bit0），不是收集时的相位 —— 见结构体注释。*/
    r = trance_reward(type, born_trance, 0);

    /* ★★ 2026-09-16g（清单第 ② 条）：**蓝灵的最大得点加成**落到卡内账
     *   （一手：type 1 普通 **+10** / **在灵界里收 +100**，常量 `0x4aeea4` / `0x4aeaf0`）。
     *   累计值由 `trance_declare_score()` 每帧声明给计分层（= 之后每颗点道具都多这么多，
     *   等价于一手"抬最大得点"）。⚠ 档位跟 `born_trance` 走（**一手口径**：奖励档看出生旗标）；
     *   用户 2026-09-16 的提醒「灵界中吃蓝灵是 +100」在常见情形（生在灵界、当场吃掉）与此一致，
     *   两者只在"生在灵界、出了灵界才吃"这种交叉情形不同 —— 差异登记在 `AUDIT.md` §DI。*/
    if (r.max_point > 0) {
        s->max_point_bonus += r.max_point;         /* 卡内账：HUD / 日志 / 对账继续用 */
        /* ★★ 2026-09-18（owner 口径）：这一档**现在有了载体** —— 写进**共享 PIV 池**（`ce_piv_add`）。
         *   语义就是「之后每颗点道具都多这么多」，与一手逐值对应：`th13:0x438baf`（神灵 type 1，+10 显示）、
         *   `th13:0x438d5e`（灵界内，+100 显示）—— 出处 `engine/faith/th13/01-point-item-value.md`
         *   的「第二加法器四调用点」表。池按 **th12–th17 那一族**归一（线下 ×5/6、线上 ×1），
         *   并按本卡可写上限（`cap_i − init_i`）设闸。
         *   ⇒ `SPEC §9 #5` 的「三条候选」里**选了第三条**（卡内计数 → 落点就在本函数），
         *   而不是原先那句「声明给计分层」（那条会让 110/111 的聚合把它当信仰量吃进去，见上方长注）。*/
        ce_piv_add(ce_card_id(c), r.max_point);
        if (s_stat_maxpoint_log < TRANCE_MAXPOINT_LOG_LIMIT) {
            ++s_stat_maxpoint_log;
            ce_log("trance: type 1 -> 最大得点 +%d（%s）累计 %d 点（"
                   "声明给计分层，见 trance_declare_score）",
                   (int)r.max_point, born_trance ? "灵界内" : "普通", (int)s->max_point_bonus);
        }
    }
    /* 收集音的声像 = **神灵实体的 x**（一手 `th13:0x438b38`：`MOV EDI,0x3f` 之后 `CALL 0x460c50`，
     * 而 `th13:0x438b17` 那一支算完距离就把 `spirit+0x14`（= 神灵 x）当 x 传进去的）。
     * ★ id `0x3f` 在**两作都是** `se_lgodsget`（TH13 名表 `th13:0x4bb320` / TH18 cfg 行 `th18:0x4c9b80`），
     *   即"神灵·拿到"那枚文件本身 ⇒ 这条是**复刻**，不是自定。*/
    ce_play_sound(TRANCE_SE_PICK, x);

    /* ★★ ③ 数字弹窗（一手 `FUN_0046f820(&out, script, spirit, 0, -1, 0)`，`th13:0x438b79` 那一族）——
     *   位置 = **神灵自己的坐标**（不是自机），脚本 50 帧上浮 16 px、第 54 帧自己 delete。
     *   变体：一手按 `spirit+0x84` bit0 选（= **出生时**在不在灵界）。我们的池把"当前样式"
     *   存在 `p->style` 里（进出灵界整批重建 ⇒ 它同时就是玩家看到的那一态）⇒ 用它选，
     *   🟡 与一手的差别只在"生在普通态、收在灵界态"这一种交叉情形（一手会用普通态那张）。
     *   type 3 原版**不起 VM**（它走 `0x44ad60` 改分数显示）⇒ 这里也不起。*/
    popup = trance_popup_script(type, born_trance);
    if (popup > 0) {
        int32_t id = (int32_t)ce_anm_spawn(CE_ABILITY_ANM(), popup, TRANCE_SPIRIT_LAYER);
        if (id) {
            (void)ce_anm_set_pos((uint32_t)id, x, y, 0.0f);
            /* ★★ 2026-09-13 第五轮：**显式给色**（用户实跑第 5 条「没有那个 +几 的 UI」）。
             *   一手 TH13 的弹窗脚本里没有 `color()` —— TH13 的 VM 模板默认是 `0xffffffff`。
             *   TH18 侧实测"VM 建起来了、sprite 也对、rect 也对，但整支是透明的"⇒ 模板默认色不含 alpha
             *   （本仓既有的 TH18 做法同样是在起 VM 之后立刻写色：`cards/season.c`）。
             *   脚本 255..260 里也补了 `color(255,255,255); alpha(255);`，这里是第二道保险。*/
            (void)ce_anm_set_color((uint32_t)id, 0xffffffffu);
            trance_popup_remember(id);            /* 记账：删卡时统一收（池满就丢最老的那支）*/
        }
        ++s_stat_popup;
        if (!id) ++s_stat_popup_failed;
        else if (s_stat_popup <= TRANCE_POPUP_LOG_LIMIT) {
            /* ★ 诊断（用户实跑第 3b 条"只有个色块"）：把**引擎当时认到的 sprite 号与 scale**
             *   一起打出来 —— 若 sprite 号不是我们编译进去的那个，问题在"脚本→sprite 解析"；
             *   若都对而画面仍是一块，问题在贴图/尺寸（见 `AUDIT` §CA-5b/§CA-7）。*/
            const uint8_t *vm = ce_anm_get_vm((uint32_t)id);
            if (vm)   /* ★ 有效矩形：应当是 32×8（弹窗贴图就是 32×8 或 24×8）*/
                ce_log("trance:   popup rect %.0fx%.0f px, layer %d, color 0x%08x, flags 0x%x "
                       "(贴图若是别的矩形 ⇒ 引擎认到的不是我们切的那张图，AUDIT §CA-9)",
                       (double)*(const float *)(vm + TRANCE_VM_SPRITE_W),
                       (double)*(const float *)(vm + TRANCE_VM_SPRITE_H),
                       (int)*(const int32_t *)(vm + TRANCE_VM_LAYER),
                       (unsigned)*(const uint32_t *)(vm + CE_ANM_VM_COLOR1),
                       (unsigned)*(const uint32_t *)(vm + TRANCE_VM_FLAGS_LO));
            ce_log("trance: collect popup VM %d (script %d, type %d, %s) at (%d,%d) "
                   "sprite_now %d scale (%.2f,%.2f)%s",
                   (int)id, (int)popup, (int)type, born_trance ? "trance" : "plain", (int)x, (int)y,
                   vm ? *(const int32_t *)(vm + CE_ANM_VM_SPRITE_ID) : -1,
                   vm ? (double)((const float *)(vm + CE_ANM_VM_SCALE))[0] : 0.0,
                   vm ? (double)((const float *)(vm + CE_ANM_VM_SCALE))[1] : 0.0,
                   vm ? "" : " (VM 追不回！)");
        }
    }

    if (col.tier_crossed) {
        const float *tp = ce_player_pos();
        ++s->stat_levelups;
        /* ★ 声像 0 = **一手原样**：TH13 跨格走的是**无参版** `th13:0x460bc0`（`th13:0x43d574 MOV EDX,0x44`），
         *   那个入口没有 x 参数 ⇒ 原版这条音就是居中的（不是我们偷懒）。
         * ★ 2026-09-16n：这枚**不再是替身** —— TH13 的 `se_astralup.wav` 随 mod 发（音效表扩容 id `0x57`，
         *   见 `TRANCE_SE_LEVELUP` 的注释与 `AUDIT` §DK-7）。*/
        ce_play_sound(TRANCE_SE_LEVELUP, 0.0f);
        /* ★ 图形提示（口径 #7）：一手在**自机位置**起一支 `front.anm` 的 100 帧 VM，
         *   脚本自己上浮 16 px。我们先收掉上一支（连满两格时不该两支叠着飞）。*/
        if (s_tierup_vm) { ce_anm_delete((uint32_t)s_tierup_vm); s_tierup_vm = 0; }
        s_tierup_vm = (int32_t)ce_anm_spawn(CE_ABILITY_ANM(),
                                            trance_tierup_script(s->core.gauge.tier),
                                            TRANCE_SPIRIT_LAYER);
        if (s_tierup_vm && tp)
            ce_anm_set_pos((uint32_t)s_tierup_vm, tp[0], tp[1] - TRANCE_TIERUP_LIFT, 0.0f);
        ce_log("trance: tier up -> gauge %d (tier %d), popup VM %d (script %d) at player%s",
               (int)s->core.gauge.gauge, (int)s->core.gauge.tier, (int)s_tierup_vm,
               trance_tierup_script(s->core.gauge.tier), tp ? "" : " (no player pos: at 0,0)");
    }
    /* ★★ 连锁白灵**不在这里**（2026-09-14 从"收集时刻"搬到"掉落时刻"）——
     *   一手 `spawn_spirit` 的尾部（`th13:0x4391a0`）才是这条链的落点，见 `trance_spirit_spawn()`
     *   尾部那一段。挂在收集上的版本会让"玩家连续吸灵"把 60 帧窗口一直撑住 ⇒ 第 10 颗之后每颗都白。*/
    /* 奖励落点：命碎 / 符卡碎走引擎原生加法（各自自带进位与 HUD 刷新）*/
    if (r.life_frag > 0) {
        int k;
        for (k = 0; k < r.life_frag; ++k) ce_add_life_fragment();   /* `sdk.h`：自带进位 + 刷 HUD */
    }
    if (r.spell_frag > 0) {
        int k;
        /* ★ 2026-09-13：SDK 补了 `ce_add_bomb_fragment()`（**逐条复刻**原生道具拾取那段，
         *   见 `sdk.h` 的注释与 `th18:0x446460`）：3 个碎片 = 1 个炸弹、炸弹满则不加。
         *   ⇒ 「绿灵（type 2）收了没反应」这条反馈就是这里以前只打日志造成的。*/
        for (k = 0; k < r.spell_frag; ++k) ce_add_bomb_fragment();
        ce_log("trance: spell (bomb) fragment +%d applied -> bombs %d frag %d/%d",
               (int)r.spell_frag, (int)CE_CURRENT_BOMBS(), (int)CE_BOMB_FRAGMENTS(), 3);
    }
    if (r.max_point > 0) {
        int32_t add = trance_max_point_field_addends(r.max_point, TRANCE_MAXPOINT_FIELD_UNIT);
        /* ⚠ 口径：**没有落笔**（`SPEC §9 #5` 未拍 ⇒ `trance_declare_score()` 已停发声明）
         * ⇒ 这条日志里的 add 是「若接线会声明多少」，不是已经加过的最大得点。*/
        ce_log("trance: max point +%d display (=%d field units) NOT applied "
               "(score layer STOPPED — SPEC 9 #5)", (int)r.max_point, (int)add);
    }
    ce_log("trance: collect type %d (%s-born) gauge +%d -> %d, lifeFrag +%d spellFrag +%d maxPt +%d "
           "chain (window count %d, bonus spawned total %d; chain runs at SPAWN, not here)",
           (int)type, born_trance ? "trance" : "plain",
           (int)col.gauge_add, (int)s->core.gauge.gauge, (int)r.life_frag, (int)r.spell_frag,
           (int)r.max_point, (int)s->core.chain_count, (int)s_stat_chain_bonus);
}

/* ============================ ★ 左下角**指示槽**（口径 #4；一手 `th13:0x42b030` 的每帧写） ============================
 *
 * 逐条对应一手（见文件头常量段的地址/字段）：
 *   · 起 4 支叶子 VM（缺哪支补哪支；`ce_anm_spawn_ui` = **UI 列表**，脚本里 `layer(24)` 写在 HUD 层）；
 *   · **框**：`gauge >= 200` ⇒ 亮图，`< 200` ⇒ 暗图（一手的 `中断 3`；**只在边沿换**）；
 *   · **总条**：`scale.x = (槽/200)/3 = 槽/600`（夹 0..1）；
 *   · **游标**：`scale.x = (槽%200)/200`、`entity_pos.x = 基点 + 满格数×32`（★ 第六轮订正，见宏注释）；
 *   · **淡出**：自机 `x < -64 && y > 384`（一手原值，**场地坐标**）⇒ `中断 5`，回来 ⇒ `中断 4`。
 * 一手注：**第二条（174/我们的 177）引擎不写** —— 它只靠中断 4/5 跟着淡入淡出，所以卡侧也不写它。
 *
 * ★★ 2026-09-13 第六轮的两条根因（用户实跑「扭曲程度减小了，但还是在左右动」）：
 *   ① **美术**：三条里 `TRANCE_GAUGE_BAR/BAR2` 早先是「48 宽的画布、内容只占左 16 px」
 *      （PIL 拿 48 宽去裁 16 宽的图 ⇒ 越界补透明）⇒ 一条上只有 1 片叶子、右 2/3 全空。
 *      一手：`agauge2.png` 贴图 **16 宽**，sprite123/124 声明 **48 宽** ⇒ 引擎按 `U1 = 48/16 = 3.0`
 *      + 采样器 **WRAP** 把它**平铺 3 次**（= 3 个槽）。本仓的图集不能越界 UV ⇒
 *      **在 `make_trance_art.py` 里把平铺烘进 PNG**（`_tile_cols`），声明尺寸仍是 48 ⇒ 引擎侧零改动。
 *   ② **几何**：游标的 `entity_pos.x` 写的是**格内比例**×16，一手是**满格数**×16（见 `TRANCE_HUD_MARK_SLIDE`）。*/
/* ★ 进度条落笔（第五轮）：`scale.x` 与 `uv_scale.u` **同时**写 = 裁切；`flags_lo |= 0x18` 与原生充能条一致。
 *   vertical 的尺寸仍走 `TRANCE_HUD_ART_SCALE`（我们的切图是原尺寸，脚本里没有 `scale()`）。*/
#define TRANCE_HUD_PROGRESS_SCALE(f)  ((f) * TRANCE_HUD_ART_SCALE)
static void trance_hud_set_progress(int32_t id, float f)
{
    uint8_t *vm = ce_anm_get_vm((uint32_t)id);
    if (!vm) return;
    /* ★★ 2026-09-13 第五轮补（用户实跑：「扭曲程度降低了，但还是不对」）：
     *   `uv_scale` 里的 f 是**贴图内**的采样比例，而我们的美术是 TH13 原尺寸、
     *   整体要靠 `TRANCE_HUD_ART_SCALE`（2.0）放大到 TH18 的 UI 尺度
     *   ⇒ **quad 的宽度必须是 `f × ART_SCALE`**，否则横向只有纵向的一半（= 又扁又歪）。
     *   旧写法写成 `scale.x = f`（= 不放大），所以裁切对了、比例还是错的。*/
    ((float *)(vm + CE_ANM_VM_SCALE))[0] = TRANCE_HUD_PROGRESS_SCALE(f);
    ((float *)(vm + CE_ANM_VM_SCALE))[1] = TRANCE_HUD_ART_SCALE;
    *(float *)(vm + TRANCE_VM_UV_SCALE) = f;
    *(uint32_t *)(vm + TRANCE_VM_FLAGS_LO) |= 0x18u;
}

static void trance_hud_tick(const trance_card_t *s)
{
    void *anm = CE_ABILITY_ANM();
    const float *pp = ce_player_pos();
    const int32_t gauge = s->core.gauge.gauge;
    const int32_t seg = gauge % TRANCE_GAUGE_TIER;             /* 当前格内进度 */
    const int32_t seg_full = gauge / TRANCE_GAUGE_TIER;        /* 已满格数（0..3）*/
    /* ★★★ 2026-10-03x：每帧补认领 + 取最新槽（换关 / 纪元清表 / 碰撞回退都跟得上）*/
    trance_hud_claim();
    (void)ce_hud_slot_get(TRANCE_NUM, &s_hud_slot);
    /* ★★★ 2026-10-03z16：**绝对定位**（每帧把四支 VM 放到"设计位置 + 槽位移"上）——
     *   与 112 同款；不再用增量差值（脚本改 `pos` 时增量会漂，墨迹探针已实测到漂移 100+ px）。*/
    {
        int q2;
        for (q2 = 0; q2 < TRANCE_HUD_N; ++q2) {
            uint8_t *vm2;
            const float *sp2, *p22;
            float *ep2;
            if (!s_hud_design_ok[q2] || !s_hud_vm[q2]) continue;
            vm2 = ce_anm_get_vm((uint32_t)s_hud_vm[q2]);
            if (!vm2) { s_hud_design_ok[q2] = 0; continue; }
            sp2 = (const float *)(vm2 + CE_ANM_VM_SCRIPT_POS);
            p22 = (const float *)(vm2 + CE_ANM_VM_POS_2);
            ep2 = (float *)(vm2 + CE_ANM_VM_POS);
            ep2[0] = (s_hud_design_x[q2] + TRANCE_HUD_DX) - (sp2[0] + p22[0]);
            ep2[1] = (s_hud_design_y[q2] + TRANCE_HUD_DY) - (sp2[1] + p22[1]);
        }
        s_hud_applied_dx = TRANCE_HUD_DX;
        s_hud_applied_dy = TRANCE_HUD_DY;
    }
    /* ★ 2026-09-15g：门 = 「自机的体量碰到本卡指示槽的块」——矩形每帧现算（块是死的，
     * 但走共享件 = 与别的卡同一套动态）；alpha 仍在 anm 脚本里（见下面 ⑤ 的注释）。*/
    int hidden;
    {
        hud_dim_rect_t r;
        /* ★★★ 2026-10-03z10（**撤销 z4 的"喂带"**）：owner 实跑「**113 直接看不见了**」——
         *   本卡的 `hud_dim_gate()` 是**二值 hide 门**（不是 alpha 斜坡）：喂"带"之后，
         *   自机一进整条带（= 站位常在底部）**整块面板就消失**。
         *   ⇒ 退回**自己那块**矩形。⚠ "一起淡"要等它改成 alpha 斜坡型消费者（或带的语义支持 hide 型）再说。*/
        hud_dim_rect_surface(&r, TRANCE_HUD_BASE_X + TRANCE_HUD_DX,
                             TRANCE_HUD_BLOCK_BOTTOM + TRANCE_HUD_DY,
                             TRANCE_HUD_BLOCK_W, TRANCE_HUD_BLOCK_H);
        hidden = hud_dim_gate(s_hud_dim, &r, pp);
        s_hud_dim = hidden;
    }
    int k;

    if (!anm) return;

    /* ★★★ 2026-10-03z11（owner：「**113没接统一变暗**」）——
     *   本卡的压暗原来是**二值 hide 门**（`hud_dim_gate()` ⇒ 边沿发中断 4/5，整块显/隐），
     *   所以 z4 那版"喂整条带"会让它**整块消失**（owner 实跑「113 直接看不见了」）而退回 z10。
     *   ⇒ 正解：**两条腿分开** ——
     *     · **hide 门**（自机真的压在面板上）继续用**自己那块**矩形（保持"让开自机"的原设计）；
     *     · **统一变暗**改成 **alpha 斜坡**：每帧把共享带的 alpha 写进 4 支 VM 的颜色
     *       （只改 A、不动贴图 RGB）⇒ 自机进带时整块**跟着淡**、但不会消失。
     *   ⚠ 与 108/109/112 同款（alpha 斜坡型消费者），这正是「一起淡」的正确接法。*/
    {
        const uint32_t a = (uint32_t)ce_hud_band_dim_alpha() & 0xffu;
        int q2;
        for (q2 = 0; q2 < TRANCE_HUD_N; ++q2) {
            uint8_t *vm2 = s_hud_vm[q2] ? ce_anm_get_vm((uint32_t)s_hud_vm[q2]) : 0;
            uint32_t *cd;
            if (!vm2) continue;
            cd = (uint32_t *)(vm2 + CE_ANM_VM_COLOR1);
            ce_anm_set_color((uint32_t)s_hud_vm[q2], (a << 24) | (*cd & 0x00ffffffu));
        }
    }

    /* ① 缺哪支补哪支（一手在关卡开始一次性建；我们每帧兜底：anm 重载 / 首帧还没装载都能自愈）
     *   ⚠ 诊断：**重建次数要记**（若每帧都在重建 ⇒ 说明 id 追不回 VM ⇒ 会像漏水的桶一样涨到崩）。
     *   ★★ 2026-09-13 第三轮：**兜底必须有上限** —— 若 `ce_anm_spawn_ui()` 因为任何原因返回 0
     *     （或 id 立刻追不回），"每帧重试"就等于**每帧新建一支 VM 且不记账**（= 泄漏 ⇒ 闪退）。
     *     所以试满 `TRANCE_HUD_SPAWN_TRIES` 帧还不成 ⇒ 放弃本关、打一行日志、不再重试。*/
    ++s_hud_tries;
    if (s_hud_gave_up) return;
    for (k = 0; k < TRANCE_HUD_N; ++k) {
        if (s_hud_vm[k] && ce_anm_get_vm((uint32_t)s_hud_vm[k])) continue;
        if (s_hud_vm[k]) ++s_hud_respawn;      /* 之前有 id、现在追不回 ⇒ 记一笔（危险信号）*/
        {
            const float base[3] = { TRANCE_HUD_BASE_X, TRANCE_HUD_BASE_Y, 0.0f };
            s_hud_vm[k] = (int32_t)ce_anm_spawn_ui(anm, trance_hud_script(k), base);
            if (s_hud_vm[k]) {                 /* ★ z16：记下"设计位置"（脚本按基点算出来的那个点）*/
                uint8_t *nvm = ce_anm_get_vm((uint32_t)s_hud_vm[k]);
                if (nvm) {
                    const float *nsp = (const float *)(nvm + CE_ANM_VM_SCRIPT_POS);
                    const float *np2 = (const float *)(nvm + CE_ANM_VM_POS_2);
                    const float *nep = (const float *)(nvm + CE_ANM_VM_POS);
                    s_hud_design_x[k] = nsp[0] + np2[0] + nep[0];
                    s_hud_design_y[k] = nsp[1] + np2[1] + nep[1];
                    s_hud_design_ok[k] = 1;
                }
            }
        }
        if (!s_hud_vm[k]) {                    /* anm 没装载 / 建失败 ⇒ 下一帧再试（有上限）*/
            if (s_hud_tries >= TRANCE_HUD_SPAWN_TRIES) {
                s_hud_gave_up = 1;
                ce_log("trance: gauge HUD GIVE UP after %d frames - ce_anm_spawn_ui returned 0 "
                       "(anm %p, base (%.0f,%.0f)) => 本关没有指示槽（诊断见 AUDIT §CA-5）",
                       (int)s_hud_tries, anm, (double)TRANCE_HUD_BASE_X, (double)TRANCE_HUD_BASE_Y);
            }
            return;
        }
    }
    if (!s_hud_up_logged) {                    /* 只打一次：起全了 */
        s_hud_up_logged = 1;
        /* ★ 2026-09-13 第三轮：日志里带上**每支的实测落点**（`entity_pos + 脚本的 pos`）——
         *   用户报「看不见」时，第一条该看的就是"它到底画在屏幕的哪一点上"。
         *   ⚠ 屏面是 1280×960（表面 px），游玩区 = x 257..1023、y 32..928（用户量过）。
         *   `+0x5f0` = 实体坐标（我们出生时给的基点），脚本里的 `pos()` 是相对它的偏移。*/
        ce_log("trance: gauge HUD up - vm ids [%d %d %d %d] scripts [%d %d %d %d], base (%.0f,%.0f), "
               "screen (248,928) = 游玩区左下角, took %d frames, respawns %d (anm %p)",
               (int)s_hud_vm[0], (int)s_hud_vm[1], (int)s_hud_vm[2], (int)s_hud_vm[3],
               trance_hud_script(0), trance_hud_script(1), trance_hud_script(2), trance_hud_script(3),
               (double)TRANCE_HUD_BASE_X, (double)TRANCE_HUD_BASE_Y, (int)s_hud_tries, (int)s_hud_respawn,
               anm);
        for (k = 0; k < TRANCE_HUD_N; ++k) {
            const uint8_t *vm = ce_anm_get_vm((uint32_t)s_hud_vm[k]);
            const float *ep = vm ? (const float *)(vm + CE_ANM_VM_POS) : 0;
            ce_hud_vm_probe("trance.hud", (uint32_t)s_hud_vm[k]);
            ce_log("trance:   hud[%d] script %d entity_pos (%d,%d) sprite %d%s",
                   k, trance_hud_script(k), ep ? (int)ep[0] : -9999, ep ? (int)ep[1] : -9999,
                   vm ? *(const int32_t *)(vm + CE_ANM_VM_SPRITE_ID) : -1,
                   vm ? "" : " (VM 追不回！)");
        }
    }

    /* ② 框的亮/暗两态（一手：槽 < 200 发中断 3 换成暗的那张）*/
    {
        const int32_t dark = (gauge < TRANCE_GAUGE_TIER) ? 1 : 0;
        if (dark != s_hud_frame_dark) {
            s_hud_frame_dark = dark;
            (void)ce_anm_set_sprite(anm, (uint32_t)s_hud_vm[0],
                                    dark ? CE_ANM_ABILITY_SPRITE_TRANCE_GAUGE_FRAME_B
                                         : CE_ANM_ABILITY_SPRITE_TRANCE_GAUGE_FRAME_A);
        }
    }
    /* ②b 框与第二条的**尺寸 ×2**（它们的脚本里没有 `scale()`，默认 1.0 = 原版的一半；
     *     一手的尺寸是脚本给的，我们这一版没有，所以每帧写一次幂等值 —— 只在边沿写也行，
     *     但每帧写更省心（`ce_anm_set_scale` 就是两个 float store）。*/
    (void)ce_anm_set_scale((uint32_t)s_hud_vm[0], TRANCE_HUD_ART_SCALE, TRANCE_HUD_ART_SCALE);
    (void)ce_anm_set_scale((uint32_t)s_hud_vm[2], TRANCE_HUD_ART_SCALE, TRANCE_HUD_ART_SCALE);

    /* ③ 总条：`scale.x = uv_scale.x = 槽/600`（一手 `(槽/200)/3`，同一件事）
     *   + **尺寸 ×2**（见 `TRANCE_HUD_ART_SCALE`）。
     *   ★ 第五轮：改成"裁切"而非"拉伸" —— `scale.x` 与 `uv_scale.u` **一起**写（见上面宏的注释）。*/
    {
        float f = (float)gauge / (float)TRANCE_GAUGE_MAX;
        if (f < 0.0f) f = 0.0f;
        else if (f > 1.0f) f = 1.0f;
        trance_hud_set_progress(s_hud_vm[1], f);
    }

    /* ④ 游标：`scale.x = uv_scale.x = (槽%200)/200`（格内填充比例）
     *   + `entity_pos.x = 基点 + 满格数×32` —— **按整格跳，格内不平移**（见 `TRANCE_HUD_MARK_SLIDE`）。
     *   ⚠ `ce_anm_set_pos` 写的是 **entity_pos**（`+0x5f0`），而绘制位置 = `pos + entity_pos`
     *     ⇒ 这里只给"基点 + 满格跳步"这一段，脚本里的 `pos(48,0)` 由引擎自己加（一手 `th18:0x48171f`）。
     *   ★ 第六轮修（用户实跑「还是在左右动」）：旧写法这里是 `f * TRANCE_HUD_MARK_SLIDE`，
     *     即**格内比例**也要乘一次 ⇒ 游标在格内一边变长一边右移 32 px。
     *     一手是**满格数**（x87 栈收支 + "游标右缘 ≡ 总条右缘"两条判据，见宏注释与
     *     `engine/spirit/th13/01` §8 的订正注）。*/
    {
        float f = (float)seg / (float)TRANCE_GAUGE_TIER;
        if (f < 0.0f) f = 0.0f;
        else if (f > 1.0f) f = 1.0f;
        trance_hud_set_progress(s_hud_vm[3], f);
        (void)ce_anm_set_pos((uint32_t)s_hud_vm[3],
                             TRANCE_HUD_BASE_X + TRANCE_HUD_DX + (float)seg_full * TRANCE_HUD_MARK_SLIDE,
                             TRANCE_HUD_BASE_Y + TRANCE_HUD_DY, 0.0f);
    }
    /* ⑤ 诊断（第六轮）：一帧/秒 —— 用户报"还在动"时，这一行能一眼分开三种可能：
     *   · `mark 左` 在**格内**连续变（而不是每满一格跳 32）⇒ 是 ④ 那条写错了；
     *   · `uv` 与 `满格/格内` 对不上 ⇒ 裁切量/总条算错；
     *   · `满格=3 时 mark 左=392` 且等于条右缘 ⇒ 几何对，那就是**美术（平铺）**问题。
     *   期望关系（一手）：`mark 左 = 296 + 32×满格`（296 = 基点 248 + 脚本 pos 48），
     *   总条右缘 = `296 + 96×(槽/600)`，且 **mark 左 + 32×(格内/200) ≡ 总条右缘**（同一个数）。*/
    if (++s_hud_diag_frame >= TRANCE_HUD_DIAG_PERIOD) {
        const float fill_right = 296.0f + 96.0f * ((float)gauge / (float)TRANCE_GAUGE_MAX);
        const float mark_left  = 296.0f + (float)seg_full * TRANCE_HUD_MARK_SLIDE;
        const float mark_right = mark_left
                               + TRANCE_HUD_MARK_SLIDE * ((float)seg / (float)TRANCE_GAUGE_TIER);
        s_hud_diag_frame = 0;
        ce_log("trance: gauge HUD %d/600 -> 满格 %d 格内 %d | fill uv %.3f 右缘 %.0f | "
               "mark 左 %.0f 右缘 %.0f (应 == 条右缘) | 美术 BAR 48px = 3x16px 平铺",
               (int)gauge, (int)seg_full, (int)seg,
               (double)((float)gauge / (float)TRANCE_GAUGE_MAX), (double)fill_right,
               (double)mark_left, (double)mark_right);
    }

    /* ⑤ 淡出（一手：自机贴到槽上 ⇒ 中断 5 淡到 32；离开 ⇒ 中断 4 淡回 255）。
     * ★★ 2026-09-15g（跨卡统一）：**门**已换成共享件按本卡块矩形推出来的那一套（见上面 `hidden`）；
     *   深度与帧数都统一（脚本 175–178 的 `interruptLabel(5)`；一手 TH13 原值是 `alphaTime(10,0,32)`
     *   ⇒ 深度同值、帧数按 owner 口径统一成 20）。
     * ★★ 2026-09-15j：**过渡也统一完了**（用户口径「**过渡帧得根治**」）—— 脚本 175–178 的四支
     *   `interruptLabel(5): alphaTime(4, 0, 32)`（压暗）/ `interruptLabel(4): alphaTime(4, 0, 255)`（淡回），
     *   原值 10 / 30 帧退役；当年与 C 侧 ±56（4 帧）同帧数。
     * ★★ 2026-09-21：**又换了一档基准**（owner：20 帧 / 50% 统一）⇒ 四支现在是
     *   `alphaTime(8, 0, 64)` / `alphaTime(8, 0, 255)`，与共享件 `HUD_DIM_FRAMES`(8) /
     *   `HUD_DIM_ALPHA_DIM`(64) 同帧数同终点（机器判据 `native/tools/anm_hud_alpha_check.py`）。
     *   ⚠ **四支一起改**：本函数下面那个循环对 `s_hud_vm[0..3]`（脚本 175/176/177/178）**一起**发 4/5
     *   ⇒ 只改 175/176 会把这一排统一成一半。**幕布（261–271）没动**。
     *   ⚠ **alpha 仍然只由脚本管**（本卡不写这四支的 `color`）：在 C 侧硬写会与脚本的 `alphaTime` /
     *   `colorTime`（175/176 那条 60 帧脉冲）抢同一个 `color_1`（`+0x524`，见 `sdk.h` 的 `ce_anm_mirror_color`）。
     *   机器判据：`native/tools/anm_hud_alpha_check.py`（`make check`）。见 `AUDIT.md` §DD-9-1。*/
    if (hidden != s_hud_hidden) {
        s_hud_hidden = hidden;
        for (k = 0; k < TRANCE_HUD_N; ++k)
            if (s_hud_vm[k]) ce_anm_interrupt((uint32_t)s_hud_vm[k], hidden ? 5 : 4);
    }
}

/* ==================== ★★ 灵界形态（白剪影）：每帧抢写本体 sprite ====================
 * 一手（TH13）：进灵界那一刻 `Player::begin` 把**脚本 5** 拷进本体 VM
 *   （`th13:0x405b43` `copy_vm(player+0x14, player+0x10, 5)` + `0x405b49` 跑一次）——
 *   而 TH13 的脚本 5..9 与普通的 0..4 **逐条同构**（8/8/12/8/12 帧、step 5/1/1/1/5、
 *   同样的 offset / jump 结构），**只有引用的 sprite 不同**：
 *     普通 = 0..23（`pl0X.png`）；灵界 = 43..66（pl00）/ 45..68（pl01）/ 34..57（TH13 pl02），
 *     全部落在各自的**灵界专用贴图** `pl00f.png` / `pl01d.png` / `pl02c.png` 上
 *     （同一矩形网格上的平均亮度 142 / 117 / 127，而普通图是 80 / 46 / 62 ⇒ 一眼可分的白剪影）。
 *   ⇒ **「灵界形态」在原版就是"换 sprite 号"，不是"换脚本"**：脚本内容一字不差。
 *
 * 本卡的实现把这句话原样搬过来，但**不换脚本**（换脚本 = VM 重启、丢状态；`cards/youkai.c`
 *   的一手注里也明确不做）。做法：本体脚本每帧把 `vm+0x24` 写成 0..23，我们**在绘制前**把它
 *   换成 `0..23 + CE_PL_ANM_FORM_BASE_*`（= 追加进 pl0X.anm 的那 24 张**原版现切**贴图的 sprite 号，
 *   `assets/build_player_anm.py` 的 `TH13_TRANCE_ENTRIES`）。脚本、帧序、步长全是引擎自己的
 *   ⇒ 逐帧对齐原版，不引入任何自定的动画节奏。
 *
 * 三条硬约束（`youkai.c` 的一手逐条，照抄、不自创）：
 *   ① **不能直写** `player+0x14+0x24` —— `AnmLoaded__set_sprite`（`0x477b00`）还会把 UV
 *      （`vm+0x3ac..0x3b8` / `0x3c0..0x3c8`）、尺寸基准（`+0x74/+0x78`）与 scale 反比一起算出来
 *      （`477b2e`–`477d1a`）；直写会让这些缓存停在上一张图上 ⇒ 必须调函数。
 *   ② 传的 anm 必须是 **`player+0x0c`（pl0X.anm）**：绘制时贴图取自「VM 自己那一页的 anm」
 *      （`th18:0x481117`–`0x481171`）。`player+0x10` 开火期间被换成 ability.anm（engine.h 的注）
 *      ⇒ 传错 = UV 从一个 anm 取、贴图从另一个取，画出来是错的。
 *   ③ 本体 VM 是**内嵌 zAnmVm**（`player+0x14`）、**没有 anm id** ⇒ 走不了 sdk.h 的
 *      `ce_anm_set_sprite(anm, id, sprite)`（它内部走 `ce_anm_get_vm(id)`）；直接调 `0x477b00`、
 *      复用 sdk.h 的 `ce_fn_anm_set_sprite_t` 签名（**不改共享件**）。
 *
 * 记账（`s->form_biased`）：引擎每次 `ins_300` 都会把它写回 0..23 ⇒「读到的值 ≠ 我们上次写的值」
 *   就是「引擎刚换了帧」，这时才写一次（step 1 的段落每帧换、step 5 的五帧换一次，两边都不空转）。
 *   帧号落在 0..23 之外（形态外的号：24+ 的死亡 / 其它脚本）⇒ **不接形态**并作废记账。
 *   退出灵界（或删卡）时把偏移**减回去一次**，免得最后一帧留着一张白剪影。
 *
 * ★ 咲夜的例外：TH13 没有这个自机 ⇒ 她那份形态图**不是原版成品**，而是拿她自己的 TH18 图过一遍
 *   **从 TH13 三对原版图量出来的**色调变换（`AUDIT` §DG-7，**❓自定**）。机制与三机体完全相同。*/
static const int32_t TRANCE_FORM_BASE[4] = {
    CE_PL_ANM_FORM_BASE_PL00,   /* 0 灵梦   —— TH13 `pl00` sprite43..66 */
    CE_PL_ANM_FORM_BASE_PL01,   /* 1 魔理沙 —— TH13 `pl01` sprite45..68 */
    CE_PL_ANM_FORM_BASE_PL02,   /* 2 咲夜：TH13 **没有这个自机**（既无贴图也无脚本）⇒ **没有成品可切**。
                                 *   用户 2026-09-16 定案：拿**她自己的 TH18 那 24 帧**、施加从 TH13
                                 *   三对原版图量出来的「普通 → 灵界」色调变换 ⇒ **❓自定**
                                 *   （`AUDIT` §DG-7；性质同她的灵界弹 §DF-3）。*/
    CE_PL_ANM_FORM_BASE_PL03,   /* 3 早苗   —— TH13 `pl02` sprite34..57 */
};
#define TRANCE_FORM_FRAMES 24   /* 三个机体都是 24 帧（与本体 0..23 一一对应；脚本 8/8/12/8/12）*/

/* 真的去换 sprite（三处调用共用；`anm` 必须是 pl0X.anm、`vm` = player+0x14）。*/
static void trance_form_set_sprite(void *anm, uint8_t *vm, int32_t sprite)
{
    ((ce_fn_anm_set_sprite_t)CE_FN_ANM_SET_SPRITE)(anm, vm, sprite);
}

/* 把偏移减回去一次（`on_draw` 的退出沿、以及 `trance_clear()` 的删卡 / 局末路共用）。
 * ⚠ 只有"当前值 == 我们写进去的那个值"才减 —— 引擎已经写了别的帧号时减了会写坏。*/
static void trance_form_restore(trance_card_t *s)
{
    uint8_t *p;
    void *anm;
    int32_t v;

    if (!s || !s->form_biased) return;
    p = CE_PLAYER();
    anm = p ? *(void **)(p + CE_PLAYER_ANM) : 0;
    if (p && anm) {
        v = *(int32_t *)(p + CE_PLAYER_VM + CE_ANM_VM_SPRITE_ID);
        if (v == s->form_biased)
            trance_form_set_sprite(anm, p + CE_PLAYER_VM, v - s->form_base);
    }
    s->form_biased = 0;
    if (s->form_on) {
        s->form_on = 0;
        ce_log("trance: 灵界形态 OFF —— 本体 sprite 已还原（退出 / 删卡）");
    }
}

/* 每帧（`on_draw` = 绘制相位、`+0x48`）：在灵界里把本体帧号加上形态偏移；出灵界减回去。*/
static void trance_form_tick(trance_card_t *s)
{
    uint8_t *p;
    void *anm;
    int32_t base, v, ch;

    if (!s) return;
    p = CE_PLAYER();
    if (!p) { s->form_biased = 0; return; }              /* 自机不在（菜单 / 结算）⇒ 记账作废 */
    anm = *(void **)(p + CE_PLAYER_ANM);
    if (!anm) { s->form_biased = 0; return; }
    v = *(int32_t *)(p + CE_PLAYER_VM + CE_ANM_VM_SPRITE_ID);

    ch = trance_character();
    base = TRANCE_FORM_BASE[(ch >= 0 && ch <= 3) ? ch : 0];
    if (s->core.phase != TRANCE_PHASE_IDLE && base > 0) {
        if (v >= 0 && v < TRANCE_FORM_FRAMES) {
            if (v + base != s->form_biased) {            /* 引擎刚换了帧（或第一帧）⇒ 加一次偏移 */
                trance_form_set_sprite(anm, p + CE_PLAYER_VM, v + base);
                s->form_biased = v + base;
                s->form_base = base;
                if (!s->form_on) {
                    s->form_on = 1;
                    ce_log("trance: 灵界形态 ON —— 本体 sprite 0..23 +%d（= 追加 entry 的 %d..%d，"
                           "TH13 原版灵界贴图现切；脚本不换，只换帧号）", (int)base,
                           (int)base, (int)(base + TRANCE_FORM_FRAMES - 1));
                }
            }
        } else if (v != s->form_biased) {                /* 形态外的号（24+）⇒ 不接，作废记账 */
            s->form_biased = 0;
        }
    } else {
        trance_form_restore(s);
    }
}

/* ============================ HUD（SPEC §9 #6 ⏳；落点见 TRANCE_HUD_* 注释） ============================ */

static int on_draw(ce_card_t *c)
{
    trance_card_t *s = st(c);
    char buf[24];
    int32_t cs;                 /* 剩余百分之几秒（整数，避免任何浮点 → dllx87 门）*/
    int32_t sec, frac;
    int     capped;             /* 实际剩余 > 9.99s ⇒ 显示封顶并闪黄（2026-09-16 用户口径）*/

    if (!s) return 0;

    /* ★★ 灵界形态（白剪影）：必须在这一相位（绘制前）动手 —— 本体脚本每帧写 `vm+0x24`，
     *   我们抢写 `AnmLoaded__set_sprite` 的成品（UV/尺寸缓存一起更新），见上面那段长注释。*/
    trance_form_tick(s);

    /* ★ 绘制路径也镜像一次"引擎把 BGM 停了没有"：暂停菜单期间**游玩帧停摆**，
     *   `on_item_frame` 那条路走不到，而这里（HUD 绘制）暂停时照跑 ⇒ 第二路不会
     *   在暂停时继续响。只做这一件事，不推进淡化、不补 buffer。*/
    ce_trance_bgm_pause_poll();

    /* ★ 2026-09-13：**槽值那一行文字撤了** —— 左下角的**指示槽**（`trance_hud_tick` 驱动的 4 支 VM）
     *   已经把"几格 / 格内进度"画出来了，再叠一行 `gauge 600` 是重复（一手也只有图形槽 + `%.2fs`）。
     *   这一行原来是为"没有图形槽时的兜底"写的（❓自定），现在使命结束。*/
    ce_hud_vm_text_clear(&s_txt_gauge);

    /* 剩余秒数行：一手是 `%.2fs`（`th13:0x405940`）⇒ 这里打成 `3.45`（`s` 打不出来）。
     * ★ 2026-09-13：改走 `trance_remaining_centisec()` —— 因为**咲夜的槽消耗是每 2 帧 −1**
     *   （机体特技"灵界时间延长"），直接拿槽值当帧数会把她那 20 s 显示成 10 s。*/
    cs = trance_remaining_centisec(&s->core);     /* 百分之几秒（整数，已含机体倍率）*/
    /* ★★ 2026-09-15：字形换成 **TH13 那套**（`CE_HUD_FONT_TH13_BIG`，来自 TH13 `ascii.png` 的
     *   大字体 32×40 字格）—— 用户口径「不要用 th18 遗留的什么东西，得是 ascii 那张图的 anm」。
     *   于是 TH13 的 `%.2fs` 在这里是**逐字复刻**：数字 + `.` + `s` 全部来自原版切图
     *   （切图/图集登记见 `assets/ability/make_th13_ascii_art.py` 与 AUDIT §CZ）。*/
    /* ★ 2026-09-16 用户口径：**显示按实际剩余时间，但封顶 9.99s**（咲夜那档实际能到 20 s），
     *   封顶期间这一串在**黄 / 白**之间按 `TRANCE_HUD_CAP_FLASH_FRAMES` 的节奏翻转
     *   （现为**隔帧**翻转 = "时间很小"那一档的频率，2026-09-16d 用户口径）。
     *   ⇒ 低槽的"闪红"与封顶的"闪黄"是两条独立规则（红优先：它表示"快没了"）。*/
    capped = (cs > TRANCE_HUD_TIME_CAP_CS);
    if (capped) cs = TRANCE_HUD_TIME_CAP_CS;
    sec = cs / 100;
    frac = cs % 100;
    snprintf(buf, sizeof buf, "%d.%02ds", (int)sec, (int)frac);
    if (s->core.phase != TRANCE_PHASE_IDLE)
        ce_hud_vm_text_set_font(&s_txt_time, TRANCE_HUD_X + TRANCE_HUD_X_OFFSET, TRANCE_HUD_TIME_Y,
                                trance_time_flash(s->core.gauge.gauge, s->frame) ? TRANCE_HUD_COLOR_FLASH
                                : (capped && (((s->frame / TRANCE_HUD_CAP_FLASH_FRAMES) & 1) != 0))
                                      ? TRANCE_HUD_COLOR_CAPPED
                                      : TRANCE_HUD_COLOR,
                                /* ★ 2026-09-15 用户：「位置不在游玩区正中间」—— `x = 224` 一手值本来就是
                                 *   游玩区（32..416）的水平中心，我原来配**左对齐** ⇒ 整行往右偏。改居中。*/
                                CE_ASCII_ALIGN_CENTER, 0.0f, buf, "trance", CE_HUD_FONT_TH13_BIG);

    else
        ce_hud_vm_text_clear(&s_txt_time);

    s->last_gauge = s->core.gauge.gauge;
    return 0;
}

/* ============================ 生命周期 ============================ */

static void trance_clear(ce_card_t *c)
{
    trance_boss_reset();          /* Boss 掉灵表 + 待撒队列（文件级 static ⇒ 删卡/局末必须清）*/
    /* ★ 2026-09-13 第二轮：**boss 相位批次的待喂队列**也在文件级 static ⇒ 同一条理由必须清
     *   （它只存标量，没有 VM；清空就是"这批不喂了"，与删卡/局末语义一致）。*/
    s_phase_q_head = 0;
    s_phase_q_count = 0;
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
    ce_no_hit_declare(TRANCE_NUM, 0);   /* ★ 声明是**粘性**的：删卡不撤 = 闸门永久开着 */
    /* ★ 2026-09-16g（第 ② 条）：**计分声明同样是粘性的** ⇒ 删卡 / 局末 / 换关必须显式撤
     *   （`METHOD.md` 铁律二：长持续效果删卡要收干净）。累计值本身**不在这里清** —— 一手的 PIV
     *   是一局里一直涨的（跨关保留），只有 `on_run_reset`（一局开始）才归零。*/
    ce_score_declare_delta(TRANCE_NUM, 0);
    /* ★ `METHOD.md` 铁律二（长持续效果必须交代"被删掉会怎样"）—— 本卡的答案是**没有东西要收**：
     *   主炮组用的是**自己的永久槽**（`CE_SHT_SET_TRANCE` = 0x28，偏移项由生成器常驻写好），
     *   卡侧不写 sht 内存、不改原生格 ⇒ 删卡后 `on_shot_set_index` 不再被问到，
     *   主炮自然回到原生组；那一格本来就该指着灵界行（没装本卡也不会有别的读者）。
     *   （旧版借用原生 0x0e + 临时改偏移项时，这里必须跑还原 —— 那段已随 §1w 第 5 步删掉。）*/
    /* ★ 灵界形态也是"跨帧留在场上的东西"（我们改的是**引擎的**本体 VM）⇒ 删卡时还回去
     *   （`METHOD.md` 铁律二）。删卡发生在商店里、那一刻 tick 全停 —— 只有这里跑得到。*/
    trance_form_restore(s);
    ce_hud_vm_text_clear(&s_txt_gauge);
    ce_hud_vm_text_clear(&s_txt_time);
    /* ★ **灵界版 BGM 的收尾**：`immediate = 1` —— 删卡发生在**商店**里、tick 全停，
     *   渐变永远走不完 ⇒ 必须当场停流 + 关事件 + 还原主 BGM 音量（`trance_bgm.c` 的
     *   `finish_release()` 一处收掉三样东西）。放在 `if (!s) return;` **之前**：
     *   就算本卡状态拿不到，第二路流也还在响。*/
    ce_trance_bgm_release(1);
    /* ★ 灵界背景也是**文件级 static 的 VM 句柄** ⇒ 在这里**无条件**收掉（删卡发生在商店里、
     *   那一刻 tick 全停 ⇒ 只靠 tick 的清理等于没有清理，`METHOD.md` 铁律二）。*/
    if (s_bg_vm) { ce_anm_delete((uint32_t)s_bg_vm); s_bg_vm = 0; }
    s_bg_angle = 0.0f;              /* 自转记账也跟着归零（下一次进灵界从头转）*/
#if TRANCE_MASK_ENABLE
    if (s_mask_vm) { ce_anm_delete((uint32_t)s_mask_vm); s_mask_vm = 0; s_mask_alpha = 0; }
#else
    /* 遮罩关着（第六轮：一手没有"矩形暗罩"这件东西，见上面的段注释）——
     * 变量与脚本都留着，一键把 `TRANCE_MASK_ENABLE` 改回 1 就能回滚；这里只做"确实没在用"的声明。*/
    (void)s_mask_vm; (void)s_mask_alpha;
#endif
    {   /* ★ 进/出幕布（4+4 台；★ 第六轮加、第七轮补进场）—— 它们**自己会 delete**，
         *   但删卡那一刻卡在幕布中途（商店里 tick 全停）⇒ 必须在这里收掉（`METHOD.md` 铁律二）。*/
        int k;
        for (k = 0; k < TRANCE_WIPE_N; ++k) {
            if (s_wipe_vm[k]) ce_anm_delete((uint32_t)s_wipe_vm[k]);
            s_wipe_vm[k] = 0;
        }
        if (s_wipe_cover_vm) { ce_anm_delete((uint32_t)s_wipe_cover_vm); s_wipe_cover_vm = 0; }
        /* ★ 第八轮新增的**目的地 alpha 清零幕**（脚本 271）同样不会自己消失 ⇒ 在这里收
         *   （`METHOD.md` 铁律二：删卡发生在商店里、tick 全停，只有 on_delete 跑得到）。*/
        if (s_alpha_zero_vm) { ce_anm_delete((uint32_t)s_alpha_zero_vm); s_alpha_zero_vm = 0; }
        s_wipe_phase_prev = 0;          /* 新一关重新抓上升沿/下落沿（不然第一帧就会误判成"刚退出"）*/
        s_wipe_bursts = 0;
        s_wipe_bursts_out = 0;
    }
    if (s_tierup_vm) { ce_anm_delete((uint32_t)s_tierup_vm); s_tierup_vm = 0; }   /* 在飞的满格提示 */
    trance_popup_clear_all();           /* ★ 在飞的数字弹窗（收集反馈 ③；同样不会自己等到删卡）*/
    {   /* ★ 左下角指示槽的 4 支（它们自己不会消失，必须在这里删）*/
        int k;
        for (k = 0; k < TRANCE_HUD_N; ++k) {
            if (s_hud_vm[k]) ce_anm_delete((uint32_t)s_hud_vm[k]);
            s_hud_vm[k] = 0;
        }
        s_hud_frame_dark = 0;
        s_hud_hidden = 0;
        s_hud_dim = 0;              /* ★ 2026-09-15g：压暗闩锁也复位（共享件那一半）*/
        s_hud_up_logged = 0;
        s_hud_gave_up = 0;          /* 新一关重新试（上限是"本关"的）*/
        s_hud_tries = 0;
        s_hud_respawn = 0;
    }
    trance_spirit_clear_all();          /* ★ 神灵实体：删光池里所有 VM + 清计数 */
    if (!s) return;
    /* ★ 私有状态清零 = **撤销这一次排队中的救援**（`pending_frame = -1`）：
     *   删卡之后不该再替玩家取消死亡 —— 那一刻 AbilityManager 冻住、本卡的 tick 全停，
     *   排队永远不会被消费 ⇒ 留着它只会让 `phase` 卡在 3。
     *   ⚠ 用 `trance_state_init()` 而不是 `memset`：`pending_frame` 的「空闲」值就是 **−1**
     *     （0 会被读成「第 0 帧报到过一次死亡」）。*/
    trance_state_init(&s->core);
}

static int ctor(ce_card_t *c)
{
    trance_card_t *s = st(c);
    if (!s) return 0;
    memset(s, 0, sizeof *s);
    trance_state_init(&s->core);          /* ★ `pending_frame = -1`（空闲）+ 槽 0，别用裸 memset */
    s->core.cooldown = TRANCE_COOLDOWN_FRAMES;  /* 开局不必等 30 帧冷却（关卡开始槽是 0，进不去）*/
    s->last_gauge = -1;
    /* ⚠ ctor **不清神灵池**：ctor 会在「每关开始引擎对卡组里每张卡再调一次」时重进
     *   （`sdk.h:357` 的一手记录），而池的生命周期是**关卡**（由 `on_stage_start` / `on_run_reset` /
     *   `on_delete` 收）—— 在 ctor 里清会在换关那一瞬间把上一关残影删掉，也会误删刚撒的实体。
     *   ⇒ 这里只清**卡的私有状态**（上面那行 memset），池交给 trance_clear()。*/
    ce_log("trance: ctor (id %d) — trance gauge 0/%d, **主动卡**（D 切到本卡 + 按 C 发动灵界）"
           "+ **中弹救援（救援层最后一家：取消死亡 → 进灵界 → 出灵界再死）**, "
           "spirit pool %d slots (static)",
           TRANCE_NUM, TRANCE_GAUGE_MAX, TRANCE_SPIRIT_POOL);
    return 0;
}

static int on_load(ce_card_t *c)
{
    trance_card_t *s = st(c);
    if (!s) return 0;
    /* ⚠ 一次性把「已接线 / 未接线」说清楚（免得实跑看见日志以为坏了）。
     * selfcheck 非 0 = 常量与语义脱钩（改了常量忘了改口径）⇒ 必须在日志里喊出来。*/
    ce_log("trance: load — selfcheck %d (0 = ok); WIRED: enemy-death hook -> kill queue -> "
           "spirit ENTITY pool (%d slots, ORIGINAL TH13 scripts type*6+1/+4 = %d..%d layer %d) "
           "-> walk within %d px to collect "
           "(type %d; per-kill count = TH13 channel B decay n=(T-alive)/(T/N)+1, "
           "+1 on Hard/Lunatic, cap %d, fallback (T,N) by HP band: hp<%d -> (60,1), hp>=%d -> (120,4)) -> trance_collect -> gauge; "
           "MID-HIT RESCUE (2nd revision, landing fixed): at card slot +0x0c "
           "(on_death_after_deathbomb, after the 8-frame deathbomb window and after the B path) -> "
           "no other claimer -> trance_force_on_death + cancel_impending_death 0x%x + **return 1**; "
           "window end -> Player__die 0x%x (delay, not immunity; chaining allowed); "
           "CHARACTER TRAIT: #%d %s (drain_div %d => trance window %d frames, spirit life %d f, "
           "reimu-slow %d, sanae-type1-bonus %d); "
           "2026-09-14 RECONCILIATION vs TH13 1st-hand (AUDIT CM): (1) chain WINDOW %d f / threshold %d "
           "now runs at SPAWN time (TH13 0x4391a0 is in spawn_spirit) -> white spirits only in dense "
           "drop bursts, NOT per collected spirit; (2) boss PHASE batch = fixed per-stage table "
           "(stage-boss %d/%d/%d/%d/%d/%d/%d, midboss %d/%d/%d/%d/%d/%d/%d) gated by 'a spell card was "
           "active in the last %d f' (zSpellcard 0x%x +0x78 bit0) -> NON-spell phase ends drop nothing; "
           "(3) spirit life %d f + script tail %d f = %d f, scale held %d f then ->%d permille over %d f "
           "(TH13 astral.anm script1 '+200:', was 200 f too early); "
           "DEBUG BURST does NOT advance the chain; "
           "TRANCE BGM (route A): second CStreamingSound via 0x4898f0 + crossfade %d f "
           "(available=%d, phase-idle=%d); "
#if TRANCE_DEBUG_KEY
           "DEBUG KEY: SPACE = burst %d spirits (blue(type %d) x%d / white(type %d) x%d / "
           "green(type %d) x%d / red(type %d) x%d) around the player, SHIFT+SPACE = gauge +%d; "
#endif

           "PENDING: score CARRIER (SPEC 9 #5 — **declaration STOPPED**, see trance_declare_score), "
           "HUD slot (#6), channel E (player-death spawns, TH13 0x4434d8), astral art (SPEC 5)",
           trance_core_selfcheck(), TRANCE_SPIRIT_POOL,
           CE_ANM_ABILITY_SCRIPT_SPIRIT_PLAIN, CE_ANM_ABILITY_SCRIPT_SPIRIT_TRANCE3,
           TRANCE_SPIRIT_LAYER, TRANCE_SPIRIT_PICK_RADIUS,
           TRANCE_KILL_SPIRIT_TYPE, TRANCE_KILL_SPIRIT_MAX,
           TRANCE_KILL_FB_HP_BIG, TRANCE_KILL_FB_HP_BIG,
           (unsigned)TRANCE_FN_CANCEL_IMPENDING_DEATH, (unsigned)TRANCE_FN_PLAYER_DIE,
           (int)trance_character(), trance_traits(trance_character())->name,
           (int)trance_traits(trance_character())->drain_div,
           (int)(TRANCE_DURATION_FRAMES_MAX * trance_traits(trance_character())->drain_div),
           (int)(TRANCE_SPIRIT_LIFE_FRAMES * trance_traits(trance_character())->spirit_life_permille / 1000),
           (int)trance_traits(trance_character())->reimu_slow_attract,
           TRANCE_CHAIN_WINDOW, TRANCE_CHAIN_THRESHOLD,
           /* 逐关档位**直接从 core 查表打出来**（不是抄一份常量 ⇒ 表改了这行自动跟着变）*/
           (int)trance_boss_phase_spirit_count(1, 0, 1), (int)trance_boss_phase_spirit_count(2, 0, 1),
           (int)trance_boss_phase_spirit_count(3, 0, 1), (int)trance_boss_phase_spirit_count(4, 0, 1),
           (int)trance_boss_phase_spirit_count(5, 0, 1), (int)trance_boss_phase_spirit_count(6, 0, 1),
           (int)trance_boss_phase_spirit_count(7, 0, 1),
           (int)trance_boss_phase_spirit_count(1, 1, 1), (int)trance_boss_phase_spirit_count(2, 1, 1),
           (int)trance_boss_phase_spirit_count(3, 1, 1), (int)trance_boss_phase_spirit_count(4, 1, 1),
           (int)trance_boss_phase_spirit_count(5, 1, 1), (int)trance_boss_phase_spirit_count(6, 1, 1),
           (int)trance_boss_phase_spirit_count(7, 1, 1),
           TRANCE_SPELL_SEEN_FRAMES, (unsigned)TRANCE_SPELLCARD_PTR,
           TRANCE_SPIRIT_LIFE_FRAMES, TRANCE_SPIRIT_SCRIPT_TAIL_FRAMES,
           (int)(TRANCE_SPIRIT_LIFE_FRAMES + TRANCE_SPIRIT_SCRIPT_TAIL_FRAMES),
           TRANCE_SPIRIT_SCALE_DELAY, TRANCE_SPIRIT_SCALE_END_PERMILLE, TRANCE_SPIRIT_SCALE_FRAMES,
           (int)trance_traits(trance_character())->sanae_type1_bonus,
           (int)TRANCE_BGM_XFADE_FRAMES, ce_trance_bgm_available(), !ce_trance_bgm_active()
#if TRANCE_DEBUG_KEY
           , TRANCE_DEBUG_BLUE_N + TRANCE_DEBUG_WHITE_N + TRANCE_DEBUG_GREEN_N + TRANCE_DEBUG_RED_N
           , TRANCE_DEBUG_BLUE_TYPE, TRANCE_DEBUG_BLUE_N
           , TRANCE_DEBUG_WHITE_TYPE, TRANCE_DEBUG_WHITE_N
           , TRANCE_DEBUG_GREEN_TYPE, TRANCE_DEBUG_GREEN_N
           , TRANCE_DEBUG_RED_TYPE, TRANCE_DEBUG_RED_N
           , TRANCE_DEBUG_GAUGE_STEP
#endif
           );
    return 0;
}

static void on_stage_start(ce_card_t *c)
{
    trance_card_t *s = st(c);
    if (!s) return;
    /* ★★ 进关的槽初值 = **一格（200）** —— ✅ **复刻**，一手证据在 `trance_core.h` 的
     *   `TRANCE_GAUGE_INITIAL` 注释里（`FUN_0042d3c0`：`esi = 0x4be7b8`，`movl $0xc8, 0x50(%esi)` ⇒ `[0x4be808] = 200`，调用点 `th13:0x42bb6b`）。
     *   相位一并回空闲（上一关末帧不该带进来）。
     * ⚠ `trance_clear()` 里已经把神灵池清空（并删光它们的 VM）⇒ 上一关的残影不会带进新关。*/
    /* ★★ 2026-09-16n（用户口径：「th13 灵界槽和最大得点一手是**跨面不归零/充值**」）：
     *   跨关（一面→二面）时**槽原样带过去**（不清零、也不补到 200）；只有**新一局**才写一格 200。
     *   ⚠ 为什么要这么写：`trance_clear()` 结尾的 `trance_state_init()` 会把 core（**含槽**）清零
     *     ⇒ 跨关必须**先存后恢**。旧版直接写 200 是把玩家攒的槽压回一格（与一手不符）。*/
    {
        const int32_t st_now = (int32_t)CE_CURRENT_STAGE();
        const int32_t keep  = (s->last_stage > 0 && st_now >= s->last_stage)
                            ? (int32_t)s->core.gauge.gauge : -1;
        trance_clear(c);
        if (keep >= 0) {
            trance_gauge_set(&s->core.gauge, keep);          /* 跨面保留（一手）*/
        } else {
            trance_gauge_set(&s->core.gauge, TRANCE_GAUGE_INITIAL);   /* 新一局 ⇒ 一格 */
            s->max_point_bonus = 0;                          /* 最大得点也是一局一清 */
            s->decl_last = 0;
        }
        s->last_stage = st_now;
        s->stage_init_frames = 0;                            /* 新策略下不需要自愈窗口 */
    }
    s->core.cooldown = TRANCE_COOLDOWN_FRAMES;
    s->last_gauge = -1;
    /* 关卡开始也把**池的诊断计数**归零（新关从零对账；`trance_spirit_clear_all()` 刻意不清它们，
     * 因为删卡那条路上我们想留着「删卡前撒了多少」）。*/
    s_stat_spirit_spawned = s_stat_spirit_picked = 0;
    s_stat_spirit_expired = s_stat_spirit_dropped = 0;
    s_spirit_spawn_index = 0;      /* 出生序号按关重置（一手在每关开始时也从头数）*/
    s_stat_popup = s_stat_popup_failed = 0;
    s_popup_overflow = 0;
    /* ★ 击破颗数的诊断（2026-09-13）：新关从零对账 —— 「这次掉的是原生档位还是兜底」要看得出趋势。*/
    s_stat_kill_native = s_stat_kill_fallback = s_stat_kill_clamped = 0;
    s_stat_kill_maxn = 0;
    s_kd_count = s_kd_total = 0;
    /* ★ boss 相位批次（2026-09-13 第二轮）：队列与计数按关重置（`trance_clear()` 已经清过队列）。*/
    s_stat_phase_events = s_stat_phase_spirits = s_stat_phase_full = 0;
    s_stat_phase_final = s_stat_phase_nospell = 0;
}

static void on_run_reset(ce_card_t *c)
{
    trance_card_t *s = st(c);
    (void)s;
    trance_clear(c);                       /* 含 `ce_score_declare_delta(113, 0)`（撤声明）*/
    /* ★★ 2026-09-16k（用户质疑：“收灵最大得点会在一面进二面时清空吗”）——这里**不再**清那个累计值。
     *   `on_run_reset`（虚表 `+0x4c`）的触发时机由引擎定；一旦它在**关卡切换**时也跑，
     *   加成就会被静默清掉（一手：PIV 在一局里是**一直涨、跨关也不清**的）。
     *   现在改成**卡侧自证**：只在“关卡号**变小**”（= 回到第 1 关 / 重开一局 / 练习重选关）时归零，
     *   见 `on_item_frame` 的进关块。*/
}
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
 *   帧末由 `on_item_frame` 的 `trance_kill_take()` 一次取走（`DEATH` §4.1 对 109 的 O(n²) 警告同款）。
 *
 * ★★ **2026-09-13：这里多算一件事 —— 这一次击破掉几颗**（TH13 通道 B 的衰减）。
 *   ① 读**敌人自己的**掉落表（入参表与一手见文件头「★★ 击破掉灵几颗」那一段）：
 *      `T` = `drop+0xa4`、`N` = `max(drop+0x54[0..19])`、`alive` = `zEnemyData.time_alive`；
 *   ② 没配窗口（`T <= 0 || N <= 0`，**TH18 的常态**：749 个子程序里只有 64 个配了）⇒ 用兜底 (120, 5)；
 *   ③ `trance_kill_spirit_count()`（core，纯逻辑 + 单测）算颗数；
 *   ④ `trance_kill_spirit_clamp()` 夹上界，**夹了就计数**（`s_stat_kill_clamped`，进汇总日志）；
 *   ⑤ `trance_kill_note(..., n)` **一次击破一条**（带颗数，绝不按颗数展开入队）。
 *   ⚠ 全程 **O(1)**（固定 21 次整数读 + 一次数组写）、**只读不写**引擎字段、**不缓存 `ev->enemy`**
 *     —— 指针只在本次回调内用来读那 21 个整数（`DEATH` §2.4 禁的是"缓存到下一帧"）。
 *   ⚠ 这里**不打日志**（清场帧会被调 100–300 次）：诊断只写 4 格文件级小环，由帧末统一打。*/
static void on_enemy_death(ce_card_t *c, const ce_enemy_death_t *ev)
{
    trance_card_t *s = st(c);
    const uint8_t *e;
    int32_t alive = 0, T = 0, N = 0, hp = 0, n, i, fallback = 0, clamped = 0, was_full;

    if (!s || !ev) return;
    if (ev->kind != CE_EDEATH_KILLED) return;                 /* 只要「被打死的主路」*/
    if (!(ev->flags & CE_EDEATH_F_REAL)) return;              /* 只要真敌人（极性见 public.h）*/

    /* ① 读敌人自己的 (T, N, alive, hp)。`ev->enemy` 仅本次回调有效（读 22 个 int 是 O(1)，不缓存）。*/
    e = (const uint8_t *)ev->enemy;
    if (e) {
        const uint8_t *drop = e + CE_ENEMY_DROP;
        alive = *(const int32_t *)(e + CE_ENEMY_DATA + CE_ED_TIME_ALIVE);
        hp    = *(const int32_t *)(e + CE_ENEMY_DATA + TRANCE_ENEMY_LIFE_MAX);
        T = *(const int32_t *)(drop + CE_ENEMY_DROP_TIMED_TIME);
        for (i = 0; i < CE_ENEMY_DROP_TYPES; ++i) {            /* 取各 type 里的最大值（原生每次只配一种）*/
            const int32_t v = *(const int32_t *)(drop + CE_ENEMY_DROP_TIMED_COUNTS + i * 4);
            if (v > N) N = v;
        }
    }
    /* ② 没配原生窗口（含 `ev->enemy` 为空这种异常）⇒ **按 HP 上限查兜底档**
     *    （core 的纯函数；口径与三条一手锚见 `trance_core.h` 的 `TRANCE_KILL_FB_*`）。
     *    ⚠ 旧版是写死的 `(120,5)`（瞬杀 6 颗）—— 与"配了窗口的敌人只掉 2 颗"形成相对落差，
     *    就是 owner 那次「神灵掉的相对数量偏多」（见本文件上方那段与 `TUNING.md` §2.0c）。*/
    if (T <= 0 || N <= 0) {
        trance_kill_fallback(hp, &T, &N);
        fallback = 1;
    }

    /* ③ 公式（core）→ ④ 夹上界。难度号**原值直传**：TH18 `0x4ccd00` 与 TH13 同一套 0..3（+4 EXTRA），
     *    而一手的 `+1` 只认 `== 2 || == 3`（Hard / Lunatic）⇒ EXTRA 不加（照抄）。*/
    n = trance_kill_spirit_count(alive, T, N, *(volatile int32_t *)(uintptr_t)CE_ADDR_DIFFICULTY);
    {
        const int32_t capped = trance_kill_spirit_clamp(n);
        if (capped != n) clamped = 1;
        n = capped;
    }

    /* ⑤ 诊断（4 格环 + 计数；**不打日志**，帧末统一打）与**一条队列项** */
    was_full = (s->core.kill_pending >= TRANCE_KILL_QUEUE_MAX) ? 1 : 0;
    ++s_kd_total;
    if (s_kd_count < TRANCE_KILL_DIAG_MAX) {
        trance_kill_diag_t *d = &s_kd[s_kd_count++];
        d->T = T; d->N = N; d->alive = alive; d->hp = hp; d->n = n;
        d->flag = (fallback ? 1 : 0) | (clamped ? 2 : 0) | (was_full ? 4 : 0);
    }
    if (!was_full) {                                          /* 丢掉的次数不进"档位"统计（它没真的掉）*/
        if (fallback) ++s_stat_kill_fallback; else ++s_stat_kill_native;
        if (clamped) ++s_stat_kill_clamped;
        if (n > s_stat_kill_maxn) s_stat_kill_maxn = n;
    }
    trance_kill_note(&s->core, s->frame, ev->x, ev->y, n);    /* O(1)：标量入队，指针一律不碰 */
}

/* ============ ★★ boss **相位击破** → 灵批次（事件钩子；断点 `0x430510`，与 110/111/116/117 共用） ============
 *
 * **用户口径**：「击破 boss 的**符卡**或者**非符**应当也会掉**大量**灵」。
 * 机制、一手普查表、TH18 落点与三个 ❓ 自定值全在文件头「★★ boss 相位击破的灵批次」那一段
 * （一手的完整数据在 `trance_core.h`，TH18 侧的一手审计在 `engine/ecl/th18/02-item-drops.md` §3.4/§4.2）。
 *
 * 本函数**只做三件事**：判是不是 boss 的**真掉落事件** → 算颗数（core）→ 入队（O(1)）。
 * **不建 VM**（帧末 `trance_phase_burst_tick()` 才建）、**不改 `counts[]`**（那是 110/111 与 SDK
 * 掉落乘法层的地盘 —— 我们只读）、**不碰 116 的闩锁字节**（见下面的去重说明）、**不缓存敌人指针**
 * （`counts` 反推出的敌人对象只在本次回调内用来读 flags 与位置）。
 *
 * 去重（与 116 同一套判据，但**不用**它的闩锁）：一手 `Enemy__drop_items_and_notify_cards` `0x430510`
 *   的**结尾**自己 `_memset(this+4, 0, 0x50)`（清 20 个 type 的计数）⇒ 同一个掉落事件的**第二次**
 *   回调（ECL `dropItems` → `0x42d0c0`）看到的计数表**必然全零** ⇒ 「求和 > 0」既是"真掉落"的判据、
 *   也就是去重的全部。⚠ 116 另外用 `enemy+0x6838`（`CE_ENEMY_DDC_UNUSED`）当**小怪兜底**的闩锁 ——
 *   那个字节是**跨卡共享**的，本卡**不写它**（写了会掐掉 116 的一次掉落，见 `AUDIT` §BE 的登记）。
 *   ⚠ 本卡**没有**"小怪兜底"那一支：小怪走通道 B（逐敌衰减），这里**只认 boss**。
 *
 * 「击破 vs 超时」为什么不用我们自己判：TH18 的 `@BossItem*` 模板体里就有
 *   `unless ($TIMEOUT == 0) goto end;`（与 TH13 逐字同款）⇒ **超时就不会调 `dropItems()`**，
 *   引擎也就不会消费掉落表 ⇒ 本钩子根本不会被调到。⇒ 钩子一响就是"相位被击破"。
 *   同时**也不需要**判符卡/非符：TH18 的相位（符卡与非符）各自都会调一次 `@BossItem*`。*/
static void on_enemy_drop_pre(ce_card_t *c, int32_t *counts)
{
    trance_card_t *s = st(c);
    uint8_t *e;
    const float *ep;
    int32_t items = 0, batch, i, slot;
    int32_t is_midboss = 0, spell_seen;
    float area = 0.0f;
    uint32_t flags;

    if (!s || !counts) return;
#if !TRANCE_PHASE_DROP_ENABLE
    return;                                          /* 闪退二分开关（见常量注释）*/
#endif
    /* ① 从 `counts` 反推敌人对象（与 116 同一个算式：counts = enemy + CE_ENEMY_DROP + CE_ENEMY_DROP_COUNTS）*/
    e = (uint8_t *)counts - CE_ENEMY_DROP_COUNTS - CE_ENEMY_DROP;
    flags = *(const uint32_t *)(e + CE_ENEMY_FLAGS);
    if (!(flags & CE_ENEMY_FLAG_BOSS)) return;        /* 只要 boss / 道中 boss（ECL opcode 512 setBoss）*/
    /* ② 「真掉落」判据 = 去重（见上面那段）：计数全零 ⇒ 不是相位奖励（重复回调 / 没配掉落）*/
    for (i = 0; i < CE_ENEMY_DROP_TYPES; ++i) items += counts[i];
    if (items <= 0) return;
    /* ③ 档位与"这是哪一类事件"的两个一手判据（都从**引擎自己的字段**读，不解析 ECL）：
     *   · `is_midboss`：`dropArea` 的半径（写进掉落结构 `+0xbc`，`th18:0x42d090 movss [ecx+0xbc],xmm1`）。
     *     `default.ecl` 的 `BossItem` 恒 `64.0f`（stage boss）、`BossItemPhase`（道中 boss 死亡）
     *     由调用方传 `48.0f`（本机重跑 `thecl -d 18`：31 处三参调用 / 8 处四参调用末参全 48.0f）。
     *     ⚠ 例外：`st06mbs` 的道中 boss 死亡用的是 `@BossItem(16,50,50)`（半径 64）⇒ 会被归成 stage boss，
     *       再由下面的符卡门挡掉（TH18 的 `*mbs` **一个符卡都没声明**，`thecl -d 18` 全量：`spell(` 计数 0）。
     *   · `spell_seen`：★ **符卡门**（用户口径 #3「非符不掉」）——一手的灵批次只写在**符卡刚结束**
     *     的那一相位的序言里（`spellEnd();` 之后几行），**非符相位的结束一颗不掉**。
     *     TH18 侧同形，但「引擎掉了一次道具」本身不够：`st02bs` 的 `BossCard1`
     *     （**符卡相位自己的序言** = 上一段是非符）也调了两次 `@BossItem` ⇒ 只看事件会多掉一整批。
     *     判据 = `zSpellcard` 全局（`th18:0x4cf2c0` 指向对象本体）`+0x78` 的 **bit0 = 符卡进行中**
     *     （`spellEnd` `th18:0x42a780`：`mov edi,[0x4cf2c0]; test byte [edi+0x78],1; … and dword [edi+0x78],0xfffffffe`；
     *      符卡开始 `0x42a320` 写 `+0x74`=id）。卡侧每帧采一次、留 `TRANCE_SPELL_SEEN_FRAMES` 帧容差
     *     （采样在 `on_item_frame`、事件在敌人 tick —— 谁先谁后都不影响判定）。
     *     ⚠ **道中 boss 死亡那一路不过这道门**：一手 `MBossDead` 的批次与"上一相位是不是符卡"无关
     *       （`st04mbs` 的 `MBoss1` 在 Easy 上直接 `setInterrupt(0,0,2700,"MBossDead")` ⇒ 非符相位结束
     *       照样掉 19 颗），而且 TH18 的 `*mbs` 根本没有符卡 ⇒ 拿符卡门去卡它就是一颗都不掉。*/
    area = *(const float *)(e + CE_ENEMY_DROP + TRANCE_ENEMY_DROP_AREA);
    is_midboss = (area > 0.0f && area < TRANCE_MIDBOSS_AREA_CUT) ? 1 : 0;
    spell_seen = s_spell_active_seen;
    if (!spell_seen && !is_midboss) {
        ++s_stat_phase_nospell;
        if (s_stat_phase_nospell <= 4)
            ce_log("trance: boss drop IGNORED (no spell card in the last %d f; dropArea %.0f => stage-boss "
                   "template) - items %d, life %d/%d, flags %#x, frame %d [TH13: only a defeated SPELL CARD "
                   "drops spirits; a NON-spell phase end drops nothing]",
                   TRANCE_SPELL_SEEN_FRAMES, (double)area, (int)items,
                   (int)*(const int32_t *)(e + CE_ENEMY_DATA + TRANCE_ENEMY_LIFE_CUR),
                   (int)*(const int32_t *)(e + CE_ENEMY_DATA + TRANCE_ENEMY_LIFE_MAX),
                   (unsigned)flags, (int)s->frame);
        return;
    }
    /* ★★ 2026-09-16i（`AUDIT` §DJ）：**终符不该爆灵** —— 判据见 `TRANCE_CARD_ITEM_*` 那一段的注释。*/
    if (!is_midboss) {
        int32_t card_items = 0;
        int32_t main_type = -1;
        for (i = TRANCE_CARD_ITEM_LO; i <= TRANCE_CARD_ITEM_HI && i < CE_ENEMY_DROP_TYPES; ++i) {
            if (counts[i] > 0) {
                card_items += counts[i];
                if (main_type < 0) main_type = i;
            }
        }
        if (card_items <= 0 || main_type == TRANCE_CARD_ITEM_EXTEND2) {
            ++s_stat_phase_final;
            if (s_stat_phase_final <= 4)
                ce_log("trance: boss PHASE batch SKIPPED - %s (stage %d, dropArea %.0f, life %d/%d, "
                       "card items %d, main type %d, items %d, frame %d) [TH13: the FINAL phase (BossDead) "
                       "drops NO spirits at all - all 7 stages; consult AUDIT DJ]",
                       card_items <= 0 ? "drop has no card item (0x10..0x13)"
                                       : "card item is 0x10 EXTEND2 = the final-phase type",
                       (int)CE_CURRENT_STAGE(), (double)area,
                       (int)*(const int32_t *)(e + CE_ENEMY_DATA + TRANCE_ENEMY_LIFE_CUR),
                       (int)*(const int32_t *)(e + CE_ENEMY_DATA + TRANCE_ENEMY_LIFE_MAX),
                       (int)card_items, (int)main_type, (int)items, (int)s->frame);
            return;
        }
    }
    batch = trance_boss_phase_spirit_count(CE_CURRENT_STAGE(), is_midboss, items);
    if (batch <= 0) return;
    ep = ce_enemy_pos(e);
    if (!ep) return;
    /* ③ 入队（O(1)：三次数组写 + 计数 ++）。环满（正常不会：一次相位结束一个事件）⇒ 记一次并丢这个事件。*/
    if (s_phase_q_count >= TRANCE_PHASE_QUEUE_MAX) {
        ++s_stat_phase_full;
        ce_log("trance: boss phase batch DROPPED (queue full %d) - items %d, would spawn %d",
               TRANCE_PHASE_QUEUE_MAX, (int)items, (int)batch);
        return;
    }
    slot = (s_phase_q_head + s_phase_q_count) % TRANCE_PHASE_QUEUE_MAX;
    s_phase_qx[slot] = ep[0];
    s_phase_qy[slot] = ep[1];
    s_phase_qn[slot] = batch;                          /* 还剩几颗没喂 */
    s_phase_qt[slot] = batch;                          /* 总颗数（选 type 用）*/
    ++s_phase_q_count;
    ++s_stat_phase_events;
    /* 一行日志（事件很稀：一次相位结束一条）—— 实跑对账"击破了几个相位、各掉多少"就靠它。*/
    ce_log("trance: BOSS PHASE spirit batch -> %d spirits @(%.0f,%.0f) "
           "[stage %d, %s, dropArea %.0f, spell_seen %d f, life %d/%d, flags %#x] "
           "[engine drop event: %d item(s) in counts[], boss flags %#x, frame %d, queued %d, "
           "total events %d, fed %d]",
           (int)batch, (double)ep[0], (double)ep[1], (int)CE_CURRENT_STAGE(),
           is_midboss ? "midboss tier (TH13 MBossDead)" : "stage-boss tier (TH13 BossN prologue)",
           (double)area, (int)spell_seen,
           (int)*(const int32_t *)(e + CE_ENEMY_DATA + TRANCE_ENEMY_LIFE_CUR),
           (int)*(const int32_t *)(e + CE_ENEMY_DATA + TRANCE_ENEMY_LIFE_MAX),
           (unsigned)flags,
           (int)items,
           (int)s->frame, (int)s_phase_q_count, (int)s_stat_phase_events, (int)s_stat_phase_spirits);
}

/* ★★ Boss 吃到伤害（事件钩子；落点 `0x42f2cb`，见上面那段的一手链）。
 *   ⚠ 回调纪律：**只入队标量**（x/y）、只写**卡自己的**表；不建 VM、不碰 `ev->enemy`（仅本次有效）。*/
/* ★★ 2026-09-18（owner 口径「你改我跑」）：**恢复订阅** —— 本函数重新挂回 `CE_CARD(113, …)`。
 *   历史（别删，它解释了为什么树上曾有"站点声明着但没人订阅"的怪状态）：2026-09-17a 做「站点总数
 *   越线」假说的二分时把 `.on_enemy_damage` 退订、本函数暂无人用 ⇒ 当时标了 `unused` 保 `-Werror`。
 *   2026-09-18 的结论是：**取消订阅并不改变站点声明集与跳板池布局**（声明一直在、桩一直在、handler
 *   一直在每敌每帧跑），所以「订不订阅」与那条崩溃假说无关 —— 见 `AUDIT.md` §BE-EI 与 §BE 详录 #41。*/
static void on_enemy_damage(ce_card_t *c, const ce_enemy_damage_t *ev)
{
    trance_card_t *s = st(c);
    trance_boss_slot_t *b;
    const float *pp;
    int32_t inc;

    if (!s || !ev) return;
#if !TRANCE_BOSS_DROP_ENABLE
    return;                                            /* 开关（见宏注释；闪退二分用）*/
#endif
    if (!(ev->flags & CE_EDAMAGE_F_BOSS)) return;      /* 🟡 有意窄化：只认 boss / 道中 boss */
    if (ev->damage <= 0) return;                       /* 免伤闸之后才到这儿；<=0 只可能是异常 */
    /* ★★ 2026-09-13 第五轮实跑后的**合理性闸**：第一次实跑里这一路读到的是 `enemy id 0 /
     *   hp 0/217638672 / pos (0,0)` —— 说明**这一颗的数据不是一颗正常敌人**（要么那个对象不是敌人、
     *   要么字段偏移不对）。读错字段本身不至于崩，但我们会拿它去建 VM、还会按错值算距离 ⇒
     *   这里先按"看起来像不像一颗活着的敌人"筛一遍：不像就**只记一次日志**（把原值打出来当证据）、
     *   不计入、不撒灵。`poison` 那一格只用于诊断（**不**做粘性禁用，免得一次误判把功能关掉）。*/
    {
        const int ok_id    = (ev->id != 0 || s_stat_boss_hits > 0);   /* id 0 合法但罕见 ⇒ 只当提示 */
        /* ★ 实跑证据：被闸掉的那 4 笔与放过的那 1 笔全是 `hp 0/0`、`pos (0,0)` ——
         *   那是**当帧已死**（life 已 0、坐标已被清）的敌人 ⇒ `>= 0` 太松，必须 `> 0`。*/
        const int ok_life  = (ev->hp_cur > 0 && ev->hp_cur <= 1000000);
        const int ok_dmg   = (ev->damage > 0 && ev->damage <= 10000000);
        const int ok_pos   = (ev->x > -1000.0f && ev->x < 1000.0f &&
                              ev->y > -2000.0f && ev->y < 2000.0f);
        if (!ok_life || !ok_dmg || !ok_pos || !ok_id) {
            if (s_stat_boss_bad < 4) {
                ++s_stat_boss_bad;
                ce_log("trance: boss-drop event REJECTED by sanity gate (#%d): id %u dmg %d "
                       "hp %d/%d flags 0x%x pos (%.1f,%.1f) - 见 AUDIT §CI-7（这一路的数据可疑）",
                       (int)s_stat_boss_bad, (unsigned)ev->id, (int)ev->damage,
                       (int)ev->hp_cur, (int)ev->hp_max, (unsigned)ev->flags,
                       (double)ev->x, (double)ev->y);
            }
            return;
        }
    }
    b = trance_boss_slot(ev->id, s->frame);
    if (!b) return;
    b->accum += ev->damage;
    ++s_stat_boss_hits;
    if (b->accum < b->threshold) return;
    if (s->frame - b->last_drop < TRANCE_BOSS_DROP_COOLDOWN) return;   /* ❓ 自定冷却 */
    b->last_drop = s->frame;
    pp = ce_player_pos();
    inc = trance_boss_bucket(ev->x - (pp ? pp[0] : 0.0f), ev->y - (pp ? pp[1] : 0.0f));
    b->threshold = b->accum + inc;                     /* ★ 一手是"阈值再加增量"（不是重置）*/
    if (s_boss_queue < TRANCE_BOSS_SLOTS) {
        s_boss_qx[s_boss_queue] = ev->x;
        s_boss_qy[s_boss_queue] = ev->y;
        ++s_boss_queue;
    }
    if (s_stat_boss_drop < 8 || (s_stat_boss_drop % 20) == 0) {
        ce_log("trance: boss drop #%d (enemy id %u, dmg %d, hp %d/%d, accum %d, next threshold %d, "
               "+%d by distance) at (%d,%d)",
               (int)s_stat_boss_drop, (unsigned)ev->id, (int)ev->damage, (int)ev->hp_cur, (int)ev->hp_max,
               (int)b->accum, (int)b->threshold, (int)inc, (int)ev->x, (int)ev->y);
    }
    ++s_stat_boss_drop;                                 /* （日志用上一行的序号，这里再 ++）*/
}

/* ★ 计分层：本卡**刻意不注册** `.on_item_score`（载体 `SPEC §9 #5` 未拍 —— 注册了也只是把
 *   声明值再写一遍、没有载体语义），并且 **2026-09-12 起连 `ce_score_declare(113, …)` 的写入也停了**
 *   （那 10000 会被 110/111 的落笔者当信仰算进 PV 聚合 ⇒ 污染）。完整理由与「拍板后怎么接」
 *   见文件头「计分声明：已停用」段 + `trance_declare_score()`。
 *   本卡的其它功能（灵界槽 / 主动卡发动 / **中弹救援（第二版）** / `+0x14` 收尾 / 无敌声明 /
 *   HUD / 击破记账）**一律照常**，与计分层无关。
 *
 * ★ 注册的三个死亡相关槽（**2026-09-12 第二版**）：
 *   · `+0x0c on_death_after_deathbomb` = ★ **中弹救援就在这里**（决死窗口与 B 键那条路都走完之后
 *     才广播 ⇒ 天然排在 107/117/决死 B 之后）；**必须 `return 1`** 才会阻止引擎扣命；
 *   · `+0x14 on_death_frame2` = **只记一行日志**（补死落地的回执；旧版的「在这里强制开灵界」已删）；
 *   · **不挂 `+0x10`**（那是 107/117 的位置 —— 在 `Player__die` 里就出手，比 113 早两档）。*/
CE_CARD(113,
        .active_recharge = 1,                   /* ★ 主动卡（代价 = 灵界槽，见 on_activate）*/
        .ctor = ctor,
        .on_activate = on_activate,
        .on_load = on_load,
        .on_item_frame = on_item_frame,
        .on_item_collect = trance_on_item_collect,   /* ★ 2026-09-18：点道具 ⇒ 共享 PIV 池 +100 显示 */
        .on_death_after_deathbomb = on_death_after_deathbomb,   /* ★★ 中弹救援（救援层最后一家）*/
        .on_death_frame2 = on_death_frame2,     /* ★ 死亡结算之后 +0x14：只记日志 */
        .on_enemy_death = on_enemy_death,       /* ★ 击破 → 神灵（事件钩子，非虚表槽）*/
        /* ★★ 2026-09-18 恢复（owner 口径「你改我跑」）：boss 吃到伤害 → 掉灵。
         *   ⚗ 2026-09-17a 曾为「站点总数越线」假说把本行注释掉 ⇒ 那期间日志恒为
         *     `boss hits 0(drops 0, rejected 0)`（站点在跑、卡侧没人订阅）。恢复**不需要**动
         *     `patch`/`.def`/`Makefile`：那三处是"声明"层面的，声明现在一直在（24 条之一），
         *     所以当时写的"三处回滚"已不适用；只改本行 + handler 的 `unused` 属性即可。
         *   见 `AUDIT.md` §BE-EI。*/
        .on_enemy_damage = on_enemy_damage,
        /* ★★ boss **相位击破** → 灵批次（2026-09-13 第二轮，用户口径「击破符卡/非符也掉大量灵」）。
         *   断点 `0x430510`，**与 110/111/116/117 共用**（本卡只读 counts、不改、不用共享闩锁）
         *   ⇒ 协作须知登记在 `AUDIT` §BE。*/
        .on_enemy_drop_pre = on_enemy_drop_pre,
        .on_tick_2 = on_tick_2,                 /* ★ 魔理沙的灵界移速（必须在 Player tick 之前乘）*/
        /* ★★ 灵界主炮组（TH13 灵界 set10 复刻，2026-09-13）：见上面「灵界主炮组」段。
         *   · `on_shot_set_index`（断点 `th18:0x45ebfb`）= 与 117 咆哮**同一个断点**：
         *     灵界窗口内返回借用格 `CE_SHT_SET_TRANCE`，否则 −1（原版主炮，一个字节不动）；
         *   · `shot_set_index_priority = CE_SHT_PRIO_113`（= 10）：同帧多张卡都要覆盖时按数字最大者赢；
         *     ★ 2026-09-16z **用户人为规定「117 > 113」** ⇒ 咆哮（`CE_SHT_PRIO_117` = 20）赢，灵界让位；
         *   · `on_option_shot_gate`（断点 `th18:0x40a9c3`）= 灵界期间停掉装备卡子机弹
         *     （TH13 灵界组一条子机 row 都没有）。*/
        .on_shot_set_index = on_shot_set_index,
        .shot_set_index_priority = CE_SHT_PRIO_113,
        .on_option_shot_gate = on_option_shot_gate,
        .bullet_damage_permille = CE_DMG_UNIT,  /* ★ 进伤害层的基线（×1，中性；运行时按特技改）*/
        .on_stage_start = on_stage_start,
        .on_draw = on_draw,
        .on_run_reset = on_run_reset,
        .on_delete = on_delete);
