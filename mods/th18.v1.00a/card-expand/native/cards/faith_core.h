/* faith_core.h —— 绿点 / 信仰系统（风神录 110 + 地灵殿 111 共用）的**纯逻辑**层。
 *
 * 只 <stdint.h>，主机 gcc 直接测（`tests/test_faith_core.c`）。引擎访问 / VM 池在 `faith.c`。
 *
 * 用户定案（2026-09-12，`docs/superpowers/specs/2026-09-11-system-card-spec-review.md` §7.1 / §3.2.1）：
 *   「111 的绿点和 110 的绿点**完完全全就是同一个东西**，它们加的都是分，只是加的数量不一样」
 *   ⇒ **一套实现 + 两个记账槽**（不是各做一套、也不是共用一个槽）。
 *   本文件就是那「一套」：**按槽（slot）记账**的加值 / 夹取 / 衰减 / 连击 / 收集判定。
 *
 * 三个槽的量纲（§7.1）：
 *   SLOT_110（风神录）：单位 = 信仰点（会消失的最大得点），基础恒 50000，Δ 下限 0。
 *   SLOT_111（地灵殿）：单位 = SA 绿点（不会消失），基础按难度，Δ 上限 = 原版基础。
 *   SLOT_SIGNAL（信号，111 侧）：0..130 的强度，**单列**（它是乘数的一半，不是得分）。
 *
 * 「单位换算」在这里的实现方式：**槽的原始单位 ×100 定点**存 `delta`（大信仰点 5000 → 500000），
 * 出槽外统一 `/100`（见 `faith_score_unit()`）—— 定点只是为了 111 的「小绿点每 100 个 +10」（§3.2.1
 * 的卡私有余数计数器）能不丢余数，110 这边数值全是 100 的倍数，等价于整数。
 */
#pragma once
#ifndef FAITH_CORE_H
#define FAITH_CORE_H
#include <stdint.h>

/* ---- 槽 ---- */
enum {
    FAITH_SLOT_110 = 0,     /* 风神录（信仰）*/
    FAITH_SLOT_111 = 1,     /* 地灵殿（SA 绿点）*/
    FAITH_SLOT_SIGNAL = 2,  /* 信号强度（111；0..130，不是得分）*/
    FAITH_SLOT_COUNT = 3
};

/* ---- 定点 ---- */
#define FAITH_FIXED         100   /* 槽内原始单位 ×100 定点 */

/* ---- 基础值（§7.2）---- */
#define FAITH_BASE_110      50000  /* 恒（所有难度）*/
/* 基础_111 按难度：E 25000 / N 50000 / H 100000 / L 200000 / EX 200000。用难度下标查（见下）。*/
#define FAITH_BASE_111_EASY    25000
#define FAITH_BASE_111_NORMAL  50000
#define FAITH_BASE_111_HARD   100000
#define FAITH_BASE_111_LUNATIC 200000
#define FAITH_BASE_111_EXTRA  200000

/* 难度下标（TH18 的 difficulty 值，与 `cards.js` / ECL 同口径：0 EASY … 3 LUNATIC、4 EXTRA）。
 * `faith_base_111()` 对越界入参钳到 LUNATIC。*/
enum { FAITH_DIFF_EASY = 0, FAITH_DIFF_NORMAL = 1, FAITH_DIFF_HARD = 2,
       FAITH_DIFF_LUNATIC = 3, FAITH_DIFF_EXTRA = 4 };

/* ---- 绿点三档单颗值（§7.1 的表）---- */
#define FAITH_MOF_BIG_EN   5000    /* 大信仰点 E/N（th10 一手：0x41b57f 的难度表）*/
#define FAITH_MOF_BIG_H    8000    /* 大信仰点 HARD */
#define FAITH_MOF_BIG_L   10000    /* 大信仰点 LUNATIC（/EX）*/
#define FAITH_MOF_WEI       10     /* 微信仰点：**消弹每颗 +10**（§2.2）*/
#define FAITH_MOF_SMALL    100     /* 小信仰点（满 P 时 P 点转化）*/
#define FAITH_SA_BIG       100     /* 大绿点（111）*/
#define FAITH_SA_MID        10     /* 中绿点（111，满 P 后吃 P 点）*/
#define FAITH_SA_SMALL_N   100     /* 小绿点：**每 100 个** +10（§3.2.1）*/
#define FAITH_SA_SMALL_V    10

/* ---- 信仰减少（§7.5 / §2.2）---- */
#define FAITH_MISS_DIV      3      /* miss −(信仰−50000)/3（对 Δ 而言 = −Δ/3）*/
#define FAITH_SPELL_MISS_COST   3000   /* 非决死灵击 −3000 */
#define FAITH_ALL_AVOID_COST   30000   /* 全避 boss 非符 / 符卡 −30000（时符必减）*/
#define FAITH_EMPTY_NO_ENEMY_PER_SEC  10800   /* 连击槽空 且 场上无敌人：−10800/s */
#define FAITH_EMPTY_ENEMY_PER_SEC       600   /* 连击槽空 且 场上有敌人：−600/s */
#define FAITH_FRAMES_PER_SEC  60   /* zTimer 口径：60 帧 = 1 s */

/* ---- 连击条（§7.5）---- */
#define FAITH_COMBO_FULL  130      /* 满槽 = 130 帧 */
#define FAITH_COMBO_BIG       130  /* 大信仰点 */
#define FAITH_COMBO_GREEN     100  /* 蓝点（= 我们的绿点收集）*/
#define FAITH_COMBO_SMALL_P    60  /* 小 P / 小信仰点 */
#define FAITH_COMBO_BIG_P      20  /* 大 P / 击杀非 boss */
#define FAITH_COMBO_WEI         3  /* 微信仰点（消弹每颗）*/
#define FAITH_COMBO_KILL_BOSS   0  /* 1UP / 击杀 boss = +0 */

/* ---- 灵击：门槛 / 消耗 / 下限（§3.2 + §3.2.1，110 与 111 共用同一套）----
 * 换算式：MoF 下限 0.00 ↔ TH18 下限 1.00 ⇒ TH18 1.00 = MoF 0.00、TH18 2.00 = MoF 1.00。
 * 门槛 = TH18 ≥ 2.00 档；每次扣 1 档；★ 用户「加一个サービス」⇒ **下限 1.05 档**。
 *
 * ⚠ 为什么必须自写扣法（不要用 `ce_spend_power` / 原生 `GlobalsInner__spend_power` `0x457480`）：
 *   ① 它在 `0x4574a3` 判 `CURRENT_POWER <= 一档` 就**直接拒绝**（返回 0、不扣）；
 *   ② 它**只能整档扣**（档数语义），落不到 1.05 这种非整档值。
 *   ⇒ 见 `faith_power_spend()`：自己写 `CURRENT_POWER`（`0x4ccd38` = `GlobalsInner+0x5c`）+ 夹下限，
 *      扣完**必须** `ce_repopulate_options()`。 */
#define FAITH_POWER_PER_LEVEL  100     /* 一档 = CE_ADDR_POWER_PER_LEVEL 0x4ccd40 = 100（0x45a98b）*/
#define FAITH_POWER_MIN_LEVEL  2       /* 门槛：TH18 ≥ 2.00 档 */
#define FAITH_POWER_FLOOR      105     /* 下限 = 1.05 档（定点 105）*/
#define FAITH_POWER_COST       100     /* 每次扣 1 档 */

/* ---- 信号强度（§7.5，111 侧）----
 * ±50/帧、+0.09/帧、+8/graze、−0.3/帧 都不是整数 ⇒ 用**千分点**定点（1 点 = 1000）。
 * 这里的值来自 §7.5 的数值表，**不是**我从原作抄的 ⇒ 111 实装前按 `sa-signal-design.md` 对一遍。*/
#define FAITH_SIGNAL_MAX        130000   /* 上限 130 点 */
#define FAITH_SIGNAL_HI_MARK    100000   /* 线上且未满 100 时 +50/帧；超 100 后 +0.09/帧 */
#define FAITH_SIGNAL_GAIN_LINE     50000 /* +50.0 / 帧（线上，未满 100）*/
#define FAITH_SIGNAL_GAIN_TRICK       90 /* +0.09 / 帧（已超 100）*/
#define FAITH_SIGNAL_GAIN_GRAZE    8000  /* +8 / graze */
#define FAITH_SIGNAL_LOSS_OFFLINE    3000 /* −0.3 / 帧（线下）*/

/* ---- 擦弹倍率（§7.5）---- */
#define FAITH_GRAZE_PER_GRAZE   1000   /* 倍率定点：1 graze = 0.0001 ⇒ 千分点 1 */
#define FAITH_GRAZE_MULT_MAX    8990   /* min(graze × 0.0001, 8.99) */

/* ---- 连击槽空的扣减：整数化（具名常量，避免浮点漂移）----
 * −10800/s 与 −600/s 按帧除不尽，这里用**每 15 帧结算一次**的精确整数写法：
 *   无敌人：每 15 帧扣 2700（= 10800 × 15/60）
 *   有敌人：每 30 帧扣  300（=   600 × 30/60）
 * ⇒ 均分到每帧与 /s 完全一致、无余数丢失。⚠ 这是**我们选的实现口径**（原作是每秒整数扣）。
 */
#define FAITH_EMPTY_NO_ENEMY_EVERY  15
#define FAITH_EMPTY_NO_ENEMY_STEP   2700
#define FAITH_EMPTY_ENEMY_EVERY     30
#define FAITH_EMPTY_ENEMY_STEP       300

/* ---- 收集判定（照原生道具的几何：圆圈收 + 矩形吸引环给坐标）---- */
#define FAITH_COLLECT_R_DEFAULT   30.0f   /* player+0x4798c 的初值（引擎 `Player__reset`）*/
#define FAITH_ATTRACT_R_DEFAULT   70.0f   /* player+0x47990/94 的初值 */

/* ---- 绿点事件（绿点是从哪来的）---- */
typedef enum {
    FAITH_EV_BIG = 0,        /* 大信仰点 ⊕（+5000/8000/10000）· 大绿点（+100）*/
    FAITH_EV_GREEN,          /* 绿点收集（蓝点通道；连击 +100f）*/
    FAITH_EV_SMALL_P,        /* 小 P（连击 +60f）*/
    FAITH_EV_SMALL_FAITH,    /* 小信仰点（+100 / 连击 +60f）*/
    FAITH_EV_BIG_P,          /* 大 P（连击 +20f）*/
    FAITH_EV_KILL_NONBOSS,   /* 击杀非 boss（连击 +20f）*/
    FAITH_EV_MICRO,          /* 微信仰点 = 消弹每颗（+10 / 连击 +3f）*/
    FAITH_EV_KILL_BOSS,      /* 击杀 boss（连击 +0f）*/
    FAITH_EV_ONEUP,          /* 1UP（连击 +0f）*/
    FAITH_EV_COUNT
} faith_ev_t;

/* ---- 槽状态（纯数据；卡可以整块拷进自己的 ce_state）---- */
typedef struct {
    int32_t delta;      /* 累积增量，**原始单位 ×FAITH_FIXED 定点**（110：5000 → 500000）*/
    int32_t combo;      /* 连击槽 0..130（帧）*/
    int32_t signal;     /* 信号强度 ×1000（0..130000）；只有 111 会动它 */
    int32_t small_rem;  /* 111 的小绿点余数 0..99（**跨关保留**，§3.2.1）*/
    int32_t base;       /* 该槽的基础值，**原始单位**（110 = 50000 常数；111 = 按难度）*/
    int32_t delta_cap;  /* Δ 上限，**原始单位**（0 = 无上限）。111 = 原版基础，110 = 0（无上限）*/
} faith_slot_t;

/* 只读快照（给「落笔者」算 PV 用：落笔者**不读别的卡的状态**，只读自己与聚合值）。*/
typedef struct {
    int32_t faith_total;   /* 原始单位：基础 + Δ（110 侧这就是 MoF 的「信仰」）*/
    int32_t delta;
    int32_t base;
    int32_t combo;
} faith_view_t;

/* ---- 初始化 ---- */
/* 110 槽：基础恒 50000、Δ 从 0 起算、下限 0（= 「50000 扣不穿」）、无上限。*/
void faith_slot_init_110(faith_slot_t *s);
/* 111 槽：基础按难度、Δ 上限 = 原版基础（§7.2）。*/
void faith_slot_init_111(faith_slot_t *s, int difficulty);
void faith_slot_reset(faith_slot_t *s);          /* 清 delta/combo/signal，保留 base/cap */

int32_t faith_base_111(int difficulty);          /* 越界钳 LUNATIC */
int32_t faith_big_mof(int difficulty);           /* 大信仰点难度表：E/N 5000 · H 8000 · L/EX 10000 */

/* ---- 加值 ---- */
/* 落一颗绿点 / 信仰点到某个槽。delta 是**原始单位**（5000 传 5000）。返回真正加进去的量（原始单位）。*/
int32_t faith_slot_add(faith_slot_t *s, faith_ev_t ev, int32_t delta);
/* 定点版（微信仰点 / 小绿点这类 1/100 单位的通道用）。返回真正加进去的定点量。*/
int32_t faith_slot_add_fixed(faith_slot_t *s, int32_t delta_fixed);
/* 消弹累加器 → 110 的微信仰点（每颗 +10）与 111 的小绿点（每 100 个 +10，余数跨帧/跨关保留）。
 * n = 本次消掉的弹数（直接吃 `ce_cancel_radius()` 的返回值，§5.5 订正 3）。
 * 返回值写进 *out_110_add / *out_111_add，可为 NULL。
 * ⚠ **单位是定点（×100），不是原始单位** —— 交出去的是 `faith_slot_add()` 的返回值（定点量），
 *   与 `faith_slot_add()` 的「原始单位」不同（2026-09-12 订正：原来这句写成「原始单位」是错的）。
 *   调用方若只要「这一步有没有进位」，判 `> 0` 即可；要原始单位就 `/ FAITH_FIXED`。*/
void faith_slot_add_cancel(faith_slot_t *s110, faith_slot_t *s111, int32_t n,
                           int32_t *out_110_add, int32_t *out_111_add);

/* ---- 减益 ---- */
#define FAITH_REDUCE_NONE    0
#define FAITH_REDUCE_MISS    1   /* miss −(信仰−50000)/3；对 Δ 而言 = −(Δ/3)（向下取整）*/
#define FAITH_REDUCE_BOMB    2   /* 非决死灵击 −3000 */
#define FAITH_REDUCE_AVOID   3   /* 全避 boss 非符 / 符卡 −30000 */
/* 施加一条减益到槽；返回真正的扣减量（正数 = 扣掉多少，原始单位）。Δ 夹在 [0, ∞)。*/
int32_t faith_slot_reduce(faith_slot_t *s, int kind);
/* 连击槽空时的持续扣信仰。frames 一般传 1；enemies_present = 场上还有没有敌人。
 * 返回本帧真正扣掉的量（原始单位）。combo > 0 时不动。*/
int32_t faith_slot_drain_empty(faith_slot_t *s, int frames, int enemies_present);
/* 「连击槽空」是 drain 的前置条件（= combo == 0）。*/
int     faith_combo_empty(const faith_slot_t *s);

/* ---- 连击槽 ---- */
int32_t faith_combo_value(faith_ev_t ev);         /* 该事件的 +f 值（§7.5 的表）*/
int32_t faith_combo_add(faith_slot_t *s, faith_ev_t ev);   /* 加到满槽 130 为止；返回真正加了多少 f */
int32_t faith_combo_set(faith_slot_t *s, int32_t frames);
int     faith_combo_full(const faith_slot_t *s);  /* == 130 */

/* ---- 信号强度（111）---- */
/* 本帧信号增量（千分点）。above_line = 过回收线；offline = 线下（两者互斥，above_line 优先）。
 * graze = 本帧擦弹数。返回可正可负；调用方用 `faith_signal_add()` 落值。*/
int32_t faith_signal_step(int32_t signal_now, int above_line, int offline, int32_t graze);
void    faith_signal_add(faith_slot_t *s, int32_t d);   /* 夹 [0, FAITH_SIGNAL_MAX] */
void    faith_signal_reset(faith_slot_t *s);            /* 被弹 / 关卡结束归 0 */

/* ---- 单位换算 / 快照 ---- */
int32_t faith_slot_total(const faith_slot_t *s);          /* 基础 + Δ（原始单位）*/
int32_t faith_score_unit(const faith_slot_t *s);          /* 出槽：原始单位（= total，定点 /100 后）*/
void    faith_slot_view(const faith_slot_t *s, faith_view_t *out);

#endif /* FAITH_CORE_H */
