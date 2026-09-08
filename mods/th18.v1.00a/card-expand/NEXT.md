# NEXT —— 下一个会话从这里开始：实跑行为与资源段

> **版本**：TH18 v1.00a（`th18.exe`，imagebase `0x400000`）。本文裸地址默认属该版本；引用其他版本须写成 `th16:0x…`。
> 交接文档。写给**没有本会话上下文**的下一个会话；读完这一页应该能直接动手。

## 0. 现状一句话

一张新卡的 **10 段路全部有了**（[`MAP.md`](MAP.md) §0）：JSON 登记（[`DATA.md`](DATA.md)）+ C 行为（[`SDK.md`](SDK.md)）。
第一批行为卡 = 黑桃 10/J/Q/K/A（id 58–62，`native/cards/`）。**第 7（商店）、9（行为）、10（数据）三段静态审计通过、待实跑。**

| 段 | 状态 |
| --- | --- |
| 1–6 表 / 分配器 / owned / 存档 / 文案 / 图鉴编成 | ✅ 实跑（2026-09-02）|
| 7 商店、10 JSON 数据 | 🔧 待实跑（AUDIT §N）|
| 9 行为（SDK + 黑桃五张 + 强欲之壶 + 反转牌）| 🔧 待实跑（AUDIT §O）← **先做这个** |
| 8 卡图 | 🔧 工具就位（[`assets/`](assets/README.md)）：黑桃五张占位图 sprite 118–127 已进 `_255/th18/abcard.anm`，待实跑 |
| **24 股市（id 80）** | 🔧 **待实跑**（AUDIT §AF）—— 第一张「新机制」卡：C 买 / Shift+C 卖 50 金钱，K 线面板常驻 HUD 右下。**面板坐标 / 字号全是猜的**，实跑第一件事是把它挪到不挡东西的位置（§1p）。顺带 SDK：状态块 512、`on_delete` 回调 |
| **23 旋风斩（id 79）** | 🔧 **待实跑**（AUDIT §AE）—— 被动：放炸弹时额外再连放 ⌊火力⌋−1 发，代价是同数的火力；不开新断点，全用已有的件（§1o）|
| **22 衔尾蛇（id 78）+ 伤害层 + HUD 文字** | 🔧 **待实跑**（AUDIT §AA / §AB / §AC）—— 每 miss X 次自机弹伤害永久 +20％、X 递增；顺带把「加伤害」提炼成 SDK 的一层，K♠ / 古锭刀已迁移（§1n）|
| **21 豌豆射手（id 77）** | 🔧 **待实跑**（AUDIT §Z）—— 装备卡：豌豆射手子机向上吐豌豆，每 20–30 s 进机枪状态泼 140 颗；**第一张真走 SHT shooterset 的卡**（§1m）|
| **20 古锭刀（id 76）** | 🔧 **待实跑**（AUDIT §Y）—— 被动武器：boss 终符期间自机弹伤害 ×2（含 Extra 道中）；判据 = 活动中断槽名含 `Dead`（§1l）|
| **19 真银圣剑（id 75）** | 🔧 **待实跑**（AUDIT §X）—— 被动武器：每关前 2 发炸弹 +400 伤害 + 1 残机碎片；**HUD 小卡上画剩余次数**（第一次用虚表 `+0x44` 与 UI 列表建 VM，偏移待调，§1k）|
| **18 倒吊人（id 74）** | 🔧 **待实跑**（AUDIT §W）—— 即时卡：随机摧毁卡组里另一张卡 + 300 金钱；**第一个局中删卡原语 `ce_destroy_card`**（§1j）|
| **17 黄金小丑（id 73）** | 🔧 **待实跑**（AUDIT §W）—— 被动：每关进店金钱 +20％；新事件 `on_shop_open`（不开断点）|
| **16 炎魔之王拉格纳罗斯（id 72）** | 🔧 **待实跑**（AUDIT §V）—— 主动：扣 2.00 火力召唤；800 HP 挡弹、每 7 s 随机移动 + 向随机敌人投火球（§1i）|
| **15 破损核心（id 71）** | 🔧 **待实跑**（AUDIT §U）—— 第一张真装备卡：电球子机 + 瞬发电弧（定点伤害源）|
| **14 腐化（id 70）** | 🔧 **待实跑**（AUDIT §T）—— 被动：放炸弹扣的是上限，一次给满七发、用完为止 |
| **13 加倍（id 69）** | 🔧 **待实跑**（AUDIT §S）—— 被动：Miss 掉 2 命、敌人掉落 ×2；**2026-09-07 修小怪主掉落没翻（S10），待复验** |
| **12 黄昏（id 68）** | 🔧 **待实跑**（AUDIT §R）—— 被动：最后一颗炸弹用掉后自动再放一发 |
| **11 音效表扩容 / 语音** | 🔧 **待实跑**（AUDIT §Q）—— 四张音效表搬进 codecave 加长到 116 行，32 个语音 id `0x54`–`0x73` |

## 0.5 先读什么

[`README.md`](README.md) → [`DATA.md`](DATA.md) → [`SDK.md`](SDK.md) → [`MAP.md`](MAP.md)（追溯表，**每改必补**）→ AUDIT §N、§O。

## 1. 实跑清单（叠 `_255` + `_test`，Windows `git pull` 后）

日志 `th18_card_expand.log` 应有：

```
table: 255 rows filled … NULL/BACK shop weight := 6
grow: … shop loops 255 ids
cards: 58 "黑桃 10" (SPADE_10) tier 5 weight 2 dmode 0 sprites 116/117 initial_unlocked   ← 五行（卡池在 _255 自己的 th18/cards.js）
cards: 5 registered from cards.js; shop pool 397/560 slots, guaranteed offers <= 17/57
cards_dev: start_deck has 5 ids, trace=1
menu: … (56 retail + 5 new + NULL, rest BACK); encyclopedia entries = 61 …
sdk: 58 bound (.on_item_money = on_item_money)         ← 五行
sdk: 5 behaviors, 0 registered cards without behavior; base vtable @ 004b4c78 verified; trace=1
OK: … cards loaded, menu extended, behaviors bound, 100/100 sites verified
```

进游戏：卡组编成里把前五格清空 → 开局：

```
test: initial deck slot 0: empty -> id 58   … slot 4 -> id 62
trace: card 58 object … bound to vtable …    ← 五行
trace: allocate_new_card(id=58, mode=1)  <- NEW ID   … （自动 mark_obtained）
trace: card 59 on_tick_2 (+0x2c) first hit / card 62 on_tick_2 / card 61 on_bullet_created / card 60 on_load / card 58 on_item_money
```

体感：J 移速明显快一点；Q 道具老远就飞过来；K 打 boss 快一点（看血条）；死一次 → A 的无敌时间明显更长（280 → 420 帧）；
10♠：吃满 10 个金钱道具时钱数跳 2（右上角金钱）。商店里五张会以 5–9 档价出现。任何 `FAIL:` / `mitigation:` / 游戏崩溃都是回归——
崩溃优先怀疑：绑定断点的寄存器（O1）、`on_tick_2` 里 `PLAYER_PTR` 为空（已判空）、桩签名（O3）。

**强欲之壶（63）**：商店里买它（300）→ 日志 `pot: gave card X (allocate -> N)` 两行、`trace: card 63 ctor` → 卡组 HUD 多两张、壶本身不在；
它可以再次刷出（`repeatable`）。编成里**不该**出现它（`deck_visible: 0`）。

**反转牌（64，第一张主动卡）**：`cards_dev.js` 起手带 64 → 它出现在主动卡组（HUD 有充能条，一开始满 = 可用）→ 关卡里按 C：
日志 `trace: card 64 c_press`、`reverse: N bullets reversed`，弹幕整体掉头 + Tenshi 的发动音；充能条清空、约 60 s 后回满可再按；
过关充能不丢，局末清零；`_test` 的 `trace` 里 `bound to vtable … (active)`。崩溃优先怀疑：绑定时的 flags / 计时器初值（O19）、`ce_play_sound`（O22）。

通过后：MAP 第 7/9/10 段 🔧 → ✅；AUDIT §N / §O 顶部各记一行「实跑通过」；[`CARDS.md`](CARDS.md) 状态列改 ✅。

## 1b. 商店走两遍（2026-09-04，待实跑）

每关过关后商店开 **2 次**（`shop_core.h` 的 `CE_SHOP_VISITS_DEFAULT`），第二次商品重抽（已买的自动排除）、仍是零售流程（必须买一张）。
实现：`native/shop.c` 两个放行断点（`ce_shop_bought` `0x4183ea` 记成交；`ce_shop_reopen` `0x443b05` 在 GameThread 里把 `0x20000` 位加回 eax），
状态机 `shop_core.c`（主机单测）。引擎链：[`engine/card/th18/05-shop-and-money.md`](../../../engine/card/th18/05-shop-and-money.md) §3.5；审计 AUDIT §P。

日志应有：`shop: 2 visits per stage` → 过关 `shop: opened by msg (visit 1/2 …)` → `shop: bought (…)` → **同一帧** `shop: reopen (visit 2/2 …)`
→ 第二家店（进场动画再来一次，商品不含刚买的）→ `shop: bought` → 正常进下一关。`reopen` 行必须是 `bought + 31`，出现 `(late, gap!)` 就是 P4′ 的空档帧真的发生了。练习模式 / replay 回放里不该出现 `reopen`。
空白卡、买不起、暂停后退到标题都不该多开店（AUDIT P6 / P8）。崩溃优先怀疑：`0x443b05` 处 esi 不是 GameThread（P3）。

通过后：MAP §5 与 AUDIT §P 顶部记「实跑通过」。想改次数就改常量（以后可挂 `cards.js`）；
**关卡中间开店**（MSG opcode 36 或 DLL 直接置 `GameThread+0xb0 |= 0x20000`）技术上可行，但 Stage / Spellcard 不冻结，只该在对话里做——见 §3.5。

## 1p. 股市（id 80，2026-09-08，待实跑）

主动卡，不开新断点。`cards_dev.js` 起手最后一格已换成 80（原 67）。实现 `native/cards/stock.c` + `stock_core.c`（主机单测 10 组）；审计 AUDIT §AF。

进关卡后日志应有：

```
sdk: 80 bound (.active_recharge = 30, .ctor = ctor, .on_activate = on_activate, .on_tick_2 = on_tick_2, .on_draw = on_draw, .on_run_reset = on_run_reset, .on_delete = on_delete)
stock: opened at 100.00
stock: panel spawned (bg id … at 1056,862 size 432x180; candles x 848..1264 y 776..872; 2x UI space)
```

体感：**HUD 右下**一块半透明黑底板，里面一排红绿蜡烛每 4 秒多一根、最右那根每 0.25 秒变形；底板下方三行字 `STK 100.00 ^` / `HOLD 0.00 = 0` / `P/L +0`。
选中股市（D 键切到它）→ 场上有敌人时按 C：金钱 −50、`HOLD` 变 `0.50`、`se_cardget` 音、日志 `stock: buy 50 @ 100.00 -> hold 0.5000 (money …)`；
充能条空了约半秒回满。按住 Shift 再按 C：金钱回来（差 ≤ 1）、`se_bonus` 音、`stock: sell …`。钱 < 50 按 C / 空仓按 Shift+C → 无效音 + `card: refused (no cash / no holding)`，充能不掉。
过关进店：面板停住（行情冻结）、持仓还在；下一关接着走。局末 `stock: run reset (…)`，面板消失。
倒吊人摧毁它 → 面板立刻消失（`on_delete`）。

**要调的**（全在 `stock.c` 顶部常量）：底板 / 蜡烛位置若压到卡组第三行图标（装备行 y≈398..430）或超出窗口，改 `UI_*`；
三行字若重叠 / 太大，改 `TEXT_Y*`（HUD 字高按 Momoyo 的 `x1.5` 目测）；蜡烛矩形若以左上角为锚而不是中心，几何要整体偏 `(w/2, h/2)`（AF6）。
崩溃优先怀疑：33 个 UI VM 起不出来（日志 `FAILED to spawn`，AF4）、`ce_anm_set_scale` 被脚本插值器覆盖（脚本里没有 scaleTime，不该）。
行为怀疑：Shift+C 被当成买（AF2：`INPUT_HELD` 与 `INPUT_PRESSED` 同帧一致性）。
反过来的情形（2026-09-08 复审 I1，AUDIT AF14）：**C 被当成 Shift+C**——如果玩家开着「射击时自动低速」这个选项，
按住射击键（几乎全程）会让低速位被录制器强行 OR 成 1，于是按 C 永远走卖出分支，怎么按都买不进。
测试时如果发现「C 键一直在卖、从没买过」，先去关掉这个选项再下结论，不要以为卡坏了。
**已知的日志不精确**（2026-09-08 复审留的观察，不是 bug）：卖出那行日志里的金额永远印 `STOCK_LOT`（50），
而实际成交额在两种情况下会更小 —— 持仓不足 50 金钱等值时是全卖，以及尾数被向下取整吃掉的那部分。
看日志对账时以 `money` 那个字段的前后差为准，别信行首那个 50。

## 1o. 旋风斩（id 79，2026-09-08，待实跑）

被动卡，**不开新断点**：`on_bomb_spent`（腐化那个断点）算账 + `on_tick_2` 调引擎自己的 `do_bomb` 接续（黄昏那套机制）。
实现 `native/cards/whirlwind.c` + `whirlwind_core.c`（主机单测 6 组）；审计 AUDIT §AE。

带满火力（4.00）进关，按 B：

```
sdk: 79 bound (.on_bomb_spent = on_bomb_spent, .on_tick_2 = on_tick_2, …)
whirlwind: bomb at power 400 -> 3 extra release(s), power 400 - 300 = 100 (level changed 1)
whirlwind: extra release #1 (do_bomb -> 0), 2 still owed, bombs 2      ← 三行，一发接一发
whirlwind: extra release #2 (do_bomb -> 0), 1 still owed, bombs 2
whirlwind: extra release #3 (do_bomb -> 0), 0 still owed, bombs 2
```

体感：**一次按 B 连着炸四发**（炸弹演出一发接一发，中间不重叠 —— 引擎自己的守卫决定的），
右上角火力从 4.00 掉到 1.00、子机跟着减档（`repopulate`），**炸弹数只少 1 颗**（额外那三发不花炸弹）。
再按一次 B：火力 1.00 ⇒ 日志 `no extra release (need >= 200)`，只炸一发 —— 这条同时验证了「自带终止」。
火力 2.00 / 3.00 分别应接 1 / 2 发。

要一并看的联动（都不是 bug，AUDIT §AE10/AE11）：与**腐化（70）**同时带 ⇒ 一次按 B 炸弹上限掉 4；
与**真银圣剑（75）**同时带 ⇒ 前两发各算一次挥砍（各回一个残机碎片，耐久每关 2 点用完就没）。

崩溃优先怀疑：`on_tick_2` 里自机指针为空时调 `do_bomb`（已判空，黄昏踩过这个坑）。
行为怀疑：X 算错（看日志 `power %d - %d = %d`）、炸弹数补多补少（看每行末尾的 `bombs %d`）。
卡图 `WHIRLWIND` sprite 160/161（程序生成，`assets/cards/make_whirlwind_art.py`）。

## 1f. 破损核心（id 71，2026-09-05，待实跑）

**装备卡，本 mod 第一张走零售装备卡机制的卡**：身边一颗电球子机（`Player__allocate_option`，引擎管位置 /
聚焦位移 / 进店收起），每 1 秒朝最近的敌人（512 px 内）**瞬间**劈一道电弧，那一个敌人吃 120 伤害
（连续 4 帧 × 30 的定点伤害源 `ce_damage_rect` 钉在目标上 + 电弧 / 火花两条特效 VM；2026-09-06 平衡：原 80 / 2 s 只有零售子机卡的三分之一）。实现 `native/cards/broken_core.c` +
`broken_core_core.c`；设计 `docs/superpowers/specs/2026-09-05-broken-core-design.md`；审计 AUDIT §U。
**不改任何游戏资源文件**（只多 `ability.anm` 的两个 entry + 三个脚本）。`category: 1`：图鉴 / 编成里应排在零售子机卡 REIMU_OP…MAGATAMA **那一段之后**、而不是所有新卡的末尾（2026-09-06 顺序表按类别插入，AUDIT M12；日志 `menu: new card 71 (category 1) at order[…], after id 51`）。
图鉴整体应是：第 2 行 MOKOU 后接强欲之壶、方片 2；第 4 行 MAGATAMA 后接破损核心；第 7–8 行 MUKADE 后接黑桃 ×5、黄昏、加倍、腐化；
第 10 行 RICEBALL 后接反转牌、神之宣告、青眼白龙、炎魔之王；最后一行只有 MAGATAMA2（零售就把它放在最后）。

**顺带解决了 `engine/card/th18/OPEN-questions.md` §1**（装备卡的子机 shooter 数据存在哪）：就在四个 `pl0X.sht`
的 `+0xe0` 偏移数组里，40 项、零售用 23 项、尾部 17 项空着。新开的 `engine/sht/th18/` 两篇是布局与字段图；
`assets/sht/append_shooterset.py` 能往空位追加 shooterset。**这张卡的第一版用过它**（子机真开火），后来改成
定点伤害源——自机弹天生要飞过去，与「瞬发单体」相悖。工具留着（`APPEND` 为空 = 不产出），以后真要「子机连射」的卡再用。

日志应有：`sdk: 71 bound (.on_power_level_change = …, .on_tick_2 = …, .on_load = …, .on_run_reset = …)`
→ 进关 `broken_core: option allocated (ptr …, anm id …)` → 有敌人时
`broken_core: fire #N at frame …, orb (x, y) -> target (x, y) dist … angle …`（只记前 3 发与每第 25 发）。
体感：自机右侧一颗青白电球（慢转 + 亮度呼吸）；每 1 秒一道电弧**瞬间**连到最近的敌人、命中点一团火花 +
电流噪声（`se_noise` `0x46`），敌人掉血（devstage 的 boss 血量 ÷100，看得很清楚）；没有敌人时**攒着不发**，
敌人一进射程立刻劈。进商店电球收起、出店回来；过关不消失（装备卡）。
崩溃优先怀疑：`ce_allocate_option` 的压栈序（U1）、子机槽认领（U3）。视觉怀疑：电弧方向 / 长度（U10、`anchor(1, 0)`）。

## 1c. 神之宣告（id 66，2026-09-04，待实跑）

主动卡：boss 符卡中按 C → 残机减半（向上取整，1 条也能用）→ 符卡立刻按超时结束（血条落到阈值、失败演出、无奖励）。
不在符卡里 / 残机 0 / 没有带超时槽的 boss → 0x10 无效音、充能不消耗。实现 `native/cards/judgment.c`，不开断点；
引擎一手 [`engine/ecl/th18/01-boss-interrupts-and-spellcard.md`](../../../engine/ecl/th18/01-boss-interrupts-and-spellcard.md)，审计 AUDIT O28。
卡图 `JUDGMENT` sprite 134/135（强欲之壶同批 132/133，abcard.anm 已重建）。
**注意**：`cards_dev.js` 的起手卡组只有 5 格，2026-09-06 起是 **71/70/68/72/67** —— 要试 66 / 69 就临时换掉一格。发动演出 script77（`JUDGMENT_FX` entry15 / sprite117，ability.anm 已重建）：卡图铺满弹幕区半透明浮现 → 放大上浮 → 淡出，共 75 帧，日志带 `flash anm id`；发动同时全屏消弹（弹 → 点道具、激光一起消，O28h）。
日志应有：`sdk: 66 bound (.active_recharge = 3600, .on_activate = on_activate)`；符卡里按 C → `judgment: lives 3 -> 1 (cost 2), 1 boss attack(s) expired, spell flags …`
→ 同帧符卡计时 00.00、boss 血条落到阈值、「失败」演出、进下一段；非符 / 无命 / 刚超时那几帧按 C → `judgment: refused (…)` 且充能条仍满。
崩溃优先怀疑：`0x441f10` 的参数顺序（O28c）、`CE_ENEMY_DATA` 0x122c 的推导（O28a 三处交叉）。

## 1d. 青眼白龙（id 67，2026-09-04，待实跑）

主动卡：关卡里按 C → 残机 −1 → 自机上方出现龙（用户原创俯视图，头朝上），跟着自机、并上下轻微呼吸浮动；弹碰到龙变点道具、龙闪一下蓝色、龙下方血条缩短（蓝 → 黄 → 红）；
第 300 帧起每 5 s 龙头向上一道 Master Spark 式白蓝光束 45 帧（魔理沙贴图，宽约 126、判定 96），boss 血条掉一截（devstage ÷100 一波就死）。1500 发后龙放大淡出。
残机 0 按 C → 无效音、充能不动。过关龙消失。设计 `docs/superpowers/specs/2026-09-04-blue-eyes-design.md`，审计 AUDIT O29，
引擎一手 `engine/player/th18/02-damage-sources.md`。`cards_dev.js` 起手卡组已带 67。

日志应有：`sdk: 67 bound (.active_recharge = 600, …)` → `blue_eyes: summoned, lives 3 -> 2, hp 1500, anm id …` →
`blue_eyes: hp N (blocked …)`（有挡弹的整秒）→ `blue_eyes: wave 1 start at frame 300 (hp …, cap C)`（cap 是 sht 的 max_dmg，只作记录：2026-09-06 起光束帧把它抬到 300，一波实打 45 × 100 = 4500；
**验伤害要在自机开着火的时候看**，boss 血条那 45 帧掉的应明显多于自机弹本身的速率）→ `blue_eyes: died after …` 或过关 `blue_eyes: dismissed (stage start) …`；
残机 0 → `blue_eyes: refused (no lives)`。崩溃优先怀疑：O29a/b 的栈参顺序与 XMM、O29c 的 thiscall、`interruptLabel(1)` 是否被 `stop()` 后的 VM 接住
（不接就改成 `ce_anm_delete`）。视觉怀疑：光束是否朝上（子脚本各自 `rotate −90°`，若子 VM 还叠加父旋转也仍是 0 + −90°）、是否从龙头起（父 VM 钉在龙头，子 anchor 左端）；blendMode 9 白核是照魔理沙抄的——坐标假设见 spec §2.3。

## 1i. 炎魔之王拉格纳罗斯（id 72，2026-09-06，待实跑）

主动卡：火力 ≥ 3.00 时按 C → 火力 −2.00（HUD 火力条掉两档、子机重建）→ 先听到召唤语音、自机上方 1 s 内大量橙色火星向一点汇聚（此时还没有本体、不挡弹）→ 三圈爆炸（光环 + 火星 + 小震，间隔 8 帧）中出现炎魔（用户正面像，约 64 px 高，待命时上下轻微浮动、身上不断往上冒火星）+ 血条 + 发动音；
弹碰到它变点道具、它闪一下橙色、血条缩短。出现后第 420 帧起每 7 s：滑到弹幕区**下 1/3** 的一个随机点（60 帧、先慢后快再慢），到位那帧向场上随机一个敌人
投火球（橙红彗星、身后拖尾 + 撒橙白粒子，4 px/帧慢慢飞），**沿途把碰到的弹变成点道具**，落到敌人**当时所在的位置**爆炸（先把半径 96 内的弹清成点道具，再大光环 + 一圈火星 + 画面小震一下 + `0x2c`），那一片（96×96）的敌人掉 **1200** 血（devstage ÷100 一发就死；2026-09-06 加强，爆炸帧把每敌每帧伤害上限抬到 300，否则自机弹会把配额吃满、火球白打，AUDIT V20）。日志每次落地一行 `firelord: blast at (x, y): cancelled N here + M in flight; 12 frames x 100 dmg with cap 300`。**验伤害要在自机开着火的时候看**：boss 血条掉的那一截应明显多于自机弹本身的速率；不抬上限时它几乎不掉。
800 发后放大淡出。火力 < 3.00 按 C → 无效音、充能不动。过关消失。设计 `docs/superpowers/specs/2026-09-06-firelord-design.md`，
审计 AUDIT §V。`cards_dev.js` 起手卡组 2026-09-06 起是 **71/70/68/72/67**（顶掉了 69 加倍——要试 69 就临时换回）。

语音：召唤 `FIRELORD_SUMMON`（id `0x55`）叠在 `0x4d` 上、投球 `FIRELORD_ATTACK`（`0x56`）——**这两条同时也是 §1e 音效表扩容的实跑样本**
（`snd:` 那几行启动就能验掉扩表；语音响不响才是这张卡的事）。

日志应有：`sdk: 72 bound (.active_recharge = 600, …)` → `firelord: summoning, power 400 -> 200 (level changed 1), hp 800, target (x, y), intro 60 frames`
→ 60 帧后 `firelord: appeared at elapsed 60, anm id …` → 每 7 s `firelord: move #N at frame …: (x, y) -> (x, y)` → 40 帧后 `firelord: shot #N at frame …: enemy i/n at (x, y), F frames of flight, angle …`
（没敌人则 `firelord: no target …`）→ `firelord: hp N (blocked …)`（有挡弹的整秒）→ `firelord: died after …` 或过关 `firelord: dismissed (stage start) …`；
火力不够 → `firelord: refused (power too low)`。
崩溃优先怀疑：V1 / V3 的两个 `ret 4`（照 Tsukasa 抄的，但我们是从 SDK 桩里调）、V4 repopulate 在我们的 `on_activate` 里重入广播
`on_power_level_change`（破损核心会在此重申请子机——零售装备卡同款）、`ce_rand` 的 thiscall、V17 震屏工厂的 fastcall + `ret 0x10`（第一颗火球落地那帧崩就是它）。
视觉怀疑：火球方向（script92 不碰 rotate，C 写 `vm+0x44`；贴图朝 +x）、本体是否被 script91 拉回原点（脚本不碰 pos，应不会）、
拖尾是否堆在火球身后而不是身前（起在当前帧坐标、火球下一帧才前进）。

## 1n. 衔尾蛇（id 78）+ 自机弹伤害倍率层（2026-09-08，待实跑）

**两件事一起**：① 把「给自机弹加伤害」提炼成 SDK 的一层（AUDIT §AA），② 第一张用它的新卡（§AB）。
不开新断点、不调新引擎入口、不改任何字节。

**层**：卡声明 `.bullet_damage_permille`（千分比），SDK 连乘成聚合倍率，由**id 最小的参与者**在 `+0x28` 里
对 `bullet+0x9c` 就地乘**一次**（单次取整、与链表顺序无关），并在 `+0x2c` 里把每敌每帧伤害上限
`player+0x47984` 按同一倍率抬起来。启动日志里 K♠ / 古锭刀 / 衔尾蛇那几行应带
`.bullet_damage_permille = 1100 / 1000 / 1000`。
**回归重点**：古锭刀的两行 `guding: final spell ON/OFF` 与血条掉速应与改之前**一模一样**；
K♠ 单带时打 boss 的血条掉速应**略快于**改之前（它以前漏抬上限，+10% 有一部分被配额吃掉）。
两张一起带 = ×2.2，`ce_dmg_scale(90) = 198`（不是逐张取整的 198→…漂移，见 §AA4）。

**HUD 倍率读数**（新原语 `ce_hud_text`，AUDIT §AC）：只要场上有任何一张改伤害的卡且倍率不是 ×1，
**火力值左边**就应该出现一行 `x1.10` / `x1.60` / `x2.20`（与火力同色、同一行 y = 182）。
带 K♠ 进关就能看到 `x1.10`；古锭刀进终符那一瞬间应该跳到 `x2.00`、符卡结束跳回（没有别的伤害卡时整行消失）。
★ **第一次实跑要把位置贴回来**：字号与对齐用的是环境值（只设了字色），所以 `x` 可能压到火力上、也可能太靠左，
调 `engine.h` 的 `CE_HUD_DMG_X` / `CE_HUD_POWER_Y`。崩溃优先怀疑 `ce_hud_text` 的 cdecl 声明（AC1）。

**衔尾蛇**：`_test` 商店里买（价位档 9），或临时把 `cards_dev.js` 起手一格换成 78。
带着它去送命（devstage 最省事）：

```
ouroboros: miss #1 -> stack 1 (bullet damage x1.20), next stack needs 2 more miss(es)
ouroboros: miss #2 -> 1/2 toward stack 2
ouroboros: miss #3 -> stack 2 (bullet damage x1.40), next stack needs 3 more miss(es)
```

第 1/3/6/10 次 miss 各升一层；升层那一下有 `se_powerup`（`0x0d`）。HUD 小卡右下角的数字应跟着变
**0 → 1 → 2**（第一次出现时日志有 `ouroboros: hud digit spawned (icon id …)`——**把坐标贴回来**，
数字偏移与真银圣剑共用 script99，还没实跑调过）。过关数字不清零（「永久」= 一整局）；
局末 `ouroboros: run reset (…)`，重开一局回到 0 层、倍率回 ×1。
血条掉速在 2 层（×1.4）之后应该看得出来。
崩溃优先怀疑：`ce_anm_spawn_ui` 的 `ret 0x10` 第四个哑参（§X1，真银圣剑同款，也还没实跑）。

通过后：CARDS.md 两行 🔧 → ✅；AUDIT §AA / §AB 顶部各记「实跑通过」；MAP.md §5 那条记一行。

## 1m. 豌豆射手（id 77，2026-09-08，待实跑，致敬植物大战僵尸）

装备卡，**本 mod 第一张真走 SHT shooterset 的卡**（`assets/sht/append_shooterset.py` 造好之后一直没人用那条链）。
子机在自机右侧（偏移 `0x28`）向**正上方**吐豌豆：平时 **60 帧一颗 × 90 伤害**（PVZ 原版是 1.425 s ≈ 85 帧，2026-09-08 用户改成整 1 秒），
每 20–30 秒进入 100 帧的**机枪状态**，20 排 × 7 颗 = 140 颗小豌豆（每颗 7，合 980）。综合约 123 DPS，
但**只在按住射击键时打**（`+0x1c` 本来就只在射击时广播，所以「开大」也不会在不开火时白烧）。
不改任何 exe 字节、不加断点；新写入面只有 `pl0X.sht` 尾部的两组 shooterset。审计 [`AUDIT.md`](AUDIT.md) §Z。
`cards_dev.js` 起手卡组没带它 —— `_test` 商店里买（新卡权重 20，很快刷到），或临时把起手一格换成 77。

启动应有 `sdk: 77 bound (.on_power_level_change = …, .on_tick_shooters = …, .on_tick_2 = …, .on_load = …, .on_run_reset = …)`。
拿到卡进关 → `peashooter: option allocated (ptr …, anm id …)` + `peashooter: next gatling in N shooting frames`（N ∈ 1100–1700）。
**按住射击键**：右边那株豌豆射手每 **1 秒**吐一颗大豌豆（约 21 px 的亮绿球）直直朝上飞；
攒够之后 → `peashooter: gatling #1 start at (x, y): 20 frames x 7 peas / 5 frames = 140 peas`，
子机位置一团绿光 + `se_powerup`，接着约 1.7 秒里**一堵 7 列宽的小豌豆墙**滚上去，
结束 `peashooter: gatling #1 done, M peas fired in total` + 下一行新的 `next gatling in …`。
松开射击键：豌豆停、日志也停，**再按住时接着上次的进度**（不是从头等）。
伤害：devstage 的 boss 血条在机枪那 1.7 秒明显掉一大截；平时那颗单独看不出来，**要在自机开着火的时候比**
（Z10 抬上限就是为了让那颗 90 不被自机弹挤掉配额）。进商店子机收起、出店回来；过关不消失（装备卡）。

崩溃优先怀疑：`ce_tick_shooters_for_card` 的压栈序（Z2，**第一次真调用**）、`ce_allocate_option`（§U1，破损核心已走过）。
**开局就崩 / 一按射击键就崩**：先怀疑 `pl0X.sht` 没被 patch 应用到（日志里没有 `option allocated` 却有豌豆 = 不可能；
真没应用的话开火时会去解析零售留空的第 `0x17` 项 → 指向 set 0 → fire_rate 0 → **除零**）。
`dist/patch-step3/th18/pl0[0-3].sht` 四个文件必须在包里（`files.js` 应列出它们）。
视觉怀疑：豌豆太大 / 太小（script100 / 101 的 `scale`）、子机位置（`PS_OPTION_OFFSET`）、
机枪那堵墙太宽 / 太窄（`append_shooterset.py` 里 `PEA_GATLING` 的七个 `spawn_dx`）。

通过后：CARDS.md 🔧 → ✅；AUDIT §Z 顶部记「实跑通过」；MAP.md §5 那条记一行。

## 1l. 古锭刀（id 76，2026-09-07，待实跑，致敬三国杀）

被动「武器」：boss 的最后一张符卡（终符）期间自机弹伤害 ×2，每敌每帧上限同步抬到 sht 基值 ×2；炸弹 / 其他卡不翻。
不开新断点、不改字节、不调新引擎函数。判据：符卡进行中 && boss 活动中断槽子程序名含 `Dead`（ECL 一手：七关 + Extra 道中全覆盖，
[`engine/ecl/th18/01`](../../../engine/ecl/th18/01-boss-interrupts-and-spellcard.md) §6）。审计 [`AUDIT.md`](AUDIT.md) §Y。
`cards_dev.js` 起手卡组没带它——`_test` 商店里买，或临时把起手一格换成 76。**要在零售关看**（devstage 空壳 ECL 的 boss 只有一段攻击）。

应有：非终符阶段日志无 `guding:` 行；boss 进最后一张符卡的那一帧 `guding: final spell ON (boss enemy_id N, interrupt slot 0) -> bullet damage x2, cap 90 -> 180`
（Reimu 90 / Marisa 160 / Sakuya 60 / Sanae 120），血条掉速肉眼约两倍；符卡结束（击破 / 超时 / 神之宣告）→ `guding: final spell OFF (F frames, B bullets …)`。
st01 Mike 第二张符卡最直观；Extra 道中 Tsukasa 只有第三张符卡触发（前两张 `MBoss2` / `MBoss3`）。主线 1–6 关道中 boss 不该出现 `ON`。
局末 `guding: run reset (…)`。崩溃优先怀疑：`find_enemy` 链表（神之宣告同款，已实跑通过）。

通过后：CARDS.md 🔧 → ✅；AUDIT §Y 顶部记「实跑通过」。

## 1k. 真银圣剑（id 75，2026-09-06，待实跑，致敬炉石传说）

被动「武器」：每关前 2 发炸弹是挥砍——炸弹放出的下一帧起 4 帧 × 100 盖满弹幕区的伤害源（上限抬 400），并 +1 残机碎片；HUD 小卡右下角画本关剩余次数。
不开新断点、不改字节；三个新引擎入口都是普通函数指针（UI 列表建 VM `0x409670`、`set_sprite` `0x477b00`、加碎片 `0x457570`）。审计 [`AUDIT.md`](AUDIT.md) §X。
`cards_dev.js` 起手卡组没带它——`_test` 商店里买（新卡权重 20，很快刷到），或临时换掉起手一格。

进关应有：`truesilver: stage start, durability 2` → HUD 那一行的小卡上出现数字 **2**（日志 `truesilver: hud digit spawned (icon id N at x, y, z; digit id M)` —— **把这行的坐标贴回来**，数字偏移要按它调）。
放第一发炸弹 → `truesilver: swing #1 -> durability 1, +400 over 4 frames (cap 400); life fragments L/F -> L/F'`，右上角残机碎片 +1（满 3 进整命、残机已满则不加），
数字变 **1**，boss 血条比单放炸弹多掉一截（devstage ÷100 一发就死，换零售关看）；第二发 → 数字 **0**；第三发 → `truesilver: bomb without durability`，无额外效果。
过关 → 数字回 **2**。进店买卡后 HUD 重建 → 数字仍在（日志再来一行 `hud digit spawned`）。
崩溃优先怀疑：X1（`0x409670` 的 `ret 0x10` 第四个哑参）、X2（`set_sprite` 的 thiscall）、X3（加碎片的 `ret 4` 哑参）。视觉怀疑：数字位置 / 大小（X5：改 script99 的 `pos` 或贴图尺寸）、数字残留（X6）。

通过后：CARDS.md 🔧 → ✅；AUDIT §X 顶部记「实跑通过」。

## 1j. 黄金小丑（id 73）+ 倒吊人（id 74）（2026-09-06，待实跑，致敬 Balatro）

两张都不开新断点、不改字节。审计 [`AUDIT.md`](AUDIT.md) §W。卡图先占位 116/117（等 PNG）。

**黄金小丑**（被动）：过关 → 商店开门那一帧金钱 +20％ 向下取整（无上限）。`_test` 商店里买到它后，下一次过关应有：

```
shop: opened by msg (visit 1/2, stage N, frame F, money M)
golden: interest 20% -> money M + G = M'
```

且商店里显示的钱就是 `M'`（不是 `M`）；重开的第二家店（`shop: reopen`）**不该**再有 `golden:` 行。钱 < 5 时是 `golden: money M, no interest`。

**倒吊人**（即时卡，价位档 3）：店里买它 → 日志 `destroy: card X (flags 0x..) removed; now total T = A active + E equipment + P passive`
→ `hanged: destroyed card X, money M -> M+300 (shop still deducts the price)` → 卡组 HUD 那一行少一张、图标重排、钱 = M + 300 − 价钱。
卡组只有选中的主动卡（或空）时 → `hanged: no other card to destroy, nothing gained`。它可以再刷出（`repeatable`）；被摧毁的卡也能再刷出（`owned` 清零）。
编成里**不该**出现它（`deck_visible: 0`）。删装备卡（破损核心 / 零售子机卡）→ 子机留到关店、出店时按剩余卡重建。
崩溃优先怀疑 W3 / W4（`+0x50` thiscall、`0x408d00` ret 4）；幽灵图标 / HUD 不重排看 W4。

通过后：CARDS.md 两行 🔧 → ✅；AUDIT §W 顶部记「实跑通过」。

## 1e. 音效表扩容 / 语音（2026-09-05，待实跑）

零售音效表是写死的 84 槽（`0x00`–`0x53`），**一个空闲 id 都没有**。四张表（cfg `0x4c9b80`、
wav 名 `0x4b47a0`、slot `0x56c804`、blob `0x56cfe4`）整体搬进 codecave 加长到 116 行 / 104 个 wav 槽，
腾出 **32 个自定义音效 id `0x54`–`0x73`**，第一批装角色语音。51 处 binhack 全是「只换常量」，
唯一手写机器码是 `th18_snd_patch_init` 那 56 字节。引擎一手
[`engine/_shared/th18-sound-table.md`](../../../engine/_shared/th18-sound-table.md)，
设计 `docs/superpowers/specs/2026-09-05-voice-expand-design.md`，审计 [`AUDIT.md`](AUDIT.md) §Q。

语音就是 SE：**可叠加、不做独占通道、不打断**，跟随游戏的 SE 音量。
加一句语音三步：`assets/voice/<NAME>.wav` + `ORDER.txt` 一行 → `patch/th18/voice.js` 一条（带 `id`）
→ `make voice` → 代码里 `ce_play_voice(NAME, x)`。细则 [`assets/voice/README.md`](assets/voice/README.md)。

日志应有（启动时，与是否触发无关 —— **这一段就把扩表本身验掉了**）：

```
snd: caves cfg=<addr> names=<addr> slots=<addr> blobs=<addr>
snd: voice id 0x54 "ROYAL_RAGTIME" -> voice/ROYAL_RAGTIME.wav (405762 bytes, wav slot 72, +0 dB/100, pri 100)
snd: OK 1 voices, 116 rows, I1/I2 hold
```

**听得到的验证在皇家同花顺**：商店里买齐五张黑桃（58–62）→ 第五张成交时触发演出，
日志 `royal: show anm id ... + ragtime`。拉格泰姆从演出第 0 帧起，与 script70 的时间线对齐：
小节 1 底下走五张牌弹出（帧 0/10/20/30/40），**帧 60 金色横幅弹出正好是小节 2 的强拍**
（trophy 音效 `0x4f` 照旧叠在上面 —— 语音就是可叠加的 SE），帧 180 收在 C 上落进 170–194 的淡出里。
Alt-Tab 切出切回再触发一次，号角仍在。退出游戏不崩。

⚠️ 买齐五张黑桃要几关，用 `_devstage`（关卡是空壳）+ `cards_dev.js` 的商店权重会快很多；
**扩表本身不需要凑齐** —— 启动日志那三行就已经证明 116 行表、I1/I2、槽 20 都对了。

通过后：MAP 第 11 段 🔧 → ✅；AUDIT §Q 顶部记一行「实跑通过」。

## 1f. 黄昏（id 68，2026-09-05，待实跑）

被动卡：用掉**最后一颗**炸弹时，那一发结束后**自动再放一发**。不开断点、不改任何引擎字节 ——
`on_tick_2` 盯炸弹管理器 `[0x4cf2b8]+0x30` 的边沿，1→0 时调引擎自己的 `do_bomb()` `0x420360`。
扣数 `0x4574d0` 自带钳 0，所以我们完全不碰 `CURRENT_BOMBS`。审计 [`AUDIT.md`](AUDIT.md) §R。

`cards_dev.js` 起手卡组已带 68（换掉了 58）。把炸弹用到只剩 1 颗 → 放 → 那一发结束时应有：

```
dusk: last bomb spent -> chaining a second one (do_bomb -> 0)
```

画面上炸弹**再放一次**，右上角炸弹数仍是 0。剩 2 颗以上时放炸弹**不该**有任何 `dusk:` 行。
`-> -1` = 被 `do_bomb` 自己的守卫拦下；**一行都没有** = `+0x30` 没被清零（AUDIT R8 那条 🟡），
那就得换成盯别的字段。

## 1g. 加倍（id 69，2026-09-05，待实跑）

桥牌的 Double：**Miss 损失 2 条残机，但敌人掉落的道具全部 ×2**。审计 [`AUDIT.md`](AUDIT.md) §S。

- 掉 2 命挂 `on_death_after_deathbomb`（`+0x0c`）—— 在引擎自己扣命**之前**多扣 1，
  它 `0x45d1a0` 之后两处判的都是 `js`（`< 0`），所以 game over 照常触发。挂 `+0x14` 会绕过判定
- 掉落 ×2 走**新 SDK 事件** `on_enemy_drop_pre`：断点 `ce_enemy_drop` @ `0x430510` 入口
  （thiscall，`ecx` = `zEnemyDrop` = `enemy+0x623c`），在引擎撒附加表之前把 `+0x04` 起的 20 个掉落数翻倍。
  撒的活还是引擎干，各 type 的角度 / 速度不用我们管
- **2026-09-07 修（S10）**：实跑发现杂兵没加倍——杂兵的掉落是 `dropMain` 的主掉落（`zEnemyDrop+0x00`），引擎在调 `0x430510` 之前就撒了，
  之前只翻附加表。现在断点里把主掉落类型补进附加表对应格 +1，引擎再撒一颗。复验：打杂兵每只掉 **2** 颗（第二颗位置略散、同样朝上飞），
  日志每只 `double: main drop type 1 already spawned once, +1 via ex_drop[0]`；boss 的 `BossItem` 掉落仍是 ×2

`cards_dev.js` 起手卡组已带 69（换掉了 59）。打杂兵应看到掉落**明显翻倍**；Miss 一次残机少 **2**：

```
double: miss costs 2 lives (now N before the engine's own -1)
```

残机 1 时 Miss → 直接 game over（**不该**出现残机 −1 还能继续玩）。
`sdk:` 那行要报 `ce_enemy_drop` 断点已应用，否则门会 `FAIL` 并还原。

## 1h. 腐化（id 70，2026-09-05，待实跑）

被动卡：拿到时炸弹上限补满 **7**，此后**放炸弹消耗的是上限而不是当前数**，过关也不回复
—— 一次给满七发，用一发少一发。审计 [`AUDIT.md`](AUDIT.md) §T。

- 新增 SDK 事件 `on_bomb_spent` + 断点 `ce_bomb_spent` @ `0x4203bc`（`do_bomb` 里
  `consume_bomb` 刚返回那一条）。`0x4574d0` 全库只有 `0x4203b7` 一个调用方，所以覆盖面
  不多不少；**同帧改完，HUD 不闪**
- `on_stage_start` 抵消引擎每关的 `CURRENT = min(3, 上限)` 补给 —— 不加东西，只是不让它把预算显示压小

`cards_dev.js` 起手卡组已带 70（`[70, 68, 69, 66, 67]`，腐化 + 黄昏 + 加倍一起测）。

```
corruption: acquired — bombs 7/7 (from now on a bomb spends the MAX, not the count)
corruption: bomb spent the cap — bombs 6/6 left
```

放一发看到 **7 → 6**（而不是当前数 3 → 2）；过关后仍是 6/6（**不该**回到 3）；
放到 0/0 后按炸弹键没反应。`sdk:` 那行要报 `ce_bomb_spent` 已应用，否则门会 `FAIL` 并还原。

## 2. 后续开发（按优先级）

| 块 | 内容 | 接缝 / 依据 |
| --- | --- | --- |
| ~~皇家同花顺~~ | 🔧 已做（`cards/royal.c`，五张共用 ctor，不开断点；金钱 +800 / 命 +2 / bomb +2），待实跑 | AUDIT O26；`ce_owned()` / `ce_add_life()` / `ce_add_bomb()` 进了 sdk.h |
| ~~主动卡基类~~ | ✅ C 键 / 充能 / HUD / replay 计时器透传已进入 SDK，反转牌等主动卡已接入，待实跑 | `04-active-cards.md` §3–§6；实现与契约见 [`SDK.md`](SDK.md) §3、§9 |
| 辅助函数提炼 | 动作出现第二次就进 `sdk.h` | 方案 3 的约定 |
| 图鉴上限 | 新卡 > 71 时才需要：两处 `cmp r,imm8` → cave | AUDIT §M3 |
| ANM 特效 | 🔧 已有第一例：`ce_anm_spawn` + `assets/ability/`（反转牌 script68 亮牌），待实跑；透视真假当场看 | AUDIT O24；`engine/anm/th18/01-vm-instantiate.md` |
| 更多事件 | 按需：击破、符卡开始 / 结束、擦弹… | 每个 = 一个断点 + AUDIT 一条 |
| `truesilver.c` / `ouroboros.c` 补 `on_delete` | 两张卡的 HUD 数字 VM 只在局末 `on_run_reset` 里释放；被倒吊人局中摧毁时数字会在 HUD 上留到本局结束才消。股市（id 80）已经加了 `on_delete` 释放面板 VM，是现成的样板——照抄即可，本次不动这两张卡 | 2026-09-08 复审跟手记录，不是股市这条分支的缺陷 |

## 2b. 开发辅助（`_test`）

`make ecl`：st01–st06 空壳 + boss 血量 ÷100 + 死时掉 300 金（`assets/ecl/make_dev_ecl.py`）→ **独立 patch `th18_card_expand_devstage`**（`patch-devstage/`，启动器里单独勾，不依赖别的 patch；想打正常弹幕就不勾）；`cards_dev.js` 的 `retail_weight: 6` / `new_weight: 20` 让商店基本只出新卡（在 `_test`）。正式包不受影响。

## 3. 工具链与验收

```bash
cd native
make quick          # 主机 C 单测 + pytest，日常反馈
make check          # 资源 / 站点 / 文档 / DLL ABI 完整只读门
make package        # 生成完整 dist 发布包
make step3          # 255 行全套 + 主机单测（cards_def、sdk_core）+ DLL + x87 检查 + files.js
make dllverify
make conflicts OTHERS="<modkit>/thcrap/repos/nmlgc/base_tsa/th18.v1.00a.js …"
make release PUSH=1
```

## 4. 别忘了

- 先读 [`../../LESSONS.md`](../../LESSONS.md)——ABI / 对象模型 / 时序 / 自检 / 崩溃日志的经验都在那。

- 自检门是断点 `ce_gate`，**不要用 `*_mod_post_init`**（AUDIT §H′）。
- **「编译器把什么放在哪一格」的假设都要从上下文重取**（AUDIT §K′、§M6、§O1）。
- 移速类效果只能写 `on_tick_2`（AbilityManager tick 先于 Player tick）；`on_tick` 在复位之前（O7）。
- `+0x14` 死亡槽在复活置无敌计时器**之前**触发；「复活后」的效果用 `on_tick_2` 识别计时器签名（O8）。
- `internal_name` 带 `'\n'` 前缀（N4）；找卡用注册表。
- 装载器 / SDK 全有或全无：任何 FAIL 都还原分配器上界。
- `make release` 只覆盖生成物 + `_test` 的两个 JSON；modkit 里 `patch.js` / README 是手工文案。
- 每条新写入点过 [`../../_template/AUDIT-checklist.md`](../../_template/AUDIT-checklist.md)，追加到 `AUDIT.md`。
