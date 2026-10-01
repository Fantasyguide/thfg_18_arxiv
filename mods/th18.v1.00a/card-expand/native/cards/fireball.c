/* fireball.c —— 83「火球术」（主动卡）
 *
 * ============================ ★ 占位（效果未实装）============================
 * 目标效果（上游原文）：见 `cards.js` 的 `desc`（玩家面向）与
 *   `upstream/stg-agent-proto:mods/th18.v1.00a/card-expand/native/cards/fireball.c` 的文件头（机器面向）。
 * 本文件现在只注册**占位行为**：`category: 0`（主动）所需的 `active_recharge` 按上游值取 480 帧、
 *   `on_activate` 只打一条日志并返回 0（瞬发、不消耗）⇒ 按 C 不会有效果，也不会卡住状态。
 * `active_recharge` = 480（上游值，照抄；实现搬迁时不要改这个数）。
 * 搬迁工作量（上游）：fireball.c 141 行，复用炎魔之王（72）的贴图/脚本 ⇒ 不需要新美术*/
#include "sdk.h"

static int on_activate(ce_card_t *c)
{
    (void)c;
    ce_log("fireball: placeholder (card 83 火球术) - 效果未实装；上游 fireball.c 待搬迁");
    return 0;                     /* 瞬发，不进入持续态 */
}

CE_CARD(83, .active_recharge = 480, .on_activate = on_activate);
