# 108 · TH08 全 24 份 ECL「非本体位置的发射口」普查 ⇄ 我们的槽/壳模型

> **版本**：跨版本。本文出现的地址一律带版本前缀（`th08:0x422720`、`th18:0x433ffa`）。
> **方法**：只读。① 一手**定向反汇编**复核关键点（`objdump --adjust-vma` 按 PE 头换算，见 §4-5 的 VA 注 + `th08:0x4227a4`/`th18:0x433ffa`）；
> ② TH08 语义用 `local/vendor/th08-decomp`（reccmp 重建，`C_ASSERT` 锁字段偏移）当索引，再用一手字节复核，**只此一源者标 🟡**；
> ③ **24 份 ECL 反编译文本全量机扫**（`local/_scratch/th08f/ecldata*.txt`；脚本 `local/_scratch/c108/scan_v3.py`）＋抽样人读；
> ④ 卡侧只读（`native/cards/youkai.c`）。**未改任何 `.c/.h`、未 make、未 release。**
> **可信度**：类别清单与计数 ✅（机扫 + 人工回读样本）；TH08 字段偏移、`etEx` 真义、TH18 站点 ✅（decomp `C_ASSERT` + 一手字节）；激光槽细节、`familiarCreateF` 建位差 🟡。

## 0. 结论（最短版）

1. **TH08 根本没有「非实体发射口」**：发射口 = `敌人+0x2e24` 那**一支**弹模板的 `shootOffset`（每敌人各一套），ECL 里 `et*` 只出现在 `sub` 内、时间轴段 **0 条** ⇒ 口永远挂在某个敌人身上。
2. 「非本体」只有四种实现：**偏移离开本体**（含 −`%SELF_X` 硬锚到场地）、**使魔/子敌人自己的口**、**激光槽**（每敌人 `laserSlots[32]`，位置独立且持久）、**口的位置取自某发弹的当前位置**（EclExIns id 27）。
3. ★ 一手更正：**`etEx`(op111) 不是「槽表」，是 `bulletSpawnDescriptor.transforms[id]`**（弹在飞的行为，1269 处）。`RESEARCH-emitter-disable.md` §A.2(3) 把它读成发射口槽 ⇒ 那条要作废；TH08 每个敌人**只有一支弹模板**这一条成立。
4. **模型 cover 得了的**：本体口 / 使魔口 / 绝对场地坐标建出的载体（`familiarCreateA`）/ 弹位建敌人（1 例）。
5. **cover 不了或勉强**（按 频率×影响）：① 同帧多原点（最多 25 点，3 个 sub） ② 激光槽（12 份 / 97 条） ③ 临时离位（138 组）＋绕飞偏移（`mathCirclePos` 110 处）④ 弹的变换（`etEx` 1269 处，另案）。细节与补法见 §3。

## 1. 对照基准（我们的模型，只读复核）

| 环节 | 位置 | 行为 |
| --- | --- | --- |
| 槽 | `props[i] = 敌人 + 0x17c4 + i*0x488`，16 个 | `th18:0x434013` `lea eax,[esi+0x598]`（`edi = &data`）|
| 开火 | `etOn(slot)` → 算原点 → `th18:0x43401a call 0x427810` → `th18:0x43402d jmp 0x436417` | 真发在 `th18:0x4278c9 call 0x424fe0`；**同帧可重入** ⇒ 同帧重复 `etOffset+etOn` = 一槽多原点 |
| 壳 | 身份 = `(enemy_id, slot)`；位置 = 槽偏移推的世界坐标 | 采样 `enemy+0x6084+i*0xc`（= `data+0x4e58`，一手见 `th18:0x433f5b`）|
| 闸 | 站点 `th18:0x433ffa` per-call 否决该槽 `etOn` | 窗 `f3 0f 10 8f 44 51 00 00`、retaddr `th18:0x43402d` ✅ 已核字节 |
| 本体口 | `yk_emit_on_body()`：偏移恰 (0,0) ⇒ **不建壳** | 与 TH08「本体口是常态」一致 |

## 2. 普查表（24 份 = `ecldata1..8`（第 4 面拆 `4a`/`4b`）各含普通与 `sp` + `_al/_rm/_sk/_yk/_ym/_yy`）

| 用法类别 | TH08 一手样本（sub/行/实参） | 我们模型怎么覆盖 | cover | 若不能：缺什么 / 建议补法 |
| --- | --- | --- | --- | --- |
| 本体口（偏移 0） | `ecldata5.txt:Sub1:30` `etFanAimed(2, 6, 1, 1, 2.0f, 0.5f, 0.0f, 0.28559932f, 515)` | `yk_emit_on_body()` 整格跳过不建壳；弹照发 | ✅ | —（共 1486 个 body 发射点里的多数）|
| 本体口（常量偏移，跨帧保持）| `ecldata8.txt:3909` `etOffset(-60.0f, 60.0f)` → `call("Sub152")`；同段 9 个固定点（`±128/±64/60/192`，`:3917…:3961`）| 槽→壳，壳**停在**该固定点，母机飞走也不动 | ✅ | 现成；壳读活偏移恰好就是这个值 |
| 本体口（常量偏移，其它实例）| `ecldata6.txt:1855` `etOffset(-16.0f, -32.0f)`（同类 `ecldata6sp.txt:1624`）| 同上 | ✅ | — |
| 使魔口（零偏移）| `ecldata5.txt:Sub6:147` `etFan(1, 6, 1, 2, 2.2f, 1.2f, %SELF_ANGLE, 0.0f, 514)`（`Sub2` 建 13 只）| `(enemy_id, slot)` 一壳；打死壳否决该槽 | ✅ | 模型靶心；522 个 familiar 发射点 |
| 使魔口 + 动态偏移 | `ecldata6.txt:Sub38:1114` `etOffset(%F8, %F9)` → `:1118` `etRing(3, 6, 1, 1, 0.0f, 0.5f, %F0, 0.2617994f, 918024)` | 壳跟槽原点（现为瞬移）| 🟡 勉强 | 观感；补法见 §3-③（D2 插值，`youkai.c:2603`）|
| 绕本体飞（随机半径）| `ecldata8.txt:Sub77:1854` `mathCirclePos(%F8, %F9, %RANDRAD, 64.0f)`+`etOffset`+`etRing(11, 2, 32, 1, 1.0f, 1.0f, %F0, 0.3926991f, 514)` | 同上 | 🟡 勉强 | 同上；`mathCirclePos` 全库 110 处 / 6 份（`ecldata8` 34、`ecldata8sp` 59）|
| ★ 硬锚到场地线（−`%SELF_X`）| `ecldata4b.txt:Sub38:1118` `fset_sub(%F8, %F0, %SELF_X)`（`%F0=-7` 或 `391`）→`:1137` `etOffset(%F8, %F9)`→`:1138` `etFan(12, 13, …)` | 弹 ✅；壳会横跨全屏跳 | 🟡 勉强 | 同 ③；壳采样用「发弹时定格的 `props[i]+0x08`」而不是活偏移，见 §3-③ |
| ★ 同帧多原点（一发多点）| `ecldata8sp.txt:Sub111:3719` 起 **25** 个 `mathCirclePos+etOffset(%F8,%F9)+etFan(19, 1, 1, 1, 0.5f, 0.5f, %F0, 0.0f, 8722)` | 引擎能（同帧重入 `etOn`）；但 1 槽 : 1 壳 表示不了 25 个原点 | ❌ | 壳层：见 §3-①。3 个 sub（`ecldata8:Sub98`、`ecldata8sp:Sub52/Sub111`）|
| ★ 一槽多原点（数十帧内换点）| `ecldata7.txt:Sub36:1185→1196` 三发 `etRing`（弹型 10/3/7）各配一次 `etOffset(%F8,%F9)`（±32 随机）| 同上（同槽 3 原点）| ❌ | 同 ①；「换 offset 再发」共 **84 组** |
| 载体 = 绝对场地坐标建的敌人 | `ecldata1.txt:Sub28:914` `familiarCreateA("Sub32", %SELF_X, %SELF_Y, 120, -2, 100)`（op90 = 脚本绝对坐标，214 处**全部**非零）| 任何有活跃槽的敌人都会建壳 | ✅ | — |
| 口的位置 = 某发弹的当前位置 | `ecldata_sk.txt:Sub1:50` `funcCall(27, 0)` = EclExIns `SpawnEnemiesFromMarkedBullets`：在每发带 MARKER 的弹处建敌人（life 800）| 新敌人有自己的槽 → 壳跟着走 | ✅ | 全库仅 1 例（🟡 单样本）|
| 口挂在非敌人对象（道具/背景/自机）| 无（24 份 `et*` 只在 `sub` 内；时间轴段 0 条）| — | ❌ 不存在 | 无需覆盖 |
| `etEx` = 多门炮？| `ecldata7.txt:Sub36:1179` `etEx(3, 32, 0, 240, -1, %F0, %F1)`；7 实参 = index/kind/allow/int/int/float/float | 与炮位无关（弹的飞行行为）| ❌ 更正 | 不是发射口；另案（§3-④）|
| 使魔死了/清场了口还在发 | 无（口是敌人的字段，实体没了口就没了；`enmKillAll` op95 段首清非 boss）| 打死壳⇒关槽是我们的 ❓自定构造 | ❌ 不存在 | 与我们语义**不冲突** |
| 口的所属者变换 | `ecldata6.txt:Sub50:1408` `bossCall(1, "Sub51")`（op88 = 在 `bosses[1]` 上跑该 sub）；但 `Sub51` 只有 `effCreateAngle`，无 `et*` | — | ❌ 不存在 | 发弹 sub 无「双载体」（已按名字建图核查）|
| ★ 激光槽（每敌人 32 个）| `ecldata6sp.txt:Sub55:1701` `laserId(0)` + `:1704` `laserOn(0, 6, %F0, 0.0f, 0.0f, 640.0f, 640.0f, 8.0f, 60, 540, 60, 60, 60, 0)` + `:1718` `laserTest(0)` | 我们只挂弹槽 `props[16]`，**没有激光位** | ❌ | 12 份 / 97 条指令；见 §3-② |

统计口径：发射指令点 **2008**（familiar 522 / body 1486）；`etOffset` 实参 423 处 = 零 210 / 动态 185 / 常量 28；`etEx` 1269；激光 97；`familiarCreateA` 214 / `familiarCreateF` 425。

## 3. 结论：cover 不了 / 勉强（按 频率 × 影响 排序，只给补法不实现）

1. **同帧/换帧多原点**（3 个 sub 达 25 点；84 组换 offset）：影响最大 —— 它直接否掉「1 槽 : 1 壳」的身份与「打死壳⇒关这门炮」的语义。
   补法（**0 新站点**）：认下「一槽一壳」当**视觉代理**，壳只锚该帧**最后一次** `etOn` 的原点（或干脆锚母机）；闸退回「整个槽一起开/关」。站点侧不用动。
2. **激光槽**（12 份 / 97 条，`laserOn` 45 条）：TH08 有一整条平行通道（`laserSlots[32]`、`selectedLaserSlot`、`laserOffset` 可重定位、`laserRotate/laserAim/laserAngle` 独立瞄准）。我们模型里没有它的位置。
   补法（**1 新站点 + 1 张表**）：在 TH18 侧激光生成点另立 hook，套同一套「口→壳」；代价 ≈ 站点 1 个 + 卡侧一张 `laser_slot→fam_id` 表。🟡 TH18 侧的激光生成点未查（本任务范围外）。
3. **临时离位与绕飞偏移**（138 组：非零 `etOffset` 后 ≤4 行内一次发射；`mathCirclePos` 110 处）：**弹对、壳错** —— 壳读的是**活**偏移字段（`youkai.c:570` → `enemy+0x6084+i*0xc`），而 TH08 的写法常在发弹后立刻 `etOffset(0, 0)`（9 组显式收回）⇒ 壳只在那一帧闪一下。
   补法（**0 新站点**，2 选 1）：(a) 站点 `th18:0x433ffa` 就地 3 次写，把发弹时定格的 `props[i]+0x08/+0x0c/+0x10` 抄给卡侧（= `HANDOFF-108-EMITTER-MODEL.md` §3a）；(b) 或壳改读 `offset_final` 而非活偏移。代价：站点 3 次写 / 卡侧改 1 处采样点。
4. **弹的变换（`etEx` 1269 处）**：不是炮位问题，但 TH08 的观感大量靠它（弹开花/绕转/加速）。**本模型不碰**；若要复刻需要一整套 `transforms[18]` 语义，登记为另案。
5. **场锚 −`%SELF_X`**（6 文件 12 sub）：同 3 的补法；额外注意壳会横跨全屏跳，建议只做 (a) 不做插值。

## 4. 🟡 不确定项

| # | 未定 | 现状 |
| --- | --- | --- |
| 1 | `familiarCreateF`(op92) 与 op90 的建位差 | decomp 两 case 都调 `SpawnChildAtScriptPosition`，但 92 另设 `positionOffset = 父 position` + `inheritParentPosition`；只此一源 |
| 2 | 激光细节 | `laserSlots[32]`@`+0x3280`、`selectedLaserSlot`@`+0x3300` 来自 decomp `C_ASSERT`，未在 exe 逐条复核 |
| 3 | 调用图口径 | 按**名字**建图；`callAsync(0, -1)`（不换 sub）没跟；timeline 的 `enmCreate(id,…)` 是数字 id，未映回 sub 名 |
| 4 | `funcCall` 182 处 | 只映到 `EclGlobals.cpp:65` 那张 id 表（id 16 未用、id 27 仅 1 处）；其余 id 作用未逐条核 |
| 5 | ★ VA 陷阱（会误导后来者）| `local/_scratch/c108/th08.asm` 这类 objdump 平坦 dump 的地址 = **真 VA − `0x200`**（th08；th18 为 − `0xc00`，因 `.text` 的 `Raw≠VA`）。本文件全用真 VA |

## 5. 相关

- 我们的模型：`HANDOFF-108-EMITTER-MODEL.md`（§3a 与之呼应；§1.4/§2 关于 `etEx`、`etFan` 首参的描述需按本文 §0-3 更正）
- 关槽调研（scratch）：`local/_scratch/c108/RESEARCH-emitter-disable.md`（§A.2(3) 的「发射口槽表」应作废）
- 使魔是什么：[`01-familiars-and-anm.md`](../../../engine/enemy/th08/01-familiars-and-anm.md) · 使魔计数：[`03-familiar-count-vs-hp.md`](../../../engine/enemy/th08/03-familiar-count-vs-hp.md)
- TH18 槽与波次：[`07-emitter-slots-waves-and-phase-transitions.md`](../../../engine/enemy/th18/07-emitter-slots-waves-and-phase-transitions.md)
- TH08 重建索引（vendor，只读）：`local/vendor/th08-decomp/src/`（`EclRunHigh.inl` / `EnemyManager.hpp` / `BulletManager.hpp` / `EclDependencies.cpp`）

## 6. 本轮处置（2026-10-01，按 owner 分派）

### ① 已实现：**壳读「发弹时定格的 `offset_final`」**（138 组 / `mathCirclePos` 110 处）
· 现象：`etOffset(非零) → et* → etOffset(0,0)` 在同一帧内就把活偏移清回 (0,0)，而我们是**每帧采样**
  ⇒ 壳只在那一帧闪一下（「一闪一缩」）；
· 修法（**0 新站点**）：在**已在用**的 `th18:0x433ffa`（`yk_on_shot_slot`）里把 `etOn` 刚写好的
  `props[i]+0x08/+0x0c/+0x10`（**offset_final**）抄进卡侧每槽小缓存 `s_emit_latch[]`（8 组环形，按
  `(enemy_id, slot)` 取最新），壳的目标位置**优先**用这个定格值（没发过弹 ⇒ 回落活偏移）；
· 判据（日志）：`youkai/emit: latched offset_final for slot i of enemy_id N = (x,y,z) at the etOn site …`
  （前 8 次）+ 台账行新增 `latched=N` ⇒ 实跑看到**壳停在那一刻的出点**（不再一闪一缩）。

### ② 口径：**一槽一壳 = 视觉代理**（同帧多原点）
TH08 一帧最多 **25 个出点**（`laserSlots[32]` 之外的多原点 fan/circle 族），我们 1 槽 : 1 壳**无法表达**
⇒ **接受「一槽一壳 = 视觉代理」**，并写清好处：**打死壳 ⇒ 该槽的 25 个口一起关**
（正好是 owner 的主需求「打掉使魔 ⇒ 关闭这个发射口」）。登记 `TUNING.md` §2.1ccc-41。

### ③ 未来工作：**激光槽**（TH08 `laserSlots[32]`，12 份 ECL / 97 条指令）
我们模型只有弹槽 `props[16]`，**没有激光位** ⇒ 要用需 **1 个新站点** + 卡侧 `laser_slot→fam_id` 表；
TH18 侧激光生成点**未查（🟡）**。**本轮不做**，代价与入口记在此处。

### ④ objdump 平坦 dump 的地址差（普查者给的一手）
反汇编平坦 dump 时：**th08 的地址 = 文件偏移 − `0x200`**、**th18 = − `0xc00`**
（即 dump 里带的是 `imagebase` 后的 VA）⇒ 交叉引用地址前先减掉这个差。
