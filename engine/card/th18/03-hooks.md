# TH18 卡牌系统 — 21 个虚表钩子 × 引擎调用点

> **版本**：TH18 v1.00a（`th18.exe`）。本文裸地址默认属该版本；引用其他版本须写成 `th16:0x…`。
> **方法**：槽名取自 ExpHP 的 `zVTableCard`；**调用点全部一手定位**（按 `CALL dword ptr [reg+disp]`
> 的字节模式全库扫描 + 逐个反编译确认宿主函数）。
> **可信度**：21 槽全部落地 ✅；两处 ExpHP 误名已订正并在本仓 Ghidra 库落注释。
> **前置**：[`01-object-model.md`](01-object-model.md)。

## 0. 一句话结论

**21 个槽全部有确定的引擎调用点**。它们分布在 6 个宿主里：玩家主 tick、能力管理器 tick/draw/HUD、
玩家开火、自机弹生成、敌人掉落、以及分配/回收本身。想改卡牌行为，改的就是这张表里的某一格。

## 1. 全表（槽 → 名 → 调用点 → 语义）

| 槽 | ExpHP 名 | 引擎调用点（地址） | 语义（一手） |
| --- | --- | --- | --- |
| +0x00 | `constructor` | `allocate_new_card` `0x412D11` | 仅 `mode & 1 == 0` 时调；返回非 0 → 当场删卡 |
| +0x04 | `destructor` | `allocate_new_card` `0x412D35` | 仅 `mode & 2` 时调；返回非 0 → 当场删卡 |
| +0x08 | `c_press` | `Player__on_tick__body` `0x45C048` | **C 键释放选中的主动卡** |
| +0x0c | `on_player_death_after_deathbomb` | 同上 case 4 `0x45C2xx` | 决死窗口耗尽；返回非 0 = 我救下了 |
| +0x10 | `on_player_before_deathbomb` | **`Player__die` `0x45D574`** | 中弹瞬间，窗口长度已置 8 帧后广播 |
| +0x14 | `..._frame_2` | `Player__on_tick__body` case 2 frame 3 | 死亡结算**之后**（扣钱扣火力已发生）|
| +0x18 | `on_power_level_change` | `Player__repopulate_options_and_notify_cards` `0x45DCFB` | 火力档变化；装备卡在此生成子机 |
| +0x1c | `on_tick_shooters` | `Player__tick_shooting_state` `0x45ECB8` | 每射击帧；装备卡子机开火 |
| +0x20 | `on_load` | `AbilityManager__notify_cards_on_load` `0x408AE5`；replay 复原 `0x4179D7` | OR 累加返回值 |
| +0x24 | `on_tick` | `Player__on_tick__body` case 1 | 玩家侧每帧钩子 |
| +0x28 | `on_bullet_created` | `PlayerBullet__create` `0x45E7F5` | 每生成一颗自机弹 |
| +0x2c | `__on_tick_2` | `AbilityManager__on_tick` `0x408640` | 管理器侧每帧钩子（主力）|
| +0x30 | `recharge` ⚠️ | **`Enemy__drop_items_and_notify_cards` `0x4306E9`** | **误名**：实为「敌人掉落道具后」，见 §3 |
| +0x34 | `__on_load__2` | **`GameThread__thread_start` `0x442F98`** | **每关开场**广播；「每过一关 +X」类卡在此 |
| +0x38 | `method_38` | replay 复原 `0x4179xx` | = `set_recharge_timer(n)`，见 §2 |
| +0x3c | `get_bomb_timer` ⚠️ | **`AbilityManager__dump_recharge_timers` `0x408BB9`** | **误名**：实为 `get_recharge_remaining()`，见 §2 |
| +0x40 | `method_40` | `AbilityManager__draw_active_card_hud_entry` `0x4088E8` | = `is_firing()`（`flags & 0x20`）|
| +0x44 | `on_anm_id_assigned_to_hud` | `AbilityManager__create_card_list_for_hud` `0x408FC4` | HUD 图标建好后把 anm id 交给卡 |
| +0x48 | `on_draw` | `AbilityManager__on_draw` `0x408ABC` | 每帧绘制（`CardMomoyo` 画倍率）|
| +0x4c | `method_4C` | `recount_and_recategorize_cards` `0x4080E0` | 局末重置卡内状态 |
| +0x50 | `operator_delete` | `reset_cards` / `recount` / `allocate` | 销毁 |

> 基类默认实现在 `0x413010`–`0x413173`，绝大多数是 `ret`。
> `AbilityManager__notify_cards_on_load` / `__dump_recharge_timers` / `__draw_active_card_hud_entry`
> 是本仓给 `FUN_00408ad0` / `FUN_00408ba0` / `FUN_00408890` 的命名，已回落 Ghidra 库。

## 2. ★ 订正一：+0x3c 不是 `get_bomb_timer`，是「充能剩余」

- **① 发现**：基类实现 `0x413130` 只有一句 `return *(int*)(this + 0x38)`——
  即 +0x34 那个 `zTimer` 的 `current`。
- **② 推测**：ExpHP 叫它 `get_bomb_timer`，暗示是炸弹时长；但 +0x34 的语义本身就有争议
  （[`01-object-model.md`](01-object-model.md) §3）。
- **③ 验证（两条独立证据）**：
  1. **replay 序列化**：`AbilityManager__dump_recharge_timers` `0x408BA0` 遍历全卡把 `vtable+0x3c`
     的返回值写成数组（−1 终止），`AbilityShop__sub_417880` 把它存进 replay `+0xe64`；
     回放时用 `vtable+0x38`（`set_recharge_timer`）灌回去。**replay 需要复现的是充能剩余，不是炸弹时长。**
  2. **HUD 充能条**：`AbilityManager__draw_active_card_hud_entry` `0x408890` 算
     `fill = 1.0 - card[0xf] / card[0x12]` = `1.0 − (card+0x3c) / (card+0x48)`
     = **1 − 剩余 / 总充能**。分母正是 `recharge_time`。
- **④ 结论**：**+0x3c = `get_recharge_remaining()`，+0x34 那组计时器 = 充能倒计时** ✅（TH18 v1.00a）。
- **⑤ 证据**：`0x413130`、`0x408BA0`、`0x408890`、`0x4088E8`、`0x417880`。

## 3. ★ 订正二：+0x30 不是 `recharge`，是「敌人掉落道具后」

- **① 发现**：唯一调用点 `0x4306E9` 在 `Enemy__drop_items_and_notify_cards` `0x430510`
  （本仓命名，原 `FUN_00430510`）：该函数按 `enemy+0x04 + (type−1)*4` 的每类掉落数
  逐个 `ItemManager__spawn_item`（type 1..0x13），**然后**：

  ```c
  for (card in list) card->vtable[0x30](drop_pos, &enemy->drop_counts);
  memset(&enemy->drop_counts, 0, 0x50);
  ```
- **② 推测**：签名是 `(pos, drop_count_table)`，不是「充能」。
- **③ 验证**：唯一覆盖它的卡是 `CardYachie__recharge` `0x40D630`（id 34 MONEY）：
  它取 `drop_counts[type2] / 3`，在 `pos` 附近散射**同样数量的 type 2（金钱）道具**。
  与 THBWiki「让敌人掉落额外的金钱」逐字吻合，且**给出了确切倍率：额外 1/3**。
- **④ 结论**：**+0x30 = `on_enemy_dropped_items(pos, drop_counts)`；Yachie 的效果 = 金钱道具 ×(1 + 1/3)** ✅。
- **⑥ 补（2026-09-07，一手）**：`0x430510` 的 `ecx` 是 `zEnemyDrop`（`enemy+0x623c`，`0x430499`），不是敌人；表是它的 `+0x04 ex_drop[20]`（ECL `dropExtra`），
  循环只撒 type 1..0x13。`+0x00 main_drop`（ECL `dropMain` / `enmCreate` 第 6 参，杂兵的那一颗）由调用方在**调它之前**单独 `spawn_item`（`0x430469`、`0x42d0c6`），
  不在这张表里、也不进 `+0x30` 的广播。要改杂兵掉落得在 `0x430510` 入口前后同时看两处（`mods/th18.v1.00a/card-expand` AUDIT S10）。
- **⑤ 证据**：`0x430510`、`0x4306E9`、`0x40D630`；item type 2 的收集处理见
  `ItemManager__collect_money_item` `0x446B00`（[`05-shop-and-money.md`](05-shop-and-money.md) §6）。

## 4. 死亡相关的三个槽，顺序很重要

一次中弹会依次触发三个不同的槽，**它们看到的世界不一样**：

```
中弹  → Player__die 0x45D090 前段：
          PLAYER+0x47908 (num_deathbomb_frames) = 8          ← 默认决死窗口
          for card: vtable[0x10]  on_player_before_deathbomb  ← CardTewi 改成 15
        玩家状态 → 4（决死窗口）

窗口内 → Player__on_tick__body case 4：input & 0x2 且可炸 → do_bomb() + cancel_impending_death()

窗口满 → case 4：acc = 0; for card: acc |= vtable[0x0c](acc)   ← 救命卡；传的是累加器
          acc != 0 → 死亡被取消（只有第一张返回非 0 的卡生效）
          acc == 0 → Player__commit_death_and_enter_state2 0x45D090（扣命、重置炸弹）→ 状态 2

状态 2 → 第 3 帧：spend_power(POWER_PER_LEVEL) → 撒 7 个道具
          → MONEY -= min(MONEY/3, 100)                        ← 金钱惩罚
          → for card: vtable[0x14]  ..._frame_2               ← 补偿类卡在此回填
          → repopulate_options_and_notify_cards
```

**救命卡的让位逻辑**（一手）：`CardEirin__on_player_death_after_deathbomb` `0x40A4F0`
收到 `acc == 0` 时先扫链表找 card_id `0x23`（35 ROKUMON）；若在场**且** `MONEY > 199`，
它 `return 0` **主动让位**给六文钱（免得白费两张符卡）；否则才走
`Card__death_save_bomb_revive` `0x40A2A0`（扣 **2** 张炸弹）。
`CardShikiEiki__on_player_death_after_deathbomb` `0x40DA10` 则要求 `MONEY > 199`，
走 `Card__death_save_money_revive` `0x40D840`（**扣 200 金**）。

**补偿类卡都挂在 +0x14**，因此它们看到的是「惩罚已经发生」之后的状态：

| 卡 | 实现 | 做什么 |
| --- | --- | --- |
| `CardTewi`(24) | `0x40A730` | 把 `vtable+0x0c` 时快照进 `card+0x54` 的 `MONEY` 写回 → **金钱惩罚被抵消** |
| `CardMamizou`(32) | `0x40D410` | 把快照的 `CURRENT_POWER` 写回，封顶 `POWER_PER_LEVEL * 3` |
| `CardKaguya`(31) | `0x40D2E0` | `spawn_item(type 7, 玩家位, 角度 −π/2, 速度 3.2, 3)` = 掉符卡道具 |

> `CardTewi` 的快照发生在 `vtable+0x0c`（`0x40A720`：`card+0x54 = MONEY`），
> 而它在那里 `return 0` —— **不救命，只记账**。这就是「延长决死窗口 + 死亡保住金钱」的实现。

## 5. 装备卡的两个槽

- **+0x18 `on_power_level_change`**：`Player__repopulate_options_and_notify_cards` `0x45D5E0` 尾部广播。
  装备卡在此 `Player__allocate_option(...)` 生成子机，指针存 `card+0x54`（位置偏移逐卡不同）。
- **+0x1c `on_tick_shooters`**：`Player__tick_shooting_state` `0x45EA00` 每射击帧对**每张卡**调，
  传 `(short_timer = player+0x476d4, long_timer = player+0x476e8)`；装备卡转发给
  `Player__tick_shooters_for_ability_card` `0x40A9C0`，后者按**逐卡烘死的 shooter 索引**取表：

  ```c
  shooter_table = *(char**)( *(int*)(PLAYER_PTR + 0x47940) + 0xe0 + index*4 );
  ```

  逐卡索引与子机偏移见 [`08-catalog.md`](08-catalog.md) §D。**「这些 shooter 表存在哪」已经解决**
  （2026-09-05）：就在 `pl0X.sht` 里，`+0xe0` 那张 40 项偏移数组的第 `0x0a`–`0x16` 项，四个角色的文件同构 ——
  见 [`../../sht/th18/01-file-layout-and-shooterset-index.md`](../../sht/th18/01-file-layout-and-shooterset-index.md)。

三条对写卡的人有用的推论（一手，出处见 [`../../sht/th18/02-shooter-record.md`](../../sht/th18/02-shooter-record.md)）：

- **子机的贴图与子弹的贴图都取自 `ability.anm`**：`allocate_option` 的最后一个参数是 `ability.anm`
  的脚本号（Reimu 用 2、Alice 用 `0x10`）；`0x40A9C0` 开火期间把 `player+0x10` 换成
  `AbilityManager->ability_anm`，于是 shooter 的 `+0x22` 也按 `ability.anm` 解释，打完还原。
- **瞄准**：把角度写进 `player+0x479cc`，并让该组 shooter 的 `func_on_init = 5`（`0x4612d0` 会用它
  覆写 `bullet+0x64`）。这是 `CardAlice` 的做法，零售天天在跑。
- **`+0x1c` 只在按住射击键时广播**（`0x45EA00` 里被 `short_timer != prev` 那层门罩着）；
  想要不按射击键也定时开火，得自己在别的槽里调 `0x40A9C0` 并自带计时器。
