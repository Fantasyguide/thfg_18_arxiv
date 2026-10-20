# 01 — TH18 激光子系统（EX_LASER / LaserManager）

> **版本**：TH18 v1.00a（`th18.exe`，imagebase `0x400000`，md5 `9969cac756098c1da05a81de45437a70`，847360 B）。
> 本文裸地址默认属该版；引别作写 `th16:0x…`。
> **状态**：★ **A 部分（分配器 / manager / type 枚举）已一手**；★ **B 部分（spec 与路径节点）进行中**。
> 对照：[`../th16/03-lasers.md`](../th16/03-lasers.md)（TH16 同族，5 个类同名）。

## 1. 类（RTTI 名直接来自 exe）

```
.?AVLaserLineInf@@  .?AVLaserInfiniteInf@@  .?AVLaserCurveInf@@  .?AVLaserBeamInf@@  .?AVLaserDataInf@@
```

| 类 | TypeDescriptor | COL | vtable | 对象大小（`operator_new`） |
| --- | --- | --- | --- | ---: |
| `LaserLineInf` | `0x4ce684` | `0x4c047c` | `0x4b687c` | `0x1e0c` |
| `LaserInfiniteInf` | `0x4ce648` | `0x4c0454` | `0x4b680c` | `0x1824` |
| ★ `LaserCurveInf` | `0x4ce668` | `0x4c0468` | ★ `0x4b672c` | ★ `0x1844` |
| `LaserBeamInf` | `0x4ce62c` | `0x4c0440` | `0x4b679c` | `0x21f4` |
| `LaserDataInf`（基类） | `0x4ce6a0` | `0x4c0490` | `0x4b68ec` | — |

★ 对象大小由分配器里 `push <size>; call 0x48dc71`（= `operator new`）**实读**，不是猜。

## 2. ★★ 分配器：`LaserManager__allocate_new_laser`（`0x448920`，1306 B）

```
undefined4 __stdcall LaserManager__allocate_new_laser(int type, int *spec)   // ret 8
```

| 步骤 | 一手 |
| --- | --- |
| 取 manager | `0x448925  mov edi, ds:0x4cf3f4` ⇒ ★ **LaserManager\* 在 `0x4cf3f4`** |
| 上限 | `0x44892b  cmp [edi+0x798], 0x200` ⇒ ★ **最多 512 条**；满了 `return 0` |
| 序号 | `0x448940  inc [edi+0x79c]`（钳到 ≥`0x10000`）⇒ 返回值就是它 |
| type 分派 | `0x44895f  cmp eax,3 / ja default` ⇒ `0x44896a  jmp [eax*4+0x448e3c]` |
| 建对象 | 每个分支 `push <size>; call 0x48dc71`（`operator new`）→ 调基类 ctor `0x448170`（Line 例外：`0x4482c0`）→ 写 vtable |
| 入链 | `[eax+0x04] = [edi+0x794]`（前一条）、`[ecx+0x08] = eax`、`[edi+0x794] = eax`（★ 链尾）、`inc [edi+0x798]` |
| 初始化 | `push spec` 后 `call [vtable+0xc]`（★ **spec 由各类的 `vtable[3]` 消费**）；Line 分支另有 `[eax+0x80] = 序号` |
| 返回 | ★ **返回序号**（`[edi+0x79c]`），**不是指针** |

### 2.1 type 枚举（跳转表 `0x448e3c` 直读）

| `type` | 分支 | 类 | 对象大小 |
| ---: | --- | --- | ---: |
| `0` | `0x448971` | `LaserLineInf` | `0x1e0c` |
| `1` | `0x4489c5` | `LaserInfiniteInf` | `0x1824` |
| ★ `2` | `0x448c86` | ★ **`LaserCurveInf`（写 vtable `0x4b672c`）** | `0x1844` |
| `3` | `0x448b70` | `LaserBeamInf`（写 vtable `0x4b679c`） | `0x21f4` |

## 3. Manager 字段（已一手）

| 偏移 | 含义 |
| --- | ---: |
| `+0x14` | ★ 节点链头（`destroy_all 0x442480`：`mov esi,[ebx+0x14]`；节点 `+0x00`=vtable、`+0x08`=next） |
| `+0x794` | ★ 链尾（分配器写） |
| `+0x798` | ★ 当前条数（上限 `0x200`） |
| `+0x79c` | ★ 序号计数器（分配器的返回值） |
| `+0x7a0`／`+0x7a8` | 工作向量（`cancel_in_radius 0x449010` 写） |

## 4. 调用者（`getReferencesTo(0x448920)`，Ghidra 一手）

`0x427270`（在 **`Bullet__run_ex` `0x425c60`** 内 ⇒ ★ 子弹 VM 的 EX_LASER 路径）、
`0x449f40`／`0x44a00b`／`0x44a0d9`／`0x44a1ac`（激光类自身方法区）、`0x451c5b`、
★ **`0x435273`／`0x43545f`／`0x4355cb`／`0x435756`（ECL 区 `0x435xxx`，四处）**。

## 5. ★ spec 是怎么来的（一手，ECL 侧调用点）

ECL 区 `0x435xxx` 的调用点（`0x435273` 那处）逐字：

```
435251  lea  eax,[ebp-0x548]      ; ★ &spec（栈上）
435268  push eax                  ; param_2 = &spec
435269  push 0x0                  ; param_1 = type = 0（Line）
435273  call 0x448920             ; allocate_new_laser(0, &spec)
```

★ spec **不是** ECL 参数直接传的，而是**从单位/子弹对象拷**：
`[edi+esi+0x5a0]`／`+0x5a4`／`+0x5a8`／`+0x5b4`／`+0x5bc`／`+0x9e0`／`+0xa10`／`+0xa14`
分别落到 spec 的 `+0x10`／`+0x14`／`+0x18`／`+0x1c`／`+0x20`／`+0x2c`／`+0x458`／`+0x45c`（相对 `&spec`）。
⇒ ★ **spec ≈ 0x460 字节**，字段像是 (x, y, z, angle, speed, length, width, …)。

★ 另一条线索（更省事）：`Bullet__run_ex` 里那处调用先用
`rep movs` 从 **`[manager+0x69c]`** 拷 **`0x108` dwords（0x420 字节）** 到栈上 spec，再补几个字段
⇒ ★ **`manager+0x69c` 很可能就是"默认 spec 模板"** —— 卡侧要桥，可以"抄模板 + 改几个字段"，不必从零构造。

## 6. ★★ Curve 的 `vtable+0xc`（`0x44ed70`）—— spec 直接整块拷进对象

```
44ed7c  ecx = 0x118                 ; ★ 0x118 dwords = 0x460 字节
44ed82  esi = [ebp+8]               ; ★ spec 指针
44ed86  edi = ebx+0x788             ; ★ 目标 = 激光对象 +0x788
44ed8c  rep movs                    ; ★ spec 整块拷进 laser+0x788
44ed8e  eax = [ebx+0x7a0]           ; = spec+0x18
44ed9a  [ebx+0x780] = eax           ; （另一个独立字段）
44eda8  [ebx+0x10] = 2 ; [ebx+0x14] = 2
44edbc  call 0x41b430               ; 段/节点数组初始化（esi = laser+0xbe8）
44edcc  cmp [ebx+0x780], 1          ; ★ spec+0x18 == 1 ?
44edd1     ecx=[mgr+0x7b8] ; eax=0x142          → call 0x477d60(esi=laser+0xbe8, eax)
44edee     ecx=[mgr+0x7b8] ; eax=(spec+0x18)+0x8e → call 0x477d60(...)
```

⇒ ★★ **spec 的布局就是 `laser+0x788` 起的那 0x460 字节**（同一份内存）。
⇒ ★ **`spec+0x18` 是"激光外观/类型 id"**：等于 1 时取 sprite `0x142`，否则取 `spec+0x18 + 0x8e`
   ⇒ 与"激光精灵来自 `bullet.anm` 预载表 `[mgr+0x7b8]`"这一条吻合（TH16 是 `mgr+0x604`，TH18 是 **`mgr+0x7b8`**）。
⇒ ★ `0x41b430(laser+0xbe8)` 就是**段/节点数组的初始化** —— 下一轮读它得段结构。

## 7. 待补（下一轮）

1. ★ **`spec` 逐字段语义** —— 已知 `+0x18` = 外观/类型 id（决定 sprite id）；其余偏移待读 `0x41b430` 与绘制路径。
2. ★ **Curve 的路径怎么表达**（节点数组？折线？逐帧追加？）—— 决定"能不能从卡侧构造路径"。
3. ★ 前置条件：`ds:0x4cf3f4` 由谁初始化（`on_tick 0x4488c4` 里 `ds:0x4cf2a4` 为 0 时 `call 0x448760` 是嫌疑）、
   精灵预载（TH16 是 `LaserManager::initialize` → `mgr+0x604`，TH18 位置待查）。

## 8. 可行性判定（★ 2026-10-21，基于 A/B 的已知部分）

★ **结论：可桥，但有一处硬前置 —— 必须自己填一份 `spec`（0x460 字节）。**

| 问题 | 一手答案 |
| --- | --- |
| ★ 有统一入口吗 | ★ **有**：`LaserManager__allocate_new_laser(type, spec)`（`0x448920`，stdcall，ret 8） |
| ★ 能指定曲线吗 | ★ **能**：`type = 2` ⇒ `LaserCurveInf`（对象 `0x1844`，vtable `0x4b672c`） |
| ★ 拿到什么 | ★ **序号**（不是指针）⇒ 要操作它得按序号去 manager 链里找，或直接用 vtable 方法 |
| ★ manager 要先初始化吗 | ★ 需要：`ds:0x4cf3f4` 非空（`on_tick 0x4488c4` 里 `ds:0x4cf2a4==0` 时 `call 0x448760` 是初始化嫌疑，待验） |
| ★ 精灵要预载吗 | ★ **不需要自己预载**：精灵表在 `[mgr+0x7b8]`（TH16 是 `mgr+0x604`），由引擎 `initialize` 灌好 |
| ★ tick 会自己跑吗 | ★ **会**：激光对象入 manager 链（`[mgr+0x794]` 链尾 / `+0x14` 链头），由 `LaserManager__on_tick 0x4488c4` 每帧推进 ⇒ 卡侧**不用**自己 tick |
| ★ 判定/擦弹是原生的吗 | ★ **是**：`LaserCurve__check_graze_or_kill 0x44fa53` 等由引擎调用 |
| ★ 硬前置 | ★ **必须构造一份合法的 0x460 字节 spec**（卡侧没有 bullet 单位，不能像 ECL 那样从单位字段拷）；★ 模板在 `mgr+0x69c`（`0x420` 字节）可抄，但**仍需知道要改哪几个字段** |

★ 于是"on_activate 里调一次能不能当场看见一条曲线激光"的答案：
★ **能，前提是先有一份能用的 spec** —— 最小可行路径 = **抄一份现成激光的 spec**（例如让引擎先造一条 ECL 激光，或在 ECL 里造一条后把它的 `laser+0x788` 那 0x460 字节读出来当模板），改 `type/sprite/路径/位置/寿命` 后调用分配器。

★ 风险与回归（若 owner 点头做桥）：见 [`06-...`]（下一轮补：改哪几处、`make anm`／`make check`／四 DLL 同 md5、判定/擦弹变原生对其它卡的影响）。
