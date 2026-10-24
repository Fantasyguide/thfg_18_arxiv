# TH18 原生卡 29「疾风木屐 / SPEEDQUEEN / 疾風の下駄」——整张卡的一手档案

> **版本**：TH18 v1.00a（`th18.exe`，imagebase `0x400000`）。本文裸地址默认属该版本；引用其他版本须写成 `th16:0x…`。
> **方法**：`objdump -d`（GNU binutils 2.38，PE i386）+ Ghidra 12.1.3 headless 反编译
> （`tooling/ghidra/scripts/decompile_funcs.py th18 …`）+ `tooling/pe_read.py` 直读 `.rdata/.data`
> + `local/vendor/th-re-data` 的 ExpHP 命名 + `thanm` 解包出的 anm 原文。
> **可信度**：身份/虚表/构造函数/回调 ✅（逐指令）· 移速与判定的**字段与数值** ✅（常量实读）·
> 残像 VM 的链路 ✅ / 它的**观感** 🟡 · 「与我们的卡」的数值 ✅（另一侧的地址在各自文档里）。
> **没有实跑**：本文全部结论来自静态反汇编与数据实读，运行时指针值与画面观感未观测（标 🟡）。
> **前置**：[`12-card29-aya-speedqueen.md`](12-card29-aya-speedqueen.md)（同一张卡的「起步无敌」专题；
> 本文补的是**整张卡**：判定 / 移速 / 残像 / 删除行为）。排版口径见 [`../../../DOCSTYLE.md`](../../../DOCSTYLE.md)。

## 0. 一句话

**疾风木屐 = 一张只作用于「非聚焦」状态的状态卡：不按射击时把自机的直线/斜向步长钉成原生的 2 倍
（8.00 / 5.66 px/帧）、把非聚焦判定缩到 1/3（半径 1.0、AABB 半宽高 0.5）、每帧留一具自机残像；
起步那一帧再 +4 帧无敌。**

## 1. 身份

| 项 | 值 | 一手出处 |
| --- | --- | --- |
| 国服（玩家可见）编号 | **31** | `engine/card/th18/_sources/card-names-cn.md` L45（`31 \| 疾风木屐 \| 射命丸文`）|
| 内部 id | **29**（`0x1d`） | `zTableCardData` 行 `0x4c5aa8` 的 `+0x04` = `0x1d` |
| 内部名（卡表字符串） | **`SPEEDQUEEN`** | 行 `0x4c5aa8` 的 `+0x00` → `0x4b4b2c`（ASCII，实读 `SPEEDQUEEN\0`）|
| 类名（C++ / ExpHP） | **`CardAya`** | 构造函数 `0x41231a`、虚表 `0x4b5510`、`th18-funcs.json` 的 `CardAya::on_tick` 等 |
| 中文名 | **疾风木屐** | 同上 CN 表 |
| 日文名 | **疾風の下駄** | 游戏自带 `ability.txt` 的 `@SPEEDQUEEN` 条目（cp932）|
| 归属 | **射命丸文** | CN 表 L45 + `ability.txt` 正文「射命丸文の能力」|
| 表行地址 | `0x4c5aa8`（58 行里的物理第 34 行，0 起） | 实读 `0x4c53c0 + 34×0x34` |
| 卡面贴图 | `abcard.anm` **entry62** `ability/SPEEDQUEEN_max.png` / **entry63** `_min.png` | 行 `+0x2c = 0x3e` / `+0x30 = 0x3f`；entry 名实读 |

**国服号 vs 内部号**：本仓 `_sources/card-names-cn.md` 开头已钉死两者差 **+2**
（内部 29 ↔ 国服 31）。**写玩家面向的卡名一律用国服号 31**，写代码/地址一律用内部 id 29。

**为什么不是我们那批 58+**：`allocate_new_card` 的 id→类跳转表 `0x412dac` 只有 **57 项**（id 0–56），
每项都是原生类；我们扩的卡从 58 起（`native/cards_def.h` 的 `CE_CARD_ID_MIN 58`）。
29 就在原生表里，构造函数 `0x41231a` 是跳转表第 29 项（实读）。

**⚠ 任务书里说的「`native/cards_def.c` = 卡表」要澄清**：那份文件是**我们自己**的 58+ 卡元数据
校验/编表逻辑（`ce_card_def_t`），**不含**原生卡；TH18 的原生卡表是 exe 里的
`zTableCardData[]` `0x4c53c0`（见 [`01-object-model.md`](01-object-model.md) §6）。

### 1.1 表行 13 个 dword 的实读（`0x4c5aa8`）

| 偏移 | 值 | 语义 |
| --- | --- | --- |
| `+0x00` | `0x4b4b2c` | `internal_name` → `"SPEEDQUEEN"` |
| `+0x04` | `0x1d` | `card_id` = 29 |
| `+0x0c` | `2` | 类别/分页（`f0c`）|
| `+0x10` | `4` | 价格档位（索引价格表 `0x4b35c4`）|
| `+0x14` | `3` | 抽卡权重 |
| `+0x18` | `0` | `dmode` = 无关卡限制 |
| `+0x20` | `1` | 卡组编成菜单可见 |
| `+0x24` | `0` | **不是**新档初期解禁 |
| `+0x28` | `1` | `hud_show`（字段名取 `native/cards_def.h` 的表行注释）|
| `+0x2c` / `+0x30` | `0x3e` / `0x3f` | `sprite_large` / `sprite_small` = abcard entry 62 / 63 |

### 1.2 日文原文（游戏自带 `ability.txt`，cp932）

```text
@SPEEDQUEEN
疾風の下駄

超高速移動が可能になる射命丸文の能力
ショットを撃っていない時は超高速移動出来る
超高速移動中は当たり判定が極小になり、
更に移動開始時の一瞬だけ無敵になる
天狗ならではの詐欺判定を引き起こす能力だ
```

这段是**卡面文案**，不是实现；实现见 §2。它给了三个词：**不射击**（条件）、**判定极小**、**起步一瞬无敌**。
§2 逐条对代码：前两条 ✅ 完全对上，第三条 ✅ 但「移动中」要精确成「**非聚焦**」（见 §2.2）。

## 2. 行为（一手）：它到底改了哪些字段、什么时候生效、有没有上限

### 2.1 这张卡只覆盖 5 个虚表槽

`zVTableCard` 共 21 槽 / 0x54 字节（[`01-object-model.md`](01-object-model.md) §2）。CardAya 的虚表
**`0x4b5510`**，其中只有 5 格不是基类缺省：

| 槽 | 名字（[`03-hooks.md`](03-hooks.md)） | 实现 | 什么时候被调 |
| --- | --- | --- | --- |
| `+0x20` | `on_load` | **`0x40c7b0`** | 关卡开场 / replay 复原。两处调用点都在 `AbilityShop__sub_417880` 里：`0x41791a`（经 `notify_cards_on_load` `0x408ad0`）与 `0x4179d7`（直呼 `call *0x20`）|
| `+0x24` | `on_tick` | **`0x40c7f0`** | 玩家 tick，**每帧**；调用点 `0x45c0e5`（`call *0x24`）|
| `+0x34` | `__on_load__2` | `0x40c790`（`jmp *0x4c`，转发）| `GameThread__thread_start`（每关开场）|
| `+0x4c` | `method_4C` | `0x40c7a0`（`and [card+0x50], ~2`）| 局末重置 |
| `+0x50` | `operator delete` | **`0x40cb50`** | 删卡 / 局末回收 |

构造函数在 **`0x41231a`**：`new(0x5c)` → `memset(self,0,0x54)` → 挂虚表 `0x4b5510` → `+0x54=0`、`+0x58=0`
→ 跳到共用尾部 `0x412cd5`（写 `flags |= 4`、`card_id`、`table_entry`、链表、分类计数）。
**子类只多两个 int：`+0x54`（状态）和 `+0x58`（上一帧的移动方向）。**

### 2.2 `on_load` `0x40c7b0`：把**非聚焦**判定缩到 1/3

一次写三格，都是 `float`（原生值取自 `Player__reset` `0x45ae99`–`0x45aebe`）：

| 字段 | 卡写的值 | 原生 | 谁读 |
| --- | --- | --- | --- |
| `player+0x4799c` 判定圆半径（非聚焦）| **1.0f** | 3.0f | 圆模式子弹判定 `0x45cdda` |
| `player+0x479a4` AABB 半宽（非聚焦）| **0.5f** | 1.5f | 矩形模式判定 `0x45cc5a` |
| `player+0x479a8` AABB 半高（非聚焦）| **0.5f** | 1.5f | 矩形模式判定 `0x45cc62` |

**★ 只写非聚焦那三格**：`+0x479a0`（聚焦圆半径）、`+0x479b0`/`+0x479b4`（聚焦 AABB 半宽高）**一个字节都没动**。
⇒ 按住 Shift 时判定回到原生的 3.0 / 1.5。这正是卡面文案「超高速移動中は判定が極小」的**真实边界**：
超高速本身只在非聚焦时给（§2.3），所以「超高速时判定小」成立；但代码层面它是**「非聚焦时判定小」**。
（社群/我们旧文档里写的「移动中判定极小」应以此为准：判定缩小**不判**你在不在移动。）

**全二进制穷举**：`0x4799c` 的写点只有两处 —— `0x40c7b5`（本卡）与 `0x45ae99`（`Player__reset`）。
`0x479a4`/`0x479a8` 同理只有 `0x40c7bf`/`0x40c7c9` 与 `0x45aeb7`/`0x45aead`。
⇒ **没有第二张原生卡碰这几个字段**，也不存在「逐帧重写」——是**一次写、留到下一次 `Player__reset`**。

⚠ 因此有个静态上可推的副作用：**中弹复活后判定会回到 3.0/1.5**（`Player__reset` 重写），
直到下一次 `on_load`（下一关开场）。这不是我实跑看到的，是「写点只有这两处」的直接推论，标 🟡。

### 2.3 `on_tick` `0x40c7f0`：两个分支

入口第一句就是**「本帧按着 Z（射击）」**的判据：

```asm
40c80a: test DWORD PTR [player+0x4779c], 0x200    ; 0x200 = 本帧按着射击
40c814: je   0x40c869                             ; 没按 ⇒ 走「超高速」支
```

那个 `0x200` 是 `Player__tick_shooting_state` `0x45ea17` 按 `INPUT_HELD` `0x4ca428` 的 **bit0（射击）** 置的
（`0x45ea06 testb $1,[0x4ca428]` → `0x45ea17 orl $0x200,[player+0x4779c]`）。

#### (a) 射击中：把移速还原成 SHT 的原生值

| 指令 | 动作 |
| --- | --- |
| `0x40c816`–`0x40c829` | 取 `player+0x47940`（SHT 行指针）→ `+0x10`（直线速度）× `0x4b9310`（**128.0f**）→ `cvttss2si` |
| `0x40c82d` | `player+0x477b4 = (int)(sht+0x10 × 128)` = **原生直线步长** |
| `0x40c833`–`0x40c844` | 同样取 `sht+0x18`（斜向速度）→ `player+0x477bc` |
| `0x40c84c` / `0x40c853` | `card+0x54 = 1`（状态）、`card+0x58 = 0`（把「上一帧方向」钉成 0）|
| `0x40c85a` | `ret`（本帧**不**加无敌、**不**造残像）|

#### (b) 没射击：四项一起发生

1. **残像**（`0x40c869`–`0x40ca74`）：见 §4。
2. **移速 ×2**：
   ```asm
   40ca81: mov DWORD PTR [player+0x477b4], 0x400     ; 1024 / 128 = 8.00 px/帧（原生 4.00）
   40ca8b: mov DWORD PTR [player+0x477bc], 0x2d4     ;  724 / 128 = 5.66 px/帧（原生 2.83）
   ```
   只写**非聚焦**那两格（`+0x477b8`/`+0x477c0` 是聚焦用的，卡不碰）⇒ 按 Shift 时没有加速。
   两个值恰好是原生直线 `4.0` 与斜向 `4/√2 = 2.828` 的 **2.0 倍**（`0x2d4/0x400 = 0.7070`）。
3. **起步无敌 +4 帧**（`0x40ca95`–`0x40cb2a`）：条件 = `card+0x58 != move_dir ∧ card+0x58 == 0 ∧ focus == 0`；
   加法不是赋值，**没有上限闸**。完整推演见 [`12-card29-aya-speedqueen.md`](12-card29-aya-speedqueen.md) §2–§5。
4. **收尾**：`0x40cb30`–`0x40cb3b` 把 `move_dir`（`player+0x4793c`）写进 `card+0x58`。

#### 量纲：为什么是「除以 128」，不是旧注释说的「除以 256」

`Player__reset` 把 SHT 的四个速度字段装进自机时，**乘数就是 `0x4b9310 = 128.0f`**
（`0x45ae0f movss xmm1,[0x4b9310]` → `0x45ae4b mulss`），而自机位置是 **1/128 px 定点**
（`player+0x62c`，[`../player/th18/01-position-and-state-timers.md`](../../player/th18/01-position-and-state-timers.md) §1）。
⇒ `0x400 = 1024/128 = 8.00 px/帧`。旧注释 `trance.c:2255` 与
`CARD-NATIVE-CONFLICT-AUDIT.md:122` 的「1024/256 = 4.00 px/帧」**把量纲写错了**
（4.00 是原生直线速度，不是这张卡给的值）。这条本文订正，见 §7。

### 2.4 生效时机：卡的回调在 `Player::move` **之后**

`Player__on_tick__body` 的 case 1 顺序（一手）：

```asm
45c0c1: cmpl $0x0, 0x4cf2a4      ; 非商店/非冻结
45c0cc: call 0x45b170            ; ← Player::move（这里读 +0x477b4/+0x477bc 并乘 +0x477ec）
45c0e5: call *0x24(%eax)         ; ← 逐张卡的 on_tick（本卡在这里写速度）
```

⇒ 本卡写进去的步长**从下一帧的 move 才生效**（换挡时差 1 帧）。
而 `+0x477ec`（移速**倍率**）由引擎在 `0x45c702` 每帧复位 `1.0f`，位置在 move 与卡 tick 之后；
我们自己的卡在 `AbilityManager__on_tick`（更早）里乘它 ⇒ **倍率与步长在同一帧内都有效**（§5.1）。

### 2.5 上限 / 边界

| 量 | 上限 | 一手 |
| --- | --- | --- |
| 直线/斜向步长 | **没有**。两个常量，不乘、不夹 | `0x40ca81` / `0x40ca8b` |
| 无敌计时器 | **没有**。`addss` 累加，无比较/钳位 | `0x40cb16`（`0x4b9248 = 4.0f`）+ §12 文档的量 |
| 判定缩小 | **没有**叠加逻辑。一次写死 1.0/0.5/0.5 | `0x40c7b5` / `0x40c7bf` |
| 残像 | 每帧 1 具，生命周期由脚本给（20 帧）⇒ 稳态约 20 具 | §4 |

### 2.6 删卡会怎样（`operator delete` `0x40cb50`）

```asm
40cb50: ...  movl $0x4b6010,(%esi)   ; 虚表退回基类 zCardBaseClass
40cb5c-40cb73: 从链表里摘掉自己
40cb76: testb $1,0x8(%ebp)           ; 「要不要 free」的参数
40cb8a: call 0x48dca1                ; operator delete
```

**只做「摘链 + 还虚表 + free」，不还原 `+0x477b4/+0x477bc`，也不还原判定三格。**
⇒ 若它被**关卡中途**删掉，加速与 1.0/0.5 的判定会**留到下一次 `Player__reset`**（换关/复活才自愈）。
原生删卡的几条路都发生在商店（那一刻卡的回调全停，只有 `+0x50` 跑得到），商店之后紧接下一关开场
`Player::reset` ⇒ 原生流程下不会看出问题。**但我们的卡（倒吊人 / 空白卡）若在关卡中途删原生卡，
就得自己补这一步**。这条按 [`../../../METHOD.md`](../../../METHOD.md) 的「长持续效果必须交代被删掉会怎样」**留档**（本文只记录，不改代码）。

## 3. 反汇编

### 3.1 构造函数 `0x41231a`（id 29 → 类）

```asm
; ---- 0041231a <.text+0x1131a>:
41231a:	6a 5c                	push   $0x5c
41231c:	e8 50 b9 07 00       	call   0x48dc71
412321:	8b f0                	mov    %eax,%esi
412323:	83 c4 04             	add    $0x4,%esp
412326:	89 75 fc             	mov    %esi,-0x4(%ebp)
412329:	c7 06 10 60 4b 00    	movl   $0x4b6010,(%esi)
41232f:	6a 54                	push   $0x54
412331:	c7 46 1c 00 00 00 00 	movl   $0x0,0x1c(%esi)
412338:	83 66 30 fe          	andl   $0xfffffffe,0x30(%esi)
41233c:	83 66 44 fe          	andl   $0xfffffffe,0x44(%esi)
412340:	6a 00                	push   $0x0
412342:	56                   	push   %esi
412343:	e8 a8 d5 07 00       	call   0x48f8f0
412348:	c7 46 04 ff ff ff ff 	movl   $0xffffffff,0x4(%esi)
41234f:	89 76 0c             	mov    %esi,0xc(%esi)
412352:	c7 46 10 00 00 00 00 	movl   $0x0,0x10(%esi)
412359:	c7 46 14 00 00 00 00 	movl   $0x0,0x14(%esi)
412360:	c7 46 18 00 00 00 00 	movl   $0x0,0x18(%esi)
412367:	c7 06 10 55 4b 00    	movl   $0x4b5510,(%esi)
41236d:	c7 46 58 00 00 00 00 	movl   $0x0,0x58(%esi)
412374:	c7 46 54 00 00 00 00 	movl   $0x0,0x54(%esi)
41237b:	e9 55 09 00 00       	jmp    0x412cd5
```

Asm 里的两个 call：`0x48dc71` = `operator_new`（size `0x5c`）、`0x48f8f0` = `_memset`（`self,0,0x54`）；
尾巴的 `0x48dca1` = `operator_delete`、`0x48dc60` = `__security_check_cookie`（名字全部取自 `th18-funcs.json`）。

尾部 `0x412cd5` 是所有卡共用的：`flags = (flags & ~0x4a) | 4`、`card_id`、`array_index = num_total + 1`、
`table_entry = TableCardData__get(id)`、挂链表、按 `flags` 计入 active/equipment/passive 三路计数。

### 3.2 虚表 `0x4b5510`（21 槽）

```text
[ 0] 0x413010   [ 1] 0x413020   [ 2] 0x413030   [ 3] 0x413040
[ 4] 0x413050   [ 5] 0x413060   [ 6] 0x413070   [ 7] 0x413080
[ 8] 0x40c7b0 ← on_load        [ 9] 0x40c7f0 ← on_tick
[10] 0x4130b0   [11] 0x4130c0   [12] 0x4130d0
[13] 0x40c790 ← __on_load__2   [14] 0x4130f0   [15] 0x413130   [16] 0x413140
[17] 0x413150   [18] 0x413160   [19] 0x40c7a0 ← method_4C   [20] 0x40cb50 ← operator delete
```

未列出的格子 = 基类缺省实现（`0x413010`–`0x413173`，多为 `ret`）。逐槽调用点见 [`03-hooks.md`](03-hooks.md)。

### 3.3 `on_load` `0x40c7b0`（全文）

```asm
; ---- 0040c7b0 <.text+0xb7b0>:
40c7b0:	a1 10 f4 4c 00       	mov    0x4cf410,%eax
40c7b5:	c7 80 9c 79 04 00 00 	movl   $0x3f800000,0x4799c(%eax)
40c7bc:	00 80 3f
40c7bf:	c7 80 a4 79 04 00 00 	movl   $0x3f000000,0x479a4(%eax)
40c7c6:	00 00 3f
40c7c9:	c7 80 a8 79 04 00 00 	movl   $0x3f000000,0x479a8(%eax)
40c7d0:	00 00 3f
40c7d3:	33 c0                	xor    %eax,%eax
40c7d5:	c7 41 58 00 00 00 00 	movl   $0x0,0x58(%ecx)
40c7dc:	c7 41 54 00 00 00 00 	movl   $0x0,0x54(%ecx)
40c7e3:	c3                   	ret
```

### 3.4 `on_tick` `0x40c7f0`（全文，`0x40c7f0`–`0x40cb4b`）

> 读法：`esi` 一开始 = `PLAYER_PTR`（`0x4cf410`），`ecx` = 卡对象 `self`；
> `0x40c869`–`0x40ca74` 是**内联的「按 id 找 VM」助手** + 残像 VM 的建/跑/插/贴图/染色，重复块不必逐行看。

```asm
; ---- 0040c7f0 <.text+0xb7f0>:
40c7f0:	55                   	push   %ebp
40c7f1:	8b ec                	mov    %esp,%ebp
40c7f3:	83 ec 1c             	sub    $0x1c,%esp
40c7f6:	a1 4c 4a 4c 00       	mov    0x4c4a4c,%eax
40c7fb:	33 c5                	xor    %ebp,%eax
40c7fd:	89 45 fc             	mov    %eax,-0x4(%ebp)
40c800:	56                   	push   %esi
40c801:	8b 35 10 f4 4c 00    	mov    0x4cf410,%esi
40c807:	89 4d e4             	mov    %ecx,-0x1c(%ebp)
40c80a:	f7 86 9c 77 04 00 00 	testl  $0x200,0x4779c(%esi)
40c811:	02 00 00
40c814:	74 53                	je     0x40c869
40c816:	8b 96 40 79 04 00    	mov    0x47940(%esi),%edx
40c81c:	f3 0f 10 42 10       	movss  0x10(%edx),%xmm0
40c821:	f3 0f 59 05 10 93 4b 	mulss  0x4b9310,%xmm0
40c828:	00
40c829:	f3 0f 2c c0          	cvttss2si %xmm0,%eax
40c82d:	89 86 b4 77 04 00    	mov    %eax,0x477b4(%esi)
40c833:	f3 0f 10 42 18       	movss  0x18(%edx),%xmm0
40c838:	f3 0f 59 05 10 93 4b 	mulss  0x4b9310,%xmm0
40c83f:	00
40c840:	f3 0f 2c c0          	cvttss2si %xmm0,%eax
40c844:	89 86 bc 77 04 00    	mov    %eax,0x477bc(%esi)
40c84a:	33 c0                	xor    %eax,%eax
40c84c:	c7 41 54 01 00 00 00 	movl   $0x1,0x54(%ecx)
40c853:	c7 41 58 00 00 00 00 	movl   $0x0,0x58(%ecx)
40c85a:	5e                   	pop    %esi
40c85b:	8b 4d fc             	mov    -0x4(%ebp),%ecx
40c85e:	33 cd                	xor    %ebp,%ecx
40c860:	e8 fb 13 08 00       	call   0x48dc60
40c865:	8b e5                	mov    %ebp,%esp
40c867:	5d                   	pop    %ebp
40c868:	c3                   	ret
40c869:	80 3d be 17 52 00 00 	cmpb   $0x0,0x5217be
40c870:	a1 f4 cc 4c 00       	mov    0x4cccf4,%eax
40c875:	0f 28 05 d0 94 4b 00 	movaps 0x4b94d0,%xmm0
40c87c:	53                   	push   %ebx
40c87d:	8b 5e 0c             	mov    0xc(%esi),%ebx
40c880:	0f 11 45 ec          	movups %xmm0,-0x14(%ebp)
40c884:	8b 44 85 ec          	mov    -0x14(%ebp,%eax,4),%eax
40c888:	57                   	push   %edi
40c889:	8d be 20 06 00 00    	lea    0x620(%esi),%edi
40c88f:	89 45 e8             	mov    %eax,-0x18(%ebp)
40c892:	74 11                	je     0x40c8a5
40c894:	68 38 17 52 00       	push   $0x521738
40c899:	ff 15 7c d0 4a 00    	call   *0x4ad07c
40c89f:	fe 05 b9 17 52 00    	incb   0x5217b9
40c8a5:	ff 83 34 01 00 00    	incl   0x134(%ebx)
40c8ab:	e8 70 ca 07 00       	call   0x489320
40c8b0:	ff 75 e8             	push   -0x18(%ebp)
40c8b3:	8b f0                	mov    %eax,%esi
40c8b5:	8b cb                	mov    %ebx,%ecx
40c8b7:	56                   	push   %esi
40c8b8:	e8 63 ab ff ff       	call   0x407420
40c8bd:	81 8e 38 05 00 00 00 	orl    $0x1000,0x538(%esi)
40c8c4:	10 00 00
40c8c7:	0f 57 c0             	xorps  %xmm0,%xmm0
40c8ca:	85 ff                	test   %edi,%edi
40c8cc:	75 17                	jne    0x40c8e5
40c8ce:	0f 14 c0             	unpcklps %xmm0,%xmm0
40c8d1:	c7 45 f8 00 00 00 00 	movl   $0x0,-0x8(%ebp)
40c8d8:	8b 45 f8             	mov    -0x8(%ebp),%eax
40c8db:	66 0f d6 86 f0 05 00 	movq   %xmm0,0x5f0(%esi)
40c8e2:	00
40c8e3:	eb 0f                	jmp    0x40c8f4
40c8e5:	f3 0f 7e 07          	movq   (%edi),%xmm0
40c8e9:	66 0f d6 86 f0 05 00 	movq   %xmm0,0x5f0(%esi)
40c8f0:	00
40c8f1:	8b 47 08             	mov    0x8(%edi),%eax
40c8f4:	8b ce                	mov    %esi,%ecx
40c8f6:	89 86 f8 05 00 00    	mov    %eax,0x5f8(%esi)
40c8fc:	c7 46 44 00 00 00 00 	movl   $0x0,0x44(%esi)
40c903:	e8 78 bc 06 00       	call   0x478580
40c908:	56                   	push   %esi
40c909:	8d 45 e8             	lea    -0x18(%ebp),%eax
40c90c:	c7 86 20 05 00 00 00 	movl   $0x0,0x520(%esi)
40c913:	00 00 00
40c916:	50                   	push   %eax
40c917:	e8 34 ba 07 00       	call   0x488350
40c91c:	80 3d be 17 52 00 00 	cmpb   $0x0,0x5217be
40c923:	8b 30                	mov    (%eax),%esi
40c925:	74 11                	je     0x40c938
40c927:	68 38 17 52 00       	push   $0x521738
40c92c:	ff 15 80 d0 4a 00    	call   *0x4ad080
40c932:	fe 0d b9 17 52 00    	decb   0x5217b9
40c938:	85 f6                	test   %esi,%esi
40c93a:	75 07                	jne    0x40c943
40c93c:	33 ff                	xor    %edi,%edi
40c93e:	e9 80 00 00 00       	jmp    0x40c9c3
40c943:	8b c6                	mov    %esi,%eax
40c945:	25 ff 7f 00 00       	and    $0x7fff,%eax
40c94a:	3d ff 7f 00 00       	cmp    $0x7fff,%eax
40c94f:	75 44                	jne    0x40c995
40c951:	8b 0d 5c f6 51 00    	mov    0x51f65c,%ecx
40c957:	8b 81 f0 06 00 00    	mov    0x6f0(%ecx),%eax
40c95d:	85 c0                	test   %eax,%eax
40c95f:	74 11                	je     0x40c972
40c961:	8b 38                	mov    (%eax),%edi
40c963:	39 b7 44 05 00 00    	cmp    %esi,0x544(%edi)
40c969:	74 58                	je     0x40c9c3
40c96b:	8b 40 04             	mov    0x4(%eax),%eax
40c96e:	85 c0                	test   %eax,%eax
40c970:	75 ef                	jne    0x40c961
40c972:	8b 81 f8 06 00 00    	mov    0x6f8(%ecx),%eax
40c978:	85 c0                	test   %eax,%eax
40c97a:	74 15                	je     0x40c991
40c97c:	0f 1f 40 00          	nopl   0x0(%eax)
40c980:	8b 38                	mov    (%eax),%edi
40c982:	39 b7 44 05 00 00    	cmp    %esi,0x544(%edi)
40c988:	74 39                	je     0x40c9c3
40c98a:	8b 40 04             	mov    0x4(%eax),%eax
40c98d:	85 c0                	test   %eax,%eax
40c98f:	75 ef                	jne    0x40c980
40c991:	33 ff                	xor    %edi,%edi
40c993:	eb 2e                	jmp    0x40c9c3
40c995:	69 c8 24 06 00 00    	imul   $0x624,%eax,%ecx
40c99b:	a1 5c f6 51 00       	mov    0x51f65c,%eax
40c9a0:	80 bc 01 1c 0d 00 00 	cmpb   $0x0,0xd1c(%ecx,%eax,1)
40c9a7:	00
40c9a8:	75 04                	jne    0x40c9ae
40c9aa:	33 ff                	xor    %edi,%edi
40c9ac:	eb 15                	jmp    0x40c9c3
40c9ae:	39 b4 01 44 0c 00 00 	cmp    %esi,0xc44(%ecx,%eax,1)
40c9b5:	74 04                	je     0x40c9bb
40c9b7:	33 ff                	xor    %edi,%edi
40c9b9:	eb 08                	jmp    0x40c9c3
40c9bb:	8d b8 00 07 00 00    	lea    0x700(%eax),%edi
40c9c1:	03 f9                	add    %ecx,%edi
40c9c3:	8b 1d 10 f4 4c 00    	mov    0x4cf410,%ebx
40c9c9:	ff 73 38             	push   0x38(%ebx)
40c9cc:	8b 4b 0c             	mov    0xc(%ebx),%ecx
40c9cf:	57                   	push   %edi
40c9d0:	e8 2b b1 06 00       	call   0x477b00
40c9d5:	33 c0                	xor    %eax,%eax
40c9d7:	85 ff                	test   %edi,%edi
40c9d9:	0f 45 c6             	cmovne %esi,%eax
40c9dc:	85 c0                	test   %eax,%eax
40c9de:	75 07                	jne    0x40c9e7
40c9e0:	33 c9                	xor    %ecx,%ecx
40c9e2:	e9 8d 00 00 00       	jmp    0x40ca74
40c9e7:	8b c8                	mov    %eax,%ecx
40c9e9:	81 e1 ff 7f 00 00    	and    $0x7fff,%ecx
40c9ef:	81 f9 ff 7f 00 00    	cmp    $0x7fff,%ecx
40c9f5:	75 4e                	jne    0x40ca45
40c9f7:	8b 35 5c f6 51 00    	mov    0x51f65c,%esi
40c9fd:	8b 96 f0 06 00 00    	mov    0x6f0(%esi),%edx
40ca03:	85 d2                	test   %edx,%edx
40ca05:	74 1a                	je     0x40ca21
40ca07:	66 0f 1f 84 00 00 00 	nopw   0x0(%eax,%eax,1)
40ca0e:	00 00
40ca10:	8b 0a                	mov    (%edx),%ecx
40ca12:	39 81 44 05 00 00    	cmp    %eax,0x544(%ecx)
40ca18:	74 5a                	je     0x40ca74
40ca1a:	8b 52 04             	mov    0x4(%edx),%edx
40ca1d:	85 d2                	test   %edx,%edx
40ca1f:	75 ef                	jne    0x40ca10
40ca21:	8b 96 f8 06 00 00    	mov    0x6f8(%esi),%edx
40ca27:	85 d2                	test   %edx,%edx
40ca29:	74 16                	je     0x40ca41
40ca2b:	0f 1f 44 00 00       	nopl   0x0(%eax,%eax,1)
40ca30:	8b 0a                	mov    (%edx),%ecx
40ca32:	39 81 44 05 00 00    	cmp    %eax,0x544(%ecx)
40ca38:	74 3a                	je     0x40ca74
40ca3a:	8b 52 04             	mov    0x4(%edx),%edx
40ca3d:	85 d2                	test   %edx,%edx
40ca3f:	75 ef                	jne    0x40ca30
40ca41:	33 c9                	xor    %ecx,%ecx
40ca43:	eb 2f                	jmp    0x40ca74
40ca45:	69 d1 24 06 00 00    	imul   $0x624,%ecx,%edx
40ca4b:	8b 0d 5c f6 51 00    	mov    0x51f65c,%ecx
40ca51:	80 bc 0a 1c 0d 00 00 	cmpb   $0x0,0xd1c(%edx,%ecx,1)
40ca58:	00
40ca59:	75 04                	jne    0x40ca5f
40ca5b:	33 c9                	xor    %ecx,%ecx
40ca5d:	eb 15                	jmp    0x40ca74
40ca5f:	39 84 0a 44 0c 00 00 	cmp    %eax,0xc44(%edx,%ecx,1)
40ca66:	74 04                	je     0x40ca6c
40ca68:	33 c9                	xor    %ecx,%ecx
40ca6a:	eb 08                	jmp    0x40ca74
40ca6c:	81 c1 00 07 00 00    	add    $0x700,%ecx
40ca72:	03 ca                	add    %edx,%ecx
40ca74:	8b 55 e4             	mov    -0x1c(%ebp),%edx
40ca77:	c7 81 24 05 00 00 00 	movl   $0xffff0000,0x524(%ecx)
40ca7e:	00 ff ff
40ca81:	c7 83 b4 77 04 00 00 	movl   $0x400,0x477b4(%ebx)
40ca88:	04 00 00
40ca8b:	c7 83 bc 77 04 00 d4 	movl   $0x2d4,0x477bc(%ebx)
40ca92:	02 00 00
40ca95:	8b 42 58             	mov    0x58(%edx),%eax
40ca98:	3b 83 3c 79 04 00    	cmp    0x4793c(%ebx),%eax
40ca9e:	0f 84 8c 00 00 00    	je     0x40cb30
40caa4:	85 c0                	test   %eax,%eax
40caa6:	0f 85 84 00 00 00    	jne    0x40cb30
40caac:	39 83 cc 76 04 00    	cmp    %eax,0x476cc(%ebx)
40cab2:	75 7c                	jne    0x40cb30
40cab4:	8b 83 80 77 04 00    	mov    0x47780(%ebx),%eax
40caba:	83 f8 01             	cmp    $0x1,%eax
40cabd:	72 0c                	jb     0x40cacb
40cabf:	c7 83 80 77 04 00 00 	movl   $0x0,0x47780(%ebx)
40cac6:	00 00 00
40cac9:	33 c0                	xor    %eax,%eax
40cacb:	8b 0c 85 c0 35 4b 00 	mov    0x4b35c0(,%eax,4),%ecx
40cad2:	8b 83 78 77 04 00    	mov    0x47778(%ebx),%eax
40cad8:	89 83 74 77 04 00    	mov    %eax,0x47774(%ebx)
40cade:	85 c9                	test   %ecx,%ecx
40cae0:	74 2c                	je     0x40cb0e
40cae2:	f3 0f 10 09          	movss  (%ecx),%xmm1
40cae6:	0f 2f 0d 74 91 4b 00 	comiss 0x4b9174,%xmm1
40caed:	76 0d                	jbe    0x40cafc
40caef:	f3 0f 10 05 88 91 4b 	movss  0x4b9188,%xmm0
40caf6:	00
40caf7:	0f 2f c1             	comiss %xmm1,%xmm0
40cafa:	77 12                	ja     0x40cb0e
40cafc:	f3 0f 59 0d 48 92 4b 	mulss  0x4b9248,%xmm1
40cb03:	00
40cb04:	f3 0f 58 8b 7c 77 04 	addss  0x4777c(%ebx),%xmm1
40cb0b:	00
40cb0c:	eb 10                	jmp    0x40cb1e
40cb0e:	f3 0f 10 8b 7c 77 04 	movss  0x4777c(%ebx),%xmm1
40cb15:	00
40cb16:	f3 0f 58 0d 48 92 4b 	addss  0x4b9248,%xmm1
40cb1d:	00
40cb1e:	f3 0f 2c c1          	cvttss2si %xmm1,%eax
40cb22:	f3 0f 11 8b 7c 77 04 	movss  %xmm1,0x4777c(%ebx)
40cb29:	00
40cb2a:	89 83 78 77 04 00    	mov    %eax,0x47778(%ebx)
40cb30:	8b 83 3c 79 04 00    	mov    0x4793c(%ebx),%eax
40cb36:	8b 4d fc             	mov    -0x4(%ebp),%ecx
40cb39:	5f                   	pop    %edi
40cb3a:	5b                   	pop    %ebx
40cb3b:	89 42 58             	mov    %eax,0x58(%edx)
40cb3e:	33 cd                	xor    %ebp,%ecx
40cb40:	33 c0                	xor    %eax,%eax
40cb42:	5e                   	pop    %esi
40cb43:	e8 18 11 08 00       	call   0x48dc60
40cb48:	8b e5                	mov    %ebp,%esp
40cb4a:	5d                   	pop    %ebp
40cb4b:	c3                   	ret
```

### 3.5 `operator delete` `0x40cb50`（全文）

```asm
; ---- 0040cb50 <.text+0xbb50>:
40cb50:	55                   	push   %ebp
40cb51:	8b ec                	mov    %esp,%ebp
40cb53:	56                   	push   %esi
40cb54:	8b f1                	mov    %ecx,%esi
40cb56:	c7 06 10 60 4b 00    	movl   $0x4b6010,(%esi)
40cb5c:	8b 4e 10             	mov    0x10(%esi),%ecx
40cb5f:	85 c9                	test   %ecx,%ecx
40cb61:	74 06                	je     0x40cb69
40cb63:	8b 46 14             	mov    0x14(%esi),%eax
40cb66:	89 41 08             	mov    %eax,0x8(%ecx)
40cb69:	8b 4e 14             	mov    0x14(%esi),%ecx
40cb6c:	85 c9                	test   %ecx,%ecx
40cb6e:	74 06                	je     0x40cb76
40cb70:	8b 46 10             	mov    0x10(%esi),%eax
40cb73:	89 41 04             	mov    %eax,0x4(%ecx)
40cb76:	f6 45 08 01          	testb  $0x1,0x8(%ebp)
40cb7a:	c7 46 10 00 00 00 00 	movl   $0x0,0x10(%esi)
40cb81:	c7 46 14 00 00 00 00 	movl   $0x0,0x14(%esi)
40cb88:	74 0b                	je     0x40cb95
40cb8a:	6a 5c                	push   $0x5c
40cb8c:	56                   	push   %esi
40cb8d:	e8 0f 11 08 00       	call   0x48dca1
40cb92:	83 c4 08             	add    $0x8,%esp
40cb95:	8b c6                	mov    %esi,%eax
40cb97:	5e                   	pop    %esi
40cb98:	5d                   	pop    %ebp
40cb99:	c2 04 00             	ret    $0x4
```

### 3.6 等价 C 伪代码（**伪代码，不是原始源码**）

名字按本仓口径；括号里是硬偏移，可直接对回上面的反汇编。

```c
/* 伪代码 —— 由 0x41231a / 0x40c7b0 / 0x40c7f0 / 0x40cb50 反汇编改写，非 ZUN 源码 */

int CardAya__on_load(zCardAya *self)
{
    zPlayer *p = PLAYER_PTR;                 /* 0x4cf410 */
    p->hitbox_r_nf  (+0x4799c) = 1.0f;       /* 原生 3.0（Player__reset 0x45ae99）*/
    p->hitbox_hw_nf (+0x479a4) = 0.5f;       /* 原生 1.5 */
    p->hitbox_hh_nf (+0x479a8) = 0.5f;       /* 原生 1.5 */
    /* ★ 聚焦那三格（+0x479a0 / +0x479b0 / +0x479b4）不写 */
    self->state   (+0x54) = 0;
    self->last_dir(+0x58) = 0;
    return 0;                                /* on_load 的返回值按 OR 累加，本卡不表态 */
}

void CardAya__on_tick(zCardAya *self)
{
    zPlayer *p = PLAYER_PTR;

    /* ---------- 分支 1：本帧按着射击 ⇒ 只还原移速，然后返回 ---------- */
    if (p->flags (+0x4779c) & 0x200) {       /* 0x200 = 本帧按着 Z */
        const uint8_t *sht = p->sht_row (+0x47940);
        p->speed_straight (+0x477b4) = (int)(*(float *)(sht + 0x10) * 128.0f);
        p->speed_diag     (+0x477bc) = (int)(*(float *)(sht + 0x18) * 128.0f);
        self->state   (+0x54) = 1;
        self->last_dir(+0x58) = 0;
        return;
    }

    /* ---------- 分支 2：没按射击 ---------- */

    /* 2a. 残像：每帧一具，贴图 = 自机当下的 sprite */
    zAnmLoaded *anm = *(zAnmLoaded **)((uint8_t *)p + 0xc);   /* plXX.anm */
    static const int ghost_script[4] = { 17, 19, 14, 14 };    /* 0x4b94d0，索引 = 角色 0x4cccf4 */
    zAnmVm *vm = AnmManager__allocate_new_vm();
    AnmManager__sub_407420(anm, vm, ghost_script[CHARACTER]);  /* 克隆脚本模板（stride 0x60c）*/
    vm->flags_hi (+0x538) |= 0x1000;
    vm->entity_pos (+0x5f0) = p->inner.pos (+0x620);           /* x,y,z 一起拷 */
    vm->rotation.z (+0x44) = 0.0f;
    AnmVm__run(vm);                                            /* 先跑一帧 */
    vm->mode_of_create_child (+0x520) = 0;
    int id = AnmManager__insert_in_world_list_back(vm);         /* 拿到 id（内部会再查回 VM）*/
    AnmLoaded__set_sprite(anm, vm, p->vm.sprite_id);            /* ★ 覆盖脚本里的 sprite() */
    vm->color_1 (+0x524) = 0xffff0000;                          /* D3DCOLOR */

    /* 2b. 移速：只写非聚焦两格，钉成常量 */
    p->speed_straight (+0x477b4) = 0x400;    /* 1024/128 = 8.00 px/帧 */
    p->speed_diag     (+0x477bc) = 0x2d4;    /*  724/128 = 5.66 px/帧 */

    /* 2c. 起步无敌：+4 帧，无上限闸 */
    if (self->last_dir(+0x58) != p->move_dir (+0x4793c) &&
        self->last_dir(+0x58) == 0 &&
        p->focus (+0x476cc) == 0)
    {
        uint32_t factor = *(uint32_t *)(0x4b35c0 + 4 * p->timescale_idx (+0x47780));
        float add = 4.0f;
        if (factor) {                                     /* 表项 0 指向 0x4ccbf0 = 游戏速度 */
            float f = *(float *)factor;
            if (f <= 0.95f || f >= 1.01f) add = 4.0f * f;  /* 只在明显变速时改 */
        }
        p->invuln.prev  (+0x47774) = p->invuln.cur (+0x47778);
        p->invuln.cur_f (+0x4777c) += add;
        p->invuln.cur   (+0x47778) = (int)p->invuln.cur_f;
    }

    /* 2d. 尾：缓存本帧方向（全分支汇合）*/
    self->last_dir (+0x58) = p->move_dir (+0x4793c);
}

void CardAya__operator_delete(zCardAya *self, int free_it)
{
    self->vtable = &zVTableCardBase;        /* 0x4b6010 */
    unlink(&self->list_node);               /* +0x0c 那对 next/prev */
    if (free_it & 1) operator delete(self);
    /* ★ 不还原玩家的移速/判定 —— 见 §2.6 */
}
```

## 4. anm 脚本

**结论：这张卡自己不开 `ability.anm` 的脚本；它开的是「自机自己的 anm 的残像脚本」。**

### 4.1 取 anm 的点不是 `ability.anm`

| 证据 | 一手 |
| --- | --- |
| 残像 VM 的模板源 = `*(zAnmLoaded **)(player + 0xc)` | `0x40c87d mov 0xc(%esi),%ebx` → 后面 `ecx = ebx` 传进 `AnmManager__sub_407420` / `AnmLoaded__set_sprite` |
| `ability.anm` 的指针是 `ABILITY_MANAGER+0x0c`，不是 `player+0xc` | [`10-extensibility-limits.md`](10-extensibility-limits.md) §anm 表 |
| 脚本号按**角色**取：表 `0x4b94d0` = `{17, 19, 14, 14}`，索引 `0x4cccf4` = 角色（0 灵梦 / 1 魔理沙 / 2 咲夜 / 3 早苗）| `0x40c875 movaps` + `0x40c884`；`0x4cccf4` 的语义见 `engine/sht/th18/01-file-layout-and-shooterset-index.md` |
| 贴图 = 自机当下的 sprite id：`AnmLoaded__set_sprite(anm, vm, player->vm.sprite_id)` | `0x40c9c9 push 0x38(%ebx)`（= `player+0x14` 那个 vm 的 `+0x24`）|

### 4.2 四个角色实际取到的脚本（`pl00/pl01/pl02/pl03.anm`，正文四家完全相同）

```text
script script17 {          ; 灵梦（pl00）—— 魔理沙是 pl01 script19，咲夜 pl02 script14，早苗 pl03 script14
    layer(13);
    type(1);
    sprite(sprite0);       ; ← 被 set_sprite 覆盖成自机当前帧
    color(60, 0, 192);
    alphaTime(20, 0, 0);
+20: // 20
    delete();
}
```

⇒ **残像寿命 20 帧**，层 13，`type(1)`。脚本里的 `sprite(0)` 随后被卡改写成自机当前贴图，
所以画出来是「自机当前姿势的残影」，不是某个固定图案。每帧一具 ⇒ 稳态约 20 具（60 fps 下 1/3 秒拖尾）。

> ⚠ 同名的 `ability.anm` script14/17/19 是**别的东西**（14 = sprite52 缩放脉冲、17 = sprite62 静置旋转、
> 19 = sprite64–67 四连帧），正文与残像对不上；别拿这三个号去照抄。这是本文顺手排掉的一个坑。

### 4.3 卡面 / 文案

| 资产 | 位置 | 一手 |
| --- | --- | --- |
| 大卡面 | `abcard.anm` entry62 `ability/SPEEDQUEEN_max.png`（256×320，sprite62）| 表行 `+0x2c = 0x3e`；entry 名实读 |
| 小卡面（HUD/图鉴） | `abcard.anm` entry63 `ability/SPEEDQUEEN_min.png`（64×80，sprite63）| 表行 `+0x30 = 0x3f` |
| 名字 + 文案 | `ability.txt` 的 `@SPEEDQUEEN` 段（cp932）| §1.2 |

## 5. 与我们的卡（58+）的交互

### 5.1 移速：**乘法叠加**，不冲突 ✅

- 本卡改的是**基础步长** `player+0x477b4`/`+0x477bc`；
  我们的卡改的是**倍率** `player+0x477ec`（`CE_PLAYER_SPEED_MULT`，`engine.h:509`）。
- `Player::move` `0x45b170` 先按方向选步长（`0x45b590`/`0x45b59a`），最后才乘倍率
  （`0x45b5b6 movss xmm2,[edi+0x477ec]` → `0x45b5db mulss`）⇒ 结构上**相乘**。
- 双方一手：本卡全函数**不碰** `+0x477ec`（grep 全 exe，`+0x477ec` 的写点只有 `0x45c702` 每帧复位 1.0
  与我们自己的卡）；我们这边的写点：113（魔理沙 ×1.15）、59（黑桃 J ×1.1）、116（秋释放 ×1.5）。

| 组合 | 结果（直线 / 斜向，px/帧）| 一手 |
| --- | --- | --- |
| 原生 | 4.00 / 2.83 | `0x4b9310 = 128.0`，SHT 行 `+0x10 = 4.0` |
| 只装疾风木屐 | **8.00 / 5.66** | `0x40ca81` / `0x40ca8b` |
| 疾风木屐 + 113 | 9.20 / 6.51 | ×1.15（`trance.c:2386`）|
| 疾风木屐 + 59 | 8.80 / 6.22 | ×1.10（`sj.c:12`）|
| 疾风木屐 + 113 + 59 | 10.12 / 7.16 | 连乘 |

> 我们**内部**还有一处要留意（与本卡无关，是既有状况）：111 的支援特技「文·超高速」对同一个旋钮是
> **赋值**（`signal.c` 的 `= SIGNAL_HYPER_SPEED_PERMILLE/1000.0f`），与 113/59 的**乘法**谁后跑谁赢。
> 那属于我们自己三张卡的语义问题（`CARD-NATIVE-CONFLICT-AUDIT.md` §H 已登记），**不在本文范围**。

### 5.2 判定：**只有 113（灵梦）会碰**，且已经按「比例相乘」处理 ✅

- 全 exe 写 `+0x4799c`/`+0x479a4`/`+0x479a8` 的只有本卡与 `Player__reset`；我们这边只有 113 写判定。
- 113 是**每帧写目标值**，且只在 `small_hitbox`（灵梦）时写；它用
  `ext = 当前值 / 原生值` **探测一次再粘住**（`trance.c` 的 `s_hb_ext`），所以
  「Aya 写 1.0」会被识别成 `ext = 1/3`：

| 组合 | 非聚焦圆半径 | 非聚焦 AABB 半宽/高 |
| --- | --- | --- |
| 只装疾风木屐 | 1.0 | 0.5 / 0.5 |
| 只装 113（灵梦）| 2.0 | 1.0 / 1.0 |
| 疾风木屐 + 113（灵梦）| **0.667** | **0.333 / 0.333** |
| 疾风木屐 + 113（非灵梦）| 1.0（113 一个字不写）| 0.5 / 0.5 |

- **聚焦档**：本卡不写聚焦三格，所以聚焦时判定是「113 的 2.0/1.0」或「原生的 3.0/1.5」，
  与本卡无关。⚠ 这会让「缩判定」在聚焦时**看起来没生效**——那是原生就有的行为，不是 bug。
- 判定同时是**擦弹半径**的来源（`engine/player/th18/02-graze-and-item-slowdown.md` §1b）：
  本卡把 `r` 从 3.0 缩到 1.0 ⇒ 圆模式有效擦弹半径 `√((r+max(br/2.5,40))² + br²)` 掉约 2.0 px
  （br=4–8 时 43.2–43.7 → 41.2–41.8）。
  读擦弹计数的卡（106/107/110/111/115/116）收益跟着掉一点 —— 这是「缩判定」的**对称后果**，不是冲突。

### 5.3 无敌：**相加/取大，不互吞** ✅

- 本卡直写 `player+0x47778`（`+0x4777c` 累加，`addss`），**绕过**我们 SDK 的声明层。
- SDK 的 `ce_invuln_apply()`（`native/sdk.h:1117`）**只在新值更大时才写**（`if (t->cur < frames)`），
  天然不截短原生给的无敌；反过来原生也不会缩短我们的。
- 我们的 `on_delete`/`on_run_reset` 只把**自己的声明**退回 0（`ce_layer_drop_card`），不碰计时器本身。
⇒ 两条路并存，语义上等于「取较大者 + 原生可累加」。

### 5.4 残像 VM：不冲突，但是**每帧一个**的资源事实

- 不射击时每帧 1 具，20 帧自灭 ⇒ 稳态 ~20 个 VM。我们自己的卡也在建 VM（109 的 HUD、113 的神灵等），
  但各自走 `AnmManager__allocate_new_vm` 的池子，互不认领，**没有覆盖关系**。
- 若哪天出现「VM 不够用」，这张卡是**常态占用 ~20 个**的来源之一（这条只是记一笔，🟡 未实测上限）。

### 5.5 待确认（静态读不出来）

| # | 问题 | 为什么是 🟡 / 怎么验 |
| --- | --- | --- |
| 1 | `player+0xc` 运行时到底指向哪个 `zAnmLoaded`（推定 plXX.anm） | 静态只能证明「是自机自己的 anm + 脚本号按角色取」；实跑读一次指针或看 `+0x11c` 表即可 |
| 2 | 残像的最终颜色 | 脚本写 `color(60,0,192)`，卡又写 `color_1 = 0xffff0000`（ARGB 读 = 不透明红）；谁最后生效要看 `AnmVm__run` 的重算时序，🟡 观感问题 |
| 3 | 中弹复活后判定是否真的回到 3.0 | 由「写点只有两处」推出，未实跑 |
| 4 | 若我们的卡在关卡中途删它，残留移速/判定 | 静态结论「会留到 `Player__reset`」，实跑可验（删卡后看 `+0x477b4`）|

## 6. 复现命令

```bash
# 身份：表行 0x4c53c0 + 34*0x34 → 名字指针与 id
python3 tooling/pe_read.py local/th18.v1.00a/th18.exe 0x4c5aa8 --dwords 13
python3 tooling/pe_read.py local/th18.v1.00a/th18.exe 0x4b4b2c --bytes 16

# id → 构造函数（跳转表第 29 项）
python3 tooling/pe_read.py local/th18.v1.00a/th18.exe 0x412dac --dwords 57

# 虚表 21 槽
python3 tooling/pe_read.py local/th18.v1.00a/th18.exe 0x4b5510 --dwords 21

# 三个回调 + 构造函数
objdump -d --start-address=0x41231a --stop-address=0x412380 local/th18.v1.00a/th18.exe
objdump -d --start-address=0x40c7b0 --stop-address=0x40c7e4 local/th18.v1.00a/th18.exe
objdump -d --start-address=0x40c7f0 --stop-address=0x40cb4c local/th18.v1.00a/th18.exe
objdump -d --start-address=0x40cb50 --stop-address=0x40cb9a local/th18.v1.00a/th18.exe

# 常量 / 脚本号表 / 角色索引
objdump -s --start-address=0x4b9308 --stop-address=0x4b9318 local/th18.v1.00a/th18.exe  # 112/120/128/130
python3 tooling/pe_read.py local/th18.v1.00a/th18.exe 0x4b94d0 --dwords 4              # {17,19,14,14}
python3 tooling/pe_read.py local/th18.v1.00a/th18.exe 0x4b9248 --dwords 1              # 4.0f

# 反编译（Ghidra 12，pyghidra）
source tooling/env.sh && python3 tooling/ghidra/scripts/decompile_funcs.py th18 0x40c7f0 0x40c7b0

# anm 原文（已解包在 local/，gitignored）
sed -n '/^script script17 /,/^}/p' local/th18.v1.00a/anm/pl00/pl00.anm.txt
sed -n '/entry entry62/,/^}/p'    local/th18.v1.00a/anm/abcard/abcard.anm.txt
```

## 7. 可信度账 + 对既有文档的订正

| # | 断言 | 可信度 | 一手 |
| --- | --- | --- | --- |
| 1 | 内部 id 29 / 名 `SPEEDQUEEN` / 类 `CardAya` | ✅ | 表行 `0x4c5aa8`、跳转表 `0x412dac`、虚表 `0x4b5510` |
| 2 | `on_load` 只缩**非聚焦**三格（1.0/0.5/0.5），一次写 | ✅ | `0x40c7b5`/`0x40c7bf`；写点穷举 |
| 3 | 不射击时步长 = `0x400`/`0x2d4` = 原生 ×2 | ✅ | `0x40ca81`/`0x40ca8b`；`0x4b9310 = 128.0` |
| 4 | 射击时从 SHT `+0x10`/`+0x18` 还原 | ✅ | `0x40c816`–`0x40c844` |
| 5 | 起步无敌 +4 帧，无上限 | ✅ | `0x40ca95`–`0x40cb2a`（详见 [`12`](12-card29-aya-speedqueen.md)）|
| 6 | 残像 = 每帧一具、脚本按角色 17/19/14/14、贴图取自机当前帧 | ✅ | `0x40c875`–`0x40c9d0` + 四份 anm 正文 |
| 7 | 回调在 `Player::move` 之后 ⇒ 速度下一帧生效 | ✅ | `0x45c0cc` vs `0x45c0e5` |
| 8 | 删卡不还原速度/判定 | ✅ | `0x40cb50` 全函数（无相关写点）|
| 9 | 与 113/59/116 的移速相乘、与 SDK 无敌不互吞 | ✅ | `0x45b5b6`/`0x45b5db`、`sdk.h:1117` |
| 10 | 残像颜色、复活后判定回 3.0、runtime `player+0xc` | 🟡 | 静态可推，未实跑（§5.5）|

**订正（2 条，都是量纲/口径，不改代码，只改说法）**

1. `native/cards/trance.c:2255` 与 `CARD-NATIVE-CONFLICT-AUDIT.md:122` 把 `0x400` 写成了
   「1024/256 = 4.00 px/帧」。**应为 1024/128 = 8.00 px/帧**（= 原生 4.0 的 2 倍）：
   自机是 1/128 px 定点（`player+0x62c`），`Player__reset` 装 SHT 速度时乘的就是 `0x4b9310 = 128.0f`
   （`0x45ae0f` / `0x45ae4b`）。这条不影响任何实现（写的是常量 `0x400`），只影响注释与审计表里的数。
2. 「移动中判定极小」**应精确成「非聚焦时判定极小」**：判定缩小是 `on_load` 一次写死的非聚焦三格，
   代码里**没有**任何「在移动」的判据；聚焦档读的是另外三格，本卡没写。
   （`08-catalog.md` / `09-community-crosscheck.md` 的社区口径可以保留为「玩家观感」，但引擎结论按本文。）
