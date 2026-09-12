/* senro_core.h —— 系统卡 107「妖妖梦」的纯逻辑（th07 樱点 / 森罗结界）。
 *
 * 本文件**不 include 任何引擎头**（engine.h / sdk.h 都不需要），全是整数算术：
 * 可以直接在主机上编出来跑单测（`tests/test_senro_core.c`）。
 * 引擎接线（消弹调用、无敌声明、HUD、道具 / 得分事件、删卡清理）全在 `senro.c`。
 *
 * ★ 也**不用任何浮点**：TH18 的 `make dllx87` 门断言「我们的 object 里 0 条 x87 指令」，
 *   而 i686 Windows ABI 的 float 返回值走 x87 `st(0)` ⇒ 这里一个 float 都不出现（整数定点）。
 *
 * ── 一手依据（TH07 v1.00b，`local/th07.v1.00b/th07.exe`，imagebase `0x400000`）────────────
 * 本文所有 `0x…` 均属 th07（反汇编全文 = `local/_scratch/th07.asm`，172,741 行）；
 * 台账与研究：`local/_scratch/research-107-th07-remaining.md`（下称 research）、
 * `docs/superpowers/specs/2026-09-11-system-card-spec-review.md` §9（下称 review）；
 * 本卡 spec：`docs/superpowers/specs/2026-09-12-senro-107-design.md`（下称本卡 spec）。
 *
 *   · **樱点加法器**（唯一「加」出口）`0x42f5a2(delta)`：`cherry(+0x961c) += delta`，
 *     夹 ≤ `cherry_max(+0x9618)`；再 `if (delta > 0 && [0x4bfee5] == 0) plus(+0x9620) += delta`，
 *     `plus ≥ 基准 + 0xc350(50000)` ⇒ 夹到基准+50000 并 `call 0x441960` 开结界。
 *     逐指令：`0x42f5bf` / `0x42f5d7` / `0x42f5f7`(闸门) / `0x42f60b` / `0x42f623` / `0x42f652`。
 *     ⇒ 「负的 delta 不动 +C」与「+C 只涨不因扣分而掉」是同一条 if 的直接后果。
 *   · **只加樱点**（不动 +C）：`0x42f69f(delta)`（含「满值飘字」）与 `0x42f736(delta)`。
 *     两者函数体逐指令只有 0x961c 一个写点（`0x42f6c2` / `0x42f759`）。
 *   · **只加最大樱点**：`0x42f789(delta)` = `cherry_max += delta`，夹 `基准 + 0x989676`（`0x42f7b1`）。
 *   · **最大樱点初值**（按难度查表，`0x42eb7a`–`0x42ec57`，写 `+0x9618`）：
 *     E 200000 / N 200000 / H 250000 / L 300000 / EX 400000(且 `cherry` 初值 200000, `0x42ec21`) /
 *     PH 400000(且 `cherry` 初值 300000, `0x42ec57`)。见 `senro_cherry_max_init()`。
 *   · **道具侧三档加值**（`ItemManager::on_tick` `0x432990` 的 type 跳表，research §1.2）：
 *     type 2 = `1000 + 100×[globals+0x1c]`（`0x433886`–`0x4338d9`，走 0x42f5a2 ⇒ 樱点与 +C 同加）；
 *     type 7（★）= `+20`（`0x43370b`→`0x433712`）；
 *     type 8（小樱点）= `+30`（`0x433742`→`0x433749`，走 0x42f5a2）**加** `+70`
 *     （`0x43374e`→`0x433755`，走 0x42f69f）⇒ **樱点 +100 / +C +30**；
 *     type 9 = 先 `((命/40)×10+300)/10` 进分数（`0x43394c`），再 `+100`（`0x433958`→`0x433992`）。
 *   · **结界中擦弹**（`Player_score_graze` `0x43eb90` 尾段 `0x43ed5f`–`0x43eda9`，一手）：
 *     `[player+0x240d] == 1`（结界中）时，`[player+0x240b] == 0`（= 非聚焦 / 高速）⇒
 *     `add_cherry_max(0x50=80)` + `add_cherry(0x50=80)`；`!= 0`（聚焦 / 低速）⇒ 各 `0x1e=30`。
 *     ★ **两者都加「樱点」与「最大樱点」，都不动 +C** —— 由此 review §7.7 的「高速 +80 / 低速 +30」
 *     从 🟡 单源升为 ✅ 一手；`[player+0x240b]` = 低速位（写点 `0x43efa4`/`0x43f0ba`，源 = 键位掩码 `0x4b9e50` bit 0x4）。
 *   · **自然结束** `Player_update_border_end` `0x441670`：`cherry_max += 10000`（`0x44167a`→`0x42f789`）、
 *     `cherry += 10000`（`0x441689`→`0x42f736`），随后 `分数 += (cherry − 基准)`（`0x44169d`–`0x4416c6`；
 *     中间的 `imul ×10`（`0x4416af`）与 `idiv 10`（`0x4416c4`）相消，末尾 `sub eax,edx` = 向下取整到 10），
 *     Plus = 基准（`0x4416ee`）。
 *   · **打断（中弹 / 按 B）** `0x441bd0`：清结界、起特效、`[player+0x23fc] = 0x28 = 40` 帧无敌
 *     （`0x441d8e`；同段 `player+0x16a00` 的 zTimer 长度也是 0x28）、Plus = 基准（`0x441da4`）。
 *     中弹入口 `0x440d1d`（`[player+0x240d] == 1` 门）、按 B 入口 `0x4419d2`。
 *   · **Miss 扣樱点** `0x440e84`–`0x440ef0`：`cherry −= round10(min(trunc(cherry_rel × [SHT+0x1c]), CAP))`，
 *     `CAP = 60000`（`[0x62f645] == 2` = 咲夜）否则 `100000`；**不夹回基准**、**不动 +C / 最大樱点**。
 *     `[SHT+0x1c]`：灵梦 / 魔理沙 **0.5**、咲夜 **0.33**（review §9.3）。
 *   · **Bomb 扣樱点** `0x440ad9`（额度 `[player+0x16a2c]` 由 `0x408610` 按各炸弹的 ratio 0.17–0.41 写）：
 *     扣完**夹回基准**（`0x440aec`）、**不动 +C / 最大樱点**。
 *   · **蓝点（点道具）分值**（type 1，`0x4330db`–`0x4331e5`）：线上 = `50000`（`0x43310a`），
 *     线下 = `50000 − (y − 线高)×100`；再 `+ (樱点 − 基准 − 50000)/5`（`0x4331c8` `idiv 5`），
 *     末尾向下取整到 10（`0x4331db` `idiv 10` + `sub`）。⇒ 「樱点 = 最大得点」的一手公式。
 *   · **满槽开门阈值** `0xc350 = 50000`：全 exe 只有 4 处（`0x4049a4` / `0x404c5a` / `0x404c86` / `0x428001`，
 *     review §9.4），其中 `0x4049a4` 判「≥ 50000 → 亮青 `(0x80,0xFF,0xFF)`、否则浅蓝 `(0x80,0xD0,0xFF)`」。
 *
 * ── 与 research 的两处**一手订正**（本文件的实现以订正后为准；本卡 spec §2 记了逐条）──────────
 *   ① research §1.4 把 `0x42f69f` 认成「最大樱点 / Plus 相关」加法器 —— **错**：逐指令它写的是
 *      `[+0x961c]`（樱点），且不碰 plus。⇒ 小樱点 = **樱点 +100**（30 + 70）/ **+C +30**。
 *   ② research §1.4 说 ★（type 7）另给「cherry_max 相关 +10」—— **错**：`0x433738` 走的是
 *      `0x42f69f`（只加樱点），`0x433755` 同理。全 exe 的 `+0x9618` 写点只有 `0x42ebxx`（难度表初值）
 *      与 `0x42f789`（结界自然结束 / 结界中擦弹）两族 ⇒ ★ 与小樱点都**不涨最大樱点**。
 *   ③ `0x433702 cmp [0x4d44f8],0` 的分支（★ 走 +10、且半数只加樱点）依赖一个**全 exe 无写点**的
 *      运行时标志 ⇒ 本卡按默认分支 `[0x4d44f8] == 0`（+20 / +C +20）实现，标志身份 🟡 未解。
 */
#pragma once
#include <stdint.h>

/* ============================ 数值表 ============================ */

/* —— 时间 / 阈值（一手）—— */
#define SENRO_PLUS_FULL            50000   /* +C 满槽（th07:0x42f623 的 0xc350）*/
#define SENRO_BORDER_FRAMES          420   /* 结界时长 = 7 s @60fps（用户 2026-09-12 定案）。
                                            * ⚠ th07 里并存 0x21c(540)（0x4419fa 写计时长度）与 0x1a4(420)
                                            * （0x40dbdc）两个数，且计时闸门 [player+0x16a14] 全 exe 无写点
                                            * ⇒ 7 s 是**用户口径**，不是反汇编结论（本卡 spec §5.2）。*/
#define SENRO_INVULN_FRAMES           40   /* 灵击后的无敌帧（th07:0x441d8e `[player+0x23fc] = 0x28`）*/
#define SENRO_BORDER_END_ADD       10000   /* 自然结束：樱点与最大樱点各 +10000（0x44167a / 0x441689）*/
#define SENRO_CHERRY_MAX_CAP     9999990   /* 最大樱点的绝对上限（0x42f7b1 的 0x989676，相对基准）*/

/* —— 各通道加值（一手）—— */
#define SENRO_ITEM_BIG_BASE         1000   /* 樱点（大）道具：1000 + 100×符卡奖励次数（0x433886）*/
#define SENRO_ITEM_BIG_PER_BONUS     100   /* 上面那一项（0x433895 `imul ecx,ecx,0x64`）*/
#define SENRO_STAR_CHERRY             20   /* ★（消弹产物）樱点 / +C 各 +20（0x43370b→0x433712）*/
#define SENRO_SMALL_CHERRY_ADD        30   /* 小樱点：樱点 +30 **且** +C +30（0x433742→0x42f5a2）*/
#define SENRO_SMALL_CHERRY_EXTRA      70   /* 小樱点：再只加樱点 +70（0x43374e→0x42f69f）*/
#define SENRO_GRAZE_FAST              80   /* 结界中擦弹（非聚焦 / 高速）：樱点与最大樱点各 +80（0x43ed96）*/
#define SENRO_GRAZE_SLOW              30   /* 结界中擦弹（聚焦 / 低速）：各 +30（0x43ed7c）*/

/* —— 点道具（蓝点）分值：一手公式的三个锚点 —— */
#define SENRO_POINT_BASE           50000   /* 线上满额基线（0x43310a 的 0xc350）*/
#define SENRO_POINT_DIV                5   /* 超出基线的部分 /5（0x4331c8 `idiv 5`）*/

/* —— ❓ 自定占位（原版量在 TH18 侧没有对应载体 / 未取到；全部具名 + TODO(th07)）—— */
#define SENRO_HIT_DIV                 20   /* ❓ 自定 TODO(th07)：自机弹命中 → 樱点。
                                            * 一手锚点只有「th07 那条公式的输出**上限 70/次**」
                                            * （0x42121f `cmp [ebp-0x40],0x46` + 0x421225 夹取）与
                                            * 「输出恒为 10 的倍数」（0x421203 `imul eax,eax,0xa`）；
                                            * 它的输入来自敌人数据（`0x42118b` 之前的浮点累加），
                                            * TH18 拿不到 ⇒ 用「伤害 / 本常量」折算。*/
#define SENRO_HIT_CAP                 70   /* 一手：单次命中加值上限（0x42121f / 0x421225）*/
#define SENRO_HIT_FAST_NUM             3   /* IDEAS §107：「同机体高速射击约低速的 3 倍」*/
#define SENRO_HIT_SLOW_NUM             1
#define SENRO_MISS_NUM                 1   /* Miss 扣减比例：th07 每机体不同（灵梦/魔理沙 1/2、咲夜 1/3，
                                            * [SHT+0x1c]，review §9.3）。TH18 不读 SHT 表 ⇒ ❓ 自定取 1/2；
                                            * 「按 TH18 自机分档」见本卡 spec §9 未定项。*/
#define SENRO_MISS_DEN                 2
#define SENRO_MISS_CAP            100000   /* 一手：Miss 扣减上限（0x440eb2/0x440ec4 的 CAP；咲夜 60000）*/
#define SENRO_BOMB_NUM                 1   /* Bomb 扣减比例：th07 十二个炸弹各自 0.17–0.41（review §9.3）
                                            * ⇒ ❓ 自定取 1/4，TODO(th07) 逐炸弹取值。*/
#define SENRO_BOMB_DEN                 4

/* —— TH18 item type（engine.h 的 `CE_ITEM_TYPE` 一手枚举；th07 侧无此编号）—— */
#define SENRO_TH18_ITEM_MONEY          2   /* TH18 金钱道具 = 本卡的「点道具 / 樱点道具」载体（type 2）*/
#define SENRO_TH18_ITEM_SCORE_LO       9   /* TH18 分数转换点族（消弹产物）= 本卡的「★」载体（type 7）*/
#define SENRO_TH18_ITEM_SCORE_HI     0xe

/* ============================ 事件（位域） ============================ */
#define SENRO_EV_NONE                 0
#define SENRO_EV_BORDER_OPEN       0x01    /* 本轮开门（th07:0x441960 正常展开路径）*/
#define SENRO_EV_BORDER_QUEUE      0x02    /* 满槽但正在放炸弹 ⇒ 排队（th07:0x441983）*/
#define SENRO_EV_END_NATURAL       0x04    /* 自然结束（th07:0x441670）*/
#define SENRO_EV_CHERRY_MAXED      0x08    /* 樱点刚触到最大樱点（th07 会飘一条「满值」字，0x42f679）*/

/* ============================ 状态 ============================ */
typedef struct {
    int32_t cherry;        /* 樱点（= 最大得点 的载体；th07 `[gm+0x961c]`）*/
    int32_t cherry_max;    /* 最大樱点（上限；th07 `[gm+0x9618]`）*/
    int32_t plus;          /* +C（结界充能 0..SENRO_PLUS_FULL；th07 `[gm+0x9620]`）*/
    int32_t active;        /* 1 = 结界展开中（th07 `[player+0x2408] == 4` / `[player+0x240d] == 1`）*/
    int32_t queued;        /* 1 = 满槽但放炸弹中，等炸弹结束再开（th07 `[player+0x240d] == 2`）*/
    int32_t timer;         /* 结界剩余帧（th07 `[player+0x16a08]`）*/
    int32_t invuln;        /* 灵击后的无敌剩余帧（走 SDK 的无敌声明层，本卡不写计时器）*/
    int32_t graze_last;    /* 上一帧原生擦弹计数；-1 = 未初始化（差分求「本帧擦了几颗」）*/
    int32_t spell_bonuses; /* 本关取得的「符卡奖励」次数（th07 type 2 的 `[globals+0x1c]` 项）*/
    int32_t last_bonus;    /* 上一次自然结束的得分 Bonus（日志 / 飘字用）*/
    /* 事件与帧态（由本模块维护，卡侧只读 / 取走）*/
    int32_t ev_pending;    /* 累积的 SENRO_EV_* 位域；`senro_take_events()` 取走并清零 */
    int32_t bomb_active;   /* 本帧「正在放炸弹」（`senro_tick()` 每帧记；加法器用它决定开门还是排队）*/
    /* 诊断计数（日志对账）*/
    int32_t stat_open, stat_natural, stat_libyou, stat_cancel, stat_items, stat_hits;
} senro_state_t;

/* 难度下标（th07 的 6 档；TH18 的 0..3 = E/N/H/L，4 = Extra，5 用不到）*/
#define SENRO_DIFF_EASY      0
#define SENRO_DIFF_NORMAL    1
#define SENRO_DIFF_HARD      2
#define SENRO_DIFF_LUNATIC   3
#define SENRO_DIFF_EXTRA     4
#define SENRO_DIFF_PHANTASM  5

/* ============================ 核心 ============================ */

/* 难度表（一手 0x42eb7a–0x42ec57，返回值已含「基准」= 0）：最大樱点初值 / 樱点初值。 */
int32_t senro_cherry_max_init(int difficulty);
int32_t senro_cherry_init(int difficulty);

/* 初始化成「一关的开场态」：cherry = 难度表初值、cherry_max = 难度表初值、plus = 0、
 * 结界关、无敌 0、擦弹差分基准撤销（-1）。统计计数清零。 */
void senro_init(senro_state_t *s, int difficulty);
/* 每关开场：与 init 同（th07 每关重设基准 `0x4043b9`；本卡把「基准」取 0 ⇒ 樱点每关从 0 起）。
 * 统计计数保留（跨关累计，日志用）。 */
void senro_stage_reset(senro_state_t *s, int difficulty);

/* 樱点加法器（th07:0x42f5a2）：cherry += delta（夹 ≤ cherry_max）、
 * plus += delta（**只在 delta > 0 时**，夹 ≤ SENRO_PLUS_FULL），满则开门 / 排队。
 * 返回「累积至今」的 SENRO_EV_* 位域（与其它加法器共用 `ev_pending`，用 take_events 取走）。
 * **所有正向加值都走这里**（道具 / 擦弹以外的通道 / 小樱点）。 */
int senro_add_cherry(senro_state_t *s, int32_t delta);
/* 只加樱点（th07:0x42f69f / 0x42f736）：不动 +C、不动最大樱点。返回累积事件。 */
int senro_add_cherry_only(senro_state_t *s, int32_t delta);
/* 只加最大樱点（th07:0x42f789）：夹 ≤ SENRO_CHERRY_MAX_CAP。 */
void senro_add_cherry_max(senro_state_t *s, int32_t delta);
/* 取走累积事件并清零（卡侧在每次入账之后 / 每帧 `senro_tick` 之后调它做表现与日志）。 */
int senro_take_events(senro_state_t *s);

/* 每帧推进（bomb_active = 「正在放炸弹」，th07 用它决定开门还是排队）。
 * 返回 SENRO_EV_* 位域；自然结束时额外置 SENRO_EV_END_NATURAL 并把得分 Bonus 写进 `s->last_bonus`。 */
int senro_tick(senro_state_t *s, int bomb_active);

/* 结界中擦弹：`n` 颗、`slow` = 聚焦（低速）。返回本次加到樱点的总量（0 = 不在结界中）。 */
int32_t senro_graze(senro_state_t *s, int32_t n, int slow);

/* 收一颗道具：返回本颗加了多少樱点（0 = 这颗道具不给樱点）。type = TH18 item type。 */
int32_t senro_item_cherry(senro_state_t *s, uint32_t type);

/* 自机弹命中（`slow` = 聚焦）：damage → 樱点/ +C。返回本次加了多少樱点。 */
int32_t senro_hit(senro_state_t *s, int32_t damage, int slow);

/* 点道具（蓝点）分值 = 本卡的「最大得点」：`SENRO_POINT_BASE + max(0, cherry − BASE)/5`，
 * 向下取整到 10（th07:0x4330db–0x4331e5 的一手结构）。返回**分**（不是 ×10 的身价）。 */
int32_t senro_point_item_value(const senro_state_t *s);

/* 灵击（中弹 / 按 B 的那一下）：结束结界 + 每颗被消的弹记一颗小樱点 + 40 帧无敌。
 * `bullets` = 本次全屏消弹的颗数（调用方用 `ce_cancel_radius()` 的返回值）。
 * 返回 1 = 确实发了灵击（原本就在结界里），0 = 没结界（什么都没做）。
 * ⚠ 中弹那一路的消弹要在**下一帧**才做得到（见 `senro.c` 的说明）⇒ 卡侧把这一步拆成
 *   `senro_libyou(s, 0)`（当场收尾）+ 下一帧的 `senro_small_cherry(s, n)`（补记颗数）。 */
int senro_libyou(senro_state_t *s, int32_t bullets);

/* 小樱点入账（灵击消弹的颗数）；th07 type 8 的两条调用：+30 走加法器（樱点与 +C 同加）、
 * +70 只加樱点 ⇒ 每颗 **樱点 +100 / +C +30**。返回加到樱点的总量。 */
int32_t senro_small_cherry(senro_state_t *s, int32_t bullets);

/* Miss / Bomb 扣樱点：**不动 +C、不动最大樱点**（用户 2026-09-10 口径 + 一手一致）。
 * 返回扣掉的量（正数）。 */
int32_t senro_penalty_miss(senro_state_t *s);
int32_t senro_penalty_bomb(senro_state_t *s);

/* 工具：th07 的「取整到 10」= `v − v%10`（`0x440ee2` / `0x4331e5` 的 `idiv 10` + `sub`；对负数取向下）。*/
int32_t senro_round10(int32_t v);
/* 结界中 +C 的每帧值（th07:0x4414db：`timer × 50000 / 总时长`）。timer ≤ 0 → 0。 */
int32_t senro_plus_for_timer(int32_t timer);
/* 结界是否展开中。 */
int senro_border_active(const senro_state_t *s);
