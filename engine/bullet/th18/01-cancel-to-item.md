# TH18 消弹 → 撒出实体道具 —— 这条链被谁走到

> **版本**：TH18 v1.00a（`th18.exe`，imagebase `0x400000`，md5 `9969cac756098c1da05a81de45437a70`）。本文裸地址默认属该版本；引用其他版本写成 `th16:0x…`。
> **方法**：一手 objdump `-d -M intel` 直读（关键段逐字节复核）。调用点穷举 = 全 `.text` 线性扫 `call rel32`；间接调用证否 = 把 8 个函数入口地址当 32 位 dword 在整个 exe 文件里搜（**0 命中**）。
> **辅助（非一手，只用来取名）**：ExpHP `th18-funcs.json`、`th-re-data` 的 `labels/statics`、thtk 的 `th18.eclm` op 名表。
> **可信度**：✅ 主体（每条给地址 + 指令原文，§9 给了可重跑的命令）· 未钉死 / 待验 8 项集中列在 §8，另 3 处 🟡 就地标在 §3 / §6。
> **相关**：卡侧调用点 [`../../card/th18/04-active-cards.md`](../../card/th18/04-active-cards.md) · `0x446f40` 的 9 个调用点归类 [`../../ecl/th18/02-item-drops.md`](../../ecl/th18/02-item-drops.md) §6 · 中途波次 [`../../chapter/th18/03-midstage-waves.md`](../../chapter/th18/03-midstage-waves.md)

## 0. 一句话结论

**出不出实体道具，只看 `mode` 一个数**：`0` / `3` = 纯删除（一颗实体都没有；`0` 连计数器都不动，`3` 只把计数器 `++`），`1` = 撒 **type 9**（小分道具），
`4` = 撒 **type 9 多数 + type 2 少数**（≈37% 钱，§2.2）。所以「打掉 / 捕获符卡」走的 ECL `etCancel`(615) 出点，
「超时清屏」`etClear`(616) 与 **boss 击破的 `etClearAll`(613) 子弹侧**一颗都不出（§5）。
我们自己的卡里只有 **109 灵击爆风 / 117 三光球·霊撃 / 116 季节点 / 66 审判的激光侧**真的撒出过实体（§4.1）。

## 1. 弹幕侧：五个消弹入口（不是四个）

| # | 入口 | 本仓 / ExpHP 名 | 签名 | mode 从哪来 | 查 `+0x24`？ | 计数器 |
| --- | --- | --- | --- | --- | :---: | --- |
| 1 | `0x428e90` | `Bullet__cancel` | `thiscall(bullet; mode)`，`ret 4` | 调用方栈参 | ❌ | ❌ |
| 2 | `0x429250` | `BulletManager__cancel_radius` | `(pos*, mode)` + `xmm2` 半径，`ret 8` | 调用方栈参 | **❌** | `[mgr+0x7a41e8]` |
| 3 | `0x429370` | `cancel_radius_as_bomb` | `(pos*, mode, max_count, tag)` + `xmm2` 半径，`ret 0x10` | 调用方栈参 | ✅ | `[mgr+0x7a41e8]` |
| 4 | `0x4294b0` | `cancel_rect_as_bomb`（本文命名） | `(pos*, size*, mode, tag)` + `xmm3` 角度，`ret 0x10` | 调用方栈参 | ✅ | `[mgr+0x7a41e8]` |
| 5 | `0x4297a0` | `BulletManager__cancel_all` | `thiscall(mgr; 一个从不读的栈参)`，`ret 4` | **硬编码 0**（`0x429932`） | ❌ | ❌ |

- 「计数器」= `inc DWORD PTR [mgr+0x7a41e8]`，与 mode 无关：**消掉就 +1**（`0x429300` / `0x42942d` / `0x429768`）。
- 2–4 都只认 `state ∈ {1,2}`（`+0xf68`）：`0x4292a7`、`0x4293ba`、`0x4294da`；**#1 不看 state**（它只写 `+0xf68`，`0x42904a`——调用方负责筛）。`state` 语义：`0` 空槽 / `1,2` 活动中 / `3` 擦弹消去 / `4` 取消收尾。
- 5 全函数体 `0x4297a0`–`0x429a0e`（`ret 4`）没有 `+0xf68` 门 —— 它自己内联走完「每弹型回调（`0x4297f0 mov edx,1` / `0x4297f5 call eax`）+ 转特效（`0x407420` / `0x488350` / `0x489320`）」，然后把 mode 0 交给 `0x4290e0`（`0x42993a`）。

### 1.1 清单是全集（穷举 + 证否）

线性扫全 `.text` 的 `call rel32`，命中数：`0x428e90` × **9**、`0x429250` × **3**、`0x429370` × **20**、
`0x4294b0` × **4**、`0x4297a0` × **3**、`0x4290e0` × **10**、`0x449010` × **11**、`0x449090` × **3**、`0x448fa0` × **1**、`0x446f40` × **9**。
每个命中都回到指令边界复核（objdump 反出的就是该地址处的 `e8` / `ff` 本身）。

**间接调用证否**：把这 8 个函数入口地址当 32 位 dword 在整个 exe 文件（`.text` / `.rdata` / `.data`）里搜，
**每个地址都是 0 命中** ⇒ 不存在存着这些地址的函数指针表 / vtable 槽 / 回调登记，
**所有调用都是直接 `call`**，上面的清单就是全集（[`METHOD.md` 第 4 条](../../../METHOD.md)：字节穷举只当候选发现器 —— 这里它的用途是**证否**）。

### 1.2 #4 `0x4294b0` 是「只有四个入口」漏掉的那个

函数体 `0x4294b0`–`0x42979f`：开头 `mov edi,[0x4cf2bc]`（`0x4294b8`）、`mov [ebp-0x18],0x7d0`（`0x4294c6`，2000 槽循环）、
`lea esi,[edi+0xec]`（`0x4294cd`，槽 0）、`+0x24` 保护位在 `0x4294e8`、收尾 `0x42979d ret 0x10`。
`push [ebp+0x10]`（mode，`0x429773`）→ `0x42977c call 0x428e90`，`[esi+0x678] = [ebp+0x14]`（tag，`0x429776`）。

调用点**四个**：`0x40ba0b`（原生卡 19 OKINA_OP）、`0x40f4cb`（原生卡 46 VAMPIRE）、`0x420eb2`（`BombMarisa__method_called_by_on_tick`）、`0x435a6a`（**ECL op 712**）。

### 1.3 #2 不查 `+0x24`（订正）

`0x429250` 全函数体（`0x429250`–`0x429368`）只读 `state`（`+0xf68`）、`radius`（`+0x658`）、`pos`（`+0x638` / `+0x63c`）。
`cmp DWORD PTR [esi+0x24],0x0` 只出现在 `0x4293c8`（#3）与 `0x4294e8`（#4）。
⇒ 「保护单弹位能挡 `0x429250`」是**错的**：能挡的只有 #3 / #4 两个「as_bomb」型入口。

### 1.4 #5 永不撒（这是 `ce_cancel_all_bullets()` 的子弹侧）

`0x429932 xor edx,edx` → `0x42993a call 0x4290e0` ⇒ mode 0 ⇒ 连计数器都不动，**一颗实体都不撒**。
全屏清弹的「出点」印象全部来自它的**激光侧**（§3、§4.1 的 66）。

## 2. `Bullet__cancel` `0x428e90` 与撒道具核心 `gen_items_from_et_cancel` `0x4290e0`

### 2.1 mode 真值表（一手，逐字节）

| mode | 撒什么 | 角度 / 速度 | 证据 |
| --- | --- | --- | --- |
| 0 | **什么都不撒**（连计数器都不动） | — | `0x4290ea test edx,edx` / `0x4290ec je 0x429248` |
| 1 | **type 9**（小分道具） | `rand×0.1745329(=10°) − 1.5707964(=π/2)`，速度 `0x400ccccd` = 2.2 | `0x429168`–`0x4291a7`（`push 0x9` 在 `0x4291a5`） |
| **3** | **什么都不撒**，只 `[mgr+0x7a41d0]++` | — | `0x4291b2 cmp edx,4` / `0x4291b5 jne 0x429248`（`0x42915d` 的 inc 已发生） |
| **4** | **多数 type 9，少数 type 2**（≈37%） | type 2 支：角度 `0xbfc90fdb` = −π/2，速度 2.2；type 9 支同 mode 1 | `0x4291b2`–`0x429243` |

`0x428e90` 本体（`ecx` = 弹指针，1 个栈参 = mode，`ret 4`）：每弹型取消回调（`mov eax,[edi+0x608]` → `[[eax*4+0x4b4308]]`，`edx=1`）
→ `[edi+0x4bc]=1`（`0x428ecb`）→ 停 VM（`0x428ed5 call 0x478580`）→ **`test DWORD PTR [edi+0x20],0x200`（`0x428ee7`）**：
置位就整段跳过「转特效 + 撒道具」，直接写收尾（`0x429015 mov WORD PTR [edi+0xf68],4`）。

### 2.2 mode 4 的分布（静态分支推导）

`n = [mgr+0x7a41d0]`（`0x4291e0` 重读，注意 `0x42915d` 已经先 `++` 过），对所有分支 `idiv 5`：

| `n % 5` | 分流 | 结果 |
| --- | --- | --- |
| 0 | `0x4291c5 je 0x429222` | **type 2** |
| 2 | `0x4291d1` 抽 int，`al&1==0` → type 2 | type 2 或 type 9 |
| 4 | `0x4291f3` 抽 int，`%3==0` → type 2 | type 2 或 type 9 |
| 1 / 3 | 直接落进 `0x429205` | **type 9** |

⇒ P(type 2) ≈ 1/5 + (1/5)(1/2) + (1/5)(1/3) ≈ **0.367**，其余 type 9。

### 2.3 场地门（撒之前先过，圈外一颗都不撒）

`x ± 32.0f`（`0x4b92b4`）与 `−192.0f`（`0x4b946c`）/ `+192.0f`（`0x4b933c`）；`y` 与 `0.0f` / `448.0f`（`0x4b939c`）。
四处 `jae 0x429248` 直接返回（`0x42910d` / `0x429121` / `0x429139` / `0x42914a`）⇒ `x ≤ −224`、`x ≥ +224`、`y ≤ −32`、`y ≥ +480` 不撒、**也不加计数器**。
⇒ 「消了 n 颗弹」与「撒了 m 颗道具」不相等：贴着场地边消掉的弹是**零产出**的。

### 2.4 RNG 消耗

rand 全部取自 `REPLAY_SAFE_RNG` `0x4cf288`：mode 1 每次 1 次 **float** 抽取（`0x402810`）；
mode 4 是 **0–1 次 int（`0x402740`）+ 0–1 次 float**（`n%5==0` 与「直接落 type 9 的 1/3 支」不抽 int）。
⇒ 我们那几张传 1 / 4 的卡（109 / 117 / 116）**每帧都在推 replay-safe RNG 流**（原生 `etCancel` 同理；desync 影响见 §6 第 6 条）。

### 2.5 落地函数 `ItemManager__spawn_item` `0x446f40`

`ecx = ITEM_MANAGER_PTR` `0x4cf2ec`，8 个栈参 `(type, pos, junk, angle, speed, timer, u2, u3)`，`ret 0x20`；`type >= 0x14` 直接返回 0（`0x446f4a`）。
`0x4290e0` 的两处调用（`0x4291a7` / `0x429243`）**与原生敌机主掉落同形**（`junk` 走 `sub esp,8` 的未初始化区、`timer = 0`）
⇒ 这两条路撒的是**同一个原生道具**，不是特制道具。全 exe `0x446f40` 共 9 个调用点，逐处归类见
[`../../ecl/th18/02-item-drops.md`](../../ecl/th18/02-item-drops.md) §6（消弹链只占其中 2 处）。

## 3. 激光是另一套（3 个入口 + 1 处内联）

| 地址 | 形态 | 派发 | 撒不撒 |
| --- | --- | --- | --- |
| `0x449010` | `LaserManager__cancel_in_radius` `(pos*, A, B)` + `xmm2` 半径，`ret 0xc` | 遍历激光表 → `laser->vtable[0x24]` = `LaserX__cancel_as_bomb_circle(pos, 半径, A, B)` | **由 A 决定** |
| `0x449090` | `(mode, unused)`，`ret 8` | `laser->vtable[0x28]` = `LaserX__cancel(mode, ·)` | **由 mode 决定** |
| `0x448fa0` | 无参 | 同上，但**硬编码** `push 0` / `push 1`（`0x448ff0`） | **永远 mode 1 ⇒ 永远撒** |
| `0x43e2f4`–`0x43e30f` | **内联写法**，在 `GuiMsgVm__constructor` 里（不是独立函数） | 直接遍历 `LASER_MANAGER+0x14`，跳过 `[laser+0x10]==1` 的槽，`push 0` / `push 0` / `0x43e30f call [eax+0x28]`（eax = 激光指针） | **硬编码 mode 0 ⇒ 不撒** |

- A 是调用方的第 2 个栈参：`0x449010` 把 `[ebp+0x10]` / `[ebp+0xc]` / 半径 / `[ebp+8]` 依次压给 vtable+0x24，
  故被调方的 `[ebp+0x10]` = A（`0x44be7f mov edx,[ebp+0x10]` 就是取它当 mode）。🟡 只核了 Line 一族，见 §8。
- `0x4290e0` 的 10 个直接调用点里 8 个就是 `Laser{Line,Infinite,Curve}__cancel{,_as_bomb_circle,_as_bomb_rectangle}` 这一族：
  `0x44af8e` / `0x44be94` / `0x44c702` / `0x44d3de` / `0x44e204` / `0x44eb39` / `0x45196c` / `0x4522c8`；另两个是 `0x429010` 与 `0x42993a`。
- **激光侧把 A / mode 交给同一个 `0x4290e0`** ⇒ §2.1 的真值表对激光同样适用（传 3 也一样只加计数器）；
  本节列出的原生激光路实际传的只有 0（卡 23/35/43/44/47/49/53、玩家中弹）、1（ECL 615、613 激光侧、545、玩家死亡 `0x45c3cc`）与 **4**（卡 42 KOZUCHI：`0x40f01f push 4`）。

## 4. 逐个来源：谁真的撒出实体

### 4.1 我们自己的卡（`native/cards/*.c`，出处 = 源码行 + 上表反汇编）

| 卡 | 调用点 | 入口 | mode | 撒实体？ |
| --- | --- | --- | --- | --- |
| 110 风神录 | `cards/mof.c:972` | `0x429370` | 0 | ❌ |
| 111 花映冢（信）| `cards/signal.c:585` | `0x429370` | 0 | ❌ |
| 107 千枪 | `cards/senro.c:527` / `569` | `0x429370` | 0（`senro.c:69`） | ❌ |
| 109 灵击爆风 | `cards/pofv.c:202` | **直调 `0x428e90` 逐弹** | 1（`pofv_core.h:168`） | ✅ type 9 |
| 109 CA 红圈 | `cards/pofv.c:239` | `0x429370` | 0（`pofv_core.h:169`） | ❌ |
| 117 三光球 | `cards/animal.c:2878` | `0x429370` | 1（`animal.c:2762`） | ✅ type 9 |
| 117 霊撃 | `cards/animal.c:2980` | `0x429370` | `by_hit ? 0 : 1` | 命中那支 ❌、另一支 ✅ |
| 116 季节点 | `cards/season.c:1119` | `0x429370` | 4 | ✅ 9 多数 / 2 少数 |
| 66 审判 | `cards/judgment.c:62` | `0x4297a0` + `0x449090(1,0)` | 弹 0 / 激光 1 | 弹 ❌、**激光 ✅** |
| 112 / 113 / 114 | 不消弹（`ufo.c` / `trance.c` / `collect2x.c` 零调用） | — | — | — |
| 67 青眼 / 72 炎魔 | `cards/blue_eyes.c:99` / `cards/firelord.c:150/193/203` | `0x429370` | 0 | ❌（别人的卡，登记备查） |

SDK 两个包装：`ce_cancel_radius(pos, r, max_count, mode)`（`sdk.h:144`）= `0x429370`，**mode 没有默认值**，由调用方给；
`ce_cancel_all_bullets()`（`sdk.h:129`）= `0x4297a0`（子弹 mode 0）+ `0x449090(1, 0)`（激光 mode 1）—— **子弹不撒、激光撒**，见 §7。

### 4.2 TH18 原生清弹卡（`Card<角色>__on_tick(_2)`，名字取自 `th18-funcs.json`）

| id | 内部名 | 调用点 | 入口 | mode | 撒实体 | 激光侧 |
| --- | --- | --- | --- | --- | :---: | --- |
| 43 | KANAME 要石 | `0x40eb2f` | `0x429370` | 0 | ❌ | `0x449010` A=0 ⇒ ❌ |
| 44 | MOON | `0x40df8f` | `0x429370` | 0 | ❌ | A=0 ⇒ ❌ |
| 45 | MIKOFLASH | `0x40e4c1` | 直调 `0x428e90` | 0 | ❌ | 无 |
| 46 | VAMPIRE | `0x40f4cb` | `0x4294b0` | 0 | ❌ | — |
| 47 | SUN | `0x40f9ff` | `0x429370` | 0 | ❌ | A=0 ⇒ ❌ |
| 49 | BASSDRUM | `0x410145` | `0x429370` | 0 | ❌ | A=0 ⇒ ❌ |
| 50 | PSYCO | `0x41063f` | 直调 `0x428e90` | 0 | ❌ | 无 |
| 53 | RICEBALL | `0x410b9f` | `0x429370` | 0 | ❌ | A=0 ⇒ ❌ |
| **42** | **KOZUCHI 小槌** | `0x40f00b` | `0x429370` | **4** | **✅** | `0x449010` A=4 ⇒ **✅** |
| 23 | AUTOBOMB | `0x40a5ec` | `0x429370` | 0 | ❌ | A=0 ⇒ ❌ |
| 35 | ROKUMON | `0x40db0c` | `0x429370` | 0 | ❌ | A=0 ⇒ ❌ |
| 30 | OPTION_BR | `0x40d0a7` | `0x429370` | 0 | ❌ | 无 |
| 20 | NUE_OP | `0x40bca3` | `0x429370` | 0 | ❌ | 无 |
| 51 | MAGATAMA | `0x40bf83` / `0x40bfd3` | `0x429370` | 0 | ❌ | 无 |
| 19 | OKINA_OP | `0x40ba0b` | `0x4294b0` | 0 | ❌ | 无 |
| 52 | CYLINDER | 走原生 BOMB（下 §4.4） | `0x429370` 等 | 0 / 3 | ❌ | — |
| 33 | YUYUKO | **不走任何入口**（`0x4249be` 直接写 `state = 3`） | — | — | ❌ | — |
| 34 | MONEY | 不消弹：`0x40d713` 自己撒 type 2 | — | — | ✅（非消弹链） | — |
| 31 | DEAD_SPELL | 不消弹：`0x40d30b` 撒 type 7 ×3 | — | — | ✅（非消弹链） | — |

⇒ **原生清弹卡里唯一出点的是 42 KOZUCHI（小槌，mode 4）**；其余一律 mode 0，纯删除。

### 4.3 ECL（op 号 ↔ 调用点 ↔ mode）

| op | 名（thtk / ExpHP） | 调用点 | 入口 | mode | 撒实体？ |
| --- | --- | --- | --- | --- | :---: |
| 615 | `etCancel` | `0x4359d9` | `0x429250` | 1（`0x4359b9 push 1`） | ✅ type 9 |
| 615 激光侧 | 同上 | `0x4359f8` | `0x449010` | A=1 / B=1 | ✅（只对激光） |
| 616 | `etClear` | `0x435a9d` | `0x429250` | 0 | ❌ |
| 613 | `etClearAll` | `0x434d4e` | `0x4297a0` | 0 | ❌ |
| 613 激光侧 | 同上 | `0x434d5d` | `0x449090` | 1（`0x434d5b push 1`） | ✅（只对激光） |
| 635 | `etCancel2` | `0x435adb` | `0x429370` | 1 | ✅ type 9 |
| 636 | `etClear2` | `0x435b15` | `0x429370` | 0 | ❌ |
| 712 | `etCancelRect` / `712__hitboxRect` | `0x435a6a` | `0x4294b0` | 1 | ✅ type 9 |
| 545 | `laserCancel` | `0x435e27` | `0x448fa0` | 1（硬编码） | ✅（只对激光） |
| 518 | `dialogRead` | `0x434e07` + `0x434e16` | `0x4297a0` / `0x449090` | 0 / 0 | ❌ / ❌ |
| 523 | `spellEnd` | `0x435045` | `0x446f40` | type 4（非消弹） | ✅ 1 颗命碎片 |

op 712 的名字两套冲突（ExpHP 标签 `712__hitboxRect` vs thtk `etCancelRect`）；从原生卡 19 OKINA_OP `0x40ba0b` 看该 op 的实义是
「矩形消弹 + 矩形伤害源」两步 —— 两套名都留着，不擅自定名（§8 未钉死 #6）。

### 4.4 引擎侧（对话 / 中弹 / 弹自毁 / 炸弹 / 敌机死亡）

| 来源 | 调用点 | 入口 | mode | 撒实体？ |
| --- | --- | --- | --- | :---: |
| 对话开始（`GuiMsgVm__constructor`） | `0x43e2ea` | `0x4297a0` | 0 | ❌ |
| 同上（**内联**激光清屏） | `0x43e2ef`–`0x43e30f` | 激光 `vtable+0x28` | 0 | ❌ |
| 玩家中弹 / 死亡（子弹） | `0x45bf66` / `0x45bf85` | `0x429250` / `0x429370` | 0（`0x45bf19 mov edx,0x21` 的拷贝循环把 `edx` 减到 0） | ❌ |
| 同上（激光） | `0x45bfc7` / `0x45bfdf` | `0x449010` | A=0 | ❌ |
| 同上（激光，另一支） | `0x45c3cc` | `0x449090` | **1** | ✅（只对激光） |
| 弹自身自毁（寿命 / 出界 / 微码） | `0x4263df` / `0x4268c1` / `0x42728b` / `0x427545` | `0x428e90` | 0 | ❌ |
| 炸弹池 / 自机炸弹 | `0x4212e7` / `0x422109` / `0x4223e6` / `0x420eb2` | `0x429370` / `0x4294b0` | 0 | ❌ |
| `BombSakuya__method_called_by_on_tick` | `0x422c96` | `0x429370` | **3** | ❌ |
| `BombSanae__method_called_by_on_tick` | `0x423756` | `0x429370` | `~SPELLCARD[+0x78] & 1`（运行时 0/1） | 视符卡状态 |
| 33 YUYUKO 擦弹消去 | `0x42498b`–`0x4249be` | **无入口**，直写 `state = 3` | — | **❌**（不撒） |
| 敌机死亡 / `enmKillAll` / killAll 兄弟 | — | **一个入口都没调** | — | **❌ 不清弹** |

- `0x42498b`–`0x4249be`（弹的命中 / 擦弹处理器 `0x4248a0`）：读 `BULLET_MGR+0x7a41f0`（「擦弹消弹」使能位，卡 33 在 `0x40d5a9` 置 1）
  → `inc [mgr+0x7a41f4]` / `idiv 6` / `test edx,edx`（每 6 次擦弹触发一次）→ `mov WORD PTR [esi+0xf68],3`。这一段里**既没有 `call 0x4290e0` 也没有 `call 0x446f40`**。
- 敌机管理器的删除路径（含三个 killAll 兄弟与敌人死亡处理器 `0x430240` 一带）**一处都没调过上面 5 个入口中的任何一个**：
  敌人只撒自己的掉落（`0x430493` / `0x430624`）。**「boss 死了弹幕消失」是脚本干的，不是引擎干的**（§5）。

### 4.5 关卡脚本三兄弟（`etCancel` / `etClear` / `etClearAll`）

零售 `.ecl` 原文（`thecl` 反编译；`0N` = `01`…`07`，`st07` = Extra；逐关的数字按 N = 1…7 排列，`bs` 行的最后一位就是 `st07`）：

| 脚本 | `etCancel` | `etClear` | `etClearAll` | `laserCancel` |
| --- | :---: | :---: | :---: | :---: |
| `st0N.std`（7 关） | 0 | 0 | 0 | 0 |
| `st0N.ecl`（7 关） | 0 | **2** | 0 | 0 |
| `st0Nbs.ecl`（boss） | 7/11/11/11/15/28/**33** | 7/11/11/11/15/28/**32** | 1 | 3/5/5/5/7/12/19 |
| `st0Nmbs.ecl`（中 boss） | 2 | 2 | 0 | 0 |
| `st07mbs.ecl` | 9 | 9 | 0 | 5 |

> `.std` 里**一条消弹指令都没有** —— 清弹全在 `st0N.ecl` / `st0Nbs.ecl` / `st0Nmbs.ecl` 三兄弟里。
> `st07bs.ecl` 的 `etCancel` 比 `etClear` 多 1（33 vs 32），是唯一不对齐的一关。

固定套路（`st01bs.ecl` 偏移 `+0x9c` 起，逐字）：

```
unless ($TIMEOUT == 0) goto Boss2_200 @ 0;
etCancel(640.0f);        // ← 打掉 / 捕获 → 弹变道具
goto Boss2_220;
Boss2_200:
etClear(640.0f);         // ← 超时 → 直接消，不掉道具
Boss2_220: ... spellEnd(); funcSet(0); laserCancel(); ...
```

**boss 击破**（每关同形，`st01bs.ecl` 的 `BossDead_316`）：

```
BossDead_316: enmKillAll();
+60:          // thecl 标注的 60 帧等待
etClearAll(); spellEnd(); @BossItem(16, 10, 10); ...
```

`@BossItem` 的实体在 `default.ecl`：`unless ($TIMEOUT == 0) goto BossItem_240` ⇒ **超时则整段不掉**；
否则 `dropClear(); dropExtra(1,$B); dropExtra(2,$C); dropArea(64,64); dropMain($A); dropItems();`。
关卡中途波次（`st01.ecl` 偏移 `+0x4f4` / `+0x504`）：`enmKillAll(); etClear(640.0f);` ⇒ **也不掉**。

## 5. ★ 反直觉结论：「boss 被击破，满屏弹幕消失」那些弹**不出点**

`etClearAll`(613) 的子弹侧是 `0x4297a0` → mode 0（§1.4）⇒ **一颗点都不出**。
**但同一条 op 的激光侧是 `0x449090(1, 0)` ⇒ mode 1 ⇒ 每条激光撒一颗 type 9** —— 这是原生的一处不对称。
正常「打掉 / 捕获」走的是 `etCancel`(615)（mode 1）与 `etCancel2`(635)，**那些才出点**；
`etClear`(616) / `etClear2`(636) 是「超时清屏」，与 `etClearAll` 一样不出点。

## 6. 账：撒了但我们没记 / 记了但那个来源不撒

**撒了实体、账上看不到（我们侧）**

1. **`ce_cancel_all_bullets()` 的激光侧**（`sdk.h:133` 传 1）：每条激光撒一颗 type 9；而 `ce_cancel_frame_note(-1)`
   只登记「本帧消过弹、颗数不明」⇒ **66 审判（以及任何调这个包装的卡）激光那块是隐形的**。
2. **109 灵击爆风**（mode 1）：`pofv_account_blast(&s->st, n, chain)` 记的是 **n = 消掉几颗弹**，不是「撒了几颗道具」；
   且落点在场地外时一颗不撒也不加计数器（§2.3）⇒ 「消了 n 颗」里可能有若干颗**零产出**。
3. **117 三光球（mode 1）/ 116 季节点（mode 4）**：账是「本帧谁消的弹」（`ce_cancel_latch_*`）或颗数，**没有一条记实体道具**。
4. **116 的 mode 4 会动本作核心货币**：type 2 被吃到时 `ItemManager__collect_money_item` `0x446B00` 给 `MONEY += 1`
   （`MONEY` 同时是计分乘数，见 [`../../card/th18/05-shop-and-money.md`](../../card/th18/05-shop-and-money.md)）；没有任何一处记下「这次消弹额外产生了多少金钱」。
5. **114 的批只认 type ∈ {1,2} 且 `item+state == 3`**（`cards/collect2x.c:210`）⇒ 消弹撒的 type 9 **永远进不了 114 的批**；
   只有 116 / 原生 42 的那部分 **type 2**（≈37%）被真空吃到时才进。
6. **RNG**：mode 1 / 4 会推 `REPLAY_SAFE_RNG`（§2.4）⇒ 装卡后的老 replay 有必然 desync 点（🟡，本轮只做静态读）。

> 另（🟡 只核了 Line 一族）：`LaserLine__cancel_as_bomb_circle` 在 `0x44be8e` 也 `inc [mgr+0x7a41e8]`
> ⇒ 那个「消弹计数器」是**弹 + 激光共用**的；而 `ce_cancel_radius()` 只走子弹（`0x429370`），
> 所以它读回的颗数**不含激光**（`ce_cancel_all_bullets()` 走的激光侧更是连返回值都没人接）。

**记了账、但那个来源一颗实体都不撒（我们侧）**

1. **110（`mof.c`）**：`faith_cancel_account(n)` 每颗 +10 信仰点（`faith_slot_add_cancel(&s->slot, 0, bullets, 0, 0)`），
   但 `ce_cancel_radius(..., 0)` ⇒ **零实体**；`faith.c:36` 自己也写了「直接进账，不撒实体」。这条账在设计上说得通（给的是「信仰点」不是 TH18 道具），
   但在「消弹 → 撒出实体道具」这条链上 110 是**断的**。
2. **111（`signal.c:585`）**：同上（小绿点每 100 颗 +10），零实体。
3. **107（`senro.c`）**：`stat_cancel` / `senro_small_cherry(&s->st, n)` 记小樱点，零实体。
4. **SDK 的「本帧消弹记录」本身**（`ce_cancel_frame_note` / `ce_cancel_latch_*`，`sdk.h:728` / `faith.h:117`）：
   记的是**谁在消弹**（公平分配闩锁），**从不表示「有实体道具产出」**。用闩锁去推「本帧该收多少点」的卡，
   在 110 / 111 / 107 / 109-CA 这些 mode 0 路上只会拿到**空账**。

## 7. 仓内注释订正（三处 —— **本节只写文档，未改任何 `.c` / `.h` / spec**）

> 这三处都要单独授权才能动源码 / spec 原文；本节只登记「错在哪、一手证据是什么、该改成什么」。

1. **`mods/th18.v1.00a/card-expand/native/cards/pofv.c:38` / `:40`**（`Bullet__cancel` 的头注）：
   ① 说「全 EXE 的消弹入口只有**四个**」→ ❌ **五个**（漏 `0x4294b0` `cancel_rect_as_bomb`，§1.2）；
   ② 说 mode `4 → push 2` → ❌ mode 4 是「**多数 push 9 + 少数 push 2**」，而**真正什么都不撒、只 inc 计数器的是 mode 3**（§2.1）。
   证据：`0x4291b2`–`0x429243`（mode 4 的三路分流）与 `0x429163` / `0x4291b5`（mode 3 落 `0x429248`）。
2. **`sdk.h:124`** 的「全屏消弹：弹幕（**→ 点道具**）+ 激光」与 **`cards/judgment.c:62`** 的「安全垫：全屏消弹（弹 → 点道具）+ 激光」：
   两处都**写反了**。`ce_cancel_all_bullets()` = `0x4297a0`（子弹，内部 `xor edx,edx` ⇒ **不撒**）+ `0x449090(1, 0)`（激光 mode 1 ⇒ **撒**）。
   ⇒ 正确口径是「**子弹不撒、激光撒**」。按原注释去算「全屏清弹能收多少点」的卡会**算反**。
   证据：`0x429932` / `0x42993a`（子弹侧 mode 0）与 `sdk.h:133` 的 `(1, 0)` + §3 的激光表。
3. **`docs/superpowers/specs/2026-09-11-system-card-spec-review.md` §6.4**：
   ① 「全 EXE 的消弹入口只有四个」→ ❌ **五个**（同上漏 `0x4294b0`）；
   ② 「`0x429250` 在用」的清单把 `etCancel` / `etClear` 记成 `0x4359d9` / `0x435a9d`（这两条**是对的**），但把「保护单弹 `bullet+0x24`」记成
   「能挡 `0x429250` / `0x429370`」→ ❌ **半错**：`0x429250` 全函数体不读 `+0x24`，能挡的只有 `0x429370` 与 `0x4294b0`（§1.3）。

这三条与 `pofv.c` 自己在 2026-09-12 的订正一致（它已经改对了 `+0x24` 那半），**没改的只剩上面列的**。

## 8. 否定结论与未钉死项（全部保留）

**否定结论（本轮证否，和肯定结论一样重要）**

1. **引擎不负责「符卡结束 / boss 击破清弹」**：敌机管理器一处都没调那 5 个入口（§4.4）。
2. **没有带 exclude 数组的消弹入口**；要「消一堆但放过某些」只能自己扫池逐弹调 `0x428e90`（或临时用 `+0x24` 标记，但只有 `0x429370` / `0x4294b0` 吃这一套）。
3. **不存在消弹入口的函数指针表**：8 个入口地址当 dword 全文件搜 0 命中（§1.1）。
4. **超时清屏（`etClear` / `etClear2` / `etClearAll` 子弹侧）不出点**，无论弹有多少颗。
5. **`0x429250` 不吃 `+0x24` 保护位**（§1.3）。

**未钉死 / 缺的一手**

| # | 项 | 试过什么 | 缺什么 |
| --- | --- | --- | --- |
| 1 | `bullet+0x20` bit `0x200` 的**置位点**（置位 ⇒ `0x428ee7` 处连特效都不转、也不撒道具） | 全 asm 搜 `+0x20],0x200` 的写（只有 `0x41d916`，属 STD 区、对象不是弹）；搜弹生成函数 `0x4258e0`–`0x425a60` 的 `+0x20]` 写（只有 `or [edi+0x20],0x10`）；三个读点 `0x4245a9` / `0x42486d` / `0x424d78` 都用来**跳过碰撞判定** | 哪条路把 `0x200` 塞进 flags（弹种表 `0x4c5f90` 的哪个字段？还是别处的 `or eax,0x200`）。❓ **本条会让「mode 1 一定撒」出现例外** |
| 2 | mode 4 的**实跑**分布 | 只做了静态分支推导（§2.2），且 `[mgr+0x7a41d0]` 的语义（是否「累积消弹数」、跨关清不清零）没查 | 一次实跑计数（打 mode 4 的 116 / 原生 42），或 `0x7a41d0` 全部读点分类 ❓ |
| 3 | `0x449010` 的 mode 到底是 A 还是 B | 用 vtable 压栈序 + `LaserLine__cancel_as_bomb_circle` `0x44be7f mov edx,[ebp+0x10]` 反推 ⇒ mode = A | 再读 `LaserInfinite__cancel_as_bomb_circle` `0x44e050` 与 `LaserCurve__cancel_as_bomb_circle` `0x4521b0` 的同一读点做三重复核 🟡 |
| 4 | `0x45c3cc`（玩家死亡时 `0x449090(1,·)`）的语义 | 读到它在 `Player__on_tick__body` 的 `[player+0x638] ∈ {4, 0xf}` 分支里 | 玩家状态机 state 4 / 0xf 的语义（[`../../player/th18/03-player-hit-gate.md`](../../player/th18/03-player-hit-gate.md) 本轮没查） |
| 5 | `0x43e2ea`（`GuiMsgVm__constructor` 里的 `cancel_all` + 内联激光清屏）的触发时机 | 确认它在构造函数体内、子弹 + 激光都清（激光那段是内联、mode 0） | 这个构造函数有几个调用点、是否等于「每次对话开始」❓ 若是，则它与 ECL op 518 `dialogRead` 是两条并行路，会**重复清两次** |
| 6 | ECL op 712 的名字冲突 | 同一 opcode：ExpHP `712__hitboxRect` vs thtk `712 etCancelRect` | 两套命名的取舍（本文两个名字都给，不擅自定名） |
| 7 | ECL op 523 `spellEnd` 里 `has_card(0x28)` 的卡是谁 | 核了 `0x412fe0` = 遍历卡链比 `[card+4]`；`0x28` = 十进制 40 | 卡 40 = KISERU 这条映射是**间接**来的（[`../../card/th18/08-catalog.md`](../../card/th18/08-catalog.md)），没从 vtable 三重复核 ⇒ 「spellEnd 撒 type 4」标 🟡 |
| 8 | 关卡切换是否还有引擎侧清弹 | 全 exe 直接调用清单 + 间接地址搜索（0 命中）⇒ 没有 | 若要 100% 收口，需要 Ghidra 递归下降 xref 与线性扫描对账（本轮没抢工程锁）🟡 |

**✅ 已肯定（曾经的疑点，已收口）**：`0x4294b0` **也** `++[mgr+0x7a41e8]`（`0x429768`，在 `0x42977c call 0x428e90` 之前）
⇒ 卡侧若要「矩形消弹返回颗数」也能读 `0x7a41e8`，但它与 `0x429370` 一样**只按「消掉即 +1」计，与 mode / 是否真撒道具无关**。

## 9. 复核命令（本轮证据链，可逐条重跑）

```bash
cd local/th18.v1.00a
# 撒道具核心 + mode 真值表（§2.1/§2.2）
objdump -d -M intel --start-address=0x4290e0 --stop-address=0x429250 th18.exe
# 五个入口的函数体（§1）
for a in 0x428e90 0x429250 0x429370 0x4294b0 0x4297a0; do objdump -d -M intel --start-address=$a --stop-address=$((a+0x2a0)) th18.exe; done
# 调用点穷举（§1.1）：线性扫全 .text 的 call rel32
objdump -d -M intel th18.exe | grep -c "call   0x428e90"     # 9
objdump -d -M intel th18.exe | grep -E "call   (0x429250|0x429370|0x4294b0|0x4297a0)"
objdump -d -M intel th18.exe | grep -E "call   (0x449010|0x449090|0x448fa0|0x446f40)"
# 间接调用证否：入口地址当 dword 全文件搜（每个 0 命中）
# ECL 侧（脚本原文）
local/vendor/thtk/build/thecl/thecl -d 18 -x -m local/vendor/eclmap/eclmap/th18.eclm dat/st01bs.ecl | grep -nE "etCancel|etClear|laserCancel|enmKillAll|BossItem"
local/vendor/thtk/build/thecl/thecl -d 18 -x -m local/vendor/eclmap/eclmap/th18.eclm dat/default.ecl | grep -n -A12 "^void BossItem"
```

> 机读出处：`games/th18.v1.00a/symbols.json` 记的 `exe_md5` = 本文声明的 `9969cac756098c1da05a81de45437a70`（同 build）。

---

## 7. `zBullet.state`（`+0xf68`, uint16）取值表 —— 2026-10-12 定稿（mod 侧交叉实测）

来源：命名金矿 `local/vendor/th-re-data/data/th18.v1.00a/type-structs-own.json`
（`zBullet.state` 就在 **0xf68**，且 `zBullet` 整好 **0xfa0** 字节）+ 本文 §1 的消弹门 + mod 侧只读探针：

| 值 | 含义 | 证据 |
| :-: | --- | --- |
| `0` | 空槽 | 直方图：槽数 − 活弹 − 哨兵 |
| `1` | **活**（活动弹）| mod 侧对账：活弹链表包含它 ✓（**链表 = `[1]+[2]+[3]`**，见下条修正）|
| `2` | **出现中**（spawning）| §1 的入口 #2–4 门 `state ∈ {1,2}`（`0x4292a7`/`0x4293ba`/`0x4294da`）|
| `3` | **消弹中** | `cancel_all` 跳过；§1.4 那段的"置位即整段跳过" |
| `4` | 收尾（**不可消**）| 由 `{1,2}` 门反推 |
| `5` | **池尾哨兵（假弹）** | mod 侧 2026-10-12 实跑钉死：**恒为池内最后一格（下标 2000）**、`flags=0`、`index_in_array=0`、**`vm=0`（无 anm）**、32/32 次一致、**从不进 `tick_list_head`** |

**★ 活弹链表的精确口径**（2026-10-12 实跑修正）：`mgr+0xbc` 链表数 = `[1] + [2] + [3]`
（实测 10=10+0+0、11=10+1、9=7+1+1）⇒ 链表 = **所有非空槽且非哨兵**；
早先只在"恰好全是 `[1]`"的时刻对账过，故曾写成"≡ `[1]`"（那是特例）。

**⚠ 给改造者的硬结论**：**"活弹"必须按 `state == 1`（或走 `mgr+0xbc` 活弹链表）判** ✓；
数组平扫里用 `state != 0` 会把 **5 号哨兵**当真弹 ✗（mod 侧 108/111/116/reverse 都踩过）。

**相关结构**（同一份金矿）：`zBulletManager.tick_list_head` = **`mgr+0xbc`**（活弹双向循环链表 ✓）、
`bullets[0x7d0]` = **`mgr+0x108c`**（静态池，每张 `0xfa0`）、`zBulletList = {entry,next(+4),prev(+8)}`、
每张弹的链表节点在 **`bullet+0x10`**、`bullet+0x660` = `index_in_bullet_array` ✓。
