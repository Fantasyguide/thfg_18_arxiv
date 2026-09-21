# card — 卡牌 / 能力系统（TH18 及其直接后继 TH18.5）

> **版本**：跨版本。本文出现的地址一律带版本前缀（如 `th16:0x442560`）。

TH18《虹龍洞》用**卡牌/能力系统**取代了 TH16 的季节系统。它不是一个小功能，而是一整条
横跨 玩法 / 商店 / HUD / 存档 / replay 的纵向系统——这也是为什么「加一张新卡」不能只往
静态注册表后面追加一项。

放在 `engine/card/` 而不是 `th18/` 目录下，是刻意的：**特有机制也是子系统**。
将来 TH19 若有类似机制，矩阵加一列即可，不必再开版本目录。

## 断言 × 版本矩阵

| 断言 | th16 | th18 | 证据 |
| --- | :---: | :---: | --- |
| 存在卡牌/能力系统 | ❌ | ✅ | [th18/01 §0](th18/01-object-model.md) |
| `AbilityManager` 统一负责分配 / 选中 / 每帧 tick | ❌ | ✅ | [th18/01 §5](th18/01-object-model.md) |
| 卡是 0x54 字节基类 + **21 槽虚表**，槽全部有确定引擎调用点 | ❌ | ✅ | [th18/03](th18/03-hooks.md) |
| 分配只有一个入口，`mode` 决定调 ctor/dtor 与 `flags` bit0 | ❌ | ✅ | [th18/02 §2](th18/02-lifecycle.md) |
| **即时卡**靠 ctor/dtor 返回非 0 当场自毁，从不入链表 | ❌ | ✅ | [th18/02 §3](th18/02-lifecycle.md) |
| 主动卡按 C 键（输入位 `0x400` 上升沿）触发，带充能倒计时 | ❌ | ✅ | [th18/04 §1,§2](th18/04-active-cards.md) |
| 卡的两个 `zTimer`：+0x34 是充能、+0x20 是激活经过帧（ExpHP 名反了）| ❌ | ✅ | [th18/01 §3](th18/01-object-model.md)、[th18/03 §2](th18/03-hooks.md) |
| 卡牌分类写在 `card->flags(+0x50)` 的位义里（引擎只分三类）| ❌ | ✅ | [th18/01 §4](th18/01-object-model.md) |
| 装备卡带**每卡一个 shooter 索引**，经 `Player__allocate_option` 挂子机 | ❌ | ✅ | [th18/03 §5](th18/03-hooks.md)、[th18/08 D](th18/08-catalog.md) |
| ↑ 但这些 shooter 数据**存在哪里**仍未定 | — | 🟡 | [th18/OPEN §1](th18/OPEN-questions.md) |
| X 键炸弹系统与卡牌**并行存在**（不是被取代） | ✅ | ✅ | th16：[player/OVERVIEW](../player/OVERVIEW.md)；th18：[06 §1](th18/06-resource-economy.md) |
| 决死窗口默认 8 帧，救命卡在窗口尽头按 OR 累加抢答 | ✅ | ✅ | th18：[03 §4](th18/03-hooks.md) |
| 58 项**静态**注册表 `zTableCardData[]`（可获得的是前 56 项）| ❌ | ✅ | [th18/07](th18/07-registry.md) |
| 商店 offer = 3 档随机 + 两个保证循环，卡也可由道具直接掉落 | ❌ | ✅ | [th18/05 §4,§8](th18/05-shop-and-money.md) |
| 出现规则由 `CardData__is_available_at_stage` `th18:0x416E10` 判定，返回 0/1/**2** | ❌ | ✅ | [th18/05 §5](th18/05-shop-and-money.md) |
| **`MONEY` 既是购卡货币，也是计分乘数** | ❌ | ✅ | [th18/05 §2](th18/05-shop-and-money.md) |
| 用火力补差价会把剩余金钱一并清零；至少保留 1.00 火力 | ❌ | ✅ | [th18/05 §6](th18/05-shop-and-money.md) |
| 死亡罚款 `min(MONEY/3, 100)`，`CardTewi` 可完全抵消 | ❌ | ✅ | [th18/05 §9](th18/05-shop-and-money.md) |
| 空白卡（id 0）的效果实现在 `AbilityShop` 而非卡类里 | ❌ | ✅ | [th18/05 §7](th18/05-shop-and-money.md) |
| 存档与 replay 保存 card-id 字节数组（replay 另存每卡充能剩余）| ❌ | ✅ | [th18/02 §6,§7](th18/02-lifecycle.md) |

> **图例**：✅ 该版本一手验过（证据列给地址/出处） · 🟡 待验（从别的版本借来的假设，或单源） · ❌ 已知不同/不存在 · ❓ 存疑 · — 未看
>
> **本页不许出现没有出处的断言。** 从某作借到另一作的判断一律 🟡，在该版本 exe 上验过才能改 ✅（[`METHOD.md`](../../METHOD.md)）。

### th185（TH18.5《100th Black Market》）—— 同源，但**改过**

用户 2026-09-18 把该作（`D:\SteamLibrary\steamapps\common\th185`）指过来，问「它的全部卡 + 货币能不能搬进来」。
第一手清点（清单与地址见 [`th185/README.md`](th185/README.md)）：

- **同源是事实**：`AbilityManager`（`on_tick` `th185:0x408a40`）、`zAnmVm` / `zGui` / `zUpdateFuncRegistry`
  一族类型名都在，且都带同一套 `on_tick__stub` 调度约定。
- **但结构改过**：th185 的 `zAbilityManager` 是 `0x117c`，th18 同名结构是 `0xd70`（差 `0x40c`）
  ⇒ **hook 与字段必须先按 th185 重核，不能直接搬**（🟡）。
- **商店/货币在 th185 里是独立系统**：`zBlackMarket`（`0x1254`，`th185:0x41d430`/`th185:0x41e150`/`th185:0x41f4a0`）
  与 `zPermanentCardShop`（`0xe4c`，`th185:0x41fb50`/`th185:0x420150`/`th185:0x421130`）——后者的名字直接说明
  th185 有**跨局持久的"买断"卡**，这正是"货币"这一问的核心。
- **金矿在这个版本变薄**：`zTableCardData` / `zCardBaseClass` / `zVTableCard` / `zAbilityMenu` **均无命名**
  ⇒ 卡张数、卡 id 位宽、货币字段都要自己反汇编认（三条待解问题见 [`th185/README.md §4`](th185/README.md)）。
- **本页矩阵暂不加 th185 列**：等第 4 节那三条查清（卡数 / 货币字段 / id 位宽）后一次性补列，避免**未验证的 ✅**。


## ★ 加新卡的关键判断

**新增卡不能只往 `zTableCardData[]` 后面写一项。** 原生表是静态的，而
`allocate_new_card` 的 **id→类跳转表只有 57 项**（`th18:0x412dac`，id 0–56）、
商店的表遍历硬编码 `id < 56`、HUD/解锁位/存档卡组各有自己的容量假设。
要做完整的新卡，这些必须以同一套新 ID/元数据模型一起设计和验证——
路线见 [`../../mods/th18.v1.00a/card-rework/ROADMAP.md`](../../mods/th18.v1.00a/card-rework/ROADMAP.md)，
接缝清单见 [th18/04 §8](th18/04-active-cards.md) 与 [th18/03](th18/03-hooks.md)。

**★ 未结：id > 128 会坏（用户实测）。** 承 TH18.5 的卡要占新号段，所以这条必须先查清"是**哪一层**在
127→128 处断"：卡 id 的**位宽/掩码**、存档与 replay 的 card-id **字节数组**、解锁位图、id→类跳转表
（`th18:0x412dac`，57 项）各自的容量假设，都可能是断点。定位中，结论与扩展方案落地后写进
[th18/10-extensibility-limits](th18/10-extensibility-limits.md)，侦察线索见 [`th185/README.md §4`](th185/README.md)。

## 系统卡的**原作侧**一手（106–117 要复刻的机制）

mod 里 id 106–117 那 12 张「系统卡」各自复刻一作的原版机制；它们的**原作侧语义**不属本页
（本页只管 TH18 自己的卡牌系统），按版本登记在各自目录。已建的一篇：

- **TH09 花映塚（109 `POFV_BLAST`）**：
  [`th09/01-pofv-charge-and-cancel-fields.md`](th09/01-pofv-charge-and-cancel-fields.md) ——
  消弹圈 = **锚定场**（`(圆心, r₀, dr, 寿命, kind)`，每帧 `r += dr`、寿命 `--`；建 `th09:0x41cf30` 族 /
  推进 `th09:0x41c8e0` / 消费 `th09:0x41dff0` ⇒ 弹 state 5）；拦 Z 的闸 = **按住满 10 帧**（`th09:0x41fa1b`）；
  气槽 = 4×100、上限 400，蓄力量被它钳上界；CA 三级半径 = **192 / 256 / 448**。

## 社区交叉验证

[th18/09](th18/09-community-crosscheck.md) 记录了与 THBWiki 的逐项对账：高度一致，
社区解开了我们几处 🟡；**分歧处社区全对，代码侧已订正**。原始 wikitext 存在
`engine/card/th18/_sources/thbwiki-cards.txt`，**是素材不是结论**。

开放问题集中在 [th18/OPEN-questions.md](th18/OPEN-questions.md)。
