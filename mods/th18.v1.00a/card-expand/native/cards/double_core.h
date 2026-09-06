/* double_core.h —— 加倍（id 69）的纯逻辑：掉落表翻倍。纯 C11，主机单测 tests/test_double_core.c。
 *
 * 敌人的掉落结构 zEnemyDrop（enemy+0x623c，也是 Enemy__drop_items_and_notify_cards 0x430510 的 ecx）：
 *   +0x00 main_drop   int32   dropMain(type) / enmCreate 第 6 参：一颗，引擎在调 0x430510 **之前**已经撒掉（0x430469 / 0x42d0c6），调完才清零
 *   +0x04 ex_drop[20] int32   dropExtra(type, n)：0x430510 按 type 1..0x13 逐类撒，撒完 memset 清零
 * 断点在 0x430510 入口：主掉落那一颗已经飞出去了，翻倍只能把它「补」进 ex_drop 对应格（+1），让引擎的附加表循环再撒一颗；
 * ex_drop 本身 ×2。合计每种 = 2 ×（主 + 附加）。AUDIT §S10。 */
#pragma once
#include <stdint.h>

#define DBL_MULT        2
#define DBL_DROP_TYPES  20      /* ex_drop 格数（memset 0x50 字节）*/
#define DBL_DROP_MAX    19      /* 0x430510 的循环只撒 type 1..0x13：第 20 格永远不撒，主掉落补格也只认到 19 */

/* main_drop：主掉落类型（0 = 无）；ex：ex_drop[DBL_DROP_TYPES]。返回补进 ex 的主掉落格下标（-1 = 没补）*/
int dbl_apply(int32_t main_drop, int32_t *ex);
