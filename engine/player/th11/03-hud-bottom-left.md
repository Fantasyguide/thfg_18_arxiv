# TH11 左下角那块 HUD —— 蓝点 / 6 位最大得点「×信号强度」/ 信号条（**截断**的一手写法）

> **版本**：TH11 v1.00a（`th11.exe`，688,128 B，imagebase `0x400000`）。本文裸地址默认属该版本；引用其他版本须写成 `th10:0x…`。
> **方法**：一手平坦反汇编（`objdump -d -M intel`）+ 常量按 PE 节表 VA→文件偏移直读
> + `thanm -l 11 -m local/vendor/eclmap/anmmap/v4.anmm local/th11.v1.00a/dat/front.anm`（v4 map 自注「and version 7: SA, UFO, DS, GFW」）
> + 贴图像素直读（`front00.png` / `ascii.anm` 解出的 `ascii.png`）。
> **可信度**：四个 VM（脚本 32..35）与坐标/锚点 ✅；数字的**位数与格式串** ✅；「×」的字形身份 ✅；
> **信号条的一手填充写法（宽 + UV 两条一起写）**✅；信号条的两层里只有一层真画 ✅。
> **为什么放在 `player/`**：这块是 `zGui`（玩家侧运行时 UI），由自机位置与交信度驱动；
> `engine/` 没有 `hud/` 层，`engine/player/th11/` 已是 TH11 玩家侧一手结论的家。
> 交信度这个**量**与它的档位/闪色在 [`../../faith/th11/02-koushin-signal.md`](../../faith/th11/02-koushin-signal.md)；
> 本文只解**画面**。TH10 的对应块见 [`../th10/02-hud-bottom-left.md`](../th10/02-hud-bottom-left.md)，
> 合并规格见 [`04-merged-hud-spec.md`](04-merged-hud-spec.md)。
> ⚠️ 文件名是 `03-`（`02-` 已被 `02-support-special-effects.md` 占用，不覆盖既有文档）。

## 0. 一句话结论

左下角那一块 = `zGui` 里**连续 4 个 VM**（`Gui+0x2e4c`，stride `0x434`，装载循环 `th11:0x41a76a`–`th11:0x41a78e`
给它们装 `front.anm` **script 32..35**，每帧由 `th11:0x41b55d`–`th11:0x41b577` tick）：

| VM | 脚本 | sprite | `pos`（640×480）| anchor | layer | 是什么 |
| --- | --- | --- | --- | --- | --- | --- |
| 0（`Gui+0x2e4c`）| 32 | **sprite57**（48×16）| (42, 456) | (1,2) | 19 | **槽底**（`color(32,32,64)`）|
| 1（`Gui+0x3280`）| 33 | **sprite58**（48×16）| (42, 456) | (1,2) | 18 | **信号条本体**（宽度由引擎每帧写，见 §4）|
| 2（`Gui+0x36b4`）| 34 | sprite59（与 58 **同矩形**）| (42, 456) | (1,2) | 18 | 第二层 —— **引擎把它宽度写成 0 ⇒ 不画**（§4.4）|
| 3（`Gui+0x3ae8`）| 35 | **sprite26**（16×16）| (30, 468) | (1,2) | 18 | **蓝点图标** |

**最大得点那一行不是 VM，是 `AsciiManager` 文本**：引擎画 `"%.6d*%1.2f"`（`th11:0x41ce46`），
笔位 `(48, 455)`（`th11:0x4980d0` = 48.0f、`th11:0x498328` = 455.0f）——
⇒ **6 位、零填充**，`*` 就是**ASCII 字体的星号字形**（不是乘号贴图），后面 4 个字符是倍率。

★ **本任务重点（用户 2026-09-15 指出的那个错）**：信号条的「活动部分」原版是
**宽与 UV 一起写 ⇒ 截断（crop）**，不是只写宽度（那是伸缩）：
`VM+0x4c = 9 + 4×档`（quad 的像素宽）**并且** `VM+0x78 = VM+0x88 = uv0 + (9 + 4×档)/512`（右边的 u）。
一手在 `th11:0x41b679`–`th11:0x41b711`，见 §4.2。

## 1. 块由哪 4 个 VM 组成（一手）

```
41a764: mov ebp,[esp+0x14] / add ebp,0x2e4c        ; 组基址 = Gui+0x2e4c
41a774: mov edi,[ecx+0x444c]                        ; front.anm 的 zAnmLoaded*
41a77a: lea ebx,[esi+0x20]                          ; ★ script = 0x20 + i = 32..35
41a77f: call 0x44acd0                               ; AnmVm::set_script（写 +0x3a2 script / +0x3a8 instr）
41a785: add ebp,0x434 / cmp esi,0x4 / jb 0x41a770   ; 4 支，stride 0x434
```

- `anchor(1,2)` = **（左，底）**（取值表 0=中 / 1=左 / 2=右、0=中 / 1=顶 / 2=底；
  独立证法：`front.anm` script1..4 的边框条 sprite0/1/2/3 在 `pos(0,0)/(416,0)/(32,0)/(32,464)`
  正好铺满屏幕四边 ⇒ 只有「pos = 矩形左上角」成立；另：图标 `pos(30,468)` + 16 高若按「顶」锚会到 468..484，
  超出 480 ⇒ 2 只能是「底」）。
- ⇒ **槽底/信号条的矩形 = x 42..90 / y 440..456**（48×16）；**图标 = x 30..46 / y 452..468**（16×16）。
- 可见墨迹（`front00.png` 直读，阈值 α>40）：槽底 x 42..89 / y 445..454；信号条 x 43..88 / y 446..453；
  图标 x 32..44 / y 453..466（精灵框内相对 bbox：sprite57 (0,5)-(47,14)、sprite58 (1,6)-(46,13)、sprite26 (2,1)-(14,14)）。

## 2. 蓝点图标（用户口径「一个蓝点」）

| 项 | 值 | 一手 |
| --- | --- | --- |
| anm / 脚本 / sprite | `front.anm` script35 → **sprite26**（`(256, 80, 16, 16)`）| `thanm` 导出 + sprite 表 |
| 640×480 空间 | 框 **x 30..46 / y 452..468** | `pos(30,468)` + `anchor(1,2)` |
| 表面 px（×2）| **x 60..92 / y 904..936** | 分辨率倍率 |
| 颜色 | 贴图自带：`(50,54,200)` 蓝 + 黑描边 + 白高光 | `front00.png` 直读 |
| 层 | `layer(18)` | script35 |
| 行为 | `alpha(0)` + 20 帧淡入；中断 2 = 淡回 255 / 3 = 压暗到 64 | script35 |

**与 TH10 的蓝点是同一张美术**：TH10 用 `sprite29`、TH11 用 `sprite26`，两者都在 `(256,80,16,16)`，
像素也几乎一致（TH10 主色 `(51,51,204)`、TH11 `(50,54,200)`）⇒ 合并时**任取一张**即可（§合并规格）。

## 3. 最大得点：6 位零填充 + 「×」+ 倍率（一手）

绘制在 `Gui::on_draw_2a`（`th11:0x41c660`）里，`th11:0x41cd74`–`th11:0x41ce50`：

```
41cd74: mov DWORD PTR [ecx+0x18480],0xffffffff     ; AsciiManager 当前颜色 = 不透明白
41cd84: mov DWORD PTR [ecx+0x18498],2              ; 字体/宽度表索引
41cd94: mov DWORD PTR [ecx+0x1849c],1              ; 垂直对齐 = 顶
41cd9e: mov al,BYTE PTR [esi+0x31c3]               ; ★ Gui 的 HUD alpha
41cda8: mov BYTE PTR [ecx+0x18483],al              ; → 写进颜色的 A 字节
41cdae: mov eax,ds:0x4a56f4                        ; 交信度 → min(⌊v/100⌋,100) → ebx
41cdda: mov edx,ds:0x4a5754                        ; 擦弹 → ⌊g/100⌋ 钳 0x383 → 合流
41ce04: mov edx,ds:0x4a56f0                        ; ★ CURRENT_PIV → ÷100 → edi
41ce25: fild [esp+0x50] / fdiv QWORD ds:0x497e50 (double 100.0)   ; 倍率 = (擦弹项+交信度项)/100
41ce37: idiv ebx(10) ; 41ce43: sub edi,edx         ; ★ 最大得点截到 10 的倍数
41ce46: push 0x494198 ("%.6d*%1.2f")
41ce4b: call 0x4015a0                              ; AsciiManager::printf（位置由 ebx 指向的 (48,455,0)）
```

| 问 | 答 | 一手 |
| --- | --- | --- |
| 几位 | **6 位**（`%.6d`）| 格式串 `th11:0x494198` 原字节 `"%.6d*%1.2f"` |
| 前导零 | ✅ **有**（`%.6d` 零填充；值再在 `th11:0x41ce43` 截到 10 的倍数，如 50012 → `"050010"`）| 同左 |
| 字体 | **`ascii.anm`**（16×18 网格、字形 **14×14**；`'0'..'9'` = sprite16..25）| `thanm -l 11 dat/ascii.anm` |
| 字距 | **9 px**（= `[AsciiManager+0x184a4]` × scale.x；该字段初值 `9` 在 `th11:0x401055`，`scale.x/y = 1.0f` 在 `th11:0x41cd12`/`th11:0x41cd1b`）| `th11:0x401762: fild [ebx+0x184a4] / fmul [esi+0x110]` |
| 位置 | 笔位 `(48, 455)`，左对齐、顶对齐 ⇒ 墨迹 y ≈ 456..466 | `th11:0x4980d0` / `th11:0x498328` |
| 颜色 | **白**（`0xffffffff`，A 取自 `Gui+0x31c3`）—— 引擎**显式写**（与 TH10 相反）| `th11:0x41cd74` |
| **有没有乘号字形** | ✅ 有：`'*'` = 字体里的星号字形（字符 `0x2A` → 字形表下标 `0x2A-0x20` = 10；`ascii.png` 里 (140,0,14,14)，墨迹 7×8 偏上）| `th11:0x494198` 的 `*` 是**普通字符** |
| 被乘的是谁 | **最大得点**（那一行的 6 位数）：点道具得分 = `trunc10((A+B)×Y/100)`，`Y` 就是它 ⇒ 读作「最大得点 × 倍率」| [`../../faith/th11/01`](../../faith/th11/01-faith-and-points.md) §2.3 |
| 倍率的量纲 | `(min(⌊graze/100⌋,899) + min(⌊交信度/100⌋,100)) / 100` ⇒ `0.00..9.99`，**恰好 4 个字符** | 同左 + `th11:0x41f4e0`/`th11:0x41f640` |

⇒ 用户口径「最大得点 6 位『×信号强度』」逐条成立，**且「×」是 ASCII 星号**，不是贴图也不是「数字 + 图标」。
⇒ **倍率里同时含交信度项与擦弹项**（不是只有交信度）——这一点社区/口述常漏，代码是 `add edx,ebx`（`th11:0x41ce1f`）。

## 4. 信号条：位置、贴图、**填充方式**（★ 一手）

### 4.1 相对位置（核实用户口径「最大得点上面是信号强度」）

| 元素 | 640×480 空间 | 与数字那一行的关系 |
| --- | --- | --- |
| 槽底 + 信号条 | 矩形 x 42..90 / y 440..456（墨迹 y 445..454）| **在数字上面**（数字墨迹 y 456..466）✓ |
| 蓝点图标 | x 30..46 / y 452..468 | 在数字**左边**（数字从 x 48 起）✓ |
| 数字行 `"NNNNNN*M.MM"` | 笔位 (48,455)，墨迹 x ≈ 50..150 | — |

### 4.2 ★ 活动部分怎么画：**宽 + UV 两条一起写**（= 截断）

`Gui::on_tick`（`th11:0x41b679`–`th11:0x41b720`）逐条：

```
41b679: mov ecx,ds:0x4a56f4 / cmp ecx,0x2710 / jl … / mov ecx,0x64
        ; 槽 ≥ 10000 ⇒ 100，否则 ⌊槽/100⌋（= 显示的 0..130）
41b6a2: 魔数 0x66666667 → eax = level = min(⌊槽/100⌋,100)/10      ; ★ 0..10
41b6b3: mov [esp+0x10],eax / fild [esp+0x10]                      ; st0 = level
41b6c3: lea edx,[eax*4]                                           ; edx = level×4
41b6cf: fmul QWORD ds:0x498230  (double 4.0)
41b6d5: fmul QWORD ds:0x4982d0  (double 0.001953125 = 1/512)      ; = level×4/512
41b6db: fld   DWORD PTR [ebx+0x32f0]                             ; ★ = VM1+0x70 = uv_quad[0].x（= sprite58 的 u0）
41b6e1: fadd  QWORD ds:0x4982c8 (double 0.017578125 = 9/512)
41b6e7: faddp st(1),st                                            ; u_end = u0 + (9 + 4×level)/512
41b6f5: fst   DWORD PTR [ebx+0x32f8]                             ; ★ VM1+0x78 = uv_quad[1].x
41b6fb: fstp  DWORD PTR [ebx+0x3308]                             ; ★ VM1+0x88 = uv_quad[3].x（右两角一起改）
41b701: or    DWORD PTR [ebx+0x3684],8                           ; VM1 flags_lo |= 8
41b707: fild  [esp+0x10](= level×4)
41b70b: fadd  QWORD ds:0x498098 (double 9.0)
41b711: fstp  DWORD PTR [ebx+0x32cc]                             ; ★ VM1+0x4c = 9 + 4×level（quad 的**像素宽**）
41b717: or    DWORD PTR [ebx+0x3ab8],8                           ; VM2 flags_lo |= 8
41b71d: fldz / 41b71f: fst DWORD PTR [ebx+0x3700]                ; ★ VM2+0x4c = 0.0（第二层宽度归零）
```

`ebx = Gui`（`th11:0x41b6b7: mov ebx,edi`），`VM1 = Gui+0x3280`、`VM2 = Gui+0x36b4`（`0x3280-0x2e4c = 0x434`）。
偏移属于 TH11 的 `zAnmVm`（ExpHP `type-structs-own.json`）：`+0x4c __sprite_size`、`+0x70 uv_quad_of_sprite`、
`+0x404 flags_lo`。

**为什么这两条一起写才是「截断」**（一手，`th11:0x44fe30` = `write_sprite_corners__without_rot` 的头四条）：

```
44fe33: fld  DWORD PTR [esi+0x4c]      ; 宽（px）
44fe3c: fmul DWORD PTR [esi+0x3c]      ; × scale.x  ⇒ **quad 的像素宽 = 0x4c × scale.x**
```
而绘图用的 u 来自 `uv_quad_of_sprite`（`th11:0x44ff70` 起按 `flags_lo>>20 & 3` 选四个角的 u）⇒
**四边形的宽度**与**采样到的贴图宽度**是两个独立字段：

| 写法 | quad 宽 | 采样宽度 | 观感 |
| --- | --- | --- | --- |
| 只写 `+0x4c`（= 我们 111 曾经的写法）| 变窄 | **不变**（还是整张 48 px）| 美术被**横向压扁** = 伸缩 ✗ |
| **两个都写**（`+0x4c` + `+0x78/0x88`，**原版**）| `9+4×档` px | 同样 `9+4×档` px | **从左边裁掉一段** = 截断 ✓ |

**1:1 的算术自洽**：`u_end = u0 + (9+4×档)/512`，`512` = `front.anm entry0` 的贴图宽
（`front.anm.txt` 的 `THTXWidth: 512`）⇒ 采样 = `9+4×档` 个纹素，而 quad 也是 `9+4×档` px（scale = 1）
⇒ 纹素密度不变。**引擎自己也用同一个归一化**：`AnmVm::set_sprite` `th11:0x44ac8f`–`0x44ac98`
`fld [vm+0x4c] / fdiv [sprite+0x20]`（÷ 贴图宽）—— `sprite+0x20` 就是贴图宽。

| 项 | 真值 |
| --- | --- |
| 档 `level` | `min(⌊槽/100⌋, 100) / 10` ∈ **0..10**（槽 ≥ 10000 时 → 100 → 10）|
| 条宽 | `9 + 4 × level` px ⇒ **9 / 13 / … / 49**（槽 0 / 10.00 / … / 100.00+）|
| 起点 | 9 px（= 左端那个固定「头」的宽，`49 > 48` 时会向右边邻图溢 1 px，原版如此）|
| 满 / 档门限 | 档位闪色：显示值 `<100` → 常态白；`100..109` → 8；`110..119` → 9；`≥120` → 10（`th11:0x41b5a0`/`0x41b5d1`/`0x41b608`/`0x41b644`）|
| 前 20 帧 | 不发档位中断（`th11:0x41b593: cmp [edi+0x43cc],0x14`）|
| 色 | 由 anm 的 `color()` 给（槽底 `(32,32,64)`，条常态白，闪色由中断 8/9/10/11 切）|

档位与闪色的完整语义（含 2/3 的中断 = 整块淡入/压暗）见
[`../../faith/th11/02-koushin-signal.md`](../../faith/th11/02-koushin-signal.md) §6，本文不重复。

### 4.3 → 给我们（111）的落地对照

| 项 | 原版（TH11）| 我们的等价物 |
| --- | --- | --- |
| 缩到 `f`（0..1）| `+0x4c = f×48` + `uv 右端 = u0 + f` | **`ce_anm_set_clip_x(id, f, 1.0f, 1.0f)`**（`scale.x = f` + `uv_scale.u = f` + `flags_lo \|= 0x18`）|
| `f` 的分母 | 48（sprite 自己 48 px 宽）| **该 sprite 的矩形宽**（我们 `SIGNAL_GAUGE_BAR` 是 **64**，不是 48 —— 见合并规格 §3.1 ⚠）|
| 左端钉住 | `anchor(1,2)` = 左下 | 同（脚本 213 已照抄 `anchor(1,2)`）|

### 4.4 两条「第二层」其实不画（对既有文档的订正）

`th11:0x41b71f` 把 **VM2（script34 / sprite59）的 `+0x4c` 写成 0.0**，而同一条路径**没有**任何地方再把它写回来
（全 exe 对 `Gui+0x3700` 只有这一处写、对 `Gui+0x3ab8` 只有这一处 `\|= 8`）⇒ 第二层每帧都是 0 宽 ⇒ 不可见。
所以 ⚠️ [`../../faith/th11/02-koushin-signal.md`](../../faith/th11/02-koushin-signal.md) §6.1 的「两层增长条」
应订正为：**原版只有一层增长条**（script33 / sprite58），script34/sprite59 是留在数据里的第二层（同 UV、不同底色），
引擎每帧把它归零 —— 复刻时**不必起这第四个 VM**。

## 5. 这一块的锚点与占用

| 元素 | 640×480 空间 | 表面 px（×2）| 尺寸（640 空间）|
| --- | --- | --- | --- |
| 蓝点图标 | 框 x 30..46 / y 452..468（墨迹 x 32..44 / y 453..466）| x 60..92 / y 904..936 | 16×16 |
| 槽底 | x 42..90 / y 440..456（墨迹 y 445..454）| x 84..180 / y 880..912 | 48×16 |
| 信号条（满档）| 从 x 43 起 `9+4×10 = 49` px ⇒ x 43..91（墨迹 y 446..453）| x 86..182 | 49×8（墨迹）|
| 数字行 | 笔位 (48,455)；11 字符 × 9 px ⇒ 墨迹 x ≈ 50..149 / y 456..466 | x ≈ 100..298 / y 912..932 | 99×11（步进）|
| **整块外接** | **x 30..149 / y 440..468** | **x 60..298 / y 880..936** | **119×28** |

与我们的货架锚点（[`HUD-LAYOUT.md`](../../../mods/th18.v1.00a/card-expand/HUD-LAYOUT.md) §1.2：表面 `(257,924)`）
的关系同 TH10：原版这一块的左沿在 30、底沿在 468（= 640 空间的 `(30,468)`），都不是我们的锚点线
⇒ 只能抄**块内相对几何**（见 [`04-merged-hud-spec.md`](04-merged-hud-spec.md) §2）。

## 6. 复现命令

```bash
# ① 4 个 VM 的装载（脚本号来源）与每帧 tick
objdump -d -M intel --start-address=0x41a764 --stop-address=0x41a792 local/th11.v1.00a/th11.exe
objdump -d -M intel --start-address=0x41b55d --stop-address=0x41b578 local/th11.v1.00a/th11.exe
# ② ★ 信号条的宽 + UV 写点
objdump -d -M intel --start-address=0x41b679 --stop-address=0x41b722 local/th11.v1.00a/th11.exe
# ③ 0x4c 是「quad 像素宽」的一手
objdump -d -M intel --start-address=0x44fe30 --stop-address=0x44fe60 local/th11.v1.00a/th11.exe
objdump -d -M intel --start-address=0x44ac8f --stop-address=0x44acba local/th11.v1.00a/th11.exe
# ④ 文本（格式串 / 笔位 / 字距字段）
objdump -d -M intel --start-address=0x41cd74 --stop-address=0x41ce50 local/th11.v1.00a/th11.exe
objdump -d -M intel --start-address=0x401740 --stop-address=0x401780 local/th11.v1.00a/th11.exe
python3 -c "
import struct;d=open('local/th11.v1.00a/th11.exe','rb').read()
rd=lambda v,n=4: d[0x65000+(v-0x400000-0x466000):][:n]
print(rd(0x494198,12))
print(struct.unpack('<f',rd(0x4980d0)), struct.unpack('<f',rd(0x498328)))
print(struct.unpack('<d',rd(0x498230,8)), struct.unpack('<d',rd(0x4982d0,8)), struct.unpack('<d',rd(0x4982c8,8)), struct.unpack('<d',rd(0x498098,8)))"
# ⑤ 脚本原文（v4 map；TH11 的 front.anm 是 version 7）
local/vendor/thtk/build/thanm/thanm -l 11 -m local/vendor/eclmap/anmmap/v4.anmm \
    local/th11.v1.00a/dat/front.anm | sed -n '/^script script32 {/,/^script script36/p'
# ⑥ 字体度量（ascii.anm 的 14×14 字形 / '*' = sprite10）
local/vendor/thtk/build/thanm/thanm -l 11 -m local/vendor/eclmap/anmmap/v4.anmm local/th11.v1.00a/dat/ascii.anm | head -40
```

## 7. 结论表 / 还缺什么

| # | 断言 | 可信度 | 版本 |
| --- | --- | --- | --- |
| 1 | 块 = `Gui+0x2e4c` 起 4 个 VM，脚本 32..35，`anchor(1,2)` = （左，底）| ✅ 一手 | v1.00a |
| 2 | 蓝点 = script35 → sprite26（16×16），框 x 30..46 / y 452..468 | ✅ 一手 | v1.00a |
| 3 | 最大得点 = `AsciiManager` 文本 `"%.6d*%1.2f"`，笔位 (48,455)，字距 9 px，**零填充 6 位** | ✅ 一手 | v1.00a |
| 4 | 「×」= **ASCII 星号字形**（字体 sprite10），不是乘号贴图 | ✅ 一手 | v1.00a |
| 5 | 倍率 = `(擦弹项 + 交信度项)/100`，被乘的对象是最大得点 | ✅ 一手（跨篇公式）| v1.00a |
| 6 | **信号条活动部分 = 宽与 UV 两条一起写 ⇒ 截断**（`+0x4c` 与 `+0x78/0x88`）| ✅ 一手 | v1.00a |
| 7 | 条宽 = `9 + 4×level`，`level = min(⌊槽/100⌋,100)/10` | ✅ 一手 | v1.00a |
| 8 | script34/sprite59 那一层被引擎每帧写 0 宽 ⇒ **不画**（订正既有文档的「两层」）| ✅ 一手 | v1.00a |
| 9 | 数字行右侧的实际终止 x（149）| 🟡 由「11 字符 × 9 px + 末字形墨迹宽」推出，未截图核对 | v1.00a |

**还缺什么**

- 🟡 数字行墨迹的精确右界与基线（§5 的 149/456..466）：需要一次截图取色核对（笔位与步进都是一手，只有「墨迹相对字形框的偏移」是贴图推的）。
- ❓ `[AsciiManager+0x18498] = 2` 这个「字体/宽度表索引」在别处的取值集合（只影响字距，不影响本块）。
- ❓ 中断 11（蓝↔品红闪）全 exe 找不到发送点（[`../../faith/th11/02`](../../faith/th11/02-koushin-signal.md) §6.3 已登记）。
