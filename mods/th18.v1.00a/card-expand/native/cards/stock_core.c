/* stock_core.c —— 股市（id 80）纯逻辑。见 stock_core.h。 */
#include "stock_core.h"

#include <string.h>

static int32_t clamp_price(int64_t p)
{
    if (p < STOCK_PRICE_MIN) return STOCK_PRICE_MIN;
    if (p > STOCK_PRICE_MAX) return STOCK_PRICE_MAX;
    return (int32_t)p;
}

static void candle_open(stock_candle_t *k, int32_t p) { k->o = k->h = k->l = k->c = p; }

static void candle_track(stock_candle_t *k, int32_t p)
{
    if (p > k->h) k->h = p;
    if (p < k->l) k->l = p;
    k->c = p;
}

/* 自带 LCG（Numerical Recipes 常数）。只为「不去动引擎那只共用 RNG」而存在，见头文件。 */
static uint32_t lcg(stock_state_t *s)
{
    s->rng = s->rng * 1664525u + 1013904223u;
    return s->rng;
}

/* 取 v 的低 10 位，映射到 [-amp, amp] 的均匀整数 */
static int32_t uni(uint32_t v, int32_t amp)
{
    return (int32_t)(((v & 0x3ffu) * (uint32_t)(2 * amp + 1)) >> 10) - amp;
}

static int32_t clampi(int32_t x, int32_t lo, int32_t hi)
{
    return x < lo ? lo : x > hi ? hi : x;
}

void stock_init(stock_state_t *s, uint32_t seed)
{
    memset(s, 0, sizeof *s);
    s->price = STOCK_PRICE_OPEN;
    s->rng   = seed ? seed : 1u;          /* 0 是 LCG 的合法状态，但换成 1 省得看着像没播种 */
    s->drift = 0;
    s->vol   = STOCK_VOL_BASE;
    candle_open(&s->cur, s->price);
}

/* 一步 = STOCK_STEP_FRAMES 帧。两个隐藏状态各自做均值回归，价格再按它们走。 */
void stock_step(stock_state_t *s)
{
    uint32_t r = lcg(s);

    /* 估值拉力：价格越高，漂移的回归目标越负 —— 贵了就该跌，便宜了就该涨 */
    int32_t target = clampi(-(s->price - STOCK_PRICE_OPEN) * STOCK_VALUATION_PULL / STOCK_PRICE_OPEN,
                            -STOCK_TARGET_CAP, STOCK_TARGET_CAP);
    int32_t drift  = s->drift + (target - s->drift) / STOCK_DRIFT_TAU + uni(r, STOCK_DRIFT_NOISE);
    s->drift = (int16_t)clampi(drift, -STOCK_DRIFT_CAP, STOCK_DRIFT_CAP);

    int32_t vol = s->vol + (STOCK_VOL_BASE - s->vol) / STOCK_VOL_TAU + uni(r >> 10, STOCK_VOL_NOISE);
    s->vol = (int16_t)clampi(vol, STOCK_VOL_MIN, STOCK_VOL_MAX);

    /* 两个均匀数相加近似正态；用与上面不重叠的位 */
    int32_t noise = (uni(r >> 20, 1000) + uni(lcg(s), 1000)) / 2;
    int64_t rr = s->drift + (int64_t)s->vol * noise / 1000;
    s->price = clamp_price(s->price + (int64_t)s->price * rr / 10000);

    candle_track(&s->cur, s->price);
    s->steps++;

    if (++s->step_in_candle >= STOCK_STEPS_PER_CANDLE) {
        s->step_in_candle = 0;
        s->hist[s->head] = s->cur;
        s->head = (uint8_t)((s->head + 1) % STOCK_CANDLES);
        if (s->count < STOCK_CANDLES) s->count++;
        candle_open(&s->cur, s->price);
    }
}

void stock_shock(stock_state_t *s, int permille)
{
    int64_t p = (int64_t)s->price * (1000 + permille) / 1000;
    s->price = clamp_price(p);
    candle_track(&s->cur, s->price);
}

int stock_visible(const stock_state_t *s, stock_candle_t out[STOCK_CANDLES])
{
    int closed = s->count < STOCK_CANDLES - 1 ? s->count : STOCK_CANDLES - 1;   /* 最多 15 根已收 + cur */
    int n = 0;
    for (int i = closed; i >= 1; --i) {
        int idx = (s->head - i + STOCK_CANDLES) % STOCK_CANDLES;              /* head−1 是最新的已收 */
        out[n++] = s->hist[idx];
    }
    out[n++] = s->cur;
    return n;
}

/* 买：固定 STOCK_LOT 金钱 → 持股 += 50 × 1e6 / price（向下取整，零头 = 手续费）。 */
int stock_buy(stock_state_t *s, int32_t *money)
{
    if (*money < STOCK_LOT) return 0;
    int64_t q = (int64_t)STOCK_LOT * STOCK_HOLD_PER_MONEY / s->price;
    *money -= STOCK_LOT;
    s->hold += (int32_t)q;
    s->cost += STOCK_LOT;
    return 1;
}

/* 卖：STOCK_LOT 金钱等值的持股，不足全卖；所得向下取整。
 * M5：所得向下取整到 0 时（仓位薄到几个持股单位、又赶上低价）算不成交，别改状态——
 * 否则卡会当真卖了一笔在播音效、日志报「卖 50」，而 money 其实分毫未动，误导对拍手感的人。
 * 这跟空仓拒绝走的是同一条路：调用方看到 0 就照抄"no holding"那句拒绝词，够用，不必再分错误码。 */
int stock_sell(stock_state_t *s, int32_t *money)
{
    if (s->hold <= 0) return 0;
    int64_t q = (int64_t)STOCK_LOT * STOCK_HOLD_PER_MONEY / s->price;
    if (q > s->hold) q = s->hold;
    int64_t got = q * s->price / STOCK_HOLD_PER_MONEY;
    if (got <= 0) return 0;
    s->hold -= (int32_t)q;
    *money += (int32_t)got;
    s->cost -= (int32_t)got;
    return 1;
}

int32_t stock_value(const stock_state_t *s)
{
    return (int32_t)((int64_t)s->hold * s->price / STOCK_HOLD_PER_MONEY);
}

int32_t stock_pnl(const stock_state_t *s)
{
    return stock_value(s) - s->cost;
}
static int32_t imax(int32_t a, int32_t b) { return a > b ? a : b; }
static int32_t imin(int32_t a, int32_t b) { return a < b ? a : b; }

/* 价 → y（价高在上）；hi > lo 由调用方保证 */
static int price_y(int32_t p, int32_t lo, int32_t hi, int y0, int h)
{
    return y0 + h - (int)((int64_t)(p - lo) * h / (hi - lo));
}

void stock_layout(const stock_state_t *s, int x0, int y0, int w, int h,
                  stock_rect_t body[STOCK_CANDLES], stock_rect_t wick[STOCK_CANDLES])
{
    stock_candle_t v[STOCK_CANDLES];
    int n = stock_visible(s, v);
    int pitch = w / STOCK_CANDLES;
    int bw = pitch * 7 / 13;
    int32_t lo = v[0].l, hi = v[0].h;

    memset(body, 0, sizeof(stock_rect_t) * STOCK_CANDLES);
    memset(wick, 0, sizeof(stock_rect_t) * STOCK_CANDLES);
    for (int i = 1; i < n; ++i) { lo = imin(lo, v[i].l); hi = imax(hi, v[i].h); }
    if (hi <= lo) hi = lo + 1;                          /* 四价全同（刚开盘）：别除零 */

    for (int i = 0; i < n; ++i) {
        int slot = STOCK_CANDLES - n + i;               /* 最新的永远在最右 */
        int cx = x0 + pitch * slot + pitch / 2;
        int top = price_y(imax(v[i].o, v[i].c), lo, hi, y0, h);
        int bot = price_y(imin(v[i].o, v[i].c), lo, hi, y0, h);
        int bh = imax(bot - top, 2);
        int wt = price_y(v[i].h, lo, hi, y0, h);
        int wb = price_y(v[i].l, lo, hi, y0, h);
        int wh = imax(wb - wt, 1);
        body[slot].cx = (int16_t)cx; body[slot].cy = (int16_t)((top + bot) / 2);
        body[slot].w = (int16_t)bw;  body[slot].h = (int16_t)bh;
        body[slot].up = v[i].c >= v[i].o;
        wick[slot].cx = (int16_t)cx; wick[slot].cy = (int16_t)((wt + wb) / 2);
        wick[slot].w = 2;            wick[slot].h = (int16_t)wh;
        wick[slot].up = body[slot].up;
    }
}
