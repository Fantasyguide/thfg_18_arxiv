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
EffectManager__spawn_effect(&vm, 2, &spawn);     // ★★ 0x44a947 第 5 处创建点：召唤特效（紫圆+蓝闪），见 §10d
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
| **召唤特效**（第 5 处创建点）| `EffectManager` **effect #2** = 紫圆(32 边实心、`0xFF00FF`、半径 128→−1.5/帧、50F) + 每帧 2 条蓝闪(1px LINESTRIP)。**不是 anm** | `0x44a947`、效果表 `0x4af160`，见 §10d |

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
| `UFO_EtBreak2` | `etCancel(%B)`，`%B` 从 16 每帧 **+16** 到 640 | 击破时的大消弹（半径 16→640） |

> **2026-09-15 订正**：`UFO_EtBreak2` 的步长原写作 +6，**错**。`dat/default.ecl` 反编译原文
> （`thecl -d 12 -m eclmap/th12.eclm default.ecl`）：`UFO_EtBreak` 用 `+1: //1 / 6`、比较 `< 112.0f`；
> **`UFO_EtBreak2` 用 `+1: //1 / 16`、比较 `< 640.0f`** ⇒ 步长是 **+16**。两处一字之差，逐条对过脚本原文。

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
- **上钩那一刻它做了什么（2026-09-24 复核补）** ✅：`0x4271ec`–`0x427204` 三条 `FLDZ`/`FST`
  把道具的**速度向量 `+0x978/+0x97c` 与吸引距离 `+0x9bc` 全部清零** ⇒ 道具会**当场停住**，
  之后速度从 `0.02/帧` 起爬 —— 也就是说「先愣一下再慢慢加速」**就是一手行为**（不是 bug）。
  ⚠ 那个 `fldz` 也解释了为什么它看着"定住"：第 1 帧的位移只有 0.02 单位（≈0.04 表面 px）。
- **上钩的门（逐条）** ✅：`0x4271c0` 的跳表以 `zUfo+0x30`（**UFO 颜色**）索引：
  `colour + 1 == 1` ⇔ `colour == 0` ⇒ **直接返回 0**（没召唤时颜色是 0 ⇒ 这一格是
  "UFO 真的存在吗"的守卫，不是某个颜色不吸）；再加 `UFO.timer >= 60`（`0x4271c6 cmp [ecx+0x14],0x3c`）
  与 `item+0x9b4 ∈ {1,2}`（P / 点道具）⇒ 三者同时成立才置 state 7。
  **没有任何半径条件**（全场 P/点道具一起上钩）。
- **退出这条 mode** ✅：`0x42687f cmp [ecx+0x2c],0`（UFO 还在吗）⇒ 不在时速度清零并**落回 state 1**
  （`0x4260a5`，正常飘落）；捕获那一下是 `state := 0` + **SE `0x26`**（`0x426970 mov eax,0x26`）。
  另外被吸的道具**一路都还能被自机收走** —— state 7 的尾巴仍然走公共收点判定（`0x4260af` 起）。
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

### 7b. 吸收的**类型分组表**与"有没有 ×2"（2026-09-26 逐字节复核，回答 owner 的提问）✅

`0x44aca0` 起是吸收本体，结构 = **两张表**：

| 表 | 地址 | 内容 | 读法 |
| --- | --- | --- | --- |
| 类型 → 组（**byte** 表，12 项 = type 1..12）| `0x44aeb0` | `[0,1,3,3,3,3,3,3,3,2,2,2]` | `movzbl 0x44aeb0(%eax),%eax`（`0x44acba`）|
| 组 → 分支（dword，4 项）| `0x44aea0` | `{0x44acc8, 0x44ad0c, 0x44ad39, 0x44ad63}` | `jmp *0x44aea0(,%eax,4)`（`0x44acc1`）|

| 组 | 类型 | 计数 | 填充 |
| --- | --- | --- | --- |
| 0 | **1（P）** | `cntA(+0x48) += 1`；彩虹（`color == -1`）⇒ `cntB(+0x4c) += 1` | 按**颜色**查 `0x44aebc` → `0x44acf5`：`fill += 0.032 − 0.002×stage`（`0x4a3fa0`/`0x4a4350`，double）|
| 1 | **2（点/金钱）** | `cntB += 1`；彩虹 ⇒ `cntA += 1`（"反过来"）| 按颜色查 `0x44aed0` → **同一个** `0x44acf5` ⇒ **同一条式子** |
| 2 | **10/11/12（令牌）** | **不计数** | 按颜色查 `0x44aee4` → `0x44ad57`：`fill += 常数`（`0x4a3e28`，double，**与关卡无关**）|
| 3 | **3..9** | **不计数** | **不加** `fill`（直接跳到尾 `0x44ad63`）|

- 两张"颜色 → 分支"表都按 **`color+1`** 索引；索引 1（= color 0 hmm）落 `0x44ad63`（跳过）。
- ★ **吸收段里没有任何 ×2**：计数恒 `add %edi,…` 且 `mov $1,%edi`（`0x44acac`）；P 与金钱的
  **fill 式子逐字节相同** ⇒ 「收满」这件事在 P / 金钱之间**没有差别**。
- **唯一的按色不对称在赏金结算**（同名函数 `0x426cd2` vs `0x426c9a`/`0x426d27`）：
  **红** 乘的是 **`cntA`(P)**，其余色乘的是 **`cntB`(金钱)** ⇒ owner 记的"P 对红 UFO 是 ×2"
  来源是这条（`mult` 恰为 2 时才等于 ×2，见 `ufo_drop_settle_value()`）。
- 反向印证：**只有 type 1/2/10/11/12 会被吸收**（3..9 什么都不发生）—— 与本卡 `ufo_absorb_type_index()`
  的"1 → P、2 → 金钱、其它 −1"白名单**一致** ✓。

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

### 8b. ★★★ 击破奖励的**按色倍率 N** 与赏金公式（2026-09-26 逐字节，回答 owner 的四色口径）✅

`0x44af00` 的每个颜色分支都会**先算一个 float N 写进 `[ufo+0x9d4]`**，再读 **PIV**（`0x4b0c78`）算赏金值
写进 `[ufo+0x68]`（= 赏金道具携带的分）。**赏金值的两条形状**（`PV = PIV/100` 向下取整到 10 的倍数）：

| 颜色 | 值 | 出处 |
| --- | --- | --- |
| 红 | `trunc(cntA × PV × N) + cntB × PV` | `0x44b1d0`–`0x44b1f3`（`imul 0x48(%esi)` = cntA、`imul %ebx,%edx` + `0x4c(%esi)` = cntB）|
| 蓝 / 绿 / 彩虹 | `trunc(cntB × PV × N)`（**只有 money 通道**）| `0x44b2f4`/`0x44b3ad`/`0x44b562`（`imul 0x4c(%esi),%ecx`）|

**N 的算法（逐分支）**：

| 颜色 | N | 一手指令 | 常量 |
| --- | --- | --- | --- |
| **红** | `power >= max_power` ? **2.0** : **1.0** | `0x44b149 mov 0x4b0c48,%eax` / `cmp 0x4b0cd0,%eax` / `jl` / `flds 0x4a4014` : `fld1` | `0x4a4014` = **2.0f**；`0x4b0c48` = 当前火力、`0x4b0cd0` = 最大火力 |
| **绿** | **2.0**（恒）| `0x44b33f flds 0x4a4014` | 同上 |
| **蓝** | `fill == 1.0` ⇒ **8.0**；`fill >= 0.5` ⇒ **6.0**；否则 **2.0 + 4.0×fill** | `0x44b252`–`0x44b28a`：`fldl 0x4a4078`(=8.0) / `flds 0x4a4088`(0.5) `fcomps 0x50(%esi)` / `fldl 0x4a43c0`(=6.0) / `flds 0x50(%esi)`×`fmull 0x4a4108`(=4.0)`faddp 0x4a3d00`(=2.0) | `fill` = `[ufo+0x50]` |
| **彩虹** | ⏳ 待读（owner 口径：常规 2~3、喂饱 4）| 该分支的 N 写在读 PIV 之前 | — |

⇒ 与 owner 给的四色口径**逐条对上**：「红未满P = 相当于蓝点分（N=1）、满P = 2 倍（N=2）」、「绿 ×2」、
「蓝 ×2~6 与道具数有关（= `2+4×fill`，fill 就是吸收进度）」、「蓝喂饱 ×8（fill 正好 = 1.0 那一档）」✅。
⇒ **"蓝点分数"= PIV = 最大得点**（`PV`），owner 的预计正确 ✅；"×N"= 赏金对该通道的**倍率**，
不是给玩家挂一个全局倍率。

**对卡侧的两个含义**：
1. 我们的 `mult_permille` 不能只在吸收时累加 —— 一手是**在击破那一刻**按 `(颜色, fill, 是否满P)` 算出来的；
2. 赏金值也应**在击破那一刻**定下来（一手写进 `[ufo+0x68]` 随道具携带），而不是收到时才按当时的 PIV 算。

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

> ★ **2026-09-22 追加（三件一手，逐指令 + 数据直读）** —— 全文见下面 §10b：
> ① 这些 HUD/世界脚本的坐标所在的是 **anm 世界空间**，那套空间里**场地占 x∈[32,416] / y∈[16,464]**
> （不是屏幕空间；屏幕空间只在 `0x18f80` 一族 ASCII 与层 23/24 的引擎 VM 上用）；
> ② UFO 光环的 `entity_pos` = **敌人 + (32+192, 16)**，其中 192/16 是**空间换算项**
> ⇒ 环与敌人**同心**（残差 0）；③ **击破计时器** = `on_draw_16` 里的一条 ASCII 倒计时
> （`"%2d:%.2d"`、`remaining = 720 − 存活帧`、画在 UFO 正下方 20 单位）。

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

## 10b. ★★ 三件补充一手（2026-09-22，P3 工单：槽位 UI / 光环落点 / 击破计时器）✅

> 方法：`objdump -d --start-address=… -M intel local/th12.v1.00b/th12.exe` 逐指令 +
> `thanm -l 12 -m local/vendor/thpages/static/mapfile/v4.anmm local/th12.v1.00b/dat/front.anm`
> 逐脚本 + 常量**按 PE 节映射直读**（`.rdata` 文件偏移 = VA − `0x498000` + `0x97000`，8 字节 double）。
> 可信度：三条都 ✅ 一手（各带 2–3 处互相印证的落脚点）。

### (a) 那套 anm 世界空间里**场地占 x∈[32,416] / y∈[16,464]** ✅

| # | 一手 | 推出什么 |
| --- | --- | --- |
| 1 | `front.anm` script82（UFO 大横幅，`sprite68` 304×32）`pos(224.0f, 208.0f)` | 224 = 32 + 384/2 = **场地中心 x**；208 = 16 + 192（场地内上半）⇒ 空间 = 场地相对 + (32, 16) |
| 2 | script79（小横幅 96×16）`pos(-96,467)` → `posTime(60,4, 32,467)`：终点 x=32 = **场地左沿**、y=467 = 场地**下沿（464）下 3 px** | 同上 |
| 3 | script80（令牌槽）`pos(52/73/94, 456)` 三个 posTime 目标 | 场地相对 = 52/73/94 − 32 = **20/41/62**、456 − 16 = **440**（槽高 16 ⇒ 底边与场地下沿齐平）|
| 4 | `Ufo::on_tick` `0x44a42a`：`fld [enemy+0x1074]`（敌人 x，**以场地中心为原点**）→ `fadd [0x4a3d70]`=**32.0** → `fadd [0x4a3d28]`=**192.0**；y 只 `fadd [0x4a3d68]`=**16.0** | 换算敌人坐标到那套空间 = **+192+32 / +16** ⇒ 这两项是**换算**，不是偏移 |

⇒ 结论：**该空间里的坐标 = 场地相对坐标 + (32, 16)**（等价说法：0 在场地左沿往左 32 px、上沿往上 16 px）。
把这套数字搬到别的引擎时，**必须先减掉 (32,16) 再折到目标引擎的场地坐标**；直接当屏幕 px 用会把
HUD 画到场地外（我们 112 的槽就踩过：见 `mods/th18.v1.00a/card-expand/AUDIT.md` §BE 详录 #52）。

### (b) UFO 光环的落点 = 敌人身上（残差 0）✅

- **发现**：`Ufo::summon` `0x44aa8e`–`0x44aab4` 与 `Ufo::on_tick` `0x44a42a`–`0x44a45f`
  **逐条同形**地写 `vm+0x430`（= 那个 VM 的实体坐标，三格 float3）：
  `x = enemy.x + 32.0 + 192.0`、`y = enemy.y + 16.0`、`z = enemy.z`。
- **推测**：这三项就是 (a) 里的换算 ⇒ 环画在敌人身上。
- **验证**（三处独立）：① `bullet.anm` script207 自身**没有 `pos()`**（只有 `alpha(0)` +
  `scriptNew(204/205/206)` + `stop()`），三个子脚本也都无 `pos()` ⇒ 绘制点 = 父的实体坐标；
  ② 同一个敌人的**血条 VM** 由 `0x44a83e`–`0x44a849` 调 `0x461e30(&zUfo+0x44, &enemy.x)` 摆位
  （同一套换算），其脚本 `front.anm` script136 写 `pos(0,-28)` ⇒ 条在敌人上方 28 单位（合理）；
  ③ `th12:0x44a43f`–`0x44a447` 里那两项加法在同一段里紧跟敌人坐标读取。
- **结论** ✅：**环与 UFO 同心**（`(32+192, 16)` 全是空间换算项）。
- **证据**：上面四个地址段 + 常量 double 值 32.0 / 192.0 / 16.0。

### (c) 击破计时器 = `on_draw_16` 里的 ASCII 倒计时（不是条）✅

```
44a6e3  mov ecx,[ebx+0x38]        ; zUfo+0x38 = 场上 UFO 的 enemy_id（0 ⇒ 不画）
44a6ee  cmp [ebx+0x14],0x3c       ; zUfo+0x14 = 存活帧数；< 60 帧不画
44a782  mov eax,[ebx+0x28]        ; zUfo+0x28 = 存活上限 720（0x2d0）
44a78d  sub eax,[ebx+0x14]        ; remaining = 720 − 存活帧数
44a7b0  fdiv [0x4a3d20]           ; ÷ 60.0            → 秒
44a7c4  fmul [0x4a3ce8]           ; × 100.0 → (int) → % 100 = 百分秒
44a7da  push 0x4a215c             ; "%2d:%.2d"
44a7e3  call 0x4020a0             ; AsciiManager__printf
```

| 项 | 一手值 | 出处 |
| --- | --- | --- |
| 上限 | **720 帧**（= `zUfo+0x28`）| `0x44a8a0`、`0x44a782` |
| 显示形态 | **ASCII 计时**，格式 `"%2d:%.2d"` = **秒 : 百分秒**（形如 `12:00`、`11:91`、` 7:00`）| `0x4a215c` 字符串直读 |
| 颜色 | `0xc080ffc0`（A=0xc0 / R=0x80 / G=0xff / B=0xc0，淡绿）| `0x44a778` |
| 字号 | **1.0**（`fld1` → `[AsciiMgr+0x18f84/+0x18f88]`）；字符集 `[+0x18f98] = 2` | `0x44a66e`–`0x44a682` |
| 位置 | 敌人的**屏幕坐标** + x **−12.0**（5 字符手工居中）、y **+20.0**（= 行距常量）| `0x44a748`–`0x44a7a2`、`0x4a4128`=12.0、`0x4a4170`=20.0 |
| 出现/消失 | 只受「UFO 在场 + 存活帧 ≥ 60」控制（**不是**吸收进度，也不在 HUD 槽上）| 同上 |
| 时间到的行为 | 计时器**不驱动任何逻辑**：离场由 ECL 的状态机管（存活 420 帧起飞走，见 §6）⇒ 720 只是**显示上限** | `0x44a3af` 段、§6 |

- **结论** ✅（TH12 v1.00b）：原作**没有**「击破进度条」这类 UI；玩家看到的"计时"就是这条
  720 帧倒计时（UFO 从召唤起最多待 12 秒、ECL 在 7 秒（420 帧）开始让它飞走）。
- ⚠ 与 §10 那条 🟡 的关系：`on_draw_16` **共画两样** —— 击破弹字（`*%1.1f` + 分数）与
  **这条倒计时**；§10 表里原先只记了弹字与血条 `scale.x`，本条补上第三样。
- **证据**：`0x44a6e3`–`0x44a7e8` 全段反汇编 + `"%2d:%.2d"`（`0x4a215c`）/ `0xc080ffc0`（`0x44a778`）
  两个立即数 + 三个 double 常量（60.0 / 100.0 / 20.0 / 12.0）。

## 10c. ★ 召唤特效的 anm 侧：哪一行代码 → 哪份 anm → 哪支脚本画什么 ✅（2026-09-26）

> **方法**：`objdump -d` 直读 `0x44a8a0` 里四条「造 VM」调用（anm 指针来自哪个全局 + 推入的脚本号），
> 再把 `dat/{front,bullet,enemy}.anm` 用 `thanm -l 12 -m v4.anmm` 逐脚本读原文。
> **可信度**：地址链 ✅ 一手；脚本体 ✅ 一手（`.dat` 原文）。
> **本节存在的理由**：把「召唤的 134」当成 `enemy.anm` 134 会让整支特效全错 —— 槽位属于 `front.anm`。

### (a) 创建点 → 文件 → 脚本 → 画什么

| 创建点 | 文件 | 脚本 | 脚本原文要点 |
| --- | --- | --- | --- |
| `0x44a953` | `front.anm` | **82**(`0x52`) | 横幅：`sprite(68)`(304×32)、`layer 6`、`type 2`、`pos(224,208)`、`alpha 0→128`/20F、`+30 scaleTime(30,2,2,2)`+淡出、`+5/+10/+15 scriptNew(84)`、`+30 delete()`；`interruptLabel 7..10` = 四色 |
| 同上 | `front.anm` | 83 / 84 | 碎笔触：83 = `sprite(69)` 由 x=88 扫到 360(`scrollX(0.0297)`)；84 = `sprite(68)` 放大 2× 淡出 |
| `0x44aabe` | `bullet.anm` | **207**(`0xcf`) | 只三件事：`alpha(0)`、`scriptNew(204)`、`scriptNew(205)`、`scriptNew(206)`、`stop()`；`interruptLabel 1` → `+20 delete()` |
| 子 | `bullet.anm` | 204 / 205 | **常驻光环**（⚠ 不是"召唤瞬间那个缩小的紫圆"——那是 §10d 的 C 侧 effect #2）：`drawPolyBorder(62,48)`/`(55,48)` + `blendMode(1)` + `color(96,96,224)` + `alphaTime(60,0,255)`——**只渐入、半径固定、不缩**；`interruptLabel 2` = 吸收时七彩闪 |
| 子 | `bullet.anm` | 206 | 旋转光环：`iset($I0,32)`+`iset($I1,2)`+`ins_101($I0)`(texCircle 家族) + `sprite(404)`、`scale(6,59)`、`scrollY(0.025)`、`rotate(π,0,π/2)`；角度由 C 代码逐帧覆写（见 (b)） |
| `0x44ab01` | `front.anm` | **134**(`0x86`) | 碟体贴图：`sprite(102)`(96×96，`front/front_ufo.png`)、`layer 23`、`resampleMode(1)`、`scale(0,0)→scaleTime(20,0,1,1)`、`+60 alphaTime(20,0,255)`；`interruptLabel 1` → `scaleTime(20,0,0,0)` `+20 delete()` |
| 同上 | `front.anm` | 135 | `sprite(103)`(96×96 第二帧) 同节奏，`layer 24` |
| `0x44ab2c` | `front.anm` | **136**(`0x88`) | 血槽底：`sprite(-1)`+`drawRect(32,4)`、`layer 10`、`color(255,96,96)`、`pos(0,-28)` |
| 同上 | `front.anm` | 137 | 血槽黑边：`drawRect(32,4)`、`layer 9`、`color(0,0,0)`、`pos(1,1)` |

⇒ 一手的召唤画面 = **横幅(front 82/83/84) + 常驻光环(bullet 204/205/206) + 碟体(front 134/135) + 血槽(front 136/137)**
**+ C 侧程序化特效（§10d：紫圆 + 蓝闪，这两样不在 anm 里）**。
**anm 这四处没有一处是贴图粒子爆闪**：204/205/206 全是几何体（`drawPolyBorder` / texCircle 家族）。

> ⚠ 上表压掉的两处 204/205 细节写在这里：`interruptLabel 2` 的循环色 = `(255,255,0)` → `(255,0,0)` →
> `(0,255,0)` → `(0,255,255)`（每 2 帧换一个 = 吸收时的七彩闪）；`interruptLabel 1` = `alphaTime(20,0,0)`
> 再 `+20 delete()`（UFO 离场时收掉）。

### (b) 206 的角度由 C 代码逐帧写 ✅

`Ufo__on_tick` `0x44a476`–`0x44a4b8`：在世界 VM 链里按 `vm+0x3ea == 0xce`(=206) 找到 VM，写
`vm.flags |= 4` 与 `vm+0x24 = ufo.field_0x50 × 2π` ⇒ 206 的旋转 = UFO 自身角度。

### (c) ⚠ 订正：`enemy.anm` 139/140/141 在 v1.00b 里**找不到调用者**

139/140 = `drawPoly(192,3)` + `color(0,0,0)` + `color2(255,128,255)` + `scaleTime(60,5,0,0)`（大→小）
+ `alpha2Time(60,0,255)`（透明→实）；141 = 造 3 对 139/140。**"缩小 + 变实"的节奏看着像**，但形状是 3 边（三角），且：

- 全 exe 没有把 139/140/141 当脚本号传给 anm 的地方（`push $0x8d` / `$0x8b` / `$0x8c` 均不存在）。
- 8 关 ECL + `default.ecl` 用 `thecl -d 12 -m th12.eclm` 全解：`anmPlay(0,N)` 最大 N = **130**，
  `anmSetSprite(0,N)` 只到 158，**139/140/141 一次都没出现**。

⇒ 这三个脚本在 v1.00b 属**孤立数据**（🟡 疑旧 revision 残留）。**据此，把从 139/140 抄来的实现标成
「复刻」是不成立的**，必须改标「自定」。
★ **2026-09-26 二次订正**：一手的那道"缩小的紫圆"**既不是 139/140/141，也不是本节的 204/205** ——
它根本不在 anm 里，是 C 侧的 `EffectManager` effect #2（见 **§10d**）。204/205 是同一次召唤里**另一层**
常驻光环（只渐入、不缩）。

> **2026-09-26 补验（owner 的实机口径）**：owner 手上的 `[th12] 东方星莲船 (汉化版+日文版)` 与本文样本**逐字节同一份**
> —— `th12.exe` md5 `212f187591f4f1fa99eb2845d1b3cf20` 相同，`enemy.anm`/`bullet.anm`/`front.anm`/`default.ecl`/`stage01.ecl`
> 五份 md5 也全同 ⇒ **本节结论就是 owner 实机看到的版本的结论**（不存在"他玩的是另一版"这条路）。
> 唯一的补丁：`anmPlay(1, 141)` 确实存在——stage05/06 的 `Cir00`（琪露诺）出生串
> （`flagSet(3); playSound(40); anmSelect(1); anmPlay(1, 141); wait(40); anmSetSprite(0, $A)`）。
> 但**索引 1 = `stgenmNN.anm`**（ECL 头 `anim { "enemy.anm"; "stgenmNN.anm"; }` 的第二项），
> 而 `stgenm05` 只有 68 支脚本、`stgenm06` 只有 83 支 ⇒ **脚本 141 在那两份文件里不存在，这条调用是空放**
> （同类空放还有每关都有的 `anmPlay(1, 95)` / `anmPlay(2, 158)`）。**且它属于琪露诺，不属于飞碟召唤路径。**

> **旧 build 的坑**：本仓遗留的 `/tmp/stage01_raw.txt`（2026-09-18 另一会话）里 `BossDead` 是
> `anmPlay(0,76) / playSound(18) / anmPlay(0,141)`，看着像「141 有主」。但我们的 v1.00b `stage01.ecl`
> 从 `0x8d18` 起**逐字节**解出来是 `flagSet(140) / anmPlay(0,79) / playSound(5) / anmPlay(0,129) /
> moveVel(RANDRAD,0.4)`（opcode `0x0192 / 0x0107 / 0x01a0 / 0x0107 / 0x0130`）。旧 dump 的 `BossDead`
> 落在文件 `0x4bb0`、我们的在 `0x8d08` ⇒ **那是另一个 revision 的 stage01.ecl，不能当 v1.00b 的证据**。

### (d) 附带一手：`enemy.anm` 是被 **ECL 头**加载的 ✅

`th12.exe` 里**没有** `enemy.anm` / `stageNN.anm` 字符串（只有 ascii/bullet/front/text/pl0N/stgenmNN/stNNlogo/title 等）。
它们在 ECL 头里：`default.ecl` 开头 = `ANIM` magic + count=1 + `"enemy.anm"`；`stageNN.ecl` = count=2 +
`"enemy.anm"` + `"stgenmNN.anm"`（两串分别在文件偏移 44 / 54）。⇒ 敌人 anm **索引 0 = `enemy.anm`、
索引 1 = 本关 `stgenmNN.anm`**，正是 `anmPlay/anmSetSprite` 第一参的含义。
UFO 本体动画 = `default.ecl` 的 `anmSetSprite(0, 135..138)`（= `enemy.anm` 135..138：`rotateTime(96,9,…)`
转着播 `sprite205..220` = `enemy/ufo_anim.png`）。

## 10d. ★★★ 召唤特效的真正来源：`EffectManager` **effect #2**（紫圆）/ **#1**（蓝闪）—— **不在 anm 里** ✅（2026-09-26）

> **触发本节的实机口径（owner）**：召唤 UFO 时是「一个**一边缩小一边变实**的紫圆 + **单像素粗细**的蓝色闪电」，
> 且「`bullet/etama3.png` 里的东西不会在召唤时出现」。
> **方法**：`objdump -d` 逐指令直读 `0x44a8a0` 的**第 5 处创建点**（`0x44a947` → `0x40fbe0`）+ 效果表 `0x4af160`
> 静态直读（PE 节映射）+ 两条回调链全反汇编；符号交叉用 `th-re-data` v1.00b 表。
> **可信度**：地址链 ✅ / 常量 ✅ / 绘制原语（`DrawPrimitiveUP` 的类型）✅ —— 三层互证。

### (a) 发现：召唤函数其实是**五**处创建点，第 5 处不是 VM 而是 effect

| 创建点 | 调用 | 造出什么 |
| --- | --- | --- |
| `0x44a953` | `0x4615a0`(`front_anm`, 82) | 横幅 VM（§10c）|
| `0x44aabe` | `0x454d10`(`bullet_anm`, 207) | 常驻光环 VM（§10c）|
| `0x44ab01` | `0x4615a0`(`front_anm`, 134) | 碟体 VM |
| `0x44ab2c` | `0x4615a0`(`front_anm`, 136) | 血槽 VM |
| **`0x44a947`** | **`0x40fbe0`(index `2`)** | **召唤特效本体（紫圆 + 蓝闪）← 本节** |

- `0x40fbe0` = `EffectManager::spawn_effect(out, index, params)`：新 VM 的脚本号来自**静态表 `0x4af160`**，
  每项 `0x18` 字节 = `{int16 脚本号; (pad); 预回调; on_tick; on_draw; on_destroy; on_interrupt}`，
  后四个写进 VM 的 `+0x488/+0x48c/+0x490/+0x494`。
- 表的 VM 一律用 **EffectManager 自己的 anm** 创建：`EffectManager::initialize` `0x40f947`
  `mov ebx,0x49f69c; call 0x45fe60`，字符串 `0x49f69c` 内容 = **`"bullet.anm"`**（存在 `[mgr+0x10]`）。
- effect #2 @ `0x4af190` = `{script 0, 0x410720, 0x4107e0, 0x410840, 0x410890, 0x4108a0}`；
  **`bullet.anm` script 0 是空桩**（全文 = `alpha(0); stop();`）⇒ **画面 100% 来自 C 回调，anm 库里永远查不到**。
- 另：`Ufo::on_break` `0x44b092` 也 spawn 一条 **effect #1**（`push 0x1`）⇒ **击破时那一条蓝闪**即此。

### (b) 紫圆 = effect #2 的 `0x410720` / `0x4107e0` / `0x410840` ✅

| 阶段 | 一手指令 | 值 |
| --- | --- | --- |
| 预回调 `0x410720` | `malloc(0x2c)`（`0x46d04a`）+ `memset`（`0x477420`）| 44B 实例 = pos(12) + `zTimer`(20) + 三个字段 |
| | pos = 出生点 `+(32+192, 16)`（`0x4a3d70`/`0x4a3d28`/`0x4a3d68`）| 同一套 anm 世界空间换算（§10b(a)）|
| | `[+0x20] = 128.0f`（`0x4a3db8`）| 初始半径 **128** |
| | `[+0x28] = 0x00FF00FF` | **纯紫 (R255,G0,B255)，A 从 0 起**（写死的立即数）|
| | `[vm+0x20] = 0xd` | 层 **13**（世界层）|
| on_tick `0x4107e0` | `cmp [inst+0x10],0x32` → `return 1` | 满 **50 帧**自毁 |
| | `[+0x20] -= 1.5`（常量 `0x4a4220` = double 1.5）| 半径 **−1.5/帧** ⇒ 128 − 75 = **≈53 就消失** |
| | `add BYTE [inst+0x2b],3` | **A 每帧 +3**（0 → 150，永不到 255）|
| | `push 0x1` ×2 @ `0x410802`/`0x41080f` | **每帧 spawn 两条 effect #1（蓝闪）** |
| on_draw `0x410840` | `call 0x45d430`（= anm `drawPoly` 用的同一个绘制器）| `sides = 0x20`(32)、半径 `[+0x20]`、起始角 `0.0`、色 `[+0x28]` |

- **④ 结论** ✅（TH12 v1.00b）：**32 边实心多边形、纯紫 `(255,0,255)`、半径 128 每帧 −1.5、
  alpha 每帧 +3、存活 50 帧**。
- **与 owner 口径逐条对上**：「一边缩小一边变实」= 半径递减 + alpha 递增 ✅；
  「**有 r_min**」= 50 帧 × 1.5 = 75 ⇒ **半径停在 ≈53（53/128 ≈ 0.41）随实例一起消失，永远缩不到 0** ✅；
  「紫色」= `0xFF00FF` 立即数（anm 库里任何一支脚本都没有这个颜色）。
- 🟡 `[+0x24] = 0x40FF0000`(=7.96875f) 写了但**两条回调都不读**（`0x41085f` 载入 eax 后立刻被 `push 0x20`
  覆盖）⇒ 残留字段。

### (c) 蓝闪 = effect #1 的 `0x4102b0` / `0x4103f0` / `0x410590` ✅

| 阶段 | 一手指令 | 值 |
| --- | --- | --- |
| 预回调 `0x4102b0` | `malloc(0xd8)` | 216B = **16 顶点×8B**(`+0x00..0x7f`) + **16 顶点色×4B**(`+0x80..0xbf`) + 当前角(`+0xc0`) + `zTimer`(`+0xc4..`) |
| | 基色 `0xff0040ff` 铺 16 项 | **A255 R0 G64 B255 = 蓝**；逐项改字节（前 8 项 `R` 累计 −0x20、后 8 项 `A` 累计 −0x10，**uint8 回绕** 🟡 别当干净渐变）|
| | 初始角 = `(rand·2⁻³¹ − 1)·π` | ∈ **[−π, 0)**（`0x4a3cd8` 是 **float π 被当 double 读** —— ZUN 混合精度）|
| on_tick `0x4103f0` | `cmp [inst+0xc8],0x10` → `return 1` | 寿命 **16 帧**（= 16 段，每帧长一段）|
| | 已生成段 `[+0x83+i*4] -= 0x10` | 尾部**逐帧淡出**（A 字节，下限 0）|
| | `Rng::rand_int` `0x464440` → 步长 = `8 + rand·8` | `0x4a4078`=8.0、`0x4a3eb0`=2⁻³² ⇒ **步长 ∈ [8, 12)** |
| | 顶点 = `0x4108e0`（`fsincos` × 步长）再**累加到上一顶点** | **随机游走折线**（不是圆弧）|
| | 每步转角 = `(rand·2⁻³¹ − 1)·π/10`，经 `0x464640`(`math_reduce_angle`) 归约 | ∈ **[−π/10, 0)** ⇒ 每步 −0°..−18°，折线锯齿化 |
| on_draw `0x410590` | `call 0x45d5d0`，收尾 `push 0x3` | **`D3DPT_LINESTRIP`** ⇒ **天生单像素**（stride `0x14`）|

- **④ 结论** ✅（TH12 v1.00b）：**每帧 2 条、共 16 段、步长 8–12、每步转 −0°..−18°、`LINESTRIP` 单像素、
  蓝色渐变、存活 16 帧**；一次召唤约 spawn 100 条。
- 「**单像素粗细**」这条现在是**代码级判据**：effect #1 的折线（`0x45d5d0`）与 `drawPolyBorder`
  （`0x45d710`）收尾都是 `push 0x3` = **LINESTRIP**；而 anm 的 `drawPoly`（`0x45d430`）是 `push 0x6` =
  **TRIANGLEFAN**。⇒ **「细线/圆环」观感 = LINESTRIP，「实心盘」观感 = TRIANGLEFAN**，可当场判一支
  anm 脚本画出来是哪种。

### (d) 订正：§10c 的「紫环本体 = `bullet.anm` 204/205」说的是**另一件东西** ✅

`bullet.anm` **207** 那支 VM 确实在召唤时创建，但它是**常驻光环**：204/205 只 `alphaTime(60,0,255)` **渐入、
半径固定 62/55、不缩**，207 一直挂到 UFO 离场才 `interrupt 1` 淡出（§9）。
⇒ **玩家在召唤瞬间看到的"缩小的紫圆"来自本节 (b) 的 C 侧 effect #2，不是 204/205**；204/205 是它身下
**一直转的那层细线光环**（旋转角 = 吸收进度 `fill`）。两件事在同一次召唤里同时存在，此前被混为一谈。

### (e) 否证（顺手排掉的候选）

| 候选 | 一手原文 | 判掉的理由 |
| --- | --- | --- |
| `enemy.anm` 139/140/141 | `drawPoly(192,3)` + `color2(255,128,255)`、`scaleTime(60→0)` + `alpha2Time(60→255)` | **3 边（三角）**；且 v1.00b 全 exe 无调用者（§10c(c)）|
| `bullet.anm` 118 | `drawPoly(256,48)` + `scaleTime(40→0,0)` + `alpha2Time(40,0,255)`，颜色 `(255,80,80)/(255,255,192)` | 填充 48 边、**缩到 0（没有 r_min）**、颜色也不对；不在召唤链上 |
| `bullet.anm` 204/205 | `drawPolyBorder(62/55,48)` + `color(96,96,224)` | 见 (d)：常驻光环、**不缩** |
| `bullet.anm` 206 | `ins_101(32)` + `sprite(404)` + `scale(6,59)` | `sprite404` = `bullet/etama3.png` 的一块 14×128 竖条 ⇒ **与 owner「etama3.png 里的东西不会在召唤时出现」冲突**，见 §12 未答 9 |

### (f) 证据（可独立复核）

- 反汇编：`objdump -d --start-address=0x44a8a0 --stop-address=0x44ab60`（summon，含 `0x44a947`）；
  `0x40fbe0`（spawn_effect）；`0x40f940`（initialize，字符串 `0x49f69c`）；`0x410720`/`0x4107e0`/`0x410840`（effect #2）；
  `0x4102b0`/`0x4103f0`/`0x410590`/`0x4108e0`（effect #1）；`0x45d430`/`0x45d5d0`/`0x45d710`（三个绘制器）。
- 静态表/常量（PE 节映射直读）：效果表 `0x4af160`（stride `0x18`）；`0x4a4220`=1.5、`0x4a3db8`=128.0、
  `0x4a4078`=8.0、`0x4a3eb0`=2⁻³²、`0x4a3ea8`=2⁻³¹、`0x4a3cd8`=float π、`0x4a3e40`=10.0、
  `0x4a3d70`/`0x4a3d28`/`0x4a3d68`=32/192/16。
- 符号交叉（`th-re-data` v1.00b）：`0x4b43d4 = EFFECT_MANAGER_PTR`、`0x4b43c8 = BULLET_MANAGER_PTR`
  （`zBulletManager.bullet_anm@+0x4debdc`）、`0x4b43e4 = GUI_PTR`（`zGui.front_anm@+0x6d44`）、
  `0x454d10 = AnmLoaded::load_script_into_vm`、`0x4621c0 = AnmManager::allocate_vm`、
  `0x464440 = Rng::rand_int`、`0x464640 = math_reduce_angle`、`0x45c3a0 = AnmManager::draw_vm__mode_9__texCircle`。
- 中间产物：`local/_scratch/ufo-ring-verify/`（`summon.asm`、`effect2_callbacks.asm`、
  `effect1_callbacks.asm`、`texcircle_handlers.asm`、`{bullet,front,enemy}.list.txt`）。

## 11. 一句话模型（地址串）

```
敌人死 → dropExtra(10/11/12, n) 或 dropMain(...)  (ECL opcode 407/410, handler 0x4179cc / 0x417a38)
   → ItemManager__spawn_item 0x4273f0（类型 10..15 进 token_items[]）
   → 自机吃到 → ItemManager__on_tick__body 0x425c20 的 case 10/13|11/14|12/15
   → FUN_004270b0 0x4270b0 → FUN_00422e80 0x422e80 记 3 个槽
   → 三同色/三异色 → GLOBALS.__field_24 0x4b0c64 + __UFO_COUNT__COPY_1C 0x4b0c5c = 3
   → Ufo__on_tick 0x44a2d0 检测到 → FUN_0044a8a0 0x44a8a0 生成 ECL 敌机 UFO_Red/Blue/Green/Rainbow
     + EffectManager__spawn_effect(idx=2) 0x44a947 ⇒ 紫圆(effect #2: 32 边实心 0xFF00FF, 半径 128→−1.5/帧, 50F)
       + 每帧 2 条蓝闪(effect #1: 16 段随机折线, LINESTRIP 单像素, 0xFF0040FF 蓝)              ← §10d
     + front.anm 82 横幅 / bullet.anm 207 常驻光环 / front.anm 134 碟体 / front.anm 136 血槽   ← §10c
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
9. 🟡 **`bullet.anm` 206（召唤光环里那支 texCircle 环带）到底画不画得出来**：它的 `sprite(404)` 指向
   `bullet/etama3.png`（该 sprite = 14×128、全不透明浅蓝竖条；同一张贴图其余部分是 TH07 时代
   「Border of Life / Spell Card Attack」竖排文字条）⇒ 与 owner 实机口径「`etama3.png` 里的东西
   不会在召唤时出现」**冲突**。待验方向：mode 9/12/13（`AnmManager::draw_vm__mode_9__texCircle`
   `0x45c3a0`）把 `vm+0x478` 当作**已排好的** TRIANGLESTRIP 直接画掉，而全 exe 的 49 处 `+0x478`
   读写里**只有 mode 10 自带填充**（`type()` handler `0x456e29` → `0x45d9a0`：`malloc(0x4b0)`
   装 11×84B 表 + 覆写 `+0x488/+0x48c` = `0x45dcd0`/`0x45df30`）—— **mode 9/12/13 的缓冲填充者没定位**，
   所以 206 有可能实际画不出东西（🟡 与 owner 观察方向一致，但未坐实）。
10. ❓ `enemy.anm` 139/140/141（紫三角）在 v1.00b 无调用者（§10c(c)）—— 是否旧 revision 残留未定；
   顺便：`drawPoly` 的**两个**颜色（`color()`/`color2()`）各管哪一段顶点（中心 vs 边缘）没逐字节核，
   复刻用色时会用到（🟡）。

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
  §9 道具/脚本号/换色、§10 两个 draw 函数、§10c 召唤的 anm 侧四棵 VM、**§10d C 侧 effect #2/#1
  （含三个绘制器的 `DrawPrimitiveUP` 类型）**。
- 🟡 **推断/未定**：§6 的设计意图读法、§10 调试读数可见性、§12 全部条目。
- 复核入口：Ghidra 工程 `local/th12.v1.00b/ghidra_projects/th12.exe.gpr`（程序名 `/th12.exe`），或
  `python3 tooling/ghidra/scripts/decompile_funcs.py th12 0x44a2d0 0x44a8a0 0x44aca0 0x44af00 0x422e80`。
- §10d 的复核**不需要 Ghidra**（全是静态直读）：
  `objdump -d --start-address=0x44a8a0 --stop-address=0x44ab60 -M intel local/th12.v1.00b/th12.exe`
  （看 `0x44a947` 的 `push 0x2`）；同理 `0x410720`–`0x4108b0`（effect #2/#1 六个回调）、
  `0x45d430` / `0x45d5d0` / `0x45d710`（三个绘制器的收尾 `push 0x6` / `0x3` / `0x3`）。
- 数据侧复核：`thdat -x 12 -C <dir> local/th12.v1.00b/th12.dat`；
  `thecl -d 12 -m local/vendor/eclmap/eclmap/th12.eclm default.ecl`；
  `thanm -l 12 -m local/vendor/thpages/static/mapfile/v4.anmm front.anm`。
- 交叉：社区 [`東方Project攻略 Wiki 星/Data`](https://wikiwiki.jp/thk/%E6%98%9F/Data)（填充阈值 34/36/39/42/46/51/56、
  彩虹令牌 150F 换色/靠近暂停、红→残机碎片、绿→Bomb 碎片）。
- 相邻子系统：[`ecl/OVERVIEW.md`](../../ecl/OVERVIEW.md)（ECL VM）、
  [`_shared/community-sources.md`](../../_shared/community-sources.md)。
