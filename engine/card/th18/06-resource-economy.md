# TH18 残机 / 符卡 资源经济 —— 库存、炸弹与资源卡

> **版本**：TH18 v1.00a（`th18.exe`）。本文裸地址默认属该版本；引用其他版本须写成 `th16:0x…`。
> **方法**：一手反编译资源卡的 ctor/dtor、`ItemManager` 的道具分派、炸弹子系统与死亡结算。
> **可信度**：库存三元组与卡侧写入 ✅；`LIVES_STOCK` 与 `CURRENT_LIVES` 的确切分工 🟡。
> **前置**：[`02-lifecycle.md`](02-lifecycle.md)（即时卡机制）。

## 0. 一句话结论

TH18 的**炸弹（X 键）与卡牌是两套并行系统**，卡牌只取代了 TH16 的季节释放（C 键）。
炸弹本体照旧消费全局库存，而**库存被资源卡喂养**：`CardBomb` 抬上限、`CardLife` 加命、
碎片卡进位。资源卡都是「即时卡」——在 ctor 或 dtor 里改完全局就自毁，从不进卡组。

## 1. 炸弹（X 键）子系统与卡牌并行

> **订正记录（保留）**：早期 port-plan 据「ExpHP 没命名 `zBomb` 结构体」推断 TH18 无炸弹，
> **一手证伪**。教训：**「ExpHP 没命名某结构」≠「该机制不存在」**。

- `Bomb__operator_new` `0x41FD40` 按角色选 vtable（`BombReimuAInf` / `BombMarisaAInf` /
  `BombSakuyaInf` / `BombSanaeAInf`）；`zVTableBomb` 7 槽 28 字节
  （`operator_delete` / `begin` / `on_tick` / `on_draw` / …）。
- **`do_bomb` `0x420360`**：由 `Player__on_tick__body` 在 `INPUT_PRESSED & 0x2` 时调。
  门控 `BOMB[0xc] == 0`（未在炸）→ 置进行中、重置计时（`BOMB+0xd/0x10/0x11`）、
  spell 中炸置失败标记（`BOMB+0x1a`）、放音 `0x2c`、调 `BOMB->vtable+0x4`（角色专属 begin）、
  清 `ENEMY_MANAGER+0x44`。与 `th16:` 主炸同形。
- **`Bomb__can_bomb_and_deathbomb_check` `0x420420`**：`CURRENT_BOMBS > 0` + `BOMB+0x30 != 1`
  + 无对话 + 有敌人。普通炸（case 1）与决死炸（case 4）都走它。

## 2. 符卡库存三元组（✅）

HUD 更新函数 `Gui__sub_4420e0(GUI, CURRENT_BOMBS, BOMB_FRAGMENTS, MAX_BOMBS)` 三参，坐实三元组：

| 全局 | 地址 | 语义 | 证据 |
| --- | --- | --- | --- |
| `CURRENT_BOMBS` | `0x4ccd58` | 当前可用符卡 | `0x420420` 门控 `>0`；处处钳到 `MAX_BOMBS` |
| `BOMB_FRAGMENTS` | `0x4ccd5c` | 符卡碎片 | 道具 type 6 收集时 `+1`，**`> 2` 时归零并进位** |
| `MAX_BOMBS` | `0x4ccd64` | 符卡上限 | `CardBomb__destructor` `0x409C20` `+1`（封顶 7）|

- **碎片进位**：`ItemManager__on_tick__body` `0x445A80` case 6 —— 仅当 `CURRENT_BOMBS < MAX_BOMBS`
  才累加碎片；`BOMB_FRAGMENTS > 2` → 归零 + `FUN_00457690(0x4cccdc)`（进位到整张符卡）。
  → **3 个碎片 = 1 张符卡** ✅。已满则碎片直接清零。
- **消费**：`do_bomb`（X）；`Card__death_save_bomb_revive` `0x40A2A0` 连扣**两次**
  （每次钳 `[0, MAX_BOMBS]`，中间各刷一次 HUD）→ 即「消费 2 张符卡，只剩 1 张时扣 1 张」，
  与 THBWiki 对 `CardEirin`（id 23 AUTOBOMB）的描述逐字一致。
- **补货**：`CardPatchouli____on_load__2` `0x409F40`（**每关开场**，见 [`03-hooks.md`](03-hooks.md) §1）
  `CURRENT_BOMBS += 1`，钳 `MAX_BOMBS`，未溢出则放音 `0x2e`；道具 type 7 同形。
- **死亡重置**：`Player__commit_death_and_enter_state2` `0x45D090` 把 `CURRENT_BOMBS` 置
  `DAT_004ccd60`（每命初始符卡数）并钳到 `MAX_BOMBS`。

## 3. 残机库存（🟡 分工待定）

| 全局 | 地址 | 写入方 |
| --- | --- | --- |
| `LIVES_STOCK_cardfed_cap7` | `0x4ccd54` | `CardLife__destructor` `0x409B80`、`CardLifeFragment__constructor` `0x409CC0`、`CardMokou__destructor` `0x409DF0`（均 `+1`，封顶 7）|
| `CURRENT_LIVES` | — | `Player__commit_death_and_enter_state2` `0x45D090` `-1`；`< 0` → Game Over |
| `LIFE_FRAGMENTS` | `0x4ccd4c` | 道具侧 |

HUD 三参函数是 `FUN_00441f10(GUI, CURRENT_LIVES, LIFE_FRAGMENTS, LIVES_STOCK)` ——
形状与符卡那组一致，**但 `LIVES_STOCK` 出现在第 4 参（= `MAX_BOMBS` 的位置）**，
所以它更像「残机上限」。`ItemManager` case 5（残机道具）也把 `CURRENT_LIVES` 钳到 `LIVES_STOCK`，
并在到顶时清 `LIFE_FRAGMENTS` —— 支持「上限」解读。

- 🟡 **仍未闭合**：`CardLife`（整命）与 `CardLifeFragment`（碎片）都对**同一个** `LIVES_STOCK` `+1`。
  若它真是上限，那「整命卡」加的也只是上限，实际残机在别处补。待逐条反 `CURRENT_LIVES` 的写入点。

## 4. 资源卡 → 库存映射（一手）

全部走「即时卡」路径（[`02-lifecycle.md`](02-lifecycle.md) §3）：门控 `flags & 2`，
施加后 `flags &= ~2` 并 `return 1` 让分配尾部当场销毁。

| card_id | 卡 | 钩子 | 效果 |
| --- | --- | --- | --- |
| 1 | `CardLife` | `destructor` `0x409B80` | `LIVES_STOCK += 1`（封顶 7）+ `FUN_004575f0` |
| 2 | `CardBomb` | `destructor` `0x409C20` | `MAX_BOMBS += 1`（封顶 7）|
| 3 | `CardLifeFragment` | `constructor` `0x409CC0` | `LIVES_STOCK += 1`（封顶 7）+ `FUN_00457570` |
| 4 | `CardBombFragment` | `constructor` `0x409D60` | `FUN_004576e0`（碎片进位）|
| 5 | `CardNazrin` | `constructor` `0x40A020` | `MONEY += 50`、`MONEY_TOTAL_COLLECTED += 50` |
| 6 | `CardRingo` | `constructor` `0x40A0B0` | 火力 `+0.50`，跨档则重建 option |
| 7 | `CardMokou` | `destructor` `0x409DF0` | 写 `LIVES_STOCK`（+3 命，见 [`08-catalog.md`](08-catalog.md)）|
| 36 | `CardNarumi` | `__on_load__2` `0x409E90` | **每关开场** +1 命碎片 |
| 37 | `CardPatchouli` | `__on_load__2` `0x409F40` | **每关开场** +1 符卡 |

**ctor 型 vs dtor 型的差别是有后果的**：道具掉落（mode 0）**只调 ctor**，
所以能靠道具直接生效的只有 ctor 型（id 3/4/5/6）——而 `ITEM_TYPE_TABLE_sprite_and_card` `0x4b4020`
给出的卡道具正好就是这四张（[`05-shop-and-money.md`](05-shop-and-money.md) §8）。自洽。

> 注意 id 36/37 挂的是 `__on_load__2`（+0x34，每关开场）而**不是** ctor/dtor，
> 所以它们**留在卡组里**，每过一关触发一次——这正是社区说的「每过一关 +1」。

## 5. Follow-up

- 🟡 `LIVES_STOCK` vs `CURRENT_LIVES` 的分工（§3）。
- ✅ **2026-09-10 一手闭合**：`0x457570`（= 道具 `type 4` 与 `CardLifeFragment` 共用）= **命碎累加**——
  `[0x4cccdc+0x70]`(`LIFE_FRAGMENTS` `0x4ccd4c`) `+1`，按 `[+0x74]` 取下标查难度表
  （`difficulty(0x4ccd00) == 4` → `0x4b3ff4`，否则 `0x4b40c8`），满则 `sub 阈值` + `FUN_004575f0`（进位加命）。
  ⇒ **命碎阈值随难度表**（实测全 3），而 **B 碎阈值硬编码 3**（`0x44648f` `cmp eax,3`），两者不对称。
- ✅ `0x457690` = **符卡进位**（读 `CURRENT_BOMBS` / `BOMB_FRAGMENTS` / `MAX_BOMBS`，放音 `0x2e`，刷 HUD `0x4420e0`）。
- ⏳ `FUN_004575f0` / `FUN_004576e0` 本体仍未逐反（前者 = 命碎片进位落点）。
- 📌 道具 type 全表（分派器 `0x44625f` → byte 表 `0x44685c` → jump 表 `0x446830`，含 `0x10–0x13` 卡道具）
  见 [`2026-09-10-th06-playrank-design.md` §3.1](../../../docs/superpowers/specs/2026-09-10-th06-playrank-design.md)。
- ⏳ `DAT_004ccd60`（每命初始符卡数）随难度/角色的取值。
