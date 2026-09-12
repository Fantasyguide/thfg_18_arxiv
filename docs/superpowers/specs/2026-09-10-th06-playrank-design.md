# 红魔乡（card-expand id 106）—— PlayRank + 无判定点 + 冴月麟 设计

> **版本**：TH18 v1.00a（`th18.exe`，imagebase `0x400000`）。本文裸地址默认属该版本；引用其他作品写成 `th06:0x…`。
> 日期：2026-09-10。状态：**设计定稿、待开工**。归属：`mods/th18.v1.00a/card-expand/`。
> 原作一手：**GensokyoClub/th06 完整反编译（C 源码）** + 本地 `local/vendor/th-re-data/data/th06.v1.02h/`。
> 排队来源：[`IDEAS.md`](../../../mods/th18.v1.00a/card-expand/IDEAS.md) 的 **106 红魔乡**（整蛊卡）。

## 0. 一句话 + 用户定案

**被动卡「红魔乡」= 一张整蛊卡**，三个件：

1. **低速不显示判定点** —— 红魔乡独有（别的作聚焦都显示自机受击点）；
2. **PlayRank 动态难度** —— **算法照 th06 源码 1:1 还原**，影响面**自研**；
3. **冴月麟彩蛋** —— 往一块**不参与任何判定**的内存里写入「冴月麟」。

**用户 2026-09-10 定案**：
- rank 机制**肯定得自己改**（TH18 的 rank 是死代码，见 §2.3）；
- 先做**简单有效**的影响面（MVP），不追求全量复刻；
- 定位就是整蛊卡，**不必做强**（其他卡已经够超模）。

## 1. 从 th06 抄什么 / 不抄什么

| th06 原件 | 一手 | 本卡怎么办 |
| --- | --- | --- |
| `GameManager.rank / maxRank / minRank / subRank` | th06 反编译 | **照抄**（§2.2） |
| `IncreaseSubrank` / `DecreaseSubrank` | th06 反编译（逐字一致） | **照抄**（§2.2） |
| 难度表（含 **EASY 下限 = 12**） | th06 反编译 | **照抄**（§3） |
| rank 增减的 12 条条件 | th06 反编译 + 用户来源 | **照抄**，事件映射到 TH18（§2.2 / §4） |
| rank 驱动：`Enemy::BulletRankAmount/Speed`（`scale*(high-low)/32+low`） | th06 反编译 | **改**：TH18 无这套 per-enemy 插值 → 走 §2.3 自研影响面 |
| 低速不显示判定点 | 原版行为 | **照抄**（§2.1，TH18 侧需主动隐藏） |
| 「冴月麟」内存写入 | 用户 2026-09-10 | **新加**（§2.4） |

## 2. 机制

### 2.1 低速不显示判定点

**TH18 现状（一手反汇编）**：判定点指示在 `Player::sub_45b170` `0x45b170` 里维护——
- `player+0x476cc` = **聚焦标志**（0 = 高速 / 1 = 低速；由 `0x45b3fa` 的输入位算出）
- 聚焦且 `player+0x476b0 == 0` 时：`AnmManager__instantiate_vm_to_world_list_back` `0x405bf0`，
  **script `0x1a`、layer `0xe`** → 句柄写回 **`player+0x476b0`**
- 聚焦帧还会写它的 **scale（`vm+0x5c` / `vm+0x60`）** 并 `OR [vm+0x534], 0x8`
- **离开聚焦**时：`AnmManager::interrupt_tree` `0x488be0`（参数 `(handle, 1)`）+ 句柄清 0

**本卡实现（用户 2026-09-10 定：用 alpha = 0）**：
```c
uint32_t id = *(uint32_t*)(player + 0x476b0);   // 判定点指示的 anm id
if (id) ce_anm_set_color(id, 0x00FFFFFFu);      // color_1(vm+0x524) 的 alpha 位置 0 → 全透明
```
- SDK 现成：`ce_anm_set_color(id, rgba)` 写 `vm + CE_ANM_VM_COLOR1`（`+0x524`，D3DCOLOR `0xAARRGGBB`）。
- **低速也保持 0**：把 alpha 写 0 即可（不是靠"离开聚焦才透明"）。
- ⚠ 若脚本自己每帧重设颜色（`color()`/`sprite()`），需像 LoLK 弹染色那样在 **`on_draw` 每帧重写**
  （season.c 有同款注记：写死 alpha 会把脚本的 `alphaTime` 按掉 —— 这里正是我们要的效果）。
- **备选**：把 `scale_2`（`vm+0x5c/0x60`）置 0；或 BP `0x45b44a` 跳过创建。
**备选（更彻底，但要断点）**：BP 在 **`0x45b450`**（`PUSH 0x1a` 在 `0x45b44a`，所以断点要下在**那条 call** 上），卡在场时跳过创建调用。

> ⚠ 时序：SDK 文档写明 **AbilityManager（`on_tick_2`，优先级 0x16）先于 Player（0x17）**，
> `on_tick` 也在 Player 复位前 → **只有 `on_draw`（绘制阶段）晚于 Player tick**，隐藏必须放那里。

### 2.2 PlayRank（照 th06 源码 1:1）

**存储**（卡内状态，`int32` × 4）：`rank` / `maxRank` / `minRank` / `subRank`。

**增减**（逐字照抄）：
```c
void IncreaseSubrank(i32 n) { subRank += n;
    while (subRank >= 100) { rank++; subRank -= 100; }
    if (rank > maxRank) rank = maxRank; }        // 小数不修正

void DecreaseSubrank(i32 n) { subRank -= n;
    while (subRank < 0) { rank--; subRank += 100; }
    if (rank < minRank) rank = minRank; }        // 小数不修正
```
**每关开场**：`rank = 难度表初值(16)`；`minRank / maxRank` 按难度设；**`subRank = 0`**。
**不进 replay**（照原版：小数部分每关清零）。

**12 条增减 → TH18 事件映射**（§4 有表）。

### 2.3 rank 影响面（自研）—— TH18 侧一手结论

**TH18 的 rank 是死代码**（2026-09-10 复核，见 IDEAS §1.6）：
- 全局 **`0x4ccd08`**（= `GlobalsInner+0x2c`）：全程序 **185231 条指令只有 2 条读**，**没人写**，初值 0；
- ECL 的 `529–534`（`rankF3/F5/F2/I3/I5/I2`）读它，但 **22 个关卡 ECL 里一次都没用过**（0 次）。

⇒ **写 `0x4ccd08` 没有任何效果**，不能搭车。

⏸ **用户 2026-09-11 定：rank 影响面完全搁置。** 本卡只做到「一个**没有任何意义**的 rank 系统」——
rank / subRank 照 th06 源码 1:1 算，但**不产生任何游戏效果**（也不显示，见 §5）。
理由：再往下是**弹幕设计层**的事，不属于这张卡。§2.3 下面那条难度闸路线**留档待日后**，本批**不做**。

**（留档，本批不做）难度闸路线：**
```
rank  →  f(真难度, rank)  →  hook 0x42aa20（ECL 的难度取值器）
                                   ↓
             所有 ECL 的 ins_535 diffI / ins_536 diffF 一起偏移
                                   ↓
                    波数 / 波间隔 / 弹速 **整体切档**（离散 E/N/H/L 四常量，不是连续缩放；不用改 ECL）
```
- `0x42aa20` 本体 = `mov eax,[ecx+0x40]`（`GLOBALS+0x40` = **`0x4ccd00` 难度**），
  按 0/1/2/3 跳表在 **E/N/H/L 四个常量**里挑一个；
  **xref 只有 ECL 派发器 `FUN_00430d30`** → **hook 它只影响 ECL**（不动结算 / 分数 / 刷敌）。
- ⚠ **别直接写 `0x4ccd00`**：它还被 `EnemyManager__allocate_new_enemy` / `GameThread__end_stage` /
  `ScoreFileShotData::add_to_top10_if_worthy` 读 → 会波及结算与记录。
- ⚠ **门控不动**：ECL 的 `rank_mask` 走 **per-context `difficulty_mask`**（非 `0x42aa20`）→
  只改 `diffI/diffF` 的值，不改「哪些指令会跑」。
- **备选（零 hook）**：只做「**缩短 boss 符卡两波间隔**」这类自定效果（用户提过），实现 = 找波循环的
  `ins_23` 等待点…但那要改 ECL，反而更重 → **优先 hook 版**。

### 2.4 冴月麟（彩蛋）

- 选一块**不被读取、不参与任何判定**的区域写入 UTF-8 字符串「`冴月麟`」：
  **优先 = 本卡 DLL 的静态字符串常量**（`static const char k_sakuyue[]`），
  次选 = codecave 里已有的空闲字符串区。
- 写入**一次**（`on_load` 或首帧），**零逻辑影响**。
- 卡面文案不提它（靠玩家自己发现）。

## 3. 数值（th06 反编译一手）

**难度表**（`DifficultyInfo { rank, minRank, maxRank }`）：

| 难度 | rank（初值） | minRank | maxRank |
| --- | --- | --- | --- |
| EASY | 16 | **12** | 20 |
| NORMAL | 16 | 10 | 32 |
| HARD | 16 | 10 | 32 |
| LUNATIC | 16 | 10 | 32 |
| EXTRA | 16 | 14 | 18 |

> ⚠ 用户最早来源写 EASY 下限 10 —— **以反编译的 12 为准**。

**rank 增减 12 条**（`subRank` 单位：100 = 1.00 rank；**下表全部有 th06 源码出处**）：

| 方向 | 条件 | th06 源码 | 原值 | **本卡值** |
| --- | --- | --- | --- | --- |
| 增 | 帧数条件 `f % (2400 − p×240) == 0` | `EnemyManager.cpp:170` `Inc(100)` | +1.00 | **+1.00 照抄**（`p` = 残机数；用户：残机数量级无差）|
| 增 | 擦弹（含 Bomb 擦弹） | `Player.cpp:1394` `Inc(6)` | +0.06 | **+0.06**（用户 2026-09-11 定：**不除**）|
| 增 | 吃命道具 | `ItemManager.cpp:299` `Inc(200)` | +2.00 | **整命（item type 5）→ +2.00 照抄**；**命碎（type 4）→ +0.7（定值；3 碎 = +2.1）** |
| 增 | 吃 B 道具 | `ItemManager.cpp:291` `Inc(5)` | +0.05 | **整炸（item type 7）→ +0.05 照抄**；**B碎（type 6）→ +0.02** |
| 增 | 吃蓝点（`y < 128`） | `ItemManager.cpp:234` `Inc(30)` | +0.30 | **+0.06**（**÷5**）|
| 增 | 吃蓝点（`y ≥ 128`） | `ItemManager.cpp:238` `Inc(3)` | +0.03 | **+0.006**（**÷5**）|
| 增 | 吃**小** P | `ItemManager.cpp:205` `Inc(1)` | +0.01 | **+0.002**（**÷5**）|
| 增 | 吃**大** P | **无调用**（`ITEM_POWER_BIG` 分支里没有 Subrank）| **0** | **0（不加）** |
| 减 | 道具掉出屏幕 | `ItemManager.cpp:145` `Dec(3)` | −0.03 | **−0.03 照抄**（用户：**不除**）|
| 减 | 放 Bomb（含决死 B） | `Player.cpp:185` `Dec(200)` | −2.00 | **−2.00 照抄** |
| 减 | Miss（不含决死） | `Player.cpp:225` `Dec(1600)` | −16.00 | **−16.00 照抄** |

**归一化规则（用户 2026-09-10 定）**：
- **÷5 只作用于「蓝点 == 金钱」与「小 P」两条通道**（TH18 道具量 ≈ 5× th06）。
  `EOSD_ITEM_RATE_RATIO = 5` 做成常量，实跑调。
- **出屏 / B 道具 / miss / Bomb / 帧数 不除**。
- **资源类分「整 / 碎」两档，而且两档的来路不同**（2026-09-10 一手，全表见 §3.1）：
  **整命 / 整炸 = 只能「商店买」**（card id 1 / 2，登记表 weight 0 ⇒ 必上架）**或**由 ECL 直接掉（`item type 5` / `7`）；
  **命碎 / B碎 = 只能「掉落」**（`item type 4` / `6`，或卡道具 `0x10` / `0x11` → card id 3 / 4）。
  数值：整 **+2.00 / +0.05 照抄**；碎 **+0.7（定值，3 碎 = +2.1）/ +0.02**（B 侧 3 碎 = 1 张，一手）。
  依据：**每命碎片数 = 3**（普通表 `0x4b40c8` ×24、Extra 表 `0x4b3ff4` ×9 全是 3；选表只看 `difficulty == 4`）。
  ⇒ 代价：**碎片阈值对 rank 无意义**（只影响几颗进位），实现**不读表**，直接按「收到一颗 = +0.7 / +0.02」。
- **大 P 不加 rank**（th06 源码确认）。

### 3.1 资源通道全表 + 卡登记表（2026-09-10 一手）

**道具分派器** `ItemManager__on_tick__body` `0x44625f`：`type = *(u32*)(item+0xc78)`；
`type-1 ≤ 0x12` → byte 表 **`0x44685c`** 取索引 → jump 表 **`0x446830`**：

| item type | 处理 | 语义 | 本卡 |
| --- | --- | --- | --- |
| 1 | `0x4462cc` | P 点 | 小 P +0.002 |
| 2 | `0x4463ba` | 金钱（== 蓝点）| +0.06 / +0.006 |
| 3 | `0x4462d8` | 大 P | 0 |
| **4** | `0x4463c6` → `0x457570` | **命碎**（累加，满难度表值 → 进位 +1 命）| **+0.7** |
| **5** | `0x4463d6` | **整命**（`CURRENT_LIVES++`，钳 `LIVES_STOCK`）| **+2.00** |
| **6** | `0x446465` | **B碎**（`BOMB_FRAGMENTS++`，满硬编码 3 → `0x457690`）| **+0.02** |
| **7** | `0x4464d5` | **整炸**（`CURRENT_BOMBS++`，钳 `MAX_BOMBS`）| **+0.05** |
| 8 | `0x44627f` | 满 P → 分数 | 0 |
| 9–14 | `0x446547` | 分数（`f(MONEY)`）| 0 |
| 15 | — | 空 | 0 |
| **0x10–0x13** | `0x446531` | **卡道具** → `allocate_new_card(0x4b4020[type*8], mode 0)` | 见下 |

**卡道具 ↔ 卡 id**（`0x4b4020`，stride 8，`+4` 全 `0xffffffff`）：**`0x10`→3 残碎 / `0x11`→4 B碎** / `0x12`→5 金钱+50 / `0x13`→6 火力+0.5。
⇒ **「整残 / 整炸」根本不在卡道具表里**。

**卡登记表 `zTableCardData` `0x4c53c0`（stride `0x34`）**：

| card id | 卡 | `+0x18` dmode | `+0x1c` 可重复买 | `+0x14` weight | 后果 |
| --- | --- | --- | --- | --- | --- |
| **1** | `CardLife` 整残 | 0 | **1** | **0** | **loop1 必上架**（只能买）|
| **2** | `CardBomb` 整炸 | 0 | **1** | **0** | **loop1 必上架**（只能买）|
| **3** | `CardLifeFragment` 残碎 | 0 | 1 | **6** | 不进随机池、也不上架 ⇒ **只掉落** |
| **4** | `CardBombFragment` B碎 | 0 | 1 | **6** | 同上 ⇒ **只掉落** |
| 5 / 6 | 金钱+50 / 火力+0.5 | 0 | 1 | 0 | 必上架 **且** 可掉落 |

- 随机池 `CardShop__pick_weighted_random_offer` `0x416f50` 第 4 步 = `weight != 0 && weight != 6`
  （`0x4170a0` / `0x4170cf` 两条 `je 拒绝`）⇒ **0 与 6 都是「不进随机池」的哨兵**，不是权重。
- 必出 loop1 = `is_available_at_stage() == 1 && weight == 0`
  （[`05-shop-and-money.md`](../../../engine/card/th18/05-shop-and-money.md) §4）。
- ⚠ **`+0x1c == 1` = 可重复购买**（随机池第 1 步是「`owned[id] == 0` **或** `+0x1c != 0`」）——
  ⇒ 同一局里**能反复买整残 / 整炸**，`owned[]` 差分会漏掉第二次（见 §4）。

## 4. 落地（TH18 侧）

| 件 | 落地 |
| --- | --- |
| 判定点隐藏 | **`on_draw`**：`id = player+0x476b0` → `ce_anm_set_color(id, 0x00FFFFFF)`（`color_1` = `vm+0x524` 的 alpha 置 0；脚本若每帧重设则每帧重写）|
| 帧数条件 +1.00 | `on_tick_2` 自建帧计数（原版门：除暂停/对话/Boss 战/3·6·EX 中 boss 外每帧 +1 —— ⏳ **门控细节留给实现时一手反编译**，用户 2026-09-10 定：不阻塞 spec）|
| 擦弹 +0.06 | **G1 帧差**：`GRAZE 0x4ccd0c` 的增量（115 / 116 先例）|
| **整合命 +2.00** | **`CURRENT_LIVES` `0x4ccd48` 正向增量**（`on_tick_2` 差分；覆盖道具 `type 5` / **商店买 card 1** / 任何来路）。⚠ **同帧 `LIFE_FRAGMENTS` 净减 ⇒ 抑制**（那是碎片进位，不是收到整合资源）|
| **命碎 +0.7**（3 碎 = +2.1） | **`on_item_collect`：`type 4` 或 `0x10`** —— 每颗都算，**含触发进位的那一颗** |
| **整炸 +0.05** | **`CURRENT_BOMBS` `0x4ccd58` 正向增量**（道具 `type 7` / **商店买 card 2**）。⚠ 同帧 `BOMB_FRAGMENTS` 净减 ⇒ 抑制 |
| **B碎 +0.02**（3 碎 = 1 张）| **`on_item_collect`：`type 6` 或 `0x11`** |
| **蓝点（== 金钱）+0.06 / +0.006** | `on_item_collect`：**item type 2**（金钱）；**分档阈值 = 道具回收线高度 `player+0x47998`**（用户 2026-09-10 定：把 th06 的 `y<128` 换成 TH18 的回收线）：**收集时 `y < 回收线` → +0.06；否则 +0.006** |
| 小 P +0.002（装了 110/111 时 **×5**）| `on_item_collect`：**item type 1**；×5 规则见 §4.1 |\| ce_owned(111)`（SDK 现成，不算跨卡耦合）|
| 大 P | 不加（type 3 不处理）|
| 道具出屏 −0.03 | ✅ **一手定案（2026-09-10）**：引擎出界删除在 `ItemManager__on_tick__body` `0x445e38`（`state(0xc74)=0` + 删 anm + 摘链，**位置不清**），判据 = **`|pos.x| >= 200.0`（`0x4b9340`；注意是 `>=`）或 `pos.y > 472.0`（`0x4b93a4`）**。`on_item_frame` 断点在 **`0x446ee3`（每帧、池 body 之前）** → 卡侧直接**逐帧扫池预测**：alive 槽（`state != 0`）且已越界 → 记一次出屏（引擎同帧就删，故每颗只算一次）|
| 放 Bomb −2.00 | `on_bomb_spent`（现成）|
| Miss −16.00 | `on_death_after_deathbomb`（**非决死**；117 / 69 同族）|
| **rank → 难度** | ⏸ **用户 2026-09-11 定：完全搁置**（本批只做「没有任何意义的 rank 系统」，留档见 §2.3）|
| 冴月麟 | `on_load` 一次性写静态字符串常量 |
| **探测方式为什么「整」「碎」分工** | 见下方 §4.1 |
| **每关开场 card 36 / 37 +0.7 / +0.02** | 用户 2026-09-11 定：**算获得、给 rank**；不经过道具，`on_stage_start` 显式补（见 §4.1）|

### 4.1 为什么「整」用库存增量、「碎」用 `on_item_collect`

**「整」只能走增量**：商店买 card 1 / 2 走的是**引擎自己的卡 ctor/dtor**，`on_item_collect` 根本不经过；
且 `+0x1c == 1` = **可重复购买**（§3.1）⇒ `owned[]` 差分只有第一次 0→1 会跳变，第二次买就漏
（卡 1/2 `weight == 0` ⇒ 连随机池都不进，没有第三条来路）。

**「碎」不能走增量**：收第 3 颗时计数器 `2→3→0`（净 −2）**并当场进位 +1 命 / 炸**
（`0x457570` / `0x446465`）——只认正向增量会**丢掉第 3 颗的 +0.7**、反把进位当成 +2.00
（3 命碎会算成 +3.4）。

⇒ 分工：碎片**逐颗**在 `on_item_collect` 计；整命 / 整炸在库存增量里，**同帧碎片净减时抑制**。

## 5. UI

- **不显示 rank 数字**（用户 2026-09-10 定案）。本卡**没有 HUD 元素**。

## 6. 文件与测试

```
native/cards/eosd.c       行为（on_load / on_tick_2 / on_draw / on_item_* / on_bomb_spent / on_death_*）
native/cards/eosd_core.c  纯逻辑：rank/subrank 增减 + 难度表 + 12 条条件（主机单测）
patch/th18/cards.js       "106": { … }（实装时把空壳 desc 换掉）
```

单测（`make test-host`）覆盖：`IncreaseSubrank`/`DecreaseSubrank` 的进位/借位与上下限夹取、
难度表初值、12 条增减的符号与量、帧数条件公式、每关清零。

## 7. 明确不做

- **不写 `0x4ccd08`**（死代码，写了没用）；
- **不直接写 `0x4ccd00`**（波及结算 / 分数 / 刷敌）；
- 不照抄 th06 的 `Enemy::BulletRankAmount/Speed`（TH18 无这套 per-enemy 插值）；
- 不做全量 ECL 改写（MVP 只走难度闸）。

## 8. TH18 接缝

| 需要 | 现状 | 落地 |
| --- | --- | --- |
| 聚焦位 / 判定点句柄 | `player+0x476cc` / `player+0x476b0` ✅（一手） | §2.1 |
| 取 VM | `ce_anm_get_vm_with_id` `0x488b40` ✅ | §2.1 |
| 擦弹计数 | `GRAZE 0x4ccd0c` ✅ | §4 |
| 残机 / 命碎 | `CURRENT_LIVES` `0x4ccd48` / `LIFE_FRAGMENTS` `0x4ccd4c` ✅ | §4 |
| 符卡 / B 碎 | `CURRENT_BOMBS` `0x4ccd58` / `BOMB_FRAGMENTS` `0x4ccd5c` ✅（3 碎 = 1 张） | §4 |
| 商店购买整残 / 整炸 | ✅ **不用新断点**：买 card 1/2 的效果就是抬 `CURRENT_LIVES` / `CURRENT_BOMBS`，被上一行的差分顺带吃到 | §3.1 / §4 |
| 放 Bomb | `on_bomb_spent` ✅ | §4 |
| Miss（非决死） | `on_death_after_deathbomb` ✅ | §4 |
| 道具收集 / 出屏 | `on_item_collect` / `on_item_frame` ✅（114 先例） | §4 |
| ECL 难度取值器 | `0x42aa20`（只被 ECL 派发器调）✅ | §2.3 |

**小 P 的 ×5 规则**（用户 2026-09-11 定）：装了 **110 或 111** 时，那两张卡把 P 点改成
**+0.05 火力 = 5×**，所以本卡的小 P rank 也 ×5（+0.002 → **+0.01**）。
判据 `ce_owned(110) || ce_owned(111)` —— 用的是 SDK 现成的**存在性查询**，**不算跨卡耦合**。

**每关开场补货**（用户 2026-09-11 定：算获得）：`CardNarumi`(36) 每关 +1 命碎、`CardPatchouli`(37) 每关 +1 符卡，
**不经过道具** ⇒ `on_item_collect` 抓不到，要在 `on_stage_start` 里按 `ce_owned(36)` / `ce_owned(37)` 显式补 **+0.7 / +0.02**。
