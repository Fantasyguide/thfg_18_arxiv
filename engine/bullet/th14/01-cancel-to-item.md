# TH14 辉针城 消弹 → 出不出实体道具

> **版本**：TH14（`local/_scratch/th14_100b.exe`，897536 字节，PE32 i386，md5 `41a8225e4822f217fb57b4e6530ac593`）。
> ⚠ **`local/th14.v1.00b/` 不存在** ⇒ 本作只有**引擎侧一手**，脚本侧（`dat/*.ecl`）核不了（§3 第 1 条）。
> 本文裸地址默认属该版本；引用别作一律写 `th18:0x…`。跨作对照见 [`../th18/01-cancel-to-item.md`](../th18/01-cancel-to-item.md)。
> 一手工具：`objdump -d -M intel --no-show-raw-insn`（222047 行）；只用来取名（**非一手**）：ExpHP `th14.v1.00b/funcs.json`。
> 复现命令与常量真值集中列在 §4。

## 1. 发现（一手事实）

### 1.1 核心：`0x41ae00`（`ecx` = `float3* pos`，`edx` = `mode`，无栈参）

```
41ae0a  test edx,edx
41ae0c  je   0x41b2b4          ; ★ mode 0 → 直接跳到函数尾（连计数器都不动）
41ae12  movss xmm2,[esi]       ; x        ← 下面四道场地门，任一出界即跳 0x41b2b4
41ae16  movss xmm3,ds:0x4c1b14 ; 32.0f
41ae75  inc  DWORD PTR [eax+0x9bf6c8]   ; eax = [0x4db530]（BULLET_MANAGER_PTR）
41ae81  cmp  edx,0x1 / jne 0x41af3f     ; mode 1：就地建 1 颗道具
41af3f  cmp  edx,0x3 / jne 0x41b14d     ; mode 3：同一套
41b14d  cmp  edx,0x2 / jne 0x41b2b4     ; mode 2：力量道具 + 小道具
41b2b4  ret
```

### 1.2 mode 真值表（逐字节）

| mode | 行为 | 一手出处 |
| --- | --- | --- |
| 0 | 直接 return：**不撒、也不 `inc` 计数器** | `0x41ae0a` / `0x41ae0c je 0x41b2b4` |
| 1 | 每颗弹 **1 个 `type 0xa`** | `0x41ae81`；建道具写 `[item+0xbf4]=0xa` 在 `0x41b00c` |
| 3 | 每颗弹 **1 个 `type 0xa`**（另有一支「槽满」处理 `0x41b0ef`） | `0x41af3f`；`[item+0xbf4]=0xa` 在 `0x41b1d1` |
| 2 | 每 5 颗补 1 个 **`type 1`（力量道具）** + 1 个 `type 0xa`，两者都受符卡位门控 | `0x41b14d`；`push 0x1; call 0x4397b0`（`ItemManager::spawn_item`） |
| 4 / 5 | 只 `inc` 计数器，**什么都不撒**；**无调用点** | 落 `0x41b2b4` |
| 其余 | 同 4 / 5 | 同上 |

- mode 2 的门 = `(SPELLCARD[[0x4db534]+0x7c] & 1) == 0`（即**非符卡进行中**才撒）；`n % 5 == 0` 里的 `n` 研究**没点明取址**（TH15 的同形支一手读的是刚 `inc` 过的计数器 `[mgr+0x141f0c8]`，TH14 侧未核）⇒ 🟡 见 §3 第 5 条。
- `inc [mgr+0x9bf6c8]` 在**分派之前**（`0x41ae75`）⇒ mode 2/3/4/5 都会 `+1`，只有 mode 0 与出界弹不 `+1`。

### 1.3 撒出来的是什么（一手）

- 在 `[0x4db660]`（ITEM_MANAGER）的池 `+0xddd868` 上**就地建实体道具**：`[item+0xbf0]=5`、`[item+0xbf4]=**0xa**`、`[item+0xbf8]=**0xa**`，pos 复制，速度实参 `0x400ccccd` = **2.2f**。
- 收 1 个（`0x438b95` → `0x439d30` + `0x41d5e0`）改**两个**计数器（一手逐字节；与 TH15 的 `0x43b200` + `0x420f50` **逐条同形**）：

| 计数器 | 加多少 | 单位 / 钳位 | 一手 |
| --- | --- | --- | --- |
| `[0x4f584c]` | `(int)(2.0f × 100.0f)` = **+200** | ×100 定点（按 1/100 读 ⇒ 显示 **+2**）；钳 `[0x4f5854]` | `0x439d30`：`mulss xmm1,ds:0x4c1b54`(=100.0f) → `cvttss2si eax` → `add` |
| `[0x4f5830]` | `10/10` = **+1** | 钳 `0x3b9aca00`（= 999,999,999） | `push 0xa; call 0x41d5e0`：魔术数 `0x66666667` + `sar edx,2` ⇒ **÷10** |

- ⇒ **两个计数器、两条都对**：`+200` 是 `[0x4f584c]` 的原始加值（×100 定点），`+1` 是 `[0x4f5830]` 的原始加值。**type 0xb 那支**是 `20.0f` ⇒ `[0x4f584c] += 2000` + `[0x4f5830] += 1`。
- 🟡 两个计数器的**名字没有一手钉死**（只钉到「`[0x4f584c]` 是 ×100 定点、`[0x4f5830]` 钳 999,999,999」）⇒ 本文只写「计数器（疑为分数 / 最大得点系）」，不当已钉死的名字用。
- `ItemManager::on_tick__body` `0x438550` 在 `0x438b3e` 读 `[item+0xbf4]` 当 type（`dec eax / cmp eax,0xa` + **11 项**跳转表 `0x439088`）：**type 9 与 0xa 同一支**（`0x438b95`），0xb 是「20.0f ⇒ +2000」。
- **每颗弹 1 个**（mode 1/3 各只有一处建道具）·**是实体、走过去收**（有 pos / 速度 / anm VM）。
- ★ 与 TH16/TH18 同款**「越挤越难拿」**：按 `[ITEM_MANAGER+0xddd87c]`（在库数）与 `+0xddd878` 分档写 `[item+0xc00]` 透明帧（`0x400`/`0x200`/`0x100` 三档 → `+0x10`/`+0x8`/`+0x4`/`+0x2`）。

### 1.4 消弹入口全集（弹幕 5 个 + 激光 1 族）

| # | 地址 | 签名 / 形态 | mode 从哪来 |
| --- | --- | --- | --- |
| 1 | `0x41b2f0` | `cancel_radius(pos, mode)` `ret 8` + `xmm2` 半径（2000 槽） | 调用方栈参 |
| 2 | `0x41b3f0` | 同上 + 查 `[弹+0x24]==0`（as_bomb 变体） | 调用方栈参 |
| 3 | `0x41b4f0` | `cancel_rectangle(pos, size, mode)` `ret 0xc` + `xmm3` 角度 | 调用方栈参 |
| 4 | `0x41b880` | `cancel_all`（2000 槽全清） | **硬编码 0**（`0x41b94a xor edx,edx`）⇒ 永不撒 |
| 5 | `0x41ac70` | `Bullet::cancel(ecx=弹, [ebp+8]=mode)` `ret 4` → `0x41ae00(pos, mode)` | 调用方栈参 |
| 6 | 激光族 | `0x43ab80`（半径传 `vtable[0x24]`）/ `0x43ac00`（全清）/ `0x43aaf0`（ECL 545） | 调用方栈参 |

- 调用点计数（一手，全 `.text` 线性扫 `call rel32`）：#1 **只有 2 个**（`0x4293dc` = ECL 615 mode 1、`0x429451` = ECL 616 mode 0）；#2 **10 个**（`0x413115` BombMarisaB、`0x414a27`/`0x414b1d`、`0x415127` BombReimuB、`0x415a23` BombSakuyaA::on_tick、`0x415ad9` BombSakuyaA::method_10、`0x415f22` BombSakuyaB、`0x429483`/`0x4294b5` ECL、`0x44dc88` 玩家）；#3 **3 个**（`0x412a5a` BombMarisaA::method_10、`0x413f1e` BombReimuA::method_10、`0x42942f` ECL 712）；#5 **5 个**（`0x418635`/`0x4189d3` 弹自毁、`0x41b396`/`0x41b49c`、`0x41b857`）。
- 激光类里**直接 `call 0x41ae00`** 是 **8 处**（研究记的 5 处不全）：`0x43c814`（mode=`[ebp+0x14]`）、`0x43d4de`（`[ebp+0x10]`）、`0x43dc30`（`[ebp+0x8]`）、`0x43e823`（`[ebp+0x14]`）、`0x43f40d`（`[ebp+0x10]`）、`0x43fb70`、`0x442bf1`、`0x4434c0` ⇒ **激光与弹幕共用同一个核心、同一张 mode 表**。
- ★ 订正：研究把 #3（矩形入口 `0x41b4f0`）的调用点只记成「ECL 712 → mode 1」，实读是 **3 处**，另两处是炸弹（`0x412a5a` `BombMarisaA::method_10`、`0x413f1e` `BombReimuA::method_10`），各自带自己的 mode。

### 1.5 出的那几条 / 不出的那几条

**✅ 出（mode 1，部分 mode 2/3）**

- ECL **615 `etCancel`**（`0x4293cd push 0x1` → `0x4293dc`）、ECL **635 `etCancel2`**（`0x429474`）、ECL **712 `etCancelRect`**（`0x429424`）。
- **炸弹 —— 一手读全：正是「六种」（6 个 `Bomb*` 类各有 ≥ 1 处调用点，`th14-funcs.json` 里这 6 个类都在）**：

| 类 | 调用点（一手） | 入口 | mode |
| --- | --- | --- | --- |
| `BombReimuA` | `0x413f1e`（method_10） | `0x41b4f0`（矩形） | 调用方栈参 |
| `BombReimuB` | `0x415127`（method_10） | `0x41b3f0` | `push 0x2`（研究记 `0x415125` = push 位） |
| `BombMarisaA` | `0x412a5a`（method_10） | `0x41b4f0`（矩形） | 调用方栈参 |
| `BombMarisaB` | `0x413115`（method_10） | `0x41b3f0` | `push 0x2`（研究记 `0x41310c` = push 位） |
| `BombSakuyaA` | `0x415a23`（on_tick）+ `0x415ad9`（method_10）+ 激光 `0x415a3e` / `0x415afc` | `0x41b3f0` + `0x43ab80` | `~符卡位&1` |
| `BombSakuyaB` | `0x415f22`（method_10） | `0x41b3f0` | `push 0x3`（研究记 `0x415f1f` = push 位） |

- 另有两处 `0x41b3f0` 的调用点（`0x414a27` / `0x414b1d`，研究记作「`BombReimuA` 的两处」）落在**符号表里没有名字的一个函数**里（起头 ≈ `0x4149ff`，在 `InterpFloat::reset_timer` 与 `BombReimuB::begin` `0x414be0` 之间）⇒ 🟡 归属靠上下文、不是符号；但它与 TH15 的同类子过程同形（`push 0x1b` → `0x46e470` → `push 0x1` → as_bomb）。**这不影响「六种」这个计数**（6 个类各自都已有一手调用点）。
- **激光**（传 mode 1 时；ECL 615/635 的激光半边传的就是 `push 0x1`）。

**❌ 不出**

- ECL **616 `etClear`** / **636 `etClear2`**（mode 0）。
- ECL **613 `etClearAll`** / 614：`cancel_all` `0x41b880` —— **弹幕半边硬编码 mode 0 ⇒ 一颗不出**；激光半边 `0x43ac00(1,0)` 走激光类**自己的** mode。
- **玩家中弹**（`Player::on_tick` `0x44dc85`，mode 0）、**弹自毁**（`0x418631` / `0x4189cf`，经 #5 的两处 `push 0x0`）。

### 1.6 场地门与 RNG

- 门（`0x41ae00` 内联，位于 `inc` **之前**）：`x+32 ≤ −192`、`x−32 ≥ 192`、`y+32 ≤ 0`、`y−32 ≥ 448` ⇒ **不撒、也不 `inc`**。
- RNG：**✅ 消耗**。mode 1/2/3 每撒一颗调一次 `[0x4db510]` 上的 `0x4034b0`（`finit` → `0x403310` 取 u32 → `×[0x4c18dc]` → `−[0x4c19fc]` = `[−1,1)` 的 float），再算角度。mode 0 在 rand 之前就 return；4/5 只 `inc`、不取 rand。

### 1.7 boss 击破 / 符卡结束 / 关卡切换属哪一档

- **引擎侧（一手 handler）**：`etCancel`(615) = mode 1 ⇒ ✅ 出；`etClear`(616)/`etClear2`(636) = mode 0 ⇒ ❌ 不出；`etClearAll`(613)/614 = `cancel_all`（弹幕硬编码 0）⇒ 弹幕 ❌、激光看激光类自己的 mode。**档位与 TH13 / TH15 / TH17 同型**（本作不是 TH12 那种「`etClearAll` 也出货」的例外）。
- **敌机死亡 / `enmKillAll`**：清弹入口的调用点全集里**没有敌机管理器** ⇒ 引擎侧不清弹（弹幕消失是脚本干的）。
- **关卡切换 / 对话 —— ✅ 一手钉死：清弹，但一颗道具都不出。** `GuiMsgVm::initialize` 在 `0x4320b1` 调 `cancel_all` `0x41b880`（弹幕、硬编码 mode 0），随后 `0x4320b6 push 0x0` / `0x4320b8 push 0x0` / `0x4320ba call 0x43ac00`（激光全清、**两个开关都传 0**）⇒ **与 TH15 的 `0x437b8e` 那条逐条同形**。

## 2. 验证（怎么排除掉别的路）

1. **调用点穷举回指令边界**：全 `.text` 线性扫 `call rel32`，把 9 个入口（6 个弹幕 + 3 个激光）的全部命中按 `th14.v1.00b/funcs.json` 归到函数（名字**非一手**，只用来定位），再逐处 `objdump --start-address` 回到 `e8` + rel32 本身读实参 —— **没有一处**靠「函数名像」推断。新增/订正的 3 处激光调用点（`0x43fb70` / `0x442bf1` / `0x4434c0`）都回读了机器码（`e8 8b b2 fd ff` 一类的 direct call）。
2. **type 字段两头对上**：建道具写 `[item+0xbf4]=0xa`（`0x41b00c` / `0x41b1d1`）↔ 派发读 `[item+0xbf4]`（`0x438b3e`）。全 exe **只有这两处**写 `0xa` 进 `+0xbf4`，且都在 `0x41ae00` 内 ⇒ **「消弹专属道具」= type 0xa**。
3. **第三方互证**：`ItemManager::spawn_item` `0x4397b0` 里 `type ∈ {9,0xa,0xb}` 走独立槽（`0x4397c0`–`0x4397d5` 三条 `cmp/je`）；`0x41ae00` 的 mode 2 支走的是 `spawn_item`（`call 0x4397b0`）而不是就地建 —— 两条生产路都指向同一个道具池。
4. **「消了 n 颗」≠「出了 m 个」**：出界弹过不了门（门在 `inc` 之前）⇒ 贴着场地边消掉的弹**零产出**、连计数器都不加。台面上「全屏清弹」的手感里有一部分是**白消**。
5. **排除「收点系统自己消弹」**：#1–#4 四个入口的调用点全集里**没有任何收点 / 道具收集函数** ⇒ TH14 的「收点批次 / 倍率」系统**本身不消弹**（这正是卡 114 的主题，见 §5）。
6. **排除「敌机管理器的删除路径清弹」**：清弹入口的调用点 owner 里没有敌机管理器函数。
7. **除法魔术数按指令逐步复算**：`0x41d5e0` 的 `0x66666667` + `imul` + `sar edx,2` + `shr eax,0x1f` + `add`，代入 `n = 10 / 25 / 50 / 100` 得 `1 / 2 / 5 / 10` ⇒ **确实是 ÷10**（不是 ÷5）⇒ `push 0xa` 的结果是 `+1`。

## 3. 结论（可信度 + 适用范围）

> **TH14：ECL 615 `etCancel` / 635 `etCancel2` / 712 `etCancelRect`（mode 1）、六种机体炸弹（6 个 `Bomb*` 类，见 §1.5 表）、
> 以及激光（传 mode 1 时）—— 把每颗被消的弹变成 1 个 `item type 0xa`（收 1 个 = `[0x4f584c] += 200`、`[0x4f5830] += 1`），
> 实体、落地走过去收。`etClear` / `etClear2` / `etClearAll` 的弹幕半边、玩家中弹、弹自毁 —— 一颗都不出。消耗 RNG。**
> **场地门与 TH16/TH17/TH18 逐条同形**（32 / −192 / 192 / 0 / 448）；**对话开始清弹但一颗不出**（§1.7）。

**可信度：✅ 高（引擎侧）。** 依据：`0x41ae00` 逐字节读通（mode 分派 + 场地门常量 + 就地建道具三元组 `5/0xa/0xa` + RNG + 池压力分档）；type 用「写 `[item+0xbf4]`」与「读 `[item+0xbf4]`」两头对上；全部调用点逐条回指令边界。

**⚠ 限制（不许省略）：`local/th14.v1.00b/` 不存在。** 本轮用的是野样本 `local/_scratch/th14_100b.exe`（md5 `41a8225e4822f217fb57b4e6530ac593`，PE 节表可读、字节完整），**没有 `th14.dat`** ⇒ **只有引擎侧一手，脚本侧核不了**：ECL `etCancel` 的调用点是**反汇编读出来的**（`0x4293cd` 那一带的 `push 0x1`），**不是**从反编译脚本读出来的。因此「符卡结束用 `etCancel`、超时用 `etClear`、终盘用 `etClearAll`」这条**只有引擎侧 handler 已钉死，脚本侧标 🟡**。

**🟡 / ❓ 未钉死项（照实保留）**

| # | 项 | 现状 | 怎么钉 |
| --- | --- | --- | --- |
| 1 | ECL 脚本侧档位 | 🟡 无 `th14.dat`，`dat/*.ecl` 读不到 | 拿到样本后 `thecl -d 14 -m local/vendor/eclmap/eclmap/th14.eclm dat/st0Xbs.ecl` 数三个 op |
| 2 | 两个计数器的**名字** | 🟡 只钉到「`[0x4f584c]` 是 ×100 定点（钳 `[0x4f5854]`）、`[0x4f5830]` 钳 999,999,999」；**名字（分数 / 最大得点）没有一手** | 跟 HUD 的得点 / 最大得点显示对一次账 |
| 3 | **矩形 / 点族入口的门** | 🟡 只逐条读了「圆（半径）」族；`0x41b4f0`（矩形）的门未单独逐条读 | 单读 `0x41b4f0` 的判定段（`0x41ae00` 的门是内联的，但矩形入口自己的门要单独核） |
| 4 | TH14 的 `0x414a27` / `0x414b1d` 归哪个类 | 🟡 落在符号表没有名字的函数里（起头 ≈ `0x4149ff`），研究记作「`BombReimuA` 的两处」；**不影响「六种」的计数** | 找谁登记/调用 `0x4149ff`（vtable 槽或炸弹状态机） |
| 5 | mode 2 里 `n % 5` 的 `n` 取址 | 🟡 研究只写了 `n % 5`，**没给 `n` 的读取点**（TH15 同形支是 `mov eax,[eax+0x141f0c8]`） | 读 `0x41b14d` 支里 `n` 的来源（是否就是刚 `inc` 的 `[mgr+0x9bf6c8]`） |

**适用范围：仅 `th14_100b.exe`（md5 `41a8225e4822f217fb57b4e6530ac593`），不可外推**（mode 号跨作不可移植；TH14↔TH15 是唯一一对 mode 表 + item type 都同形的，可互证，但仍各自读过自己的 exe）。

## 4. 证据（地址 / 常量 / 复现命令）

| 项 | 真值 | 地址 / 位置 |
| --- | --- | --- |
| 场地边距 | `32.0f` | `0x4c1b14` |
| 左界 | `−192.0f` | `0x4c1c9c` |
| 右界 | `192.0f` | `0x4c1b74` |
| 下界 | `448.0f` | `0x4c1bc0` |
| y 上界 | `0.0f`（内联立即数） | `0x41ae56` |
| 角度系数 | `0.17453292`（= 10°） | `0x4c195c` |
| 角度偏移 | `1.5707964`（= π/2） | `0x4c1a54` |
| 速度实参 | `0x400ccccd` = `2.2f` | 指令立即数 |
| 计数器系数 | `2.0f` | `0x4c1a78` |
| ×100 定点系数 | `100.0f` | `0x4c1b54` |
| 消弹计数器 | `[BULLET_MGR+0x9bf6c8]` | `inc` 在 `0x41ae75` |
| 计数器 A（疑为最大得点系） | `[0x4f584c]`（钳 `[0x4f5854]`）：`+= 200` | `0x439d30` |
| 计数器 B（疑为分数） | `[0x4f5830]`（钳 999,999,999）：`+= 1`（`n/10`） | `0x41d5e0` |
| 对话清弹 | 弹幕 `0x4320b1` + 激光 `0x4320ba`（两开关 0） | `GuiMsgVm::initialize` |
| RNG | `[0x4db510]` → `0x4034b0` | — |
| 道具池 | `[0x4db660]+0xddd868`；压力 `+0xddd87c` / `+0xddd878` | — |

> 上表的 float 真值由 PE 节表映射回文件偏移**直读 u32 复核**（不是照抄）：`0x4c1a54` = `0x3fc90fdb`（π/2，代码里作 `subss` 的被减数 ⇒ 实得 −π/2）、
> `0x4c1a78` = `0x40000000`（2.0f）、`0x4c1b54` = `0x42c80000`（100.0f）；四道门依次 `0x42000000` / `0xc3400000` / `0x43400000` / `0x43e00000`。
> 角度那两支也在指令里对上了：`0x41aea2 mulss xmm1,ds:0x4c195c` → `0x41aeb6 subss xmm1,ds:0x4c1a54`。

```bash
# 样本：local/_scratch/th14_100b.exe（md5 41a8225e4822f217fb57b4e6530ac593）
cd local/_scratch
objdump -d -M intel --no-show-raw-insn th14_100b.exe > /tmp/th14.asm
# 核心 + mode / 场地门（§1.1–1.3）
objdump -d -M intel --start-address=0x41ae00 --stop-address=0x41b2f0 th14_100b.exe
# 五个弹幕入口的函数体（§1.4）
for a in 0x41b2f0 0x41b3f0 0x41b4f0 0x41b880 0x41ac70; do
  objdump -d -M intel --start-address=$a --stop-address=$((a+0x1c0)) th14_100b.exe; done
# 调用点穷举（每个命中回指令边界看实参）
grep -nE "call +(0x41b2f0|0x41b3f0|0x41b4f0|0x41b880|0x41ac70|0x41ae00)" /tmp/th14.asm
# 激光侧 8 处直调核心（§1.4）
grep -nE "call +0x41ae00" /tmp/th14.asm
# type 0xa 的两处写入（§2 第 2 条）
grep -nE "mov +DWORD PTR \[e..\+0xbf4\],0xa" /tmp/th14.asm
# 两个计数器 + 除法魔术数（§1.3 / §2 第 7 条）
objdump -d -M intel --start-address=0x439d30 --stop-address=0x439d58 th14_100b.exe
objdump -d -M intel --start-address=0x41d5e0 --stop-address=0x41d612 th14_100b.exe
# 对话开始清弹（§1.7）
objdump -d -M intel --start-address=0x4320a9 --stop-address=0x4320c0 th14_100b.exe
```

## 5. 对卡的直接影响（只读观察，不改代码）

- 现状（`grep -n "ce_cancel\|cancel" mods/th18.v1.00a/card-expand/native/cards/collect2x.c`）：**0 命中** —— 114（COLLECT_2X）**一次都没调过任何消弹入口**。
- 一手判定：**✅ 与一手一致**。TH14 的「收点批次 / 倍率」系统（`collect2x.c` 复刻的那个）**本身不消弹**：`0x41b2f0` / `0x41b3f0` / `0x41b4f0` / `0x41b880` 的调用点全集里没有任何收点函数（§2 第 5 条）⇒ **「114 不消弹」是复刻的一手结论，不是漏做**。
- ⚠ 但不要把「TH14 消弹会出实体」与 114 的批混起来：**如果**将来要让 114 顺带模拟「TH14 的消弹产物」，TH18 侧对应的 mode 是 **1**（`type 9` 小分道具），**不是 mode 4**（TH18 的 4 = `type 9` 多数 + `type 2` 少数，会往批里混钱；而 TH18 没有 TH14 的 `type 0xa` 这个道具）。
- 另：114 的批成员门只认 `type ∈ {1,2}` 且 `item->state == 3`（`collect2x.c:284` 一带）⇒ 即使将来补了消弹，**撒出来的 `type 9` 也进不了批**（与 116 那档同一条账，另见 [`../th18/01-cancel-to-item.md`](../th18/01-cancel-to-item.md) §6）。
- 本文**只读**，未改任何 `.c` / `.h` / spec，未 `git add`。
