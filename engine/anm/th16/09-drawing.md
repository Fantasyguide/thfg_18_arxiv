# ANM v8 Drawing 组 + render state — TH16 一手逐条确认（opcode 600–613，14 条）

> **版本**：TH16 v1.00a（`th16.exe`，imagebase `0x400000`）。本文裸地址默认属该版本；引用其他版本须写成 `th18:0x…`。
> **方法**：**不经 Ghidra 渲染**——直接解 PE 节表把 `.text` 字节喂 capstone，逐条反汇编 14 个 case 体、
> 绘制分发 `draw_vm` `0x468490`、几何填充 `0x4632f0`、render state 设置器 `0x464f10` 与各 mode 绘制函数；
> 高段分发表（`0x462fa8` 索引 + `0x462e64` 跳表）逐字节解出自证「opcode → case 地址」；
> 再用自写解析器跑 `local/th16.v1.00a/dat/` 的 **54 个真实 `.anm`（41339 指令，其中本组 90 条）**做对账与证伪。
> **可信度**：14 条的**签名（参数个数/类型/落点）与渲染模式全部 ✅**（每条指得到一条读/写 `flags_lo` 或 vm 字段的机器指令）；
> render state 扫描的 blend / sampler / texture-stage / FVF 全部 ✅；609/610 的**精确 3D 几何公式**与
> 601/602 的**弧角展开公式**未逐条追完，标 🟡（社区 thpages 描述正确，本文只证到「字段读在哪」这一层）。
> **衔接**：指令头、`param_mask` 见 [`01-format.md`](01-format.md) §5；`zAnmVm` 字段图见 [`02-vm-core.md`](02-vm-core.md) §3；
> 变量号（`I0`=10000、`F0–F3`=10004–10007）见 [`04-variables.md`](04-variables.md) §2；渲染模式字段位见
> [`02-vm-core.md`](02-vm-core.md) §3.6（bit 25–29）。本文不重复这些论证。
> **不含版权字节**：只写偏移、地址与语义。

## 0. 一句话结论

Drawing 组 14 条**全部是「设置渲染模式 + 写 shape 尺寸/顶点缓冲」的置状态指令**，自身不发 draw call：
每条 opcode 只做两件事——把 `flags_lo` bit 25–29（渲染模式）写成一个特定值，再写一个 vm 字段
（`sprite_size` / `I0` / 顶点缓冲）。真正的绘制分两步，都在 **frame-tail 之后**：

1. **几何填充**：`0x4632f0`（`AnmVm::sub_4632f0`，02 §2 帧尾的 `update_geometry_for_render_mode`）按渲染模式
   把顶点写进 `vm+0x5b8` 指向的缓冲（读 `I0`/`I1`/`F0–F3`/`scale`/`rotation`）。
2. **绘制**：`draw_vm` `0x468490` 按渲染模式分发到各 mode 函数，后者设 render state + `SetFVF` + `DrawPrimitive(UP)`。

**签名三族**（`$`=int32、`f`=float32，与 thpages 逐条一致）：

- **`S nmax`**（1 个 int）：`600`/`601`/`602`（×56 分配缓冲）、`609`/`610`（×48 分配缓冲）。
- **`ff w, h`**（2 个 float）：`603`/`606`/`607`/`608`/`612`/`613`，写 `sprite_size`（`+0x70`/`+0x74`）。
- **`fS radius, nInit`**：`604`/`605`；**`ffS radius, thickness, nInit`**：`611`——写 `sprite_size.x` 与 `I0`（`+0x4a0`）。

**render state 扫描结论**（对应 spec §10.3 的三行 🟡，全部降掉，见 [§4](#4-★-render-state-一手扫描)）：

- **blend modes**：`flags_lo` bit 5–8 是 10 种混合模式（`SRCBLEND`/`DESTBLEND`/`BLENDOP` + `ALPHATESTENABLE` 开关），✅。
- **alpha test**：全局重置 `Supervisor__reset_render_state` `0x45ba80` 设 `ALPHAREF=1`、`ALPHAFUNC=GREATEREQUAL(7)`、
  `ALPHATESTENABLE=1`；ANM 只按 blend mode 开/关 `ALPHATESTENABLE`。✅（「GREATEREQUAL」之名 🟡，值 7 是一手）。
- **texture stage states**：`COLOROP/ALPHAOP=MODULATE`，`COLORARG2/ALPHAARG2=DIFFUSE`（sprite/textureCircle）或
  `=TFACTOR`（textureArc3D）；`COLORARG1/ALPHAARG1` 从不设（用 D3D9 默认 `D3DTA_TEXTURE`）。✅。

---

## 1. 怎么核的：opcode → case 地址是自己解跳表得到的

**① 发现**：`th-re-data` 在 `AnmVm::run` `0x45f980` 函数体内给本组 14 条 case label（社区单源），
形如 `0x46246c 600__kaleidoscope`、`0x462608 603__drawRectSharp`；其中 601/602/609/610/611/612/613 七条只有裸数字。

**② 推测**：这些 label 的「数字前缀」就是它服务的 opcode。**待验假设**。

**③ 验证**：不看 label，直接解高段分发表（02 §7 的参数）：`SUB EAX,0x12d` 后 `CMP EAX,0x138`，
索引表 `0x462fa8`（313 字节）+ 跳表 `0x462e64`（81 dword）。逐字读出 opcode 600–613 的落点：

| opcode | case 地址 | opcode | case 地址 |
| :---: | --- | :---: | --- |
| 600 | `0x46246c` | 607 | `0x462697` |
| 601 | `0x4624c1` | 608 | `0x4626ac` |
| 602 | `0x4624d3` | 609 | `0x4624e5` |
| 603 | `0x462608` | 610 | `0x462552` |
| 604 | `0x4626eb` | 611 | `0x462778` |
| 605 | `0x462766` | 612 | `0x4626d6` |
| 606 | `0x462685` | 613 | `0x4626c1` |

**与社区 label 逐条吻合，0 处错位** ✅。**④ 结论**：本组 14 个 case 地址全是一手的。**⑤ 证据**：表 `0x462fa8`/`0x462e64`。

### 1.1 每条 case 的公共骨架

本组每条 handler 只做「改 `flags_lo` 渲染模式字段 + 写一个 vm 字段」，然后 `instr_offset += size` 回主循环：

```c
// EBX = zAnmVm*, EDI = 当前指令指针
flags_lo = vm[+0x530];
flags_lo = (flags_lo & ~<mode 里应为 0 的位>) | <mode 里应为 1 的位>;   // 净效果 = 只改写 bit 25–29
vm[+0x530] = flags_lo;
...按 param_mask 解析 arg，写 sprite_size / I0 / 调 0x4064e0 分配缓冲...
vm.instr_offset += ins.size;          // movzx eax,[edi+2] ; add [ebx+0x28],eax
goto 0x45fb9a;
```

**判据（可机器复核）**：case 体开头恒是 `mov eax,[ebx+0x530]` + `and eax,<mask>` + `or eax,<val>` + `mov [ebx+0x530],eax`。
每个 `and` 清掉的位与该 mode 的 5 位二进制里「应为 0」的位**精确互补**、`or` 置位的位与「应为 1」的位**精确一致**，
两者并集恒为 bit 25–29 全 5 位（`0x3E000000`）。所以**净效果是「只改渲染模式字段，其余 `flags_lo` 位一个不动」** ✅——
本组 14 条**不碰** blend mode（bit 5–8）、sampler 地址模式（bit 30–31）等其他 flag。

---

## 2. ★ 14 条指令表

**记号**：`$` = int32 参数（`get_int_var` 侧）· `f` = float32 参数（`get_float_var` 侧）· `S` = thpages 的整型助记。
「渲染模式」= 写入 `flags_lo` bit 25–29 的值。`+0x70`/`+0x74` = `sprite_size.x/y`（02 §3.3），
`+0x4a0` = `I0`（变量号 10000，04 §2），`+0x5b8`/`+0x5bc` = 顶点缓冲指针/大小（02 §3.5）。

| opcode | 助记（thpages） | 签名 | 语义 | 渲染模式 | case 地址 | 可信度 |
| :---: | --- | --- | --- | :---: | --- | :---: |
| 600 | `textureCircle` | `$ nmax` | 分配 `56*nmax` 字节顶点缓冲（`0x4064e0`） | 9 | `0x46246c` | ✅ |
| 601 | `textureArcEven` | `$ nmax` | 同上（弧角「居中于 0」） | 13 | `0x4624c1` | ✅ |
| 602 | `textureArc` | `$ nmax` | 同上（弧角「从 0 起」） | 14 | `0x4624d3` | ✅ |
| 603 | `drawRect` | `f w, f h` | `sprite_size.x=w; sprite_size.y=h` | 16 | `0x462608` | ✅ |
| 604 | `drawPoly` | `f r, $ n` | `sprite_size.x=r; I0=n` | 17 | `0x4626eb` | ✅ |
| 605 | `drawPolyBorder` | `f r, $ n` | 同上 | 18 | `0x462766` | ✅ |
| 606 | `drawRectGrad` | `f w, f h` | `sprite_size.x=w; sprite_size.y=h` | 20 | `0x462685` | ✅ |
| 607 | `drawRectShadow` | `f w, f h` | 同上 | 21 | `0x462697` | ✅ |
| 608 | `drawRectShadowGrad` | `f w, f h` | 同上 | 22 | `0x4626ac` | ✅ |
| 609 | `textureCylinder3D` | `$ nmax` | 分配 `48*nmax` 字节顶点缓冲 | 24 | `0x4624e5` | ✅ |
| 610 | `textureRing3D` | `$ nmax` | 同上 | 25 | `0x462552` | ✅ |
| 611 | `drawRing` | `f r, f t, $ n` | `sprite_size.x=r; sprite_size.y=t; I0=n` | 19 | `0x462778` | ✅ |
| 612 | `drawRectBorder` | `f w, f h` | `sprite_size.x=w; sprite_size.y=h` | 27 | `0x4626d6` | ✅ |
| 613 | `drawLine` | `f len, f _` | `sprite_size.x=len; sprite_size.y=_`（渲染忽略 y） | 26 | `0x4626c1` | ✅ |

> **可信度挂在断言上**：上表每行「签名 + 渲染模式 + 落点字段」都在 `th16.exe` 上一手复核 ✅。但
> 601/607/609/610/611/613 这 6 条 **ZUN 自家 `.anm` 一次都没用过**（§6）——它们的「opcode 语义」是 ✅，
> 「渲染出来的画面」没有 ground truth 可对，见 §8。

### 2.1 三个共用落点（穷举）

本组 case 体大量互相跳转复用尾巴。判据：case 体末尾 `jmp` 到一个落在别的 case 体内部的地址。
「谁用」用「扫全部 14 个 case 的最后一条 `jmp` 目标」穷举得到：

| 落点 | 地址 | 干什么 | 谁用（穷举） |
| --- | --- | --- | --- |
| 600 的 `×56` 分配尾巴 | `0x46247c` | 解析 `arg0`（int，`get_int_var`）→ `×56` → `call 0x4064e0` | 601、602 |
| 609 的 `×48` 分配尾巴 | `0x4624f5` | 同上，`×48` → `call 0x4064e0` | 610 |
| 603 的「写 `sprite_size`」尾巴 | `0x462618` | `arg0(f)→+0x70`、`arg1(f)→+0x74` | 606、607、608、612、613 |
| 604 的「写 `sprite_size.x` + `I0`」尾巴 | `0x4626fb` | `arg0(f)→+0x70`、`arg1(int)→+0x4a0` | 605 |

`611`（`drawRing`）三条参数全自写，不复用别的尾巴；`600`/`609` 也各自独立写 `flags_lo` 后再接尾巴。

**求值次序**（承 [`03-math-jumps.md`](03-math-jumps.md) §9.1 的判据）：本组每条都是「先 `arg0` 后 `arg1`（后 `arg2`）」的
**下标升序**，且**左值解析（写字段）恒在右值读取之后**。`600`/`609` 的 `arg0` 只读一次（做 `×56`/`×48`），
没有「先右值后左值」的拆法——因为本组没有「读变量可能推进 PRNG」的随机参数，次序差异不可观察 🟡（推论）。

### 2.2 `0x4064e0` = 「按字节数分配顶点缓冲」

600/601/602/609/610 把 `arg0` 乘以 56/48 后调 `0x4064e0`（`__thiscall ECX=vm`，栈参 1 个）：

```c
// 0x4064e0: vm[+0x5bc] = size;  vm[+0x5b8] = operator_new(size);  // 0x477fa7 = malloc
```

所以 `+0x5b8` = 顶点缓冲指针、`+0x5bc` = 字节大小（与 02 §3.5 的「顶点缓冲」字段一致，本文补上大小字段）。
**56 = 28×2、48 = 24×2**——textureCircle 的顶点 stride 是 28 字节（见 §4.4），textureArc3D 是 20 字节
（§4.4），`nmax` 是「点数上限」，每条点的顶点数不同，故倍数不同。**缓冲只是分配，顶点由 `0x4632f0` 在帧尾填**（§3）。

---

## 3. 专项① 渲染模式 → 绘制函数的映射

`draw_vm` `0x468490` 是总分发器（`render_layer` `0x46e750` 逐 VM 调它）。它对渲染模式做两级分发，
**两张表都是直读 PE 解出的**（不经 label 猜）：

```
render_mode = (flags_lo >> 0x19) & 0x1f;           // 0x4684e7..0x4684ea
if (mode > 0x1b) goto default;                     // 0x4684ed..0x4684f0  (0x1b = 27)
jmp [byte[mode + 0x468bac] * 4 + 0x468b70];        // 索引表 28 字节 + 跳表
```

**mode → handler 全表**（索引表 `0x468bac` / 跳表 `0x468b70` 逐字解出，含非本组的 mode 供对照）：

| 模式 | handler | 谁设的 | 模式 | handler | 谁设的 |
| :---: | --- | --- | :---: | --- | --- |
| 0/1/3 | `0x468504`/`0x468542`（sprite 角点） | `300 sprite` / 默认 | 15 | `0x468647`（存变换 + mode 8） | `302` |
| 2 | `0x4686e0`（sprite，−0.5px 偏移） | `302` | 16/20/21/22/26/27 | `0x468722`（rect 族） | **603/606/607/608/613/612** |
| 4 | `0x468584`（mode 4） | `302` | 17/18/19 | `0x4689b3`（poly 族） | **604/605/611** |
| 5/6/7/8 | `0x4685af`/`0x4685d5`/`0x4685fb`/`0x468621` | `302` | 24/25 | `0x4686c1`（textureArc3D） | **609/610** |
| 9/12/13/14 | `0x468683`（textureCircle） | **600**/`302`/**601/602** | 10/23/28+ | `0x468b63`（default） | — |
| 11 | `0x4686a2`（mode 11） | `302` | | | |

三个要点：

1. **600/601/602 在绘制层完全同函数**（`0x4681f0` textureCircle）——它们的差别在**帧尾几何** `0x4632f0`
   （§3.1），不在 `draw_vm`。同理 609/610 共用 `0x467d00`。
2. **rect 族（16/20/21/22/26/27）共用 `0x468722`**，它再按 `mode-16` 走第二张跳表 `0x468bc8`（12 项）分到
   6 个 rect handler（§4.4）；poly 族（17/18/19）走 `0x4689b3`，按 `mode-17` 分到 3 个 poly handler。
3. **`+0x4a0`（`I0`）是绘制时的点数/边数**：`0x468683`/`0x4686c1` 都 `mov eax,[esi+0x4a0]; add eax,eax` 把 `2*I0`
   当顶点数传进绘制函数——所以「`I0` = 点/边数」是**绘制层一手**的，不是抄 thpages ✅。

### 3.1 帧尾几何填充 `0x4632f0`（`AnmVm::sub_4632f0`）

它每帧在帧尾被调（02 §2 的 `update_geometry_for_render_mode`），**按 `render_mode - 9`（mode 9–25）分发**
（`0x46330d`–`0x46332c`，索引 `0x463b14` / 跳表 `0x463b04`），把顶点写进 `vm+0x5b8` 缓冲。字段读落点（直读反汇编）：

| 模式族 | 读的字段（vm 偏移） | 对应变量 | 含义（thpages 描述，🟡 未逐条追几何） |
| --- | --- | --- | --- |
| 9/13/14（textureCircle/Arc） | `+0x4a0` `+0x4a4` `+0x40` `+0x50` `+0x54` `+0x520`/`+0x524` `+0x5b8` | `I0` `I1` `rotation.z` `scale.xy` 色 | 点数 / 纹理重复次数 / 弧角 / 环宽与半径 / 渐变色 |
| 24/25（textureCylinder/Ring3D） | `+0x4a0` `+0x4a4` `+0x4b0` `+0x4b4` `+0x4b8` `+0x4bc` `+0x5b8` | `I0` `I1` `F0` `F1` `F2` `F3` | 点数 / 重复次数 / 总角 / 高(宽) / 半径 / 中心角 |

> **这一节是对 thpages 的一手背书**：thpages 说 609/610 用 `F0–F3` + `I0`/`I1`，601/602 用 `rotation.x` 定弧角——
> 本文在 `0x4632f0` 里逐条找到了这些字段读（`0x4638da`=`+0x4b0` F0、`0x4638ef`=`+0x4bc` F3、
> `0x463951`=`+0x4a4` I1、`0x463997`=`+0x4b4` F1、`0x4639b0`=`+0x4b8` F2、`0x46336c`=`+0x4a4` I1…）。
> 字段映射 ✅；**具体到每条几何公式**（弧角怎么折、`F0` 是 `2π` 时怎样闭合）本文没追完，🟡。

---

## 4. ★ render state 一手扫描

回答 spec §10.3 的三行 🟡：「ANM 到底设了哪些状态」。按调用点分三处：全局重置、per-VM 状态、mode 函数。

**识别 vtable 偏移**（IDirect3DDevice9，d3d9.h 顺序）：`[reg+0xe4]`=57=**SetRenderState**、
`[reg+0x10c]`=67=**SetTextureStageState**、`[reg+0x114]`=69=**SetSamplerState**、
`[reg+0x104]`=65=SetTexture、`[reg+0xb0]`=44=SetTransform、`[reg+0x164]`=89=SetFVF、
`[reg+0x190]`=100=SetStreamSource、`[reg+0x144]`=81=DrawPrimitive、`[reg+0x14c]`=83=DrawPrimitiveUP。
调用形如 `push <value>; push <state>; push <device>; mov ecx,[eax]; call [ecx+0xe4]`（`this` 走栈）。

### 4.1 全局重置：`Supervisor__reset_render_state` `0x45ba80`

设备初始化时设一次默认状态（逐条反汇编，✅；括号内是 D3D9 SDK 的枚举名，🟡）：

| state | value | 含义 |
| --- | :---: | --- |
| `0x1b`=ALPHABLENDENABLE | 1 | **全局开混合**（所以 ANM 各 blend mode 才生效） |
| `0xf`=ALPHATESTENABLE | 1 | 开 alpha test |
| `0x18`=ALPHAREF | 1 | 参考值 `1/255 ≈ 0.004` |
| `0x19`=ALPHAFUNC | 7 | `GREATEREQUAL`（`alpha ≥ 1/255` 才通过） |
| `0x13`=SRCBLEND | 5 | `SRCALPHA` |
| `0x14`=DESTBLEND | 6 | `INVSRCALPHA` |
| `0xab`=BLENDOP | 1 | `ADD` |
| `0x17`=ZFUNC | 8 | `ALWAYS`（深度测试恒过） |

**alpha test 的落地**：`ALPHAREF=1`、`ALPHAFUNC=GREATEREQUAL` ⇒ **丢弃 `alpha < 1/255` 的像素**（约等于「丢弃全透明像素」，
不是 no-op）。WebGL 对应：fragment shader 里 `if (a < 1.0/255.0) discard;`。⚠️ 这只在 `ALPHATESTENABLE=1` 时生效，
而 ANM 的 blend mode 3（Replace）会把它关掉（§4.2）。**枚举名是 SDK 事实 🟡；数值（1、7）是一手 ✅。**

### 4.2 per-VM：`setup_render_state_for_vm` `0x464f10`（th-re-data 已命名）

它读 VM 的四个 flag 字段，各自带「与上次不同的才 flush + 重设」的缓存（`mgr+0x184fbb4/bba/bbc/bbd`）。

**(a) blend mode** = `flags_lo` bit 5–8（`(flags_lo >> 5) & 0xf`，缓存 `+0x184fbb4`），10 项跳表 `0x465254`
（`>9` 走 default 不设）。逐项解出（✅；`SRCALPHA` 等名来自 D3D9 SDK，🟡）：

| mode | ALPHATEST | SRCBLEND | DESTBLEND | BLENDOP | 常见解读 |
| :---: | :---: | :---: | :---: | :---: | --- |
| 0 | 1 | 5 SRCALPHA | 6 INVSRCALPHA | 1 ADD | Normal 透明 |
| 1 | 1 | 5 SRCALPHA | 2 ONE | 1 ADD | Add 加色 |
| 2 | 1 | 5 SRCALPHA | 2 ONE | 3 REVSUBTRACT | Subtract 减色 |
| 3 | **0** | 2 ONE | 1 ZERO | 1 ADD | Replace 不透明 |
| 4 | 1 | 10 INVDESTCOLOR | 4 INVSRCCOLOR | 1 ADD | 反向色混合 |
| 5 | 1 | 9 DESTCOLOR | 1 ZERO | 1 ADD | Multiply 正片叠底 |
| 6 | 1 | 4 INVSRCCOLOR | 6 INVSRCALPHA | 1 ADD | — |
| 7 | 1 | 7 DESTALPHA | 8 INVDESTALPHA | 1 ADD | 前置（画在 dst 后） |
| 8 | 1 | 5 SRCALPHA | 2 ONE | 4 MIN | 只取暗部 |
| 9 | 1 | 5 SRCALPHA | 2 ONE | 5 MAX | 只取亮部 |

**与 thpages 的 blendMode 表逐格一致** ✅（thpages 自己标 `wip:1`「只看了汇编」，本文在 th16 上复核成 ✅）。
这张表是 `303 blendMode`（R5）写 bit 5–8 后**被谁读**的答案；Drawing 组本身不碰它。

**(b) 纹理过滤** = `flags_hi` bit 11（缓存 `+0x184fbba`）：`SetSamplerState(0, MAGFILTER=5, v)` +
`SetSamplerState(0, MINFILTER=6, v)`，`v = 2(LINEAR)` 当 bit11=0，`v = 1(POINT)` 当 bit11=1。
→ **bit 11 = 0 线性 / 1 最近邻**（即 `311 resampleMode` 的落点）。

**(c) U 寻址** = `flags_hi` bit 0–1（缓存 `+0x184fbbc`）：`SetSamplerState(0, ADDRESSU=1, v)`，
`v ∈ {1=WRAP, 3=CLAMP, 2=MIRROR}` 对应 `(flags_hi&3) ∈ {0,1,2}`。

**(d) V 寻址** = `flags_lo` bit 30–31（缓存 `+0x184fbbd`）：`SetSamplerState(0, ADDRESSV=2, v)`，映射同上。
→ **(c)(d) 是 `312 scrollMode` 的落点**（U 在 `flags_hi`、V 在 `flags_lo`，两个字段是拆开的）。

> **一个易踩的错**：`0x114` 是 **SetSamplerState（vtable 69）不是 SetTextureStageState（67=`0x10c`）**。
> 把 (b)(c)(d) 当成 texture-stage state 去写 shader 会多造出 color/alpha op——它们只是**采样器过滤与寻址**。

### 4.3 mode 函数里的 texture-stage state（`[reg+0x10c]`）

真正设 texture-stage state 的是 **flush_sprites `0x465a80`、textureCircle `0x4681f0`、textureArc3D `0x467d00`**：

| 来源 | COLOROP(1) | ALPHAOP(4) | COLORARG2(3) | ALPHAARG2(6) | 组合语义 |
| --- | :---: | :---: | :---: | :---: | --- |
| flush_sprites / textureCircle | 4 MODULATE | 4 MODULATE | 0 DIFFUSE | 0 DIFFUSE | `纹理 × 顶点漫反射色` |
| textureArc3D | 4 MODULATE | 4 MODULATE | 3 TFACTOR | 3 TFACTOR | `纹理 × TEXTUREFACTOR` |

`COLORARG1`/`ALPHAARG1` **从不设**（用 D3D9 默认 `D3DTA_TEXTURE`）✅。textureArc3D 还
`SetRenderState(TEXTUREFACTOR=0x3c, 该 VM 算出的色)`（`0x467a48`）——这是它调色/淡出的手段。
**结论**：固定管线是「stage0 把纹理和 diffuse/tfactor 相乘」，WebGL 一条 `tex * color` 就复刻了整条 texture-stage ✅。

### 4.4 顶点格式与 draw call（✅）

| 路径 | FVF | 顶点 stride | 顶点内容 | DrawPrimitive 类型 |
| --- | --- | :---: | --- | --- |
| sprite 批量（flush_sprites） | `0x144` = XYZRHW\|DIFFUSE\|TEX1 | 28 | xyzw + D3DCOLOR + uv | `TRIANGLELIST(4)`，`2×n` 三角形 |
| textureCircle（mode 9/12/13/14） | 同上 `0x144` | 28 | 同上 | `DrawPrimitiveUP(TRIANGLESTRIP(5), 2*I0-2, buf, 28)` |
| textureArc3D（mode 24/25） | `0x102` = XYZ\|TEX1 | 20 | xyz + uv | `DrawPrimitive(TRIANGLESTRIP(5), 2, count)` |
| rect 族（16/20/21/22/26/27） | （共用 sprite 批量缓冲） | — | — | 经 `0x468c70` 等写批量缓冲后 flush |

rect 族（`drawRect` 等）**写进 sprite 批量缓冲**（`0x4694a8` 处 `SetRenderState(ZWRITEENABLE=0xe, 0)` 关深度写），
走 `flush_sprites` 的 `TRIANGLELIST`。`sprite_size`（`+0x70`/`+0x74`）在 `0x468722`/`0x4689b3` 里
**乘以 `scale`**（`movss xmm0,[esi+0x70]; mulss xmm0,[esi+0x50]`）得最终尺寸 ✅。

---

## 5. 专项② 三个签名族的字段落点

**(1) `S nmax`（600/601/602/609/610）**：`arg0` 按 int 读（`get_int_var` `0x45f610`，mask bit0 置位时查变量），
`×56`（textureCircle 族）或 `×48`（textureArc3D 族）→ `0x4064e0` 分配缓冲。**真实数据里 ZUN 传的是变量 `$I0`**
（`10000`，§6），于是「缓冲大小 = `56 × 当时的 I0`」；之后脚本若再改 `I0`，缓冲不变、绘制按新 `I0`——
这正是 thpages「`I0` MUST be `<= nmax`」的成因 ✅。

**(2) `ff w,h`（603/606/607/608/612/613）**：`arg0`/`arg1` 按 float 读（`get_float_var` `0x45f2d0`），写
`+0x70`（`sprite_size.x`）/`+0x74`（`sprite_size.y`）。**这是与 `set_sprite` 共用的字段**（02 §3.3：
`sprite_size` 由 `0x45ee36` 填）——sprite 的像素尺寸和 rect 的 `w/h` 落在同一对槽里，播放器不能各存一份。

**(3) `fS` / `ffS`（604/605/611）**：`arg0`（radius）→ `+0x70`，末位 int（nInit）→ `+0x4a0`（`I0`）。
`611` 多一个中间 float（thickness）→ `+0x74`。**`I0` 是「点数/边数」，绘制时被 `2*I0` 用**（§3 要点 3）。

**`613 drawLine` 的 `arg1`**：case 体**确实写了** `+0x74`（与 603 共用尾巴），但 mode 26 的绘制 handler
（`0x46882a` → `0x469330`）不读 `+0x74`（线是 1 像素厚、只取 x 长）。所以 thpages 的 `args:['len','unused']`
**「unused」是渲染层的说法**；VM 层它照写不误 ✅。

---

## 6. 真实数据对账（54 个 `.anm`）

自写解析器（按 01 §3–§5 布局）跑 `local/th16.v1.00a/dat/*.anm`：54 文件 / 41339 指令，本组 **90 条**。

| 检查项 | 结果 |
| --- | --- |
| 本组每条 `size == 8 + 4×参数个数`（按 §2 签名推） | **90 / 90 吻合** |
| 各 opcode 出现次数（分母 90） | `604`×42 · `606`×25 · `600`×11 · `603`×4 · `605`×4 · `608`×2 · `602`×1 · `612`×1 |
| **一次都没出现的 6 条** | `601` `607` `609` `610` `611` `613` |
| 600/602 的 `arg0`（int，mask bit0） | **11+1 = 12 条全是被 mask 标成的变量 `10000`（`$I0`）** |
| 604/605 的 `arg0`（float radius） | 42+4 条全 mask=0 字面量，值域 `[1.0, 256.0]` |
| 604/605 的 `arg1`（int nInit） | 全 mask=0，值 ∈ `{3,12,32,36,40,50}`（正多边形的边数） |
| 603/606/608/612 的 `arg0/arg1`（float w/h） | 全 mask=0，值如 `8×8`、`640×480`、`96×2`、`128×96`、`102×10`（像素尺寸） |

**证伪过的竞争假设**：

- 「600 的 `nmax` 是字面量」→ **证伪**：12/12 是变量 `$I0`（thpages 说「arg 决定缓冲大小」，没说它是变量；本文补上）。
- 「604/605 的 radius 是变量」→ **证伪**：46/46 是字面量（radius 想动态改就用 `scale`，正合 thpages）。
- 「613 的 arg1 被 VM 丢弃」→ **证伪**：case 体写 `+0x74`（§5(3)）。

**结论**：ZUN 自家只用 8/14 条；`601`/`607`/`609`/`610`/`611`/`613` 的 handler **实现完整**（各有独立 case + mode），
只是没数据用——mod 作者可用，但**无 ground truth 可对**（§8）。

---

## 7. 与社区不一致处的裁决

对账对象：thpages `anm.ts`（`local/vendor/thpages/js/tables/reference/anm.ts`）与 th-re-data 的 case label
（`th16-anm-index.md`）。

| # | 社区说法 | 一手 | 裁决 |
| --- | --- | --- | --- |
| 1 | th-re-data：`600__kaleidoscope`、`604__drawStar`、`603__drawRectSharp`、`607__drawRectSmooth` | 一手：mode 9=textureCircle、17=drawPoly、16=drawRect、21=drawRectShadow | **纯命名差异**，thpages 的 `textureCircle/drawPoly/drawRect/drawRectShadow` 是现名；th-re-data 是旧名（`drawStar`/`drawRectSharp`/`drawRectSmooth`）。语义无冲突 |
| 2 | thpages：609/610 用 `F0–F3`+`I0`/`I1` | 一手：字段读在**帧尾 `0x4632f0`**（`+0x4b0/4b4/4b8/4bc` + `+0x4a0/4a4`），不在绘制函数 `0x467d00`（后者只读 rotation/scale/uv） | **thpages 对**；本文补「读在哪」。绘制函数 `0x467d00` 读的是 `rotation`/`scale`/`scale_2`/`uv_scale`（§4.4 前，构造 WORLD 矩阵），几何参数读在 `0x4632f0` |
| 3 | thpages：`613 drawLine` `args:['len','unused']` | VM 层 `arg1` 照写 `+0x74`，渲染层（mode 26）忽略 | thpages 的「unused」是渲染层；VM 层不是「丢弃」。措辞级 |
| 4 | thpages blendMode 表标 `wip:1` | 本文在 th16 上逐格复核（§4.2） | thpages 对，`wip` 降为 ✅（数值；枚举名仍 🟡） |

其余 10 条的签名、渲染模式、字段落点与 thpages **0 处错位** ✅。

---

## 8. 写播放器会踩的坑

| 坑 | 说明 |
| --- | --- |
| **Drawing 组不发 draw call** | 它只置 `flags_lo` 渲染模式 + 写 `sprite_size`/`I0`/分配缓冲。绘制在帧尾 `0x4632f0`（填顶点）+ `draw_vm`（发 call）。`group-draw.js` 必须只置状态，与 I8 的渲染后端分离 |
| **`sprite_size` 是「基础尺寸」** | 绘制时 `×scale`（`0x468722` 的 `mulss [esi+0x50]`）。`drawRect(1.0,1.0)` + `scale=100` 才是 100px。别把 `w/h` 当最终像素 |
| **`I0` 复用作点数/边数** | 600/604/605/609/610/611 都把 `n`（或 `nmax`）和 `I0` 绑在一起。播放器改了 `I0` 要立刻反映到顶点数 |
| **600 的 `nmax` 是变量 `$I0` 本身** | ZUN 数据 12/12 传 `$I0`。汇编器若把 `nmax` 只当字面量，会与真实数据对不上 |
| **`sprite_size` 与 sprite 共用** | `drawRect` 写 `+0x70/0x74` 与 `set_sprite` 写的是同一对槽。VM 状态对象里只有一份 `sprite_size` |
| **render mode 字段是 bit 25–29** | 每条只改这 5 位（§1.1），**不碰** blend（bit 5–8）/采样地址（bit 30–31）。播放器别把「设 render mode」和「设 blend」混成一个动作 |
| **alpha test 不是 no-op** | `ALPHAREF=1`、`ALPHAFUNC=GREATEREQUAL`：丢弃 `alpha < 1/255`。WebGL 要在 shader 里 `discard`，但只在 blend mode ≠ 3 时（§4.1/4.2） |
| **texture stage = 一次乘法** | `tex × (DIFFUSE 或 TFACTOR)`，`COLORARG1` 恒为纹理。别照「固定管线默认」多写一层 color/alpha op |
| **blend mode 3 关 alpha test** | 只有这一种模式把 `ALPHATESTENABLE` 设 0。其余 9 种都开 |
| **6 条无 ground truth** | `601`/`607`/`609`/`610`/`611`/`613` ZUN 没用过。实现要靠本文语义 + thpages 描述，别「跑一遍看看」（没有可跑的真实样本） |
| **`609/610` 的几何在帧尾算** | 顶点填在 `0x4632f0`（读 F0–F3/I0/I1），绘制函数只发 call。播放器若把「算顶点」和「发 call」放一处，会跟 `302 renderMode` 切模式时的时序对不上 |

---

## 9. 还没搞清楚的

| 问题 | 状态 |
| --- | :---: |
| 609/610（textureCylinder3D/textureRing3D）的**精确 3D 几何公式**（`F0–F3` 如何构成圆柱/环截面、`I1` 重复次数的映射）——本文只证到字段读落点 | 🟡 |
| 601/602 在 `0x4632f0` 里「弧从 0 起 / 居中于 0」的精确展开公式（只证到 mode 9/13/14 走同绘制函数、几何分支在 `0x4632f0`） | 🟡 |
| rect 族的 `flags_lo` bit 21–24（`0x468722`/`0x4689b3` 里 `(flags_lo>>21)&3`、`(flags_lo>>23)&3`）是谁设的、编码什么（疑似 R6 的 `423 colorMode`） | ❓ 归 R6 |
| `Supervisor__reset_render_state` `0x45ba80` 的完整调用时机（是每帧还是仅初始化）与其余状态（`0x89`、`FOG*`）的语义 | 🟡 |
| `613 drawLine` 的 `arg1`（写进 `+0x74` 但渲染忽略）是否有别的读点 | ❓ |
| 本组在 TH18 的差异（本文只做 TH16，不外推） | — 阶段 3 |

---

## 关联

- 指令头 / `param_mask` / script 表：[`01-format.md`](01-format.md)
- VM 主循环、`zAnmVm` 字段图、渲染模式字段位、帧尾流程：[`02-vm-core.md`](02-vm-core.md) §2/§3.3/§3.6
- 变量号（`I0`=10000、`F0–F3`=10004–10007）：[`04-variables.md`](04-variables.md) §2
- 操作数求值次序判据（本组 §2.1 引用）：[`03-math-jumps.md`](03-math-jumps.md) §9.1
- 渲染阶段 × 相机（th18 参照，th16 应同构但需自证）：[`../th18/02-render-stages.md`](../th18/02-render-stages.md)
- 子系统总览：[`../OVERVIEW.md`](../OVERVIEW.md)
- 本作入口：[`README.md`](README.md)
- 想要的 Ghidra 重命名（待 T10 统一落库）：[`naming-batch.md`](naming-batch.md)
- 纪律：[`../../../METHOD.md`](../../../METHOD.md)、[`../../../DOCSTYLE.md`](../../../DOCSTYLE.md)
