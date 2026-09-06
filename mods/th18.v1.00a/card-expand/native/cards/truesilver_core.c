/* truesilver_core.c —— 真银圣剑纯逻辑（说明见 .h）。 */
#include "truesilver_core.h"

void ts_stage_start(ts_state_t *s)
{
    s->durability = TS_DURABILITY;
    s->burst_left = 0;
}

int ts_on_bomb(ts_state_t *s)
{
    if (s->durability <= 0) return 0;
    s->durability--;
    s->swings++;
    s->burst_left = TS_BURST_FRAMES;
    return 1;
}

int ts_tick(ts_state_t *s)
{
    if (s->burst_left == 0) return 0;
    s->burst_left--;
    return TS_BURST_DMG;
}
