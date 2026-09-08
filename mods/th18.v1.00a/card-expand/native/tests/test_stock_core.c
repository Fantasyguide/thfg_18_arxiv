/* 股市（id 80）纯逻辑单测：行情步进 / K 线缓冲 / 交易 / 蜡烛几何。
 *   cc -std=c11 -Wall -Wextra -Werror -o t tests/test_stock_core.c cards/stock_core.c && ./t */
#include <stdio.h>
#include <string.h>

#include "../cards/stock_core.h"

static int fails;
#define CHECK(cond, ...) do { if (!(cond)) { printf("FAIL %s:%d ", __FILE__, __LINE__); \
    printf(__VA_ARGS__); printf("\n"); fails++; } } while (0)

/* 测试自己的随机序列（LCG）；引擎里换成 ce_rand() */
static uint32_t lcg_state;
static uint32_t lcg(void) { lcg_state = lcg_state * 1664525u + 1013904223u; return lcg_state; }

static int candle_ok(const stock_candle_t *k)
{
    int hi = k->o > k->c ? k->o : k->c, lo = k->o < k->c ? k->o : k->c;
    return k->h >= hi && k->l <= lo && k->h >= k->l;
}

static void test_init(void)
{
    stock_state_t s;
    memset(&s, 0xaa, sizeof s);
    stock_init(&s);
    CHECK(s.price == STOCK_PRICE_OPEN, "开盘价应为 %d，得到 %d", STOCK_PRICE_OPEN, s.price);
    CHECK(s.hold == 0 && s.cost == 0, "初始无持仓");
    CHECK(s.count == 0 && s.head == 0 && s.step_in_candle == 0, "缓冲应为空");
    CHECK(s.cur.o == STOCK_PRICE_OPEN && s.cur.h == STOCK_PRICE_OPEN && s.cur.l == STOCK_PRICE_OPEN && s.cur.c == STOCK_PRICE_OPEN,
          "当前那根四价都该是开盘价");
    CHECK(s.regime <= 2, "阶段应在 0..2");
    CHECK(s.regime_left >= STOCK_REGIME_MIN && s.regime_left <= STOCK_REGIME_MAX, "初始阶段时长 %u 越界", s.regime_left);
}

/* 同一随机序列两次跑，逐字节相同（replay 的根本）*/
static void test_deterministic(void)
{
    stock_state_t a, b;
    stock_init(&a); stock_init(&b);
    lcg_state = 7; for (int i = 0; i < 5000; ++i) stock_step(&a, lcg());
    lcg_state = 7; for (int i = 0; i < 5000; ++i) stock_step(&b, lcg());
    CHECK(memcmp(&a, &b, sizeof a) == 0, "同一序列两次结果不同");
    CHECK(a.price != STOCK_PRICE_OPEN, "5000 步之后价格居然没动");
}

/* 极端随机数长跑：价格永在钳位内，且没有整数溢出把价格弄成负数 */
static void test_clamp(void)
{
    stock_state_t s;
    stock_init(&s);
    for (int i = 0; i < 100000; ++i) {
        stock_step(&s, 0xffffffffu);
        CHECK(s.price >= STOCK_PRICE_MIN && s.price <= STOCK_PRICE_MAX, "全 1 随机数第 %d 步价格 %d 越界", i, s.price);
        if (s.price < STOCK_PRICE_MIN || s.price > STOCK_PRICE_MAX) return;
    }
    stock_init(&s);
    for (int i = 0; i < 100000; ++i) {
        stock_step(&s, 0u);
        CHECK(s.price >= STOCK_PRICE_MIN && s.price <= STOCK_PRICE_MAX, "全 0 随机数第 %d 步价格 %d 越界", i, s.price);
        if (s.price < STOCK_PRICE_MIN || s.price > STOCK_PRICE_MAX) return;
    }
    lcg_state = 99;
    stock_init(&s);
    for (int i = 0; i < 200000; ++i) {
        stock_step(&s, lcg());
        if (s.price < STOCK_PRICE_MIN || s.price > STOCK_PRICE_MAX) { CHECK(0, "随机长跑第 %d 步价格 %d 越界", i, s.price); return; }
    }
}

/* 阶段：每段 80..240 步，编号 0..2。重抽时 regime_left 从 0 跳到 L−1（L ∈ [80, 240]），
 * 两次跳变之间的步数就是段长；同号连抽也算换段（所以不看 regime 字段） */
static void test_regime(void)
{
    stock_state_t s;
    stock_init(&s);
    lcg_state = 3;
    unsigned since_pick = 0, picks = 0;
    uint16_t prev_left = s.regime_left;
    for (int i = 0; i < 50000; ++i) {
        stock_step(&s, lcg());
        CHECK(s.regime <= 2, "阶段编号 %u 越界", s.regime);
        ++since_pick;
        if (s.regime_left > prev_left) {                     /* 刚重抽 */
            if (picks > 0)                                    /* 第一段是 init 给的 120，不计 */
                CHECK(since_pick >= STOCK_REGIME_MIN && since_pick <= STOCK_REGIME_MAX, "第 %u 段长 %u 越界", picks, since_pick);
            CHECK(s.regime_left >= STOCK_REGIME_MIN - 1 && s.regime_left <= STOCK_REGIME_MAX - 1, "重抽后 regime_left=%u 越界", s.regime_left);
            since_pick = 0;
            ++picks;
        }
        prev_left = s.regime_left;
    }
    CHECK(picks >= 50000 / STOCK_REGIME_MAX - 1, "50000 步只重抽了 %u 次，太少", picks);
}

/* K 线：16 步收一根；环形 16 根；visible 给最新 ≤16 根、旧→新、最后一根 = cur */
static void test_candles(void)
{
    stock_state_t s;
    stock_candle_t v[STOCK_CANDLES];
    stock_init(&s);
    lcg_state = 11;
    for (int i = 0; i < STOCK_STEPS_PER_CANDLE - 1; ++i) stock_step(&s, lcg());
    CHECK(s.count == 0 && s.step_in_candle == STOCK_STEPS_PER_CANDLE - 1, "15 步还不该收盘");
    CHECK(stock_visible(&s, v) == 1 && v[0].o == s.cur.o, "只有当前那根时 visible 应为 1");
    stock_step(&s, lcg());
    CHECK(s.count == 1 && s.head == 1 && s.step_in_candle == 0, "第 16 步该收盘：count=%u head=%u sic=%u", s.count, s.head, s.step_in_candle);
    CHECK(s.hist[0].o == STOCK_PRICE_OPEN, "第一根开盘价应为 %d，得到 %d", STOCK_PRICE_OPEN, s.hist[0].o);
    CHECK(candle_ok(&s.hist[0]), "第一根 OHLC 不自洽");
    CHECK(s.cur.o == s.hist[0].c && s.cur.h == s.cur.o && s.cur.l == s.cur.o, "新一根的开盘价该等于上一根收盘价");
    CHECK(stock_visible(&s, v) == 2 && v[0].o == s.hist[0].o && v[1].o == s.cur.o, "visible 应给 [hist0, cur]");

    for (int i = 0; i < STOCK_STEPS_PER_CANDLE * 30; ++i) { stock_step(&s, lcg()); CHECK(candle_ok(&s.cur), "第 %d 步 cur 不自洽", i); }
    CHECK(s.count == STOCK_CANDLES, "满 16 根之后 count 应钉在 16，得到 %u", s.count);
    CHECK(s.head == (1 + 30) % STOCK_CANDLES, "head 应绕圈，得到 %u", s.head);
    for (int i = 0; i < STOCK_CANDLES; ++i) CHECK(candle_ok(&s.hist[i]), "hist[%d] 不自洽", i);
    int n = stock_visible(&s, v);
    CHECK(n == STOCK_CANDLES, "满缓冲时 visible 应为 16，得到 %d", n);
    CHECK(v[STOCK_CANDLES - 1].o == s.cur.o && v[STOCK_CANDLES - 1].c == s.cur.c, "最后一根应是 cur");
    for (int i = 1; i < STOCK_CANDLES; ++i) CHECK(v[i].o == v[i - 1].c, "第 %d 根开盘价应接上一根收盘价（顺序错了）", i);
}

static void test_shock(void)
{
    stock_state_t s;
    stock_init(&s);
    stock_shock(&s, 200);
    CHECK(s.price == 12000, "+20％ 冲击后应为 12000，得到 %d", s.price);
    stock_shock(&s, -500);
    CHECK(s.price == 6000, "−50％ 冲击后应为 6000，得到 %d", s.price);
    for (int i = 0; i < 40; ++i) stock_shock(&s, 900);
    CHECK(s.price == STOCK_PRICE_MAX, "连续暴涨应钳在上限，得到 %d", s.price);
    for (int i = 0; i < 40; ++i) stock_shock(&s, -900);
    CHECK(s.price == STOCK_PRICE_MIN, "连续暴跌应钳在下限，得到 %d", s.price);
    CHECK(s.cur.h == STOCK_PRICE_MAX && s.cur.l == STOCK_PRICE_MIN, "冲击也要记进当前那根的高低价");
}

/* 买：50 金钱换 50e6 / price 单位持股；钱不够拒绝 */
static void test_buy(void)
{
    stock_state_t s;
    stock_init(&s);
    int32_t money = 120;
    CHECK(stock_buy(&s, &money) == 1 && money == 70, "第一次买应成交、剩 70，得到 %d", money);
    CHECK(s.hold == 50 * STOCK_HOLD_PER_MONEY / STOCK_PRICE_OPEN, "100.00 买 50 应得 0.5 股 = %lld 单位，得到 %d",
          (long long)(50 * STOCK_HOLD_PER_MONEY / STOCK_PRICE_OPEN), s.hold);
    CHECK(s.cost == 50, "成本应为 50");
    CHECK(stock_buy(&s, &money) == 1 && money == 20, "第二次买应成交、剩 20，得到 %d", money);
    CHECK(stock_buy(&s, &money) == 0 && money == 20, "20 块买不起，不该扣钱");
    money = STOCK_LOT;
    CHECK(stock_buy(&s, &money) == 1 && money == 0, "正好 50 应能买");
    money = -5;
    CHECK(stock_buy(&s, &money) == 0, "负数现金不该成交");
}

/* 卖：50 金钱等值；不足全卖；没持仓拒绝；现金永不减 */
static void test_sell(void)
{
    stock_state_t s;
    stock_init(&s);
    int32_t money = 0;
    CHECK(stock_sell(&s, &money) == 0 && money == 0, "空仓不该卖");
    money = 200;
    stock_buy(&s, &money); stock_buy(&s, &money); stock_buy(&s, &money);      /* 150 进场，1.5 股 */
    CHECK(money == 50 && s.hold == 15000, "三次买后应剩 50、持 1.5 股，得到 %d / %d", money, s.hold);
    s.price = 20000;                                                        /* 翻倍 */
    CHECK(stock_sell(&s, &money) == 1, "有持仓应能卖");
    CHECK(money == 100, "200.00 卖 50 块等值：现金 50 → 100，得到 %d", money);
    CHECK(s.hold == 15000 - 2500, "应卖掉 0.25 股，剩 %d", s.hold);
    CHECK(s.cost == 150 - 50, "成本应减去所得");
    s.price = 200000;                                                       /* 2000.00：50 块只够 0.025 股 */
    stock_sell(&s, &money);
    CHECK(s.hold == 12500 - 250 && money == 150, "高价卖：hold=%d money=%d", s.hold, money);
    s.price = 10000; s.hold = 100;                                          /* 只剩 0.01 股 = 1 块钱 */
    int32_t before = money;
    CHECK(stock_sell(&s, &money) == 1 && s.hold == 0 && money == before + 1, "不足 50 应全卖：hold=%d money=%d", s.hold, money);
    CHECK(stock_sell(&s, &money) == 0, "卖空了再卖应拒绝");
}

/* 一次买卖来回，任意价位损耗 ≤ 1 金钱（零头 = 手续费）；现金永不为负、持股永不为负 */
static void test_round_trip(void)
{
    static const int32_t prices[] = { STOCK_PRICE_MIN, 1234, 9999, STOCK_PRICE_OPEN, 12345, 99999, 123456, 500000, STOCK_PRICE_MAX };
    for (unsigned i = 0; i < sizeof prices / sizeof prices[0]; ++i) {
        stock_state_t s;
        stock_init(&s);
        s.price = prices[i];
        int32_t money = 1000;
        CHECK(stock_buy(&s, &money) == 1, "价位 %d 买入失败", prices[i]);
        CHECK(stock_sell(&s, &money) == 1, "价位 %d 卖出失败", prices[i]);
        CHECK(s.hold == 0, "价位 %d 一买一卖后应清仓，剩 %d", prices[i], s.hold);
        CHECK(money >= 999 && money <= 1000, "价位 %d 来回损耗应 ≤ 1：money=%d", prices[i], money);
        CHECK(s.cost == 1000 - money, "价位 %d 成本应等于净支出", prices[i]);
    }
    /* 随机买卖长跑 */
    stock_state_t s;
    stock_init(&s);
    lcg_state = 5;
    int32_t money = 500;
    for (int i = 0; i < 20000; ++i) {
        uint32_t r = lcg();
        stock_step(&s, r);
        if ((r >> 30) == 0) stock_buy(&s, &money);
        else if ((r >> 30) == 1) stock_sell(&s, &money);
        if (money < 0 || s.hold < 0) { CHECK(0, "第 %d 步 money=%d hold=%d 为负", i, money, s.hold); return; }
    }
}

static void test_value_pnl(void)
{
    stock_state_t s;
    stock_init(&s);
    CHECK(stock_value(&s) == 0 && stock_pnl(&s) == 0, "空仓市值 / 盈亏为 0");
    s.hold = 10000; s.price = 12345; s.cost = 100;                          /* 1 股 × 123.45 */
    CHECK(stock_value(&s) == 123, "市值应为 123，得到 %d", stock_value(&s));
    CHECK(stock_pnl(&s) == 23, "盈亏应为 +23，得到 %d", stock_pnl(&s));
    s.price = 5000;
    CHECK(stock_pnl(&s) == -50, "跌到 50.00 盈亏应为 −50，得到 %d", stock_pnl(&s));
    s.hold = 100000000; s.price = STOCK_PRICE_MAX;                          /* 1 万股 × 9999：int64 中间量 */
    CHECK(stock_value(&s) == 99990000, "大持仓市值应为 99990000，得到 %d", stock_value(&s));
}

int main(void)
{
    test_init();
    test_deterministic();
    test_clamp();
    test_regime();
    test_candles();
    test_shock();
    test_buy();
    test_sell();
    test_round_trip();
    test_value_pnl();
    printf("test_stock_core: %s\n", fails ? "FAILED" : "OK");
    return fails ? 1 : 0;
}
