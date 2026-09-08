/* stock_core.h —— 股市（id 80）的纯逻辑：行情模拟 / K 线环形缓冲 / 交易 / 蜡烛几何。
 * 纯 C11、无引擎依赖、全整数、无浮点；随机数由调用方注入（引擎侧 ce_rand() = REPLAY_SAFE_RNG）。
 * 主机单测 tests/test_stock_core.c。设计：docs/superpowers/specs/2026-09-08-stock-market-design.md。
 *
 * 单位：价格 0.01 金钱（10000 = 100.00）；持股 0.0001 股；成本 / 市值 / 盈亏 = 金钱。
 * 行情模型是**占位**：接口（stock_init / stock_step / stock_shock）固定，内部以后换更科学的。 */
#pragma once
#include <stdint.h>

#define STOCK_CANDLES          16      /* 环形缓冲根数 = 屏幕上的根数 */
#define STOCK_STEP_FRAMES      15      /* 每 15 帧走一步（1 秒 4 次）*/
#define STOCK_STEPS_PER_CANDLE 16      /* 16 步 = 240 帧 = 4 秒一根 */
#define STOCK_LOT              50      /* 每次买 / 卖的金钱数 */
#define STOCK_PRICE_OPEN       10000
#define STOCK_PRICE_MIN        1000
#define STOCK_PRICE_MAX        999900
#define STOCK_REGIME_MIN       80      /* 阶段时长（步）：20 s */
#define STOCK_REGIME_MAX       240     /*                 60 s */
#define STOCK_HOLD_PER_MONEY   1000000LL  /* 1 金钱 = 100 (0.01) × 10000 (0.0001 股) —— 换算常量：hold = 金钱 × 1e6 / price */

typedef struct { int32_t o, h, l, c; } stock_candle_t;

typedef struct {
    int32_t  price;
    int32_t  hold;
    int32_t  cost;             /* 累计买入 − 累计卖出所得；盈亏 = 市值 − cost */
    uint32_t steps;
    uint8_t  regime;           /* 0 平 / 1 牛 / 2 熊 */
    uint8_t  step_in_candle;
    uint16_t regime_left;
    stock_candle_t cur;        /* 未收盘那根 */
    stock_candle_t hist[STOCK_CANDLES];
    uint8_t  head;             /* 下一根写入下标 */
    uint8_t  count;            /* 已收根数 ≤ STOCK_CANDLES */
} stock_state_t;

void stock_init(stock_state_t *s);                    /* 开盘 100.00、清仓、清缓冲 */
void stock_step(stock_state_t *s, uint32_t rnd);      /* 走一步（调用方每 STOCK_STEP_FRAMES 帧调一次）*/
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
