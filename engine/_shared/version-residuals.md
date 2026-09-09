# 跨版本残骸台账 —— ZUN 换版本时留下什么

> **版本**：跨版本。本文出现的地址一律带版本前缀（如 `th18:0x4ccd6c`）。
> **方法**：ExpHP `th-re-data` 逐版本结构体字段名对账 + 一手反汇编全量引用普查（`grep` 穷举字段偏移）。
> **可信度**：本文每条都标「活的 / 半死 / 死 / 不存在」，并给穷举证据。

## 0. 一句话

ZUN 换版本时**不删旧代码**：他把「喂数端」和「消费端」拆掉，把**字段、槽位、常量、音效**留着。
于是引擎里躺着一批「**槽还在、逻辑没了**」的死代码。这对逆向有两面：

1. **能白捡**：TH18 里 `zItemManager.lolk_item_slowdown_factor` 就是 TH15 的机制残骸——
   引擎照旧每帧读它、照旧每帧恢复，只是没人写 0.3；补上那一行就复活了（本 mod 的绀珠传卡）。
2. **容易踩**：死字段长得跟活字段一模一样。TH16 的 `GRAZE_IN_CHAPTER` 就被误当过活的
   （[`player/th16/01`](../player/th16/01-hit-life-system.md) §2c）。

**所以动手找接缝前先翻这份台账**；新发现的残骸也回填到这里。

## 1. 台账

| 机制（来源作） | 宿主 | 字段 / 地址 | 状态 | 证据 |
| --- | --- | --- | --- | --- |
| **DDC 每章道具生成计数**（收点批的输入） | th15 | `zGlobalsInner+0x90` `th15:0x4e7480`（ExpHP 直名 `__DDC_ITEM_SPAWN_COUNT`） | **半死**：TH15 会 `inc`（`th15:0x44097f`），但没有读者 | ExpHP statics + 一手引用普查 |
| 同上 | th18 | `zGlobalsInner+0x90` `th18:0x4ccd6c` | **死**：只有两处 `add`（`th18:0x42f033`、`th18:0x42f435`，后者带 `enemy+0x6824 == CURRENT_CHAPTER` 门），**全 exe 无读者**；被加的 `enemy+0x6828` **只被清零、从不递增**（5 处清零 / 2 处读） | 一手穷举 `enemy+0x6828` 与 `th18:0x4ccd6c` 全部引用 |
| **DDC 每章刷怪数** | th15 | `zGlobalsInner+0x94` `th15:0x4e7484`（`ENEMIES_SPAWNED_IN_CHAPTER`） | **活**：`th15:0x432f00` 累加、结算 `th15:0x43ab70` 读 | [chapter/th15/01](../chapter/th15/01-chapter-and-bonus.md) §1a |
| 同上 | th18 | `zGlobalsInner+0x94` `th18:0x4ccd70` | **死**：全 exe 只有一处 `mov …,0`（`th18:0x4427eb`，每 run 初始化） | 一手穷举 |
| **DDC 每章击破数** | th15 | `zGlobalsInner+0x98` `th15:0x4e7488` | **活**：`th15:0x433130` 累加、结算读它算击破率 | [chapter/th15/01](../chapter/th15/01-chapter-and-bonus.md) §2a |
| 同上 | th18 | `zGlobalsInner+0x98` `th18:0x4ccd74` | **不存在**：全 exe **零引用** | 一手穷举 |
| **DDC 敌人专用字段** | th14 | `zEnemy+0x11ec`（ExpHP `__only_used_in_ddc_and_isc`） | **活**：`inc` `th14:0x453852`、`dec` `th14:0x45352c`、读 `th14:0x45322d`（与 `0xa..0xe` 比） | 一手引用普查 |
| 同上 | th18 | `zEnemy+0x6838`（ExpHP `__only_used_in_ddc`） | **死**：全 exe 只有一处 `mov [esi+0x6838],0`（`th18:0x42e427`，初始化） | 一手穷举 |
| **DDC 玩家缩放** | th14 | `zPlayer+0x18308 player_scale` | **活**：多处读写 | 一手引用普查 |
| 同上 | th18 | `zPlayer+0x4797c`（ExpHP `player_scale__requires_flag_0x_10__from_ddc`） | **驱动死**：代码仍在（每帧被插值器写 `th18:0x45c753`；读点 `th18:0x45b485` / `th18:0x45cdeb` 拿它乘命中半径 3.6），但**闸门 `player+0x4779c` 的 bit 4 全 exe 无置位者**、另一个门 `+0x47974` 只被写 0 → 两个读点都不可达，值恒 1.0 | 一手穷举（写点只清 bit2/9、置 bit5/9） |
| **LoLK 道具减速因子** | th15 | `zItemManager.inner+0xe5dee0`（绝对 `th15:0x4e9a9c+0xe5def0`） | **活**：擦弹写 0.3，道具 tick 读它当位移乘数 | [player/th15/01](../player/th15/01-graze-and-item-slowdown.md) §4 |
| 同上 | th18 | `zItemManager+0xe6bb14`（ExpHP `lolk_item_slowdown_factor`） | **死**：引擎每帧 `+0.1` 恢复、物理读它，但**全 exe 无人写 0.3** | [player/th18/02](../player/th18/02-graze-and-item-slowdown.md) §2 |
| **LoLK 擦弹副作用**（PIV +100 / 浮字 / 减速） | th15 | `th15:0x4573a0` 全都有 | **活** | [player/th15/01](../player/th15/01-graze-and-item-slowdown.md) §1 |
| 同上 | th18 | `th18:0x45e160` 只剩「两个计数 + anm 脚本 0x18 + 音效 0x2a」 | **半死**（同号脚本 / 同号音效是同一套代码被阉割的旁证） | [player/th18/02](../player/th18/02-graze-and-item-slowdown.md) §1 |
| **DDC 收点档位表**（×0.5/0.8/1.2/1.6/2.0） | th14 | 表在 `th14:0x4c1a78` 等 | **不存在于 th18**：TH18 全 exe 没有 `1.2` 这个 float（IEEE 位模式 3F99999AH 零命中） | 一手常量普查 |
| **DDC 收点音** `se_pin00`（id `0x39`） | th14 | 收点结算播 `0x39` | **死**：TH18 全 exe `push 0x39` 只有 2 处、**都不在播音路径** | 一手穷举 |
| **DDC 收点线（POC）** | th14 | 回收线 + 批结算 | **活**（只剩线）：TH18 `zPlayer+0x47998 poc_height` 被零售卡 `ITEM_LINE`(22) 抬到 224（`th18:0x40c275`） | [card/th18/08](../card/th18/08-catalog.md) §「22 ITEM_LINE」 |
| **DDC 收点倍率弹字**（`BONUS %.1f` / `NO BONUS`） | th18 | `PopupManager__on_draw` `th18:0x464180`（串 `th18:0x4b71f8` / `th18:0x4b7204`） | **显示活 / 写入侧死**：绘制分支在，但 adder `th18:0x4645f0` 的签名是 `(pos*, int value, color)`、**没有倍率参数**，11 个原生调用点全传整数分数 → 倍率槽（`mgr+0x9cc` 起 5 个）**没有任何原生写者** | 一手（11 个调用点逐个核过） |
| **TH14 收点批记账块**（计时 8 / 批计数 / 批分） | th14 | `th14:0x4f5884` / `th14:0x4f5880` / `th14:0x4f5878` | **不存在于 th18**：`mov [glob],8` 全 exe 只有主菜单状态那 3 处；TH14 的分档比较链（`cmp …,0x3c→0x32→0x28→0x1e→0x14`）在 TH18 **0 命中**；道具路径无全局 `dec` | 一手反证三路（`th18:0x445A80` 反编译无计数器） |
| **TH12 UFO 计数** | th14 | `zItemManager+0xddd884 __disused__ufos_spawned_since_stage_begin` | TH14 里**就已经 disused** —— 残骸的残骸 | ExpHP 字段名 |
| **TH16 季节点道具**（item type 16 `ITEM_SEASON`） | th18 | item type 分派 `th18:0x445a80` 的 case 1..0x13 | **不存在**：没有 type 16 的 case，`type >= 0x14` 直接返回 0 | 一手反编译全 case；[season/th18/01](../season/th18/01-season-residuals.md) §1 |
| **TH16 季节子机数组** | th16 → th18 | `zPlayerInner.subseason_options`（player+0x3e0，4×0xe4）| **槽位被接管**：TH18 同一位置是 `equipment`(+0x410)；TH18 的子机只留一组（+0x50，4×0xf0）| th-re-data 三版本结构体对账；[season/th18/01](../season/th18/01-season-residuals.md) §2 |
| **TH16 季节解放（第二个 Bomb）** | th16 → th18 | `SUBSEASON_BOMB_PTR`(`th16:0x4a6da4`) + `zBomb.is_season(+0xd4)` | **不存在**：TH18 只有一个 `BOMB_PTR`(`th18:0x4cf2b8`)、4 个角色 vtable、zBomb 无 `is_season` | 一手 statics + 结构体；[season/th18/01](../season/th18/01-season-residuals.md) §3b |
| **TH16 季节专用道具字段**（只写不读的 `force_autocollect`） | th16 → th17 → th18 | th16 `zItem+0xc68` → th17 `+0xc70 __might_be_unused__force_autocollect__from_th16` → th18 推算 `+0xc8c` | th17 **留着且改名标注**；th18 **零引用**（同族 `+0xc90 appear_sound` 有 1 处读，位移推算成立）| 一手扫描 `[reg+0xc8c]` 全 exe 0 命中 |
| **TH17 动物灵管理器 / token 链表** | th17 → th18 | `GOAST_MANAGER_PTR`(`th17:0x4b7684`) + `zGoastManager` / `zToken` | **找不到**：TH18 funcs 729 / statics 154 里零符号；item type 分派里也没有动物灵 case | [animal/th18/01](../animal/th18/01-animal-residuals.md) §1 |
| **TH17 咆哮槽 / 计时 / 标志** | th17 → th18 | `zGlobalsInner+0x84 HYPER_FILL` / `+0x88 TOKENS[5]` / `+0xc8 HYPER_TIME` / `+0xe8 HYPER_FLAGS` | **无对应字段**：TH18 同偏移是 `+0x84` 活字段、`+0x88 MAX_BOMBS`；`+0xc8`/`+0xe8` 是死槽但与 TH17 不同血统（TH18 inner = TH15/TH16 布局）| 一手逐地址引用普查；[animal/th18/01](../animal/th18/01-animal-residuals.md) §2b |
| **TH18 玩家全局里的死槽** | th18 | `zGlobalsInner+0x90/+0x94/+0x98`、`+0xc8`(`th18:0x4ccda4`)、`+0xe8`(`th18:0x4ccdc4`)、`0x4ccda0/da8/dac/db0/db4/db8/dc0/dc8/dcc/dd0/dd4` | **死**：全部只在 `GameThread__thread_start` 里初始化，之后没有读者（`th18:0x4ccdbc`、`th18:0x4ccdd8` 例外，是活的）| 一手逐地址引用普查（读/写分类）|

## 2. 怎么读这张表

- **「死」= 全 exe 没有读者**（或没有写者）。这类字段**可以拿来用**（写进去就生效），
  因为引擎那边还在读——`lolk_item_slowdown_factor` 就是这么被本 mod 复活的。
- **「半死」= 读写都在，但效果被门控**。用之前先找到那个门（如 `player+0x4779c` bit4）。
- **「不存在」= 连槽都没了**。想复刻只能自建（DDC 的档位表就属于这一类）。
- **别把「有名字」当「活着」**：ExpHP 的名字本身就写着 `disused` / `only_used_in_ddc` / `lolk_`，
  那是他跨版本对照后的判断，值得先信、再自己验。

## 3. 相关一手文档

- 章节系统：[`chapter/th15/01`](../chapter/th15/01-chapter-and-bonus.md)（TH15 活的那半）、
  [`chapter/th18/01`](../chapter/th18/01-chapter-residuals.md)（TH18 只剩槽）
- 擦弹 / 道具减速：[`player/th15/01`](../player/th15/01-graze-and-item-slowdown.md)、
  [`player/th18/02`](../player/th18/02-graze-and-item-slowdown.md)
- 复刻它的卡：`mods/th18.v1.00a/card-expand/` 的辉针城（id 127，收点）、绀珠传（id 118，章节 + 减速）
- TH16 季节 / TH17 动物灵在 TH18 的残骸审计：[`season/th18/01`](../season/th18/01-season-residuals.md)、
  [`animal/th18/01`](../animal/th18/01-animal-residuals.md)（两个子系统都**没有可复活残骸**，只能自建）
