# item 逆向 01：TH06 点道具（蓝点）的收点分值 —— 难度 × 高度公式，没有「会增长的点道具价值」
> **版本**：TH06 v1.02h（`local/th06.v1.02h/th06.exe`，md5 `99920041bc37e3ad4dc0201af84244c1`，512,000 B，imagebase `0x400000`）。
> 本文所有地址都带 `th06:` 前缀，全部属该版本。目录里另有一个 `東方紅魔郷.exe`（同节区布局、内容不同，md5 `706cc43f…`），**本文一律以 `th06.exe` 为准**。
>
> **方法**：自写 PE 解节区（`.text` RVA `0x1000` / 文件 `0x1000` / 430,080 B）→
> `objdump -D -b binary -m i386 -M intel --adjust-vma=0x401000 /tmp/_th06_text.bin > /tmp/th06_full.asm`（134,498 行）
> + 直读 `.rdata`(`0x6a000`)/`.data`(`0x76000`) 常量与跳表原始字节 + `local/vendor/th-re-data/data/th06.v1.02h/*.json` 命名（**只当线索，逐条回指令复核**）。
> 日期 2026-09-18。**没用 Ghidra**（工程锁独占）。
> **可信度**：§1/§2/§3.3/§5 的**算式、跳表、常量、调用点**= ✅（一手指令 + 直读字节）；
> §3.1 的否定结论按该节的**穷举口径**（写清搜了什么、覆盖哪些寻址形态）；难度号 ↔ 名（Easy…Extra）🟡；
> `type 0/2` 的 P 道具细节只做到「够用来证明它跟点道具无关」的程度，未展开。

## 0. 一句话结论

TH06 的蓝点（`item_type == 1`）**分值不来自任何全局累计量**：收点那一帧现算
`value = f(difficulty, item->y)`，只读 **难度** `th06:0x69bcb0` 和**这颗道具自己的 y** `[item+0x114]`。

| 难度 | y < 128（回收线以上） | y ≥ 128（线以下，逐像素递减） | 证据 |
| --- | --- | --- | --- |
| 0 / 1（表项共用一格） | **100000** | `60000 − (y−128)×100` | `th06:0x41fb0b` 分支 |
| 2 | **150000** | `100000 − (y−128)×180` | `th06:0x41fb88` 分支 |
| 3 | **200000** | `150000 − (y−128)×270` | `th06:0x41fc08` 分支 |
| 4 | **300000** | `200000 − (y−128)×400` | `th06:0x41fc85` 分支 |

⇒ 「一个会随时间/玩法增长的点道具价值变量」在 TH06 **不存在**（见 §3.1）；
但 TH06 里有**两个别的**增长型机制，别跟 PIV 混（见 §3.3）：消弹点道具（`type 6`）按**擦弹数**给分、
满火力时 P 道具按**连吃计数**查 31 项表。

## 1. 先定位：TH06 的道具系统骨架

| 项 | 值 | 一手证据 |
| --- | --- | --- |
| 单例 | `th06:0x69e268`（`zItemManager`，所有调用点都 `mov ecx,0x69e268`） | 23 处 `spawn_item` 调用点 |
| 池 | **512** 条（`0x200`），每条 **`0x144`** B，环状游标 `+0x28944` | `th06:0x41f527` `cmp [ebp-0x4],0x200`、`th06:0x41f51e` `add edx,0x144` |
| 活跃标志 | `item+0x141`（0 = 空槽） | `th06:0x41f537` |
| 类型 | `item+0x140`（u8） | `th06:0x41f921` `movsx edx,BYTE PTR [ecx+0x140]` |
| 状态 | `item+0x143`（u8；1 = 8.0 px/帧归航玩家） | 写点 `th06:0x41f77f`、判点 `th06:0x41f708` |
| 位置 / 速度 | `item+0x110`(Float3) / `item+0x11c`(Float3) | `th06:0x41f34f`、`th06:0x41f368` |
| 生成口 | `th06:0x41f290`（thiscall，`ret 0xc`：`pos*, type, state`） | 全 exe 唯一入口，§1.1 |
| 每帧 | `ItemManager::on_tick__` `th06:0x41f4a0`（`0x41f4a0`–`0x420130`） | 池循环 + 类型跳表 |
| 类型分派 | 跳表 `th06:0x4200f8`（7 项，`type` 直接索引） | `th06:0x41f941` `jmp DWORD PTR [eax*4+0x4200f8]` |

**`item` 结构里没有「分值」字段** —— 0x144 字节用满：`vm`(0x0)–`pos`(0x110)–`velocity`(0x11c)–`vec3`(0x128)–`timer`(0x134)–`type`(0x140)–`in_use`(0x141)–`unk`(0x142)–**`state`(0x143，最后一个字节)**。
⇒ 分值只能是「收点时现算」的局部量，**结构层就先排除**了「每颗道具带一个会长的 value」。

### 1.1 类型号（跳表直读，7 项）

| type | 目标 | 一手行为（本节只需 1 和 6） |
| --- | --- | --- |
| 0 | `th06:0x41f948` | 小 P：`power<128` → `power+=1` 且 `score+=10`；`power==128` → 查表 `th06:0x476660` |
| **1** | **`th06:0x41fae6`** | **蓝点（点道具）：§2** |
| 2 | `th06:0x41fd78` | 大 P：`power+=8`（钳 128）+`score+=10`；满火力 → 查同一张表 |
| 3 | `th06:0x41ff02` | Bomb：`bombs_remaining<8` 则 +1，`rank_increase(5)` |
| 4 | `th06:0x41ff89` | 满火力：`power=128`、`score+=1000`、弹幕全转点 |
| 5 | `th06:0x41ff3d` | 1UP：`lives_remaining<8` 则 +1，`rank_increase(0xc8)` |
| **6** | **`th06:0x420028`** | **消弹专用点道具（蓝点，另一个 sprite）：§3.3** |

> `type 0/2` 的「满火力表」`th06:0x476660` 是 31 项 dword，直读：
> `10,20,30,…,100,200,…,1000,2000,…,12000,51200`，索引 = `th06:0x69d4b9`（`GAME_MANAGER+0x1819`，每次 +1/+8，钳 `0x1e`=30）。

## 2. 点道具（`type 1`）的收点算式（逐指令）

### 2.1 收点判定：`th06:0x426fd0`

| 地址 | 指令要点 | 语义 |
| --- | --- | --- |
| `th06:0x426fe5` | `movsx ecx,BYTE PTR [eax+0x9e0]` | 玩家 `state`；`!=0 && !=3` ⇒ `return 0`（死亡/出场中不收） |
| `th06:0x427009` | `fld 1.0f / fdiv 2.0f` | 把参数 `&0x69e234`（三个 `16.0f`）折半 = 8.0 |
| `th06:0x42712b` | `fld [ecx+0x470] / fcomp` | 玩家收集盒 `+0x470/+0x474/+0x47c/+0x480` 与「道具 ±8 px」盒子求交 |
| `th06:0x42717f` | `mov eax,0x1` | 相交 ⇒ 返回 1 = 收到 |

调用点 `th06:0x41f911`（`ecx = th06:0x6ca628` = `PLAYER`，参数 = `item+0x110`、`&0x69e234`），
返回 0 直接 `je th06:0x42008f`（跳过本帧、继续下落）。

### 2.2 进入 `type 1` 分支

```
th06:0x41f921  movsx edx,BYTE PTR [ecx+0x140]   ; item_type
th06:0x41f92e  cmp   [ebp-0xd4],0x6 / ja 0x420079
th06:0x41f941  jmp   DWORD PTR [eax*4+0x4200f8]  ; → type 1 = 0x41fae6
```

`th06:0x41fae6` 第一件事就是**按难度分派**（这是「按难度查表还是常量」的正面答案：**是跳表**）：

```
th06:0x41fae6  mov  eax,ds:0x69bcb0        ; ★ GAME_MANAGER+0x10 = difficulty
th06:0x41faf1  cmp  [ebp-0xd8],0x4
th06:0x41faf8  ja   th06:0x41fd00          ; difficulty > 4 ⇒ 跳过（value 保持 §2.1 的 0）
th06:0x41fb04  jmp  DWORD PTR [ecx*4+0x420114]
```

**难度跳表 `th06:0x420114` 原始 5 个 dword**（`.text` 直读，小端）：

```
0b fb 41 00 | 0b fb 41 00 | 88 fb 41 00 | 08 fc 41 00 | 85 fc 41 00
→ 0x41fb0b  | 0x41fb0b   | 0x41fb88   | 0x41fc08   | 0x41fc85
```

**难度 0 与难度 1 指向同一格**（一手，不是抄来的）⇒ 低两个难度的点道具同值。

### 2.3 四个难度格的常量（`.text` 立即数原始字节）

| 难度分支 | y < 128 的常量 | y ≥ 128 的基值 / 斜率 | 指令 |
| --- | --- | --- | --- |
| `th06:0x41fb0b`（0/1） | `0x186a0` = 100000 | `0xea60` = 60000，`imul eax,eax,0x64` = ×100 | `th06:0x41fb20` `c7 85 24 ff ff ff a0 86 01 00` |
| `th06:0x41fb88`（2） | `0x249f0` = 150000 | `0x186a0` = 100000，`imul …,0xb4` = ×180 | `th06:0x41fb9d` |
| `th06:0x41fc08`（3） | `0x30d40` = 200000 | `0x249f0` = 150000，`imul …,0x10e` = ×270 | `th06:0x41fc1d` |
| `th06:0x41fc85`（4） | `0x493e0` = 300000 | `0x30d40` = 200000，`imul …,0x190` = ×400 | `th06:0x41fc9a` |

每格逐指令同形（以难度 0 为例）：

```
th06:0x41fb0b  mov  edx,[ebp-0x14]            ; item
th06:0x41fb0e  fld  DWORD PTR [edx+0x114]     ; ★ item->pos.y
th06:0x41fb14  call th06:0x45ba78             ; float → int（fistp qword + fild 的 MSVC 取整序列）
th06:0x41fb19  cmp  eax,0x80                  ; 128
th06:0x41fb1e  jge  th06:0x41fb2c
th06:0x41fb20  mov  [ebp-0xdc],0x186a0        ; y < 128 ⇒ 100000（常量）
th06:0x41fb2c  ...  sub eax,0x80              ; y-128
th06:0x41fb3f  imul eax,eax,0x64              ; ×100
th06:0x41fb42  mov  ecx,0xea60                ; 60000
th06:0x41fb47  sub  ecx,eax                   ; 60000-(y-128)*100
th06:0x41fb4f  mov  [ebp-0x8],value
th06:0x41fb6a  push color ; push value ; push &item->pos
th06:0x41fb7e  call th06:0x401940             ; AsciiManager::CreatePopup1(pos, value, color)
```

**冒字颜色**：`value < V_max` ⇒ `push 0xffffffff`，否则 `push 0`（`th06:0x41fb58`–`0x41fb6a`，
四格同形：`th06:0x41fbda`、`th06:0x41fc5a`、`th06:0x41fcd7`）——即「非满值白色、满值另一种色」。

### 2.4 公共尾 `th06:0x41fd00`（只有 `type 1` 会走到这里）

```
th06:0x41fd00  mov ecx,ds:0x69bca4          ; GAME_MANAGER+0x4 = score
th06:0x41fd06  add ecx,[ebp-0x8]            ; ★ score += value
th06:0x41fd09  mov ds:0x69bca4,ecx
th06:0x41fd0f  mov dx,WORD PTR ds:0x69d4b4  ; GAME_MANAGER+0x1814 阶段内点道具计数
th06:0x41fd16  add dx,0x1 / th06:0x41fd1a 写回
th06:0x41fd21  mov ax,WORD PTR ds:0x69d4b6  ; +0x1816 另一个计数器 +1
th06:0x41fd31  and ch,0xfc / or ch,0x2      ; 写 ds:0x69bc30 的 HUD 状态位
th06:0x41fd43  fld [item+0x114] / fcomp th06:0x46a534 (=128.0f)
th06:0x41fd59  y < 128 ⇒ rank_increase(0x1e=30)   ; th06:0x41c57e
th06:0x41fd67  y ≥ 128 ⇒ rank_increase(3)
th06:0x41fd73  jmp th06:0x420079 → item->in_use = 0（消费掉）
```

⇒ **收一颗蓝点 = `score += value`、阶段点道具数 +1、rank 上涨**（线以上涨得多：30 vs 3）。
`type 3/4/5/6` 都**不**经过 `0x41fd00` ⇒ 只有 `type 1` 计入点道具计数与 rank 那一档。

### 2.5 线上 / 线下（回收线）差别：**有，而且是同一个常量**

| 问题 | 答案 | 证据 |
| --- | --- | --- |
| 回收线在哪 | `y = 128.0f`（`.rdata` `th06:0x46a534` = `128.0f`，原始字节 `00000043`） | 两处引用同一常量 |
| 线上（`y<128`） | 分值 = 该难度的**平顶** `V_max`；rank +30 | `th06:0x41fb19`、`th06:0x41fd59` |
| 线下（`y≥128`） | 分值 = `V_base − (y−128)×K`，**逐像素递减**；rank +3 | `th06:0x41fb2c`、`th06:0x41fd67` |
| 屏幕外上方的道具 | 用另一套 sprite（`0x207+type`）并按 y 淡出，与分值无关 | `th06:0x420220`、`th06:0x46b14c` = `-8.0f` |
| 道具什么时候消失 | `item->y ≥ arcade_region_size.y + 16.0f` ⇒ `in_use=0` 且 `rank_decrease(3)` | `th06:0x41f884`–`th06:0x41f8b3` |

**自动收集（吸点）的条件**：`power ≥ 0x80` **且** `player.y < 128.0f`（`th06:0x41f70d`–`th06:0x41f72f`），
满足则把该道具 `state = 1`、速度设成朝玩家 8.0 px/帧（`th06:0x41f731`–`th06:0x41f77c`）。
`type 6`（消弹点道具）**生成时就是 `state = 1`**，不需要这个条件。
⇒ 线上收点拿的是平顶值，是因为**归航到最后位置就在玩家头上（y<128）**；`y` 用的是**收点那一帧的道具 y**，
不是「掉落点」也不是「最高点」。

## 3. 「会增长的点道具价值」存不存在

### 3.1 ❌ 不存在 —— 穷举口径

**（a）算式本身只读两处内存。** `type 1` 分支 `th06:0x41fae6`–`th06:0x41fd73` 的**全部内存读**：
`ds:0x69bcb0`（难度）、`[item+0x140/0x114/0x110]`（类型/坐标）、`ds:0x69bca4`（score，只加）、
`ds:0x69d4b4`/`0x69d4b6`（计数器，只加）、`ds:0x69bc30`（HUD 状态位，只写）。
**没有任何「价值/倍率」类全局被读进来**。

**（b）结构层排除。** `zItem` 只有 0x144 B 且最后字段是 `state`（§1）⇒ 没有位置存「每颗道具的价值」，
就只能现算，也就没有「会长」的载体。

**（c）grep 覆盖面（写清搜了什么）。** 对 `/tmp/th06_full.asm`（`.text` 全量 430,080 B，**含
`ds:` 绝对寻址与 `[reg+disp32]` 两种形态**，objdump 对两者都印出完整 8 位十六进制地址）做了：
`0x69bca4`（score，44 处）、`0x69bcb0`（难度，79 处）、`0x476660`/`0x4766dc`（两张表）、
`0x69d4b0/b4/b6/b9`（power / 点道具计数 / 满 P 表索引）逐条打开所在指令复核。
结论：**两张表只在 `type 0/2`（P 道具）路径被读**（写点见 §3.3），
**`type 1` 路径一次都不读**；`GAME_MANAGER` 里也没有「点道具价值」字段（`zGameManager` 到 `+0x1a80` 结束，逐字段有名字）。
⚠ 反例登记：`0x69bcb0` 在 `th06:0x4028e7` 等处只做 `< 4` 判据；`0x69bcb0` 的**写点**全在菜单/回放载入
（`th06:0x42a956`、`0x4359ee`、`0x43641c`、`0x437cb8`…），**没有**「随玩法上调」的路径。

**（d）一句话判据**：把难度、关卡、擦弹数全清 0 再收一颗蓝点，分值只由「难度 + 这颗的 y」决定。

### 3.2 那么「TH07 樱点那种雏形」在哪：**TH06 里没有对应物，但有两个别的增长计数器**

| 机制 | 载体 | 增长方式 | 与「点道具分值」的关系 |
| --- | --- | --- | --- |
| 满火力吃 P 的分数 | `th06:0x69d4b9`（`GAME_MANAGER+0x1819`） | 每次吃 P **+1**（大 P **+8**），钳 30 | 作用于 `type 0/2`，**不是**蓝点 |
| 消弹点道具的分值 | `th06:0x69bcb4`（`GAME_MANAGER+0x14` graze_in_stage） | 每擦一颗弹 +1（钳 9999），**关卡开始时清 0** | ★ 作用于 `type 6` 蓝点，见 §3.3 |

### 3.3 ★ `type 6`（消弹点道具）的分值随擦弹数增长 —— 这是 TH06 最接近 PIV 的东西

```
th06:0x420028  mov  eax,ds:0x69bcb4        ; ★ graze_in_stage
th06:0x42002d  cdq / idiv 3                ; ÷3
th06:0x420035  imul eax,eax,0xa            ; ×10
th06:0x420038  add  eax,0x1f4              ; +500
th06:0x42003d  mov  [ebp-0x8],eax          ; value = 500 + 10*floor(graze/3)
th06:0x420040  cmp  DWORD PTR ds:0x6d1bf0,0
th06:0x420049  jne  → [ebp-0x8] = 0x64     ; ★ 灵击中 ⇒ 固定 100
th06:0x420050  score += [ebp-0x8]          ; 同型但走 CreatePopup2(0x401a60)
th06:0x420079  item->in_use = 0            ; ★ 不经过 0x41fd00：不计点道具数、不涨 rank
```

| 项 | 值 | 证据 |
| --- | --- | --- |
| 载体 | `th06:0x6d1bf0` = `PLAYER(0x6ca628) + 0x75c8` = `zPlayerInner.__dword_0` | 地址算术逐字节对得上 |
| 该字段的语义 | **灵击（bomb）进行中** | 置 1 于 `th06:0x4289fb`（同段 `bombs_remaining -= 1`）；清 0 于 `th06:0x405292`（bomb 计时到）；`th06:0x42893a` 用它决定是否调 bomb tick |
| 谁能造出 `type 6` | 只有消弹链：`th06:0x4141d3`、`0x414306`、`0x4143db`、`0x4144e9`、`0x41456e`（`BulletManager` 两个消弹函数内）、`0x415bd1`、`0x415c32`（`BulletManager::on_tick_0b` `th06:0x4149d0` 内）；**7 处全是 `push state=1; push type=6`** | 23 处 `spawn_item` 调用点逐个回看压栈 |
| 冒字 | `CreatePopup2` `th06:0x401a60`（另一套槽位、另一次 idiv 10 拆位） | `th06:0x420074` |

⇒ **TH06 的「点道具价值」有两档互不相干的口径**：
`type 1`（ECL 掉落 / 敌机掉落）＝**难度×高度常量公式**（无增长）；
`type 6`（消弹）＝**`500 + 10×⌊graze/3⌋`**（随擦弹增长，灵击期间被压成 100）。
⚠ **别把 `type 6` 的公式搬到 `type 1`**：两者连 sprite 都不同（`0x200+type`，见 `th06:0x4202e7`/`0x42025e`）。

## 4. 收点的其它加成 / 减少

| 路径 | 对「分值」的影响 | 一手证据 |
| --- | --- | --- |
| 吸点（回收线以上、满火力） | **不改公式**；只改「什么时候收」+「收点时的 y」（于是通常拿到平顶值） | `th06:0x41f70d`–`0x41f77c` |
| 灵击（bomb） | ★ **把全屏道具设成归航** `state=1`（注意：不是删除！） | `th06:0x420130` 只写 `[item+0x143]=1`；4 个调用点 = 4 个机体 bomb tick：`th06:0x405356`(灵A)、`0x4061de`(灵B)、`0x40676e`(魔A)、`0x406f07`(魔B) |
| 灵击期间收 `type 6` | **500+10⌊graze/3⌋ ⇒ 固定 100** | `th06:0x420040`–`0x420049` |
| 灵击期间擦弹 | 不涨 graze（于是也不涨 `type 6` 的分） | `th06:0x427639` 读同一字段，非 0 就跳过 +1 |
| Miss（中弹） | **不改点道具分值**；不死清道具池、不动 graze/计数。死亡段落改为**撒 P 道具**（1 颗大 P + 4 颗小 P，`power -= 16`） | 死亡段 `th06:0x428a6e`–`0x428c2d`；graze 只在阶段初始化 `th06:0x402f58` 清 0 |
| 满火力吃 `type 0/2` | 走 31 项表（`th06:0x476660`），与蓝点无关 | §3.2 |
| 阶段结算 | ★ **只把「阶内点道具颗数」当**一个数加进 bonus（`+ Point Item %3d`），**不乘分值** | `th06:0x417502` 起的 `Stage Clear` 结算屏：`th06:0x41764c` 读 `0x69d4b4` 直推 |

**顺带一个穷举（对 §4 「清道具池」这类说法有用）**：全 exe 里写 `item+0x141`（in_use）的只有 3 处 ——
`th06:0x41f342`（生成时置 1）、`th06:0x41f8a3`（掉出屏幕底销毁）、`th06:0x42007c`（被收掉）；
写 `item+0x143`（state）的只有 3 处 —— `th06:0x41f39e`（生成时写参数）、`th06:0x41f77f`（进入归航）、`th06:0x420179`。
⇒ **TH06 根本没有「清空道具池」的函数**；死掉 / bomb / 换关都不会删道具（bomb 反而是**全屏吸点**）。

阶段结算的其它行（同一函数 `th06:0x417502` 起）：

| 行 | 算式 | 地址 |
| --- | --- | --- |
| `Stage * 1000` | `current_stage(0x69d6d4) × 1000` | `th06:0x4175ad` |
| `Power *  100` | `power(0x69d4b0) × 100` | `th06:0x4175e0` |
| `Graze *   10` | `graze_in_stage(0x69bcb4) × 10` | `th06:0x417617` |
| `* Point Item %3d` | `point_items_in_stage(0x69d4b4)`，**裸计数** | `th06:0x41764c` |
| `Player    =` | `lives(0x69d4ba) × 3000000` | `th06:0x417689` |
| `Bomb      =` | `bombs(0x69d4bb) × 1000000` | `th06:0x4176b9` |
| `Total     =` | 上面求和（存 `[obj+0x2be0]`） | `th06:0x417866` |

## 5. 量纲：显示值 ↔ 存储值

| 项 | 结论 | 证据 |
| --- | --- | --- |
| `score` 存储值 | `GAME_MANAGER+0x4` = `th06:0x69bca4`，纯 `int32`，**没有定点缩放** | 全 exe 44 处引用 |
| HUD 显示 | 直接 `%.9d` 打印 **`GAME_MANAGER+0x0`**（显示镜像） | `th06:0x41ab7f` `push ds:0x69bca0` + `push 0x46ac24`(`"%.9d"`) |
| 显示镜像 | `+0x0` 由代码逐步拷 `+0x4`（例如 `th06:0x40288b`、`th06:0x402f36`、`th06:0x418ef9`） | 三处 `mov ecx,ds:0x69bca4 / mov ds:0x69bca0,ecx` |
| **有没有 /10** | **没有**。44 处 `score` 引用逐条打开，**没有一处**「读出 score 再除 10」；`idiv` 除数 `0xa` 只出现在十进制拆位（`th06:0x40199c`、`th06:0x401abe`，popup 用）与 RNG/绘图 | `th06:0x41ab7f` 直接 `push ds:0x69bca0` 给 `%.9d` |
| 冒字（popup） | `th06:0x401940` / `0x401a60` 内部 `idiv 10` 逐位拆十进制 ⇒ **印的就是 value 原值** | `th06:0x40199c`、`th06:0x401abe` |
| 因此 | 蓝点冒的 `100000` = 加进 `score` 的 `100000` = HUD 上看到的那 9 位里的 `100000` | — |

playfield：`th06:0x69d6e4`(x) = `0x43c00000` = **384.0f**、`th06:0x69d6e8`(y) = `0x43e00000` = **448.0f**
（`th06:0x42ffe5`/`0x42ffef` 等 6 处写入）⇒ 道具最晚在 `y = 464` 被销毁，
⇒ 难度 0/1 的最低可能值 = `60000 − 336×100 = 26400`（**不会为负**，公式里也没有钳位指令）。

## 6. 未解 / 需要实跑核对

1. ❓ **难度号 ↔ 名字**：`0x69bcb0 ∈ {0,1,2,3,4}`，跳表 5 项 ⇒ 与菜单的 Easy/Normal/Hard/Lunatic/Extra 一一对应是**推断（🟡）**；
   一手只证明「难度 0 与 1 同值」「有 5 档」。要不要钉死，得去菜单写点处（`th06:0x4363ee`/`0x4389bf` 从 eax 写入）读菜单状态机。
2. ❓ `th06:0x41f9c9` 那段（`type 0` 非满火力路径里对 `th06:0x4766dc` 的循环）我做了「拿当前 power 定位档位」读法，
   **没有**逐指令追它后面用到哪里；不影响蓝点结论（`type 1` 不碰这张表）。
3. ❓ 难度 0/1 共用一格是否真是 ZUN 本意（不是编译期表复用）——一手只能证明**两个表项字节相同**。
4. ❓ `ds:0x69bc30`（HUD 状态位字）的完整字段图未做；本文只用到「点道具会置位 `bit9`」。

## 7. 证据 / 可复现

```bash
# 1) 解 .text 并平坦反汇编（本文全部地址来自这份）
python3 - <<'PY'
import struct
d=open('local/th06.v1.02h/th06.exe','rb').read()
open('/tmp/_th06_text.bin','wb').write(d[0x1000:0x1000+0x69000])
PY
objdump -D -b binary -m i386 -M intel --adjust-vma=0x401000 /tmp/_th06_text.bin > /tmp/th06_full.asm

# 2) 直读难度跳表 / 常量表（.rdata RVA 0x6a000→文件 0xc000；.data RVA 0x76000→文件 0x6000）
python3 - <<'PY'
import struct
d=open('local/th06.v1.02h/th06.exe','rb').read()
t=d[0x1000:0x1000+0x69000]                     # .text
print([hex(x) for x in struct.unpack_from('<5I',t,0x420114-0x401000)])   # 难度跳表
print([hex(x) for x in struct.unpack_from('<7I',t,0x4200f8-0x401000)])   # 类型跳表
g=lambda va: struct.unpack_from('<f',d[0xc000+va-0x46a000])[0]
print(g(0x46a534), g(0x46b14c))                # 128.0 / -8.0
PY
```

- 反汇编：`/tmp/th06_full.asm`（134,498 行）；`local/_scratch/th06_text.asm` 是同一节区的早期导出（可直接互校）。
- 命名/结构（**线索**）：`local/vendor/th-re-data/data/th06.v1.02h/{funcs,statics,type-structs-own}.json`
  （`ItemManager::on_tick__` `0x41f4a0`、`ItemManager::spawn_item` `0x41f290`、`ITEM_MANAGER` `0x69e268`、
  `GAME_MANAGER` `0x69bca0`、`PLAYER` `0x6ca628`、`zItem`/`zItemManager`/`zGameManager`/`zPlayer`）。
  ⚠ 该来源把 `th06:0x420130` 记作「RemoveAllItems」，**本文一手读作 `state=1`（全屏吸点）**，
  这是假阳性登记（别照抄那个名字）。
