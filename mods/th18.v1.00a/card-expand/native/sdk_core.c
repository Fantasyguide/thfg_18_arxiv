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
