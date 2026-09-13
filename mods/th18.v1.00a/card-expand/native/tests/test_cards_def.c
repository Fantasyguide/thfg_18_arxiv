/* test_cards_def.c —— cards_def.c 的主机单测（Linux gcc 跑，不碰 Windows / thcrap）。
 *
 *   make test-host
 *
 * 只测纯逻辑：默认值、文案校验、字段范围、表行编码、商店容量。JSON 胶水（cards.c）只能在游戏里验。
 */
#include <stdio.h>
#include <string.h>
#include "../cards_def.h"

static int s_fail, s_pass;
#define CHECK(cond) do { if (cond) ++s_pass; else { ++s_fail; printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } } while (0)

static uint32_t rd32(const uint8_t *p) { uint32_t v; memcpy(&v, p, 4); return v; }
static void wr32(uint8_t *p, uint32_t v) { memcpy(p, &v, 4); }

static void test_defaults(void)
{
    ce_card_def_t d;
    ce_card_def_defaults(&d, 58);
    CHECK(d.id == 58);
    CHECK(strcmp(d.internal_name, "58") == 0);
    CHECK(d.name[0] == 0 && d.ndesc == 0);
    CHECK(d.f08 == 0 && d.category == 2 && d.dmode == 0 && d.repeatable == 0);
    CHECK(d.deck_visible == 1 && d.initial_unlocked == 0 && d.hud_show == 1);
    CHECK(d.price_tier == CE_CARD_UNSET && d.weight == CE_CARD_UNSET);
    CHECK(d.sprite_large == CE_CARD_UNSET && d.sprite_small == CE_CARD_UNSET);
}

static void test_text_ok(void)
{
    char s64[65], s63[64];
    memset(s64, 'a', 64); s64[64] = 0;
    memset(s63, 'a', 63); s63[63] = 0;
    CHECK(ce_card_text_ok("测试卡牌"));
    CHECK(ce_card_text_ok(s63));
    CHECK(!ce_card_text_ok(s64));           /* 64 字节：放不进 0x40 连 NUL */
    CHECK(!ce_card_text_ok(""));
    CHECK(!ce_card_text_ok(NULL));
    CHECK(!ce_card_text_ok("100%"));        /* 会被当 printf 格式串 */
}

static ce_card_def_t good(void)
{
    ce_card_def_t d;
    ce_card_def_defaults(&d, 58);
    strcpy(d.name, "测试卡牌");
    strcpy(d.desc[0], "第一行"); strcpy(d.desc[1], "第二行"); d.ndesc = 2;
    d.price_tier = 5; d.weight = 2; d.sprite_large = 116; d.sprite_small = 117;
    return d;
}

static void test_validate(void)
{
    char err[128];
    ce_card_def_t d = good();
    CHECK(ce_card_validate(&d, err, sizeof err));

    d = good(); d.id = 57;  CHECK(!ce_card_validate(&d, err, sizeof err)); CHECK(strstr(err, "id") != NULL);
    d = good(); d.id = 255; CHECK(!ce_card_validate(&d, err, sizeof err));
    d = good(); d.id = 254; CHECK(ce_card_validate(&d, err, sizeof err));
    d = good(); d.name[0] = 0;              CHECK(!ce_card_validate(&d, err, sizeof err)); CHECK(strstr(err, "name") != NULL);
    d = good(); strcpy(d.name, "a%b");      CHECK(!ce_card_validate(&d, err, sizeof err));
    d = good(); strcpy(d.desc[1], "x%");    CHECK(!ce_card_validate(&d, err, sizeof err)); CHECK(strstr(err, "desc") != NULL);
    d = good(); d.ndesc = 7;                CHECK(!ce_card_validate(&d, err, sizeof err));
    d = good(); d.price_tier = CE_CARD_UNSET; CHECK(!ce_card_validate(&d, err, sizeof err)); CHECK(strstr(err, "price_tier") != NULL);
    d = good(); d.price_tier = 15;          CHECK(!ce_card_validate(&d, err, sizeof err));
    d = good(); d.price_tier = 14;          CHECK(ce_card_validate(&d, err, sizeof err));
    d = good(); d.weight = CE_CARD_UNSET;   CHECK(!ce_card_validate(&d, err, sizeof err)); CHECK(strstr(err, "weight") != NULL);
    d = good(); d.weight = 256;             CHECK(!ce_card_validate(&d, err, sizeof err));
    d = good(); d.weight = 0;               CHECK(ce_card_validate(&d, err, sizeof err));
    d = good(); d.dmode = 13;               CHECK(!ce_card_validate(&d, err, sizeof err)); CHECK(strstr(err, "dmode") != NULL);
    d = good(); d.repeatable = 2;           CHECK(!ce_card_validate(&d, err, sizeof err));
    d = good(); d.deck_visible = 2;         CHECK(!ce_card_validate(&d, err, sizeof err));
    d = good(); d.initial_unlocked = 2;     CHECK(!ce_card_validate(&d, err, sizeof err));
    d = good(); d.hud_show = 2;             CHECK(!ce_card_validate(&d, err, sizeof err));
    d = good(); d.category = 4;             CHECK(!ce_card_validate(&d, err, sizeof err)); CHECK(strstr(err, "category") != NULL);
    d = good(); d.f08 = 2;                  CHECK(!ce_card_validate(&d, err, sizeof err));
    d = good(); d.sprite_large = CE_CARD_UNSET; CHECK(!ce_card_validate(&d, err, sizeof err)); CHECK(strstr(err, "sprite_large") != NULL);
    d = good(); d.sprite_small = CE_CARD_UNSET; CHECK(!ce_card_validate(&d, err, sizeof err)); CHECK(strstr(err, "sprite_small") != NULL);
}

static void test_batch(void)
{
    ce_card_batch_t batch;
    char err[128];
    ce_card_batch_init(&batch);
    CHECK(batch.count == 0);

    ce_card_def_t a = good();
    CHECK(ce_card_batch_add(&batch, &a, err, sizeof err));
    CHECK(batch.count == 1 && batch.defs[0].id == 58);
    CHECK(!ce_card_batch_add(&batch, &a, err, sizeof err));
    CHECK(batch.count == 1 && strstr(err, "duplicate") != NULL);

    ce_card_def_t invalid = good(); invalid.id = 57;
    CHECK(!ce_card_batch_add(&batch, &invalid, err, sizeof err));
    CHECK(batch.count == 1);

    ce_card_def_t b = good(); b.id = 59;
    CHECK(ce_card_batch_add(&batch, &b, err, sizeof err));
    CHECK(batch.count == 2 && batch.defs[1].id == 59);
}

static void test_encode_row(void)
{
    ce_card_def_t d = good();
    d.f08 = 1; d.category = 3; d.dmode = 7; d.repeatable = 1; d.deck_visible = 0; d.initial_unlocked = 1; d.hud_show = 0;
    uint8_t row[CE_ROW_BYTES];
    memset(row, 0xee, sizeof row);
    ce_card_encode_row(&d, 0x12345678u, row);
    CHECK(rd32(row + 0x00) == 0x12345678u);   /* internal_name 指针 */
    CHECK(rd32(row + 0x04) == 58);            /* id */
    CHECK(rd32(row + 0x08) == 1);             /* f08 */
    CHECK(rd32(row + 0x0c) == 3);             /* category */
    CHECK(rd32(row + 0x10) == 5);             /* price_tier */
    CHECK(rd32(row + 0x14) == 2);             /* weight */
    CHECK(rd32(row + 0x18) == 7);             /* dmode */
    CHECK(rd32(row + 0x1c) == 1);             /* repeatable */
    CHECK(rd32(row + 0x20) == 0);             /* deck_visible */
    CHECK(rd32(row + 0x24) == 1);             /* initial_unlocked */
    CHECK(rd32(row + 0x28) == 0);             /* hud_show */
    CHECK(rd32(row + 0x2c) == 116);           /* sprite_large */
    CHECK(rd32(row + 0x30) == 117);           /* sprite_small */
}

/* 小表：行 = (id, weight, dmode) */
static void mkrow(uint8_t *t, unsigned r, uint32_t id, uint32_t weight, uint32_t dmode)
{
    uint8_t *p = t + r * CE_ROW_BYTES;
    memset(p, 0, CE_ROW_BYTES);
    wr32(p + 0x04, id); wr32(p + 0x14, weight); wr32(p + 0x18, dmode);
}

static void test_capacity(void)
{
    char err[160];
    uint8_t t[8 * CE_ROW_BYTES];
    unsigned pool, guaranteed;

    /* 普通：w2 → 7 份；w0 → 保证卡；dmode 3 → 本关必出；56/57 行忽略（哪怕 weight 0）；w6 不进池 */
    mkrow(t, 0, 0, 2, 0);
    mkrow(t, 1, 1, 0, 0);
    mkrow(t, 2, 2, 4, 3);
    mkrow(t, 3, 56, 0, 0);
    mkrow(t, 4, 57, 0, 0);
    mkrow(t, 5, 58, 6, 0);
    CHECK(ce_shop_capacity_check(t, 6, &pool, &guaranteed, err, sizeof err));
    CHECK(pool == 7 + 9);                     /* (2+5) + (4+5) */
    CHECK(guaranteed == 1 + 1 + CE_SHOP_RANDOM_MAX);   /* w0 一张 + dmode 1-5 一张 + 随机最多 6 */

    /* 池超限：边界按 CE_SHOP_POOL_SLOTS 表达，池子搬进 codecave 改大之后不会朽。
     * 64 张卡、每张 (POOL/64 - 5) 权重 ⇒ 份数正好等于上限；第 65 张就过不去。 */
    #define FIT_N 64u
    #define FIT_W (CE_SHOP_POOL_SLOTS / FIT_N - 5u)
    static uint8_t big[(FIT_N + 1) * CE_ROW_BYTES];
    for (unsigned i = 0; i < FIT_N + 1; ++i) mkrow(big, i, 58 + i, FIT_W, 0);   /* 避开 56/57 */
    CHECK(ce_shop_capacity_check(big, FIT_N, &pool, &guaranteed, err, sizeof err));
    CHECK(pool == CE_SHOP_POOL_SLOTS);
    CHECK(!ce_shop_capacity_check(big, FIT_N + 1, &pool, &guaranteed, err, sizeof err));
    CHECK(strstr(err, "pool") != NULL);
    #undef FIT_W
    #undef FIT_N

    /* 保证卡超限：52 张 weight 0 → 52 + 6 = 58 > 57 */
    for (unsigned i = 0; i < 52; ++i) mkrow(big, i, 58 + i, 0, 0);
    CHECK(!ce_shop_capacity_check(big, 52, &pool, &guaranteed, err, sizeof err));
    CHECK(strstr(err, "guaranteed") != NULL);
    for (unsigned i = 0; i < 51; ++i) mkrow(big, i, 58 + i, 0, 0);
    CHECK(ce_shop_capacity_check(big, 51, &pool, &guaranteed, err, sizeof err));
}

static void test_weight_override(void)
{
    static uint8_t t[255 * CE_ROW_BYTES];
    memset(t, 0, sizeof t);
    #define W(id) (*(uint32_t *)(t + (id) * CE_ROW_BYTES + 0x14))
    W(1) = 2; W(2) = 0; W(3) = 6; W(55) = 4; W(56) = 2; W(57) = 2; W(58) = 2; W(59) = 0;
    uint32_t ids[2] = { 58, 59 };
    CHECK(ce_weight_override(t, 255, 6, 20, ids, 2) == 3);    /* 1、55、58 */
    CHECK(W(1) == 6 && W(55) == 6);
    CHECK(W(2) == 0 && W(3) == 6);                                     /* 保底 / 已排除 不动 */
    CHECK(W(56) == 2 && W(57) == 2);                                   /* 哨兵不动 */
    CHECK(W(58) == 20 && W(59) == 0);                                  /* 新卡改，weight 0 的新卡不动 */
    CHECK(ce_weight_override(t, 255, -1, -1, ids, 2) == 0);   /* -1 = 不动 */
    #undef W
}

/* dev 权重覆盖不许把商店随机池顶爆。2026-09-08 实跑事故：new_weight 20 × 23 张新卡 = 575 > 560，
 * 门里 FAIL → ce_menu_setup 不跑 → 卡组编成里读到没建出来的卡对象，按 Z 空指针崩溃（AUDIT §AG）。
 * ce_weight_override_fit 把 new_weight 往下压到刚好能过容量检查；压到 1 还不行就原样返回，让门照常 FAIL。 */
static void test_weight_override_fit(void)
{
    static uint8_t t[255 * CE_ROW_BYTES];
    static uint32_t ids[64];
    char err[160];
    unsigned changed = 0, pool = 0, guaranteed = 0;
    #define W(id) (*(uint32_t *)(t + (id) * CE_ROW_BYTES + 0x14))
    #define ID(id) (*(uint32_t *)(t + (id) * CE_ROW_BYTES + 0x04))
    /* 真实形状：装载器给 NULL/BACK 与所有没用的行写 weight 6（退出随机池），
     * 原生 1..55 与 23 张新卡进池。不这么铺的话，weight 0 的空行会被算成「必出卡」而先撞另一条上限。 */
    #define SETUP_N(n, w) do { memset(t, 0, sizeof t); \
        for (unsigned q = 0; q < 255; ++q) { ID(q) = q; W(q) = CE_SHOP_NEVER_WEIGHT; } \
        for (unsigned q = 1; q < 56; ++q) W(q) = 2; \
        for (unsigned q = 0; q < (n); ++q) { ids[q] = 58 + q; W(58 + q) = (w); } } while (0)
    #define SETUP() SETUP_N(23u, 2u)

    /* 顶爆时往下压。上限已随 codecave 提到 CE_SHOP_POOL_SLOTS，所以用能撑爆它的组合：
     * CLAMP_N 张新卡、权重 255（份数 CLAMP_N × 260）⇒ 压到 POOL/CLAMP_N − 5 为止。
     * 2026-09-08 的真实事故是 23 张 × 25 = 575 > 560 —— 那条现在压根到不了上限（见 §AH）。 */
    #define CLAMP_N 32u
    #define CLAMP_WANT (int)(CE_SHOP_POOL_SLOTS / CLAMP_N - 5u)
    SETUP_N(CLAMP_N, 255);
    CHECK(ce_weight_override_fit(t, 255, 6, 255, ids, CLAMP_N, &changed) == CLAMP_WANT);
    CHECK((int)W(58) == CLAMP_WANT && (int)W(58 + CLAMP_N - 1) == CLAMP_WANT);
    CHECK(ce_shop_capacity_check(t, 255, &pool, &guaranteed, err, sizeof err));
    CHECK(pool <= CE_SHOP_POOL_SLOTS);
    CHECK(changed > 0);

    /* 本来就放得下：用原值 */
    SETUP();
    CHECK(ce_weight_override_fit(t, 255, 6, 5, ids, 23, &changed) == 5);
    CHECK(W(58) == 5);

    /* -1 = 不覆盖：原样返回，一行不改 */
    SETUP();
    CHECK(ce_weight_override_fit(t, 255, -1, -1, ids, 23, &changed) == -1);
    CHECK(changed == 0 && W(58) == 2);

    /* 原生自己就撑爆了（不覆盖原生）：压 new_weight 也救不回来 → 原样返回，交给门 FAIL */
    SETUP();
    for (unsigned id = 1; id < 56; ++id) W(id) = 200;
    CHECK(ce_weight_override_fit(t, 255, -1, 20, ids, 23, &changed) == 20);
    CHECK(!ce_shop_capacity_check(t, 255, &pool, &guaranteed, err, sizeof err));
    #undef SETUP
    #undef SETUP_N
    #undef CLAMP_WANT
    #undef CLAMP_N
    #undef ID
    #undef W
}

static void test_build_order(void)
{
    /* 原生缩影（含两张掉队的 c2：块首 BLANK、块尾 MAGATAMA2）：新卡 100(被动) 101(子机) 102(主动) 103(类别 9 不存在) 104(子机) */
    const uint32_t retail[]     = { 0, 1, 2, 8, 9, 21, 22, 23, 41, 42, 55 };
    const uint32_t retail_cat[] = { 2, 3, 3, 1, 1, 2,  2,  2,  0,  0,  2 };
    const uint32_t new_ids[]    = { 100, 101, 102, 103, 104 };
    const uint32_t new_cat[]    = { 2,   1,   0,   9,   1 };
    uint32_t out[20];
    unsigned vis = ce_build_order(out, 20, retail, retail_cat, 11, new_ids, new_cat, 5, 56, 57);
    const uint32_t want[20] = { 0, 1, 2, 8, 9, 101, 104, 21, 22, 23, 100, 41, 42, 102, 55, 103, 56, 57, 57, 57 };
    CHECK(vis == 16);
    for (unsigned i = 0; i < 20; ++i) CHECK(out[i] == want[i]);
    /* 没有新卡：原生原序 + NULL + BACK */
    vis = ce_build_order(out, 20, retail, retail_cat, 11, NULL, NULL, 0, 56, 57);
    CHECK(vis == 11 && out[11] == 56 && out[12] == 57);
    /* 放不下：返回 0 */
    CHECK(ce_build_order(out, 16, retail, retail_cat, 11, new_ids, new_cat, 5, 56, 57) == 0);
    /* 同长两段：取第一段 */
    const uint32_t r2[] = { 5, 5, 9, 5, 5 }, c2[] = { 2, 2, 0, 2, 2 }, n2[] = { 100 }, nc2[] = { 2 };
    vis = ce_build_order(out, 8, r2, c2, 5, n2, nc2, 1, 56, 57);
    CHECK(vis == 6 && out[2] == 100 && out[3] == 9);
}

int main(void)
{
    test_build_order();
    test_weight_override();
    test_weight_override_fit();
    test_defaults();
    test_text_ok();
    test_validate();
    test_batch();
    test_encode_row();
    test_capacity();
    printf("test_cards_def: %d passed, %d failed\n", s_pass, s_fail);
    return s_fail ? 1 : 0;
}
