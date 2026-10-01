/* elytra_mace_core.c —— 鞘翅重锤的时机条状态机（规则与对位表在 elytra_mace_core.h）。
 * 只用加减乘除，零 sin/cos、零 x87、零引擎依赖。 */
#include <string.h>

#include "elytra_mace_core.h"

void em_reset(em_state_t *s)
{
    memset(s, 0, sizeof *s);
    s->state = EM_IDLE;
    s->cursor = -EM_BAR_HALF;      /* 下次 em_begin 从这里出发 */
    s->dir = 1.0f;
}

void em_begin(em_state_t *s)
{
    if (!s) return;
    s->state = EM_AIM;
    s->frames = 0;
    s->no_target = 0;
    s->cursor = -EM_BAR_HALF;      /* 从左端开始往右扫（Undertale 那种观感）*/
    s->dir = 1.0f;
    s->armed = 0;                  /* ★ 激活这一帧的 C 边沿必须丢掉（否则当场自己确认）*/
}

int em_invuln_left(const em_state_t *s)
{
    if (!s) return 0;
    switch (s->state) {
    case EM_AIM:
        /* 瞄准剩余 + 砸击 + 缓冲：一次性声明足，卡侧每帧重声明 ⇒ 结束时自然掉 */
        return (int)(EM_AIM_FRAMES - s->frames) + EM_SMASH_FRAMES + EM_EXIT_INVULN;
    case EM_SMASH:
        return (int)(EM_SMASH_FRAMES - s->frames) + EM_EXIT_INVULN;
    case EM_EXIT:
        return (int)(EM_EXIT_INVULN - s->frames);
    default:
        return 0;
    }
}

int em_in_window(const em_state_t *s)
{
    if (!s) return 0;
    return (s->cursor >= -EM_WINDOW && s->cursor <= EM_WINDOW);
}

int em_tick(em_state_t *s, int has_target, int c_edge)
{
    if (!s) return EM_EV_NONE;

    switch (s->state) {
    case EM_AIM:
        s->frames++;
        if (has_target) {
            s->no_target = 0;
            /* 有敌人 ⇒ 时机条开始动（owner 口径）；没敌人就停着 */
            s->cursor += EM_CURSOR_SPEED * s->dir;
            if (s->cursor >= EM_BAR_HALF) { s->cursor = EM_BAR_HALF; s->dir = -1.0f; }
            if (s->cursor <= -EM_BAR_HALF) { s->cursor = -EM_BAR_HALF; s->dir = 1.0f; }
        } else {
            s->no_target++;
        }

        /* 第一帧只是"解锁"：不消费 C（那一帧的边沿属于引擎的发动）*/
        if (!s->armed) { s->armed = 1; return EM_EV_NONE; }

        if (c_edge && has_target) {
            if (em_in_window(s)) {
                s->hits++;
                s->state = EM_SMASH;
                s->frames = 0;
                return EM_EV_HIT;
            }
            s->misses++;
            s->state = EM_EXIT;
            s->frames = 0;
            return EM_EV_MISS;
        }
        if (!has_target && s->no_target >= EM_NO_TARGET_FRAMES) {
            s->timeouts++;
            s->state = EM_EXIT;
            s->frames = 0;
            return EM_EV_TIMEOUT;
        }
        if (s->frames >= EM_AIM_FRAMES) {
            s->timeouts++;
            s->state = EM_EXIT;
            s->frames = 0;
            return EM_EV_TIMEOUT;
        }
        return EM_EV_NONE;

    case EM_SMASH:
        s->frames++;
        if (s->frames >= EM_SMASH_FRAMES) {          /* 砸完 ⇒ 送回原位，然后进缓冲 */
            s->state = EM_EXIT;
            s->frames = 0;
            return EM_EV_RETURN;
        }
        return EM_EV_NONE;

    case EM_EXIT:
        s->frames++;
        if (s->frames >= EM_EXIT_INVULN) {
            s->state = EM_IDLE;
            s->frames = 0;
            return EM_EV_DONE;
        }
        return EM_EV_NONE;

    default:
        return EM_EV_NONE;
    }
}
