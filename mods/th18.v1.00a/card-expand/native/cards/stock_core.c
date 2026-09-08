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

/* 阶段重抽：用 rnd 的一个整数散列，跟噪声用的位分开 */
static void pick_regime(stock_state_t *s, uint32_t rnd)
{
    uint32_t h = rnd * 2654435761u;
    s->regime = (uint8_t)((h >> 8) % 3u);
    s->regime_left = (uint16_t)(STOCK_REGIME_MIN + (h >> 16) % (uint32_t)(STOCK_REGIME_MAX - STOCK_REGIME_MIN + 1));
}

void stock_init(stock_state_t *s)
{
    memset(s, 0, sizeof *s);
    s->price = STOCK_PRICE_OPEN;
    candle_open(&s->cur, s->price);
    s->regime = 0;
    s->regime_left = 120;               /* 首段：平盘 30 秒，让玩家先看一会儿 */
}

/* 10 位均匀数 → [−1000, 1000] */
static int32_t uni10(uint32_t v) { return (int32_t)(((v & 0x3ffu) * 2001u) >> 10) - 1000; }

/* 占位模型（spec §3）：r（基点）= 阶段漂移 + 阶段波动 × 噪声 + 均值回归；price += price × r / 10000 */
void stock_step(stock_state_t *s, uint32_t rnd)
{
    static const int32_t drift[3] = { 0, 15, -15 };
    static const int32_t vol[3]   = { 40, 60, 60 };

    if (s->regime_left == 0) pick_regime(s, rnd);       /* 本步已用新阶段；段长 = L 步（含本步）*/
    s->regime_left--;
    if (s->regime > 2) s->regime = 0;

    int32_t noise = (uni10(rnd) + uni10(rnd >> 10) + uni10(rnd >> 20)) / 3;          /* 三个均匀数之和 / 3，近似正态，[−1000, 1000] */
    int64_t r = drift[s->regime] + (int64_t)vol[s->regime] * noise / 1000 + (STOCK_PRICE_OPEN - s->price) / 400;
    int64_t p = s->price + (int64_t)s->price * r / 10000;
    s->price = clamp_price(p);
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

/* —— 交易与几何：Task 3 / Task 4 —— */
int stock_buy(stock_state_t *s, int32_t *money) { (void)s; (void)money; return 0; }
int stock_sell(stock_state_t *s, int32_t *money) { (void)s; (void)money; return 0; }
int32_t stock_value(const stock_state_t *s) { (void)s; return 0; }
int32_t stock_pnl(const stock_state_t *s) { (void)s; return 0; }
void stock_layout(const stock_state_t *s, int x0, int y0, int w, int h,
                  stock_rect_t body[STOCK_CANDLES], stock_rect_t wick[STOCK_CANDLES])
{ (void)s; (void)x0; (void)y0; (void)w; (void)h; (void)body; (void)wick; }
