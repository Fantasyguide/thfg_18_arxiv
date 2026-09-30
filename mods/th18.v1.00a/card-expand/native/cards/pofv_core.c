/* pofv_core.c —— 花映塚（id 109）纯逻辑的实现。语义与数值纪律见 pofv_core.h 的头注释。
 * 纯 C11，无引擎地址、无 libm、无 float 返回值（i386 x87 不变式）。 */
#include "pofv_core.h"
#include <string.h>
#include <stddef.h>          /* NULL（`pofv_storm_slot` 的空槽判定）*/

/* ---------------------------------------------------------------- A. 白弹阈值 */

pofv_bullet_class_t pofv_classify_bullet_radius(float r)
{
    if (!(r > POFV_R_SENTINEL)) return POFV_BULLET_SENTINEL;   /* ≤ 0（含 NaN：NaN 的比较恒假 ⇒ 走这里）*/
    if (r <= POFV_WHITE_R_MAX)  return POFV_BULLET_WHITE;      /* ★ 边界含 4.0（一手：半径 ≤ 4.0）*/
    return POFV_BULLET_UNBREAKABLE;
}

int pofv_is_white_bullet(float r)
{
    return pofv_classify_bullet_radius(r) == POFV_BULLET_WHITE;
}

int pofv_scan_count_white(const float *radii, int n)
{
    int i, c = 0;
    if (!radii || n <= 0) return 0;
    for (i = 0; i < n; ++i)
        if (pofv_is_white_bullet(radii[i])) ++c;
    return c;
}

/* ------------------------------------------------------- B. 气槽 / 蓄力 / CA */

static int32_t clamp_i32(int32_t v, int32_t lo, int32_t hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

void pofv_state_init(pofv_state_t *st)
{
    if (!st) return;
    memset(st, 0, sizeof *st);
    st->phase = POFV_CHARGE_IDLE;
    st->last_kill_frame = -1;
}

int pofv_ca_cost(int level)
{
    if (level < POFV_CA_LEVEL_MIN || level > POFV_CA_LEVEL_MAX) return 0;
    return level - 1;                       /* 一手：C1 0 / C2 1 / C3 2 / C4 3 */
}

int pofv_ca_available(const pofv_state_t *st, int level)
{
    if (!st) return 0;
    if (level < POFV_CA_LEVEL_MIN || level > POFV_CA_LEVEL_MAX) return 0;
    return st->gauge >= level;              /* 一手门槛：有 L 格气才发得动 L 级 */
}

int pofv_charge_max_level(int32_t gauge)
{
    return clamp_i32(gauge, 0, POFV_GAUGE_MAX);
}

int pofv_charge_begin(pofv_state_t *st)
{
    if (!st) return 0;
    if (st->phase == POFV_CHARGE_HOLD) return 0;
    st->phase = POFV_CHARGE_HOLD;
    st->hold_frames = 0;
    st->level = 0;
    st->max_level = pofv_charge_max_level(st->gauge);
    return 1;
}

int pofv_charge_hold(pofv_state_t *st)
{
    int32_t lv;
    if (!st || st->phase != POFV_CHARGE_HOLD) return 0;
    ++st->hold_frames;
    st->max_level = pofv_charge_max_level(st->gauge);       /* b：可用上限（气槽）*/
    /* ★★ 2026-09-22（P5 #3「没有任何蓄力机制」）：**蓄力量 a 只按时间涨到 4 档**，
     *   不再被气槽夹（一手：原版每帧 `charge += k`，只在**结算/钳上界**那一步比气槽 —
     *   `th09:0x41fab0 fcomp [player+0x30388]` / `0x41fabd` 钳；能不能放出去看 `pofv_charge_usable_level`）。
     *   旧实现 `if (lv > max_level) lv = max_level;` 在气槽空时把 a 永远钉在 0 ⇒ 用户看到「没有任何蓄力机制」。*/
    lv = st->hold_frames / POFV_CHARGE_FRAMES_PER_LEVEL;
    if (lv > POFV_CA_LEVEL_MAX) lv = POFV_CA_LEVEL_MAX;
    if (lv > st->level) { st->level = lv; return 1; }       /* 升级：卡侧播「叮」*/
    return 0;
}

int pofv_charge_usable_level(const pofv_state_t *st)
{
    int32_t lv;
    if (!st) return 0;
    /* 一手：原版蓄力量被气槽**钳上界**（`th09:0x41fab0` / `0x41fabd`）⇒
     * 能放出的档 = min(蓄力量档 a, 气槽档 b)。 */
    lv = st->level < st->max_level ? st->level : st->max_level;
    if (lv < 0) lv = 0;
    if (lv > POFV_CA_LEVEL_MAX) lv = POFV_CA_LEVEL_MAX;
    return (int)lv;
}

int pofv_charge_release(pofv_state_t *st, int *out_level)
{
    int32_t lv;
    if (!st || st->phase != POFV_CHARGE_HOLD) return 0;
    st->max_level = pofv_charge_max_level(st->gauge);
    lv = pofv_charge_usable_level(st);        /* ★ 可用档 = min(a, b)（一手：蓄力量被气槽钳上界）*/
    st->phase = POFV_CHARGE_IDLE;
    st->hold_frames = 0;
    st->level = 0;
    if (lv < POFV_CA_LEVEL_MIN || !pofv_ca_available(st, lv)) {
        /* 可用档 0（点射）或气不够：什么都不发动 —— 交回原生弹 */
        st->max_level = pofv_charge_max_level(st->gauge);
        if (out_level) *out_level = 0;
        return 0;
    }
    st->gauge -= pofv_ca_cost(lv);
    st->max_level = pofv_charge_max_level(st->gauge);
    if (out_level) *out_level = lv;
    return 1;
}

int pofv_charge_cancel(pofv_state_t *st)
{
    int was;
    if (!st) return 0;
    was = (st->phase == POFV_CHARGE_HOLD);
    st->phase = POFV_CHARGE_IDLE;
    st->hold_frames = 0;
    st->level = 0;
    st->max_level = pofv_charge_max_level(st->gauge);
    return was;
}

void pofv_gauge_add_points(pofv_state_t *st, int32_t pts)
{
    if (!st || pts <= 0) return;
    if (st->gauge >= POFV_GAUGE_MAX) { st->gauge = POFV_GAUGE_MAX; st->gauge_pts = 0; return; }
    st->gauge_pts += pts;
    while (st->gauge_pts >= POFV_GAUGE_PTS_PER_LEVEL && st->gauge < POFV_GAUGE_MAX) {
        st->gauge_pts -= POFV_GAUGE_PTS_PER_LEVEL;
        ++st->gauge;
    }
    if (st->gauge >= POFV_GAUGE_MAX) st->gauge_pts = 0;     /* 满槽：余点丢弃（不攒着下一轮爆）*/
}

/* ★ 一手：C1 = 8 帧无敌；C2–C4「无敌越长」⇒ ❓ 自定占位（TODO(th09) 原版帧数未逆到）。 */
static const int32_t s_ca_invuln[POFV_CA_LEVEL_MAX] = { 8, 16, 24, 32 };

int pofv_ca_invuln_frames(int level)
{
    if (level < POFV_CA_LEVEL_MIN || level > POFV_CA_LEVEL_MAX) return 0;
    return s_ca_invuln[level - 1];
}

/* ---- CA 无敌窗口的声明层推进（契约与 off-by-one 的由来见 pofv_core.h）---- */
void pofv_invuln_init(pofv_invuln_t *w)
{
    if (w) w->left = 0;
}

int32_t pofv_invuln_grant(pofv_invuln_t *w, int level)
{
    int32_t frames = pofv_ca_invuln_frames(level);

    if (!w) return 0;
    w->left = frames;         /* 起窗帧算窗口的**第 1 帧**（调用方立即声明 frames）*/
    return frames;
}

int32_t pofv_invuln_tick(pofv_invuln_t *w)
{
    if (!w || w->left <= 0) return 0;
    /* ★ 先递减、再声明：起窗帧占了第 1 帧 ⇒ 本帧（第 2 帧起）声明的剩余量是 N−1 … 1，
     *   第 N+1 帧 `left` 已经到 0 ⇒ 返回 0（退声明）。
     * ⚠ 旧实现是「先声明 `left`、再 `--left`」⇒ 起窗帧之外又声明了 N 帧（共 N+1 帧）。*/
    --w->left;
    return w->left;
}

int32_t pofv_invuln_left(const pofv_invuln_t *w)
{
    return w ? w->left : 0;
}

/* ------------------------------------------------- C. 暴风 / 红圈 / SP / 记账 */

/* ★★ 2026-09-22（P5 #4 机制重做）：暴风 = **锚定场**（圆心不动、半径逐帧长、活有限帧）。
 * 旧实现是「`pofv_chain_register_kill()` 的 30 帧窗口给档 + 队列入队后一次性消弹 2 帧」——
 * 前者让档位靠**节拍**往上蹿（用户：「连锁似乎不该这么快」），后者让场只存在 2 帧
 * （妖精根本来不及飞进来）。一手（TH09）两件事都不是这样：场是节点，见 pofv_core.h §C 的表。 */

/* 连爆档 chain 的**终点半径** R。
 * ★ 档 1 = **176 px**（一手：TH09 的「CA 释放」那个消弹节点 r₀=16、dr=20、8 帧
 *   ⇒ 16 + 20×8 = 176，`th09:0x41e875`–`0x41e88c`；同一族的 CA L1 终点是 192）。
 * ⚠ 档 2..4 每档 +48 是 **❓ 自定**（TODO(th09)：原版「暴风随连锁变大」的表没逆到）——
 *   旧值是 64/80/96/120（比 TH09 **最小的**那个圈还小一半），这就是用户报的「暴风缺乏范围」。 */
static const float s_storm_r[POFV_BLAST_CHAIN_MAX] = { 176.0f, 224.0f, 272.0f, 320.0f };

void pofv_blast_radius(int chain, float *out)
{
    int32_t c;
    if (!out) return;
    c = clamp_i32(chain, 1, POFV_BLAST_CHAIN_MAX);
    *out = s_storm_r[c - 1];
}

void pofv_storm_pool_init(pofv_storm_pool_t *p)
{
    if (!p) return;
    memset(p, 0, sizeof *p);
}

int pofv_storm_spawn(pofv_storm_pool_t *p, float x, float y, int32_t chain)
{
    int i;
    if (!p) return -1;
    for (i = 0; i < POFV_STORM_MAX; ++i) {
        pofv_storm_t *s = &p->s[i];
        if (s->active) continue;
        s->x = x;
        s->y = y;
        s->chain = clamp_i32(chain, 1, POFV_BLAST_CHAIN_MAX);
        s->age = 0;
        s->life = POFV_STORM_FRAMES;
        s->active = 1;
        ++p->live;
        return i;
    }
    ++p->dropped;
    return -1;                                  /* 池满：丢弃（日志在卡侧）*/
}

/* 当前半径：与 TH09 画环的 `posTime` 同律（起点 16、终点 R、线性、时长 = 寿命）。
 * ⚠ 全程 float 运算、走出参 —— 不许引入 x87 返回值（`make dllx87` 的零 x87 不变式）。*/
void pofv_storm_radius(const pofv_storm_t *st, float *out)
{
    float r0 = POFV_STORM_R0, r1, t;
    int32_t life, age;

    if (!out) return;
    if (!st || !st->active || st->life <= 0) { *out = 0.0f; return; }
    pofv_storm_final_radius(st->chain, &r1);
    if (r1 < r0) r1 = r0;
    life = st->life;
    age = st->age;
    if (age < 0) age = 0;
    if (age > life) age = life;
    t = (float)age / (float)life;
    *out = r0 + (r1 - r0) * t;
}

void pofv_storm_final_radius(int32_t chain, float *out)
{
    pofv_blast_radius(chain, out);
}

int pofv_storm_chain_at(const pofv_storm_pool_t *p, float x, float y)
{
    int i, best = 0;
    if (!p) return 0;
    for (i = 0; i < POFV_STORM_MAX; ++i) {
        const pofv_storm_t *s = &p->s[i];
        float r, dx, dy;
        if (!s->active) continue;
        pofv_storm_radius(s, &r);
        dx = x - s->x;
        dy = y - s->y;
        if (dx * dx + dy * dy > r * r) continue;   /* 没盖住这一点 */
        if (s->chain > best) best = s->chain;
    }
    return best;
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

/* ❓ 自定占位（TODO(th09)：原版 CA 的半径**已经逆到一手**了，见下 —— 这里只留红圈的
 * 「C4 = 近乎全屏」这一条语义）。★ 2026-09-22：改成**一手终点半径**：
 *   L1 = 192（r₀ 0 + dr 4.0 × 48 帧，`th09:0x403c30`）、L2 = 256（4.0 × 64，`th09:0x403c90`）、
 *   L3 = 448（3.5 × 128，`th09:0x403cf0`）；L4 在 TH09 里是 **boss attack**（`th09:0x41f984` 的
 *   ≥400 分支），本卡单人化后按用户口径保留成「近乎全屏」⇒ 480（❓ 自定，游戏区半宽 ≈ 208–240）。 */
static const float s_ca_r[POFV_CA_LEVEL_MAX] = { 192.0f, 256.0f, 448.0f, 480.0f };

void pofv_ca_circle_radius(int level, float *out)
{
    int32_t l;
    if (!out) return;
    l = clamp_i32(level, POFV_CA_LEVEL_MIN, POFV_CA_LEVEL_MAX);
    *out = s_ca_r[l - 1];
}

/* ★ 一手阈值：10 万 / 30 万 / 50 万（IDEAS 109「连击 / SP」）。 */
static const int32_t s_sp_threshold[POFV_SP_STAGE_MAX] = { 100000, 300000, 500000 };

int32_t pofv_sp_threshold(int stage)
{
    if (stage < 0 || stage >= POFV_SP_STAGE_MAX) return 0;
    return s_sp_threshold[stage];
}

int pofv_sp_attack_stage(int32_t sp)
{
    int i, n = 0;
    for (i = 0; i < POFV_SP_STAGE_MAX; ++i)
        if (sp >= s_sp_threshold[i]) ++n;
    return n;
}

int pofv_sp_take_attack(pofv_state_t *st)
{
    int stage;
    if (!st) return 0;
    stage = pofv_sp_attack_stage(st->sp);
    if (stage > st->sp_stage) {                 /* 跨档只报一次（跨两档也算一次）*/
        st->sp_stage = stage;
        ++st->attacks;
        return 1;
    }
    return 0;
}

void pofv_account_blast(pofv_state_t *st, int32_t n_white, int32_t chain)
{
    int32_t c;
    if (!st || n_white <= 0) return;
    c = clamp_i32(chain, 1, POFV_BLAST_CHAIN_MAX);
    st->white_cancelled += n_white;
    st->any_cancelled += n_white;
    pofv_gauge_add_points(st, n_white * POFV_GAUGE_PTS_PER_WHITE_CANCEL * c);  /* 按连爆质量回气 */
    st->sp += n_white * POFV_SP_PER_WHITE_CANCEL;
}

void pofv_account_ca(pofv_state_t *st, int32_t n_any)
{
    if (!st || n_any <= 0) return;
    st->any_cancelled += n_any;
    /* ★ 一手：红圈消掉的白弹**不回复气** ⇒ 这里刻意不碰 gauge / gauge_pts。 */
    st->sp += n_any * POFV_SP_PER_RED_CANCEL;
}

void pofv_account_kill(pofv_state_t *st)
{
    if (!st) return;
    ++st->kills;
    pofv_gauge_add_points(st, POFV_GAUGE_PTS_PER_KILL);
    st->sp += POFV_SP_PER_KILL;
}

/* ---- 暴风池的实现见上面 §C 开头（`pofv_storm_*`）；旧的爆风队列已删除（场模型不需要它）---- */

/* ------------------------------------------------------------- D. 活化灵 */

void pofv_spirit_pool_init(pofv_spirit_pool_t *p)
{
    if (!p) return;
    memset(p, 0, sizeof *p);
}

int pofv_spirit_spawn(pofv_spirit_pool_t *p, float x, float y, int32_t src_enemy_id)
{
    int i;
    if (!p) return -1;
    for (i = 0; i < POFV_SPIRIT_MAX; ++i) {
        pofv_spirit_t *s = &p->s[i];
        if (s->active) continue;
        memset(s, 0, sizeof *s);
        s->active = 1;
        s->x = x;
        s->y = y;
        s->hp = POFV_SPIRIT_HP_TOUGH;          /* 一手：普通幽灵很硬（只有连爆波及才爆）*/
        s->selfdestruct_in = -1;
        s->src_enemy_id = src_enemy_id;
        ++p->live;
        return i;
    }
    return -1;                                 /* 池满：丢弃（日志在卡侧）*/
}

int pofv_spirit_activate_in_domain(pofv_spirit_pool_t *p, float px, float py, int32_t *out_n)
{
    int i, n = 0;
    const float r2 = POFV_DOMAIN_R * POFV_DOMAIN_R;
    if (out_n) *out_n = 0;
    if (!p) return 0;
    for (i = 0; i < POFV_SPIRIT_MAX; ++i) {
        pofv_spirit_t *s = &p->s[i];
        float dx, dy;
        if (!s->active || s->activated) continue;
        dx = s->x - px;
        dy = s->y - py;
        if (dx * dx + dy * dy > r2) continue;
        s->activated = 1;
        s->hp = POFV_SPIRIT_HP_ACTIVATED;      /* ★ 一手语义：HP **减半** */
        s->selfdestruct_in = POFV_SPIRIT_SELFDESTRUCT_FRAMES;
        ++n;
    }
    if (out_n) *out_n = n;
    return n;
}

int pofv_spirit_tick(pofv_spirit_pool_t *p, int32_t *out_slots, int cap)
{
    int i, n = 0;
    if (!p) return 0;
    for (i = 0; i < POFV_SPIRIT_MAX; ++i) {
        pofv_spirit_t *s = &p->s[i];
        if (!s->active) continue;
        ++s->age;
        if (!s->activated) continue;
        s->y += POFV_SPIRIT_RISE_PER_FRAME;    /* 一手：活化灵**缓慢上浮** */
        if (s->selfdestruct_in > 0) --s->selfdestruct_in;
        if (s->selfdestruct_in != 0) continue;
        if (n >= cap) continue;                /* 本帧名额用完：**留在槽里**，下一帧再爆（不漏、不丢）*/
        out_slots[n] = i;
        ++n;
        s->active = 0;                          /* 自爆：槽当场释放（3 颗小玉由调用方做）*/
        --p->live;
    }
    return n;
}

int pofv_spirit_blast_hit(pofv_spirit_pool_t *p, float x, float y, float r, int32_t dmg,
                          int32_t *out_slots, int cap)
{
    int i, n = 0;
    if (!p || r <= 0.0f) return 0;
    for (i = 0; i < POFV_SPIRIT_MAX; ++i) {
        pofv_spirit_t *s = &p->s[i];
        float dx, dy;
        if (!s->active) continue;
        dx = s->x - x;
        dy = s->y - y;
        if (dx * dx + dy * dy > r * r) continue;
        s->hp -= dmg;
        if (s->hp > 0) continue;
        if (n < cap) out_slots[n] = i;
        ++n;
        s->active = 0;
        --p->live;
    }
    return n;
}

void pofv_spirit_remove(pofv_spirit_pool_t *p, int32_t slot)
{
    if (!p || slot < 0 || slot >= POFV_SPIRIT_MAX) return;
    if (!p->s[slot].active) return;
    p->s[slot].active = 0;
    --p->live;
}
