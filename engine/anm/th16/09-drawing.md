# ANM v8 Drawing 组 + render state — TH16 一手逐条确认（opcode 600–613，14 条）

> **版本**：TH16 v1.00a（`th16.exe`，imagebase `0x400000`）。本文裸地址默认属该版本；引用其他版本须写成 `th18:0x…`。
> **方法**：**不经 Ghidra 渲染**——直接解 PE 节表把 `.text` 字节喂 capstone，逐条反汇编 14 个 case 体、
> 绘制分发 `draw_vm` `0x468490`、几何填充 `0x4632f0`、render state 设置器 `0x464f10` 与各 mode 绘制函数；
> 高段分发表（`0x462fa8` 索引 + `0x462e64` 跳表）逐字节解出自证「opcode → case 地址」；
> 再用自写解析器跑 `local/th16.v1.00a/dat/` 的 **54 个真实 `.anm`（41339 指令，其中本组 90 条）**做对账与证伪。
> **可信度**：14 条的**签名（参数个数/类型/落点）与渲染模式全部 ✅**（每条指得到一条读/写 `flags_lo` 或 vm 字段的机器指令）；
> render state 扫描的 blend / sampler / texture-stage / FVF 全部 ✅。
> **★ 2026-09-10 补**：原先标 🟡 的两条几何公式（609/610 的 3D 截面、601/602 的弧角展开）已在
> `0x4632f0` 上**逐条解完并升 ✅**，见 [§3.2](#32-mode-1314--2d-圆环弧601602弧角展开公式原-已解)–[§3.4](#34-mode-2425--3d-圆柱--圆环609610几何公式原-已解)；
> 同一轮更正了 §4.4 的 textureArc3D 顶点格式（FVF `0x142` 不是 `0x102`、stride 24 不是 20、
> `DrawPrimitiveUP` 不是 `DrawPrimitive`）。
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
**56 = 28×2、48 = 24×2**——`nmax` 是「**点对**数上限」，每个点对写 **2 个顶点**：
textureCircle 族的顶点 stride 是 **28** 字节（FVF `0x144`），textureArc3D 族是 **24** 字节（FVF `0x142`），
所以倍数是 `2 × stride`。**缓冲只是分配，顶点由 `0x4632f0` 在帧尾填**——§3.2–§3.4 已逐条解出填法，
两族的顶点数都恰好是 `2 × I0`，与这里的 `56 × nmax` / `48 × nmax` 自洽（这也是「`I0` MUST be `<= nmax`」的由来）。
（★ 2026-09-10：原文此处写「textureArc3D 是 20 字节」，与同句的 `48 = 24×2` 自相矛盾，已按一手更正为 24，见 §4.4。）

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
| 0 → `0x468504`；1 与 3 → `0x468542`（sprite 角点） | 见左 | `300 sprite` / 默认 | 15 | `0x468647`（存变换 + mode 8） | `302` |
| 2 | `0x4686e0`（sprite，−0.5px 偏移） | `302` | 16/20/21/22/26/27 | `0x468722`（rect 族） | **603/606/607/608/613/612** |
| 4 | `0x468584`（mode 4） | `302` | 17/18/19 | `0x4689b3`（poly 族） | **604/605/611** |
| 5/6/7/8 | `0x4685af`/`0x4685d5`/`0x4685fb`/`0x468621` | `302` | 24/25 | `0x4686c1`（textureArc3D） | **609/610** |
| 9/12/13/14 | `0x468683`（textureCircle） | **600**/`302`/**601/602** | 10/23/28+ | `0x468b63`（default） | — |
| 11 | `0x4686a2`（mode 11） | `302` | | | |

三个要点：

1. **600/601/602 在绘制层完全同函数**（`0x4681f0` textureCircle）——600 的差别在**帧尾几何** `0x4632f0`
   有自己的 case，而 **601 与 602 连帧尾几何都共用同一段**（`0x4635ed`），分野在那段内部的一次
   `cmp eax,0x1c000000`（详见 §3.1/§3.2）。绘制层（`draw_vm`）对三者一视同仁。
   同理 609/610 在绘制层共用 `0x467d00`、在帧尾几何也共用 `0x4638da`（§3.4）。
2. **rect 族（16/20/21/22/26/27）共用 `0x468722`**，它再按 `mode-16` 走第二张跳表 `0x468bc8`（12 项）分到
   6 个 rect handler（§4.4）；poly 族（17/18/19）走 `0x4689b3`，按 `mode-17` 分到 3 个 poly handler。
3. **`+0x4a0`（`I0`）是绘制时的点数/边数**：`0x468683`/`0x4686c1` 都 `mov eax,[esi+0x4a0]; add eax,eax` 把 `2*I0`
   当顶点数传进绘制函数——所以「`I0` = 点/边数」是**绘制层一手**的，不是抄 thpages ✅。

### 3.1 帧尾几何填充 `0x4632f0`（`AnmVm::sub_4632f0`）—— ★ 2026-09-10 逐条解完

它每帧在帧尾被调（02 §2 的 `update_geometry_for_render_mode`），**按 `render_mode - 9`（mode 9–25）分发**
（`0x46330d`–`0x46332c`，索引表 `0x463b14` / 跳表 `0x463b04`），把顶点写进 `vm+0x5b8` 缓冲。

**分发一手解出**（索引表 17 字节 `[0,3,3,3,1,1,3,3,3,3,3,3,3,3,3,2,2]`，跳表 4 项
`{0x463333, 0x4635ed, 0x4638da, 0x463afb}`）：

| mode | case | 谁设的 |
| :---: | --- | --- |
| 9 | `0x463333` | `600 textureCircle` |
| **13 与 14** | **`0x4635ed`（同一个 case 体）** | `601 textureArcEven` / `602 textureArc` |
| **24 与 25** | **`0x4638da`（同一个 case 体）** | `609 textureCylinder3D` / `610 textureRing3D` |
| 其余（含 10/11/12/15–23） | `0x463afb`（default，什么都不填） | — |

> ⚠️ **这里更正 §3 要点 1 的措辞**：原文说「600/601/602 的差别在帧尾几何 `0x4632f0`」。准确说法是
> **600 有自己的 case，而 601 与 602 连帧尾几何都共用同一段**——它们的分野在那段代码**内部**的一次
> `cmp eax, 0x1c000000`（见 §3.2）。609/610 同理。

### 3.2 mode 13/14 = 2D 圆环弧（`601`/`602`）——弧角展开公式（✅，原 🟡 已解）

**① 发现**：case 体 `0x4635ed` 开头就读 `+0x38`（`rotation.x`）与 `+0x40`（`rotation.z`），且
`0x4636b1`–`0x4636bb` 有一次 `and eax,0x3e000000` / `cmp eax,0x1c000000` 的**自我模式判别**。

**② 推测**：`rotation.x` = 总弧角、`rotation.z` = 基准角，那次自判就是 601 与 602 的分野。

**③ 验证**（逐条反汇编 `0x4635ed`–`0x4638d9`）：

```c
arc   = vm.rotation.x;                      // 0x4635ed  +0x38
base  = vm.rotation.z;                      // 0x4635f4  +0x40
centered = normalize_angle(base - arc*0.5); // 0x463615 减半；0x463621-0x463664 内联归一
dtheta = arc / (I0 - 1);                    // 0x463692  -> [ebp-0x14]
dv     = I1  / (I0 - 1);                    // 0x463696  -> [ebp-0x10]
// ★ 601 与 602 的唯一分野（0x4636b1..0x4636cc）：
theta = ((flags_lo & 0x3e000000) == 0x1c000000)   // 0x1c000000 >> 25 = 14
      ? normalize_angle(vm.rotation.z)             // mode 14 = 602：从 base 起
      : centered;                                  // mode 13 = 601：以 base 为中心
```

然后循环 **`I0` 次**，每次写两个顶点（stride `0x1c` = 28 字节），角度每轮 `theta += dtheta` 后
**内联归一进 `[-π, π]`**（`0x463896`–`0x4638c7`，与 `129 mathReduceAngle` 同一套 34 步守卫）：

| 顶点 | 半径 | 位置 | 颜色 | u | v |
| --- | --- | --- | --- | --- | --- |
| 偶（外） | `r_out` | `(cos θ·r, sin θ·r, 0) + final_pos` | 见下 | `uv_scroll.x + sprite_uv[0].u` | `uv_scroll.y + k·dv` |
| 奇（内） | `r_in` | 同上，换半径 | 同左 | `uv_scroll.x + sprite_uv[1].u` | 同左 |

- **半径来自 `scale`**（`0x4636e7`–`0x46370a`）：`r_out = scale.y + scale.x/2`、`r_in = scale.y - scale.x/2`
  ⇒ **`scale.y` = 环的中心半径、`scale.x` = 环宽**。
- `(cos, sin)` 由 `0x464d60` 算（`fld θ; fsincos; fmul r` ⇒ ST0 = cos ⇒ `[eax]=cos·r`、`[eax+4]=sin·r`）。
- **位置偏移** `final_pos` 由 `0x406c40` 给出 = `pos + entity_pos`（`+0x2c..+0x34` 加 `+0x5e0..+0x5e8`）
  ⇒ mode 9/13/14 写的是**世界坐标**顶点。
- **颜色**：`(flags_lo & 0x60000)` 非 0 取 `color_2`（`+0x524`）、否则 `color_1`（`+0x520`）——
  ★ **内外两个顶点用的是同一个颜色**，本族**没有**内外渐变（与 mode 9 不同，见 §3.3）。
- `sprite_uv[0]`/`[1]` = `+0x3a8`/`+0x3b0`（02 §3.3 的「当前 sprite 的 UV 四角」`f32[2][4]` 的前两角的 u）。
- **`I1` = 贴图沿环重复次数**：v 从 `uv_scroll.y` 均匀走到 `uv_scroll.y + I1`（`k·dv`，`k = 0..I0-1`）。

**④ 结论**（✅ TH16）：

- **`601 textureArcEven`（mode 13）**：弧**以 `rotation.z` 为中心**展开 —— 起始角 = `rotation.z - rotation.x/2`。
- **`602 textureArc`（mode 14）**：弧**从 `rotation.z` 起**展开 —— 起始角 = `rotation.z`。
- 两者其余完全相同：`rotation.x` = 总弧角（可负 = 反向），`I0` = 点对数，顶点数 = `2·I0`，
  角步长 `rotation.x/(I0-1)` ⇒ 最后一对正好落在「起始角 + 总弧角」。

**证伪与量纲检查**：
- **缓冲大小自洽** ✅：`2·I0` 个顶点 × 28 字节 = `56·I0`，正是 `600/601/602` 分配的 `56 × nmax`（§2.2）。
- **唯一一条真实 `602` 对得上** ✅（`front.anm` #244，本组仅此一条）：
  `set(I0,38)` · `set(I1,1)` · `602($I0)` · `rotate(-6.2832, 0, -1.5708)` ——
  `rotation.x = -2π` **恰好是整圈**（负 = 反向），`rotation.z = -π/2` 是起始角，`I1 = 1` = 贴图绕一圈重复一次。
  三个量纲全部成立；若 `rotation.x` 不是「总弧角」，`-2π` 这个取值没有任何意义。
  且 `dθ = -2π/37`、走 38 对 ⇒ 末对正好回到起点，**环自然闭合**——这解释了为什么本族**不像 mode 9
  那样在末尾补一对闭合顶点**（§3.3），两族最终都是 `2·I0` 个顶点。
- **社区交叉对名** ✅：thpages 的 `textureArcEven`（*even* = 居中）与 `textureArc` 的语义方向一致。

**⑤ 证据**：`0x4635ed`–`0x4638d9` 逐条；模式自判 `0x4636b1`/`0x4636b6`；归一 helper `0x402d90`
（与 `0x402d30` 同形，直接取 `xmm0` 不先加 y）；极坐标 `0x464d60`；位置 `0x406c40`；
常量 `0x494588`=π、`0x494734`=−π、`0x4945b8`=2π、`0x49449c`=0.5。

### 3.3 mode 9 = 完整圆环（`600 textureCircle`）

与 §3.2 同族，四处差异（`0x463333`–`0x4635ec`）：

1. **角度**：起始角 = `rotation.z`（不减半、不自判），步长恒 `2π/(I0-1)`（`0x463363`）——**永远整圈**，
   `rotation.x` 不参与。
2. **循环 `I0-1` 次**，末尾**再补一对闭合顶点**（`0x46358b`–`0x4635df`：把缓冲里的第 0/1 个顶点整块
   拷到末尾，只把 v 改写成累计终值）⇒ 顶点数同为 `2·I0`。
3. **内外可用不同颜色**：外顶点恒 `color_1`（`ESI`），内顶点在 `(flags_lo & 0x60000)` 非 0 时用
   `color_2` ⇒ **本 mode 支持内外径向渐变**，`601/602` 不支持。
4. **`I1` 同样是贴图重复次数**：v 走到 `uv_scroll.y + I1`。

### 3.4 mode 24/25 = 3D 圆柱 / 圆环（`609`/`610`）——几何公式（✅，原 🟡 已解）

**① 发现**：case 体 `0x4638da` 读 `F0`（`+0x4b0`）、`F1`（`+0x4b4`）、`F2`（`+0x4b8`）、`F3`（`+0x4bc`）
与 `I0`/`I1`，且 `0x4639c2` 有一次 `cmp eax,0x32000000` 的自我模式判别（`0x32000000 >> 25 = 25`）。

**② 推测**：四个 `F` 分别是「总角 / 高（宽）/ 半径 / 中心角」（thpages 的说法），那次自判是 609 与 610 的分野。

**③ 验证**（逐条反汇编 `0x4638da`–`0x463afa`）：

```c
theta  = normalize_angle(F3 - F0*0.5);   // 0x4638ec-0x463951：恒居中，本族没有「从 0 起」的变体
dtheta = F0 / (I0 - 1);                  // 0x463975
dv     = I1 / (I0 - 1);                  // 0x463979
h = F1 * 0.5;  rA = F2;  rB = F2;        // 0x463997/0x4639b0：mode 24 的取值
if ((flags_lo & 0x3e000000) == 0x32000000) {   // mode 25 = 610
    rA = F2 - F1*0.5;                    // 0x4639c9  内半径
    h  = 0;                              // 0x4639cd  压平
    rB = F2 + F1*0.5;                    // 0x4639d2  外半径
}
// 循环 I0 次，每次两个顶点，stride 0x18 = 24 字节：
//   顶点 A = (cos θ·rA,  +h, sin θ·rA)   0x463a3c-0x463a54
//   顶点 B = (cos θ·rB,  -h, sin θ·rB)   0x463a98-0x463ab1（-h 由 XORPS 0x494890 符号翻转得到）
//   两者都带 color（同一个）、u 分别取 sprite_uv[0].u / sprite_uv[1].u、v = uv_scroll.y + k·dv
// θ += dtheta 后内联归一进 [-π, π]（0x463ac0-0x463af1）
```

**④ 结论**（✅ TH16）：两条指令共用一段代码，靠 `F1` 的**用法**分出两种形状：

| | `609 textureCylinder3D`（mode 24） | `610 textureRing3D`（mode 25） |
| --- | --- | --- |
| 两圈的半径 | 都是 `F2` | `F2 - F1/2`（内）与 `F2 + F1/2`（外） |
| 两圈的高（y） | `+F1/2` 与 `-F1/2` | 都是 `0` |
| 形状 | 半径 `F2`、**高 `F1`** 的**圆柱侧壁** | y=0 平面内、中心半径 `F2`、**宽 `F1`** 的**平面圆环** |

公共项：`F0` = 总角、`F3` = 中心角（**恒以 `F3` 为中心**展开，没有 601/602 那种「从 0 起」的变体）、
`I0` = 点对数、`I1` = 贴图沿环重复次数、顶点数 `2·I0`。
**圆在 XZ 平面、高度沿 Y 轴**；顶点是**模型坐标**——本族**不加** `final_pos`（对比 §3.2），
世界变换由绘制函数 `0x467d00` 的 WORLD 矩阵完成（与 §7 裁决 2 一致）。

**证伪与量纲检查**：
- **缓冲大小自洽** ✅：`2·I0` × 24 字节 = `48·I0`，正是 `609/610` 分配的 `48 × nmax`（§2.2）。
  这同时**证伪了 §4.4 原表的 stride「20 字节」**——见 §4.4 的更正块。
- **交叉对名** ✅：mode 24 解出来是「同半径、上下两圈」= 圆柱，mode 25 是「同平面、内外两圈」= 圆环，
  与 thpages 的 `Cylinder` / `Ring` 逐个对上。**若把两者解反，名字会立刻对不上**——这是本条最强的对抗证伪。
- ⚠️ **无真实数据**：`609`/`610` 在 ZUN 的 54 个 `.anm` 里一次都没出现（§6），所以上表**没有画面级 ground truth**，
  可信度靠「一手到底 + 缓冲大小自洽 + 交叉对名」三条闸门，而不是靠跑一遍。

**⑤ 证据**：`0x4638da`–`0x463afa` 逐条；模式自判 `0x4639c2`；符号掩码 `0x494890` 直读 = 四个 `0x80000000`；
`SetFVF` `0x468133` `push 0x142`；`DrawPrimitiveUP` `0x4681cc`（`push 0x18` = stride 24）。

### 3.5 三族共用的两件事

1. **父缩放**（`0x4633c8`–`0x4633dd` / `0x46370e`–`0x463723`）：父非空且 `!(flags_hi & 0x10000)` 时
   `r_out *= parent.scale.x`、`r_in *= parent.scale.y`。
   ❓ **两个半径乘的是父 `scale` 的不同分量**——父的 scale 非等比时环会畸变（内外被不同倍率拉伸）。
   两处（mode 9 与 mode 13/14）写法一致，所以不是抄漏；是不是 ZUN 的笔误无从判断，如实记录。
   （mode 24/25 这条路径上没有父缩放。）
2. **`313 resolutionMode`**（`0x4633e2`–`0x463424` / `0x463728`–`0x46376a`）：
   `flags_hi & 0x700000` == `0x100000` ⇒ 两个半径都 × `0x4d9d34`；== `0x200000` ⇒ 都 × `0x4d9d34` × 0.5。
   与 06 §3.6 的结论一致（值 0 与 3 都不缩放）。

## 4. ★ render state 一手扫描

回答 spec §10.3 的三行 🟡：「ANM 到底设了哪些状态」。按调用点分三处：全局重置、per-VM 状态、mode 函数。

**识别 vtable 偏移**（IDirect3DDevice9，d3d9.h 顺序）：`[reg+0xe4]`=57=**SetRenderState**、
`[reg+0x10c]`=67=**SetTextureStageState**、`[reg+0x114]`=69=**SetSamplerState**、
`[reg+0x104]`=65=SetTexture、`[reg+0xb0]`=44=SetTransform、`[reg+0x164]`=89=SetFVF、
`[reg+0x190]`=100=SetStreamSource、`[reg+0x144]`=81=DrawPrimitive、`[reg+0x14c]`=83=DrawPrimitiveUP。
调用形如 `push <value>; push <state>; push <device>; mov ecx,[eax]; call [ecx+0xe4]`（`this` 走栈）。

### 4.1 全局重置：`Supervisor__reset_render_state` `0x45ba80`

设备初始化时设一次默认状态（逐条反汇编，✅；括号内是 D3D9 SDK 的枚举名，🟡）。
★ **下表只是它设的一个子集**（2026-09-10 审阅补）：`0x45ba80` 除这 8 条 `SetRenderState` 外，还调了
**8 次 `SetTextureStageState`**（`0x45bc36`–`0x45bcc2`，`[ecx+0x10c]`）与 **6 次 `SetSamplerState`**
（`0x45bcd6`–`0x45bd3a`，`[ecx+0x114]`）设默认值。这些默认值后续都被 per-VM（§4.2）与 mode 函数（§4.3）
覆盖，**不影响本节的行为结论**——但原文「逐条反汇编」这句没说明只挑了一个子集，写播放器时若照本节
当成「ANM 只设了这 8 条」会漏掉初始态。

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
| textureArc3D（mode 24/25） | **`0x142`** = XYZ\|DIFFUSE\|TEX1 | **24** | xyz + D3DCOLOR + uv | `DrawPrimitiveUP(TRIANGLESTRIP(5), n−2, buf, 24)` |
| rect 族（16/20/21/22/26/27） | （共用 sprite 批量缓冲） | — | — | 经 `0x468c70` 等写批量缓冲后 flush |

> ★ **2026-09-10 更正（textureArc3D 那一行原来三处都错）**：一手是 `0x468133` `push 0x142` →
> `SetFVF(0x142)` = `D3DFVF_XYZ(0x2) | D3DFVF_DIFFUSE(0x40) | D3DFVF_TEX1(0x100)`，**带 DIFFUSE**；
> `0x4681cc` 是 `DrawPrimitiveUP`（vtable `0x14c` = 83）不是 `DrawPrimitive`，且 `push 0x18` ⇒ **stride 24**，
> 图元数 = 顶点数 − 2（`0x4681c5` `add eax,-2`）。原写的 `0x102`/20/`DrawPrimitive` 三项均不成立。
> **§2.2 自己的算术早就指向 24**（`48 = 24×2`），只是 §4.4 这一行写成了 20 —— 两处自相矛盾，
> 现在以 §3.4 逐条解出的顶点布局（xyz 12 + color 4 + uv 8 = 24）为准。

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
| **`600/601/602` 的半径藏在 `scale` 里** | `scale.y` = 环的**中心半径**、`scale.x` = **环宽**（`r_out = scale.y + scale.x/2`、`r_in = scale.y - scale.x/2`，§3.2）。把 `scale` 当普通缩放用会画出完全不同的东西 |
| **`609/610` 的圆在 XZ 平面、高沿 Y** | 顶点是 `(cos θ·r, ±h, sin θ·r)`（§3.4），不是常见的 XY 平面。`F1` 在 mode 24 当**高**、在 mode 25 当**宽**——同一个参数两种量纲 |
| **两族的坐标空间不同** | mode 9/13/14 的顶点已加 `pos + entity_pos`（`th16:0x406c40`）= **世界坐标**；mode 24/25 **不加**，是**模型坐标**，靠 `th16:0x467d00` 的 WORLD 矩阵变换。混用会让 3D 族的位置错两次 |
| **角度每轮都要归一** | 三族的循环里 `θ += dθ` 之后都**内联**了 `[-π, π]` 归一（34 步上限，同 `129 mathReduceAngle`）。省掉它在大 `I0` × 大弧角时会与真机分岔 |
| **`I1` 是贴图重复次数不是别的** | v 坐标从 `uv_scroll.y` 均匀走到 `uv_scroll.y + I1`（三族一致）。`I1 = 0` 会让整条环共用一行纹素 |

---

## 9. 还没搞清楚的

| 问题 | 状态 |
| --- | :---: |
| ~~609/610 的精确 3D 几何公式~~ → **已解**（2026-09-10）：见 [§3.4](#34-mode-2425--3d-圆柱--圆环609610几何公式原-已解)。⚠️ 无真实数据可对画面，可信度靠「一手到底 + 缓冲大小自洽 + 交叉对名」 | ✅ |
| ~~601/602 的精确弧角展开公式~~ → **已解**（2026-09-10）：见 [§3.2](#32-mode-1314--2d-圆环弧601602弧角展开公式原-已解)。分野是同一段代码内的 `cmp eax,0x1c000000` | ✅ |
| 父缩放把 `r_out`/`r_in` 分别乘父 `scale.x`/`scale.y`（§3.5 第 1 条）是有意还是笔误 | ❓ |
| rect 族的 `flags_lo` bit 21–24（`0x468722`/`0x4689b3` 里 `(flags_lo>>21)&3`、`(flags_lo>>23)&3`）是谁设的、编码什么（疑似 R6 的 `423 colorMode`） | ❓ 归 R6 |
| `Supervisor__reset_render_state` `0x45ba80` 的完整调用时机（是每帧还是仅初始化）与其余状态（`0x89`、`FOG*`）的语义 | 🟡 |
| `613 drawLine` 的 `arg1`（写进 `+0x74` 但渲染忽略）是否有别的读点 | ❓ |
| 本组在 TH18 的差异（本文只做 TH16，不外推） | — 阶段 3 |

---

## 关联

- 指令头 / `param_mask` / script 表：[`01-format.md`](01-format.md)
  —— ★ 顺带关掉它 §7 的一条开放项：「`D3DFMT_A8` 贴图在 TH16 的 texture stage 里从哪取 color
  （推测 `D3DTA_DIFFUSE`，未在 exe 上证）」🟡，答案在本文 [§4.3](#43-mode-函数里的-texture-stage-statereg0x10c)：
  `COLORARG1`/`ALPHAARG1` **从不设**，恒为 D3D9 默认 `D3DTA_TEXTURE`；被 MODULATE 乘上去的是
  `COLORARG2` = `DIFFUSE`（sprite/textureCircle）或 `TFACTOR`（textureArc3D）。
- VM 主循环、`zAnmVm` 字段图、渲染模式字段位、帧尾流程：[`02-vm-core.md`](02-vm-core.md) §2/§3.3/§3.6
- 变量号（`I0`=10000、`F0–F3`=10004–10007）：[`04-variables.md`](04-variables.md) §2
- 操作数求值次序判据（本组 §2.1 引用）：[`03-math-jumps.md`](03-math-jumps.md) §9.1
- 渲染阶段 × 相机（th18 参照，th16 应同构但需自证）：[`../th18/02-render-stages.md`](../th18/02-render-stages.md)
- 子系统总览：[`../OVERVIEW.md`](../OVERVIEW.md)
- 本作入口：[`README.md`](README.md)
- 想要的 Ghidra 重命名（待 T10 统一落库）：[`naming-batch.md`](naming-batch.md)
- 纪律：[`../../../METHOD.md`](../../../METHOD.md)、[`../../../DOCSTYLE.md`](../../../DOCSTYLE.md)
