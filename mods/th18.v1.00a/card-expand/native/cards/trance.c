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
 *      且 `flags & CE_EDEATH_F_REAL` ⇒ `trance_kill_note()` **只做 O(1) 入队**（`DEATH` §2.4）。
 *   ② **撒实体**（帧末 `on_item_frame` ⑦）：`trance_kill_take()` 取走 → 每颗按
 *      `TRANCE_KILL_SPIRIT_TYPE`（= **1**，SPEC §2.2：击破走通道 B）与
 *      `TRANCE_KILL_SPIRITS_PER_KILL`（❓ 自定 = 1）调 `trance_spirit_spawn()` **建一颗神灵 VM**
 *      （自建池，见下「神灵实体」段）——★ **不再**在帧末按「已收」入账。
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
 *     击破一颗只算 1 颗（❓ 自定，见 §9 #4）⇒ 本卡一关的强弱直接由击破数决定，实跑首日先看汇总日志。
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
 *   · **不下落**（裁决 1）：`x/y` 是**出生点**，整个生命周期**只被原地微动**改变
 *     （`trance_spirit_bob_offset()`，±1 px 方波，❓ 自定）——**没有速度、没有重力、不出界删除**。
 *   · **靠近才吃**（裁决 2）：`trance_spirit_in_pickup_range()`（core，整数平方比较，
 *     半径 `TRANCE_SPIRIT_PICK_RADIUS` = ❓ 自定 24 px；不是引擎道具的 30 px 收点半径）。
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
 *   · **通道 E（玩家死亡每帧掉 type 3）/ F（连锁真撒一颗 type 3）**：SPEC §3.3 ② / §4 都是 ⏳ 待拍
 *     ⇒ 只记账 / 只打日志（`trance_chain_note` 的计数与 `chain_extra` 日志照跑），不撒。
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
 *     神灵实体本身用**借来的现成脚本 + 贴图 + 染色**顶着（见下「载体脚本」段的 ❓ 自定登记）。
 */
#include <stddef.h>
#include <string.h>
#include <stdio.h>

#include "sdk.h"
#include "trance_core.h"

#define TRANCE_NUM  113          /* ⚠ `CE_CARD` 会对 ID 做 ## 拼接 ⇒ 里面必须写字面量 113 */
#define TRANCE_LOG_EVERY 600     /* 每 600 游玩帧一条汇总 */

/* ---- ★ 中弹救援用到的两个引擎函数（第二版新增；**地址不是猜的**，逐条给一手证据）----
 *
 * `Player__cancel_impending_death` `th18:0x45d590` —— thiscall(**zPlayer***)，**无栈参**，返回值 **void**。
 *   一手（本机 Capstone 反汇编 `local/th18.v1.00a/th18.exe`，70 字节整函数）：
 *     `mov eax,[ecx+0x644] / test al,1 / … or eax,1 / mov [ecx+0x640],0 / mov [ecx+0x644],eax`
 *     `mov [ecx+0x638],0x3c / mov [ecx+0x63c],0x42700000(60.0f) / mov [ecx+0x634],0x3b`
 *     `mov [ecx+0x476ac],1 / ret`
 *   ⇒ 它**只写 6 个字段、读零个计时器**：`state(+0x476ac) → 1`（活）、无敌计时器 = 60 帧。
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

/* ---- 音效：TH13 的 id 在 TH18 无意义 ⇒ **全部 ❓ 自定，实跑首日校准**（SPEC §5 末行）----
 * 只挑 TH18 现有表里语义最近的（SDK.md §7 的原生 se 表）。 */
#define TRANCE_SE_PICK      0x3f   /* ❓ 自定（TH13 收神灵也是 0x3f，但两作表不同源）—— 收集音 */
#define TRANCE_SE_LEVELUP   0x44   /* ❓ 自定（TH13 跨格 0x44）—— 跨格音 */
#define TRANCE_SE_ENTER     0x34   /* ❓ 自定（TH13 灵界进入 0x34）—— 进入灵界 */
#define TRANCE_SE_END       0x46   /* ❓ 自定（TH13 灵界结束 0x46）—— 结束灵界 */
#define TRANCE_SE_FORCED    0x2c   /* ❓ 自定（se_bomb 系）—— 死亡强制的灵界（TH13 无对应音）*/

/* ==================== ★ 神灵实体（自建 VM 池）：载体脚本 / 贴图 / 层 / 染色 ====================
 *
 * TH18 **没有「神灵」这个美术**（原生 `astral.anm` 是 TH13 的资产，SPEC §5 台账里是 ⏳ 未做）
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
 * ### 贴图：原生 P 点（`CE_ANM_ABILITY_SPRITE_SOUL_0`）
 * 裁决把神灵定义成「≈ **P 点**、蓝点等下落道具」⇒ 用**原生切图**里的小道具样贴图（16×16），
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
#define TRANCE_SPIRIT_SPRITE      CE_ANM_ABILITY_SPRITE_SOUL_0                  /* ❓ 自定：原生魂贴图 */
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
    int32_t stat_rescued;     /* 诊断：中弹救援成功几次（帧点里取消死亡 + 开死亡强制窗）*/
    int32_t stat_redie;       /* 诊断：灵界窗口结束「补死」几次（调 Player__die）*/
    int32_t stat_rescue_lost; /* 诊断：在 `+0x0c` 里**没救成**几次（别人救了 / 槽不够）*/
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
    ce_play_sound(TRANCE_SE_ENTER, 0.0f);   /* ❓ 声像：进入不是 `+0x14` ⇒ 本可读自机 x，
                                             * 但主动卡发动发生在 Player tick 里，保守起见仍传 0 */
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

typedef void (*trance_fn_player_t)(void *player);

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
    ((trance_fn_player_t)(uintptr_t)TRANCE_FN_PLAYER_DIE)(p);
}

/* ★★ **`+0x0c` `on_death_after_deathbomb` —— 113 的中弹救援就是它**（第二版·落点订正版）。
 * 返回值：**1 = 救下了**（引擎据此跳过扣命），0 = 不救、照常死。
 * `acc` = 引擎传进来的累加器（前面各卡返回值 OR 起来，初值 0；一手 `0x45c114` 的 `push edi`）。
 * 本卡不看它，但**必须接收**（stdcall 由被调方清栈 —— 少一个形参就是栈错位）。*/
static int on_death_after_deathbomb(ce_card_t *c, uint32_t acc)
{
    trance_card_t *s = st(c);
    int owner;

    (void)acc;
    if (!s) return 0;

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
    ce_play_sound(TRANCE_SE_FORCED, 0.0f);   /* ❓ 声像 0：死亡栈上不读自机坐标 */
    ce_log("trance: +0x0c RESCUED (last in the rescue layer) - death CANCELLED + return 1, "
           "death-free window opened (phase %d, gauge %d, frame %d); will re-die when it ends",
           (int)s->core.phase, (int)s->core.gauge.gauge, (int)s->frame);
    return 1;                        /* ★ 实质动作：引擎在 `0x45c12d` 用累加器决定扣不扣命 */
}

/* `on_item_frame` = 断点 `0x446ee3`（ItemManager tick 的门卫位次，`sites.py:711` 一手 disasm）：
 * 正常游玩帧一次；菜单 / 商店 / 暂停 / 对话**冻结帧连 CMP 都到不了**（**这里不触发**）
 * ⇒ 它就是本卡要的帧源（与 `cards/collect2x.c` 的 `s_play_frame` 同口径，但**存在私有状态里**）。
 * ⚠ 中弹救援**不在**这里（那是卡虚表 `+0x0c` 的事，见上）；本帧点只做：
 * 推窗口 / **补死** / 无敌声明 / 撒神灵 / 收神灵 / HUD / 日志。*/
static void on_item_frame(ce_card_t *c)
{
    trance_card_t *s = st(c);
    trance_tick_t t;
    float kx[TRANCE_KILL_QUEUE_MAX];
    float ky[TRANCE_KILL_QUEUE_MAX];
    int i;

    if (!s) return;
    ++s->frame;

    /* ① 窗口推进（空闲：推冷却 + 连锁窗口；在灵界里：槽 −1/帧、归零收尾并报 `re_die`）*/
    t = trance_tick(&s->core, s->frame);

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
                trance_player_die();
                ++s->stat_redie;
                ce_log("trance: re-die (death-free window ended, frame %d) — Player__die called; "
                       "chaining is allowed (107/117/113 may rescue this death again)", (int)s->frame);
            } else {
                ce_log("trance: re-die SKIPPED (player state %d, not 1) — death is already in flight",
                       (int)pstate);
            }
        }
    }

    /* ④ 「中弹无伤」（SPEC §3.2 ⏳）：灵界进行中**只声明无敌帧**（层取 max）。
     * 每帧重新声明 ⇒ 窗口一结束声明自动掉回 0；`on_delete` 里也显式清一次。
     * ⚠ 本卡**没有**「排队相位」那种中间态（救援在 `+0x0c` 里一次做完）⇒ 只有真在灵界里才声明。*/
    ce_invuln_request(TRANCE_NUM,
                      (s->core.phase == TRANCE_PHASE_MANUAL
                       || s->core.phase == TRANCE_PHASE_DEATH_FREE) ? TRANCE_INVULN_DECLARE : 0);

    /* ⑤ 结束反馈 */
    if (t.ended) {
        ce_play_sound(TRANCE_SE_END, 0.0f);
        ce_log("trance: end (%s), gauge %d, frame %d%s", t.ended_death_free ? "death-free" : "manual",
               (int)s->core.gauge.gauge, (int)s->frame,
               t.re_die ? " — re-die owed (延时不是免死)" : "");
    }

    /* ⑥ ★ **击破 → 神灵实体 → （靠近才）入槽**（2026-09-12 第二轮：实体层就是这一步接上的）。
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

    /* ⑦ ★ **神灵池每帧推进**（缩小 / 微动 / 靠近才吃 / 到点消失）—— 裁决四条都在这一个函数里。
     * ⚠ 顺序：**先撒后推**（本帧刚撒的也能吃到；`age` 从 1 开始，出生那帧的 scale 已在 spawn 里写过）。*/
    trance_spirits_tick(c);

    /* ⑧ 计分声明（模板 `EC` §3）：★ **已停用**（`SPEC §9 #5` 未拍 ⇒ 只记本地账，不发声明）。
     * 见 trance_declare_score()：那 10000 一旦发出去会污染 110/111 的 PV 聚合。*/
    trance_declare_score(s);

    /* ⑨ 汇总日志 */
    if (s->frame - s->log_frame >= TRANCE_LOG_EVERY) {
        s->log_frame = s->frame;
        ce_log("trance: gauge %d/%d tier %d phase %d, spirits %d/%d/%d/%d, chain %d(bonus %d), "
               "activations %d rescued %d redie %d resclost %d levelups %d killdrop %d killspirit %d "
               "| entity pool: %d/%d alive, spawned %d picked %d expired %d dropped %d "
               "(r=%d px, life %d f, scale %d->%d over %d f), "
               "score-decl BLOCKED (decl %d, %d frames)",
               (int)s->core.gauge.gauge, TRANCE_GAUGE_MAX, (int)s->core.gauge.tier, (int)s->core.phase,
               (int)s->core.spirits_seen[0], (int)s->core.spirits_seen[1],
               (int)s->core.spirits_seen[2], (int)s->core.spirits_seen[3],
               (int)s->core.chain_count, (int)s->core.chain_bonus_spawned,
               (int)s->core.activations, (int)s->stat_rescued, (int)s->stat_redie,
               (int)s->stat_rescue_lost, (int)s->stat_levelups,
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
    ce_log("trance: +0x14 (frame %d, phase %d, gauge %d) - death settled", 
           (int)s->frame, (int)s->core.phase, (int)s->core.gauge.gauge);
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
           "spirit ENTITY pool (%d slots, script %d sprite %d layer %d) -> walk within %d px to collect "
           "(type %d, %d per kill) -> trance_collect -> gauge; "
           "MID-HIT RESCUE (2nd revision, landing fixed): at card slot +0x0c "
           "(on_death_after_deathbomb, after the 8-frame deathbomb window and after the B path) -> "
           "no other claimer -> trance_force_on_death + cancel_impending_death 0x%x + **return 1**; "
           "window end -> Player__die 0x%x (delay, not immunity; chaining allowed); "
           "PENDING: score CARRIER (SPEC 9 #5 — **declaration STOPPED**, see trance_declare_score), "
           "HUD slot (#6), channel E/F spawns (#4), astral art (SPEC 5)",
           trance_core_selfcheck(), TRANCE_SPIRIT_POOL, TRANCE_SPIRIT_ANM_SCRIPT, TRANCE_SPIRIT_SPRITE,
           TRANCE_SPIRIT_LAYER, TRANCE_SPIRIT_PICK_RADIUS,
           TRANCE_KILL_SPIRIT_TYPE, TRANCE_KILL_SPIRITS_PER_KILL,
           (unsigned)TRANCE_FN_CANCEL_IMPENDING_DEATH, (unsigned)TRANCE_FN_PLAYER_DIE);
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
        .on_death_after_deathbomb = on_death_after_deathbomb,   /* ★★ 中弹救援（救援层最后一家）*/
        .on_death_frame2 = on_death_frame2,     /* ★ 死亡结算之后 +0x14：只记日志 */
        .on_enemy_death = on_enemy_death,       /* ★ 击破 → 神灵（事件钩子，非虚表槽）*/
        .on_stage_start = on_stage_start,
        .on_draw = on_draw,
        .on_run_reset = on_run_reset,
        .on_delete = on_delete);
