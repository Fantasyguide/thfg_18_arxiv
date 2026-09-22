# item/ — 道具子系统

道具 = 敌人/符卡/消弹送出来的那些「点 / P / 樱点 / 时点 / 金钱」实体。
各作的道具**类型号、池结构、掉落驱动**都不同，所以一手证据按版本放 `<版本>/`，
本文件只做跨版本索引 + 断言矩阵。

**TH10 / TH11 侧**（2026-09-15 回流）：两作沿用同一张「`type−1` 跳表」形状，但**类型号一个都不能搬**
（TH10 的 type 3/8/9 = 大/微/小信仰点，TH11 的 type 3/9/8 = 大/中/小绿点）。
TH11 另有两点是本层新东西：**type 5 = 残机碎片（5 片换 1 命）**、以及一个独立的
**收集 `+N` 弹窗管理器**（`th11:0x438440`，11 槽、60 帧寿命、数值逐帧线性归零）——
TH10 的 HUD 是 vm 数字组、没有这套弹字。

> **版本**：跨版本。下文出现地址时一律写全 `thXX:0x……`。

> **玩家视角（「这一作到底有哪些道具、长什么样」）见 [`../_shared/rosters/th14-th18.md`](../_shared/rosters/th14-th18.md)** ——
> 本文件管引擎语义，那份管「原版有没有」；TH18 的擦弹产出物结论与排除法证据链在那份的 §5.2。

## 一手文档

| 版本 | 文档 | 覆盖 |
| --- | --- | --- |
| th07 v1.00b | [`th07/01-sakura-items.md`](th07/01-sakura-items.md) | 类型表 0–9 / `spawn_item` / 池 1100×0x288 / 三个掉落 opcode（119、124、154）/ 樱点三档的**全部**来源与数量 / etama.anm 图 |
| th08 v1.00d | [`th08/01-time-orbs.md`](th08/01-time-orbs.md) | 时点（刻符）= type 7；符卡 100/700/1000；敌机 6..26 / 40；**state 机（0 下落 / 1 归航 / 2 撒入 / 3 时点出生态 / 5 type10 档）**；中弹停住与自动收集；贴图 = `etama.anm` sprite179 |
| th11 v1.00a | [`th11/01-item-types-and-popup.md`](th11/01-item-types-and-popup.md) | 11 项类型跳表 `th11:0x423f00`（type 5 = 命碎、7 = 1UP 的**订正**）/ `zItem` 字段与贴图 id `type+0x60` / 满火力改写表 / **收集 `+N` 弹窗（`th11:0x438440`：11 槽、60 帧、逐帧归零）** |
| th15 v1.00b | [`th15/01-tick-shake-tint-and-graze-item.md`](th15/01-tick-shake-tint-and-graze-item.md) | `zItem` 两 VM 槽（`+0x10` 主 / `+0x618` 压暗变体，stride `0xc88`）/ type→脚本表 `th15:0x4cb058` / **减速因子的画面副作用（±1 px 抖 + `color_1` 压暗）** / **擦弹 45 帧弹出 type 13 擦弹道具** |

**TH15 侧**（2026-09-19 回流）：TH15 的道具是**两个 AnmVm 槽**（`+0x10` 主 / `+0x618` 压暗变体），
type 1..8 两槽都有、9..15 只有主槽。引擎里唯一会「每帧改道具画法」的地方是**擦弹减速期**
（`th15:0x43fa05`：`VM1.pos = (±1,±1)` + `color_1 = 0xFFFFA0FF`）——
这是「擦弹时满屏道具一起抖一下」的机制，**不是**屏幕后处理。TH15 另有一条别作没有的产物：
**弹幕在擦弹圈里连续待满 45 帧 ⇒ 在擦弹点弹出一颗 type 13 擦弹道具**（`th15:0x419de6`，橄榄黄 additive）。

## 断言 × 版本矩阵

| 断言 | th07 | th08 | th10 | th11 | 出处 |
| --- | :---: | :---: | :---: | :---: | --- |
| 道具有实体（`Item` 池 + 每帧速度积分） | ✅ 池 1100 × `0x288` B | ✅ 池 0x830 × `0x2e4` B | 🟡 | ✅ 2198 × `0x478` B | th07 §1 / th08 §1 / [th11/01 §2](th11/01-item-types-and-popup.md) |
| `spawn_item(pos, type, mode/state)` 是唯一生成口 | ✅ `th07:0x4326f0`（`ret 0xc`） | ✅ `th08:0x4400a0` | ✅ `th10:0x41bb00` | 🟡 | th07 §1.3 / th08 §2 |
| 「火力满 ⇒ 点类道具被改写成别的 type」 | ✅ type 0/2 → **7**（大樱点） | ✅ type 0/2 → **8** | ✅ type 1/4 → **9**、10/11 → **5** | ✅ type 1/4/10/11 → **9** | th07 `th07:0x432730` / th08 `th08:0x4400f1` / [th10/01 §3.6](../faith/th10/01-faith-and-points.md) / [th11/01 §1](th11/01-item-types-and-popup.md) |
| 类型分派表是 `type−1` 的跳表 | ❓ | ❓ | ✅ 11 项 `th10:0x41be1c` | ✅ 11 项 `th11:0x423f00` | [th11/01 §1](th11/01-item-types-and-popup.md) |
| 贴图脚本号 = 常数 + type | ✅ `0x2ac+type` 一族 | ✅ 脚本号 `61+type` | ✅ `0x176+type` | ✅ `type+0x60` | th08 §6.1 / [th11/01 §2](th11/01-item-types-and-popup.md) |
| 敌机死亡掉落数量由**引擎**算、不是 ECL 定量 | ✅ 死敌各 1 颗 ★ + item 槽各 1 颗 | ✅ 6..26 / 40（按连锁段数） | ❓ | ❓ | th07 §3.2 / th08 §4 |
| ECL 有「掉落」opcode | ✅ 119 / 124 / 154 | ✅ 142 / 168 | ✅ 329 系列 `th10:0x40fe9e` | ❓ | th07 §3.1 / th08 §3 |
| 消弹会掉樱点 / 时点 / 绿点 | ✅ 每颗弹 1 颗（type 6 或 8） | ✅ 每颗弹 1 颗 type 7 | ❓ | ✅ 小绿点（type 8，+0.1 显示 +10 分） | th07 §3.2 / th08 §3 / [faith/th11/01 §6](../faith/th11/01-faith-and-points.md) |
| 中弹会停住道具池 | ❓ 未查 | ✅ `cancel_autocollect` | ❓ | ❓ | th08 §7.2 |
| 「第二条计分行」满值 ⇒ 落下中的道具全被收 | — | — | ❌ | ✅ `th11:0x42364a` → state 3 | [faith/th11/02 §5](../faith/th11/02-koushin-signal.md) |
| 收集时弹 `+N` 数字，由独立管理器负责 | ❓ | ❓ | ❓（HUD 是 vm 数字组） | ✅ `th11:0x438440`：11 槽 / 60 帧 | [th11/01 §3](th11/01-item-types-and-popup.md) |
| 道具能加成残机 | ✅ 1UP 类 | ✅ 1UP / 刻符 | ✅ type 7 = 1UP | ✅ type 5 = 命碎（**5 片 = 1 命**）、7 = 1UP | [th11/01 §1](th11/01-item-types-and-popup.md)、[player/th11/01 §4](../player/th11/01-bomb-and-lifefrags.md) |
| 类型 → sprite 的落点 | ✅ `0x2ac+type` / `0x2b6+type`（etama2.png） | ❓ 未查 | ❓ | 🟡 `type+0x60`（未解包 `item.anm`） | th07 §3.3 |

> **玩家视角清册**（这一作玩家看到哪些道具、长什么样、叫什么、擦弹产不产拾取物）见
> [`_shared/rosters/th06-th09.md`](../_shared/rosters/th06-th09.md)（th06–th09）；另外两册是 `th10-th13` / `th14-th18`。

## 与 mod 侧的关系

TH18 卡牌 mod 的**系统卡 107（妖妖梦 / 森罗结界）**要用 th07 的樱点机制：
出处与偏差表在 [`docs/superpowers/specs/2026-09-12-senro-107-design.md`](../../docs/superpowers/specs/2026-09-12-senro-107-design.md)。
⚠ 该 spec §4.2 的**两个 TH07 类型号错位一格**（把大樱点记成 type 2、把 ★ 记成 type 7），
以 [`th07/01-sakura-items.md`](th07/01-sakura-items.md) §2.1 为准。
