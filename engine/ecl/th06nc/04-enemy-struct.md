# TH06NC 的 Enemy 结构体：已定位的字段

> **版本**：跨版本。本文出现的地址一律带版本前缀（`th06nc:0x140022060`）。
> 结构体偏移不是地址，按 `+0xb0` 这样写。

状态：**结构体已被 NC 重排** ✅ 一手证实——不能从 TH06 的字段顺序推 NC 的。
已定位 15 个 `Enemy` 字段 + 4 个 `EnemyBulletShooter` 字段，**置信度分三档**，见 §3。

---

## 1. 最重要的一条：NC 重排了 Enemy

CC0 的 TH06 `Enemy`（`th06-decomp/src/Enemy.hpp`，`ZUN_ASSERT_SIZE(Enemy, 0xec8)`）
把 9 个 `AnmVm`（`primaryVm` + `vms[8]`）放在**最前面**，`position` 排在很后面。

NC 不是这样：

| 字段 | TH06 顺序 | NC 偏移 |
| --- | --- | --- |
| `position` | 9 个 AnmVm + 两组 EclContext 之后 | **`+0xb0`** |
| `bulletProps` | 更靠后（`color` 之后） | **`+0x26c`** |
| `shootOffset` | 在 `bulletProps` **之前** | **`+0xc5c`**（在它**之后**） |

**顺序整个变了。** 这和 [`02`](02-th06-baseline-comparison.md) / [`03`](03-opcode-crossmatch.md)
的结论并不冲突——那两篇证明的是**指令语义**没变；结构体布局是另一回事，NC 重排了它。

> ⚠️ 所以做 NC 的敌人子系统时，**TH06 的结构体只能提供「有哪些字段」，
> 不能提供「字段在哪」**。每个偏移都要自己验。

## 2. 怎么验的（以 `bulletProps` 为例）

一开始我按 TH06 的顺序推断，发现 `bulletProps +0x26c` 排在 `shootOffset +0xc5c` 前面，
与 TH06 矛盾，于是回去查——结论是**偏移没错，是顺序假设错了**：

```
140025374  LEA  R12,[RDI + 0x26c]        ; RDI = enemy（RunEcl 的第二个参数，MOV RDI,RDX）
...
140025d63  MOV  RDX,R12                  ; 作为第二个实参
140025d66  LEA  RCX,[0x1403ec2a0]        ; g_BulletManager
140025d6d  CALL 0x14000f870              ; SpawnBulletPattern(mgr, bulletProps)
```

R12 被**当作 `bulletProps` 实参传进去**了，所以 `enemy + 0x26c` 就是它 ✅。
另一处旁证：

```
1400269c7  ADDSS XMM0,[RDI + 0xc5c]      ; + shootOffset.x
1400269df  MOVSS [RDI + 0x278],XMM0      ; -> bulletProps.position.x  (0x26c + 0x0c)
```

即 TH06 的 `bulletProps.position = enemy->position + enemy->shootOffset`，两边语义一致。

## 3. 已定位的字段

置信度三档：**code** = 直接从指令读出；**xmatch** = opcode 交叉比对且有 ≥2 个 opcode 支撑；
**weak** = 只有 1 个 opcode 支撑，**只算线索**。

### `Enemy`

| 偏移 | 字段（CC0 名） | 来源 | 证据 |
| --- | --- | :---: | --- |
| `+0xa0` | `lifeCallbackThreshold` | weak | opcode 113 |
| `+0xa4` | `lifeCallbackSub` | weak | opcode 114 |
| `+0xb0` | `position` | **code** | `[RDI+0xb0/0xb4/0xb8]` 三个 f32 |
| `+0xbc` | `flags` | **code** | `TEST byte ptr [RDI+0xbc],0x20` |
| `+0xc0` | `laserProps` | xmatch | opcodes 85, 86；起一片连续偏移 |
| `+0x230` | `bossId` | weak | opcode 101 |
| `+0x238` | `maxLife` | **code** | `SpawnEnemy` 里 `MOV [enemy+0x238],[enemy+0x234]`（[`../../enemy/th06nc/01`](../../enemy/th06nc/01-pool-and-spawn.md) §5）|
| `+0x26c` | `bulletProps` | **code** | 见 §2 |
| `+0x2c0` | `hitboxDimensions` | weak | opcode 103；`+0x2c0/0x2c4/0x2c8` |
| `+0xbd8` | `laserStore` | xmatch | opcodes 85, 86, 87 |
| `+0xbe0` | `deathCallbackSub` | xmatch | opcodes 108, 133 |
| `+0xc54` | `timerCallbackThreshold` | weak | opcode 115 |
| `+0xc58` | `timerCallbackSub` | xmatch | opcodes 116, 133 |
| `+0xc5c` | `shootOffset` | **code** | 见 §2 |
| `+0xf70` | `runInterrupt` | weak | opcode 110 |

结构体总大小**未知**。`+0xf70` 是目前见过的最大偏移，所以至少 `0xf74`
（TH06 是 `0xec8`）。

### `EnemyBulletShooter`（相对 `bulletProps` 基址）

**这个也重排了**：TH06 是 `sprite +0x00` / `aimMode +0x48`，NC 差得远。

| 偏移 | 字段 | 来源 | 证据 |
| --- | --- | :---: | --- |
| `+0x0c` | `position` | code | 写 `+0x0c/0x10/0x14` |
| `+0x18` | `count1` | code | `MOV word ptr [R12+0x18]`，rank 缩放后写入 |
| `+0x2c` | `sprite` | code | 指令首个 u16 参数 |
| `+0x30` | `aimMode` | code | `SUB AX,0x43` 的结果；消费者 switch 的变量 |

`aimMode +0x30` 与消费侧 `th06nc:0x14000f870` 的 `switch((ushort)param_2[0xc])`
（`0xc × 4 = 0x30`）互相印证 ✅。

## 4. 方法：字段级交叉比对的产出**远低于**函数级

同样是拿 opcode 当坐标系（[`03`](03-opcode-crossmatch.md) 的手法），
函数级配出 16 个，字段级只配出 **4 个**（≥2 opcode 支撑的）。原因是结构性的：

- **字段访问密度高得多**。一个 handler 动辄读写十几个偏移，
  而 handler 区间是按「下一个 handler 起点」近似切的，噪音被放大。
- **大集合字段配不上**。CC0 里 `flags` 被 44 个 opcode 碰、`position` 24 个，
  但 NC 侧没有任何单个偏移的 opcode 集合与之精确相等——
  因为内联和区间误差都会改变集合。
- **单 opcode 的匹配不可靠**：一个 opcode 碰多个字段时它们键相同，
  双向唯一性会把它们全丢掉（刻意如此，宁漏不错）。表里 7 条 weak 就是放宽后捡回来的，
  **标着 weak 就是提醒别拿它当结论**。

## 5. 下一步

1. ❓ **把 handler 区间改成跟随控制流**而不是按地址切。这是提升字段级召回的关键，
   对函数级也有帮助（[`03`](03-opcode-crossmatch.md) §7 同一条）。
2. ❓ **从 `EnemyEclInstr__*` 那 16 个函数入手**：它们参数少、职责单一，
   逐个读比在 27928 字节的 VM 里切区间可靠得多。
   例如 `EnemyEclInstr__MovePosTime` 只碰 move 相关的几个字段。
3. ✅ ~~结构体总大小~~ —— **已解**：`0x10b0`（TH06 是 `0xec8`），
   从 `EnemyManager__SpawnEnemy` 的找空闲槽循环直接读出。
   见 [`../../enemy/th06nc/01`](../../enemy/th06nc/01-pool-and-spawn.md) §1。
4. — **仍不在 Ghidra 里建 `Th06ncEnemy` 结构体**：总大小已经知道了，
   但已定位字段仍只有 20 出头，现在建出来会把敌人相关的反编译输出全改一遍，
   收益不抵风险。理由与去留见
   [`../../enemy/th06nc/01`](../../enemy/th06nc/01-pool-and-spawn.md) §6。
5. — 本文这 15 个字段来自 opcode 交叉比对；**从生成路径读出来的另一批**在
   [`../../enemy/th06nc/01`](../../enemy/th06nc/01-pool-and-spawn.md)。
   两张表合并是一次独立的整理，尚未做。

## 6. 复现

```sh
python3 -c "import sys; sys.path.insert(0,'tooling/ghidra'); \
  import th06nc_ecl_spec as s; print(s.ENEMY_FIELDS); print(s.BULLET_PROPS_FIELDS)"
tooling/ghidra/serve.sh th06nc 6092    # 0x140022060 = ECL VM
```
