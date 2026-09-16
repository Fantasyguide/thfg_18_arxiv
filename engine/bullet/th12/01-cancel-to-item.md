# TH12 星莲船 消弹 → 出不出实体道具

> **版本**：TH12（`local/th12.v1.00b/th12.exe`，md5 `212f187591f4f1fa99eb2845d1b3cf20`）。本文裸地址默认属该版本；
> 引用别作一律写 `th18:0x…`。样本 737280 字节，`PE32 executable (GUI) Intel 80386`。
> **一手工具**：`objdump -d -M intel --no-show-raw-insn …`（写本文时本机重跑，dump 落 `/tmp/can/th12.asm`，203781 行）；
> 常量按 PE 节表把 VA 映射回文件偏移、读原始字节验值（脚本见 §4）。**非一手**：`local/th12.v1.00b/th12-funcs.json`（ExpHP，只用来取名）。
> **可信度**：✅ 主体——本文每条结论都写「地址 + 指令原文 + 常量真值」，且我**把下面每一条都在本机复核过一遍**（不是转抄）；
> 🟡 / ❓ 未钉死项集中在 §3.3，就地标出的在 §1.1 与 §2.4。
> **相关**：宿主对照 [`../th18/01-cancel-to-item.md`](../th18/01-cancel-to-item.md) · TH12 飞碟全链 [`../../ufo/th12/01-ufo-system.md`](../../ufo/th12/01-ufo-system.md) ·
> 素材 `local/_scratch/research-cancel-original-th12-th17.md` §1（本文对它有复核与两处措辞订正）。

## 0. 一句话

TH12 的「消弹出不出实体」**不是 mode 号，是一个布尔开关**：核心入口 `0x40caa0` 的第 2 个栈参 `spawn_items`（0/1）。
为 1 时**每颗被扫到的弹就地变 1 个 `type 9` 实体道具**（落地、走过去捡：PIV 计数器 `+10`、分数 `+1`）；为 0 时连一颗都没有。
★ 两条**本作独有**：**`etClearAll`（op 510）也出货**、**boss 终盘击破也出货**（TH13–TH18 都不是）。
★ **场地门外扩边距是 `2.0`，不是后面几作的 `32.0`**。★ 全链**不消耗 RNG**（角度/速度都是常量）。

## 1. 发现（一手事实）

### 1.1 核心入口 `0x40caa0`（`BulletManager__cancel_in_radius`）

- `ret 0xc`（`0x40cbd5`）⇒ **3 个栈参，没有第 4 个**：`[esp+0x24]` = `arg0 = float 半径`、
  `[esp+0x28]` = `arg1 = int spawn_items`、`[esp+0x2c]` = `arg2 = int require_flag`（`0x40cac7` / `0x40cb87` / `0x40cbbe` 三处读法一致）。
- 循环：`mov ebp,0x7d0`（`0x40cad5`，**2000 个弹槽**）＋ `add esi,0x9f8`（`0x40cbab`，槽步长 `0x9f8`）；
  存活判定 = `[弹+0x532] ∈ {1,2}`（`0x40cadb`/`0x40cade` 起的 `movzx` 两连比较）。
- **圆心不在栈参里**：它随调用约定走 **`ebx`**。证据：512 的 handler `0x419086 lea esi,[ebx+0x34]` ＋ `0x41908a mov ebx,esi`
  （`ebx+0x34` = ECL 上下文的位置字段）；玩家侧同形 `0x436c46 lea esi,[edi+0xa08]` ＋ `0x436c53 mov ebx,esi`；
  本函数尾部把同一个 `ebx` **转交**给敌人那一遍：`0x40cbc7 push ebx`。
- 半径还有一个**前置缩放**：若 `[0x4b43cc]+0x7c & 1` 且 `[+0x78] ∈ [0x60,0x63]` ⇒ `半径 /= [0x4a3fd8]`（`0x40cac7`–`0x40cad1`）。
  `[0x4a3fd8]` 按字节读 = **double 3.0**。该条件的语义 🟡 未钉死（见 §3.3）。

```asm
40cb38  ; 比 (dx²+dy²) 与 (半径+弹半径)²（x87，fcompp/fnstsw/test ah,0x41）
40cb67  call 0x40c8b0            ; Bullet__cancel（转取消态）★ 先取消
40cb7e  call 0x40d560            ; 场地门 → eax: 0=场内 / 1=场外
40cb85  jne 0x40cbab             ; 场外 ⇒ 跳过撒道具（★ 弹已经被取消了）
40cb87  cmp [esp+0x28],eax       ; ★★ arg1(spawn_items) == 门返回值 ⇒ 跳过
40cb8d  fld [0x4a41f4]           ; 速度 = 0.6f（常量）
40cb9a  fld [0x4a4060]           ; 角度 = −π/2（常量，不取 rand）
40cba4  push 0x9 ; call 0x4273f0 ; ★ 每颗弹 1 个 item type 9
40cbc8  call 0x414f40            ; ★ 扫完弹槽后，再来一遍「敌人」的（见 1.4）
```

### 1.2 场地门 `0x40d560` 与场地常量（★ 边距 2.0）

- 门函数：`(ecx = pos, [esp+4] = x 边距, [esp+8] = y 边距)`，返回 **0 = 场内（可以撒）/ 1 = 场外（跳过）**。
- 界值（`0x4a3d60` / `0x4a3d28` / `0x4a3d58` / `0x4a3d50`，**都是 double**）：`−192.0` / `192.0` / `0.0` / `448.0`。
- ★ **调用处传的边距 = `[0x4a4014]` = float `2.0`**（`0x40cb6c fld [0x4a4014]`，两轴同值）——
  TH13/TH16/TH17/TH18 同位置传的是 `32.0`。⇒ **TH12 的「不撒区」比后面几作窄 15 倍**。
- 同一族里 `0x414f40`（敌人那一遍）也吃同一套界值：`0x4a3d00` = double **2.0**（另一处的同名常量），另有 `0x4a3d68` = double 16.0。

### 1.3 撒出来的是什么

- `ItemManager__spawn_item` `0x4273f0`（**实体**，type 9 走普通道具池；道具 anm 脚本 = 类型 + `0xa5` ⇒ type 9 = 脚本 174）。
- `ItemManager__on_tick__body` `0x425c20` 在 `0x426174` 用 `type-1` 查 18 项跳转表 `0x426f7c`；**type 9 → `0x426c24`**：
  `fld [0x4a4210]`（= float **0.1**）→ `call 0x427ca0`，`esi = 0x4b0c40`。
- PIV 助手 `0x427ca0`：`[esp+4] × [0x4a3ce8]`（= double **100.0**）→ ftol → `add [esi+0x38], eax`（钳 `[esi+0xb4]`）
  ⇒ **`[0x4b0c40+0x38] += 10`**。横向标定同表的 type 3（`0x426a70`）用 `[0x4a41fc]` = float 200.0 ⇒ `+20000`。
- 分数：type 9 尾部 `push 0xa` → 公共尾 `0x426d9a mov ecx,0x4b0c40; call 0x40e730`；
  `0x40e730` = `arg/10`（magic `0x66666667`＋`sar 2`）加到 `[ecx+0x4]`，钳 `0x3b9ac9ff` ⇒ **`0xa/10` = 分数 +1**。
- 每颗 **1 个**（弹幕侧 `0x40cba6` 一处 `push 0x9`，循环体内每弹一次）。
- 激光侧同值：`LaserLine__method_18` `0x429ddc` 前 `0x429dc1`–`0x429ddc` 是同一对常量（速度 `0.6f`、角度 `−π/2`）。

### 1.4 ★ 本作独有：扫完弹槽，再遍历**敌人链表**各撒 1 颗

- `0x414f40` 是**独立函数**（`ret 0xc`）：`mov eax,[0x4b43dc]; mov eax,[eax+0x68]`（`0x414f40`/`0x414f45`，**敌人链表头**）、
  逐节点 `test [enemy+0x26f8],0x800`（`0x414f5f`，起手/存活位）、坐标取 `[enemy+0x1074]`（`0x414f76`/`0x414f88`）、
  撒点 `0x41508e push 0x9; call 0x4273f0`。
- **唯一调用点 = `0x40cbc8`**（`cancel_in_radius` 扫完弹槽之后，我全 `.text` 扫 `call` 只命中 1 次）
  ⇒ **每个半径内的敌人也各吐 1 颗 type 9**（出不出由同一个 `spawn_items` 定）。

### 1.5 消弹入口全集（含 ECL opcode ↔ handler）

| 路径 | 地址链（一手） | 撒不撒 |
| --- | --- | --- |
| ECL **512 `etCancel`** | handler `0x419071` → `0x40caa0(spawn=1, require=0)`（`0x419082 push 0x0` / `0x419084 push 0x1`） | ✅ |
| ECL **513 `etClear`** | handler `0x4190aa` → `0x40caa0(spawn=0, require=0)` | ❌ |
| ECL **532 `etCancel2`** | handler `0x4190e3` → `0x40caa0(spawn=1, require=1)` | ✅ |
| ECL **533 `etClear2`** | handler `0x41911c` → `0x40caa0(spawn=0, require=1)` | ❌ |
| ★ ECL **510 `etClearAll`** | handler `0x4187af`：`push 0x1; call 0x40cf60` ＋ `call 0x428750`（激光全清） | ✅ |
| 玩家炸弹（共用） | `0x408068` / `0x408547` / `0x409294`：`spawn = ~([0x4b43cc]+0x7c) & 1`、require=1 | 非符卡时 ✅ / 符卡中 ❌ |
| 激光 | `LaserManager__cancel_in_radius` `0x4286f0` → `laser->vtable[0x1c]`；类实现里同样 `push 0x9` | ✅ |
| 玩家中弹 / 死亡 | `0x436c6c` 与 `0x436c8b` 两处（**两处 spawn 都是 0**，见 §2.4）；另有 `0x436c2c`＋`0x436c35` 的分支（🟡） | ❌ |
| 擦弹 | 一手：TH12 擦弹**不消弹**（type 9 的全部 spawn 点都落在取消/激光函数里） | ❌ |
| 敌机死亡 / `enmKillAll` | 引擎侧不清弹（取消入口的调用点全集里没有敌机管理器） | — |

- **激光侧那道开关我也追到底了**（这是本文里唯一一处「不进类方法就说不清」的）：
  `0x4286f0`（`ret 0xc`）把 `(pos, 半径, X, Y)` 转给 `laser->vtable[+0x1c]`（`0x428733 mov eax,[eax+0x1c]`）；
  类方法里 `0x429d9c cmp [ebp+0x10],0 / je 0x429de3` 就是开关（`[ebp+0x10]` = 调用方**第二次** push 的那个 int），
  后面 `0x429db8 call 0x42e820`（场地门）→ `0x429dc1`/`0x429dd0` 两个常量 → `0x429dd9 push 0x9; call 0x4273f0`。
  该方法 = `LaserLine__method_18`（`0x429bc0` 起 1583 字节，ExpHP 名），它在文件里**只有一处 dword 引用 = `0x4a066c` = vtable `0x4a0650` + `0x1c`** ⇒ 与 `0x428733` 的槽号严丝合缝。
- ECL 四个 op 传给激光两个 int（push 原文）：**512 / 532 = `(1,1)`**（`0x419098`/`0x41909a`、`0x41910a`/`0x41910c`），
  **513 / 533 = `(1,0)`**（`0x4190d1`/`0x4190d3`、`0x419143`/`0x419145`）—— 而开关取的正是那个 `0` ⇒
  **`etClear`(513) / `etClear2`(533) 的激光半边一颗都不出**，512/532 出（与弹幕侧同步）。
- **ECL opcode → handler 是机械可验的**：handler 指针跳转表基址 `0x4197f4`，**索引 = opcode − 510**。
  我按表读出来是：`[0x4197f4] = 0x4187af`(510)、`[0x4197fc] = 0x419071`(512)、`[0x419800] = 0x4190aa`(513)、
  `[0x41984c] = 0x4190e3`(532)、`[0x419850] = 0x41911c`(533)，与 eclmap 的 op 名逐条对上（`local/vendor/eclmap/eclmap/th12.eclm`）。

### 1.6 ★★ 本作独有：飞碟开奖的扩圈 `etCancel`

- **本轮我重跑了 ECL 反汇编**（命令见 §4）：`local/th12.v1.00b/dat/default.ecl` 里
  `UFO_EtBreak()` = `%A = 16.0f` → 循环 `etCancel(%A)` ＋ `%A += 6` → `if (%A < 112.0f)` ⇒ **半径 16→112，每帧 +6**；
  `UFO_EtBreak2()` = `%B = 16.0f` → 循环 `etCancel(%B)` ＋ `%B += 16` → `if (%B < 640.0f)` → `delete()` ⇒ **半径 16→640，每帧 +16**（全屏扫）。
- 召唤飞碟时就带一圈：`UFO_Blue()` / `UFO_Green()` 在 `anmSelect(1); anmSetSprite(0,136);` 之后紧接着就是 `@UFO_EtBreak() async;`。
  击破飞碟时由 `0x44af00` 在 UFO 位置生成 `UFO_EtBreak2` 敌机（该函数侧见 [`../../ufo/th12/01-ufo-system.md`](../../ufo/th12/01-ufo-system.md) §8，**那一节我没复核**）。
- ⇒ 扩圈扫过的每颗弹都变 1 个 type 9。⚠ `default.ecl` 里名字近似的 `Ecl_EtBreak_ni`（步长 `+10`）/ `Ecl_EtBreak2_ni`（步长 `+6`）
  用的是 **`etClear`**（不出货）—— **两套名字只差 `_ni`，行为相反**，别混。
- ⚠ **与仓库既有文档的冲突（照实登记）**：`engine/ufo/th12/01-ufo-system.md:155` 写 `UFO_EtBreak2` 是「每帧 +6 到 640」，
  而 `default.ecl` 的指令原文是 **`+16`**（`thecl` 输出里 `+1: 16; addf(); setf(%B)`）。**以脚本原文为准**；
  该文件不属本文范围，我**没有**去改它。

### 1.7 boss 击破 / 符卡结束 / 关卡切换

- **符卡打掉 / 捕获** = `etCancel(640.0f)`；**超时** = `etClear(640.0f)` —— 两者都紧跟 `unless ($TIMEOUT == 0)` 分支，
  在 8 个 `dat/stage0*.ecl` 里逐个出现过（`stage01.ecl:183` / `stage02.ecl:137,480` / … `stage07boss.ecl:80,170,…`）⇒ 前者 **✅ 撒**、后者 **❌**。
- ★★ **boss 终盘**（我把 8 个 `dat/stage0*.ecl` 全过了一遍，原文如右）：`void BossDead()` 里
  `unless ($TIMEOUT == 0) goto BossDead_308;` → **击破支**：`enmCreate("Ecl_EtBreak2", …)`；
  `BossDead_308:`（= 超时支）`enmCreate("Ecl_EtBreak2_ni", …)`；两支合流后 `BossDead_364: enmKillAll(); +60: etClearAll(); spellEnd();`。
  ⇒ `Ecl_EtBreak2` 是 **`etCancel`** 环（出货）、`_ni` 是 **`etClear`** 环（不出货）——
  **但两条支路都要过同一个 `etClearAll()`**，而本作 `etClearAll` 的 handler 传的是 **spawn=1**
  （`0x4187af push 0x1`，`0x40cf60` 的开关在 `[esp+0x5c]`，逐颗撒在 `0x40d099`–`0x40d0b9`）
  ⇒ **TH12 的 boss 终盘（不管击破还是超时）都会把场上剩余的弹变成 type 9**（TH13–TH18 都不出）。
  ⚠ 素材的那段摘录**漏了 `unless ($TIMEOUT == 0)` 那一行**，读起来像「击破支用的是 `_ni`」——**是摘录不完整，不是结论错**（结论方向一致，且比它更强）。
- **关卡切换 / 对话**：★ **不清弹**。TH12 全部取消入口（`0x40c8b0` / `0x40caa0` / `0x40cd00` / `0x40cf60` / `0x414f40` / `0x4286f0` / `0x428750`）
  的调用点 owner 只有 `EnemyData::run_ecl_256_plus`、`Bomb::on_tick_*`、`Bullet::run_ex`、`Player::on_tick`、两个 `cancel_in_radius`
  —— **没有一个 `GuiMsg*`**（`GuiMsgVm::run` = `0x41fcc0`，不在其中）。

## 2. 验证（怎么排除掉别的路）

1. **穷举调用点**（全 `.text` 扫 `call rel32`，`grep -c "call   0x…$"`）：`0x40caa0` × **9**、`0x40c8b0` × 8、`0x40cd00` × 6、
   `0x40cf60` × 2、`0x414f40` × **1**、`0x4286f0` × 9、`0x428750` × 5。9 个 `0x40caa0` 调用点 =
   3 个炸弹（`0x408068`/`0x408547`/`0x409294`）＋ 4 个 ECL handler（`0x41908f`/`0x4190c8`/`0x419101`/`0x41913a`）＋ 玩家 2 处（`0x436c6c`/`0x436c8b`）。
   **没有第 10 个** ⇒ §1.5 的表就是全集。
2. **死代码证否**：`0x40cbe0` 与 `0x40ce20` 的直接 `call` = **0**（全文件先当候选发现器，再回指令边界确认）。
   两个函数体我一并看进族里，但**没有任何一处调用它们**。
3. **常量不是抄的**：`2.0` / `0.6f` / `−1.5707964f` / `−192.0` / `192.0` / `0.0` / `448.0` / `0.1f` / `200.0f` / `100.0` / `3.0`
   都按 PE 节表把 VA 映射回文件偏移读了原始字节（§4 脚本），值写进 §1。
4. **两处措辞我按指令边界订正了素材**（不是照抄）：
   - **门的位置**：素材 §1③ 同时写了「门不过就整颗跳过，**连取消都不做**」和「不撒时弹已经被取消了」。
     实测顺序是 `0x40cb67 call 0x40c8b0`（**先取消**）→ `0x40cb7e call 0x40d560`（门）→ 撒道具
     ⇒ **只有前一种说法成立**：门只决定「撒不撒」，**不决定「取不取消」**。
   - **玩家中弹**：素材写「`0x436c6c` 传 spawn 或 0」。实测**两处都是 0**：`0x436c43 push 0x1`(=require) ＋ `0x436c45 push ecx`(=0，`0x436c12 xor ecx,ecx`)
     与 `0x436c75 push 0x0` ＋ `0x436c7d push 0x0`。⇒ 玩家中弹在 TH12 **一颗都不出**。
5. **判据不是「有没有 `push 0x9`」**：撒点只有一处弹幕侧（`0x40cba6`）＋激光侧各一处；「不撒」是**入口的开关**先短路，
   不是靠撒点缺失 —— 所以「某函数里有 `push 0x9`」不能反推它会被调用（§1.5 靠调用点穷举，不靠这个）。
6. **激光侧是「追进类方法」验的**（不是靠调用点）：`0x4286f0` 的槽号从指令读出（`0x428733` = `+0x1c`），
   该槽在文件里唯一的 dword 引用是 `0x4a066c`（= `0x4a0650 + 0x1c`），类方法的开关 `0x429d9c` 读的是 `[ebp+0x10]`，
   而那个位置正好是调用方的第二次 push —— 四组实参（512/532 = `(1,1)`、513/533 = `(1,0)`）因此可判。
7. **ECL 侧我重跑了脚本反汇编**（不是转抄素材）：`thecl -d 12` 过 `dat/default.ecl` ＋ 8 个 `dat/stage0*.ecl`，
   §1.6 的扩圈步长、§1.7 的击破/超时分支都是**在脚本原文里逐行看到的**（顺带发现仓库既有 UFO 文档一处数值冲突，已登记在 §1.6）。

## 3. 结论（可信度 + 适用范围）

### 3.1 结论

> **TH12：`etCancel`(512) / `etCancel2`(532) / ★`etClearAll`(510)、三机体炸弹（非符卡时）、
> 飞碟开奖的 `UFO_EtBreak`/`UFO_EtBreak2`（扩圈 `etCancel`）、激光 —— 都会把扫过的每颗弹变成 1 个 `type 9`
> （PIV 计数器 `+10`、分数 `+1`），落地走过去收；**半径内的敌人也各吐 1 颗**（`0x414f40`）。
> `etClear`(513) / `etClear2`(533)、玩家中弹、擦弹 —— 一颗都不出。
> 全场不消耗 RNG；场地门外扩边距 = `2.0`（不是 `32.0`）。**
> ★ 本作是这一批里**唯一「连 `etClearAll` 都出货」**的（也因此是唯一「boss 击破会出货」的）。

### 3.2 可信度与适用范围

- **✅ 高**：入口/调用点逐条回指令边界；x87 常量逐个按字节读出；opcode↔handler 用跳转表机械还原并与 eclmap 对名；
  item 语义用 18 项跳转表 ＋ `0x427ca0`/`0x40e730` 的算术横向标定。
- **范围：仅 TH12 v1.00b（md5 `212f187591f4f1fa99eb2845d1b3cf20`），不可外推。**
  横向只成立一条：TH12 与 TH13 的 type 9 处理逐条同形（`0x426c24` vs `th13:0x42eb46`，都是 `0.1f`＋`+10`）⇒ 两作互证，**但都不能外推到 TH15+**（TH14/15 出 `type 0xa`、TH16 出季节道具）。

### 3.3 🟡 / ❓ 未钉死（照实搬，不许当结论用）

| # | 项 | 现状 | 怎么钉 |
| --- | --- | --- | --- |
| 1 | `0x40cac7` 那个「半径 ÷ `3.0`」的前置条件 | 🟡 只读到 `[0x4b43cc]+0x7c & 1` 且 `[+0x78] ∈ [0x60,0x63]`；语义未定 | 读 `[0x4b43cc]+0x78/+0x7c` 的写入点，对时间轴（符卡/超时） |
| 2 | type 9「值多少」的**显示单位** | 🟡 只钉到「同一个 PIV 计数器 `+10`」；与 HUD 的 PIV/最大得点没对账 | 拿实跑存档对一次 PIV 显示 |
| 3 | 判定形状的**全集** | 🟡 只逐条读了「圆（半径）」族（`0x40caa0`/`0x414f40`）＋矩形全清 `0x40cf60`；其余矩形/点族入口的门未逐条读 | 对每个矩形/点入口单独读门 |
| 4 | `0x436c2c call 0x40d230`（玩家 tick 里、走 `>= 0x1e` 那支） | 🟡 我复核调用点时顺手看到：**它不在素材列的 7 个入口里**，性质未定 | 读 `0x40d230` 全函数体，看它是否有 `push 0x9` |
| 5 | 擦弹不消弹 | 素材给的是**否定式**一手（type 9 的 20 个 spawn 点无一在擦弹判定里）；我没有逐点重扫判定链 | 从擦弹入口 `FUN_*` 反查是否有取消调用 |

## 4. 证据（地址 / 常量 / 复现命令）

- **样本**：`local/th12.v1.00b/th12.exe`，md5 **`212f187591f4f1fa99eb2845d1b3cf20`**（737280 字节）。
- **关键地址**：`0x40caa0`（弹幕侧入口，`ret 0xc`）、`0x40c8b0`（`Bullet__cancel`）、`0x40d560`（场地门，返回 0/1）、
  `0x414f40`（敌人链表撒道具，唯一调用点 `0x40cbc8`）、`0x40cf60`（矩形全清，开关 `[esp+0x5c]`）、`0x4286f0`/`0x428750`（激光）、
  `0x4273f0`（`spawn_item`）、`0x427ca0`（PIV）、`0x40e730`（分数）。
- **关键常量**（真值）：边距 `[0x4a4014] = 2.0f`、`[0x4a3d00] = 2.0`(double)；场地 `[0x4a3d60] = −192.0`、`[0x4a3d28] = 192.0`、
  `[0x4a3d58] = 0.0`、`[0x4a3d50] = 448.0`（全 double）；撒道具速度 `[0x4a41f4] = 0.6f`、角度 `[0x4a4060] = −1.5707964f`；
  type 9 值 `[0x4a4210] = 0.1f`、换算 `[0x4a3ce8] = 100.0`(double)；半径缩放 `[0x4a3fd8] = 3.0`(double)。
- **opcode 表**：`[0x4197f4] = 0x4187af`(510)、`[0x4197fc] = 0x419071`(512)、`[0x419800] = 0x4190aa`(513)、
  `[0x41984c] = 0x4190e3`(532)、`[0x419850] = 0x41911c`(533)；索引 = opcode − 510。

```bash
# 1) 全量反汇编（本机重跑，约 3 秒）
objdump -d -M intel --no-show-raw-insn local/th12.v1.00b/th12.exe > /tmp/can/th12.asm
# 2) 入口调用点穷举（「全集」就是这么来的）
grep -c "call   0x40caa0$" /tmp/can/th12.asm     # 9
grep -c "call   0x414f40$" /tmp/can/th12.asm     # 1（唯一调用点）
grep -c "call   0x40cbe0$" /tmp/can/th12.asm     # 0（死代码）
# 3) 单函数原文（本文贴的三段都在这里）
objdump -d -M intel --no-show-raw-insn --start-address=0x40cb58 --stop-address=0x40cbe0 local/th12.v1.00b/th12.exe
objdump -d -M intel --no-show-raw-insn --start-address=0x4187af --stop-address=0x4187c0 local/th12.v1.00b/th12.exe
# 4) 常量真值（按 PE 节表映射 VA→文件偏移，读原始字节）
#    python3：解析 PE 节表 → rd(VA,8) → struct.unpack('<d'/'<f')
# 5) ECL 侧对名与脚本原文（★ §1.6 的扩圈参数就是这么验的）
grep -n -E "^(510|512|513|532|533)[ ,]" local/vendor/eclmap/eclmap/th12.eclm
local/vendor/thtk/build/thecl/thecl -d 12 -m local/vendor/eclmap/eclmap/th12.eclm \
    local/th12.v1.00b/dat/default.ecl | sed -n '126,159p'   # UFO_EtBreak / UFO_EtBreak2
```

## 5. 对卡的直接影响（只读观察，不改代码）

- **我们那张卡**：`mods/th18.v1.00a/card-expand/native/cards/ufo.c`。现状（`grep -n "ce_cancel\|cancel" ufo.c`）：
  全文只在 `ufo.c:2250` 的自述里提到消弹 —— 「`15. #6 扩圈消弹（0x429250 vs 0x429370）—— 不选，一次都没调`」。
  **`ce_cancel_*` 调用次数 = 0**。
- **与原版对比：★ 不一致。** TH12 原版的**飞碟召唤**与**飞碟击破**各带一圈扩圈 `etCancel`
  （`UFO_EtBreak` 16→112 / `UFO_EtBreak2` 16→640，逐帧 `+6` / `+16`），扫过的每颗弹 → 1 个 type 9。
  我们这张卡既不召唤扩圈、也不在击破时消弹 ⇒ **「开奖不消弹」与原版行为不同**（不是实现方式差异，是缺了一条原版路径）。
- **若将来要补**：TH12 的 `etCancel` 语义在宿主里对应 **TH18 的 `mode 1`**（type 9 小分道具），
  即 `ce_cancel_radius(pos, r, max, 1)`；**不要用 `mode 4`**（那会混进原版没有的 type 2 金钱，
  见 [`../th18/01-cancel-to-item.md`](../th18/01-cancel-to-item.md) §2.2）。扩圈参数照原版：半径 16 起、每帧 `+6`（召唤）/ `+16`（击破）、到 112 / 640。
- **本文不改任何代码**，也不对卡该不该补下结论（那是设计取舍）；这里只登记「一手原版是什么 / 我们现状差在哪」。
