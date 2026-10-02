/* ufo_bounty.h —— ★★★ 2026-09-26（owner）：「给压缩包一个**等价计数**：我吃了这个等价于吃了
 *   多少个大信仰点、多少个中信仰点、多少个樱点、多少个 P、多少个金钱 —— **其他卡要用**」。
 *
 * 用途：107（樱点）/ 110·111（信仰）拿到一个被吃掉的压缩包时，可以按自己那套口径折算入账，
 *   而不必去猜「这个包是怎么攒出来的」。计数 = UFO 在场期间**真正吞下去的东西的原样分类**
 *   （不是转换后的通道计数 —— 通道会被彩虹互换、被满 P 规则改写，折算不了）。
 *
 * 用法（**取走即清**，一次性闩，与 `ce_cancel_latch_claim()` 那一族同一个口径）：
 *      ufo_bounty_eq_t eq;
 *      if (ufo_bounty_eq_take(&eq)) { ...把 eq 折算进自己那本账... }
 *   ⚠ 只在**压缩包被吃下那一帧**置位；没人取就一直留着（直到下一个包被吃）。
 *   `ufo_bounty_eq_pending()` = 现在有没有未取走的等价计数（诊断/调试用）。
 */
#ifndef UFO_BOUNTY_H
#define UFO_BOUNTY_H

typedef struct {
    int32_t p;          /* 等价于吃了多少颗 **P 点**（原生 type 1）*/
    int32_t money;      /* ……**金钱点**（原生 type 2）*/
    int32_t big_faith;  /* ……**大信仰点**（110/111 的 `FAITH_TIER_BIG`）*/
    int32_t mid_faith;  /* ……**中信仰点**（110/111 的 `FAITH_TIER_SMALL`）*/
    int32_t cherry;     /* ……**樱点实体**（107，`1 << FAITH_SLOT_107`）*/
} ufo_bounty_eq_t;

int ufo_bounty_eq_pending(void);
int ufo_bounty_eq_take(ufo_bounty_eq_t *out);   /* 1 = 取到了（并已清零）；0 = 没有 */

#endif /* UFO_BOUNTY_H */
