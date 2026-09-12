/* th06rank_core.h —— 系统卡 106「红魔乡」的纯逻辑（PlayRank 复刻）。
 *
 * 本文件**不 include 任何引擎头**（engine.h / sdk.h 都不需要）：全是整数算术 + 一张常量表，
 * 可以直接在主机上编出来跑单测（`tests/test_th06rank_core.c`）。
 * 引擎接线、帧源、库存差分全在 `th06rank.c`。
 *
 * ── 单位（本卡唯一要记住的一件事）────────────────────────────────────────────
 * th06 的 `subRank` 单位是 **1/100 rank**（`while (subRank >= 100) rank++`）。
 * 但 spec 表里有两档数值在 1/100 里**除不尽**：蓝点非最大 +0.06、非最大蓝点的 1/5 档 +0.006。
 * 所以本卡把 subRank 再放大 100 倍 ⇒ 单位 = **1/100000 rank**（= 10⁻⁵ rank），进位阈值
 * `T06_SUBRANK_CARRY = 100000`（= 1.00 rank）。这样 12 条的数值全部是精确整数，
 * 且**每条都与 spec §3 一一对应**：
 *
 *   +1.00 帧数 → 100000 | +0.06 擦弹 → 6000     | +2.00 整命 → 200000
 *   +0.05 整炸 → 5000   | +0.30 蓝点 → 30000    | +0.03 蓝点 → 3000   （÷5 后 = 6000 / 600）
 *   +0.01 小 P → 1000   | +0.70 命碎 → 70000    | +0.02 B碎 → 2000
 *   −0.03 出屏 → 3000   | −2.00 Bomb → 200000   | −16.00 Miss → 1600000
 *
 * ⇒ **换算锚点：th06 的原值单位（1/100 rank）× T06_UNIT_PERMILLE(1000) = 本卡单位**
 *   （因为 100000 单位 = 1.00 rank = 100 个原值单位 ⇒ 1 原值单位 = 1000 单位）。
 *   ÷5 的两条通道（蓝点 / 小 P）里存的是**除之前的 th06 原值**（原值 30 / 3 / 1），
 *   由 `t06_point_gain()` / `t06_power_small_gain()` 在运行期 ÷ T06_RATIO_DIV ——
 *   两条通道的**最终值**同样精确落在 spec 的 +0.06 / +0.006 / +0.002 上。
 *
 * ★★ **2026-09-12 对抗审计订正（两条常量的量纲是错的，`_Static_assert` 已把它钉死）**：
 *   ① `T06_INC_GRAZE` 原写 `60`（= 0.0006 rank），比 spec 的 **+0.06 rank** 小 **100 倍**
 *      —— 要擦满 1666 次才进 1 档（应为 16 次）⇒ 已改 **6000**。
 *   ② `T06_DEC_OFFSCREEN` 原写 `300`（= 0.003 rank），比 spec 的 **−0.03 rank** 小 **10 倍** ⇒ 已改 **3000**。
 *   ③ 其余十条（含两条 ÷5 通道的**最终值**）逐条复核无误；末尾那组 `_Static_assert` 防止再犯同类错。
 *
 * rank 本身仍是 th06 的整数（初值 16、EASY 下限 12、上限 20/32/32/32/18）。
 * 出处：spec `docs/superpowers/specs/2026-09-10-th06-playrank-design.md` §2.2/§3
 * + 订正 `2026-09-11-system-card-spec-review.md` §5.1（碎片会计、`>= 200.0`、最小 rank 12/10/10/10/14）。
 */
#pragma once
#include <stdint.h>

/* ── 常量 ─────────────────────────────────────────────────────────────── */

/* subRank 的单位 = 1/100000 rank（进位阈值 T06_SUBRANK_CARRY = 1.00 rank）。
 * 换算锚点：**1 个 th06 原值单位（1/100 rank）= T06_UNIT_PERMILLE(1000) 个本卡单位**
 * （100000 单位 = 1.00 rank = 100 个原值单位）。 */
#define T06_SUBRANK_CARRY      100000
#define T06_UNIT_PERMILLE        1000     /* = th06 的 1 个原值单位（0.01 rank）换算成本卡单位 */

/* 难度表下标（= th06 的 `g_DifficultyInfo[]` 下标顺序）。th06:GameManager.cpp:70-76 一手。 */
#define T06_DIFF_EASY     0
#define T06_DIFF_NORMAL   1
#define T06_DIFF_HARD     2
#define T06_DIFF_LUNATIC  3
#define T06_DIFF_EXTRA    4
#define T06_DIFF_COUNT    5

/* 帧数条件（th06:EnemyManager.cpp:162-170 一手）：
 *     interval = 10*4*60 − livesRemaining * 4*60        （2400 − 残机数 × 240，帧）
 *     if (stage_frame != 0 && stage_frame % interval == 0) IncreaseSubrank(1.00)
 * ⇒ 残机越多升越快。`stage_frame` = **关卡时间轴**（每关从 0 起），本卡用
 * `on_item_frame` 的游玩帧计数当它（见 th06rank.c 的帧源注）。 */
#define T06_FRAME_BASE         2400
#define T06_FRAME_PER_LIFE      240

/* 出屏判据（TH18 替换 th06 的 `|x| >= 200.0` / `y > 472.0`：一手
 * `ItemManager__on_tick__body` 0x445e38 段，常量 `0x4b9340` = 200.0f / `0x4b93a4` = 472.0f）。
 * ★ 这两个是**浮点引擎常量**，只在卡片侧用一次，定义在 `th06rank.c` 的
 *   `T06R_ITEM_X_LIMIT` / `T06R_ITEM_Y_LIMIT`（纯逻辑 core 不碰浮点，保持可在主机单测里跑整数算术）。 */

/* 12 条增减的数值（本卡单位 = 1/100000 rank；出处见文件头与 spec §3）。
 *   换算锚点：**「照抄 / 不除」那九条 = th06 原值 × T06_UNIT_PERMILLE**（= 1000）。
 *   ÷5 的两条通道例外（见文件头）：它们存的是**除之前的 th06 原值**，运行期再 ÷ T06_RATIO_DIV。
 *   ★ 末尾那组 `_Static_assert` 是这条锚点的**编译期闸门**：谁把某一位写错一个 0，这里就红。 */
#define T06_INC_FRAME          100000     /* +1.00  原值 100   帧数条件（EnemyManager.cpp:170 Inc(100) 照抄）*/
#define T06_INC_GRAZE            6000     /* +0.06  原值 6     擦弹（Player.cpp:1394 Inc(6)，用户定「不除」）
                                           * ★ 审计订正：原写 60（= 0.0006 rank，比 spec 小 100 倍）*/
#define T06_INC_LIFE_FULL      200000     /* +2.00  原值 200   整命 / 1UP（ItemManager.cpp:299 Inc(200) 照抄）*/
#define T06_INC_LIFE_FRAG       70000     /* +0.70  定值 70    命碎（TH18 自有通道；3 碎 = 2.1，不读难度表）*/
#define T06_INC_BOMB_FULL        5000     /* +0.05  原值 5     整炸（ItemManager.cpp:291 Inc(5) 照抄）*/
#define T06_INC_BOMB_FRAG        2000     /* +0.02  定值 2     B 碎（TH18 自有通道；3 碎 = 1 张）*/
#define T06_INC_POINT_HIGH      30000     /* 蓝点线上：原值 30（ItemManager.cpp:234 Inc(30)）÷5 后 = 6000
                                           * = spec 的 **+0.06 rank**（★ 这一条本来就对，审计只改注释）*/
#define T06_INC_POINT_LOW        3000     /* 蓝点线下：原值 3（ItemManager.cpp:238 Inc(3)）÷5 后 = 600
                                           * = spec 的 **+0.006 rank**（同上，本来就对）*/
#define T06_INC_POWER_SMALL      1000     /* 小 P：原值 1（ItemManager.cpp:205 Inc(1)；大 P 不加）÷5 后 = 200
                                           * = spec 的 **+0.002 rank**；装了 110/111 时 ×5 ⇒ +0.01（spec §8）*/
#define T06_RATIO_DIV               5     /* 用户定：÷5 只作用于「蓝点」「小 P」两条通道（spec §3 归一化规则）*/
#define T06_DEC_OFFSCREEN        3000     /* −0.03  原值 3     道具掉出屏幕（ItemManager.cpp:145 Dec(3)，不除）
                                           * ★ 审计订正：原写 300（= 0.003 rank，比 spec 小 10 倍）*/
#define T06_DEC_BOMB           200000     /* −2.00  原值 200   放 Bomb（Player.cpp:185 Dec(200)）*/
#define T06_DEC_MISS          1600000     /* −16.00 原值 1600  Miss 非决死（Player.cpp:225 Dec(1600)）*/

/* ── 量纲闸门（编译期）：把「th06 原值 → 本卡单位」这条换算钉死 ──────────────────
 * 这组断言是 2026-09-12 审计加的：上面两处订正（擦弹 60→6000、出屏 300→3000）都是
 * 「少写一个 0」这一种错误，而注释挡不住它 —— 静态断言可以。 */
_Static_assert(T06_UNIT_PERMILLE == 1000, "1 个 th06 原值单位（1/100 rank）= 1000 本卡单位");
_Static_assert(T06_SUBRANK_CARRY == 100 * T06_UNIT_PERMILLE, "1.00 rank 必须 = 100 个原值单位");
/* 「照抄 / 不除」的九条：本卡值 = th06 原值 × 1000 */
_Static_assert(T06_INC_FRAME     ==  100 * T06_UNIT_PERMILLE, "帧数条件：原值 100（+1.00）");
_Static_assert(T06_INC_GRAZE     ==    6 * T06_UNIT_PERMILLE, "擦弹：原值 6（+0.06）");
_Static_assert(T06_INC_LIFE_FULL ==  200 * T06_UNIT_PERMILLE, "整命：原值 200（+2.00）");
_Static_assert(T06_INC_BOMB_FULL ==    5 * T06_UNIT_PERMILLE, "整炸：原值 5（+0.05）");
_Static_assert(T06_DEC_OFFSCREEN ==    3 * T06_UNIT_PERMILLE, "出屏：原值 3（−0.03）");
_Static_assert(T06_DEC_BOMB      ==  200 * T06_UNIT_PERMILLE, "Bomb：原值 200（−2.00）");
_Static_assert(T06_DEC_MISS      == 1600 * T06_UNIT_PERMILLE, "Miss：原值 1600（−16.00）");
/* ÷5 的三条：存的**不是**最终值，而是「还没 ÷5 的 th06 原值」⇒ 这里查
 * `最终值 = 原值 × 10000 ÷ 5`，即 spec §3「本卡值」列的字面值（6000 / 600 / 200）。 */
_Static_assert(T06_INC_POINT_HIGH % T06_RATIO_DIV == 0, "蓝点线上的 ÷5 必须整除");
_Static_assert(T06_INC_POINT_LOW  % T06_RATIO_DIV == 0, "蓝点线下的 ÷5 必须整除");
_Static_assert(T06_INC_POWER_SMALL % T06_RATIO_DIV == 0, "小 P 的 ÷5 必须整除");
_Static_assert(T06_INC_POINT_HIGH / T06_RATIO_DIV ==    6 * T06_UNIT_PERMILLE, "蓝点线上 ÷5 后 = +0.06 rank");
_Static_assert(T06_INC_POINT_LOW  / T06_RATIO_DIV ==    6 * T06_UNIT_PERMILLE / 10, "蓝点线下 ÷5 后 = +0.006 rank");
_Static_assert(T06_INC_POWER_SMALL / T06_RATIO_DIV ==   2 * T06_UNIT_PERMILLE / 10, "小 P ÷5 后 = +0.002 rank");
_Static_assert(T06_INC_POWER_SMALL / T06_RATIO_DIV * T06_RATIO_DIV == 1 * T06_UNIT_PERMILLE,
               "装了 110/111 时小 P ×5 回到原值 = +0.01 rank");



/* on_item_collect 的 item type → 动作（TH18 item type；spec §3.1 一手全表）。 */
typedef enum {
    T06_ITEM_IGNORE = 0,   /* 3 大 P / 8 满 P 转分 / 9-0xe 分数 / 0x12 / 0x13 / 未知 */
    T06_ITEM_POWER_SMALL,  /* 1 = P 点：÷5 ⇒ +0.002（装了 110/111 时 ×5 ⇒ +0.01）*/
    T06_ITEM_POINT,        /* 2 = 金钱（== 蓝点）：按回收线 +0.06 / +0.006 */
    T06_ITEM_LIFE_FRAG,    /* 4 = 命碎：+0.70（每颗都算，含进位那颗）*/
    T06_ITEM_BOMB_FRAG     /* 6 = B 碎：+0.02 */
} t06_item_action_t;

/* ── 难度表（th06:GameManager.cpp:61-77 一手；难度表的 min/max 就是夹取上下限） ── */
typedef struct {
    int32_t rank;      /* 每关开场初值，恒 16 */
    int32_t min_rank;  /* EASY 12（不是 10 —— 反编译值）*/
    int32_t max_rank;
} t06_difficulty_t;

const t06_difficulty_t *t06_difficulty_table(void);   /* 5 条，下标 T06_DIFF_* */

/* ── rank 状态（4 个 int32，照 th06 的 `GameManager.rank/minRank/maxRank/subRank`） ── */
typedef struct {
    int32_t rank;
    int32_t min_rank;
    int32_t max_rank;
    int32_t sub_rank;    /* PERMILLE，恒 [0, T06_SUBRANK_CARRY) */
} t06_rank_t;

/* ── 核心（逐字照 th06 `GameManager::IncreaseSubrank/DecreaseSubrank`） ──────────
 * 归一化后语义完全一致：整数部分进位/借位、跨过上下限就夹取、**小数部分不修正**。
 * 返回值 = 本次调用净变的 rank 数（可为 0 / 正 / 负；夹取后再算，用于日志）。 */
void    t06_init(t06_rank_t *r, int32_t difficulty);
int32_t t06_increase(t06_rank_t *r, int32_t amount);
int32_t t06_decrease(t06_rank_t *r, int32_t amount);

/* 只读快照（card 层日志 / 主机单测断言用）。 */
static inline int32_t t06_rank(const t06_rank_t *r)     { return r->rank; }
static inline int32_t t06_sub_rank(const t06_rank_t *r) { return r->sub_rank; }

/* ── 12 条里的纯函数部分（没有引擎状态的那几条） ───────────────────────────── */

/* 帧数条件的判据（th06 `timelineTime.AsFrames() % interval == 0`，且 0 帧不触发）。
 * `lives` = 残机数（th06 用 `livesRemaining`）。返回 1 = 本帧该 +1.00。
 * ⚠ `interval` 恒 > 0：`lives` 被钳在 [0, 10]（TH18 残机上限 7，th06 上限 8）。 */
int t06_frame_interval(int32_t lives);
int t06_frame_tick_due(int32_t stage_frame, int32_t lives);

/* 蓝点分档：`collected_at_poc` = 收集时道具 y 是否低于回收线
 *（th06 用 `currentPosition.y < 128.0`，TH18 换成 `player+0x47998`）。返回 PERMILLE。 */
int32_t t06_point_gain(int collected_at_poc);
int32_t t06_power_small_gain(int owned_110_or_111);

/* on_item_collect 的 type → 动作（5 整命 / 7 整炸 返回 IGNORE：它们走库存增量，见 spec §4.1）。 */
t06_item_action_t t06_item_action(uint32_t type);

/* ── 完整状态 = rank + 帧计数 + 差分窗口 ────────────────────────────────────
 * 一个 `t06_state_t` 就是这张卡的全部私有状态（`ce_state(c, t06_state_t)`）。 */
typedef struct {
    t06_rank_t rank;
    int32_t    stage_frame;      /* 关卡时间轴帧（本卡用 on_item_frame 的游玩帧计数当它）*/
    int32_t    life_frag_delta;  /* 本窗口命碎净变（+1 / −1 颗）*/
    int32_t    bomb_frag_delta;  /* 本窗口 B 碎净变 */
    /* 上一次 commit 时的库存快照（差分基准）。「卡被删掉再拿回来」时新对象首帧
     * 只对账、不补记中间那段库存变动（`seeded == 0`）。 */
    int32_t    life_stock_last;
    int32_t    life_frag_last;
    int32_t    bomb_stock_last;
    int32_t    bomb_frag_last;
    int32_t    seeded;           /* 0 = 还没建立基准（首帧 commit 只对账）*/
} t06_state_t;

/* 初始化四件套（rank = 难度表初值、min/max 按难度、subRank = 0）+ 帧计数清零。
 * `difficulty` 越界时按 NORMAL 处理（TH18 难度位 0..3 之外的练习 / 特殊模式兜底）。 */
void t06_state_init(t06_state_t *s, int32_t difficulty);

/* 每关开场：th06 的 `timelineTime`（关卡时间轴）与 `subRank` 都每关清零，rank 留在上一关的值。
 * 差分窗口也一起清空并撤销基准（新关首帧只对账）。 */
void t06_state_stage_reset(t06_state_t *s);

/* 帧源：每关一个 `stage_frame`，从 0 起每帧 +1（调用方保证只在「正常游玩帧」调）。 */
void t06_tick_frame(t06_state_t *s);
int32_t t06_stage_frame(const t06_state_t *s);

/* ── 碎片会计（spec §4.1 + 订正 §5.1）：本窗口的增量开关 ──────────────────────
 * 调用顺序固定三步（th06rank.c 保证）：
 *   [上一帧 on_tick_2] commit → [本帧 item 回调] 碎片登记 → [本帧 on_tick_2] commit
 * ⇒ 整命 / 整炸的库存增量与同帧的碎片事件落在**同一个窗口**里，
 *   抑制条件才成立：**本窗口内碎片净减**（不是「本帧有碎片收集」）。
 *     3 碎进位那一帧 = 碎片计数器 2→3→0（净 −2）、CURRENT_LIVES +1
 *       ⇒ 抑制该 +2.00（那 +1 命是碎片换来的；碎片那颗自己的 +0.70 已逐颗计过）
 *     碎片净 0（收到一颗、同时别处掉一颗出屏）⇒ 不抑制（正解）
 * 负向库存增量（死亡扣命 / 商店扣炸）永远不加分也不扣分（Miss / Bomb 各走自己的事件）。 */
void t06_window_item_life_fragment(t06_state_t *s, int delta);   /* +1 / −1 颗 */
void t06_window_item_bomb_fragment(t06_state_t *s, int delta);
/* 返回本次 commit 里因「整命 / 整炸」涨的 rank 数（日志用）。life_frag/bomb_frag 是**当前库存**。 */
int32_t t06_commit_frame(t06_state_t *s, int life_stock, int life_frag, int bomb_stock, int bomb_frag);
