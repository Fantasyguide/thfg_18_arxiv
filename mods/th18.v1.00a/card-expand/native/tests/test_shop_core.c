/* test_shop_core.c —— shop_core.c 的主机单测（make test-host）。 */
#include <stdio.h>
#include "../shop_core.h"

static int s_fail, s_pass;
#define CHECK(cond) do { if (cond) ++s_pass; else { ++s_fail; printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } } while (0)

/* 原生流程：MSG 开店 → 买 → 30 帧后关店 → 同一帧 GameThread 看到指针为 0 → 重开一次；第二次买完不再开 */
static void test_two_visits(void)
{
    ce_shop_state_t s; ce_shop_reset(&s, 2);
    int st = 1, f = 5000;
    CHECK(!ce_shop_on_gamethread(&s, 1, 0, st, f, 0));      /* MSG 置位那一帧：放行，不额外做事 */
    for (int i = 0; i < 100; ++i) CHECK(!ce_shop_on_gamethread(&s, 0, 1, st, ++f, 0));   /* 店开着 */
    ce_shop_on_bought(&s, st, f);
    for (int i = 0; i < 30; ++i) CHECK(!ce_shop_on_gamethread(&s, 0, 1, st, ++f, 0));    /* 成交后 30 帧退场动画 */
    CHECK(ce_shop_on_gamethread(&s, 0, 0, st, ++f, 0));      /* 指针清零的同一帧 → 重开 */
    CHECK(s.visits_left == 0 && s.bought == 0);
    for (int i = 0; i < 50; ++i) CHECK(!ce_shop_on_gamethread(&s, 0, 1, st, ++f, 0));
    ce_shop_on_bought(&s, st, f);
    for (int i = 0; i < 30; ++i) CHECK(!ce_shop_on_gamethread(&s, 0, 1, st, ++f, 0));
    CHECK(!ce_shop_on_gamethread(&s, 0, 0, st, ++f, 0));     /* 第二家关门：不再开 */
    CHECK(!ce_shop_on_gamethread(&s, 0, 0, st, ++f, 0));
}

/* 没成交就关门（练习 / replay 的 30 帧自动退；空白卡路线）→ 不重开 */
static void test_no_purchase_no_reopen(void)
{
    ce_shop_state_t s; ce_shop_reset(&s, 2);
    CHECK(!ce_shop_on_gamethread(&s, 1, 0, 1, 100, 0));
    for (int i = 0; i < 30; ++i) CHECK(!ce_shop_on_gamethread(&s, 0, 1, 1, 101 + i, 0));
    CHECK(!ce_shop_on_gamethread(&s, 0, 0, 1, 131, 0));
    CHECK(s.visits_left == 1);                               /* 名额还在，但没人成交就没人用 */
}

/* 成交后中途退出（回标题）：换了关卡 / 帧数不连续 → 陈旧成交作废，新一局不会凭空开店 */
static void test_stale_bought_is_discarded(void)
{
    ce_shop_state_t s; ce_shop_reset(&s, 2);
    CHECK(!ce_shop_on_gamethread(&s, 1, 0, 3, 4000, 0));
    ce_shop_on_bought(&s, 3, 4010);
    CHECK(!ce_shop_on_gamethread(&s, 0, 0, 1, 0, 0));        /* 新一局第 1 关第 0 帧 */
    CHECK(s.bought == 0);
    CHECK(!ce_shop_on_gamethread(&s, 0, 0, 1, 1, 0));

    ce_shop_reset(&s, 2);
    CHECK(!ce_shop_on_gamethread(&s, 1, 0, 3, 4000, 0));
    ce_shop_on_bought(&s, 3, 4010);
    CHECK(!ce_shop_on_gamethread(&s, 0, 0, 3, 4010 + CE_SHOP_REOPEN_WINDOW + 1, 0));   /* 同关但超窗 */
    CHECK(s.bought == 0);
}

/* MSG 再次置位（下一关）会重置名额 */
static void test_next_stage_resets(void)
{
    ce_shop_state_t s; ce_shop_reset(&s, 2);
    CHECK(!ce_shop_on_gamethread(&s, 1, 0, 1, 100, 0));
    ce_shop_on_bought(&s, 1, 110);
    CHECK(ce_shop_on_gamethread(&s, 0, 0, 1, 141, 0));
    ce_shop_on_bought(&s, 1, 200);
    CHECK(!ce_shop_on_gamethread(&s, 0, 0, 1, 231, 0));
    CHECK(!ce_shop_on_gamethread(&s, 1, 0, 2, 100, 0));      /* 第 2 关 MSG */
    CHECK(s.visits_left == 1 && s.bought == 0);
    ce_shop_on_bought(&s, 2, 110);
    CHECK(ce_shop_on_gamethread(&s, 0, 0, 2, 141, 0));
}

/* 成交后暂停很久：暂停时 GameThread 提前返回、TIME_IN_STAGE 不走，帧号仍连续 → 300 帧窗口的前提成立 */
static void test_pause_after_purchase(void)
{
    ce_shop_state_t s; ce_shop_reset(&s, 2);
    CHECK(!ce_shop_on_gamethread(&s, 1, 0, 1, 100, 0));
    ce_shop_on_bought(&s, 1, 110);
    /* 暂停 5000 个真实帧：本断点根本不被调，TIME_IN_STAGE 停在 110..140 之间 */
    CHECK(ce_shop_on_gamethread(&s, 0, 0, 1, 141, 0));
    CHECK(s.last_bought_frame == 110);
}

/* blocked（练习 / replay 回放）：即使有成交记录也不开 */
static void test_blocked(void)
{
    ce_shop_state_t s; ce_shop_reset(&s, 2);
    CHECK(!ce_shop_on_gamethread(&s, 1, 0, 1, 100, 1));
    ce_shop_on_bought(&s, 1, 110);
    CHECK(!ce_shop_on_gamethread(&s, 0, 0, 1, 141, 1));
}

/* visits = 1 就是原生 */
static void test_one_visit_is_retail(void)
{
    ce_shop_state_t s; ce_shop_reset(&s, 1);
    CHECK(!ce_shop_on_gamethread(&s, 1, 0, 1, 100, 0));
    ce_shop_on_bought(&s, 1, 110);
    CHECK(!ce_shop_on_gamethread(&s, 0, 0, 1, 141, 0));
    ce_shop_reset(&s, 0);                                    /* 非法值钳到 1 */
    CHECK(s.visits == 1);
}

int main(void)
{
    test_two_visits();
    test_no_purchase_no_reopen();
    test_stale_bought_is_discarded();
    test_next_stage_resets();
    test_pause_after_purchase();
    test_blocked();
    test_one_visit_is_retail();
    printf("test_shop_core: %d passed, %d failed\n", s_pass, s_fail);
    return s_fail ? 1 : 0;
}
