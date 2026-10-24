/* 黑桃 J —— 移动速度 +10%。
 *
 * ★ 2026-10-24（owner 定案）：**走"改步长"这条路，不碰移速倍率**。
 *   一手（`th18:0x40c816`–`0x40c844`，原生卡 29 疾风木屐就是这么写的）：
 *     `sht = [player+0x47940]`（SHT 指针）
 *     `player+0x477b4`（直线步长，1/128 px）= `(int)([sht+0x10] * 128.0f)`
 *     `player+0x477bc`（斜向步长）          = `(int)([sht+0x18] * 128.0f)`
 *   ⇒ 引擎自带的那条「加速中」黄色染色**只读 `0x477ec`**（`0x45c6c2` 读、`0x45c6e6` 写 `color_2`），
 *     改步长**完全不会触发它** ⇒ 不再需要 `on_item_frame` 顶色（那版已撤）。
 *   与疾风木屐的关系：它把步长**钉死**成 2×（`0x400`/`0x2d4`），我们只 ×1.1；两者都写同一格，
 *   同时装时谁后写谁生效（AbilityManager tick 顺序 = 获得顺序），属预期。
 */
#include "sdk.h"
#include "royal.h"   /* 皇家同花顺：五张黑桃共用 ctor */

static int on_tick_2(ce_card_t *c)
{
    ce_royal_tick(c);
    uint8_t *p = CE_PLAYER();
    if (p) {
        const uint8_t *sht = *(const uint8_t *const *)(p + CE_PLAYER_SHT_PTR);
        if (sht) {
            *(int32_t *)(p + CE_PLAYER_STEP_STRAIGHT) =
                (int32_t)(*(const float *)(sht + 0x10) * 128.0f * 1.1f);
            *(int32_t *)(p + CE_PLAYER_STEP_DIAG) =
                (int32_t)(*(const float *)(sht + 0x18) * 128.0f * 1.1f);
        }
        {   static unsigned s_n;
            if ((++s_n % 120u) == 0u)
                ce_log_key("sj: STEP straight=%d diag=%d mult=%.3f",
                           *(int32_t *)(p + CE_PLAYER_STEP_STRAIGHT),
                           *(int32_t *)(p + CE_PLAYER_STEP_DIAG),
                           (double)*(volatile float *)(p + CE_PLAYER_SPEED_MULT));
        }
    }
    return 0;
}

/* on_delete：删卡（倒吊人 / 原版空白卡）时收掉皇家同花顺演出的文件级倒计时（见 royal.c 的 ce_royal_dismiss）。
 * 步长不用还：引擎每帧按 SHT 重写 `0x477b4/0x477bc`（疾风木屐也是每帧写），
 * 我们不再写就自动回到原值。*/
CE_CARD(59, .ctor = ce_royal_flush_ctor, .on_tick_2 = on_tick_2, .on_delete = ce_royal_dismiss);
