#include "../sdk/public.h"

static int on_tick(ce_card_t *card)
{
    (void)card;
    return 0;
}

static const ce_hooks_t hooks = {
    .on_tick = on_tick,
    .active_recharge = 0,
};

int main(void)
{
    return hooks.on_tick == on_tick && CE_ACTIVATE_REFUSED == -1 ? 0 : 1;
}
