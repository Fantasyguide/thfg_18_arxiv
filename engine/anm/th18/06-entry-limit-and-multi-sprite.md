# anm/th18 §6 — entry 上限 256 的真身（贴图键低 8 位）× 一个 entry 多 sprite × 贴图尺寸 × scale 缩的是谁

> **版本**：TH18 v1.00a（`th18.exe`，imagebase `0x400000`，md5 以 `games/th18.v1.00a/symbols.json` 为准）。
> 本文所有裸地址都属该版本；引用其他版本须写成 `th16:0x…`。
> **方法**：一手平坦反汇编（PyGhidra 只读开库：`dump_func_asm.py` / `decompile_funcs.py` / 自写扫描脚本）+
> 直读 `th18.exe` 的 RVA→文件偏移拿字符串与 IAT 名（`CreateFileA` / `D3DXCreateTexture` / `D3DXLoadSurfaceFromMemory` /
> `D3DXCreateTextureFromFileInMemoryEx`）+ 原生 anm 数据普查（`thanm -l 18`，56 个文件）+ 我们的构建产物 `ability.anm` 普查。
> **缘起**：card-expand 的「贴图错位」根因假设复核（用户 2026-09-13）。任务书四问，逐条给结论 + 证据。
> **可信度**：§0 四问全 ✅ 一手（每条都有写入点 + 读取点两端）；§3 的「引擎别处也绝不检查尺寸」是 🟡；
> 没做到的列在 §6。

## 0. 一句话结论（四问速查）

| # | 问题 | 结论 | 关键地址 | 可信度 |
| --- | --- | --- | --- | --- |
| 1 | 256 卡在哪 | **卡在「贴图键」的低 8 位 = entry 下标**：键 = `(anm 槽号 << 8) \| entry 下标`，绑定贴图时 entry 只取低 8 位 | 打包 `0x486d38`、解包 `0x47e1a4` | ✅ |
| 1b | 上限是 255 还是别的 | **一个 anm 文件最多 256 个 entry**（下标 0..255）；**sprite 号不受限**（dword） | 同上 | ✅ |
| 2 | 一个 entry 多个 sprite | **完全合法，且是原生的常态**：sprite 表是全文件扁平的，按 `Σ(每个 entry 头的 u16 sprite 数) × 0x44` 一次性 malloc | malloc `0x4866a7`、无原生上限 | ✅ |
| 3 | 贴图尺寸要 2 的幂吗 | **不要**。加载路径对 w/h **零检查**：纹理尺寸 = entry 头的 `w/h`（按需折半），像素数据 1:1 拷到左上角 | `0x486488`、`0x486515`、`0x486190` | ✅ |
| 4 | `scale.x` 缩谁 | 缩 **sprite 自己的矩形**（`vm+0x74`），不是整张画布；`vm+0x74` = sprite 矩形宽（画布像素） | `0x4816fe`–`0x481719` | ✅ |

**⇒ 与我们假设的关系**：`engine/anm/th18/01-vm-instantiate.md` §3a2 的方向**对**（entry 序号 8 位、sprite 号没事、
修法是「多张图并进一个 entry」），本次把它从「统计 + 实跑合证」升级为**机制一手**，并**订正三处读法**（见 §7）。

## 1. Q1 —— 256 卡在哪：**打包贴图键的低 8 位**

### 1.1 完整链（entry/sprite → D3D 贴图）

```
① anm 文件头 (u16 sprites / u16 scripts) + entry 表 + sprite 表
        ↓  0x486bc0（每个 entry 解析一遍）
② sprite[idx] +0x08 = 贴图键 = (AnmLoaded+0x00 = 本 anm 槽号) << 8 | entry 下标
        ↓  VM 执行 sprite(N) → AnmLoaded__set_sprite 只把 N 写进 vm+0x24（dword），键留在 sprite 表里
③ FUN_0047dce0（draw 路径：把一张 VM 画成四边形）读 sprite[vm+0x24]+0x08
        ↓  解包：>>8 = 槽号 → mgr->anms[]；&0xff = entry 下标 → entry 表
④ entry 表[idx*0x18 + 0x00] = IDirect3DTexture9* → SetTexture(0, tex)
```

| 环节 | 地址（th18 裸地址） | 一手证据 |
| --- | --- | --- |
| 解析器入口（每个 entry 一次） | `0x486bc0` | 调用点 `0x486969`（`FUN_00486920` 里逐 entry 调用，参数 5 个） |
| **打包** | `0x486d30`–`0x486d4a` | `MOV EAX,[ESI]`（AnmLoaded+0 = 槽号）→ `SHL EAX,0x8` → `OR EAX,EDI`（EDI = entry 下标） |
| sprite 结构落盘 | `0x486e08`–`0x486e2a` | 4 条 `MOVUPS` 把 0x44 字节写进 `[anm+0x11c] + idx*0x44` |
| **解包 + SetTexture** | `0x47e16e`→`0x47e1b6` | `SAR EAX,0x8`（槽号）/ `MOVZX EAX,CL`（★ entry 低 8 位）/ `PUSH [EAX+ECX*8]` / `CALL [ESI+0x104]` |
| anm 槽号从哪来 | `0x486850`（`FUN_004867e0`） | 装载时 `MOV dword ptr [ESI],EDI` 把槽号写进 AnmLoaded+0x00；槽号上限 `< 0x21`（`0x4867fe`） |

打包处（`0x486bc0` 内，sprite 填表循环的第一条）：

```asm
486d30  MOV EAX,dword ptr [ESI]        ; ESI = AnmLoaded；+0x00 = 本 anm 的槽号（装载时写的）
486d32  MOV dword ptr [EBP + 0xffffff74],EAX
486d38  SHL EAX,0x8                    ; ★ 槽号 << 8
486d3b  OR  EAX,EDI                    ; ★ | entry 下标（EDI = 参数 2 = 第几个 entry）
486d3f  MOV dword ptr [EBP + 0xffffff7c],EAX   ; → sprite+0x08（贴图键）
486d4a  MOV dword ptr [EBP + 0xffffff78],EDI   ; → sprite+0x04（下标原样存一份，dword）
```

解包处（`FUN_0047dce0`，绑贴图那一段）：

```asm
47e15f  MOV ECX,dword ptr [EDI + 0x24]      ; vm+0x24 = sprite 序号（dword）
47e162  SHL ECX,0x4
47e165  ADD ECX,dword ptr [EDI + 0x24]      ; ×0x11 → 再 ×4 = idx*0x44
47e168  MOV EAX,dword ptr [EDX + 0x11c]     ; sprite 数组
47e16e  MOV EAX,dword ptr [EAX + ECX*0x4 + 0x8]   ; ★ 键
47e172  CMP dword ptr [ESI + 0x3120e04],EAX ; 与「当前已绑的键」比（全 32 位）
47e178  JZ  0x0047e1c2                      ; 没变就不重绑
47e187  MOV ECX,dword ptr [ESI + 0x3120e04]
47e198  SAR EAX,0x8                         ; ★★ 高 24 位 = anm 槽号
47e19d  MOV EDX,dword ptr [EDX + EAX*0x4 + 0x312072c]  ; anm = mgr->anms[槽号]
47e1a4  MOVZX EAX,CL                        ; ★★★ 低 8 位 = entry 下标（>255 的位被丢掉）
47e1a7  LEA ECX,[EAX + EAX*0x2]             ; ×3
47e1aa  MOV EAX,dword ptr [EDX + 0x124]     ; entry 表
47e1b0  PUSH dword ptr [EAX + ECX*0x8]      ; entry[idx*0x18 + 0x00] = IDirect3DTexture9*
47e1b6  CALL dword ptr [ESI + 0x104]        ; IDirect3DDevice9::SetTexture(0, tex)
```

**结论**：
- 8 位字段不是「某个结构体字段声明成 8 位」，而是**32 位贴图键里的字段划分**：`槽号(24位) | entry(8位)`，
  打包时用 `SHL 8 / OR`，解包时用 `SAR 8 / MOVZX CL`。**能表示的 entry 下标 = 0..255 ⇒ 每个 anm ≤ 256 entry**。
- entry 下标 ≥ 256 时**回绕**（256→0、257→1、…），所以画面是「别的贴图」而不是「白块 / 不画」：
  键本身仍与缓存不同（`0x47e172` 比的是全 32 位），所以**会重绑、只是绑错**——与实跑症状（画成别的图的一个窗口）逐条吻合。
- 同一套解包在**五个**绑定点上一模一样，都是 8 位：`0x47e198`/`0x47e1a4`（`FUN_0047dce0`）、
  `0x480819`/`0x480823`（`FUN_00480160`）、`0x480d70`（`FUN_00480a50`）、`0x480fdd`/`0x480fee`（`FUN_00480f70`）、
  `0x481147`/`0x48115d`（`FUN_004810d0`）。⇒ 换哪条绘制路径都躲不过。
- 全 `.text` 里 `SHL reg,0x8` 紧跟 `OR` 的窄化打包**只有这一处**在 anm 子系统内（其余是卡牌/玩家/CRT 的无关位运算）。

### 1.2 为什么「sprite 号 >255 没事」——两条独立的计数

| 量 | 存哪 | 位宽 | 原生证据 |
| --- | --- | --- | --- |
| sprite 序号 | `vm+0x24`（dword）+ 全文件扁平 sprite 表下标 | 32 位 | `enemy.anm` 344 sprite / 脚本引用 `sprite342`；`bullet.anm entry0` 单 entry 368 sprite |
| sprite 序号（引擎自己的兜底值） | 指令解析失败时 `MOV EDX,0x120` = **288** | 32 位 | `0x4799fc`（`AnmVm__run` 的 `sprite` 分支） |
| **entry 下标** | 贴图键低 8 位 | **8 位** | 见 §1.1 |

引擎自己就拿 288 当兜底 sprite —— 这就是「sprite 号 >255 是原生常态」的一手铁证（`0x4799fc`）。

### 1.3 entry 表（`anm+0x124`，步长 `0x18`）字段图 —— **订正我们的旧读法**

| 偏移 | 内容 | 证据 |
| --- | --- | --- |
| `+0x00` | **`IDirect3DTexture9*`**（SetTexture 用的就是它） | 写：`0x486488`（D3DXCreateTexture 的 ppTexture）/ `0x486cc5`；读：`0x47e1b0` |
| `+0x04` | 该 entry 图像文件的**内存字节**（`CreateFileA`+`ReadFile` 读进来的） | `0x48674b` 存返回值；消费者 `0x486187`（`D3DXCreateTextureFromFileInMemoryEx` 的 pSrcData） |
| `+0x08` | 该图像文件的**字节数** | `0x486741` 存 `*out`；消费者 `0x486184`（SrcDataSize） |
| `+0x0c` | 每像素字节数（格式表 `0x4b4920[format]`） | `0x486cd2`、`0x486528` |
| `+0x10` | 原生 anm 文件里**该 entry 的指针** | `0x486c37` |
| `+0x14` bit0 | 「纹理已建/未建」标志（建前清 0） | `0x486ca9`、`0x486168`、`0x4863af` |

⚠ **旧文档（01 §0 表 + 04）写的 `+8` = 「该 entry 的 sprite 起点」是错的**：`+8` 是图像文件字节数。
引擎**不存**每 entry 的 sprite 起点——解析器用「跨 entry 累加的运行计数」当参数（见 §2.2）。

## 2. Q2 —— 一个 entry 多个 sprite：**合法，且原生就这么用**

### 2.1 加载器复核：`th18:0x4865b0`

```asm
48662d  MOVZX EAX,word ptr [ESI + 0x6]   ; 第 1 个 entry 头 +0x06 = u16 scripts
486633  MOVZX EDI,word ptr [ESI + 0x4]   ; 第 1 个 entry 头 +0x04 = u16 sprites
486642  MOV EAX,dword ptr [ESI + 0x24]   ; +0x24 = nextoffset（0 = 最后一个 entry）
486650  ADD ECX,EAX                      ; 顺着 nextoffset 走 entry 链
486653  MOVZX EAX,word ptr [ECX + 0x6]
486657  ADD dword ptr [EBP - 0x21c],EAX  ; Σ scripts
48665d  MOVZX EAX,word ptr [ECX + 0x4]
486661  ADD EDI,EAX                      ; Σ sprites
486673  MOV dword ptr [EBX + 0x110],EDX  ; anm+0x110 = entry 数
48667c  PUSH EDI / CALL 0x00491a34       ; malloc(entry 数 × 0x18)
486685  MOV dword ptr [EBX + 0x124],EAX  ; anm+0x124 = entry 表
486694  MOV EDI,dword ptr [EBP - 0x220]  ; = Σ sprites
48669d  MOV EAX,EDI / 48669f SHL EAX,0x4 / 4866a2 ADD EAX,EDI / 4866a4 SHL EAX,0x2   ; ×0x44
4866a8  CALL 0x00491a34                  ; ★ malloc(Σ sprite × 0x44)
4866ad  MOV dword ptr [EBX + 0x11c],EAX  ; anm+0x11c = 全文件扁平 sprite 表
4866b6  MOV EAX,[EBP - 0x21c] / 4866bc SHL EAX,0x2 / 4866c0 CALL 0x00491a34
4866c5  MOV dword ptr [EBX + 0x120],EAX  ; anm+0x120 = 脚本指针表（Σ scripts × 4）
48677d  IMUL ESI,dword ptr [EBP - 0x21c],0x60c / 486788 CALL 0x00491a34
486790  MOV dword ptr [EDI + 0x10c],EAX  ; anm+0x10c = 脚本体（Σ scripts × 0x60c）
```

⇒ **复核成立**：`malloc(总 sprite 数 × 0x44)`（`0x4866a7`–`0x4866ad`），总 sprite 数 = 逐 entry 累加 entry 头 `+0x04`
（**u16**，`0x48662d`/`0x486633` 用的是 `MOVZX word`），是 `dword` 累加，**没有窄化**。

### 2.2 sprite 起点怎么来的（不是存在 entry 表里）

- 解析器 `0x486bc0` 的第 3 个参数 = **本 entry 的 sprite 起始序号**，由调用者 `FUN_00486920` 跨 entry 累加（`0x48697e`：
  `MOVZX EAX,word[ESI+4]; ADD [ESP+0xc],EAX`）。
- 解析器里 `EDX = arg3 * 0x44`（`0x486d1c`–`0x486d25`），写 `[anm+0x11c] + EDX`（`0x486df7`、`0x486e0f`）。
- 第 4 个参数同理是脚本起始序号，写 `[anm+0x120] + arg4*4`（`0x486f05`–`0x486f0e`）。

### 2.3 `AnmLoaded__set_sprite`（`0x477b00`）算式复核 ✅

```asm
477b1a  MOV EAX,dword ptr [EBP + 0xc]      ; arg2 = sprite 序号（dword）
477b1f  SHL ECX,0x4                        ; ECX = EAX
477b22  ADD ECX,EAX                        ; ×0x11
477b24  MOV dword ptr [EDX + 0x24],EAX     ; vm+0x24 = sprite 序号（dword，无窄化）
477b27  MOV EAX,dword ptr [ESI + 0x11c]    ; sprite 表基址
477b2e  MOVSS XMM0,dword ptr [EAX + ECX*0x4 + 0x24]   ; + idx*0x44
```

⇒ 我们读作 `[anm+0x11c] + idx*0x44` **完全正确**（`ECX*0x4` 里的 ECX 已经是 ×0x11）。

### 2.4 **sprite 结构（0x44 字节）字段图**（解析器 `0x486bc0` 写、`0x477b00`/`0x47dce0` 读）

| 偏移 | 类型 | 含义 | 证据 |
| --- | --- | --- | --- |
| `+0x00` | i32 | 本 anm 的槽号 | `0x486d30` / `0x486e0f` |
| `+0x04` | i32 | entry 下标（原样存，引擎自己不读） | `0x486d4a`；无读取点 |
| `+0x08` | u32 | **贴图键 = 槽号<<8 \| entry 下标** | `0x486d3f`；读 `0x47e16e` |
| `+0x0c` | f32 | u（纹理像素）= `x × texW/entryW` | `0x486dc7` |
| `+0x10` | f32 | v（纹理像素）= `y × texH/entryH` | `0x486dd5` |
| `+0x14` | f32 | u2 = `(x+w) × texW/entryW` | `0x486de8` |
| `+0x18` | f32 | v2 = `(y+h) × texH/entryH` | `0x486e03` |
| `+0x1c` | f32 | 纹理高 | `0x486e1e` |
| `+0x20` | f32 | 纹理宽 | `0x486e1e` |
| `+0x24`/`+0x28` | f32 | u/texW、v/texH | `0x486e3f`、`0x486e6f` |
| `+0x2c`/`+0x30` | f32 | u2/texW、v2/texH | `0x486e57`、`0x486e87` |
| `+0x34`/`+0x38` | f32 | **sprite 矩形 高 / 宽（画布像素）** | `0x486ecb`、`0x486ea3` |
| `+0x3c`/`+0x40` | f32 | `texW/entryW`、`texH/entryH`（低分辨率缩放因子） | `0x486e26`（16B 存）+ `0x486d8d`/`0x486dbf` |

### 2.5' 订正（2026-09-14）：`ability.anm` 的 entry 数

本文后续段落里写的「我们的 build **367 entry**」是**图集合并之前**的旧数（当时正是它撞了 256 上限）。
合并后实测 **49 entry**（原生 7 + 追加 42，`thanm -l` 复核；`CAPACITY.md` §3.2 与 `native/tools/capcheck.py` 都按这个数走）⇒ 余量 207。

## 2.5 原生数据：一个 entry 放几百个 sprite 是常态

| 文件 | entry 数 | sprite 数 | 单 entry 最多 sprite |
| --- | --- | --- | --- |
| `bullet.anm` | 10 | 604 | **368**（entry0） |
| `ascii.anm` / `ascii_960.anm` / `ascii1280.anm` | 6 | 715 | **320**（entry0） |
| `enemy.anm` | 7 | 344 | 120（entry0） |
| `ability.anm`（原生） | 7 | 109 | 100（entry0） |
| `ability.anm`（**我们的 build**） | **367** | 469 | 1（一 entry 一 sprite） |
| `abcard.anm` | **118**（全表最大） | 118 | 1 |
| 全表（56 个文件） | 599 | 4772 | — |

⇒ **结论 ✅**：一个 entry 里放 N 个 sprite 完全合法（原生单 entry 最高 368），sprite 号连续即可；
sprite 号是**全文件连续**的（跨 entry 累加），不是 entry 内重新编号。旧文档写的「每 entry 最多 120」只是 `enemy.anm` 的最大值。

## 3. Q3 —— 贴图尺寸：**没有** 2 的幂要求，也没有任何尺寸检查

### 3.1 三条创建路径（IAT 名已解出）

| 场景 | 函数 | 调用 | 贴图尺寸 = |
| --- | --- | --- | --- |
| `hasData=1`（数据内嵌在 anm 里，原生与我们的 build 都是这条） | `0x486390` | `D3DXCreateTexture(device, entryW, entryH, 1, 0, fmt, pool, &entry.tex)` `0x486488` | **entry 头的 u16 `w`/`h`** |
| 承上：把 THTX 像素拷进贴图 | `0x486390` | `D3DXLoadSurfaceFromMemory(...)` `0x486515`（pDestRect = 图像自身矩形） | 1:1 拷到画布左上角 |
| `hasData=0`（图来自外部文件，如 `ability/*.png`） | `0x486140` | `D3DXCreateTextureFromFileInMemoryEx(device, entry+4, entry+8, 0, 0, …)` `0x486190` | Width/Height 传 0 ⇒ **取文件自己的尺寸** 🟡 |
| 名字以 `@`/`@R` 开头 | `0x486bc0` | `D3DXCreateTexture(device, entryW, entryH, …)` `0x486cc5`（`@R` 时 entry 的 w/h 先被屏幕尺寸改写，`0x486c5f`） | entry 头的 u16 `w`/`h` |

- 三条路径**都没有** `w & (w-1)` 之类的检查、没有向上对齐、没有断言；唯一的失败出口是
  「テクスチャが作成できません」（`0x4b8fc4` / `0x4b8f80`）——即 **D3D 自己建不出来才报错**。
  🟡 「整个 exe 里别处也不检查」这句是**推断**：我只逐条读了 anm 装载/取图的这三条路径（`0x4865b0`、`0x486bc0`、
  `0x486140`、`0x486390`），没有穷举引擎其余部分。
- 唯一的「尺寸会被改」是**低分辨率模式**：`lowResScale != 0` 且世界缩放 `[0x56aca0] < 2.0`（即非 1280×960）时，
  贴图与图像各乘 `scale × 0.5`（`0x4863f3`–`0x486464`）。这也是解析器算 `texW/entryW` 因子的**唯一原因**。

### 3.2 数据：原生非 2 次幂的尺寸遍地都是

| 类别 | 规模 | 例子 |
| --- | --- | --- |
| **内嵌图像（THTX）非 2 次幂** | **354 / 591** entry（60%） | `abcard.anm` `ability/abitem.png` = 40×44；`ability/BACK_max.png` = 256×320 |
| 同上（我们引用的那条） | — | `pl01.anm entry2` `pl01b2.png`：画布 128×256，**THTX = 96×256**，矩形 `(0,0,95,255)` |
| 同上（全身立绘） | — | `pl01.anm entry4..11` `face/pl01/*.png`：画布 512×256，**THTX = 282×216** |
| **entry 头（= 贴图尺寸）非 2 次幂** | 3 entry / 56 文件 | `help.anm entry1`（名 `@`，768×1024）；`text.anm entry2/3`（名 `@R`，640×480） |
| 非 2 次幂的**渲染面** | — | 1280×960 的离屏面（见 `engine/anm/th18/05-snapshot-and-surfaces.md`） |

⇒ **结论 ✅**：`pl01b2 = 96×256` 这条依据**成立但要说清量纲**——96×256 是**内嵌图像（THTX）**的尺寸，
不是画布/贴图尺寸（画布是 128×256）。真正的「贴图可以是非 2 次幂」证据是 `help.anm` 的 768×1024 与 `text.anm` 的
640×480/1280×960。**两条合起来**才说明「builder 里那条 2 的幂限制原本就不必要」。
⚠ 反过来说：`hasData=1` 时**画布 ≥ 图像**才合理（图像 1:1 拷到左上角，画布小了会溢出/裁掉）——
原生 60% 的 entry 都是「画布是 2 的幂、图像不是」，正是这个关系。

## 4. Q4 —— `scale.x` 缩的是**sprite 自己的矩形**

```asm
; AnmManager__draw_vm  0x481210（0x4816ed 一带，四个分支同形，另一处 0x481a6d）
4816fe  MOVSS XMM0,dword ptr [EDI + 0x74]   ; vm+0x74 = sprite 矩形宽（画布像素）
481703  MULSS XMM0,dword ptr [EDI + 0x54]   ; ★ × vm+0x54 = scale.x
481709  MOVSS dword ptr [ESP + 0x14],XMM0   ; 存进栈上的尺寸对（随后 0x4063d0 消费）
48170f  MOVSS XMM0,dword ptr [EDI + 0x78]   ; vm+0x78 = sprite 矩形高
481714  MULSS XMM0,dword ptr [EDI + 0x58]   ; ★ × vm+0x58 = scale.y
481719  MOVSS dword ptr [ESP + 0x18],XMM0
```

（同形第二处 `0x481a6d`/`0x481a72`；若 VM 挂了子 VM `vm+0x5c0`，子 VM 的 scale 会**再乘一次**这一对尺寸，
`0x48178b`–`0x4817b8`。⇒ 无论哪条路，缩的都是「矩形 × 各级 scale」。）

`vm+0x74/+0x78` 的来源（`AnmLoaded__set_sprite`）：

```asm
477b86  MOVSS XMM3,dword ptr [EAX + ECX*0x4 + 0x38]   ; sprite+0x38 = 矩形宽
477b8f  MOVSS dword ptr [EDX + 0x74],XMM3             ; → vm+0x74
477b94  MOVSS XMM2,dword ptr [EAX + ECX*0x4 + 0x34]   ; sprite+0x34 = 矩形高
477cf3  MOVSS dword ptr [EDX + 0x78],XMM2             ; → vm+0x78
477cf8  DIVSS XMM3,dword ptr [EAX + ECX*0x4 + 0x20]   ; w / texW
477cfe  MULSS XMM3,dword ptr [EAX + ECX*0x4 + 0x3c]   ; × (texW/entryW) = w/画布宽
477d04  MOVSS dword ptr [EDX + 0x454],XMM3
477d0c  DIVSS XMM2,dword ptr [EAX + ECX*0x4 + 0x1c]   ; h / texH
477d12  MULSS XMM2,dword ptr [EAX + ECX*0x4 + 0x40]   ; × (texH/entryH) = h/画布高
477d1a  MOVSS dword ptr [EDX + 0x468],XMM2
```

- `sprite+0x38` / `+0x34` = `(u2_px-u_px)/(texW/entryW)`、`(v2_px-v_px)/(texH/entryH)` = **文件里的矩形宽/高**
  （乘一次缩放因子再除回去，λ 掉了 `texW/entryW`）⇒ `vm+0x74` 与贴图/画布尺寸**无关**，只由 anm 里那条矩形决定。
- 顺带钉死：`vm+0x454/+0x468` = **UV 缩放** = `矩形/画布`（不是「画布/矩形」），送 `drawRect` 系配方用。

⇒ **结论 ✅**：`scale(2,2)` 的含义是「把这个 sprite 的矩形放大 2 倍」。
**把小图并进大画布不会改变脚本里 `scale()` 的观感**，只要合并后的矩形**紧贴**该美术（rect 的 w/h 保持原值）。
唯一会变的是 `vm+0x454/+0x468`（`矩形/画布` 变小）——那是给「1×1 白点画任意矩形」的配方用的，
我们若继续用 `sprite(N)` + `scale()` 就完全不受影响。

## 5. 给 builder 的人（`anmlib.make_entry` / `build_ability.py`）—— 一个 entry 放 N 个 sprite 的注意点

1. **`ORDER.txt` 的 entry 总数硬顶 256**（下标 0..255）。`ability.anm` 现在 367 entry ⇒ 后 111 个的贴图全部回绕。
   修法二选一：① 把多张图并进一个 entry（本报告证明合法）；② 拆到第二个 anm 槽（槽号是键的高 24 位，天然支持；
   但槽号上限 `< 0x21` = 33，见 `0x4867fe`）。
2. **sprite 号在 spec 里就是全文件连续的**：`spriteN` 的 N 不按 entry 重新计数——
   原生 `pl01.anm` entry0 是 `sprite0..52`、entry1 接着 `sprite53`；我们的 build 里 entry0 是 `sprite0..99`、
   entry1 从 `sprite100` 起。追加 entry 时 N 必须接在上一个 entry 的最后一个之后（引擎也按同一规则累加）。
3. **一个 entry 的多个 sprite 共用一条贴图**（贴图键挂在 entry 上，不在 sprite 上）。
   合并画布时把每个 sprite 的矩形**紧贴**它的美术——`scale()` / `drawRect` 都按矩形算。
4. **矩形可以超出画布**：原生 `pl01.anm entry0` 的 `sprite28/29` 是 512 宽，而画布只有 256 宽（UV 会 >1）。
   builder 不必（也不该）替引擎做这个 clamp。
5. **`hasData=1` 时画布 `w/h` = 贴图分配尺寸**，图像 1:1 拷到左上角：**画布 ≥ 图像**，
   其余区域留空（原生 60% 的 entry 都这么干）。画布不需要是 2 的幂（原生有 768×1024 的先例），
   但要 ≤ D3D 的 `MaxTextureWidth/Height`，否则会在装载时报「テクスチャが作成できません」。
6. **别用 entry 表 `+0x08` 当 sprite 起点**（那是图像文件字节数）。要算起点就自己按 entry 顺序累加。
7. **低分辨率模式会折半**：`lowResScale=1` 的 entry 在非 1280×960 下贴图变 `w/2 × h/2`，矩形坐标同步缩放
   （引擎自己算因子）。我们的 build 一律 `lowResScale: 0`（原生 append 也是 0）就与尺寸无关了。

## 6. 没查清的（❓）

1. `D3DXCreateTextureFromFileInMemoryEx(…, Width=0, Height=0, …)` 里 0 是否等价于「取文件尺寸」——
   参数映射是一手（15 个 push 逐个对上），但「0 = 从文件取」这条是 D3DX 的文档语义，没在机器上验。
2. `@R` entry 的 `w/h` 运行时被改写成 `[0x56ac80]`/`[0x56ac84]`（`0x486c5f`）——这两个全局在 `.data` 的非 raw 区，
   没找到写入点；按 `05-snapshot-and-surfaces.md` 的 1280×960 推断是屏幕尺寸 🟡。
3. entry 表 `+0x14` 的语义只判到 bit0（建纹理前清 0），其余位没读。
4. `vm+0x5ec` → 表 `0x4b48c0` 的 `sprite` 变体解析器（`0x4799d7`）我只确认了「返回值最终仍走 `set_sprite`」
   （`0x479a1a`），没有逐个展开 5 个变体；理论上不构成第二条贴图选择路径。
5. 我没有穷举整个 exe 找「别处有没有 8 位 entry 下标」：已查的是 5 个 SetTexture 点、entry 表的全部读取点、
   以及全 `.text` 的 `SHL …,0x8`+`OR` 打包点。若有走别的寄存器形态的窄化，本报告覆盖不到。
6. 「entry256 画出来具体是哪张图」没有实跑对照（只有机制 + 我们的 367 entry 数据 + 用户实跑症状三者吻合）。

## 7. 要回流到既有文档的订正（本次未改任何既有文件）

| 文件 | 现在写的 | 应改成 | 依据 |
| --- | --- | --- | --- |
| `engine/anm/th18/01-vm-instantiate.md` §0 表 | entry 表 `+8` = 「该 entry 的 sprite 起点」 | `+8` = 图像文件字节数；`+0` = 贴图，`+4` = 图像内存 | §1.3 |
| 同上 §3a2 | 「255 就是上限」/「entry 总数 ≤255」 | 下标上限 **255**（即每 anm **256 entry**），措辞统一到「下标 0..255」 | §1.1 |
| 同上 §3a2 | 「（每 entry 最多 120）」 | 原生单 entry 最高 **368**（`bullet.anm entry0`），120 只是 `enemy.anm` 的最大值 | §2.5 |
| `mods/th18.v1.00a/card-expand/AUDIT.md` §CC-1 #5 | 「entry 序号（贴图引用）是 8 位」🟡→ | ✅ 一手：`(槽号<<8)\|entry`，解包 `MOVZX EAX,CL`（`0x47e1a4`） | §1.1 |

## 8. 复现命令

```bash
source tooling/env.sh
python3 tooling/ghidra/scripts/dump_func_asm.py  th18 0x4865b0 0x486bc0 0x477b00 0x47dce0 0x481210
python3 tooling/ghidra/scripts/decompile_funcs.py th18 0x4865b0 0x477b00 0x486bc0
# 原生普查（entry / sprite / 尺寸）
grep -c '^entry'  local/th18.v1.00a/anm/*/*.anm.txt | sort -t: -k2 -n | tail
$THANM -l 18 mods/th18.v1.00a/card-expand/native/build/ability.anm | grep -c '^entry'   # 367
```
