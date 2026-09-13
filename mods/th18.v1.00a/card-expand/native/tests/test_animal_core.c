/* test_animal_core.c —— animal_core.c 的主机单测（make test-host）。
 * 语义来源：TH17 v1.00b 一手（engine/animal/th17/01-animal-spirits.md）
 *   §5.2 就绪判定与时长表 840/1140/1500、混合 1140、多数类 == 自机凭依时 +480
 *   §5.3 霊撃 40 帧、半径 (1 − f⁴) × 200
 *   §5.4 结束：鱼灵换道具（`FUN_00410640`）；正常结束放出 2 个变色动物灵
 *   §4.3 寿命变形 type % 3 + 1
 * 覆盖：时长表 / 就绪多数与混合 / 释放等级 / 收集（队列 / 延长 / 鱼灵 / 溢出）/
 *       延长表 / 计时与超时 / 霊撃时间轴与半径 / 清空回灌 / 鱼灵抽取 / 结束爆散。 */
#include <stdio.h>
#include <string.h>

#include "../cards/animal_core.h"

static int s_fail, s_pass;
#define CHECK(cond) do { if (cond) ++s_pass; else { ++s_fail; printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } } while (0)

static void fill2(animal_core_t *st, const int32_t *types, int n, int32_t goast)
{
    int i;
    memset(st, 0, sizeof *st);
    st->goast = goast;                  /* 自机凭依（卡侧每帧从 `CE_ADDR_CHARACTER % 3` 写）*/
    for (i = 0; i < n; ++i) animal_collect(st, types[i]);
}

/* 默认凭依 = 鹫（下标 2）：下面所有「不带加成」的老断言都靠它不被 +480 打扰 */
static void fill(animal_core_t *st, const int32_t *types, int n) { fill2(st, types, n, 2); }

int main(void)
{
    animal_core_t st;
    int32_t out[8];
    float r;

    /* —— 时长表（TH17 `.rdata` 0x4a13cc..0x4a13d8）—— */
    CHECK(animal_hyper_duration(1, 3) == 840);
    CHECK(animal_hyper_duration(1, 4) == 1140);
    CHECK(animal_hyper_duration(1, 5) == 1500);
    CHECK(animal_hyper_duration(2, 3) == 840);       /* 表本身不含凭依加成（加成在 ready_compute）*/
    CHECK(animal_hyper_duration(3, 5) == 1500);
    CHECK(animal_hyper_duration(4, 5) == 1140);      /* 混合恒 1140 */
    CHECK(animal_hyper_duration(1, 2) == 840);       /* count 夹到 3 */
    CHECK(animal_hyper_duration(1, 99) == 1500);     /* count 夹到 5 */
    CHECK(animal_hyper_duration(0, 5) == 1140);      /* 非法 type 当混合 */

    /* —— 就绪判定：多数 ≥3 取该类型 —— */
    {
        int32_t t[5] = {1, 1, 1, 2, 3};
        fill(&st, t, 5);
        CHECK(st.fill == 5 && st.ready == 1);
        CHECK(st.hyper_type == 1 && st.duration == 840);
    }
    {
        int32_t t[5] = {2, 2, 2, 2, 1};
        fill(&st, t, 5);
        CHECK(st.hyper_type == 2 && st.duration == 1140);
    }
    {
        int32_t t[5] = {1, 2, 3, 4, 5};
        fill(&st, t, 5);
        CHECK(st.hyper_type == 4 && st.duration == 1140);   /* 都不到 3 → 混合 */
    }
    {
        int32_t t[5] = {1, 1, 2, 2, 2};
        fill(&st, t, 5);
        CHECK(st.hyper_type == 2 && st.duration == 840);
    }

    /* —— 凭依加成：多数类 == 自机凭依 → +480 帧（TH17 `th17:0x40fbed`）—— */
    {
        int32_t t[5] = {1, 1, 1, 2, 3};
        fill2(&st, t, 5, 0);                                     /* 凭依 = 狼，多数也是狼 */
        CHECK(st.hyper_type == 1 && st.duration == 840 + 480);
    }
    {
        int32_t t[5] = {3, 3, 3, 3, 3};
        fill2(&st, t, 5, 2);                                     /* 凭依 = 鹫，多数也是鹫 */
        CHECK(st.hyper_type == 3 && st.duration == 1500 + 480);
    }
    /* ★ 2026-09-11 用户定案：TH18 无凭依选择 ⇒ 默认「主凭依灵不存在」，任何暴走都不吃 +480 */
    {
        int32_t t[5] = {1, 1, 1, 2, 3};
        fill2(&st, t, 5, ANIMAL_GOAST_NONE);
        CHECK(st.hyper_type == 1 && st.duration == 840);
    }
    {
        int32_t t[5] = {3, 3, 3, 3, 3};
        fill2(&st, t, 5, ANIMAL_GOAST_NONE);
        CHECK(st.hyper_type == 3 && st.duration == 1500);
    }
    /* ★ 彩蛋：持有「弱肉强食之理」(25)/「鬼杰组长的恐吓」(34) ⇒ 主凭依灵恒存在 ⇒ 任何暴走都 +480 */
    {
        int32_t t[5] = {1, 1, 1, 2, 3};
        fill2(&st, t, 5, ANIMAL_GOAST_ANY);
        CHECK(st.hyper_type == 1 && st.duration == 840 + 480);
    }
    {
        int32_t t[5] = {2, 2, 2, 2, 2};
        fill2(&st, t, 5, ANIMAL_GOAST_ANY);
        CHECK(st.hyper_type == 2 && st.duration == 1500 + 480);
    }
    {
        int32_t t[5] = {1, 2, 3, 4, 5};                          /* 混合：ANY 也不加 */
        fill2(&st, t, 5, ANIMAL_GOAST_ANY);
        CHECK(st.hyper_type == 4 && st.duration == 1140);
    }
    {
        int32_t t[5] = {2, 2, 2, 1, 1};
        fill2(&st, t, 5, 0);                                     /* 凭依 = 狼，多数是獭 → 不加 */
        CHECK(st.hyper_type == 2 && st.duration == 840);
    }
    {
        int32_t t[5] = {1, 2, 3, 4, 5};
        fill2(&st, t, 5, 0);                                     /* 混合（type 4）不吃加成 */
        CHECK(st.hyper_type == 4 && st.duration == 1140);
    }

    /* —— 释放等级 = 最后一个 8..14 的 type−7（本卡不做稀有，字段留给以后）—— */
    {
        int32_t t[5] = {1, 1, 1, 8, 10};
        fill(&st, t, 5);
        CHECK(st.release_level == 3);
    }
    {
        int32_t t[5] = {14, 1, 1, 1, 1};
        fill(&st, t, 5);
        CHECK(st.release_level == 7);
    }
    {
        int32_t t[5] = {1, 2, 3, 4, 5};
        fill(&st, t, 5);
        CHECK(st.release_level == 0);
    }

    /* —— 收集：队列 / 延长 / 鱼灵立刻换 / 霊撃中溢出 —— */
    {
        int32_t t[5] = {1, 1, 1, 1, 1};
        fill(&st, t, 5);
        CHECK(st.time_left == st.duration);
        st.time_left = 100;                            /* 先压低，免得延长被 duration 钳掉 */
        CHECK(animal_collect(&st, 2) == 1);            /* 满 + 基本动物 → 延长 */
        CHECK(st.time_left == 190);                    /* 100 + 90（第一次延长）*/
        CHECK(animal_collect(&st, 3) == 1);
        CHECK(st.time_left == 250);                    /* +60 */
        CHECK(animal_collect(&st, 1) == 1);
        CHECK(st.time_left == 292);                    /* +42 */
        CHECK(animal_collect(&st, 99) == 0);           /* 非法 type 丢弃 */
        CHECK(st.fill == 5);
    }
    {
        int32_t t[5] = {1, 1, 1, 1, 1};
        fill(&st, t, 5);
        st.time_left = 100;
        CHECK(animal_collect(&st, 4) == 3);            /* 满 + 鱼灵 → 调用方立刻换道具 */
        CHECK(st.time_left == 100);                    /* 不延长 */
        CHECK(animal_collect(&st, 7) == 3);
    }
    {
        int32_t t[5] = {1, 1, 1, 1, 1};
        fill(&st, t, 5);
        CHECK(animal_hyper_start(&st, 0) == 1);
        CHECK(st.hypering == 1 && st.ready == 0);
        CHECK(animal_collect(&st, 7) == 2);            /* 霊撃中 → pending */
        CHECK(st.pending_count == 1 && st.pending[0] == 7);
        CHECK(animal_collect(&st, 6) == 2);
        CHECK(st.pending_count == 2);
    }

    /* —— 延长表（用户口径 90/60/42/…）—— */
    {
        memset(&st, 0, sizeof st);
        CHECK(animal_next_extend(&st) == 90);
        CHECK(animal_next_extend(&st) == 60);
        CHECK(animal_next_extend(&st) == 42);
        CHECK(animal_next_extend(&st) == 28);
        for (int i = 0; i < 20; ++i) animal_next_extend(&st);
        CHECK(animal_next_extend(&st) == 1);           /* 用完停在 1 */
    }

    /* —— 计时与超时 —— */
    {
        int32_t t[5] = {1, 1, 1, 1, 1};
        fill(&st, t, 5);
        st.time_left = 2;
        CHECK(animal_ready_tick(&st) == 0);
        CHECK(st.time_left == 1);
        CHECK(animal_ready_tick(&st) == 1);            /* 超时 */
        CHECK(st.time_left == 0 && st.ready == 0);
        CHECK(animal_ready_tick(&st) == 0);            /* 已超时，不再报 */
    }

    /* —— 霊撃时间轴：40 帧、半径 0→200 —— */
    {
        int32_t t[5] = {1, 1, 1, 1, 1};
        fill(&st, t, 5);
        CHECK(animal_hyper_start(&st, 0) == 1);
        CHECK(st.time_left == 40);
        CHECK(animal_hyper_start(&st, 0) == 0);        /* 已在霊撃中，不能再开 */
        animal_hyper_radius(40, 40, &r); CHECK(r == 0.0f);
        animal_hyper_radius(0, 40, &r);  CHECK(r == 200.0f);
        animal_hyper_radius(20, 40, &r); CHECK(r > 150.0f && r < 190.0f);   /* 1 − 0.5⁴ = 0.9375 → 187.5 */
        animal_hyper_radius(0, 0, &r);   CHECK(r == 0.0f);
        for (int i = 0; i < 39; ++i) CHECK(animal_hyper_tick(&st) == 0);
        CHECK(st.time_left == 1);
        CHECK(animal_hyper_tick(&st) == 1);
        CHECK(st.hypering == 0 && st.time_left == 0);
    }

    /* —— 结束：清队列 + 回灌 pending —— */
    {
        int32_t t[5] = {1, 1, 1, 1, 1};
        fill(&st, t, 5);
        animal_hyper_start(&st, 1);
        CHECK(st.by_hit == 1);
        animal_collect(&st, 4);
        animal_collect(&st, 5);
        CHECK(st.pending_count == 2);
        CHECK(animal_clear_and_restore(&st) == 0);
        CHECK(st.fill == 2 && st.slots[0] == 4 && st.slots[1] == 5);
        CHECK(st.pending_count == 0 && st.ready == 0);
    }
    {
        int32_t t[5] = {1, 1, 1, 1, 1};
        fill(&st, t, 5);
        animal_hyper_start(&st, 0);
        for (int i = 0; i < 5; ++i) animal_collect(&st, 1);
        CHECK(st.pending_count == 5);
        CHECK(animal_clear_and_restore(&st) == 1);
        CHECK(st.fill == 5 && st.ready == 1 && st.hyper_type == 1);
    }

    /* —— 鱼灵抽取（队列里的 4..7）—— */
    {
        int32_t t[5] = {1, 4, 2, 7, 3};
        fill(&st, t, 5);
        CHECK(animal_fish_in_queue(&st, out, 8) == 2);
        CHECK(out[0] == 4 && out[1] == 7);
        CHECK(animal_fish_in_queue(&st, out, 1) == 1);   /* cap 截断 */
        {
            int32_t t2[5] = {1, 2, 3, 1, 2};
            fill(&st, t2, 5);
            CHECK(animal_fish_in_queue(&st, out, 8) == 0);
        }
    }

    /* —— 正常结束爆散：2 个变色动物灵（1..3）—— */
    /* 单一暴走（1..3）= 用户口径：2 只都等于本次咆哮类型（不看 rand）*/
    memset(out, 0, sizeof out);
    CHECK(animal_end_burst(out, 8, 3, 0) == 2);
    CHECK(out[0] == 3 && out[1] == 3);
    memset(out, 0, sizeof out);
    CHECK(animal_end_burst(out, 8, 1, 0) == 2);
    CHECK(out[0] == 1 && out[1] == 1);
    /* 混合（4）/ 未知（0）= TH17 一手回退：1 + rand()%3（rand=NULL → 确定性轮转 1,2）*/
    memset(out, 0, sizeof out);
    CHECK(animal_end_burst(out, 8, 4, 0) == 2);
    CHECK(out[0] == 1 && out[1] == 2);
    memset(out, 0, sizeof out);
    CHECK(animal_end_burst(out, 8, 0, 0) == 2);
    CHECK(out[0] == 1 && out[1] == 2);
    /* cap 截断 */
    memset(out, 0, sizeof out);
    CHECK(animal_end_burst(out, 1, 0, 0) == 1);

    /* —— 寿命变形 —— */
    CHECK(animal_next_type(1) == 2);
    CHECK(animal_next_type(2) == 3);
    CHECK(animal_next_type(3) == 1);
    CHECK(animal_next_type(4) == 4);
    CHECK(animal_next_type(14) == 14);

    /* —— 分类 —— */
    CHECK(animal_type_is_animal(1) && animal_type_is_animal(3) && !animal_type_is_animal(4));
    CHECK(animal_type_is_fish(4) && animal_type_is_fish(7) && !animal_type_is_fish(3));

    printf("test_animal_core: %d passed, %d failed\n", s_pass, s_fail);
    return s_fail ? 1 : 0;
}
