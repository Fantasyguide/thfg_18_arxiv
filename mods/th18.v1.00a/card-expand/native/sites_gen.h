/* 由 sites.py 生成，不要手改。与行数无关。*/
#pragma once
#include <stdint.h>
#define CE_ROW_SIZE    0x34
#define CE_ROW_COUNT   58
#define CE_NULL_ROW    56
#define CE_MAX_ROWS    255
#define CE_TABLE_RVA   0x0c53c0
#define CE_CAVE_NAME   "codecave:th18_card_table"
#define CE_JT_RVA      0x012dac
#define CE_JT_COUNT    57
#define CE_CASE56_RVA  0x011489
#define CE_JT_CAVE_NAME "codecave:th18_card_jumptable"
#define CE_ALLOC_CMP_RVA 0x011479
#define CE_ALLOC_JMP_RVA 0x011482
enum { CE_K_START = 0, CE_K_HIT = 1, CE_K_FALLBACK = 2, CE_K_END = 3 };
typedef struct { uint32_t rva; uint8_t len, prefix_len, prefix[3], kind; uint32_t field; } ce_site_t;
static const ce_site_t CE_SITES[] = {
    { 0x007d72, 5, 1, { 0xb8, 0, 0 }, CE_K_START, 0x4 },
    { 0x007d7f, 5, 1, { 0x3d, 0, 0 }, CE_K_END, 0x4 },
    { 0x007d86, 5, 1, { 0xb8, 0, 0 }, CE_K_FALLBACK, 0x0 },
    { 0x007d8f, 5, 1, { 0x05, 0, 0 }, CE_K_HIT, 0x0 },
    { 0x009321, 5, 1, { 0xb8, 0, 0 }, CE_K_START, 0x4 },
    { 0x00932f, 5, 1, { 0x3d, 0, 0 }, CE_K_END, 0x4 },
    { 0x009336, 7, 3, { 0xc7, 0x45, 0x0c }, CE_K_FALLBACK, 0x30 },
    { 0x009342, 5, 1, { 0x05, 0, 0 }, CE_K_HIT, 0x30 },
    { 0x014412, 5, 1, { 0xb8, 0, 0 }, CE_K_START, 0x4 },
    { 0x014428, 5, 1, { 0x3d, 0, 0 }, CE_K_END, 0x4 },
    { 0x01442f, 5, 1, { 0xb8, 0, 0 }, CE_K_FALLBACK, 0x0 },
    { 0x014439, 5, 1, { 0x05, 0, 0 }, CE_K_HIT, 0x0 },
    { 0x014457, 5, 1, { 0xb8, 0, 0 }, CE_K_START, 0x4 },
    { 0x014468, 5, 1, { 0x3d, 0, 0 }, CE_K_END, 0x4 },
    { 0x01446f, 5, 1, { 0xb8, 0, 0 }, CE_K_FALLBACK, 0x0 },
    { 0x014483, 5, 1, { 0x05, 0, 0 }, CE_K_HIT, 0x0 },
    { 0x01449d, 5, 1, { 0x3d, 0, 0 }, CE_K_END, 0x4 },
    { 0x0144a4, 5, 1, { 0xb8, 0, 0 }, CE_K_FALLBACK, 0x0 },
    { 0x0144ae, 5, 1, { 0x05, 0, 0 }, CE_K_HIT, 0x0 },
    { 0x0144c9, 5, 1, { 0xb8, 0, 0 }, CE_K_START, 0x4 },
    { 0x0144d9, 5, 1, { 0x3d, 0, 0 }, CE_K_END, 0x4 },
    { 0x0144e0, 5, 1, { 0xb8, 0, 0 }, CE_K_FALLBACK, 0x0 },
    { 0x0144ea, 5, 1, { 0x05, 0, 0 }, CE_K_HIT, 0x0 },
    { 0x0149b4, 5, 1, { 0xb8, 0, 0 }, CE_K_START, 0x4 },
    { 0x0149c8, 5, 1, { 0x3d, 0, 0 }, CE_K_END, 0x4 },
    { 0x0149cf, 5, 1, { 0xb8, 0, 0 }, CE_K_FALLBACK, 0x20 },
    { 0x0149d9, 5, 1, { 0x05, 0, 0 }, CE_K_HIT, 0x20 },
    { 0x0149fc, 5, 1, { 0xb8, 0, 0 }, CE_K_START, 0x4 },
    { 0x014a09, 5, 1, { 0x3d, 0, 0 }, CE_K_END, 0x4 },
    { 0x014a10, 5, 1, { 0xb8, 0, 0 }, CE_K_FALLBACK, 0x0 },
    { 0x014a1a, 5, 1, { 0x05, 0, 0 }, CE_K_HIT, 0x0 },
    { 0x014a62, 5, 1, { 0xb8, 0, 0 }, CE_K_START, 0x4 },
    { 0x014a6f, 5, 1, { 0x3d, 0, 0 }, CE_K_END, 0x4 },
    { 0x014a76, 5, 1, { 0xb8, 0, 0 }, CE_K_FALLBACK, 0x0 },
    { 0x014a80, 5, 1, { 0x05, 0, 0 }, CE_K_HIT, 0x0 },
    { 0x01612f, 5, 1, { 0xbe, 0, 0 }, CE_K_START, 0x0 },
    { 0x01616d, 6, 2, { 0x81, 0xfe, 0 }, CE_K_END, 0x0 },
    { 0x016175, 5, 1, { 0xbb, 0, 0 }, CE_K_FALLBACK, 0x4 },
    { 0x01617f, 6, 2, { 0x81, 0xc3, 0 }, CE_K_HIT, 0x4 },
    { 0x016960, 5, 1, { 0xb8, 0, 0 }, CE_K_START, 0x4 },
    { 0x01696d, 5, 1, { 0x3d, 0, 0 }, CE_K_END, 0x4 },
    { 0x016974, 5, 1, { 0xb8, 0, 0 }, CE_K_FALLBACK, 0xc },
    { 0x01697e, 6, 2, { 0x8d, 0x80, 0 }, CE_K_HIT, 0xc },
    { 0x016fad, 5, 1, { 0xb8, 0, 0 }, CE_K_START, 0x4 },
    { 0x016fba, 5, 1, { 0x3d, 0, 0 }, CE_K_END, 0x4 },
    { 0x016fc1, 5, 1, { 0xb8, 0, 0 }, CE_K_FALLBACK, 0x1c },
    { 0x016fcb, 6, 2, { 0x8d, 0x80, 0 }, CE_K_HIT, 0x1c },
    { 0x016fdc, 5, 1, { 0xb8, 0, 0 }, CE_K_START, 0x4 },
    { 0x016fe9, 5, 1, { 0x3d, 0, 0 }, CE_K_END, 0x4 },
    { 0x016ff0, 5, 1, { 0xb9, 0, 0 }, CE_K_FALLBACK, 0x0 },
    { 0x016ffa, 6, 2, { 0x81, 0xc1, 0 }, CE_K_HIT, 0x0 },
    { 0x017010, 5, 1, { 0xb8, 0, 0 }, CE_K_START, 0x4 },
    { 0x01701d, 5, 1, { 0x3d, 0, 0 }, CE_K_END, 0x4 },
    { 0x017024, 5, 1, { 0xb8, 0, 0 }, CE_K_FALLBACK, 0x10 },
    { 0x01702e, 6, 2, { 0x8d, 0x80, 0 }, CE_K_HIT, 0x10 },
    { 0x017044, 5, 1, { 0xb8, 0, 0 }, CE_K_START, 0x4 },
    { 0x017058, 5, 1, { 0x3d, 0, 0 }, CE_K_END, 0x4 },
    { 0x01705f, 5, 1, { 0xb8, 0, 0 }, CE_K_FALLBACK, 0x10 },
    { 0x017069, 6, 2, { 0x8d, 0x80, 0 }, CE_K_HIT, 0x10 },
    { 0x01707c, 5, 1, { 0xb8, 0, 0 }, CE_K_START, 0x4 },
    { 0x017089, 5, 1, { 0x3d, 0, 0 }, CE_K_END, 0x4 },
    { 0x017090, 5, 1, { 0xb8, 0, 0 }, CE_K_FALLBACK, 0x14 },
    { 0x01709a, 6, 2, { 0x8d, 0x80, 0 }, CE_K_HIT, 0x14 },
    { 0x0170ab, 5, 1, { 0xb8, 0, 0 }, CE_K_START, 0x4 },
    { 0x0170b8, 5, 1, { 0x3d, 0, 0 }, CE_K_END, 0x4 },
    { 0x0170bf, 5, 1, { 0xb8, 0, 0 }, CE_K_FALLBACK, 0x14 },
    { 0x0170c9, 6, 2, { 0x8d, 0x80, 0 }, CE_K_HIT, 0x14 },
    { 0x01745d, 5, 1, { 0xb8, 0, 0 }, CE_K_START, 0x4 },
    { 0x01746a, 5, 1, { 0x3d, 0, 0 }, CE_K_END, 0x4 },
    { 0x017471, 5, 1, { 0xb8, 0, 0 }, CE_K_FALLBACK, 0x1c },
    { 0x01747b, 6, 2, { 0x8d, 0x80, 0 }, CE_K_HIT, 0x1c },
    { 0x01748c, 5, 1, { 0xb8, 0, 0 }, CE_K_START, 0x4 },
    { 0x017499, 5, 1, { 0x3d, 0, 0 }, CE_K_END, 0x4 },
    { 0x0174a0, 5, 1, { 0xb9, 0, 0 }, CE_K_FALLBACK, 0x0 },
    { 0x0174aa, 6, 2, { 0x81, 0xc1, 0 }, CE_K_HIT, 0x0 },
    { 0x0174bc, 5, 1, { 0xb8, 0, 0 }, CE_K_START, 0x4 },
    { 0x0174c9, 5, 1, { 0x3d, 0, 0 }, CE_K_END, 0x4 },
    { 0x0174d0, 5, 1, { 0xb8, 0, 0 }, CE_K_FALLBACK, 0x14 },
    { 0x0174da, 6, 2, { 0x8d, 0x80, 0 }, CE_K_HIT, 0x14 },
    { 0x0174e7, 5, 1, { 0xb8, 0, 0 }, CE_K_START, 0x4 },
    { 0x0174f8, 5, 1, { 0x3d, 0, 0 }, CE_K_END, 0x4 },
    { 0x0174ff, 5, 1, { 0xb9, 0, 0 }, CE_K_FALLBACK, 0x0 },
    { 0x017509, 6, 2, { 0x81, 0xc1, 0 }, CE_K_HIT, 0x0 },
    { 0x01754d, 5, 1, { 0xb8, 0, 0 }, CE_K_START, 0x4 },
    { 0x01755a, 5, 1, { 0x3d, 0, 0 }, CE_K_END, 0x4 },
    { 0x017561, 5, 1, { 0xb8, 0, 0 }, CE_K_FALLBACK, 0x1c },
    { 0x01756b, 6, 2, { 0x8d, 0x80, 0 }, CE_K_HIT, 0x1c },
    { 0x017578, 5, 1, { 0xb8, 0, 0 }, CE_K_START, 0x4 },
    { 0x017588, 5, 1, { 0x3d, 0, 0 }, CE_K_END, 0x4 },
    { 0x01758f, 5, 1, { 0xb9, 0, 0 }, CE_K_FALLBACK, 0x0 },
    { 0x017599, 6, 2, { 0x81, 0xc1, 0 }, CE_K_HIT, 0x0 },
    { 0x0175ab, 5, 1, { 0xb8, 0, 0 }, CE_K_START, 0x4 },
    { 0x0175b8, 5, 1, { 0x3d, 0, 0 }, CE_K_END, 0x4 },
    { 0x0175bf, 5, 1, { 0xb9, 0, 0 }, CE_K_FALLBACK, 0x0 },
    { 0x0175c9, 6, 2, { 0x81, 0xc1, 0 }, CE_K_HIT, 0x0 },
    { 0x018e0e, 5, 1, { 0xba, 0, 0 }, CE_K_START, 0x4 },
    { 0x0636b2, 5, 1, { 0xb8, 0, 0 }, CE_K_START, 0x4 },
    { 0x0636bf, 5, 1, { 0x3d, 0, 0 }, CE_K_END, 0x4 },
    { 0x0636c6, 5, 1, { 0xb8, 0, 0 }, CE_K_FALLBACK, 0x24 },
    { 0x0636d0, 5, 1, { 0x05, 0, 0 }, CE_K_HIT, 0x24 },
};
#define CE_NSITES (sizeof(CE_SITES)/sizeof(CE_SITES[0]))

typedef struct { uint32_t rva; uint8_t len; uint8_t kind; } ce_grow_t;
enum { CE_G_SIZE = 0, CE_G_OWNED_LEA = 1, CE_G_STOSD = 2, CE_G_OWNED_DISP = 3, CE_G_SHOP_START = 4, CE_G_SHOP_END = 5 };
#define CE_MGR_SIZE    0xd70
#define CE_OWNED_NEW   0xd70
static const ce_grow_t CE_GROW[] = {
    { 0x0082d6, 5, CE_G_SIZE }, { 0x0082ec, 5, CE_G_SIZE }, { 0x00860a, 5, CE_G_SIZE },
    { 0x007eb0, 6, CE_G_OWNED_LEA }, { 0x007eb6, 5, CE_G_STOSD }, { 0x012d42, 11, CE_G_OWNED_DISP },
    { 0x016f8f, 5, CE_G_SHOP_START }, { 0x01744a, 5, CE_G_SHOP_START }, { 0x017535, 5, CE_G_SHOP_START },
    { 0x01716b, 6, CE_G_SHOP_END }, { 0x017527, 6, CE_G_SHOP_END }, { 0x0175e7, 6, CE_G_SHOP_END },
};
#define CE_NGROW (sizeof(CE_GROW)/sizeof(CE_GROW[0]))

#define CE_UNLOCKED_OFF   0x5f588
#define CE_UNLOCKED_CAVE_NAME "codecave:th18_card_unlocked"
#define CE_RETAIL_UNLOCKED 57
#define CE_SAVEDIR_RVA    0x168c61   /* %%APPDATA%%\ShanghaiAlice\th18\ ，游戏 chdir 用的缓冲，尾带反斜杠 */
#define CE_SCOREFILE_PTR_RVA 0x0cf41c
#define CE_BP_UNLOCK_WRITE_RVA 0x018e04
#define CE_BP_SAVE_LOADED_RVA  0x06398a
#define CE_BP_UNLOCK_ALL_RVA   0x0648fe
#define CE_ABILITY_TXT_PTR_RVA 0x0cf29c
#define CE_TEXT_ENTRY     0x1c0
#define CE_TEXT_ENTRIES   57      /* 原生 zAbilityText 装 57 张；第 57 张的位置已是尾部字段 */
#define CE_BP_TEXT_NAME_RVA   0x016694
#define CE_BP_TEXT_DESC_RVA   0x016779
#define CE_BP_TEXT_NOTIFY_RVA 0x01926a
#define CE_BP_CARD_BIND_RVA 0x012cec
#define CE_BP_ITEM_SCORE_RVA 0x046cf6
#define CE_BP_ITEM_MONEY_RVA 0x046d28
#define CE_BP_BOMB_SPENT_RVA 0x0203bc
#define CE_BP_ENEMY_DROP_RVA 0x030510
#define CE_BP_ITEM_FRAME_RVA 0x046ee3
#define CE_BP_ITEM_COLLECT_RVA 0x04625f
#define CE_BP_ITEM_COMMIT_RVA 0x04658a
#define CE_BP_SHOT_DAMAGE_RVA 0x05f2a2
#define CE_BP_BOMB_GATE_RVA 0x05c048
#define CE_BP_SHOT_SET_INDEX_RVA 0x05ebfb
#define CE_BP_OPTION_SHOT_GATE_RVA 0x00a9c3
#define CE_BP_ENEMY_DEATH_RVA 0x030240
#define CE_BP_ENEMY_SHOT_SLOT_RVA 0x033ffa
#define CE_BP_ITEM_SCORE_BELOW_RVA 0x046caa
#define CE_BP_SHOP_BOUGHT_RVA 0x0183ea
#define CE_BP_SHOP_REOPEN_RVA 0x043b05
#define CE_TEST_DECK_SAVE_OFF 0x5f608   /* reset_cards：byte [eax+esi+0x5f608] 初始卡组一格 */
#define CE_ORDER_RVA      0x0b3600
#define CE_ORDER_COUNT    57
#define CE_ORDER_CAVE_NAME "codecave:th18_card_order"
typedef struct { uint32_t rva; uint8_t pre_len, is_end; } ce_order_t;
static const ce_order_t CE_ORDER[] = {
    { 0x014401, 3, 0 },
    { 0x0145f8, 3, 0 },
    { 0x014639, 3, 0 },
    { 0x01499f, 1, 0 },
    { 0x014b54, 1, 1 },
    { 0x015681, 3, 0 },
    { 0x0156b6, 3, 0 },
};
#define CE_NORDER (sizeof(CE_ORDER)/sizeof(CE_ORDER[0]))
#define CE_MENU_SIZE      0x13fc
#define CE_CARD_IDS_NEW   0x13fc
#define CE_MENU_COUNT_RETAIL 56
#define CE_MENU_COUNT_MAX 127   /* 两处 cmp r,imm8 */
typedef struct { uint32_t rva; uint8_t imm_off, width; } ce_count_site_t;
static const ce_count_site_t CE_MENU_COUNT[] = {
    { 0x0137bb, 6, 4 },
    { 0x014394, 6, 4 },
    { 0x01439e, 6, 4 },
    { 0x0145e2, 2, 1 },
    { 0x01570d, 1, 4 },
    { 0x0157cb, 2, 1 },
    { 0x015817, 1, 4 },
};
#define CE_NMENU_COUNT (sizeof(CE_MENU_COUNT)/sizeof(CE_MENU_COUNT[0]))
typedef struct { uint32_t rva; uint8_t len; uint8_t want[7]; } ce_menu_t;
static const ce_menu_t CE_MENU[] = {
    { 0x013817, 5, { 0x68, 0xf8, 0x17, 0x00, 0x00, 0, 0 } },
    { 0x013831, 5, { 0x68, 0xf8, 0x17, 0x00, 0x00, 0, 0 } },
    { 0x013abb, 5, { 0x68, 0xf8, 0x17, 0x00, 0x00, 0, 0 } },
    { 0x01495c, 5, { 0xbf, 0xff, 0x00, 0x00, 0x00, 0, 0 } },
    { 0x0145d2, 6, { 0x89, 0x86, 0x10, 0x0c, 0x00, 0x00, 0 } },
    { 0x014b3f, 6, { 0x89, 0xb0, 0x10, 0x08, 0x00, 0x00, 0 } },
    { 0x014b81, 6, { 0x8d, 0x87, 0xfc, 0x13, 0x00, 0x00, 0 } },
    { 0x014beb, 7, { 0xff, 0xb4, 0x87, 0xfc, 0x13, 0x00, 0x00 } },
    { 0x014e9f, 7, { 0x8b, 0x8c, 0x8f, 0xfc, 0x13, 0x00, 0x00 } },
    { 0x014eba, 7, { 0xff, 0xb4, 0x87, 0xfc, 0x13, 0x00, 0x00 } },
    { 0x015049, 6, { 0x8d, 0x87, 0xfc, 0x13, 0x00, 0x00, 0 } },
    { 0x015115, 7, { 0x8a, 0x8c, 0x8f, 0xfc, 0x13, 0x00, 0x00 } },
    { 0x015129, 7, { 0xff, 0xb4, 0x87, 0xfc, 0x13, 0x00, 0x00 } },
    { 0x01514a, 7, { 0xff, 0xb4, 0x87, 0xfc, 0x13, 0x00, 0x00 } },
    { 0x0151ef, 7, { 0xff, 0xb4, 0x87, 0xfc, 0x13, 0x00, 0x00 } },
    { 0x01520c, 7, { 0xff, 0xb4, 0x87, 0xfc, 0x13, 0x00, 0x00 } },
    { 0x0152b4, 7, { 0xff, 0xb4, 0x87, 0xfc, 0x13, 0x00, 0x00 } },
    { 0x0152d5, 7, { 0xff, 0xb4, 0x87, 0xfc, 0x13, 0x00, 0x00 } },
    { 0x015868, 7, { 0xff, 0xb4, 0x87, 0xfc, 0x13, 0x00, 0x00 } },
    { 0x015e83, 6, { 0x8d, 0x87, 0xfc, 0x13, 0x00, 0x00, 0 } },
};
#define CE_NMENU (sizeof(CE_MENU)/sizeof(CE_MENU[0]))
#define CE_POOL_CAVE_NAME "codecave:th18_card_pool"
#define CE_POOL_SLOTS  8192
/* 每条：改后应有的字节（disp32 处先置 0），uses_cave 时 disp32 = cave + off，否则 = off */
typedef struct { uint32_t rva; uint8_t len, disp_at, uses_cave, want[8]; uint32_t off; } ce_pool_t;
static const ce_pool_t CE_POOL[] = {
    { 0x016f6e, 5, 1, 0, { 0x68, 0x00, 0x00, 0x00, 0x00, 0, 0, 0 }, 0x8000 },
    { 0x016f73, 6, 1, 1, { 0xb8, 0x00, 0x00, 0x00, 0x00, 0x90, 0, 0 }, 0x0 },
    { 0x017100, 6, 1, 1, { 0xb8, 0x00, 0x00, 0x00, 0x00, 0x90, 0, 0 }, 0x0 },
    { 0x01712f, 7, 3, 1, { 0x89, 0x14, 0xbd, 0x00, 0x00, 0x00, 0x00, 0 }, 0x0 },
    { 0x017136, 7, 3, 1, { 0x89, 0x14, 0xbd, 0x00, 0x00, 0x00, 0x00, 0 }, 0x4 },
    { 0x01713d, 7, 3, 1, { 0x89, 0x14, 0xbd, 0x00, 0x00, 0x00, 0x00, 0 }, 0x8 },
    { 0x017144, 7, 3, 1, { 0x89, 0x14, 0xbd, 0x00, 0x00, 0x00, 0x00, 0 }, 0xc },
    { 0x01714b, 7, 3, 1, { 0x89, 0x14, 0xbd, 0x00, 0x00, 0x00, 0x00, 0 }, 0x10 },
    { 0x01718f, 7, 3, 1, { 0x8b, 0x0c, 0x95, 0x00, 0x00, 0x00, 0x00, 0 }, 0x0 },
};
#define CE_NPOOL (sizeof(CE_POOL)/sizeof(CE_POOL[0]))
typedef struct { uint32_t rva; uint8_t len, pre_len, post_len, pre[2], post[2]; } ce_unlock_t;
static const ce_unlock_t CE_UNLOCK[] = {
    { 0x01440b, 7, 2, 1, { 0x38, 0x8a }, { 0x90, 0 } },
    { 0x0149ec, 8, 2, 2, { 0x80, 0xbe }, { 0x00, 0x90 } },
    { 0x016590, 8, 2, 2, { 0x80, 0xbb }, { 0x00, 0x90 } },
    { 0x01694e, 8, 2, 2, { 0x80, 0xba }, { 0x00, 0x90 } },
    { 0x016e3d, 7, 2, 1, { 0x38, 0x82 }, { 0x90, 0 } },
    { 0x017125, 8, 2, 2, { 0x80, 0xbe }, { 0x00, 0x90 } },
    { 0x017ea3, 8, 2, 2, { 0x80, 0xb9 }, { 0x00, 0x90 } },
    { 0x018df6, 8, 2, 2, { 0x80, 0xbf }, { 0x00, 0x90 } },
    { 0x018e15, 7, 2, 1, { 0x8a, 0x80 }, { 0x90, 0 } },
};
#define CE_NUNLOCK (sizeof(CE_UNLOCK)/sizeof(CE_UNLOCK[0]))

/* ---- 音效表扩容（sound_sites.py）---- */
#define CE_SND_CFG_ROWS    84      /* 原生 84 行 */
#define CE_SND_NEW_N       32      /* 新 id 0x54..0x73 */
#define CE_SND_ROWS_TOTAL  116
#define CE_SND_CFG_ROW     0x14
#define CE_SND_SLOT_SIZE   0x18
#define CE_SND_NAMES_N     72      /* 原生 wav 名 / blob 槽数 */
#define CE_SND_NAMES_TOTAL 104
#define CE_SND_FIRST_ID    0x54
#define CE_SND_LAZER2_SLOT 20     /* 0x45ff38 硬编码引用的槽；wav 下标应为 0x26 */
#define CE_SND_LAZER2_WAV  0x26
#define CE_SND_CAVE_CFG    "codecave:th18_snd_cfg"
#define CE_SND_CAVE_NAMES  "codecave:th18_snd_names"
#define CE_SND_CAVE_SLOTS  "codecave:th18_snd_slots"
#define CE_SND_CAVE_BLOBS  "codecave:th18_snd_blobs"
