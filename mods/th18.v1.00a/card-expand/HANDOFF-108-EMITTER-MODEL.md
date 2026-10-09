# 108 · TH08 发射口模型 ⇄ TH18 槽 · 「打一枪换一个地方」怎么映射

> **版本**：跨版本。本文出现的地址一律带版本前缀（如 `th08:0x41b4dc`、`th18:0x427810`）。
> **方法**：一手 —— ① PE 直读自解两个 exe 的 ECL 分派表，钉 opcode↔handler（TH08 表基址 `th08:0x41ecbe`、index=opcode−1；
> TH18 `th18:0x4366f4` 组号表 + `th18:0x436438` 组跳表，index=opcode−300）；② `objdump -d -M intel` 直接读 PE 逐指令；
> ③ 原生 ECL 反编译文本 `local/_scratch/th08f/ecldata*.txt` 与 `local/_scratch/st0*.asm`（thecl 产物）；
> ④ 卡侧代码**只读**（`youkai.c` / `youkai_core.c`）。
> **未做**：未改任何 `.c/.h`、未跑 make、未 release、未实跑。
> **可信度**：TH08 的模板/偏移/开关语义、TH18 的原点字段与站点寄存器语义 ✅（逐指令）；
> `etCount` 的 `[-1]`、TH08 偏移第三格的读点 🟡（见 §5）。

## 0. 最短结论

1. **TH08 的发射口 = 敌人自己身上的一支「弹模板」**（`th08` 侧 `zEnemy+0x2e24`），**每个敌人只有一支，没有槽号、没有实体**；
   原点在**每次发弹时现算**：`原点 = enemy 当前位置(+0x2d88) + etOffset(+0x2db8)`（`th08:0x4227f8`→`th08:0x422827`，变换函数 `th08:0x409080`）。
2. **使魔与发射口无关**：使魔是 ECL `familiarCreate*`（op90/91/92）建的**独立敌人**，弹从**它自己**的模板出；
   原作**从不**把发射口翻译成实体（`th08:0x41b883` 这类 `et*` handler 里没有 `create_enemy`）。
3. **TH18 的「打一枪换一个地方」不是换槽，是同一个槽的 `etOffset/etOffsetRad` 每波重掷**（一手见 §4.1）。
   TH08 里对应的**不是**「多只使魔轮流」，而是 **`etOffset(rand,rand)` 改那一支模板的偏移**（或本体 `moveRandTime` 飞过去）——
   **没有任何实体随之生成/销毁/瞬移**。
4. **最强可行路径**：**不新增站点** —— 在**已有的站点 `th18:0x433ffa`** 把 `props[i].offset_final`（`+0x08/+0x0c/+0x10`）覆写成壳的世界坐标
   （§3a），「壳即发射口」当场成立；再在卡侧把壳的位置策略从「每帧硬钉」改成「向该槽当前原点**插值滑行**」（§4.4 的 D）。

## 1. TH08 的发射口模型（一手，逐指令）

### 1.1 谁持有 —— 一个敌人一支模板

| 项 | 值 | 一手 |
| --- | --- | --- |
| 模板 | `zEnemy+0x2e24`（一支；`origin` 在 `+0x04`、`count_1/+0x1f4`、`count_2/+0x1f6`、`ex[]/+0x20`）| `th08:0x422735` |
| 子行为表 | `etEx(exId, …)` 写 `模板+0x20+exId*0x18`（6 dword）| `th08:0x41b55f` / `th08:0x41b565` |
| 唯一偏移 | `etOffset(x,y)` 写 `+0x2db8`(x) / `+0x2dbc`(y) / `+0x2dc0`=0 | `th08:0x41b8f1` handler |
| 原点现算 | `0x409080(enemy+0x2d88, &tmp, enemy+0x2db8)` → 抄进 `模板+0x04` | `th08:0x4227f8`–`th08:0x422827` |
| 发射 | `push <模板> / mov ecx,0xf54e90 / call 0x430e10` | `th08:0x422b64`–`th08:0x422b6d` |

⇒ **没有「槽」这个概念**：`etEx` 的 `id` 索引的是**同一支模板内部的子行为表**，不是另一门炮。

### 1.2 谁发弹 —— 「立即发」与「暂存/连发」两条路

| 写入 | 一手 | 效果 |
| --- | --- | --- |
| `et*` 家族 op96–104（共用 handler） | `th08:0x41b4dc` | 见下两条 |
| ⟶ `flags`(`+0x3324`) bit17 **清** | `th08:0x41b519` → `call 0x422720` | **当场发** |
| ⟶ bit17 **置** | `th08:0x41b501`–`th08:0x41b514` | 11 dword 参数块抄进 `enemy+0x3034`，**不发** |
| `etDelay()` op107 / `etOn()` op108 | `th08:0x41b866` / `th08:0x41b883` | `or/and 0x20000`（位置/清位）|
| `etOnAuto(n)` op105 | `th08:0x41b70f`（写 `+0x3060`；`n==0` 跳过建计时器）| 自动连发周期 |
| 连发执行 | `th08:0x423150`（每帧，thiscall `ecx=enemy`）| 到点 ⇒ `edx=enemy+0x3034` → `call 0x422720` |

**作用范围**：

- `etOnAuto(0)` = **跨帧**（把周期写 0；直到下一次非 0 的 `etOnAuto`）。原作**每个攻击段段首**都是
  `etOnAuto(0); enmKillAll(); etSound(-1,-1); flagSet(4); etOffset(0.0f, 0.0f);` 这一组「收摊」。
- `etDelay()` / `etOn()` = **跨指令、跨帧**（位是持久的）：`etDelay` 之后每条 `et*` 只暂存（覆盖同一块 `enemy+0x3034`），
  `etOn` 之后恢复当场发。**一波之内**用它做「先摆姿势后放」。
- 位17 全 exe **只有两处读者**（`th08:0x41b4f6`、`th08:0x422753`），都在同一条 `et*`→发射路径上。

### 1.3 逐指令样本（实参抄全）

**(1) 5 面道中 `Sub2`（本体）—— 全程 0 条 `et*`**，只撒使魔（`local/_scratch/th08f/ecldata5.txt`）：

```
sub Sub2()
{
    anmSet(83); anmSetSlot(0, 48); hitboxSet(48.0f, 48.0f); setInvuln(200);
    flagSet(2); dropExtra(5, 4); moveDirTime(60, 4, 1.5707964f, 3.0f);
    ... 按 %SELF_X 选 %F1（角速度）、%F0（初始角）...
    familiarCreateF("Sub3", 0.0f, 0.0f, 300, -2, 100);   // ×3（%F0 = -π / -π/3 / +π/3）
    familiarCreateF("Sub5", 0.0f, 0.0f, 300, -2, 100);   // ×5
    familiarCreateF("Sub7", 0.0f, 0.0f, 300, -2, 100);   // ×5  ⇒ 共 13 只
    +60:  moveDir(0.3926991f, 0.0f); ...
    +5000: delete();
}                                                        // ★ 本体没有任何 et*
```

**(2) 使魔 `Sub5`（`familiarCreateF` 建出来的独立敌人）**：

```
sub Sub5()
{
    anmSet(54); hitboxSet(24.0f, 24.0f); flagSet(16); flagClear(3);
    trailFamiliarSet(1);
    moveCircle(20, %F0, %F1, 3.0f);
+20: moveCircleChange(6000, %F1, 0.0f);
     callAsync(0, "Sub6");          // ★ 发射口在它**自己的 async 子程序**里
+5000: delete();
}
```

**(3) 真正扛发射口的 `Sub6`（在使魔自己的 async 子程序里）**：

```
sub Sub6()
{
!E iset($I0, 26);  !N iset($I0, 13);  !H iset($I0, 11);  !L iset($I0, 4);
Sub6_80:
!*  fset_add(%F0, %SELF_ANGLE, 3.1415927f);
    etFan(1, 6, 1, 2, 2.2f, 1.2f, %SELF_ANGLE, 0.0f, 514);
    jmp_leq($I0, 16, 0, Sub6_192);
    dec($I0);
Sub6_192:
    wait($I0);
    jmp(0, Sub6_80);
}
```

实参归位（一手 `th08:0x422720`）：`etFan(type=1, count_1=6, count_2=1, speed_1=2.2f, speed_2=1.2f,
angle_1=%SELF_ANGLE, angle_2=0.0f, flags=514)`——**从使魔自己的位置、按使魔自己的朝向出**。

**(4) ⚠ 一手更正（文献里那句 `Sub3 → callAsync(Sub4)` 只在 Easy 成立）**：

```
sub Sub3()
{
    ... moveCircle(20, %F0, %F1, 1.5f); +20: moveCircleChange(6000, %F1, 0.0f);
!NHL    callAsync(0, "Sub4");          // ← `!NHL` = 只有 E 才跑
    +5000: !* delete();
}
sub Sub4()
{
    etEx(0, 64, 0, 50, 1, 1.5707964f, 2.5f);
    iset($I0, 8);
Sub4_60:
    fset_sub(%F0, %SELF_ANGLE, 3.1415927f);
    etFan(2, 2, 1, 1, 1.5f, 0.8f, %F0, 0.0f, 578);
    wait($I0);
    jmp(0, Sub4_60);
}
```

⇒ N/H/L 上这三只是**纯靶子**（不配发射口）；扛口的是 `Sub5→Sub6`、`Sub7→Sub8`。

**(5) 6 面道中 `Sub0`（15 只）**：`iset($I4,5)` 循环 ×3 条
`familiarCreateF("Sub1"/"Sub3"/"Sub5", 0.0f, 0.0f, 600, -2, 100)`；它们的 async 子程序里才是
`Sub2: etEx(0, 64, 0, 50, 1, 1.5707964f, %F0)` + 循环 `etRing(2, 2, $I1, 1, %F0, 0.8f, %F0, 0.0f, 576)`
（`$I1` = 2/4/6/8 按难度）、`Sub4: etFan(1, 6, 4, 1, 1.4f, 1.2f, %SELF_ANGLE, 0.3926991f, 512)`。

### 1.4 「偏移随机化」的原作写法（= TH18 那两案例的直接对应，一手）

`ecldata8.txt` 的 `Sub14`（一个撒弹的中型敌，**一只使魔都不建**）：

```
Sub14_52:
    fset_mul(%F8, %RANDF2, 32.0f);
    fset_mul(%F9, %RANDF2, 32.0f);
    etOffset(%F8, %F9);                       // ★ 唯一那支偏移：每轮重掷（半径 32）
    etEx(0, 131072, 0, 120, -1, -1.0f, -1.0f);
    etEx(1, 262144, 0, -1, -1, -1.0f, -1.0f);
    etRing(3, $I11, 1, 1, 0.0f, 1.0f, %RANDRAD, 0.09817477f, 393732);
    ins_5(0, Sub14_52, $I16);
```

⇒ 与原作一致：**一个敌人、一支模板、偏移每轮重掷**——实体不生成、不销毁、不瞬移。

## 2. TH08 ⇄ TH18 对照表

| 这件事 | TH08 | TH18 |
| --- | --- | --- |
| 发射口的载体 | 敌人自己的模板 `+0x2e24`（**唯一**）| `props[0x10]` 数组 `enemy+0x17c4+i*0x488`（**16** 个）|
| 一次攻击用几个口 | 1 | 1…16 |
| 出弹原点 | 每次发弹现算 = `enemy 位置 + etOffset` | `etOn` 现算写进 `props[i]+0x08..+0x10`，真发只读它 |
| 偏移数组 | 单支 `+0x2db8/+0x2dbc/+0x2dc0` | 每槽 `enemy+0x4e58+i*0x0c`（+ `+0x4f18` 绝对模式）|
| 子行为表 | `etEx(exId, …)` → `模板+0x20+exId*0x18`（op111）| `etEx(slot, exId, …)` → `props[slot]+0x28+exId*0x2c`（op609/611）|
| 发弹 | `etFan/etRing/…`(op96–104) 立即 `call 0x422720` | `etOn(slot)`(op601) → `call 0x427810(&props[slot])` |
| 「关掉」 | `etDelay()`(位17) + `etOnAuto(0)`：**持久、跨帧** | **无原语**；只有站点 `th18:0x433ffa` 的 **per-call** 否决 |
| 「一窝 13 台、每台每 12 帧一组弹」| `familiarCreateF` ×13（13 个敌人）→ 各自 async 里 `etFan` + `wait` | **没有这个形状**：同一敌人 16 槽；要 N 个「点」只能用 N 个槽或改 offset |
| 「打一枪换一个地方」| 改 `etOffset`（§1.4）或本体 `moveRandTime`；**实体不变** | 同槽 `etOffsetRad` 每波重掷；**槽号不变**（§4.1）|

## 3. 把映射做强的三条路（给地址与代价，**不实现**）

### (a) 拦截开火：把这一发的原点换成壳的位置 ★推荐

一手：`etOn`(`th18:0x433f34`) 在 `th18:0x433f8a` / `th18:0x433fec`–`th18:0x433ff4` 把算好的原点写进
`props[i]+0x08/+0x0c/+0x10`；已有站点 `th18:0x433ffa` 在这之后、`call 0x427810` 之前；真发 `th18:0x427810`
只读这三格（瞄准 `th18:0x427841`/`th18:0x427846`；生成子弹 `th18:0x4251ea` `movq xmm0,[esi+0x8]` + `th18:0x4251f3`）。

站点处寄存器（一手）：`edi = &zEnemyData`、`esi = &data + i*0x488` ⇒ `props = esi + 0x598`。写 3 个 float、返回 0（照发）。

代价：**0 新站点**、3 次写。风险：站点极热（每槽每帧被问）；只在「该槽有我们的壳」时写；写进去的值会留到下一次 `etOn`（无害）。
附带好处：瞄准也跟着变成「从壳瞄自机」——与 TH08「弹从使魔自己出、朝自机」一致。

### (b) 让壳自己成为发射口：从 C 侧调引擎的发弹入口

一手：`th18:0x427810` 是 **`__stdcall`、1 个栈参 `zEnemyBulletShooter*`、`ret 4`、恒返回 0**；整支函数只碰
`props`、`[0x4cf410]`（vendor 名 `PLAYER_PTR`，只用于瞄准）、`[0x4cf2bc]`（`BULLET_MANAGER_PTR`，当 `th18:0x424fe0` 的 this）。
**不读任何 owner enemy** ⇒ 自备一块 0x488 的 props 就能从任意位置发弹。

前置：① 在游戏内（两个全局非空）；② 必须在游戏线程的帧内调（它 append 子弹链表）；③ 想保留 `et_protect_range`
语义要自己 `call th18:0x42cca0`（该函数 = `BULLET_MANAGER_PTR->+0x44 = xmm1`）置/复位；④ 单发入口 `th18:0x424fe0`
是 **thiscall**（`ecx=BULLET_MANAGER_PTR`，栈参 `(props, i, j, angle)`）。模板默认值抄 `etNew`（`th18:0x433dde`）。

代价：卡侧要维护模板 + 自己算子行为表；风险：**不建议**伪造 `edi` 去 `call th18:0x433f34`——参数取值要
`[[edi+0x5588]+0xc]` + `th18:0x48d4f0`（见 `th18:0x438aa0`），C 侧没有这套上下文。

### (c) 槽→实体绑定：用可观测边沿做寿命

现状 `YK_EMIT_WAVE_GAP = 24`（❓自定）拿「两次 `etOn` 间隔 ≥24 帧」当「新一波」。**一手弱点**：owner 的两个案例
`etOn` 间隔是 **2 帧（st6）/ 5 帧（st4）** ⇒ 永远 < 24 ⇒ 这条边沿**永不触发**（它本来也只服务「闸的放行」）。

改进点（**不新增站点**）：真正的「换地方」事件 = **该槽的偏移向量变了**（`enemy+0x4e58+i*0x0c`，卡侧**已经在读** =
`YK_EMIT_OFFSETS`），比 `etOn` 间隔稳，且与「打一枪换一个地方」一一对应。⇒ 用它当重定位事件，`etOn` 间隔只管闸。

## 4. ★ 追加案例：st6 开头小怪 / st4 道中 boss「打一枪换一个地方」

### 4.1 一手：TH18 侧 = 「固定槽 + 每波重掷 offset」

**st6 开头小怪 `GirlA01_at`**（`local/_scratch/st06.asm:397`）：

```
etNew(0); etAim(0, 0); etSprite(0, 5, 4); etCount(0, [-1], 1);
etAngle(0, 0.0f, 0.2268928f); etSpeed(0, 0.5f, 1.0f); ... etSound(0, 21, -1);
$A = 10;
GirlA01_at_516:  $B = 10;
GirlA01_at_580:
    etOffsetRad(0, %RANDRAD, %RANDF2 * _f(32));     // ★ 同一个槽 0，半径 ≤32 的随机点
    etAngle(0, %RANDF2 * 0.034906585f, 1.0471976f);
    etOn(0);
    wait(2);                                        // ← 每 2 帧换一个地方
GirlA01_at_788: if ($B--) goto GirlA01_at_580;
    wait(60/120);                                   // 波间
GirlA01_at_932: if ($A--) goto GirlA01_at_516;      // 10 波
```

**st4 道中 boss `MBoss1_at`**（`local/_scratch/st04mbs.asm:63`）：`etNew(0)` 一次；一段
`etOffsetRad(0, %RANDRAD, 64.0f); etAngle(0, 1.178+…); etOn(0); wait(5)`；
二段 `etOffsetRad(0, %RANDRAD, 64.0f); … etOn(0); wait($H)`。槽 1（`MBoss1_at2`）`etNew(1)` 一次后**只** `etOn(1)`
每 100 帧，**偏移 = 0 ⇒ 就在本体上**。

⇒ **「随机地在用发射口」= 同一个槽的 `offset` 每波重掷**，**槽号自始至终不变**。原生 ECL 里也没有槽号轮换：
可变槽号（`etNew($A)`）都是**子程序形参**，调用方给常量（一手 `local/_scratch/st02bs.asm:497/544` 的
`BossCard1_at(0, …)` / `(1, …)` 两条 async）。

### 4.2 一手：TH08 侧没有这个「实体问题」

TH08 一个敌人只有一支模板（§1.1）⇒ **没有「换发射口」这回事**。随机化只有两种写法：

1. `etOffset(rand,rand)` —— §1.4，**不动实体**；
2. 本体 `moveRandTime(60, 4, …)` 飞过去 —— 一手 `ecldata6.txt` 的 `Sub52`（boss）：每 180 帧
   `moveRandTime(60, 4, 1.0f)`，期间 `callAsync(Sub53)` 持续 `etFanAimed(10, 0, 10, 1, 1.0f, 1.0f, 3.1415927f, 0.3926991f, 514)`。

**Q1 答**：这两个案例在 TH08 里**既不是**「一只会移动的使魔」，**也不是**「多个发射口/多只使魔」——
**TH08 的发射口不长在实体上**；「随机换位置」改的是**同一支模板的偏移**（或本体滑行）。
使魔是独立生物，只在 `familiarCreate*` 处产生。
⇒ 推论：**`1 发射口 = 1 壳` 是我们的 ❓自定构造**（`engine/enemy/th08/01-familiars-and-anm.md` §3.3 已注明方向不同）；
TH08 没有「实体跟着发射口瞬移」的语义可抄。

### 4.3 现状一手（代码只读）

| 环节 | 位置 | 行为 |
| --- | --- | --- |
| 采样 | `youkai.c:2399`–`2405` | 每帧对每个 active 槽算 `off3/org3 → wxy`，`yk_note_slot` 存进 `sl->x/sl->y` |
| 钉位 | `youkai.c:2548`（`yk_transfer_tick`，每帧）| `yk_pin_familiar(f, sl->x, sl->y)`：**壳每帧硬钉到该槽当前原点** |
| 动作 | `youkai_core.h:1084` | 只有 `YK_ACT_CREATE` / `YK_ACT_WITHDRAW`——**没有 MOVE / 插值** |
| 本体口 | `youkai.c:2421` `yk_emit_on_body()` | 偏移 (0,0) 的槽整格跳过（不建壳）——st4 的槽 1 走这条 |

⇒ 效果 = 槽一改 `offset`，壳**瞬移**：st6 每 **2 帧**跳一次（半径 ≤32 px）、st4 每 **5 帧**跳一次（半径 64 px）。
这就是 owner 看到的「打一枪换一个地方」。

### 4.4 三条映射策略评估（供拍板，**不实现**）

| 方案 | 判定依据 | 观感预期 | 代价与风险 |
| --- | --- | --- | --- |
| **A 命中式重定位**（跨槽抢壳）| 需要「槽号轮换」存在，一手 ECL 里**不成立**（§4.1）| 壳数稳，**但不解决瞬移**| 每敌人维护壳池 + 「最近」匹配；同帧两槽同时开火会互抢/换位（两只叠一起）；本二例零收益 |
| **B 现状**（每槽一壳、每帧硬钉）| `props[i].type != 0`（`youkai.c:565`）+ 跳过本体口 | **瞬移抽搐**（2/5 帧一次）| 0 成本；风险 = 观感（owner 已报）|
| **C 混合**（同帧多槽⇒多壳；大步跳变⇒重定位）| 需要「跳变阈值」判据 | 比 B 好、比 A 简单 | 多一个阈值 + 计时器；跳变那一下仍有一次瞬移 |

**推荐（新增 D = B 的加强版，不新增站点）——把「壳的位置策略」与「壳的身份」分开**：

- **判据**：该槽的偏移向量 `off3`（`enemy+0x4e58+i*0x0c`）帧间**变化** = 换地方事件（比 `etOn` 间隔准，§3c）。
- **D1（最贴原作，默认）**：壳**锚在母机上**（`epos`），偏移只决定**弹**的出点。理由：TH08 的使魔本来就绕着母机飞，
  根本不是「发射口的化身」。观感：壳不再抽搐，弹仍从随机点出。代价：壳与弹的「出处」不一致（TH08 本来就不一致）。
- **D2**：壳跟着原点但**插值滑行**（≤N 帧，量级照 `moveRandTime(60,4,·)` 的 60 帧）：把瞬移变成「飞过去」。
  代价：一个（当前点/目标点/速度）状态机；弹的瞬时出点仍会比壳快一步。
- **D3**：`|Δ| > 阈值 D` 才按「搬家」处理 = 旧壳淡出 + 新位置建壳（= 只在同槽内做 C 的判据）。

建议组合：**D1 为默认，D2 作为可选**（owner 若要「壳就站在出弹处」则 D2 + 长滑行）。`YK_EMIT_WAVE_GAP` 在这两条
案例里都不触发 —— 保持只服务闸。

## 5. 🟡 不确定项

| # | 未定 | 现状 | 还差哪一段 |
| --- | --- | --- | --- |
| 1 | `etCount(slot, [-1], 1)` 的 `[-1]` | 原始字节就是 `0xFFFFFFFF`；`th18:0x427810` 按 i16 读 ⇒ 按 `th18:0x4278b4` 的 `cmp ax,[edi+0x46c]/jge` 内层 **0 发**；但 22 份原生 ECL 里 49 处把它当常规值用 | 读 `th18:0x48d4f0`（参数取值：`th18:0x438aa0` → `[[edi+0x5588]+0xc]` → 它）或实跑断点 |
| 2 | TH08 偏移第三格 `+0x2dc0` | handler 里写 0（一手字节 `c7 80 c0 2d …`），**没找到读点** | 读完 `th08:0x409080` |
| 3 | 「六面开头 / 四面道中」在 TH08 里的**同型敌人** | 本文给的是机制 + 两条一手写法；**没有**把 TH18 那两个敌人一一对上号 | 需 owner 认同型口径（本文用的是 `GirlA01` / `MBoss1`）|
| 4 | 实跑现场数据 | 本机日志 `th18_card_expand.log` 里 `youkai:` 行数 = **0**（最近一次启动没装 108/没进关卡）| 装 108 跑一次，取 `emitkey` / `emitter gate` 行 |

## 6. 相关

- 使魔是什么：[`01-familiars-and-anm.md`](../../../engine/enemy/th08/01-familiars-and-anm.md)
- 使魔计数： [`03-familiar-count-vs-hp.md`](../../../engine/enemy/th08/03-familiar-count-vs-hp.md)
- TH18 槽与波次：[`07-emitter-slots-waves-and-phase-transitions.md`](../../../engine/enemy/th18/07-emitter-slots-waves-and-phase-transitions.md)
- 关槽调研（scratch）：`local/_scratch/c108/RESEARCH-emitter-disable.md`
- 卡侧共享件改动登记：[`AUDIT.md`](AUDIT.md) · 数值口径：[`TUNING.md`](TUNING.md)
