/* ice_cream_core.c —— 冰淇淋的纯逻辑（语义与一手对位表在 ice_cream_core.h）。零引擎依赖。 */
#include <string.h>

#include "ice_cream_core.h"

void ic_reset(ic_state_t *s)
{
    memset(s, 0, sizeof *s);
}

int ic_cache(ic_state_t *s, int alive_normal, int32_t power, int32_t bombs)
{
    if (!s || !alive_normal) return 0;      /* 死亡 / 决死态里**不刷**快照（见头注 ①）*/
    s->power = power;
    s->bombs = bombs;
    s->have = 1;
    return 1;
}

void ic_note_miss(ic_state_t *s)
{
    if (s) s->miss = 1;
}

int ic_apply_after_miss(ic_state_t *s, int32_t max_bombs, int32_t *out_power, int32_t *out_bombs)
{
    int32_t bombs;

    if (!s || !s->miss) return 0;
    s->miss = 0;                            /* 一次性：同一次 miss 只写一遍 */
    if (!s->have) return 0;                 /* 还没采到快照（买卡当帧就死）⇒ 不动人家的值 */
    bombs = s->bombs + IC_BOMB_BONUS;
    if (bombs > max_bombs) bombs = max_bombs;   /* 引擎处处钳 MAX_BOMBS，我们同口径 */
    if (bombs < 0) bombs = 0;
    if (out_power) *out_power = s->power;       /* 火力 = 生前的值（引擎刚扣掉一档）*/
    if (out_bombs) *out_bombs = bombs;
    return 1;
}
