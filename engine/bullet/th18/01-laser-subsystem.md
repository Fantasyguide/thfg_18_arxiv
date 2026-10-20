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

## 5. 待补（下一轮）

1. ★ **`spec` 的字段布局** —— 由各类 `vtable+0xc` 消费；Curve 是 `0x44ed70`，读它才知道要填什么。
2. ★ **Curve 的路径怎么表达**（节点数组？折线？逐帧追加？）—— 决定"能不能从卡侧构造路径"。
3. ★ 前置条件：`ds:0x4cf3f4` 由谁初始化（`on_tick 0x4488c4` 里 `ds:0x4cf2a4` 为 0 时 `call 0x448760` 是嫌疑）、
   精灵预载（TH16 是 `LaserManager::initialize` → `mgr+0x604`，TH18 位置待查）。
