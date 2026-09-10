# th13/ — TH13（東方神霊廟 / Ten Desires）逆向工作区

> **版本**：TH13 v1.00c（`th13.exe`，imagebase `0x400000`）。本文裸地址默认属该版本；引用其他版本须写成 `th18:0x…`。
>
> 本文件只做**登记**：样本、工程、覆盖统计、本作特有机制清单、待挖图。
> **引擎结论一律不写在这里** —— 指回 `engine/<子系统>/th13/`。

## 样本

| 项 | 值 |
| --- | --- |
| exe | `local/th13.v1.00c/th13.exe`（gitignored）|
| md5 | `6f39c0708ee4daa94f5053526c4e10cc` |
| 大小 | 790,528 B |
| PE | i386，imagebase `0x400000`，4 段，时间戳 2011-08-17 13:56 |
| 资产 | `local/th13.v1.00c/th13.dat` |
| 解包 | `thdat -x 13 th13.dat <dir>`；`thecl -d 13 -m local/vendor/eclmap/eclmap/th13.eclm <ecl>`；`thanm -l 13 -o -m local/vendor/thpages/static/mapfile/v8.anmm <anm>` |

## Ghidra 工程

| 项 | 值 |
| --- | --- |
| 工程 | `local/th13.v1.00c/ghidra_projects/th13.exe.gpr`（gitignored）|
| 函数总数 | **2359** |
| 已命名 | **1078**（其余 1281 仍是 `FUN_*` 自动名）|
| ExpHP `th-re-data` 名 | 353 条里**已套 306**；缺的主要是 `operator new` 系列（如 `TranceManager::operator new`）|
| 导出 | `local/th13.v1.00c/th13-funcs.json`（逐函数 addr/name/size/xrefs）|

## 本作特有机制

| 机制 | 一手文档 |
| --- | --- |
| 神灵 / 灵界（Trance）—— 第三条资源轴 | [`engine/spirit/th13/01`](../../engine/spirit/th13/01-spirit-and-trance.md)、门面 [`engine/spirit/OVERVIEW`](../../engine/spirit/OVERVIEW.md) |
| 神灵的 ECL 指令 `spec0`/`spec1`/`spec2`/`spec3`（opcode 1000–1003）| 同上 §3.2 / §3.3 |
| `dropItems`(509) 与 `dropItemsSp`(562) 共用尾代码 | 同上 §3.3（**订正社区 eclmap 的命名歧义**）|

> 其余子系统（自机 / 弹幕 / ECL / anm / sht / msg / menu）本目录尚未系统开工，
> 现有知识以 TH16/TH18 为主，**不可外推到 TH13**（见 [`CLAUDE.md`](../../CLAUDE.md)）。

## 待挖图

| 事项 | 状态 |
| --- | --- |
| 自机侧（`Player` 状态机 / 开火 / 子机 / 资源经济）| ❓ 未开工 |
| ECL 全量表（opcode 名与参数个数逐条坐实）| ❓ 现在只解了神灵相关的 4 条 |
| anm 全量表（v8 opcode）| ❓ 只有 `astral.anm` / `front.anm` 局部读过 |
| `DAT_004dcc30`（挡住灵界槽 HUD 创建的全局）| ❓ 全 exe 只有读，见 [`engine/spirit/th13/01`](../../engine/spirit/th13/01-spirit-and-trance.md) §8 |
| 本作与 TH14/TH15 的引擎血缘（`.dat` 版本、结构体位移）| ❓ 未做 |
