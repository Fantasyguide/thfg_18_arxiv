/* cards_def.h —— 一张新卡的定义：校验、编成 zTableCardData 表行、商店容量。
 *
 * 纯逻辑，不含 windows.h / thcrap / jansson，所以能在 Linux 主机上单测（make test-host）。
 * JSON → ce_card_def_t 的胶水在 cards.c；字段语义与取值范围的正式说明在 ../DATA.md §2。
 *
 * 表行布局（TH18 v1.00a，stride 0x34 = 13 × dword；engine/card/th18/01-object-model.md §6）：
 *   +0x00 internal_name(char*)  +0x04 id            +0x08 f08(未知)      +0x0c category
 *   +0x10 price_tier            +0x14 weight        +0x18 dmode          +0x1c repeatable
 *   +0x20 deck_visible          +0x24 initial_unlocked  +0x28 hud_show   +0x2c sprite_large  +0x30 sprite_small
 */
#pragma once
#include <stdint.h>

#define CE_ROW_BYTES        0x34
#define CE_CARD_ID_MIN      58        /* 56 = NULL 空槽、57 = BACK 卡背，零售哨兵 */
#define CE_CARD_ID_MAX      254
#define CE_CARD_MAX_NEW     71        /* 图鉴条目 56+N 进两处 cmp r,imm8 → ≤ 127（AUDIT §M）*/
#define CE_CARD_TEXT_LINE   0x40      /* 文案一行 0x40 字节，含 NUL → 内容 ≤ 63 */
#define CE_CARD_DESC_LINES  6
#define CE_CARD_UNSET       0xffffffffu   /* 必填字段的「没给」*/

/* 商店（AUDIT §N2、边界 #34） */
#define CE_SHOP_POOL_SLOTS  560       /* pick_weighted_random_offer 的 0x8c0 栈缓冲 / 4 */
#define CE_SHOP_OFFER_SLOTS 57        /* AbilityShop__initialize 的 [ebp-0xe4] */
#define CE_SHOP_RANDOM_MAX  6         /* 随机抽 3 张，带招财猫 6 张 */
#define CE_SHOP_NEVER_WEIGHT 6        /* +0x14 == 6：永不进随机池；写进 NULL/BACK 行排除幻影 */

typedef struct {
    uint32_t id;
    char     name[CE_CARD_TEXT_LINE];
    char     desc[CE_CARD_DESC_LINES][CE_CARD_TEXT_LINE];
    unsigned ndesc;
    char     internal_name[32];
    uint32_t f08, category, price_tier, weight, dmode, repeatable,
             deck_visible, initial_unlocked, hud_show, sprite_large, sprite_small;
} ce_card_def_t;

typedef struct {
    ce_card_def_t defs[CE_CARD_MAX_NEW];
    uint8_t seen[CE_CARD_ID_MAX + 1];
    unsigned count;
} ce_card_batch_t;

void ce_card_batch_init(ce_card_batch_t *batch);
int ce_card_batch_add(ce_card_batch_t *batch, const ce_card_def_t *def, char *err, unsigned cap);

/* 默认值；internal_name = id 的十进制；必填字段置 CE_CARD_UNSET */
void ce_card_def_defaults(ce_card_def_t *d, uint32_t id);

/* 文案一行合法：非空、≤ 63 字节、不含 '%'（名字会被当 printf 格式串，text.c 头注）*/
int  ce_card_text_ok(const char *s);

/* 单张校验（不看全局约束）。返回 1；0 时 err 里是「字段: 原因」*/
int  ce_card_validate(const ce_card_def_t *d, char *err, unsigned cap);

/* 编成 0x34 字节表行；name_ptr 是写进 +0x00 的指针值（调用方负责它的生存期）*/
void ce_card_encode_row(const ce_card_def_t *d, uint32_t name_ptr, uint8_t row[CE_ROW_BYTES]);

/* 商店容量：扫整张表（零售 + 新卡），忽略 id 56/57 的行。
 *   pool       = Σ_{weight∉{0,6}} (weight + 5)              ≤ CE_SHOP_POOL_SLOTS
 *   guaranteed = count(weight==0) + count(dmode∈1..5) + 6   ≤ CE_SHOP_OFFER_SLOTS
 * 返回 1；0 时 err 里说明哪条超了。pool / guaranteed 可为 NULL。*/
/* 开发用权重覆盖（cards_dev.js 的 retail_weight / new_weight）：零售行 1..55 里 weight ∉ {0,6} 的改成 retail_weight，
 * new_ids 里的行改成 new_weight；传 -1 表示不动。返回改了几行。6 = 永不进随机池，0 = 保底资源卡（不动）。*/
unsigned ce_weight_override(uint8_t *table, unsigned nrows, int retail_weight, int new_weight,
                            const uint32_t *new_ids, unsigned n_new);

int  ce_shop_capacity_check(const uint8_t *table, unsigned nrows, unsigned *pool, unsigned *guaranteed,
                            char *err, unsigned cap);

/* 显示顺序表（图鉴 / 编成共用）：零售 56 项原序不动，**新卡按类别插进对应区段末尾**——零售表本身按类别分块
 * （0 主动 / 1 子机装备 / 2 被动能力 / 3 资源），但有两张掉队的：BLANK(2) 在最前、MAGATAMA2(2) 在最后。
 * 所以「区段」取该类别**最长的一段连续区**（被动 = ITEM_CATCH…MUKADE 那 22 张，而不是孤零零的 BLANK 或 MAGATAMA2），
 * 每个新 id 排在那一段的最后一张之后，同类别新卡之间保持注册顺序；类别在零售表里不存在的新卡排在所有零售卡之后。
 * 然后 null_row（编成空槽），余下填 back_row。返回可见条目数（零售 + 新卡）= 图鉴条目数；nretail + nnew + 1 > cap 时返回 0。*/
unsigned ce_build_order(uint32_t *out, unsigned cap,
                        const uint32_t *retail, const uint32_t *retail_cat, unsigned nretail,
                        const uint32_t *new_ids, const uint32_t *new_cat, unsigned nnew,
                        uint32_t null_row, uint32_t back_row);
