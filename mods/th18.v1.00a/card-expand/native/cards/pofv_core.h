/* pofv_core.h —— 花映塚 × 獣王園 合成卡（card-expand id 109）的**纯逻辑**：
 * 白弹半径阈值分类 / 气槽 4 级与蓄力状态机 / CA 与 QCA 的等级与代价 /
 * 暴风（蓝圈）与 CA（红圈）的半径与消弹记账 / SP 三档 / 活化灵状态机 / 暴风队列 /
 * **灵力道具池（自建实体，§E）**。
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
 * ★★ 2026-09-22（P5 #1/#3）：TH09 的一手只给了**律**，没给这个数 ——
 *   蓄力量每帧 `+= ds:0x4b36b8 × shot_data[+0x24]`（`th09:0x41fa75`–`0x41faa4`），
 *   **1 格 = 100 单位**（`th09:0x41fa80`–`0x41fab0` 的 `/100` 判级 + `th09:0x41bc90` 的 `/100` 通知 HUD），
 *   每角色每火力的 `+0x24` 速率表没逆到 ⇒ 这里仍是占位帧数。
 * 占位语义：每按住这么多帧升一级；`a`（当前等级）上限 4 档 —— ⚠ **不再被气槽夹**
 *   （原版蓄力量只被气槽**钳上界**，见 `pofv_charge_hold` 的注释）。 */
#define POFV_CHARGE_FRAMES_PER_LEVEL 45

/* ★★ 2026-10-01（owner 口径：合成卡「不拦截 Z 了，还是现代一些吧」）：
 *   **整条拦 Z 闸删除**（含 `POFV_SUPPRESS_AFTER_FRAMES` 与 `pofv_charge_suppress()`）。
 *   历史：TH09 一手 `th09:0x41fa17 push 0xa` = 「按住射击满 10 帧就停发原生弹」，
 *   P5 批（2026-09-22）照它实装过；owner 2026-09-28 判定「太屎了」⇒ 作废。
 *   ⇒ 现在**原生行循环永远照跑**，蓄力只由**按住 C 的电平**推进（见 pofv.c 的接线）。
 *   ⚠ 别再把它当「一手」引回来：一手事实仍然成立，是**本卡刻意不复刻**（登记 TUNING §2.0v #1）。*/

typedef enum {
    POFV_CHARGE_IDLE = 0,          /* 没在蓄力 */
    POFV_CHARGE_HOLD = 1           /* 按住 C：累积 hold_frames → level 上升 */
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
    int32_t chain_kills;    /* 最近一次由暴风点着的击破所记的档（★ 2026-09-22：不再是 30 帧窗口的连击数，
                             * 而是「上一颗被暴风点着的敌人的档位」，见 `pofv_storm_chain_at` 的用法）*/
    int32_t last_kill_frame;/* 上一次击破的帧号（−1 = 还没有过；只做日志/调参）*/
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

/* **按住 C 的上升沿**：进入按住态。返回 1 = 开始蓄力，0 = 已经在按住态（重复调用无副作用）。 */
int  pofv_charge_begin(pofv_state_t *st);

/* 按住态每帧一次：累积 hold_frames、按 `POFV_CHARGE_FRAMES_PER_LEVEL` 升级（钳到 max_level）。
 * 返回 1 = 本次升了一级（卡侧播「叮」一声），0 = 没有新等级。 */
int  pofv_charge_hold(pofv_state_t *st);

/* 本帧的**可用等级** = min(charge 等级 a, 气槽 b) —— 一手：原版蓄力量被气槽**钳上界**
 * （`th09:0x41fab0 fcomp [player+0x30388]` / `0x41fabd` 钳到上限），
 * 而 `气槽/100` 就是能放出的档数（`th09:0x41f92b`–`0x41fbd8` 的释放分支）。
 * 返回 0 = 这一下放不出 CA（点射 / 气不够）。 */
int  pofv_charge_usable_level(const pofv_state_t *st);

/* **C 落下沿（松开 C）**：结算。返回 1 = 发动 CA（`*out_level` = 1..4，气槽已按 `pofv_ca_cost` 扣掉），
 * 0 = 可用等级 0（没蓄到 / 气不够）⇒ **卡侧什么都不做**（原生射击这一路本来就没被拦过）。 */
int  pofv_charge_release(pofv_state_t *st, int *out_level);

/* 打断蓄力（对话 / 暂停 / 死亡 / 关末）：清成 IDLE，**不扣气、不发动**。 */
int  pofv_charge_cancel(pofv_state_t *st);

/* ★ 2026-09-12：**QCA 整条放弃**（用户裁决「109 放弃 QCA 吧」）——
 * 原设计的 QCA 要落在 TH18 的炸弹键（X）上，而拦下它必须动 `on_bomb_gate`（`0x45c048`），
 * 那条路的「代价归属」在 `EVENTS-CONTRACT.md` §5 一直未拍板；对玩家的收益也薄（省一次蓄力）。
 * ⇒ 连同 `pofv_qca_level()` / `pofv_qca_fire()` 一起删掉（不留死代码），X 键**完全交还原生炸弹**。
 * 109 的 B 侧只保留「蓄力（**按住 C 的电平**）」，见 `pofv_charge_*`。*/

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

/* ==================================================================== C. 暴风 / 红圈 / SP */

/* ★★ 2026-09-22（P5 #4「机制重做」）**暴风 = 锚定在世界上的场，不是一次性命中**
 *
 * 一手（TH09 v1.50a，`local/th09.v1.50a/th09.exe`；全链见
 * `engine/card/th09/01-pofv-charge-and-cancel-fields.md`）：TH09 的消弹圈**全都是"节点"**，
 * 一个节点 = (圆心, 起始半径 r₀, 每帧增量 dr, 寿命帧)：
 *   · 建：`th09:0x41cf30`（`+0x8 = r₀`、`+0xc = dr`、`+0x24 = 寿命`、`+0x38 = kind`），
 *     包装 `0x41cfe0` / `0x41d0d0` / `0x41d010` / `0x41d050` / `0x41d090`（各自只改 kind 与倍率）；
 *   · 推进：`th09:0x41c8e0` —— **每帧 `r += dr`、`寿命--`、到 0 释放**（`0x41c936`–`0x41c95e`）。
 * 已在册的实例（全部一手）：
 *   | 谁 | r₀ | dr | 寿命 | 终点半径 |
 *   | --- | --- | --- | --- | --- |
 *   | 自机弹命中 | 0 | 1.6 | 6 | ≈10（`th09:0x41fe93`–`0x41fea3`）|
 *   | CA 释放那一下 | 16 | 20 | 8 | **176**（`th09:0x41e875`–`0x41e88c`）|
 *   | CA L1/L2/L3 | 0 | 4.0 / 4.0 / 3.5 | 48 / 64 / 128 | **192 / 256 / 448**（`th09:0x403c30/90/f0`）|
 *   | boss attack | 500 | 0 | 30000 | 500（`th09:0x417934`–`0x41795b`）|
 * 画出来的环走**同一个律**：`etama.anm` script91/92/93 = `pos(16.0f)` + `posTime(48/64/128, 0, 192/256/448)`
 * （2026-09-16 已逆，见 `engine/card/th18/14-clear-range-barrier.md` §2.4）。
 *
 * ⇒ **原版的暴风停在原地、按帧扩张、活几十帧**；「相对世界几乎静止」（owner 2026-09-18 原话）
 *   是**字面意思**：圈不动，是妖精飞进来。连锁因此由**妖精的到场率**决定，
 *   而不是一个固定窗口的节拍器 —— 旧实现（一次性 `ce_damage_rect` 2 帧 + 30 帧连爆窗口打档）
 *   正是用户报的「暴风连锁好像不该这么快」。
 * ⚠ 本卡**没有** VM（视觉待资产），所以这里只做**判定场**：一个每帧都判的圆。 */
#define POFV_STORM_MAX              8    /* ❓ 自定（原版是节点池；本卡要控每帧工作量）*/
#define POFV_STORM_FRAMES          30    /* ❓ 自定（一手只给了形：锚定 + 逐帧扩张 + 有限寿命）*/
#define POFV_STORM_R0              16.0f /* ★ 一手：环的起始半径 16（etama script91/92/93 的 `pos(16.0f,…)`）*/
#define POFV_STORM_SCANS_PER_FRAME  4    /* ❓ 自定：每帧最多几次**整池扫弹**（O(弹池) 的预算闸）*/
#define POFV_STORM_DMG_LIFE         2    /* ❓ 自定（伤害源寿命；与旧 `POFV_BLAST_DMG_LIFE` 同值）*/

typedef struct {
    float   x, y;        /* 圆心（世界坐标）。★ 生成后**不再移动** —— 这就是「相对世界静止」*/
    int32_t chain;       /* 连爆档 1..POFV_BLAST_CHAIN_MAX */
    int32_t age;         /* 已经活了几帧（0 = 出生帧）*/
    int32_t life;        /* 寿命帧数（到点释放）*/
    int32_t active;
} pofv_storm_t;

typedef struct {
    pofv_storm_t s[POFV_STORM_MAX];
    int32_t live;
    int32_t dropped;     /* 池满丢掉的暴风数（日志 / 调参用；丢 = 少一次场，不崩）*/
} pofv_storm_pool_t;

void pofv_storm_pool_init(pofv_storm_pool_t *p);

/* 起一个暴风（O(1)：死亡回调里只准做这个）。返回槽号，−1 = 池满（丢弃）。 */
int  pofv_storm_spawn(pofv_storm_pool_t *p, float x, float y, int32_t chain);

/* ★ 连爆档的来源（替换旧的 `pofv_chain_register_kill` 帧窗口）：
 *   返回**覆盖 (x,y) 这一点的最高档**（0 = 没有任何暴风盖住它）。
 *   卡侧用法：击破事件里 `c = pofv_storm_chain_at(...)` ⇒ 新暴风档 = `c ? c+1 : 1`
 *   —— 被暴风点着的（c ≥ 1）才 +1 档，自机弹直接打死的回 1 档。
 *   ⇒ 连锁**只能跟着妖精走进场**，不会靠一个 30 帧窗口自己往上蹿。 */
int  pofv_storm_chain_at(const pofv_storm_pool_t *p, float x, float y);

/* 每帧推进：`age++`、到寿命的槽就地释放。返回本帧**还活着**的暴风数（= `p->live`）。*/
int  pofv_storm_tick(pofv_storm_pool_t *p);

/* 第 `i` 个槽（0..POFV_STORM_MAX−1）；`active == 0` 表示空槽。 */
const pofv_storm_t *pofv_storm_slot(const pofv_storm_pool_t *p, int i);

/* 当前半径（**每帧都在长**）：`r(t) = r₀(16) + (R_chain − 16) × age / life`
 *   —— 与 TH09 画环的 `posTime` 同律（起点 16、终点 R、线性、时长 = 寿命）。
 * ⚠ 本卡让**判定半径与将来画出来的环共用这一个数**（比 TH09 更严：TH09 的环与消弹圈只对齐终点）。 */
void pofv_storm_radius(const pofv_storm_t *st, float *out);

/* 连爆档 chain 的**终点半径** R。★ 档 1 = **176**（一手：TH09 的 CA 释放节点 16 + 20×8 = 176，
 * `th09:0x41e875`）—— 我们把它当最小暴风的终点半径；档 2..4 每档 +48 ❓ 自定（TODO(th09)）。 */
void pofv_storm_final_radius(int32_t chain, float *out);

/* 兼容旧名（旧实现的"爆风半径表"就叫 `pofv_blast_radius`；语义现在 = 该档暴风的终点半径）。*/
void pofv_blast_radius(int chain, float *out);

/* 连爆档上限（1..4）。❓ 自定：原版「连爆质量」的分档数没逆到；一手的**场**只给了半径律
 * （见上面 §C 的表），没给「档」这一层 —— 它是本卡对「暴风越连锁越大」的建模。 */
#define POFV_BLAST_CHAIN_MAX        4

/* 红圈（蓄力攻击）半径：按 CA 等级 1..4（C4 = 近乎全屏）。❓ 自定占位（TODO(th09)）。
 * ★ 2026-09-22：TH09 的**红色消弹圈**（= CA）已经逆到一手数字了，见
 * `pofv_ca_circle_radius()` 的注释 —— 那一支现在用的是**一手终点半径**，不再是占位。 */
void pofv_ca_circle_radius(int level, float *out);

/* ★ 一手：被消的白弹**投到对手场地**在单机没有对应（用户口径：改成**转成分**）⇒
 *   `Bullet__cancel(b, mode)` 的 mode 取 **1**（引擎 `0x4290e0`：加分 + 掉点道具）；
 *   红圈走的 mode = **0**（纯删除 —— 一手「红圈消掉的白弹**直接消失**」）。 */
#define POFV_BULLET_CANCEL_MODE_BLAST   1
#define POFV_BULLET_CANCEL_MODE_CA      0

/* 消弹来源（记账口径不同，见下面两个函数）。 */
typedef enum { POFV_CANCEL_SRC_BLAST = 0, POFV_CANCEL_SRC_CA = 1 } pofv_cancel_src_t;

/* ❓ 自定（TODO(th09)）：下面三条是「连击 SP 的每颗/每杀点数」的占位，原版**没有逆到**。
 * 量纲：SP 与原版同口径（阈值 10 万 / 30 万 / 50 万）。
 * ⚠ 充能（气槽）那三条旧常量（`POFV_GAUGE_PTS_PER_WHITE_CANCEL` / `_PER_KILL`）**已删除**：
 *   口径 5 起，充能一律**经灵力道具**（面值 = §E 的 `POFV_POWER_PTS_*`），不再有第二条直接加点路。*/
#define POFV_GAUGE_PTS_PER_LEVEL        100
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

/* 记一笔消弹（★ 2026-10-01 口径 5：**这里不再直接加气**）：
 *   · 蓝圈（`SRC_BLAST`）：消掉 `n_white` 颗**白弹** ⇒ 只记 counters + 填 SP；
 *     充能改由卡侧撒**灵力道具**（面值见 `pofv_power_value_cancel()` + §E 的打包撒）；
 *   · 红圈（`SRC_CA`）：消掉 `n_any` 颗（含不可消）⇒ **只填 SP、不回复气**
 *     ——★ 一手：「红色消弹圈消掉的白弹直接消失且**不回复气**」。 */
void pofv_account_blast(pofv_state_t *st, int32_t n_white, int32_t chain);
void pofv_account_ca(pofv_state_t *st, int32_t n_any);
/* 击破一笔（★ 同上：只记 kills + 填 SP，充能走卡侧的灵力道具）。 */
void pofv_account_kill(pofv_state_t *st);

/* ★ 消弹源要撒的**总面值**（打包前的账）：`n_white × POFV_POWER_PTS_CANCEL × 连爆档`。
 * 保留原来「连爆质量越高、同样的白弹给得越多」的味道（旧 `pofv_account_blast` 的 `× c`）。
 * 面板值与 §E 的 `POFV_POWER_PTS_CANCEL` 同源 ⇒ 改一个数就够。 */
int32_t pofv_power_value_cancel(int32_t n_white, int32_t chain);

/* ---- 性能纪律（review §4.1 仍然成立）----
 * 清场帧 `on_enemy_death` 一次被调 100–300 次 ⇒ 回调里**只准 O(1)**：现在做的是
 * 「`pofv_storm_chain_at()`（≤8 次比较）+ `pofv_storm_spawn()`（一次 memset）」，
 * 真正的消弹 / 伤害在 `on_tick_2` 帧末按 `POFV_STORM_SCANS_PER_FRAME` 的预算做。
 * （旧实现在这里 push 一个 64 格的爆风队列；场模型下不需要队列：场本身就活在池里。）*/

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

/* ==================================================================== E. 灵力道具（自建实体池）
 *
 * owner 2026-09-28 口径 5：「**灵力道具用 A**」⇒ 充能货币统一 = 灵力道具
 * （消弹 / 击破 / 结界碰弹 → 掉 → 拾取 → 涨槽）。
 *
 * ★★ 2026-10-01 落地形态（**改判规格 §4 #9 的「新道具类型」**）：经三路互证
 *   （`0x44685c` 映射表 + `0x446830` 跳表 + `0x4b4020` 贴图脚本表），TH18 **没有干净的原生道具空号**：
 *     · type 1（小 P）/ 3（大 P）= **110/111 的满 P 转换目标**（`faith_convert_full_p_items()`
 *       在满火力时**主动删掉**这两类原生道具）⇒ 用它们会被别人的卡吃掉；
 *     · type 9–0xe（分数转换点族）= **107 的 ★ + 114 的点数**载体（两卡的 `on_item_collect`
 *       都按这个区段入账）⇒ 串账；
 *     · type 7 = 112 的整炸弹、8 = 115 的满 P、4/6 = 117 的碎片、0x10/0x11 = 115/111 的碎片；
 *     · type 0 的贴图脚本 = `0xffffffff`（**隐形**）、type 0xf 在 spawner `0x446ff7` 被改写成 6。
 *   ⇒ 本卡走**自建实体池**（先例：`cards/faith.c` 的 110/111 绿点、`cards/ufo.c` 的 112 令牌、
 *     `cards/animal.c` 的 117 动物灵）：**零站点、不与任何别人的卡共享状态**。
 *   一手核表与逐 type 占用表见 `SPEC-109-FUSION.md` §8.1。
 *
 * 经济口径（**与改造前的「直接加点」等价**，只是中间插了一层可拾取的实体）：
 *   · 消弹：`n` 颗白弹 × `POFV_POWER_PTS_CANCEL` × 连爆档（= 原 `pofv_account_blast` 的式子）；
 *   · 击破：1 颗 × `POFV_POWER_PTS_KILL`（= 原 `pofv_account_kill`）；
 *   · 结界碰弹：1 颗 × `POFV_POWER_PTS_AURA`（口径 6 的新源）。
 *   **打包**（照 107 小樱点的先例）：一次事件要撒的颗数按「每帧上限 / 池余量」截断，
 *   **总面值不变**（余数摊到已撒的那几颗上）⇒ 上限与池满都不会悄悄少给充能点。
 *
 * ⚠ 池是**文件级**还是卡内由卡侧决定（本文件只给纯逻辑）；`pofv_power_clear()` 必须被
 *   `on_delete` 调到（METHOD 铁律：长持续效果删卡要收干净 —— 这里没有 VM，清了池就没有残留）。 */

#define POFV_POWER_MAX               48     /* ❓ 自定（同屏上限）*/
#define POFV_POWER_PTS_CANCEL        10     /* ❓ 自定（消弹源 1 颗的基准面值 = 旧「消弹 +10」）*/
#define POFV_POWER_PTS_KILL          20     /* ❓ 自定（击破源 = 旧「击破 +20」）*/
#define POFV_POWER_PTS_AURA           5     /* ❓ 自定（结界碰弹源；口径 6）*/
#define POFV_POWER_SPAWN_PER_FRAME    4     /* ❓ 自定（每帧上限；口径 6 的护栏之一）*/
#define POFV_POWER_SPAWN_R           24.0f  /* ❓ 自定（一批多颗时的铺开半径）*/
#define POFV_POWER_FALL               0.5f  /* ❓ 自定（飘落速度 px/帧）*/
#define POFV_POWER_MAG_R             56.0f  /* ❓ 自定（磁吸半径）*/
#define POFV_POWER_MAG_SPEED          4.0f  /* ❓ 自定（磁吸速度 px/帧）*/
#define POFV_POWER_ABSORB_R          12.0f  /* ❓ 自定（收取半径）*/
#define POFV_POWER_LIFE             600     /* ❓ 自定（寿命帧；到点自行消失，不刷屏）*/

typedef enum {
    POFV_POWER_SRC_CANCEL = 0,   /* 消弹（暴风蓝圈的白弹）*/
    POFV_POWER_SRC_KILL   = 1,   /* 击破敌人 */
    POFV_POWER_SRC_AURA   = 2    /* 固灵结界碰敌弹（口径 6）*/
} pofv_power_src_t;

typedef struct {
    int32_t active;
    float   x, y;        /* 世界坐标（卡侧维护 + 画 VM）*/
    int32_t value;       /* 收取时加的充能点（**打包值**，可能 > 基准面值）*/
    int32_t age;
    int32_t life;        /* 寿命帧（到点自行消失）*/
    int32_t src;         /* pofv_power_src_t（诊断 / 日志）*/
} pofv_power_t;

typedef struct {
    pofv_power_t s[POFV_POWER_MAX];
    int32_t live;
    int32_t spawned;     /* 累计撒出颗数（诊断）*/
    int32_t absorbed;    /* 累计收取颗数（诊断）*/
    int32_t expired;     /* 累计寿命到期消失颗数（诊断）*/
    int32_t dropped;     /* 因池满 / 每帧上限被**打包吞掉**的颗数（诊断；面值不丢）*/
} pofv_power_pool_t;

void pofv_power_pool_init(pofv_power_pool_t *p);

/* 撒一颗（`value` = 面值）。返回槽号，−1 = 池满。 */
int  pofv_power_spawn(pofv_power_pool_t *p, float x, float y, int32_t value, int32_t src);

/* ★ **打包撒**：把「`n_units` 份 × `pts_each`」的总面值摊到最多 `max_spawn` 颗实体上
 * （在 (x,y) 周围 `POFV_POWER_SPAWN_R` 内铺开；只有一颗时就在原点）。
 * 实际颗数还会被池余量钳住 ⇒ 面值守恒（余数给第一颗）；一颗都撒不出时 `*out_pts = 0`。
 * 返回实际撒出的颗数。 */
int  pofv_power_spawn_packed(pofv_power_pool_t *p, float x, float y, int32_t n_units,
                             int32_t pts_each, int32_t max_spawn, int32_t src, int32_t *out_pts);

/* 每帧推进：飘落 + 靠近自机时磁吸 + 到收取半径内**收取**（点累加进 `*out_pts`）/ 到寿命消失。
 * 返回本帧收取的**颗数**；`*out_pts` = 本帧收取的总面值（调用方 `pofv_gauge_add_points`）。
 * `player` = 自机 float3（NULL 时只飘落、不磁吸也不收取 ⇒ 供单测/无自机帧）。 */
int  pofv_power_tick(pofv_power_pool_t *p, const float *player, int32_t *out_pts);

/* 删除所有实体（`clear_card()` 用；返回清掉的颗数）。 */
int  pofv_power_clear(pofv_power_pool_t *p);

/* ★ **多位置打包撒**（结界碰弹用）：把「`n_units` 份 × `pts_each`」的总面值摊到
 * `k = min(n_units, max_spawn, 池余量)` 个**调用方给的位置**上（`ox`/`oy` 至少这么多个）。
 * 总面值守恒（余数给第一颗）；`*out_pts` = 实际撒出的总面值。返回实际撒出的颗数。 */
int  pofv_power_spawn_packed_at(pofv_power_pool_t *p, const float *ox, const float *oy,
                                int32_t n_units, int32_t pts_each, int32_t max_spawn,
                                int32_t src, int32_t *out_pts);

/* ==================================================================== F. 固灵结界（除霊オーラ）
 *
 * owner 口径 4：「**固灵结界常驻**」（与 C 键无关）；口径 6：「结界碰敌弹 = 碰弹掉道具
 * （**每颗弹只算一次 + 每帧上限**）+ 对灵沈静化 + 击破掉一把」；口径 3：**不做减速**
 * （TH19 的那三个 `0.5f` 字段一个都不写）。
 *
 * 几何（★ 一手 TH19 §7.3）：19 类里**最省的一套 = 圆**，本卡取 **Reimu 类**的判定式
 *   `dx² + dy² < r² + pad²`（`th19:0x416f10`；`pad` = 目标半径，TH19 传敌弹半径
 *   `bullet+0x674`、传敌人半径 `EnemyInf+0x1350 / 2`）—— 本卡的 `pad` = 弹半径 `+0x658`。
 *   ⚠ Sanae 族是 `dx²+dy² <= (r+pad)²`（pad 线性并进半径），与 Reimu 的平方和并入不同；
 *     本卡选 Reimu 那支（一手其一，登记 `TUNING.md`）。
 *
 * 半径（★ 一手 TH19 §7.2 的 Reimu 公式）：`clamp(S+0x8c,0,200) × K/100`，Reimu 的 **K = 64**
 *   ⇒ 满蓄力 = 128。以 TH19 的尺寸为锚，本卡取**常驻**半径 `POFV_AURA_R = 96`（❓ 自定：
 *   落在 Reimu 的 0..128 区间内；口径 4 要求「常驻」⇒ **不随 C 的档位变**）。
 *   ★ 一手性质照抄：**判定半径 = 绘制半径**（TH19 §7.1/§9.2，`aura+0x9c` 同时喂槽 7 与
 *     VM 的 `scale.x/y`）⇒ 卡侧的结界 VM 每帧写 `scale = POFV_AURA_R`。 */

#define POFV_AURA_R                 96.0f  /* ❓ 自定（常驻半径；一手 Reimu 公式的 0..128 区间内）*/
#define POFV_AURA_ITEMS_PER_FRAME    4     /* ❓ 自定（口径 6 的「每帧上限」；与消弹源同一把闸）*/

/* 结界判定（★ 一手 TH19 `th19:0x416f10` 的 Reimu 圆）：`dx² + dy² < r² + pad²`。
 * `r <= 0` ⇒ 永不命中（结界没展开）。 */
int pofv_aura_hit(float dx, float dy, float r, float pad);
