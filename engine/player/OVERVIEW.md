# player — 自机运行时对象
> **版本**：跨版本。本文出现的地址一律带版本前缀（如 `th16:0x442560`）。


自机不是一个对象，是**三个**：`player`（本体：状态机、输入、碰撞、资源）、
`PlayerInner`（射击侧：shooterset 选择、option 重建）、以及从 `PLAYER+0xd080` 池里
按需分配的**伤害源对象**（自机弹命中敌人时的载体）。每帧由主循环的优先级表调度
（[`../_shared/frame-loop.md`](../_shared/frame-loop.md)）。

生命周期是一个显式状态机；「炸弹」在 TH16 其实是**两套并行机制**——主炸消耗角色库存，
季节释放（C 键）消耗季节槽——它们走同一个 `Bomb` 对象但计费不同。这个区分在别的作品里
未必成立（TH18 用卡牌取代了季节系统），是矩阵里最需要逐版本重验的一行。

**TH10 / TH11 侧**（2026-09-15 回流）：这两作的灵击**都只吃火力**，没有「炸弹库存」这个概念——
TH10 的门槛是 `火力/20 != 0`、消耗 −20 单位（正好 1.00），TH11 改成 `POWER/BOMB_COST != 0`，
而 `BOMB_COST` 是**自机文件里的字段**（5/6 个自机 = 20 = 1.00，**魔理沙A 是 12 = 0.60**）。
伤害的实现也分道扬镳：TH10 的 `Bomb` 不自己打人，它暴露一个 `Bomb::damage_bonus_at`（`th10:0x4059f0`）
由**玩家→敌人伤害例程回调**；TH11 则统一走「矩形伤害源 × 存活帧数」原语（`th11:0x433a90`，32 槽），
与 TH16/TH18 那一族同形。两作的灵击**都没有震屏**（TH10 机械否定、TH11 倾向否定）。
另外 TH11 的玩家侧还有一套六个「支援特技」（`Player::move` 里按 `3×CHARACTER+SUBSHOT` 的 if-else 链分派），
其中霊夢×紫的缺口瞬移是**贴边后 8 帧内二次方向输入**——TH10 没有这种判定。

## TH06NC 入口

TH06NC 的玩家主循环 `th06nc:0x140068820`（chain 回调，无直接 xref）、
弹幕 tick `th06nc:0x140010870`、被弹判定 `th06nc:0x14006a980`、
激光判定 `th06nc:0x14006aba0`、射击伤害 `th06nc:0x140069d00`、
敌人 tick `th06nc:0x1400373b0` 已定位；`Die()` / `SpawnBullets` /
`UpdatePlayerBullets` 三段被内联，只登记区间。

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
| **决死窗口** | 首命 8 帧，之后每次复活变 **6** 帧 | **恒为 8 帧**（`Die()` 每次重写 `respawnTimer = 8`） | [01 §2.18](th06nc/01-main-loop-and-collision.md) 🟡 未实机 |
| `deaths++` 的时机 | 被弹当场（`Die()` 内，`th06:0x004277ff`） | **挪到死亡结算**（决死窗口耗尽才算）→ 决死成功不计死亡 | [01 §2.20](th06nc/01-main-loop-and-collision.md) 🟡 |
| 死亡结算（6 道具 / 5 道具、扣 16 火力） | — | **逐项相同** | [01 §2.19](th06nc/01-main-loop-and-collision.md) |
| **挑战模式**：不扣命 / 死亡火力下限 8 / 1UP 改抵扣死亡数 / 死 >5 次拿不到满记录 | **无对应物** | ✅ 机制一手；名称与意图由用户告知 | [01 §2.20](th06nc/01-main-loop-and-collision.md) |

半径与半宽同为 1.25，所以圆内接于方，判定面积小约 21%。

> **两条反复出现的教训**（不只对 TH06NC 成立）：
> ① **结构体顺序不能跨版本照抄**——`Enemy` 与 `PlayerBullet` 都被 NC 重排过；
> 偏移必须靠访问模式或数组步长这类硬证据。
> ② **NC 内联得比 TH06 多**，被内联的函数在 NC 侧根本没有地址可命名
> （[`../ecl/th06nc/03`](../ecl/th06nc/03-opcode-crossmatch.md) §4）。

## 断言 × 版本矩阵

| 断言 | th10 | th11 | th15 | th16 | th18 | 证据 |
| --- | :---: | :---: | :---: | :---: | :---: | --- |
| 生命是 5 态状态机，`player_update_perframe` `th16:0x442560` 按 `+0x165a8` switch | — | — | — | ✅ | 🟡 | [th16/01 §1](th16/01-hit-life-system.md) |
| 无敌帧计数在 `+0x1663c` | — | — | — | ✅ | 🟡 | [th16/01 §3](th16/01-hit-life-system.md) |
| 死亡 commit 与复活是分离的两步（复活在状态 2、帧 >0x1d） | — | — | — | ✅ | 🟡 | [th16/01 §4–5](th16/01-hit-life-system.md) |
| 聚焦 = INPUT bit3，读 `+0x165c8`（**订正**了早期把它当 SHT 字段的说法） | — | — | — | ✅ | 🟡 | [th16/03 §3](th16/03-fire-input-movement.md) |
| 开火门控在 `Player__tick_shooting_state` `th16:0x4455d0` | — | — | — | ✅ | 🟡 | [th16/03 §2](th16/03-fire-input-movement.md) |
| 擦弹落点 = 一个 per-graze 函数（`th15:0x4573a0` / `th16:0x444cf0` / `th18:0x45e160`） | ❌ 无计数 | ✅ `th11:0x40bb10` | ✅ | ✅ | ✅ | [th15/01 §1](th15/01-graze-and-item-slowdown.md)、[th16/01 §2c](th16/01-hit-life-system.md)、[th18/02 §1](th18/02-graze-and-item-slowdown.md) |
| 擦弹把「道具下落减速因子」打成 0.3（每帧 +0.1 恢复） | ❓ | ❓ | ✅ | ❌ | ❌ | [th15/01 §4](th15/01-graze-and-item-slowdown.md)、[th18/02 §2](th18/02-graze-and-item-slowdown.md) |
| `zItemManager` 有 `lolk_item_slowdown_factor` 字段、引擎每帧 +0.1 恢复（TH18 无人写它） | ❓ | ❓ | ❓ | ❓ | ✅ | [th18/02 §2](th18/02-graze-and-item-slowdown.md) |
| 擦弹判定两种模式：矩形外扩 **24 px** / 圆形边距 `max(弹半径/2.5, 40)` | 🟡 24.0f 扩的是弹框 | 🟡 | ✅ | 🟡 | ✅ | [th15/01 §1b](th15/01-graze-and-item-slowdown.md)、[th18/02 §1b](th18/02-graze-and-item-slowdown.md) |
| 同一颗弹可以重复擦弹（per-bullet 冷却） | ❌ 每弹一次 | ❌ 每弹一次 | ✅ | ❓ | ❌ | [th15/01 §1b](th15/01-graze-and-item-slowdown.md)、[th18/02 §1b](th18/02-graze-and-item-slowdown.md) |
| 炸弹由独立 `Bomb` 对象承载（0x108 字节，`Bomb__operator_new` `th16:0x40d890`） | ✅ `th10:0x4776ec` | ✅ `th11:0x4a8d64` | — | ✅ | ❓ | [th16/02 §1](th16/02-season-release-and-bombs.md)、[th16/05 §2](th16/05-object-field-maps.md) |
| **主炸与季节释放是两套计费**：主炸耗角色库存，季节释放耗季节槽 | ❌ 只有火力 | ❌ 只有火力 | — | ✅ | ❌ | [th16/02 §2–5](th16/02-season-release-and-bombs.md)；th18 见 [card/](../card/OVERVIEW.md) |
| 灵击门槛是「火力 ≥ 1.00」这类连续判据 | ✅ 火力/20 ≠ 0 | ❌ `POWER/BOMB_COST ≠ 0` | — | — | — | [th10/01 §1](th10/01-bomb-full-chain.md)、[th11/01 §1](th11/01-bomb-and-lifefrags.md) |
| 灵击的消耗是**火力**（不是库存） | ✅ −20 单位 | ✅ −`BOMB_COST`（魔理沙A 只 −0.60） | — | ❌ 耗库存 | ❌ | [th10/01 §1](th10/01-bomb-full-chain.md)、[th11/01 §1](th11/01-bomb-and-lifefrags.md) |
| 灵击会动「第二条计分行」 | ✅ 非决死 −3000 信仰 | ❌ 不动信仰也不动交信度 | — | — | — | [faith/th10/01 §3.2](../faith/th10/01-faith-and-points.md) |
| 灵击伤害走「矩形伤害源 × 存活帧数」原语 | ❌ 走 `Bomb::damage_bonus_at` 回调 | ✅ `th11:0x433a90`（32 槽） | — | — | ✅ `th18:0x45dfa0` | [th11/01 §2](th11/01-bomb-and-lifefrags.md)、[th18/02 §1](th18/02-damage-sources.md) |
| 灵击有震屏 | ❌ 机械否定 | ❓ 倾向否定 | — | — | — | [th10/01 §4](th10/01-bomb-full-chain.md)、[th11/01 §3](th11/01-bomb-and-lifefrags.md) |
| 命碎由**分数阈值 + 捡道具**两条路发放，**5 片 = 1 命** | — | ✅ | — | ❌ 无拾取来源 | — | [th11/01 §4](th11/01-bomb-and-lifefrags.md)、[th16/06 §4](th16/06-resource-economy.md) |
| 「子机上限 N」是**数组长度**而不是一个常量 | — | ✅ `options[8]` | — | — | — | [th11/02 §4](th11/02-support-special-effects.md) |
| 每机体一套「支援特技」，各带独立计数器字段 | — | ✅ 六条 | — | — | — | [th11/02](th11/02-support-special-effects.md)、[input/th11/01](../input/th11/01-special-detection.md) |
| option/子机由 `PlayerInner__repopulate_options` `th16:0x4440e0` 整体重建 | — | ✅ `th11:0x432cc0` | — | ✅ | 🟡 | [th16/04 §1](th16/04-options-subshot-system.md) |
| 本体子机挂火力档、季节子机挂季节档，是两条独立链 | — | — | — | ✅ | ❌ | [th16/04 §2–3](th16/04-options-subshot-system.md) |
| 伤害源从 `PLAYER+0xd080` 池分配（stride 0x94，256 个） | — | 🟡 stride `0x74`×32 | — | ✅ | 🟡 | [th16/05 §3](th16/05-object-field-maps.md)、[sht/th16/08 §2](../sht/th16/08-th16-player-damage-pipeline.md) |
| 命碎片**没有任何拾取来源**（负结论，evidence-complete） | — | ❌ 有（type 5 道具） | — | ✅ | 🟡 | [th16/06 §4](th16/06-resource-economy.md) |
| 自机位置 = `inner.field_0` 前 12 字节（float px）+ 8 字节 1/128 定点 | — | 🟡 `+0x888` 单位 1/256 | — | 🟡 | ✅ | [th18/01](th18/01-position-and-state-timers.md)、[th11/02 §1](th11/02-support-special-effects.md) |
| 收点线（PoC 高度）= `zPlayer+0x47998`：全 exe 只有两个写点（`Player__reset` 128 / 原生卡 22 的 `on_load` 224）、无每帧写点；开局指示线现读它 | — | ❓ | — | ❓ | ✅ | [th18/05](th18/05-poc-height-and-indicator.md)、[card/th18/08 §22](../card/th18/08-catalog.md) |

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
