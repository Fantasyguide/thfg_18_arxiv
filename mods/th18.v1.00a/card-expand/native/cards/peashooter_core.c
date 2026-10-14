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

/* ★ P40：电弹连锁的选目标（见 peashooter_core.h 的注释 ✓）——半径内、未访问过、最近的那个 ✓；
 *   没有合格的返回 -1 ✓。纯函数 ⇒ 主机单测覆盖 ✓（tests/test_peashooter_core.c）。*/
int pea_chain_pick(const float *xy, const uint32_t *id, int n, float cx, float cy, float r,
                   const uint32_t *visited, int nv)
{
    int i, k, best = -1;
    float best_d2;
    if (!xy || !id || n <= 0 || r <= 0.0f) return -1;
    best_d2 = r * r;
    for (i = 0; i < n; ++i) {
        float dx, dy, d2;
        int seen = 0;
        if (id[i] == 0) continue;                        /* 0 = 没 id ⇒ 不敢当目标 ✓ */
        for (k = 0; k < nv; ++k)
            if (visited && visited[k] == id[i]) { seen = 1; break; }
        if (seen) continue;                              /* 已连过的不再连 ✓（防成环 ✓）*/
        dx = xy[2 * i] - cx;
        dy = xy[2 * i + 1] - cy;
        d2 = dx * dx + dy * dy;
        if (d2 <= best_d2) { best_d2 = d2; best = i; }    /* `<=` ⇒ 等距取后一个（入参顺序稳定 ⇒ replay 稳 ✓）*/
    }
    return best;
}

/* ★ P42：电弧按敌人对去重（见 peashooter_core.h 注释 ✓；纯函数 ⇒ 主机单测覆盖 ✓）。*/
uint32_t pea_arc_key(uint32_t a, uint32_t b)
{
    uint32_t lo = (a < b) ? a : b, hi = (a < b) ? b : a;
    return (lo << 16) ^ (hi & 0xffffu);          /* 两个 id 各 16 位以内 ⇒ 无序对唯一 ✓（与顺序无关 ✓）*/
}

int pea_arc_find(const uint32_t *keys, int n, uint32_t key)
{
    int i;
    if (!keys || n <= 0) return -1;
    for (i = 0; i < n; ++i) if (keys[i] == key) return i;
    return -1;
}

int pea_arc_put(uint32_t *keys, uint32_t *vms, const uint8_t *alive, int n, uint32_t key, uint32_t vm)
{
    int i, free_i = -1;
    if (!keys || !vms || n <= 0) return -1;
    for (i = 0; i < n; ++i) {
        if (keys[i] == key) { vms[i] = vm; return i; }      /* 同对 ⇒ 复用那个槽（不新建 ✓）*/
        if (free_i < 0 && (!alive || !alive[i])) free_i = i;/* 记下第一个"已死"的槽 ✓ */
    }
    if (free_i < 0) return -1;                              /* 表满且无同对 ⇒ 宁可不建 ✓ */
    keys[free_i] = key; vms[free_i] = vm;
    return free_i;
}
