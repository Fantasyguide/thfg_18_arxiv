/* solring.c —— 82「阳光戒指」（被动卡）
 *
 * ============================ ★ 占位（效果未实装）============================
 * 目标效果（上游原文）：「每 20 秒自动获得 0.10 火力。只在关卡里计时，商店与菜单不算。火力已满时不再累积。」
 * 上游实现：`upstream/stg-agent-proto:.../native/cards/solring.c`（47 行）= 私有计数器 + `ce_grant_power`；
 *   60 fps ⇒ `SR_INTERVAL_FRAMES = 1200`，局末 `on_run_reset` 清零；无随机 / 无浮点 / 无键盘（replay 安全）。
 * ⚠ 同样要等 `ce_grant_power`（共享件）先落地 —— 本文件现在只注册一个空 tick，保证
 *   「卡表 `category: 2`（被动）↔ 行为」契约成立、卡在游戏里可见但不做事。*/
#include "sdk.h"

static int on_tick_2(ce_card_t *c)
{
    (void)c;                      /* 占位：目标实现 = 每 1200 帧 +0.10 火力（`ce_grant_power`）*/
    return 0;
}

CE_CARD(82, .on_tick_2 = on_tick_2);
