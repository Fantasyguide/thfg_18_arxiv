# 用 ECL opcode 当坐标系，把 TH06 的子系统函数名迁到 TH06NC

> **版本**：跨版本。本文出现的地址一律带版本前缀（`th06:0x4074a0` / `th06nc:0x14002e260`）。

状态：方法 ✅ 有自证；16 条结果里 14 条 exact、2 条 near（人工判过）。
**召回率天然偏低**，原因见 §4——配上的可信，配不上不代表两边不同。

前提是 [`02`](02-th06-baseline-comparison.md) 已经证明的：两边 ECL 指令集逐项一致。
既然每条 opcode 的语义相同，它的 handler 去碰的子系统也该相同——
于是 **opcode 号就是两个二进制之间的共同坐标系**。

---

## 1. 方法

```
CC0 源码 (th06-decomp)          NC 二进制
   每个 opcode 调哪些函数    ←→    每个 handler 调哪些地址
            ↘  按 opcode 集合双向唯一匹配  ↙
                   NC 地址 ← CC0 名字
```

三个刻意的选择：

- **源头用 CC0，不用有地址的那两个**。`th-re-data`（ExpHP）和 `th06-re` 的 XML 都**没有
  LICENSE**，它们的名字不能转发。CC0 的 [`th06-decomp`](https://github.com/GensokyoClub/th06)
  的 `EclManager.cpp` 直接写明了每个 `case ECL_OPCODE_*` 调用什么，够用且干净。
- **匹配键剔除 NC 独有的 opcode**（200/201），否则 TH06 侧永远配不上。
- **要求双向唯一**：某个 opcode 集合在两边各自只对应一个函数才算数。
  只查单边会出事——第一版只查了 TH06 侧唯一，结果 6 个 NC 函数同时配到
  `Gui::ShowSpellcard`（它们共享同一个单元素键 `{93}`）。

## 2. 自证

`th06nc:0x1400360d0` 我们在更早的一轮里**独立命名**为 `EnemyManager__SpawnEnemy`。
本次匹配在不知情的情况下（它只看 opcode 集合 `{95}`）把它配到了 CC0 的
`EnemyManager::SpawnEnemy`。**方法在一个已知答案上复现了正确结果** ✅。

三方还有一次交叉验证：CC0 源码解出的 opcode 集合与 **TH06 二进制**解出的逐项相同——
`GetVarFloat` 26 项、`GetVar` 25 项、`SetVar` 11 项、`DebugPrint2` `{36, 99}`……
源码、原版二进制、重制版二进制三边闭合。

## 3. 结果：16 个函数

| NC 地址 | 名字（CC0 写法展平） | 支撑 opcode | 置信度 |
| --- | --- | --- | :---: |
| `th06nc:0x14002e260` | `EnemyEclInstr__GetVarFloat` | 26 个 | exact |
| `th06nc:0x14002e620` | `EnemyEclInstr__SetVar` | 10 个 | **near** |
| `th06nc:0x14002e0a0` | `EnemyEclInstr__MovePosTime` | 56–60 | exact |
| `th06nc:0x14002dfc0` | `EnemyEclInstr__MoveDirTime` | 52–55 | exact |
| `th06nc:0x14002e180` | `EnemyEclInstr__MoveTime` | 61–64 | exact |
| `th06nc:0x14002ecf0` | `EnemyEclInstr__MathAdd` | 13, 20 | exact |
| `th06nc:0x14002f770` | `EnemyEclInstr__MathSub` | 14, 21 | exact |
| `th06nc:0x1400301f0` | `EnemyEclInstr__MathMul` | 15, 22 | exact |
| `th06nc:0x1400310f0` | `EnemyEclInstr__MathDiv` | 16, 23 | exact |
| `th06nc:0x140031b70` | `EnemyEclInstr__MathMod` | 17, 24 | exact |
| `th06nc:0x1400325f0` | `EnemyEclInstr__MathAtan2` | 25 | exact |
| `th06nc:0x14000f870` | `BulletManager__SpawnBulletPattern` | 67–75, 80 | **near** |
| `th06nc:0x1400105e0` | `BulletManager__SpawnLaserPattern` | 85, 86 | exact |
| `th06nc:0x14000ef00` | `BulletManager__TurnAllBulletsIntoPoints` | 83, 93 | exact |
| `th06nc:0x14002b0d0` | `EffectManager__SpawnParticles` | 102, 118 | exact |
| `th06nc:0x1400426e0` | `ItemManager__SpawnItem` | 119, 124 | exact |

两条 near 的判定理由：

- `EnemyEclInstr__SetVar`：NC 侧少了 opcode 8（`SETFLOATRAND`），其余 10 个全中，
  Jaccard 0.91。opcode 8 那条疑似被内联。
- `BulletManager__SpawnBulletPattern`：核心 67–75 与 80 全中，NC 侧多出一个 opcode 1，
  疑似 handler 区间归属误差（见 §4）。这条另有旁证：该函数内部就是
  [`02`](02-th06-baseline-comparison.md) §3 里那个 `aim_mode` switch——
  说明 NC 把 `SpawnSingleBullet` 内联进了它。

## 4. 为什么召回率低：NC 内联得多

**`EnemyEclInstr::GetVar` 在 NC 里没有独立函数**——被内联进了 25 个 handler。
证据链：CC0 说 `GetVar` 被这 25 个 opcode 调用；NC 侧在**同样这 25 个 opcode** 的
handler 里、**起点后固定约 305 字节处**出现 `CALL atan2f`。
而 CC0 的 `GetVar` 内部正好调 `Player::AngleToPlayer`（用 atan2）与 `D3DXVec3Length`（用 sqrt）。
所以内联体连叶子调用一起被复制了 25 份，`atan2f` / `sqrtf` 的 opcode 签名因此
与 `GetVar` 完全相同。

**这反过来说明方法是稳的**：即使中间层被内联，叶子调用仍然保留 opcode 签名。
但也意味着**凡是被内联的函数，NC 侧根本没有地址可命名**。

工具链差异是这一切的根源（Rich header + CRT 依赖实证）：

| | TH06 1.02h | TH06NC |
| --- | --- | --- |
| 架构 | x86-32 | x86-64 |
| Rich header build | 8755–9466 | 33145–35228 |
| CRT | 无 CRT DLL 导入（静态链接） | `VCRUNTIME140` / `MSVCP140` / `api-ms-win-crt-*` |

即 **MSVC 2002 一代 → MSVC v14x 一代**。加上 x64 调用约定要求 32 字节 shadow space
与 16 字节栈对齐，小函数的调用相对成本上升，现代编译器因此更愿意内联。
（是否还开了 LTCG/PGO 🟡 **没有直接证据**，别当结论。）

## 5. 已知的方法缺陷

1. **handler 区间靠「下一个 handler 起点」划分**，是近似。区间大小中位数 42 字节、
   均值 230，但最大有 2723 字节——大区间会吸进不属于它的调用。
   `SpawnBulletPattern` 多出的 opcode 1 大概率就是这么来的。
2. **只看直接 CALL**，不跟跳转。NC 的 handler 常 `JMP` 到共享尾部。
3. **单元素 opcode 集合不可靠**：一个 opcode 调多个函数时，这些函数键相同，
   双向唯一性会把它们全部丢弃（这是刻意的，宁可漏不可错）。

## 6. 复现

```sh
source tooling/env.sh
python3 tooling/ghidra/apply_th06nc_ecl.py --dry-run   # callees=16
python3 -m unittest discover -s tooling/ghidra/tests    # 含出处守门
```

出处守门（`tests/test_th06nc_ecl_callees.py`）会把 16 个名字逐个拿去
`local/vendor/th06-decomp/src/` 里找 `Class::method` 原形，找不到就失败——
防止哪天有人把无 LICENSE 上游的名字混进来。

## 7. 下一步

1. ❓ 把 handler 区间改成**跟随控制流**而不是按地址切，能提升召回。
2. ❓ 同样的手法可以用在 **ANM opcode** 上（[`../../anm/th06nc/02`](../../anm/th06nc/02-th06-baseline-comparison.md)
   已证明两边 case 结构一致），以及 timeline opcode。
3. ❓ 拿到这 16 个函数之后，可以反向做**结构体字段对齐**：
   `EnemyEclInstr__*` 全部接收 `enemy` 指针，两边同一 opcode 的读写偏移可以逐个对。
