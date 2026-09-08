#include "peashooter_core.h"

void pea_clear(pea_state_t *s)
{
    s->timer = 0;
    s->wait = 0;
    s->gat_left = 0;
    s->inflight = 0;
    s->bursts = 0;
    s->peas = 0;
    s->pending_wait = 1;
}

void pea_set_wait(pea_state_t *s, uint32_t rnd)
{
    s->wait = (uint32_t)PEA_WAIT_MIN + rnd % (uint32_t)PEA_WAIT_SPAN;
    s->pending_wait = 0;
}

void pea_shoot_frame(pea_state_t *s, pea_out_t *o)
{
    o->set = PEA_SET_NORMAL;
    o->timer = 0;
    o->fires = 0;
    o->rows = 0;
    o->burst_start = 0;
    o->burst_end = 0;
    if (s->pending_wait) return;                 /* 调用方还没喂随机数：这一帧什么也不打 */

    if (s->gat_left == 0 && s->wait == 0) {      /* 攒够了：本帧起开大 */
        s->gat_left = PEA_GAT_FRAMES;
        s->timer = 0;
        s->bursts++;
        o->burst_start = 1;
    }

    if (s->gat_left) {
        o->set = PEA_SET_GATLING;
        o->timer = s->timer;
        o->fires = (s->timer % (uint32_t)PEA_GAT_INTERVAL) == 0;
        o->rows = o->fires ? PEA_GAT_ROWS : 0;
        s->timer++;
        if (--s->gat_left == 0) {                /* 这是开大的最后一帧 */
            o->burst_end = 1;
            s->timer = 0;
            s->pending_wait = 1;                 /* 下一帧之前要一个新的随机等待 */
        }
    } else {
        o->set = PEA_SET_NORMAL;
        o->timer = s->timer;
        o->fires = (s->timer % (uint32_t)PEA_INTERVAL) == 0;
        o->rows = o->fires ? 1 : 0;
        s->timer++;
        s->wait--;                               /* 上面保证了此处 wait > 0 */
    }

    if (o->fires) {
        s->peas += (uint32_t)o->rows;
        s->inflight = PEA_INFLIGHT;
    }
}

void pea_frame(pea_state_t *s)
{
    if (s->inflight) s->inflight--;
}

int pea_needs_cap(const pea_state_t *s)
{
    return s->inflight != 0;
}
