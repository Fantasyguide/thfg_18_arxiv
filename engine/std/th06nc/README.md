# TH06NC STD

本目录记录 Steam New Classic 样本上的一手地址与可复核推断。
CC0 的 `th06-decomp` 只提供语义参照，地址与结构一律在 NC 自己的字节上取。

| 入口 | 地址 | 当前命名 |
| --- | --- | --- |
| 脚本解释器 | `0x140077bb0` | `Stage__OnUpdate` |
| 高优先级绘制 | `0x140078290` | `Stage__OnDrawHighPrio` |
| 低优先级绘制 | `0x140078390` | `Stage__OnDrawLowPrio` |
| chain 注册 | `0x1400789a0` | `Stage__RegisterChain` |
| 加载（含内联的 `LoadStageData`） | `0x1400785c0` | `Stage__AddedCallback` |
| 对象绘制 | `0x140078cb0` | `Stage__RenderObjects` |
| opcode 跳表 | `0x14007826c` | `g_StageOpcodeJumpTable` |
| 关卡文件表 | `0x1403de540` | `g_StageFiles` |

详见 [关卡脚本运行时与 NC 的循环 opcode](01-stage-runtime-and-nc-loop.md)。
Ghidra 应用脚本为 [`tooling/ghidra/apply_th06nc_std.py`](../../../tooling/ghidra/apply_th06nc_std.py)，
要求样本 MD5 `b0cc28ec904e9efce2439ad857c938b5`。
