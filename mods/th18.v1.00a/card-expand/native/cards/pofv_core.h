/* pofv_core.h —— 花映塚 × 獣王園 合成卡（card-expand id 109）的**纯逻辑**之一：**暴风圈**。
 *
 * 纯 C11、**不含任何引擎地址**（引擎面全在 `pofv.c`）；主机单测 `tests/test_pofv_core.c`。
 *
 * ══ ★★★ 一手 = **TH19 的「圈」**（2026-10-05 静态核死，见
 *    `engine/versus/th19/06-aura.md` §7.1/§8.5/§9.2/§9.3 + 本轮新增的 §10）════════════
 * 1) **形状（脚本一手）**：逐角色脚本（`pl00.anm` script15 / `pl02` script16 / `pl05` script10 …）逐字是
 *    `sprite(-1); layer(6); drawPoly(1.0f, 64); color(..)/color2(..); alpha(..)/alpha2(..);
 *     angleVel(-0.017453292f,0,-0.017453292f); stop();`
 *    ⇒ **不用贴图、64 边实心盘、两段顶点色（径向渐变）、自转 −1°/帧**；`stop()` ⇒ 尺寸/位置全由外部每帧写。
 *    ★ 半径 = **VM 的 scale**（`0x416ea0/0x416ea5: movss [vm+0x54]/[vm+0x58], 半径`）。
 * 2) **半径的律（代码一手）**：`aura+0x9c` = 动画器 `FUN_00402c30(&aura+0x6c)` 的每帧输出；同一量既写
 *    VM 的 scale（绘制）又写 `aura+0x9c`（判定）⇒ **判定 = 绘制** ✓
 *    目标 `aura+0x70 = clamp(蓄力, 0, 200) × K / 100`（K 逐类：Reimu 64 / Sanae 128 / Toutetsu 600 …）
 *    时长 `aura+0x94`（Reimu **20** / Sanae 60 …）；模式 `aura+0x98`（Reimu/Sanae/Nazrin… = **4**）
 *    初值 `aura+0x6c = 1.0f`（Toutetsu 16.0f）
 * 3) **扩张律（ease 一手）**：`FUN_00402c30` 的通用支 = `out = (target − cur) × ease(模式, f) + cur`，
 *    `f = 计时/时长`；`ease` = `FUN_004d5e10`（`cmp ecx,0x1f` + `jmp [ecx*4 + 0x4d66e0]`，32 条曲线）——
 *      · mode 0/7/8/17 → `xmm0 = f`（线性）
 *      · mode 1 → f²、mode 2 → f³（ease-in）
 *      · **mode 4 → `1 − (1−f)²`（ease-out 二次）** ★ 本卡用这档
 *    ⇒ **`半径(f) = 初值 + (目标 − 初值) × (1 − (1−f)²)`**，走完时长后**保持**在目标。
 * 5) 收尾：脚本 `interruptLabel(1); scaleTime(20, 4, 0, 0); +20: delete();`（20 帧缩到 0 再删）——
 *    卡侧到时发 **interrupt 1**（本卡沿用）。
 *
 * ⚠ float 一律**走出参**、不返回（i386 的 float 返回值走 x87 ST(0)，会被 `make dllx87` 拦）。
 */
#pragma once
#include <stdint.h>

#define POFV_STORM_MAX             16    /* ❓ 自定：同屏场上限（满 ⇒ 丢新场并计数，不崩）*/

/* ══ 半径律的**参数**：哪些是一手、哪些是我们自己的数 ═══════════════════════════════
 * ★★ owner 2026-10-05 纠正：「我什么时候说过暴风和蓄力有关」—— **没有任何关系**。
 *    TH19 的 `clamp(蓄力,0,200) × K/100`（K 逐类）是 **光环（オーラ）** 的公式 —— 光环天生挂在
 *    自机的蓄力槽上。**本卡不能搬那个公式**（暴风没有蓄力这个量），所以：
 *      · 能搬的 = **律的形状**：初值 / ease 曲线 / 时长 / 走完保持 —— 全是一手；
 *      · 不能搬的 = **目标半径** ⇒ 本卡**直接给半径**（❓自定，理由 + 手感，登记 TUNING）。
 *    （旧的 `POFV_STORM_K` / `POFV_STORM_CHARGE` 两个宏已删——它们是光环的量，不是暴风的。）*/
#define POFV_STORM_R0              1.0f  /* 一手：`vm+0x54` 初值 = 1.0（`ghost.anm` script17 的 `scale(1.0,1.0)`）*/
#define POFV_STORM_R_TARGET       50.0f  /* ❓**重新设计**（owner 2026-10-05）：半径取小妖精 B 的 `ins_1028` = 50.0（时长则照 A）*/
#define POFV_STORM_TWEEN_FRAMES     30   /* ❓**重新设计**（owner 2026-10-05）：时长照小妖精 A 的 `ins_1031` = 30 帧 */
#define POFV_STORM_EASE_MODE         5   /* ★ 一手：ECL `ins_1031` 的插值 `mode=5` = `1 − (1−f)³`
                                          *   （一手 `FUN_00402dd0` 通用支 + ease 表 `0x4d66e0` 第 5 项桩体 `0x4d5e74`）
                                          *   —— vector 插值布局：时长 `t[0xf]`、模式 `t[0x10]`；求值器 `FUN_00402dd0` */
#define POFV_STORM_LIFE_FRAMES      30   /* ❓ 自定：总寿命（扩张 20 帧 + 保持 + 收尾由脚本淡出）*/

typedef struct {
    float   x, y;        /* 圆心（世界坐标）。★ 一手：圈**停在原地**（只有 VM 的 scale/位置被写）*/
    float   target;      /* ★ 本场的目标半径（= 该档的 `ins_1028`）：小妖精 B = 50.0，灵 = 80.0 */
    int32_t tween;       /* ★ 本场的插值帧数（= 该档的 `ins_1031`）：小妖精 B = 50，灵 = 40 */
    int32_t dmg;         /* ★ 本场的伤害/帧（= 该档的 `ins_1029`）：小妖精 B = 6，灵 = 10 */
    int32_t age;         /* 已经活了几帧（0 = 出生帧）*/
    int32_t life;        /* 总寿命帧数（= `tween`；一手：寿命与插值时长同源）*/
    int32_t active;
} pofv_storm_t;

typedef struct {
    pofv_storm_t s[POFV_STORM_MAX];
    int32_t      live;
    int32_t      dropped;    /* 池满丢掉的场数（诊断；丢 = 少一次消弹，不崩）*/
} pofv_storm_pool_t;

void pofv_storm_pool_init(pofv_storm_pool_t *p);

/* 起一个场。返回槽号，−1 = 池满（丢弃）。**只多一次线性找空槽**。 */
int  pofv_storm_spawn(pofv_storm_pool_t *p, float x, float y, int32_t life);

/* ★ 指定档位起一个场（owner 2026-10-05：「除了灵，统一按照小妖精 B 来搞」）：
 *   · 默认（小妖精 B）= `POFV_STORM_R_TARGET` / `POFV_STORM_TWEEN_FRAMES`；
 *   · **灵档** = `POFV_SPIRIT_R_TARGET`(80) / `POFV_SPIRIT_TWEEN_FRAMES`(40) —— 由卡侧判定后走这条。 */
#define POFV_SPIRIT_R_TARGET       80.0f  /* 一手：纯灵·動物灵的 `ins_1028` */
#define POFV_SPIRIT_TWEEN_FRAMES      40  /* 一手：纯灵·動物灵的 `ins_1031`（帧）*/
#define POFV_SPIRIT_HIT_DMG           10  /* 一手：纯灵·動物灵的 `ins_1029`（伤害/帧）*/
#define POFV_STORM_HIT_DMG            10  /* ❓**重新设计**（owner 2026-10-05）：「伤害向 300 靠拢」
                                          *   ⇒ 总伤目标 300 ÷ 30 帧 = **10/帧**（一手 A 是 6/帧×30=180；
                                          *   一手 B 是 6/帧×50=300 —— 本档 = A 的时长 + B 的半径 + B 的总伤）*/
int  pofv_storm_spawn_tier(pofv_storm_pool_t *p, float x, float y, float target, int32_t tween, int32_t dmg);

/* 每帧推进：`age++`、到寿命的槽就地释放。返回本帧还活着的场数。 */
int  pofv_storm_tick(pofv_storm_pool_t *p);

/* 第 `i` 个槽；NULL = 越界；`active == 0` 表示空槽。 */
const pofv_storm_t *pofv_storm_slot(const pofv_storm_pool_t *p, int i);

/* ★ ease 曲线（一手 `FUN_004d5e10` 的 32 条表）。目前落地本卡用到的两条：
 *   mode 0/7/8/17 = 线性；mode 4 = `1 − (1−f)²`；**mode 5 = `1 − (1−f)³`（本卡）**。`f` 会被钳到 [0,1]。 */
void pofv_ease(uint32_t mode, float f, float *out);

/* 默认档的目标半径（= 小妖精 B 的 `ins_1028` = 50.0）。 */
void pofv_storm_target(float *out);

/* ★ 这个场**当前**的半径（一手扩张律）：
 *   `f = clamp(age, 0, +0x94) / +0x94`；`r = R0 + (target − R0) × ease(4, f)`。
 *   ⇒ 先快后慢、走完时长后保持在目标。判定与绘制共用这一个值。 */
void pofv_storm_radius(const pofv_storm_t *st, float *out);

/* ════════════════════════════════════════════════════════════ 灵（W1；见 `PLAN-109-SPIRITS.md`）
 * owner 2026-10-05：「只有**本卡召唤的**才是灵」⇒ 灵是**我们自己的对象**，不是引擎敌机
 *   （SDK 没有造敌机的原语 ⇒ `PLAN` §W1 的 A/B 选型走 **B**：卡侧自建）。
 * W1 的范围**故意最小**：灵能生成、会到期、**自然成仏**（一手：TH19 的自然成仏 ⇒ 变動物霊送出去）
 *   时按**灵档**起暴风 ⇒ 用「灵档 80/40/10」当验收点。
 * ⚠ 还没做（后续轮）：被自机弹打死的相交判定（要自机弹池）、外观 VM、上限 60/80/999、
 *   距离² 选种（4624/16384）、固化、TH09 阶梯、镜像记账、EX。 */
#define POFV_SPIRIT_POOL_MAX       16
#define POFV_SPIRIT_LIFE_FRAMES   300
#define POFV_SPIRIT_LIFE          900  /* 真敌机的 life（SDK 要求 < 1000；❓W1 临时 = 900 帧 ≈15 s）*/  /* ❓W1 临时：纯灵/動物灵的自然成仏计时（一手只给了「自然成仏」这条路，没给帧数）*/
typedef struct {
    float   x, y;        /* 世界坐标（死在原地 ⇒ 暴风也起在这里）*/
    int32_t kind;        /* W2 起：0 = 純霊、1..3 = 動物霊（狼/獭/鷲，按距离² 选）；W1 先恒 0 */
    int32_t level;       /* TH09 侧阶梯 1..3（W5）；W1 恒 1 */
    int32_t age, life;   /* 自然成仏计时：age ≥ life ⇒ 成仏 */
    int32_t active;
} pofv_spirit_t;

typedef struct {
    pofv_spirit_t s[POFV_SPIRIT_POOL_MAX];
    int32_t       live;
    int32_t       dropped;
} pofv_spirit_pool_t;

/* ★★★ W2：一手规则（`REPORT-109-AURA-AND-SPIRITS.md` §3.1 的镜像表）
 *   ① **每侧上限按比赛时长分档**：60 / 120 / 180 s ⇒ **60 / 80 / 999**；超限 ⇒ 改给 **100 分**。
 *   ② **动物灵种类 = 死亡点到自机距离²**：4624（68²）以内 ⇒ 狼；16384（128²）以内 ⇒ 獭；再远 ⇒ 鷲。 */
#define POFV_SPIRIT_D2_WOLF      4624.0f   /* 68²  ⇒ 狼  */
#define POFV_SPIRIT_D2_OTTER    16384.0f   /* 128² ⇒ 獭  */
/* ★ 五种灵（owner 口径）：TH19 四种 + TH09 幽灵（幽灵另有绿/黄/红三色变体，只算一种形状）。
 *   **同一套镜像映射与存储** ⇒ 这里只是**一个 kind 字段**，不另开表；族只在「判上限」时用。 */
enum { POFV_SPIRIT_KIND_PURE = 0, POFV_SPIRIT_KIND_WOLF = 1, POFV_SPIRIT_KIND_OTTER = 2,
       POFV_SPIRIT_KIND_EAGLE = 3, POFV_SPIRIT_KIND_GHOST_TH09 = 4,
       POFV_SPIRIT_KIND__COUNT = 5 };

/* kind ⇒ 族（**只给判上限用**：09 算 09、19 算 19）。 */
int pofv_spirit_family_of(int32_t kind);

/* ★ 入场运动（一手：`default.ecl` 的 `Ghost*Enable` 的 `ins_404(方向, 速度)`，难度项取 0）：
 *   狼 π/2 @1.1 · 獺 π/8 或 7π/8 @1.5（两只 L/R）· 鷲 π/2 @3.5 · 純霊 π/2 @1.5 · 幽灵 π/2 @1.0。
 *   出参：`*dir` = 方向（弧度）、`*spd` = 速度（单位/帧）。 */
void pofv_spirit_entry_motion(int32_t kind, int lr, float *dir, float *spd);

/* ★ 同一件事的**速度分量**形式（角都是固定常数 ⇒ 单位矢量预计算成常量，免 libm/免三角函数）：
 *   出参 `*vx` / `*vy` = 每帧位移（世界单位）。`lr` 只对獺有意义（0 = π/8 左、1 = 7π/8 右）。 */
void pofv_spirit_entry_vel(int32_t kind, int lr, float *vx, float *vy);

/* ★★★ 2026-10-08（owner 给一手）：**TH09 幽灵出生那一刻的角度抽取** —— 真幽灵与**对称幽灵**共用本函数 ✓
 *   一手（`FUN_0040d9e0`，owner 转述 `engine/versus/th09/02-ghost-and-seal.md` §3.31 ✓）：
 *     `u = rng % ((对手机体 != pl07) + 4)` ⇒ **0..3 或 0..4**；
 *     · `u == 0`（约 1/4~1/5）⇒ `角度 = atan2(自机 − 幽灵) ± π/8`（**±22.5° 散射** ⇒ 斜飞、且会朝自机拐 ✓）
 *     · 其余 ⇒ `角度 = π/2 ± π/200`（**±0.9°** ⇒ 看着就是正下方直落 ✓）
 *   ⚠ 角度约定 = 本引擎的 `moveVel(角度, 速度)`：**0 = +x、π/2 = +y（正下方）** ✓（ECL 一手 ✓）。
 *   ❓ `pl07` 是哪台机体**未对上** ⇒ 本卡取**分母 5**（= `!=pl07` 那一支 ✓）并登记 ❓。
 *   ❓ 两个 `rand_neg_x_to_x` 的一手分布未逐字核 ⇒ 这里取 **均匀 ±**（`r2` 映射到 [−1,1] ✓）。
 *   纯函数（无全局/无浮点副作用）⇒ **主机可单测** ✓。*/
void pofv_ghost_spawn_angle(float sx, float sy, float tx, float ty,
                            uint32_t r1, uint32_t r2, float *out_angle);

/* ★ 带时间的入场速度（一手 `ins_405(帧数, 延迟, 方向, 终速)` 的**加速段**）：
 *   鷲 = `ins_405(60, 0, π/2, 3.5)` ⇒ age ∈ [0,60] 线性从 0 涨到 3.5，之后恒 3.5；
 *   純霊 = `ins_405(30, 0, π/2, 1.5)` ⇒ 同形（终速 1.5，30 帧）；
 *   狼 / 獺 / 幽灵 一手没给 `ins_405` ⇒ **匀速**（= `ins_404` 的速度）。 */
void pofv_spirit_entry_vel_at(int32_t kind, int lr, int32_t age, float *vx, float *vy);

/* ★★ owner 2026-10-05 定盘：**五种灵都算「灵」、走同一套镜像存储**；
 *   **只有「判上限」分族** —— 「09 算 09、19 算 19」。 */
int pofv_spirit_cap(int32_t round_seconds);       /* TH19 档：60/120/180 s ⇒ 60/80/999 */
int pofv_spirit_cap_for(int32_t family, int32_t round_seconds);   /* ★ 按族取上限（TH09 = **25（送）/ 10（落地）** ❓ / TH19 = 上面那档）*/
int pofv_spirit_room(int32_t live, int32_t cap);  /* 1 = 还能生成 */
int pofv_spirit_kind_from_dist2(float d2);        /* ⇒ 1 狼 / 2 獭 / 3 鷲 */

void pofv_spirit_pool_init(pofv_spirit_pool_t *p);
int  pofv_spirit_spawn(pofv_spirit_pool_t *p, float x, float y, int32_t kind, int32_t life);
/* 推进一帧；**返回本帧自然成仏的槽号**（−1 = 没有）⇒ 卡侧据此起灵档暴风（一手：自然成仏 = 死）。 */
int  pofv_spirit_tick(pofv_spirit_pool_t *p);
const pofv_spirit_t *pofv_spirit_slot(const pofv_spirit_pool_t *p, int i);

/* ════════════════════════════════════════════════════════════ 镜像记账（W6）
 * 一手：`REPORT-109-AURA-AND-SPIRITS.md` 的**§镜像记账表**（TH09 / TH19 两张）。
 * 单人宿主（TH18）没有「对面场地」⇒ 按总纲 §6.1 的口径：**对面出现的东西 = 用同一套装帧的额外攻击**，
 * 所以这里只做**纯逻辑的映射与门**，消费侧（真去生成）下一步再接。
 * 事件号是自定的枚举（❓ 只是本卡的内部编码，不是一手值）；**映射与门都是一手**：
 *   · TH09：收 level+1 的门 = 收件方 `EM+0x2ac3b8 < 25`；落地的门 = `< 10`；
 *           `+0xa4 = min(level,2)`、`+0xa6 = min(level+1,3)`；level 3 / 已固化 ⇒ 不返还。
 *   · TH19：純霊生成上限 60/80/999（超限 100 分）；击破純霊 ⇒ 送動物霊（种类按距离²）；
 *           击破動物霊 ⇒ 返还（同 kind）；自然成仏 ⇒ 变動物霊送出；EX ⇒ 对面不回 EX。 */
enum {
    POFV_EV_STORM_SEND_L1 = 1,   /* 我方：暴风送出 level 1            → 对面：也送 level 1   */
    POFV_EV_KILL_INCOMING,       /* 我方：打死 A 处 incoming          → 对面：打死最近的灵 */
    POFV_EV_SEAL_INCOMING,       /* 我方：固化 B 处 incoming          → 对面：固化最近的灵 */
    POFV_EV_LANDING,             /* 我方：incoming 落地               → 门 = 收件方 < 10  */
    POFV_EV_PURE_SPAWN,          /* TH19：自然純霊生成                → 对面：同样（上限分档）*/
    POFV_EV_KILL_PURE,           /* TH19：击破純霊                    → 对面：送動物霊      */
    POFV_EV_KILL_ANIMAL,         /* TH19：击破動物霊                  → 对面：返还（同 kind）*/
    POFV_EV_DOMAIN_SEAL,         /* TH19：开領域固化                  → 对面：固化其灵      */
    POFV_EV_NATURAL_JOUMON,      /* TH19：自然成仏                    → 对面：变動物霊送出  */
    POFV_EV_EX_OUT               /* TH19：送出 EX                     → 对面：**不回** EX   */
};

/* ★★ 2026-10-05 owner 口径（我先前读偏了两次，以这条为准）：
 *   · **五种灵都算「灵」**（TH19 狼/獺/鷲/ピュア霊 + TH09 幽灵），**跑完全同一套镜像映射与镜像存储**；
 *   · **镜像映射只按「事件」** —— 不按族、不按种类。映射的**对象**自然可能是**别的族/别的种类**的灵
 *     （owner 举的「我固了幽灵，但此时这个事件镜像过去是固動物霊」＝这套映射**不认族**的一个**可能性**，
 *      不是「逐行 cross 属性」⇒ 不要再把它做成清单 ✗）；
 *   · **族只在两处出现**：① **判上限**（09 算 09、19 算 19 ⇒ `pofv_spirit_cap_for()`）；
 *     ② 映射落到本侧时「本侧那一只是谁」由本侧现状决定。
 *   （`POFV_FAM_*` 只给上限用。） */
#define POFV_TH09_GHOST_CAP 25
#define POFV_TH09_LANDING_CAP 10   /* ★ MODEL §1-3：TH09 **落地 10**（送 25）—— 到达口用它 ✓ */
enum { POFV_FAM_TH09 = 0, POFV_FAM_TH19 = 1 };

#define POFV_MIRROR_GATE_LEVEL_UP   25   /* 一手：收件方 EM+0x2ac3b8 < 25 */   /* ⚠ 本卡**无消费者**（核实 2026-10-05：6 处 pofv 调用只读 mev ✗）；口径保留备查 ✓ */
#define POFV_MIRROR_GATE_LANDING    10   /* 一手：落地门 < 10 */   /* ⚠ 本卡**无消费者**：落地门现改走 POFV_TH09_LANDING_CAP ✓ */

/* 我方事件 ⇒ 对面同型事件（返回同族事件号；0 = 无镜像）+ 门（0 = 无门）。
 * `mirror` / `gate` 走**出参**（i386 不许返回 float，此处虽为 int，仍保持全仓「一律出参」的写法）。 */
/* ★ 带族的镜像：我方事件（发生在本族）⇒ 对面同型事件 + **对面的族**（默认 = 换族）+ 门。
 *   owner 口径：「我可能固了幽灵，但此时这个事件镜像过去是**固动物灵**」⇒ `mirror_family` 与 `our_family` 相反。 */
/* 我方事件 ⇒ 对面同型事件（返回同族事件号；0 = 无镜像）+ 门（0 = 无门）。
 * **只有一套映射，按事件**（`mirror` / `gate` 走出参，全仓一致写法）。 */
void pofv_mirror_of(int32_t our_event, int32_t *mirror, int32_t *gate);
/* 一手 level 规则：收到 level+1 时 `a4 = min(level,2)`、`a6 = min(level+1,3)`。 */
void pofv_mirror_level_gain(int32_t level, int32_t *a4, int32_t *a6);

/* ════════════════════════════════════════════════════════════ EX（W7 的纯逻辑部分）
 * 一手（`engine/versus/th19/08-extra-attack.md`）：
 *   · 触发（CPU story 侧）= 计数 **> 149 / 200 / 300**（`CPUStage5/6StoryInf` slot 1）——按难度三档；
 *   · 对战侧 = 领域 → 攻击等级机 `FUN_004e79e0`（`DAT_0062b254` 0..3）→ `FUN_0050e820` → slot 5 `FUN_0050d8a0`；
 *   · 门 **bit19 全 exe 无写点** ⇒ **恒开、不是一次性** ⇒ 本卡不做「一次性」抑制。
 * 本卡（单人宿主）取 **CPU story 侧那三档**当节拍。 */
int pofv_ex_threshold(int32_t diff);          /* 0/1/2 ⇒ 149 / 200 / 300 */

/* ★★★ W7 EX 的**规格**（一手：本地同一 EX 样式 ⇒ 六条变换）。
 *   `engine/versus/th19/08-extra-attack.md` ＋ owner 口径：
 *   ① 旋转 **180°**（本地同一 EX 样式转过来）② **从版顶长出** ③ **己方伤害**（打得到自己）
 *   ④ **免疫己方系统**（不被自己的消弹圈/罩子吃掉）⑤ **50% 透明度起步** ⑥ **伤害量纲独立**（另开一套刻度）。
 *   ⇒ 这六条落成数据，消费端（真生成 EX）下一轮做。 */
typedef struct {
    int32_t rotate_half;   /* 恒 1：旋转 180° */
    int32_t from_top;      /* 恒 1：从版顶长出 */
    int32_t own_damage;    /* 恒 1：己方伤害 */
    int32_t immune_own;    /* 恒 1：免疫己方系统 */
    int32_t alpha_start;   /* 恒 128：50% 透明起步 */
} pofv_ex_spec_t;
void pofv_ex_spec(pofv_ex_spec_t *o);
/* ⑥ 伤害量纲独立：EX 的伤害走**自己的刻度**（一手：与本体伤害不是同一量纲）⇒ 提供一个换算口（❓ 倍率自定）。 */
#define POFV_EX_DMG_SCALE 10      /* ❓ 自定：EX 的 1 点 = 本体 10 点（量纲独立，别与 storm 的 10 混用）*/
int pofv_ex_damage(int32_t ex_points);

/* ★ 连锁**只放大数值、不做触发门槛**（一手 `engine/versus/th09/02-ghost-and-seal.md` §2.3：
 *   `iVar5 = (player+0x30448 < 0x65) ? player+0x30448 × 10 : 1000`，再由 `FUN_0041d150` 乘全局等级）。
 *   ⇒ `min(连爆数 × 10, 1000)`。 */
int pofv_mirror_value(int32_t combo);
int pofv_ex_ready(int32_t count, int32_t diff);/* count > 门限 ⇒ 1 */

/* ════════════════════════════════════════════════════════════ W6 消费侧：怎么把镜像事件「送出去」
 * 一手（`modkit/MECHANICS-109.md` §6.1 的**两条硬约束** + 对称规则）：
 *   ① **必须是「己方弹幕」**（否则与所有系统牵连太深；教训：自己的消弹圈把自己的弹幕消了 ✗）；
 *   ② 原作「从**对面版底**长出」⇒ 我们「**从版顶**长出」（上下对称）；
 *   ③ **除非本来就是己方弹幕的样式，必须「50% 透明度起步」**（用来区别「这是送过去的」）。
 * 本函数把这三条变成数据：`*from_top`（恒 1）· `*own_bullet`（恒 1）· `*alpha_start`（255 = 本来就是己方样式；
 * 128 = 50% 起步）。❓「哪些镜像事件本来就是己方样式」一手未逐条给 ⇒ 目前只把**暴风类**当己方（255），其余 128。 */
typedef struct { int32_t from_top; int32_t own_bullet; int32_t alpha_start; } pofv_mirror_emit_t;
void pofv_mirror_emit(int32_t mirror_event, pofv_mirror_emit_t *o);

/* ════════════════════════════════════════════════════════════ 固灵结界（除霊オーラ）
 * 一手（`modkit/MECHANICS-109.md` §3.5 + `engine/versus/th19/06-aura.md`）：
 *   · **展开**：长按 CHARGE（= 本卡的**按住 C**），**与蓄力同时**发生（owner：「按住 C = 蓄力，结界同时展开」）；
 *   · **四条效果**：① 形状随角色（一手每角色一个 `Aura<角色>Inf`，槽 7 = 形状判定）
 *     ② **敌人/敌弹减速** ③ 掉**灵力道具** ④ 对灵**沈静化**；
 *   · **沈静化通用化**：一手只有 `AuraOrinInf` 的形状判定返回 `-2`（只有阿燐能沈静化）⇒
 *     **owner 口径：把 `-2` 做成通用** ⇒ 所有角色都能沈静化 ✓；
 *   · **被固灵之后的后果两作不同**：TH19 四种灵 = 只会过一段时间消失（不发弹）；
 *     **TH09 幽灵** = 鸟形→**火焰形**→慢慢上飘→若**没及时打死** ⇒ **三发自机狙**
 *     （★ owner：秉承「除 th06 不变难」原则，**自机狙得做，但最终应该会去掉** ⇒ 做成**可一键关掉**的一档）。 */
typedef struct {
    int32_t active;          /* 长按 C 时 = 1 */
    int32_t slow_enemies;    /* ② 敌人/敌弹减速（恒 1）*/
    int32_t drop_power;      /* ③ 掉灵力道具（恒 1）*/
    int32_t pacify_all;      /* ④ 沈静化通用化（一手只阿燐 ⇒ owner 定：恒 1）*/
    int32_t th09_3shot;      /* TH09 幽灵超时三发自机狙（owner：**可一键关掉** ⇒ 默认 1，置 0 即去掉）*/
} pofv_aura_spec_t;
void pofv_aura_spec(int32_t holding_c, pofv_aura_spec_t *o);

/* 形状判定（一手：槽 7 的「某点是否落在オーラ内」）。本卡先做**圆**（咲夜的「锚定场」是同族）：
 *   `cx,cy` = 锚点（出生点）、`r` = 当前半径、`dx,dy` = 待判点相对锚点的位移。 */
/* ★★★★ 2026-10-08 卡 109 的「灵力 = 蓄力槽」HUD（复刻 TH19 `aura.anm`）用的一手上限：
 *   一手公式 = `clamp(S+0x8c, 0, **200**) × K / 100`（`engine/versus/th19/06-aura.md` §9.17 ✓，
 *   也就是 `aura.anm` script0 起那套蓄力槽所驱动的同一个量）⇒ 蓄力槽比例的**分母** = **200** ✓。
 *   ⚠ 本卡现值只有 `0 ↔ POFV_AURA_TARGET_CHARGE(100)` 两态（`pofv.c` 的 `:2043` / `:2085`）
 *     ⇒ 满档时条走到 **50%**；任务书明确要求「`scale.x` = `s_aura_charge / 200`」⇒ 照办
 *     （要满格只改这一处常量即可）。 */
#define POFV_AURA_CHARGE_MAX 200.0f

/* ★ 2026-10-06：一手 §7.3 的两种判定形状（本卡按角色取一套 ✓）*/
#define POFV_AURA_SHAPE_CIRCLE 0   /* 灵梦：dx²+dy² <= r²+pad² ✓ */
#define POFV_AURA_SHAPE_BAND   1   /* 魔理沙：|dx| <= r（无 y 项）✓ */
int pofv_aura_contains_ex(float dx, float dy, float r, float pad, int shape);
int pofv_aura_contains_sector(float dx, float dy, float r, float pad, float ux, float uy, int half_deg);   /* ★ 咲夜（TH09）圆×扇形：u = 方向参考向量 ✓，half_deg = 半张角(度) ✓ */   /* ★ 咲夜（TH09）圆×扇形 ✓ */
int pofv_aura_contains(float dx, float dy, float r);

/* ════════════════════════════════════════════════════════════ 对面侧的灵 = **纯数据模型**
 * ★★★ owner 2026-10-05 定稿（三条原话）：
 *   · 「我要的是对面的灵是一个**有 x y、类型、计时器**的模型」＋「**我说的计时器实际就是 age**」
 *     ⇒ 字段 `age`（**只递增** ✓）；**没有**倒计时、**没有**「到点自动送回」✗
 *   · 「**送回来只能是镜像击破事件**」⇒ 送回由**镜像击破事件**驱动（**挑哪一只 = 待 owner 定** ❓）
 *   · 「还有一个**状态位：是否被固化**」⇒ 字段 `sealed` ✓
 *   · 「**对面的位置就是我的位置**」（动物灵的击破距离按此判 ✓）
 *   · 「你水獭灵还得**模拟它怎么走的**呀」⇒ 字段 `vx, vy`（复用一手速度函数 ✓）
 *   **零渲染/零敌机/零 VM** ✓ */
#define POFV_FOE_SPIRIT_MAX 64
/* ★ 被固化后多久从对面模型里消失 —— **一手**（owner：「是有一手的」）：
 *   TH19 沈静化那支（`GhostPureHP_D`）末尾是 **`wait(100)` + `wait(60)`** ⇒ **160 帧**（见 `assets/ecl/spirit109.ecl.txt` 表头）✓ */
#define POFV_FOE_SEAL_FRAMES 500   /* ★ 2026-10-06 验收核算一手总时长 = `100+200+200 = 500` 帧 ✓（原 200 ✗ = 截半）*/   /* ★ 2026-10-06 一手 §3.7：角色号 5 才 160，其余 200 ✓（本卡四自机 ⇒ 200 ✓，原 160 ✗）*/
/* ★ 被固化后的运动 —— **一手** `GhostDisableMove` = `ins_404(-1.5707964f, 0.02f)`
 *   ⇒ 方向 −π/2、速度 0.02 ⇒ **停住 + 缓慢上浮**（owner：「甚至幽灵被固化是**往上跑**的」✓）*/
#define POFV_FOE_CALM_VY (-0.02f)
typedef struct {
    float   x, y;      /* 位置（灵被送出时的落点起步 ✓）*/
    float   vx, vy;    /* 速度（一手逐 kind：狼 1.1 / 獺 交替 1.5 / 鷲 60 帧→3.5 / 純霊 1.5 / 幽灵 1.0）*/
    int32_t age;       /* ★ 「计时器」= age（只递增 ✓）*/
    int32_t type;      /* 类型 = 狼/獺/鷲/純霊/幽灵 */
    int32_t level;     /* ★ 幽灵的 level：0 = 非幽灵 / 1 绿 / 2 黄 / 3 红（**我定的表示法** ✓）*/
    int32_t sealed;    /* ★ 状态位：是否被固化 */
    int32_t sealed_timer; /* ★★ 被固化后**多久消失**的倒计时（owner 追加 ✓；时长自定 ❓）*/
    int32_t active;
    /* ★★★ 2026-10-06 owner 裁定（经 `EX-GAP-NOTES.md` 同步 ✓）：**触发 EX = 出循环** ✓
     *   —— 一旦触发：**这一次不再返还 / 不回程 / 不再产生灵**（**循环在此终止** ✓）。
     *   本字段 = 「出循环」标记：置位后 ⇒ ① `mirror_kill` **不再选它**（不回程 ✓）
     *     ② `spawn` **直接拒绝**（不再产生灵 ✓）。❓ 触发条件/门限/外观**待重做** ✗ ⇒ 卡侧暂不调用 ✓。*/
    int32_t loop_exit;
    /* ★★★ 2026-10-08（owner 给一手 §3.31）：**幽灵出生角**（真幽灵与对称幽灵共用一条规则 ✓）。
     *   一手：斜飞不来自幽灵自己的 ECL ✗，来自**生成那一刻抽的角度** ✓（约 1/5 朝自机 ±22.5°、
     *   其余正下方 ±0.9°）。真幽灵走 `$F0` → ECL `moveVel($F0, 1.0)` ✓；对称幽灵走这两个字段 ✓。*/
    float   entry_ang;   /* ★ 抽中的角度（本引擎约定：0 = +x、π/2 = +y（正下方）✓）*/
    int32_t has_ang;     /* ★ 1 = 按 `entry_ang` 匀速运动（覆盖 `entry_vel_at` 的一手匀速支 ✓）*/
} pofv_foe_spirit_t;

void pofv_foe_spirit_init(pofv_foe_spirit_t *p, int32_t n);
/* 在对面模型里记一条（「送去对面」）⇒ 返回槽位，-1 = 满。 */
int  pofv_foe_spirit_spawn(pofv_foe_spirit_t *p, int32_t n, float x, float y, int32_t type, int32_t level);
/* 每帧推进那只灵自己：按一手运动走一格 + `age++`（**不产生任何「送回」** ✓）。 */
void pofv_foe_spirit_tick(pofv_foe_spirit_t *p, int32_t n);

/* ★★★ 2026-10-08：给**对称幽灵**设出生角 —— 与真幽灵**同一条一手规则** ✓
 *   （`pofv_ghost_spawn_angle` ✓；速度照一手幽灵 **1.0** ✓）。idx = `spawn` 的返回值 ✓。*/
void pofv_foe_spirit_set_ghost_angle(pofv_foe_spirit_t *p, int32_t n, int32_t idx, float ang);
/* ★★ **镜像击破事件**：在对面模型里挑一只（**最近的、且未固化**）⇒ 移出并交出它的数据（= 送回来那只）✓。
 *   ★★★ 2026-10-06 owner 消歧（重要 ✓）：**参考点 = 「被击破的那只灵的位置」** ✓（不是自机位置 ✗）。
 *     · **镜像击破** ⇒ 用**被击破灵**的位置 `(x0,y0)` 做最近邻 ✓（卡侧传 `ev->x/ev->y` ✓）；
 *     · **镜像固化** ⇒ 用**被固化灵**的位置做最近邻（且**排除已固化** ✓）；
 *     · 文档口径：`MODEL-109-SPIRIT-MIRROR.md:26/60`（★「最近邻是本质，**不要**用随机/其它规则」✓）。
 *   返回 1 = 有一只被击破并送回；0 = 无。
 *   ❓「挑哪一只」（最近？任意？是否考虑 `sealed`？）**owner 未定** ⇒ 现在只按「最近的活动项」实现，别当口径 ✗ */
/* ★★ **出循环**（owner 裁定 ✓）：把当前所有活动项标为「出循环」⇒ 本次循环**在此终止** ✓。 */
void pofv_foe_spirit_loop_exit(pofv_foe_spirit_t *p, int32_t n);
/* 是否已处于「出循环」状态（卡侧据此**停止一切送出** ✓）。 */
int  pofv_foe_spirit_loop_exit_any(const pofv_foe_spirit_t *p, int32_t n);
/* ★★★ 2026-10-06 owner：「**更改，镜像击破随机给过来吧**」✓
 *   ⇒ 选谁不再按「最近」✗，改为**随机挑一只**（`rnd` 由卡侧传 `ce_rand()` ✓；core 是纯数据、不自己摇 ✓）。*/
int  pofv_foe_spirit_mirror_kill(pofv_foe_spirit_t *p, int32_t n, uint32_t rnd, float ref_x, float ref_y,
                                 int32_t *out_type, float *out_x, float *out_y);
/* ★★ 镜像**固化**事件：挑最近的**未固化**那只 ⇒ 置 `sealed`。
 *   owner 口径：「**固化不会固化已固化的，这个跳过**」✓（⇒ 只用 `foe_nearest_unsealed`）
 *   ⚠ **击破**路径**不加**这个过滤 ✓（owner 只说了固化这一条）。 */
int  pofv_foe_spirit_seal(pofv_foe_spirit_t *p, int32_t n, float ref_x, float ref_y);
int  pofv_foe_spirit_count(const pofv_foe_spirit_t *p, int32_t n);

/* ════════════════════════════════════════════════════════════ 本侧暴风送出 ⇒ 两个镜像效果
 * ★★★ owner 2026-10-05 口径（原话）：
 *   「我这侧的暴风（如果能送给对面灵）是：
 *     1、**延迟 0-100 帧随机，＋动画随机帧（一手）**，认为对面也暴风了，**送一个 level 1 过来**
 *     2、**同时给对面槽位送一个 level 1**」
 * ⇒ 效果 ① = 对面模型 `pofv_foe_spirit_spawn(x,y,幽灵,level 1)`（**同时**，无延迟 ✓）；
 *   效果 ② = 我这边**延迟**收到一个 level 1 ⇒ 用下面这个**延迟到达队列**（delay 由调用方按一手算：
 *   `0..100 随机 + 动画随机帧` ✓）—— 之后它按「顶部带随机」落到本侧（等同一次「入射」✓，**纯数据** ✓）。 */
#define POFV_FOE_INCOMING_MAX 16
typedef struct {
    float   x, y;       /* 落点（顶部带随机由调用方在到达时给；这里存送出时的参考位置 ✓）*/
    int32_t type;
    int32_t level;
    int32_t delay;      /* 剩余延迟帧（一手：0..100 随机 + 动画随机帧 ✓）*/
    int32_t active;
} pofv_foe_incoming_t;

void pofv_foe_incoming_init(pofv_foe_incoming_t *q, int32_t n);
/* 压入一个「延迟到达」的 level 1（`delay <= 0` ⇒ 下一帧就到 ✓）；返回槽位，-1 = 满。 */
int  pofv_foe_incoming_push(pofv_foe_incoming_t *q, int32_t n, float x, float y,
                            int32_t type, int32_t level, int32_t delay);
/* 每帧推进：**第一个**延迟到 0 的 ⇒ 送出参（= 该落在我方的那一只）并清槽，返回 1；无 ⇒ 0。 */
int  pofv_foe_incoming_tick(pofv_foe_incoming_t *q, int32_t n, int32_t *out_type, int32_t *out_level,
                            float *out_x, float *out_y);
int  pofv_foe_incoming_count(const pofv_foe_incoming_t *q, int32_t n);

/* ════════════════════════════════════════════════════════════ 連撃/破坏**累积 + 阈值**（owner 一手，2026-10-05）
 * owner 给的一手（已回流 `engine/versus/th19/05-four-mechanics.md` §9.4）：
 *   · 时间归一 `local_10 = (int)DAT_0062b260 / *(0x5aa3e0 = 200.0)` ⇒ **t/200** ✓
 *   · **純霊**档：`+0x54 ≥ 120 − local_10 × *(0x5aa344 = 40.0)` ⇒ **120 − t/5** ✓（clamp **700**）
 *   · **另一档**：`+0x50 ≥ 150 − local_10 × *(0x5aa390 = 100.0)` ⇒ **150 − t/2** ✓（clamp **1400**）
 *   · **不重置**：送出后**扣掉阈值、保余数** ✓ —— `+0x54 = 旧 + t/5 − 120`（`*(0x5aa508) = −40.0` ✓）、
 *     `+0x50 = 旧 + t/2 − 150`（`*(0x5aa520) = −100.0` ✓）⇒ 与「减去 (阈值)」等价 ✓
 *   · 每次破坏：`+1`（灵/幽灵侧 `0x51cbf8`/`0x51cc16`）**或** `+15`（死亡分发 `0x51d968`/`0x51d979`）✓
 *   · 登记侧 clamp：`+0x50 ≤ 1400`、`+0x54 ≤ 700` ✓；门卫 `+0x34`/`+0x30` ✓
 * ⇒ 本卡是单人宿主 ⇒ **一套**累积器（发放方 = 我方）✓ */
#define POFV_ACC50_MAX 1400
#define POFV_ACC54_MAX  700
/* ★ TH09 两表步长 = 一手 param_2/param_3（❓ 精确值待核；TUNING 第 6 条记 ≈2–3）⇒ 现 3 / 2 ✓
 *   与 TH19 的 +15/+1 **不可混用** ✗；TH09 一手 clamp 未明说 ⇒ 暂不设上限 ❓ */
#define POFV_ACC09_STEP30   3
#define POFV_ACC09_STEP2C   2
/* ★ 2026-10-06 owner：「TH09 与 TH19 独立运行，按一手 + 真实难度」⇒ 四张表分**两套**：
 *   TH19 = a50/a54（阈值吃**对局时间** 150−t/2 / 120−t/5 ✓）；
 *   TH09 = a2c/a30（阈值吃**难度** (30−難度)×4 / 60−難度×1.5 ✓）⇒ **互不混用** ✗ ✓ */
typedef struct { int32_t a50; int32_t a54; int32_t a2c; int32_t a30; } pofv_acc_t;
void pofv_acc_init(pofv_acc_t *a);
int32_t pofv_acc_thr50(int32_t t);      /* 150 − t/2  （可 <0 ⇒ 视为 0 由调用方判 ✓）*/
int32_t pofv_acc_thr54(int32_t t);      /* 120 − t/5 */
void pofv_acc_add50(pofv_acc_t *a, int32_t n);   /* += n，clamp 1400（n<0 忽略 ✓）*/
void pofv_acc_add54(pofv_acc_t *a, int32_t n);   /* += n，clamp 700 */
/* 过阈值 ⇒ 扣掉阈值（**保余数** ✓）、返回 1；未过 ⇒ 0。`t` = 归一用的帧数 ✓ */
int  pofv_acc_take50(pofv_acc_t *a, int32_t t);
int  pofv_acc_take54(pofv_acc_t *a, int32_t t);
/* ★ owner 2026-10-06：TH09 的**第二套表**（独立于 TH19 ✗不混用）—— 阈值吃**真实难度** ✓ */
int32_t pofv_acc_thr30(int32_t diff);
int32_t pofv_acc_thr2c(int32_t diff);
void    pofv_acc_add30(pofv_acc_t *a, int32_t n);
void    pofv_acc_add2c(pofv_acc_t *a, int32_t n);
int     pofv_acc_take30(pofv_acc_t *a, int32_t diff);
int     pofv_acc_take2c(pofv_acc_t *a, int32_t diff);

/* ════════════════════════════════════════════════════════════ 灵力道具（自建实体池；§E）
 * ★★ 来路：这一段是 **2026-10-01「口径 5：灵力道具用 A」那一批**的复刻 —— 被「全删 109 卡」
 *   (`ad1928bd`) 删掉之前它是**完整且已对齐一手**的（= `ad1928bd^`）。本次按符号逐个搬回，
 *   并适配到当前架构（当前已没有 `pofv_state_t` / 连爆档 / 原生道具槽位）。
 *
 * 为什么**自建实体池**（而不是走原生道具）：一手核对（`SPEC-109-FUSION.md` §8.1）——
 *   TH18 **没有干净的原生道具空号**：type 1/3 = 110/111 的满 P 转换目标、type 9..0xe = 107 的 ★
 *   + 114 的点数载体、7 = 112 的整炸弹、8 = 115 的满 P、4/6/0x10/0x11 = 117/115/111 的碎片
 *   ⇒ 用任何一号都会与别人的卡**串账**。先例：`cards/faith.c`（110/111 绿点）、
 *   `cards/ufo.c`（112 令牌）、`cards/animal.c`（117 动物灵）。
 *
 * 经济口径（与「直接加点」等价，只是中间插了一层可拾取的实体）：
 *   · **击破**：1 颗 × `POFV_POWER_PTS_KILL`（卡侧 `on_enemy_death` 撒，位置 = 敌机位置）；
 *   · **固灵结界碰到敌弹**：从**该弹的原位**撒，**无条件掉**（owner 2026-10-08：
 *     一手 `FUN_004ee9b0` 的 `param_3 == 1` 支就是无条件的 ⇒ **不做概率模型**；
 *     `n%5` 那套属于 `param_3 == 4`，别混）；
 *   · **消弹**（暴风蓝圈消掉的白弹）：`n × POFV_POWER_PTS_CANCEL`（`pofv_power_value_cancel()`）
 *     —— ⚠ 本卡的**卡侧当前不接这条**（owner 2026-10-08 把生成口定案成上面两条）⇒
 *     函数与单测保留、卡侧不调用（见 `TUNING.md` 的登记）。
 *   一次事件要撒的颗数按「每帧上限 / 池余量」截断，**总面值不丢**（余数摊到已撒的几颗上）
 *   —— 照 107 小樱点的打包先例（`pofv_power_spawn_packed*`）。
 *
 * ⚠ 池是**文件级**还是卡内由卡侧决定（本文件只给纯逻辑）；卡侧必须在删卡/换关路径上
 *   （`on_delete` / `on_run_reset`）调 `pofv_power_clear()` —— METHOD 铁律「长持续效果删卡要收干净」
 *   （这里没有 VM ⇒ 清了池就没有残留）。*/

/* ★★ 2026-10-01（owner：「池满可以开大」「你得按连锁」）：同屏上限 48 → **192**。
 *   理由：一颗只是一小组 float，扫一遍 O(192) 很便宜；48 在一波清场时会频繁「池满丢弃」。
 *   ❓ 自定（池上限本身不是一手值；一手 TH19 的道具在管理器的固定数组里）。*/
#define POFV_POWER_MAX              192
/* ★ 一次事件**同一帧**最多撒几颗（防一帧把池子吃光；面值仍守恒 —— 多出来的并进已撒的那几颗）。*/
#define POFV_POWER_KILL_SPAWN_MAX     8
/* ❓ 自定（三个源每颗的**基准面值**；一手未给「道具 → 灵力/攻击槽」的换算，见 `TUNING.md` §2.0v-1 #2）：
 *   · CANCEL = 旧实现的「消弹 +10」；KILL = 旧「击破 +20」；AURA = 口径 6 的结界源（最小一档）。*/
#define POFV_POWER_PTS_CANCEL        10
#define POFV_POWER_PTS_KILL          20
#define POFV_POWER_PTS_AURA           5
/* ❓ 自定（消弹源的每帧上限；口径 6 的护栏之一）*/
#define POFV_POWER_SPAWN_PER_FRAME    4
/* ❓ 自定（一批多颗时的铺开半径）*/
#define POFV_POWER_SPAWN_R           24.0f

/* ★ 飞散方向表长度：**64 向**单位向量（≈5.6° 一档）—— 一手要的是「角在 ±π 上随机」
 *   （`randf_minus_pi_to_pi()`）⇒ 用**取随机下标**近似随机角（表体 = 构建前算好的常数数组）。*/
#define POFV_POWER_SCATTER_DIRS        64
/* ★ 一手：飞散期**剩余速度每帧 −0.03**（TH16 `item+0xc20`，`0x42f82c`–`0x42f83f`）——
 *   ⇒ 飞散距离 = 速度²/(2×0.03)（初速 0.2..2.1 代入 = 0.7..73 px），**无重力**。*/
#define POFV_POWER_DECEL              0.03f
/* ★ 一手（TH16 季节道具 state 1「飘落」）：重力 `0.03`/帧（`0x4b90a4`）。*/
#define POFV_POWER_GRAVITY            0.03f
/* ★ 一手（同上）：`vy > 2.0 → 2.0`（`0x494558`）⇒ 从 0 加速到 2.0 封顶（约 67 帧）。*/
#define POFV_POWER_FALL_MAX           2.0f
/* ★ 一手（TH16 出生）：初速 = `randf_0_to_1() × 1.9 + 0.2` ⇒ **0.2..2.1**（`0x41d880`–`0x41d8bb`）。*/
#define POFV_POWER_SPEED_SCALE        1.9f
#define POFV_POWER_SPEED_MIN          0.2f
/* ★ 一手：出生的道具 `item+0xc50 = 6` ⇒ **待发射 6 帧**（期间不动、不建 VM）。*/
#define POFV_POWER_LAUNCH_DELAY         6
/* ★ 一手：`player.y < POC` 过**回收线** ⇒ 飞向自机、速度 = `sht+8` ——
 *   实读零售 `pl00.sht` 的 `+8` = **6.0**（`2.0, 6.0, 28.0, 4.5` 里的第二个）。*/
#define POFV_POWER_LINE_SPEED         6.0f
/* ★ 一手：进**吸引矩形** ⇒ 速度 = `sht+8 / 3` = **2.0**，**每帧 +0.2、上限 12.0**（`0x4945d4`）。*/
#define POFV_POWER_MAG_SPEED0         2.0f
#define POFV_POWER_MAG_ACCEL          0.2f
#define POFV_POWER_MAG_SPEED_MAX     12.0f
/* ❓ 自定：吸引**矩形**的半宽/半高 —— 一手只写「`item_attract_box_focused/unfocused` 矩形」，
 *   矩形的具体数值本仓还没逆到 ⇒ 先给一组手感值（比旧的圆半径 56 更宽、更扁）。*/
#define POFV_POWER_BOX_HALF_W        56.0f
#define POFV_POWER_BOX_HALF_H       120.0f
/* ★ 一手：出界判据 = y **472** / |x| **200** ⇒ 比寿命更早释放。*/
#define POFV_POWER_OUT_Y            472.0f
#define POFV_POWER_OUT_X            200.0f
/* ❓ 自定（收取半径：进这个圈就算收到、槽当场释放）。*/
#define POFV_POWER_ABSORB_R          12.0f
/* ❓ 自定（寿命帧；到点自行消失，不刷屏）。*/
#define POFV_POWER_LIFE             600
/* ★★ 一手（owner 2026-10-08 定案：道具**从原位往正上方弹**）：
 *   `FUN_00521d60(kind=2, pos, …, **−π/2**, **2.2f**, …)`（TH19 `0x4eeb04`，`06-aura.md` §8.4 表）
 *   ＝ TH18 原生 main-drop 的实参（`sdk.h:410` 同一条注：`(type, 敌pos, 垃圾, −π/2, 2.2f, 0)`）
 *   ⇒ 本卡的**击破源 / 结界碰弹源**都用这一对（角度 −π/2 = 正上方；速度 2.2）。*/
#define POFV_POWER_DROP_ANGLE   (-1.5707964f)
#define POFV_POWER_DROP_SPEED         2.2f
/* ★ 一手（TH09 `POFV_BLAST_CHAIN_MAX` = 4）：`pofv_power_value_cancel()` 的连爆档上限。
 *   ⚠ 当前架构的暴风已没有「连爆档」这个量（`pofv_storm_t` 只有 target/tween/dmg、没有 chain）
 *     ⇒ 卡侧**恒传 1**（无乘数）；`clamp` 保留是为了这个函数的语义完整 + 单测。*/
#define POFV_POWER_CHAIN_MAX            4
/* ❓ 自定（结界「每帧上限」；口径 6 的护栏之一）—— 也是「擦弹掉道具」的每帧颗数上限。*/
#define POFV_AURA_ITEMS_PER_FRAME       4

/* 撒道具的三个来源（诊断 / 日志口径）。*/
typedef enum {
    POFV_POWER_SRC_CANCEL = 0,   /* 消弹（暴风蓝圈消掉的白弹）*/
    POFV_POWER_SRC_KILL   = 1,   /* 击破敌人 */
    POFV_POWER_SRC_AURA   = 2    /* 固灵结界碰到敌弹（口径 6）*/
} pofv_power_src_t;

typedef struct {
    int32_t active;
    float   x, y;        /* 世界坐标（卡侧维护 + 画 VM）*/
    int32_t value;       /* 收取时加的充能点（**打包值**，可能 > 基准面值）*/
    int32_t age;
    int32_t life;        /* 寿命帧（到点自行消失）*/
    int32_t src;         /* pofv_power_src_t（诊断 / 日志）*/
    /* ★★ TH16 季节道具的状态机字段（owner：「动态这块按照季节点做就行」✓）：
     *   0 = 飞散 / 1 = 飘落 / 2 = 过回收线 / 3 = 被吸引。*/
    int32_t st;
    float   vy;          /* 飘落阶段的竖直速度（重力累加、上限 `POFV_POWER_FALL_MAX`）*/
    float   mspd;        /* 吸引阶段的速度（每帧 +0.2、上限 12.0）*/
    int32_t delay;       /* 待发射的延时（> 0 时本帧不动）*/
    float   dx, dy;      /* 飞散方向的单位向量（= TH16 `item+0xc24` 那个角）*/
    float   lr;          /* ★ 剩余速度（= TH16 `item+0xc20`，每帧 −`POFV_POWER_DECEL`）*/
    /* ★★ 可视 VM 句柄（**归卡侧所有**；core 是纯逻辑 ⇒ **不碰、也不依赖 anm API**）。
     *   0 = 还没建（待发射延时中）/ 已删。三条所有权约定：
     *     · `power_take_slot()` / `pofv_power_pool_init()` 会 `memset` 本结构 ⇒ 出生即 0 ✓；
     *     · 卡侧每帧同步（`pofv.c` 的 `power_vm_sync()`）负责**建 / 跟随 / 删** ✓；
     *     · `pofv_power_clear()` 也是 `memset` ⇒ **看不见 VM** ⇒ 卡侧必须先遍历删掉再调 ✓。*/
    int32_t vm;
} pofv_power_t;

typedef struct {
    uint32_t rng;                    /* ★ 飞散方向/初速的伪随机源（xorshift32；单测可复现）*/
    pofv_power_t s[POFV_POWER_MAX];
    int32_t seq;         /* 出生序号（诊断）*/
    int32_t live;
    int32_t spawned;     /* 累计撒出颗数（诊断）*/
    int32_t absorbed;    /* 累计收取颗数（诊断）*/
    int32_t expired;     /* 累计出界 / 寿命到期消失颗数（诊断）*/
    int32_t dropped;     /* 因池满 / 每帧上限被**打包吞掉**的颗数（诊断；面值不丢）*/
} pofv_power_pool_t;

void pofv_power_pool_init(pofv_power_pool_t *p);

/* ★ 消弹源要撒的**总面值**（打包前的账）：`n_white × POFV_POWER_PTS_CANCEL × clamp(chain,1,4)`。
 *   保留原来「连爆质量越高、同样的白弹给得越多」的味道（旧 `pofv_account_blast` 的 `× c`）。*/
int32_t pofv_power_value_cancel(int32_t n_white, int32_t chain);

/* 撒一颗（`value` = 面值；方向 = 随机 ±π、初速 = `rand×1.9+0.2`）。返回槽号，−1 = 池满。*/
int  pofv_power_spawn(pofv_power_pool_t *p, float x, float y, int32_t value, int32_t src);

/* ★ 撒一颗**指定角/速**的（一手 `FUN_00521d60` 的实参；`ang` 单位弧度 ——
 *   本引擎约定 0 = +x、π/2 = +y 正下方）。击破源 / 结界碰弹源走这条。返回槽号，−1 = 池满。*/
int  pofv_power_spawn_dir(pofv_power_pool_t *p, float x, float y, int32_t value, int32_t src,
                          float ang, float spd);

/* ★ **打包撒**：把「`n_units` 份 × `pts_each`」的总面值摊到最多 `max_spawn` 颗实体上
 * （在 (x,y) 周围 `POFV_POWER_SPAWN_R` 内铺开；只有一颗时就在原点）。
 * 实际颗数还会被池余量钳住 ⇒ **面值守恒**（余数给第一颗）；一颗都撒不出时 `*out_pts = 0`。
 * 返回实际撒出的颗数。*/
int  pofv_power_spawn_packed(pofv_power_pool_t *p, float x, float y, int32_t n_units,
                             int32_t pts_each, int32_t max_spawn, int32_t src, int32_t *out_pts);

/* ★ **多位置打包撒**（结界碰弹用）：把「`n_units` 份 × `pts_each`」的总面值摊到
 * `k = min(n_units, max_spawn, 池余量)` 个**调用方给的位置**上（`ox`/`oy` 至少这么多个）。
 * 方向/速度走一手实参（`POFV_POWER_DROP_ANGLE` / `_DROP_SPEED`）。
 * 总面值守恒（余数给第一颗）；`*out_pts` = 实际撒出的总面值。返回实际撒出的颗数。*/
int  pofv_power_spawn_packed_at(pofv_power_pool_t *p, const float *ox, const float *oy,
                                int32_t n_units, int32_t pts_each, int32_t max_spawn,
                                int32_t src, int32_t *out_pts);

/* 每帧推进：待发射 → **飞散**（定角、速度每帧 −0.03、无重力）→ **飘落**（重力 0.03、`vy` 上限 2.0、
 * `vy ≥ 0 ⇒ vx = 0`）+ 过**回收线**全速吸 + 吸引盒内加速吸 + 到收取半径**收取**（点累加进 `*out_pts`）。
 * `player` = 自机 float3（NULL 时只飘落、不吸引也不收取 ⇒ 供单测/无自机帧）；
 * `poc_y` = **回收线高度** —— 卡侧现读 SDK 的 live 字段（`ce_poc_height_semantic()`）传进来：
 *   本层是**纯逻辑**（不含引擎地址）⇒ 不自己读全局。`player[1] < poc_y` = 自机在回收线上方。
 * 返回本帧收取的**颗数**；`*out_pts` = 本帧收取的总面值（卡侧入账）。*/
int  pofv_power_tick(pofv_power_pool_t *p, const float *player, float poc_y, int32_t *out_pts);

/* 删除所有实体（`clear_card()` / `on_delete` 用；返回清掉的颗数）。*/
int  pofv_power_clear(pofv_power_pool_t *p);

/* ============================================================ 可消弹的判据（TH18 侧）*/

/* ★★ owner 口径：「**th18 不存在白弹**，用我之前设计过的**能消那些弹**的判据」。
 *   TH19 的消弹原语 `FUN_004ee770` 只认「弹类型 `+0x1044 ∈ {1,2}` 且 `+0x14 == 0`」，
 *   那是 TH19 的弹字段，TH18 的弹上没有对应物 ⇒ 直接搬会永远消不掉任何东西。
 *   本仓口径（一手出处 = `docs/superpowers/specs/2026-09-11-system-card-spec-review.md` §5.4）：
 *     弹**半径** = `zBullet + 0x658`；**半径 ≤ 4.0 的才是可消的小弹**（0 = 哨兵弹种，不算）。 */
#define POFV_WHITE_R_MAX           4.0f

int  pofv_is_white_bullet(float r);
int  pofv_scan_count_white(const float *radii, int n);

/* ════════════════════════════════════════════════════════════ HUD「hits」（一手：`05-four-mechanics.md` §9.8）
 * ★ 一手段落逐条照抄：
 *   · 取数与锁存：`if (1 < cur && latch < cur && 0 < active) { latch = cur; 寿命 = 120; }` ✓
 *   · 只有 `latch != 0` 才画 ✓；**画两次**：数字（值 = latch）+ 标签（字符串 `hits`）✓
 *   · 寿命到 0 或 `active < 1` ⇒ 颜色在**黄 ↔ 灰**之间按 `寿命 % 6 < 3` 切换（**6 帧周期闪烁** ✓）
 *   · 一手寿命 `+0x2290 = 0x78(120)`、`+0x2294 = 120.0f`、`+0x228c = 0x77(119)` ✓ */
typedef struct {
    int32_t cur;      /* 当前 hits（一手 `+0x22b4`）*/
    int32_t latch;    /* 锁存显示值（一手 `+0x22b8`）*/
    int32_t life;     /* 剩余寿命帧（一手 `+0x2290`，初值 120）*/
    int32_t active;   /* 一手 `+0x227c`（<1 ⇒ 闪灰）*/
} pofv_hits_t;
#define POFV_HITS_LIFE 120   /* ★ 一手值 ✓（owner：「窗口就回退」✓；显示淡出约 2 s ✓）*/
void pofv_hits_init(pofv_hits_t *h);
void pofv_hits_bump(pofv_hits_t *h, int32_t n);   /* 累加当前 hits（一手：由破坏/連撃累加 ✓）*/
void pofv_hits_tick(pofv_hits_t *h);              /* 每帧：锁存判定 + 寿命递减 ✓ */
/* 该不该画（一手：`latch != 0` 才画 ✓）；颜色选择（一手：6 帧周期 黄/灰 ✓）*/
int  pofv_hits_visible(const pofv_hits_t *h);
int  pofv_hits_is_gray(const pofv_hits_t *h);      /* 1 = 灰（`life % 6 < 3` 且 `active < 1`）*/

/* ════════════════════════════════════════════════════════════ TH19「連撃（combo）」账（★一手，2026-10-10 核死）
 * ★★★★ 之前「增量写点找不到」的**真因** ✗：combo **不在字面位移 `0x22bc` 上** ——
 *   Hits 子对象 = **`PlayerInf+0x2278`**，凡是引用它的函数都拿 `this = PlayerInf+0x2278`
 *   （调用点逐字是 `81 c1 78 22 00 00` = `add ecx,0x2278` ✓），于是字段落在 **`+0x44`** ✓
 *   （`+0x22bc − 0x2278 = 0x44` ✓）。旧检索把范围限在 `0x544000–0x549000`，
 *   而**增量函数在范围之外（`0x549780`）** ✗ ⇒ 才会得出「没有增量写点」。
 *
 * 一手（TH19 v1.10c）逐条（地址都是 `th19.v1.10c`、imagebase `0x400000`）：
 *   · **+1 写点**：`FUN_00549780`（`0x549780`）末段 **`0x54980f: inc dword ptr [ecx+0x44]`** ✓；
 *     紧跟 `0x549812: mov eax,[ecx+0x44]` / **`0x549815: mov [ecx+0x3c],eax`**
 *     ⇒ **`+0x22B4 := +0x22BC`**（HUD 当前值镜像 ✓）、`0x549818/`**`0x54981d`**: `+0x22C0 = max(...)` ✓。
 *   · **谁调它**（全 exe **只有两处 `e8 rel32`** ✓）：
 *       ① **`0x50c450`**（`FUN_0050c1f0` = 通用敌人死亡处理 ✓），实参 = `Enemy+0x6350`
 *          （= ECL `ins_1032` 的值 ✓）⇒ **每次击破 +1**，并把该实参加进窗口；
 *       ② **`0x51cb91`**（`FUN_0051c8c0` = **暴风每帧 tick 的消弹循环** ✓，实参 `push 0` = `0` ✓）
 *          ⇒ **每消掉一颗小弹 +1**、窗口**不加** ✓。
 *   · **窗口 / 断连**：`+0x227C`(int) / `+0x2280`(float) —— `FUN_00549780` 里 `+= 实参`、
 *     **上限 60 帧**（`0x5497fb: mov [ecx+4],0x3c` ✓）；`FUN_005450e0`（tick，`0x5450e0`）
 *     的 `0x5459d8`–`0x5459ee` **每帧 −1.0** ✓（`0x5aa230 = 1.0f` ✓；另有难易度缩放表 `0x596da0`
 *     ⇒ 本卡不接 ❓）；**穿 0 的那一帧** ⇒ **`0x545a2e: mov dword [ebx+0x44],0`** = **combo 归零** ✓
 *     （归零前 `0x5459f5: cmp [ebx+0x44],0x14`：`combo ≥ 20` 先给该侧 struct 的 `+0x2C` 对象 +1 计数 ✓
 *      —— 本卡**没有**那个消费者 ⇒ 只留常量口径 ✓）。
 *     ⚠ **quirk（照抄 ✓）**：窗口**已经 ≤0** 时 tick 入口 `0x545962: jg` 早退 ⇒ **不再归零** ✗ ——
 *       所以「窗口已在 0 时被消弹 +1」出来的 combo 会**一直留着**，直到下一次击破把窗口顶起来再漏空 ✓。
 *   · **上限：没有** ✗ —— 一手对 `+0x44` 只有 `inc` / `= 0` / 阈值比较，**无 clamp** ✗。
 *     ⚠ 旧文档的「cap 19」是**误读** ✗（`0x5459f5` 的 `0x14` 是「归零前的阈值」，不是上限 ✓）。
 *   · **HUD：有**（但读的不是 `+0x22bc` 本身 ✓）：`FUN_00546080`（由每帧任务 `FUN_00546060` 调 ✓）
 *     读 **`+0x22B4`**（= 上面那个镜像 ✓）/ `+0x22B8`（锁存）/ `+0x227C`（active）/ `+0x2290`（寿命 120 ✓），
 *     数字 = `FUN_004e7610(vm, pos, "%d" = 0x5A0260, value)`、标签 `"hits"` = `0x5A0264`、
 *     位置 `y = 32.0` ✓、`+0x227C < 1` 时按 `寿命 % 6 < 3` 黄/灰闪 ✓
 *     ⇒ **「HUD 显示 combo」成立**（经 `+0x22B4` 的镜像 ✓）。
 *   · 其余清零路（初值）：`0x5482b9`（`FUN_005478f0` = 玩家注册/构建 ✓）、`0x42fac4`
 *     （`FUN_0042f440` = ctor 批量清零 ✓）、`0x545acd`+`0x545ad4`（`FUN_00545a90`，
 *     **全 exe 无引用 = 本 build 的死代码** 🟡）。
 *   ⇒ 本卡的账 = 上述三条：**+1（击破/消弹）· 60 帧窗口 · 穿 0 归零**，**不设上限** ✓。
 * ⚠ 实现（`_init` / `_add` / `_tick`）是**卡侧 `static`**（`pofv.c`）—— 本文件只放**数据与常量**
 *   （`pofv_core.c` 不在本轮可改范围 ✓；逻辑本身仍是纯逻辑、无引擎地址 ✓）。*/
#define POFV_TH19_COMBO_WINDOW_MAX   60   /* ★ 一手 ✓：窗口上限帧（`0x5497fb: [ecx+4] = 0x3c`）*/
#define POFV_TH19_COMBO_KILL_WIN     20   /* ★ 一手 ✓：击破的窗口加量 = `Enemy+0x6350`（ECL `ins_1032`）——
                                           *   机器码默认值 = **0x14**（`0x50ac4a: mov [esi+0x5120],0x14` ✓）、
                                           *   `wave*.ecl` 也**逐脚本都写 `ins_1032(20)`** ✓ ⇒ 取 20。
                                           *   ⚠ TH18 **没有** `ins_1032` 同类字段 ⇒ 这是**一手常数**、不是读到的字段 ✓
                                           *   （已登记 `TUNING.md` ❓）。*/
#define POFV_TH19_COMBO_CANCEL_WIN    0   /* ★ 一手 ✓：暴风消弹那支的实参 = `0`（`0x51cb82: push 0`）*/
#define POFV_TH19_COMBO_BONUS_AT     20   /* ★ 一手 ✓：归零前阈值（`0x5459f5: cmp [ebx+0x44],0x14`）——
                                           *   它触发的「该侧 struct +0x2C 对象 +1」本卡**无对应系统** ⇒
                                           *   只保留口径、**不消费** ✓（登记 `TUNING.md` ❓）。*/
typedef struct {
    int32_t combo;   /* = 一手 `PlayerInf+0x22BC`（= HUD 显示的那个数 ✓）*/
    int32_t win;     /* = 一手窗口 `PlayerInf+0x227C`（帧；≤0 = 已断连 ✓）*/
} pofv_th19_combo_t;

/* ════════════════════════════════════════════════════════════ TH09 侧「SP → 送」阈值（一手，owner 表）
 * ★ owner 2026-10-05 明确：**要双重送** ✓ ⇒ 送有**两条**扳机，各自独立：
 *   ① **hit 累积阈值**（TH19 口径 `150 − t/2` / `120 − t/5` ✓，见 `pofv_acc_*`）；
 *   ② **SP 阈值**（TH09 口径 `SP 块 +0x30 ≥ 60 − 難度×1.5`，触发后 `SP += 難度×2 − 60` ⇒ **保余数** ✓）。
 *   ⚠ 两条都送是**owner 要的**（不是我此前的推断 ✗）；SP 与 Hits 仍是**各自独立**的计数器 ✓。
 * ❓ `難度` 的取值（TH09 的難度系数）与 SP 的**回复点**未给 ⇒ 用参数传入、不写死 ✓。 */
typedef struct { int32_t sp; int32_t diff2; } pofv_sp_t;   /* diff2 = 難度×2（整数化，避免浮点 ✓）*/

void pofv_sp_init(pofv_sp_t *s);
void pofv_sp_add(pofv_sp_t *s, int32_t n);
int32_t pofv_sp_threshold(const pofv_sp_t *s);   /* 60 − 難度×1.5，用 diff2 表：60 − diff2×3/4 ✓ */
/* 过阈值 ⇒ `SP += 難度×2 − 60`（= diff2 − 60，**保余数** ✓）并返回 1；未过 ⇒ 0 */
int  pofv_sp_take(pofv_sp_t *s);
