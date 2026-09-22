# 01 · TH08（永夜抄）**敌机使魔**：是「真敌人 + familiar 旗标」，不是独立对象类

> **版本**：TH08 v1.00d（`th08.exe`，imagebase `0x400000`，样本 `local/th08.v1.00d/`；反编译 `local/vendor/th08_dec`）。
> 本文裸地址默认属该版本；引用其他版本须写成 `th18:0x…`。
> **方法**：一手 —— ① ECL 反汇编：`local/vendor/thtk/build/thecl/thecl -d 8 -m local/vendor/eclmap/eclmap/th08.eclm <dat/ecldata*.ecl>`
> （24 个文件、52725 行，本机解到 `/tmp/th08ecl/`，命令可复算）；
> ② PE 直读自解 ECL 分派表（`th08:0x41ecbe`，index = `opcode−1`）拿 handler 地址，再 `objdump -d` 逐指令读；
> ③ ExpHP 结构体名 `local/vendor/th-re-data/data/th08.v1.00d/type-structs-own.json`。
> **可信度**：opcode 号↔handler↔语义 ✅（两条独立路径，见 §1 的自检）；「使魔是普通敌人」✅；
> 「每波重建」的脚本证据 ✅（§3）；anm 脚本号 ✅（§4）；`flags_hi` 两位的具体含义 🟡。
> **用途**：卡 108（TH18 移植永夜抄的人妖系统）—— owner 口径见
> [`WORK-PACKAGES.md`](../../../mods/th18.v1.00a/card-expand/WORK-PACKAGES.md) §10 第 6⑥/6⑦ 行；
> TH18 侧的对应机制（波次 / 换阶段 / `enmKillAll`）见
> [`../../enemy/th18/07-emitter-slots-waves-and-phase-transitions.md`](../../enemy/th18/07-emitter-slots-waves-and-phase-transitions.md)。

## 0. 一句话结论

1. **使魔 = 普通的 `zEnemy`**：ECL opcode **90/91/92** = `familiarCreateA` / `familiarCreate` / `familiarCreateF`
   （签名 `NffSSS` = 子程序名 / x / y / life / drop / score）→
   `Enemy::create_enemy_familiar_2` `0x41f110`（90、92）/ `Enemy::enemy_create_familiar` `0x41f280`（91）→
   `EnemyManager::create_enemy` `0x42a680`。**它有自己的 ECL 子程序、自己的 HP、自己的掉落**。
2. 但引擎**另有一套「familiar 旗标」**挂在同一个敌人上：`zEnemy.flags_hi`（`enemy+0x3328`，一手字段名 `flags_hi`，
   与 `+0x3324 flags_lo` 相邻）由 `trailFamiliarSet`（opcode **83**，handler `0x41e532`）与
   `anmFamiliar`（opcode **174**，handler `0x41e589`）置位；另有
   `enemy+0x3380 __familiar_count__but_not_total`（**每次生成 +1**）、
   `enemy+0x53c8 familiar_circle_effect`（`struct zEffect*`）、
   `AnmManager+0x6c01c familiar_effects[0x80]`、`+0x15304 familiar_popups[0x80]`。
   ⇒ 准确说法是「**真敌人 + 使魔旗标 + 圆环特效**」，不是「另一种对象」。
3. **「每一波重建」是脚本行为，不是引擎行为**：boss 每一段攻击开头都 `enmKillAll()`（opcode **95**）**清光上一波
   （含使魔）**，本波的循环再 `familiarCreate` 重新生成 —— 全 24 个 ECL 里 `familiarCreate` 共 **1016 处**、
   `enmKillAll` 段首调用与 TH18 的段首 `enmKillAll()` **同构**（§3）。
4. **使魔的动画 = 它自己那支 ECL 子程序的 `anmSet(K)`**，K 落在 **`enemy.anm` script 53…58**（§4）：
   53/54/55/56 = 8 帧循环（sprite 69..76 / 77..84 / 85..92 / 93..100）、57/58 = 4 帧（101..104 / 105..108）。

## 1. opcode 号与 handler（自检过的两条独立路径）

分派器 `th08:0x418623 jmp DWORD PTR [edx*4+0x41ecbe]`（index = `opcode−1`）。我自己按 PE 节表读表复算了一遍：

| opcode | 名（`th08.eclm`）| handler | 自检 |
| --- | --- | --- | --- |
| 54 | `anmSet` | `0x419812` | 与 `assets/ability/make_youkai_familiar_art.py` 文件头一致 |
| 83 | `trailFamiliarSet` | `0x41e532` | 本次新解 |
| 90 / 91 / 92 | `familiarCreateA` / `familiarCreate` / `familiarCreateF` | `0x41af5b` / `0x41b10b` / `0x41b2bb` | 与生成器文件头**逐条相同** |
| 95 | `enmKillAll` | `0x41da7d` | 本次新解 |
| 174 | `anmFamiliar` | `0x41e589` | 本次新解 |

> 自检的价值：分派表基址 `0x41ecbe` 与「index = opcode−1」这两件事只要错一处，全表就全错。
> 我的复算把 opcode 90 解成 `0x41af5b`，与另一份独立记录（生成器文件头，来自另一轮会话）逐字相同。

## 2. 使魔的字段（一手）

`familiarCreate`（`0x41f280`，取参顺序）与 `familiarCreateF`（`0x41f110`）都 `call 0x42a680 EnemyManager::create_enemy`，
在生成后写：

| 参数 | 去处 | vendor 名 |
| --- | --- | --- |
| 第 1 个（子程序名）| 新敌人的 ECL 子程序 | — |
| `x` / `y`（float）| 出生坐标 | — |
| `life` | `enemy+0x2dfc` | `life` |
| `drop`（实测多为 `-2` / `-1` / `1`）| `enemy+0x3304` | `drop_main` |
| `score`（实测多为 `100` / `10`）| `enemy+0x2e08` | `score_reward` |
| — | `add DWORD PTR [ctx+0x3380],1` | `__familiar_count__but_not_total` |
| — | 读 `enemy+0x2e0c` 决定要不要翻转使魔角速度 | `__flag_field_used_by_familiars__see_0x41b246` |

`trailFamiliarSet(K)`（`0x41e532`）逐指令：`K & 1` ⇒ 把 `enemy+0x3328`（`flags_hi`）的 **bit1（0x2）** 置/清
（`and 0xfffffffd` → `or (K&1)<<1`）。
`anmFamiliar(K)`（`0x41e589`）先 `[enemy+0x53c8]+0x350 = 0`（`familiar_circle_effect` 里的一个字节），
再按 `K & 1` 置 `flags_hi` 的另一位（🟡 具体位号未逐条反完）。
⇒ 这两条 opcode 就是「这只敌人算不算使魔」的官方旗标集合；**TH18 没有对应 opcode**（`th18.eclm` 里 grep `familiar` = 0 条）。

## 3. ★「每一波重建」= 段首 `enmKillAll()` + 本波重新 `familiarCreate`（一手脚本证据）

### 3.1 段首清场（与 TH18 同构）

`local/th08.v1.00d/dat/ecldata4b.ecl`（4 面 boss）逐处（`thecl` 反汇编行号）：

```
    ins_129(2);            # 本段/本符卡的设置
    setInterrupt("Sub22");
    etOnAuto(0);
    enmKillAll();          # ★ 上一波的敌人（含使魔）在这里被清光
    etSound(-1, -1);
```

同一形状出现在 `:313` / `:1038` / `:1208` / `:1392` / `:1638` / `:1847` / `:2069`（= 每一个攻击段的段首），
`enmKillAll` 在 24 个 ECL 里共 **157 处**。
⇒ **TH08 的使魔生命周期就是「一波一次」**：上一波的使魔在段首被杀掉，本波由脚本重新生成。
这正是 owner 说的「使魔是每一波发弹重建的」的程序事实 —— 而且 TH18 的段首也有同样的 `enmKillAll()`
（[TH18 篇](../../enemy/th18/07-emitter-slots-waves-and-phase-transitions.md) §3/§4）：**清场是两作共有的引擎/脚本行为，
差别在我们这边「清掉之后没人重建」。**

### 3.2 本波重建（循环生成）

`ecldata8.ecl` 的 `sub Sub10()`（一个会撒使魔的中型敌）逐条：

```
Sub10_328:
    familiarCreate("Sub12", 0.0f, 0.0f, 100, -2, 100);   # 生成一只使魔（普通敌人）
+5: //9
    ins_5(4, Sub10_328, $I4);                            # ← $I4 = 6：这条循环发 6 只，每只隔 5 帧
+100:
    fset_sub(%F0, %ANGLE_PLAYER, 0.7853982f);
    normRad(%F0);
    familiarCreate("Sub12", 0.0f, 0.0f, 100, -2, 100);   # 第二波：按 %F0 逐只 +0.3927 rad（22.5°）扇开
    fadd(%F0, 0.3926991f); normRad(%F0);
    familiarCreate("Sub12", 0.0f, 0.0f, 100, -2, 100);
    …（再 3 只，共 5 只）
```

（`ins_5(n, label, counter)` = TH08 的循环/跳转指令，handler `0x418682`；`$I4/$I11` 是 ECL 局部整数变量。）

### 3.3 使魔自己干什么（它不是一个「贴图」，而是**发射口的载体**）

```
sub Sub12()                          # 使魔自己的 ECL
{
    anmSet(58);                      # ★ 它的外观 = enemy.anm script 58
    trailFamiliarSet(1);             # ★ 打上「使魔」旗标（flags_hi bit1）
    anmRotSet(1);
    hitboxSet(24.0f, 24.0f);         # 判定盒 24×24（与 108 卡的 YK_FAM_HURTBOX_HALF=12 同源）
    setInvuln(30);
    moveDir(%F0, %F1);               # 按创建时给的角速度飞出去
+1:
    flagClear(2);
    jmp_neq($I4, 6, 1, Sub12_168);
    callAsync(0, "Sub13");           # ★ 只有「循环里那 6 只」才去撒下面的东西
Sub12_168:
+5000:
    delete();
}

sub Sub13()                          # 使魔的子任务
{
    iset($I0, 60); iset($I4, 7);
Sub13_40:
    enmCreate("Sub14", 0.0f, 0.0f, 0.0f, 100, -2, 100);   # ★ 每 10 帧再生成一只 Sub14
    isub($I0, 8);
+10:
    ins_5(0, Sub13_40, $I4);
    ret();
}

sub Sub14()                          # ★ 真正扛发射口的那一层
{
    flagSet(8); wait($I0);
    iset($I4, 16);
Sub14_52:
    fset_mul(%F8, %RANDF2, 32.0f); fset_mul(%F9, %RANDF2, 32.0f);
    etOffset(%F8, %F9);              # ← 发射口偏移
    etEx(0, 131072, 0, 120, -1, -1.0f, -1.0f);
    etEx(1, 262144, 0, -1, -1, -1.0f, -1.0f);
    etRing(3, $I11, 1, 1, 0.0f, 1.0f, %RANDRAD, 0.09817477f, 393732);
    ins_5(0, Sub14_52, $I4);
    delete();
}
```

⇒ TH08 的形状是 **「使魔（会动的实体，带 anm 动画）→ 它撒出的子实体扛 `et*` 发射口」**。
108 卡把「发射口 → 使魔」的映射反过来做（`props[i].type != 0` ⇒ 在发射点建一只使魔），
方向不同但**几何上等价**：使魔总是钉在发射口上。**这一条对我们的修法很关键**：
TH08 里「使魔」与「发射口」是一体的，所以使魔被清掉 = 发射口消失，不存在「使魔没了但发射口还在」的夹死态。

## 4. ★ 使魔的动画（owner 说我们缺的那一块）

### 4.1 外观由**使魔自己的 ECL 子程序** `anmSet(K)` 决定

`anmSet` = opcode **54**，handler `0x419812`：取 1 个参数 K，调
`AnmLoaded::load_script_into_vm(this = `th08:0xf54e0c`, vm = &enemy+0xc primary_vm, script = K)` `0x4069f0`
⇒ **把第 K 号 anm 脚本装进这只敌人的本体 VM**。

把 24 个 ECL 里**每个 `familiarCreate*(...)` 的子程序**追到它自己的 `anmSet(K)`（脚本复算，按调用点加权）：

| K | 调用点数 | 该脚本是什么 |
| --- | --- | --- |
| **53** | 347 | 8 帧循环，sprite 69..76 |
| **54** | 162 | 8 帧循环，sprite 77..84 |
| **55** | 22 | 8 帧循环，sprite 85..92 |
| **56** | 48 | 8 帧循环，sprite 93..100 |
| **57** | 165 | 4 帧，sprite 101..104（脚本里有 anm v3 专有 `ins_25`）|
| **58** | 46 | 4 帧，sprite 105..108（同上）|
| 61 / 63 / 64 | 10 / 2 / 4 | ❓ 少数派：可能是「子程序的第一个 `anmSet` 与后面换装混在一起」的统计噪声，未逐条核实 |

⇒ **使魔长相不是按「槽位」分的，是按「生成它的那支子程序」分的**（同一波里不同 `familiarCreate("SubN")` 可以不同 K）。
⚠ 108 卡当前的做法是「使魔外观按**发射口槽** `slot & 3` 选一套（`ability.anm` 206..209 = TH08 script53..56）」——
**这是本卡的自定映射，不是复刻**（TH08 没有「槽 → 外观」这条对应）。

### 4.2 `0xf54e0c` 是哪个 anm 文件

`0x419850 mov ecx,[0xf54e0c]` 就是 `AnmLoaded*`。上界判据：`anmSet(K)` 的 K 最大到 **95**，
TH08 全 317 条 entry 里脚本数 ≥96 的只有 `etama.anm`(115) 与 `enemy.anm`(101)，
而各关 `stgNenm.anm` 只有 6–26 ⇒ 只能是这两者之一；`ins_54` 写的是**敌机本体 VM**、
`EnemyManager` 的 `+0x9dceec = …prolly_enemy_anm`（装 `enemy.anm`）⇒ 判为 **`enemy.anm`**（贴图 `data/stgenm/enemy.png`）。
🟡 残余：`etama.anm` 也满足上界，靠「本体 VM ⇒ 敌机图集」排除；未找到 `0xf54e0c` 的写入点直接坐实。

### 4.3 已移植的资产（TH18 侧）

- `ability.anm` script **206..209** = TH08 `enemy.anm` script 53..56（8 帧循环，逐条照抄，只把 sprite 换成 ORDER 名字），
  贴图 = `enemy.anm` sprite 69..100 的原生切图（32×32），生成器
  `mods/th18.v1.00a/card-expand/assets/ability/make_youkai_familiar_art.py`。
- **57/58 没移植**：它们的正文有 anm v3 专有指令 `ins_25`（写 `zAnmVm+0x1fc`，v8 anm 无对应助记符）⇒ 按纪律不移植。
  代价：TH08 里用得最多的两组（57 有 165 个调用点）我们没有对应外观。
- 卡侧播放方式 = **跟随 VM**（`youkai.c` 的 `YK_FAM_ANM_LAYER` 一节，世界层 7）——
  因为 `NoEnemyDebug` 惰性空壳不跑 ECL，没人给它装脚本；而 **ECL 的 anm 索引表里没有 `ability.anm`**
  （一手 `th18:0x438d40`：`N==0 bullet.anm` / `N==1 effect.anm` / `N>=2` = 本关 ECL 头 `anim{}` 的第 N−2 条；
  见 `mods/th18.v1.00a/card-expand/assets/ecl/ufo.ecl.txt` 文件头 Δ2）。

## 5. 对 108 移植的结论（只列结论，方案见 AUDIT §BE-EY）

| 问题 | 一手答案 |
| --- | --- |
| 是敌机还是独立对象？| **真敌人**（`EnemyManager::create_enemy`），另加 `flags_hi` 的「使魔」旗标 + `familiar_circle_effect` |
| 每波怎么重建？| **脚本循环生成**（`familiarCreate` 在波循环里）+ 段首 `enmKillAll()` 把上一波清光 |
| 生命周期字段？| 生成时给 `life`(`+0x2dfc`) / `drop_main`(`+0x3304`) / `score_reward`(`+0x2e08`)；`+0x3380` 计数 +1；自删靠自己的 `delete()` 或 `enmKillAll` |
| 动画是哪份 anm / 哪些 sprite？| `enemy.anm` script **53..58**（6 支）：53..56 = 8 帧 @2 帧间隔（sprite 69..76 / 77..84 / 85..92 / 93..100），57/58 = 4 帧（101..104 / 105..108）|
| 使魔动起来靠什么？| 它自己的 ECL：`moveDir(%F0,%F1)` + `anmRotSet` + `trailFamiliarSet(1)`；发射口在它撒出的子实体的 ECL 里（`etOffset`/`etEx`/`etRing`）|

**还未查的（写清缺口）**：① `flags_hi` 另两位（`anmFamiliar` 写的那一位）的确切位号；
② `0xf54e0c` 的写入点（把「= `enemy.anm`」从 🟡 升 ✅）；③ 61/63/64 那几个 K 是不是统计噪声；
④ `familiar_circle_effect` 的圆环特效是哪份 anm（TH08 特有，TH18 无对应 ⇒ 要复刻得自己找贴图/脚本）。
