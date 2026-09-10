# th18 输入系统 —— 键盘配置、掩码位、以及哪些键空着

> **版本**：TH18 v1.00a（`th18.exe`，md5 `9969cac756098c1da05a81de45437a70`，imagebase `0x400000`）。
> 本文裸地址默认属该版本；引用其他版本写成 `th16:0x…`。
> **方法**：Ghidra 12.1.3 一手反编译（`0x401c50` 输入装配 / `0x4019f0` 设备探测 / `0x42abc0` 边沿 /
> `0x447700` 键位设置菜单）+ 直读 exe 的配置表 + 全 exe 掩码引用普查。
> **可信度**：配置表内容、掩码位、A/S 空着 ✅ 一手；「某位没人用」是穷举后仍给 🟡。

## 0. 一句话

TH18 每帧的输入是**一条装配线**：

```
GetKeyboardState(栈上 256 字节)  ┐
                                 ├─► 硬件掩码 zInput+0x00 (th18:0x4ca210) ─► 边沿/连按 (0x42abc0)
XInput / DirectInput 手柄        ┘                                    │
                                                                     ▼
                          游戏读的掩码：INPUT_HELD th18:0x4ca428（+0x0c = 按下沿 0x4ca434）
```

键盘那一侧**只认配置表里登记的 6 个虚拟键码**，其余键游戏根本不看 —— 所以「想绑一个游戏不认识的键」
要么自己调 `GetKeyboardState`，要么改配置表（后者没用：多余槽没人读）。

## 1. 键盘配置表（一手：直读 exe + 菜单代码）

三个设备各一份 profile，间距 `0x14`（= 5 条 × 4 字节）：

| 设备 | 基址 | 说明 |
| --- | --- | --- |
| 1 | `th18:0x4ca448` | DirectInput 手柄 |
| 0 | `th18:0x4ca45c` | XInput 手柄 |
| 2 | `th18:0x4ca470` | **键盘** |

每条 = 两个 16 位虚拟键码（低 word / 高 word）。键盘 profile 的出厂值：

| 条 | 低 word | 高 word | 含义 |
| --- | --- | --- | --- |
| 0 | `0x5a` = **Z** | `0x58` = **X** | Shot / Bomb |
| 1 | `0x10` = **Shift** | `0x1b` = **Esc** | Slow / Pause |
| 2 | `0xffff` | `0xffff` | 空 |
| 3 | `0xffff` | `0xffff` | 空 |
| 4 | `0x43` = **C** | `0x44` = **D** | Item（发动卡）/ Change（切卡）|

> 方向键（←↑→↓）**不在**这张表里 —— 它们由输入装配里写死的 `local_a7..local_af` 直接读。
> 键位设置菜单的五个可改项就是 `Shot / Bomb / Slow / Item / Change`（串在 `th18:0x4b66a0` 起），
> 加 `Pause` 与 `Default`。

## 2. 掩码位（一手：`0x401c50` 的移位链逐键解出）

`0x401c50` 把 `GetKeyboardState` 的结果按上表的键码取出来，用一串移位塞进掩码。逐键解出的位：

| 位 | 键 | 谁在用 |
| --- | --- | --- |
| `0x001` | Z | Shot（`Player__tick_shooting_state` `0x45ea06`）|
| `0x002` | X | Bomb（`Player__on_tick__body` `0x45c048`）|
| `0x008` | Shift | 低速 / 聚焦（`0x45b3fa`；mod 的 `CE_INPUT_FOCUS`）|
| `0x100` | Esc | Pause（`GameThread__on_tick` `0x4439f7` 的 `0x80103`）|
| `0x400` | C | 发动选中的主动卡（`0x45c084`；mod 的 `c_press`）|
| `0x800` | D | 切换选中的主动卡（`0x45c09c` → `AbilityManager::set_selected_active_card(mgr,-1)`）|
| `0x010`/`0x020`/`0x040`/`0x080` | 方向（手柄十字键为主）| 移动 |
| `0x40000`/`0x80000`/`0x200000` | 手柄按钮 / 对话推进 | `GuiMsgVm__run` `0x440a6b` 的 `0x80001` 等 |

> mod 侧常量：`CE_ADDR_INPUT_HELD` = `th18:0x4ca428`、`CE_ADDR_INPUT_PRESSED` = `th18:0x4ca434`、
> `CE_INPUT_FOCUS` = `0x008`。

## 3. 哪些键空着（用户 2026-09-10 点名 A / S / D）

- **A（`0x41`）、S（`0x53`）：空着** ✅ —— 不在配置表、也不是方向键。
- **D（`0x44`）：已被 Change 占用** ❌（想用就先把「切卡」改到别的键）。
- 其它常见空键：`F`、`G`、`H`、`V`、`B`…（只要不进配置表）。

## 4. 怎么读一个「游戏不认识的键」

游戏自己读键盘时用的是**栈上**缓冲（`0x401c50` 里的 `local_110`），**没有全局键盘状态数组**可读：
全局 `th18:0x570ca0` 那 256 字节只在**键位设置菜单**里刷新（唯一调用方 `0x447966`，在
`KeyConfigMenu__on_tick` 内），正常游玩时是旧的。

所以卡里要读 A / S，只能自己调 user32 的 `GetKeyboardState`（SDK 的 `ce_key_down(vk)`，
`CE_VK_A` / `CE_VK_S` / `CE_VK_D`）。代价：mod 的 DLL 多一个 `USER32.dll` 依赖
（`make dllverify` 已把依赖数从 2 改成 3）。

### 4a. replay 的坑 ⚠️

回放只录 `th18:0x4ca428` 的 **16 位**掩码（`0x46295a` `MOVZX EAX,word ptr […]`），
原始键盘状态**不进流**。所以「按 A 触发」的效果在回放里**不会重现**（玩家在回放时也不会去按 A）。
要 replay 安全，只有两条路：

1. 用掩码里已有的键（C / D）——但会和主动卡 / 切卡抢；
2. 卡自己往掩码里写一个空闲位（如 `0x2000`）再读它 —— 前提是**录制时**这个写发生在
   `ReplayManager__on_tick__record_replay` 读掩码之前（顺序未验 🟡）。

## 5. 相关

- 掩码的边沿 / 连按：`Input__compute_edges` `th18:0x42abc0`（8 帧 = 连按开始、0x1a 帧 = 重发）
- 设备探测（决定「用键盘还是手柄」）：`FUN_004019f0` `th18:0x4019f0`，结果写 `th18:0x4ca444`
  （0 = XInput / 1 = DirectInput / 2 = 键盘）
- 用它的卡：`mods/th18.v1.00a/card-expand/native/cards/season.c`（季节解放绑 A）
