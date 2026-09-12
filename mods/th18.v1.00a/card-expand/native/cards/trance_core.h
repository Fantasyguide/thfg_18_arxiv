/* trance_core.h —— 神灵庙 / 灵界（card-expand id 113）的**纯逻辑**。
 * 纯 C11、**不含任何引擎地址**（没有 engine.h / sdk.h）⇒ 主机单测 `tests/test_trance_core.c`
 * （`cc -std=c11 -Wall -Wextra -Werror -o /tmp/t113 tests/test_trance_core.c cards/trance_core.c`）。
 *
 * ## 覆盖 spec 的哪几条
 *
 * 设计稿：`docs/superpowers/specs/2026-09-12-ten-desires-113-design.md`（下文 `SPEC:行号`）。
 * 一手数值来源：`engine/spirit/th13/01-spirit-and-trance.md`（下文 `SPIRIT:行号`，TH13 v1.00c 一手）。
 *
 * | 本文件 | SPEC | 一手 |
 * | --- | --- | --- |
 * | 灵界槽（上限 600 / 每 200 一格 / 收神灵 +2 / type3 再 +1 / 灵界 −1 帧 / 非死亡结束 −200）| §4 表 | `SPIRIT` §5（`th13:0x4be808` / `0x43d530` / `0x43d4f0` / `0x405cdd`）|
 * | 灵界窗口状态机（相位 0 空闲 / 1 手动·自动 / 2 死亡强制；−1/帧、归零收尾）| §3.5 步 1 | `SPIRIT` §5/§6.2（`th13:0x405a20` / `0x405b80`）|
 * | 四类神灵 type 0–3 的奖励与**按 type 重建的槽账**（不按「蓝 / 白」二分）| §2.2 · §4 | `SPIRIT:73-87`（`th13:0x438730` 两张分派表）|
 * | 奖励倍率（普通 / 灵界中「翻倍」）| §2.1 行 1 · §4 | `SPIRIT:326`（`th13:0x438d09` 起的第二张表）|
 * | 激活条件**全 8 条**（满槽 600 + 场上有敌 + 30 帧冷却 + 不在对话 + 没在放 B + 不在灵界 + 键/自动 + **`(GAME_FLAGS & 0x30) != 0x20`**）| §4 表 · §1 表 | `SPIRIT:272-284`（`th13:0x4057f0`，第 8 条 `0x405859`）|
 * | 连锁（60 帧窗口内**第 10 颗**触发 ⇒ 额外的第 11 颗是 type 3）| §4 表 · §9 #7 | `SPIRIT:209-222`（`th13:0x4391a0`：`if (+0x8838 > 9)`）|
 * | **救援层记账：一次中弹烧掉全部救援者、顺序 107 → 117** | `EC` §5.2（用户 2026-09-12 定案）| 契约文（非一手反汇编）|
 * | 死亡后的 `+0x14` 动作 = **强制开灵界**（不取消死亡、不延时、不读自机坐标）| §2.1 行 4 · §7 | TH13 语义 `th13:0x444a1d`（但我们**不做延时**，落点改在死亡结算之后）|
 * | 击破 → 神灵（type 1）= **帧内队列记账 + 该撒什么/撒几颗**（回调接线在 `cards/trance.c`，**已接**）| §3.3 候选 ① · §2.2 | `SPIRIT:153-167` 通道 B（`type=1`；颗数在 TH18 无对应 ⇒ ❓ 自定 1 颗）|
 *
 * ## 明确**不在**本文件（也**不在** TH18 侧做）
 *
 * - ❌ **伤害倍率 / 清弹 / 自动收集 / 得分倍率**：原版灵界**没有这四样**（`SPIRIT:330`，证伪式一手）
 *   ⇒ 骨架（`IDEAS:400-405`）的「全收」「得分提升」是**错的三件**，本卡不做（SPEC §2.1）。
 * - ❌ **免死 / 延时**：原版死亡触发的灵界在结束时**再调一次** `Player::die`（`th13:0x405c3e`）
 *   ⇒ 原作是**延时**；用户定案是「**死后照死、只是先开灵界**」（SPEC 用户定案 ②）
 *   ⇒ 我们连延时都不做：`trance_force_on_death()` 只开窗口，**不碰生命数、不碰死亡流程**。
 * - ❌ `player+0x47774`（那是 TH18 **复活无敌** zTimer，`engine.h:277`）：灵界的「中弹无伤」**不写它**。
 *   卡侧若需要无敌帧只走 `ce_invuln_request`（取 max，SPEC §7 · `EC` §4.1）。
 * - ❌ 自建神灵实体 / anm VM / HUD VM：那些是卡侧（`cards/trance.c`）的事，本文件只出「该撒什么、加多少」。
 *
 * ## 时间源
 *
 * 全部 tick 函数由调用方传入 `frame`（**本卡的游玩帧计数**，卡侧在 `on_item_frame`（`0x446ee3`）自增，
 * 与 `cards/collect2x.c` 的 `s_play_frame` 同口径：菜单 / 商店 / 暂停帧不计入）。
 * ⚠ 卡侧要把这个计数放进**卡的私有状态**（`ce_state`），**不要**用 collect2x.c 那种文件级 static ——
 * 113 是 0x54 纯被动卡，同时只会有一张在场，但私有状态才是删卡时收得干净的那个位置。
 * 本文件**自己不看表、不取时间**⇒ replay 安全（无随机源、无 CRT `rand`）。
 */
#pragma once
#include <stdint.h>

/* ============================ 常量（全部一手值，见文件头）============================ */

#define TRANCE_ID              113    /* card-expand id（`CE_CARD(113, …)` 里必须写字面量）*/
#define TRANCE_TYPE_COUNT      4      /* type 0..3（`SPIRIT:73-87`）；type 4 是 item 14 的别名 */

#define TRANCE_GAUGE_MAX       600    /* 槽上限（`th13:0x405815` CMP …,0x258；`0x43d530` 夹到 600）*/
#define TRANCE_GAUGE_TIER      200    /* 每 200 一格，共 3 格（`0x43d530` 旧值/200 与新值/200 比较）*/
#define TRANCE_GAUGE_TIERS     (TRANCE_GAUGE_MAX / TRANCE_GAUGE_TIER)   /* 3 格 */
#define TRANCE_GAUGE_PER_SPIRIT 2     /* 收神灵**所有类型**先统一 +2（`th13:0x438b3d`）*/
#define TRANCE_GAUGE_TYPE3_BONUS 1    /* type 3 再加 1（`th13:0x438c3d`）*/
#define TRANCE_GAUGE_DRAIN      1     /* 灵界中 −1/帧（`th13:0x43d4f0`，**不是**一次性扣 200）*/
#define TRANCE_GAUGE_END_PENALTY 200  /* 非死亡结束再 −200（`th13:0x405cdd`，夹到 0）*/
#define TRANCE_GAUGE_MIN_ENTER  200   /* `begin` 的硬门：槽 < 200 直接失败并返回 −1（`th13:0x405a40`）*/
#define TRANCE_COOLDOWN_FRAMES  30    /* 激活冷却（`th13:0x405842`：this+0x20 >= 0x1e）*/

#define TRANCE_CHAIN_WINDOW     60    /* 连锁窗口 60 帧（`th13:0x4391a0`：+0x8828 = 60，每帧递减）*/
/* ★ 阈值 = **第 10 颗**（不是第 11 颗）。一手 `th13:0x4391a0` 尾段逐条：
 *     `this+0x8828 = 60;  this+0x8838++ ;  if (this+0x8838 > 9) spawn_spirit(this, 3, pos, 0.5, 1);`
 *   ⇒ 计数由 1 涨到 **10** 的那一颗（= 窗口内第 10 颗）当场额外送 1 颗 type 3；
 *     那颗额外的 type 3 落到场上就是「第 11 颗」（`SPIRIT:222` 的同一句话，两个说法是一件事）。
 *   ⚠ 2026-09-12 订正：旧实现写 `count > TRANCE_CHAIN_THRESHOLD`（= 第 11 颗才触发）⇒ 比一手**晚一颗**，
 *     已改成 `>=`（见 `trance_chain_note`）；本常量继续是 10，单测钉住「第 10 颗触发」。*/
#define TRANCE_CHAIN_THRESHOLD  10    /* 第 10 颗触发（一手：`if (+0x8838 > 9)` ≡ `count >= 10`）*/
#define TRANCE_CHAIN_BONUS_TYPE 3     /* 连锁奖励的神灵类型（`spawn_spirit(this, 3, …)`）*/
#define TRANCE_CHAIN_STEP       1     /* 每颗非 type3/4 的神灵让连锁 +1 */

/* 击破 → 神灵的**帧内队列**（`on_enemy_death` 是清场帧一次 100–300 次的高频事件，
 * `DEATH` §1.7/§4.1）⇒ 回调里只 O(1) 入队、帧末一次性取走。
 * ★ 容量 **16**（不是 64）：队列里每格要存 x/y 两个 float（8 B）⇒ 64 格 = 512 B，
 *   加上别的字段会让 `sizeof(trance_card_t)` **超过 `ce_state` 的 496 字节硬上限**
 *   （实测 600 B ⇒ `_Static_assert` 直接红，`ce_state_user` 会**静默返回 NULL**）。
 *   16 格 = 128 B 位置 + 其余字段，稳在限内；溢出**丢尾**（不丢头：先入队的是先死的、
 *   位置更有用），丢掉的颗数记在 `trance_kill_queue_dropped`。❓ 实跑若发现漏撒就调这个值
 *   （连同 `trance_card_t` 的尺寸一起看断言）。*/
#define TRANCE_KILL_QUEUE_MAX  16

/* 每帧 −1/帧 的消耗 ⇒ 满槽 600 帧 = 10.00 s（HUD 的 `%.2fs` 就是它，`SPIRIT` §6.3）*/
#define TRANCE_DURATION_FRAMES_MAX (TRANCE_GAUGE_MAX / TRANCE_GAUGE_DRAIN)

/* 早苗特例（`SPIRIT:86`）：CHARACTER == 2 收 type 1 时**额外 +1 槽**。
 * ⚠ SPEC §9 #8 列为 ⏳ 待拍 ⇒ **默认关**；要抄就把这个宏改成 1 并在卡侧传 `is_sanae`。*/
#ifndef TRANCE_ENABLE_SANAE_TYPE1_BONUS
#define TRANCE_ENABLE_SANAE_TYPE1_BONUS 0
#endif
#define TRANCE_GAUGE_SANAE_TYPE1  1

/* ============ 奖励数值（**规格未定的那部分全部做成参数**，不许在纯逻辑里写死）============
 *
 * 两条单位链**必须分开**（这是 SPEC §9 #5 未定项的直接后果，别把它们合成一个常量）：
 *
 * 1. `max_point` = **显示单位**的增量（type 1 普通 +10 / 灵界 +100，`SPIRIT:75`）。
 *    卡侧怎么写引擎字段由它自己决定，见 `trance_max_point_to_field_addends()`。
 * 2. `score_popup` = **弹窗显示值**（type 3）。上游必须给**原始最大得点定点值**
 *    （TH13 的 `CURRENT_PIV` = `th13:0x4be7dc`；TH18 的对应物 = `0x4ccd24`）
 *    —— 因为一手的算法是 `popup = (PIV / 100) / 10`（见 `trance_type3_popup_from_piv()` 的注释）。
 */
typedef struct {
    int32_t life_frag;   /* 命碎增量（普通 1 / 灵界 2，`SPIRIT:73-87`）*/
    int32_t max_point;   /* **显示单位**的最大得点增量（普通 10 / 灵界 100）*/
    int32_t spell_frag;  /* 符卡碎增量（普通 1 / 灵界 2）*/
    int32_t gauge;       /* 槽增量：**收神灵的统一 +2**（灵界中**不加**，`SPIRIT:326`）*/
    int32_t gauge_type3; /* type 3 额外 +1 槽（灵界中**不加**）*/
} trance_reward_t;

/* 收神灵**所有类型**的统一槽增量（`th13:0x438b3d`）；灵界中 = 0（`SPIRIT:326`「收神灵不再加槽」）*/
#define TRANCE_REWARD_GAUGE_ALWAYS  TRANCE_GAUGE_PER_SPIRIT
#define TRANCE_REWARD_GAUGE_TRANCE  0

/* ============================ 相位 / 状态机 ============================ */

#define TRANCE_PHASE_IDLE         0   /* 不在灵界 */
#define TRANCE_PHASE_MANUAL       1   /* 手动/自动激活的灵界（`begin(param_1 = 0)`）⇒ 结束要 −200 */
#define TRANCE_PHASE_DEATH_FREE   2   /* **死亡强制**的灵界（`begin(param_1 = 1)`）⇒ 结束**不**补扣 200 */
/* ⚠ 相位 2 的语义再强调一次：这是「死亡**已经**发生、我们只是先开窗口」——
 *   它**不是**免死层（免死 = 救援层 `+0x10`，那是 107/117 的地盘，SPEC §7 禁止越界）。*/

/* ============================ 槽 ============================ */

typedef struct {
    int32_t gauge;   /* 0..600 */
    int32_t tier;    /* 0..3（用于「跨格」反馈：音效 `0x44` + 自机上方 32px 的效果 `0x6b+格数`）*/
} trance_gauge_t;

typedef struct {
    int32_t gauge;        /* 本帧结束后的槽值 */
    int32_t tier;         /* 本帧结束后的格 */
    int32_t crossed_tier; /* 1 = 本帧跨了格（调用方播升级音效，`th13:0x43d530`）*/
    int32_t delta;        /* 实际加进去的量（被上限夹过 ⇒ 可能 < 请求值）*/
} trance_gauge_add_t;

typedef struct {
    int32_t gauge; /* 本帧结束后的槽值 */
    int32_t floor; /* 1 = 本帧归零（调用方收尾灵界）*/
} trance_gauge_tick_t;

void trance_gauge_reset(trance_gauge_t *g);                        /* 关卡开始写 0（`th13:0x42bbfd`）*/
void trance_gauge_set(trance_gauge_t *g, int32_t v);               /* 夹 [0,600] 并重算格 */
trance_gauge_add_t trance_gauge_add(trance_gauge_t *g, int32_t delta);
trance_gauge_add_t trance_gauge_sub(trance_gauge_t *g, int32_t delta);  /* 非死亡结束的 −200 */
trance_gauge_tick_t trance_gauge_drain(trance_gauge_t *g);         /* 灵界中每帧调（−1/帧）*/
int32_t trance_gauge_tier(int32_t gauge);                          /* 0..3（≥600 ⇒ 3）*/

/* ============================ 灵界窗口状态机 ============================ */

typedef struct {
    trance_gauge_t gauge;
    int32_t phase;      /* TRANCE_PHASE_* */
    int32_t cooldown;   /* 激活冷却计数器：≥ TRANCE_COOLDOWN_FRAMES 才允许激活 */
    int32_t ended_death_free; /* 1 = 刚结束的是**死亡强制**的灵界（诊断 / 日志用）*/

    /* 连锁（`SPIRIT` §3.6）*/
    int32_t chain_count;   /* 窗口内的连锁计数（`th13:manager+0x8838`）*/
    int32_t chain_timer;   /* 窗口剩余帧（`+0x8828`）*/

    /* 神灵收账（**按 type 重建**，不是「蓝 / 白」二分，SPEC §2.2）*/
    int32_t spirits_seen[TRANCE_TYPE_COUNT];   /* 各 type 见过的颗数 */
    int32_t chain_bonus_spawned;               /* 连锁奖励撒出去的 type 3 颗数 */

    /* 击破神灵队列（卡侧从 on_enemy_death 记账、帧末撒）
     * ★ 只存**标量快照**：`on_enemy_death` 的 `ev->enemy` 指针同帧就会被 sweep 删掉
     *   （`ce_enemy_death_t.enemy` 的一手注释：**禁止缓存**）⇒ 队列里绝不放指针，
     *   位置只抄 `ev->x / ev->y` 两个 float 值。 */
    int32_t kill_pending;      /* 待撒颗数 */
    int32_t kill_last_frame;   /* 最后一次记账的帧（诊断）*/
    float   kill_x[TRANCE_KILL_QUEUE_MAX];   /* 各待撒神灵的出生位置（敌人世界坐标快照）*/
    float   kill_y[TRANCE_KILL_QUEUE_MAX];

    /* 诊断 */
    int32_t activations;       /* 本局开过几次灵界 */
    int32_t deaths_forced;     /* 死亡强制开了几次 */
} trance_state_t;


typedef struct {
    int32_t phase;         /* 本帧结束后的相位 */
    int32_t started;       /* 1 = 本帧进入了灵界（本文件不自己进，留给调用方 ⇒ 恒 0）*/
    int32_t ended;         /* 1 = 本帧结束了灵界 */
    int32_t ended_death_free; /* 1 = 刚结束的是死亡强制灵界（不补扣 200）*/
    int32_t drain_floor;   /* 1 = 槽本帧归零（灵界中槽本来就是 0 时也报 —— 幂等）*/
} trance_tick_t;

typedef struct {
    int32_t full;       /* 槽 >= 600（`th13:0x405815` 的硬门：**必须满槽**）*/
    int32_t enemies;    /* 场上有敌（`th13:0x40582a`）*/
    int32_t cooldown;   /* 冷却已过（`th13:0x405842`）*/
    int32_t dialogue;   /* 1 = 正在对话（`th13:0x40584d` ⇒ 不满足）*/
    int32_t bombing;    /* 1 = 正在放 B（`th13:0x405806` ⇒ 不满足）*/
    int32_t in_trance;  /* 1 = 已在灵界（`th13:0x4057fa` ⇒ 不满足）*/
    int32_t key;        /* 1 = 激活键（TH13 是输入位 `0x800`，`th13:0x405833`）*/
    int32_t auto_mode;  /* 1 = 「满槽自动进」兜底（SPEC §9 #1 的候选口径）：**忽略 key 那一条** */
    /* ★ **第 8 条**（`th13:0x405859`）：`(GAME_FLAGS & 0x30) != 0x20` 才允许激活。
     *   TH13 的 `GAME_FLAGS` = `th13:0x4be830`；**TH18 侧的同一个全局一手定位 = `th18:0x4cccc8`**
     *   （证据：全 exe 只有它在 `test byte ptr […], 0x30` 这一形状下被读 9 次 —— 卡侧 `trance_game_flags_ok()`
     *    的注释里逐条列了站址；该全局的位用法与 TH13 逐位同形：位 0/1/2/3/6 都是 `or`/`and` 写入，
     *    位 4–5 构成一个 2 bit 的状态域 `(flags >> 4) & 3`）。
     *   调用方从引擎面读出来填：1 = 通过（≠ 0x20）、0 = 不通过。**纯逻辑层不认识地址**（这是本文件的分工）。*/
    int32_t game_flags_ok;
} trance_cond_t;

/* 全 8 条（`th13:0x4057f0`，逐条见 `trance_cond_t` 的字段表）；`key` 那条由 `auto_mode` 兜底
 * （SPEC §9 #1：TH18 没有免费键位）⇒ **字段 9 个 / spec 的条件 8 条**（key 与 auto_mode 是同一条的两种口径）。
 * 返回 1 = 可激活。**纯判定**，不改状态。*/
int trance_can_activate(const trance_state_t *s, const trance_cond_t *c);

/* 进入（`TranceManager::begin` `th13:0x405a20`）。`death_free` = 死亡强制那条路（`param_1 = 1`）。
 * 规则（一手）：已在灵界 = 视为成功返回 1（幂等）；槽 < 200 = **失败返回 0**。
 * ★ 传入的是 trance_cond_t 而不是「逐条 bool」：调用方（卡侧）负责把引擎面读成条件。*/
int trance_begin(trance_state_t *s, int death_free, const trance_cond_t *c);

/* 结束（`TranceManager::end` `th13:0x405b80`）。
 * ★ 死亡强制的灵界结束**不补扣 200**，也**不**「再调一次 Player::die」——
 *   用户定案 ② 是「死后照死、只是先开灵界」⇒ 我们这条路上什么都不做（SPEC §2.1 行 4）。*/
void trance_end(trance_state_t *s, trance_tick_t *out);

/* 每帧推进：相位 0 ⇒ 只推冷却与连锁窗口；相位 ≠ 0 ⇒ 槽 −1/帧、归零即收尾。
 * 返回出的 out 见 trance_tick_t。**调用方**负责：满槽自动进 / 键 / 死亡强制。*/
trance_tick_t trance_tick(trance_state_t *s, int32_t frame);

/* 死亡已确定时（虚表 **`+0x14`** `on_death_frame2`）的强制开灵界。
 * 返回 1 = 本帧开了窗口（调用方起背景 / 播音）；0 = 没开（已在灵界 / 还没满槽）。
 * ⚠ 这里**不消耗生命、不取消死亡、不读自机坐标**（`+0x14` 的现场是 respawn 之后，
 *   `SPEC §3.5` / `animal.c:1777`）⇒ 本函数连坐标参数都没有。*/
int trance_force_on_death(trance_state_t *s);

/* 连锁记账（`spawn_spirit` 尾部，`th13:0x4391a0`）。`type` 是刚撒出来的神灵类型。
 * 返回 1 = **本次**应当额外再撒一颗 type 3（一手「第 10 颗触发」⇒ 那份额外的 type 3 就是场上的第 11 颗）。
 * 规则：type 3 / 4 自身不推进连锁（一手：`if (type != 3 && type != 4)`）。*/
int trance_chain_note(trance_state_t *s, int32_t type);

/* 连锁窗口每帧递减（`SPIRIT` §3.6：`+0x8828` 每帧 −1，归零则计数清 0）。
 * `frame` 单调游玩帧。返回 1 = 本帧窗口归零（计数已清）。*/
int trance_chain_tick(trance_state_t *s, int32_t frame);

/* ============================ 四类神灵：槽账 + 奖励 ============================ */

typedef struct {
    int32_t gauge_add;     /* 该 type 带来的槽增量（含统一 +2）*/
    int32_t tier_crossed;  /* 1 = 跨格 */
    int32_t chain_extra;   /* 1 = 该颗触发了连锁奖励（调用方要再撒一颗 type 3）*/
} trance_collect_t;

/* 收一颗神灵（`SpiritItem__on_tick` 的收集分支 `th13:0x438730`）。
 * 1. 槽：**所有类型先 +2**（灵界中不加）；type 3 再 +1（灵界中不加）。
 * 2. 连锁：非 3/4 才推进（返回的 chain_extra 就是「第 11 颗」）。
 * 3. 奖励：见 `trance_reward()`。★ `type` 允许 0..4（4 = item 14 的别名，按 type 1 发奖，
 *    一手 `SPIRIT:75` 注：type 4 在 `0x438ed8` 被改写成 1）。越界返回全 0 的 out 并返回 0。
 */
int trance_collect(trance_state_t *s, int32_t type, trance_collect_t *out);

/* 该 type 在当前相位下的奖励（SPEC §4 表 + `SPIRIT:73-87`）。
 * `in_trance` = 相位 ≠ 0。越界 type ⇒ 全 0。`is_sanae` 只在 `TRANCE_ENABLE_SANAE_TYPE1_BONUS`
 * 为 1 时有效（默认关：SPEC §9 #8 待拍）。*/
trance_reward_t trance_reward(int32_t type, int in_trance, int is_sanae);

/* 把奖励里的「显示单位最大得点」换算成**要写进引擎字段的加数**。
 * `field_unit_per_display` = 引擎字段 1 个显示点占多少定点单位（TH13 = 100）。
 * ★ TH18 的 `0x4ccd24` 同样是 ×100 定点（一手：`max_point = (MONEY + 1) × K × 100`，
 *   K = EASY 500 / 其余 1000，见 .c 的 `trance_max_point_field_addends` 注释），
 *   所以卡侧传 100；传错单位**不会崩**，只会让 ±10 变成 ±1000。
 * ⚠ 这是 SPEC §9 #5「奖励载体」未定项的一部分 ⇒ 我们**把它做成参数**，不写死。*/
int32_t trance_max_point_field_addends(int32_t display_delta, int32_t field_unit_per_display);

/* type 3 的弹窗分值（`th13:0x438c3d`–`0x438c9d` 一手汇编）：
 *     eax = 0x51eb851f; imul PIV; sar edx,5; (补符号) ⇒ edx = PIV / 100
 *     esi = 10; idiv esi ⇒ 商 = 再 /10，**余数 = 落地到 10 的倍数**
 *   （`SPIRIT:84`：「分数 = PIV/100 再向下取整到 10 的倍数；实际入账 值/10」）
 * `piv` 必须是**最大得点的原始定点值**（TH13 `th13:0x4be7dc` / TH18 `0x4ccd24`）。
 * 负数按 0 处理（`piv < 0` ⇒ 返回 0）。*/
int32_t trance_type3_popup_from_piv(int32_t piv);

/* 该 type 的 anm 脚本号（`astral.anm`；一手 `th13:0x43907e`：`script = type*6 + 1`（普通）/
 * `type*6 + 4`（生成时处于灵界））。`in_trance` = 生成那一刻在不在灵界。
 * ⚠ TH18 侧 `astral.anm` **还没有切图 / 入 `ability.anm`**（SPEC §5）⇒ 卡侧目前不用它。*/
int32_t trance_anm_script(int32_t type, int in_trance);

/* ============================ 救援层记账（107 → 117，一次中弹全烧）============================
 *
 * 用户 2026-09-12 定案（`EC` §5.2）：中弹时**当前所有正在生效的救援型状态各自转成一次
 * 「救援爆发」**（不是「从池里挑一个花掉」），**动作顺序 107 → 117**，且**全屏消弹只发生一次**
 * （由先执行且带消弹效果的那张做 ⇒ **107 拿这笔账**，`EC` §5.2 细则 2）。
 *
 * ★ 113 **不在**这一层（SPEC 用户定案 ③：113 是事后层 `+0x14`）⇒ `trance_rescue_resolve()`
 *   里对 113 有**硬拒**（返回 0），这是「分层不许越界」的可执行形式（SPEC §7 第一条）。
 *
 * ★ 定位（2026-09-12 订正，别再当成「待接线」）：本函数是**契约的可执行形式 + 回归断言**，
 *   **不是**要 107 / 117 在各自 `+0x10` 里回调的 TODO。四条细则都由**已有共享层**落地、卡间零互调：
 *     ① 两家各自在自己的 `+0x10` 里动作 + 调 `Player__cancel_impending_death` `0x45d590`
 *        （返回值恒 0 且 `Player__die` 忽略它 ⇒ 「先到先得」在执行层不存在，这正是定案）——
 *        107 = `cards/senro.c` 的 `senro_do_libyou(..., by_hit=1)`；117 = `cards/animal.c`。
 *     ② 全屏消弹只一次 ⇒ `ce_cancel_latch_claim`（先认领者做，后到者记日志即退）。
 *     ③ 无敌取 max ⇒ `ce_invuln_request`（`invuln_recompute` 取 max，不求和、不截短）。
 *     ④ 决死 B 拿不到机会 ⇒ `Player__die` 先置 state 4、最后才广播 `+0x10`（技术事实，无需动作）。
 *   113 的分层靠**结构性保证**（`cards/trance.c` 里不出现 `cancel_impending_death`），不靠本函数拒绝。
 */
#define TRANCE_RESCUE_COUNT   2
#define TRANCE_RESCUE_107     107   /* 森罗结界（`cards/senro.c`，2026-09-12 已实装：`senro_do_libyou`）*/
#define TRANCE_RESCUE_117     117   /* 鬼形兽咆哮（`cards/animal.c`）*/

/* 契约顺序：**107 → 117**（用户定案，不许改）。`trance_rescue_order()` 返回它，单测钉死。*/
const int32_t *trance_rescue_order(void);

typedef struct {
    int32_t id;          /* 107 / 117 */
    int32_t was_active;  /* 该家在本次中弹时是否正在生效 */
    int32_t consumed;    /* 1 = 被烧掉 */
    int32_t invuln;      /* 该家给的霊撃无敌帧（107 ≈ 40（0.67 s）· 117 = 40；`EC` §5.2 细则 3）*/
    int32_t cancel_all;  /* 1 = 该家负责全屏消弹 */
    int32_t burst;       /* 1 = 该家产生一次「救援爆发」*/
} trance_rescue_slot_t;

typedef struct {
    trance_rescue_slot_t slots[TRANCE_RESCUE_COUNT];  /* 按 trance_rescue_order() 排好 */
    int32_t any;             /* 1 = 至少烧掉一家*/
    int32_t death_cancelled; /* 1 = 死亡被取消（⇒ 113 的 +0x14 **不会**跑；调用方用它决定跳过）*/
    int32_t invuln_frames;   /* 汇总无敌帧：**取 max，不求和**（`EC` §5.2 细则 3；118/109 同层）*/
    int32_t cancel_taken;    /* 拿到全屏消弹账的那家 id；-1 = 没人认领*/
} trance_rescue_report_t;

/* 谁的霊撃给多少无敌帧（`EC` §5.2 细则 3：107 = 0.67 s ≈ 40 帧、117 = 40 帧）。
 * 两家都在这层里 ⇒ 汇总取 max 恰好 = 40，不叠加也不互相截短。*/
int32_t trance_rescue_invuln_frames(int32_t id);

/* 结算一次中弹（`EC` §5.2）。`active_mask` 的 bit i 对应 trance_rescue_order()[i] 那一张卡
 * （`active_mask` bit0 = 107、bit1 = 117）。`card_id` = 正在问的那张卡：
 *   · 传 113 ⇒ **立刻返回 0**（分层越界，见上）；
 *   · 传别的 id ⇒ 照样按契约顺序结算（本函数是「契约的可执行版本」，让 107/117 也能复用）。
 * 返回 1 = 结算完成（report 已填），0 = 入参非法 / 113 越界。*/
int trance_rescue_resolve(int32_t active_mask, int32_t card_id, trance_rescue_report_t *out);

/* ============================ 击破 → 神灵（记账层）============================
 *
 * SPEC §3.3 候选 ①（击破产神灵，`DEATH:263` 预定用 `on_enemy_death` 的 `kind == KILLED`）。
 * ✅ **已接线**（2026-09-12 实装期订正）：`on_enemy_death` 是 `ce_hooks_t` 的**事件钩子**，
 *   由 `sdk.c` 的 `BP_ce_enemy_death`（断点 `0x430240`）沿卡链表**直接** `h->on_enemy_death(card,&ev)` 派发
 *   （`sdk/public.h` 的 `ce_enemy_death_t` 注释），**不经过 21 槽虚表** ⇒ `CE_CARD` 里没有它的桩是**正常的**
 *   （与 `on_item_drop_pre` / `on_item_score` 同一类）。先前「接线阻塞」的结论是把事件钩子当成了虚表槽，**已作废**。
 *   ⇒ 卡侧 `.on_enemy_death = …` 直接生效；本文件只提供**帧内队列**：卡侧拿到死亡事件就
 *     `trance_kill_note()`，帧末 `trance_kill_take()` 一次性取走（`DEATH` §4.1 对 109 的 O(n²) 警告的同一形状）。
 *
 * 过滤规则由**调用方**做（它拿得到 `ce_enemy_death_t`），本层只认「记账 / 取走」：
 *   卡侧应当只对 `kind == CE_EDEATH_KILLED`（`retaddr == 0x42f3b7`）**且**
 *   `flags & CE_EDEATH_F_REAL` 的敌人记账（排除 `KILL_ALL` —— `DEATH:263` / `DEATH` §1.3）。
 */
/* 「击破掉下来的那颗是什么类型 / 掉几颗」——**来源是 SPEC 的条款，不是猜的**：
 *   · type：SPEC §2.2 第 2 条一手订正「**击破**走的是通道 B 的 `spec1(N)`/`spec2(N)` 计时窗口」
 *     ⇒ 通道 B 的产物 = **type 1**（`SPIRIT` §3 通道表 B 行一手：`type=1, speed=0.5, sound=1`；
 *     通道 A 累计伤害也是 type 1）。⚠ 骨架的「击破产蓝灵（分）」是错的，别按「蓝/白」二分写。
 *   · 颗数：TH13 由 ECL `spec1`（计时窗口）/ `spec2`（上限）逐敌决定（`n = (max−timer)/(max/div)+1`，
 *     最快击杀 ≈ `spec2+1`，H 以上再 +1，`SPIRIT:153-167`）⇒ **TH18 侧的对应物未查**（SPEC §9 #4）
 *     ⇒ 先取 1 颗/击破，标 ❓ 自定。*/
#define TRANCE_KILL_SPIRIT_TYPE        1   /* SPEC §2.2：击破 = 通道 B ⇒ type 1（最大得点）*/
#define TRANCE_KILL_SPIRITS_PER_KILL   1   /* ❓ 自定：TH13 的颗数来自 ECL spec1/spec2，TH18 无对应（§9 #4）*/
/* 队列溢出丢掉的颗数（诊断）。定义在 `trance_core.c`；**只增不减**、只进日志 ⇒ 卡侧不必清。
 * ⚠ 必须是 `extern` 声明：写成 `int32_t x;` 会让每个包含本头的翻译单元都留一个 tentative
 *    definition ⇒ 主机单测（core + test 两个 .o）链接时直接 `multiple definition` 报错。*/
extern int32_t trance_kill_queue_dropped;

/* 记一颗待撒（x/y = **敌人世界坐标快照**；超 TRANCE_KILL_QUEUE_MAX 丢弃并 ++trance_kill_queue_dropped）。
 * ★ **O(1)**：一次数组写 + 计数 ++。清场帧（killAll）会被调 100–300 次（`DEATH` §1.7），
 *   所以回调里**绝不做**扫全场 / 分配内存 / 建 anm VM（那些留给帧末的 take）。
 * ⚠ 回调里**禁止析构敌人、禁止缓存 `ev->enemy`**（`DEATH` §2.4）—— 本函数只吃标量。*/
void trance_kill_note(trance_state_t *s, int32_t frame, float x, float y);
/* 取走全部待撒并清零。返回颗数；`cap > 0` 时把位置按入队顺序抄进 out_x/out_y
 * （out_x 传 NULL = 不要位置，可以只数颗数）。 */
int32_t trance_kill_take(trance_state_t *s, int32_t frame, float *out_x, float *out_y, int32_t cap);

/* ============================ ★ 神灵实体（自建 VM 池）的策略 ============================
 *
 * **用户 2026-09-12 裁决（binding，逐条实现）**：
 *   「113 神灵实体 ≈ P 点、蓝点等**下落道具**，但是：**1、不会下落；2、不会被全屏收集收集，
 *     必须跟过去吃；3、会随着时间不断变小而消失；它不是敌人。**」
 * ⇒ 形状 = **自建 VM 池的道具样实体**：既**不是敌人**（不建敌人、不进 `EnemyManager`），
 *   也**不是引擎道具**（不进 `ItemManager` 的道具池 ⇒ 原生磁吸 / 回收线 / `ce_item_collect_all()`
 *   全屏收点**天然看不见它** —— 这正是裁决第 2 条要的；卡侧 `cards/trance.c` 有同一句声明）。
 *
 * 本段只放**纯策略**（全整数、无引擎地址、无浮点）：容量 / 缩小曲线 / 到期判定 / 吃取半径 /
 * 入槽值换算 / 原地微动。**建 VM、写坐标与 scale、收尾**全在卡侧（`cards/trance.c`）。
 *
 * ⚠ 为什么策略放这里而不是卡里：能算错的正是这部分（曲线端点、半径边界、越界钳位），
 *   卡侧只剩「把整数喂给引擎」的机械动作 ⇒ 主机单测 `tests/test_trance_core.c` 能全覆盖。 */

/* 池容量。一手：TH13 的神灵池 = **256** 槽（`SPIRIT` §2.1 `th13:<mgr>+0x14 items[256]`，stride 0x88）。
 * ❓ 自定 = **128**：我们不需要 256（那是 TH13 六条生成通道共用的池），128 × 32 B = 4 KB 静态区，
 * 且「128 槽 ÷ 480 帧寿命」= 每帧 0.27 颗的持续上限（≈16 颗/s）；比这更密的击破会**丢尾**并记
 * `trance_spirit_dropped`（诊断）。实跑首日若看见丢，先调这里（不是调寿命）。*/
#define TRANCE_SPIRIT_POOL              128

/* 寿命。一手：TH13 神灵 `timer_1 == 480` 帧时收（`th13:0x438a61`：`CMP [EBX+0x60],0x1e0`）
 * ⇒ 到点调 `anm_set_interrupt_by_id(vm, 1)` 让它播完自己的消失段再删。**照抄 480**。
 * ⚠ 本卡的神灵**不下落、不出界**（裁决 1）⇒ 寿命是它唯一「离开场上」的路（裁决 3）。*/
#define TRANCE_SPIRIT_LIFE_FRAMES       480

/* 缩小曲线（裁决 3）。**一手**：TH13 `astral.anm` script1 的三句（`SPIRIT` §2.3 一手字节）：
 *     -1:  `scale(1.5, 1.5)`
 *      0:  `scriptNew(2) / scriptNew(3)`          ← 外环 + 内芯（子脚本）
 *   +200:  `scaleTime(200, 0, 0.4, 0.4)`           ← 从 1.5 线性缩到 0.4、时长 200 帧
 * ⇒ **出生 1.5 → 200 帧线性缩到 0.4 → 之后保持 0.4 直到 480 帧到期**。
 * 单位用**千分点**（1000 = 缩放 1.0 ⇒ ×1.5 = 1500）⇒ 全整数，不进浮点。
 * ⚠ 原版这条路是 VM 里的 `scaleTime` 插值器在跑；我们的载体脚本主路径是 `stop()`（见卡侧注释）
 *   ⇒ 由卡每帧写 `ce_anm_set_scale`，曲线数值照抄一手。*/
#define TRANCE_SPIRIT_SCALE_START_PERMILLE  1500   /* 一手：`-1: scale(1.5,1.5)` */
#define TRANCE_SPIRIT_SCALE_END_PERMILLE     400   /* 一手：`+200: scaleTime(200, 0, 0.4, 0.4)` */
#define TRANCE_SPIRIT_SCALE_FRAMES           200   /* 一手：同句的插值时长 200 帧 */

/* 吃取半径（裁决 2「必须跟过去吃」）。**不要**照抄引擎道具的收点几何
 * （`player+0x4798c` 初值 30 px / 黄线 70 px，见 `faith_core.h` 那两条）：
 *   一手 TH13 的神灵**收集半径 = 20 px**（`th13:0x438b17`：`dist² < 400`，`double 400.0 @ 0x4aee10`）。
 * ❓ 自定 = **24 px**：原作那颗会**主动飞向自机**（96 px 吸引 / 128 px 脱离 / 每帧 +0.3 上限 12.0），
 *   我们这颗**停在原地**（裁决 1）⇒ 严格照抄 20 px 会「明明走过去却擦肩」；放宽到 24 px 仍远小于
 *   70 px 黄线，且**必须自机走到它旁边**。实跑调参点即此值（手感紧了往 28 调，别超过 30）。*/
#define TRANCE_SPIRIT_PICK_RADIUS            24

/* 原地微动（裁决 1「不下落」；「要不要原地轻微浮动」由我们定）。
 * ❓ 自定：±1 px 的**方波**（不是正弦 —— 免得引浮点 / libm），周期 32 帧（≈0.53 s）。
 * 作用只是让一堆静止的实体看起来还「活着」；**不参与吃取判定**（判定用出生点 `x/y`）。*/
#define TRANCE_SPIRIT_BOB_PX                 1
#define TRANCE_SPIRIT_BOB_PERIOD            32

/* 缩小曲线：返回**千分点**（1000 = 1.0）。age = 已活帧数（0 = 出生那一刻）。
 * age ≤ 0 ⇒ 1500；age ≥ 200 ⇒ 400；中间线性（整数除、向零截断 ⇒ 单调不增）。*/
int32_t trance_spirit_scale_permille(int32_t age);

/* 原地微动偏移（**整数像素**，∈ {−1, 0, +1}）。age < 0 ⇒ 0。*/
int32_t trance_spirit_bob_offset(int32_t age);

/* 到期判定（裁决 3 的「到点消失」）：剩余生命 ≤ 0 ⇒ 1（卡侧播消失段并回收槽）。*/
int trance_spirit_expired(int32_t life);

/* 吃取判定（裁决 2）：dx/dy = 「自机 − 神灵」的**整数像素**位移（卡侧取整；1 px 误差对 24 px 半径无意义）。
 * 用整数平方比较（core 里一个浮点都不用 ⇒ 主机单测与 i686 目标都没有 x87 风险）。
 * 返回 1 = 在半径内（**边界含**：dist² ≤ r²）。*/
int trance_spirit_in_pickup_range(int32_t dx, int32_t dy);

/* 池剩余容量（used = 当前活着的颗数）。used 钳到 [0, POOL]（< 0 当 0、≥ POOL 当满）
 * ⇒ 返回 [0, TRANCE_SPIRIT_POOL]。卡侧用它决定「撒 or 丢并计数」。*/
int32_t trance_spirit_pool_room(int32_t used);

/* 收一颗该 type 的神灵**总共**给多少槽（= `trance_reward()` 的 `gauge + gauge_type3`）：
 * 普通 2（type 3 = 3）、灵界中 0（`SPIRIT:326`：灵界里收神灵不再加槽）；越界 type ⇒ 0。
 * ★ 真实入账**仍然走 `trance_collect()`**（它还要推连锁、记账）；本函数只给「这一颗值多少槽」，
 *   供卡侧对账 / 日志与单测用（不重复实现扣加逻辑）。*/
int32_t trance_spirit_gauge_value(int32_t type, int in_trance);

/* ============================ 自检 ============================ */

/* 编译期不变式（放进卡侧或单测里都行；纯表达式、无副作用）。返回 0 = 全部成立。*/
int trance_core_selfcheck(void);
