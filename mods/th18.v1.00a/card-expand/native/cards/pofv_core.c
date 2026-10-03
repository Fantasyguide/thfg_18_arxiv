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
        s->life = pofv_storm_life_for_chain(s->chain);   /* ★ 档只放大范围 ⇒ 寿命按档 */
        s->r0 = POFV_STORM_R0;
        s->dr = POFV_STORM_DR;
        s->kind = POFV_STORM_KIND_STORM;
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
    if (!out) return;
    if (!st || !st->active || st->life <= 0) { *out = 0.0f; return; }
    /* ★★ 2026-10-01：改用**一手 TH09 的节点律**（`r = r0 + dr × age`）—— 与结界的锚定场
     *   共用 `pofv_node_radius()`（owner：「aura 和锚定场本质上是同一个东西」）。
     *   旧实现是「16 → R_chain 固定 30 帧」的自造律（前沿 5.3–10 px/帧 ⇒ 瞬间连锁）。*/
    pofv_node_radius(st->r0, st->dr, st->age, st->life, out);
}

int pofv_storm_spawn_field(pofv_storm_pool_t *p, float x, float y,
                           float r0, float dr, int32_t life, int32_t kind)
{
    int i;
    if (!p) return -1;
    for (i = 0; i < POFV_STORM_MAX; ++i) {
        pofv_storm_t *s = &p->s[i];
        if (s->active) continue;
        memset(s, 0, sizeof *s);
        s->x = x; s->y = y;
        s->active = 1;
        s->age = 0;
        s->life = life > 0 ? life : 1;
        s->r0 = r0;
        s->dr = dr;
        s->kind = kind;
        s->chain = 0;
        ++p->live;
        return i;
    }
    ++p->dropped;
    return -1;
}

void pofv_ca_field_params(int level, float *r0, float *dr, int32_t *life)
{
    /* ★ 一手：L1 `0x403c30` = 0 / 4.0 / 48；L2 `0x403c90` = 0 / 4.0 / 64；
     *        L3 `0x403cf0` = 0 / 3.5 / 128（L ≥ 3 同一支）。*/
    float a = 0.0f, b = 4.0f;
    int32_t l = 48;
    if (level <= 1)      { a = 0.0f; b = 4.0f; l = 48; }
    else if (level == 2) { a = 0.0f; b = 4.0f; l = 64; }
    else                 { a = 0.0f; b = 3.5f; l = 128; }
    if (r0) *r0 = a;
    if (dr) *dr = b;
    if (life) *life = l;
}

int pofv_ca_field_start(pofv_storm_pool_t *p, float x, float y, int level)
{
    float r0, dr;
    int32_t life;
    if (level <= 0)   /* ★ 「松手那一下」（一手 `0x41e875`：16 / 20 / 8 → 176）*/
        return pofv_storm_spawn_field(p, x, y, POFV_CA_BURST_R0, POFV_CA_BURST_DR,
                                      POFV_CA_BURST_LIFE, POFV_STORM_KIND_CA);
    pofv_ca_field_params(level, &r0, &dr, &life);
    return pofv_storm_spawn_field(p, x, y, r0, dr, life, POFV_STORM_KIND_CA);
}

int pofv_storm_is_storm(const pofv_storm_t *st)
{
    return (st && st->active && st->kind == POFV_STORM_KIND_STORM) ? 1 : 0;
}

void pofv_node_radius(float r0, float dr, int32_t age, int32_t life, float *out)
{
    float r;
    if (!out) return;
    if (age < 0) age = 0;
    if (life > 0 && age > life) age = life;
    r = r0 + dr * (float)age;
    if (!(r > 0.0f)) r = 0.0f;                       /* 负半径 / NaN ⇒ 0（没展开）*/
    *out = r;
}

void pofv_storm_profile_for(int character, pofv_storm_profile_t *out)
{
    if (!out) return;
    memset(out, 0, sizeof *out);
    switch (character) {
    case 2:   /* ★ 咲夜 → **TH09 的暴风**：连锁 + 能直接打死幽灵 */
        out->chain = 1;
        out->kills_ghosts = 1;
        break;
    case 3:   /* ★ 早苗 → **TH19**：没有暴风连锁，**只是暴风消弹** */
        out->chain = 0;
        out->kills_ghosts = 0;
        break;
    case 0:   /* 灵梦 / 魔理沙 —— ❓ owner 待定 ⇒ 占位 = TH09 档（= 改造前行为）*/
    case 1:
    default:
        out->chain = 1;
        out->kills_ghosts = 1;
        break;
    }
}

int32_t pofv_storm_life_for_chain(int32_t chain)
{
    float r1;
    int32_t c, life;

    c = clamp_i32(chain, 1, POFV_BLAST_CHAIN_MAX);
    pofv_blast_radius(c, &r1);
    if (POFV_STORM_DR <= 0.0f) return 1;
    life = (int32_t)((r1 - POFV_STORM_R0) / POFV_STORM_DR + 0.5f);   /* 四舍五入 */
    if (life < 1) life = 1;
    return life;
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
        if (!pofv_storm_is_storm(s)) continue;     /* ★ CA 红圈不参与连爆（owner：两回事）*/
        pofv_storm_radius(s, &r);                  /* 每槽自带 r0/dr ⇒ 按**这一个场**算 */
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
    if (!st || n_white <= 0) return;
    (void)chain;                                 /* 连爆档只影响**道具面值**（见 pofv_power_value_cancel）*/
    st->white_cancelled += n_white;
    st->any_cancelled += n_white;
    st->sp += n_white * POFV_SP_PER_WHITE_CANCEL;
    /* ★★ 2026-10-01（一手 TH19 §5：「連爆に巻き込んだ敵**や敵弾**は**コンボ**になります」）：
     *   被连爆消掉的弹**也算コンボ** ⇒ 这里累加（满 100 的掉落在卡侧统一处理）。*/
    (void)pofv_combo_add(st, n_white * POFV_COMBO_PER_CANCEL);
    /* ★ 2026-10-01（口径 5）：**不再直接加气** —— 充能一律经灵力道具（卡侧撒 / 收取）。 */
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
    st->sp += POFV_SP_PER_KILL;
    /* ★ 2026-10-01（口径 5）：同上，充能走卡侧的灵力道具（`POFV_POWER_PTS_KILL`）。 */
}

int32_t pofv_power_value_cancel(int32_t n_white, int32_t chain)
{
    int32_t c;
    if (n_white <= 0) return 0;
    c = clamp_i32(chain, 1, POFV_BLAST_CHAIN_MAX);
    return n_white * POFV_POWER_PTS_CANCEL * c;
}

/* ---- 暴风池的实现见上面 §C 开头（`pofv_storm_*`）；旧的爆风队列已删除（场模型不需要它）---- */

/* ------------------------------------------------- E. 灵力道具（自建实体池，见 pofv_core.h §E） */

/* 8 方向单位向量（一批多颗时铺开用；**不引 libm** —— 0.70710678f = √2/2 字面量）。*/
static const float s_power_dir[8][2] = {
    {  1.0f,        0.0f       }, {  0.70710678f,  0.70710678f },
    {  0.0f,        1.0f       }, { -0.70710678f,  0.70710678f },
    { -1.0f,        0.0f       }, { -0.70710678f, -0.70710678f },
    {  0.0f,       -1.0f       }, {  0.70710678f, -0.70710678f }
};

void pofv_power_pool_init(pofv_power_pool_t *p)
{
    if (!p) return;
    memset(p, 0, sizeof *p);
}

/* ★★ 2026-10-01（owner「动态不对」）：飞散方向 = **随机角**（一手：`randf_minus_pi_to_pi()`）。
 *   本层**禁 libm**（不能算 cos/sin）⇒ 用 **64 向单位向量表**近似「随机角」：
 *   取表的随机下标 = 5.6° 一档，肉眼与连续随机角无异。
 *   ⚠ 表是**算出来的常数**（Python `math.cos/sin`，构建前生成一次，写死在这里）——
 *     与「不引 libm」的铁律一致（运行时零 libm 调用）。*/
static const float s_scatter_dir[POFV_POWER_SCATTER_DIRS][2] = {
    { -0.998795f, -0.049068f },
    { -0.989177f, -0.146730f },
    { -0.970031f, -0.242980f },
    { -0.941544f, -0.336890f },
    { -0.903989f, -0.427555f },
    { -0.857729f, -0.514103f },
    { -0.803208f, -0.595699f },
    { -0.740951f, -0.671559f },
    { -0.671559f, -0.740951f },
    { -0.595699f, -0.803208f },
    { -0.514103f, -0.857729f },
    { -0.427555f, -0.903989f },
    { -0.336890f, -0.941544f },
    { -0.242980f, -0.970031f },
    { -0.146730f, -0.989177f },
    { -0.049068f, -0.998795f },
    {  0.049068f, -0.998795f },
    {  0.146730f, -0.989177f },
    {  0.242980f, -0.970031f },
    {  0.336890f, -0.941544f },
    {  0.427555f, -0.903989f },
    {  0.514103f, -0.857729f },
    {  0.595699f, -0.803208f },
    {  0.671559f, -0.740951f },
    {  0.740951f, -0.671559f },
    {  0.803208f, -0.595699f },
    {  0.857729f, -0.514103f },
    {  0.903989f, -0.427555f },
    {  0.941544f, -0.336890f },
    {  0.970031f, -0.242980f },
    {  0.989177f, -0.146730f },
    {  0.998795f, -0.049068f },
    {  0.998795f,  0.049068f },
    {  0.989177f,  0.146730f },
    {  0.970031f,  0.242980f },
    {  0.941544f,  0.336890f },
    {  0.903989f,  0.427555f },
    {  0.857729f,  0.514103f },
    {  0.803208f,  0.595699f },
    {  0.740951f,  0.671559f },
    {  0.671559f,  0.740951f },
    {  0.595699f,  0.803208f },
    {  0.514103f,  0.857729f },
    {  0.427555f,  0.903989f },
    {  0.336890f,  0.941544f },
    {  0.242980f,  0.970031f },
    {  0.146730f,  0.989177f },
    {  0.049068f,  0.998795f },
    { -0.049068f,  0.998795f },
    { -0.146730f,  0.989177f },
    { -0.242980f,  0.970031f },
    { -0.336890f,  0.941544f },
    { -0.427555f,  0.903989f },
    { -0.514103f,  0.857729f },
    { -0.595699f,  0.803208f },
    { -0.671559f,  0.740951f },
    { -0.740951f,  0.671559f },
    { -0.803208f,  0.595699f },
    { -0.857729f,  0.514103f },
    { -0.903989f,  0.427555f },
    { -0.941544f,  0.336890f },
    { -0.970031f,  0.242980f },
    { -0.989177f,  0.146730f },
    { -0.998795f,  0.049068f },
};


/* ★ 飞散方向/初速的伪随机源（xorshift32；**纯整数** ⇒ 不引 libm、单测可复现）。
 *   一手要的是「角在 ±π 上随机、速度 `rand×1.9+0.2`」——本层禁 libm（算不了 cos/sin）
 *   ⇒ 角用 **64 向单位向量表**取随机下标近似，速度用这里的 [0,1) 映射。*/
static uint32_t power_rng_next(pofv_power_pool_t *p)
{
    uint32_t x = p->rng ? p->rng : 0x1234567u;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    p->rng = x;
    return x;
}
static float power_rng_unit(pofv_power_pool_t *p)      /* → [0, 1) */
{
    return (float)(power_rng_next(p) >> 8) * (1.0f / 16777216.0f);
}

int pofv_power_spawn(pofv_power_pool_t *p, float x, float y, int32_t value, int32_t src)
{
    int i;
    if (!p) return -1;
    for (i = 0; i < POFV_POWER_MAX; ++i) {
        pofv_power_t *it = &p->s[i];
        if (it->active) continue;
        memset(it, 0, sizeof *it);
        it->active = 1;
        it->x = x;
        it->y = y;
        it->value = value;
        it->life = POFV_POWER_LIFE;
        it->src = src;
        /* ★★ 一手（TH16 季节道具，owner：「动态这块按照季节点做就行」）：
         *   · **待发射 6 帧**（`item+0xc50 = 6`）—— 延时里什么都不做；",
         *   · 出生角 = **随机 ±π**（一手 `randf_minus_pi_to_pi()`）⇒ 用 64 向表取随机下标近似；
         *   · 出生速度 = **`rand×1.9 + 0.2` ⇒ 0.2..2.1**（`0x41d880`–`0x41d8bb`）。*/
        {
            uint32_t di = power_rng_next(p) % POFV_POWER_SCATTER_DIRS;
            it->delay = POFV_POWER_LAUNCH_DELAY;
            it->dx = s_scatter_dir[di][0];
            it->dy = s_scatter_dir[di][1];
            it->lr = power_rng_unit(p) * POFV_POWER_SPEED_SCALE + POFV_POWER_SPEED_MIN;
            it->st = 0;
            it->vy = 0.0f;
            it->mspd = 0.0f;
        }
        ++p->seq;
        ++p->live;
        ++p->spawned;
        return i;
    }
    return -1;                                  /* 池满（打包撒会先算余量，正常到不了这里）*/
}

static int power_free_slots(const pofv_power_pool_t *p)
{
    int i, n = 0;
    if (!p) return 0;
    for (i = 0; i < POFV_POWER_MAX; ++i) if (!p->s[i].active) ++n;
    return n;
}

int pofv_power_spawn_packed(pofv_power_pool_t *p, float x, float y, int32_t n_units,
                            int32_t pts_each, int32_t max_spawn, int32_t src, int32_t *out_pts)
{
    int32_t total, k, base, rem, i, got = 0;
    int free_slots;

    if (out_pts) *out_pts = 0;
    if (!p || n_units <= 0 || pts_each <= 0 || max_spawn <= 0) return 0;
    total = n_units * pts_each;
    if (total <= 0) {                            /* 溢出保护：真溢出就什么都不撒（不撒垃圾面值）*/
        p->dropped += n_units;
        return 0;
    }
    free_slots = power_free_slots(p);
    k = n_units < max_spawn ? n_units : max_spawn;
    if (k > free_slots) k = free_slots;
    if (k <= 0) { p->dropped += n_units; return 0; }

    /* ★ 打包：total 分摊到 k 颗上，**余数给第一颗** ⇒ Σ面值 == total（一颗都不少给）。*/
    base = total / k;
    rem  = total - base * k;
    for (i = 0; i < k; ++i) {
        float px = x, py = y;
        int32_t v = base + (i == 0 ? rem : 0);
        if (k > 1) {                             /* 多颗时铺开（只有一颗时原地 = 事件位置）*/
            const float *d = s_power_dir[i & 7];
            float rr = POFV_POWER_SPAWN_R * (float)((i / 8) + 1);
            px += d[0] * rr;
            py += d[1] * rr;
        }
        if (pofv_power_spawn(p, px, py, v, src) < 0) break;
        got += v;
    }
    if (i < n_units) p->dropped += (n_units - i);
    if (out_pts) *out_pts = got;
    return (int)i;
}

int pofv_power_tick(pofv_power_pool_t *p, const float *player, int32_t *out_pts)
{
    int i, got = 0;
    int32_t pts = 0;
    const float absorb2 = POFV_POWER_ABSORB_R * POFV_POWER_ABSORB_R;

    if (out_pts) *out_pts = 0;
    if (!p) return 0;
    for (i = 0; i < POFV_POWER_MAX; ++i) {
        pofv_power_t *it = &p->s[i];
        if (!it->active) continue;
        ++it->age;
        /* ══ TH16 季节道具的状态机（owner：「**动态这块按照季节点做就行**」）════════════════════
         *   一手逐条见 `engine/season/th16/01-season-data-and-tables.md` §4：
         *     state 3 飞散  → pos += vel、速度 −0.03/帧、**无重力**、角度不变（`0x42f82c`–`0x42f83f`）
         *     state 1 飘落  → **重力 0.03**（`0x4b90a4`）、`vy > 2.0 → 2.0`（`0x494558`）、
         *                     `vy ≥ 0 ⇒ vx = 0`（`0x42fb5f` 起）＝只有竖直运动
         *     state 4 回收线 → `player.y < 128` ⇒ 速度 = `sht+8` = 6.0（实读 `pl00.sht` 的 +8）
         *     state 5 吸引   → 进吸引矩形 ⇒ 速度 = `sht+8/3` = 2.0，**每帧 +0.2、上限 12.0**（`0x4945d4`）
         *   出界 = y 472 / |x| 200（§4 尾注）。
         * ⚠ 本层**禁 libm** ⇒ 「朝自机」的方向用 **Chebyshev 归一**（除以 |dx|,|dy| 的较大者）
         *   ——不是单位向量，但走向正确；这是本实现**唯一的近似**（登记 `TUNING.md`）。*/
        if (it->delay > 0) { --it->delay; continue; }        /* 待发射 6 帧：什么都不做 */

        if (player) {
            float dx = player[0] - it->x;
            float dy = player[1] - it->y;
            float d2 = dx * dx + dy * dy;
            if (d2 <= absorb2) {                 /* 收取：点入账、槽当场释放 */
                pts += it->value;
                it->active = 0;
                --p->live;
                ++p->absorbed;
                ++got;
                continue;
            }
            if (player[1] < POFV_POWER_COLLECT_Y) {
                if (it->st != 2) it->st = 2;                  /* 过回收线（一手 state 4）*/
            } else if (it->st != 3 &&
                       dx >= -POFV_POWER_BOX_HALF_W && dx <= POFV_POWER_BOX_HALF_W &&
                       dy >= -POFV_POWER_BOX_HALF_H && dy <= POFV_POWER_BOX_HALF_H) {
                it->st = 3;                                   /* 进吸引矩形（一手 state 5）*/
                if (it->mspd < POFV_POWER_MAG_SPEED0) it->mspd = POFV_POWER_MAG_SPEED0;
            }
        }

        switch (it->st) {
        case 0:                                              /* 飞散（无重力）*/
            it->x += it->dx * it->lr;
            it->y += it->dy * it->lr;
            it->lr -= POFV_POWER_DECEL;
            if (it->lr <= 0.0f) { it->lr = 0.0f; it->st = 1; it->vy = 0.0f; }
            break;
        case 1:                                              /* ★ 飘落（加速下落 + 封顶）*/
            it->vy += POFV_POWER_GRAVITY;
            if (it->vy > POFV_POWER_FALL_MAX) it->vy = POFV_POWER_FALL_MAX;
            it->y += it->vy;                                 /* `vy ≥ 0 ⇒ vx = 0`：只动 y */
            break;
        case 2:                                              /* 过回收线：朝自机 6.0/帧 */
            if (player) {
                float dx = player[0] - it->x, dy = player[1] - it->y;
                float m = (dx < 0.0f ? -dx : dx), ay = (dy < 0.0f ? -dy : dy);
                if (ay > m) m = ay;
                if (m > 0.0001f) { it->x += dx / m * POFV_POWER_LINE_SPEED;
                                   it->y += dy / m * POFV_POWER_LINE_SPEED; }
            } else { it->st = 1; }                           /* 没有自机（单测/过场）：回去飘落 */
            break;
        default:                                             /* 吸引：每帧加速、上限 12.0 */
            if (player) {
                float dx = player[0] - it->x, dy = player[1] - it->y;
                float m = (dx < 0.0f ? -dx : dx), ay = (dy < 0.0f ? -dy : dy);
                it->mspd += POFV_POWER_MAG_ACCEL;
                if (it->mspd > POFV_POWER_MAG_SPEED_MAX) it->mspd = POFV_POWER_MAG_SPEED_MAX;
                if (ay > m) m = ay;
                if (m > 0.0001f) { it->x += dx / m * it->mspd; it->y += dy / m * it->mspd; }
            } else { it->st = 1; }
            break;
        }

        /* ★ 一手：出界 = y **472** / |x| **200** ⇒ 释放（比寿命更早、更符合原版）。*/
        if (it->y > POFV_POWER_OUT_Y || it->x < -POFV_POWER_OUT_X || it->x > POFV_POWER_OUT_X) {
            it->active = 0; --p->live; ++p->expired; continue;
        }
        if (it->age >= it->life) { it->active = 0; --p->live; ++p->expired; }
    }
    if (out_pts) *out_pts = pts;
    return got;
}

int pofv_power_clear(pofv_power_pool_t *p)
{
    int n;
    if (!p) return 0;
    n = p->live;
    pofv_power_pool_init(p);
    return n;
}

int pofv_power_spawn_packed_at(pofv_power_pool_t *p, const float *ox, const float *oy,
                               int32_t n_units, int32_t pts_each, int32_t max_spawn,
                               int32_t src, int32_t *out_pts)
{
    int32_t total, k, base, rem, i, got = 0;
    int free_slots;

    if (out_pts) *out_pts = 0;
    if (!p || !ox || !oy || n_units <= 0 || pts_each <= 0 || max_spawn <= 0) return 0;
    total = n_units * pts_each;
    if (total <= 0) { p->dropped += n_units; return 0; }
    free_slots = power_free_slots(p);
    k = n_units < max_spawn ? n_units : max_spawn;
    if (k > free_slots) k = free_slots;
    if (k <= 0) { p->dropped += n_units; return 0; }
    base = total / k;
    rem  = total - base * k;
    for (i = 0; i < k; ++i) {                    /* 位置由调用方给（命中点原位）*/
        int32_t v = base + (i == 0 ? rem : 0);
        if (pofv_power_spawn(p, ox[i], oy[i], v, src) < 0) break;
        got += v;
    }
    if (i < n_units) p->dropped += (n_units - i);
    if (out_pts) *out_pts = got;
    return (int)i;
}

/* ------------------------------------------------------- F. 固灵结界（几何，见 pofv_core.h §F）*/

int pofv_aura_hit_ex(float dx, float dy, float r, float pad, int pad_linear)
{
    float d2 = dx * dx + dy * dy;

    if (!(r > 0.0f)) return 0;                       /* 结界没展开 ⇒ 永不命中 */
    if (pad_linear) {
        /* ★ 一手 TH19 §7.3 的**第二支**（`th19:0x418690`，Sanae/Nazrin/Seiran/Toutetsu/PL16/PL17
         *   六类共用）：`dx²+dy² <= (r + pad)²` —— pad **线性并进半径**。*/
        float rr = r + pad;
        return d2 <= rr * rr;
    }
    /* ★ 一手 TH19 §7.3 的**第一支**（`th19:0x416f10` Reimu）：`dx²+dy² < r² + pad²`
     *   —— pad **以平方和方式**并入（严格 `<`）。*/
    return d2 < (r * r + pad * pad);
}

int pofv_aura_hit(float dx, float dy, float r, float pad)
{
    return pofv_aura_hit_ex(dx, dy, r, pad, /*pad_linear=*/0);
}

/* ---- 逐角色结界档案（owner 2026-10-01 口径；见 pofv_core.h §F 的长注）---- */

int32_t pofv_aura_shift_charge(int32_t shift_frames)
{
    int32_t c;
    if (shift_frames <= 0) return 0;
    c = shift_frames * POFV_AURA_SHIFT_CHARGE_PER_FRAME;
    if (c > POFV_AURA_CHARGE_MAX) c = POFV_AURA_CHARGE_MAX;
    return c;
}

void pofv_aura_profile_for(int character, pofv_aura_profile_t *out)
{
    if (!out) return;
    memset(out, 0, sizeof *out);
    switch (character) {
    case 2:   /* ★ 咲夜 → **TH09 的「消弹圈 = 锚定场」**（一手 §1.4 的「CA 释放」节点）*/
        out->mode = POFV_AURA_FIELD;
        out->pad_linear = 0;                     /* 场形态的判定沿用第一支（圆，pad 平方和）*/
        out->r0 = POFV_AURA_TH09_R0;             /* 16（一手 `th09:0x41e875`）*/
        out->dr = POFV_AURA_TH09_DR;             /* 20（一手）*/
        out->life = POFV_AURA_TH09_LIFE;         /* 8 帧（一手；终点 16 + 20×8 = 176）*/
        break;
    case 3:   /* ★ 早苗 → **TH19 `AuraSanaeInf`**（圆 + K = 128；半径由「Shift 蓄力」算）*/
        out->mode = POFV_AURA_STATIC;
        out->pad_linear = 1;                     /* Sanae 支 `<= (r+pad)²`（一手 §7.3）*/
        out->r_fixed = 0.0f;                     /* 不用固定值 ⇒ 走下面的 K */
        out->k = POFV_AURA_SANAE_K;              /* 一手 §7.2：`AuraSanaeInf` K = 128 */
        out->grow_frames = POFV_AURA_SANAE_GROW_FRAMES;   /* 一手：`+0x94` = 60 */
        /* ★ 「同一个东西」：生长速度也按 px/帧 给（= 满档目标 256 / 60 帧 ≈ 4.27 px/帧）。*/
        out->dr = out->k * (float)POFV_AURA_CHARGE_MAX / 100.0f / (float)out->grow_frames;
        break;
    case 0:   /* 灵梦 —— ❓ owner 待定 ⇒ 占位：TH19 `AuraReimuInf` 的常驻圆 96 */
    case 1:   /* 魔理沙 —— ❓ 同上 */
    default:
        out->mode = POFV_AURA_STATIC;
        out->pad_linear = 0;                     /* Reimu 支 `dx²+dy² < r²+pad²` */
        out->r_fixed = POFV_AURA_R;              /* 96（= 改造前的行为，零变化）*/
        out->k = 0.0f;
        break;
    }
}

void pofv_aura_radius(const pofv_aura_profile_t *p, int32_t age, int32_t charge, float *out)
{
    if (!out) return;
    if (!p) { *out = 0.0f; return; }

    if (p->mode == POFV_AURA_FIELD) {
        /* ★ 与暴风**共用同一个节点律**（owner：「aura 和锚定场本质上是同一个东西」）。*/
        pofv_node_radius(p->r0, p->dr, age, p->life, out);
        return;
    }
    if (p->k > 0.0f) {                            /* STATIC + 蓄力（早苗）——「有上限的生长」*/
        float target, r;
        int32_t c = charge;
        if (c < 0) c = 0;
        if (c > POFV_AURA_CHARGE_MAX) c = POFV_AURA_CHARGE_MAX;
        target = (float)c * p->k / 100.0f;        /* ★ 一手 TH19：`clamp(S+0x8c,0,200) × K/100` */
        r = p->dr * (float)(age > 0 ? age : 0);   /* ★ 与节点律同一个「按 px/帧 长」的模型 */
        if (r > target) r = target;               /* 长到蓄力给的上限就停 */
        *out = r > 0.0f ? r : 0.0f;
        return;
    }
    *out = p->r_fixed;                            /* STATIC 固定（占位档）*/
}

/* ------------------------------------------- I. ボスアタック（见 pofv_core.h §I）*/

int pofv_bossatk_start(pofv_bossatk_t *b, int ca_level)
{
    if (!b) return 0;
    if (ca_level < POFV_BOSSATK_MIN_LEVEL) return 0;   /* ★ owner：C1 起就有 */
    if (ca_level > 4) ca_level = 4;
    b->active = 1;
    b->level = ca_level;
    switch (ca_level) {
    case 1:  b->left = POFV_BOSSATK_C1_FRAMES; break;
    case 2:  b->left = POFV_BOSSATK_C2_FRAMES; break;
    case 3:  b->left = POFV_BOSSATK_C3_FRAMES; break;
    default: b->left = POFV_BOSSATK_C4_FRAMES; break;
    }
    return 1;
}

int pofv_bossatk_tick(pofv_bossatk_t *b)
{
    if (!b || !b->active) return 0;
    if (b->left <= 0) { b->active = 0; return 0; }
    --b->left;
    if (b->left <= 0) b->active = 0;
    return 1;
}

void pofv_bossatk_stop(pofv_bossatk_t *b)
{
    if (!b) return;
    b->active = 0;
    b->left = 0;
    b->level = 0;
}

int pofv_bossatk_is_boss(const pofv_bossatk_t *b)
{
    return (b && b->active && b->level >= 4) ? 1 : 0;
}

/* ------------------------------------------- H. オートパリア（抵挡护盾，见 pofv_core.h §H）*/

int pofv_shield_arm(pofv_state_t *st, int ca_level)
{
    if (!st) return 0;
    if (ca_level < POFV_SHIELD_ARM_LEVEL) return 0;   /* ★ 一手：只有阶段 3 / 4 回复（0x40b420）*/
    if (st->shield) return 0;                         /* 已经有盾：不重复（也不刷新）*/
    st->shield = 1;
    return 1;
}

int pofv_shield_take(pofv_state_t *st)
{
    if (!st || !st->shield) return 0;
    st->shield = 0;                                   /* ★ 一手：「挡 1 发」（state 1 → 2）*/
    return 1;
}

int pofv_shield_up(const pofv_state_t *st)
{
    return (st && st->shield) ? 1 : 0;
}

/* ------------------------------------------------- G. 灵（自制真敌人，见 pofv_core.h §G） */

void pofv_ghost_pool_init(pofv_ghost_pool_t *p)
{
    if (!p) return;
    memset(p, 0, sizeof *p);
}

int pofv_ghost_cycle_tick(pofv_ghost_pool_t *p, int boss_present, int32_t *out_kind)
{
    if (!p) return 0;
    if (p->cycle > 0) --p->cycle;
    if (p->cycle > 0) return 0;
    if (boss_present) return 0;                     /* 口径 9：任何 boss 在场就不撒（不重置）*/
    if (p->live >= POFV_GHOST_MAX) return 0;        /* 同屏上限（一手 TH09 = 4）*/
    p->cycle = POFV_GHOST_CYCLE_FRAMES;             /* 撒成功才重新计时 */
    if (out_kind) *out_kind = p->next_kind;
    p->next_kind = (p->next_kind + 1) % POFV_GHOST_KIND_MAX;
    return 1;
}

int pofv_ghost_register(pofv_ghost_pool_t *p, int32_t enemy_id, int32_t kind)
{
    int i;
    if (!p || enemy_id <= 0) return -1;
    for (i = 0; i < POFV_GHOST_MAX; ++i) {
        pofv_ghost_t *g = &p->s[i];
        if (g->active) continue;
        memset(g, 0, sizeof *g);
        g->active = 1;
        g->enemy_id = enemy_id;
        g->kind = kind;
        ++p->live;
        ++p->spawned;
        return i;
    }
    return -1;                                      /* 池满：调用方应立刻把刚建的敌人删掉 */
}

int pofv_ghost_find(const pofv_ghost_pool_t *p, int32_t enemy_id)
{
    int i;
    if (!p || enemy_id <= 0) return -1;
    for (i = 0; i < POFV_GHOST_MAX; ++i)
        if (p->s[i].active && p->s[i].enemy_id == enemy_id) return i;
    return -1;
}

int pofv_ghost_calm(pofv_ghost_pool_t *p, int slot)
{
    pofv_ghost_t *g;
    if (!p || slot < 0 || slot >= POFV_GHOST_MAX) return 0;
    g = &p->s[slot];
    if (!g->active || g->calmed) return 0;          /* 幂等：已沈静化的不重置计时 */
    g->calmed = 1;
    g->calm_age = 0;
    ++p->calmed_total;
    return 1;
}

int pofv_ghost_tick(pofv_ghost_pool_t *p, int32_t *out_slots, int cap)
{
    int i, n = 0;
    if (!p) return 0;
    for (i = 0; i < POFV_GHOST_MAX; ++i) {
        pofv_ghost_t *g = &p->s[i];
        if (!g->active) continue;
        ++g->age;
        if (!g->calmed) continue;
        ++g->calm_age;
        if (g->calm_age < POFV_GHOST_CALM_FRAMES) continue;
        if (n >= cap) continue;                     /* 名额用完：留在槽里，下一帧再成佛（不漏报）*/
        if (out_slots) out_slots[n] = i;
        ++n;
        g->active = 0;                              /* 成佛：槽当场释放（ECL 侧自删由调用方喊）*/
        --p->live;
        ++p->expired;
    }
    return n;
}

int pofv_ghost_release(pofv_ghost_pool_t *p, int slot)
{
    if (!p || slot < 0 || slot >= POFV_GHOST_MAX) return 0;
    if (!p->s[slot].active) return 0;
    p->s[slot].active = 0;
    --p->live;
    return 1;
}

int pofv_ghost_clear(pofv_ghost_pool_t *p)
{
    int n;
    if (!p) return 0;
    n = p->live;
    pofv_ghost_pool_init(p);
    return n;
}

/* ---------------------------------------------------------------- 数学：SSE 版 atan2（见头文件）

 * 手写近似（❓ 自定）：先归约到第一象限的 [0, 1] 区间，再用一个 5 项多项式算 atan，
 * 最后按象限补 π/2 / π 与符号。全程只用标量加减乘除 ⇒ **不会产出 x87 指令**
 * （`make dllx87` 闸门要求我们的目标文件里 x87 数 = 0；实测引擎那条 i387 atan2 会踩闸门）。*/
void pofv_atan2(float y, float x, float *out)
{
    const float PI = 3.14159265358979f;
    const float HALF_PI = 1.57079632679490f;
    float ay = y < 0.0f ? -y : y;
    float ax = x < 0.0f ? -x : x;
    float z, r, z2;

    if (!out) return;
    if (ax == 0.0f && ay == 0.0f) { *out = 0.0f; return; }

    /* 第一八分圆：z = min/max ∈ [0,1] */
    if (ay <= ax) {
        z = (ax != 0.0f) ? ay / ax : 0.0f;
        z2 = z * z;
        r = z * (0.9998660f + z2 * (-0.3302995f + z2 * (0.1801410f
              + z2 * (-0.0851330f + z2 * 0.0208351f))));
    } else {
        z = (ay != 0.0f) ? ax / ay : 0.0f;
        z2 = z * z;
        r = z * (0.9998660f + z2 * (-0.3302995f + z2 * (0.1801410f
              + z2 * (-0.0851330f + z2 * 0.0208351f))));
        r = HALF_PI - r;                       /* 用余角 */
    }
    /* 按 x 的符号补到第二/三象限 */
    if (x < 0.0f) r = PI - r;
    /* 按 y 的符号取负 */
    *out = (y < 0.0f) ? -r : r;
}

/* ---------------------------------------------------------------- 逐弹归属（见头文件）

 * 集合逐条来自 `assets/ecl/pofv109_boss.ecl.txt` 的 `etSprite` 调用（三角色 × 两档）。*/
int pofv_bossatk_bullet_type_ours(uint32_t type)
{
    static const uint32_t ours[] = { 0u, 1u, 2u, 11u, 12u, 13u, 18u, 20u };
    size_t i;
    for (i = 0; i < sizeof ours / sizeof ours[0]; ++i)
        if (type == ours[i]) return 1;
    return 0;
}

/* ---------------------------------------------------------------- コンボ（连続撃破）

 * 一手（`engine/versus/th19/04-manual-spec.md` §5 + 第 7 行）：
 *   「ザコ敵を倒すと爆発します…連爆に巻き込んだ敵や敵弾はコンボになります」
 *   ★「combo 满 100 → kind 9」（kind 9 = 霊力アイテム）。
 * ⇒ 攒满 `POFV_COMBO_MAX` 就掉一颗（返回值 = 满的次数，调用方据此撒道具）并保留余数。
 * ⚠ 增量值一手没逆到（同文档第 75 行：combo 的写点只有 2 处、都是清零）⇒ 见头文件的 ❓ 常量。*/
int32_t pofv_combo_add(pofv_state_t *st, int32_t n)
{
    int32_t full;
    if (!st || n <= 0) return 0;
    st->combo += n;
    full = st->combo / POFV_COMBO_MAX;
    if (full > 0) {
        st->combo -= full * POFV_COMBO_MAX;      /* 保留余数（不是清零，超出的部分不浪费）*/
        st->combo_drops += full;
    }
    return full;
}
