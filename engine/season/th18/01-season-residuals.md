# th18 季节系统残骸审计 —— 五个部件，一个都没剩

> **版本**：TH18 v1.00a（`th18.exe`，md5 `9969cac756098c1da05a81de45437a70`，imagebase `0x400000`）。
> 本文裸地址默认属该版本；引用其他版本写成 `th16:0x…`。
> **方法**：ExpHP `th-re-data` 逐版本结构体字段对账 + 一手穷举（`ItemManager__on_tick__body` 的
> item type 分派、`zEnemyDrop` 的消费者、`zBomb` 字段、`zPlayerInner` 字段）+ 全 exe 字符串/立即数普查。
> **可信度**：item type 分派与 `zEnemyDrop` / `zBomb` / `zPlayerInner` 三条 ✅ 一手；
> 「某字段不存在」只能给到 🟡（零引用 = 看不见，不等于没有槽）。

## 0. 一句话

TH16 的季节系统在 TH18 **没有可复活的残骸**：季节点道具（type 16）、季节槽全局、
季节子机数组、等级表、第二个 Bomb 对象**五样全无**。
唯一「像」的地方是 `zEnemyDrop` 多了一条 `timed_ex_drop` 通道：它的公式与 TH16 `dropSeason` **同形同向**
（`n = ex + timed×(T−timer)/T`，出场越久给得越少），但触发是**出场计时**不是 TH16 通道 A 的**累计伤害**，
掉的是普通道具不是季节点（§1b / §1b′）。

**结论：想复刻季节，只能自建。** 可行的接缝都在卡侧（`on_enemy_drop` 掉道具、`c_press` 释放、
`ce_cancel_all_bullets` 清弹、`ce_option_alloc` 当季节子机）—— 见 §4。
★ 补充（2026-09-10，§1d）：**「季节点」这个道具本身可以借原生 P/钱的道具管线**（下落 / 回收线 /
磁吸 / 收集全免费），唯一卡住的是外观（贴图与脚本都绑在 `bullet.anm` 的类型表上）。

## 1. 季节点（item type 16）

TH16：`zItemType` 枚举里 **`ITEM_SEASON = 16`**（th-re-data 的 `type-enums.json`），
收集端 `th16:0x43de50 item_collect_season`，anm 选图 `th16:0x430c90 Item::init_anm_for_piv_or_season`。

TH18 的 item type 分派（`ItemManager__on_tick__body` `0x445a80`，一手反编译，逐 case）：

| type | 行为 |
| --- | --- |
| 1 | `ItemManager__collect_power_item` |
| 2 | `ItemManager__collect_money_item` |
| 3 | 火力满则加分，否则 `repopulate_options` |
| 4 | 残机碎片（`FUN_00457570`）|
| 5 | 残机 +1（`FUN_00476be0(0x11)` = se_extend）|
| 6 | 符卡碎片（3 个换 1 个 B）|
| 7 | 符卡 +1 |
| 8 | 满火力（F）|
| 9–0xe | 分数转换点（六 type 共指一个加分 handler）|
| 0x10–0x13 | `AbilityManager__allocate_new_card`（**抽一张卡**）|

**没有 type 16 的 case**，`type >= 0x14 → 返回 0`（`0x447137` 的守卫）。
→ 季节点在 TH18 **不存在** ✅。

### 1b. `zEnemyDrop` 多出来的那条通道（不是季节点）

| 版本 | `zEnemyDrop` |
| --- | --- |
| th16 | `main_type(+0)` / `extra_counts[0x11](+4)` / `area(+0x48)`；季节掉落另在 `zEnemyDropSeason` |
| th17 | `main_drop(+0)` / `ex_drop[0x22](+4)` / `width/height(+0x8c/0x90)` |
| th18 | `main_drop(+0)` / `ex_drop[0x14](+4)` / **`timed_ex_drop[0x14](+0x54)`** / `timed_ex_drop_time(+0xa4)` / `timed_ex_drop_timer(+0xa8)` / `width/height(+0xbc/0xc0)` |

TH18 的消费者 `Enemy__drop_items_and_notify_cards` `0x430510`（一手反编译）：

```c
iVar1 = drop->timed_ex_drop_time;                       // +0xa4
iVar6 = drop->ex_drop[i];                               // +0x04 + i*4
if (iVar1 != 0 && drop->timed_ex_drop_timer.cur < iVar1   // +0xac
    && (iVar4 = drop->timed_ex_drop[i]) > 0)              // +0x54 + i*4
    iVar6 += iVar4 - iVar4 * drop->timed_ex_drop_timer.cur / iVar1;
```

→ 即 **`n = ex_drop[i] + timed_ex_drop[i] × (T − timer) / T`**（`timer` 从 0 往上走、走到 `T` 之后这一支不再生效）
⇒ **出场后 T 帧内结算给满 `ex + timed`，拖到计时走完只剩 `ex`**。

> ★ 2026-09-10 订正：本文件早前把这条写成「随时间线性补足」「走到 `timed_ex_drop_time` 才补满」——**方向写反了**
> （`iVar4 − iVar4×timer/T` 是**递减**）。与 TH16 的 `dropSeason` 完全同向，见 §1b′。

### 1b′. ★ TH18 的这条通道是**活的**，而且就是 TH16 `dropSeason` 的同款（2026-09-10 一手）✅

三个部件全都找到了一手证据：

| 部件 | 地址 / 事实 |
| --- | --- |
| 设窗口 `T` | ECL **opcode 574**（社区 eclmap 里无名，thecl 反编译成 `ins_574`）→ handler `0x433a70` → `FUN_0042d040(drop, T)`：写 `+0xa4 = T` 并把计时器复位（`+0xa8 prev = -1`、`+0xac cur = 0`、`+0xb0 cur_f = 0`、`+0xb8 control |= 1`）|
| 设每 type 的加成 | ECL **opcode 573** → handler `0x433a4a` → `FUN_0042d020(drop, type, count)`：写 `drop[type*4 + 0x50]`（type 1 → `+0x54`）|
| 计时器每帧 +1 | `FUN_0042ff80`（敌人每帧入口，`this` = `zEnemyData` 基址）`0x430161`–`0x4301d8` **内联**，见 §1b″ |

### 1b″. 计时器怎么走（一手反汇编，`FUN_0042ff80` `0x430161`–`0x4301d8`）

`drop` 在 `zEnemyData+0x5010` ⇒ 计时器四个字段落在 `zEnemyData+0x50b8`（`prev`）、`+0x50bc`（`cur`）、
`+0x50c0`（`cur_f`）、`+0x50c4`（游戏速度索引，0 = 常速）：

```asm
00430161  MOV EAX,[ESI+0x50c4]        ; 速度索引
00430167  CMP EAX,1 / JC ... / MOV [ESI+0x50c4],0 / XOR EAX,EAX
00430178  MOV ECX,[EAX*4+0x4b35c0]    ; 速度表（与 time_in_ecl 同一张）
0043017f  MOV EAX,[ESI+0x50bc]        ; cur
00430185  MOV [ESI+0x50b8],EAX        ; prev = cur
0043018b  TEST ECX,ECX / JZ 0x4301bf
004301a9  ADDSS XMM1,[ESI+0x50c0] ... ; 有速度指针 → cur_f += speed
004301bf  MOVSS XMM0,[ESI+0x50c0] / INC EAX   ; 否则 cur += 1
```

⇒ **每帧 +1**（随游戏速度缩放），从 opcode 574 设窗口那一刻（敌人 ECL 开头）开始计。
（这条计时器不是 `Timer::increment` 的调用点，是内联展开的 —— 早前按「找 `Timer::increment` 调用」扫不到，
差点误判成「计时器从不前进」。）

原生普查（`thecl -d 18 -m th18.eclm`，22 个 `*.ecl` 全扫，64 处 `574`）：

| 敌人族 | `T` | `573` 计数 | 典型 |
| --- | --- | --- | --- |
| 道中最小（`Cir00/01`、`GirlA0x`、`GirlRedA01`）| 40 / 60 / 70 / 80 / 90 / 120 | type 2 ×1 | `unknown573(2,1); ins_574(120);` |
| 中型（`GirlB01`、`GirlC01`）| 70 / 120 / 360 | 1..4 / 3..10 | `unknown573(2,10); ins_574(360);` |
| 大型（`GirlD01-03`）| 200 / 300 / 400 | 4..10 | `unknown573(2,10); ins_574(300);` |
| 道中 boss（`*mbs`）| 600 / 900 | 15 / 20 | `unknown573(2,20); ins_574(900);` |
| boss（`*bs`）| — | — | **一处都没有**（boss 的掉落走 `dropExtra`/`dropItems`，不设窗口）|

`T` 分布（64 处）：40×4、60×2、70×5、80×3、90×7、**120×21**、150×2、200×4、300×8、360×3、400×2、600×2、900×1。
计数只有 type 1（P）与 type 2（钱/点）两种，1..20。

⇒ **TH18 原生就有「越大只 → 窗口越长 + 掉得越多」的逐敌人档位**，而且字段就在敌人身上
（`enemy+0x5010+0xa4` = `T`、`+0x54..+0xa0` = 各 type 的加成）——卡侧不需要解析 ECL 就能读到。
**形状与 TH16 `dropSeason(T,N,M)` 一模一样**（`ex_drop` 扮 `M`、`timed_ex_drop` 扮 `N−M`），
只是产物是普通道具、且 TH16 的通道 A（按累计伤害掉）在 TH18 没有对应物。

**它到底是干什么的（2026-09-10 补充）**：

- **唯一消费点** = `Enemy__drop_items_and_notify_cards` `0x430510`（`0x430546` 的 `MOV ESI,[ECX+0xa4]`；
  全 exe 扫 `MOV r32,[reg+0xa4]` 后按 drop 结构邻域过滤，只有这一处）。kill-all 那两条 `+0x62e0` 是**复位**不是消费。
- **语义** = 「**出场后 T 帧内击破就多掉**」：`unknown573(2,N); ins_574(T);` 的敌人，
  T 帧内打死多掉 N 个**金钱道具**（type 2；type 1 是 P 点）——TH18 的金钱是商店货币，
  所以这条通道 = **ZUN 给的「速杀奖励」**（与 TH16 季节掉落同一设计，只是产物不同）。
- **不会被结算清零**：`0x430510` 尾段只 `_memset(this+4, 0, 0x50)`（清 `ex_drop`），
  `timed_ex_drop`（`+0x54`）与 `timed_ex_drop_time`（`+0xa4`）**留着** ⇒ 同一个敌人若调多次 `dropItems`，
  每次都会按**同一个窗口**再给一遍衰减加成。清空要靠 `dropClear`（opcode 506 → `0x42d180`）或再调一次 574。

### 1c. TH16 的字段残骸一路留到 TH17，TH18 零引用

TH16 的 `zItem.force_autocollect__season_only`（+0xc68）在 TH17 里变成
`zItem.__might_be_unused__force_autocollect__from_th16`（+0xc70，ExpHP 自己标了「可能没用」）。
按 TH17→TH18 的 item 结构位移（+0x1c）推算，TH18 的同槽在 `item+0xc8c`：
**全 exe 零引用**（一手扫描 `MOV r32,[reg+0xc8c]` / `MOV [reg+0xc8c],r32` / `LEA`，0 命中）。
同族里 `item+0xc90`（TH17 的 `appear_sound`）**有 1 处读**（`0x447323`）→ 说明位移推算成立，
`+0xc8c` 那个槽要么没了、要么彻底没人碰。**零引用 = 看不见**，所以这条只能 🟡。

## 1d. 「借 P 点 / 金钱的道具函数」到底能不能复用（2026-09-10 一手普查）✅

用户问：季节点能不能干脆当 P 点/金钱那种原生道具生成，白拿引擎的下落、回收线、磁吸与收集？
结论：**引擎侧全部可复用，但外观要另想办法；本仓选了「自己造 VM + 照抄原版运动学」。** 逐条：

**① 收集分派没有 default case ⇒ 类型 0 是「无副作用」的** ✅（一手：`ItemManager__on_tick__body`
`0x445a80` 的收集段 `0x44625f` 起 `switch (item+0xc78)`）

```
case 1  → collect_power_item          case 4/5/6/7 → 碎片 / 命 / 符卡
case 2  → collect_money_item          case 8 → 满火力
case 3  → 火力满加分 / 否则 repopulate case 9..0xe → 分数转换点
case 0x10..0x13 → AbilityManager__allocate_new_card（抽卡）
（没有 default）→ 落到共享尾 LAB_0044658a：play_sound(0x25) + 清 state + 删 VM
```

⇒ `item+0xc78 = 0`（或 ≥0x14）的道具**照常下落 / 被吸 / 被收，收集时只放 `se_item00` 并删除，不产生任何副作用**。

**② 卡能在分派前改类型** ✅：SDK 的 `on_item_collect` 断点正好落在 `0x44625f`（`mov edx,[edi+0xc78]`
的前一条），handler 里写 `*(uint32_t*)(item+0xc78) = 0` 就能把一颗原生道具「洗白」。
（`item+0xc8c` 全 exe 零引用，可当标记位；见上 §1c 的一手扫描。）

**③ 卡住的只有外观** 🟡：道具的**贴图与脚本都在出生时按类型表写死**——
`0x430960` 尾 / `0x445a80` 的 state 5→2 分支都调 `AnmManager__sub_407420(bullet_anm, vm+4, ITEM_TYPE_TABLE[type])`，
即 VM 属于 **`bullet.anm`**，`ce_anm_set_sprite` 换的是「该文件内的 sprite 号」。
要在道具上显示 TH16 的季节图与出场动画，只有两条路：
(a) **补丁 `bullet.anm`**（追加 5 张 16×16 + 1 个脚本，同 `ability.anm` 的做法）——文件大、要保证原生字节不变；
(b) 新增一个「给 VM 重指脚本 / 贴图到别的 anm 文件」的引擎入口（`0x407420` 是普通函数，可加 SDK 封装）。
两条都还没做。

**④ 本仓现状（id 116）**：自己起 `ability.anm` script115 的 VM，但**运动学逐条照抄**原版
（重力 0.03、上限 2.0、出界 472/200、吸引初速 = 磁速/3 后每帧 +0.2 到 12.0、回收线 = `player+0x47998`、
半径/磁速直接读 `player+0x47988..0x47994`）⇒ 手感与原生 P/钱一致，且原生卡 ITEM_CATCH / ITEM_LINE
改参数时季节点跟着变；代价是它不进 TH18 的得分/道具统计（卡片自己的资源），收集音是自己放的 `se_item00`。
出场动画照 TH16 `bullet.anm` script129（含 `angleVel(%F0)` 的 0..5°/帧随机自转）。

**复核入口**：`python3 tooling/ghidra/scripts/decompile_funcs.py th18 0x445a80`（收集段在 300–470 行）、
`dump_func_asm.py th18 0x447430`（极坐标 → 速度的 `FSINCOS`，卡侧用 `ce_polar_to_vel`）。

## 2. 季节子机（`subseason_options`）

| 版本 | `zPlayerInner` 里的两套子机数组 |
| --- | --- |
| th16 | `main_options(+0x50)` 4×0xe4、**`subseason_options(+0x3e0)` 4×0xe4**、`num_main_options(+0x15fe4)`、`num_season_options(+0x1608c)` |
| th17 | `options_50(+0x50)`、`options_3e0(+0x3e0)`（同形状，名字退化成编号）|
| th18 | `options(+0x50)` **4×0xf0**（每槽 +0xc 字节）、**`equipment(+0x410)`**、`bullets(+0xf50)` |

TH18 的 0x410−0x50 = 0x3c0 = 4×0xf0 ⇒ 只留**一组** 4 槽子机；
TH16 的 `subseason_options` 那块地址现在是 **`equipment`（装备卡块）**。
→ 季节子机数组**不存在** ✅（它的槽位被装备卡系统接管）。
`num_season_options` 那种计数字段在 TH18 的 `zPlayerInner` 里**没有对应物**（th-re-data 的 TH18 结构体只到 +0x47304）。

## 3. 季节子机的升级 + 季节解放

### 3a. 升级（等级表）

TH16 一手部件：`th16:0x4a5810 SEASON_POWER_LEVEL_DELTAS`、`th16:0x4a583c SEASON_POWER_LEVEL_REQUIREMENTS`
（两张 int 表）、`th16:0x43deb0 Player::init_season_level_deltas`、`th16:0x43df20 get_season_gauge_fill_ratio`、
`th16:0x42c600 season_gauge_render_42c600`（槽的渲染）。

TH18：`zPlayerInner` 里没有等级数组/等级字段；全 exe **没有**与这两张表同形状的常量块
（季节槽渲染那条路径也没了）。→ 升级系统**不存在** ✅（TH18 的子机是「一张装备卡 = 一个子机」，
没有档位概念 —— [`card/th18`](../../card/OVERVIEW.md)）。

### 3b. 季节解放（第二个 Bomb）

| 版本 | Bomb 对象 |
| --- | --- |
| th16 | **两个**：`MAIN_BOMB_PTR`(`th16:0x4a6da8`) + `SUBSEASON_BOMB_PTR`(`th16:0x4a6da4`)；`zBomb` 0x108 字节，`is_season(+0xd4)` / `season_level(+0xd8)`；5 个季节 vtable |
| th17 | 一个 `BOMB_PTR`；`zBomb` 只 0x9c 字节 |
| th18 | 一个 `BOMB_PTR`(`0x4cf2b8`)；4 个角色 vtable（Reimu/Marisa/Sakuya/Sanae），**没有季节 vtable** |

TH18 的 `zBomb` 里能确认的字段只有 `+0x30 bomb_is_in_use`（`CE_BOMB_ACTIVE()` 读它）。
没有 `is_season` / `season_level` 的痕迹，也**没有第二个炸弹全局**。→ 季节解放**不存在** ✅。

### 3c. 输入位

TH16 的季节释放走 `INPUT_RISING_EDGE & 0x800`。TH18 里 **`0x800` 也被用了，但语义不同**：

```
0x45c084  TEST [0x4ca434], 0x400   ; C 键 → 发动选中的主动卡（vtable +0x8）
0x45c09c  TEST [0x4ca434], 0x800   ; → AbilityManager__set_selected_active_card(mgr, -1) + 音效 0x4e
```
（一手反汇编。）→ 同一个输入位被**切换主动卡**占用，不是季节释放的残骸 ✅。

## 4. 想复刻的话，接缝都在卡侧（本仓结论）

| TH16 部件 | TH18 自建方案 | 现成原语 |
| --- | --- | --- |
| 季节点掉落 | `on_enemy_drop` 里按敌人掉落追加 `ce_spawn_item(type, …)` | SDK `on_enemy_drop` / `ce_spawn_item` |
| 掉落的**计时衰减 + 逐敌人档位** | 计时器用 `zEnemyData.time_alive`（`+0x2d0`，见 [`ecl/th18/01`](../../ecl/th18/01-boss-interrupts-and-spellcard.md) §1）；窗口 `T` 与档位读敌人自己的 `timed_ex_drop_time` / `timed_ex_drop`（§1b′）| 无 `dropSeason`，但有**同形的原生通道**（opcode 573/574）|
| 季节槽 | 卡私有状态（≤496 B）自己数 | `ce_state` |
| 季节子机 | `ce_option_alloc`（12 个公共槽，装备卡同款）| SDK §U |
| 升级 | 自己按槽值换 anm 脚本 / shooter 组 | `ce_option_*` / `ce_tick_shooters_for_card` |
| 季节解放 | 主动卡 `c_press` + 清弹 + 无敌帧 | `ce_cancel_all_bullets` / `ce_cancel_radius` / `ce_player_pos` |

> 本仓占位实现见 `mods/th18.v1.00a/card-expand/native/cards/season.c`（id 116）与
> `docs/superpowers/specs/2026-09-10-season-and-animal-design.md`。
