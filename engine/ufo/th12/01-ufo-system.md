# ufo 逆向 01：TH12 飞碟（UFO）系统全链（令牌 / 召唤 / 吸收 / 奖励）
> **版本**：TH12 v1.00b（`th12.exe`，imagebase `0x400000`）。本文裸地址默认属该版本；引用其他版本须写成 `th13:0x…`。
>
> **方法**：Ghidra 12 工程 `local/th12.v1.00b/ghidra_projects/th12.exe.gpr` 一手反编译/反汇编
> + `.dat` 侧 `thecl -d 12` / `thanm -l 12` + `.data` 静态字节直读。日期 2026-09-09。
> **可信度**：本文绝大多数条目 ✅（一手反编译 + 反汇编复核）；推断/未解一律标 🟡/❓。

## 0. 一句话结论

TH12 的飞碟系统 = **「收 3 个同色令牌 → 出一个对应颜色（或三色各一 → 彩虹）的 ECL 敌机 UFO
→ 它在上方 280×128 的框里飘、并吸走你的 P/点道具 → 击破（或吸满）时吐奖励」**。
全部判定都在 `Ufo`（ZUN 源码里叫 **`UfoInf`**，见 `0x44a113` 的日志串 `initialize UfoInf`）对象里：
令牌槽状态机 `0x422e80`、召唤 `0x44a8a0`、吸收 `0x44aca0`、击破奖励 `0x44af00`。

## 1. 样本与地址对齐（本文的证据前提）

- exe：`local/th12.v1.00b/th12.exe`，737,280 B，md5 `212f187591f4f1fa99eb2845d1b3cf20`，PE 时间戳 2009-08-27，i386，imagebase `0x400000`。
- ✅ **对齐复核**：`th-re-data` 的 v1.00b 地址在本文件里逐条对上——`Ufo__on_tick` `0x44a2d0`
  反编译出 `GLOBALS.__UFO_COUNT__COPY_1C == 3`、`field_20`；`Ufo__initialize` `0x449fa0` 里三次
  `malloc(0x24)` + `UpdateFuncRegistry__register__on_tick/on_draw`；`Ufo__operator_new` `0x44a230`
  的 `malloc(0x6c)` 与 zUfo 大小一致。**地址空间与 th-re-data 的 v1.00b 一致** ✅。
- 资产：`local/th12.v1.00b/th12.dat`（THA1）。本文用 `thdat -x 12`、`thecl -d 12 -m local/vendor/eclmap/eclmap/th12.eclm`、
  `thanm -l 12 -m local/vendor/thpages/static/mapfile/v4.anmm`（★ **TH12 用 v4.anmm，不是 v8**）。

## 2. 全局状态：`GLOBALS`（`0x4b0c40`）里的 UFO 字段 ✅

`FUN_00422e80` 把 `0x4b0c4c` 当作结构基址用（`ESI = 0x4b0c4c`，字段 `+0x0..+0x18`），
所以这几个字段**连在一起**，偏移相对 `0x4b0c4c`：

| 地址 | zGlobals 偏移 | 真名（本文定名） | 含义 |
| --- | --- | --- | --- |
| `0x4b0c4c` | +0xc | `COLLECTED_UFOS[0]` | 第 1 个令牌槽，存**颜色值** 1/2/3 |
| `0x4b0c50` | +0x10 | `COLLECTED_UFOS[1]` | 第 2 个令牌槽 |
| `0x4b0c54` | +0x14 | `COLLECTED_UFOS[2]` | 第 3 个令牌槽 |
| `0x4b0c58` | +0x18 | `__UFO_COUNT__COPY_18` | 已填槽数 1/2/3（HUD 用） |
| `0x4b0c5c` | +0x1c | `__UFO_COUNT__COPY_1C` | 同上，但 **4 = 「第 3 个令牌没凑成组」**；**== 3 才召唤** |
| `0x4b0c60` | +0x20 | `__field_20` | 每次收令牌清 0、每帧 `+GAME_SPEED`（`0x422b87`）；召唤要求 `>= 0` |
| `0x4b0c64` | +0x24 | `__field_24` | **要召唤的 UFO 颜色**：1/2/3，`-1` = 彩虹 |

- **① 发现**：`Ufo__on_tick` `0x44a4b2`–`0x44a4c8`：`CMP [0x4b0c5c],3` / `CMP [0x4b0c60],EBP(=0)` / `JL` /
  `MOV EAX,[0x4b0c64]` / `CALL 0x44a8a0`。`0x4b0c60` 只在 `GameThread__on_tick` `0x422b87` 被 `ADD ... ,EBX`。
- **② 推测**：`0x4b0c5c` 是「3 个令牌凑齐」的旗标，`0x4b0c64` 是召唤参数。
- **③ 验证**：反编译 `0x422e80`（§3）——它往 `0x4b0c58/5c/60/64` 逐个写入，语义自洽；召唤函数
  `0x44a8a0` 用 `in_EAX-1` 索引 `0x4b33e0` 的名字表，与颜色 1/2/3 对上（§5）。
- **④ 结论**：上表 ✅（TH12 v1.00b）。`__field_20` 的「`>= 0`」门**实际恒真**（它每帧自增），
  🟡 疑为防御性写法/残留。
- **⑤ 证据**：`0x44a4b2`、`0x422b87`、`0x422e80` 反汇编；`th-re-data` 的 `zGlobals` 命名（`COLLECTED_UFOS` 等）。

## 3. 令牌槽状态机 `FUN_00422e80` `0x422e80` ✅（一手反汇编逐条）

调用点：`FUN_004270b0` `0x427155`（令牌被收集时），`EDI = 令牌颜色(1/2/3)`、`ESI = 0x4b0c4c`。

```c
/* ① 发现（0x422e80 反汇编） */
if (color <= 0) return 0;
field_20 = 0;                                      // 0x4b0c60
if (slots[2] != 0) return 0;                       // 第 3 槽已占，忽略
if (slots[0] == 0) { cnt18=1; cnt1c=1; slots[0]=color; return 0; }
if (slots[1] == 0) { cnt18=2; cnt1c=2; slots[1]=color; return 0; }
slots[2] = color; cnt18 = cnt1c = 3;
if (slots[0]==color && slots[1]==color) { field_24 = color; return color; }   // 三同色
if (slots[1]==color || slots[0]==slots[1]) goto KEEP2;                        // 两同一异
field_24 = -1; return -1;                                                     // 三色各一 → 彩虹
KEEP2:  FUN_00421a60();                            // HUD 槽位重排
        slots[0]=slots[1]; slots[2]=0; cnt18=2; cnt1c=4; slots[1]=color; return 0;
```

- **④ 结论** ✅：**收 3 个令牌时判定**——三同色 → 出该色；三色各一 → 彩虹；**恰好两同一异 →
  丢弃最旧的那个、保留最近两个**（`cnt1c = 4`），不召唤。
- **「计数何时清零」** ✅：`Ufo::on_tick` 的「UFO 结束」分支（`0x44a326`–`0x44a344`）把
  `COLLECTED_UFOS[0..2]`、`0x4b0c58`、`0x4b0c5c` 全清 0；另外 `GameThread__thread_start` `0x421e3b`
  与 `FUN_0043c730` `0x43c87d`（进关/重开重置）也清。
- **⑤ 证据**：`0x422e80` 全函数反汇编（`0x422e81`–`0x422f18`）；`0x422f15` 的 `return 0` 分支。

## 4. `Ufo` 对象：`zUfo` 结构体（`malloc(0x6c)`）✅

`Ufo__operator new` `0x44a230` 里 `malloc(0x6c)`、`Ufo__initialize` `0x449fa0` 写 `+0x8/+0xc/+0x24`
三个 `zUpdateFunc` 句柄、`Ufo__destructor` `0x44a120` 注销这三个句柄。

| 偏移 | 类型 | 含义 | 出处 |
| --- | --- | --- | --- |
| `+0x00` | uint | 标志位（`operator new` 里 `|= 2`） | `0x44a230` |
| `+0x08` | `zUpdateFunc*` | on_tick 句柄 | `0x449fa0` |
| `+0x0c` | `zUpdateFunc*` | on_draw_3c 句柄 | `0x449fa0` |
| `+0x10` | `zTimer` | 帧计时器（`prev/cur/cur_f/speed=&GAME_SPEED/control`） | `0x449fa0`、`0x44a4d8` |
| `+0x14` | int | **= 计时器 `cur`：UFO 存活帧数** | `0x44a2e0`、`0x44a3b2` |
| `+0x24` | `zUpdateFunc*` | on_draw_16 句柄 | `0x449fa0` |
| `+0x28` | int | 存活上限 = **720**（`0x2d0`） | `0x44a8a0` |
| `+0x2c` | int | 「已召唤」闩锁（0 = 未召唤） | `0x44a8a0`、`0x44a34f` |
| `+0x30` | int | UFO 颜色：1 红 / 2 蓝 / 3 绿 / `-1` 彩虹 | `0x44a8a0` |
| `+0x34` | `zEnemy*` | 生成的 UFO 敌机指针 | `0x44a8a0` |
| `+0x38` | int | `enemy->enemy_id`（`zEnemy+0x27b4`）副本 | `0x44a8a0` |
| `+0x3c` | int | `bullet.anm` 脚本 **`0xcf`(207)** 的 VM id（UFO 光环） | `0x44a8a0` |
| `+0x40` | int | `front.anm` 脚本 **`0x86`(134)** 的 VM id（血条外框） | `0x44a8a0` |
| `+0x44` | int | `front.anm` 脚本 **`0x88`(136)** 的 VM id（血条填充） | `0x44a8a0` |
| `+0x48` | int | 吸收的**道具类型 1** 计数 | `0x44aca0` |
| `+0x4c` | int | 吸收的**道具类型 2** 计数 | `0x44aca0` |
| `+0x50` | float | **吸收填充率 0..1**（满 1.0 后不再涨） | `0x44aca0` |
| `+0x54/58/5c` | float3 | 奖励弹字坐标 | `0x44af00` |
| `+0x60` | float | 弹字倍率（`*%1.1f`） | `0x44af00`、`0x44a5d0` |
| `+0x64` | int | 弹字剩余帧（击破时置 120） | `0x44af00` |
| `+0x68` | int | 弹字要显示的分数 | `0x44af00` |

**生命周期** ✅：`Ufo::operator new`（建对象 + 注册三个更新函数）→ `Ufo::on_tick` 每帧
（`+0x38 == 0` 时空转；凑齐 3 令牌则召唤）→ UFO 敌机被击破（`on_death_callback`，§8）或飞离
（ECL `delete()`）→ `on_tick` 发现敌机列表里找不到 `+0x38` 的 id → 走「结束」分支：清计数器、
停循环音、删光环 VM、给两个血条 VM 发 `interrupt 1`（淡出）。

## 5. 召唤 `FUN_0044a8a0` `0x44a8a0` ✅（一手反汇编）

```c
/* ① 发现：0x44a8a0 起（in_EAX = GLOBALS.__field_24 = 颜色） */
if (this->summoned) return;                      // this+0x2c
this->summoned = 1;
memset(&spawn, 0, 0x50);
spawn.rel_pos = {0.0f, 128.0f, 0.0f};            // +0x0/+0x4/+0x8
spawn.life = DAT_004b33f0[CURRENT_POWER / POWER_PER_LEVEL];   // +0x14
idx = (color == -1) ? 3 : color - 1;
enemy = EnemyManager__allocate_new_enemy((&PTR_s_UFO_Red_004b33e0)[idx], &spawn);
this->enemy = enemy;  this->id = enemy->enemy_id;  this->color = color;
enemy->on_death_callback = 0x44a890;             // ★ 击破奖励入口
... 新建 front.anm 脚本 0x52 的 VM，pending_interrupt = (color==-1?3:color-1) + 7
... 新建 bullet.anm 脚本 0xcf 的 VM（layer 0x17），entity_pos = enemy + (224, 16)
this->timer = 0;  this->fill = 0;  this->cntA = 0;  this->cntB = 0;
this->vm40 = front.anm 脚本 0x86 的 VM;  this->vm44 = front.anm 脚本 0x88 的 VM;
this->limit_frames = 720;
play_sound_centered(0x34);  play_sound_at_position(0x36, enemy->x);   // 0x36 循环、随 x 调 pan
```

| 项 | 值 | 出处 |
| --- | --- | --- |
| 颜色 → ECL 子程序名表 | `0x4b33e0` = {`UFO_Red`, `UFO_Blue`, `UFO_Green`, `UFO_Rainbow`} | `0x44a909` 索引、字符串在 `0x4a20d8..0x4a20fc` |
| 颜色 1/2/3/`-1` | 红 / 蓝 / 绿 / 彩虹 | `0x44a985`–`0x44a991` |
| 出生位置 | rel_pos = **(0, 128, 0)** | `0x44a8a0`（`local_54 = 0x43000000` = 128.0f） |
| **HP 表**（按火力档） | `0x4b33f0` = `{1600, 1900, 2100, 2300, 2500}`，档 = `CURRENT_POWER / POWER_PER_LEVEL` | `0x44a8f3`、`EnemyManager__allocate_new_enemy` `0x412990` 写 `life.cur = life.max = param[5]` |
| 存活上限 | `this+0x28 = 720`（ECL 在 `timer == 420` 时离开，见 §6） | `0x44a8a0`、`0x44a3b6` |

> ★ **HP 表不是分数表**：`EnemyManager__allocate_new_enemy` 把 `param_2[5]` 同时写进
> `zEnemyData+0x1608`（`life.current`）和 `+0x160c`（`life.maximum`）——所以 `0x4b33f0` 是 UFO 的
> **血量**，不是得分。`on_draw_16` 的 `scale.x = life.current / life.maximum`（`0x44a5d0`）也印证。
> `life > 999` 时引擎额外置 `zEnemyData.flags |= 0x20000000`（`0x412a94`），故 UFO 恒有血条。

## 6. ECL 侧：`default.ecl` 的四个 UFO 子程序 ✅

`thdat -x 12` 解出的 **`default.ecl`** 是唯一含 UFO 的脚本（8 个关卡 `.ecl` 里一个都没有）；
`thecl -d 12 -m th12.eclm` 得 4 个主体 + 2 个特效子程序：

| 子程序 | 关键指令 | 含义 |
| --- | --- | --- |
| `UFO_Red` | `anmSetSprite(0, 135)` | 红（sprite 135） |
| `UFO_Blue` | `anmSetSprite(0, 136)` | 蓝（sprite 136） |
| `UFO_Green` | `anmSetSprite(0, 137)` | 绿（sprite 137） |
| `UFO_Rainbow` | `anmSetSprite(0, 138)` | 彩虹（sprite 138） |
| `UFO_EtBreak` | `etCancel(%A)`，`%A` 从 16 每帧 +6 到 112 | 召唤瞬间的**扩圈消弹**（半径 16→112） |
| `UFO_EtBreak2` | `etCancel(%B)`，`%B` 从 16 每帧 +6 到 640 | 击破时的大消弹（半径 16→640） |

四个主体的脚本**逐字节相同**，只有 sprite 号不同：

```
anmSelect(1); anmSetSprite(0, 135..138);
@UFO_EtBreak() async;                       // 扩圈消弹
moveLimit(0.0f, 128.0f, 280.0f, 128.0f);    // 活动框
flagSet(1090);  setHurtbox(48,24); setHitbox(16,16);
flagSet(15); wait 60; flagClear(15);        // 前 60 帧带 flags 0..3（bit0 = 无敌）
moveCircleRel(%RANDRAD, 0.0349, 0, 0); moveCircleRelTime(60, 0, 0.0349, 16, 0);
/* 状态机（见下） */
```

- **`moveLimit` 的真语义** ✅：ECL opcode 404 的 handler `0x4174fd` 写
  `move_limit_center = (arg0, arg1)`、`move_limit_size = (arg2, arg3)`，夹取函数 `0x413700` 用
  `center ± size*0.5` ⇒ 活动框 = **x∈[−140,140]、y∈[64,192]**（以 (0,128) 为中心）。
- **`flagSet/flagClear` 的真语义** ✅：opcode 402/403 的 handler `0x417934`/`0x417974` 是
  `zEnemyData.flags |= arg` / `&= ~arg`（`+0x16b8`，即 `zEnemy+0x26f8`）——**参数是位掩码，不是位号**
  （全 ECL 里 `flagSet(64)/(32)/(16)/(797)…` 都是掩码形态）。
  ⚠️ `flagSet(1090)` = `flags |= 0x442`（位 1/6/10）：**全 exe 里没有任何指令读这三位**（🟡 疑似残留）。
- **掉落指令的真语义** ✅（本文用到的部分）：`dropMain(i)`（op 410，handler `0x417a38`）写
  `drop.main_drop = i`；`dropExtra(i, n)`（op 407，handler `0x4179cc`）写 `[enemy+0x1620 + i*4] = n`，
  i≥1 落在 `drop.ex_drop[i-1]`，而 `EnemyDrop__spawn_ex_drops` `0x4121e0` 把 `ex_drop[j]` 生成为
  **道具类型 j+1** ⇒ **`dropExtra(i, n)` = 掉 n 个类型 i 的道具**（i=0 例外，会写 `main_drop`）。
  据此：全 8 关 ECL 里令牌掉落只有 **`dropExtra(10,1)`（stage01）** 与 **`dropExtra(11,1)`（stage02）**
  两处 ⇒ 绝大多数令牌来自本文未普查的敌人脚本（§12 未答 8）。
- **状态机（`$I0` = `int_vars[0]` = `zEnemyData+0x23c`，引擎每帧写）** ✅：

| `Ufo::on_tick` 写的值 | 条件（`0x44a3af`–`0x44a3df`） |
| --- | --- |
| `$I0 = -999` | 存活帧数 `< 420` 且场上无 BOSS（`FUN_00449f20` 查 `manager+0x1c[0..7]`） |
| `$I0 = 999` | 存活帧数 `== 420`，**或**场上有 BOSS |
| 不改 | 存活帧数 `> 420` 且无 BOSS（保持上一次的 999） |

ECL 据此：`_480` 每 60 帧轮询一次，见到 999 → `_648` → `_920`（**`moveLimitReset()` + 300 帧飞到 y=−64**，
即「离场」）；`_1124` 每 60 帧轮询，若 `$I0 == -999` → 飞回 y=128 重新开始。`$D = 300` 次轮询后
（离场再等 300×60 帧）`delete()`。

> **读法** 🟡（数值 ✅、设计意图推测）：**UFO 从召唤起最多待 7 秒（420 帧）就飞走**；
> BOSS 在场时它一开始就把 `$I0` 置 999、约 240 帧就飞走；如果它在飞走途中 BOSS 被打死
> （`timer < 420` 且无 BOSS → 引擎重新写 −999），它会飞回来。这是 ECL 里那条 `_1124`
> 「回来」分支唯一能被触发的场合。

## 7. 吸收链：道具飞向 UFO ✅

- **谁被吸** ✅：`FUN_004271c0` `0x4271c0`——`UFO 存活帧数 > 59` 且 **道具类型 == 1 或 2** 时，把道具
  `state` 置 7、速度清零、吸引距离清零，返回 1。它在道具的 4 个运动状态里都被调用
  （`0x425cc9` / `0x425f31` / `0x425ff2` / `0x42624b`），命中后一律跳 `0x426879`
  （state 6/7 的「飞向 UFO」通道）。**注意是全场道具，不是半径内**。
  ⚠️ **类型 1/2 的身份** ✅（效果一手）：类型 1 的收集效果是「火力已满则 +1 分，否则加火力」
  （`0x426c3f` → `FUN_00426ff0` `0x426ff0`）⇒ P 道具；类型 2 按最大得点给分
  （`Globals__sub_421880` `0x421880`）⇒ 点道具。命名依效果推断。
- **飞行** ✅（`0x426879`–`0x4268f0`）：方向 = `atan2(UFO − item)`（`FUN_0044ac00` `0x44ac00`），
  速度 `item+0x9bc` 每帧 `+= (UFO.timer/600)*0.05 + 0.02`（上限 `0x4a4250` = 6.0），
  到 UFO 中心 18 px 内（距离² `< 0x4a43e0` = 324）→ 道具消失 + 调 `FUN_0044aca0`。
- **入账 `FUN_0044aca0` `0x44aca0`** ✅（`in_EAX` = 道具类型）：

| 道具类型 | 效果 |
| --- | --- |
| 1（P 道具） | 颜色 ≠ −1：`cntA(+0x48)++`；颜色 = −1：`cntB(+0x4c)++`；填充 `+= 0.032 − 关卡号×0.002` |
| 2（点道具） | 反过来：`cntB++`（彩虹时 `cntA++`）；填充同上 |
| 10/11/12（固定色令牌） | 填充 `+= 0.1` |
| 其它 | 无效果 |

- **吸满（`fill > 1.0`）时** ✅：把 `fill` 夹回 1.0、放音、给光环 VM 发 `interrupt 2`、
  然后按颜色吐**一件**奖励：红 → 道具类型 4（残机碎片）；绿 → 类型 7（整颗 Bomb）；彩虹 →
  类型 13/14/15（按 `COLLECTED_UFOS[2]`）；**蓝 → 什么都不吐**（switch 无 case 2）。
- **填充阈值 vs 社区** ✅✅（强交叉验证）：`fill` 每次 +`0.032 − 0.002×stage`，判定是**严格**
  `> 1.0`，所以所需个数 = `floor(1/(0.032−0.002·stage)) + 1`：

| 关卡 | 1 | 2 | 3 | 4 | 5 | 6 | Ex |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 本文一手算出 | 34 | 36 | 39 | 42 | 46 | 51 | 56 |
| [東方Project攻略 Wiki 星/Data](https://wikiwiki.jp/thk/%E6%98%9F/Data) | 34 | 36 | 39 | 42 | 46 | 51 | 56 |

> 同一页还说「虹ベントラーの色は約150Fで変わる／近づくと変化までの時間が止まる」——
> 与 §9 的 `item+0x9a0`（150 帧换色、60 px 内暂停）**逐条对上**。

## 8. 击破奖励 `FUN_0044af00` `0x44af00` ✅

入口 = UFO 敌机的 `on_death_callback`（`zEnemy+0x103c`，在 `0x44a8a0` 里设为 **`0x44a890`**，
内容是 `CALL 0x44af00`）。`Enemy__die` `0x414c40`–`0x414c4c` 调它。
★ **只有「被打死」才走这条路**：ECL 的 `delete()`（超时离场）**不**经过 `Enemy__die`
（全 exe 只有 `EnemyData__on_tick` `0x413f3c`/`0x413f6c` 调 `0x414af0`）。

`0x44af00` 先做三件事：在 UFO 位置生成 **`UFO_EtBreak2`** 敌机（`life = 3000`，扩圈消弹）、
播放大消弹音、生成 bullet.anm 脚本 `0xd0`(208) 的特效 VM；然后按 `cntA/cntB` 生成「赏金道具」：

| 赏金道具类型 | 条件 | 内容 |
| --- | --- | --- |
| 18（`0x12`） | `cntA != 0 && cntB != 0` | `+0x9cc = cntA`、`+0x9d0 = cntB`、`+0x9c8 = UFO 颜色` |
| 17（`0x11`） | 只有 `cntB != 0` | 同上 |
| 16（`0x10`） | 只有 `cntA != 0` | 同上 |
| 无 | 两个都 0 | 不生成 |

赏金道具的生成参数是 **角度 0、速度 0**（`0x44b0b1`–`0x44b0e1` 的两条 `FLDZ`），即原地不动。
再按 UFO 颜色补一件 + 一个彩虹令牌（角度 `0x4a4060` = **−1.5707964**、速度 `0x4a42c0` = 2.2）：

| 颜色 | 额外掉落 |
| --- | --- |
| 1 红 | 道具 **4**（残机碎片） + 道具 **13**（彩虹令牌，起始红） |
| 2 蓝 | 道具 **14**（彩虹令牌，起始蓝） |
| 3 绿 | 道具 **5**（Bomb 碎片） + 道具 **15**（彩虹令牌，起始绿） |
| −1 彩虹 | 道具 **13/14/15**，按 `COLLECTED_UFOS[2]`（= 最后收的第 3 个令牌的颜色） |

- **④ 结论** ✅（TH12 v1.00b）：**击破 UFO = 赏金道具（分数，值随吸收量） + 颜色加成
  （红给残机碎片 / 绿给 Bomb 碎片 / 蓝无） + 一个彩虹令牌**；赏金道具的类型由两种吸收计数决定。
- **⑤ 证据**：`0x44af00` 反编译 + 9 个 `spawn_item` 调用点的上下文反汇编（参数 = 类型、
  角度 `0x4a4060` = −π/2、速度 `0x4a42c0` = 2.2）；`0x44a890` = `CALL 0x44af00`。
- **社区对账** ✅：Wiki 用「红+虹令牌共 55 个 → 27 次 UFO」推算（55÷2≈27，因为每个 UFO 击破后
  还回 1 个彩虹令牌）、「27 个红 UFO → 6 次 extend」「28 个绿 UFO → 9 颗 Bomb」——
  与「红给残机碎片（`collect_life_fragments` `0x422d30`：碎片 >4 时 +1 命）」「绿给 Bomb 碎片
  （`collect_bomb_fragment` `0x422e10`：每 +2，>4 时 +1 颗）」逐条吻合。

## 9. 令牌道具与 HUD / anm ✅

**道具类型 ↔ 颜色 ↔ anm 脚本**（`ItemManager__spawn_item` `0x4273f0`：类型 10..15 走
`ItemManager.token_items[0x10]` 专用数组并 `ufos_spawned_since_stage_begin++`；
道具自身 anm 脚本 = **类型 + `0xa5`(165)**，读点在 `0x425c86`）：

| 道具类型 | 颜色 | bullet.anm 脚本 | 来源 |
| --- | --- | --- | --- |
| 10 / 11 / 12 | 红 / 蓝 / 绿（**固定色**） | 175 / 176 / 177 | 敌人 `dropExtra(10..12, n)` |
| 13 / 14 / 15 | 红 / 蓝 / 绿（**彩虹令牌的三种状态**） | 178 / 179 / 180 | UFO 击破奖励 |

- **彩虹令牌换色** ✅（`ItemManager__on_tick__body` `0x4262f5`–`0x4264b6`）：类型 13/14/15 时
  `item+0x9a0` 每帧 +1；>149（`0x95`）时 `type = ((type − 0xc) % 3) + 0xd`（13→14→15→13，
  `0x4264b6`），并重建 VM 与音效；**玩家在 60 px 内（距离² `< 3600`）时该计时器暂停**，
  90 帧（`0x5a`）起开始闪烁。与 Wiki「约 150F 换色、靠近暂停」✅ 一致。
- **令牌收集 `FUN_004270b0` `0x4270b0`** ✅：类型 10/13 → 颜色 0、11/14 → 1、12/15 → 2
  （调用点 `0x426c3f`/`0x426c4d`/`0x426c5e` 分别 `MOV EAX,0/1/2`）；先建 bullet.anm 脚本
  `类型+0xc9`(211..216) 的「收集爆环」VM（`interruptLabel 7/8/9/10` = 红/蓝/绿/彩虹），
  再调 `0x422e80` 记账，最后 `FUN_004214b0` 刷新 HUD。`__UFO_COUNT__COPY_18 >= 3` 时令牌
  只给 `MAXIMUM_PIV` 上限内的 +100000 PIV，不再召唤。

**HUD（`front.anm`，脚本号用 v4.anmm 命名）**：

| 脚本 | 用途 | 关键 opcode / 数值 |
| --- | --- | --- |
| `0x4f`(79) | 「UFO 出现」横幅 | sprite67（96×16），`pos(-96,467)` → `posTime(60,4, 32,467)` |
| `0x50`(80) | 令牌槽（3 个，`gui+0x54b8[1..3]`） | sprite59..62；`interruptLabel 10/11/12/13` = 空/色1/色2/色3；7/8/9 = 槽位 x=52/73/94 |
| `0x51`(81) | 收集时的槽位闪光 | 同上标签，`scaleTime(8,4,1.5,1.5)` |
| `0x52`(82) | UFO 出现大横幅（`front/front02.png` sprite68，304×32） | `interruptLabel 7/8/9/10` = 红/蓝/绿/彩虹染色 |
| `0x86`(134) | 血条 VM 之一：sprite102（`front/front_ufo.png`，96×96，`pos(528,288)`），子脚本 135 用 sprite103 | `0x44a8a0` 建 VM |
| `0x88`(136) | 血条 VM 之二：`sprite(-1)`、`color(255,96,96)`；**引擎每帧写它的 `scale.x = life.cur/max`** | `0x44a5d0` |
| bullet `0xcf`(207) | UFO 光环（layer 0x17） | `scriptNew(204/205/206)`；204/205 = 旋转多边形环，206 = 旋转条（`sprite404`，14×128） |
| bullet `0xd0`(208) | 击破特效 | `drawPoly(1.0,32)`、`scaleTime(40,4,192,192)`，子脚本 209 |
| bullet `0xd3`(211) | 收集令牌的爆环 | `drawPolyBorder(1.0,5)`、`angleVel(0,0,0.0349)`、标签 7/8/9/10 |

- **★ 填充环**：`Ufo::on_tick` `0x44a47b`–`0x44a4a9` 在 bullet 脚本 207 的子 VM 里找
  `script_number == 0xce`(206) 的那个，每帧写 `flags_lo |= 4` 与
  `rotation.x = this->fill × 2π`（常量 `0x4a3cd0` = 2π）——**光环的旋转角就是吸收进度**。
- **HUD 计数刷新**：`FUN_004214b0` `0x4214b0` 把 3 个槽的 `pending_interrupt` 写成
  `COLLECTED_UFOS[i] + 10`，并（颜色 ≥ 0 时）另建一个 `0x51` VM 标出当前颜色；
  `FUN_00421690` `0x421690` 按 `(timer % 16 == 1 ? 1 : 0)` 在 sprite 组 **59..62 / 63..66**
  之间切换（UFO 在场时令牌槽闪烁）。

## 10. `on_draw_3c` / `on_draw_16` 各画什么 ✅

| 函数 | 画什么 | 证据 |
| --- | --- | --- |
| `Ufo__on_draw_3c` `0x44a4f0` | UFO 在场且 `timer ≥ 20` 时用 `AsciiManager__printf` `0x4020a0` 打三行：`cntA`(`%3d`)、`cntB`(`%3d`)、`fill×100`(`%3d%%`)；起点常量 `0x4a4100`=512.0、`0x4a4178`=260.0，行距 `0x4a4170`=20、`0x4a3e78`=30 | `0x44a4f0` 反汇编 + 常量字节 |
| `Ufo__on_draw_16` `0x44a5d0` | ① 击破弹字：`scale 2.0`、`font 4`、`printf("*%1.1f", +0x60)` + 逗号分数（`FUN_004021a0` `0x4021a0`），剩余帧 `+0x64` 按 `%10` 闪色；② 把血条 VM（`+0x44`）的 `scale.x` 写成 `life.current/life.maximum` | `0x44a5d0` 反编译、`0x4021a0` |

> 🟡 两条血条 VM 的**视觉分工**未验：`0x86`(134) 有 sprite、`0x88`(136) 是 `sprite(-1)` 却被写
> `scale.x`；到底哪块是外框、哪块是填充，需要实机截图确认（§12 未答 6 同类）。
>
> 🟡 `on_draw_3c` 那三行是**调试读数**（TH12 里玩家未必看得见）：位置在 640×480 屏幕的
> x=512、y=260 附近，实测截图未做。函数无 debug 开关判断，故 v1.00b 里每帧都会调用。

## 11. 一句话模型（地址串）

```
敌人死 → dropExtra(10/11/12, n) 或 dropMain(...)  (ECL opcode 407/410, handler 0x4179cc / 0x417a38)
   → ItemManager__spawn_item 0x4273f0（类型 10..15 进 token_items[]）
   → 自机吃到 → ItemManager__on_tick__body 0x425c20 的 case 10/13|11/14|12/15
   → FUN_004270b0 0x4270b0 → FUN_00422e80 0x422e80 记 3 个槽
   → 三同色/三异色 → GLOBALS.__field_24 0x4b0c64 + __UFO_COUNT__COPY_1C 0x4b0c5c = 3
   → Ufo__on_tick 0x44a2d0 检测到 → FUN_0044a8a0 0x44a8a0 生成 ECL 敌机 UFO_Red/Blue/Green/Rainbow
   → 敌机每帧把 int_vars[0] 写成 999/-999（0x44a3af）驱动 default.ecl 的状态机
   → 类型 1/2 的道具被吸走（0x4271c0 + 0x44aca0），fill(+0x50) 涨；光环旋转 = fill（0x44a499）
   → 击破（Enemy__die 0x414c40 → 回调 0x44a890）→ FUN_0044af00 0x44af00 吐赏金道具 16/17/18
     + 红给残机碎片 4 / 绿给 Bomb 碎片 5 / 彩虹给彩虹令牌 13-15
```

## 12. 未答清单（❓/🟡）

1. 🟡 `flagSet(1090)`（`flags |= 0x442`，位 1/6/10）在 v1.00b 里**没有消费者**；位 1/6/10 的真意未定。
2. 🟡 `this+0x28 = 720` 与 `−300` 的关系：ECL 在 `timer == 420` 离场，但 720 本身没有第二个用途被找到。
3. 🟡 `enemy.life.is_spell |= 2`（`0x44a3ab`，对话中）——TH12 里 `is_spell` 是否其实是位场，未验。
4. ❓ 音效 id `0x34`（居中）与 `0x36`（带 pan、循环）：`0x4aea50` 的 se 名表只覆盖 0..0x30，
   这两个 id 的名字没查到；`0x36` 由 `FUN_00453f10` `0x453f10` 停掉。
5. 🟡 蓝 UFO「吸满不吐东西」是设计还是遗漏（`0x44ad99` 的 switch 无 case 2）。
6. 🟡 `on_draw_3c` 的三行调试读数在实机上是否可见、两条血条 VM（`0x86`/`0x88`）的视觉分工
   （需实机/截图）。
7. ❓ `ItemManager.ufos_spawned_since_stage_begin`（`+0x666fe0`）的消费者没找到。
8. ❓ 敌人 ECL 里令牌掉落只出现在 `dropExtra(10,1)`（stage01）与 `dropExtra(11,1)`（stage02）；
   大量令牌实际来自「UFO 向日葵」一类敌人的**别的**掉落路径（本文未逐关普查 ECL 子程序）。

## 13. ExpHP 符号对账（🟡 → ✅ / 证伪）

`local/vendor/th-re-data/data/th12.v1.00b/` 里与 UFO 有关的符号共 10 条，逐条一手坐实：

| ExpHP 名 | 地址 | 对账 |
| --- | --- | --- |
| `Ufo::initialize` | `0x449fa0` | ✅ 一手（三次 `malloc(0x24)` + 注册 on_tick/on_draw×2 + `zTimer` 初始化） |
| `Ufo::destructor` | `0x44a120` | ✅ 一手（注销三个句柄 + `UFO_PTR = 0` + 删光环 VM） |
| `Ufo::operator new` | `0x44a230` | ✅ 一手（`malloc(0x6c)`，与 zUfo 大小一致） |
| `Ufo::on_tick` | `0x44a2d0` | ✅ 一手（召唤 / 驱动 ECL 变量 / 同步光环与血条） |
| `Ufo::on_draw_3c` | `0x44a4f0` | ✅ 一手（三行 ASCII 读数） |
| `Ufo::on_draw_16` | `0x44a5d0` | ✅ 一手（击破弹字 + 血条 scale） |
| `Ufo::on_tick__stub` 等 3 个 `__stub` | `0x44a860`/`0x44a870`/`0x44a880` | ✅ 一手（各只有一条尾调用） |
| `UFO_PTR`（statics） | `0x4b4534` | ✅ 一手；注释「收 3 令牌召唤 + tick」**完全正确** |
| `zUfo`（结构体） | — | ⚠️ ExpHP 只填了 `0x0 __unknown` / `0x6c __end`，**等于没填**；本文 §4 补出 22 个字段 |
| `__UFO_COUNT__COPY_18/1C`、`__field_20/24` | `0x4b0c58`…`0x4b0c64` | 🟡 前两个命名基本对（确是计数）；后两个本文定名为「每帧自增计数器」「召唤颜色」 |

- **证伪/纠正**：`Ufo::on_draw_3c` / `on_draw_16` 的 `_3c`/`_16` 后缀**不是层号、也不是 anm 脚本号**
  ——两者是 `UpdateFuncRegistry` 的 on_draw 句柄（`zUpdateFunc.priority = 0`，`0x449fa0`）。
  🟡 疑为 ExpHP 自己的区分约定，不要当成地址或层号用。
- **`UFO_PTR` 的注释**「Doesn't seem to be responsible for updating the tokens」——✅ **正确**：
  令牌计数确实在 `FUN_00422e80`（由道具侧调用），不在 `Ufo` 对象里。

## 14. 可信度与证据

- ✅ **一手**：§2 全局字段、§3 `0x422e80`、§4 zUfo 字段、§5 `0x44a8a0` + HP 表、§6 ECL 全 4 个
  子程序与 `moveLimit`/`flagSet` 的 handler、§7 `0x4271c0`/`0x44aca0` + 阈值表、§8 `0x44af00`、
  §9 道具/脚本号/换色、§10 两个 draw 函数。
- 🟡 **推断/未定**：§6 的设计意图读法、§10 调试读数可见性、§12 全部条目。
- 复核入口：Ghidra 工程 `local/th12.v1.00b/ghidra_projects/th12.exe.gpr`（程序名 `/th12.exe`），或
  `python3 tooling/ghidra/scripts/decompile_funcs.py th12 0x44a2d0 0x44a8a0 0x44aca0 0x44af00 0x422e80`。
- 数据侧复核：`thdat -x 12 -C <dir> local/th12.v1.00b/th12.dat`；
  `thecl -d 12 -m local/vendor/eclmap/eclmap/th12.eclm default.ecl`；
  `thanm -l 12 -m local/vendor/thpages/static/mapfile/v4.anmm front.anm`。
- 交叉：社区 [`東方Project攻略 Wiki 星/Data`](https://wikiwiki.jp/thk/%E6%98%9F/Data)（填充阈值 34/36/39/42/46/51/56、
  彩虹令牌 150F 换色/靠近暂停、红→残机碎片、绿→Bomb 碎片）。
- 相邻子系统：[`ecl/OVERVIEW.md`](../../ecl/OVERVIEW.md)（ECL VM）、
  [`_shared/community-sources.md`](../../_shared/community-sources.md)。
