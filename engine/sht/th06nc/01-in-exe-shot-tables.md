# TH06NC：烧在 exe 里的自机火力表

> **版本**：TH06NC（`th06nc.exe`，MD5 `b0cc28ec904e9efce2439ad857c938b5`，
> imagebase `0x140000000`）。本文裸地址默认属该版本；引用原版一律写成 `th06:0x…`。

状态：表的定位与字段图 ✅ 一手（NC 反汇编 + 表内容 + 量纲常识三边闭合）；
与 TH06 原版的逐字段比对 ✅ 一手（两个 exe 各自读出来直接比）；
`bulletType` 0/1/2 的**行为** ✅（CC0 源码 + NC 常量双边一致），**名字**是我们的命名选择 🟡。

一句话：**NC 的自机数据就是 TH06 原版的那一份**——193 条弹记录逐字段比对，
除四类系统性差异外一字不差；四类里只有**一条**影响玩法（满火力档的阈值 `127 → 128`），
外加**一处原版疏漏被补上**（ReimuA 火力 16–31 档原本没有射击音效）。

---

## 1. 先说清楚：这一作没有 `.sht`

`engine/sht/OVERVIEW.md` 开头那句「SHT 是纯二进制配置文件」是 **TH07 以后**的事。
三条独立佐证：

- `sht-webedit`（社区最完整的跨版本 sht 工具）的 struct 表**最早是 `struct_07.js`**，
  没有 `struct_06.js`。
- `thtk` 与 `truth` **完全不支持 sht**（见 [`../format-reference.md`](../format-reference.md) §0）。
- 样本自己：`local/th06nc/extracted/` 解出 125 个 `.anm`、7 个 `.std`、7 个 `.ecl`、
  7 个 `msg*.dat`……**`.sht` 零个**。有测试钉着这条。

所以这一作的自机数据是**一组 exe 内静态表**。子系统还是同一个（自机射击配置），
只是载体不同——这正是本仓「子系统为主、版本为辅」要表达的东西。

## 2. 三层结构

```
g_CharData[4]                       0x1403de3b0，步长 0x20
  ├ f32 orthogonalMovementSpeed / …Focus / diagonal… / …Focus
  ├ ptr fireBulletCallback
  └ ptr fireBulletFocusCallback
        ↓ 四个 26 字节转发桩，各自把一张表的地址交给 0x14006a580
g_CharacterPowerData<X>[9]          九档
  { i32 numBullets; i32 powerThreshold; CharacterPowerBulletData *bullets; }   步长 0x10
        ↓
CharacterPowerBulletData[numBullets]                                          步长 0x24
```

`g_CharData` 的四项（一手读出来的值）：

| idx | 自机型 | 正交速度 | 低速 | `fireBulletCallback` | `…FocusCallback` |
| ---: | --- | ---: | ---: | --- | --- |
| 0 | ReimuA | 4.0 | 2.0 | `0x14006a840` | **同一个** |
| 1 | ReimuB | 4.0 | 2.0 | `0x14006a860` | **同一个** |
| 2 | MarisaA | 5.0 | 2.5 | `0x14006a880` | **同一个** |
| 3 | MarisaB | 5.0 | 2.5 | `0x14006a8a0` | **同一个** |

> **两个回调指向同一个函数**——所以 TH06 系的「聚焦」**不换射击表**，
> 只改移动速度。这与 TH16/TH18 的 SHT 按「火力档 × 聚焦」选 shooterset 不同，
> 见 [`../OVERVIEW.md`](../OVERVIEW.md) 的矩阵。

## 3. ★ `powerThreshold` 是开区间上界，不是「本档的火力值」

`Player__FireBulletFromTable` `0x14006a580` 的开头就是选档：

```asm
14006a5a1  MOVZX EAX,word ptr [0x1404f1e88]   ; currentPower，**按 word 读**
14006a5ae  CMP  EAX,dword ptr [RBX + 0x4]     ; tier->powerThreshold
14006a5b1  JL   0x14006a5bc                   ; power < 阈值 -> 就是这一档
14006a5b3  ADD  RBX,0x10                      ; ++tier
14006a5b7  CMP  EAX,dword ptr [RBX + 0x4]
14006a5ba  JGE  0x14006a5b3                   ; while (power >= 阈值)
14006a5bc  MOV  RSI,qword ptr [RBX + 0x8]     ; tier->bullets
14006a5c0  LEA  RDI,[R12 + R12*0x8]           ; idx * 9
14006a5c9  CMP  byte ptr [RSI + RDI*0x4 + 0x1f],0x3   ; idx*0x24，+0x1f = bulletType
```

即 `while (currentPower >= tier->powerThreshold) tier++`。于是第 i 档实际覆盖
`[前一档阈值, 本档阈值)`：

| 档 | 阈值 | 实际生效的火力区间 |
| ---: | ---: | --- |
| 0 | 8 | 0–7 |
| 1 | 16 | 8–15 |
| 2 | 32 | 16–31 |
| 3 | 48 | 32–47 |
| 4 | 64 | 48–63 |
| 5 | 80 | 64–79 |
| 6 | 96 | 80–95 |
| 7 | **128** | 96–127 |
| 8 | **999** | **128（满火力）** |

九档不重不漏地铺满 0–128（有测试逐档验缝）。`LEA RDI,[R12+R12*8]` 再 `RDI*4`
= **步长 `0x24`**，与 CC0 的 `ZUN_ASSERT_SIZE(CharacterPowerBulletData, 0x24)` 对上。

> ⚠️ 本仓 `th06nc_player_spec.py` 一度写成
> `POWER_LEVELS = (8, 16, 32, 48, 64, 80, 96, 128)` 并叫它「档位」——**两处都错**：
> 漏了第 9 档，而且把开区间上界当成了档位火力。已更正为 `POWER_THRESHOLDS`（9 项）。

**这个 `while` 没有边界检查**（与 TH06 一样）。火力上限 128 < 999，所以走不出去；
但任何把火力改到 ≥ 999 的 mod 都会让它冲出表尾。

## 4. 弹记录：`CharacterPowerBulletData`（`0x24` 字节）

x86 与 x64 下**大小相同**（没有指针成员），所以两作可以直接按字节比。

| 偏移 | 字段 | 说明 |
| --- | --- | --- |
| `+0x00` | `i16 waitBetweenBullets` | 非激光：发射间隔；**激光：冷却帧数** |
| `+0x02` | `i16 bulletFrame` | 非激光：间隔里的相位；**激光：`laserTimer` 槽号（0/1）** |
| `+0x04` | `f32[2] motion` | 相对生成点的偏移 |
| `+0x0c` | `f32[2] size` | 激光的 y 是 `480` = 一屏高 |
| `+0x14` | `f32 direction` | 弧度。**样本里 193 条全是整度数** |
| `+0x18` | `f32 velocity` | |
| `+0x1c` | `u16 damage` | |
| `+0x1e` | `u8 spawnPositionIdx` | `0` = 自机中心；`1`/`2` = `orbsPosition[idx-1]`，即两个子机 |
| `+0x1f` | `u8 bulletType` | 见 §5 |
| `+0x20` | `i16 anmFileIdx` | |
| `+0x22` | `i16 bulletSoundIdx` | `-1` = 不发声 |

> **「全是整度数」是最便宜的一道量纲常识闸门**：`-90/-120/-97/-83/-110/-130/-170`……
> 字段偏移只要错一个字节就不会这么整齐。有测试守着。

## 5. `bulletType`：CC0 只命名了一个，另外三个我们补上

CC0 的 `Player.hpp` 写的是 `BULLET_TYPE_0 / _1 / _2 / _LASER`——**0/1/2 没命名**。
行为在 CC0 的 `Player.cpp` 里写得很清楚，四个常量也都在 NC 里一手复核过：

| 值 | 暂定名 🟡 | 行为 | NC 侧一手证据 |
| ---: | --- | --- | --- |
| 0 | `STRAIGHT` | 逐帧不改速度，直飞（CC0 的 switch 里没有它的 case） | — |
| 1 | `HOMING` | 开火后 **40 帧**内朝 `positionOfLastEnemyHit` 转向，速度钳 **10**、下限 1；没目标时每帧 **+1/3** 加速到 10 | `0x1400698c4` 的 `COMISS` 拿 `-100.0f`（`0x14030d0f0`）比 `lastEnemyHit.x`；`0x1400698d1` 的 `CMP EAX,0x28` 就是那 40 帧；钳位 `10.0f` 在 `0x14030cea0` |
| 2 | `RISING` | 每帧 `velocity.y -= 0.3f`，越飞越快地上冲 | `0x1400698a3` 的 `SUBSS XMM0,XMM12`，`XMM12` = `0.3f`（`0x14030cd40`） |
| 3 | `LASER` | **CC0 唯一命名的**。占 `laserTimer[bulletFrame]`（`Player+0x79e8`，步长 8），`waitBetweenBullets` 是冷却 | `0x14006a5c9` 的 `CMP byte ptr [RSI+RDI*4+0x1f],0x3` |

**每种特殊弹型只属于一个自机型**，与设定对得上（有测试钉着）：

| 弹型 | 只出现在 | 与设定 |
| --- | --- | --- |
| `HOMING` | ReimuA | 霊符・夢想封印 = 诱导 ✅ |
| `RISING` | MarisaA | 魔符 = 上冲的星 ✅ |
| `LASER` | MarisaB | 恋符 = 激光 ✅ |

> 🟡 **名字是我们的命名选择**，行为是 CC0 写明的。标 🟡 是因为社区（CC0 本身）
> 没给它们起名，我们不能把自己的用词说成「社区共识」。

## 6. ★ 与 TH06 原版逐字段比对：193 条，四类差异

TH06 的四张表由 `th06:0x4260d0` / `th06:0x426420` / `th06:0x426450` / `th06:0x426480`
四个桩 `PUSH` 的立即数给出（`0x476f78` / `0x477770` / `0x477ed8` / `0x478468`）。
两边各自按 §4 的布局读出来直接比：

```
$ python3 tooling/ghidra/th06nc_sht_dump.py --diff
共比对 193 条弹记录（4 自机型 x 9 档）
```

| # | 差异 | 处数 | 影响玩法？ |
| ---: | --- | ---: | --- |
| 1 | `anmFileIdx` 全部 **+32** | 193 / 193 | ❌ 纯 ANM 脚本重编号 |
| 2 | 第 7 档阈值 **`127` → `128`** | 4（四个自机型全部） | ✅ **唯一的玩法差异** |
| 3 | `direction` 差 **1 ULP** | 46 | ❌ 编译期浮点求值路径不同 |
| 4 | `bulletSoundIdx` `-1` → `0` | 1 | ✅ **补上了原版的一处疏漏** |
| 5 | 其它 | **0** | — |

**除此之外逐字节相同。** 五类里第五类必须为空，有测试守着。

### 6.1 阈值 `127 → 128`：唯一的玩法差异

配合 §3 的 `while (power >= 阈值)`：

| | 满火力档（第 8 档）从哪个火力开始生效 |
| --- | --- |
| TH06 原版 | 阈值 127 → 火力 **127** 就进满火力档 |
| TH06NC | 阈值 128 → 要满 **128** 才进 |

也就是说 **NC 把「差 1 点火力也算满火力」这个行为收紧了**。
TH06 的火力上限就是 128，所以受影响的只有 `power == 127` 这一个点。

> 🟡 **未实机验证。** 静态推理链完整（阈值字节 + 比较指令方向都一手读过），
> 但没有在游戏里停在 127 火力上数过弹幕。

### 6.2 `direction` 的 1 ULP：不是数据改动

46 处全部**恰好差 1 ULP**，方向有正有负。拿 `float(radians(整度数))` 当基准去比，
**两边各占一半**（TH06 更近 25 处、NC 更近 20 处、还有一处两边都不精确）——
所以**不是谁更准**，是两个编译器在编译期把同一批整度数常量求值的路径不同。

举例：`-70°` TH06 是 `0xbf9c61aa`、NC 是 `0xbf9c61ab`；
`-110°` 反过来，TH06 `0xbff5be0c`、NC `0xbff5be0b`。
量级 10⁻⁷ 弧度，**玩法上不可能有差别**。

### 6.3 那处疏漏：ReimuA 火力 16–31 档原本没有射击音

把四个自机型 × 九档共 36 档逐档看「本档有没有任何一条 `bulletSoundIdx >= 0`」：

| | 结果 |
| --- | --- |
| TH06 原版 | 36 档里 **35 档有**，只有 **ReimuA 第 2 档（火力 16–31）整档没有** |
| TH06NC | **36 档全有** |

NC 把那条记录的 `bulletSoundIdx` 从 `-1` 改成了 `0`。
**这看起来是 ZUN 原版的一处疏漏，NC 顺手补上了。** 有测试正反两面各钉一条。

> 🟡 **未实机验证**——没在原版里停在 16 火力上听过。但 35:1 的分布本身就很说明问题。

## 7. 下一步

1. 🟡 **实机验证两条**：火力 127 时 NC 与原版的弹幕是否不同；原版 ReimuA
   火力 16–31 是否真的没有射击音。和别的几条 🟡 一起攒一次 Windows。
2. ❓ **`g_GameManager` 的结构体基址**。目前只知道三个字段的绝对地址：
   `0x1404f1e80` 角色、`0x1404f1e81` 机型、`0x1404f1e84` 关卡号、`0x1404f1e88` 火力。
3. ❓ **`orbsPosition`（两个子机的位置）怎么算**——`spawnPositionIdx` 1/2 指向它，
   本轮只看到消费侧。
4. ❓ 把这张表接进面向 THBWiki 的 [`../WIKI-DRAFT.md`](../WIKI-DRAFT.md)：
   TH06 期需要单开一节说明「这一作没有 `.sht`」。

## 8. 复现

```sh
python3 tooling/ghidra/th06nc_sht_dump.py                 # NC 全表
python3 tooling/ghidra/th06nc_sht_dump.py --version th06  # 原版全表
python3 tooling/ghidra/th06nc_sht_dump.py --diff          # 逐字段比对
python3 -m unittest discover -s tooling/ghidra/tests      # 含数据复核（9 条）

source tooling/env.sh
python3 tooling/ghidra/apply_th06nc_sht.py --dry-run
"$JAVA_HOME/bin/python" tooling/ghidra/scripts/dump_func_asm.py th06nc 0x14006a580
"$JAVA_HOME/bin/python" tooling/ghidra/scripts/dump_func_asm.py th06.v1.02h 0x4260d0
```
