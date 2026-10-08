/* pofv_core.c —— 见 pofv_core.h。纯 C11，无平台依赖，无浮点返回值（`make dllx87`）。 */
#include <string.h>
#include <math.h>
#include "pofv_core.h"
#include "broken_core_core.h"   /* ★ bc_atan2f：纯 SSE 确定性 atan2 ✓（`sdk.h:577` 说的 pofv_atan2 已不存在 ✗）*/

static int32_t clamp_i32(int32_t v, int32_t lo, int32_t hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static float clamp_f(float v, float lo, float hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

void pofv_storm_pool_init(pofv_storm_pool_t *p)
{
    if (!p) return;
    memset(p, 0, sizeof *p);
}

int pofv_storm_spawn(pofv_storm_pool_t *p, float x, float y, int32_t life)
{
    int i;
    if (!p) return -1;
    for (i = 0; i < POFV_STORM_MAX; ++i) {
        pofv_storm_t *s = &p->s[i];
        if (s->active) continue;
        s->x = x;
        s->y = y;
        s->target = POFV_STORM_R_TARGET;
        s->tween = POFV_STORM_TWEEN_FRAMES;
        s->dmg = POFV_STORM_HIT_DMG;
        s->age = 0;
        s->life = clamp_i32(life, 1, 0x7fffffff);
        s->active = 1;
        ++p->live;
        return i;
    }
    ++p->dropped;
    return -1;                                  /* 池满：丢弃（日志在卡侧）*/
}

int pofv_storm_spawn_tier(pofv_storm_pool_t *p, float x, float y, float target, int32_t tween, int32_t dmg)
{
    int i = pofv_storm_spawn(p, x, y, tween);
    if (i >= 0) {
        p->s[i].target = target;
        p->s[i].tween = clamp_i32(tween, 1, 0x7fffffff);
        p->s[i].dmg = clamp_i32(dmg, 0, 0x7fffffff);
    }
    return i;
}

int pofv_storm_tick(pofv_storm_pool_t *p)
{
    int i;
    if (!p) return 0;
    for (i = 0; i < POFV_STORM_MAX; ++i) {
        pofv_storm_t *s = &p->s[i];
        if (!s->active) continue;
        ++s->age;
        if (s->age >= s->life) { s->active = 0; --p->live; }
    }
    return p->live;
}

const pofv_storm_t *pofv_storm_slot(const pofv_storm_pool_t *p, int i)
{
    if (!p || i < 0 || i >= POFV_STORM_MAX) return NULL;
    return &p->s[i];
}

/* ★ 一手 `FUN_004d5e10`：跳表 `0x4d66e0`（32 项），桩体见 `pofv_core.h` 的文件头。 */
void pofv_ease(uint32_t mode, float f, float *out)
{
    float t = clamp_f(f, 0.0f, 1.0f);
    if (!out) return;
    switch (mode) {
    case 1:  *out = t * t; break;                       /* f²            */
    case 2:  *out = t * t * t; break;                   /* f³            */
    case 4:  { float u = 1.0f - t; *out = 1.0f - u * u; } break;          /* 1 − (1−f)²   */
    case 5:  { float u = 1.0f - t; *out = 1.0f - u * u * u; } break;      /* 1 − (1−f)³ ★ 本卡（一手 mode 5）*/
    default: *out = t; break;                           /* 0/7/8/17 = 线性（其余未落地）*/
    }
}

void pofv_storm_target(float *out)
{
    if (!out) return;
    /* ❓ 自定：**直接给半径**。TH19 那边这个量 = `clamp(蓄力,0,200) × K/100`（光环的公式），
     * 本卡没有蓄力 ⇒ 不搬公式、只搬「律的形状」（见 pofv_core.h 的文件头）。 */
    *out = POFV_STORM_R_TARGET;
}

void pofv_storm_radius(const pofv_storm_t *st, float *out)
{
    float f, e, target;
    if (!out) return;
    if (!st || !st->active || st->life <= 0) { *out = 0.0f; return; }
    /* 一手：f = 计时 / 时长（钳 0..1）⇒ 走完时长后保持在目标 */
    target = st->target;
    f = (float)clamp_i32(st->age, 0, st->tween) / (float)st->tween;
    pofv_ease(POFV_STORM_EASE_MODE, f, &e);
    *out = POFV_STORM_R0 + (target - POFV_STORM_R0) * e;   /* ease(0)=f ⇒ 线性 */
}

/* ════════════════════════════════════════════════════════════ 固灵结界 */

void pofv_aura_spec(int32_t holding_c, pofv_aura_spec_t *o)
{
    if (!o) return;
    o->active         = holding_c ? 1 : 0;   /* 长按 C 展开（与蓄力同时）*/
    o->slow_enemies   = 1;                   /* ② */
    o->drop_power     = 1;                   /* ③ */
    o->pacify_all     = 1;                   /* ④ owner：把一手的 `-2` 做成通用 */
    o->th09_3shot     = 1;                   /* ❓ owner：自机狙最终会去掉 ⇒ 留一个开关，置 0 即关 */
}

/* ★★★ 2026-10-06 owner：「**照一手重做可视和判定**」✓ ⇒ 判定式逐条照一手 `06-aura.md` §7.3
 *   （「槽 7 的具体形状（逐类，一手逐条读出）」✓）取两套：
 *   · **灵梦**（`AuraReimuInf` 槽 7 = `0x416f10`）：`dx² + dy² < r² + pad²` ✓ —— 圆，
 *     且 **`pad` 以「平方和」方式并入** ✓（与下面 Sanae 那档的 `(r+pad)²` 线性并入不同 ✓）；
 *   · **魔理沙**（`AuraMarisaInf` 槽 7 = `0x417a10`）：`|px - cx| < r` ✓ —— **完全没有 y 项** ✓
 *     ⇒ **竖向无限带** ✓（与其视觉「两条 1×448 竖带」完全自洽 ✓，`06-aura.md` §9.3 ✓）。
 *   `pad` = 命中对象的**实体半径 / 2** ✓（一手 `FUN_00509030` 传 `EnemyInf+0x1350 / 2` ✓）。
 *   ⚠ 唯一偏差（登记 ✓）：一手两式都是**严格** `<`，这里写 `<=`（**含边界**）—— 与既有单测
 *     （`contains(10,0,10) == 1`）以及本卡「恰好触及边界也算命中」口径一致 ✓；差 1 px 级 ✓。*/
/* ★ 魔理沙竖带的 y 限幅 = 场地高的一半 ✓（448/2 ✓，与那两条 1×448 的带的视觉一致 ✓）*/

int pofv_aura_contains_ex(float dx, float dy, float r, float pad, int shape)
{
    if (r <= 0.0f) return 0;
    if (shape == POFV_AURA_SHAPE_BAND) {                 /* ★ 魔理沙：竖带 ✓ */
        /* ★★★ owner 2026-10-06：「魔理沙的固灵范围是 y ∈ (-∞,+∞)」＋「**得是，现在不是**」✓
         *   ⇒ 一手确证：`AuraMarisaInf` 槽 7 = `|px-(aura+0x18)| < r`，**没有 y 项** ✓
         *     （槽 3 把 `aura+0x1c` 强制置 0 ✓ ⇒ 竖直方向**无限** ✓）。
         *   ⚠ 我一度按「视觉只有 448 高」加了 y 限幅 ✗ ⇒ **已撤回** ✓（以 owner/一手为准 ✓）。*/
        return ((dx < 0.0f ? -dx : dx) <= r) ? 1 : 0;    /* 只判 x，y **不参与** ✓ */
    }
    return (dx * dx + dy * dy <= r * r + pad * pad) ? 1 : 0;   /* ★ 灵梦：圆 + pad² ✓ */
}

/* ★★★ 2026-10-06 接咲夜（owner：「归另一个 agent 是你不需要核，你只需要做」✓）——
 *   一手 `engine/versus/th09/02-ghost-and-seal.md:493`（pl02 咲夜）× TUNING #3 定案：
 *   形状 = **圆 × 扇形**：先判圆（半径 `+0x7c` ✓），再判「目标方向角 − 方向参考」折回后 `|Δ| <= 半张角` ✓；
 *   半张角 `+0x84`：**每帧 +3°（0.05235988）上限 90°** ✓；方向 `+0x80`：**朝自机 8 方向缓动 0.25/帧** ✓。
 *   ⚠ 卡与单测都**不链 libm**（`Makefile:272` 无 `-lm` ✗）⇒ 这里**不用三角函数**：
 *     令 `u` = 方向参考（任意长度 ✓）、`d` = 目标向量，则
 *       `|Δ角| <= half`  ⟺  `dot ≥ 0 且 dot² ≥ |d|² · cos²(half) · |u|²`   （两边同乘，**完全等价** ✓）
 *     ⇒ `cos²(half)` 查下面那张 **3° 步长表** ✓（一手半张角正是每帧 +3° ✓，步长与它对齐 ✓）。
 *   判定式按一手 §9.15：`半径 + pad` ✓。*/
static const float kCos2Half[31] = { 1.000000f, 0.997261f, 0.989074f, 0.975528f, 0.956773f, 0.933013f, 0.904508f, 0.871572f, 0.834565f, 0.793893f, 0.750000f, 0.703368f, 0.654508f, 0.603956f, 0.552264f, 0.500000f, 0.447736f, 0.396044f, 0.345492f, 0.296632f, 0.250000f, 0.206107f, 0.165435f, 0.128428f, 0.095492f, 0.066987f, 0.043227f, 0.024472f, 0.010926f, 0.002739f, 0.000000f };   /* half = 0,3,...,90° 的 cos² ✓ */

int pofv_aura_contains_sector(float dx, float dy, float r, float pad, float ux, float uy, int half_deg)
{
    float rr, d2, dot, c2, u2;
    int idx;
    if (r <= 0.0f) return 0;
    rr = (r + pad) * (r + pad);
    d2 = dx * dx + dy * dy;
    if (d2 > rr) return 0;                                   /* ★ 圆的部分 ✓ */
    idx = half_deg / 3; if (idx < 0) idx = 0; if (idx > 30) idx = 30;
    c2 = kCos2Half[idx];
    u2 = ux * ux + uy * uy;
    dot = dx * ux + dy * uy;
    if (dot < 0.0f) return 0;                                /* ★ 背向 ⇒ 角差 > 90° ✓ */
    return (dot * dot >= d2 * c2 * u2) ? 1 : 0;              /* ★ 扇形的部分 ✓ */
}

int pofv_aura_contains(float dx, float dy, float r)
{   /* 兼容旧调用：默认圆式（= 灵梦那套 ✓）*/
    return pofv_aura_contains_ex(dx, dy, r, 0.0f, POFV_AURA_SHAPE_CIRCLE);
}

/* ════════════════════════════════════════════════════════════ 对面侧的灵（纯数据模型，owner 定稿） */

void pofv_foe_spirit_init(pofv_foe_spirit_t *p, int32_t n)
{
    int32_t i;
    if (!p) return;
    for (i = 0; i < n; ++i) {
        p[i].x = 0.0f; p[i].y = 0.0f; p[i].vx = 0.0f; p[i].vy = 0.0f;
        p[i].age = 0; p[i].type = -1; p[i].level = 0;
        p[i].sealed = 0; p[i].sealed_timer = 0; p[i].active = 0; p[i].loop_exit = 0;
    }
}

/* ★★ 出循环（owner 裁定 ✓）：标记 + 查询 ✓ */
void pofv_foe_spirit_loop_exit(pofv_foe_spirit_t *p, int32_t n)
{
    int32_t i;
    if (!p) return;
    for (i = 0; i < n; ++i) if (p[i].active) p[i].loop_exit = 1;
}

int pofv_foe_spirit_loop_exit_any(const pofv_foe_spirit_t *p, int32_t n)
{
    int32_t i;
    if (!p) return 0;
    for (i = 0; i < n; ++i) if (p[i].active && p[i].loop_exit) return 1;
    return 0;
}

int pofv_foe_spirit_spawn(pofv_foe_spirit_t *p, int32_t n, float x, float y, int32_t type, int32_t level)
{
    int32_t i;
    if (!p || n <= 0) return -1;                /* ★ 先判空（我上一版把检查放在这之前 ⇒ 传 NULL 段错误 ✗ 已修 ✓）*/
    {   int i2;                                  /* ★ **对侧发 EX ⇒ 不返回来灵** ✓（owner 裁定 ✓）：出循环后不再产生灵 ✓ */
        for (i2 = 0; i2 < n; ++i2) if (p[i2].active && p[i2].loop_exit) return -1;
    }
    for (i = 0; i < n; ++i)
        if (!p[i].active) {
            p[i].x = x; p[i].y = y; p[i].type = type; p[i].level = level;
            p[i].age = 0; p[i].sealed = 0; p[i].sealed_timer = 0; p[i].active = 1;
            p[i].entry_ang = 0.0f; p[i].has_ang = 0;      /* ★ 幽灵出生角：默认未抽（见 set_ghost_angle ✓）*/
            pofv_spirit_entry_vel(type, 0, &p[i].vx, &p[i].vy);   /* 一手初速 ✓ */
            return (int)i;
        }
    return -1;
}

void pofv_foe_spirit_set_ghost_angle(pofv_foe_spirit_t *p, int32_t n, int32_t idx, float ang)
{
    if (!p || idx < 0 || idx >= n) return;
    p[idx].entry_ang = ang;
    p[idx].has_ang = 1;
    p[idx].vx = cosf(ang) * 1.0f;      /* ★ 立刻生效（下一帧 tick 还会再写一次 ✓）*/
    p[idx].vy = sinf(ang) * 1.0f;
}

static float foe_sealed_vy(int32_t type, int32_t el);   /* ★ 前向声明（定义在文件后半 ✓）*/
void pofv_foe_spirit_tick(pofv_foe_spirit_t *p, int32_t n)
{
    int32_t i;
    if (!p || n <= 0) return;
    for (i = 0; i < n; ++i) {
        float nvx, nvy;
        if (!p[i].active) continue;
        nvx = p[i].vx; nvy = p[i].vy;
        if (p[i].sealed) {
            nvx = 0.0f; nvy = foe_sealed_vy(p[i].type, POFV_FOE_SEAL_FRAMES - p[i].sealed_timer);   /* ★ 按族：幽灵上浮 / TH19 静止（owner 2026-10-06 ✓）*/
        } else if (p[i].has_ang) {
            /* ★★★ 2026-10-08（一手 §3.31 ✓）：**对称幽灵**照真幽灵同一条规则 ——
             *   角度在出生时抽一次（`pofv_ghost_spawn_angle` ✓）、速度恒 **1.0** ✓（≠ 纯灵 1.5 ✓）
             *   ⇒ 不再走 `entry_vel_at` 的「正下 1.0」匀速支 ✗。*/
            nvx = cosf(p[i].entry_ang) * 1.0f;
            nvy = sinf(p[i].entry_ang) * 1.0f;
        } else {
            pofv_spirit_entry_vel_at(p[i].type, p[i].age & 1, p[i].age, &nvx, &nvy);   /* 獺交替 / 鷲加速 ✓ */
        }
        p[i].vx = nvx; p[i].vy = nvy;
        p[i].x += p[i].vx; p[i].y += p[i].vy;
        /* ★★★ owner 2026-10-06（固化口径）：「**对于幽灵，固化会转变为活化灵，同时缓慢向上移动，
         *   移动到最高点消失**」✓ ⇒ 幽灵的**活化结束判据 = 位置**（升到顶边**之上**即移除 ✓），
         *   不是只靠时间（`sealed_timer` 仍在 ⇒ 兜底 ✓）。
         *   ❓「最高点」的一手精确值未核 ⇒ 现取 `y < 0`（越过场地顶边 ✓，一手场地 y ∈ [0,448] ✓）。
         *   ⚠ 这是**活化/离场**的移除（**不计击破、不折算、不发镜像事件** ✓），与出界释放占位不同 ✓。*/
        if (p[i].sealed && p[i].y < 0.0f) {
            p[i].active = 0; p[i].type = -1; p[i].sealed = 0; p[i].sealed_timer = 0;
            p[i].level = 0; p[i].vx = 0.0f; p[i].vy = 0.0f;   /* ★ 与 OOB 占位 / timer 到点两条移除路**对齐** ✓（核实建议；陈旧值本就读不到，纯卫生 ✓）*/
            continue;
        }
        /* ⚠⚠ **占位（owner 2026-10-05：「这个你留口，**现在还没到**」）** —— 按 MODEL §1-2/§6-3「出界即消失/界外即释放」先放一个
         *   **可撤销的占位实现**，**不作为定论** ❓：
         *   · 常量（x ∈ ±192、y ∈ [0,448]、余量 32）**都是我选的** ❓，一手**未核**（§6-3 本就登记为「❓ 自定」✓）；
         *   · 撤法：把这一个 `if (…释放…)` 块删掉即回到「不出界消失」的旧行为 ✓（一行范围）。
         *   owner 说「现在还没到」⇒ **不要**据此改别处、也不要把这当一手口径引用 ✗。 */
        if (p[i].y > 448.0f + 32.0f || p[i].x < -192.0f - 32.0f || p[i].x > 192.0f + 32.0f) {
            p[i].active = 0; p[i].type = -1; p[i].level = 0;
            p[i].sealed = 0; p[i].sealed_timer = 0; p[i].vx = 0.0f; p[i].vy = 0.0f;
            continue;
        }
        ++p[i].age;                                   /* ★ 计时器 = age，只递增 ✓ */
        /* ★★ 被固化后**多久消失**（owner 追加）：`sealed` 期间递减，到 0 就从模型里移除 ✓ */
        if (p[i].sealed) {
            if (--p[i].sealed_timer <= 0) {
                p[i].active = 0; p[i].type = -1; p[i].level = 0;
                p[i].sealed = 0; p[i].sealed_timer = 0;
                p[i].vx = 0.0f; p[i].vy = 0.0f;
            }
        }
    }
}



/* ★ 挑最近的那只（owner：「**选择规则不变**」✓ —— 我上一版误改成随机 ✗，已回退 ✓）；
 *   `rnd` 参数保留但**不参与选择** ✓（预留给以后别的用途 ✓）。*/
static int foe_nearest_killable(pofv_foe_spirit_t *p, int32_t n, float ref_x, float ref_y)
{
    int32_t i, best = -1; float bd = 0.0f;
    if (!p) return -1;
    for (i = 0; i < n; ++i) {
        float dx, dy, d;
        if (!p[i].active) continue;
        if (p[i].level >= 3) continue;            /* ★ level 3 永不返还 ✓ */
        if (p[i].sealed) continue;                /* ★ 被固化 ⇒ 永不回返 ✓ */
        if (p[i].loop_exit) continue;             /* ★ 出循环 ⇒ 永不回程 ✓ */
        dx = p[i].x - ref_x; dy = p[i].y - ref_y; d = dx * dx + dy * dy;
        if (best < 0 || d < bd) { best = i; bd = d; }
    }
    return best;
}

int pofv_foe_spirit_mirror_kill(pofv_foe_spirit_t *p, int32_t n, uint32_t rnd, float ref_x, float ref_y,
                                 int32_t *out_type, float *out_x, float *out_y)
{
    int32_t k2;
    (void)rnd;                                    /* ★ 选择规则不变（挑最近 ✓）⇒ rnd 不用 ✓ */
    if (!p || n <= 0) return 0;
    k2 = foe_nearest_killable(p, n, ref_x, ref_y);
    if (k2 < 0) return 0;
    if (out_type) *out_type = p[k2].type;
    if (out_x)    *out_x    = p[k2].x;
    if (out_y)    *out_y    = p[k2].y;
    p[k2].active = 0; p[k2].type = -1; p[k2].level = 0;
    p[k2].sealed = 0; p[k2].sealed_timer = 0; p[k2].loop_exit = 0;
    p[k2].vx = 0.0f; p[k2].vy = 0.0f;
    return 1;
}

/* 内部：挑最近的**未固化**那只 —— ★ owner 口径：「**固化不会固化已固化的，这个跳过**」✓
 *   ⚠ 只给**固化**路径用；**击破**路径不加这个过滤 ✓（owner 只说了固化这一条）*/
static int foe_nearest_unsealed(const pofv_foe_spirit_t *p, int32_t n, float rx, float ry)
{
    int32_t i, best = -1;
    float best_d = 0.0f;
    for (i = 0; i < n; ++i) {
        float dx, dy, d;
        if (!p[i].active || p[i].sealed) continue;      /* ★ 已固化 ⇒ 跳过（owner 口径）*/
        dx = p[i].x - rx; dy = p[i].y - ry; d = dx * dx + dy * dy;
        if (best < 0 || d < best_d) { best = i; best_d = d; }
    }
    return best;
}

/* ★★ 镜像**固化**事件：挑最近的**未固化**那只 ⇒ 置 `sealed` ✓（参考点同样 = **我方事件的 x,y** ✓）*/
int pofv_foe_spirit_seal(pofv_foe_spirit_t *p, int32_t n, float ref_x, float ref_y)
{
    int k;
    if (!p || n <= 0) return 0;
    k = foe_nearest_unsealed(p, n, ref_x, ref_y);
    if (k < 0) return 0;
    p[k].sealed = 1;
    p[k].sealed_timer = POFV_FOE_SEAL_FRAMES;          /* 一手：160 帧（wait(100)+wait(60)）✓ */
    p[k].vx = 0.0f; p[k].vy = foe_sealed_vy(p[k].type, 0);   /* ★ 按族（幽灵上浮 / TH19 静止）✓ */
    return 1;
}

int pofv_foe_spirit_count(const pofv_foe_spirit_t *p, int32_t n)
{
    int32_t i, c = 0;
    if (!p) return 0;
    for (i = 0; i < n; ++i) if (p[i].active) ++c;
    return (int)c;
}

/* ════════════════════════════════════════════════════════════ 延迟到达队列（本侧暴风送出的效果 ②） */

void pofv_foe_incoming_init(pofv_foe_incoming_t *q, int32_t n)
{
    int32_t i;
    if (!q) return;
    for (i = 0; i < n; ++i) { q[i].x = 0.0f; q[i].y = 0.0f; q[i].type = -1; q[i].level = 0; q[i].delay = 0; q[i].active = 0; }
}

int pofv_foe_incoming_push(pofv_foe_incoming_t *q, int32_t n, float x, float y,
                           int32_t type, int32_t level, int32_t delay)
{
    int32_t i;
    if (!q || n <= 0) return -1;
    for (i = 0; i < n; ++i)
        if (!q[i].active) {
            q[i].x = x; q[i].y = y; q[i].type = type; q[i].level = level;
            q[i].delay = (delay > 0) ? delay : 0;      /* 0 ⇒ 下一帧到达 ✓ */
            q[i].active = 1;
            return (int)i;
        }
    return -1;
}

int pofv_foe_incoming_tick(pofv_foe_incoming_t *q, int32_t n, int32_t *out_type, int32_t *out_level,
                           float *out_x, float *out_y)
{
    int32_t i;
    if (!q || n <= 0) return 0;
    for (i = 0; i < n; ++i) {
        if (!q[i].active) continue;
        if (q[i].delay > 0) { --q[i].delay; if (q[i].delay > 0) continue; }   /* 还没到点 */
        if (out_type)  *out_type  = q[i].type;
        if (out_level) *out_level = q[i].level;
        if (out_x)     *out_x     = q[i].x;
        if (out_y)     *out_y     = q[i].y;
        q[i].active = 0; q[i].type = -1; q[i].level = 0; q[i].delay = 0;
        return 1;
    }
    return 0;
}

int pofv_foe_incoming_count(const pofv_foe_incoming_t *q, int32_t n)
{
    int32_t i, c = 0;
    if (!q) return 0;
    for (i = 0; i < n; ++i) if (q[i].active) ++c;
    return (int)c;
}

/* ════════════════════════════════════════════════════════════ 連撃/破坏累积 + 阈值（owner 一手） */

void pofv_acc_init(pofv_acc_t *a) { if (a) { a->a50 = 0; a->a54 = 0; a->a2c = 0; a->a30 = 0; } }

int32_t pofv_acc_thr50(int32_t t) { return 150 - t / 2; }   /* 150 − t/2 ✓ */
int32_t pofv_acc_thr54(int32_t t) { return 120 - t / 5; }   /* 120 − t/5 ✓ */

void pofv_acc_add50(pofv_acc_t *a, int32_t n)
{
    if (!a || n <= 0) return;
    a->a50 += n;
    if (a->a50 > POFV_ACC50_MAX) a->a50 = POFV_ACC50_MAX;    /* 一手 clamp ✓ */
}
void pofv_acc_add54(pofv_acc_t *a, int32_t n)
{
    if (!a || n <= 0) return;
    a->a54 += n;
    if (a->a54 > POFV_ACC54_MAX) a->a54 = POFV_ACC54_MAX;
}

int pofv_acc_take50(pofv_acc_t *a, int32_t t)
{
    int32_t thr;
    if (!a) return 0;
    thr = pofv_acc_thr50(t);
    if (thr < 0) thr = 0;                                   /* ❓ 阈值趋负时按 0 处理（一手未明说）*/
    if (a->a50 < thr) return 0;
    a->a50 -= thr;                                          /* ★ 保余数（一手：不重置 ✓）*/
    return 1;
}
int pofv_acc_take54(pofv_acc_t *a, int32_t t)
{
    int32_t thr;
    if (!a) return 0;
    thr = pofv_acc_thr54(t);
    if (thr < 0) thr = 0;
    if (a->a54 < thr) return 0;
    a->a54 -= thr;
    return 1;
}

/* ════════════════════════════════════════════════════════════ TH09 的第二套表（**独立于 TH19** ✗不混用）

 *   ★ owner 2026-10-06：「**th09 和 th19 独立运行**，按照一手，并且按照**真实难度**」
 *     ⇒ 阈值吃**难度**（0=E/1=N/2=H/3=L/4=EX ✓）：`+0x30` = `60 − 難度×1.5`、`+0x2c` = `(30 − 難度)×4` ✓；
 *     ⇒ 步长 = 一手 `param_2`/`param_3`（`TUNING` 第 6 条：≈2–3）⇒ ❓ 精确值待核（现 3 / 2 ✓）。
 *   ❓ 一手的 clamp 未明说 ⇒ 这里**不设上限**（TH19 的 1400/700 是 TH19 专属 ✓），已登记 TUNING ✓。 */
int32_t pofv_acc_thr30(int32_t diff) { return 60 - diff * 3 / 2; }   /* 60 − 難度×1.5 ✓ */
int32_t pofv_acc_thr2c(int32_t diff) { return (30 - diff) * 4; }      /* (30 − 難度)×4 ✓ */

void pofv_acc_add30(pofv_acc_t *a, int32_t n) { if (a && n > 0) a->a30 += n; }
void pofv_acc_add2c(pofv_acc_t *a, int32_t n) { if (a && n > 0) a->a2c += n; }

int pofv_acc_take30(pofv_acc_t *a, int32_t diff)
{
    int32_t thr;
    if (!a) return 0;
    thr = pofv_acc_thr30(diff);
    if (thr < 0) thr = 0;                     /* ❓ 趋负按 0 处理（一手未明说）*/
    if (a->a30 < thr) return 0;
    a->a30 -= thr;                            /* ★ 保余数 ✓ */
    return 1;
}
int pofv_acc_take2c(pofv_acc_t *a, int32_t diff)
{
    int32_t thr;
    if (!a) return 0;
    thr = pofv_acc_thr2c(diff);
    if (thr < 0) thr = 0;
    if (a->a2c < thr) return 0;
    a->a2c -= thr;
    return 1;
}


/* ════════════════════════════════════════════════════════════ HUD「hits」（一手 §9.8 的取数/锁存/寿命/闪灰） */

void pofv_hits_init(pofv_hits_t *h)
{
    if (!h) return;
    h->cur = 0; h->latch = 0; h->life = 0; h->active = 1;
}

void pofv_hits_bump(pofv_hits_t *h, int32_t n)
{
    if (!h || n <= 0) return;
    h->cur += n;
}

void pofv_hits_tick(pofv_hits_t *h)
{
    if (!h) return;
    /* 一手：`1 < cur && latch < cur && 0 < active` ⇒ 锁存并重置寿命 120 ✓ */
    if (1 < h->cur && h->latch < h->cur && 0 < h->active) {
        h->latch = h->cur;
        h->life = POFV_HITS_LIFE;
    } else if (h->life > 0) {
        --h->life;                                  /* 一手：`+0x2290` 递减 ✓ */
    }
}

int pofv_hits_visible(const pofv_hits_t *h) { return (h && h->latch != 0) ? 1 : 0; }

int pofv_hits_is_gray(const pofv_hits_t *h)
{
    if (!h) return 0;
    if (h->active >= 1) return 0;                   /* 一手：只在 `+0x227c < 1` 时闪 ✓ */
    return ((h->life % 6) < 3) ? 1 : 0;             /* 一手：`+0x2290 % 6 < 3` ⇒ 灰 ✓ */
}

/* ════════════════════════════════════════════════════════════ SP → 送（一手：SP 块 +0x30 ≥ 60 − 難度×1.5 ✓） */

void pofv_sp_init(pofv_sp_t *s) { if (s) { s->sp = 0; s->diff2 = 0; } }
void pofv_sp_add(pofv_sp_t *s, int32_t n) { if (s && n > 0) s->sp += n; }

int32_t pofv_sp_threshold(const pofv_sp_t *s)
{
    if (!s) return 0;
    return 60 - (s->diff2 * 3) / 4;             /* 難度×1.5 = diff2×3/4 ✓（整数运算，避免浮点 ✓）*/
}

int pofv_sp_take(pofv_sp_t *s)
{
    int32_t thr;
    if (!s) return 0;
    thr = pofv_sp_threshold(s);
    if (thr < 0) thr = 0;
    if (s->sp < thr) return 0;
    s->sp -= thr - (s->diff2 - 60);             /* 一手：触发后 `SP += 難度×2 − 60` ⇒ 等价于 `SP -= thr − (diff2−60)` ✓ */
    if (s->sp < 0) s->sp = 0;
    return 1;
}

/* ════════════════════════════════════════════════════════════ W6 消费侧 */

void pofv_mirror_emit(int32_t mirror_event, pofv_mirror_emit_t *o)
{
    if (!o) return;
    o->from_top   = 1;                                  /* ★ ② 一律从版顶长出（上下对称）*/
    o->own_bullet = 1;                                  /* ★ ① 一律是己方弹幕 */
    o->alpha_start = 128;                               /* ★ ③ 默认 50% 透明起步（0x80）*/
    if (mirror_event == POFV_EV_STORM_SEND_L1) {
        o->alpha_start = 255;                           /* ❓ 暴风类**本来就是我们自己的样式** ⇒ 不降透明 */
    }
}

/* ════════════════════════════════════════════════════════════ EX（W7 的纯逻辑部分） */

int pofv_ex_threshold(int32_t diff)
{
    if (diff <= 0) return 149;               /* 一手：Easy 档 */
    if (diff == 1) return 200;               /* 一手：Normal 档 */
    return 300;                              /* 一手：Hard/Lunatic 档 */
}

int pofv_mirror_value(int32_t combo)
{
    int32_t v;
    if (combo < 0) combo = 0;
    v = combo * 10;
    return (v > 1000) ? 1000 : v;        /* 一手：到 0x65=101 之后恒 1000 */
}

void pofv_ex_spec(pofv_ex_spec_t *o)
{
    if (!o) return;
    o->rotate_half = 1;      /* ① 旋转 180° */
    o->from_top    = 1;      /* ② 从版顶长出 */
    o->own_damage  = 1;      /* ③ 己方伤害 */
    o->immune_own  = 1;      /* ④ 免疫己方系统 */
    o->alpha_start = 128;    /* ⑤ 50% 透明起步 */
}

int pofv_ex_damage(int32_t ex_points)
{
    if (ex_points <= 0) return 0;
    return ex_points * POFV_EX_DMG_SCALE;   /* ⑥ 伤害量纲独立（自己的刻度）*/
}

int pofv_ex_ready(int32_t count, int32_t diff)
{
    return (count > pofv_ex_threshold(diff)) ? 1 : 0;   /* 一手是「计数 > 149/200/300」*/
}

/* ════════════════════════════════════════════════════════════ 镜像记账（W6） */

void pofv_mirror_of(int32_t our_event, int32_t *mirror, int32_t *gate)
{
    int32_t m = 0, g = 0;
    switch (our_event) {
    case POFV_EV_STORM_SEND_L1:  m = POFV_EV_STORM_SEND_L1;  break;   /* 对面也送 level 1 */
    case POFV_EV_KILL_INCOMING:  m = POFV_EV_KILL_INCOMING;  g = POFV_MIRROR_GATE_LEVEL_UP; break;
    case POFV_EV_SEAL_INCOMING:  m = POFV_EV_SEAL_INCOMING;  break;   /* 映射不认族：固幽灵 ⇒ 对面那只是動物霊（一个可能性）*/
    case POFV_EV_LANDING:        m = POFV_EV_LANDING;        g = POFV_MIRROR_GATE_LANDING;  break;
    case POFV_EV_PURE_SPAWN:     m = POFV_EV_PURE_SPAWN;     break;
    case POFV_EV_KILL_PURE:      m = POFV_EV_KILL_PURE;      break;
    case POFV_EV_KILL_ANIMAL:    m = POFV_EV_KILL_ANIMAL;    break;
    case POFV_EV_DOMAIN_SEAL:    m = POFV_EV_DOMAIN_SEAL;    break;
    case POFV_EV_NATURAL_JOUMON: m = POFV_EV_NATURAL_JOUMON; break;
    case POFV_EV_EX_OUT:         m = 0;                      break;   /* ★ 对面**不回** EX */
    default:                     m = 0;                      break;
    }
    if (mirror) *mirror = m;
    if (gate)   *gate   = g;
}

void pofv_mirror_level_gain(int32_t level, int32_t *a4, int32_t *a6)
{
    int32_t a = level, b = level + 1;
    if (level < 1) { a = 1; b = 2; }        /* ❓ 越界兜底：level 至少 1 */
    if (a > 2) a = 2;                        /* 一手：`+0xa4 = min(level,2)`   */
    if (b > 3) b = 3;                        /* 一手：`+0xa6 = min(level+1,3)` */
    if (a4) *a4 = a;
    if (a6) *a6 = b;
}

/* ════════════════════════════════════════════════════════════ 灵（W1/W2） */

int pofv_spirit_cap(int32_t round_seconds)
{
    if (round_seconds <= 60)  return 60;      /* 一手：60 s 档 */
    if (round_seconds <= 120) return 80;      /* 一手：120 s 档 */
    return 999;                               /* 一手：180 s 档 */
}

int pofv_spirit_cap_for(int32_t family, int32_t round_seconds)
{
    /* 存储/流程不分族，**只有这里分**：TH09 幽灵一套（❓ 一手上限未定音，取 owner 实跑的 5），TH19 一套（60/80/999）。 */
    if (family == POFV_FAM_TH09) return POFV_TH09_GHOST_CAP;
    return pofv_spirit_cap(round_seconds);
}

int pofv_spirit_room(int32_t live, int32_t cap)
{
    return (cap > 0 && live < cap) ? 1 : 0;
}

void pofv_spirit_entry_motion(int32_t kind, int lr, float *dir, float *spd)
{
    /* 一手 `ins_404` 的实参（难度项已取 0）。獺那档一手给的是**两个方向**（π/8 与 7π/8 ⇒ L/R 两只），
     * 用 `lr` 选（0 = π/8 左、1 = 7π/8 右）。 */
    float d = 1.5707964f, s = 1.5f;                  /* 默认 = 純霊：π/2 @1.5 */
    switch (kind) {
    case POFV_SPIRIT_KIND_WOLF:       d = 1.5707964f;  s = 1.1f; break;   /* 狼  */
    case POFV_SPIRIT_KIND_OTTER:      d = lr ? 2.7488935f : 0.3926991f; s = 1.5f; break;  /* 獺 L/R */
    case POFV_SPIRIT_KIND_EAGLE:      d = 1.5707964f;  s = 3.5f; break;   /* 鷲（一手 60 帧加速到 3.5；本卡先用终速 ❓）*/
    case POFV_SPIRIT_KIND_GHOST_TH09: d = 1.5707964f;  s = 1.0f; break;   /* TH09 幽灵（绿档）*/
    default:                          d = 1.5707964f;  s = 1.5f; break;   /* 純霊 */
    }
    if (dir) *dir = d;
    if (spd) *spd = s;
}

void pofv_spirit_entry_vel(int32_t kind, int lr, float *vx, float *vy)
{
    /* cos/sin 预计算：π/2 ⇒ (0,1)；π/8 ⇒ (0.9238795, 0.3826834)；7π/8 ⇒ (−0.9238795, 0.3826834) */
    float x = 0.0f, y = 1.5f;                        /* 默认純霊 */
    switch (kind) {
    case POFV_SPIRIT_KIND_WOLF:       x = 0.0f;                       y = 1.1f; break;
    case POFV_SPIRIT_KIND_OTTER:      x = lr ? -1.3858193f : 1.3858193f; y = 0.5740251f; break;
    case POFV_SPIRIT_KIND_EAGLE:      x = 0.0f;                       y = 3.5f; break;
    case POFV_SPIRIT_KIND_GHOST_TH09: x = 0.0f;                       y = 1.0f; break;
    default:                          x = 0.0f;                       y = 1.5f; break;
    }
    if (vx) *vx = x;
    if (vy) *vy = y;
}

void pofv_ghost_spawn_angle(float sx, float sy, float tx, float ty,
                            uint32_t r1, uint32_t r2, float *out_angle)
{
    /* ⚠ **出参而不是返回值**：本 DLL 的 `dllx87` 铁律 —— 导出函数**返回 float 会走 x87
     *   （`flds` ∈ st(0)）** ✗（实测本函数原先就吃了 2 条 ✗）。仓库同族助手（`pofv_spirit_entry_vel`
     *   / `pofv_storm_radius` …）一律是 `void` + 出参 ✓ ⇒ 照它们写 ✓。*/
    const float PI      = 3.14159265f;
    const float HALF_PI = 1.5707964f;
    float j;                                  /* ∈ [−1, 1] 均匀（❓一手分布未逐字核 ✓）*/
    if (!out_angle) return;
    j = ((float)(r2 % 2001u) / 1000.0f) - 1.0f;
    if (r1 % 5u == 0u) {                      /* ★ 抽中「瞄准」支（一手 u == 0 ✓）*/
        float dx = tx - sx, dy = ty - sy;     /* ★ 角度约定：0 = +x、π/2 = +y（正下方）✓ */
        *out_angle = bc_atan2f(dy, dx) + j * (PI / 8.0f);          /* 朝自机 ± 22.5° ✓ */
        return;
    }
    *out_angle = HALF_PI + j * (PI / 200.0f);                      /* 正下方 ± 0.9° ✓ */
}

void pofv_spirit_entry_vel_at(int32_t kind, int lr, int32_t age, float *vx, float *vy)
{
    float k = 1.0f;
    if (age < 0) age = 0;
    if (kind == POFV_SPIRIT_KIND_EAGLE) {            /* 一手 ins_405(60, 0, π/2, 3.5) */
        k = (age >= 60) ? 1.0f : (float)age / 60.0f;
    } else if (kind == POFV_SPIRIT_KIND_PURE || kind == POFV_SPIRIT_KIND_WOLF ||
               kind == POFV_SPIRIT_KIND_OTTER || kind == POFV_SPIRIT_KIND_GHOST_TH09) {
        /* 純霊一手有 ins_405(30,0,π/2,1.5)；狼/獺/幽灵一手没给加速 ⇒ 匀速（k = 1）。 */
        if (kind == POFV_SPIRIT_KIND_PURE) k = (age >= 30) ? 1.0f : (float)age / 30.0f;
    }
    pofv_spirit_entry_vel(kind, lr, vx, vy);
    if (vx) *vx *= k;
    if (vy) *vy *= k;
}

int pofv_spirit_family_of(int32_t kind)
{
    return (kind == POFV_SPIRIT_KIND_GHOST_TH09) ? POFV_FAM_TH09 : POFV_FAM_TH19;
}

/* ★★★ owner 2026-10-06（固化口径）：「对于**幽灵**，固化会转变为活化灵，同时**缓慢向上移动**；
 *   而对于 **th19 的灵，固化会不动**」✓ ⇒ 固化期的竖直速度**按族**：TH09 = 缓慢上浮 ✓ / TH19 = **0（原地不动）** ✓。
 *   （一手 TH19 §10.2 其实是「0.02 上浮 + 末段加速」，但 **owner 原话优先** ⇒ 取 0 ✓。）*/
static float foe_sealed_vy(int32_t type, int32_t el)
{
    /* ★★★ 2026-10-06 按 owner 转述的核实（`engine/versus/th09/02-ghost-and-seal.md` §3.8 ✓）逐族照抄：
     *   · **TH09（幽灵）**：`vx = 随机 ±0.1`；`vy`：前 **120 帧**从 0 **线性爬到 −1.0**，之后恒 **−1.0** ✓；
     *     （`0x48e7f4 = −1.0`、`0x4915a4 = −1/120`、`push 0.1f` ✓）
     *   · **TH19（狼/獭/鹫/純霊）**：ECL `GhostDisableMove()` ✓ =
     *     `ins_404(-1.5707964f, 0.02f)` ＋ `ins_23(100)` ⇒ `ins_405(200, 3, -1.5707964f, 2.0f)` ＋ `ins_23(200)` ✓
     *     ⇒ 前 **100 帧恒 −0.02**（「几乎停住」✓），随后 **200 帧线性加速到 −2.0**，之后恒 **−2.0** ✓。*/
    if (pofv_spirit_family_of(type) == POFV_FAM_TH09) {
        if (el <= 0) el = 1;                       /* ★ 首帧：倒计时还没递减 ⇒ 按"已过 1 帧"算 ✓（否则第一帧 vy=0 ✗）*/
        if (el >= 120) return -1.0f;
        return -1.0f * (float)el / 120.0f;
    }
    if (el <= 100) return -0.02f;
    /* ★ owner 实跑订正（2026-10-06）：一手 `2.0` 直用会 200 帧冲 400 px ✗（"升到版顶" ✓）
     *   ⇒ 实机只是"微微上升" ✓ ⇒ 取 **0.2**（200 帧 ≈ 40 px ✓）；与本侧 ECL 同步 ✓。❓ 一手量纲待核。*/
    if (el <= 300) { const float f = (float)(el - 100) / 200.0f; return -0.02f + (-0.2f + 0.02f) * f; }
    return -0.2f;
}

int pofv_spirit_kind_from_dist2(float d2)
{
    if (d2 < 0.0f) return POFV_SPIRIT_KIND_WOLF;          /* 取不到自机坐标时按最近档 */
    if (d2 <= POFV_SPIRIT_D2_WOLF)  return POFV_SPIRIT_KIND_WOLF;
    if (d2 <= POFV_SPIRIT_D2_OTTER) return POFV_SPIRIT_KIND_OTTER;
    return POFV_SPIRIT_KIND_EAGLE;
}


void pofv_spirit_pool_init(pofv_spirit_pool_t *p)
{
    if (!p) return;
    memset(p, 0, sizeof *p);
}

int pofv_spirit_spawn(pofv_spirit_pool_t *p, float x, float y, int32_t kind, int32_t life)
{
    int i;
    if (!p) return -1;
    for (i = 0; i < POFV_SPIRIT_POOL_MAX; ++i) {
        pofv_spirit_t *s = &p->s[i];
        if (s->active) continue;
        s->x = x; s->y = y; s->kind = kind; s->level = 1;
        s->age = 0; s->life = clamp_i32(life, 1, 0x7fffffff); s->active = 1;
        ++p->live;
        return i;
    }
    ++p->dropped;
    return -1;
}

int pofv_spirit_tick(pofv_spirit_pool_t *p)
{
    int i, formed = -1;
    if (!p) return -1;
    for (i = 0; i < POFV_SPIRIT_POOL_MAX; ++i) {
        pofv_spirit_t *s = &p->s[i];
        if (!s->active) continue;
        ++s->age;
        if (s->age >= s->life) {                 /* 自然成仏（一手：TH19 的自然成仏 ⇒ 变動物霊送出）*/
            s->active = 0;
            --p->live;
            if (formed < 0) formed = i;          /* 本帧只报第一只（卡侧按槽号取位置）*/
        }
    }
    return formed;
}

const pofv_spirit_t *pofv_spirit_slot(const pofv_spirit_pool_t *p, int i)
{
    if (!p || i < 0 || i >= POFV_SPIRIT_POOL_MAX) return NULL;
    return &p->s[i];
}

/* ============================================================ 可消弹的判据（TH18 侧）*/

int pofv_is_white_bullet(float r)
{
    return (r > 0.0f && r <= POFV_WHITE_R_MAX) ? 1 : 0;
}

int pofv_scan_count_white(const float *radii, int n)
{
    int i, c = 0;
    if (!radii || n <= 0) return 0;
    for (i = 0; i < n; ++i) if (pofv_is_white_bullet(radii[i])) ++c;
    return c;
}
