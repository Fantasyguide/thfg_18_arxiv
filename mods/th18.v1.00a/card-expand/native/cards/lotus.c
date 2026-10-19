/* lotus.c —— 81「黑莲花」（**即时卡**，`cards.js` 的 `category: 3`）
 *
 * ============================ ★ 占位（效果未实装）============================
 * 本文件现在只做**占位注册**：卡在商店 / 图鉴 / 卡组里可见（卡图 = `assets/cards/BLACK_LOTUS_*.png`，
 * 上游同源），但**买下/入手不会产生任何效果**。等实现时按下面这份目标说明补。
 *
 * **目标效果**（上游 `stg-agent-proto` 的原文，逐字）：
 *   「商店免费，购买时立刻获得 3.00 火力；超出火力上限的部分按 1 点 ＝ 1 金钱折算。本身不会进入卡组。」
 * **上游实现**：`upstream/stg-agent-proto:mods/th18.v1.00a/card-expand/native/cards/lotus.c`（47 行）
 *   —— 走 `ce_grant_power`（**我们这边还没有这个 helper** ⇒ 真搬迁要先把它加进 `sdk.h`，属共享件改动）。
 * **一手依据**（上游文件头）＝ `CardRingo__constructor` `th18:0x40a0b0` 的四步 + 溢出由引擎自己钳 `MAX_POWER`。
 *
 * 【本占位为什么 `ctor` 返回 0】目标实现返回 1（成交即消耗、`deck_visible: 0`）；占位返回 0 ⇒
 *   卡留在手里、什么也不做（不会出现「花了钱卡没了还没效果」这种更糟的占位表现）。*/
#include "sdk.h"

static int ctor(ce_card_t *c)
{
    (void)c;
    ce_log("lotus: placeholder (card 81 黑莲花) - 效果未实装；上游 lotus.c 待搬迁（需先补 ce_grant_power）");
    return 0;                     /* 目标实现 = return 1 */
}

CE_CARD(81, .ctor = ctor);
