/* double_core.c —— 加倍（id 69）的纯逻辑。见 double_core.h。 */
#include "double_core.h"

int dbl_apply(int32_t main_drop, int32_t *ex)
{
    for (int i = 0; i < DBL_DROP_TYPES; ++i)
        if (ex[i] > 0) ex[i] *= DBL_MULT;
    if (main_drop >= 1 && main_drop <= DBL_DROP_MAX) {
        ex[main_drop - 1] += DBL_MULT - 1;          /* 引擎已经撒了 1 颗，补 (倍率−1) 颗 */
        return main_drop - 1;
    }
    return -1;
}
