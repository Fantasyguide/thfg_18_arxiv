# th13 神灵 / 灵界（Trance）系统 —— 一手反编译（生成 / 运动 / 槽 / 灵界 / HUD）

> **版本**：TH13 v1.00c（`th13.exe`，md5 `6f39c0708ee4daa94f5053526c4e10cc`，790,528 B，PE 时间戳 2011-08-17 13:56，i386，imagebase `0x400000`）。本文裸地址默认属该版本；引用其他版本须写成 `th17:0x…`。
> **方法**：Ghidra 12.1.3 headless 一手反编译/反汇编（工程 `local/th13.v1.00c/ghidra_projects/th13.exe.gpr`，2359 函数）＋ `thdat`/`thecl`/`thanm` 解包 ＋ exe 字节直读 ＋ ExpHP `th-re-data` 符号对账。
> **可信度**：结论以 ✅ 一手为主；ExpHP 符号名**先当 🟡**，本文明写哪些已坐实、哪些证伪。

## 0. 一句话模型（地址链）

```
敌人受伤累积伤害 ─┐
敌人死亡(spec1/spec2 计时) ─┼─► SpiritItemManager__spawn_spirit `0x438ec0` ─► 场上神灵(astral.anm)
spec0(N) / item 10..14 / 玩家死亡 ─┘                    │
                                                         ▼ 距离 ≤20px
                                        SpiritItem__on_tick `0x438730` 收集
                                                         │
                        CURRENT_SPIRIT_GAUGE `0x4be808` += 2（`0x43d530`）
                                                         │ 满 600
                       按灵界键（输入位 `0x800`）→ TranceManager__on_tick `0x4057f0`
                                                         ▼
                        TranceManager::begin `0x405a20`：GAME_FLAGS `0x4be830` |= 0x100
                                                         │
        每帧 GameThread__on_tick 调 `0x43d4f0` → 槽 -1/帧 → 归零 → end `0x405b80`
```

**「灵界中中弹不死」有前后两道闸门**：`Player::on_hit` `0x444ca0` 开头见 trance 标志就直接 return（`0x444ca6`）；
真被打死时 `Player::die` `0x444a10` 会先调 `TranceManager::begin` `0x405a20`（参数 1 = 免费），成功就不扣命（`0x444a41`）。

## 1. 样本与工程（本文的证据前提）

| 项 | 值 |
| --- | --- |
| exe | `local/th13.v1.00c/th13.exe`，790,528 B，md5 `6f39c0708ee4daa94f5053526c4e10cc` |
| PE | i386，imagebase `0x400000`，4 段（`.text`/`.rdata`/`.data`/`.rsrc`），PE 时间戳 2011-08-17 13:56 |
| Ghidra 工程 | `local/th13.v1.00c/ghidra_projects/th13.exe.gpr`（2359 函数）|
| 复现 | `python3 tooling/ghidra/scripts/decompile_funcs.py th13.v1.00c 0x4057f0 0x438ec0 …` |
| 资产 | `thdat -x 13 local/th13.v1.00c/th13.dat <dir>`；`thanm -l 13 -o -m local/vendor/thpages/static/mapfile/v8.anmm <anm>` |

> ⚠️ 本机 `thanm` 没有 `spec` 子命令：脚本文本要用 `-l VERSION`（列出规格），贴图用 `-x VERSION -u`。
> 灵界/神灵的 anm 是 **`astral.anm`**（不是 `front.anm`，也不在 `bullet.anm`）。

## 2. 神灵对象（`SpiritItem` / `SpiritItemManager`）

### 2.1 对象形状 ✅

`SpiritItemManager` 全局指针 `SPIRIT_ITEM_MANAGER_PTR` `0x4c22a4`，`operator new` `0x4385d0` 里 `malloc(0x8840)` —— 与 th-re-data 的 `zSpiritItemManager` 尺寸 **0x8840 逐字节对上** ✅。
槽位数组从 `+0x14` 起，**stride `0x88`**、**256 个**（`0x14 + 256*0x88 = 0x8814`，正是 `__num_spirits_alive` 的位置）。

| 偏移 | 名字 | 含义 / 证据 |
| --- | --- | --- |
| `+0x0` | state | 0=空槽，1=漂移，2=被吸，4=蕾米特例，5=死代码 |
| `+0x4` | type | 0..3（4 = 按 1 处理，见 §3.5）|
| `+0xc` | spawn_index | 每生成一个 ++，决定抛出象限 |
| `+0x10` | speed_now | 吸引时的当前速度（0 → 上限）|
| `+0x14/18/1c` | pos | x/y/z（生成时被夹到 x∈[-192,192]、y∈[0,384]）|
| `+0x2c` | speed | 初速，每帧 -0.12666667，地板 0.13 |
| `+0x30` | angle | 运动方向（弧度）|
| `+0x54` | flags | 生成时清低 4 位 |
| `+0x58` | anm_vm_id | 该神灵的 anm VM 句柄 |
| `+0x5c` | timer_1 | 寿命计时（==480 帧时发中断 1）|
| `+0x70` | timer_2 | 第二个 zTimer（用途未定 🟡）|
| `+0x84` | flags2 | bit0 = **生成时处于灵界**（`0x438fde`）；bit1 = 生成时置位，用途未定 🟡 |

管理器侧（一手反编译 `0x438660`/`0x438ec0`）：

| 偏移 | 名字 | 含义 |
| --- | --- | --- |
| `+0x14` | items[256] | 神灵槽数组 |
| `+0x8814` | num_alive | 每帧由 `on_tick__body` 重数 |
| `+0x8820` | num_spawned_in_stage | 每生成一个 ++ |
| `+0x8824` | spirit_chain_timer | 神霊连锁计时（`0x3b`/`0x3c` 帧）|
| `+0x8838` | chain_counter | 连锁计数，见 §3.6 |

### 2.2 四种神灵 = 四种奖励 ✅（一手反编译 `0x438730` 的收集分支）

| type | item type | 普通奖励（一手数值）| 灵界中奖励 | 效果 id（普通/灵界）|
| --- | --- | --- | --- | --- |
| 0 | 10 | 生命碎片 +1（`0x43d330`）| 生命碎片 **+2** | `0x60` / `0x61` |
| 1 | 11 | 最大得点 +10（`0x42f510`，常量 `0x4aeea4`）| **+100**（`0x4aeaf0`）| `0x66` / `0x67` |
| 2 | 12 | 符卡碎片 +1（`0x43d480`）| 符卡碎片 **+2** | `0x63` / `0x64` |
| 3 | 13 | 灵界槽 +1 且加分（`0x43d530`）| 只加分（灵界中不加槽）| 分数弹窗 |

- 收集时**所有类型先统一加灵界槽 +2**（`0x438b3d`：`LEA EAX,[EDI-0x3d]`，EDI=0x3f）✅；type 3 再加 1（`0x438c3d`）。
- 收集音效 = `0x3f`（63）✅（`0x438b38`：`MOV EDI,0x3f` + `CALL 0x00460c50`）。
- type 3 的分数 = `CURRENT_PIV__SCALED`（`0x4be7dc`）`/100` 再**向下取整到 10 的倍数**；弹窗显示该值（`0x44ad60`，色 `0xffffff00`），
  实际入账 `值/10`（`0x413bf0`：`CURRENT_SCORE += 值/10`）✅（`0x438c3d`–`0x438c9d` 一手汇编）。
- **早苗（CHARACTER==2）特例** ✅：收 type 1 时除加最大得点外**额外 +1 灵界槽**（`0x438bea`：`CMP [0x4be7b8],2`）。
- item type 14 → `spawn_spirit(4)`；type 4 在 `0x438ed8` 被改写成 1（奖励同 type 1），但**不计入连锁**（§3.6）。

> 🟡 **颜色 ↔ 类型**：颜色只能从 `astral.anm` 的 `color()` 常量读出（见 §2.3），没有独立的语义表。
> 不能反推「玩家看到的颜色」是否与 `color()` 一致 —— 那需要跑游戏或查社区。

### 2.3 anm 脚本与颜色 ✅（`astral.anm` 一手 `thanm -l`）

`TranceManager::initialize` `0x405530` 用 `MOV ECX,0x4aa338`（字符串 `"astral.anm"`）＋ `MOV EAX,0x19`（25）调 `AnmManager__preload_anm` `0x46df40`，
句柄存进 `TRANCE_MANAGER_PTR+0x10`；`spawn_spirit` 就用它建 VM（`0x439090`：`MOV ECX,[0x4c2164]` → `MOV EAX,[ECX+0x10]`）✅。

**脚本号公式** ✅（`0x43907e` 起一手汇编）：`script = type*6 + 1`（普通）／`type*6 + 4`（**生成时处于灵界**，`0x438fde` 用 `GAME_FLAGS>>8 & 1` 判定）。

| type | 普通脚本 | 灵界脚本 | 普通配色（`color()` 一手值）| 灵界配色（贴图自带）|
| --- | --- | --- | --- | --- |
| 0 | 1 | 4 | 外环 `(0,127,0)` 绿 + 内芯 `(255,32,255)` 品红 | 白环 sprite3 + 品红 glow sprite7 `(255,31,255)` |
| 1 | 7 | 10 | 外环 `(63,63,0)` 橄榄 + 内芯 `(80,80,255)` 蓝 | 白环 sprite4 + 蓝 glow sprite8 `(28,59,255)` |
| 2 | 13 | 16 | 外环 `(63,0,63)` 紫 + 内芯 `(32,255,32)` 绿 | 白环 sprite5 + 绿 glow sprite9 `(39,241,39)` |
| 3 | 19 | 22 | 外环 `(63,63,63)` 灰 + 内芯 `(128,128,128)` 灰 | 白环 sprite6 + 青 glow sprite10 `(210,255,255)` |

贴图 = `astral.anm` entry1 `enemy/littlegods.png`（256×64，**10 个 30×30** sprite）；`eff_astral.png`（512×512）是灵界背景。

普通脚本（以 script1 为例，一手字节）：

```
-1:  alpha(0); scale(1.5,1.5); layer(6);
0:   scriptNew(script2); scriptNew(script3);     // 外环 + 内芯
+200:scaleTime(200, 0, 0.4, 0.4); stop(); interruptLabel(1); +40: delete();
```

灵界脚本（script4）则是 `layer(13)`、无染色、`scaleTime(20,9,0.95→1.0)` 循环呼吸，无淡出段。

## 3. 神灵怎么生成（六条通道，全部一手）

`spawn_spirit` 的真实签名 ✅（`0x438ec0` 序言一手汇编；Ghidra 多塞了一个幻影 `EDX` 参数）：

```c
SpiritItemManager::spawn_spirit(this /*ECX*/, int type, float* pos, float speed, int play_sound)
// 音效 = type + 0x3b（59..62）；pos 被夹到 x∈[-192,192]、y∈[0,384]
```

| 通道 | 调用点 | 参数（一手）| 说明 |
| --- | --- | --- | --- |
| A 伤害阈值 | `0x419de9`（`FUN_00419a90`）| `type=1, speed=2.0, sound=1` | 累计伤害越过阈值就掉一颗，见 §3.1 |
| B 死亡计时 | `0x419f8e`（`FUN_00419a90`）| `type=1, speed=0.5, sound=1` × N | `spec1`/`spec2` 配置，见 §3.2 |
| C ECL spec0 | `0x41f6c1`（opcode 1000）| `type=ECL 参数0, speed=2.0` | 见 §3.3 |
| D item 10..14 | `0x42f04b`（`ItemManager__spawn_item`）| `type=item_type-10, speed=0.5` | `dropMain(14)` 即此路 |
| E 玩家死亡 | `0x4434f3`（`Player__on_tick`）| `type=3, speed=2.0, sound=0` | 死亡动画期间每帧一颗 |
| F 连锁奖励 | `0x439250`（递归）| `type=3, speed=0.5, sound=1` | 见 §3.6 |

### 3.1 通道 A：按累计伤害掉 ✅

`FUN_00419a90`（敌人逻辑，`0x41a914` 由 `FUN_0041a8a0` 调用）里：

```c
enemy->+0x3f60 += damage;                       // 累计伤害
if (enemy->+0x3ffc <= enemy->+0x3f60) {         // 越过阈值
    if (enemy->+0x4004 > 0x13 && !(enemy->+0x4034 & 2))
        spawn_spirit(manager, 1, enemy+0x44, 2.0, 1);   // 掉 1 颗
    enemy->+0x3ffc += 200/400/600/1000;         // 按与自机距离选增量
}
enemy->+0x4004 = 30;                            // 冷却 30 帧（>19 才再掉）
```

距离分档（`0x419e3a` 起一手）：`dist²<2304(48²)→+200`、`<9216(96²)→+400`、`<36864(192²)→+600`、否则 `+1000`
⇒ **越靠近敌人打，掉得越密** ✅。`+0x4034` bit1 = 关闭掉落（由 `spec3` 开关，§3.3）。

### 3.2 通道 B：死亡计时衰减（ECL `spec1` / `spec2`）✅

`spec1(N)`（opcode 1001，`0x41f6c8`）写 `enemy+0x3ff4 = N`；`spec2(N)`（opcode 1002，`0x41f6d9`）写 `enemy+0x3ff8 = N`。
死亡时（`0x419f41` 起）：

```c
max = enemy->+0x3ff4;  div = enemy->+0x3ff8;
if (enemy_timer <= max) {
    n = (max - timer) / (max / div) + 1;
    if (DIFFICULTY == 2 || DIFFICULTY == 3) n += 1;      // Hard/Lunatic 多 1
    for (i = 0; i < n; i++) spawn_spirit(manager, 1, enemy+0x44, 0.5, 1);
}
```

⇒ **`spec1` = 计时窗口（帧）、`spec2` = 上限颗数**（最快击杀时 `n = spec2+1`，拖满窗口只剩 1）。

ECL 全量普查（`thecl -d 13`，全 8 关 + 各 BOSS，按子程序归并）：

| 关卡 | 敌人 → `spec2/spec1` |
| --- | --- |
| st01 | `GirlB01 (5,120)`、`GirlB02 (5,140)`、`Phantom01 (1,40)`、`Phantom02 (1,60)`、`Phantom02b (1,60)` |
| st02 | `ECir00 (3,90)`、`ECir00b (3,70)`、`ECir01_func (4,60)`、`GirlA01 (4,120)`、`GirlB02 (6,140)`、`GirlC01func (6,120)`、`Phantom02 (1,60)` |
| st03 | `ECir00 (2,90)`、`ECir00b (2,70)`、`GirlB01_at (2,120)`、`GirlC01 (4,120)`、`GirlD01 (4,120)`、`Phantom01 (2,70)`、`Phantom02 (2,60)` |
| st04 | `ECir00 (2,90)`、`GirlA01_at (3,60)`、`GirlB01_at (2,120)`、`GirlC01 (4,120)`、`GirlD01 (10,350)`、`GirlD03 (10,400)`、`Phantom01_at0 (3,100)` |
| st05 | `ECir00 (5,90)`、`ECir01 (5,120)`、`GirlA01_at (1,90)`、`GirlC01 (10,400)`、`GirlC02 (10,400)`、`GirlD01 (16,350)` |
| st06 | `ECir00 (10,180)`、`GirlA01_at (1,90)`、`GirlC01 (10,400)`、`GirlD01 (16,350)` |
| st07 | `GirlA01_at (2,90)`、`GirlB02_at (8,180)`、`GirlC01 (10,400)`、`GirlD01 (20,350)`、`GirlD02 (40,400)` |
| st03bs | `PhantomC1 (4,300)`；`BossCard1/BossCard3` 调 `spec3(1)` 关闭掉落，`Boss2/Boss3` 调 `spec3(0)` |

### 3.3 `spec0` / `spec3`（opcode 1000 / 1003）✅

ECL 分发表一手解出（`0x41b2cf`：`byte 表 0x41f9d8` → `指针表 0x41f760`，索引 = opcode-300）：

| opcode | 名字（社区 eclmap）| 处理分支 | 行为 |
| --- | --- | --- | --- |
| 1000 | `spec0` | `0x41f6a1` | `spawn_spirit(manager, ECL 参数0, enemy+0x44, 2.0, 1)` |
| 1001 | `spec1` | `0x41f6c8` | `enemy+0x3ff4 = 参数0`（死亡掉落计时窗口）|
| 1002 | `spec2` | `0x41f6d9` | `enemy+0x3ff8 = 参数0`（死亡掉落上限）|
| 1003 | `spec3` | `0x41f71d` | 用参数0 置/清 `enemy+0x4034` bit1（**关/开神灵掉落**）|

> ⚠️ **社区 eclmap 的一个坑**：`local/vendor/eclmap/eclmap/th13.eclm` 把 `562` 命名成 `dropItemsSp`，但一手分发表显示
> **562 与 509 `dropItems` 共用同一段尾代码 `0x41d325`**（562 只是跳过 509 的「暂停中不生成」检查 `0x41d316`）。
> 两者都走 `FUN_00417ad0(&enemy+0x3f68, &enemy->abs_pos)` → `ItemManager__spawn_item(type=+0x3f68, -π/2, 2.2)`。
> st03bs 的 `dropMain(14); dropItemsSp();` 因此 = **掉一颗 item 14（= 神灵 type 4）**，不是另一套掉落。

### 3.4 通道 D：item type 10..14 ✅

`ItemManager__spawn_item` `0x42f020` 开头：`if (type-10 <= 4) { spawn_spirit(manager, type-10, pos, 0.5, 1); return 0; }`（`0x42f02c`–`0x42f050` 一手）。
⇒ **item 10/11/12/13/14 = 神灵 type 0/1/2/3/4**。ECL 的 `dropMain`/`dropExtra` 指向这些 type 时，掉落的就是神灵。

### 3.5 通道 E：玩家死亡 ✅

`Player__on_tick` 死亡状态（`+0x65c == 2`）的分支入口 `0x44341c`：`+0x668`（死亡计时，0→30）不等于 3 时直接跳到 `0x4434d8`，
于是 **每帧**执行 `spawn_spirit(manager, 3, player+0x5b8, 2.0, 0)`（`0x4434d8`–`0x4434f3`）；等于 3 时先撒 7 个道具再落到同一段。
⇒ 静态看是一次死亡约 30 颗 type-3 神灵 🟡（跳转表 `0x443d74` 一手：state2→`0x44341c`、state4→`0x443408`；未做逐帧实跑验证）。

### 3.6 通道 F：神霊连锁奖励 ✅

`spawn_spirit` 尾部（`0x4391a0` 起）：

```c
if (type != 3 && type != 4) {
    this->+0x8828 = 60;  this->+0x882c = 60.0f;  this->+0x8824 = 59;   // 重置 60 帧窗口
    this->+0x8838++;                                                    // 连锁计数
    if (this->+0x8838 > 9) spawn_spirit(this, 3, pos, 0.5, 1);          // 第 10 颗 → 额外送 1 颗 type 3
}
```

窗口在 `SpiritItemManager__on_tick__body` `0x438660` 里每帧递减；归零则 `+0x8838 = 0`。
⇒ **60 帧内连掉 10 颗 → 第 11 颗是 type 3（加分 + 灵界槽）** ✅。type 3/4 自身不推进连锁。

## 4. 神灵运动与收集 ✅（`SpiritItem__on_tick` `0x438730`，一手反编译 + 常量直读）

| 项 | 值 | 出处 |
| --- | --- | --- |
| 初速 | `speed = 参数 + randf()*3.5` | `0x4391xx`（`FUN_00472de0`）|
| 抛出方向 | 按 `spawn_index % 4` 选象限，再 ±π/8 随机 | `0x4390e1` 的 switch（−3π/4 / +π/4 / +3π/4 / −π/4）|
| 速度衰减 | 每帧 `-0.12666667`，地板 `0.13` | `0x438850` 附近 |
| 撞墙反弹 | 出 x∈[-192,192] 翻 x 分量角；出 y∈[0,448] 翻 y 分量角 | `0x438800` 附近 + `FUN_0040bb70(pos,0,224,384,448)` |
| **吸引半径** | **96 px**（`dist² ≥ 9216` 退出吸引）| state 1 分支 |
| **脱离半径** | **128 px**（`dist² ≥ 16384` 退回漂移，迟滞）| state 2 分支 |
| 吸引加速 | 每帧 `+0.3`，上限 **12.0** | state 2 分支 |
| **收集半径** | **20 px**（`dist² < 400`）| `0x438b17`（`double 400.0` @ `0x4aee10`）|
| 蕾米特例 | `CHARACTER==0` 且灵界生成 → state 4：任意距离缓吸 `+0.05` 上限 4.0，进入 **112 px** 转正常吸引 | state 1→4 / state 4 分支 |
| 死亡/放 B 时 | `player+0x65c == 2 或 4` → 不被吸引、不收 | state 1/2/4 分支 |
| 寿命 | timer_1 == **480 帧** → `anm_set_interrupt_by_id(vm, 1)` + 清 flag bit1 | `0x438a61`（`CMP [EBX+0x60],0x1e0`）|
| 出界删除 | 出 x∈[-224,224]、y∈[-32,480] → 槽置 0 并释放 VM | `0x438ad1` + `FUN_0040bb70(pos,0,224,448,512)` |
| 与自机子弹/擦弹 | **无交互**：神灵不参与擦弹，也不被子弹判定 | 全 exe 无「子弹 × SpiritItem」路径 🟡（证伪式结论）|

state 5（`0x438a42`）引用 `ENEMY_MANAGER_PTR->field_1c+0x40` 与 `FUN_00439550`，但**全函数没有任何写入 state=5 的地方** ⇒ 本 build 的死代码 🟡。

## 5. 灵界槽（`CURRENT_SPIRIT_GAUGE` `0x4be808`）✅

| 项 | 值 | 出处 |
| --- | --- | --- |
| 上限 | **600** | `0x405815`（`CMP …,0x258`）、`0x43d530` 里夹到 600 |
| 分档 | **每 200 一格**（共 3 格）| `0x43d530` 的 `iVar2 = 旧值/200` 与 `新值/200` 比较 |
| 收神灵 | **+2**（所有类型）；type 3 再 +1 | `0x438b3d` / `0x438c3d` |
| 升格反馈 | 每跨一格播音 `0x44` + 在自机上方 32px 生成效果 `0x6b+格数`（107/108/109）| `0x43d530` |
| 归零 / 重置 | 关卡开始写 0（`0x42bbfd`，EDI=0）；自机重置分支写 0 且把炸弹补到 ≥2（`0x4435b4`–`0x4435d9`）；Game Over 菜单写 200（`0x4406b6`）| 一手汇编 |
| 灵界中消耗 | **-1/帧**（不是一次性扣 200）| `0x43d4f0` |
| 灵界结束补扣 | 非死亡触发的灵界再 **-200**（夹到 0）| `0x405cdd` |

`0x43d4f0`（每帧由 `GameThread__on_tick` `0x42cb2e` 调用）：

```c
if (gauge_state_4be80c == 2 || == 3) {           // 灵界进行中
    Gui__sub_42b030_spirit_related();            // 刷 HUD
    CURRENT_SPIRIT_GAUGE -= 1;
    if (CURRENT_SPIRIT_GAUGE < 1) { TranceManager__end(`0x405b80`); gauge_state_4be80c = 0; }
}
```

⇒ **灵界时长 = 进入时的槽值帧数**（满槽 600 帧 = 10 秒），HUD 上的 `%.2fs` 就是它 ✅。

## 6. 灵界（Trance）

### 6.1 激活条件 ✅（`TranceManager__on_tick` `0x4057f0` 一手汇编）

不处于灵界时，**全部满足**才激活（否则把 `this+0x18` 清零）：

| 条件 | 汇编点 |
| --- | --- |
| `GAME_FLAGS & 0x100 == 0`（不在灵界）| `0x4057fa` |
| `BOMB_PTR != 0 && BOMB_PTR->+0x40 == 0`（没在放 B）| `0x405806`–`0x405813` |
| **`CURRENT_SPIRIT_GAUGE >= 600`（必须满槽）** | `0x405815` |
| `ENEMY_MANAGER_PTR->+0xb8 != 0`（场上有敌）| `0x40582a` |
| **输入位 `0x800`**（`KEY_INPUT` `0x4e4c14`）| `0x405833` |
| `this+0x20 >= 0x1e`（30 帧冷却）| `0x405842` |
| `GUI_PTR->msg == 0`（不在对话）| `0x40584d` |
| `(GAME_FLAGS & 0x30) != 0x20` | `0x405859` |

满足后播**音效 `0x34`**（`0x40586d`，位置 = 自机 x）并调 `0x405a20(this, 0)`。
`0x4e4c14` 就是输入位域（同一 word 的 bit1 = B 键，见 `Player__on_tick` `0x4433dc`）；具体按键由配置文件决定，exe 里没有硬编码表 🟡。

### 6.2 进入 / 退出 ✅

`TranceManager::begin` `0x405a20`（参数 `param_1` = 是否「死亡触发」）：

```c
if (this+0x14 & 1) return 0;                       // 已在灵界 → 也返回 0（视为成功）
if (CURRENT_SPIRIT_GAUGE < 200) return -1;         // 槽不足 → 失败
GAME_FLAGS |= 0x100;                               // ★ 灵界标志
this+0x14 = (this+0x14 | 1) 且 bit1 = param_1;
重置 this+0x1c/0x20/0x24 计时；this+0x28 = &GAME_SPEED;
this+0x38 = AnmManager 句柄;  anm_set_interrupt_by_id(this+0x38)   // 起灵界背景
BGM: 取 SOUND_MANAGER.bgm_filename，把扩展名前一位改成 'b' 再播  // 灵界专用 BGM 变体
DAT_004be80c = 2;                                  // 灵界相位
玩家 anm 换成灵界形态（AnmLoaded__copy_vm + AnmVm__run）；player+0x650..0x657 = 0
```

`TranceManager::end` `0x405b80`：

```c
if (!(this+0x14 & 1)) return;
this+0x14 &= ~1;  GAME_FLAGS &= ~0x100;
STAGE_PTR->+0x404c |= 1;
if (this+0x14 & 2) {                    // ★ 死亡触发的灵界
    播音效 + 在自机处生成效果 0x1a;
    if (PLAYER_PTR) FUN_00444a10();     // 灵界结束 → 真死（再走一次 Player::die）
}
… 恢复玩家 anm / BGM …
if (!(this+0x14 & 2)) { CURRENT_SPIRIT_GAUGE -= 200; if (<0) = 0; }   // 手动灵界才扣 200
DAT_004be80c = 0;  FUN_00405440(0x46);   // 音效 0x46
```

### 6.3 灵界期间的效果（一手汇总）

| 效果 | 证据 |
| --- | --- |
| **中弹无伤**（`Player::on_hit` 直接返回）| `0x444ca6` |
| **死亡前自动消耗灵界保命**（死亡触发的灵界免费）| `0x444a14` + `0x405a50` |
| 神灵**换外观**（`type*6+4` 无染色版）| `0x43907e` |
| 收神灵**不再加槽**，奖励翻倍（生命/符卡碎片 ×2、最大得点 +100）| `0x438d09` 起的第二张分派表 |
| 灵界背景 anm（`astral.anm` script0，blendMode 7、缓慢自转）| `0x405ac5` |
| BGM 换成 `xxx b.xxx` 变体 | `0x405b05`–`0x405b27` |
| HUD 显示剩余 `%.2fs` | `TranceManager__on_draw__body` `0x405940` |
| **没有**：伤害倍率、清弹、自动收集、得分倍率 | 全 exe 无对应读点 🟡（证伪式）|

## 7. 「灵界中中弹不死」完整地址链 ✅

```
子弹 × 自机判定  FUN_00443fd0 `0x443fd0`（四个角色各一份：0x443fd0 / 0x444140 / 0x444260 / 0x444750）
    │  AABB 命中且 player+0x65c ∉ {2,3,4} 且无敌计时 +0x14688 < 1
    ▼
Player::on_hit  FUN_00444ca0 `0x444ca0`
    │  0x444ca6: TEST [0x4be830],0x100 → 非 0 就 return   ← ★ 闸门 1
    │  否则：播音效 2、生成效果 0x19、记 miss、player+0x65c = 4
    ▼  8 帧内没放 B（deathbomb 窗口 `0x443525`）
Player::die  FUN_00444a10 `0x444a10`
    │  0x444a14: TEST byte [0x4be830],0x30 / gui->msg
    │  0x444a1d: CALL TranceManager::begin(TRANCE_MANAGER_PTR, 1)   ← ★ 闸门 2（免费灵界）
    │  返回 0 且槽 ≥ 200 → player+0x65c = 1（`0x444a41`）、不扣命、直接 return
    ▼  槽 < 200 或已在灵界外
    CURRENT_LIVES-- ；player+0x65c = 2（死亡动画）
    ▼
TranceManager::begin `0x405a20` → GAME_FLAGS |= 0x100；`0x4be80c = 2`
```

两个细节：

- `0x444a14` 的 `FUN_00405a20` 返回值语义是一手汇编定的：`0x405a4c` `OR EAX,-1`（槽 <200 失败）、`0x405b55`/`0x405b69` `XOR EAX,EAX`（成功，含「已在灵界」）。
- 死亡触发的灵界在结束时（`0x405c3e`）会**再调一次 `Player::die`** ⇒ 该次保命是**延时**而非免死；
  此时槽已被逐帧扣到 0，第二次调用必然失败（`0x405a40` 检查 <200），于是正常扣命。✅

## 8. HUD：灵界槽 ✅

- **根脚本**：`front.anm` **script171**（`layer(17)`、`pos(-196,448)`、`scriptNew(172/173/174/175)`）。
  贴图：`front/agauge1.png`（sprite121/122，32×48）＋ `front/agauge2.png`（sprite123/124 48×32、sprite125 16×32）✅。
- **创建点**：`FUN_00425170` 内 `0x425eed`（`PUSH 0xab` → `FUN_0046f820`），句柄写 `gui+0x11c`（`0x425f00`），随后立刻调一次 `0x42b030`。
- **每帧更新** `Gui__sub_42b030_spirit_related` `0x42b030`（一手反编译 + 常量直读）：

| 子脚本 | 引擎写什么 | 观感 |
| --- | --- | --- |
| 172 (`0xac`) | `pending_interrupt = 3`（槽 <200）／`2`（槽 ≥200）| 槽够不够放灵界的**两态**（`0x42b08x`）|
| 173 (`0xad`) | `scale.x = uv_scale.x = (槽/200)/3.0`（除 `3.0` @ `0x4aeca8`）| **三段式**充能条，满 600 才 =1.0 |
| 175 (`0xaf`) | `scale.x = uv_scale.x = (槽%200)/200`（除 `200.0` @ `0x4aece8`）；`entity_pos.x = 比例 × 16.0`（`0x4ae8e0`）| 当前格内的细分条 + 0..16px 滑块 |

> 174（`0xae`）引擎**不写**，只在脚本里 `interruptLabel(4/5)` 淡入淡出；脚本 173 自身还有 `colorTime` 蓝↔白呼吸（`front.named.txt` 一手）。

- **数字**：`TranceManager__on_draw__body` `0x405940` 用 ASCII 字体 7 打 `"%.2fs"`（剩余秒数）；
  槽 <30 且为奇数、或 30..89 且被 8 整除时把颜色改成 `(0,0,255,255)` 蓝 —— 即**末尾闪烁** ✅。
- **玩家靠近左下角淡出**：`Gui::on_tick` `0x426632`/`0x426676` —— 闩锁 `gui+0x180` bit0：
  `player.x < -64 && player.y > 384` → `anm_set_interrupt_by_id(gui+0x11c, 5)`（淡出）；
  恢复条件 `player.y < 384 || player.x > -64` → 中断 4（淡回）。常量 `-64.0` `0x4af068` / `384.0` `0x4aedb8` ✅。
  与 TH16 季节槽是**同一套触发区 + 迟滞**（[`season/th16/01`](../../season/th16/01-season-data-and-tables.md) §9）。
- 🟡 **未定位**：创建点被 `CMP [0x4dcc30],0` 挡住（`0x425ed5`），而 `DAT_004dcc30` 全 exe **只有读、没有写**
  （15 处引用全是 `CMP`/`XOR`/`MOV eax,[abs]`，且它落在 `.data` 的 BSS 尾段）—— 运行时它必然非 0，否则槽 HUD 永不创建，但**写点没找到** ❓。

## 9. 跨作对照：TH17 动物灵 / 咆哮 🟡

| 维度 | TH13 神灵 / 灵界 | TH17 动物灵 / 咆哮 |
| --- | --- | --- |
| 管理器 | `SpiritItemManager` `0x4383a0` 起（256 槽，stride 0x88）| `GoastManager` `th17:0x40e420` 起（token 链表）|
| 全局指针 | `SPIRIT_ITEM_MANAGER_PTR` `0x4c22a4` | `GOAST_MANAGER_PTR` `th17:0x4b7684` |
| 充能槽 | `CURRENT_SPIRIT_GAUGE` `0x4be808`（0..600，3 格）| `HYPER_FILL` `th17` inner+0x84 |
| 按什么进 | 灵界键 `0x800`，**要求满槽** | 咆哮键，槽满 |
| 中弹保护 | 灵界中直接免伤；死亡前自动免费灵界 | `GoastManager::do_hyper_cancel` `th17:0x40f6d0` |
| 是否同源 | **不同源**：类名/结构/字段全不同，TH13 是「神灵对象池」，TH17 是「token 链表」 |

⇒ **机制同构（第三条资源轴 + 中弹保命），代码不同源** ✅（两边都是一手符号 + th-re-data 结构体）。
「同源代码」这一说法**没有证据**，不写。

## 10. 未答 / 下一步

| 事项 | 状态 |
| --- | --- |
| `DAT_004dcc30`（挡住槽 HUD 创建）的写点 | ❓ 全 exe 只有读；需按「大结构体整体 memcpy 初始化」的思路继续挖 |
| `zSpiritItem+0x70`（timer_2）与 `+0x54` 低 4 位 flag 的读者 | ❓ 未追 |
| 死亡状态下 type-3 神灵是否真的每帧掉 | 🟡 静态看是每帧，需实跑或逐帧断点确认 |
| 神灵颜色 ↔ 玩家观感的社区对账 | 🟡 本文只给 `color()` 一手值 |
| `ENEMY_MANAGER_PTR->field_1c+0x40` 是什么（state 5 用）| ❓ |
| 灵界键的**默认按键**（输入位 `0x800` 对应哪个键）| 🟡 输入位一手，键位映射未反 |
| TH13 试玩版是否有同一套（`0x4dcc30` 是否就是它）| ❓ 无试玩版样本 |

## 11. 可信度与证据

- ✅ **一手**：§2 的对象形状/奖励数值/anm 脚本号与配色、§3 的六条生成通道与 ECL 分发表、§4 的全部半径与速度常量、
  §5 的槽上限/分段/增减点、§6 的激活条件与进出流程、§7 的死亡链、§8 的 HUD 子脚本与淡出常量。
- 🟡 **推断/单源**：§2.2 的颜色语义、§3.5 的每帧掉落、§4 的「与子弹无交互」、§9 的同构判定。
- ❓ **未解**：§8 的 `DAT_004dcc30`、§10 表内各项。
- **ExpHP 符号对账**：`TranceManager::*` 8 条、`SpiritItemManager::*` 8 条、`SpiritItem::on_tick`、`Gui::sub_42b030_spirit_related`
  **全部 ✅ 坐实**（地址一致、行为与命名吻合）；`zSpiritItem`/`zSpiritItemManager`/`zTranceManager` 的结构体尺寸与字段偏移 ✅ 对上。
- **复核入口**：Ghidra 工程 `local/th13.v1.00c/ghidra_projects/th13.exe.gpr`；
  `python3 tooling/ghidra/scripts/decompile_funcs.py th13.v1.00c 0x4057f0 0x405a20 0x405b80 0x438ec0 0x438730 0x43d530 0x43d4f0 0x444a10 0x444ca0 0x42b030`。
- **数据侧**：`thdat -x 13` 解包 → `thecl -d 13 -m local/vendor/eclmap/eclmap/th13.eclm`、`thanm -l 13 -o -m local/vendor/thpages/static/mapfile/v8.anmm`。
- 交叉：[`animal/OVERVIEW`](../../animal/OVERVIEW.md)（TH17 咆哮）、[`season/th16/01`](../../season/th16/01-season-data-and-tables.md)（同款「道具 → 槽 → 按 C」结构 + 同款 HUD 淡出）。
