# ANM v8 文件格式 — TH16 一手确认

> **版本**：TH16 v1.00a（`th16.exe`，imagebase `0x400000`）。本文裸地址默认属该版本；引用其他版本须写成 `th18:0x…`。
> **方法**：三方对账 —— (a) 一手反编译 TH16 的 anm 加载路径（`0x46cdd0` → `0x46d1c0` → `0x46d3b0` → `0x46cb60`/`0x46c920`）；
> (b) thtk `thanm` 的 `anm_types.h`（社区单源，格式权威）；(c) 自写解析器跑 `local/th16.v1.00a/dat/` 的 **54 个真实 `.anm`（423 entry / 1929 script / 43268 条指令）**。
> **可信度**：字段表 25 条 ✅（每条都指得到一条读该偏移的机器指令）/ 4 条 🟡（游戏不读，只有 thanm + 真实文件双证）。
> **不含版权字节**：本文只写偏移与语义，不贴任何 ZUN 数据。

## 0. 一句话结论

TH16 的 `.anm` = **一条 entry 单向链表**（每个 entry 自带 0x40 字节头 + sprite 偏移表 + script 偏移表 +
名字串 + 脚本字节码 + 可选 THTX 贴图块），**所有偏移都相对该 entry 的起始**，链表由头部 `+0x24`
的 `next_offset` 串起，`0` 结束。TH16 的加载器**只接受 `version == 8`**，没有任何旧版本分支。

---

## 1. 怎么找到加载路径的

**① 发现**：`th-re-data` 的 th16 命名里有 `AnmManager::do_load_anm` `0x46cdd0` / `do_preload_anm` `0x46cf80` /
`AnmLoaded::load_one_script` `0x46d0c0`（社区单源，路标）。
**② 推测**：`do_load_anm` 是「读文件 → 建表」的总入口。
**③ 验证**：反编译 `0x46cdd0`：它 (a) 把整个文件读进一块内存挂到 `AnmLoaded+0x108`；
(b) 沿 `entry[+0x24]` 走链表，把每个 entry 的 `+0x04`/`+0x06` 累加成总 sprite 数 / 总 script 数；
(c) **四次** `malloc`：entry 贴图 slot 表（`n_entries * 0x18`）、运行时 sprite 表
（`n_sprites * 0x44`）、脚本指针表（`n_scripts * 4`）、VM 模板数组（`n_scripts * 0x5fc`）；
(d) 逐 entry 调 `0x46d0c0`。
`0x46d0c0` 只处理「外部 png」一支，**真正解析表结构的是它后面那个未命名的
`FUN_0046d3b0`**（由 `FUN_0046d1c0` 逐 entry 调用）——sprite 表、script 表、贴图创建全在里面。
**④ 结论**：加载路径 = `0x46cdd0`（读文件 + 预扫链表 + 分配）→ `0x46d0c0`（外部 png 预读）→
`0x46d1c0`（**增量**驱动：每次调用只推进一个 entry）→ `0x46d3b0`（解析单个 entry）→
`0x46cb60`/`0x46c920`/`0x46cd30`/`0x46cd80`（建贴图）✅。
**⑤ 证据**：上列各地址；错误串 `0x494280`（Shift-JIS「アニメのバージョンが違います」）正是版本检查的失败分支。

> ⚠️ `AnmLoaded::load_one_script` 这个社区名**名不副实**：`0x46d0c0` 干的是「若 `has_data==0`
> 且名字不以 `@` 开头，就把同名 png 读进 slot」。改名建议见 [`naming-batch.md`](naming-batch.md)。

---

## 2. 整体布局

```
文件 = entry0 [+ next_offset] entry1 [+ next_offset] … entryN(next_offset = 0)

entry (一切偏移相对 entry 起始):
  +0x00  header                     0x40 字节，见 §3
  +0x40  sprite_offsets[n_sprites]  u32 × n，每个 = sprite 记录相对 entry 的偏移
         script_entries[n_scripts]  {i32 id; u32 offset} × n
         name                       NUL 结尾字符串（name_offset 恒指到这里）
         …                          sprite 记录（20 字节 × n）、脚本字节码
  +thtx_offset  THTX 块（仅 has_data != 0）
```

**字节序：全文件小端（little-endian）** ✅ —— 目标平台是 x86 32 位 PE（`th16.exe`，
`imagebase` `0x400000`），加载器对所有多字节字段都是 `MOV`/`MOVZX`/`MOVSX` 直读，
没有任何字节交换。float 是 IEEE-754 单精度小端（`MOVSS` 直读，如 `0x46d54e`）。

**名字串：NUL 结尾的字节串，不带长度前缀** ✅（`0x46d42a` 取指针后
直接当 C 字符串用：`0x46d432` 比较首字节、`0x46d132` 喂 `sprintf` 的 `%s`，
再交给读文件的 `0x402440`）。**编码上游是 Windows ANSI 代码页**（日文环境 = Shift-JIS）——
因为它最终进的是 `fopen` 族的窄字符 API，不是 UTF-8 也不是 UTF-16。
**实测 423/423 个名字全是纯 ASCII**（无 ≥ `0x80` 字节），所以按 ASCII 或 UTF-8 解在 TH16 上等价；
要稳妥就按 Shift-JIS 解、失败回退 latin-1。

**实测**：54 个文件 423 个 entry 里，`name_offset` **恒等于** `0x40 + 4*n_sprites + 8*n_scripts`
（423/423），即名字紧跟在 script 表后面。**但加载器读的是 `name_offset` 字段本身**（`0x46d42a`：
`MOV EAX,[ESI+0x10]; ADD EAX,ESI`），所以写解析器要老实用字段，别假设这个巧合。

---

## 3. Entry header（0x40 字节）字段表

| 偏移 | 类型 | 名 | 含义 | 可信度 | 一手证据 |
| --- | --- | --- | --- | :---: | --- |
| `+0x00` | u32 | `version` | 必须 = 8，否则弹错报错退出 | ✅ | `0x46d0e2`、`0x46d404` |
| `+0x04` | u16 | `n_sprites` | 本 entry 的 sprite 数 | ✅ | `0x46d4ba`；累加于 `0x46cdd0` |
| `+0x06` | u16 | `n_scripts` | 本 entry 的 script 数 | ✅ | `0x46d5bf`、`0x46d5e9` |
| `+0x08` | u16 | （零） | 加载路径全程不读；423/423 实测为 0 | 🟡 | 见 §3.1 |
| `+0x0a` | u16 | `width` | 运行时贴图的**分配**宽（2 的幂） | ✅ | `0x46d4f5`、`0x46d61e`、`0x459640` |
| `+0x0c` | u16 | `height` | 运行时贴图的**分配**高 | ✅ | `0x46d52c`、`0x46d619` |
| `+0x0e` | u16 | `format` | 目标贴图像素格式，索引 §7 的两张表 | ✅ | `0x46d473`、`0x46d623`、`0x46d663` |
| `+0x10` | u32 | `name_offset` | 名字串偏移（相对 entry） | ✅ | `0x46d11b`、`0x46d42a` |
| `+0x14` | **s16** | `x_offset` | 外部 png 里的裁剪原点 X（`MOVSX`，有符号） | ✅ | `0x46d614` |
| `+0x16` | **s16** | `y_offset` | 外部 png 里的裁剪原点 Y（`MOVSX`，有符号） | ✅ | `0x46d60f` |
| `+0x18` | u32 | `memory_priority` | **TH16 加载路径全程不读**（历史字段） | ✅ | 见 §3.1 |
| `+0x1c` | u32 | `thtx_offset` | THTX 块偏移（相对 entry），仅 `has_data` 时用 | ✅ | `0x46d668` |
| `+0x20` | u16 | `has_data` | ≠0 → 贴图内嵌为 THTX；=0 → 外部 png / `@` | ✅ | `0x46d111`、`0x46d420` |
| `+0x22` | u8 | `low_res_scale` | ≠0 且窗口未满分辨率时，贴图整体减半 | ✅ | `0x46cbbf`、`0x46cc96`；`0x46c920` 内同款判断 |
| `+0x23` | u8 | （零） | 加载路径不读；thanm 记为 TH19 的 `jpeg_quality` | 🟡 | 423/423 实测为 0 |
| `+0x24` | u32 | `next_offset` | 下一个 entry 的偏移（相对本 entry），`0` = 末尾 | ✅ | `0x46cdd0`、`0x46d1c0` |
| `+0x28` | u8[24] | （零） | 加载路径不读；423/423 实测全零 | 🟡 | thanm 把前 4 字节记为 TH19 的 `w_max`/`h_max` |

**header 固定 0x40 字节**：sprite 偏移表的取址是硬编码的 `LEA EBX,[ESI+0x40]`（`0x46d4b4`）✅。

### 3.1 「不读」是怎么确认的

`+0x08`、`+0x18`、`+0x23`、`+0x28..0x40` 这四段：拿到 entry 指针的函数只有五个——
`0x46d0c0`、`0x46d1c0`、`0x46d3b0`，以及经 slot `+0x10` 回指 entry 的 `0x46cb60`、`0x46c920`
（外加只读 `+0x0a`/`+0x0c` 的 `0x459640`）。这五个函数我**逐条反汇编读完**，对 entry 的访问
只出现在上表列出的偏移上；`+0x18` 一次都没出现。所以「TH16 的 anm 加载路径不读
memory_priority」是 ✅；「整个游戏都不读」只能算 🟡（没做全局穷举）。这与 thanm 注释
「As of TH16.5, unused in every game that uses this structure」一致。

### 3.2 `width`/`height` 是**贴图分配尺寸**，不是图像尺寸

**① 发现**：`0x46cb60` 用 entry 的 `width`/`height`/`format` 调 `D3DXCreateTexture`（`0x46cc45` 附近），
再用 **THTX 自己的** `format`/`width`/`height` 调 `D3DXLoadSurfaceFromMemory`，目标矩形 = `{0,0,thtx_w,thtx_h}`。
**② 推测**：entry 的 w/h 是「分配到 2 的幂」的贴图，THTX 的 w/h 是真实像素区。
**③ 验证**：417 个带 THTX 的 entry 里 **215 个** entry.w/h 与 THTX.w/h 不同（例：某 entry `h=64` 而 THTX `h=48`），
且 entry.w/h 恒 ≥ THTX.w/h 并为 2 的幂。
**④ 结论**：`width`/`height` = D3D 贴图分配尺寸；THTX 的 w/h = 实际图像尺寸 ✅（TH16）。
**⑤ 证据**：`0x46cb60`、`0x46cc45`、`0x46cce8`；解析器统计见 §10。

### 3.3 名字的三种形态

`0x46d3b0` 在 `has_data == 0` 时看名字第一个字节（`0x46d432`：`CMP byte ptr [EAX],0x40`）：

| 名字 | 行为 | 证据 |
| --- | --- | :--- |
| 普通路径（如 `foo/bar.png`） | 由 `0x46d0c0` 预读该文件字节，`0x46c920` 用 D3DX 从内存解码 | `0x46d0c0`、`0x46d632` |
| `@` 开头 | 不读任何文件，`0x46cd30` 直接建一张 `width×height`、`format` 的空贴图 | `0x46d435`、`0x46d48b` |
| `@R` 开头 | 先把 entry 的 `width`/`height` **原地改写**成屏幕分辨率，再建空贴图 | `0x46d43b`–`0x46d46c` |

`@R` 那支从 `0x4d9d2c`（`RESOLUTION_X`）/`0x4d9d30`（`RESOLUTION_Y`）取值并写回 `entry+0x0a`/`+0x0c` ✅。
外部 png 那支（`0x46c920`）把 `x_offset`/`y_offset` 当作源图里的**裁剪左上角**，宽高取
`min(width, png_w - x_offset)`，即「从大图里抠一块」——这解释了 thanm 里 `xOffset`/`yOffset` 的用途 ✅。

### 3.4 `MOVSX` 说明这些是有符号量 —— 但**对解析器没有影响**

TH16 读 `entry+0x14`/`+0x16`（`0x46d614`/`0x46d60f`）和 THTX 的 `format`/`width`/`height`
（`0x46ccc0`/`0x46ccc5`/`0x46cb8c`/`0x46cb90`）用的都是 **`MOVSX`**（符号扩展），
而 thanm 的 `anm_header11_t` / `thtx_header_t` 把这五个字段都声明成 `uint16_t`。
**ABI 层面是 thanm 记松了 ✅**，但**写解析器时按 u16 还是 s16 读，结果完全一样**，原因有二：

1. **下游会直接失败**。`x_offset`/`y_offset` 进的是 `0x46c920` 里 `D3DXLoadSurfaceFromSurface`
   的**源 `RECT` 左上角**；负的 `RECT.left`/`RECT.top` 会让该调用返回失败。
   THTX 的 w/h 则会算出负的行距。也就是说，能让 s16 与 u16 解析结果不同的文件（需要
   `x ≥ 32768` 像素），**在这条路径上根本跑不起来**。
2. **真实数据离那条界线很远**。423 个 entry 的 `x_offset` ∈ [0, 1024]、`y_offset` ∈ [0, 448]，
   THTX 的 w/h 全部 ≤ 2048，**没有任何一个值 ≥ `0x8000`**（见 §10）。

所以这两条更正的价值是「把 ABI 写准，将来做写回/校验时不会踩」，**不是**「按 u16 解会解错」。

---

## 4. Sprite 表

**偏移表**：`entry+0x40` 起，`n_sprites` 个 u32，每个是 **sprite 记录相对 entry 的偏移**
（`0x46d4d4`：`MOV ECX,[EBX]; ADD ECX,ESI`），步长 4（`0x46d5a2`）✅。

**sprite 记录（20 字节）**：

| 偏移 | 类型 | 名 | 含义 | 可信度 | 证据 |
| --- | --- | --- | --- | :---: | --- |
| `+0x00` | u32 | `id` | **加载器不读**；实测恒 = 该 sprite 在文件内的全局序号 | ✅ | 见下 |
| `+0x04` | f32 | `x` | 左上角 X（entry 贴图坐标系，像素） | ✅ | `0x46d54e` |
| `+0x08` | f32 | `y` | 左上角 Y | ✅ | `0x46d564` |
| `+0x0c` | f32 | `w` | 宽 | ✅ | `0x46d572` |
| `+0x10` | f32 | `h` | 高 | ✅ | `0x46d585` |

**id 是装饰性的**：`0x46d3b0` 把解析结果写进 `AnmLoaded+0x11c` 的运行时数组时，下标用的是
「跨 entry 累加的运行序号」（`0x46d5aa`：`INC dword ptr [EBP+0x10]`），**从不读 `+0x00`** ✅。
实测 54 个文件 3101 个 sprite，`id` 与运行序号 **0 处不符**——所以两种解释在 ZUN 的文件上等价，
但**照实现写解析器要按位置索引**。

**运行时 sprite 结构 0x44 字节**（`0x46d8a0`）：存 `{anm槽, entry序号, (槽<<8)|entry, x0,y0,x1,y1, texH, texW, u0,v0,u1,v1, …}`，
其中 `x0 = sprite.x * (实际贴图宽 / entry.width)`，UV = 像素 / 贴图边长。
**注意 sprite 坐标可以超出 entry 的 w/h**（例：某 16 高的 entry 上有 `y=144` 的 sprite），
靠 D3D 的 wrap 寻址绕回来 ✅（`0x46d8a0` 的除法里没有任何 clamp）。

---

## 5. Script 表与指令流

**script 表**：紧接 sprite 偏移表之后，每项 **8 字节**（`0x46d5ed`：`ADD ECX,8`）：

| 偏移 | 类型 | 名 | 含义 | 可信度 | 证据 |
| --- | --- | --- | --- | :---: | --- |
| `+0x00` | s32 | `id` | **加载器不读**；真实文件里可为负 | ✅ | `0x46d5c8`–`0x46d5d4` 跳过它 |
| `+0x04` | u32 | `offset` | 脚本字节码偏移（相对 entry） | ✅ | `0x46d5d4`、`0x46d5df` |

`0x46d3b0` 把 `entry + offset` 存进 `AnmLoaded+0x120` 的指针数组，下标同样是跨 entry 的运行序号 ✅。
**这里 id 与序号会真的对不上**：e01–e08.anm 的 script id 是 −72…−1，而 VM 取脚本一律用序号
（`0x45fba0` 附近：`AnmLoaded[+0x120][script_index]`）✅。thanm 的行为与之一致——它按位置命名
`script0/script1/…`，只在 id ≠ 序号时才把 id 打印出来。

### 5.1 指令头（8 字节）

| 偏移 | 类型 | 名 | 含义 | 可信度 | 证据 |
| --- | --- | --- | --- | :---: | --- |
| `+0x00` | s16 | `opcode` | 指令号；`-1`（`0xffff`）= 脚本结束 | ✅ | `0x45fbf5` 的 switch；`0x45faf1` 取 `[EDI]` → `0x45faff` `CMP AX,-1` |
| `+0x02` | u16 | `size` | **整条指令**字节数（含 8 字节头） | ✅ | VM 用它步进 |
| `+0x04` | s16 | `time` | 触发帧；`vm_timer >= time` 才执行 | ✅ | `0x45f980` 里 `psVar18[2] <= timer` |
| `+0x06` | u16 | `param_mask` | bit *i* = 第 *i* 个参数是**变量 id**，取值前要查 VM 变量 | ✅ bit0–8 / ❓ bit9–15 | 见 [§5.2](#52-param_mask-的位宽与位序是逐位反出来的) |
| `+0x08` | u32[] | `args` | `(size - 8) / 4` 个 4 字节参数 | ✅ | `psVar18+4`/`+6` = `+0x08`/`+0x0c` |

**结束标记的坑**：`opcode = -1` 那条的 `size` 字段实测**恒为 0**，但它在文件里**占 8 字节**——
下一个 script 的 offset 恰好在它之后 8 字节（1680 处全部吻合）。游戏永远不会越过它，所以
`size=0` 对运行时无害；**解析器必须把它当 8 字节处理**，否则会死循环 ✅。

**跳转是脚本内字节偏移**：`case 200`（`0x45fbfc`）把 `arg0` 直接写进 VM 的「脚本内偏移」字段、
把 `arg1` 写进计时器 ✅ —— 所以 `jmp` 的目标是**相对该 script 起点的字节偏移**，不是指令序号。

### 5.2 `param_mask` 的位宽与位序是逐位反出来的

VM 里 `EDI` = 当前指令指针，于是 `[EDI+0x6]` = `param_mask`、`[EDI+0x8+4*i]` = 第 *i* 个参数。
把 `.text` 里所有 `TEST byte ptr [EDI+0x6], imm8` 扫出来（**210 处，全部落在 `AnmVm::run`
`0x45f980`–`0x4630f0` 内**），每处后面紧跟的取参偏移是唯一的：

| 位 | 判据指令 | 随后取的参数 | 首个地址 | 站点数 |
| :---: | --- | :---: | --- | :---: |
| 0 | `TEST byte [EDI+6], 0x01` | `[EDI+0x08]` = arg0 | `0x45fc12` | 95 |
| 1 | `TEST byte [EDI+6], 0x02` | `[EDI+0x0c]` = arg1 | `0x45fce8` | 62 |
| 2 | `TEST byte [EDI+6], 0x04` | `[EDI+0x10]` = arg2 | `0x45fd7d` | 34 |
| 3 | `TEST byte [EDI+6], 0x08` | `[EDI+0x14]` = arg3 | `0x460663` | 11 |
| 4 | `TEST byte [EDI+6], 0x10` | `[EDI+0x18]` = arg4 | `0x46195f` | 5 |
| 5 | `TEST byte [EDI+6], 0x20` | `[EDI+0x1c]` = arg5 | `0x461c83` | 1 |
| 6 | `TEST byte [EDI+6], 0x40` | `[EDI+0x20]` = arg6 | `0x461c6a` | 1 |
| 7 | `TEST byte [EDI+6], 0x80` | `[EDI+0x24]` = arg7 | `0x461b7c` | 1 |
| 8 | `TEST word [EDI+6], SI`（`ESI = 0x100`） | `[EDI+0x28]` = arg8 | `0x461ba1` | 1 |

**所以 `bit i ↔ arg i` 是一手 ✅，不是从 thanm 抄的**：八个 `imm8` 常量 `0x01`…`0x80` 全部出现，
每个都紧跟一次唯一的取参偏移，位与参数序号一一对应。

**位宽也是一手 ✅**：`0x461ba1` 是一条 **16 位** `TEST word ptr [EDI+0x6], SI`，
其中 `ESI` 由 `0x45fb9a` 的 `MOV ESI, 0x100` 在进入分发循环前一次性设好——
这是全 exe 唯一一处对 `[EDI+0x6]` 的 16 位测试（`0x46069b`、`0x4606a2` 也用
`MOVZX r32, word ptr [EDI+0x6]` 把它整字读出来，但随后只测 `AL`/`CL`）。
一个只有 8 位的字段不会有人拿 `0x100` 去测它。

**bit 9–15 ❓**：exe 里从没出现过对它们的测试。真实文件里最高只用到 **bit 8**
（4 条 `ins_420`，`param_mask` = `0x180` / `0x186`，见 §10），恰好被 `0x461ba1` 那条接住。

---

## 6. THTX 贴图块（16 字节头 + 像素）

| 偏移 | 类型 | 名 | 含义 | 可信度 | 证据 |
| --- | --- | --- | --- | :---: | --- |
| `+0x00` | char[4] | magic | `"THTX"`；**加载器不校验** | 🟡 | 417/417 实测；`0x46cb60` 未读 |
| `+0x04` | u16 | （零） | 不读；417/417 实测为 0 | 🟡 | 同上 |
| `+0x06` | s16 | `format` | **源**像素格式（可与 entry.format 不同） | ✅ | `0x46ccc0` `MOVSX`，见 [§3.4](#34-movsx-说明这些是有符号量--但对解析器没有影响) |
| `+0x08` | s16 | `width` | 源图宽（也用来算行距） | ✅ | `0x46cb8c`、`0x46ccc5`（均 `MOVSX`） |
| `+0x0a` | s16 | `height` | 源图高 | ✅ | `0x46cb90`（`MOVSX`） |
| `+0x0c` | u32 | `size` | 像素字节数；**加载器不读**，实测 = `w*h*bpp` | 🟡 | 417/417 |
| `+0x10` | u8[] | 像素 | 行距 = `width * bpp[format]`，紧密无 padding | ✅ | `0x46ccdb`、`0x46cccb` |

行距用的是 **THTX 自己的 format**（`0x46cccb`：`IMUL EAX,[ECX*4 + 0x491b90]`），
而目标贴图用 **entry 的 format**（`0x46cc45`）——54 个文件里有 1 个 entry 两者不同 ✅。

---

## 7. `format` 枚举（一手表，胜过 thanm 的四项枚举）

TH16 有两张并列的 9 项查表，都按 `format` 值索引：`0x491b90` = 每像素字节数，`0x491bb4` = `D3DFORMAT`。
逐字读出来（`0x46cc45`、`0x46cccb`、`0x46cd10` 都在用它们）：

| format | bpp | D3DFORMAT | 名 | thanm 有无 |
| :---: | :---: | :---: | --- | :---: |
| 0 | 4 | `0`（UNKNOWN） | 不可用 | ❌ |
| 1 | 4 | `21` | `A8R8G8B8` | ✅ `BGRA8888` |
| 2 | 2 | `25` | `A1R5G5B5` | ❌ |
| 3 | 2 | `23` | `R5G6B5` | ✅ `RGB565` |
| 4 | 3 | `20` | `R8G8B8` | ❌ |
| 5 | 2 | `26` | `A4R4G4B4` | ✅ `ARGB4444` |
| 6 | 2 | `29` | `A8R3G3B2` | ❌ |
| 7 | 1 | `28` | `A8`（**只有 alpha，不是灰度**） | 🟡 thanm 记作 `GRAY8` |
| 8 | 1 | `27` | `R3G3B2` | ❌ |

> **表里的可信度分两层**：**九对字节是一手 ✅**（直读 `0x491b90` / `0x491bb4` 的 80 字节，
> 三处消费点 `0x46cc45`、`0x46cccb`、`0x46cd10`）；**「名」这一列是按 D3D9 的 `D3DFORMAT`
> 枚举值翻的 🟡**——`D3DFORMAT` 的数值定义来自 DirectX SDK 头文件，不是从这个 exe 里读出来的。
> 另外 format **0/2/4/6/8 只有表项，没有代码路径证据**（真实数据里 6 出现 1 次，0/2/4/8 从未出现），
> 它们的行只说明「查表会得到这个值」，不说明游戏走得到那条路 🟡。

**实测出现的 format**：1（191 个 entry）、3（53）、5（139）、6（1）、7（39）。

**format 7 = `D3DFMT_A8`（表项 `0x491bb4[7] = 28`、bpp = 1）是一手 ✅**，与 thanm 的
`FORMAT_GRAY8` 命名冲突。**由此推出的渲染行为则是 🟡**：按 D3D9 规范 `D3DFMT_A8` 只有 alpha
通道、RGB 采样为 0，所以「当灰度贴图渲」多半会渲错——但**本文没有在 exe 里确认 TH16 给这类
贴图配的 texture stage 到底从哪取 color**（大概率 `D3DTA_DIFFUSE`，未证）。WebGL 端要正确复刻
还差这一步，留给将来的渲染文档。

---

## 8. 运行时侧（写播放器要知道的四件事）

- **anm 槽上限 31**：`0x46cf80` 里 `if (0x1e < slot) 报错`，槽数组基址 `ANM_MANAGER_PTR + 0x184f4f0`，
  遍历处 `0x459640`、`0x46d690` 都以 `0x1f` 为界 ✅。
- **`AnmLoaded` 对象 0x13c 字节**（`0x46cf80` 的 `operator_new(0x13c)`）；关键字段：
  `+0x108` 文件字节、`+0x10c` 脚本模板数组、`+0x110` entry 数、`+0x114` 总 script 数、
  `+0x118` 总 sprite 数、`+0x11c` 运行时 sprite 数组（步长 `0x44`）、`+0x120` 脚本指针数组、
  `+0x124` 每 entry 的贴图 slot 数组（步长 `0x18`）✅。
- **entry 是分次解析的**：`0x46d1c0` **每次调用只解析一个 entry**——它从文件头重走链表，
  但只对下标 = `AnmLoaded+0x128` 减一的那个 entry 调 `0x46d3b0`，然后把 `+0x128` 加一就返回；
  驱动它的是 `0x46d690`（每帧检查 31 个槽）。**做离线解析器可以一次性走完链表**，
  但要知道游戏里加载是跨帧摊开的 ✅。
- **走到最后一个 entry 才预跑 script 模板**：`0x46d1c0` 在 `next_offset == 0` 那一次跳出循环后，
  对 `n_scripts` 个 VM 模板逐个 `AnmVm::wipe` + `AnmVm::run`（步长 `0x5fc`），
  结果就是 `+0x10c` 的模板数组；实例化时由 `0x407b20` 拷出来——
  **只拷模板的前 `0x538` 字节（prefix），不是整个 `0x5fc`**：suffix 先被 `AnmVm::wipe_suffix` `0x407a50` 清掉，
  两个 zTimer 随后重置为 `set_value(0)`。细节见 [`02-vm-core.md`](02-vm-core.md) §8 ✅。
  → **播放器要复刻这一步**：脚本的 `time <= 0` 段在「实例化之前」就已执行完。
- **贴图 slot（0x18 字节）**：`+0x00` `IDirect3DTexture9*`、`+0x04`/`+0x08` 外部 png 的字节与长度、
  `+0x0c` bpp、`+0x10` 回指 entry、`+0x14` 标志位 ✅。

---

## 9. v8 与更早版本的差异

**TH16 的加载器里没有版本分支。** 两处检查（`0x46d0e2`、`0x46d404`）都是
「`version == 8` 否则弹 `0x494280` 的错误框并返回 `-1`」——**没有 else 分支去按旧布局解析** ✅。
所以从 TH16 这个二进制上，「v8 与更早版本的差异」是**问不出来的**：它只会读 v8。

旧版本差异只能引社区单源（thtk `anm_types.h`，🟡，本仓未在任何旧 exe 上复核）：

| 断言 | 出处 | 可信度 |
| --- | --- | :---: |
| v0 = TH06；v2 = TH07；v3 = TH08/09；v4 = TH09.5/TH10；v7 = TH11–TH12.5；v8 = TH13+ | thanm `anm_types.h` | 🟡 |
| v7 及以前用 `anm_header06_t`：字段全是 u32，`version` 在 `+0x2c` 而非 `+0x00` | 同上 | 🟡 |
| v8 起改用 `anm_header11_t`：`version` 挪到 `+0x00`，计数/宽高/格式压成 u16 | 同上 | 🟡 |
| v8 内部又有增补：`jpeg_quality`(`+0x23`)、`w_max`/`h_max`(`+0x28`) 是 TH19 才启用 | 同上 | 🟡 |
| sprite 记录在 TH19 从 20 字节扩到 40 字节（多 5 个 float） | 同上 | 🟡 |

**能一手说的只有一句**：TH16 用的 v8 布局就是本文 §3–§6，且 TH16 对 `+0x23`/`+0x28..0x40`
一个字节都不读 ✅ —— 与「这些是后来的作品才启用的空位」相容，但**不构成对 TH19 的证明**。

---

## 10. 校验：拿真实文件跑解析器

按 §3–§6 写的解析器跑遍 `local/th16.v1.00a/dat/*.anm`（54 个文件，ZUN 版权数据，不入库）：

| 检查项 | 结果 |
| --- | --- |
| entry 链表走通、`version` 全 = 8 | 54 文件 / 423 entry ✅ |
| `name_offset == 0x40 + 4*n_sprites + 8*n_scripts` | 423/423 |
| `+0x08`、`+0x23`、`+0x28..0x40` 全零 | 423/423 |
| sprite 记录步长 20 字节、`id` = 全局序号 | 全部吻合，0 处例外 |
| 指令流走通、`size` 全为 4 的倍数 | 43268 条指令 ✅ |
| 每个 script 以 `opcode = -1` 结束，且下个 script 起点 = 该条 + 8 | 1929 个 script ✅ |
| THTX magic = `"THTX"`、`size == w*h*bpp` | 417/417 |
| `x_offset` / `y_offset` 实测取值范围 | `[0, 1024]` / `[0, 448]`，无一 ≥ `0x8000` |
| 名字串是否含非 ASCII 字节 | 0/423 |
| `param_mask` 用到的最高位 | bit 8（4 条 `ins_420`，mask `0x180`/`0x186`，10 个参数） |

**逐字节对账**：把解析结果按 thanm 的写法打印出来，与 T0 解出的 thanm `-l` 文本
（`local/th16.v1.00a/anm/<名>/<名>.anm.txt`）逐条比对——挑了 `pl02sub`（2 entry / 5 script，小）、
`enemy`（4 entry / 172 script，多 entry）、`effect`（13 entry / 198 script，多 script），
另加 `ascii`（290 sprite）与 `pl04sub`。指令号、`time`、参数值、`param_mask` 标出的变量参数
（thanm 的 `%F0` / `$I3`）**全部一致**，`jmp(offset136, 16)` 的目标 136 也与「第 10 条指令
距 script 起点 136 字节」吻合 ✅。

---

## 11. 还没搞清楚的

| 问题 | 状态 |
| --- | :---: |
| `+0x08` / `+0x18` / `+0x23` / `+0x28..0x40` 在**整个** exe 里是否真无人读（只查了加载路径） | ❓ |
| `format` 0/2/4/6/8 只有查表项、没有代码路径证据（真实数据里仅 6 出现 1 次），见 §7 的分层可信度 | 🟡 |
| `param_mask` 的 bit 9–15：exe 里从没测试过，真实数据也从没置位——是保留位还是别作用到的 | ❓ |
| `D3DFMT_A8` 贴图在 TH16 的 texture stage 里从哪取 color（推测 `D3DTA_DIFFUSE`，未在 exe 上证） | 🟡 |
| opcode 语义（137 个 case）—— 本文只定死了指令**头**，不含语义 | — 归下一份文档 |
| `AnmVm` 0x5fc 字节的字段图 | — 归下一份文档 |

---

## 关联

- 子系统总览：[`../OVERVIEW.md`](../OVERVIEW.md)
- 本作入口与起步锚点：[`README.md`](README.md)
- 想要的 Ghidra 重命名（待 T10 统一落库）：[`naming-batch.md`](naming-batch.md)
- 纪律：[`../../../METHOD.md`](../../../METHOD.md)、[`../../../DOCSTYLE.md`](../../../DOCSTYLE.md)
