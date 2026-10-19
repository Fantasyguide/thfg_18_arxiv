/* 118「挑战・十二合一」的发放计划（纯逻辑；语义与一手出处见 `give12_core.h` 的头注释）。
 * ⚠ 本文件**不碰引擎**：不读内存、不打日志、不调 `ce_give_card` ⇒ 能直接链进主机单测 ✓。*/

#include "give12_core.h"

int ce_give12_should_give(const int32_t *owned, unsigned owned_len, uint32_t id)
{
    if (id < CE_GIVE12_FIRST || id > CE_GIVE12_LAST) return 0;   /* 区间外：本卡不管 */
    if (!owned) return 1;                                        /* 拿不到管理器 ⇒ 不挡（老行为）*/
    if (id >= owned_len) return 1;                               /* 上界外不读（同 `ce_owned()` 口径 ✓）*/
    return owned[id] == 0;                                       /* 已持有 ⇒ 不发 ✓（0x411460 不会替你判 ✗）*/
}

unsigned ce_give12_plan(const int32_t *owned, unsigned owned_len, uint32_t *out, unsigned cap)
{
    unsigned n = 0;
    uint32_t id;

    if (!out || !cap) return 0;
    for (id = CE_GIVE12_FIRST; id <= CE_GIVE12_LAST; ++id) {
        if (!ce_give12_should_give(owned, owned_len, id)) continue;
        if (n >= cap) break;
        out[n++] = id;
    }
    return n;
}
