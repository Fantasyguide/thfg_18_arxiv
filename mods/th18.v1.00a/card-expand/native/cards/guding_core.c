/* guding_core.c —— 古锭刀（id 76）的纯逻辑。见 guding_core.h。 */
#include "guding_core.h"
#include <string.h>

int gd_find_active_slot(const uint8_t *interrupts, unsigned n)
{
    for (unsigned i = 0; i < n; ++i) {
        const uint8_t *slot = interrupts + i * GD_SLOT_STRIDE;
        int32_t hp, t;
        memcpy(&hp, slot + GD_SLOT_HP, 4);
        memcpy(&t, slot + GD_SLOT_TIME, 4);
        if (hp > -1 && t > 0) return (int)i;
    }
    return -1;
}

/* 在最多 0x40 字节、可能无终止符的名字里找 "Dead" */
static int name_has_mark(const uint8_t *name)
{
    const size_t mark = sizeof(GD_FINAL_MARK) - 1;
    size_t len = 0;
    while (len < GD_SLOT_SUB_LEN && name[len]) ++len;
    if (len < mark) return 0;
    for (size_t i = 0; i + mark <= len; ++i)
        if (memcmp(name + i, GD_FINAL_MARK, mark) == 0) return 1;
    return 0;
}

int gd_slot_is_final(const uint8_t *slot)
{
    return name_has_mark(slot + GD_SLOT_SUB_TIMEOUT) || name_has_mark(slot + GD_SLOT_SUB_LIFE);
}

int gd_update(gd_state_t *s, int final_now)
{
    int was = s->active;
    s->active = final_now ? 1 : 0;
    if (s->active) ++s->frames;
    if (s->active && !was) return 1;
    if (!s->active && was) return -1;
    return 0;
}

void gd_count_bullet(gd_state_t *s)
{
    if (s->active) ++s->bullets;
}
