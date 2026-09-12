/* pofv_core.h —— 花映塚（TH09 系统，card-expand id 109）的**纯逻辑**：
 * 白弹半径阈值分类 / 气槽 4 级与蓄力状态机 / CA 与 QCA 的等级与代价 /
 * 爆风（蓝圈）与 CA（红圈）的半径与消弹记账 / SP 三档 / 活化灵状态机 / 爆风队列。
 * 纯 C11、**不含任何引擎地址**（引擎侧调用点全在 pofv.c）；主机单测 `tests/test_pofv_core.c`。
 *
 * 设计依据：spec [`docs/superpowers/specs/2026-09-12-pofv-109-design.md`]（2026-09-12）。
 * 机制记录（用户口径，一手抄录）：`mods/th18.v1.00a/card-expand/IDEAS.md` §「109 花映塚」。
 * 体检结论：`docs/superpowers/specs/2026-09-11-system-card-spec-review.md` §3.2 / §5.4 / §11 #11/#47。
 *
 * ★ 数值纪律（本文件每个常量两类之一，别混）：
 *   · **一手锚点**：出处写在注释里（地址 / 文档节），**不许改**；
 *   · **❓ 自定占位**：原版（th09）数值**尚未逆到**，用具名常量占位 + `TODO(th09)`，
 *     spec 的「数值表」里逐条标 `❓ 自定`。占位值只保证量纲与单调性，不代表原版。
 *
 * ⚠️ float 一律**走出参**、不返回（i386 的 float 返回值走 x87 ST(0)，会破坏 `make dllx87`
 * 的「零 x87」不变式 —— 同 `animal_core.h` 的 `animal_hyper_radius`）。
 */
#pragma once
#include <stdint.h>

/* ==================================================================== A. 白弹阈值 */

/* ★ 一手（review §5.4）：**白弹 = 半径 ≤ 4.0**。
 *   半径字段 = `zBullet + 0x658`；出生时从弹种表 `0x4c5f90`（stride `0x118`）的 `[+0]`
 *   （= `0x4c6094`）拷入。贴图尺寸 ≈ 4 × 半径，故 8×8/14×14/16×16 等都是白弹，
 *   30×30 起（半径 ≥ 5.0）= 不可消。⇒ **一个常量搞定，不需要白名单**（§11 #4 的旧方案作废）。 */
#define POFV_WHITE_R_MAX        4.0f

/* 半径 0.0 的 7 个哨兵弹种（51 / 53 / 55–59）不是白弹也不是不可消弹，是空弹种（review §5.4）。*/
#define POFV_R_SENTINEL         0.0f

typedef enum {
    POFV_BULLET_SENTINEL    = 0,   /* r <= 0：哨兵弹种（不参与任何消弹判定）*/
    POFV_BULLET_WHITE       = 1,   /* 0 < r <= 4.0：**可消**（蓝圈 / 红圈都吃）*/
    POFV_BULLET_UNBREAKABLE = 2    /* r > 4.0：蓝圈消不掉，**只有红圈（CA）能消** */
} pofv_bullet_class_t;

pofv_bullet_class_t pofv_classify_bullet_radius(float r);
int  pofv_is_white_bullet(float r);

/* 自扫弹池时的计数判据（蓝圈只能消白弹，而引擎**没有带 exclude 的半径消弹接口** ⇒ 自己扫池，
 * 逐弹判半径、逐弹调 `Bullet__cancel`）。radii = 每颗候选弹的 `+0x658`。返回白弹颗数。 */
int  pofv_scan_count_white(const float *radii, int n);

/* ==================================================================== B. 气槽 / 蓄力 / CA */

/* ★ 一手（IDEAS 109「蓄力 / 气槽」）：气槽**共 4 级**。
 * ★ 一手（IDEAS 109 的 C1–C4 表）：CA **等级 L 需要 L 格气**（C1 要 1 格），
 *   **消耗 L−1 格**（C1 不消耗 / C2 1 格 / C3 2 格 / C4 3 格）⇒ 放完永远至少剩 1 格。 */
#define POFV_GAUGE_MAX          4
#define POFV_CA_LEVEL_MIN       1
#define POFV_CA_LEVEL_MAX       POFV_GAUGE_MAX

/* ❓ 自定（TODO(th09)：原版「按住射击多久升一级」的帧数未逆到）。
 * 占位语义：每按住这么多帧升一级；`a`（当前等级）上限 = `b`（当前可蓄最高等级 = min(气槽, 4)）。 */
#define POFV_CHARGE_FRAMES_PER_LEVEL 45

typedef enum {
    POFV_CHARGE_IDLE = 0,          /* 没在蓄力（此时 Z 走原生行循环）*/
    POFV_CHARGE_HOLD = 1           /* 按住 Z：累积 hold_frames → level 上升 */
} pofv_charge_phase_t;

/* 本卡的全部标量状态（卡侧 `ce_state` 用；纯逻辑可独立单测）。*/
typedef struct {
    int32_t gauge;          /* 气槽：0..POFV_GAUGE_MAX（实心格数）*/
    int32_t gauge_pts;      /* 未满一格的充气点（0..POFV_GAUGE_PTS_PER_LEVEL-1）*/
    int32_t phase;          /* POFV_CHARGE_* */
    int32_t hold_frames;    /* 本次按住已持续帧数 */
    int32_t level;          /* a：当前等级（0 = 还没升到 C1）*/
    int32_t max_level;      /* b：当前可蓄最高等级 = min(gauge, 4) */
    int32_t sp;             /* 连击 SP（原版：击破与消弹都填）*/
    int32_t sp_stage;       /* 已触发过的 SP 档数 0..POFV_SP_STAGE_MAX */
    int32_t chain_kills;    /* 连爆窗口内的击破数（连爆质量档的来源）*/
    int32_t last_kill_frame;/* 上一次击破的帧号（−1 = 还没有过）*/
    int32_t kills;          /* 本局击破计数（日志 / 调参用）*/
    int32_t white_cancelled;/* 本局消掉的白弹数（日志 / 调参用）*/
    int32_t any_cancelled;  /* 本局消掉的全部弹数（含不可消；红圈的账）*/
    int32_t attacks;        /* 本局 SP 档触发的「boss attack」次数 */
} pofv_state_t;

void pofv_state_init(pofv_state_t *st);

/* CA 等级 L 的气槽代价 = L − 1（一手；非法等级钳到 0..POFV_CA_LEVEL_MAX）。 */
int  pofv_ca_cost(int level);

/* 等级 L 现在发得出来吗（气槽 ≥ L；一手门槛）。 */
int  pofv_ca_available(const pofv_state_t *st, int level);

/* b = 当前可蓄最高等级 = clamp(gauge, 0, 4)。 */
int  pofv_charge_max_level(int32_t gauge);

/* Z 上升沿：进入按住态。返回 1 = 开始蓄力，0 = 已经在按住态（重复调用无副作用）。 */
int  pofv_charge_begin(pofv_state_t *st);

/* 按住态每帧一次：累积 hold_frames、按 `POFV_CHARGE_FRAMES_PER_LEVEL` 升级（钳到 max_level）。
 * 返回 1 = 本次升了一级（卡侧播「叮」一声），0 = 没有新等级。 */
int  pofv_charge_hold(pofv_state_t *st);

/* ★ **拦 Z 条件**（`on_native_shot_suppress` 的返回值，用户 2026-09-12：
 *   「得拦 Z，这是花映塚的魅力」）：**按住 Z 且已经升到 ≥1 级**才抑制本帧原生弹。
 *   还没升到 1 级的那几帧不拦 ⇒ **点一下 Z = 正常发一发**（原版「Z 点一下 = 发一发」）。 */
int  pofv_charge_suppress(const pofv_state_t *st);

/* Z 落下沿：结算。返回 1 = 发动 CA（`*out_level` = 1..4，气槽已按 `pofv_ca_cost` 扣掉），
 * 0 = 等级 0（没蓄到 C1）⇒ **卡侧什么都不做，交回原生弹**（这一发就是「点射」）。 */
int  pofv_charge_release(pofv_state_t *st, int *out_level);

/* 打断蓄力（对话 / 暂停 / 死亡 / 关末）：清成 IDLE，**不扣气、不发动**。 */
int  pofv_charge_cancel(pofv_state_t *st);

/* ★ 2026-09-12：**QCA 整条放弃**（用户裁决「109 放弃 QCA 吧」）——
 * 原设计的 QCA 要落在 TH18 的炸弹键（X）上，而拦下它必须动 `on_bomb_gate`（`0x45c048`），
 * 那条路的「代价归属」在 `EVENTS-CONTRACT.md` §5 一直未拍板；对玩家的收益也薄（省一次蓄力）。
 * ⇒ 连同 `pofv_qca_level()` / `pofv_qca_fire()` 一起删掉（不留死代码），X 键**完全交还原生炸弹**。
 * 109 的 B 侧只保留「蓄力（长按 Z）」，见 `pofv_charge_*`。*/

/* 加充气点（蓝圈消弹 / 击破）；满 `POFV_GAUGE_PTS_PER_LEVEL` 进一格，气槽上限处**溢出丢弃**。 */
void pofv_gauge_add_points(pofv_state_t *st, int32_t pts);

/* CA 的无敌帧数。★ 一手：**C1 = 8 帧**（IDEAS 109 表）；C2–C4「无敌越长」但**帧数未逆到**
 * ⇒ ❓ 自定占位（TODO(th09)）。 */
int  pofv_ca_invuln_frames(int level);

/* ---- CA 无敌窗口的**声明层推进**（纯逻辑；卡侧 `on_tick_2` 每帧喂一次）★ 2026-09-12 审计修（任务 4）----
 * 契约（spec §5.1 一手锚点「C1 无敌 = 8 帧」/ §5.2 的 C2–C4 占位值）：
 *   **声明量 > 0 的帧数恰好 = `pofv_ca_invuln_frames(level)`**（声明 N 帧就是护住 N 帧）。
 * 用法（`pofv.c` 的 `ca_invuln_grant()` + `on_tick_2` 的开头）：
 *   起窗帧 F：`frames = pofv_invuln_grant(&w, level)` ⇒ 那一帧立即 `ce_invuln_request(id, frames)`
 *             （= 窗口的第 1 帧）；
 *   F+1..F+N−1：每帧 `pofv_invuln_tick(&w)` ⇒ 依次返回 N−1 … 1；
 *   **F+N：`pofv_invuln_tick(&w)` 返回 0**（= 退掉声明，窗口结束）。
 * ⚠ 旧实现（卡侧写 `if (left > 0) { ce_invuln_request(id, left); --left; }`）在起窗帧之外
 *   **又**声明了 N 帧 ⇒ 实际护住 F..F+N 共 **N+1** 帧（审计：C1 变成 9 帧）。
 *   ⇒ 修法就是「**先递减、再声明**」，搬进本文件是为了有主机单测
 *   （卡侧代码在主机编不出来 —— 同 `signal_core` / `mof_core` 的那两处搬迁）。*/
typedef struct { int32_t left; } pofv_invuln_t;

/* 清窗（`clear_card()` / 对话打断用）：窗口结束、调用方应 `ce_invuln_request(id, 0)`。 */
void    pofv_invuln_init(pofv_invuln_t *w);
/* 起窗：返回**起窗帧**应声明的帧数（= 档位值；等级非法 → 0，调用方什么都不做）。
 * 同时把内部剩余量置成同一个值（`tick` 从下一帧开始递减）。 */
int32_t pofv_invuln_grant(pofv_invuln_t *w, int level);
/* 起窗帧之后的每一帧调一次：返回**本帧**应声明的剩余帧数（0 = 窗口已结束 ⇒ 退声明）。 */
int32_t pofv_invuln_tick(pofv_invuln_t *w);
/* 当前剩余量（诊断 / 单测；0 = 窗口不在）。 */
int32_t pofv_invuln_left(const pofv_invuln_t *w);

/* ==================================================================== C. 爆风 / 红圈 / SP */

/* 连爆质量档：1..POFV_BLAST_CHAIN_MAX（❓ 自定：原版「连爆质量」的分档与判据未逆到，
 * 本实现 = 「距上次击破 ≤ POFV_CHAIN_WINDOW_FRAMES 帧就 +1 档」）。
 * ★ 纯算术、O(1) —— 它是**死亡回调里唯一允许做的事**（review §4.1 的性能陷阱）。 */
#define POFV_BLAST_CHAIN_MAX        4
#define POFV_CHAIN_WINDOW_FRAMES    30   /* ❓ 自定（TODO(th09)）*/

/* 击破登记：推进连爆窗口（`frame - last_kill_frame <= 窗口` 则 +1 档，否则回 1 档），
 * 返回本次爆风应使用的档位（1..POFV_BLAST_CHAIN_MAX）。`frame` = 卡侧的游玩帧计数（单调）。 */
int  pofv_chain_register_kill(pofv_state_t *st, int32_t frame);

/* 蓝圈（爆风）半径：按连爆质量档。❓ 自定占位（TODO(th09)：原版爆风半径未逆到）。 */
void pofv_blast_radius(int chain, float *out);
/* 红圈（蓄力攻击）半径：按 CA 等级 1..4（C4 = 近乎全屏）。❓ 自定占位（TODO(th09)）。 */
void pofv_ca_circle_radius(int level, float *out);

/* ★ 一手：被消的白弹**投到对手场地**在单机没有对应（用户口径：改成**转成分**）⇒
 *   `Bullet__cancel(b, mode)` 的 mode 取 **1**（引擎 `0x4290e0`：加分 + 掉点道具）；
 *   红圈走的 mode = **0**（纯删除 —— 一手「红圈消掉的白弹**直接消失**」）。 */
#define POFV_BULLET_CANCEL_MODE_BLAST   1
#define POFV_BULLET_CANCEL_MODE_CA      0

/* 消弹来源（记账口径不同，见下面两个函数）。 */
typedef enum { POFV_CANCEL_SRC_BLAST = 0, POFV_CANCEL_SRC_CA = 1 } pofv_cancel_src_t;

/* ❓ 自定（TODO(th09)）：下面五条是「填槽速率」的占位，原版（爆风按连爆质量回气、
 * 连击 SP 的每颗/每杀点数）**没有逆到**。量纲：SP 与原版同口径（阈值 10 万 / 30 万 / 50 万）。*/
#define POFV_GAUGE_PTS_PER_LEVEL        100
#define POFV_GAUGE_PTS_PER_WHITE_CANCEL  10
#define POFV_GAUGE_PTS_PER_KILL          20
#define POFV_SP_PER_WHITE_CANCEL         10
#define POFV_SP_PER_RED_CANCEL           10
#define POFV_SP_PER_KILL                100

/* ★ 一手（IDEAS 109「连击 / SP」）：SP 到 **10 万 / 30 万 / 50 万** → 发动 boss attack。 */
#define POFV_SP_STAGE_MAX 3
int32_t pofv_sp_threshold(int stage);          /* stage 0..2 → 100000 / 300000 / 500000；越界 → 0 */
int     pofv_sp_attack_stage(int32_t sp);      /* 当前 SP 达到了几档（0..3）*/

/* SP 越过新档时**只报一次**（跨两档也只报一次；下一次要再跨一档才会再报）。
 * 返回 1 = 本次应发动 boss attack。 */
int  pofv_sp_take_attack(pofv_state_t *st);

/* 记一笔消弹：
 *   · 蓝圈（`SRC_BLAST`）：消掉 `n_white` 颗**白弹** ⇒ 充气（按连爆质量 `chain` 加权）+ 填 SP；
 *   · 红圈（`SRC_CA`）：消掉 `n_any` 颗（含不可消）⇒ **只填 SP、不回复气**
 *     ——★ 一手：「红色消弹圈消掉的白弹直接消失且**不回复气**」。 */
void pofv_account_blast(pofv_state_t *st, int32_t n_white, int32_t chain);
void pofv_account_ca(pofv_state_t *st, int32_t n_any);
/* 击破一笔：填 SP + 充气（连爆质量由调用方在前面用 pofv_chain_register_kill 推进）。 */
void pofv_account_kill(pofv_state_t *st);

/* ---- 爆风队列（review §4.1 强制：死亡回调里**只登记**，帧末统一消弹）----
 * 清场帧一次回调 100–300 次，回调里扫弹池 = O(n²)（弹池 2000 槽 × stride 0xfa0）⇒
 * 回调只 push（O(1)），每帧只真消 `POFV_BLASTS_PER_FRAME` 次。 */
#define POFV_BLAST_QUEUE_MAX   64   /* ❓ 自定（TODO：原版无此队列；按最坏清场帧 100–300 定的折中）*/
#define POFV_BLASTS_PER_FRAME   8   /* ❓ 自定（每帧消弹次数上限，防 O(n²)）*/

typedef struct { float x, y; int32_t chain; } pofv_blast_t;
typedef struct {
    pofv_blast_t item[POFV_BLAST_QUEUE_MAX];
    int32_t head;      /* 队首下标（环形）*/
    int32_t n;         /* 队列长度 */
    int32_t dropped;   /* 因队满丢掉的爆风数（日志 / 调参用；丢 = 少一次消弹，不崩）*/
} pofv_blast_queue_t;

void pofv_blast_queue_init(pofv_blast_queue_t *q);
int  pofv_blast_enqueue(pofv_blast_queue_t *q, float x, float y, int32_t chain);  /* 1 = 入队，0 = 队满丢弃 */
int  pofv_blast_take(pofv_blast_queue_t *q, pofv_blast_t *out);                   /* FIFO；0 = 空 */
/* 带帧预算的出队：`*budget <= 0` 直接返回 0；否则出队一颗并 `--*budget`。 */
int  pofv_blast_take_budget(pofv_blast_queue_t *q, pofv_blast_t *out, int32_t *budget);

/* ==================================================================== D. 活化灵 */

/* ★ 一手（IDEAS 109「固灵（吸灵）」）：
 *   · 按低速键（Shift）→ 低速移动 + 展开暗色**吸灵领域**（本卡叫「固灵」）；
 *   · 进入领域的**幽灵** → 变圆形、**HP 减半**、**缓慢上浮** → 此时叫**活化灵**；
 *   · 活化灵**一段时间内没被击破会自爆**，生成 **3 颗自机狙小玉**（3 = 一手）；
 *   · **普通幽灵很硬（3 枪打不掉）、只有被连爆波及时才爆**；活化灵很脆、**击破有爆风**。
 * ⚠ 一手未逆到的量（❤ 占位，spec「数值表」标 ❓ 自定）：领域半径、幽灵 HP、自爆倒计时、
 *   上浮速度、场上灵魂上限。 */
#define POFV_SPIRIT_MAX                  32   /* ❓ 自定 */
#define POFV_SPIRIT_HP_TOUGH              2   /* ❓ 自定（普通幽灵「3 枪打不掉」⇒ 只有爆风打得动）*/
#define POFV_SPIRIT_HP_ACTIVATED          1   /* ★ 一手语义「HP **减半**」⇒ 恒 = HP_TOUGH / 2 */
#define POFV_SPIRIT_SELFDESTRUCT_FRAMES 180   /* ❓ 自定（原版「一段时间」未逆到）*/
#define POFV_SPIRIT_SELFDESTRUCT_BULLETS  3   /* ★ 一手：自爆生成 3 颗自机狙小玉 */
#define POFV_SPIRIT_RISE_PER_FRAME        0.5f/* ❓ 自定（活化灵**缓慢上浮**，px/帧）*/
#define POFV_DOMAIN_R                    96.0f/* ❓ 自定（固灵领域半径）*/
#define POFV_BLAST_SPIRIT_DMG             2   /* ❓ 自定（爆风对灵魂的伤害；≥ HP_TOUGH ⇒ 连爆必杀）*/

typedef struct {
    int32_t active;          /* 1 = 槽在用 */
    int32_t activated;       /* 1 = 活化灵（HP 已减半、正在缓慢上浮、有自爆倒计时）*/
    float   x, y;            /* 世界坐标（卡侧维护；本卡当前**不建 VM**，视觉待 anm 资产）*/
    int32_t hp;
    int32_t age;             /* 出生帧数 */
    int32_t selfdestruct_in; /* 活化灵自爆倒计时（帧）；≤0 = 本帧自爆；未活化 = -1 */
    int32_t src_enemy_id;    /* 来源敌人的 enemy_id（记账；**不缓存敌人指针**）*/
} pofv_spirit_t;

typedef struct {
    pofv_spirit_t s[POFV_SPIRIT_MAX];
    int32_t live;
} pofv_spirit_pool_t;

void pofv_spirit_pool_init(pofv_spirit_pool_t *p);
/* 掉一颗灵魂（拿卡后像 hyz 一样掉）：返回槽号，−1 = 池满（丢弃）。 */
int  pofv_spirit_spawn(pofv_spirit_pool_t *p, float x, float y, int32_t src_enemy_id);
/* 固灵：把领域（圆心 px,py、半径 POFV_DOMAIN_R）内的**未活化**灵魂转成活化灵
 * （HP 减半 + 起自爆倒计时）。`*out_n` = 本次活化的个数。返回本次活化个数。 */
int  pofv_spirit_activate_in_domain(pofv_spirit_pool_t *p, float px, float py, int32_t *out_n);
/* 每帧推进：活化灵缓慢上浮 + 自爆倒计时。到点的槽写进 `out_slots`（调用方接着做自爆效果）
 * 并**就地清槽**。返回自爆个数。cap 用满时多出的自爆**留在槽里**、下一帧再爆（不漏报）。 */
int  pofv_spirit_tick(pofv_spirit_pool_t *p, int32_t *out_slots, int cap);
/* 爆风波及：圆心 (x,y)、半径 r 内的灵魂吃 `dmg`。普通灵魂只有爆风打得动（一手）。
 * 被击破的槽号写进 `out_slots` 并清槽。返回击破个数。
 * ⚠ cap 传 `POFV_SPIRIT_MAX`（卡侧就这么用）：超过 cap 的击破**仍会清槽但不会被报出来**
 *   （少一次连锁爆风），所以别传小。 */
int  pofv_spirit_blast_hit(pofv_spirit_pool_t *p, float x, float y, float r, int32_t dmg,
                           int32_t *out_slots, int cap);
void pofv_spirit_remove(pofv_spirit_pool_t *p, int32_t slot);
