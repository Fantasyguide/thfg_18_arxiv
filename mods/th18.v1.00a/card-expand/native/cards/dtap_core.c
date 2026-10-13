/* dtap_core.c —— 「完整双击」纯逻辑状态机（共享件；语义与注释见 dtap_core.h ✓）
 *
 * ★★★ 2026-10-14（P23）：本文件是把 116（`season_core.c`）2026-09-29 的实现**逐字搬出来**的 ✓
 *   —— 逻辑一字未改，只把写死的 `SEASON_DTAP_WINDOW` 换成结构里的 `window` 字段 ✓。
 *   谁在用：116（薄包装 `season_dtap_*`，行为逐字不变 ✓）与 111（直接调用 ✓）。
 *   为什么搬：owner 要求 111 的手势与 116 一致 ⇒ 两卡共用**同一份**实现 ✓，
 *   不再各写一份（本仓明确反对"同一类表现、做法茫茫多" ✗）。*/

#include "dtap_core.h"

void ce_dtap_init(ce_dtap_t *d, int window)
{
    if (!d) return;
    d->state = 0;
    d->win = 0;
    d->window = (window > 0 && window <= 255) ? (uint8_t)window : (uint8_t)CE_DTAP_WINDOW_DEFAULT;
}

int ce_dtap_tick(ce_dtap_t *d, int held)
{
    uint8_t W;

    if (!d) return 0;
    W = d->window ? d->window : (uint8_t)CE_DTAP_WINDOW_DEFAULT;   /* ★ 零值安全（卡侧 memset 即可 ✓）*/
    held = held ? 1 : 0;

    switch (d->state) {
    case 0:                                     /* 空闲：等第一次按下 */
        if (held) {
            d->state = 1;
            d->win = W;                         /* ★ 窗口从**第一次按下**这一刻起算 ✓ */
        }
        return 0;

    case 1:                                     /* 第一次按下：等「放开」这一下 */
        if (d->win > 0) --d->win;
        if (!held) {
            d->state = (d->win > 0) ? 2 : 0;    /* 窗口已经走完才松手 ⇒ 这次手势作废 ✓ */
        } else if (d->win == 0) {
            d->state = 3;                       /* 一直按着（窗口用光）⇒ 作废，等松手才重新武装 ✓ */
        }
        return 0;

    case 2:                                     /* 已松手：等第二次按下（窗口内）*/
        if (held) {
            d->state = 4;                       /* ★ 触发；等这次松手才重新武装（防连发 ✓）*/
            return 1;
        }
        if (d->win > 0) {
            --d->win;
            if (d->win == 0) d->state = 0;      /* 窗口内没等到第二次按下 ⇒ 清零 ✓ */
        }
        return 0;

    case 3:                                     /* 第一次按太久：等松手 → 回空闲 */
        if (!held) d->state = 0;
        return 0;

    default:                                    /* state 4：刚触发完，等松手 → 回空闲 */
        if (!held) d->state = 0;
        return 0;
    }
}
