# 股市（card-expand id 80）—— 设计

> **版本**：TH18 v1.00a（`th18.exe`，imagebase `0x400000`）。本文裸地址默认属该版本；引用其他版本须写成 `th16:0x…`。
> 日期：2026-09-08。状态：用户已定（§0）。归属：`mods/th18.v1.00a/card-expand/`。
> 引擎一手：[`engine/card/th18/04-active-cards.md`](../../../engine/card/th18/04-active-cards.md) §1–§2（C 键链路与输入位）、
> [`engine/card/th18/05-shop-and-money.md`](../../../engine/card/th18/05-shop-and-money.md) §1（`MONEY`）、
> [`engine/anm/th18/01-vm-instantiate.md`](../../../engine/anm/th18/01-vm-instantiate.md)（VM 字段）。
> SDK 用到的原语：[`SDK.md`](../../../mods/th18.v1.00a/card-expand/SDK.md) §9（主动卡）、§14（`ce_hud_text`）、§15（UI 层 VM）。

## 0. 一句话

**主动卡**：带着它，屏幕 HUD 右下角常驻一块股市面板（现价 + 16 根 K 线 + 持仓 / 盈亏）。
选中它时 **C = 买入 50 金钱**、**Shift + C = 卖出 50 金钱等值**。股价由一个回放安全的随机过程驱动，
玩家靠看图择时赚差价。

用户已定（2026-09-08）：**键位方案 = C 买 / Shift+C 卖**（三个候选里最简单、且完全落在 replay 记录的输入位内）/
**每次固定 50 金钱折算股数** / **行情 = 纯模拟 + 预留外部冲击接口**（联动事件以后再接）/
**面板放 HUD 右下、常驻** / **持仓跨关保留、商店只看现金** / **行情模型本次是占位**，接口固定、内部以后换更科学的。

## 1. 为什么是主动卡、为什么只能用这几个键

replay 只重放 `INPUT_HELD` `0x4ca428` 里的位：射击 `0x001`、炸弹 `0x002`、低速 `0x008`、方向 `0xf0`、用卡 `0x400`、切卡 `0x800`
（`04-active-cards.md` §1，逐位一手）。任何别的键进不了 replay 流，回放时买卖就对不上、金钱与后续商店购买全乱。
所以交易只能由这些位组合出来，而「用卡位」的分派引擎已经做好：**选中的主动卡**才收到 `c_press`，D 键切卡由引擎管。

于是：`CE_CARD(80, .active_recharge = 30, .on_activate = …)`，`on_activate` 里读 `INPUT_HELD & 0x008` 区分买卖。
`active_recharge = 30`（半秒）只是节流；充能条顺便当「可下单」提示。

引擎门控照旧（`04-active-cards.md` §2 的四重门）：**对话中、场上没有敌人时下不了单，充能也不走**。零售主动卡共性，不改。

`engine.h` 新增 `CE_ADDR_INPUT_HELD 0x4ca428`（出处同上；已有 `CE_ADDR_INPUT_PRESSED 0x4ca434`）。

## 2. 交易规则（`stock_core`，纯整数）

| 量 | 单位 | 类型 |
| --- | --- | --- |
| 价格 `price` | 0.01 金钱 | `int32`，开盘 `10000`（= 100.00），硬钳位 `[1000, 999900]` |
| 持股 `hold` | 0.001 股 | `int32` |
| 成本 `cost` | 金钱 | `int32`，累计买入金额 − 累计卖出所得（盈亏显示用）|

- **买**（C）：`MONEY < 50` → `ce_refuse("stock", "no cash")`。否则 `MONEY −= 50`，`hold += 50 × 100 × 1000 / price`（= `5000000 / price`），`cost += 50`。
- **卖**（Shift + C）：`hold == 0` → `ce_refuse("stock", "no holding")`。否则 `q = min(hold, 5000000 / price)`，
  `MONEY += q × price / 100000`（向下取整，零头等于手续费），`hold −= q`，`cost −= 所得`。
- 现金永不为负、卖出永不超持股、一次买卖来回损耗 ≤ 1 金钱：三条都是单测。
- 成交放零售音效（买 / 卖各一个，用现有 id；实施时挑），日志 `stock: buy 50 @ 123.45 -> hold 0.405 (money 1234)` / `stock: sell …`。
- `MONEY` 上限：引擎有钳位就照它（实施时查 `05-shop-and-money.md` §1）；没有就不钳。

## 3. 行情模型（占位；接口固定，内部随时换）

接口（`stock_core.h`）：

```c
typedef struct stock_state stock_state_t;             /* 全部 int32，无浮点 */
void     stock_init(stock_state_t *s);                 /* 开盘 100.00，清缓冲 */
void     stock_step(stock_state_t *s, uint32_t rnd);   /* 走一步；rnd = 调用方给的 32 位随机数 */
void     stock_shock(stock_state_t *s, int permille);  /* 价格 ×(1000+permille)/1000，以后接事件用；本次无调用者 */
int      stock_buy (stock_state_t *s, int32_t *money); /* 返回 0 拒绝 / 1 成交 */
int      stock_sell(stock_state_t *s, int32_t *money);
```

随机数由调用方注入：胶水层传 `ce_rand()`（`REPLAY_SAFE_RNG` `0x4cf288`，弹幕与零售卡同源，replay 天然同步），单测传自己的序列。

占位算法（**每 15 帧一步**，1 秒 4 次）：

- `price += price × r / 10000`，`r`（基点）= `阶段漂移 + 阶段波动 × 噪声 / 3 + 均值回归`；噪声 = 三个 `[−1, 1]` 均匀数之和（近似正态）；
  均值回归 = `(10000 − price) / 400`（温和拉回 100，防归零或飞天）。
- **阶段**：平 / 牛 / 熊，漂移 `0 / +15 / −15`、波动 `40 / 60 / 60`；每段持续 20–60 秒（80–240 步）后用同一只随机数重抽。期望收益约零。
- **K 线**：每 16 步（240 帧，4 秒）收一根 OHLC，环形缓冲 16 根（64 秒）。缓冲连同「当前未收盘那根」一起给显示层。

## 4. 显示（HUD 右下）

两层，各走各的原语：

**K 线 = 33 个 UI 层 VM，常驻就地改**。`ability.anm` 追加一个脚本 `stock_rect`：

```
sprite(-1); layer(26); resolutionMode(2); zWriteDisable(1); blendMode(0);
drawRect(1.0f, 1.0f); color(255,255,255); alpha(255); stop();
interruptLabel(1); delete();
```

（青眼血条 script87 已用 `drawRect` 画过纯色矩形；`layer(26); resolutionMode(2)` 是 HUD 图标同款，真银圣剑数字 script99 验证过。）
用 `ce_anm_spawn_ui` 起 1 个底板（深色、alpha 160，约 200×90）+ 16 根蜡烛 × {实体, 影线} = 33 个 VM，anm id 存私有状态。
每步用 `ce_anm_set_pos / set_scale / set_color` 改：实体宽 7 高 = |收 − 开|，影线宽 1 高 = 最高 − 最低，纵向按 16 根的最高 / 最低价缩放。
**红涨绿跌**，一个常量可换。VM 不在了（场景切换）在 `on_tick_2` 里按需重建，与 `ce_option_get` 的认领套路同型。

**文字 = `on_draw` 里 `ce_hud_text` 三行**（HUD 的 ASCII 字，只用字母数字）：现价 + 涨跌箭头（`^` / `v`）、持股与市值、累计盈亏。

⚠️ 底板坐标、蜡烛像素、字号 / 三行位置**要实跑一次才能定死**（真银圣剑 / 衔尾蛇数字同款问题）；日志打面板锚点，先按 640×480 布局猜一版。

## 5. 生命周期

| 时点 | 做什么 |
| --- | --- |
| `ctor` | `ce_fresh_acquire` 为真才 `stock_init`（ctor 每关会再被调一次，SDK §9 ★）|
| `on_tick_2` | 认领 / 补建 33 个 VM；每 15 帧 `stock_step(ce_rand())` 并刷 VM；商店 / 菜单里不跑，行情冻结、图停在原地 |
| `on_activate` | 读 `INPUT_HELD & 0x008` → `stock_sell` / `stock_buy`；拒绝走 `ce_refuse`；返回 0（瞬发）|
| `on_draw` | 三行文字 |
| `on_stage_start` | 不动仓、不动行情（持仓跨关）|
| `on_run_reset` | 删 33 个 VM、清状态 |
| `+0x50`（删对象，含倒吊人摧毁）| 同上 |

私有状态：OHLC `int32` × 4 × (16 根 + 当前未收盘) = 272 字节，33 个 anm id 132 字节，价格 / 持股 / 成本 / 阶段 / 两个计时约 24 字节，
合计约 428 字节。`ce_state` 现有块是 256 字节（`CE_STATE_BYTES`，去掉 SDK 块头 16 字节实际 240），装不下。
**本次把 `CE_STATE_BYTES` 提到 512**（`sdk_core.h` 一个常量；静态表 `s_mem[256][512]` 从 64 KB 变 128 KB，用户已定 2026-09-08）。
这个上限是 SDK 自己定的，不是引擎约束。

## 6. 文件与测试

```
native/cards/stock.c          胶水：CE_CARD(80)、输入、VM、文字、日志
native/cards/stock_core.c/.h  模拟 / 交易 / K 线缓冲（make test-host）
assets/ability/scripts/NNN_stock_rect.anm.txt
patch/th18/cards.js           id 80 登记（category 0；卡图先占位）
```

单测（`stock_core_test.c`）：同一随机序列两次跑结果逐位相同；价格永在钳位内；阶段时长落在 80–240 步；
买后现金 ≥ 0、卖出 ≤ 持股、买卖来回损耗 ≤ 1；缓冲滚动后最高 / 最低价正确；`stock_shock` 后价格与钳位。

SDK 改动：`sdk_core.h` `CE_STATE_BYTES 256 → 512`，`SDK.md` §4 的「上限 256 字节」同步改；`make test-host` 里状态槽的单测跟着改常量。

文档：`CARDS.md` / `MAP.md` / `NEXT.md` 加条目，`AUDIT.md` 新开一节（无新断点，重点审 `INPUT_HELD` 读法、VM 数量与 UI 列表容量、`MONEY` 写入）。
不开新断点、不改游戏资源文件。

## 7. 明确不做

商店里显示持仓、浮盈计入金钱、事件联动（只留 `stock_shock`）、多只股票、杠杆、做空。
