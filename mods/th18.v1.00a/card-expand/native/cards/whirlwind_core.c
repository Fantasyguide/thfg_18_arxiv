/* whirlwind_core.c —— 旋风斩纯逻辑（说明见 .h）。 */
#include "whirlwind_core.h"

void ww_clear(ww_state_t *s)
{
    s->left = 0;
    s->chaining = 0;
    s->waited = 0;
}

int ww_extra(int power, int per_level)
{
    int x;
    if (per_level <= 0 || power <= 0) return 0;
    x = power / per_level - 1;
    if (x < 0) return 0;
    return x > WW_MAX_EXTRA ? WW_MAX_EXTRA : x;
}

int ww_on_bomb(ww_state_t *s, int power, int per_level)
{
    int x;
    if (s->chaining) return 0;                 /* 我们接出来的那几发不再算 X、不再花火力 */
    x = ww_extra(power, per_level);
    if (!x) return 0;
    s->left = (uint8_t)x;
    s->waited = 0;
    return x;
}

int ww_tick(ww_state_t *s, int bombing)
{
    if (bombing) return 0;                     /* 上一发还在放 —— 引擎自己也会拒绝重入 */
    if (!s->left) { s->chaining = 0; return 0; }   /* 都放完了：下一发真炸弹重新算 X */
    if (++s->waited > WW_GIVE_UP) { ww_clear(s); return 0; }
    s->chaining = 1;                           /* 必须在 do_bomb 之前 */
    return 1;
}

void ww_fired(ww_state_t *s, int ok)
{
    if (!ok) return;                           /* 被守卫拦下：欠账留着，下一帧再试 */
    if (s->left) s->left--;
    s->chained++;
    s->waited = 0;
}
