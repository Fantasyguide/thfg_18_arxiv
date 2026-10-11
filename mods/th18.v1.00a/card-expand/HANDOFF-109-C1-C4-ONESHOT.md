# HANDOFF-109-C1-C4-ONESHOT：TH19 自机 C1–C4 的**一手逐条规格**（灵梦 / 魔理沙 / 早苗）

> **版本**：TH19 v1.10c（`th19.exe` md5 `dd0e1ff8c02327bf6408f763b8beedc9`，imagebase `0x400000`）。
> 本文件裸地址一律属 th19 v1.10c；引用其它版本必须写 `th18:0x…` 形式。
> **范围**：`pl00`（灵梦）/ `pl01`（魔理沙）/ `pl02`（早苗）三人的 C1–C4。
> ★ **不含**：C3/C4「发到敌方半场」的内容（owner 2026-10-11 明确本轮不管）；咲夜/TH09 完全不在范围。
> **一手面**：`local/th19.v1.10c/decomp/all.c` + `_ext/pl0{0,1,2}.txt`（anm）+ `_ext/ecl/pl0{0,1,2}.txt`（ECL）
> + `_scratch/sht19/pl0{0,1,2}.sht`（弹表，二进制直读）+ 指令级反汇编（`local/_scratch/thdis.py`）。
> **相关**：[`HANDOFF-109-C1-C4.md`](HANDOFF-109-C1-C4.md)（概念规格）· [`TUNING.md`](TUNING.md)。

---

## 0. 结论速览（本文件要回答的六件事）

| 档 | 一手载体 | 本侧产物 | 是否随角色变 |
| --- | --- | --- | --- |
| **C1** | `EX<角色>Inf` 槽 3（初始化）+ 槽 11（每帧发射） | 自机弹（`plNN.sht` **set10**） | ★ **是**（弹表 + anm + tick 全不同） |
| **C2** | `EX<角色>Inf` 槽 4 | 灵梦 = `EXReigekiInf`；魔理沙 = 6×`EXLaserInf`；早苗 = 弹表 **set11** | ★ **是**（完全不同） |
| **C3** | 槽 5 = `0x50d8a0`（送对侧）+ 本侧 `Ecl_EtBreakLv3` | 本侧 = 消弹/画环（`default.ecl`，三人**同一份**）+ 回罩 + 计数 | ✗ 否（**本侧**三人相同） |
| **C4** | 槽 6 = `0x50d800`（送对侧）+ 本侧 `Ecl_EtBreakLv4` | 同上（Lv4 半径序列更长） | ✗ 否（**本侧**三人相同） |

★ **三人共用 C1 的弹表索引 = set10**，但**弹表内容、anm 脚本、tick 函数全不同**（§2.1 / §3.1 / §4.1）。
★ **`plNN.sht` 的档位阈值**（`+0x2c..+0x38` ×10）：三人都是 **500 / 1000 / 1700 / 2500**（§1.3 表）。

---

## 1. 共用层（一手；三人相同）

### 1.1 四档分派：蓄力机 `0x4e79e0`（`AuraCtrlInf` 每帧）

记 `S` = `*(record + 0x2c)`（自场地状态块），`charge` = `*(aura + 0x94)`。

| 档 | 判据（一手） | 动作 |
| --- | --- | --- |
| **C1** | `charge >= clamp(S+0x28, 0, 2500)` | 调 `EX` 对象 **槽 3**（虚表 `+0xc`），实参 = **自机坐标指针** `player+0x68c` |
| **C2** | `charge < S+0x34` 且 `charge >= clamp(S+0x2c)` | 调 **槽 4**（`+0x10`）；仅当其返回 `0` 时才继续 |
| **C3** | `charge >= S+0x34` 且 **槽 9**（`+0x24`）返回 **非 0**；或 `S+0x30 <= charge < S+0x34` | 见 §1.4 |
| **C4** | `charge >= S+0x34` 且 **槽 9** 返回 **0** | 见 §1.4 |

- 地址：C1 判据与调用 `0x4e841c`–`0x4e843b`；C2 判据 `0x4e82ce`–`0x4e82fb` 与 `0x4e8301`–`0x4e8336`；C3/C4 分岔 `0x4e811d` 附近（decomp `all.c:186620`）。
- 阈值取值助手：`FUN_0040b760` = `S+0x28`、`FUN_0040b720` = `S+0x2c`、`FUN_0040b6e0` = `S+0x30`、`FUN_0040b6a0` = `S+0x34`（均 clamp `[0,2500]`）。
- ★ **修正**：`S+0x38`（C3 档，clamp `[0,7]`）在 C3 分支里 `+1`（`all.c:186667`）；**C3/C4 的"次数"计数器另有其人**：
  `S+0x4c`（C3，`0x50e820`）与 `S+0x50`（C4，`0x50e7d0`），都 clamp `[0,999]`；C2 的是 `S+0x48`（`all.c:186567`）。

### 1.1b 释放状态机：C3/C4 是否**同时**放 C1（★ owner 质疑，一手核实）

- 字段：`aura+0x2c` = 蓄力按键计数（`>= 1` = 按住）、`aura+0x28` = 蓄力计时器、
  `aura+0x14/+0x18/+0x1c` = **释放计时器**（Timer 三元组 prev / cur / fp）。
- `0x4e7a7b cmp [ebx+0x2c],1 ; jge 0x4e7c12` ⇒ 按住 → 蓄力分支（`0x4e7c12`…`0x4e80da`）；松开 → 释放路径（`0x4e7a8e`…）。
- `0x4e7abb mov eax,[ebx+0x18]` + `0x4e7ac0 test eax,eax ; jle 0x4e857d` ⇒ **只有 `+0x18 >= 1` 才走「放炮」那一整块**（`0x4e7ac6`…）。
- `0x4e857d jns 0x4e86e5`（`+0x18 == 0` ⇒ 本帧到此为止）；`+0x18 < 0` ⇒ `0x4e8583 call 0x4026c0`（释放计时器前进）。
- 蓄力分支里 `0x4e7cb2 call 0x4026c0` 推进的是**同一个**计时器 ⇒ 按住期间 `+0x18` 从 −10 一路涨到 `>= 1`。
- 放炮块末尾 `0x4e8481`（写 0）/`0x4e8499`（写 `set_value(-10)`）⇒ ★ **每次释放只放一次**（放完进 10 帧冷却；只有再按住键才可能回到 `>= 1`）。

★ **C1 那一支在放炮块的最尾部、对全部档位都执行**：C2/C3/C4 三条路都汇到 `0x4e83b9`（ECL 调用在 `0x4e83e3`），
随后**顺序落到** `0x4e83e8`（`S+0x28` clamp）→ `0x4e841c`（`charge >= 阈值`）→ `0x4e843b call [esi+0xc]` = **EX 槽 3**；
C2 档在 `0x4e82e2 jl 0x4e83e8` 时也直接进同一处。

| 释放时 charge | C4 判据 `S+0x34` | 本帧额外发生（**除「C1 一轮」之外**） |
| --- | --- | --- |
| `< S+0x28`(500) | — | 无（`0x4e841c` 不满足 ⇒ 什么都不放） |
| `[500, 1000)` | — | 无（`0x4e82e2 jl 0x4e83e8` ⇒ 只做 C1） |
| `[1000, 1700)` | — | EX 槽 4（角色 C2）+ `Ecl_EtBreakLv2`（`0x4e82c9 jmp 0x4e83b9`） |
| `[1700, 2500)` | `<` | 回罩 + `0x50e820`（`S+0x4c++` / 送 ExtraAttack）+ `S+0x38++` + `Ecl_EtBreakLv3` |
| `>= 2500` 且槽 9 ≠ 0 | `>=` | 同上（`0x4e7b1b jne 0x4e811d`） |
| `>= 2500` 且槽 9 = 0 | `>=` | 回罩 + `0x50e7d0`（`S+0x50++` / 送 BossAttack）+ 扣 `C4−C1` + `Ecl_EtBreakLv4` |

★ **一处例外（早苗）**：早苗的槽 3（C1 处理器 `0x418c90`）开头是 `cmp [ecx+0xdc],0 ; jne 0x418d10`（**直接返回**），
而槽 4（C2）已经把 `+0xdc` 置 1 ⇒ **C2 帧里紧随其后的槽 3 调用是空操作**（否则会把 `+0xd8` 从 11 覆写回 10）。
⇒ 对早苗：C2 释放**不会**再叠一份 set10；灵梦/魔理沙的槽 3 没有这个守卫，C1 那一支照常生效。

★ **判定：owner 的「C3 C4 还会至少放一份 C1」= 成立** ✓（灵梦 / 魔理沙 ✓；早苗的 C2 帧被 `+0xdc` 守卫吃掉，但 C3/C4 帧照放）。
不是"每帧"也不是"每阶段一次"的含糊说法 —— **每次释放恰好跑一帧放炮块**（放完即把释放计时器置 −10），
而 C1 那一支就在**同一次释放的同一帧、无条件**被评估（唯一门槛是 `charge >= clamp(S+0x28)`）。
⇒ 卡 109 落地时：**任何一个档位的释放都必须同时放一份 C1**（对应现状的"C1 只在 500 上升沿放"要改成"随释放一起放"）。

### 1.2 C1 的发射链（三人同一条链）

```
C1 → EX 槽 3（快照自机坐标，按角色可能改 EX+0xd8/set 号）
   → EX 槽 11（每帧；受 EX+0x8c 计时器窗口限制）
   → FUN_0054f990(this = player+0x22f8, arg1 = timer, arg2 = 0, arg3 = set)
   → FUN_0054e960((set<<8)|row, …)   ← 每个命中行生成一个「自机攻击体」
```

- `FUN_0054f990` `0x54f990`：**遍历 set 的全部行**；每行 `row[0]`（i8，`+0x00`）= 周期、`row[1]`（`+0x01`）= 相位；
  命中条件 = `row[0] == 0` 或 `timer % row[0] == row[1]`；`row[0] < 0` = **表末尾哨兵**（`0x54f9d0`）。
  ★ 若 `row[0x32] != 0` 则改用 `arg2 % row[0x32] == row[0x33]`（三人的 set10/11 都是 0 ⇒ 走 `arg1`）。
  ★ **焦点不改变 C1/C2**：`if (player+0x2070 != 0 && set < 10) set += 5`（`0x54f9a9`–`0x54f9bc`）⇒ set10/11 不受影响 ✓。
- `FUN_0054e960` `0x54e960`：新攻击体字段（直接照抄这张表的量纲）：

| 行内 | 类型 | 语义 | 一手 |
| --- | --- | --- | --- |
| `+0x00` / `+0x01` | i8 / i8 | 周期 / 相位 | `0x54f9e2` |
| `+0x02` | i16 | **伤害** | `0x54ea18` → 攻击体 `+0x94`（`0x520240` 之后） |
| `+0x04` / `+0x08` | f32 | 出膛偏移 x / y（class 4/5/6 时叠加） | `0x54edcd` |
| `+0x0c` / `+0x10` | f32 | 判定体 A / B | `0x54ea23` / `0x54ea2d` |
| `+0x14` | f32 | **角度（弧度）** | `0x54eb5d` |
| `+0x18` | f32 | **角度增量 / 帧** | `0x54df10` 首段：`shot+0x70 += row[0x18]` |
| `+0x1c` | f32 | **速度** | `0x54eb56` → `shot+0x6c` |
| `+0x20` | f32 | **速度增量 / 帧** | `0x54df10`：`shot+0x6c += row[0x20]` |
| `+0x28` 低 4 位 | i8 | **class**（只决定出膛位置来源） | `0x54eaad`–`0x54eb33` |
| `+0x29` | i8 | mode（`2` = 占子机槽） | `0x54eb3a` |
| `+0x2a` | i16 | **anm 脚本号** | `0x54ed4f` → `FUN_004cb960` 的 `param_4` |
| `+0x2c` | i16 | 音效 id | `0x54efb6` |
| `+0x34` | i32 | **寿命（帧）** | `0x54df10`：`0 < life <= age` ⇒ 销毁 |
| `+0x38` | i32 | 子机槽 | `0x54eb41` |
| `+0x48`/`+0x4c`/`+0x50`/`+0x54` | i32 | func 索引 → `0x5a0440`(init) / `0x5a0400`(**tick**) / `0x5a03fc` / `0x5a03d0` (hit) | `0x54ea65`–`0x54ea9a` |

- ★ **class 0 = 从自机坐标出膛**（`0x54eab7`–`0x54eac9`，**不蕴含速度 0**）；`class != 0` 时出膛点 = `player+0x650+class*0x128`（`0x54eae6`–`0x54eb33`）。
- ★ **anm 管理器**：`ecx = player+0x4c`（`0x54ed3c`）。`player+0x44` = `pl%.2d.anm`、`player+0x48` = `pl%.2df.anm`（焦点版）、`player+0x4c = +0x44`（`0x547bb3`，`FUN_005478f0`）⇒ **C1 的弹用 `plNN.anm`** ✓。
- ★ **tick 用 `FUN_0054df10`（`0x54df10`）** 每帧调一次 `[shot+0x48]`；攻击体 = anm VM + 伤害源（`FUN_0051f320`，`0x51f320`），`FUN_00520240` 回填 `+0xb8=1`、`+0x94=伤害`。

### 1.3 `plNN.sht` 头部（三人实测，`_scratch/sht19/*.sht`）

| 偏移 | pl00 | pl01 | pl02 | 语义（一手） |
| --- | --- | --- | --- | --- |
| `+0x2c` | 50 | 50 | 50 | C1 阈值 ×10 = **500** |
| `+0x30` | 100 | 100 | 100 | C2 阈值 ×10 = **1000** |
| `+0x34` | 170 | 170 | 170 | C3 阈值 ×10 = **1700** |
| `+0x38` | 250 | 250 | 250 | C4 阈值 ×10 = **2500** |
| `+0xf0 + set*4` | 相对偏移表 | 同 | 同 | **绝对 = `sht + 0x190 + 表项`**（`0x547b97`） |
| 行 stride | `0x78` | 同 | 同 | `0x54faae` |

### 1.4 C3 / C4：一手**本侧**做什么（三人相同）

| 步骤 | C3 | C4 |
| --- | --- | --- |
| 回罩 | `FUN_0040b420()`（`0x40b420`：`barrier.state==2` ⇒ 回 1，`+0x18 += S+0xa4`） | 同 |
| 次数计数器 | `S+0x4c += 1`（clamp `[0,999]`，`0x50e820`） | `S+0x50 += 1`（`0x50e7d0`） |
| 送对侧 | 槽 5 `0x50d8a0` → `FUN_0050d410("ExtraAttack", …)`（★ 本轮不管） | 槽 6 `0x50d800` → `"BossAttack"`（本轮不管） |
| 本侧 ECL | `FUN_0050d410("Ecl_EtBreakLv3", 自机坐标)` | `FUN_0050d410("Ecl_EtBreakLv4", 自机坐标)` |
| 资源 | `S+0x24 / S+0x28 / S+0x30` 重算（`all.c:186642`–`186660`） | 扣 `S+0x34 − S+0x28`（`0x40b3e0`） |
| 档位 | `S+0x38 = clamp(S+0x38 + 1, 0, 7)` | — |

★ **侧别（关键）**：`FUN_0050d410(name, params)` 建的是 `EnemyInf`（大小 `0x68e4`，`0x50d410`）；
它用的侧别 = **`this + 0x4c80`**，而 `this = [record[side] + 8]`（该侧 stage 对象）。
`0x4e83b9` 的 `mov ecx,[ecx+8]`（ecx = **自机 record**）⇒ **`Ecl_EtBreakLvN` 的本体在「我方场地」** ✓；
而 `0x50d8a0`/`0x50d800` 先把 `*(0x5d19bc+0x710) = 1 − EX+0xcc`、且 `this` 取 **对面 record+8** ⇒ **送过去的敌人在对面** ✓。
（★ 这推翻 `HANDOFF-109-C1-C4.md` §1 里「Lv3/Lv4 的环被送到对侧」的字面读法：**环留在本侧，只有 ExtraAttack/BossAttack 过去**。）

### 1.5 `Ecl_EtBreakLv2/3/4`（`_ext/ecl/default.txt`，三人**同一份**）

```
void Ecl_EtBreakLv2() { var A; %A = 8.0f;  ins_502(32); goto Lv2_216 @1;
Lv2_120: ins_615(%A); %A ; +1 ; 4; ins_51(); ins_45(%A);
Lv2_216: if (%A < 24.0f) goto Lv2_120 @0; ins_1(); }     // 半径 8/12/16/20，共 4 帧
void Ecl_EtBreakLv3() { %A = 16.0f; … 上限 40.0f … }     // 半径 16/24/32（+8）
void Ecl_EtBreakLv4() { %A = 16.0f; … 上限 56.0f … }     // 半径 16/24/32/40/48（+8）
```

- 行号：`_ext/ecl/default.txt:269`（Lv2）/ `:287`（Lv3）/ `:305`（Lv4）。
- `ins_502(32)` = flagSet(32)；`ins_615(r)` = **半径 r 的环原语（画）**（🟡 名字据 `engine/anm/th19/01-opcode-crosscheck-th16.md:35`）；
  `ins_45(r)` = ❓未定（与 `ins_615` 成对、同参数，疑为"该半径的消弹/命中处理"）；`ins_51()` = wait 1 帧。
- ★ **一手确实有可见的扩张环**（= `ins_615` 画的），与 owner 实跑口径一致 ✓。

---

## 2. 灵梦（TH19 `pl00`）

### 2.1 C1 = `pl00.sht` **set10** + `pl00.anm` **script10/script12**

**一手出处**：`EXReimuInf` vftable `0x5a8a18`（槽 3 = `0x53b110`、槽 11 = `0x53ac80`）；
弹表 set10 @ `pl00.sht+0x1928`（相对 `0x1798`）；anm `_ext/pl00.txt`（script10 = `:367`、script12 = `:402`）。

**逐条配方（set10，4 行；★ 直接照抄）**

```
行 0x1928: rate=100 phase=0 dmg=7  off=(0,0) hit=(20,20) ang=-2.0944rad(-120°) spd=14 cls=0 anm=10 life=999
行 0x19a0: rate=100 phase=0 dmg=7  off=(0,0) hit=(20,20) ang=-1.7453rad(-100°) spd=14 cls=0 anm=10 life=999
行 0x1a18: rate=100 phase=0 dmg=7  off=(0,0) hit=(20,20) ang=-1.3963rad( -80°) spd=14 cls=0 anm=10 life=999
行 0x1a90: rate=100 phase=0 dmg=7  off=(0,0) hit=(20,20) ang=-1.0472rad( -60°) spd=14 cls=0 anm=10 life=999
func 表：init=11 → `0x54b3e0`；tick=8 → `0x54b380`；hit=7 → `0x54b370`
```

**anm 脚本正文（一手逐条）**

```
script10 { -1: layer(11); type(1); alpha(255); color(255,255,255); zWriteDisable(1); sprite(sprite38);
            1: angleVel(0.4712389f, 0.0f, 0.4712389f);
offset100: scriptNewRoot(script12); +1: jmp(offset100, 1); stop(); interruptLabel(1); +8: delete(); }
script12 { -1: layer(6); type(1); blendMode(1); alpha(64); color(255,64,64); zWriteDisable(1);
              sprite(sprite38); alphaTime(20,0,0); 20: delete(); }
```

**贴图**：`sprite38` = 内部名 **`player/pl00/pl00s.png`**（entry1），源矩形 **`{x:1, y:81, w:30, h:30}`**。
★ **订正**：它不是 `pl00.png`（`pl00.png` 是 entry0 = 立绘；`pl00s.png` 才是弹表贴图）——
实测切图 = 一张粉白御札（见 `local/_scratch/pl00art/` 的复算）。

**时间 / 运动（★ owner 的"停一下再走"就在这里）**

- 发射：`0x53ac80` 每帧被调（★ 每帧的驱动者 = `FUN_0050df40` `0x50df40`：`call [[param_1+0x18]+0x2c]` = 槽 11），
  每帧调 `FUN_0054f990(EX+0x8c, 0, 10)`，窗口 = `EX+0x8c ∈ [0, 0x14]`（21 帧）；
  `rate=100` ⇒ **只在 `timer == 0` 命中 ⇒ 全程只放 1 轮 4 发** ✓。
- 运动 = **tick `0x54b380`**（一手常量：`0x5aa228 = 0.95f`、`0x5aa1f4 = 0.2f`、`0x5aa1c0 = 0.0f`，阈值 `0x78 = 120`）：

```
if (shot+0x9c != 1) return;
s = shot+0x6c;                                    // 速度
if (shot+0x1c >= 120) { shot+0x6c = s + 0.2f; return; }   // ★ 第 120 帧后重新加速
if (s <= 0.0f) return;
s -= 0.95f; shot+0x6c = s;                        // ★ 每帧 −0.95
if (0.2f < s) return;
shot+0x6c = 0.2f;                                 // ★ 钉在 0.2（≈停住）
```

⇒ **速度 14 → 0.2（约 15 帧）→ 保持 0.2（到第 120 帧）→ 每帧 +0.2 再出发**。
位移：减速段 ≈ 110 单位、2 帧/单位巡航段 ≈ +21 单位（到 120 帧），随后加速。
- 攻击体本身 `life = 999`，但被 `FUN_0054df10` 的"出屏"清理提前回收。

**与卡 109 现状的差异**（见 §5 汇总表）：无减速/停顿（恒速 14）、无"二次出发"、**多了撞墙反弹**（一手没有）。

### 2.2 C2 = `EXReigekiInf`（槽 4 = `0x53af60` 建对象）

**一手出处**：`0x53af60`（构造 + 挂链）· 槽 1 初始化 `0x53b920` · 槽 2 每帧 `0x53b3a0` · 槽 5 析构 `0x53b280`；
vftable `0x5a8ab8`；RTTI 类名 `EXReigekiInf`（`th19-rtti.json`）。

**逐条配方**

```
构造 0x53af60: FUN_004d6b50(0x7b8,4) → memset 0 → *obj = &EXReigekiInf__vftable → 调槽1(init) → 挂进 EX 的链表
init 0x53b920(param = 指针→自机坐标 {x,y,z}):
  +0x58 = -1;  +0x6c = 0x2d (=45);
  +0x70/+0x74/+0x78 = 自机坐标（从实参拷贝）;
  循环 3 次: FUN_004cb960(&out, +0x70, 0x14, +0x70, 0, -1, 0, 0)
              ⇒ 3 台 anm VM，脚本号 = param_4 = 0x14 = 20，位置 = +0x70，管理器 ecx = [player+0x44] = plNN.anm
              ⇒ 句柄存 +0x77c / +0x780 / +0x784
  +0x7a0 = 0; +0x7a4 = 0; +0x79c = -1; +0x7b4 = 1; +0x7b0 = +0x6c/2 (=22);
  +0x788 = 0; +0x78c = 0x42800000 (=64.0f);
  +0x770 = 随机角（[−π,π) 归一化，常量 π=`0x5aa2bc`、2π 步进=`0x5aa280`）;
  FUN_004bb8d0(0x31, 0);
  player+0x2074/0x2078/0x207c = 0x27 / 0x28 / 0x42200000（先清零再写）⇒ ★ Timer `set_value(40)` = 39 / 40 / 40.0f（**40 帧无敌**）
tick 0x53b3a0（while +0x5c < +0x6c + 0x3c = 45 + 60 = 105 帧）:
  R = ±(100.0f × 1000.0f / 192.0f) = ±520.833   ← 常量 `0x5aa390`/`0x5aa520`/`0x5aa478`/`0x5aa3dc`
      ★ 符号 = `+0x774 != 0 ? + : −`；而 memset 后 `+0x774 == 0` ⇒ **R = −520.833（常量！无 0→64 缓动）**
  三个角 = +0x770, +0x770+2π/3, +0x770+4π/3（常量 `0x5aa2b8` = 3.0f）
  每点位置 = 自机坐标 + R·(cosθ, sinθ)   （写进 VM+0x614/+0x61c）
  if (+0x5c == +0x6c/2 = 22): +0x788 = 64.0f; +0x78c = 0; +0x79c = -1; +0x7b0 = 22; +0x7b4 = 1
  if (+0x5c == +0x6c   = 45): 三台 VM FUN_004cb830(vm,1); FUN_004bb8d0(0x2c, …)
  +0x770 += (π/180) × 10.0f   ⇒ ★ +10°/帧（常量 `0x5aa3d4`=180, `0x5aa2f8`=10）
  for 每个点 p（共 3）:
      FUN_004ee770(p, 64.0f, 0, 99999, 0)     ⇒ ★ 以 **p 为圆心、半径 64** 消敌弹（圆心不是自机）
      FUN_00530c60(p, …)                      ⇒ 🟡 该点撒道具
      if (+0x5c % 5 == 0) FUN_005200f0(out, p, 0.0f, 1, 4)
  if (+0x5c >= 45):
      R2 = ((+0x5c − 45) × 0x40) / 0x3c + 128.0f     ⇒ 128 → 192（60 帧线性，常量 `0x5aa3a0`=128）
      FUN_004ee770(point0, R2, 0, 99999, 0)          ⇒ ★ 圆心 = **第 1 个环绕点**（不是自机、不是原点）
      FUN_00530c60(point0, …)
      FUN_005200f0(out, point0, 1.0f, 24, 1)
  if (+0x5c >= 105) return 0xffffffff ⇒ 调用方销毁
```

- ★ **`+0x788/+0x78c/+0x79c/+0x7a0/+0x7b0/+0x7b4` 在 `init`/`tick` 里只被写**：全语料找不到以该基址读它们的指令
  ⇒ **不能**拿它们当"环绕半径 0→64 / 22 帧"的一手依据 ✗（见 §6 存疑 1）。
- ★ `FUN_005200f0(out, pos, a, b, c)` 写入目标对象：`+0x18`（xmm3）、`+0x20 = a`、`+0x7c/+0x80/+0x84 = b−1 / b / (float)b`（Timer 三元组）、`+0x98 = c`；
  **它不写 `+0x94`**，而射击系统的伤害值写在 `+0x94` ⇒ 🟡「`24` 是伤害还是存活帧」「`1`/`4` 谁是伤害谁是 kind」**未钉死**。
- ★ `FUN_004ee770(center, radius, ?, 99999, 0)` = 圆内消敌弹（`0x4ee770`：遍历敌弹池、`bullets+0x1044 ∈ {1,2}`、`FUN_004ee950(bullet+0x650, center, r/2+radius)`）。

**贴图（灵梦 C2）**：三台 VM 用 `pl00.anm` **script20**（`_ext/pl00.txt:580`）：

```
script20 { alpha(0); layer(20);
  scriptNew(script23) ×2; scriptNew(script25) ×2; scriptNew(script27) ×2; stop();
  interruptLabel(1): scriptNew(script24) ×2; scriptNew(script26) ×2; scriptNew(script28) ×2; scriptNew(script22);
  +60: delete(); }
```

- script20 自己 `alpha(0)`、`layer(20)` ⇒ **本体不可见**；可见的全是子级，全部用 **`sprite42`** = `player/pl00/pl00b.png` `{1,1,126,126}`：
  · **存活期（age < 45）**：`script23`（粉 `color(255,208,255)` alpha128）×2 + `script25`（紫 `color(128,0,240)` alpha128）×2 + `script27`（红 `color(255,0,0)` alpha128）×2；
    每支 = `scaleTime(20,1,0→1)` + 随机 `rotate` + `fsetDiv(%F0,%RANDRAD,64)` 的 `angleVel` + `moveBezier(20, …)` 循环（每 20 帧换随机目标）⇒ 环绕点"喷粒子"。
  · **age == 45**：tick 调 `FUN_004cb830(vm, 1)` = **anm 中断 1** ⇒ `interruptLabel(1)`：再建 `script24`×2 / `script26`×2 / `script28`×2
    （同色，但 `pos` + `posTime(50,4,…)` 飞散 + `scaleTime(50,1,→0)` + `alphaTime(50,1,0)`）+ `script22`（白 `(255,255,255)` 爆闪：
    `scaleTime(20,1,→1)`、`scaleTime(20,4,→6)`、`alphaTime(20,4,0)`），然后 **`+60: delete()`**。
- ⇒ ★ **灵梦 C2 的可见物 = 3 个"粒子发射器"（贴图 `pl00b.png` 的 126×126 sprite42），不是 3 颗弹** ✓（与 owner 的"C1 再加 3 个像 Bomb 的东西"吻合）。
- ⇒ ★ **时间轴与伤害轴完全对齐**：age 0–45 喷粒子（= 每点半径 64 消弹）→ age 45 爆开（= 扩张环 128→192 起点）→ age 105 全部消失。

### 2.3 C3 / C4（本侧）

**一手出处**：分派见 §1.1；本侧 ECL 见 §1.5；侧别规则见 §1.4；ECL 入口 `_ext/ecl/pl00.txt:642`（`ExtraAttack`）/ `:471`（`BossAttack`）。
**本侧配方** = §1.4 表（灵梦无额外本侧产物）。**差异**：卡 109 现把 `ExtraAttack` 的**对侧**弹幕搬到了本侧（§5）。

---

## 3. 魔理沙（TH19 `pl01`）

### 3.1 C1 = `pl01.sht` **set10** + `pl01.anm` **script8/9**

**一手出处**：`EXMarisaInf` vftable `0x5a8ba8`（槽 3 = `0x417b10`、槽 11 = `0x53cbe0`）；
set10 @ `pl01.sht+0x1478`（相对 `0x12e8`）；anm script8 = `_ext/pl01.txt:290`、script9 = `:318`。

**逐条配方（set10 = **1 行**；另有 set11 = 2 行，C2 之后才用）**

```
set10 行 0x1478: rate=60 phase=0 dmg=10 off=(0,0) hit=(0,16) ang=-1.5708rad(-90°) spd=0.0 cls=0 anm=8 life=999
    func: init=12 → `0x54ccf0`；tick=9 → `0x54c830`；hit=8 → `0x54c7c0`
set11 行 0x14f4: rate=60 dmg=6 off=(-32,0) hit=(0,16) ang=-90° spd=0 cls=0 anm=8 life=999
set11 行 0x156c: rate=60 dmg=6 off=(+32,0) hit=(0,16) ang=-90° spd=0 cls=0 anm=8 life=999
```

- 发射：`0x53cbe0` 只在 `EX+0x8c == 0` 那一帧调 `FUN_0054f990(0,0,10)`；
  ★ 若 `EX+0xd4 == 1` 再补一发 `FUN_0054f990(0,0,0xb)` 并把 `EX+0xd4` 清 0。
  `EX+0xd4` 由 **C2 处理器 `0x417b90` 置 1**（`0x417c49`），**紧接着的这一次 C1 放**（= C2 释放帧尾部那一支，见 §1.1b）
  就把它消耗掉 ⇒ ★ **一手机制：C2 释放的那一份 C1 会同时多两发（set11，∓32 偏移）**，而不是"下一次再补"。
- ★ **速度 0 = 定点**：弹体停在自机位置，靠 `pl01.anm` script8 的动画表现。

**anm 脚本正文（一手逐条）**

```
script8 { scriptNew(script9); layer(15); sprite(sprite29); type(1); anchor(1,0); scale(1.0f,0.0f);
          scaleTime(4,1,1.0f,1.0f); alpha(220); blendMode(1); rotateAuto(1);
          fset(%F0,%RANDF); scrollX(%F0); +1: scrollX(-0.04761905f); stop();
          interruptLabel(2): colorTime(8,0,160,160,255); caseReturn();
          interruptLabel(3): colorTime(2,0,255,255,255); caseReturn();
          interruptLabel(1): scaleTime(8,1,1.0f,0.0f); +8: delete(); }
script9 { layer(13); type(1); alpha(224); blendMode(1); angleVel(0,0,0.5067085f); sprite(sprite30);
          offset80: scaleTime(4,9,1.0f,1.0f); +4: scaleTime(4,9,0.5f,0.5f); +4: jmp(offset80,0);
          interruptLabel(1): scaleTime(8,1,0.0f,0.0f); +8: delete(); }
```

**贴图**：`sprite29` = **`player/pl01/pl01s.png`** `{x:1, y:145, w:510, h:30}`（★ 一整条**激光/星屑光束**，宽 510 > 图宽 256 ⇒ 一手就是"超宽精灵 + `scrollX` 滚动"的写法）；
子脚本用 `sprite30` = 同图 `{49,49,30,30}`。

**时间 / 运动（tick `0x54c830`，1216 字节；一手常量 `0x5aa45c = 512.0f`、`0x5aa310 = 12.0f`）**

- 速度恒为 `sht+0x1c = 0` ⇒ 弹体**钉在自机位置**；朝向由 `sht+0x14`（−90°）与 `shot+0x70` 决定。
- ★ **光束伸长**：`shot+0xb0` 每帧 `+= 12.0f` 直到 `>= 512.0f`（`0x54c8xx`：`comiss`/`addss` 常量 `0x5aa310`、`0x5aa45c`）
  ⇒ 约 **43 帧**伸到满长 512（端点 = `shot+0xb0 / 2`，常量 `0x5aa280 = 2.0f`）。
  同值写进 anm VM（`vm+0x6c = shot+0xb0 / 512.0f`，`vm+0x4b9/+0x4ba = 1` 打开拉伸）与伤害源（`FUN_00520240(shot+0xdc)` 的 `+0x34/+0x38`）。
- ★ **收尾**：`shot+0x1c > 0x77`（年龄 > **119 帧**）⇒ `shot+0x9c = 2`（结束）+ `FUN_004bb7d0(0x14)` + 清子机占用 `player+0x12894[shot+0x38] = 0`。
- ★ 卡 109 现状把魔理沙 C1 当"1 发小弹、会飞走/反弹" ✗；一手是**贴脸伸长、原地不动、约 120 帧后收起**的光束。

### 3.2 C2 = 6 × `EXLaserInf`（槽 4 = `0x417b90`）

**一手出处**：`0x417b90`（736 字节，建 6 个对象）；`EXLaserInf` vftable `0x5a8b8c`（槽 1 = `0x53d280` init、槽 2 = `0x53d150` tick、槽 5 = `0x53d060` 析构）。

**逐条配方**

```
0x417b90:
  EX+0xd4 = 1;                                  ← ★ 下一次 C1 补发 set11 的开关
  base 角 = -π/2（= -90°）；步长 = 2π / 6 = 60°；半径 = 600.0f（两点都用 600）
  FUN_004bb8d0(0x33, ±100×1000/192)
  循环 6 次（角度 = -90°, -30°, +30°, +90°, +150°, +210°）:
      new EXLaserInf(0x77c, vftable 0x5a8b8c)
      参数结构 {自机 x, 自机 y, 自机 z, 对面自机 x, 448.0f, 角度偏移 A, 角度偏移 B, side, 1}
      调槽 1（init）；挂链表
init 0x53d280:
  +0x6c = rand()%100 + 60                     ⇒ ★ **寿命 = 60…159 帧随机**（`FUN_00401960` = 取随机）
  +0x108 = +0x6c; +0x10c = 8; +0xf4 = -1
  端点经 `DAT_0062eeb0`（屏幕缩放）与按 side 的横向偏移换算；颜色/宽度写 +0x5e8..+0x5eb/+0x194
tick 0x53d150:
  if (+0x5c < +0x6c) 更新端点并 return 0
  else: *(0x5d19bc+0x710) = 1 − +0x774; FUN_0050d410("ExtraAttack00", …)   ⇒ ★ 到期时把 ExtraAttack00 送对面
```

- ★ 一手**没有**"6 条激光之外的可见弹"；激光是**引擎原生激光对象**（`EXLaserInf`，**不挂 anm VM** ✗）。
  可见参数在 init `0x53d280` 尾部写死：`obj+0x5e8 = 0xff`、`obj+0x5e9 = 0x8080`、`obj+0x5eb = 0x80`、
  `obj+0x190 / obj+0x194 = 屏幕缩放(DAT_0062eeb0) × 0.8f`（常量 `0x5aa21c = 0.8f`）⇒ 卡侧要复刻就**自建一条长条 VM**（贴图自定）。
- ★ 卡 109 现状把魔理沙 C2 当"6 条激光打本侧"，一手是"6 条激光 + **到期发 ExtraAttack00 到对面**"（本轮不管后一半）。

### 3.3 C3 / C4（本侧）

**一手出处**：ECL 入口 `_ext/ecl/pl01.txt:427`（`ExtraAttack`）、`:468`（`ExtraAttack00`）、`:419`（`BossAttack`）。
**本侧配方** = §1.4 表（与灵梦逐条相同）。

---

## 4. 早苗（TH19 `pl02`）

### 4.1 C1 = `pl02.sht` **set10** + `pl02.anm` **script7**

**一手出处**：`EXSanaeInf` vftable `0x5a8cc0`（槽 3 = `0x418c90`（写 `EX+0xd8 = 10`）、槽 11 = `0x53df80`）；
set10 @ `pl02.sht+0xde8`（相对 `0xc58`）；anm script7 = `_ext/pl02.txt:235`。

**逐条配方（set10 = **26 行**）**

```
角度 = -π + k·(π/6)（k = 0…25，即 -180° 起每 30° 整圈）；dmg=22；speed=16；hit=(8,48)；
rate=16；cls=0；anm=7；life=999；off=(0,0)；
★ 前 13 行（-180°…0°）phase=0；后 13 行（+30°…+330°）phase=8
func: init=4 → `0x54c2d0`；tick=4 → `0x54c080`；hit=5 → `0x54bf50`

★ 26 行**逐行**（除角度/相位外其余字段全同；角度保留 6 位小数，单位 rad）：
  行 0..12 （phase 0）: -3.141593, -2.617994, -2.094395, -1.570796, -1.047198, -0.523599, 0.000000,
                        0.523599, 1.047198, 1.570796, 2.094395, 2.617994, 3.141593
  行 13..25（phase 8）: 3.665191, 4.188790, 4.712389, 5.235988, 5.759587, 6.283185, 6.806784,
                        7.330383, 7.853982, 8.377580, 8.901179, 9.424778, 9.948377
  ⇒ 角度 = `-π + k·(π/6)`，k = 0…25（§2.1/§3.1 的"−180° 起每 30°"）✓
```

- 发射：`0x53df80`；当 `EX+0xdc == 0`（C1）时窗口 `EX+0x8c < 0x1e`（**30 帧**），set = `EX+0xd8`（C1 时为 10）。
  `rate=16` + `phase 0/8` ⇒ ★ **在 t = 0 / 8 / 16 / 24 各放半圈 13 发**（2 圈 × 26 发，不是"一次 26 发"）。
- init `0x54c2d0`：`shot+0x40 &= ~0x3c`；`shot+0xa0 = 0`。

**时间 / 运动（tick `0x54c080`，582 字节；一手常量 `0x5aa30c = 16.0f`）**

- 巡航速度 = `sht+0x1c` = **16.0**（恒定；`+0x18` 角度增量 = 0、`+0x20` 速度增量 = 0）。
- ★ **命中敌机 ⇒ 停 4 帧 ⇒ 再以 14 出发**（`0x54c080` 的敌机扫描分支）：
  `FUN_0040eab0` 迭代敌机；判据 = 敌 `+0x127c`(x) 落在 `±16.0f` 内且 `+0x1278`(y) 已被越过 ⇒ 命中时
  `shot+0x40` 状态位置 4、`FUN_004cb830(shot VM, 2)`（= **anm 中断 2** ⇒ script7 的 `interruptLabel(2)` 那一支）、复位 `shot+0x30` 计时器、
  `shot+0x6c = 0`（**速度归零**）、敌机坐标抄进 `shot+0x100..`；
  4 帧后（`shot+0x30 == 4`）⇒ `shot+0x6c = 0x41600000 = 14.0f`（**再次出发，速度 14 而非 16**）、`shot+0x40` 状态位置 8。
- ⇒ 早苗的"两段运动"是**命中时**停 4 帧；与灵梦的"飞出固定距离后停住再加速"机制不同（§2.1）。

**anm 脚本正文（一手逐条）**

```
script7 { layer(13); type(1); blendMode(1); zWriteDisable(1); rotateAuto(1); anchor(2,0);
          scale(0.0f,1.0f); scaleTime(4,0,1.0f,1.0f);
          offset112: sprite(sprite30); +4: sprite(sprite31); +4: sprite(sprite32); +4: sprite(sprite33);
          +4: jmp(offset112,0);
          interruptLabel(2): scriptNew(script8); scaleTime(4,0,0.1f,1.0f); +4: scaleTime(6,0,1.0f,1.0f); +6: caseReturn();
          interruptLabel(1): layer(15); anchor(2,0); blendMode(1); type(1); sprite(sprite30);
          scaleTime(20,0,0.0f,1.0f); +20: delete(); }
```

script8（script7 的 `interruptLabel(2)` 子级，命中时那支）:
```
script8 { layer(13); type(1); sprite(sprite25); blendMode(1); scale(0.0f,0.0f);
          scaleTime(4,0,3.0f,3.0f); +8: scaleTime(4,0,0.0f,0.0f); +4: delete(); }
```
（`sprite25` = `pl02s.png {17,1,14,14}`；★ 这就是命中时那 12 帧的"打中"小爆闪。）

**贴图**：`sprite30/31/32/33` = **`player/pl02/pl02s.png`** `{1,17,62,14} / {65,17,62,14} / {129,17,62,14} / {193,17,62,14}`
（★ 62×14 的**绿橙"龙/蛇"条**；实测切图见 §7）。

### 4.2 C2 = 同一套发射链改用 **set11**（槽 4 = `0x418d20`）

**一手出处**：`0x418d20`（槽 4）· `0x418c90`（槽 3，写 `EX+0xd8 = 10`）· `0x53df80`（槽 11）。

**逐条配方**

```
槽 3（C1）0x418c90: ★ 开头 `cmp [ecx+0xdc],0 ; jne ret`（**C2 帧里被守卫吃掉**）；否则复位 EX+0x8c/0x90/0x94=0、
                    +0x88=-1、快照自机坐标到 +0x9c..+0xa4、EX+0xd8 = 10
槽 4（C2）0x418d20: 同上复位 + 快照 + ★ EX+0xd8 = 0xb (=11)、EX+0xdc = 1
                    player+0xc18 = (int)(自机 x × 128.0f)   ← 常量 `0x5aa3a0` = 128.0f
                    player+0xc1c = (int)(自机 y × 128.0f)      🟡 读者未定位
槽 11 0x53df80: EX+0xdc != 0 ⇒ 窗口 EX+0x8c < 0x3c（60 帧），set = EX+0xd8 = 11
set11（26 行）: 角度/伤害/rate 同 set10；★ speed = 3.0；★ cls = 5（= 从子机槽 5 出膛）；
                hit = (24,24)；anm = 9；life = 999；前 13 行 phase=0、后 13 行 phase=8；sfx 首行 = 40
```

⇒ 同样在 t = 0/8/16/24/32/40/48/56 各放半圈（60 帧窗口内 8 次 × 13 发）。
★ `cls = 5` ⇒ 出膛点来自 `player+0x650 + 5*0x128`（§1.2 的 class≠0 分支）。

**贴图（anm 9 = `pl02.anm` script9，`_ext/pl02.txt:285`）**：

```
script9 { layer(13); type(1); alpha(192); zWriteDisable(1); rotateAuto(1); scale(1.0f,1.0f); sprite(sprite34);
          offset88: scaleTime(4,9,1.2f,1.2f); +3: scaleTime(4,9,1.0f,1.0f); +3: jmp(offset88,0); stop();
          interruptLabel(1): layer(15); type(1); alpha(128); zWriteDisable(1); scale(1.0f,1.0f);
            sprite(sprite25); alpha(255); scale(0.12f,0.12f); scaleTime(20,4,1.0f,1.0f); alphaTime(20,1,64);
            sprite(sprite40); +5: sprite(sprite41); +5: sprite(sprite42); +5: sprite(sprite43); +5: delete(); }
```

- `sprite34` = `pl02s.png` `{1,33,30,30}`（白 4 角星）；`sprite25` = `pl02s.png` `{17,1,14,14}`；
- ★ `sprite40..43` 定义在 **另一个 entry**：`player/pl02/pl02c.png` `{1,1,126,126} {129,1,126,126} {257,1,126,126} {385,1,126,126}`
  ⇒ 🟡 **跨 entry 引用**（脚本属于 entry1，贴图属于 entry2）；一手按"贴图随 sprite 记录走"实现即可照抄这 4 个矩形。

### 4.3 C3 / C4（本侧）

**一手出处**：ECL 入口 `_ext/ecl/pl02.txt:392`（`ExtraAttack`）、`:295`（`BossAttack`）。
（`ExAttack` `:302` / `ExAttack2` `:311` / `ExAttackFunc` `:320` / `ExAttackFunc2` `:356` **不是** C3/C4，属动物灵 EX ❓）
**本侧配方** = §1.4 表（与前两人逐条相同）。

---

## 5. 与卡 109 现状的差异表（`native/cards/pofv.c` + `assets/ability/scripts/`）

| 档 | 现状（卡 109） | 一手应改成 | 证据 |
| --- | --- | --- | --- |
| C1 灵梦 | 恒速 14 直飞 + **撞墙反弹 4 次**（`POFV_C1_BOUNCE_*`） | ★ **减速 0.95/帧 → 钉 0.2 → 第 120 帧起 +0.2/帧**；★ **一手无反弹** | `0x54b380`；`FUN_0054df10` 全函数无撞墙判据 |
| C1 三人 | 只在"C1 阈值 500 的上升沿"放一次 | ★ 一手：**每次释放都放一份 C1**（C2/C3/C4 的释放也放，同帧）；门槛只有 `charge >= clamp(S+0x28)` | §1.1b / `0x4e841c` |
| C1 魔理沙 | 1 发飞出去的小弹 | ★ 一手：**原地钉住的光束**，`shot+0xb0` 每帧 +12 伸长到 512（≈43 帧），年龄 > 119 帧收 | `0x54c830` / `0x5aa310` / `0x5aa45c` |
| C1 早苗 | 26 发一轮放完、恒速 16 | ★ 一手：t=0/8/16/24 各 13 发；**命中敌机时停 4 帧、再以 14 继续**（+ anm 中断 2 的小爆闪） | `0x53df80` / `0x54c080` |
| C2 灵梦 | 可见物 = 自烤 `POFV109_C_RING` 红环 | ★ 一手 = `pl00.anm` **script20** 的三台粒子发射器（`pl00b.png {1,1,126,126}`），age 45 中断爆开 | `pl00.txt:580` |
| C1 灵梦 | 贴图 = 自烤 `POFV109_C1_BULLET` | ★ 一手 = `pl00s.png` `{1,81,30,30}` | `_ext/pl00.txt:229` |
| C1 灵梦 | 注释称贴图来自 `pl00.png` | ★ 订正 = **`pl00s.png`**（entry1，弹表贴图） | `_ext/pl00.txt:201` |
| C1 魔理沙 | 1 发小弹（贴图同上） | ★ 一手 = **原地光束** `pl01s.png {1,145,510,30}` + 子粒子 `{49,49,30,30}`；速度 0 | `0x54ccf0` / `pl01.txt:290` |
| C1 魔理沙 | 无 | ★ **C2 过后下一次 C1 追加 set11 两发（∓32，dmg 6）**（`EX+0xd4`） | `0x53cbe0` / `0x417be8` |
| C1 早苗 | **一轮 26 发**（`rate 16` 被"简化"） | ★ 一手 = 30 帧窗口内 **t=0/8/16/24 各半圈 13 发** | `0x53df80` / set10 phase |
| C1 早苗 | 贴图 = 自烤弹 | ★ 一手 = `pl02s.png` 4 帧 `{1,17,62,14}`…（62×14 龙条） | `pl02.txt:235` |
| C2 灵梦 | 环绕半径 **0→64（22 帧缓动）** | ★ 一手 = **恒定 520.833**（`±100×1000/192`），无缓动；64.0f 是别的字段 | `0x53b3a0` 首段 |
| C2 灵梦 | 消弹圆 / 扩张环中心 = 自机 | ★ 一手 = **第 1 个环绕点**（扩张环）与**每个环绕点**（64 半径） | `0x53b7xx` |
| C2 灵梦 | `POFV_C2_POINT_HIT_DMG 4`（把第 5 参当伤害） | 🟡 一手 = `FUN_005200f0(p, 0.0f, 1, 4)` ⇒ 伤害/kind 归属未钉死 | `0x5200f0` |
| C2 灵梦 | 可见物 = 借 `POFV109_C_RING` 自烤环 | ★ 一手 = `pl00.anm` **script20** 的 3 台粒子发射器（贴图 `pl00b.png` `{1,1,126,126}`） | `pl00.txt:580` |
| C2 灵梦 | 扩张环半径由卡侧每帧写 scale | ★ 一手 = 128 + (age−45)×64/60（60 帧到 192），**只在 age ≥ 45** | `0x53b3a0` 尾 |
| C2 魔理沙 | 6 条激光（自定数量/朝向） | ★ 一手 = **6 条**，角度 −90°+k·60°，寿命 **60+rand%100**，到期发 `ExtraAttack00` 送对面 | `0x417b90` / `0x53d280` |
| C2 早苗 | 无（未实现） | ★ 一手 = **弹表 set11**（26 行、speed 3、cls 5、60 帧窗口、anm 9） | `0x418d20` / `0x53df80` |
| C3/C4 | 把 `ExtraAttack` **对侧**弹幕搬到本侧自转 | ★ 一手本侧只有 **`Ecl_EtBreakLv3/Lv4` 的扩张环 + 回罩 + 计数**；对侧内容本轮不做 | §1.4 / §1.5 |
| C3/C4 | 触发阈值（1700 / 2500） | ✓ 与一手一致（三人同为 500/1000/1700/2500） | `plNN.sht+0x2c..` |

★ 关于「撞墙反弹」：`EX_BOUNCE` 是 **TH16** 的 ECL 概念（`engine/ecl/th16/ECL-info.md:249`、`engine/bullet/th16/01-core-engine.md:189`），**不是** TH19 `plNN.sht` 的字段；
TH19 的 `plNN.sht` 行里没有任何 walls/bounce 位，tick `0x54b380` 与共享更新 `FUN_0054df10` 也不含反弹 ✗。

---

## 6. 未定位 / 存疑清单（🟡 = 有证据但未钉死，❓ = 未定位）

★ 本轮追加核实后**已结清**：① 「C3/C4 是否同时放 C1」→ **成立**（§1.1b）；② 「C1 的节奏 = 每帧还是每次释放一次」→ **每次释放恰好一帧**（放炮块末尾把释放计时器置 −10，§1.1b）。

1. 🟡 **`EXReigekiInf+0x788/+0x78c/+0x79c/+0x7a0/+0x7b0/+0x7b4`**：`init`/`tick` 里只写不读（全语料无该基址读者）
   ⇒ 卡 109 现在的"环绕 0→64 / 22 帧"**没有一手支撑**；本文件按"恒定 520.833"给规格（fsincos 的乘数在 `0x53b5c1`/`0x53b5c6`）。
   试过：`grep "0x788)" all.c`（只命中同函数与两个异类对象）、逐条读 `0x53b3a0` 全函数。
2. 🟡 **`FUN_005200f0` 的形参语义**：`(out, pos, a→+0x20, b→Timer(+0x7c/0x80/0x84), c→+0x98)`，**不写 `+0x94`**
   （射击系统的伤害字段）；其它调用点全是 `(…, 0, 1, 10/20/4/…, …)` 的小整数形态 ⇒ "24 是伤害"待证。
3. 🟡 **`ins_45(r)`**：与画环原语 `ins_615(r)` 成对、同参数；`engine/anm/th19/01-opcode-crosscheck-th16.md` 只钉了 615。
   试过：`grep ins_45 engine/`（只命中 TH10 的一处同形），TH16 表无该号。
4. ❓ **`player+0xc18/+0xc1c`**（早苗 C2 槽 4 写入的 `(int)(坐标×128)`）的读者：未定位。
5. 🟡 **魔理沙 C1 tick `0x54c830`（1216 B）的端点公式**：读到 `sht+0x14`/`shot+0x70` + `FUN_00403bc0`/`FUN_00403d00`，
   未逐条还原（不影响"速度 0 / 原地光束"的结论）。
6. 🟡 **早苗 C1 tick `0x54c080`**（582 B）：读敌机坐标做判定/瞄准（`FUN_0040eab0` + `enemy+0x1278/+0x127c`），
   是"命中处理"还是"追踪修正"未定 ⇒ 本文件只保证"速度 16 恒定"这一条（来自 set10 的 `+0x1c`）。
7. ❓ **灵梦 C2 的三台 VM 是否在 age 45 之后仍被脚本 20 的 `+60: delete()` 收掉**：脚本第 60 帧自删，而 tick 到 105 帧；
   105 帧后仍用已删的 VM 句柄写位置（`FUN_004cb8b0` 返回 0 时安全跳过）—— 一手的"宽容写法"，卡侧不必照抄。
8. 🟡 **`plNNf.anm`（焦点版）**：`player+0x48`；C1 的 set10/11 因 `set >= 10` **不会**切到焦点表 ✅，
   但焦点时 `player+0x4c` 会换成 `+0x48`（`FUN_005478f0`）⇒ **焦点下同一 anm 脚本号会取自 `plNNf.anm`**，
   本文件未比对 `plNNf.anm` 的对应脚本（需要时先 `thanm -x` 取出 `pl00f.anm`）。
9. ❓ **C4 的 `BossAttack` ECL 本体**（`"Boss00"` 敌机 + `Boss00BossCard1`）本轮明令不做；本文件只给本侧（§1.4）。

---

## 7. 复现命令与工具（照抄用）

```bash
# 弹表（二进制，绝对偏移 = 0x190 + 表项；行 stride 0x78；+0x00 < 0 结束）
python3 /tmp/sht.py local/_scratch/sht19/pl00.sht 10        # ← 本文件用的临时解析器（见下）
# 反汇编（实参是绝对 VA，imagebase 0x400000）
python3 local/_scratch/thdis.py local/th19.v1.10c/th19.exe rva 0x54b380 20
# 读常量真值
python3 tooling/pe_read.py local/th19.v1.10c/th19.exe 0x5aa228 --dwords 1
# anm 文本 / 贴图导出（★ thanm -x 会误报 not found，导出后先 find）
thanm -x 19 local/th19.v1.10c/_ext/pl00.anm player/pl00/pl00s.png
```

- 本文用到的临时 .sht 解析器要点：表在 `+0xf0`（40 项 ×4），`绝对 = 0x190 + 表项`；
  行字段：`+0x00` rate(i8)、`+0x01` phase(i8)、`+0x02` dmg(i16)、`+0x04/+0x08` off(f32×2)、`+0x0c/+0x10` hit(f32×2)、
  `+0x14` ang、`+0x18` dang、`+0x1c` spd、`+0x20` dspd、`+0x28` class(低 4 位)、`+0x29` mode、`+0x2a` anm(i16)、
  `+0x34` life(i32)、`+0x38` opt(i32)、`+0x48/4c/50/54` func 索引。
- anm 脚本行号：`_ext/pl00.txt:367`(script10)、`:402`(script12)、`:580`(script20)、`:229`(sprite38)；
  `_ext/pl01.txt:290`(script8)、`:318`(script9)、`:220`(sprite29)、`:221`(sprite30)；`_ext/pl02.txt:235`(script7)、`:285`(script9)。
