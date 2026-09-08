/* stock_core.h —— 股市（id 80）的纯逻辑：行情模拟 / K 线环形缓冲 / 交易 / 蜡烛几何。
 * 纯 C11、无引擎依赖、全整数、无浮点；随机数走自带 LCG，只在开局播一次种（见下）。
 * 主机单测 tests/test_stock_core.c。设计：docs/superpowers/specs/2026-09-08-stock-market-design.md。
 *
 * 单位：价格 0.01 金钱（10000 = 100.00）；持股 0.0001 股；成本 / 市值 / 盈亏 = 金钱。
 *
 * 行情模型（2026-09-08 第二版，替换占位的三档 regime）——**隐藏参数 + 动量**：
 *   玩家只看得到价格，看不到下面两个状态量，得从 K 线里把它们推断出来。
 *
 *   · `drift`（动量）：每步的漂移本身是一个均值回归过程，回归目标随估值走
 *     （价格越高，目标越负）。漂移有惯性 ⇒ **价格变化自相关 ⇒ 趋势会延续**，
 *     这正是「看图择时」成为技能而不是赌博的原因。时间常数 STOCK_DRIFT_TAU 步。
 *   · `vol`（波动率）：同样是均值回归过程，围绕 STOCK_VOL_BASE 起伏 ⇒ **波动率聚集**：
 *     安静期与狂暴期交替。它是**可见的**——K 线的实体与影线长度就是它的证据
 *     （实测：本根全长与下一根绝对涨跌的相关系数 0.76）。
 *
 *   参数按三条验收目标调出来的（300 关模拟，起始现金 300，每关约 200 秒）：
 *     趋势跟随一关净赚中位 +390、反向操作 −191、随机乱按 −177、买入不动 −20；
 *     完美抄底逃顶中位 2.31 倍；撞钳位的关数 0.3％。
 *   也就是：**会看图 ≈ 一关赚一张卡钱，乱按要亏，什么都不做不赚不亏。**
 *   「乱按亏钱」来自每次买卖各自的取整损耗，反复横跳会被这条税吃掉——这是有意的。
 *
 * ★ 随机数用**自带的 LCG**，只在开局从引擎的回放安全 RNG 取一次种子。
 *   理由：引擎那只 RNG 是弹幕 / 敌人共用的，如果每步都去取，带不带这张卡会让同一关的
 *   弹幕长得不一样。现在我们对它的消耗是「每局一次」而不是「每秒四次」。
 *   回放安全性不变：种子取自确定的时点，之后纯函数推进。 */
#pragma once
#include <stdint.h>

#define STOCK_CANDLES          16      /* 环形缓冲根数 = 屏幕上的根数 */
#define STOCK_STEP_FRAMES      15      /* 每 15 帧走一步（1 秒 4 次）*/
#define STOCK_STEPS_PER_CANDLE 16      /* 16 步 = 240 帧 = 4 秒一根 */
#define STOCK_LOT              50      /* 每次买 / 卖的金钱数 */
#define STOCK_PRICE_OPEN       10000
#define STOCK_PRICE_MIN        1000    /* 10.00：与上限关于开盘价 100.00 对数对称 */
#define STOCK_PRICE_MAX        100000  /* 1000.00 */
#define STOCK_DRIFT_TAU        64      /* 漂移的回归时间常数（步）= 16 秒 ≈ 4 根 K 线，一屏能看到三四段趋势 */
#define STOCK_DRIFT_NOISE      11      /* 每步给漂移的扰动幅度（基点）；调大 = 趋势更猛、也更容易撞钳位 */
#define STOCK_DRIFT_CAP        300     /* 漂移绝对值上限（基点 / 步）*/
#define STOCK_VALUATION_PULL   200     /* 估值拉力：漂移的回归目标 = −(price − open) × 此值 / open */
#define STOCK_TARGET_CAP       60      /* 上面那个目标的绝对值上限（基点）*/
#define STOCK_VOL_TAU          64      /* 波动率的回归时间常数 */
#define STOCK_VOL_NOISE        4
#define STOCK_VOL_BASE         45      /* 波动率的静息值（基点）*/
#define STOCK_VOL_MIN          15
#define STOCK_VOL_MAX          160
#define STOCK_HOLD_PER_MONEY   1000000LL  /* 1 金钱 = 100 (0.01) × 10000 (0.0001 股) —— 换算常量：hold = 金钱 × 1e6 / price */

typedef struct { int32_t o, h, l, c; } stock_candle_t;

typedef struct {
    int32_t  price;
    int32_t  hold;
    int32_t  cost;             /* 累计买入 − 累计卖出所得；盈亏 = 市值 − cost */
    uint32_t steps;
    uint32_t rng;              /* 自带 LCG 的状态；开局由调用方用引擎 RNG 播一次种 */
    int16_t  drift;            /* 隐藏：动量（基点 / 步）*/
    int16_t  vol;              /* 隐藏：波动率（基点）*/
    uint8_t  step_in_candle;
    uint8_t  head;             /* 环形缓冲：下一根写入下标 */
    uint8_t  count;            /* 已收根数 ≤ STOCK_CANDLES */
    uint8_t  pad;
    stock_candle_t cur;        /* 未收盘那根 */
    stock_candle_t hist[STOCK_CANDLES];
} stock_state_t;

void stock_init(stock_state_t *s, uint32_t seed);      /* 开盘 100.00、清仓、清缓冲、播种 */
void stock_step(stock_state_t *s);                     /* 走一步（调用方每 STOCK_STEP_FRAMES 帧调一次）；随机数取自自带 LCG */
void stock_shock(stock_state_t *s, int permille);     /* 价格 × (1000 + permille) / 1000，钳位；留给以后的事件联动，本版无调用者 */
int  stock_visible(const stock_state_t *s, stock_candle_t out[STOCK_CANDLES]);   /* 最新 ≤16 根，旧→新，最后一根 = cur；返回根数 */

/* 交易：money 是调用方的现金（引擎 MONEY），成交就地改。返回 1 成交 / 0 拒绝（现金不够 / 没持仓）。 */
int  stock_buy (stock_state_t *s, int32_t *money);
int  stock_sell(stock_state_t *s, int32_t *money);
int32_t stock_value(const stock_state_t *s);          /* 持仓市值（金钱，向下取整）*/
int32_t stock_pnl(const stock_state_t *s);            /* 市值 − cost */

/* 蜡烛几何：把 visible 的根数铺进 (x0, y0, w, h) 的区域，最新一根永远在最右。
 * 坐标是调用方的像素单位（胶水层用 UI 层 VM 的 2× 空间）；cx/cy = 矩形中心；空槽 w = h = 0。 */
typedef struct { int16_t cx, cy, w, h; uint8_t up; } stock_rect_t;
void stock_layout(const stock_state_t *s, int x0, int y0, int w, int h,
                  stock_rect_t body[STOCK_CANDLES], stock_rect_t wick[STOCK_CANDLES]);
