# local/ —— 本机专属（gitignored）

> ⚠️ **2026-10-16 事故**：本目录曾被并发会话误操作毁成"一个 30 字节的普通文件"，
> 内容（vendor / Ghidra 工程 / `_scratch` / 下面几份 md）**大部分丢失**。
> 已恢复：`th18.v1.00a/th18.exe`（md5 `9969cac756098c1da05a81de45437a70`）、`_scratch/thdis.py`、本文件与另两份小 md。

## 这里放什么
| 路径 | 内容 | 现状 |
| --- | --- | --- |
| `th18.v1.00a/th18.exe` | 游戏样本（ZUN 版权，由用户放入） | ✅ 已恢复 |
| `_scratch/thdis.py` | 临时反汇编小工具（capstone 直读 RVA） | ✅ 已重建 |
| `vendor/th-re-data` | ExpHP 的逐版本 funcs/statics/结构体（★ 命名金矿） | ❌ **丢失，需重新 clone** |
| Ghidra 工程 | `tooling/ghidra/bootstrap.py` 建的库 | ❌ **丢失，需重跑 bootstrap** |
| `GAME-LOG.md` / `DLL-SYNC.md` / `MEMORY.md` | 交接与纪律 | 见同目录 |

## 重建步骤
1. 样本：把 `th18.exe` 放进 `local/th18.v1.00a/`（md5 应为 `9969cac7…`）；
2. 环境：`python3 tooling/doctor.py` → `source tooling/env.sh`（Ghidra 12.x + JDK 21 + pyghidra）；
3. 命名金矿：重新 clone ExpHP 的 `th-re-data` 到 `local/vendor/th-re-data`；
4. 工程：`tooling/ghidra/bootstrap.py`（幂等，会重跑分析并套名）；
5. 导出：`tooling/ghidra/symbols.py export th18.v1.00a`。
