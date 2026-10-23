# 06 · TH18 `enemy+0x6320`：那个「不算真敌人」的字段是 `invuln_timer.current`

> **版本**：TH18 v1.00a（`th18.exe`，imagebase `0x400000`，md5 `9969cac756098c1da05a81de45437a70`）。本文裸地址默认属该版本；引用其他版本须写成 `th16:0x…`。
> **方法**：一手 —— PE 直读（跳表 / 操作码分派表）+ 线性反汇编逐指令（快照 `local/_scratch/th18.dis`，objdump 格式）+
> ExpHP 结构体（`local/vendor/th-re-data/data/th18.v1.00a/`）+ `local/vendor/eclmap/eclmap/th18.eclm` 的 opcode/gvar 名 + 原生 `st0*.ecl` 字节扫描。
> **可信度**：字段定位 / 类型 / xref 穷举 / 四道闸的判据 ✅（逐指令）；「原生脚本不用这个计数器」🟡（负结论，靠字节扫描）；
> 本文全部结论**未实跑**（只到「指令层确证」；需要实跑的是观感类，见 §7）。2026-09-12。

## 0. 一句话结论

`enemy+0x6320` = **`zEnemyData.invuln_timer.current`**（`int32_t`，属于 `struct zTimer` @ `zEnemyData+0x50f0` = `enemy+0x631c`）。

**`> 0` 的两件事是同一根闸**：① 从 ECL 的 `ENM_CNT` 过滤计数与 GUI 的 boss 列表里被排除；② **`life` 不再掉血**
（伤害只累加进 `life.total_damage_including_ignored`）。

⇒ 它对 108 使魔**不是**「可被打但不计数」的杠杆：想让血掉，就一定会被算进 `ENM_CNT`（§5 有反证）。
⇒ 与 `AUDIT.md` 的 AQ7 结论一致；**`docs/superpowers/specs/2026-09-12-youkai-108-design.md` §3.9 的「替代杠杆」应作废**（§6）。

## 1. 字段定位（一手）

| 项 | 值 | 出处 |
| --- | --- | --- |
| 绝对偏移 | `zEnemy+0x6320` | 本文全部 xref |
| `zEnemyData` 内偏移 | `+0x50f4` | ExpHP `zEnemyData.invuln_timer.current` |
| 所属子结构 | `invuln_timer`（`struct zTimer`，0x14 字节）@ `zEnemyData+0x50f0` = `enemy+0x631c` | `local/vendor/th-re-data/data/th18.v1.00a/type-structs-own.json` |
| 类型 / 宽度 | `int32_t`（`zTimer.current`）；同段还有 `prev` / `current_f`(float) | `Timer__set_value` `0x405d10`、`Timer__decrement` `0x409750` |
| 现成名 | `zEnemyData.invuln_timer.current`（ExpHP `type-structs-own.json`）；本仓 `games/th18.v1.00a/symbols.json` 无此字段名 | 两侧都没错，只是我们那层没导出结构体 |

**两处「不同写法」的地址其实是同一个**：

```
zEnemy.data  @ +0x122c                （ExpHP zEnemy.data，engine.h CE_ENEMY_DATA 同值）
0x122c + 0x50f4 = 0x6320              ← 所以 [eax+0x6320] 与 [ebx+0x50f4] 命中同一格
```

**基址分别核实过**：`[ebx+0x50f4]` 的 `ebx = zEnemyData`（`EnemyData__step_game_logic` `0x42ed40` 段内，
`0x42f314 sub [ebx+0x4ff4]` 扣血、`0x42f392 [ebx+0x5130]` 读 flags 交叉确认）；
`[eax+0x6320]` / `[edi+0x6320]` 的 `eax` / `edi` 是 **`zEnemy*`**（都从 `mgr+0x18c` 的链表节点 `[node]` 取出，
`node = enemy+0x1514 = data.node_in_global_storage`）。

`zTimer` 一段（`ce_ztimer_t` 已是本仓 SDK 的既有 typedef，`native/engine.h`）：

| 绝对偏移 | 字段 | 备注 |
| --- | --- | --- |
| `enemy+0x631c` | `prev` | 出生写 `1`（`0x42d8b2`）、构造写 `-1`（`0x42e380`）|
| **`enemy+0x6320`** | **`current`** | 本文主角；出生写 `2`、构造写 `0` |
| `enemy+0x6324` | `current_f` | 出生写 `2.0f`（`0x42d8a8`）★ 递减实际走这一格（见 §4）|
| `enemy+0x6328` | `speed_src` | 0 = 每帧减 `1.0`；`Timer__decrement` 读它选倍率 |
| `enemy+0x632c` | `control` | 位 0 = 「已初始化」（`0x409750` 用它决定是否重置 `speed_src`）|

**邻居（同一段 zTimer 数组，别混）**：`no_hitbox_timer` @ `enemy+0x6330`（ECL opcode **541** `noHitboxDur` 写它，
但本 build **只有递减、无人读** ⇒ 死字段）；`__timer_5118__counts_down` @ `enemy+0x6344` = 命中闪光计时器
（`0x42f33f` / `0x42f355` 写、`0x42f86d` 递减）。

## 2. xref 全表（穷举）

`0x6320` 全文本命中 **4 处**、`0x50f4` **2 处**，合起来就是下表 7 条（含一条 `lea` 到整段 zTimer 的写）。
两处 `lea ...,+0x50f0` / `+0x5104`（`0x433c28` / `0x433c42`）是 ECL 的两个写点，其中 `+0x50f0` 那个就是本字段。

| # | 地址 | 所在函数 | 访问 | 条件 / 后果 |
| --- | --- | --- | --- | --- |
| W1 | `0x42d89e` | `EnemyManager__allocate_new_enemy` | 写 `cur = 2` | 出生自带 **2 帧免伤**，无条件（同帧还把 `prev=1`、`cur_f=2.0f`）|
| W2 | `0x42e36c` | `Enemy__constructor` | 写 `cur = 0` | 构造 / 槽复用清 0（`prev=-1`、`cur_f=0.0f`）|
| W3 | `0x433c28` | `FUN_00430d30`（敌人 ECL 分派器）| `Timer__set_value` `0x405d10` | ECL opcode **515 `setInvuln`**，值 = 指令的 int 操作数 |
| R1 | `0x4301de` | `FUN_0042ff80` | 读 `> 0` → `Timer__decrement` | 每帧末 −1；`<= 0` 直接跳过（不会变负）|
| R2 | `0x42f2c2` | `EnemyData__step_game_logic` | 读 `> 0` → `jg 0x42f31c` | **不扣 `life`**，只累加 `total_damage_including_ignored` |
| R3 | `0x42d460` | `FUN_0042d440`（ECL `ENM_CNT` 计数）| 读 `> 0` → 该敌不计 | `jg 0x42d46d`；与 `flags_lo & 0x31` 并列为两条排除判据 |
| R4 | `0x43c137` | `Gui__on_tick` | 读 `> 0` → `jg 0x43c6d7` | 本帧不进 GUI 的 boss / 血条列表 |

### 2.1 逐条读条件

- **W1 / W2**：出生与构造都没有分支地写（`0x42d881` 那个 `test al,1` 只管要不要顺手重置 `speed_src`）。
  ⇒ 卡侧「写一个大值不管了」不会污染别的敌人：槽被复用时 `Enemy__constructor` 会清 0 ✅。
- **W3**：分派器入口把 `ecx`（`zEnemy*`）`add 0x122c` 变成 `zEnemyData`，该 case 取指令的 int 操作数
  （`EnemyData__get_int_arg__trampoline` `0x438aa0` → `EclRunContext__ecl_get_int_arg` `0x48d4f0`）
  后 `lea ecx,[edi+0x50f0]` 调 `Timer__set_value`，即**原版自己的**「敌人免伤 N 帧」指令。
  opcode 号由 PE 里两层跳表反解（`0x4366f4` 字节表 → `0x436438` 指针表）
  得到 `515`，与 `local/vendor/eclmap/eclmap/th18.eclm:176` 的 `515 setInvuln` **独立对上** ✅。
  同法得到 opcode `541 noHitboxDur` 写 `enemy+0x6330`（`0x433c42`）。
- **R1**：`0x4301de cmp [esi+0x50f4],0` / `jle` 跳过，否则 `lea ecx,[esi+0x50f0]; call 0x409750`。
  调用链：`EnemyManager__on_tick` `0x42df50`（`0x42dfd1`）→ `FUN_0042fe80` → `FUN_0042ff80`；
  而 `FUN_0042ff80` **先**（`0x42fff0`）调 `EnemyData__step_game_logic`（伤害 / ECL），**后**才走计时器。
  ⇒ 同一帧里 R2 看到的是**递减前**的值 ✅（§4 的写法依赖这一点）。
- **R2**：伤害段完整顺序见 §3.2；`jg 0x42f31c` 的后继是 `add [ebx+0x5008],esi`，然后**照常**走命中闪光与死亡判定。
- **R3**：`FUN_0042d440` 遍历 `mgr+0x18c` 链表（`node+0` = `zEnemy*`、`node+4` = next），
  对每个敌人判 `flags_lo & 0x31` 与 `invuln_timer`，两者都干净才 `count++`（`0x42d472 cmovne`）。
  ExpHP 没给它命名；它是 **ECL `ENM_CNT` 的实现**（见 §3.3 的 id 反解）。
- **R4**：`Gui__on_tick` 的第二段（`0x43c0e0` 起）按 `mgr->boss_ids[4]`（`mgr+0x48`）找敌人，
  再依次要求 `life.current < 0x186a0`（100000）、`!(flags_lo & 0x31)`、`invuln_timer <= 0`、
  `this+0x1b0 == 0`，才把它填进 GUI 的 per-boss 条目（活力度 `life.current / life.maximum`）。
  `this+0x1b0` 按 ExpHP 是 `zGui.msg` 🟡（他的 `zGui` 定义不全）；**判据本身是「非 0 就跳过」**，
  语义上 5 处消费者（`0x408bf9` / `0x420441` / `0x43c144` / `0x45c069` / `0x45c92d`）都把它当「对话中」用。

> ⚠ **坑**：`FUN_0042d5b0` 的基址链是 `GUI_PTR`(`0x4cf2e0`) `→ +0x1b0 → +0x198`，**不是**敌人管理器；
> 按 ExpHP 结构体那是 `zGui.msg` 指向对象的 `__dword_incremented_by_enemyAppear`（对话期敌人出场计数）🟡，
> **不是** `enemy_count_real`。两个 `+0x198` 长得一样、意义无关；
> 真读 `enemy_count_real` 的写法是 `[[0x4cf2d0]+0x198]`（`ENEMY_MANAGER_PTR`，`0x4cf2d0`）。

## 3. 语义

### 3.1 一句话

**置位（`> 0`）= 这个敌人「在计数与 GUI 眼里不存在」，同时「打上去不掉血」；清零 = 一切照旧。**
它是个**递减计时器**（帧数），不是布尔开关：原生写点只有出生 `2` 帧与 ECL `setInvuln`，之后每帧 −1。

### 3.2 置位后还吃不吃自机子弹？—— 判定链逐层看

自机弹 = 伤害源池里的一条长寿命记录（[`../../player/th18/02-damage-sources.md`](../../player/th18/02-damage-sources.md) §1），
敌人每帧自己扫池。`EnemyData__step_game_logic` `0x42ed40` 的伤害段：

| 步 | 地址 | 判据 | `invuln > 0` 时 |
| --- | --- | --- | --- |
| ① flags 首闸 | `0x42f101` | `test cl,0x21`（bit0 / bit5）| 通过（本字段不碰 `flags_lo`）|
| ② 受击盒 | `0x42f10a` | `hurtbox_size.x > 0` | 通过 |
| ③ 判重叠 + 累加 + 计分 | `0x42f16e` `call 0x45f0f0` | 池遍历、OBB 判重叠、`SCORE += (total/10+10)/10`（`0x45f2a2`）、写「一源一敌一辈子一次」tag（`0x45f45a`）| **照常执行** |
| ④ 忽略闸 | `0x42f2b9` / `0x42f2c2` | `flags_lo & 0x10` 或 `invuln > 0` ⇒ `jg 0x42f31c` | **命中，但不扣 `life`** |
| ⑤ 记账 | `0x42f31c` / `0x42f2cb` | `total_damage_including_ignored += dmg` | 两条路都加（`enemy+0x6234`）|
| ⑥ 命中闪光 / 音效 | `0x42f322` | 设 `0x5118` / `0x511c` / `0x5120` 计时器 | 两条路都跑 |
| ⑦ 死亡判定 | `0x42f392` | `life.current <= 0 && !(flags_lo & 0x80)` → `Enemy__die` `0x430240` | `life` 没变 ⇒ 不触发 |

⇒ **结论**：置位后「打到」这件事**仍然发生**（判重叠、吃 tag、计分、出命中特效、进 `total_damage_including_ignored`），
**唯一被掐掉的是 `life` 的扣除**。对 108 来说这正好是要命的地方：转嫁触发量取自 `life.current` 的逐帧差分
（spec §3.7），而那一格永远不会变 ⇒ **转嫁恒为 0**。

> 顺带（对转嫁路线有用的副产品）：`life.total_damage_including_ignored`（`enemy+0x6234`）在**两条路**上都累加，
> 且只在构造时清 0（`0x42e486`）⇒ 它是「这个敌人这一辈子被打进去多少（含被忽略的）」的通用计数器。
> `AUDIT.md` AQ7 的否决理由（「设正值 = 使魔无敌」）在效果上成立，但措辞可以再准确一档：不是打不到，是打不掉血。

### 3.3 置位后还计不计入敌人计数 / 波次 / 掉落 / 得分？

先把「敌人数」拆开——TH18 有**两个**不同的计数，别混：

| 名字 | 实现 | 谁读 |
| --- | --- | --- |
| `ENM_CNT`（ECL gvar id `-9908`）| `FUN_0042d440`：现算，遍历链表并**过滤** | ECL `Enemy::ecl_get_int_global` `0x437360` / `..._float_global` `0x437c90` |
| `ENM_CNT_REAL`（ECL gvar id `-9946`）| `zEnemyManager+0x198`，`0x42d9ef` 出生 `inc` / 析构 `dec` | 同一对 getter + 原生卡 + 炸弹门（见下表）|

id → 实现是**独立对上的**：`0x437360` 内 `add eax,0x2710; jmp [eax*4+0x437978]`，直读 PE 跳表，
`idx 0x5c`（= id `-9908`）落在 `0x4375b9`（`call 0x42d440`），`idx 0x36`（= id `-9946`）落在 `0x4375a8`
（读 `[mgr+0x198]`）；名字取自 `local/vendor/eclmap/eclmap/th18.eclm` 的 `!gvar_names` ✅。

| 消费者 | 判据 | 使魔置位（`invuln > 0`）时 |
| --- | --- | --- |
| ECL `ENM_CNT` | `FUN_0042d440` 的过滤 | **不计**（R3）|
| ECL `ENM_CNT_REAL` | `mgr+0x198`（出生 `inc`，与 flags 无关，`0x42d9ef`）| **照计**，且析构才 `dec`（`0x42d2e8`）|
| GUI boss / 血条列表 | `Gui__on_tick` `0x43c137` | **不显示** |
| 炸弹 / 决死门 | `Bomb__can_bomb_and_deathbomb_check` `0x420453`、`Player__on_tick__body` `0x45c07b`：`enemy_count_real != 0` 才允许 | **照计** ⇒ 空场也能放炸弹（不希望）|
| 原生卡内部门 | `CardClownpiece::__on_tick_2` `0x40de3f` 一族、`FUN_00408bf0` `0x408c0b`：`enemy_count_real == 0` 才动作 | **照计** ⇒ 那些卡认为「还有敌人」|
| ECL 生成上限 | `FUN_004369e0` `0x4369ec`：`enemy_count_real >= mgr+0x8c` ⇒ 拒绝生成 | 照计（但 `mgr+0x8c` 建管理器时 = `99999`，实际打不到 🟡）|
| 掉落 | 只在 `Enemy__die` `0x430240` 里撒（`0x430469` 读 `enemy+0x623c`）| 不掉血就不死 ⇒ **不撒**（要的就是这个）|
| 得分 | `enm_compute_damage_sources` 内 `0x45f2a2`（在忽略闸**之前**）| **照常计分**（每次命中给个位数分）|

**关于「波次推进」**：原生 TH18 v1.00a 的剧本**一次都没读过这两个计数器**——
对 `local/th18.v1.00a/dat/*.ecl`（`st01`–`st07` + `bs`/`mbs` + `default.ecl`，共 22 个文件）
按 `!gvar_names` 的 101 个 id 逐个做 int32-LE 字节命中统计：`ENM_CNT` **0** 次、`ENM_CNT_REAL` **0** 次
（对照组：`TIMEOUT` 154、`CAPTURE` 81、`MISS_COUNT` 68 ⇒ 编码方式由对照组坐实）。
⇒ 使魔污染 `ENM_CNT` 这件事**在原生剧本上没有后果** 🟡（负结论，字节扫描是代理证据）。
真正会被使魔影响的消费者是上表里 `enemy_count_real` 那一组（炸弹门 / 原生卡门 / 生成上限）。

## 4. 可操作性：卡侧怎么写

⚠ **不能只写 `+0x6320` 一格**。递减走的是 `current_f`：`Timer__decrement` `0x409750` 是
`cur_f -= 1.0f; cur = (int)cur_f`，而守卫（R1）只看 `cur > 0`。只写 `cur = 5` 而 `cur_f` 还是 `0.0f`，
下一帧就 `cur = -1`（一次塌掉）。**要整段写，或直接调 `Timer__set_value` `0x405d10`**（原版 ECL opcode 515 用的就是它）。

`ce_spawn_enemy` 返回的就是 `zEnemy*`（`native/sdk.h`，底层 `CE_FN_ALLOCATE_NEW_ENEMY` `0x42d7d0`），所以：

```c
/* 照抄写法（每帧在 on_tick_2 里对每个使魔来一次；优先级 0x16 早于敌人结算 0x1b）：
 *   set_value 会写 prev = n-1 / cur = n / cur_f = (float)n，并把 speed_src 归 0（每帧减 1.0）。*/
typedef void (__thiscall *yk_fn_timer_set_t)(void *timer_ecx, int value);
#define YK_FN_TIMER_SET_VALUE  ((yk_fn_timer_set_t)0x405d10)
#define YK_ED_INVULN_TIMER     0x631c      /* zEnemyData+0x50f0；+0x0004 = current = enemy+0x6320 */

YK_FN_TIMER_SET_VALUE((uint8_t *)f + YK_ED_INVULN_TIMER, 2);   /* 2 帧足够：本回调每帧都跑 */
```

等价的裸写法（不新增函数指针，风格与 `youkai.c` 现有的 `*(int32_t *)(f + YK_ED_LIFE_CUR)` 一致）：

```c
int32_t *t = (int32_t *)((uint8_t *)f + 0x631c);   /* prev, cur, cur_f, speed_src, control */
t[0] = 1; t[1] = 2; *(float *)&t[2] = 2.0f; t[3] = 0; t[4] |= 1u;
```

- **收尾**：`on_delete` / `on_run_reset` 不用还原这一格——退役走静默撤退（`flags_lo |= 0x2000000`，`0x42d200`）
  或让引擎析构，析构/复用时的 `Enemy__constructor` 会写 0（W2）✅。
- **想少写一次**：可以只在「值 `<= 2`」时补写（读 `t[1]`），代价是多一次读；108 的使魔数 ≤ 32，两种都便宜。
- **顺带**：正因为置位后 `life` 不动，**「被打了多少」要改读 `enemy+0x6234`**
  （`zEnemyData+0x5008 life.total_damage_including_ignored`，两条路都累加、构造时清 0）：
  `int32_t taken = *(int32_t *)(f + 0x6234);` 逐帧差分 × 1/2 ⇒ 与 spec §3.7 想要的量**逐值等价**，
  且这条路的敌人天然不进 `ENM_CNT`、不进 GUI boss 列表。
- 若只需要「忽略伤害」这一半语义（不在乎 `ENM_CNT` 的实时扣减），`flags_lo` bit4 一次写入即可：
  `*(uint32_t *)(f + CE_ENEMY_FLAGS) |= 0x10;`（同一个忽略闸 `0x42f2b9`，也在排除掩码 `0x31` 里）。

## 5. 反面排查：没有「可掉血 + 不计数」的 `zEnemy` 字段

### 5.1 反证（集合关系，不靠穷举）

- **掉血**需要同时：`D1 !(flags_lo & 0x21)`（`0x42f101`）∧ `D2 hurtbox.x > 0`（`0x42f10a`）
  ∧ `D3 !(flags_lo & 0x10)`（`0x42f2b9`）∧ `D4 invuln_timer <= 0`（`0x42f2c2`）。
- **被 `ENM_CNT` 计数**需要同时：`C1 !(flags_lo & 0x31)`（`0x42d457`）∧ `C2 invuln_timer <= 0`（`0x42d460`）。
- 两个掩码覆盖同一组位（`0x31 = 0x01|0x10|0x20`、`0x21|0x10 = 0x31`）⇒ **`C1 ≡ D1 ∧ D3`**；又 `C2 ≡ D4`。
- ⇒ 「不被计数」⇒ `flags_lo & 0x31 ≠ 0` **或** `invuln > 0` ⇒ `¬(D1 ∧ D3)` 或 `¬D4` ⇒ 掉血三条件必有其一不成立。
- **QED：在这个结构里，「能掉血的命中」⇒「被计入 `ENM_CNT`」。两者共用同一对闸。**

### 5.2 逐个候选（都在一手地址上判死）

| 候选 | 机制 | 判定 |
| --- | --- | --- |
| `flags_lo` bit0 `0x1` | 断在伤害段第①道闸（`0x42f101 test 0x21`）| ❌ 完全打不到 |
| `flags_lo` bit5 `0x20` | 同上，且进锁定黑名单（`CE_ENEMY_FLAG_NO_LOCK` `0x0c000021`）| ❌ 打不到 + 不可锁定 |
| `flags_lo` bit4 `0x10` | 排除计数（`test 0x31`）+ 只累加忽略伤害（`0x42f2b9`）| ⭕ 等价于 `invuln > 0`：命中算、血不掉 |
| `invuln_timer > 0`（本文主角）| 同上 + 每帧递减 | ⭕ 同上；多两条原生写点（出生 2 帧 / ECL 515）|
| `no_hitbox_timer`（`zTimer` @ `enemy+0x6330`）| ECL opcode 541 `noHitboxDur` 写 `.current`（`+0x6334` = `zEnemyData+0x5108`）| **✅ 有读者**（旧结论订正，见下注）：`0x42f51c` 是体术段前置闸 ⇒ 不是死字段 |
| `flags_hi` bit2 `enemy+0x6360` | 析构时不 `dec enemy_count_real`（`0x42d2da`）| ❌ 出生照样 `inc`（`0x42d9ef`）⇒ 只会泄漏计数；且全 exe 无人写这一位 🟡 |
| 摘 `node_in_global_storage` `enemy+0x1514` | 从 `mgr+0x18c` 摘链 | ❌ tick 与计数共用这条链（`0x42df99`）⇒ 摘了就不 tick |
| `hurtbox_size = 0`（`enemy+0x133c`）| 受击盒关掉（`0x42f10a`）| ❌ 打不到 |
| `spawn+0x14 >= 1000` | 置 `flags_lo` bit30 进锁定黑名单（`0x42d8f6`）| ❌ 不可锁定 |

> **★ 2026-10-20 订正（`no_hitbox_timer` 不是死字段）**：读者就是
> `0x42f51c cmp dword ptr [ebx+0x5108],0 / jg 0x42f69f` —— `EnemyData__step_game_logic 0x42ed40`
> **体术段的前置闸**（`ebx = zEnemyData`），`> 0` ⇒ 整段接触致伤（含三个几何判定）跳过，
> 而它**不在**伤害段的两道闸（`0x42f101 test cl,0x21` / `0x42f2b9 test cl,0x10`）里 ⇒
> 「无体术、但照样可被打中 / 可锁定 / 计数照常」的正解杠杆。⚠ 写 `.current` **只管当帧**：
> 每帧 `0x4301f3` 会调 `Timer__decrement 0x409750`，而它是
> `current = (int)(current_f - speed)` 重算出来的（`current_f` = `+0x6338`）⇒ 要跨帧必须两格一起写。

### 5.3 真正的落点

1. **计数侧**：不是找一个「隐身」位，而是承认「掉血的敌人一定进 `ENM_CNT`」；而 `ENM_CNT` 在原生剧本里
   **无人使用**（§3.3），所以「不影响波次推进」这个要求在原生数据上本就成立；
   真正要盯的是 `enemy_count_real` 那一组消费者（炸弹门 `0x420453` / 原生卡门 / GUI boss 列表）。
2. **信号侧**：把转嫁触发量从 `life.current`（`enemy+0x6220`）换成
   `life.total_damage_including_ignored`（`enemy+0x6234`），配合 `invuln > 0` 或 `flags_lo |= 0x10`
   ⇒ 同时拿到「不进 `ENM_CNT`、不上 boss 列表、命中仍计分出特效、被打量可读」。
   ⚠ 代价：`+0x6234` 是**累计**量（构造清 0），必须差分读，不能当瞬时值。
3. 若坚持「`life` 真的掉」⇒ 使魔必然进 `enemy_count_real`（无解），此时唯一的护栏是卡侧自己限制
   「使魔只在与真实敌人同场时存在」，以及接受炸弹门被放开。

## 6. 对既有文档的勘误（不改别人的文件，交主 agent 处理）

| 位置 | 现状 | 应改为 |
| --- | --- | --- |
| `docs/superpowers/specs/2026-09-12-youkai-108-design.md` §3.9（`+0x6320` 的「★ 替代杠杆」段）| 🟡 待验，称「设正值 ⇒ 既排除又不碰 bit0/bit5 ⇒ 可被打」| ❌ **作废**：它是 `invuln_timer.current`，置位后 `life` 不掉 ⇒ 转嫁恒 0（本文 §3.2 / §5.1）|
| 同上 §10 第 ⑦ 行 | 「🟡 待一手确认字段语义」| ✅ 已确认（本文），结论 = 不采用 |
| `mods/th18.v1.00a/card-expand/AUDIT.md` AQ7 | ✅ 一手，理由 =「设正值 = 使魔无敌」| 结论 ✅ 保留；理由可精确成「命中仍登记/计分，但不扣 `life` ⇒ 差分转嫁为 0」|
| `local/_scratch/research-108-damage-transfer.md` §2.1 表 | `invuln_timer.current` = `enemy+0x6320` ✅ | 结论正确，已由 ExpHP 结构体（`+0x50f4`）+ 偏移加法二次确认 |
| 同上 §2.3 末 | 「计数经 `+0x19c` 落到 `+0x198`，写点 `0x43bbd8`」| ❌ 写点不对：`0x43bbd8` 是 `Gui::on_tick` 里 **Gui 自己**的 `+0x198/+0x19c`；`enemy_count_real` 的真写点是 `0x42d9ef`(`inc`) / `0x42d2e8`(`dec`) |

## 7. ❓ 未决项

| # | 事项 | 说明 |
| --- | --- | --- |
| 1 | 未实跑 | 本文全部结论是**指令层**；「命中闪光确实出现」「转嫁差分等价」需要一局实测（打使魔 → 日志对账 boss 掉血）|
| 2 | 原生剧本扫描的编码假设 | `ENM_CNT` 0 命中依赖「gvar 以 int32-LE 编码」；对照组（`MISS_COUNT` 68）支持它，但没解出读操作数的通用例程来正面证明 |
| 3 | `Gui__on_tick` 那段列表的确切 UI 用途 | 已定：按 `mgr->boss_ids[4]` 逐 boss 填活力度条目；未确认它画的是血条还是符卡名条（不影响判据）|
| 4 | `mgr+0x8c enemy_limit` 是否真的恒 `99999` | 只在 `EnemyManager__operator_new` `0x42dd9c` 找到写点；若某关会改它，「使魔吃生成上限」的后果需要重估 |
| 5 | `flags_hi` bit2 是否真的无写点 | 全文本 `+0x6360` 仅 4 处（2 读 bit2 + 2 读写 bit0），未见 `lea ...,+0x635c/+0x5130` 形式的计算基址；理论上仍可能被别的寻址方式写入 |
