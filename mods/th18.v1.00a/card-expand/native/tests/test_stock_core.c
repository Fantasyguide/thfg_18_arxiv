/* 股市（id 80）纯逻辑单测：行情步进 / K 线缓冲 / 交易 / 蜡烛几何。
 *   cc -std=c11 -Wall -Wextra -Werror -o t tests/test_stock_core.c cards/stock_core.c && ./t */
#include <stdio.h>
#include <string.h>

#include "../cards/stock_core.h"

static int fails;
#define CHECK(cond, ...) do { if (!(cond)) { printf("FAIL %s:%d ", __FILE__, __LINE__); \
    printf(__VA_ARGS__); printf("\n"); fails++; } } while (0)

/* 测试自己的随机序列（LCG）；引擎里换成 ce_rand() */

static int candle_ok(const stock_candle_t *k)
{
    int hi = k->o > k->c ? k->o : k->c, lo = k->o < k->c ? k->o : k->c;
    return k->h >= hi && k->l <= lo && k->h >= k->l;
}

static void test_init(void)
{
    stock_state_t s;
    memset(&s, 0xaa, sizeof s);
    stock_init(&s, 12345u);
    CHECK(s.price == STOCK_PRICE_OPEN, "开盘价应为 %d，得到 %d", STOCK_PRICE_OPEN, s.price);
    CHECK(s.hold == 0 && s.cost == 0, "初始无持仓");
    CHECK(s.count == 0 && s.head == 0 && s.step_in_candle == 0, "缓冲应为空");
    CHECK(s.cur.o == STOCK_PRICE_OPEN && s.cur.h == STOCK_PRICE_OPEN && s.cur.l == STOCK_PRICE_OPEN && s.cur.c == STOCK_PRICE_OPEN,
          "当前那根四价都该是开盘价");
    CHECK(s.drift == 0, "初始漂移应为 0，得到 %d", s.drift);
    CHECK(s.vol == STOCK_VOL_BASE, "初始波动率应是静息值 %d，得到 %d", (int)STOCK_VOL_BASE, s.vol);
    CHECK(s.rng != 0, "必须播过种（0 会让 LCG 退化成常数）");
    stock_state_t z; stock_init(&z, 0u);
    CHECK(z.rng != 0, "种子给 0 时也要落到一个非零状态");
}

/* 同一随机序列两次跑，逐字节相同（replay 的根本）*/
static void test_deterministic(void)
{
    stock_state_t a, b;
    stock_init(&a, 12345u); stock_init(&b, 12345u);
    for (int i = 0; i < 5000; ++i) stock_step(&a);
    for (int i = 0; i < 5000; ++i) stock_step(&b);
    CHECK(memcmp(&a, &b, sizeof a) == 0, "同一序列两次结果不同");
    CHECK(a.price != STOCK_PRICE_OPEN, "5000 步之后价格居然没动");
}

/* 极端随机数长跑：价格永在钳位内，且没有整数溢出把价格弄成负数 */
static void test_clamp(void)
{
    stock_state_t s;
    stock_init(&s, 12345u);
    for (int i = 0; i < 100000; ++i) {
        stock_step(&s);
        CHECK(s.price >= STOCK_PRICE_MIN && s.price <= STOCK_PRICE_MAX, "全 1 随机数第 %d 步价格 %d 越界", i, s.price);
        if (s.price < STOCK_PRICE_MIN || s.price > STOCK_PRICE_MAX) return;
    }
    stock_init(&s, 12345u);
    for (int i = 0; i < 100000; ++i) {
        stock_step(&s);
        CHECK(s.price >= STOCK_PRICE_MIN && s.price <= STOCK_PRICE_MAX, "全 0 随机数第 %d 步价格 %d 越界", i, s.price);
        if (s.price < STOCK_PRICE_MIN || s.price > STOCK_PRICE_MAX) return;
    }
    stock_init(&s, 12345u);
    for (int i = 0; i < 200000; ++i) {
        stock_step(&s);
        if (s.price < STOCK_PRICE_MIN || s.price > STOCK_PRICE_MAX) { CHECK(0, "随机长跑第 %d 步价格 %d 越界", i, s.price); return; }
    }
}

/* 动量：漂移是均值回归过程 ⇒ 相邻两步的价格变化正相关。这条是「看图择时算技能」的根据，
 * 也是与上一版三档 regime 模型最本质的区别。同时钉住两个隐藏量始终在各自的界内。 */
static void test_momentum(void)
{
    stock_state_t s;
    long long n = 0, sxy = 0, sx = 0, sy = 0, sxx = 0, syy = 0;
    int32_t prev_price, prev_d = 0;
    stock_init(&s, 4242u);
    prev_price = s.price;
    for (int i = 0; i < 60000; ++i) {
        stock_step(&s);
        int32_t d = s.price - prev_price;
        prev_price = s.price;
        CHECK(s.drift >= -STOCK_DRIFT_CAP && s.drift <= STOCK_DRIFT_CAP, "漂移 %d 越界", s.drift);
        CHECK(s.vol >= STOCK_VOL_MIN && s.vol <= STOCK_VOL_MAX, "波动率 %d 越界", s.vol);
        if (i) { sxy += (long long)prev_d * d; sx += prev_d; sy += d; sxx += (long long)prev_d * prev_d; syy += (long long)d * d; ++n; }
        prev_d = d;
    }
    /* 相关系数 > 0 即可（实测约 0.2）。这里只用整数判号，避免浮点。 */
    long long num = n * sxy - sx * sy;
    CHECK(num > 0, "相邻步价格变化应正相关（动量），得到分子 %lld", num);
    CHECK(sxx > 0 && syy > 0, "价格必须真的在动");
}

/* 波动率聚集：高波动期之后仍是高波动期。用「前 100 步的绝对涨跌之和」分组比较后 100 步。 */
static void test_vol_clustering(void)
{
    stock_state_t s;
    long long calm = 0, wild = 0; int nc = 0, nw = 0;
    stock_init(&s, 777u);
    for (int blk = 0; blk < 600; ++blk) {
        int32_t p0 = s.price; long long a = 0;
        for (int i = 0; i < 100; ++i) { int32_t q = s.price; stock_step(&s); a += s.price > q ? s.price - q : q - s.price; }
        int wild_now = s.vol > STOCK_VOL_BASE;
        int32_t p1 = s.price; long long b = 0;
        for (int i = 0; i < 100; ++i) { int32_t q = s.price; stock_step(&s); b += s.price > q ? s.price - q : q - s.price; }
        (void)p0; (void)p1; (void)a;
        if (wild_now) { wild += b; ++nw; } else { calm += b; ++nc; }
    }
    CHECK(nc > 50 && nw > 50, "两种状态都要有足够样本（calm %d / wild %d）", nc, nw);
    CHECK(wild / nw > calm / nc, "高波动期之后的波动应更大：wild %lld vs calm %lld", wild / nw, calm / nc);
}

/* K 线：16 步收一根；环形 16 根；visible 给最新 ≤16 根、旧→新、最后一根 = cur */
static void test_candles(void)
{
    stock_state_t s;
    stock_candle_t v[STOCK_CANDLES];
    stock_init(&s, 12345u);
    for (int i = 0; i < STOCK_STEPS_PER_CANDLE - 1; ++i) stock_step(&s);
    CHECK(s.count == 0 && s.step_in_candle == STOCK_STEPS_PER_CANDLE - 1, "15 步还不该收盘");
    CHECK(stock_visible(&s, v) == 1 && v[0].o == s.cur.o, "只有当前那根时 visible 应为 1");
    stock_step(&s);
    CHECK(s.count == 1 && s.head == 1 && s.step_in_candle == 0, "第 16 步该收盘：count=%u head=%u sic=%u", s.count, s.head, s.step_in_candle);
    CHECK(s.hist[0].o == STOCK_PRICE_OPEN, "第一根开盘价应为 %d，得到 %d", STOCK_PRICE_OPEN, s.hist[0].o);
    CHECK(candle_ok(&s.hist[0]), "第一根 OHLC 不自洽");
    CHECK(s.cur.o == s.hist[0].c && s.cur.h == s.cur.o && s.cur.l == s.cur.o, "新一根的开盘价该等于上一根收盘价");
    CHECK(stock_visible(&s, v) == 2 && v[0].o == s.hist[0].o && v[1].o == s.cur.o, "visible 应给 [hist0, cur]");

    for (int i = 0; i < STOCK_STEPS_PER_CANDLE * 30; ++i) { stock_step(&s); CHECK(candle_ok(&s.cur), "第 %d 步 cur 不自洽", i); }
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
    stock_init(&s, 12345u);
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

/* 买：一次一股，花掉 price/100 金钱 */
static void test_buy(void)
{
    stock_state_t s;
    stock_init(&s, 1u);
    int32_t money = 250;
    CHECK(stock_share_price(&s) == 100, "100.00 一股应花 100 金，得到 %d", stock_share_price(&s));
    CHECK(stock_buy(&s, &money) == 1 && money == 150 && s.hold == 1, "第一股：money=%d hold=%d", money, s.hold);
    CHECK(stock_buy(&s, &money) == 1 && money == 50 && s.hold == 2, "第二股：money=%d hold=%d", money, s.hold);
    CHECK(stock_buy(&s, &money) == 0 && money == 50 && s.hold == 2, "钱不够一股就不该成交");
    CHECK(s.cost == 200, "成本应为 200，得到 %d", s.cost);
    s.price = 4000;                                  /* 跌到 40.00，同样的钱能买了 */
    CHECK(stock_buy(&s, &money) == 1 && money == 10 && s.hold == 3, "便宜时能买：money=%d hold=%d", money, s.hold);
    money = -5;
    CHECK(stock_buy(&s, &money) == 0, "负数现金不该成交");
}

/* 卖：一次一股；没持仓拒绝；同价买回卖出正好归零（一股一价，买卖同式）*/
static void test_sell(void)
{
    stock_state_t s;
    stock_init(&s, 2u);
    int32_t money = 0;
    CHECK(stock_sell(&s, &money) == 0 && money == 0, "空仓不该卖");
    money = 1000;
    stock_buy(&s, &money); stock_buy(&s, &money);     /* 100.00 × 2 股 */
    CHECK(money == 800 && s.hold == 2, "两股之后 money=%d hold=%d", money, s.hold);
    s.price = 25000;                                  /* 涨到 250.00 */
    CHECK(stock_sell(&s, &money) == 1 && money == 1050 && s.hold == 1, "卖一股：money=%d hold=%d", money, s.hold);
    CHECK(stock_sell(&s, &money) == 1 && money == 1300 && s.hold == 0, "卖光：money=%d hold=%d", money, s.hold);
    CHECK(stock_sell(&s, &money) == 0, "卖光了再卖应拒绝");
    CHECK(s.cost == 200 - 500, "成本应被卖出所得抵成 %d，得到 %d", 200 - 500, s.cost);
}

/* 同价来回：任意价位都正好不赚不亏（这是「一股一价、买卖同式」的直接推论）*/
static void test_round_trip(void)
{
    static const int32_t prices[] = { STOCK_PRICE_MIN, 1234, 9999, STOCK_PRICE_OPEN, 12345, 99999, STOCK_PRICE_MAX };
    for (unsigned i = 0; i < sizeof prices / sizeof prices[0]; ++i) {
        stock_state_t s;
        stock_init(&s, 3u);
        s.price = prices[i];
        int32_t money = 2000, before = money;
        CHECK(stock_buy(&s, &money) == 1, "价位 %d 买入失败", prices[i]);
        CHECK(stock_sell(&s, &money) == 1, "价位 %d 卖出失败", prices[i]);
        CHECK(s.hold == 0, "价位 %d 应清仓", prices[i]);
        CHECK(money == before, "价位 %d 同价来回应正好归零，得到 %d", prices[i], money - before);
        CHECK(s.cost == 0, "价位 %d 成本应归零，得到 %d", prices[i], s.cost);
    }
    /* 随机买卖长跑：钱与股数都不许为负 */
    stock_state_t s;
    stock_init(&s, 5u);
    int32_t money = 500;
    for (int i = 0; i < 20000; ++i) {
        stock_step(&s);
        if ((i & 3) == 0) stock_buy(&s, &money);
        else if ((i & 3) == 1) stock_sell(&s, &money);
        if (money < 0 || s.hold < 0) { CHECK(0, "第 %d 步 money=%d hold=%d 为负", i, money, s.hold); return; }
    }
}

static void test_value_pnl(void)
{
    stock_state_t s;
    stock_init(&s, 12345u);
    CHECK(stock_value(&s) == 0 && stock_pnl(&s) == 0, "空仓市值 / 盈亏为 0");
    s.hold = 1; s.price = 12345; s.cost = 100;                              /* 1 股 × 123.45 */
    CHECK(stock_value(&s) == 123, "市值应为 123，得到 %d", stock_value(&s));
    CHECK(stock_pnl(&s) == 23, "盈亏应为 +23，得到 %d", stock_pnl(&s));
    s.price = 5000;
    CHECK(stock_pnl(&s) == -50, "跌到 50.00 盈亏应为 −50，得到 %d", stock_pnl(&s));
    s.hold = 100000; s.price = STOCK_PRICE_MAX;                             /* 10 万股 × 1000.00：int64 中间量 */
    CHECK(stock_value(&s) == 100000000, "大持仓市值应为 100000000，得到 %d", stock_value(&s));
}

static void test_layout(void)
{
    stock_state_t s;
    stock_rect_t body[STOCK_CANDLES], wick[STOCK_CANDLES];
    stock_init(&s, 12345u);
    /* 手工造两根：hist[0] = 涨 (100 → 120, 高 130 低 90)，cur = 跌 (120 → 110, 高 125 低 100) */
    s.hist[0].o = 10000; s.hist[0].c = 12000; s.hist[0].h = 13000; s.hist[0].l = 9000;
    s.head = 1; s.count = 1;
    s.cur.o = 12000; s.cur.c = 11000; s.cur.h = 12500; s.cur.l = 10000;
    s.price = 11000;

    stock_layout(&s, 848, 776, 416, 96, body, wick);       /* pitch 26；价区 9000..13000 → 96 px，24 px / 1000 */
    for (int i = 0; i < STOCK_CANDLES - 2; ++i) CHECK(body[i].w == 0 && body[i].h == 0 && wick[i].w == 0 && wick[i].h == 0, "槽 %d 应为空", i);

    const stock_rect_t *b0 = &body[14], *w0 = &wick[14], *b1 = &body[15], *w1 = &wick[15];
    CHECK(b0->cx == 848 + 26 * 14 + 13 && b1->cx == 848 + 26 * 15 + 13, "中心 x 应按 pitch 排：%d / %d", b0->cx, b1->cx);
    CHECK(w0->cx == b0->cx && w1->cx == b1->cx, "影线与实体同一列");
    CHECK(b0->w == 14 && w0->w == 2, "实体宽 14 / 影线宽 2，得到 %d / %d", b0->w, w0->w);
    CHECK(b0->up == 1 && b1->up == 0, "第一根涨、第二根跌：%u / %u", b0->up, b1->up);
    /* 第一根：开 100 → y 776+96−24 = 848；收 120 → 776+96−72 = 800；实体高 48、中心 824 */
    CHECK(b0->h == 48 && b0->cy == 824, "第一根实体 h=%d cy=%d，应为 48 / 824", b0->h, b0->cy);
    /* 第一根影线：高 130 → 776；低 90 → 872；高 96、中心 824 */
    CHECK(w0->h == 96 && w0->cy == 824, "第一根影线 h=%d cy=%d，应为 96 / 824", w0->h, w0->cy);
    /* 第二根：开 120 → 800；收 110 → 824；实体高 24、中心 812；影线 125 → 788、100 → 848：高 60、中心 818 */
    CHECK(b1->h == 24 && b1->cy == 812, "第二根实体 h=%d cy=%d，应为 24 / 812", b1->h, b1->cy);
    CHECK(w1->h == 60 && w1->cy == 818, "第二根影线 h=%d cy=%d，应为 60 / 818", w1->h, w1->cy);

    /* 十字星：开 = 收，实体最小 2；只有一根、四价相同（价区退化）也不能除零 */
    stock_init(&s, 12345u);
    stock_layout(&s, 848, 776, 416, 96, body, wick);
    CHECK(body[15].h == 2 && wick[15].h >= 1 && body[15].w == 14, "退化蜡烛：h=%d wick=%d", body[15].h, wick[15].h);
    CHECK(body[15].cy >= 776 && body[15].cy <= 776 + 96, "退化蜡烛应落在价区内，cy=%d", body[15].cy);
    for (int i = 0; i < STOCK_CANDLES - 1; ++i) CHECK(body[i].w == 0, "只有一根时其它槽应为空");

    /* 满缓冲：16 槽全有、最右是 cur、全部在区域内 */
    for (int i = 0; i < STOCK_STEPS_PER_CANDLE * 40; ++i) stock_step(&s);
    stock_layout(&s, 848, 776, 416, 96, body, wick);
    for (int i = 0; i < STOCK_CANDLES; ++i) {
        CHECK(body[i].w == 14 && body[i].h >= 2 && wick[i].h >= 1, "槽 %d 应有蜡烛", i);
        CHECK(wick[i].cy - wick[i].h / 2 >= 776 - 1 && wick[i].cy + wick[i].h / 2 <= 776 + 96 + 1, "槽 %d 影线越界", i);
        CHECK(body[i].cx >= 848 && body[i].cx <= 848 + 416, "槽 %d x 越界", i);
    }
    CHECK(body[15].up == (s.cur.c >= s.cur.o), "最右一根应是 cur");
}


/* 买入均价（回本线画在哪）。cost 被卖出所得抵成非正数时不画 —— 那笔仓位已经稳赚。 */
static void test_basis(void)
{
    stock_state_t s;
    int32_t money = 1000;
    stock_init(&s, 1u);
    CHECK(stock_basis(&s) == 0, "空仓不该有均价");

    s.price = 10000;                       /* 100.00 买一股 */
    stock_buy(&s, &money);
    CHECK(stock_basis(&s) == 10000, "单股买入的均价就是成交价，得到 %d", stock_basis(&s));

    s.price = 20000;                       /* 200.00 再买一股 ⇒ 均价 = (100+200)/2 = 150.00 */
    stock_buy(&s, &money);
    CHECK(stock_basis(&s) == 15000, "两股均价应为 15000，得到 %d", stock_basis(&s));

    /* 涨到 600.00 卖一股：一股就把两股的成本全收回来了 */
    s.price = 60000;
    stock_sell(&s, &money);
    CHECK(s.hold == 1, "还剩一股，得到 %d", s.hold);
    CHECK(s.cost <= 0, "卖回超过成本之后 cost 应 ≤ 0，得到 %d", s.cost);
    CHECK(stock_basis(&s) == 0, "成本已收回时不画回本线");
}

/* 价格 → y：与 stock_layout 用同一套映射；越界钳到边并报告越界 */
static void test_price_row(void)
{
    stock_state_t s;
    stock_rect_t body[STOCK_CANDLES], wick[STOCK_CANDLES];
    int32_t lo, hi; int y;
    stock_init(&s, 9u);
    s.hist[0].o = 10000; s.hist[0].c = 12000; s.hist[0].h = 13000; s.hist[0].l = 9000;
    s.head = 1; s.count = 1;
    s.cur.o = 12000; s.cur.c = 11000; s.cur.h = 12500; s.cur.l = 10000;
    stock_range(&s, &lo, &hi);
    CHECK(lo == 9000 && hi == 13000, "视野区间应是 9000..13000，得到 %d..%d", lo, hi);

    stock_layout(&s, 848, 776, 416, 96, body, wick);
    CHECK(stock_price_row(10000, lo, hi, 776, 96, &y) == 1, "10000 在区间内");
    CHECK(y == 848, "10000 应映射到 y=848（与蜡烛实体同一套），得到 %d", y);
    CHECK(stock_price_row(13000, lo, hi, 776, 96, &y) == 1 && y == 776, "最高价贴顶，得到 %d", y);
    CHECK(stock_price_row(9000, lo, hi, 776, 96, &y) == 1 && y == 872, "最低价贴底，得到 %d", y);
    CHECK(stock_price_row(50000, lo, hi, 776, 96, &y) == 0 && y == 776, "越上界应报 0 并钳到顶");
    CHECK(stock_price_row(100, lo, hi, 776, 96, &y) == 0 && y == 872, "越下界应报 0 并钳到底");
}

int main(void)
{
    test_init();
    test_deterministic();
    test_clamp();
    test_momentum();
    test_vol_clustering();
    test_candles();
    test_shock();
    test_buy();
    test_sell();
    test_round_trip();
    test_value_pnl();
    test_layout();
    test_basis();
    test_price_row();
    printf("test_stock_core: %s\n", fails ? "FAILED" : "OK");
    return fails ? 1 : 0;
}
