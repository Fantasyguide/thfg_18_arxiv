# _shared/ — 跨子系统的引擎知识

这里放**不专属任何单一子系统**的东西：每帧调度、数学与 PRNG、资产归档、社区来源速查。
判据很简单——如果一条结论 player 和 bullet 和 ecl 都要引用，它就属于这里。

| 文件 | 内容 | 版本 |
| --- | --- | --- |
| [`frame-loop.md`](frame-loop.md) | 优先级更新表 `UpdateFuncRegistry` + 每帧子系统调度链 | TH16 |
| [`math-and-prng.md`](math-and-prng.md) | 角度/向量原语、CRT 浮点、ZUN 16 位 PRNG（算法级解出）、数学常量 | TH16 |
| [`archive-tha1.md`](archive-tha1.md) | THA1 `.dat` 归档 + ZUN 加密 + LZSS（游戏全格式通用） | TH16 |
| [`community-sources.md`](community-sources.md) | 社区工具/人物/权威来源速查；★ ExpHP `th-re-data` 命名金矿 | 跨版本 |
| [`version-residuals.md`](version-residuals.md) | ★ **跨版本残骸台账**：ZUN 换版本时留下的死字段 / 死槽（哪些能白捡、哪些是坑） | 跨版本 |

每篇头部都自带**适用版本 + 可信度**声明，所以文件名不带版本前缀。
新增版本的证据时，在文中开小节而不是新建文件——除非该版本行为已知**结构性不同**。
