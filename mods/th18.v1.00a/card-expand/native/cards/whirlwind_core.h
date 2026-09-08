/* whirlwind_core.h —— 旋风斩（id 79）的纯逻辑：X 的算法 + 接续状态机。
 * 纯 C11、无引擎依赖，主机单测 tests/test_whirlwind_core.c。
 *
 * 规则（用户定）：放一发炸弹时额外再放 X 次，X = ⌊当前火力⌋ − 1，代价是 X 点火力。
 * 火力是百分之一档的 int（`CE_POWER_PER_LEVEL` = 100），所以 X = power / per_level − 1。
 *
 * 两条它自带的好性质，不是我们凑出来的：
 *   · **永远扣得实**：X > 0 时 power − X·per ∈ [per, 2·per)，正好落在引擎 `spend_power`
 *     「≤ 一档就拒绝、扣完钳一档」的界内侧 —— 那条拒绝分支永远碰不到。
 *   · **自带终止**：扣完之后火力在 [1.00, 1.99]，⌊·⌋ − 1 = 0，所以接出来的那几发再算 X 就是 0，
 *     不可能无限连。`chaining` 那一位是第二道保险（也让「这一发是我们放的」可被日志区分）。
 *
 * 时序（引擎侧在 whirlwind.c）：`do_bomb` 先置「正在放」再扣炸弹，我们的 `on_bomb_spent` 断点在扣之后 ——
 * 所以计算 X 的那一刻 bombing 已经是 1，`ww_tick` 不会在同一发里抢跑。 */
#pragma once
#include <stdint.h>

#define WW_MAX_EXTRA   3     /* 火力上限 4.00 ⇒ X ≤ 3；钳一下，别让异常的火力值放出一串 */
#define WW_GIVE_UP     600   /* 欠着的发数等这么多帧还放不出去（引擎守卫一直拒绝）就作废，不无限重试 */

typedef struct {
    uint8_t  left;       /* 还欠几发 */
    uint8_t  chaining;   /* 场上这一发是我们接出来的 —— 抑制它再算一次 X、再花一次火力 */
    uint16_t waited;     /* 欠着的这几发已经等了多少帧 */
    uint32_t chained;    /* 统计：本局接出来的总发数 */
    uint32_t spent;      /* 统计：本局为此花掉的火力（百分之一档）*/
} ww_state_t;

void ww_clear(ww_state_t *s);                     /* 只清状态机，不清统计 */

/* X = ⌊power / per_level⌋ − 1，钳进 [0, WW_MAX_EXTRA]。per_level <= 0 时返回 0。 */
int  ww_extra(int power, int per_level);

/* 玩家放了一发炸弹（`on_bomb_spent`）。返回要接的发数 X（0 = 不接，调用方也就不扣火力）；
 * 非 0 时已记进 left。我们自己接出来的那几发（chaining）一律返回 0。 */
int  ww_on_bomb(ww_state_t *s, int power, int per_level);

/* 每帧喂「引擎正在放炸弹吗」。返回 1 = 现在就该放下一发（调用方去调 do_bomb）。
 * 返回 1 时 chaining 已经置好 —— 必须如此：`on_bomb_spent` 在 `do_bomb` 内部就会触发。 */
int  ww_tick(ww_state_t *s, int bombing);

/* `do_bomb` 的结果回灌。ok = 引擎真的放出去了（返回 0）：欠账 −1、统计 +1；
 * 被守卫拦下则什么都不减，下一帧再试（`waited` 到 WW_GIVE_UP 为止）。 */
void ww_fired(ww_state_t *s, int ok);
