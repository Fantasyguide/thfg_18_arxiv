/* 加倍（id 69）—— 桥牌的 Double（X）：罚分加倍，奖励也加倍。
 *
 *   · Miss 掉 **2** 条命（而不是 1）
 *   · 敌人掉落的道具全部 **×2**
 *
 * 【掉 2 命】挂 +0x0c on_death_after_deathbomb，**不是** +0x14。
 * 死亡序列（engine/card/th18/03-hooks.md §4）：
 *     决死窗口耗尽 → acc = 0; for card: acc |= vtable[0x0c]      ← 我们在这
 *        acc == 0 → Player__commit_death_and_enter_state2 0x45D090（扣命、判 game over）
 *     状态 2 第 3 帧 → 扣火力/撒道具 → 扣钱 → vtable[0x14]        ← 补偿类卡在这
 * 在引擎扣命**之前**多扣 1，它自己的判定就会看到正确的残机数。
 * `0x45d1a0 sub eax,1` 之后两处都是 **js**（判 < 0，不是精确 == -1），所以 1 → 0 → -1 会
 * 正常触发 game over。挂 +0x14 则会绕过那个判定，残机可能停在 -1 而游戏继续。
 * 与 CardTewi 同样的手法：在 +0x0c 里只记账、`return 0` 不救命。
 *
 * ★ 只在 CURRENT_LIVES > 0 时多扣，残机永远不会被我们压到负数（引擎自己那一下照常）。
 *
 * 【掉落 ×2】2026-09-12 改成**声明式**（EVENTS-CONTRACT.md §2）：本卡只 `ce_drop_mult_set(69, 2000)`
 * 声明「我的倍数是 ×2」，乘法由 SDK 的掉落层在敌人撒之前**一次性**作用在「最终总数」上：
 *     final[t] = ( base[t] + Σ 各卡的加法 ) × Π 各卡的乘数       （base[main] = counts[main] + 1）
 * ⇒ 不再依赖卡链顺序，且**别的卡加到的那颗也会被翻倍**（`(1+1)×2 = 4`，旧实现给 `1×2+1 = 3`）。
 * 小怪的主掉落（dropMain 一颗，引擎在调 0x430510 **之前**就撒了）也由层里的 `+1` 补偿覆盖
 * —— 旧实现把这条补偿写在卡自己的 `dbl_apply` 里，现在搬进了层（单一落笔，见 AUDIT §S10）。
 */
#include "sdk.h"

static int on_death_after_deathbomb(ce_card_t *c, uint32_t acc)
{
    (void)c; (void)acc;
    if (CE_CURRENT_LIVES() > 0) {
        CE_CURRENT_LIVES() -= 1;                    /* 引擎紧接着还会再扣 1 → 一次 Miss 共 2 条 */
        ce_log("double: miss costs 2 lives (now %d before the engine's own -1)", CE_CURRENT_LIVES());
    } else {
        ce_log("double: miss with %d lives left, engine's -1 ends the run anyway", CE_CURRENT_LIVES());
    }
    return 0;                                        /* 不救命，只记账 */
}

/* 声明 ×2（千分比 2000）。声明是粘性的：绑定/每关 on_load 时登记一次，
 * 删卡（倒吊人 / 原版空白卡）时 CE_CARD 的 on_run_reset / opdelete 自动 `ce_layer_drop_card()` 清掉。*/
static int on_load(ce_card_t *c)
{
    (void)c;
    ce_drop_mult_set(69, 2000);
    return 0;
}

CE_CARD(69, .on_load = on_load,
            .on_death_after_deathbomb = on_death_after_deathbomb);
