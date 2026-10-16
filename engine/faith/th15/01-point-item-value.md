# TH15 点道具身价（最大得点 / PIV）—— 载体、量纲、难度两表、全写点、蓝点分档公式

> **版本**：TH15 v1.00b（`local/th15.v1.00b/th15.exe`，md5 `bddd41a586e09e4447f795a14009d68a`，imagebase `0x400000`）。
> 本文裸地址默认属该版本；引用其他版本写作 `th13:0x…`。
> **方法**：一手平坦反汇编 `local/_scratch/th15_full.asm`（`objdump -D -M intel` 直接反汇编 `th15.exe` 的 PE，全 `.text`）
> + PE 节区 VMA 换算直读原始字节 + 全 exe **4 字节小端穷举**（口径见 §1.4）+ 关键分支另跑原始 objdump 复核
> + `bullet/item.png` 像素判色（`local/th15.v1.00b/anm/bullet/bullet/item.png`）。
> **可信度**：载体 / 量纲 / HUD 取值 / 难度两表 / 写点穷举 / 蓝点公式 / 「无第二项」= ✅；
> HUD 那个数的**画面标签**、item type 8–15 的**游戏内名称** = 🟡；
> **「初值那一段在运行时由哪条路径执行」= ✅ 已结案**（§3.3）：用户口径 **最大得点跨关不清零** ⇒ 该块不是每关跑。

## 0. 一句话结论

- **载体**：`0x4e7434` = `CURRENT_PIV`（ExpHP `zGlobalsInner +0x44`，`GLOBALS = 0x4e73f0`）。
  它**就是 HUD 上那个数**：HUD 画的整数 = `trunc10(CURRENT_PIV / 100)`（`0x437263`–`0x437284`）。
  上限 = `0x4e743c`（`MAXIMUM_PIV`，`+0x4c`）；另有 `0x4e7438`（`INITIAL_PIV`，`+0x48`，**只写不读**）。
- **量纲**：**存储 = 显示 × 100**。蓝点身价、擦弹一次 `+100` 存储 = **+1 显示**。
- **初值**：按难度表 ×100 —— `INITIAL_PIV = T550[diff]×100`（10000 / 100000）、
  `MAXIMUM_PIV = T538[diff]×100`（250000 / 500000 / 1000000），写入点 `0x43c21f`–`0x43c25b`（唯一）。
- **涨入口**（全 exe 只有 5 个活写点）：擦弹 `+1 显示`、章节奖 `+bonus/5000 显示`、
  F 道具（满 P 时）`+100 显示`、小道具 type 9/10 `+2 显示`、小道具 type 11 `+20 显示`。
  ⚠ **2026-09-18 可达性订正**（见 §4.6）：**type 8 / 9 / 11 在原版数据里没有生成点**（引擎代码在、掉落链与 ECL 都不产生它们）——
  真正活的只有 **type 10（消弹掉的小道具，`+2`）**；miss 撒落的是 **type 1 ×7（P 小道具）**，不是 type 9。
  **miss / 灵击 / Boss 奖 / 碎片都直接不写 PIV**（负结论，已穷举）；★ 2026-09-18 订正：**miss 也没有间接入口** ——
  它撒的是 **type 1 ×7（P 小道具）**，不涨 PIV（旧稿写「撒 type 9」是错的，见 §4.6 的可达性穷举）。
- **蓝点 = item type 2**（像素坐实：蓝底 + 白字「点」）。被收时分值 = **单值、分两档**：
  线上/自动回收 = `trunc10(PIV/100)`；线下 = `trunc10( (3/4)·T·(1 − F/450) )`，`T = trunc10(PIV/100)`、
  `F = (int)玩家 y − 128`（`[0x4e7404] == 1` 时线是 148）。**没有乘法器、没有加数项**。
- 入账得分 = **`身价 / 10`**（`CURRENT_SCORE += value/10`，封顶 `0x3b9aca00`）。

## 1. 载体、量纲与使用点

### 1.1 字段表（ExpHP 名 + 一手对锚）

ExpHP `local/vendor/th-re-data/data/th15.v1.00b/type-structs-own.json` 的 `zGlobalsInner` 给的名字
（基址 `0x4e73f0`，一手对锚：`0x43c210` `mov ecx,0x4e73f0` 当 this 传 `0x43e6d0`；`0x43e8d7` `push 0x4e73f0` 给 `memset`）：

| 偏移 | 绝对地址 | ExpHP 名 | 本链里的角色 |
| --- | --- | --- | --- |
| `+0x40` | `0x4e7430` | `NUM_POINT_ITEMS_COLLECTED` | 收蓝点时 `inc`（`0x44074b`） |
| `+0x44` | `0x4e7434` | `CURRENT_PIV` | ★ 载体（最大得点） |
| `+0x48` | `0x4e7438` | `INITIAL_PIV` | 初值；**全 exe 无读点**（见 §3.2） |
| `+0x4c` | `0x4e743c` | `MAXIMUM_PIV` | 夹取上限（4 处 `cmovg`） |
| `+0x50` | `0x4e7440` | `CURRENT_POWER` | 道具 handler 用（与 PIV 无算式耦合） |
| `+0x54` | `0x4e7444` | `__CURRENT_POWER_COPY` | 同上（满 P 判据） |
| `+0x58` | `0x4e7448` | `POWER_PER_LEVEL` | HUD 的 `idiv` 除数 |
| `+0x74` `+0x7c` | `0x4e7464` `0x4e746c` | `__unknown` | 收点时被 `add`/`inc`，**零读点**（见 §6） |
| `+0x84` | `0x4e7474` | `LAST_POINT_OR_POWER_COLLECTED_POS` | 收点时抄一份坐标（`zFloat3`） |

> ⚠ 与 TH12 的情况**相反**：`engine/faith/th12/01-point-item-value.md` §1.3 里 ExpHP 的 `CURRENT_PIV` 是死字段；
> TH15 这个 `0x4e7434` 是**活载体**（6 个真读点、5 个写点都在活代码里，§1.4 / §4）。

### 1.2 量纲与 HUD 取值（✅ 一手）

HUD 在 `0x436990`（`Gui::on_draw`）里逐行画数字。最大得点那一行：

```
437263  eax = 0x51eb851f                       ; ← ÷100 魔数
437268  imul DWORD PTR ds:0x4e7434             ;   edx:eax = PIV × magic
43726e  sar  edx,5
437271  ecx = edx ; shr ecx,0x1f ; add ecx,edx ; ★ ecx = PIV / 100（有符号除）
437278  eax = ecx ; cdq ; idiv esi             ;   esi = 10（0x4371f7 mov esi,0xa）
437280  sub  ecx,edx                           ; ★ ecx = trunc10(PIV/100)  ← 向下取到 10 的倍数
437282  push ecx ; lea eax,[ebp-0x20] ; push eax
437284  call 0x40c5c0                          ;   画整数（无格式串）
```

同一函数里相邻行的量（用来定位这一格）：

| y | 常量 | 画什么 | 地址 |
| --- | --- | --- | --- |
| 204.0 | `0x4d001c` | ★ `trunc10(PIV/100)`，右对齐(`+0x19248=2`)、色 `0xff40c0ff`（淡蓝） | `0x437263`–`0x437284` |
| 226.0 | `0x4d0028` | `GRAZE_IN_CHAPTER`（`0x4e7420`） | `0x4372d5` |
| 243.0 | `0x4d0030` | `GRAZE_TOTAL`（`0x4e741c`） | `0x437330` |
| 182.0 | `0x4d000c`（x=540 `0x4d0080`） | power `"%d."`（`0x4e7440 / 0x4e7448`） | `0x4370e4`–`0x437100` |

- **发现**：`0x437268` 是 HUD 里唯一以 `0x4e7434` 为被乘数的 `imul`，其后紧跟 `÷10 → trunc10` 与整数直画。
- **验证**：① 全 exe 引用 `0x4e7434` 的只有 13 处（§1.4），其中落在绘制函数里的只有这一处；
  ② 同一函数里 226/243 那两行分别画 `GRAZE_IN_CHAPTER` / `GRAZE_TOTAL`，与
  [`../../player/th15/01-graze-and-item-slowdown.md`](../../player/th15/01-graze-and-item-slowdown.md) 的字段表一致；
  ③ 量纲 `÷100` 与擦弹的 `+0x64`（§4）自洽 —— 存储 100 = 显示 1。
- **结论**：**HUD 上那个数 = `trunc10(CURRENT_PIV/100)`**，单值，10 的倍数 ✅。
- 🟡 **未坐实的一步**：「这一格在原版 UI 上的文字标签是『最大得点』」—— 标签画在 HUD 贴图里，
  本文没做 OCR；依据 = ExpHP 字段名 + 该值 100% 由 PIV 决定 + 与 TH10/11/13 同构（见 `../OVERVIEW.md`）。
- 另有一个独立取值函数 `0x43b3e0`（`eax = trunc10(PIV/100); ret`，`0x43b3e0`–`0x43b405`）：
  **全 exe 零调用**（`call 0x43b3e0` 0 处）⇒ 死代码，HUD 用的是内联副本（同 TH13 §4 的情形）。

### 1.3 擦弹那 100 的确切指令（并勘误一处既有地址）

```
4573df  mov ecx,DWORD PTR ds:0x4e7434   ; PIV
4573e5  add ecx,0x64                    ; ★ +100 存储 = +1 显示
4573e8  cmp ecx,DWORD PTR ds:0x4e743c   ; MAXIMUM_PIV
4573ee  cmovg ecx,DWORD PTR ds:0x4e743c ; ★ 夹取
4573fc  mov DWORD PTR ds:0x4e7434,ecx   ; ★ 写回
```

⚠ **勘误**：既有文档 `engine/player/th15/01-graze-and-item-slowdown.md` §1 把这条写成 `` `0x4573f5` ``。
按**指令边界**复核，`0x4573f5` 是 `cmp BYTE PTR ds:0x503d86,0x0`（`80 3d 86 3d 50 00 00`），
**不是** PIV 写点；PIV 的写回在 `0x4573fc`。语义（「+100、被 MAXIMUM_PIV 夹」）是对的，地址差 7 字节。

### 1.4 穷举口径（可复核）

```bash
# 地址穷举：全文件 4 字节小端 + 落到"包含它的那条指令"上报（避免把立即数当引用）
python3 - <<'EOF'
import struct
d=open('local/th15.v1.00b/th15.exe','rb').read()
print(d.count((0x4e7434).to_bytes(4,'little')))   # 13
print(d.count((0x4e743c).to_bytes(4,'little')))   # 9
print(d.count((0x4e7438).to_bytes(4,'little')))   # 1
EOF
```

`0x4e7434` 的 **13 处**（逐条看过，全是 `mov ecx,ds:` / `mov ds:,ecx` / `imul ds:` 三种绝对形态）：
读 `0x437268`（HUD）、`0x43ad25`（章节奖）、`0x43b208`（float 加法器）、`0x43b3e5`（死 getter）、
`0x440617` `0x4406af`（蓝点公式两支）、`0x4573df`（擦弹）；写 `0x43ad58` `0x43b221` `0x43c25b` `0x440372` `0x4573fc`。
**没有** `lea` / 寄存器相对形态。

⚠ **穷举的边界（本文踩到了）**：这条口径只覆盖**写了绝对地址的指令**。
凡是「结构体整体 `rep movs` / `memcpy`」写进去的字段，绝对地址穷举**看不见** ——
TH15 的 PIV 正好有这么一条：`Globals::restore_snapshot` `0x43dee0`（§4.6）。这是 §3.3 未解项的根源。

## 2. 蓝点 = item type 2（载体身份，像素坐实）

### 2.1 收道具的分派

`ItemManager::on_tick__body` `0x43f830` 在收点处按道具类型跳表分派：

```
43feba  eax = [item+0xc68]            ; 类型（zItem 的 type 字段）
43fec1  eax -= 1 ; cmp eax,0xe ; ja default
43feca  jmp DWORD PTR [eax*4+0x440194]  ; ★ 15 项跳表（type 1..15）
```

跳表 `0x440194` 一手读出的全部 15 支：

| type | handler | 事件身份（下节详述） | 对 PIV 的影响 |
| --- | --- | --- | --- |
| 1 | `0x43fee3` → `call 0x4401d0`（ExpHP：`Item::collect_power`） | 小 P | 无 |
| **2** | **`0x43ff07` → `call 0x4405c0`** | **蓝点（点道具）** | 无（只拿分） |
| 3 | `0x43fef5` → `call 0x440420` | 大 P | 无 |
| 4 | `0x43ff13` → `call 0x44fa60` | 残机碎片 | 无 |
| 5 | `0x43ff21` → `call 0x44fa20` | 残机碎片（另一档） | 无 |
| 6 | `0x43ff2f` → `call 0x44fbc0` | 符卡碎片 | 无 |
| 7 | `0x43ff3d` → `call 0x44fb80` | 符卡碎片（另一档） | 无 |
| 8 | `0x43fed1` → `call 0x440330` | F（满 P 道具） | ★ 满 P 时 `+100 显示` —— ⚠ **原版不可达**（掉落链与 ECL 都不产出 type 8） |
| 9 | `0x43ff4b` | 「散落小道具」 | ★ `+2 显示` —— ⚠ **原版不可达**（唯一口 `th15:0x45b0a0` 的三累加器无自增点） |
| 10 | `0x43ff4b` | **消弹**掉的小道具（活） | ★ `+2 显示`（`gen_items_from_cancel` `th15:0x41e4b0`，10 个 caller） |
| 11 | `0x43ff5c` | ❓ 产出者未追（§9） | ★ `+20 显示` |
| 12 | `0x43ff94` | 野生型（`spawn_item` 把它改写成 4 或 6，见 §2.3） | 无 |
| 13 | `0x43ff6d` | 擦弹掉的小道具（§4.5） | 无（加 graze） |
| 14 | `0x43ff78` | ❓ | 无（加 graze） |
| 15 | `0x43ff83` | ❓ | 无（加 graze） |

### 2.2 像素判色：type 2 = 蓝底 + 白字「点」✅

- 道具的贴图归属：`0x418e00` `push "bullet.anm"`(`0x4cbac4`) `push 7 ; call 0x485f70 ; [item_manager+0x141f0d8] = eax`
  ⇒ 道具 VM 来自 **`bullet.anm`**（不是 front.anm）；贴图 = `bullet/item.png`（256×64）。
- 每型两个脚本号在表 `0x4cb058`（**8 字节/型**：`[+0]` = 落地动画、`[+4]` = 被收动画，`-1` = 没有）：

| type | `[+0]` script | sprite | `[+4]` script | sprite |
| --- | --- | --- | --- | --- |
| 1 | 112 | 500 | 126 | 501 |
| **2** | **113** | **502** | **127** | **503** |
| 3 | 114 | 504 (32×32) | 128 | 505 |
| 8 | 119 | 514 (32×32) | — | — |
| 9 / 10 / 11 | 120 / 121 / 122 | 516 / 517 / 518 | — | — |
| 13 / 14 / 15 | 123 / 124 / 125 | 516 / 517 / 518（scale 3.0，alpha 0→255 淡入） | — | — |

- `sprite502` 的矩形 = `x208,y0,16×16`；像素主色（`PIL` 直读 `item.png`）：
  **白 `(224,224,224)` 30 px + 蓝 `(32,32,192)` 21 px**；白字像素形状与 `sprite500`（type 1，红 `(224,0,0)`）
  明显不同、形如「点」（上「占」下「灬」）。
- 交叉：type 1 = 红底「P」（小 P，ExpHP 的 `Item::collect_power` 命名 + 颜色一致）；
  type 3/8 = 32×32 红底「P」/「F」（F = `FullPower`，其 handler 正是"满了就转成最大得点"§4.3）。
- **结论**：**type 2 = 蓝点（点道具）** ✅（贴图像素 + 分派表 + handler 语义三证）。
- 落地动画 `bullet.anm` script113：`sprite(502); originMode(1); type(1); angleVel(0,0,0.7853982f); scale(0.5,0.5); scaleTime(16,0,1.0,1.0)`；
  被收动画 script127：`sprite(503)`。同一文件里的 item 块 = script112–133。

### 2.3 道具类型由谁产出（与 §4 的事件身份有关）

`ItemManager::spawn_item` `0x4408d0(type, pos, ...)`，类型写进 `[item+0xc68]`（`0x4409fa`）。全部 12 个调用点：

| 调用点 | 类型 | 场景 |
| --- | --- | --- |
| `0x419de6` | `push 0xd` = 13 | `Bullet::sub_419a50` 的**擦弹**分支内（`0x419cd1` 调 `do_graze` 之后） |
| `0x41e585` `0x41e616` | `push 0xa` = 10 | `gen_items_from_cancel` `0x41e4b0`（**弹消し**掉道具） |
| `0x41e5d0` | `push 1` | 同上（小 P） |
| `0x424ec3` `0x428d4b` | 变量 | 敌机自带的掉落槽 `[enemy+0x519c]`（ECL 写） |
| `0x424fd3` | `lea eax,[ebx+1]`，`ebx` 从 0 起递增 → **连号 type（从 1 起）** | 敌机被击破的掉落环（个数 = 敌机掉落槽计数） |
| `0x454c8a` | 7 项数组 `[ebp+esi*4-0x20]` | 玩家侧（按机体/子机）掉落 |
| `0x45b0e7` `0x45b21b` | `push 1` | **miss 散落**（见 §4.4） |
| `0x45b14d` `0x45b1e1` | `push 9` | **miss 散落**（见 §4.4） |

> 敌机掉落环只出 type 1..7；type 11 / 14 / 15 的产出者**没找到**（❓，§9）。

## 3. 初值与上下限：难度两表

### 3.1 两张表（✅ 原始字节直读）

初始化在 `0x43c21f`–`0x43c25b`，索引 = `DIFFICULTY`（`0x4e7410` = `GLOBALS+0x20`，一手：它被同时用作
`%d.` 一行 power 的判据与章节奖的难度分支）：

```
43c21f  edx = ds:0x4e7410                 ; 难度索引
43c225  imul ecx,DWORD PTR [edx*4+0x4cc550],0x64   ; INITIAL 表 → ×100
43c22d  imul eax,DWORD PTR [edx*4+0x4cc538],0x64   ; MAXIMUM 表 → ×100
43c235  mov  ds:0x4e7438,ecx              ; INITIAL_PIV = T550[diff] × 100
43c23b  mov  ds:0x4e743c,eax              ; ★ MAXIMUM_PIV = T538[diff] × 100
43c240  movd xmm1,ecx ; cvtdq2ps
43c247  divss xmm1,DWORD PTR ds:0x4cffe8  ; ÷100.0f（= 回显示量纲）
43c24f  mulss xmm1,DWORD PTR ds:0x4cffe8  ; ×100.0f（= 回存储量纲）
43c257  cvttss2si eax,xmm1
43c25b  mov  ds:0x4e7434,eax              ; ★ CURRENT_PIV 初值（= INITIAL_PIV）
43c260  mov  eax,DWORD PTR [edx*4+0x4cc520] ; 第三张表
43c267  mov  ds:0x4e77a4,eax              ; → 0x4e77a4
```

| 难度索引 | 表地址 | `T550` 初值（显示） | `T538` 上限（显示） | `T520` → `0x4e77a4` |
| --- | --- | --- | --- | --- |
| 0 | — | 10000 | 250000 | 5 |
| 1 | — | 10000 | 500000 | 5 |
| 2 | — | 10000 | 500000 | 5 |
| 3 | — | 10000 | 500000 | 5 |
| 4 | — | 10000 | 500000 | 0 |
| 5 | — | 100000 | 1000000 | 0 |

- 表地址：`INITIAL_PIV` = `0x4cc550`、`MAXIMUM_PIV` = `0x4cc538`、第三张 = `0x4cc520`（都是 `int32[diff]`，直读原始字节）。
- 🟡 索引 4 / 5 对应的难度名未查（TH15 只有 4 个可选难度 + Extra；第 6 槽是 TH13/TD 同形的继承槽）。
- 🟡 `0x4e77a4`（`= T520[diff]`，5 或 0）的用途未追：它在玩家代码里被 `push`（`0x450b77`）、
  `cmp`（`0x45230f`）、`dec`（`0x452912`）——像一个按难度给的计数器，与身份价无算式耦合。
- 同一段还把 `GRAZE_TOTAL`（`0x4e741c`）与 `CURRENT_SCORE`（`0x4e740c`）清零、`CONTINUES_USED`（`0x4e7414`）
  按 `MODEFLAGS & 8` 处理，再 `call 0x43e6d0` 重置 power/bomb/碎片/收点计数
  ⇒ 这段是**「新开一局」的初始化**（不是每帧）。

### 3.2 `INITIAL_PIV` 是死字段（✅ 否定结论）

`0x4e7438` 全文件 **1 处**引用（`0x43c235` 写），**零读点**（4 字节穷举 + 无部分宽度访问，见 §3.3 的同一套检查）。
它只是初值的中间量（`CURRENT_PIV` 从它算出来）。⇒ 别把它当第二个「基准」。

### 3.3 ❓ 这段初始化在运行时到底由哪条路径执行（本文最重要的未解项）

**发现**：`0x43c1f0` 这个块（含 §3.1 的 PIV 初始化、§3.1 末尾的清零、`0x43e6d0` 的计数器重置）
在函数 `0x43bff0` 内，而 `0x43bff0` 只能从 `0x43c690`（`jmp 0x43bff0`）进入；
`0x43c690` 是在 **GameThread 构造**里被登记成线程入口的：
`0x43cbde push 0x43c690` → `0x44d7f0`（线程创建，内部 `call 0x4908d9`）→ 线程体 `0x43bff0`。

**验证**（逐条做过）：
1. 全文件搜 `0x43bff0` / `0x43c690` 的 4 字节小端：唯一命中就是 `0x43cbdf` 那条 `push`；
   对整段 `[0x43bff0, 0x43c695]` 做全文件窗口扫描，也没有别的指针/跳表指向它 ⇒ 只有这一个入口。
2. 块本身只被 `0x43c171` / `0x43c1a8` 两条 `jmp 0x43c1f0` 进入，而这两条都在
   `cmp ds:0x4e7ed8,0x0 / je 0x43c3e6`（`0x43c0e8` / `0x43c0ef`，原始字节 `83 3d d8 7e 4e 00 00`
   + `0f 84 f1 02 00 00`，已用独立 objdump 复核）的**非零**一侧。
3. 可是 `0x4e7ed8` **全 exe 没有任何写指令**：4 字节穷举 14 处**全是读**
   （`42ec18` `42f9b8` `432930` `4346a7` `43b540` `43c044` `43c0e8` `43e7f0` `43e802` `45b7b6` `45b7c5` `45cf8c` `45d0f8` `45d400`）；
   偏移 ±1..3 字节的部分宽度访问（`0x4e7ed9/da/db`）**0 处**；能覆盖它的 8/16 字节整块写在
   `[0x4e7ec9, 0x4e7ee0]` 里**一个立即数都没有**；`.data` 镜像里它的初值 = `0`。
   ⇒ **纯静态读法下这个块不可达**，而「最大得点会涨」是游戏的可见事实 ⇒ 必有我没抓到的写入路径。

**结论（❓）**：不判定。候选两条，各自都有反证：
- (a) `0x4e7ed8` 其实被写（例如经由指针/整块拷贝），只是我的穷举口径抓不到 —— 但我把
  「取地址」「部分宽度」「整块 store」「bulk copy 目的基址」四种形态都查过了，没找到；
- (b) **PIV/MAX 是被快照 memcpy 写进去的**（§4.6）：`Globals::restore_snapshot` `0x43dee0` 用
  `rep movs` 把 `0x1cc` 字节从快照 `0x4e75c8` 抄回 `0x4e73f0`，**覆盖 `+0x44`/`+0x48`/`+0x4c`** ——
  这条路绝对地址穷举看不见；但它的**种子**（快照本体）在 `.data` 镜像里全 0，且它的两个调用点
  （`0x413aab`、`0x43d612`）里 `0x43d612` 自己也被一个同样只读不写的 `0x4e7edc` 门住。

**这条为什么要紧**：如果这个块在每关开始时执行，则**最大得点每关重置到难度初值**；
如果执行的是 `je` 那一侧（`0x43c3e6`：只更新最高分 + 建管理器），则不重置。
两种口径下 PIV 的涨点算式一样，但「一局的总最大得点公式」不一样。
**建议的钉法**：动态断点 `0x43c23b`（写 MAXIMUM_PIV）与 `0x43df22`（快照恢复），
或只看 `0x43c0e8` 处 `[0x4e7ed8]` 的实值。

> ★ **2026-09-18 结案（用户口径）**：**TH15 的最大得点跨关不清零** ⇒ 走的正是 `je` 那一侧
> （`0x43c3e6`：只更新最高分 + 建管理器），`th15:0x43c1f0` 那个块**不是每关**执行
> （按上面的一手，它只在「新局 / 读档 / 快照恢复」这类时机跑）。
> ⇒ 本作「一局的总最大得点公式」用**累加**口径：PIV 从难度初值起，一局内跨关只增（除表里的夹取与快照恢复）。

> 方法论副产物（值得写进 `METHOD.md` 的候选）：**「全 exe 穷举写点」只能证明「没有别的 `mov` 写它」，
> 不能证明「没人写它」** —— 结构体整块拷贝（`rep movs`/`memcpy`）是穷举的盲区。

## 4. 涨 / 跌入口穷举（✅ 全 exe 写点）

### 4.1 一览

| # | 入口 | 写指令 | 增量（存储 / 显示） | 事件身份 |
| --- | --- | --- | --- | --- |
| 1 | 擦弹 | `0x4573fc` | `+100` / **+1** | `Player::do_graze` `0x4573a0`（子弹+3 种激光；敌机擦弹不减速） |
| 2 | 章节奖 | `0x43ad58` | `+(bonus/50000×10)×100` = `bonus/50` / `+bonus/5000` | `FUN_0043ab70`，每章结算 |
| 3 | F 道具（type 8） | `0x440372` | `+0x2710 = 10000` / **+100** | 满 P 时收 F 道具（⚠ 原版不可达，代码在） |
| 4 | 小道具 type 9 / 10 | `0x43b221`（经 `0x43b200`，x=`2.0f` @`0x4cff10`） | `+200` / **+2** | **消弹**掉的小道具（type 10，活）；type 9 不可达 |
| 5 | 小道具 type 11 | `0x43b221`（经 `0x43b200`，x=`20.0f` @`0x4cff98`） | `+2000` / **+20** | ❓ |
| 6 | 初始化 | `0x43c25b` | 置初值 | §3.3 ❓ |
| — | 死 setter | `0x43e61c` | `[ecx+0x44] = (int)(x×100)`（thiscall，`ecx=0x4e73f0`） | **零调用/零引用** ⇒ 死代码 |
| — | 快照恢复 | `0x43df22` | 整块 `rep movs` 0x1cc 字节（覆盖 `+0x44/+0x48/+0x4c`） | `Globals::restore_snapshot` `0x43dee0` |

### 4.2 章节奖（`0x43ad1f`–`0x43ad5a`）

```
43ad14  mov eax,0x14f8b589                        ; ← ÷50000 魔数
43ad1f  imul DWORD PTR [edi+0x12c]                ; edi = GUI，+0x12c = 本章 bonus
43ad25  mov ecx,ds:0x4e7434                       ; ecx = CURRENT_PIV
43ad2b  sar edx,0xc ; 43ad2e..43ad33 符号修正     ; eax = bonus / 50000
43ad35  lea eax,[eax+eax*4] ; 43ad38 add eax,eax  ; ×10
43ad3a  movd xmm0,eax ; 43ad3e cvtdq2ps
43ad41  mulss xmm0,xmm2                           ; xmm2 = 100.0f（`0x43ac70` movss xmm2,ds:0x4cffe8）
43ad45  cvttss2si eax,xmm0
43ad49  add ecx,eax
43ad4b  cmp ecx,ds:0x4e743c ; 43ad51 cmovg ecx,ds:0x4e743c
43ad58  mov ds:0x4e7434,ecx                       ; ★
```

⇒ 与 `engine/chapter/th15/01-chapter-and-bonus.md` §2a 的 `(bonus/50000)×10×100` 逐指令一致 ✅
（显示口径：`+bonus/5000`；bonus 1,500,000 → +300 显示）。

### 4.3 F 道具（type 8 = `0x440330`）：**满 P 才转成最大得点**

```
440337  esi = ds:0x4e7444        ; __CURRENT_POWER_COPY（满 P 判据）
44033e  edi = ds:0x4e7440        ; CURRENT_POWER
440347  cmp edi,esi ; jl 0x440398   ; 未满 → 走加 P 分支
44034b  edx = ds:0x4e7434        ; PIV
440357  edx += 0x2710            ; ★ +10000 存储 = +100 显示
44035d  cmp edx,ds:0x4e743c
440363  push 0xff40ff40          ; 飘窗色（绿）
440368  cmovg edx,ds:0x4e743c    ; ★ 夹取
44036f  push 0x64                ; 飘窗数值 = 100（显示口径）
440371  push item+0xc20          ; 位置
440372  mov ds:0x4e7434,edx      ; ★ 写回
440378  call 0x45ef60            ; PopupManager::generate_small_score_popup
440381  push 0xd ; call 0x476400 ; 音效
```

⇒ 「满 P 时收 F 道具 → 最大得点 +100（显示）」，并且**同帧飘窗显示的就是这 100**（`0x45ef60`）。

### 4.4 miss（★ 不直接写 PIV，但散落 type 9 —— 一条真入口）

函数 `0x45b090`–`0x45b23a`（`esi = 0x4e9bb8` 玩家）：

```
45b0b0  cmp DWORD PTR [esi+0x2c048],0x1f4   ; 阈值 500
45b0c0  eax = CURRENT_POWER ; idiv POWER_PER_LEVEL ; cmp eax,4
45b0e5  push 0x1 ; call 0x4408d0            ; 掉小 P
45b14b  push 0x9 ; call 0x4408d0            ; ★ 掉 type 9
45b158  add DWORD PTR [esi+0x2c048],0xffffffce   ; 每次 -0x32
45b1df  push 0x9 ; call 0x4408d0            ; ★ 又一档 type 9
45b219  push 0x1 ; call 0x4408d0
```

⇒ **miss 本身一票不写 PIV**（无处可写，§4.1 已穷举）；它把身上的 power/碎片转成一堆 **type 9** 道具，
每收一颗 `+2 显示`（§4.5）。这条链是「miss 之后最大得点会往上走一点」的一手解释 ✅ / 🟡（事件身份由代码邻接推）。
**跌的入口：没有。** 全 exe 没有对 `0x4e7434` 的减/乘/除写点；「跌」只有两处：
① 4 处 `cmovg` 夹到 `MAXIMUM_PIV`（PIV 只会被压回上限，不会掉）；
② 快照恢复（Pointdevice 重开点 → PIV 回滚到快照值，§4.6）。

### 4.5 两个 float 加法器与 type 9/10/11

`0x43b200`（自定义调用约定：float 在 `xmm1`，无 `this`）——**唯一被两个道具分支调用的 PIV 加法器**：

```
43b200  mulss xmm1,DWORD PTR ds:0x4cffe8   ; ×100（显示 → 存储）
43b208  ecx = ds:0x4e7434
43b20e  cvttss2si eax,xmm1
43b212  add ecx,eax
43b214  cmp ecx,ds:0x4e743c ; 43b21a cmovg ecx,ds:0x4e743c
43b221  mov ds:0x4e7434,ecx                ; ★ 写回
```

两个调用点（都在道具分派里）：

| 调用点 | `xmm1` | 效果（存储 / 显示） | 之后 |
| --- | --- | --- | --- |
| `0x43ff53`（type 9/10） | `2.0f` @`0x4cff10` | `+200` / **+2** | `push 0xa; call 0x420f50`（得分 +1） |
| `0x43ff64`（type 11） | `20.0f` @`0x4cff98` | `+2000` / **+20** | 同上 |

（`0x420f50` = `add_score(x)`：`CURRENT_SCORE += x/10`、封顶 `0x3b9aca00`。）

### 4.6 快照恢复（唯一能「整块」改 PIV 的路径）

```
43df15  ebx = edi + 0x1d8        ; edi = 0x4e73f0 → ebx = 0x4e75c8 = &snapshot_inner
43df1b  ecx = 0x73 (115 dwords = 0x1cc 字节)
43df20  esi = ebx
43df22  rep movs DWORD PTR es:[edi],DWORD PTR ds:[esi]   ; ★ 快照 → 活 globals（含 +0x44/+0x48/+0x4c）
```

- 对称的保存侧：`0x45b7e4` / `0x45cf9d` 也是 `0x73` dwords，从 `0x4e73f0` 抄到 `[obj+0x14]`。
- 调用点：`0x413aab`、`0x43d612`（后者被同样只读不写的 `0x4e7edc` 门住 → 该支路静态看也是死的）。
- **这就是「全 exe 穷举写点」不够用的地方**：PIV 可以被 `rep movs` 写，而穷举只看绝对地址（§1.4）。

## 5. 蓝点被收时的分值公式（✅ 逐指令）

### 5.1 函数与分支

`0x4405c0`（ExpHP 无名，本文建议 `Item::collect_point`；**全 exe 只有 `0x43ff09` 一个调用点** = type 2 的 handler）。

```
4405c0  prologue ；[ebp-4] = this(item)
4405c6  cmp DWORD PTR ds:0x4e7404,0x1     ; [GLOBALS+0x14]，ExpHP: CHARACTER
4405cd  eax = 0x80 (128)                   ; 普通线
4405d2  edx = 0x94 (148)
4405da  cmove eax,edx                      ; ★ 线 = (CHARACTER==1) ? 148 : 128
4405df  edi = ds:0x4e9bb8                  ; PLAYER（zPlayer+0x618 = zPlayerInner.pos）
4405e5  xmm0 = (float)base
4405ef  xmm1 = [player+0x61c]              ; ★ 玩家 y（zPlayerInner.pos.y）
4405f7  comiss xmm0,xmm1
4405fa  jae 0x4406a5                       ; ★ base >= y → 线上档（全额）
440600  cmp DWORD PTR [item+0xc64],0x3     ; 道具状态 3 = 线上自动回收
440607  je  0x4406a5                       ; 状态 3 → 也走线上档
        ; 否则 → 线下档（0x44060d 起）
```

- 状态 3 的来源同源：同文件 `ItemManager::on_tick__body` 里用**同一个 128/148**判据自动回收
  （`0x43f863` `ecx=0x94`、`0x43f90c` `eax=0x80`、`0x43f91b` `comiss xmm0,[player+0x61c]`、`0x43f922 ja 0x43fcda`），
  在那里 `0x43fce9` 把 `[item+0xc64] = 3`。⇒ 判据与收点判据是**同一条线** ✅。
- `[0x4e7404] == 1 ⇒ 148` 这一条是**一手事实**；🟡 它的语义（ExpHP 名 `CHARACTER`，TH15 有 4 个角色）
  没再验：为什么恰好一个角色（索引 1）的线低 20 px，留给下一轮。

### 5.2 线上档（`0x4406a5`–`0x440700`）

```
4406a5  mov    eax,0x51eb851f             ; ← ÷100 魔数
4406aa  mov    esi,0xa                    ; 后面 idiv 用
4406af  imul   DWORD PTR ds:0x4e7434      ; edx:eax = PIV × magic
4406b5  push   0xffffff00                 ; 飘窗色（黄）
4406ba  sar    edx,0x5
4406bd  mov    ecx,edx
4406bf  shr    ecx,0x1f
4406c2  add    ecx,edx                    ; ecx = D = PIV/100
4406c4  mov    eax,ecx ; 4406c6 cdq ; 4406c7 idiv esi   ; edx = D mod 10
4406c9  mov    eax,0x66666667             ; ← ÷10 魔数
4406ce  sub    ecx,edx                    ; ★ T = D − (D mod 10) = trunc10(PIV/100)
4406d0  imul   ecx ; 4406d2 sar edx,2 ; 4406d5 mov eax,edx ; 4406d7 shr eax,0x1f ; 4406da add eax,edx
4406dc  lea    esi,[eax+eax*4] ; 4406df mov eax,0xa ; 4406e4 add esi,esi   ; esi = 10×⌊T/10⌋ = T
4406e6  test   esi,esi ; 4406e8 cmovle esi,eax    ; ★ 下限 10
4406eb  mov    eax,[ebp-0x4]              ; this = item
4406ee  push   esi                        ; 飘窗数值
4406ef  add    eax,0xc20
4406f4  push   eax                        ; 位置 = item+0xc20
4406f5  call   0x45ef60                   ; PopupManager::generate_small_score_popup
4406fa  inc    DWORD PTR ds:0x4e746c      ; （只写不读，§6）
440700  add    DWORD PTR ds:0x4e7464,esi  ; （只写不读，§6）
440706  movq   xmm0,QWORD PTR [edi+0x618] ; edi = PLAYER
44070e  movq   QWORD PTR ds:0x4e7474,xmm0 ; → LAST_POINT_OR_POWER_COLLECTED_POS = 玩家坐标
```

### 5.3 线下档（`0x44060d`–`0x4406a3`）—— **乘 3/4 再按深度扣**

```
44060d  mov    eax,0x51eb851f             ; ← ÷100 魔数
440612  mov    esi,0xa
440617  imul   DWORD PTR ds:0x4e7434
44061d  push   0xffffffff                 ; 飘窗色（白）
44061f  sar    edx,0x5
440622  mov    ecx,edx
440624  shr    ecx,0x1f
440627  add    ecx,edx                    ; ecx = D = PIV/100
440629  mov    eax,ecx ; 44062b mov [ebp-0xc],ecx
44062e  cdq ; 44062f idiv esi              ; edx = r = D mod 10
440631  mov    edi,edx ; 440633 mov eax,edx
440635  sub    eax,ecx                    ; eax = r − D = −T
440637  cvttss2si ecx,xmm1                ; ecx = (int)玩家 y
44063b  lea    eax,[eax+eax*2]            ; −3T
44063e  cdq ; 44063f and edx,0x3
440642  sub    ecx,DWORD PTR [ebp-0x8]    ; ★ F = (int)y − base（base = 128/148）
440645  lea    esi,[edx+eax*1]
440648  mov    eax,0x91a2b3c5             ; ← ÷450 魔数
44064d  sar    esi,0x2                    ; ★ esi = ⌊−3T/4⌋
440650  imul   esi,ecx                    ; × F
440653  imul   esi                        ; edx:eax = 0x91a2b3c5 × esi
440655  mov    eax,[ebp-0xc] ; 440658 sub eax,edi      ; eax = T
44065a  add    edx,esi
44065c  sar    edx,0x8                    ; → ⌊…/450⌋ 型结果
44065f  mov    ecx,edx ; 440661 shr ecx,0x1f
440664  lea    eax,[eax+eax*2]            ; 3T
440667  add    ecx,edx                    ; 负数修正（负数时 +1）
440669  cdq ; 44066a and edx,0x3 ; 44066d add eax,edx ; 44066f sar eax,0x2   ; ★ ⌊3T/4⌋
440672  add    ecx,eax
440674  mov    eax,0x66666667 ; 440679 imul ecx ; 44067b sar edx,0x2
44067e  mov    eax,edx ; 440680 shr eax,0x1f ; 440683 add eax,edx        ; ⌊/10⌋
440685  lea    esi,[eax+eax*4] ; 440688 mov eax,0xa ; 44068d add esi,esi      ; ×10（trunc10）
44068f  test   esi,esi ; 440691 cmovle esi,eax    ; ★ 下限 10
440694  mov    eax,[ebp-0x4] ; 440698 add eax,0xc20 ; 44069d push eax
44069e  call   0x45ef60                   ; 飘窗（数值 = 上面这个 esi）
4406a3  jmp    0x440722
```

**闭式（✅ 逐值验证过）**：

```
T = trunc10(PIV / 100)                 # 显示量纲
F = (int)player.y − base               # base = 128（CHARACTER==1 时 148）
线上 / 自动回收:  value = T
线下:            value = trunc10( (3/4)·T·(1 − F/450) )
两条都再取 min 10（cmovle esi,0xa）
```

抽样（用逐指令模拟器跑出来的真实值，不是手算）：

| PIV（存储） | T | F | value |
| --- | --- | --- | --- |
| 1,000,000 | 10000 | 1 | 7480 |
| 1,000,000 | 10000 | 72 | 6300 |
| 1,000,000 | 10000 | 172 | 4630 |
| 1,000,000 | 10000 | 272 | 2960 |
| 2,500,000 | 25000 | 22 | 17830 |
| 2,500,000 | 25000 | 172 | 11580 |

- 「÷450」的魔数 `0x91a2b3c5`（`imul` + `add edx,esi` + `sar edx,8`）在正数区间**恰好等于除以 450**
  （穷举 d=1..5000 只有 450 通过）；负数区间它不等于 `floor` 也不等于 `trunc`（例 n=−450 → −2），
  所以上表用**逐指令模拟**而不引用闭式的取整细节：闭式是近似口径，精确口径是 §5.3 的指令序列。✅
- 物理含义：**线上（或线上自动回收）拿全额；线下拿 75% 再按"低于线多少"线性扣**，
  `F = 450` 时归零（被下限 10 托住），而屏幕高 448 ⇒ 最低约 0.22·T。

### 5.4 入账（两条支路共用，`0x440722`–`0x44075a`）

```
440722  ecx = ds:0x4e740c               ; CURRENT_SCORE
440728  mov eax,0x66666667 ; 44072d imul esi ; 440730 sar edx,2 ; 符号修正   ; value/10
44073a  add ecx,eax
440741  cmp ecx,0x3b9aca00 ; 440748 cmovae ecx,0x3b9ac9ff   ; 封顶 999,999,999
44074b  inc DWORD PTR ds:0x4e7430       ; ★ NUM_POINT_ITEMS_COLLECTED++
440751  mov ds:0x4e740c,ecx             ; ★ CURRENT_SCORE = ecx
```

⇒ **蓝点入账得分 = `value/10`**（value 已是 10 的倍数，所以正好是 `value/10` 的整数分）。
`value` 本身走 `0x45ef60` 飘窗；**没有别的加数、也没有乘数**。

## 6. 有没有「第二项」（基础值之外的额外加数）？

**答：蓝点这一链没有。** 分三层排除（每条都有一手地址）：

1. **算式层**：`0x4405c0` 的输入只有三个 —— `CURRENT_PIV`、`player.y`（`zPlayerInner.pos.y`）、
   `[item+0xc64]`（状态）。输出是**单值**，之后只做 `trunc10` 与下限 10。
   **没有任何 per-item 乘数槽**（对比 TH12 的飞碟奖励道具：`engine/faith/th12/01-point-item-value.md` §6
   用 `[item+0x9c8/0x9cc/0x9d0/0x9d4]` 做两项相乘 —— TH15 的 type 2 这些偏移全不读）。
2. **`MAXIMUM_PIV` 不是加数**：它只出现在 4 处 `cmovg` 夹取里
   （`0x4573ee` `0x43b21a` `0x440368` `0x43ad51`），没有一处是 `add`。⇒ 是**上限**，不是第二份基准。
3. **`GLOBALS+0x74` / `+0x7c`（`0x4e7464` / `0x4e746c`）不是加数**：收点确实会写它们
   （`add` 4 处：`0x440302` `0x440500` `0x440700` `0x440e4c`；`inc` 4 处：`0x4402f9` `0x4404fa` `0x4406fa` `0x440e43`），
   但**全 exe 零读点**（4 字节穷举 + 无部分宽度访问）⇒ 只写不读的累加器（ExpHP 也标 `__unknown`）。
   把它们当第二项会直接把公式带错。

⚠ **别和「分档」混**：线下档的 `3/4` 是**折扣**（同一项的不同系数），不是「基础值 + 额外项」。
同样别和「擦弹让道具变慢」混：那个因子住在 `ItemManager+0xe5def0`（`0.3` 起步、每帧 `+0.1` 回到 1.0），
**只乘道具位移**，全库没有任何渲染或算分代码读它
（[`../../player/th15/01-graze-and-item-slowdown.md`](../../player/th15/01-graze-and-item-slowdown.md) §4）。

### 6.1 给「总最大得点公式」的口径

```
显示的最大得点 = trunc10(CURRENT_PIV / 100)                  # HUD 那个数
CURRENT_PIV 的涨点（显示量纲）:
  擦弹             +1 / 次
  章节奖           + bonus/5000                （bonus = 本章奖，见 chapter/th15/01）
  F 道具（已满 P） +100 / 颗
  小道具 type 9/10 +2 / 颗       ← miss 散落 / 弹消し（灵击也算间接入口）
  小道具 type 11   +20 / 颗       ← 产出者未追（§9）
  蓝点收集         0            ← 蓝点只拿分，不涨最大得点
  miss / Boss 奖 / 碎片 本体 0   （穷举可见的写点里没有它们；miss 经散落 type 9 间接 +2/颗）
蓝点入账得分 = value/10（value 见 §5）
```

## 7. 与既有文档的对照 / 勘误

| 文档 | 说法 | 本文一手复核 |
| --- | --- | --- |
| [`player/th15/01`](../../player/th15/01-graze-and-item-slowdown.md) §1 | 擦弹 `CURRENT_PIV += 100` 于 `` `0x4573f5` `` | 语义 ✅；**地址应为 `0x4573fc`**（`0x4573f5` 是 `cmp BYTE ds:0x503d86,0`） |
| [`chapter/th15/01`](../../chapter/th15/01-chapter-and-bonus.md) §2a | `CURRENT_PIV += (bonus/50000)×10×100` | ✅ 逐指令一致（`0x43ad14`–`0x43ad58`，`xmm2` = `100.0f` @`0x4cffe8`） |
| [`chapter/th15/01`](../../chapter/th15/01-chapter-and-bonus.md) §1a | `+0x44` = `CURRENT_PIV`、`+0x4c` = `MAXIMUM_PIV` | ✅ 与 ExpHP `zGlobalsInner` 一致 |
| [`../th13/01-point-item-value.md`](../th13/01-point-item-value.md) §5 | TH13 蓝点值 = `trunc10(PIV/100)`，无高度分档 | TH15 **多了一层高度分档**（线上 100% / 线下 75%−深度扣）⇒ 跨版本断言要加这一条 |

## 8. 证据清单（可独立复核）

- exe：`local/th15.v1.00b/th15.exe`（md5 `bddd41a586e09e4447f795a14009d68a`）
- 平坦反汇编：`local/_scratch/th15_full.asm`（248067 行）
- 结构体/字段名：`local/vendor/th-re-data/data/th15.v1.00b/type-structs-own.json`（`zGlobalsInner` / `zPlayer` / `zItem`）
- 道具贴图与脚本：`local/th15.v1.00b/anm/bullet/bullet.anm.txt`（script112–133）、
  `local/th15.v1.00b/anm/bullet/bullet/item.png`（sprite500–518 的像素）
- 难度表原始字节：`0x4cc520` / `0x4cc538` / `0x4cc550`（各 6 个 `int32`）
- 常量：`0x4cffe8` = 100.0f、`0x4cff10` = 2.0f、`0x4cff98` = 20.0f、`0x4d001c` = 204.0f、
  `0x4d0028` = 226.0f、`0x4d0030` = 243.0f、`0x4d000c` = 182.0f、`0x4d0080` = 540.0f

## 9. 开放项

- ❓ **§3.3**：PIV/MAX 的初始化块（`0x43c1f0`）在运行时到底由哪条路径执行（决定「最大得点是否每关重置」）。
  已查过的入口：`0x43c0e8` 的 `[0x4e7ed8]` 判据（14 处引用全为读、无写、镜像值 0）、
  `0x43c690`/`0x43bff0` 的引用（只有 `0x43cbdf` 的 `push`）、快照恢复 `0x43dee0` 的两个调用点。
- ❓ **type 11 / 14 / 15 的产出者**：12 个 `spawn_item` 调用点全看过（§2.3），没找到；
  敌机掉落槽 `[enemy+0x519c]` 由 ECL 写（`0x4260af`），但那条 opcode 的编号没解出来。
  这一条影响「一局里 +20 那条涨点入口是否真会出现」。
- 🟡 **HUD 标签**：`y=204` 那一格的画面文字（应为「最大得点」）没做贴图 OCR。
- 🟡 `0x4e7404 == 1 ⇒ 线 148`：ExpHP 命名为 `CHARACTER`；哪个角色、为什么，未验。
- 🟡 `0x4e77a4`（`= T520[diff]` = 5/0）的用途未追。
- 🟡 类型 4/5 与 6/7 分别对应残机碎片 / 符卡碎片的哪一档（`0x44fa60` `0x44fa20` `0x44fbc0` `0x44fb80` 未逐条读）。

## 按 mod 的「额外机制」口径重述（2026-09-18）

### ★ mod 里的线高口径（用户 2026-09-18）

> 「**14 15 16 17 的线就按照实际 POC 来吧**」

本节记的**本作原生线**（上表的 128 / 148 与它的角色条件）**只作文档**：
mod 侧一律用 **TH18 的实际 POC**（`player+0x47998`，live 128 ↔ 224）作锚，
即 `d = y_src − POC_TH18(t)` —— 与 `coeff_0` 同一个锚、同一个 `d`。
（由此 th12–th17 的 `coeff_i / coeff_0` 是常数：线上 1、线下 **5/6**，见 [`OVERVIEW`](../OVERVIEW.md)。）


参照系：`G = G_0 × coeff_0 + Σ ΔG_i × coeff_i`（`G_0 = G_18`）。本节一手 = 复核 `th15:0x4405c0`–`th15:0x44068f`（蓝点支）、
`th15:0x43b200`（小道具加法器）、`th15:0x44034b`–`th15:0x440372`（满 P）、`th15:0x43ad25`–`th15:0x43ad58`（章节奖）
与三个浮点常量原字节。

| 项 | 一手 | 结论 |
| --- | --- | --- |
| 线高 | `th15:0x4405c6` `cmp ds:0x4e7404,1` → `eax=0x80` / `edx=0x94` / **`cmove eax,edx`** ⇒ 线 = **128，条件为 1 时 148** | 与 th14 同形（条件切换） |
| 判谁 | `th15:0x4405df` `edi = [ds:0x4e9bb8]`（自机）、`th15:0x4405ef` `xmm1 = [edi+0x61c]` ⇒ **自机 y**；`jae` ⇒ `线 ≥ 自机 y` 走满值 | 自机 y |
| 例外位 | `th15:0x440600` `cmp [item+0xc64],3` ⇒ 无视高度满值 | 同族字节 |
| 线下系数 | `(3/4)·trunc10(PIV/100) × (1 − t/450)`，**`t = (int)自机 y − 线`**（`th15:0x440642` 的 `sub ecx,[ebp-0x8]`，`[ebp-0x8]` 正是那条 128/148） | ⇒ **0.75 族**（与 th14 逐指令同形） |
| 涨源：擦弹 | `th15:0x4573a0`（`PIV += 0x64` = +1 显示） | ★ **已并入跨作共用的 `ΔG_擦弹`** |
| 涨源：满 P | `th15:0x440357` `add edx,0x2710` ⇒ **+100 显示**，夹上限 | ✅ |
| 涨源：小道具 | `th15:0x43b200`：`PIV += (int)(farg × 100.0)`（`th15:0x4cffe8` = **100.0**）；两个调用点实参 `th15:0x4cff10` = **2.0** ⇒ +2 显示、`th15:0x4cff98` = **20.0** ⇒ +20 显示 | ✅（与 th14 的绿点同形） |
| 涨源：章节奖 | **已追完，见下方「§章节奖全链」** | ✅ 算式 / 🟡 两个计数器的语义 |
| 初始化 | `th15:0x43c25b`：`PIV = (int)(xmm1)`，`xmm1` 乘的常量就是 `th15:0x4cffe8`（= 100.0）⇒ 难度表 ×100 | ✅ |
| 第二项？ | 蓝点支里**没有**任何第二项（只有 PIV / 线 / 例外位三样输入） | ❌ 无（与 th14 的一括回収不同） |

⇒ **`ΔG_15` = PIV 本身；`coeff_15` = 线上 `1` / 线下 `0.75×(1−t/450)`（线 128，条件 148；判自机 y）** ⇒ 进 `coeff_15`。
本作**没有额外项**；★ 订正（§4.6）：**miss 不涨 PIV**（撒的是 type 1 ×7 的 P 小道具；旧稿写的「撒 type 9 ⇒ 每颗 +2」已被可达性穷举否掉）。

## ★ 章节奖全链（2026-09-18 追完；一手）

例程 `th15:0x43ab70`（`edi = ds:0x4e9a8c` = **`GUI_PTR`**，ExpHP 名；即这是一段 **GUI 侧的章节结算**），
开头 `th15:0x43ab79` `cmp ds:0x4e7484,0` / `je` ⇒ **`A == 0` 时整段跳过**；调用者 `th15:0x43d0d0`（带一个 0/1 实参）。

```
43ac4a  [gui+0x120] = ds:0x4e7420           ; ★ 章内擦弹数（= 擦弹例程里 +1 的第二个计数器）
43ac50  xmm1 = (float)ds:0x4e7488 ; xmm0 = (float)ds:0x4e7484
43ac78  xmm1 = xmm1 / xmm0                   ; ★ 两个计数器的比值
43ac70/43ac85  xmm1 *= 100.0 (th15:0x4cffe8) ; ⇒ 百分数 pct
43ac8c  [gui+0x124] = [gui+0x128] = pct
43ac94  xmm0 = (float)([gui+0x120]) × pct    ; 章内擦弹 × pct
43aca0  xmm0 *= 50.0 (th15:0x4cffc0)
43aca8  ecx = (int)xmm0
43acac/43acb8  ecx = 10 × (ecx/10)           ; ⇒ trunc10
43acc4  [gui+0x12c] = ecx                    ; ★ base
43acbd  if (pct >= 1.0 (th15:0x4cfe98)) [gui+0x134] = (int)(base/pct) else 0
43aced  ds:0x4e740c += base/10               ; ★ 分数 += base/10（钳 999,999,999，th15:0x43ad06）
43ad1f  eax = 10 × (base / 50000)            ; 魔数 0x14f8b589 + sar 12 = ÷50000
43ad41  eax = (int)(eax × 100.0)             ; xmm2 此刻仍是 100.0
43ad49  PIV += eax ; clamp MAX ; th15:0x43ad58 落笔
```

**⇒ 两条产出（一手算式）**：

| 产出 | 算式 |
| --- | --- |
| 分数 | `SCORE += base / 10`，其中 `base = trunc10( 章内擦弹 × pct × 50 )`、`pct = (ds:0x4e7488 ÷ ds:0x4e7484) × 100` ⇒ 即 **≈ `5 × 章内擦弹 × pct`** |
| **最大得点** | ★ **两步取整（照指令序列）**：`PIV += 1000 × ⌊base / 50000⌋`（**存储**）⇒ 显示 = `10 × ⌊base / 50000⌋`。<br>⚠ 旧稿写的 `⌊base/5000⌋` 是**化简式**，二者在 base 非 50000 整数倍时差 ≤ 9 显示；一…（详见本节正文） |

### ★★★ 权重不是「一杀 +1」——**是脚本在生成点写死的字面量**（2026-09-18 两轮：初判 + 订正）

> ⚠ **本节先改一处我自己的误读**：`0x430320(obj, idx)` 取的不是「敌人脚本变量 #0」，
> 而是**当前正在执行的那条 ECL 指令的第 `idx` 个实参**（`th15:0x48e750` 的取指链 =
> `instr = code + 0x10 + PC`、`[instr+8]` 是 **thtk 的 `param_mask`（位 = 1 表示该参数是「引用」，
> 不是「已定义」）**、`[instr+idx*4+0x10]` 是参数槽；与 [`engine/ecl/th16/04`](../../ecl/th16/04-ecl-vm-interpreter.md) 的取指逐字节同形）。
> ⇒ 对 op **569** 而言，它就是脚本里写的 `569(n)` 的那个 `n`。

**一手链（`zEnemyData` 内嵌于 `zEnemy+0x120c`，故 `+0x452c` == `zEnemy+0x5738`）**：

| 写 / 消费 | 地址 | 语义 |
| --- | --- | --- |
| op **500 `setHurtbox`** | `th15:0x42b6b3` / `th15:0x42b6bd` | **默认权重 = 1**，`A += 1` |
| op **569** | `th15:0x42b701` / `th15:0x42b70f` / `th15:0x42b72c`–`0x42b73b` | `weight = 参数0`；`A += w`；`w == 1` 那一支把 setHurtbox 的 +1 **改写成真权重**（`A = A − 1 + w`） |
| op **570** | `th15:0x42b745` / `th15:0x42b765` | `B += w`、权重清零（脚本**显式**声明「已击破」） |
| op **524 `setChapter`** | `th15:0x42d4a5` / `th15:0x42d4ab` | 章戳 `[obj+0x4528]`、权重清零 |
| **引擎销毁路径** | `th15:0x428d66`–`0x428d84`（孪生 `0x4300e2` / `0x4302f7`） | `if (weight > 0 && own_chapter == CURRENT_CHAPTER) B += weight` ← ★ **普通杂兵的击杀走这里**，同函数还按 `set_death` 名字跑死亡脚本 |
| 章戳自动盖 | `th15:0x42620c` | 敌机分配时 `[obj+0x5734] = CURRENT_CHAPTER` |

**发行数据实测（`thecl -d 15 -x` 全解 23 个 `.ecl`，反编译文本留在 `local/_scratch/var0/`）**：

- **104 处 `569(...)`，全部是字面量**（无变量表达式）；另有 **84 个 sub 只调 `setHurtbox` ⇒ 权重 1**（引擎默认）。
- 取值分布：`1` × 63、`-1` × 19、`3` × 8、`6` × 6、`0` × 4、`2` × 2、`4` × 1、`5` × 1。
- ★ **决定性反例（同型不同权重）**：`st07bs` 的 `Boss3` 与 `Boss7` **精灵设置完全相同**（`anmMain 7`、`anmSetSprite 20/107/132`、`lifeSet(20000)`），
  却分别是 `569(1)`（章 45）与 `569(3)`（章 49）；`Boss2` / `Boss6` 同形（1 vs 3）。
  ⇒ **「按敌机类型查表」只是近似**（道中妖精家族确实一致：GirlA*=1 / GirlC*=3 / GirlD*=6，同名 sub 跨关保值），
  boss 相位由 ZUN 逐相位手写。
- `569(0)` = **完全不计入**（例 `st05bs:Boss5`、`st07bs:Boss8/9/10`）；
  `569(-1)` = **生成但不占单位**（销毁路径 `jle` 不记账，且顺手挡住 `setHurtbox` 的默认 +1；19 处，如 `st04bs:BCir01..03`）。
- 还有 8 处脚本在**生成时**就显式 `570()`（如 `st06:Phantom04/05`）⇒「提前把自己记成已击破」，避免拉低击破率。

**⇒ 对 mod 的两条结论**：① 引擎侧**没有**按敌机类型给权重的表 ⇒ 想「按类型近似」在引擎里无依据（但可按 ECL 的 104 处字面量建表，或直接读敌机现场字段 `zEnemy+0x5738`）；
② 既然权重是**逐生成点**的字面量，「同一种敌人不同波不同权重」**确实会发生** —— 这正是 owner 2026-09-18 裁定「就用目前这套统计」的直接依据。

### ★★ 「两个计数器」已钉死：借 ExpHP 的 `zGlobalsInner` 字段名 + 一手对照

`GLOBALS = ds:0x4e73f0`（ExpHP `statics.json`：`GLOBALS, struct zGlobals`），其 `inner`（`zGlobalsInner`）里：

| 全局 | 偏移 | 名字（vendor） | 本例程里的角色 |
| --- | --- | --- | --- |
| `ds:0x4e73f8` | `GLOBALS+0x8` | **`CURRENT_CHAPTER`** | 门：`[obj+0x4528] / [obj+0x5734] == CURRENT_CHAPTER`；另有多处 `cmp CURRENT_CHAPTER,0x29 / 0x2b` |
| `ds:0x4e740c` | `GLOBALS+0x1c` | **`CURRENT_SCORE`** | 章节奖的分数落点（`+= base/10`） |
| `ds:0x4e7418` | `GLOBALS+0x28` | `RANK` | `th15:0x432ef0` 的 getter 返回它 |
| `ds:0x4e741c` | `GLOBALS+0x2c` | **`GRAZE_TOTAL`** | 擦弹例程 `th15:0x4573a0` 里 +1 的第一个计数 |
| `ds:0x4e7420` | `GLOBALS+0x30` | **`GRAZE_IN_CHAPTER`** | ★ 擦弹例程里 +1 的第二个计数 ⇒ 就是 `[gui+0x120]` |
| `ds:0x4e7434` | `GLOBALS+0x44` | **`CURRENT_PIV`** | 本文一直用的最大得点载体 ✅ |
| `ds:0x4e7438` | `GLOBALS+0x48` | `INITIAL_PIV` | 只写不读 ✅ 与旧记录一致 |
| `ds:0x4e743c` | `GLOBALS+0x4c` | `MAXIMUM_PIV` | 上限 ✅ |
| **`ds:0x4e7484`** | **`GLOBALS+0x94`** | ★★ **`ENEMIES_SPAWNED_IN_CHAPTER`** | 公式里的 **A（分母）** |
| **`ds:0x4e7488`** | **`GLOBALS+0x98`** | ★★ **`ENEMIES_DESTROYED_IN_CHAPTER`** | 公式里的 **B（分子）** |

⇒ 于是 `pct = B / A × 100` **就是「本章敌机击破率（%）」**，`base = trunc10( 本章擦弹 × 击破率% × 50 )`。
一手也自洽：`A` 的加值紧跟着把对象打上本章标签（`th15:0x42b6b3` `[obj+0x452c] = 1` 后 `inc A`；
`th15:0x42d4a5` 写 `[obj+0x4528] = <id>` 并 `[obj+0x452c] = 0`），
`B` 的加值则**只在对象标签 == 当前章**时把该计数收走再清零（`th15:0x42b765` / `0x428d7e` / `0x4300fa` / `0x4302fd` / `0x433136`）—— 
即「**本章生成、且在本章被击破**」才算，跨章的不计入 ⇒ 与字段名的语义完全吻合。

⚠ 换句话说：**章节奖不是「擦弹奖」，是「击破率 × 擦弹」的合成奖**；它的大头在分数（`base/10 ≈ 5 × 擦弹 × 击破率%`），
给最大得点的只有 `⌊base/5000⌋` 显示（击破率 100% 且擦弹 1000 才 +10）。

### 4.6 ★★ 道具 type 的**可达性**（2026-09-18，一手穷举 + `thecl` 全解 23 个 `.ecl`）

> 起因：mod 侧要判断「TH18 的哪一类道具 ≈ TH15 的 `+20` 档」。结论是 **TH15 的 type 11 在原版没有任何生成点**。

- **生成函数**唯一 = `th15:0x4408d0`（type 在 `[ebp+8]`；`item+0xc68` 的唯二写点 `th15:0x4409fa` / `th15:0x440b07` 都在它里面）；
  它对 `type ∈ {9, 10, 11, 13}` 走同一分支 `th15:0x440a65`。
- 它的 12 个调用点实参（全读）：`0xd` · `0xa` ×2 · `1` ×7（miss 撒落表 `th15:0x4d0170`）· `9` ×2（`th15:0x45b14d` / `0x45b1e1`，在**不可达**的 `FUN_0045b0a0` 里）
  · 以及三处**数据驱动**：`th15:0x424ec3` / `0x428d4b` = 敌人的 `drop.main_type`（`zEnemyData+0x3f90`）、`th15:0x424fd3` = `drop.extra_counts[ebx]` 门控下的 `type = ebx+1`。
- **ECL 全量枚举（`thecl -d 15 -x` 23 个 `.ecl` + 独立 raw 扫描，双路一致）**：
  `dropMain` ∈ {0,1,2,3,5,6,7}（`1`×76 / `2`×43 / `7`×4 / `3`×2 / `0`×2 / `6`×1 / `5`×1，另 2 处负 id 运行时取值）；
  `dropExtra` 的 arg0 ∈ {1,2,3}；`enmCreate*` 的第 6 参（item）∈ {0,1}。**零处 8、零处 11。**
- ⇒ **原版可达的 type 只有**：1 / 2 / 3 / 5 / 6 / 7（+ 消弹的 10；miss 的 1）。
  **不可达**：4（道具形态，章节奖直调而非掉落）、**8**、**9**、**11**、12、13、14、15。
- ⚠ 本文 §4 那两行「miss 撒落的小道具」与「type 9」需要按此理解：
  **miss 掉的是 type 1 ×7**（P 小道具，`Player__on_tick` `th15:0x454c8a`，死亡动画第 3 帧；按着射击/低速键则跳过 = deathbomb），
  而 `+2 显示` 那一档在**活数据里只有消弹（type 10）**这一条来源。
- **对 mod 的意义**：TH15 的 9/10/11 是**同一分支、同一 anm 家族、只差 PIV 增量**的「只涨点数不涨资源」道具，
  而 **11 谁都不产出** ⇒ 「TH18 的哪一类 ≈ +20 档」**没有原版依据**（只能按效果对应，属于自定）；
  ⇒ 建议只接 **`+2`**（TH18 的 `type 9–0x0e` 分数转换点族 = 消弹/击破产物，对 TH15 的 type 10 语义最近）。
