/* 冰淇淋（id 90）—— **被动卡**，致敬《杀戮尖塔》的遗物「冰淇淋」（Ice Cream）。
 *
 * 效果（owner 2026-10-02 逐字）：「miss 时不会损失火力，且 bomb 直接 +3。
 *   正常 miss 时候会掉 1p 火力，并且 bomb 重设为 3 的」。
 *
 * ## 一手：TH18 一次 miss 原生做什么（decompile，见 `ice_cream_core.h` 与 AUDIT §BE #214）
 * `Player__on_tick__body` 的死亡分支 `0x45c133`–`0x45c154`：
 *   ① `0x45c135 call 0x45d090`（`Player__commit_death_and_enter_state2`）
 *      ⇒ `CURRENT_BOMBS = clamp(3, 0, MAX_BOMBS)`（`0x45d1cb`–`0x45d1e8`）、`CURRENT_LIVES -= 1`、状态 → 2；
 *   ② `0x45c140 cmp [player+0x638], 3` → 只有这一帧刚死才继续；
 *   ③ `0x45c149 push [0x4ccd40]`（一档火力）+ `0x45c154 call 0x457480`（`GlobalsInner__spend_power`）
 *      ⇒ **`CURRENT_POWER -= 一档`**（钳到一档，`spend_power` 自己的下界）。
 * 之后引擎**遍历卡表**逐个调虚表 `+0x14`（`0x45c251`–`0x45c255`）—— 本卡就在那里把两个值写回去，
 * 于是「不损失火力 + 符卡 += 3」。**顺序是一手的**：`+0x14` 严格在扣火力之后（`0x45c255` > `0x45c154`）。
 *
 * ## 纪律
 * · `+0x0c`（真死那一刻）**只置位**；真正的写放在 `+0x14`（引擎已经结算完）。
 * · 快照**只在正常态**（`player+0x476ac == 1`）采，每帧一采（纯逻辑层判据，见 core 的头注 ①）。
 * · 写回符卡后要刷 HUD（`ce_gui_update_bombs()` = 引擎 `0x4420e0`，死亡结算里也是它刷的）；
 *   火力那行引擎自己的绘制每帧直接读全局 ⇒ 不用手动刷。
 * · 本卡**不留任何跨帧资源**（没有 VM / 池 / 子机槽）⇒ `on_delete` 无事可做，只清快照。 */
#include "sdk.h"
#include "ice_cream_core.h"

/* +0x2c：AbilityManager 每帧（菜单 / 商店 / 暂停里不跑）。快照就靠这一句保鲜。*/
static int on_tick_2(ce_card_t *c)
{
    ic_state_t *s = ce_state(c, ic_state_t);
    uint8_t *p = CE_PLAYER();
    if (!s || !p) return 0;
    ic_cache(s, *(int32_t *)(p + CE_PLAYER_STATE) == 1,          /* 正常态（1）才采 */
             CE_CURRENT_POWER(), CE_CURRENT_BOMBS());
    return 0;
}

/* +0x0c：真死那一刻（死弹窗结算之后）。只置位 —— 死亡栈上不写任何引擎字段。*/
static int on_death_after_deathbomb(ce_card_t *c, uint32_t acc)
{
    ic_state_t *s = ce_state(c, ic_state_t);
    (void)acc;
    if (s) ic_note_miss(s);
    return 0;
}

/* +0x14：死亡结算**之后**（引擎在这一步遍历卡表）—— 写回「生前火力」与「生前符卡 + 3」。*/
static int on_death_frame2(ce_card_t *c)
{
    ic_state_t *s = ce_state(c, ic_state_t);
    int32_t power = 0, bombs = 0;

    if (!s) return 0;
    if (!ic_apply_after_miss(s, CE_MAX_BOMBS(), &power, &bombs)) return 0;
    CE_CURRENT_POWER() = power;                 /* 把引擎刚扣掉的那一档还回来 */
    CE_CURRENT_BOMBS() = bombs;                 /* 引擎重设成 3 了，这里改成"生前 + 3" */
    ce_gui_update_bombs();                      /* 刷 HUD 符卡行（引擎 0x4420e0 同款） */
    ce_log("ice_cream: miss survived - power kept at %d, bombs -> %d (engine had reset it to 3)",
           (int)power, (int)bombs);
    return 0;
}

/* 局末 / 每关开场 / 删卡：清快照（下一帧正常态会立刻重新采）。本卡没有 VM / 池要收。*/
static void on_run_reset(ce_card_t *c)
{
    ic_state_t *s = ce_state(c, ic_state_t);
    if (s) ic_reset(s);
}

static int on_load(ce_card_t *c)
{
    on_run_reset(c);
    return 0;
}

static void on_delete(ce_card_t *c) { on_run_reset(c); }

CE_CARD(90, .on_tick_2 = on_tick_2,
            .on_death_after_deathbomb = on_death_after_deathbomb,
            .on_death_frame2 = on_death_frame2,
            .on_load = on_load, .on_run_reset = on_run_reset, .on_delete = on_delete);
