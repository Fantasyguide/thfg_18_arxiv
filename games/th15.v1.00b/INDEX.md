# th15.v1.00b/ — TH15（東方紺珠伝 / Legacy of Lunatic Kingdom）逆向工作区

> **版本**：TH15 v1.00b（`th15.exe`，imagebase `0x400000`）。本文裸地址默认属该版本；引用其他版本须写成 `th18:0x…`。

> 本目录只做**登记与导航**：样本、工程、覆盖统计、待挖图。
> 引擎结论一律回 [`engine/`](../../engine/README.md)，本页不写语义（[`games/README.md`](../README.md)）。

## 样本与工程

| 项 | 值 |
| --- | --- |
| exe | `local/th15.v1.00b/th15.exe`（951,808 B，md5 `bddd41a586e09e4447f795a14009d68a`，`Legacy of Lunatic Kingdom. ver 1.00b`） |
| 归档 | `local/th15.v1.00b/th15.dat`（77,453,661 B）；解包产物 `dat/` + `anm/`（`tooling/thtk/unpack.py th15.v1.00b`） |
| Ghidra 工程 | `local/th15.v1.00b/ghidra_projects/th15.exe.gpr` |
| MCP `database_id` | `th15` |
| exe 内函数总数 | 3068 |
| 已命名 | 880 |
| 🔬 真·待挖 | 2188 |
| 导入统计 | `[create-missing] created=89 existed=397 disassembled=30 failed=4`；`[safe] applied=590 skipped=19 missing=4`；`types=116 statics=132`；`labels created=369`；`sjis found=13` |

> 建库一条命令（幂等）：`tooling/ghidra/bootstrap.py th15`。
> 本机 pyghidra 装在 `local/pyghidra-site`（系统 python 没装、沙箱也写不进 `~/.local`），
> 跑法见 [`tooling/ghidra/README.md`](../../tooling/ghidra/README.md)。

## 已反出的东西（一手，正文在 engine/）

| 主题 | 结论在哪 | 状态 |
| --- | --- | --- |
| 章节（波次）系统 + 章节奖 | [`engine/chapter/th15/01-chapter-and-bonus.md`](../../engine/chapter/th15/01-chapter-and-bonus.md) | ✅ 边界/计数/结算/清零/UI 全链 |
| 擦弹：计数、橙色火花、道具减速因子、逐弹染色 | [`engine/player/th15/01-graze-and-item-slowdown.md`](../../engine/player/th15/01-graze-and-item-slowdown.md) | ✅（「全屏橙色滤镜」= 负结论） |
| th15but14 魔改壳（d3d9.dll 收点系统） | `local/_scratch/TH15BUT14-REPORT.md`（scratch，不入库） | ✅ 状态机/档位/安装点 |

## 待挖图

| 区域 | 为什么值得挖 | 已知线索 |
| --- | --- | --- |
| ECL 524 `setChapter` 的**脚本侧**用法 | 章节边界怎么切完全由脚本决定 | opcode 分支 `0x42d494`；`.ecl` 反汇编需 thecl + eclmap |
| `FUN_00427a10`（敌机擦弹判定所在函数） | 唯一不减速的擦弹路径，函数名未定 | `0x428031` 调 `do_graze` |
| Pointdevice 快照的完整字段面 | `0x43dd30` 只看了外壳 | 各 Manager 的 `store_snapshot` 族 |
| `PopupManager` 弹字项布局（`+0x24`/`+0x28`） | 「BONUS %.1f / NO BONUS」的创建者还没找到 | 绘制端 `0x45ee9e` / `0x45eede` |
| 玩家/敌机/激光的擦弹判定几何 | 复刻擦弹面时要原样 | `player_collide_*` 族尚未在 th15 上逐条对 |

## 与其他作的关系

- TH16 的擦弹是**同构但少一步**（无道具减速因子）：[`engine/player/th16/01-hit-life-system.md`](../../engine/player/th16/01-hit-life-system.md) §2c。
- 章节全局字段（`GRAZE_IN_CHAPTER` 等）在 TH16 也还在：跨版本矩阵见 [`engine/chapter/OVERVIEW.md`](../../engine/chapter/OVERVIEW.md)。
