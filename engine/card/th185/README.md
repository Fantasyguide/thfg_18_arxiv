# th185（100th Black Market）的卡牌 / 货币系统 —— 侦察起点

> **版本**：跨版本（本文主写 TH18.5 = th185 v1.00a，兼与 TH18 对照）。跨版本文档：**每个地址都带版本前缀**
> （`th185:` / `th18:`）。

**背景**：用户 2026-09-18 把 TH18.5 的 Steam 安装目录指过来，提出三件事：
① 能不能把 th185 的**全部卡**加进本仓的 TH18 卡牌 mod（`mods/th18.v1.00a/card-expand/`）；
② **货币（弹货）系统**能不能一并复刻；③ 为承载这些卡，卡表要扩到 **id > 128**（用户实测 >128 有问题）。
本文只做**第一手清点**：先把这个游戏里"卡/货币"相关的系统、全局和结构大小钉死，作为后续深入的路标。

## 1. 样本与一手来源（🟢）

- 样本：`local/th185.v1.00a/th185.exe`，md5 `234b1e6eb1cc1b8b0c6e490487cf191c`
  （原始位置 `D:\SteamLibrary\steamapps\common\th185`，只复制了 exe；`th185.dat` 等留在原处）。
- 版本：`readme.txt` 标 2022/08/14；exe 内含字符串 `1.00a` ⇒ 目录名按 `th185.v1.00a` 登记。
- 命名来源：`local/vendor/th-re-data/data/th185.v1.00a/`（ExpHP 金矿）。
  **金矿在这个版本上明显变薄**：`funcs.json` 58 条、`statics.json` 41 条、
  `type-structs-own.json` 36 个类型、`type-structs-ext.json` 89 个类型，
  且**没有** `zTableCardData` / `zCardBaseClass` / `zVTableCard` / `zAbilityMenu` 这些
  th18 侧已有的卡系统类型名 ⇒ **th185 的卡层没有被社区命名**，要自己认。

## 2. 三个系统的锚点（符号名来自金矿，🟢）

| 系统 | 全局（静态指针） | 结构大小 | 入口 |
| --- | --- | --- | --- |
| 能力/卡管理器 | `zAbilityManager*` @ `th185:0x4d7ab8` | `0x117c` | `th185:0x408690`（operator new）、`on_tick` `th185:0x408a40`、`on_draw` `th185:0x408fc0` |
| 黑市（弹货商店） | `zBlackMarket*` @ `th185:0x4d7ac4` | `0x1254` | `initialize` `th185:0x41d430`、`on_tick` `th185:0x41e150`、`on_draw` `th185:0x41f4a0` |
| **永久卡商店**（货币消费端） | `zPermanentCardShop*` @ `th185:0x4d7ac8` | `0xe4c` | `initialize` `th185:0x41fb50`、`on_tick` `th185:0x420150`、`on_draw` `th185:0x421130` |

- ★ **结构大小对比（一手，来自两份金矿的结构表末项）**：th185 的 `zAbilityManager` 是 `0x117c`，
  th18 的同名结构是 `0xd70` ⇒ **差了 `0x40c`，两者不是同一份结构**。
  推论：卡系统同源但**已经改过**，任何"把 TH18 的 hook 直接搬过去"的想法都必须先按字段核对（🟡 推测）。
- 顺带登记：`th185:0x421580`（金矿名 `sub_421580_trophy_related`，成就/奖杯相关）；
  商店两张 UI 的 tick 就是上表的 `th185:0x41e150` 与 `th185:0x420150`。

## 3. 其它可用锚点（金矿直接给出，🟢）

- 全局：`GAME_THREAD_PTR` `th185:0x4d7b0c`、`STAGE_PTR` `th185:0x4d7ad8`、`GUI_PTR` `th185:0x4d7b08`、
  `CURRENT_STAGE_DATA` `th185:0x4d7c54`、`STAGE_DATA_TABLE`（`zTableStageData[0xa]`）`th185:0x4cba90`、
  `ANM_MANAGER_PTR` `th185:0x529e88`、`UPDATE_FUNC_REGISTRY` `th185:0x4d7ab4`。
- anm 一线（复刻"按脚本号起 VM"这种机制的必要入口）：
  `AnmManager::get_vm_with_id` `th185:0x48a4a0`、`AnmVm::run` `th185:0x47ac70`、
  `AnmManager::preload_anm` `th185:0x488360`、`AnmManager::on_tick_23` `th185:0x4899d0`。
- 别的子系统：`BulletManager::operator new` `th185:0x429ec0`、`Gui::on_tick` `th185:0x442650`、
  `StageInner::run_std` `th185:0x425e00`、`WinMain` `th185:0x473950`。
  （注意：th185 的 `statics.json` 里**没有** th18 那一批 `BULLET_MANAGER_PTR` / `ENEMY_MANAGER_PTR`
  之类的指针名，弹池/敌机池的全局地址要自己找。）

## 4. 三个待解问题（**下一轮直接从这三条切**）

1. **th185 有多少张能力卡？卡定义表在哪？**
   th18 侧是 `zTableCardData[]`，每项 `0x34` 字节、共 58 项（见
   [`../th18/07-registry.md`](../th18/07-registry.md)）。th185 侧对应结构**没有被命名**，
   要从 `AbilityManager` 的 tick/initialize 里反推表基址与项大小，**先拿到张数** —— 这个数直接决定
   "全部卡搬进来"的规模，也决定第 5 节那个问题的答案。
2. **货币字段在哪？**
   `zPermanentCardShop`（`0xe4c`）与 `zBlackMarket`（`0x1254`）的成员表在金矿里是**空白的**，
   只能反汇编认：钱是哪个偏移、什么类型（int32？）、扣款/加款的唯一入口是哪个函数、
   **跨局持久化**写进哪个存档结构（这是"复刻货币"的真正难点，不是画个 UI 就完）。
3. **卡 id > 128 的坏点**（与 TH18 mod 共通的题目）：
   若 th185 自己的卡数 **>127 且引擎跑得动**，那就**直接证明限制出在 mod 侧而不是引擎侧**；
   反之若 th185 的卡 id 也被压进 ≤7 位，就要找到那个位域的确切位置。**这一条是第 1 条的直接副产品**，
   所以顺序是：先数卡 → 再看 id 位宽。

## 5. 与 TH18 卡牌 mod 的关系（先记判断，别当结论）

- 同源是**事实**（同名 `AbilityManager`、同名 `zAnmVm`/`zGui`/`zUpdateFuncRegistry` 一族）。
- 但 `zAbilityManager` 大小不同 ⇒ **卡基类/虚表槽位必须逐字段核对**后才能谈移植（🟡）。
- "把卡搬过来"的实际工作量取决于**卡的效果是数据驱动还是代码驱动**：th18 侧已经证明是
  「`zTableCardData` 静态表 + 每卡一个 C++ 子类（21 槽虚表）」（见 [`../th18/01-object-model.md`](../th18/01-object-model.md)），
  若 th185 同构，则"加卡"= **搬数据 + 搬行为代码**，不是改几个数字。这条在下一轮用 th185 的
  `AbilityManager::on_tick`（`th185:0x408a40`）确认。
