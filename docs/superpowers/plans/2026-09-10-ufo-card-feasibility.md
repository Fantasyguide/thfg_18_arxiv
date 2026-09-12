# 星莲船 UFO 卡（card-expand id 112）可行性 + 落地方案

> **版本**：跨版本。本文出现的地址一律带版本前缀（如 `th18:0x40fe70` / `th12:0x44a8a0`）。
> 日期：2026-09-10。性质：**可行性 + 落地方案（Stage 0/1）**，不写实现。
> 输入：`mods/th18.v1.00a/card-expand/IDEAS.md` 的 113 行、[2026-09-08-wishlist-feasibility.md](2026-09-08-wishlist-feasibility.md) 的 G5。
> 方法：Ghidra 12 一手反编译/反汇编 + `thecl -d 18`（`th18.eclm`）+ `local/vendor/th-re-data/data/th18.v1.00a/` 结构体交叉核对。
> 可信度：✅ 一手反编译/反汇编可复核；🟡 单源推断/跨版本类比；❓ 未解。

## 0. 一句话结论

- **① 召唤实体**：`CardLilyWhite__c_press` `th18:0x40fe70` 用 `EnemyManager__allocate_new_enemy` `th18:0x42d7d0` 按**名字**创建 ECL 敌机 `ItemCarryLily`（每第 3 次为 `ItemCarryLily2`）。它是一个**普通 zEnemy**（不是 boss 注册对象）：有血量、hurtbox、被自机弹打、死亡走通用掉落。**召唤路径可复用**，但**行为不可复用**（上浮+开火+随机漂移+下坠写死在 `default.ecl`）；要 UFO 的飘行/变色必须加 ECL 子程序或 C 侧驱动。🟡→✅
- **② 道具磁吸**：TH18 的 state 3/4 吸附目标**硬编码为 `PLAYER_PTR->inner.pos`**（`th18:0x445f79` / `th18:0x446030` 读 `player+0x620/624`），zItem **没有任何「指定目标点」字段**。让非自机实体吸引道具只能：**A. C 侧每帧扫池直写 pos（推荐 Stage 1）**；**B. patch 两处目标读点改成卡写的全局目标（Stage 2）**。两者都确定性、replay 安全；A 不碰引擎代码，风险最低。✅
- **最推荐落地路线**：id 112 做成主动卡（充能 7200，对齐零售 Lily）→ 计数满时用新增 `ce_spawn_enemy` 复用 `th18:0x42d7d0` 召唤一个占位敌机 → `on_item_frame` `th18:0x446ee3` 里 C 侧伪磁场把 P/点道具拉向 UFO、到近处用新增 `ce_item_despawn`（复刻 `th18:0x44658a` 尾段）吸收 → UFO 死/吸满时 `ce_spawn_item` 开奖。Stage 2 再补自定义 ECL 子程序与原生目标重定向。

---

## 1. `CardLilyWhite` 全链一手

### 1.1 召唤链：`c_press` → `allocate_new_enemy` → ECL 名字解析

**① 发现**（`th18:0x40fe70` 反编译 + 反汇编）：

- 门：`card+0x38`（充能 current）`< 1` 且 `ENEMY_MANAGER_PTR` `th18:0x4cf2d0` 非空。
- 在栈上 `memset` 一个 **0x54 字节**的 spawn 结构（`th18:0x40fe93` `push 0x54`；`th18:0x40fe95` `lea eax,[ebp-0x54]`），逐字段填好后调用 `th18:0x42d7d0`。
- 字段写入点（反汇编地址）：`th18:0x40feaa` pos.x=0、`th18:0x40feb9` pos.y=`(float)0xc2000000`（−32.0f）、`th18:0x40fec2` pos.z=0、`th18:0x40feef` main_drop、`th18:0x40fef7/0x40ff12` hp、`th18:0x40ff2a` `$I0`、`th18:0x40fedd` parent_enemy_id=0。
- 名字指针：`th18:0x40ff3a` `push 0x4b4c4c`（`"ItemCarryLily2"`）/ `th18:0x40ff41` `push 0x4b4c5c`（`"ItemCarryLily"`）；调用点 `th18:0x40ff46`。
- 调用 ABI（一手反汇编）：`thiscall`，`ecx = ENEMY_MANAGER_PTR`（`th18:0x40ff35` `mov ecx,edi`），**3 个栈参**（`th18:0x40ff34` push 一个未读值、`th18:0x40ff37` push spawn 指针、`th18:0x40ff3a/41` push 名字），callee 尾 `th18:0x42d9fa` `ret 0xc`。Ghidra 只给 2 个形参，是因为第 3 个栈参**从不读取**（`th18:0x42d7d0`–`th18:0x42d9fa` 内没有 `[ebp+0x10]` 的读）。
- `th18:0x42d7d0` 本体：`operator_new(0x683c)`（`th18:0x42d7d6`）→ `Enemy__constructor` `th18:0x42e100`（`th18:0x42d7ed`）→ 把 spawn 字段拷进敌人 → `FUN_0042fe80` 首帧 → 挂进 `mgr+0x18c` 活动链表、`mgr+0x198` 计数 +1，返回敌人指针。
- `Enemy__constructor` `th18:0x42e100` 尾部 `th18:0x42e43f` 调 `Enemy__sub_48d920` `th18:0x48d920`：对 `enemy+0x1218`（= `ENEMY_MANAGER_PTR+0x188`，ECL 资源表）里的名字表二分查找，命中后把 **sub 索引**写进 `*(uint32_t*)(*(uint8_t**)(enemy+0xc) + 4)`；未命中写 `-1`（`th18:0x48d9a7`）。所以 `"ItemCarryLily"` 是**加载中的 `default.ecl` 的一个 ECL 子程序名**，不是 C 函数名。

**② 推测**：spawn 结构就是「敌机出生参数」：位置、分数、主掉落、血量、两个 flags 位、8 个 ECL int 变量、4 个 ECL float 变量、父敌机 id。

**③ 验证**（独立手段）：

- 用 `objdump` 复核 `th18:0x42d7f7`–`th18:0x42d8fc` 的逐字段拷贝，与 `th-re-data` 的 `zEnemyData` 偏移逐条对上（下表）。
- 用 `thecl -d 18 -m local/vendor/eclmap/eclmap/th18.eclm local/th18.v1.00a/dat/default.ecl` 解出 `ItemCarryLily` / `ItemCarryLily2` 两个子程序，行为与「妖精」一致。
- 名字串 `grep -a` 只在 `local/th18.v1.00a/dat/default.ecl` 命中（8 个 stage `.ecl` 里没有）。

**④ 结论**：`CardLilyWhite` 召唤的是 `default.ecl` 的 ECL 敌机子程序 **`ItemCarryLily` / `ItemCarryLily2`**；创建入口 `EnemyManager__allocate_new_enemy` `th18:0x42d7d0`（`thiscall(mgr; name, spawn*, unused) ret 0xc`）。✅ TH18 v1.00a。

**⑤ 证据**：`th18:0x40fe70`、`th18:0x40ff46`、`th18:0x42d7d0`、`th18:0x42e100`、`th18:0x48d920`；`local/th18.v1.00a/dat/default.ecl` 的 `thecl` dump。

#### spawn 结构（0x54 字节）字段表

| 偏移 | 类型 | 语义 | Lily 实参 | 一手证据 |
| --- | --- | --- | --- | --- |
| `+0x00` | float3 | 出生世界坐标 | `(0, -32, 0)` | `th18:0x42d7f7` 拷进 `enemy+0x12b4`（`zEnemyData.abs_pos`）|
| `+0x0c` | int32 | `score_reward` | 0 | `th18:0x42d80f` 拷进 `enemy+0x621c` |
| `+0x10` | int32 | `drop.main_drop`（主掉落道具类型）| 6 或 4 | `th18:0x42d833` 拷进 `enemy+0x623c` |
| `+0x14` | int32 | `life.current/maximum/remaining` | 4000+300n / 7000 | `th18:0x42d818/0x42d821/0x42d82a` |
| `+0x18` | int32 | 仅 bit0 → `flags_lo \|= 0x80000` | 0 | `th18:0x42d839` `shl 0x13` |
| `+0x1c` | int32 | 仅 bit0 → `flags_lo \|= 0x4000000` | 0 | `th18:0x42d8bc` `shl 0x1a` |
| `+0x20` | int32[8] | `ecl_int_vars[0..7]`（`$I0..$I7`）| `$I0 = n/2` 或 4 | `th18:0x42d860/0x42d86b` |
| `+0x40` | float[4] | `ecl_float_vars[0..3]`（`$F0..$F3`）| 0 | `th18:0x42d876` |
| `+0x50` | int32 | `parent_enemy_id` | 0 | `th18:0x42d8e7` 拷进 `enemy+0x6834` |

> `n` = `card+0x54` 的**已召唤次数**（本次调用前的值）。`th18:0x42d8ed` 还做了一件关键事：`if (spawn+0x14 >= 1000) flags_lo |= 0x40000000`（`th18:0x42d8f6`）。该位的读者未直接定位（🟡；TH12 同类「life>999 出血条」是 `th12:0x412a94` 的 `flags=0x20000000`，仅作类比，不当作 TH18 结论）。

#### ECL 侧行为（`thecl -d 18`，`default.ecl`）

`ItemCarryLily`：`anmSelect(2)` → `anmSetMain(0, 229)` → `setHurtbox(48.0, 48.0)` → `setHitbox(16.0, 16.0)` → `moveVel(π/2, 5.2)`（上浮）→ `moveSpeedTime(80, 0, 0)` → `playSound(74)` → `wait(80)` → 按 `$I0` 起 `@ItemCarryLilyAt0..4()` 异步子弹幕 → `moveLimit(0, 160, 320, 128)` → 10 次 `moveRand(80,0,2.0); wait(160)` → `moveLimitReset()` → `moveVel(-π/2, 2.2)`（下坠）→ `wait(1000)` 死循环。

- `ItemCarryLily2` 同构，只有 `anmSetMain(0, 235)` 不同；`$I0` 决定 `At0..At4`（6/12/18/20/26 颗弹的弹幕，速度/等待递减）。
- `setHurtbox(48,48)` = 自机弹命中箱；`setHitbox(16,16)` = 敌弹对自机判定箱。
- 该 ECL **没有** `dropMain` / `dropExtra` / `dropItems`；掉落完全由 spawn 结构的 `main_drop` 决定（§1.2）。

### 1.2 被打 / 死亡 / 掉落

**① 发现**：

- 自机弹命中走通用敌人伤害管线：`enm_compute_damage_sources` `th18:0x45f0f0` → `EnemyData__step_game_logic` `th18:0x42ed40`；Lily 没有特殊标记，`hurtbox` 由 ECL 设为 48×48，血量由 spawn 设为 4000+。
- `th18:0x42ed40` 在 `th18:0x42ee8b` 用 `ecx = [ebx+0x5588]`（= `zEnemyData.full`，敌人指针）调 `FUN_00430240` `th18:0x430240`（死亡/清理）。
- `th18:0x430240` 关键段：`th18:0x430469` 读 `enemy+0x623c`（`drop.main_drop`）非 0 时，用 `ItemManager__spawn_item` `th18:0x446f40` 在敌人坐标撒**一颗**该类型的道具（角度 `angle=0xbfc90fdb` = −π/2、速度 `speed=0x400ccccd` = 2.2，与原生 main-drop 同参）；`th18:0x43049f` 调 `Enemy__drop_items_and_notify_cards` `th18:0x430510` 撒 `ex_drop[0..18]` 并向所有卡的虚表 `+0x30` 广播；`th18:0x4304aa` 清 `main_drop`；最后 `th18:0x4304f9` 若 `enemy+0x682c` 非空则调用它（`on_death_callback`）。
- Lily 的 `on_death_callback` 是 **0**（`Enemy__constructor` `th18:0x42e100` 在 `th18:0x42e3df` 写 0；`th18:0x430240` 只在非空时调）——卡没有设它，所以死亡只掉主掉落。
- 主掉落类型：`n % 3 == 2`（第 3/6/9…次）→ **4 = 命碎片**（`th18:0x446ec0` body case 4 → `th18:0x457570`），否则 **6 = 符卡/Bomb 碎片**（case 6）。注意这是**道具类型**，不是「4 颗/6 颗」；每次只撒 1 颗。

**② 推测**：`main_drop` 是「杂兵那一颗」的道具类型；`ex_drop` 表由 ECL `dropExtra` 填，Lily 没用。

**③ 验证**：`th18:0x430469` 的 `push eax`（type=main_drop）与 `ItemManager__spawn_item` 的 8 参 ABI 对上；道具 body case 4/6 的效果与 `engine.h` 的 `CE_ITEM_TYPE` 注释一致；`ex_drop` 全 0 由 `th18:0x42d7d0` 只写 `main_drop`、不写 `+0x04` 佐证。

**④ 结论**：Lily 是**普通敌机**：自机弹正常打、血量到 0 走 `th18:0x42ed40` → `th18:0x430240`，**只掉 1 颗道具**（type 6，或每第 3 次的 type 4）。✅ TH18 v1.00a。

**⑤ 证据**：`th18:0x45f0f0`、`th18:0x42ed40`（`th18:0x42ee8b`）、`th18:0x430240`（`th18:0x430469`/`th18:0x43049f`/`th18:0x4304aa`/`th18:0x4304f9`）、`th18:0x446f40`、`th18:0x446ec0` body。

### 1.3 每 3 次强化 + 计数持久化

**① 发现**：

- `n = card+0x54`；`th18:0x40fea0` 读出，`th18:0x40fec9` `cmp ebx,2`（`n%3`），`th18:0x40feda` `setne al`，`th18:0x40fee8` `lea eax,[eax*2+4]` → main_drop = 6（非第 3 次）/ 4（第 3 次）。
- 名字分支：`th18:0x40ff2d` `cmp ebx,2`；等于 2 → `ItemCarryLily2`，否则 `ItemCarryLily`。
- 血量：`th18:0x40fef2` `cmp ecx,0xa`；`n<10` → `n*300+4000`（`th18:0x40ff07`），否则 7000（`th18:0x40fef7`）。
- `$I0`：`n<8` → `n/2`（`th18:0x40ff23`），否则 4（`th18:0x40ff1a`）。
- 调用后 `th18:0x40ff84` `inc [esi+0x54]` 计数 +1；`th18:0x40ff87` `or [esi+0x50],0x20` 置「开火中」；充能重置 `th18:0x40ff78/0x40ff7d/0x40ff81`（`card+0x3c` cur_f、`card+0x38` current、`card+0x34` previous = `card+0x48` × `mgr+0xc58`）。
- 计数持久化：`CardLilyWhite__method_38` `th18:0x40ffa0` 把 `card+0x54` 塞进 `__unknown >> 0x10`，`CardLilyWhite__method_3C` `th18:0x40ffe0` 用 `(card+0x54 << 0x10) | recharge_cur` 还原——这两个槽就是 HUD/replay 用的充能计时器三件套（`+0x34/+0x38/+0x3c`）的 getter/setter。`CardLilyWhite__method_4C` `th18:0x40fde0`（局末 reset）把 `card+0x54` 清 0。
- 构造：`AbilityManager__allocate_new_card` `th18:0x411460` case 0x30（id 48）`operator_new(0x58)`（比基类多 4 字节）、`card+0x48 = 0x1c20`（7200 帧 = 120 s）、flags 走主动卡写法、`card+0x54 = 0`。

**② 推测**：每第 3 次换 `ItemCarryLily2`（外观 235）且主掉落从 Bomb 碎片变命碎片，是「强化」；血量与 `$I0` 随次数递增。

**③ 验证**：`th18:0x40fde0`/`th18:0x40ffa0`/`th18:0x40ffe0` 三函数反编译互相对上（同一 `card+0x54` 字段）；`th18:0x411460` case 0x30 的 `0x58` 分配与 `0x1c20` 与 `engine/card/th18/04-active-cards.md` 的 7200/120 对上。

**④ 结论**：`n%3==2` → `ItemCarryLily2` + 命碎片；否则 `ItemCarryLily` + Bomb 碎片。血量 `4000+300n`（n<10）/7000（n≥10），`$I0=n/2`（n<8）/4。计数存 `card+0x54`，通过 vtable `+0x38/+0x3c` 打包进充能计时器供 HUD/replay；`th18:0x40fde0` 局末清零。✅ TH18 v1.00a。

**⑤ 证据**：`th18:0x40fe70`、`th18:0x40fde0`、`th18:0x40ffa0`、`th18:0x40ffe0`、`th18:0x411460`。

### 1.4 能否复用成 UFO 实体（判据）

| 判据 | 结果 | 依据 |
| --- | --- | --- |
| 能指定出生位置/血量/主掉落/ECL 变量 | ✅ | spawn 结构字段表（§1.1）|
| 能被自机弹打到 | ✅ | 普通 zEnemy；hurtbox 由 ECL `setHurtbox` 定 |
| 能被识别/追踪 | ✅ | `enemy+0x6830` `enemy_id` 单调递增 |
| 能挂死亡/开奖回调 | ✅（有坑）| `enemy+0x682c` 无参回调；单 UFO 可用全局标志 |
| 能自定义飘行/变色/不开火 | ❌ | ECL 子程序行为写死；C 只能改参数 |
| 自带吸引道具 | ❌ | 实体本身无此机制，见 §2 |
| 是「boss」 | 🟡 | 不是 `boss_ids` 注册 boss；hp≥1000 会置一个 flags 位，语义未定位 |

**结论**：

- **召唤路径可复用**：`th18:0x42d7d0` 是通用「按名字造 ECL 敌机」入口，任何在 `default.ecl` 里的子程序都能造，且能喂位置/血量/掉落/ECL 变量。
- **行为不可复用**：`ItemCarryLily` 是「上浮→开火→随机漂移→下坠」的妖精，不是 UFO。Stage 1 可拿它当**占位实体**（有血、能被弹打、能挂死亡）；UFO 的飘行/变色/光环必须 **Stage 2 新增 `default.ecl` 子程序**（用仓库已有的 ECL 资产管线，见 §3.4），或退而用 C 每帧直写 `enemy+0x1270`（会与 ECL 运动打架，不推荐）。
- **死亡识别有坑**：`enemy+0x682c` 回调是**无参 cdecl**，拿不到是哪个 UFO；单 UFO 可用卡私有状态里的全局标志，多 UFO 需要 trampoline 或走 §3.2 的「扩展掉落事件带 enemy 指针」。
- **掉落可复用**：`main_drop` 已能指定任意 type 1..0x13；UFO 奖励可以直接用 `ce_spawn_item`（比走 spawn 结构灵活）。

---

## 2. 道具磁吸一手

### 2.1 自机吸引参数：`ItemManager__on_tick__body` 怎么读

**① 发现**（`th18:0x445a80` 反汇编）：

| 参数 | 地址 | 读点 | 用途 |
| --- | --- | --- | --- |
| `attract_speed` | `player+0x47988` | `th18:0x44663b` | state 4 速度 = 它 / 3.0；V 门 state 3 速度 = 它（`th18:0x445f63`）|
| `item_collect_radius` | `player+0x4798c` | `th18:0x44624a` | 距离² < 它² → 收集 |
| `attraction_radius_focused` | `player+0x47990` | `th18:0x4465f3` | 聚焦时进 state 4 的半径 |
| `attraction_radius_unfocused` | `player+0x47994` | `th18:0x446606` | 非聚焦时进 state 4 的半径 |
| `poc_height` | `player+0x47998` | `th18:0x445baa` | 自机过线 → 全屏真空 |
| 聚焦位 | `INPUT_HELD 0x4ca428 & 8` | `th18:0x4465ea` | 选 focused/unfocused 半径 |

- 所有参数都从 `PLAYER_PTR` `th18:0x4cf410` 读；没有「读某个 UFO 实体」的分支。
- 默认 `{5,30,70,70}`；Nitori `on_load` 写 `{10,30,110,110}`；Q♠ 卡（id 60）就是写这四个字段。

**② 推测**：这四个 float 是唯一的「磁力旋钮」，引擎只认自机。

**③ 验证**：反汇编逐条对上；`th18:0x445a80` 开头 `pzVar13 = PLAYER_PTR`，全程用 `esi`/`pzVar13` 读这些偏移，没有第二个玩家对象。

**④ 结论**：参数语义与偏移 ✅ TH18 v1.00a；目标恒为 `PLAYER_PTR`。

### 2.2 state 3/4 的飞行怎么算

**① 发现**（`th18:0x445a80` + 反汇编）：

- **state 1（抛物/坠落）**：`pos += vel × GAME_SPEED(0x4ccbf0) × slowdown_factor(0xe6bb14)`；`vel.y += GAME_SPEED × 0.03 × factor`；`vel.y ≥ 0` 时 `vel.x = 0`、`vel.y > 2` 钳 2.0。
- **V 门（state 1 → 3）**：`th18:0x445b9a`–`th18:0x445be0`：`player.state != 2 && != 4 && player.y <= poc_height`，或炸弹管理器 `th18:0x4cf2b8 +0x30 == 1 && +0x38 < 0x3c`，或 GUI `th18:0x4cf2e0 +0x1b0 != 0`（对话）→ `th18:0x445f63`：`item+0xc80 = player.attract_speed`、`item+0xc74 = 3`。
- **state 2（延迟释放下落）**：纯重力；`vel.y ≥ 0`（到顶点）→ 同样走 `th18:0x445f63` 进 state 3。
- **state 3（真空飞行）**：`th18:0x445f79` 读 `player+0x624 - item+0xc30`、`player+0x620 - item+0xc2c` → `atan2`（`th18:0x4a94a0`）→ `th18:0x447430`（`polar_to_vel`）写 `item+0xc38` → `pos += vel × GAME_SPEED`；`item+0xc80 += 0.2` 上限 12.0；自机 state==4（死亡）→ state 1、速度清零（`th18:0x446146`）。
- **state 4（70 px 环吸附）**：进入点 `th18:0x4465da`：state 已是 3/4 则跳过；否则按聚焦选半径（`th18:0x4465f3`/`th18:0x446606`），`dist² < radius²` 且 type ∉ {9,10,11,12,13,14}（`th18:0x446617`–`th18:0x446639`）→ `th18:0x446647` state=4、`item+0xc80 = attract_speed / 3.0`（`th18:0x44663b`–`th18:0x446651`）。state 4 的每帧运动与 state 3 同构（`th18:0x446030` 起读 `player+0x620/624`）；POC/炸弹/对话门命中 → 回 state 3（`th18:0x445fe4`–`th18:0x44602a`）；自机死亡 → state 1（`th18:0x446146`）。
- **收集**：`th18:0x44621f`–`th18:0x446259` 算 `dist²` 与 `item_collect_radius²`；小于则 `th18:0x44625f` 按 `item+0xc78` 分派（type 1=P、2=钱、4=命碎、6=Bomb 碎、9–0xe=分、0x10–0x13=卡）。
- **慢因子只作用于 state 1**：state 2/3/4 的位移不乘 `itemmgr+0xe6bb14`（`engine/player/th18/02-graze-and-item-slowdown.md` §2）。

**② 推测**：state 3/4 是两套「飞向玩家」的实现：3 是强制真空/到顶点，4 是普通 70 px 环。

**③ 验证**：反汇编读点逐条定位；`engine.h` 的 `CE_ITEM_STATE` 注释与本次反汇编一致；`collect2x` 卡已实跑过 state 3/4 谓词。

**④ 结论**：state 3/4 的飞行 = **每帧重算「item → player」角度、按 `item+0xc80` 速度直线飞、`GAME_SPEED` 缩放**；目标不可配置。✅ TH18 v1.00a。

### 2.3 有没有「指定目标点」字段？

**① 发现**：`th18:0x445f79`/`th18:0x446030` 的两处目标读点都直接读 `PLAYER_PTR`（`th18:0x4cf410`）+ `0x620/0x624`，中间没有解引用 item 的 target 字段；`local/vendor/th-re-data/data/th18.v1.00a/type-structs-own.json` 的 `zItem` 只有 `__vm_1`/`__vm_2`/`0xc28 __unknown`，没有 target/attractor 字段。

**② 推测**：ZUN 在 TH18 把 TH12 的「飞向 UFO」状态删了，只留「飞向玩家」。

**③ 验证**：TH12 有独立状态 7 + 目标指针（`th12:0x426879` 飞向 UFO、`th12:0x4271c0` 置 state 7），TH18 的 state 集合是 {0,1,2,3,4,5}（写点全集见 `engine.h` `CE_ITEM_STATE`），没有 6/7。

**④ 结论**：**TH18 没有「指定目标点」字段**；唯一引擎原生目标是自机。要让非自机实体吸引道具，必须绕开或改这条硬编码。✅ TH18 v1.00a。

**⑤ 证据**：`th18:0x445f79`、`th18:0x446030`、`th18:0x4465da`、`zItem` 结构；对照 [`engine/ufo/th12/01-ufo-system.md`](../../../engine/ufo/th12/01-ufo-system.md) §7。

### 2.4 让 UFO 吸引道具的候选方案

#### 方案 A（推荐，Stage 1）：C 侧伪磁场 + 复刻吸收尾段

- **做法**：在 `on_item_frame` `th18:0x446ee3`（`ItemManager__on_tick` `th18:0x446ec0` 内、body `th18:0x445a80` **之前**）扫道具池（`mgr+0x14`、4696 槽、stride `0xc94`；先例 `ce_item_pool_any_attracting` 在 `native/sdk.c`）。对 **type∈{1,2} 且 state==1** 的槽：清零 `item+0xc38/+0xc3c/+0xc40`，把 `item+0xc2c/+0xc30/+0xc34` 每帧朝 UFO 坐标推进；到 UFO 近处（建议 18 px，对齐 TH12 `th12:0x44aca0` 的判定）调用新增 `ce_item_despawn(item)` 复刻 `th18:0x44658a` 尾段：`item+0xc74=0` → `ce_anm_delete(item+0xc28)`（`th18:0x488cf0`）→ 按 `*(item)` 的 freelist 头做 `item+4/item+8` 摘挂（`th18:0x4465b4`–`th18:0x4465d2`），然后 UFO 填充计数 +1。
- **风险**：① 每帧 4696 槽扫描（UFO 存活期；可早停/按距离桶）；② 必须跳过 state 3/4（引擎真空/环吸，避免抢）与玩家 30 px 收集圈；③ `ce_item_despawn` 的 freelist 摘挂必须逐条照抄，否则槽泄漏；④ 不要对 type 9–0xe（分数转换点）做。
- **时序**：`on_item_frame` 在 body 之前，本帧直写的 pos 之后仍会被 body 的 state 1 物理加一点（我们已把 vel 清零，只剩重力 +0.03/帧，可忽略）；玩家过 POC / 放 B 前 60 帧 / 对话时引擎会强制 state 3 抢走，优先引擎（跳过即可）。
- **replay 安全**：✅ 全程确定性、无 RNG；只要 mod 在录/放两端一致，输入重放轨迹一致。

#### 方案 B（Stage 2）：patch 引擎目标读点，改指卡写的全局目标

- **做法**：把 `th18:0x445f79`/`th18:0x446030` 的 `player+0x620/624` 改成读一个卡写的全局 `g_ufo_magnet_target`（默认 = 自机坐标，UFO 在场时 = UFO 坐标）；state 4 的入口半径判定 `th18:0x4465f3`/`th18:0x446606` 也要改（或直接由卡把目标道具置 state 3）。
- **优点**：复用引擎原生加速（`+0.2`/帧）、音效、`polar_to_vel`，飞行手感与原生一致。
- **风险**：① 改引擎代码，必须 gate/还原；② 收集判定 `th18:0x44621f` 仍用玩家距离，道具飞到 UFO 后不会自动消失，吸收仍要自己做；③ 改动影响所有道具轨迹，replay 只在「装了同一 patch」时安全。
- **replay 安全**：🟡 确定性，但属于「改玩法」的全局 patch；建议只作为 Stage 2 选项。

#### 方案 C（否决项，留档）

- 移动 `PLAYER_PTR` / 自机坐标去「假装」UFO：会影响移动、判定、POC、子机，❌ 不可行。
- 复用 TH12 的 state 7：TH18 无此状态（§2.3），❌ 不可直接搬。
- 把 `player+0x47988..94` 写到 UFO 附近：这些是**数值**不是**目标**，引擎仍从 `player+0x620/624` 取目标，❌ 无效。

---

## 3. 落地设计（id 112）

### 3.1 组件表

| 组件 | 做什么 | SDK 接缝 | 主要文件 | 阶段 |
| --- | --- | --- | --- | --- |
| 掉计数 | 击杀特定敌人 → UFO 计数 | `on_enemy_drop_pre` `th18:0x430510`（扩展带 enemy）| `native/sdk.c`、`native/sdk/public.h`、`native/cards/ufo.c` | Stage 1 |
| 召唤 | 计数满 → 造 UFO 敌机 | 新增 `ce_spawn_enemy` 包 `th18:0x42d7d0` | `native/engine.h`、`native/sdk.h`、`ufo.c` | Stage 1 |
| 吸引 | UFO 拉 P/点道具并吸收 | `on_item_frame` `th18:0x446ee3` + 新增 `ce_item_despawn` | `native/sdk.c`、`native/sdk.h`、`ufo.c` | Stage 1 |
| 开奖 | UFO 死/吸满 → 奖励 | `enemy+0x682c` 或扩展掉落事件；`ce_spawn_item` `th18:0x446f40`、`ce_add_score_delta` | `native/sdk.c`、`ufo.c` | Stage 1 |
| 视觉/HUD | UFO 光环 + 计数/填充条 | `ce_anm_spawn` `th18:0x405bf0`、`ce_hud_digit_*`、`on_hud_anm` | `assets/ability/`、`native/anm_ids.h` | Stage 1 |
| 自定义 ECL | UFO 飘行/变色/不开火 | `default.ecl` patch（`thecl` 资产管线）| `assets/ecl/`、`native/release.py` | Stage 2 |
| 原生目标重定向 | 引擎 state 3/4 飞向 UFO | patch `th18:0x445f79`/`th18:0x446030` | `patch/th18/*.js` + cave | Stage 2 |

### 3.2 每个组件的 SDK 接缝与文件

**掉计数（Stage 1，推荐卡侧计数）**

- 现有 `on_enemy_drop_pre(card, counts)` 在 `th18:0x430510` 入口触发，`counts = zEnemyDrop+0x04`（20 个 int32）；但 `BP_ce_enemy_drop` `native/sdk.c` 手里有 `regs->ecx = zEnemyDrop = enemy+0x623c`，可以算出 `enemy = ecx - 0x623c`（`th18:0x430499` `lea ecx,[edi+0x623c]`）。
- **建议**：在 `ce_hooks_t` 尾部追加 `on_enemy_death(card, enemy)`（designated-init 增量，老卡不受影响），在同一个断点里把 `enemy` 传给卡。卡用 `enemy+0x6830`（`enemy_id`）或 ECL sub 索引 `*(enemy+0xc)+4` 判定「特定敌人」。
- **❓自定**：TH18 道具 type 固定 1..0x13（`th18:0x446f40` 开头 `if (0x13 < type) return 0`），且 0x10–0x13 是卡道具；**不能**照搬 TH12 的 token type 10–15（那些在 TH18 是分数转换点）。所以 Stage 1 用**卡私有状态计数**（`ce_state`），不做 token 道具；Stage 2 若要原义 token，需要扩 `ITEM_TYPE_TABLE` `th18:0x4b4020`（硬边界）。

**召唤（Stage 1）**

- 新增 `ce_spawn_enemy(const char *name, const ce_enemy_spawn_t *spawn)`：`thiscall` 包 `th18:0x42d7d0`，`ecx = ENEMY_MANAGER_PTR` `th18:0x4cf2d0`，压 3 个栈参（name、spawn、未读 dummy），返回 `zEnemy*`。
- `ce_enemy_spawn_t` = §1.1 的 0x54 字节结构；卡可只填 pos/hp/main_drop/`$I0`，其余 0。
- Stage 1 用 `"ItemCarryLily"` 或 `"ItemCarryLily2"` 占位；Stage 2 换成 `"UFO_Red"` 等自定义子程序。
- 卡记 `enemy+0x6830` 到 `ce_state`，后续用 `ce_enemy_by_id`（`native/sdk.h`）确认存活。

**吸引（Stage 1）**

- `on_item_frame` 里扫池（先例 `ce_item_pool_any_attracting`）；只处理 type∈{1,2}、state==1、不在玩家收集圈、UFO 在场的槽。
- 到近处调新增 `ce_item_despawn`（复刻 `th18:0x44658a` 尾段；`ce_anm_delete` 已有）；填充计数 +1。
- **❓自定**：吸引半径、每帧速度、吸收判定半径、是否吸 type 4/6（TH12 只吸 1/2）。

**开奖（Stage 1）**

- 识别 UFO 死亡：优先用 §3.2 的 `on_enemy_death(card, enemy)` 比 `enemy_id`；退路是 `enemy+0x682c` 设一个无参回调置全局标志，`on_tick_2` 消费。
- 奖励：`ce_spawn_item(type, pos, −π/2, 2.2)`（`th18:0x446f40`）撒奖励道具 + `ce_add_score_delta`；残机/Bomb 碎片可走 `ce_add_life_fragment` / `ce_add_bomb`。
- **❓自定**：奖励表（颜色→碎片/分数）、吸满阈值、是否也走「吸满自动开奖」。

**视觉/HUD（Stage 1）**

- UFO 光环：`ce_anm_spawn(CE_ABILITY_ANM(), script, layer)` `th18:0x405bf0`；脚本走 `assets/ability/` + `build_ability.py`（先例：龙、横幅、青眼血条）。
- 计数/填充：`on_hud_anm` 拿图标 VM，用 `ce_hud_digit_attach/set` 或 `ce_hud_vm_text_*`（`native/sdk.h`）。
- **❓自定**：光环脚本号、颜色、填充条几何。

### 3.3 Stage 1 / Stage 2 边界

| 面 | Stage 1（复刻）| Stage 2（原义/补强）|
| --- | --- | --- |
| 召唤路径 | `th18:0x42d7d0` + spawn 字段语义 | 自定义 `default.ecl` 子程序 |
| 实体行为 | 复用 `ItemCarryLily`（占位）| UFO 飘行/变色/不开火 |
| 掉计数 | 卡私有计数（`ce_state`）| token 道具 type（需扩 `ITEM_TYPE_TABLE`）|
| 吸引 | C 侧伪磁场（方案 A）| 引擎目标重定向（方案 B）|
| 开奖 | `ce_spawn_item` + 固定表 | 颜色/稀有度、赏金道具（对齐 TH12 `th12:0x44af00`）|
| 死亡识别 | 扩展 `on_enemy_drop_pre` 带 enemy | 多 UFO trampoline / 原生回调 |
| HUD | `ce_hud_digit_*` | 原生 `front.anm` 血条/横幅（对齐 TH12）|

**明确标 ❓自定**（原版 TH18 没有对应实现）：UFO ECL 子程序、token 道具类型、吸引半径/速度、吸满阈值、奖励表、HUD 布局、音效 id。

### 3.4 自定义 ECL 的资产路径（Stage 2 可行性）

- 仓库已有 ECL 资产管线：`mods/th18.v1.00a/card-expand/assets/ecl/` 的 `stNN.ecl.txt` + `make_dev_ecl.py`（`thecl -d` 反编译 → 改 → `thecl -c` 编回 → `native/build/ecl/`），`native/release.py` 把 `patch-devstage/th18/stNN.ecl` 映射进 thcrap 仓库。
- 同理可把自定义子程序加进 `default.ecl` 并作为 `patch/th18/default.ecl` 发布；这是**新文件映射**，需要在 `release.py` 的映射表加一行。🟡 可行性高，但本计划不落地。

---

## 4. 未解 ❓ + 下一步

### 未解

1. ❓ **`th18:0x42d8f6` 的 `flags_lo |= 0x40000000` 谁读**：未在 exe 里找到直接 `test [enemy+0x635c], 0x40000000`；TH12 的同类「life>999 出血条」是 `th12:0x412a94` 的 `flags=0x20000000`（类比，不能当 TH18 结论）。UFO 是否需要原生血条，取决于这个读者。
2. ❓ **`enemy+0x682c` 回调的 this/参数**：`th18:0x4304f9` 是裸 `call eax`，没有传参；多 UFO 追踪需要 trampoline 或扩展 SDK 事件。
3. ❓ **`ce_item_despawn` 的 freelist 语义**：`th18:0x44658a` 尾段的 `*(item)` 是哪个 freelist 头（`mgr+0xe6baf4` 还是 cancel_items 的 `mgr+0xe6bb04`），需要实跑确认；写错会导致槽泄漏或链表损坏。
4. ❓ **token 道具原义**：TH18 的 `ITEM_TYPE_TABLE` `th18:0x4b4020` 是 0x14 项、type>0x13 拒绝；扩 type 需要同时扩 sprite/anm/card 三张表，硬边界未评估。
5. ❓ **自定义 `default.ecl` 子程序**：需要确认 thcrap 对 `default.ecl` 的文件替换路径（现有管线只做过 stage `.ecl`）。
6. ❓ **`ItemCarryLily` 的 `moveLimit` 与 `$I0` 行为**：ECL dump 给了指令，但「UFO 应该怎么飘」是设计题，不是 RE 题。

### 下一步（建议顺序）

1. **实跑基线**：把 id 112 做成主动卡（`category: 0`、`active_recharge: 7200`），先只调 `ce_spawn_enemy("ItemCarryLily", …)`，确认能召唤、能被自机弹打、能掉碎片、能挂 `enemy+0x682c` 回调。
2. **方案 A 最小闭环**：`on_item_frame` 只处理 type 2（金钱）→ 拉向 UFO → `ce_item_despawn` → 计数；跑一关看槽是否泄漏、replay 是否同步。
3. **SDK 扩展**：`ce_spawn_enemy`、`ce_item_despawn`、`on_enemy_death(card, enemy)` 三个接缝，每个配主机单测 + AUDIT 条目。
4. **Stage 2 预研**：`default.ecl` 自定义子程序的 thcrap 映射；`flags_lo 0x40000000` 读者；`ITEM_TYPE_TABLE` 扩容边界。
5. **文档回流**：把 §1 的召唤链、§2 的磁吸模型回填 `engine/card/th18/`（另开会话，本计划不改 `engine/`）。
