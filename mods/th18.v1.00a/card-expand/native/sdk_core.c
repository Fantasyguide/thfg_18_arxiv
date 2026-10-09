/* sdk_core.c —— 见 sdk_core.h。纯 C11。 */
#include <string.h>
#include "sdk_core.h"
#include "cards/hud_dim.h"   /* hud_card_shelf_x0()：让位算式（一手几何在 cards/hud_dim.h）*/

/* ---- 注册表 ---- */
static const ce_behavior_t *s_beh[CE_SDK_MAX_BEHAVIORS];
static const ce_behavior_t *s_beh_by_id[CE_SDK_CARD_ID_MAX + 1];
static unsigned s_nbeh;
static uint32_t s_reg_error_id;
static int s_reg_error_reason;

static int registration_fail(uint32_t id, int reason)
{
    if (s_reg_error_reason == CE_SDK_REG_NONE) {
        s_reg_error_id = id;
        s_reg_error_reason = reason;
    }
    return 0;
}

int ce_sdk_register(const ce_behavior_t *b)
{
    if (!b || b->id < CE_SDK_CARD_ID_MIN || b->id > CE_SDK_CARD_ID_MAX)
        return registration_fail(b ? b->id : 0, CE_SDK_REG_ID_RANGE);
    if (s_beh_by_id[b->id]) return registration_fail(b->id, CE_SDK_REG_DUPLICATE);
    if (s_nbeh >= CE_SDK_MAX_BEHAVIORS) return registration_fail(b->id, CE_SDK_REG_CAPACITY);
    s_beh[s_nbeh++] = b;
    s_beh_by_id[b->id] = b;
    return 1;
}

int ce_sdk_registration_error(uint32_t *id, int *reason)
{
    if (id) *id = s_reg_error_id;
    if (reason) *reason = s_reg_error_reason;
    return s_reg_error_reason != CE_SDK_REG_NONE;
}

unsigned ce_sdk_behavior_count(void) { return s_nbeh; }
const ce_behavior_t *ce_sdk_behavior_at(unsigned i) { return i < s_nbeh ? s_beh[i] : (const ce_behavior_t *)0; }

const ce_behavior_t *ce_sdk_find(uint32_t id)
{
    return id <= CE_SDK_CARD_ID_MAX ? s_beh_by_id[id] : (const ce_behavior_t *)0;
}

void ce_sdk_reset_for_test(void)
{
    ce_card_flag_reset();
    memset(s_beh, 0, sizeof s_beh);
    memset(s_beh_by_id, 0, sizeof s_beh_by_id);
    s_nbeh = 0;
    s_reg_error_id = 0;
    s_reg_error_reason = CE_SDK_REG_NONE;
}

int ce_sdk_bind_check(const uint32_t *json_ids, unsigned n, uint32_t *bad_id, unsigned *unbound)
{
    for (unsigned i = 0; i < s_nbeh; ++i) {
        int found = 0;
        for (unsigned k = 0; k < n && !found; ++k) found = json_ids[k] == s_beh[i]->id;
        if (!found) { if (bad_id) *bad_id = s_beh[i]->id; return 0; }
    }
    unsigned u = 0;
    for (unsigned k = 0; k < n; ++k) if (!ce_sdk_find(json_ids[k])) ++u;
    if (unbound) *unbound = u;
    return 1;
}

int ce_sdk_contract_check(const uint32_t *json_ids, const uint32_t *categories, unsigned n,
                          uint32_t *bad_id, int *reason)
{
    for (unsigned i = 0; i < n; ++i) {
        const ce_behavior_t *b = ce_sdk_find(json_ids[i]);
        if (categories[i] == 0 && (!b || !b->is_active)) {
            if (bad_id) *bad_id = json_ids[i];
            if (reason) *reason = CE_SDK_CONTRACT_ACTIVE_BEHAVIOR_REQUIRED;
            return 0;
        }
        if (b && b->is_active && categories[i] != 0) {
            if (bad_id) *bad_id = json_ids[i];
            if (reason) *reason = CE_SDK_CONTRACT_ACTIVE_CATEGORY_REQUIRED;
            return 0;
        }
    }
    if (reason) *reason = CE_SDK_CONTRACT_NONE;
    return 1;
}

/* ---- 私有状态槽 ---- */
static const void *s_key[CE_STATE_SLOTS];
static unsigned char s_mem[CE_STATE_SLOTS][CE_STATE_BYTES];
static unsigned s_used;

static int slot_of(const void *key)
{
    for (unsigned i = 0; i < CE_STATE_SLOTS; ++i) if (s_key[i] == key) return (int)i;
    return -1;
}

void *ce_state_alloc(const void *key, unsigned size)
{
    if (!key || size > CE_STATE_BYTES) return (void *)0;
    int i = slot_of(key);
    if (i >= 0) return s_mem[i];
    for (unsigned k = 0; k < CE_STATE_SLOTS; ++k)
        if (!s_key[k]) {
            s_key[k] = key;
            memset(s_mem[k], 0, CE_STATE_BYTES);
            ++s_used;
            return s_mem[k];
        }
    return (void *)0;
}

void *ce_state_get(const void *key)
{
    int i = key ? slot_of(key) : -1;
    return i >= 0 ? s_mem[i] : (void *)0;
}

void ce_state_free(const void *key)
{
    int i = key ? slot_of(key) : -1;
    if (i >= 0) { s_key[i] = (const void *)0; --s_used; }
}

unsigned ce_state_in_use(void) { return s_used; }

void *ce_state_user(const void *key, unsigned size)
{
    if (size > CE_STATE_BYTES - CE_STATE_RESERVED) return (void *)0;
    unsigned char *p = ce_state_alloc(key, CE_STATE_RESERVED + size);
    return p ? p + CE_STATE_RESERVED : (void *)0;
}


/* ── 集卡判定 ──────────────────────────────────────────────────────── */
int ce_royal_flush_ready(const int32_t *owned, uint32_t self_id, const uint32_t *set, unsigned n)
{
    int self_in = 0;
    for (unsigned i = 0; i < n; ++i) {
        if (set[i] == self_id) { self_in = 1; continue; }
        if (set[i] >= 255 || owned[set[i]] == 0) return 0;
    }
    return self_in;
}

int ce_judgment_cost(int lives)
{
    return lives <= 0 ? 0 : (lives + 1) / 2;
}

int ce_interest(int money, int percent)
{
    if (money <= 0 || percent <= 0) return 0;
    return (int)(((int64_t)money * percent) / 100);
}

int ce_pick_index(uint32_t rnd, unsigned n)
{
    return n == 0 ? -1 : (int)(rnd % n);
}


/* ---- 自机弹伤害倍率层 ---- */

static int s_dmg_permille[CE_SDK_CARD_ID_MAX + 1];
static int s_dmg_agg = CE_DMG_UNIT;
static int s_dmg_applier = -1;

static void dmg_recompute(void)
{
    long long agg = CE_DMG_UNIT;
    s_dmg_applier = -1;
    for (int id = 0; id <= CE_SDK_CARD_ID_MAX; ++id) {
        if (s_dmg_permille[id] <= 0) continue;
        if (s_dmg_applier < 0) s_dmg_applier = id;          /* id 最小的那张落笔，稳定且与链表顺序无关 */
        agg = agg * s_dmg_permille[id] / CE_DMG_UNIT;
    }
    s_dmg_agg = (int)agg;
}

void ce_dmg_reset(void)
{
    for (int id = 0; id <= CE_SDK_CARD_ID_MAX; ++id) s_dmg_permille[id] = 0;
    s_dmg_agg = CE_DMG_UNIT;
    s_dmg_applier = -1;
}

void ce_dmg_set(uint32_t id, int permille)
{
    if (id > (uint32_t)CE_SDK_CARD_ID_MAX) return;
    if (permille < 0) permille = 0;
    s_dmg_permille[id] = permille;
    dmg_recompute();
}

int ce_dmg_aggregate(void) { return s_dmg_agg; }
int ce_dmg_applier(void)   { return s_dmg_applier; }

int ce_dmg_scale(int damage)
{
    if (s_dmg_agg == CE_DMG_UNIT) return damage;
    return (int)((long long)damage * s_dmg_agg / CE_DMG_UNIT);
}


/* ---- 无敌帧声明层（取最大值）---- */
static int s_invuln[CE_SDK_CARD_ID_MAX + 1];
static int s_invuln_frames;
static int s_invuln_applier = -1;

static void invuln_recompute(void)
{
    int best = 0, who = -1;
    for (int id = 0; id <= CE_SDK_CARD_ID_MAX; ++id) {
        if (s_invuln[id] <= 0) continue;
        if (s_invuln[id] > best) { best = s_invuln[id]; who = id; }
        else if (s_invuln[id] == best && who < 0) who = id;
    }
    s_invuln_frames = best;
    s_invuln_applier = who;
}

void ce_invuln_reset(void)
{
    for (int id = 0; id <= CE_SDK_CARD_ID_MAX; ++id) s_invuln[id] = 0;
    s_invuln_frames = 0;
    s_invuln_applier = -1;
}

void ce_invuln_request(uint32_t id, int frames)
{
    if (id > (uint32_t)CE_SDK_CARD_ID_MAX) return;
    if (frames < 0) frames = 0;
    s_invuln[id] = frames;
    invuln_recompute();
}

int ce_invuln_frames(void)  { return s_invuln_frames; }
int ce_invuln_applier(void) { return s_invuln_applier; }


/* ---- ★ PoC 高度层（收点线；语义 / 一手证据全在 sdk_core.h 的块注释里）----
 * 存储用 **float 的位型**（`uint32_t`）：聚合与夹取在整数域做（非负有限 float 的位型序 = 数值序），
 * 于是本文件里**一条浮点指令都没有** —— 这不是洁癖：i686 的 float 返回值走 x87 st(0)，
 * 一旦这里出现「返回 float」的函数，`make dllx87` 的「我们的 object 里 0 条 x87」不变式当场变红。*/
static uint32_t s_poc[CE_SDK_CARD_ID_MAX + 1];   /* 各卡声明的**绝对**高度（位型；0 = 没声明）*/
static uint8_t  s_poc_mask[CE_SDK_CARD_ID_MAX + 1];  /* ★「遮」声明（覆盖位；见 sdk_core.h 那一节）*/
static int      s_poc_winner = -1;               /* 线最低（y 最大）的那张卡；并列取 id 最小 */
static int      s_poc_any;                       /* 有人在声明（落笔闸门）*/
static uint32_t s_poc_target;                    /* 聚合 + 夹取后的落笔值 */
static int      s_poc_base_valid;                /* 记着「我们动手之前」的字段原值 */
static uint32_t s_poc_base;                      /* 那个原值（删卡时写回）*/
static uint32_t s_poc_written;                   /* 我们上一次写进去的值（0 = 还没写过）*/
static int      s_poc_mask_any;                  /* ★ 有人在「遮」*/
static int      s_poc_mask_owner = -1;           /* ★ 「遮」声明者 id（最小）；无人 = -1 */

/* 合法声明 = 非负、有限（bit31 = 0 且指数域 != 0xFF）、非 0。*/
static int poc_ok(uint32_t bits)
{
    if (bits == 0u) return 0;                     /* 0.0f = 退出声明（惯例，同 ce_dmg_set(id,0)）*/
    if (bits & 0x80000000u) return 0;             /* 负 */
    if ((bits & 0x7f800000u) == 0x7f800000u) return 0;   /* Inf / NaN */
    return 1;
}

static void poc_recompute(void)
{
    int best = -1;

    s_poc_any = 0;
    s_poc_winner = -1;
    s_poc_target = 0;
    s_poc_mask_any = 0;
    s_poc_mask_owner = -1;
    for (int id = 0; id <= CE_SDK_CARD_ID_MAX; ++id) {
        if (s_poc_mask[id]) {
            s_poc_mask_any = 1;
            if (s_poc_mask_owner < 0) s_poc_mask_owner = id;
        }
        if (s_poc[id] == 0u) continue;
        s_poc_any = 1;
        if (best < 0 || s_poc[id] > s_poc_target) { s_poc_target = s_poc[id]; best = id; }
    }
    /* 夹取：上限 448（游玩区高）。下限 **不用判** —— `CE_POC_HEIGHT_MIN_BITS` = 0
     * 同时是「退出声明」的哨兵（见 `poc_ok()`），所以进到这里的一定 > 0。*/
    if (s_poc_target > CE_POC_HEIGHT_MAX_BITS) s_poc_target = CE_POC_HEIGHT_MAX_BITS;
    s_poc_winner = best;
}

void ce_poc_reset(void)
{
    for (int id = 0; id <= CE_SDK_CARD_ID_MAX; ++id) { s_poc[id] = 0u; s_poc_mask[id] = 0u; }
    s_poc_winner = -1;
    s_poc_any = 0;
    s_poc_target = 0;
    s_poc_mask_any = 0;
    s_poc_mask_owner = -1;
    s_poc_base_valid = 0;
    s_poc_base = 0;
    s_poc_written = 0;
}

int ce_poc_declare(uint32_t id, uint32_t height_bits)
{
    if (id > (uint32_t)CE_SDK_CARD_ID_MAX) return 0;
    /* 高度声明与「遮」声明互斥：一张卡要么报绝对高度、要么要求遮（同 id 后写的那条生效）。*/
    s_poc_mask[id] = 0u;
    if (!poc_ok(height_bits)) { s_poc[id] = 0u; poc_recompute(); return 0; }
    s_poc[id] = height_bits;
    poc_recompute();
    return 1;
}

void ce_poc_drop(uint32_t id)
{
    if (id > (uint32_t)CE_SDK_CARD_ID_MAX) return;
    s_poc[id] = 0u;
    s_poc_mask[id] = 0u;                       /* ★ 删卡/退出：两个都撤 */
    poc_recompute();
}

int ce_poc_has_declaration(void) { return s_poc_any || s_poc_mask_any; }   /* ★ 含「遮」 */
int ce_poc_winner(void)          { return s_poc_winner; }

uint32_t ce_poc_target_bits(void)
{
    uint32_t t = s_poc_target;

    if (t > CE_POC_HEIGHT_MAX_BITS) t = CE_POC_HEIGHT_MAX_BITS;
    return t;                                     /* 无声明 ⇒ 0（调用方必须先用 has_declaration 把门）*/
}

uint32_t ce_poc_effective_bits(uint32_t engine_bits)
{
    return s_poc_any ? ce_poc_target_bits() : engine_bits;
}

int ce_poc_base(uint32_t *out_bits)
{
    if (!s_poc_base_valid) return 0;
    if (out_bits) *out_bits = s_poc_base;
    return 1;
}

/* ★ 落笔的**记账**（`ce_poc_frame` 与 `ce_poc_frame_masked` 共用；`want` 已经是本帧的目标值）：
 *   ① 第一次落笔 ⇒ 把「动手之前」的字段值记成基准；
 *   ② 之后若发现字段既不是我们上次写的、也不是本帧目标（= 别人改过，典型是原生卡 22 / `Player__reset`）
 *      ⇒ 基准跟着刷新（删卡时写回的是**别人的**值，不是写死 128）；
 *   ③ 同值也照写（一个字的内存写，幂等）。*/
static int poc_commit(uint32_t engine_bits, uint32_t want, uint32_t *out_bits)
{
    if (!s_poc_base_valid) {
        s_poc_base = engine_bits;
        s_poc_base_valid = 1;
    } else if (engine_bits != s_poc_written && engine_bits != want) {
        s_poc_base = engine_bits;
    }
    s_poc_written = want;
    if (out_bits) *out_bits = want;
    return 1;
}

int ce_poc_frame(uint32_t engine_bits, uint32_t *out_bits)
{
    if (s_poc_any) return poc_commit(engine_bits, ce_poc_target_bits(), out_bits);

    s_poc_written = 0;
    if (s_poc_base_valid) {                       /* 声明全撤：写回基准（一次），然后忘掉它 */
        uint32_t back = s_poc_base;

        s_poc_base_valid = 0;
        if (out_bits) *out_bits = back;
        return 1;
    }
    return 0;
}

int ce_poc_drop_take_base(uint32_t id, uint32_t *out_bits)
{
    ce_poc_drop(id);
    if (s_poc_any || s_poc_mask_any || !s_poc_base_valid) return 0;
    s_poc_base_valid = 0;                         /* 立刻写回基准（删卡在商店里，帧钩子不跑）*/
    s_poc_written = 0;
    if (out_bits) *out_bits = s_poc_base;
    return 1;
}


/* ---- ★★ 「遮」声明 / 语义高度（2026-09-20；语义与一手证据全在 sdk_core.h 那一节）----
 * 这一节的关键是**两条判据的严格/非严格之差**（一手）：
 *   过线收点（真空态）的门 `th18:0x445bb9` / `0x446003` = `ja`（**严格** `线 > y`）；
 *   线上满额的门 `th18:0x446995` / `0x446c01` = `jae`（**非严格** `线 ≥ y`）。
 * ⇒ 落笔值取 `min(语义高度, y)` 时，「等于 y」这一档同时满足「不吸」与「满额」——
 *   这不是巧合，是两条判据的形态差，是本实现（以及 §7-A 的闸）成立的支点。*/
int ce_poc_mask_declare(uint32_t id, int on)
{
    if (id > (uint32_t)CE_SDK_CARD_ID_MAX) return 0;
    s_poc_mask[id] = on ? 1u : 0u;
    if (on) s_poc[id] = 0u;                       /* 互斥：报了遮就不再有绝对高度声明 */
    poc_recompute();
    return 1;
}

int ce_poc_mask_active(void) { return s_poc_mask_any; }
int ce_poc_mask_owner(void)  { return s_poc_mask_owner; }

uint32_t ce_poc_semantic_bits(uint32_t engine_bits)
{
    if (s_poc_any) return ce_poc_target_bits();   /* 别家声明（不含「遮」）取最大 */
    if (s_poc_base_valid) return s_poc_base;      /* 我们动手之前的字段值（原生卡 22 的 224 / 128）*/
    return engine_bits;                           /* 从没动过手 ⇒ 字段实读值 */
}

int ce_poc_frame_masked(uint32_t engine_bits, uint32_t ceiling_bits, uint32_t *out_bits)
{
    uint32_t want;

    if (!s_poc_mask_any) return ce_poc_frame(engine_bits, out_bits);
    /* 语义高度：别家声明 → 基准 → 本帧字段实读值。落笔值 = min(它, 自机 y)。*/
    want = ce_poc_semantic_bits(engine_bits);
    if (ceiling_bits == 0u || ceiling_bits > CE_POC_HEIGHT_MAX_BITS) {
        /* 读不到自机 y（空指针 / 越界）⇒ 收敛到「极小正数」：线跑到屏幕最上沿之外
         * （**永不过线** = 106 的原设计；代价是折额走到最坏档，由 `ce_poc_fold_ratio_permille()` 兜底）。*/
        want = CE_POC_TINY_BITS;
    } else if (want > ceiling_bits) {
        want = ceiling_bits;
    }
    if (want < CE_POC_TINY_BITS) want = CE_POC_TINY_BITS;   /* 0.0f 是哨兵；负值位型序会翻 ⇒ 只能取极小正数 */
    if (want > CE_POC_HEIGHT_MAX_BITS) want = CE_POC_HEIGHT_MAX_BITS;
    return poc_commit(engine_bits, want, out_bits);
}


/* ---- ★ 线显示（`Gui+0x194` bit `0x2000`）：声明层 + 基准记账 ----
 * 一手：唯一原生写者 = 原生卡 22 的 `on_load` `0x40c288`（`or`，**只置不清**），唯一读点 = `0x43af0b`。
 * ⇒「有人 hide」时我们**清**它（新增写点，登记 AUDIT §BE）；撤完按基准写回（原生那位自己不会再置回来）。*/
static uint8_t s_poc_hide[CE_SDK_CARD_ID_MAX + 1];
static int     s_poc_hide_any;
static int     s_poc_hide_owner = -1;
static int     s_poc_line_base_valid;            /* 记着「我们清之前」那一位是什么 */
static int     s_poc_line_base;
static int     s_poc_line_cleared;               /* 我们已经清过一次（= 欠一次写回）*/

static void poc_line_recompute(void)
{
    s_poc_hide_any = 0;
    s_poc_hide_owner = -1;
    for (int id = 0; id <= CE_SDK_CARD_ID_MAX; ++id) {
        if (!s_poc_hide[id]) continue;
        s_poc_hide_any = 1;
        if (s_poc_hide_owner < 0) s_poc_hide_owner = id;
    }
}

void ce_poc_line_hide_declare(uint32_t id, int on)
{
    if (id > (uint32_t)CE_SDK_CARD_ID_MAX) return;
    s_poc_hide[id] = on ? 1u : 0u;
    poc_line_recompute();
}

int ce_poc_line_hidden(void)     { return s_poc_hide_any; }
int ce_poc_line_hide_owner(void) { return s_poc_hide_owner; }

int ce_poc_line_apply(int bit_now, int decl_exists, int *out_bit)
{
    int now = bit_now ? 1 : 0;

    if (s_poc_hide_any) {
        if (!s_poc_line_cleared) {                /* 第一次清：把「动手之前」的那一位记成基准 */
            s_poc_line_base = now;
            s_poc_line_base_valid = 1;
            s_poc_line_cleared = 1;
        }
        if (now == 0) return 0;                   /* 已经是清的：一个字都不写 */
        if (out_bit) *out_bit = 0;
        return 1;
    }
    if (s_poc_line_cleared) {                     /* hide 撤完：按基准写回一次 */
        int back = s_poc_line_base_valid ? s_poc_line_base : 0;

        s_poc_line_cleared = 0;
        s_poc_line_base_valid = 0;
        if (now == back) return 0;
        if (out_bit) *out_bit = back;
        return 1;
    }
    if (decl_exists && now == 0) {                /* 旧行为：有人声明高度 ⇒ 照原生卡 22 把那条线打开 */
        if (out_bit) *out_bit = 1;
        return 1;
    }
    return 0;
}

int ce_poc_line_release(uint32_t id, int *out_bit)
{
    if (id > (uint32_t)CE_SDK_CARD_ID_MAX) return 0;
    s_poc_hide[id] = 0u;
    poc_line_recompute();
    if (s_poc_hide_any || !s_poc_line_cleared) return 0;
    s_poc_line_cleared = 0;                       /* 删卡在商店里 ⇒ 帧钩子不跑，这里立刻写回 */
    {
        int back = s_poc_line_base_valid ? s_poc_line_base : 0;

        s_poc_line_base_valid = 0;
        if (out_bit) *out_bit = back;
        return 1;
    }
}

void ce_poc_line_reset(void)
{
    for (int id = 0; id <= CE_SDK_CARD_ID_MAX; ++id) s_poc_hide[id] = 0u;
    s_poc_hide_any = 0;
    s_poc_hide_owner = -1;
    s_poc_line_base_valid = 0;
    s_poc_line_base = 0;
    s_poc_line_cleared = 0;
}


/* ---- ★ 引擎折额的兜底补偿（倍率千分点；1000 = 不动）----
 * 一手（`0x446c17`–`0x446cf3` 与 `0x4469a7`–`0x446a41` 两段同形，逐指令核过）：
 *   `d = ⌊y − 字段⌋`（`cvttss2si` = 向零截断）⇒ 系数 `c(d) = 0.9·(450 − d)/450`（`d ≥ 450` 时引擎自己走
 *   「下限 +1 分」那一档）；`d ≤ 0`（或真空飞行 `state == 3`）⇒ 满额支（系数 1）。
 * 本函数给出 `c(d_语义) / c(d_实际)`（同一族里 `0.9` 约掉 ⇒ 只剩两个 `450 − d` 的比）。*/
static int poc_coeff_permille(int32_t d_px)
{
    if (d_px <= 0) return 1000;                   /* 满额支 */
    if (d_px >= 450) return 0;                    /* 引擎的下限档（系数被顶到 0）*/
    return 900 * (450 - d_px) / 450;
}

/* Q16 → px（**向零截断**，与 `cvttss2si` 同向）；`d = y − poc`。*/
static int32_t poc_d_px(int32_t y_q16, int32_t poc_q16)
{
    int32_t dy = (int32_t)(((int64_t)y_q16 - (int64_t)poc_q16) / 65536);

    return dy;
}

int ce_poc_fold_ratio_permille(int32_t y_q16, int32_t poc_field_q16, int32_t poc_sem_q16, int vacuum)
{
    int c_field, c_sem;
    int32_t ratio;

    if (vacuum) return 1000;                      /* 真空飞行：两条路都是满额支 ⇒ 无可补偿 */
    c_field = poc_coeff_permille(poc_d_px(y_q16, poc_field_q16));
    c_sem   = poc_coeff_permille(poc_d_px(y_q16, poc_sem_q16));
    if (c_field <= 0) return 1000;                /* 实际系数已经到底：掰不回来（宁可少给也不放大 32 倍）*/
    if (c_sem == c_field) return 1000;            /* 常见情形：落笔值就是语义值 ⇒ 一个字节都不动 */
    ratio = (int32_t)((int64_t)c_sem * 1000 / c_field);
    if (ratio <= 1000) return 1000;               /* 只往上补（往下压会让"线下折额"变小，语义上没有依据）*/
    if (ratio > 4000) return 1000;                /* 越界 = 数值不可信 ⇒ 不动 */
    return (int)ratio;
}


/* ---- 「中弹无伤」闸门（任一声明即生效）---- */
static uint8_t s_no_hit[CE_SDK_CARD_ID_MAX + 1];
static int     s_no_hit_active;
static int     s_no_hit_owner = -1;

static void no_hit_recompute(void)
{
    int any = 0, who = -1;

    for (int id = 0; id <= CE_SDK_CARD_ID_MAX; ++id) {
        if (!s_no_hit[id]) continue;
        any = 1;
        if (who < 0) who = id;
    }
    s_no_hit_active = any;
    s_no_hit_owner  = who;
}

void ce_no_hit_declare(uint32_t id, int on)
{
    if (id > (uint32_t)CE_SDK_CARD_ID_MAX) return;
    s_no_hit[id] = on ? 1u : 0u;
    no_hit_recompute();
}

int ce_no_hit_active(void) { return s_no_hit_active; }
int ce_no_hit_owner(void)  { return s_no_hit_owner; }

void ce_no_hit_reset(void)
{
    for (int id = 0; id <= CE_SDK_CARD_ID_MAX; ++id) s_no_hit[id] = 0;
    no_hit_recompute();
}

/* ---- ★★ 「未满 P」声明层（见 `sdk_core.h`）：106 每帧声明 `power < MAX_POWER`。
 *   消费者 = ① 过线收点的两条 `ja`（`BP_ce_poc_gate_a/b`，`th18:0x445bb9` / `0x446003`）
 *             ② 111 的信号推进（`faith_signal_step_gated(..., allow_line_topup)` 关掉「线上补满」那一支）。*/
static uint8_t s_full_p_gate[CE_SDK_CARD_ID_MAX + 1];
static int     s_full_p_gate_owner = -1;

/* 重算：**任一声明即生效**（active），owner = id 最小者（诊断用）。
 * ⚠ 必须按卡记账（照 `ce_no_hit` 的形状）：只存一个 owner 的话，先声明的那张卡一撤
 *   就会把**别人的声明**一起抹掉（111 与 106 同时声明时的静默失效）。*/
static void full_p_gate_recompute(void)
{
    int id;
    s_full_p_gate_owner = -1;
    for (id = 0; id <= CE_SDK_CARD_ID_MAX; ++id) {
        if (s_full_p_gate[id]) { s_full_p_gate_owner = id; break; }
    }
}

void ce_full_p_gate_declare(uint32_t id, int on)
{
    if (id > (uint32_t)CE_SDK_CARD_ID_MAX) return;
    s_full_p_gate[id] = on ? 1u : 0u;
    full_p_gate_recompute();
}

int  ce_full_p_gate_active(void) { return s_full_p_gate_owner >= 0; }
int  ce_full_p_gate_owner(void)  { return s_full_p_gate_owner; }
void ce_full_p_gate_reset(void)
{
    int id;
    for (id = 0; id <= CE_SDK_CARD_ID_MAX; ++id) s_full_p_gate[id] = 0;
    full_p_gate_recompute();
}

/* ---- ★ SDK 帧号（见 sdk_core.h）：帧点 `BP_ce_item_frame` 每帧 bump 一次，
 *      给「共用模块每帧只推进一次」的守卫用（`faith_pool_tick()` 等）。---- */
static uint32_t s_sdk_frame;
void     ce_sdk_frame_bump(void) { ++s_sdk_frame; }
uint32_t ce_sdk_frame(void)      { return s_sdk_frame; }

/* ---- ★★ 「隐藏原生子机」声明层（`sdk_core.h` 那一节的落地；纯逻辑，不碰引擎）----
 * 语义只有两条：**粘性**（1 = 生效到改口为止）+ **机体门**（只在声明指定的机体上生效）。
 * 机体门放在这里而不是留给卡，是因为**换机体发生在商店 / 编成里**（那一刻卡 tick 冻着），
 * 卡来不及改口；而断点 handler 随时会跑 ⇒ 它必须自己按「当前机体」判。*/
static uint8_t s_natopt_hide[CE_SDK_CARD_ID_MAX + 1];
static int     s_natopt_char[CE_SDK_CARD_ID_MAX + 1];   /* 每条声明生效的机体号 */
static int     s_natopt_owner = -1;                     /* 诊断：id 最小的声明者 */

void ce_native_option_hide_declare(uint32_t id, int on, int character)
{
    if (id > (uint32_t)CE_SDK_CARD_ID_MAX) return;
    s_natopt_hide[id] = on ? 1u : 0u;
    s_natopt_char[id] = character;
    s_natopt_owner = -1;
    for (int i = 0; i <= CE_SDK_CARD_ID_MAX; ++i)
        if (s_natopt_hide[i]) { s_natopt_owner = i; break; }
}

/* ★★★ 2026-09-23（108 子机修法第 1 步）：**「隐藏，但请求保留记录」**声明（照 `s_natopt_hide` 那一对的写法）。
 *   语义：有卡这么声明时，构建断点**不动手**（让引擎照常建记录/坐标），而**显示**由那张卡自己的
 *   每帧 `ce_native_option_sweep_keep_state()` 收掉 ⇒ 两家的需求同时成立（详见 `HANDOFF-108.md` §4）。*/
static uint8_t s_natopt_records[CE_SDK_CARD_ID_MAX + 1];

void ce_native_option_keep_records(uint32_t id, int on)
{
    if (id > (uint32_t)CE_SDK_CARD_ID_MAX) return;
    s_natopt_records[id] = on ? 1u : 0u;
}

int ce_native_option_records_wanted(void)
{
    for (int i = 0; i <= CE_SDK_CARD_ID_MAX; ++i) if (s_natopt_records[i]) return 1;
    return 0;
}

int ce_native_option_hidden(int character)
{
    for (int i = 0; i <= CE_SDK_CARD_ID_MAX; ++i)
        if (s_natopt_hide[i] && s_natopt_char[i] == character) return 1;
    return 0;
}

int ce_native_option_hide_owner(void) { return s_natopt_owner; }

void ce_native_option_hide_reset(void)
{
    for (int i = 0; i <= CE_SDK_CARD_ID_MAX; ++i) { s_natopt_hide[i] = 0; s_natopt_char[i] = 0; }
    for (int i = 0; i <= CE_SDK_CARD_ID_MAX; ++i) s_natopt_records[i] = 0;
    s_natopt_owner = -1;
}

/* ============================ ★ 计分层（2026-09-13 按用户公式重做）============================
 * 规格 / 推导 / 一手证据：`../SCORE-LAYER-REWORK.md`。一句话：
 *     G = G_0 + Σ未耦合 + coupling‰ × Σ耦合 / 1000 ;  落笔 = 站点 `0x446bf9` 的 `ecx = 1000 × G`
 * 单位约定（**三个，别混**；2026-09-13 第二次修订时逐条对着 exe 核过）：
 *   · 「点」= 玩家看到的分数（SCORE 增量、卡声明值、G_0、本层对外返回的 total 都是点）。
 *   · 「身价（引擎单位）」= 点 × 10 = 引擎的弹窗值 = 站点 `ecx` ÷ 100：满额支 `esi = 10×floor(ecx/1000)`，
 *     它既是弹窗参数、也是 `SCORE += esi/10` 的被除数（一手 `0x446cb4`–`0x446d22`）。
 *     `ce_score_bridge()` 收的就是它；`0x4ccd24` 存的是**它的 ×10**（「×100 定点」这个名字指的是
 *     「点 × 100」，即身价 ×10 —— 别被名字骗了）。
 *   · 「站点 ecx」= 1/1000 点（`0x446b53`–`0x446bf1` 钳的那一格）：`ecx = 1000 × 点 = 10 × 身价`。
 *     SDK 落笔写它（`BP_ce_item_score_in`）。
 *   ⇒ 上下限常数（`CE_SCORE_BASE_MIN_PTS` / `CE_SCORE_BASE_MAX_*`）是**身价**单位，
 *     所以 `ce_score_base_points()` 先钳身价、再 ÷10 得点（引擎自己已钳过 ⇒ 现场钳恒等）。
 * 粘性：声明写一次管到改口为止（站点可能落在两帧之间；`ce_layer_drop_card` 负责删卡时退场）。*/
static int32_t s_score_delta[CE_SDK_CARD_ID_MAX + 1];      /* 未耦合加数（点）*/
static int32_t s_score_coupled[CE_SDK_CARD_ID_MAX + 1];    /* 耦合加数（点）*/
static int32_t s_score_mul[CE_SDK_CARD_ID_MAX + 1];        /* 信号强度（千分点）*/
static int32_t s_score_legacy[CE_SDK_CARD_ID_MAX + 1];     /* 旧接口声明的值（点；-1 = 未声明）*/

static int32_t s_score_raw;          /* 桥值 = 引擎的身价（= 点 × 10；站点 ecx ÷ 100）*/
static int     s_score_diff;         /* 难度 0..4 */
static int     s_score_coeff = CE_SCORE_COUPLING_DEFAULT;   /* 引擎已算好的高度系数（千分点，只读）*/

/* ---- ★★ 共享 PIV 池 + 擦弹族（用户 2026-09-18 定案；口径见 `sdk_core.h` 的同名注释块）----
 * 池 = 「额外」最大得点（点），初值 0；六作共用一块；**每卡各报一份读数**（= 各系统各报一份）。*/
static int32_t s_piv_pool;                                    /* 池的当前值（额外量，点）*/
static int32_t s_piv_cap_p1[CE_SDK_CARD_ID_MAX + 1];          /* 可写上限 **+1**（0 = 不在场 ——
                                                               *   正好等于静态零初始化值，省一次初始化）*/
static int32_t s_piv_wrote[CE_SDK_CARD_ID_MAX + 1];           /* 本卡**已写进池**的量（写入闸的账）*/
static uint32_t s_graze_present[CE_SDK_CARD_ID_MAX + 1];      /* 擦弹族在场标记（1/0）*/
static unsigned s_graze_present_n;
static uint32_t s_graze_total;                                /* 桥进来的本局擦弹总数（0x4ccd0c）*/

/* ---- 计分层换版（每项走自己原作的曲线；用户 2026-09-18）---- */
static uint8_t s_score_family[CE_SDK_CARD_ID_MAX + 1];       /* 各卡声明的系数族（0 = NONE）*/
static int32_t s_coeff0_mille;                               /* 本颗的 coeff_0（千分点）；0 = 未知 */
static int32_t s_geom_d_px;                                  /* 自机 y 的线下深度 d（px）；线上 = 0 */
static int32_t s_geom_d_item_px;                             /* **道具 y** 的线下深度（th06/07/08/th10 吃这个）*/
static int32_t s_family10_faith;                             /* th10 的显示信仰 F（110 填；0 = 未知）*/
static int     s_geom_valid;                                 /* 本颗填过几何没有（区分「深度 ≥ 450 ⇒ coeff_0 = 0」
                                                              *   与「没几何信息 ⇒ 回落到标记位」）*/

static int32_t s_score_delta_sum, s_score_coupled_sum, s_score_mul_sum;
static int32_t s_score_legacy_sum;
static int     s_score_delta_n, s_score_coupled_n, s_score_mul_n, s_score_legacy_n;
static int     s_score_applier_id = -1;      /* 声明者里 id 最小的（诊断 / HUD 归属；不再决定落笔）*/

/* 三个求和通道一起重算（O(n) 只在声明时发生；读值全是 O(1)）。*/
static void score_recompute(void)
{
    int32_t d = 0, c = 0, m = 0, l = 0;
    int nd = 0, nc = 0, nm = 0, nl = 0;
    int who = -1;

    for (int id = 0; id <= CE_SDK_CARD_ID_MAX; ++id) {
        if (s_score_delta[id])   { d += s_score_delta[id];   ++nd; if (who < 0) who = id; }
        if (s_score_coupled[id]) { c += s_score_coupled[id]; ++nc; if (who < 0) who = id; }
        if (s_score_mul[id])     { m += s_score_mul[id];     ++nm; }
        if (s_score_legacy[id] >= 0) {
            /* 旧语义：声明值 = 这张卡自己的得分；老落笔公式 = `*value = max(v,10) × 10`
             * ⇒ 折成「点」时先把 10 分下限补上，逐字节等价才有依据（单测钉死）。*/
            int32_t v = s_score_legacy[id];
            if (v < 10) v = 10;                      /* 负数也一并被这一行顶到 10（旧公式就是这么钳的）*/
            l += v; ++nl; if (who < 0) who = id;
        }
    }
    s_score_delta_sum   = d;  s_score_delta_n   = nd;
    s_score_coupled_sum = c;  s_score_coupled_n = nc;
    s_score_mul_sum     = m;  s_score_mul_n     = nm;
    s_score_legacy_sum  = l;  s_score_legacy_n  = nl;
    s_score_applier_id  = who;
}

void ce_score_reset(void)
{
    for (int id = 0; id <= CE_SDK_CARD_ID_MAX; ++id) {
        s_score_delta[id]  = 0;
        s_score_coupled[id] = 0;
        s_score_mul[id]    = 0;
        s_score_legacy[id] = -1;
    }
    s_score_raw = 0;
    s_score_diff = 0;
    s_score_coeff = CE_SCORE_COUPLING_DEFAULT;
    s_piv_pool = 0;
    s_graze_total = 0;
    s_coeff0_mille = 0;
    s_geom_d_px = 0;
    s_geom_valid = 0;
    s_geom_d_item_px = 0;
    s_family10_faith = 0;
    for (uint32_t fid = 0; fid <= (uint32_t)CE_SDK_CARD_ID_MAX; ++fid) s_score_family[fid] = CE_COEFF_FAMILY_NONE;
    score_recompute();
}

void ce_score_bridge(uint32_t raw_value, int difficulty)
{
    /* `raw_value` = **身价**（点 × 10）。SDK 在新站点 0x446bf9 用 `ecx / 100` 填
     * （`ecx` 单位 1/1000 点）⇒ `ce_score_base_points() == ecx/1000`（单测钉死）。*/
    s_score_raw = (int32_t)raw_value;
    s_score_diff = difficulty;
}

void ce_score_set_coeff(uint32_t permille)
{
    s_score_coeff = (int)permille;
}

/* ============================ 共享 PIV 池 + 擦弹族（用户 2026-09-18）============================ */

void ce_piv_reset(void)
{
    s_piv_pool = 0;
    /* 在场声明与上限**保留**（由 `ce_piv_claim` / `ce_layer_drop_card` 管）；池与各卡**已写账**清零。*/
    for (uint32_t id = 0; id <= (uint32_t)CE_SDK_CARD_ID_MAX; ++id) s_piv_wrote[id] = 0;
}

int ce_piv_claim(uint32_t id, int32_t writable_cap)
{
    if (id > (uint32_t)CE_SDK_CARD_ID_MAX) return 0;
    if (writable_cap < 0) writable_cap = 0;
    if (writable_cap > 0x7ffffffe) writable_cap = 0x7ffffffe;
    s_piv_cap_p1[id] = writable_cap + 1;
    return 1;
}

void ce_piv_release(uint32_t id)
{
    if (id > (uint32_t)CE_SDK_CARD_ID_MAX) return;
    {
        int was_present = (s_piv_cap_p1[id] > 0);
        s_piv_cap_p1[id] = 0;
        s_piv_wrote[id]  = 0;             /* 退场 = 那套系统撤了，写入账一并清 */
        /* ★★★ 2026-09-27（第三十九批 · owner「retry 不清空值的」）：**最后一个在场者退场 ⇒ 池归 0**。
         *   为什么需要：TH18 有**两条**重开路 ——
         *     · 「重开本局」（gamemode 10/11）走 recount ⇒ 持久卡只收虚表 `+0x4c`
         *       （`sdk.h` 的 `on_run_reset` 桩已在那里 `ce_piv_reset()`）；
         *     · 「全新一局」（gamemode 4/2 → `AbilityManager__reset_cards(mgr,1)` `0x407da0`）
         *       **删掉并重建所有卡对象**（走 `+0x50`），**不经过** `+0x4c` ⇒ 池会残留。
         *   这一条覆盖后者：全部池来源都退场 ⇒ 「额外量」清零（语义与 `sdk_core.h` 的
         *   「池里装的是额外最大得点、初值 0」一致）。副作用（登记 `TUNING.md`）：局中在商店里
         *   把**最后一个**池来源删掉时，池也会归 0。*/
        if (was_present) {
            int any = 0;
            for (uint32_t i = 0; i <= (uint32_t)CE_SDK_CARD_ID_MAX; ++i)
                if (s_piv_cap_p1[i] > 0) { any = 1; break; }
            if (!any) { ce_piv_reset(); }
        }
    }
}

void ce_piv_add(uint32_t id, int32_t points)
{
    /* ★ **写入闸在这一侧**（用户 2026-09-18：「大家都往 PIV 里面写相当于自动相加了，只是写的时候
     *   自己这边有一道最大写入的闸」）：本卡累计能写进去的 = `cap_i − init_i`（claim 时记好的可写上限），
     *   写满就再也写不进去；**池本身不做读值钳位**。
     * 未 claim 的卡不加（删卡之后不该继续涨）。池按 int32 安全上界夹，防溢出成负数。*/
    int64_t room, add, v;
    if (id > (uint32_t)CE_SDK_CARD_ID_MAX) return;
    if (s_piv_cap_p1[id] <= 0) return;
    if (points == 0) return;
    room = (int64_t)(s_piv_cap_p1[id] - 1) - (int64_t)s_piv_wrote[id];
    if (room <= 0) return;                            /* 闸到底：这张卡不再往池里加 */
    add = (points > 0) ? (int64_t)points : 0;         /* 负增量不当「倒扣池」，只当不写 */
    if (add > room) add = room;                       /* 只写它还能写的那部分 */
    v = (int64_t)s_piv_pool + add;
    if (v < 0) v = 0;
    if (v > 0x7fffffff) v = 0x7fffffff;
    s_piv_pool     = (int32_t)v;
    s_piv_wrote[id] = (int32_t)((int64_t)s_piv_wrote[id] + add);
}

/* ---- 112–117（th12–th17）的难度两表（**显示单位**；索引 = 难度 0..4 = E/N/H/L/EX）----
 * 一手（各作 exe 直读原始字节；逐条出处见 `engine/faith/th12..th17/01-*.md`）：
 *   th12: init {5000,10000,15000,20000,20000} / cap {100000,200000,300000,400000,500000}
 *   th13: init {5000,10000,15000,20000,20000} / cap {100000,250000,500000,500000,500000}
 *   th14: init {10000×5}                      / cap {100000,250000,500000,500000,500000}
 *   th15: init {10000×5}                      / cap {250000,500000,500000,500000,500000}
 *   th16: init {10000×5}                      / cap {500000×5}
 *   th17: init {10000×5}                      / cap {500000×5}
 * ⚠ 第 6 行（各作表里那一格）**不引用**（口径：难度只 E/N/H/L/EX，见 OVERVIEW 口径 1/2）。*/
static const int32_t k_piv_init[6][5] = {
    {  5000,  10000,  15000,  20000,  20000 },   /* 112 th12 */
    {  5000,  10000,  15000,  20000,  20000 },   /* 113 th13 */
    { 10000,  10000,  10000,  10000,  10000 },   /* 114 th14 */
    { 10000,  10000,  10000,  10000,  10000 },   /* 115 th15 */
    { 10000,  10000,  10000,  10000,  10000 },   /* 116 th16 */
    { 10000,  10000,  10000,  10000,  10000 },   /* 117 th17 */
};
static const int32_t k_piv_cap[6][5] = {
    { 100000, 200000, 300000, 400000, 500000 },  /* 112 th12 */
    { 100000, 250000, 500000, 500000, 500000 },  /* 113 th13 */
    { 100000, 250000, 500000, 500000, 500000 },  /* 114 th14 */
    { 250000, 500000, 500000, 500000, 500000 },  /* 115 th15 */
    { 500000, 500000, 500000, 500000, 500000 },  /* 116 th16 */
    { 500000, 500000, 500000, 500000, 500000 },  /* 117 th17 */
};

int32_t ce_piv_cap_for(uint32_t id, int difficulty)
{
    int row;
    if (id < 112u || id > 117u) return -1;           /* 表只覆盖 th12–th17 这六张 */
    if (difficulty < 0 || difficulty > 4) return -1; /* E/N/H/L/EX */
    row = (int)(id - 112u);
    return k_piv_cap[row][difficulty] - k_piv_init[row][difficulty];
}

int ce_piv_claim_auto(uint32_t id, int difficulty)
{
    int32_t extra = ce_piv_cap_for(id, difficulty);
    if (extra < 0) return 0;
    return ce_piv_claim(id, extra);
}

int32_t ce_piv_written(uint32_t id)
{
    if (id > (uint32_t)CE_SDK_CARD_ID_MAX) return 0;
    return s_piv_wrote[id];
}

int32_t ce_piv_room(uint32_t id)
{
    if (id > (uint32_t)CE_SDK_CARD_ID_MAX) return 0;
    if (s_piv_cap_p1[id] <= 0) return 0;
    return (s_piv_cap_p1[id] - 1) - s_piv_wrote[id];
}

int32_t ce_piv_pool(void) { return s_piv_pool; }

int32_t ce_piv_reading(uint32_t id)
{
    /* 旧名保留：现在每张卡的「读数」就是**池的值**（一份、自动相加）—— 钳位已在**写入侧**做完。
     * 本卡不在场 ⇒ 0。*/
    if (id > (uint32_t)CE_SDK_CARD_ID_MAX) return 0;
    if (s_piv_cap_p1[id] <= 0) return 0;
    return s_piv_pool;
}

int32_t ce_piv_cap_of(uint32_t id)
{
    if (id > (uint32_t)CE_SDK_CARD_ID_MAX) return -1;
    return s_piv_cap_p1[id] > 0 ? s_piv_cap_p1[id] - 1 : -1;
}

int32_t ce_piv_family_points(void)
{
    /* ★★ 族项 = **池的当前值**（用户 2026-09-18：「大家都往 PIV 里面写**相当于自动相加**了」）。
     *   ⇒ 多卡同装不是把池数 N 遍，而是「**多一套系统往同一块池里写**」⇒ 值更高、但只进公式一份。
     *   没有卡在场 ⇒ 0。*/
    return ce_piv_present_count() > 0 ? s_piv_pool : 0;
}

int ce_piv_present_count(void)
{
    int n = 0;
    for (uint32_t id = 0; id <= (uint32_t)CE_SDK_CARD_ID_MAX; ++id) if (s_piv_cap_p1[id] > 0) ++n;
    return n;
}

void ce_graze_bridge(uint32_t graze_total) { s_graze_total = graze_total; }
uint32_t ce_graze_total(void) { return s_graze_total; }

int ce_graze_family_claim(uint32_t id)
{
    if (id > (uint32_t)CE_SDK_CARD_ID_MAX) return 0;
    if (!s_graze_present[id]) { s_graze_present[id] = 1; ++s_graze_present_n; }
    return 1;
}

void ce_graze_family_release(uint32_t id)
{
    if (id > (uint32_t)CE_SDK_CARD_ID_MAX) return;
    if (s_graze_present[id]) { s_graze_present[id] = 0; if (s_graze_present_n) --s_graze_present_n; }
}

int ce_graze_family_present(void) { return s_graze_present_n > 0; }

int32_t ce_graze_points(void)
{
    /* ★ 一手（th12–th17 四作同形）：每 +1 擦弹 ⇒ 最大得点 +1 显示 ⇒ 本项 = 本局擦弹数（点）。
     * **只算一次**（用户口径：「别加一张卡擦弹数的作用就 +1 次」）⇒ 这是状态的函数，不是每卡一份。
     * 族内无卡 ⇒ 0（没有这一套系统时，擦弹不该给最大得点）。*/
    int64_t g;
    if (s_graze_present_n == 0) return 0;
    g = (int64_t)(int32_t)s_graze_total;         /* 引擎给的是 int32；负数按 0 夹 */
    if (g < 0) g = 0;
    if (g > 0x7fffffff) g = 0x7fffffff;
    return (int32_t)g;
}

/* ---- 计分层换版：几何桥 + 系数族 + 归一（用户 2026-09-18，路线 A）---- */

void ce_score_bridge_geom(int32_t y_q16, int32_t poc_q16)
{
    ce_score_bridge_geom2(y_q16, y_q16, poc_q16);
}

void ce_score_bridge_geom2(int32_t y_player_q16, int32_t y_item_q16, int32_t poc_q16)
{
    /* Q16 定点：`d_px = (poc − y) / 65536`（**截断**，与 0.75 族原作的 `(int)y − 线` 同向）。
     * 线上（y ≥ 线）⇒ coeff_0 = 1000‰、d = 0。*/
    int64_t dy  = (int64_t)poc_q16 - (int64_t)y_player_q16;
    int64_t dyi = (int64_t)poc_q16 - (int64_t)y_item_q16;
    s_geom_valid = 1;
    s_geom_d_item_px = dyi > 0 ? (int32_t)(dyi >> 16) : 0;
    if (dy <= 0) { s_coeff0_mille = 1000; s_geom_d_px = 0; return; }
    s_geom_d_px = (int32_t)(dy >> 16);
    if (s_geom_d_px >= 450) { s_coeff0_mille = 0; return; }        /* 引擎那一档只剩「下限 +1 分」 */
    s_coeff0_mille = (int32_t)((int64_t)900 * (450 - s_geom_d_px) / 450);
}

int ce_score_coeff0_permille(void)
{
    /* 有几何信息就用自算值；没有（单测 / 旧路径）就回落到 `ce_score_set_coeff()` 那个标记位：
     * 1000 = 线上（满额）、0 = 线下（值未知 ⇒ 不归一）。*/
    if (s_geom_valid) return s_coeff0_mille;      /* 几何在场：0 也是**真值**（深度 ≥ 450 那一档）*/
    return s_score_coeff;                         /* 没几何（单测 / 旧路径）：回落到标记位 */
}

int ce_score_ratio_permille(int family)
{
    int32_t c0 = ce_score_coeff0_permille();
    int64_t r;
    if (c0 <= 0) return 1000;                     /* 不知道该乘什么 ⇒ 不归一（= 老行为） */
    switch (family) {
    case CE_COEFF_FAMILY_075: {
        /* th12–th17：`0.75×(1−d/450)` 对 `0.9×(1−d/450)` ⇒ **线下**恒 5/6；
         * **线上两者都是满额**（1000‰）⇒ 比值 1。所以不能写死 833，要按同一个 `d` 现算。*/
        int32_t ci = (s_geom_d_px <= 0) ? 1000 : (int32_t)((int64_t)750 * (450 - s_geom_d_px) / 450);
        r = (int64_t)ci * 1000 / c0;
        return (int)r;
    }
    case CE_COEFF_FAMILY_08: {
        /* th08：coeff = 0.5×(1−d/500) ⇒ 千分点 `500 − d`，**d 取道具 y 那个**（一手：th08 判的是道具 `item+0x2a8`）。
         * 线上 = 1000‰；d ≥ 500 夹 0。*/
        int32_t di = s_geom_d_item_px;
        int32_t ci = (s_geom_d_px <= 0) ? 1000 : (500 - di);
        if (ci < 0) ci = 0;
        r = (int64_t)ci * 1000 / c0;
        return (int)r;
    }
    case CE_COEFF_FAMILY_10: {
        /* th10（风神录）：一手 `value = F/2 − d×(F−100000)/6080`、线上 = `F`
         * ⇒ 千分点 `500 − d×(F−100000)×1000/(6080·F)`，**d 取道具 y**（一手：th10 判 `item+0x3c4` 与 144）。
         * `F` 未桥进来（≤ 0）⇒ 不归一（比值 1000，= 老行为）。*/
        int32_t di = s_geom_d_item_px;
        int64_t ci;
        if (s_family10_faith <= 0) return 1000;
        if (s_geom_d_px <= 0) return 1000;                  /* 线上满额 */
        ci = 500 - ((int64_t)di * (int64_t)(s_family10_faith - 100000) * 1000)
                 / ((int64_t)6080 * (int64_t)s_family10_faith);
        if (ci < 0) ci = 0;
        if (ci > 1000) ci = 1000;
        r = ci * 1000 / c0;
        return (int)r;
    }
    case CE_COEFF_FAMILY_1:
        /* th07 / th11：这一项不吃折扣 ⇒ 反归一放大（线下 > 1000‰）*/
        r = (int64_t)1000 * 1000 / c0;
        return (int)r;
    case CE_COEFF_FAMILY_NONE:
    default:
        return 1000;                              /* 10：TODO（缺 F 输入）；NONE：老行为 */
    }
}

void ce_score_family_declare(uint32_t id, int family)
{
    if (id > (uint32_t)CE_SDK_CARD_ID_MAX) return;
    if (family < 0 || family > CE_COEFF_FAMILY_10) family = CE_COEFF_FAMILY_NONE;
    s_score_family[id] = (uint8_t)family;
}

void ce_score_family10_faith(int32_t faith_display)
{
    s_family10_faith = faith_display > 0 ? faith_display : 0;
}

int ce_score_family_of(uint32_t id)
{
    if (id > (uint32_t)CE_SDK_CARD_ID_MAX) return CE_COEFF_FAMILY_NONE;
    return (int)s_score_family[id];
}

void ce_score_declare_delta(uint32_t id, int32_t points)
{
    if (id > (uint32_t)CE_SDK_CARD_ID_MAX) return;
    s_score_delta[id] = points > 0 ? points : 0;
    score_recompute();
}

void ce_score_declare_coupled(uint32_t id, int32_t points)
{
    if (id > (uint32_t)CE_SDK_CARD_ID_MAX) return;
    s_score_coupled[id] = points > 0 ? points : 0;
    score_recompute();
}

void ce_score_declare_legacy(uint32_t id, int32_t value)
{
    if (id > (uint32_t)CE_SDK_CARD_ID_MAX) return;
    /* 旧语义 `0` = 这张卡不参与 ⇒ 记 -1（**不能**记 0：0 是「声明了 0」）。*/
    s_score_legacy[id] = (value != 0) ? value : -1;
    score_recompute();
}

void ce_score_declare(uint32_t id, int32_t value) { ce_score_declare_legacy(id, value); }

void ce_score_mul_declare(uint32_t id, int permille)
{
    if (id > (uint32_t)CE_SDK_CARD_ID_MAX) return;
    s_score_mul[id] = permille > 0 ? permille : 0;
    score_recompute();
}

int ce_score_graze_mult(uint32_t graze)
{
    /* `graze` 语义上是非负计数，但引擎给的是 int32 ⇒ 先按 int32 读回（负数按 0 夹），
     * 免得上层把 `-1` 传成 4294967295 之后被当成上限。*/
    int32_t g = (int32_t)graze;
    int32_t steps;
    if (g <= 0) return 0;
    /* ★ 2026-09-15 一手订正：整数阶梯 —— `min(⌊graze/100⌋, 899) × 10` 千分点（见 sdk_core.h 的常量注）。*/
    steps = g / CE_SCORE_GRAZE_STEP;
    if (steps > CE_SCORE_GRAZE_STEP_MAX) steps = CE_SCORE_GRAZE_STEP_MAX;
    return steps * 10;
}

int ce_score_signal_strength(void) { return s_score_mul_sum; }
int ce_score_mul_aggregate(void)   { return s_score_mul_sum; }

int ce_score_coupling_permille(void)
{
    /* ★ 只有这一项带乘数，而且**两边都没有贡献时必须是 1000（×1.0）**：
     * 用户规格 `(擦弹倍率 + 信号强度) if 有 111 else 1` —— 「有 111」那一支里
     * 擦弹倍率的基准是 1.0（th11 的擦弹倍率初值）、信号强度基准也是 1.0。
     * 若这里返回 0，未耦合族的卡会被缩到 0，那不是公式的意思。*/
    if (s_score_mul_n == 0) return CE_SCORE_COUPLING_DEFAULT;
    return CE_SCORE_COUPLING_DEFAULT + s_score_mul_sum;
}

int32_t ce_score_base_points(void)
{
    /* ★★ 2026-09-13 第二次修订（单位订正）：**先钳身价，再 ÷10**。
     * 旧写法是 `clamp(raw/10, 10000, 表值)` —— 把两个**身价**单位的界钳在「点」上，
     * 严了 10 倍：身价 < 100000（= 站点 ecx < 1e7，NORMAL 资金力 ≤ 98 / EASY ≤ 198）时
     * 会把 G_0 抬到 10000 点，而引擎原生只给 ecx/1000 ∈ [1000, 10000] 点
     * ⇒ 落笔后 SCORE 增量比原生大最多 10 倍。
     * 引擎自己在 0x446b53–0x446bf1 已经钳过同一个界（钳 ecx，±100 倍）⇒ 现场这次钳**恒等**：
     *   `G_0 ≡ s_score_raw / 10 ≡ 站点 ecx / 1000`（单测 `test_score_base_unit_identity` 钉死）。
     * 保留它只为「桥值来自别处 / 单测直接喂界外值」时兜底。*/
    int64_t raw = (int64_t)s_score_raw;
    int64_t hi;

    switch (s_score_diff) {
    case CE_SCORE_DIFF_EASY:    hi = CE_SCORE_BASE_MAX_EASY;    break;
    case CE_SCORE_DIFF_HARD:    hi = CE_SCORE_BASE_MAX_HARD;    break;
    case CE_SCORE_DIFF_LUNATIC: hi = CE_SCORE_BASE_MAX_LUNATIC; break;
    case CE_SCORE_DIFF_EXTRA:   hi = CE_SCORE_BASE_MAX_EXTRA;   break;
    default:                    hi = CE_SCORE_BASE_MAX_NORMAL;  break;
    }
    if (raw < CE_SCORE_BASE_MIN_PTS) raw = CE_SCORE_BASE_MIN_PTS;
    else if (raw > hi) raw = hi;
    return (int32_t)(raw / CE_SCORE_ENGINE_UNIT);
}

int32_t ce_score_total(void)
{
    int64_t coupled = (int64_t)s_score_coupled_sum * ce_score_coupling_permille() / 1000;
    /* ★ 共享 PIV 池族（各卡各报一份读数）+ 擦弹那**一项**（只算一次）—— 口径见 sdk_core.h。
     * ⚠ 现在这两项**先吃引擎的 `coeff_0`**（落笔点在系数之前）；「每项走自己原作的曲线」
     *   （0.75 族 → 5/6·coeff_0 等）在计分层换版那一步落地（`NEXT.md` 待办 ②）。*/
    int64_t piv = ce_piv_family_points();
    int64_t grz = ce_graze_points();
    /* ★ 路线 A：**逐项按自己的族归一**（`ΔG_i × coeff_i / coeff_0`）—— 引擎随后乘 `coeff_0`，
     *   实际到手就是 `G_0·coeff_0 + Σ ΔG_i·coeff_i`。未声明族的卡比值 = 1 ⇒ **老行为逐字节不变**。
     *   PIV 池与擦弹那一项属于 th12–th17 族 ⇒ 恒 ×5/6。*/
    int64_t sum = (int64_t)s_score_legacy_sum + coupled;
    uint32_t fid;
    int64_t norm = 0;
    for (fid = 0; fid <= (uint32_t)CE_SDK_CARD_ID_MAX; ++fid) {
        if (!s_score_delta[fid]) continue;
        norm += (int64_t)s_score_delta[fid] * ce_score_ratio_permille((int)s_score_family[fid]) / 1000;
    }
    norm += piv * ce_score_ratio_permille(CE_COEFF_FAMILY_075) / 1000;
    norm += grz * ce_score_ratio_permille(CE_COEFF_FAMILY_075) / 1000;
    sum += norm;
    int64_t total;

    if (sum < 0) sum = 0;
    if (sum > 0x7fffffff) sum = 0x7fffffff;
    total = (int64_t)ce_score_base_points() + sum;
    if (total > 0x7fffffff) total = 0x7fffffff;
    return (int32_t)total;
}

int32_t ce_score_apply(void)
{
    return ce_score_total() * CE_SCORE_ENGINE_UNIT;      /* 引擎随后 SCORE += value / 10 */
}

int ce_score_participants(void)
{
    /* ★ PIV 池族与擦弹族也算「有卡参与」：否则只有它们在场时 SDK 一个字都不写、那两项白算。
     *   （PIV 族按在场卡数算、擦弹族算 1 —— 两者都是「有这套系统就参与」。）*/
    return s_score_delta_n + s_score_coupled_n + s_score_legacy_n
         + ce_piv_present_count() + (ce_graze_family_present() ? 1 : 0);
}

int32_t ce_score_aggregate(void)
{
    return s_score_delta_sum + s_score_coupled_sum + s_score_legacy_sum;
}

int ce_score_applier(void)      { return s_score_applier_id; }
int ce_score_legacy_count(void) { return s_score_legacy_n; }
int32_t ce_score_legacy_sum(void) { return s_score_legacy_sum; }
int ce_score_coeff_permille(void) { return s_score_coeff; }



/* ---- 「本帧已消过弹」闩锁 + 本帧消弹记录（EVENTS-CONTRACT §5.2 细则 2）----
 * 两半分工（2026-09-13 加，原生卡冲突审计的 G 通道）：
 *   · **闩锁**（owner）= 「本帧谁拿全屏消弹这一笔」的**仲裁**（用户 2026-09-12 定案：只有一家做）；
 *   · **记录**（count/seen）= 「本帧本层到底消掉了几颗弹」的**事实**，由 SDK 的消弹 helper **自动**登记
 *     （`sdk.h` 的 `ce_cancel_radius` / `ce_cancel_all_bullets`），**不靠卡自己记得调**。
 * 为什么要记录：闩锁以前是纯 opt-in 的君子协定 ⇒ 卡会把「我没认领到」当成「本帧没消过弹」，
 * 在有原生卡清弹（47 SUN 每帧全屏清、19/20/51 每帧 option 清…）或别的卡先动手时结算错位。
 * 现在卡侧能拿到**真实颗数**：count == 0 才代表"真的一颗都没消"。*/
static int s_cancel_latch_owner;
static int s_cancel_frame_count;   /* 本帧本层消掉的弹数合计（数不出颗数的那种只置 unknown，不加这里）*/
static int s_cancel_frame_seen;    /* 本帧有没有发生过消弹（含数不出颗数的 ce_cancel_all_bullets）*/
static int s_cancel_frame_unknown; /* 本帧有没有「颗数数不出来」的消弹（诊断用）*/

void ce_cancel_latch_reset(void)
{
    s_cancel_latch_owner = -1;
    s_cancel_frame_count = 0;
    s_cancel_frame_seen = 0;
    s_cancel_frame_unknown = 0;
}

/* 记录一次消弹。n = 颗数（>= 0）；n < 0 = 数不出来（`ce_cancel_all_bullets` 这类 void 全屏清）。*/
void ce_cancel_frame_note(int n)
{
    s_cancel_frame_seen = 1;
    if (n > 0) s_cancel_frame_count += n;
    else if (n < 0) s_cancel_frame_unknown = 1;
}

int ce_cancel_frame_count(void) { return s_cancel_frame_count; }
int ce_cancel_frame_seen(void)  { return s_cancel_frame_seen; }
int ce_cancel_frame_unknown(void) { return s_cancel_frame_unknown; }

/* 认领之后发现**一颗都没消**（原生卡先清了池、或池本来就空）⇒ 把闩锁交回去。
 * 语义：闩锁代表「本帧有人真的消过弹」，不是「有人曾经想消弹」——一次空认领不该把
 * 后面的卡整帧挡在门外（那正是「没记到 ≠ 没消」的反面）。只有认领者自己能交回。*/
int ce_cancel_latch_release(uint32_t id)
{
    if (id > (uint32_t)CE_SDK_CARD_ID_MAX) return 0;
    if (s_cancel_latch_owner != (int)id) return 0;
    s_cancel_latch_owner = -1;
    return 1;
}

/* ---- 「这次死亡由谁救」闩锁（用户 2026-09-12 定案：救援层顺序 107 → 117 → 决死 B → 113）----
 * 纯记账：SDK 每帧 reset；107/117 无条件报到，113 只在 owner < 0 时才动手（见 sdk_core.h）。*/
static int s_rescue_note_owner = -1;

void ce_rescue_note_reset(void) { s_rescue_note_owner = -1; }

int ce_rescue_note_claim(uint32_t id)
{
    if (id > (uint32_t)CE_SDK_CARD_ID_MAX) return 0;
    if (s_rescue_note_owner >= 0) return 0;
    s_rescue_note_owner = (int)id;
    return 1;
}

int ce_rescue_note_owner(void) { return s_rescue_note_owner; }

/* ---- ★★ 「这次死亡是卡自己造的」通报位（见 sdk_core.h 的说明块）----
 * 为什么需要它：113 的补死（`Player__die` 直调）会广播 `+0x10`，与「撞弹瞬间」同形；
 * 只有造死的卡自己知道那不是撞弹 ⇒ 由它通报，救援层据此让位。 */
static int s_forced_death;

void ce_forced_death_begin(void) { s_forced_death = 1; }
void ce_forced_death_end(void)   { s_forced_death = 0; }
int  ce_forced_death_active(void) { return s_forced_death; }
void ce_forced_death_reset(void)  { s_forced_death = 0; }

/* ---- ★★ 「这次死亡最终有没有被取消」结果闩锁（2026-09-13；CARD-NATIVE-CONFLICT-AUDIT §A-2）----
 * 一手（逐指令）：case 4 里五家救援者（卡虚表 `+0x0c`）全跑完，才在 `th18:0x45c127` 判一次
 * （`acc` = 各返回值的 OR）；非 0 ⇒ 跳 `0x45c3d1`（取消），0 ⇒ `0x45c135 call 0x45d090`（扣命）。
 * `+0x0c` 的形参只能看见**前面**的卡 ⇒ 排在后面的原生 35（不看 acc）与别人的卡 69（无条件扣命）
 * 需要**最终结果**才能不白记账。本层只**复述引擎那一次判定**，不参与任何控制流。
 * 写入者只有 `sdk.c` 的两处断点；卡侧只读。语义见 sdk_core.h。*/
static int      s_death_result = CE_DEATH_RESULT_UNKNOWN;
static unsigned s_death_result_seq;

void ce_death_result_reset(void) { s_death_result = CE_DEATH_RESULT_UNKNOWN; }

void ce_death_result_note(int cancelled)
{
    s_death_result = cancelled ? 1 : 0;
    ++s_death_result_seq;
}

int ce_death_result(void) { return s_death_result; }
int ce_death_cancelled(void) { return s_death_result == 1; }
unsigned ce_death_result_seq(void) { return s_death_result_seq; }

int ce_cancel_latch_claim(uint32_t id)
{
    if (id > (uint32_t)CE_SDK_CARD_ID_MAX) return 0;
    if (s_cancel_latch_owner >= 0) return 0;
    s_cancel_latch_owner = (int)id;
    return 1;
}

int ce_cancel_latch_owner(void) { return s_cancel_latch_owner; }


/* ---- 低速子机声明层（选举 id 最小者 + 逐格偏移 + 每帧清集合 + 落笔权交接）----
 * 语义 / 一手依据（为什么必须逐格）全在 sdk_core.h 的块注释里。这里只写实现上的取舍：
 *   · 声明是**每帧**的（`ce_optpose_frame_reset` 由 SDK 在每个 `BP_ce_item_frame` 里调）⇒
 *     不声明就退出，不需要「粘性 + 显式清 0」那一套（与伤害层不同，那张表是粘性的）；
 *   · 清集合只清 `count`（落笔者的 cells/script 只在该 id 赢时才被读，赢了必然是这一帧刚写的）；
 *   · 交接记账 `s_optpose_current` 只在 `ce_optpose_frame()` 里推进 —— 删卡（`ce_optpose_drop`）
 *     不改它，好让「上一帧的落笔者这一帧不在了」照走一次退位通知（对方可能已经不在了，SDK 自己会跳过）。*/
static ce_option_pose_t s_optpose[CE_SDK_CARD_ID_MAX + 1];
static int s_optpose_winner = -1;
static int s_optpose_current = -1;

static void optpose_recompute(void)
{
    s_optpose_winner = -1;
    for (int id = 0; id <= CE_SDK_CARD_ID_MAX; ++id) {
        if (s_optpose[id].count > 0) { s_optpose_winner = id; break; }   /* id 最小的那张 */
    }
}

void ce_optpose_reset(void)
{
    for (int id = 0; id <= CE_SDK_CARD_ID_MAX; ++id) {
        s_optpose[id].count = 0;
        s_optpose[id].anm_script = 0;
        for (int k = 0; k < CE_OPTION_POSE_CELLS; ++k) {
            s_optpose[id].cell[k].fx = 0.0f;
            s_optpose[id].cell[k].fy = 0.0f;
            s_optpose[id].cell[k].ux = 0.0f;
            s_optpose[id].cell[k].uy = 0.0f;
        }
    }
    s_optpose_winner = -1;
    s_optpose_current = -1;
}

int ce_optpose_declare(uint32_t id, const ce_option_pose_t *p)
{
    int n;

    if (id > (uint32_t)CE_SDK_CARD_ID_MAX) return 0;
    if (!p || p->count <= 0) {                 /* 退出声明（也覆盖「声明 NULL」这条）*/
        s_optpose[id].count = 0;
        optpose_recompute();
        return 0;
    }
    n = p->count > CE_OPTION_POSE_CELLS ? CE_OPTION_POSE_CELLS : p->count;
    s_optpose[id].count = n;
    s_optpose[id].anm_script = p->anm_script;
    for (int k = 0; k < n; ++k) s_optpose[id].cell[k] = p->cell[k];   /* 只拷有效格 */
    optpose_recompute();
    return n;
}

void ce_optpose_drop(uint32_t id) { (void)ce_optpose_declare(id, NULL); }

void ce_optpose_frame_reset(void)
{
    for (int id = 0; id <= CE_SDK_CARD_ID_MAX; ++id) s_optpose[id].count = 0;
    s_optpose_winner = -1;
}

int ce_optpose_winner(void) { return s_optpose_winner; }

const ce_option_pose_t *ce_optpose_pose(void)
{
    if (s_optpose_winner < 0) return NULL;
    return &s_optpose[s_optpose_winner];
}

int ce_optpose_frame(int *out_prev)
{
    int win = s_optpose_winner;
    int prev = (s_optpose_current >= 0 && s_optpose_current != win) ? s_optpose_current : -1;

    if (out_prev) *out_prev = prev;
    s_optpose_current = win;
    return win;
}


/* ---- HUD 槽位分配器 ---- */
static struct {
    int    used;
    int    reserved;        /* 局中 release 后保留占位（不参与重排）*/
    uint32_t id;
    float  w, h;
    int    side;
    int    priority;        /* ★ 2026-10-03k：大的先摆（⇒ 落在更下面的行）*/
    int    own_row;         /* ★ 2026-10-03k：独占一行（宽块用）*/
    int    fell_back;       /* ★ 2026-10-03l：pinned 撞了 ⇒ 已降级成货架成员（幂等要它）*/
    float  x, y;
} s_hud[CE_SDK_CARD_ID_MAX + 1];
static int s_hud_near_corner;
static int s_hud_card_present;      /* 原生「当前卡」那块 UI 在不在（`ce_hud_card_set()`，见 sdk_core.h）*/

/* ★★★ 2026-10-03l（owner：「**理论上，包含左下角 HUD 的都应当进这个系统**」）——
 *   **PINNED 也进系统**：登记后拿它自己的矩形与**已占位**块做碰撞检测：
 *     · **不相交** ⇒ 用它自己的锚点（**保真**：各卡复刻的是各自原作的 HUD 位置）；
 *     · **相交**   ⇒ **回退货架**（把这一块降级成 `LEFT` 参与排布）⇒ **两张卡同装时不压在一起**。
 *   语义：**后认领的那张让开**。认领顺序 = 卡组顺序（同一次开局 / 同一次读档恒定）
 *   ⇒ 同样的卡组永远得到同样的结果（replay 安全）；`s_hud_fallbacks` 是诊断计数。
 *   ⚠ `reserved`（局中删卡后的保留位）**不算在场** ⇒ 不能因为它把新卡赶走。*/
static int s_hud_fallbacks;
int ce_hud_slot_fallbacks(void) { return s_hud_fallbacks; }

/* 矩形相交：块的 `(x, y)` = **左下角**（y = 下边缘，TH 坐标 y 向下增长）⇒ 占 `x..x+w` × `y-h..y`。*/
static int hud_rect_hit(float ax, float ay, float aw, float ah,
                        float bx, float by, float bw, float bh)
{
    int xo = (ax < bx + bw) && (bx < ax + aw);
    int yo = (ay - ah < by) && (by - bh < ay);
    return xo && yo;
}


void ce_hud_card_set(int present) { s_hud_card_present = present ? 1 : 0; }

static void hud_relayout(void)
{
    /* 底部货架（HUD-LAYOUT §2）：按 card_id 升序扫描（数组按 id 索引 ⇒ 天然有序），两侧各自算。
     * 左列：cursor 从左锚点向右；右列：块的**右边缘**贴在 cursor 上、cursor 向左走。
     * ★ 2026-09-22：左列的**起点**不再写死锚点，而是 `hud_card_shelf_x0()` —— 有当前卡时取
     *   `max(锚点, 块右沿 + 间隙)`，没有时就是锚点（那块整体不存在，不占地方）。
     *   现值代入是恒等（`HUD_CARD_POS_X + HUD_CARD_W` = 116，`+8` = 124 < 257）；
     *   意义在**不变式**：谁把锚点左移过 124 或把块加宽到压过锚点，会立刻在这里让位。*/
    float cx[2], row_bottom[2], row_h[2];
    float left0;
    hud_card_shelf_x0(&left0, CE_HUD_ANCHOR_X, CE_HUD_GAP, s_hud_card_present);
    cx[0] = left0;                   row_bottom[0] = CE_HUD_ANCHOR_Y; row_h[0] = 0.0f;
    cx[1] = CE_HUD_ANCHOR_X_RIGHT;   row_bottom[1] = CE_HUD_ANCHOR_Y; row_h[1] = 0.0f;

    /* ★★★ 2026-10-03k（owner：「首先得考虑某种**美观性**」）—— 摆放顺序 = (priority 降, card_id 升)。
     *   为什么需要它：原来只按 card_id 排 ⇒ 谁先注册谁占底边行，**块有多宽、多重要都不影响**；
     *   于是「109 那条 452 宽的蓄力槽」会和别人挤在同一行（右边吊着个小块，很难看）。
     *   现在：优先级高的先摆 ⇒ 落在更下面的行；`own_row` 的块**独自占一行**。
     *   实现用**插入排序**（N ≤ 255，纯整数比较）⇒ 不引进 qsort（宿主/目标两套 libc 行为差异）。*/
    static int order[CE_SDK_CARD_ID_MAX + 1];    /* ⚠ 文件静态：钩子里的栈很紧（255×4 = 1 KB 不进栈）*/
    int n = 0;
    for (int id = 0; id <= CE_SDK_CARD_ID_MAX; ++id) {
        if (!s_hud[id].used) continue;
        if (s_hud[id].side == CE_HUD_SIDE_PINNED) continue;         /* 自带锚点：不参与货架排布 */
        order[n++] = id;
    }
    for (int i = 1; i < n; ++i) {                                   /* 插入排序：(priority 降, id 升) */
        int key = order[i], j = i - 1;
        while (j >= 0 && (s_hud[order[j]].priority < s_hud[key].priority ||
                          (s_hud[order[j]].priority == s_hud[key].priority && order[j] > key))) {
            order[j + 1] = order[j];
            --j;
        }
        order[j + 1] = key;
    }

    for (int k = 0; k < n; ++k) {
        int id = order[k];
        int side = (s_hud[id].side == CE_HUD_SIDE_RIGHT) ? 1 : 0;
        int fits;
        /* ★ `own_row`：**摆之前**先断行（把上一行封口），保证它从一条干净的行走起。*/
        if (s_hud[id].own_row && row_h[side] > 0.0f) {
            row_bottom[side] -= row_h[side] + CE_HUD_GAP;
            cx[side] = (side == 0) ? left0 : CE_HUD_ANCHOR_X_RIGHT;
            row_h[side] = 0.0f;
        }
        if (side == 0) fits = (cx[0] + s_hud[id].w <= CE_HUD_X_MAX);
        else           fits = (cx[1] - s_hud[id].w >= CE_HUD_X_MIN);
        /* 这一行放不下 ⇒ 上抬一行。空行上的「超大块」不抬（认领时已过 §4.2 硬约束），
         * 否则单块就会被推到一条没意义的第 2 行上。*/
        if (!fits && row_h[side] > 0.0f) {
            row_bottom[side] -= row_h[side] + CE_HUD_GAP;
            cx[side] = (side == 0) ? left0 : CE_HUD_ANCHOR_X_RIGHT;
            row_h[side] = 0.0f;
        }
        if (side == 0) {
            s_hud[id].x = cx[0];
            s_hud[id].y = row_bottom[0];
            cx[0] += s_hud[id].w + CE_HUD_GAP;
        } else {
            s_hud[id].x = cx[1] - s_hud[id].w;
            s_hud[id].y = row_bottom[1];
            cx[1] -= s_hud[id].w + CE_HUD_GAP;
        }
        if (s_hud[id].h > row_h[side]) row_h[side] = s_hud[id].h;
        /* ★ `own_row`：**摆之后**也断行 ⇒ 后面的块绝不会贴到它右边。*/
        if (s_hud[id].own_row) {
            row_bottom[side] -= row_h[side] + CE_HUD_GAP;
            cx[side] = (side == 0) ? left0 : CE_HUD_ANCHOR_X_RIGHT;
            row_h[side] = 0.0f;
        }
    }
}

static int s_hud_stage = -1;                                    /* 上一次认领时的关卡号 */

void ce_hud_stage_begin(int stage)
{
    if (stage == s_hud_stage) return;
    s_hud_stage = stage;
    ce_hud_slots_reset();                                       /* 新关卡 ⇒ 按 id 升序重排 */
    s_hud_stage = stage;                                        /* （reset 不动纪元，重申一次更清晰）*/
}

int ce_hud_stage_current(void) { return s_hud_stage; }

void ce_hud_slots_reset(void)
{
    for (int id = 0; id <= CE_SDK_CARD_ID_MAX; ++id) {
        s_hud[id].used = 0;
        s_hud[id].reserved = 0;
        s_hud[id].id = 0;
        s_hud[id].w = s_hud[id].h = 0.0f;
        s_hud[id].side = CE_HUD_SIDE_LEFT;
        s_hud[id].priority = 0;
        s_hud[id].own_row = 0;
        s_hud[id].fell_back = 0;
        s_hud[id].x = s_hud[id].y = 0.0f;
    }
    s_hud_near_corner = 0;
    s_hud_fallbacks = 0;
}
/* 把一块登记进表（不动锚点），再按 side 决定要不要重排。返回 0 = 参数非法。
 * `anchor_x/y` 只在 pinned 时用；`do_relayout` 由调用方给（pinned 不动别的位置）。*/
static int hud_register(uint32_t card_id, const ce_hud_block_t *blk,
                        float anchor_x, float anchor_y, int pinned, int priority, int own_row,
                        ce_hud_slot_t *out)
{
    const int was_used = (card_id <= (uint32_t)CE_SDK_CARD_ID_MAX && s_hud[card_id].used &&
                          !s_hud[card_id].reserved);      /* ★ 幂等判据要在改表之前取 */
    if (card_id > (uint32_t)CE_SDK_CARD_ID_MAX || !blk || !out) return 0;
    if (blk->w <= 0.0f || blk->h <= 0.0f) return 0;
    if (pinned) {
        if (blk->side != CE_HUD_SIDE_PINNED) return 0;
        if (anchor_x + blk->w > CE_HUD_X_HARD_MAX) return 0;        /* §4.2 对自带锚点同样成立 */
    } else {
        if (blk->side == CE_HUD_SIDE_PINNED) return 0;              /* pinned 必须走带锚点的入口 */
        /* §4.2 硬约束：块**右边缘**不许越过 1040（左列从 257 起算，右列从 1040 向左）*/
        if (blk->side == CE_HUD_SIDE_RIGHT) {
            if (CE_HUD_ANCHOR_X_RIGHT - blk->w < CE_HUD_X_MIN) return 0;
        } else {
            if (CE_HUD_ANCHOR_X + blk->w > CE_HUD_X_HARD_MAX) return 0;
        }
    }
    s_hud[card_id].used = 1;
    s_hud[card_id].reserved = 0;                                    /* 重新认领 = 复活这个占位 */
    s_hud[card_id].priority = priority;                             /* ★ 2026-10-03k（美观规则）*/
    s_hud[card_id].own_row  = own_row ? 1 : 0;
    s_hud[card_id].id = card_id;
    s_hud[card_id].w = blk->w;
    s_hud[card_id].h = blk->h;
    s_hud[card_id].side = blk->side;
    if (pinned) {
        /* ★ PINNED 的**碰撞回退**（见 `hud_rect_hit` 的注）：撞了就把这一块降级成货架成员。
         * ⚠ **幂等**（§4.4「同一关内不重排」）：**已经认领过**的卡**不重算** ——
         *   上次落哪儿就还在哪儿（否则「on_load 每关重认领」会把位置来回抖，`
         *   也会被后来插进来的块"顶走"）。*/
        if (was_used) {
            if (s_hud[card_id].fell_back) {
                s_hud[card_id].side = CE_HUD_SIDE_LEFT;
                hud_relayout();
            } else {
                s_hud[card_id].x = anchor_x;
                s_hud[card_id].y = anchor_y;
            }
        } else {
            int hit = 0;
            for (int id2 = 0; id2 <= CE_SDK_CARD_ID_MAX && !hit; ++id2) {
                if (id2 == (int)card_id || !s_hud[id2].used || s_hud[id2].reserved) continue;
                if (hud_rect_hit(anchor_x, anchor_y, blk->w, blk->h,
                                 s_hud[id2].x, s_hud[id2].y, s_hud[id2].w, s_hud[id2].h)) hit = 1;
            }
            /* ★★★ 2026-10-03o（owner：「你得考虑**原生 UI 的主动卡本身** —— 当你有主动卡的时候，
             *   **最左下角就会多出来一个卡面**」）：那块**原生卡面**也是**在场的一块**
             *   （几何 = `cards/hud_dim.h` 的 `HUD_CARD_POS_X/Y` + `HUD_CARD_W/H` ⇒ 表面 68..116 × 864..924）。
             *   我们的 pinned 块（64/68/88 那一片）本来就压在它身上 ⇒ 卡面在的时候必须让开。
             *   ⚠ 货架那一侧**早就让了**（`hud_card_shelf_x0()` ⇒ 起点 max(锚点, 116+8)）；
             *     这里补的是 **PINNED 不该成为例外**：撞卡面 ⇒ 同样回退货架（落到 124 右边，天然不撞）。*/
            if (!hit && s_hud_card_present &&
                hud_rect_hit(anchor_x, anchor_y, blk->w, blk->h,
                             HUD_CARD_POS_X, HUD_CARD_POS_Y, HUD_CARD_W, HUD_CARD_H)) hit = 1;
            if (hit) {                                   /* 回退：降级成 LEFT 参与排布 */
                s_hud[card_id].side = CE_HUD_SIDE_LEFT;
                s_hud[card_id].priority = 0;
                s_hud[card_id].own_row = 0;
                s_hud[card_id].fell_back = 1;
                ++s_hud_fallbacks;
                hud_relayout();
            } else {
                s_hud[card_id].fell_back = 0;
                s_hud[card_id].x = anchor_x;
                s_hud[card_id].y = anchor_y;
            }
        }
    } else { hud_relayout(); }
    out->x = s_hud[card_id].x;
    out->y = s_hud[card_id].y;
    out->x640 = s_hud[card_id].x * CE_HUD_640_SCALE;
    out->y640 = s_hud[card_id].y * CE_HUD_640_SCALE;
    out->w = s_hud[card_id].w;
    out->h = s_hud[card_id].h;
    out->side = s_hud[card_id].side;
    return 1;
}

int ce_hud_slot_claim_pinned(uint32_t card_id, const ce_hud_block_t *blk,
                             float anchor_x, float anchor_y, ce_hud_slot_t *out)
{
    return hud_register(card_id, blk, anchor_x, anchor_y, 1, 0, 0, out);
}

int ce_hud_slot_claim(uint32_t card_id, const ce_hud_block_t *blk, ce_hud_slot_t *out)
{
    return hud_register(card_id, blk, 0.0f, 0.0f, 0, 0, 0, out);
}

int ce_hud_slot_claim_ex(uint32_t card_id, const ce_hud_block_t *blk,
                         int priority, int own_row, ce_hud_slot_t *out)
{
    return hud_register(card_id, blk, 0.0f, 0.0f, 0, priority, own_row, out);
}

int ce_hud_slot_get(uint32_t card_id, ce_hud_slot_t *out)
{
    if (card_id > (uint32_t)CE_SDK_CARD_ID_MAX || !out) return 0;
    if (!s_hud[card_id].used) return 0;
    out->x = s_hud[card_id].x;
    out->y = s_hud[card_id].y;
    out->w = s_hud[card_id].w;
    out->h = s_hud[card_id].h;
    out->side = s_hud[card_id].side;
    return 1;
}

void ce_hud_slot_release(uint32_t card_id)
{
    if (card_id > (uint32_t)CE_SDK_CARD_ID_MAX) return;
    if (!s_hud[card_id].used) return;
    /* §4.4：局中删卡**不当场重排** ⇒ 先只标记保留；下一关开场 reset 时真正收回。*/
    s_hud[card_id].reserved = 1;
}

int ce_hud_slot_claimed(uint32_t card_id)
{
    if (card_id > (uint32_t)CE_SDK_CARD_ID_MAX) return 0;
    return s_hud[card_id].used;
}

void ce_hud_near_corner_update(float player_x, float player_y)
{
    if (!s_hud_near_corner) {
        if (player_x < -64.0f && player_y > 400.0f) s_hud_near_corner = 1;
    } else {
        if (player_y < 384.0f || player_x > -64.0f) s_hud_near_corner = 0;
    }
}

int ce_hud_near_corner(void) { return s_hud_near_corner; }

int ce_hud_player_near_corner(float player_x, float player_y)
{
    ce_hud_near_corner_update(player_x, player_y);
    return s_hud_near_corner;
}


/* ---- 掉落层（先加后乘，乘作用最终总数）---- */
static int s_drop_mult[CE_SDK_CARD_ID_MAX + 1];
static int s_drop_mult_agg = CE_DROP_MULT_UNIT;

static void drop_recompute(void)
{
    long long agg = CE_DROP_MULT_UNIT;
    for (int id = 0; id <= CE_SDK_CARD_ID_MAX; ++id) {
        if (s_drop_mult[id] <= 0) continue;
        agg = agg * s_drop_mult[id] / CE_DROP_MULT_UNIT;
    }
    s_drop_mult_agg = (int)agg;
}

void ce_drop_reset(void)
{
    for (int id = 0; id <= CE_SDK_CARD_ID_MAX; ++id) s_drop_mult[id] = 0;
    s_drop_mult_agg = CE_DROP_MULT_UNIT;
}

void ce_drop_mult_set(uint32_t id, int permille)
{
    if (id > (uint32_t)CE_SDK_CARD_ID_MAX) return;
    s_drop_mult[id] = permille > 0 ? permille : 0;
    drop_recompute();
}

int ce_drop_mult_aggregate(void) { return s_drop_mult_agg; }

void ce_drop_apply(int32_t *zdrop)
{
    if (!zdrop) return;
    int mult = s_drop_mult_agg;
    if (mult == CE_DROP_MULT_UNIT) return;                 /* 没人参与 ⇒ 一个字节都不碰 */
    int32_t main_drop = zdrop[0];
    for (int i = 0; i < CE_DROP_TYPES; ++i) {
        int type = i + 1;
        int is_main = (type == main_drop) && (type >= 1) && (type <= CE_DROP_MAX_MAIN);
        long long base = zdrop[1 + i] + (is_main ? 1 : 0);
        if (base < 0) base = 0;
        long long final_v = base * mult / CE_DROP_MULT_UNIT;
        long long extra = final_v - (is_main ? 1 : 0);
        if (extra < 0) extra = 0;
        if (extra > 0x7fffffffLL) extra = 0x7fffffffLL;
        zdrop[1 + i] = (int32_t)extra;
    }
    /* timed 表（+0x54，与 ex 表同 20 格）：引擎按 `ex[i] + timed[i]×(T−timer)/T` 折算，
     * 对乘数同样线性 ⇒ 一起乘；**不做「已撒一颗」补偿**（主掉落只补进 ex 表）。
     * ⚠ 索引：基址起的第 21..40 个 int32；引擎对 ≤0 的格直接跳过（0x43055d `test ecx,ecx / jle`）⇒ 不动它们。*/
    for (int i = 0; i < CE_DROP_TIMED_TYPES; ++i) {
        int32_t v = zdrop[1 + CE_DROP_TYPES + i];
        if (v <= 0) continue;
        long long f = (long long)v * mult / CE_DROP_MULT_UNIT;
        if (f > 0x7fffffffLL) f = 0x7fffffffLL;
        zdrop[1 + CE_DROP_TYPES + i] = (int32_t)f;
    }
}


/* ==================== ★★ 自建实体 → 114 收点批（虚拟收集 / 虚拟在飞）====================
 * 语义、契约、取舍（为什么是声明式而不是卡间直连）、复用意图（116 季节点 / 117 动物灵）——
 * **全在 `sdk_core.h` 的那段长注里**（那里是唯一出处）。这里只记实现上的三条取舍：
 *   · **自过期、无 reset 依赖**：本层不进 `ce_sdk_setup()` 的复位表（那是 `sdk.c` 的地盘），
 *     所以两条通道都用「SDK 帧号 + 容差窗口」表达时效 —— 没有消费者 / 生产者退场时，
 *     状态在 `CE_VITEM_HOLD_FRAMES` 帧内自然归零，跨局、跨关不会残留。
 *   · **容差窗口是读侧的事**：写侧永远写「这一帧」的真值（不含任何衰减），读侧才比较帧号
 *     ⇒ 单测可以只用 `ce_sdk_frame_bump()` 推进时间，不必猜生产者的调用节奏。
 *   · **uint32 帧差**：`ce_sdk_frame()` 是单调递增的 uint32（`BP_ce_item_frame` 每游玩帧 +1）
 *     ⇒ `now - frame` 用**无符号**减法，回绕也安全（2^32 帧 ≈ 2.2 年 @60fps）。
 *   · **零浮点**：整层是整数运算（`make dllx87` 的「我们的 object 里 0 条 x87」不变式）。*/
static struct {
    int      n;          /* 该源本帧声明的在飞数（`CE_VITEM_HALF` 恒 0）*/
    uint32_t frame;      /* 声明的 SDK 帧号（读侧按 `CE_VITEM_HOLD_FRAMES` 判时效）*/
} s_vitem_fly[CE_SDK_CARD_ID_MAX + 1];       /* 下标 = 源号（0 = 共享模块；其余 = card_id）*/

static int      s_vitem_note;                /* 成员累加器（待消费者抽干）*/
static uint32_t s_vitem_note_frame;          /* 最后一次上报的 SDK 帧号（时效判据）*/

void ce_vitem_reset(void)
{
    for (int i = 0; i <= CE_SDK_CARD_ID_MAX; ++i) { s_vitem_fly[i].n = 0; s_vitem_fly[i].frame = 0; }
    s_vitem_note = 0;
    s_vitem_note_frame = 0;
}

void ce_vitem_note_collect(int kind, int n)
{
    const uint32_t now = ce_sdk_frame();

    if (kind != CE_VITEM_FULL) return;                   /* 半道具：不入批（§1x.3，见 sdk_core.h 契约 ①）*/
    if (n <= 0) return;
    /* 上一批已经过期（没有消费者）⇒ 从零重新开始，别把陈年计数算进下一次抽干。*/
    if (s_vitem_note > 0 && now - s_vitem_note_frame > (uint32_t)CE_VITEM_HOLD_FRAMES) s_vitem_note = 0;
    if (s_vitem_note < CE_VITEM_NOTE_MAX) {
        s_vitem_note += n;
        if (s_vitem_note > CE_VITEM_NOTE_MAX) s_vitem_note = CE_VITEM_NOTE_MAX;
    }
    s_vitem_note_frame = now;
}

static int vitem_note_fresh(void)
{
    if (s_vitem_note <= 0) return 0;
    return (ce_sdk_frame() - s_vitem_note_frame) <= (uint32_t)CE_VITEM_HOLD_FRAMES;
}

int ce_vitem_collect_pending(void)
{
    return vitem_note_fresh() ? s_vitem_note : 0;
}

int ce_vitem_take_collect(void)
{
    const int n = ce_vitem_collect_pending();

    s_vitem_note = 0;                                    /* 过期与否都清零：陈年计数一次性作废 */
    return n;
}

void ce_vitem_declare_flying(uint32_t source, int kind, int n)
{
    if (source > (uint32_t)CE_SDK_CARD_ID_MAX) return;
    if (kind != CE_VITEM_FULL) n = 0;                    /* 半道具不算「有东西在飞」（契约 ①）*/
    s_vitem_fly[source].n = n > 0 ? n : 0;
    s_vitem_fly[source].frame = ce_sdk_frame();          /* 显式声明 0 也写帧号 ⇒ 立刻生效、不再靠过期 */
}

void ce_vitem_drop(uint32_t source)
{
    if (source > (uint32_t)CE_SDK_CARD_ID_MAX) return;
    s_vitem_fly[source].n = 0;
    s_vitem_fly[source].frame = 0;
}

int ce_vitem_flying_from(uint32_t source)
{
    if (source > (uint32_t)CE_SDK_CARD_ID_MAX) return 0;
    if (s_vitem_fly[source].n <= 0) return 0;
    if ((ce_sdk_frame() - s_vitem_fly[source].frame) > (uint32_t)CE_VITEM_HOLD_FRAMES) return 0;
    return s_vitem_fly[source].n;
}

int ce_vitem_flying(void)
{
    int sum = 0;

    for (uint32_t src = 0; src <= (uint32_t)CE_SDK_CARD_ID_MAX; ++src) sum += ce_vitem_flying_from(src);
    return sum;
}


/* ---- ★★ 2026-10-01 跨卡只读标志（接口与语义见 sdk_core.h）--------------------------------- */
static uint8_t s_card_flag[CE_SDK_CARD_ID_MAX + 1];

void ce_card_flag_set(uint32_t card_id, uint32_t bits)
{
    if (card_id > (uint32_t)CE_SDK_CARD_ID_MAX) return;
    s_card_flag[card_id] = (uint8_t)(bits & 0xffu);
}

uint32_t ce_card_flag_get(uint32_t card_id)
{
    if (card_id > (uint32_t)CE_SDK_CARD_ID_MAX) return 0u;
    return (uint32_t)s_card_flag[card_id];
}

int ce_card_flag_any(uint32_t bits)
{
    if (!bits) return 0;
    for (uint32_t id = 0; id <= (uint32_t)CE_SDK_CARD_ID_MAX; ++id) {
        if (s_card_flag[id] & (uint8_t)bits) return 1;
    }
    return 0;
}

void ce_card_flag_reset(void)
{
    memset(s_card_flag, 0, sizeof s_card_flag);
}

/* ---- ★★ 2026-10-02（Jera 88）：复制代数闸 + 可见数台账（语义见 sdk_core.h 末节）---- */

typedef struct {
    uint32_t id;      /* 0 = 空槽 */
    int32_t  n;       /* 本卡当前场上可复制的实体数 */
} ce_dup_vis_t;

static uint32_t    s_dup_gen = 1;                      /* 从 1 起（0 = 生产者的「还没见过」哨兵）*/
static ce_dup_vis_t s_dup_vis[CE_DUP_SLOT_MAX];

void ce_dup_reset(void)
{
    s_dup_gen = 1;
    memset(s_dup_vis, 0, sizeof s_dup_vis);
}

uint32_t ce_dup_bump(void)
{
    ++s_dup_gen;
    if (!s_dup_gen) s_dup_gen = 1;                     /* 0 保留作哨兵（回绕只可能发生在 40 亿次发动之后）*/
    return s_dup_gen;
}

uint32_t ce_dup_gen(void)
{
    return s_dup_gen;
}

int ce_dup_vis_set(uint32_t card_id, int n)
{
    int i, free_i = -1;
    if (!card_id) return 0;
    if (n < 0) n = 0;
    for (i = 0; i < CE_DUP_SLOT_MAX; ++i) {
        if (s_dup_vis[i].id == card_id) { s_dup_vis[i].n = n; return 1; }
        if (!s_dup_vis[i].id && free_i < 0) free_i = i;
    }
    if (free_i < 0) return 0;
    s_dup_vis[free_i].id = card_id;
    s_dup_vis[free_i].n = n;
    return 1;
}

void ce_dup_vis_drop(uint32_t card_id)
{
    int i;
    for (i = 0; i < CE_DUP_SLOT_MAX; ++i)
        if (s_dup_vis[i].id == card_id) { s_dup_vis[i].id = 0; s_dup_vis[i].n = 0; }
}

int ce_dup_vis_total(void)
{
    int i, t = 0;
    for (i = 0; i < CE_DUP_SLOT_MAX; ++i) t += s_dup_vis[i].n;
    return t;
}

int ce_dup_producers(void)
{
    int i, n = 0;
    for (i = 0; i < CE_DUP_SLOT_MAX; ++i) if (s_dup_vis[i].id) n++;
    return n;
}

/* ---- 清扫闸（92 用；实现与复制闸同款，只有"生产者做什么"不同）---- */
static uint32_t s_sweep_gen = 1;

void ce_sweep_reset(void)
{
    s_sweep_gen = 1;
}

uint32_t ce_sweep_bump(void)
{
    ++s_sweep_gen;
    if (!s_sweep_gen) s_sweep_gen = 1;
    return s_sweep_gen;
}

uint32_t ce_sweep_gen(void)
{
    return s_sweep_gen;
}

int ce_sweep_should_run(uint32_t *seen)
{
    uint32_t g = s_sweep_gen;
    if (!seen) return 0;
    if (!*seen) { *seen = g; return 0; }     /* 第一次 tick：只登记 */
    if (g == *seen) return 0;
    *seen = g;
    return 1;
}

int ce_dup_should_run(uint32_t *seen)
{
    uint32_t g = s_dup_gen;
    if (!seen) return 0;
    if (!*seen) { *seen = g; return 0; }     /* 第一次 tick：只登记（否则开局会误复制一次）*/
    if (g == *seen) return 0;
    *seen = g;
    return 1;
}

/* ---- 「这个引擎道具由我负责复制」的认领谓词（112 的满 B 用；语义见 sdk_core.h）---- */

typedef struct {
    uint32_t        id;      /* 0 = 空槽 */
    ce_dup_claim_fn fn;
} ce_dup_claim_t;

static ce_dup_claim_t s_dup_claim[CE_DUP_SLOT_MAX];

int ce_dup_claim_set(uint32_t card_id, ce_dup_claim_fn fn)
{
    int i, free_i = -1;
    if (!card_id) return 0;
    for (i = 0; i < CE_DUP_SLOT_MAX; ++i) {
        if (s_dup_claim[i].id == card_id) {
            s_dup_claim[i].fn = fn;               /* 幂等：同一张卡每帧重申一次也只会更新指针 */
            if (!fn) { s_dup_claim[i].id = 0; }   /* 撤认领 = 释放槽 */
            return 1;
        }
        if (!s_dup_claim[i].id && free_i < 0) free_i = i;
    }
    if (!fn || free_i < 0) return 0;
    s_dup_claim[free_i].id = card_id;
    s_dup_claim[free_i].fn = fn;
    return 1;
}

void ce_dup_claim_drop(uint32_t card_id)
{
    int i;
    for (i = 0; i < CE_DUP_SLOT_MAX; ++i)
        if (s_dup_claim[i].id == card_id) { s_dup_claim[i].id = 0; s_dup_claim[i].fn = 0; }
}

int ce_dup_item_claimed(const void *item)
{
    int i;
    if (!item) return 0;
    for (i = 0; i < CE_DUP_SLOT_MAX; ++i)
        if (s_dup_claim[i].id && s_dup_claim[i].fn && s_dup_claim[i].fn(item)) return 1;
    return 0;
}
