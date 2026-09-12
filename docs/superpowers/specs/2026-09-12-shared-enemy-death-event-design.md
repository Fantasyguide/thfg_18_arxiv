# 共享「敌人死亡 / 击破」事件（109 / 110 / 112 / 113 共用）—— 设计

> **版本**：TH18 v1.00a（`th18.exe`，imagebase `0x400000`）。本文裸地址默认属该版本；引用其他作品须写成 `th08:0x…`。
> **日期**：2026-09-12（起草）。**状态**：设计定稿待实装（SDK 面 + AUDIT + 实跑）。
> **归属**：事件与 SDK 面落 `mods/th18.v1.00a/card-expand/`（`native/sites.py` / `native/sdk.c` / `native/sdk/public.h`）。
> **用户拍板（2026-09-11）**：「敌人死亡断点**要开**，109 / 110 / 112 / 113 **共用一个** —— 一次断点、一次链表遍历、多卡回调」，并要求「共享设计现在定（越晚改越贵）」。
> **依据**：一手普查 [`server` 报告] `local/_scratch/research-death-signal-survey.md`（地址 + 指令原文全集）
> + 主线逐条复核（本文标 ✅ 的都是复核过的）；体检上下文见
> [`2026-09-11-system-card-spec-review.md`](2026-09-11-system-card-spec-review.md) §4.2 / §12。
> **可信度**：✅ 一手已复核 · 🟡 待实跑/待再核 · ❌ 已排除。

## 0. 一句话结论

**开一个断点：`0x430240`（我们命名 `Enemy__die`），在「撒掉落之前」沿卡链表广播 `on_enemy_death`；
用 `regs->retaddr` 分死因，其中 `0x42f3b7` = 被打死的主路（life ≤ 0，= 四张卡要的「击破」）；`0x42ee90` 是**炸弹秒杀**路。**
可选第二断点 `0x42d220`（`zEnemy` 析构）只做「静默移除」的收尾钩子，**不进** `on_enemy_death`。

## 1. 为什么下在这个点

### 1.1 入口与字节（✅ 一手复核）

| 项 | 值 |
| --- | --- |
| 地址 | `0x430240`（th-re-data 表里叫 `FUN_00430240`，**未命名**；`Enemy__die` 是**我们起的名字**）|
| 前 6 字节 | `55 8b ec 83 ec 14`（`push ebp` / `mov ebp,esp` / `sub esp,0x14`）|
| 断点参数 | `cavesize 6`，`expected "558bec83ec14"`；窗内无相对寻址、无窗内跳转目标 |
| ABI | **thiscall，`ecx` = `zEnemy`**；返回值恒定 `mov eax,1` |
| 调用方拿到非 0 后 | 同帧走 sweep 删掉该敌人 ⇒ **一敌一生正好一次** |

### 1.2 体内顺序（✅ 一手复核：这是「为什么必须选它」的理由）

```
0x430240  Enemy__die 入口                       ← ★ 断点下这里
   ├ 死亡特效
   ├ 0x430469  mov eax,[edi+0x623c]             ← 读掉落表（zEnemyDrop）
   ├ 0x430493  call 0x446f40                    ← ★ 引擎自己撒主掉落（极坐标）
   ├ 0x43049f  call 0x430510                    ← 既有事件 BP_ce_enemy_drop_pre（在撒之后）
   ├ 0x4304aa  mov [edi+0x623c],0               ← 掉落表用完即清零
   ├ 0x4304bb/0x4304c2 call 0x42cdd0 / 0x42ce80
   └ 0x4304c8  call 0x48d920                    ← 死亡 ECL 子程序（名字在 enemy+0x67d4）
```

⇒ **两个直接后果**：

1. 它**在撒掉落之前** ⇒ 这是全 exe 唯一能在「主掉落生成前」动手的位置 —— 卡可以在回调里改
   `enemy+0x623c`（掉落表），让引擎用**原生极坐标**（`0x446f40`）撒出改过的掉落。
   对比：`on_enemy_drop_pre`（`0x430510`）在撒**之后**，只能「额外再撒一颗」，做不了替换。
2. 它**在 `on_enemy_drop_pre` 之前** ⇒ 同一次死亡里的顺序是 **`on_enemy_death` → `on_enemy_drop_pre`**；
   两者都订阅的卡必须知道这个顺序，且**在 death 里改掉落表会影响随后的撒落与 `drop_pre` 看到的 counts**。

### 1.3 七个调用点 = 死因表（✅ 一手 grep 全集 + 逐点上下文）

| `regs->retaddr` | 所在函数（th-re-data 名）| 判据（调用点前一条）| `kind` |
| --- | --- | --- | --- |
| **`0x42f3b7`** | `EnemyData__step_game_logic` `0x42ed40` | **`cmp [enemy+0x4ff4],eax` + `setle` + `test eax,ecx`**（`0x42f39d`–`0x42f3aa`）= **life ≤ 0 的死亡判定**（`enemy+0x4ff4` = `zEnemyLife.current`）| **`KILLED`（被打死的主路）** ✅ |
| `0x42ee90` | 同上 | 前面是 `call 0x45f0f0`（伤害源结算）+ `test eax,eax` + **`cmp [esp+0x10],0`（特殊标记）** | `BOMB_KILL`（**炸弹秒杀**路）🟡 |
| `0x42f3dd` | 同上 | `or [enemy+0x5130],0x200000`（置位）+ `test [enemy+0x500c],0x2` | `OTHER`（标志驱动移除）🟡 |
| `0x4307f1` | `FUN_00430710` | killAll 一族 | `KILL_ALL` 🟡 |
| `0x430a00` | `FUN_00430910` | killAll 一族 | `KILL_ALL` 🟡 |
| `0x430c08` | `FUN_00430b20` | killAll 一族 | `KILL_ALL` 🟡 |
| `0x430f59` | `FUN_00430d30` | ECL `die` opcode | `ECL_DIE` 🟡 |

- ⚠ **本表在 2026-09-12 被订正过一次**：初稿把 `0x42ee90` 当主路、`0x42f3b7` 漏掉 —— **写反了**。
  一手复核依据：`0x42ee90` 那条的守卫里有一个**特殊标记**（`[esp+0x10] != 0`），而 `0x42f3b7` 那条
  守卫是**明确的 `life` 比较**（`enemy+0x4ff4`）；且炸弹源独有的 `flags bit2`（`or …,0x4`，只在
  `BombMarisa` / `BombReimu` 的 6 处置位、通用建源函数显式清它）对应的正是前者。
- ⇒ **四张卡要的「击破」= `kind == KILLED`，随 `retaddr == 0x42f3b7`**；炸弹秒杀单列 `BOMB_KILL`
  （109/110/112/113 要不要算击破，各自决定；110 的连击槽「击杀 boss +0f」之类的分支要一起定）。
- 🟡 **仍待实跑直方图定案**（§5 第 2 条）：七条 retaddr 的实际触发场景（尤其「炸弹打死走哪条」、
  「ECL 把 life 打成 0 会不会也走 `0x42f3b7`」）。**契约先按本表实现，实跑后校准。**

- **★ `KILLED` 的判据只有一条**：`retaddr == 0x42f3b7`（life ≤ 0 的主路）。四张卡要的「击破」就是这个。
  `0x42ee90` 是**炸弹秒杀**路，单列 `BOMB_KILL`（要不要算击破由各卡决定）。
  ⇒ 实现上**不需要任何推断**（不用比血量、不用看有没有掉落），一条 retaddr 比较即可。
- 🟡 **待实跑确认**：① 炸弹致死是否**只**走 `0x42ee90`（会不会同时也过 `0x42f3b7`）；② 变身（delete + create）会不会误报成 `KILLED`；
  ③ 七个 retaddr 的直方图（实跑打印一次就能定案）。

### 1.4 这个点**覆盖**什么、**不覆盖**什么

| 类别 | 覆盖 | 说明 |
| --- | --- | --- |
| 被自机打死 | ✅ `KILLED` | 四张卡要的就是它 |
| ECL 自删 / `die` opcode | ✅ 报，kind = `ECL_DIE` / `OTHER` | 卡自己过滤 |
| killAll 三兄弟（boss 部件清场等） | ✅ 报，kind = `KILL_ALL` | ⚠ 与「过场全清」不同：这是**打出来的**清场 |
| **ECL `deleteEnemyById`** | ❌ 不报 | `0x4363fc` → `0x42d200` 置位 → sweep **静默删**（无特效、无掉落）|
| **`EnemyManager__destroy_all`（关卡结束 / 重开）** | ❌ 不报 | `0x42ab40`，走 `[mgr+0x18c]` 全清 |
| **重开时链头尾直接清零** | ❌ 不报 | `0x44301f` / `0x443029` 写 `[mgr+0x18c]=0` / `[mgr+0x190]=0`（连析构都不走）|

- ⇒ **对四张卡来说，漏掉的这三类「静默消失」本来就不该算击破**（112 的 `cntA/cntB` 若把过场全清算进去会被刷满）
  —— **漏掉是对的**，不需要补救。
- ⚠ 唯一的例外需求：**卡在敌人身上挂了状态**（例如 109 的「活化灵」若走真敌人），敌人被全清时要收尾 ——
  这由**第二断点**（§1.5）解决，**不混进** `on_enemy_death`。

### 1.5 第二断点（可选，只做收尾）：`0x42d220`

| 项 | 值 |
| --- | --- |
| 地址 | `0x42d220`（`FUN_0042d220` = `zEnemy` 标量删除析构入口）|
| 为何 grep 不到调用 | **经虚表槽 `+0x14` 间接调用**：`0x42ab7d call [eax+0x14]`、`0x42dff2 call [eax+0x14]`（⚠ 主线复核踩过这个坑）|
| 两个调用点 | `0x42ab7d` 在 `EnemyManager__destroy_all` `0x42ab40`；`0x42dff2` 在 `EnemyManager__on_tick` `0x42df50` 的 sweep |
| retaddr 语义 | 返回到 `0x42ab80` = **全清**；返回到 `0x42dff5` = **逐个死亡（含刚被打死的）** |
| 覆盖 | 含全清在内的 **100% 移除** |

⚠ **它与主断点会重复**：被 normal kill 的敌人同帧先过 `Enemy__die`、再过析构 ⇒ 订阅「析构」的卡会**收到第二遍**。
⇒ 本设计**不把它做成 `on_enemy_death`**，而是**独立事件 `on_enemy_despawn`**（语义 = 「这只敌人没了，且不是击破」），
并把「也会带上刚死的敌人」写进契约；要精确去重时用 `enemy_id` 环（§2.6，P2）。

### 1.6 断点现场的可用数据（✅ 一手）

| 数据 | 来源 | 备注 |
| --- | --- | --- |
| **`ecx` = `zEnemy`** | 每个调用点都就地 `mov ecx,<enemy>`（证据 `0x42ee85 mov ecx,[ebx+0x5588]` = `zEnemyData` 的 owner 回指；`0x4307ec` 前 `mov ecx,ebx` = 结点自指针）| **唯一可信的寄存器** |
| `eax` / `ebx` / `esi` / `edi` | — | ❌ 各调用点含义不同，**不可用** |
| `enemy_id` | `enemy+0x6830`（单调递增）| ✅ 用于去重 / 卡侧记账，**不要缓存指针** |
| 坐标 | `enemy+0x1270` / `+0x1274` | ✅ 爆风中心 / 掉落位置 |
| `flags_lo` | `enemy+0x635c`（`& 0x31` = 算作真敌人；`& 0xc000021` = 锁定黑名单）| ✅ 卡侧判 boss / 真敌人 |
| **掉落表（未消费）** | `enemy+0x623c`（= `zEnemyDrop`，与 `on_enemy_drop_pre` 的 `counts` 同一块内存）| ✅ **可写** ⇒ 引擎随后用原生极坐标撒 |
| 链表 | **仍然挂着**（本点是「死」，摘链在同帧 sweep）| ✅ 但**别在回调里改链表**（§2.5）|
| 槽索引 | — | ❌ 没有（`enemy+0x636c` 常态 −1）|

### 1.7 成本（✅ 一手）

- 敌人**无池**：`EnemyManager__allocate_new_enemy` `0x42d7d0` 是 `push 0x683c; call 0x48dc71`（堆 `new`），**无上限**。
- **单帧最坏调用次数 = 当帧活敌数**：killAll 在一个循环里把全场逐个送进 `0x430240` ⇒ 清场帧 **100–300 量级**。
- 派发成本：卡链（guard 256）× 27 条行为线扫 ≈ 7e3 次比较/次 ⇒ 可接受。
- ⚠ **真正的风险在回调体**：每死扫全场 = **O(n²)**（109 极易踩，见 §4.1）。

## 2. 事件契约

### 2.1 名字与形状

```c
/* kind */
#define CE_EDEATH_KILLED     0u   /* 被打死的主路（retaddr 0x42f3b7，life ≤ 0）—— 四张卡要的「击破」 */
#define CE_EDEATH_BOMB_KILL  1u   /* 炸弹秒杀路（retaddr 0x42ee90，伤害源带炸弹 bit2） */
#define CE_EDEATH_ECL_DIE    2u   /* ECL die opcode */
#define CE_EDEATH_OTHER      3u   /* 脚本 / 计时 / 标志驱动的移除 */
#define CE_EDEATH_KILL_ALL   4u   /* killAll 一族（打出来的清场） */

typedef struct {
    uint32_t id;          /* enemy+0x6830（单调递增，可用于去重 / 记账） */
    uint32_t kind;        /* CE_EDEATH_* */
    uint32_t flags;       /* bit0 = boss？(判据 ⏳ 见下) bit1 = 真敌人 = (flags_lo & 0x31)==0 bit2 = 掉落表非空 */
    float    x, y;        /* enemy+0x1270 / +0x1274 */
    int32_t  drop_main;   /* enemy+0x623c 当前值（未消费；改了就会影响引擎随后的撒落） */
    void    *enemy;       /* ★ 只在本次回调内有效，禁止缓存（同帧 sweep 会删） */
} ce_enemy_death_t;
```

> ⚠ **`flags.bit0`（boss）的判据还没定**：`engine.h:368` 的 `CE_ENEMY_OWN_BOSS_ID 0x636c`「非 0 = boss」
> **不能当判据**（一手 2026-09-12 订正：`grep 636c` 全集只有 1 读（`0x42d308`，被 `flags_lo & 0x800000` 门控，
> 当 `CE_EM_BOSS_IDS` 的**下标**用）+ 1 写（构造写死 −1）⇒ 它是 boss 数组下标、默认 −1）。
> ⇒ 110 的连击槽「击杀 boss +0f」、109 的「非 boss 门」都受此影响，需要另找 boss 判据（**列为待查一手**）。

> ⚠ **`enemy+0x6320 > 0` 是「排除真敌人」的第二杠杆**（`0x42d460` / `0x43c137` 两处判定都带上它），
> 且**不碰伤害可及位**（`flags_lo & 0x21`）⇒ 想让某类敌人「不被计数/门控看见、但仍可打」时优先考虑它。
> 🟡 字段语义未定，用前先一手确认。

> ⚠ **`flags` 的 bit1 极性已订正（2026-09-12）**：`flags_lo & 0x31` **置位 = 被排除在「真敌人」之外**
> （不是「算作真敌人」）。铁证：计数函数的收尾是
> `0x42d472 test ecx,ecx` / `0x42d477 cmovne eax,edx`（**置位 ⇒ 不加计数**）；`0x43c12a` 同向
> （置位 ⇒ 跳过 boss 血条 / 门控那段）。⇒ 回调里报 `真敌人` 时应当用 **`(flags_lo & 0x31) == 0`**。
> ⚠ 另注：`0x31` 含 bit0/bit5，而伤害只在 `flags_lo & 0x21 == 0` 时可及 ⇒ **置位 = 伤害免疫**（见 §4.1）。

```c
typedef struct {
    uint32_t id;
    uint32_t full_clear;  /* 1 = EnemyManager__destroy_all / 重开；0 = sweep（含刚被打死的） */
    void    *enemy;
} ce_enemy_despawn_t;
```

回调（加在 `ce_hooks_t` **尾部**，见 §3.2）：

```c
void (*on_enemy_death)(ce_card_t *, const ce_enemy_death_t *);
void (*on_enemy_despawn)(ce_card_t *, const ce_enemy_despawn_t *);  /* P1，可后做 */
```

### 2.1.1 ★ `KILL_ALL` 的用途分界（2026-09-12 定，实装期裁决）

判据不是「算不算死」，而是「**这个 kind 会不会被换成玩家的收益**」：

- **给奖励 / 加计数 / 得分 / 掉落的用途 ⇒ 必须排除 `KILL_ALL`**（109 爆风、110 大信仰点、112 飞碟计数）。
  两个理由：① 过场清场会把计数刷满；② `0x4307f1` / `0x430a00` / `0x430c08` 三路在调 `0x430240` **之前**
  已经 `memset(enemy+0x6240, …)` 并清掉主掉落 ⇒ 这些敌人**本来就不该算击破**（它们在事件点已经「没有掉落」了）。
- **纯状态判断 ⇒ 把 `KILL_ALL` 算进去是对的**（108 妖机体：「这一槽的使魔没了 ⇒ 标 broken」；清场时使魔确实消失，
  若不算，卡的内部状态会与场上不一致；下一波 ECL `etNew+etOn` 重新武装时状态自然复位）。
- ⇒ 用 `KILL_ALL` 的卡请在注释里**写清自己属于哪一类**（108 与 109/110/112 的做法**故意相反**）。

### 2.2 触发时机与顺序

- **一次**：`0x430240` 命中 → 建 `ce_enemy_death_t`（栈上）→ **沿卡链一趟**广播 → 放行原指令。
- **卡间顺序** = 卡链表顺序（与既有事件一致；SDK 现有派发即此顺序，别另立规矩）。
- **同帧内与其他事件的先后**：`on_enemy_death`（`0x430240`）→ `on_enemy_drop_pre`（`0x430510`）
  →（若挂了）`on_item_*`（道具实际生成在那之后）。

### 2.3 一敌一次 / 去重

- 主断点天然一敌一次（`0x430240` 恒定返回 1，调用方立即返回并 sweep）⇒ **不需要去重**。
- 第二断点（P1）会**重复带上刚死的敌人** ⇒ 订阅 `on_enemy_despawn` 的卡自己按 `kind`/`full_clear` 过滤；
  需要严格去重时见 §2.6。

### 2.4 回调里允许 / 禁止

| 允许 | 禁止 |
| --- | --- |
| 读敌人字段（`id` / 坐标 / `flags_lo` / 掉落表）| **析构任何敌人**（`0x42d220` 那条路）⇒ UAF |
| 写 **本敌人** 的掉落表 `enemy+0x623c`（影响随后撒落）| 改敌人链表 / 摘链 / 遍历中改结构 |
| spawn 自建 VM 实体、撒道具、加分、消弹（见 §4.1 的性能约束）| 缓存 `enemy` 指针到下一帧 |
| 调 `ce_destroy_card`（**但不许删卡链表里的「下一张」**，SDK 派发循环在 hook 返回后才读 `node+0x4`）| 依赖 `eax/ebx/esi/edi` 的调用点语义 |

### 2.5 重入边界（✅ 一手）

- 死亡断点**必然在敌人链表遍历栈里**（sweep 先存 next：`0x42dfc2 mov edi,[esi+0x4]`；killAll 另存 `[ebp−8]`）
  ⇒ 回调里**不许析构别的敌人**；**新建敌人安全**（尾插）。
- SDK 自己的派发循环（`sdk.c` 的卡链遍历）在 hook 返回后才读下一结点 ⇒ 回调里删「下一张卡」会踩释放内存。

### 2.6 第二断点的精确去重（P2，可后做）

- 方案：SDK 维护一个 **`enemy_id` 环**（512 × u32 = 2KB）：主断点报过的 id 入环；
  `0x42d220` 命中时若 id 在环内则**只报 `on_enemy_despawn`**（不再另报死亡）。
- 用 `enemy_id`（单调递增、不复用）而不是指针（会被复用）。

## 3. SDK 落地清单（**五处同落，缺一处就不算装上**）

> ⚠ **硬教训**：事件断点的**唯一真源 = `native/sites.py` 的 `SDK_SITES`**，
> 且 **handler 存在 ≠ 断点已装** —— 反例 `BP_ce_graze`：handler 在 `sdk.c` 写好了，
> `sites.py:802` 那行却是**注释掉的**（2026-09-10 二分「游戏打不开」临时摘掉）。

### 3.1 `native/sites.py` 的 `SDK_SITES`（草案）

```python
    ("ce_enemy_death", 0x430240, 6, "558bec83ec14",
     "FUN_00430240（我们命名 Enemy__die）入口：push ebp/mov ebp,esp/sub esp,0x14，无相对寻址。"
     "thiscall，ecx = zEnemy；此刻**掉落尚未生成**、链表仍挂着。"
     "handler 用 regs->retaddr 分死因（0x42f3b7 = 被打死主路；0x42ee90 = 炸弹秒杀），沿卡链表调 on_enemy_death(&ev)"),
```

（第二断点 `0x42d220` 的条目在 P1 再加：`cavesize 5` / `expected "55 8bec 6aff 68"` 需实测确认。）

### 3.2 `native/sdk/public.h`

- 新增 `ce_enemy_death_t` / `ce_enemy_despawn_t` / `CE_EDEATH_*`。
- `ce_hooks_t` **尾部追加**两个函数指针（⚠ 追加会改 `CE_CARD` 桩偏移表 —— `sdk.h` 里那张
  `CE_STUB*` 是**按虚表槽**排的，属于**卡的虚表**；而 `ce_hooks_t` 是 SDK 自己的稳定面，
  两者别混。新增字段只能加在 `ce_hooks_t` 尾部，并同步 `SDK.md`）。

### 3.3 `native/sdk.c`

- 头注释里加 `BP_ce_enemy_death 0x430240 …` 一行（照既有 11 条的写法）。
- handler：从 `regs->ecx` 取敌人 → 组 `ce_enemy_death_t` → 沿卡链派发 → `return BP_EXEC_ORIGINAL`。
- **不改控制流**（这条事件是只读广播 + 允许写掉落表，不需要 `retaddr` 改写）。

### 3.4 文档

- `mods/th18.v1.00a/card-expand/SDK.md` §6 事件表加两行（断点 / 回调 / 本批用）。
- 顺手订正 **§6 的「沿 `mgr+0x18` 卡链表」→ 「首结点在 `mgr+0x1c`」**（`+0x18` 是表头）。
- 顺手订正 `ce_enemy_drop` 的 title：`0x430510` 的 **`ecx` = `zEnemyDrop`（`enemy+0x623c`）**，不是「敌人」。

### 3.5 AUDIT

- `mods/th18.v1.00a/card-expand/AUDIT.md` 新开一节（照 [`mods/_template/AUDIT-checklist.md`](../../../mods/_template/AUDIT-checklist.md)），
  必含：断点字节对账、窗内无相对寻址/无窗内跳转目标、`ecx` 来源逐调用点、`retaddr` 分类表、
  重入禁令、性能预算、回放安全（只读，卡侧不得 `ce_rand()`）、实跑清单（§5）。

## 4. 四张卡怎么用

| 卡 | 订阅 | 过滤 | 干什么 |
| --- | --- | --- | --- |
| **109** 花映塚 | `on_enemy_death` | `kind == KILLED` | 蓝色爆风（半径内消**白弹** T ≤ 4.0）+ 按连爆质量回气槽 |
| **110** 风神录 | `on_enemy_death` | `kind == KILLED && drop_main == 2` | 大信仰点：**改 `enemy+0x623c` 让引擎自己撒**（比 `drop_pre` 加撒更省）|
| **112** 星莲船 | `on_enemy_death` | `kind == KILLED` | `cntA` / `cntB` 计数 —— **必须排除 `KILL_ALL` / 清场**，否则计数器被过场刷满 |
| **113** 神灵庙 | `on_enemy_death` | `kind == KILLED` | 击破产灵（位置 = `x,y`）|

### 4.1 ⚠ 109 的性能陷阱（必须写进卡侧实现）

- 清场帧一次回调 100–300 次，**回调里扫全场弹池 = O(n²)**（弹池 2000 槽 × stride `0xfa0`）。
- ⇒ 109 的正确做法：回调里**只登记**（位置 + 半径进一个小队列，O(1)），
  **帧末**（`on_draw` / `on_tick_2`）再统一消弹；或只处理「本帧第一次死亡」的爆风。

## 5. 实跑验证清单

1. **计数**：打死一只道中妖精 → `on_enemy_death` **恰好 1 次**（日志计数）。
2. **死因**：分别用 ① 自机弹打死 ② 炸弹打死 ③ boss 符卡结束（ECL 自删）④ killAll（清场）
   → 打印 `retaddr` 直方图，核对 §1.3 表（**这是唯一需要实跑才能定案的项**）。
3. **清场不误报**：过场 / 重开时**不应**收到 `KILLED`。
4. **顺序**：同时订阅 `on_enemy_death` 与 `on_enemy_drop_pre` 的卡，日志顺序必须是 death → drop_pre。
5. **掉落表改写**：在 death 里写 `drop_main`，确认引擎撒出的是改后的种类（110 用）。
6. **不回归**：装 117（咆哮）+ 114（辉针城）+ 116（季节）各跑一局，无崩溃 / 无事件串扰。
