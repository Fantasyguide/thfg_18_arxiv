# TH18 自机伤害源管线（玩家侧建源 → 敌人侧结算）

> **版本**：TH18 v1.00a（`th18.exe`，imagebase `0x400000`）。本文裸地址默认属该版本；引用其他版本须写成 `th16:0x…`。
> 方法：headless 反编译（`tooling/ghidra/scripts/decompile_funcs.py`）+ 反汇编核出口。2026-09-04。
> 可信度：函数签名 ✅（出口逐条看过）；池字段语义 🟡（从两个 create 与结算函数交叉，未实跑）。
> 对照 [`../../sht/th16/08-th16-player-damage-pipeline.md`](../../sht/th16/08-th16-player-damage-pipeline.md)（th16 同构，stride 0x94 → th18 0x9c）。

## 0. 一句话

自机对敌人的伤害不在弹里算：玩家侧往 `player+0x20574` 的 **1024 槽伤害源池**放一条记录，敌人侧每帧遍历池、判重叠、累加伤害并钳到 `player+0x47984`。
主动卡 Miko / Remilia 就是直接往池里放源来打伤害的。

## 1. 池

`zPlayer+0x20574` 起，每槽 `0x9c`，`0x400` 槽；`player+0x20570` = 上次分配的下标（环形找空槽）。槽内（相对槽首）：

| 偏移 | 含义 | 出处 |
| --- | --- | --- |
| +0x00 | flags：bit0 active，bit1 圆形（清 = 矩形），bit2 由结算函数读作「命中即置 `*param_4`」 | 两个 create、`0x45f0f0` |
| +0x04 / +0x08 | 圆形：XMM2 / XMM3（半径类参数）| `0x45de40` |
| +0x0c | 矩形：角度（归一到 `[-π, π]`，两端都闭——见表下补注）| `0x45dfa0` |

> ★ **2026-09-10 一手更正（在 `th18.exe` 上自证，不是从 th16 外推）**：区间是 **`[-π, π]`，两端都闭**，不是 `(-π, π]`。th18 的归一例程 `th18:0x402890`（常量 π `th18:0x4b922c`、2π `th18:0x4b9260`、−π `th18:0x4b9430`，float32 值与 th16 逐字节相同）两段循环的出口判据分别是 `x <= π` 与 `-π <= x`；守卫 `cmp eax,0x20; jg` 比的是**自增前**的值 ⇒ 上限 **34** 步，泄漏阈值 `|θ| ≲ 216.77`（= 69π）。`0x45dfa0` **内联**了同一对循环（守卫在 `th18:0x45e06f` / `th18:0x45e08d`），不是 `call`。

| +0x14 / +0x18 | 矩形：宽 / 高（XMM2 / XMM3）| `0x45dfa0` |
| +0x1c..+0x24 | 位置 x/y/z（create 拷 `*center`）| 两个 create |
| +0x60/+0x64/+0x68 | 寿命 zTimer prev/cur/cur_f = life−1 / life / life | 两个 create |
| +0x74 | 每次结算的伤害 | 两个 create（第 4 参）|
| +0x78 | 累计已造成 | `0x45f0f0` += |
| +0x7c | 累计上限（9999999）| create |
| +0x80 | 命中间隔（1 = 每帧）| create |
| +0x98 | 特殊 handler 索引（≠ 0 走 `0x4b4270` 表）| `0x45f0f0` |

## 2. 两个建源函数

| 函数 | 签名 | 备注 |
| --- | --- | --- |
| `Player__create_damage_source_45de40` `0x45de40` | thiscall(player; center*, life, dmg) + XMM2/XMM3，`ret 0xc` | 圆形；Miko `0x40e5c0` 调 `(pos, 0x78, 10)` |
| `Player__create_damage_source_rect`（`FUN_0045dfa0`）`0x45dfa0` | stdcall(center*, angle, life, dmg) + XMM2 宽 / XMM3 高，`ret 0x10` | 矩形；Remilia `0x40f4a6` 调 `(玩家上方, 0.0, 2, 200)` 宽 32 |
| `Player__get_damage_source_by_index` `0x409a30` | stdcall(idx) `ret 4` | 1-based → 槽地址；0 → `player+0x46ff0` 哨兵 |

## 3. 结算：`enm_compute_damage_sources` `0x45f0f0`

敌人侧调（stdcall 7 参 + XMM3）：遍历 1024 槽，active 且寿命未到且 `timer % 命中间隔 == 0`；矩形按 OBB（角度 +0xc、半宽 +0x14×0.5、半高 +0x18×0.5）与敌人碰撞盒判，
圆形按 `(r + 敌半径)² < dist²` 排除。命中：`total += +0x74`、`+0x78 += +0x74`，累计到 +0x7c 就停用。
**`total` 钳 `player+0x47984`**（`0x45f28b`），`GameThread__on_tick` `0x443d3b` 每帧从 `sht+0x28`（max_dmg）复位它；Remilia 脉冲期间 `0x40f48e` 把它写成 300。
非 bomb 模式（`param_6 == 0`）时 `SCORE += (total/10 + 10)/10`，封顶 999999999。

## 4. 用它的 mod

`mods/th18.v1.00a/card-expand/native/sdk.h` 的 `ce_damage_rect`（青眼白龙光束；AUDIT O29b）。
