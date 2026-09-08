# sht — SHT 自机（shoot type）配置
> **版本**：跨版本。本文出现的地址一律带版本前缀（如 `th16:0x442560`）。


**SHT 不是脚本语言，是纯二进制配置文件**，逐版本差异大。它描述一个自机的移动速度、判定、
以及一组组 shooter（发弹器）；运行时按「火力档 × 聚焦」选中一组，逐个 shooter 派发。

难点从来不是字节布局（社区早已搞清），而是 **`func_on_init/tick/draw/hit` 这些行为函数索引到底指向什么**，
以及 `flags` 段的含义——社区至今无公开破解。**TH16 这块已经攻下**：四张跳转表全反、
索引→行为表建立、`flags` 证明为运行时不读。

格式与工具现状见 [`format-reference.md`](format-reference.md) 与 [`tools-analysis.md`](tools-analysis.md)（跨版本）。
逐版本一手：[`th16/`](th16/README.md)（func_\* 跳转表 / flags 段）、[`th18/`](th18/README.md)（文件布局 / 装备卡子机 / 字段图）。

## 断言 × 版本矩阵

| 断言 | th16 | th18 | 证据 |
| --- | :---: | :---: | --- |
| SHT 是纯二进制配置，无脚本语义（→ IDE 该做表单而非编译） | ✅ | ✅ | [format-reference.md](format-reference.md)、[th16/01](th16/01-runtime-semantics.md) |
| 解析器把 func_\* 索引**解成函数指针**，无边界检查 | ✅ | ✅ | [th16/03 §1](th16/03-th16-funcstar-jumptables.md)；th18 见 [th18/01 §3](th18/01-file-layout-and-shooterset-index.md) |
| 四张函数指针表在 .rdata（tick 表 `th16:0x4919a0`） | ✅ | ❌部分 | th18 有三张在 .rdata（`th18:0x4b4230` init / `th18:0x4b4210` tick / `th18:0x4b41f0` hit），**draw 那张在 .data**（`th18:0x4cf414`，运行时填）——[th18/01 §3](th18/01-file-layout-and-shooterset-index.md) |
| shooter 结构 stride：th16 = **0x58**，**th18 = `0x5c`（不同！）** | ✅ | ✅ | [th16/05 §2](th16/05-th16-flags-no-runtime-read.md)；th18 逐字段图见 [th18/02 §1](th18/02-shooter-record.md) |
| **`flags` 段运行时完全不被读**（负结论，过了对抗证伪） | ✅ | **❌** | [th16/05 §3–4](th16/05-th16-flags-no-runtime-read.md)；**th18 有一个读取点**：`func_on_init = 3` 读 `+0x4c` 作横移倾斜系数——[th18/03 §5.2](th18/03-func-tables-timers-and-dead-fields.md) |
| shooterset 按「火力档 × 聚焦」选择 | ✅ | ✅ | th16 `th16:0x445470`；th18 `th18:0x45ea00`，**且 th18 在 10 组主炮之后还有 13 组「装备卡子机」**，见 [th18/01 §4](th18/01-file-layout-and-shooterset-index.md) |
| 组内区分主弹与子机弹（shooter `+0x20` 的子机槽号）| ✅ | ✅ | [th16/07 §2](th16/07-th16-shooterset-organization.md)；th18 [th18/02 §1](th18/02-shooter-record.md) |
| 自机弹伤害管线：伤害源池 `PLAYER+0xd080`，stride 0x94，256 个 | ✅ | 🟡 | [th16/08 §1–2](th16/08-th16-player-damage-pipeline.md) |
| header `+0x04`「可配置判定半径」是**哑弹**——运行时不读 | ✅ | ✅ | [th16/99-QUIRK](th16/99-QUIRK-可配置判定半径其实是哑弹.md)；th18 连读都不读（`+0x04/08/0c` 无引用）——[th18/03 §2](th18/03-func-tables-timers-and-dead-fields.md) |
| 四张 func 表逐项行为已反；1/2（寻的/激光）与 hit 表跨作稳定，init/tick 3–5 各作自定 | ✅ | ✅ | [th16/03 §6.4](th16/03-th16-funcstar-jumptables.md)、[th18/03 §5–6](th18/03-func-tables-timers-and-dead-fields.md) |
| 发射计时器：短 15 帧、长 120 帧（fire_rate2） | 🟡 | ✅ | [th18/03 §1](th18/03-func-tables-timers-and-dead-fields.md)；th16 见 [th16/07 §1](th16/07-th16-shooterset-organization.md)（读到两计时器，周期未数） |
| 子机位置块：高速/低速各 10 对，三角索引 {0,1,3,6} | — | ✅ | [th18/03 §3](th18/03-func-tables-timers-and-dead-fields.md) |

> **图例**：✅ 该版本一手验过（证据列给地址/出处） · 🟡 待验（从别的版本借来的假设，或单源） · ❌ 已知不同/不存在 · ❓ 存疑 · — 未看
>
> **本页不许出现没有出处的断言。** 从某作借到另一作的判断一律 🟡，在该版本 exe 上验过才能改 ✅（[`METHOD.md`](../../METHOD.md)）。

## ⚠️ 这个子系统的宣称风险最高

上表多条是**社区标 `unknown` 而我们解开的**，没有外部佐证 = 风险最高。
每条都过了 [`METHOD.md`](../../METHOD.md) 的四道闸门（一手到底 / 对抗证伪 / 量纲常识 / 交叉对名），
踩过的坑记录在 [th16/99-QUIRK](th16/99-QUIRK-可配置判定半径其实是哑弹.md) 和
[th16/02](th16/02-community-recheck-funcstar-flags.md)。**动它们之前先读那两篇。**

## 开放

- TH19 完全未验。**TH18 已齐**（2026-09-06）：四张表 18 项全反（[th18/03](th18/03-func-tables-timers-and-dead-fields.md)），
  func 编号与 TH16 **只有 1/2 与 hit 表共用**；`flags` 段只有 `+0x4c` 有读取点。其余 `+0x3c..+0x5b` 在 th18 只穷举了
  sht 基址消费者，未做全指令扫描（🟡）。
- 面向 THBWiki 的成稿：[WIKI-DRAFT.md](WIKI-DRAFT.md)。
- 引擎其余切口（敌人/道具/图形/音效）的锚点索引见 [th16/06](th16/06-th16-engine-incisions.md)。
