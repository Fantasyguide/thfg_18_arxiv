# 对战循环 — TH09《東方花映塚》的花/进度、送攻击、霊与胜负

> **版本**：TH09 v1.50a（`th09.exe`，685,056 B，imagebase `0x400000`，md5 `cf634df46e05552e104fa97a971aaac0`）。本文裸地址默认属该版本；引用其他版本须写成 `th19:0x…`。

> **一手面**：`local/th09.v1.50a/decomp/all.c`（2002 函数）+ `tooling/ghidra/th20_show.py`（读语料，不占 Ghidra 锁）+ `tooling/pe_read.py`（常量真值）+
> 指令级复核（python 直读 PE 节表后按 VA 取字节，本文所有「指令级」标注都是这么来的）。
> **只借名字**：`local/vendor/th-re-data/data/th09.v1.50a/`（ExpHP）。**本文每条结论都落到自己的反编译/指令阅读上**；与社区名冲突处已显式标出（§1.4）。
> **相关**：[`../../bullet/th09/01-cancel-to-item.md`](../../bullet/th09/01-cancel-to-item.md)（消弹不出道具；本文 §3 补上它 N28「吸霊到底动什么」的答案）·
> [`../../../games/th09.v1.50a/INDEX.md`](../../../games/th09.v1.50a/INDEX.md)（登记）。

---

## 0. 先纠正一件事：「花」不是代码里的机制

| 任务里的词 | 代码里实际是什么 | 一手 |
| --- | --- | --- |
| 「花」 | **不存在**。全 exe 的 SJIS 串里「花」只出现在标题与菜单项「花映塚モードを開始します」`0x8eb4c` | ✅ 全串扫过 |
| 「进度」 | 三个全局：`0x4a7e44`（等级）/ `0x4a7e90`（关·局）/ `0x4a7eac`（难度）（§2.2），与「花」无关 | ✅ `0x41aa5f` |
| 「送 Boss 攻击」 | **effect 载体对象**飞过去 → 写对手的 `EnemyManager` / `BulletManager`（§2.3） | ✅ `0x4102b0` `0x40d9e0` |
| 「霊」 | `zPlayer+0x30454` 的 **4 槽数组**（§3），不是道具系统 | ✅ `0x41db90` |

**发现**：`花映塚モード` 只是 **Story 模式**的菜单文案；本作对战资源轴的代码名是 **チャージ（charge）** 与 **ExAttack**。
**验证**：`se_charge00/charge01b/chargeup/exattack.wav` 在包内（`thdat -l 9`）；设置项串「チャージボタンの動作、Slow:低速移動、Charge:チャージ」`0x8e9e4`。
**结论**：✅（v1.50a）本作**没有名为「花」的字段/表/图形**。写卡时「花/进度」应映射到 §2 的充能轴，**不要**在引擎里找 flower 对象。
—— 119 工单里「花」这个词是从**作品题材**推来的，不是从代码推来的。

---

## 1. 对战骨架

### 1.1 两侧 = 两个 player 槽（不是「自己 + 对手」的一主一从）

**发现**：`GAME_MANAGER`（`0x4a7d90`）里排着 `side_1`(+0x0) 与 `side_2`(+0x38)，**每个 side 自带一整套 7 件世界对象**。
**验证**：`Supervisor::thread_start`（`0x41af2d`）的建场循环 —— 以 `0x4a7dc4`（= GM+0x34）为起点、步长 `0x38`、终止 `< 0x4a7e34`（= GM+0xa4），**正好 2 次**，
每次把 7 个 `*__operator_new(side)` 写进 side 的 7 个槽；`side` 序号 = 循环变量（0/1）。
**结论**：✅（v1.50a）**两个对称 player 槽**，没有任何「主场/客场」的非对称字段。

| side 槽 | 偏移 | 类 | 建场调用（`0x41af2d`） | tick 优先级 |
| --- | --- | --- | --- | --- |
| `stage` | +0x0 | `Stage` | `Stage__operator_new(side)` `0x403ae0` | `side+8` |
| `player` | +0x4 | `Player` | `Player__operator_new(0,side,…)` `0x41f170` | `side+0xf` |
| `bullet_manager` | +0x8 | `BulletManager` | `BulletManager__operator_new(side)` `0x4150d0` | `side+0xc` |
| `effect_sys` | +0xc | `EffectSys` | `EffectSys__operator_new(side,0x100,7)` `0x40d320` | `side+0x13` |
| `enemy_manager` | +0x10 | `EnemyManager` | `EnemyManager__operator_new(side)` `0x412240` | `side+0x10` |
| `spellcard` | +0x14 | `Spellcard` | `Spellcard__operator_new(side)` `0x404600` | `side+0x11` |
| `gui_side` | +0x18 | `GuiSide` | `GuiSide__operator_new(side)` `0x41a5d0` | `side+0x17` |

> 优先级 = `register__on_tick` 的第 3 参（在各自的 `*__operator_new` 里；`param_1`/`param_2` 就是 side）。
> 每侧顺序：`Stage` → `EnemyManager` → `BulletManager` → `Player` → `Spellcard` → `EffectSys` → `GuiSide`。
> 全局另有 `Gui`(`0x4a7e38`，pri `0x16`)、共享 `EffectSys`(`0x4a7e0c`，side=2)、`ExAttackManager`(`0x4a7e3c`，pri `0xe`)、`GameManager`(pri `2`)。

### 1.2 每帧入口

| 层 | 地址 / 名字 | 说明 |
| --- | --- | --- |
| 注册表 | `register__on_tick` `0x42b1e0` / `UpdateFuncRegistry::run_all_on_tick` `0x42c700` | 按优先级串行跑；`run_all_on_draw` `0x42c7e0` |
| 全局帧 | `GameManager::on_tick` `0x41aa5f`（注册在 `GameManager::initialize` `0x41b9d0`，pri 2） | 帧计数、等级升级、超时、扩展残机 |
| 各对象 | §1.1 表 | 自注册，无需中心派发 |
| 局内帧数 | `GM+0x140` | `6000` 帧警告（`ScreenEffect__operator_new` + `Supervisor::sub_42fe20`），`0x17e8` 帧 → `return 3` 🟡 |
| 「有对话框在跑」门 | `FUN_00404980(GUI_PTR)` `0x404980` | 返回 0 = 空闲；攻击/自机逻辑都拿它当闸（读 `GUI+0xe94c`） |

### 1.3 胜负判定

**发现**：自机被弹到低血量会走一条「本局判负」的函数，它把**对手**判为胜者。
**验证**（指令级/逐行）：
- `FUN_0041e420`（`0x41e420`）＝ **自机被打死**：`player+0xa8 -= DAT_004a7e48`（+`_DAT_004a7e4c` 当 `player+0x20 != 0`）；`< 2` 则 `param_1[0x2a] = 0` 并调 `FUN_00415d70(GUI_PTR, 1 - param_1[2])`。
  `param_1[2]` = `zPlayer+8` = `zSideData.side_num` ⇒ `1 - side` = **对手**。
- `FUN_00415d70`（`0x415d70`）＝ **某侧赢一局**：`DAT_004a7e90++`；`(&DAT_004a7e98)[side]++`；若 `DAT_004a7e94 <= 胜局数` 则摆胜利用 sprite（`0x3c/0x3d/0x3e`）并把 `GUI+0x11ea8 = 1`（进结算相位）。
- 胜局阈值 `DAT_004a7e94`：`Supervisor::thread_start` 里 `= (mode == 2) + 1` ⇒ **Match 2 胜、Story/Extra 1 胜**。
- `FUN_0040d4f0(GUI_PTR)`（`0x40d4f0`，读 `GUI+0x11ea8`）是「已分胜负」闸，攻击生成函数开头都 `if (!=0) return;`。
**结论**：✅（v1.50a）胜负 = 「一局内血量耗光 → 对手 +1 胜 → 达阈值出结算」；**没有"两条命/残机作为败北条件"**（`player+0xa8` 就是局内血量，见 §4.4）。
❓ 超时（`GM+0x140 == 0x17e8` → `return 3`）由谁判胜未读；`UpdateFuncRegistry` 的返回码语义（3 / 4）未读。

### 1.4 `zSideData` 的第 3 个字段：社区名是「偏移」，一手是**对手指针**

**发现**：ExpHP 把 `zSideData+8` 名为 `__weird_offset_from_side_ptr_to_side_2`（`0x38`），听起来是个**常量**。
**验证**：`Spellcard__operator_new`（`0x404600`）里
`piVar3[1] = &GAME_MANAGER + param_1*0x38;`（＝**本侧** side 指针）与
`piVar3[2] = &DAT_004a7dc8 + param_1*-0x38;`（＝param_1=0 → `0x4a7dc8`＝side_2；param_1=1 → `0x4a7d90`＝side_1 ⇒ **对手侧指针**）。
**结论**：✅（v1.50a）`zSideData = {+0 side_num, +4 本侧 zGameManagerSide*, +8 对手 zGameManagerSide*}`。
⇒ 代码里所有 `&DAT_004a7dXX + side*(-0x38)` 都是「取 1−side 那一侧的字段」，这是读本作对战逻辑的**通用钥匙**。

### 1.5 `zGameManagerSide` 剩余字段（补 ExpHP 的 `__field_2c` 等）

| 偏移 | 我读到的语义 | 证据 |
| --- | --- | --- |
| +0x1c | 指向 **0xa0 B 的比分块**（`{+0 显示分 float, +4 分, +8 目标, +0xc 速度, +0x10 扩展计数, +0x14 角色 byte}`） | `0x41af2d` 里 `operator_new(0xa0)`；`0x41aa5f` 步长 `0xe` dword 遍历；`0x418a90` 读 `+4`/`+0x14` |
| +0x20 | 角色号（0..15） | `0x41a000` 与 6 比较；`0x41af2d` 读 `0xd` |
| +0x28 | **该侧是 CPU**（≠0） | `0x41af2d`：≠0 ⇒ 把配置 `DAT_004a7e78+0xb4+side` 清 0 |
| +0x2c | **角色号（用于 tuning 表索引）** | `0x4049a0`：`*(int*)(side+0x2c)*0x1c` 索引表 `0x4a0330/…/0x4a0348` |
| +0x34 | bit0 = **该侧本帧停摆**（暂停/演出） | `0x41e900`、`0x4049a0`、`0x410730` 都先测它 |

---

## 2. 花与进度 → 充能（チャージ）与「送攻击」

### 2.1 充能槽是两个数（容量 + 当前量）

**发现**：HUD 上有**两条**套在一起的槽，都是 `zPlayer` 尾部的 float。
**验证**（逐行）：
- `FUN_0041bc90(player, amount)`（`0x41bc90`）：`player+0x30388 += amount`；`side+0x20 == 1` 的角色再加成（`_DAT_0048ef60` = **0.15f**，`0x48ef60`）；上限夹到 `_DAT_0048e3c4` = **400.0f**（`0x48e3c4`）。
  它是**被击杀喂的**（`0x4102b0` 每杀一敌调一次：`min(player+0x30448, 0x81) * K + 1.0`）。
- `FUN_0041f810`（`0x41f810`，充能按键处理器）：按住 charge 键时 `player+0x30384 += dt * (sht+0x24)`，夹到 `player+0x30388`；充能到顶并持续 300 帧（`Timer::is_ge(player+0x3038c, 300)`）会**自动进发射**。
- `FUN_0041f310(player, cost)`（`0x41f310`）：`player+0x30388 -= cost`，下限 1.0。
- GUI：`GuiSide::on_tick` `0x418a90` 用 `player+0x30388`/`+0x30384` 各设一个 VM 的 `scale.x`（系数 `_DAT_0048ef04` = **0.0025f**，`0x48ef04`）⇒ 外层=容量、内层=当前量。
**结论**：✅（v1.50a）**`zPlayer+0x30388` = 充能容量（0..400，初值 100），`zPlayer+0x30384` = 当前充能量**；两条都画在 HUD 上。
🟡 **未完全拆开**：`0x41f810` 的"按键边沿 / Slow-Charge 配置 / 到顶自动"三种进发射路径我只确认到"都会汇进 `LAB_0041f92b`"，具体哪种组合才真的发射没有逐位核（U9）。

### 2.2 发射三档与「进度」

**验证**：`0x41f810` 的发射档位（常量真值我手读）：进门条件是 `player+0x30384 >= _DAT_0048e3c8` = **100.0f**（`0x48e3c8`）。
- `[200,300)`（`_DAT_0048eefc` = 200.0f，`0x48eefc`）⇒ 扣 **100**，消息 `sht+0x2c`，`FUN_00403c30` `0x403c30`（state 3、`Timer=0x30`、effect `0xd`）
- `>= 300`（`_DAT_0048ef00` = 300.0f）⇒ 扣 **200**，消息 `sht+0x6c`，`FUN_00403c90` `0x403c90`（`Timer=0x40`、effect `0xe`）
- `== 400` 且 **本侧场上无 3 级敌机**（`FUN_0040f7d0(em,3) != 0` 为假）⇒ 扣 **300**，消息 `sht+0xac`，`FUN_00403cf0` `0x403cf0`（`Timer=0x80`、effect `0xf`）

三者都调 `FUN_00403e50(spellcard, k,k,0, msg)`（`0x403e50`）＝ **符卡宣告**，k = 0/1/2 对应三档。
**进度字段**（都在 `GameManager::on_tick` `0x41aa5f` 与 `0x41af2d`）：

| 全局 | 语义 | 一手 |
| --- | --- | --- |
| `0x4a7e44` | **等级**（每次攻击的强度/HP 基数，`DAT_004a7e44 * 0.1 + 1.0`） | `0x41aa5f`：每 `DAT_004a7e54` 帧 +1，上限 `0x4a7e58` |
| `0x4a7e54` / `0x4a7e58` | 升级间隔（720/900/1800/600）/ 上限（16/20/22） | `0x41af2d` 按难度设 |
| `0x4a7e90` | **关卡/局序号**（story 里＝第几关；每赢一局 +1） | `0x415d70` +1；`0x415910` 读作对手选择索引 |
| `0x4a7e8c` | story 累计进度（每开一局 +1，用于曲线缩放） | `0x41af2d` |
| `0x4a7e48` | **每被弹扣的血**（初 2，每 720 帧 +1 至 6） | `0x41aa5f` + `0x41e420` |
| `0x4a7eac` | **难度 0..4**（Easy/Normal/Hard/Lunatic/Extra） | `0x425acb` 写；`0x41ebc0` 按档给血/充能 |
**结论**：✅（v1.50a）「进度」= `0x4a7e44`（等级）+ `0x4a7e90`（关/局）+ `0x4a7eac`（难度）三者；**没有单一"花进度条"**。

### 2.3 ★ 「把攻击送进对手场地」的入口与它改的字段

**发现**：跨场地投送**不是**直接写对手对象，而是**建一个"载体 effect"**，载体沿贝塞尔飞到对面，落地时才写对手的池。
**验证**（三条独立链，全部逐行读过）：

| 链 | 生成 | 载体类型（表 `0x4a0b48`） | 落地函数 | 落地写什么 |
| --- | --- | --- | --- | --- |
| A 敌机连锁 | `FUN_004102b0` `0x4102b0`（敌机被击破） | type `side+3`（3/4） | `FUN_0040d9e0` `0x40d9e0` | **对手 `EnemyManager`** 生成敌机 |
| B 霊 type1 | `FUN_0041dc20` `0x41dc20` case 1 | type `side+1`（1/2） | `FUN_0040d600` `0x40d600` | **对手 `BulletManager`** 发弹 |
| C 预告 | `FUN_0040c770/0x40c810/0x40c860` `0x40c770` 等 | `FUN_004151f0` 的 ExAttack type `10/0xc/0x16` | `ExAttackManager::on_tick` `0x415340` | 对手侧**演出**（sprite/VM） |

**载体生命周期**（一手，逐行）：`EffectManager::sub_40ccc0_create_something` `0x40ccc0` 取 effect 槽（步长 `0xd8`）→
`FUN_0040ca60` `0x40ca60` 按 type 查表 `0x4a0b48`（步长 `0x18`，字段序 = `{+0 起始 script id, +4 script id 步长, +8 VM 数, +0xc 构造/校验函数, +0x10 → effect+0xbc, +0x14 → effect+0xc0}`），
⇒ **`effect+0xbc` 就是落地函数指针**（type1/2 → `0x40d600`；type3/4 → `0x40d9e0`；type0/5/19..23 为 0 = 纯演出）。
各型：t1/t2 = script `0x21/0x22` + init `0x40d520`；t3/t4 = script `0x23/0x24` + init `0x40d910`（与前者同构的一对）；
t6 = script `0x56` + `+0xbc`=`0x40d470`、`+0xc0`=`0x40dc70`；t10/16/17/18/24/25 的 `+0xbc` 落在 `0x44xxxx`（ExAttack 实现区）。
`FUN_0040d520` `0x40d520`（type1/2 的 init）：把源点/目标点各做一次跨场坐标换算（`FUN_00401680` `0x401680` / `FUN_004016b0`），
随机两条贝塞尔控制臂（±π、长度 60..316），`+0x9c = rand(100.0) + 60.0`（`_DAT_0048e4a4`=60.0f）⇒ **飞行 60..160 帧**。

**A 链改的字段（★ 核心）**：`FUN_0040d9e0`
```
if (对手.charge_attack_enemy_count < 10)               // EM+0x2ac3b8，一手条件
  e = FUN_0040f1d0(对手 EM, *(short*)(载体+0xa4), 载体+0x18, 0x14, -2, 1000, 0, 0);
  e[0xce0] ^= ((short)(载体+0xa6) << 6) & 0x1c0;        // ★ 敌机 +0x3380 的 bit6..8 = charge 等级
  e[0xb7e] += *(float*)(载体+0xa0);                     // 敌机 +0x2df8 = HP 加成
  概率 1/(4或5) 时用 FUN_0041bb70(对手 player, 敌机位置) 瞄准；否则小角度抖动
```
`载体+0xa4` = 生成的敌机 type（ECL id，如 `0x13`）；`+0xa6` = charge 等级；`+0xa8` = **发起侧**；`+0xa0` = HP 加成。
**谁喂这些字段**：`FUN_004102b0` 里 `+0xa4 = min(等级,2)`、`+0xa6 = min(等级+1,3)`、`+0xa8 = 本侧 side_num`；
`FUN_0041dc20` case 1 里 `+0xa4 = 3`、`+0xa6 = 4`、`+0xa8 = 本侧`，且**一次建 `DAT_004a7e44 + 10 + DAT_004a7eac*2` 个**。

**A 链的触发条件（一手）**：`FUN_004102b0` 只在 `((enemy+0x3380 >> 6) & 7) ∈ {1,2}`（charge 等级 1/2）、`param_1 < 2`（由自机弹击破）且**对手** `EM+0x2ac3b8 < 0x19`(25) 时链下一环。
（对手 EM 的取法：`em+0x324` = `zSideData+8` = **对手 side 指针**，再 `+0x10` = 对手 `enemy_manager`。）
⇒ **等级 0（普通敌机）不连锁；等级 3 是终点**。所以「送攻击」的等级是**按对手场上已有的 charge 敌机层层加码**的。

**B 链改的字段**：`FUN_0040d600` 组 `zEnemyBulletShooter`（栈上）后调
`BulletManager::shoot_fairy_bullets_array(&DAT_004a7dd0 + side*(-0x38), …)` ⇒ **直接写对手的 `fairy_bullets` 池**；
对手 `character == 10` 时弹速再乘 `_DAT_0048e4b8`(**0.93f**)；弹速下限 `(level*0.1) + 1.0`（`_DAT_0048e4b0`=0.1f）。

**ExAttack（C 链）**：
- 管理器 `ExAttackManager`（`0x4a7e3c`，`ExAttackManager__operator_new` `0x4157d0`，`malloc(0x4c80)` = `0x101`×`0x4c`）。
- 入口 `FUN_004151f0(EM, type, pos, side, arg)` `0x4151f0`：先按 `side` 数已有条目，与 `*(int*)(EM + side*4 + 0x14)` 比较（**每侧上限**，在 `ExAttackManager::on_registration` `0x415750` 里按 `角色表 × 难度 + 角色表` 算），超限直接返回哨兵 `EM+0x4c1c`。
- 命中条目后写：`+0xc=1`(活) `+0x8=side` **`+0x4=1−side`** `+0x20/+0x24/+0x28=pos` `+0x38=arg`，
  并从**类型表 `0x4a0f78`**（**24 型 × 4 dword** = `{构造/校验, on_tick, on_draw, on_cleanup}`；`0x44ab20` 是空实现，draw/cleanup 都用它）取后 3 个写到 `+0x40/+0x44/+0x48`。
- `ExAttackManager::on_tick` `0x415340` 只做 `call [item+0x40]` 派发 + 跑 VM + 链表维护（**它自己不创建**）。
**结论**：✅（v1.50a）「送 Boss 攻击」的**入口 = `FUN_004102b0`（A，敌机连锁）/ `FUN_0041dc20` case 1（B，霊）/ `FUN_004151f0`（C，演出）**；
**它改的字段 = 对手 `EnemyManager.charge_attack_enemy_count`(`+0x2ac3b8`) 计数的敌机（A）与对手 `BulletManager.fairy_bullets`（B）**；
🔑 **`zEnemyManager+0x2ac3b8`（社区名 `charge_attack_enemy_count`）= 该侧场上"charge 敌机"数**，`EnemyManager::on_tick` `0x410730` 每帧按 `enemy+0x3380 & 0x1c0` 重算。

### 2.4 敌机的两个标记域（读对战逻辑必须知道）

| 位域 | 位置 | 语义 | 一手 |
| --- | --- | --- | --- |
| bit6..8 (`0x1c0`) | `zEnemy+0x3380` | **charge 等级 0..3**（0=普通；1..3=送来的攻击机） | `0x40d9e0` 写、`0x4102b0`/`0x410730` 读 |
| bit10..11 (`0xc00`) | 同上 | **敌机级 0..3**；`FUN_0040f7d0(EM,k)` `0x40f7d0` 找 `((v>>10)&3)==k` 的活敌机（k=3 ⇒ "场上有 Boss"） | `0x40f7d0` |
| bit13 (`0x2000`) | 同上 | 「双场同步生成」标记（`0x41aa5f` 用 type `0x13` 同时在两侧建场并置位） | `0x41aa5f` |
| +0x335c | `zEnemy` | 上述敌机的变体号（决定掉哪种霊） | `0x41aa5f`、`0x4102b0` |

---

## 3. 霊（幽灵）

### 3.1 它不是道具系统，是 `zPlayer` 尾部的 4 槽数组

**发现**：本作确实有一个「场上可收集物」，但它是**挂在自机对象上的 4 槽数组**，不是全局对象池。
**验证**（逐行）：
- `FUN_0041db90(player, kind, pos)`（`0x41db90`）：在 `player+0x30454` 起找空槽，**最多 4 个**（步长 `0x2c4`），写
  `{+0 kind, +4..+0xc pos, +0x10=0, +0x14=−3.0f(上飘), +0x1c=1(活), +0x20 起 AnmVm}`，
  并用 sprite `kind + 0x46` 载进 VM，贴图来自 `zBulletManager.etama_anm`（`+0x25e1bc`）。
- `FUN_0041dc20(player)`（`0x41dc20`）：每帧移动 + `FUN_0041bee0(player, pos, box)` 做拾取判定（判定盒由 `sht+0x10` 给），命中即按 `kind` 分发。
**结论**：✅（v1.50a）**霊 = `zPlayer+0x30454[4]`，步长 `0x2c4`，sprite = `etama.anm` 的 `0x46+kind`**。
⇒ 与 [`../../bullet/th09/01-cancel-to-item.md`](../../bullet/th09/01-cancel-to-item.md) 的「本作没有实体道具系统」**不冲突**：
那篇讲的是"消弹不撒道具"（✅ 仍成立），本文补的是"**收集物走自机内嵌数组，不走 Item 池**"。

### 3.2 生成来源与被击破（收集）的效果

| 项 | 一手 |
| --- | --- |
| 生成：`enemy+0x3380 & 0xc00`（Boss 级）被击破 | `0x4102b0`：`FUN_0041db90(player, rand_upto_n(4), 敌机位置)` |
| 生成：`enemy+0x3380 & 0x2000`（双场机）被击破 | 同上：`kind = *(int*)(enemy+0x335c)` |
| 拾取效果 kind 0 | `FUN_0041bc90(player, 400.0)` ⇒ **充能容量直接补满**（夹到 400） |
| 拾取效果 kind 1 | 建 `DAT_004a7e44+10+DAT_004a7eac*2` 个「送弹」载体（§2.3 B 链）⇒ **给对手灌弹幕** |
| 拾取效果 kind 2 | `FUN_0041d150(&score, pos, 1, 0, 400, 0)` ⇒ 加分（约 400 档） |
| 拾取效果 kind 3 | 同上，参数 `(0,0,0,70000)` ⇒ **约 70000 分档** |
| 自机弹的副瞄准点 | `zPlayer+0x30f64/+0x30f68` = 盒内**尚未收到**的霊的位置（无则 `-1000.0f`）；`0x41dc20` 写、`0x4049a0` 读 ⇒ 有霊就朝霊打、否则朝 `对手EM+0x2ac444` 的目标敌机打 ✅ |
**结论**：✅（v1.50a）**被击破/收集的收益是「充能 / 送攻击 / 分数」三选一**，没有道具。
⇒ 这条**收口**了 [`../../bullet/th09/01-cancel-to-item.md`](../../bullet/th09/01-cancel-to-item.md) §2.3 的 **N28「吸霊到底动什么」**：
`FUN_0041db90` 生成 → `FUN_0041dc20` 拾取 → 按 `kind` 分四路（0 充能 / 1 送攻击 / 2 小分 / 3 大分）。🟡 但"低速吸霊"是不是**同一套**（输入位 → 吸引半径）我没读，仍留 ❓。
❓ 霊被"吸"的半径/速度常量（`_DAT_0048ef98`=0.03f 是每帧加速度）未逐常量核。

---

## 4. 自机侧与单人作的差异

### 4.1 `.sht` 与"对战专用"资产

**验证**：`Player::on_registration` `0x41ee50`：
- `sht` 表 = `0x4a1238`，**16 角色 × 6 指针**（`PTR_s_pl00_sht_004a1238[char*6]`；表尾正好 `0x4a13b8`）。6 个文件即 `plNN.sht / plNN.anm / plNNb.anm / plNN.msg / plNN_match.msg / plNN.ecl`（串表 `0x48ecc0`–`0x48ed0c`）。
- 按 `side` 缓存已加载的 sht 指针于 `0x4a8160[side]`，结果存 `zPlayer+0x30338`。
- 另有 5 张按角色的表 → `zPlayer+0x303f0/0x303f4/0x303f8/0x303fc/0x30400`（`0x4a1bd0/0x4a1b90/0x4a1b50/0x4a1b10/0x4a1ad0`）；
  2 个按角色的**函数指针** → `+0x30404`（表 `0x4a1a50`）/ `+0x30408`（表 `0x4a1a90`）；1 个指针 → `+0x30450`（表 `0x4a19d0`，紧邻霊数组）。
**结论**：✅（v1.50a）**单人/对战差异被塞进资源层**：每角色多一份 `_match.msg` 台词库（`MATCH_MSG_FILE_1/2` `0x4a7d88`/`0x4a7d8c`，装填 `GuiMsgVm::load_match_msg_files` `0x4182e0`，按 `mode==2` 分两支）与 `plNNb.anm`。

### 4.2 决死位 = **チャージボタン**（不是 Bomb）

| 项 | 一手 |
| --- | --- |
| 每侧输入块 | `0x4ace18 + side*0x8e`（2 块，总长 `0x11c`，尾接 `INPUT_MANAGER` `0x4acf34`）；每帧由 `0x41aa5f` 从 `INPUT_MANAGER` 抄 |
| charge 键位 | 按钮 **1** 的上升沿（`InputManager::is_rising_edge(this,1)` `0x415cd0`） |
| 配置 | 每侧 byte `DAT_004a7e78 + 0xb4 + side`：`0` = Charge（充能）, `1` = Slow（低速移动）—— 见设置串 `0x8e9e4` |
| CPU 侧 | `0x41af2d`：`side+0x28 != 0` ⇒ 把该 byte 强制清 0（CPU 只会充能） |
| 充能/消费 | `0x41f810`（收）→ `0x41f310`（扣 100/200/300） |
**结论**：✅（v1.50a）**本作的"决死"键与"低速"键共用，由配置分派**；`zPlayerBomb`（`zPlayer+0x36c`，首字段回指 player，见 `0x41ee50`）仍是自机弹/爆风槽（`FUN_0041d810` `0x41d810` 取槽）。

### 4.3 命中判定 / 场地

- 场地 = `x ∈ [−144,+144]`、`y ∈ [0,448]`（`FUN_0041a6eb` `0x41a6eb`，常量 `0x48e808`/`0x48e804`/`0x48e314`/`0x48e3b8`）—— 比历代窄，详见 bullet 篇 §1.3。**对战框架不额外加场地门**（两侧用同一套坐标，跨场靠 §2.3 的换算）。
- 自机命中体：`FUN_0041dff0` `0x41dff0`（圆）/ `FUN_0041e350` `0x41e350`（圆或 AABB，外扩 `0x48e4a4` = 60.0f）。
- 自机停摆闸 = `side+0x34` bit0（§1.5），**这是本作特有的**：一侧可以停摆而不影响另一侧。

### 4.4 血量与结算计数（都在 `zPlayer`）

| 偏移 | 语义 | 一手 |
| --- | --- | --- |
| +0xa8 | **局内血量 0..10**；初值来自 `0x4a8108 + side*4`，由菜单在开赛前写 **10** | `FUN_00429e23` `0x429e23` 里 `_DAT_004a8108 = 10; _DAT_004a810c = 10;`（指令级：`89 1d 08 81 4a 00`）；`0x41ebc0` 抄进 player |
| +0xa8 的初值陷阱 | `.data` 里 `0x4a8108/0x4a810c` 的**静态初值是 π（`0x40490fdb`，ZUN 的"未设"哨兵）** | ✅ 手读 `.data` + 指令 `8b 0c 85 08 81 4a 00` / `89 8e a8 00 00 00` |
| +0xa0 / +0xa4 | 两个按难度的计数（1..14），**语义未定** ❓ | `0x41ebc0` switch(`DAT_004a7eac`)；GUI 以 2 位数字画（`0x418a90`） |
| +0xac / +0xb0 / +0xb4 / +0xb8 | story 结算用计数（`{+0,×150000,×300000,×300000}`，上限 67/34/34） | `Gui::sub_415e50_show_stage_results` `0x415e50` |
| +0xa8 | story 结算 `×100000`（残机奖励） | 同上 |
| +0x30418 | ×10000 奖励（`< 1000`） | 同上（🟡 与 combo 读法一致，见 bullet 篇 §1.6） |
**结论**：✅（v1.50a）「血量 10 / 每弹扣 `0x4a7e48`」是本作的"残机-制"替代；**没有单人作的 `残機→Bomb` 循环**，取而代之是 §2.1 的两条充能槽。

---

## 5. `vs`（Match）与 `story` / `extra` 的系统差异

**模式判定就三个函数，全在 `GAME_MANAGER+0x118`（`0x4a7ea8`）上**：

| 模式 | 值 | 判定函数 | 一手 |
| --- | :-: | --- | --- |
| Story（花映塚モード） | 0 | `FUN_0040e470` `0x40e470`（`== 0`） | 3 行 |
| Extra | 1 | `FUN_00404970` `0x404970`（`== 1`） | 3 行 |
| Match（対戦） | 2 | `GameManager::sub_415d30` `0x415d30`（`== 2`） | 3 行 |

| 差异点 | Story / Extra | Match（=2） | 一手 |
| --- | --- | --- | --- |
| 胜局阈值 `0x4a7e94` | 1 | **2** | `0x41af2d` |
| 对手选择 | `FUN_00415910` `0x415910`（按 `0x4a7e90` 关卡；-1=随机、-2=全员；解锁表 `0x4a81cc`） | 同函数但走"全员/随机"，或由网络/菜单给 | `0x415910` |
| 台词库 | `plNN.msg` | **`plNN_match.msg`**（`MATCH_MSG_FILE_1/2`） | `0x4182e0` |
| 菜单子模式 | — | `DAT_004a7ea4` 0..4 = Com/Com、Com/人、人/Com、人/人(+Extra) | `0x425acb` 写；串 `0x8e060`..`0x8e090` |
| 侧是 CPU | 由剧本定 | `side+0x28`≠0 ⇒ 强制"自动充能"；`GM+0x28`/`GM+0x60` 分别记两侧 | `0x41af2d` |
| tick 门 | — | `0x41aa5f` 开头：`mode != 2`、或 `GM+0x28 != 0`、或 `GM+0x60 != 0` 才抄输入 | `0x41aa5f` |
| 结算画面 | `Gui::sub_415e50_show_stage_results` `0x415e50`（story 计分） | `GuiSide` 少跑一组 sprite（`0x41a000` 里 `mode==2` 跳过 sprite `0x28` 组） | 两处 |
| 网络 | — | `NetworkManager`（`0x4b42d0`，`NetworkManager::initialize` `0x4324c0`），Dial 名 `TH09YoukaiMatch`（`0x8fde4`），DirectPlay8 Peer | 串 |

**结论**：✅（v1.50a）**系统层面 vs 与 story 的差别是"参数级"的**（胜局阈值、台词库、对手选择、CPU 标志、结算），**不是两套循环**：
两侧 player 槽、充能轴、送攻击链、霊数组在三种模式下都是同一套代码；story 只是**由 `FUN_00415910` 每关换对手**并跑结算计分。

---

## 6. TH09 ↔ TH19 对照（**待验证**表）

> TH19 一侧本文**没有挖**（另有 agent）；下表右列全部来自 [`../../../games/th19.v1.10c/INDEX.md`](../../../games/th19.v1.10c/INDEX.md) 的**登记层**（RTTI 类名），
> **不是我的一手**。左列是本文 ✅ 的一手结构。**凡是没对上的一律 ❓。**

| TH09（本文一手） | TH19 候选（❓ 待验） | 待验证的具体问题 |
| --- | --- | --- |
| 两个对称 player 槽 `GM+0x0/+0x38`（7 件套/侧） | `GameRuleTaskInf` + `PlayerInf`×2 ❓ | TH19 还是"两个对称槽"吗？ |
| 充能容量 `zPlayer+0x30388`（0..400）/ 当前量 `+0x30384` | `EXReigekiInf`（霊撃）/ チャージ ❓ | TH19 是不是"容量+当前量"两条？ |
| 送攻击 = effect 载体（type `side+1/+3`）→ 对手 `EM`/`BM` | **`ExtraAttack*Inf`**（疑＝動物霊送り込み）❓ | 载体对象存在吗？落地写谁？ |
| `zEnemy+0x3380` bit6..8 = charge 等级 0..3；`EM+0x2ac3b8` 计数 | ❓ 无对应登记 | TH19 有"等级层层加码"吗？ |
| 霊 = `zPlayer+0x30454[4]`（`etama.anm` `0x46+k`），4 种效果 | **`GhostCtrlInf`** / `SmallScoreInf` ❓ | 是 manager+池，还是自机内嵌数组？ |
| `ExAttackManager` `0x101`×`0x4c` + 类型表 `0x4a0f78`（24 型） | ❓ 无对应登记 | TH19 还有 ExAttack 这一层吗？ |
| 胜负 = `FUN_00415d70` + `0x4a7e98[side]` + 阈值 `0x4a7e94` | `GameRuleTaskInf` ❓ | 规则 task 里是不是同构？ |
| 模式 = `GM+0x118` 0/1/2（Story/Extra/Match） | ❓ | TH19 的模式枚举怎么分？ |
| 无分数？→ **TH09 有分数**（`side+0x1c` 比分块、`0x41e420` 加算） | 登记称 TH19 **无分数 / 无 Replay**（二手）❓ | 这是本组**最大的一条反向差异**，要一手核 |
| 每角色多一份 `plNN_match.msg` | `ability.txt` 的每角色能力卡 ❓ | 不是同层概念，别硬对 |

---

## 7. 未解与待补反编译

### 7.1 未解（本文没钉死的）

| # | 项 | 现状 |
| --- | --- | --- |
| U1 | `zPlayer+0xa0` / `+0xa4` 两个按难度的 1..14 计数是什么（`0x41ebc0` switch） | ❓ 只知 GUI 以 2 位数字画 |
| U2 | 超时（`GM+0x140 == 0x17e8` → `GameManager::on_tick` `return 3`）由谁判胜 | ❓ `UpdateFuncRegistry` 返回码语义未读 |
| U3 | 低速（Slow）模式是否也吸引霊 / 是否清弹 | ❓ 只知 charge 键与 Slow 键共用（§4.2） |
| U4 | `EnemyManager::on_tick` 里 `enemy+0x1000` 位那一支发的弹幕（`local_2ac` 结构）属于谁 | ❓ 只读到它调 `shoot_fairy_bullets_array`（本场） |
| U5 | `0x41dc20` case 1 的载体 `+0x26`（延时索引）与 `+0x28`（HP）如何叠加成对手看到的强度 | ❓ 只读到写入 |
| U6 | `Spellcard`（`0x4041f0` on_tick）在"三档攻击"之外还做什么 | ❓ 未逐行 |
| U7 | `ExAttack` 24 型各自的语义（`0x440e40`..`0x44b940` 一片） | ❓ 只列了类型表 |
| U8 | `DPlay` 网络同步了哪些字段（`0x429e23` / `0x425acb` / `0x4289db` 的 `_DAT_004a8108` 往返） | ❓ 只看到初血值在菜单/网络间搬运 |
| U9 | `0x41f810` 的发射触发到底是"松键 / 到顶 300 帧 / Slow 配置"哪种组合 | 🟡 只确定了三条路径都汇进 `LAB_0041f92b` |

### 7.2 待补反编译（**不新开 Ghidra 库**，列给下一轮）

语料库已覆盖全部 2002 函数，下列是**语料里有、但本轮没读透**的函数（按价值排序）：

| # | 地址 | 为什么值得读 |
| --- | --- | --- |
| D1 | `0x41c170`（1832 B，Player 状态机） | `zPlayer+0` 状态机；U1/U3 的答案在这里 |
| D2 | `0x41fcd0`（985 B） | Player 尾部一大块；可能含 `+0xa0/+0xa4` 的消费点 |
| D3 | `0x4041f0` `Spellcard::on_tick` + `0x404410` on_draw | U6；三档攻击的对手侧表现 |
| D4 | `0x40c960` / `0x40d470` / `0x40dc70`（effect type7／t6/8/13..15 的落地） | 还有几种跨场投送没读 |
| D5 | `0x440e40`–`0x44b940`（ExAttack 24 型的 init/tick） | U7；对手侧"预告"的全部表现 |
| D6 | `0x415910` `FUN_00415910` 的表 `0x4a14c8`/`0x4a1500`/`0x4a1504` | 对手选择与关卡曲线的真值 |
| D7 | `0x41db90` 的上游 `0x4102b0` 的 `param_1`（击破来源）分类 | "谁算 player 击杀"（影响霊生成） |
| D8 | `0x41bee0`（拾取判定） | 霊的吸引半径/盒形（U3） |

### 7.3 复现命令

```bash
cd /home/nipporita/renkolab
# ① 骨架 / 胜负
python3 tooling/ghidra/th20_show.py --corpus local/th09.v1.50a/decomp 0x41af2d   # 建场（两侧 7 件套）
python3 tooling/ghidra/th20_show.py --corpus local/th09.v1.50a/decomp 0x41e420   # 自机死 → 判对手胜
python3 tooling/ghidra/th20_show.py --corpus local/th09.v1.50a/decomp 0x415d70   # 某侧赢一局
# ② 充能 / 送攻击
python3 tooling/ghidra/th20_show.py --corpus local/th09.v1.50a/decomp 0x41f810   # charge 键 → 三档
python3 tooling/ghidra/th20_show.py --corpus local/th09.v1.50a/decomp 0x4102b0   # 敌机死 → 建载体
python3 tooling/ghidra/th20_show.py --corpus local/th09.v1.50a/decomp 0x40d9e0   # 落地：写对手 EM
python3 tooling/ghidra/th20_show.py --corpus local/th09.v1.50a/decomp 0x40d600   # 落地：写对手 BM
# ③ 霊
python3 tooling/ghidra/th20_show.py --corpus local/th09.v1.50a/decomp 0x41db90   # 生成
python3 tooling/ghidra/th20_show.py --corpus local/th09.v1.50a/decomp 0x41dc20   # 拾取 4 种效果
# ④ 常量真值（务必自己读，别信社区）
python3 tooling/pe_read.py local/th09.v1.50a/th09.exe 0x48e3b0 --dwords 12   # 400/100/25/… 
python3 tooling/pe_read.py local/th09.v1.50a/th09.exe 0x4a0b48 --dwords 24   # effect 类型表
python3 tooling/pe_read.py local/th09.v1.50a/th09.exe 0x4a0f78 --dwords 96   # ExAttack 24 型表
python3 tooling/pe_read.py local/th09.v1.50a/th09.exe 0x4a8104 --dwords 4    # 初血表（静态=π 哨兵）
```

**常量真值（本文用到的）**：`0x48e3c4`=400.0f、`0x48e3c8`=100.0f、`0x48eefc`=200.0f、`0x48ef00`=300.0f、`0x48ef04`=0.0025f、
`0x48e4b0`=0.1f、`0x48e4b4`=0.3f、`0x48e4b8`=0.93f、`0x48e4a4`=60.0f、`0x48efa0`=32.5f、`0x48e32c`=10.0f、
`0x40490fdb`=π（初血表的静态哨兵）、`0x48e3d4`=96.0f、`0x48e3d0`=288.0f、`0x48e3cc`=192.0f。
