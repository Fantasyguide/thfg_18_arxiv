/* great_work_core.c —— 伟大之作的两条公式（见 great_work_core.h）。只用整数运算。 */
#include "great_work_core.h"

int32_t gw_money_for_hp(int32_t hp)
{
    int32_t n;

    if (hp <= 0) return 1;                       /* 能转出来的就不该白转 */
    n = hp / GW_MONEY_PER_HP;
    if (n < 1) n = 1;
    if (n > GW_MONEY_MAX) n = GW_MONEY_MAX;
    return n;
}

int32_t gw_recharge(int32_t bullets, int64_t hp_sum)
{
    int64_t frames;

    if (bullets < 0) bullets = 0;
    if (hp_sum < 0) hp_sum = 0;
    frames = (int64_t)GW_BASE + (int64_t)bullets * GW_PER_BULLET + hp_sum * GW_PER_HP;
    if (frames < GW_MIN) frames = GW_MIN;
    if (frames > GW_MAX) frames = GW_MAX;
    return (int32_t)frames;
}

int32_t gw_recharge_permille(int32_t frames)
{
    if (frames <= 0) return 0;
    if (frames >= GW_MAX) return 1000;
    return (int32_t)((int64_t)frames * 1000 / GW_MAX);
}
