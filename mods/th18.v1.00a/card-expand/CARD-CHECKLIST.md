# CARD-CHECKLIST —— 新增一张卡的自检清单（生命周期 / 删卡 / 资源回收）

> **版本**：TH18 v1.00a（`th18.exe`，imagebase `0x400000`）。本文裸地址默认属该版本；引用其他版本须写成 `th16:0x…`。
> **用途**：写卡前打勾、写完复查。机制细节在 [`SDK.md`](SDK.md)，铁律全文在 [`METHOD.md`](../../../METHOD.md)。
> **为什么有这一页**：2026-09-10 用户问「长持续效果撞上倒吊人删卡会怎样」→ 一次查出 7 张卡漏收尾
> （[`NEXT.md`](NEXT.md) 第三十六轮）。根因不是谁写错一行，而是**没有一张清单**。

> ★ **单卡实跑待办**（owner 2026-09-18 的逐卡观察，12 张卡 / 49 条）见 [`SINGLE-CARD-TEST.md`](SINGLE-CARD-TEST.md) —— 那份是**一次装配一张卡**的测试结果，不是本清单的替代；本清单管「一张卡该怎么自检」，那份管「现在哪一条坏了」。

## ★ 新开一张卡：流程 / 容量 / 登记面（owner 2026-10-01 起开**新非系统卡**）

> **口径**：本文 §0–§3 管「一张卡该**怎么写**」；这一节管「**开卡这件事本身**」（id / 图 / 表 /
> 键位 / 文案 / 数值 / 登记 / 闸门）。容量数字**不许手抄**——`python3 native/tools/capcheck.py`
> 每次现算（`make check` 的一环），下表只是 2026-10-01 的快照。

### ★ 容量账（现算；来源 = [`CAPACITY.md`](CAPACITY.md) §1.0 + `capcheck.py`）

| 墙 | 每张卡吃多少 | 现在 | 折算还能进 |
| --- | --- | --- | --- |
| **`abcard.anm` entry（卡面图）** | **2 条/张**（`_max` + `_min`）| **200 / 256**（`CAPACITY.md` 附录 A0 现算；2026-10-01 搬 81–85 后）| ★ **28 张（瓶颈）** |
| `ability.anm` entry | 3 条/张（保守上界）| 91 / 256 | 65 |
| 卡 id 空间 / `owned[]` / 影子数组 | 1 个 id/张（**从最小的空 id 起连续分配**）| — | 160 |
| 图鉴 / 编成条目数 | 1 格/张 | 36 / 197 | 160 |
| 商店随机池 | `weight + 5` /张 | — | 1261 |
| 音效 cfg / wav 名 / 站点 | — | 87/116 · 75/104 · **33/24（超记账线）** | 都不是瓶颈，但**站点余量为 0** |

⇒ **本次新卡批次的安全容量 = 33 张**。要更多 ⇒ `CAPACITY.md` §1.0 列了两条扩容路：
① 大小图并进**一个** entry（一 entry 两 sprite）⇒ 69 → **138 张**；② 拆到别的空闲 anm 槽。

### ★ id 分配纪律（`CAPACITY.md` §1.0 定；**跳号 = 白扔余量**）

`owned[]` / `unlocked_cards` / 影子数组的用量口径 = **最高在用 id + 1** ⇒ **新卡从最小的空 id 起连续分配**，
不要从「最高在用 id + 1」往上贴（id 200 的测试卡是唯一例外，用完即撤）。

**现在的空 id 段**：`1–57` · `81–105` · `119–199` · `201–254`。
★ **已定案（owner 2026-10-01）**：「**非系统卡跟着 80 走，从 81 开始**」⇒ 本批 **从 81 起连续分配**，
**不碰 118 / 119**（那两号按 owner 2026-09-28 口径保持空号）。
`CAPACITY.md` 里「从 119 起」那句是默认纪律，本批按 owner 口径覆盖（两条都低、都连续 ⇒ 都不白扔余量）。

★ **upstream 同步（2026-10-01 查）**：`upstream/main` 与我们**卡集合完全一致**
（36 张同名同 id；只多出我们自己的 `200` 测试卡），卡实现文件与卡面图**无缺**；
**feature 分支按 owner 口径不动**（⚠ 仅记录：`upstream/stg-agent-proto` 上有 81–85 五张原型卡
「黑莲花 / 阳光戒指 / 火球术 / 向日葵 / 金盏花」—— 我们**不使用**，但**占号在那边**，将来若要对齐先看那条分支）。

### ★ 开卡七步（每步的产物 + 登记面）

| # | 做什么 | 产物 / 落点 | 登记面 |
| :-: | --- | --- | --- |
| 1 | **定 id** | 见上面的分配纪律与建议段 | — |
| 2 | **卡面图** | `assets/cards/<NAME>_max.png`（**256×320**）+ `_min.png`（源 64×80，构建期垫成 64×128）→ append 到 `assets/cards/ORDER.txt` | 构建期硬门 `build_abcard.py` 的 `MAX_ENTRIES`（超了当场红）|
| 3 | **卡表行** | `patch/th18/cards.js`：`name` / `desc` / `category` / `internal_name` / `initial_unlocked` / `price_tier` / `weight` / `sprite_large` / `sprite_small` | `make cards-check`（desc ≤6 行 · 每行 <64 B · 无 ASCII `%`；`category` 必须与行为一致）|
| 4 | **行为** | `native/cards/<name>.c`（+ `<name>_core.{c,h}` 纯逻辑 + `native/tests/test_<name>_core.c`）；用 `CE_CARD(id, …)` 注册 | 本文 §0–§3（四条铁律 + 六个生命周期钩子 + 删卡收尾）|
| 5 | **键位** | 要按键先查 [`CARDS.md`](CARDS.md) 的**「键位总表（全部卡）」**；组合键按 `WORK-PACKAGES.md` §1c 登记 | 写完**更新那张总表**（它就是仓里的组合键登记表）|
| 6 | **文案两层** | 玩家面向 = `cards.js` 的 `desc`（**玩家心智模型**）；机器面向 = `CARDS.md` 一行 + 数值出处 | owner 2026-10-01e 口径；`❓ 自定` 的数值进 [`TUNING.md`](TUNING.md) |
| 7 | **闸门与发布** | `make check` **rc=0**（含 `capcheck` / `cards-check` / `anm-verify` / `check-docs`）| 动共享件或别人的卡 ⇒ [`AUDIT.md`](AUDIT.md) §BE 登记；发布面（DLL / `ability.anm` / `pl0X.sht` / `patch/**`）**必须同批** |

**`category` 速查**：`0` = 主动卡（`active_recharge` + `on_activate`，走 `D` 选中 + `C` 发动）·
`1` = 装备区卡 · `2` = 被动 · `3` = 购买时 / 即时卡（如强欲之壶、方片 2、倒吊人）。

## 0. 四条铁律（先看这四句）

1. **UI 与动画行为先从原代码找**（脚本号 / 地址 / 数值 / 触发条件），找不到标 `❓ 自定` 并登记。
2. **长持续效果必须交代「被删掉会怎样」**：跨帧留下东西的卡，全部要在 `on_delete` 里收干净 ——
   删卡的路都发生在**商店里**，那一刻卡的所有 tick 回调都停着。
3. **生成可收集道具前必须过两问**（2026-09-10 用户口径，踩过多次）：
   - **会不会出生在自机吸收圈里？** TH18 环收 = `th18:0x4465da`：自机到道具的距离² 比
     `player+0x47990`（聚焦）/`+0x47994`（非聚焦）²，进了就 state 翻 4 开始吸附 ⇒
     **出生在圈里 = 一出生就飞走、动画都看不见**。⚠️ 半径单位是 **ECL 单位**（默认 70；道具回收范围卡
     （21）抬到 110），而 TH17 的鱼灵换道具基座只有 `y−80` ⇒ 有卡 21 时就落进圈里了。
     **先量、再决定**：`animal.c` 的 `animal_in_pickup_ring(pos)` 是现成的判据（照 `player+0x47990/94`）。
     ⚠️ 修法**不是**把每颗都推到圈外 —— 逐颗推会让整组贴到圈边上（2026-09-10 实跑：用户看到
     「椭圆没填满、而且大了」）。要挪就**整组一起挪**，或者干脆靠第②问的无敌时间顶着。
   - **有没有无敌时间？** `spawn` 第 6 参 = **intro 倒计时**（`item+0xc84`，`th18:0x445b78` 的
     `jg 0x4467e5` 把 state-1 整段跳过）⇒ 那段时间既不动也不能收。TH17 鱼灵换道具传的是
     `10,12,…,68`，照抄。

4. **不许碰 x87**（`make dllx87` 门）：i686 上 `float` 返回值（`st(0)`）与 `float/double ↔ int` 转换
   都会生成 x87 指令。core 尽量整数化（千分比/百万分比存 `int32`），必须返回浮点时写成头里的
   `static inline float …` 或 out 参数。

## 1. 六个生命周期钩子：谁在哪一刻被调、该负责什么

| 钩子（虚表槽） | 什么时候被调 | 该负责 |
| --- | --- | --- |
| `ctor`（`+0x00`） | `allocate_new_card`（开局重建卡组 / 商店买到 / 空白卡发卡 / replay 复原）| 初始化；**即时卡**在这里做完就 `return 1`（引擎当场删自己）|
| `on_load`（`+0x20`） | **关卡开场**（`notify_cards_on_load`，`Player::reset` 之后）。⚠️ 不是「开局一次」，每关结束的商店/结算里也会跑 | 写「每关都要重设」的自机参数（例：黑桃 Q 的道具半径）|
| `on_stage_start`（`+0x34`） | 每关开始（含换关）| 收掉**跨关不该留**的东西（例：青眼白龙的龙、绀珠传的面板）|
| `on_tick` / `on_tick_2` / `on_active_tick` | 游玩帧。**商店 / 菜单 / 暂停里不跑**（`ABILITY_SHOP_PTR` 等门）| 每帧行为；**不能**把清理托付给它们 |
| `on_run_reset`（`+0x4c`） | 局末（新一局会用新对象）| 清局内累计状态 + 还原全局旋钮（SDK 会先做 `ce_dmg_set(id, 0)`）|
| **`on_delete`（`+0x50`）** | **对象没了**：倒吊人删卡、原版空白卡弃整副牌、`reset_cards`、replay 复原 | **收干净所有跨帧资源**（见 §3）。SDK 的 `opdelete` 桩顺序 = `ce_dmg_set(id,0)` → `on_delete` → `ce_state_free`，所以 `on_delete` 里 `ce_state` 还活着 |

> ⚠️ **`on_run_reset` 不等于 `on_delete`**：前者是「这一局结束了」，后者是「这个对象没了」。
> 两条路都要能收干净 —— 能共用就共用同一个函数（季节卡 / 动物灵卡的 `clear_card()` 就是这种写法）。

## 2. 删卡的三条路（都要求 `on_delete` 能收）

| 路径 | 机制 | 一手指向 |
| --- | --- | --- |
| 倒吊人（card-expand id 74） | **商店一次性购买**的即时卡：`ctor` 里 `ce_destroy_card(victim)` → `[vt+0x50](victim,1)` | [`native/cards/hanged.c`](native/cards/hanged.c)、[`SDK.md`](SDK.md) §W |
| **原版空白卡**（id 0 / `CardChimata`） | 商店 state 3 确认 → `AbilityManager__reset_cards(mgr, 0)` = **整副卡组逐张 `vtable->operator_delete(1)`** + 免费发本次商店的非消耗型待售卡 + `MONEY = 0` | [`engine/card/th18/05-shop-and-money.md`](../../../engine/card/th18/05-shop-and-money.md) §7、[`02-lifecycle.md`](../../../engine/card/th18/02-lifecycle.md) §6 |
| 局末 / replay 复原 | `on_run_reset`（`+0x4c`）→ 之后才是对象销毁 | [`SDK.md`](SDK.md) §2 |

**共同点：全部在商店里发生**，而商店期间 `AbilityManager__on_tick__stub` `0x408a90` 开头
`ABILITY_SHOP_PTR != 0` 就跳过本帧 ⇒ 卡的 `on_tick_2` / `on_active_tick` 停摆、`on_load` 不会再来。
**只有 `on_delete` 跑得到。**

## 3. 资源清单：写完对着过一遍

| 资源 | 谁申请 | 谁必须收 | 不收的后果 |
| --- | --- | --- | --- |
| 自建 VM（`ce_anm_spawn` / `_ui`）| 卡自己 | `on_delete`（`ce_anm_delete` / `interrupt`）| 脚本多半是 `stop()`（不自己 delete）⇒ **永久留在屏幕上** |
| 子机槽（`ce_option_alloc` / `ce_option_claim`）| 卡自己 | `on_delete` → `ce_option_drop`（顺带收激光）| 12 格池**没有别的回收者**，每漏一次少一格（`option allocated FAILED (pool full)`）|
| HUD 数字（`ce_hud_digit_*`）| 卡自己 | `on_delete` → `ce_hud_digit_clear` | 数字是**独立 VM**，删卡时的 HUD 重建带不走它 ⇒ 屏幕角落一串孤儿数字 |
| HUD/SDK 文字 VM（`ce_hud_vm_text_*` / `ce_hud_number_*`）| 卡自己 | `on_delete` → `_clear` | 同上（面板 / 读数留在屏上）|
| 伤害源（`ce_damage_rect` / `create_damage_source`）| 卡自己 | 一般自带寿命（帧数），**活过卡的要显式收** | 卡没了伤害还在打 |
| 全局旋钮（自机道具半径、移速倍率、道具减速因子…）| 卡自己 | 每帧重写 / SDK 按卡聚合（删卡即退出）/ `on_delete` 里还原 | 效果残留整局（换关可能被 `Player::reset` 覆盖，别指望）|
| 文件级 `static` 缓存 / 共享池索引 | 卡自己 | `on_delete` 里清空 | 空白卡连删整副牌时串味（不能假设「只有我一张」）|

## 4. 自检清单（每张新卡动手前打勾）

- [ ] 这张卡会不会跨帧留下东西？（对着 §3 的表逐类点名，没有就明确写「无」）
- [ ] 写了 `on_delete` 吗？它和 `on_run_reset` 用的是同一套清理吗？
- [ ] 清理**完全不依赖 tick** 吗？（商店里没有下一帧）
- [ ] **连续删多张**（原版空白卡那条路）时还成立吗？
- [ ] 即时卡（`ctor` 里干活）有没有顺手动别人的资源？动完是否已收干净？
- [ ] 全局旋钮：每帧重写 / 按卡聚合 / `on_delete` 还原，三选一交代清楚了吗？
- [ ] `NEXT.md` 的实跑清单里写了「买倒吊人删掉它」的验收步骤吗？
- [ ] `check-docs.py` 过了吗？（地址反引号、版本声明、表格 ≤200 字符）

## 5. 实跑验收模板（照抄到 `NEXT.md` 对应轮次）

```
1. 开局带这张卡、开出它的长持续效果（召唤 / 面板 / 子机 / HUD 读数）
2. 进商店 → 买倒吊人，让它删掉这张卡
   期望：日志 <card>: dismissed (card destroyed)；屏幕上不留任何该卡的 VM；HUD 不残数字
3. 再买一次同一张卡 → 效果能正常重开（说明子机槽 / VM 池没漏）
4. 另跑一局：拿到原版空白卡并用掉（弃整副牌）
   期望：同上的清理全部生效，且新发的卡能正常工作
```
