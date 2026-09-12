/* pofv_core.c —— 花映塚（id 109）纯逻辑的实现。语义与数值纪律见 pofv_core.h 的头注释。
 * 纯 C11，无引擎地址、无 libm、无 float 返回值（i386 x87 不变式）。 */
#include "pofv_core.h"
#include <string.h>

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
    st->max_level = pofv_charge_max_level(st->gauge);       /* 蓄力期间吃到气 ⇒ 上限抬升 */
    lv = st->hold_frames / POFV_CHARGE_FRAMES_PER_LEVEL;
    if (lv > st->max_level) lv = st->max_level;
    if (lv > st->level) { st->level = lv; return 1; }       /* 升级：卡侧播「叮」*/
    return 0;
}

int pofv_charge_suppress(const pofv_state_t *st)
{
    /* ★ 拦 Z 条件（用户 2026-09-12）：按住 Z **且已升到 ≥1 级**。
     * 还没升到 1 级的那几帧不拦 ⇒ 点一下 Z 照旧发一发（原生行循环照跑）。 */
    if (!st) return 0;
    if (st->phase != POFV_CHARGE_HOLD) return 0;
    if (st->level < POFV_CA_LEVEL_MIN) return 0;
    return st->gauge >= POFV_CA_LEVEL_MIN;                  /* 没气就蓄不动 ⇒ 也不拦 */
}

int pofv_charge_release(pofv_state_t *st, int *out_level)
{
    int32_t lv;
    if (!st || st->phase != POFV_CHARGE_HOLD) return 0;
    lv = st->level;
    st->phase = POFV_CHARGE_IDLE;
    st->hold_frames = 0;
    st->level = 0;
    st->max_level = pofv_charge_max_level(st->gauge);
    if (lv < POFV_CA_LEVEL_MIN || !pofv_ca_available(st, lv)) {
        /* 等级 0（点射）或气不够：什么都不发动 —— 交回原生弹 */
        if (out_level) *out_level = 0;
        return 0;
    }
    st->gauge -= pofv_ca_cost(lv);
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

/* ------------------------------------------------- C. 爆风 / 红圈 / SP / 记账 */

int pofv_chain_register_kill(pofv_state_t *st, int32_t frame)
{
    if (!st) return 1;
    if (st->last_kill_frame >= 0 && (frame - st->last_kill_frame) <= POFV_CHAIN_WINDOW_FRAMES)
        ++st->chain_kills;
    else
        st->chain_kills = 1;
    st->last_kill_frame = frame;
    if (st->chain_kills > POFV_BLAST_CHAIN_MAX) st->chain_kills = POFV_BLAST_CHAIN_MAX;
    return st->chain_kills;
}

/* ❓ 自定占位（TODO(th09)：原版爆风半径按角色 / 连爆质量各异，未逆到）。
 * 约束：单调不减、chain 越界钳制（单测保证）。 */
static const float s_blast_r[POFV_BLAST_CHAIN_MAX] = { 64.0f, 80.0f, 96.0f, 120.0f };

void pofv_blast_radius(int chain, float *out)
{
    int32_t c;
    if (!out) return;
    c = clamp_i32(chain, 1, POFV_BLAST_CHAIN_MAX);
    *out = s_blast_r[c - 1];
}

/* ❓ 自定占位（TODO(th09)：原版 C1–C4 的红圈半径未逆到；一手只给语义
 * 「等级越高 → 消弹圈越大；C4 = 近乎全屏」）。 240 ≈ 全屏（游戏区半宽 ≈ 208–240）。*/
static const float s_ca_r[POFV_CA_LEVEL_MAX] = { 80.0f, 128.0f, 200.0f, 480.0f };

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

/* ---- 爆风队列 ---- */

void pofv_blast_queue_init(pofv_blast_queue_t *q)
{
    if (!q) return;
    memset(q, 0, sizeof *q);
}

int pofv_blast_enqueue(pofv_blast_queue_t *q, float x, float y, int32_t chain)
{
    int32_t tail;
    if (!q) return 0;
    if (q->n >= POFV_BLAST_QUEUE_MAX) { ++q->dropped; return 0; }
    tail = q->head + q->n;
    if (tail >= POFV_BLAST_QUEUE_MAX) tail -= POFV_BLAST_QUEUE_MAX;
    q->item[tail].x = x;
    q->item[tail].y = y;
    q->item[tail].chain = clamp_i32(chain, 1, POFV_BLAST_CHAIN_MAX);
    ++q->n;
    return 1;
}

int pofv_blast_take(pofv_blast_queue_t *q, pofv_blast_t *out)
{
    if (!q || !out || q->n <= 0) return 0;
    *out = q->item[q->head];
    if (++q->head >= POFV_BLAST_QUEUE_MAX) q->head = 0;
    --q->n;
    return 1;
}

int pofv_blast_take_budget(pofv_blast_queue_t *q, pofv_blast_t *out, int32_t *budget)
{
    if (!budget || *budget <= 0) return 0;
    if (!pofv_blast_take(q, out)) return 0;
    --*budget;
    return 1;
}

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
