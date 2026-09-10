# TH06NC 敌人子系统：池与生成

> **版本**：跨版本。本文出现的地址一律带前缀（`th06nc:0x1400360d0` / `th06:0x00411390`）。
> 结构体偏移不是地址，按 `+0xb0` 这样写。

状态：池的三个量 ✅ 一手（立即数直接读出）；与 **TH06 原版二进制**逐条对照过。
本轮最重要的一条是 §3：**NC 给 `SpawnEnemy` 加了一个 TH06 没有的参数**。

> opcode 交叉比对得到的那 15 个 `Enemy` 字段在
> [`../../ecl/th06nc/04`](../../ecl/th06nc/04-enemy-struct.md)——那才是它们的出处。
> 本文只放这一轮从**生成路径**读出来的东西。

---

## 1. 池

`EnemyManager__SpawnEnemy` `th06nc:0x1400360d0` 的找空闲槽循环，三个立即数：

```
1400360f4  LEA RBX,[RCX + 0x8]              ; enemies = manager + 0x08
140036100  CMP byte ptr [RBX + 0xbc],R12B   ; flags；符号位(bit7) = 该槽被占用
140036107  JGE 找到                          ; 有符号 >= 0 即空闲
14003610b  ADD RBX,0x10b0                   ; sizeof(Enemy)
140036112  CMP EAX,0x100                    ; 256 个
14003611e  LEA RDX,[RCX + 0x10c130]         ; 模板
140036132  MOV R8D,0x10b0                   ; memcpy(newEnemy, template, 0x10b0)
```

**这关掉了 [`../../ecl/th06nc/04`](../../ecl/th06nc/04-enemy-struct.md) §5 的第 3 条 ❓**
（结构体总大小）。

### 布局与已有结论自洽

`0x08 + 256 × 0x10b0 = 0x10b008`，模板在 `+0x10c130`，中间空出 `0x1128`。
那段空档里正好装着两处早先独立定位的东西：

| 地址 | 出处 |
| --- | --- |
| `+0x10c0d0` / `+0x10c0d4` | `spellcardInfo.isActive` / `.usedBomb`（[`../../player/th06nc/01`](../../player/th06nc/01-main-loop-and-collision.md) §2.16） |
| `+0x10c128` | timeline 指针（[`../../ecl/th06nc/04`](../../ecl/th06nc/04-enemy-struct.md)） |

已写成 `validate()` 的断言。

## 2. 与 TH06 的对照（一手读过原版二进制）

`th06:0x00411390`（290 字节）逐条读过，不是只看 CC0 源码：

| | TH06 | TH06NC |
| --- | --- | --- |
| `enemies[]` 位置 | `manager + 0xed0` | **`manager + 0x08`** |
| 模板位置 | `manager + 0x08` | **`manager + 0x10c130`** |
| `sizeof(Enemy)` | `0xec8` | **`0x10b0`** |
| 数量 | 256 | **256（没变）** |
| 占用位 | `enemy+0xe50` 的 bit7（`SHR CL,7; AND CL,1`） | `enemy+0xbc` 的 bit7（符号位） |
| 复制方式 | `MOVSD.REP`，`ECX = 0x3b2` 个 dword = `0xec8` | `memcpy`，`R8D = 0x10b0` |

**两者把池和模板的位置对调了**，容量与占用位语义都没变。

## 3. ★ NC 给 `SpawnEnemy` 加了第 7 个参数

CC0 的签名是 5 个参数：

```cpp
Enemy *EnemyManager::SpawnEnemy(i32 eclSubId, D3DXVECTOR3 *pos, i16 life, i16 itemDrop, i32 score)
```

原版二进制以 **`RET 0x14`** 收尾——20 字节栈参，正好 5 个，**不可能有第 6、7 个** ✅。

NC 的版本多一个：

```
140036165  TEST RCX,RCX          ; RCX = 第 7 实参（[RSP+0x70]）
140036168  JZ 跳过
14003616a  MOV EAX,[RCX + 0x08]  ; -> [enemy + 0x48]
   …          14 个 dword          …
1400361b8  MOV EAX,[RCX + 0x3c]  ; -> [enemy + 0x7c]
```

### 这个参数是什么

调用点有 7 处。**6 处 timeline 传 0**，只有 ECL VM 那处传了非零：

```
14002854c  MOV qword ptr [RSP + 0x30], R13   ; 第 7 实参
14002856a  CALL EnemyManager__SpawnEnemy
```

而 `EclManager__RunEcl` 里 `th06nc:0x1400220db` 是 `LEA R13,[RDX + 0x40]`，
`RDX` 就是 `enemy` 参数。所以**第 7 实参 = 父敌人的 `EclContext`**（在 `enemy+0x40`）。

于是拷贝的源是 `父 context + 0x08`、目标是 `子 enemy + 0x48` = `子 context + 0x08`——
**同一个偏移，整块 14 个 dword**。

### 那 14 个 dword 是 ECL 变量

NC 的 `EnemyEclContext` 在 `Enemy + 0x40`，三处独立命中（见 §4）确定了它的边界：
`+0x00` 是 `ZunTimer`、`+0x40` 是 `subId`、`+0x48` 是 `currentInstr`。
夹在中间的 `+0x08`–`+0x3c` 正是 CC0 里的
`var0-3` / `float0-3` / `var4-7` / `compareRegister` 那一段。

> **结论：NC 的 `ENEMYCREATE` 让子敌人继承父敌人的 ECL 变量；TH06 不会。**
> 🟡 **未实机验证**，也没查证社区是否已知。推理链是完整的一手，
> 但「这在游戏里表现为什么」本文不下结论。

## 4. NC 的 `EnemyEclContext`（在 `Enemy + 0x40`）

| 偏移 | 字段 | 证据 |
| --- | --- | --- |
| `+0x00` / `+0x04` | `time.previous` / `.current` | `SpawnEnemy` 写 `-999` / `0` |
| `+0x08`–`+0x3c` | 14 个 dword 变量区 | §3 的拷贝 |
| `+0x40` | `subId` | `SpawnEnemy` 写 `word [enemy+0x80]` |
| `+0x48` | `currentInstr` | `SpawnEnemy` 写 `qword [enemy+0x88]`，取自子表指针数组 `th06nc:0x140a6eb80[subId]` |

三个偏移换算回 `Enemy` 分别是 `+0x40/44`、`+0x80`、`+0x88`——
与 CC0 的 `EnemyEclContext`（`0x4c`，x86-32）按 x64 指针与 `ZunTimer` 宽度换算后一致 ✅。

（`ZunTimer` 在这里又是 8 字节——第八次独立印证，见
[`../../player/th06nc/01`](../../player/th06nc/01-main-loop-and-collision.md) §2.13。）

## 5. `SpawnEnemy` 写到的字段

与 CC0 `EnemyManager.cpp:92-122` 逐条对照：

| 偏移 | 字段 | 备注 |
| --- | --- | --- |
| `+0x0b0` | `position` | `MOVSD` 拷 x/y，z 单独一条 |
| `+0x1f4` | `primaryVm.color` | 下一条的源 |
| `+0x234` | `life` | `life >= 0` 时写；两边都写两次 |
| `+0x238` | `maxLife` | **从 weak 升为 code** —— 原先只有 opcode 111 一个锚 |
| `+0x23c` | `unk_23c` ❓ | 紧接着又写了一份同样的 `life`。**TH06 只写一次**（`th06:0x004114a1`） |
| `+0xc50` | `score` | `score >= 0` 时写 |
| `+0xf74` | `color` | `= [enemy+0x1f4]`，对应 CC0 的 `color = primaryVm.color` |
| `+0x10a8` | `itemDrop` | byte |

## 6. 未解与待办

1. ❓ `+0x23c` 是什么：NC 比 TH06 多写的那一份 `life`。
2. ❓ `primaryVm` 的基址：只知道它的 `color` 在 `+0x1f4`（TH06 是 `primaryVm` 在 `enemy+0`、
   `color` 在 `+0x7c`）。要定它得先确认 NC 的 `AnmVm.color` 偏移。
3. ❓ `EnemyManager` 的绝对基址本轮未定位——`SpawnEnemy` 只用 `this` 指针。
4. — **仍不在 Ghidra 里建 `Th06ncEnemy` 结构体**。总大小已知了（`0x10b0`），
   但已定位字段仍只有 20 出头；现在建出来会把所有敌人相关的反编译输出改一遍，
   收益不抵风险。等字段数上去再说。
5. — **归属待办**：`Enemy` 的字段表现在分在两处
   （[`../../ecl/th06nc/04`](../../ecl/th06nc/04-enemy-struct.md) 与本文）。
   合并成一张表是一次独立的整理，本轮不做——那 15 个字段确实来自 ECL 交叉比对，
   出处放在那边并不算错。

## 7. 复现

```sh
# MCP：open_database 指向 local/th06nc/th06nc.exe（会复用我们的工程）
#   disassemble_function EnemyManager__SpawnEnemy
#   get_xrefs_to EnemyManager__SpawnEnemy
python3 -c "import sys; sys.path.insert(0,'tooling/ghidra'); \
  import th06nc_enemy_spec as s; print(s.validate())"
```
