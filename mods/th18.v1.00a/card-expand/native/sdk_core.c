/* sdk_core.c —— 见 sdk_core.h。纯 C11。 */
#include <string.h>
#include "sdk_core.h"

/* ---- 注册表 ---- */
static const ce_behavior_t *s_beh[CE_SDK_MAX_BEHAVIORS];
static unsigned s_nbeh;

int ce_sdk_register(const ce_behavior_t *b)
{
    if (s_nbeh >= CE_SDK_MAX_BEHAVIORS) return 0;
    for (unsigned i = 0; i < s_nbeh; ++i) if (s_beh[i]->id == b->id) return 0;
    s_beh[s_nbeh++] = b;
    return 1;
}

unsigned ce_sdk_behavior_count(void) { return s_nbeh; }
const ce_behavior_t *ce_sdk_behavior_at(unsigned i) { return i < s_nbeh ? s_beh[i] : (const ce_behavior_t *)0; }

const ce_behavior_t *ce_sdk_find(uint32_t id)
{
    for (unsigned i = 0; i < s_nbeh; ++i) if (s_beh[i]->id == id) return s_beh[i];
    return (const ce_behavior_t *)0;
}

void ce_sdk_reset_for_test(void) { s_nbeh = 0; }

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
