# TARGET —— th18_bgwarp 死绑登记

> **版本**：TH18 v1.00a（`th18.exe`，imagebase `0x400000`）。本文裸地址默认属该版本；引用别的版本须写成 `th16:0x…`。

**这份文件回答一个问题：换一个 exe build，我要重取哪些量？**
本 DLL **不装 binhack / 不声明断点 / 不读游戏数据文件**，它自己在两处挂内联 hook 并改写一处顶点数组。
所以「写入点」= 两个 hook 站点；「读取点」= 它顺带观测的网格/舞台字段。

## 目标二进制

| 项 | 值 |
| --- | --- |
| 游戏 / 版本 | TH18 v1.00a |
| exe md5 | `9969cac756098c1da05a81de45437a70` |
| 大小 | 847360 B |
| imagebase | `0x400000` |
| DYNAMICBASE | **无**（`DllCharacteristics=0x8100`）⇒ 镜像必落 `0x400000` |
| thcrap 版本匹配 | **不依赖** —— 本 DLL 按下面 4 组签名自己认版本 |

⚠️ 虽然该 exe 无 ASLR，DLL 仍按 `GetModuleHandleW(NULL)` 取实际基址再加 RVA，不假设 `0x400000`。

## 写入点（两个内联 hook；`make check` 逐字节验签 + 验指令边界）

| # | 地址 | RVA | 被搬走的原字节（cavesize） | 语义 |
| --- | --- | --- | --- | --- |
| ① | `0x42b540` | `0x2b540` | `55 8b ec 51 57 8b f9`（7） | `FUN_0042b540` 入口：把 `mesh+0x14` 的顶点拷进各 VM 的 `+0x5c8` |
| ② | `0x41ee38` | `0x1ee38` | `8b 7c 24 1c 33 c9`（6） | `StageInner____on_tick_part_2` 的 `[mode==2]` 逐顶点重算**循环出口** |

★ 两个窗口都必须是**整数条指令且不含相对寻址**（跳板把这 6–7 字节原样搬走再执行）——
`native/check_constants.py` 用 capstone 逐条验，切在指令中间或含 `call/jmp rel` 就直接 FAIL。

## 签名校验点（相当于 binhack 的 `expected`；任一不匹配即自我卸载）

| # | 地址 | RVA | 原字节（16） | 依据 |
| --- | --- | --- | --- | --- |
| A | `0x42b540` | `0x2b540` | `55 8b ec 51 57 8b f9 8b 07 85 c0` | 网格→VM 顶点缓冲的上传函数 |
| B | `0x42b3e0` | `0x2b3e0` | `55 8b ec 83 ec 10 57 8b f9 0f 28 c2` | 建网格（`xmm1/xmm2/xmm3` + 栈上 1 个 float）并调用 A |
| C | `0x41e7b4` | `0x1e7b4` | `a1 c0 f2 4c 00 85 c0 74 0a f6 40 78 01` | 非符判定：读符卡管理器 `[0x4cf2c0]` 的 `+0x78` bit0 |
| D | `0x41ee38` | `0x1ee38` | `8b 7c 24 1c 33 c9 f3 0f 10 8f 34 34 00 00` | `[mode==2]` 旋涡循环出口 |

## 读取点（只观测，不改）

| # | 地址 / 算式 | 类型 | 语义 | 依据 |
| --- | --- | --- | --- | --- |
| 1 | `mesh+0x00` / `+0x04` | int | 行数（ctor 写死 `0x11`=17）/ 列数 | ctor `0x42b22d` / `0x42b233` |
| 2 | `mesh+0x08` | int | sprite id（`0x7fff` = 屏幕面特例） | ctor `0x42b275`–`0x42b2d2` |
| 3 | `mesh+0x10` | `zAnmVm*[17]` | 每段一台 VM（16 段被 `0x42b540` 填顶点） | ctor `0x42b360`–`0x42b3c6` |
| 4 | `mesh+0x14` | 28 B/顶点 | `{x,y,z,w, D3DCOLOR, u,v}`（7 dword） | 建网格 `0x42b480`–`0x42b4eb` |
| 5 | `vm+0x5c8` | `void*` | `special_vertex_buffer_data`，`draw_vm` 推给 D3D | 上传读点 `0x42b56b`；绘制 `0x481643`／`0x48166d`／`0x481697` |
| 6 | `stage+0x3410` | `void*` | 那份网格句柄 | 建 `0x41dace`；用 `0x41e77c` |
| 7 | `stage+0x3414` | float | 半径阈值 **112.0f** | `0x41da0d` 写；`0x41eae5` 比较 |
| 8 | `stage+0x3418` | float | 当前半径（`>`阈值时回退 `2.0f`） | `0x41eadd` 读；`0x41eaff` 写 |
| 9 | `stage+0x3434` / `+0x3438` | float | 相位/幅度（`0x41e78a`／`0x41e797` 读进 [esp+0x10]/[esp+0x18]） | 逐顶点重算里当振幅用 |
| 10 | `stage+0x343c` | int | 网格模式：`1` = 波浪，`2` = 旋涡圆盘 | `0x41e7ab` 分派 / `0x41ead4` 再分派 |

## hook 函数的调用约定（这是本 DLL 唯一的 ABI 假设）

`FUN_0042b540` 是 **thiscall**：`ecx = mesh`，无栈参（尾部 `ret`）。
本 DLL **不调用**它，只在它入口挂跳板 —— 跳板 `pushad/pushfd` 后把 `&x86_reg_t` 传给 C 处理函数，
返回后 `popfd/popad` 再执行被搬走的原字节。C 处理函数只读 `regs->ecx`（= mesh）与
`regs->retaddr`（= 调用者返回地址），**不改 ESP、不碰 FPU 栈**。

★ 一手出处：`0x42b529` 的调用点前是 `mov ecx,edi`，函数尾是 `c2 04 00`（`ret 4`）与裸 `c3` 两支 —— 见 `0x42b5d0` 一带。

## 结构偏移（来自网格 ctor `0x42b1f0` 一手）

```text
mesh (= operator_new(0x1c)；ctor 参数 (n 写死 0x11, m, sprite_base))
  +0x00 int    n (=17 行)
  +0x04 int    m (列数)
  +0x08 int    sprite id
  +0x0c int[17] 每段 sprite id（malloc(0x43)）
  +0x10 ptr[17] 每段 zAnmVm*（malloc(0x43)）
  +0x14 byte[]  顶点区（malloc(0x1dc × m) = 28 B/顶点）
  +0x18 byte[]  位置副本（malloc(0xcc × m) = 12 B/顶点）
```

## 换版本时必须重取

- [ ] 4 组签名（A–D）的地址与原字节
- [ ] 两个 hook 窗口的长度（必须仍是整数条指令、无相对寻址）
- [ ] `mesh` 的字段偏移（`0x00/0x04/0x08/0x10/0x14/0x18`）与顶点步长（现为 28）
- [ ] `stage` 的字段偏移（`0x3410/0x3414/0x3418/0x3434/0x3438/0x343c`）
- [ ] `x86_reg_t` 的字段顺序（thcrap 侧：`thcrap/src/expression.h`；本仓复刻：`mods/th18.v1.00a/card-expand/native/thcrap_bp.h`）
- [ ] 上传调用点的返回地址（现为 `0x2b52e`）—— 注入的白名单就靠它

可以借的只有：方法论（跳板 + 指令边界验签）、字段的*语义*、「网格 → 每段 VM 的 `+0x5c8` → `draw_vm`」这条链路形状。
