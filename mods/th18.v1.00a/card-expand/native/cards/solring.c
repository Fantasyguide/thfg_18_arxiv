/* 阳光戒指（id 82，致敬・万智牌 Sol Ring）—— 被动：每 20 秒 +0.10 火力，一直滴。
 *
 * 【时钟走哪一条】
 * `on_tick_2`（虚表 `+0x2c`，管理器侧每帧）——**菜单 / 商店里不跑**（SDK.md §2），
 * 所以「20 秒」数的是真正在游戏里的帧，过关结算和逛商店的时间不算。60 fps ⇒ 1200 帧。
 * 计数器放这张卡的私有状态（`ce_state`，SDK.md §4），局末 `on_run_reset` 清零。
 *
 * 【加火力照零售的原型抄】
 * `ce_grant_power`（sdk.h）= `CardRingo__constructor` `0x40a0b0`（DANGO，+0.50）的四步：
 * `GlobalsInner__gain_power` `0x4573f0` → 只在**档数真变了**时 repopulate + 弹「火力上升」箭头 + `se_powerup`(0x0d)。
 * 火力满了引擎自己拒绝（一点不加、不响、不弹），所以**这张卡满火力时就是白滴**，不转成别的东西
 * （黑莲花 81 那条折算是它自己的效果，不是加火力这条路的通则）。
 *
 * 【replay 安全】无随机、无浮点、无键盘；计数器只被帧驱动 ⇒ 同一段输入必得同一结果。AUDIT §AL。 */
#include "sdk.h"

#define SR_INTERVAL_FRAMES  1200   /* 20 s × 60 fps */
#define SR_POWER_TENTHS     1      /* 每次 +0.10 = POWER_PER_LEVEL / 10 */

typedef struct {
    uint32_t frames;               /* 距上一次滴过去了几帧 */
    uint32_t ticks;                /* 本局滴了几次（只进日志）*/
} sr_state_t;

static int on_tick_2(ce_card_t *c)
{
    sr_state_t *s = ce_state(c, sr_state_t);
    if (!s) return 0;
    if (++s->frames < SR_INTERVAL_FRAMES) return 0;
    s->frames = 0;

    int amount = SR_POWER_TENTHS * CE_POWER_PER_LEVEL() / 10;
    int before = CE_CURRENT_POWER();
    int gained = ce_grant_power(amount);
    s->ticks++;
    ce_log("solring: tick %u -> power %d + %d = %d (gained %d, max %d)",
           s->ticks, before, amount, CE_CURRENT_POWER(), gained, CE_MAX_POWER());
    return 0;
}

static void on_run_reset(ce_card_t *c)
{
    sr_state_t *s = ce_state(c, sr_state_t);
    if (s) { s->frames = 0; s->ticks = 0; }
}

CE_CARD(82, .on_tick_2 = on_tick_2, .on_run_reset = on_run_reset);
