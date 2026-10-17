/* 黑莲花（id 81，致敬・万智牌 Black Lotus）—— 商店**免费**的一次性卡：买下当场 +3.00 火力，
 * 超出火力上限的那部分按「1 点火力 = 1 金钱」折算成金钱。本身不进卡组。
 *
 * 【为什么是 ctor + return 1】
 * 即时卡，与强欲之壶 / 方片 2 / 倒吊人同一条路（SDK.md §2：ctor 返回非 0 → 分配尾段当场删掉自己）。
 * 商店成交是 mode 2（`0x4185c7`），ctor 只跑一次。价格档 0 ⇒ 价格表 `0x4b35c4`[0] = 0，所以「免费」
 * 是数据层的事，这里不碰钱包的扣款那一侧（05-shop-and-money.md「成交」）。
 *
 * 【加火力照零售的原型抄】
 * `ce_grant_power`（sdk.h）= `CardRingo__constructor` `0x40a0b0`（DANGO，+0.50）的四步：
 * `GlobalsInner__gain_power` `0x4573f0` → 档数变了才 repopulate + 弹「火力上升」箭头 + `se_powerup`(0x0d)。
 * **溢出不用自己算**：引擎自己钳 `MAX_POWER`、满火力时直接拒绝（一点不加），
 * 所以 `溢出 = 名义 − 真加进去的`，是引擎给的数，不是我们推的。满火力横幅也由引擎自己起。
 *
 * 【1 点火力 = 1 金钱 是怎么定的】
 * 不是引擎里的汇率（引擎唯一的火力↔钱接口是商店补差价，那是**惩罚性**的：动用火力会把金钱清零，
 * 05-shop-and-money.md §6）。这里取的是**零售资源卡自己的等价**：同一批掉落卡里
 * `CardNazrin`（PENDULUM，id 5）给 +50 金钱、`CardRingo`（DANGO，id 6）给 +0.50 火力
 * （道具 type `0x12`/`0x13`，同级掉落）⇒ 0.50 火力 ≍ 50 金钱 ⇒ 0.01 火力 ≍ 1 金钱。
 * 于是满火力买这张 = 300 金钱，火力全空买 = 满上 3.00。设计取值，不是引擎断言。AUDIT §AL。 */
#include "sdk.h"

#define BL_POWER_LEVELS     3    /* +3.00 火力 = 3 × POWER_PER_LEVEL（引擎里一档 = 100）*/
#define BL_MONEY_PER_POWER  1    /* 溢出的每 1 点（0.01）火力折 1 金钱 */

static int ctor(ce_card_t *c)
{
    (void)c;
    int want = BL_POWER_LEVELS * CE_POWER_PER_LEVEL();
    int before = CE_CURRENT_POWER();
    int gained = ce_grant_power(want);
    int overflow = want - gained;                          /* 引擎钳出来的差额，不是我们推的 */

    if (overflow > 0) {
        int32_t money = overflow * BL_MONEY_PER_POWER;
        CE_MONEY() += money;
        CE_MONEY_TOTAL() += money;                         /* 只记收入的统计量，与金钱道具入账一致 */
        ce_log("lotus: power %d + %d -> %d (max %d), overflow %d -> money +%d = %d",
               before, want, CE_CURRENT_POWER(), CE_MAX_POWER(), overflow, money, CE_MONEY());
    } else {
        ce_log("lotus: power %d + %d -> %d (max %d), no overflow",
               before, want, CE_CURRENT_POWER(), CE_MAX_POWER());
    }
    return 1;                                              /* 即时卡：当场销毁，不进卡组 */
}

CE_CARD(81, .ctor = ctor);
