# 股市（card-expand id 80）实施计划

> **版本**：TH18 v1.00a（`th18.exe`，imagebase `0x400000`）。本文裸地址默认属该版本。

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 给 TH18 card-expand 加一张主动卡「股市」（id 80）：C 买 / Shift+C 卖各 50 金钱，股价由回放安全的随机过程驱动，HUD 右下常驻 16 根 K 线 + 三行读数。

**Architecture:** 纯逻辑（模拟 / 交易 / K 线缓冲 / 蜡烛几何）在 `stock_core.c`，主机单测；引擎胶水（输入、`MONEY`、33 个 UI 层 VM、HUD 文字）在 `stock.c`，走现有 SDK 原语。SDK 本身改三处：私有状态块 256 → 512、新增 `on_delete` 回调（删对象前让卡清自己的 VM）、`engine.h` 加 `INPUT_HELD`。不开新断点、不改游戏资源文件。

**Tech Stack:** C11（主机 gcc 单测；DLL 用 `native/Makefile` 的 llvm-mingw i686）、thcrap 断点框架（已有）、ANM 脚本文本（`assets/build_ability.py`）、Python 3 + PIL（卡图）。

**Spec:** `docs/superpowers/specs/2026-09-08-stock-market-design.md`

## Global Constraints

- 版本死绑 TH18 v1.00a（`th18.exe`，imagebase `0x400000`）；文档里裸地址默认该版本。
- 所有 `.md` 改动后跑 `python3 tooling/check-docs.py`（仓根）；地址写成 `` `0x4ca428` ``（反引号、无 `@`）。
- 单测编译标志固定：`cc -std=c11 -Wall -Wextra -Werror`；`stock_core.c` 不许依赖引擎头（它同时进 DLL 和主机测试）。
- 全整数、无浮点、随机数只从调用方注入（胶水层给 `ce_rand()` = `REPLAY_SAFE_RNG` `0x4cf288`）。
- 交易只用 replay 记录的输入位：用卡 `0x400`（引擎分派）+ 低速 `0x008`（`INPUT_HELD` `0x4ca428`）。
- 每次买卖固定 `50` 金钱；价格单位 0.01（开盘 `10000`，钳 `[1000, 999900]`）；持股单位 0.0001 股。
- 每 15 帧一步、每 16 步一根 K 线、缓冲 16 根；面板 HUD 右下、常驻；持仓跨关、商店只看现金。
- 工作区里有其他会话的未提交改动：提交只 `git add` 本任务列出的文件，**绝不** `git add -A`。
- 提交信息末尾两行固定：
  ```
  Co-Authored-By: Claude Fable 5.1 <noreply@anthropic.com>
  Claude-Session: https://claude.ai/code/session_011gRYCoH8AKyQatW1gtKojT
  ```

所有路径相对仓根 `/data/sunyunbo/www/renkolab`；`native/` 指 `mods/th18.v1.00a/card-expand/native/`，`assets/` 指 `mods/th18.v1.00a/card-expand/assets/`。

---

## 文件总览

| 文件 | 职责 | 任务 |
| --- | --- | --- |
| `native/sdk_core.h`、`native/tests/test_sdk_core.c` | `CE_STATE_BYTES` 512 | 1 |
| `native/sdk/public.h`、`native/sdk.h`、`native/engine.h`、`SDK.md` | `on_delete` 回调、`INPUT_HELD` | 1 |
| `native/cards/stock_core.h/.c`、`native/tests/test_stock_core.c`、`native/Makefile` | 模拟 / K 线 / 交易 / 几何 | 2–4 |
| `assets/ability/scripts/104_stock_rect.anm.txt`、`native/anm_ids.h`（生成） | 纯色矩形脚本 | 5 |
| `native/cards/stock.c`、`patch/th18/cards.js`、`patch-test/th18/cards_dev.js` | 引擎胶水 + 登记 | 5 |
| `assets/cards/make_stock_art.py`、`assets/cards/ORDER.txt`、`assets/cards/STOCK_*.png`、`assets/cards/_art/STOCK.png` | 卡图 | 6 |
| `CARDS.md`、`NEXT.md`、`AUDIT.md` | 登记 / 交接 / 审计 | 7 |

---

### Task 1: SDK 底座——状态块 512、`on_delete` 回调、`INPUT_HELD`

**Files:**
- Modify: `native/sdk_core.h:13`
- Modify: `native/tests/test_sdk_core.c`（状态槽那段测试之后）
- Modify: `native/sdk/public.h`（`ce_hooks_t`）
- Modify: `native/sdk.h`（`CE_CARD` 宏里的 `ce_s_##ID##_opdelete`）
- Modify: `native/engine.h:51` 附近
- Modify: `mods/th18.v1.00a/card-expand/SDK.md` §2 表的 `+0x50` 行、§4

**Interfaces:**
- Produces: `CE_STATE_BYTES == 512`；`ce_hooks_t.on_delete`（`void (*)(ce_card_t *)`，删对象时、释放私有状态**之前**调）；`CE_ADDR_INPUT_HELD`、`CE_INPUT_HELD()`、`CE_INPUT_FOCUS`。

- [ ] **Step 1: 写会失败的测试（状态块能装 480 字节的卡状态）**

在 `native/tests/test_sdk_core.c` 里找到 `ce_state_user(&k3, CE_STATE_BYTES - CE_STATE_RESERVED + 1) == NULL` 那一段之后、`/* 满 */` 之前，插入：

```c
    /* 2026-09-08：股市（id 80）要 430 多字节（17 根 OHLC + 33 个 anm id），块提到 512 */
    int k4 = 0;
    CHECK(CE_STATE_BYTES >= 512);
    unsigned char *big = ce_state_user(&k4, 480);
    CHECK(big != NULL);
    big[479] = 1;
    CHECK(((unsigned char *)ce_state_get(&k4))[CE_STATE_RESERVED + 479] == 1);
    ce_state_free(&k4);
```

- [ ] **Step 2: 跑测试确认失败**

```bash
cd mods/th18.v1.00a/card-expand/native && mkdir -p tests/build && \
cc -std=c11 -Wall -Wextra -Werror -o tests/build/test_sdk_core tests/test_sdk_core.c sdk_core.c && ./tests/build/test_sdk_core
```
Expected: `FAIL tests/test_sdk_core.c:<行> ` 两条（`CE_STATE_BYTES >= 512` 与 `big != NULL`），最后一行 `test_sdk_core: FAILED`。

- [ ] **Step 3: 改常量**

`native/sdk_core.h:13`：
```c
#define CE_STATE_BYTES       512       /* 每张卡私有状态上限（2026-09-08 256 → 512：股市 id 80 要 430+ 字节；静态表 256 × 512 = 128 KB）*/
```

- [ ] **Step 4: 跑测试确认通过**

```bash
cd mods/th18.v1.00a/card-expand/native && cc -std=c11 -Wall -Wextra -Werror -o tests/build/test_sdk_core tests/test_sdk_core.c sdk_core.c && ./tests/build/test_sdk_core
```
Expected: `test_sdk_core: OK`。

- [ ] **Step 5: 加 `on_delete` 回调**

`native/sdk/public.h`：在 `void (*on_shop_open)(ce_card_t *);` 之后加一行：
```c
    /* 删对象（局末 recount / 倒吊人 ce_destroy_card）时、SDK 释放私有状态之前调：卡在这里删自己起的 VM 等外部资源。
     * on_run_reset 只覆盖局末那条路，局中被摧毁只走这里。2026-09-08 股市（80）第一个用户。 */
    void (*on_delete)(ce_card_t *);
```

`native/sdk.h` 的 `CE_CARD` 宏里，把
```c
    static void CE_TC ce_s_##ID##_opdelete(void *self, uint32_t flag) { \
        ce_sdk_trace(ID, 0x50, "operator_delete"); \
        ce_dmg_set(ID, 0);                    /* 退出聚合；无条件，运行时才参与的卡同样要清 */ \
        ce_state_free(self); \
```
改成
```c
    static void CE_TC ce_s_##ID##_opdelete(void *self, uint32_t flag) { \
        ce_sdk_trace(ID, 0x50, "operator_delete"); \
        ce_dmg_set(ID, 0);                    /* 退出聚合；无条件，运行时才参与的卡同样要清 */ \
        if (ce_hooks_##ID.on_delete) ce_hooks_##ID.on_delete((ce_card_t *)self); /* 私有状态还活着 */ \
        ce_state_free(self); \
```

- [ ] **Step 6: 加 `INPUT_HELD`**

`native/engine.h`，紧跟 `CE_ADDR_INPUT_PRESSED` 那行之后：
```c
#define CE_ADDR_INPUT_HELD         0x4ca428   /* 本帧按住的位（录制时原始位、回放时 replay 写入；04-active-cards.md §1）。0x008 = 低速 / 聚焦（0x45b3fa 读它）*/
#define CE_INPUT_FOCUS             0x008
#define CE_INPUT_HELD()   (*(uint32_t *)CE_ADDR_INPUT_HELD)
```

- [ ] **Step 7: 编一次 DLL 与主机测试全套确认没破**

```bash
cd mods/th18.v1.00a/card-expand/native && make test-host 2>&1 | tail -3 && make dll 2>&1 | tail -3
```
Expected: 每个 `test_*: OK`；`make dll` 无 error，产出 `th18_card_expand.dll`（`public.h` 的编译测试 `test_public_sdk_compile.c` 也在 test-host 里）。

- [ ] **Step 8: 改 SDK.md**

`SDK.md` §2 槽表里 `+0x50` 那一行（现在写的是「（SDK 自己占）… 先清私有状态，再跳基类」）改为：
```
| +0x50 | `on_delete` | `void (ce_card_t*)` | 删对象：SDK 先调它（私有状态仍在，卡删自己的 VM 等），再释放私有状态、跳基类 `0x411410`。局中被倒吊人摧毁只走这里，`on_run_reset` 不会来 |
```
§4 里「`size` 上限 256 字节」改为「`size` 上限 `CE_STATE_BYTES − 16` = 496 字节（2026-09-08 块 256 → 512，股市 id 80 要 430+）」。

```bash
python3 tooling/check-docs.py 2>&1 | tail -2
```
Expected: 违规数不比改前多（改前 5 处，全在别的文件）。

- [ ] **Step 9: 提交**

```bash
git add mods/th18.v1.00a/card-expand/native/sdk_core.h mods/th18.v1.00a/card-expand/native/tests/test_sdk_core.c \
  mods/th18.v1.00a/card-expand/native/sdk/public.h mods/th18.v1.00a/card-expand/native/sdk.h \
  mods/th18.v1.00a/card-expand/native/engine.h mods/th18.v1.00a/card-expand/SDK.md
git commit -m "sdk(card-expand): 状态块 512 + on_delete 回调 + INPUT_HELD

Co-Authored-By: Claude Fable 5.1 <noreply@anthropic.com>
Claude-Session: https://claude.ai/code/session_011gRYCoH8AKyQatW1gtKojT"
```

---

### Task 2: `stock_core` —— 状态、行情步进、K 线缓冲

**Files:**
- Create: `native/cards/stock_core.h`
- Create: `native/cards/stock_core.c`
- Create: `native/tests/test_stock_core.c`
- Modify: `native/Makefile`（`test-host` 目标末尾）

**Interfaces:**
- Produces（Task 3/4/5 都用）：

```c
#define STOCK_CANDLES          16
#define STOCK_STEP_FRAMES      15
#define STOCK_STEPS_PER_CANDLE 16
#define STOCK_LOT              50
#define STOCK_PRICE_OPEN       10000
#define STOCK_PRICE_MIN        1000
#define STOCK_PRICE_MAX        999900
#define STOCK_REGIME_MIN       80
#define STOCK_REGIME_MAX       240
typedef struct { int32_t o, h, l, c; } stock_candle_t;
typedef struct {
    int32_t price;        /* 0.01 金钱 */
    int32_t hold;         /* 0.0001 股 */
    int32_t cost;         /* 金钱：累计买入 − 累计卖出所得 */
    uint32_t steps;       /* 总步数 */
    uint8_t  regime;      /* 0 平 1 牛 2 熊 */
    uint8_t  step_in_candle;
    uint16_t regime_left; /* 本阶段剩余步数 */
    stock_candle_t cur;   /* 未收盘那根 */
    stock_candle_t hist[STOCK_CANDLES];   /* 环形 */
    uint8_t  head;        /* 下一根写入下标 */
    uint8_t  count;       /* 已收根数 ≤ 16 */
} stock_state_t;
void stock_init(stock_state_t *s);
void stock_step(stock_state_t *s, uint32_t rnd);
void stock_shock(stock_state_t *s, int permille);
/* 最新的 n ≤ 16 根（最多 15 根已收 + 当前），旧 → 新；返回 n */
int  stock_visible(const stock_state_t *s, stock_candle_t out[STOCK_CANDLES]);
```

- [ ] **Step 1: 写会失败的测试**

`native/tests/test_stock_core.c`：

```c
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

int main(void)
{
    test_init();
    test_deterministic();
    test_clamp();
    test_regime();
    test_candles();
    test_shock();
    printf("test_stock_core: %s\n", fails ? "FAILED" : "OK");
    return fails ? 1 : 0;
}
```

- [ ] **Step 2: 跑测试确认失败**

```bash
cd mods/th18.v1.00a/card-expand/native && cc -std=c11 -Wall -Wextra -Werror -o tests/build/test_stock_core tests/test_stock_core.c cards/stock_core.c
```
Expected: 编译错误 `../cards/stock_core.h: No such file or directory`。

- [ ] **Step 3: 写头文件**

`native/cards/stock_core.h`：

```c
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
```

- [ ] **Step 4: 写实现（本任务只写 init / step / shock / visible；buy/sell/value/pnl/layout 放 Task 3、4，先给个能链接的桩）**

`native/cards/stock_core.c`：

```c
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
```

- [ ] **Step 5: 跑测试确认通过**

```bash
cd mods/th18.v1.00a/card-expand/native && cc -std=c11 -Wall -Wextra -Werror -o tests/build/test_stock_core tests/test_stock_core.c cards/stock_core.c && ./tests/build/test_stock_core
```
Expected: `test_stock_core: OK`。若 `test_regime` 的「段长越界」失败，检查 `stock_step` 是「`== 0` 时重抽、然后无条件 `--`」（段长 = L 步，含重抽那步）。

- [ ] **Step 6: 挂进 Makefile**

`native/Makefile` `test-host` 目标最后（`./tests/build/test_whirlwind_core` 那行之后）加：
```make
	$(HOSTCC) -std=c11 -Wall -Wextra -Werror -o tests/build/test_stock_core tests/test_stock_core.c cards/stock_core.c
	./tests/build/test_stock_core
```
（Tab 缩进。）

```bash
cd mods/th18.v1.00a/card-expand/native && make test-host 2>&1 | tail -2
```
Expected: 最后两行 `./tests/build/test_stock_core` / `test_stock_core: OK`。

- [ ] **Step 7: 提交**

```bash
git add mods/th18.v1.00a/card-expand/native/cards/stock_core.h mods/th18.v1.00a/card-expand/native/cards/stock_core.c \
  mods/th18.v1.00a/card-expand/native/tests/test_stock_core.c mods/th18.v1.00a/card-expand/native/Makefile
git commit -m "feat(card-expand): 股市 stock_core：占位行情模型 + K 线环形缓冲

Co-Authored-By: Claude Fable 5.1 <noreply@anthropic.com>
Claude-Session: https://claude.ai/code/session_011gRYCoH8AKyQatW1gtKojT"
```

---

### Task 3: `stock_core` —— 买 / 卖 / 市值 / 盈亏

**Files:**
- Modify: `native/cards/stock_core.c`（替换 Task 2 的四个桩）
- Modify: `native/tests/test_stock_core.c`

**Interfaces:**
- Consumes: Task 2 的 `stock_state_t`、`STOCK_LOT`、`STOCK_HOLD_PER_MONEY`。
- Produces: `stock_buy / stock_sell / stock_value / stock_pnl` 的真实语义（见头文件注释）。

- [ ] **Step 1: 写会失败的测试**

`test_stock_core.c` 的 `main` 之前加：

```c
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
```

`main` 里 `test_shock();` 之后加：
```c
    test_buy();
    test_sell();
    test_round_trip();
    test_value_pnl();
```

- [ ] **Step 2: 跑测试确认失败**

```bash
cd mods/th18.v1.00a/card-expand/native && cc -std=c11 -Wall -Wextra -Werror -o tests/build/test_stock_core tests/test_stock_core.c cards/stock_core.c && ./tests/build/test_stock_core
```
Expected: `FAIL … 第一次买应成交` 等多条，`test_stock_core: FAILED`。

- [ ] **Step 3: 实现**

把 `stock_core.c` 里 Task 2 的四个交易桩（`stock_buy / stock_sell / stock_value / stock_pnl`）替换为：

```c
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

/* 卖：STOCK_LOT 金钱等值的持股，不足全卖；所得向下取整。 */
int stock_sell(stock_state_t *s, int32_t *money)
{
    if (s->hold <= 0) return 0;
    int64_t q = (int64_t)STOCK_LOT * STOCK_HOLD_PER_MONEY / s->price;
    if (q > s->hold) q = s->hold;
    int64_t got = q * s->price / STOCK_HOLD_PER_MONEY;
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
```

- [ ] **Step 4: 跑测试确认通过**

```bash
cd mods/th18.v1.00a/card-expand/native && cc -std=c11 -Wall -Wextra -Werror -o tests/build/test_stock_core tests/test_stock_core.c cards/stock_core.c && ./tests/build/test_stock_core
```
Expected: `test_stock_core: OK`。

- [ ] **Step 5: 提交**

```bash
git add mods/th18.v1.00a/card-expand/native/cards/stock_core.c mods/th18.v1.00a/card-expand/native/tests/test_stock_core.c
git commit -m "feat(card-expand): 股市 stock_core：买卖 / 市值 / 盈亏

Co-Authored-By: Claude Fable 5.1 <noreply@anthropic.com>
Claude-Session: https://claude.ai/code/session_011gRYCoH8AKyQatW1gtKojT"
```

---

### Task 4: `stock_core` —— 蜡烛几何 `stock_layout`

**Files:**
- Modify: `native/cards/stock_core.c`（替换 `stock_layout` 桩）
- Modify: `native/tests/test_stock_core.c`

**Interfaces:**
- Consumes: `stock_visible`。
- Produces: `stock_layout(s, x0, y0, w, h, body[16], wick[16])`：槽 i 的中心 x = `x0 + pitch × i + pitch / 2`（`pitch = w / 16`）；实体宽 `pitch × 7 / 13`（pitch 26 → 14）、影线宽 2；y 按 visible 所有根的最高 / 最低价线性映射到 `[y0, y0 + h]`（价高在上）；实体高最小 2、影线高最小 1；`up = 收 ≥ 开`；空槽 `w = h = 0`；最新一根在槽 15。

- [ ] **Step 1: 写会失败的测试**

`test_stock_core.c` 的 `main` 之前加：

```c
static void test_layout(void)
{
    stock_state_t s;
    stock_rect_t body[STOCK_CANDLES], wick[STOCK_CANDLES];
    stock_init(&s);
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
    stock_init(&s);
    stock_layout(&s, 848, 776, 416, 96, body, wick);
    CHECK(body[15].h == 2 && wick[15].h >= 1 && body[15].w == 14, "退化蜡烛：h=%d wick=%d", body[15].h, wick[15].h);
    CHECK(body[15].cy >= 776 && body[15].cy <= 776 + 96, "退化蜡烛应落在价区内，cy=%d", body[15].cy);
    for (int i = 0; i < STOCK_CANDLES - 1; ++i) CHECK(body[i].w == 0, "只有一根时其它槽应为空");

    /* 满缓冲：16 槽全有、最右是 cur、全部在区域内 */
    lcg_state = 21;
    for (int i = 0; i < STOCK_STEPS_PER_CANDLE * 40; ++i) stock_step(&s, lcg());
    stock_layout(&s, 848, 776, 416, 96, body, wick);
    for (int i = 0; i < STOCK_CANDLES; ++i) {
        CHECK(body[i].w == 14 && body[i].h >= 2 && wick[i].h >= 1, "槽 %d 应有蜡烛", i);
        CHECK(wick[i].cy - wick[i].h / 2 >= 776 - 1 && wick[i].cy + wick[i].h / 2 <= 776 + 96 + 1, "槽 %d 影线越界", i);
        CHECK(body[i].cx >= 848 && body[i].cx <= 848 + 416, "槽 %d x 越界", i);
    }
    CHECK(body[15].up == (s.cur.c >= s.cur.o), "最右一根应是 cur");
}
```
`main` 里 `test_value_pnl();` 之后加 `test_layout();`。

- [ ] **Step 2: 跑测试确认失败**

```bash
cd mods/th18.v1.00a/card-expand/native && cc -std=c11 -Wall -Wextra -Werror -o tests/build/test_stock_core tests/test_stock_core.c cards/stock_core.c && ./tests/build/test_stock_core
```
Expected: `FAIL … 中心 x 应按 pitch 排` 等，`FAILED`。

- [ ] **Step 3: 实现**

替换 `stock_core.c` 的 `stock_layout` 桩：

```c
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
```

- [ ] **Step 4: 跑测试确认通过**

```bash
cd mods/th18.v1.00a/card-expand/native && make test-host 2>&1 | tail -2
```
Expected: `test_stock_core: OK`。

- [ ] **Step 5: 提交**

```bash
git add mods/th18.v1.00a/card-expand/native/cards/stock_core.c mods/th18.v1.00a/card-expand/native/tests/test_stock_core.c
git commit -m "feat(card-expand): 股市 stock_core：蜡烛几何 stock_layout

Co-Authored-By: Claude Fable 5.1 <noreply@anthropic.com>
Claude-Session: https://claude.ai/code/session_011gRYCoH8AKyQatW1gtKojT"
```

---

### Task 5: 引擎胶水 `stock.c` + ANM 矩形脚本 + JSON 登记

**Files:**
- Create: `assets/ability/scripts/104_stock_rect.anm.txt`
- Create: `native/cards/stock.c`
- Modify: `patch/th18/cards.js`（`"79"` 条目之后）
- Modify: `patch-test/th18/cards_dev.js`
- Generated: `native/anm_ids.h`（`make anm`）、`native/build/ability.anm`

**Interfaces:**
- Consumes: Task 1 的 `on_delete` / `CE_INPUT_HELD()` / `CE_INPUT_FOCUS`；Task 2–4 的全部 `stock_*`；SDK 现有 `ce_state`、`ce_rand`、`ce_refuse`、`ce_play_sound`、`ce_player_x`、`ce_hud_text`、`ce_anm_spawn_ui`、`ce_anm_get_vm`、`ce_anm_set_pos / set_scale / set_color`、`ce_anm_delete`、`CE_ABILITY_ANM()`、`CE_MONEY()`。
- Produces: `CE_CARD(80, …)`；`CE_ANM_ABILITY_SCRIPT_STOCK_RECT`（生成）。

- [ ] **Step 1: ANM 脚本**

`assets/ability/scripts/104_stock_rect.anm.txt`（编号接现有 99..103 之后；`ls assets/ability/scripts | tail -1` 确认最后一个是 `103_pea_burst.anm.txt`，否则顺延）：

```
script script104 {
    sprite(-1);
    layer(26);
    resolutionMode(2);
    zWriteDisable(1);
    blendMode(0);
    drawRect(1.0f, 1.0f);
    color(255, 255, 255);
    alpha(255);
    stop();
    interruptLabel(1);
    delete();
}
```
（`layer(26); resolutionMode(2)` = 零售 HUD 图标 / 真银圣剑数字 script99 同款；`drawRect + scale` = 青眼血条 script87 同款。位置 / 尺寸 / 颜色全由 C 写 VM 字段。）

```bash
cd mods/th18.v1.00a/card-expand/native && make anm 2>&1 | tail -5 && grep STOCK_RECT anm_ids.h
```
Expected: 构建通过（零售部分自检不变），`#define CE_ANM_ABILITY_SCRIPT_STOCK_RECT 104`。

- [ ] **Step 2: 写 `stock.c`**

`native/cards/stock.c`：

```c
/* 股市（id 80）—— 主动卡：选中它时 C = 买入 50 金钱，Shift + C = 卖出 50 金钱等值。
 * 股价由占位随机过程驱动（stock_core.c，每 15 帧一步、4 秒一根 K 线），HUD 右下常驻面板：16 根 K 线 + 三行读数。
 * 持仓跨关保留、商店只看现金、局末清零。设计 docs/superpowers/specs/2026-09-08-stock-market-design.md，审计 AUDIT §AF。
 *
 * 为什么只能是这两个键：replay 只重放 INPUT_HELD 的位（射击 / 炸弹 / 低速 / 方向 / 用卡 / 切卡），别的键回放会失同步。
 * 用卡位的分派引擎已做好（选中的主动卡才收到 c_press），我们只在 on_activate 里读低速位区分买卖。
 * 随机数走 ce_rand()（REPLAY_SAFE_RNG），与弹幕 / 零售卡同源 ⇒ replay 天然同步。
 *
 * 画图：33 个 UI 层 VM（1 底板 + 16 × {实体, 影线}），脚本 stock_rect（drawRect 1×1，C 写 pos / scale / color 就地改）。
 * UI 层 VM 的坐标空间是 2× 的（HUD 图标根 VM pos(1056, 537) 对应 640×480 空间的 (528, 268)，AUDIT X5），
 * ce_hud_text 用的是 640×480 字面量空间（AUDIT AC6）—— 两套常量分开写。⚠️ 全部「待实跑微调」。 */
#include <string.h>

#include "sdk.h"
#include "stock_core.h"

#define STOCK_SE_BUY    0x0f            /* se_cardget */
#define STOCK_SE_SELL   0x2f            /* se_bonus */

/* —— 面板几何：UI 层 2× 空间（1280×960）—— */
#define UI_CANDLE_X0    848             /* 640 空间 424 */
#define UI_CANDLE_Y0    776             /* 640 空间 388 */
#define UI_CANDLE_W     416             /* 16 根 × pitch 26 */
#define UI_CANDLE_H     96
#define UI_BG_CX        1056            /* 底板中心 / 尺寸：盖住蜡烛区 + 下面三行字（640 空间 386..476）*/
#define UI_BG_CY        862
#define UI_BG_W         432
#define UI_BG_H         180
/* —— 文字：640×480 字面量空间 —— */
#define TEXT_X          424.0f
#define TEXT_Y1         440.0f
#define TEXT_Y2         452.0f
#define TEXT_Y3         464.0f
#define TEXT_COLOR      CE_HUD_POWER_COLOR

#define COLOR_BG        0xa0000000u     /* AARRGGBB：黑，alpha 160 */
#define COLOR_UP        0xffff4040u     /* 红涨 */
#define COLOR_DOWN      0xff40ff40u     /* 绿跌 */

typedef struct {
    stock_state_t st;
    uint32_t bg_id;
    uint32_t body_id[STOCK_CANDLES];
    uint32_t wick_id[STOCK_CANDLES];
    uint8_t  frame;                      /* 距上一步的帧数 */
    uint8_t  inited;
    uint8_t  vms_logged;
} stock_card_t;

static uint32_t spawn_rect(void)
{
    static const float origin[3] = { 0.0f, 0.0f, 0.0f };
    return ce_anm_spawn_ui(CE_ABILITY_ANM(), CE_ANM_ABILITY_SCRIPT_STOCK_RECT, origin);
}

static void kill_vms(stock_card_t *s)
{
    ce_anm_delete(s->bg_id); s->bg_id = 0;
    for (int i = 0; i < STOCK_CANDLES; ++i) {
        ce_anm_delete(s->body_id[i]); s->body_id[i] = 0;
        ce_anm_delete(s->wick_id[i]); s->wick_id[i] = 0;
    }
}

static int vms_alive(const stock_card_t *s)
{
    if (!s->bg_id || !ce_anm_get_vm(s->bg_id)) return 0;
    for (int i = 0; i < STOCK_CANDLES; ++i)
        if (!s->body_id[i] || !ce_anm_get_vm(s->body_id[i]) || !s->wick_id[i] || !ce_anm_get_vm(s->wick_id[i])) return 0;
    return 1;
}

/* 底板先起（UI 列表尾插 = 后画在上），再 16 组蜡烛。任何一个没了就整组重来，保证叠放顺序。 */
static int spawn_vms(stock_card_t *s)
{
    kill_vms(s);
    s->bg_id = spawn_rect();
    for (int i = 0; i < STOCK_CANDLES; ++i) { s->wick_id[i] = spawn_rect(); s->body_id[i] = spawn_rect(); }
    if (!vms_alive(s)) { kill_vms(s); if (!s->vms_logged) { ce_log("stock: FAILED to spawn panel VMs"); s->vms_logged = 1; } return 0; }
    ce_anm_set_pos(s->bg_id, (float)UI_BG_CX, (float)UI_BG_CY, 0.0f);
    ce_anm_set_scale(s->bg_id, (float)UI_BG_W, (float)UI_BG_H);
    ce_anm_set_color(s->bg_id, COLOR_BG);
    if (!s->vms_logged) {
        ce_log("stock: panel spawned (bg id %u at %d,%d size %dx%d; candles x %d..%d y %d..%d; 2x UI space)",
               s->bg_id, UI_BG_CX, UI_BG_CY, UI_BG_W, UI_BG_H, UI_CANDLE_X0, UI_CANDLE_X0 + UI_CANDLE_W, UI_CANDLE_Y0, UI_CANDLE_Y0 + UI_CANDLE_H);
        s->vms_logged = 1;
    }
    return 1;
}

static void apply_rect(uint32_t id, const stock_rect_t *r)
{
    ce_anm_set_pos(id, (float)r->cx, (float)r->cy, 0.0f);
    ce_anm_set_scale(id, (float)r->w, (float)r->h);        /* w = h = 0 的空槽就是看不见 */
    ce_anm_set_color(id, r->up ? COLOR_UP : COLOR_DOWN);
}

static void draw_candles(stock_card_t *s)
{
    stock_rect_t body[STOCK_CANDLES], wick[STOCK_CANDLES];
    stock_layout(&s->st, UI_CANDLE_X0, UI_CANDLE_Y0, UI_CANDLE_W, UI_CANDLE_H, body, wick);
    for (int i = 0; i < STOCK_CANDLES; ++i) { apply_rect(s->wick_id[i], &wick[i]); apply_rect(s->body_id[i], &body[i]); }
}

/* ctor：每关开场引擎会再调一次；只在还没初始化时铺初值（存档 / replay 装载的 mode 1 不调 ctor，所以不用 owned[] 判「刚获得」）。 */
static int ctor(ce_card_t *c)
{
    stock_card_t *s = ce_state(c, stock_card_t);
    if (s && !s->inited) { stock_init(&s->st); s->inited = 1; ce_log("stock: opened at %d.%02d", s->st.price / 100, s->st.price % 100); }
    return 0;
}

/* C 键（引擎已门控：对话中 / 无敌人时不会来）。低速位按住 = 卖，否则买。返回 0 = 瞬发。 */
static int on_activate(ce_card_t *c)
{
    stock_card_t *s = ce_state(c, stock_card_t);
    if (!s || !s->inited) return ce_refuse("stock", "no state");
    int sell = (CE_INPUT_HELD() & CE_INPUT_FOCUS) != 0;
    int32_t money = CE_MONEY();
    int ok = sell ? stock_sell(&s->st, &money) : stock_buy(&s->st, &money);
    if (!ok) return ce_refuse("stock", sell ? "no holding" : "no cash");
    CE_MONEY() = money;
    ce_play_sound(sell ? STOCK_SE_SELL : STOCK_SE_BUY, ce_player_x());
    ce_log("stock: %s %d @ %d.%02d -> hold %d.%04d (money %d, cost %d, value %d, pnl %d)",
           sell ? "sell" : "buy", STOCK_LOT, s->st.price / 100, s->st.price % 100,
           s->st.hold / 10000, s->st.hold % 10000, money, s->st.cost, stock_value(&s->st), stock_pnl(&s->st));
    return 0;
}

/* +0x2c：管理器侧每帧（菜单 / 商店里不跑 ⇒ 行情冻结）。每 15 帧走一步；VM 丢了就整组重建。 */
static int on_tick_2(ce_card_t *c)
{
    stock_card_t *s = ce_state(c, stock_card_t);
    int redraw = 0;
    if (!s || !s->inited) return 0;
    if (++s->frame >= STOCK_STEP_FRAMES) {
        s->frame = 0;
        stock_step(&s->st, ce_rand());
        redraw = 1;
    }
    if (!vms_alive(s)) { if (!spawn_vms(s)) return 0; redraw = 1; }
    if (redraw) draw_candles(s);
    return 0;
}

/* +0x48：HUD 文字，只能在这里画（AUDIT AC）。 */
static int on_draw(ce_card_t *c)
{
    stock_card_t *s = ce_state(c, stock_card_t);
    if (!s || !s->inited) return 0;
    const stock_state_t *st = &s->st;
    char arrow = st->price > st->cur.o ? '^' : st->price < st->cur.o ? 'v' : '-';
    int32_t pnl = stock_pnl(st);
    ce_hud_text(TEXT_X, TEXT_Y1, TEXT_COLOR, "STK %d.%02d %c", st->price / 100, st->price % 100, arrow);
    ce_hud_text(TEXT_X, TEXT_Y2, TEXT_COLOR, "HOLD %d.%02d = %d", st->hold / 10000, st->hold % 10000 / 100, stock_value(st));
    ce_hud_text(TEXT_X, TEXT_Y3, TEXT_COLOR, "P/L %c%d", pnl < 0 ? '-' : '+', pnl < 0 ? -pnl : pnl);
    return 0;
}

/* +0x4c：局末。持仓 / 行情清零，面板删掉。 */
static void on_run_reset(ce_card_t *c)
{
    stock_card_t *s = ce_state(c, stock_card_t);
    if (!s) return;
    if (s->inited) ce_log("stock: run reset (price %d.%02d, hold %d.%04d, pnl %d, %u steps)",
                          s->st.price / 100, s->st.price % 100, s->st.hold / 10000, s->st.hold % 10000, stock_pnl(&s->st), s->st.steps);
    kill_vms(s);
    memset(s, 0, sizeof *s);
}

/* +0x50：删对象（局末 recount / 倒吊人摧毁）。私有状态还在，只需把 VM 删掉。 */
static void on_delete(ce_card_t *c)
{
    stock_card_t *s = ce_state(c, stock_card_t);
    if (s) kill_vms(s);
}

CE_CARD(80, .active_recharge = 30, .ctor = ctor, .on_activate = on_activate, .on_tick_2 = on_tick_2,
            .on_draw = on_draw, .on_run_reset = on_run_reset, .on_delete = on_delete);
```

- [ ] **Step 3: 登记 JSON**

`patch/th18/cards.js`：在 `"79": { … }` 条目之后（保持 JSON 逗号正确）加：

```json
  "80": {
    "name": "股市",
    "desc": [
      "主动（C 键）：投入 50 金钱买入股票，",
      "Shift + C 卖出 50 金钱等值。",
      "股价随时间涨跌，K 线画在右下角。",
      "持仓过关保留；商店只认现金。"
    ],
    "internal_name": "STOCK",
    "initial_unlocked": 1,
    "category": 0,
    "price_tier": 8,
    "weight": 2,
    "sprite_large": 168,
    "sprite_small": 169
  }
```
（sprite 168/169 = `assets/cards/ORDER.txt` 第 25 行（0 起），Task 6 追加 `STOCK` 后 `make anm` 会打印并校验；在 Task 6 之前 `make anm` 的 abcard 校验会报 JSON 与 ORDER 不一致——**所以 Task 5 的构建只跑 `make dll` 与 `python3 ../assets/build_ability.py`，abcard 留到 Task 6。**）

`patch-test/th18/cards_dev.js`：`start_deck` 的最后一格 `67` 改成 `80`（起手带股市，其它四张不动）。

- [ ] **Step 4: 编 DLL**

```bash
cd mods/th18.v1.00a/card-expand/native && python3 ../assets/build_ability.py 2>&1 | tail -2 && make dll 2>&1 | grep -i "error\|warning" ; ls -la th18_card_expand.dll
```
Expected: 无 error / warning；DLL 存在且时间是刚才。若报 `CE_ANM_ABILITY_SCRIPT_STOCK_RECT` 未定义，先跑 Step 1 的 `make anm`（或 `build_ability.py`）。若 `-Werror` 报 `stock_card_t` 太大：`sizeof(stock_card_t)` 应约 434，`ce_state` 上限 496（Task 1）；超了就是 Task 1 没做。

- [ ] **Step 5: 主机测试全套 + 编译期断言私有状态尺寸**

在 `stock.c` 的 `typedef … stock_card_t;` 之后加一行编译期断言（`sdk_core.h` 的常量在 `sdk.h` 里可见）：
```c
_Static_assert(sizeof(stock_card_t) <= CE_STATE_BYTES - CE_STATE_RESERVED, "stock_card_t must fit ce_state");
```
（`CE_STATE_BYTES` / `CE_STATE_RESERVED` 在 `sdk_core.h`；若 `sdk.h` 没把它带进来，`stock.c` 顶部加 `#include "sdk_core.h"`。）
```bash
cd mods/th18.v1.00a/card-expand/native && make dll 2>&1 | grep -i "error" ; make test-host 2>&1 | grep -c ": OK"
```
Expected: 无 error；`OK` 计数 = 改前 + 1（现在 13 个测试程序）。

- [ ] **Step 6: 提交**

```bash
git add mods/th18.v1.00a/card-expand/assets/ability/scripts/104_stock_rect.anm.txt mods/th18.v1.00a/card-expand/native/anm_ids.h \
  mods/th18.v1.00a/card-expand/native/cards/stock.c mods/th18.v1.00a/card-expand/patch/th18/cards.js \
  mods/th18.v1.00a/card-expand/patch-test/th18/cards_dev.js
git commit -m "feat(card-expand): 股市（id 80）引擎胶水：C 买 / Shift+C 卖、K 线面板、HUD 读数

Co-Authored-By: Claude Fable 5.1 <noreply@anthropic.com>
Claude-Session: https://claude.ai/code/session_011gRYCoH8AKyQatW1gtKojT"
```
（`native/build/` 是否入库照仓里现状：`git check-ignore native/build/ability.anm` 说被忽略就不加。）

---

### Task 6: 卡图（程序生成 K 线图）+ `abcard.anm`

**Files:**
- Create: `assets/cards/make_stock_art.py`
- Create（脚本产出）: `assets/cards/_src/STOCK_face.png`、`assets/cards/_art/STOCK.png`、`assets/cards/STOCK_max.png`、`assets/cards/STOCK_min.png`
- Modify: `assets/cards/ORDER.txt`（追加一行 `STOCK`）

**Interfaces:**
- Consumes: `assets/fit_card.py`（`NAME src --no-detect --trim 0 --margin 0 --bg '#0b0e14'`）；`assets/README.md` 的 sprite 规则 `118 + 2k / 119 + 2k`。
- Produces: sprite 168 / 169，与 Task 5 的 JSON 对上。

- [ ] **Step 1: 写生成脚本**

`assets/cards/make_stock_art.py`：

```python
#!/usr/bin/env python3
"""股市（id 80）的卡图画面，程序生成（固定种子，重跑逐字节一致）。

    python3 make_stock_art.py
    python3 ../fit_card.py STOCK _src/STOCK_face.png --no-detect --trim 0 --margin 0 --bg '#0b0e14'

深蓝黑底上一段 K 线：红涨绿跌，最后几根拉出一波急涨，右上角一个亮点收尾。
走势由 stock_core 同款的占位模型（阶段漂移 + 噪声 + 均值回归）在 Python 里复算，不是随手画的。
"""
import random
from pathlib import Path

from PIL import Image, ImageDraw

HERE = Path(__file__).resolve().parent
SRC = HERE / "_src"
SEED = 0x1980  # 股市 = id 80

W, H = 256, 320
BG = (11, 14, 20)
UP = (255, 72, 72)
DOWN = (72, 224, 96)
GRID = (28, 34, 48)
GLOW = (255, 220, 120)


def walk(n: int) -> list[tuple[int, int, int, int]]:
    rng = random.Random(SEED)
    price = 10000.0
    regimes = [(0, 40), (15, 60), (-15, 60)]
    reg, left = 1, 0
    candles = []
    for _ in range(n):
        o = price
        hi = lo = price
        for _ in range(16):
            if left == 0:
                reg, left = rng.randrange(3), rng.randrange(80, 241)
            left -= 1
            drift, vol = regimes[reg]
            noise = sum(rng.uniform(-1, 1) for _ in range(3)) / 3
            r = drift + vol * noise + (10000 - price) / 400
            price = max(1000.0, min(999900.0, price + price * r / 10000))
            hi, lo = max(hi, price), min(lo, price)
        candles.append((int(o), int(hi), int(lo), int(price)))
    return candles


def main() -> None:
    SRC.mkdir(exist_ok=True)
    img = Image.new("RGB", (W, H), BG)
    d = ImageDraw.Draw(img)
    for y in range(40, H, 40):
        d.line([(0, y), (W, y)], fill=GRID)
    candles = walk(20)
    lo = min(c[2] for c in candles)
    hi = max(c[1] for c in candles)
    top, bottom = 48, H - 48

    def py(p: int) -> int:
        return bottom - int((p - lo) * (bottom - top) / max(1, hi - lo))

    pitch = W // len(candles)
    for i, (o, h, l, c) in enumerate(candles):
        x = i * pitch + pitch // 2
        color = UP if c >= o else DOWN
        d.line([(x, py(h)), (x, py(l))], fill=color, width=2)
        y0, y1 = sorted((py(o), py(c)))
        d.rectangle([x - pitch // 2 + 2, y0, x + pitch // 2 - 2, max(y1, y0 + 2)], fill=color)
    last = candles[-1]
    gx, gy = (len(candles) - 1) * pitch + pitch // 2, py(last[3])
    for r, a in ((14, 40), (9, 90), (5, 200)):
        d.ellipse([gx - r, gy - r, gx + r, gy + r], fill=tuple(int(BG[k] + (GLOW[k] - BG[k]) * a / 255) for k in range(3)))
    out = SRC / "STOCK_face.png"
    img.save(out)
    print(f"STOCK: {out} {W}x{H}")


if __name__ == "__main__":
    main()
```

- [ ] **Step 2: 出图 + 追加 ORDER**

```bash
cd mods/th18.v1.00a/card-expand/assets/cards && python3 make_stock_art.py && \
python3 ../fit_card.py STOCK _src/STOCK_face.png --no-detect --trim 0 --margin 0 --bg '#0b0e14' && \
echo STOCK >> ORDER.txt && tail -2 ORDER.txt && python3 -c "from PIL import Image; print(Image.open('STOCK_max.png').size, Image.open('STOCK_min.png').size)"
```
Expected: `WHIRLWIND` / `STOCK` 两行；尺寸 `(256, 320) (64, 80)`。

- [ ] **Step 3: 重编 abcard.anm 并对账 sprite 号**

```bash
cd mods/th18.v1.00a/card-expand/native && make anm 2>&1 | grep -i "STOCK\|error\|mismatch"
```
Expected: 打印 `STOCK … 168 / 169`（措辞照脚本），无 error / mismatch。若打印的不是 168/169，把 `cards.js` 里 `"80"` 的两个 sprite 改成打印值。

- [ ] **Step 4: 看一眼图**

用 Read 工具打开 `mods/th18.v1.00a/card-expand/assets/cards/STOCK_max.png`：应是带零售同款边框、深蓝黑底、红绿 K 线、右上角一个暖光点。

- [ ] **Step 5: 提交**

```bash
git add mods/th18.v1.00a/card-expand/assets/cards/make_stock_art.py mods/th18.v1.00a/card-expand/assets/cards/ORDER.txt \
  mods/th18.v1.00a/card-expand/assets/cards/_src/STOCK_face.png mods/th18.v1.00a/card-expand/assets/cards/_art/STOCK.png \
  mods/th18.v1.00a/card-expand/assets/cards/STOCK_max.png mods/th18.v1.00a/card-expand/assets/cards/STOCK_min.png
git commit -m "art(card-expand): 股市卡图（程序生成 K 线），sprite 168/169

Co-Authored-By: Claude Fable 5.1 <noreply@anthropic.com>
Claude-Session: https://claude.ai/code/session_011gRYCoH8AKyQatW1gtKojT"
```
（若 `_src/` 在 `.gitignore` 里，`git add` 会拒绝——那就不加它，脚本能重生成。）

---

### Task 7: 文档——CARDS / NEXT / AUDIT §AF

**Files:**
- Modify: `mods/th18.v1.00a/card-expand/CARDS.md`（「其它」表）
- Modify: `mods/th18.v1.00a/card-expand/NEXT.md`（§0 表 + 新 §1p）
- Modify: `mods/th18.v1.00a/card-expand/AUDIT.md`（§AE 之后、§G 之前新开 §AF）

- [ ] **Step 1: CARDS.md**

「其它」表（`| 69 | 加倍 |` 所在的表）末尾加一行：

```
| 80 | 股市 | 主动（C 键）：选中时 **C 买入 50 金钱**、**Shift + C 卖出 50 金钱等值**（不足全卖）。股价按占位随机过程每 15 帧走一步、4 秒一根 K 线，HUD 右下常驻 16 根 K 线 + 现价 / 持仓 / 盈亏。持仓跨关、商店只认现金、局末清零。充能 30 帧（节流） | `on_activate` 读 `INPUT_HELD` 低速位分买卖、写 `MONEY`；`on_tick_2` 每 15 帧 `stock_step(ce_rand())` + 33 个 UI 层 VM（`stock_rect` script104 就地改 pos/scale/color）；`on_draw` 三行 `ce_hud_text`；`on_delete`（新回调）删 VM。纯逻辑 `stock_core.c`（主机单测）；`stock_shock()` 预留事件联动 | `cards/stock.c` + `stock_core.c/.h` | 🔧 待实跑（AUDIT §AF）|
```

- [ ] **Step 2: NEXT.md**

§0 表在 `| **23 旋风斩（id 79）** |` 那行**之前**加：

```
| **24 股市（id 80）** | 🔧 **待实跑**（AUDIT §AF）—— 第一张「新机制」卡：C 买 / Shift+C 卖 50 金钱，K 线面板常驻 HUD 右下。**面板坐标 / 字号全是猜的**，实跑第一件事是把它挪到不挡东西的位置（§1p）。顺带 SDK：状态块 512、`on_delete` 回调 |
```

§1o 之前加一节：

````
## 1p. 股市（id 80，2026-09-08，待实跑）

主动卡，不开新断点。`cards_dev.js` 起手最后一格已换成 80（原 67）。实现 `native/cards/stock.c` + `stock_core.c`（主机单测 10 组）；审计 AUDIT §AF。

进关卡后日志应有：

```
sdk: 80 bound (.active_recharge = 30, .ctor = ctor, .on_activate = on_activate, .on_tick_2 = on_tick_2, .on_draw = on_draw, .on_run_reset = on_run_reset, .on_delete = on_delete)
stock: opened at 100.00
stock: panel spawned (bg id … at 1056,862 size 432x180; candles x 848..1264 y 776..872; 2x UI space)
```

体感：**HUD 右下**一块半透明黑底板，里面一排红绿蜡烛每 4 秒多一根、最右那根每 0.25 秒变形；底板下方三行字 `STK 100.00 ^` / `HOLD 0.00 = 0` / `P/L +0`。
选中股市（D 键切到它）→ 场上有敌人时按 C：金钱 −50、`HOLD` 变 `0.50`、`se_cardget` 音、日志 `stock: buy 50 @ 100.00 -> hold 0.5000 (money …)`；
充能条空了约半秒回满。按住 Shift 再按 C：金钱回来（差 ≤ 1）、`se_bonus` 音、`stock: sell …`。钱 < 50 按 C / 空仓按 Shift+C → 无效音 + `card: refused (no cash / no holding)`，充能不掉。
过关进店：面板停住（行情冻结）、持仓还在；下一关接着走。局末 `stock: run reset (…)`，面板消失。
倒吊人摧毁它 → 面板立刻消失（`on_delete`）。

**要调的**（全在 `stock.c` 顶部常量）：底板 / 蜡烛位置若压到卡组第三行图标（装备行 y≈398..430）或超出窗口，改 `UI_*`；
三行字若重叠 / 太大，改 `TEXT_Y*`（HUD 字高按 Momoyo 的 `x1.5` 目测）；蜡烛矩形若以左上角为锚而不是中心，几何要整体偏 `(w/2, h/2)`（AF6）。
崩溃优先怀疑：33 个 UI VM 起不出来（日志 `FAILED to spawn`，AF4）、`ce_anm_set_scale` 被脚本插值器覆盖（脚本里没有 scaleTime，不该）。
行为怀疑：Shift+C 被当成买（AF2：`INPUT_HELD` 与 `INPUT_PRESSED` 同帧一致性）。
````

- [ ] **Step 3: AUDIT.md §AF**

在 `## G. OPEN` 之前插入：

````
## AF. 股市（id 80）—— C 买 / Shift+C 卖，K 线面板（2026-09-08）

**没有新断点、不改任何字节。** 新引擎访问只有一处读：`INPUT_HELD` `0x4ca428`。新 SDK 面：状态块 256 → 512、`on_delete` 回调（`+0x50` 桩在 `ce_state_free` 之前调）。
纯逻辑 `stock_core.c` 主机单测（确定性 / 钳位 / 阶段 / K 线 / 买卖 / 来回损耗 / 几何）。

| # | claim | 结论 |
| --- | --- | --- |
| AF1 | replay 安全 | **CONFIRMED（构造）** —— 输入只用 replay 重放的位（用卡 `0x400` 走引擎分派、低速 `0x008` 读 `INPUT_HELD`，回放时二者都由 replay 写入，`04-active-cards.md` §1）；随机数 `ce_rand()` = `REPLAY_SAFE_RNG` `0x4cf288`（零售 Suwako / Yachie / Miko 同源）；步进只在 `on_tick_2`（管理器 tick，回放同样跑）。全整数 |
| AF2 | `on_activate` 里读到的低速位与这次 C 键同帧 | **🟡 待实跑** —— `Input__compute_edges` `0x42abc0` 每帧先算好 `INPUT_HELD` / `INPUT_PRESSED`，Player tick（优先级 0x17）里 `c_press` 才被调，所以读的是同一帧的 held。风险只在「按 C 的同一帧才按下 Shift」这种人手做不到的情形 |
| AF3 | 写 `MONEY` 的安全 | **照先例** —— 黄金小丑（§W）/ 倒吊人直接写 `0x4ccd34`，HUD 每帧重读；没有钳位（引擎自己也没有发现上限，SM §1）。买时先查 `>= 50` 再减，卖只加：单测 `test_round_trip` 钉住「现金永不为负」 |
| AF4 | 33 个 UI 层 VM 起得出来 | **🟡 待实跑** —— `ce_anm_spawn_ui` 走 `0x409670`（真银圣剑 / 衔尾蛇各起 1 个已验）；VM 池是 fast 数组（`id & 0x7fff` 下标，`01-vm-instantiate.md`），容量未查。失败返回 0 → 整组删掉、日志一行、下一帧重试；不会半张图 |
| AF5 | `drawRect` + `scale` = 像素尺寸 | **照先例** —— 青眼血条 script87 `drawRect(1,1); scale(56, 4)` 画出 56×4 的条；`ce_anm_set_scale` 写 `vm+0x54`（O29k）。脚本里没有 `scaleTime`，插值器不会盖 |
| AF6 | 矩形锚点是中心 | **🟡 待实跑** —— ANM 默认 anchor 居中（thpages）；`stock_layout` 按中心算 cx/cy。若实跑发现是左上角锚，几何整体偏 (w/2, h/2)，改 `apply_rect` 一处 |
| AF7 | UI 层坐标空间是 2× | **推断（X5）** —— HUD 图标根 VM `pos(1056, 537)`、x 步进 80 对应 640 空间的 (528, 268) / 40 px 图标；`resolutionMode(2)` 同款。面板常量按 2× 写；`ce_hud_text` 是 640 字面量空间（AC6），两套分开 |
| AF8 | VM 生命周期不漏 | **CONFIRMED（构造）** —— 每帧 `vms_alive` 认领（任一没了整组重建，保证底板在下）；`on_run_reset` / `on_delete` 都 `kill_vms`。`on_delete` 是本次新加的 `+0x50` 回调：先调卡再 `ce_state_free`，倒吊人摧毁（`ce_destroy_card` → `vt+0x50`）与局末 recount 都经过它 |
| AF9 | ctor 幂等 | **CONFIRMED（构造）** —— `inited` 位；每关开场引擎再调 ctor 不会重开盘（SDK §9 ★）。不用 `owned[]`：mode 1 装载不调 ctor，状态本来就是零 |
| AF10 | 无敌人 / 对话中不能交易 | **引擎门控** —— `04-active-cards.md` §2 四重门；充能也停。零售主动卡共性，用户已接受 |
| AF11 | 整数不溢出 | **CONFIRMED（单测）** —— 交易 / 市值用 int64 中间量；`test_value_pnl` 1 万股 × 9999.00；价格步进 `price × r` ≤ 999900 × 2500 < 2^31 但仍走 int64 |
| AF12 | 状态块 512 的副作用 | **CONFIRMED** —— `s_mem[256][512]` 静态 128 KB，`ce_state_alloc` 的上界检查用同一常量（单测）；`_Static_assert(sizeof(stock_card_t) <= 496)` |
| AF13 | 与其它主动卡共存 | **引擎语义** —— 只有选中的主动卡收到 `c_press`；D 键切卡由引擎管（`0x800`）。面板常驻不随选中变 |

**实跑要看的**：见 [`NEXT.md`](NEXT.md) §1p。
````

- [ ] **Step 4: 文档检查 + 提交**

```bash
python3 tooling/check-docs.py 2>&1 | tail -2
git add mods/th18.v1.00a/card-expand/CARDS.md mods/th18.v1.00a/card-expand/NEXT.md mods/th18.v1.00a/card-expand/AUDIT.md
git commit -m "docs(card-expand): 股市（id 80）登记 / 交接 / 审计 §AF

Co-Authored-By: Claude Fable 5.1 <noreply@anthropic.com>
Claude-Session: https://claude.ai/code/session_011gRYCoH8AKyQatW1gtKojT"
```
Expected: 违规数仍是改前那 5 处（都不在本次文件）。

---

## 自查记录（写完计划后对照 spec）

- spec §1 键位 / 主动卡 / 充能 30 → Task 5 `CE_CARD(80, .active_recharge = 30, …)`、`on_activate` 读 `CE_INPUT_FOCUS`；`INPUT_HELD` 进 `engine.h` → Task 1。
- spec §2 交易规则 → Task 3（持股单位改成 0.0001 股：spec 写 0.001 时高价位来回损耗可到 10 金钱，达不到「≤ 1」；换算常量 `STOCK_HOLD_PER_MONEY` 一处，spec §2 表随 Task 7 一起改成 0.0001）。
- spec §3 模型 / 接口 / `stock_shock` / 每 15 帧 / 16 步一根 / 16 根 → Task 2。
- spec §4 33 个 VM、`stock_rect` 脚本、红涨绿跌、三行文字、按需重建 → Task 4（几何）+ Task 5（胶水）。
- spec §5 生命周期表（ctor / on_tick_2 / on_activate / on_draw / on_stage_start 不动 / on_run_reset / +0x50）→ Task 5；`+0x50` 需要卡侧回调 → Task 1 的 `on_delete`（spec 写「同上」但 SDK 原本不给卡机会，这是本计划补的一块）。
- spec §6 文件 / 单测 / `CE_STATE_BYTES` / 文档 → Task 2–7。
- 类型一致性：`stock_rect_t{cx,cy,w,h,up}`、`stock_layout(s,x0,y0,w,h,body,wick)`、`stock_visible` 返回 int、`STOCK_HOLD_PER_MONEY` 为 `1000000LL` 在 Task 2 头文件定义、Task 3/4/5 一致引用。
