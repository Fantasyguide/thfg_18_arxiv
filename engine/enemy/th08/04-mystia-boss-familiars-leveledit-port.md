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
3. **人/妖侧外观分流没了**（TH08 `enemyType = IsYoukai()?0:2`；移植只有一套图）。
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
- **只能新做（❓）**：**圆环特效**（TH18 无对应；TH08 是哪支 anm/effect **未查**）；**拖尾**（TH18 无落点）；
  **人/妖侧外观分流**（TH18 敌机无侧概念）；**5 槽 mask 暗场**（TH18 全 22 个 ECL 里只有 `st06bs.ecl:1873-1879`
  用单槽 `anmSetSprite(3,22)+anmMove`；vanilla `st02bs` 完全不用 `anmMove/anmScale/anmAlpha` ⇒ 多槽是移植方自创）；
  **「常驻」本身**（原作没有）。

## 6. ❓ 未解
1. **TH08 圆环特效的 anm/贴图是哪支**（`linkedEffect@0x53c8` 的 `AllocEffectSlot(0x20,…)` 对应哪份 anm/script/interrupt）
   —— 决定「环」能不能复刻（已查 eclm / decomp / `engine/enemy/th08/*`，均无落点）。
2. **TH18 `enmCreateF`(309)/`enmCreateAF`(310) 的 F 是不是「跟随父敌」** —— 若是，移植改用它就能便宜地补上"跟随"
   （`engine/ecl/th18/05-stage1-midstage-spawns.md:58` 只写「F 变体」❓）。
3. `MystiaChild28` 的 `wait(600)` 是否有原作依据（判为移植方自定）。
4. 本体走动动画 script22-25 为何从不调用（还原度可补）。
5. 建议先读：`engine/anm/th18/04-anm-slot-selection.md`（`anmSelect(N)`/`anmSetSprite(K,S)` 语义）、
   `engine/enemy/th18/08-parent-enemy-id-and-sub-enemies.md`、`engine/enemy/th08/01-familiars-and-anm.md`。
