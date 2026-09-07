#pragma once
#include <stdint.h>

typedef int  (*ce_allocate_card_cb)(void *ctx, uint32_t id, uint32_t mode);
typedef void (*ce_mark_card_cb)(void *ctx, uint32_t id, uint32_t notify);

int ce_give_card_transaction(uint32_t id, uint32_t mode, uint32_t notify,
                             ce_allocate_card_cb allocate_card, ce_mark_card_cb mark_card,
                             void *ctx);
