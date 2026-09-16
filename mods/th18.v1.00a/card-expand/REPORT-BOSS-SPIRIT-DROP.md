# REPORT-BOSS-SPIRIT-DROP —— TH18「Boss 随血量掉灵」（TH13 通道 A 的移植钩点）

> **版本**：TH18 v1.00a（`th18.exe`，md5 `9969cac756098c1da05a81de45437a70`，847,360 B，i386，imagebase `0x400000`）。
> 本文裸地址默认属该版本；引用别作一律写成 `th13:0x…`。
> **日期 / 性质**：2026-09-13。**只读研究**（不改 exe、不改任何既有文件、未实跑）——本文只给落笔方案，落地由实现者按纪律登记 `AUDIT.md` §BE。
> **需求**：用户报「Boss（含道中boss）随血量减小没有掉灵」= TH13 的**通道 A**（一手见 [`engine/spirit/th13/01-spirit-and-trance.md`](../../../engine/spirit/th13/01-spirit-and-trance.md) §3.1）在卡 113（[`native/cards/trance.c`](native/cards/trance.c)）里尚未实现。
> **方法**：PE 直读 + capstone 线性反汇编（`local/_scratch/th18.dis` 快照 + 现算）+ 全 exe xref 普查 + ExpHP `th-re-data` 结构体/函数名 + `thecl -j -d 18` 全量 ECL 普查。**不占 Ghidra 锁。**

## 0. 一页答案

| 问题 | 答案 | 可信度 |
| --- | --- | --- |
| 伤害落点（唯一汇聚处）| `EnemyData::step_game_logic` `0x42ed40` 的 `0x42f2cb`；`ebx` = `zEnemyData`、`esi` = 最终伤害 | ✅ 一手 |
| `0x45e7ff` 能不能用来累计伤害 | **不能**：它在 `PlayerBullet::create` `0x45e320` 里、每建一颗弹一次、现场**没有敌人指针** | ✅ 一手 |
| boss 判据 | `zEnemyData.flags_lo`（`enemy+0x635c`）**bit23 `0x800000`** = ECL `setBoss` 注册位 ⟺ boss/道中 boss | ✅ 一手 + ECL 全量普查 |
| 掉落位置 | `zEnemyData.final_pos` = `enemy+0x1270`（= 既有 `ce_enemy_pos()`）| ✅ 一手 |
| 每敌状态 | `zEnemyData` 无 TH13 那三格；推荐 **mod 侧 8 槽表、按 `enemy_id` 键**（空闲字段 `enemy+0x6314` 已查实可用，见 §5）| ✅ / 🟡 |
| 风险 | 断点必须回放原指令；掉落实体**延到帧末**（死亡栈纪律同款）；无敌期不计（与用户口径一致）| ✅ / 🟡 |

## 1. 伤害落点：`0x42f2cb`（`EnemyData::step_game_logic` `0x42ed40`）

### 1.1 发现（一手地址链）

所有自机弹 / 炸弹对敌人的掉血，最终都汇到 `EnemyData::step_game_logic` 这一段：

```text
0042f11d  comiss xmm0,xmm1              ; hurtbox_size.x > 0 ？
0042f122  mov eax,[ebx+0x5588]          ; zEnemy*
0042f131  mov eax,[eax+0x6830]          ; enemy_id（作为参数）
0042f16e  call 0x45f0f0                 ; enm_compute_damage_sources → eax = 本帧原始伤害
0042f177  mov eax,[0x4cf410]            ; PLAYER
0042f17f  mulss xmm0,[eax+0x47980]      ; × 自机伤害倍率（CE_PLAYER_DAMAGE_MULT）
0042f187  cvttss2si esi,xmm0            ; esi = 伤害
0042f199  call eax                      ; + ECL 扩展伤害钩子 [ebx+0x55f0]
0042f19d  mov eax,[ebx+0x50d4]
0042f1a7  add esi,eax                   ; + ECL 额外伤害（[ebx+0x50d4]）
0042f1c7  mov eax,0x66666667 / imul esi ; 某些 shot type 走 ÷5
0042f1de  cmp [GUI+0x1b0],0 / cmove edi,esi   ; 对话中 ⇒ edi = 0（也就不掉血）
0042f240  mulss xmm0,[ebx+0x512c]       ; 炸弹生效帧 × bomb_damage_multiplier
0042f287  test esi,esi / je 0x42f3c5    ; ★ esi = **最终伤害**（int，≥1）
0042f2b9  test byte [ebx+0x5130],0x10   ; 闸①：flags_lo bit4
0042f2c0  jne 0x42f31c                  ;    → 记 total_damage 但**不掉血**
0042f2c2  cmp [ebx+0x50f4],0            ; 闸②：invuln_timer.current > 0
0042f2c9  jg 0x42f31c                   ;    → 同上
0042f2cb  add [ebx+0x5008],esi          ; ★★ life.total_damage_including_ignored += 伤害
0042f2d1  test byte [ebx+0x500c],1      ; life.is_spell ？
0042f2d8  je 0x42f314                   ;   非符卡 → 直接扣
0042f2da  ... 符卡路径：重算 current_scaled_by_seven → mov [ebx+0x4ff4],eax
0042f314  sub [ebx+0x4ff4],esi          ; life.current -= 伤害
0042f31c  add [ebx+0x5008],esi          ; （免伤路的落点）
0042f322  ... 命中闪光计时器 [ebx+0x5118..0x5128] = 0x1d/0x1e/30.0f
0042f39d  cmp [ebx+0x4ff4],eax / setle  ; life.current <= 0 且未被锁定 ⇒
0042f3b2  call 0x430240                 ;   Enemy__die（本 mod 的 ce_enemy_death）
```

### 1.2 验证（为什么这是「唯一」汇聚点）

| 验证项 | 一手结论 |
| --- | --- |
| `life.current` 全 exe 写点穷举 | 6 处：`0x42d818`（出生按 ECL）、`0x42e468`（构造清零）、`0x42ef4d`/`0x42f44c`（中断槽换相位写 marker）、`0x42f30c`、`0x42f314` |
| ⇒ 伤害引起的写点 | **只有 `0x42f30c` 与 `0x42f314`**，两者都在 `0x42f2cb` **之后** |
| `0x42ed40` 的调用方 | 只有 `0x42fff0`（在 `FUN_0042ff80`「敌人每帧入口」里，`this` = `zEnemyData`）|
| `FUN_0042ff80` 的调用方 | 只有 `0x42fedd` / `0x42ff52`，全在 `FUN_0042fe80`（单敌推进器）里，且是 **if/else** |
| ⇒ 频率 | `FUN_0042fe80` 里 `[ebx+0x1510]`（slowdown）>0 走慢速支、否则走常规支 ⇒ **每敌每帧恰好一次** |
| 另一条调用 `0x45f0f0` 的路 | `0x42ee11`–`0x42ee92`（`flags_lo & 0x800`）是「命中即死、不掉血」；`0x43955b`/`0x43959d` 在 `0x439320` 里只是**累加返回值**（伤害查询），都不写 `life` |
| 既有站点是否冲突 | `sites.py` 19 个站点里最近的是 `ce_enemy_death` `0x430240`（差 `0x1000`+）⇒ 不冲突 |

**结论（✅ 一手）**：`0x42f2cb` 是「这一帧这个敌人的 HP **即将**减少」的**唯一**汇聚点，且此处 `esi` 已是最终伤害（倍率、ECL 扩展、炸弹倍率**都已算完**）。

### 1.3 推荐站点

| 站点名（建议）| 地址 | 长度 | exe 字节 | 原指令 | 纪律四问 |
| --- | --- | --- | --- | --- | --- |
| `ce_enemy_damage` | `0x42f2cb` | 6 | `01b308500000` | `add [ebx+0x5008],esi` | ①6∈[5,27] ✅ ②无相对寻址 ✅ ③窗内无跳转靶（窗内为 `0x42f2cb..0x42f2d1`）✅ ④整指令对齐 ✅ |

> 四问用 `python3 local/_scratch/scan_bp_window.py 0x42f2cb:6` 现算过（capstone 线性扫全 `.text`，204,100 条指令、重同步 66 处）。

**寄存器 / 内存语义（handler 里可读的现场）**

| 位置 | 值 | 一手出处 |
| --- | --- | --- |
| `ebx` | `zEnemyData*` = `zEnemy* + 0x122c` | `0x42ed4f`；调用方 `0x42fedd lea ecx,[ebx+0x122c]` |
| `esi` | 本帧最终伤害（int，≥1） | `0x42f287` 前置检查 + `0x42f2cb` 直接用它 |
| `[ebx+0x5588]` | **`zEnemy*`**（`zEnemyData.full`，构造时唯一一次写 `0x42e208`，此后不变） | 引擎自己就这么用：`0x42f35f` 读它取 `+0x6220`、`0x42f3d2` 读它喂 `0x430240` |
| `enemy+0x6830` | `enemy_id`（每实例唯一、单调递增） | ExpHP `zEnemy.enemy_id` |
| `[ebx+0x4ff4]` / `[ebx+0x5004]` | `life.current` / `life.maximum`（= `enemy+0x6220`/`+0x6224`） | ExpHP `zEnemyLife` |
| `[ebx+0x5130]` | `flags_lo`（= `enemy+0x635c`） | ExpHP `zEnemyData.flags_lo` |
| `enemy+0x1270` | `final_pos.x/y/z`（float3）= 掉落位置 | 见 §4 |

**handler 必须做什么**

1. `zEnemy*` = `[ebx+0x5588]`（空则直接 `BP_EXEC_ORIGINAL`）。
2. 门：`flags_lo & 0x800000`（§3）——不是 boss 就立刻返回（一次位测试，杂鱼几乎零成本）。
3. 读 `esi`（伤害）、`life.current/maximum`、`zEnemy+0x1270`（x/y 快照）。
4. 按 `enemy_id` 查/建 mod 侧状态，累计伤害（§5 公式）；越过阈值 ⇒ **只入队一条掉落请求** `(enemy_id, x, y, type=1)`。
5. **返回 `BP_EXEC_ORIGINAL`** —— 那条 `add [ebx+0x5008],esi` 是引擎自己的记账，**绝不能跳过**。
   紧随其后的 `0x42f2d1 test byte [ebx+0x500c],1` 自带设标志 ⇒ 本站点不依赖也不干扰标志位。

### 1.4 备选站点（用途不同，不是替代）

| 站点 | 地址 | 长度 | 字节 | 什么时候用它 |
| --- | --- | --- | --- | --- |
| 闸前全身位 | `0x42f2b9` | 7 | `f6833051000010` | 若要「**无敌期也计数**」（`invuln_timer>0`、`flags_lo` bit4 期间的伤害也算）。窗内有 1 个落在**站点自身**的跳转靶（`0x42f2a4 jne 0x42f2b9`）——按 house 规则合法（跳到站点 = 走断点）|
| 只扣血那一条 | `0x42f314` | 6 | `29b3f44f0000` | **不推荐**：只覆盖非符卡分支（符卡走 `0x42f2da`），会漏掉符卡期一半 ✅反证 |

> 三个站点**互斥选用**（同时挂会重复计数）。默认选 `0x42f2cb`：它的语义与用户原话「随**血量减小**」逐字对齐。

## 2. `ce_bullet_damage_final`（`0x45e7ff`）体检 —— verdict：**不能**用作伤害累计点

### 2.1 发现

- 所在函数 = `PlayerBullet::create` `0x45e320`（ExpHP 名；本仓 `games/th18.v1.00a/symbols.json` 未收录）。
- `edi` = **正在构造的自机弹对象**：函数入口 `0x45e331 mov edi,ecx`（`this`），全程不变；`0x45e7e1 mov [edi+8],esi` 是给这颗弹写 VM id。
- `esi` 此刻**不是伤害**——是卡链节点游标（`0x45e7e4 mov esi,[eax+0x1c]`），`0x45e7f0`–`0x45e7fd` 是卡链表遍历。
- 站点之后 `0x45e837 push [edi+0x9c]` → `0x45e861 call 0x45dfa0`（`Player::create_damage_source_45de40`），`0x98967f` = 寿命 9999999：**一颗弹把自己的伤害登记进伤害源池**（不是打中敌人）。

### 2.2 结论（✅ 一手，否决）

| 问题 | 答案 |
| --- | --- |
| 现场有敌人指针吗 | **没有**。只有 `edi` = 自机弹；`esi`/`eax` 都是卡链相关 |
| 伤害已经施加了吗 | 都没有。`[edi+0x9c]` 是弹的**配置伤害**，此刻还没进伤害源池 |
| 触发频率 | **每建一颗自机弹一次**（唯一调用方 `0x45e9ec`，在 `Player::shoot_one_bullet` `0x45e930` 的建弹循环里）——既不是每次命中、也不是每帧 |
| 能用来「按敌累计伤害」吗 | **不能**，结构上不可能：没有敌人可归属 |
| 它正确的用途 | 「缩放这颗弹的伤害值」——即现有文档（[`BP30-IMPLEMENTATION.md`](BP30-IMPLEMENTATION.md) §1）说的那件事，与本文用途**正交** |

> 另注：既有事件钩子 `on_shot_damage` / `on_shot_damage_any`（断点 `ce_shot_damage` `0x45f2a2`，在 `enm_compute_damage_sources` 内部）虽然给「本帧本敌伤害 + 位置」，但**不带敌人身份**，且值在倍率/ECL 扩展**之前** ⇒ 也不适合做通道 A 的累计（要按坐标配对敌人，脆）。

## 3. boss判据：`flags_lo & 0x800000`

### 3.1 发现

`zEnemyData.flags_lo`（绝对偏移 `enemy+0x635c`）的 **bit23 `0x800000`** 由 ECL 的 `setBoss` 指令置/清：

```text
00433b95  ...                            ; ECL opcode 512 = setBoss 的 handler（edi = zEnemyData）
00433ba6  mov [ebp-0x56c],eax            ; 参数存起来（= boss 槽号）
00433bbd  test eax,eax / jns 0x433bf3    ; 参数 >= 0 ⇒ 注册；< 0 ⇒ 注销
00433bc1  test ecx,0x800000              ; 注销：已注册才清表
00433bd7  call 0x42d490                  ;   set_boss_id(slot = [edi+0x5140], 0)
00433be2  and ecx,0xff7fffff             ; ★ 清 bit23
00433bf5  or  ecx,0x800000               ; ★ 注册：置 bit23
00433c08  call 0x42d490                  ;   set_boss_id(slot = 参数, enemy)
00433c13  mov [edi+0x5140],eax           ;   记下自己的槽号（own_boss_id）
0042d4aa  mov [edx+eax*4+0x48],ecx       ; 0x42d490 本体：mgr->boss_ids[slot] = enemy->enemy_id
```

### 3.2 验证（三条独立证据）

| 证据 | 内容 |
| --- | --- |
| ① opcode 号双重独立对上 | 分派指针表 `0x436438` 的 index 57 = `0x433b95`，60 = `0x433c1e`；`455+57=512`、`455+60=515` ⇒ `512 setBoss` / `515 setInvuln`，与 eclmap 名**逐条同差** ✅ |
| ② ECL 全量普查 | `thecl -j -d 18` 跑全部 22 个 `.ecl`：**只有** `st0Xbs.ecl`（boss）与 `st0Xmbs.ecl`（道中 boss）各有 1 次 `setBoss(0)` + 2..3 次 `setBoss(-1)`；7 个舞台脚本 + `default.ecl` **0 次** |
| ③ 引擎自己也用这一位 | `0x42eed0 test [ecx+0x635c],0x800000` → 为真才走 boss 血条/相位段（写 `[GUI+0x1b8]/[GUI+0x1bc]`）；`0x434c20` 同一位 + `0x434c30 cmp [edi+0x5140],0` 走另一段 boss 专属逻辑 |

**结论（✅ 一手）**：判据 = `(*(int32_t *)(enemy + 0x635c) & 0x800000) != 0`，一行、无管理器查找、无 4 次遍历。
**并且它天然含道中 boss** —— 这不是推断，是 14 个 boss/道中 boss ECL 文件的普查结果 ✅。

**两个坑（都有一手反证）**

- ⚠ **`own_boss_id`（`enemy+0x636c`）不能单独当判据**：注册时写槽号（本作 ECL 恒 `0`），注销时**不清**（`0x433bc1`–`0x433bf3` 只清表和 bit23）⇒ 道中 boss 退场后仍是 0，区分不出来。
- ⚠ 现有 `enemy_is_boss()`（[`native/cards/season.c`](native/cards/season.c)、`animal.c`：按 `boss_ids` 查 id）与位测试**语义等价**（同表同源），但多一次管理器解引用 + ≤4 次比较；新代码用位测试即可，**不必改**既有函数。
- ⚠ 别与 `flags_lo` 的 bit30 `0x40000000` 混淆：那是「`life ≥ 1000` ⇒ 进锁定黑名单」（`0x42d8f6` 一手，`AUDIT` AQ5），与 boss 无关。

## 4. 掉落位置：`enemy+0x1270`

| 项 | 值 | 一手证据 |
| --- | --- | --- |
| 推荐 | `zEnemyData.final_pos.x/y/z` = **`enemy+0x1270`** | `Enemy__die` `0x430255 movss xmm2,[edi+0x1270]`、`0x43026b lea ebx,[edi+0x1270]`（引擎自己用它定位死亡特效/掉落）|
| 交叉印证 | 伤害判定也用它当敌人位置 | `0x42ee31 lea edi,[ebx+0x44]`（= `data+0x44` = `0x1270`）作为位置参数喂 `enm_compute_damage_sources`；`0x42f12e` 同 |
| 口径一致 | 就是本仓既有的 `ce_enemy_pos()`（`native/sdk.h` 返回 `e + CE_ENEMY_POS_X`） | `engine.h` 的 `CE_ENEMY_POS_X 0x1270`；所有卡都从这里取坐标 |
| 备选 | `zEnemyData.abs_pos` = `enemy+0x12b4` | 出生 `0x42d7fb` 从生成结构拷入；读点 `0x437402`/`0x43805a`/`0x438900`（ECL 侧坐标查询）🟡 插值移动中会与 `final_pos` 短暂不一致 |
| 自机位置（算距离分档）| `ce_player_pos()` = `PLAYER_PTR`(`0x4cf410`) + `0x620`（float3）| `engine.h` 的 `CE_PLAYER_X 0x620` |

**结论（✅ 一手）**：用 `ce_enemy_pos(zEnemy)` 撒神灵，与死亡通道同一口径；`abs_pos` 只在做 ECL 对照时才有意义。

## 5. 每敌状态：TH18 没有 TH13 那三格 ⇒ 推荐 mod 侧小表

### 5.1 发现：TH13 的三个字段在 TH18 不存在

TH13 的 `+0x3f60`（累计）/ `+0x3ffc`（阈值）/ `+0x4004`（冷却）换到 TH18 的 `zEnemy` 里会落到：
`0x3f60 − 0x122c = 0x2d34`、`0x3ffc → 0x2dd0`、`0x4004 → 0x2dd8` —— 全在 **`zEnemyData.bullet_props`（`+0x598`，stride `0x488`，16 槽）** 的 shooter[8]/[9] 里面，
即**弹幕发射器参数**，不可能是计数器 ✅（ExpHP `type-structs-own.json` 的 `zEnemyData` 字段表逐条对过，`0x4ff4..0x5600` 段里也没有同义字段）。

### 5.2 空闲字段普查（有，但只有一个 int32，且另一格已被占）

| 候选 | 绝对偏移 | 普查结论 | 能不能用 |
| --- | --- | --- | --- |
| `zEnemy.__only_used_in_ddc` | `enemy+0x6838` | 真死字段（全 exe 只有 `0x42e427` 一处构造清零）| ❌ **已被 110 季节卡占用**（`season.c` 的“已掉过季节点”闩锁，`engine.h` 的 `CE_ENEMY_DDC_UNUSED`）|
| `zEnemyData.__i_think_this_field_is_legit_unused` | **`enemy+0x6314`**（`data+0x50e8`）| 全 exe **0 读 0 写**（`0x50e8`/`0x6314` 两种拼法的 xref 都为空）；且 `Enemy__constructor` 有 `memset(data, 0, 0x5600)`（`0x42e1e7`–`0x42e1f3`）⇒ 每敌出生必为 0 ✅ | ✅ 可用（4 B/敌，ExpHP 名 + 一手普查双重）|

> 结论（✅）：**有**空闲字段，但只有 **1 个 int32**；而通道 A 需要 2 个（累计 + 阈值/冷却）。且抢字段要过 `AUDIT.md` §BE，还可能与别的卡再撞。

### 5.3 推荐：mod 侧 8 槽表，按 `enemy_id` 键（主方案）

```c
/* 卡 113 内部；不碰引擎内存 */
typedef struct { int32_t enemy_id; int32_t accum; int32_t threshold;
                 int32_t last_drop_frame; int32_t last_seen_frame; } ce_boss_dmg_t;
static ce_boss_dmg_t s_boss_dmg[8];
```

| 设计点 | 取值 / 理由 |
| --- | --- |
| 键 | `enemy_id`（`enemy+0x6830`，每实例唯一、单调递增）——**不是指针**：敌人槽会被复用，指针会 ABA 静默串味 |
| 槽数 | **8**。门上 boss 位之后，同时在场目标 ≤ `zEnemyManager.boss_ids[4]` = 4（本作 ECL 恒用 slot 0，多数帧只有 1）⇒ 8 槽永不驱逐活 boss |
| 淘汰 | `last_seen_frame` 最旧者；另可在 `on_enemy_death` 里按 `enemy_id` 主动摘除（纯卫生，非必需）|
| 帧源 | 卡自己的 `s->frame`（`on_item_frame` 计数），别用引擎计时器 |
| 为什么不上引擎字段 | ① 需要 2 个 int32，空闲字段只 1 个；② 写引擎内存要登记 + 抢格风险；③ 表方案能在 `native/tests/` 里纯函数单测（本仓既有模式）|

**计数公式（照 TH13 通道 A 同形，`th13:0x419de9` / `th13:0x419e3a` 一手）**

```c
st = table_find_or_new(enemy_id, frame);
st->accum += dmg;
if (st->accum >= st->threshold && frame - st->last_drop_frame >= 20) {
    queue_drop(enemy_id, x, y, /*type=*/1);          /* 只入队，帧末再建 VM */
    st->threshold += bucket(dist2_player_to_enemy);  /* ★ += 不是 = */
    st->last_drop_frame = frame;
}
/* bucket: dist2 < 2304(48²) → 200；< 9216(96²) → 400；< 36864(192²) → 600；else 1000 */
```

> ⚠ 两条必须照抄的细节：**阈值是累加**（TH13 一手 `enemy->+0x3ffc += …`）；**距离越近掉得越密**。
> 掉落类型取 **1**（TH13 通道 A 就是 `spawn_spirit(manager, 1, …, 2.0, 1)`，与卡 113 现有击破通道 `TRANCE_KILL_SPIRIT_TYPE = 1` 同值）。

## 6. 风险与处置

| # | 风险 | 处置 |
| --- | --- | --- |
| 1 | 断点跳过原指令 | 必须 `BP_EXEC_ORIGINAL`：`add [ebx+0x5008],esi` 是引擎的 `total_damage` 记账，跳过 = 悄悄改引擎数值 |
| 2 | 在伤害现场建 VM | **死亡栈纪律同款**（`AUDIT` §AL55、[`native/cards/trance.c`](native/cards/trance.c) 注释）：现场**只入队标量**（x/y/type），帧末 `on_item_frame` 再 `trance_spirit_spawn()` |
| 3 | 死敌 / 槽复用 | 现场敌人一定活着（本函数由每敌每帧入口调）；**绝不跨帧存 `zEnemy*`**，只存 `enemy_id` + x/y 快照 |
| 4 | 重入 | handler 里除整数运算外不调引擎分配器/链表操作 ⇒ 无重入；VM 建立延到帧末 |
| 5 | 性能 | 每敌每帧 ≤1 次 handler；最坏 ~百次/帧，每次 1 次位测试 + 8 槽线性查 ⇒ 可忽略（真开销 `enm_compute_damage_sources` 的 0x400 源扫描本来就在）|
| 6 | 无敌期口径 | 站点在两道闸**之后** ⇒ 免伤/无敌期伤害**不计**，与用户口径「随血量减小」一致；要改成「也算」就换 `0x42f2b9` |
| 7 | 与击破通道叠加 | 打死那一帧，伤害通道与 `ce_enemy_death` 通道都会给 —— 与 TH13 同（通道 A 与 B 本来就叠加）。要防重复就在 handler 里额外跳过 `life.current - dmg <= 0` 的那次（🔧 可选旋钮）|
| 8 | 已知缺口（已查、可忽略）| `flags_lo & 0x800` 的敌人走 `0x42ee11`–`0x42ee92`「命中即死、不掉血」，**不到**本站点 ⇒ 不掉灵。但全 22 个 ECL 的 `flagSet(...)` 取值普查**没有 2048**（只出现 3/12/13/15/16/32/47/64/76/156/288/303）⇒ 本作 ECL 不用这条路 ✅ |
| 9 | 冻结的敌人 | `0x42dfc5 test [enemy+0x635c],0x2000000` 为真时整帧不推进该敌 ⇒ 伤害本就不发生，本站点自然不触发 |

**累计放现场还是延后？** —— 两者拆开：**累计必须在现场**（`esi` 只在那一瞬可见，且 `ebx`/`esi` 不跨帧有效），**掉落实体必须延后**（帧末 `on_item_frame`）。这是本仓既有的「事件记事实、帧末兑现」模式。

## 7. ❓ 未钉死 / 🟡 推断

| # | 项 | 状态 |
| --- | --- | --- |
| U1 | 符卡路径下「`life.current` 每帧下降多少」| 🟡 `0x42f2da`–`0x42f30c` 是 `(scaled − 7×maximum)/28 + maximum` 的重算，只能保证**会下降**，看不出一帧掉多少 ⇒ 若符卡期节奏不对，需一次实跑记录（打一张符卡，同时记 `hp_cur` 与掉落时刻）|
| U2 | 距离分档的「距离」取自机中心还是判定点 | ❓ 未对。TH13 用 `enemy->+0x44` 与自机坐标（阈值 48/96/192 与 200/400/600/1000 是 ✅ 一手值），TH18 侧建议取 `ce_player_pos()`（中心），首轮照此 |
| U3 | 20 帧冷却 | 🟡 「20」抄自 TH13 一手（`enemy->+0x4004 > 0x13`），**不是** TH18 的观感值 ⇒ 建议做成 `TUNING.md` 旋钮，首轮 20，实跑看密度 |
| U4 | TH18 是否有 boss 出场/退场瞬移导致掉落在屏外 | ❓ 完全未查（要实跑）|
| U5 | 判据窄化 | 🟡 TH13 通道 A 对**所有敌人**生效（这就是 TH13 小怪也掉灵的来源）；本文按用户口径只开到 boss/道中 boss ⇒ 这是**有意的窄化，不是复刻**，代码注释里必须写 `❓ 自定`，别包装成「复刻」|
| U6 | 空闲字段 `enemy+0x6314` 的可用性 | ✅ 静态已查实（0 读 0 写 + 构造 memset）——但要落地成「写引擎内存」仍需 `AUDIT.md` §BE 登记；本报告不推荐走这条路（§5.3 已述）|
| U7 | 实跑 | ❓ 本文**全部**结论均为静态一手，**未实跑** |

## 8. 建议的实跑判据（给实现者）

- 启动日志的断点自检表里必须有 `ce_enemy_damage`；缺 = `FAIL`（照 `sdk.c` 既有风格）。
- 打 1 面**道中 boss**：每掉一颗打一行 `boss-dmg-drop: id=N acc=A thr=T d2=D hp=cur/max frame=F`。
- **反证（必须做）**：打 10 只 `Cir*` 杂鱼应 **0 行**（判据没漏）；打 `GirlD01`（HP 1000 但没调 `setBoss`）也应 **0 行** —— 这一条同时验证「判据不是 HP 阈值」。

## 9. 复现命令

```bash
cd /home/nipporita/renkolab

# 站点四问（长度 / 相对寻址 / 窗内跳转靶 / 指令对齐）
python3 local/_scratch/scan_bp_window.py 0x42f2cb:6 0x42f2b9:7 0x42f314:6

# 关键窗口反汇编（现算，不占 Ghidra 锁）
python3 - <<'PY'
import capstone, importlib.util
spec = importlib.util.spec_from_file_location("pe_read","tooling/pe_read.py")
m = importlib.util.module_from_spec(spec); spec.loader.exec_module(m)
d, base, secs = m.load_sections("local/th18.v1.00a/th18.exe")
md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
code,_ = m.read_va(d, base, secs, 0x42f280, 0x100)
for i in md.disasm(code, 0x42f280):
    print("%06x  %-22s %s %s" % (i.address, i.bytes.hex(), i.mnemonic, i.op_str))
PY

# boss 判据的 ECL 普查（全 22 个 .ecl）
for f in default st01 st01bs st01mbs st02bs st02mbs st03bs st03mbs st04bs st04mbs \
         st05bs st05mbs st06bs st06mbs st07bs st07mbs; do
  local/vendor/thtk/build/thecl/thecl -j -d 18 -m local/vendor/eclmap/eclmap/th18.eclm \
      local/th18.v1.00a/dat/$f.ecl /tmp/e_$f.txt >/dev/null 2>&1
  echo "$f : $(grep -o 'setBoss([^)]*)' /tmp/e_$f.txt | sort | uniq -c | tr '\n' ' ')"
done

# opcode 号反解（分派指针表 0x436438，opcode = 455 + index）
python3 - <<'PY'
import importlib.util, struct
spec = importlib.util.spec_from_file_location("pe_read","tooling/pe_read.py")
m = importlib.util.module_from_spec(spec); spec.loader.exec_module(m)
d, base, secs = m.load_sections("local/th18.v1.00a/th18.exe")
ptrs = struct.unpack_from("<1024I", m.read_va(d,base,secs,0x436438,4096)[0], 0)
for i in (47, 57, 60):
    print("opcode", 455+i, hex(ptrs[i]))   # → 502 flagSet / 512 setBoss / 515 setInvuln
PY
```

## 10. 证据地址总表

| 地址 | 是什么 | 用途 |
| --- | --- | --- |
| `0x42ed40` | `EnemyData::step_game_logic`（ExpHP 名）| 伤害落点所在函数；`ecx`=`zEnemyData` |
| **`0x42f2cb`** | `add [ebx+0x5008],esi` | **推荐站点**（HP 即将减少的唯一汇聚点）|
| `0x42f2b9` | `test byte [ebx+0x5130],0x10` | 备选站点（闸前，含无敌期）|
| `0x42f314` | `sub [ebx+0x4ff4],esi` | 非符卡扣血（不推荐单挂）|
| `0x42f2da`–`0x42f30c` | 符卡路径重算 `life.current` | 说明为什么不能挂在 `0x42f314` |
| `0x42f16e` | `call 0x45f0f0` | 伤害计算处（在站点之前）|
| `0x42fff0` | 唯一调 `0x42ed40` 处 | 每敌每帧一次的证据 |
| `0x42fedd` / `0x42ff52` | `FUN_0042ff80` 的两个调用点（`FUN_0042fe80` 内 if/else）| 每敌每帧一次的证据 |
| `0x45e320` | `PlayerBullet::create` | §2 体检对象 |
| `0x45e7ff` | 既有站点 `ce_bullet_damage_final` | §2 否决对象 |
| `0x45e9ec` | 唯一调 `0x45e320` 处（`Player::shoot_one_bullet`）| 触发频率 = 每颗弹 |
| `0x45e861` | `call 0x45dfa0` 把弹的伤害登记进伤害源池 | 证明“不是命中” |
| `0x433b95` | ECL opcode 512 `setBoss` handler | boss 判据的写点 |
| `0x42d490` | `set_boss_id(slot, enemy)` → `mgr->boss_ids[slot]` | boss 表维护 |
| `0x42eed0` | `test [ecx+0x635c],0x800000` | 引擎自己用 boss 位（血条/相位段）|
| `0x430240` | `Enemy__die`（既有断点 `ce_enemy_death`）| 死亡通道；也用 `enemy+0x1270` 定位 |
| `0x42e208` | `mov [esi+0x67b4],esi`（`zEnemyData.full` 唯一写点）| `[ebx+0x5588]` 恒为 `zEnemy*` |
| `0x42e1e7`–`0x42e1f3` | 构造期 `memset(data,0,0x5600)` | 空闲字段出生必为 0 |
| `0x42e427` | `mov [esi+0x6838],0` | `enemy+0x6838` 是死字段（但已被 110 占用）|
| `0x42dfc5` | `test [ecx+0x635c],0x2000000` | 冻结的敌人整帧不推进 |
| `0x42d8f6` | `or ecx,0x40000000`（`life ≥ 1000`）| 别与 boss 位混淆 |

## 11. 落地时要动的文件（给实现者，含协作登记）

| 文件 | 改什么 | 要不要登 `AUDIT.md` §BE |
| --- | --- | --- |
| `native/sites.py` | `SDK_SITES` 加 `("ce_enemy_damage", 0x42f2cb, 6, "01b308500000", …)`；`emit_header()` 加 `CE_BP_ENEMY_DAMAGE_RVA` | 是（共享件）|
| `native/sites_gen.h` | 生成物 | 是 |
| `native/th18_card_expand.def` | `BP_ce_enemy_damage @32` | 是 |
| `native/Makefile` | 导出行数 31 → 32 | 是 |
| `native/sdk.c` / `sdk.h` / `sdk/public.h` | 新 handler + `ce_hooks_t.on_enemy_damage` + 事件结构；自检表补一行 | 是 |
| `native/cards/trance.c` | 新回调只入队；`on_item_frame` 帧末兑现 | 否（自己的卡）|
| `patch/th18.v1.00a.js` | 生成物 | 是 |
| `TUNING.md` | 冷却 20 / 分档 200-400-600-1000 / 冷却旋钮 | 是 |

> ⚠ 建议站点名**不要**叫 `ce_bullet_damage_final` 的近亲（避免与 §2 那个混淆）；它也不是“bullet”伤害，是**所有**玩家伤害源（含 bomb）的落点。

---

**复核状态**：本文全部结论为静态一手（反汇编 + xref 穷举 + ECL 普查），**未实跑**；派生出实现方案，未改任何既有文件、未动 exe。
