# th18 道中波次 —— TH15 的「一章」是什么，TH18 用什么当等价信号

> **版本**：TH18 v1.00a（`th18.exe`，md5 `9969cac756098c1da05a81de45437a70`，imagebase `0x400000`）。
> 本文裸地址默认属该版本；引用其他版本写成 `th15:0x…`。
> **方法**：TH15/TH18 全 exe 反汇编离线检索（`local/_scratch/th15_full.asm`、`th18_full.asm`，
> 与两份 exe 的 md5/字节逐条对过）+ pyghidra 反编译 + 零售 ECL（`thecl -d 15/18 -x`，8 关全量）。
> **可信度**：TH15 的 524 链路与分布 ✅（地址级）；「TH15 道中一章 = 一个 MainSubNN 敌人组」✅；
> TH18 关卡脚本识别与读法 ✅（引擎自己的用法）；阈值标定 🟡（静态读 ECL 推的，没跑过实机）。

## 0. 一句话结论

**TH15 的道中「一章」不是引擎按时间或清场猜的，而是关卡脚本自己用 ECL 524 `setChapter(n)`
在每一段敌人组（`MainSubNN`）开头宣告的**——`MainFront` 里 `setChapter(1) → MainSub01 …
setChapter(2) → MainSub02 …`，一章 = 一个敌人组。

**TH18 的道中一个 524 都没有**（8 关 `st*.ecl` 里 524 只出现在开场 0、进 boss 前 41 与 boss 段
43/81/2，全是 debug 或 boss 段）。但**脚本结构一模一样**（`main → MainFront / MainLatter →
MainSubNN`），所以等价信号 = **关卡脚本的 ECL 主上下文子程序号**：
**子程序换了 + 该子程序内第一次刷出 = 新的一波**。

## 1. TH15 道中波的判定机制 ✅

### 1a. 链路：ECL 524 → `set_chapter` → 下一帧结算

| 环节 | 地址 | 一手证据 |
| --- | --- | --- |
| ECL opcode 524 分支 | `th15:0x42d494` | `push 0; mov ecx,edi; call 0x430320`（取参数）→ `push esi; call 0x432ec0` |
| `set_chapter(n)` | `th15:0x432ec0` | `mov ecx,[0x4e9a94]`（GameThread）→ `or [ecx+0x90],0x8000` → `mov [ecx+0xb0],n` → `ret 4` |
| 标志位消费者 | `th15:0x43d081` | `test [edi+0x90],0x8000`；`th15:0x43d0ad` 读 `[edi+0xb0]` 当章号 |
| 结算 / 清零 | `th15:0x43ab70` / `th15:0x43dd30` | 见 [`01-chapter-and-bonus.md`](../th15/01-chapter-and-bonus.md) §2、§3 |

**章节号只有这一个写点**（全 exe 检索 `-x 0x432ec0` 只有 `th15:0x42d4a0` 一条 `call`；
`or [x+0x90],0x8000` 只有 `th15:0x432ecd` 一处，`test [x+0x90],0x8000` 只有 `th15:0x43d081` 一处）✅。

**引擎不会在别处推进 / 重置章节号** ✅：

- 扫「引用 GameThread 全局指针 `th15:0x4e9a94` 的 49 个函数」，其中对 `+0xb0` 的访问只有
  `set_chapter` 的写（`th15:0x432ed7`）与 `GameThread__on_tick` 的两次读（`th15:0x43d0ad`、
  `th15:0x43d0d5`）；其余同名偏移的写点（`th15:0x433c60`、`th15:0x43e9df`、`th15:0x4532b0`、
  `th15:0x45a84b` …）属于 `Gui` / `HelpManual` / `Player` / `FUN_00459e10` 等**别的对象**，
  它们不引用 GameThread 指针。
- GameThread 对象是**每关重建**的：`GameThread__operator_new` `th15:0x43cb80` 里
  `operator new(0xbc)` 之后 `push 0xbc; push 0; push esi; call 0x49b080`（`memset`）→
  **`+0xb0` 每次重建都是 0**；销毁在 `th15:0x43cb08`（把全局指针清 0）。
  → 「换关/重开 → 章号回 0」是对象生命周期的副作用，不是某个 tick 在推进。

**所以：章节边界 = ECL 524 调用（标志位），不是章号数值的变化** ✅。
零售脚本里章号会重复（st05 `MainLatter` 连调两次 `setChapter(7)`，st01mbs 两次 `setChapter(2)`），
靠「轮询 `CURRENT_CHAPTER` 跳变」会漏掉第二次——这是本仓以前那条口径（[`02-wave-signals.md` §4](02-wave-signals.md)）的漏洞。

### 1b. 章号是「相位 ID」：全 8 关的 524 分布 ✅

`thecl -d 15 -x` 解 `local/th15.v1.00b/dat/st*.ecl`（下表是**全部** 524 调用点，位置 = 所在子程序）：

| 章号 | 出现在哪 | 语义（从前后文读出来） |
| --- | --- | --- |
| `0` | 关卡脚本 `main` 开头（8 关全有）；st05mbs `MBossDead`/`MBossEscape` | **不结算**的边界：计数器清零但不发奖（§1c） |
| `1`…`9` | `MainFront` / `MainLatter`（st05 用到 1–8，st07 用到 1–4、7–9） | **道中敌人组**：一段 `MainSubNN` 一波 |
| `22`…`24` | `MainMBossDebug`；st07mbs `MBoss2`/`MBoss3` | 道中 boss 调试 / 第二个道中 boss |
| `31`…`33` | st01mbs2 / st03mbs / st07mbs 的 `MBossCard*` | 道中 boss 的符卡 |
| `41` | `main`（进关底 boss 前） | 关底 boss 入场 |
| `43`…`52` | `stXXbs.ecl` 的 `Boss`/`Boss2`…（第 k 个非符 = `42+k`）、`BossEscape` | 关底 boss 非符相位 |
| `62`…`71` | `stXXbs.ecl` 的 `BossCardN`（第 k 张符卡 = `61+k`） | 关底 boss 符卡相位 |
| `81` | `stXX.ecl` 的 `MainBoss`（`bossWait()` 之后） | boss 对话 / 结算演出 |

「道中一章 = 一个 `MainSubNN` 敌人组」的直接证据（st03/st05 最干净）✅：

```
void MainFront()            /* st03.ecl */
{
    @MainSub00(); wait(260); etClear(640.0f); enmKillAll();
    setChapter(1); wait(1); wait(255); @MainSub01(); wait(500); etClear(640.0f);
    setChapter(2); @MainSub02(); wait(220); etClear(640.0f); enmKillAll();
    setChapter(3); return;
}
```

模式是固定的：**先清场（`enmKillAll` / `etClear`）或等上一组走掉，再 `setChapter(n)`，
紧接着 `MainSubNN()` 刷出这一组**。st05 的 `MainLatter` 里 `setChapter(4)`…`setChapter(8)`
逐段对应 `MainSub04`…`MainSub09`（其中 `7` 连用两次）——所以**边界是调用，不是数值**。

### 1c. 复核「章号 0」：不结算，但**计数器照样清零** ✅

`GameThread__on_tick` 的消费端形状（`th15:0x43d081` 起）：

```
43d081: test [edi+0x90],0x8000      ; 有标志位？
43d0ad: mov  eax,[edi+0xb0]         ; 新章号 n
43d0b5: je   0x43d0d5               ; n == 0 → 跳过章节奖结算
43d0d0: call 0x43ab70               ; 否则结算（n>0）
43d0d5: mov  eax,[edi+0xb0]
43d0dd: js   0x43d0ea               ; n < 0 → 连快照都不做
43d0e0: call 0x43dd30               ; 快照 + 每章计数器清零
```

→ **`chapter == 0` 的意思是「这一段不计分」**（不发章节奖），**但计数器仍然被 `th15:0x43dd30`
清零并打 Pointdevice 快照**。所以 `0` 不是「道中」也不是「无章节」，而是**丢弃段**
（开场、st05 道中 boss 段）。[`01-chapter-and-bonus.md`](../th15/01-chapter-and-bonus.md) §1
原来写的「第 0 章是道中」应改读为「第 0 章不结算章节奖」🟡→✅（地址见上）。

### 1c. 同一子程序里的多波（2026-09-09 用户实跑补充）✅→🟡

用户实跑发现：**开场那三波阴阳玉原来被并成一章**。查 ECL 对上了 —— st01 `MainSub00` 里就是
**三段 burst**：`enmCreate` ×2 → `wait(10)` → ×2 → `wait(120)` → 下一段（见下表）。

| 信号 | 例子 | 结论 |
| --- | --- | --- |
| 子程序边界 | `MainFront` 里 `@MainSub00() … @MainSub01() …` | 一组一波（主信号）|
| **同子程序内的 burst 间隔** | `MainSub00` burst 间 `wait(120)`、burst 内 `wait(10)` | 静默 ≥ **100 帧** 后再刷出 = 新一波 |

卡里落成 `LOLK_MID_BURST_GAP 100`（`lolk_core.c` 的 `mid_boundary` 规则②）。
⚠️ 阈值只有静态读 ECL 的依据 🟡：`st06 MainSub07` 块内刷出间隔 500 帧、`st07 MainSub08` 300–400 帧
—— 那些也会被切成多波（是否合意待实机看日志）。

## 2. TH18 的 524 分布：道中确实切不出来 ✅（旧口径证伪）

`thecl -d 18 -x` 解 `local/th18.v1.00a/dat/st*.ecl`（含 `st0Xmbs` / `st0Xbs`，8 关全量）：

| 文件 | 524 出现在 | 章号 |
| --- | --- | --- |
| `st01`…`st07.ecl` 的 `main` | 开场 | `0` |
| 同上，`main` 的末尾 | 进关底 boss 前 | `41` |
| `st0X.ecl` 的 `MainBoss` | boss 入场 / `bossWait()` 后 | `43` / `81` |
| `st0X.ecl` 的 `MainBossDebug` / `MainBossDebug2` / `MainMBossDebug` / `MainBossTest` | **debug 子程序** | `41` / `22` / `43` / `81` |
| `st0Xbs.ecl` 的 `Boss` / `BossEscape` / `BossCard1`（仅 st07） | 关底 boss | `43` / `43` / `62` |
| `st0Xmbs.ecl` 的 `MBossDead` / `MBossEscape` | 道中 boss 退场 | `2` |

**`MainFront` / `MainLatter` / `MainSubNN` 里一个 524 都没有** ✅。
所以 [`02-wave-signals.md` §4](02-wave-signals.md) 写的「道中段 `0 / 22 / 41 / 81`、轮询
`th18:0x4ccce4` 跳变即可当道中波边界」是**错的**：`22` 是 `MainMBossDebug`（debug 子程序），
`81` 是 boss 对话，`41` 是 boss 入场——没有一个是道中敌人组。本节即为该条的反证。

> 顺带：TH18 的 `set_chapter` `th18:0x439e80` 只有两条写（`GLOBALS.inner+0x24` 与
> `GameThread+0x24`），**没有 TH15 的延迟标志位**，所以就算道中切章，也只能轮询数值，
> 遇到重复章号会漏——这条路本来就不可靠。

## 3. TH18 的等价信号：关卡脚本 ECL 子程序边界 + 刷出 ✅/🟡

### 3a. 关卡脚本本身就是一个敌人 ✅

引擎在每关开始时先 `EnemyManager__destroy_all`，再
`EnemyManager__allocate_new_enemy(&"main", params)` 建**关卡脚本**：

| 东西 | 地址 | 一手证据 |
| --- | --- | --- |
| 创建点 | `0x444228`、`0x444471` | `push 0x4b6634`（"main" 字串）→ `call 0x42d7d0`；两处前一行都是 `_memset(params,0,0x54)` |
| "main" 字串 | `0x4b6634` | 反编译里以 `&DAT_004b6634` 出现 |
| 分配 / 构造 | `0x42d7d0` / `0x42e100` | `operator_new(0x683c)` → `Enemy__constructor(name, params)` |
| 参数里 parent | `0x6834` | 由 params+0x50 拷入；引擎那条路 memset 过 → **关卡脚本 `parent_enemy_id == 0`** |
| 子程序名 → 子程序号 | `0x48d920` | 二分查找 `fm+0x8c` 表（每项 8 字节 {name, bytecode}），表长 `fm+8` |

关卡脚本的子程序名是 `main` / `MainFront` / `MainLatter` / `MainSubNN`…，
普通敌人是 `Girl*` / `Cir*` / `Phantom*`… → 用「`parent_enemy_id == 0` + 子程序名前缀
`main`/`Main`」认它。

### 3b. 读法（引擎自己的用法）✅

| 想读的东西 | 地址 | 一手证据 |
| --- | --- | --- |
| 当前 ECL 上下文 | `zEnemy+0xc` | `0x436a1c`：`[eax+0x1218]`（file_manager）前先 `eax=[eax+0xc]`；`0x42e43f` 调 `0x48d920` 后立刻读 `[esi+0xc]` |
| 当前子程序号 | `ctx+0x4` | `0x436a14`：`mov ecx,[eax+0x4]`，随后当 `subs[ecx]` 索引 |
| 当前字节码偏移 | `ctx+0x8` | 同一条链：`[eax+0x8]` 加上 `subs[ecx].bytecode` |
| 帧末一定是**主**上下文 | `0x48d4d6` | `Enemy__ecl_run` 收尾：`lea eax,[ebx+0x10]; mov [ebx+0xc],eax` —— 异步子上下文只在 tick 内临时挂上 |
| ECL 文件管理器 | `zEnemy+0x1218` | `0x42e431`：`[EnemyManager+0x188]` 拷进来 |
| 子程序表 / 表长 | `fm+0x8c` / `fm+8` | `0x48d920` 的二分查找；`0x436a2b` 的索引 |
| 本帧刷出数 | `zEnemyManager+0x90` | `Enemy__constructor` `0x42e40b`：每建一个敌人 `+1`（全 exe 只有它与 manager 构造写）|

### 3c. 判定规则（落在 `cards/lolk_core.c` 的 `mid_boundary()`）

```
关卡脚本子程序号变了 → 重置窗口
本帧有刷出(spawn_delta > 0)：
    认出关卡脚本(stage_sub >= 0)：该子程序内**第一次**刷出且距进入 ≤ LOLK_MID_SUB_WINDOW(256) 帧 → 新波
    没认出(stage_sub < 0)        ：静默 ≥ LOLK_MID_QUIET_GAP(240) 帧后的刷出 → 新波
boss 在场时不走这条（boss 相位 / 符卡位信号接管）
```

「**该子程序内第一次**刷出」是关键：一个 `MainSubNN` 会先自己刷一批，再启动异步子程序
（`@MainSubNNb() async`）在后面继续刷；那些后续刷出属于**同一波**，不切。

### 3d. 为什么不用「静默间隔 / 清场」当主信号 🟡→❌

零售 ECL 静态实测（`thecl -d 18`，脚本直读，🟡 因为没跑实机）：

| 量 | 实测范围 | 出处 |
| --- | --- | --- |
| 「进入 `MainSubNN` → 首次刷出」的等待 | 0…240 帧（最长 st05 `MainSub09` = 240） | 各关 `MainSub*` 头部 |
| 块内相邻刷出的静态间隔 | 最长 400–500 帧（st06 `MainSub07` = 500、st07 `MainSub08` = 400、st05 `MainSub02` = 400） | 各关 `MainSub*` 主体 |
| 块与块之间（`MainFront`/`MainLatter` 的 wait） | 160…500 帧 | 各关 `MainFront` / `MainLatter` |

→ **块内间隔（可达 500 帧）比块间间隔（可低到 160 帧）还长**，任何单一静默阈值都会
同时「过切」和「漏切」。清场也不可靠：st01/st03/st04/st07 的 `main` 会刷一个
`MapleEnemy`（`flagSet(32)` = `flags_lo` bit 0x20，**不会被 `enmKillAll` 清**，
见 `0x430710` 的 `& 0xc004a0` 判定），st01 那个活 1000+2500+600 帧（≈68 s），
场上根本不会空。所以主信号必须是**脚本结构**（子程序号），静默间隔只做认不出关卡脚本时的退路。

## 4. 落码与接线

| 层 | 文件 | 关键名字 |
| --- | --- | --- |
| 纯逻辑 | `mods/th18.v1.00a/card-expand/native/cards/lolk_core.c/.h` | `mid_boundary()`、`LOLK_MID_SUB_WINDOW`、`LOLK_MID_QUIET_GAP`、`lolk_in_t.stage_sub` / `.spawn_delta`、`lolk_state_t.sub_age` / `.sub_fired` / `.quiet` |
| 接缝 | `mods/th18.v1.00a/card-expand/native/cards/lolk.c` | `stage_sub_read()`、`stage_script_find()`、`spawn_id_read()`、`CE_ENEMY_ECL_CTX` / `CE_ECL_FM_SUB_TABLE` / `CE_EM_NEXT_ENEMY_ID` |
| 单测 | `mods/th18.v1.00a/card-expand/native/tests/test_lolk_core.c` | 子程序边界 / 同子程序再刷 / 窗口过期 / 退路 / boss 抑制 / reset |

接缝只**读**内存（`zEnemy+0xc`、`+0x1218`、`+0x6834`、`zEnemyManager+0x90`），不加断点、不改字节。

## 5. 失效场景（如实登记）

| 场景 | 后果 | 可信度 / 处置 |
| --- | --- | --- |
| 块内某个异步子程序在前一块**返回之后**才刷出 | 会把这一刷当成新波（多切一刀） | 🟡 实测零售块内异步子程序都在块内跑完（st01 `MainSub01b/c`、`MainSub02b` 都短于父块），暂不处理 |
| `MainSubNN` 自己一个敌人都不刷、全靠异步子程序且首个刷出晚于 256 帧 | 这一块漏切（并入上一波） | 🟡 实测最长 240 帧，窗口 256 留了余量 |
| 认不出关卡脚本（子程序名不是 `Main*`，例如别的 build / 别的作品） | 退到静默间隔，块内长间隔会过切 | 已实现退路 + 单测覆盖 |
| boss 入场那一帧 | 关卡脚本 `MainBoss` 里 `setChapter(43)` + 刷 boss 同时发生，旧的「切章」规则与本信号都指向同一帧 → 只切一次；但 boss 注册（`setBoss`）在 boss 自己的脚本里，那一帧 `boss_present` 可能还是 0 → 会先开一个「道中波」再被下一帧的 boss 波关掉（spawned=1、killed=0 → 0％空波） | ❓ 这是**既有**行为（原来靠 `chapter_changed` 也会这样），本次没改；要修得在 `close_wave` 里加「只含 boss 一刷」的特判 |

## 6. 与 TH15 的对照

| 想要的东西 | TH15 | TH18 |
| --- | --- | --- |
| 道中波边界 | 关卡脚本 ECL 524（一章 = 一个 `MainSubNN`）✅ | 关卡脚本 ECL 子程序号 + 刷出 ✅（本仓接线 🟡）|
| 边界时机 | `setChapter` 后**下一帧**结算 | 帧内立即（本卡同帧结算）|
| 道中章号 0 | 不结算章节奖，但计数器清零 + 快照 ✅ | 无结算端，无所谓 |
| 每章刷出 / 击破计数 | `ENEMIES_*_IN_CHAPTER` ✅ | 死槽 → 本卡差分 `enemy_count_real` |

## 7. 剩余 ❓

- 没有实机 trace，阈值（256 / 240）只有**静态读 ECL** 的依据 🟡；上实机后应按日志
  （`lolk: wave N start (mid …)` 里带 `sub`）校准。
- 关卡脚本子程序号在**换关瞬间**可能短暂读到旧对象（id 复用前的空窗）——接缝按 id 复核 +
  找不到就重认，但实机上有没有「认错对象」的窗口 ❓ 未测。
- `zEclRunContext.async_id`（`ctx+0x1014`）能区分主/异步上下文，本卡没用它（帧末读主上下文
  已经够用）；要做「异步子程序归属」的精细切分才需要 ❓。
