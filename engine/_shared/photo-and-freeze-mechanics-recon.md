# 三作特殊机制侦察：妖精大战争的**冻结** / 文花帖与秘封噩梦日记的**拍照**（2026-09-18 起）

> **版本**：跨版本（TH09.5 v1.02a / TH12.8 v1.00a / TH16.5 v1.00a 三个新样本，
> 目标是把它们**搬进 TH18 v1.00a** 的卡系统）。**本文是侦察稿**：只写一手已确认的锚点 + 待办，
> 未确认的一律标 🟡/❓，不许当结论用。
> 纪律：`METHOD.md`（发现→推测→验证→结论+可信度→证据）、`DOCSTYLE.md`（地址反引号、跨版本带前缀）。

## 0. 目标（用户 2026-09-18 口径，逐字）

「……新放的小数点作，你研究一下**妖精大战争的冻结机制**和**其他两作的拍照机制**可不可以做进来，
**他们是怎么实现那些机制的**」⇒ 两问：**① 原版怎么做的（一手）**；**② 能不能做进 TH18 的卡系统**。

## 1. 样本与登记（已完成）

| 作 | 样本（本机）| 社区命名金矿（一手）| INDEX |
| --- | --- | --- | --- |
| TH09.5 文花帖 | `local/th095.v1.02a/th095.exe`（696832 B，2006-01-13）| `local/vendor/th-re-data/data/th095.v1.02a/` | [`games/th095.v1.02a/INDEX.md`](../../games/th095.v1.02a/INDEX.md) |
| TH12.8 妖精大战争 | `local/th128.v1.00a/th128.exe`（752640 B，2010-08-14）| `…/th128.v1.00a/` | [`games/th128.v1.00a/INDEX.md`](../../games/th128.v1.00a/INDEX.md) |
| TH16.5 秘封噩梦日记 | `local/th165.v1.00a/th165.exe`（740352 B，2018-07-31）| `…/th165.v1.00a/` | [`games/th165.v1.00a/INDEX.md`](../../games/th165.v1.00a/INDEX.md) |

金矿三作**都有**（funcs / statics / 结构体齐全）⇒ 命名不用从零开始。汉化版另有 `th095c.exe` / `th128_CN.exe`，
留在原目录（**版本不同 ⇒ 结论只对日文原版负责**）；`.dat`（资产）未复制（要挖贴图/脚本时再说）。

## 2. 拍照机制：锚点（一手 = 金矿命名，地址已出）

**TH09.5（文花帖）—— `PhotoManager` 一族**（都是函数名 + 地址，来自金矿 `funcs.json`）：

| 符号 | 地址 |
| --- | --- |
| `PhotoManager::constructor` | `th095:0x42a8a0` |
| `PhotoManagerArrayItem::constructor` / `denstructor` | `th095:0x42a940` / `th095:0x42a9c0` |
| `PhotoManager::operator new` | `th095:0x42abc0` |
| `PhotoManager::on_tick_08` | `th095:0x42c3e0` |
| `PhotoManager::on_draw_1a` | `th095:0x42c410` |

结构体：`zPhotoManager` / `zPhotoManagerArrayItem`（金矿 `type-structs-own.json`）⇒ **下一轮**逐字段读出来。

**TH16.5（秘封噩梦日记）—— `PhotoManager` + `zCamera`**：

| 符号 | 地址 |
| --- | --- |
| `PhotoManager::operator new` / `on_tick` / `on_draw_36` / `sub_441540` | 金矿 `funcs.json`（下一轮取地址）|
| `PauseMenu::take_snapshot_for_pause` | 同上（**暂停截图**，与"拍照攻击"是两件事，别混）|
| 结构体 `zCamera` / `zCameraSky` / `zPhotoManager` | 金矿 `type-structs-own.json` |

**待答（下一轮的一手问题）**：① 拍照判定盒（矩形）在哪、尺寸从哪来；② "入镜"计分（弹数/敌数）怎么统计；
③ 冷却/次数/胶卷（StB 的"胶卷"= 关卡内可拍次数）存在哪个字段；④ 拍到之后的消弹/伤害链是什么；
⑤ TH16.5 与 TH09.5 这两套是同一谱系（都叫 `PhotoManager`）还是各写一套。

## 3. 冻结机制：TH12.8（妖精大战争）

金矿里**没有** `freeze`/`ice` 命名的函数或字段 ⇒ 名字得自己找（🟡 未开始）。下一轮的抓手（按代价排）：

1. `zBullet` / `zBulletManager` 的**结构体字段**里找"冻结计时/冻结位"（金矿 `type-structs-own.json`）；
2. 玩家侧的"冰"状态：GFW 的冻结是**按住射击键冻住附近弹**、松开炸碎 ⇒ 找玩家 tick 里
   读写**弹池**的那个循环（TH12 的 `BulletManager` 锚点本仓已有：`engine/` 里 TH12 相关的反汇编笔记）；
3. 冻结的**表现**（冰晶/闪烁）与**结算**（碎片的消弹/掉点）分别在哪个函数 ⇒ 决定"搬进 TH18"要几个站点。

## 4. 「能不能做进来」的初判（🟡 框架，等一手填实）

TH18 侧我们已经有的**可复用面**（一手，本仓已有）：

| 需要的能力 | TH18 侧现成物 | 出处 |
| --- | --- | --- |
| 每帧扫弹池（冻结/拍照的判定都靠它）| `on_tick_2` / `ce_item_frame` 等既有每帧站点 + `Bullet__cancel` 函数调用 | `COMPAT-MATRIX.md`、`engine/_shared/thcrap-breakpoint-pools.md` §2 |
| 矩形伤害 / 范围消弹 | `ce_damage_rect` / `ce_cancel_radius`（既有 SDK 调用，**0 新站点**）| 同上（109 暴风连锁已跑通）|
| 蓝圈自扫 + 白弹判据 | 109 已实装（半径 ≤ 4.0 = 白弹）| `engine/_shared/site-budget-and-primitives.md` §2.3 |
| 屏上贴图/遮罩（冰晶、取景框、闪光）| `ce_hud_vm_*` / 原生 anm 脚本追加（`ability.anm`）| `assets/README.md`、`engine/anm/th18/` |

⇒ **框架结论（🟡，待一手钉）**：两套机制在 TH18 侧都不需要"新站点"就能搭出**近似**版本
（判定/消弹/表现全走既有调用与 anm 脚本）；真正的成本在**原版量的复刻**（冻结半径/持续/解冻伤害；
拍照盒尺寸/入镜计分/冷却）——那些必须逐条从三作的 exe 里读出来，不许自定后包装成"复刻"（`METHOD.md` 铁律）。

## 5. 下一轮计划（按代价排）

1. **TH09.5 拍照**：读 `zPhotoManager` / `zPhotoManagerArrayItem` 字段表 + `on_tick_08` 的判定盒；
2. **TH16.5 拍照**：读 `zCamera` / `PhotoManager` + `on_draw_36`，与 TH09.5 对照谱系；
3. **TH12.8 冻结**：先扫 `zBullet` 字段找冻结位，再顺玩家 tick 找冻结/解冻的写入点；
4. 每条结论回写本文件（或按子系统拆到 `engine/<子系统>/<版本>/`），并给 TH18 侧的落地核算（站点数 + 一手量）。

## 6. 工具配方（本轮踩过的坑，下一轮直接用）

**金矿 `type-structs-own.json` 的形状**（三作同形）：顶层是 **`dict`：结构体名 → 成员表**，
成员表是 **`[[offset, 成员名, C 类型], …]`**（`offset` 是 `'0x..'` 字符串，最后一条常是 `['0x..','__end',None]`）。
⇒ 读字段表要**按这个形状遍历**（别按 `{name, addr, members}` 那套猜，本轮猜错过一次）：

```python
d = json.load(open("local/vendor/th-re-data/data/<ver>/type-structs-own.json", encoding="utf-8"))
for off, name, typ in d["zBullet"]:            # ← 结构体名做键
    print(off, name, typ)
```

**已确认存在的结构体（可作下一轮起点）**：TH12.8 有 `zBullet` / `zBulletManager` / `zPlayer`（冻结要在这三个里找）；
TH09.5 有 `zPhotoManager` / `zPhotoManagerArrayItem`；TH16.5 有 `zCamera` / `zCameraSky` / `zPhotoManager`。
最后一条常是 `['0x..','__end',None]` ⇒ 它就是对象大小（别当字段读）。

**汉化 exe 与本作的关系**：`th095c.exe`（700416 B）/ `th128_CN.exe`（752640 B，与日文同尺寸）是**另一版**
（汉化改过 exe 或只是换文本）⇒ 本轮所有地址**只对日文原版**负责；要从汉化版挖东西必须另开一版登记。

## 7. ★ TH18 侧「遗留代码」盘点（用户 2026-09-18 追问：有没有 th18 遗留代码可用，比如时间缓速）

**结论：有，而且不止一处。** 一手 = 金矿四作的符号表对查（`th15.v1.00b` / `th165.v1.00a` /
`th17.v1.00b` / `th18.v1.00a`），表格里只列**符号存在与否**；「是否存在活的代码路径」另标。

| 遗留 | TH18 里有吗 | 证据（一手）| 对我们的用处 |
| --- | --- | --- | --- |
| **TH15 擦弹减速因子**（float）| **有**（本仓已在读）| `native/engine.h` 的 `CE_ITEM_SLOWDOWN` = `th18:0xe6bb14`；`cards/faith.c` 的 `faith_item_slowdown_permille()` 已在用 | 现成的"缓速"旋钮（**只作用于道具速度**，不是全局时间）|
| **相机子系统**（TH17 那一套）| **有**（同名同族）| 见下方清单（`th17` 与 `th18` 都出现）| 拍照机制最值钱的遗留：取景/姿态/视野可直接驱动 |
| **全局 `slowdown`**（TH16.5 的相机模式时间缓速）| **没有** | `slowdown` 只在 `th165.v1.00a` 出现 | 全局时间缓速要自建（每帧家务 / 数据层），或用上表第二行的相机近似 |
| **拍照充能** `camera_charge_multiplier` / `zCamera` / `zCameraSky`（TH16.5 专有）| **没有** | 只在 `th165.v1.00a` | 充能拍照的**量**要从 TH16.5 一手读出来自己实现 |
| 暂停截图 `PauseMenu::take_snapshot_for_pause` | **没有**（TH17/TH16.5 有）| `th165`/`th17` 有、`th18` 无 | 与拍照攻击无关，但说明沿用**不保证逐代保留** |
| TH15 的 snapshot/restore 家族（`AnmManager::store_snapshot_of_vm` / `BombReimu::store_snapshot` …）| **没有** | 只在 `th15.v1.00b` | 与"拍照"无关（它是 bomb 的 VM 快照）—— 别混 |
| TH18 独有 | `stage_gamestate_snapshots` / `zReplayGamestateSnapshot` | 只在 `th18` | 回放状态快照 ≠ 拍照 |

**相机那一行的完整符号清单（一手，两作同名）**：`camera_0`…`camera_3` / `camera_pos_i` / `camera_fov_i` /
`camera_sky_i` / `camera_facing_i` / `camera_up_i` / `current_camera` / `current_camera_index` / `camera_index` /
`Supervisor::set_camera_by_index`（`th17.v1.00b` 与 `th18.v1.00a` 都出现）；TH16.5 另有 `zCamera`/`zCameraSky`/
`camera_charge_multiplier`（TH18 无）。

**由此定的两条路线（下一轮按这个挖）**

1. **拍照卡**：走 TH18 的**相机遗留**（第一手要先核 `Supervisor::set_camera_by_index` 与那批 camera 全局
   在 TH18 的**每帧驱动者**是谁 ⇒ 决定是"数据层驱动"还是"每帧家务 +0 站点"）；
   TH16.5 的 `camera_charge_multiplier` / `zCamera` 一手读出**充能量与判定盒**，TH09.5 的 `PhotoManager`
   读出**胶卷/冷却**语义（两作对照 ⇒ 哪个更像我们要复刻的）。
2. **冻结卡**：先确认 TH18 的 `zBullet` 是否留着 TH12.8 的冻结位（把两作的 `zBullet` 字段名**逐条对查**，
   金矿两边都有结构体 ⇒ 可脚本化）；有遗留 ⇒ 数据层即可；没有 ⇒ 走"每帧扫弹池 + 自己记冻结表"
   （站点 0 的成本，109 暴风连锁已是先例）。
