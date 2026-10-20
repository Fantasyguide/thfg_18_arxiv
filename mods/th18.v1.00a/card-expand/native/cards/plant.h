/* plant.h —— 「消耗火力，在自机当下的位置种一株植物」这一类卡的共用引擎层（向日葵 84 / 金盏花 85）。
 *
 * 两张卡的引擎侧动作逐字相同（扣火力三步 → 起 ANM + 血条 → 每帧挡弹扣血 + 产出计时 → 枯萎收尾），
 * 只有数值与「产什么道具」不同，所以按 SDK.md §5 的约定（出现第二次就提炼）收到这里。
 * 纯逻辑在 `plant_core.c`（主机单测）；这一层只做引擎调用，不含判断分支之外的算术。
 *
 * 加一张新植物卡 = 写一个 `plant_def_t` + 四行转发：
 *
 *     static const plant_def_t DEF = { "sunflower", CE_ANM_ABILITY_SCRIPT_SUNFLOWER, … };
 *     static int  on_activate(ce_card_t *c)                 { return plant_activate(c, &DEF); }
 *     static int  on_active_tick(ce_card_t *c, uint32_t e)   { return plant_tick(c, &DEF, e); }
 *     static void on_stage_start(ce_card_t *c)               { plant_clear(c, &DEF, "stage start"); }
 *     static void on_run_reset(ce_card_t *c)                 { plant_clear(c, &DEF, "run reset"); }
 */
#pragma once
#include "sdk.h"
#include "plant_core.h"

typedef struct {
    const char *tag;          /* 日志前缀（也是拒绝发动时的名字）*/
    int         script;       /* `ability.anm` 里这株植物的脚本号 */
    int32_t     hp;           /* 血量：每挡一发弹 −1 */
    uint32_t    period;       /* 每存活多少帧产一轮 */
    uint32_t    items;        /* 一轮撒几个道具 */
    uint32_t    item_type;    /* 道具 type：1 = 火力、2 = 金钱（05-shop-and-money.md §8）*/
    int         power_cost;   /* 扣多少火力（100 = 1.00）*/
    int         power_gate;   /* 门槛：成本 + 一档（引擎永远保留 1.00，不留一档实际扣的会比名义少）*/
    float       radius;       /* 挡弹半径 */
} plant_def_t;

int  plant_activate(ce_card_t *c, const plant_def_t *def);
int  plant_tick(ce_card_t *c, const plant_def_t *def, uint32_t elapsed);
void plant_clear(ce_card_t *c, const plant_def_t *def, const char *why);
