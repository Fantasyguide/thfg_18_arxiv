/* animal_core.c —— animal_core.h 的实现。纯逻辑，无引擎依赖。 */
#include "animal_core.h"

static int32_t clamp_i32(int32_t v, int32_t lo, int32_t hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

int32_t animal_hyper_duration(int32_t hyper_type, int32_t majority_count)
{
    if (hyper_type < ANIMAL_TYPE_GOAST_MIN || hyper_type > ANIMAL_TYPE_GOAST_MAX)
        return 1140;                       /* 混合：TH17 §5.2「都不足 3 → type 4、duration 1140」*/

    switch (clamp_i32(majority_count, 3, 5)) {
    case 3:  return 840;                   /* 表 `0x4a13d0` */
    case 4:  return 1140;                  /* 表 `0x4a13d4` */
    default: return 1500;                  /* 表 `0x4a13d8`（count 5）*/
    }
}

int animal_ready_compute(animal_core_t *st)
{
    int32_t count[3] = {0, 0, 0};
    int32_t best_type = 0, best_n = 0;
    int i;

    if (!st || st->fill < ANIMAL_SLOT_MAX) return 0;

    for (i = 0; i < ANIMAL_SLOT_MAX; ++i) {
        int32_t t = st->slots[i];
        if (animal_type_is_animal(t)) ++count[t - 1];
    }
    for (i = 0; i < 3; ++i) {
        if (count[i] > best_n) { best_n = count[i]; best_type = i + 1; }
    }

    if (best_n >= 3) {
        st->hyper_type = best_type;                        /* 暴走咆哮 */
        st->duration = animal_hyper_duration(best_type, best_n);
        /* TH17 `th17:0x40fbe5`：`cmp esi,[0x4b59f8]`（esi = 多数类的下标 0..2、0x4b59f8 = 自机凭依）
         * → `th17:0x40fbed` `add eax,0x1e0` = **+480 帧**。混合理由那条路不走（`th17:0x40fc40` 直接 0x474）。*/
        if (st->goast == ANIMAL_GOAST_ANY || best_type - 1 == st->goast)
            st->duration += ANIMAL_GOAST_BONUS;
    } else {
        st->hyper_type = 4;                                /* 混合 */
        st->duration = animal_hyper_duration(4, 0);
    }

    /* 释放等级 = 队列里**最后一个** 8..14 的 type−7（TH17 `0x40ff5e` 清 / `0x40ff64` 起置高半字节）。
     * 本卡不做稀有动物，恒 0；字段留给以后。 */
    st->release_level = 0;
    for (i = ANIMAL_SLOT_MAX - 1; i >= 0; --i) {
        int32_t t = st->slots[i];
        if (t >= ANIMAL_TYPE_RARE_MIN && t <= ANIMAL_TYPE_RARE_MAX) {
            st->release_level = t - 7;
            break;
        }
    }

    st->extends_used = 0;                                  /* 新窗口：延长表从头数 */
    st->ready = 1;
    st->time_left = st->duration;
    return 1;
}

int animal_collect(animal_core_t *st, int32_t type)
{
    if (!st) return 0;
    if (type < ANIMAL_TYPE_MIN || type > ANIMAL_TYPE_MAX) return 0;

    if (st->hypering) {
        if (st->pending_count < ANIMAL_PENDING_MAX)
            st->pending[st->pending_count++] = type;
        return 2;                          /* 溢出：进 pending（TH17 `+0xa0`）*/
    }

    if (st->fill < ANIMAL_SLOT_MAX) {
        st->slots[st->fill++] = type;
        if (st->fill >= ANIMAL_SLOT_MAX) animal_ready_compute(st);
        return 0;
    }

    /* 队列已满、还没进霊撃 */
    if (animal_type_is_fish(type)) return 3;               /* 鱼灵：调用方立刻换道具（TH17 `FUN_004108d0`）*/

    if (st->ready) {                                       /* 基本动物灵：延长窗口（TH17 `FUN_0040f980`）*/
        st->time_left += animal_next_extend(st);
        if (st->time_left > st->duration) st->time_left = st->duration;
    }
    return 1;
}

int32_t animal_next_extend(animal_core_t *st)
{
    /* 用户口径：1.5 s / 1.0 s / 0.7 s（= 90 / 60 / 42 帧），第 4 次起递减（×2/3 取整）。
     * TH17 代码是 `extend = 180; if (extend > 30) extend = extend*2/3;` ⇒ 120/80/53/…（🟡 两说并存）。*/
    static const int32_t tbl[ANIMAL_EXTEND_COUNT] = {90, 60, 42, 28, 18, 12, 8, 5, 3, 2, 1};
    int32_t i = st ? st->extends_used : 0;

    if (i < 0) i = 0;
    if (i >= ANIMAL_EXTEND_COUNT) i = ANIMAL_EXTEND_COUNT - 1;
    if (st) st->extends_used = i + 1;
    return tbl[i];
}

int animal_ready_tick(animal_core_t *st)
{
    if (!st || !st->ready || st->hypering) return 0;
    if (st->time_left > 0) --st->time_left;
    if (st->time_left <= 0) {
        st->time_left = 0;
        st->ready = 0;                     /* 只报一次；调用方随后 clear_and_restore */
        return 1;                          /* 超时 → 正常结束结算 */
    }
    return 0;
}

int animal_hyper_start(animal_core_t *st, int32_t by_hit)
{
    if (!st || !st->ready || st->hypering) return 0;
    st->hypering = 1;
    st->by_hit = by_hit ? 1 : 0;
    st->ready = 0;
    st->time_left = ANIMAL_HYPER_FRAMES;
    return 1;
}

int animal_hyper_tick(animal_core_t *st)
{
    if (!st || !st->hypering) return 0;
    if (st->time_left > 0) --st->time_left;
    if (st->time_left <= 0) {
        st->time_left = 0;
        st->hypering = 0;
        return 1;
    }
    return 0;
}

int animal_clear_and_restore(animal_core_t *st)
{
    int32_t n, i;

    if (!st) return 0;

    st->fill = 0;
    st->ready = 0;
    st->hypering = 0;
    st->by_hit = 0;
    st->hyper_type = 0;
    st->duration = 0;
    st->time_left = 0;
    st->release_level = 0;
    st->extends_used = 0;
    for (i = 0; i < ANIMAL_SLOT_MAX; ++i) st->slots[i] = 0;

    n = st->pending_count;
    st->pending_count = 0;
    for (i = 0; i < n && st->fill < ANIMAL_SLOT_MAX; ++i)
        st->slots[st->fill++] = st->pending[i];
    for (i = 0; i < ANIMAL_PENDING_MAX; ++i) st->pending[i] = 0;

    if (st->fill >= ANIMAL_SLOT_MAX) return animal_ready_compute(st);
    return 0;
}

void animal_hyper_radius(int32_t time_left, int32_t total, float *out)
{
    float f;

    if (!out) return;
    if (total <= 0) { *out = 0.0f; return; }
    if (time_left < 0) time_left = 0;
    if (time_left > total) time_left = total;
    f = (float)time_left / (float)total;
    *out = (1.0f - f * f * f * f) * ANIMAL_RADIUS_MAX;
}

int animal_fish_in_queue(const animal_core_t *st, int32_t *out, int32_t cap)
{
    int32_t n = 0, i;

    if (!st || !out || cap <= 0) return 0;
    for (i = 0; i < ANIMAL_SLOT_MAX; ++i) {
        if (animal_type_is_fish(st->slots[i])) {
            if (n < cap) out[n] = st->slots[i];
            ++n;
        }
    }
    return n < cap ? (int)n : cap;
}

int animal_end_burst(int32_t *out, int32_t cap, int32_t roar_type, uint32_t (*rand)(void))
{
    uint32_t seq = 0;
    int32_t n = 0, k;
    /* 单一兽灵暴走（1..3）→ 用户口径：2 只都是**本次咆哮那一只**；
     * 其余（混合 4 / 未知 0）→ TH17 一手回退：`1 + rand()%3`（= 15..17 的 base 型）。
     * ⚠ 单一型这条是**用户覆盖**（一手 TH17 是随机），混合这条才是一手。 */
    int single = (roar_type >= ANIMAL_TYPE_GOAST_MIN && roar_type <= ANIMAL_TYPE_GOAST_MAX);

    if (!out || cap <= 0) return 0;
    for (k = 0; k < 2; ++k) {
        uint32_t r = rand ? rand() : seq++;
        int32_t t = single ? roar_type : (int32_t)(1 + r % 3);
        if (n < cap) out[n] = t;
        ++n;
    }
    return n < cap ? (int)n : cap;
}

int32_t animal_next_type(int32_t type)
{
    if (type >= 1 && type <= 3) return type % 3 + 1;   /* 1→2→3→1（TH17 §4.3）*/
    return type;
}
