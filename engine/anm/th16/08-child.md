# ANM v8 Child management 组 — TH16 一手逐条确认（opcode 500–509，10 条）

> **版本**：TH16 v1.00a（`th16.exe`，imagebase `0x400000`）。本文裸地址默认属该版本；引用其他版本须写成 `th18:0x…`。
> **方法**：不经 Ghidra 渲染——直读 PE 解高段分发表（`0x462fa8` 索引 + `0x462e64` 跳表）自证「opcode → case 地址」，
> 逐条反汇编 10 个 case 体与它们共用的 2 个 helper（`create_managed_child` / `create_managed_root`）、
> 4 个列表插入函数、`EffectManager::create_effect`；再用自写解析器跑 `local/th16.v1.00a/dat/` 的 **54 个真实 `.anm`
> （1929 script / 41339 指令，其中本组 1293 条）**做对账与证伪。
> **可信度**：10 条的签名与数据流**全部 ✅**（每条断言背后是一条读/写该字段的机器指令）；
> `508 createEffect` 的「被调 callback 各自干什么」是 🟡（只核了 kind 1 的 callback，见 [§6](#6-与社区不一致处的裁决)）。
> 与社区（thpages）**2 处不一致**，见 [§6](#6-与社区不一致处的裁决)。
> **衔接**：指令头 8 字节与 `param_mask` 见 [`01-format.md`](01-format.md) §5；VM 主循环、`zAnmVm` 字段图、
> 终止语义（父死子树一并销毁）见 [`02-vm-core.md`](02-vm-core.md)；变量号表见 [`04-variables.md`](04-variables.md)。
> 本文不重复这些结论。
> **不含版权字节**：只写偏移、地址与语义。

## 0. 一句话结论

这 10 条是 ANM VM 的**树与列表管理层**：7 条（500–506）创建新 VM 并决定它进哪张渲染列表、
挂到哪棵父子里，2 条（507 / 509）改当前 VM 的继承开关 / 拷父变量，1 条（508）按硬编码 effect 表
重配当前 VM。四件容易搞错的事，一手结论是：

1. **`arg0`（脚本号）恒为字面量**：500–506 的 `arg0` 是「**相对当前 VM 所属 `.anm` 文件的脚本运行序号**」
   （索引 `AnmLoaded+0x10c` 模板数组），真实数据 100% 字面量（`param_mask` bit0 从没置位）；
   `505/506` 的 `x`/`y` 参数**可以是变量**（float32 编码，真实数据里出现过 `F0`/`F1`/`F2`）。
2. **createChild 族（500/501/502/503/505）`entity_pos` 清 0、继承父的 layer 与 root**；
   **createRoot 族（504/506）无父、把源 VM 的 `rotation` 与 `entity_pos` 整块拷走、`pos` 拷进 `pos_2`**（[§3](#3-专项①-父子坐标继承与-createchild--createroot-的分野)）。
3. **「world 列表」与「UI 列表」是两张独立渲染链表**（`mgr+0xdc` / `mgr+0xe4`），
   create 挂尾部、prepend 挂头部；**子链（父 +0x588）则永远 prepend**，与 create/prepend 无关（[§5](#5-专项③-world-列表与-ui-列表)）。
4. **`509 copyParentVars` 只拷 64 字节「寄存器/随机参数块」（+0x4a0..0x4df），不拷 `pos`/`rotation`**；
   `507 ignoreParent` 是**设值**不是翻转（`flags_hi bit 0x10000 = arg0 & 1`）（[§4](#4-专项②-ignoreparent-与-copyparentvars)）。

---

## 1. 怎么核的：opcode → case 地址是自己解跳表得到的

**① 发现**：`02-vm-core.md` §7 已定死高段分发（`301 ≤ op ≤ 613`）：`SUB EAX,0x12d; CMP EAX,0x138; JA 默认`，
索引表 `0x462fa8`（313 字节，下标 = opcode − 301）+ 跳表 `0x462e64`（81 项）。

**② 推测**：本组 500–509 全落高段（下标 199–208），case 地址 = `0x462e64[0x462fa8[op−301]]`。

**③ 验证**：直读 PE 解两张表。`0x462fa8[199..208]` = `38 39 3a 3b 3c 3d 3e 3f 40 41`（字节，即跳表下标 56–65）；
`0x462e64[56..65]` = `0x460c9d 0x460dc8 0x460d95 0x460dfb 0x460ec3 0x460ce7 0x460e88 0x462811 0x460f0c 0x460e2e`。

**④ 结论**（✅ TH16）：10 个 case 地址如下表，全部一手，不依赖社区 label。

**⑤ 证据**：高段分发序言 `0x460c46`–`0x460c5d`（`SUB EAX,0x12d; CMP EAX,0x138; MOVZX [EAX+0x462fa8]; JMP [EAX*4+0x462e64]`）；
表 `0x462fa8`（`u8[313]`）与 `0x462e64`（`void*[81]`）。

### 1.1 每条 case 的公共骨架与两个 helper

10 条 handler 都遵循 `03-math-jumps.md` §1.1 的骨架：`EBX = zAnmVm*`（当前 VM）、`EDI = 当前指令指针`、
`arg i = *(u32*)(EDI + 8 + 4*i)`、`param_mask bit i` 管 `arg i`。参数取值方式：

- `$`（整型参数）：`test byte [edi+6], 1<<i` → 置位走 `get_int_var` `0x45f610`，否则取字面量；
- `%`（浮点参数）：`test byte [edi+6], 1<<i` → 置位走 `get_float_var` `0x45f2d0`，否则取字面量。

两个创建 helper（都是 `AnmLoaded::` 成员，`ECX = AnmLoaded*`，4 个栈参数，`RET 0x10`）：

| helper | 地址 | 签名（thiscall） | 干什么 |
| --- | --- | --- | --- |
| `create_managed_child` | `0x46ed60` | `(AnmLoaded*, void* out, int script, zAnmVm* parent, int mode)` | 建子 VM：`entity_pos=0`、`parent`/`root`/`layer` 继承、进列表、挂子链 |
| `create_managed_root` | `0x46eea0` | `(AnmLoaded*, void* out, int script, zAnmVm* source, int unused)` | 建根 VM：拷 source 的 transform、进 world 列表、无父；**第 4 参忽略** |

**`AnmLoaded` 取自当前 VM 自己的 anm 槽**：两个 case 里都是
`ECX = [0x4c0f48]`（`ANM_MANAGER_PTR`）→ `EAX = [EBX+0x1c]`（`anm_slot`）→ `ECX = [ECX+EAX*4+0x184f4f0]`（`AnmLoaded*`）。
⇒ **createChild/createRoot 只能在当前 VM 所属的那个 `.anm` 文件里选脚本** ✅。

**创建即 tick**：两个 helper 都在插列表**之前**各调一次 `AnmVm::run` `0x45f980`
（`0x46ee00` / `0x46ef4e`），即新 VM 一出生就把自己脚本里 `time <= 0` 的段跑掉
（与 [`02-vm-core.md`](02-vm-core.md) §8 一致）。

---

## 2. ★ 10 条指令表

**记号**：`$x` = 整型参数（变量号 int32 编码，右值）· `%x` = 浮点参数（float32 编码，右值）。
`arg0` = 脚本运行序号（literal，见 §0.1）。`child.pos_2` = 子 VM `+0x4e0..0x4e8`（`02-vm-core.md` §3.3 的 `pos_2`）。

| opcode | 助记（thpages / thanm） | 签名 | 语义 | case 地址 | 可信度 |
| :---: | --- | --- | --- | --- | :---: |
| 500 | `createChild` | `$script` | 建子 VM，进 **world 尾部**，父 = 当前 | `0x460c9d` | ✅ |
| 501 | `createChildUi` | `$script` | 建子 VM，进 **UI 尾部**，父 = 当前 | `0x460dc8` | ✅ |
| 502 | `prependChild` | `$script` | 建子 VM，进 **world 头部**，父 = 当前 | `0x460d95` | ✅ |
| 503 | `prependChildUi` | `$script` | 建子 VM，进 **UI 头部**，父 = 当前 | `0x460dfb` | ✅ |
| 504 | `createRoot` | `$script` | 建根 VM（无父），进 world 尾部，拷源 transform | `0x460ec3` | ✅ |
| 505 | `createChildPos` | `$script, %x, %y` | 500 + 把子 `pos_2.x/y` 写成 `x`/`y` | `0x460ce7` | ✅ |
| 506 | `createRootPos` | `$script, %x, %y` | 504 + 把新根 `pos_2.x/y` 写成 `x`/`y` | `0x460e88` | ✅ |
| 507 | `ignoreParent` | `$n` | `flags_hi bit 0x10000 = n & 1` | `0x462811` | ✅ |
| 508 | `createEffect` | `$kind` | 按 `0x4a2250` effect 表重配**当前 VM** | `0x460f0c` | ✅ |
| 509 | `copyParentVars` | ∅（无参） | 从父拷 `+0x4a0..0x4df` 64 字节变量块 | `0x460e2e` | ✅ |

### 2.1 500–503 的四条只差一个「mode」立即数

**① 发现**：四条 case 体形状一致，都只做「解析 `arg0` → `create_managed_child(out, arg0, EBX, mode)`」，
唯一区别是压栈的第 4 参（mode）：

| opcode | 助记 | mode（第 4 参） | 落点 |
| :---: | --- | :---: | --- |
| 500 | `createChild` | `0` | `0x460cb2` `push 0` |
| 502 | `prependChild` | `2` | `0x460da5` `push 2` |
| 501 | `createChildUi` | `4` | `0x460dd8` `push 4` |
| 503 | `prependChildUi` | `6` | `0x460e0b` `push 6` |

**② 推测**：mode 编码「哪张列表 + 头还是尾」。

**③ 验证**：`create_managed_child` `0x46ee05`–`0x46ee44` 读第 4 参（`[ebp+0x14]`），
`and eax,6; cmp al,6` 后按位分派到四个函数（都是 `AnmManager::` 成员）：

| mode | 分派条件 | 函数 | ExpHP 名 |
| :---: | --- | --- | --- |
| 0 | 落到 `test cl,4`/`test cl,2` 都不中 | `0x46e7d0` | `insert_in_world_list_back` |
| 2 | `test cl,2` 中 | `0x46e890` | `insert_in_world_list_front` |
| 4 | `test cl,4` 中 | `0x46e940` | `insert_in_ui_list_back` |
| 6 | `cmp al,6` 相等 | `0x46ea00` | `insert_in_ui_list_front` |

四个函数逐条读：`0x46e7d0`/`0x46e940` 把节点接在列表尾（`mgr+0xe0`/`+0xe8` 之后），
`0x46e890`/`0x46ea00` 把节点插到列表头（改写 `mgr+0xdc`/`+0xe4`）。**与 ExpHP 命名逐条吻合，0 处不符** ✅。

**④ 结论**（✅ TH16）：mode 的 **bit2（=4）判列表**（0=world / 1=UI）、**bit1（=2）判位置**（0=尾 / 1=头），bit0 不用。
`500`=world 尾、`502`=world 头、`501`=UI 尾、`503`=UI 头。**UI 列表 ≠ 子链**——见 [§5](#5-专项③-world-列表与-ui-列表)。

**⑤ 证据**：`0x460c9d`/`0x460d95`/`0x460dc8`/`0x460dfb`；`create_managed_child` `0x46ee05`–`0x46ee49`；
四函数 `0x46e7d0`/`0x46e890`/`0x46e940`/`0x46ea00`（列表头/尾 = `mgr+0xdc`/`+0xe0`/`+0xe4`/`+0xe8`）。

### 2.2 `505/506` 的尾巴：把手柄解回 VM 指针再写 `pos_2`

**① 发现**：`505` 调完 `create_managed_child` 后，`LEA ECX,[esp+0x54]`（out 手柄地址）→
`call 0x46f2e0`（ExpHP 名 `anm_find_existing_or_clear_id`）→ 返回 `EAX` = 子 VM 指针 → `MOV ESI,EAX`，
然后写 `[ESI+0x4e0] = x`、`[ESI+0x4e4] = y`。`506` 同样接在 `create_managed_root` 之后。

**② 推测**：`0x46f2e0` 把手柄解析回 VM 指针；`+0x4e0/+0x4e4` 是 `pos_2.x/y`。

**③ 验证**：`0x46f2e0` = `mov esi,ecx; mov ecx,[0x4c0f48]; push [esi]; call 0x46efa0`（`get_vm_with_id`），
若返回空则把 `*esi` 清 0。`+0x4e0` = `pos_2` 由 `02-vm-core.md` §3.3 一手坐实。**只写 `.x/.y`，不写 `.z`（+0x4e8）** ✅。

**④ 结论**（✅ TH16）：`505/506` = 创建 + 把新 VM 的 `pos_2.x/y` 设成 `x`/`y`。
**注意写入时机在 `AnmVm::run`（创建即 tick）之后**——子 VM 的 `time<=0` 脚本看到的是模板的 `pos_2`，看不到这次传参。
且**写的是 `pos_2`（+0x4e0），不是 `pos`（+0x2c）**——`pos` 仍是模板值（见 [§3](#3-专项①-父子坐标继承与-createchild--createroot-的分野)）。

**⑤ 证据**：`0x460ce7`/`0x460e88` 的 `0x460d1e`/`0x460d3e`/`0x460d60` 段；`0x46f2e0`；`0x46efa0`。

---

## 3. 专项① 父子坐标继承与 createChild / createRoot 的分野

**① 发现**：两个 helper 对新 VM 的字段写入，逐条对出来是这样（`02-vm-core.md` §8.1 已列过，这里按 opcode 视角重说）：

| 目标字段 | `create_managed_child`（500–503/505） | `create_managed_root`（504/506） |
| --- | --- | --- |
| `flags_hi \| 0x400`（托管） | 置 | 置 |
| `layer` `+0x18` | ← 父的 layer | ← 源的 layer |
| `entity_pos` `+0x5e0` | **清 0**（`xorps`+写 0） | ← 源的 entity_pos |
| `rotation` `+0x38` | 不写（模板值） | ← 源的 rotation |
| `pos_2` `+0x4e0` | 不写（模板值；505 随后写 .x/.y） | ← 源的 **`pos` `+0x2c`** |
| `parent` `+0x5b0` | ← 当前 VM | 不写（= 0，自己是根） |
| `root` `+0x5ac` | ← 父的 root ?: 父 | 不写（= 0，自己是根） |
| `flags_hi bit 0x2000000` | 从父同位复制 | 从源同位复制 |

**② 推测**：child 的坐标靠「父指针 + 模板局部值」在渲染期组合；root 因为无父，必须当场把源的变换拷过来续接画面。

**③ 验证**：
- child 侧逐条：`0x46edb5`（layer←父）、`0x46edba`/`0x46edc3`（entity_pos=0）、`0x46ede1`（parent=父）、
  `0x46edf8`（root = 父的 root ?: 父）、`0x46eddc`/`0x46ede7`（bit `0x2000000` 同位）。
- root 侧逐条：`0x46ef0a`/`0x46ef12`/`0x46ef1a`/`0x46ef20`（entity_pos←源）、`0x46ef26`–`0x46ef35`（rotation←源）、
  `0x46ef38`/`0x46ef3d`/`0x46ef45`/`0x46ef48`（**pos_2 ← 源的 pos**，`MOVQ XMM0,[EDX+0x2c]; MOVQ [ESI+0x4e0],XMM0`）。

**④ 结论**（✅ TH16）：

1. **child 不继承任何变换值**——`entity_pos` 清 0、`pos`/`rotation`/`scale` 全来自模板；
   它靠 `+0x5b0`（父指针）与 `+0x5ac`（root）在渲染期挂到父的坐标系。**精确的「父变换怎么叠加」归渲染文档**，
   本文只坐实「child 出生时除了 layer / root / bit `0x2000000` 之外不拷父的变换字段」。
2. **root 是「续接」**：把源的 `rotation`、`entity_pos` 整块拷走，把源的 `pos` 搬进自己的 `pos_2`——这就是 thpages
   `createRoot` 伪代码里 `pos_2 = this.pos` 的一手出处。**它不碰 `pos`（+0x2c）**，所以新根的 `pos` 仍是模板值
   （`time<=0` 段跑完后的值），thpages 那句 `new_vm.pos = {0,0,0}` 是「通常成立」的近似，**不是硬保证**。
3. **坐标继承的开关是 `507 ignoreParent`**：`flags_hi bit 0x10000` = 「不向父/根继承（减速、缩放、有效旋转）」
   （`02-vm-core.md` §3.6 / `04-variables.md` §6.2 的 10026 有效旋转累积走 `+0x5ac` 且受此位闸门）。

**⑤ 证据**：上表每格的地址；`0x46ed60`/`0x46eea0` 全函数反汇编（`state/f/0046ed60.html`、`0046eea0.html`）。

---

## 4. 专项② `ignoreParent` 与 `copyParentVars`

> ★ **2026-09-10 审阅补：本节的表与 [`02-vm-core.md`](02-vm-core.md) §8.1 内容重复**，是把 02 已经一手
> 坐实的字段表按 opcode 视角重排了一遍——这与本文开头「衔接…本文不重复这些结论」自相矛盾。
> 更要紧的是**重排过程中丢了 02 §8.1 点名的一条写入时序陷阱**：`flags_hi | 0x400` 与 `entity_pos`
> 清零相对 `copy_vm` / `wipe_suffix` 的**先后顺序**在 root 路径与 child 路径上是不同的
> （root 在 `copy_vm` 之后 `th16:0x46eedf`、child 在之前 `th16:0x46eda8`）。只读本文会漏掉这个雷，
> 必须回 02 §8.1 补。本文不再重述，只在此挂一条指路。

### 4.1 `507 ignoreParent` 是「设值」，不是翻转

**① 发现**：case 体（`0x462811`–`0x462845`）收尾 `JMP 0x4629c2`（共用的「设 flag」尾巴），
语义上是一段位运算——但★ **控制流是两条独立路径而不是一段**（2026-10 审阅补，逐字节复核）：
`0x462811` `test byte [edi+6],1` 分叉，变量侧 `0x462817`–`0x46282f`（`call 0x45f610` 取变量后
`shl eax,0x10` / `xor [ebx+0x534]` / `and 0x10000`）、字面量侧 `0x462834`–`0x462845`（同样三步），
**两侧各算一遍**再一起 `jmp 0x4629c2` 汇合。下面的伪代码是把两条路径化简成一条后的等价写法：

```
eax = resolve(arg0); eax <<= 16; eax ^= flags_hi; eax &= 0x10000; flags_hi ^= eax;
```

**② 推测**：这是 MSVC 的 `flags = (flags & ~mask) | ((value << shift) & mask)` 惯用形，即**设值**。

**③ 验证**（代入穷举）：`arg0 & 1 == 0` → `flags_hi bit 0x10000 = 0`；`arg0 & 1 == 1` → `= 1`。
两者都不依赖旧值 ⇒ 是设值，不是 `toggle`。真实数据 1 次 `507`，`arg0 = 0`（即「**不** ignore parent」）。

**④ 结论**（✅ TH16）：`ignoreParent(n)` 令 `flags_hi bit 0x10000 = (n & 1)`。
与 thpages *"Sets a bitflag. If the bitflag is 1, …"* 一致。

**⑤ 证据**：`0x462811`–`0x462845`；共用尾巴 `0x4629c2`（`XOR [EBX+0x534],EAX`）。

### 4.2 `509 copyParentVars` 只拷 64 字节「寄存器/随机参数块」

**① 发现**：case 体（`0x460e2e`–`0x460e83`）先 `EAX = [EBX+0x5b0]`（父指针），
**父为空 → `JE 0x4629c8`（直接当 no-op）**；否则 4 条 `MOVUPS` 把父 `+0x4a0..0x4df` 整块拷到自身：

```
[EBX+0x4a0]←[父+0x4a0]   [EBX+0x4b0]←[父+0x4b0]   [EBX+0x4c0]←[父+0x4c0]   [EBX+0x4d0]←[父+0x4d0]
```

**② 推测**：拷的是「变量块」，不含 `pos`/`rotation`。

**③ 验证**：按 `04-variables.md` §2 的字段落点展开这 64 字节：

| 字节区间 | 覆盖的变量号（社区名） |
| --- | --- |
| `+0x4a0..0x4af` | 10000–10003 = `i0 i1 i2 i3` |
| `+0x4b0..0x4bf` | 10004–10007 = `f0 f1 f2 f3` |
| `+0x4c0..0x4cf` | 10033/10034/10035 = `mystery-angle-x/y/z` + 10008 = `i4` |
| `+0x4d0..0x4df` | 10009 = `i5` + 10027/10028/10029 = `rand-param-one/pi/int` |

**= 恰好 16 个变量，与 thpages 的 16 格表逐条一致** ✅。`pos`（10013–10015 @ +0x2c）与 `rotation`
（10023–10025 @ +0x38）**不在范围里，不拷**。

**④ 结论**（✅ TH16）：`copyParentVars` 把父的 16 个「寄存器 + 随机参数」变量拷到自身；
`pos`/`rotation` 不拷；**父为空则整条是 no-op**（只推进指针）。

**⑤ 证据**：`0x460e2e`–`0x460e83`；字段落点引用 [`04-variables.md`](04-variables.md) §2。

---

## 5. 专项③ world 列表与 UI 列表

**① 发现**：四个插入函数里，列表头/尾的基址分两对：`mgr+0xdc`（头）/`+0xe0`（尾）与 `mgr+0xe4`（头）/`+0xe8`（尾）。

**② 推测**：这是两张独立链表，`02-vm-core.md` §8 记为 `tick_world`（遍历 `mgr+0xdc`）与 `tick_ui`（遍历 `mgr+0xe4`）。

**③ 验证**：`02-vm-core.md` §8 一手已坐实 `AnmManager::on_tick_21__tick_world` `0x46e340` 走 `mgr+0x14c`/`+0xdc` 链、
`on_tick_09__tick_ui` `0x46e490` 走 `mgr+0xe4` 链；thpages `createChildUi` *"UI ANMs are ticked even while the game is paused"*
与 [`02-vm-core.md`](02-vm-core.md) §2 的暂停闸门（`flags_hi & 0xc000`）自洽——UI 列表的 VM 不受暂停闸门挡住。

**④ 结论**（✅ TH16）：

1. **world（`mgr+0xdc`/`+0xe0`）与 UI（`mgr+0xe4`/`+0xe8`）是两张渲染链表**，各自有 on_tick 驱动；UI 列表暂停期也 tick。
2. **「create / prepend」管的是这张渲染链表的位置**（尾/头），**不是子链**——子链（父 `+0x588` 的 `node16`）
   在 `create_managed_child` 尾部**无条件 prepend**（`0x46ee53`–`0x46ee75`：`new.next = 父.首子`、`父.首子 = new`），
   create 与 prepend 都一样。⇒ **「prependChild」= 创建子 VM 但插到渲染链头（先画、垫底），不是插到子链头**。
3. **层号折叠（th18）归渲染文档**：`th18:0x488260` 的 world 37–45 减 13 / UI 24–32 加 13 记在
   [`../th18/02-render-stages.md`](../th18/02-render-stages.md) §0。**本文没有在 th16 上复核 th16 的 `render_layer`
   是否有同款折叠**（那是渲染组的事），此处仅转引、不升可信度。

**⑤ 证据**：`0x46e7d0`/`0x46e890`/`0x46e940`/`0x46ea00`；`0x46ed60` 尾部 `0x46ee53`–`0x46ee75`；
`02-vm-core.md` §8 的 `0x46e340`/`0x46e490`。

---

## 6. 与社区不一致处的裁决

对账对象：thpages `local/vendor/thpages/js/tables/reference/anm.ts` 的 `byRefId`（`createChild`…`copyParentVars`）。
10 条的 `sig` 与基本语义逐条比对后，**8 条一致**（500–506、507、509 的「建子/建根/进哪张表/设位/拷变量」都对得上），
分歧只在下面两处。

### 6.1 `508 createEffect` —— 社区说「造一个特殊子图」，一手是「重配当前 VM」

thpages：*"Creates a special child graphic that may have additional hardcoded behavior."*（`wip:1`）。

**① 发现**：case 体（`0x460f0c`–`0x460f46`）解析 `arg0`（kind）后调 `EffectManager::create_effect` `0x418af0`，
参数是 `(out, kind, EBX, EBX)`——**当前 VM 被压了两次**（`0x460f27`/`0x460f28` 两条 `push ebx`）。

**② 推测**：`create_effect` 的第 3/4 参分别是「callback 上下文」与「要配置的 VM」；
第 4 参传当前 VM ⇒ 它配的是**自己**，不造新 VM。

**③ 验证**（读 `0x418af0` 全函数）：
- 入口 `esi = 0x4a2250 + kind*0x20`（effect 表，8-dword 项：`{s16 anm槽, s16 脚本, callback, 6 个回调下标}`）；
  `entry.s16[2] < 0` → 直接返回（无效 kind）。
- `edi = [ebp+0x14]`（第 4 参）；`test edi,edi; jne 0x418b48`——**第 4 参非空 → 跳过创建分支**，
  `edi` 保持 = 第 4 参（= 当前 VM），`*out = 0`。
- 随后 `if (entry.callback) entry.callback(edi, [ebp+0x10])`，再把 `entry[8..0x1c]` 六项写进 `edi+0x5c4..0x5d8`。
- 对照：通用特效 helper `FUN_0040e730`（游戏侧造特效）调它时**第 4 参传 0**，走「创建分支」——
  `AnmLoaded::create_effect` `0x406380` 建一个**根** VM 再装 callback。

**④ 结论**（分两层）：

- **机制**（✅ TH16）：`508` 在 ANM VM 里把**当前 VM** 作为 `create_effect` 的第 4 参 ⇒ **不创建任何新 VM**，
  只把 effect 表 `kind` 项的 6 个回调下标写进当前 VM（`+0x5c4`=on_tick、`+0x5d0`=on_interrupt、
  `+0x5c8`/`+0x5cc`/`+0x5d4`/`+0x5d8` 四个未在字段图里登记的槽），并（若表项有 callback）用 `(当前VM, 当前VM)` 调它。
  真实数据 `508` ×42 全部 `kind = 1`，表项 1 的 callback `0x405670` 一手确认是**在当前 VM 上**建顶点缓冲
  （写 `vm+0x5b8`/`+0x5bc`，缓冲内部结构再塞 `0xfff0bdc1` 计时器哨兵；`0x405679`/`0x405692`）。
- **「重配自己」这个解读**（✅ 机制层面）与 thpages *"creates a special child graphic"* **冲突**：
  thpages 是从名字外推的（`wip:1`），一手不支持「造子图」。**本组判社区这条错**，但**不下「这是 ZUN 的 bug 还是有意」的判断** ❓——
  「effect 表每项 callback 各自干什么」也只核了 kind 1，其余 🟡（归 EffectManager/gameplay）。

**⑤ 证据**：`0x460f0c`–`0x460f46`；`0x418af0`（`0x418b16`–`0x418b48` 的 arg3 分支、`0x418b4e`–`0x418b8c` 的 callback 安装）；
effect 表 `0x4a2250`（直读 PE，项 1 的 callback = `0x405670`）；对照 `FUN_0040e730`（`0x40e754` `push 0` = 第 4 参空）。

### 6.2 `505/506` 的 `pos` 措辞 —— 社区伪代码里 `child.pos = {0,0,0}` 是近似

thpages `createChildPos` 伪代码：`child.pos = {0,0,0}; child.pos_2 = {x,y,0}; child.pos_3 = {0,0,0}`。

一手（[§2.2](#22-505506-的尾巴把手柄解回-vm-指针再写-pos_2)）：**只写 `pos_2.x/y`（+0x4e0/+0x4e4），`pos_2.z` 不写**；
`pos`（+0x2c）与 `pos_2.z` 都是模板值（`time<=0` 段跑完后的值），`entity_pos`（thpages 的 `pos_3`）由
`create_managed_child` 清 0（505）或由 `create_managed_root` 从源拷来（506）。
⇒ 社区那句 `child.pos = {0,0,0}` 只在「子脚本 `time<=0` 段没动过 pos」时成立，**不是硬保证**。
裁决：**社区没写全**（漏了「写的是 `pos_2` 不是 `pos`」「`.z` 不写」「`pos` 是模板值」这三点），
对写播放器的影响见 [§8](#8-写播放器会踩的坑)。

---

## 7. 真实数据对账（54 个 `.anm`）

自写解析器（复用 `runtime/anm` 的 `parseAnm`，分母 = 54 文件 / 1929 script / 41339 指令，
本组 **1293 条**，分布 870+91+75+4+192+15+0+1+42+3）：

| 检查项 | 结果 |
| --- | --- |
| 每条 `size` 是否 = `8 + 4×参数个数`（§2 表推） | **1293 / 1293 吻合**，0 例外 |
| `500–508` 的 `arg0` 是否总被 `param_mask bit0` 标成变量 | **否：1290 / 1290 全为字面量**（bit0 从没置位）；仅 `505` 的 `x`/`y` 用了 bit1/bit2 |
| `505` 的 `param_mask` 分布 | `0x0` ×12、`0x4` ×2（`y` 是变量）、`0x6` ×1（`x`/`y` 都是变量） |
| `505` 变量参数按 float32 解 | `10004.0`（F0）×2、`10005.0`（F1）×1、`10006.0`（F2）×1，**反例集合为空** |
| `507` 的 `arg0` | 1 次，值 = `0`（= 清「ignore parent」位） |
| `508` 的 `arg0`（kind） | 42 次，**全部 = 1** |
| `509` 的参数个数 | 3 次，**全 0 参** |
| 各 opcode 的 `arg0` 取值范围 | 500 `[1,313]`（285 个不同值）· 501 `[2,155]` · 502 `[6,34]` · 503 `{0x10,0x39}` · 504 `[77,90]`（8 个值）· 505 `[10,39]` |

**本组在 TH16 自家资源里的使用分布**（9 条用到 / 1 条一次没用）：

按出现次数排（合计 1293 条）：`500`×870 · `504`×192 · `501`×91 · `502`×75 · `508`×42 · `505`×15 ·
`503`×4 · `509`×3 · `507`×1 · **`506`×0**。

**一次都没出现的 1 条**：`506 createRootPos`。它的实现完整（独立 handler），只是 ZUN 自家 anm 没用到——
mod 作者可以用，但**没有 ground truth 可对**。

> 两个统计口径要说清：`500` 的 870 条、`504` 的 192 条加起来占了本组 82%，说明「建子 / 建根」是
> ANM 复用的绝对主力；`502` 的 75 条里 `arg0=0x12`（=18）占了多数，是某几处固定复用同一子脚本的「prepend 到渲染链头」。

---

## 8. 写播放器会踩的坑

| 坑 | 说明 |
| --- | --- |
| **`arg0` 是脚本运行序号，不是文件里的 script id** | 500–506 的 `arg0` 索引 `AnmLoaded+0x10c` 模板数组（跨 entry 累加的运行序号，`01-format.md` §5 的「按位置索引」）。ZUN 数据里恒为字面量，但 handler 确实查了 bit0——畸形文件可以让它读变量 |
| **`505/506` 写的是 `pos_2`，不是 `pos`** | §2.2。播放器若把 x/y 写进 `pos`（+0x2c），渲染期父变换一叠加就整体平移错位 |
| **`505/506` 的写入在「创建即 tick」之后** | 子 VM 的 `time<=0` 脚本读到的是模板 `pos_2`，看不到这次传参。要复刻「创建→先 tick→再写 pos_2」的顺序 |
| **创建即 tick** | 500–506 都在插列表前跑一次 `AnmVm::run`；模板本身也已预跑过 `time<=0` 段（`02-vm-core.md` §8） |
| **child 不拷父的 pos/rotation/scale** | §3。子 VM 的局部变换全来自模板；继承只发生在渲染期通过父指针。播放器别在「创建」时把父的 transform 抄给子（那是 `504/506` 才做的事） |
| **`createRoot` 把源的 `pos` 拷进新根的 `pos_2`** | §3。写播放器时别抄错目标字段（`MOVQ [ESI+0x4e0] ← [EDX+0x2c]`） |
| **`507` 是设值不是翻转** | `ignoreParent(n)` = 位 = `n&1`，连续调两次 `ignoreParent(1)` 仍是 1。别写成 `toggle` |
| **`509` 父为空 = no-op** | 根 VM 上执行 `copyParentVars` 什么都不发生，别当 crash 或报错 |
| **`509` 不拷 `pos`/`rotation`** | 只拷 16 个寄存器/随机参数变量。别把「拷贝父变量」理解成「拷贝父的全部状态」 |
| **`508` 不建子 VM** | 它重配当前 VM（装回调下标 + 调 setup callback）。照 thpages 写成「创建子图」会和真机发散 |
| **mode 编码列表**（bit2=UI、bit1=头） | `0`=world 尾 / `2`=world 头 / `4`=UI 尾 / `6`=UI 头。播放器建渲染桶时照这个进表，UI 列表还要「暂停期照 tick」 |

---

## 9. 还没搞清楚的

| 问题 | 状态 |
| --- | :---: |
| `508` 各 effect kind 的 callback（`0x4a2250` 表项 [4]）具体干什么 | 🟡（只核了 kind 1 = 建顶点缓冲 + 哨兵计时器） |
| `+0x5c8`/`+0x5cc`/`+0x5d4`/`+0x5d8` 四个回调下标字段对应哪几张回调表 | ❓（`02-vm-core.md` §3.5 只登记了 `+0x5c0`/`+0x5c4`/`+0x5d0`/`+0x5dc`） |
| `+0x51c`（创建时存 mode 0/2/4/6）下游被谁读、作何用 | 🟡 |
| 子链尾指针 `+0x594` 的更新在 `0x46ee69`–`0x46ee6f` 处「非空才更新」的怪癖是否有下游影响 | 🟡 |
| th16 的 `render_layer` 是否有 th18 `0x488260` 同款的层号折叠（world/UI 折叠） | — 归渲染文档 |
| **VM 池耗尽时 `create_managed_child`/`create_managed_root` 会怎样**（`AnmManager::allocate_vm` `th16:0x46f600` 返回空？复用最老 VM？）——见表下补注 | ❓ |
| 「父变换怎么叠加到子」的精确矩阵（含 `flags_hi 0x10000` 的继承闸门） | — 归渲染文档 |

> **关于「VM 池耗尽」这一条**（2026-09-10 审阅补挂账）：分配机制本身归
> [`02-vm-core.md`](02-vm-core.md) §8，但 `500`–`506` 恰恰是 `AnmManager::allocate_vm` `th16:0x46f600`
> 最主要的调用方，而本文 §8 的「写播放器会踩的坑」一字未提——播放器若无限建子 VM，行为与真机
> 在池满那一刻就分岔了。本次没有追这条路径，先挂在这里而不是假装它不存在。

---

## 操作数求值次序（2026-09-09 审阅补做）

[`03-math-jumps.md`](03-math-jumps.md) §9.1 把「按 `test byte [edi+6]` 的执行次序定操作数求值序」
立成了本子系统的通用纪律（理由：读变量会推进 PRNG，次序可观察且写反不报错）。本组原文没做，补上：

| opcode | arg 求值次序 | 说明 |
| --- | --- | --- |
| `505 createChildPos` | `(0, 1, 2)` **升序** | script id → x → y。次序被数据依赖强制（id 是 create 调用的入参，x/y 是创建后另写回的字段），属**平凡**情形 |
| `506 createRootPos` | 自身 case 体只读 arg0（`th16:0x460e88` `test byte [edi+6],1`），随即 `jmp th16:0x460d1e` 进**共用尾巴**读 x/y | 与 `505` 同族 |
| 其余 8 条 | — | 0 或 1 个参数，次序问题不适用 |

**「平凡」也要写下来**：否则读者无法区分「作者查过、确认是平凡的」与「作者没查」。

## 关联

- 文件格式 / `param_mask`：[`01-format.md`](01-format.md)
- VM 主循环 / `zAnmVm` 字段图 / 终止（父死子树一并销毁）：[`02-vm-core.md`](02-vm-core.md)
- 变量号表（`pos`/`rotation`/随机参数落点）：[`04-variables.md`](04-variables.md)
- 渲染阶段 / 层号折叠（th18，转引未自证）：[`../th18/02-render-stages.md`](../th18/02-render-stages.md)
- 子系统总览：[`../OVERVIEW.md`](../OVERVIEW.md)
- 本作入口：[`README.md`](README.md)
- 想要的 Ghidra 重命名（待 T10 统一落库）：[`naming-batch.md`](naming-batch.md)
- 纪律：[`../../../METHOD.md`](../../../METHOD.md)、[`../../../DOCSTYLE.md`](../../../DOCSTYLE.md)
