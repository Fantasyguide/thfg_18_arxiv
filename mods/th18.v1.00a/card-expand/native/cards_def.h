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
#define CE_CARD_ID_MIN      58        /* 56 = NULL 空槽、57 = BACK 卡背，原生哨兵 */
#define CE_CARD_ID_MAX      254
#define CE_CARD_MAX_NEW     197       /* = id 空间大小（58..254，= `CE_CARD_ID_MAX`−`CE_CARD_ID_MIN`+1）。
                                       * 另有两条更松的界：表行 255、图鉴条目 255 − 原生 56 = 199
                                       * ——都不是约束。旧值 71 来自图鉴条目数那两处 `cmp r,imm8`
                                       * 的符号界（≤127）；那两处 2026-09-19 已原地改成无符号字节比较
                                       * （`sites.py` 的 `MENU_COUNT_BCMP`）⇒ 界抬到 255。
                                       * 现实里**先撞的是资源墙**：abcard.anm entry 256（每卡 2 条）
                                       * ⇒ 还能进 46 张。见 CAPACITY.md 与 native/tools/capcheck.py。*/
/* ★ 作品系统卡（106..117）的 id 区间：**只用于显示分组** —— `menu.c` 建顺序表时把这一段按同一个显示类别插入，让它们在选卡界面连成一条（`ce_build_order` 按类别分段）。
 * ⚠ 别拿它去改 `cards.js` 的 `category`：那张表的 `category` 必须与行为一致
 *   （主动卡 = 0），SDK 的 `ce_sdk_contract_check` 会拦（reason 2），见 AUDIT §BE-EJ。*/
#define CE_SYS_CARD_LO      106
#define CE_SYS_CARD_HI      117

#define CE_CARD_TEXT_LINE   0x40      /* 文案一行 0x40 字节，含 NUL → 内容 ≤ 63 */
#define CE_CARD_DESC_LINES  6
#define CE_CARD_UNSET       0xffffffffu   /* 必填字段的「没给」*/

/* 商店（AUDIT §N2、边界 #34） */
#define CE_SHOP_POOL_SLOTS  8192      /* 池子已搬进 codecave（AUDIT §AH）。原生只有 560：
                                       * pick_weighted_random_offer 的 0x8c0 栈缓冲 / 4，且末尾紧贴栈金丝雀、填池无越界检查。
                                       * 必须与 sites_gen.h 的 CE_POOL_SLOTS 一致 —— cards.c 里有 _Static_assert 钉着。*/
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

/* 商店容量：扫整张表（原生 + 新卡），忽略 id 56/57 的行。
 *   pool       = Σ_{weight∉{0,6}} (weight + 5)              ≤ CE_SHOP_POOL_SLOTS
 *   guaranteed = count(weight==0) + count(dmode∈1..5) + 6   ≤ CE_SHOP_OFFER_SLOTS
 * 返回 1；0 时 err 里说明哪条超了。pool / guaranteed 可为 NULL。*/
/* 开发用权重覆盖（cards_dev.js 的 retail_weight / new_weight）：原生行 1..55 里 weight ∉ {0,6} 的改成 retail_weight，
 * new_ids 里的行改成 new_weight；传 -1 表示不动。返回改了几行。6 = 永不进随机池，0 = 保底资源卡（不动）。*/
unsigned ce_weight_override(uint8_t *table, unsigned nrows, int retail_weight, int new_weight,
                            const uint32_t *new_ids, unsigned n_new);

/* 同上，但**保证结果能过 `ce_shop_capacity_check`**：若覆盖后的随机池超过 `CE_SHOP_POOL_SLOTS`，
 * 把 new_weight 逐级往下压（下限 1 —— 0 会把卡变成必出资源卡，6 会把它踢出池，都不是「少出现一点」的意思），
 * 直到放得下为止。返回**实际生效的** new_weight（-1 = 没做覆盖；压不下去时返回传入值，容量问题交给门 FAIL）。
 * `*changed` = 最终那一遍改了多少行。
 * 由来：2026-09-08 实跑，dev 配置 new_weight 20 × 23 张新卡 = 575 > 560 让整个卡表装载被拒，
 * 而静态 binhack 早已把菜单结构撑到 255 项 —— 卡组编成于是读到没建出来的卡对象，按 Z 就崩（AUDIT §AG）。 */
int ce_weight_override_fit(uint8_t *table, unsigned nrows, int retail_weight, int new_weight,
                           const uint32_t *new_ids, unsigned n_new, unsigned *changed);

int  ce_shop_capacity_check(const uint8_t *table, unsigned nrows, unsigned *pool, unsigned *guaranteed,
                            char *err, unsigned cap);

/* 显示顺序表（图鉴 / 编成共用）：原生 56 项原序不动，**新卡按类别插进对应区段末尾**——原生表本身按类别分块
 * （0 主动 / 1 子机装备 / 2 被动能力 / 3 资源），但有两张掉队的：BLANK(2) 在最前、MAGATAMA2(2) 在最后。
 * 所以「区段」取该类别**最长的一段连续区**（被动 = ITEM_CATCH…MUKADE 那 22 张，而不是孤零零的 BLANK 或 MAGATAMA2），
 * 每个新 id 排在那一段的最后一张之后，同类别新卡之间保持注册顺序；类别在原生表里不存在的新卡排在所有原生卡之后。
 * 然后 null_row（编成空槽），余下填 back_row。返回可见条目数（原生 + 新卡）= 图鉴条目数；nretail + nnew + 1 > cap 时返回 0。*/
unsigned ce_build_order(uint32_t *out, unsigned cap,
                        const uint32_t *retail, const uint32_t *retail_cat, unsigned nretail,
                        const uint32_t *new_ids, const uint32_t *new_cat, unsigned nnew,
                        uint32_t null_row, uint32_t back_row);
