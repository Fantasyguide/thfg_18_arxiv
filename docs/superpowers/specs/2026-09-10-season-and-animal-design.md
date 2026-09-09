# 季节（TH16）/ 动物灵（TH17）在 TH18 的复刻设计 —— 先占位

> **版本**：跨版本。本文地址一律带版本前缀（如 `th18:0x445a80`），默认目标版本 = TH18 v1.00a。
>
> **背景**：用户 2026-09-10 点名「查 th16 季节系统 / th17 动物灵系统在 th18 有没有代码残留」，
> 并要求「简单的部分先做一个占位」。
> **一手结论**（两份审计）：[`engine/season/th18/01`](../../../engine/season/th18/01-season-residuals.md)、
> [`engine/animal/th18/01`](../../../engine/animal/th18/01-animal-residuals.md)。

## 0. 结论一句话

**两个系统在 TH18 都没有可复活的残骸**（不是「半死」，是「不存在」）：

| 检查项（用户点名） | TH18 现状 | 证据 |
| --- | --- | --- |
| TH16 掉落季节点 | ❌ 没有 item type 16 的 handler | `th18:0x445a80` 的 type 分派全 case |
| TH16 季节子机 | ❌ `subseason_options` 的槽位被 `equipment` 接管 | 三版本 `zPlayerInner` 对账 |
| TH16 季节子机升级 | ❌ 没有等级表 / `get_season_gauge_fill_ratio` | `zPlayerInner` + 全 exe 常量普查 |
| TH16 季节解放 | ❌ 只有一个 `BOMB_PTR`，`zBomb` 无 `is_season` | statics + 结构体 |
| TH17 出现动物灵 | ❌ 没有 `GoastManager` / `zToken` / 全局指针 | funcs 729 / statics 154 全表 |
| TH17 动物灵收集槽 | ❌ 没有 `TOKENS[5]` / `HYPER_FILL` 对应字段 | `zGlobalsInner` 逐地址引用普查 |
| TH17 咆哮模式 | ❌ 没有 `HYPER_TIME` / `HYPER_FLAGS` / `do_hyper_cancel` | 同上 |
| TH17 中弹只退出咆哮 | ❌ 引擎无此分支；**通用机制**（deathbomb + `th18:0x45d590` + 卡钩子）活着 | `Player::die` 一手反编译 |

→ 复刻只能**自建**，而且接缝都在卡侧（SDK 已有）。

## 1. 季节卡（id 119，`native/cards/season.c` + `cards/season_core.c`）—— 已实现（**装备卡**）

用户 2026-09-10 定案：**这是装备卡**，且「季节解放」要绑游戏不用的键（A/S）。
2026-09-09 拿到 TH16 脱壳版 exe 后（[`engine/season/th16/01`](../../../engine/season/th16/01-season-data-and-tables.md)），
**全部数值改成 TH16 一手值**，纯逻辑抽到 `season_core.c`（59 断言单测 `tests/test_season_core.c`）。

| 部件 | 做法 | 一手 / 备注 |
| --- | --- | --- |
| 季节点来源 ① | **擦弹**：读原生擦弹计数 `th18:0x4ccd0c` 的每帧增量撒点（TH16 `Player__do_graze` `th16:0x444cf0` 每颗擦到的弹都撒一颗）| 每帧上限 4 颗 |
| 季节点来源 ② | **敌人掉落**：`on_enemy_drop_pre`，有掉落就补一颗 | 钩子不给敌人坐标 → 撒在自机周围 |
| 季节点来源 ③ | **解放消弹**：`ce_cancel_radius` 的返回值每 8 颗给 1 点（TH16 `gen_items_from_cancel` `th16:0x416a00`）| 每帧上限 4 颗 |
| **季节点外观** | 本卡 anm VM（script 115）+ **TH16 零售切图**：`bullet/item.png` 右上 2×2 = 春/夏/秋/冬 + `(224,32)` = 土用，各 16×16（与 P 点同尺寸），按副季节换 sprite；动画照 script129 | [`season/th16/01`](../../../engine/season/th16/01-season-data-and-tables.md) §4 |
| 季节点运动/收集 | 落体 0.75 px/帧；进 96 px 被吸（每帧走 18% 剩余距离）；16 px 内算捡到 → 加 1 点 + `interruptLabel(1)` 放大淡出 | 本卡自建（TH16 里它们是真道具，会 PoC）|
| 分档 | **TH16 阈值 100/230/390/590/840/1140 → 档位 0..6** | `Player__init_season_level_deltas` `th16:0x43deb0` + seed 字节 |
| **季节子机** | 档 1..3 各多一颗；**阵位照 TH16 副 sht 的 `option_pos`**（每档一套独立阵型：秋 ±48 最宽、冬竖线、其余小扇），写 option 记录的 `+0x64/+0x68/+0x6c/+0x70`（零售卡 `th18:0x40b8c1` 同款写法）；shooterset `SEASON`，弹贴图 `SEASON_SEED` | 表值一手来自 `local/th16.v1.00a/dat/pl0Xsub.sht` +0x40 |
| **子机升级** | 档位 → 子机数 + 射速：`short_timer = 帧数 × 档位`、`fire_rate = 120` ⇒ 间隔 120/60/40/30/24 帧 | 换弹型留作下一步 |
| **副季节** | **S 键**循环 春→夏→秋→冬→土用（TH16 是菜单里选；本卡给个运行时开关）| HUD 显示当前副季节 |
| **季节解放** | **A 键**；按副季节查 TH16 表出「半径 / 持续帧 / 每帧伤害 / 无敌帧」，持续期间**每帧**半径清弹 + 一个伤害源 | 表见 [`season/th16/01`](../../../engine/season/th16/01-season-data-and-tables.md) §3 |
| 消耗 | 春/秋/冬 = 槽清零；**夏/土用 = 只扣一档** | `Bomb__activate_bomb` `th16:0x40db20` |
| 副季节特性 | 秋：释放期间移速 ×1.5（写 `th18:0x477ec`）；冬：释放期间自机弹伤害 ×1.5（`ce_set_bullet_damage_permille`）| TH16 `th16:0x16688` / `th16:0x2c7cc` |
| HUD | 屏幕**左下角**两行（照 TH16 计量槽位置）；玩家贴左下角（`x < -64 && y > 400`）→ 4 帧淡到 alpha 64，离开淡回 255（TH16 `th16:0x427cf0` 的闩锁 + 迟滞）。**纯 ASCII** | |
| 卡图 | 临时复用绀珠传的 116/117 | 占位 |

**五副季节的释放参数（一手，`season_core.c` 里的表；下标 = 档位 0..6）**：

| 副季节 | 持续帧 | 半径 | 每帧伤害 | 无敌帧 | 消耗 |
| --- | --- | --- | --- | --- | --- |
| 春 Spring | 10→20 | 60→**300** | 100 | 持续 + 60 | 清零 |
| 夏 Summer | 恒 30 | 40→50 | 100 | 9 | 只扣一档 |
| 秋 Fall | 40→130 | 恒 60 | 30 | 30 | 清零 |
| 冬 Winter | 30→**190** | 30→60 | 9 | 30 | 清零 |
| 土用 Doyou | 恒 40 | 50→60 | 100 | 10 | 只扣一档 |

**已知代价 / 未做**：
1. ⚠️ **A/S 键不进 replay 输入流** → 用它触发的效果回放不重现。要 replay 安全得改用掩码里的键（C/D）
   或让卡往掩码写一个空闲位（顺序未验 🟡，[`engine/input/th18/01`](../../../engine/input/th18/01-key-mapping.md) §4a）。
2. mod 的 DLL 因此多了 `USER32.dll` 依赖（`make dllverify` 已改成 3 个依赖）。
3. 季节点是**本卡自己的 anm VM**（外观已按 TH16 重画），因此不进 TH18 的得分/道具系统；
   **绿分数道具**（TH16 消弹专用、提高蓝点最大得点）没做（TH18 无此概念）。
4. 土用「消弹圈向后移动并缩小」没做（本卡清弹圈钉在自机身上，秋的"随自机移动"因此等效）。
5. 子机按档位换弹型（现在只提速）。

## 2. 动物灵卡（id 120，**未实现**，设计先行）

TH18 没有残骸，全部自建；机制拆成四块，全部有现成接缝：

| 部件 | 设计 | 接缝 |
| --- | --- | --- |
| 出现动物灵 | 击杀时从**敌人坐标**撒一颗占位「动物灵」（`on_shot_damage(c, dmg, pos)` 给的是敌世界坐标，用它记住最后击杀点）| `on_shot_damage` / `ce_spawn_item` |
| 收集槽 | `on_item_collect` 计数（只认自己撒的）→ 5 类计数可选（先做单一槽）| `on_item_collect` |
| 咆哮模式 | 槽满 → 自动进咆哮（或 C 键）：`HYPER_TIME` 用卡私有状态计帧；期间抬自机弹伤害倍率 | `ce_set_bullet_damage_permille`（SDK §AA）|
| 中弹只退出咆哮 | `on_death_before_deathbomb`：咆哮中 → `Player::cancel_impending_death`(`th18:0x45d590`) + 清咆哮 + 播个音，**不扣命** | SDK 死亡三钩子 |

**风险 / 待验**：
- `on_death_before_deathbomb` 里调 `cancel_impending_death` 是否足够「不死」（要不要连 `CURRENT_LIVES`
  一起回滚？零售 death-save 卡是怎么做的 → 读 `CardEirin`/`CardMamizou` 的 `on_player_death_after_deathbomb`）；
- 咆哮期间的自机弹伤害倍率会与伤害层聚合（`bullet_damage_permille`）叠加，量级要定。

## 3. 测试与验收

- 季节卡：`make dll` 通过；实机看 ① 敌人死时掉点 ② 收 10 颗进一档 ③ C 键清弹+无敌+扣档 ④ HUD 行。
- 动物灵卡：待实现。
- 主机单测：两张卡的纯逻辑（槽/档/计时）如果超过几十行就抽 `*_core.c` + 单测；占位版逻辑留在卡里。
