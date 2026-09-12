/* sdk_core.c —— 见 sdk_core.h。纯 C11。 */
#include <string.h>
#include "sdk_core.h"

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


/* ---- 计分声明层（求和 + id 最小者落笔）---- */
static int32_t s_score[CE_SDK_CARD_ID_MAX + 1];
static int32_t s_score_sum;
static int s_score_applier = -1;
static int32_t s_score_mul[CE_SDK_CARD_ID_MAX + 1];   /* 乘数附加项（千分点）*/
static int32_t s_score_mul_sum;

static void score_recompute(void)
{
    int32_t sum = 0;
    int who = -1;
    for (int id = 0; id <= CE_SDK_CARD_ID_MAX; ++id) {
        if (s_score[id] == 0) continue;
        if (who < 0) who = id;
        sum += s_score[id];
    }
    s_score_sum = sum;
    s_score_applier = who;
}

void ce_score_reset(void)
{
    for (int id = 0; id <= CE_SDK_CARD_ID_MAX; ++id) { s_score[id] = 0; s_score_mul[id] = 0; }
    s_score_sum = 0;
    s_score_mul_sum = 0;
    s_score_applier = -1;
}

void ce_score_declare(uint32_t id, int32_t value)
{
    if (id > (uint32_t)CE_SDK_CARD_ID_MAX) return;
    s_score[id] = value;
    score_recompute();
}

int32_t ce_score_aggregate(void) { return s_score_sum; }
int     ce_score_applier(void)   { return s_score_applier; }

void ce_score_mul_declare(uint32_t id, int permille)
{
    if (id > (uint32_t)CE_SDK_CARD_ID_MAX) return;
    s_score_mul[id] = permille > 0 ? permille : 0;
    int32_t sum = 0;
    for (int k = 0; k <= CE_SDK_CARD_ID_MAX; ++k) sum += s_score_mul[k];
    s_score_mul_sum = sum;
}

int ce_score_mul_aggregate(void) { return s_score_mul_sum; }


/* ---- 「本帧已消过弹」闩锁 ---- */
static int s_cancel_latch_owner;

void ce_cancel_latch_reset(void) { s_cancel_latch_owner = -1; }

int ce_cancel_latch_claim(uint32_t id)
{
    if (id > (uint32_t)CE_SDK_CARD_ID_MAX) return 0;
    if (s_cancel_latch_owner >= 0) return 0;
    s_cancel_latch_owner = (int)id;
    return 1;
}

int ce_cancel_latch_owner(void) { return s_cancel_latch_owner; }


/* ---- 低速子机声明层（选举 id 最小者）---- */
static ce_option_pose_t s_optpose[CE_SDK_CARD_ID_MAX + 1];
static int s_optpose_winner = -1;

void ce_optpose_reset(void)
{
    for (int id = 0; id <= CE_SDK_CARD_ID_MAX; ++id) {
        s_optpose[id].count = 0;
        s_optpose[id].anm_script = 0;
        s_optpose[id].fx = s_optpose[id].fy = 0.0f;
        s_optpose[id].ux = s_optpose[id].uy = 0.0f;
    }
    s_optpose_winner = -1;
}

void ce_optpose_declare(uint32_t id, int count, int anm_script, float fx, float fy, float ux, float uy)
{
    if (id > (uint32_t)CE_SDK_CARD_ID_MAX) return;
    ce_option_pose_t *p = &s_optpose[id];
    p->count = count > 0 ? count : 0;
    p->anm_script = anm_script;
    p->fx = fx; p->fy = fy; p->ux = ux; p->uy = uy;
    s_optpose_winner = -1;                       /* 重新选举：id 最小且 count > 0 的那张 */
    for (int i = 0; i <= CE_SDK_CARD_ID_MAX; ++i) {
        if (s_optpose[i].count > 0) { s_optpose_winner = i; break; }
    }
}

int ce_optpose_winner(void) { return s_optpose_winner; }

const ce_option_pose_t *ce_optpose_pose(void)
{
    if (s_optpose_winner < 0) return NULL;
    return &s_optpose[s_optpose_winner];
}


/* ---- HUD 槽位分配器 ---- */
static struct {
    int    used;
    int    reserved;        /* 局中 release 后保留占位（不参与重排）*/
    uint32_t id;
    float  w, h;
    int    side;
    float  x, y;
} s_hud[CE_SDK_CARD_ID_MAX + 1];
static int s_hud_near_corner;

static void hud_relayout(void)
{
    /* 底部货架（HUD-LAYOUT §2）：按 card_id 升序扫描（数组按 id 索引 ⇒ 天然有序），两侧各自算。
     * 左列：cursor 从左锚点向右；右列：块的**右边缘**贴在 cursor 上、cursor 向左走。*/
    float cx[2], row_bottom[2], row_h[2];
    cx[0] = CE_HUD_ANCHOR_X;         row_bottom[0] = CE_HUD_ANCHOR_Y; row_h[0] = 0.0f;
    cx[1] = CE_HUD_ANCHOR_X_RIGHT;   row_bottom[1] = CE_HUD_ANCHOR_Y; row_h[1] = 0.0f;

    for (int id = 0; id <= CE_SDK_CARD_ID_MAX; ++id) {
        if (!s_hud[id].used) continue;
        if (s_hud[id].side == CE_HUD_SIDE_PINNED) continue;        /* 自带锚点：不参与货架排布 */
        int side = (s_hud[id].side == CE_HUD_SIDE_RIGHT) ? 1 : 0;
        int fits;
        if (side == 0) fits = (cx[0] + s_hud[id].w <= CE_HUD_X_MAX);
        else           fits = (cx[1] - s_hud[id].w >= CE_HUD_X_MIN);
        /* 这一行放不下 ⇒ 上抬一行。空行上的「超大块」不抬（认领时已过 §4.2 硬约束），
         * 否则单块就会被推到一条没意义的第 2 行上。*/
        if (!fits && row_h[side] > 0.0f) {
            row_bottom[side] -= row_h[side] + CE_HUD_GAP;
            cx[side] = (side == 0) ? CE_HUD_ANCHOR_X : CE_HUD_ANCHOR_X_RIGHT;
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
        s_hud[id].x = s_hud[id].y = 0.0f;
    }
    s_hud_near_corner = 0;
}

/* 把一块登记进表（不动锚点），再按 side 决定要不要重排。返回 0 = 参数非法。
 * `anchor_x/y` 只在 pinned 时用；`do_relayout` 由调用方给（pinned 不动别的位置）。*/
static int hud_register(uint32_t card_id, const ce_hud_block_t *blk,
                        float anchor_x, float anchor_y, int pinned, ce_hud_slot_t *out)
{
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
    s_hud[card_id].id = card_id;
    s_hud[card_id].w = blk->w;
    s_hud[card_id].h = blk->h;
    s_hud[card_id].side = blk->side;
    if (pinned) { s_hud[card_id].x = anchor_x; s_hud[card_id].y = anchor_y; }
    else        { hud_relayout(); }
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
    return hud_register(card_id, blk, anchor_x, anchor_y, 1, out);
}

int ce_hud_slot_claim(uint32_t card_id, const ce_hud_block_t *blk, ce_hud_slot_t *out)
{
    return hud_register(card_id, blk, 0.0f, 0.0f, 0, out);
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
