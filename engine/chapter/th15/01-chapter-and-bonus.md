# th15 章节（波次）系统与章节奖 — 一手

> **版本**：TH15 v1.00b（`th15.exe`，md5 `bddd41a586e09e4447f795a14009d68a`，imagebase `0x400000`）。
> 本文裸地址默认属该版本；引用其他版本须写成 `th18:0x…`。
> **方法**：Ghidra 12.1.3 反编译 + ExpHP `th-re-data` 命名/结构体 + 原始反汇编交叉；
> `front.anm` / `effect.anm` 用 `thdat -x` + `thanm -l` 解出（`tooling/thtk/unpack.py`）。
> **可信度**：核心链路（边界 → 计数 → 结算 → 重置）✅ 全部有地址；UI 文本 4 行 ✅；
> 「BONUS %.1f / NO BONUS」弹字的**创建者** 🟡（只找到绘制端）。

## 0. 一句话结论

TH15 的「章节」**不是**靠阶段或时间切出来的，而是 **ECL 脚本显式宣告**：
敌机脚本调 ECL opcode **524 `setChapter(n)`** → 引擎在**下一帧**结算**刚刚结束那一章**的章节奖、
然后把每章计数器清零并打一次 Pointdevice 快照。一个 boss 的非符 + 符卡是否算「一波」，
由**脚本怎么写**决定，引擎不猜。

```
ECL 524 setChapter(n) ──► set_chapter(n): GameThread.chapter = n, flags |= 0x8000
                                   │（下一帧 GameThread::on_tick 消费）
                                   ├─► 章节奖结算   FUN_0043ab70(mode)
                                   └─► 计数器清零 + 快照  Globals::do_pointdevice_snapshot_and_autosave(chapter)
```

## 1. 边界：ECL 524 → `set_chapter` → GameThread 标志位

| 环节 | 地址 | 证据（一手） |
| --- | --- | --- |
| ECL opcode **524 `setChapter`** 分支 | `0x42d494` | ExpHP `labels.json` ecl 表 `524__setChapter`；分支体在 `EnemyData__ecl_run_over_300` 内 |
| `set_chapter(n)` | `0x432ec0` | `mov ecx,[0x4e9a94]`（GameThread）→ `or [ecx+0x90],0x8000` → `mov [ecx+0xb0],n` → `ret 4` |
| 标志位消费者 | `0x43d081` | `GameThread__on_tick` `0x43cc50` +0x431：`test [edi+0x90],0x8000` / `je` |

`set_chapter` 只有**一个调用方**（`0x42d4a0`，即上面那条 ECL 分支），所以「章节边界 = ECL 524」
是**唯一入口**，不存在别的隐式切章路径（`-x 0x432ec0` 全库一条）。

消费端的形状（`0x43d081`–`0x43d0f4`）：

- 先过一道「现在能不能结算」的门：`[0x4e7794] & 0x300`（MODEFLAGS 的练习/Pointdevice 位）
  与 `player+0x16220 ∈ {0,1}`（自机状态）；
- `chapter = GameThread+0xb0`；`chapter == 0` 时**不发章节奖，但计数器照样清零并打快照**
  （`0x43d0b5 je 0x43d0d5` 跳过 `0x43ab70`，`0x43d0d5` 仍调 `0x43dd30`）——
  所以 0 的语义是「这一段不计分」，不是「道中」也不是「无章节」（见 §1b）；
- `mode = (chapter ≥ 0x2b) || (0x16 ≤ chapter ≤ 0x28) ? 1 : 0` —— 这个 0/1 只影响**面板的
  文字/配色分支**（§4c），不参与算分；
- 调 `FUN_0043ab70(mode)` 结算，再调 `Globals::do_pointdevice_snapshot_and_autosave(chapter)`（§3）；
- 最后 `and [edi+0x90],~0x8000` 清位。

> 边界是**标志位**而不是章号数值：原版脚本会重复用同一个章号（st05 `MainLatter` 连调两次
> `setChapter(7)`、st01mbs 两次 `setChapter(2)`），轮询 `CURRENT_CHAPTER` 的跳变会漏掉第二次。
> 另外 `+0xb0` 只有 `0x432ec0` 一个写点（全库检索），GameThread 对象每关重建且 `memset` 到 0
> （`0x43cb80`）——引擎不会在别处推进/重置章号 ✅。

### 1a. 每章计数器的住处

`zGlobals` 对象基址 `0x4e73f0`（`GameThread::on_tick` 里 `mov ecx,0x4e73f0` 直接传 this）。

| 偏移 | 绝对地址 | 名字（ExpHP） | 谁写 |
| --- | --- | --- | --- |
| `+0x08` | `0x4e73f8` | `CURRENT_CHAPTER` | `0x43dd30` |
| `+0x10` | `0x4e7400` | `TIME_IN_CHAPTER` | 每帧 `0x43d126`；换章清零 |
| `+0x1c` | `0x4e740c` | `CURRENT_SCORE` | 结算 `0x43ad19` |
| `+0x2c` | `0x4e741c` | `GRAZE_TOTAL` | `Player::do_graze` `0x4573ce` |
| `+0x30` | `0x4e7420` | `GRAZE_IN_CHAPTER` | `Player::do_graze` `0x4573d9` |
| `+0x44` | `0x4e7434` | `CURRENT_PIV` | 擦弹 +100 `0x4573f5`；结算追加 |
| `+0x4c` | `0x4e743c` | `MAXIMUM_PIV` | 结算里当上限 |
| `+0x94` | `0x4e7484` | `ENEMIES_SPAWNED_IN_CHAPTER` | `0x432f00` |
| `+0x98` | `0x4e7488` | `ENEMIES_DESTROYED_IN_CHAPTER` | `0x433130` |
| `+0x1c8` | `0x4e75b8` | `__MAYBE__POINTDEVICE_RESETS_IN_CHAPTER` | `0x43dd30` 清零 |

> `0x4e7418`（`CURRENT_CHAPTER` 前一格）是 `0x432ef0` 那条 getter 读的值，**不是**章节号——
> 该 getter 的语义未定 🟡，别照名字用。

### 1b. 章号是「相位 ID」：全 8 关 524 的分布 ✅（2026-09-10）

`thecl -d 15 -x` 解 `local/th15.v1.00b/dat/st*.ecl`（含 `stXXbs` / `stXXmbs` / `stXXmbs2`；
下表覆盖全部 524 调用点，位置 = 所在子程序）：

| 章号 | 出现在哪 | 语义 |
| --- | --- | --- |
| `0` | 关卡脚本 `main` 开头（8 关全有）；st05mbs `MBossDead`/`MBossEscape` | 不计分段：计数器清零但不发奖 |
| `1`…`9` | `MainFront` / `MainLatter`（st05 用到 1–8、st07 用到 1–4 与 7–9） | **道中敌人组**：一段 `MainSubNN` 一波 |
| `22`…`24` | `MainMBossDebug`；st07mbs `MBoss2`/`MBoss3` | 道中 boss 调试 / 第二个道中 boss |
| `31`…`33` | `MBossCard*`（st01mbs2 / st03mbs / st07mbs） | 道中 boss 的符卡 |
| `41` | `main`（进关底 boss 前） | 关底 boss 入场 |
| `43`…`52` | `stXXbs.ecl` 的 `Boss`/`Boss2`…（第 k 个非符 = `42+k`）、`BossEscape` | 关底 boss 非符相位 |
| `62`…`71` | `stXXbs.ecl` 的 `BossCardN`（第 k 张符卡 = `61+k`） | 关底 boss 符卡相位 |
| `81` | `stXX.ecl` 的 `MainBoss`（`bossWait()` 之后） | boss 对话 / 结算演出 |

**道中一章 = 一个 `MainSubNN` 敌人组** ✅。st03 的 `MainFront` 是最干净的样本：

```
@MainSub00(); wait(260); etClear(640.0f); enmKillAll();
setChapter(1); wait(1); wait(255); @MainSub01(); wait(500); etClear(640.0f);
setChapter(2); @MainSub02(); wait(220); etClear(640.0f); enmKillAll();
setChapter(3); return;
```

模式固定：**清场（`enmKillAll` / `etClear`）或等上一组走掉 → `setChapter(n)` → `MainSubNN()` 刷出**。
st05 的 `MainLatter` 里 `setChapter(4)`…`setChapter(8)` 逐段对应 `MainSub04`…`MainSub09`
（`7` 连用两次）。TH18 没有这套 524，等价信号与阈值标定见
[`../th18/03-midstage-waves.md`](../th18/03-midstage-waves.md)。

## 2. 结算 `FUN_0043ab70(mode)` ✅

函数体 `0x43ab70`–`0x43b0f6`，`edi = GUI_PTR`（`0x4e9a8c`）。用 `[edi+0x11c]` 起面板 VM，
再把 8 个数字写进 gui 对象供绘制端读（§4b）。

**开头的短路**：`ENEMIES_SPAWNED_IN_CHAPTER == 0` → 直接 return（**没刷过敌人就不结算**，
道中切章不会白拿分）。

### 2a. 公式

```
rate  = destroyed / spawned × 100.0            # 击破率，百分比（0–100）
bonus = (int)(graze_in_chapter × rate × 50.0) / 10 × 10   # 向下取整到 10
```

| 常数 | 地址 | 值 |
| --- | --- | --- |
| 击破率放大 | `0x4cffe8` | `100.0` |
| 章节奖系数 | `0x4cffc0` | `50.0` |
| 满分基准 | `0x4cfe98` | `1.0`（rate 是否 ≥ 100% 的判据） |

结算写出的东西：

| 目标 | 值 | 地址 |
| --- | --- | --- |
| `gui+0x120` | `GRAZE_IN_CHAPTER`（面板第 1 行） | `0x43ac4a` |
| `gui+0x124` / `+0x128` | `rate`（float，第 2 行用 `+0x124`） | `0x43ac8c` / `0x43ac98` |
| `gui+0x12c` | `bonus` | `0x43acc4` |
| `gui+0x134` | `rate ≥ 1.0 ? bonus/rate : 0`（满分标尺） | `0x43acdb` / `0x43ace3` |
| `gui+0x138` | `__MAYBE__POINTDEVICE_RESETS_IN_CHAPTER`（第 3 行） | `0x43ae51` |
| `CURRENT_SCORE` | `+= bonus / 10`，封顶 `0x3b9aca00`（1e9） | `0x43ad19` |
| `CURRENT_PIV` | `+= (bonus / 50000) × 10 × 100`，封顶 `MAXIMUM_PIV` | `0x43ad49`–`0x43ad58` |
| `gui+0x130` | 归零（第 4 行 `+%d` 的滚动显示值，由别处递增） | `0x43ac63` |

> `CURRENT_PIV` 那条的量纲：先整除 50000 再 ×1000，所以 bonus 1,500,000 只换 30,000 PIV，
> 且被 `MAXIMUM_PIV` 夹住。**bonus 与 PIV 不是同一个数量级**，抄数值时别直接搬。

### 2b. 碎片奖励（bonus ≥ 1,000,000）

`cmp [edi+0x12c],0xf4240` `0x43ae42` → 够线才走奖励分支：

| 条件 | 调用 | 含义 |
| --- | --- | --- |
| `MODEFLAGS & 0x300 == 0` | `FUN_0044fa60` | 残机碎片路径 |
| `MODEFLAGS & 0x300 != 0` | `FUN_0044fbc0` | 符卡（Bomb）碎片路径 |

两个函数都去操作 TH15 自己的碎片计数器簇（`[0x4e7450/54/58]`、阈值表 `0x4cb118/0x4cb130`、
结算 `0x43a850`/`0x43a960`）——即 th15but14 那个魔改壳挂的同一片宿主
（`local/_scratch/TH15BUT14-REPORT.md` §3）。**bonus < 1e6 时不发碎片**，只发分与 PIV。

### 2c. 面板 VM 的挂点

- 起 VM：`AnmManager__allocate_vm` `0x489460` + `AnmLoaded__copy_vm(anm=0x2c8, id=0xf1)`
  `0x40b880`，插进 **world list**（`0x487bd0`），句柄存 `gui+0x11c`；
- 起之前先销毁上一块面板（`0x488d30` on `gui+0x11c`）；
- 子脚本 `0xf2`（残机碎片图标）/ `0xf3`（符卡碎片图标）由 `FUN_00489250(vm, id, index)` 找出来，
  `vm+0x18 &= ~1`、`vm+0x34 = -1` —— 就是「这一章拿没拿到碎片」的两个图标开关；
- `gui+0x19c |= 0x800`（同时清 `0x1000`）。

## 3. 结算后的清零与快照 `Globals::do_pointdevice_snapshot_and_autosave` `0x43dd30` ✅

`this = 0x4e73f0`（zGlobals），参数 = 章节号：

1. `chapter != CURRENT_CHAPTER` → `TIME_IN_CHAPTER = 0`；然后 `CURRENT_CHAPTER = chapter`；
2. `GRAZE_IN_CHAPTER = 0`、`ENEMIES_SPAWNED_IN_CHAPTER = 0`、`ENEMIES_DESTROYED_IN_CHAPTER = 0`、
   `POINTDEVICE_RESETS_IN_CHAPTER = 0` —— **每章计数器的唯一清零点**；
3. 拷当前 BGM 文件名进 `globals+0x9c`；
4. `AnmManager__destroy_snapshot_vms` → Player/Enemy/Stage/Bullet/Item(`memcpy` `0xe5dee4` 字节)/
   Effect/Popup 各自 `store_snapshot` —— 这就是**绀珠传 Pointdevice 重开点**；
5. 顺序上**先结算再清零**（`GameThread::on_tick` 里 `call 0x43ab70` 在前、`call 0x43dd30` 在后），
   所以结算读到的永远是刚结束那一章的计数。

## 4. UI（用户点名的「关键在 UI」）

### 4a. 面板 = `front.anm` script **241** ✅

`thanm -l` 解出的 `local/th15.v1.00b/anm/front/front.anm.txt`：

```
script241 { layer(25); resolutionMode(2); type(0); pos(448.0, 402.0, 0.0);
            scriptNew(244..248); scriptNew(242); scriptNew(243);
            stop(); interruptLabel(1); alphaTime(20,0,0); +20: delete(); }
```

- 面板常驻（`stop()` 挂住），**被下一次结算销毁**（§2c），所以一块面板 ≈ 一章的显示时长；
- `script242` = `sprite150`、`scale(0.0f,1.0f)` → 90 帧后 `scaleTime(10,0,1,1)` 拉开、再闪烁
  （`pos(0,240)`），`script243` = `sprite151` 同构；
  **谁是谁由像素坐实（2026-09-09）**：`sprite150`(y=160) 主色 **绿 `(34,255,85)` = 符卡碎片条**，
  `sprite151`(y=192) 主色 **粉 `(255,119,221)` = 残机碎片条** ——
  ⚠️ 本节早前把这两条写反了（说 script242 是残机碎片条），已订正；
  实机症状：卡里「掉残机碎片却显示绿色符卡碎片条」。
- 244–248 是面板上的五个静态装饰/标签脚本。

### 4b. 五行文字 = `Gui::on_draw_2e` `0x436990`（+0xdf 起）✅

`AnmManager__get_vm_with_id(gui+0x11c)` 拿面板 VM，VM 可见才画；字号/对齐用 AsciiManager 的
全局字段（`+0x1923c` font、`+0x19240` group、`+0x19248` align_h、`+0x1924c` align_v），
文字色取 VM 的 alpha 字节（`vm+0x53b`）。

★ **字体 id 的一手值（2026-09-10 反编译补全）**：顶上那个大号章节奖数字走
`font_id = 4` + `group = 2` + 对齐 `0/0`；下面那五行走 **`font_id = 2`** + `group = 2` +
`alignment_mode_h = 2`（右对齐）+ `alignment_mode_v = 0`，画完还原 `font_id = 0 / group = 0`。
TH18 的 `draw_string` `th18:0x41a2b0` 里字体 2 的字形映射与 TH15 逐条相同
（见 [`../../anm/th18/03-ascii-text.md`](../../anm/th18/03-ascii-text.md) §3），
⇒ 在 TH18 复刻这块面板直接用 **字体 2** 即可（卡里 `LOLK_FONT` 就是这么定的）。

★ **五行全表**（位置 / 格式串 / 取值 / 颜色，一手反汇编 `0x436b1d`–`0x436be9`）：

| 行 | 位置 (x,y) | 格式串 | 值 | 颜色 |
| --- | --- | --- | --- | --- |
| 1 | (300, 226) | `"%d"` `0x4cb718` | `gui+0x120` = 本章擦弹数 | 白 |
| 2 | (308, 246) | `"%3.1f%%"` `0x4cc3e0` | `gui+0x124` = 击破率 | 白 |
| 3 | (300, 266) | `"%3d"` `0x4cc3e8` | `gui+0x138` = Pointdevice 重开次数 | 白 |
| 4 | (300, 286) | `"%6d"` `0x4cc3ec` | `gui+0x130` = 章节奖**滚动显示值** | 白 |
| 5 | (300, **296**) | `"  +%d"` `0x4cc3f0` | `(gui+0x130 / 50000) × 10` = **PIV 增量** | **`0xff8080ff`（蓝紫）** |

- 第 5 行**只比第 4 行低 10 px**（286 → 296），不是 20。
- 第 5 行的除数来自 `0x436bb3` 的魔数 `0x14f8b589` + `SAR EDX,12` = **除以 50000**，再 `×10`
  （`LEA EAX,[EAX+EAX*4]` + `ADD EAX,EAX`）⇒ 它显示的是**章节奖换到的 PIV（最大得点）增量**，
  与「加进总分的 `bonus/10`」（`0x43ad19`）**不是一回事**，后者面板上不显示。
- 颜色 `0x436bbf` 直接写 `[mgr+0x19224] = 0xff8080ff`，D3DCOLOR = 0xAARRGGBB
  ⇒ R=80 G=80 B=FF = **蓝紫**（不是粉；`0xff8080` 那种写法是 R/B 写反了）。

> 坐标是 AsciiManager 空间的原值（= 640×480 空间；TH15 的 anm 面板空间是它的 2 倍）。

### 4c. 面板分支（`mode`）

| `mode` | `gui+0x1b4` | 配色/位置 | 用途推测 🟡 |
| --- | --- | --- | --- |
| 0 | `0x122` | 文字色 `0xffffffff`，`+0x1a4=0`、`+0x1a8=0` | 道中/普通章 |
| 1 | `0xdc` | `+0x1a4=0x59`、`+0x1a8=90.0f`、`+0x1a0=0x58` | 章节号 ≥ 43 或 22–40（boss 段） |

### 4d. 「BONUS %.1f」/「NO BONUS」弹字 🟡

两个字符串 `0x4cd8dc` / `0x4cd8e8` 全库只有一个引用点：`PopupManager::on_draw` `0x45ee9e` /
`0x45eede`（`0x45eb10` 内）。弹字项里 `+0x24 < 0` → `NO BONUS`，否则 `sprintf("BONUS %.1f", +0x28)`。
**谁创建这种弹字项没找到**（章节奖结算函数里没有任何 PopupManager 调用），
所以它是不是章节奖的配套显示、还是 Pointdevice 模式的别的提示，**未定** 🟡。

## 5. 与 TH18 的差距

| 需要的东西 | TH15 | TH18 |
| --- | --- | --- |
| 章节边界的显式宣告 | ECL 524 → `0x432ec0` ✅ | ❓ 未见（需在 th18 ECL 分派表里找 524） |
| 每章擦弹计数 | `GLOBALS+0x30` ✅ | ❓（TH16 有同名字段，见 [player/](../../player/OVERVIEW.md)） |
| 击破/刷怪计数 | `+0x94`/`+0x98` ✅ | ❓ |
| 面板图元 | `front.anm` script 241 ✅ | 无（TH18 的 anm 里没有章节面板） |
| 面板文字 | AsciiManager `drawf` ✅ | 有等价原语（[card/th18](../../card/OVERVIEW.md) 的 HUD 文字层） |
| 碎片奖励宿主 | `0x44fa60` / `0x44fbc0` ✅ | 有（LIFE/BOMB 碎片，[card/th18/06](../../card/th18/06-resource-economy.md)） |

落卡设计见 `docs/superpowers/specs/`（TH18 章节奖重构）。
