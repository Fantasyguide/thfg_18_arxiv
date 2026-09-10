# chapter — 章节（波次）系统与章节奖
> **版本**：跨版本。本文出现的地址一律带版本前缀（如 `th15:0x432ec0`）。

「章节」是 ZUN 引擎里一个**横跨多作、名字一直在、系统时有时无**的东西：
一组**每章计数器**（擦弹数、刷怪数、击破数、重开次数）+ 一个**边界事件** + 一次**边界结算**
（章节奖）+ 一次**状态快照**。TH15《绀珠传》把它做成了玩法主轴（章节奖 + Pointdevice 重开点），
TH16/TH18 的全局表里还留着 `GRAZE_IN_CHAPTER` 这类字段，但没有结算端。

理解它的关键：**边界不是引擎猜出来的**。TH15 由 ECL 脚本用 opcode 524 `setChapter(n)`
显式宣告——「一个 boss 的非符 + 一张符卡算一波」这种分法，是**脚本怎么调 524** 决定的，
不是引擎按阶段/时间切出来的。

## 断言 × 版本矩阵

| 断言 | th15 | th16 | th18 | 证据 |
| --- | :---: | :---: | :---: | --- |
| 存在全局对象 zGlobals，内含 `CURRENT_CHAPTER` / `TIME_IN_CHAPTER` / `GRAZE_IN_CHAPTER` / `ENEMIES_*_IN_CHAPTER` | ✅ | 🟡 | 🟡 | [th15/01 §1a](th15/01-chapter-and-bonus.md)、[th18/01 §1](th18/01-chapter-residuals.md) |
| 章节边界由 ECL opcode **524 `setChapter`** 显式宣告 | ✅ | ❓ | ✅ | [th15/01 §1](th15/01-chapter-and-bonus.md)、[th18/01 §3](th18/01-chapter-residuals.md) |
| 边界事件走「写 GameThread 标志位 + 下一帧消费」的两段式 | ✅ | ❓ | ❌ | [th18/01 §1](th18/01-chapter-residuals.md) |
| 每章有「刷出 / 击破」计数并参与结算 | ✅ | ❓ | ❌ | [th15/01 §1a](th15/01-chapter-and-bonus.md) |
| 章节奖公式 = 本章擦弹 × 击破率 × 50，向下取整到 10 | ✅ | — | ❌ | [th15/01 §2a](th15/01-chapter-and-bonus.md) |
| 刷怪数为 0 的章不结算 | ✅ | — | — | [th15/01 §2](th15/01-chapter-and-bonus.md) |
| 结算后才清零计数器（顺序固定） | ✅ | ❓ | ❌ | [th18/01 §2](th18/01-chapter-residuals.md) |
| 边界同时打一次 Pointdevice 快照（重开点） | ✅ | — | ❌ | [th15/01 §3](th15/01-chapter-and-bonus.md) |
| bonus ≥ 1e6 才发碎片，且残机/符卡碎片由 MODEFLAGS 分流 | ✅ | — | — | [th15/01 §2b](th15/01-chapter-and-bonus.md) |
| 章节点板是一块常驻 anm 面板 + AsciiManager 四行文字 | ✅ | — | ❌ | [th15/01 §4](th15/01-chapter-and-bonus.md) |
| 擦弹计数器 `GRAZE` / `GRAZE_IN_CHAPTER` 存在并被写 | ✅ | ✅ | ✅ | [player/th16/01 §2c](../player/th16/01-hit-life-system.md)、[th18/01 §2](th18/01-chapter-residuals.md) |
| `GRAZE_IN_CHAPTER` 会被归零 / 被读 | ✅ | ❓ | ❌ | [th18/01 §2](th18/01-chapter-residuals.md) |
| 擦弹会让场上掉落物减速（LoLK 因子） | ✅ | ❌ | ❌ | [player/th15/01 §4](../player/th15/01-graze-and-item-slowdown.md)、[player/th18/02 §2](../player/th18/02-graze-and-item-slowdown.md) |
| 相位成败（击破 / 超时）由中断槽 `{hp_value, time}` 判定，引擎自己分两条子程序 | ✅ | ❓ | ✅ | [th15/01 §2](th15/01-chapter-and-bonus.md)、[th18/02 §2](th18/02-wave-signals.md) |
| 「耐久符卡（时符）」有显式标记位，且超时按收卡算 | ❓ | ❓ | ✅ | [th18/02 §1](th18/02-wave-signals.md) |
| 道中「一章 / 一波」= 关卡脚本里的一个敌人组块 | ✅ | ❓ | ❌（脚本里没有 524，用子程序边界代替）| [th15/01 §1b](th15/01-chapter-and-bonus.md)、[th18/03](th18/03-midstage-waves.md) |

> **图例**：✅ 该版本一手验过（证据列给地址/出处） · 🟡 待验（从别的版本借来的假设，或单源） · ❌ 已知不同/不存在 · ❓ 存疑 · — 未看
>
> **本页不许出现没有出处的断言。** 从某作借到另一作的判断一律 🟡，在该版本 exe 上验过才能改 ✅（[`METHOD.md`](../../METHOD.md)）。

## th18 现状（2026-09-09 一手，2026-09-10 复核）

TH18 **保留了 opcode 524 且零售脚本还在调**（`ins_524(43)` = boss 入场、`ins_524(2)` = 道中 boss 退场、
关卡脚本开场 `0` 与进 boss 前 `41`；`81`/`22` 在 boss 对话与 debug 子程序里），但引擎只剩「记章号」：
没有延迟标志位、没有每章结算、`GRAZE_IN_CHAPTER` 只写不读。**这套东西在 TH18 是死代码**——
`engine/chapter/th18/01-chapter-residuals.md`。

> ⚠️ 2026-09-10 全量复核修正了一条旧口径：**道中段没有 524**（`MainFront`/`MainLatter`/`MainSubNN`
> 里一个都没有），所以「轮询 `th18:0x4ccce4` 当道中波边界」不成立 ——
> [`th18/02 §4`](th18/02-wave-signals.md)、[`th18/03`](th18/03-midstage-waves.md)。

但**判相位成败的机器还在**：敌机中断槽 + `life.current <= hp_value`（击破）/ `slot.time <= time_in_ecl`
（超时），超时另置 `flags_lo` bit24 与符卡位 bit7；ECL 542 给耐久符卡打 bit3。
复刻章节奖时这三个位就是现成的击破率来源 —— `engine/chapter/th18/02-wave-signals.md`。

要「非符 + 一张符卡 = 一波」这一级，原生切不出来：得用符卡位 `SPELLCARD_PTR+0x78 & 1`
的下降沿自建（[`player/th18/02`](../player/th18/02-graze-and-item-slowdown.md) §3）。
道中那一级（一个敌人组 = 一波）用**关卡脚本 ECL 子程序号 + 刷出**：
TH15 里它由 ECL 524 显式宣告（[th15/01 §1b](th15/01-chapter-and-bonus.md)），TH18 没有 524
但脚本结构一样 —— [`th18/03`](th18/03-midstage-waves.md)。

> 仍未反：TH18 的 `zGameThread` 字段图（ExpHP 数据里没有这个结构体），
> 所以 `GameThread+0x24` 这个「章号副本」的确切用途 ❓。
