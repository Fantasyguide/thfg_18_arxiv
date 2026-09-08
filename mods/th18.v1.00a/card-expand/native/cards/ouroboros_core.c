#include "ouroboros_core.h"

void ob_reset(ob_state_t *s)
{
    s->stacks = 0;
    s->need = 1;
    s->progress = 0;
    s->misses = 0;
}

int ob_on_miss(ob_state_t *s)
{
    if (!s->need) ob_reset(s);              /* 没初始化过的零状态：need = 0 是不可能的合法值 */
    ++s->misses;
    ++s->progress;
    if (s->progress < s->need) return 0;
    s->progress = 0;
    if (s->stacks >= OB_MAX_STACKS) return 0;   /* 到顶：门槛也不再涨，省得溢出 */
    ++s->stacks;
    ++s->need;
    return 1;
}

int ob_permille(const ob_state_t *s)
{
    return OB_BASE_PERMILLE + (int)s->stacks * OB_STEP_PERMILLE;
}

uint32_t ob_remaining(const ob_state_t *s)
{
    uint32_t need = s->need ? s->need : 1;
    return need > s->progress ? need - s->progress : 0;
}
