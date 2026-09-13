# player 逆向 02:TH16 双炸弹体系 + ★季节释放(按 C)
> **版本**：TH16 v1.00a（`th16.exe`，imagebase `0x400000`）。本文裸地址默认属该版本；引用其他版本须写成 `th18:0x…`。
>

> 方法:Ghidra(ghidra-re MCP)一手反编译 th16.exe(用户自有,ExpHP th-re-data 符号已套)。
> 日期 2026-06-12。分级 ✅高 / 🟡中 / ❓未解。**仅 TH16 v1.00a**。
> 这是用户点名的 **TH16 招牌"季节子机按 C 触发技能"** 系统;它和角色主炸共用同一套 `Bomb` 引擎。

## 0. 一句话结论

TH16 每局**同时存在两个 `Bomb` 对象**(`operator_new(0x108)` 各一,见 §1):

| 对象 | 全局 | 怎么选 | 类型 `+0xd4` | 消耗 | 输入上升沿位 | 默认键 |
| --- | --- | --- | --- | --- | --- | --- |
| **主炸(spellcard bomb)** | `MAIN_BOMB_PTR @0x4a6da8` | 按 `CHARACTER` | **0** | `CURRENT_BOMBS`(库存)| `INPUT_RISING_EDGE & 0x2` | 主炸键(X)🟡 |
| **★季节释放(Season Release)** | `SUBSEASON_BOMB_PTR @0x4a6da4` | 按 `SUBSEASON` | **1** | `CURRENT_SEASON_POWER`(季节槽)| `INPUT_RISING_EDGE & 0x800` | **季节键(C)** 🟡 |

**季节释放**就是用户说的"按 C 触发的技能":消耗**季节槽**(捡春/夏/秋/冬道具充能),释放一发**随槽位档次
变大的清弹屏障 + 短无敌**;多数副季节释放后槽**清零**,而**夏 / 土用**两个副季节**只扣一档**(可连放)。
触发都在 `player_update_perframe`(`engine/player/th16/01` §1)的状态 1 / 状态 4(决死)里。

> 输入位值 = ✅(反编译);"X / C" 物理键名 = 🟡(TH16 默认布局 + DInput 重映射未逐位解;关键是
> **两个独立动作位 0x2 与 0x800**,季节释放是 0x800)。

## 1. 构造与分派:`Bomb__operator_new` `0x40d890` ✅

每次进关建两个 Bomb,各 `0x108` 字节:

```c
// —— 主炸:按 CHARACTER(SUBSHOT__ZERO_IN_TH16 在 TH16 恒 0)——
switch (CHARACTER) {
  1: vftable = BombCirnoAInf;   2: vftable = BombAyaAInf;
  3: vftable = BombMarisaAInf;  default(0): vftable = BombReimuAInf;
}
Bomb__initialize(main, 0);        // param=0 → +0xd4=0(库存型)
MAIN_BOMB_PTR = main;
// —— 季节释放:按 SUBSEASON ——
switch (SUBSEASON) {
  1: vftable = BombCirnoSubInf;   2: vftable = BombAyaSubInf;
  3: vftable = BombMarisaSubInf;  4: vftable = BombAllSubInf;
  default(0): vftable = BombReimuSubInf;
}
Bomb__initialize(sub, 1);         // param=1 → +0xd4=1(季节槽型)
SUBSEASON_BOMB_PTR = sub;
```

**`CHARACTER` 枚举(由本 switch 一手坐实):`{0:Reimu, 1:Cirno, 2:Aya, 3:Marisa}`** = TH16 选关菜单的
四自机顺序。⚠️ 这**与 `pl0X.sht` 文件编号不同**(文件:pl00=灵梦/pl01=魔理沙/pl02=琪露诺/pl03=文,见
`engine/sht/th16/03` §4);CHARACTER 全局是**菜单序**,别把两者当同一索引。

**`SUBSEASON` 枚举 = 玩家选的副季节**,`{0,1,2,3,4}`,对应 vftable 用"拥有该季节的角色"命名;ExpHP 的
**函数名直接给了季节语义**(`BombSub*__begin`):

| SUBSEASON | vftable(角色名)| ExpHP 季节函数 | 季节 | activate 消耗(§3)|
| --- | --- | --- | --- | --- |
| 0 | BombReimuSubInf | `BombSubSpring` `0x411460` | 春 | 槽清零 |
| 1 | BombCirnoSubInf | `BombSubSummer` `0x40f590` | 夏 | **只扣一档**(可连放)|
| 2 | BombAyaSubInf | `BombSubFall` `0x40ec70` | 秋 | 槽清零 |
| 3 | BombMarisaSubInf | `BombSubWinter` `0x410150` | 冬 | 槽清零 |
| 4 | BombAllSubInf | `BombSubDoyou` `0x40e0f0` | 土用(无季/Extra)| **只扣一档** |

> 角色↔季节配对(灵梦/春、琪露诺/夏、文/秋、魔理沙/冬、All/土用)= TH16 设定 + ExpHP 命名双证。
> 存在 `MainMenu__do_subseason_select` `0x450af0` = 副季节选择菜单(佐证 SUBSEASON 是玩家选的)。

**`Bomb__initialize` `0x40d600`**:注册 on_tick/on_draw UpdateFunc,初始化两个内部 Timer
(`+0x34..` 冷却、`+0xe0..0xf4` 冷却/渲染),并存类型 `+0xd4 = param`(0 主 / 1 季节)。

## 2. 季节槽(season gauge):充能与档位 ✅

全局:`CURRENT_SEASON_POWER @0x4a5808`、`MAX_SEASON_POWER @0x4a580c`、
`SEASON_POWER_LEVEL_REQUIREMENTS @0x4a583c`(每档阈值数组,以 `0x4a5854` 为上界遍历→**至多 6 档:0..5**)、
`SEASON_POWER_LEVEL_DELTAS @0x4a5810`(每档增量)。

> ✅ **档位阈值已完全解出(2026-06-12,一手 + 复核纠错)**:`player_shot_init` `0x440fb0` 进关时调
> `Player__init_season_level_deltas(i, seed[i])` i=0..7,**seed 来自静态常量** `DAT_00494810`(={0,100,130,160})
> + `DAT_00494790`(={200,250,300,0})→ **DELTAS 序列 = `{0, 100, 130, 160, 200, 250, 300, 0}`**(实测字节)。
> `init_season_level_deltas` 把 DELTAS **累计**进 `SEASON_POWER_LEVEL_REQUIREMENTS`(`0x4a583c`,跨到 `0x4a5854` = **6 档**):
>
> | 档位 | 阈值(累计季节点)| 该档增量 |
> | --- | --- | --- |
> | 1 | **100** | 100 |
> | 2 | **230** | 130 |
> | 3 | **390** | 160 |
> | 4 | **590** | 200 |
> | 5 | **840** | 250 |
> | 6 | **1140**(= `MAX_SEASON_POWER`)| 300 |
>
> → **每档需要的季节点递增(100,130,160,200,250,300),累计 100→230→390→590→840→1140**,共 6 档,满槽 1140。
>
> ⚠️ **复核纠错记录(纪律)**:`init_season_level_deltas` 的累计公式里第二表 `DAT_004a5814` **不是独立的零表**——
> `0x4a5814 = SEASON_POWER_LEVEL_DELTAS(0x4a5810) + 4 = &DELTAS[1]`,即它**别名进 DELTAS 数组本身**,
> 读的是**奇数下标 deltas**(100/160/250,运行时由同一批 init 调用写入)。一个子 agent 因"静态镜像为 0 +
> 单一 READ xref"误判其 ≡0,算出错误阈值 {0,130,130,330,330,630};经一手反编译 + 地址相邻性核出别名,
> 手工逐调用模拟得正确值 {100,230,390,590,840,1140}。**教训:别把"静态为 0 / 单 xref"当"运行时为 0"
> ——别名/同数组写入会绕过按符号的 xref。**

**充能 `item_collect_season` `0x43de50`**(捡一个季节道具):`CURRENT_SEASON_POWER += 1`(封顶 `MAX_SEASON_POWER`);
返回 `true` 当且仅当这一加**跨过了一个档位阈值**(给 UI/音效播"升档")。
**档位 vs 填充比**:「当前档」= 数 `CURRENT_SEASON_POWER` 越过几个阈值（0..6，各调用点内联计数）；
**`get_season_gauge_fill_ratio` `0x43df20` 返回的是「当档内的填充比 0..1」**（满档返回常量 `0x4944d8`），
不是档位本身 —— 2026-09-09 一手反编译订正，见
[`season/th16/01`](../../season/th16/01-season-data-and-tables.md) §2.3。

**季节点从哪来**（掉落机制）:两条通道 —— ①按累计伤害（`seasonItemDamage(D)` → 每 `D` 点伤害掉一个）；
②死亡时按计时器线性衰减（`dropSeason(T,N,M)` → `N` 个衰减到 `M` 个）。全链一手反编译见
[`season/th16/01`](../../season/th16/01-season-data-and-tables.md) §5。

## 3. 能不能放 / 放出去:`Bomb::can_bomb` / `activate_bomb` ✅

> 注:下方 `param[N]` 是**反编译器的 dword 下标**(= 字节偏移 `4N`)。如 `param[0xc]`=字节 `+0x30`=发动标志、
> `param[0x35]`=字节 `+0xd4`=类型。**Bomb 对象 0x108 完整字节偏移字段图见 `player/05-object-field-maps.md` §2**。

**`Bomb__can_bomb` `0x40dda0`**(主炸与季节释放共用,按 `+0xd4` 分流):
```c
if (this+0xd4 == 0)  { if (CURRENT_BOMBS < 1) return 0; }              // 主炸:要有库存
else {               // 季节释放:
   level = 数 CURRENT_SEASON_POWER 越过 SEASON_POWER_LEVEL_REQUIREMENTS 几档;
   if (level < 1) return 0;            // 槽至少满 1 档
   if (this+0x38 < 0) return 0;        // 冷却中(+0x38 = 冷却 Timer,见 §4)
}
// 通用门:两个 Bomb 都不在发(+0x30!=1)、不在对话(GUI+0x1c8==0)、场上有敌(ENEMY_MANAGER+0x18c!=0)
return 1;
```

**`Bomb__activate_bomb` `0x40db20`**(按炸触发,`engine/player/th16/01` §1 状态1/4 调):
```c
if (param[0xc] != 0) return -1;        // 已在发,忽略(+0xc = "正在执行"标志)
param[0xc] = 1;                        // 置发动
if (param[0x35] == 0) {                // —— 主炸 ——
   CURRENT_BOMBS = clamp(CURRENT_BOMBS − 1, 0, 8);  Gui__update_bombs(...);
   if (符卡奖励 active 且 +0x24>=0x3c) param[0x1a]=1;   // 符卡内炸 → 标失败
   SoundManager__play_sound(0x2c);
}
(**param[0])();                        // ★ vtable[0]:执行炸/释放(下接 §5 BombSub*::begin)
if (param[0x35] == 0) { ENEMY_MANAGER+0x44 = 0; return; }   // 主炸到此为止
// —— 季节释放(param[0x35]!=0)——
if (param[0x3a] > 0x3c) { ... 设冷却 Timer +0x39=60、记 +0x38=+0x37、存释放原点 ... }
level = 数 CURRENT_SEASON_POWER 越过阈值几档;  param[0x36] = level;  param[0x37] = 0;
if (SUBSEASON==1 || SUBSEASON==4)      // 夏 / 土用:只扣一档
     CURRENT_SEASON_POWER = max(CURRENT_SEASON_POWER − SEASON_POWER_LEVEL_DELTAS[level], 0);
else { CURRENT_SEASON_POWER = 0; /* 槽清零,释放力度=满 */ }      // 春/秋/冬:全清
PlayerInner__repopulate_options(...);  season_gauge_render_42c600();
```

要点 ✅:
- **季节释放强度 = 当前档位 `level`**(`param[0x36]`),`BombSub*::begin` 据此查表设半径/时长(§5)。
- **消耗规则**:春/秋/冬 = **整槽清零**(一次用满);**夏 / 土用** = **只扣一档**(`SEASON_POWER_LEVEL_DELTAS[level]`),
  槽里余下的可**连续再放**——这就是这两个副季节"可连放小释放"的机制根源。
- `param[0x35]` 与 `+0xd4` 同义(主=0 / 季节≠0)。

## 4. 释放冷却:`initiate_season_release_cooldown` `0x40e040` ✅

季节释放后进冷却,锁住 `can_bomb` 的 `+0x38 < 0` 分支:
```c
bomb+0xe0 = -1.0;  bomb+0xe8 = 0xb4(180);  bomb+0xec = 180.0f;   // 渲染/计时 Timer
bomb+0x38 = -45;   bomb+0x3c = -45.0f;                            // ★ 冷却:+0x38<0 → can_bomb 拒绝
```
→ 释放后约一段时间(`+0x38` 从 −45 计回 0)内不能再放。`Bomb__on_tick` `0x40dd00` 每帧推进
`+0x38`(`+0xe` 计时)并在发动时调 vtable[1] 执行释放逐帧逻辑,返回非 0 即结束(`+0xc=0`)。

## 5. 季节释放干了什么:`BombSub*::begin`(以 `BombSubDoyou` `0x40e0f0` 为样本)✅

`activate_bomb` 的 `vtable[0]()` 进到对应副季节的 `begin`。Doyou(土用)样本:
```c
存释放原点 = 自机坐标(+0x14..);  SoundManager__play_sound_centered(0x4a);  // 释放音效
建 2 个 anm 释放特效(sprite 3、4);
level = 当前季节档;                              // 强度档
特效对象 +0x4b0 = RELEASE_LEVEL_RADIUS_DOYOU[level];     // 内圈清弹半径(随档变大)
特效对象 +0x4b4 = RELEASE_LEVEL_RADIUS_2_DOYOU[level] (+8); // 外圈半径
特效对象 +0x4ac = RELEASE_LEVEL_DURATION_DOYOU[level];   // 释放持续时长(随档变长)
PLAYER+0x1663c = 10(0x41200000=10.0f);          // ★ 释放瞬间 10 帧无敌(见 engine/player/th16/01 §3)
ENEMY_MANAGER+0x40 += 1;                          // 释放计数器(疑计分/无释放奖励统计,🟡)
```
- **释放 = 一个半径式清弹屏障**:`+0x4b0/4b4` 内外半径、`+0x4ac` 时长,逐帧由 vtable[1](on_tick)推进、
  在半径内**当炸弹清弹**(`BulletManager__cancel_radius_as_bomb` / `LaserXxx__cancel_as_bomb_*` 族)。
- **强度随季节档**:三张 per-副季节表 `RELEASE_LEVEL_RADIUS_* / _RADIUS_2_* / _DURATION_*`(Doyou 版已见;
  其余副季节各有一组,命名同形)→ 档越高,清弹圈越大、屏障越久。
**释放的视觉** ✅(2026-09-10 补,`BombSubDoyou__begin` `0x40e0f0` 一手 + `pl00sub.anm` 字节):

```c
pos = player->pos;  bomb+0x2c = -1.5707964f;  play_sound_centered(0x4a);
vm_out = create(PLAYER_PTR->subseason_anm_file, script 3, pos);   // 外圈
vm_out->F0 = RELEASE_LEVEL_RADIUS_<季节>[level];      // +0x4b0 = float_script_vars[0]
vm_out->F1 = RELEASE_LEVEL_RADIUS_2_<季节>[level];    // +0x4b4 = float_script_vars[1]
vm_out->I3 = RELEASE_LEVEL_DURATION_<季节>[level];    // +0x4ac = int_script_vars[3]
vm_in  = create(PLAYER_PTR->subseason_anm_file, script 4, pos);    // 内圈
```

anm 源 = **`zPlayer.subseason_anm_file`**（= 副 sht 的贴图包 `plXXsub.anm`），两个脚本逐条同构：

| 脚本 | 层 | 混合 | 颜色 | 动作 |
| --- | --- | --- | --- | --- |
| `pl00sub script3`（外） | 13 | `blendMode(2)` 减算 | `color(128,128,128)` → `color2(0,128,0)` | `drawPoly(1.0f,32)` |
| `pl00sub script4`（内） | 12 | `blendMode(1)` 加算 | `color(255,128,128)` → `color2(255,0,0)` | `drawPoly(1.0f,32)` |

两者都是：`sprite(-1)` + `drawPoly(1.0f, 32)`（1 单位半径的 32 边形 = 实心圆），
第 1 帧 `scaleTime(10, 4, %F0, %F1)`（10 帧扩到半径）、第 11 帧起 `alphaTime(250, 0, 64)`，
然后 `wait($I3)`（持续帧数）→ `interruptLabel(1)` → `alphaTime(20, 0, 0)` → `delete()`。
→ **特效的生命周期就是释放的生命周期**（§5d 的 `on_tick` 拿 VM 还在不在决定结束）。

### 5b. 五个季节释放 `begin` 同构(Spring 实证,补)✅

`BombSubSpring__begin` `0x411460` 与 Doyou 版**逐行同构**,仅换 per-副季节常量:
- 共同:存释放原点、**音效 0x4a**、建 2 个 anm 特效(sprite 3、4)、给特效设 `+0x4b0/+0x4b4` 半径与 `+0x4ac` 时长、
  自机 **`+0x1663c=10`(10 帧无敌)**、`ENEMY_MANAGER+0x40 += 1`(释放计数)。
- 差异:半径/时长查 **`RELEASE_LEVEL_RADIUS_SPRING` / `RELEASE_LEVEL_DURATION_SPRING`**(Doyou 用 `*_DOYOU`),
  第二特效膨胀常量 Spring **+16** / Doyou **+8**。
- → **五个副季节(春/夏/秋/冬/土用)`begin` 是同一模板**:都是"按档查本副季节的半径/时长表 → 设清弹屏障特效 +
  10 帧无敌"。**释放的视觉/范围差异 = 各 `RELEASE_LEVEL_*_<季节>` 表的数值差**(各表数值未 dump,❓)。
- ✅ **释放逐帧机制(on_tick line-trace,2026-06-12 补,升 ✅)**:见 §5d——确证"清弹 + 清激光 + 造伤害"。

### 5d. 季节释放逐帧:清弹 + 清激光 + 造伤害(on_tick line-trace,一手)✅

`Bomb__on_tick` `0x40dd00` 在发动期每帧调 vtable[1] = `BombSub*::on_tick`。以 Doyou 实证
(`BombSubDoyou__on_tick` `0x40e330`):
```c
vm = AnmManager__get_vm_with_id(release_anm_id @param[0x17]);
if (vm == 0) { initiate_season_release_cooldown(this); return -1; }   // 特效播完 → 进冷却,结束释放
pos = anm_40e490_compute_final_pos(vm);                                // 当前释放中心
Player__create_damage_source_4449b0(PLAYER_PTR, pos, 1, 100);          // ★ 对范围内敌人造伤害(伤害源)
(**vtable[0x10])();                                                    // → method_10:清场
```
`BombSubDoyou__method_10` `0x40e3c0`(vtable[4],清场):
```c
vm = AnmManager__get_vm_with_id(param+0x5c);  if (!vm) return;
pos = anm_40e490_compute_final_pos(vm);
BulletManager__cancel_radius_as_bomb(pos, 4);                          // ★ 半径内清敌弹
for (laser in g_laser_mgr 链表)  if (laser+0x10 != 1)
    (**laser->vtable[0x24])(pos, vm+0x50, 4, 1);                       // ★ 逐条激光 cancel_as_bomb 清激光
```
→ **季节释放每帧 = 在"当前释放中心"半径内:清敌弹(`BulletManager__cancel_radius_as_bomb` `0x416d20`)+
  清所有激光(各 `Laser*__cancel_as_bomb_*` 族)+ 造伤害(`Player__create_damage_source`)**。半径/中心来自
  begin 设的释放特效 anm vm(其 `+0x4b0/4b4` 半径、`+0x4ac` 时长由 `RELEASE_LEVEL_*` 表按档定,anm 脚本驱动扩张);
  **特效播完 → `initiate_season_release_cooldown`** 进冷却(§4)。这把 §5b 的 🟡 升 **✅**。

### 5c. 角色主炸 `BombMain*`(Reimu 实证)与季节释放的结构差 ✅

`BombReimu__begin` `0x410d10`(`MAIN_BOMB_PTR` 的 vtable[0],主炸):
- 存释放原点、**音效 0x31**(≠季节释放的 0x4a)、符卡失败处理、`ENEMY_MANAGER+0x40 += 1`、建 anm 特效 0x17。
- **★ 额外 `malloc(0x6c0)` 挂 `+0x70`(memset 0)= 主炸的工作缓冲**(疑追踪/弹幕状态),季节释放**没有**这一步。
- **不查 `RELEASE_LEVEL_*` 表、不设半径/时长字段、不设 10 帧无敌** → 主炸不是"半径屏障"模板,而是
  **角色专属符卡攻击**(实际弹幕/清屏在 `BombReimu__on_tick`/`method_10`,本篇未反)。
- 主炸列表:`BombMain{Reimu,Cirno,Aya,Marisa}`(按 `CHARACTER`),各有自己的 begin/on_tick/method。
- 结论:**主炸(库存型,角色专属符卡)与季节释放(季节槽型,半径屏障+随档)是两套不同的 begin 模板**,只共用
  `Bomb` 外壳(can_bomb/activate/on_tick 调度、`+0xd4` 类型分流、决死可用)。

### 5e. 五个季节释放的 per-副季节差异(逐个反 on_tick,2026-06-12 补)✅

`method_10`(清弹+清激光)**五个副季节字节级一致**(Doyou/Summer/Fall/Winter/Spring 都是"半径 4 清弹 +
遍历激光清"),差异全在 **on_tick**:

| 副季节 | on_tick 地址 | 每帧伤害(`create_damage_source` 第 4 参)| 额外副作用(写 player 字段)|
| --- | --- | --- | --- |
| 夏 Summer | `0x40f7d0` | 100 | 无(同 Doyou)|
| 土用 Doyou | `0x40e330` | 100 | 无 |
| 春 Spring | `0x4116a0` | 100 | 每帧写 `PLAYER+0x1663c=50`(=**持续刷新 50 帧无敌**);且**只在前 16 帧**(`+0xe<0x10`)放清弹脉冲 |
| 秋 Fall | `0x40eeb0` | **30** | 每帧写 `PLAYER+0x16688=1.5f`;把 2 个 anm vm 同步到自机坐标 |
| 冬 Winter | `0x4103e0` | **9** | 每帧写 `PLAYER+0x2c7cc=1.5f` →**触发 `+0x1664c` bit0x20**(见 `05` §1b 标志表,跨 agent 互证)|

要点(✅ 地址级,🟡"为什么"是推断):
- **伤害/帧不同**(夏/土用/春 100、秋 30、冬 9):多半由各释放的**持续帧数 / 覆盖范围**反向平衡,非单纯强弱。
- **春**额外给**持续无敌**(每帧把无敌帧刷到 50),且清弹脉冲只在前 16 帧 → 春释放偏"短促强清 + 长保护"。
- **秋 `+0x16688`、冬 `+0x2c7cc`** 写 1.5f:这俩是 player 结构里的**系数/状态字段**,被别处读(冬的 `+0x2c7cc`
  经 `EnemyManager on_tick` 转成 `+0x1664c` bit0x20,见 `05`);具体玩法效果未逐链追(🟡)。
- ⚠️ 复核:这些 per-副季节副作用由子 agent 反编译报出、地址明确;伤害数值与字段写点可一手复核,**"效果含义"
  保持 🟡**(未追下游消费)。

### 5f. 角色主炸 = 自主追踪机制(BombReimu 详,补 §5c)✅

`BombReimu__on_tick` `0x410de0` / `method_10` `0x411280` 与季节释放**根本不同**:它管理 **8 个寻的灵球**:
- begin(首帧)按 π/4 等分**生成 8 球**,各建 anm(sprite 0xf)+ `create_damage_source(…, 9999, 0xf)`(超长寿命寻的伤害源);
- 每帧逐球 `find_nearest_enemy` + `crt_atan2` 瞄准、转速上限 π/8、`motion_update_mode` 积分运动(真·寻的 AI);
- `method_10`:**逐球**按帧奇偶轮流 `cancel_radius_as_bomb(球位, 5)` + 激光清(半径 64)= 把清弹脉冲摊到 8 球 × 多帧;
- 球命中/到期 → `cancel + create_damage_source(…,0xb,100)` + 屏震(ScreenEffect)+ 音效 0x1b;帧 119 全灭或帧 200 强制收尾 → return -1;
- 用 `+0x70` 的 `malloc` 工作缓冲存 8 球描述符(stride 0xd8)。
- → **主炸 = 实例化自主寻的弹对象(自带物理/AI/生命周期),季节释放 = 跟随一个 anm 特效在其中心清弹+造伤**。
  两者只共用 `Bomb` 外壳调度。(BombReimu 实证;其余角色主炸 begin 同样置 `+0x1664c` bit0x4 + 120 帧无敌,见 `05`。)

### 5g. 五个副季节的**释放圈配色/形状**（一手逐字节，2026-09-10 补）✅

`BombSub*__begin` 各建 **两个** `drawPoly` VM，脚本号**不是统一的 3/4** —— 冬在 `pl01sub` 的 **20/21**：

| 副季节 | anm 文件 | 脚本（外/内） | 外圈 `color` → `color2` | 内圈 `color` → `color2` | 内圈边数 | 自转 `angleVel` | 内圈 `alpha` | 额外 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 春 Spring | `pl00sub` | 3 / 4 | (128,128,128)→(0,128,0) 灰→绿 | (255,128,128)→(255,0,0) 粉→红 | 32 | 无 | 255 | — |
| 夏 Summer | `pl02sub` | 3 / 4 | (255,128,128)→(255,128,0) 粉→橙 | (128,192,255)→(0,192,255) 蓝→青 | 12 | ±0.17453292 | 128 | — |
| 秋 Fall | `pl03sub` | 3 / 4 | (128,128,255)→(0,128,255) 蓝→青 | (255,192,128)→(255,192,0) 橙→黄 | 12 | ±0.17453292 | 128 | 两段 scale |
| 冬 Winter | `pl01sub` | **20 / 21** | (255,128,128)→(255,128,0) 粉→橙 | (128,192,255)→(0,192,255) **蓝→青** | 12 | ±0.17453292 | 128 | 两段 scale |
| 土用 Doyou | `pl04sub` | 3 / 4 | (255,128,128)→(255,128,0) 粉→橙 | (128,192,255)→(0,192,255) 蓝→青 | 12 | ±0.17453292 | 128 | `posTime(60,1,0,48,0)` |

- 两个脚本都是 `sprite(-1)` + `drawPoly(1.0f, N)`（半径 1 的 N 边形 = 圆）：**外圈 `blendMode(2)` 减算、
  内圈 `blendMode(1)` 加算**；自转 `angleVel(±0.17453292, 0, ±0.17453292)` = 10°/帧（外 +、内 −）。
  ⚠️ 2D 图元只读 **rz**（`AnmManager__draw_vm` `th18:0x4817d8` 只取 `vm+0x44`）⇒ x 分量不参与观感。
- 扩张：春/夏 `scaleTime(10, 4, %F0, %F1)`；秋/冬 `scaleTime(10,4,%F0,%F0)` 后再
  `scaleTime($I3, 4, %F1, %F1)`；土用 `scale(%F0,%F0)` + `scaleTime(50, 0, %F1, %F1)`。
- **冬 = 蓝圈**（用户实跑口径「冬天是蓝色的，外面有描边」= 蓝→青的 12 边形内圈叠在粉→橙外圈上），
  这也是「五季都写成春的粉色」的对照物：卡的 `season_ring()` 表逐行照本表。
- 脚本号一手：`BombSub*__begin` 里两处 `AnmLoaded__create_40e5c0(..., script, ...)` 的实参
  （`0x410150` 冬 = `0x14`/`0x15`，其余 `0x411460`/`0x40f590`/`0x40ec70`/`0x40e0f0` = `3`/`4`）。

### 5h. ★ 秋 / 冬 的「加速态」= 1.5× 速度 + 每帧一个**自机残像**（一手，2026-09-10）✅

**① 两个字段**（`BombSub*__on_tick` 每帧写、消费端每帧复位 1.0）：

| 副季节 | on_tick | 写的字段 | 值 | 消费端（一手） |
| --- | --- | --- | --- | --- |
| 秋 Fall | `0x40eeb0` | `player+0x16688` | 1.5f | `Player__move` `0x441f43`：**自机移速 ×1.5** + `Player__on_tick` `0x442ce7`（残像）|
| 冬 Winter | `0x4103e0` | `player+0x2c7cc` | 1.5f | `EnemyData__step_logic` `0x41c5d6`：`enm_compute_damage_sources` `0x445a30` 算出的**对敌伤害 ×1.5**；另 `EnemyManager__on_tick_1a__body` `0x41b449` 按它 >1.01 置 `player+0x1664c` bit0x20（→ 残像）|

> ⚠️ 早前 `05` §1b 把 bit0x20 记成「速度缩放态」是**错的**：全程序读 bit0x20 的只有
> `Player__on_tick` `0x442c32`（残像/闪色），真正被乘的是 `+0x2c7cc` 自己 ——
> 乘在**伤害**上（`0x41c5d6` 紧跟在 `enm_compute_damage_sources` 的返回之后）。
> 秋/冬 的差异因此是「**秋 = 加速、冬 = 加伤**」，不是同一种。

**② 残像**（`Player__on_tick` `0x442c76` / `0x442d33`，两个分支各一份、逐字节同构）：

```c
// 无敌中（0x442bda 那一支）只写 color_1 = 0xff0000ff（洋红），**不建残像**；
// 否则：
color_1 = (flag 0x20 ? 0xffff0000 /*蓝*/ : 0xffffff00 /*黄*/);   // 0x442c61 / 0x442d1e
AnmLoaded__create_40e5c0(player+0xc /* 自机 anm */, &out, AFTERIMAGE_SCRIPT[CHARACTER], 自机坐标, 0, -1);
vm = AnmManager__get_vm_with_id(out);
AnmLoaded__set_sprite(player+0xc, vm, player+0x34 /* 自机当下那帧 sprite */);
vm->color_1 = 状态色;
```

- **脚本表 `0x494800` = `{10, 9, 13, 16, 0}` 按 `CHARACTER`**（`0x4a57a4` = `CHARACTER`，
  **不是**副季节 —— 一开始按副季节读会得出「秋 = 8 连残像」的错误结论）。
- 每帧建一个 ⇒ 玩家移动时留下一串淡出的自机影子（`alphaTime(30,0,0)`）。
  只有秋/冬会走到这两支（它们才写速度字段），且**无敌结束后**才建（无敌期是闪色）。
- TH18：`Player__on_tick__body` **把建 VM 那段删了**（`0x45c6e4` 直接跳 `0x45c702`），
  但原版 `pl0X.anm` 的残像脚本还在（`sprite(sprite0)` + `color(60,0,192)` + `alphaTime(20,0,0)`，
  脚本号按角色 **pl00 17 / pl01 19 / pl02 14 / pl03 14**）⇒ 要复刻得自己每帧起一个
  （card-expand 的 `season.c` `afterimage_tick` 就是这么做的）。

## 6. player 侧的触发点(回指 `engine/player/th16/01`)✅
`player_update_perframe`(`0x442560`):
- **状态 1(存活)**:`MAIN_BOMB_PTR & can_bomb & (INPUT_RISING_EDGE&0x2)` → 主炸;
  `SUBSEASON_BOMB_PTR & can_bomb & (INPUT_RISING_EDGE&0x800)` → **季节释放**。
- **状态 4(决死窗口)**:同样可按炸 → `activate_bomb` + `player_set_alive_after_bomb`(决死救命)。
  → **主炸和季节释放都能 deathbomb**(只要 `can_bomb` 过:主炸要有库存,季节释放要槽≥1 档且不在冷却)。

## 7. 待办 / ❓

1. ✅ **季节档阈值/增量已解（2026-09-09 一手复核）**:阈值 `{100,230,390,590,840,1140}`、DELTAS
   `{0,100,130,160,200,250,300,0}`、满槽 1140 —— 见
   [`season/th16/01`](../../season/th16/01-season-data-and-tables.md) §2.1（含 `init_season_level_deltas` 反编译）。
2. 部分完成:`BombSub{Spring,Doyou}::begin` + Doyou `on_tick`/`method_10` 已反(§5b/§5d,清弹+清激光+伤害
   ✅);🟡 剩 Summer/Fall/Winter 的 on_tick 是否有副季节专属差异(自动收集等)、与 `BombMain*` 主炸 on_tick。
3. ✅ **`RELEASE_LEVEL_*` 三表的逐档数值已取（2026-09-09）**：五副季节 × (时长 / 半径 / 半径2) 共 10 张表的
   全部字节见 [`season/th16/01`](../../season/th16/01-season-data-and-tables.md) §2（静态 `.rdata` 直读）。
   春 = 半径最大(60→300)、时长最短(10→20 帧)；冬 = 半径最小(30→60)、时长最长(30→190 帧)。🟡 剩 `RADIUS_2` 语义。
4. 🟡 物理键位:DInput 重映射(`Supervisor__read_keyboard_input` `0x401d50`)未逐位解;确认 0x2=主炸键、
   0x800=季节键的默认物理键(TH16 默认 X / C)。
5. 季节槽对**敌人伤害/符卡**的反作用:`CURRENT_SEASON_POWER` 被 `EnemyData__step_logic`、
   `enm_compute_damage_sources`、`Spellcard__on_tick` 读取(xref 已见)——季节槽可能影响火力/判定,值得追。

## 8. 可信度 / 复核

- ✅ 一手:`Bomb__operator_new`(`0x40d890`)、`Bomb__initialize`(`0x40d600`)、`Bomb__can_bomb`(`0x40dda0`)、
  `Bomb__activate_bomb`(`0x40db20`)、`Bomb__on_tick`(`0x40dd00`)、`initiate_season_release_cooldown`(`0x40e040`)、
  `item_collect_season`(`0x43de50`)、`get_season_gauge_fill_ratio`(`0x43df20`)、`BombSubDoyou__begin`(`0x40e0f0`)、
  `BombSubSpring__begin`(`0x411460`)、`BombReimu__begin`(`0x410d10`)、`BombSubDoyou__on_tick`(`0x40e330`)、
  `BombSubDoyou__method_10`(`0x40e3c0`)全部本会话反编译;两个 Bomb 指针的读写点经 xref 闭合;清弹/清激光落到
  `BulletManager__cancel_radius_as_bomb`(`0x416d20`)+ Laser*__cancel_as_bomb 族。
- 🟡 SUBSEASON↔季节名靠 ExpHP 函数名 + TH16 设定(非逐字节验 vftable→begin 链);释放表值未取。
- ❓ 季节档数值运行时填充,未取。
- 复核入口:Ghidra DB `th16`,地址见上。
- ❗ 纪律:本篇多处"比社区进一步"(季节释放消耗规则、夏/土用连放、释放强度随档)——已落到**具体地址 +
  读写点**;但**未动态实测**,与游戏内体感冲突时先疑帧/档换算。
