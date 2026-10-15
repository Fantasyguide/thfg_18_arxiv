# CARDS —— 已实装的新卡一览

> **版本**：TH18 v1.00a（`th18.exe`，imagebase `0x400000`）。本文裸地址默认属该版本；引用其他版本须写成 `th16:0x…`。
> 人看的目录。**数据的真相在 `patch/th18/cards.js`，行为的真相在 `native/cards/*.c`**；本表每加一张卡补一行。
> id 58–254 可用（≤ 197 张；实际先撞 `abcard.anm` 的 256 entry，见 [`CAPACITY.md`](CAPACITY.md) §5-①）。状态：✅ 实跑通过 / 🔧 待实跑 / 💡 设计中。

> ★★ **文案的两层分工（owner 2026-10-01e 口径）**：
> 「比如**115 切波规则**，这是**机器得规定死**，但是**对于我们东方玩家来讲就得是这样的东西**」——
> · **玩家面向**（`patch/th18/cards.js` 的 `desc`，商店/图鉴里显示的那几句）写**玩家的心智模型**：
>   115 就写「一个非符 + 一张符卡」切成一波（玩家在游戏里看到的那一版），不写引擎内部的判据；
> · **机器面向**（本表 + `engine/**` 文档）必须写**精确规则**：115 的一手是「**一段攻击 = 一波**」
>   （`trance` 那套 `phase` 同源的分章判据），两者**不是同一句话**，别互相搬。
> ⇒ 描述与实现脱节时：**本表改成现实现状**，**cards.js 改成玩家能对得上的说法**。

## 键位总表（**全部卡**；owner 2026-10-01e：「可能主要是关于哪些键是干什么的」）

> 引擎本身的键（每张卡上都照常）：`Z` 射击 / `X` 炸弹 / `Shift` 低速（聚焦）/ `C` 发动**选中的主动卡** /
> `D` 切换选中卡 / `Esc` 暂停。**卡侧只读引擎掩码里的位**（`0x001` Z · `0x002` X · `0x008` Shift ·
> `0x400` C · `0x800` D）⇒ 天然 replay 安全；**空格系一律是调试键，玩法禁止占用**（owner 硬规矩）。
> 下表 = **每张卡额外占用/改变哪个键**；写「（无）」的卡不读任何键（被动或只借引擎的 C/D 链路）。

| id | 卡 | 额外键 / 按键动作 | 依据 |
| :-: | --- | --- | --- |
| 58–63 · 65 · 68–71 · 73–79 | 黑桃五张 / 方片 2 / 强欲之壶 / 黄昏 / 加倍 / 腐化 / 破损核心 / 黄金小丑 / 倒吊人 / 真银圣剑 / 古锭刀 / 豌豆射手 / 衔尾蛇 / 旋风斩 | **（无）** —— 被动，或只借引擎的「D 选中 + C 发动」 | 代码无键读取 |
| 64 | 反转牌 | `C`（主动卡）| 卡表 `category` |
| 66 | 神之宣告 | `C`（主动卡）| 同上 |
| 67 | 青眼白龙 | `C`（主动卡）| 同上 |
| 72 | 炎魔之王拉格纳罗斯 | `C`（主动卡，消耗 2.00 火力）| 同上 |
| 80 | 股市 | **`Shift` + `C` = 卖出**，单按 `C` = 买入（读出点 `stock.c` 的 `on_activate`）| `CE_INPUT_HELD() & CE_INPUT_FOCUS` |
| 106 | 红魔乡 | **（无）**（`空格` = 满 P 测试键）| 被动卡 |
| 107 | 妖妖梦 | **按 `X` 放炸弹** ⇒ 结界中以灵击崩掉结界（未注册 bomb gate ⇒ 炸弹原语义不变）| 一手 `th07` |
| 108 | 永夜抄 | **`Shift`** = 切人侧 / 妖侧；**放 B 中、灵界中、狼灵与大鹫灵咆哮中不切侧** | owner 2026-10-01e 口径 |
| 109 | 花映塚 × 獣王園 | **按住 `C`** = 蓄力（同时张结界），松手放攻击。⚠ 另有「Shift 时长也充能」正按 owner 实跑 #1 订正 ⇒ **以 109 那条线最新为准** | `SPEC-109-FUSION.md` |
| 110 | 风神录 | **`D` 切到本卡后按 `C`** = 灵击（火力 ≥2.00 档）；**没有炸弹残弹时按 `X` 同效** | 一手 `th10` |
| 111 | 地灵殿 | 同上（`C`；无残弹时 `X` 同效）；槽满自动全收道具**不占键** | 一手 `th11` |
| 112 | 星莲船 | **（无）**（`空格` / `Shift+空格` / `Ctrl+空格` = 令牌调试）| — |
| 113 | 神灵庙 | **`D` 切到本卡后按 `C`** = 发动灵界（槽满才可）；中弹时自动触发救援 | 一手 `th13` |
| 114 · 115 | 辉针城 · 绀珠传 | **（无）** | 被动卡 |
| 116 | 天空璋 | **双击 `Z`** = 切副季节；**`D` 切到本卡后按 `C`** = 季节解放。⚠ `S` 键 / `Shift+C` / `Shift+D` 都是**历史**（2026-09-29 定案：双击 Z）| owner 口径 |
| 117 | 鬼形兽 | **槽满即自动起咆哮**（不按键）；**按 `X`** 把咆哮用成 40 帧霊撃。`空格` / `Ctrl+空格` = 调试 | owner 2026-09-11 定案 |
| 118 | 挑战・十二合一 | **`D` 切到本卡后按 `C`** = 发动 | 卡表 `category` |
| 86 · 87 · 88 · 89 · 90 · 91 · 92 | 东尼的悲恸 · 天顶剑 · 杰拉 · 星星炮 · 冰淇淋 · 鞘翅重锤 · 伟大之作 | 91 = **主动，且自己要读两次 C**（发动一次、时机条确认一次；`INPUT_PRESSED` bit `0x400`）；其余同左 | `elytra_mace.c` 读 `0x4ca434` 的 `0x400` 位 |

> ★ **这张表就是仓里欠的「组合键登记表」**（`WORK-PACKAGES.md` §1c 风险 #3 一直挂着没建）：
> 新卡要占用某个键/组合键前，**先在这里查**，撞了就改设计或登记例外；改完顺手更新本表。

## ★ 号位预留（owner 本人正在做的卡；**别人不要占**）

| id | 名字 | category | 状态 |
| :-: | --- | :-: | --- |
| ~~86~~ | 东尼的悲恸 | 2（被动）| ✅ **2026-10-02 落地**（统筹代做）—— 见「其它」段的 86 行；卡图 = owner 提供的《杀戮尖塔》「送死亡」整卡（裁出卡体矩形后铺满，见「其它」段 86 行） |
| ~~87~~ | 天顶剑 | 1（装备区）| ✅ **2026-10-02 落地** —— 见「装备（子机）」段的 87 行；卡面 = 白底 + Terraria 原版天顶剑物品 |

> **号位不再预留**（两条都已实装）。预留台账原文见 [`HANDOFF-CROSSCARD-STATE.md`](HANDOFF-CROSSCARD-STATE.md) §11.4b。

## 黑桃（原型：德州扑克・皇家同花顺）

| id | 名字 | 效果 | 实现（槽 / 事件） | 文件 | 状态 |
| --- | --- | --- | --- | --- | --- |
| 58 | 黑桃 10 | 从道具获得的金钱 +10％（每第 10 个金钱道具多给 1；确定性计数，replay 安全）| `on_item_money` | `s10.c` | 🔧 |
| 59 | 黑桃 J | 移动速度 +10％ | `on_tick_2` 写移速倍率 `player+0x477ec` | `sj.c` | 🔧 |
| 60 | 黑桃 Q | 道具自动回收范围略微增加（吸引半径 70 → 95、吸速 5 → 7；Nitori 是 110）| `on_load` 写玩家回收四参（抄 Nitori）| `sq.c` | 🔧 |
| 61 | 黑桃 K | 自机弹伤害 +10％ | 走 SDK 的自机弹伤害层（`.bullet_damage_permille = 1100`，AUDIT §AA；2026-09-08 从自己写 `bullet+0x9c` 改过来，顺手补上了漏抬的每帧伤害上限）| `sk.c` | 🔧 |
| 62 | 黑桃 A | Miss 后的无敌时间 +50％（280 → 420 帧）| `on_tick_2` 识别复活计时器 {279,280} | `sa.c` | 🔧 |

**卡图**：五张用标准英式牌面（Wikimedia Commons，Dmitry Fomin，CC0；`assets/cards/_src/english_pattern/`，`fit_card.py` 出图），sprite 118/119 … 126/127。

🔧 **皇家同花顺**（`cards/royal.c`，五张共用 `.ctor`）：买到第五张黑桃时触发一次——金钱 +800、残机 +2（上限先 +1 钳 7，照 CardLife）、bomb +2（同法照 CardBomb）。判定：ctor 时其余四张已在 `owned[]` 且自己尚未 owned（新获得；每关开始的重调不算）。奖励金钱 888。演出（`ability.anm` script70 父脚本）：五张黑桃卡图每 10 帧一张在场地中央排开 → 60 帧 trophy 音效（0x4f）+ 金色「ROYAL FLUSH」横幅弹出（拉格泰姆钢琴 `ROYAL_RAGTIME` 从第 0 帧起铺整段，帧 60 正好是它第二小节的强拍） → 74 帧「+888 GOLD」→ 170 帧起一起淡出上浮、194 帧消失。AUDIT O26。

## 方片

| id | 名字 | 效果 | 实现（槽 / 事件） | 文件 | 状态 |
| --- | --- | --- | --- | --- | --- |
| 65 | 方片 2 | 商店一次性购买：买下后金钱翻倍（先扣购买价再翻倍；钱不够用火力补差价时结果为 0）；本身不进卡组 | `ctor` 里 `MONEY = 2·M − price`（ctor 先于扣款），`MONEY_TOTAL += 增量`，返回 1 当场销毁 | `d2.c` | 🔧 |

## 致敬・游戏王

| id | 名字 | 效果 | 实现（槽 / 事件） | 文件 | 状态 |
| --- | --- | --- | --- | --- | --- |
| 63 | 强欲之壶 | 购买时立刻获得两张随机卡牌（商店随机池的规则：未拥有、本关可用、按权重）；本身不进卡组 | `ctor` 里 `pick_weighted_random_offer` ×2 → `allocate_new_card(mode 2)`，返回 1 当场销毁（即时卡）。卡图 `POT_OF_GREED`（sprite 132/133，用户原创）| `pot.c` | 🔧 |
| 66 | 神之宣告 | 主动（C 键）：消耗一半残机（向上取整），让 boss 当前符卡立刻按超时结束（无奖励、失败演出）。不在符卡中 / 已超时 / 无残机时拒绝发动（无效音、充能不消耗）。充能 60 s。卡图与演出见下 | `on_activate`：写 boss 中断槽计时到阈值 + 清符卡奖励位；`ce_gui_update_lives` 刷 HUD；拒绝 = `CE_ACTIVATE_REFUSED` | `judgment.c` | 🔧 |
| 67 | 青眼白龙 | 主动（C 键）：献祭 1 残机召唤跟随自机的龙；1500 点生命替玩家挡子弹（每发 −1，弹变点道具）；每 5 秒向上一道光束；生命归零死亡，过关消失。残机 0 拒绝。充能 10 s | `on_activate` 扣命 + 起龙 ANM；`on_active_tick` 跟随（Tenshi lerp）+ `ce_cancel_radius(max = hp)` + `ce_damage_rect`；`on_stage_start`/`on_run_reset` 删 VM | `blue_eyes.c` + `blue_eyes_core.c` | 🔧 |

**青眼白龙（67）补充**：光束 = 45 帧 × 每帧请求 100 的矩形伤害源（2026-09-04 平衡：HP 2500 → 1500、光束 30 → 45 帧）（宽 32、从龙口到区域顶边），2026-09-06 起光束帧把每敌每帧伤害上限 `player+0x47984` 抬到 300（炎魔同款 `ce_raise_damage_cap`，AUDIT V20 / O29e′），一波实打 4500；此前不抬时自机开火会把配额吃满、光束几乎白打；光束不消弹、激光不挡。跟随目标自机上方 80 px（要石同款 lerp 0.04）+ 上下呼吸浮动 ±5 px / 96 帧（`be_bob_dy`，画面 / 判定 / 光束起点 / 血条共用；2026-09-06）、挡弹半径 48、有命中那帧龙染蓝 `0xff0080ff`（要石同款；D3DCOLOR ARGB）；龙下方 66 px 有血条（两个根 VM `drawRect(1,1)`，C 每帧写 `vm+0x54` scale = (56 × hp/1500, 4)，>50％ 蓝 / >20％ 黄 / 红；原生 HUD 充能条同款做法）。卡图（用户原创立绘，`fit_card.py`，sprite 136/137）；场上的龙 = 用户原创俯视图抠黑底（`assets/ability/make_blue_eyes_art.py`，ability sprite 118，script78 缩放 0.45）；光束照魔理沙 Master Spark（原生 `pl01b`/`pl01b2` 贴图 119/120，script79–85：白核 + 四层彩 + 彩光），判定宽 96。设计 `docs/superpowers/specs/2026-09-04-blue-eyes-design.md`，AUDIT O29。

**神之宣告（66）补充**：卡图 `JUDGMENT`（sprite 134/135，用户原创）。发动同时全屏消弹（弹幕 → 点道具 + 激光，引擎 `cancel_all`，O28h）。演出：发动音 0x4d（反转牌同款）+ `ability.anm` script77——卡图副本铺满弹幕区半透明浮现（alpha 140，scale 1.25 → 1.45）、75 帧缓缓放大并上浮 24 px、45 帧后 30 帧淡出。限制：耐久符卡的超时脚本自带掉落照旧（AUDIT O28b）。

## 致敬・UNO

| id | 名字 | 效果 | 实现（槽 / 事件） | 文件 | 状态 |
| --- | --- | --- | --- | --- | --- |
| 64 | 反转牌 | 主动（C 键）：场上所有子弹速度方向反向；充能 60 s。卡图 UNO 反转牌（`fit_card.py`，sprite 128/129）| 第一张主动卡：`active_recharge = 3600`，`on_activate` 扫子弹池翻 `velocity` 与 `angle`（不动激光），并 `ce_anm_spawn` 起 `ability.anm` script68 亮牌（卡图副本绕 Y 轴一圈）| `reverse.c` | 🔧 |

## 致敬・Balatro

| id | 名字 | 效果 | 实现（槽 / 事件） | 文件 | 状态 |
| --- | --- | --- | --- | --- | --- |
| 68 | 黄昏 | 被动：用掉**最后一颗**炸弹时，那一发结束后自动再放一发 | `on_tick_2` 盯炸弹管理器 `[0x4cf2b8]+0x30` 的两个边沿：0→1 时若 `CURRENT_BOMBS == 0` 就武装，1→0 时调引擎自己的 `do_bomb()` `0x420360`。不开断点、不碰炸弹计数（`0x4574d0` 自带钳 0）。AUDIT §R | `dusk.c` | 🔧 |
| 73 | 黄金小丑 | 被动：每关过关、进商店那一刻金钱 +20％（向下取整，无上限）| 新 SDK 事件 `on_shop_open`（不开断点：`ce_shop_reopen` 的 MSG 开店分支里分派；重开的第二家不算）；`ce_interest` 主机单测。AUDIT §W | `golden.c` | 🔧 |
| 74 | 倒吊人 | 商店一次性购买：随机摧毁卡组里的另一张卡，获得 300 金钱（固定：摧毁垃圾牌赚、摧毁关键牌亏）；本身不进卡组 | `ctor` 里 `ce_pick_random_card`（游戏 RNG，排除当前选中的主动卡）→ `ce_destroy_card`（照 recount `0x4080e0` 的删卡分支 + 引擎自己重画 HUD）→ 金钱 +300、返回 1 当场销毁。没有可删的卡就什么都不给。AUDIT §W | `hanged.c` | 🔧 |

**黄昏 = Dusk**（最后一手重触发 → 最后一颗炸弹再放一发）。倒吊人的删卡原语 `ce_destroy_card` 是本 mod 第一个局中改卡组的动作，
Mr. Bones / Gros Michel 这类「自毁」卡以后都走它。

## 致敬・神话

| id | 名字 | 效果 | 实现（槽 / 事件） | 文件 | 状态 |
| --- | --- | --- | --- | --- | --- |
| 78 | 衔尾蛇 | 被动：每 miss **X** 次，自机弹伤害**永久** +20％（加法），然后 X += 1；X 初值 1。⇒ 第 N 层要累计 N(N+1)/2 次 miss（1/3/6/10/15…）。HUD 小卡上画当前层数 | `on_death_after_deathbomb`（+0x0c，加倍同款「真的 miss」信号）计数 → 报给 SDK 的自机弹伤害层（AUDIT §AA）；`on_hud_anm` 复用真银圣剑的数字脚本。AUDIT §AB | `ouroboros.c` + `ouroboros_core.c` | 🔧 |

**衔尾蛇（78）补充**：咬住自己尾巴的蛇——每一次死亡喂大下一次的自己，但下一口越来越难吃到。
「永久」= 一整局之内（局末 `on_run_reset` 清零）。残机有限 ⇒ 现实里到 2–3 层、极限 4 层，自限，不用另设上限
（代码里 `OB_MAX_STACKS = 9` 只是给 HUD 那一位数字兜底）。与**加倍（69）反协同**：那张让每次 miss 掉 2 命。
伤害与每帧伤害上限都归伤害层，这张卡一行都不写；纯计数、无随机无浮点 ⇒ replay 天然安全。
卡图程序生成（`assets/cards/make_ouroboros_art.py`：极坐标距离场画一条渐细的蛇盘成环，鳞片与明暗都是角度的函数；sprite 158/159）。

## 其它

| id | 名字 | 效果 | 实现（槽 / 事件） | 文件 | 状态 |
| --- | --- | --- | --- | --- | --- |
| 69 | 加倍 | 被动：Miss 损失 **2** 条残机；敌人掉落的道具全部 **×2** | 掉命挂 `on_death_after_deathbomb`（引擎扣命**之前**多扣 1，`return 0` 不救命）；掉落走新 SDK 事件 `on_enemy_drop_pre`（断点 `ce_enemy_drop` @ `0x430510` 入口，撒之前把敌人 `+0x04` 起的 20 个掉落数翻倍）。AUDIT §S | `double.c` | 🔧 |
| 114 | 辉针城 | 被动：TH14 Collect Point 1:1 复刻 —— 全屏收点（V 门真空飞行的 P/钱）按批结算（批闭 = 收点流真停后 ~8 帧，presence 续批）；批 ≥20 按档位 ×0.5/0.8/1.2/1.6/2.0（只作用分数，低档负补差）+ 每批 1 枚碎片（4:1 循环 / ≥60 命碎）；被动二：非聚焦射击命中掉 P 道具（按火力节流）| 四个新 SDK 事件 `on_item_frame`/`on_item_collect`/`on_item_commit`/`on_shot_damage`（断点明细见 MAP §Z 表）：collect/commit 夹引擎分值加法器、stash 差量入批（成员门 = state 3，Z22）；frame 判无 state-3 在飞 → 关批结算 + 碎片 spawn。AUDIT §Z | `collect2x.c` + `collect2x_core.c` | ✅ 实跑通过（2026-09-09，Z22 成员门回归）|
| 80 | 股市 | 主动（C 键）：选中时 **C 买入一股**、**Shift + C 卖出一股**（一股价钱 = 现价，随行情浮动）。股价按**隐藏参数模型**（动量 + 波动率聚集）每 15 帧走一步、4 秒一根 K 线，HUD 右上角常驻 K 线与现价，金钱下方显示持仓与盈亏，自机头顶显示一股的价钱（见下「补充」）。持仓跨关、商店只认现金、局末清零。充能 30 帧（节流） | `on_activate` 读 `INPUT_HELD` 低速位分买卖、写 `MONEY`；`on_tick_2` 走 `stock_step` + 34 个 UI 层 VM；`on_draw` 三处文字（市场 / 仓位 / 自机头顶）；`on_delete` 删 VM。纯逻辑 `stock_core.c`（主机单测） | `cards/stock.c` + `stock_core.c/.h` | 🔧 待实跑（AUDIT §AF）|
| 86 | 东尼的悲恸 | **致敬・杀戮尖塔2（社区梗）**。被动：**16 秒（960 帧）内收下一张符卡** ⇒ 紧接着的那个**非符**段开场血量就是 **1**（一枪即死；`maximum` 不动，所以血条如实见底）| **致敬・杀戮尖塔2（社区梗）**。被动：**16 秒（960 帧）内收下一张符卡** ⇒ 紧接着的非符段开场血量就是 **1**；触发时 BOSS 身上浮**卡面虚影** + 一声 `se_wolf`（voice 0x62）。⚠ **00.00 结束的符卡（耐久符）不算收下**| `tony_grief.c` + `tony_grief_core.c`（主机单测，复用 76 的槽原语）| 🔧 |


**股市（80）补充**：显示分三处 —— 右上角面板（16 根 K 线 + **买入均价横线**，蜡烛在线上方即盈利）与其下的现价；金钱正下方两行（股数 × 持仓总价、本局盈亏，**红涨绿跌**，空仓不画）；**自机头顶**一个数字 = 一股的价钱，格式与侧栏一致；颜色即答案（红 = 高于均价该卖、绿 = 低于均价该买、白 = 空仓）。
`on_tick_2` 每 15 帧 `stock_step()` 走一步（自带 LCG），34 个 UI 层 VM（1 底板 + 16 × {实体, 影线} + 1 回本线）用 `stock_rect` script104 就地改 pos/scale/color；`on_delete`（本次新加的 `+0x50` 回调）负责把 VM 删掉。纯逻辑层留了 `stock_shock()` 给以后的事件联动，本版没有调用者。

**加倍（69）补充**（2026-09-07 修）：实跑退回「小怪掉落没加倍」。根因：杂兵的掉落是 `dropMain` 给的**主掉落**（`zEnemyDrop+0x00`，一颗），引擎在调 `0x430510` 之前就撒了；
断点只翻了 `+0x04` 的附加表（`dropExtra`，基本只有 boss / 中型敌人用）。修：断点里读主掉落类型（`ce_drop_main`），往附加表对应格 +1，引擎的附加循环再撒一颗。
**2026-09-12 再改**：这条补偿搬进 SDK 的掉落乘法层（`on_load` 里只 `ce_drop_mult_set(69, 2000)` 一句，
EVENTS-CONTRACT §2）⇒ 与卡链顺序无关、别的卡加到的那颗也一起翻倍（`cards/double_core.c` 退役，AUDIT S10）。

**辉针城（114）补充**：数值行（spec §3，`collect2x_core.h` 同名常量）：批集 = V 门真空飞行（state 3）的 {1(P)、2(钱)}（触收 1 / 环收 4 / 转换点 9–0xe 一律不入批，AUDIT §Z19/Z22）；档位 20/30/40/50/60 → 倍率 500/800/1200/1600/2000 permille，倍率**只作用分数**：`SCORE += batch_score×(mult−1000)/1000`（×0.5/×0.8 为负补差，1:1 原义；`ce_add_score_delta` 钳 0..9 亿）；碎片每批 1 枚，<60 走 `fragment_cycle` 4 符卡碎 : 1 命碎（循环按 run 计，`on_run_reset` 归零）、≥60 命碎不动游标。
批分值捕获（stash）：collect 断点在 type 分派**前**存 SCORE、commit 断点（分值已入 SCORE）累加差量；stash 只对批集收集置位、任意提交后失效 —— 批集道具按池槽顺序「收集→提交」背靠背走完，夹在中间的碎片提交见不到活 stash。帧源 = 接线层自计数（`on_item_frame` 分派每正常游玩帧一次；`card+0x20` zTimer 只被主动卡 tick、被动卡读它是死值，Z14），节流游标同源，弃用 TIME_IN_STAGE。
早结敞口（Z7，Task 9 实跑定案）：批闭判定 = 池内无 state-3 在飞批集道具（成员门 Z22 + 8 帧 idle/presence 续批 Z21），若存在「收集了、分值提交滞后到批闭之后」的道具，那一枚分值会漏出本批（日志会现 `commit after batch closed` 行）。
卡图：**2026-09-18 落地** —— 白底 + 辉针城 exe 图标最大居中，sprite **180/181**（`COLLECT_2X`；此前暂填 116/117 的透明占位，做法同黑桃五张最初那版，DATA.md §6）。`price_tier` 10（300 档）、`weight` 2 —— 占位，待用户确认。设计 `docs/superpowers/specs/2026-09-08-collect-2x-design.md`。

**东尼的悲恸（86）补充**（2026-10-02，统筹代做）：规则逐字 = owner 2026-10-01「若用时 16s 之内成功收取一张符卡，则下一个非符血量降为 1」。
两侧判据都是引擎自己的口径（一手在 [`tony_grief_core.h`](native/cards/tony_grief_core.h) 文件头逐条给了地址）：
**收取** = `Spellcard__end` `0x42a780` 里 bit1（奖励存活）在 bit0 清掉时仍置位（`0x42a7c2` 先清 bit0、`0x42a7ed` 才测 bit1，全程不写 bit1）；
**超时**靠 bit7 排除（`0x42efc0` 置位 —— 超时但 elapsed < 60 帧时 bit1 **保留**，只看 bit1 会误判）；
**用时** = `SPELLCARD+0x24`（宣言清零、每帧 +1，只有这两个写点）`≤ 960`。
**非符** = 段边界（活动中断槽 `(槽名, hp, time, 名字前 16 字节)` 签名变化，算式同族于 `animal.c`/`lolk.c`，名字取 16 字节而不是 4 ⇒ `Boss2`/`BossCard3` 分得开）处 bit0 = 0；**死亡段**（槽名含 `Dead`，复用古锭刀 76 的 `gd_slot_is_final`）不写，免得一枪跳过死亡演出。
落笔只写 `zEnemy+0x6220`（非符段的权威血量；`maximum` 不动 ⇒ 血条见底），段内每帧幂等补写、签名一变立刻停。
卡图 = **owner 2026-10-02 提供**（《杀戮尖塔》「送死亡」整卡）：原件 `_src/TONY_GRIEF.png`，按 alpha 遮盖取**卡体矩形** `(72,53,771,1042)` = 699×989 裁出 `_src/TONY_GRIEF_CARD.png`，再 `fit_card.py TONY_GRIEF … --no-detect --trim 0 --margin 0 --fill` 铺满 256×320 + 套原生框（sprite 200/201 不变）；裁法与出处见 `assets/cards/_src/README.md`；`price_tier 9 / weight 2 / initial_unlocked 1` 是占位值（进 `TUNING.md`）。
⚠ 口径待 owner 一句话：**耐久符卡**（bit3）超时在 flag 上与「收下」不可区分（引擎自己也按收下算），本卡按引擎口径**算收下**。

## 致敬・炉石传说

| id | 名字 | 效果 | 实现（槽 / 事件） | 文件 | 状态 |
| --- | --- | --- | --- | --- | --- |
| 72 | 炎魔之王拉格纳罗斯 | 主动（C 键）：消耗 2.00 火力召唤**不跟随自机**的炎魔；800 点生命替玩家挡子弹；每 7 秒滑到弹幕区下 1/3 的随机落点，到位后向场上**随机一个**敌人投火球（飞行物，沿途消弹；落地消弹 r 96 + 爆炸 1200 + 小震屏）；生命归零死亡，过关消失。火力 < 3.00 拒绝。充能 10 s | `on_activate` 照 Tsukasa 三步（门槛 → `spend_power` → `repopulate`）；`on_active_tick` 挡弹 + 状态机 `fl_step` + 火球位姿 + 落地 8 帧 `ce_damage_rect`；随机走 `ce_rand()`（详见下）。AUDIT §V | `firelord.c` + `firelord_core.c` | 🔧 |
| 70 | 腐化 | 被动：拿到时炸弹上限补满 7；此后**放炸弹消耗的是上限而不是当前数**，且过关不回复 —— 一次给满七发，用一发少一发 | `ctor`（`ce_fresh_acquire` 门控）顶满上限；新 SDK 事件 `on_bomb_spent`（断点 `ce_bomb_spent` @ `0x4203bc`，consume_bomb 刚返回）还原当前数、扣上限；`on_stage_start` 抵消引擎的 `min(3, 上限)` 补给。AUDIT §T | `corruption.c` | 🔧 |
| 75 | 真银圣剑 | 装备「武器」：每关前 2 发炸弹是挥砍——附加 400 伤害（盖满弹幕区，范围内每个敌人各吃）并回 1 个残机碎片；耐久每关回满；HUD 小卡右下角画本关剩余次数 | `on_bomb_spent` 扣耐久 + `ce_add_life_fragment`（引擎 `0x457570`）；`on_tick_2` 出 4 帧 × 100 的全场矩形源并抬上限 400；`on_hud_anm`（虚表 `+0x44`，第一次被用）拿图标 id → UI 列表起 script99 数字 VM、`set_sprite` 换数字。AUDIT §X | `truesilver.c` + `truesilver_core.c` | 🔧 |
| 79 | 旋风斩 | 被动：放一发炸弹时**额外再连放 X 发**，X = ⌊当前火力⌋ − 1，代价是 **X 点火力**（额外那几发不花炸弹数）。4.00 火力 = 一次按 B 炸四发，火力落回 1.00 | `on_bomb_spent` 算 X + `ce_spend_power(X × 100)` + `repopulate`；`on_tick_2` 每帧看「还在放吗」，不在放就调引擎自己的 `ce_do_bomb()` 接下一发（黄昏同款机制，只是接 X 发），接完把炸弹数精确补回。AUDIT §AE | `whirlwind.c` + `whirlwind_core.c` | 🔧 |

**炎魔之王（72）补充**：扣火力照原生 `CardTsukasa__c_press` `0x410e60`——门槛「成本 + 一档」（引擎 `spend_power` 永远保留 1.00，
不留一档实际扣的会比名义少）、`spend_power(200)`、无条件 `repopulate_options`（重建子机 + 广播 `on_power_level_change`）。
火球 = **飞行物 + 落地 12 个定点伤害源**（2026-09-06 加强：飞行中每帧消弹 r 24，落地消弹 r 96；每帧 100 并把每敌每帧伤害上限抬到 300 —— 自机弹与卡的源共用这个上限，不抬就被自机弹吃满，Remilia 同款做法，AUDIT V20）：直线飞向开火时记下的敌人坐标（4 px/帧，非追踪；飞行中每帧 2 颗橙白粒子 script95），到点起爆炸光环 + 10 颗火星（script96）+ 小震屏（`ce_screen_shake(14, 5, 0)`，ECL setScreenShake 同一工厂 `0x476060`）、放 `0x2c`，之后
连续 12 帧每帧一个 96×96、50 伤害的 `ce_damage_rect`——一个源对同一敌人只结算一次、50 不超四个自机的每帧上限，所以恰好 600。
随机（落点 / 目标）走游戏自己的 `Rng__rand_dword(&REPLAY_SAFE_RNG)` `0x402740` / `0x4cf288`。血条复用青眼的 script86 / 87。
美术：卡图用户立绘（`fit_card.py --fill`，sprite 146/147）；本体用户正面像抠白底 256×256（`ability/make_firelord_art.py`，script91 缩放 0.25 ≈ 64 px 高、挡弹半径 33.6）；待命上下呼吸浮动 ±4 px（`fl_bob_dy`）+ 每帧 2 颗身体火焰粒子（script97，往上飘），移动 60 帧 quintic easing、落点限弹幕区下 1/3；
火球 / 拖尾 / 爆炸贴图程序生成（同脚本，script92–94）。语音 `FIRELORD_SUMMON`（id `0x55`）/ `FIRELORD_ATTACK`（`0x56`）：用户 ogg 经 `voice/convert_voice.py` 转 wav。
登场序列：语音 → 60 帧聚气粒子（script98，向中心汇聚）→ 本体 + 第一圈爆炸（复用火球落地的 `impact_fx`）→ 每 8 帧再一圈，共 3 圈。设计 `docs/superpowers/specs/2026-09-06-firelord-design.md`。

**真银圣剑（75）补充**：`category: 1`（用户：放装备区）——图鉴 / 编成排在原生子机卡之后，且 SDK 绑定时按原生装备卡置 flags bit6 ⇒ HUD 装备行（破损核心同此，AUDIT X11）。炉石 4/2 → 400 伤害 / 2 耐久。数字贴图 `ability/truesilver/DIGIT_0..9.png`（`make_truesilver_art.py`，DejaVu Serif Bold 白字黑描边 32×32，ability.anm entry24–33 / sprite126–135）；script99 与原生 HUD 图标脚本同款 `layer(26); resolutionMode(2)`，`pos(1080, 561)` = 图标根脚本 (1056, 537) + (24, 24)，实体坐标抄图标根 VM 的 `+0x5f0`。**偏移待实跑调**。卡图用户提供的圣剑插画（`_art/Holy_Sword.png` 居中裁 4:5 后 `fit_card.py`，sprite 152/153）。

**旋风斩（79）补充**：X 的定义**天然落在引擎的界内侧** —— `spend_power` `0x457480` 在火力 ≤ 一档时拒绝不扣、扣完钳一档，
而 X = ⌊P⌋ − 1 使得 X > 0 时扣完必然剩 [1.00, 1.99]，那条拒绝分支永远碰不到（单测 `test_cost_never_refused` 枚举 0..400 钉住）。
**也自带终止**：扣完火力在 [1.00, 1.99]，⌊·⌋ − 1 = 0，所以接出来的那几发再算 X 就是 0，不可能无限连（`chaining` 位是第二道保险）。
四发不可能同帧 —— `do_bomb` `0x420360` 自己的守卫（`+0x30` 非零就拒绝）决定了只能一发接一发，所以是「`on_bomb_spent` 记账 + `on_tick_2` 接续」两段。
**已知的跨卡联动**（都是设定内的，不是 bug）：与**腐化（70）**同时在场时，接出来的每一发都各扣一次炸弹上限 —— 一次按 B 烧掉 X+1 点预算；
与**真银圣剑（75）**同时在场时，每一发都各算一次挥砍（各出 400 伤害 + 各回一个残机碎片）；与**黄昏（68）**同时在场时两张卡都在接，
可能多接一发。卡图程序生成（`assets/cards/make_whirlwind_art.py`，固定种子；一道扫过 1.45π 的刀光，亮度只是角度与半径的函数，sprite 160/161）。

## 致敬・三国杀

| id | 名字 | 效果 | 实现（槽 / 事件） | 文件 | 状态 |
| --- | --- | --- | --- | --- | --- |
| 76 | 古锭刀 | 装备「武器」：boss 的**最后一张符卡**（终符）期间自机弹伤害 ×2（含 Extra 道中 boss）；炸弹与其他卡的伤害源不翻 | `on_tick_2` 每帧判「符卡进行中 && boss 活动中断槽的子程序名含 `Dead`」并把每敌每帧上限抬到 `sht+0x28` 基值 ×2；`on_bullet_created` 照 K♠ 把 `bullet+0x9c` ×2。AUDIT §Y | `guding.c` + `guding_core.c` | 🔧 |

**古锭刀（76）补充**：「终符」的判据来自 ECL 一手（[`engine/ecl/th18/01`](../../../engine/ecl/th18/01-boss-interrupts-and-spellcard.md) §6）：ZUN 每段攻击用
`setInterrupt(0, hp, time, "下一段子程序")` 衔接，七关 boss + Extra 道中最后一张符卡的槽名一律 `BossDead` / `MBossDead`，其余 `Boss2`…`Boss10`。
血条旁的星数（ECL 540 `stars`）不可靠——Extra 道中第二、三张符卡前都写 `stars(0)`，所以不用它。主线 1–6 关道中 boss 没有符卡，自然不触发。
只翻自机弹不抬上限会白翻（贴脸时自机弹已把 `player+0x47984` 吃满），所以两处一起做；抬的是 sht 基值的 2 倍而不是当前值的 2 倍，免得把炎魔 / 青眼那帧抬过的 300 再翻。
`category: 1`（武器归装备区，真银圣剑同款）。卡图用户提供的三国杀原卡（`_src/GUDING.png` 320×436，`fit_card.py --no-detect --trim 0 --margin 0` 等比覆盖裁上下各 ~4％，sprite 154/155）。

## 装备（子机）

| id | 名字 | 效果 | 实现（槽 / 事件） | 文件 | 状态 |
| --- | --- | --- | --- | --- | --- |
| 71 | 破损核心 | 装备：身边多一颗电球子机，每 1 秒朝**最近的敌人**（512 px 内）**瞬间**劈一道电弧，那一个敌人吃 120 伤害（名义 120 DPS，原生子机卡中位）| 第一张走**原生装备卡机制**的卡：`on_power_level_change` 生成子机、`on_tick_2` 计时 + 选目标 + 定点伤害源 + 电弧特效（详见下）。AUDIT §U | `broken_core.c` + `broken_core_core.c` | 🔧 |
| 77 | 豌豆射手 | 装备：身边多一株豌豆射手子机，向**正前方**（画面上方）吐豌豆；平时 1 秒一颗、一颗 90，每 20–30 秒进入**机枪状态** 100 帧，泼出 20 排 × 7 = 140 颗小豌豆（合 980 伤害）。综合约 123 DPS，但只在按住射击键时打 | 第一张真走 **SHT shooterset** 的卡：`on_power_level_change` 生成子机、`on_tick_shooters` 自带计数器当 short_timer 调 `ce_tick_shooters_for_card`（两组 shooterset 切换）、`on_tick_2` 在豌豆在途的帧抬每帧伤害上限。AUDIT §AJ | `peashooter.c` + `peashooter_core.c` | 🔧 |
| 87 | 天顶剑 | 装备：**按住 Z** 才甩剑；跟班**不画**（option 挂 1×1 全透明贴图，只剩定位锚；`ZN_FAM_CYCLE = 0`）| **运动 / 贴图 / 特效全部照泰拉瑞亚复刻**（`Projectile.AI_182_FinalFractal` + `FinalFractalHelper`，反 IL 读出）；跟班 = 原生子机；剑刃 = 卡自管 ANM VM 池（8 格）+ 椭圆用 `ce_polar_to_vel`。AUDIT §BE 详录 #211 | `zenith.c` + `zenith_core.c`（主机单测）| 🔧 |

**破损核心（71）补充**：子机 = `Player__allocate_option(card, 0x18, ability script88)`（指针存 `ce_state()`，
不是原生的 `card+0x54`：我们的对象只有 `0x54` 字节）。闪电 = **定点伤害源 + 特效**两件独立的事：
一道 120 拆成连续 4 帧、每帧一个 `ce_damage_rect(目标坐标, 0, 寿命 2, 30, 32×32)` 钉在敌人身上（Remilia / 青眼同一个原语；一个伤害源对同一敌人
只结算一次，所以每帧恰好 30；拆帧是为了单帧不撞 Sakuya 60 的每帧上限。2026-09-06 平衡：80 / 2 s → 120 / 1 s，对齐原生子机卡名义 DPS 中位 120–135），script89 电弧从电球拉到敌人（C 写 pos / rotation.z / scale.x = 距离 / 256）、
script90 火花在命中点。瞄准角与距离是自己的确定性算术（不引 libm）。
**不改任何游戏资源文件**——第一版曾追加 `pl0X.sht` 的 shooterset 让子机真开火，后来改掉：自机弹天生要飞过去，
与「瞬发单体」相悖。那条链的研究（[`engine/sht/th18/`](../../../engine/sht/th18/README.md)）与工具
（`assets/sht/`）留着给以后真需要「子机连射」的卡。卡图（青色裂核，白底放大居中）与场上电球（黄绿球）是用户原创，电弧（黄白闪电链）程序生成——都过 `assets/ability/make_broken_core_art.py`；卡图 sprite 144/145。
`category: 1`（子机装备）：图鉴 / 编成里排在原生子机卡 REIMU_OP…MAGATAMA 那一段之后（2026-09-06 起顺序表按类别插入，见 DATA.md `category`）。

**豌豆射手（77）补充**（致敬・植物大战僵尸）：子机 = `Player__allocate_option(card, 0x28, ability script102)`
（横向偏移与破损核心的 `0x18` 错开，两张同时装不重叠）。开火走**原生装备卡的第二个槽** `+0x1c on_tick_shooters`
——`assets/sht/append_shooterset.py` 造好之后一直没人用的那条链，本卡是第一个用户（破损核心第一版走过、后来改成定点伤害源）。
两组追加进 `pl0X.sht` 的 shooterset（索引 `0x17` / `0x18`，落在原生留空的 17 项里）：

| | fire_rate | 一排几颗 | damage | speed | anm |
| --- | --- | --- | --- | --- | --- |
| `PEA_NORMAL` | 60（1 秒；PVZ 原版是 1.425 s ≈ 85 帧）| 1 | 90 | 14 | script100 |
| `PEA_GATLING` | 5 | 7（`spawn_dx` −48…+48）| 7 | 18 | script101 |

**节奏不用引擎的射击计时器**：那个（`player+0x476d4`）只在 0..14 循环、松键就跳，表达不出 60 帧；
C 自带一个计数器当 `short_timer` 传进去（`param_2` 只进模运算、`PlayerBullet__create` 里根本没用它，AUDIT Z3）。
计数只在射击帧推进 ⇒ 「开大」不会在不开火时白烧；切换形态时清 0 ⇒ 开大那一刻立刻就有第一排。
随机等待 1100–1700 个射击帧（`ce_rand`，游戏自己的 replay-safe RNG），加上开大的 100 帧 = 周期 **20–30 秒**。
开大起手放 `se_powerup`（`0x0d`）+ 子机位置一团绿光（script103，复用豌豆贴图加色混合）。
**每敌每帧伤害上限**：平时那颗 90 正好塞满 Reimu 的 90（自机弹一开火就把它挤掉）、又超出 Sakuya 的 60，所以有豌豆在途的 40 帧里 `ce_raise_damage_cap(180)`
（Remilia 同款）；开大那排 7 × 7 = 49 本来就装得下，不用抬。豌豆走的是正常自机弹管线，所以 K♠ / 古锭刀的加成**也吃**（AUDIT Z16）。
美术全部程序生成（`assets/ability/make_peashooter_art.py`）：豌豆 32×32、子机 64×64、卡图侧面像 + 三颗飞出去的豌豆（sprite 156/157）。

**天顶剑（87）补充**（致敬・泰拉瑞亚的 Zenith；owner 2026-10-01 开卡，2026-10-02 落地，同日按 owner「按照泰拉瑞亚的状态做，包括贴图和特效」重做）：
**运动 / 贴图 / 特效三样都是原版复刻**，公式逐行对照见 [`native/cards/zenith_core.h`](native/cards/zenith_core.h) 的文件头：
- **物品号先订正**：天顶剑本体 = **4956**（不是 4144；4144 是「地灵之刃 Terragrim」，只是剑阵 21 把之一）。一手 = `Item.cs` 的 `shoot = 933`、`GetAlpha` 的 `case 4956` + wiki。
- **弹幕 = `ProjectileID.FinalFractal` 933 / `aiStyle 182`**：`velocity` 是**出膛定死的偏移**（`(目标−自机)/2`，`UpdatePosition` 对 933 直接早退 ⇒ 从不积分）⇒ 剑**绕着「跟班 + velocity」画椭圆**，`ang = π + sign(vx)·L·2π`（`L = 相位/60`）、`R = max(60, |v| + GetLerpValue(0.5,1,L)·40)`、第二半轴 `ai[0] ∈ [−100,100]`（我们每把用 `ce_rand()` 抽）、还有一个随 `L` 衰减的起始偏移；相位 120 消失、`Opacity = GetLerpValue(0,5,t)·GetLerpValue(120,115,t)`。**两次 `RotatedBy` 走引擎的 `ce_polar_to_vel()`（FSINCOS）⇒ 我们的目标文件零 sin/cos、零 x87。**
- **贴图 = `FinalFractalHelper._fractalProfiles` 的 21 把剑**（物品号 + trailColor 都是反 IL 读出来的）：落在 `ability.anm` 的 `ATLAS_ZENITH_BLADE`（entry 92、21 个成员、画布 432×288）；每把按原版规则挑 —— **每第 3 把固定是天顶剑本体**，其余从另外 20 把随机（`GetRandomProfileIndex` 会重掷 4956）。
- **跟班也轮播整组 21 把**（owner 2026-10-02「跟班也轮流显示整组 21 把」）：每 40 帧 `ce_anm_set_sprite` 换成下一把，一轮 14 s；子机 VM 被引擎重建（换关 / 火力档变 / 开店）时立刻补上当前那一把（`zn_fam_cycle`）。
- **特效**：残影（每 2 帧一枚、按这把剑的 `trailColor` 染色、脚本自毁、渐隐缩小 —— 对位原版的 4 个 ghost + ribbon）、出膛音 `se_slash`（对位 `SoundID.Item169`）、命中冷却 **7 帧**（对位 `localNPCHitCooldown = 7`）。
- **挥动特效（2026-10-03 补，owner「87 尽量做吧」）**：① **刃尖星芒**（原版 `DrawPrettyStarSparkle` 的 4 个 `Extra[98]` quad：`t 15→30` 淡入、`30→45` 淡出，画在 `Center + bladeDir·(bladeLength/2)`）；② **×1.25 白色叠加**（原版的辉光第二遍：同 sprite、白、alpha 半）；③ **尘埃**（原版 `DustID 278` 的 1-in-9 × 1–3 颗，两支自走自灭的小星点、染那把剑的 `trailColor`）。剑阵表也补了**每把剑的 `bladeLength`**（一手 1.4.5.0 表）—— 刃尖位置要用它。
- ⚠ **做不了的两样**（不是没想到）：原版的 **ribbon**（60 采样顶点条带 + `Misc["FinalFractal"]` shader + `Extra_201/193`）—— 引擎只有四边形 VM；**两处光照**（`Lighting.AddLight`）—— 引擎无动态光照通道。弧线仍由残影顶替。
- ⚠ 跟班与剑刃**共用同一张图集** `ATLAS_ZENITH_BLADE`；原先给跟班单开的那张单图 entry `ZENITH_SWORD` 同日撤掉（唯一消费者已改走图集，**没有别人的号受影响**）。
- 跟班 = 原生装备卡子机（与 71/77 同一套 `ce_option_*`；侧向站位自己写 `option+0x64/0x6c`）；`on_delete` 收光全部剑刃 + 残影 VM + 子机槽。
- ❓ 自定（进 `TUNING.md` §2.10）：出膛周期 10 帧、射程 640、弧长上限 200、池 8、每次命中 20、判定盒 40×40、残影间隔 2、跟班站位。

**星星炮（89）补充**（致敬・泰拉瑞亚的 Star Cannon；owner 2026-10-02 开卡）：
**数值逐条反编译对位**（一手 = 1.4.4.5 的 `Item.cs` / `Projectile.cs`，物品号与弹幕号从用户本机
1.4.5.8 的 exe 常量表读出，见 `native/cards/star_cannon_core.h` 的对照表）：
- 物品 = **`ItemID.StarCannon = 197`**（`_src/Item_197.png` 52×18，与 `width=50/height=18` 对上）；
  弹幕 = **`ProjectileID.StarCannonStar = 955`**（`Projectile_955.png` 22×24，`width=height=18`）。
- **攻速复刻无加成白板**：`useTime = useAnimation = 12`、`autoReuse = true` ⇒ **12 帧一发**（5 发/秒）；
  `shootSpeed = 14`（px/帧）；`aiStyle 5`：**不加速、不重力**（只有 0.14 rad/帧自转 + 0…200 闪烁）、
  **`penetrate = -1`（无限穿透）**、同一敌人吃默认的 `localNPCHitCooldown = 10`。
- 音效 **不是**借 TH18 现成的：走**自建语音槽** `CE_VOICE_STAR_CANNON_SHOT`（`0x61`），
  构建期从用户本机的 `Content/Sounds/Item_9.xnb` 现取（对位原版 `UseSound = SoundID.Item9`；
  取法 = `xnbcli` 解 XNB，与贴图同一条路）—— 与 112 取 TH12 `se_piyo.wav`、108 取 TH08 `se_option.wav` 同一口径。
- ❓ 自定：锁敌（原版用鼠标，我们没有光标 ⇒ 最近敌 640 px，无目标朝正上）、伤害 12（原版 55，按弹幕区尺度缩）、
  池 12、跟班站位、**不设弹药**（原版的坠落之星弹药 —— 卡名那句「桑百颗够吗？」问的就是它）。

**弹匣（社区梗）**：一个弹匣 **300 发**（对位原版的坠落之星弹药 —— 卡名那句「桑百颗够吗？」问的就是它）；打满 ⇒ 子机**变灰** + 装填 `SC_RELOAD_FRAMES = 180` 帧（3 s，这期间不开火），装完计数归零；**不显示任何数字**（owner 2026-10-03：「不需要 on_draw」）—— 计数只在内部（诊断日志里报）。❓ 装填时长（180 帧）是我定的。
**被动（死亡讯息）**：`+0x0c`（真死那一刻）置位、下一帧 `on_tick_2` 建 VM（不在死亡栈上建 VM），
贴图 = `assets/ability/star_cannon/make_death_msg.py` **按泰拉瑞亚死亡界面的一手规格**烘的两行中文（**覆盖**混合 = `blendMode(0)`，不是加算）（暖红 RGB(229,127,127)、**不描边**、两行盒顶差 50 px、最长 15 s（原版底下那行是 15 秒起步的复活倒计时），**自机第一次碰到它就起 20 帧淡出收掉**（照 115 的防遮挡）；见 AUDIT §BE 详录 #213 追加 B）
（0 依赖引擎字体：TH18 的 ASCII 图集没有汉字字形），挂 UI 层 25、位置 (320, 240) = 游玩区正中，
显示 300 帧、最后 40 帧淡出。

## 致敬・泰拉瑞亚

| id | 名字 | 效果 | 实现（槽 / 事件） | 文件 | 状态 |
| --- | --- | --- | --- | --- | --- |
| 89 | 星星炮 | 装备：**按住 Z** 且**场上有可攻击的敌人**才开火（炮体**跟着目标转**），每 **12 帧**（复刻原版白板 `useTime`）朝最近的敌人射出一颗**穿透**的星星| 跟班 = 原生子机（`ce_option_*`）；弹幕 = 卡自管 VM 池（12 格，`star_cannon_core.c` 纯逻辑 + 主机单测）；讯息 = 预渲染贴图（TH18 字体图集没有汉字）挂 UI 层 25 | `star_cannon.c` + `star_cannon_core.c` | 🔧 |
（**87 天顶剑**也是泰拉瑞亚的，按功能归在下面「装备（子机）」那一节。）

## 致敬・以撒的结合

| id | 名字 | 效果 | 实现（槽 / 事件） | 文件 | 状态 |
| --- | --- | --- | --- | --- | --- |
| 88 | 杰拉 | 主动（C 键）：**复制场上所有掉落物** —— 引擎道具（P 点 / 金钱 / 碎 / 卡道具 / 消弹小分）**加上各卡自己的**（信仰点·绿点 / 樱点 / 刻符·永点 / 灵力道具 / 压缩包 / 季节点 / 飞碟令牌 / 神灵 / 动物灵）。复制品在原位（略散开），当帧就被 ItemManager tick 到 ⇒ 在回收线上/吸收圈里的会立即被全屏吸走 | 见下「补充」（三段通道：引擎池 / 共享信仰点池 / 各卡私有池走 SDK 复制代数闸）。AUDIT §BE #212 | `jera.c` | 🔧 |

**杰拉（88）补充**（致敬・以撒的结合的符文「Jera」= 「Duplicates all pickups and chests in the room」，一手 = 官方 wiki）：
**为什么分三段**：本仓一半「掉落物」**不在引擎道具池里**（各卡 `.c` 的 static VM 池）⇒ 只有那张卡自己够得着。
已接的生产者：**108 刻符 / 109 灵力道具 / 112 压缩包 + 飞碟令牌 / 113 神灵 / 116 季节点 / 117 动物灵**（107 樱点与 110-111 绿点走共享信仰点池，不需要单独接；114 收点批没有自己的实体）。★ 112 的**满 B** 是两件套（原生 B 道具 + 一张卡面 VM）⇒ 用 SDK 的**认领谓词**把道具从池那一路摘出来，由 112 自己整份重撒（不缺卡面、也不多一颗 B）。
★ owner 2026-10-02：「112 和 117 的 token **不能重合**啊，得换个移动方向」—— 112 的复制品**水平方向与原物相反**（一手只有两个对角线方向），117 直接复用 `token_spawn_heading()` 自带的**方向避让**（24 px 内同向就转开）。
SDK 那一层因此只做两件事：**代数**（Jera 发动时 +1，生产者比对后复制一遍，差一帧天然被吃掉）与
**可见数台账**（生产者每帧上报，Jera 用它判「值不值得发动」——全场空就 `ce_refuse`，不烧充能）；
删卡由 `ce_layer_drop_card` 自动退台账。**绝不做每帧复制**（池会几何翻倍）。
数值全是 `❓ 自定`（充能 1800 帧 / 单次上限 512 / 散开 12 px），登记在 `TUNING.md` §2.10。
卡图 = **owner 2026-10-02 提供**（`Runes_sprite.png` 的卢恩字母 ᛃ，透明底紫色辉光）⇒ `fit_card.py JERA _src/JERA.png --trim 0 --margin 24 --bg '#ffffff'`，sprite 204/205。

## 致敬・杀戮尖塔

| id | 名字 | 效果 | 实现（槽 / 事件） | 文件 | 状态 |
| --- | --- | --- | --- | --- | --- |
| 90 | 冰淇淋 | 被动：**miss 时不会损失火力，且符卡 +3**（正常 miss 是掉一档火力、符卡重设为 3）| `+0x0c` 只置位 → `+0x14`（引擎遍历卡表、**在扣完火力之后**）把生前火力写回、符卡 = 生前 + 3 钳 `MAX_BOMBS`，再刷 HUD 符卡行。AUDIT §BE 详录 #214 | `ice_cream.c` + `ice_cream_core.c`（主机单测）| 🔧 |

**冰淇淋（90）补充**（致敬・杀戮尖塔的遗物「冰淇淋」：原版是**能量不随回合清空**，这里译成「miss 不掉火力」）：
一次 TH18 的 miss 原生做两件事（一手 decompile，见 `native/cards/ice_cream_core.h` 的对照）：① `0x45d090 Player__commit_death_and_enter_state2` ⇒ `CURRENT_BOMBS = clamp(3, 0, MAX_BOMBS)`、命 −1；② `0x45c154 call 0x457480 spend_power(一档)` ⇒ **`CURRENT_POWER` 掉一档**（钳到一档）。
引擎随后**遍历卡表逐个调虚表 `+0x14`**（`0x45c251`–`0x45c255`）—— 本卡就在那里把两个值写回去，**顺序是一手的**（`+0x14` 严格在扣火力之后）。快照**只在正常态**（`player+0x476ac == 1`）每帧采一次，避免把已经被扣过的值当「生前值」。符卡写回后刷 HUD（`ce_gui_update_bombs()` = 引擎 `0x4420e0` 同款）。

## 致敬・Minecraft

| id | 名字 | 效果 | 实现（槽 / 事件） | 文件 | 状态 |
| --- | --- | --- | --- | --- | --- |
| 91 | 鞘翅重锤 | 主动（C）：发动后进入**无敌**并弹出 **Undertale 式时机条**；有敌人可攻击时光标开始左右扫，在**中间亮带**里再按一次 C ⇒ **瞬移到最近的敌人上空**，重锤砸下造成 **800** 高额伤害，然后**回到原位**。3 秒没敌人 / 没卡准 / 按 D 切走 ⇒ 自动退出并留 30 帧无敌缓冲 | 见下「补充」（时机条 VM + 纯逻辑状态机 / 直接写自机坐标 / 声明层无敌）。AUDIT §BE 详录 #215 | `elytra_mace.c` + `elytra_mace_core.c` | 🔧 |

**鞘翅重锤（91）补充**（致敬 Minecraft 1.21 的**鞘翅 + 重锤**；贴图直接取用户本机 1.21.10 客户端 jar 里的 `item/mace.png` / `item/elytra.png`，见 `entries/ORDER.txt` 末段）：
状态机 = `IDLE → AIM → SMASH → EXIT → IDLE`（`AIM` 里三种退出：卡歪 / 3s 没敌人 / 3s 总预算）。三条关键口径：
① **激活那一帧的 C 边沿必须丢掉**（engine 的 `on_activate` 就是那一帧的 C 触发的；core 的 `armed` 管这个）；
② 引擎的发动门是 `card->state == 0 && recharge_cur < 1` ⇒ 进 AIM 后充能在跑，**第二次 C 不会再触发发动**，所以那一下能安全当确认键读；
③ 按 **D** 切卡（同一帧比对 `AbilityManager+0x38` 的选中卡是不是自己）与「3s / 卡歪」走**同一条**退出路径 —— 都进 `EXIT`，都拿那 30 帧缓冲无敌。

## 致敬・Noita

| id | 名字 | 效果 | 实现（槽 / 事件） | 文件 | 状态 |
| --- | --- | --- | --- | --- | --- |
| 92 | 伟大之作 | 主动（C）：把场上**所有弹幕与敌人变成金钱道具**（item type 2）；敌人按**剩余血量**换出多枚（每 20 血 1 枚、上限 40），**boss 期间**改成**直接击破一个阶段**。冷却按**转换量**等比例算（`600 + 弹幕×3 + Σ血量`，钳 `[600, 9000]`）；空场不消耗 | 见下「补充」（弹幕池逐颗 `Bullet__cancel(b, 0)` + 自撒 type 2 / 按剩余血量抬本帧总伤害上限 / 改写充能三元组）。AUDIT §BE 详录 #216 | `great_work.c` + `great_work_core.c` | 🔧 |

**伟大之作（92）补充**：卡面 = Noita 原版**三宝磨（Sampo）**贴图（用户本机 `data/data.wak` → `data/entities/animals/boss_centipede/sampo.png`，24×24；取法见 `assets/cards/_src/README.md`）。两条公式（金钱枚数 / 冷却）都是 **owner 口径**、不是原版的（Noita 的「伟大之作」在那边是炼金终点）⇒ 系数全标 ❓ 并进 `TUNING.md` §2.10。可复刻的只有「东西」本身：
· **金钱道具 = item type 2**（一手 `engine/faith/th18/01-point-item-value.md` §3：收下 = `MONEY += 1` + `SCORE += 身价/10`）；
· **弹幕池** = `CE_BM_BULLETS` 起 `0x7d0 = 2000` 槽、`state ∈ {1,2}` = 活动；
· **boss 判据** = `CE_ENEMY_FLAGS` 的 bit23（`CE_ENEMY_FLAG_BOSS`，一手 = ECL `setBoss`）。

## 致敬・东方绀珠传

| id | 名字 | 效果 | 实现（槽 / 事件） | 文件 | 状态 |
| --- | --- | --- | --- | --- | --- |
| 115 | 绀珠传 | 被动：**擦弹判定区里有敌方子弹时**，子弹变黄、自机周围出现一圈环、场上掉落物一直慢（速度 ×0.3，出框后约 0.12 s 恢复）；boss 战按「**一段攻击 = 一波**」切章，每波末按**擦弹数 × 击破率**结算章节奖，够高再给碎片；左侧一块 TH15 风格的波次面板显示实时读数（**玩家贴近时整块淡到 0.3 alpha**，本卡自定——绀珠传把波次拉长，面板挡视线） | `on_tick_2`：差分读 `GRAZE` `0x4ccd0c` + 符卡位 `SPELLCARD+0x78` 下降沿切波；擦弹帧写 `ITEM_MANAGER+0xe6bb14` = 0.3（引擎自己每帧 +0.1 恢复）；`on_draw` 三行 HUD 文字 | `lolk.c` + `lolk_core.c` | 🔧 |

**绀珠传（115）补充**：TH15 章节系统的复刻，一手在
[`engine/chapter/th15/01`](../../../engine/chapter/th15/01-chapter-and-bonus.md) 与
[`engine/player/th15/01`](../../../engine/player/th15/01-graze-and-item-slowdown.md)，
设计 [`docs/superpowers/specs/2026-09-09-lolk-chapter-design.md`](../../../docs/superpowers/specs/2026-09-09-lolk-chapter-design.md)。
公式照 TH15：`bonus = ⌊擦弹 × 击破率％ × 50 / 10⌋ × 10`，分数 += `bonus/10`，
`bonus ≥ 1,000,000` 给一枚碎片 —— ★ **固定残机碎片**（`0x10`）：本卡取 TH15 的**传统 / legacy 模式**口径
（完美模式才给 bomb 碎片），**没有「每 5 波循环」那回事**（旧描述错，代码一直恒命碎）；
✅ **残机满 8 时改给 bomb 碎片**（TH15 **传统（遗产）模式**原规则；owner 2026-10-15 拍板「对标传统模式」⇒ **已实装** ✓）。
**两个接缝都是引擎里现成的残骸**（用户猜的「ZUN 的残留代码」成立）：TH18 的
`zItemManager` 里还留着 `lolk_item_slowdown_factor`（`+0xe6bb14`，引擎每帧恢复、**没人写它**），
`Player__do_graze` 也还在（只加两个计数，TH15 的 PIV/浮字/减速三样都被删了）——
见 [`engine/chapter/th18/01`](../../../engine/chapter/th18/01-chapter-residuals.md) 与
[`engine/player/th18/02`](../../../engine/player/th18/02-graze-and-item-slowdown.md)。
**不加断点、不改字节**：只读三个字段、写一个 float。
面板五行的数值（TH15 `Gui::on_draw_2e` 的 `"%d"` / `"%3.1f%%"` / `"%3d"` / `"%6d"` / `"  +%d"`）
**2026-09-10 起从 ASCII 文字改成贴图 VM**（`ce_hud_vm_text_set` + 脚本 136 + TH18 font 2 原生字形）：
面板本体在 UI 层 24（暂停时被模糊罩盖住），数值行放层 25（与罩子同桩、罩子后起）⇒ 一起被盖住。
卡名 / 文案仍是**占位**，待用户确认；卡图 **2026-09-18 落地** —— 白底 + 绀珠传 exe 图标，sprite **182/183**（`LOLK_CHAPTER`）。

## 致敬・东方天空璋

| id | 名字 | 效果 | 实现（槽 / 事件） | 文件 | 状态 |
| --- | --- | --- | --- | --- | --- |
| 116 | 季节 | **主动卡**（2026-09-12 用户定案）：季节点（擦弹 / 掉落 / 解放消弹）→ 0..6 档 → 子机 → **D 切到本卡后按 C 解放**、**双击 Z** 切副季节（2026-09-29 定，S/Shift+D 均为历史）；掉落量、运动学、出场动画、HUD 全部照 TH16 一手，详见下表下方的「季节（116）补充」 | `on_tick_2` + `on_enemy_drop_pre` + `on_power_level_change` + `on_tick_shooters` + `on_draw` | `season.c` + `season_core.c` | 🔧 |

**季节（116）补充**：TH16 季节系统的复刻（**主动卡**：D 切到本卡 + C 发动季节解放），数值全部来自 TH16 v1.00a **脱壳版**一手
（[`engine/season/th16/01`](../../../engine/season/th16/01-season-data-and-tables.md)；纯逻辑在 `season_core.c`，
单测 `tests/test_season_core.c`）。残骸审计见 [`engine/season/th18/01`](../../../engine/season/th18/01-season-residuals.md)：
**TH18 里五样全无**（季节点道具 / 季节槽全局 / 季节子机数组 / 等级表 / 第二个 Bomb），所以全部自建。
要点：
- 季节点三条通道：**擦弹**（原版计数 `th18:0x4ccd0c` 的增量，TH16 `Player__do_graze` 同款）、
  **敌人掉落**（`on_enemy_drop_pre`，位置 = 敌人坐标；数量见下条）、
  **解放消弹**（`ce_cancel_radius` 返回值**每 1 颗 1 点** —— 2026-09-15 按 TH16 一手 `th16:0x416a00` 的
  mode 4「每颗弹 1 个季节道具」定的比例；实体发放另有每帧 4 颗的 ❓ 自定闸，见 `TUNING.md`）；
- **掉落量按 TH16 的计时衰减结算，档位逐敌人**（用户 2026-09-10：「小怪出场时间推移打死掉得会更少」✓
  → 「th16 有不同档位吗？」✓ 有，是逐敌人手写的 `(T,N,M)`）。
  衰减公式一手 `th16:0x420585`（`dropItems` 结算那一刻）：`n = (N−M) × 剩余帧 / T + M`（剩余 < 1 → `M`），
  计时器由 `dropSeason(T,N,M)`（`th16:0x422a26`）在敌人子程序开头设成 `T`；
  计时用引擎自己的 `zEnemyData.time_alive`（`enemy+0x1500`，每帧 +1 —— 一手 `0x430209` 的
  `LEA ECX,[ESI+0x2d0]; CALL 0x405990`，`ESI` = zEnemyData 基址）。
  **T 与档位取敌人自己的原生配置**（TH18 有同款机制：ECL opcode 573/574 → `zEnemyDrop.timed_ex_drop_time`
  / `timed_ex_drop`，结算 `n = ex_drop + timed×(T−timer)/T`；一手见
  [`engine/season/th18/01`](../../../engine/season/th18/01-season-residuals.md) §1b′）：
  `T` = `enemy+0x623c+0xa4`（原版 40..900），数量 = 原版 timed 计数（1..20）映射到 TH16 量级
  8/10/13/30（`season_drop_native_tier`，映射本身 ❓ 自定），`M` 恒 1；没配原生窗口的敌人
  （T = 0，含所有 boss）退回 TH16 小妖精 `(60,8,1)`。boss 恒 **200 颗**（每相位一次）。
  纯逻辑 = `season_core.c` 的 `season_drop_count_decay()` + `season_drop_native_tier()`（单测 31 断言）；
- 季节点是**本卡自己的 VM**（`ability.anm` script115，DLL 静态池 512 槽），**运动学逐条照抄原版道具**
  （`th16:0x42f4e0` / `th18:0x445a80`）：**出生态 = 待发射（`item+0xc50 = 6`，错帧 `total_items_created & 3`、
  VM 还没建）→ 飞散（TH16 state 3：`pos += vel × 减速因子`、`r -= 0.03`、`vel = polar(角度, r)`，
  角度不变、速度线性衰减，飞 `速度²/0.06` = 0.7..73 px，**无重力也不清 `vx`**）→ 飘落（重力 0.03、
  下落上限 2.0、`vy ≥ 0 → vx = 0`）/ 飞向自机**；出界 472/200、吸引初速 = 磁速/3 后每帧 +0.2 到 12.0、
  过回收线 `player+0x47998` 初速 = 磁速、收集半径/吸引半径/磁速读 `player+0x47988..94`
  ——**为什么不直接借原生道具**见 [`engine/season/th18/01`](../../../engine/season/th18/01-season-residuals.md) §1d
  （原生管线可用，但贴图/脚本绑在 `bullet.anm` 的类型表上）；
- 档位 = TH16 阈值 100/230/390/590/840/1140（0..6 档）；**子机数 = 档位（最多 6 颗）**——
  TH16 `num_season_options = season_lvl`、阵位表按 `{0,1,3,6,10,15,21,28}` 累计；子机走 `shooterset SEASON`
  （`assets/sht/append_shooterset.py`，弹 = `SEASON_SEED`），节奏 = `帧数 × 档位` 当 short_timer、
  `fire_rate = 120` ⇒ 间隔 120/60/40/30/24 帧；
- ★ **2026-09-12 用户定案：116 改成「主动卡」** —— 触发**季节解放**的方式是
  **用 D 键切换到该卡、然后按 C**（走 SDK 的主动卡通路：`active_recharge = 1` + `on_activate`，
  门槛「档位 0 / 正在解放」⇒ `ce_refuse`）。**原来的 A 键解放已删**。
  `active_recharge = 1` 的原因：真实代价是**季节点档位**而不是时间（先例 = 117 咆哮的动物灵槽）。
- **副季节切换仍绑 S 键**：TH18 的键盘配置只占 Z/X/Shift/Esc/C/D，S 空着
  （一手 [`engine/input/th18/01`](../../../engine/input/th18/01-key-mapping.md)）。
  ⚠️ 原始键不进 replay 输入流 → 回放里不重现（同文档 §4a）；`C` 是引擎自己的主动卡键，进流。
设计 [`docs/superpowers/specs/2026-09-10-season-and-animal-design.md`](../../../docs/superpowers/specs/2026-09-10-season-and-animal-design.md)。
子机阵位写 option 记录的 `+0x64/+0x68/+0x6c/+0x70`（原生卡 `th18:0x40b8c1` 同款）；**双击 Z 切副季节时子机的贴图与阵位一起换**（`refresh_options`）。
季节点外观 = `ability.anm` script 115 + **TH16 原版切图**（`bullet/item.png` 右上 2×2 = 春/夏/秋/冬 + `(224,32)` = 土用，各 16×16，按副季节换 sprite；出场动画照 TH16 script129，**含 `angleVel(%F0)` 的 0..5°/帧 随机自转**）。
档位数字（原 ASCII）**2026-09-10 起改成贴图 VM**（`ce_hud_vm_text_set` + 脚本 136 + TH18 font 2 的原生切图），
这样暂停时数字也跟槽一起被模糊罩盖住（ASCII 文字做不到，见 [`engine/anm/th18/03`](../../../engine/anm/th18/03-ascii-text.md) §2f）。
HUD = **游玩区内的左下角**（字面量空间 x 44 起）：季节字 PNG（`front00.png` (832,80)+32k，32×32）+ 实心 `drawRect(102,10)` 打底 + `drawRect(100,8)` 填充（scale.x = 档内进度）+ 档位数字贴在条右端；**UI VM 坐标 = 640 字面量 × 2**（AUDIT AF7），`ce_hud_text` 仍是 640 空间。★ 矩形脚本必须写 `sprite(-1)`，否则整条不可见。
**解放视觉**：两个 `drawPoly(1.0f,32)` 实心圆照 TH16 `pl00sub.anm` script3/4（外圈 layer13 减算灰→绿、内圈 layer12 加算红），前 10 帧扩到半径、之后 250 帧淡到 64、最后 20 帧淡到 0；**圆心 = 释放原点，只有秋跟着自机**（TH16 `BombSubFall__on_tick` `0x40eeb0`）。
卡图 **2026-09-18 落地** —— 白底 + 天空璋 exe 图标，sprite **184/185**（`SEASON`；占位期与绀珠传共用 116/117）。

## 致敬・东方红魔乡 / 妖妖梦 / 永夜抄 / 花映塚 / 风神录 / 地灵殿 / 星莲船 / 神灵庙 / 鬼形兽

| id | 名字 | 效果 | 实现（槽 / 事件） | 文件 | 状态 |
| --- | --- | --- | --- | --- | --- |
| 106 | 红魔乡 | **红魔乡 PlayRank**：12 条增减（收点/擦弹/吃道具/Rank 四件套），判定点指示 alpha=0；**`rank → 难度` 用户拍板完全搁置** | `on_item_collect` + `on_item_score` + `on_draw` + `on_stage_start` | `th06rank.c` + `th06rank_core.c` | 🔧 待实跑（spec §2.2 十二条 + 碎片会计三条已单测 40168 断言） |
| 107 | 妖妖梦 | **樱点 / 森罗结界**：樱点来源 = **击中敌人（含小怪，一手公式）+ ★（分数转换点族，各 +20）+ 小樱点 + 结界中擦弹**；**金钱 / P 不给樱点**（2026-09-30 订正）。满 50000 开结界 **9 s**；中弹 / B ⇒ 灵击崩结界（消弹转原生小分道具 + 40 帧无敌）。详见 `TUNING.md` §2.0u-2 |
| 108 | 永夜抄 | **妖机体 + 使魔**：低速止射（不改 Z 位）→ 换妖侧弹；子机走声明层（id 最小者落笔）；使魔 = 自建真敌人载体（`ce_spawn_enemy`），伤害转嫁 1/2（❓ 自定） | `on_native_shot_suppress` + `on_bullet_created` + `on_enemy_death` + `on_tick_2` + `on_draw` | `youkai.c` + `youkai_core.c` | 🔧 待实跑（194 断言；资产未到 ⇒ 本体 sprite / 妖侧 shooterset `0x16` / 子机声明数全留 -1·0 占位）|
| 109 | 花映塚 **× 獣王園（合成卡）** | **机制全部落地 + 蓄力攻击「混合路线」**（C1/C2 = sht、C3/C4 = ECL 版底伪 boss 载体 + 逐弹自伤源）⇒ ★ 细节见下方 §109 | `on_enemy_death` + `on_tick_2` + `on_run_reset` + `on_delete` | `pofv.c` + `pofv_core.c` + `assets/ecl/{spirit109,pofv109_boss}.ecl.txt` | 🔧 **待实跑**（**846** 断言；数值占位，受一手「每帧伤害上限 90」约束）· 规格 [`SPEC-109-FUSION.md`](SPEC-109-FUSION.md) §10 · ❓ 见 `TUNING.md` §2.0v-1…7 | |
| 110 | 风神录 | **风神录信仰**：绿点实体池（与 111 共用）+ 按槽记账、连击 130f、灵击（门槛 2.00 档 / 扣 1 档 / 下限 1.05；★ 消弹圈/伤害/音效/满 P 表按 TH10 一手重写 ⇒ `TUNING.md` §2.5）、PV→计分只由落笔者落笔；★★ 2026-09-15 生成语义收口 ⇒ §2.5c；★★ 大信仰点判据换成**宿主表**（不再按「掉钱」）⇒ §2.5f | `on_enemy_death`（宿主表） + `on_item_collect` + `on_item_score` + `on_activate`（新）+ `on_tick_2` + `on_draw` | `mof.c` + `mof_core.c` + `faith.c` + `faith_core.c` | 🔧 待实跑（**1007** 断言；绿点实体池 `faith.c` 也是 111 的共用件）；★★ **2026-09-21c 主动卡 + 灵击合并**：灵击多一个 **C** 入口（D 切到本卡后按 C），实现 = **110/111 共用的唯一一份** `cards/bomb10.{h,c}`（新增共享件）⇒ `AUDIT.md` §BE-FA |
| 111 | 地灵殿 | **地灵殿信号槽**：回收线上方 +50/f、满 100 自动全收道具、线下 −0.3/f、被弹归零（0–130）；绿点信仰（共用池，按槽记账）；蓝点分 = 信仰 ×(擦弹倍率 + 信号强度) 落在金钱道具分数上（**不改金钱数量**）；B 双选与 110 同规则；★ **满 P 转换与 110 是同一台机器**（共用件扫池 ⇒ 中绿点、type 9 档；`TUNING.md` §2.5c） | `on_activate`（新）+ `on_item_collect` + `on_item_score` + `on_tick_2` + `on_draw` + `on_death_after_deathbomb` | `signal.c` + `signal_core.c`（共用 `faith.c`） | 🔧 待实跑（**653** 断言；HUD 坐标/配色与灵击伤害是 ❓ 自定占位）；★ **2026-09-21 主动卡**：灵击多一个 **C** 入口（D 切到本卡后按 C）—— 与 X 调同一个入口（现为共用件 `bomb10_start()`，2026-09-21c 起 110/111 共用），灵击侧规则一字未变；`AUDIT.md` §BE-EV |
| 112 | 星莲船 | **飞碟**：**token = 自建 VM 池**（照 117/113；不再是敌人载体 ⇒ 打不掉、只能碰）；吸收链 = 伪磁场拉拽 type 1/2 → 置空槽 → 计数；击破走 `on_enemy_death`（排除清场）；凑组三同色 / 三色各一 / **两同一异 = 丢最旧**（一手 KEEP2）；**金 UFO = 蓝 UFO**（P 图调色）| `on_enemy_death` + `on_item_frame` + `on_tick_2` + `on_draw` | `ufo.c` + `ufo_core.c` | 🔧 待实跑（**992** 断言；HUD 第一版在层 25；token 会飞会反弹、来源=小怪击破；**真 ECL 已从 TH12 移植**（`assets/ecl/ufo.ecl.txt`）；**本体/令牌用官方原版切图**（`ability.anm` script 164–173）由**卡侧跟随 VM** 画、ECL 侧 `anmSetSprite(0,-1)` 当隐形载体 ⇒ 已可发布）|
| 113 | 神灵庙 | **神灵庙灵界**：① **手动**（D 切到本卡 + C，槽满才可）；② **中弹救援**：救援层最后一家（107→117→决死 B→113，落点 = 卡虚表 `+0x0c`）；**延时不是免死**；神灵池 = 原版切图/脚本；★ 指示槽/机体特技/主炮组（§BV/§BZ/§CJ）；★ **掉灵**：小怪 = 通道 B 衰减、**boss 相位击破 = 一大批**（§CK/§CL）| | `+0x0c`（**return 1**）+ `+0x14` + `on_item_frame` + `on_item_draw` + `on_enemy_death` + `on_draw` + **`on_shot_set_index`**（§CJ）+ **`on_option_shot_gate`**（灵界停子机） + **`on_enemy_drop_pre`**（§CL）| `trance.c` + `trance_core.c` + `trance_bgm.c` + `trance_bgm_core.c` | 🔧 待实跑（计分载体搁置；音效 5 个 ❓ 自定；★ ABI 缺陷已修 §BP-1；★ 灵界主炮组 §CJ；★ 掉灵 §CK/§CL；★ **灵界版 BGM 真素材**（TH18 各面曲子 + naive 低音放大，`tools/make_trance_bgm.py`，见 §CO））|
| 117 | 鬼形兽 | **主动**：收集 5 颗动物灵 → **满槽即自动起咆哮**（暴走 = 同类 ≥3 时主炮换成兽灵专属弹幕），**按 X（炸弹键）**把它用成 40 帧霊撃；中弹可用咆哮抵消死亡。细节见下「鬼形兽（117）补充」| `on_enemy_drop_pre` + `on_item_collect` + `on_death_before_deathbomb` + `on_activate` + `on_tick_2` + `on_draw` | `animal.c` + `animal_core.c` | 🔧 待实跑（Stage 1；兽灵弹 ANM 已修，见 §AL27）|

**鬼形兽（117）补充**：TH17 动物灵 / 咆哮的卡侧复刻（一手 [`engine/animal/th17/01`](../../../engine/animal/th17/01-animal-spirits.md)；
TH18 无残骸审计 [`engine/animal/th18/01`](../../../engine/animal/th18/01-animal-residuals.md)）。数值全部照 TH17：

- **动物灵实体**：自己的 VM 池（40 只 = TH17 `zGoastManager` 上限 0x28），贴图 = 原版 `beast1.png` 的
  13 个 32×32 单元（狼 / 獭 / 鹫 各 3 帧 + 4 种鱼灵），脚本 137–143 由 `beast.anm` script0–6 生成
  （`assets/ability/make_animal_art.py`）。**DVD 式约 45° 弹跳**：版边 x±180 / y 128..384 翻符号 +
  速度下限 `±(0.5+rand×0.3)`（TH17 `0x410c8c` 段）；24 px 吸收（`vel=(player−token)×0.25`，约 10 帧）、
  60 px 内减速 ×0.5；长寿命变体远离时 240→180 帧循环变色（狼→獭→鹫）、靠近冻结；年龄 7800 变暗、
  8400 起停止弹跳 → 漂出版边消失；对话中不移动 / 不消失、只走变色计时。
- **鱼灵（道具鱼灵）**：4 = 炸弹碎片、5 = 残机碎片、6 = 火力、7 = 点；咆哮中收到**立刻**换成原生道具
  （TH17 `FUN_004108d0`），咆哮结束时把队列里剩下的鱼灵换成道具（`FUN_00410640`：4→item6、5→item4、
  6→10×item1、7→10×item2）。位置 = **自机上方 80 px 的一次性固定点**（不跟随自机）；
  蓝点 / P 出 10 颗并额外播 **se `0x0d`**（一手 `th18:0x4462c2`），残碎 / B碎各 1 颗、走引擎道具通路。
- **自然掉落（对齐 TH17 `BossItem` 全表）**：杂兵**不掉**动物灵（TH17 杂兵只有 item 1/2/3）；
  **boss / 中 boss 死亡** → 炸弹碎片灵 / 残机碎片灵（`@BossItem(19/20)`，21:15 ≈ 3:2）；
  **boss 相位 / 符卡边界** → 随机变色动物灵（87%）或点鱼灵（13%）（`@BossItem(33)` ×39 +
  `(30/32)` ×10 + `(22)` ×7 的合并近似）；道中 boss 的相位边界不掉（TH17 只有稀有条件，本卡不做）。
  信号 = `on_enemy_drop_pre` + boss id 表，以及 boss **活动中断槽的 `phase_sig` 变化**
  （照绀珠传卡的一手 `phase_read`，`zEnemy+0x6374` 起 8 槽）。
- **5 格队列** = `TOKENS[5]`（存 type，不是 5 类计数）。满 5 → 就绪判定：狼 / 獭 / 鹫某一类 ≥3 →
  **暴走咆哮** + 时长表 **840 / 1140 / 1500**；**主凭依灵**（TH17 的自机固定兽灵）默认**不存在**
  ⇒ 不吃 +480；卡组里带着「弱肉强食之理」(25) /「鬼杰组长的恐吓」(**34**, 吉吊八千慧) 时视为主凭依灵**恒存在**（⚠ 01 段曾错写成 30；30 是袿姬的「偶像亲卫队」）
  ⇒ 任何暴走咆哮都 **+480**（用户 2026-09-11 定案，见 AUDIT §AL25）；
  都不足 3 → 混合、恒 **1140**（AUDIT §AL 的二次定案）。
- **★ 咆哮 = 满槽自动生效**（2026-09-11 用户定案 + 一手）：TH17 `FUN_0040fb60` 在满 5 格**就绪那一刻**
  就置 `HYPER_FLAGS` bit1，而 `Player__do_shooting`（`th17:0x44a710`）的 shot 组索引读的就是 bit1
  ⇒ **主炮组在就绪那一刻就换**、不需要按键。本卡同款：`on_ready` 起咆哮即生效（见下「兽灵专属效果」）。
- **霊撃 = 按 X（炸弹键，TH17 原位置）= 「中弹」手感**（用户 2026-09-11 口径）：把已在跑的咆哮**用掉** ——
  40 帧、半径 `(1 − f⁴) × 200` 消弹 + 每帧 40 伤害源 + 60 帧无敌；水獭暴走额外 3 个旋转消弹点。
  TH17 在这个状态下会屏蔽原版炸弹（`player_is_trying_to_bomb` `0x411d40`）；C（`on_activate`）是同一动作的
  TH18 主动卡版入口，中弹抵消死亡走同一段（`by_hit=1`）。
- **中弹不扣命** = `on_death_before_deathbomb` 里 `Player::cancel_impending_death`（`th18:0x45d590`）
  + 开霊撃（`by_hit=1`）；`Player__die` 先置 state 4 再广播 `+0x10`，所以这个槽里取消是安全的（AUDIT §AL2）。
- **咆哮计时**：停止射击时窗口每帧 −1、有存活子弹时 −2（TH17「有子弹再 −1」）；咆哮中收动物灵延长
  **90 / 60 / 42 / …** 帧（用户口径；TH17 代码是 180 起步 ×2/3 ⇒ 120/80/53，见 `animal_next_extend`）；
  正常结束（超时未霊撃）→ 鱼灵换道具 + 自机附近放 **2 个变色动物灵**；霊撃结束（C 或中弹）→ 鱼灵照换、
  **不放**那 2 个（用户定案）。
- **Miss**（`on_death_frame2`）→ 掉 2 个**不变色**的动物灵，与自机兽灵同种。
- **HUD**：左下角 5 格灵槽（**空槽 = 动态小灵魂** = TH17 script19 的 16×16 序列，脚本 154；
  **槽满 = 火焰** = TH17 script24 的 48×48 序列 sprite53–64，脚本 156；图标 = 槽内 token 的贴图；
  `ability.anm` script 144 / 145，**layer 25** —— 与 `ce_hud_vm_text` 同一个暂停模糊层；
  ★ 灵魂/火焰都必须 `layer(25)`（= 相机 2），`layer(20)` 是相机 1、坐标空间不同会整体偏半屏）。
  **槽内图标**（2026-09-10 二次定案）：动物灵与**四条道具灵都**用原生 `beastf.png` 的槽位图
  （`ICON_*`）——「槽里显示资源卡卡面」那版**已回退作废**（`CARD_BOMB` / `CARD_LIFE` / `CARD_MONEY`
  三张卡面贴图仍留在 ORDER 里备用，只是不再被引用）。
  ⚠️ 飞在画面上的灵永远用 `beast1.png` 的 token 单元，别和槽里的图标混用。
  **金钱鱼灵（token 7）的槽内图标也换成用户手绘的金版**：`assets/ability/animal_custom/ICON_FISH_MONEY.png`
  （32×32，`sprite_for_type()` 的 default）**；飞行贴图 = 用户 2026-09-10 手绘**：`assets/ability/animal_custom/FISH_MONEY.png`
  （32×32，原生蓝点鱼改成金色；ORDER 里是 `FISH_MONEY`，脚本 143 的 `sprite(@FISH_MONEY)`）——
  手绘源单独放 `animal_custom/`，不进 `make_animal_art.py` 的产出目录（生成器只管原生切图）。
  **鱼灵换道具**（TH17 `0x410640` 的形状）：
  6(P鱼)→type 1 火力道具 ×30；**7(金钱鱼)→type 2 金钱道具 ×30**（2026-09-10 二次定案回退，不用金钱卡）、
  **5(残鱼)→残机碎片卡 ×1**（`0x10` EXTEND2 一次性卡）、**4(B鱼)→符卡碎片卡 ×1**（`0x11` BOMB2 一次性卡）——
  **碎片**（残 / B）走 TH18 原生的一次性卡（`th18:0x4b4020` + [`engine/card/th18/05`](../../../engine/card/th18/05-shop-and-money.md) §8），
  **不是** type 4/6 那些别的作的遗留道具槽（用户 2026-09-10：「残碎 / B碎在虹龙洞是一次性卡」；
  辉针城卡 2026-09-09 已纠偏过同一个坑，见 `native/cards/collect2x.c:180`）；
  **金钱**（7）2026-09-10 二次定案**回退**成 TH17 的 30 颗 type 2 金钱道具（不用 `0x12` 金钱卡）。
  位置照 TH17 的展开椭圆，且**出生点先推离自机吸收圈**（AUDIT §AL12 / §AL13）。
  几何：间距照 TH17 `pos(-360+36i, 872)`（36 px），绝对位置按用户 2026-09-10 的 1280 截图定为
  **x = 88+36i / y = 904（表面 px）**（在「锚游玩区左下角」那版上**左移 193、下移 4**）。**暂时允许与别的卡的左下角 UI 重叠**（用户 2026-09-10 口径；
  最终做竖直堆叠 / 右下角，见 [`SDK.md`](SDK.md) §17）。
- **兽灵专属效果（暴走时自动生效，2026-09-11 已接）**：狼 / 鹫 = **换主炮 shot 组**（把 `pl0X.sht`
  `+0xe0` 偏移数组的 0..9 槽临时指向 `assets/sht/append_shooterset.py` 追进去的 TH17 set11..14 / 16..19，
  灵梦狼 = 7 发散射 `hit_w=112`、鹫 = 5 发诱导；聚焦不影响，`hyper_shot_restore()` 在咆哮结束时还原）；
  獭 = 3 颗旋转光球（脚本 149）；混合不换。
  ★ 2026-09-11 二修：TH17 `PlayerBullet__fire` 实际读 **`bullet.anm script(anm+5)`**
  （`th17:0x44a180`），TH18 同字段却直接索引 **`pl0X.anm`**。旧实现把 sht 的 anm=5/6/8/11
  同号指到普通主炮脚本，所以贴图错；且照抄的 TH17 wrapper 带 `delete()`，VM 被删后
  TH18 整发弹也失效，所以射程变短。现在 `assets/build_player_anm.py` 仍把 TH17 `bullet.anm`
  的 **9 个 entry** 纯追加进四个 `pl0X.anm`，但追加脚本改为 TH18 原生绘制：槽5/6/8/11
  画 sprite339/340/341/342（无 `delete`/位移），槽7 是一次性束端特效（新号：`pl00` 41..45、
  `pl01` 47..51、`pl02` 30..34、`pl03` 29..33）。`HYPER_ANM_MAP` 指向这些新号；发布包带
  `th18/pl0X.anm`。一手、失败模式和贴图 sha256 对账见 AUDIT §AL27 + §AL28。
- **咆哮期间的自机特效**：**全屏渐变圆**（TH17 script32 = 脚本 155：`drawPoly(1.0f,32)` + `blendMode(2)` +
  `color(青)`→`color2(橙)`、`scaleTime(60,1,600,600)`、layer 6 在弹幕区背后、跟随自机，霊撃开始按
  TH17 `do_hyper_cancel` 清掉）+ 类型光环（原版 `beastAURA.png` 的 24 个 64×64 单元，脚本 146–148 照 TH17
  script41–43 生成，layer 13 跟随自机，红狼 / 绿水獭 / 紫鹫）；**霊撃本体 = 三色光圈**
  （原版 `beastr.png`，脚本 150–153 照 TH17 script33–36 生成，父脚本每 3 帧起红 / 绿 / 紫光圈、
  92 帧后自删）；水獭暴走额外 3 个旋转消弹体（原生 `beastot.png`，脚本 149）；**自机弹按类型染红 / 绿 / 紫**。
  （狼 / 鹫原本的「染色 + ×1.5」标准版已被真正的 shot 组切换取代 —— 见上「兽灵专属效果」。）
- 纯逻辑 `animal_core.c`，主机单测 `tests/test_animal_core.c`（**116 断言**）。

⚠️ **Stage 3 边界（❓自定 / 待实装）**：① ~~暴走咆哮的射击强化是标准版~~ → **2026-09-11 已换成
TH17 的 shot 组**（`pl0X.sht` +10/+15 组，脚本 `assets/sht/append_shooterset.py` 追加）；
② 稀有动物灵（8..14）的爆散不做（用户定案）；③ 灵槽 / 光环几何待实跑调。
详见 [`NEXT.md`](NEXT.md) §1v。

## ⚠️ 改 cards.js 前必看：字段不合规 = 整批新卡消失

`desc` ≤ 6 行、每行 < 64 字节 UTF-8、非空、**不含 ASCII `%`**（用 `％`）；`price_tier` 0..14、
`weight` 0..255、sprite 必填 —— 任何一条不合规，DLL 的自检（`cards_def.c`）会走
`mitigation: allocator bound restored to retail (56)`，**整批新卡失效**（现象是「卡全没了」）。
构建期已加 `make cards-check`（并入 `make check` 与 `make release`）。
### §109 细节（2026-10-01：混合路线落地后）

**「混合」= owner 拍板的路线**：**C1/C2 走 sht**（自机弹：朝上半圈 + 各 2 条自机狙行）；
**C3/C4 走 ECL**（版底伪 boss 载体 `CEPofvBoss109` + 逐弹自伤源）。

| 项 | 状态 |
| --- | --- |
| 蓄力/结界/暴风/护盾/灵（真敌人）| ✅ 已落地（见 `TUNING.md` §2.0v-1…5）|
| **C3/C4 的 ECL 载体** | ✅ **四角色 × 两档逐条转写**：灵梦/魔理沙/咲夜 = **TH09** `plNN.ecl`；★ **早苗 = TH19** `pl02.ecl`。伪 boss 口径：隐形、判定 1×1、**不登记任何 boss 身份** |
| **逐弹自伤源** | ✅ 扫敌弹池 + **四道过滤**（弹型 ∈ ECL 的 `etSprite` 集合 / 锚点覆盖区 / 往上飞 / 从锚点发散）+ 置 `0x200`（跳过碰撞，**不伤自机**）|
| **自家机制隔离** | ✅ 清弹（暴风/CA 场/结界）不吃自家弹；自机狙不瞄自家载体 |
| **构建硬闸（2 条）** | ✅ ECL 不许调用不存在的子程序；ECL 弹型 ↔ C 侧名单逐条对账 |
| **一手挖到的约束** | ✅ 出膛坐标 = `option+0x5c/0x60`（`0x40a9c0`，逐指令）；世界坐标 x 以 **0** 为中心（锚点 `(0,440)`）；**每帧总伤害上限 = 90**（`sht+0x28`）|
| **数值** | ⏳ **占位**（DPS / 自机狙密度 / 覆盖格）—— 已在「≤90/帧」的硬约束内配平（`16×5 = 80`），等 owner 实跑反馈 |

## 约定

- **即时卡**（买了就生效、不进卡组）：`ctor` 或 `dtor` 施加效果后 `return 1`（原生 EXTEND / 六文钱同款，`02-lifecycle.md` §3）。
  这种卡 `deck_visible: 0`（编成里不列，初始携带不调 ctor 会变成死卡）、`repeatable: 1`（可再刷出）、`category: 3`（图鉴里和 EXTEND / PENDULUM 那批资源卡放一起）。
- 「随机」一律走游戏自己的 RNG 或确定性计数，不引入自己的随机源（replay）。
- 主动卡：`category: 0`，`active_recharge` 给帧数（原生瞬发卡 20–60 s），`on_activate` 返回 0 瞬发 / 1 持续。
- `category` 决定图鉴 / 编成里排在哪一段（同类别最长连续段之后）：0 主动（RICEBALL 之后）、1 子机装备（MAGATAMA 之后）、2 被动（MUKADE 之后）、3 资源 / 即时购买卡（MOKOU 之后；强欲之壶、方片 2 填 3）。
- 文案不能含 ASCII `%`，用 `％`。
