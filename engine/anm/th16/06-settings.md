# ANM v8 Settings 组 — TH16 一手逐条确认（opcode 300–317，18 条）

> **版本**：TH16 v1.00a（`th16.exe`，imagebase `0x400000`）。本文裸地址默认属该版本；引用其他版本须写成 `th18:0x…`。
> **方法**：**不经 Ghidra 渲染**——直读 PE 节表把 `.text` 字节喂 capstone，逐条反汇编 18 个 case 体与其调用的 helper
> （`set_sprite` `0x45edb0`、`set_layer` `0x406d80`、`0x469e20`）；高段分发表（`0x462fa8` 索引 + `0x462e64` 跳表）
> 逐字节解出自证「opcode → case 地址」；再用 capstone 全 `.text` **逐函数**扫「谁读/写 `vm+0x530`(flags_lo) / `vm+0x534`(flags_hi)」
> 反查每个 flag 位的读者；最后用自写解析器跑 `local/th16.v1.00a/dat/` 的 **54 个真实 `.anm`
> （1929 script / 41339 指令，其中本组 10587 条）**做对账与证伪。
> **可信度**：18 条的「置哪个位 / 写哪个字段 / 调哪个 helper」**全部 ✅**（每条指得到一条机器指令）；
> 位含义栏：**能一手钉死读者的 ✅，读者未追到的 🟡**（见 §3 逐位）。三个社区 `wip:2`（306/316/317）与
> `307 randMode` 的「位被谁读」本次一手收敛，见 §3.1–§3.3 与 §7 裁决。
> **衔接**：指令头 8 字节与 `param_mask` 见 [`01-format.md`](01-format.md) §5；VM 主循环、时间闸门、
> `zAnmVm` 字段图、`flags_lo`/`flags_hi` 基础位见 [`02-vm-core.md`](02-vm-core.md) §2/§3；
> 变量号与变量编码宽度见 [`04-variables.md`](04-variables.md)。本文不重复这三份的论证。
> **不含版权字节**：只写偏移、地址与语义。

## 0. 一句话结论

这 18 条是 ANM VM 的**「渲染/外观设置」层**：几乎全部只改 VM 的两个 flag 字（`flags_lo` `+0x530`、
`flags_hi` `+0x534`）里的某几个位，外加两条改贴图（300/301）与一条改 layer（304）。四件容易搞错的事，一手结论：

1. **除了 300/301/304/308/309，其余 13 条都只改 flag 字（302–303、305–307、310–317）**——真正的渲染效果由
   `AnmManager::setup_render_state_for_vm` `0x464f10` 与 `AnmManager::draw_vm` `0x468490` 在**每帧渲染时**
   读这些位才兑现（[§3](#3-专项每个-flag-位-被谁读)）。所以「改了 bit 就立刻有效」这个心智模型对，但**有效点不在 opcode handler 里，在渲染器里**。
2. **参数宽度不统一，且分「字面量-only」与「可为变量」两族**（[§4](#4-专项参数宽度与字面量变量两族)）：
   `304/307/315` 只读参数的**低 8 位**（`movzx`）；`302/303/305/306/310/311` 读**字面量 32 位**（不查 `param_mask`，
   永远不可为变量）；只有 `300/301/312/313/314` 会查 `param_mask` 把参数交给 `get_int_var`（可为变量）——
   但真实数据里**这 5 条也一次都没用变量**（`param_mask` 恒 0，见 [§6](#6-真实数据对账54-个-anm)）。
3. **三条 `wip:2` flag 的位被谁读，本次一手钉死**（[§3.1–§3.3](#31-306-置-flags_lo-位15-0x8000--帧尾把全局滚动加进-entity_pos)）：
   `306` 置 `flags_lo` 位 15（`0x8000`）→ 帧尾 `0x462a19` 把全局滚动加进 `entity_pos`；
   `316`/`317` 置/清 `flags_lo` 位 1（`0x2`）→ `AnmManager::draw_vm` `0x4684ca` 读它当**第二道绘制闸门**（清位 = 不画）。
4. **`307 randMode` 在 TH16 是死位**（[§3.4](#34-307-置-flags_hi-位10-0x400--但全-exe-无读者)）：
   它置 `flags_hi` 位 10（`0x400`），但**全 exe 没有任何一条 `test/and/cmp` 读它**——PRNG 流的选择是
   [`03-math-jumps.md`](03-math-jumps.md) §7.4 里的**硬编码 `mov ecx, 0x4a6d80/0x4a6d88`**，不经过这个位。

---

## 1. 怎么核的：opcode → case 地址自己解高段跳表

**① 发现**：Settings 组落在高段分发段（`301 <= opcode <= 613`）。派发序言在 `0x460c46`（低段表覆盖到 300 之后）：

```
00460c46  sub  eax, 0x12d              ; idx = opcode - 301
00460c4b  cmp  eax, 0x138              ; 313（无符号）
00460c50  ja   0x4629c8                ; 越界 → 默认分支（= nop）
00460c56  movzx eax, byte ptr [eax + 0x462fa8]   ; 索引表（313 项）
00460c5d  jmp  dword ptr [eax*4 + 0x462e64]       ; 跳表（81 项）
```

opcode 300 例外：低段序言 `0x45fbdc` 的 `je 0x460ba9` 把它**内联**进主循环体（不查表）。

**② 推测**：`301..317` 的 case 地址 = 跳表 `0x462e64` 按索引表 `0x462fa8` 的下标取。

**③ 验证**：把 `0x462fa8` 的 313 字节与 `0x462e64` 的 81 个 dword 逐字解出来（`pe_read.py --bytes/--dwords`）。
本组 18 条的落点与社区 label / 与 `02-vm-core.md` §3.6 已坐实的写点（如 `316` 置位 `0x461370`、`317` 清位 `0x46138b`、
`302` 写 `0x4623df`）**逐条吻合，0 处错位** ✅。

**④ 结论**：本文全部 18 个 case 地址一手，不依赖社区 label ✅（TH16）。

**⑤ 证据**：`0x460c46`–`0x460c5d`；索引表 `0x462fa8`（`u8[313]`）、跳表 `0x462e64`（`void*[81]`）。

### 1.1 公共骨架与「位域更新」惯用法

Settings 组的 handler 分成两种形态：

- **直接置/清位**（`316`/`317`）：`or/and dword [ebx+0x530], 2`。
- **「按值写位」惯用法**：`dst ^= (src ^ dst) & mask`（三条指令 `mov eax,src; xor eax,dst; and eax,mask; xor dst,eax`）。
  它等价于「把 `dst` 的 mask 位**设成** `src` 的同位值」（**不是 toggle**）——`300` 里的 `302/303/305/306/310/311`
  与 `312/313/314/315` 全用这个。判据：`xor`（不是 `or`）+ 紧跟 `and mask` + 再 `xor` 回写。
  别把它读成「翻转」，否则 `visible(1)` 会被理解成「取反可见性」。

**每条 handler 收尾前几乎都有 `movss xmm1, [0x494720]`**——`0x494720` 直读 = `0xbf800000` = `-1.0f`
（PE 手工解字节）。它是编译器把一个循环不变量搬回 `xmm1` 的噪声，与本组语义无关
（[`03-math-jumps.md`](03-math-jumps.md) §9 的同款坑，`308`/`309` 恰好顺手借它做 `* -1.0f`）。

---

## 2. ★ 18 条指令表

**记号**：`S` = 整型右值（字面量，**不可为变量**）· `$S` = 整型右值（可为变量，`param_mask` bit0 置位时查变量）·
`SS` = 两个整型右值（各自可为变量）· `b` = 参数只取低 8 位（`movzx byte`，字面量）· `—` = 无参数。
「置位」列：`flags_lo` = `vm+0x530`，`flags_hi` = `vm+0x534`。case 地址均为一手（§1）。

| opcode | 助记（thpages / thanm） | 签名 | 语义（一手） | 置位 | case 地址 | 可信度 |
| :---: | --- | --- | --- | --- | --- | :---: |
| 300 | `sprite` / `sprite` | `$S id` | 见 §2.1；`flags_lo \|= 1`，经 `set_sprite` 换贴图 | `flags_lo` bit0 = 1 | `0x460ba9` | ✅ |
| 301 | `spriteRand` / `spriteRand` | `$S a, $S b` | `id = a + (raw % (u32)b)`（流 B）；`flags_lo \|= 1`；`b==0` 崩 | `flags_lo` bit0 = 1 | `0x460f4b` | ✅ |
| 302 | `renderMode` / `type` | `S mode` | `flags_lo` bit25–29 = `mode & 0x1f`；`mode==10` 再 `call 0x469e20` | bit25–29 | `0x4623dc` | ✅ |
| 303 | `blendMode` / `blendMode` | `S mode` | `flags_lo` bit5–8 = `mode & 0xf` | bit5–8 | `0x461571` | ✅ |
| 304 | `layer` / `layer` | `b n` | `set_layer(vm, n & 0xff)`（§2.2） | `+0x18` + `flags_hi` 派生位 | `0x462950` | ✅ |
| 305 | `noZBuffer` / `zWriteDisable` | `S disable` | `flags_lo` bit13 = `disable & 1` | bit13 | `0x461868` | ✅ |
| 306 | `v8-306` /（无） | `S enable` | `flags_lo` bit15 = `enable & 1`；见 §3.1 | bit15（`0x8000`） | `0x461893` | ✅ |
| 307 | `v8-randMode` / `randMode` | `b mode` | `flags_hi` bit10 = `mode & 1`；**死位**，见 §3.4 | bit10（`0x400`） | `0x4629b0` | ✅ |
| 308 | `flipX` / `flipX` | — | `scale.x = -scale.x`；`flags_lo ^= 0x800` | bit11（`0x800`）toggle | `0x4612ed` | ✅ |
| 309 | `flipY` / `flipY` | — | `scale.y = -scale.y`；`flags_lo ^= 0x1000` | bit12（`0x1000`）toggle | `0x461323` | ✅ |
| 310 | `visible` / `visible` | `S visible` | `flags_lo` bit0 = `visible & 1` | bit0 | `0x46177b` | ✅ |
| 311 | `resampleMode` / `resampleMode` | `S n` | `flags_hi` bit11 = `n & 1` | bit11（`0x800`） | `0x4618be` | ✅ |
| 312 | `scrollMode` / `scrollMode` | `$S x, $S y` | `flags_hi` bit0–1 = `x & 3`；`flags_lo` bit30–31 = `y & 3` | hi bit0–1 / lo bit30–31 | `0x46284a` | ✅ |
| 313 | `resolutionMode` / `resolutionMode` | `$S n` | `flags_hi` bit20–22 = `n & 7` | bit20–22（`0x700000`） | `0x4628b3` | ✅ |
| 314 | `attached` /（无） | `$S enable` | `flags_hi` bit23 = `enable & 1`；见 §3.7 | bit23（`0x800000`） | `0x4628ec` | ✅ |
| 315 | `colorizeChildren` / `colorizeChildren` | `b enable` | `flags_hi` bit25 = `enable & 1`；见 §3.8 | bit25（`0x2000000`） | `0x461359` | ✅ |
| 316 | `v8-flag-316` /（无） | — | `flags_lo \|= 2`（位 1）；见 §3.2 | bit1 | `0x461370` | ✅ |
| 317 | `v8-flag-317` /（无） | — | `flags_lo &= ~2`（清位 1）；见 §3.2 | bit1 清 | `0x46138b` | ✅ |

### 2.1 `300 sprite` 的完整数据流（不是「只改一个字段」）

**① 发现**：`0x460ba9`–`0x460c41` 逐条读，比「置位 + 换贴图」多两步：

```c
flags_lo |= 1;                                    // 0x460ba9  可见
idx = (param_mask & 1) ? get_int_var(vm, arg0) : arg0;      // 0x460bb9..0x460bf5
if (vm.on_sprite_set_idx /* +0x5dc */ != 0) {
    idx = ANM_ON_SPRITE_SET_CALLABLES[vm.on_sprite_set_idx](vm, idx);  // 0x460bd0..0x460bdf，返回值回填 idx
}
if (idx < 0) {                                     // 0x460bf8  ★ 负数走「系统贴图」分支
    AnmLoaded* sys = *(AnmLoaded**)(*(void**)0x4a6d98 + 0x19240);
    set_sprite(sys, vm, 0x102);                    // 0x460bfc..0x460c0d + 0x460c21
} else {
    AnmLoaded* anm = *(AnmLoaded**)(*(void**)0x4c0f48 + vm.anm_slot*4 + 0x184f4f0);
    set_sprite(anm, vm, idx);                      // 0x460c0f..0x460c21
}
vm[+0x494] = vm.timer.current;                     // 0x460c26..0x460c34  记录「上次 texSelect 时间」
vm.instr_offset += ins.size;                       // 0x460c3a..0x460c3e
```

**② 推测**：`+0x5dc` 是「sprite 映射回调」下标（thpages 对 `sprite` 的 `wip:2` 注记正是指它）；
负数 `id` 是「不画贴图」，由引擎代换成一个系统空贴图。

**③ 验证**：回调表 `0x491b1c` 直读 PE = `{0, 0x417140, 0x431fa0, 0x43a840, 0, 0x4073a0, 0x405ec0, 0x406860}`（7 个非空项）；
写 `+0x5dc` 的是敌人/激光侧（`0x4212f3`、`0x432745` 等）——即**弹/激光装上自己的贴图重映射回调**。
`set_sprite` `0x45edb0` 是 `__thiscall(AnmLoaded, vm, sprite_idx)`。`0x4a6d98` 直读发现它由
`AsciiManager__constructor` `0x407c57` 写、`FUN_00407e00` `0x407fd8` 清，即 **`0x4a6d98` = AsciiManager 单例指针**，
`+0x19240` 是它内嵌的 anm 归档（ascii.anm）的 `AnmLoaded`。

**④ 结论**（✅ TH16，结构层）：
1. `sprite` 置 `flags_lo` bit0（可见）+ 换贴图 + 记录 `+0x494` 时间戳，三件事。
2. **sprite 下标可被 `on_sprite_set` 回调（`+0x5dc`，表 `0x491b1c`）改写**——回调先于 `set_sprite`，
   返回值当新下标用。这就是 thpages 那句「unknown sprite-mapping function」的机制 ✅。
3. **`id < 0` 走「系统贴图」分支**：换成 **AsciiManager**（`0x4a6d98`）`+0x19240` 处的 anm 归档（ascii.anm）里的 **sprite `0x102`**。
   `0x4a6d98` 在 PE 里的初值是 `0xffffff21`（运行时才填 AsciiManager 指针，见 §5.2），`0x102` 是哪个贴图**未追** 🟡。
4. 真实数据里 `id` 恒字面量、恒非负（§6），所以回调分支与负数分支**在 ZUN 数据里一条都没走到**——这两条是理论路径 🟡。

**⑤ 证据**：`0x460ba9`–`0x460c41`；回调表 `0x491b1c`；`set_sprite` `0x45edb0`；`+0x5dc` 写点 `0x4212f3`。

### 2.2 `304 layer` 调 `set_layer`（不只是写 `+0x18`）

`0x462950` 把 `arg0` 的**低 8 位**（`movzx byte`）交给 `set_layer` `0x406d80`。`set_layer` 一手：

```c
vm[+0x18] = layer;                       // 0x406d86
if (3 <= layer <= 19)  flags_hi = (flags_hi & ~0x80000) | 0x40000;   // 0x406d8e..0x406da3
else if (20 <= layer <= 23) flags_hi = (flags_hi & ~0x40000) | 0x80000; // 0x406dab..0x406dc5
else flags_hi &= ~0xc0000;               // 0x406dcd
if ((20 <= layer <= 31) || (36 <= layer <= 42))                        // 0x406dd7..0x406de9
    flags_hi = (flags_hi & ~0x600000) | 0x100000;                      // 0x406deb..0x406dfb
```

所以 `layer` 一次写四个地方：`+0x18`（layer 字段）+ `flags_hi` 的 `0x40000`/`0x80000`（layer 区间 3–19 / 20–23，
承 `02-vm-core.md` §3.6）+ **`flags_hi` 位 `0x100000`**（layer 20–31 或 36–42，`02-vm-core.md` §3.6 **未收录**，本文补）。
这些派生位由渲染器按 layer 区间决定「画在哪个阶段 / 怎么缩放」，`layer` 本身只做分桶 ✅。

---

## 3. 专项① 每个 flag 位被谁读

**方法**：capstone 按 `functions.jsonl` 的函数边界逐函数反汇编全 `.text`，扫所有 `[reg+0x530]`/`[reg+0x534]`
访问点（`flags_lo` 217 处 / `flags_hi` 277 处），把「置位」opcode 与「读位」渲染器一一对上。读者是下面的关键几条：

### 3.1 `306` 置 `flags_lo` 位15（`0x8000`）→ 帧尾把全局滚动加进 `entity_pos`

**① 发现**：`306` case `0x461893` 用位域惯用法把 `flags_lo` bit15 设成 `arg0 & 1`。
**② 推测**：bit15（`0x8000`）是「帧尾跟随全局滚动」标志。
**③ 验证（读者一手）**：`AnmVm::run` 帧尾 `0x462a19`：

```
0x462a19  test dword [ebx+0x530], 0x8000
0x462a23  je   0x462a6d
0x462a25  movss xmm0, [ebx+0x5e0]        ; entity_pos.x
0x462a2d  addss xmm0, [0x4c1790]         ; += 全局滚动 DX
0x462a35  movss [ebx+0x5e0], xmm0
… 同款 `+0x5e4` += `0x4c1794`、`+0x5e8` += `0x4c1798` …
```

`entity_pos`（`+0x5e0`）会在 `0x40e490` 算最终位置时被加进 `pos`（[§3.7](#37-314-attached-置-flags_hi-位23--被父旋转带着走) 那一段同一函数里可见
`[edi+0x5e0]` 被加进输出），所以它是「位置相关向量」。
**④ 结论**（✅ TH16）：`306` = 「开启 `entity_pos` 每帧追全局滚动」。这与社区 `v8-306` 那句
「stage background camera data 加进 position-related vector」**对上**，且比它更具体：被加的是
`entity_pos`（`+0x5e0`），加的是滚动量 `0x4c1790/94/98`。**「滚动量 = 舞台相机滚动」这个定性沿用 `02-vm-core.md` §2 的命名**
（✅ 该文已标），本文没追 `0x4c1790` 是谁写的，故「相机」一词 🟡。社区标 `wip:2` 的 306 到此**一手到底** ✅。
**⑤ 证据**：`0x461893`（置位）、`0x462a19`（读者）；`0x4c1790/94/98`。

### 3.2 `316`/`317` 置/清 `flags_lo` 位1（`0x2`）→ 第二道绘制闸门

**① 发现**：`316` `0x461370` = `or [ebx+0x530], 2`；`317` `0x46138b` = `and [ebx+0x530], 0xfffffffd`。
**② 推测**：位 1 是某个「绘制相关」开关。
**③ 验证（读者一手）**：`AnmManager::draw_vm` `0x468490` 开头：

```
0x4684b5  mov eax, [esi+0x530]       ; flags_lo
0x4684bb  test al, 1                 ; bit0 可见
0x4684bd  jne 0x4684ca               ; 可见才继续
0x4684bf  or eax, -1; … ret          ; 不可见 → 直接返回 -1（不画）
0x4684ca  test al, 2                 ; bit1（316/317）
0x4684cc  je  0x4684bf               ; bit1 清 → 同样返回 -1（不画）
0x4684ce  test byte [esi+0x534], 0x60 ; 再查 flags_hi 的销毁位 0x20/0x40
```

**默认值**：`AnmVm::wipe` `0x409128` `mov eax, 7` → `0x409220` `mov word [ecx+0x530], ax`，
即 **`flags_lo` 低 16 位默认 = 7**（bit0/1/2 全置）。所以位 1 **默认是置位的（可画）**。
**④ 结论**（✅ TH16）：`316`/`317` 开关的是 `draw_vm` 里的**第二道绘制闸门** `0x4684ca`：
位 1 清 → VM 整条跳过（和不可见一样返回 -1）；位 1 置（默认）→ 继续画（除非 `flags_hi & 0x60` 销毁位）。
它区别于 bit0（`visible`）的仅是**置位方**：bit0 由 `300/301/310` 与 interrupt 管，bit1 由 `316/317` 与 GUI/菜单代码
（`GuiMsgVm__sub_42b610`、`FUN_0042b820` 等直接 `or/and [..+0x530], 2`）管。两者在 `draw_vm` 里是**串行的两个 `je` 闸门**，
语义上都是「不满足就不画」。社区 `wip:2` 到此**一手到底** ✅。
**⑤ 证据**：`0x461370`/`0x46138b`（置/清）；读者 `0x4684ca`；默认 `0x409128`/`0x409220`。

### 3.3 `307` 置 `flags_hi` 位10（`0x400`）——但全 exe **无读者**

**① 发现**：`307` `0x4629b0` 把 `flags_hi` bit10（`0x400`）设成 `arg0 & 1`（`movzx byte` 低 8 位）。
**② 推测**：bit10 曾是「选 PRNG 流」的标志（社区 `v4-randMode` 的残余）。
**③ 验证（穷举 + negative set）**：全 `.text` 逐函数扫，`flags_hi` 位 10 的**写点**有：`307`（`0x4629b0`）、
`AnmLoaded::create_effect` `0x4063d1`、`AnmLoaded::copy_vm_418fe0` `0x41901c`、`AnmLoaded::create_40e5c0` `0x40e60a`、
`EnemyData::step_interpolators` `0x41c058`、ECL 建 anm 侧 `0x41e0a8`/`0x41e187`/`0x41e374`、若干 GUI 侧。
**读点（`test/and/cmp` 带 `0x400` 且目标偏移是 `+0x534`）：0 条**——所有 `0x400` 的读都是对 `+0x530`（`flags_lo` bit10，
那是「位置改写 `+0x4e0`」flag，`02-vm-core.md` §3.6 已解，**别混**）。negative set：写点附近对 `+0x534` 的 `and`
（如 `0x419071` 的 `and …, 0xffff3fff`）清的是 `0xc000` 位，不是位 10 的读。
**④ 结论**（✅ TH16）：`307` 在 TH16 **只有写、没有读**——它置的 `flags_hi` bit10 是死位，PRNG 流选择
由 [`03-math-jumps.md`](03-math-jumps.md) §7.4 的硬编码 `mov ecx, 0x4a6d80/0x4a6d88` 决定。社区
`v8-randMode` 的 *"all of the code that uses the bitflag appears to be gone"* **被一手证实** ✅。
**⑤ 证据**：`0x4629b0`（写）；读点穷举见上（0 条）。

### 3.4 `305 noZBuffer` 位13 → 关 z-buffer 写

读者在 `Stage__sub_40af70` `0x40b200`（`test [esi+0x530], 0x2000`）：命中后 `call 0x465a80`（`SetRenderState`）
并把 `0x4c1ae0`（「z-write 已开」的全局缓存）清 0。另有 `AnmManager::draw_vm__mode_8` `0x46746c`、
`mode_X__textureArc3D` `0x467d5c` 两处同款读。→ `305` = 「关 z-buffer 写」，与社区 `zWriteDisable` 一致 ✅。
⚠️ 别与 `flags_hi` bit13（`0x2000` = 「帧尾重算 UV 四角」，读点 `0x462a6d`）搞混——**同名位、不同字**。

### 3.5 `303 blendMode` / `311 resampleMode` / `312 scrollMode` → 渲染状态

`AnmManager::setup_render_state_for_vm` `0x464f10` 是这批位的读者，逐条：

| opcode 置的位 | 读者 | 怎么用 |
| --- | --- | --- |
| `303` `flags_lo` bit5–8 | `0x464f1a` `mov eax,[edi+0x530]; shr 5; and 0xf` | 命中跳表 `0x465254` 选 D3D 混合态（SRCBLEND/DESTBLEND/BLENDOP） |
| `311` `flags_hi` bit11 | `0x465136` `mov eax,[edi+0x534]; shr 0xb; and 1` | 选采样过滤（0→线性 / 1→点，`SetSamplerState` `0x114`） |
| `312` x `flags_hi` bit0–1 | `0x46519d` `and eax,3` | `0→1(WRAP), 1→3(CLAMP), 2→2(MIRROR)` 映射后 `SetSamplerState`（U 轴） |
| `312` y `flags_lo` bit30–31 | `0x4651f1` `shr 0x1e; and 3` | 同上（V 轴） |

`303` 的 4 位（0–9 十个模式）对应 thpages `blendMode` 表；本文只钉「位 → 渲染状态」这一层，**D3D 常量映射沿用社区 🟡**
（没逐个反 D3D 枚举数值）。

### 3.6 `302 renderMode` 位25–29 / `313 resolutionMode` 位20–22 → 渲染路径与缩放

- `302`：读者 `AnmManager::draw_vm` `0x4684e1`（`shr 0x19; and 0x1f`）→ 按模式分派到各 `draw_vm__mode_N`；
  `AnmVm::sub_4632f0` `0x46330d` 也读（`02-vm-core.md` §3.6）。**特例**：case 体里 `0x4623fe` `cmp eax, 0x14000000`
  （= mode `10`）命中时 `call 0x469e20`（`02-vm-core.md` §3.5：写 `on_tick` 下标 4 = `0x46a0b0`）。
- `313`：读者 `AnmManager::draw_vm` `0x4687aa`（`and 0x700000`）→ 值 1/2 时把顶点坐标乘上 `0x4d9d34`
  （分辨率比例因子，BSS 运行时填）及 `0x49449c`（= `0x3f000000` = `0.5f`，PE 直读）——即「随分辨率缩放」，值 0 不缩放。

### 3.7 `314 attached` 置 `flags_hi` 位23 → 被父旋转带着走

**读者一手**：`0x40e490`（`anm_40e490_compute_final_pos`）里：

```
0x40e50a  mov eax, [edi+0x534]
0x40e510  test eax, 0x10000      ; 「不继承」→ 跳过父变换
0x40e515  jne 0x40e5ae
0x40e51b  test eax, 0x800000     ; attached（bit23）
0x40e520  je  0x40e570           ; 不 attached → 不加旋转变换
0x40e522  movss xmm0, [ebx+0x40] ; 父的 rotation.z
0x40e527  call 0x405510          ; sin(父 rotation.z)
0x40e537  call 0x4054f0          ; cos(父 rotation.z)
… (x,y) 绕父 z 轴旋转 …
0x40e570  … 递归算父的最终位置并相加 …
```

**④ 结论**（✅ TH16）：`314` = 「在叠加父位置**之前**，先把本 VM 的局部位置绕**父的 z 旋转**转一下」，
与社区 *"the graphic will move as its parent rotates"* 一致。**措辞**：转的是「本 VM 的局部位置」，
用的是「父（`+0x5ac`，即根 VM）的 `rotation.z`」，沿 `+0x5ac` 的递归链（与 `10026` 同款拓扑，见
[`04-variables.md`](04-variables.md) §6.2）。真实数据里 `314` 只出现 1 次（§6），这条是**理论上完整、实际几乎没用**的指令 ✅。

### 3.8 `315 colorizeChildren` 位25 → 子贴图颜色被父色调制

**读者一手**：`AnmManager::render_sprite_2d` `0x465725`：

```
0x465725  test [edi+0x534], 0x2000000   ; colorizeChildren
0x46572f  mov  [ebp+0xc], eax           ; 本 VM 颜色（+0x520 或 +0x524，按 bit17–18 选）
0x465732  je   0x4657f0                 ; 未置 → 跳过
0x465738  mov  edx, [edi+0x5b0]         ; 父 VM
0x465746  mov  edx, [edx+0x528]         ; 父的「混合继承色」
… 把本 VM 颜色 × 父色（按 16 位分量乘再右移）…
```

→ `315` = 「子图颜色乘父的继承色（`+0x528`）」。社区 *"colorization affects children"* 一致 ✅。
`+0x528` 是 `02-vm-core.md` §3.3 的「混合后的继承色」（🟡，语义未坐实）——本文把它在「子色调制」里的**读法**补上，但「父色从哪来」仍 🟡。

---

## 4. 专项② 参数宽度与「字面量 / 变量」两族

**① 发现**：18 条的取参指令分三族（判据 = case 体第一处 `[edi+8]` 访问的形态）：

| 族 | 判据 | opcode |
| --- | --- | --- |
| **字面量 32 位**（永不查 `param_mask`） | `mov eax, [edi+8]`，全程无 `test byte [edi+6]` | `302 303 305 306 310 311` |
| **字面量低 8 位**（`movzx byte`） | `movzx eax, byte [edi+8]` | `304 307 315` |
| **可为变量**（查 `param_mask` → `get_int_var`） | `test byte [edi+6], 1<<i` 后 `call 0x45f610` | `300 301 312 313 314` |
| 无参数 | — | `308 309 316 317` |

**② 推测**：只有第三族会真的把 `param_mask` 位当「变量引用」；第一、二族即使脚本写了 `param_mask`，handler 也不理。
**③ 验证（对抗证伪）**：真实数据 54 个文件里本组 10587 条指令，**`param_mask` 非 0 的条数 = 0**（§6）。
即：即便 `300/301/312/313/314` 在机器码层支持变量参数，ZUN 自己的数据**一次都没用**——「可为变量」是**能力**，
不是「被用过」。`304/307/315` 读低 8 位：真实数据里 `layer` 值全部 < 256、`307`/`315` 恒 1，**无一被截断**（§6）。
**④ 结论**（✅ TH16）：
1. **`302/303/305/306/310/311` 的参数永远不可能引用变量**——写反汇编器/播放器时，这几条直接当字面量，别生成 `$`。
2. **`304/307/315` 只取低 8 位**：传 `> 255` 会被 `movzx` 静默截断（真实数据无此情形，理论边界 🟡）。
3. **`300/301/312/313/314` 可为变量**（`get_int_var` 族），但 ground truth 里恒字面量——mod 作者可以用变量，
   **但没有游戏数据可对**，播放器要按 `get_int_var` 语义实现（整型变量号，[`04-variables.md`](04-variables.md)）。
**⑤ 证据**：上表判据地址（代表）：`302` `0x4623dc`、`304` `0x462950`、`312` `0x46284a`；统计见 §6。

---

## 5. 真实数据对账（54 个 `.anm`）

自写解析器（[`01-format.md`](01-format.md) §3–§5 布局 + `parseAnm`）跑 `local/th16.v1.00a/dat/*.anm`：
54 文件 / 1929 script / **41339 条指令**（不含每 script 一条的 `-1` 结束标记，与 `runtime/anm/OVERVIEW.md` 的 41339 吻合），
其中本组 **10587 条**：

| 检查项 | 结果 |
| --- | --- |
| 本组 18 条的 `size` 是否 = `8 + 4 × 参数个数` | **10587 / 10587 吻合**（argcount 分布见下） |
| 本组 `param_mask` 非 0 的条数 | **0 / 10587**（即「可为变量」的 5 条也全用字面量） |
| `304 layer` 的 `arg0` 是否全 < 256（不被 `movzx` 截断） | **是**（最大 42，见下） |
| `307`/`315` 的 `arg0` 是否恒 1 | **是**（`307`×546、`315`×8，全 = 1） |
| `300 sprite` 的 `id` 是否恒非负（不走「系统贴图」分支） | **是**（0 次负数，见 §2.1） |

**使用分布**（合计 10587 条，按出现次数排）：

```
300×4683 · 304×2057 · 313×1207 · 302×1109 · 307×546 · 303×520 · 311×339 · 308×39 ·
305×34 · 312×22 · 306×14 · 301×8 · 315×8 · 314×1 · 309×0 · 310×0 · 316×0 · 317×0
```

**一次都没出现的 4 条**：`309 flipY`、`310 visible`、`316 v8-flag-316`、`317 v8-flag-317`。
它们实现完整（每条有独立 handler），只是 ZUN 自家 anm 没用——**没有 ground truth**，播放器要按本文语义实现，别靠「跑一遍」。

**关键取值的分布（证伪/佐证用）**：

- `302 renderMode` 用到的模式：`1×574, 0×291, 8×196, 2×26, 3×12, 4×9, 21×1`（模式 10 一次都没有 → `0x469e20` 分支 ZUN 没走到）。
- `303 blendMode`：`1×440, 2×43, 0×16, 3×12, 4×6, 9×2, 7×1`。
- `304 layer`：`26×363, 29×233, 25×191, 20×129, 30×127, 8×124, 21×119, 13×79, 22×79, 6×75, 7×74, 27×74 …`（宽分布，全部 < 256）。
- `312 scrollMode`：`x∈{2×14, 0×8}`、`y∈{0×14, 1×8}`。
- `313 resolutionMode`：`2×1128, 3×42, 1×36, 0×1`。
- `301 spriteRand`：`a∈{120,128,136,144}`（各 2）、`b=8`（恒）→ `a + raw%8`。
- `300 sprite` 的 `id` 顶部：`1×306, 0×208, 2×159, 3×135, 540..547 各×104`（后 8 个是「假下标」，靠 `on_sprite_set` 回调重映射——见 §2.1）。

### 5.1 从数据侧证伪「`307 randMode` 影响 RNG」

如果 `307` 真能切 PRNG 流，ZUN 的数据会在「用 Stream A 变量的脚本」与「用 Stream B 变量的脚本」之间看到 `307` 的分野。
实测 **`307` 出现 546 次且 `arg0` 恒 = 1**，而 [`03-math-jumps.md`](03-math-jumps.md) §7.4 已证变量 `10030–10032` 走 Stream A、
`10010–10012/10022` 走 Stream B **与任何 flag 无关**（硬编码 `mov ecx`）。所以 `307` 的「`mode` 参数」在 TH16 数据里
**没有可观察效果**，与 §3.3 的「无读者」互为印证 ✅。

### 5.2 「系统贴图」指针 `0x4a6d98` = AsciiManager 单例

`0x4a6d98` 在 PE 里的初值是 `0xffffff21`（`pe_read.py` 直读），即**运行时才被填成对象指针**（`-223` 不可能是静态指针）。
写它的是 `AsciiManager__constructor` `0x407c57`（`mov [0x4a6d98], esi`）、清它的是 `FUN_00407e00` `0x407fd8`，
大量 `*__on_draw`（`Bomb__on_draw`、`Spellcard__on_draw`、`FpsCounter__on_draw` 等）读它画字 —— 即
**`0x4a6d98` = AsciiManager 单例指针** ✅。所以「负数 sprite」路径 = 从 ascii.anm 归档（`AsciiManager+0x19240`）取
sprite `0x102`，`0x102` 具体是哪张贴图**未追** 🟡。

---

## 6. 与社区不一致处的裁决

对账对象：thpages `local/vendor/thpages/js/tables/reference/anm.ts`（`byRefId` 描述）与 `v8.anmm`（thtk 风格 anmmap）。
逐条比对后，**签名与语义 15 条全部一致** ✅；分歧与「我们补」如下：

| # | 条目 | 社区 | 一手 | 裁决 |
| --- | --- | --- | --- | --- |
| 1 | `306`（`wip:2`） | 「camera data 加进 position-related vector，目的不明」 | 置 `flags_lo` bit15，读者 `0x462a19` = `entity_pos += 全局滚动` | **社区方向对、欠具体**；本文钉死位 + 读者 ✅ |
| 2 | `316`/`317`（`wip:2`） | 「置/清 unknown bitflag」 | 置/清 `flags_lo` bit1，读者 `0x4684ca` = 第二道绘制闸门 | **社区只有「有位」**；本文钉死位 + 读者 ✅ |
| 3 | `307`（`wip:1`） | 「completely ineffectual leftover」 | 置 `flags_hi` bit10，全 exe 无读者 | **社区对，本文升为 ✅**（穷举 + negative set） |
| 4 | `310 visible` 的 `sig` | `'b'` | `mov eax, [edi+8]`（**32 位**字面量，非 byte），`and eax,1` | 行为等价（都只看 bit0）；**宽度**是社区记错了 ✅，见 §4 |
| 5 | `315 colorizeChildren` 的 `sig` | `'b'` | `movzx eax, byte [edi+8]`（**低 8 位**） | 社区对（确实是 byte） ✅ |
| 6 | `304 layer` | 「sets the layer」 | 还派生 `flags_hi` 位 `0x40000/0x80000/0x100000`（§2.2） | 社区漏了派生位；本文补 ✅ |
| 7 | `302 renderMode` 的 `type` 别名 | `v8.anmm` 记 `type` | case 置 `flags_lo` bit25–29，与 `02-vm-core.md` §3.6 一致 | 别名只是 thanm 命名，语义一致 ✅ |
| 8 | `300 sprite` 的「sprite-mapping」（`wip:2`） | 「机制不明」 | `on_sprite_set` 回调（`+0x5dc`，表 `0x491b1c`）改写下标 | **本文补上机制** ✅；回调内部逐条语义归 ECL/渲染文档，🟡 |

> **可信度口径**：上表「社区」列都是单源 🟡；「一手」列的位/读者/字段是 ✅（每条指得到机器指令）。
> 「位含义」里凡是读者没追到的（`0x4c1790` 的写方、`+0x528` 父色的来源、D3D 枚举数值、`0x102` 贴图）一律 🟡 标在 §3/§5。

---

## 7. 写播放器会踩的坑

| 坑 | 说明 |
| --- | --- |
| **「按值写位」不是 toggle** | `302/303/305/306/310/311/313/314/315` 用 `dst ^= (src^dst)&mask`，等价于「把 mask 位设成 src 的同位」，**不是异或翻转**（§1.1） |
| **`308/309` 才是真 toggle** | `flipX/flipY` 用 `xor [..+0x530], 0x800/0x1000`，且同时 `scale = -scale`——连续调两次 = 还原 |
| **参数宽度三族** | `304/307/315` 低 8 位、`302/303/305/306/310/311` 字面量 32 位、`300/301/312/313/314` 可为变量（§4）。`304 layer` 传 >255 会截断 |
| **`307` 是死位** | 别拿它实现「切 PRNG 流」——TH16 里它没有读者，流选择是硬编码的（§3.3）。播放器可以忽略它，但要记录 |
| **`316`/`317` 与 `310` 是两道不同的闸门** | `draw_vm` 里 bit0 与 bit1 是两个串行 `je`；bit1 默认置位（wipe 低 16 位 = 7）。只实现 `310 visible` 而不实现 `316/317`，会在 mod 用 317 清位时画错 |
| **`300`/`301` 会隐式置「可见」** | `flags_lo \|= 1`。播放器若把「可见」与「换贴图」分两个状态，`sprite(id)` 后要记得补上可见位 |
| **`301` 的取模是无符号、无零守卫** | `div`（u32 除），`b==0` 崩（#DE）；`b<0` 按 u32 解释成巨大模数 → 结果几乎恒 = `a`（§2）。与 `122 rand` 同款边界，[`03-math-jumps.md`](03-math-jumps.md) §4.3 |
| **`300` 的负数 sprite 是「系统贴图」不是「不画」** | `id<0` 走 `0x4a6d98+0x19240` 的 sprite `0x102`（§2.1）。真「不画」靠 `310 visible(0)` / `317` / `4 stopHide` |
| **`+0x494` 是 texSelect 时间戳** | `300` 每次都写 `vm[+0x494] = timer.current`（§2.1）。TH16 里只写不读（`02-vm-core.md` §3.1），但复刻 VM 状态要保留它 |
| **`302` 的 mode 10 有副作用** | 命中 mode 10 → `call 0x469e20` 写 `on_tick` 回调下标 4（§3.6）。播放器只存 5 位模式、不复刻这个回调，会在 mode 10 上跑偏 |

---

## 8. 还没搞清楚的

| 问题 | 状态 |
| --- | :---: |
| `300` 负数 sprite 的「系统贴图」= ascii.anm 归档（`0x4a6d98` = AsciiManager）里的 sprite `0x102`，但 `0x102` 具体是哪张图 / `+0x19240` 的确切字段名 | 🟡 |
| `306` 的「全局滚动」`0x4c1790/94/98` 是谁写的（定性沿用 `02-vm-core.md` 的「相机滚动」，未追写方） | 🟡 |
| `315` 调制用的父色 `+0x528`（「混合继承色」）从哪来 | 🟡 |
| `300/301` 的 `on_sprite_set` 回调（表 `0x491b1c` 的 7 个非空项）各自怎么重映射 sprite 下标 | 🟡（归 ECL/渲染文档） |
| `303 blendMode` 0–9 十个模式对应的精确 D3D 混合常量（本文只钉「位 → 渲染状态」，枚举数值沿用社区） | 🟡 |
| 本组在 TH18 的差异（本文只做 TH16） | — 阶段 3 |

---

## 关联

- 指令头 / `param_mask` / 分发表骨架：[`01-format.md`](01-format.md) §5、[`02-vm-core.md`](02-vm-core.md) §7
- VM 字段图（`flags_lo`/`flags_hi` 基础位、`entity_pos` `+0x5e0`、`on_sprite_set` `+0x5dc`）：[`02-vm-core.md`](02-vm-core.md) §3
- 变量号 / 整型变量编码（`300/301/312/313/314` 的 `get_int_var` 参数）：[`04-variables.md`](04-variables.md)
- 随机流划界（`301` 用 Stream B、`307` 与流无关）：[`03-math-jumps.md`](03-math-jumps.md) §7.4
- 子系统总览：[`../OVERVIEW.md`](../OVERVIEW.md)
- 本作入口：[`README.md`](README.md)
- 想要的 Ghidra 重命名（待 T10 统一落库）：[`naming-batch.md`](naming-batch.md)
- 纪律：[`../../../METHOD.md`](../../../METHOD.md)、[`../../../DOCSTYLE.md`](../../../DOCSTYLE.md)
