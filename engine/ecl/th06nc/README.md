# TH06NC ECL

本目录记录 Steam New Classic 样本上的一手地址与可复核推断。TH06 社区反编译只能提供语义参照，不能直接搬地址或结构。

| 入口 | 地址 | 当前命名 |
| --- | --- | --- |
| 敌机脚本执行 | `0x140022060` | `EclManager__RunEcl` |
| 敌机生成 | `0x1400360d0` | `EnemyManager__SpawnEnemy` |
| 时间线执行 | `0x140036260` | `EnemyManager__RunEclTimeline` |
| 主分派标签 | `0x1400221b1` | `ECL_OPCODE_DISPATCH` |
| 时间线分派标签 | `0x140036492` | `ECL_TIMELINE_OPCODE_DISPATCH` |

详见 [TH06 基线与 NC 差异](01-th06-baseline-and-nc-deltas.md)；
与原版的指令集逐项对照见 [TH06 ↔ TH06NC 的 ECL 指令集对照](02-th06-baseline-comparison.md)；
用 opcode 当坐标系把子系统函数名迁过来的方法与结果见 [opcode 交叉比对](03-opcode-crossmatch.md)；
敌人结构体已定位的字段见 [Enemy 结构体](04-enemy-struct.md)——⚠️ **NC 重排了它，字段顺序不能照抄 TH06**。
Ghidra 应用脚本为 [`tooling/ghidra/apply_th06nc_ecl.py`](../../../tooling/ghidra/apply_th06nc_ecl.py)，要求样本 MD5 `b0cc28ec904e9efce2439ad857c938b5`。
