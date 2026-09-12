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


/* ---- 2026-09-12 新增的四层（EVENTS-CONTRACT.md）：无敌取最大 / 计分声明 / 消弹闩锁 / 低速子机选举 ---- */
static void test_declare_layers(void)
{
    /* 无敌：取最大值；并列取 id 最小；退出声明后回落 */
    ce_invuln_reset();
    CHECK(ce_invuln_frames() == 0 && ce_invuln_applier() == -1);
    ce_invuln_request(117, 40);
    CHECK(ce_invuln_frames() == 40 && ce_invuln_applier() == 117);
    ce_invuln_request(107, 40);                        /* 同值 ⇒ 并列取 id 最小 */
    CHECK(ce_invuln_frames() == 40 && ce_invuln_applier() == 107);
    ce_invuln_request(117, 280);                       /* 更长者胜 */
    CHECK(ce_invuln_frames() == 280 && ce_invuln_applier() == 117);
    ce_invuln_request(117, 0);                         /* 退出 ⇒ 回落 */
    CHECK(ce_invuln_frames() == 40 && ce_invuln_applier() == 107);

    /* 计分：求和 + id 最小者落笔 */
    ce_score_reset();
    CHECK(ce_score_aggregate() == 0 && ce_score_applier() == -1);
    ce_score_declare(111, 25000);
    ce_score_declare(110, 50000);
    CHECK(ce_score_aggregate() == 75000);
    CHECK(ce_score_applier() == 110);                  /* 110 < 111 */
    ce_score_declare(110, 0);                          /* 110 退出 ⇒ 111 落笔 */
    CHECK(ce_score_aggregate() == 25000 && ce_score_applier() == 111);

    /* 消弹闩锁：本帧只有第一个认领者拿得到 */
    ce_cancel_latch_reset();
    CHECK(ce_cancel_latch_owner() == -1);
    CHECK(ce_cancel_latch_claim(107) == 1);
    CHECK(ce_cancel_latch_claim(117) == 0);            /* 已被 107 认领 */
    CHECK(ce_cancel_latch_owner() == 107);

    /* 「这次死亡由谁救」闩锁（用户 2026-09-12 定案：救援层顺序 107 → 117 → 决死 B → 113）。
     * 107/117 **无条件**报到（两家同时触发是定案），113 只在没人认领时才动手。*/
    ce_rescue_note_reset();
    CHECK(ce_rescue_note_owner() == -1);
    CHECK(ce_rescue_note_claim(107) == 1);             /* 107 先报到 */
    CHECK(ce_rescue_note_owner() == 107);
    CHECK(ce_rescue_note_claim(117) == 0);             /* 117 照样救，但拿不到账 */
    CHECK(ce_rescue_note_owner() == 107);              /* 归属不变 ⇒ 113 会看到「有人救了」*/
    CHECK(ce_rescue_note_claim(113) == 0);             /* ⇒ 113 放弃（不烧槽、不排重新死亡）*/
    ce_rescue_note_reset();
    CHECK(ce_rescue_note_owner() == -1);
    CHECK(ce_rescue_note_claim(113) == 1);             /* 前面三家都没动手 ⇒ 轮到 113 */
    ce_rescue_note_reset();
    CHECK(ce_rescue_note_claim(999) == 0);             /* 越界 id：不记、返回 0 */
    CHECK(ce_rescue_note_owner() == -1);

    /* 低速子机声明：id 最小者胜；退出后第二张顶上 */
    ce_optpose_reset();
    CHECK(ce_optpose_winner() == -1 && ce_optpose_pose() == NULL);
    ce_optpose_declare(111, 2, 30, 20.0f, 0.0f, 30.0f, 0.0f);
    CHECK(ce_optpose_winner() == 111);
    ce_optpose_declare(108, 4, 17, 12.0f, 0.0f, 18.0f, 0.0f);
    CHECK(ce_optpose_winner() == 108);                 /* 108 < 111 ⇒ 妖机体覆盖 SA 子机层 */
    CHECK(ce_optpose_pose()->count == 4 && ce_optpose_pose()->anm_script == 17);
    ce_optpose_declare(108, 0, 0, 0.0f, 0.0f, 0.0f, 0.0f);
    CHECK(ce_optpose_winner() == 111 && ce_optpose_pose()->count == 2);
}


/* ---- 2026-09-12 掉落层（先加后乘、乘作用最终总数）---- */
static void test_drop_layer(void)
{
    /* z = zEnemyDrop 基址：[0] main_drop、[1..20] ex_drop、[21..40] timed_ex_drop */
    int32_t z[41];

    /* 无贡献者 ⇒ 一个字节都不碰 */
    ce_drop_reset();
    memset(z, 0, sizeof z);
    z[0] = 1; z[1] = 1;
    ce_drop_apply(z);
    CHECK(z[1] == 1);

    /* ×2：主掉落那一颗已经撒了 ⇒ final = (1 + 1) × 2 = 4 ⇒ 写回 3
     * （★「别的卡/附加表加的那颗也被翻倍」= 用户 2026-09-12 的裁决）*/
    ce_drop_reset();
    ce_drop_mult_set(69, 2000);
    CHECK(ce_drop_mult_aggregate() == 2000);
    memset(z, 0, sizeof z);
    z[0] = 1; z[1] = 1;
    ce_drop_apply(z);
    CHECK(z[1] == 3);
    /* 对照：旧实现（只翻附加表、主掉落 +1）给 1×2+1 = 3 ⇒ 写回 2。差一位就是这次改动的意义。*/

    /* 无主掉落（z[0] == 0）⇒ 不做「已撒一颗」补偿 */
    ce_drop_reset(); ce_drop_mult_set(69, 2000);
    memset(z, 0, sizeof z);
    z[0] = 0; z[1] = 1;
    ce_drop_apply(z);
    CHECK(z[1] == 2);

    /* timed 表同步乘（引擎按 ex[i] + timed[i]×(T−timer)/T 折算 ⇒ 对乘数线性） */
    ce_drop_reset(); ce_drop_mult_set(69, 2000);
    memset(z, 0, sizeof z);
    z[0] = 0;
    z[1 + CE_DROP_TYPES + 0] = 5;                     /* timed[type 1] = 5 */
    z[1 + CE_DROP_TYPES + 18] = 3;                    /* timed[type 19] = 3 */
    z[1 + CE_DROP_TYPES + 19] = 7;                    /* type 20：引擎不撒，但层照乘（无害） */
    z[1 + CE_DROP_TYPES + 5] = -4;                    /* 负数 = 引擎直接跳过 ⇒ 层不动它 */
    ce_drop_apply(z);
    CHECK(z[1 + CE_DROP_TYPES + 0] == 10);
    CHECK(z[1 + CE_DROP_TYPES + 18] == 6);
    CHECK(z[1 + CE_DROP_TYPES + 19] == 14);
    CHECK(z[1 + CE_DROP_TYPES + 5] == -4);
    /* timed 表**不**做「已撒一颗」补偿：main == 1 也只乘 */
    ce_drop_reset(); ce_drop_mult_set(69, 2000);
    memset(z, 0, sizeof z);
    z[0] = 1; z[1] = 4;
    z[1 + CE_DROP_TYPES + 0] = 4;
    ce_drop_apply(z);
    CHECK(z[1] == 9);                                 /* (4 + 1) × 2 = 10 ⇒ 写回 9 */
    CHECK(z[1 + CE_DROP_TYPES + 0] == 8);             /* 4 × 2，无补偿 */

    /* 多个乘数连乘（2000‰ × 1500‰ = 3000‰） */
    ce_drop_reset();
    ce_drop_mult_set(69, 2000);
    ce_drop_mult_set(70, 1500);
    CHECK(ce_drop_mult_aggregate() == 3000);
    /* 退出声明后回落 ×1 */
    ce_drop_mult_set(70, 0);
    CHECK(ce_drop_mult_aggregate() == 2000);
    ce_drop_mult_set(69, 0);
    CHECK(ce_drop_mult_aggregate() == CE_DROP_MULT_UNIT);

    /* 主掉落类型超出引擎会撒的范围（> 19）⇒ 不补偿 */
    ce_drop_reset(); ce_drop_mult_set(69, 2000);
    memset(z, 0, sizeof z);
    z[0] = 20; z[1] = 1;
    ce_drop_apply(z);
    CHECK(z[1] == 2);

    /* 负计数（引擎当 0 处理）⇒ 钳 0，不写出负数 */
    ce_drop_reset(); ce_drop_mult_set(69, 2000);
    memset(z, 0, sizeof z);
    z[0] = 0; z[1] = -5;
    ce_drop_apply(z);
    CHECK(z[1] == 0);

    /* 空指针 ⇒ 直接返回（断点里 drop 可能为 0） */
    ce_drop_apply(NULL);
}

/* ---- 2026-09-12 HUD 槽位分配器（HUD-LAYOUT.md §2/§3/§4）---- */
static void test_hud_slots(void)
{
    ce_hud_slot_t a, b, again;
    ce_hud_block_t blk   = { 108.0f, 36.0f, CE_HUD_SIDE_LEFT };
    ce_hud_block_t blk2  = { 108.0f, 36.0f, CE_HUD_SIDE_LEFT };

    /* 首块：贴左锚点、坐底边，640 空间给同一点 */
    ce_hud_slots_reset();
    CHECK(!ce_hud_slot_claimed(111));
    CHECK(ce_hud_slot_claim(111, &blk, &a));
    CHECK(a.x == CE_HUD_ANCHOR_X && a.y == CE_HUD_ANCHOR_Y);
    CHECK(a.x640 == CE_HUD_ANCHOR_X * CE_HUD_640_SCALE);
    CHECK(a.y640 == CE_HUD_ANCHOR_Y * CE_HUD_640_SCALE);
    CHECK(a.w == 108.0f && a.h == 36.0f && a.side == CE_HUD_SIDE_LEFT);

    /* 底部货架（§2）：按 card_id 升序**从左往右**排 ⇒ 108 落在 111 左边、同一行。
     * ⚠ 认领那一刻填的坐标会被同批次后来的认领重排 ⇒ 用 getter 拿最新值。*/
    CHECK(ce_hud_slot_claim(108, &blk2, &b));
    CHECK(ce_hud_slot_get(108, &b) && ce_hud_slot_get(111, &a));
    CHECK(b.x == CE_HUD_ANCHOR_X);
    CHECK(a.x == CE_HUD_ANCHOR_X + blk2.w + CE_HUD_GAP);   /* 257 + 108 + 8 = 373 */
    CHECK(a.y == b.y);                                     /* 同一行（y 向下增长，底边同值） */

    /* 幂等（§4.4）：同一张卡重复认领返回同一个槽（on_load 每关都会再调） */
    CHECK(ce_hud_slot_claim(111, &blk, &again));
    CHECK(again.x == a.x && again.y == a.y);

    /* 排不下 ⇒ 上抬一行；游标**单向**（抬行后不回填上一行的空档，照 §2 伪码）——
     * 108(36高) 坐底行、109 放不下 ⇒ 行 2 底边 924−36−8 = 880、
     * 111 又放不下行 2（965+108 > 1000）⇒ 行 3 底边 880−20−8 = 852。*/
    ce_hud_block_t mid = { 700.0f, 20.0f, CE_HUD_SIDE_LEFT };
    CHECK(ce_hud_slot_claim(109, &mid, &again));
    CHECK(ce_hud_slot_get(108, &b) && ce_hud_slot_get(109, &again) && ce_hud_slot_get(111, &a));
    CHECK(b.x == CE_HUD_ANCHOR_X && b.y == CE_HUD_ANCHOR_Y);
    CHECK(again.x == CE_HUD_ANCHOR_X && again.y == CE_HUD_ANCHOR_Y - blk.h - CE_HUD_GAP);
    CHECK(a.x == CE_HUD_ANCHOR_X && a.y == CE_HUD_ANCHOR_Y - blk.h - mid.h - 2.0f * CE_HUD_GAP);

    /* 局中删卡不当场重排（§4.4）：release 保留占位、别人坐标不变 */
    ce_hud_slot_release(108);
    CHECK(ce_hud_slot_claimed(108));                              /* 仍算已认领 */
    CHECK(ce_hud_slot_get(111, &again) && again.x == a.x && again.y == a.y);
    CHECK(ce_hud_slot_get(108, &again) && again.x == CE_HUD_ANCHOR_X);
    /* 关卡开场 reset 才真正收回 */
    ce_hud_slots_reset();
    CHECK(!ce_hud_slot_claimed(108) && !ce_hud_slot_claimed(109) && !ce_hud_slot_claimed(111));

    /* 硬约束（§4.2）：块右边缘不许越过 1040 ⇒ 声明宽度超限直接拒绝 */
    ce_hud_block_t too_wide = { CE_HUD_X_HARD_MAX - CE_HUD_ANCHOR_X + 1.0f, 36.0f, CE_HUD_SIDE_LEFT };
    CHECK(!ce_hud_slot_claim(110, &too_wide, &a));
    /* 越软上限 1000 但仍合法 ⇒ 单独占一行（空行上的超大块不抬行） */
    ce_hud_slots_reset();
    ce_hud_block_t greed = { 780.0f, 36.0f, CE_HUD_SIDE_LEFT };   /* 257+780 ≤ 硬上限 1040 */

    CHECK(ce_hud_slot_claim(110, &greed, &a));
    CHECK(a.x == CE_HUD_ANCHOR_X && a.y == CE_HUD_ANCHOR_Y);
    ce_hud_block_t next = { 100.0f, 36.0f, CE_HUD_SIDE_LEFT };
    CHECK(ce_hud_slot_claim(111, &next, &b));
    CHECK(b.x == CE_HUD_ANCHOR_X);                                /* 1045 + 100 > 1000 ⇒ 抬行 */
    CHECK(b.y == CE_HUD_ANCHOR_Y - greed.h - CE_HUD_GAP);
    ce_hud_block_t bad = { 0.0f, 36.0f, CE_HUD_SIDE_LEFT };
    CHECK(!ce_hud_slot_claim(110, &bad, &a));                     /* 尺寸非法 */
    CHECK(!ce_hud_slot_claim(112, &next, NULL));                 /* out == NULL */
    CHECK(!ce_hud_slot_claim(CE_SDK_CARD_ID_MAX + 1u, &next, &a)); /* id 非法 */
    CHECK(!ce_hud_slot_get(CE_SDK_CARD_ID_MAX + 1u, &a));
    CHECK(!ce_hud_slot_get(107, &a));                             /* 没认领过 */

    /* 右列：从右锚点**向左**排，块右边缘贴游标（§6.2 ❓ 无先例卡） */
    ce_hud_slots_reset();
    ce_hud_block_t right  = { 100.0f, 20.0f, CE_HUD_SIDE_RIGHT };
    ce_hud_block_t right2 = { 150.0f, 20.0f, CE_HUD_SIDE_RIGHT };
    CHECK(ce_hud_slot_claim(113, &right, &a));
    CHECK(a.x == CE_HUD_ANCHOR_X_RIGHT - right.w && a.y == CE_HUD_ANCHOR_Y);
    CHECK(ce_hud_slot_claim(112, &right2, &b));
    CHECK(ce_hud_slot_get(112, &b) && ce_hud_slot_get(113, &a));
    CHECK(b.x == CE_HUD_ANCHOR_X_RIGHT - right2.w);               /* 112 先排、贴右锚点 */
    CHECK(a.x == CE_HUD_ANCHOR_X_RIGHT - right2.w - CE_HUD_GAP - right.w);
    CHECK(a.y == b.y);


    /* 自带锚点（CE_HUD_SIDE_PINNED，117）：按量过的位置登记，且**不参与货架排布** */
    ce_hud_slots_reset();
    {
        ce_hud_block_t pin = { 193.0f, 48.0f, CE_HUD_SIDE_PINNED };
        ce_hud_slot_t p1, p2;
        ce_hud_block_t shelf = { 108.0f, 36.0f, CE_HUD_SIDE_LEFT };
        CHECK(ce_hud_slot_claim_pinned(117, &pin, 88.0f, 924.0f, &p1));
        CHECK(p1.x == 88.0f && p1.y == 924.0f);            /* 锚点原样 */
        CHECK(p1.x640 == 44.0f && p1.y640 == 462.0f);      /* 640 空间同步 */
        /* 货架卡照旧从锚点 257 起（pinned 不占它的位、也不被它顶动）*/
        CHECK(ce_hud_slot_claim(111, &shelf, &p2));
        CHECK(p2.x == CE_HUD_ANCHOR_X && p2.y == CE_HUD_ANCHOR_Y);
        CHECK(ce_hud_slot_get(117, &p1) && p1.x == 88.0f && p1.y == 924.0f);
        /* 重复认领幂等 */
        CHECK(ce_hud_slot_claim_pinned(117, &pin, 88.0f, 924.0f, &p1) && p1.x == 88.0f);
        /* side 不符 ⇒ 拒绝（两个入口互相把关）*/
        CHECK(!ce_hud_slot_claim(117, &pin, &p1));         /* pinned 走 claim → 拒 */
        CHECK(!ce_hud_slot_claim_pinned(111, &shelf, 88.0f, 924.0f, &p1));  /* 非 pinned 走 pinned → 拒 */
        /* 越硬上限也拒 */
        ce_hud_block_t wide = { 1000.0f, 20.0f, CE_HUD_SIDE_PINNED };
        CHECK(!ce_hud_slot_claim_pinned(117, &wide, 88.0f, 924.0f, &p1));
    }

    /* 关卡纪元：同一关不重排；关卡号一变 ⇒ 先清空再认领（HUD-LAYOUT §4.4）*/
    ce_hud_slots_reset();
    /* 模拟「认领前推进纪元」：直接调 core 层的 stage_begin（sdk.h 的 wrapper 会带 CE_CURRENT_STAGE）*/
    ce_hud_stage_begin(3);
    CHECK(ce_hud_stage_current() == 3);
    CHECK(ce_hud_slot_claim(108, &blk, &a));
    CHECK(ce_hud_slot_claim(111, &blk, &b));
    CHECK(ce_hud_slot_claimed(108) && ce_hud_slot_claimed(111));
    ce_hud_stage_begin(3);                                  /* 同一关重复推进：不动 */
    CHECK(ce_hud_slot_claimed(108) && ce_hud_slot_claimed(111));
    ce_hud_slot_release(108);                               /* 局中删卡：占位保留 */
    CHECK(ce_hud_slot_claimed(108));
    ce_hud_stage_begin(4);                                  /* 下一关：全部收敛、重新认领 */
    CHECK(!ce_hud_slot_claimed(108) && !ce_hud_slot_claimed(111));
    CHECK(ce_hud_slot_claim(111, &blk, &a));
    CHECK(a.x == CE_HUD_ANCHOR_X);                          /* 重排后 111 从锚点起（108 已不在）*/
    ce_hud_stage_begin(-1);
    CHECK(!ce_hud_slot_claimed(111));

    /* 计分层第二通道：乘数附加项（EVENTS-CONTRACT §3.1，110×111 同装时落笔者读它） */
    ce_score_reset();
    CHECK(ce_score_mul_aggregate() == 0);
    ce_score_mul_declare(111, 500);                       /* 信号强度 0.50 ⇒ 500‰ */
    CHECK(ce_score_mul_aggregate() == 500);
    ce_score_mul_declare(110, 250);                       /* 两张卡都声明 ⇒ 求和 */
    CHECK(ce_score_mul_aggregate() == 750);
    ce_score_mul_declare(110, 0);                         /* 退出声明 */
    CHECK(ce_score_mul_aggregate() == 500);
    ce_score_mul_declare(111, -3);                        /* 负数按 0 处理 */
    CHECK(ce_score_mul_aggregate() == 0);
    ce_score_mul_declare(111, 1234);
    ce_score_declare(111, 50000);
    CHECK(ce_score_mul_aggregate() == 1234);
    CHECK(ce_score_aggregate() == 50000);                 /* 两个通道互不干扰 */
    /* 非法 id 不改聚合 */
    ce_score_mul_declare(CE_SDK_CARD_ID_MAX + 1u, 999);
    CHECK(ce_score_mul_aggregate() == 1234);
    /* reset 两个通道一起清 */
    ce_score_reset();
    CHECK(ce_score_mul_aggregate() == 0 && ce_score_aggregate() == 0);

    /* 淡出栅格 + 迟滞（TH16 同款）：★ 每帧只推进一次（SDK 调 update），卡只读 near_corner() */
    ce_hud_slots_reset();
    CHECK(ce_hud_near_corner() == 0);
    ce_hud_near_corner_update(-100.0f, 500.0f);            /* 进栅格 ⇒ 淡出 */
    CHECK(ce_hud_near_corner() == 1);
    ce_hud_near_corner_update(-100.0f, 390.0f);            /* y 回到 384..400 之间 ⇒ 迟滞保持 */
    CHECK(ce_hud_near_corner() == 1);
    ce_hud_near_corner_update(-100.0f, 383.0f);            /* y < 384 ⇒ 淡回 */
    CHECK(ce_hud_near_corner() == 0);
    ce_hud_near_corner_update(-100.0f, 500.0f);            /* 再进 */
    CHECK(ce_hud_near_corner() == 1);
    ce_hud_near_corner_update(-10.0f, 500.0f);             /* x > −64 ⇒ 淡回 */
    CHECK(ce_hud_near_corner() == 0);
    /* 兼容入口 = update + read */
    CHECK(ce_hud_player_near_corner(-100.0f, 500.0f) == 1);
    CHECK(ce_hud_near_corner() == 1);

    /* 淡出栅格 + 迟滞（TH16 同款） */
    ce_hud_slots_reset();
    CHECK(!ce_hud_player_near_corner(-10.0f, 500.0f));
    CHECK(ce_hud_player_near_corner(-100.0f, 500.0f));   /* x < −64 && y > 400 ⇒ 淡出 */
    CHECK(ce_hud_player_near_corner(-100.0f, 390.0f));   /* 迟滞：还没到恢复条件 */
    CHECK(!ce_hud_player_near_corner(-100.0f, 380.0f));  /* y < 384 ⇒ 淡回 */
}

int main(void)
{
    test_dmg_layer();
    test_declare_layers();
    test_drop_layer();
    test_hud_slots();
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
