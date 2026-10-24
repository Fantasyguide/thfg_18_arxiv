/* 金盏花（id 85，致敬植物大战僵尸）—— 主动卡：消耗 **1.00 火力**，在自机当下的位置种一株金盏花。
 * 与向日葵（84）同一套机制（`plant.c`），只有两处不同：**成本翻倍**、**产的是金钱道具**。
 *
 * 【为什么贵一倍】
 * PvZ 里金盏花产的是金币而不是阳光 —— 钱是拿去买东西的，不回到战斗里。TH18 这边更极端：
 * `MONEY` **既是购卡货币又是计分乘数**（`05-shop-and-money.md` §2），所以 25 金钱比 25 点火力值钱得多，
 * 成本从 0.50 抬到 1.00。门槛相应是 2.00（成本 + 引擎永远保留的那一档）。
 *
 * 【产的是道具，不是直接加钱】
 * 撒零售道具 type 2（金钱），引擎自己走「`MONEY += 1` + 按身价计分」那条（`ItemManager__collect_money_item`
 * `0x446b00`）—— 于是黑桃 10（58，金钱道具 +10％）、辉针城（127，收点批结算）这些吃道具的卡都能叠上，
 * 直接写 `MONEY` 就全断了。一轮 25 个 = 25 金钱（一张便宜卡的一半）。
 *
 * 美术：PvZ1 原版 `Marigold.reanim` 逐帧渲染 24 帧（`ability.anm` script469（上游 138）/ `@MG_1..@MG_24`，
 * 每帧停 4 game frame；9b623ff1 的「加密提速」把 9 帧 × 18 改成 24 帧 × 4），
 * 花头与花瓣的白底按原版的做法着成金黄；出处见 `assets/ability/marigold/_src/README.md`。 */
#include "plant.h"

static const plant_def_t MARIGOLD = {
    .tag        = "marigold",
    .script     = CE_ANM_ABILITY_SCRIPT_MARIGOLD,
    .hp         = 75,           /* 与向日葵同：血量不多（9b623ff1 平衡：150 → 75）*/
    .period     = 1200,         /* 2026-10-24 owner 调整：每存活 20 s 产一轮；见 AUDIT §BE-RELEASE-20261024。 */
    .items      = 25,
    .item_type  = 2,            /* 金钱道具 */
    .power_cost = 100,          /* 1.00 火力 */
    .power_gate = 200,          /* 成本 + 一档 */
    .radius     = 22.0f,        /* 场上缩到 0.7（script469）→ 植株约 41×55，与向日葵同一档（9b623ff1：32 → 22）*/
};

static int  on_activate(ce_card_t *c)                   { return plant_activate(c, &MARIGOLD); }
static int  on_active_tick(ce_card_t *c, uint32_t e)    { return plant_tick(c, &MARIGOLD, e); }
static void on_stage_start(ce_card_t *c)                { plant_clear(c, &MARIGOLD, "stage start"); }
static void on_run_reset(ce_card_t *c)                  { plant_clear(c, &MARIGOLD, "run reset"); }

CE_CARD(85, .active_recharge = 600,                     /* 10 s，与向日葵同 */
            .on_activate = on_activate, .on_active_tick = on_active_tick,
            .on_stage_start = on_stage_start, .on_run_reset = on_run_reset);
