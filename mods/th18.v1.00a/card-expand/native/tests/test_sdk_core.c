/* test_sdk_core.c —— sdk_core.c 的主机单测（make test-host）。 */
#include <stdio.h>
#include <string.h>
#include "../sdk_core.h"

static int s_fail, s_pass;
#define CHECK(cond) do { if (cond) ++s_pass; else { ++s_fail; printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } } while (0)

static int vt_a[21], vt_b[21];

static void test_registry(void)
{
    ce_sdk_reset_for_test();
    ce_behavior_t a = { 58, vt_a, "ctor", 0 }, b = { 60, vt_b, "on_tick_2", 0 }, dup = { 58, vt_b, "x", 0 };
    CHECK(ce_sdk_register(&a));
    CHECK(ce_sdk_register(&b));
    CHECK(!ce_sdk_register(&dup));                 /* 同 id 两次 = 编程错误 */
    CHECK(ce_sdk_behavior_count() == 2);
    CHECK(ce_sdk_find(58) == &a);
    CHECK(ce_sdk_find(60)->vtable == vt_b);
    CHECK(ce_sdk_find(59) == NULL);
    CHECK(ce_sdk_find(0) == NULL);

    /* 满：71 张 */
    ce_sdk_reset_for_test();
    static ce_behavior_t many[CE_SDK_MAX_BEHAVIORS + 1];
    for (unsigned i = 0; i <= CE_SDK_MAX_BEHAVIORS; ++i) { many[i].id = 58 + i; many[i].vtable = vt_a; many[i].slots = ""; }
    for (unsigned i = 0; i < CE_SDK_MAX_BEHAVIORS; ++i) CHECK(ce_sdk_register(&many[i]));
    CHECK(!ce_sdk_register(&many[CE_SDK_MAX_BEHAVIORS]));
}

static void test_bind_check(void)
{
    ce_sdk_reset_for_test();
    ce_behavior_t a = { 58, vt_a, "", 0 }, b = { 60, vt_b, "", 0 };
    ce_sdk_register(&a); ce_sdk_register(&b);
    uint32_t bad = 0; unsigned unbound = 99;

    uint32_t j1[] = { 58, 59, 60, 61 };              /* 59、61 无行为：允许 */
    CHECK(ce_sdk_bind_check(j1, 4, &bad, &unbound));
    CHECK(unbound == 2);

    uint32_t j2[] = { 58, 59 };                      /* 60 有行为但 JSON 没登记：FAIL */
    CHECK(!ce_sdk_bind_check(j2, 2, &bad, &unbound));
    CHECK(bad == 60);

    CHECK(ce_sdk_bind_check(NULL, 0, &bad, &unbound) == 0 && bad == 58);   /* 空 JSON、有行为 → 第一个就报 */

    ce_sdk_reset_for_test();                         /* 没有任何行为：任何 JSON 都过 */
    CHECK(ce_sdk_bind_check(j1, 4, &bad, &unbound) && unbound == 4);
}

static void test_state(void)
{
    int k1 = 0, k2 = 0;
    CHECK(ce_state_get(&k1) == NULL);
    unsigned char *p = ce_state_alloc(&k1, 16);
    CHECK(p != NULL);
    CHECK(p[0] == 0 && p[15] == 0);                  /* 首次清零 */
    p[3] = 7;
    CHECK(ce_state_alloc(&k1, 16) == p);             /* 幂等，同块 */
    CHECK(((unsigned char *)ce_state_get(&k1))[3] == 7);
    CHECK(ce_state_alloc(&k2, CE_STATE_BYTES + 1) == NULL);
    CHECK(ce_state_in_use() == 1);
    ce_state_free(&k1);
    CHECK(ce_state_get(&k1) == NULL);
    CHECK(ce_state_in_use() == 0);
    ce_state_free(&k1);                              /* 重复释放是空操作 */
    p = ce_state_alloc(&k1, 4);
    CHECK(p && p[3] == 0);                           /* 释放后再分配：重新清零 */
    ce_state_free(&k1);

    /* 块头预留：SDK 的主动卡状态机住块头，卡的私有状态从 CE_STATE_RESERVED 起；同键同块、一次 free */
    int k3 = 0;
    unsigned char *head = ce_state_alloc(&k3, 4);
    unsigned char *user = ce_state_user(&k3, 8);
    CHECK(user == head + CE_STATE_RESERVED);
    CHECK(ce_state_user(&k3, 8) == user);                          /* 幂等 */
    head[0] = 0xaa; user[0] = 0x55;
    CHECK(head[0] == 0xaa && head[CE_STATE_RESERVED] == 0x55);     /* 互不覆盖 */
    CHECK(ce_state_user(&k3, CE_STATE_BYTES - CE_STATE_RESERVED + 1) == NULL);
    CHECK(ce_state_in_use() == 1);
    ce_state_free(&k3);
    CHECK(ce_state_get(&k3) == NULL);

    /* 满 */
    static int keys[CE_STATE_SLOTS + 1];
    for (unsigned i = 0; i < CE_STATE_SLOTS; ++i) CHECK(ce_state_alloc(&keys[i], 8) != NULL);
    CHECK(ce_state_alloc(&keys[CE_STATE_SLOTS], 8) == NULL);
    for (unsigned i = 0; i < CE_STATE_SLOTS; ++i) ce_state_free(&keys[i]);
    CHECK(ce_state_in_use() == 0);
}

static void test_royal(void)
{
    static const uint32_t set[5] = { 58, 59, 60, 61, 62 };
    int32_t owned[255] = { 0 };
    CHECK(!ce_royal_flush_ready(owned, 58, set, 5));               /* 一张没有 */
    owned[59] = owned[60] = owned[61] = 1;
    CHECK(!ce_royal_flush_ready(owned, 58, set, 5));               /* 还差 62 */
    owned[62] = 1;
    CHECK(ce_royal_flush_ready(owned, 58, set, 5));                /* 58 是第五张 */
    CHECK(!ce_royal_flush_ready(owned, 63, set, 5));               /* 不在集合里的卡不触发 */
    owned[58] = 1;
    CHECK(ce_royal_flush_ready(owned, 62, set, 5));                /* 自己已 owned 也不影响（重复购买不可能，但判定不依赖它）*/
}

static void test_judgment(void)
{
    CHECK(ce_judgment_cost(0) == 0);
    CHECK(ce_judgment_cost(-1) == 0);
    CHECK(ce_judgment_cost(1) == 1);
    CHECK(ce_judgment_cost(2) == 1);
    CHECK(ce_judgment_cost(3) == 2);
    CHECK(ce_judgment_cost(4) == 2);
    CHECK(ce_judgment_cost(7) == 4);
}

static void test_interest_and_pick(void)
{
    CHECK(ce_interest(0, 20) == 0);
    CHECK(ce_interest(-50, 20) == 0);
    CHECK(ce_interest(4, 20) == 0);          /* 0.8 → 0 */
    CHECK(ce_interest(5, 20) == 1);
    CHECK(ce_interest(999, 20) == 199);      /* 199.8 → 199 */
    CHECK(ce_interest(1000, 20) == 200);
    CHECK(ce_interest(2147483647, 20) == 429496729);   /* 不溢出 */
    CHECK(ce_pick_index(123u, 0) == -1);
    CHECK(ce_pick_index(0u, 1) == 0);
    CHECK(ce_pick_index(7u, 3) == 1);
    CHECK(ce_pick_index(0xffffffffu, 5) == 0);          /* 4294967295 % 5 = 0 */
}

int main(void)
{
    test_interest_and_pick();
    test_judgment();
    test_royal();
    test_registry();
    test_bind_check();
    test_state();
    printf("test_sdk_core: %d passed, %d failed\n", s_pass, s_fail);
    return s_fail ? 1 : 0;
}
