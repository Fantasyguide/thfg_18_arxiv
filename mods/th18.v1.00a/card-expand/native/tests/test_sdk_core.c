/* test_sdk_core.c —— sdk_core.c 的主机单测（make test-host）。 */
#include <stdio.h>
#include <string.h>
#include "../sdk_core.h"
#include "../cards/hud_dim.h"   /* ★ 2026-09-22：货架让位那条不变式要读 HUD_CARD_* 一手常量 */

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

    /* 满：`CE_SDK_MAX_BEHAVIORS` 张。
     * ★ 2026-09-19：MAX_BEHAVIORS 现在是**导出量**（= id 空间 58..254 = 197。旧值 71 来自图鉴
     *   条目数两处 `cmp r,imm8` 的符号界，那两处已改成无符号字节比较）⇒「容量」与「id 范围」
     *   两条护栏正好重合：填满全部合法 id 之后，第 198 次登记必被其中一条拦住。
     *   这里核的是「填满后一张也进不来」，不再区分是哪条拒的。*/
    ce_sdk_reset_for_test();
    static ce_behavior_t many[CE_SDK_MAX_BEHAVIORS];
    static ce_behavior_t extra;
    for (unsigned i = 0; i < CE_SDK_MAX_BEHAVIORS; ++i) {
        many[i].id = CE_SDK_CARD_ID_MIN + i; many[i].vtable = vt_a; many[i].slots = "";
    }
    for (unsigned i = 0; i < CE_SDK_MAX_BEHAVIORS; ++i) CHECK(ce_sdk_register(&many[i]));
    CHECK(ce_sdk_behavior_count() == CE_SDK_MAX_BEHAVIORS);
    CHECK(many[CE_SDK_MAX_BEHAVIORS - 1].id == CE_SDK_CARD_ID_MAX);   /* 最后一张正好是最大合法 id */
    extra.id = CE_SDK_CARD_ID_MIN; extra.vtable = vt_b; extra.slots = "";
    extra.hooks = 0; extra.is_active = 0;
    CHECK(!ce_sdk_register(&extra));
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
    /* ★★ 2026-09-16za：「未满 P」声明层（106 的两条新设计共用一个来源；消费方 = 两条过线断点 + 111）*/
    {
        ce_full_p_gate_reset();
        CHECK(ce_full_p_gate_active() == 0 && ce_full_p_gate_owner() == -1);
        ce_full_p_gate_declare(106, 1);
        CHECK(ce_full_p_gate_active() == 1 && ce_full_p_gate_owner() == 106);
        ce_full_p_gate_declare(111, 1);                       /* 第二家声明：owner = id 最小者 */
        CHECK(ce_full_p_gate_active() == 1 && ce_full_p_gate_owner() == 106);
        ce_full_p_gate_declare(106, 0);                       /* 先撤 106 ⇒ 111 还在 ⇒ 仍然生效 */
        CHECK(ce_full_p_gate_active() == 1 && ce_full_p_gate_owner() == 111);
        ce_full_p_gate_declare(111, 0);
        CHECK(ce_full_p_gate_active() == 0 && ce_full_p_gate_owner() == -1);
        ce_full_p_gate_declare(106, 1);
        ce_full_p_gate_declare(106, 0);                       /* 幂等：重复撤 */
        CHECK(ce_full_p_gate_active() == 0);
        ce_full_p_gate_declare(CE_SDK_CARD_ID_MAX + 1, 1);    /* 越界 id：忽略 */
        CHECK(ce_full_p_gate_active() == 0);
    }

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

    /* 计分：**legacy 通道**求和（旧语义：单卡逐字节等价，见 test_score_rework）*/
    ce_score_reset();
    CHECK(ce_score_aggregate() == 0 && ce_score_applier() == -1);
    ce_score_declare(111, 25000);
    ce_score_declare(110, 50000);
    CHECK(ce_score_aggregate() == 75000);
    CHECK(ce_score_applier() == 110);                  /* 110 < 111（只是归属标签，不决定落笔）*/
    ce_score_declare(110, 0);                          /* 110 退出 */
    CHECK(ce_score_aggregate() == 25000 && ce_score_applier() == 111);

    /* 消弹闩锁：本帧只有第一个认领者拿得到 */
    ce_cancel_latch_reset();
    CHECK(ce_cancel_latch_owner() == -1);
    CHECK(ce_cancel_latch_claim(107) == 1);
    CHECK(ce_cancel_latch_claim(117) == 0);            /* 已被 107 认领 */
    CHECK(ce_cancel_latch_owner() == 107);

    /* ---- ★ 本帧消弹记录（2026-09-13，原生卡冲突审计 G 通道）----
     * 「闩锁 = 仲裁」与「记录 = 事实」拆开：记录由 SDK 的消弹 helper 自动登记（这里直接调 note 模拟），
     * 卡侧的正确读法是 **count == 0 才代表"真的一颗都没消"**；owner 不是你不代表没人消过弹。*/
    ce_cancel_latch_reset();
    CHECK(ce_cancel_frame_seen() == 0 && ce_cancel_frame_count() == 0);
    ce_cancel_frame_note(0);                           /* 消了 0 颗：发生过，但账是 0 */
    CHECK(ce_cancel_frame_seen() == 1 && ce_cancel_frame_count() == 0);
    ce_cancel_frame_note(12);
    ce_cancel_frame_note(8);
    CHECK(ce_cancel_frame_count() == 20);              /* 同帧多次消弹累加 */
    ce_cancel_frame_note(-1);                          /* ce_cancel_all_bullets：数不出颗数 */
    CHECK(ce_cancel_frame_unknown() == 1 && ce_cancel_frame_count() == 20);
    ce_cancel_latch_reset();                           /* 帧界清干净 */
    CHECK(ce_cancel_frame_seen() == 0 && ce_cancel_frame_count() == 0 && ce_cancel_frame_unknown() == 0);

    /* 空认领要能交回：闩锁代表「本帧有人真的消过弹」，不是「有人曾经想消弹」 */
    CHECK(ce_cancel_latch_claim(107) == 1);
    CHECK(ce_cancel_latch_release(117) == 0);          /* 不是认领者 ⇒ 交不回（也不会误清别人的）*/
    CHECK(ce_cancel_latch_owner() == 107);
    CHECK(ce_cancel_latch_release(107) == 1);          /* ★ 自己交回 */
    CHECK(ce_cancel_latch_owner() == -1);
    CHECK(ce_cancel_latch_claim(117) == 1);            /* ⇒ 后面的卡这一帧还有机会 */
    CHECK(ce_cancel_latch_release(117) == 1);
    CHECK(ce_cancel_latch_release(999) == 0);          /* 越界 id：不动别人 */

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

    /* ---- ★ 「中弹无伤」闸门声明层（2026-09-13；一手 engine/player/th18/03-player-hit-gate.md）----
     * 语义与 `ce_invuln_*` **不同**：无敌 = 子弹不判定；本层 = 判定照跑、**弹被吃掉**
     * （= 一手 TH13：`Player::on_hit` 第一行 return）。声明是**粘性**的（断点可能落在两个 tick 之间）。
     * 仲裁：**任一声明即生效**（不像无敌取 max、也不像计分求和）—— 因为它是个布尔闸门。*/
    ce_no_hit_reset();
    CHECK(ce_no_hit_active() == 0);   /* 初始闸门必须关着（默认逐字节放行）*/
    CHECK(ce_no_hit_owner() == -1);
    ce_no_hit_declare(113, 1);
    CHECK(ce_no_hit_active() == 1);
    CHECK(ce_no_hit_owner() == 113);
    ce_no_hit_declare(120, 1);                         /* 第二张卡也声明 ⇒ 仍然只有「开」这一种结论 */
    CHECK(ce_no_hit_active() == 1);
    CHECK(ce_no_hit_owner() == 113);  /* owner = id 最小者（诊断用）*/
    ce_no_hit_declare(120, 0);                         /* 撤掉后来者 ⇒ 不影响 113 */
    CHECK(ce_no_hit_active() == 1);
    CHECK(ce_no_hit_owner() == 113);
    ce_no_hit_declare(113, 0);                         /* 最后一个声明者撤了 ⇒ 闸门关 */
    CHECK(ce_no_hit_active() == 0);
    CHECK(ce_no_hit_owner() == -1);
    ce_no_hit_declare(113, 1);
    ce_no_hit_reset();                                 /* SDK 初始化路径也要能全清 */
    CHECK(ce_no_hit_active() == 0);
    ce_no_hit_declare(999, 1);                         /* 越界 id：不记、也不许开闸门 */
    CHECK(ce_no_hit_active() == 0);
    ce_no_hit_declare(113, 5);                         /* 非 0 一律当「开」（参数不是帧数）*/
    CHECK(ce_no_hit_active() == 1);
    ce_no_hit_declare(113, 0);
    CHECK(ce_no_hit_active() == 0);

    /* ---- ★★ 2026-09-16：「隐藏原生子机」声明层（111 的魔理沙侧；机制见 sdk_core.h / sdk.c）----
     * 与上面那一层同形状（粘性 + 任一声明即生效 + owner = id 最小者），**多一条机体门**：
     * 只有「当前机体 == 声明时给的那个」才算生效 —— 因为换机体发生在商店/编成里（卡 tick 冻着），
     * 不能指望卡来改口。这里把两条语义都钉住。*/
    ce_native_option_hide_reset();
    CHECK(ce_native_option_hidden(0) == 0 && ce_native_option_hidden(1) == 0);
    CHECK(ce_native_option_hide_owner() == -1);
    ce_native_option_hide_declare(111, 1, 1);          /* 111 声明「魔理沙(1) 机上隐藏」*/
    CHECK(ce_native_option_hidden(1) == 1);            /* 当前机体 = 魔理沙 ⇒ 生效 */
    CHECK(ce_native_option_hidden(0) == 0);            /* 灵梦 ⇒ 不生效（★ 机体门）*/
    CHECK(ce_native_option_hidden(2) == 0);            /* 咲夜 ⇒ 不生效 */
    CHECK(ce_native_option_hidden(3) == 0);            /* 早苗 ⇒ 不生效 */
    CHECK(ce_native_option_hide_owner() == 111);
    ce_native_option_hide_declare(113, 1, 0);          /* 第二张卡声明「灵梦(0) 机上隐藏」*/
    CHECK(ce_native_option_hidden(0) == 1 && ce_native_option_hidden(1) == 1);
    CHECK(ce_native_option_hide_owner() == 111);       /* owner = id 最小者 */
    ce_native_option_hide_declare(113, 0, 0);          /* 撤掉后来者 ⇒ 111 那条不受影响 */
    CHECK(ce_native_option_hidden(0) == 0 && ce_native_option_hidden(1) == 1);
    ce_native_option_hide_declare(111, 0, 1);          /* 最后一个声明者撤了 ⇒ 全关 */
    CHECK(ce_native_option_hidden(0) == 0 && ce_native_option_hidden(1) == 0);
    CHECK(ce_native_option_hide_owner() == -1);
    ce_native_option_hide_declare(111, 1, 1);
    ce_native_option_hide_reset();                     /* SDK 初始化路径（ce_sdk_setup）也要能全清 */
    CHECK(ce_native_option_hidden(1) == 0 && ce_native_option_hide_owner() == -1);
    ce_native_option_hide_declare(999, 1, 1);          /* 越界 id：不记 */
    CHECK(ce_native_option_hidden(1) == 0);
    ce_native_option_hide_declare(111, 1, 1);
    CHECK(ce_native_option_hidden(1) == 1);
    CHECK(ce_native_option_hidden(-1) == 0);           /* 机体号只做相等比较，不越界读 */
    ce_native_option_hide_declare(111, 0, 0);
    CHECK(ce_native_option_hidden(1) == 0);

    /* ---- 低速子机声明层（spec §3.3.1：逐格偏移 / id 最小者胜 / 每帧清集合 / 落笔权交接）----
     * 「为什么必须逐格」在 sdk_core.h 的 `CE_OPTION_POSE_CELLS` 块注释（一手）；
     * 这里钉**行为**：选举、逐格存储、钳位、每帧退出、退位通知。*/
    {
        ce_option_pose_t p;
        int prev = 123;

        ce_optpose_reset();
        CHECK(ce_optpose_winner() == -1 && ce_optpose_pose() == NULL);

        /* 111（SA 子机层）先声明 2 格 */
        memset(&p, 0, sizeof p);
        p.count = 2; p.anm_script = 30;
        p.cell[0].fx = 20.0f; p.cell[0].ux = 30.0f;
        p.cell[1].fx = -20.0f; p.cell[1].ux = -30.0f;
        CHECK(ce_optpose_declare(111, &p) == 2);
        CHECK(ce_optpose_winner() == 111);
        CHECK(ce_optpose_pose()->count == 2 && ce_optpose_pose()->anm_script == 30);
        CHECK(ce_optpose_pose()->cell[1].fx == -20.0f);        /* 逐格：第 2 格不是第 1 格的副本 */

        /* 108（妖机体）声明 4 格 ⇒ 108 < 111 ⇒ 它落笔 */
        memset(&p, 0, sizeof p);
        p.count = 4; p.anm_script = 17;
        for (int k = 0; k < 4; ++k) { p.cell[k].fx = (float)(k + 1); p.cell[k].ux = (float)(10 * (k + 1)); }
        CHECK(ce_optpose_declare(108, &p) == 4);
        CHECK(ce_optpose_winner() == 108);
        CHECK(ce_optpose_pose()->count == 4 && ce_optpose_pose()->anm_script == 17);
        CHECK(ce_optpose_pose()->cell[3].fx == 4.0f && ce_optpose_pose()->cell[3].ux == 40.0f);

        /* 帧界落笔点（先落笔、后清集合）：第一次只记录落笔者，退位者 = -1 */
        CHECK(ce_optpose_frame(&prev) == 108 && prev == -1);

        /* 108 退出声明（松 Shift）⇒ 111 顶上；**退位通知**给出 108（SDK 要补叫它一次去收子机）*/
        CHECK(ce_optpose_declare(108, NULL) == 0);
        CHECK(ce_optpose_winner() == 111);
        CHECK(ce_optpose_frame(&prev) == 111 && prev == 108);

        /* 落笔者没变 ⇒ 不重复通知 */
        CHECK(ce_optpose_frame(&prev) == 111 && prev == -1);

        /* 每帧清集合：清完就没人声明（= 上一帧声明过的卡不重发就退出）*/
        ce_optpose_frame_reset();
        CHECK(ce_optpose_winner() == -1 && ce_optpose_pose() == NULL);
        CHECK(ce_optpose_frame(&prev) == -1 && prev == 111);   /* 111 也退位 ⇒ 通知一次 */

        /* 钳位：count > CELLS ⇒ 钳到 CELLS（返回值 = 真正记下的格数）*/
        memset(&p, 0, sizeof p);
        p.count = CE_OPTION_POSE_CELLS + 3; p.anm_script = 9;
        CHECK(ce_optpose_declare(108, &p) == CE_OPTION_POSE_CELLS);
        CHECK(ce_optpose_pose()->count == CE_OPTION_POSE_CELLS);
        /* count <= 0 / 越界 id：都是「不参与」，且不破坏已有选举 */
        p.count = -1;
        CHECK(ce_optpose_declare(111, &p) == 0);
        CHECK(ce_optpose_winner() == 108);
        CHECK(ce_optpose_declare(999, &p) == 0);               /* 越界 id：不记 */
        ce_optpose_drop(108);                                  /* 删卡 = 立刻退出声明 */
        CHECK(ce_optpose_winner() == -1);
        (void)ce_optpose_frame(&prev);
        ce_optpose_reset();
        CHECK(ce_optpose_winner() == -1);
    }
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

    /* ★★ 2026-09-22：**货架给原生那块「当前卡」UI 让位**（`ce_hud_card_set()`，见 `sdk_core.h`）。
     *   一手：那块 UI = `abcard.anm` script0 的树，块 = 表面 x 68..116 / y 864..924；有它时货架起点
     *   取 `max(锚点, 116 + 8)`，没有时就是锚点（那块整体不存在 ⇒ 不占地方）。
     *   ⚠ 现值代入**恒等**（124 < 257）—— 这两条断言钉的是**不变式**：谁把锚点左移过 124，
     *     或把块加宽到压过锚点，`present=1` 那一支就会与 `present=0` 分叉。*/
    {
        ce_hud_slot_t p1, p0;
        ce_hud_block_t small = { 40.0f, 20.0f, CE_HUD_SIDE_LEFT };
        ce_hud_slots_reset();
        ce_hud_card_set(0);
        CHECK(ce_hud_slot_claim(120, &small, &p0));
        ce_hud_slots_reset();
        ce_hud_card_set(1);
        CHECK(ce_hud_slot_claim(120, &small, &p1));
        /* ★★★ 2026-10-03v（锚点 257 → 68）：**两者不再相同** —— 锚点已经落到卡面那一片里了，
         *   所以「让位」这条机制从"恒等"变成**真的在起作用**：没卡面 = 68、有卡面 = 124。*/
        CHECK(p0.x == CE_HUD_ANCHOR_X);
        CHECK(p1.x == HUD_CARD_POS_X + HUD_CARD_W + CE_HUD_GAP);     /* = 68 + 48 + 8 = 124 */
        CHECK(p1.x >= HUD_CARD_POS_X + HUD_CARD_W);                 /* ★ 且绝不压到那块 UI */
        ce_hud_card_set(0);
        ce_hud_slots_reset();
    }

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

    /* ★★ 2026-10-03p（owner：「第二行第三行用起来，我建议是看看能不能搞一个**贪心**或什么的算法」）：
     *   同优先级内改成 **宽降序**（first-fit-decreasing）⇒ **800 宽的 mid 先坐底边**（x = 68），
     *   108（108 宽）紧挨着它（876+108 = 984 ≤ 1040 ✓），111 放不下（984+108+8 = 1100 > 1040）
     *   ⇒ 抬到第二行（底边 924 − 20 − 8 = 896）。
     * ⚠ 旧断言假设「按 card_id 摆、锚点 257、软上限 1000」—— 已被 2026-10-03p/v 两条口径取代。*/
    ce_hud_block_t mid = { 800.0f, 20.0f, CE_HUD_SIDE_LEFT };
    CHECK(ce_hud_slot_claim(109, &mid, &again));
    CHECK(ce_hud_slot_get(109, &again) && ce_hud_slot_get(108, &b) && ce_hud_slot_get(111, &a));
    CHECK(again.x == CE_HUD_ANCHOR_X && again.y == CE_HUD_ANCHOR_Y);       /* 宽的先摆、坐底边 */
    CHECK(b.x == CE_HUD_ANCHOR_X + mid.w + CE_HUD_GAP && b.y == CE_HUD_ANCHOR_Y);   /* 紧挨 800 那块 */
    /* ★ 2026-10-03w skyline：111（108×36）填进 **109 上方那条缝**（底边 924 − 20 − 8 = 896），
     *   而不是像行式那样被抬到 924 − 36 − 8 = 880 —— 这正是"不用行、用矩形排布"省下来的 16 px。*/
    CHECK(a.x == CE_HUD_ANCHOR_X && a.y == CE_HUD_ANCHOR_Y - mid.h - CE_HUD_GAP);

    /* 局中删卡不当场重排（§4.4）：release 保留占位、别人坐标不变 */
    ce_hud_slot_release(108);
    CHECK(ce_hud_slot_claimed(108));                              /* 仍算已认领 */
    CHECK(ce_hud_slot_get(111, &again) && again.x == a.x && again.y == a.y);
    {   /* ★ 2026-10-03v：108 现在落在**行内**（不是锚点本身）⇒ 断言"与删卡前同一处" */
        ce_hud_slot_t before;
        CHECK(ce_hud_slot_get(108, &before));
        CHECK(ce_hud_slot_get(108, &again) && again.x == before.x && again.y == before.y);
    }
    /* 关卡开场 reset 才真正收回 */
    ce_hud_slots_reset();
    CHECK(!ce_hud_slot_claimed(108) && !ce_hud_slot_claimed(109) && !ce_hud_slot_claimed(111));

    /* ★★★ 2026-10-03l（owner：「包含左下角 HUD 的都应当进这个系统」）：
     *   **PINNED 碰撞回退** —— 不撞 ⇒ 用原位（保真）；撞了 ⇒ 降级成货架成员（不压住先来的那张）。*/
    {
        ce_hud_slot_t p_first, p_far, p_hit;
        ce_hud_block_t pinblk = { 100.0f, 40.0f, CE_HUD_SIDE_PINNED };
        ce_hud_slots_reset();
        CHECK(ce_hud_slot_claim_pinned(116, &pinblk, 68.0f, 924.0f, &p_first));
        CHECK(p_first.x == 68.0f && p_first.y == 924.0f);            /* ① 原位 */
        CHECK(ce_hud_slot_fallbacks() == 0);
        CHECK(ce_hud_slot_claim_pinned(117, &pinblk, 600.0f, 924.0f, &p_far));
        CHECK(p_far.x == 600.0f && p_far.y == 924.0f);               /* ② 不相交 ⇒ 原位 */
        CHECK(ce_hud_slot_fallbacks() == 0);
        CHECK(ce_hud_slot_claim_pinned(118, &pinblk, 100.0f, 924.0f, &p_hit));
        CHECK(ce_hud_slot_fallbacks() == 1);                         /* ③ 相交 ⇒ 回退 */
        CHECK(p_hit.side == CE_HUD_SIDE_LEFT && p_hit.x == CE_HUD_ANCHOR_X);
        CHECK(ce_hud_slot_get(117, &p_far) && p_far.x == 600.0f);    /* ⚠ 不动别的块 */
        ce_hud_slots_reset();
        CHECK(ce_hud_slot_fallbacks() == 0);
    }

    /* ★★★ 2026-10-03o（owner：「当你有主动卡的时候，最左下角就会多出来一个卡面」）：
     *   **原生卡面也是在场的一块**（表面 68..116 × 864..924）⇒ pinned 撞上它 ⇒ 回退货架。
     *   ⚠ 货架那一侧早就让位了（`hud_card_shelf_x0()`）；这里补的是「PINNED 不是例外」。*/
    {
        ce_hud_slot_t pin_slot;
        ce_hud_block_t pin_68 = { 132.0f, 32.0f, CE_HUD_SIDE_PINNED };   /* 112 那种 68..200 的块 */
        ce_hud_slots_reset();
        ce_hud_card_set(0);                                              /* 没有主动卡 ⇒ 原位 */
        CHECK(ce_hud_slot_claim_pinned(112, &pin_68, 68.0f, 924.0f, &pin_slot));
        CHECK(pin_slot.x == 68.0f && pin_slot.side == CE_HUD_SIDE_PINNED);
        CHECK(ce_hud_slot_fallbacks() == 0);
        ce_hud_slots_reset();
        ce_hud_card_set(1);                                              /* 有主动卡 ⇒ 撞卡面 ⇒ 回退 */
        CHECK(ce_hud_slot_claim_pinned(112, &pin_68, 68.0f, 924.0f, &pin_slot));
        CHECK(pin_slot.side == CE_HUD_SIDE_LEFT);
        CHECK(ce_hud_slot_fallbacks() == 1);
        CHECK(pin_slot.x >= HUD_CARD_POS_X + HUD_CARD_W);                 /* 落在卡面右边 */
        ce_hud_card_set(0);
        ce_hud_slots_reset();
    }

    /* ★★★ 2026-10-03q（owner：「**所有左下角 HUD 应当是"一块 HUD"**」）：
     *   带的矩形 = **所有已认领块的并集** ∪（有主动卡时）**原生卡面**（68..116 × 864..924）。*/
    {
        float bx, by, bw, bh;
        ce_hud_block_t b1 = { 100.0f, 40.0f, CE_HUD_SIDE_LEFT };
        ce_hud_block_t b2 = { 200.0f, 60.0f, CE_HUD_SIDE_LEFT };
        ce_hud_slots_reset();
        ce_hud_card_set(0);
        CHECK(ce_hud_band_empty());
        CHECK(ce_hud_slot_claim(120, &b1, &b) && ce_hud_slot_claim(121, &b2, &a));
        ce_hud_band_rect(&bx, &by, &bw, &bh);
        CHECK(!ce_hud_band_empty());
        CHECK(bx == CE_HUD_ANCHOR_X);                     /* 并集左沿 = 第一个块的左沿 */
        CHECK(by == CE_HUD_ANCHOR_Y);                     /* 底边仍是锚点那一条 */
        CHECK(bh >= 60.0f);                               /* 高 = 最高的块（200×60 那个）*/
        /* 有主动卡 ⇒ 卡面那一块（68..116）也并进来 ⇒ 左沿被拉到 68、高至少 60 */
        ce_hud_slots_reset();
        ce_hud_card_set(1);
        CHECK(ce_hud_slot_claim(120, &b1, &b));
        ce_hud_band_rect(&bx, &by, &bw, &bh);
        CHECK(bx == HUD_CARD_POS_X);                      /* ★ 包括原生 */
        CHECK(by == HUD_CARD_POS_Y);
        CHECK(bh >= HUD_CARD_H);
        /* 共享淡化：空带 ⇒ 满亮；tick 后仍在合法区间 */
        ce_hud_card_set(0);
        ce_hud_slots_reset();
        ce_hud_band_dim_reset();
        CHECK(ce_hud_band_dim_alpha() == HUD_DIM_ALPHA_FULL);
    }

    /* 硬约束（§4.2）：块右边缘不许越过 1040 ⇒ 声明宽度超限直接拒绝 */
    ce_hud_block_t too_wide = { CE_HUD_X_HARD_MAX - CE_HUD_ANCHOR_X + 1.0f, 36.0f, CE_HUD_SIDE_LEFT };
    CHECK(!ce_hud_slot_claim(110, &too_wide, &a));
    /* 越软上限 1000 但仍合法 ⇒ 单独占一行（空行上的超大块不抬行）
     * ★ 2026-10-03v：锚点 68 ⇒ 900 宽的块（68+900 = 968）之后 100 宽的塞不下（968+100+8 > 1040）*/
    ce_hud_slots_reset();
    ce_hud_block_t greed = { 900.0f, 36.0f, CE_HUD_SIDE_LEFT };   /* 68+900 ≤ 硬上限 1040 */

    CHECK(ce_hud_slot_claim(110, &greed, &a));
    CHECK(a.x == CE_HUD_ANCHOR_X && a.y == CE_HUD_ANCHOR_Y);
    ce_hud_block_t next = { 100.0f, 36.0f, CE_HUD_SIDE_LEFT };
    CHECK(ce_hud_slot_claim(111, &next, &b));
    CHECK(b.x == CE_HUD_ANCHOR_X);                                /* 968 + 100 + 8 > 1040 ⇒ 抬行 */
    CHECK(b.y == CE_HUD_ANCHOR_Y - greed.h - CE_HUD_GAP);
    ce_hud_block_t bad = { 0.0f, 36.0f, CE_HUD_SIDE_LEFT };
    CHECK(!ce_hud_slot_claim(110, &bad, &a));                     /* 尺寸非法 */
    CHECK(!ce_hud_slot_claim(112, &next, NULL));                 /* out == NULL */
    CHECK(!ce_hud_slot_claim(CE_SDK_CARD_ID_MAX + 1u, &next, &a)); /* id 非法 */
    CHECK(!ce_hud_slot_get(CE_SDK_CARD_ID_MAX + 1u, &a));
    CHECK(!ce_hud_slot_get(107, &a));                             /* 没认领过 */

    /* ★★★ 2026-10-03k（owner：「首先得考虑某种**美观性**」；点名 109 的口径）：
     *   **优先级 + 独占行** —— priority 大的先摆（⇒ 落在更下面的行）；own_row 的块独自占一行。*/
    {
        ce_hud_slot_t lo, wide, hi;
        ce_hud_block_t blk_lo   = { 100.0f, 20.0f, CE_HUD_SIDE_LEFT };
        ce_hud_block_t blk_wide = { 452.0f, 24.0f, CE_HUD_SIDE_LEFT };     /* 109 那条蓄力槽 */
        ce_hud_block_t blk_hi   = { 100.0f, 20.0f, CE_HUD_SIDE_LEFT };
        ce_hud_slots_reset();
        CHECK(ce_hud_slot_claim(200, &blk_lo, &lo));
        CHECK(ce_hud_slot_claim_ex(201, &blk_wide, 100, 1, &wide));        /* 优先级最高 + 独占一行 */
        CHECK(ce_hud_slot_claim_ex(202, &blk_hi, 1, 0, &hi));
        CHECK(ce_hud_slot_get(201, &wide) && ce_hud_slot_get(200, &lo) && ce_hud_slot_get(202, &hi));
        CHECK(wide.x == CE_HUD_ANCHOR_X && wide.y == CE_HUD_ANCHOR_Y);          /* 优先级最高 ⇒ 底边最左 */
        /* ★ 2026-10-03w skyline：wide 不再"独占一行" —— hi/lo 直接摆在它**右边**同一条底边
         *   （452 宽右侧还有 520 px 空位）；`own_row` 的语义改成"**在已摆块之上**另起一条"。*/
        CHECK(hi.x == CE_HUD_ANCHOR_X + blk_wide.w + CE_HUD_GAP && hi.y == CE_HUD_ANCHOR_Y);
        CHECK(lo.x == hi.x + blk_hi.w + CE_HUD_GAP && lo.y == hi.y);
        {   /* `own_row`（后摆、低优先级）⇒ 必须落在**所有已摆块之上**：底边 = 最高块上沿 − 间隙 */
            ce_hud_slot_t own;
            ce_hud_block_t blk_own = { 100.0f, 20.0f, CE_HUD_SIDE_LEFT };
            CHECK(ce_hud_slot_claim_ex(203, &blk_own, 0, 1, &own));
            CHECK(ce_hud_slot_get(203, &own));
            CHECK(own.y == CE_HUD_ANCHOR_Y - blk_wide.h - CE_HUD_GAP);          /* 892 = 924 − 24 − 8 */
            CHECK(own.x == CE_HUD_ANCHOR_X);
        }
        /* 幂等：重复认领不改变优先级/独占行的结果 */
        CHECK(ce_hud_slot_claim_ex(201, &blk_wide, 100, 1, &wide));
        CHECK(ce_hud_slot_get(202, &hi) && hi.y == CE_HUD_ANCHOR_Y && hi.x == CE_HUD_ANCHOR_X + blk_wide.w + CE_HUD_GAP);
        ce_hud_slots_reset();
    }

    /* ★★ 2026-09-15c 回归：**111 当年的 bug 就是这一行** —— `on_load` 里
     *   `ce_hud_slot_claim_card(111, &blk, 0)` 传了 NULL `out`（上面那条断言说明 SDK 一定会拒绝它）
     *   ⇒ 111 的 HUD 槽从来没认领成功、3 支原版 VM 一次都没起过（用户报「看不见信号 UI」）。
     *   这条按**卡侧真实的那一组参数**（111 的块 208×56、LEFT 列）走一遍，锁住「传了真 out 就必须成功」
     *   与「拿到的槽能被 `_get` 读回来」两件事 —— 谁再把 out 写成 0，这里立刻红。*/
    ce_hud_slots_reset();
    {
        ce_hud_block_t sig = { 208.0f, 56.0f, CE_HUD_SIDE_LEFT };   /* = SIGNAL_HUD_BLOCK_W/H */
        ce_hud_slot_t got;
        CHECK(ce_hud_slot_claim(111, &sig, &got));
        CHECK(ce_hud_slot_claimed(111));
        CHECK(ce_hud_slot_get(111, &a) && a.x == got.x && a.y == got.y);
        CHECK(a.x == CE_HUD_ANCHOR_X && a.y == CE_HUD_ANCHOR_Y);
        CHECK(!ce_hud_slot_claim(111, &sig, NULL));                 /* 传 NULL 就一定是「没认领」 */
        ce_hud_slots_reset();
    }

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
    CHECK(ce_score_signal_strength() == 500);             /* 重做后的正名 */
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

/* ============================ 计分层（2026-09-13 重做）单测 ============================
 * 被测规格（用户 2026-09-13，逐条见 ../SCORE-LAYER-REWORK.md）：
 *     SCORE += G × coeff_06 × rest ;  G = G_0 + Σ G_i
 *     G_0 = 原生身价换算成点 = 引擎自己给这一颗的 SCORE 增量
 *     耦合项（唯一带乘数的）：(Δ_10 + Δ_11) × (擦弹倍率 + 信号强度)
 * ★★ 单位（2026-09-13 第二次修订时逐条对着 exe 核过，**三个别混**）：
 *     站点 ecx（1/1000 点） --满额支--> 身价（= 弹窗值 = 点 × 10） --`SCORE += esi/10`--> 点
 *   即 `身价 = ecx / 100`、`点 = ecx / 1000`。桥值（`ce_score_bridge`）收的是**身价**，
 *   上下限常数（`CE_SCORE_BASE_MIN_PTS` / `..._MAX_*`）也是**身价**单位。
 * 四条要点各有一组断言：① 单卡 = 旧行为；② 两卡相加；③ 耦合项相乘；④ 上下限 / 难度表。*/

/* 旧行为（重做前）的落笔值，单位 = 引擎单位（身价）。旧公式（三张卡都这么写）：
 *     applier: *value = max(declared, 10) * 10  ;  引擎随后 SCORE += *value / 10。
 * 这里逐字复刻它，用来断言「单卡时新旧逐字节等价」。`declared` = 那一张卡自己算出来的
 * 「最大得点 / 信仰」（107 是它自己的数、110/111 是「基础 + Δ」——旧卡把基础一起声明了）。*/
static int32_t legacy_value(int32_t declared)
{
    if (declared < 10) declared = 10;
    return declared * 10;
}
/* 单卡 legacy 等价：新式（原样声明 = 旧声明值，耦合系数取 1000 = 不缩放）对上旧落笔值。
 * `g0` = 造出来的桥值对应的 `G_0`（点）；`old_declared` = 旧代码当年声明的那个数。*/
static void check_single_card_equivalence(int32_t g0, int32_t old_declared, int32_t new_declared)
{
    ce_score_reset();
    ce_score_bridge((uint32_t)g0 * CE_SCORE_ENGINE_UNIT, CE_SCORE_DIFF_NORMAL);  /* 造出 G_0 = g0 */
    CHECK(ce_score_base_points() == g0);
    ce_score_declare(107, new_declared);                /* legacy 通道（用 107 那一格）*/
    CHECK(ce_score_legacy_count() == 1);
    CHECK(ce_score_legacy_sum() == new_declared);       /* 新式声明值 = 旧声明值 − G_0 */
    CHECK(new_declared + g0 == old_declared);           /* 两者是同一个数（基础由层出）*/
    CHECK(ce_score_apply() == legacy_value(old_declared));
}

/* 站点 `th18:0x446bf9` 处的 `ecx`（单位 1/1000 点；一手 0x446b30 / 0x446b86 / 0x446b53–0x446bf1）：
 *   未钳 = `(1 + 资金力) × k × 100`（EASY k=500、其余 1000；读点在 `0x446b2e`，MONEY 还没 inc）
 *   引擎自己把它钳到 `[表 0x4b666c 值 ×100, 表 0x4b6654 值 ×100]` = `[1,000,000, 表值 ×100]`
 *   （`0x4ccd28` / `0x4ccd2c`，由 `0x44286e` / `0x442898` 写入）——本站点看到的就是**钳后**的值。
 * ★ 引擎已钳 ⇒ 层的兜底钳在场上恒等，`G_0 ≡ ecx/1000`（下一条断言组钉死）。*/
static uint32_t score_ecx(int32_t money, int difficulty)
{
    int32_t k = (difficulty == CE_SCORE_DIFF_EASY) ? CE_SCORE_EASY_K : CE_SCORE_NORMAL_K;
    int64_t v = (int64_t)(money + 1) * k * 100;
    int64_t hi;
    switch (difficulty) {
    case CE_SCORE_DIFF_EASY:    hi = CE_SCORE_BASE_MAX_EASY;    break;
    case CE_SCORE_DIFF_HARD:    hi = CE_SCORE_BASE_MAX_HARD;    break;
    case CE_SCORE_DIFF_LUNATIC: hi = CE_SCORE_BASE_MAX_LUNATIC; break;
    case CE_SCORE_DIFF_EXTRA:   hi = CE_SCORE_BASE_MAX_EXTRA;   break;
    default:                    hi = CE_SCORE_BASE_MAX_NORMAL;  break;
    }
    if (v > hi * 100) v = hi * 100;
    else if (v < (int64_t)CE_SCORE_BASE_MIN_PTS * 100) v = (int64_t)CE_SCORE_BASE_MIN_PTS * 100;
    return (uint32_t)v;
}
/* sdk.c `BP_ce_item_score_in` 填给层的桥值：**身价** = ecx / 100（= ecx/1000 点 × 10）。*/
static uint32_t score_bridge_of(uint32_t ecx) { return ecx / 100u; }

static void test_score_rework(void)
{
    /* ---- ④a 难度修正表（EASY 500 / 其余 1000）+ 上下限（**身价**单位）+ ★ G_0 == ecx/1000 ----
     * 一手（th18:0x446b30 / 0x446b86 算、0x446b53–0x446bf1 钳、0x446cb4 起除）：
     *   ecx = clamp((1+资金力) × k × 100, 1e6, 表值×100) ;  身价 = ecx/100 ;  点 = ecx/1000。*/
    CHECK(CE_SCORE_EASY_K == 500 && CE_SCORE_NORMAL_K == 1000);
    CHECK(score_ecx(0, CE_SCORE_DIFF_EASY) == 1000000u);              /* (0+1)×500×100 = 50000 ⇒ 引擎顶到下限 1e6 */
    ce_score_reset();
    ce_score_bridge(score_bridge_of(score_ecx(0, CE_SCORE_DIFF_EASY)), CE_SCORE_DIFF_EASY);
    CHECK(ce_score_base_points() == 1000);                            /* 身价 10000 / 10；原生这一颗 SCORE +1000 */
    CHECK(ce_score_base_points() == (int32_t)(score_ecx(0, CE_SCORE_DIFF_EASY) / 1000u));
    CHECK(score_ecx(2, CE_SCORE_DIFF_EASY) == 1000000u);              /* (2+1)×500×100 = 150000 ⇒ 仍被下限顶到 1e6 */
    ce_score_bridge(score_bridge_of(score_ecx(2, CE_SCORE_DIFF_EASY)), CE_SCORE_DIFF_EASY);
    CHECK(ce_score_base_points() == 1000);                            /* 下限档 = 1000 点（原生 SCORE +1000）*/
    CHECK(score_ecx(19, CE_SCORE_DIFF_NORMAL) == 2000000u);           /* (19+1)×1000×100 */
    ce_score_bridge(score_bridge_of(score_ecx(19, CE_SCORE_DIFF_NORMAL)), CE_SCORE_DIFF_NORMAL);
    CHECK(ce_score_base_points() == 2000);                            /* ★ ecx/1000 = 2000 点 */
    /* 「1 + 资金力」里的一手锚点：MONEY = 19 ⇒ ecx = 2,000,000 ⇒ 身价 20000 ⇒ 点 2000 */
    CHECK(score_ecx(99, CE_SCORE_DIFF_EASY) == 5000000u);             /* 100×500×100，未到 EASY 上限（2e7）*/
    ce_score_bridge(score_bridge_of(score_ecx(99, CE_SCORE_DIFF_EASY)), CE_SCORE_DIFF_EASY);
    CHECK(ce_score_base_points() == 5000);                            /* = min(5e6, 2e7)/1000 */
    CHECK(score_ecx(99, CE_SCORE_DIFF_NORMAL) == 10000000u);
    ce_score_bridge(score_bridge_of(score_ecx(99, CE_SCORE_DIFF_NORMAL)), CE_SCORE_DIFF_NORMAL);
    CHECK(ce_score_base_points() == 10000);
    /* 上限：身价到表值就停（引擎钳 ecx = 表值×100）——逐难度一一对应 */
    ce_score_bridge(score_bridge_of(score_ecx(1000000, CE_SCORE_DIFF_HARD)), CE_SCORE_DIFF_HARD);
    CHECK(ce_score_base_points() == CE_SCORE_BASE_MAX_HARD / CE_SCORE_ENGINE_UNIT);
    ce_score_bridge(score_bridge_of(score_ecx(1000000, CE_SCORE_DIFF_LUNATIC)), CE_SCORE_DIFF_LUNATIC);
    CHECK(ce_score_base_points() == CE_SCORE_BASE_MAX_LUNATIC / CE_SCORE_ENGINE_UNIT);
    ce_score_bridge(score_bridge_of(score_ecx(1000000, CE_SCORE_DIFF_EXTRA)), CE_SCORE_DIFF_EXTRA);
    CHECK(ce_score_base_points() == CE_SCORE_BASE_MAX_EXTRA / CE_SCORE_ENGINE_UNIT);   /* EXTRA 与 NORMAL 同上限 */
    ce_score_bridge(score_bridge_of(score_ecx(1000000, CE_SCORE_DIFF_EASY)), CE_SCORE_DIFF_EASY);
    CHECK(ce_score_base_points() == CE_SCORE_BASE_MAX_EASY / CE_SCORE_ENGINE_UNIT);
    /* ★ 层的兜底钳（**身价**单位）：只有桥值本身越界时才看得见（场上引擎已钳过 ⇒ 恒等）*/
    ce_score_bridge(5000u, CE_SCORE_DIFF_NORMAL);                     /* 界外小值 */
    CHECK(ce_score_base_points() == CE_SCORE_BASE_MIN_PTS / CE_SCORE_ENGINE_UNIT);   /* ⇒ 1000 点 */
    ce_score_bridge(99999999u, CE_SCORE_DIFF_NORMAL);                 /* 界外大值 */
    CHECK(ce_score_base_points() == CE_SCORE_BASE_MAX_NORMAL / CE_SCORE_ENGINE_UNIT); /* ⇒ 50000 点 */

    /* ---- 裸 G_0（没有卡参与）：SDK **一个字都不写** ⇒ 原生路径逐字节不变 ----
     * 层这一侧仍要算对：桥值（身价）= ecx/100，`G_0 = 身价/10 = ecx/1000`（点）。*/
    ce_score_reset();
    CHECK(score_ecx(199, CE_SCORE_DIFF_NORMAL) == 20000000u);         /* (199+1)×1000×100，未越界 */
    ce_score_bridge(score_bridge_of(score_ecx(199, CE_SCORE_DIFF_NORMAL)), CE_SCORE_DIFF_NORMAL);
    CHECK(ce_score_base_points() == 20000);                           /* 身价 200000 / 10 */
    CHECK(ce_score_participants() == 0 && ce_score_applier() == -1);
    CHECK(ce_score_total() == 20000);
    CHECK(ce_score_apply() == 20000 * CE_SCORE_ENGINE_UNIT);

    /* ---- ② 加法族：G = G_0 + Σ G_i（两卡相加，谁也不吞谁）---- */
    ce_score_reset();
    ce_score_bridge(100000u, CE_SCORE_DIFF_NORMAL);                   /* 桥值（身价）⇒ G_0 = 10000 点 */
    CHECK(ce_score_base_points() == 10000);
    ce_score_declare_coupled(110, 5000);                              /* Δ_10 */
    CHECK(ce_score_participants() == 1 && ce_score_aggregate() == 5000);
    ce_score_declare_coupled(111, 10000);                             /* Δ_11 */
    CHECK(ce_score_participants() == 2 && ce_score_aggregate() == 15000);
    CHECK(ce_score_coupling_permille() == 1000);                      /* 没人声明信号 ⇒ ×1.0 */
    CHECK(ce_score_total() == 10000 + 15000);
    CHECK(ce_score_apply() == 25000 * CE_SCORE_ENGINE_UNIT);
    ce_score_declare_delta(107, 105500);                              /* 未耦合族：原样进 G */
    CHECK(ce_score_participants() == 3);
    CHECK(ce_score_total() == 10000 + 15000 + 105500);
    /* ★ 基础**只出现一次**：旧口径（id 最小者把 Σ 当最终 PV）会写成 G_0×2 + Σ */
    CHECK(ce_score_total() != 2 * 10000 + 15000 + 105500);

    /* ---- ③ 耦合项相乘（唯一带乘数的）：(Δ_10+Δ_11) × (擦弹倍率 + 信号强度) ---- */
    /* ★ 2026-09-15 一手订正（research-th11-faith-and-points.md §2.3）：整数阶梯、每 100 擦弹 +0.01、
     * 上限 8.99 的触发点 = 89900 擦弹（旧实现按 1 graze = 1‰，8990 就封顶 ⇒ 提前 10 倍且零头被计入）*/
    CHECK(ce_score_graze_mult(0) == 0);
    CHECK(ce_score_graze_mult(1) == 0);                               /* 不足一步 ⇒ 0 */
    CHECK(ce_score_graze_mult(99) == 0);
    CHECK(ce_score_graze_mult(100) == 10);                            /* 一步 = 0.01 = 10‰ */
    CHECK(ce_score_graze_mult(5000) == 500);                          /* 0.5 */
    CHECK(ce_score_graze_mult(89899) == 8980);
    CHECK(ce_score_graze_mult(89900) == CE_SCORE_GRAZE_MULT_MAX);     /* 8.99 的触发点 */
    CHECK(ce_score_graze_mult(89999) == CE_SCORE_GRAZE_MULT_MAX);
    CHECK(ce_score_graze_mult(100000) == CE_SCORE_GRAZE_MULT_MAX);
    CHECK(ce_score_graze_mult(0xffffffffu) == 0);                     /* −1（int32 读回）⇒ 0 */

    ce_score_reset();
    ce_score_bridge(100000u, CE_SCORE_DIFF_NORMAL);                   /* 桥值（身价）⇒ G_0 = 10000 点 */
    ce_score_declare_coupled(110, 5000);
    ce_score_declare_coupled(111, 10000);
    ce_score_mul_declare(111, 1000);                                  /* 信号 1.00 ⇒ 1000‰ */
    CHECK(ce_score_signal_strength() == 1000);
    CHECK(ce_score_coupling_permille() == 2000);                      /* 基准 1000（= 擦弹 1.0）+ 1000 */
    CHECK(ce_score_total() == 10000 + 15000 * 2000 / 1000);           /* = 40000 */
    ce_score_mul_declare(111, 0);                                     /* 粘性：掉到 0 也要声明 */
    CHECK(ce_score_coupling_permille() == 1000);
    CHECK(ce_score_total() == 10000 + 15000);
    ce_score_mul_declare(111, 500);
    ce_score_mul_declare(112, 250);
    CHECK(ce_score_signal_strength() == 750);                         /* 两张卡的信号相加 */
    CHECK(ce_score_coupling_permille() == 1750);
    CHECK(ce_score_total() == 10000 + (int32_t)((int64_t)15000 * 1750 / 1000));
    ce_score_declare_delta(107, 1000);                                /* 未耦合族不受耦合系数影响 */
    CHECK(ce_score_total() == 10000 + (int32_t)((int64_t)15000 * 1750 / 1000) + 1000);
    CHECK(ce_score_apply() == ce_score_total() * CE_SCORE_ENGINE_UNIT);

    /* ---- ① 单卡 = 旧行为（逐字节）----
     * 三张卡各来一遍，桥值选得让 `G_0` 正好等于旧口径里那个「基础」：
     *   NORMAL、资金力 0 ⇒ G_0 = 1000 点（ecx = 1,000,000 ⇒ 身价 10000）—— 下面 ①b 用这个档。
     * 这里用 G_0 = 10000 点（身价 100000）那一档：
     *   107：旧声明 = 自己的最大得点（**不含** G_0，它本来就是 TH07 的数）⇒ 新式等价；
     *   110/111：旧声明 = 基础 + Δ ⇒ 新式声明 Δ + 层出的 G_0，两边逐字节相同。*/
    ce_score_reset();
    ce_score_bridge(100000u, CE_SCORE_DIFF_NORMAL);                   /* G_0 = 10000 */
    CHECK(ce_score_base_points() == 10000);
    ce_score_declare_delta(107, 105500);
    CHECK(ce_score_total() == 10000 + 105500);
    CHECK(ce_score_apply() == 115500 * CE_SCORE_ENGINE_UNIT);         /* ★ 与旧行为逐字节相同 */
    check_single_card_equivalence(10000, 115500, 105500);              /* 旧声明 = G_0 + G_i */

    ce_score_reset();
    ce_score_bridge(100000u, CE_SCORE_DIFF_NORMAL);
    ce_score_declare_coupled(110, 55000 - 10000);                     /* 旧声明 55000 = G_0 + Δ */
    CHECK(ce_score_total() == 55000);
    CHECK(ce_score_apply() == legacy_value(55000));

    ce_score_reset();
    ce_score_bridge(100000u, CE_SCORE_DIFF_NORMAL);
    ce_score_declare_coupled(111, 60000 - 10000);
    CHECK(ce_score_apply() == legacy_value(60000));

    /* ---- ①b legacy API（`ce_score_declare`）单卡逐字节等价 ----
     * 旧 .c 一行都不用改也能编过：`ce_score_declare(id, v)` 的 v 仍是「该卡自己的得分值」，
     * 层按 `max(v, 10)` 折成加数（= 老落笔公式 `*value = max(v,10)*10` 的 G 那一半）。
     * ★ NORMAL 资金力 0 这一档：ecx = 1,000,000 ⇒ 身价 10000 ⇒ `G_0 = 1000` 点（原生 SCORE +1000）。*/
    ce_score_reset();
    ce_score_bridge(score_bridge_of(score_ecx(0, CE_SCORE_DIFF_NORMAL)), CE_SCORE_DIFF_NORMAL);
    CHECK(score_ecx(0, CE_SCORE_DIFF_NORMAL) == 1000000u);            /* (0+1)×1000×100，被下限顶到 1e6 */
    CHECK(ce_score_base_points() == 1000);                            /* = ecx/1000 */
    check_single_card_equivalence(10000, 115500, 105500);             /* ★ 旧 115500 ↔ 新 (10000 档) 105500 */
    ce_score_reset();
    ce_score_bridge(score_bridge_of(score_ecx(0, CE_SCORE_DIFF_NORMAL)), CE_SCORE_DIFF_NORMAL);
    CHECK(ce_score_base_points() == 1000);
    ce_score_declare(107, 115500);
    CHECK(ce_score_legacy_count() == 1 && ce_score_legacy_sum() == 115500);
    CHECK(ce_score_total() == 1000 + 115500);
    ce_score_declare(107, 0);                                         /* 0 = 退出（旧语义）*/
    CHECK(ce_score_legacy_count() == 0);
    CHECK(ce_score_apply() == 1000 * CE_SCORE_ENGINE_UNIT);           /* 只剩 G_0 */
    ce_score_declare(107, 1);                                         /* 旧公式的 10 分下限也在 */
    CHECK(ce_score_legacy_sum() == 10);
    CHECK(ce_score_apply() == (1000 + 10) * CE_SCORE_ENGINE_UNIT);
    ce_score_declare(110, 50000);                                     /* 多张 legacy 卡 ⇒ 求和 */
    CHECK(ce_score_legacy_count() == 2 && ce_score_legacy_sum() == 50010);
    CHECK(ce_score_apply() == (1000 + 50010) * CE_SCORE_ENGINE_UNIT);
    ce_score_declare(CE_SDK_CARD_ID_MAX + 1u, 999999);                /* 越界 id：不记、不动别人 */
    CHECK(ce_score_legacy_count() == 2 && ce_score_legacy_sum() == 50010);

    /* ---- 删卡：四个通道都要能退（`ce_layer_drop_card` 逐个调）---- */
    ce_score_reset();
    ce_score_bridge(99999999u, CE_SCORE_DIFF_NORMAL);                 /* 界外大值 ⇒ 兜底钳到 N 上限 */
    ce_score_declare_delta(107, 1000);
    ce_score_declare_coupled(110, 2000);
    ce_score_declare_coupled(111, 3000);
    ce_score_mul_declare(111, 500);
    ce_score_declare(112, 4000);
    CHECK(ce_score_participants() == 4 && ce_score_signal_strength() == 500);
    ce_score_declare_delta(107, 0);
    ce_score_declare_coupled(110, 0);
    ce_score_declare_coupled(111, 0);
    ce_score_mul_declare(111, 0);
    ce_score_declare(112, 0);
    CHECK(ce_score_participants() == 0);
    CHECK(ce_score_signal_strength() == 0);
    CHECK(ce_score_coupling_permille() == 1000);
    CHECK(ce_score_total() == CE_SCORE_BASE_MAX_NORMAL / CE_SCORE_ENGINE_UNIT);   /* 全退掉 ⇒ 只剩 G_0 */

    /* ---- ④d coeff_06：**引擎的机制，本层不重实现**（用户规格：不要新建）----
     * 一手：系数算在**落笔点之后**、乘在**身价**上（`th18:0x446bf9`–`0x446cf3` / `0x446cfe`），
     * 判据 = 回收线 `player+0x47998` 与 **`player+0x624`（自机的 y）**。本层只**记录**线上还是线下：
     *   `ce_score_set_coeff(1000)` = 线上 / 真空飞行；`0` = 线下（引擎打折，值我们不猜）。
     * 折扣曲线的精确形状 🟡 未钉死（见 SCORE-LAYER-REWORK.md §4），因此**不写断言**。*/
    ce_score_reset();
    ce_score_bridge(100000u, CE_SCORE_DIFF_NORMAL);
    CHECK(ce_score_coeff_permille() == 1000);          /* reset 后 = 不打折 */
    ce_score_set_coeff(0);
    CHECK(ce_score_coeff_permille() == 0);             /* 线下标记 */

    /* ---- 引擎系数只是**只读**：不参与本层算式（引擎自己乘） ---- */
    ce_score_reset();
    ce_score_bridge(100000u, CE_SCORE_DIFF_NORMAL);
    ce_score_declare_delta(107, 1000);
    ce_score_set_coeff(500);
    CHECK(ce_score_coeff_permille() == 500);
    CHECK(ce_score_total() == 11000);                                 /* 系数不改本层结果（G_0 10000 + 107 的 1000）*/
    ce_score_set_coeff(1000);
    CHECK(ce_score_coeff_permille() == 1000);
}

/* ★★ 2026-09-13 新增：**落笔点搬到入口之后的单位恒等式**（`SCORE-LAYER-REWORK.md` §2 单位表）。
 * 这是本次改动的核心可执行证据：SDK 在新站点 `th18:0x446bf9` 拿到的 `ecx` 与层算出的 `G_0`
 * 必须满足
 *       ce_score_base_points() == ecx / 1000          （满额支 ⇒ SCORE 增量 == G_0）
 *       10 × ce_score_base_points() == ecx / 100      （= 引擎满额支的 esi / 弹窗值）
 * 而且要**跨难度、跨资金力**都成立（包括引擎自己那两个钳：下限 1e6 与逐难度上限表值×100）。*/
static void test_score_base_unit_identity(void)
{
    static const int diffs[5] = { CE_SCORE_DIFF_EASY, CE_SCORE_DIFF_NORMAL, CE_SCORE_DIFF_HARD,
                                  CE_SCORE_DIFF_LUNATIC, CE_SCORE_DIFF_EXTRA };
    static const int32_t moneys[9] = { 0, 1, 8, 9, 19, 99, 199, 499, 1000000 };
    int d, m;

    /* 引擎那两个界（一手 0x44286e / 0x442898 写 0x4ccd28 / 0x4ccd2c，0x446b53–0x446bf1 用）：
     *   ecx ∈ [1,000,000, 表值 ×100]  ⇒  点 ∈ [1000, 表值/10]。*/
    for (d = 0; d < 5; ++d) {
        int32_t hi_pts = 0;
        switch (diffs[d]) {
        case CE_SCORE_DIFF_EASY:    hi_pts = CE_SCORE_BASE_MAX_EASY;    break;
        case CE_SCORE_DIFF_HARD:    hi_pts = CE_SCORE_BASE_MAX_HARD;    break;
        case CE_SCORE_DIFF_LUNATIC: hi_pts = CE_SCORE_BASE_MAX_LUNATIC; break;
        case CE_SCORE_DIFF_EXTRA:   hi_pts = CE_SCORE_BASE_MAX_EXTRA;   break;
        default:                    hi_pts = CE_SCORE_BASE_MAX_NORMAL;  break;
        }
        hi_pts /= CE_SCORE_ENGINE_UNIT;
        for (m = 0; m < 9; ++m) {
            uint32_t ecx = score_ecx(moneys[m], diffs[d]);
            int32_t pts = (int32_t)(ecx / 1000u);
            ce_score_reset();
            ce_score_bridge(score_bridge_of(ecx), diffs[d]);          /* = sdk.c 填的那个数 */
            CHECK(ecx >= 1000000u && (int32_t)(ecx / 100u) <= hi_pts * CE_SCORE_ENGINE_UNIT);
            CHECK(pts >= 1000 && pts <= hi_pts);
            CHECK(ce_score_base_points() == pts);                     /* ★ G_0 == ecx/1000 */
            CHECK(ce_score_base_points() * CE_SCORE_ENGINE_UNIT == (int32_t)(ecx / 100u));  /* = 弹窗值 */
            /* 有卡声明时：总数 = G_0 + Σ，且落笔 `ecx' = 1000 × total` ⇒ 引擎给 total 分 */
            ce_score_declare_delta(107, 3000);
            CHECK(ce_score_total() == pts + 3000);
            CHECK(ce_score_total() * 1000 == pts * 1000 + 3000 * 1000);
        }
    }
    /* 无卡声明：**SDK 一个字都不写**（sdk.c 的判据就是 participants() == 0）⇒ 原生逐字节不变。*/
    ce_score_reset();
    ce_score_bridge(score_bridge_of(score_ecx(19, CE_SCORE_DIFF_NORMAL)), CE_SCORE_DIFF_NORMAL);
    CHECK(ce_score_participants() == 0);                              /* ⇒ 站点处不写 regs->ecx */
    CHECK(ce_score_total() == ce_score_base_points());                /* ⇒ total 只是原生基础 */
    ce_score_reset();
    CHECK(ce_score_participants() == 0);
}

/* ★★ 2026-09-13：伤害层落笔点搬到卡链遍历之后（`BP_ce_bullet_damage_final`，`th18:0x45e7ff`）
 * + 死亡结果闩锁（`BP_ce_death_result`，`th18:0x45c127`）。层本身是纯逻辑，所以「语义」这一半
 * 能在主机上钉死；「站点挂没挂上」由 `ce_sdk_setup` 的断点自检 + `sites.py check` 管。 */
static void test_native_conflict_fixes(void)
{
    /* ---- ① 伤害：**乘**在别人算完的值上（不是覆盖）----
     * 一手算例（CARD-NATIVE-CONFLICT-AUDIT §B）：原生 25 Saki 的 `+0x28` 把伤害重算成
     * `shooter × 1.4`，我们的 113 声明 1250（×1.25）⇒ 最终 = 140 × 1250 / 1000 = 175（= ×1.75）。
     * 旧写法（落笔在 `+0x28` 桩里、Saki 在链里靠后）会得到 140 —— 我们的倍率被静默抹掉。*/
    ce_dmg_reset();
    ce_dmg_set(113, 1250);
    CHECK(ce_dmg_aggregate() == 1250);
    CHECK(ce_dmg_scale(140) == 175);                   /* 140 = 别人（Saki ×1.4）算完的值 */
    CHECK(ce_dmg_scale(100) == 125);                   /* 干净基值也一样 */
    /* 两张原生装备卡都在我们前面 ⇒ ×1.75（而不是被任一家顶掉） */
    CHECK(ce_dmg_scale(112) == 140);                   /* 112 = 80 × 1.4 */
    /* 中性（没有加伤害的卡）⇒ 一个字节都不该动：`ce_sdk_bullet_damage_final` 直接 return */
    ce_dmg_reset();
    CHECK(ce_dmg_aggregate() == CE_DMG_UNIT);
    CHECK(ce_dmg_scale(140) == 140);
    /* 只乘一次（单次取整）：落笔点唯一 ⇒ 不存在「同帧施加两次」这一档。
     * 这里把它写成算例：175 再乘一次就变 218，差额 43 —— 实跑对账时就是看这个差值。*/
    ce_dmg_reset();
    ce_dmg_set(113, 1250);
    CHECK(ce_dmg_scale(ce_dmg_scale(140)) == 218);     /* ✗ 施加两次的样子（不该出现）*/
    ce_dmg_reset();

    /* ---- ② 死亡结果闩锁：引擎那一次判定的复述（-1 未知 / 0 未取消 / 1 已取消）---- */
    ce_death_result_reset();
    CHECK(ce_death_result() == CE_DEATH_RESULT_UNKNOWN);
    CHECK(ce_death_cancelled() == 0);                  /* ★ 未知**不算**「被取消」*/
    CHECK(CE_DEATH_RESULT_UNKNOWN != 0);               /* 与 0（会扣命）是两个不同的值 */
    ce_death_result_note(0);                           /* 五家跑完 acc == 0 ⇒ 会扣命 */
    CHECK(ce_death_result() == 0);
    CHECK(ce_death_cancelled() == 0);
    CHECK(ce_death_result_seq() == 1);
    ce_death_result_note(1);                           /* 有人返回 1 ⇒ 取消 */
    CHECK(ce_death_result() == 1);
    CHECK(ce_death_cancelled() == 1);
    CHECK(ce_death_result_seq() == 2);
    /* 一次死亡开始（BP_ce_player_die）⇒ 回到未知；_seq 只数判定次数，不被复位影响 */
    ce_death_result_reset();
    CHECK(ce_death_result() == CE_DEATH_RESULT_UNKNOWN);
    CHECK(ce_death_cancelled() == 0);
    CHECK(ce_death_result_seq() == 2);
    /* 非 0 一律当「取消」（与 acc 是 OR 累加器一致；别把参数当计数）*/
    ce_death_result_note(7);
    CHECK(ce_death_result() == 1 && ce_death_cancelled() == 1);
    ce_death_result_note(-3);
    CHECK(ce_death_result() == 1);
    ce_death_result_note(0);
    CHECK(ce_death_result() == 0 && ce_death_cancelled() == 0);
    CHECK(ce_death_result_seq() == 5);
    ce_death_result_reset();
}

/* ★★ 2026-09-15（`NEXT.md` §1x.2）：**自建实体 → 114 收点批**的两个通道（SDK 层）。
 * 接线级的合成语义（消费者一侧真的按 `cards/collect2x.c` 的形状一帧一帧跟生产者对跑）在
 * `tests/test_vitem_batch.c`；这里钉的是**层本身**的五条契约（唯一出处 = `sdk_core.h` 的长注）：
 *   ① 成员通道只累加、被抽干后不重复计；
 *   ② **半道具（`CE_VITEM_HALF`）两条通道都不认**（§1x.3：微档 = 消弹那档的半道具）；
 *   ③ 在飞通道按源分账、带 `CE_VITEM_HOLD_FRAMES` 容差；显式声明 0 / 过期 ⇒ 立刻不在飞；
 *   ④ **没有消费者时自过期、且不碰引擎侧任何东西**（114 不在场 = 零副作用）；
 *   ⑤ 通道**不进计分层**（`ce_score_*` 不受影响）——用户口径「只 +1，不进 SCORE」。*/
static void test_vitem_channel(void)
{
    /* ⑤ + ①：报一颗 ⇒ 抽得到一颗，SCORE / 计分层纹丝不动 */
    ce_vitem_reset();
    ce_score_reset();
    ce_score_bridge(50000, 1);                       /* 身价 50000（= 点 × 10）⇒ G_0 = 5000 点 */
    const int32_t score_before = ce_score_total();
    CHECK(score_before == 5000);
    CHECK(ce_vitem_take_collect() == 0);             /* 空：没有东西可抽 */
    CHECK(ce_vitem_collect_pending() == 0);
    ce_vitem_note_collect(CE_VITEM_FULL, 1);
    CHECK(ce_vitem_collect_pending() == 1);
    CHECK(ce_score_total() == score_before);         /* ★ 只 +1：SCORE 一分没动 */
    CHECK(ce_score_participants() == 0);             /* 也没让谁「参与」计分层 */
    CHECK(ce_vitem_take_collect() == 1);             /* 抽干 */
    CHECK(ce_vitem_take_collect() == 0);             /* 不重复计（抽走就是抽走）*/
    CHECK(ce_vitem_collect_pending() == 0 && ce_score_total() == score_before);
    /* 一帧里收了一撮点：同帧多次上报累加 */
    ce_vitem_note_collect(CE_VITEM_FULL, 3);
    ce_vitem_note_collect(CE_VITEM_FULL, 4);
    CHECK(ce_vitem_collect_pending() == 7);
    CHECK(ce_vitem_take_collect() == 7);
    /* n <= 0 = 不做事 */
    ce_vitem_note_collect(CE_VITEM_FULL, 0);
    ce_vitem_note_collect(CE_VITEM_FULL, -2);
    CHECK(ce_vitem_collect_pending() == 0);

    /* ② 半道具（微档）：上报被丢弃，且不会污染已累加的完整道具 */
    ce_vitem_reset();
    ce_vitem_note_collect(CE_VITEM_HALF, 5);
    CHECK(ce_vitem_collect_pending() == 0);
    CHECK(ce_vitem_take_collect() == 0);
    ce_vitem_note_collect(CE_VITEM_FULL, 2);
    ce_vitem_note_collect(CE_VITEM_HALF, 100);
    CHECK(ce_vitem_take_collect() == 2);             /* 只算那 2 颗完整道具 */
    ce_vitem_declare_flying(CE_VITEM_SRC_SHARED, CE_VITEM_HALF, 9);
    CHECK(ce_vitem_flying() == 0);                   /* 半道具也不算「有东西在飞」 */
    CHECK(ce_vitem_flying_from(CE_VITEM_SRC_SHARED) == 0);

    /* ③ 在飞：同帧可读 → 容差窗内仍可读（生产者/消费者同帧不同序）→ 超窗 = 不在飞 */
    ce_vitem_reset();
    ce_vitem_declare_flying(CE_VITEM_SRC_SHARED, CE_VITEM_FULL, 3);
    CHECK(ce_vitem_flying() == 3 && ce_vitem_flying_from(CE_VITEM_SRC_SHARED) == 3);
    for (int i = 0; i < CE_VITEM_HOLD_FRAMES; ++i) {
        ce_sdk_frame_bump();
        CHECK(ce_vitem_flying() == 3);               /* 生产者这一帧还没声明也读得到（顺序无关）*/
    }
    ce_sdk_frame_bump();
    CHECK(ce_vitem_flying() == 0);                   /* 超窗：生产者不再声明 ⇒ 不在飞 */
    /* 「收完了」= 生产者显式声明 0（faith 每帧都声明，池空就报 0）⇒ **当帧**就不在飞 */
    ce_vitem_declare_flying(CE_VITEM_SRC_SHARED, CE_VITEM_FULL, 2);
    CHECK(ce_vitem_flying() == 2);
    ce_vitem_declare_flying(CE_VITEM_SRC_SHARED, CE_VITEM_FULL, 0);
    CHECK(ce_vitem_flying() == 0);
    /* 多源求和 + 单源退出（删卡 / 池清空）：只撤自己那一份 —— 116 是「将来复用」的示意 */
    ce_vitem_declare_flying(CE_VITEM_SRC_SHARED, CE_VITEM_FULL, 2);
    ce_vitem_declare_flying(116, CE_VITEM_FULL, 4);
    CHECK(ce_vitem_flying() == 6);
    ce_vitem_drop(CE_VITEM_SRC_SHARED);
    CHECK(ce_vitem_flying() == 4 && ce_vitem_flying_from(116) == 4);
    ce_vitem_drop(116);
    CHECK(ce_vitem_flying() == 0);
    /* 越界源号：不写、不崩 */
    ce_vitem_declare_flying((uint32_t)CE_SDK_CARD_ID_MAX + 1, CE_VITEM_FULL, 7);
    ce_vitem_drop((uint32_t)CE_SDK_CARD_ID_MAX + 1);
    CHECK(ce_vitem_flying() == 0 && ce_vitem_flying_from(999) == 0);

    /* ④ 114 不在场（没人抽干 / 没人读）= 零副作用：两条通道都**自过期**，不留状态、不动引擎侧 */
    ce_vitem_reset();
    ce_score_reset();
    ce_score_bridge(10000, 1);
    const int32_t sc = ce_score_total();
    ce_vitem_note_collect(CE_VITEM_FULL, 3);         /* 没有消费者 */
    ce_vitem_declare_flying(CE_VITEM_SRC_SHARED, CE_VITEM_FULL, 2);
    CHECK(ce_vitem_collect_pending() == 3 && ce_vitem_flying() == 2);
    for (int i = 0; i <= CE_VITEM_HOLD_FRAMES; ++i) ce_sdk_frame_bump();
    CHECK(ce_vitem_collect_pending() == 0);          /* 过期：陈年计数作废（不会攒到下一次抽干）*/
    CHECK(ce_vitem_take_collect() == 0);
    CHECK(ce_vitem_flying() == 0);
    CHECK(ce_score_total() == sc && ce_score_participants() == 0);   /* 引擎侧的分一分没动 */
    /* 上限钳：没人抽干时也不会溢出（钳在 CE_VITEM_NOTE_MAX）*/
    ce_vitem_reset();
    for (int i = 0; i < 10; ++i) ce_vitem_note_collect(CE_VITEM_FULL, 1000);
    CHECK(ce_vitem_collect_pending() == CE_VITEM_NOTE_MAX);
    CHECK(ce_vitem_take_collect() == CE_VITEM_NOTE_MAX);
    ce_vitem_reset();
    CHECK(ce_vitem_collect_pending() == 0 && ce_vitem_flying() == 0);

}

/* ---- ★ PoC 高度层（收点线；2026-09-15）--------------------------------------------
 * 层语义 / 一手（两个写点 `0x45ae8f`=128.0 / `0x40c275`=224.0、四个读点全在「线上/线下」）
 * 见 `sdk_core.h` 的块注释与 `engine/player/th18/05-poc-height-and-indicator.md`。*/
static void test_poc_layer(void)
{
    const uint32_t D = CE_POC_HEIGHT_DEFAULT_BITS;   /* 128.0f */
    const uint32_t K = CE_POC_HEIGHT_KANAKO_BITS;    /* 224.0f */
    uint32_t out = 0u;

    /* ① 位型常量是真的（与一手机器码里的立即数逐位相同）*/
    CHECK(CE_POC_HEIGHT_DEFAULT_BITS == 0x43000000u);
    CHECK(CE_POC_HEIGHT_KANAKO_BITS == 0x43600000u);
    CHECK(ce_f32_bits(128.0f) == 0x43000000u && ce_f32_bits(224.0f) == 0x43600000u);
    CHECK(ce_f32_from_bits(0x43600000u) == 224.0f);
    CHECK(ce_f32_bits(448.0f) == CE_POC_HEIGHT_MAX_BITS);
    CHECK(ce_f32_bits(0.0f) == 0u);
    /* 非负 float 的位型序 = 数值序（本层全靠这条：聚合/夹取都在整数域做）*/
    CHECK(CE_POC_HEIGHT_DEFAULT_BITS < CE_POC_HEIGHT_KANAKO_BITS);
    CHECK(CE_POC_HEIGHT_KANAKO_BITS < CE_POC_HEIGHT_MAX_BITS);

    /* ② 没声明：一个数都不写，访问器就读引擎字段的实读值 */
    ce_poc_reset();
    CHECK(!ce_poc_has_declaration());
    CHECK(ce_poc_winner() == -1);
    CHECK(ce_poc_target_bits() == 0u);
    CHECK(ce_poc_effective_bits(D) == D);             /* 无人声明 ⇒ 引擎值原样（含原生卡 22 的 224）*/
    CHECK(ce_poc_effective_bits(K) == K);
    CHECK(!ce_poc_frame(D, &out));                    /* 帧决策：本帧别写 */
    CHECK(!ce_poc_base(&out));

    /* ③ 单卡声明：落笔 = 声明值；基准 = 动手之前的字段值 */
    ce_poc_reset();
    CHECK(ce_poc_declare(111, K));
    CHECK(ce_poc_has_declaration());
    CHECK(ce_poc_winner() == 111);
    CHECK(ce_poc_target_bits() == K);
    CHECK(ce_poc_effective_bits(D) == K);             /* 有声明 ⇒ 生效值 = 声明值 */
    CHECK(ce_poc_frame(D, &out) && out == K);
    CHECK(ce_poc_base(&out) && out == D);             /* 基准 = 128（我们动手前字段里的值）*/
    CHECK(ce_poc_frame(K, &out) && out == K);         /* 幂等：同值照写 */
    CHECK(ce_poc_base(&out) && out == D);             /* 基准不被自己刷新 */

    /* ④ 多卡在场：**取最大**（线最低的那条），并列取 id 最小；赢家可查 */
    CHECK(ce_poc_declare(114, ce_f32_bits(160.0f)));
    CHECK(ce_poc_target_bits() == K && ce_poc_winner() == 111);
    CHECK(ce_poc_frame(K, &out) && out == K);
    CHECK(ce_poc_declare(58, ce_f32_bits(300.0f)));
    CHECK(ce_poc_target_bits() == ce_f32_bits(300.0f) && ce_poc_winner() == 58);
    CHECK(ce_poc_frame(K, &out) && out == ce_f32_bits(300.0f));
    ce_poc_drop(58);
    CHECK(ce_poc_target_bits() == K && ce_poc_winner() == 111);
    ce_poc_reset();
    CHECK(ce_poc_declare(111, K) && ce_poc_declare(114, K));   /* 并列：id 小者当赢家（只用于诊断）*/
    CHECK(ce_poc_winner() == 111);

    /* ⑤ 夹取与拒收：> 448 夹到 448；0 / 负 / NaN / Inf / 越界 id 一律不成立 */
    ce_poc_reset();
    CHECK(ce_poc_declare(111, ce_f32_bits(1000.0f)));
    CHECK(ce_poc_target_bits() == CE_POC_HEIGHT_MAX_BITS);
    CHECK(ce_poc_frame(D, &out) && out == CE_POC_HEIGHT_MAX_BITS);
    CHECK(!ce_poc_declare(111, 0u));                          /* 0.0f = 退出声明 */
    CHECK(!ce_poc_has_declaration());
    CHECK(!ce_poc_declare(111, 0xbf800000u));                 /* -1.0f */
    CHECK(!ce_poc_declare(111, 0x7f800000u));                 /* +Inf */
    CHECK(!ce_poc_declare(111, 0x7fc00000u));                 /* NaN */
    CHECK(!ce_poc_declare(255 + 1u, K));                      /* id 越界 */
    CHECK(!ce_poc_has_declaration());

    /* ⑥ 「被删掉会怎样」：声明全撤 ⇒ 写回**基准**（不是写死 128）*/
    ce_poc_reset();
    CHECK(ce_poc_declare(111, ce_f32_bits(256.0f)));
    CHECK(ce_poc_frame(K, &out) && out == ce_f32_bits(256.0f));   /* 场景：原生卡 22 已把线写到 224 */
    CHECK(ce_poc_base(&out) && out == K);
    CHECK(ce_poc_drop_take_base(111, &out) && out == K);          /* 删卡 ⇒ 立刻写回 224（不是 128）*/
    CHECK(!ce_poc_has_declaration());
    CHECK(!ce_poc_drop_take_base(111, &out));                     /* 基准已消费 ⇒ 不再重复写 */
    /* 还有人声明时删一张：不写回，剩下的那条继续生效 */
    ce_poc_reset();
    CHECK(ce_poc_declare(111, K) && ce_poc_declare(114, ce_f32_bits(160.0f)));
    CHECK(ce_poc_frame(D, &out) && out == K);
    CHECK(!ce_poc_drop_take_base(114, &out));                     /* 114 走了，111 还在 */
    CHECK(ce_poc_has_declaration() && ce_poc_target_bits() == K);
    CHECK(ce_poc_drop_take_base(111, &out) && out == D);          /* 最后一个走 ⇒ 写回基准 128 */

    /* ⑦ 别人中途改过 ⇒ 基准跟着刷新（原生卡 22 / `Player__reset` 都可能动手）*/
    ce_poc_reset();
    CHECK(ce_poc_declare(111, ce_f32_bits(160.0f)));
    CHECK(ce_poc_frame(D, &out) && out == ce_f32_bits(160.0f));
    CHECK(ce_poc_frame(K, &out) && out == ce_f32_bits(160.0f));   /* 字段被改成 224（≠ 我们写的 160）*/
    CHECK(ce_poc_base(&out) && out == K);                         /* ⇒ 基准跟到 224 */
    CHECK(ce_poc_drop_take_base(111, &out) && out == K);

    /* ⑧ 无声明时**一个字都不写**（帧决策返回 0）——否则会把原生卡 22 的 224 顶回 128 */
    ce_poc_reset();
    out = 0xdeadbeefu;
    CHECK(!ce_poc_frame(K, &out) && out == 0xdeadbeefu);
    CHECK(!ce_poc_frame(D, &out) && out == 0xdeadbeefu);
    CHECK(!ce_poc_effective_bits(D) == 0);                        /* 未声明时生效值 = 引擎值（不是 0）*/
    CHECK(ce_poc_effective_bits(12345u) == 12345u);
    ce_poc_reset();
}

/* ============================ 共享 PIV 池 + 擦弹族（用户 2026-09-18）============================
 * 口径：**池一份、自动相加**（谁都往它写）；**写入闸在写这一侧**（每卡可写 = `cap_i − init_i`，写满不写）；
 * 擦弹是一个**共用写入者**（只算一次）。*/
static void test_shared_piv_and_graze(void)
{
    ce_score_reset();
    ce_graze_bridge(0);
    ce_piv_reset();

    /* ① 没人声明 ⇒ 池为 0、不参与（participants == 0 ⇒ SDK 一个字不写）。*/
    CHECK(ce_piv_pool() == 0);
    CHECK(ce_piv_family_points() == 0);
    CHECK(ce_graze_points() == 0);
    CHECK(ce_score_participants() == 0);

    /* ② 单卡：claim(114, 95000)（用户举例：th13 EASY 100000 − 5000 = 95000）。*/
    CHECK(ce_piv_claim(114, 95000) == 1);
    CHECK(ce_piv_present_count() == 1);
    CHECK(ce_piv_cap_of(114) == 95000);
    CHECK(ce_piv_room(114) == 95000);
    ce_piv_add(114, 1234);
    CHECK(ce_piv_pool() == 1234);
    CHECK(ce_piv_written(114) == 1234);
    CHECK(ce_piv_family_points() == 1234);
    CHECK(ce_score_participants() == 1);

    /* ③ **写入闸**：它累计最多 95000 ⇒ 写 200000 只有 95000 进池（其余被闸掉）。*/
    ce_piv_add(114, 200000);
    CHECK(ce_piv_pool() == 95000);
    CHECK(ce_piv_written(114) == 95000);
    CHECK(ce_piv_room(114) == 0);
    ce_piv_add(114, 5000);                              /* 闸到底，再加也不动 */
    CHECK(ce_piv_pool() == 95000);

    /* ④ 多卡同装 = **多一套系统往同一块池里写**（用户：多加了一套系统）：115 的上限 490000。
     *    池**自动相加**，但族项仍然只进公式**一份**（= 池的值）。*/
    CHECK(ce_piv_claim(115, 490000) == 1);
    CHECK(ce_piv_present_count() == 2);
    ce_piv_add(115, 100000);
    CHECK(ce_piv_pool() == 195000);                     /* 95000 + 100000 */
    CHECK(ce_piv_family_points() == 195000);            /* ★ 一份，不是两份 */
    CHECK(ce_piv_reading(114) == 195000);
    CHECK(ce_piv_reading(115) == 195000);
    CHECK(ce_score_participants() == 2);

    /* ⑤ 未 claim 的 id 写不进去（删卡之后不该继续涨）。*/
    ce_piv_add(113, 5000);
    CHECK(ce_piv_pool() == 195000);

    /* ⑥ 退场：那张卡的账清了，池**保留**（已经写进去的是既成事实）。*/
    ce_piv_release(114);
    CHECK(ce_piv_present_count() == 1);
    CHECK(ce_piv_written(114) == 0);
    CHECK(ce_piv_family_points() == 195000);

    /* ⑦ 擦弹那一个**共用写入者**：族内无卡 = 0；有卡 = 本局擦弹数；两张同族**仍是同一个数**。*/
    ce_graze_bridge(4321);
    CHECK(ce_graze_points() == 0);
    CHECK(ce_graze_family_claim(114) == 1);
    CHECK(ce_graze_points() == 4321);
    CHECK(ce_graze_family_claim(115) == 1);
    CHECK(ce_graze_points() == 4321);                   /* ★ 不叠加 */
    ce_graze_family_release(114);
    CHECK(ce_graze_points() == 4321);                   /* 还有 115 在 ⇒ 仍算一次 */
    ce_graze_family_release(115);
    CHECK(ce_graze_points() == 0);

    /* ⑧ 新局：池归 0 + 各卡已写账清零；在场声明保留。*/
    ce_piv_reset();
    CHECK(ce_piv_pool() == 0);
    CHECK(ce_piv_present_count() == 1);                 /* 115 仍在场 */
    CHECK(ce_piv_written(115) == 0);
    CHECK(ce_piv_room(115) == 490000);                  /* 账清干净 ⇒ 又能写满 */

    /* ⑨ 进 G：G = G_0 + Σ(未耦合/legacy/耦合) + **池一份** + 擦弹一项。*/
    ce_score_bridge(200000, CE_SCORE_DIFF_NORMAL);      /* 身价 200000 ⇒ G_0 = 20000 点 */
    ce_piv_add(115, 300000);
    CHECK(ce_piv_family_points() == 300000);
    CHECK(ce_score_total() == ce_score_base_points() + 300000);
    ce_graze_family_claim(114);
    ce_graze_bridge(777);
    CHECK(ce_score_total() == ce_score_base_points() + 300000 + 777);

    /* ⑩ 内置表入口：112–117 × E/N/H/L/EX 的可写上限 = cap−init（一手表；抽查三档）。*/
    CHECK(ce_piv_cap_for(113, CE_SCORE_DIFF_EASY) == 95000);      /* th13 E: 100000−5000 */
    CHECK(ce_piv_cap_for(113, CE_SCORE_DIFF_NORMAL) == 240000);   /* th13 N: 250000−10000 */
    CHECK(ce_piv_cap_for(112, CE_SCORE_DIFF_EXTRA) == 480000);    /* th12 EX: 500000−20000 */
    CHECK(ce_piv_cap_for(116, CE_SCORE_DIFF_LUNATIC) == 490000);  /* th16 L: 500000−10000 */
    CHECK(ce_piv_cap_for(111, CE_SCORE_DIFF_NORMAL) == -1);       /* 不在表里 */
    CHECK(ce_piv_cap_for(113, 9) == -1);                          /* 难度越界 */
    ce_piv_release(113);
    CHECK(ce_piv_claim_auto(113, CE_SCORE_DIFF_NORMAL) == 1);
    CHECK(ce_piv_cap_of(113) == 240000);
    ce_piv_add(113, 500000);                                      /* 闸：只进 240000 */
    CHECK(ce_piv_written(113) == 240000);
    ce_piv_release(113);

    /* 收尾：退干净，别影响后面的单测。*/
    ce_graze_family_release(114);
    ce_graze_family_release(115);
    ce_piv_release(115);
    ce_piv_reset();
    CHECK(ce_piv_family_points() == 0);
    CHECK(ce_graze_points() == 0);
}

/* ============================ 计分层换版：每项走自己原作的曲线（用户 2026-09-18，路线 A）============
 * 写出去的 = `G_0 + Σ ΔG_i × (coeff_i/coeff_0)`；引擎随后乘 `coeff_0` ⇒ 到手 = `G_0·coeff_0 + Σ ΔG_i·coeff_i`。*/
static void test_per_family_coeff(void)
{
    const int32_t Q = 65536;                    /* Q16 定点的 1.0 */
    ce_score_reset();
    ce_score_bridge(200000, CE_SCORE_DIFF_NORMAL);   /* 身价 200000 ⇒ G_0 = 20000 点 */

    /* ① 线上（无几何 = 满额）：coeff_0 = 1000、任何族比值都是 1 ⇒ 与老行为逐字节相同。*/
    ce_score_bridge_geom(0, 0);
    CHECK(ce_score_coeff0_permille() == 1000);
    CHECK(ce_score_ratio_permille(CE_COEFF_FAMILY_NONE) == 1000);
    CHECK(ce_score_ratio_permille(CE_COEFF_FAMILY_075) == 1000);
    CHECK(ce_score_ratio_permille(CE_COEFF_FAMILY_08) == 1000);
    CHECK(ce_score_ratio_permille(CE_COEFF_FAMILY_1) == 1000);
    ce_score_family_declare(117, CE_COEFF_FAMILY_075);
    ce_score_declare_delta(117, 6000);
    CHECK(ce_score_total() == ce_score_base_points() + 6000);

    /* ② 线下 d = 100 px：coeff_0 = 900×(450−100)/450 = 700‰；0.75 族 = 750×350/450 = 583‰
     *   ⇒ 比值 583×1000/700 = 832（≈5/6）。*/
    ce_score_bridge_geom(100 * Q, 200 * Q);
    CHECK(ce_score_coeff0_permille() == 700);
    CHECK(ce_score_ratio_permille(CE_COEFF_FAMILY_075) == 832);
    CHECK(ce_score_total() == ce_score_base_points() + 6000 * 832 / 1000);
    /* 不吃折扣的族（th07 / th11）：反归一放大 ⇒ 1000×1000/700 = 1428‰ */
    ce_score_family_declare(107, CE_COEFF_FAMILY_1);
    ce_score_declare_delta(107, 5000);
    CHECK(ce_score_ratio_permille(CE_COEFF_FAMILY_1) == 1428);
    CHECK(ce_score_total() == ce_score_base_points() + 6000 * 832 / 1000 + 5000 * 1428 / 1000);
    /* th08 的 0.5×(1−d/500)：`500 − d` = 400‰ ⇒ 比值 571‰ */
    ce_score_family_declare(108, CE_COEFF_FAMILY_08);
    ce_score_declare_delta(108, 6000);
    CHECK(ce_score_ratio_permille(CE_COEFF_FAMILY_08) == 571);
    /* 未声明族的卡（= 所有老卡）比值仍是 1 ⇒ 不进归一 */
    ce_score_declare_delta(58, 7000);
    CHECK(ce_score_total() == ce_score_base_points() + 6000 * 832 / 1000 + 5000 * 1428 / 1000
                              + 6000 * 571 / 1000 + 7000);

    /* ③ 深度 ≥ 450 px：引擎那一档只剩「下限 +1 分」⇒ coeff_0 = 0 ⇒ **不归一**（各比值回 1000）。*/
    ce_score_bridge_geom(0, 500 * Q);
    CHECK(ce_score_coeff0_permille() == 0);
    CHECK(ce_score_ratio_permille(CE_COEFF_FAMILY_075) == 1000);
    CHECK(ce_score_ratio_permille(CE_COEFF_FAMILY_1) == 1000);

    /* ④ th10（110 那一族）：曲线 `0.5 − d×(F−100000)/(6080·F)`，**d 取道具 y**。
     *   设 d_自机 = 0（线上）与 d_道具 = 100 px、F = 999990 ⇒ 千分点
     *   `500 − 100×(999990−100000)×1000/(6080×999990)` = 500 − 14 = 486‰ ⇒ 比值 */
    ce_score_bridge_geom2(200 * Q, 100 * Q, 300 * Q);   /* 自机线下 100px、道具线下 200px */
    ce_score_family10_faith(999990);                     /* F = 上限档 */
    CHECK(ce_score_coeff0_permille() == 700);            /* coeff_0 只看自机 y */
    ce_score_family_declare(110, CE_COEFF_FAMILY_10);
    ce_score_declare_delta(110, 10000);
    {
        int r075 = ce_score_ratio_permille(CE_COEFF_FAMILY_075);
        int r1   = ce_score_ratio_permille(CE_COEFF_FAMILY_1);
        int r08  = ce_score_ratio_permille(CE_COEFF_FAMILY_08);
        int r10  = ce_score_ratio_permille(CE_COEFF_FAMILY_10);
        CHECK(r10 == 672);       /* coeff_10 = 500 − 200×899990×1000/(6080×999990) = 471‰
                                  * ⇒ 比值 471×1000/coeff_0(=700) = 672‰ */
        /* 同几何下 08 用**道具 y**（d=200）⇒ `500−200` = 300‰ ⇒ 比值 428‰（自机 d 只有 100）*/
        CHECK(r08 == 428);
        CHECK(ce_score_total() == ce_score_base_points()
                                  + 6000 * r075 / 1000 + 5000 * r1 / 1000
                                  + 6000 * r08 / 1000 + 7000 + 10000 * r10 / 1000);
    }
    /* `F` 没填 / 自机在线下但道具在线上 ⇒ 不归一（回 1000） */
    ce_score_family10_faith(0);
    CHECK(ce_score_ratio_permille(CE_COEFF_FAMILY_10) == 1000);
    ce_score_family_declare(110, CE_COEFF_FAMILY_NONE);
    ce_score_declare_delta(110, 0);

    /* 收尾 */
    ce_score_family_declare(117, CE_COEFF_FAMILY_NONE);
    ce_score_family_declare(107, CE_COEFF_FAMILY_NONE);
    ce_score_family_declare(108, CE_COEFF_FAMILY_NONE);
    ce_score_declare_delta(117, 0);
    ce_score_declare_delta(107, 0);
    ce_score_declare_delta(108, 0);
    ce_score_declare_delta(58, 0);
    ce_score_bridge_geom(0, 0);
}

/* ---- ★★ 2026-09-20「遮」声明 / 语义高度 / 线显示 / 折额兜底 -------------------------------
 * 语义与全部一手（字段的 2 写 5 读、两条判据的严格/非严格之差 `ja` vs `jae`、帧内顺序
 * Player 0x17 → ItemManager 0x1e、线显示位 `Gui+0x194` bit 0x2000）见 `sdk_core.h` 的那一节；
 * 卡侧用法见 `sdk.h`；背景 = `AUDIT.md` §BE-EQ / §BE-ER 与 `WORK-PACKAGES.md` §7 A/A2/A3/A4。*/
/* Q16 定点（与 `sdk.h` 的 `ce_q16_from_bits` 同式；本单测不 include `sdk.h`）。*/
static int32_t q16(float f) { return (int32_t)(f * 65536.0f); }

static void test_poc_mask_layer(void)
{
    const uint32_t D    = CE_POC_HEIGHT_DEFAULT_BITS;    /* 128.0f（Player__reset）*/
    const uint32_t K    = CE_POC_HEIGHT_KANAKO_BITS;     /* 224.0f（原生卡 22 on_load）*/
    const uint32_t Y100 = ce_f32_bits(100.0f);           /* 「自机在语义线上方」*/
    const uint32_t Y300 = ce_f32_bits(300.0f);           /* 「自机在语义线下方」*/
    uint32_t out = 0u;

    /* ① `CE_POC_TINY_BITS` 是「极小正数」：非 0（0 是哨兵）、非负（负的位型序会翻）、远小于任何自机 y */
    CHECK(CE_POC_TINY_BITS != 0u);
    CHECK((CE_POC_TINY_BITS & 0x80000000u) == 0u);
    CHECK(ce_f32_from_bits(CE_POC_TINY_BITS) > 0.0f);
    CHECK(CE_POC_TINY_BITS < ce_f32_bits(0.001f));
    CHECK(CE_POC_TINY_BITS < Y100);

    /* ② 声明 / 撤回 / owner（粘性；`ce_poc_drop` 两个都撤）*/
    ce_poc_reset();
    CHECK(!ce_poc_mask_active() && ce_poc_mask_owner() == -1);
    CHECK(ce_poc_mask_declare(106, 1));
    CHECK(ce_poc_mask_active() && ce_poc_mask_owner() == 106);
    CHECK(ce_poc_has_declaration());                 /* 「遮」也算声明（线显示那一路要用）*/
    CHECK(ce_poc_mask_declare(111, 1) && ce_poc_mask_owner() == 106);   /* 并列取 id 最小（诊断用）*/
    ce_poc_drop(111);
    CHECK(ce_poc_mask_active() && ce_poc_mask_owner() == 106);
    ce_poc_drop(106);
    CHECK(!ce_poc_mask_active() && !ce_poc_has_declaration());

    /* ③ 没别家声明：语义高度 = 字段实读值（= 基准 128 / 原生卡 22 的 224）*/
    ce_poc_reset();
    CHECK(ce_poc_semantic_bits(D) == D);
    CHECK(ce_poc_semantic_bits(K) == K);

    /* ④ 落笔值 = `min(语义高度, 自机 y)`：这正是「不吸」与「线上满额」同时成立的支点 */
    ce_poc_reset();
    ce_poc_mask_declare(106, 1);
    CHECK(ce_poc_frame_masked(D, Y300, &out) && out == D);      /* y 在语义线下方 ⇒ 落笔 = 语义高度 */
    CHECK(ce_poc_frame_masked(D, Y100, &out) && out == Y100);   /* y 在语义线上方 ⇒ 落笔 == y（严格 ja 不成立、jae 成立）*/
    CHECK(ce_poc_frame_masked(D, 0u, &out) && out == CE_POC_TINY_BITS);   /* 读不到 y ⇒ 极小正数（永不过线）*/
    /* 基准记账：第一次落笔之前字段里是什么，删卡时就写回什么 */
    CHECK(ce_poc_base(&out) && out == D);
    CHECK(ce_poc_drop_take_base(106, &out) && out == D);

    /* ⑤ 与别家声明同场：**「遮」忽略别家高度**（覆盖位），但语义高度仍取别家的值 */
    ce_poc_reset();
    CHECK(ce_poc_declare(111, ce_f32_bits(160.0f)));
    CHECK(ce_poc_mask_declare(106, 1));
    CHECK(ce_poc_semantic_bits(D) == ce_f32_bits(160.0f));      /* 语义 = 111 的声明（不含「遮」）*/
    CHECK(ce_poc_frame_masked(K, Y300, &out) && out == ce_f32_bits(160.0f));
    CHECK(ce_poc_frame_masked(K, Y100, &out) && out == Y100);
    CHECK(ce_poc_winner() == 111);                              /* 固定高度那条的赢家照旧可查 */
    /* 互斥：同 id 后报的绝对高度会顶掉「遮」 */
    CHECK(ce_poc_declare(106, K) && !ce_poc_mask_active());
    CHECK(ce_poc_target_bits() == K && ce_poc_winner() == 106);
    ce_poc_reset();

    /* ⑥ 语义访问器的**输入**是字段实读值：遮在场时它照样给语义值（不含遮）*/
    ce_poc_reset();
    ce_poc_mask_declare(106, 1);
    CHECK(ce_poc_frame_masked(K, Y300, &out) && out == K);      /* 字段被压到 224（= 语义高度）*/
    CHECK(ce_poc_semantic_bits(out) == K);
    CHECK(ce_poc_base(&out) && out == K);                       /* 基准 = 动手之前的 224（不是写死 128）*/
    CHECK(ce_poc_frame_masked(K, Y100, &out) && out == Y100);   /* 字段又被压到自机 y */
    CHECK(ce_poc_semantic_bits(out) == K);                      /* ← 语义仍是 224，不是自机 y */
    ce_poc_reset();

    /* ⑦ 线显示位（`Gui+0x194` bit 0x2000）：清 + 基准记账 + 撤完写回；没人 hide 时照旧「有人声明就打开」*/
    ce_poc_line_reset();
    {
        int bit = 7;
        CHECK(!ce_poc_line_hidden() && ce_poc_line_hide_owner() == -1);
        /* 旧行为：没有人 hide、有人声明高度 ⇒ 把线打开（照原生卡 22）*/
        CHECK(ce_poc_line_apply(0, 1, &bit) && bit == 1);
        CHECK(!ce_poc_line_apply(1, 1, &bit));                  /* 已经是开的 ⇒ 一个字都不写 */
        CHECK(!ce_poc_line_apply(0, 0, &bit));                  /* 没声明也没 hide ⇒ 不动 */
        /* 106 声明「不画」：第一次清之前把「动手之前」的那一位记成基准（这里是 1 = 原生卡 22 已置）*/
        ce_poc_line_hide_declare(106, 1);
        CHECK(ce_poc_line_hidden() && ce_poc_line_hide_owner() == 106);
        CHECK(ce_poc_line_apply(1, 1, &bit) && bit == 0);
        CHECK(!ce_poc_line_apply(0, 1, &bit));                  /* 已经清了 ⇒ 幂等 */
        /* 删卡（商店里帧钩子不跑）：立刻写回基准 1 */
        CHECK(ce_poc_line_release(106, &bit) && bit == 1);
        CHECK(!ce_poc_line_hidden() && !ce_poc_line_release(106, &bit));
        /* 基准本来是 0（原生卡 22 没置过）⇒ 撤完不用写 */
        ce_poc_line_reset();
        ce_poc_line_hide_declare(106, 1);
        CHECK(!ce_poc_line_apply(0, 1, &bit));                  /* 第一次清：基准 = 0；本来就是清的 ⇒ 不写 */
        CHECK(ce_poc_line_release(106, &bit) && bit == 0);      /* 撤完按基准 0 写回 */
        ce_poc_line_reset();
        ce_poc_line_hide_declare(106, 1);
        CHECK(ce_poc_line_apply(1, 1, &bit) && bit == 0);       /* 基准 = 1（原生卡 22 置过）*/
        CHECK(ce_poc_line_release(106, &bit) && bit == 1);
        /* 还有别人 hide ⇒ 不写回 */
        ce_poc_line_reset();
        ce_poc_line_hide_declare(106, 1);
        ce_poc_line_hide_declare(107, 1);
        CHECK(ce_poc_line_apply(1, 1, &bit) && bit == 0);
        CHECK(!ce_poc_line_release(106, &bit));                 /* 107 还在声明 */
        CHECK(ce_poc_line_hidden() && ce_poc_line_hide_owner() == 107);
        CHECK(ce_poc_line_release(107, &bit) && bit == 1);      /* 最后一个走 ⇒ 写回基准 1 */
        ce_poc_line_reset();
    }

    /* ⑧ 折额兜底：落笔值就是语义值时**恒 1000**（正常路径一个字节都不动）*/
    {
        const int32_t y300 = q16(300.0f);
        const int32_t y100 = q16(100.0f);
        const int32_t k    = q16(224.0f);

        CHECK(ce_poc_fold_ratio_permille(y300, k, k, 0) == 1000);   /* 线下：两个 d 相同 */
        CHECK(ce_poc_fold_ratio_permille(y100, y100, k, 0) == 1000);/* 线上：落笔 == y（满额支）*/
        CHECK(ce_poc_fold_ratio_permille(y300, k, k, 1) == 1000);   /* 真空飞行 ⇒ 无可补偿 */
        /* 字段被第三方改烂（这里是「固定极小正数」那种写法）：d_实际 = 300、d_语义 = 76
         *   c_实际 = 900·150/450 = 300、c_语义 = 900·374/450 = 748 ⇒ 倍率 2493‰（≤4000 才认）*/
        CHECK(ce_poc_fold_ratio_permille(y300, q16(0.0f), k, 0) == 2493);
        /* 实际系数已经到底（d_实际 ≥ 450）⇒ 掰不回来 ⇒ 不动 */
        CHECK(ce_poc_fold_ratio_permille(q16(448.0f), q16(0.0f), k, 0) == 1000);
        /* 只往上补：语义比实际更差（d_语义 > d_实际）⇒ 不动 */
        CHECK(ce_poc_fold_ratio_permille(y300, k, CE_POC_TINY_BITS, 0) == 1000);
    }
}

/* ★★ 2026-10-01：**跨卡只读标志**（`HANDOFF-108-CROSSCARD-SHIFT.md` 方案①）——
 * 置 / 读 / 任意聚合 / 粘性 / 撤（= `ce_layer_drop_card` 的收口）/ 越界防护。*/
static void test_card_flags(void)
{
    ce_card_flag_reset();
    CHECK(ce_card_flag_get(113) == 0);
    CHECK(ce_card_flag_any(CE_CARD_FLAG_TRANCE) == 0);

    ce_card_flag_set(113, CE_CARD_FLAG_TRANCE);
    CHECK(ce_card_flag_get(113) == CE_CARD_FLAG_TRANCE);
    CHECK(ce_card_flag_any(CE_CARD_FLAG_TRANCE) == 1);
    CHECK(ce_card_flag_any(CE_CARD_FLAG_ROAR) == 0);          /* 位不相干 */

    ce_card_flag_set(117, CE_CARD_FLAG_ROAR);
    CHECK(ce_card_flag_any(CE_CARD_FLAG_ROAR) == 1);
    CHECK(ce_card_flag_any(CE_CARD_FLAG_TRANCE | CE_CARD_FLAG_ROAR) == 1);   /* 任一位命中即 1 */

    /* 粘性：读方不刷新，值就一直是它（不做「当帧有效、跨帧自过期」）*/
    CHECK(ce_card_flag_get(113) == CE_CARD_FLAG_TRANCE);

    /* 撤 = `ce_layer_drop_card()` 里那一行（删卡 / 局末 / 换关的唯一收口）*/
    ce_card_flag_set(113, 0u);
    CHECK(ce_card_flag_get(113) == 0);
    CHECK(ce_card_flag_any(CE_CARD_FLAG_TRANCE) == 0);
    CHECK(ce_card_flag_any(CE_CARD_FLAG_TRANCE | CE_CARD_FLAG_ROAR) == 1);   /* 117 仍在场 */

    /* 越界 id：不写、不读（数组不许被越界索引）*/
    ce_card_flag_set((uint32_t)CE_SDK_CARD_ID_MAX + 1u, CE_CARD_FLAG_ROAR);
    CHECK(ce_card_flag_get((uint32_t)CE_SDK_CARD_ID_MAX + 1u) == 0);

    ce_card_flag_reset();
    CHECK(ce_card_flag_any(CE_CARD_FLAG_TRANCE | CE_CARD_FLAG_ROAR) == 0);
}

int main(void)
{
    test_dmg_layer();
    test_card_flags();
    test_declare_layers();
    test_drop_layer();
    test_hud_slots();
    test_score_rework();
    test_shared_piv_and_graze();
    test_per_family_coeff();
    test_score_base_unit_identity();
    test_interest_and_pick();
    test_judgment();
    test_royal();
    test_registry();
    test_bind_check();
    test_contract_check();
    test_state();
    test_native_conflict_fixes();
    test_vitem_channel();
    test_poc_layer();
    test_poc_mask_layer();
    printf("test_sdk_core: %d passed, %d failed\n", s_pass, s_fail);
    return s_fail ? 1 : 0;
}
