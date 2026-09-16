# TH15 绀珠传 消弹 → 出不出实体道具

> **版本**：TH15 v1.00b（`local/th15.v1.00b/th15.exe`，md5 `bddd41a586e09e4447f795a14009d68a`）。本文裸地址默认属该版本；
> 引用别作一律写 `th18:0x…`。跨作对照见 [`../th18/01-cancel-to-item.md`](../th18/01-cancel-to-item.md)。
> 一手工具：`objdump -d -M intel`（238302 行 dump）+ `thecl -d 15` 反编译 `dat/*.ecl`；只用来取名（**非一手**）：`th15-funcs.json`。
> 复现命令与常量真值集中列在 §4。

## 1. 发现（一手事实）

### 1.1 核心：`0x41e4b0`（368 字节，`ecx` = `float3* pos`，`edx` = `mode`，无栈参）

```
41e4b7  test edx,edx
41e4b9  je   0x41e61b          ; ★ mode 0 → 直接到函数尾（连计数器都不动）
41e4bf  movss xmm2,[esi]       ; x        ← 下面四道场地门，任一出界即跳 0x41e61b
41e4c3  movss xmm3,ds:0x4cffb0 ; 32.0f
41e51d  mov  eax,ds:0x4e9a6c   ; BULLET_MANAGER_PTR
41e522  inc  DWORD PTR [eax+0x141f0c8]  ; 计数器 ++
41e528  mov  eax,[eax+0x141f0c8]        ; eax = n（mode 2 支要用）
41e52e  cmp  edx,0x1 / jne 0x41e54a    ; mode 1：1 颗
41e54a  cmp  edx,0x3 / jne 0x41e59e    ; mode 3：2 颗
41e59e  cmp  edx,0x2 / jne 0x41e61b    ; mode 2：力量道具 + 小道具
41e61b  ret
```

### 1.2 mode 真值表（逐字节）

| mode | 行为 | 一手出处 |
| --- | --- | --- |
| 0 | 直接 return：**不撒、也不 `inc` 计数器** | `0x41e4b7` / `0x41e4b9 je 0x41e61b` |
| 1 | 每颗弹 **1 个 `type 0xa`**（各自取一次 rand） | `0x41e52e`；`push 0xa; call 0x4408d0` |
| 3 | 每颗弹 **2 个 `type 0xa`**（两处 `push 0xa`：`0x41e585`、`0x41e616`） | `0x41e54a`；**无调用点（死支）** |
| 2 | 每 5 颗补 1 个 **`type 1`（力量道具）** + 1 个 `type 0xa`，两者都受符卡位门控 | `0x41e59e`；**无调用点（死支）** |
| 4 / 5 | **没有专门分支**（落进只 `inc` 的尾部），**无调用点** | 落 `0x41e61b` |

- mode 2 的门 = `([SPELLCARD 0x4e9a70]+0x78 & 1) == 0`；力量道具那支角度是 `0xbfc90fdb`（= `−π/2`）、速度 2.2，小道具那支角度 = `rand×10°−90°`。
- mode 3「一颗弹出 2 个」是本轮查过的四作里**唯一**一个一弹出多颗的 mode（但**没有调用点**）。
- ★ **本作没有「mode 4 = 掉道中的东西」这回事**：4/5 空转。对比 TH16 的 4 = 季节道具、TH18 的 4 = `type 9` 多数 + `type 2` 少数。

### 1.3 撒出来的是什么（一手）

- 走 `ItemManager::spawn_item` `0x4408d0`（8 个栈参 `(type, pos, junk, angle, speed, timer, u2, u3)`，`ret 0x18`）⇒ **实体、走过去收**，不是当场入账。
- `ItemManager__on_tick__body` `0x43f830` 在 `0x43feba` 用 `type−1` 查 **15 项**跳转表 `0x440194`：

| type | 语义 | 处理 |
| --- | --- | --- |
| 1 | 力量道具 | `0x43fee3` → `0x4401d0` |
| 2 | 点道具 | `0x43ff07` → `0x4405c0` |
| **9 / 0xa** | **同一条 handler**（消弹撒的就是 0xa） | **都是 `0x43ff4b`** → `0x43b200` + `0x420f50` |
| 0xb | 同族、值 ×10 | `0x43ff5c`（`[0x4cff98]` = 20.0f） |
| 0xc | 空道具 | `0x43ff94` |
| 0xd / 0xe / 0xf | 计数类小道具 | `0x43ff6d` / `0x43ff78` / `0x43ff83`：`0x440eb0` 实参 **5 / 10 / 25**，随后 `0x420f50` 实参 **10 / 50 / 100**（⇒ 计数器 +1 / +5 / +10） |

- ★ **跳转表里 `type 9` 与 `type 0xa` 指向同一条 handler `0x43ff4b`**（15 项表逐项读过）⇒ 消弹撒的 `type 0xa` 与 `type 9` **完全同价**；它与 `type 2`（点道具 `0x4405c0`）**不是**一条。
- 收 1 个 `type 0xa` 改**两个**计数器（一手逐字节，与 TH14 的 `0x439d30` + `0x41d5e0` **逐条同形**）：

| 计数器 | 加多少 | 单位 / 钳位 | 一手 |
| --- | --- | --- | --- |
| `[0x4e7434]` | `(int)(2.0f × 100.0f)` = **+200** | ×100 定点（按 1/100 读 ⇒ 显示 **+2**）；钳 `[0x4e743c]` | `0x43b200`：`mulss xmm1,ds:0x4cffe8`(=100.0f) → `cvttss2si eax` → `add` |
| `[0x4e740c]` | `10/10` = **+1** | 钳 `0x3b9aca00`（= 999,999,999） | `push 0xa; call 0x420f50`：魔术数 `0x66666667` + `sar edx,2` ⇒ **÷10** |

- ⇒ 研究里「PIV `+2`」与「`+200`」**不是冲突**：`+200` 是 `[0x4e7434]` 的原始加值（×100 定点 ⇒ 显示 +2），`+1` 是 `[0x4e740c]` 的原始加值。**两个计数器、两条都对。**
- 🟡 两个计数器的**名字没有一手钉死**：只钉到「`[0x4e740c]` 钳 999,999,999（ZUN 常见分数上限形状）且出现在 `0x43d90d mul [0x4e740c]` / `0x43b3e5 imul [0x4e7434]` 这类**结算**路径里」⇒ 本文只写「计数器（疑为分数 / 最大得点系）」。
- **type 0xa 属「消弹专属池」**：`0x4408d0` 开头把 `type ∈ {9,0xa,0xb,0xd}` 分流到另一支（`0x4408e7`–`0x440905` 四条 `cmp/je`），走 `[0x4e9a9c]+0xe5dee4`（上限检查 `cmp eax,0x400` 在 `0x440a73`），与普通池 `+0xe5ded4` 分开；type 存进 `item+0xc68`（`0x4409fa`）—— **这条同时印证 dispatch 用的字段就是 item type**。

### 1.4 消弹入口全集（弹幕 6 个 + LaserManager 3 个）

| # | 地址 | 形态 | mode 从哪来 |
| --- | --- | --- | --- |
| 1 | `0x41e250` | `Bullet::cancel`（`ecx`=弹，`[ebp+8]`=mode，`ret 4`） | 调用方栈参（`0x41e3bc mov edx,[ebp+8]` → `0x41e3c5 call gen_items`） |
| 2 | `0x41e650` | `cancel_radius(pos, mode)` `ret 8` + `xmm2` 半径 | 调用方栈参 |
| 3 | `0x41e750` | 同上 + 查 `[弹+0x24]==0`（`0x41e7b1`，as_bomb 变体） | 调用方栈参 |
| 4 | `0x41e850` | `cancel_rectangle(pos, size, mode)` + `xmm3` 角度 | 调用方栈参（`0x41ed88 call gen_items`） |
| 5 | `0x41ebf0` | `cancel_all`（2000 槽循环） | **硬编码 0**（`0x41ed80 xor edx,edx`）⇒ 永不撒 |
| 6 | `0x431720` | vtable 方法（槽在 `0x4caf60`，`ecx` 带 `+0x44/+0x48` pos、`+0x29c` 半径²） | **硬编码 0**（`0x4317a4 push 0x0`）⇒ 永不撒 |
| 7 | `0x441db0` | `LaserManager__cancel_in_radius` `(pos, mode, arg)` `ret 0xc` | 调用方栈参 → `laser->vtable[0x24](pos, radius, mode, arg)` |
| 8 | `0x441e30` | `LaserManager__cancel_all` | 调用方两个开关（`vtable[0x28](arg0, arg1)`） |
| 9 | `0x441d20` | ECL 545 专用实现 | 遍历激光表清状态位后 `vtable[0x28]` ⇒ mode 1 |

- 两族都跳过 `[laser+0x10]==1` 的槽。`0x41e850` 内部还有两处直调：`0x41ebc9 call Bullet::cancel(mode=[ebp+0x10])`、`0x41ed88 call gen_items`。
- **激光类实现里直调 `gen_items` 共 8 处**：`0x443cb3`（mode=`[ebp+0x14]`）、`0x444bec`（`[ebp+0x10]`）、`0x4453fc`（`[ebp+0x8]`）、`0x4460f3`（`[ebp+0x14]`）、`0x446f1c`（`[ebp+0x10]`）、`0x447815`（`[ebp+0x8]`）、`0x44a52f`（`[ebp+0x14]`）、`0x44af88`（`[ebp+0x10]`）⇒ **激光侧走同一个核心、同一张 mode 表**，只是入口在 LaserManager。

### 1.5 逐条路径：ECL op ↔ handler ↔ mode

| op | 名 | handler / 调用点 | 入口 | mode | 出实体？ |
| --- | --- | --- | --- | --- | --- |
| 615 | `etCancel` | `0x42d389` | `0x41e650(pos,1)` + 激光 `0x441db0(pos,1,1)` | 1 | **✅ 每颗弹 1 个 `type 0xa`** |
| 616 | `etClear` | `0x42d3fe` | `0x41e650(pos,0)` + 激光 `0x441db0(pos,0,1)` | 0 | ❌ |
| 613 | `etClearAll` | `0x42c818` | `0x41ebf0()` + `0x441e30(1,0)` | 弹 0 / 激光 1 | 弹 ❌ / **激光 ✅** |
| 635 | `etCancel2` | `0x42d430` | `0x41e750(pos,1)` + `0x441db0(pos,1,1)` | 1 | **✅** |
| 636 | `etClear2` | `0x42d462` | `0x41e750(pos,0)` | 0 | ❌ |
| 712 | `etCancelRect` | `0x42d3bb`（`0x42d3e4 push 0x1`） | `0x41e850(pos,size,1)` | 1 | **✅** |
| 545 | `laserCancel` | `0x42d6f4` → `0x441d20` | `laser->vtable[0x28]` | 1 | **✅（只对激光）** |

- opcode ↔ handler 是**机械还原**的：派发在 `0x429283`（byte 表 `0x42de78` → 指针表 `0x42dbcc`，索引 `opcode−300`），不靠猜。
- **玩家炸弹 —— 一手读全：4 机体 / 4 个 `Bomb*` 类，名字与调用点都对得上**：

| 类 | 弹幕侧调用点 | 入口 | 激光侧 |
| --- | --- | --- | --- |
| `BombReimu` | `0x416c77`（method_10）+ 子过程 `0x416404` / `0x4164e9` | `0x41e750`（`push 0x1`） | `0x416416` `(pos, 1, 1)` |
| `BombMarisa` | `0x415521`（method_10） | **`0x41e850`（唯一用矩形的炸弹）** | — |
| `BombReisen` | `0x417a1a` / `0x417bce`（on_tick） | `0x41e750`（mode = `~符卡位&1`） | `0x417a35` / `0x417bec` `(pos, ~符卡位&1, 1)` |
| `BombSanae` | `0x41878f`（method_10） | `0x41e750` | — |

- ★ 两条订正：研究把 `0x416c77` / `0x41878f` 记成 `BombReimu__on_tick` / `BombSanae__on_tick`，实读都在各自的 **`method_10`** 里；研究把炸弹的激光半边写成 `0x441db0(pos, 1, ·)`，实读第 2 参是 **`~符卡位&1`**（`0x417a2e not eax / and eax,0x1`）⇒ 「符卡中 ⇒ 一颗都不出」**弹幕与激光两边都成立**。
- 两个无名字函数 `0x4163d0` / `0x4164a0`（被 `BombReimu__on_tick` `0x4168a4` / `0x416ae5` 与 `BombReimu__method_10` `0x416d07` 调用）是 **Reimu 炸弹的私有子过程**，它们的三处 cancel 调用（`0x416404` / `0x4164e9` / `0x416416`）都传 1。
- **玩家中弹（补研究没列的一条）**：`Player__on_tick` 有 5 处调取消入口 —— `0x454b02` → `0x41e750(·, **0**)`（弹幕，不出）、`0x454b44` / `0x454b5b` → `0x441db0`（`(pos, 0, 1)` / `(pos, 0, 0)` ⇒ 激光 mode 0，不出）、`0x454b0b` / `0x454de4` → `0x441e30`。⚠ `0x454b0b` 压的是 `(0, 1)`、而 ECL 613 是 `(1, 0)` ⇒ **哪个开关是「撒」未判定** ⇒ 🟡 见 §3。
- **❌ 不出的**：弹自毁（`0x41b68a` / `0x41c4dc`，在 `Bullet::run_ex` `0x41b180` 内 `push 0x0; call Bullet::cancel`）、`0x41ebf0` / `0x431720`（两个硬编码 0 的入口）、613 的弹幕半边、616 / 636、**玩家中弹的弹幕与激光半边（mode 0）**。

### 1.6 场地门、RNG、擦弹、章节

- **场地门**（`0x41e4b0` 内联，在 `inc` 之前）：`x+32 ≤ −192`、`x−32 ≥ 192`、`y+32 ≤ 0`、`y−32 ≥ 448` ⇒ **不撒、也不 `inc`**。
- **RNG**：✅ 消耗。mode 1/2/3 每撒一颗调一次 `[0x4e9a48]` 上的 `0x403840`（`finit` → `0x4036a0` 取 u32 → `×[0x4cfd8c]`（= 2⁻³¹）→ `−[0x4cfe98]`（= 1.0f）⇒ `[−1,1)` 的 float），再算角度。mode 0 在 rand 之前 return；4/5 只 `inc`、不取 rand。
- **擦弹**：一手 **❌ 不消弹**（只有减速，另见 [`../../player/th15/01-graze-and-item-slowdown.md`](../../player/th15/01-graze-and-item-slowdown.md)）。`0x41e250` 的 6 个调用点里**没有任何擦弹判定**；擦弹相关的 `type 0xd` 出现在 `0x419de6`（`Bullet::sub_419a50` 内），那是**特定弹型**的行为，不是「擦到就消」。
- **章节结算**：一手 **❌ 不消弹也不撒道具** —— `FUN_0043ab70`（章节 UI/anm 结算，读 `ENEMIES_SPAWNED_IN_CHAPTER`）**不在** `Bullet::cancel` / `cancel_*` / `cancel_all` / `gen_items` / `spawn_item` 的任何调用点里；章节号本身由 ECL `setChapter(N)` 设（`st01bs.ecl` 里紧跟在清弹之后）。
- **对话开始 / 关卡切换**：★ 一手正面结论 —— `GuiMsgVm::initialize`（`0x437520`）在 `0x437b8e` 调 `0x41ebf0()`（弹幕全清、硬编码 mode 0）、`0x437b93 push 0x0` / `0x437b95 push 0x0 → call 0x441e30`（激光全清、两个开关都传 0）⇒ **清弹，但一颗道具都不出**。
- **敌机死亡 / `enmKillAll`**：清弹入口的调用点全集里没有敌机管理器函数 ⇒ 引擎侧不清弹。

### 1.7 boss 击破 / 符卡结束 / 关卡切换属哪一档（脚本原文）

`thecl -d 15 -m local/vendor/eclmap/eclmap/th15.eclm` 反编译 `dat/*.ecl`（一手脚本）：

```text
unless ($TIMEOUT == 0) goto Boss2_200;
etCancel(640.0f);        ← 打掉 / 捕获 ⇒ mode 1 ⇒ ✅ 出
goto Boss2_220;
Boss2_200:
etClear(640.0f);         ← 超时 ⇒ mode 0 ⇒ ❌ 不出
Boss2_220:
setChapter(44);          ← ★ 章节号就在清弹之后由 ECL 设
```

- **boss 击破（终盘）**：`enmKillAll(); etClearAll(); spellEnd();` ⇒ 弹幕 `cancel_all`（mode 0）⇒ **❌**；每个 `st0Xbs.ecl` 恰好 **1 处** `etClearAll`，而 `etCancel` 有 7–39 处。
- **符卡结束处另有 `laserCancel()`（op 545）** ⇒ mode 1 ⇒ **每条激光出 1 个**。

## 2. 验证（怎么排除掉别的路）

1. **opcode ↔ handler 机械还原**（`0x429283` + 两张表 + `opcode−300`），不是按名字对号入座。
2. **item 语义三处互证**：跳转表 `0x440194`（type−1 索引，**15 项逐项读过：type 9 与 0xa 都是 `0x43ff4b`**）↔ `spawn_item` 的 `[item+0xc68]` 写回（`0x4409fa`）↔ `type ∈ {9,0xa,0xb,0xd}` 走专属池（`0x4408e7`–`0x440905`）。三处都指向「`0xa` = 消弹专属的那种道具」。
3. **两个计数器按指令逐步读**：`0x43b200` 的 `mulss xmm1,[0x4cffe8]`（= 100.0f，已直读复核）⇒ `+200`；`0x420f50` 的除法魔术数 `0x66666667` + `sar edx,2` **代入 `n = 10 / 50 / 100` 逐步复算得 `1 / 5 / 10` ⇒ 是 ÷10**（不是 ÷5）⇒ `push 0xa` 的结果是 `+1`。
4. **炸弹名单与调用点对齐**：`th15-funcs.json` 里 `Bomb*` 共 4 个机体类（`BombMarisa` / `BombReimu` / `BombReisen` / `BombSanae`），线性扫描把每个 `on_tick` / `method_10` 里的 cancel 调用点都归到类上（§1.5 表）⇒ 「4 机体」与名单**对得上**；两个无名字函数（`0x4163d0` / `0x4164a0`）靠**调用者**（都在 `BombReimu` 内）定性，不是靠猜名字。
5. **压栈约定先钉死再读实参**：用 ECL 615/616 这对已知实参（`(pos, 1, 1)` / `(pos, 0, 1)`）反推「**最后一个 `push` = 第 1 个参数**」，再据此读炸弹与玩家中弹那几处的 mode（否则 `push 0/1` 的序列会读反）。
6. **激光 vtable 三组已钉死**（ExpHP 名对照）：`0x4cc97c`–`0x4cc98c` = **LaserLine**（`0x4439a0` / `0x444a50` / `0x445140`）、`0x4cc910`–`0x4cc920` = **LaserInfinite**（`0x445de0` / `0x446d70` / `0x4474f0`）、`0x4cc83c`–`0x4cc844` = **LaserCurve**（`0x44a330` / `0x44ae80`）；三组的 `+0x20` / `+0x24` / `+0x28` 正是 LaserManager 的矩形 / 半径 / 全清入口（`+0x20` 取 `[ebp+0x14]`、`+0x24` 取 `[ebp+0x10]`、`+0x28` 取 `[ebp+0x8]`），8 个 `gen_items` 调用点分别落在这 9 个方法里。
7. **否定结论的排法**：「擦弹不消弹」「章节不消弹」「对话清弹但不出货」「敌机死亡不清弹」四条都是**调用点全集**上的排除，逐处回指令边界读实参，不是「没找到所以没有」；`0x431720` 则反过来证否：**全 exe 一处直接 `call` 都没有**（与「它是 vtable 方法」一致）。
8. **「消了 n 颗」≠「出了 m 个」**：出界弹过不了门（门在 `inc` 之前）⇒ 贴着场地边消掉的弹零产出、连计数器都不加。

## 3. 结论（可信度 + 适用范围）

> **TH15：ECL 615 `etCancel` / 635 `etCancel2` / 712 `etCancelRect` / 545 `laserCancel`、613 `etClearAll` 的激光半边、
> 以及四种机体炸弹（4 个 `Bomb*` 类，非符卡时）—— 传 mode 1，撒 `item type 0xa`（计数器 `[0x4e7434] += 200`、`[0x4e740c] += 1`），每颗弹 1 个，落地走过去收。
> 超时 `etClear` 族、613 的弹幕半边、弹自毁、玩家中弹、`0x41ebf0` / `0x431720` —— 一颗都不出。
> 擦弹与章节（`setChapter`）本身既不消弹也不撒道具；对话开始清弹但一颗不出。**

**可信度：✅ 高。** 依据：核心函数逐字节；全 `.text` 线性扫 `call rel32` 穷举 6 个弹幕入口 + 3 个 LaserManager 入口 + 8 个激光类实现调用点（按 `th15-funcs.json` 归函数，**名字非一手、只用来定位**），逐处回指令边界读实参；跳转表 `0x440194` 逐项读过；opcode↔handler 机械还原；item 语义三处互证。**范围：仅 TH15 v1.00b（md5 `bddd41a586e09e4447f795a14009d68a`），不可外推。**

**🟡 / ❓ 未钉死项（照实保留）**

| # | 项 | 现状 | 怎么钉 |
| --- | --- | --- | --- |
| 1 | mode 2 / 3 是**死支** | ✅ 已钉「一个调用点都没有」；但**行为仍逐字节读过**（2 = 力量道具 + 小道具、3 = 每颗 2 个） | 无（保留原结论，别把它当活路径用） |
| 2 | 两个计数器的**名字** | 🟡 只钉到「`[0x4e7434]` 是 ×100 定点（钳 `[0x4e743c]`）、`[0x4e740c]` 钳 999,999,999 且在结算路径被引用」；**名字（分数 / 最大得点）没有一手** | 跟 HUD 的得点 / 最大得点显示对一次账 |
| 3 | `Player__on_tick` `0x454b0b` → `0x441e30(0, 1)` 的两个开关 | 🟡 ECL 613 那条是 `(1, 0)`，这条是 `(0, 1)` ⇒ **哪个开关是「撒」未判定**（同函数另两处 `0x41e750` / `0x441db0` 已读实为 mode 0 ⇒ 不出） | 读 `0x441e30` → `vtable[0x28]` 三个实现（LaserLine/Infinite/Curve）各自的第 1 参用法 |
| 4 | 矩形 / 点族入口自己的门 | 🟡 只逐条读了「圆（半径）」族的门；`0x41e850`（矩形）的门未单独读 | 单读 `0x41e850` 的判定段 |
| 5 | 擦弹相关的 `type 0xd` 属哪个弹型 | ❓ 本轮不查（低优先）：已知 `0x419de6` 在 `Bullet::sub_419a50` 内，被 `0x41950d` / `0x4196f6` / `0x41a0dc` 调用 | 从**弹型的 run 处理表**入手（`0x4cb4c8` 是取消回调表，别再用它找） |
| 6 | 激光是「每条 1 个」还是「每段 1 个」 | 🟡 本作未核（TH17 已钉为**每段 1 个**，步长 16px） | 读本作 `LaserLine__cancel` 的循环体是否有同样的步进 |

## 4. 证据（地址 / 常量 / 复现命令）

| 项 | 真值 | 地址 / 位置 |
| --- | --- | --- |
| 场地边距 | `32.0f` | `0x4cffb0` |
| 左界 | `−192.0f` | `0x4d0138` |
| 右界 | `192.0f` | `0x4d0014` |
| 下界 | `448.0f` | `0x4d006c` |
| y 上界 | `0.0f`（内联立即数） | `0x41e4ff` |
| 速度实参 | `0x400ccccd` = `2.2f` | 指令立即数 |
| 力量道具角度 | `0xbfc90fdb`（= `−π/2`） | mode 2 支 |
| 计数器系数 | `2.0f` | `0x4cff10` |
| ×100 定点系数 | `100.0f` | `0x4cffe8` |
| 0xb 的系数 | `20.0f` | `0x4cff98` |
| 消弹计数器 | `[BULLET_MGR+0x141f0c8]` | `inc` 在 `0x41e522` |
| 计数器 A（疑为最大得点系） | `[0x4e7434]`（钳 `[0x4e743c]`）：`+= 200` | `0x43b200` |
| 计数器 B（疑为分数） | `[0x4e740c]`（钳 999,999,999）：`+= 1`（`n/10`） | `0x420f50` |
| RNG | `[0x4e9a48]` → `0x403840`；系数 `0x4cfd8c` = 2⁻³¹、`0x4cfe98` = 1.0f | — |
| 道具池 | 专属 `[0x4e9a9c]+0xe5dee4`（上限 `0x400`）/ 普通 `+0xe5ded4` | `0x4408e7`–`0x440a73` |

> 上表的 float 真值由 PE 节表映射回文件偏移**直读 u32 复核**（不是照抄）：`0x4cfd8c` = `0x30000000`（2⁻³¹）、`0x4cfe98` = `0x3f800000`（1.0f）、
> `0x4cff10` = `0x40000000`（2.0f）、`0x4cff98` = `0x41a00000`（20.0f）；四道门依次 `0x42000000` / `0xc3400000` / `0x43400000` / `0x43e00000`（与 TH14 逐条同形）。
> mode 2 力量道具的角度位型 `0xbfc90fdb` = **−π/2**（TH14 那支是 `0x3fc90fdb` = +π/2 再 `subss`，两作实得同向）。

```bash
cd local/th15.v1.00b
objdump -d -M intel th15.exe > /tmp/th15.asm
# 核心 + mode / 场地门（§1.1–1.3）
objdump -d -M intel --start-address=0x41e4b0 --stop-address=0x41e650 th15.exe
# 六个弹幕入口（§1.4）
for a in 0x41e250 0x41e650 0x41e750 0x41e850 0x41ebf0 0x431720; do
  objdump -d -M intel --start-address=$a --stop-address=$((a+0x300)) th15.exe; done
# 三个 LaserManager 入口（§1.4 的 7–9）
for a in 0x441d20 0x441db0 0x441e30; do
  objdump -d -M intel --start-address=$a --stop-address=$((a+0x110)) th15.exe; done
# 调用点穷举（每个命中回指令边界看实参）
grep -nE "call +(0x41e250|0x41e650|0x41e750|0x41e850|0x41ebf0|0x431720|0x41e4b0)" /tmp/th15.asm
# ECL 脚本侧档位（§1.7）
local/vendor/thtk/build/thecl/thecl -d 15 -m local/vendor/eclmap/eclmap/th15.eclm dat/st01bs.ecl \
  | grep -nE "etCancel|etClear|laserCancel|setChapter|enmKillAll"
# 对话开始清弹（§1.6）
objdump -d -M intel --start-address=0x437b80 --stop-address=0x437ba0 th15.exe
```

## 5. 对卡的直接影响（只读观察，不改代码）

- 现状（`grep -n "ce_cancel\|cancel" mods/th18.v1.00a/card-expand/native/cards/lolk.c`）：**0 命中** —— 115（`LOLK`）**一次都没调过任何消弹入口**。
- 一手判定：**✅ 与一手一致**。115 只做两件事（`lolk.c` 文件头）：**擦弹减速**（`ITEM_MANAGER+0xe6bb14` 写 0.3，`LOLK_SLOW_FACTOR_OFF`）+ **章节波次**（原生 `SPELLCARD+0x78` bit0 的两个沿结算波次）。
  而一手是：**TH15 的擦弹不消弹**（§1.6）、**章节结算不消弹也不撒道具**（§1.6）⇒ 卡不消弹**不是漏做**。
- ⚠ 唯一需要留意的是「炸弹半边」：TH15 的炸弹**是**会出道具的（mode = `~符卡位&1`），但那属于「玩家放 B」这条链，**与 115 这张被动卡无关** —— 115 里没有炸弹路径。
- 与 114（`collect2x.c`）并列看：两张系统卡都**不消弹**，且都是**与各自那一作的一手一致**（TH14 的收点批次、TH15 的章节/擦弹，原版都不消弹）。
- 本文**只读**，未改任何 `.c` / `.h` / spec，未 `git add`。
