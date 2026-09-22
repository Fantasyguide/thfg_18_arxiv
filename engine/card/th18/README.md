# TH18 卡牌/能力系统 — 一手结论

> **版本**：TH18 v1.00a（`th18.exe`，database_id `th18`）。各篇裸地址默认属该版本。
> 这里只放 **TH18 自己一手验证过的结论**；跨版本断言在 [`../OVERVIEW.md`](../OVERVIEW.md)。
> 证据链纪律见 [`../../../METHOD.md`](../../../METHOD.md)，排版规范见 [`../../../DOCSTYLE.md`](../../../DOCSTYLE.md)。

## 怎么读

按依赖顺序，前四篇是**机制主线**，后四篇是**数据与对账**：

| # | 文档 | 讲什么 |
| --- | --- | --- |
| 01 | [`01-object-model.md`](01-object-model.md) | 对象与数据布局：`zCardBaseClass` / `zVTableCard` / `zAbilityManager` / `zTableCardData`、`flags` 位义、两个计时器的订正 |
| 02 | [`02-lifecycle.md`](02-lifecycle.md) | 生命周期：分配的四种 mode、即时卡为什么拿不到手、局末回收、存档与 replay |
| 03 | [`03-hooks.md`](03-hooks.md) | **21 个虚表槽 × 引擎调用点全表**，含两处 ExpHP 误名的订正 |
| 04 | [`04-active-cards.md`](04-active-cards.md) | **C 键释放全链路**：输入位 → 门控 → `c_press` → 充能 → 状态机 → HUD/replay |
| 05 | [`05-shop-and-money.md`](05-shop-and-money.md) | **商店与金钱系统**：收支穷举、offer 生成、`dmode` 规则、定价与购买、空白卡、道具侧 |
| 06 | [`06-resource-economy.md`](06-resource-economy.md) | 残机/符卡库存三元组与资源卡怎么喂它 |
| 07 | [`07-registry.md`](07-registry.md) | `zTableCardData[]` 58 项 dump + 字段语义 |
| 08 | [`08-catalog.md`](08-catalog.md) | 逐卡效果目录（56 张可获得卡）|
| 09 | [`09-community-crosscheck.md`](09-community-crosscheck.md) | 与 THBWiki 的逐项对账 |
| 10 | [`10-extensibility-limits.md`](10-extensibility-limits.md) | ★ **新增卡牌的硬边界**：被原生卡集合写死的地方（本篇 12 处 + 11 篇 21 处）|
| 11 | [`11-sentinels-56-57.md`](11-sentinels-56-57.md) | ★ **两个哨兵 id 56/57 的真身**：查表回退行 / 存档里的空槽值 / 卡背；含**为什么写死**的全二进制穷举 |
| 15 | [`15-selected-card-hud.md`](15-selected-card-hud.md) | ★ **左下角「当前（选中）主动卡」UI 全链路**：`abcard.anm` script0–4、`mgr+0x3c` VM、`visible` 位判据、出场 20 帧 / 压暗 8 帧到 64 的一手数值 |
| — | [`OPEN-questions.md`](OPEN-questions.md) | 开放问题与验法 |

> 12–14 三篇（`12-card29-aya-speedqueen` / `13-mallet-clear-and-th17-charge` / `14-clear-range-barrier`）
> 不在上表里，是历史欠账；编号已占，新篇从 15 起。

原始素材（社区 wikitext）在 [`_sources/`](_sources/thbwiki-cards.txt)，**不是结论**。

> **消弹侧在别的子系统**：卡里那几支「消弹」到底撒不撒实体道具、`mode` 该给几，
> 一手结论与本仓三处注释的订正都在 [`../../bullet/th18/01-cancel-to-item.md`](../../bullet/th18/01-cancel-to-item.md)
> （§4.1 逐卡 mode、§7 订正 `pofv.c` / `sdk.h` / `judgment.c` 与 spec §6.4 的错）。**那几处源码本轮没动。**

## ⚠️ 代码块的约定：伪代码 ≠ Ghidra 输出

各篇里的 C 代码块是**加了语义名的伪代码**，不是反编译器的原样输出。

**卡牌这 268 个函数现在已经绑好类型了**（`tooling/ghidra/bind_types.py`，2026-09-01），
所以库里看到的是 `self->flags`、`(self->__timer_2__prolly_bomb_time).current`，
而不再是 `*(int *)(param_1 + 0x50)`。但**子类自有字段（≥0x54）仍显示成 `self->field_0x58`**
——因为那些字段还没逐卡反出来命名。

**所以各篇一律保留裸偏移**——写成 `card->state(+0x54)`、`card->recharge_cur(+0x38)`、`player+0x620`，
让每一行都能直接对回反编译输出（无论绑没绑）。看到 `->名字(+0x…)` 这种写法，括号里的才是可核对的事实。

⚠️ **别把 ExpHP 的字段名当结论**：绑定之后 `+0x34` 会显示成
`__timer_2__prolly_bomb_time`，而一手结论是它才是**充能倒计时**
（[`01-object-model.md`](01-object-model.md) §3）。名字来自上游，语义以我们的证据为准。

## 纪律

- 每条结论按 [`../../../METHOD.md`](../../../METHOD.md) 写全五段链条（发现 → 推测 → 验证 → 结论 → 证据）。
- 引用 TH16 结论时显式写 `th16:` 前缀，**严禁把 TH16 地址/偏移写成 TH18 事实**。
- 「超过社区」的宣称要过额外闸门（一手到底 / 对抗证伪 / 量纲常识 / 交叉对名），复核前一律 🟡。
- 想做运行时改造，先读 [`../../../mods/th18.v1.00a/card-rework/ROADMAP.md`](../../../mods/th18.v1.00a/card-rework/ROADMAP.md)：
  现有结论足以指导定点实验；运行时底座已由
  [`../../../mods/th18.v1.00a/mouse-control/README.md`](../../../mods/th18.v1.00a/mouse-control/README.md) 实跑验通，
  卡牌侧的第一个注入产物 [`../../../mods/th18.v1.00a/card-expand/README.md`](../../../mods/th18.v1.00a/card-expand/README.md)
  已实跑（搬表 + 分配器 + 扩容）；存档层与图鉴层未做。

## 本轮（2026-09-01）新增与订正

- 21 槽虚表**全部**落到确定的引擎调用点（此前只有 9 个）→ [`03-hooks.md`](03-hooks.md)。
- ExpHP 两处误名订正：`+0x3c get_bomb_timer` → `get_recharge_remaining`；
  `+0x30 recharge` → `on_enemy_dropped_items`。两条都有**双独立证据**。
- 卡的两个 `zTimer` 语义确认互换（HUD 充能条的分子分母是决定性佐证）。
- 分配的四种 mode、`flags` bit0 的来源、以及**即时卡靠 ctor/dtor 返回值当场自毁**的机制。
- 空白卡（`CardChimata`）效果**一手闭合**：实现在 `AbilityShop` 的 state 3。
- 金钱系统收支穷举，并发现 **`MONEY` 同时是计分乘数**。
- 死亡的金钱惩罚 `min(MONEY/3, 100)`、默认决死窗口 8 帧、初始卡槽 1→2→3 的解锁条件。
- 新增卡牌的**硬边界全表**（33 处）与**两个哨兵 id 56/57 的真身** → [`10`](10-extensibility-limits.md) / [`11`](11-sentinels-56-57.md)。
- 全二进制线性反汇编穷举（204116 条指令）给出「为什么写死」：`id 56` 既是集合大小、
  又是**写进存档的空槽字面值**；`TableCardData__get` 被内联 25 次是主要放大器。
- 注册表两处订正：**表不按 id 排序**；`+0x20`（菜单可见）与 `+0x24`（初期解禁）反出来 → [`07`](07-registry.md)。
