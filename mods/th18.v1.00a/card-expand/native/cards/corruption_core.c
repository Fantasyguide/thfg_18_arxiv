/* corruption_core.c —— 腐化（id 70）的纯逻辑。语义与一手对位表在 `corruption_core.h` 头注。 */
#include "corruption_core.h"

int32_t corruption_charge_death_save(corruption_budget_t *b, int32_t spent)
{
    if (!b || spent <= 0) return 0;             /* 没有支出（按 B 自救 / 救场卡都在这一类）*/

    /* 钳位：预算不为负。`spent` 来自引擎（1–2）或卡侧上报（108 的 1），正常永远 ≤ MAX；
     * 真出现 > MAX 的异常值也只把预算扣到 0，并把「实际记了多少」如实返回给调用方去记日志。*/
    if (spent > b->max_bombs) spent = b->max_bombs;
    if (spent <= 0) {                           /* MAX 已经是 0 ⇒ 无可扣（不该发生：没 B 时引擎不放行死亡回避）*/
        b->current_bombs = b->max_bombs;
        return 0;
    }

    b->max_bombs -= spent;                      /* ★ 腐化只动上限 */
    b->current_bombs = b->max_bombs;            /* 当前数拉平到新预算 ⇒ 钉不变式不会再白送 */
    return spent;
}
