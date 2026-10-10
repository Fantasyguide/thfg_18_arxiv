# 108 发射口使魔 —「5 面中 boss 只建一侧」+「只持续几帧」根因定位与回归分析

> **版本**：TH18 v1.00a（`th18.exe`，imagebase `0x400000`）。本文裸地址默认属该版本；引 TH08 处写成 `th08:0x…`。
> **方法**：一手 ECL 反编译（`st05mbs.ecl`）+ 实跑 log 原文 + `git diff 2b9aea82..HEAD`。**只读**：未改任何代码、未 make。
> **可信度**：Q1/Q2 为 ✅（ECL 一手 + log 原文 + 代码行号）；标 🟡 的集中在 §6。
> **被查对象**：`youkai.c` / `youkai_core.c` / `youkai_core.h`（见 [youkai.c](native/cards/youkai.c)）；log = `/mnt/d/项目/幻想指南/18/th18_modkit/game/th18_card_expand.log`（build `Oct 10 2026 19:52:19`，log:571，= `2c5eda07`）。

## 0. 结论（4 条）

1. **Q1 不是闸、不是坐标来源、不是表容量，是数据模型。** `st05mbs` 的 `MBoss1_at` 每 12 帧让**同一个 slot** 在 `±90°/r=70` 两侧各 `etOn` 一次，且 slot 0 与 slot 1 的 `etOffsetRad` **完全相同**；我们的表按 `(enemy_id, slot)` 一槽一壳、位置变化「只动位置绝不重建」⇒ **结构上只能有一只、且永远在同一侧**。
2. **Q2 = `YK_EMIT_FAM_HP_OVERRIDE = 1`（`youkai.c:714`）**：每只壳 `life=1`，挨一下就死；死后闸夹住该槽，log 实测夹了 **43 个 `etOn` ≈ 8.6 s** 才放行 ⇒ 看到的就是「几帧」。
3. **纯回退到 `2b9aea82` 修不好 Q1**（同一条 ECL、同一 `(敌人,槽)` 模型）；Q3 里真正的回归是别的东西（见 §4）。
4. **唯一能同时给出「对称两只」+「打死使魔 ⇒ 关该发射口」的是方案 C**：身份键改成 `(敌人,槽,出点)`、关火改成「只关被打死的那个出点」（§5）。

## 1. 现场（最新 log，一局 5 面）

| 量 | 值 | 出处（log 行） |
| --- | --- | --- |
| build | `Oct 10 2026 19:52:19` | 571 |
| 本局合计 | 149 spawned / 30 killed / 119 withdrew / 0 silent | 40552 |
| **两槽敌人 = enemy 303** | slot 0/1 两次建壳，**两次都同坐标** | 36914-36915、39754-39755 |
| 该敌人被拒行 | `CONFIGURED but has NOT fired` slot 0/1 | 36937-36938 |
| 两槽被打死 | fam 305 `frame 4023` / fam 306 `frame 4026` | 37697、37726 |
| 闸夹住 | `CLAMPED slot 0 no_fam_frames=11` / `slot 1 = 8` | 37787-37788 |
| 放行 | `WAVE-GAP(adaptive) after vetoing 43 etOn(s)` | 39757 |
| 第二轮同样收场 | fam 307/308 `frame 4347/4350` → `CLAMPED` → `re-armed total 22` | 40065、40090、40106-40115 |

⚠ 任务书里的 `enemy 158` 是**更早那几局**的 id（本局 158 是一只使魔的 `fam_id`）；本局的两槽敌人是 **enemy 303**（🟡 见 §6-A）。
⚠ **本局 enemy 303 一条逐槽诊断都没有**（额度全在前 3 只敌人用完，见 §6-B）——下面 Q1 靠的是 ECL 一手 + 历史提交里的 log 证据。

## 2. Q1 根因（✅）

### 2.1 一手 ECL：`st05mbs` 的 `MBoss1_at`（`local/_scratch/c108/th18ecl2/st05mbs.txt:93-104`）

```
93: MBoss1_at_736:
94:     etOffsetRad(0, %B - 1.5707964f, 70.0f);   // slot 0：玩家方向 -90°，半径 70
96:     etOn(0);
97:     etOffsetRad(0, %B + 1.5707964f, 70.0f);   // slot 0：玩家方向 +90°  ← 同一槽、同一帧、另一侧
98:     etOn(0);
99:     etOffsetRad(1, %B - 1.5707964f, 70.0f);   // slot 1：与 slot 0 **逐字相同**的偏移
101:    etOn(1);
102:    etOffsetRad(1, %B + 1.5707964f, 70.0f);
103:    etOn(1);
104:    wait(12);  %B = %B + %D;
```

- `%B = %ANGLE_PLAYER`、`%C = %B - π`（原文 88-89 行）；外层 `MBoss1` 每轮 `@MBoss1_at(±10°)` + `wait(30)`。
- ⇒ **owner 要的「对称两只」= 同一个 slot 的两次 `etOn`**，不是两个 slot。
- ⇒ 一帧之内：`props[0]+0x08/0x0c` 被写两次（`0x433fec` 写 +0x08/0x0c、`0x433ff4` 写 +0x10，都在断点 `0x433ffa` **之前**，一手见 sdk.c:3392-3397），**后写覆盖先写**；两槽写的是同一个值。

### 2.2 我们这一侧：为什么结构上只能一只

| 环节 | 代码 | 后果 |
| --- | --- | --- |
| 身份键 = `(enemy_id, slot)` | `youkai_core.c:466` `yk_map_find()` / `502` `yk_map_alloc()` | 一个槽**只能**挂一只壳 |
| 建壳门 | `youkai_core.c:627` | `active && fired && !fam_id && !broken` |
| 位置来源 | `youkai.c:2949-2957`（latch，否则 `props[i]+0x08/0x0c`） | latch 也按 `(enemy_id, slot)` 存（`youkai.c:588`），同帧第二次 `etOn` **覆盖**第一次 ⇒ 只剩 B+90° 那一侧 |
| 位置变化不算重建 | `youkai.c:1065-1070`（注释即「这里只动位置 —— 绝不 CREATE/WITHDRAW」） | 偏移换边**不会**给另一侧补一只 |
| 同点去重**已关** | `youkai.c:715` `YK_EMIT_SHARE_SAME_POS 0` | 两只**都**建了，只是**叠在同一点**（log 36914/36915） |

⇒ 观感 = **一侧一只**；另一侧（`%B-90°`）的那次 `etOn` 在任何一环里都被覆盖掉，永远不会变成壳。

### 2.3 交叉证据（历史提交里的 log 原文）

| 时间 | 证据 | 出处 |
| --- | --- | --- |
| 16:26 #136 | `enemy 158` slot 0/1 都 `SAMPLE => CREATE`（`type=9`、离体 70px） | `git show f7da84ae` 提交信息 |
| 16:39 #139 | gate check `(158, slot 0)` 与 `(158, slot 1)` 都 `fam_seen=1 have_fam_id=1 broken=0`；两槽 `emitter_world` 完全相同 `(-70.0,128.1)` ⇒ 结论「最可能两只重叠」 | `git show a9fc201e` 提交信息 |
| 19:18 #145 | `SPAWN slot 0/1 pos=(-69.6,135.3)` 同坐标、`COORD-SPAWN requested==engine_actual` | `git show 9e4b4738` 提交信息 |
| 本局 | `(-70.0,127.1)` / `(-99.4,121.8)` 两次都同坐标 | log 36914-36915、39754-39755 |

**三次独立观测、跨 4 个 build，两槽位置恒等** ⇒ 这是 ECL 决定的，不是抖动。

### 2.4 逐个排除（这些全都不是元凶）

| 嫌疑 | 代码/旋钮现状 | log 判据 | 判定 |
| --- | --- | --- | --- |
| 表满 `YK_FAM_MAX=32` | `youkai_core.c:502-517` 表满静默 `return 0`；`fam_full` **从不打印** | 无 `spawn FAILED` 行；同屏活跃槽 = 2 | ❌ 不是（但 🟡 无法从 log 证明，见 §6-D） |
| `SHARE_SAME_POS` 翻回 1 | `youkai.c:715` = **0** | `shares the shell` 0 行；两只都建了 | ❌ |
| latch 环 32 / sticky 环 64 挤出 | `youkai.c:582/826` | boss 只有 2 个槽 | ❌ |
| 32px 距离闸 | `youkai_core.h:1372` = 32 | 两槽都建成功 | ❌ |
| no-origin 守卫 | `youkai.c:3131-3140` | `REJECT(no-origin)` **0 行** | ❌ |
| type 黑名单 | `youkai_core.h:1386` = 0（关） | `REJECT(bad-type)` **0 行** | ❌ |
| produced / no-bullets | `youkai.c:721` = 0（软） | `REJECT(no-bullets)` **0 行** | ❌ |
| `fired` 门 | `youkai_core.c:627` | 两槽都过了（都建了壳） | ❌ |
| 第二次 `ce_spawn_enemy` 失败 | — | 两条 `SPAWN` 都是 `ret=1`，且 305/306 之后都被打死 | ❌ |

## 3. Q2 根因（✅）：HP=1 + 粘性闸

| 因子 | 一处代码 | 一处 log |
| --- | --- | --- |
| **寿命 = 1** | `youkai.c:714`（`=1`）→ `2656` `life = YK_EMIT_FAM_HP_OVERRIDE` | 2606 `CREATE hp=1 (override)`；2608 `CREATE shell #1 … life=1` |
| 一击即死 | 同上（`life=1` ⇒ 任何一发伤害都够） | `familiar enemy_id 305 killed (kind 0, real=1)`（log 37697） |
| 死后该槽静默 | `youkai_core.c:779-819`（夹住后**每次都掐**，只有主复位口/TTL 放行） | `slot 1 … after vetoing 43 etOn(s)`（39757）；12 帧/次 ⇒ ≈516 帧 ≈ 8.6 s |
| 反复重建 | `youkai_core.c:790-800`（WAVE-GAP） | `released 2 slot(s) on an etOn GAP`（39756） |

⇒ 观感 = 「使魔露面几帧 → 死 → 该口哑 ~9 秒 → 再露几帧」。**这一条与 Q1 无关、可独立关掉**：把 `YK_EMIT_FAM_HP_OVERRIDE` 设 0（恢复 `yk_map_fam_hp()` 正式口径）。
🟡 另外 119 次 `withdraw` **没有任何逐事件日志**（`youkai.c:2763-2768` 只 `++s->lost`）⇒ 这 119 次里有多少是「槽不活跃被撤」、多少是「帧末退休」、多少是「换关/删卡」**从本局 log 判不出来**（见 §6-C）。

## 4. Q3 回归时间线（`2b9aea82..HEAD`，只列改到建壳/位置/寿命/表的）

基线 `2b9aea82`（10-09 23:01）的形态：位置 = `yk_slot_offsets()` + `yk_emit_world_pos(epos, 活偏移, origins)`；建壳门 = `active && !fam_id && !broken`（**无** `fired`/`produced`/距离闸/黑名单）；本体口靠 `yk_emit_on_body(off3, org3)`（活偏移恒 0 ⇒ 相对模式槽一律按本体口跳过）。

| 提交 | 改了什么 | 影响了什么 | 证据 |
| --- | --- | --- | --- |
| #115/#116 | 壳改 D2 插值跟随 → 又改回硬钉 | `YK_EMIT_FOLLOW_FRAMES` 0/60 | 提交信息；`youkai_core.h:1324` 现为 0 |
| #117 | WAVE_GAP 自适应（修「打死使魔不关火」） | 放行判据 | 提交信息 |
| **#118** | **新增 latch：在 `0x433ffa` 抄 `props[i]+0x08/0x0c/0x10`，按 `(敌人,槽)` 存** | 位置来源开始读 props；**同帧第二次 `etOn` 起就只会剩最后一次** | 提交信息；`youkai.c:588-604`、`6951-6956` |
| #123 | 建壳门加 `e->fired`（修幽灵壳） | `youkai_core.c:627` | `git show c25bb06e:…/youkai_core.c` 第 627 行已是该式 |
| #125 | 加 32px 距离闸（替代本体口规则） | 相对模式槽不再被无条件跳过 | 提交信息；`youkai_core.h:1372` |
| **#127** | 位置 = latch 直接当世界坐标（不再叠加） | 第一次「不加敌人位置」 | 提交信息（含 `0x433f9e` 反汇编） |
| #133 | no-origin 守卫 | `(0,0)` 槽不建 | `youkai.c:3131` |
| #134 | type 黑名单 + SAMPLE | 现默认 0（关） | `youkai_core.h:1386` |
| **#135** | **位置回退成 `epos + latch`（二次叠加）** | 壳落到 `2 × enemy_pos`（owner 现场） | 提交信息；`git show 9967b51a` 第 2715-2717 行 |
| #136 | `fired` 改本波粘性（环 64） | 慢节奏 `etOn` 不再漏抓 | `youkai.c:826`；提交信息 |
| **#145** | 加「同出点共壳」并**默认开** | 两槽同点 ⇒ **合成一只**（owner 当场否掉） | 提交信息；`youkai.c:2669-2699` |
| **#146** | 撤共壳（旋钮 = 0）+ POSSRC 并列诊断 | 恢复一槽一壳 ⇒ 两只叠在同点 | `youkai.c:715` |
| **#148** | **`YK_EMIT_FAM_HP_OVERRIDE = 1`** | **Q2 的直接原因** | `youkai.c:714`；log 2606 |
| **#149** | 位置 = `props[i]+0x08/0x0c`（绝对、不叠加）；**删掉 `yk_slot_offsets()`/`yk_emit_world_pos()` 的唯一调用** | 位置定案 ✅，但 `origins[i]` 从此**无人读**、`org3` 变成未初始化栈（`youkai.c:2892/2959/3028`） | `git show b027a3a7` 的 diff（`- yk_slot_offsets(e, i, off3, org3);`） |
| #150 | 距离闸恢复 32.0f | 本体口被拒（`BODY PORT` 5852 行） | `youkai_core.h:1372` |
| #151 | `produced` 硬前提（+POST） | 0 子弹槽不建；另一面 `0x7d0` 读错 | 提交信息 |
| #152 | `produced` 降级为软（`=0`） | 恢复「距离闸唯一」 | `youkai.c:721` |

**哪一次把「两边能建」弄坏的？**
- 对**中 boss 的两侧**：**没有单独一次提交**——`(敌人,槽)` 模型从基线起就如此，ECL 也从基线起就如此；#118 之后「同帧覆盖」才显式化，`#139`（16:39）已把它写成「两只重叠」。
- 真正的**功能回归**（不是中 boss）有两次，都可指认：**#135** 把壳二次叠加到 `2×enemy_pos`（owner 当场报「正下方」）；**#149** 顺手删掉了 `origins[]` 这条唯一能表达「脚本给的绝对出点」的通路，并留下未初始化的 `org3`。
- **#148** 是「只持续几帧」的独立原因。

## 5. Q4 方案（不实现，只写）：A 最小修复 / B 整体回退 / **C 多原点壳（推荐）**

### 方案 A：最小修复（现状基座上的就地改法）

| # | 改哪 | 怎么改 | 风险 |
| --- | --- | --- | --- |
| A1 | `youkai.c:714` | `YK_EMIT_FAM_HP_OVERRIDE` 1 → **0**（恢复 `yk_map_fam_hp()`） | 无（只影响手感数值）⚠ 先做这条，Q2 立刻消失 |
| A2 | `youkai.c:588-604` + `2949-2960` | latch 由「每 `(敌人,槽)` 一条、后写覆盖」改成**每帧每槽最多两条**：同帧第二次 `etOn` 且位置距第一条 > 1px ⇒ 存成第二条（伪槽 `slot + 16`） | 需把 `YK_EMIT_SLOTS`（`youkai_core.h:922`）的**真实 props 上限**与**虚拟槽上限**分开 |
| A3 | `youkai_core.c:534/627` | 桶用表项已有的 `x/y`（见 C3，**不动结构**）；建壳门/位置按「槽 + 出点」；闸与 `fam_broken` 仍**映射回真实槽**。⚠ `yk_slot_t` 32 B **已排满**（无 padding 空洞）⇒ 想加 `side` 字节必须重排 | 打死任一侧 ⇒ 该真实槽停火（与 owner 现有语义一致） |
| A4 | `youkai.c:2892/2959/3028`、`569` | 要么把 `yk_slot_offsets()` 调用恢复（让 `org3` 变实数、`#37` 本体口规则复活），要么删掉 `yk_emit_on_body()` 分支（承认 32px 闸已取代它）。**现状是未初始化栈 UB，必须二选一** | 不改则编译器的栈布局可改变判据 |
| A5 | `youkai.c:3163-3171` | phantom 行加上 `yk_fired_sticky_get()`（否则壳还在也打「has NOT fired」） | 纯日志 |

判据：5 面中 boss 非符里同屏看到 **左/右各一只**；打死任一只 ⇒ 该发射口当帧哑；下一波重建；一面关底前 0 壳。
代价/收益：改的是**身份键语义**（不是阈值），约 4 处 40-80 行；**保留**全部已验证的成果（`props` 绝对坐标定案、id 回收防护、latch 世代、粘性 `fired`、`on_body`/距离闸）。

### 方案 B：完全回退到 `2b9aea82` 后只加回 owner 真正要的 4 件事

| 步骤 | 保留 / 丢 |
| --- | --- |
| 基座 | `youkai.c` / `youkai_core.{c,h}` 回 `2b9aea82` |
| ① 打死使魔 ⇒ 关该发射口 | 加回 **#117 + #92/#93 + 「夹到主复位口/TTL」的 P1 口径**（`yk_shot_slot_gate` / `fam_seen` / `broken` / `YK_EMIT_REBUILD_TTL` / `yk_fam_broken` / `yk_fam_wave_clear`） |
| ② 无体术 | 加回 **#129 + #130**（`e+0x6334 = 2` 的 `yk_no_body_attack()`，映射层 + 绕转层） |
| ③ 空格调试击杀 | 加回 **#140→#144**（`ce_key_down(CE_VK_SPACE)` + 写父 id + 父子活体校验）＋ **#147**（清 `NO_DAMAGE`） |
| ④ HP 临时旋钮 | 加回 **#148** 的旋钮，但默认 **0** |
| 建议一并保留 | **#123 + #136**（`fired` 门 + 粘性）——否则「配了没发的幽灵壳」复发；**#125 的距离闸**可留可丢（它是 #37 的替代品） |
| 丢掉 | #127 / #131 / #135 / #149 / #150 / #151 / #152 的取位与猜测闸；`SHARE_SAME_POS`（#145，已 0）；D2 跟随（#115，已 0） |

**代价**：会丢 ①「`props+0x08` 是绝对世界坐标」的定案（退回 `epos + 活偏移`，而活偏移在卡 tick 恒 0 ⇒ 壳又贴回母机身上）② id 回收防护相关的表结构改动 ③ latch 世代/`frame` 时间戳 ④ 一轮二分（#100-#114）证明过的回调净化。
**⇒ 关键：方案 B 也修不好 Q1。** 同一条 ECL、同一个 `(敌人,槽)` 模型 ⇒ 中 boss 依旧只能一侧（要么两只重叠、要么被 `on_body` 全跳掉）。**要让「对称两只」出现，A2/A3 那一步是绕不开的。**

### 方案 C（**唯一同时满足「对称两只」与「打死使魔 ⇒ 关该发射口」的方案**）

**核心**：身份键从 `(enemy_id, slot)` 改成 **`(enemy_id, slot, 出点 ε 分桶)`**，并把「关火」从「整槽」改成「**只关被打死的那个出点**」。

| # | 要点 | 落点 |
| --- | --- | --- |
| C1 | **多原点捕获**：`0x433ffa` 每次 `etOn` 把一个条目 `(enemy_id, slot, x=props+0x08, y=props+0x0c, frame)` **追加**进本帧列表；`(敌人,槽)` 已有 ε 内条目 ⇒ 覆盖（保持「位置只动不重建」）；同帧不同出点 ⇒ **追加第二条** | 改 `s_emit_latch[]` 的 `yk_emit_latch_put()`（`youkai.c:588-604`），环 32 → 64 |
| C2 | **按出点建壳**：tick 对每个 latch 条目做 `yk_note_slot`，查表用新的 `yk_map_find_at(m, enemy_id, slot, x, y, ε)` | `youkai_core.c:466/502/534`；`youkai.c:2949-2960`、`3172` |
| C3 | **桶 = `yk_slot_t.x/y` 本身**（表项已经存了出点坐标）⇒ **`sizeof(yk_slot_t)` 不用动**（该结构 32 B 已排满，见 `youkai_core.h:1144`） | 零结构改动 |
| C4 | **关火按出点匹配**：打死壳 ⇒ `yk_fam_broken()` 把该条目的 `(enemy_id, slot, x, y)` 写进「已关出点」环；站点处算出**本条 `etOn` 的出点**（`props+0x08/0x0c`，`0x433fec/0x433ff4` 就在本站点之前写，✅ 一手见 sdk.c:3392-3397）命中该环才否决 | `youkai_core.c:676-694`、`779-819`；站点 `youkai.c:6995-7011` |
| C5 | **同出点的孪生壳一起撤**：`slot 0` 与 `slot 1` 出点逐字相同（§2.1）⇒ 会建 4 只、视觉 2 只；打死任一只时把同 `(敌人, ε 内出点)` 的**所有**条目一起撤，避免留下「不再发弹却在场的幽灵壳」 | `yk_run_action` 的 WITHDRAW 前多一步扫（`youkai.c:2763`） |
| C6 | 四放行口不变（波次边沿 / 重新武装 / 相位 / TTL 600），只是它们清的是「已关出点」环而不是整槽的 `fam_seen/broken` | `youkai_core.c:821-900` |
| C7 | **不新增站点**：出点在站点前已写好，只读 | sdk.c 不动 |

**代价 / 风险**

| 风险 | 量级 | 对策 |
| --- | --- | --- |
| `YK_FAM_MAX = 32` 不够（`youkai_core.c:517` 表满**静默**放弃） | 中 boss = 4 条（2 槽 × 2 出点）；同屏多只多原点敌人会顶到 32 | 提到 64；`fam_full` 必须进台账（现在从不打印）。⚠ `yk_action_t tail[YK_FAM_MAX]` 在栈上（`youkai.c:2891`，24 B × N）⇒ 64 条 = 1.5 KB 栈，需实测 |
| **同帧 25 个出点**那种 ECL | 一次性 25 只壳 | 每 `(敌人,槽)` 每波上限 `YK_EMIT_ORIGIN_MAX = 4`；且新出点须**连续 ≥2 帧**出现才建壳（去抖） |
| ε 取值 | 两侧相距 140 px ⇒ 不敏感 | `YK_EMIT_ORIGIN_EPS = 1.0f` px，记 `TUNING.md`；太小被浮点抖动分裂、太大把小偏移槽并成一个 |
| **把 stage-1 小怪也放大** | 是（凡是同帧多出点的槽都会翻倍） | ① 32px 距离闸保留（本体口 `props≈enemy_pos` 仍被拒）② 单出点槽行为**完全不变** ③ 可选再加一道「只对 boss 表里的敌人开放」（读 `em + CE_EM_BOSS_IDS`，复用 `yk_boss()` 的读法） |
| 主功能口径变化 | 「整槽全关」→「只关该出点」⇒ `vetoed K etOns` 会变小 | 更贴 TH08（一只使魔一个口）；但 `#117` 验收与 `native/tests/test_youkai_core.c` 的闸用例要同步改 |
| 皮肤 / VM | 同槽两个出点共用 `yk_fam_mapped_kind(slot)`（`youkai.c:2753`）⇒ 左右同色 | 可接受；要区分就按出点象限给变体 |

**方案 C 的最小验证判据**（这几行一出，结论即定）

1. **同一 `(enemy_id, slot)` 出现两条 `SPAWN` 且坐标不同**：`youkai/emit SPAWN: slot 0 … pos=(x1,y1)` 与 `… pos=(x2,y2)`，`|x1-x2| ≈ 140`。**今天永远只有一条** ⇒ 这一对就是「多原点壳」的判据。
2. 新增 `youkai/emit FIRE: enemy_id N slot i pos=(x,y) frame F bucket#k`：中 boss 每帧 4 行（slot 0/0/1/1）、2 个不同坐标。
3. 新增 `youkai/emit ORIGIN-GATE: enemy_id N slot i pos=(x,y) closed=0/1 => veto=0/1`：打死左侧后，`x1` 那几行 `closed=1 veto=1`、`x2` 那几行 `veto=0` ⇒ **单侧关火**当场可判。
4. 台账：新开 `origins: frames_multi=N max_per_slot=M capped=K`；`fam_full` 必须为 0；中 boss 的 `create` ≈ 今天 ×2。
5. 反例回归：stage 1–2 的 `CREATE shell` 数、`BODY PORT` 量与今天**同量级**；打死壳仍能让它自己那个口停火。

### 推荐

**推荐方案 C，分三小步落（不整体回退）**：

1. **C0（立刻、1 行）**：`YK_EMIT_FAM_HP_OVERRIDE` 1 → 0（`youkai.c:714`）⇒ Q2 消失，与 Q1 无关、可先给 owner 一个能看的版本。
2. **C1（只加数据，不改行为）**：多原点 latch + `FIRE` / `ORIGIN-GATE` 两行日志（判据 1/2/3 的先决条件）。
3. **C2**：按 C1 的数据把身份键换成 `(敌人,槽,出点)` + 「只关该出点」的闸 + 孪生壳同撤。

**为什么不选 B（整体回退）**：① 同一 ECL、同一 `(敌人,槽)` 模型 ⇒ 回退后中 boss 依旧只能一侧（§4 结论），回退的代价换不到 owner 要的东西；② 会丢掉已验证的一手结论（`props+0x08` 绝对坐标、活偏移在卡 tick 恒 0）与 `#100-#114` 那轮回调净化、id 回收防护、latch 世代。
**为什么不用 A 代替 C**：A2/A3 与 C1/C2 是同一件事（都要改身份键），C 只是把「位置落点」也一起改成可匹配的出点，从而**顺带把主功能从「整槽全关」修成 TH08 的「一只使魔一个口」**——A 做不到后者。
**若 owner 仍要「先回退再慢慢来」**：退 `#133/#134/#150/#151/#152` 五道猜测闸（旋钮回默认关）即可，**但不要退 `#149` 的 `props` 绝对坐标与 `#118` 的 latch**，那两条是 C 的地基。

## 6. 🟡 不确定项

- **A. enemy 303 是不是 `st05mbs`**：本局 log 没有任何一行给出 303 的 sprite/life（SAMPLE 额度早用完）。判它是中 boss 的依据 = 「全 log 只有它一个敌人出现 slot 1」+ `MBoss1_at` 的两槽形态。🟡
- **B. 诊断额度**：`SAMPLE`（`youkai.c:781`，48 行）在 **enemy 3** 就用完、`POSSRC`/`SLOTSTATE`（64）在 **enemy 250** 用完、`CREATE shell`/`COORD-SPAWN`/`POST`/`gate check`（24）、`COORD`（8）、`RAW`（64）全部更早 ⇒ **boss 现场 0 行**。任何「从最新 log 看 303 的 POSSRC/POST/SAMPLE」都不可能（除非 owner 直接在 5 面中 boss 处开局重跑）。
- **C. 119 次 withdraw 无逐事件日志**：`youkai.c:2763-2768` 只 `++s->lost`。要归因必须加一行（带 `why`/槽/帧）。
- **D. `fam_full` 从不打印**；`YK_FAM_MAX=32` 是否触顶**无法从 log 判定**（我判「不是」靠的是同屏活跃槽只有 2 个，属推断）。
- **E. `origins[i]`（`0x4f18`，`.z > 0.9` = 绝对模式）是否被 `st05mbs` 用到**：**#149 之后全卡无人读它**，所以基线那套 `yk_emit_world_pos()` 究竟会不会给中 boss ±70 两只，**本局 log 证明不了**。这条决定「恢复基线取位公式」是否有用（我倾向「没用，因为两槽偏移逐字相同」，但那一步没有直接观测）。
- **F. 引擎是否真的把同帧两次 `etOn` 都发出去**：我们只看到 `props+0x08` 的**后写值**；卡侧 hook 每帧应被问 4 次（0/0/1/1），但 log 里没有「每次 fire」的行可核。🟡
- **G. WAVE-GAP 为何要 43 个 `etOn` 才放行**（预期 ≈ 4-5 个）：`eton_gap` 只在夹持期累加（`youkai_core.c:599-601`），与 `gap_ref` 的自适应阈值交互未追。

## 7. 请 owner 复现时观察并回传的行（**别猜着写结论**）

1. **每个 fire 一行**（站点 `0x433ffa`，含**同帧第 2 次**）：`enemy_id / slot / 本帧第几次 / props[slot]+0x00,+0x04 / props+0x08,0x0c,0x10 / offsets[slot](0x4e58) / origins[slot](0x4f18) 含 .z / frame`。**这是唯一能一次判死 A/E/F 的观测。**
2. **BOSS 不限量的 `POSSRC`/`COORD`**：把 `s_possrc_logged < 64u` 改成「只对同屏 ≥2 个 active 槽的敌人」或「同帧位置不同的槽」。
3. **每槽的 `SLOTSTATE` + 每条 `etOn` 的 gate 裁定**，对 boss 不设 cap。
4. **每次 WITHDRAW 一行**：`(enemy_id, slot, why = inactive|frame_end|drop_all|wave_clear, frame)`。
5. `run done` 行加上 `fam_full`（现在从不打印）。
6. 建壳/撤壳的 **frame 号**（现在 `SPAWN` 行没有帧号，只能靠 `youkai/popup spawn … frame N` 反推）。

> 只要拿到第 1 条，Q1 的 A/E/F 三个 🟡 都能一次落定；在那之前，§2 的结论依赖的是 ECL 一手文本（`st05mbs.txt:93-104`）与三次跨 build 的同坐标观测。

## 8. 顺带发现的日志口径问题（不改代码，但会误导排查）

| 现象 | 真相 | 位置 |
| --- | --- | --- |
| 台账 `latched=8` | 打的是 `s_emit_latch_logged`（**打过 8 行日志**），不是 latch 次数 | `youkai.c:8246` |
| `re-armed … rebuilt in the same tick` | **假的**：`rearm_pending` 自 P1 起是纯记账，无任何消费者 | `youkai.c:8860`；`youkai_core.c:811-818` |
| `CONFIGURED but has NOT fired` | 只看 `fired_recently`、**不看 sticky** ⇒ 壳明明在也会打 | `youkai.c:3163-3171` |
| `is a BODY PORT (distance 968.6 px < 32)` | 打印的是 **d²**（`yk_emit_dist` 返回平方），判据是 `d2 >= 32²` | `youkai.c:806`、`3152-3161` |
| `BODY PORT` 5852 行 / `phantom` 77 行 | seen 环只有 8（`youkai.c:800`）⇒ 同一 `(敌人,槽)` 被反复计数/刷屏，计数**不是**「被拒槽数」 | `youkai.c:800`、`804-814` |

---

**只读声明**：本次调查未修改 `youkai.c` / `youkai_core.{c,h}` / 配置 / 文档之外的任何文件；未 `make`、未 `release`、未 `git checkout/reset/stash`；本文是唯一新增文件。
⚠ `tooling/check-docs.py` 只扫 `git ls-files`（见 [`DOCSTYLE.md`](../../../DOCSTYLE.md) §「新建文档的起手式」）⇒ 本文目前**未跟踪**、不在那次 449 文件的扫描里。已用同一套 `check_links/check_addr/check_version/check_tables` 单独跑过本文：**0 违例**；正式验收前请 `git add -N` 本文再跑一次。
