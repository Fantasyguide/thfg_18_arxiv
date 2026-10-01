/* pofv_core.h —— 花映塚 × 獣王園 合成卡（card-expand id 109）的**纯逻辑**：
 * 白弹半径阈值分类 / 气槽 4 级与蓄力状态机 / CA 与 QCA 的等级与代价 /
 * 暴风（蓝圈）与 CA（红圈）的半径与消弹记账 / SP 三档 /
 * **灵力道具池（自建实体，§E）** / **固灵结界几何（§F）** / **灵（真敌人）句柄池（§G）**。
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
    int32_t shield;         /* ★ オートパリア：1 = **有効**（挡 1 发）；0 = 无（见 §H）*/
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

/* ★ 2026-10-01（109 的「逐弹归属」）：判断一颗**敌弹**是不是**我们那个伪 boss 载体**发的。
 *
 * 判据 = 弹种类型在**我们 ECL 用到的集合**里（逐条来自 `assets/ecl/pofv109_boss.ecl.txt` 的
 * `etSprite` 调用）：**0 / 1 / 2 / 11 / 12 / 13 / 18 / 20**
 *   · 灵梦（TH09）：C3 `(2,·)` `(0,·)`；C4 `(11,·)` `(0,·)`
 *   · 魔理沙（TH09）：`(12,·)` `(13,·)` `(0,·)`
 *   · 咲夜（TH09）：`(20,·)` `(0,·)`
 *   · ★ 早苗（TH19）：C3 `(0,·)`；C4 `(11,·)` `(1,·)` `(18,·)`
 * ⚠ **这份名单必须与 `assets/ecl/pofv109_boss.ecl.txt` 里的 `etSprite` 类型一致** ——
 *   `make_ufo_ecl.py` 有一道**硬闸**逐条对账（漏一个类型 = 那几颗弹既不会伤敌机、
 *   也不会被置「免撞」位 ⇒ **会打到自机**）。
 * ⚠ **单靠类型不足以归属**（`0`/`2` 之类的原生敌人也在用）⇒ 调用方必须**再叠一层覆盖区过滤**
 *   （见 `pofv.c` 的 `bossatk_damage()`）。本函数只做「类型在不在集合里」这一件事，**便于单测**。*/
int pofv_bossatk_bullet_type_ours(uint32_t type);


/* ★ 2026-10-01（自机狙）：**只用 SSE 标量的 atan2 近似**。
 *
 * 为什么不用引擎的：`th18:0x4a94a0` 是 MSVC i387 版（结果在 `st(0)`）⇒ 调它我们的目标文件里
 * 必然出现 `fld/fstp`，被 `make dllx87` 闸门拦（实测 `x87 in our objects: 3`）。
 * 也不用 libm（本仓铁律）。⇒ 自己写一个多项式近似，**放在纯逻辑层**（主机可单测）。
 *
 * ❓ **自定（数学近似，非一手）**：精度目标是「瞄准够用」，实测误差 < 1e-4 rad（单测里拿
 *   `math.h` 的 `atan2` 对 0..2π 扫一遍对账）。
 * 返回值域 = `(-π, π]`，与引擎那条同口径。
 *
 * ⚠ **不返回 float**（i386 上 float 返回值走 `st0` ⇒ 目标文件里出现 `flds`，踩 `dllx87` 闸门；
 *   实测：签名写成 `
float pofv_atan2(y, x)` 时闸门报 `x87 in our objects: 2`）
 *   ⇒ 结果经 `*out` 写出。这就是本仓「C 函数不返回浮点」那条铁律的具体来由之一。*/
void pofv_atan2(float y, float x, float *out);

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
 *   | 自机弹命中 | 0 | 1.6 | **70** | **112**（`th09:0x41fe93`–`0x41fea3`）|
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
#define POFV_STORM_MAX             16    /* ❓ 自定（原版是节点池；寿命改长后要更大的池，见下）*/
/* ★★★ 2026-10-01 owner 口径（第二版）：暴风改用**一手 TH09「自机弹命中」节点**
 *   （`r0 = 0` / `dr = 1.6` / 寿命 **70** 帧 ⇒ 终点 **112**，`th09:0x41fe93`–`0x41fea3`）
 *   ＋「**连爆档只放大范围**」（档不改前沿速度）。
 *   ★ 关键洞察（owner 原话：「连锁爆破在敌人链条上的移动速度和敌人链条本身移动的速度相当，
 *     之前几乎是瞬间连锁」）：场是**锚定**的、圈里碰到就炸 ⇒ **连锁前沿的移动速度就是 `dr`**
 *     （px/帧）。旧的「16 → R_chain 固定 30 帧」是**自造**的（前沿 5.3–10 px/帧，比妖精快 3–5 倍
 *     ⇒ 瞬间连锁）。现在前沿 = **1.6 px/帧 ≈ 妖精爬行速度**。
 *   ★ 「只放大范围」怎么实现：`dr` 固定 1.6，**寿命按档给**（`life_k = R_k / dr`），
 *     于是档只决定「这个场最终能长到多大」——`R_k` 仍用一手锚定的 176/224/272/320
 *     （档 1 = 176 是一手：CA 释放节点 `16 + 20×8`，`th09:0x41e875`）。*/
#define POFV_STORM_R0               0.0f /* ★ 一手：「自机弹命中」节点的 r0 = 0 */
#define POFV_STORM_DR               1.6f /* ★ 一手：同一个节点的 dr = 1.6 px/帧（= 连锁前沿速度）*/
#define POFV_STORM_SCANS_PER_FRAME  4    /* ❓ 自定：每帧最多几次**整池扫弹**（O(弹池) 的预算闸）*/
#define POFV_STORM_DMG_LIFE         2    /* ❓ 自定（伤害源寿命；与旧 `POFV_BLAST_DMG_LIFE` 同值）*/

typedef struct {
    float   x, y;        /* 圆心（世界坐标）。★ 生成后**不再移动** —— 这就是「相对世界静止」*/
    int32_t chain;       /* 连爆档 1..POFV_BLAST_CHAIN_MAX */
    int32_t age;         /* 已经活了几帧（0 = 出生帧）*/
    int32_t life;        /* 寿命帧数（到点释放）*/
    int32_t active;
    /* ★★ 2026-10-01（owner「**消弹圈是逐帧扩张的场**」）：场**不再只有暴风一种** ——
     *   蓄力攻击的红圈也是同一个场（一手 L1/L2/L3 = 0/4.0/48、0/4.0/64、0/3.5/128），
     *   甚至「松手那一下」那个 16/20/8 → 176 也是。⇒ 每个槽自带 `r0`/`dr` + `kind`。*/
    float   r0;          /* 起始半径（暴风 = POFV_STORM_R0；CA 圈 = 一手各自的值）*/
    float   dr;          /* 每帧增量（暴风 = POFV_STORM_DR；CA 圈 = 4.0 / 4.0 / 3.5 / 20）*/
    int32_t kind;        /* pofv_storm_kind_t */
} pofv_storm_t;

typedef enum {
    POFV_STORM_KIND_STORM = 0,   /* 暴风（口径 4/6：消白弹掉灵力道具 + 连爆 + 能打死灵）*/
    POFV_STORM_KIND_CA    = 1    /* ★ 蓄力攻击的红圈（一手：消**所有**弹、**不**连爆、**不**掉道具）*/
} pofv_storm_kind_t;

typedef struct {
    pofv_storm_t s[POFV_STORM_MAX];
    int32_t live;
    int32_t dropped;     /* 池满丢掉的暴风数（日志 / 调参用；丢 = 少一次场，不崩）*/
} pofv_storm_pool_t;

void pofv_storm_pool_init(pofv_storm_pool_t *p);

/* 起一个暴风（O(1)：死亡回调里只准做这个）。返回槽号，−1 = 池满（丢弃）。 */
int  pofv_storm_spawn(pofv_storm_pool_t *p, float x, float y, int32_t chain);

/* 起一个**任意场**（r0/dr/寿命/kind 全给）—— 暴风与 CA 红圈共用这一个入口。−1 = 池满。 */
int  pofv_storm_spawn_field(pofv_storm_pool_t *p, float x, float y,
                            float r0, float dr, int32_t life, int32_t kind);

/* ★★ 蓄力攻击的扩张圈（一手 `0x403c30`/`0x403c90`/`0x403cf0` ＋「松手那一下」`0x41e875`）：
 *   `level` = **0 = 「松手那一下」**（16 / 20 / 8 → 176，**任何一次松手都起**）；
 *   `level` = 1..4 = 该档的圈（1: 0/4.0/48→192；2: 0/4.0/64→256；3·4: 0/3.5/128→448）。
 *   ⇒ owner：「消弹圈是**逐帧扩张的场**」——**不是**一次性按终点半径消一圈。
 *   返回槽号，−1 = 池满。 */
int  pofv_ca_field_start(pofv_storm_pool_t *p, float x, float y, int level);

/* 「松手那一下」的一手参数（`0x41e875`–`0x41e88c`）。 */
#define POFV_CA_BURST_R0      16.0f
#define POFV_CA_BURST_DR      20.0f
#define POFV_CA_BURST_LIFE        8

/* 档 L 的圈参数（一手；L ≥ 3 同一支 = L3）。 */
void pofv_ca_field_params(int level, float *r0, float *dr, int32_t *life);

/* 该场**是不是**暴风（`pofv_storm_chain_at()` 只认暴风；CA 圈不参与连爆）。 */
int  pofv_storm_is_storm(const pofv_storm_t *st);

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

/* ★ 连爆档 k 的**寿命帧数** = `(R_k − r0) / dr`（dr 固定 ⇒ 档只放大范围，不改前沿速度）。
 * 越界钳到 1..`POFV_BLAST_CHAIN_MAX`。 */
int32_t pofv_storm_life_for_chain(int32_t chain);

/* ★★ **TH09 的节点律**（一手 §1.3：每帧 `r += dr`、寿命到点释放）——**场与结界的锚定场共用这一个律**
 *   —— owner 2026-10-01：「th09 和 th19 的那个 aura 和锚定场**本质上是同一个东西**」。
 *   `r = r0 + dr × clamp(age, 0, life)`；`life <= 0` 或 `age` 非法时按钳位算。 */
void pofv_node_radius(float r0, float dr, int32_t age, int32_t life, float *out);

/* 连爆档 chain 的**终点半径** R。★ 档 1 = **176**（一手：TH09 的 CA 释放节点 16 + 20×8 = 176，
 * `th09:0x41e875`）—— 我们把它当最小暴风的终点半径；档 2..4 每档 +48 ❓ 自定（TODO(th09)）。 */
void pofv_storm_final_radius(int32_t chain, float *out);

/* 兼容旧名（旧实现的"爆风半径表"就叫 `pofv_blast_radius`；语义现在 = 该档暴风的终点半径）。*/
void pofv_blast_radius(int chain, float *out);

/* ★★★ 2026-10-01 owner 第四 / 第五条口径：**暴风也按自机角色取哪一边** ——
 *   「**th09 暴风可以直接打死 th09 幽灵**」「不过似乎 **th19 没有暴风连锁，只是暴风消弹**」。
 *   与结界同一套分派（咲夜取 TH09 / 早苗取 TH19 / 灵梦·魔理沙待 owner）。 */
typedef struct {
    int32_t chain;        /* 1 = **连爆**（TH09：击破 ⇒ 新场的档 +1）；0 = **不连锁**（TH19）*/
    int32_t kills_ghosts; /* 1 = 暴风**能直接打死灵**（TH09）；0 = **只消弹**、不给敌人伤害（TH19）*/
} pofv_storm_profile_t;

/* 按 TH18 角色号取暴风档案（未识别 / 待定 ⇒ 回落到 TH09 档 = 改造前行为）。 */
void pofv_storm_profile_for(int character, pofv_storm_profile_t *out);

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
#define POFV_POWER_FALL               0.5f  /* ❓ 自定（飞散结束后的飘落速度 px/帧）*/
/* ★★★ 2026-10-01（owner：「**灵力道具得按照类季节道具做，动态什么的一模一样**」）：
 *   照 TH16 季节道具（= 116 `cards/season.c` 那份一手复刻）的**出生/飞散动态**：
 *     · **出生先「待发射」**：错帧延时 = `total_items_created & 3` 帧（TH16 `0x430a0e` 段，
 *       `item+0xc50` 在延时里是 6、结束后才 `mov [item+0x530], 0xffffffff` 去建 VM）；
 *     · 然后**直线飞散**：`item+0xc24` = 飞散角（**整个飞散期不变**），
 *       `item+0xc20` = 剩余速度，**每帧 −0.03**（TH16 `0x42f54e` / `0x430c90`）。*/
#define POFV_POWER_SCATTER_DELAY_MASK   3    /* ★ 一手：TH16 的错帧 = `& 3`（0..3 帧）*/
#define POFV_POWER_DECEL              0.03f  /* ★ 一手：TH16 `item+0xc20` 每帧 −0.03 */
#define POFV_POWER_SCATTER_SPEED       2.2f  /* ❓ 自定（初速）：一手候选 = 原生 main-drop 实参 2.2
                                              *   （`cards/collect2x.c` 的注释：P/钱 与碎片卡都是 2.2）；
                                              *   TH16 季节道具那边是 1.2（`[0x494510]`）。取 2.2 更飘得开。*/
#define POFV_POWER_SCATTER_DIRS          8    /* ❓ 自定：飞散方向用 **8 向单位向量表**近似
                                              *   （一手只给「角在飞散期不变」，没给角的分布；
                                              *     用表 ⇒ **不引 libm**，也够表达四散的观感）*/
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
    /* ★ 2026-10-01：照季节道具的**出生/飞散**动态（见上面常量段的注释）*/
    int32_t delay;       /* 待发射的错帧延时（> 0 时本帧不动）*/
    float   dx, dy;      /* 飞散方向的单位向量（= TH16 `item+0xc24` 那个角）*/
    float   lr;          /* ★ 剩余速度（= TH16 `item+0xc20`，每帧 −`POFV_POWER_DECEL`）*/
} pofv_power_t;

typedef struct {
    pofv_power_t s[POFV_POWER_MAX];
    int32_t seq;         /* ★ 出生序号（= TH16 的 `total_items_created`，给待发射错帧用）*/
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
 * 半径（★ 一手 TH19 §7.2 的公式）：`clamp(S+0x8c,0,200) × K/100`，**K 逐类**（Reimu 64 /
 *   Sanae 128 / …）。以 TH19 的尺寸为锚，本卡取**常驻**半径 `POFV_AURA_R = 96`（❓ 自定：
 *   落在 Reimu 的 0..128 区间内）。
 *   ★ 一手性质照抄：**判定半径 = 绘制半径**（TH19 §7.1/§9.2，`aura+0x9c` 同时喂槽 7 与
 *     VM 的 `scale.x/y`）⇒ 卡侧的结界 VM 每帧写 `scale =` 本帧的判定半径。
 *
 * ★★★ 2026-10-01 **owner 追加口径**：结界**按自机角色取不同游戏的版本** ——
 *   「固灵结界 **咲夜取 th09，早苗取 th19**，灵梦和魔理沙**待定**」＋（追问早苗的半径与
 *   口径 4「常驻」冲突时）「**那你就把 shift 时长当作某种蓄力**」。落地成一张**逐角色档案表**：
 *   · **咲夜**（TH18 角色号 2）→ **TH09 的「消弹圈 = 锚定场」**（一手 §1.4 的「CA 释放」节点：
 *     r₀ = 16 / dr = 20 / 寿命 8 帧 / 终点 176，`th09:0x41e875`）：**圆心锚定在出生点**、
 *     逐帧扩张、寿命到点**在原地（或自机处）重新起一个**（TH09 的节点本来就是「建 → 逐帧长 → 到点释放」）。
 *   · **早苗**（TH18 角色号 3）→ **TH19 `AuraSanaeInf`**：圆 `dx²+dy² <= (r+pad)²`（§7.3 的 6 类共用支）、
 *     `K = 128`、视觉 = `pl02.anm` script16 的 `drawPoly(1.0,64)`；半径的「蓄力量」= **按住 Shift 的帧数**
 *     （owner 原话；❓ 每帧涨多少见 `POFV_AURA_SHIFT_CHARGE_PER_FRAME`）⇒ 满档 = 200 ⇒ r = 256。
 *   · **灵梦 / 魔理沙**（0 / 1）→ **owner 待定** ⇒ 先沿用 TH19 `AuraReimuInf` 的**常驻圆 96**
 *     （= 本卡原来的行为，零变化）作**占位**，登记 `TUNING.md` §2.0v-2。 */

#define POFV_AURA_R                 96.0f  /* ❓ 占位（灵梦 / 魔理沙待 owner；一手 Reimu 公式 0..128 区间内）*/
#define POFV_AURA_ITEMS_PER_FRAME    4     /* ❓ 自定（口径 6 的「每帧上限」；与消弹源同一把闸）*/

/* ---- 逐角色档案（owner 2026-10-01 口径；卡侧按 `CE_ADDR_CHARACTER` 选）---- */

typedef enum {
    POFV_AURA_STATIC = 0,   /* 常驻圆（TH19 系）：圆心 = 自机、半径按档案算 */
    POFV_AURA_FIELD  = 1    /* TH09 的**锚定场**：圆心锚定、逐帧扩张、有限寿命后重生 */
} pofv_aura_mode_t;

/* TH09「CA 释放」节点的一手参数（`engine/card/th09/01…` §1.4 / `th09:0x41e875`）。*/
#define POFV_AURA_TH09_R0            16.0f
#define POFV_AURA_TH09_DR            20.0f
#define POFV_AURA_TH09_LIFE              8

/* TH19 的蓄力量量程与 `AuraSanaeInf` 的 K（一手 §2/§7.2）。*/
#define POFV_AURA_CHARGE_MAX           200
#define POFV_AURA_SANAE_K            128.0f
/* 一手：TH19 `AuraSanaeInf` 的动画器时长 `+0x94` = **60**（§7.2）⇒ 从 0 长到满档目标用 60 帧。*/
#define POFV_AURA_SANAE_GROW_FRAMES     60

/* ❓ 自定（owner 只说「把 shift 时长当作某种蓄力」，没给换算）：按住 Shift 每 1 帧涨多少蓄力。
 * 取 **2** ⇒ 100 帧（≈1.67 s）涨满 200 ⇒ 早苗的结界从 0 长到 256。 */
#define POFV_AURA_SHIFT_CHARGE_PER_FRAME  2

/* ★★ 2026-10-01（owner 第二点）：「th09 和 th19 的那个 aura 和锚定场**本质上是同一个东西**」
 *   ⇒ 两种形态**共用同一个「半径随帧演化」的模型**（都靠 `dr` = px/帧 长），差别只有：
 *     · **起点/寿命**（TH09 节点：`r0 + dr×age`，到寿命释放）；
 *     · **上限从哪来**（TH19：上限 = 蓄力折算的目标半径，长到就停）。
 *   实现上就是 `pofv_node_radius()` 一个函数 + 一支「有上限」的分支（见 `pofv_aura_radius`）。*/
typedef struct {
    int32_t mode;        /* pofv_aura_mode_t */
    int32_t pad_linear;  /* 几何支：0 = `dx²+dy² < r²+pad²`（Reimu 支）；1 = `<= (r+pad)²`（Sanae 支）*/
    float   r_fixed;     /* STATIC 且不用蓄力时的半径（≤0 = 由蓄力算）*/
    float   k;           /* STATIC 且用蓄力时：目标半径 = `clamp(charge,0,200) × k/100`（≤0 = 恒用 r_fixed）*/
    float   dr;          /* **半径生长速度（px/帧）**：FIELD = 一手节点的 dr；STATIC+蓄力 = 满档目标 / grow_frames */
    float   r0;          /* FIELD：起始半径 */
    int32_t life;        /* FIELD：寿命帧（到点重生）*/
    int32_t grow_frames; /* STATIC+蓄力：从 0 长到**满档目标**用多少帧（一手 TH19 `AuraSanaeInf` 的 `+0x94` = 60）*/
} pofv_aura_profile_t;

/* 按 TH18 的**角色号**（0 灵梦 / 1 魔理沙 / 2 咲夜 / 3 早苗）取结界档案。
 * 未识别 / 待定的角色一律回落到「常驻圆 96」那一档（= 改造前的行为）。 */
void pofv_aura_profile_for(int character, pofv_aura_profile_t *out);

/* 「按住 Shift 的帧数」→ TH19 的蓄力量 0..`POFV_AURA_CHARGE_MAX`（owner 口径）。 */
int32_t pofv_aura_shift_charge(int32_t shift_frames);

/* 本帧的**判定半径**（也是绘制半径）——两种形态共用「按 `dr` 长」的模型：
 *   FIELD  ：`r = r0 + dr × clamp(age,0,life)`（= `pofv_node_radius`；TH09 的节点律）。
 *   STATIC ：`k > 0` ⇒ `r = min(dr × age, clamp(charge,0,200) × k/100)`（**长到蓄力给的上限就停**）；
 *             否则 `r = r_fixed`（占位档）。
 * `age` = 这个结界已经长了多少帧（调用方维护；蓄力归零时应把它清 0 ⇒ 结界缩回没有）。
 * `*out` 恒被写（半径 ≤ 0 表示「结界没展开」）。 */
void pofv_aura_radius(const pofv_aura_profile_t *p, int32_t age, int32_t charge, float *out);

/* 结界判定 —— **两种几何各一支**（一手 TH19 §7.3 的两支）：
 *   `pad_linear = 0` ⇒ Reimu 支 `dx² + dy² < r² + pad²`（`th19:0x416f10`）；
 *   `pad_linear = 1` ⇒ Sanae 支 `dx² + dy² <= (r + pad)²`（`th19:0x418690`，6 类共用）。
 * `r <= 0` ⇒ 永不命中（结界没展开）。 */
int pofv_aura_hit_ex(float dx, float dy, float r, float pad, int pad_linear);

/* 旧签名（= Reimu 支的包装）：`pofv_aura_hit(dx,dy,r,pad) == pofv_aura_hit_ex(dx,dy,r,pad,0)`。 */
int pofv_aura_hit(float dx, float dy, float r, float pad);

/* ==================================================================== H. オートパリア（抵挡护盾）
 *
 * owner 2026-10-01 甜头口径：「**按照 th19 口径，蓄力到 C3 or C4 可以给一个抵挡的护盾，
 *   保命等级最优先，无论如何先消耗它**」。
 *
 * ★ 一手（TH19，`engine/versus/th19/05-four-mechanics.md` §5 / `02-self-field-systems.md` §4）：
 *   · バリア是**独立对象**，状态 `[0]`：1 = 有効 / 2 = 破壊中 / 3 = 无（`0x544be0`）。
 *   · **命中先吃它**：被弹处理 `FUN_00549570` 开头就是
 *     `if (PlayerInf+0x18 == 1 && FUN_00544c90(barrier) != 0) return;`（`0x549570`）
 *     ⇒ 有盾时**这一发根本不进「真正的被弹处理」**。
 *   · **挡 1 发**：`FUN_00544c90` 开头 `if (*p != 1) return 0;` ⇒ 第一发把它打成 `2`，第二发直接放行。
 *   · **怎么回复**：`FUN_0040b420`（`0x40b420`）把 `2 → 1`，而它的**唯二调用点**
 *     = `0x4e7b8d`（**阶段 4 ボスアタック**）/ `0x4e8176`（**阶段 3 EXスペシャル**）
 *     ⇒ **只有 C3 / C4 的蓄力攻击发动才回复**（Manual「EXスペシャルアタックかボスアタックを
 *     発動すると復活」逐条对上）。
 *
 * ⇒ 本卡的落地（**按一手**）：蓄力攻击在 **C3 / C4** 发动 ⇒ `shield = 1`（破了以后要再发动一次
 *   才能拿回来）；中弹（救援层）时**先吃盾**。
 *
 * ❓ 自定（本卡没有 TH19 那套「命中的前处理」，只能在**救援层**（虚表 `+0x10`，死亡已发起）里
 *   等价成「吃盾 + 取消这次死亡」）：吸收后给的**无敌帧数**（`POFV_SHIELD_INVULN_FRAMES`）与
 *   **音效**（TH19 是 SE `0x1b`；TH18 的表里没有同一枚）都登记在 `TUNING.md`。
 *   ★ 「保命等级最优先 / 无论如何先消耗它」在这里 = **挂最早的那一层（`+0x10`，比决死 B 与 113 的
 *     `+0x0c` 都早）**、且**无条件吃**（不与其他救援者抢，也不让位）。 */

#define POFV_SHIELD_ARM_LEVEL        3     /* ★ 一手：`0x40b420` 的唯二调用点 = 阶段 3 / 4 */
#define POFV_SHIELD_INVULN_FRAMES   40     /* ❓ 自定（与 107 结界 / 117 霊撃 同一档 40 帧）*/

/* 蓄力攻击发动（`level` = 1..4）：**C3/C4 才**把盾（重新）架上。
 * 返回 1 = 本次真的从「无」变成「有」（卡侧据此打一行日志 / 演出），0 = 没变。 */
int  pofv_shield_arm(pofv_state_t *st, int ca_level);

/* 中弹：**先吃盾**。返回 1 = 这一发被吃掉（卡侧接着取消死亡），0 = 没盾（什么都不做）。 */
int  pofv_shield_take(pofv_state_t *st);

/* 诊断 / 单测：盾在不在。 */
int  pofv_shield_up(const pofv_state_t *st);

/* ==================================================================== I. ボスアタック
 *
 * owner 2026-10-01 口径（逐字）：「C3 C4 能不能做一个**把自己变成 boss** 的 boss attack？
 *   也就是自己会额外发射如对战作**送到对面的攻击**，但是这些攻击**是会对敌人造成伤害的**」；
 *   「**C3 也有**送到对面的额外攻击——只不过自己**不是** boss」；
 *   「这些弹幕得**往"上"打**，版顶=版底」「**也可以画在自己这边，但是是对敌人造成伤害**」；
 *   「**C4 确实是一个不断扩张的消弹圈**」；「最好**观感一致**」。
 *
 * ★ 一手（TH09 `0x41f999` 那一支，见 `engine/card/th09/01` §1.4 与 `engine/versus/th09/01` §2.3）：
 *   · 门槛 = 蓄力量 **≥400** 且场上**没有 kind-3 boss**（`0x41f984` → `0x40f7d0(EM,3)`）；
 *   · `0x403cf0` = **CA L3 的消弹圈**（r0 0 / dr 3.5 / 寿命 128 → **448**，**逐帧扩张**）；
 *   · `0x403e50(2,2,0,&field)` = 送攻击（A 敌机 / B `fairy_bullets` / C 演出）；
 *   · `0x41f310(300.0f)` = 消耗；`0x401500(8)` = 8 帧计时。
 *   ⇒ 在**发动者自己**这边就是「一个**不断扩张**的消弹圈 + 一路往上打的弹幕」。
 *
 * ✅ owner 已拍：**C4 的圈就是那个扩张圈（不是永久场）**；`0x417630` 那支 500/30000 的永久场
 *   **排除出 C4**。 */

/* ★★★ 2026-10-01 owner 把四档的**取向**分开了：
 *   「**C1 主要是打伤害，C2 也算是打伤害，C3 C4 是打对面**」
 *   ＋「C1 和 C2 本来就是自己攻击己方敌人的呀」＋「C3……只不过自己不是 boss」。
 *   ⇒ **C1 / C2 = 伤害取向**（向上弹幕，少而重：1/2 路、单发 24/18）；
 *     **C3 / C4 = 打对面取向**（符卡/非符那一路的观感：3/5 路、扇面 ±0.14/±0.28、单发 12/10）；
 *     **C4 = 自己是 boss**（`pofv_bossatk_is_boss()`），C1–C3 不是。
 *   ⚠ 四档**都**是「从自机往上打的原生自机弹幕」（owner：「这些弹幕得往"上"打」）。*/
#define POFV_BOSSATK_MIN_LEVEL        1   /* ★ owner：C1 起就有（打自己场上的敌人）*/
#define POFV_BOSSATK_C1_FRAMES       45   /* ❓ 自定（档越低打得越短）*/
#define POFV_BOSSATK_C2_FRAMES       60   /* ❓ 自定 */
#define POFV_BOSSATK_C3_FRAMES       90   /* ❓ 自定 */
#define POFV_BOSSATK_C4_FRAMES      150   /* ❓ 自定（最长 + 自己是 boss）*/

typedef struct {
    int32_t active;   /* 1 = 正在打 */
    int32_t left;     /* 剩余帧 */
    int32_t level;    /* 起手时的蓄力档（3 / 4）*/
} pofv_bossatk_t;

/* 蓄力攻击发动（`ca_level` = 1..4）：**C3 / C4** 才起手（低于 3 不做）。
 * 返回 1 = 本次真的起手（卡侧据此起子机 / 打日志），0 = 没起。 */
int  pofv_bossatk_start(pofv_bossatk_t *b, int ca_level);

/* 每帧推进：返回 1 = **本帧要开火**（还没打完），0 = 没在打 / 刚打完。 */
int  pofv_bossatk_tick(pofv_bossatk_t *b);

/* 收手（删卡 / 换关 / 真死都调）。 */
void pofv_bossatk_stop(pofv_bossatk_t *b);

/* 「自己是不是 boss」= 起手档是 **C4**（owner：「C3……只不过自己不是 boss」）。 */
int  pofv_bossatk_is_boss(const pofv_bossatk_t *b);

/* ==================================================================== G. 灵（自制**真敌人**）
 *
 * owner 口径 8/9/10：**两种灵都自建**（不借原版敌人、不借那两作的 ECL）、
 *   **按周期循环撒**、**任何 boss（含道中）在场就不撒**、
 *   本体 = **TH12 UFO 型真敌人**（`ce_spawn_enemy()` + 我们自己注册进 `default.ecl` 的子程序）。
 *
 * 本节只放**纯逻辑**：句柄池 + 循环撒的节拍 + boss 门 + 沈静化的计时。
 *   真正的 `ce_spawn_enemy()` / `ce_enemy_by_id()` / ECL 变量写读在 `pofv.c`（卡侧）。
 *
 * ⚠ **旧的自建活化灵池（`pofv_spirit_*`）已在 2026-10-01 整条撤掉**：
 *   它（卡自己算位置、Shift 领域吸收、HP 减半、自爆 3 颗）是口径 10 之前的实现；
 *   口径 10 定「灵的本体 = TH12 UFO 型真敌人」⇒ 本节是真敌人载体，旧池不再存在。
 *   撤除的一手交代（哪些 TH09 事实因此不再被复刻）登记在 `TUNING.md` §2.0v-1。
 *
 * 数值出处：
 *   · 同屏上限 = **4** —— ★ 一手 TH09：`initialize_vector_without_destructor(player + 0x30454,
 *     0x2c4, **4**, …)`（`HANDOFF-109.md` §5；TH19 那边是数据驱动、拔不到常数）；
 *   · 循环周期 = `POFV_GHOST_CYCLE_FRAMES` = **600** ❓ owner 2026-10-01 拍（`TUNING.md` §2.0v-1 #13）；
 *   · 耐久：enable 侧 `400 + I0*400`、沈静化后 `50 + I0*10` —— ✅ 一手 TH19（`06` §10.3，
 *     `@GhostPureHP_E` / `@GhostPureHP_D`）；本卡 I0 = 0 ⇒ **400 / 50**（❓ 本卡取值）；
 *   · 成佛计时 = **160** 帧 —— 🟡 一手 TH19 `GhostPureOrinDisable` 的 `wait(100)` + `wait(60)`
 *     （⚠ 一手**没有**自毁指令，见 `06` §10.4；本卡把它当「自然成佛」的时长，登记 ❓）。*/

#define POFV_GHOST_MAX              4      /* ★ 一手 TH09 的槽数 = 4（同屏上限）*/
#define POFV_GHOST_CYCLE_FRAMES   600      /* ❓ 自定（owner 2026-10-01 拍）*/
#define POFV_GHOST_CALM_FRAMES    160      /* 🟡 一手 TH19 的 wait(100)+wait(60) */
#define POFV_GHOST_HP             400      /* ❓ 本卡取值（TH19 enable 侧 400 + I0*400，I0 = 0）*/
#define POFV_GHOST_HP_CALM         50      /* ✅ 一手 TH19 沈静化后 50 + I0*10（I0 = 0）*/
#define POFV_GHOST_PTS_KILL        20      /* ❓ 自定（击破一只灵掉的灵力道具面值；同 §E 的击破源）*/
#define POFV_GHOST_DROP_ITEMS       4      /* ❓ 自定（击破一只灵掉**几颗**：口径 6 的「击破掉一把」）*/

typedef enum {
    POFV_GHOST_KIND_WOLF  = 0,   /* TH19 三族：狼 */
    POFV_GHOST_KIND_OTTER = 1,   /* 獺 */
    POFV_GHOST_KIND_EAGLE = 2,   /* 鷲 */
    POFV_GHOST_KIND_HYZ   = 3,   /* TH09 的霊（口径 8：两种都自建）*/
    POFV_GHOST_KIND_MAX   = 4
} pofv_ghost_kind_t;

typedef struct {
    int32_t active;
    int32_t enemy_id;    /* 引擎分配的 enemy_id（每帧 `ce_enemy_by_id` 找；**不缓存指针**）*/
    int32_t kind;        /* pofv_ghost_kind_t */
    int32_t calmed;      /* 1 = 已被固灵结界沈静化（耐久已骤降、停住）*/
    int32_t calm_age;    /* 沈静化后经过帧数；到 `POFV_GHOST_CALM_FRAMES` ⇒ 自然成佛 */
    int32_t age;
    float   x, y;        /* 上一帧从引擎读到的位置（诊断 / 结界判定用）*/
} pofv_ghost_t;

typedef struct {
    pofv_ghost_t s[POFV_GHOST_MAX];
    int32_t live;
    int32_t cycle;       /* 距离下一次该撒还有几帧（0 = 本帧就够条件）*/
    int32_t next_kind;   /* 下一只的族（四族轮转）*/
    int32_t spawned, killed, calmed_total, expired;   /* 诊断账 */
} pofv_ghost_pool_t;

void pofv_ghost_pool_init(pofv_ghost_pool_t *p);

/* 循环撒的节拍：每帧调一次。返回 1 = **本帧该撒一只**（`*out_kind` = 族），0 = 不撒。
 *   门（口径 9）：`boss_present`（任何 boss 在场）或 `live >= POFV_GHOST_MAX` ⇒ 不撒，
 *   且**不重置**倒计时（下一帧够条件就撒）—— 这就是「boss 在场就不撒」。 */
int  pofv_ghost_cycle_tick(pofv_ghost_pool_t *p, int boss_present, int32_t *out_kind);

/* 记一只撒出去的灵（`enemy_id` = `ce_spawn_enemy()` 返回值的 `+CE_ENEMY_ID`）。返回槽号，−1 = 池满。 */
int  pofv_ghost_register(pofv_ghost_pool_t *p, int32_t enemy_id, int32_t kind);

/* 按 enemy_id 找槽（找不到 / 空 id 返回 −1）。 */
int  pofv_ghost_find(const pofv_ghost_pool_t *p, int32_t enemy_id);

/* 沈静化一只（**幂等**：已沈静化的不重置计时）。返回 1 = 本次真的沈静化了。 */
int  pofv_ghost_calm(pofv_ghost_pool_t *p, int slot);

/* 每帧推进：`age++`；已沈静化的 `calm_age++`，到 `POFV_GHOST_CALM_FRAMES` 的写进 `out_slots`
 * （调用方接着做「成佛」：ECL 侧自删 + 卡侧释放槽）并**就地释放**。返回本帧成佛只数。 */
int  pofv_ghost_tick(pofv_ghost_pool_t *p, int32_t *out_slots, int cap);

/* 释放一个槽（敌人被击破 / 被清场 / 成佛 / 删卡）。返回 1 = 真的释放了。 */
int  pofv_ghost_release(pofv_ghost_pool_t *p, int slot);

/* 清空（`clear_card()` 用；返回清掉的只数）。 */
int  pofv_ghost_clear(pofv_ghost_pool_t *p);
