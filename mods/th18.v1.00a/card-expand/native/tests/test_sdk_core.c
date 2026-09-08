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
    ce_behavior_t a = { 58, vt_a, "ctor", 0, 0 }, b = { 60, vt_b, "on_tick_2", 0, 0 }, dup = { 58, vt_b, "x", 0, 0 };
    uint32_t error_id = 0;
    int error_reason = 0;
    CHECK(!ce_sdk_registration_error(&error_id, &error_reason));
    CHECK(ce_sdk_register(&a));
    CHECK(ce_sdk_register(&b));
    CHECK(!ce_sdk_register(&dup));                 /* 同 id 两次 = 编程错误 */
    CHECK(ce_sdk_registration_error(&error_id, &error_reason));
    CHECK(error_id == 58 && error_reason == CE_SDK_REG_DUPLICATE);
    CHECK(ce_sdk_behavior_count() == 2);
    CHECK(ce_sdk_find(58) == &a);
    CHECK(ce_sdk_find(60)->vtable == vt_b);
    CHECK(ce_sdk_find(59) == NULL);
    CHECK(ce_sdk_find(0) == NULL);

    ce_sdk_reset_for_test();
    ce_behavior_t low = { 57, vt_a, "", 0, 0 }, high = { 255, vt_b, "", 0, 0 };
    CHECK(!ce_sdk_register(&low));
    CHECK(ce_sdk_registration_error(&error_id, &error_reason));
    CHECK(error_id == 57 && error_reason == CE_SDK_REG_ID_RANGE);
    ce_sdk_reset_for_test();
    CHECK(!ce_sdk_registration_error(&error_id, &error_reason));
    CHECK(!ce_sdk_register(&high));
    CHECK(ce_sdk_registration_error(&error_id, &error_reason));
    CHECK(error_id == 255 && error_reason == CE_SDK_REG_ID_RANGE);

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
    ce_behavior_t a = { 58, vt_a, "", 0, 0 }, b = { 60, vt_b, "", 0, 0 };
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

static void test_contract_check(void)
{
    uint32_t bad = 0;
    int reason = 0;
    uint32_t ids[] = { 58, 59 };
    uint32_t passive_categories[] = { 2, 1 };

    ce_sdk_reset_for_test();
    CHECK(ce_sdk_contract_check(ids, passive_categories, 2, &bad, &reason));

    uint32_t active_categories[] = { 0, 1 };
    CHECK(!ce_sdk_contract_check(ids, active_categories, 2, &bad, &reason));
    CHECK(bad == 58 && reason == CE_SDK_CONTRACT_ACTIVE_BEHAVIOR_REQUIRED);

    ce_behavior_t active = { 58, vt_a, "active", 0, 1 };
    ce_sdk_register(&active);
    CHECK(ce_sdk_contract_check(ids, active_categories, 2, &bad, &reason));
    CHECK(!ce_sdk_contract_check(ids, passive_categories, 2, &bad, &reason));
    CHECK(bad == 58 && reason == CE_SDK_CONTRACT_ACTIVE_CATEGORY_REQUIRED);

    ce_sdk_reset_for_test();
    ce_behavior_t passive = { 58, vt_a, "tick", 0, 0 };
    ce_sdk_register(&passive);
    CHECK(!ce_sdk_contract_check(ids, active_categories, 2, &bad, &reason));
    CHECK(bad == 58 && reason == CE_SDK_CONTRACT_ACTIVE_BEHAVIOR_REQUIRED);
    CHECK(ce_sdk_contract_check(ids, passive_categories, 2, &bad, &reason));
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

    /* 2026-09-08：股市（id 80）要 430 多字节（17 根 OHLC + 33 个 anm id），块提到 512 */
    int k4 = 0;
    CHECK(CE_STATE_BYTES >= 512);
    unsigned char *big = ce_state_user(&k4, 480);
    CHECK(big != NULL);
    big[479] = 1;
    CHECK(((unsigned char *)ce_state_get(&k4))[CE_STATE_RESERVED + 479] == 1);
    ce_state_free(&k4);

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


/* ---- 自机弹伤害倍率层（AUDIT §AA）---- */
static void test_dmg_layer(void)
{
    ce_dmg_reset();
    CHECK(ce_dmg_aggregate() == CE_DMG_UNIT);          /* 没有贡献者 = 中性 */
    CHECK(ce_dmg_applier() == -1);
    CHECK(ce_dmg_scale(100) == 100);

    ce_dmg_set(61, 1100);                              /* K♠ */
    CHECK(ce_dmg_aggregate() == 1100);
    CHECK(ce_dmg_applier() == 61);
    CHECK(ce_dmg_scale(100) == 110);

    ce_dmg_set(76, 2000);                              /* 叠上古锭刀的终符 */
    CHECK(ce_dmg_aggregate() == 2200);
    CHECK(ce_dmg_applier() == 61);                     /* 落笔人永远是 id 最小的那张 */
    CHECK(ce_dmg_scale(90) == 198);                    /* 90 × 2.2，单次取整 */

    /* 三层 +20% 加法叠（衔尾蛇）：单次取整 90 × 1.6 = 144。
     * 逐张各自取整会走成 90 → 108 → 129 → 154 —— 这层要消灭的就是这个漂移。 */
    ce_dmg_reset();
    ce_dmg_set(78, 1600);
    CHECK(ce_dmg_scale(90) == 144);
    CHECK(90 * 12 / 10 * 12 / 10 * 12 / 10 == 154);    /* 对照：旧写法确实漂 10 点 */

    /* 登记顺序不影响聚合，也不影响落笔人 */
    ce_dmg_reset();
    ce_dmg_set(78, 1600); ce_dmg_set(61, 1100);
    CHECK(ce_dmg_applier() == 61);
    CHECK(ce_dmg_aggregate() == 1760);

    /* 卡被摧毁 / 局末置 0 = 退出聚合，落笔人顺延 */
    ce_dmg_set(61, 0);
    CHECK(ce_dmg_applier() == 78);
    CHECK(ce_dmg_aggregate() == 1600);
    ce_dmg_set(78, 0);
    CHECK(ce_dmg_applier() == -1);
    CHECK(ce_dmg_aggregate() == CE_DMG_UNIT);

    /* 中性值 1000 也算在场（古锭刀平时就是它），得能当落笔人 */
    ce_dmg_reset();
    ce_dmg_set(76, CE_DMG_UNIT);
    CHECK(ce_dmg_applier() == 76);
    CHECK(ce_dmg_aggregate() == CE_DMG_UNIT);
    CHECK(ce_dmg_scale(90) == 90);

    /* 越界 id 不写坏任何东西 */
    ce_dmg_reset();
    ce_dmg_set(9999, 2000);
    CHECK(ce_dmg_aggregate() == CE_DMG_UNIT);
    CHECK(ce_dmg_applier() == -1);
    ce_dmg_reset();
}

int main(void)
{
    test_dmg_layer();
    test_interest_and_pick();
    test_judgment();
    test_royal();
    test_registry();
    test_bind_check();
    test_contract_check();
    test_state();
    printf("test_sdk_core: %d passed, %d failed\n", s_pass, s_fail);
    return s_fail ? 1 : 0;
}
