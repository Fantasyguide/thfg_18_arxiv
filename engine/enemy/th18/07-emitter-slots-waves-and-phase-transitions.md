# 07 · TH18 敌人发射口（`props[]`）的「波次」与 boss 换阶段：引擎到底做了什么

> **版本**：TH18 v1.00a（`th18.exe`，imagebase `0x400000`，md5 `9969cac756098c1da05a81de45437a70`）。本文裸地址默认属该版本；引用其他版本须写成 `th16:0x…`。
> **方法**：一手 —— ① PE 直读，自己解 ECL opcode 分派表（`0x4366f4` 组号字节表 + `0x436438` 组跳表）把 opcode 号钉到 handler；
> ② `objdump -d --start-address/--stop-address` 逐指令读 handler；
> ③ 原生 ECL 反汇编（`thecl -d 18 -m th18.eclm` 出的 **22 份**：`default.ecl` + `st01..st07` × {stage, bs, mbs}，本机在 `local/_scratch/*.asm`）
   做**全量计数**（不抽样；这 22 份 = `local/th18.v1.00a/dat/` 里 ECL 的**全集**，已逐一对应过文件名）；
> ④ 名表 `local/vendor/eclmap/eclmap/th18.eclm`（opcode 名）。
> **可信度**：opcode↔handler、字段写点、`enmKillAll` 的实现与过滤 ✅（逐指令）；「波次同帧」✅（226/226 全量）；
> §5 的因果链 = 🟢 机制 + 🟢 触发 + 🟡 稳态（稳态那一格要一次实跑，见 §6）。
> **起因**：卡 108（永夜抄，`mods/th18.v1.00a/card-expand/native/cards/youkai.c`）owner 报「只要击破 boss 第一个阶段，后面就再也不发弹」。
> 本文只放**一手事实**与由此得到的因果链；108 侧的修法约束见 §7。相关：[`01-boss-interrupts-and-spellcard.md`](../../ecl/th18/01-boss-interrupts-and-spellcard.md)。

## 0. 一句话结论

1. **「波次」在程序上不是一个回调，是一段 ECL 指令**：`etNew(i)`（把整个 `props[i]` 清零并写默认值）→
   一连串 `etSprite/etCount/etEx/etAngle/etSpeed…` → `etOn(i)`（发弹）。**`etNew` 与同一槽的 `etSprite` 永远在同一帧里**
   （22 份原生 ECL、**282 处 `etNew` 全量**核对：226 处字面槽 + 56 处变量槽 `etNew($A)`，
   同槽 `etSprite` 都在**任何 `wait()` 之前**，**0 例外**）⇒ 外部**每帧采样一次**的观察者
   （卡 tick 优先级 `0x16`，敌人 ECL `0x1b`）**看不到 `props[i].type == 0` 的那一瞬**。
2. **boss「换阶段」不换敌人**：阶段是 `zEnemyData.interrupts[8]` 里的一格（`enemy+0x6374`，stride `0x88`），
   血量掉到 `hp_value` 或 `time_in_ecl` 到点，引擎 `EnemyData__step_game_logic` `0x42ed40` 就把 `life.current`
   **钉到阈值**并跑 `sub_life` / `sub_timeout` 子程序（一手全文见上引 01 篇）。⇒ **boss 的 `enemy_id` 整场不变**。
3. **每一段攻击的开头都清场**：原生 ECL 在 `Boss2` / `BossCard1` / `BossCard2` / `BossDead` 这些**段首**都写着
   `killAllAsync(); enmKillAll();`（`enmKillAll` = opcode `525`/`0x20d`，handler `0x434e1b` → `0x430710`），
   而 `0x430710` 会把场上**几乎所有敌人**（含我们卡侧自建的使魔）交给 `Enemy__die` `0x430240` 杀掉。
   ⇒ 「击破 boss 第一阶段」这一下，**恰好是全场的使魔被引擎一次性清掉的时刻**。

## 1. 发射口槽：数据布局与「活跃」判据（一手）

| 项 | 值 | 证据 |
| --- | --- | --- |
| 数组 | `zEnemyData.bullet_props[0x10]`，stride **`0x488`** | ExpHP `local/vendor/th-re-data/data/th18.v1.00a/type-structs-own.json`（`zEnemyBulletShooter`）|
| 绝对偏移 | `zEnemy+0x122c+0x598` = **`zEnemy+0x17c4`**（`+ i*0x488`）| `0x433ff4 mov [esi+0x5a8],eax` / `0x43404e mov [esi+edi*1+0x598],eax`（`esi = i*0x488`、`edi = &data`）|
| `props[i]+0x00` | `type`（int32）—— 卡 108 用来判「这个发射口活跃」 | `0x43404e` 写 = `etSprite` 的 arg1 |
| `props[i]+0x04` | `color`（int32） = `etSprite` 的 arg2 | `0x43405a` |
| 清零点 | `etNew`（opcode **600** / `0x258`，handler `0x433dde`）：`memset(props[i],0,0x488)` + 默认值 `+0x14=0`、`+0x1c=2.0f`、`+0x46c=0x00010001`、`+0x474=0x23`、`+0x478=0x15`、`+0x47c=0x26` | handler 逐指令；同结论见 `local/_scratch/research-108-shot-slot-off.md` §1.1 |
| 发弹点 | `etOn`（opcode **601** / `0x259`，handler `0x433f34`）在 `0x433ffa` 汇聚：设保护半径 → `call 0x427810(&props[i])` → 复位 | 同上 |
| 整槽拷贝 | `etCopy`（opcode **614** / `0x266`，handler `0x433ea2`）：`rep movs` 0x122 dword | 同上；**全量只有 2 处使用**（见 §2）|

**opcode 号是独立复核过的**：我用 `0x4366f4` 的组号字节表 + `0x436438` 的 175 项组跳表自己解了一遍，
`0x258→0x433dde`、`0x259→0x433f34`、`0x25a→0x434032`、`0x266→0x433ea2` 与
`local/_scratch/research-108-shot-slot-off.md` §1.1 的表**逐条相同**（两条独立路径）。

## 2. 波次 = `etNew(i)` + 配置 + `etOn(i)`，**同帧**（全量 226/226）

### 2.1 发现（一手，可复算）

对 22 份原生 ECL 反汇编（`local/_scratch/st01..st07{,bs,mbs}.asm` + `default.asm`；= `dat/` 里 ECL 的全集）做全量统计：

| 量 | 数 | 判据 |
| --- | --- | --- |
| `etNew(slot)` 出现处 | **282**（226 处字面槽 `etNew(0)` + **56 处变量槽 `etNew($A)`**）| `^\s*etNew\((.*?)\);` |
| 其中「**同一槽**的 `etSprite(slot, …)` 出现在**任何 `wait()` 之前**」 | **282（100%，0 例外）** | 从 `etNew` 往下扫到第一个 `wait(` 或第一个 `etSprite(`，再比槽参数是否同一个（字面量或同一个变量名）|
| 其中「`wait()` 出现在 `etSprite` 之前」 | **0** | 同上 |
| `etSprite` 出现处 | 221（槽分布 0:164 / 1:38 / 2:12 / 3:5 / 4:1 / 9:1）| `^\s*etSprite\(` |
| 其中 type 参数是 **0**（= 显式「拆发射口」）| **2**（`st07bs.asm:1530` `etSprite(0,0,8)`、`st07bs.asm:1846` `etSprite(1,0,2)`）| 同上 |
| `etCopy` | **2** | `^\s*etCopy\(` |

典型形状（`local/_scratch/st01bs.asm`，`Boss1_at` / `BossCard1_at2` / `BossCard1_at_et` 三处各代表一类）：

```
Boss1_at_et(%A,%B):            # 每波**只**重写
    etExSet(0,1,1,4,60,-9999994, 0.08f+(%RANDF2*0.03f), %A+(%RANDF2*0.5235988f));
    etSprite(0, 7, $B);        # ← type 每波被重写一次（没有 etNew）
    etAngle(0, …, 0.17453292f);
    etOn(0);                   # ← 同一条子程序里发弹
```

```
BossCard1_at2():               # 一波一把 etNew
    etNew(1); etAim(1,3); etSprite(1,4,6);   # ← 相邻三行，同一帧
    …
    goto loop;
loop:
    circlePos(%A,%B,%RANDRAD,%RANDF*_f(128));
    etOffset(1,%A,%B*0.4f);    # ← 之后 6000 次循环里**只有 etOffset/etOn**，不再 etNew/etSprite
    etOn(1);
    wait($D);
```

### 2.2 结论（这一条是 108 那个 bug 的地基）

- **「ECL 是否重新武装了这一槽」在外部不可观测**：`etNew`（清零）与 `etSprite`（写回 `type != 0`）
  在同一帧内首尾相接 ⇒ 观察者每帧读一次，读到的**永远是「武装好的」那一态**。
  若观察者在卡的 tick（`AbilityManager` 节点优先级 `0x16`）里读，且敌人 ECL 在 `0x1b`（
  [`AUDIT.md`](../../../mods/th18.v1.00a/card-expand/AUDIT.md) AA6 行的一手时序），
  那么**连同一帧内的先后都无关紧要**：跨帧采样点之间不存在 `type == 0` 的帧。
- ⇒ **`type` 的 0→1 边沿只在两种情形下真的出现**：① 该槽在某帧走 `etNew` 却在**下一帧或更晚**才 `etSprite`
  （全量 0 例）；② 该槽被 `etSprite(i, 0, …)` 显式拆掉（全量 2 例）或被 `etCopy` 覆盖（全量 2 例）之后再武装。
- ⇒ 结论 ✅：**「波次」不是一个能被每帧采样观察到的信号**；要它精确，只能在**引擎自己的指令处**拿事件
  （`etNew` 的 handler `0x433dde`），或退而求其次用「`etOn` 被调用」当波次代理（§7）。

## 3. boss「阶段」：中断槽，不换敌人（一手）

- 阶段推进 = `zEnemyData.interrupts[8]`（`zEnemyData+0x5148` = `enemy+0x6374`，stride `0x88`：
  `+0 hp_value`（-1 = 空）、`+4 time`、`+8 sub_life[]`、`+0x48 sub_timeout[]`），
  每帧由 `EnemyData__step_game_logic` `0x42ed40` 判：血量 ≤ `hp_value` 走 `sub_life`，
  `time_in_ecl` 到点走 `sub_timeout`；两条路都**先把 `life.current` 钉到 `hp_value`**，再跑子程序。
  全文与地址见 [`01-boss-interrupts-and-spellcard.md`](../../ecl/th18/01-boss-interrupts-and-spellcard.md) §1–§2。
- ⇒ **「击破一个阶段」= 血量穿过中断槽阈值**（不是敌人死亡）⇒ 敌人对象存活、**`enemy_id` 整场不变**。
  原生 ECL 一侧的形状（`local/_scratch/st01bs.asm`）：

```
void Boss()                    # ← 整场只有这一个敌人脚本线程
{   setBoss(0); … lifeSet(14100); … lifeSet(2100); @BossCard1(); … @Boss1(); delete(); }
void Boss1()  { … setInterrupt(0, 2100, 2100, "BossCard1"); lifeMarker(0, 2100.0f, -24448); … }
void BossCard1(){ … setInterrupt(0, 0, 2100, "Boss2"); … }        # 击破 = life ≤ 0
void Boss2_220: { timerReset(); killAllAsync(); enmKillAll(); … } # ★ 每一段的段首
```

- `setBoss` 在 `st01bs.asm` 里**只出现一次**（`Boss()` 开头）⇒「一个 boss = 一个敌人」这一条对本战成立。

## 4. 段首的 `enmKillAll()` 会杀掉谁（一手）

| 项 | 值 | 证据 |
| --- | --- | --- |
| ECL | `enmKillAll` = opcode **525**（`0x20d`）| `local/vendor/eclmap/eclmap/th18.eclm:186` |
| handler | `0x434e1b`（分派表解出）→ `mov ecx,[0x4cf2d0]`（`zEnemyManager*`）→ `call 0x430710` | `0x434e1b`–`0x434e26` |
| 实现 | `0x430710`：遍历 `mgr+0x18c` 的活动敌人链表，逐个读 `enemy+0x635c`（`flags_lo`）| `0x430720`–`0x43073b` |
| 过滤 | `test eax,0xc004a0`：**掩码为 0 ⇒ 杀**；非 0 且 `0x100` 未置 ⇒ 跳过；非 0 且 `0x100` 置 ⇒ 杀 | `0x430741`–`0x43074d` |
| 杀法 | `call 0x430240`（`Enemy__die`）⇒ 卡侧收到死亡广播，`retaddr 0x4307f1` = `CE_EDEATH_KILL_ALL`（值 **4**）| `0x4307ec`；SDK 的 `sdk.c` `edeath_is_known_call_site` |
| 使用频度 | 22 份原生 ECL 里 **110 处**，且集中在**段首**：`st01bs.asm:177`（`Boss2_220`）、`:310`（`BossCard1_180`）、`:515`（`BossCard2_180`）、`:708`（`BossDead_316`）、`:726`（`BossEscape`）| `grep -c enmKillAll` |

**卡 108 自建的使魔会不会被杀**：使魔 = `ce_spawn_enemy("NoEnemyDebug", spawn)`，
`EnemyManager__allocate_new_enemy` `0x42d7d0` 只按 spawn 参数写 `flags_lo`
的 **`0x80000`**（`CE_SPAWN_FLAG_A` 位 0）/ **`0x4000000`**（`FLAG_B` 位 0）/ **`0x40000000`**（life ≥ 1000），
而 `youkai.c` 的 `yk_spawn_familiar` 把这两个 flag 都填 0 ⇒ **`flags_lo & 0xc004a0 == 0` ⇒ 必被杀** ✅
（证据：`0x42d83f`/`0x42d8c2` 两处 `xor/and 0x80000 / 0x4000000`，`0x42d8f6` 的 `0x40000000`）。

## 5. 因果链：这条机制怎么把 108 的「发射口闸」夹死

108 的闸（`youkai_core.c` 的 `yk_shot_slot_suppressed`）判据是
「这个 `(enemy_id, slot)` **曾经有我们的使魔、现在回查不到**」⇒ 在 `0x433ffa` 站点返回 1 ⇒ 该槽本帧不发弹。
它的**复位口**只有三个：

| 复位口 | 实装处 | 在实战里发生了什么 |
| --- | --- | --- |
| `props[i].type` 的 0→1 边沿（「ECL 重新武装」）| `yk_note_slot` 的 `active && !was_active` 分支 | **结构上观测不到**（§2）⇒ 实战里基本不发生 |
| `YK_EMIT_BROKEN_TTL`=120 帧的到期自愈 | `yk_note_slot` 的 `no_fam_frames` 计数 | 会发生；但自愈的那一次调用**同帧就会把使魔重建**（`active==1` 且 `broken` 刚被清）|
| `yk_map_reset`（删卡 / 松低速 / 局末 / 断链）| `yk_drop_all` | 只在卡退场时发生 |

**链**（🟢 前四步都是一手；最后一步是稳态，标 🟡）：

1. 进 boss 战，发射口武装（`etNew`+`etSprite`）⇒ 108 按 1:1 映射建使魔（`fam_seen=1`）。
2. **击破第一个阶段** ⇒ 下一段的段首先跑 `enmKillAll()`（§3/§4）⇒ 使魔被 `Enemy__die` 杀，
   死亡广播 `kind = KILL_ALL(4)` ⇒ `yk_fam_broken` 把该槽置 `broken=1`、清 `fam_id`、留 `fam_seen=1`。
3. 闸从此夹住：判据成立 ⇒ 该槽每一帧的 `etOn` 都被跳过（本帧这一槽不发弹）。
4. 由于 `broken=1`，1:1 映射**不再重建**使魔；而 ECL 的「重新武装」观测不到（§2）⇒
   **唯一的出口是 120 帧 TTL**。
5. 120 帧后自愈：清 `fam_seen`/`broken`，同帧按 `active==1` 重建使魔（`YK_FAM_LIFE = 1` 的占位值，
   钉在发射口上）⇒ **它再被打死一次，循环回到第 3 步**。
   ⇒ 稳态 = **每 120 帧里只有 1–2 帧在发弹**（≈1.7% 占空比），玩家侧看到的就是「boss 再也不发弹」。
   🟡 这一步只差一次实跑确认（第 5 步里「重建出来的使魔会再死一次」是这条链唯一需要现场验证的环节）——
   观察点见 §6。**注意它同时解释了 owner 的「这不是 120 帧的那种问题」：改 TTL 只改「每次死亡沉默多久」，
   不改「会再死一次」这个循环。**

### 5.1 一条可用的交叉判据：条目退休会**治好**这个夹死

108 的槽表条目在「本帧没被 `CE_ENEMY_FOREACH` 扫到」时会被 `yk_frame_end` 退休（`enemy_id=0`、
`fam_seen=0`、`broken=0`）⇒ 下一帧重新分配的空条目**没有记忆** ⇒ 闸会放开、1:1 映射会重建使魔。
⇒ **只要 boss 有哪怕一帧不在敌人链表遍历里，这个症状就自愈。** 现象既然是「持续不发弹」，
反过来钉死一条否证：**boss 每帧都在 `mgr+0x18c` 链表上被扫到** —— 它排除了
「敌人遍历漏扫 / 条目被提前退休」这一类解释，把范围锁在 §5 的第 3–5 步。

## 6. 观察点（实跑里看什么，逐条对应上面的每一步）

> 日志路径与规则见 `local/GAME-LOG.md`（本机铁律；先按 `=== th18_card_expand <时间> ===` 切出本次启动）。

| 要验的 | 看什么 | 判读 |
| --- | --- | --- |
| 步骤 2 的触发者 | `youkai: familiar enemy_id N killed (kind K, real=R) at (x, y)` | **`K == 4` = KILL_ALL** ⇒ 就是段首 `enmKillAll` 干的（我们的理论）；`K == 0` = 被自机弹打死 ⇒ 触发者换成玩家火力 |
| 步骤 3 是否真的发生 | `youkai: emitter gate ACTIVE - slot S of enemy_id N stays silent until ECL re-arms it` | 只打一次 ⇒ 至少夹住过一次；把 `(S, N)` 与 boss 的 `enemy_id` 对上 |
| 步骤 4 的出口是否存在 | `youkai: emitter gate self-healed X slot(s) after 120 frames without ECL re-arm (total Y)` | **在涨** ⇒ 自愈发生了 ⇒ 现象是「循环」而不是「永久夹死」（⇒ 必须往第 5 步查）；**完全不出现** ⇒ 夹住后连 `yk_note_slot` 都没再跑到该条目（那就要查 `CE_ENEMY_FOREACH` 是否漏扫该敌人 / 条目是否被退休）|
| 步骤 5 的重建—再死循环 | `youkai: ledger - N spawned, M killed, W withdrew, D dropped`（每 600 帧一行）+ `run done` | `killed` 与 `spawned` **同步单调上涨、且差值≈0** ⇒ 每建必死 = 循环坐实；`killed` 停在某值而 `gate ACTIVE` 仍挂着 ⇒ 不是循环而是别的夹法 |
| 现场字段（可选，加一行日志） | 该 `(enemy_id, slot)` 的 `props[i].type`、`fam_id`、`broken`、`fam_seen`、`no_fam_frames` | 直接看出是「ECL 真的没再武装」（`type==0`）还是「武装了但闸看错了」|
| 反面对照 | 只用**大妖精 / 道中杂鱼**（不开 boss 战）时 `killed` 是否也在涨 | 道中不会 `enmKillAll` 清我们的使魔 ⇒ 若道中也涨，说明还有第二个杀手（多半是自机弹） |

## 7. 对修法的约束（本轮只记约束，不改 `youkai.c`）

1. **站点预算已经超线**：`python3 native/sites.py check` 当前输出
   `本 patch 声明 27 条`（记账线 24，⚠ 记账口径）、
   `进程级 47 条`（我们 25 + 测试归档 2 + 外部 `base_tsa` 20），
   而安全线是 `SITE_TOTAL_BUDGET = 42`（**42 崩过**）。
   ⇒ **不能再加站点**：想挂 `etNew`（`0x433dde`）拿「真波次事件」这条路**现在走不通**，
   除非先减档（关测试归档只腾 2 格，仍 45 > 42）。
2. ⇒ 修法必须**骑在已有的 `ce_enemy_shot_slot`（`0x433ffa`）站点上**：那个站点每帧每槽被问一次，
   本身就是「这个发射口现在要发弹」的事件 —— 用它当波次代理（而不是 `props[i].type` 的边沿）。
   三条候选（成本/风险/是否符合原作）留给 owner 拍板，见
   [`AUDIT.md`](../../../mods/th18.v1.00a/card-expand/AUDIT.md) §BE 的新条目。
3. **`YK_FAM_LIFE = 1` 是占位值**（`youkai_core.h` 自己标着 TODO(数值设计)）：在 1 点血的使魔 +
   钉在发射口上的几何关系下，「建出来就死」是常态，任何「使魔没了就掐闸」的语义都会被这个值放大成永久效应。
   修法应**同时**决定这个值（或者决定「掐闸」这件事要不要保留）。
