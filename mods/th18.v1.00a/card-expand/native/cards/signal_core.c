/* signal_core.c —— 地灵殿（111）纯逻辑的实现。接口与依据见 signal_core.h。
 *
 * ★ 本文件**不重写共用件**：信号槽的落值 / 夹取在 `faith_core.c`，擦弹倍率在 `mof_core_graze_mult()`，
 *   灵击扣档在 `mof_core_power_spend()`。这里只做 **111 私有** 的那几件：
 *     ① 擦弹按难度查表；② 回收线档位；③ 蓝点分整数式；④ miss 扣信仰；
 *     ⑤ 命碎 / 得分奖残；⑥ HUD 条宽。
 *
 * 数值出处（一条都没自己发明，逐条在 .h 的表格里）：
 *   `local/_scratch/research-th11-signal.md`
 *   `local/_scratch/research-th11-faith-and-points.md`
 *   `local/_scratch/research-th11-bomb-lifefrag-hud.md`（§4 由本轮一手复核订正，见 `signal_core.h`）
 * 主机单测：`tests/test_signal_core.c`。
 */
#include "signal_core.h"
#include <string.h>      /* ★ 2026-09-15i：`memset` / `memcpy` —— 六条新特技的逐帧状态要整块重建/清零
                          *   （`.h` 仍然只 include `<stdint.h>`，这是 .c 侧的事，仍可主机单测）。*/

/* ---- 擦弹 → 槽（一手 §3 的四张表；E/N/H/L/EX）---- */
int32_t signal_core_graze_gain(int difficulty, int is_laser)
{
    if (!is_laser) {
        /* 子弹 / 敌体擦弹：`th11:0x4a7950` = `th11:0x4a79d4` = {800,500,500,500,500} 存储 */
        switch (difficulty) {
        case FAITH_DIFF_EASY:  return FAITH_SIGNAL_GRAZE_BULLET_EASY;
        case FAITH_DIFF_HARD:  return FAITH_SIGNAL_GRAZE_BULLET_HARD;
        case FAITH_DIFF_LUNATIC:
        case FAITH_DIFF_EXTRA: return FAITH_SIGNAL_GRAZE_BULLET_LUNATIC;
        case FAITH_DIFF_NORMAL:
        default:               return FAITH_SIGNAL_GRAZE_BULLET_NORMAL;
        }
    }
    /* 激光擦弹：`th11:0x4a7a68` = `th11:0x4a7a7c` = {500,400,300,200,200} 存储
     *（原版还要求「每 3 帧才算 1 擦弹」；TH18 的计数不区分来源 ⇒ 这一支目前没人调，见 TUNING）。*/
    switch (difficulty) {
    case FAITH_DIFF_EASY:   return FAITH_SIGNAL_GRAZE_LASER_EASY;
    case FAITH_DIFF_NORMAL: return FAITH_SIGNAL_GRAZE_LASER_NORMAL;
    case FAITH_DIFF_HARD:   return FAITH_SIGNAL_GRAZE_LASER_HARD;
    case FAITH_DIFF_LUNATIC:
    case FAITH_DIFF_EXTRA:
    default:                return FAITH_SIGNAL_GRAZE_LASER_LUNATIC;
    }
}

/* ---- 回收线档位（一手 §2.2；分界 = 线上 > 64 px / 48…64 px / 0…48 px / 线下）---- */
int signal_core_band(int32_t above_line_px_x100)
{
    if (above_line_px_x100 > FAITH_SIGNAL_BAND_TOP_PX * 100) return FAITH_SIGNAL_BAND_TOP;
    if (above_line_px_x100 > FAITH_SIGNAL_BAND_MID_PX * 100) return FAITH_SIGNAL_BAND_MID;
    if (above_line_px_x100 > 0) return FAITH_SIGNAL_BAND_LOW;
    return FAITH_SIGNAL_BAND_OFFLINE;      /* 恰好在线（0 px）⇒ 线下（一手 `y >= 128` 那一支）*/
}

int32_t signal_core_step(int32_t signal_now, int band, int32_t graze, int difficulty)
{
    int32_t gain = 0;

    /* 擦弹是**独立通道**（一手 §3：4 个站点都不读玩家 y）⇒ 先按难度把它折成增量。*/
    if (graze > 0) gain = graze * signal_core_graze_gain(difficulty, 0);
    return faith_signal_step(signal_now, band, gain);
}

int32_t signal_core_clamp(int32_t v)
{
    if (v < 0) return 0;
    if (v > SIGNAL_MAX) return SIGNAL_MAX;
    return v;
}

int32_t signal_core_apply(faith_slot_t *s, int32_t d)
{
    if (!s) return 0;
    faith_signal_add(s, d);          /* 共用件里已经夹了 [0, FAITH_SIGNAL_MAX] */
    return s->signal;
}

int32_t signal_core_apply_step(faith_slot_t *s, int band, int32_t graze, int difficulty)
{
    if (!s) return 0;
    return signal_core_apply(s, signal_core_step(s->signal, band, graze, difficulty));
}

int signal_core_reset(faith_slot_t *s)
{
    if (!s) return 0;
    if (s->signal == 0) return 0;
    faith_signal_reset(s);
    return 1;
}

int signal_core_is_full(int32_t signal_now)
{
    return signal_now >= SIGNAL_FULL_AT;
}

int32_t signal_core_pickup(faith_slot_t *s, faith_ev_t ev, int32_t value)
{
    /* `value` = 这颗实体的单颗值（大绿点 100 / 中绿点 10，由卡侧 `faith_spawn()` 给）。
     * 加值 / Δ_111 上限（= 原版基础）都由共用件夹；本函数只决定「走哪个档」。
     * ★ 2026-09-15：**不再动连击槽** —— th11 一手没有连击槽（`research-th11-faith-and-points.md` §6.1
     *   的信仰写入点穷举里既无连击槽也无逐帧衰减）⇒ 旧的那笔 `faith_combo_add()` 是错加的机制。*/
    if (!s || value <= 0) return 0;
    return faith_slot_add(s, ev, value);   /* 定点 ×FAITH_FIXED；夹到上限时 = 0 */
}

int32_t signal_core_strength(int32_t signal_now)
{
    /* 一手 `th11:0x41f640`：`A = min(⌊槽存储/100⌋, 100)`，单位 0.01。
     * 我们的槽是**千分点**（1 显示点 = 1000，显示点 = 槽存储/100）⇒
     *   `⌊槽存储/100⌋ = ⌊signal_now/1000⌋` ⇒ `A = min(signal_now/1000, 100)`，
     *   返回千分点 `A × 10`（0…1000）—— 与 `mof_core_graze_mult()`（`B × 10`）同刻度。*/
    int32_t a;
    if (signal_now <= 0) return 0;
    if (signal_now >= SIGNAL_HI_MARK) return SIGNAL_UNIT;   /* 满（≥100.00）一律 1.00（`0x41f64c`）*/
    a = signal_now / SIGNAL_UNIT;                           /* ⌊显示值⌋ ∈ 0..99 */
    return a * 10;
}

/* ---- 蓝点分（★ 一手整数式，见 .h 的公式块）---- */
#define SIGNAL_BLUE_MIN 10      /* 一手 `th11:0x423bce: mov ebx,0xa`（S ≤ 0 ⇒ 抬到 10）*/

/* trunc10（`th11:0x41f526`–`0x41f532` 的 `idiv 10` + `sub eax,edx`）：截断到 10 的倍数。*/
static int32_t signal_trunc10(int32_t v)
{
    if (v <= 0) return 0;
    return (v / 10) * 10;
}

/* `S = trunc10((A + B) × Y / 100)`，`S ≤ 0 ⇒ 10`。
 * 入参：`faith_total` = 信仰**显示值**（`Y` 的原料）；`a_permille` = `A × 10`；`b_permille` = `B × 10`。
 * 全程整数：`(A+B)×10 = a_permille + b_permille` ⇒ `(A+B)×Y/100 = (a+b) × Y / 1000`。*/
static int32_t signal_blue_value(int32_t faith_total, int32_t a_permille, int32_t b_permille)
{
    int64_t x, y, s;

    if (faith_total <= 0) return SIGNAL_BLUE_MIN;        /* Y = 0 ⇒ S = 0 ⇒ 抬到 10 */
    y = (int64_t)signal_trunc10(faith_total);            /* Y = trunc10(⌊PIV/100⌋) */
    x = (int64_t)(a_permille / 10) + (int64_t)(b_permille / 10);   /* X = A + B（单位 0.01）*/
    if (x <= 0) return SIGNAL_BLUE_MIN;
    s = (x * y) / 100;                                   /* ⌊(A+B)×Y/100⌋（非负 ⇒ 除法即向下取整）*/
    s = (s / 10) * 10;                                   /* trunc10（`lea [eax+eax*4]; add ebx,ebx`）*/
    if (s <= 0) return SIGNAL_BLUE_MIN;
    if (s > 0x7fffffff) s = 0x7fffffff;                  /* 防御：调用方量级下到不了 */
    return (int32_t)s;
}

int32_t signal_core_bonus(int32_t faith_total, int32_t graze_mult_permille, int32_t signal_now)
{
    if (graze_mult_permille < 0) graze_mult_permille = 0;   /* `mof_core_graze_mult` 不会给负数 */
    return signal_blue_value(faith_total, signal_core_strength(signal_now), graze_mult_permille);
}

int32_t signal_core_bonus_fixed(int32_t faith_fixed, int32_t graze_mult_permille, int32_t signal_now)
{
    int32_t s = signal_core_bonus(faith_fixed / FAITH_FIXED, graze_mult_permille, signal_now);
    return s * FAITH_FIXED;      /* 与入参同刻度（×FAITH_FIXED）；S 已经是 10 的倍数，无额外误差 */
}

/* ---- 契约 PV（旧口径的回归面；2026-09-13 起不参与落笔）---- */
int32_t signal_core_pv_111(const faith_slot_t *s111, int32_t aggregate, int have110)
{
    int64_t pv;

    if (!s111) return 0;
    if (!have110) {
        /* 只装 111：`PV = 信仰_111`（基础按难度 + Δ_111）。⚠ 不读聚合值 ——
         * 111 单装时聚合值也是它自己，但直接读槽更少一处约定（且与基础口径绑定）。*/
        return faith_score_unit(s111);
    }
    /* 两卡同装：`PV = 基础均值 + Δ_110 + Δ_111`，而
     *   `aggregate` = 两卡各自的「基础 + Δ」之和（每张只声明一份自己的基础）
     * ⇒ `Δ 之和 = aggregate − 基础_110 − 基础_111` ⇒ 基础只取一份。*/
    pv = (int64_t)(FAITH_BASE_110 + s111->base) / 2
         + (int64_t)aggregate - (int64_t)FAITH_BASE_110 - (int64_t)s111->base;
    if (pv < 0) pv = 0;
    if (pv > 0x7fffffff) pv = 0x7fffffff;
    return (int32_t)pv;
}

int32_t signal_core_pv_111_scaled(const faith_slot_t *s111, int32_t aggregate, int have110,
                                  int32_t graze_mult_permille, int32_t signal_now)
{
    /* ★ 2026-09-15：改成与「蓝点分」**同一个式子**（一手里 PV 就是蓝点分：
     *   `th11:0x423b7c` 那一段算出来的 S 既进弹窗、又进 `add_to_score`）。*/
    return signal_core_bonus(signal_core_pv_111(s111, aggregate, have110),
                             graze_mult_permille, signal_now);
}

/* ---- Miss（一手 `th11:0x4327d0` 段）---- */
int32_t signal_core_miss_penalty(faith_slot_t *s)
{
    if (!s) return 0;
    return faith_slot_sub(s, FAITH_SA_MISS_PIV_LOSS);      /* Δ 夹 0；返回值 = 真正扣掉的显示值 */
}

/* ---- 命碎 / 残机（一手复核见 .h 的说明块）---- */
static const int64_t s_frag_score[2][SIGNAL_LIFEFRAG_TABLE_LEN] = {
    { 1000000000LL },   /* 通常表 `th11:0x4a3b1c`（**只取到第 0 项**）*/
    { 1000000000LL }    /* EX 表  `th11:0x4a3d60`（同上）*/
};

int32_t signal_core_lifefrag_table_len(void) { return SIGNAL_LIFEFRAG_TABLE_LEN; }

int64_t signal_core_lifefrag_threshold(int32_t idx, int ex)
{
    if (idx < 0 || idx >= SIGNAL_LIFEFRAG_TABLE_LEN) return -1;    /* ❓ 表未取到 ⇒ 没有这一项 */
    return s_frag_score[ex ? 1 : 0][idx];
}

int signal_core_lifefrag_score(int64_t score, int32_t *idx, int ex)
{
    int64_t thr;

    if (!idx) return 0;
    if (*idx < 0 || *idx >= SIGNAL_LIFEFRAG_TABLE_LEN) return 0;   /* 越界 ⇒ 不再发（❓ 见 .h）*/
    thr = s_frag_score[ex ? 1 : 0][*idx];
    /* 一手 `th11:0x41efe3: cmp edx,[eax*4+0x4a3b1c]` + `0x41efff: jl` ⇒ `score >= thr` 才发。*/
    if (score < thr) return 0;
    ++(*idx);          /* 一手 `th11:0x41f006: inc DWORD PTR ds:0x4a5750` */
    return 1;
}

int signal_core_is_lifefrag_item(uint32_t type)
{
    /* TH18 的命碎道具 = item type `0x10`（EXTEND2 卡道具 → `CardLifeFragment` id 3）。
     * ⚠ 旧值 4 已订正：type 4 **不在** TH18 的道具分派 switch 里（`engine/card/th18/05-shop-and-money.md`
     * §8 的 type 速查表里没有它），属「旧引擎血统残留」（`cards/collect2x_core.h` 的既有结论）。*/
    return type == (uint32_t)SIGNAL_ITEM_TYPE_LIFEFRAG;
}

/* ---- ★★ 命碎段机（全部论证与一手出处写在 `signal_core.h` 的那一段里）------------------
 * 这里只有状态推进：没有浮点、没有引擎地址、没有「位置」概念（撒在哪由卡侧按一手取）。*/

void signal_seg_init(signal_seg_t *g)
{
    if (!g) return;
    g->open = 0;
    g->is_spell = 0;
    g->died = 0;
    g->pending = 0;
    g->segs = 0;
    g->segs_clean = 0;
    g->drops = 0;
    g->last_sig = 0;
    g->last_spell = 0;
    g->last_boss = 0;
    g->debounce = 0;
    g->frame = 0;
    g->start_frame = 0;                 /* ★ 2026-09-16m：段长计时的起点 */
    g->last_len = -1;                   /* ★ 诊断：还没结算过任何一段 */
}

/* 换关开场：只重挂边沿记忆（保留 `pending` 余数与诊断计数）——理由见 `signal_core.h`。*/
void signal_seg_rebase(signal_seg_t *g)
{
    if (!g) return;
    g->open = 0;
    g->is_spell = 0;
    g->died = 0;
    g->last_sig = 0;
    g->last_spell = 0;
    g->last_boss = 0;
    g->debounce = 0;
}

/* 收一段：**本段没死过** ⇒ 攒 1 个机会；攒够 `SIGNAL_SEG_DROP_EVERY` ⇒ 输出「撒 1 颗」并把余数留下。*/
/* ★★ 2026-09-16m：**段太短就不算一段**（用户实跑两次报「对话结束后发残碎」）。
 * 机制：对话**结束**那一帧之后，boss/相位会重新出现 ⇒ 机器按设计开一段；可紧接着又出现一次
 * 边沿（新 boss 的相位槽登记）⇒ 这个「刚开几帧」的段被当成**一整段**结算、且必然「没死过」
 * ⇒ 攒够 2 段就白撒一颗碎片。一手：真段（攻击相位）都是几百帧量级，几帧的段不存在。
 * ⇒ 段长 < `SIGNAL_SEG_MIN_FRAMES` 的**只记诊断、不记账**（既不 +`segs_clean`、也不 +`pending`）。
 * ❓ 自定（100 帧 ≈ 1.7 s；理由 = 远大于任何边沿抖动、远小于最短的真相位）。
 * 常量本体在 `signal_core.h`（主机单测要用它来构段）。*/

static void signal_seg_resolve(signal_seg_t *g, int32_t *out_drop)
{
    int32_t len;

    if (!g || !g->open) return;
    len = g->frame - g->start_frame;
    g->last_len = len;
    ++g->segs;
    if (len < SIGNAL_SEG_MIN_FRAMES) {              /* 太短 ⇒ 不记账（见上面那段说明）*/
        g->open = 0;
        g->died = 0;
        g->is_spell = 0;
        return;
    }
    if (!g->died) {
        ++g->segs_clean;
        ++g->pending;
        if (g->pending >= SIGNAL_SEG_DROP_EVERY) {
            g->pending -= SIGNAL_SEG_DROP_EVERY;      /* 余数跨段保留（用户口径：满 2 清零）*/
            ++g->drops;
            if (out_drop) *out_drop = 1;
        }
    }
    g->open = 0;
    g->died = 0;
    g->is_spell = 0;
}

int32_t signal_seg_tick(signal_seg_t *g, const signal_seg_in_t *in, int died_this_frame, int32_t *out_drop)
{
    int32_t ev = SIGNAL_SEG_EV_NONE;
    int boss, edge = 0, effective;

    if (out_drop) *out_drop = 0;
    if (!g || !in) return SIGNAL_SEG_EV_NONE;
    ++g->frame;
    if (g->debounce > 0) --g->debounce;      /* ⚠ 被吞的边沿**不重置**窗口：窗口是「切换后 N 帧」*/

    boss = in->boss_present ? 1 : 0;

    /* ★★ 2026-09-16：**对话中只吸收、不结算、不开段**（用户实跑：「一面结束对话后进关底直接就掉了
     *   一个残碎片」）。机制：关底 boss 被击破 → 对话 → boss 对象随后被释放 ⇒ 原来「boss 1→0」
     *   会被判成**又一次**段收尾 ⇒ 多算一段 ⇒ 攒够 2 段就撒一颗。一手：那一次收尾是 `BossDead`
     *   子程序自己的 `dropMain(5)`（对话**之前**就跑完了），对话期间没有任何 `dropMain` ⇒ 闭嘴。
     *   ⚠ 必须**照常更新 `last_*`**（= 吸收），否则对话结束那一帧会把「对话期间发生的变化」
     *     补成一次边沿，症状原样复现。*/
    if (in->dialogue) {
        if (in->phase_sig) g->last_sig = in->phase_sig;
        g->last_spell = in->spell_active ? 1 : 0;
        g->last_boss = boss;
        g->open = 0;                         /* 段在对话里作废：**不**结算、**不**置 `pending` */
        g->died = 0;
        if (out_drop) *out_drop = 0;
        return SIGNAL_SEG_EV_NONE;
    }

    /* ① 边界判据 = 三个原生信号的任一跳变。
     *    · boss 1→0（最后一段收尾）优先 —— 那一刻相位槽与符卡位都会被一起收掉；
     *    · 相位指纹变化（**只在非 0 时更新** ⇒ 「槽清空 → 新槽」合成一次）；
     *    · 符卡位边沿（起 / 收）。*/
    if (g->last_boss && !boss) edge = 1;
    else if (in->phase_sig && g->last_sig && in->phase_sig != g->last_sig) edge = 1;
    else if (in->spell_active != g->last_spell) edge = 1;

    effective = (edge && g->debounce == 0);  /* 去抖窗口内的多余边沿：吞掉，不算第二次切换 */

    if (effective) {
        if (g->open) {
            signal_seg_resolve(g, out_drop);
            ev |= SIGNAL_SEG_EV_END;
        }
        g->debounce = SIGNAL_SEG_DEBOUNCE;
    } else if (g->open && died_this_frame) {
        g->died = 1;                          /* 死亡归属：算在**本帧处理完仍开着的那个段**上 */
    }

    /* ② 开新段（边界之后，或 boss 刚出现）。边界那一帧的死亡算**新段**的
     *    —— 边界是靠「相位信号已经变成新段的值」认出来的，所以那一帧已经属于新段。*/
    if (!g->open && boss) {
        g->open = 1;
        g->is_spell = in->spell_active ? 1 : 0;
        g->died = died_this_frame ? 1 : 0;
        g->start_frame = g->frame;                  /* ★ 2026-09-16m：算段长用 */
        ev |= SIGNAL_SEG_EV_START;
    }

    /* ③ 段内补标符卡位：符卡通常在相位开始后几帧才宣告（形状同 `lolk_core.c` 的 `saw_spell`）。*/
    if (g->open && in->spell_active) g->is_spell = 1;

    /* ④ 记忆。⚠ 指纹只在非 0 时更新 —— 相位切换中间那几帧槽是空的（`hp_value = -1`），
     *    把 0 记进去会把一次切换拆成「旧→0」与「0→新」两个边沿。*/
    if (in->phase_sig) g->last_sig = in->phase_sig;
    g->last_spell = in->spell_active ? 1 : 0;
    g->last_boss = boss;
    return ev;
}

/* ---- HUD：连续填充条（一手 §6.1(b)）---- */
int32_t signal_core_bar_level(int32_t signal_now)
{
    int32_t v;

    if (signal_now <= 0) return 0;
    v = signal_now / SIGNAL_UNIT;                     /* 显示值（= 槽存储/100）*/
    if (v > 100) v = 100;                             /* 一手 `th11:0x41b67f/0x41b687` 的钳 */
    return v / 10;                                    /* level ∈ 0..10 */
}

int32_t signal_core_bar_width_px(int32_t signal_now)
{
    return SIGNAL_BAR_BASE_PX + SIGNAL_BAR_STEP_PX * signal_core_bar_level(signal_now);
}

/* ======================================================================================
 * 六条「支援特技」的纯逻辑（依据与出处全部在 `signal_core.h` 的那一段注释里）
 * ==================================================================================== */

/* ---- ① 霊夢×紫：缺口瞬移 ----------------------------------------------------------
 * 状态机逐条对齐 `th11:0x430e50`（tick）+ `th11:0x4307b0`–`0x430826`（起手/钳位）；
 * 每个分支的地址与语义写在 `signal_core.h` 的那张表里。**「松手再按」是硬要求**
 * （state 1/3 里 `dir==同侧` 只 `frames++`，永远走不到 state 2/4）。*/
int signal_gap_edge(uint32_t input, int32_t x_fixed)
{
    if (x_fixed <= -SIGNAL_GAP_EDGE_FIXED) return (input & SIGNAL_IN_LEFT)  ? SIGNAL_GAP_DIR_LEFT  : 0;
    if (x_fixed >=  SIGNAL_GAP_EDGE_FIXED) return (input & SIGNAL_IN_RIGHT) ? SIGNAL_GAP_DIR_RIGHT : 0;
    return 0;
}

static void signal_gap_reset(signal_gap_t *g)
{
    g->phase = SIGNAL_GAP_IDLE;
    g->dir = 0;
    g->frames = 0;
    g->warp_dir = 0;
}

int signal_gap_tick(signal_gap_t *g, uint32_t input, int32_t x_fixed, int native_active, int *out_dir)
{
    int32_t side, d;

    if (out_dir) *out_dir = 0;
    if (!g) return 0;

    /* 原生瞬移还在跑（`th18:0x409a60` 已把方向位置 1，`0x45b170` 第 16 帧才清）：
     * 压回 IDLE、不认边、不计数 —— 否则每一帧都会重开一次 16 帧动画。
     * ⚠ 这是我们**自己加的安全门**（一手 TH11 里没有对应物：TH11 的等价门是 GUI/Boss 那两条）。*/
    if (native_active) {
        if (g->phase != SIGNAL_GAP_IDLE) ++g->blocked;
        signal_gap_reset(g);
        return 0;
    }
    if (g->phase == SIGNAL_GAP_ACTIVE) signal_gap_reset(g);   /* 原生那边收尾了（方向位被它自己清）*/

    /* 取消：`state != 0` 时按到 `INPUT & 0x9`（**射击或低速**）⇒ state=0, frames=0（`th11:0x430e79`）。
     * ⚠ 一手在 `state == 0` 时**不判**这一条（`0x430e5d: cmp eax,edx / je 0x431068` 在它之前）
     *   ⇒ 这里也只在非 IDLE 时判（否则「按着 Z 贴边」永远武装不上，而一手是先武装、下一帧才被取消）。*/
    if (g->phase != SIGNAL_GAP_IDLE && signal_core_pressing_atk(input)) {
        signal_gap_reset(g);
        return 0;
    }

    side = (x_fixed <= -SIGNAL_GAP_EDGE_FIXED) ? SIGNAL_GAP_DIR_LEFT
         : (x_fixed >=  SIGNAL_GAP_EDGE_FIXED) ? SIGNAL_GAP_DIR_RIGHT : 0;
    d = signal_core_dir_of(input);

    switch (g->phase) {
    case SIGNAL_GAP_IDLE:
        /* 起手（`th11:0x4307de` / `0x43080f`）：已被钉在该侧、且 `state==0` ⇒ 武装。
         * ⚠ 一手起手段**不看方向键**（只看 x 与 state），第二次按下才是方向键的任务 ⇒ 照抄。*/
        if (side) { g->phase = SIGNAL_GAP_ARMED; g->dir = side; g->frames = 0; }
        break;
    case SIGNAL_GAP_ARMED:
        if (d == 0) {                                  /* 松手 ⇒ 开窗（`th11:0x430ea9`→state 2）*/
            g->phase = SIGNAL_GAP_WAIT;
            g->frames = 1;                             /* 一手 `0x430eb3`：= 1，不是 0 */
        } else if (d == g->dir) {
            ++g->frames;                               /* 还按着同侧 ⇒ 只 frames++（`th11:0x431062`）*/
        } else {
            signal_gap_reset(g);                       /* 按了另一侧 ⇒ state=0（`th11:0x430ed0`）*/
        }
        break;
    case SIGNAL_GAP_WAIT:
        if (g->frames > SIGNAL_GAP_WINDOW_VERDICT) {   /* 超时（`th11:0x430ee5: cmp …,0x8 / jg`）*/
            signal_gap_reset(g);
            break;
        }
        if (d == g->dir) {                             /* ★ 第二次同方向 ⇒ 发动（`th11:0x430eff`）*/
            g->phase = SIGNAL_GAP_ACTIVE;
            g->frames = 0;
            g->warp_dir = g->dir;
            ++g->wins;
            if (out_dir) *out_dir = (int)g->dir;
            return 1;
        }
        ++g->frames;                                   /* 否则只 frames++（`th11:0x430f62`）*/
        break;
    case SIGNAL_GAP_ACTIVE:
    default:
        signal_gap_reset(g);
        break;
    }
    return 0;
}

/* ---- ②③ 萃香 / 文：连续不按键计数器（一手 `th11:0x43077a` / `0x430651`）----
 * ★ 与上一版的差别：**触发不清零**（一手只在判据不成立时归零）⇒ 返回的是**持续态**；
 *   卡侧的「付费」用 `rising` 这个**上升沿**（❓ 自定，见 .h）。*/
int signal_hold_tick(signal_hold_t *h, int ok, int *rising)
{
    int was;

    if (rising) *rising = 0;
    if (!h) return 0;
    if (!ok) {
        h->frames = 0;
        h->active = 0;
        return 0;
    }
    /* 一手 `0x43065f`/`0x43078f`：先 `inc`，再 `cmp …,0xa` + `jle`
     * ⇒ 阈值是**严格大于 10**。一手让计数器无界自增，这里钳在 11（判据只看 `>10`，等价）。*/
    if (h->frames <= SIGNAL_SUPPORT_HOLD_FRAMES) ++h->frames;
    was = h->active;
    h->active = (h->frames > SIGNAL_SUPPORT_HOLD_FRAMES) ? 1 : 0;
    if (h->active && !was) {
        ++h->fires;
        if (rising) *rising = 1;
    }
    return h->active;
}

/* ---- ④ 魔理沙×爱丽丝：灵击 0.5 档 + P 点 +0.02/+0.03 交替（2026-09-15g）
 * 依据 / 一手核对（`pl01a.sht +0x24 = 12`，**核不到 0.50**）/ 门槛与下限的选择 / 为什么用交替 —— 全部
 * 写在 `signal_core.h` 的 ④ 段，这里不重复推理，只放可执行形式（主机单测钉得住）。*/
mof_power_t signal_core_alice_power_spend(int32_t power, int alice_on)
{
    /* ★ 2026-09-24：规则本体搬进**共用件**（`mof_core_bomb_power_spend()`）—— 因为 owner 口径是
     *   「110 与 111 的灵击是**完全一套系统**，半档是 111+魔理沙 的额外特技」，无论谁放都该走同一条
     *   （原来只有 111 自己放的那一发吃到半档）。本函数留作**兼容入口**，行为与旧版逐值相同，
     *   主机单测（`test_signal_core`）原样通过。*/
    return mof_core_bomb_power_spend(power, alice_on);
}

/* ⛔ **已退役（2026-09-15h）**：`signal_alice_p_total()` / `signal_alice_p_topup()` 两支连同
 * `signal_alice_t`（含交替相位 `alt`）一起**搬进共享件** —— 因为 P 点补值的落笔者**恒为 110**
 * （`mof_core_p_owner()` 选举），相位住在 111 的卡状态里够不到（卡之间不许读对方状态）
 * ⇒ 双卡同装时爱丽丝的 P 点减益会被绕过。
 * 现在的位置：纯逻辑 `faith_core.h` 的 `faith_core_p_topup_total()`、相位 + 判据 `faith.h` 的
 * `faith_p_topup_take()`、补值 `mof_core_p_topup_units()`（`mof_core.h`）。
 * ⚠ 这里**不许**再长回第二份相位（两个 `alt` 会分叉成两条序列，见 `faith.h` 的不变式）。*/

/* ---- ⑤ 魔理沙×帕秋莉：五档阵型（★★ 2026-09-16zb：**换成 TH11 一手几何**）----
 *
 * 一手链条（自己反汇编 + 自己读样本；全过程 = `engine/player/th11/04-marisa-b-option-geometry.md`）：
 *   · 数据在 **`pl01b.sht + 0x28 + formation*0x50`**（`0x50` = 10 条 × 8 字节 = 一个 formation 的表）；
 *     每条 = **(x, y) 两个 float**（px）。原版把**第 1 个加到 `player+0x888`（x）、第 2 个加到
 *     `player+0x88c`（y，**向下为正**）** —— `th11:0x433092` / `0x433098`。
 *   · 条目索引 = **`T(档−1) + 子机序号`**（`B[档−1]` = 静态表 `0x4a7ac8` 给的三角数 0/1/3/6 =
 *     每档块的起点；子机序号 = 循环尾 `th11:0x43353f inc esi` / `0x43354d cmp esi,[esp+8](档数)`）。
 *     ⇒ 每张表 10 条 = Σ(档 1..4) ✓（实测 `pl01b` 正好 50 条、`pl01c` 20 条）。
 *   · `Player::repopulate_options`（`th11:0x432cc0`）把**每个 formation 的表都读一遍**写进该子机的
 *     `+0x74 + formation*8`（5 个读点 `+0x28/+0x78/+0xc8/+0x118` 步长 `0x50` ✓）；运行期
 *     `Player::move`（`th11:0x430970`）再按当前 formation 把那一对拷进"当前/目标"字段。
 *   · **聚焦不改变偏移**（`[ebx+0x8d20]` = Shift 位，`th11:0x430368` 解得；它只决定取"当前"还是
 *     "目标"那一份去算出生点）。
 *
 * ⚠ 本表**逐值照抄**上面那条链；单位从 px 换成我们表用的 1/10 px（×10）。
 *   **y 不取负**：原版 `+y = 下`，本仓 `signal_cell_t` 的口径（`y 负 = 在自机上方`）与它**同向**。
 * ⚠ 元素顺序沿用本仓既有的 formation↔元素 映射（`pl0X.anm` 脚本 `0x22+formation`：火/水/木/金/土），
 *   与"发射方向 ↔ 站位"的交叉印证自洽（火 = 上方 + 两侧、木 = 斜上、土 = 下方、金 = 竖列）。
 * ⚠ 原来那张 ❓ 自定表与 `signal_formation_cell_sym()`（"按对称轴取格"）**已作废**：
 *   一手表**每档的块本身就是该档的 L 个格子**（x 逐档对称）⇒ 直接取 `T(档−1)+序号` 即可。*/
static const signal_cell_t s_formation_geo[SIGNAL_FORMATION_COUNT][10] = {
    /* 0 火（`pl01b.sht + 0x28`）：档1 (0,-32) / 档2 (±24,0) / 档3 (0,-32)+(±24,0) / 档4 (±24,±12) */
    { {    0, -320 }, {  240,    0 }, { -240,    0 }, {    0, -320 },
      {  240,    0 }, { -240,    0 }, {  240,  120 }, { -240,  120 },
      {  240, -120 }, { -240, -120 } },
    /* 1 水（`+0x78`）：整档在下方 24 px —— 档2 ±16 / 档3 (0,±24) / 档4 (±30,±10) */
    { {    0,  240 }, { -160,  240 }, {  160,  240 }, {    0,  240 },
      { -240,  240 }, {  240,  240 }, { -300,  240 }, {  300,  240 },
      { -100,  240 }, {  100,  240 } },
    /* 2 木（`+0xc8`）：斜上前伸 —— 档2 (±32,-24) / 档3 (0,-32)+(±32,-24) / 档4 (±32,-12)+(±24,-32) */
    { {    0, -320 }, { -320, -240 }, {  320, -240 }, {    0, -320 },
      { -320, -240 }, {  320, -240 }, { -320, -120 }, {  320, -120 },
      { -240, -320 }, {  240, -320 } },
    /* 3 金（`+0x118`）：竖列（x 恒 0）—— 档2 (0,∓40) / 档3 加 (0,-80) / 档4 加 (0,80) */
    { {    0, -400 }, {    0, -400 }, {    0,  400 }, {    0, -400 },
      {    0,  400 }, {    0, -800 }, {    0, -400 }, {    0,  400 },
      {    0, -800 }, {    0,  800 } },
    /* 4 土（`+0x168`）：下方聚拢 —— 档2 (±10,20) / 档3 (0,20)+(±12,30) / 档4 (±10,20)+(±20,30) */
    { {    0,  200 }, {  100,  200 }, { -100,  200 }, {    0,  200 },
      {  120,  300 }, { -120,  300 }, {  100,  200 }, { -100,  200 },
      {  200,  300 }, { -200,  300 } },
};

int32_t signal_formation_tri(int32_t level)
{
    if (level <= 1) return 0;
    if (level > SIGNAL_FORMATION_LEVELS) level = SIGNAL_FORMATION_LEVELS;
    return level * (level - 1) / 2;
}

/* 取「第 `level` 档（1..4）的第 `idx` 格」（0-based）的一手偏移；成功返回 1。*/
int signal_formation_cell_lv(int32_t formation, int32_t level, int32_t idx, signal_cell_t *out)
{
    int32_t base;
    if (!out || formation < 0 || formation >= SIGNAL_FORMATION_COUNT) return 0;
    if (level < 1 || level > SIGNAL_FORMATION_LEVELS) return 0;
    if (idx < 0 || idx >= level) return 0;
    base = signal_formation_tri(level);
    *out = s_formation_geo[formation][base + idx];
    return 1;
}

/* ⛔ **已退役（2026-09-16zb）**：`s_formation[][]`（❓ 自定五档阵型）与 `signal_formation_cell_sym()`
 *   （"按对称轴取格"）—— 两者都是"一手取不到"时的替代品。现在一手表拿到了（见上），
 *   而且**每档的块本身就是该档的 L 个格子且 x 对称** ⇒ 直接 `signal_formation_cell_lv()` 即可，
 *   「左倾」问题在数据层面就不存在了。
 *   保留旧函数名 `signal_formation_cell()` 只为兼容主机单测（它退化成"档 4 的前 4 格"）。*/
int signal_formation_cell(int32_t formation, int32_t idx, signal_cell_t *out)
{
    return signal_formation_cell_lv(formation, SIGNAL_FORMATION_LEVELS, idx, out);
}

/* ★★ 2026-09-16m：**按对称轴取格**（用户实跑：「子机的位置摆放不对，**左倾严重，低 p 不对称**」）。
 *
 * 阵型表 `s_formation[.][4]` 的四格是**左右成对**排的（0 = 最左 … 3 = 最右，x 左右对称、
 * y 各档不同）。原来「取前 n 格」的写法在 n < 4 时必然是**偏左的那几格**：
 *   n=1 → 最左那一格；n=2 → 左边两格；n=3 → 左边两格 + 靠轴一格 —— 正是用户看到的「左倾」。
 *
 * 本函数改成按**对称轴**取（n = 本次声明几格，idx = 第几格，0..n-1）：
 *   · n = 4 → 整张表（原样，表本身就对称）
 *   · n = 3 → 最内那一对 + **轴上**那一点（轴上点 = 内圈两格的 x 平均 = 0，y 取内圈的值）
 *   · n = 2 → **最内那一对**（左右各一）
 *   · n = 1 → **轴上**那一点
 * ⇒ 任何 n 都左右对称，且 n 越大越往外扩（1 → 2 → 3 → 4 是「从轴往外」的顺序）。
 *
 * ⚠ **❓ 自定**：一手（TH11 的五张阵型表）取不到 —— 它们在 `zPlayerOption+0x74`，全 exe **没有写点**
 *   （值来自 `pl*.sht` 的 option 位置块 `+0x40`–`+0xdf`，本仓未逐字段核对）⇒ 「对称」是我们自己定的口径。
 *   ⚠ 若将来把那块 sht 数据读进来，本函数就是被替换的那一处（`signal.c` 只有它一个调用点）。*/
int signal_formation_cell_sym(int32_t formation, int32_t idx, int32_t n, signal_cell_t *out)
{
    /* ⛔ 2026-09-16zb：**已退役的自定逻辑**（原来在这里按"对称轴"从 4 格表里凑 n 格）。
     *   一手表到手后「n 格」= 第 n 档的块本身（`signal_formation_cell_lv` 直接取）⇒ 这里只做委托，
     *   保留函数名是为了不动主机单测的调用点。*/
    return signal_formation_cell_lv(formation, n, idx, out);
}

int32_t signal_formation_wrap(int32_t n)
{
    int32_t m = n % SIGNAL_FORMATION_COUNT;
    if (m < 0) m += SIGNAL_FORMATION_COUNT;
    return m;
}

/* 纯映射：0..4 → 0..4（**第几个新组**）。真正的槽号 = `CE_SHT_SET_MARISA_FORMATION_0` + 本值，
 * 由 `signal.c` 加（`signal_core.c` 不 include 生成物 `sht_ids.h`，保持纯逻辑可主机单测）。*/
int32_t signal_formation_sht_set(int32_t formation)
{
    return signal_formation_wrap(formation);
}

/* ★★ 2026-09-16：子机格数 = 原生火力档（`.h` 的 B/C 两段是口径的唯一出处；这里只给可执行形式）。
 * 钳位与原生同向：**0 档 = 0 格**（不是 1 格）；上限 = 声明层给的格数上限（TH18 原生数组也是 4 格）。*/
int32_t signal_power_cells(int32_t power, int32_t per_level, int32_t cells_max)
{
    int32_t lvl;

    if (per_level <= 0 || cells_max <= 0) return 0;
    lvl = power / per_level;
    if (lvl < 0) lvl = 0;
    if (lvl > cells_max) lvl = cells_max;
    return lvl;
}

/* ---- ⑥ 魔理沙×荷取：**河童罩子**（2026-09-16q 按一手重写，对照表在 .h）---- */
int32_t signal_nitori_refund_of(int32_t cost)
{
    if (cost <= 0) return 0;
    /* 先乘后除：比值只由 NUM/DEN 决定（半档 = 50 单位，整数除法在这里不截断东西）。*/
    return cost * SIGNAL_NITORI_REFUND_NUM / SIGNAL_NITORI_REFUND_DEN;
}

void signal_nitori_open(signal_nitori_t *n, int32_t cost)
{
    if (!n) return;
    n->active  = 1;
    n->broken  = 0;
    n->pending = signal_nitori_refund_of(cost);
}

void signal_nitori_break(signal_nitori_t *n)
{
    if (!n) return;
    n->broken = 1;
}

int32_t signal_nitori_expire(signal_nitori_t *n)
{
    if (!n || !n->active) return 0;
    n->active = 0;
    if (n->broken || n->pending <= 0) {       /* 泡被撞过（一手：破罩那条路不退款）/ 本来就没扣到 */
        ++n->denials;
        n->pending = 0;
        return 0;
    }
    {   /* 泡自己走完 920 帧、没被撞 ⇒ 退一半 */
        int32_t r = n->pending;
        n->pending = 0;
        ++n->refunds;
        return r;
    }
}

void signal_nitori_close(signal_nitori_t *n)
{
    if (!n) return;
    n->active  = 0;
    n->pending = 0;
}

int32_t signal_nitori_break_radius_px(int32_t k)
{
    if (k <= 0) return SIGNAL_NITORI_BREAK_R0_PX;
    if (k > SIGNAL_NITORI_BREAK_FRAMES) k = SIGNAL_NITORI_BREAK_FRAMES;
    return SIGNAL_NITORI_BREAK_R0_PX + SIGNAL_NITORI_BREAK_DR_PX * (k - 1);
}

/* ★★ 2026-09-16u：文·超高速的残像上色（TH11 `pl00.anm` script19 的逐条等价式，见 .h）。*/
int32_t signal_trail_rgb(int32_t age)
{
    int32_t t, r, g, b;

    if (age < 0) age = 0;
    if (age > SIGNAL_TRAIL_RAMP_FRAMES) age = SIGNAL_TRAIL_RAMP_FRAMES;   /* 一手 colorTime(15, …) */
    t = age;
    r = 255 + (64 - 255) * t / SIGNAL_TRAIL_RAMP_FRAMES;                  /* color(255,64,64) */
    g = 64 + (64 - 64) * t / SIGNAL_TRAIL_RAMP_FRAMES;
    b = 64 + (255 - 64) * t / SIGNAL_TRAIL_RAMP_FRAMES;                   /* → (64,64,255) */
    if (r < 0) r = 0;
    if (r > 255) r = 255;
    if (g < 0) g = 0;
    if (g > 255) g = 255;
    if (b < 0) b = 0;
    if (b > 255) b = 255;
    return (r << 16) | (g << 8) | b;
}

/* ★★ 2026-09-16u：罩子的时钟（见 .h 的三个输入）—— 无敌 / 类无敌的帧**不推进寿命**。*/
int32_t signal_nitori_clock_runs(int32_t invuln_cur, int32_t no_hit, int32_t player_state)
{
    if (invuln_cur > 0) return 0;
    if (no_hit) return 0;
    if (player_state == 2 || player_state == 3 || player_state == 4) return 0;
    return 1;
}

/* ★★ 2026-09-16u：泡的可见曲线（一手 `alphaTime(10,0,255)` / `scaleTime(20,0,1,1)` /
 *   `scaleTime(900,0,0.1,0.1)` 的等价式；整数运算，两端都精确）。*/
int32_t signal_nitori_bubble_scale_milli(int32_t age)
{
    if (age <= 0) return 0;
    if (age < SIGNAL_NITORI_GROW_FRAMES)                   /* 0..20 帧：0 → 1.0 */
        return 1000 * age / SIGNAL_NITORI_GROW_FRAMES;
    if (age >= SIGNAL_NITORI_SHIELD_FRAMES)                /* 920 帧：到尽头（一手 0.1）*/
        return SIGNAL_NITORI_END_SCALE_MILLI;
    {   /* 20..920 帧：1.0 → 0.1（一手那 900 帧的 `scaleTime`）*/
        int32_t u = age - SIGNAL_NITORI_GROW_FRAMES;       /* 0..900 */
        int32_t t = SIGNAL_NITORI_SHIELD_FRAMES - SIGNAL_NITORI_GROW_FRAMES;  /* 900 */
        return 1000 - (1000 - SIGNAL_NITORI_END_SCALE_MILLI) * u / t;
    }
}

int32_t signal_nitori_bubble_alpha(int32_t age)
{
    if (age <= 0) return 0;
    if (age >= SIGNAL_NITORI_FADE_IN_FRAMES) return 255;
    return 255 * age / SIGNAL_NITORI_FADE_IN_FRAMES;
}

/* ============ ★ 2026-09-15c「灵击 → 决死」（决死 B 形态）—— 依据全在 `signal_core.h` 那段注释 ============
 * 与 110 侧（`mof_core.c` 的同名四个函数）是**两份独立的实现**：两卡的账本/数值表分家，
 * 且两边那条 🟡 的依据不同（110 = 引文被推翻后的自定；111 = 等 TH11 一手）⇒ 故意不共用。
 * 这里同样**没有浮点、没有引擎地址**（那个「4」由卡侧读进来）。*/

int32_t signal_core_deathbomb_window(int32_t player_state)
{
    /* 一手 `th18:0x45d4e4`：全 exe 只有这一处把 `player+0x476ac` 写成 4（卡侧本轮穷举复核）。*/
    return (player_state == SIGNAL_DEATHBOMB_STATE) ? 1 : 0;
}

void signal_bomb_form_start(signal_bomb_form_t *f, int32_t by_hit)
{
    if (!f) return;
    f->by_hit  = (by_hit == SIGNAL_BOMB_BY_HIT) ? SIGNAL_BOMB_BY_HIT : SIGNAL_BOMB_BY_PRESS;
    f->cancels = 0;
}

void signal_bomb_form_cancel_note(signal_bomb_form_t *f, int32_t bullets)
{
    if (!f || bullets <= 0) return;
    f->cancels += bullets;
}

int32_t signal_bomb_form_cancel_accounts(const signal_bomb_form_t *f)
{
    /* ✅ 一手：决死形态照常记账（开关与地址链见 `signal_core.h`）。*/
    if (f && f->by_hit == SIGNAL_BOMB_BY_HIT) return SIGNAL_BOMB_BY_HIT_ACCOUNT ? 1 : 0;
    return 1;
}

int32_t signal_bomb_form_opens_refund(const signal_bomb_form_t *f)
{
    /* ❓ 自定（理由见 `signal.c` 的 `signal_bomb()`）：决死形态默认不开荷取退款窗口。*/
    if (f && f->by_hit == SIGNAL_BOMB_BY_HIT) return SIGNAL_BOMB_BY_HIT_REFUND ? 1 : 0;
    return 1;
}

int32_t signal_bomb_form_is_deathbomb(const signal_bomb_form_t *f)
{
    return (f && f->by_hit == SIGNAL_BOMB_BY_HIT) ? 1 : 0;
}

/* ======================================================================================
 * ★★ 2026-09-15i：咲夜(2) / 早苗(3) 的六条支援特技 —— 纯逻辑
 * ======================================================================================
 * 依据 / 一手地址 / 用户口径的分层**全部**写在 `signal_core.h` 的那一段注释里，这里不重复推理，
 * 只放可执行形式（主机单测钉得住：`tests/test_signal_core.c` §17）。*/

/* ---- ① 大小姐：逐弹 3 帧循环（1 帧计入 + 2 帧不计）------------------------------------ */
void signal_remilia_reset(signal_remilia_t *r)
{
    if (!r) return;
    memset(r, 0, sizeof *r);
    /* `cd` 全 0 = 每颗都「本帧计入」⇒ 清完等价于「全都是第一次见到」。*/
}

int32_t signal_remilia_tick(signal_remilia_t *r, const uint16_t *keys, int32_t n,
                            uint16_t *out_keys, int32_t out_max)
{
    uint16_t nk[SIGNAL_REMILIA_TRACK_MAX];
    uint8_t  nc[SIGNAL_REMILIA_TRACK_MAX];
    int32_t  i, j, nn = 0, counted = 0;

    if (!r) return 0;
    if (!keys || n <= 0) return 0;         /* 取不到弹池 ⇒ 什么都不做（**不**清表，见 .h）*/
    ++r->frames;

    /* 逐颗处理：先从**上一帧**的表里找同一个槽号（找到 ⇒ 带着它的冷却；找不到 ⇒ 新弹，`cd = 0`），
     * 再按逐弹循环算这一帧计不计：
     *   `cd > 0` ⇒ 不计，`cd--`；`cd == 0` ⇒ **计入**，并把 `cd` 置回 `OFF_FRAMES`。
     * ⇒ 一颗一直在圈里的弹在它的第 1 / 4 / 7 … 帧计入（周期 3、有效 1 帧）。
     * ⚠ 表按「本帧见过的弹」整张重建（看不见的弹丢表）⇒ 同一帧里 O(n²)，n ≤ 36 ⇒ 最多 1296 次
     *   整数比较/帧，与 2000 槽的弹池扫描（`lolk.c` / `pofv.c` 每帧都在做）相比可以忽略。*/
    for (i = 0; i < n; ++i) {
        int32_t cd = 0, dup = 0;

        if (nn >= SIGNAL_REMILIA_TRACK_MAX) { ++r->dropped; continue; }  /* 表满 ⇒ 当帧不计、也不进表 */
        for (j = 0; j < nn; ++j) { if (nk[j] == keys[i]) { dup = 1; break; } }
        if (dup) continue;                        /* 同一槽号在同帧被喂两次（卡侧不会）⇒ 只算一次 */
        for (j = 0; j < r->n; ++j) { if (r->key[j] == keys[i]) { cd = r->cd[j]; break; } }
        if (cd > 0) --cd;                         /* 冷却中 ⇒ 本帧不计 */
        else {                                    /* ★ 本帧计入 */
            if (out_keys && counted < out_max) out_keys[counted] = keys[i];
            ++counted;
            cd = SIGNAL_REMILIA_GRAZE_OFF_FRAMES;
        }
        nk[nn] = keys[i];
        nc[nn] = (uint8_t)cd;
        ++nn;
    }

    memcpy(r->key, nk, sizeof(uint16_t) * (size_t)nn);
    memcpy(r->cd,  nc, sizeof(uint8_t)  * (size_t)nn);
    r->n = nn;
    r->total += counted;
    return counted;
}

/* ---- ② 二小姐：蓄力 300 帧 ⇒ 松手清屏 ------------------------------------------------- */
void signal_flandre_reset(signal_flandre_t *f)
{
    if (!f) return;
    memset(f, 0, sizeof *f);
}

int signal_flandre_tick(signal_flandre_t *f, int shift_held, int interrupted)
{
    int fire = 0;

    if (!f) return 0;

    if (interrupted) {
        /* ❓ 自定：中弹 ⇒ 蓄力清零（挨打了还留着蓄力不合理；登记 `TUNING.md` §2.1g）。*/
        if (f->charge > 0 || f->armed) ++f->breaks;
        f->charge = 0;
        f->armed = 0;
        f->last = shift_held ? 1 : 0;
        return 0;
    }

    if (shift_held) {
        if (f->charge < SIGNAL_FLANDRE_CHARGE_FRAMES) ++f->charge;
        if (f->charge >= SIGNAL_FLANDRE_CHARGE_FRAMES) f->armed = 1;
    } else if (f->last) {
        /* ★ 松手那一帧：充满了 ⇒ 清屏；没充满 ⇒ 什么都没发生（且蓄力清零）。*/
        if (f->armed) {
            fire = 1;
            ++f->fires;
        } else {
            ++f->aborted;
        }
        f->charge = 0;
        f->armed = 0;
    }
    f->last = shift_held ? 1 : 0;
    return fire;
}

int32_t signal_flandre_cancel_radius_x100(int32_t player_r_x100)
{
    /* 擦弹圈的「自机那一侧」= 判定半径 + 40.0 px（一手 `0x45cd90` 的 `0x4b92c4`；
     * 40.0 是 `max(br/2.5, 40.0)` 的下界 ⇒ 圈的**最小值**）。清屏半径 = 2×。*/
    int32_t r = player_r_x100;

    if (r <= 0) r = 300;                       /* 原生判定半径 3.0 px（`0x45ae99` 写 0x40400000）*/
    return SIGNAL_FLANDRE_RADIUS_MUL * (r + SIGNAL_FLANDRE_CIRCLE_MIN * 100);
}

int32_t signal_flandre_ring_ratio_permille(int32_t charge)
{
    /* 用户 2026-09-18：「张开一个逐渐变大的圆，最大了就知道满了」。
     * 线性 + 一个可见下限（120‰）—— 下限的理由：`charge == 1` 时若按 1/300，环只有 0.3 px，
     * 等于"蓄力前半秒什么都看不见"。**只影响观感**：判定用的半径始终是 `…_cancel_radius_x100()`。
     * ★ 全整数（不碰浮点）：千分比 = 120 + 880 × charge / 300 —— 端点与单调性都是精确的。*/
    if (charge <= 0) return SIGNAL_FLAN_RING_MIN_PERMILLE;
    if (charge >= SIGNAL_FLANDRE_CHARGE_FRAMES) return 1000;
    return SIGNAL_FLAN_RING_MIN_PERMILLE +
           (1000 - SIGNAL_FLAN_RING_MIN_PERMILLE) * charge / SIGNAL_FLANDRE_CHARGE_FRAMES;
}

/* ---- ④ 诹访子：贴板底 --------------------------------------------------------------- */
int signal_core_at_board_bottom(int32_t y_fixed)
{
    /* 一手 `th18:0x45b7e7`：引擎把 `player+0x630`（1/128 px 定点）钳在 `[0x1000, 0xd800]`
     * ⇒ `>= 0xd800` = 「已被引擎钉在板底」（不是我们挑的常数）。*/
    return y_fixed >= SIGNAL_BOARD_BOTTOM_FIXED ? 1 : 0;
}

/* ---- ⑤ 神奈子：收点线「快照基准 + 相对下移」 ------------------------------------------
 * ✅ 2026-09-15：共享 PoC 高度层（`sdk_core.h` / `sdk.h` 的 `ce_poc_*`）**已落地、卡侧已迁移**
 *   （`signal.c`：读 `ce_poc_height()`、声明 `ce_poc_declare_px_id()`、删卡写回 `ce_poc_release()`）。
 *   本文件这三个函数是「纯逻辑」那一半，**一个字没改**，照旧被卡侧用 + 单测覆盖。
 *   ⚠ `signal_kanako_restore()` 现在**没有卡侧调用者**了（层的基准记账更准：原生卡 22 中途把线抬到 224
 *   也不会被我们顶掉）—— 保留它是为了单测与将来复用，别顺手删。*/
void signal_kanako_reset(signal_kanako_t *k)
{
    if (!k) return;
    memset(k, 0, sizeof *k);
}

int32_t signal_kanako_tick(signal_kanako_t *k, int32_t poc_now_x100, int32_t drop_x100)
{
    int32_t want;

    if (!k) return 0;
    if (poc_now_x100 <= 0) return 0;              /* 现值读不到 ⇒ 这帧别写（也不动基准）*/

    if (k->base_x100 == 0) {
        k->base_x100 = poc_now_x100;              /* 第一次：现值就是基准 */
    } else if (poc_now_x100 != k->applied_x100) {
        /* ★ 现值 ≠ 我们上一帧写的值 ⇒ 有人改过（引擎 `0x45ae8f` 的 128.0 / 原生卡 `0x40c275` 的 224.0）
         * ⇒ **重取基准**（这样「原生卡把线抬到 224」不会被我们长期顶回 128）。*/
        k->base_x100 = poc_now_x100;
        ++k->resnaps;
    }
    if (drop_x100 < 0) drop_x100 = 0;
    /* ★ 2026-09-16y（用户口径「同装只多加 0.5」）：基准不是默认值 ⇒ **追加量减半**
     *   （典型 = 原生 22 号卡把线写成 224.0 ⇒ 我们只再加 48 ⇒ 272，而不是 320）。*/
    if (k->base_x100 != SIGNAL_KANAKO_POC_BASE_DEFAULT_PX * 100) drop_x100 /= 2;
    want = k->base_x100 + drop_x100;
    k->applied_x100 = want;
    ++k->writes;
    return want;
}

int32_t signal_kanako_restore(const signal_kanako_t *k)
{
    /* 从没写过 ⇒ 0（卡侧按「什么都不做」处理）。写过 ⇒ 写回基准值。*/
    if (!k || k->applied_x100 == 0 || k->base_x100 == 0) return 0;
    return k->base_x100;
}

/* ---- ⑥ 恋恋：放开全键位（除移动键）⇒ 4 帧无敌（量照抄原生，见 .h） ----------------------
 * 原生 Aya 卡（`th18:0x40c7f0` 的 `0x40ca95`–`0x40cb2a`）的载体与加法语义见 `signal_core.h`；
 * 这里只放**用户口径**的触发（放开那一下的下降沿）与量（1 帧）。*/
void signal_koishi_reset(signal_koishi_t *k)
{
    if (!k) return;
    memset(k, 0, sizeof *k);
}

int signal_koishi_tick(signal_koishi_t *k, uint32_t input, int32_t move_dir, int focusing)
{
    int fire = 0, now_action;

    if (!k) return 0;
    ++k->frames;

    /* ---- (a) 原生 Aya 的条件（✅ 复刻 `0x40c7f0` 的 `0x40c80a`/`0x40ca95`–`0x40cb2a`）----
     * 射击键按住 ⇒ 原生那条路**每帧把 `card+0x58` 强制 0 并早退**（`0x40c853`）⇒ 我们照抄：
     * 不记方向、也不判（所以「放开射击、还按着方向」下一帧就落在 `last_dir == 0` 那一边）。
     * 化简后的判据 = `last_dir == 0 && move_dir != 0 && !focusing`
     * （原生那两条 `last_dir != dir` / `last_dir != 0` 的跳过分支都被它蕴含）。*/
    if (input & SIGNAL_IN_SHOT) {
        k->last_dir = 0;
    } else {
        if (k->last_dir == 0 && move_dir != 0 && !focusing) { fire = 1; ++k->fires_native; }
        k->last_dir = move_dir;                 /* 照抄 `0x40cb3b` */
    }

    /* ---- (b) 用户口径（★ 2026-09-16：「**恋恋改成放开 shift 和 Z 吧**」+ 同日的补充澄清）----
     *   ★ 用户补充口径（逐字）：「**不是 or，是上一帧按着 shift or 按着 Z，这一帧不按 shift and
     *     不按着 Z**」⇒ 判据要读成**两个条件**：
     *       · **上一帧**：`Z` 按着 **或** `Shift` 按着（其中一个就够 —— 这就是「or」的位置）；
     *       · **这一帧**：`Z` **不**按 **且** `Shift` **不**按（**两个都得松** —— 这就是「and」的位置）。
     *     换句话说：**不是「放开任意一个」，是「两个都放开」**。
     *   ⇒ 实现就是「合并掩码从『有』掉到『没有』的那一个下降沿」：
     *       `prev_any = (input_prev & (Z|Shift)) != 0`、`now_any = (input & (Z|Shift)) != 0`、
     *       发动条件 = `prev_any && !now_any`。
     *     ⚠ 因此「上一帧 Z+Shift 都按着，这一帧只松开其中一个」⇒ `now_any` 仍为真 ⇒ **不发动** ✓
     *       （单测 §17.6 的 ④ 与 ⑦ 就是这条的钉子）。
     *   ⚠ 原口径是「放开全键位（除移动键）」= 六个动作键（`SIGNAL_IN_ACTION_MASK`）——
     *     已按新口径收窄成 `SIGNAL_IN_Z_OR_SHIFT`（X / C / 暂停 / 发动卡 / 切卡 不再参与）。*/
    now_action = (input & SIGNAL_IN_Z_OR_SHIFT) != 0u;
    if (k->prev_action && !now_action) { fire = 1; ++k->fires_user; }
    k->prev_action = now_action;

    return fire;
}

/* ---- ③④ 的落点：共用件那两个函数的上游修饰 ------------------------------------------- */
int32_t signal_core_step_mod(int32_t signal_now, int band, int32_t graze, int difficulty,
                             const signal_step_mod_t *m)
{
    int32_t gain = 0, d;

    /* ④ 诹访子：贴板底 ⇒ **整个不施加**线下那一档（换成 `HOLD`，线项 = 0、擦弹加值照给）。
     * ⚠ 不是「减了再夹 0」：那会把同帧擦弹挣来的加值一起吃掉（见 `signal_core.h`）。*/
    if (m && m->at_bottom && band == FAITH_SIGNAL_BAND_OFFLINE) band = FAITH_SIGNAL_BAND_HOLD;

    /* 擦弹是独立通道（一手 §3）⇒ 先按难度折成增量，再交给共用件（与 `signal_core_step()` 同一句）。*/
    if (graze > 0) gain = graze * signal_core_graze_gain(difficulty, 0);
    /* ★ 2026-09-16za：满 P 闸（106 声明）只作用在**「线上补满」那一支**上（用户选定口径）。*/
    d = faith_signal_step_gated(signal_now, band, gain, (m && m->no_line_topup) ? 0 : 1);

    /* ③ 红美铃：正增量 ×1.5 + 下限 = 满值一半。 */
    if (m && m->meiling_on) {
        if (d > 0) {
            /* ×1.5 = `d + ⌊d/2⌋`（= ⌊3d/2⌋，整数向下取整；括号写全，别靠结合律读）*/
            d = d + (d / SIGNAL_MEILING_GAIN_DEN) * (SIGNAL_MEILING_GAIN_NUM - SIGNAL_MEILING_GAIN_DEN);
            /* ★ 保住一手「线上未满 ⇒ **补满并钳到 `FAITH_SIGNAL_HI_MARK`(100.00)**」
             *   （`faith_signal_step()` 的 `signal_now < FAITH_SIGNAL_HI_MARK` 那一支）。
             *   ⇒ 两档表现（不是漏做、也不是全生效，见 .h）：`slot < 50.00` 时那句 `min(+50.00, …)`
             *     够不着 100.00 ⇒ ×1.5 真的更快（0 → 75.00）；`slot ≥ 50.00` 时它本来就"正好补满"
             *     ⇒ 放大后再被这里钳回去，观察不到。**不**为了「看得见」去掉那个钳（那是一手的）。*/
            if (signal_now < FAITH_SIGNAL_HI_MARK && d > FAITH_SIGNAL_HI_MARK - signal_now &&
                band != FAITH_SIGNAL_BAND_OFFLINE && band != FAITH_SIGNAL_BAND_HOLD) {
                d = FAITH_SIGNAL_HI_MARK - signal_now;
            }
        }
        /* ★★ 2026-09-16 修：**下限要写成「结果不低于 FLOOR」，不能写成「低于 FLOOR 时才抬回去」**
         *   —— 后者每帧一抬一落 ⇒ HUD 的条/数值**逐帧抖**（用户实跑：「你实现 >0.5 的方式很烂，
         *   导致它的 UI 在 1 帧 1 帧跳」）。旧写法带 `signal_now < FLOOR` 前置条件：
         *   槽**恰好等于** FLOOR 时那条不成立 ⇒ 这一帧照常扣（掉到 FLOOR−Δ）⇒ 下一帧又被抬回
         *   FLOOR ⇒ 在 FLOOR 与 FLOOR−Δ 之间**每帧振荡**。
         *   现在改成无条件钳「结果」：`now + d < FLOOR ⇒ d = FLOOR − now`
         *   ⇒ 恰好等于 FLOOR 时 `d = 0`（线上档本来也是 0）⇒ **稳定停在 FLOOR**，不再抖。*/
        if (signal_now + d < SIGNAL_MEILING_FLOOR) {
            d = SIGNAL_MEILING_FLOOR - signal_now;      /* 下限（用户口径「不会低于 ×0.5」）*/
        }
    }
    return d;
}

int32_t signal_core_apply_step_mod(faith_slot_t *s, int band, int32_t graze, int difficulty,
                                   const signal_step_mod_t *m)
{
    if (!s) return 0;
    /* `m == NULL` ⇒ 与 `signal_core_apply_step()` 逐值相同（`signal_core_step_mod()` 在
     * `m == NULL` 时就退化成 `signal_core_step()`）—— 单测 §17.4(a) 全域对拍。*/
    return signal_core_apply(s, signal_core_step_mod(s->signal, band, graze, difficulty, m));
}
