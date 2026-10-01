/* SDK「复制代数闸 + 可见数台账」的主机单测（Jera 88 的共享件；语义见 sdk_core.h 末节）。
 *   cc -std=c11 -Wall -Wextra -Werror -o t tests/test_dup_layer.c sdk_core.c cards/hud_dim.c && ./t
 * 为什么要单独测：这一层跨卡 —— 代数差一格会让**每一张有私有掉落池的卡**多复制一遍（或漏一遍），
 * 台账差一格会让 Jera 的「值不值得发动」判错。纯逻辑，与引擎无关。 */
#include <assert.h>
#include <stdio.h>

#include "../sdk_core.h"

static int fails;
#define CHECK(cond, ...) do { if (!(cond)) { printf("FAIL %s:%d ", __FILE__, __LINE__); \
    printf(__VA_ARGS__); printf("\n"); fails++; } } while (0)

static void test_generation(void)
{
    uint32_t g0;

    ce_dup_reset();
    g0 = ce_dup_gen();
    CHECK(g0 != 0, "代数不该是 0（0 是生产者的「还没见过」哨兵）");
    CHECK(ce_dup_bump() == g0 + 1, "bump 该 +1");
    CHECK(ce_dup_gen() == g0 + 1, "gen 该读到新值");
    CHECK(ce_dup_bump() == g0 + 2, "bump 该单调");
}

static void test_should_run(void)
{
    uint32_t seen = 0;

    ce_dup_reset();
    /* 第一次调用只登记（否则开局那一刻会误触发一次全场复制）*/
    CHECK(!ce_dup_should_run(&seen), "第一次调用不该触发");
    CHECK(seen == ce_dup_gen(), "第一次该把 seen 设成当前代数");
    CHECK(!ce_dup_should_run(&seen), "没人发动时不该触发");
    CHECK(!ce_dup_should_run(&seen), "同上（连续多帧）");

    ce_dup_bump();
    CHECK(ce_dup_should_run(&seen), "发动后该触发一次");
    CHECK(!ce_dup_should_run(&seen), "同一次发动只该触发一次（下一帧不再触发）");

    ce_dup_bump();
    ce_dup_bump();                       /* 两帧内发动两次（理论上不会，但状态机要稳）*/
    CHECK(ce_dup_should_run(&seen), "第二次发动该再触发");
    CHECK(!ce_dup_should_run(&seen), "触发后回到不触发");

    /* 空指针 = 不触发、不崩 */
    CHECK(!ce_dup_should_run(0), "NULL 该安全返回 0");
}

static void test_vis_table(void)
{
    ce_dup_reset();
    CHECK(ce_dup_vis_total() == 0, "空台账该是 0");
    CHECK(ce_dup_producers() == 0, "空台账没有生产者");

    CHECK(ce_dup_vis_set(108, 3), "登记 108");
    CHECK(ce_dup_vis_set(116, 0), "登记 116（0 也要占位：它是「在场但场上没有」）");
    CHECK(ce_dup_vis_total() == 3, "合计该是 3，得到 %d", ce_dup_vis_total());
    CHECK(ce_dup_producers() == 2, "两个生产者");

    CHECK(ce_dup_vis_set(108, 5), "同一张卡重复上报 = 覆盖");
    CHECK(ce_dup_vis_total() == 5, "覆盖后该是 5，得到 %d", ce_dup_vis_total());
    CHECK(ce_dup_producers() == 2, "重复上报不该多占一格");

    ce_dup_vis_drop(108);
    CHECK(ce_dup_vis_total() == 0, "删卡后该扣掉，得到 %d", ce_dup_vis_total());
    CHECK(ce_dup_producers() == 1, "删卡后只剩一个生产者");
    ce_dup_vis_drop(108);                       /* 幂等 */
    CHECK(ce_dup_producers() == 1, "重复删卡不该出问题");
    ce_dup_vis_drop(116);
    CHECK(ce_dup_producers() == 0, "全删光");

    /* 槽满：多出来的登不上（返回 0），但已登记的读数不受影响 */
    {
        int i;
        for (i = 0; i < CE_DUP_SLOT_MAX + 4; ++i) ce_dup_vis_set((uint32_t)(200 + i), 1);
        CHECK(ce_dup_producers() == CE_DUP_SLOT_MAX, "该占满 %d 格，得到 %d",
              CE_DUP_SLOT_MAX, ce_dup_producers());
        CHECK(ce_dup_vis_total() == CE_DUP_SLOT_MAX, "合计该是 %d", CE_DUP_SLOT_MAX);
        CHECK(!ce_dup_vis_set(999, 1), "槽满该返回 0");
        ce_dup_vis_drop(999);
        CHECK(ce_dup_producers() == CE_DUP_SLOT_MAX, "登不上的 id 删了也不影响别人");
    }

    /* 负数按 0 处理（防调用方算错把台账弄负）*/
    ce_dup_reset();
    ce_dup_vis_set(112, -5);
    CHECK(ce_dup_vis_total() == 0, "负数该钳成 0，得到 %d", ce_dup_vis_total());
    /* id = 0 是哨兵，不接受 */
    CHECK(!ce_dup_vis_set(0, 3), "id 0 该拒绝");
}

int main(void)
{
    test_generation();
    test_should_run();
    test_vis_table();
    printf(fails ? "dup_layer: %d FAILED\n" : "dup_layer: ok\n", fails);
    return fails ? 1 : 0;
}
