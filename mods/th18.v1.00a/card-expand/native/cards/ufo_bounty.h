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

/* ★★ 2026-10-03h（owner：「**接：三张卡都按各自的格子折算**」+ 统筹技术拍板）——
 *   **广播读法（新卡一律用这一支）**：112 是**单例**，而 107 / 110 / 111 三张卡都可能想拿同一份计数；
 *   旧的 `take()` 是「谁先取谁得」⇒ 先取的赢、后面的什么都拿不到。
 *   ⇒ 现在每次包被吃下会 **`++代数`**（`ufo_bounty_eq_gen()`），消费者**各自记一个代数**：
 *
 *       static uint32_t s_eq_seen;                  // 本卡自己的"看到哪一代"
 *       ufo_bounty_eq_t eq; uint32_t gen;
 *       if (ufo_bounty_eq_peek(&eq, &gen) && gen != s_eq_seen) {
 *           s_eq_seen = gen;
 *           ... 只把**自己关心那几格**折进自己那本账（读**不清**，三张卡互不影响）...
 *       }
 *
 *   ⚠ 代数 **0 = 从没有过包被吃下**（哨兵）；回绕时跳过 0。
 *   `ufo_bounty_eq_take()` 保留只为兼容旧调用点（会清 pending，**别和新读法混用**）。*/
uint32_t ufo_bounty_eq_gen(void);
int      ufo_bounty_eq_peek(ufo_bounty_eq_t *out, uint32_t *out_gen);   /* 1 = 有得读（并回代数）*/

/* ★★ 2026-10-03h（续）——**逐颗账**（比五个计数更精确；按 `mask` 只折自己槽里被吞的那些）：
 *
 *       ufo_bounty_eq_t eq; uint32_t gen; int i, n;
 *       if (ufo_bounty_eq_peek(&eq, &gen) && gen != s_eq_seen) {
 *           s_eq_seen = gen;
 *           n = ufo_bounty_eq_log_n();
 *           for (i = 0; i < n; ++i) {
 *               int32_t tier, value, mask;
 *               if (!ufo_bounty_eq_log(i, &tier, &value, &mask)) break;
 *               if (!(mask & MY_SLOT)) continue;          // ← 只折**我自己的**槽
 *               my_credit(tier, value);                   // ← 用**原值**折算，不用拍数
 *           }
 *       }
 *
 *   ⚠ 快照与 `peek` 是**同一代**的（每吃一个包一起换）；`ufo_bounty_eq_log_n()` = 本代几颗
 *     （0 = 这一代没吞过点池的东西）。`take()` 那一族**不会**换快照 ⇒ 新旧两种读法别混用。*/
int ufo_bounty_eq_log_n(void);
int ufo_bounty_eq_log(int index, int32_t *out_tier, int32_t *out_value, int32_t *out_mask);

#endif /* UFO_BOUNTY_H */
