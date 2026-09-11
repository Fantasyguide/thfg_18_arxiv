# TH06NC AsciiManager：HUD 文字的字符串表（怎么在屏幕上画一行字）

> **版本**：TH06NC（`th06nc.exe`，MD5 `b0cc28ec904e9efce2439ad857c938b5`，imagebase `0x140000000`）。
> 本文裸地址默认属该版本；引用原版须写成 `th06:0x…`。
> **方法**：MCP 直读（2026-09-11）：链注册 `0x140009400`、初始化 `0x140009790`、绘制 `0x140009890`、
> FPS 那处**内联的 `AddString`** `0x14007a3a0`；对照 CC0 `AsciiManager/AddString.c` / `DrawStrings.c`。
> **可信度**：字段偏移与常量 ✅（指令直读 + 两处独立命中）；`isGui` / `isSelected` 的名字借自 CC0 🟡。

## 0. 一句话

想画一行字，不用调任何函数：往 `g_AsciiManager + 0x7bb4` 的记录表**追加一条 `0x60` 字节的记录**、
`+0x7bb0` 的计数 +1 即可。绘制回调每帧画完把计数清零，所以**每帧都要重新加**。
引擎自己画 FPS 就是这么干的（`AddString` 被内联进 `0x14007a3a0`）。

## 1. 对象与链

| 项 | 值 | 证据 |
| --- | --- | --- |
| `g_AsciiManager` | `0x1403de620` | `0x140009400` 里三个 `ChainElem` 的 `arg` 都是它 |
| calc 回调 | `0x1400090d0`，优先级 **1** | 同上（`_DAT_1403ec1e0 = 1`）——在 calc 链里几乎最先跑 |
| draw 回调 | `0x140009260`（优先级 `0x13`）、`0x1400092a0`（`0xd`） | 同上 |
| `AddedCallback` | `AsciiManager__AddedCallback` `0x140009700` | 载 `data/ascii.anm` 等四个 anm 后调 `0x140009790` 初始化 |
| 画字符串表 | `0x140009890` | `0x140009260` 第一件事就是调它 |

与 TH06 的对应：`RegisterChain` / `OnUpdate` / `OnDraw` / `DrawStrings`（CC0 同名）。

## 2. 管理器里与字符串有关的字段

| 偏移 | 类型 | 含义 | 证据 |
| --- | --- | --- | --- |
| `+0x00` / `+0x04` | f32 | 当前缩放 x / y（`AddString` 时拷进记录；FPS 用 0.52 后还原 1.0） | `0x14007a3a0` |
| `+0x10` | AnmVm | 画字用的 VM（`0x140009890` 把每条记录的量灌进 `+0xd8` 位置、`+0xf4/+0xf8` 缩放、`+0xfc` 颜色再 `AnmManager__Draw`） | `0x140009890` |
| `+0x52c8` | u8 | 当前 `isGui`（拷进记录 `+0x0c`） | `0x14007a3a0`、CC0 `isGui` |
| `+0x52c9` | u8 | 当前 `isSelected`（拷进记录 `+0x5c`；初始化写 0） | `0x140009790`、CC0 `isSelected` |
| `+0x52cc` | u32 | 当前颜色 `0xAARRGGBB`（初始化写 `0xffffffff`；拷进记录 `+0x08`） | 同上 |
| `+0x7bb0` | i32 | `numStrings`，**上限 0x100** | `0x14007a3a0`：`if (n < 0x100) n++` |
| `+0x7bb4` | 记录[256] | `strings[]`，每条 `0x60` | 同上 |

## 3. 记录布局（`0x60` 字节）

| 偏移 | 类型 | 字段 | 绘制时怎么用（`0x140009890`） |
| --- | --- | --- | --- |
| `+0x00` / `+0x04` | f32 | `scale.x/y` | 灌进 VM；**每字前进 `scale.x × 12.0`**（`0x14030cea8`），换行 `scale.y × 16.0`（`0x14030ceb8`） |
| `+0x08` | u32 | `color` | 灌进 VM `+0xfc` |
| `+0x0c` | u8 | `isGui` | 值变化时切一次渲染状态（`0x14007af40` / `0x14007b120`） |
| `+0x10` / `+0x14` / `+0x18` | f32 | `pos.x/y/z` | 起笔位置；`\n` 回到 `pos.x` 并下一行 |
| `+0x1c` | char[64] | `text`（NUL 结尾） | 逐字节：`' '` 只前进；`0x20 < c < 0x82` 才画 |
| `+0x5c` | u8 | `isSelected` | 0 → 精灵 `c − 0x15`；非 0 → 精灵 `c + 0x62`（第二套字形，菜单选中项用） |

与 TH06 的差别只有一处数值：**字宽 14.0 → 12.0**（CC0 `DrawStrings.c:22` 是 `scale.x * 14.0`）。

## 4. 坐标空间

FPS 画在 `(758.0, 464.0)`（`0x14030d048` / `0x14030d018`）。`464 = 480 − 16`、`758 > 640`：
这是 NC 的**虚拟屏**——高固定 480、宽随窗口宽高比伸展（16:9 → 853.3），与
[`mods/th06nc/mouse-control/TARGET.md`](../../../mods/th06nc/mouse-control/TARGET.md) §6 实测的布局一致。
关卡区左上角在 `((W − 384) / 2, 16)`，`W = 480 × 客户区宽 / 高`。

## 5. 用它的 mod

[`mods/th06nc/autoplay`](../../../mods/th06nc/autoplay/README.md)：每帧在 `Player__OnUpdate` 入口
（calc 链，晚于优先级 1 的清零、早于 draw 链）追加一条 `AI Control On/Off`。

## 6. 未做

- 没在 Ghidra 里给 `0x140009400` / `0x1400090d0` / `0x140009890` / `0x1403de620` 起名入库
  （`symbols.json` 尚无它们）。下次跑迁名脚本时补。
- `isGui` 两种渲染状态具体差什么没读（`0x14007af40` / `0x14007b120`）。
