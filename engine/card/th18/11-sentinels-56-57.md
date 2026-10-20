# TH18 id 56 / 57 —— 两个「哨兵」到底是什么，能不能让出来

> **版本**：TH18 v1.00a（`th18.exe`，imagebase `0x400000`）。本文裸地址默认属该版本；引用其他版本须写成 `th16:0x…`。
> **性质**：定点逆向。回答 [`10-extensibility-limits.md`](10-extensibility-limits.md) §7 留下的第一题——
> 「路线 B（吃掉哨兵）可不可行」。
> **可信度**：全篇 ✅ 为本次一手反汇编读出；🟡 处逐条注明。

## 0. 结论先行：路线 B 死了

**两个都不是空位，两个都在服役。**

| id | 真身 | 为什么让不出来 |
| --- | --- | --- |
| **56 `NULL`** | ①**全部内联查表（25 处）的「查不到」回退行**；②初始卡组数组里的**「空槽」字面值**（`memset(…, 56, 48)`，**写进存档**）；③一个**必须可分配**的无行为卡对象 | 拿它当新卡 = 任何一次查表失配都会返回你的新卡；且**每一份现有存档里都写着 56**，包括别人的 |
| **57 `BACK`** | 图鉴里**未解锁卡的「卡背」图**（唯一用途） | `allocate_new_card` 的 `cmp ebx,0x38; ja` 根本不放行 57，而跳转表 57 项紧接着就是代码，原地扩不了 |

顺带的收获比结论本身更有用：这一轮把**注册表的两个未命名字段反出来了**，
把 [`10`](10-extensibility-limits.md) 的 12 处边界扩到 **33 处**，
并用**全二进制线性反汇编穷举**（204116 条指令）给出了「为什么写死到这个地步」的解释（§4.5）
与确切工价（**132 处立即数 / 15 个函数**）。

## 1. 两行的原始数据

`zTableCardData[]` `0x4c53c0`，stride `0x34`。两个哨兵**不在表尾**，
而是**表的第 56、57 行**——但表并不按 id 排序（见 §4），所以行号恰好等于 id 只是巧合。

| 字段 | 行 56（`0x4c5f20`） | 行 57（`0x4c5f54`） |
| --- | --- | --- |
| `+0x00` internal_name | `0x4b4c00` = `"NULL"` | `0x4b4c08` = `"BACK"` |
| `+0x04` id | 56 | 57 |
| `+0x08` | 1 | 1 |
| `+0x0c` 类别 | **4** | **4** |
| `+0x10` 价格档 / `+0x14` 权重 / `+0x18` dmode / `+0x1c` | 0 / 0 / 0 / 0 | 0 / 0 / 0 / 0 |
| `+0x20` ★菜单可见 | **1** | 0 |
| `+0x24` ★初期解禁 | **1** | **1** |
| `+0x2c` sprite_large | 116 | **2** |
| `+0x30` sprite_small | 117 | **3** |

### ★ 两个新反出来的字段

**`+0x24` = 初期解禁标志。**存档创建时逐 id 拷进 `unlocked_cards[]`：

```asm
0x463670  XOR EDX, EDX                      ; card_id = 0
loop:     ...内联 get(card_id)...           ; 找不到 → 0x4c5f44 (= 行56 的 +0x24)
0x4636D5  MOV AL, byte ptr [EAX]            ; entry->+0x24
0x4636D7  MOV byte ptr [EDI+EDX+0xd0], AL   ; obj+0xd0+card_id
0x4636DE  INC EDX
0x4636DF  CMP EDX, 0x39                     ; card_id < 57
0x4636E2  JL loop
```

判定为「就是 `unlocked_cards`」的依据是两条独立算式对上：`FUN_00463670` 由存档加载器
`FUN_004637d0` 以 `scorefile + 0x5f4b8` 为 `this` 调用，而 `0x5f4b8 + 0xd0 = 0x5f588`
——正是 `zScoreFile.unlocked_cards` 的偏移。✅

实读全表：`+0x24 == 1` 的只有 id 1–6（EXTEND/BOMB/两个掉落/PENDULUM/DANGO）、16、24、42、56、57。
即**新档一开局就解禁的是这 11 个 id**。

**`+0x20` = 卡组编成菜单可见。**`0x4149de` 处 `CMP dword ptr [EAX], 0x0; JZ 跳过`。
实读：id 8–54 与 **56** 为 1，id 0–7、55、57 为 0。

## 2. id 56（`NULL`）的四重身份

### 2.1 它是所有内联查表的「查不到」回退行 ✅

`TableCardData__get` `0x407d70` 被**大面积内联**。每个内联点长这样：

```asm
          XOR ECX, ECX
          MOV EAX, 0x4c53c4              ; &table[0].id
loop:     CMP dword ptr [EAX], <id>
          JZ  found
          ADD EAX, 0x34
          INC ECX
          CMP EAX, 0x4c5f8c              ; 表尾（= 表基 + 58*0x34 + 4）
          JL  loop
          MOV EAX, 0x4c5f20              ; ← 查不到:行 56，可带字段偏移
          JMP after
found:    IMUL EAX, ECX, 0x34
          ADD EAX, 0x4c53c0
after:
```

`0x4c5f20`（及其 `+0x0c` / `+0x10` / `+0x14` / `+0x1c` / `+0x20` / `+0x24` / `+0x30` 变体
`0x4c5f2c` `0x4c5f30` `0x4c5f34` `0x4c5f3c` `0x4c5f40` `0x4c5f44` `0x4c5f50`）在 `.text` 里
共 **24 处操作数，全部是回退臂**（按反汇编操作数实测，见 §6），横跨
`AbilityMenu__on_tick`、`AbilityShop__initialize`、`CardShop__pick_weighted_random_offer`、
`TableCardData__get`、`FUN_00409310`、`FUN_00416940`、`FUN_00463670`。

> **这就是「NULL」这个名字的含义**：它是查表失败的安全返回值，不是一张卡。
> 把它改成真卡 = 把「找不到」变成「找到了你的新卡」。

### 2.2 它是卡组编成里的「空槽」伪卡 ✅

**决定性证据是一处按值比较**，不是按地址：

```asm
0x415049  LEA EAX, [EDI + 0x304]           ; zAbilityMenu.__card_ids_0x304
          ...在其中定位当前光标项...
0x41506E  MOV EAX, dword ptr [EAX]         ; zTableCardData*
0x415070  CMP dword ptr [EAX + 0x4], 0x38  ; ★ entry->id == 56 ?
0x415074  JNZ 0x0041516f
```

`+0x04` 是 id 字段，`0x38` = 56。**游戏显式判断「选中的这项是不是 NULL」**——
这只有在 NULL 是一个可被光标选中的菜单项时才有意义。

配套证据三条：
- `+0x20 = 1`（菜单可见）且 `+0x24 = 1`（初期即解禁）→ 它**从一开始就出现在列表里**；
- **显示顺序表 `0x4b3600` 的第 57 项就是 56**，而遍历该表的循环上界是
  `0x414b54  CMP EAX, 0x4b36e4`（= 表基 + 57 项）→ **覆盖到它**；
- 它自带一对专属 sprite（116 / 117），不是复用别人的。

**这条链在存档两端都闭合了**（2026-09-01 全二进制扫描补证）：

```asm
; ① 新档创建:整个初始卡组数组填成 56
0x4636e4  PUSH 0x30                  ; size = 48
0x4636ec  PUSH 0x38                  ; ★ value = 56
0x4636ee  PUSH ESI                   ; dst = scorefile+0x5f608
0x4636ef  CALL 0x0048f8f0            ; memset
0x4636fd  MOV byte ptr [ESI], 0x2a   ; 再把第一格设成 id 42 (KOZUCHI)

; ② 卡组编成界面:把选中项的 id 低字节写回存档
0x415115  MOV CL, byte ptr [EDI+ECX*4 + 0x304]   ; __card_ids_0x304[cursor] 的低字节
0x41511c  MOV byte ptr [EDX+EAX + 0x5f608], CL   ; → initial_cards_per_shottype

; ③ 开局:逐字节读回,无条件分配
0x407ee3  MOVZX EAX, byte ptr [EAX+ESI + 0x5f608]
0x407eec  CALL 0x00411460                        ; allocate_new_card(id, 1)

; ④ 事后:按 card->id == 56 把空卡从链表摘掉
0x407feb  CMP dword ptr [ECX+0x4], 0x38          ; card->id (+0x04, 由 0x412cec 写入)
0x407ff9  CALL dword ptr [EAX+0x50]              ; 虚表 +0x50 → 移除
```

结论升级为 ✅：**id 56 是存档里「空槽」的字面值**，不是一个可有可无的菜单项。
`memset(初始卡组, 56, 48)` 是决定性证据——一个纯粹的菜单哨兵不会被写进存档。

### 2.3 它是一个可分配的、无行为的卡对象 ✅

`AbilityManager__allocate_new_card` `0x411460` 的跳转表 `0x412dac` 共 **57 项、无重复目标**，
第 56 项指向 `0x411489`：

```asm
0x411489  PUSH 0x54
0x41148B  CALL 0x0048dc71          ; operator new(0x54)
0x411498  MOV dword ptr [ESI], 0x4b6010
          ...通用初始化...
0x4114D6  MOV dword ptr [ESI], 0x4b4c78   ; ← 最终虚表
0x4114DC  JMP 0x00412cd5
```

对比 id 0（`0x411f4b`）：多一步 `CALL 0x00407da0`（`AbilityManager__reset_cards`），
其余骨架相同。**即 `allocate_new_card(56)` 是合法的，产出一张挂着基类虚表 `0x4b4c78` 的空卡。**

> **它必须可分配，不是「顺手留的空 case」。**§2.2 的链路 ③ 会拿空槽的 56 无条件
> 调 `allocate_new_card`——如果 56 不合法，开局就会有一次失败分配。这也解释了
> 为什么 `allocate_new_card` 的失败路径要「干净地返回 −1」：它本来就常走。

### 2.4 它落在商店池之外 ✅

商店的三个遍历都以 `zAbilityManager` 的「本局是否拥有」数组为轴：

| 位置 | 起 | 止 | 覆盖 id |
| --- | --- | --- | --- |
| `CardShop__pick_weighted_random_offer` `0x416f8f` / `0x41716b` | `mgr+0xc84` | `< 0xd64` | 0–55 |
| `AbilityShop__initialize` `0x41744a` / `0x417527` | `mgr+0xc84` | `< 0xd64` | 0–55 |
| 同上第二轮 `0x417535` / `0x4175e7` | `mgr+0xc84` | `< 0xd64` | 0–55 |

`zAbilityManager` 实测大小 `0xd70`（`operator new` 的实参），数组止于 `0xd64`
→ **对象内还剩 12 字节 = 3 个 dword 的余量**，把上界改成 `0xd68` 在结构体内是放得下的。✅

> ⚠️ ExpHP 的 `zAbilityManager` 在这一带**有偏差**：它把 `0xc68` 起 32 字节标成 `__thread`、
> `0xc88` 起 232 字节标成 `field_c88`，而一手代码明确以 `0xc84` 为数组基址、`0xd64` 为止。
> 两者在 `0xc84`–`0xc88` 重叠。以一手为准。

## 3. id 57（`BACK`）：卡背

**它在整个 `.text` 里没有任何一处按地址引用**（`get_xrefs_to 0x4c5f54` 返回 0），
只被两处**按 id 字面量 `0x39`** 查到，两处都在 `AbilityMenu__on_tick`、
且都挂在「这张卡没解锁」的分支上：

```asm
0x414401  MOV EDX, dword ptr [EDX*0x4 + 0x4b3600]   ; card_id = 显示顺序表[i]
0x41440B  CMP byte ptr [EAX + EDX*0x1 + 0x5f588], CL ; unlocked_cards[card_id] == 0 ?
0x414417  JZ  0x00414494                             ; ← 未解锁,跳去查 id 0x39
...
0x414494  CMP dword ptr [EAX], 0x39                  ; 线性查 id == 57
0x4144D0  CMP dword ptr [EAX], 0x39                  ; 同一帧的第二个 sprite 槽
```

两处随后都把查到的行交给 `FUN_004091a0`，后者用行的 `+0x2c` 当 sprite 建 ANM VM：

```c
AnmLoaded__instantiate_vm_to_ui_list_front(ABILITY_MANAGER_PTR->abcard_anm, ..., script, ...);
AnmLoaded__set_sprite(..., *(int *)(entry + 0x2c));
```

行 57 的 sprite 是 **2 / 3**（整个表里最小的一对，其余卡从 4 起步）
→ **`BACK` 就是图鉴里未解锁卡显示的那张「背面 / ？」图。**✅
名字读作**卡背**（card back），不是「返回按钮」。

**为什么连改都难：**

| 障碍 | 事实 |
| --- | --- |
| 分配不放行 | `0x411460  cmp ebx, 0x38; ja 失败` —— 57 在跳转表之前就被挡掉 |
| 跳转表扩不了 | 表 `0x412dac` 恰好 57 项，止于 `0x412e90`，而 `0x412e90` 的字节是 `55 8b ec …` = **下一个函数的序言**。原地加一项就是覆盖代码 |
| 它不在显示顺序表里 | `0x4b3600` 的 57 项是 id 0–56 的重排，**不含 57** → 它永远不是菜单项 |
| 它也不进 `unlocked_cards` | 初期解禁循环上界 `CMP EDX, 0x39` = id 0–56，写不到 57 |
| 改了就丢卡背 | 那是图鉴里所有未解锁卡的显示依据 |

## 4. 顺带反出来的三件事

### 4.1 注册表**不按 id 排序** ✅

实读行号 → id：行 0–7 是 id 0–7，**行 8 是 id 38**，行 13 是 id 16，行 41 是 id 42……
这解释了为什么 `TableCardData__get` 要做线性查找而不是 `table[id]`——
**它没法直接索引**。同时也说明：想加新 id，往表尾追加一行在语义上是够的（顺序无所谓），
问题全在「表尾在哪」和「谁知道表尾」。

### 4.2 显示顺序表 `0x4b3600` ✅

57 个 dword，`0x4b3600`–`0x4b36e3`，内容是 id 0–56 的一个重排：

```
0..20, 51, 21..40, 54, 41..50, 52, 53, 55, 56
```

即把 MAGATAMA(51)、MUKADE(54) 插到分类中间，NULL(56) 压在最后。

### 4.3 菜单有**两个上界不同**的遍历 ✅

| 循环 | 位置 | 上界 | 覆盖 | 是什么 |
| --- | --- | --- | --- | --- |
| 图鉴 | `0x4143c0`–`0x4145e5` | `CMP EAX, 0x38` = 56 | 顺序表前 56 项（id 0–55） | 固定 56 格；未解锁的画卡背（§3） |
| 卡组编成 | `0x4149b0`–`0x414b59` | `CMP EAX, 0x4b36e4` = 顺序表尾 | 全 57 项（含 NULL） | 按 `+0x20 != 0` 且已解锁筛选，条目数写回 `menu+0x300` / `+0x1c4` |

**上界不同不是笔误**：图鉴要展示「共 56 张」这个固定盘面，所以连未解锁的都占格；
卡组编成只列你能装的，所以要动态计数，并且要把「空槽」（NULL）也列进去。

## 4.5 ★ 为什么会写死到这个地步 —— 全二进制穷举后的解释

方法：`llvm-objdump -d th18.exe` 线性反汇编全 `.text`（**204116 条指令**），
按立即数模式穷举，而不是靠 xref 抽样。**全二进制只有 112 条指令带 `$0x38`/`$0x39`/`$0x3a`
立即数**，其中卡牌子系统占 20 条——可以逐条看完，下表就是全部。

### 4.5.1 三个叠加的原因

**① 二进制里根本不存在「卡牌数量」这个变量。**
没有任何一处从表长算边界——没有 `sizeof(表)/sizeof(行)` 的运行时等价物，也没有
`g_num_cards` 之类的全局。数量既是数组维度、又是循环上界，在 C 源码里天然同源
（一个 `#define` / enum），**编译之后就再也看不出同源了**。这不是疏忽，是静态表的常规写法。

**② `TableCardData__get` 被全量内联，把 1 处放大成 25 处。**
这是关键放大器。表的「起点 / 步长 / 尾界 / 查不到回退行」本该只在一个函数里各出现一次；
内联之后**每个调用点都复制了一整套**。所以 99 处地址立即数里绝大多数是同一段 6 行代码
被复制了 25 遍。旁证：`TableCardData__get` `0x407d70` 本体还在，但几乎没人调用它。

**③ 56 不只是「数量」，还被当成一个「值」在用——而且写进了存档。**
这是最要命的一层。同一个数字同时是：

- 查表失败的**回退行**（25 处，§2.1）；
- 初始卡组数组的**空槽填充值**（`memset(…, 56, 48)`，§2.2）；
- 三处 `id == 56` 的**分支判据**（`0x407feb` / `0x415070` / `0x415e63`）。

**所以不能只把边界 56 改成 57**——你还得回答「那空槽用什么值表示」，
而这个答案已经烤在**每一份现有存档**里了，包括别人的。

### 4.5.2 两张表都被邻居堵死了

| 表 | 结束于 | 紧接着是什么 |
| --- | --- | --- |
| `zTableCardData[]` `0x4c53c0` | `0x4c5f88` | **两个热全局** `0x4c5f88` / `0x4c5f8c`（§6） |
| 显示顺序表 `0x4b3600` | `0x4b36e4` | **另一张索引表**（`0x4337f4  movl 0x4b36e4(,%eax,4), %eax`，`FUN_00430d30`）|
| `allocate` 跳转表 `0x412dac` | `0x412e90` | **下一个函数的序言** `55 8b ec …` |

三张表、三个方向，**一处填充字节都没有**。这不是巧合——链接器本来就不留空隙。
「就地加一行」在这个二进制里是系统性地不成立的。

### 4.5.3 全部字面 56/57 的出现处（穷举，20 条）

| # | 地址 | 指令 | 角色 |
| --- | --- | --- | --- |
| 1 | `0x407eb6` | `mov ecx, 0x38` + `rep stosl` | `reset_cards` 清零「本局拥有」数组 = 56 项 |
| 2 | `0x407feb` | `cmp [ecx+4], 0x38` | ★ `card->id == 56` → 调虚表 `+0x50` 摘除 |
| 3 | `0x411479` | `cmp ebx, 0x38` | `allocate_new_card` 的 id 上界 |
| 4 | `0x4137bb` | `mov [edi+0x1c4], 0x38` | `AbilityMenu__initialize` 图鉴项数 |
| 5 | `0x414394` | `mov [edi+0x300], 0x38` | 图鉴项数 |
| 6 | `0x41439e` | `mov [edi+0x1c4], 0x38` | 图鉴 `zMenuSelect` 项数 |
| 7 | `0x414494` | `cmp [eax], 0x39` | ★ 卡背查表 |
| 8 | `0x4144d0` | `cmp [eax], 0x39` | ★ 卡背查表（第二 sprite 槽）|
| 9 | `0x4145e2` | `cmp eax, 0x38` | 图鉴填充循环 |
| 10 | `0x41495c` | `mov edi, 0x38` | 卡组编成 ANM 清理循环 |
| 11 | `0x415070` | `cmp [eax+4], 0x38` | ★ `entry->id == 56`：空槽判定 |
| 12 | `0x41570d` | `mov eax, 0x38` → `idiv` | 图鉴翻页行数 = 56 ÷ 列数 |
| 13 | `0x4157cb` | `cmp edx, 0x38` | 图鉴光标高亮遍历 |
| 14 | `0x415817` | `mov edi, 0x38` | 图鉴 ANM 清理循环 |
| 15 | `0x415e63` | `cmp [eax+4], 0x38` | ★ `entry->id == 56`：`on_draw` 跳过整段绘制 |
| 16 | `0x418e24` | `cmp ecx, 0x38` | 全收集成就（前 56 行）|
| 17 | `0x4636df` | `cmp edx, 0x39` | 存档初期解禁写入 = id 0–56 |
| 18 | `0x4636ec` | `push 0x38`（memset 的**值**）| ★ `memset(初始卡组, 56, 48)` |
| 19 | `0x4648fc` | `push 0x38`（memset 的**长度**）| `memset(unlocked_cards, 1, 56)`，`ScoreFile__unlock_all` `0x464860` |
| 20 | `0x46d8ec` | `cmp eax, 0x39` | 成就菜单 `0x570980` 字节数组遍历（起点 32）|

★ = **把 56/57 当值用**，不是当数量用。这五条才是真正的钉子。

### 4.5.4 立即数总账（按「要改什么」分层）

| 层 | 内容 | 处数 | 函数数 |
| --- | --- | --- | --- |
| 1 | 表基 `0x4c53c0` 系（含 7 个 `+字段` 变体） | 49 | 9 |
| 2 | 表尾界 `0x4c5f8c` / `0x4c5f88`（**仅作立即数的**） | 25 | 9 |
| 3 | 查不到的回退行 `0x4c5f20` 系（7 个变体） | 25 | 8 |
| 4 | 字面 56 / 57（§4.5.3） | 20 | 12 |
| 5 | `mgr` 本局拥有数组的 `0xc84` / `0xd64` | 6 | 2 |
| 6 | 显示顺序表 `0x4b3600` / `0x4b36e4` | 7 | 1 |
| — | **合计** | **132** | **15** |

层 1–3 的 99 处是「搬表」的价；加上层 4–6 才是「真加一张卡」的价。
**另有三处不是立即数、只能靠改结构体布局**：`zAbilityMenu.__card_ids_0x304` `int[56]`
（紧跟 `num_total_cards`）、`zScoreFile.unlocked_cards` `uint8_t[57]`（11 处按 `+0x5f588` 位移访问）、
`zScoreFile.initial_cards_per_shottype`（2 处按 `+0x5f608` 位移访问）。

> ⚠️ **0x30 vs 0x40 的疑点**：`memset(初始卡组, 56, 0x30)` 只填 48 字节，
> 而 ExpHP 把该数组标成 `uint8_t[4][0x10]` = 64 字节。读写侧用的 stride 确实是 16
> （`0x407eda  shl eax, 4`），所以是 3 行还是 4 行没定论。🟡 下一轮的题目。

## 4.6 ★ 这在源码里长什么样 —— 132 处的绝大多数不是作者写的

**结论：ZUN 的源码里几乎不可能有 132 个字面 `56`。**
二进制里那 132 处，是**编译器把少数几个源码构造摊开**的结果。逐层拆开看：

### 4.6.1 99 处地址立即数 = 一个 37 字节函数被内联了 24 次

`TableCardData__get` `0x407d70` 全文只有 37 字节：

```asm
0x407d70  xor  edx, edx                 ; i = 0
0x407d72  mov  eax, 0x4c53c4            ; p = &table[0].id
0x407d77  cmp  [eax], ecx               ; p->id == id ?
0x407d79  je   命中
0x407d7b  add  eax, 0x34
0x407d7e  inc  edx
0x407d7f  cmp  eax, 0x4c5f8c            ; p < 表尾
0x407d84  jl   0x407d77
0x407d86  mov  eax, 0x4c5f20            ; return &table[NULL]
0x407d8b  ret
命中:     imul eax, edx, 0x34           ; ★ 用 i 重算地址,而不是 eax-4
0x407d8f  add  eax, 0x4c53c0
0x407d94  ret
```

**它同时维护指针 `eax` 和下标 `edx`，命中时用 `i` 重算地址**——这说明源码是**下标式**的：

```c
zTableCardData* TableCardData__get(int id) {
    for (int i = 0; i < ABILITY_TABLE_LEN; i++)
        if (ability_table[i].id == id) return &ability_table[i];
    return &ability_table[ABILITY_NULL];      // ← 源码里就这一处
}
```

「查不到返回 NULL 行」在**源码里只出现一次**。二进制里的 25 份拷贝，
全部来自 MSVC 把这个小函数在每个调用点内联。更进一步的证据是**字段折叠**：
调用点写的是 `get(id)->weight`，编译器把 `+0x14` 预加进两条臂的常量里，于是产生了
`0x4c53dc`（命中臂）和 `0x4c5f34`（回退臂）这样的**成对变体**——
7 个字段变体 × 两条臂，全是同一个源码表达式的不同实例化。

**全二进制只有 30 处「静态表线性扫描」，其中 24 处是这张卡表。**
（其余：`0x4b35c0` 步长 8 的两处、`0x4b36e4` 一处、`0x4ca214` 步长 `0x14` 一处。）
所以这不是 ZUN 到处这么写，而是**这一张表被 UI 反复查**，内联把它放大了。

### 4.6.2 `cmp ebx, 0x38` 也不是手写的边界

```asm
0x411469  cmp  dword ptr [edi+0x28], 0x100
0x411470  jge  0x412da0
0x411476  mov  ebx, [ebp+8]
0x411479  cmp  ebx, 0x38
0x41147c  ja   0x412da0            ; ← 与上面同一个落点
0x411482  jmp  dword ptr [0x412dac + ebx*4]
0x412da0: pop edi; pop esi; or eax, -1; ...; ret 8
```

`cmp/ja` 是 **MSVC 给 `switch` 生成的跳转表范围检查**，`0x38` 由最大 case 标签
（`case ABILITY_NULL:`）自动推出。而它和「持有数 ≥ 256」共用一个 `return -1` 出口，
说明源码里**有** `default: return -1`：

```c
if (this->num_total_cards >= 256) return -1;
switch (id) {
    case ABILITY_BLANK:  ... break;
    ...
    case ABILITY_NULL:   card = new zCardBaseClass(); break;
    default:             return -1;
}
```

> **订正** [`10-extensibility-limits.md`](10-extensibility-limits.md) §3 的措辞：
> 那里说「没有 default 分支」——作为**二进制**陈述成立（没有一个可单独挂钩的 default 标签，
> 它被编译器和 256 检查合并了），但别据此以为**源码**里也没有。源码里有。

### 4.6.3 那作者到底做了什么决定？

把编译器产物剥掉之后，剩下的**真·作者决定**只有三条，而且前两条都是**好设计**：

| # | 决定 | 证据 | 评价 |
| --- | --- | --- | --- |
| 1 | **把 `id` 冻结成序列化值，与表内位置解耦** | 表**不按 id 排序**（§4.1），每行自带 `id` 字段，所以必须线性查 | ✅ 正确。id 进存档和 replay，必须稳定；表要能随便重排、按分类分组 |
| 2 | **定长数组直接落盘** `unlocked_cards[57]`、`initial_cards[…][16]` | `+0x5f588` / `+0x5f608` 的直接位移访问 | ✅ 同人游戏常规做法，省事且快 |
| 3 | **把哨兵 `NULL` 放在枚举末尾，并且让它同时是「空槽的值」** | `memset(初始卡组, 56, 48)`；三处 `id == 56` 分支 | ⚠️ **唯一真正制造麻烦的那一条** |

第 3 条在源码里看起来完全无害——写出来大概是：

```c
enum { ABILITY_BLANK = 0, ..., ABILITY_MAGATAMA2 = 55,
       ABILITY_NULL = 56, ABILITY_BACK = 57, ABILITY_CNT = 58 };

memset(deck, ABILITY_NULL, sizeof(deck));     // 空槽
if (entry->id == ABILITY_NULL) { ... }        // 判空
```

**问题不在于「哨兵」，而在于哨兵的数值紧挨着集合的大小。**
`ABILITY_NULL = 56` 与「56 张真卡」是同一个数——于是「扩大集合」和「保留哨兵」
在数值上直接冲突。如果当初写成 `ABILITY_NULL = -1` 或者放在枚举**开头**，
今天加卡就只是往末尾追加。这是一个**在源码里毫无代价、在二进制里代价极高**的选择。

### 4.6.4 所以根本上是怎么回事

**编译是有损的，而且损失的正是我们需要的那部分。**

源码里加一张卡大概是四处改动（enum 加一项、表加一行、switch 加一个 case、
`ability.txt` 加一段）。编译之后，同一件事变成 132 处立即数——
因为编译器把「一个概念」摊成了 N 份拷贝，**并且不留任何回指**。
没有一处二进制代码说「我这个 `0x4c5f8c` 和那边那个是同一个东西」。

这就是逆向改造的根本不对称：**我们付的不是作者写代码时的价，是编译器展开后的价。**
也顺带解释了为什么 thcrap 生态里 ExpHP 那些 gameplay patch 全是「改行为」而不是「加内容」
——改行为只要动一处，加内容要动被摊开的全部。

## 5. 边界表增补

[`10-extensibility-limits.md`](10-extensibility-limits.md) §2 记了 12 处。本轮又坐实 15 处，
且把其中 4 条 🟡 转成 ✅：

| # | 位置 | 边界 | 含义 |
| --- | --- | --- | --- |
| 13 | `0x4145e2` | `CMP EAX, 0x38` | 图鉴填充循环 = 56 格 |
| 14 | `0x414394` | `MOV [EDI+0x300], 0x38` | 图鉴条目数 |
| 15 | `0x41439e` | `MOV [EDI+0x1c4], 0x38` | 图鉴 `zMenuSelect` 项数 |
| 16 | `AbilityMenu__initialize` `0x413650` | `this+0x1c4 = 0x38` | 同上，初始化时先设一次 |
| 17 | `0x41570d` | `MOV EAX, 0x38` → `IDIV [0x5704bc]` | 图鉴**翻页行数**由 56÷列数 算 |
| 18 | `0x4157cb` | `CMP EDX, 0x38` | 图鉴光标高亮遍历 |
| 19 | `0x415817` | `MOV EDI, 0x38` | 图鉴 ANM 清理循环 |
| 20 | `0x41495c` | `MOV EDI, 0x38` | 卡组编成 ANM 清理循环 |
| 21 | `0x414b54` | `CMP EAX, 0x4b36e4` | 卡组编成循环 = 顺序表 57 项 |
| 22 | 顺序表 `0x4b3600` | 57 dword | 新 id 不在里面就不显示 |
| 23 | `0x415070` | `CMP [EAX+4], 0x38` | **id 56 当哨兵值**：清空槽判定 |
| 24 | `0x414494` / `0x4144d0` | `CMP [EAX], 0x39` | **id 57 当哨兵值**：卡背查表 |
| 25 | `0x4636df` | `CMP EDX, 0x39` | 存档初期解禁写入 = id 0–56 |
| 26 | `0x46d8ec` | `CMP EAX, 0x39` | 成就菜单里 `0x570980` 字节数组遍历（起点 32） |
| 28 | `0x407eb6` | `mov ecx, 0x38` + `rep stosl` | `reset_cards` 清零本局拥有数组 |
| 29 | `0x407feb` | `cmp [ecx+4], 0x38` | **id 56 当哨兵值**：把空卡从链表摘掉 |
| 30 | `0x415e63` | `cmp [eax+4], 0x38` | **id 56 当哨兵值**：`on_draw` 跳过整段绘制 |
| 31 | `0x4636ec` | `memset(初始卡组, 56, 48)` | **id 56 当哨兵值**：写进存档的空槽填充 |
| 32 | `0x4648fc` | `memset(unlocked_cards, 1, 56)` | `ScoreFile__unlock_all` `0x464860` |
| 33 | 顺序表邻居 `0x4b36e4` | 另一张索引表就地起头 | 顺序表也**无法就地加长** |
| 27 | `zAbilityMenu.__card_ids_0x304` | `int[56]`，紧跟 `num_total_cards` | **写第 57 项会踩到计数字段** 🟡（尺寸来自 ExpHP；「紧跟」由 `0x4145d2  MOV [ESI-0x4e8], EAX` 的落址一手佐证） |

**转正的 🟡**：

- #10 `unlocked_cards` 在 `+0x5f588`、按 card_id 索引的 `uint8_t` 数组，长度 ≥ 57
  —— 三处独立一手（`0x418df6` 读写、`0x4636d7` 初始化写、`0x41440b`/`0x41694e` 读）✅。
- #12 全收集成就：`CardCollection__mark_obtained_and_notify` `0x418de0` 遍历**表的前 56 行**
  查 `unlocked_cards[row.id]`，全非零才发成就 `0x1d`。前 56 行的 id 恰为 0–55
  → **NULL/BACK 不参与全收集** ✅。
- #8 `zAbilityManager` 数组 `0xc84`–`0xd64`、对象大小 `0xd70` ✅（见 §2.4）。
- #7 商店池三个遍历的起止 ✅（见 §2.4 表）。

## 6. 路线判据更新

| 路线 | 状态 |
| --- | --- |
| **A. 换皮** | 不变。仍是唯一零边界成本的做法 |
| **B. 吃掉哨兵** | ❌ **判死**。56 是查表回退 + 空槽哨兵，57 连分配都进不去 |
| **C. 整体搬迁** | 工价现在算得出来了，见下 |

### 路线 C 的确切工价

**先说一个订正**：早先按 xref 估的 126 处偏大——xref 会把常量装载与随后的使用各算一次。
按**全二进制线性反汇编实测**（§4.5），只搬表是 **99 处**，全部改完是 **132 处**。下表是搬表那 99 处：

| 立即数 | 含义 | 处数 |
| --- | --- | --- |
| `0x4c53c4` | 内联查表的**起点**（`&table[0].id`） | 25 |
| `0x4c53c0` | 表基（`found` 臂算行地址用） | 13 |
| `0x4c53cc` `0x4c53d0` `0x4c53d4` `0x4c53dc` `0x4c53e0` `0x4c53e4` `0x4c53f0` | 表基 + 字段偏移的 7 个变体 | 12 |
| `0x4c5f8c` / `0x4c5f88` | 表**尾界** | 24 / 1 |
| `0x4c5f20` + 6 个字段变体 | 查不到的回退行 | 24 |
| — | **合计** | **99** |

分布很集中——**只有 9 个函数**（实测总数 100，见 [`card-expand`](../../../mods/th18.v1.00a/card-expand/README.md)）：

| 函数 | 处数 |
| --- | --- |
| `AbilityShop__initialize` `0x4171b0` | 28 |
| `AbilityMenu__on_tick` `0x413af0` | 27 |
| `CardShop__pick_weighted_random_offer` `0x416f50` | 24 |
| `TableCardData__get` `0x407d70` / `FUN_00409310` `0x409310` / `FUN_00416940` `0x416940` / `FUN_00463670` `0x463670` | 4 each |
| `FUN_004160b0` `0x4160b0`（`ability.txt` 解析） | 3 |
| `CardCollection__mark_obtained_and_notify` `0x418de0` | 1 |

99 处机械替换，每处都是 5 字节的 `mov r32, imm32` / `cmp r32, imm32`，
**thcrap 的 binhack 逐条写 `expected` 就能做**。但这只解决了**表**；
§4.5.4 的层 4–6（另 33 处）、§5 的数组边界、以及 `zScoreFile` 的存档格式，一条都没动。

### ⚠️ 表尾之后不是空白 —— 是两个热全局

原始字节 `0x4c5f88: FF FF FF FF FF FF FF FF` 看着像填充，**不是**。
它们是两个各自独立、被大量读写的 `int` 全局：

**`0x4c5f88`** —— `Ending__initialize` `0x42b735` 读、`0x42b751` 写 −1；
`MainMenu__tick_menu_17_achievement` `0x46d532` 写。

**`0x4c5f8c`** —— 12 处读写，横跨 9 个函数：

- 卡牌侧：`AbilityShop__on_tick` `0x417cc7`
- 关卡流程：`GameThread__end_stage` `0x444b97`（读）/ `0x444baa`（写 −1）、
  `GameThread__thread_start` `0x4429ff`、`FUN_00457a60` `0x457af1` `0x457b25` `0x457b4c`
- ECL：`Enemy__ecl_get_int_global` `0x437956`、`Enemy__ecl_get_float_global` `0x43860d`
- 主菜单：`MainMenu__thread_start` `0x464c10`、`MainMenu__tick_menu_08_stage_select` `0x46769d`、
  `MainMenu__tick_menu_17_achievement` `0x46d9cb`

**巧合是双重的**：`0x4c5f8c` 既是卡表的尾界立即数，又是一个 ECL 全局变量的地址。
两种用法在反汇编里长得一样（`CMP EAX, 0x4c5f8c` vs `MOV EAX, [0x004c5f8c]`），
**动手时必须按寻址方式区分，不能按地址一把梭**——这是本轮最容易埋雷的地方。

结论：**表原地向后长这条路是堵死的**，不是「搬走邻表就行」。真要长表，
只能整表搬进 codecave，付上面那 99 处的工价。

### 6.2 ★ `0x4c5f8c` 的语义已经查清：**Extra（第 7 面）单关局标记** ✅（2026-10-19b 一手）

> 起因：`mods/th18.v1.00a/card-expand` 的「符卡练习自动装 108」把 `0x4c5f8c >= 0` 当练习判据，
> 实跑（2026-10-19 01:57）在符卡练习里一次都没触发。查下来是**判据错**，不是探针错。

- **取值只有两种**：`1`（Extra）与 `-1`（其它）。全 `.text` 里只有这 5 个**写**点：
  | 写点 | 值 | 出处 |
  | --- | --- | --- |
  | `0x46d9cb` | **1** | `MainMenu__tick_menu_17_achievement`：Extra 开局（同段还写 `CURRENT_STAGE=7` `0x4cccdc`、关卡数据 `0x4cf428=0x4c99dc`、启动方式 `0x4cf438=6`）|
  | `0x464c10` | −1 | `MainMenu__thread_start` |
  | `0x46769f` | −1 | `MainMenu__tick_menu_08_stage_select` = **关卡练习开局**（同段写 `0x4cf438=4`）|
  | `0x444baa` | −1 | `GameThread__end_stage`（收尾）|
  | `0x457b4c` | −1 | `FUN_00457a60` |
- **读点**（`>= 0` 语义全都自洽于「Extra」）：`0x4429ff` `GameThread__thread_start`（Extra 不重播随机种子）、
  `0x444b97` `end_stage`（收尾走 gamemode 2/4 而不是下一面）、`0x457af1` / `0x457b25`、
  商店 `0x417cc7`（Extra 里 30 帧自动退）、ECL 取系统变量 `0x437956`（int）/ `0x43860d`（float）。
- **ECL 侧实证**（本机 `local/th18.v1.00a/dat/` 的 17 个 `st??.ecl` 逐文件数立即数）：
  系统变量 **`-9899`** 只有 **`st07.ecl` / `st07bs.ecl` 各 1 次**，其余 15 个脚本 **0 次**
  ⇒ 只有第 7 面（Extra）用它。反过来，系统变量 **`-9907` = `0x4ccd14`**（= **本局练的符卡 id**）被
  `st01`–`st07` 的全部 boss 脚本引用（`st01bs` 5 次 … `st05bs` 9 次 / `st06bs` 13 次 / `st07bs` 13 次）
  ⇒ 那才是**符卡练习**的判据。
- **符卡练习的真判据（一手）**：`0x4cf438 == 5`（本局启动方式 = 符卡练习，唯一写点
  `MainMenu__tick_menu_14_spell_prac_diff_select` `0x46cfce`）+ `0x4ccd14 >= 0`（本局练的符卡 id，
  唯一非负写点 `0x46cffe`，值 = 该面符卡列表 `menu+0x5d34[cursor]`；普通开局 `0x466be0` / 回放 `0x468025` /
  Extra `0x46d9c1` 都写 −1）。引擎自己在 `ReplayManager__initialize 0x461a0d..0x461a2f` 按 `0x4ccd14 >= 0`
  把 `GAME_FLAGS & 0x30` 置成 `0x20`（= 商店 / MSG / trance 卡读的那个「练习」位），所以
  `DAT_004cccc8 & 0x30 == 0x20` 是**派生值**、不是独立信号。
- **对搬表的意义**：`0x4c5f8c` 只是「一个 int 全局」，`0x4c5f88` 同理（`Ending__initialize` `0x42b735` 读、
  `0x42b751` 写 −1，Extra 菜单 `0x46d532` 写）——搬表时**只搬表、两个全局原地不动**即可；
  §4.5 那 99 处里属于「热全局」的那些（`mov r32,[0x4c5f8c]` 与 `cmp r32,0x4c5f8c` 同址不同用法）必须按**寻址方式**分类。

## 7. 本次未查

- 卡组编成界面里 NULL 条目的**实际观感**（机制已 ✅ 闭合，只是没看过它长什么样）。
- ~~`0x4c5f88` / `0x4c5f8c` 这两个全局各是什么语义~~ → **`0x4c5f8c` 已结案** = Extra 单关局标记（§6.2 ✅）；
  `0x4c5f88` 仍 🟡（只有 `Ending__initialize` 读 / 写 −1 与 Extra 菜单写）。
- ~~三个卡牌 ANM 的 sprite 索引余量~~ → 已解包（2026-09-04）：`abcard.anm` 正好 118 个 sprite（0..117，
  2/3 = `BACK_max`/`dummy`，116/117 = `empty_max`/`dummy`，与两行哨兵完全对上），格式层可无限追加；
  运行时上限仍 🟡。见 [`10-extensibility-limits.md`](10-extensibility-limits.md) §5。
- `zScoreFile` 中 `0x5f5c1`–`0x5f608` 那 71 字节未知区是否空闲（`unlocked_cards` 后紧跟的空档，
  是唯一可能不改存档尺寸就扩解锁位的地方）。
- `initial_cards_per_shottype` 到底是 `[3][16]` 还是 `[4][16]`（§4.5.4 的 `0x30` vs `0x40` 疑点）。
- replay 里卡组的存储格式与长度上限。
