# TH17 鬼形兽 消弹 → 出不出实体道具

> **版本**：TH17 v1.00b（`local/th17.v1.00b/th17.exe`，imagebase `0x400000`，md5 `7a5b5bc1c231792098a28d8367928aaf`）。
> 本文裸地址（如 `` `0x419c30` ``）默认属该版本；别作写成 `th16:0x416a00`、`th18:0x4290e0`。**一手工具**：`objdump -d -M intel <exe>`，PE 常量与跳转表按节表映射只读直读。
> **可信度**：✅ 主体 —— 核心函数逐字节 + **全部**入口调用点回指令边界 + **ECL 派发表机械复原**（2026-09-15 复核）。
> 两条由父 agent 一手钉死的结论（`do_hyper_cancel` 实参语义、水獭三光球 `r = 28.0f` / `mode = 1`）按一手采信，见 §1.6 / §1.7。
> 未钉死项在 §2.6。**结论只适用 TH17 v1.00b**。相关：[`../th16/04-cancel-to-item.md`](../th16/04-cancel-to-item.md)（反面对照）·
> [`../th16/01-core-engine.md`](../th16/01-core-engine.md)（弹池/运动 VM，两作同构）· 宿主 [`../th18/01-cancel-to-item.md`](../th18/01-cancel-to-item.md)。

## 0. 一句话结论

**只有 `mode 1` 出货，撒的是 `item type 9`（PIV 小道具，每颗弹 1 个，PIV `+5` 并进分数）**；`mode 0` 连计数器都不动，
其余值（含 3 / 4）只 `inc` 计数器。**出的那几条**：ECL 615 / 635 / 712 / 545、613 的**激光半边**、**咆哮 hyper**（按键档）、水獭三光球。
**不出的那几条**：ECL 616 / 636 / 613 的**弹幕半边** / 518 `dialogRead`、`do_bomb`、**玩家三种炸弹（灵梦 / 魔理沙 / 妖梦，一律传 0）**、
玩家中弹与死亡、弹自毁、敌机死亡。

## 1. 发现（一手事实）

### 1.1 核心函数 `gen_items_from_et_cancel` `0x419c30`（`ecx` = pos，`edx` = mode，无栈参）

```asm
419c37  test edx,edx / je 0x419ce5      ; ★ mode 0 → 直接 return（连计数器都不动）
        ...场地门 4 条（§4.1），任一命中 → return...
419c91  mov  eax,ds:0x4b768c            ; BULLET_MANAGER_PTR
419c96  inc  DWORD PTR [eax+0x71b538]   ; ★ 计数器 ++：唯一「消了但没出」的痕迹
419c9c  cmp  edx,0x1 / jne 0x419ce5     ; ★★ 只有 mode 1 撒；其余值只 inc 后 return
419ca1  mov  ecx,0x4b7668 / call 0x402880     ; REPLAY_SAFE_RNG 取 float ∈ [−1,1)
419cab  fmul ds:0x4a3af8(=10°) → subss ds:0x4a3bf4(=π/2)  ; 角度 = rand×10° − 90°
419ccf  mov  DWORD PTR [esp+0x8],0x400ccccd   ; 速度 = 2.2f
419cde  push 0x9 / call 0x434740              ; ★ 撒 item type 9（每颗弹 1 个）
```

### 1.2 mode 真值表（一手，逐字节）

| mode | 撒什么 | 谁在用（一手调用点，§2.3） |
| :---: | --- | --- |
| 0 | ❌ 一颗不撒，**连计数器都不动**（`0x419c39` 直接 return） | ECL 616/636、613 弹幕半边、518、`do_bomb`、玩家 3 种炸弹、玩家中弹/死亡、弹自毁 4 处 |
| **1** | ✅ **`item type 9`（PIV 小道具）每颗弹 1 个** | **ECL 615/635/712、545**、613 **激光半边**、**咆哮 hyper**（按键档）、水獭三光球 |
| 2 / 3 / 4 | ❌ 只 `inc` 计数器 | 均无调用点（★ TH18 的 4 = 多数 `type 9` + 少数 `type 2`；TH16 的 4 = 季节道具） |

### 1.3 撒出来的是什么 —— `type 9` = PIV `+5`，并进分数

`ItemManager__on_tick__body` `0x433410` 在 `0x433b27` 用 `type-1` 查 14 项跳转表 **`0x4341c0`**：`type 9–14` 走
`0x433f09`，即 `float_tbl[0x4a0d6c + type*4] × 100.0f[0x4a3d30]` 加进 `[0x4b5a24]`（钳 `[0x4b5a2c]`），再折算写分数 `[0x4b59fc]`。

- **值**：`float_tbl[0x4a0d6c]` 的 index 9–14 = `5, 10, 20, 30, 40, 50`（本轮直读 `th17:0x4a0d90 = 5.0`、`th17:0x4a0da4 = 50.0`）
  ⇒ **`type 9` = PIV 计数器 `+5`**（`× 100` 后入账），并同时按 `[esp+0x28] = 10` 折算进分数。
- **跨作交叉**：TH16 的同一张表 `th16:0x4917bc` 在 index 9 上**也是 `5.0`**（`th16:0x4917e0`，本轮直读），代码同样是「`value × 100` 加进钳位累积量」⇒ 两条独立线索都指向 `type 9` = PIV `+5`。
- **是实体、要走过去收**：走 `ItemManager__spawn_item` `0x434740`（`ecx` = `ITEM_MANAGER_PTR` `0x4b76b8`，8 个栈参），不是当场入账；现场与原生写法逐字同形（junk 走未初始化区、`timer = 0`、`u3 = −1`）。**每颗弹 1 个**：mode 1 支只有一处 `push 0x9`（`0x419cde`），无计数器分流。

### 1.4 消弹入口全集（弹幕 5 个 + 激光管理器 4 个）

| # | 地址 | 名（ExpHP，非一手） | 形态 | 撒不撒 |
| :---: | --- | --- | --- | --- |
| 1 | `0x419ac0` | `Bullet__cancel` | `(ecx=弹, [ebp+8]=mode)`，`ret 4` | 由 mode 定 |
| 2 | `0x419cf0` | `BulletManager__cancel_radius` | `(pos, mode)` + `xmm2`=半径，`ret 8`；**不查保护位** | 由 mode 定 |
| 3 | `0x419df0` | `cancel_radius_as_bomb` | 同上 + 查 `[弹+0x24] == 0`（`0x419e51`） | 由 mode 定 |
| 4 | `0x419f00` | `cancel_rectangle_as_bomb` | `(pos, size, mode)` + `xmm3`=角度，`ret 0xc`；查 `[弹+0x24]`（`0x419f36`） | 由 mode 定 |
| 5 | `0x41a1e0` | `BulletManager__cancel_all` | `ret 4` 但**全函数不读栈参**；逐弹路径硬编码 `xor edx,edx`（`0x41a2b6`） | **永不撒** |
| 6 | `0x4359b0` | `LaserManager__cancel_in_rectangle` | → `laser->vtable[0x20](pos, pos2, &rad, 0, 1)` | 由 mode 定 |
| 7 | `0x435ab0` | `LaserManager__cancel_in_radius` | → `laser->vtable[0x24](pos, radius, mode, arg)` | 由 mode 定 |
| 8 | `0x435b30` | `LaserManager__cancel_all` | → `laser->vtable[0x28](arg0, 0)` | 由 arg0 定 |
| 9 | `0x435a40` | `LaserManager__ecl_545_impl` | → `laser->vtable[0x28](**1**, 0)` | **恒撒** |

激光类这三个方法有的**内联**同一份「场地门 + 计数器 `++` + `cmp mode,1` + 撒 `type 9`」，有的**直接 `call 0x419c30`**（`0x437e73` / `0x43a643` / `0x43f324` / `0x43fc7d`，mode 取栈参）。
vtable 槽真值（`.rdata` 逐 dword 直读）：`LaserLine` `th17:0x4a1944/48/4c` = `0x437b80` / `0x438be0` / `0x4393d0`；`LaserInfinite` `th17:0x4a18d8/dc/e0` = `0x43a350` / `0x43b290` / `0x43baa0`；
`LaserCurve` `th17:0x4a1800/04/08` = `0x43f140` / `0x43fb70` / `0x43ff40`；`LaserBeam__cancel` `0x440560` @ `th17:0x4a1874`。

### 1.5 「出的那几条 / 不出的那几条」（一手调用点）

**出（mode 1）**

| 路径 | 地址链（一手） |
| --- | --- |
| ECL 615 `etCancel` | handler `0x425b9d`：`0x425ba6 push 1` → `0x425bc6 call 0x419cf0`；激光 `0x425bcb`/`0x425bcd push 1,1` → `0x425be5 call 0x435ab0` |
| ECL 635 `etCancel2` | handler `0x425c96`：`0x425c9f push 1` → `0x425cbf call 0x419df0` |
| ECL 712 `etCancelRect` | handler `0x425bef`：`0x425c27 push 1` → `0x425c55 call 0x419f00`（同块内 `0x405b60`/`0x41da80` 是无栈参叶子，吃不掉这个 push） |
| ECL 545 `laserCancel` | handler `0x425ffe` → `0x426004 call 0x435a40` → `vtable[0x28](1, 0)` |
| ECL 613 的**激光半边** | handler `0x424ff5`：`0x42500a push 1` → `0x42500c call 0x435b30(1, 0)` |
| **咆哮 hyper 清弹** | `0x40f690 call 0x419df0(pos, mode)` + `0x40f6ab call 0x435ab0(pos, mode, 1)`，`mode = ~([0x4b5ac4]>>3) & 1` |
| 水獭三光球 | `0x40f291 push 1` → `0x40f2cc call 0x419df0`，`xmm2 = [0x4a3ce0] = 28.0f` |

**不出（mode 0 或内部硬编码 0 的全清入口）**

| 路径 | 地址链（一手） | mode |
| --- | --- | :---: |
| ECL 616 `etClear` / 636 `etClear2` | handler `0x425c5f`：`0x425c68 push 0` → `0x425c88 call 0x419cf0`（激光 `0x425c8f push 0`）；handler `0x425cc9`：`0x425cd2 push 0` → `0x425cf2 call 0x419df0` | 0 |
| ECL 613 的**弹幕半边** | handler `0x424ff5`：`0x424ffb push 0` → `0x424ffd call 0x41a1e0` | 0 |
| ECL 518 `dialogRead` | handler `0x42506e`：`0x42508b call 0x41a1e0`；激光 `0x42509a call 0x435b30(0, 0)` | 0 |
| 通用 `do_bomb` | `0x411cff call 0x419cf0(player_pos, 0)`，半径 `[0x4a3d50] = 192.0f`；门控 `[pl+0x74] == 0x64` | 0 |
| 玩家三种炸弹 | 灵梦 `0x412dc0`/`0x413778`/`0x4139cd`/`0x413aaf` → `0x419df0(·,0)`；魔理沙 `0x412614` → `0x419f00(·,0)`；妖梦 `0x4143db` → `0x419f00(·,0)` | 0 |
| 玩家中弹 / 死亡 | `0x447a66 call 0x419cf0`（`0x447a64 push edx`，`edx` 是刚退出的循环计数 = **0**，半径 `[0x4a3d38] = 128.0f`）、`0x447a7e call 0x419df0(·, 0)`（半径 `[0x4a3db4] = 640.0f`） | 0 |
| 弹自毁（`Bullet__run_ex`） | `0x416ec2`、`0x4172e3`、`0x417d66`、`0x418009` 四处 `push 0` | 0 |

★ **TH17 的玩家三种炸弹清弹一律传 0 ⇒ 一颗道具都不出**；出道具的是 ECL 的 `etCancel` 族、激光、咆哮 hyper。

### 1.6 咆哮（hyper）清弹与 `do_hyper_cancel` 实参语义（✅ 已由父 agent 一手钉死）

`GoastManager__sub_40ee70_beast_related` `0x40f690` 的 mode 是**取反**写法：
`0x40f681 mov eax,[0x4b5ac4]` / `0x40f686 shr eax,3` / `0x40f689 not eax` / `0x40f68b and eax,1`
⇒ `mode = ~([0x4b5ac4] >> 3) & 1`，然后 `0x40f68e push eax` / `0x40f68f push esi` / `0x40f690 call 0x419df0`。

- `do_hyper_cancel` `0x40f6d0` 把实参写进 `[0x4b5ac4]` bit3，标准「替换 bit3」写法在 **`0x40f82a`–`0x40f847`**：
  `mov ecx,[ebx+0x8]` / `shl ecx,3` / `xor ecx,[0x4b5ac4]` / `and ecx,8` / `xor ecx,[0x4b5ac4]` / `or ecx,4` /
  `mov [0x4b5ac4],ecx` ⇒ **bit3 = 实参**、bit2 = 霊撃进行中。
- 两个调用点（门指令逐条核过）：
  - `0x40f385 push 0x0`（门 = `0x40f37c test BYTE PTR ds:0x4b3448,0x2`；`0x4b3448` = **当前输入状态**，由 `ReplayManager__on_tick_11__record_replay` 从 `0x4b3230` 装入 ⇒ bit1 = **霊撃键**）⇒ bit3 = 0 ⇒ **mode = 1（撒）**；
  - `0x447bfe push 0x1`（门 = `0x447bf0 test al,0x2` 且 `0x447bf4 test al,0x4` / `jne` 跳过；紧跟 `0x447c07 call 0x4495e0` = `Player__cancel_impending_death`）⇒ **中弹致死取消（deathbomb）** ⇒ bit3 = 1 ⇒ **mode = 0（不撒）**。
- ⇒ **TH17 的霊撃「按键发动 = 撒、中弹取消 = 不撒」**，与 `~bit3 & 1` 的取反正好对上 ✅。

### 1.7 水獭三光球：`r = 28.0f`、`mode = 1`（✅ 已由父 agent 一手钉死）

`0x40f289 movss xmm2,ds:0x4a3ce0`（**本轮直读 PE = `28.0f`**）；`0x40f291 push 0x1`（这一 push 到调用点之间
没有任何 `call`，不会被吃掉）；`0x40f2c5 push eax` / `0x40f2cc call 0x419df0` ⇒ `cancel_radius_as_bomb(pos, 1)`。
⇒ **水獭三光球 = 半径 `28.0f`（`th17:0x4a3ce0`）+ `mode 1`（撒 `type 9`）**，入口是 as_bomb 型（查 `[弹+0x24]`）。

### 1.8 boss 击破 / 符卡结束 / 关卡切换属哪一档

- **符卡打掉 / 捕获**（`$TIMEOUT == 0`）→ `etCancel(640.0f)` ⇒ mode 1 ⇒ **✅ 撒**（例 `st01bs.ecl` 的 `Boss2`）。
- **符卡超时** → `etClear(640.0f)` ⇒ mode 0 ⇒ ❌；符卡结束处另有的 `laserCancel()`（545）⇒ 激光 mode 1 ⇒ ✅。
- **boss 击破（终盘）** → `enmKillAll(); etClearAll(); spellEnd(); @BossItem(...)` ⇒ 613 弹幕半边 `0x41a1e0`（内部硬编码 0）
  ⇒ **弹幕一颗不出**；但**同一处的激光半边传 1** ⇒ 激光每段各出 1 个（§2.4）。
- **敌机死亡 / `enmKillAll`**：清弹入口的调用点全集里没有敌机管理器函数 ⇒ **引擎侧不清弹**。
- **关卡切换 / 对话开始**：`GuiMsgVm__initialize` 内 `0x42d5f8 call 0x41a1e0`（§2.5 订正 1）⇒ **清弹、mode 0 不出货**。

### 1.9 消耗 RNG

- ✅ **`mode 1` 消耗 replay-safe RNG**：每颗弹一次 `[0x4b7668]` 上的 `0x402880`（`0x4027b0` 取 u32 → `× 2^-31`
  （`0x4a3a8c`）→ `− 1.0f`（`0x4a3b9c`）= `[−1,1)` 的 float），再 `× 10°` 得角度。
- ❌ **`mode 0` 不消耗**：`0x419c39` 在取 rand 之前就 `return`，也**不 `inc` 计数器** ⇒ 只有真正出货的路径推 RNG 流。

## 2. 验证（怎么排除掉别的路）

### 2.1 ECL opcode ↔ handler：机械复原（✅ 不靠猜）

派发 = `opcode − 300` 查 **byte 表 `0x4268d0`**（值 = 指针表索引）→ 查 **指针表 `0x42661c`**：

| op | 名 | byte 索引 | handler | op | 名 | byte 索引 | handler |
| :---: | --- | :---: | --- | :---: | --- | :---: | --- |
| 613 | `etClearAll` | 124 | `0x424ff5` | 636 | `etClear2` | 147 | `0x425cc9` |
| 615 | `etCancel` | 126 | `0x425b9d` | 712 | `etCancelRect` | 165 | `0x425bef` |
| 616 | `etClear` | 127 | `0x425c5f` | 545 | `laserCancel` | 86 | `0x425ffe` |
| 635 | `etCancel2` | 146 | `0x425c96` | 518 | `dialogRead` | 63 | `0x42506e` |

★ 同一套编号在 **TH16 上逐项相同**（byte 表 `th16:0x422d70` / 指针表 `th16:0x422ab8`；615 → `th16:0x42219e`、613 → `th16:0x421655`、712 → `th16:0x4221d4`、545 → `th16:0x422550`）⇒ 两作的消弹 opcode 集合一致，**只有分派不同**。

### 2.2 mode 闭包 = {0, 1}（✅ 本轮复核）

`0x419ac0` 的 7 个调用点（4 处弹自毁 `push 0` + 3 处入口内部透传）、`0x419cf0` 的 4 个、`0x419df0` 的 9 个、
`0x419f00` 的 3 个、`0x41a1e0` 的 3 个（栈参被忽略）、`0x435ab0` 的 4 个、`0x435b30` 的 3 个 —— 实参立即数**全是 0 / 1**；
`0x419c30` `gen_items` 的 6 个调用点 = 2 个弹幕侧（`0x419b69` 透传 `[ebp+8]`、`0x41a2be` 硬编码 `xor edx,edx`）
+ 4 个激光方法（`0x437e73` / `0x43a643` / `0x43f324` / `0x43fc7d`，mode 取栈参 `[ebp+0x14]` / `[ebp+0x10]`）。
⇒ **没有任何路径传 2/3/4**，`mode 2` / `3` / `4` 在 TH17 里是死支（另有若干激光方法把同一段逻辑**内联**，同上不传 2/3/4）。

### 2.3 调用点穷举（全集）+ 间接调用证否

| 目标 | 调用点数 | 目标 | 调用点数 |
| --- | :---: | --- | :---: |
| `0x419ac0` `Bullet__cancel` | 7 | `0x419c30` `gen_items` | 6 |
| `0x419cf0` 半径 | 4 | `0x434740` `spawn_item` | 17 |
| `0x419df0` 半径 as_bomb | 9 | `0x4359b0` 激光矩形 | 1 |
| `0x419f00` 矩形 as_bomb | 3 | `0x435ab0` 激光半径 | 4 |
| `0x41a1e0` `cancel_all` | 3 | `0x435b30` 激光全清 / `0x435a40` 545 | 3 / 1 |

**间接调用证否**（METHOD.md 第 4 条）：把这些入口地址当 32 位 dword 在整个 exe 文件里搜 ⇒ 没有函数指针表 / vtable 槽形式的间接调用（vtable 槽只出现在 §1.4 已逐 dword 读出的激光类三个方法里，且它们也被同一张 mode 表管辖）。所有直接调用点都回指令边界复读了实参。

### 2.4 激光侧：另一套入口，但**语义与弹共用同一张 mode 表**

545 `laserCancel` → `0x435a40` → `vtable[0x28]`（`LaserLine__cancel` `0x4393d0`）**传 1** ⇒ 出；613 的激光半边
→ `0x435b30(1, 0)` ⇒ 出；518 的激光半边 → `0x435b30(0, 0)` ⇒ 不出；玩家死亡路径的激光 `0x447ac0` / `0x447ad8` 传 0 ⇒ 不出，
但**玩家侧还有一处传 1 的**：`0x447eee call 0x435b30(1, ·)`（门 = `0x447edb mov eax,[edi+0x628]` 后 `eax == 0xf` 且 `!= 4`）
⇒ **激光那条会撒 `type 9`**（`player+0x628` 这个状态的语义本轮没回溯 → 🟡）。

- **「每条 1 个」还是「每段 1 个」**：✅ **每段 1 个** —— `LaserLine__cancel` 的循环推进 `0x439750 addss xmm5,[0x4a3cc8]`（**本轮直读 `th17:0x4a3cc8 = 16.0`**，步长 16px），循环条件比 `[edi+0x70]`（激光长度）与 `xmm5 + 8.0`（**`th17:0x4a3ca8 = 8.0`**，本轮直读）⇒ 一条长度 `L` 的激光最多撒 ≈ `L/16` 颗（计数器也按段 `+1`）。**循环体本身本轮未逐条重读**（只核了常量）。

### 2.5 本轮对既有记录的订正（结论不变，机制 / 地址更硬）

1. **对话开始会清弹**：既有记录标 🟡「TH17 的对话开始不清弹」。本轮读到 `GuiMsgVm__initialize` 内 `0x42d5df`–`0x42d5f8` 与 TH16 的 `th16:0x429f67`–`th16:0x429f80` **逐条同形**，以 `0x42d5f8 call 0x41a1e0` 收尾 ⇒ 应订正为「**清弹、但 mode 0 不出货**」。
2. **`cancel_all` 的「三个调用点全传 0」不是机制**：`0x41a1e0`（`ret 4`）**全函数不读 `[ebp+0x8]`**，mode 在逐弹路径硬编码（`0x41a2b6 xor edx,edx` → `0x41a2be call 0x419c30`）⇒ 应写成「内部硬编码 0，**谁调都不出货**」。
3. **灵梦炸弹的清弹调用点是 4 处**（`0x412dc0`、`0x413778`、`0x4139cd`、`0x413aaf`），既有记录列了 3 处；四处都 `push 0`。
4. **地址一律取指令起始**：实参替换段是 `0x40f82a`–`0x40f847`（`0x40f810`–`0x40f825` 是紧邻的另一段 `push 1; push [esi]; call 0x476510` 三次循环，不是替换段）；`push 0x0` 那条在 `0x40f385`（`0x40f386` 是它的第 2 个字节）；`push 0x1` 那条在 `0x447bfe`（`0x447c00` 是紧随的 `call`）。

### 2.6 照实保留的未钉死项

- 🟡 **「PIV +5」的显示单位**：`[0x4b5a24]` 的 `+5` 是计数器步长，与 HUD 的 PIV 显示没做换算（TH12/13 的 `+10`、TH14/15 的 `+200` 同类问题）。**怎么钉**：跟 HUD 的 PIV / 最大得点对一次账。
- 🟡 **判定形状全集**：本页逐条读的是「圆（半径）」族的门与矩形入口的**保护位**门；矩形 / 点族的**几何语义**没读。**怎么钉**：读 `0x419f00` 全函数（`0x419f00`–`0x41a1db`）的 OBB 段。
- ❓ **`cancel_all` 的逐弹「弹型回调」**：`0x41a1e0` 除清弹外还调 `0x4a0f68[type]`（`0x41a223`，`edx = 1`，`0x41a235 call eax`）。这张表**没有逐条读** ⇒ 若某个弹型的回调自己撒道具，「613 弹幕半边一颗不出」就存在例外。**怎么钉**：取 `th17:0x4a0f68` 的每一项，逐个看有没有 `call 0x419c30` / `call 0x434740`。
- ❓ **样本**：同目录另有 `th17.exe.steamstub`，不是本文样本（md5 不同），本文只对 `th17.exe` 负责。

## 3. 结论（可信度 + 适用范围）

> **TH17 的「消弹 → 实体」= 所有传 `mode 1` 的路径：ECL 615 / 635 / 712、545 `laserCancel`、613 的激光半边、
> 咆哮（hyper）清弹的按键档、水獭三光球。撒的是 `item type 9`（PIV `+5`），每颗弹 1 个，掉在地上要走过去收。
> 玩家三种炸弹、ECL 616/636、613 的弹幕半边、518 `dialogRead`、玩家中弹与死亡、弹自毁 —— 一颗都不出。**

- **可信度：✅ 高**（核心函数逐字节；5 个弹幕入口 + 4 个激光管理器入口 + 4 个激光类方法的调用点全集逐条回指令边界；opcode↔handler 用两张表**机械还原**；item 语义用跳转表 `0x4341c0` + TH16 同表同值两条交叉）。
- **范围：仅 TH17 v1.00b**（md5 `7a5b5bc1c231792098a28d8367928aaf`），**不可外推**。
- ★ **与 TH16 的对照**：`etCancel`(615) 的 mode 在 TH16 是 **1 → 什么都不撒**、在 TH17 是 **1 → 撒 `type 9`**；玩家炸弹 TH16 是 **4/5 → 撒季节道具**、TH17 是 **0 → 不撒**；613 的激光半边 TH16 不撒、TH17 撒 ⇒ 就算 mode 号碰巧相同，**每作必须回它自己的 `0x41…` 里读分派**。

## 4. 证据（地址 / 常量 / 复现命令）

### 4.1 场地门常量真值（核心函数头部，任一条命中即 `return`，**连计数器都不加**）

| 判据 | 常量地址 | 真值 | 源码指令 |
| --- | --- | --- | --- |
| `x + 32 ≤ −192` | `0x4a3ce8` / `0x4a3e90` | `32.0f` / `−192.0f` | `0x419c43` / `0x419c5a` |
| `x − 32 ≥ 192`（即 `x ≥ 224`） | `0x4a3d50` | `192.0f` | `0x419c67` |
| `y + 32 ≤ 0`（即 `y ≤ −32`） | （`xorps` 0.0f） | `0.0f` | `0x419c7f` |
| `y − 32 ≥ 448`（即 `y ≥ 480`） | `0x4a3d9c` | `448.0f` | `0x419c88` |

与 TH16（`th16:0x4945fc` 一族）、TH18 的门**逐条同形**（外扩 32、`−192` / `192` / `0` / `448`）。

### 4.2 其它一手常量（本轮直读 PE）

水獭清弹半径 `0x4a3ce0 = 28.0f`；角度抖动 `0x4a3af8 = 0.17453292f`（10°）、基准 `0x4a3bf4 = 1.5707964f`（π/2）；
`type 9` 的 PIV 系数 `0x4a0d6c + 9*4 = 0x4a0d90 = 5.0f`（`× 100` 入账）；清弹半径常量 `0x4a3d38` / `0x4a3d18` / `0x4a3db4`
= `128.0f` / `64.0f` / `640.0f`；激光逐段步长 `0x4a3cc8 = 16.0f`、段尾余量 `0x4a3ca8 = 8.0f`。

### 4.3 复现命令（只读，不改仓库）

```bash
cd /home/nipporita/renkolab
md5sum local/th17.v1.00b/th17.exe      # 7a5b5bc1c231792098a28d8367928aaf
objdump -d -M intel local/th17.v1.00b/th17.exe > /tmp/can/th17.asm      # 180489 行
objdump -d -M intel --start-address=0x419c30 --stop-address=0x419cea local/th17.v1.00b/th17.exe
grep -cE 'call[[:space:]]+0x41a1e0$' /tmp/can/th17.asm                  # 3（全清入口调用点数）
# ECL 派发表：byte 表 VA 0x4268d0[idx = op-300] -> 指针表 VA 0x42661c[该值]
#   （按节表把 VA 映射到文件偏移后读字节 / 双字；§2.1 的八条 opcode 就是这么复原的）
# 逐条实参：对每个调用点用 objdump --start-address 回读前面若干条指令的 push 立即数
```

一手脚本侧：`thecl -d 17 -m local/vendor/eclmap/eclmap/th17.eclm local/th17.v1.00b/dat/st01bs.ecl`（符卡结束 `etCancel(640.0f)` vs 超时 `etClear(640.0f)`；每个 `st0Xbs.ecl` 恰好 1 处 `etClearAll`（终盘））。

## 5. 对卡的直接影响（只读观察，不改代码）

现状来自 `mods/th18.v1.00a/card-expand/native/cards/animal.c`（`grep -n "ce_cancel\|cancel" animal.c`，2026-09-15 实读）。**本节只做只读登记，未改任何代码。**

1. **水獭三光球 —— ✅ 与一手一致（复刻）**：`orb_cancel` 在 `animal.c:2878` 调 `ce_cancel_radius(pos, ANIMAL_ORB_CANCEL_RADIUS, 9999, ANIMAL_ORB_CANCEL_MODE)`，常量在 `animal.c:2761`/`2762` = `28.0f` / `1`。一手即 §1.7（`0x40f291 push 1` → `0x40f2cc call 0x419df0`、`xmm2 = th17:0x4a3ce0 = 28.0f`）⇒ **同值同 mode ⇒ 复刻** ✅。宿主 `ce_cancel_radius` 落到 `th18:0x429370`（as_bomb 型、查 `[弹+0x24]`），与原版的 `0x419df0`（同样查 `[弹+0x24]`，`0x419e51`）**保护位语义也对得上** ✅。
2. **`hyper_frame` 的 `by_hit ? 0 : 1` —— ✅ 与一手完全对上（复刻）**：`animal.c:2980` 调 `ce_cancel_radius(pp, r, 9999, s->core.by_hit ? 0 : 1)` ⇒ 中弹取消（`by_hit = 1`）→ mode 0 不撒、按键发动（`by_hit = 0`）→ mode 1 撒 —— 正是 §1.6 的 `~bit3 & 1`（`0x40f385 push 0` = 按键 ⇒ 撒；`0x447bfe push 1` = deathbomb ⇒ 不撒）⇒ **一一对应** ✅。
3. **已删除的鹫束消弹 —— ✅ 有支持**：TH17 消弹入口的调用点全集里**没有**任何鹫束 / 鹰专属函数（owner 见 §2.3、§2.5），与卡在 `animal.c:2491` 注释里登记的「鹫激光不消弹」一致。
4. **与一手一致、无需改**：TH17 原版**玩家三种炸弹一律传 0（不出货）**，我们卡不模拟玩家炸弹的消弹 ⇒ 不冲突。
5. **若将来要补「TH17 的 ECL 消弹档」**：宿主 TH18 的 `mode 1`（`th18:0x4290e0`）与 TH17 的 `mode 1` **号与义都对**（都是 `type 9`）⇒ 两作唯一可以直接沿用的重合档；但 TH17 的 **`mode 4` = 什么都不撒**，不要照 TH18 的 `mode 4`（`type 9` + `type 2`）去映射。
