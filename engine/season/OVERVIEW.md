# season — 季节系统（TH16 的招牌机制）
> **版本**：跨版本。本文出现的地址一律带版本前缀（如 `th16:0x4a5808`）。

TH16《天空璋》的「季节」是**第二条资源轴**，和主炸（Bomb）平行：

```
捡季节点（item type 16）──► CURRENT_SEASON_POWER 涨 ──► 按 SEASON_POWER_LEVEL_* 分档
                                                            │
                                        按 C（INPUT_RISING_EDGE & 0x800）│
                                                            ▼
                              第二个 Bomb 对象（SUBSEASON_BOMB_PTR，is_season=1）
                              = 清弹屏障 + 短无敌，按季节（春/夏/秋/冬/土用）行为不同
```

一句话：**季节 = 一套「道具充能 → 分档 → 按 C 释放」的副炸弹**，与主炸共用 `Bomb` 引擎
（第二个对象），子机侧另有 `subseason_options` 一组。

## 断言 × 版本矩阵

| 断言 | th15 | th16 | th17 | th18 | 证据 |
| --- | :---: | :---: | :---: | :---: | --- |
| 有季节点道具（item type 16 `ITEM_SEASON`） | ❌ | ✅ | ❌ | ❌ | [th16/06 §2](../player/th16/06-resource-economy.md)、[th18/01](th18/01-season-residuals.md) §1 |
| 有季节槽全局 `CURRENT_SEASON_POWER` / `MAX_SEASON_POWER` | ❌ | ✅ | ❌ | ❌ | `th16:0x4a5808` / `th16:0x4a580c` |
| 有季节子机数组 `zPlayerInner.subseason_options` | ❌ | ✅ | ❌ | ❌ | `th16` 结构体（player+0x3e0）|
| 季节子机可升级（等级表 + 充能比） | ❌ | ✅ | ❌ | ❌ | `th16:0x4a5810` / `th16:0x4a583c` / `th16:0x43df20` |
| 季节解放 = 第二个 Bomb 对象（`is_season`） | ❌ | ✅ | ❌ | ❌ | [player/th16/02](../player/th16/02-season-release-and-bombs.md)、`th16` zBomb+0xd4 |
| 季节释放走独立输入位 `0x800` | ❌ | ✅ | ❌ | ❌ | [th16/02 §0](../player/th16/02-season-release-and-bombs.md) |
| 季节点掉落按「累计伤害」+「死亡计时衰减」两条通道 | ❌ | ✅ | ❌ | ❌ | [th16/01](th16/01-season-data-and-tables.md) §5（`dropSeason`/`seasonItemDamage` 一手反编译）|
| 掉落有「主掉落 + 额外通道」两段结构 | ❌ | ✅ | ✅ | ✅ | th16 `extra_counts[0x11]` / th17 `ex_drop[0x22]` / th18 `ex_drop[0x14]`+`timed_ex_drop[0x14]`（见 [th18/01](th18/01-season-residuals.md) §1b）|

> **图例**：✅ 该版本一手验过 · 🟡 待验 · ❌ 已知不同/不存在 · ❓ 存疑

## 版本脉络

- **TH16**：完整实现（季节槽 + 季节点 + 季节子机 + 升级 + 解放）。
- **TH15 / TH17 / TH18**：**没有**这套系统。TH17 里 `zItem.__might_be_unused__force_autocollect__from_th16`
  是 TH16 留下的**字段残骸**（ExpHP 自己命名成「可能没用」），TH18 里该偏移**零引用**。
- TH18 的**道具掉落**结构里多了一条 `timed_ex_drop` 通道：`n = ex_drop + timed_ex_drop×(T−timer)/T`
  —— 与 TH16 `dropSeason` **同形同向**（出场 T 帧内给满、之后衰减到 `ex_drop`），但它是**出场计时**通道、
  产物是分数/金钱/卡，不是季节点（[th18/01](th18/01-season-residuals.md) §1b′）。

## 相关一手文档

- TH16 季节**全链一手**（6 档阈值 / 10 张释放档位表 / 季节点道具 anm / 掉落两通道 / `dropSeason` ECL 普查）：[`season/th16/01`](th16/01-season-data-and-tables.md)
- TH16 季节释放（双炸弹 + 输入位 + 消耗规则）：[`player/th16/02`](../player/th16/02-season-release-and-bombs.md)
- TH16 子机 / 子季系统：[`player/th16/04`](../player/th16/04-options-subshot-system.md)
- TH16 资源经济（季节点在哪一档）：[`player/th16/06`](../player/th16/06-resource-economy.md)
- TH18 侧残骸审计：[`season/th18/01`](th18/01-season-residuals.md)
- 跨版本台账：[`_shared/version-residuals.md`](../_shared/version-residuals.md)
