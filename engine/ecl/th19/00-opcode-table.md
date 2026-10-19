# 00 — TH19 的 ECL 派发机制与 opcode → handler 表

> **版本**：TH19 v1.10c（`th19.exe`，imagebase `0x400000`，样本 md5 `dd0e1ff8c02327bf6408f763b8beedc9`，已脱壳）。
> 本文裸地址默认属该版本。一手来源：`objdump` 反汇编 + 派发表直读（不依赖 Ghidra 命名）。
> 相关：[`01-et-family-args.md`](01-et-family-args.md)、[`../OVERVIEW.md`](../OVERVIEW.md)。

## 1. 解释器与两级派发

解释器 = `0x4fecc0`（全 exe 最大函数，25 994 B，1 个 xref）。取指与派发：

```
0x4fecf6  movzx edx, WORD PTR [esi+0x4]      ; opcode = 当前指令 +0x4（u16）
0x4feceb  call 0x56bef0                       ; esi = 当前指令指针（[ctx+0x55c8]+0xc 取的游标）
0x4fecff  add ecx, 0xfffffed4                 ; ecx = opcode − 300
0x4fed09  cmp ecx, 0x2e6                      ; 范围闸 0..742 ⇒ 覆盖 opcode 300..1042
0x4fed15  movzx eax, BYTE PTR [ecx+0x505560]  ; 一级：字节表 @0x505560（743 项）
0x4fed1c  jmp DWORD PTR [eax*4+0x50524c]      ; 二级：指针表 @0x50524c（197 项 → 每次 196 个不同 handler）
```

⇒ **`handler(opcode) = dword[0x50524c + 4 × byte[0x505560 + (opcode − 300)]]`**，`opcode ∈ [300, 1042]`。

* ★ **系统码（< 300）不走这张表**：`0x4fed0f` 的 `ja 0x505234` 把越界/低值分支出去，系统码在解释器函数体内另行处理
  （与 TH16 的"系统码内联、游戏码走 `vtable[0]`"同构，见 [`../th16/04-ecl-vm-interpreter.md`](../th16/04-ecl-vm-interpreter.md)）。
* ★ 一级是**字节**索引、二级只有 197 项 —— 所以按"扫大跳转表"的办法找它**找不到**（本仓曾试过两种扫法皆落空）。

### 1.1 完整链路（含调用方与 `this` 偏移）

```
0x5687d0   EclRunContext::ecl_run          ; 取指层（vm = [ctx+0x1018] → fm = [vm+0x1218] → subs = [fm+0x20c]）
                                            ; instr = subs[sub].code + 0x10 + off
           opcode ≤ 0x61 (97)  → 内联处理：byte 表 0x56b0d8 + dword 表 0x56afbc（71 项）
           opcode > 0x61       → 0x56af16 虚调用 vtable[1]
0x5a852c   敌机 vtable；vtable[1] = 0x5058b0 = `add ecx,0x1230; jmp 0x4fecc0`
                                            ; ★ 所以 handler 的 `this` = enemy + 0x1230（不是 enemy 本身）
```

* ★ **opcode 98..299 是 no-op**（`op − 300` 为负 ⇒ `ja 0x505234` 直接 `return 0`）。
* 参数 API：`0x50c1b0` = int、`0x50c1d0` = float、`0x50d050` = int **输出**地址、`0x50d060` = float **输出**地址；
  `push` 的立即数就是参数序号 —— 读 handler 时按这个顺序数参数。

## 2. 取参与写回助手

| 地址 | 作用 |
| --- | --- |
| `0x50c1b0` | `get_int_arg(N)`（`push N; mov ecx, ctx; call`） |
| `0x50c1d0` | `get_float_arg(N)` |
| `0x50d050` / `0x50d060` | 取 **ECL 输出变量**地址（int / float；`569` 用 `0x50d060(0/1)` 写回两个分量） |
| `0x40ebd0` / `0x40ebc0` / `0x40ee10` / `0x40ee00` | 取（某结构体基址, 槽号）的元素地址；不同助手对应不同结构体 |
| `0x402780` | 把 vec3 清零 |
| `0x404010` | **vec3 相加**（逐分量 `addss`，`out = a + b`） |
| `0x404530` | 由角度造方向向量（`569` 用） |
| `0x404a70` | 角度预处理（`604` 用；语义未定 ❓） |

## 3. opcode 号是**逐作重编**的（★ 别拿别作的 map 套）

`thtk` 的 `get_format(19, id)` 的 fallthrough 链是
`th19 → th185 → th18 → th17 → th165 → th16 → th15 → th143 → th14 → th13 → th128 → th125 → th12 → th11 → alcostg → th10`。

C3–C4 闭包里 51 种 opcode 的来源：**`th13` 43 个、`th19` 7 个、`th185` 1 个**。而 TH19 对若干号**重新定义**了形状：

| opcode | TH13 的形状 | TH19 的形状 |
| ---: | --- | --- |
| `618` | `Sffffffffff`（11 参） | `Sff` |
| `620` | `SSSSSSS`（7 参） | `Sff` |
| `621` | `SSSSSSSSSSS`（11 参） | `fS` |
| `622` | `SSSSS`（5 参） | `S` |
| `624` | `Sffffffff`（9 参） | `S` |
| `569` | `th15: S` | `fffff` |
| `570`／`571` | `th15: 空` | 9 元表 |

⇒ ★ 「拿 TH18 的 `618 = etSpeedR5` 套 TH19」在形状上就不成立。

## 4. 与卡 109 相关的逐条释读

C3–C4 闭包那 51 条（含 ✅/🟡/❓ 分级与每条的证据、未解原因）在
[`mods/th18.v1.00a/card-expand/HANDOFF-109-TH19-C34-ECL.md`](../../../mods/th18.v1.00a/card-expand/HANDOFF-109-TH19-C34-ECL.md) §4；
`et` 族的字段布局见 [`01-et-family-args.md`](01-et-family-args.md)。
