# th12.v1.00b/ — TH12（東方星蓮船 / Undefined Fantastic Object）逆向工作区
> **版本**：TH12 v1.00b（`th12.exe`，imagebase `0x400000`）。本文裸地址默认属该版本；引用其他版本须写成 `th18:0x…`。

> 本目录只做**登记与导航**：样本、工程、覆盖统计、待挖图。
> 引擎结论一律回 [`engine/`](../../engine/README.md)，本页不写语义（[`games/README.md`](../README.md)）。

## 样本与工程

| 项 | 值 |
| --- | --- |
| exe | `local/th12.v1.00b/th12.exe`（737,280 B，md5 `212f187591f4f1fa99eb2845d1b3cf20`） |
| PE | i386，PE 时间戳 2009-08-27 03:39，imagebase `0x400000`，4 段（`.text`/`.rdata`/`.data`/`.rsrc`），**未加壳** |
| 归档 | `local/th12.v1.00b/th12.dat`（THA1；解包 `thdat -x 12 -C <dir> …/th12.dat`） |
| Ghidra 工程 | `local/th12.v1.00b/ghidra_projects/th12.exe.gpr`（程序名 `/th12.exe`，已分析完成） |
| exe 内函数总数 | 2156 |
| 已命名 | **1324**（`local/th12.v1.00b/th12-funcs.json` dump 实测 2026-09-09；与 bootstrap 报的 1323 差 1，疑口径差异） |
| 未命名 | 832（仍是 `FUN_…`） |
| 符号来源 | ExpHP `th-re-data` 的 `data/th12.v1.00b/{funcs,statics,type-structs-own,type-structs-ext,labels}.json`，已由 bootstrap 套用 |

> 复核入口：`python3 tooling/ghidra/scripts/decompile_funcs.py th12.v1.00b 0x44a2d0`。
> 地址对齐已抽检：th-re-data 的 v1.00b 地址与本 exe 一致（`Ufo__on_tick` `0x44a2d0` 等）。

## 本作特有机制（只登记，结论在 engine/）

| 机制 | 结论在哪 | 状态 |
| --- | --- | --- |
| **UFO / 令牌系统**（收 3 令牌召唤、吸收道具充能、击破给奖励） | [`engine/ufo/th12/01-ufo-system.md`](../../engine/ufo/th12/01-ufo-system.md)、[`engine/ufo/OVERVIEW.md`](../../engine/ufo/OVERVIEW.md) | ✅ 全链一手 |
| 彩虹令牌按计时换色（~150 帧、靠近暂停） | 同上 §9 | ✅ 一手 + 社区对账 |
| 道具类型 10..15 = 令牌专用数组 `ItemManager.token_items[0x10]` | 同上 §9 | ✅ 一手 |
| 本作「PIV / 最大得点」与道具得分的换算 | — | ❓ 未开工（只反到 `Globals::sub_421880` 一处） |

> 其它作的特有机制不在本页：TH16 季节 → [`engine/season/`](../../engine/season/OVERVIEW.md)，
> TH18 卡牌 → [`engine/card/`](../../engine/card/)，TH15 章节 → [`engine/chapter/`](../../engine/chapter/)。

## 待挖图

| 区域 | 为什么值得挖 | 已知线索 |
| --- | --- | --- |
| 令牌掉落来源（「UFO 向日葵」等敌人） | 8 个关卡 ECL 里几乎不写令牌掉落 | `default.ecl` 只有 4 个 UFO 子程序；关卡 ECL 仅 2 处 `dropExtra(10/11,1)` |
| `flagSet(1090)`（`flags |= 0x442`）的消费者 | 全 exe 找不到读位 1/6/10 的代码 | handler `0x417934`；🟡 疑残留 |
| 音效 id `0x34` / `0x36` 的名字 | se 名表 `0x4aea50` 只到 0x30 | `0x453d90` / `0x453e20` / `0x453f10` |
| `ItemManager.ufos_spawned_since_stage_begin` | 递增点明确，消费者未找到 | `0x427411` |
| 道具/道具管理器的完整字段图 | 本文只用到令牌相关字段 | `zItem` 大小 `0x9d8`；`ItemManager__on_tick__body` `0x425c20` |
| 自机 / 子弹的完整字段面 | 本作尚无一手文档 | `engine/player/` 目前只有 th15/16/18 |

## 与其他作的关系

- UFO 系统**只有 TH12 有**（th13/15/16/17/18 的 exe 里既无 `UFO` 串也无 `Ufo` 符号）：
  跨版本矩阵见 [`engine/ufo/OVERVIEW.md`](../../engine/ufo/OVERVIEW.md)。
- 本作与 TH10–TH13 共用同一代 ECL VM / anm 格式（anm 映射用 `v4.anmm`，不是 TH13+ 的 `v8.anmm`）；
  ECL VM 的通用语义见 [`engine/ecl/OVERVIEW.md`](../../engine/ecl/OVERVIEW.md)。
