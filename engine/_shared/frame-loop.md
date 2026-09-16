# TH16 主循环 / 每帧更新调度(引擎主干)
> **版本**：TH16 v1.00a（`th16.exe`，imagebase `0x400000`）。本文裸地址默认属该版本；引用其他版本须写成 `th18:0x…`。
> **2026-09-16 追加 §4**：TH18 v1.00a 的同一张表逐条核过（地址对照 / `UpdateFunc` 布局 / 返回码 / 注册者清单）——
>   两版**返回码与字段偏移完全同构**，本节地址一律带 `th18:` 前缀。
>

> **对象**:TH16 `th16.exe`,imagebase `0x400000`。日期 2026-06-10。
> **方法**:ExpHP th-re-data 命名 + 本仓库一手反编译 `run_all_on_tick`(`0x401460`)与 `EnemyManager::on_tick_1a__body`(`0x41b3d0`)双重坐实;xref 追到顶层 `Window::do_frame`。
> **可信度**:派发器机制 + 敌机/弹幕驱动链 = ✅一手;子系统命名/优先级 = ExpHP(✅✅,与本仓库吻合)。仅 TH16。
> 这补上了 `engine/bullet/th16/01-core-engine.md` §6 标 🟡 的"调度器顶层 runner 未反"。

---

## 1. 架构:优先级更新表 `UpdateFuncRegistry`

引擎没有手写的大主循环;每帧靠一个**优先级回调表** `UpdateFuncRegistry` 驱动所有子系统。

- 子系统启动时把自己的 `on_tick`/`on_draw` 回调用 `register__on_tick`(`0x401300`)/`register__on_draw`(`0x4013b0`)注册进表(节点 `UpdateFunc`,`operator new` `0x401730`);带**优先级**(决定遍历顺序)。
- 每帧 `run_all_on_tick`(`0x401460`)遍历表、按序调每个回调;`run_all_on_draw`(`0x4015a0`)同构跑渲染。

**`run_all_on_tick`(`0x401460`)机制(✅一手)**:遍历 `registry+0x18` 链表(游标存 `+0x50`,节点 `[1]`=next);每节点取 `UpdateFunc` 对象 `obj`,若 `obj+8`(on_tick 指针)非空且 `obj+4 & 2`(启用位)→ 调 `(*obj+8)()`。**回调返回码驱动生命周期**:`0`→注销(`unregister` `0x4017a0`)、`3`→停止本帧返回 1、`5`→返回 -1、`6`→重头遍历、`7`→继续、其它→下一项;另在某些码下调 `obj+0x10`(第二回调)。临界区保护(`DAT_004c0f58`)。
> 这套 0/3/5/6 返回码 = 我们在 ECL/弹幕 handler 见过的 -1/0/1 生命周期约定的同源。

---

## 2. 每帧链(顶层 → 子系统 → VM)

```
Window::do_frame__normal_version (0x45a8a0)              ← 顶层帧驱动(另有 frameskip 0x45ac50 / 变体 0x45ae70)
 └ UpdateFuncRegistry::run_all_on_tick (0x401460) ✅      ← 按优先级遍历
    ├ Stage::on_tick_11            (0x409e50, prio 0x11)  → 关卡/STD/背景
    ├ EnemyManager::on_tick_1a     (0x41b4f0 / body 0x41b3d0, prio 0x1a) ✅一手
    │    └ 遍历敌人链表 mgr+0x180;每敌机 Enemy::on_tick (0x41d1e0)
    │         └ Enemy::ecl_run (0x473bc0) → EclRunContext::ecl_run (0x472030)   【ECL VM,见 engine/ecl/th16/04】
    ├ LaserManager::on_tick_1b     (0x4316b0 / body 0x431510, prio 0x1b) ✅一手(2026-06-13 补)
    │    └ 遍历激光活动链 mgr+0x14(g_laser_mgr=0x4a6ee0);按节点 flags 路由 vtable[4]/[6]/[13]
    │         └ vtable[4] = LaserLine/Inf/Curve/Beam_frame_tick(各类一份)  【激光 vtable,见 engine/bullet/th16/03 §2/§7】
    ├ BulletManager::on_tick_1c    (0x412c50 / body 0x412860, prio 0x1c) ✅一手
    │    └ 遍历弹池活动链 mgr+0x70;每弹 Bullet::on_tick (0x411e70)
    │         └ Bullet::run_ex (0x413860)   【弹运动 VM,见 engine/bullet/th16/01 §3】
    ├ EffectManager::on_tick_1f    (0x418ab0, prio 0x1f)
    ├ Spellcard::on_tick (0x417930) · Bomb::on_tick (各机体) · Ending::on_tick_23 (0x4199f0) · AsciiManager::on_tick (0x408fb0) …
 └ UpdateFuncRegistry::run_all_on_draw (0x4015a0)          ← 渲染遍(各子系统 on_draw,按渲染层)
```

- **优先级即顺序**:名字里的十六进制(`_11`/`_1a`/`_1c`/`_1f`/`_23`)就是注册优先级 → tick 先后(Stage→Enemy→Bullet→Effect→…)。
- **两条 VM 的入口都在这条链上**:ECL VM 经 EnemyManager(0x1a)→Enemy::on_tick→ecl_run;弹运动 VM 经 BulletManager(0x1c)→Bullet::on_tick→run_ex。我们之前一手反的子系统,现在挂到了主干上。
- **`Supervisor`**(顶层游戏主管):`read_joypad_input`(`0x4018e0`)/`read_keyboard`(`0x401d50`)/`on_registration`(`0x43b520`,也是 PRNG 播种点,见 `th16-engine-math.md` §3.4)。输入采集在帧驱动里,先于 tick。

---

## 3. 开放
- `Window::do_frame`(`0x45a8a0`)内部(WinMain 消息泵 / 计时 / 帧率节流 / frameskip 三变体差异)未一手反。
- `Enemy::on_tick`(`0x41d1e0`)内部→`ecl_run` 的确切调用点未逐行反(已确认它遍历敌机 + 调 per-enemy 更新 + vtable[0x14];ECL 驱动在其内)。
- 各子系统注册的**完整优先级表**(谁在 0x11..0x23 之间)可枚举 `register__on_tick` 调用方补全。

## 4. TH18（v1.00a）逐条对照 —— 同一张表，2026-09-16 一手核过

> 本节地址一律带 `th18:` 前缀（本文件默认版本仍是 TH16）。方法：`local/_scratch/thdis.py`（RVA 反汇编）
> + `local/_scratch/th18xref.py`（`e8 rel32` 调用者扫描 + `local/th18.v1.00a/th18-funcs.json` 命名）。
> 可信度：**✅一手**（逐条反汇编）；与 TH16 的对照 = ✅✅（两边**同一套返回码、同一组字段偏移**）。

### 4.1 地址对照（机制逐条相同）

| 部件 | TH16 | TH18 |
| --- | --- | --- |
| 注册 `on_tick` | `0x401300` | `th18:0x401180` |
| 注册 `on_draw` | `0x4013b0` | `th18:0x401230` |
| 每帧遍历 tick | `0x401460` | `th18:0x4012e0` |
| 每帧遍历 draw | `0x4015a0` | `th18:0x401420` |
| `UpdateFunc::operator new` | `0x401730` | `th18:0x401530`（`push 0x28`）|
| 摘链 `unregister` | `0x4017a0` | `th18:0x4015a0`（按指针在 `registry+0x14`/`+0x3c` 两条链里找格再摘）|

遍历骨架（TH18，✅一手）：`th18:0x401303 ecx=[registry+0x18]`（链头）→ `th18:0x40131f edi=[ecx]`（**节点**）
→ `th18:0x401321 ecx=[ecx+4]`（next 格）→ `th18:0x401323 cmp [edi+8],0 / je` → `th18:0x401329 test byte [edi+4],2`
（暂停位）→ `th18:0x40134f ecx=[edi+0x24]`（**this**）→ `th18:0x401352 eax=[edi+8]` → `th18:0x401355 call eax`。
游标（本节点）存 `registry+0x50`；链格是**嵌在节点里**的 `+0x14`（`+0x14` 自指、`+0x18` next、`+0x1c` prev），
`register__on_tick` 按**节点 `+0x00` 升序**插入（`th18:0x4011d5 cmp [eax],esi` / `th18:0x4011d9 jge`）。

### 4.2 `UpdateFunc` 布局（`th18:0x401530` 逐条，0x28 字节）

| 偏移 | 含义 | 证据 |
| --- | --- | --- |
| `+0x00` | **优先级/排序键** int | `th18:0x4011cc mov [edi],esi`（esi = `register` 的第二个参数）|
| `+0x04` | flags（bit0 = 存在；**bit1 = 暂停**）| `th18:0x401546 and [ecx+4],~1` + `th18:0x40157d or [ecx+4],1`；遍历里 `th18:0x401329 test …,2` |
| `+0x08` | **`on_tick` 函数指针** | `th18:0x401581 mov [ecx+8],eax`（eax = `operator new` 的唯一参数）|
| `+0x0c` | 注册期**一次性**回调（有值就调，随即清 0）| `th18:0x401193` / `th18:0x40119d` / `th18:0x4011a1` |
| `+0x10` | **第二回调**（与 `+8` 同一 `this`）| `th18:0x4013ab mov eax,[edi+0x10]` → `th18:0x4013b5 call eax` |
| `+0x14` | 链格（见 §4.1）| `th18:0x401565 mov [ecx+0x14],ecx`（自指初始化）|
| `+0x24` | **`this`**（回调的 `ecx`）| `th18:0x40134f`；`operator new` 与 `register` **都不写它** ⇒ 由调用者在两者之间填 |

### 4.3 返回码 → 生命周期（TH16 §1 那张表在 TH18 上逐条重现）

跳转表 `th18:0x4013f8`（`th18:0x401373 cmp esi,8` / `th18:0x401378 jmp [esi*4+0x4013f8]`，9 项）：

| 返回码 | TH18 落点 | 行为 | 与 TH16 对照 |
| --- | --- | --- | --- |
| 0 | `th18:0x401397` → `th18:0x4015a0` | **注销**（摘链，不释放）| ✅「0 → 注销」|
| 1 | `th18:0x4013b7` | **纯空转**：链继续（不碰节点）| — |
| 2 | `th18:0x40137f` | 重查暂停位，没暂停就**再调一次** `on_tick` | — |
| 3 | `th18:0x4013c7` | `ret 1` = **停止本帧** | ✅「3 → 停止本帧返回 1」|
| 4 / 8 | `th18:0x4013c3` | `ret 0` | — |
| 5 | `th18:0x4013ce` | `ret -1` | ✅「5 → 返回 -1」|
| 6 | `th18:0x401303` | **重头遍历** | ✅「6 → 重头遍历」|
| 7 | `th18:0x4013ab` | 调 `+0x10` 第二回调后继续 | ✅「另在某些码下调 `obj+0x10`」|

### 4.4 谁在这张表上（`th18:0x401530` 的 89 处调用，按所在函数归类，✅一手）

- `Supervisor__initialize`（11 处，主力）、`Supervisor__on_registration`（2 处）。
- 子系统：`Stage`、`Player__initialize`、`EnemyManager`、`BulletManager`、`ItemManager`、`EffectManager`、
  `AsciiManager`、`Gui`、`ReplayManager`、`ScreenEffect`、`PopupManager`。
- **卡牌侧**：`AbilityManager__operator_new`（2 处）、`AbilityMenu`/`AbilityShop`、`CardCollection`。
- **运行期动态注册**（生命周期短）：`Bomb*`（`Bomb__operator_new` / `BombMarisa__begin` / `BombReimu__on_tick` /
  `BombReimu__method_that_does_some_cleanup` / `BombSanae__*`）、`TrophyNotice__award_trophy`、
  `Stage__on_draw_03`、`MainMenu__tick_menu_01_title`。
- 摘链的都各管理器自己的析构（`th18:0x4015a0` 的调用者）：`Player__operator_delete`、`Stage__destructor`、
  `BulletManager__operator_delete`、`EnemyManager__operator_delete`、`Bomb__operator_delete`、
  `AbilityMenu__operator_delete`、`ReplayManager__destructor`、`OptionsMenu`/`PopupManager`/`Stage` 等
  ⇒ 「**先摘链、后释放**」是引擎自己的规矩；`AbilityManager` **不在**摘链名单里（进程级单例，注册后不删）。
- 覆盖了 §3 那条开放项（「枚举 `register__on_tick` 调用方补全优先级表」）的**对象清单**这一半；
  逐个节点的**优先级数值**要等实跑日志（`th18_card_expand` 的链地图，见下）。

### 4.5 为什么关心它：头号崩溃就落在这一行的派发上

`mods/th18.v1.00a/card-expand` 的确定性崩溃（`0xC0000005` **写** `0x1`、`Eax = 1`、`Eip` 落在堆、
`Ebp/Esp` 每次同址）经 dump 栈回溯 = 正是 `th18:0x401355 call eax`：**某节点的 `+0x08` 是悬垂/被改写的堆指针**
（进程没有 DEP ⇒ 把堆数据当指令执行）。⇒ 引擎侧的结论：**这张表的节点一旦被提前释放而没摘链，
崩法就是「跳到堆里」**，不会在摘链点报错。
取证与止血（链地图 / `+0x08`、`+0x24` 写监视 / 不可执行即跳过这次 `call` 走返回码 1）
= `mods/th18.v1.00a/card-expand/native/sdk.c` 的 `BP_ce_chain_node_guard`（唯一出处），站点 `th18:0x401352`。

## 关联
- ECL VM:`engine/ecl/th16/04-ecl-vm-interpreter.md`(`ecl_run`)、`engine/ecl/th16/02-runtime-vm.md`(每敌机一解释器)。
- 弹运动 VM / 弹池 tick:`engine/bullet/th16/01-core-engine.md` §1/§3/§6(本文补其 §6 的 🟡 调度器 runner)。
- 子系统切口:`engine/sht/th16/06-th16-engine-incisions.md`(敌人/道具/图形…)。
- 命名来源:ExpHP `local/vendor/th-re-data`(`UpdateFuncRegistry`/`Window::do_frame`/各 `on_tick_NN`);纪律 `METHOD.md`。
