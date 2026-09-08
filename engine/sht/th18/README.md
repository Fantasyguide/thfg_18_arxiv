# engine/sht/th18 — TH18 的 SHT 一手

> **版本**：TH18 v1.00a（`th18.exe`，imagebase `0x400000`）。本文裸地址默认属该版本；引用其他版本须写成 `th16:0x…`。

TH16 那一批（[`../th16/`](../th16/README.md)）搞的是「func_\* 跳转表 / flags 段」这些**语义**问题。
TH18 这一批的出发点不同：card-expand 要给新卡加一个**会开火的子机**，于是必须回答
「装备卡的 shooter 数据到底存在哪、能不能安全地往里加一组」。

| 文档 | 回答什么 |
| --- | --- |
| [`01-file-layout-and-shooterset-index.md`](01-file-layout-and-shooterset-index.md) | `pl0X.sht` 的整体布局、装载与解析、23 组 shooterset 各是谁、尾部 17 个空位 |
| [`02-shooter-record.md`](02-shooter-record.md) | 单条 shooter（`0x5c`）的字段图，以及一条 shooter 怎么变成一发自机弹 |
| [`03-func-tables-timers-and-dead-fields.md`](03-func-tables-timers-and-dead-fields.md) | 四张行为函数表 18 项全反 + 与 TH16 对照、15/120 帧计时器、头部判定字段无人读、子机位置块三角索引、**flags 段 `+0x4c` 在 TH18 有读取点**（咲夜倾斜系数）|

面向 THBWiki 的成稿：[`../WIKI-DRAFT.md`](../WIKI-DRAFT.md)（2026-09-06）。

**这两篇一起关掉了** [`../../card/th18/OPEN-questions.md`](../../card/th18/OPEN-questions.md) §1
（「装备卡的子机 shooter 数据存在哪」——该篇列的三个互斥假设里，**假设 1 成立**）。

装备卡怎么调用这套东西（`allocate_option` → `tick_shooters_for_ability_card`）在
[`../../card/th18/03-hooks.md`](../../card/th18/03-hooks.md) §5；逐卡的索引表在
[`../../card/th18/08-catalog.md`](../../card/th18/08-catalog.md) §D。
