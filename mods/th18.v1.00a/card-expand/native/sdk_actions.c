#include "sdk_actions.h"

int ce_give_card_transaction(uint32_t id, uint32_t mode, uint32_t notify,
                             ce_allocate_card_cb allocate_card, ce_mark_card_cb mark_card,
                             void *ctx)
{
    if (!allocate_card || !mark_card) return -1;
    int result = allocate_card(ctx, id, mode);
    if (result >= 0) mark_card(ctx, id, notify);
    return result;
}
