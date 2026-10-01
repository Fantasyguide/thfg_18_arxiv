/* sunflower.c —— 84「向日葵」（主动卡）
 *
 * ============================ ★ 占位（效果未实装）============================
 * 目标效果（上游原文）：见 `cards.js` 的 `desc`（玩家面向）与
 *   `upstream/stg-agent-proto:mods/th18.v1.00a/card-expand/native/cards/sunflower.c` 的文件头（机器面向）。
 * 本文件现在只注册**占位行为**：`category: 0`（主动）所需的 `active_recharge` 按上游值取 600 帧、
 *   `on_activate` 只打一条日志并返回 0（瞬发、不消耗）⇒ 按 C 不会有效果，也不会卡住状态。
 * `active_recharge` = 600（上游值，照抄；实现搬迁时不要改这个数）。
 * 搬迁工作量（上游）：plant.c 122 行 + plant_core.c 34 行共基类 + 本卡数值；sunflower 还需 ability.anm 新脚本与 PvZ 原版帧素材（上游 assets/ability/sunflower/_src/）*/
#include "sdk.h"

static int on_activate(ce_card_t *c)
{
    (void)c;
    ce_log("sunflower: placeholder (card 84 向日葵) - 效果未实装；上游 sunflower.c 待搬迁");
    return 0;                     /* 瞬发，不进入持续态 */
}

CE_CARD(84, .active_recharge = 600, .on_activate = on_activate);
