# 108 · TH18 原生 ECL（全 22 份）「发射口」用法普查 ⇄ 我们的槽/壳模型

> **版本**：跨版本。本文出现的地址一律带版本前缀（`th18:0x433ffa`、`th08:0x422720`）。
> **方法**：只读。① 22 份原生 ECL（`local/th18.v1.00a/dat/default.ecl` + `st01..st07`×{stage,bs,mbs} = `dat/` 里的全集）
> 用 `thecl -d 18 -m local/vendor/eclmap/eclmap/th18.eclm` 重新 dump 到 `local/_scratch/c108/th18ecl2/*.txt`（与既有的
> `local/_scratch/st0*.asm` **逐字节相同**），全量机扫（脚本 `local/_scratch/c108/scan_th18_emit.py`）+ 抽样人读；
> ② 语义用**真 VA 平坦反汇编** `local/_scratch/th18_full.asm` + `local/th18.v1.00a/th18.exe` 原始字节现读
> （派发表 `th18:0x4366f4`/`th18:0x436438` 自己解过一遍，opcode↔handler 与 `local/_scratch/research-108-shot-slot-off.md` §1.1 逐条相同）；
> ③ 卡侧**只读**（`native/cards/youkai.c` / `youkai_core.c` / `youkai_core.h`）。
> **未做**：未改任何 `.c/.h`、未 make、未 release、未实跑；**未改任何人的卡/共享件**。
> **可信度**：用法分类与计数 ✅（22/22 全量机扫，判据写在 §2 表内）；`etOn`/`laserOn`/`laserNew`/`etDist` 的一手字段语义 ✅（逐指令）；
> `laserNew` 四个浮点的可见语义、`th18:0x4351f5` 的 opcode 归属 🟡（§5）。

## 0. 结论（最短版）

1. **TH18 的发射口 = 敌人自己的 16 个 `props[i]`（`zEnemy+0x17c4+i*0x488`）**，弹与激光**共用**这批槽 —— TH18 **没有** TH08 那种独立的 `laserSlots[32]`。
2. 原生 ECL 的用法主体是 **①本体口 + ②同一槽偏移随波重掷**（`etNew` 276 次 vs 位置声明指令 80 次）⇒ 我们现有模型（槽→壳、壳钉在活/定格偏移算出的世界点）**覆盖得了这两类**。
3. **cover 不了 / 勉强**（按频率 × 影响，§3 详列）：**① 激光整条通道**（95 条指令 / 8 文件，其中生成光束的 34 条；我们的闸只在 `etOn` 里）；**② 一帧多原点**（33 个 sub；1 槽 : 1 壳表示不了 N 个原点）；**③ `etDist`（42 次）**——它把子弹生成点从 `offset_final` 再沿角度推 `dist` px，壳与弹**不同点**；**④ `type==0` 的激光槽**（5 处，`yk_slot_type()` 直接看不见）；**⑤ `etCopy`（2 次）**——两个槽同位 ⇒ 两只壳完全重叠。
4. **「能不能做到使魔自己发弹」：弹这条路能（0 新站点），激光那条不能（≥1 新站点，而站点预算已超线）。**
5. ⚠ **一手更正 owner 口径里的算术**：`props[i]+0x08/+0x0c/+0x10` 是**世界坐标**（`th18:0x433f9e` 相对路已经把 `final_pos` 加进去了；`th18:0x4251ea` 原样抄进子弹）
   ⇒ 要改成「壳的位置」应当**直接写壳的世界坐标**，写成「壳的世界坐标 − 敌人坐标」会**再减一次敌人位置**、弹跑到场外。
6. 顺手结掉老 🟡：`etCount(slot,[-1],1)` 的 `[-1]` **不是字面 −1**，是 thecl 的**难度分支占位**（全库 `[-1]`/`[-1.0f]` 共 375 处，**100%** 紧邻 `!E/!N/!H/!LO/!*` 块；一手 `th18:0x48d4f0`）。

## 1. 对照基准（我们的模型，只读复核）

| 环节 | 一手 / 现状 | 行为 |
| --- | --- | --- |
| 槽 | `props[i] = &zEnemyData + 0x598 + i*0x488`（= `zEnemy+0x17c4+i*0x488`），16 个 | `th18:0x434013` `lea eax,[esi+0x598]`（`esi = i*0x488`、`edi = &zEnemyData`）|
| 开火 | `etOn(slot)` = opcode 601 / handler `th18:0x433f34`；在 `th18:0x433ffa` 汇聚到 `call th18:0x427810` | 同帧**可重入**；单次调用发 `count_1×count_2` 发（双层循环 `th18:0x4278b4`）|
| 原点 | `props[i]+0x08/+0x0c/+0x10` = **世界坐标**（相对路 `th18:0x433f9e` 加 `final_pos`；绝对路 `th18:0x433f8a`） | 真发只读它：`th18:0x4251ea` `movq xmm0,[esi+0x8]` → 抄成子弹位置；`th18:0x427810` 用它做 atan2 瞄准 |
| 活跃 | `props[i].type(+0x00) != 0`（`youkai.c:565` `yk_slot_type`） | 只有 `etNew` 清零、`etSprite`（`th18:0x434032`）写它 |
| 壳 | 身份 `(enemy_id, slot)`；位置 = 该槽**发弹时定格的** `offset_final`（站点抄回，`youkai.c:6163`）| 跟随 = 瞬移（`YK_EMIT_FOLLOW_FRAMES = 0`，`youkai_core.h:1318`）|
| 闸 | 站点 `th18:0x433ffa` per-call 否决 `etOn`（`retaddr = th18:0x43402d`）；窗 `f3 0f 10 8f 44 51 00 00` ✅ 已复核字节 | **只在 `etOn` 里**；无 `etOff`、无槽使能位 |
| 绝对模式 | `etOffsetAbs`（628 / `th18:0x43414e`）写 `origins[i]`（`+0x4f18/+0x4f1c`）并把 `origins[i].z` 置 `th18:0x4b9178`（= 1.0f）| `etOn`/`laserOn` 按 `origins[i].z > th18:0x4b916c`（= 0.9f）分流；我们已有 `YK_ORIGIN_ABS_THRESHOLD` ✅ |

## 2. 普查表（22/22 全量；样本给 sub / 行 / 实参，行号 = `thecl -d 18` dump 的行号）

| 用法类别 | TH18 原生样本（sub / 行 / 实参） | 我们模型怎么覆盖 | cover | 能做到「使魔自己发弹」吗 | 若不能：缺什么 / 建议补法（地址 + 代价，不实现） |
| --- | --- | --- | --- | --- | --- |
| ① 本体口（相对 + 偏移恒 0） | `st03bs.txt:99 Boss1_at`：`etNew(0)`→`etSprite(0,10,4)`→`etOn(0)`，全程无 `etOffset*` | `yk_emit_on_body()`（`youkai_core.c:382`）整格跳过，不建壳；弹照发 | ✅ | 不需要（口本来就在本体） | —（`etNew` 276 vs 位置声明 80 ⇒ 这是原生多数派） |
| ② 偏移随波重掷（跨帧） | `st06.txt:426 GirlA01_at`：`etOffsetRad(0,%RANDRAD,%RANDF2*_f(32))`→`:428 etOn(0)`→`:429 wait(2)`，10×10 波；`st04mbs.txt:91 MBoss1_at`：半径 64、`:93 etOn(0)`、`wait(5/20)` | 壳每帧钉在「活偏移/定格 latch」算出的世界点（瞬移）；`yk_emit_world_pos` `youkai_core.c:363` | ✅ | **能（现状即近似）**：弹的原点 = 壳的钉位；差 1 帧（§4.3） | 观感另案（滑行 `YK_EMIT_FOLLOW_FRAMES`，`youkai_core.h:1318`） |
| ③ ★ 一帧多原点（同槽同帧 N 次 `etOn`） | `st05bs.txt:1135-1160 BossCard4_at2`：同帧 6 次 `etOn($A)`（各配 `etAngle`+`etSprite`）；`st03.txt:791-806 GirlD01_at`：5 组 `etOffsetRad+etAngle+etOn`；`st03bs.txt:99 Boss1_at`：**无 wait 嵌套循环** ⇒ 一帧 $B×$C 次 | 引擎能；但 1 槽 : 1 壳表示不了 N 个原点，latch 只留最后一次 | ❌（表示力） | 能但**塌缩**：N 个原点全落到壳上；TH08 里这 N 个是 N 个不同原点 | 认下「一槽一壳 = 视觉代理」（同 `HANDOFF-108-EMITTER-COVERAGE.md` §3-1）；**0 新站点** |
| ④ ★ 非本体位置：绝对锚定 | `st06bs.txt:140 Boss1Card_at`：`etOffsetAbs(0,%PLAYER_X2,%PLAYER_Y2)`（**锚在自机身上**）；`st06bs.txt:1805/1808`：`±192.0f`；`:1838/1842`：`0/448.0f`；`st07bs.txt:2489-2519 BossCard9_at2`：同槽 2 的四个场边点 | `YK_ORIGIN_ABS_THRESHOLD`（`youkai_core.h:1060`）已分流；壳位置 = `origins+offsets` ✅ | ✅ | 能（写 `offset_final` 即可） | —（22 次 / 4 文件；「取消绝对模式」的 `origins.x < th18:0x4b947c`(−990) 路：实参里**无此字面量**，🟡 变量实参未穷举） |
| ⑤ ★ 非本体位置：`etDist` 再推一段 | `st06bs.txt:141 Boss1Card_at`：`etDist(0,-72.0f)`→`:142 etOn(0)`；`:147/149/151/153 etDist(1,-88/-104/-120/-136)` 各配一次 `etOn(1)` | ❌ 壳位置 = `offset_final`，**不含 dist**；一手 `th18:0x4251f9` 读 `props+0x24`，非零则 `th18:0x425219` 算 `dist·(cos,sin)` 加到子弹位置（`th18:0x425222`） | ❌ | **勉强**：不覆写时壳与弹差 `dist` px；覆写也只解决原点那一半 | 站点里**同时**写 `props[i]+0x24 = 0`（清 dist）；**0 新站点**、1 次写。42 次 / 7 文件 |
| ⑥ `etCopy`（整槽拷贝 ⇒ 两口同位） | `st02bs.txt:611 BossCard2_at`：`etCopy($A+1,$A)`；`st06bs.txt:143 Boss1Card_at`：`etCopy(1,0)` | 拷贝 `props[j]`→`props[i]`（handler `th18:0x433ea2`，`rep movs` 0x122 dword）含 offsets/origins ⇒ 下帧两槽都活跃 ⇒ 两只壳**完全重叠** | 🟡❌ | 能（两槽各写各自 `offset_final`） | 低频 2 次；要么接受重叠、要么按 `(enemy_id, i)` 的「同位置去重」建一只（卡侧，**0 新站点**） |
| ⑦ `etSprite(slot,0,·)` 显式拆口 | `st07bs.txt:1530`（`etSprite(0,0,8)`）、`:1846`（`etSprite(1,0,2)`）、`:2157/2174/2191`（槽号是变量 `$B`）| `type==0` ⇒ 不活跃 ⇒ 不建壳；已建的壳走 `fam_seen/broken` 路 | ✅ | 不适用 | —（共 **5** 处；`engine/enemy/th18/07` §2.1 记的 2 处是只算了字面槽号） |
| ⑧ 激光（同一批槽的另一条通道） | `st05bs.txt:800-826 BossCard2_at`（`laserNew`+`laserTiming`+`laserStOn`+`wait([-1])` ×300）；`st03bs.txt:749/767/772`；`st07mbs.txt:289/298`（实参见 §3-1） | ❌ 我们的站点 `th18:0x433ffa` 在 `etOn` 里；`laserOn`（702 / `th18:0x4350c3`）自己按 `offsets/origins`+敌人位置重算原点（`th18:0x435104`–`th18:0x435196`）⇒ **打死壳激光照发** | ❌ | **不能（0 站点）** | **1 新站点**：所有激光生成都过 `th18:0x448920`（四个调用点见 §3-1）；代价 = 1 站点 + 卡侧 `laser_slot→fam_id` 表。⚠ 站点预算 47 > 42 ⇒ 现在加不了 |
| ⑨ `type==0` 的激光槽（模型看不见） | `st07bs.txt:1525 BossCard3_at`：`etNew(0);etSprite(0,0,8)`（type=0）+`laserNew(0,-1,-1,-1,16)`+循环 `etOffsetAbs(0,D+rand,rand)`/`laserCuOn(0)`；`st07bs.txt:2151 BossCard7_at`：3 槽全 type=0 | ❌ `yk_slot_type()` 只认 `type!=0` ⇒ 这些槽**根本不进我们的槽表**（不建壳、不进闸） | ❌ | 不能 | 与 ⑧ 同一处补法；或放宽卡侧活跃判据（`type!=0` 或激光计时字段非 0，`props+0x458..0x468`），**0 新站点但语义变宽**（见 §3-4） |
| ⑩ 口与实体解耦（原生真实存在的一条） | 激光是**独立对象**：`th18:0x448920` 每次 `malloc 0x1e0c`，管理器 `th18:0x4cf3f4`；敌人死了激光还在，靠段首 `laserCancel`（545 / `th18:0x435e21`，61 次）收 | ❌ 我们只有弹槽；「敌人不在、口还在发」**只有激光成立** | ❌ | 不适用（要另做一套） | 若要，就得同时有 ⑧ 的站点 + 激光对象寿命表（`laser_slot→fam_id`）；代价同上 |
| ⑪ `etCount(slot,[-1],1)` 的 `[-1]` | `st06.txt:413`（上面块 `!E 1;!N 1;!H 3;!LO 3`）⇒ `count_1 = 1/1/3/3`；全库 79 次 | 我们不用这个实参；老 🟡 **结案** | ✅ | 不适用 | 一手：`th18:0x48d4f0` 按 `arg+0x8` 的 rank 位掩码取每难度值；`[-1]` 是 thecl 占位符（375/375 紧邻 `!` 块，0 例外） |
| ⑫ 变量槽（`etNew($A)`） | 56 次（`st05bs.txt:800` 的 `$A`、`st02bs.txt:601` 的 `$A`）；槽号由 sub 形参/循环给 | 身份用**解析后**的槽号（handler `th18:0x438aa0` 解参）| ✅ | 能 | — |
| ⑬ 口挂在别的对象上 / 跨敌人共享槽 | **不存在**：`etCopy` 的 `edi` 是同一敌人的 `&data`；`props` 长在 `zEnemyData` 里；TH18 原生 ECL 里 `enmCall`(800)/`bossCall` **0 次** | — | ❌ 不存在 | 不适用 | 无需覆盖 |

统计口径（可复算：`python3 local/_scratch/c108/scan_th18_emit.py`）：
`etNew` 276（字面 220 + 变量 56）/ `etOn` 305 / `etSprite` 284（type=0：5）/ `etOffset` 22 / `etOffsetRad` 36 / `etOffsetAbs` 22 / `etDist` 42 / `etCopy` 2 /
`etCancel` 139 / `etClear` 152 / `etClearAll` 7；激光 `laserNew` 10 / `laserTiming` 7 / `laserOn` 3 / `laserStOn` 7 / `laserStAngle` 5 / `laserCuOn` 2 / `laserCancel` 61；
一帧多原点：文本组 25 + 无 wait 循环组 19 ⇒ **33 个不同 sub**。

## 3. 结论：cover 不了 / 勉强（按 频率 × 影响 排序，只给补法不实现）

1. **激光整条通道**（95 条指令 / 8 文件，生成光束的 34 条；生成唯一出口 `th18:0x448920`）。影响最大：boss 战里「打掉使魔 ⇒ 关这个口」对激光**完全不成立**，因为闸只在 `etOn`。
   补法：**1 新站点**（`th18:0x448920` 入口，调用点 `th18:0x435273` / `th18:0x43545f` / `th18:0x4355cb` / `th18:0x435756`）+ 卡侧 `laser_slot→fam_id` 表。
   ⚠ 站点预算 47 > 42，**现状加不了**（要么先减档，要么这条挂着）。原生实参：
   `st05bs.txt:800-826 BossCard2_at` = `etNew($A);etSprite($A,38,$I0);laserNew($A,512,512,0,16);laserTiming($A,[-1],20,60,10,0);laserStOn($A,0);wait([-1])` ×300；
   `st03bs.txt:749/767/772` = `laserNew(1,0,256,0,16)`+`etOffset(1,0,-40)`+`laserOn(1)`×5（无 wait 循环）；`st07mbs.txt:289/298` = `laserNew($A,16,160,0,16)`+`laserOn($A)`×12。
2. **一帧多原点**（33 个 sub；最大 6 次/帧同槽）。不是"能不能发"的问题，是"1 槽 : 1 壳"的**表示力**。
   补法：**0 新站点** —— 明确写死「一槽一壳 = 视觉代理」，闸退回整槽一起开/关（与 `HANDOFF-108-EMITTER-COVERAGE.md` §3-1 同一结论）。
3. **`etDist` 让弹与壳错位**（42 次 / 7 文件）。补法：**0 新站点**，站点里多写 `props[i]+0x24 = 0`（1 次写）；或把壳放在 `offset_final + dist·方向`（要读 `props+0x14` 角度，贵）。
4. **`type==0` 的激光槽**（5 处，`st07bs`）。补法：同 ⑧，或放宽卡侧活跃判据（`type!=0 || laser_time_* != 0`，`props+0x458..0x468`）；**0 新站点**但要一次实跑对账（怕把普通空槽算进来）。
5. **`etCopy` 同位双壳**（2 次）。补法：**0 新站点**，卡侧建壳前按位置去重（`|Δ| < 1px` 且同一敌人 ⇒ 复用壳）。
6. **已是 ✅ 的**：本体口 / 跨帧重掷偏移 / `etOffsetAbs` 绝对锚（含锚自机）/ 变量槽 / `[-1]` 占位符 —— 都不用动。

## 4. 「使魔自己发弹」专项（owner 追加判据）

### 4.1 机制（一手）与一处口径更正

- 站点 `th18:0x433ffa` 是 `etOn` 里 `offset_final` **已经算好**、`call th18:0x427810` **之前**的唯一位置；从站点到 `call` 只有 `th18:0x434002`–`th18:0x43401a` 六条指令 ⇒ **本站点写入即最终值，不会被后续指令覆盖** ✅。
- `th18:0x427810` 拿 `props+0x08` 对自机做 `atan2`（`th18:0x427810` 开头 `movss xmm0,[eax+0x624]` / `[eax+0x620]` 减 `[edi+0xc]`/`[edi+0x8]`）⇒ **瞄准也是从这个点出发**；`th18:0x4251ea` 再把它原样抄成子弹位置。
- ⚠ **这三格是世界坐标**（相对路 `th18:0x433f9e`–`th18:0x433ff4` 已经把 `final_pos` 加进去）⇒ owner 口径的「壳的世界坐标 − 敌人坐标」**会再减一次**，正确写法是**直接写壳的世界坐标**。
- `etAim`/`etAngle`/`etOffsetRad` 的先后：它们写的是**别的格**（`+0x470`、`+0x14`、`+0x4e58`），站点在生产端的下游 ⇒ 不存在"被覆盖"的问题 ✅。

### 4.2 三条风险逐条核

| 风险 | 判定 | 一手 |
| --- | --- | --- |
| 被后续指令覆盖 | **不会** | 站点后仅 `th18:0x434002`–`th18:0x43401a`，直接 `call th18:0x427810` |
| 同槽同帧 N 次 `etOn` 全塌到壳上 | **观感变了，但形状保留** | TH18 单次 `etOn` 本就发 `count_1×count_2` 发（`th18:0x4278b4`）；TH08 的 N 组 `etOffset+et*` 是 N 个不同原点（同一支模板）⇒ 塌缩丢掉"一口多点"，不是丢"一只使魔出一片弹" |
| **自反馈（重点）** | **会，只要「先写后读」；按「先读后写」则无** | latch 就在同一站点读 `props+0x08`（`youkai.c:6163`）⇒ 必须**先把引擎算出的值抄进 latch，再覆写**。先覆写再抄 = 壳的目标恒等于壳自己 = **冻结**。这是一条一次性的时序约束，不需要额外状态 |

### 4.3 还有一个时序事实（决定这一手值不值得做）

卡 tick 优先级 `0x16` **早于**敌人 ECL `0x1b` ⇒ 本帧站点运行时，壳还停在**上一帧**的原点：

| 方案 | 弹的起点 | 壳的位置 | 结果 |
| --- | --- | --- | --- |
| 现状（不覆写） | 本帧 `O_k` | 上帧 `O_{k-1}` | 每帧/每 2 帧重掷的写法里最多错 1 帧（st6 半径 ≤32px 那一类） |
| 覆写 `offset_final := 壳当前位置` | `O_{k-1}` | `O_{k-1}` | 弹与壳**同点**，但弹滞后一帧 |
| 站点里**把壳挪到** `O_k` | `O_k` | `O_k` | 完全同点、无滞后；**需要能从 hook 里调用的「移动使魔」API**（现在只有 tick 里的 `yk_pin_familiar`）|

⇒ **现状（壳钉在 latch 的原点）已经让「弹从壳出」近似成立**；覆写的价值在：壳改走「锚本体 / 插值滑行」策略时，仍保证弹与壳同点，且与「打死壳 ⇒ 关该槽」的闸语义自洽。

### 4.4 最少改动（**不实现**，只记代价）

- **弹（0 新站点）**：站点 `th18:0x433ffa` 内「① 先抄 latch；② 写 `props+0x08/+0x0c/+0x10` = 壳的世界坐标；③（可选，见 ⑤）写 `props+0x24 = 0`」。
  风险：站点极热（每槽每次 `etOn`），且 `props+0x08` 还有别的读者（`th18:0x4351f5` 在 `laserOn` handler 里、`th18:0x4353c2` 在 `laserStOn` handler 里）⇒ 需一次实跑对账（§5-3）。
- **激光（1 新站点）**：`th18:0x448920` 入口挂同一套「槽→壳」，套 `laser_slot→fam_id` 表；**站点预算 47 > 42 ⇒ 现在加不了**（见 [`07-emitter-slots-waves-and-phase-transitions.md`](../../../engine/enemy/th18/07-emitter-slots-waves-and-phase-transitions.md) §7-1）。
- **不建议**：改 `props[i].type` 来"关激光" —— `laserOn`（`th18:0x4350c3`）不读 type。

## 5. 🟡 不确定项（addresses 记真 VA）

| # | 未定 | 现状 |
| --- | --- | --- |
| 1 | `laserNew(slot, a, b, c, d)` 四个浮点的可见语义 | handler `th18:0x434b3f` 写 `props+0x08/+0x0c/+0x10/+0x448`；`laserOn` 里**现算的原点**与 `props+0x08` 同时进描述符 ⇒ 哪个是光束可见起点未定（细节见 §5.1）|
| 2 | `laserStOn(slot, n)` 第二参 | 疑似子光束序号：`st07bs.txt:1936-1944` 用 1..5，配 `laserStAngle(1..5, …)`（`:1952-1956`）🟡 |
| 3 | `props+0x08` 的其它读者 | `th18:0x4351f5`（∈ `laserOn` handler `th18:0x4350c3`）与 `th18:0x4353c2`（∈ `laserStOn` handler `th18:0x43527d`）都读它；两者都在同一个大函数体（`FUN_00430d30`，`th18:0x430d30`）内 ⇒ 覆写 `props+0x08` 的副作用面没画全 |
| 4 | ★ VA 陷阱 | 裸 `objdump` 平坦 dump `th18.exe` 的地址 = 真 VA − `0xc00`；`thecl` dump 不带地址（详见 §5.2）|
| 5 | TH08 侧对照 | 只用既有文档（`HANDOFF-108-EMITTER-MODEL.md`），未重新复核 |

### 5.1 `laserNew` 的四个浮点

实参见到的三组并存：`512/512/0`（`st05bs.txt:804`）、`0/256/0`（`st03bs.txt:749`）、`-1/-1/-1`（`st07bs.txt:1533`，显然是"不改"哨兵）。
`laserOn`（`th18:0x4350c3`）里，**现算的原点**（`th18:0x435104`–`th18:0x435196`）与 `props+0x08`（`th18:0x4351f5`）**同时**被填进描述符 ⇒
哪个是光束的可见起点未定。差一次断点（在 `th18:0x448920` 里看它读描述符的哪一格）。

### 5.2 VA 陷阱（会误导后来者）

本文用 `local/_scratch/th18_full.asm`，**已是真 VA**：`th18:0x433ffa` 处字节 `f3 0f 10 8f 44 51 00 00` 与 PE 直读一致。
若用**裸 `objdump`** 平坦 dump `th18.exe`，地址 = 真 VA − `0xc00`（`HANDOFF-108-EMITTER-COVERAGE.md` §6④）；
`thecl` 的 dump **不带地址**，本表行号 = `thecl -d 18` 文本行号。

## 6. 相关

- 我们的模型：[`HANDOFF-108-EMITTER-MODEL.md`](HANDOFF-108-EMITTER-MODEL.md) · 上一轮 TH08 普查：[`HANDOFF-108-EMITTER-COVERAGE.md`](HANDOFF-108-EMITTER-COVERAGE.md)
- TH18 槽/波次/阶段：[`07-emitter-slots-waves-and-phase-transitions.md`](../../../engine/enemy/th18/07-emitter-slots-waves-and-phase-transitions.md)
- TH08 使魔是什么：[`01-familiars-and-anm.md`](../../../engine/enemy/th08/01-familiars-and-anm.md)
- 关槽一手研究（scratch）：`local/_scratch/research-108-shot-slot-off.md` · 本轮机扫脚本：`local/_scratch/c108/scan_th18_emit.py` · dump：`local/_scratch/c108/th18ecl2/*.txt`

## 本轮处置（2026-10-01，按 owner 分派）

### ① 已回滚：`etDist` 归零（owner 硬约束：「**只能不发弹，不能改发弹位置**」）
⛔ 本项**已按 owner 2026-10-01 原话回滚**（不写 `props+0x24`），且「覆写 `offset_final`/偏移让弹从壳出」
一类方案全部**禁止**；站点只允许「读事实 + 否决 `etOn`」。保留下面这段只为记录**曾尝试过什么**与一手出处：

（历史）
一手：`th18:0x4251f9` 读 `props+0x24`，非零就把子弹生成点**再沿角度推 dist px**（`th18:0x425219`）
⇒ 壳与弹**不同点**（全库 **42 次 / 7 文件**用 `etDist`）。修法：在**已在用**的 `th18:0x433ffa`
对**我们的槽**（表里有条目 ⇒ 建过壳/在闸内）把 `props[i]+0x24 = 0`；
**顺序写死：先抄 latch，再清零**（注释已写）。判据：台账行多一格 **`dist0=N`**。

### ② 登记（不改代码）
1. ★ **激光整条通道**（95 条 / 8 文件；生成光束 34 条；唯一出口 `th18:0x448920`，被
   `0x435273/0x43545f/0x4355cb/0x435756` 调）：`laserOn`(op 702) **自己重算原点** ⇒ **打死壳激光照发**。
   要修需 **1 新站点 + `laser_slot→fam_id` 表**，而**站点预算 47 > 42 ⇒ 现状加不了**
   ⇒ **记为「阻塞 + 未来工作」**（`TUNING §2.1ccc-42`；先腾站点才能做）。
2. **一帧多原点**（33 sub；最大 `st05bs:1121 BossCard4_at2` 同帧 **6 次** `etOn($A)`）⇒ 一槽一壳 = 视觉代理
   口径已在册；补一句：**同帧 N 次 `etOn` 会全塌到壳上**（形状保留、观感变）。
3. **`type == 0` 的激光槽**（5 处：`st07bs:1530/1846/2157/2174/2191`）⇒ `yk_slot_type()` 不认
   ⇒ 不进我们的槽表；与第 1 条合并登记。★ **订正** `engine/enemy/th18/07` 里「2 处」的说法为 **5 处**。
4. **`etCopy`**（`st02bs:611`、`st06bs:143`）⇒ 两壳完全重叠；**登记为已知现象**（不修）。
5. ★ **更正**：`props[i]+0x08/0x0c/0x10` 是**世界坐标**（相对路已加 `final_pos`）⇒
   想「弹从壳出」应当**直接写壳的世界坐标**，**不是**「壳 − 敌人」（前一条写法作废）。
6. **顺手结掉老 🟡**：`etCount(slot,[-1],1)` 的 `[-1]` = thecl 的**难度分支占位**
   （全库 375 处 **100%** 紧邻 `!E/!N/!LO` 块；一手 `th18:0x48d4f0` 按 rank 掩码取值）
   ⇒ `HANDOFF-108-EMITTER-MODEL.md` §5-1 与 `engine/enemy/th18/07` 各补一句「**已结案**」（只加注，不改结论）。
