# TH16 v1.00a —《東方鬼形獣》Wily Beast and Weakest Creature

本作是本仓库的**主力研究对象**。引擎结论按子系统分散在 [`../../engine/`](../../engine/README.md)，
本页只做登记与导航。

## 样本与工程

| 项 | 值 |
| --- | --- |
| exe | `local/th16.v1.00a/th16.exe`，683,520 B，md5 `70d3429444e1e6c18a72e7200a4d7ce8`（**Steamless 脱壳版**；原版 md5 `cb9caf54ce5738f70086e783ec88fd2a` 因 PE 容器重建而不同，代码段一致）|
| imagebase | `0x400000`（32 位 PE） |
| Ghidra 工程 | `local/th16.v1.00a/ghidra_projects/th16.exe.{gpr,rep}` |
| MCP `database_id` | `th16` |
| 归档 | `local/th16.v1.00a/th16.dat`（THA1，解包结论见 [`archive-tha1.md`](../../engine/_shared/archive-tha1.md)） |

> ✅ **2026-09-09 样本现状**：本机只有 **Steam 包装版** exe（md5 `701a603c2afc686776ffddacbed4ee4e`，
> `.text` 被 SteamStub 3.1 加密），已用 **Steamless** 在 Linux 上脱壳为可分析样本（配方见
> [`local/README.md`](../../local/README.md)），Ghidra 工程已重建。上表 Ghidra 工程路径仍有效。

## 符号覆盖

由 `tooling/ghidra/bootstrap.py th16` 重建后实测（2026-09-09，脱壳版 exe）：

| 来源 | 数量 |
| --- | --- |
| exe 内函数总数 | 1863 |
| 已命名（ExpHP + 我们自己） | 1334 |
| 🔬 真·待挖（仍是 `FUN_`） | 529 → 清单见 [`unexplored.md`](unexplored.md) |
| ExpHP 符号 | `applied=1267 skipped=41 missing=0`；补建函数 `created=213` |
| ExpHP 结构体 | 158（`failed=7`）+ statics 398 + labels 422 |

> ⚠️ [`unexplored.md`](unexplored.md) 本身的统计停在 2026-06-10（当时 872 已命名 / 515 待挖），
> 比上表旧。要刷新，跑 `bootstrap.py` 后再跑 `tooling/ghidra/build_worklist.py`。

## 本作特有机制

**季节系统**：季节槽充能 + 季节释放（C 键），与主炸（X 键，消耗角色库存）是**两套并行计费**。
详见 [`engine/player/OVERVIEW.md`](../../engine/player/OVERVIEW.md) 的对应行与
[`th16/02`](../../engine/player/th16/02-season-release-and-bombs.md)。

菜单里的**副季节选择**同样是本作独有，链路见
[`engine/menu/th16/03`](../../engine/menu/th16/03-character-subseason-sht-chain.md)。

## 各子系统进度

| 子系统 | 状态 |
| --- | --- |
| [player](../../engine/player/OVERVIEW.md) | ✅ 生命 / 季节释放 / 开火·输入·移动 / option / 字段图 / 资源经济 |
| [sht](../../engine/sht/OVERVIEW.md) | ✅ func_\* 跳转表 / flags 证负 / shooterset / 伤害管线（社区此前无公开破解） |
| [ecl](../../engine/ecl/OVERVIEW.md) | ✅ VM 核心已基本反完 |
| [bullet](../../engine/bullet/OVERVIEW.md) | ✅ 核心引擎 / 运动 VM / 激光 |
| [msg](../../engine/msg/OVERVIEW.md) | ✅ 两套指令集全反，已告一段落 |
| [menu](../../engine/menu/OVERVIEW.md) | ✅ 状态机 + 选择链 |
| [anm](../../engine/anm/OVERVIEW.md) | ⚠️ 未系统开工 |
| [_shared](../../engine/_shared/README.md) | ✅ 主循环 / 引擎数学+PRNG / THA1 归档 |

## 下一步候选

1. **深挖有名却语义空白的玩法系统**：Bomb 各角色行为、Spellcard。
2. **ANM 系统开工**（[`engine/anm/`](../../engine/anm/OVERVIEW.md)），它是图形侧总接缝。
3. **把已有结论变成实跑的 mod**：[`mods/th16.v1.00a/tracking-laser/`](../../mods/th16.v1.00a/tracking-laser/README.md)
   已过静态审计但**尚未在游戏里跑过**。

## 待挖图

[`unexplored.md`](unexplored.md) — 由 `tooling/ghidra/build_worklist.py` 自动算出的
「谁都没命名的处女地」清单，避免重复劳动。
