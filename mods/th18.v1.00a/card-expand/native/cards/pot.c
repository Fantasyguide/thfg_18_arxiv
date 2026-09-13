/* 强欲之壶（游戏王）—— 购买时立刻获得两张随机卡牌；本身不进卡组。
 *
 * 即时卡：ctor 里施加效果后 return 1，分配尾段当场 operator_delete，不入链、不计数、不上 HUD
 * （原生 EXTEND / 六文钱同款，02-lifecycle.md §3）。mode 0（道具）与 mode 2（购买）都调 ctor；
 * mode 1（初始携带）不调——所以 cards.js 里 deck_visible: 0，编成里选不到它。
 *
 * 「随机」走商店自己的抽卡函数（游戏 RNG、未拥有、本关可用、按权重），与商店刷出来的一致。
 * 排除表里放自己的表行（否则可能抽到自己 → 递归）和已抽的那张。ctor 里 card+0x4c 还没写，表行用 ce_table_entry。
 * 再加一层深度护栏：万一排除失效，最多嵌套 1 层。 */
#include "sdk.h"

static int s_depth;

static int ctor(ce_card_t *c)
{
    uint32_t self = ce_card_id(c);
    if (s_depth > 0) { ce_log("pot: nested pot ignored"); return 1; }
    ++s_depth;
    uint8_t *exclude[3];
    int n = 0;
    exclude[n++] = ce_table_entry(self);
    for (int i = 0; i < 2; ++i) {
        uint8_t *e = ce_shop_pick_random(0, 14, exclude, n);
        if (!e) { ce_log("pot: random pool exhausted after %d card(s)", i); break; }
        exclude[n++] = e;
        uint32_t id = ce_entry_id(e);
        int r = ce_give_card(id, CE_MODE_SHOP, 0);
        ce_log("pot: gave card %u (allocate -> %d)", id, r);
    }
    --s_depth;
    return 1;                                   /* 当场销毁：壶本身不进卡组 */
}

CE_CARD(63, .ctor = ctor);
