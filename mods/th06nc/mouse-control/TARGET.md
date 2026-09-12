# TARGET —— th06nc/mouse-control 死绑登记

> **版本**：TH06NC（Steam 2026 重制版，`th06nc.exe`，imagebase `0x140000000`）。
> 本文裸地址默认属该版本；引用其他版本一律写成 `th06:0x…`。
>
> 这份文件回答一个问题：**换一个 exe build，我要重取哪些量？**

## 1. 目标二进制

| 项 | 值 |
| --- | --- |
| 游戏 / 版本 | 東方紅魔郷 New Classic（TH06NC） |
| exe md5 | `b0cc28ec904e9efce2439ad857c938b5` |
| 大小 | 5539328 B |
| 架构 | PE32+ x86-64 |
| imagebase | `0x140000000` |
| 版本校验 | **不靠哈希**——运行时比对 `Player__OnUpdate` 入口 13 字节签名，见 §2 |

不走 thcrap（TH06NC 暂无 thcrap 生态），所以没有 `expected` 由 thcrap 代验这回事；
**校验是我们自己做的**，不匹配就永久停用，见 §2。

## 2. 唯一的写入点

只有一个：`Player__OnUpdate` 的函数入口。

| 项 | 值 |
| --- | --- |
| 地址 | `0x140068820`（RVA `0x68820`） |
| 覆盖长度 | 13 字节 |
| expected（原字节） | `48 8b c4 57 41 54 48 81 ec d8 00 00 00` |
| 写入内容 | `48 b8 <stub 绝对地址 8 字节>` + `ff e0` + `90` |
| 依据 | [`engine/player/th06nc/01`](../../../engine/player/th06nc/01-main-loop-and-collision.md) §2 |

`expected` 逐字节的语义（这 13 字节会被原样搬进跳板，所以它们必须是**完整指令**
且**位置无关**——两条都由 `native/check.py` 用 capstone 验过）：

| 偏移 | 字节 | 指令 |
| --- | --- | --- |
| `+0` | `48 8b c4` | `MOV RAX,RSP` |
| `+3` | `57` | `PUSH RDI` |
| `+4` | `41 54` | `PUSH R12` |
| `+6` | `48 81 ec d8 00 00 00` | `SUB RSP,0xd8` |

合计正好 13 = 4 条完整指令，无相对分支、无 RIP 相对寻址 ✅。

### 为什么选这里

`Player__OnUpdate` 是玩家每帧的 tick（chain 回调，**调用者 0 个**）。
在它入口注入意味着：我们写下的输入位，正是这一帧玩家移动代码**马上要读**的那份。
时序天然正确，不需要挑优先级。

## 3. 代理入口：为什么是 `VCRUNTIME140_1.dll`

不用注入器、不用 thcrap，靠 **DLL 代理**。候选有两个，**实测只有一个成立**：

| | `xinput1_4.dll` | `VCRUNTIME140_1.dll` |
| --- | :---: | :---: |
| 在 exe 静态导入表里 | ❌ 运行时 `LoadLibraryW` | ✅ **是** |
| 会不会被跳过 | **会**，见下 | 不会——加载器在 exe 入口点前必须加载 |
| 是 KnownDLL 吗（是则代理不成立） | 否 | 否 |
| 要转发几个导出 | 4 | **1**（`__CxxFrameHandler4`） |
| 实测 | ❌ **没被加载** | 待实测 |

### `xinput1_4.dll` 为什么不成立

它确实被引用（UTF-16 字符串 `th06nc:0x1402c2390`），加载点在 `th06nc:0x1400ab7f4`，
是无标志的 `LoadLibraryW`，按 `1_4 → 1_3 → 9_1_0` 顺序试——这些都对。
**但它上面有个闸门**：

```
1400ab77c  CMP dword ptr [0x14055caa0],ESI   ; ESI = 0
1400ab7b4  JNZ 0x1400ab868                   ; 非 0 -> 整段跳过 XInput 初始化
```

前置子系统初始化没成功就整段跳过，连游戏自己的日志
（`th06nc:0x1402c23f0` = `XInput DLL の読み込み中... `）都不会打印。
用户实测：游戏目录下我们的日志与游戏自带的 `./log.txt` **都是空的** ✅ 与此一致。

### `VCRUNTIME140_1.dll` 的依据

exe 的静态导入表（`th06nc:` 导入目录）里，非 KnownDLL、非 apiset、非 Steam 的只有三项：

| DLL | 导入函数数 |
| --- | --- |
| `MSVCP140.dll` | 36（全是 C++ 修饰名） |
| `VCRUNTIME140.dll` | 14 |
| **`VCRUNTIME140_1.dll`** | **1** |

`api-ms-win-crt-*.dll` 走 **API set** 解析，不吃应用目录重定向，**不能代理**。
`KERNEL32/USER32/ole32` 是 KnownDLL，同样不行。`steam_api64.dll` 刻意排除——
那是 Steam 的正版校验路径，动它性质不同。

于是 `VCRUNTIME140_1.dll` 是唯一既**必定加载**又**只需转发 1 个函数**的选择。
`native/check.py` §F 会把 exe 实际要的函数列表与我们的导出逐项对账。

### 转发 `__CxxFrameHandler4`

它是 C++ 异常展开时由系统 unwinder 调用的 language-specific handler，
调用现场由 unwinder 建立。**不能用 C 包装函数转发**（会插进自己的栈帧），
所以用一条 naked `jmp`：

```
ff 25 <rel32>    jmp qword ptr [rip + disp]   ; -> .data 里的函数指针槽
```

槽的静态初值是 0，`DllMain` 从 `System32\VCRUNTIME140_1.dll` 取真身填进去；
取不到就返回 `FALSE`，让进程在启动期**响亮失败**（`0xc0000142`），
而不是留一个 NULL 等到异常展开时才炸。逐条审计见 [`AUDIT.md`](AUDIT.md) §B.5。

## 4. codecave 调用的引擎函数

**一个都没有。**

整个 mod 只做一件事：往 `INPUT_HELD` 写一个 dword。不调用任何引擎函数，
所以 [`AUDIT-checklist`](../../_template/AUDIT-checklist.md) A 节（调用约定 / 栈参 /
`RET imm`）**整节不适用**——这也是刻意把 mod 设计成这样的原因，
A 节正是历史上唯一出过真 BLOCKER 的地方。

跳板本身要遵守 x64 ABI（它 `call` 我们自己的 C 函数），这部分在 [`AUDIT.md`](AUDIT.md) §B。

## 5. 依赖的全局与结构偏移

全部有一手出处；`native/check.py` 会把它们与 `games/th06nc/symbols.json`
和 `tooling/ghidra/th06nc_player_spec.py` 逐条对账，对不上就失败。

### 全局

| 名字 | 地址（RVA） | 用途 | 依据 |
| --- | --- | --- | --- |
| `Player__OnUpdate` | `0x68820` | hook 目标 | [`player/01`](../../../engine/player/th06nc/01-main-loop-and-collision.md) §2 |
| `INPUT_HELD` | `0xa6ec60` | **唯一写入的东西** | [`menu/02`](../../../engine/menu/th06nc/02-classiclager-unlock-cheat.md) §8 |
| `g_Player` | `0x4ff3a0` | 玩家对象（只读） | [`player/01`](../../../engine/player/th06nc/01-main-loop-and-collision.md) §2.5 |
| `playerMovementArea` | `0x4ff0e0` | 钳位用的可动区（4 个 f32，只读） | [`player/01`](../../../engine/player/th06nc/01-main-loop-and-collision.md) §2.11 |

### `Player` 偏移（全部只读）

| 偏移 | 字段 | 干什么用 | 依据 |
| --- | --- | --- | --- |
| `+0x7730` | `positionCenter.x` | 算与光标的差 | [`player/01`](../../../engine/player/th06nc/01-main-loop-and-collision.md) §2.5 |
| `+0x7734` | `positionCenter.y` | 同上 | 同上 |
| `+0x785c` | `isFocused` | 选常速还是低速 | [`player/01`](../../../engine/player/th06nc/01-main-loop-and-collision.md) §2.10 |
| `+0x7860` | `orthogonalMovementSpeed` | 死区 = 本帧步长 | [`player/01`](../../../engine/player/th06nc/01-main-loop-and-collision.md) §2.10 |
| `+0x7864` | `orthogonalMovementSpeedFocus` | 同上（低速时） | 同上 |
| `+0x7898` | `playerState` | 出生/死亡时不接管 | [`player/01`](../../../engine/player/th06nc/01-main-loop-and-collision.md) §2.9 |

`+0x774c`（`hitboxRadius`）在 `th06nc.h` 里有定义但**当前代码没用到**，
留着是因为它是同一段调研的产物。

### 逻辑按钮位（`INPUT_HELD` 的位定义）

| 位 | 含义 |
| --- | --- |
| `0x01` | 射击（Z） |
| `0x02` | 炸弹（X） |
| `0x04` | 低速（Shift） |
| `0x10 / 0x20 / 0x40 / 0x80` | 上 / 下 / 左 / 右 |

出处：[`engine/menu/th06nc/02`](../../../engine/menu/th06nc/02-classiclager-unlock-cheat.md) §8。
mod 只改 `0xf0` 这四位，其余位一位不碰。

## 6. 屏幕布局：NC 与 TH06 **不同**（已实测确认）

| 量 | 值 | 来源 |
| --- | --- | --- |
| 自机坐标系 | 关卡区局部，原点在关卡区左上角 | ✅ 一手：出生点硬编码 `(192,384)` |
| 可动区 | `(8,16) + (368,416)` | ✅ 运行时读 `0x1404ff0e0`；实测右界 376 与 TH06 逐值相同 |
| 关卡区尺寸 | `384 x 448` | ✅ 与 TH06 相同 |
| **虚拟屏** | 高固定 **480**，宽随窗口宽高比扩展（**不做 4:3 信箱**） | ✅ 两次实测标定 |
| **缩放** | `s = 客户区高 / 480` | ✅ 实测 `1.5000` / `2.2500`，两组精确命中 |
| **关卡区水平位置** | **居中**：`left = (客户区宽 - 384s) / 2` | ✅ 实测左边界虚拟 `235.33` / `235.11`，居中值 `234.67` |
| **关卡区顶边** | 虚拟 `y = 16`（与 TH06 相同） | ✅ 实测 `16.67` / `14.22` |

### 这就是那个「偏右 200px」的根因

**TH06 的关卡区左边界固定在 `x = 32`**（`GameManager.cpp:286`，右侧留 HUD 栏）。
**NC 把它居中了。** 照抄 TH06 的布局去反推光标位置，误差 = 真实左边界 − 假设左边界：

| 客户区 | 旧代码假设的左边界 | 实际左边界 | 自机偏右 |
| --- | --- | --- | --- |
| 1280×720 | 208 px | 352 px | **144 px** |
| 1920×1080 | 312 px | 528 px | **216 px** |

与最初报告的「约 200px」吻合 ✅ —— 这是模型的第三重独立验证
（另两重：两个分辨率下反推出的左边界一致；`s = ch/480` 在两组上精确命中）。

### 复现

`native/check.py` §G 把这两组实测点击点钉成回归样本：模型预测与实测最大差
**7 px**（手点两角的精度量级），而旧的 4:3 信箱模型差 **211 px**。

### 仍未一手确认的部分

这套布局是**从两组实测数据拟合**出来的，不是从二进制读出来的常量 🟡。
`arcadeRegionTopLeftPos` 在 NC 的代码里没找到——很可能压根不存在，
关卡区的定位由 D3D 视口/投影矩阵完成 ❓。

两组样本都是 **16:9**。非 16:9（例如 4:3 窗口）下模型未验证——
按公式会得到 `left = (640 - 384)/2 = 128`（虚拟单位），合理但没测过。
布局不对时用标定覆盖即可，见 [`README.md`](README.md)。

## 7. 换版本时必须重取

- [ ] `Player__OnUpdate` 地址与入口 13 字节（重编译后全变）
- [ ] `INPUT_HELD` / `g_Player` 地址
- [ ] 全部 `Player` 字段偏移（结构会长大、会重排——NC 相对 TH06 就重排过，
      见 [`engine/ecl/th06nc/04`](../../../engine/ecl/th06nc/04-enemy-struct.md) §1）
- [ ] 按钮位定义（不要假设不变）
- [ ] 复核屏幕布局（§6 是拟合结果，换 build 后重标一次确认）
- [ ] 重新确认 `SetDefaultDllDirectories` 仍未被调用，否则代理路线失效

可以借的只有：方法论、字段语义、跳板设计、踩过的坑。
