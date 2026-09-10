# player — 自机运行时对象
> **版本**：跨版本。本文出现的地址一律带版本前缀（如 `th16:0x442560`）。


自机不是一个对象，是**三个**：`player`（本体：状态机、输入、碰撞、资源）、
`PlayerInner`（射击侧：shooterset 选择、option 重建）、以及从 `PLAYER+0xd080` 池里
按需分配的**伤害源对象**（自机弹命中敌人时的载体）。每帧由主循环的优先级表调度
（[`../_shared/frame-loop.md`](../_shared/frame-loop.md)）。

生命周期是一个显式状态机；「炸弹」在 TH16 其实是**两套并行机制**——主炸消耗角色库存，
季节释放（C 键）消耗季节槽——它们走同一个 `Bomb` 对象但计费不同。这个区分在别的作品里
未必成立（TH18 用卡牌取代了季节系统），是矩阵里最需要逐版本重验的一行。

## TH06NC 入口

TH06NC 的玩家主循环 `th06nc:0x140068820`（chain 回调，无直接 xref）、
弹幕 tick `th06nc:0x140010870`、被弹判定 `th06nc:0x14006a980`、
激光判定 `th06nc:0x14006aba0`、射击伤害 `th06nc:0x140069d00`、
敌人 tick `th06nc:0x1400373b0` 已定位。

### TH06 → TH06NC 的已验差异

底本是 CC0 的 [`th06-decomp`](https://github.com/GensokyoClub/th06)，逐条一手比对。
详见 [th06nc/01-main-loop-and-collision.md](th06nc/01-main-loop-and-collision.md)。

| 断言 | TH06 | TH06NC | 出处 |
| --- | --- | --- | --- |
| 被弹判定形状 | 轴对齐盒（三字段） | **圆**（半径 1.25，`player+0x774c`） | [01 §2.7](th06nc/01-main-loop-and-collision.md) |
| 擦弹判定形状 | 轴对齐盒 | **圆**，复用同一半径字段 | [01 §2.9](th06nc/01-main-loop-and-collision.md) |
| 每关擦弹上限 | 9999 | **99999** | [01 §2.9](th06nc/01-main-loop-and-collision.md) |
| `ZunTimer` | 12 字节 `{previous, subFrame, current}` | **8 字节 `{previous, current}`** | [01 §2.13](th06nc/01-main-loop-and-collision.md) |
| 关卡区在窗口里的位置 | 左边界固定 `x=32`，右侧留 HUD | **水平居中**，虚拟屏高固定 480、宽随宽高比 | [01 §2.11](th06nc/01-main-loop-and-collision.md) |
| 自机坐标系 / 可动区 | 关卡区局部，可动区 `(8,16)+(368,416)` | **相同** | [01 §2.11](th06nc/01-main-loop-and-collision.md) |
| 移动速度与角色数据 | `CharacterData` 32 字节 | **相同**（灵梦 4.0/2.0、魔理沙 5.0/2.5） | [01 §2.10](th06nc/01-main-loop-and-collision.md) |
| `PlayerBullet` | `AnmVm` 打头 | **重排**：小标量提前（`bulletState +0x10`、`sprite +0x18`），大小 `0x170`、80 发 | [01 §2.12](th06nc/01-main-loop-and-collision.md) |
| `SpawnBullets` / `UpdatePlayerBullets` | 独立函数 | **内联进 `Player__OnUpdate`**，只能登记区间 | [01 §2.12](th06nc/01-main-loop-and-collision.md) |
| 射击→敌人的伤害链（钳 70、体术 -10、`(dmg/5)*10` 计分、符卡 ÷7/÷3） | — | **逐条同构** | [01 §2.16](th06nc/01-main-loop-and-collision.md) |
| 玩法数值常量（两轮共 13 个：0.5 / 0.3 / -100 / 10 / 14 / 0.25 / 1.5 / 0.125 / 0.1 / 32 / 42 / 48 …） | — | **零偏差全中** | [01 §2.14](th06nc/01-main-loop-and-collision.md)、[§2.16](th06nc/01-main-loop-and-collision.md) |
| `Player` 前 0x400 字节 | `playerSprite` 打头 | **重排**：四张炸弹区域平行数组排到最前 | [01 §2.16](th06nc/01-main-loop-and-collision.md) |

半径与半宽同为 1.25，所以圆内接于方，判定面积小约 21%。

> **两条反复出现的教训**（不只对 TH06NC 成立）：
> ① **结构体顺序不能跨版本照抄**——`Enemy` 与 `PlayerBullet` 都被 NC 重排过；
> 偏移必须靠访问模式或数组步长这类硬证据。
> ② **NC 内联得比 TH06 多**，被内联的函数在 NC 侧根本没有地址可命名
> （[`../ecl/th06nc/03`](../ecl/th06nc/03-opcode-crossmatch.md) §4）。

## 断言 × 版本矩阵

| 断言 | th15 | th16 | th18 | 证据 |
| --- | :---: | :---: | :---: | --- |
| 生命是 5 态状态机，`player_update_perframe` `th16:0x442560` 按 `+0x165a8` switch | — | ✅ | 🟡 | [th16/01 §1](th16/01-hit-life-system.md) |
| 无敌帧计数在 `+0x1663c` | — | ✅ | 🟡 | [th16/01 §3](th16/01-hit-life-system.md) |
| 死亡 commit 与复活是分离的两步（复活在状态 2、帧 >0x1d） | — | ✅ | 🟡 | [th16/01 §4–5](th16/01-hit-life-system.md) |
| 聚焦 = INPUT bit3，读 `+0x165c8`（**订正**了早期把它当 SHT 字段的说法） | — | ✅ | 🟡 | [th16/03 §3](th16/03-fire-input-movement.md) |
| 开火门控在 `Player__tick_shooting_state` `th16:0x4455d0` | — | ✅ | 🟡 | [th16/03 §2](th16/03-fire-input-movement.md) |
| 擦弹落点 = 一个 per-graze 函数（`th15:0x4573a0` / `th16:0x444cf0` / `th18:0x45e160`） | ✅ | ✅ | ✅ | [th15/01 §1](th15/01-graze-and-item-slowdown.md)、[th16/01 §2c](th16/01-hit-life-system.md)、[th18/02 §1](th18/02-graze-and-item-slowdown.md) |
| 擦弹把「道具下落减速因子」打成 0.3（每帧 +0.1 恢复） | ✅ | ❌ | ❌ | [th15/01 §4](th15/01-graze-and-item-slowdown.md)、[th18/02 §2](th18/02-graze-and-item-slowdown.md) |
| `zItemManager` 有 `lolk_item_slowdown_factor` 字段、引擎每帧 +0.1 恢复（TH18 无人写它） | ❓ | ❓ | ✅ | [th18/02 §2](th18/02-graze-and-item-slowdown.md) |
| 擦弹判定两种模式：矩形外扩 **24 px** / 圆形边距 `max(弹半径/2.5, 40)` | ✅ | 🟡 | ✅ | [th15/01 §1b](th15/01-graze-and-item-slowdown.md)、[th18/02 §1b](th18/02-graze-and-item-slowdown.md) |
| 同一颗弹可以重复擦弹（per-bullet 冷却） | ✅ | ❓ | ❌ | [th15/01 §1b](th15/01-graze-and-item-slowdown.md)、[th18/02 §1b](th18/02-graze-and-item-slowdown.md) |
| 炸弹由独立 `Bomb` 对象承载（0x108 字节，`Bomb__operator_new` `th16:0x40d890`） | — | ✅ | ❓ | [th16/02 §1](th16/02-season-release-and-bombs.md)、[th16/05 §2](th16/05-object-field-maps.md) |
| **主炸与季节释放是两套计费**：主炸耗角色库存，季节释放耗季节槽 | — | ✅ | ❌ | [th16/02 §2–5](th16/02-season-release-and-bombs.md)；th18 见 [card/](../card/OVERVIEW.md) |
| option/子机由 `PlayerInner__repopulate_options` `th16:0x4440e0` 整体重建 | — | ✅ | 🟡 | [th16/04 §1](th16/04-options-subshot-system.md) |
| 本体子机挂火力档、季节子机挂季节档，是两条独立链 | — | ✅ | ❌ | [th16/04 §2–3](th16/04-options-subshot-system.md) |
| 伤害源从 `PLAYER+0xd080` 池分配（stride 0x94，256 个） | — | ✅ | 🟡 | [th16/05 §3](th16/05-object-field-maps.md)、[sht/th16/08 §2](../sht/th16/08-th16-player-damage-pipeline.md) |
| 命碎片**没有任何拾取来源**（负结论，evidence-complete） | — | ✅ | 🟡 | [th16/06 §4](th16/06-resource-economy.md) |
| 自机位置 = `inner.field_0` 前 12 字节（float px）+ 8 字节 1/128 定点 | — | 🟡 | ✅ | [th18/01](th18/01-position-and-state-timers.md) |

> **图例**：✅ 该版本一手验过（证据列给地址/出处） · 🟡 待验（从别的版本借来的假设，或单源） · ❌ 已知不同/不存在 · ❓ 存疑 · — 未看
>
> **本页不许出现没有出处的断言。** 从某作借到另一作的判断一律 🟡，在该版本 exe 上验过才能改 ✅（[`METHOD.md`](../../METHOD.md)）。

## th18 待验清单

上表 🟡 全部是「TH16 如此，th18 待验」。TH18 的 `zPlayer` 已从 th16 的 0x2c828 长到
**0x479d4**，所以**每一个偏移都必须重取**——只有字段的*语义*可以借
（[`../../games/th18.v1.00a/port-plan.md`](../../games/th18.v1.00a/port-plan.md)）。

## 开放

- TH16 的 Spellcard / 各角色 Bomb 具体行为仍是语义空白（有名字，没反）。
- 擦弹（graze）判定与 `sht/` 的 header +0x04 哑弹结论有交叉，见 [sht/th16/99-QUIRK](../sht/th16/99-QUIRK-可配置判定半径其实是哑弹.md)。
