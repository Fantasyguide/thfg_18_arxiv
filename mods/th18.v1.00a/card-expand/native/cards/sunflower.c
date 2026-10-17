/* 向日葵（id 84，致敬植物大战僵尸）—— 主动卡：消耗 0.50 火力，在**自机当下的位置**种一株向日葵。
 * 它不跟随自机、原地不动，替玩家挡子弹（每挡一发 −1 血），血量不多（75，9b623ff1 平衡 150→75）；
 * 每存活 5 秒撒一轮 25 个**火力道具**。
 * 血尽即枯萎，过关 / 局末消失。
 *
 * 机制与代码都在 `plant.c`（与金盏花 85 共用，AUDIT §AN / §AO）；这里只给这株植物的数值。
 * 为什么产的是道具而不是直接加火力：直接写 `CURRENT_POWER` 就没有「向日葵在产阳光、你得去捡」这一层了。
 * 撒的是零售道具 type 1，掉落 / 回收 / **满火力自动转分**全走引擎自己的路。
 * 25 个 = 名义 0.25 火力，两轮回本（成本 0.50）—— 前提是你活着把它们捡回来。
 *
 * 美术：PvZ1 原版待机动画 27 帧（`ability.anm` script468（上游 137）/ `@SF_1..@SF_27`，每帧停 4 game frame；
 * 9b623ff1 的「加密提速」把 9 帧 × 18 改成 27 帧 × 4），
 * 出处见 `assets/ability/sunflower/_src/README.md`。 */
#include "plant.h"

static const plant_def_t SUNFLOWER = {
    .tag        = "sunflower",
    .script     = CE_ANM_ABILITY_SCRIPT_SUNFLOWER,
    .hp         = 75,           /* 「血量不多」：炎魔 800 / 青眼 1500 的零头（9b623ff1 平衡：150 → 75）*/
    .period     = 300,          /* 每存活 5 s 产一轮 */
    .items      = 25,
    .item_type  = 1,            /* 火力道具 */
    .power_cost = 50,           /* 0.50 火力 */
    .power_gate = 150,          /* 成本 + 一档（引擎永远保留 1.00）*/
    .radius     = 22.0f,        /* 场上缩到 0.7（script468）→ 植株约 52×51，取半宽略小一点（9b623ff1：32 → 22）*/
};

static int  on_activate(ce_card_t *c)                   { return plant_activate(c, &SUNFLOWER); }
static int  on_active_tick(ce_card_t *c, uint32_t e)    { return plant_tick(c, &SUNFLOWER, e); }
static void on_stage_start(ce_card_t *c)                { plant_clear(c, &SUNFLOWER, "stage start"); }
static void on_run_reset(ce_card_t *c)                  { plant_clear(c, &SUNFLOWER, "run reset"); }

CE_CARD(84, .active_recharge = 600,                     /* 10 s，与青眼 / 炎魔同 */
            .on_activate = on_activate, .on_active_tick = on_active_tick,
            .on_stage_start = on_stage_start, .on_run_reset = on_run_reset);
