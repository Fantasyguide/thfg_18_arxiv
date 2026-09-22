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

> ★ **type 1 那行的复核确认（第 2 轮，2026-09 复核 `engine/faith/th13/01-point-item-value.md`）**：本行「最大得点 +10 / 灵界内 +100」**经一手复核为正确，无需订正**。
> 曾被怀疑的「那 10/100 只是弹窗显示的数」已**排除**：`0x42f510` 的正文是
> `add DWORD PTR [esi+0x24], ftol(farg × *(double*)0x4ae868)` + 夹到 `[esi+0x2c]`，
> 而 4 个调用点全部在调用前 `mov esi,0x4be7b8` ⇒ `+0x24`/`+0x2c` 就是 `CURRENT_PIV__SCALED`（`0x4be7dc`）与 `MAXIMUM_PIV__SCALED`（`0x4be7e4`）；
> `0x4ae868` 直读 = **double 100.0** ⇒ 传 10.0f 即 **+1000 存储 = +10 显示**、传 100.0f 即 **+10000 存储 = +100 显示**。
> 换句话说 `0x4aeea4`(10.0f) / `0x4aeaf0`(100.0f) 是**显示单位的增量**，弹窗上的 10/100 与它是同一个数（道具侧 `0x42e8ca` 那支把同一个 100 同时喂给弹窗与加法器，是最直接的对照）。
> ⚠ 附带订正：`0x42f510` **不是**「只被这里调用的私有函数」，它在道具侧另有 2 个调用点（`0x42e8d9` / `0x42eb55`），本行只提神灵侧是**不完整但不错误**。
> 详见 [`../../faith/th13/01-point-item-value.md`](../../faith/th13/01-point-item-value.md) §3.1 / §6。

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

普通脚本（以 script1 为例，一手字节，`thanm -l 13` 打印的**绝对时间**在右）：

```
-1:  alpha(0); scale(1.5,1.5); layer(6);
0:   scriptNew(script2); scriptNew(script3);     // 外环 + 内芯
+200:scaleTime(200, 0, 0.4, 0.4); stop(); interruptLabel(1);   // 时间基 = 200（+200 从 0 起算）
+40: delete();                                                  // 时间基 = 240（= 标签 200 + 40）
```

★ **可见寿命 = 480 + 40 = 520 帧（8.67 s @60fps）** ✅（2026-09-14 逐帧复核）：
`stop()` 把主时间线冻结在脚本时间 200（背景 VM `script0` 是同一用法：它靠 `stop()` 之后**仍然旋转**
证明"冻的是脚本推进、不是整个 VM"）⇒ 那条绝对时间 240 的 `delete()` **只有经中断才会被执行**；
引擎在第 480 帧发中断 1（`0x438a61`）⇒ 之后 40 帧删。两个数都要用上：
① 缩小发生在脚本时间 **200 → 400**（不是 0 → 200）；② 删除发生在 **480 + 40**。

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

#### 3.2b 全量重跑 + 「能不能给出一条规则」的结论（2026-09-23，113 的兜底档要用它）

> 动机：113（`TD_SPIRIT`）在 TH18 里对**没配原生掉落窗口**的敌人给了一个兜底档，owner 报
> 「神灵掉的**相对**数量偏多」。要判断"兜底能不能用一条**一手规则**算出来"，先把这张表读全。

**① 重跑（`local/vendor/thtk/build/thecl/thecl -d 13 -m local/vendor/eclmap/eclmap/th13.eclm`，21 个 `*.ecl`）**：

| 项 | 值 |
| --- | --- |
| 子程序总数 | **732** |
| 配了 `spec1` + `spec2` 的 | **59**（⇒ 其余 **673 个**敌人子程序在 TH13 里**一颗灵都不掉**：`0x419f48 cmp esi,ecx` / `jg` 直接跳过整段）|
| `N`（`spec2`）分布 | 1×12、2×12、3×5、4×6、5×6、6×2、8×2、10×9、16×2、20×2、40×1（**众数 = 1/2，中位 = 4，均值 = 5.7**）|
| `T`（`spec1`）分布 | 40×1、60×11、70×6、80×1、90×7、100×3、120×10、140×2、180×3、300×1、350×6、400×8（**众数 = 60，次 120**）|

**② 一线索：`N/T` 与"敌人档次"同向，但**没有闭式规则**。** 把每个配置项连同它自己的
`setHurtbox/setHitbox`（体型）与 `enmCreate` 调用点的 **`life` 实参（= HP）** 放在一起看：

| TH13 敌族（名字里的档） | 体型（hurt/hit） | 出怪 HP | `(N,T)` |
| --- | --- | --- | --- |
| `Phantom01/02/03/04` | 28×28 / 16×16 | 60–130 | `(1,40)`–`(3,100)` |
| `ECir00/01` | 24×24 / 16×16 | 100–200 | `(2,70)`–`(10,180)`（**同族跨关差异极大**）|
| `GirlA01..03` | 24–32 / 16–24 | 40–90 | `(1,60)`–`(4,120)` |
| `GirlB01..03` | 24–32 / 16–24 | 120–800 | `(2,120)`–`(8,180)` |
| `GirlC01/02` | 32×32 / 24×24 | 600–800 | `(4,120)`–`(10,400)` |
| `GirlD01..03` | 32×32 / 24×24 | 400–2000 | `(4,120)`–`(40,400)` |

- **体型**只有三档（hit 16 / 24），同一档里 `N` 从 1 跨到 40 ⇒ **体型不能当判据**；
- **HP** 的单调性**不成立**：`GirlD01` 在 st03 是 HP 900 / `(4,120)`，在 st06 是 HP 500 / `(16,350)`
  ⇒ `(N,T)` 是**逐关逐敌人手写**的，和 HP 没有函数关系（作者按关卡强度与敌人角色调的）；
- **掉落件数**也不是：`GirlD01@st07` `dropExtra(2,20)` 对应 `N=20`、`GirlB01@st03` `dropExtra(2,3)` 对应 `N=2`、
  `GirlA01@st04` `dropExtra(1,2)` 对应 `N=3` ⇒ 只是同向，**不是等值**。

⇒ **结论（🟡 → 可用于决策）**：TH13 的 `(T,N)` 是**逐敌人 ECL 常量**，**没有**"按 HP / 体型 / 掉落件数"的
一手公式可抄。任何把它搬到另一作（TH18）的兜底档**必然是映射**；能做的只是让这个映射
**落在 TH18 自己的原生档位分布上**（结论与落笔见 `mods/…/cards/trance_core.h` 的 `TRANCE_KILL_FB_*`
与 `TUNING.md` §2.0c）。

**③ 复核（本次逐条对的地址，一手）**：

| 项 | 地址 | 指令 |
| --- | --- | --- |
| 死亡掉落段 | `0x419f41` | `mov esi,[edi+0x2bc]`（timer）/ `mov ecx,[edi+0x3ff4]`（T）/ `cmp esi,ecx` + `jg 0x419f96`（过期 ⇒ **一颗都不撒**）|
| 步长与颗数 | `0x419f4f`–`0x419f64` | `idiv [edi+0x3ff8]`（= `T/N`）→ `(T − timer)/step` → `inc eax`（`+1`）|
| H/L 加成 | `0x419f65`–`0x419f6f` | `cmp [0x4be7c4],2` / `cmp …,3` ⇒ `inc eax` |
| 撒灵循环 | `0x419f79`–`0x419f94` | `call 0x438ec0`（`spawn_spirit`）× `n`（速度取常量 `0x4aedf8`）|
| `spec1` 写点 | `0x41f6c8` | `mov [edi+0x3ff4],eax`（opcode 1001）|
| `spec2` 写点 | `0x41f6d9` | `mov [edi+0x3ff8],eax`（opcode 1002）|

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

**2026-09-14 逐字节复核（本会话重跑，用于卡 113 的实跑对账）** ✅：

| 项 | 字节/地址 | 说明 |
| --- | --- | --- |
| 跳过 type 3/4 | `0x4391c5 cmp edi,3 / je 0x43925f`、`0x4391ce cmp edi,4 / je` | 直接 ret，**连窗口都不重置** |
| 窗口重置 | `0x439218 mov [ecx+0x8828],0x3c`、`0x439222 fstp [ecx+0x882c]`（`[0x4aedd0]` = 60.0f）、`0x439228 mov [ecx+0x8824],0x3b` | **每颗**重置 60 帧（不是"每杀"）|
| 计数 | `0x439232 inc [ecx+0x8838]` | 按**颗**（每颗非 3/4 的灵），且**不因新一颗而清零** |
| 阈值 | `0x439238 cmp [ecx+0x8838],0xa` / `0x43923f jl 0x439261` | 加完 ≥ 10（= 第 10 颗起**每一颗**都触发）|
| 奖励参数 | `0x439241 fld [0x4aedf8]`（= **0.5f**）、`push 1`（有声）、`push edx`（当前这颗的 pos）、`push 3`、`0x439250 call 0x438ec0` | 位置 = **掉落点**（不是自机脚下）|
| 窗口递减 | `0x438696 cmp [esi+0x8828],0` → `jle 0x438713`（那一支 `mov [esi+0x8838],0`）| 归零 ⇒ 计数清零 |
| 递减步长 | `0x4386ad mov [esi+0x8824],eax` 之后：乘数指针在 `+0x8830`（惰性初始化成 `0x4c0a28`，本会话读到 `*(float*)0x4c0a28 == 1.0f`），`0.99 < m < 1.01` 时按 1.0 扣、否则按 `m` 扣（`0x4ae9d8` = 0.99、`0x4ae9d0` = 1.01、`0x4ae860` = 1.0）| ZUN 的**帧率补偿**；正常速度就是每帧 −1 |

⇒ **60 帧内连掉 10 颗 → 之后每一颗都额外送一颗 type 3** ✅。type 3/4 自身不推进连锁。

> ★ **粒度要点（易误读）**：这一段长在 `spawn_spirit` 里 ⇒ 由「**掉**一颗灵」驱动，
> 与「玩家有没有吃到」**无关**，也与「杀了几个敌人」无关。把它挂到"收集"上会让
> 「连续吸灵」把 60 帧窗口一直撑住 ⇒ 白灵变成"每颗都配一颗"（卡 113 的 2026-09-14 订正，
> 见 `mods/th18.v1.00a/card-expand/AUDIT.md` §CM-1）。
> 另一条推论：**小怪自己不掉 type 3**（`st0X.ecl` 里小怪只丢 item 10/11/12）⇒
> 玩家看到的"白灵"在原版里**只可能来自这条连锁**（以及玩家死亡的通道 E）。

### 3.7 boss 相位的灵批次：**只认符卡相位结束** ✅（2026-09-14 新增，`thecl -d 13` 全量）

**规则**：灵批次（`dropExtra(10,·)` / `(12,·)` / `(13,N)`）写在**符卡刚结束**的那个非符相位的
**序言**里，且整段被 `unless ($TIMEOUT == 0) goto …` 包着：

```c
void Boss2() {                              // 非符相位
    ...
    spellEnd();                             // ← 上一张符卡在这里结束
    setInterrupt(0, 1500, 2100, "BossCard2");   // 本相位自己的出口 = 下一张符卡
    unless ($TIMEOUT == 0) goto Boss2_928;      // 超时 ⇒ 一颗不掉
    dropExtra(10, 1); dropExtra(12, 1); dropExtra(13, 20); dropItems();
Boss2_928: ...
}
```

| 情形 | 一手 | 证据 |
| --- | --- | --- |
| 符卡结束（击破，`$TIMEOUT == 0`）| **掉**（stage boss 22..47 颗）| `st01bs` 154/240/283、`st06bs` 169/287/413/639/704/751 |
| 符卡超时 | 一颗不掉 | 同一段的 `unless ($TIMEOUT == 0)` |
| **非符相位的结束** | **一颗不掉** | 各文件 `Boss1()` 序言没有任何 drop；符卡相位的序言也没有 |
| **stage boss 终符 `BossDead`** | **一颗不掉**（只有 P 与金钱）| `st01bs` 803、`st02bs`、`st03bs`… |
| 道中 boss 死亡（`MBossDead`）| **掉 5..25 颗**，与"上一相位是不是符卡"无关 | 6 个 `st0Xmbs`；`st04mbs` `MBoss1` 在 Easy 上 `setInterrupt(0,0,2700,"MBossDead")` |

**颗数表**（`dropExtra(10,·)+dropExtra(12,·)+dropExtra(13,·)` 之和；构成恒为
`1×type0 + 1×type2 + N×type3`）：

| 关卡 | stage boss（逐相位）| 道中 boss（`MBossDead`）|
| --- | --- | --- |
| st01 | 22 / 22 / 22 | 5 |
| st02 | 22 / 22 | 9 |
| st03 | 22 / 22 | 24（另有 `MBoss2` 序言 21）|
| st04 | 32 / 32 | 19（另有 `BossMiyako*Dead` 5/8/8/15/15/15）|
| st05 | 37 / 32 / 22 | 19 |
| st06 | 32 / 37 / 32 / 37 / 37 / 52 | 19 |
| st07（EX）| 47 ×9 | 25 |

⇒ TH18 卡 113 的落地（符卡门判据 = `th18:0x4cf2c0` 的 `zSpellcard+0x78` bit0）
见 `mods/th18.v1.00a/card-expand/AUDIT.md` §CM-2/§CM-3。

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
| 寿命 | `[EBX+0x60] == 480` 帧 → `anm_set_interrupt_by_id(vm, 1)` + 清 flag bit1 | `0x438a61`（`CMP [EBX+0x60],0x1e0` + `jne`）★ **判定读的是 `+0x60`**（本表早期把计时器写成 `+0x5c`）；中断后脚本再走 40 帧才 `delete()` ⇒ 可见寿命 **520 帧** |
| 出界删除 | 出 x∈[-224,224]、y∈[-32,480] → 槽置 0 并释放 VM | `0x438ad1` + `FUN_0040bb70(pos,0,224,448,512)` |
| 与自机子弹/擦弹 | **无交互**：神灵不参与擦弹，也不被子弹判定 | 全 exe 无「子弹 × SpiritItem」路径 🟡（证伪式结论）|

state 5（`0x438a42`）引用 `ENEMY_MANAGER_PTR->field_1c+0x40` 与 `FUN_00439550`，但**全函数没有任何写入 state=5 的地方** ⇒ 本 build 的死代码 🟡。

### 4.1 ★ 收集那一刻到底发生什么（2026-09-13 一手重读整段收集分支）✅

**发现**：`dist² < 400`（20 px）命中后的收集分支里**没有任何针对神灵本体的动画指令** ——
没有 `anm_set_interrupt_by_id`、没有换脚本、没有改 scale/alpha。它按 `spirit[1]`（type）分派后做：

| 步骤 | 一手 | 说明 |
| --- | --- | --- |
| ① 收集音 | `0x438b38` `SoundManager__play_sound_at_position`（`EDI=0x3f`）| 声像 = 神灵 x |
| ② 灵界槽 +2 | `0x438b45` `FUN_0043d530(&CURRENT_SPIRIT_GAUGE)`（`EAX=2`）| 所有 type 统一 +2 |
| ③ 奖励 | type 见 §2.2 表 | — |
| ④ **数字弹窗 VM** | `FUN_0046f820(&out, script, spirit, 0, -1, 0)`（`0x438b79` 一族）| 位置取**神灵自己的坐标**（第 3 参是 `spirit`）|
| ⑤ 覆盖槽 + 返回 | `FUN_00418330` + `return -1` | 上层**当场释放这个槽** |

**验证**（`FUN_00418330` `0x418330` 反编译，67 字节）：`*spirit = 0;` → `AnmManager__get_vm_by_id(spirit[0x16])`
→ 命中则 `vm[+0x1c] |= 0x20`，并对 `+0x560` 链上的子 VM 逐个置同一位 → `spirit[0x16] = 0`。
位 `0x20` 是"冻结/待回收"标记：`AnmManager__on_tick_1d__body` `0x46f1c7` 见到它就**跳过正常 tick**
改调 `AnmManager__sub_46f690` `0x46f690`（摘 4 条链 + 调 `+0x598` 上的析构回调 + 收子 VM + 归还池）。
⇒ **神灵本体是"当帧标记、下一帧回收"，视觉上就是瞬间消失**（没有淡出段）。

**结论**：玩家看到的"被吃掉"= **音 + 槽涨 + 一张数字弹窗**，不是神灵自己被吸进去的动画。
出界删除（`0x438ad1`）走的是同一个 `FUN_00470110`（同形，只是 id 取自 `spirit[0]` 而不是 `[0x16]`）。

**弹窗脚本（`front.anm`）**：

| type | 普通态 | 灵界态 | 贴图 | 内容 |
| --- | --- | --- | --- | --- |
| 0 | `0x60`=96 | `0x61`=97 | sprite66 / 67 | 命碎图标 + 1 / 2 |
| 1 | `0x66`=102 | `0x67`=103 | sprite72 / 73 | 最大得点图标 + 10 / 100 |
| 2 | `0x63`=99 | `0x64`=100 | sprite69 / 70 | 符卡碎图标 + 1 / 2 |
| 3 | — | — | — | **不起 VM**（改分数显示：`0x44ad60` + `0x413bf0`）|

脚本正文（96/97/99/100 逐字节相同，只有 `sprite()` 不同）：

```
sprite(spriteNN); ins_431(1); randMode(1); resampleMode(1); anchor(0, 2); layer(11);
scale(1.0, 0.0); scaleTime(4, 0, 1.0, 1.0); posTime(50, 1, 0.0, -16.0, 0.0);
0: wait(46); scaleTime(4, 0, 1.0, 0.0); +4: delete();
```

⇒ **寿命 54 帧**（50 帧上浮 16 px + 4 帧纵向收平）、从扁平撑开。
"普通 / 灵界"由 `spirit+0x84` bit0 决定（**生成时**是否处于灵界，`0x438fde`），不是收集时刻的状态。
⚠ 对照：TH18 自己的同类弹窗（`front.anm` script85..99，同形同层 13）贴图是 **48×16 / 64×16**，
正好是 TH13 这两档（24×8 / 32×8）的 **2 倍** —— 见 `engine/anm/th18/01-vm-instantiate.md` §3b。

## 5. 灵界槽（`CURRENT_SPIRIT_GAUGE` `0x4be808`）✅

| 项 | 值 | 出处 |
| --- | --- | --- |
| 上限 | **600** | `0x405815`（`CMP …,0x258`）、`0x43d530` 里夹到 600 |
| 分档 | **每 200 一格**（共 3 格）| `0x43d530` 的 `iVar2 = 旧值/200` 与 `新值/200` 比较 |
| 收神灵 | **+2**（所有类型）；type 3 再 +1 | `0x438b3d` / `0x438c3d` |
| 升格反馈 | 每跨一格播音 `0x44` = **`se_astralup.wav`**（§6.4 表）+ 在自机上方 32px 生成效果 `0x6b+格数`（107/108/109）| `0x43d530`（`0x43d574` 播，走**无声像**的 `0x460bc0`）|
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

满足后播**音效 `0x34` = `se_boon01.wav`**（`0x40586d MOV EDI,0x34` + `0x40587a CALL 0x460c50`；位置 = 自机 x，
`0x405867 fld [PLAYER+0x5b8]`）并调 `0x405a20(this, 0)`。
⚠ 这个音在 `TranceManager::on_tick` 里、**不在 `begin` 里面** ⇒ **死亡触发的那次进入不播音**（见 §6.2 / §6.4）。
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
    播音效 0x42（se_pldead01，0x405bf8 MOV EDI,0x42 + 0x405c05 CALL 0x460c50，位置 = 自机 x）
      + 在自机处生成效果 0x1a;
    if (PLAYER_PTR) FUN_00444a10();     // 灵界结束 → 真死（再走一次 Player::die）
}
… 恢复玩家 anm / BGM …
if (!(this+0x14 & 2)) { CURRENT_SPIRIT_GAUGE -= 200; if (<0) = 0; }   // 手动灵界才扣 200
                                        // ★ 这条分支里**一个 play_sound 都没有** ⇒ 手动结束是静音的
DAT_004be80c = 0;  FUN_00405440(0x46);   // ★ 订正：**不是音效** —— 0x405440 = Player::set_invulnerability(frames)
                                        //   （设 player+0x14684 的 zTimer）⇒ 0x46 = **70 帧 ≈1.17 s 无敌**
                                        //   一手见 engine/spirit/th13/05-trance-attack.md §4
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
| **自机攻击方式改变**（主炮 shooterset 组号被硬换成常量 **10**）| `0x446006`/`0x446036`；逐角色表见 [05-trance-attack.md](05-trance-attack.md) |
| HUD 显示剩余 `%.2fs` | `TranceManager__on_draw__body` `0x405940` |
| **没有**：伤害倍率、清弹、自动收集、得分倍率 | 全 exe 无对应读点 🟡（证伪式）|

### 6.4 音效表（id → `se_*.wav`，一手直读 exe）✅

本表此前本仓没有（2026-09-13 补）：TH13 的 cfg 表 `0x4bad94`（**71 行** × 0x14，`+0x00` = **名表下标**）+
名表 `0x4bb320`（60 项）⇒ **`id → wav = 名表[ cfg13[id] . +0x00 ]`**（行号就是 id：loader `0x460090` 起
行/槽同步推进，slot 数组 `0x4e0c28` 紧跟其后的 blob 数组 `0x4e12d0` 正好是 71 × 0x18）。
⚠ 与 TH18 **不同源结构**：TH18 的 `+0x00` 是「槽号 = id」的置换、`+0x04` 才是名表下标 ⇒ **两作的 id 不能互相照抄**，
只能按 wav 名字对齐（跨作对齐表见 `engine/_shared/th18-sound-table.md` §10）。

「备注」列：★ = 本节正文用到的语义；其余给出静态调用点（`MOV EDI/EDX,id` + `CALL 0x460c50`/`0x460bc0`，
全量扫描所得，`0x460bc0` 是无声像参数的短版）。

| id | wav | 备注 / 一手站点 |
| --- | --- | --- |
| `0x00` | `se_plst00.wav` | 菜单/标题（3 处） |
| `0x01` | `se_plst00.wav` |  |
| `0x02` | `se_enep00.wav` | 静态站点 `0x44de8d` |
| `0x03` | `se_enep00.wav` |  |
| `0x04` | `se_pldead00.wav` |  |
| `0x05` | `se_power0.wav` |  |
| `0x06` | `se_power1.wav` |  |
| `0x07` | `se_tan00.wav` | 静态站点 `0x42dd31`、`0x43e93e`、`0x43ea73` |
| `0x08` | `se_tan01.wav` |  |
| `0x09` | `se_tan02.wav` | 静态站点 `0x42dbc1`、`0x42dddb`、`0x43ed1c` |
| `0x0a` | `se_ok00.wav` | 静态站点 `0x42db57`、`0x42dcdc`、`0x42dd0d` |
| `0x0b` | `se_cancel00.wav` |  |
| `0x0c` | `se_select00.wav` |  |
| `0x0d` | `se_gun00.wav` | 静态站点 `0x42e8fe`、`0x42e943`、`0x42e996` |
| `0x0e` | `se_cat00.wav` |  |
| `0x0f` | `se_tan00.wav` | 玩家射击 |
| `0x10` | `se_lazer00.wav` |  |
| `0x11` | `se_lazer01.wav` | 静态站点 `0x43d393`、`0x43d416`、`0x4538aa` |
| `0x12` | `se_enep01.wav` |  |
| `0x13` | `se_damage00.wav` |  |
| `0x14` | `se_nodamage.wav` | `Player::on_hit` 的中弹音（`0x446ee6`） |
| `0x15` | `se_item00.wav` |  |
| `0x16` | `se_tan00.wav` |  |
| `0x17` | `se_tan01.wav` |  |
| `0x18` | `se_tan02.wav` |  |
| `0x19` | `se_kira00.wav` |  |
| `0x1a` | `se_kira01.wav` |  |
| `0x1b` | `se_kira02.wav` | 静态站点 `0x40b2c2`、`0x40b37b`、`0x40b812` |
| `0x1c` | `se_timeout.wav` |  |
| `0x1d` | `se_graze.wav` |  |
| `0x1e` | `se_powerup.wav` | 力量道具 |
| `0x1f` | `se_graze.wav` |  |
| `0x20` | `se_kira00.wav` |  |
| `0x21` | `se_pause.wav` | 静态站点 `0x4135d0` |
| `0x22` | `se_cardget.wav` | `0x41a2e8` |
| `0x23` | `se_damage01.wav` | 敌机受击 `0x41a2ca` |
| `0x24` | `se_timeout2.wav` | 静态站点 `0x419d22` |
| `0x25` | `se_invalid.wav` | 静态站点 `0x42eb77` |
| `0x26` | `se_slash.wav` |  |
| `0x27` | `se_slash.wav` |  |
| `0x28` | `se_ok00.wav` |  |
| `0x29` | `se_ch00.wav` |  |
| `0x2a` | `se_ch01.wav` |  |
| `0x2b` | `se_extend.wav` |  |
| `0x2c` | `se_cardget.wav` | 拾卡/收点（`0x40a46e`、`0x40cd34`） |
| `0x2d` | `se_nep00.wav` |  |
| `0x2e` | `se_bonus.wav` | 静态站点 `0x413b93`、`0x43d457`、`0x43d4b7` |
| `0x2f` | `se_bonus2.wav` |  |
| `0x30` | `se_enep02.wav` |  |
| `0x31` | `se_lazer02.wav` | 常驻激光音（4 处；**循环标志 = 1**） |
| `0x32` | `se_boon00.wav` | 静态站点 `0x451b49`、`0x452a0a` |
| `0x33` | `se_don00.wav` |  |
| `0x34` | `se_boon01.wav` | ★ **灵界进入** `0x40586d` |
| `0x35` | `se_boon01.wav` |  |
| `0x36` | `se_ch02.wav` |  |
| `0x37` | `se_ch03.wav` | 常驻音（`0x4461eb`；**循环标志 = 1**） |
| `0x38` | `se_extend2.wav` |  |
| `0x39` | `se_pin00.wav` |  |
| `0x3a` | `se_pin01.wav` |  |
| `0x3b` | `se_lgods1.wav` | ★ **神灵出生 type 0** |
| `0x3c` | `se_lgods2.wav` | ★ **神灵出生 type 1**（击破掉灵也走它：通道 A/B 的 `type = 1`） |
| `0x3d` | `se_lgods3.wav` | ★ **神灵出生 type 2** |
| `0x3e` | `se_lgods4.wav` | ★ **神灵出生 type 3**（含连锁奖励通道 F） |
| `0x3f` | `se_lgodsget.wav` | ★ **神灵收集** `0x438b38`（另见 `0x438cf5`、`0x422ba1`） |
| `0x40` | `se_msl.wav` | 导弹 |
| `0x41` | `se_msl2.wav` |  |
| `0x42` | `se_pldead01.wav` | ★ **灵界结束（死亡触发那条）** `0x405bf8` |
| `0x43` | `se_heal.wav` | `0x422bb5` |
| `0x44` | `se_astralup.wav` | ★ **灵界槽跨格** `0x43d574`（**TH18 无此 wav**） |
| `0x45` | `se_fault.wav` |  |
| `0x46` | `se_noise.wav` | ⚠ 曾被误记成「灵界结束音」—— `0x405d29 push 0x46` 其实是 `Player::set_invulnerability(70)` |

两条容易记错的：

- **灵界结束没有"结束音"**：`0x405d29 push 0x46` 是 `Player::set_invulnerability(70)`（见 §6.2）；
  手动结束**一个音都不播**，只有死亡触发那条在 `0x405bf8` 播 `0x42`（`se_pldead01`）之后才 `Player::die`。
- **死亡触发的灵界进入也不播音**：`Player::die` `0x444a10` 只调 `TranceManager::begin(…, 1)`；
  玩家听到的"呜"是 `Player::on_hit` `0x444ca0` 自己播的 id **2**（`0x444ccc lea edx,[ebx+2]` + `CALL 0x460bc0`，
  ebx=0；§7 写的"音效 2"✅ 对）—— 而 **id 2 在 TH13 是 `se_enep00.wav`**（见上表）、
  **在 TH18 是 `se_pldead00.wav`**（TH18 `Player__die` `th18:0x45d3b7` 也是 id 2）
  ⇒ 这是"两作 id 空间不同"的最好例子：**同一个数字、两枚不同的文件**。

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
- ★ **本条已在本仓被用作 TH18 侧 id 113（灵界）的实装依据**（2026-09-12 第二版用户定案：
  「在确定中弹后，取消掉死亡，进入灵界状态；灵界状态退出后，重新死亡」，且「113 一定在救援层的**最后**出发、
  甚至比决死 B 还靠后」）⇒ TH18 落点与一手证据见
  [`mods/th18.v1.00a/card-expand/AUDIT.md`](../../../mods/th18.v1.00a/card-expand/AUDIT.md) §BN 与
  [`EVENTS-CONTRACT.md`](../../../mods/th18.v1.00a/card-expand/EVENTS-CONTRACT.md) §5.4。**本文正文不改**。

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
