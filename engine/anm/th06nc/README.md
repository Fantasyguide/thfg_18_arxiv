# TH06NC ANM

本目录记录 Steam New Classic 样本上的 ANM 一手结论。TH06 社区反编译只提供语义参照，
地址与结构一律在 NC 上自行验证。

TH06NC 沿用 **ANM v0 容器 + TH06 式单函数解释器**，但指令集从 0–31 扩到 **0–35**。

| 入口 | 地址 | 当前命名 |
| --- | --- | --- |
| 脚本解释器 | `0x1400069b0` | `AnmManager__ExecuteScript` |
| opcode 跳表 | `0x140007494` | `g_AnmOpcodeJumpTable`（36 条） |
| 指令推进尾部 | `0x1400070e3` | `ANM_ADVANCE_TO_NEXT_INSTR` |
| 中断标签扫描 | `0x1400068f0` | `AnmManager__SeekInterruptLabel` |
| 设置当前精灵 | `0x140002980` | `AnmManager__SetActiveSprite` |
| 加载 anm | `0x1400020b0` | `AnmManager__LoadAnm` |
| 管理器全局 | `0x140a6e9b0` | `g_AnmManager` |

详见 [VM 与新增锚点 opcode](01-vm-and-anchor-opcodes.md)；
与原版的逐项对照见 [TH06 ↔ TH06NC 的 ANM VM 对照](02-th06-baseline-comparison.md)。
Ghidra 应用脚本为 [`tooling/ghidra/apply_th06nc_anm.py`](../../../tooling/ghidra/apply_th06nc_anm.py)；
数据侧的独立统计工具是 [`tooling/anm0_scan.py`](../../../tooling/anm0_scan.py)，
两者都要求样本 MD5 `b0cc28ec904e9efce2439ad857c938b5`。
- [`03-ascii-manager-strings.md`](03-ascii-manager-strings.md) —— HUD 文字：`g_AsciiManager` 的字符串表布局，画一行字只需追加一条记录（2026-09-11）
