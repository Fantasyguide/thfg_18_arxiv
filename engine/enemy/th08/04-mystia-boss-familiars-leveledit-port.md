# TH08 米斯蒂亚（boss）的使魔 —— 以及 TH18 modkit 那个「二面关底改米斯蒂亚」补丁怎么做的

> **版本**：TH08 v1.00b/d 与 TH18 v1.00a 并引；地址/行号一律带前缀（`th08:` / `th18:`）。
> 本文是**第三方补丁的拆解 + 原作一侧的一手**，供 TH08 侧实现者接手；**结论全部给证据**。
> 补丁来源：modkit `upstream/main`（`2d1cd51`）的 `thcrap/repos/leveledit/th18_leveledit_stage02_mystia/`，
> 其 git 历史显示该目录是外部工具 **`th18-leveledit`** 的构建产物（提交全是「同步构建产物（th18-leveledit …）」）。

## 1. ★★ 先纠正口径：**原作 boss 的使魔不常驻**

| | 玩家侧「妖侧使魔」（108 卡的复刻对象）| **boss 的使魔**（本文对象）|
| --- | --- | --- |
| 对象 | `Player::options[4]`（`Player.hpp:176-189`，`struct PlayerOption`）| **真 `zEnemy`**（sizeof 0x53d0），挂在施放者的**子敌人链**上 |
| 驱动 | `plyNNa.sht` / `plyNNas.sht` + `g_SpiritOptionInitCallbacks`/`UpdateCallbacks` | **ECL opcode 90/91/92**（`familiarCreateA` / `familiarCreate` / `familiarCreateF`）|
| 生命周期 | **常驻**（整关跟自机）| **一波一次**：每段攻击开头 `enmKillAll()`（op95）清场，本波再 `familiarCreate` |
| 会被弹打到 | 不会 | **会**（有 hitbox / HP / 掉落）|

⇒ **「让使魔常驻」是设计决定，不是复刻**（原作那 54 处使魔每波重建）；真正常驻的是
**玩家侧**那套（= 108 在做）。唯一共同点：**都读 `g_Player.IsYoukai()`**（boss 使魔的外观/圆环随人/妖侧变），
并共用圆环特效配方（`linkedEffect@+0x53c8`）。

## 2. TH08 侧的一手（原作那场是什么）

- **实体身份**：`familiarCreate*` = ECL opcode **90/91/92**（`th08.eclm:88-90`）。**不是独立对象类**，
  而是普通 `zEnemy` 挂到施放者的子敌人链：`prevSubEnemy@0x4` / `nextSubEnemy@0x8` / `ownerEnemy@0x2da4` /
  `subEnemyCount@0x3380`（`EnemyManager.hpp:102/103/134/214`）。
- **生成**：`EclManager.cpp:2143-2226`（case 89/90/91）→ `GetLastSubEnemy`(`th08:0x41efc0`) +
  `InitBulletPattern`(`0x41f110`)/`InitBulletPatternAbs`(`0x41f280`) → `SpawnEnemy2`(`0x42a680`)；
  随后 `ownerEnemy = 施放者`、`subEnemyCount++`。参数 =（子程序名, x, y, life, drop, score），
  **life 落 `enemy+0x2dfc`**（stage2 使魔 HP = 600/100/20 等）。
- **旗标**：`trailFamiliarSet(1)`（op **83**，handler `0x41e532`）写 `enemy+0x3328 anmFlags` bit1；
  `anmFamiliar`（op 174）另一位。**每只使魔一个圆环特效**：生成时 `AllocEffectSlot(0x20,…)`，
  每帧把 `enemy+0x2d88` 拷进去，`SetInterrupt(IsYoukai()?2:1)`（`EclManager.cpp:2156-2165`）。
- **人/妖侧分流**：生成时 `flags` bit11 = `g_Player.IsYoukai()`，**`enemyType = IsYoukai()?0:2`**
  （`EclManager.cpp:2150-2151`）。
- **那种使魔自己的 ECL 形态**（`Sub28` 行 807-831）：`anmSet(57)` + `trailFamiliarSet(1)` + `anmRotSet(1)` +
  `hitboxSet(24,24)` + `setInvuln(30)` + `%F0=%ANGLE_PLAYER+%F1` + 3×`etEx` 发射口 + `etFan(7,4,…)` +
  `etOn/etOnAutoDelay(16)` + `moveDir(%F0,0.5f)` + `moveAccel(0.0283)`；+60 停加速、+5000 `delete()`。
  stage2 只用了 90/91（**没用 92/F，即没有 TRACK_POS 跟随**）。
- **米斯蒂亚这一场的数量**（`th08:ecldata2.ecl` 反汇编行号）：

| 父 sub | 子 sub | 次数 | 行号 |
| --- | --- | --- | --- |
| Sub27 | Sub28 | **7** | 790,792,794,796,798,800,802 |
| Sub29 | Sub30 | **16** | 877…912 |
| Sub35 | Sub37（`familiarCreateA`）| 2 | 1099,1108 |
| Sub40 | Sub43（`familiarCreateA`）| 2 | 1244,1253 |
| Sub44 | Sub47 / Sub49 | 6 / 5 | 1366-1377 / 1400-1408 |
| Sub52 | Sub49 / Sub55 | 2 / 2 | 1569,1580 / 1600,1611 |
| Sub58 | Sub59 | 12 | 1768…1795 |
| **boss 段合计** | | **54** | （道中另有 32 处，属 `st02.ecl`）|

## 3. 那个补丁怎么做的

### 3.1 `st02bs.ecl` = **TH18 格式的「TH08→TH18 逐指令机器翻译」**（不是原件、也不是纯手写）
判据：① 容器是 TH18 的（`SCPT` + ver 1 + `ANIM`/`ECLI` 段），TH08 `ecldata2.ecl` 头完全不同；
② 子程序 **61** 个（TH08 是 65）；③ **同号命名** `TSub26…TSub61` ↔ TH08 `Sub26…Sub61`；
④ 逐条对拍（TH08 `Sub26` 704-740 ↔ 补丁 `TSub26` 511-623：同弹幕同循环、opcode 一一对应）。
翻译规则（一手行号）：

| TH08 | 补丁 | 行号 |
| --- | --- | --- |
| `iset($I4,$I0)` | `$E = $A`（局部 var 全改成 19 个形参 A..S，调用点补 0）| :706 / :516 |
| `etEx(i,…)` | `etExSet(sub,i,…)` | :707-708 / :517-518 |
| `etFan(3,10,1,1,3.0f,0.5f,%F0,0.2618,8786)` | `etSprite`+`etAim`+`etCount`+`etSpeed`+`etAngle` 五条（尾参 8786 丢）| :718-721 / :533-537 |
| `ins_5(0,label,$I4)` | `$E=$A; … if ($E>0) goto label` | :726 / :572-574 |
| `%F0/%F1/%F2/%F4/%F7` | `%I/%J/%K/%M/%P` | — |

### 3.2 `st02enm.anm`（+30 entry / 30 script）：贴图**全部取自 TH08 且像素/字节级相同**
| 新 entry | 内容 | 出处（一手）|
| --- | --- | --- |
| 27 | 夜盲遮罩 `mystia/mask.png`（sprite69 128×128 + sprite70 1×1 黑）| **TH08 `etama.anm` sprite229**（`data/etama/etama5.png`），**像素级 identical** |
| 28 | 本体 `mystia/boss.png`（sprite71..82，48×64，4×3）| **整张 PNG 与 TH08 `stg2enm.anm` 的 `data/stgenm/stg2enm.png` 字节级相同**；script21..26 ↔ TH08 script0..5（脚本号 +21、sprite +71、加 `layer(7)`、去 `posMode(1)`）|
| 29 | **使魔** `mystia/familiar.png`（sprite83..90，32×32）| **整张 PNG 与 TH08 `enemy.anm` 的 `data/stgenm/enemy.png` 字节级相同**；script28/29 ↔ TH08 `enemy.anm` **script57/58 逐帧同形**（4 sprite、`+4:`、`jmp(offset12,0)`；只删了 TH08 独有的 `ins_25(1)` 并加 `layer(7)`）|

★ 注意：**TH08 `enemy.anm` 的 script 57/58 正是 108 卡当初判定「不移植」的那两套** ⇒
**boss 使魔的外观脚本与贴图现在已经随这个补丁躺在 modkit 里**（TH08 agent 可直接沿用）。

使魔外观映射**100% 忠实**（TH08 `anmSet` ↔ 补丁）：Sub30:919=57、Sub37:1146=57、Sub43:1312=57、
**Sub47:1444=58、Sub49:1485=58、Sub55:1659=58**、Sub59:1802=57。
调用点（补丁）：script28 ← TSub28:767 / TSub30:1104 / TSub37:1459 / TSub43:1740 / TSub59:2671；
script29 ← TSub47:2058 / TSub49:2129 / TSub55:2410。

### 3.3 `front.anm`
382 script 不变、13 entry（原 12）：新增 entry12 `mystia/hud-name.png`（sprite186，256×32）；
**唯一改动 = script155** 的 `sprite(sprite104)` → `sprite(sprite186)`（其余逐字相同）⇒ 二面 boss 名条改指向米斯蒂亚。

### 3.4 `.msg`
`st02a–d.msg` 四段对话被替换（1964/1808/1900/2012 vs 原 2300/2096/2120/2460 B）。

## 4. 移植里使魔**有**，数量逐 sub 全对（54 = 54）
| TH08 父→子（次数）| 补丁（`st02bs.ecl` 行号）|
| --- | --- |
| Sub27→Sub28 ×7 | `enmCreate("MystiaChild28",0,0,600,0,0)` ×7（671,685,699,713,727,741,755）|
| Sub29→Sub30 ×16 | `MystiaChild30` ×16（864…1091）|
| Sub35→Sub37 ×2 | `enmCreateA("MystiaChild37",%Q-192,%R,50,0,0)` ×2（1363,1385）|
| Sub40→Sub43 ×2 | ×2（1590,1612）|
| Sub44→Sub47 ×6 / Sub49 ×5 | ×6（1796…1870）/ ×5（1916…1972）|
| Sub52→Sub49 ×2 / Sub55 ×2 | ×2（2185,2215）/ ×2（2254,2284）|
| Sub58→Sub59 ×12 | ×12（2500…2659）|

**缺的 7 条**（否定证据：补丁反汇编里 `amiliar` 出现 **0** 次；`enmCreate*` 只用 300/301）：
1. **`trailFamiliarSet(1)`（op83）⇒ 拖尾/残影没了**（TH18 无此 opcode）。
2. **圆环特效（`linkedEffect@0x53c8`）完全没了**（TH08 生成时内联分配、每帧跟随、按人妖侧 `SetInterrupt(1/2)`）。
3. ~~**人/妖侧外观分流没了**（TH08 `enemyType = IsYoukai()?0:2`；移植只有一套图）。~~
   ★ **2026-09-30 改判（一手，见 §7 + §9）**：那条 `enemyType = IsYoukai()?0:2` 写进去的是
   **`enemy+0x332f` 绘制层桶**，**不是**样式/图集/脚本 ⇒ **TH08 里本来就没有「随侧换外观」这件事**；
   而「随侧变化」这一类（环的颜色/存在、火花的开关…）是 **DLL 的活** ⇒ 从「移植缺项」改为
   **「不属于移植补丁的义务」**（owner 口径见 §9）。
4. **生成语义换了**：TH08 = 父敌子敌人链；TH18 = `enmCreate`/`enmCreateA` 建普通敌机
   （TH18 只保留 `parent_enemy_id`（`enemy+0x6834`，`enmCreate` 自动写），**「跟随父敌」行为没有**）。
5. **掉落/分数清零**：`familiarCreate(...,600,-2,100)` → `enmCreate(...,600,0,0)`。
6. **存活时长**：TH08 `Sub28` 活到 +5000（或段首 `enmKillAll`）；移植 wrapper 是 `wait(600); delete();`（600 帧）。
7. **若干 sub 没有对应 `TSub`**（TH08 Sub32/50/51/56/57/60/62/63/64，含夜盲浮点动画与 boss 死/逃段）；
   另 stg2enm script22-25（本体走动）**拷了但从不调用** ⇒ 移动时不转向。

## 5. 交接单（给 TH08 侧）

**先与用户确认口径**：原作 boss 使魔**不常驻**（每波重建）；要「常驻」= 设计决定。

| 候选载体 | 入口 | 能否承载 boss 使魔 | 代价 |
| --- | --- | --- | --- |
| **A. 就用移植 ECL 自己 `enmCreate`（现状）** | `st02bs.ecl` 的 54 处 | **已经能做，且与原作逐个数对齐** | 代价 0；只需补上面 ①②③⑥ 四条语义 ⇒ **建议作为基线** |
| B. 108 的绕转使魔池 `yk_orbit_*` | `youkai.c:1271/1395`、`youkai_core.c:1764`、`YK_ORBIT_MAX 48` | 部分 | ① `yk_orbit_scan` **显式排除 boss**（`youkai.c:1295`）⇒ 要动**别人的卡**（须按 `CLAUDE.md` 在 AUDIT §BE 登记）；② 台数按父敌最大 HP 分档，对不上原作 7/16/2/2/6/5/2/2/12；③ 只在装了 108 卡时活 |
| C. `parent_enemy_id` + 自建 hook | `enemy+0x6834` | 部分 | TH18 没有 TH08 子敌人链；`enmKillAll` 就是对位物，A 已够 |
| D. TH18 原生 option | `Player::options` | **不能**（boss 没有 option 系统）| — |
| E. 112 的 token VM 池 | `animal.c:1534`、`ufo.c:389` | 只解决「画」 | 不解决实体/命中/计数 |

**TH08 侧「必须常驻」的到底是什么**：boss 使魔**不常驻**；**每只使魔身上的圆环特效常驻**（生成时分配、
每帧跟随、随使魔死）；**玩家侧妖侧使魔常驻**（`Player::options[4]`）—— 那才是常驻系统。

**复刻 / 新做 的分界**：
- **复刻**（有原作出处）：使魔数量与时机（54 处表）；使魔外观 script（TH08 `enemy.anm` 57/58 = 补丁 28/29，
  sprite 101-108 = 83-90）；本体外观（TH08 `stg2enm.anm` 0-5 = 21-26，sprite 0-11 = 71-82）；
  夜盲贴图（TH08 `etama.anm` sprite229）；夜盲动画数值（TH08 `Sub50/56/57/60` 的 `floatTime`：440→320/288/256/224、120 帧；
  192/128/96/64、1800 帧 —— 补丁 `MystiaMaskAnimate` 逐条对上）；使魔行为（`Sub28` 的 `etEx`×3 /
  `etOnAutoDelay(16)` / `moveDir(0.5)` / `moveAccel(0.0283)` / +60 停加速）；符卡名与数值。
- **只能新做（❓）**：**圆环特效**（✅ **已解 2026-10-01**：TH08 = `AllocEffectSlot(0x20,…)` → 模板表 `th08:0x4c6d30[0x20]`
  = `etama` **script48** / sprite197 **62×62** 空心环、加法、每帧跟随、`interrupt 1=人紫(192,128,255)/2=妖蓝(96,96,255)`、
  8 帧长到 0.5(人)/0.8(妖) 后常驻、`interrupt 3` = 死时淡出 10 帧；卡 108 已移植为 `ability.anm` **309**）；
  **拖尾**（✅ **有落点 2026-10-01**：TH18 **anm v8 原生**有 `scriptNewRoot`(504)/`scriptNewPos`(505)/`scriptNewRootPos`(506)
  —— `local/th18.v1.00a/anm/enemy/enemy.anm.txt` 的 script91 **每帧** `scriptNewRoot(script89)` 撒加法粒子，
  script89 = `layer(9) + blendMode(1) + spriteRand(sprite120,8) + scaleTime(10,1,0,0) + +10 delete()`
  ⇒「敌机自己周期性撒加法火花」可以**纯 anm** 复刻（不必 DLL、不必额外敌人）；
  **人/妖侧外观分流**（✅ **2026-10-01 复核：TH18 敌机确实无侧概念** —— ECL 全 opcode 名里无 youkai/focus 类；
  引擎侧读玩家聚焦位 `player+0x476cc` 的 18 处**全是自机侧**函数（`Player__*` / `Card*` / `Bullet__try_kill_player`）；
  TH18 的外观变体机制是 `anmSelect`(302) 写 `zEnemyData+0x264 selected_anm_index` + `anmSelectedPlay`，即**由 ECL 脚本选**）；**5 槽 mask 暗场**（TH18 全 22 个 ECL 里只有 `st06bs.ecl:1873-1879`
  用单槽 `anmSetSprite(3,22)+anmMove`；vanilla `st02bs` 完全不用 `anmMove/anmScale/anmAlpha` ⇒ 多槽是移植方自创）；
  **「常驻」本身**（原作没有）。

## 6. ❓ 未解
1. **TH08 圆环特效的 anm/贴图是哪支**（`linkedEffect@0x53c8` 的 `AllocEffectSlot(0x20,…)` 对应哪份 anm/script/interrupt）
   —— 决定「环」能不能复刻（已查 eclm / decomp / `engine/enemy/th08/*`，均无落点）。
2. ✅ **已解（2026-10-01）**：**F 不是「跟随父敌」**，而是「**`EnemyManager.boss_ids[0]`（`mgr+0x48`）那只 boss 已不在
   活动敌机链表里才建**」的额外门 —— 分派 case `th18:0x430ddb`（309/310/311/312 共用）：
   `push [mgr+0x48]` → `th18:0x42d500`（按 id 在 `mgr+0x18c` 链里查，非 0 = 还在）→ `jne th18:0x436417`（跳过）；
   300/301/304/305 走 `th18:0x430df1`（无此门）。⇒ **移植不能靠 F 补「跟随」**。
   引擎侧也**没有**任何每帧读 `parent_enemy_id`（`enemy+0x6834`）的代码：全 exe **1 写点**（`th18:0x42d8e7`）
   + **2 读点**，且两处都是 ECL 取值器（`th18:0x4377a8` int / `th18:0x438472` float）—— int 口径的变量 id = **−9909**
   （表 `th18:0x437978` index 91）。**「跟随」的两条可行路**：① 纯 ECL：子敌每帧
   `enmPos(%F0, %F1, <父 id>)` + `movePos`（`enmPos` = op801、case `th18:0x4361eb`：arg2 = 目标 id →
   `th18:0x42d500` 查表 → `th18:0x409980` 取坐标，分别写进两个目标变量；`enmAlive` = op555、`th18:0x4361c4` 判活）；
   ② DLL 侧（卡 108 的 `yk_fam_*` 已是这个形状）。详见
   [`../../ecl/th18/05-stage1-midstage-spawns.md`](../../ecl/th18/05-stage1-midstage-spawns.md) 的出怪变体表（同日本次补 ✅）。
3. `MystiaChild28` 的 `wait(600)` 是否有原作依据（判为移植方自定）。
4. 本体走动动画 script22-25 为何从不调用（还原度可补）。
5. 建议先读：`engine/anm/th18/04-anm-slot-selection.md`（`anmSelect(N)`/`anmSetSprite(K,S)` 语义）、
   `engine/enemy/th18/08-parent-enemy-id-and-sub-enemies.md`、`engine/enemy/th08/01-familiars-and-anm.md`。


## 7. ★ 2026-09-30 一手：`enemy+0x332f`（那份 `enemyType = IsYoukai()?0:2` 落点）到底是**什么**

**发现**：三个使魔 create 变体（opcode 90/91/92）都把 `IsYoukai()?0:2` 写进 `enemy+0x332f`
（`th08:0x41afe5` / `0x41b195` / `0x41b345`；另有 `th08:0x41c6f0` 与两处常量写 2 / 0）。

**验证（全 exe 只有两个读点）**：
- `th08:0x42de11`：`movzx eax,BYTE PTR [enemy+0x332f]` → `mov eax,[ctx+eax*4+0x9dcedc]` →
  `mov [enemy+0],eax`，随后 `th08:0x42de3d` 把**当前节点**写回 `[ctx+layer*4+0x9dcedc]`
  ⇒ 这是**链表头插**：`ctx+0x9dcedc + layer*4` 是**每层一条绘制链表**的表头，节点 +0 存 `next`。
- `th08:0x42e17f`：`mov edx,[ctx+eax*4+0x9dcedc]` ⇒ 绘制时**按层遍历**这条链表。
- 那 6 个 dword 在 `EnemyManager::Initialize` 里被清零（`th08:0x42c789`–`0x42c7b9`），
  与**图集槽**是**另外两个地址**：`0x9dceec` = `enemy.anm`、`0x9dcef0` = `stgNenm.anm`
  （`th08:0x42ec23` / `th08:0x42ec9e` 装载）—— **`layer` 不选图集**。

**结论（✅ 一手）**：`+0x332f` = **绘制层桶索引**（0 = 妖侧创建、2 = 人侧创建），只影响
「插进哪条绘制链表 / 谁先谁后」，**不影响** `anmSet(K)` 选的脚本、也不换 `anm` 图集
⇒ **人/妖侧的使魔外观完全一样**（同一支 `enemy.anm` script，同一套图）。
⇒ owner 的疑问有答案：**低速不会改 boss 使魔的样式**；TH18 侧没有这种「同层竞争」的场景，
我们不做任何分流（若哪天要照做，等价物是**世界 VM 的绘制层号**，值本身只决定遮挡顺序）。
🟡 残余：另外那几个写点（`0x41c6f0` / 常量 2 / `0x42c498` 写 0）分别属于哪些非使魔对象，
没有逐个归类；「同一支脚本在两个层里会不会有别的差异」也只证到「图集与脚本都不变」这一层。

## 8. 夜盲遮罩（`mystia/mask.png`）：**我们的判断**（不改代码）

补丁用了 **5 个 mask 槽**（`multi-slot`）来压暗场地 —— 这是**移植方自创**：vanilla `st02bs.ecl`
**不用** `anmMove/anmScale/anmAlpha` 去驱动遮罩，它只有**一张** `etama.anm` sprite229（128×128）
+ sprite70（1×1 黑）这套素材。我们的判断：**保留补丁的做法**（TH18 侧没有 TH08 那套
`floatTime` 浮点动画，5 槽是补丁为了在 TH18 的 ECL 能力内复现「逐渐压暗」而做的近似），
但要在文档里标明**它是 ❓补丁自创、不是 TH08 一手**；若将来要贴一手，方向是把
TH08 `Sub50/56/57/60` 的 `floatTime`（440→320/288/256/224、120 帧）照抄成 TH18 的动画脚本。


## 9. 口径定案（owner 2026-09-30）：**人侧 = anm 的基础层，妖侧 = DLL 提供的变化**

> **owner 逐字**：「不用核了，**一手只有人侧使魔不变，使魔变是绑定在人妖上的**。那么就很简单了，
> **没有 dll 的时候 == 人侧 == 只有一种使魔**；**有 dll 才有妖侧，才需求使魔变，正好有 dll 可以使魔变**。」

**定案**：
- **人侧 = anm 的基础层**：`st02enm.anm` / `st02bs.ecl` 这套移植产物给出的**唯一形态**，
  **没有 108 卡（DLL）时看到的**就是它 —— 所以移植补丁**没有义务**做任何「随侧变化」。
- **妖侧 = DLL 提供的变化**：环的存在与颜色（`SetInterrupt(1/2)`）、拖尾火花的开关
  （`flags_lo bit11 == 0` 那一支）、人妖侧外观分流 —— 这些都**由 DLL 在做**，
  「有 DLL 才有妖侧，才需求使魔变，正好有 DLL 可以使魔变」。
- **一手事实（★ 2026-09-30 二次订正，见 §10）**：`flags_lo bit11` **不是生成时定死** ——
  `FUN_0042c420` **尾段每帧都在刷**它（`th08:0x42c552`/`0x42c55e`/`0x42c563`）⇒ 实质是
  **「按当前侧（差一帧）」**；`enemy+0x332f` 则只在**生成时**写（且只当绘制桶，见 §7）。
  ⇒ owner「一手人侧使魔不变」的含义 = **不切侧就没有变化**（不是「人侧那套里没有变化逻辑」），
  与上面「随侧变化由 DLL 提供」**不矛盾**：anm 出**人侧基础层**、DLL 出**随侧的实时变化**。
- **对 §4 缺项表的影响**：凡涉及「人/妖侧」的条目，一律归到「DLL 的活、不属于移植补丁的义务」。


## 10. ★ 2026-09-30 二次订正：**`flags_lo bit11` 每帧刷新**（按当前侧，不是按生成时的侧）

> 上一节的初版写着「`flags_lo bit11` 与 `enemy+0x332f` 都是生成那一刻定死」—— **前半句错了**，
> 本条订正（同一手来源，地址已核）。

| 量 | 什么时候定 | 一手 | 行为 |
| --- | --- | --- | --- |
| **`flags_lo bit11`**（= 当前侧）| **每帧刷新** | `th08:0x42c552 shl $0xb` / `th08:0x42c55e and $0xfffff7ff` / `th08:0x42c563 or`（`FUN_0042c420` 尾段）| **按当前侧（差一帧）** |
| 拖尾火花 | **看 bit11**（`th08:0x42c49f` 读 `flags_hi` bit1）| 人侧：`0x42c4b1 push $0x2` → `0x42c4d3 SpawnEffect(0x26)` **每 2 帧**持续撒；切妖：同一支的 `bit11 != 0` 分支**直接跳尾**（只有一次性 `SpawnEffect(0x1e)` + `SetInterrupt(1)` + SE `0x27`）⇒ **停止撒**；切回人侧：**下一帧起又开始撒** | 跟当前侧 |
| **环色** | 生成时 + **切侧那一帧再改一次** | `th08:0x41b1e7`–`0x41b1fe` `SetInterrupt(IsYoukai()?2:1)`；人紫 `(192,128,255)` / 妖蓝 `(96,96,255)` | 跟当前侧 |
| **外观脚本 `anmSet(K)`** | **永不随侧变** | 引擎里**没有**按侧选 anm/sprite 的路径；`enemy+0x332f` 只被当绘制桶下标（`th08:0x42de11`，见 §7）| 与人妖侧**无关** |
| ★ **使魔自己的 ECL 可按侧分叉** | 生成时写、每指令读 | `enemy+0x3330`（`0x20` 人 / `0x40` 妖，**唯一写点** `th08:0x42c585`）由 `EclManager::RunEcl` 的**每指令条件门**读（`th08:0x4185c6`–`0x4185ec`）；**只有使魔有这个字节** | 按侧分叉 ECL |

⇒ **结论**：TH08 的「随侧变化」= **实时**的（差一帧），来源是 `bit11` 的每帧刷新 + 环的切侧重发中断；
**外观脚本永远一套**。我们卡侧**就是按当前聚焦状态**撒火花、环**按当前侧**换色 ⇒ **与一手一致**（不需要改）。
🟡 残余：`thecl` 把 `enemy+0x3330` 那些位打成 `*`，**看不出哪些指令用了它**（要逐 sub 反汇编才能列全）。

## 11. ★ A 方案已落地（2026-09-30）：改动在上游仓库的分支，不在本仓

**口径**（owner）：「**都开强强联合，只开一个也能用**」⇒ 往本仓 patch 里放 `th18/st02enm.anm`
会与 leveledit 补丁**同名冲突**（thcrap 后加载者赢 ⇒ 开两个反而坏一个）⇒ **B 方案作废**，
改动搬去上游仓库。

| 项 | 值 |
| --- | --- |
| 仓库 | `Fantasyguide/th18-leveledit`（**私有**；裸 HTTPS 连不上，必须 SSH：`git@github.com:Fantasyguide/th18-leveledit.git`） |
| 分支 / 提交 | **`mystia-familiar-trail`** / `115e824`（URL：`https://github.com/Fantasyguide/th18-leveledit/tree/mystia-familiar-trail`） |
| 改的源 | **`mods/stage02-mystia/assets.py`**（该仓是**生成式**：spec 由 Python 生成，不是手写 anm）—— 使魔那段（原 126–138 行）之后追加 entry30 + script30，并把 `script28/29` 的每个 `+4:` 拆成两段 `+2: scriptNewPos(script30,0,0)` |
| 加的东西 | **entry30** `mystia/fam-trail.png` 16×16（= TH08 `etama.anm` sprite219 `(0,96,16,16)` 同格；sprite 号 = `max+1` = 91，**不能用 0**，会撞 entry0 的 sprite0）+ **script30** = TH08 `etama` script71 零翻译（加法 / 20 tick / `alpha=48+rand(64)` / `color=128+rand(64)`×3 / `scale=1.1+rand(1.0)→0` / `+20: delete()`，子 VM 带 `layer(7)/type(0)`） |
| 构建 | README 的正路 = `python scripts/thcrap.py release stage02-mystia`（会**推送另一个仓** `Fantasyguide/th18_modkit`）；本次只跑了等价分步：`thanm -c18 <anm> spec.txt -m maps/anm/v8.anmm` + 回读 `-l18`/`-x18` + `scripts/lint.py` |
| lint | `fsetRand` 在库内 56 个 ANM 无先例 ⇒ 按该仓机制加 `// lint-allow: anm-no-precedent <一手出处>` ⇒ **0 error / 0 warning** |
| 验证到哪一步 | 编译 + 回读 + 与原产物**逐块对比**：除那三处编辑外逐块一致；**未实机**（也没跑完整 `release`） |
| 许可证 | 该仓**没有 LICENSE**（只有 `maps/anm/LICENSE-truth-Apache-2.0.txt`、`legacy/.../LICENSE.thtk.txt`）⇒ **不能 vendor 进本仓**；正路 = 在该私有仓开分支/提 PR（owner 有权限） |

**未闭合 / 要盯**：
1. **`scriptNewPos` 子 VM 的锚定**：一手说 `th18:0x479b30`–`0x479b5d` 只写 `__pos_2`、之后不跟父 ⇒
   火花留在撒点；**若引擎其实每帧重锚，拖尾会糊在使魔身上** —— 只能实机看观感。
2. 子 VM 预算：单台 ≈ 10 支并发（2 tick × 20 tick 寿命），一场几十台 ⇒ 几百支；TH18 anm VM 池
   够不够**未核**（失败模式 = 火花静默变少，不是崩）。
3. 本机 TH08 输入 sha 与 manifest 不符（`thdat -x8` 报 `incorrect entry magic`）⇒ 用了已解包的
   `local/th08.v1.00d/dat/`（其中 etama/enemy/stg2enm/eff02 的 sha 与 manifest 一致）。
4. 本仓保留的**配方/预览工具**：`assets/mystia/build_st02.py`（默认输出到 gitignored 的
   `local/_scratch/c108/build/`；⚠ **不要**指回本仓 `patch/th18/`）；上游侧的笔记在
   `local/_scratch/leveledit-up/NOTES.md`。

