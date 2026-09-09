# ANM v8 Math + Jumps 组 — TH16 一手逐条确认（opcode 100–131 / 200–213，46 条）

> **版本**：TH16 v1.00a（`th16.exe`，imagebase `0x400000`）。本文裸地址默认属该版本；引用其他版本须写成 `th18:0x…`。
> **方法**：**不经 Ghidra 渲染**——直接解 PE 节表把 `.text` 字节喂 capstone，逐条反汇编 46 个 case 体
> 与它们调用的 12 个 helper；两张分发表（`0x462d8c` 索引 + `0x462cb8` 跳表）逐字节解出来自证
> 「opcode → case 地址」；再用自写解析器跑 `local/th16.v1.00a/dat/` 的 **54 个真实 `.anm`
> （1929 script / 43268 指令，其中本组 2525 条）**做对账与证伪。
> **可信度**：46 条全部 ✅（每条都指得到一条机器指令）。与社区（thpages / thtk `v8.anmm`）**3 处不一致**，
> 见 [§7](#7-与社区不一致处的裁决)；顺带更正本仓 `engine/_shared/math-and-prng.md` 一条 🟡（[§7.4](#74-本仓内部更正prng-流别不是全部走-stream-a)）。
> **衔接**：指令头 8 字节与 `param_mask` 见 [`01-format.md`](01-format.md) §5；VM 主循环、时间闸门、
> `zAnmVm` 字段图见 [`02-vm-core.md`](02-vm-core.md)。本文不重复这两份的结论。
> **不含版权字节**：只写偏移、地址与语义。

## 0. 一句话结论

这 46 条是 ANM VM 的**纯计算层**：32 条算术/数学（100–131）写 VM 自己的变量槽，
14 条跳转（200–213）只改 `vm.instr_offset` 与 `vm.script_time` 两个字段。四件容易搞错的事，一手结论是：

1. **`I0–I3` = 变量号 10000–10003（`vm+0x4a0` 起的 `s32[4]`）；`F0–F3` = 10004–10007（`vm+0x4b0` 起的 `f32[4]`）**，
   整型指令把变量号编成 **int32**、浮点指令编成 **float32**（[§3](#3-专项-i0i3--f0f3-的编号与存储位置)）。
2. **整数除 0 与取模 0 没有任何保护，直接 `idiv` → #DE 崩溃**；浮点除 0 走 IEEE（±inf / NaN）；
   整数取模的余数**符号随被除数**（[§4](#4-专项-除零--取模的边界)）。
3. **跳转目标是「相对该 script 起点的字节偏移」**，857 个真实跳转 100% 落在指令边界上（[§5](#5-专项-跳转目标是字节偏移不是指令下标)）。
4. **跳转成功后脚本时间被 `Timer::set_value(t)` 直接改写成参数里的 `t`**，不是保持、不是清零，
   也不必等于目标指令的 `time` 标签（[§6](#6-专项-跳转后-time-怎么设)）。

---

## 1. 怎么核的：opcode → case 地址是自己解跳表得到的

**① 发现**：`th-re-data` 在 `AnmVm::run` `0x45f980` 函数体内给了 137 条 case label（社区单源），
形如 `0x45fce8 100__set`、`0x45fbfc 200__jmp`；其中 122–131 十条只有裸数字，没有名字。

**② 推测**：这些 label 的「数字前缀」就是它服务的 opcode。**这是待验假设**——label 是人打的，可能错位。

**③ 验证**：不看 label，直接从 PE 里把分发用的两张表读出来。派发序言在 `0x45fbce`：

```
0045fbce  movsx eax, word ptr [edi]        ; ins.opcode（s16）
0045fbd1  cmp   eax, 0x12c                 ; 300
0045fbd6  jg    0x460c46                   ; 301..613 走高段表
0045fbdc  je    0x460ba9                   ; 300 直接内联
0045fbe2  inc   eax                        ; idx = opcode + 1
0045fbe3  cmp   eax, 0xd6                  ; 214（无符号）
0045fbe8  ja    0x4629c8                   ; 越界 → 默认分支（= nop）
0045fbee  movzx eax, byte ptr [eax + 0x462d8c]   ; 索引表（215 项）
0045fbf5  jmp   dword ptr [eax*4 + 0x462cb8]     ; 跳表（53 项）
```

把 `0x462d8c` 的 215 字节与 `0x462cb8` 的 53 个 dword 逐字解出来（VA → 文件偏移 → `struct.unpack`），
得到 opcode → case 地址的映射。**本组 46 条的落点与社区 label 逐条吻合，0 处错位** ✅。
顺带定死一件事：低段索引表覆盖 opcode **−1 到 213**（`inc eax` 后无符号比 214），**213 就是低段的最后一条**。

**④ 结论**：本文全部 46 个 case 地址是**一手**的，不依赖社区 label ✅（TH16）。

**⑤ 证据**：`0x45fbce`–`0x45fbf5`；表 `0x462d8c`（`u8[215]`，下标 = opcode + 1）与 `0x462cb8`（`void*[53]`）。

### 1.1 每条 case 的公共骨架

46 条 handler 长得高度一致，读懂一条就读懂一族：

```c
// EBX = zAnmVm*, EDI = 当前指令指针, ins.args[i] = *(u32*)(EDI + 8 + 4*i)
// 取右值：mask 位置位 → 查变量；否则就是字面量
v = (ins.param_mask & (1<<i)) ? get_var(vm, ins.args[i]) : ins.args[i];
// 取左值：mask 位置位 → 变量槽指针；否则 → 指向指令流里那个参数本身（★ 见 §9）
p = (ins.param_mask & (1<<i)) ? get_var_ptr(vm, &ins.args[i]) : &ins.args[i];
...计算并写 *p...
vm.instr_offset += ins.size;        // movzx eax,[edi+2] ; add [ebx+0x28],eax
goto 0x45fb9a;                      // end_loop_iter → begin_loop_iter
```

四个 helper（都是 `AnmVm::` 的成员，`ECX = vm`）：

| helper | 地址 | 入参 | 出参 | 越界/非变量时 |
| --- | --- | --- | --- | --- |
| `get_int_var` | `0x45f610` | 栈：变量号（s32） | `EAX` | 返回入参本身（`0x45f708`） |
| `get_float_var` | `0x45f2d0` | `XMM1`：变量号（f32） | `XMM0` | 返回入参本身（`0x45f570`） |
| `get_int_var_ptr` | `0x45f890` | 栈：**指向变量号的指针** | `EAX` = `s32*` | 返回那个指针本身（`0x45f8f6`） |
| `get_float_var_ptr` | `0x45f780` | 栈：**指向变量号的指针** | `EAX` = `f32*` | 返回那个指针本身（`0x45f82a`） |

两个 `_ptr` 版**返回的是可写指针**，且默认分支把入参指针原样退回来——那正是指令流里的那 4 个字节。
这条「非变量左值 = 自修改字节码」的性质是 [§9](#9-写播放器会踩的坑) 的第一坑。

---

## 2. ★ 46 条指令表

**记号**：`$x` = 整型左值（变量号，int32 编码）· `S` = 整型右值 · `%x` = 浮点左值（变量号，**float32 编码**）·
`f` = 浮点右值 · `Soff` = 原始 s32 字节偏移，**不查 `param_mask`** · `St` = 原始 s32 时间，**不查 `param_mask`**。
`size` = 8 + 4 × 参数个数，54 个真实文件里 100% 吻合（[§8](#8-真实数据对账54-个-anm)）。

| opcode | 助记（thpages / thanm） | 签名 | 语义 | case 地址 | 可信度 |
| :---: | --- | --- | --- | --- | :---: |
| 100 | `set` / `iset` | `$a, S b` | `a = b` | `0x45fce8` | ✅ |
| 101 | `setF` / `fset` | `%a, f b` | `a = b` | `0x45fd26` | ✅ |
| 102 | `add` / `iadd` | `$a, S b` | `a += b` | `0x460169` | ✅ |
| 103 | `addF` / `fadd` | `%a, f b` | `a += b` | `0x4601a7` | ✅ |
| 104 | `sub` / `isub` | `$a, S b` | `a -= b` | `0x4601ed` | ✅ |
| 105 | `subF` / `fsub` | `%a, f b` | `a -= b` | `0x46022b` | ✅ |
| 106 | `mul` / `imul` | `$a, S b` | `a *= b`；`imul`，溢出按 32 位截断 | `0x460271` | ✅ |
| 107 | `mulF` / `fmul` | `%a, f b` | `a *= b` | `0x4602b4` | ✅ |
| 108 | `div` / `idiv` | `$a, S b` | `a = a / b`；`idiv` 截零，`b == 0` 崩，见 §4 | `0x4602fa` | ✅ |
| 109 | `divF` / `fdiv` | `%a, f b` | `a /= b`；IEEE `divss` | `0x46033f` | ✅ |
| 110 | `mod` / `imod` | `$a, S b` | `a = a % b`；余数符号随 `a`，`b == 0` 崩 | `0x460385` | ✅ |
| 111 | `modF` / `fmod` | `%a, f b` | `a = fmod(a, b)`（双精度算完再回 float） | `0x4603ca` | ✅ |
| 112 | `add3` / `isetAdd` | `$x, S a, S b` | `x = a + b` | `0x45fd68` | ✅ |
| 113 | `addF3` / `fsetAdd` | `%x, f a, f b` | `x = a + b` | `0x45fdc5` | ✅ |
| 114 | `sub3` / `isetSub` | `$x, S a, S b` | `x = a - b` | `0x45fe2c` | ✅ |
| 115 | `subF3` / `fsetSub` | `%x, f a, f b` | `x = a - b` | `0x45fe87` | ✅ |
| 116 | `mul3` / `isetMul` | `$x, S a, S b` | `x = a * b` | `0x45feee` | ✅ |
| 117 | `mulF3` / `fsetMul` | `%x, f a, f b` | `x = a * b` | `0x45ff4a` | ✅ |
| 118 | `div3` / `isetDiv` | `$x, S a, S b` | `x = a / b`；`b == 0` 崩 | `0x45ffb1` | ✅ |
| 119 | `divF3` / `fsetDiv` | `%x, f a, f b` | `x = a / b`；IEEE | `0x460011` | ✅ |
| 120 | `mod3` / `isetMod` | `$x, S a, S b` | `x = a % b`；余数符号随 `a`，`b == 0` 崩 | `0x460078` | ✅ |
| 121 | `modF3` / `fsetMod` | `%x, f a, f b` | `x = fmod(a, b)`；**先算 `b` 后算 `a`**，见 §9 | `0x4600d8` | ✅ |
| 122 | `rand` / `isetRand` | `$x, S n` | `x = (u32)prng() % (u32)n`；`n == 0` 时 `x = 0`（**不崩**） | `0x46045b` | ✅ |
| 123 | `randF` / `fsetRand` | `%x, f r` | `x = randf01() * r`，`randf01 ∈ [0, 1)` | `0x460492` | ✅ |
| 124 | `mathSin` / `fsin` | `%x, f θ` | `x = (float)sin((double)θ)` | `0x4604f2` | ✅ |
| 125 | `mathCos` / `fcos` | `%x, f θ` | `x = (float)cos((double)θ)` | `0x46052d` | ✅ |
| 126 | `mathTan` / `ftan` | `%x, f θ` | `x = (float)tan((double)θ)` | `0x460568` | ✅ |
| 127 | `mathAcos` / `facos` | `%x, f v` | `x = (float)acos((double)v)`；`abs(v) > 1` → NaN | `0x4605a3` | ✅ |
| 128 | `mathAtan` / `fatan` | `%x, f v` | `x = (float)atan((double)v)`；`±inf → ±π/2` | `0x4605de` | ✅ |
| 129 | `mathReduceAngle` / `validRad` | `%θ` | θ 折进 `[-π, π]`，**最多 33 次 ±2π**，见 §7.1 | `0x460619` | ✅ |
| 130 | `mathCirclePos` / `circlePos` | `%x, %y, f θ, f r` | `x = cos(θ)*r; y = sin(θ)*r`（一次 `fsincos`） | `0x460663` | ✅ |
| 131 | `mathCirclePosRand` / `circlePosRand` | `%x, %y, f lo, f hi` | 随机角 × 随机半径，**半径区间不是 `[lo,hi]`**，见 §7.2 | `0x4606f5` | ✅ |
| 200 | `jmp` | `Soff dest, St t` | `instr_offset = dest; script_time = t`；两参**不可为变量** | `0x45fbfc` | ✅ |
| 201 | `jmpDec` | `$x, Soff dest, St t` | `--x`；**减完 `x > 0`** 才跳，否则顺序执行 | `0x45fc12` | ✅ |
| 202 | `jmpEq` / `ije` | `S a, S b, Soff dest, St t` | `a == b` 时跳（`cmp` / `je`） | `0x4607ce` | ✅ |
| 203 | `jmpEqF` / `fje` | `f a, f b, Soff dest, St t` | `a == b` 时跳；有 NaN → **不跳** | `0x460816` | ✅ |
| 204 | `jmpNe` / `ijne` | `S a, S b, Soff dest, St t` | `a != b` 时跳 | `0x460875` | ✅ |
| 205 | `jmpNeF` / `fjne` | `f a, f b, Soff dest, St t` | `a != b` 时跳；有 NaN → **跳** | `0x4608bd` | ✅ |
| 206 | `jmpLt` / `ijl` | `S a, S b, Soff dest, St t` | `a < b` 时跳（`jl`，有符号） | `0x46091c` | ✅ |
| 207 | `jmpLtF` / `fjl` | `f a, f b, Soff dest, St t` | `a < b` 时跳；有 NaN → 不跳 | `0x460964` | ✅ |
| 208 | `jmpLe` / `ijle` | `S a, S b, Soff dest, St t` | `a <= b` 时跳（`jle`） | `0x4609bb` | ✅ |
| 209 | `jmpLeF` / `fjle` | `f a, f b, Soff dest, St t` | `a <= b` 时跳；有 NaN → 不跳 | `0x460a03` | ✅ |
| 210 | `jmpGt` / `ijg` | `S a, S b, Soff dest, St t` | `a > b` 时跳（`jg`） | `0x460a5a` | ✅ |
| 211 | `jmpGtF` / `fjg` | `f a, f b, Soff dest, St t` | `a > b` 时跳；有 NaN → 不跳 | `0x460aa2` | ✅ |
| 212 | `jmpGe` / `ijge` | `S a, S b, Soff dest, St t` | `a >= b` 时跳（`jge`） | `0x460afd` | ✅ |
| 213 | `jmpGeF` / `fjge` | `f a, f b, Soff dest, St t` | `a >= b` 时跳；有 NaN → 不跳 | `0x460b41` | ✅ |

### 2.1 三个共用尾巴

不是每条 case 都自带存结果的代码——它们互相跳：

| 尾巴 | 地址 | 干什么 | 谁用 |
| --- | --- | --- | --- |
| `set` 的整型存值 | `0x45fcff` | 解析 `arg0` 的整型左值指针，`*p = ESI`，推进指针 | 100、122 |
| `setF` 的浮点存值 | `0x45fd3f` | 解析 `arg0` 的浮点左值指针，`*p = XMM0`，推进指针 | 101、124–128 |
| 「跳转成功」 | `0x460b88` | `Timer::set_value(&vm.script_time, arg3)`；`vm.instr_offset = arg2` | 202–213 全部 12 条 |
| 默认分支 | `0x4629c8` | 只 `instr_offset += size`（= nop） | 201/213 的不跳分支 |

**12 条条件跳转共用同一个 `0x460b88`** ✅ —— 所以它们的 `dest`/`t` 一定在同样的参数位（`arg2`/`arg3`），
而且 `0x460b88` 里**没有任何 `test [edi+6], …`**，即 `dest` 与 `t` **永远按字面量读，不能是变量** ✅。
`200 jmp` 的 handler 同理，整段 `0x45fbfc`–`0x45fc10` 一次 `param_mask` 都没查 ✅。

### 2.2 122–131 十条：社区 Ghidra label 是裸数字，但它们不是算术

**① 发现**：`th-re-data` 对 `0x46045b`–`0x4606f5` 这十个 case 只给了 `122`…`131` 的裸数字 label。
按位置猜，很容易以为是「另一批三操作数算术」。

**② 推测**（两个并列）：(a) 又一组 `x = a op b`；(b) 别的东西。

**③ 验证**：逐条反汇编，全部落到 CRT / 引擎 helper 上，**没有一条是普通算术**：

| opcode | 调的 helper | 一手判据（怎么认出这个 helper 是什么） |
| :---: | --- | --- |
| 122 | `0x402be0`（PRNG raw draw，对象 `0x4a6d80`） | `xor edx,edx; div esi` 取余数；`n == 0` 有显式短路到 `xor esi,esi` |
| 123 | `0x402c70` | 本仓 [`../../_shared/math-and-prng.md`](../../_shared/math-and-prng.md) §3.6 已解出 = `u·2⁻³² ∈ [0,1)`；这里 `fmul` 上 `arg1` |
| 124 | `0x4884c0` | 常量池含 sin 的 Taylor 系数 `-1/6, 1/120, -1/5040, 1/362880` + `32/π` 归约 |
| 125 | `0x488300` | 同一族，但归约索引偏置多 `0x10`（= 16 个 π/32 扇区 = **+π/2**）⇒ `cos(x) = sin(x + π/2)` |
| 126 | `0x488790` | 与 sin/cos 同一套 `32/π` 扇区归约，但多一条 `divsd 1.0 / …`（倒数）与 NaN 常量 ⇒ **tan** |
| 127 | `0x487af0` | 常量池是 asin 的 Taylor 系数 `1/6, 3/40, 15/336, 105/3456, 945/42240, 10395/599040`，另有 3 条 `sqrtsd`、`π/2`、**`π`** ⇒ **acos** |
| 128 | `0x488010` | `0x4882a6` 判 `abs(x) == +inf` 后返回 `[0x494ad0 + 8*sign]` = **±π/2**；另有 `-1.0 / x` 归约 ⇒ **atan** |
| 129 | `0x402d30`（`math_wrap_angle_402d30`，ExpHP 已命名） | 两个 `while` 循环 ±`2π`，界 `±π`；第二参数由 case 传 `0`（`xorps xmm1,xmm1`） |
| 130 | `0x464930`（`cartesian_from_polar_464930`） | `fld θ; fsincos; fmul r; fstp *ECX; fmul r; fstp *EDX` |
| 131 | `0x464d60`（`cartesian_from_polar_464d60`） | 同上，但两个输出写进**同一个 `float[2]`**；角与半径由 case 先抽随机 |

**④ 结论**：122–131 = **随机 2 条 + 单参数学函数 5 条 + 角度归一 1 条 + 极坐标 2 条** ✅（TH16）。
与 thpages 的 `rand / randF / mathSin / mathCos / mathTan / mathAcos / mathAtan / mathReduceAngle /
mathCirclePos / mathCirclePosRand` 完全对上——**但它对 131 的描述是错的**，见 [§7.2](#72-131-mathcircleposrand-的半径区间社区错)。

**⑤ 证据**：case 地址见 §2 表；helper 地址与常量地址见本小节；常量值由直读 PE 得到
（`0x494ad0` = `1.5707963267948966`、`0x494ad8` = `-1.5707963267948966`、
`0x494588` = float `π`、`0x4945b8` = float `2π`、`0x494734` = float `-π`）。

> **函数身份是靠数值指纹钉的，不是靠符号表**（MSVC 静态链接，`__libm_sse2_*` 没留名字）。
> 上表每一行都写明了指纹是什么，复核者可以直接读那几个常量地址自证。sin/cos 的区分尤其干净：
> 两份代码逐指令同构，唯一差别是 `0x488540` 的 `add edx, 0x1c7600` 与 `0x488380` 的 `add edx, 0x1c7610`。

---

## 3. 专项① `I0–I3` / `F0–F3` 的编号与存储位置

**① 发现**：四个 helper 里各有一张「变量号 → VM 字段偏移」的两级表。逐字解出来（不是读反编译，是读字节）：

`get_int_var_ptr` `0x45f890`：`eax = *(s32*)arg - 10000`，`cmp eax, 0x1d`（29），
索引表 `0x45f91c`（30 项）+ 跳表 `0x45f8fc`（8 项）。
`get_float_var_ptr` `0x45f780`：`eax = cvttss2si(*(f32*)arg) - 10004`，`cmp eax, 0x1f`（31），
索引表 `0x45f870`（32 项）+ 跳表 `0x45f830`（16 项）。

**② 推测**：10000 起是整型槽、10004 起是浮点槽（社区 thpages 的 `anmvar:i0` / `anmvar:f0` 也这么说）。

**③ 验证**：把两张表全部展开，**可写**（左值）的变量号只有下面这些，其余一律落默认分支：

| 变量号 | 字段 | 类型 | 谁能写 | 一手证据 |
| :---: | --- | --- | --- | --- |
| 10000–10003 | `+0x4a0` `+0x4a4` `+0x4a8` `+0x4ac` | `s32[4]` = **I0–I3** | `get_int_var_ptr` | `0x45f8b0`/`0x45f8ba`/`0x45f8c4`/`0x45f8ce` |
| 10004–10007 | `+0x4b0` `+0x4b4` `+0x4b8` `+0x4bc` | `f32[4]` = **F0–F3** | `get_float_var_ptr` | `0x45f7a6`/`0x45f7b0`/`0x45f7ba`/`0x45f7c4` |
| 10008 / 10009 | `+0x4cc` / `+0x4d0` | `s32` | `get_int_var_ptr` | `0x45f8d8` / `0x45f8e2` |
| 10013–10015 | `+0x2c` `+0x30` `+0x34` = `pos.xyz` | `f32` | `get_float_var_ptr` | `0x45f7ce`/`0x45f7d5`/`0x45f7dc` |
| 10023–10025 | `+0x38` `+0x3c` `+0x40` = `rotation.xyz` | `f32` | `get_float_var_ptr` | `0x45f7e3`/`0x45f7ea`/`0x45f7f1` |
| 10027 / 10028 | `+0x4d4`（`1.0`）/ `+0x4d8`（`π`） | `f32` | `get_float_var_ptr` | `0x45f816` / `0x45f820` |
| 10029 | `+0x4dc`（`0x10000`） | `s32` | `get_int_var_ptr` | `0x45f8ec` |
| 10033–10035 | `+0x4c0` `+0x4c4` `+0x4c8` | `f32[3]` | `get_float_var_ptr` | `0x45f7f8`/`0x45f802`/`0x45f80c` |

**跨类型读会转换，不是重解释位** ✅：`get_int_var(10004..10007)` 是
`cvttss2si [ecx+0x4b0..]`（**截零**，`0x45f65b` 起四条）；`get_float_var(10000..10003)` 是
`cvtdq2ps [ecx+0x4a0..]`（`0x45f2ed` 起四条）。两个 rvalue getter 的定义域都是 **10000–10035**
（`cmp eax, 0x23`），比两个 lvalue getter 宽——**能读不能写的变量号是存在的**（例如 10010–10012、10022、10026、10030–10032）。

**变量号的编码取决于指令的类型，不取决于变量本身** ✅ —— 这是写汇编器/播放器最容易踩的一条：

- 整型指令（100/102/104/…/122/201/202/…）用 `push dword [edi+0xc]` 把参数**按 int32** 交给 `get_int_var`；
- 浮点指令（101/103/…/131/203/…）用 `movss xmm1, [edi+0xc]` 把参数**按 float32** 交出去，
  helper 第一件事就是 `cvttss2si`。所以脚本里写 `F0` 时，那 4 个字节是 `10004.0f`（`0x461C4000`），
  **不是** 整数 10004。真实数据 100% 印证：54 个文件里所有整型指令的变量参数都是 int `10000/10001/10008/10022/…`，
  所有浮点指令的变量参数都是 float `10004.0/10005.0/10035.0/…`，0 处例外（[§8](#8-真实数据对账54-个-anm)）。

**④ 结论**（✅ TH16）：`I0–I3` = 10000–10003 @ `+0x4a0`（`s32[4]`），`F0–F3` = 10004–10007 @ `+0x4b0`（`f32[4]`），
与 [`02-vm-core.md`](02-vm-core.md) §3.4 的字段图一致；本文把「哪些号**可写**」和「变量号的编码宽度」补全。

**⑤ 证据**：`0x45f890`/`0x45f780`/`0x45f610`/`0x45f2d0` 四个 helper 的两级表（地址见上表）；
真实数据统计见 §8。

---

## 4. 专项② 除零 / 取模的边界

### 4.1 整数除与取模：`idiv`，无保护

**① 发现**：四条整数除法/取模的核心是同一段：

```
; 108 idiv @0x460324        110 imod @0x4603af        118 div3 @0x45fff4        120 mod3 @0x4600bb
  mov eax, <被除数>
  cdq                       ; 符号扩展进 EDX
  idiv esi                  ; 商 → EAX，余数 → EDX
  mov [ecx], eax            ; 除法取 EAX
  mov [ecx], edx            ; 取模取 EDX
```

**② 推测**：要么前面有 `test esi,esi` 之类的守卫，要么没有。

**③ 验证**：把这四个 case 从入口到 `idiv` 逐条读完 —— **一条比较、一条条件跳转都没有**。
对照组：`122 rand` 在 `0x460472` **有** `test esi, esi; je 0x46048b`，说明 ZUN 在需要的地方是会写守卫的，
这四条是真的没写。

**④ 结论**（✅ TH16）：

- **`b == 0` → x86 `#DE`（Divide Error）→ 进程异常退出**。ANM VM 没有 try/except，也没有回退值。
- **`INT_MIN / -1` 同样 `#DE`**（`idiv` 的商溢出走同一个异常）——这条比除 0 更隐蔽。
- **商截零**（`idiv` 语义），不是 floor：`-7 / 2 == -3`。
- **余数符号随被除数**：`-7 % 2 == -1`，`7 % -2 == 1`。这与 C99 的 `%` 一致，与 Python 的 `%` **不一致**——
  写 JS/Python 播放器必须用 `Math.trunc(a/b)` 和 `a - b*Math.trunc(a/b)`，不能直接用 `%`（JS 的 `%` 恰好一致，Python 的不一致）。

**⑤ 证据**：`0x4602fa`–`0x460331`（108）、`0x460385`–`0x4603bc`（110）、`0x45ffb1`–`0x460003`（118）、
`0x460078`–`0x4600ca`（120）；对照 `0x460472`（122 的零守卫）。

### 4.2 浮点除与 `fmod`：IEEE，不崩

- **109 / 119** 用 `divss`（`0x460369` / `0x46005c`）。SSE 异常在 TH16 里是屏蔽的（helper 入口的
  `stmxcsr` 检查只用于选实现分支），所以 `x/0 = ±inf`、`0/0 = NaN`、`inf/inf = NaN`，**不抛异常** ✅。
- **111 / 121** 调 CRT `fmod`（`0x487aca`，跳到 `0x488bb0` 的 SSE2/x87 分派器；x87 分支 `0x487ad4`
  是教科书式的 `fxch st(1); fprem` 循环）。**参数顺序是一手确认的**：case 先把被除数 `fld` 进去、
  再 `fld` 除数，于是 `ST(1) = a`、`ST(0) = b`，`fprem` 算 `a mod b` ✅。
  `b == 0` → NaN；结果符号随 `a`（与 §4.1 的整数取模一致）。
- **精度**：`fmod` 走的是**双精度**——case 先 `cvtss2sd` 两个 float，算完 `fstp dword` 回 float
  （`0x460110`–`0x460131` / `0x460403`–`0x460424`）。124–128 的五个数学函数同理，全部
  `cvtss2sd → 双精度 libm → cvtsd2ss`。播放器用 JS 的 `Math.*`（双精度）再 `Math.fround` 是**等价**的 ✅。

### 4.3 `122 rand` 的取模是**无符号**的

`0x460480`：`xor edx, edx; div esi`（**`div` 不是 `idiv`**）。被除数是 PRNG 的 32 位 raw
（`0x402be0` 的返回值，当 u32 用），除数 `esi` 是 `arg1` 的值**按 u32 解释**。所以：

- `n > 0`：`x ∈ [0, n)` ✅，与社区说法一致。
- `n == 0`：显式短路成 `x = 0`（`0x46048b`），**不崩** ✅ —— 这是本组唯一一条对零除做了防护的指令。
- `n < 0`：`(u32)n` 是个 ≥ 2³¹ 的大数，余数几乎总是 raw 本身 ⇒ `x` 变成一个大正数，**不是**「负范围随机」。
  真实数据里 `122` 只出现 3 次且 `n > 0`，所以这条是理论边界 ✅（推论，未在游戏里观察到）。

---

## 5. 专项③ 跳转目标是**字节偏移**，不是指令下标

**① 发现**：`200 jmp` 的 handler 只有五条指令：

```
0045fbfc  push dword ptr [edi + 0xc]        ; arg1 = t
0045fbff  lea  ecx, [ebx + 0x540]           ; &vm.script_time
0045fc05  call 0x406490                     ; Timer::set_value
0045fc0a  mov  eax, dword ptr [edi + 8]     ; arg0 = dest
0045fc0d  mov  dword ptr [ebx + 0x28], eax  ; vm.instr_offset = dest   ★ 直接赋值，不乘不查表
0045fc10  jmp  0x45fba0                     ; 回主循环
```

`vm+0x28` 就是 [`02-vm-core.md`](02-vm-core.md) §3.1 的 `instr_offset`，主循环用它算取指地址：
`ins = script_base(vm) + vm.instr_offset`（`0x45fbbb`：`add edi, [ebx+0x28]`）。

**② 推测**：`dest` 要么是字节偏移（直接加），要么是指令下标（那就需要一次遍历或乘法）。

**③ 验证**（两路，互相独立）：

1. **代码侧**：`0x45fc0d` 是**裸赋值**，`0x45fbbb` 是**裸相加**。全条路径上没有任何乘法、
   没有任何「按 size 走 N 步」的循环 ⇒ 只能是字节偏移。12 条条件跳转共用的 `0x460b88`
   （`mov eax,[edi+0x10]; mov [ebx+0x28],eax`）与 `201` 的 `0x45fc5a` 完全同款。
2. **数据侧（对抗证伪）**：写解析器把 54 个真实 `.anm` 里**全部 857 条**跳转（200 × 708、201 × 86、
   202 × 2、204 × 61）的 `dest` 取出来，与该 script 的**指令边界集合**求交：
   **857 / 857 命中，0 例外，且全部 ≡ 0 (mod 4)**。若 `dest` 是指令下标，这些值会集中在 0–200 的小整数区间、
   且几乎不可能条条撞上字节边界（真实脚本的指令边界间距是 12/16/20/24 字节）。

**④ 结论**（✅ TH16）：**`dest` = 相对该 script 起点的字节偏移**，与
[`01-format.md`](01-format.md) §5.1 的 `jmp(offset136, 16)` 观察一致。
`instr_offset` 与 `dest` 都是 **s32**；负值会被 `0x45fa55` 的早退闸门当成「脚本已终止」，
即**跳到负偏移 = 悄悄把这个 VM 冻住**（[`02-vm-core.md`](02-vm-core.md) §2 的 `if (vm.instr_offset < 0) return`）——
真实数据里 0 处出现，属理论边界 🟡。

**⑤ 证据**：`0x45fbfc`–`0x45fc10`、`0x45fbbb`、`0x460b88`、`0x45fa55`；统计脚本跑 `local/th16.v1.00a/dat/*.anm`。

---

## 6. 专项④ 跳转后 `time` 怎么设

**① 发现**：三处跳转成功路径（`0x45fbff`、`0x45fc4c`、`0x460b8b`）都是同一个调用：
`lea ecx, [ebx+0x540]; push <t 参数>; call 0x406490`。`vm+0x540` 是
[`02-vm-core.md`](02-vm-core.md) §3.1 的 `script_time`（时间闸门比的就是它），`0x406490` 是 `Timer::set_value`。

**② 推测**：三种可能——(a) 时间保持不变；(b) 设成目标指令的 `time` 标签；(c) 设成参数里的 `t`。

**③ 验证**：

- **代码**：`Timer::set_value(v)` 的实现（`0x4064bb`–`0x4064cb`）是
  `cur = v`（`+0x04`）、`prev = v - 1`（`+0x00`）、`cur_f = (float)v`（`+0x08`）；
  首次调用还会置 `control |= 1`。所以是 **(c)**：**用参数 `t` 覆盖，与目标指令的 `time` 无关** ✅。
- **数据（证伪 (b)）**：857 条跳转里 `t` 等于目标指令 `time` 标签的只有 **751 条**，
  **85 条不等**（另 21 条目标不可比）。若语义是 (b)，这 85 条就不可能存在 ⇒ (b) 被证伪 ✅。
  另外 350 条是 `t == 0 且目标 time == 0` 的平凡情形，不构成证据。

**④ 结论**（✅ TH16）：

1. **跳转 = 改两个字段**：`instr_offset = dest`、`script_time = Timer::set_value(t)`。没有第三件事。
2. **跳完立刻回主循环**（`jmp 0x45fba0` / `0x45fb9a`），**同一帧内继续执行**——
   接着马上过一次时间闸门 `ins.time <= vm.script_time.current`。所以
   **「跳到一条 `time` 比 `t` 大的指令」= 当帧就让出，等到时间追上来**；
   **「跳到一条 `time <= t` 的指令」= 当帧继续跑**。
3. **`prev = t - 1`**：`Timer::set_value` 把 `previous` 也改了。任何依赖 `previous`（如插值器的
   「这一帧时间跨过了某个刻度」判断）的逻辑，在跳转后看到的是一个**人造的 1 帧步长**，不是真实历史 ✅。
4. **和 interrupt 的差别**：interrupt 命中时用的也是 `Timer::set_value`，但它的入参是
   **标签指令自己的 `time` 字段**（`0x45fb8b`，见 [`02-vm-core.md`](02-vm-core.md) §5），
   而跳转用的是**指令参数 `t`**。两条路都改写时间，来源不同。
5. **无限循环的成因**：`jmp` 回到同一个 `time` 刻度、且 `t` 也设回同一刻度 ⇒ 闸门永远放行 ⇒
   `0x45fba0` 死转（VM 主循环没有迭代上限，[`02-vm-core.md`](02-vm-core.md) §9）。这不是 bug 而是**约定**：
   脚本作者靠 `6 wait` 或递增的 `time` 标签自己让出。

**⑤ 证据**：`0x45fbff`（200）、`0x45fc4c`（201）、`0x460b8b`（202–213）；`Timer::set_value` `0x406490`
（`0x4064bb`–`0x4064cb`）；`0x45fbc2` 时间闸门；`0x45fb8b` interrupt 侧；跳转 `t` 统计见 §8。

---

## 7. 与社区不一致处的裁决

对账对象：thpages 的 `local/vendor/thpages/js/tables/reference/anm.ts`（生成器表 + `byRefId` 描述）
与 `local/vendor/thpages/static/mapfile/v8.anmm`（thtk 风格的 anmmap，带注释）。
**三张生成器表（`OPERATOR_DATA` / `OPERATOR_3_DATA` / `JUMP_DATA`）覆盖的 34 条，逐条比对后全部正确** ✅ ——
`a op= b`（100–111）、`x = a op b`（112–121）、`a ⋈ b` 的 12 条跳转（202–213），
包括浮点比较的 NaN 行为（`jmpNeF` 遇 NaN 跳、其余五条不跳）都对得上。分歧只在下面四处。

### 7.1 `129 mathReduceAngle` 的「最多 33 次」——社区漏了（**我们补**）

thpages：*"Reduce an angle modulo `2*PI` into the range `[-PI, +PI]`."*
一手 `0x402d30`：

```c
x += y;                                   // op129 传 y = 0（xorps xmm1,xmm1 @0x460643）
int k = 0;
while (x > PI)  { if (k++ > 0x20) break; x -= TWO_PI; }   // 0x402d50..0x402d5f
while (x < -PI) { if (k++ > 0x20) break; x += TWO_PI; }   // 0x402d70..0x402d7f
```

**计数器 `k` 在两个循环之间不重置** ⇒ 两段合计最多 33 次 ±2π ⇒ **可归约的输入区间约 ±33·2π ≈ ±207.3**。
超出这个范围的角度会**原样漏出去，不报错**。播放器若用 `x - 2π·round(x/2π)` 实现，
在 `|x| > 207` 时会与游戏不一致 ✅（TH16）。裁决：**社区没错，只是没写全**；本文补上。
证据：`0x402d30`–`0x402d81`；常量 `0x494588`（float π）、`0x4945b8`（float 2π）、`0x494734`（float −π）。

### 7.2 `131 mathCirclePosRand` 的半径区间 —— **社区错**

thpages：*"Uniformly draws a random radius `r` in the interval given by `rmin` and `rmax`"*；
`v8.anmm` 的注释更具体：`float r = randf() * (max - min) + min; // randf = (0.0f, 1.0f)`。

一手 `0x4606f5`–`0x4607c9`，两次随机都调 **`0x402cb0`**（不是 `0x402c70`）：

```c
float s1 = prng_randf_signed();        // 0x460738 → 0x402cb0
float ang = s1 * PI;                   // 0x46073d  fmul [0x494588]
float s2 = prng_randf_signed();        // 0x46074c → 0x402cb0
float r   = lo + s2 * (hi - lo);       // 0x46075e subss / 0x460771 mulss / 0x460775 addss
cartesian_from_polar_464d60(&out, ang, r);   // 0x46078c
*argptr0 = out[0];  *argptr1 = out[1];       // 0x4607ab / 0x4607c9 → 尾巴 0x45fd50
```

`0x402cb0` 的值域是 **`[-1, 1)`**（本仓 [`../../_shared/math-and-prng.md`](../../_shared/math-and-prng.md) §3.6
已一手解出公式 `u·2⁻³¹ − 1.0`；本文复核了 case 确实调它而不是 `0x402c70`）。代入：

| 量 | 社区说法 | 一手 | 差别 |
| --- | --- | --- | --- |
| 角 | 「随机角」 | `randf_signed() · π ∈ [-π, π)` | 一致 |
| 半径 | `r ∈ [lo, hi]` | `r = lo + s·(hi - lo)`，`s ∈ [-1, 1)` ⇒ **`r ∈ (2·lo - hi, hi)`** | **区间宽一倍，且向下越过 `lo`** |

举例 `lo = 10, hi = 20`：社区说 `r ∈ [10,20]`，实际 `r ∈ (0, 20)`——一半的抽样会落在 `lo` 以内，
`lo = 0` 时甚至会出现**负半径**（等价于角度翻 π）。

**裁决：社区错，我们对** ✅。thpages 那句 *"This isn't the right way to uniformly draw points on a ring"*
其实还低估了偏差——问题不只是「面积不均匀」，是**区间本身就不是 `[lo, hi]`**。
真实数据里 `131` 出现 7 次（`0x460492` 的兄弟 `123 randF` 则用 `0x402c70`，值域确实是 `[0,1)`，
所以这不是「ZUN 的随机包装只有一个」——是这条指令**选了对称版**）。

### 7.3 两处措辞级偏差（社区不算错，但会误导实现者）

| 出处 | 社区写法 | 一手 | 影响 |
| --- | --- | --- | --- |
| `v8.anmm` 的 `130` 注释 | `*y = sin(ang*dist)` | `*y = sin(ang) * dist`（`fsincos` 后各 `fmul dist`） | 明显是笔误；`0x464930` 只有一次 `fsincos`，两次 `fmul` |
| thpages `randF`（123） | `0 <= x <= r` | `x = randf01() · r`，`randf01 ∈ [0,1)` ⇒ `x ∈ [0, r)` | 开闭区间；对 `r` 取到端点的判断有影响 |

另外 thpages 的 `OPERATOR_3_DATA` 生成的说明文字是 ``Does `a = b op c` ``，而它自己声明的参数名是
`['x','a','b']`——**渲染出来的文档里符号对不上**（应为 `x = a op b`）。这是生成器模板的笔误，
不影响语义（签名 `$SS` / `%ff` 是对的）。

### 7.4 本仓内部更正：PRNG 流别不是「全部走 Stream A」

[`../../_shared/math-and-prng.md`](../../_shared/math-and-prng.md) §3.6 写：
*「调用约定:全部 `f(&DAT_004a6d88)` 单参(Stream A)」*。

一手证伪：ANM 侧的四个调用点全部传 **Stream B `0x4a6d80`**：
`0x460476`（122）、`0x4604ab`（123）、`0x46072d` 与 `0x460743`（131），
另加 `AnmVm::get_float_var` 的 `0x45f3a3` / `0x45f3c9` / `0x45f3ef` 与 `AnmVm::get_int_var` 的 `0x45f4d8`。

把 `.text` 里所有 `mov ecx, imm32` 形式的两个常量扫一遍并按所属函数归类，分工非常干净：

| 流 | 地址 | 主要调用方 | 性质 |
| --- | --- | --- | --- |
| Stream B | `0x4a6d80` | `AnmVm::run`、`AnmVm::get_*_var`、`ScreenEffect::on_tick_13__screen_shake`、`StageInner::step_fog`、`ItemManager` 的表现层 | **表现层**，不进回放 |
| Stream A | `0x4a6d88` | `EclRunContext::ecl_run`、`bullet_pool_spawn`、`PlayerBullet::create`、`EnemyDrop::eject_extra_drops`、`gen_items_from_cancel` | **gameplay**，回放关键 |

**裁决：`math-and-prng.md` 那句「全部」应改为「gameplay 侧全部」** ✅。
这条对播放器有实际意义：**ANM 脚本里的随机不影响回放**，所以 JS 播放器用任意 PRNG 都不会「跑错」，
只会「长得不一样」；反过来 ECL 侧的随机必须 bit-exact。
同时也印证 thpages 把 122/123/131 归给 *"animation RNG"* 是对的 ✅。

---

## 8. 真实数据对账（54 个 `.anm`）

自写解析器（按 [`01-format.md`](01-format.md) §3–§5 的布局）跑 `local/th16.v1.00a/dat/*.anm`，
54 文件 / 1929 script / 43268 指令，其中本组 **2525 条**：

| 检查项 | 结果 |
| --- | --- |
| 每条的 `size` 是否 = `8 + 4 × 参数个数`（按 §2 表推） | **2525 / 2525 吻合**，无一例外 |
| 857 条跳转的 `dest` 是否落在指令边界 | **857 / 857**，且全部 ≡ 0 (mod 4) |
| 跳转的 `t` 是否恒等于目标指令的 `time` | **否**：751 相等 / 85 不等 ⇒ 证伪「t 取自目标标签」 |
| `200 jmp` 的 `param_mask` | **708 / 708 全为 0** ⇒ 与「handler 不查 mask」自洽 |
| `201 jmpDec` 的 `param_mask` | **86 / 86 恒为 1** ⇒ 计数器永远是变量，§9 的自修改坑没被触发 |
| 整型指令的变量参数编码 | 恒为 int32（`10000/10001/10002/10003/10008/10022`） |
| 浮点指令的变量参数编码 | 恒为 float32（`10004.0`…`10035.0`），0 处混用 |
| 左值参数（`arg0`，130/131 还有 `arg1`）是否总被 `param_mask` 标成变量 | **是**，0 处例外 |

**本组在 TH16 自家资源里的使用分布**（24 条用到 / 22 条实现了但一次没用）：

按出现次数排（合计 2525 条）：
`200`×708 · `117`×539 · `100`×302 · `103`×235 · `120`×168 · `101`×130 · `119`×88 ·
`201`×86 · `105`×69 · `204`×61 · `107`×44 · `124`×21 · `125`×21 · `102`×9 · `112`×9 ·
`115`×8 · `131`×7 · `130`×6 · `113`×4 · `104`×3 · `122`×3 · `202`×2 · `118`×1 · `121`×1。

**一次都没出现的 22 条**：`106` `108` `109` `110` `111` `114` `116` `123` `126` `127` `128` `129`
`203` `205` `206` `207` `208` `209` `210` `211` `212` `213`。
它们**实现是完整的**（每条都有独立 handler），只是 ZUN 自家 anm 没用到——
mod 作者可以放心用，但**没有 ground truth 可对**，播放器实现时要靠本文的语义而不是「跑一遍看看」。

---

## 9. 写播放器会踩的坑

| 坑 | 说明 |
| --- | --- |
| **非变量左值 = 自修改字节码** | 若 `param_mask` 的 `bit0` 没置位，`get_*_var_ptr` 把**指令流里那 4 个字节的地址**原样退回（`0x45f8f6` / `0x45f82a`），写入会改掉脚本本身，而脚本缓冲是**该 anm 全体 VM 共享**的。ZUN 的数据从不这样（§8），但畸形/手写 anm 会 |
| **`201 jmpDec` 的减法先于取值** | `dec dword [eax]`（`0x45fc27`）在读值之前；判据是**减完之后**的值 `> 0`。`x = 1` 时减成 0 ⇒ 不跳；`x = 0` 时减成 −1 ⇒ 不跳 |
| **整数除 0 会崩，不是返回 0** | §4.1。播放器要么复刻崩（不现实），要么显式选一个约定并标注偏离 |
| **取模符号随被除数** | §4.1。Python 的 `%` 会给出不同结果 |
| **`121 modF3` 的求值顺序是反的** | 它先算 `arg2`（`0x4600d8`）再算 `arg1`（`0x4600f1`），其余 3 操作数指令都是先 `arg1` 后 `arg2`。当参数是**有副作用的随机变量**（10022/10026/10030–10032）时可观察 |
| **变量号编码宽度跟指令类型走** | §3。整型指令写 int `10004` 与浮点指令写 float `10004.0f` 是完全不同的 4 个字节 |
| **`I0–I3` / `F0–F3` 之外还有 8 个可写号** | 10008/10009/10013–10015/10023–10025/10027/10028/10029/10033–10035。写 `10013` 就是直接改 `pos.x`——**绕过了 `400 move` 的插值器**，播放器不能只把变量当"寄存器" |
| **跳转 `dest`/`t` 不能是变量** | 没有计算跳转（computed goto）。反汇编器不必为这两个位置生成 `$`/`%` |
| **跳转会改 `previous`** | `Timer::set_value` 令 `previous = t - 1`（§6）。依赖 `previous` 的逻辑在跳转后拿到的是人造值 |
| **`jmp` 之后没有让出点** | 主循环无迭代上限；自跳脚本会真死循环（[`02-vm-core.md`](02-vm-core.md) §9） |
| **`movss xmm1, [0x494720]` 是噪声** | 几乎每条 handler 收尾前都有这条（`0x494720` = `-1.0f`）。它是编译器把一个循环不变量搬回 `xmm1`，**与本组指令的语义无关**，别当成隐藏运算 |

---

## 10. 还没搞清楚的

| 问题 | 状态 |
| --- | :---: |
| 五个 CRT 数学 helper 的身份靠**数值指纹**（Taylor 系数 / 归约常量 / ±inf 早退）钉，不是符号表；虽然三条独立线索（指纹、sin↔cos 的 `0x10` 偏置、社区两源）互相印证，仍非「跑一遍验证」 | ✅（指纹可复核）/ 无运行时验证 |
| `10010–10012` `10016–10021` `10026` `10030–10032` 这些**只读**变量号的具体含义（本文只确认了「可读不可写」） | — 归变量表文档 |
| `129` 的第二参数 `y`：`math_wrap_angle_402d30(x, y)` 是 `wrap(x + y)`，op129 恒传 0；另一个调用点（xref 共 2 处）传什么没查 | ❓ |
| 跳到负 `instr_offset` 的实际后果（推得是「静默冻结」，真实数据 0 处出现，未构造样本验证） | 🟡 |
| `122` 在 `n < 0` 时的行为（推理明确，未在游戏中观察到） | 🟡 |
| 本组指令在 TH18 的差异（本文只做 TH16，不外推） | — 阶段 3 |

---

## 关联

- 指令头 / `param_mask` / script 表：[`01-format.md`](01-format.md)
- VM 主循环、时间闸门、`zAnmVm` 字段图、终止语义：[`02-vm-core.md`](02-vm-core.md)
- PRNG 算法与两条流：[`../../_shared/math-and-prng.md`](../../_shared/math-and-prng.md)（本文更正其 §3.6 一句，见 §7.4）
- 子系统总览：[`../OVERVIEW.md`](../OVERVIEW.md)
- 本作入口：[`README.md`](README.md)
- 想要的 Ghidra 重命名（待 T10 统一落库）：[`naming-batch.md`](naming-batch.md)
- 纪律：[`../../../METHOD.md`](../../../METHOD.md)、[`../../../DOCSTYLE.md`](../../../DOCSTYLE.md)
