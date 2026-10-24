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

## 5. 槽字段全表（2026-10-19 一手：逐条读 `0x45dfa0` 的写点 + `0x45f0f0`/`0x45c3e1` 的读点）

> 方法：`objdump -d -M intel` 读 `0x45de40`（圆形 create）、`0x45dfa0`（矩形 create）、
> `0x45f0f0`（敌人侧结算）、`0x45c3e1`（**玩家侧每帧推进器**，`this` = zPlayer）四段。
> 基址：槽 n（0-based，0..0x3ff）在 `PLAYER+0x20574+n*0x9c`；**1-based 口径** = `PLAYER+0x204d8+idx*0x9c`
> （弹侧 `0x45e889`、结算侧 `0x45f19f` 用后者，`idx = n+1`）。`PLAYER` = `[0x4cf410]`。

| 偏移 | 含义 | 谁写 | 谁读 | 可信度 |
| --- | --- | --- | --- | --- |
| +0x00 | flags：bit0 active、bit1 圆形（清 = 矩形）、bit2「命中即置 `*param_4`」 | 两个 create（`and ~6 / or 1`）；`0x45c50d` / `0x45f512` 清 bit0（寿终） | `0x45f1c2` / `0x45c402` / `0x45f470` | ✅ |
| +0x04 / +0x08 | **圆形专用**：`+0x04 += +0x08`（每帧）；矩形 create **不清这两格**（只 memset `+0x1c..+0x5f`） | 圆形 create `0x45ded1/0x45dee2`；推进器 `0x45c422/0x45c437` | 圆形分支 `0x45f521` | ✅ |
| +0x0c | 矩形：**角度**（归一到 `[-π, π]`；推进器每帧 `+0x0c += +0x10` 再归一） | 矩形 create `0x45e097`；推进器 `0x45c481` | `0x45f2f3` 一族 | ✅ |
| +0x10 | 角速度（create 写 0 ⇒ 不转） | 矩形 create `0x45e0a0` | 推进器 `0x45c43c` | ✅ |
| +0x14 / +0x18 | 矩形：**宽 / 高**（全宽全高，判定时 ×0.5） | 矩形 create `0x45e043/0x45e051`（XMM2/XMM3） | `0x45f37c/0x45f2f6` | ✅ |
| +0x1c / +0x20 / +0x24 | 位置 x/y/z（create 拷 `*center`） | 两个 create | 碰撞 `0x45f241` | ✅ |
| +0x60 / +0x64 / +0x68 | 寿命 zTimer：prev / cur / cur_f = `life-1` / `life` / `(float)life` | 两个 create | 结算 `0x45f1cb..0x45f1d6`、推进器 `0x45c4a5..0x45c50b` | ✅ |
| +0x6c | 伤害类别下标（create 写 0；推进器把 ≥1 的钳回 0 后当 `0x4b35c0` 表下标） | 矩形 create `0x45e0b9` | 推进器 `0x45c48d` | 🟡 |
| +0x70 | 子计时器 cur（create 只置 bit0） | 矩形 create `0x45e0c4` | 推进器 | 🟡 |
| **+0x74** | **每次结算的伤害**（`total += +0x74`） | 两个 create（第 4 参） | 结算 `0x45f447/0x45f4c2`、特殊 handler `0x45f49d` | ✅ |
| +0x78 | **累计已造成**（命中即 `+= +0x74`） | 结算 `0x45f4c5` | —（卡侧可当「这条源被结算过没有」的探针） | ✅ |
| +0x7c | 累计上限（create = `9999999`；自机弹那条改成 `10000000`） | create | 结算 `0x45f4f0`（到顶 ⇒ 清 active） | ✅ |
| +0x80 | 命中间隔（create = 1 = 每帧；`timer.cur % 它 == 0` 才结算） | create | 结算 `0x45f1d8`（`idiv`） | ✅ |
| +0x84 | 「一源一敌一次」tag = 上一次命中的 **enemy_id**（`[[zEnemyData+0x5588]+0x6830]`）；推进器**每帧清 0** ⇒ 实际语义 = 每源每敌每帧最多一次 | create 写 0；结算 `0x45f463`；推进器 `0x45c486` 清 0 | 结算 `0x45f45a` | ✅ |
| +0x88 | 冷却（推进器**每帧 `dec`**；> 0 时结算**跳过**这条源） | create 写 0；推进器 `0x45c502` | 结算 `0x45f1e3` | ✅ |
| +0x8c | 伤害类别（1..4 走 4 个 per-kind 累加槽；create 写 0） | create 写 0 | 结算 `0x45f41e/0x45f4c8` | ✅ |
| +0x90 | 所属自机弹的 id（**只有自机弹那条 write**：`0x45e8a4 mov [ecx+0x90],eax`） | 自机弹 `0x45e8a4` | ❓ 未见读者 | 🟡 |
| +0x94 | ❓ 未见写也未见读 | — | — | ❓ |
| +0x98 | 特殊 handler 下标（≠ 0 ⇒ `call [idx*4+0x4b4270]`；**自机弹写 1**，卡侧源 = 0 走通用累加） | create 写 0；自机弹 `0x45e890` 写 1 | 结算 `0x45f487/0x45f4ae` | ✅ |

**结算的自检顺序（`0x45f0f0` 每个槽）**：`+0x00 bit0` → `+0x64 != +0x60` → `+0x64 % +0x80 == 0` →
`+0x88 <= 0` → 几何（矩形走 `+0x0c/+0x14/+0x18` 的 OBB；圆形走 `+0x04/+0x08`）。
**一处常见误判**：池满时分配器（`0x45dfea`）**不写槽也不清 active，却仍返回 `idx+1 > 0`**
⇒ 「`ce_damage_rect` 返回 > 0」**不等于**源建出来了，判据要回读槽的 `+0x00 bit0` / `+0x74`。

## 6. ★ 敌人侧还有三道「扣血闸」——源命中 ≠ HP 掉（2026-10-19 一手）

`EnemyData__step_game_logic` `0x42ed40` 里**扣血只有两处**（`0x42f2da sub [ebx+0x5000],esi` 与
`0x42f314 sub [ebx+0x4ff4],esi`，`ebx` = zEnemyData = `enemy+0x122c`），两条都排在三道闸之后：

| 闸 | 地址 | 判据 | 后果 |
| --- | --- | --- | --- |
| ① 不可锁定 | `0x42f101` | `test cl,0x21`（flags `[ebx+0x5130]` bit0/bit5 = `CE_ENEMY_FLAG_NO_LOCK`）| **整段伤害结算（含 `call 0x45f0f0`）被跳过** |
| ② 免伤位 | `0x42f2b9` | `test byte [ebx+0x5130],0x10`（bit4 = `CE_ENEMY_FLAG_NO_DAMAGE`）| 只 `+0x5008 += dmg`，不扣血 |
| ③ 无敌计时器 | `0x42f2c2` | `cmp dword [ebx+0x50f4],0 / jg`（zTimer `+0x50f0`，绝对 `enemy+0x6320`；写点只有 ECL `setInvuln` `0x433c28`+`0x405d10`）| 只 `+0x5008 += dmg`，不扣血 |

`+0x5008`（绝对 `enemy+0x6234`）= **含被忽略伤害的总伤害** ⇒ 它是「源到底有没有命中」的判据：
`+0x6234` 涨了而 `life.current` 没动 ⇒ 命中被闸吃掉；两者都不动 ⇒ 源根本没命中。
实跑 6/6 条卡侧伤害源 `hp_delta=0` 全部落在闸关着的帧（2 条 flags bit0 置位被 ①、4 条 `inv>0` 被 ③）。
