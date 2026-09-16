# TH16 天空璋 消弹 → 出不出实体道具

> **版本**：TH16 v1.00a（`local/th16.v1.00a/th16.exe`，imagebase `0x400000`，md5 `70d3429444e1e6c18a72e7200a4d7ce8`）。
> 本文裸地址（如 `` `0x416a00` ``）默认属该版本；引用别作一律写成 `th17:0x419c30`、`th18:0x4290e0`。
> **一手工具**：`objdump -d -M intel <exe>`（现解，dump 落 `/tmp/can/`，不入库）；PE 常量按节表映射用只读小脚本直读（真值见 §4.2）。
> **可信度**：✅ 主体 —— 核心函数逐字节 + **全部**消弹入口调用点回指令边界重读（2026-09-15 复核）。
> 未钉死项在 §2.5，🟡 就地标注。**结论只适用 TH16 v1.00a**：同一段消弹在 TH17/TH18 的 `mode 1` 行为**正好相反**（§2.4）。
> **相关**：[`01-core-engine.md`](01-core-engine.md)（弹池/运动 VM）· [`02-bullet-vm-model.md`](02-bullet-vm-model.md)（etEx）·
> [`03-lasers.md`](03-lasers.md)（激光子系统）· 宿主对照 [`../th18/01-cancel-to-item.md`](../th18/01-cancel-to-item.md)

## 0. 一句话结论

**TH16 的「消弹 → 实体道具」只有一条路：季节解放炸弹。** 子炸弹传 `mode 4`（每颗弹 1 个）、主炸弹传 `mode 5`（每 3 颗 1 个），
撒出的是 **`type 0x10` = 季节道具**（实体、落地、走过去收，收 1 个季节点 `+1`）。
**其余一切消弹路径一颗实体都不出**：ECL 的 `etCancel`/`etCancel2`/`etCancelRect`/`etClear`/`etClear2`/`etClearAll`、
`laserCancel`、玩家中弹/死亡、弹自毁、boss 击破、对话开始 —— 它们走的 `mode 0` 与 `mode 1` 在本作只做
「删弹 +（mode≠0 时）计数器 `++`」。★ `mode 1` **只 inc 计数器、一颗都不撒**，这与 TH17/TH18 的 `mode 1`（撒 `type 9`）**正好相反**。

## 1. 发现（一手事实）

### 1.1 核心函数 `gen_items_from_cancel` `0x416a00`

签名：`ecx` = `float3* pos`，`edx` = `mode`，无栈参（`ret`）。本仓名 `gen_items_from_cancel`（ExpHP 非一手命名）。

```asm
416a0a  test   edx,edx
416a0c  je     0x416c15                  ; ★ mode 0 → 直接 return（连计数器都不动）
        ...场地门 4 条（§4.2），任一命中 → return...
416a70  mov    ecx,ds:0x4a6dac           ; BULLET_MANAGER_PTR
416a76  inc    DWORD PTR [ecx+0x1403b14] ; ★ 计数器 ++：唯一「消了但没出」的痕迹
416a82  cmp    edx,0x1 / je 0x416c15     ; ★★ mode 1 → 一颗都不撒
416a8b  cmp    edx,0x3 / je 0x416c15     ; mode 3 → 同上
416a94  cmp    edx,0x2 / jne 0x416b20    ; mode 2 支（本作无调用点，见 §2.2）
416b20  cmp    edx,0x5 / jne 0x416b7e    ; mode 5 支：每 3 颗 1 个
416b7e  cmp    edx,0x4 / jne 0x416c15    ; mode 4 支：每颗 1 个（+ bomb 补 1）
```

### 1.2 mode 真值表（一手，逐字节）

| mode | 撒什么 | 谁在用（一手调用点，§4.3） |
| :---: | --- | --- |
| 0 | ❌ 一颗不撒，**连计数器都不动**（`0x416a0c` 直接 return） | ECL 616/636、`clear_all` `0x416f40`、玩家中弹/死亡 `0x442628`、弹自毁 4 处 |
| **1** | ❌ 一颗不撒，只 `inc [mgr+0x1403b14]`（`0x416a85`） | **ECL 615/635/712、545 激光**、子炸弹 `0x410b14`/`0x410bf0` |
| 2 | ✅ `type 1`（力量）+ `type 0xa`（PIV），两者都受符卡位门控 | **无调用点（死支）** |
| 3 | ❌ 只 inc 计数器 | 无调用点 |
| **4** | ✅ **`type 0x10` 季节道具，每颗弹 1 个**；`bomb_is_in_use` 时再补 1 个 `8 + season_level` 的 PIV 道具 | 5 个子炸弹 `BombSub*__method_10` |
| **5** | ✅ `type 0x10` 季节道具，**每 3 颗弹才 1 个**（`[mgr+0x58] % 3 == 0`，每次调用都 `inc`） | 4 个主炸弹（Cirno / Aya / Marisa / Reimu），共 5 个调用点 |

mode 2 支的原文（`0x416a9d`–`0x416b1f`）：`[mgr+0x1403b14] % 5 == 0` 且 `!(SPELLCARD[0x4a6db0]+0x78 & 1)` 时撒
`type 1`（角度恒 `0xbfc90fdb` = −π/2、速度 `2.2f`）；`!(+0x78 & 1)` 时撒 `type 0xa`（角度 = rand×10° − π/2）。

### 1.3 撒出来的是什么 —— `type 0x10` = 季节道具

`ItemManager__on_tick_1d__body` 在 `0x42fd84` 用 `type-1` 查 16 项跳转表 `0x4300bc`（collect 分派）：

| type | 处理 | 语义 |
| --- | --- | --- |
| 1 | `0x430100 Globals__collect_power_item` | 力量道具 |
| 2 | `0x430620 Globals__collect_point_item` | 点道具 |
| 8 | `0x4303a0` | PIV 类 |
| 9–0xe | `0x42fe0d` → `0x430dc0`：`CURRENT_PIV[0x4a57d8] += float_tbl[0x4917bc+type*4] × 100`（钳 `MAXIMUM_PIV[0x4a57e0]`） | PIV 道具（表 index 9 = `5.0`） |
| **0x10** | `0x42fe58` → `0x43de50 item_collect_season` | ★ **季节道具（季节点）** |

`item_collect_season` `0x43de50`：`CURRENT_SEASON_POWER[0x4a5808] += 1`（钳 `MAX_SEASON_POWER[0x4a580c]`），
再与 `SEASON_POWER_LEVEL_REQUIREMENTS[0x4a583c]`（6 项表）比出升档 ⇒ **收 1 颗季节道具 = 季节点 +1**。

### 1.4 每颗几个 / 值多少 / 池

- **mode 4 = 每颗弹 1 个**（mode 4 支只有一处 `call 0x430960`，无计数器分流）；**mode 5 = 每 3 颗 1 个**（`[mgr+0x58]` 计数）。
- 物理上是**实体道具**，不是当场入账：`ItemManager__spawn_item` `0x430960`（`ret 0x1c`，7 个栈参
  `(type, pos, ?, angle, speed, ?, force_autocollect)`）。
- `type ∈ {9,0xa,…,0xe,0x10}` 走**独立池** `zItemManagerInner.cancel_items[0x1000]`（4096 格；普通池 `normal_items[0x258]` = 600 格），
  `item+state = 6`；**池空则 `0x430b08 je 0x430c81` 直接返回 0、什么都不产生**。
- 反溢出闸（`0x430b1a`–`0x430b99`）：按 `total_items_created` 与 `__num_cancel_items_this_frame` 分档写
  `item+0xc60 = intangibility_frames`（16–47 / 8–23 / 4–11 / 0–3）⇒ **越挤越难拿**。

### 1.5 消弹入口全集（弹幕 5 个 + 激光 2 个，与 TH18 同构）

| # | 地址 | 名（ExpHP，非一手） | 形态 | 撒不撒 |
| :---: | --- | --- | --- | --- |
| 1 | `0x416840` | `Bullet__cancel` | `(ecx=弹, 栈参 mode)`，`ret 4` | 由 mode 定 |
| 2 | `0x416c20` | `BulletManager__cancel_radius` | `(pos, mode)` + `xmm2`=半径，`ret 8`；**不查保护位** | 由 mode 定 |
| 3 | `0x416d20` | `cancel_radius_as_bomb` | 同上，但查 `[弹+0x24] == 0`（`0x416d81`） | 由 mode 定 |
| 4 | `0x416e20` | `cancel_rectangle_as_bomb` | `(pos, size, mode)` + `xmm3`=角度，`ret 0xc`；查 `[弹+0x24]` | 由 mode 定 |
| 5 | `0x416f40` | `BulletManager__clear_all` | 2000 槽循环；**栈参被忽略**，内部 `0x417036 xor edx,edx` ⇒ 硬编码 mode 0 | **永不撒** |
| 6 | `0x4319e0` | `LaserManager__ecl_545_impl` | op 545 专用；内部 `push 1`（`0x431a50`）⇒ **硬编码 mode 1** | ❌ |
| 7 | `0x431af0` / `0x431a70` | 激光全清 / 半径 | `(mode, ·)` / `(pos, A, B)` + `xmm2`，最终都进 `0x416a00` | ❌（TH16 无激光路径传 4/5） |

### 1.6 boss 击破 / 符卡结束 / 关卡切换属哪一档

- **boss 击破（终盘）**：`st0Xbs.ecl`（`thecl -d 16`）反编译 = `enmKillAll(); etClearAll(); spellEnd(); @BossItem(...)`；
  613 `etClearAll` → 弹幕 `0x416f40`（硬编码 mode 0）+ 激光 `0x431af0(1)` ⇒ **两侧都不出**。
- **符卡打掉 / 捕获**：`etCancel(640.0f)` → mode 1 ⇒ **也不出**（⚠ 与 TH18 相反：TH18 的 mode 1 = 撒 `type 9`）。
- **符卡超时**：`etClear(640.0f)` → mode 0 ⇒ 不出。
- **op 523 `spellEnd`** 的处理函数 `0x421891` **不含任何清弹调用**（清弹调用点全集里没有它）。
- **敌机死亡 / `enmKillAll`**：**引擎侧一处都不清弹**（5 个弹幕入口的调用点全集里没有敌机管理器函数）⇒ 弹幕靠自毁退出。
- **关卡切换 / 对话开始**：`GuiMsgVm__initialize` `0x429f80` → `0x416f40` + `0x431af0(1)`；ECL 518 `dialogRead` `0x4216b3` 同；
  ⇒ **清弹但不出货**。

### 1.7 消耗 RNG

- ✅ **mode 2 / 4 / 5 消耗 replay-safe RNG**：每颗弹调一次 `REPLAY_SAFE_RNG`（`0x4a6d88`）上的 `0x402cb0` 取 float，
  再 `× 0x49444c`（= 10°）得角度抖动（基准 −π/2，见 `0x416ae9`、`0x416b3e`、`0x416b91`）。
- ❌ **mode 0 / 1 / 3 不消耗**：它们在取 rand 之前就 `return`（`0x416a0c` / `0x416a85` / `0x416a8e`）。
- ⇒ 只有季节解放炸弹会推走 replay-safe RNG 流；TH16 的其它消弹**不推 RNG**。

## 2. 验证（怎么排除掉别的路）

### 2.1 清单是全集：调用点穷举 + 间接调用证否

线性扫全 `.text` 的 `call rel32`（`objdump` 全量 dump 后按目标地址计数），命中数（本轮实测）：

| 目标 | 调用点数 | 目标 | 调用点数 |
| --- | :---: | --- | :---: |
| `0x416840` `Bullet__cancel` | 7 | `0x416a00` `gen_items` | 10 |
| `0x416c20` 半径 | 2 | `0x430960` `spawn_item` | 12 |
| `0x416d20` 半径 as_bomb | 12 | `0x4319e0` 545 激光 | 1 |
| `0x416e20` 矩形 as_bomb | 4 | `0x431af0` 激光全清 | 3 |
| `0x416f40` `clear_all` | 3 | `0x431a70` 激光半径 | 7 |

**间接调用证否**（METHOD.md 第 4 条）：把这 9 个入口地址（5 弹幕 + `0x416a00` + `0x430960` + 两个激光入口）当
32 位 dword 在整个 exe 文件里搜 —— **每个地址 0 命中** ⇒ 没有函数指针表 / vtable 槽 / 回调登记，
**所有调用都是直接 `call`**，上表就是全集。每个命中都回指令边界复读了实参。

### 2.2 mode 闭包 = {0, 1, 4, 5} ⇒ `mode 2` / `mode 3` 是死支（✅ 本轮复核）

12 个 `0x416d20` 调用点、4 个 `0x416e20`、2 个 `0x416c20`、3 个 `0x431af0`、7 个 `0x431a70`、
4 处弹自毁 `0x416840` 的 mode **立即数全部读出**，取值集合 = **{0, 1, 4, 5}**；再回溯一层：
这些入口透传的 mode 来自 `Bullet__cancel` 的 `[ebp+8]` 与各激光方法的 `[ebp+0x10]`/`[ebp+0x14]`，
而激光方法的调用者（`0x431af0`/`0x431a70`/`0x4319e0`）传的也是 0 / 1。
⇒ **`mode 2`（力量+PIV 那支）与 `mode 3` 在 TH16 v1.00a 里没有任何路径能走到**；`mode 2` 的代码是活代码但是死支。

### 2.3 激光侧是另一套入口，但共用同一张 mode 表

三个激光入口最终都进同一个 `0x416a00`（由各激光类的 `LaserLine/Infinite/Curve__cancel` 在 `0x433b43`、`0x4348c9`、
`0x434ed7`、`0x435b76`、`0x436809`、`0x436e53`、`0x4399b2`、`0x43a3e5` **8 处**调用，实参 = 激光方法的 mode 形参）。
**TH16 没有任何激光调用点传 4/5**（全集 = {0,1}）⇒ 激光**永远不出货**。op 545 的专用入口 `0x4319e0` 里
`0x431a4e push 0x0` / `0x431a50 push 0x1` → `laser->vtable[0x28](mode=1, 0)`，**硬编码 1** ⇒ 只 `inc` 计数器。

### 2.4 本轮对既有记录的复核与订正（都是地址级的一手差异，结论不变）

1. **`mode 5` 的调用点是 5 个、不是 2 个**：既有记录只列 `0x40f518`（Cirno 主炸弹）与 `0x4112b9`（灵梦主炸弹）；
   本轮另读到矩形入口的 3 处 `mode 5`：`0x40eb98`（`BombMainAya__on_tick`）、`0x40ec41`（`BombMainAya__method_10`）、
   `0x410049`（`BombMainMarisa__method_10`）。⇒ **mode 5 = 全部 4 个主炸弹**（Aya 有两处），不是「魔理沙 + 灵梦」。
2. **`mode 4` 的调用点 = 5 个**（`0x40e411` `0x40eff1` `0x40f8b1` `0x4104d1` `0x4117e1`，owner 分别是
   `BombSubDoyou` / `BombSubFall` / `BombSubSummer` / `BombSubWinter` / `BombSubSpring` 的 `__method_10`）；
   既有记录在 mode 表里写「6 处」，按调用点数是 5（模式 4+5 合计 10 处）。
3. **`0x416f40` / `th17:0x41a1e0` 这一族「全清」入口的栈参其实是死的**：`0x416f40` 全函数（`0x416f40`–`0x417131`，
   `ret 4`）**不读 `[ebp+0x8]`**，mode 在 `0x417036` 硬编码为 0 ⇒ 「调用点全传 0」虽然结论对，
   机制上应写成「内部硬编码 0，传什么都一样、谁调都不出货」。
4. **owner 名（ExpHP，非一手）**：`0x40f518` 是 `BombMainCirno__method_10`、`0x410049` 是 `BombMainMarisa__method_10`
   —— 上个轮次的括注（把 `0x40f518` 记成「魔理沙」）与这条对不上，本轮以 ExpHP 名为准并在 §4.3 逐条列出。

### 2.5 照实保留的未钉死项

- 🟡 **矩形入口的几何语义**：`0x416e20` 的 `size` 参数与 `xmm3`（角度）本轮只确认「它同样走 `0x416a00`」，
  矩形几何（长宽怎么取、角度怎么用）没有逐条读。**怎么钉**：读 `0x416e20` 全函数（`0x416e20`–`0x416f3f`）的
  OBB 判定段，与 [`03-lasers.md`](03-lasers.md) 的旋转 OBB 对齐。
- 🟡 **「季节点」的显示单位**：`item_collect_season` 的 `CURRENT_SEASON_POWER += 1` 是**计数器步长**，
  与 HUD 上季节槽的显示换算没做对账（跨作同类问题：TH12/13 的 `+10`、TH14/15 的 `+200`、TH17 的 `+5` 都没换）。
  **怎么钉**：跟 HUD 的季节槽（或 `SEASON_POWER_LEVEL_REQUIREMENTS` 的 6 项阈值）对一次账。
- ❓ **`@BossItem(...)` 是不是也走 `spawn_item`**：boss 击破的脚本尾巴有它，本轮只钉到「弹幕侧一颗不出」，
  没查 `@BossItem` 走的引擎入口与它撒的 type。**怎么钉**：`thecl` 反编译 `st0Xbs.ecl` 找 `@BossItem` 的展开，
  再回 `0x430960` 的调用点全集里认领。

## 3. 结论（可信度 + 适用范围）

> **TH16 的「消弹 → 实体」只有一条路：季节解放炸弹。撒的是季节道具（`type 0x10`），子炸弹每颗弹 1 个、
> 主炸弹每 3 颗 1 个。所有 ECL 的 `etCancel`/`etClear`/`etCancel2`/`etClear2`/`etCancelRect`/`laserCancel`/`etClearAll`、
> 全部激光路径、玩家中弹与死亡、弹自毁、boss 击破、对话开始 —— 一颗实体都不出。`mode 1` 只 `inc` 计数器。**

- **可信度：✅ 高**。核心函数逐字节；5 个弹幕入口 + 2 个激光入口 + `0x416a00` 的**全部**调用点回指令边界复读；
  入口地址做过全文件 dword 搜索证明无间接调用；item 语义用 collect 分派表 + 全局字段名双向钉死。
- **范围：仅 TH16 v1.00a**（md5 `70d3429444e1e6c18a72e7200a4d7ce8`），**不可外推**。
- ★ **`mode` 号在作品之间不可移植**：TH16 的 `mode 1` = 只 inc（本页），TH17/TH18 的 `mode 1` = 撒 `type 9`；
  TH16 的 `mode 4` = 季节道具，TH17 的 `mode 4` = 什么都不撒，TH18 的 `mode 4` = 多数 `type 9` + 少数 `type 2`。

## 4. 证据（地址 / 常量 / 复现命令）

### 4.1 逐条消弹路径（出 / 不出）

| 路径 | 地址链（一手） | mode | 出实体？ |
| --- | --- | :---: | --- |
| ECL 615 `etCancel` | `0x4221a7 push 1` → `0x4221b8 call 0x416c20`；激光 `0x4221ca call 0x431a70(·,1,1)` | 1 | ❌ |
| ECL 616 `etClear` | `0x422226 push 0` → `0x422237 call 0x416c20`；激光 `0x422249`(0,1) | 0 | ❌ |
| ECL 613 `etClearAll` | `0x421656 call 0x416f40`（硬编码 0）；激光 `0x42165e call 0x431af0(1)` | 0 / 1 | ❌ / ❌ |
| ECL 518 `dialogRead` | `0x4216c9 call 0x416f40`；激光 `0x4216d1 call 0x431af0(0)` | 0 / 0 | ❌ |
| ECL 635 `etCancel2` | `0x42225c push 1` → `0x42226d call 0x416d20`；激光 `0x42227f`(1,1) | 1 | ❌ |
| ECL 636 `etClear2` | `0x422292 push 0` → `0x4222a3 call 0x416d20`；激光 `0x4222b5`(1,0) | 0 | ❌ |
| ECL 712 `etCancelRect` | `0x422209 push 1` → `0x422213 call 0x416e20` | 1 | ❌ |
| ECL 545 `laserCancel` | `0x422550 call 0x4319e0` → `laser->vtable[0x28](1, 0)` | 1 | ❌（激光） |
| **季节解放炸弹（出）** | 子炸弹 `0x40e411` `0x40eff1` `0x40f8b1` `0x4104d1` `0x4117e1`；主炸弹 `0x40f518` `0x4112b9` `0x40eb98` `0x40ec41` `0x410049` | 4 / 5 | ✅ `type 0x10` |
| 另两个子炸弹 | `0x410b14`、`0x410bf0` → `0x416d20(·, 1)` | 1 | ❌ |
| 玩家中弹 / 死亡 | `0x442628` → `0x416d20(·, 0)`；激光 `0x442669`、`0x442680`、`0x442a1b` | 0 / 0 / 1 | ❌ |
| 弹自身自毁（`Bullet__run_ex`） | `0x413df0` `0x4141d0` `0x414a28` `0x414bf9`，四处 `push 0` | 0 | ❌ |
| 对话开始 | `0x429f80`（`GuiMsgVm__initialize`）→ `0x416f40` + `0x431af0(1)` | 0 / 1 | ❌ |

### 4.2 场地门常量真值（核心函数头部，任一条命中即 `return`，**连计数器都不加**）

| 判据 | 常量地址 | 真值 | 源码指令 |
| --- | --- | --- | --- |
| `x + 32 ≤ −192` | `0x4945fc` / `0x49476c` | `32.0f` / `−192.0f` | `0x416a16` / `0x416a29` |
| `x − 32 ≥ 192`（即 `x ≥ 224`） | `0x494660` | `192.0f` | `0x416a3a` |
| `y + 32 ≤ 0`（即 `y ≤ −32`） | （`xorps` 0.0f） | `0.0f` | `0x416a56` |
| `y − 32 ≥ 448`（即 `y ≥ 480`） | `0x4946b0` | `448.0f` | `0x416a63` |

### 4.3 复现命令（只读，不改仓库）

```bash
cd /home/nipporita/renkolab
md5sum local/th16.v1.00a/th16.exe         # 70d3429444e1e6c18a72e7200a4d7ce8
objdump -d -M intel local/th16.v1.00a/th16.exe > /tmp/can/th16.asm      # 163349 行
objdump -d -M intel --start-address=0x416a00 --stop-address=0x416c20 local/th16.v1.00a/th16.exe
# 调用点计数（全集）：grep -cE 'call[[:space:]]+0x416f40$' /tmp/can/th16.asm   → 3
# 逐条实参：对每个调用点用 objdump --start-address 回读前 10 条指令的 push 立即数
```

一手脚本侧证据：`thecl -d 16 -m local/vendor/eclmap/eclmap/th16.eclm local/th16.v1.00a/dat/st01bs.ecl`
（`etCancel` / `etClear` / `etClearAll` 的排布）。ECL 派发表（**本轮机械复原**，与 TH17 是同一套编号）：
`opcode − 300` 查 byte 表 `0x422d70`（值 = 指针表索引）→ 查指针表 `0x422ab8` —— 613 → `0x421655`、615 → `0x42219e`、
616 → `0x42221d`、635 → `0x422253`、636 → `0x422289`、712 → `0x4221d4`、545 → `0x422550`。

## 5. 对卡的直接影响（只读观察，不改代码）

现状来自 `mods/th18.v1.00a/card-expand/native/cards/season.c`（`grep -n "ce_cancel\|cancel" season.c`，2026-09-15 实读）。

1. **`season.c:1123` 传 `mode 4` ⇒ 号对、义不对（已成文为「刻意耦合」）**：`ce_cancel_radius(pos, s->rel_radius, SEASON_CANCEL_MAX, 4)`
   在 TH18 宿主里 = `th18:0x429370`，`mode 4` = 多数 `type 9` + 少数 `type 2`（钱）；
   而 TH16 的 `mode 4` = **季节道具 `type 0x10`**，**TH18 里没有 `type 0x10` 这个道具** ⇒ 无 1:1 可能。
   卡改成「引擎撒钱 + 卡自撒季节点」，注释已如实声明为**刻意的耦合、不是复刻** —— 与一手**不冲突**。
2. **比例已对齐 TH16 ✅**：`SEASON_CANCEL_PER_POINT`（`season.c:72`）**已由 8 改成 1**，即每 1 颗消掉的弹出 1 个季节点，
   与 TH16 `mode 4` 的「每颗弹 1 个」一致。
3. **❓ 自定项**：`SEASON_CANCEL_POINT_MAX 4`（`season.c:77`）每帧最多 4 点 —— TH16 **没有**这个闸
   （mode 4 每颗弹都撒，只受 4096 格的独立池与反溢出闸约束）。属自定，需留在 `TUNING.md` 的口径里。
4. **主/子炸弹两档**：TH16 的**主炸弹是 `mode 5`（每 3 颗 1 个）**，而 `season.c` 里只有一处消弹调用（`1123`），
   没有 mode 5 那一档。若要补「主炸弹 = 每 3 颗 1 点」：TH18 的 `mode 5` 在 `th18:0x4290e0` 里**只 inc 不撒**
   ⇒ 引擎替不了，只能卡自己在每帧上限内按 1/3 比例撒。
5. **一句话**：卡的消弹行为 = 「TH18 引擎撒钱 + 卡自撒季节点」的耦合；**比例已复刻 TH16（每颗 1 点）**，
   每帧上限是 ❓ 自定项。本文只做只读登记，未改任何代码。
