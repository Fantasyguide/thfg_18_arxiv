# NEXT —— 下一个会话从这里开始：实跑行为与资源段

> **版本**：TH18 v1.00a（`th18.exe`，imagebase `0x400000`）。本文裸地址默认属该版本；引用其他版本须写成 `th16:0x…`。
> 交接文档。写给**没有本会话上下文**的下一个会话；读完这一页应该能直接动手。

> ★ **2026-09-12 用户口径**：「按**不能开游戏测试**来算 —— 所有 UI 位置问题、连带 UI 表现与音频，**到时候统一处理**；先做能做的。」
> ⇒ 实现期一切「只有看了/听了才能定」的东西，都用**具名常量 + `❓ 自定`**落地并记进 [`TUNING.md`](TUNING.md)（跨卡规则 + 逐卡清单 + 资产缺口 + 音频表），**不在实现期纠结**；
> 但「能一手核死」的（层号、字段偏移、脚本号语义、数值量纲）**必须现在核到底**，不许拿「待实跑」当借口。

## 0. 现状一句话

一张新卡的 **10 段路全部有了**（[`MAP.md`](MAP.md) §0）：JSON 登记（[`DATA.md`](DATA.md)）+ C 行为（[`SDK.md`](SDK.md)）。
第一批行为卡 = 黑桃 10/J/Q/K/A（id 58–62，`native/cards/`）。**第 7（商店）、9（行为）、10（数据）三段静态审计通过、待实跑。**

| 段 | 状态 |
| --- | --- |
| 1–6 表 / 分配器 / owned / 存档 / 文案 / 图鉴编成 | ✅ 实跑（2026-09-02）|
| 7 商店、10 JSON 数据 | 🔧 待实跑（AUDIT §N）|
| 9 行为（SDK + 黑桃五张 + 强欲之壶 + 反转牌）| 🔧 待实跑（AUDIT §O）← **先做这个** |
| 8 卡图 | 🔧 工具就位（[`assets/`](assets/README.md)）：黑桃五张占位图 sprite 118–127 已进 `_255/th18/abcard.anm`，待实跑 |
| **25 辉针城（id 114）** | ✅ **实跑通过**（第九跑 2026-09-09：无 114 五局 collect2x 零活动、有 114 三笔 settle 全 ×2 —— Z22 成员门「有就起效、没有不起效」闭环）—— 被动：TH14 收点 1:1 复刻（批量档位倍率 + 碎片 4:1 循环）+ 非聚焦命中掉 P；四个新 SDK 事件（§1s） |
| **24 股市（id 80）** | 🔧 **待实跑**（AUDIT §AF）—— 第一张「新机制」卡：C 买 / Shift+C 卖 50 金钱，K 线面板常驻 HUD 右下。**面板坐标 / 字号全是猜的**，实跑第一件事是把它挪到不挡东西的位置（§1p）。顺带 SDK：状态块 512、`on_delete` 回调 |
| **23 旋风斩（id 79）** | 🔧 **待实跑**（AUDIT §AE）—— 被动：放炸弹时额外再连放 ⌊火力⌋−1 发，代价是同数的火力；不开新断点，全用已有的件（§1o）|
| **22 衔尾蛇（id 78）+ 伤害层 + HUD 文字** | 🔧 **待实跑**（AUDIT §AA / §AB / §AC）—— 每 miss X 次自机弹伤害永久 +20％、X 递增；顺带把「加伤害」提炼成 SDK 的一层，K♠ / 古锭刀已迁移（§1n）|
| **21 豌豆射手（id 77）** | 🔧 **待实跑**（AUDIT §AJ）—— 装备卡：豌豆射手子机向上吐豌豆，每 20–30 s 进机枪状态泼 140 颗；**第一张真走 SHT shooterset 的卡**（§1m）|
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

## 0.3 ★ 已知问题（用户 2026-09-10 提交前口径：除这三条外无 bug）

1. **辉针城（114）的 bonus 弹字不参与暂停模糊** —— bonus 走 `ce_popup_mult`（**显式调用引擎的分数弹字**
   `0x464520`/`0x464572`，`collect2x.c` 的 `×0` 档还专门压 −1 + `0x80a0a0a0` 复刻灰字），
   它画在暂停模糊罩（`text.anm` script59）盖不到的那条路径上 ⇒ 暂停时弹字仍清晰。
   同源结论见第 31 轮（ASCII 文字无法被模糊罩盖住）与第 32 轮（改成贴图 VM 的才盖得住）。
2. **新作卡还没有卡图** —— sprite 116/117 与绀珠传临时共用（见 §1s 与 CARDS.md 的季节/绀珠传条目），
   等 PNG；`assets/cards/` 工具链已就位。
3. **波次切分不准** —— 绀珠传（115）的「一段攻击 = 一波」仍会对不上（道中同 `MainSub` 多波、
   boss 非符/符卡边界），第 30 轮已把 `LOLK_MID_BURST_GAP` 调过一次，仍属已知偏差。

## 0.4 ⚠️ 卡全没了？先看 cards.js 的 desc（2026-09-09 踩坑）

`cards.js` 里任何一张卡的字段不合规，**DLL 的自检会在运行时把整批新卡禁用**（日志出现
`FAIL: cards: card "119": desc: at most 6 lines` 紧接 `mitigation: allocator bound restored to retail (56)`
—— 玩家看到的现象是「改的卡全部消失」，而不是只有那张卡坏）。规则见 `native/cards_def.c`
`ce_card_validate`：`desc` ≤ **6 行**、每行 **<64 字节 UTF-8**、非空、**不能含 ASCII `%`**（用 `％`）、
`price_tier` 0..14、`weight` 0..255、sprite 必填。

**已加构建期拦截**：`make cards-check`（`mkfiles.py --check`，已并入 `make check`；`make files/dist/release`
也会跑）—— 不合规直接红着退出，不会再等到实跑。

## 0.5 先读什么

[`README.md`](README.md) → [`DATA.md`](DATA.md) → [`SDK.md`](SDK.md) → [`MAP.md`](MAP.md)（追溯表，**每改必补**）→ AUDIT §N、§O。

**写新卡之前**先过一遍 [`CARD-CHECKLIST.md`](CARD-CHECKLIST.md)（生命周期钩子 / 删卡三路径 / 资源回收 / 实跑验收模板）——尤其那条铁律：**长持续效果必须在 `on_delete` 里收干净**（删卡都发生在商店里，那时卡的 tick 全停着）。

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

## 1t. 绀珠传（id 115，2026-09-09，待实跑）—— 章节系统 + 擦弹减速

被动卡，**不加断点、不改字节**：只读引擎现成字段、写一个 float。实现 `native/cards/lolk.c` +
`lolk_core.c`（主机单测 **91 组**）；设计 `docs/superpowers/specs/2026-09-09-lolk-chapter-design.md`；
一手 `engine/chapter/th15/01`、`engine/player/th15/01`、`engine/chapter/th18/01`、
**`engine/chapter/th18/02`（波次 / 相位信号，2026-09-09 新增）**、`engine/player/th18/02`。

**用户定案（2026-09-09，三次）**：
① 波次 = **一段攻击 = 一波**（非符一波、符卡一波），**进关底 / 道中 boss 也算一波**，道中按 ECL 524 切章；
② 减速 = **擦弹判定框里有敌方子弹就一直慢**（不是「擦弹那一刻」）；
③ 击破率三来源：道中 = 击破数/刷出总数（分母 0 → 100％）；boss 非符 / 符卡 = 击破 100％、超时 0％；
   **时符（耐久符卡）恒 100％**。

启动应有 `sdk: 115 bound (.on_tick_2 = …, .on_draw = …, .on_stage_start = …, .on_run_reset = …)`。

进关：

```
lolk: wave 1 start (mid nonspell, chapter 0, sig 0, graze/slow 100%)
lolk: wave 1 (mid nonspell) settled: graze N, killed K/S = R%, bonus B, score +B/10, frag 0x11
lolk: wave 2 start (boss nonspell, chapter 41, sig 131073, graze/slow 100%)
lolk: wave 2 (boss nonspell) settled: …
```

**体感（六条一起看）**：
1. **只要弹幕进入擦弹判定区，场上的掉落物就明显变慢**（×0.3），弹离开约 0.12 s 后恢复；
   密集弹幕里应当**一直是慢的**（判定区按子弹自己的模式：圆弹 ≈ 43–50 px、方弹 ≈ 25.5 px，spec §4.1）；
2. **弹幕进圈变黄、飞出圈还原原色**（TH15 `th15:0x419d3d` 的黄 `0xffffd080`）；同一时刻自机周围
   出现一圈环（TH15 `script27` 复刻：反向减算的暗环、30 帧淡入、12 帧由大变小、12 帧渐隐，
   落定半径 44 px ≈ 判定圈；日志 `lolk: graze ring spawned`）；
3. **波次面板** = TH15 `front02.png` 的**六条**（五条标签 + 碎片标签），按 TH15 的绝对坐标摆
   （(448,402) 起、`resolutionMode(2)` 下 ×0.5），只在结算后画、保留 300 帧；标签文字是 TH15 的，
   本卡只画数值（五行右对齐，x = 300/308/300/300/300、y = 226/246/266/286/296、font 2 / group 2）；
4. **数字跳动**照 TH15：面板出现 90 帧后，击破率那行**往下减到 0**、章节奖那行**从 0 加到 bonus**、
   每 4 帧一声 se_kira01、跳完 se_bonus（`lolk.c` 的 `LOLK_ROLL_START` / `LOLK_SE_TICK` / `LOLK_SE_DONE`）；
5. **bonus ≥ 1,000,000 时**面板下方多出一条碎片标签（TH15 sprite150 = B 碎片，普通模式口径），
   并在自机上方弹出一个碎片道具（日志 `frag 0x11`）；
6. **主菜单/标题一个字都不画**（`waves_total == 0` 直接返回 + 门用 `GAME_THREAD_PTR`）。

**要调的地方**（都在 `lolk.c` / `lolk_core.h`）：
- 面板六条的坐标 `LOLK_PANEL_AX/AY` 与 `LOLK_PANEL_DX/DY`（UI 空间 = 文字空间 ×2）；
- 五行文字的 `LOLK_VAL_X / LOLK_VAL_X_RATE / LOLK_VAL_Y / LOLK_VAL_DY`（文字空间）；
- 面板保留帧数 `LOLK_HUD_HOLD`（现 300，TH15 是 290/220 两档）、跳动起点 `LOLK_ROLL_START`（现 90）；
- 章节奖量级：擦弹 200 × 100％ → bonus 1,000,000 → 分数 +100,000。嫌大就调 `LOLK_BONUS_COEF`（现 50）。

**崩溃优先怀疑**：`ITEM_MANAGER_PTR` `0x4cf2ec` / `BULLET_MGR` `0x4cf2bc` 为空（都已判空）、
HUD 文字在非 `on_draw` 里调（已按 SDK §14 放对）。
**行为怀疑**：
- 面板位置仍偏左 / 偏右 → sprite 锚点与画布：六条贴图是 **256×32、内容居中 192**（`make_lolk_ring_art.py`），
  偏了就先确认 TH18 的 UI anm 是不是中心锚；
- 结算从不出现 → 波边界三信号（`ENEMY_MGR+0x48` boss_ids、`zEnemy+0x6374` 中断槽、`0x4ccce4` 章号）；
- 击破率恒 100％ → 超时位没读到（`zEnemy+0x635c` bit24 / `SPELLCARD+0x78` bit7）；
- 时符给 0％ → `SPELLCARD+0x78` bit3 判据（ECL 542 置的耐久位）。

通过后：CARDS.md 状态 🔧 → ✅；AUDIT §AK 顶部记「实跑通过」；MAP.md §5 记一行。

## 1u. TH16 季节 / TH17 动物灵：TH18 里有没有残骸（2026-09-10，已查完）+ 季节占位卡

用户点名两套机制 + 各自的四个部件，逐个查过。**结论：两套在 TH18 都没有可复活的残骸**
（不是「半死」，是「不存在」——槽、字段、vtable、item type 全无）：

| 检查项 | 结论 | 一手 |
| --- | --- | --- |
| TH16 掉季节点 | ❌ 无 item type 16 handler | `th18:0x445a80` type 分派全 case |
| TH16 季节子机 | ❌ `subseason_options` 槽位 → `equipment` | 三版本 `zPlayerInner` 对账 |
| TH16 子机升级 | ❌ 无等级表 / `get_season_gauge_fill_ratio` | 结构体 + 常量普查 |
| TH16 季节解放 | ❌ 单个 `BOMB_PTR`、zBomb 无 `is_season` | statics + 结构体 |
| TH17 动物灵出现 | ❌ 无 `GoastManager` / `zToken` / 全局指针 | funcs 729 / statics 154 全表 |
| TH17 收集槽 | ❌ 无 `TOKENS[5]` / `HYPER_FILL` 对应字段 | `zGlobalsInner` 逐地址引用普查 |
| TH17 咆哮模式 | ❌ 无 `HYPER_TIME` / `HYPER_FLAGS` | 同上 |
| TH17 中弹只退咆哮 | ❌ 引擎无此分支；**通用机制活着**（deathbomb + `th18:0x45d590` + 卡钩子）| `Player::die` 反编译 |

→ 两套都只能自建。审计文档：[`engine/season/th18/01`](../../../engine/season/th18/01-season-residuals.md)、
[`engine/animal/th18/01`](../../../engine/animal/th18/01-animal-residuals.md)；
设计 [`specs/2026-09-10-season-and-animal-design.md`](../../../docs/superpowers/specs/2026-09-10-season-and-animal-design.md)。

**已做：季节卡（id 116，`native/cards/season.c` + `season_core.c`）—— 装备卡（2026-09-09 按 TH16 一手重做）**
- 一手依据：[`engine/season/th16/01`](../../../engine/season/th16/01-season-data-and-tables.md)
  （TH16 脱壳版 exe 反编译：阈值 / 10 张释放表 / 掉落三通道 / 消耗规则）；纯逻辑 `season_core.c`，
  单测 `tests/test_season_core.c`（**59 断言**）；
- ① 季节点**三条通道**：**擦弹**（读原生计数 `th18:0x4ccd0c` 的每帧增量，上限 2/帧）、
  **敌人掉落**（`on_enemy_drop_pre`）、**解放消弹**（`ce_cancel_radius` 返回值每 8 颗 1 点）；
  季节点是**本卡自己的 anm VM**（`ability.anm` script 115 + **TH16 零售切图** `bullet/item.png`
  右上 2×2 = 春/夏/秋/冬 + `(224,32)` = 土用，各 16×16，与 P 点/蓝点同尺寸，按副季节换 sprite；
  出场动画照 TH16 script129：加算 / 10 帧淡入 / 16 帧 2 倍→1 倍 / 随机自转 / 之后 300 帧缩到 0.7 淡到 alpha128），
  落体→靠近被吸→16 px 内收；**不再借 TH18 的分数转换点**；
- ② 按 TH16 阈值 **100/230/390/590/840/1140** 分 0..6 档，
  满槽不收、升档才响（照 `item_collect_season` `th16:0x43de50`）；
- ③ **季节子机**：档位 = 子机数（最多 3 颗），**阵位照 TH16 副 sht 的 option_pos**（每档一套独立阵型：
  秋 ±48 最宽、冬竖线、其余小扇），写 option 记录的 `+0x64/+0x68/+0x6c/+0x70`（零售卡 `th18:0x40b8c1` 同款写法），
  走 shooterset `SEASON`
  （`CE_SHT_SET_SEASON = 0x19`，弹贴图 `SEASON_SEED`，脚本 113/114）；
  **越攒越快**：`帧数 × 档位` 当 short_timer，fire_rate 120 ⇒ 间隔 120/60/40/30/24 帧；
- ④ **季节解放 = A 键**：按副季节查 TH16 原表（半径 / 持续帧 / 每帧伤害 / 无敌帧），
  持续期间每帧半径清弹 + 一个伤害源；**春/秋/冬 清槽、夏/土用 只扣一档**；
  秋释放期间移速 ×1.5（`th18:0x477ec`，乘不赋值）、冬释放期间自机弹伤害 ×1.5
  （`ce_set_bullet_damage_permille`）；
- ⑤ **副季节 = S 键**循环 春→夏→秋→冬→土用；HUD 一行 `SEASON <名> L<档> <点>/<下一档>`
  （**纯 ASCII** —— 游戏字体不吃 UTF-8 中文，别在 `ce_hud_text` 里写中文）；
- 卡图 sprite 116/117 临时与绀珠传共用。

启动应有 `sdk: 116 bound (...)`。实机看：擦弹/杀敌掉点 → `season: level up -> 1/6 (...)` + 身边多子机
→ 按 S 切副季节（`season: sub-season -> Fall`）→ 按 A `season: release #1 (Spring L2: r=100, 13 frames, 100 dmg/frame, invuln 73; points 230 -> 0)`
→ 清弹圈 + 无敌 + 子机收起 → HUD 归零。

⚠️ **A/S 键不进 replay 输入流**（回放只录 `th18:0x4ca428` 的 16 位掩码）→ 用 A/S 触发的效果回放里不重现；
要 replay 安全得改用掩码里的键（C/D）或让卡往掩码里写一个空闲位（顺序未验 🟡）——
一手见 [`engine/input/th18/01`](../../../engine/input/th18/01-key-mapping.md) §4a。

**2026-09-09 实跑第二轮（用户四点反馈）**：
1. ✅ **子机完全不出现** —— 根因在 SDK：`ce_option_drop(c, o)` 一律删 `card+0x1c` 那个 VM id，
   而引擎每次分配都会覆盖 `card+0x1c`（`th18:0x40a906`）→ 季节卡 `alloc_options` 里
   「收起没分配过的槽」把**刚分配好的那颗**删了。改成**按槽自己的 `option+0xb0`** 删
   （`th18:0x40a909`，分配器自己也用它清旧 VM）。
2. ✅ **左下角样式**：改成 TH16 原版三件套 —— **季节字**（零售 `front00.png` sprite82+SUBSEASON，
   新增 `GLYPH_*.png` 五张 + script117）+ **长方形进度条**（script118 外框 `drawRectBorder(102,10)`
   + script119 填充 `drawRect(100,8)`，宽度写 scale.x = 填充比）+ **档位数字**（游戏字体）。
3. ✅ **掉落量**：TH16 小妖精是 `dropSeason(60,8,1)`（60 帧内杀掉 8 个）→ 卡里改成
   **一颗点 = 8 点**（`SEASON_POINTS_PER_ITEM`，见 `engine/season/th16/01` §9b 的全量普查）。
4. ✅ **出场过程**：出生白色、不可收（`SEASON_POINT_TANGIBLE 18` 帧）→ 褪白变彩色才可收；
   擦弹那颗**从被擦的弹上斜向上外抛**（扫弹幕池找刚被擦的弹，沿「弹 − 自机」方向）。

**2026-09-09 实跑第三轮（用户三点反馈）**：
1. ✅ **子机泄漏 + 贴图不对** —— `alloc_options` 每次升档都**重新申请**（旧的槽还活着）→ 池里堆出一串
   泄漏的子机，表现就是「一个定在原地、一个跟着跑」。改成**只补缺的**：已有的用 `ce_option_claim`
   复用、只更新阵位与贴图，多余的才 `ce_option_drop`。贴图也换成 **TH16 原版**（自机贴图里切的
   16×16 小球，`OPT_*.png` 五张，按副季节 `ce_anm_set_sprite` 换 —— 一手：
   `PlayerInner__repopulate_options` 按 `DAT_00492be0[SUBSEASON]` 选脚本）。
2. ✅ **擦弹掉的季节点不自动收集** —— TH16 里擦弹道具要自己去捡；现在擦弹通道撒的点
   `pad = 1`（不参与 96 px 吸附，只受 16 px 接触收集），敌人掉落/消弹的仍会被吸。
3. ✅ **杀敌不掉季节点** —— `on_enemy_drop_pre` 原来要求「这个敌人有掉落才补」，而很多小怪
   drops 全 0 → 一颗不掉。TH16 的 `dropSeason` 写在**每个敌人自己的** ECL 子程序里（小妖精
   `(60,8,1)`），所以改成**每个敌人死亡都掉**。

**2026-09-10 实跑第四轮（用户六点反馈 + 一个研究问题）**：
1. ✅ **切副季节子机不换样式** —— S 键分支只改了 `subseason`，已有子机没动。新增 `refresh_options()`：
   切季时对每颗已认领的子机重写阵位（TH16 的 option_pos 本来就按副季节分表）+ 换 `OPT_*` 贴图。
2. ✅ **季节点从敌人位置掉** —— `on_enemy_drop_pre` 的 `counts` 就是 `zEnemyDrop+0x04`，反推
   `enemy = counts − 4 − 0x623c`（新常量 `CE_ENEMY_DROP`），掉落点 = `ce_enemy_pos(enemy)`。
3. ✅ **像 P 点/金钱一样下落** —— 运动学改成**逐条照抄原版道具**（`th16:0x42f4e0` = `th18:0x445a80`，
   两作同款代码同一组常量）：重力 0.03、下落上限 2.0、下落时水平速度清零、出界 472/200、
   出生随机方向（TH16 `[0x494510]` = 1.2f ⇒ 速度 1.2..2.2）、吸引初速 = 磁速/3 后每帧 +0.2 到 12.0。
   收集半径 / 吸引半径 / 磁速**直接读 `player+0x47988..94`** ⇒ 零售卡 ITEM_CATCH(21) / ITEM_LINE(22)
   改参数时季节点跟着变。收集 +10 分（TH16 `case 0x10` 尾 `add_to_score(10)`）。
4. ✅ **掉落量对齐 TH16 + boss 一大把** —— 一颗点 = **1** 点（原来 1 颗折 8 点）；小怪掉 **8** 颗、
   boss/道中 boss 掉 **200** 颗（判据 = EnemyManager 的 boss id 表 `em+0x48`）。池放到 **DLL 静态区 96 槽**
   （`ce_state` 只有 496 B 装不下），池满进队列、每帧撒 8 颗（boss 那 200 个 ≈ 0.4 s 撒完）。
5. ✅ **过回收线会被吸** —— 每帧**读** `player+0x47998`（不写死 128）⇒ 零售卡 ITEM_LINE(22, Kanako)
   把它设成 224（`CardKanako__on_load` `0x40c270`，回收线**下移**）时我们的季节点跟着变；
   另三门照原版：放 B（`bomb+0x30 == 1 && bomb+0x38 < 0x3c`，新 `CE_BOMB_COLLECTING()`）、
   对话中、**本卡季节解放期间**（TH16 的副季节炸弹那条门是 `timer < 10000` = 整个解放期都吸）。**擦弹掉的那颗也吃这条**（原来 `pad=1` 不参与吸附，
   已删 —— 一手：TH16 的吸附门只对 state 2 的上抛道具豁免 type 0x10，敌人掉落照常被吸）。
6. ✅ **HUD 三件套真的看得见** —— 两个 bug：① UI VM 坐标空间是 **640 字面量 × 2**（AUDIT AF7），
   原来按 640 写 ⇒ 全跑到屏幕外；现在走 `SEASON_UI()`。② 季节字原来用的是 **PT_\*（16×16 道具图）**，
   改成 **GLYPH_\*（32×32 季节字）**。进度条 = script118 外框 + script119 填充（scale.x = 档内进度），
   数字贴在条右端（`ce_hud_text` 仍是 640 空间，不乘 2）。
7. ✅ **季节点会自转** —— 用户观察正确：script115 原来只写了 `rotate(%RANDRAD)`（随机初始角、不转），
   缺 TH16 script129 的 `fsetMul(%F0,%RANDF2,0.08726646f)` + `angleVel(0,0,%F0)`
   ⇒ 0..5°/帧、方向随机。另外把 `alphaTime/scaleTime` 的缓动模式从 4 改回 TH16 的 0。
8. ❓ **「能不能借 P 点/金钱的道具函数」——查清了，见
   [`engine/season/th18/01`](../../../engine/season/th18/01-season-residuals.md) §1d**：
   引擎侧**可以**（收集分派 `0x44625f` 起的 switch **没有 default case** ⇒ `item+0xc78 = 0` 的道具
   照常飞/收、只放 `se_item00` 不产生副作用；SDK 的 `on_item_collect` 断点就在 `mov edx,[edi+0xc78]`
   前一条，能改类型）；卡住的只有外观（贴图与脚本都绑在 `bullet.anm` 的类型表上，要么补丁 bullet.anm、
   要么加一个「给 VM 重指脚本」的引擎入口）。本轮**没动**，仍走自建 VM + 照抄运动学。

⚠️ 上两轮的两条已被本轮取代：**掉落量**（原来 1 颗折 8 点 → 现在 8 颗 × 1 点）与
**出场过程**（原来出生白 18 帧不可收 + 擦弹点不吸附 → 现在 VM 色就是无染色、第 0 帧可收、全都会吸附；
「白色」的出处其实是 `th16:0x430960` 尾把 VM 色写成 `0xffffffff` = **不染色**，不是画成白色）。

**2026-09-10 实跑第五轮（用户六点反馈）**：
1. ✅ **擦弹不再产生季节道具** —— 根因不是没生成，而是**当帧就被收走**：TH16 的擦弹道具出生在
   **弹的位置**（`th16:0x444e03` 的实参 = 弹坐标、角度 atan2(弹−自机)、速度 **1.9**），弹在自机
   擦弹半径上（≈20 px），而 TH18 的收集半径是 30 px（读自机字段）→ 照抄就看不见。
   现在擦弹通道单独走 `spawn_point_graze()`：方向照抄 TH16 的外抛方向、速度 1.9，
   出生点沿该方向推到**收集圈外 6 px**（仍在吸引圈内，会被吸回来）。
2. ✅ **HUD 挪进游玩区** —— 游玩区在 640 字面量空间是 x 32..416 / y 16..464，原来季节字写在
   x=8（黑边里）。现在整组 HUD 在 `(44, 428)` 起：季节字 32×32、外框 102×10、填充 100×8，
   底边对齐 y=444（离游玩区底边 20 px）。
3. ✅ **子机上限 3 → 6** —— 这是**我们的 bug**，不是设定：TH16 就是「档位 = 子机数」
   （`num_season_options = season_lvl`，最多 8 槽；阵位表按 `{0,1,3,6,10,15,21,28}` 累计，
   档 6 = 第 16..21 个阵位）。`SEASON_OPTS` 改成 6，`opt_dx()` 退路也跟着排到 ±72。
4. ✅ **进度条不显示** —— 脚本 118/119 漏了 `sprite(-1)`（VM 没有可画的矩形 → 整条不可见）。
   现在照零售 `stock_rect` / `front.anm script20` 的写法补齐 `sprite(-1) + zWriteDisable(1) +
   blendMode(0)`；外框用**同尺寸实心 `drawRect(102,10)` 打底**（TH18 的 anm 里没有一处用
   `drawRectBorder`，虽然 opcode 612 在 v8 表里），填充 100×8 盖上去，露出 1 px 边框。
5. ✅ **档位数字位置** —— 贴在条右端（字面量空间 `(136, 437)`，`ce_hud_text` 不乘 2）。
6. ✅ **季节解放没有视觉效果** —— 补上 TH16 的两个圆：`pl00sub.anm` **script3**（layer13、
   `blendMode(2)` 减算、灰→绿）与 **script4**（layer12、`blendMode(1)` 加算、红），
   都是 `drawPoly(1.0f, 32)` 的实心圆。TH16 把半径/时长写进 VM 脚本变量（`+0x4b0/+0x4b4` = `%F0/%F1`、
   `+0x4ac` = `$I3`）由脚本自己扩；我们这边卡每帧写 scale + `color_1` 的 alpha，时间轴照抄：
   前 10 帧 0 → 半径、之后 255 → 64（250 帧）、最后 20 帧淡到 0。
   ★ 顺带修了一处**语义**：TH16 的清弹/伤害圆心 = **特效 VM 的位置**（= 释放原点），
   只有**秋**每帧把两个 VM 同步到自机（`BombSubFall__on_tick` `0x40eeb0`）—— 我们原来所有副季节
   都跟着自机跑，现在改成只秋跟着。

**2026-09-10 实跑第六轮（用户问：季节点会被绀珠传卡减速吗）**：
✅ **会了** —— 之前不会：季节点是自建 VM，而减速因子只被引擎的原生道具 tick 乘进去。
现在 `points_tick` 每帧读 `CE_ITEM_SLOWDOWN`（`itemmgr+0xe6bb14`），**只把飘落态的位移与重力增量**乘上去
（吸引/真空飞行不乘，与原生 state 3/4 一致）。绀珠传卡（115）每帧写 0.3 时，同一屏上的 P 点、
金钱与季节点会一起慢下来。
⚠️ 一手对照：TH16 的同名因子（`itemmgr+0xe4b974`）也**从不被写 0.3**（`0x418597` 写 1.0、
`0x4300ab` 是 +0.1/帧 恢复）→ TH16 的季节点本来就不减速，这条只在同时装了绀珠传卡时才有区别。

**2026-09-10 实跑第七轮（用户三点反馈）**：
1. ✅ **擦弹还是不出季节点 / 出生即被收** —— 根因找到了：`season.c` 的弹池常量写错
   （stride **0x680** 应为 `CE_BULLET_STRIDE` = **0xfa0**、state **0x10** 应为 `CE_BULLET_STATE` = **0xf68**）
   ⇒ 「扫擦到的那颗弹」永远扫不到 → 每次都走退路 `spawn_point_around_player()`，
   而退路把点撒在**自机上方 16 px**（30 px 收集圈内）→ 当帧收走。
   现在弹池一律用 engine.h 的常量，退路也推到收集圈外 6 px。
2. ✅ **擦弹时的抖动** —— 照 TH16/TH18 道具 tick：`factor < 1.0` 且道具年龄 > 31 帧时，
   画出来的位置加 ±1 px 随机（`th16:0x42fad2`–`0x42fb35`）。季节点现在在绀珠传卡减速期间会抖。
3. ✅ **子机弹的样式/行为** —— 查了 TH16 五个副 .sht（见
   [`engine/season/th16/01`](../../../engine/season/th16/01-season-data-and-tables.md) §5.5 的一手表）：
   春 = 寻的（初速 −0.2、func init1/tick1）、夏 = 直飞 10 速 15 伤、秋 = 匀加速（tick5）、
   冬 = 激光（init2/tick2/hit2，15 帧一发）、土用 = **向后** 24 速、每 3 帧一发。
   落地：`append_shooterset.py` 追加 **5 组** shooterset（0x19..0x1d），`season.c` 按副季节选组、
   每颗子机各发自己那发、节奏改回引擎计时器（不再 `帧×档位`）。
   ⚠️ 未复刻：TH16 每档的伤害/角度微差、每条 shooter 的 opt 扇（现在所有子机同角、伤害取档 1）。

**2026-09-10 实跑第八轮（用户六点反馈）**：
1. ✅ **季节点「变黄不回来」** —— 用户点明：这是**两卡协同**的三件套（变黄 + 抖 + 减速），
   而引擎只替**原生道具**做：`ITEM_MANAGER+0xe6bb14` 被绀珠传写成 0.3 后，引擎自己的道具 tick
   在「减速中且道具年龄 > 31 帧」时给道具 VM 染色 + 位置写 `randf(−1,1)`，减速退出写回无染色
   （`th16:0x42fa8c`–`0x42fb35` = `th18:0x445a80` 同段）。季节点是本卡自建 VM → 三件套得自己复刻：
   减速 ✓（上一轮）、抖动 ✓（上一轮）、**染色**（本轮补）——颜色用绀珠传给弹的同一支黄
   `0xffffd080`，门与引擎一致（`slow < 1.0 && age > 31`），退出时写回 `0xffffffff`。
   ★ 之前的误判：以为「季节点被绀珠传染黄」，其实绀珠传只写弹池；缺的是**我们自己**没写这步。
2. ✅ **冬的激光是「横着的不知道啥玩意」** —— 因为激光用了 32×32 的种子贴图被沿束向拉伸。
   现在照 TH16 `pl01sub.anm` entry0 sprite0（`player/pl01/pl01.png` 的 256×30 横条）
   切出 `SEASON_LASER.png`（256×32，entry61）+ script122，冬的 shooter 改用它。
3. 🟡 **子机弹的贴图** —— 冬的激光已换（见上）；其余副季节的弹**仍用 `SEASON_SEED`（程序生成）**，
   TH16 那几种弹的实际贴图还没定位（副 .sht 的 `anm` 字段 = 0/3/6/9/12/15 指的是**脚本号**，
   不是 sprite；`pl00.anm` script0 是自机行走、`bullet.anm` script0 是 `alpha(0)` 空弹）
   → 下一步按「sub-sht anm 字段 = 哪个文件的脚本」再查一轮。
4. ✅ **擦弹点「几乎瞬间被收」且不像从弹上发出** —— 两处：① 扫描时没排除**收集圈内**的弹
   （被擦过的弹可能已经被自机贴住）→ 现在只认圈外 ≥ collect_r+6 的弹；
   ② 擦弹点加了 **16 帧不吸不收**窗（`SEASON_POINT_GRAZE_GRACE`）—— TH16 的吸引盒比 TH18 的
   70px 环小，道具本来能飘一会儿，TH18 的环会当帧拽走。❓自定等效，实跑看手感。
5. ✅ **擦弹点比敌人爆的大** —— 同 4②：出场缩放（2→1 倍，16 帧）没播完就被收；有了不吸窗就播得完。
6. ✅ **道中 boss 被击破后季节点生成三波** —— `on_enemy_drop_pre` 在
   `Enemy__drop_items_and_notify_cards` 入口，同一个敌人的死亡序列**会被调多次**，每次都撒一整批。
   现在按「敌人指针 + 30 帧窗口」去重。

**2026-09-10 实跑第十轮（用户两点）**：
1. ✅ **季节点从后方生成（弹幕只在前面）** —— TH18 的 `bullet+0x20 |= 4` 是**一生一次**的擦弹标记
   （`0x424984`），所以「最近的已擦弹」可能是很久以前擦过的、甚至已经在自机背后。
   改成用**引擎自己的擦弹判据**（`engine/player/th18/02` §1b：圆弹 `d² < (r+max(br/2.5,40))²+br²`、
   方弹判定框 +24 px）挑出**当前在擦弹区里**的弹，取其中**离自机最远**的那颗（= 刚跨进边界的）。
   仍然排除收集圈内的弹（否则当帧被收）。
2. ✅ **绀珠传面板「玩家贴近渐隐」**（本卡自定，TH15 原版没有；用户：绀珠传把波次拉长，面板挡视线）——
   面板几何 UI (448,402) 起、六条 256×32 → 世界坐标 x −32..128 / y 185..321，各放 32 px 余量；
   玩家进框 → 所有面板 VM 的 alpha 写 76（0.3），出框写回 255。每帧在 `on_draw` 里刷。

**2026-09-10 实跑第十一轮（用户两点）**：
1. ✅ **季节点位置不对（从后方生成）→ 改用引擎自己的擦弹调用** —— 新增第 10 个断点
   `ce_graze` @ **`0x42497f`**（`0x42497a call Player__do_graze` 刚返回那一条 `mov eax,[0x4cf2bc]`，
   5 字节绝对寻址；`esi` = 刚被擦到的弹、位置 `esi+0x638`）。SDK 加 `ce_graze_take(&x,&y)`
   （取「自上次以来」的擦弹次数 + 最后一次的弹坐标），天空璋卡用它撒点 —— 不再扫弹池猜
   （TH18 的 `bullet+0x20 |= 4` 一生一次，「最近的已擦弹」可能是自机背后的老弹）。
   生成点：`sites.py` 的 `SDK_SITES` + `sdk.c` 的 `BP_ce_graze`/bps[] + `sdk.h` 声明；
   `make gen` 重新生成 patch / sites_gen.h（自检 expected 字节对不上就红）。
   ⚠️ **踩坑（用户实跑：整批新卡消失）**：新断点的 handler 还必须加进 `th18_card_expand.def`
   （`BP_ce_graze @23`）—— 没导出的话 thcrap 绑不上 → `FAIL: breakpoint ce_graze not applied`
   → SDK 自检失败 → `cards_dev: start_deck disabled` → **所有卡都没了**。
   同时 `Makefile` 的 `dllverify` 把导出数从 22 改成 23。
2. ✅ **绀珠传面板数字的字形** —— TH15 画那五行时**只设 group = 2，不设字体**
   （`th15:0x436af5`–`0x436b13`，`+0x19244` 没碰）→ 用的是**环境当前字体**（用户观察：
   ascii 贴图里 bonus/history 上面那一排、约 3/4 处）。`LOLK_FONT` 从硬写 2 改成 **-1 = 用环境值**。

**2026-09-10 实跑第十二轮（用户四点）**：
1. ✅ **冬子机的激光损坏内存** —— TH18 的激光（func 2 族：init2/tick2/hit2）是给**主炮**准备的
   （要 option 占用表、`+0x26` 命中特效脚本号等一整套），走装备卡子机路径会把主机体弹幕定住、画成
   白色抽动块。**先把冬退回直飞快弹**（speed 26、每 15 帧），激光要专门做（记在这里）。
2. ✅ **左下角微调**：季节字 x 44→**34**、进度条 x 80→**70**、档位数字 136→**122**、整体 y +8
   （428→436，条 447、填充 448、数字 445）。擦弹点不再盖住进度条。
3. 🟡 **chapter 字体**：TH18 的字体是**两段**（页 `mgr+0x2c` + 字形基址 `+0x19244`），
   已加一次性日志 `lolk: ascii env page=… font=… group=…`；拿到三个数再定字体（猜的 7/11 都不对）。
4. ~~🟡 暂停时额外 UI 不被模糊罩住~~ ⇒ ✅ **2026-09-12 一手定案并修好**：`0x4882b8` 只把 layer 24–32 折到组 37–45、**其它层兜底折到 39**，而模糊罩是**层 38** ⇒ **只有 layer 24/25 会被罩住**；`99/104/105` 三个 HUD 脚本原在 layer 26（= 组 39）已改 25、`ability.anm` 重建并 verify 通过。规则与现状表见 `AUDIT.md` §BD 与 `HUD-LAYOUT.md` §4.1。
   或找到暂停 shader 的覆盖范围，待做。

**2026-09-10 实跑第十三轮（用户四点）**：
1. ✅ **世界层坐标不对 → 回退 UI 层**（相机 1，x 原点 = 表面中心 −640）。世界层的 2D 空间与 UI 层
   不是简单 ×2 关系（实测：缩小一半、横向居中、纵向各偏自身高度），换算没吃透 → 先回退到已标定正确的
   UI 层 20/21。**暂停模糊的代价**：UI 层不模糊（用户实测层 20/21 也不模糊，模糊只作用在层 3–19 的
   游玩区 surface）→ 这条待做（要么吃透世界层坐标，要么接受不模糊）。
2. ❌ **BGM 那行不是文本**：`th18.exe` 与 `dat/*.msg` 里都没有 `BGM:` 字符串，关卡开场那行是
   **解包贴图（anm PNG）**（用户确认）→ 对「字体」问题没有参考价值。
3. 🟡 **字体目标锁定**：用户找到「一模一样的字体」= **符卡名/符卡 bonus 那一栏**（`bonus` 和
   `history` 的数字同款），而那一栏**暂停不模糊**（UI 层）。本轮进展：
   - `+0x19244`（font）的全部写点已穷举：`AbilityMenu`=6、`AbilityShop`=6/0、`FUN_00416ae0`=0、
     描边助手 `FUN_00419cb0` = 10→11 / 6→8 / 7→9。**GUI 画符卡名/bonus 的那段不写它**（只设颜色，
     `FUN_0041b1c0`）⇒ 符卡那栏用的是**环境字体**，和我们传 -1 同一套。
   - `+0x19240`/`+0x1923c` 不是缩放，是 **0/1 标志**（全 exe 写点成对：画前写 1、画后写 0；
     `FUN_00416ae0`/构造函数写 0）→ TH15 里那个「= 2」是别的语义，别当缩放抄。
   - 字号：把 `LOLK_TEXT_SCALE` 试成 2.0，用户实测「数字变大一倍、字形没变」→ 尺寸对了、
     **字形集仍是小字那套**（字形集由「页」选，`mgr+0x2c` 全程 = 2）。下一步：穷举 `mgr+0x2c`
     的写点（谁把它设成 2、有没有别的值），再拿符卡那栏做对照。
4. ✅ **季节升级音效**：TH16 `item_collect_season` 升档时播的是 **`0x3f`**（一手），不是 `se_extend`
   （0x11）→ `SEASON_SE_LEVEL` 改 0x3f。

**2026-09-10 实跑第十四轮（用户逐条描述 TH16 季节槽的**完整外观**）—— 下一步照这张表做**季节槽复刻**：
> 用户的观察逐条对上了 TH16 `front.anm` 的脚本，所以不是猜，是「照原脚本重画」：

| 用户看到 | TH16 原脚本（`local/th16.v1.00a/anm/front/front.anm.txt`） |
| --- | --- |
| 长方形外框 | `script114`：`drawRectBorder(102,10)` + `color(80,80,80)`、`anchor(1,2)`、`pos(-377,888)` |
| **填充 = 黄→白 左到右渐变** | `script115`：`drawRectGrad(100,8)` + `colorMode(1)`；`interruptLabel(2)` → `color(255,255,0)` / `color2(255,255,255)`（**可释放 = 黄→白**）、`interruptLabel(3)` → 蓝/蓝（空闲）|
| **背景蓝色 / 等级 0 背景透明、填充蓝** | 同上：等级 0 时引擎给的是 `pending_interrupt = 3`（蓝→蓝），且 `alpha(0)` 淡入 |
| **等级越高填充越亮** | `script116`：`drawRect(100,8)` 实心，`interruptLabel(8..13)` 逐档变亮（64→96→128→144→192→224），引擎每帧写 `pending_interrupt = 档 + 7`（`season_gauge_render_42c600`） |
| **releaseable 的黄色小贴图**（在长方形上面、季节字下方，就几 px） | 极可能是 `script118` 的 `sprite87` = `front00.png` `(832,112,64,16)` 那条（我此前没认出内容）；`pos(-344,884)` |
| **季节字 PNG** | `script117`：`sprite82`（= 82 + SUBSEASON）|
| **等级数字**（同一套字体、带透明度） | `script118` + 引擎 `pending_interrupt = 档 + 7`？—— 待反 `front.anm` 里数字那支；用户提示「`front.png` 里也有一套 0-9，没带 `%`」→ **符卡那栏的字体可能就是这里面的贴图** |

- ✅ 位置微调（本轮）：季节字 x 34→**30**、y 436→**442**；条 x 70→**66**、y 447→**441**；填充 y 448→**442**；数字 y 445→**439**。
- ❌ 尚未做：渐变填充（现在只有实心）、releaseable 黄标、等级数字改贴图/透明度、字体（见上一轮）。
- ★ 用户的判断「贴图意义上肯定在」+「得反编译的」→ 下一轮直接照上表把 `front.anm` 那 6 支脚本**逐个搬进 `ability.anm`**（贴图从 `front00.png` 切），比继续调常量靠谱。

**2026-09-10 实跑第十五轮（游戏打不开 → 已定位）**：
- 现象：`C0000005`（栈全在 ntdll/ucrtbase 的堆函数），mod 自检全过（`100/100 sites verified`、
  26 张卡全 bound），崩在**游戏启动后期**。
- **根因：存档被上一次崩溃写坏** —— 删掉 `game/scoreth18.dat` + `scoreth18bak.dat`
  与 `%APPDATA%\ShanghaiAlice\th18\th18_card_expand.sav` 后立刻能开。
- 排查过程（记下来省得重走）：先怀疑新断点 `ce_graze`（开机唯一生效的改动）→ 从 patch 摘掉后**仍然开不了**
  → 才怀疑存档。★ **教训：以后「突然打不开」先删存档再二分代码。**
- 处置：① `ce_graze` 断点**不用了** —— 擦弹位置改回**只读扫弹池 + 引擎擦弹判据**
  （`graze_pick()`，见 `season.c`），开机零风险；② `.def` 去掉 `BP_ce_graze` 导出（回到 22 导出，
  thcrap 不再警告「导出了但没声明」）。
- ⚠️ 未做：侧车 `.sav` 已有 magic/version/count 校验（坏档会被拒），但**游戏的 `.dat` 不归我们管** ——
  只能靠「崩了就删档」这条经验。若以后再出现，先删三个文件。

**2026-09-10 实跑第十六轮（新档只有 1 个卡槽）**：
- 现象：删档后新档的**编成容量 = 1**，`cards_dev.js` 的三张 dev 卡只装了第 1 张（114）。
- 原因：编成容量是**游戏自己的递增链**（1→2→3，靠通关/解锁），我们的 dev 卡组只覆盖已有槽的 id
  （`cards.c` 注释：「reset_cards 循环按容量域走」）。游戏里没法手动升（用户实测）。
- ✅ 处置：在 `BP_ce_test_deck` 里**只写内存**把容量顶到 dev 张数。一手依据：
  `reset_cards` 的循环上界**每轮重读** —— `0x407ecb` / `0x407efe`
  `CMP [scorefile + shottype*4 + 0x5f678], esi`，`scorefile = [0x4cf41c]`，
  shottype = 卡对象 `+0xc5c` ⇒ 在断点里写 `[scorefile + shottype*4 + 0x5f678] = n` 就能让三个槽都生效
  （不碰盘上的存档）。日志会打一行 `test: deck capacity 1 -> 3 (shot type X)`。

**2026-09-10 实跑第十七轮（收尾定案，全部已落盘）**：
1. **季节解放消弹 = 金钱 + 季节点（耦合）** —— 用户定案。`ce_cancel_radius(..., mode = 4)`：
   引擎 `Bullet__cancel` `0x428e90` 末尾调 `gen_items_from_et_cancel(pos, mode)` `0x4290e0`，
   **mode 4 → type 2（金钱）**、mode 1 → type 9（转换点）、mode 0 → 什么都不撒。
   ★ TH16 的 `cancel_radius_as_bomb(center, 4)` 的 4 是**季节点**（`gen_items_from_cancel`）——
   同一个 mode 值两作语义不同，别跨作抄。本卡：mode 4 撒钱 + 自己按消弹数撒季节点 = 两者同时掉。
2. **季节槽最终外观**（2026-09-10 二次定案：用户口径 + TH16 `0x42c600` 一手复核，`season.c` 的 `on_draw` 注释里也有）：
   - **渐变条 `drawRectGrad`（script115）= 进度**：TH16 `season_gauge_render_42c600` `0x42c600`
     开头 `search_children(gauge_vm, 0x73)` 取到的就是 script115 的 VM，每帧把
     `get_season_gauge_fill_ratio() × 100.0` 写进它的矩形宽度（`+0x70`，满槽写常量 `0x42c80000`）
     ⇒ **会动的那根条是渐变条**；颜色 = 档 0 蓝(64,64,255) / 档 ≥1 黄(255,255,0)→白(255,255,255)；
   - **实心条 `drawRect`（script116）= 背景**：宽度**永不被写**（恒 100），只收 `pending_interrupt = 档 + 7`
     （TH16 `interrupt 8..13` 的 64/96/128/144/192/224 → (v,v,255)）；**档 0 时透明**
     （TH16 那段连档位中断都不发：`*(uint*)(solid+0x530) &= ~2`）；
   - ⇒ 等级 0：进度蓝、背景透明（用户口径）。
   - **绘制顺序**：照 TH16 script113 的 `scriptNew(114 → 116 → 115 → 117 → 118)`
     —— 实心背景**先**生成、渐变进度**后**生成 ⇒ 同层后生成者在上，**进度盖在背景之上**
     （`hud_ensure` 的生成顺序已按此改）。
   - 颜色由**卡每帧写**（`ce_anm_set_color` + `ce_anm_set_color2`，后者写 `vm+0x528`）——
     因为 TH18 的渐变图元只认 color_1 + color_2，靠脚本的 interrupt 上色不稳。
   - ★ 教训：`0x42c600` 里宽度写进的是 `pauVar4`（`search_children(..., 0x73)` 那支子 VM），
     **别只看「填充」两个字** —— 谁是进度必须看 `search_children` 的参数对到哪支脚本。
3. **绀珠传染色「出圈不还原」** —— 我上一版加的守卫（`*p == 黄 才还原`）是元凶：弹的颜色被别处
   改过就跳过还原，下一帧再标黄时「原色」已是黄色 → 永久黄。改回**无条件写回原色**。
4. **`drawRectBorder`（opcode 612）在 TH18 不可用** —— 零售 anm 零使用；季节槽外框已改用
   `drawRect(102,10)`。**以后写 UI 脚本别用 opcode 612。**
5. **UI VM 的 `pos()` 会被脚本每帧写回** —— 从 TH16 搬来的脚本里带 `pos(-377,888)`（TH16 槽坐标）
   会让 VM 跑到屏幕外、整组消失；**位置一律由卡 `ce_anm_set_pos` 写**。
6. **`BP_ce_graze` 断点已撤**（曾疑似导致开机崩；真凶是坏存档）→ 擦弹位置改回**只读扫弹池**
   （`graze_pick()`，引擎擦弹判据），开机零风险。`.def` 回到 22 导出。

**2026-09-10 实跑第十八轮（季节槽进度条认错 → 已改，用户当场纠正）**：
- 用户：**「会动的那根条是进度条，是渐变的」**。上一轮我把 `drawRect`（实心）当进度、`drawRectGrad` 当背景 —— 反了。
- 用户口径（逐条）：「**会动的部分 = 进度矩形 = 黄白相间**；**不会动的部分 = 进度条背景矩形 = 蓝、
  随等级变白**；**唯一特别 = 档 0 时进度填蓝、背景填透明**」——代码已按此实现（进度色只判 `档 ≥ 1`，
  不再掺 `rel_frames`；背景档 0 写 alpha 0）。
- 一手复核（`th16:0x42c600` `season_gauge_render_42c600` 反编译）：
  `search_children(gauge_vm, 0x73, 0)` → **script115（渐变条）** 的 VM 就是宽度被写的那支
  （`*(float*)(vm+0x70) = fill_ratio × 100.0`，满槽写 `0x42c80000`），`interrupt 2/3` 也发在它身上；
  `search_children(..., 0x74, 0)` → script116（实心条）只收 `pending_interrupt = 档 + 7`，宽度恒 100。
  ⇒ **渐变条 = 进度，实心条 = 背景**（档 0 时背景透明）。
- 绘制顺序（一手）：`front.anm` script113 的 `scriptNew(114 → 116 → 115 → 117 → 118)`；
  TH18 侧 `AnmManager__render_layer` `0x4882b8` 从 `ui_list_head`（`mgr+0x6f8`）**表头向表尾**走，
  而 `insert_in_ui_list_back` `0x488490` 是**尾插** ⇒ **后生成的画在上面** ⇒ `hud_ensure` 改成
  **先起实心背景、后起渐变进度**（见 `engine/anm/th18/01` §2b-4）。
- 代码改动：`season.c` 的 `on_draw` —— 进度宽度/位置挪到 `hud_grad`，背景固定全长挪到 `hud_solid`；
  颜色：进度 = 档 0 蓝(64,64,255) / 档 ≥1 黄(255,255,0)→白(255,255,255)；背景 = 档位色 (v,v,255)，
  **档 0 写 alpha 0**；interrupt 拆成「状态色」与「淡出淡回」两路发（同一个 `pending_interrupt` 只有一份，
  混在一帧里发会让 4/5 把 2/3/8..13 顶掉）。
- 产出：DLL `e999317277ad1a7e0694637038e4e2d4`（`ability.anm` 未变 `1a135369e233d5379b57d0fce17ad585`）。

**2026-09-10 实跑第十九轮（115 卡字体定案：不是猜，是回 TH15 原代码读出来的）**：
- 现象：绀珠传面板五行数字的**字形不对**（用户：要「符卡名/符卡 bonus 那一栏」那种）。
  上一轮靠猜（-1 环境值 / 7 / 11）全不对，还以为是「页」的问题。
- ★ **一手定案**：`th15:0x436990` `Gui::on_draw_2e` 反编译 —— TH15 画这块面板时写的是
  `group = 2; font_id = 2; alignment_mode_h = 2; alignment_mode_v = 0`（顶上那个大号章节奖
  数字另走 `font_id = 4` + align `0/0`），画完还原 `font_id = 0 / group = 0`。
  ⇒ **五行 = 字体 2**（`LOLK_FONT` 由 -1 改成 2，sweep 与页诊断代码一并删掉）。
- 跨作可用性（一手）：TH18 `draw_string` `0x41a2b0` 里字体 2/3 的字形映射与 TH15 **逐条相同**
  （`'a'..'z'→+0x74`、`'A'..'Z'→+0x94`、`/ : - * % $ . +`→`0xce/0xcf/0xd0/0xd1/0xd2/0x101/0xd3/0xd4`），
  且 TH18 `ascii.anm` entry0 的 sprite 196..205 正是 8×10 的数字块（字体 2 的数字 = `0x30+0x94`）。
- **顺带搞清了「字体」到底怎么选**（新文档 [`engine/anm/th18/03`](../../../engine/anm/th18/03-ascii-text.md)）：
  字形集 = **页**（`vm+0x20` = `anm_loaded_index_2` → `preloaded[页]`，TH18 的 ascii 管理器里就是 `mgr+0x2c`）
  **+ 字体 id**（环境 `+0x19244`，`create_string` `0x419b90` 抄进记录 `+0x120`）两段；
  页**不**随字符串记录走。`+0x19240/+0x1923c` 是描边开关不是缩放（旧注释已订正）。
- 工具：新加只读脚本 `tooling/ghidra/scripts/find_struct_writes.py <ver> <disp> [--global addr]`
  （「谁写了结构体某个偏移」，靠助记符判写——Ghidra 的 `OperandType.WRITE` 在这份 DB 上恒为 0）。
- 产出：DLL `d8f6b85934f66b1d8b4fabd316b59e6a`（`ability.anm` 未变）。

**2026-09-10 实跑第二十轮（116 卡档位数字：也是字体 2，不是贴图）**：
- 用户此前说「等级数字用 `front00.png` 那套 0-9 贴图」——**反编译证伪**：
  `th16:0x428e70` `Gui::on_draw_2` 尾段画档位数字时走的是 ASCII 文字：
  `font_id = 2`、`group = 1`、`alignment_mode_h = 0`、`alignment_mode_v = 2`，
  数字本身由 `FUN_004082b0` = `create_string("%d")` 画；颜色取 7 档表
  `0x60606060 / 0xa0b0b080 / 0xb0b8b880 / 0xc0c0c080 / 0xd0d0d080 / 0xe0e0e080 / 0xffffff30`，
  玩家贴近左下角时 alpha 字节写 **0x40**。
  （`front.anm` 的 sprite 35..45 那排 16×18 实心圆角块是**条/边框件**，不是数字——像素实测过。）
- 处置：`season.c` 的档位数字改用 `ce_hud_text_ex(..., font=2, group=1, align_h=0, scale=0)` +
  上面那张色表（淡出走 0x40），不再是环境字体 0 的白字。
- 顺带：TH16 里 `font_id = 2` 的写点普查（`find_struct_writes.py th16.v1.00a 0x19224`）——
  `Bomb::on_draw`、`Spellcard::on_draw`、`Gui::on_draw_2`、`PopupManager::on_draw`、
  `MainMenu::on_draw__spell_practice_histories` 全用 **2**（4 只出现在大号数字）⇒
  「符卡名 / bonus / history 那一栏」= 字体 2，与 115 卡的定案同源。
- 产出：DLL `664fc1b5a35250ccec1c7ce36efa26c8`。

**2026-09-10 实跑第二十一轮（用户报两处回归 → 都定位到根因）**：
1. **天空璋等级数字消失** —— 我把 TH16 的 `group = 1` 照抄了过来，但 TH18 的字符串是
   **当帧画、下帧 tick 掉**（`AsciiManager__on_tick` `0x4198b0` 每帧把记录 `+0x12c` 减 1、
   ≤0 就压缩掉；环境 `duration` 默认 0），而 group 1 的绘制趟 `AsciiManager__on_draw_37`
   （`0x419950` → `draw_group(1)` `0x41a180`）跑在卡的 `on_draw` **之前** ⇒ 串当帧没人画、下帧被清。
   group 0 的趟 `on_draw_58`（`0x419940`）在卡之后 ⇒ **改用 group 0**。
   （四个趟：`on_draw_58`=0、`on_draw_37`=1、`on_draw_48`=2、`on_draw_3f`=3，一手反汇编。）
2. **绀珠传字体/大小** —— 字体其实已经对了：**像素级对账** TH15 与 TH18 的
   `ascii.anm` sprite 196/197/205 逐像素**完全相同**（字体 2 的数字块 8×10）。
   真错的是**字号**：`LOLK_TEXT_SCALE` 曾写 2.0，而 TH15 画面板文字用的是**环境 scale = 1.0**
   （`FUN_00416ae0` 重置 1.0），且 ASCII 空间就是 640×480 那一套（我们的 UI VM 是它的 2×）
   ⇒ 2.0 等于「放大到 2 倍」。**改回 1.0**。
- 产出：DLL `bf276ee465d322d09f9745a22e172401`。

**2026-09-10 实跑第二十二轮（季节槽像素微调，用户逐条给数）**：
| 元素 | 位移 | `hud_ensure` 新偏移 |
| --- | --- | --- |
| 季节字（script126）| 右 1、上 1 | `+15 / −24`（原 `+14 / −23`）|
| releaseable 条（script127）| 上 1 | `+31 / −16`（原 `+31 / −15`）|
| 进度条**整体**（外框 + 背景 + 进度）| 右 3、上 1 | 外框 `+51 / −13` |
| 背景条 + 进度条（在整体基础上再挪）| 左 1、上 1 | 两者 `+49 / −12`（`on_draw` 里同步改）|

- 产出：DLL `6e5c81908307dbf33e9b740b7fdad311`（`ability.anm` 未变）。

**2026-09-10 实跑第二十三轮（115 卡「章节结算」面板上移 47 px）**：
- 用户：`chapterfinish` 整块比现在**上移 47 px**（UI 表面像素）。⇒ `LOLK_PANEL_AY` 402 → **355**；
  五行数值同步 −23.5 字面量 px（47 UI px ÷ 2）⇒ `LOLK_VAL_Y` 226 → **202.5**，保持「文字贴在第 1 条上」；
  淡出框是照面板几何推的（1 UI px = 0.5 世界 px）⇒ `LOLK_PANEL_FADE_Y0/Y1` 153/353 → **129.5/329.5**。
- 字体：用户确认「就是 ascii 里的那套，已比对过」——与本文档第二十一轮的像素对账一致
  （字体 2 = `ascii.anm` sprite 196..205，与 TH15 逐像素相同），**本轮不动**。
- 产出：DLL `306971c77cfb13e6d50a238c85dea001`。
- ★ 顺带把 modkit 的 `thcrap/bin/mod_th18_card_expand.dll`（实际注入的那份）也手动刷新了：
  启动器只在**启动时**从 `mods/` 拷，之前它一直停在旧 build（见第二十二轮后的排查）。

**2026-09-10 实跑第二十四轮（★ 字体真凶：`drawf_debug` 会强制字体 1）**：
- 用户把 `sprite196-205_all.png` 三行比对完，确认目标是**第三行** = TH18 `ascii1280.anm` 的
  sprite 196..205（16×20），但「引擎 plot 的不是这个」。
- 一手反汇编 `AsciiManager__drawf_debug` `0x41a110`：
  ```
  0041a14e  IMUL EAX,[ESI+0x19224],0x138     ; num_strings * 0x138
  0041a15d  MOV  dword ptr [EAX+ESI+0xc0c], 1
  ```
  记录基址 = `mgr+0xc24+idx*0x138`，`create_string` 已把 `num_strings` 加到 `idx+1`
  ⇒ 写入地址 = `mgr+idx*0x138+0xd44` = **刚建那条记录的 `+0x120` = font_id**
  ⇒ **我们设的字体永远被改成 1**（映射 `char+0x42`）——这才是「字体扫描五行一个样」「换字体没用」的真因。
- 处置：`sdk.c` 的 `ce_hud_text_raw` 改成直接调 **`AsciiManager__create_string` `0x419b90`**
  （thiscall `mgr, pos[3], text`；文本本来就已 vsnprintf 好）；`shadow` 参数改为同时置
  `+0x19240`（描边遍开关）而不是只写 `color_2`。`engine.h` 加了 `CE_FN_ASCII_CREATE` /
  `CE_ASCII_DRAW_SHADOWS`，并把 `CE_FN_ASCII_DRAWF` 标注成「会强制字体 1，别用」。
- 另记（一手）：ascii 管理器按 UI 缩放 `DAT_00556aca0` 选 **ascii.anm / ascii_960.anm / ascii1280.anm**
  三个变体（>1.6 → 1280），字体 2 的数字分别是 8×10 / 12×15 / 16×20 三套**各自重画**的字形
  ⇒ 卡侧跟着引擎走，不用指定。
- 产出：DLL `fc918278db3804f1f972fb1f84a2b6c4`（`mods/` 与 `thcrap/bin/` 两处都已刷新）。

**2026-09-10 实跑第二十五轮（用户逐条像素 + 查「蓝紫色 +N」）**：
> 用户说明：屏幕 100%、游戏 1280×960，**此后所有 px 都是截图像素**（= UI 表面像素）。
1. **进度条外框**（1 px 边框，script123）再**左 1、下 1** ⇒ `hud_ensure` 里 `+50 / −12`。
2. **季节等级数字**左 40、下 8 截图像素 ⇒ ASCII 空间减半：`x 112→92`、`y +6→+10`。
3. **115 卡章节面板**整体**下移 16**（`LOLK_PANEL_AY` 355→**371**）、文字再下移 23
   ⇒ `LOLK_VAL_Y` 202.5→**222**；淡出框 y 跟着 +8 世界 px（137.5/337.5）。
4. ★ **蓝紫色 `+N` 查清了**：TH15 `Gui::on_draw_2e` 的**第 5 行**（`0x436bb3`–`0x436be9`）——
   - 值 = `(gui+0x130 / 50000) × 10`（魔数 `0x14f8b589` + `SAR EDX,12` = ÷50000，再 ×10）
     = **章节奖换到的 PIV（最大得点）增量**，**不是**「加进总分的 `bonus/10`」（那条在 `0x43ad19`，不显示）；
   - 色 = `[mgr+0x19224] = 0xff8080ff`（D3DCOLOR 0xAARRGGBB ⇒ R=80 G=80 B=FF = 蓝紫）；
   - 位置只比第 4 行低 **10** px（286 → 296），不是 20。
   ⇒ 卡里三处修正：`LOLK_HUD_BLUE` 从 `0xffff8080` 改成 **`0xff8080ff`**（旧值 R/B 写反 = 粉色，所以「咱们没有」）、
   数值改 `(shown/50000)*10`、行距用新的 `LOLK_VAL_DY_PIV 10`。
   同时订正 `engine/chapter/th15/01` §4b：原来只列了 4 行且把第 5 行当第 4 行。
- 产出：DLL `fe452e27118b61fdceb55e9aefb4a7db`（两处已刷新）。

**2026-09-10 实跑第二十六轮（三个 bug + TH16 子机弹贴图）**：
1. **115 卡「特殊擦弹圆」变小了** —— 一手复核 `th15:0x455d00`：TH15 的擦弹半径在
   `player+0x16294 & 0x10`（`Player__i_grow_bigger` `th15:0x457510` 置的「变大」态）下是
   **`player_scale × 3.6 × 判定半径`**（3.6 是立即数）——TH18 没有这个系数（所以「普通圈」小）。
   处置：`lolk.c` 新增 `LOLK_GRAZE_RADIUS_SCALE 3.6`（我们不复刻变大动画，直接取该态系数），
   擦弹圈 = `3.6×3.0 + max(br/2.5,40) ≈ 50.8 px`；`106_lolk_ring` 的落定 scale 由 1.0 → **1.155**
   （= 50.8 / 44，贴图亮环半径 44 px）、起始 2.15 → 2.48。
2. **擦弹季节点「初始角度应该更直上直下」** —— 一手复核 `th16:0x444e03`：原版就是
   `ItemManager__spawn_item(0x10, &bullet->pos, …, atan2(Δy, Δx), 1.9f, …)`
   （`MOVSS [ESI+4]/SUBSS [EDI+0x614]` → `FLD y; FLD x; CALL math_atan2`），
   **出生点 = 弹的位置**、方向 = 「从自机指向弹」。我们之前把出生点推到收集圈外 6 px（躲「当帧被收」），
   现在有 `SEASON_POINT_INTANGIBLE`（32 帧不吸不收）兜底 ⇒ **去掉推出、照原版从弹上起飞**。
3. **★ 切关后子机被打回春** —— 根因：`on_load`（+0x20）不是「开局一次」，它的唯一调用点
   `AbilityManager__notify_cards_on_load` 由 `AbilityShop__sub_417880` 触发，而那个函数在
   **每关结束的商店/结算**都会跑（`AbilityShop::on_tick`、`GameThread__teardown_and_recount_cards`）。
   原来在 `on_load` 里 `clear_card` ⇒ 每过一关 `memset` 把 `subseason` 清零（春）、季节点也清零。
   **改成 `on_load` 什么都不做**；只在局末（+0x4c）与销毁（+0x50）清（新一局是新对象，状态天然归零）。
4. **★ TH16 子机弹的贴图查清了**（见 [`engine/season/th16/01`](../../../engine/season/th16/01-season-data-and-tables.md) §5.6）：
   副季节弹走 `player->subseason_anm_file`（**pl0Xsub.anm**）+ 脚本号 = 副 .sht 的 `+0x22`
   （主弹才是 `pl0X.anm` 且脚本号 +5）。逐季节的 sprite 与脚本全表已落文档。
   落地：`make_season_art.py` 切出 5 套 sprite（土用 30×14→32×16）、`ORDER.txt` +18 项、
   `ability.anm` 新增 **script128..131**（春/夏/秋/土用，逐条照抄 pl0Xsub 的 script0）、
   `append_shooterset.py` 的 anm 字段改指它们（原来是同一个程序生成的 `SEASON_SEED`）。
5. **未做（激光）**：冬仍是直飞快弹。TH16 的激光 = `pl01sub.anm` script0/3/6/9/12/15
   （`anchor(1,0)` + `scale(1,0)→scaleTime(4,1,1,0.3/0.4)` + `blendMode(1)` + `scrollX(−1/21)` 滚动
   + `scriptNew(scriptN+1)` 子脚本）叠引擎蓄力（`+0x68`/`+0x70` + flag `|8|0x10`）。
   TH18 的主炮激光 func（init2/tick2/hit2）**不能**用在装备卡子机上（要 option 占用表，会把主机定住）——
   要做就**卡侧自己画束 + 自己建伤害源**，记在下一轮。
- 产出：DLL `8a157598bd42f09bb90805b911178996`、`ability.anm` `0a9a4ce8859370e675e427e0cb0da892`
  （`mods/` 与 `thcrap/bin/` 两处已刷新）。

**2026-09-10 实跑第二十七轮（「一系列长方形」→ 切图漏了 entry 的 `xOffset/yOffset`）**：
- 用户实跑：季节子机弹变成**一排白色长方形**。根因：TH16 的 anm 是**同页多 texture**，
  entry 的 `width/height` 只是逻辑尺寸，实际数据在页里的位置由 **`xOffset/yOffset`** 给，
  sprite 的 `(x,y)` 是相对该 entry 的 ⇒ 切图必须 `+ (xOffset, yOffset)`。
  各 pl0Xsub entry0 的偏移：`pl00sub (64,192)` / `pl01sub (0,176)` / `pl02sub (0,144)` /
  `pl03sub (0,176)` / `pl04sub (0,144)`。不加偏移 ⇒ 切到白色空白区 ⇒ 白方块。
- 顺带发现 `make_laser()` 的老切法也漏了 `pl01sub` 的 `yOffset 176`（之前那张 256×32 其实是空白）。
  两处都改好：`make_season_art.py` 的 `SUBS` 现在带 offset 元组，激光改成 `(0,177,256,207)`。
- 反例（偏移=0，一直是对的）：`bullet/item.png`、`front/front00.png`。
- 产出：DLL `8a157598bd42f09bb90805b911178996`（未变）、`ability.anm`
  **`eebeb02c7fb0cb5d0a3dbdb7c20ee137`**（`mods/` 与 `thcrap/bin/` 两处已刷新）。

**2026-09-10 实跑第二十八轮（★ 照零售「ミニ八卦炉」把冬的激光做出来）**：
- 用户点名：魔理沙的能力卡 **迷你八卦炉** 发的就是激光。查 `dat/ability.txt`：`@MARISA_OP` = ミニ八卦炉
  = 卡 **10** = `pl0X.sht` **set 0xb**（四个角色逐字节相同）：
  `rate=5 delay=0 dmg=13 ang=−π/2 **speed=0** opt=0x00 **mode=2** **anm=3** sfx=−1 **hit_fx(+0x26)=5**
   func=**init2/tick2/—/hit2**`。
- `ability.anm` 里 **script3 = 激光束**（`scriptNew(script4)` + `layer(15)` + `sprite(sprite8 = op00.png
  (0,16) 512×16)` + `anchor(1,0)` + `scale(1,0)→scaleTime(4,1,1,0.5)` + `blendMode(1)` + `rotateAuto`
  + `scrollX(−1/21)`），**script5 = 命中特效**（由 hit2 每 2 帧在束端起）。`script4` = 束根光斑。
  ⇒ **激光不是「弹」，是挂在子机上每帧重画的束**；`fire_rate` 只是重新起束的节奏。
- ★ 结论修正：上次「激光损坏内存」**不是这条路的锅**，是我们没照零售填参数（当时 `mode=0`、没有 `hit_fx`、
  用了别的脚本）。文档里「魔理沙 hit_fx = 8」也订正为 **5**。
- 落地（照 TH16 `pl01sub` 的对应脚本自建，贴图用 TH16 的激光条）：
  - `132_season_laser_beam`（束，TH16 pl01sub script0）+ `133_season_laser_child`（光斑，script1）
    + `134_season_laser_hit`（命中特效，照 TH18 script5 骨架 + TH16 星形迸溅贴图）；
  - `make_season_art.py` 新切 `LASER_CHILD.png` / `LASER_HIT.png`（同样 +pl01sub 的 yOffset 176）；
  - `SEASON_WINTER` 的 shooter 改成 `speed=0 / mode=2 / anm=132 / hit_fx=134 / func 2,2,2`；
  - `append_shooterset.py` 的 `pack_shooter` 补上 `+0x26`（hit_fx，支持名字→id）。
- 产出：DLL `8a157598bd42f09bb90805b911178996`（未变）、`ability.anm`
  **`2a10634ed677a2248e075ede87c280f4`**。

**2026-09-10 实跑第二十九轮（用户四个 bug + 两个问题）**：
1. **季节槽盖在对话立绘上** —— 一手把 UI 组的绘制优先级排了一遍（`engine/anm/th18/02` §1 的桩表）：
   UI 列表 VM 层 24–32 折到组 37–45，各组桩优先级 0x39(37) / 0x3e(38) / 0x41(39) / … / 0x57(**44**)。
   我们的 HUD 在层 20/21 → **折到组 39（优先级 0x41）**，而立绘在层 31 → **组 44（0x57）**
   ⇒ 组 39 排在 44 之后 = 盖住立绘。处置：HUD 五个脚本的层改成 **24/25**（组 37/38，最早的 UI 组），
   相机 2 的 x 原点是表面左上角 ⇒ `SEASON_GAUGE_OX` 由 −191 改成 **+449**。
2. **boss 击破的季节点「2 次」** —— 查 TH16 ECL 普查（`engine/season/th16/01` §5.3）：
   boss 的 `dropSeason(1200,200,10)` **每个相位各调一次**（st01 ×4、st03 ×7、st07 ×17…），
   所以「每个 boss 相位掉 200 个」是**原版行为**，不是 bug；之前那个「3 回」是同一个事件被调 3 次（已去重）。
   （若用户想要「整只 boss 只掉一次」再说 —— 那是偏离原版。）
3. **进第二面后 releaseable 的 UI 消失** —— 换关时引擎清过 anm，我们的 HUD VM 被 `hud_ensure`
   重新生成，但 `hud_last_state` 还记着旧值 ⇒ 不再发 interrupt ⇒ 条 script127 停在初始态
   （它的初始态是 `interruptLabel(3)` 里的 **alpha 0**）。处置：`hud_ensure` 里只要发现**任一 VM
   是新生成的**，就把 `hud_last_state` 打回 −1，下一帧重发全部 interrupt。
4. **擦弹掉的季节点不被吸** —— 原来 `!p->pad` 把擦弹点排除在吸引圈外（当初为了躲「一出生就被收」）；
   现在有 `SEASON_POINT_INTANGIBLE`（32 帧不吸不收）兜着，**去掉该条件**，和普通道具一样吸
   （TH16 里它就是普通 item type 16，走同一套 magnet）。
- **问题①「原版季节解放后等级归零还是归 1」** —— 一手（`Bomb__activate_bomb`，见
  `engine/player/th16/02` §3）：**春/秋/冬 = 整槽清零（等级 0）**；**夏/土用 = 只扣一档**（等级 −1，
  槽里余下的可以连放）。不是「归 1」。卡里 `season_points_after_release` 就是这个口径。
- **问题②「模糊」** —— 暂停的模糊是加在**游玩区 surface（层 3–19）**上的；我们的 HUD 和游戏自己的
  HUD（分数/残机）一样在**外围 UI surface（层 24+）**上 ⇒ **原版的 HUD 同样不被模糊**。
  想让它被模糊只有三条路：① 维持现状（与原版 HUD 一致）；② 把 HUD 挪进游玩区 surface（层 3–19）
  —— 但季节槽在屏幕左下角、**落在游玩区 384×448 之外**，会被相机 3 的视口裁掉；③ 自己检测暂停
  给 HUD 加淡化/自绘模糊（❓自定，偏离原版）。
- 产出：DLL `170167f00526803f5dba24edc503d72f`、`ability.anm` `a3aef14d0a37d38896a2d11666c0bfa5`
  （`mods/` 与 `thcrap/bin/` 两处已刷新）。

**2026-09-10 实跑第三十轮（用户三条：解放后子机 / 冬子机激光残留 / 秋·冬释放圈的拖影）**：
1. **★ 冬子机的激光在解放后残留**（用户口径：「按着 Z 的时候季节解放，不放开 Z 激光还在」）——
   一手查出根因：激光（`mode=2`）的寿命**只跟射击键走**，判据里没有「子机还在不在」
   （`tick2` `0x45fc80` 尾部：`shoot_key_short_timer >= 0` + 自机状态 + **玩家自己的** option 槽额度）。
   弹与子机的唯一联系 = `PlayerBullet__create` `0x45e3ae` 写进 `bullet+0xe4` 的子机指针。
   处置：SDK 新增 **`ce_option_kill_bullets(option)`**（扫 `PLAYER+0x1570` 的 512×0x9c 弹池，
   `bullet+0xe4 == 该子机` 的按 `tick2` 自己的自杀序列收：`bullet+0x8c = 2` + 对 `bullet+8` 的束 VM
   发 `interrupt 1`），`ce_option_drop` 里先调它。日志 `option: killed N lasers attached to …`。
2. **★ 子机槽泄漏**（同一次排查的附带发现，`Player__allocate_option` `0x40a790` 一手）：
   找空位的判据是 `equipment[i].in_use == 0`，而**全程序没有第二个写 `in_use` / `option+0xb0` 的地方**
   ⇒ 我们原来只删 VM 不还槽，每收一次子机漏一格，12 格漏完新子机再也申请不到。
   `ce_option_drop` 现在把 `in_use` 写 0（日志里三个 `option allocated` 用掉三格不同槽就是这条的证据）。
3. **★ 解放圈的配色五季统一错**（用户：冬在天空璋是**蓝色**、我们和春一样是粉色）——
   一手逐字节取五个副季节的释放脚本（**脚本号不统一：冬在 `pl01sub` 的 20/21，其余是 3/4**，
   `BombSub*__begin` 的 `AnmLoaded__create_40e5c0` 实参）：
   春 灰→绿 + 粉→红（32 边形）；夏/秋/冬/土用 外粉→橙 + 内**蓝→青**（12 边形、alpha 128、±10°/帧自转），
   土用另有 `posTime(60,1,0,48,0)` 上浮 48 px。卡侧新增 `season_core.c` 的 `season_ring()` 表 +
   `ce_anm_set_color2` 写渐变端色 + `ce_anm_set_rotation` 自转 + 新脚本 **135（内圈 12 边形）**。
   逐季节配色见 [`engine/player/th16/02`](../../../engine/player/th16/02-season-release-and-bombs.md) §5g。
4. **★ 同一个击破节点掉两次季节点**（用户：「和之前的三次一样，只是少了一次」）——
   根因：钩子挂在 `Enemy__drop_items_and_notify_cards` `0x430510` 入口，而**同一个击破节点**引擎会调它两次
   —— 死亡路径（`FUN_00430240`，三处调用点）一次 + ECL 自己的 `dropItems` 指令（opcode 509 → `0x433abf`）一次。
   旧去重只记「一个上一次敌人 + 30 帧」，两次之间若有别的敌人掉落（boss 死亡演出会清场）记录就被顶掉。
   现在 4 格环 + 90 帧窗、键 = 指针 + `enemy_id`（`Enemy__constructor` `0x42e3f0` 保证每实例唯一）；
   被压掉的会记 `season: drop dup suppressed (...)`。窗口取 90 帧 = 盖住同一次击破的两次回调、
   又不并掉下一相位（TH18 boss 相位之间至少 `wait(120)`，st01bs ECL 实证）。
5. **解放后档位/子机**（用户：「归零时不该有季节子机，但不同季节规则不一样」）——
   天空璋口径 ✅：子机数**恒等于档位**（`PlayerInner__repopulate_options` 的 `for (i = 0; i < level; i++)`，
   余槽清空），而 `Bomb__activate_bomb` **扣完槽再**调它 ⇒ 春/秋/冬 释放后 = 0 颗子机、
   夏/土用 = 少一颗。`release()` 现在**无条件**重排（原来只在档位下降时），on_tick 每帧再兜底收掉多余的；
   日志加 `level -> %d, options %d`。
6. **★ 解放残像（用户点名「照天空璋补上」）** —— 一手查出天空璋的机制：
   `Player__on_tick` `0x442c76`/`0x442d33` 在**加速态**下每帧建一个残像 VM
   （`AnmLoaded__create_40e5c0([player+0xc], &out, 脚本表[CHARACTER], 自机坐标, 0, -1)`），
   脚本表 `0x494800` = `{10, 9, 13, 16, 0}` 按**角色**（`0x4a57a4` = `CHARACTER`，不是副季节！），
   再 `set_sprite` 成自机当下那帧、color_1 写状态色（加速 = 黄 `0xffffff00`、冬的速度态 = 蓝 `0xffff0000`）；
   **无敌中不建**（走只闪色那一支）。触发只出现在秋/冬：秋 `BombSubFall__on_tick` 每帧写
   `player+0x16688 = 1.5`，冬 `BombSubWinter__on_tick` 每帧写 `player+0x2c7cc = 1.5`（→ flag bit0x20）。
   TH18 的引擎把这段删了（`Player__on_tick__body` `0x45c6e4` 直接跳到 `0x45c702`，无建 VM 调用），
   但**零售脚本还在**：`pl0X.anm` 的 `sprite(sprite0)` + `color(60,0,192)` + `alphaTime(20,0,0)`，
   脚本号按角色 **pl00 17 / pl01 19 / pl02 14 / pl03 14**。已在卡里补上（`afterimage_tick`）：
   秋/冬 解放中、**非无敌**时每帧起一个，颜色照 TH16（秋黄 / 冬蓝）。
   🟡 订正（同一轮内查清）：TH16 的**冬**释放写的 `player+0x2c7cc = 1.5` 不是速度，而是
   **对敌伤害 ×1.5**（`EnemyData__step_logic` `0x41c5d6` 紧跟在 `enm_compute_damage_sources` `0x445a30`
   返回之后）；`player+0x1664c` bit0x20 的唯一读者就是残像/闪色那一支（`05` §1b 已订正）。
   本卡**两样都已经有**：秋 = `player+0x477ec ×1.5`（`release_tick`）、冬 = `ce_set_bullet_damage_permille(1500)`。
- 产出：DLL `3a33d4bf3646455eb618b97767bb27bb`、`ability.anm` `1a9c4c7e96cbdac6d6e823c6ef15f9cf`
  （`mods/` 与 `thcrap/bin/` 两处已刷新）。校验：`test-host` 全绿（含 `test_season_core` 89 断言）、
  `cards-check` / `anm-verify` / `sht-verify` / `dllx87` / `dllverify` 全过。
  日志里新增的行：`option: killed N lasers attached to …`、`season: drop dup suppressed (…)`、
  解放行的尾巴多了 `level -> %d, options %d`。

**2026-09-10 实跑第三十一轮（用户：模糊没解决 + 槽搬进游玩区后的两处回归 + 绀珠传面板也要模糊）**：
1. **★ 季节槽 / 绀珠传章节面板搬到游玩区 surface（层 19）** —— 用户上一轮选了「把 HUD 挪进游玩区」这条路，
   本轮落地。依据一手：暂停的模糊 = `text.anm` script59 把**游玩区 surface 的游玩区矩形**降采样成 256×256
   再放大回 384×448 盖上去（`FUN_00458480` `0x458480` + `D3DXLoadSurfaceFromSurface`，只盖那一块），
   所以要被模糊就必须画在**游玩区 surface 里、且落在游玩区矩形内**。
   - 季节槽五件（脚本 123–127）：`layer(24/25)` → **`layer(19)`**、`ce_anm_spawn_ui` → **`ce_anm_spawn`**（世界列表），
     坐标从「相机 2 的表面 px」换成 **ECL**（1 ECL = 2 表面 px），并**回到天空璋真正的槽位**——
     游玩区左下角（TH16 `front.anm` script114 的 `pos(-377,888)` + `anchor(1,2)`；
     `Gui::on_tick_20` 的淡出触发「玩家 x < −64 且 y > 400」正说明槽在游玩区里）。
     新基址 `SEASON_GAUGE_BX/BY = (−166, 442)` ECL（外框中心），五个元素的相对偏移 = 旧值 ÷2（ECL）。
   - **贴图尺寸同步 ÷2**（游玩区空间 1 单位 = 2 表面 px）：`drawRect(102,10)→(51,5)`、
     `drawRectGrad/drawRect(100,8)→(50,4)`、季节字与 releaseable 条加 `scale(0.5,0.5)` ⇒ 屏幕尺寸不变。
     ⚠️ 第一版漏了这步，用户实跑看到「季节字/releaseable 条往上跑了 ~20 px」（其实是被放大成 2 倍、
     中心不动 ⇒ 顶边更高），本轮补上。
   - 绀珠传面板（脚本 107–112/116）：`layer(20)` → `layer(19)`、`ce_anm_spawn_ui` → `ce_anm_spawn`，
     坐标 `((AX+DX−640)/2, (AY+DY−32)/2)` ECL（屏幕位置不变），`scale(0,1)→(0,0.5)` 同步 ÷2。
   - ⚠️ **档位数字 / 绀珠传面板的数值行仍是 ASCII 文字**（`ce_hud_text_ex`，永远在 UI surface 上）
     ⇒ 槽/面板被模糊时**数字仍然清晰**。要连数字一起模糊就得把数字做成贴图 VM（记在「没做」里）。
2. **「季节进度条消失」不是回归** —— 那是**档内进度 = 0**（用户当时的存档正好卡在档位阈值：
   日志 `level up -> 2/6 (… 230 points)` = 第 2 档阈值 230）⇒ 进度条宽度 0、看不见。
   天空璋口径就是「当档内的填充比」（`get_season_gauge_fill_ratio`），阈值处本来就该是空的。
   跨过阈值收几个点后就会长出来；进度条本体（宽度/位置/颜色）本轮没改。
3. **★ 改为「UI 表面 + 组 37」**（用户实测「放进来的 PNG 都变模糊了」+「数字还在立绘上面」）——
   一手把 `AnmManager__render_layer` `0x4882b8` 的折叠读清了（UI 列表：层 24–32 → +13；层 37–45 → 原样；
   **其它层 → 一律 39**），于是：
   - 暂停模糊罩 = `text.anm` script59 的 UI VM，**层 38 → 组 38 → 优先级 `0x3e`**，只盖游玩区矩形；
   - 我们的 UI 放**层 24 → 组 37 → `0x39`** ⇒ **比罩子早画 = 暂停时被罩住**，且平时在 UI 表面 = **全分辨率**。
   上一版画在游玩区 surface（层 19）虽然也进模糊，但那张 RT 只有 408×472 再放大到屏幕
   （`camera_3` 视口 + `arcade_vm` 放大）⇒ **平时就发虚**（用户：「画质感觉降低了一档」）。
   季节槽五件回 `ce_anm_spawn_ui` + 层 24（尺寸回原值 102×10 / 100×8 / scale 1.0），
   绀珠传面板七件同样回 UI 表面层 24（坐标回 `(AX+DX, AY+DY)` 表面 px）。
   位置仍放在**游玩区左下角**（TH16 真正的槽位；`SEASON_GAUGE_BX/BY = (308, 916)` 表面 px）。
4. **★ 档位数字的 ASCII 组号：0 → 3 → 1 → 3（组 1 行不通，已回退）**（用户：「数字还是在对话立绘上面」→「先把 ascii 放进能模糊的地方」→「你刚才改的所有 ascii 数字都看不到在哪了」）——
   一手把四个 ASCII 绘制趟的优先级读出来（`register__on_draw` 实参 + 反编译）：
   group 1 = `on_draw_37` `0x419950`（**先切相机 0**）；group 3 = `on_draw_3f` `0x419b10`（切相机 2 全表面）；
   group 2 = `on_draw_48` `0x419a80`（相机 2 但视口换成 `camera+0x124` = 游玩区矩形）；
   group 0 = `on_draw_58` `0x419940`（**优先级 0x58 = 最后** ⇒ 盖在立绘（组 44 = `0x57`）之上）。
   卡的 `on_draw` = `AbilityManager__on_draw` `0x408ab0` 注册在 **`0x33`** ⇒ 四个趟都晚于它。
   关键：暂停模糊罩 = 层 38 → 优先级 **`0x3e`** ⇒ **只有 group 1（`0x37`）早于它**。
   ⇒ 季节档位数字与绀珠传面板数值行**都改成 group 1**（`season.c` 的 `ce_hud_text_ex` 实参、
   `lolk.c` 的 `LOLK_GROUP`，TH15 原值 2 已偏离，注明）。
   ⚠️ **实跑证伪：group 1 下两处文字全部看不见**（用户 2026-09-10：「你刚才改的所有 ascii 数字
   都看不到在哪了」）——因为 group 1 的趟（优先级 `0x37`）跑的时候**渲染目标还不是后备缓冲**：
   0x2c 把 RT 切到 **surface 1**（装层 20–23）、0x3a 才回后备缓冲，而 0x37 这一趟跑在这中间，
   **还先把相机切成相机 0**（视口 = 游玩区矩形）⇒ 串落进 surface 1 的相机 0 坐标系、跑到视口外。
   **结论：ASCII 文字无法落在暂停模糊罩之下**（唯一的「早于罩子」的趟画不到屏幕上）。
   本轮**回退**：档位数字回 **group 3**（`0x3f`，卡之后、立绘 `0x57` 之前，空间与 group 0 一致）、
   `LOLK_GROUP` 回 TH15 原值 **2**。要连数字一起被罩住**只能把数字做成贴图 VM**
   （UI 表面层 24 → 组 37 → `0x3c`，早于罩子 `0x3e`）——见「没做」。
   （经验规律仍成立：数字/文字坐标空间 = **表面 px = 2 × ASCII 坐标、原点左上**，与相机无关。）
5. **★★ 击破掉落「竖着一列同 x 坐标」修掉（两个根因）**（用户：「有的时候击破小怪掉季节点会出现
   竖着一列同 x 坐标的掉落，和 th16 的状态不符；th16 的击破掉落基本都是 x 和 y 在小怪附近随机分布」）：
   - **根因 A（速度常数用错通道）**：`spawn_point_burst` 的初速用了 `1.2 + rand×1.0`（1.2..2.2）——
     那是**另一条通道**（按累计伤害掉，`EnemyData__step_logic` `0x41c769` 的 `[0x494510] = 1.2f`）。
     一手核对死亡掉落通道 `EnemyDrop__eject_extra_drops` `0x41d700` 的 type 0x10 支（`0x41d880`–`0x41d8bb`）：
     **速度 = `randf_0_to_1() × 1.9 + 0.2` = 0.2..2.1**（`FMUL [0x494548]` + `FADD [0x494458]`）。
     ⇒ `SEASON_POINT_THROW_MIN = 0.2f` + 乘数 `0.0019f`。
   - **根因 B（缺「飞散」相位，真正让点排成竖线的那个）**：TH16 的 type 0x10 **出生不是「飘落」**——
     `0x430960` 把 0x10 放进「消弹道具池」，出生态是 `item+0xc50 = 6`（**待发射**，错帧延时
     `item+0xc60 = total_items_created & 3`、VM 都还没建），延时到点后 `Item__init_anm_for_piv_or_season`
     `0x430c90` 建 VM 并转 **state 3**：`pos += vel × GAME_SPEED`、`item+0xc20 -= 0.03`、
     `vel = polar(item+0xc24, item+0xc20)`（`0x42f82c`–`0x42f83f`）——**角度全程不变、速度线性衰减**，
     飞 `速度²/0.06` = **0.7..73 px** 才停，**这一段没有重力、也不清 `vx`**；飞完才转 state 1（飘落，
     重力 + `vx = 0`）或 state 4（放 B/回收线）。
     我们原来让点一出生就进「飘落」⇒ 朝下的那半点立刻被 `vy ≥ 0 → vx = 0` 钉在敌人的 x 上，
     又因为一批点是**同一坐标**出生的，就排成一列 ✓ 与用户观察完全一致。
     ⇒ `season.c` 现在照抄这条状态机：`state 0`（待发射 + 1..4 帧错帧 + VM 延后到发射帧才建）→
     `state 2`（飞散：直线 + 每帧 −0.03）→ `state 1/3/4`；**吸引半径的门只对 state 1 开**
     （TH16 state 3 里没有吸引检查）。新增字段 `lang/lr/delay` + `season_card_t.spawn_seq`。
   - 一手文档同步：[`engine/season/th16/01-season-data-and-tables.md`] §4 补「状态机」表 + 两条通道的速度对照，
     并把「重力/`vx = 0`」标注成**仅 state 1**。
- 产出：DLL `c01f5a2fdbdbe7e35b66974685a0771b`、`ability.anm` `d92211749551927566cab41f0feb964d`
  （`mods/`、`thcrap/bin/`、patch 仓库三处同步）。位置按用户口径「待会再调」——下一轮按截图像素
  微调 `SEASON_GAUGE_BX/BY` 与数字/面板文字坐标。

**2026-09-10 实跑第三十二轮（用户定案：「那就改：只能把数字改成贴图 VM」）**：
**季节档位数字 + 绀珠传面板五行数值全部改成贴图 VM** —— 两处原来都是 ASCII 文字，而
**ASCII 文字无法被暂停模糊罩盖住**（31 轮实跑证伪：唯一早于罩子的 group 1 趟跑在 surface 1 + 相机 0 上，
画出来看不见）。做法：
1. **字形用零售切图**（铁律）：`assets/ability/make_font2_art.py` 从
   `local/th18.v1.00a/anm/ascii1280/ascii/ascii_1280.png` 按 `ascii1280.anm.txt` 的矩形切
   **TH18 font 2** 的 15 个字形（数字 = sprite 196..205，`-`/`%`/`.`/`+` = 208/210/211/212，
   一手映射 `0x41a2b0` 的 case 2/3）→ `font2/FONT2_*.png`（**16×20 整格**，`.` 的墨迹在格内第 12–17 行，
   按墨迹裁会把它抬到基线上方）→ ORDER 里按 `16x32` 声明（构建脚本居中补透明）。
2. **脚本 136 `hud_glyph`**：`layer(25)` + `resolutionMode(2)` ⇒ UI 列表层 25 被 `0x4882b8`
   折到 **38**，与暂停模糊罩（`text.anm` script59 = UI 层 38，`FUN_00458480` 一手：
   `instantiate_vm_to_ui_list_back2` 建在 **UI 列表**）**同一个绘制桩 `0x3e`**；
   罩子暂停时每帧重起（尾插 ⇒ 画在后面）⇒ 数字被罩住、平时全分辨率。
   （选 25 不选 24 的原因：层 24 与季节槽/面板底板同层，底板重起时会盖住数字。）
3. **SDK 新增 `ce_hud_vm_text_t` / `ce_hud_vm_text_set/clear`**（`native/sdk.c`），排版**逐条照抄**
   `AsciiManager__draw_string` `0x41a2b0` 的 font 2 分支：字距 `scale_x × 7`（`.` = `× 4`）、
   水平对齐 0 居中 / 1 左 / 2 右、垂直对齐读**环境** `mgr+0x19254`（0 居中 / 1 顶 / 2 底，
   行高 `scale_y × 10`）、坐标仍是 640×480 空间（内部 × `DAT_0056aca0`）。
   一手几何（锚点表 `0x4cdb00`/`0x4cdb30`、`flags_lo` bits 22–25、分辨率位清 0）已写进
   [`engine/anm/th18/03-ascii-text.md`](../../../engine/anm/th18/03-ascii-text.md) §2f。
4. 调用点：`season.c` 的档位数字（`"%d"`，居中，颜色仍取 7 档表）、`lolk.c` 的五行
   （`"%d"` / `"%3.1f%%"` / `"%3d"` / `"%6d"` / `"  +%d"`，右对齐、scale 1.0）；
   `hud_hide` / `panel_hide` / `clear_card` 里一起 `ce_hud_vm_text_clear`，不留孤儿 VM。
   ⚠️ 位置**按原 ASCII 坐标原样传**（`(182,455)`、`(300,222)` 一族）—— 排版公式与引擎一致，
   所以屏幕落点应当与改之前**逐像素相同**；若实跑发现整行上下偏 ~10 px，那是环境 `align_v` 的锅，
   报个数就调。
5. **★ 截图逐元素对位（同一轮，用户 1280 档 / 100% 截图量的「最终渲染还要挪多少」）**：
   用户口径 = 最终渲染上的 px 位移，代码侧按**相对位移**改（与底层相机偏移无关）：
   - 季节槽：外框/进度条/背景条 **+8 x**（`hud_ensure` 与 `on_draw` 每帧写位置两处都改）、
     季节字 **+7 x**、releaseable 条 **+37 x / +8 y**；
   - 档位数字（贴图 VM）**−172 x / −6 y** ⇒ ASCII 空间 −86/−3：`(182,455)` → **(96,452)**
     ⚠️ 落点 = 表面 x≈185，在游玩区左边界 256 之外的黑边里（与槽分开）—— 待用户实跑确认是否是本意；
   - 绀珠传面板 PNG **+191.5 x / +30.5 y** ⇒ `LOLK_PANEL_AX/AY` (448,371) → **(639.5, 401.5)**；
     五行数字 **+7.5 x / −1.5 y**（表面 px）⇒ ASCII +3.75/−0.75：`LOLK_VAL_X/X_RATE/Y`
     (300, 308, 222) → **(303.75, 311.75, 221.25)**。
   - **外框改成「四条 1px 矩形拼」**（用户先量了「右边界内收 1 / 左边界左扩 2 / 上边界上扩 1，
     里面的矩形不动」= 实心矩形版能做到 左2/右1/上2/下1；随后问「能不能用四条 1px 的长方形拼出来」）：
     TH18 没有 `drawRectBorder`（用了崩），所以外框本来一直是**实心 `drawRect` 打底 + 内部条盖住**；
     现在改成**真正的 1px 描边**——脚本 123 变成 **1×1 矩形模板**（尺寸全靠 VM `scale`），
     `season.c` 的 `k_season_frame[4]` 逐边给 (中心偏移, 尺寸)，**四条边都 1px 粗**：
     上 `103×1`（行 910）、下 `103×1`（行 920）、左 `1×9`（列 263、行 911..919）、右 `1×9`（列 365）
     ⇒ 外沿 `x 263..365 / y 910..920`（103×11）。★ 用户定案：「粗细还得是 1px，**有像素会露出来**」
     —— 里面条 100×8 @ (265..364, 912..919)，上/左各差 1px ⇒ **行 911 与 列 264 露出背景**，
     右/下贴着（这就是原版「填充在框里不居中」的那 1px 观感，不是 bug）。
     四条边互不重叠（横条整宽、竖条只占中间 9 行），四角由横条覆盖 ⇒ 淡到 alpha 64 时角上不叠色。
   - **亚像素微调（同一轮）**：季节等级数字再 **左 0.5 / 上 0.5 表面 px**（ASCII −0.25/−0.25 ⇒ (95.75, 451.75)）；
     chapter finish 的五行数字再 **上移 1 表面 px**（ASCII −0.5 ⇒ `LOLK_VAL_Y` 221.25 → 220.75）。
     `hud_frame` 由一个 int 变成 `hud_frame[4]`（生成/位置/interrupt/颜色/删除五处都循环处理）。
     ⚠️ 与上一版（实心矩形、左2/右1/上2/下1）相比：**左/上灰边变 1px、外沿内收 1px**；要加粗某条边
     就改 `k_season_frame[]` 里那一行的 `sx/sy` 与 `dx/dy`（注释里给了示例）。
   - 映射复核（用户提示「各层相机的基本映射」）：UI 列表 VM 的层 24 由**优先级 0x3c 的桩**画、
     层 25 折到 38 由**0x3e 的桩**画（`0x4878d0`/`0x4878e0` 都是「push 层号 + `render_layer` + ret」的
     空壳），两桩都不改相机/2D 偏移 ⇒ 沿用 0x3c 桩留下的 `AnmManager+0xd0/+0xd4 = 0`、
     `+0xd8/+0xdc = camera2+0x104/+0x108`（一手 `0x487b12`–`0x487b40`/`0x487ba2`）⇒
     **层 24 与层 25 的最终映射相同**，表面 px 常量可以 1:1 套位移；只有 ASCII 空间的元素要 ÷DAT。
- 产出：DLL `e79cc1bd23b63460519a89a53ab4ecb3`、`ability.anm` `8c74e41f0d147ec6457df94ac607c6d8`
  （entry 87→101、script 136→137；`mods/`、`thcrap/repos/..._255/`、patch 仓库三处同步）。

**2026-09-10 实跑第三十三轮（用户四条反馈：季节道具掉两次 / 季节环颜色 / 冬残影图层 / 敌弹出擦弹圈变透明）**：

1. **★★ anm `color` 是 RGB，我们的打包宏写成了 BGR（红蓝对调）** —— 用户「春天的季节释放环是蓝色的？
   夏天是红色的？」+「我记得好像是 bgr」直接命中。一手：`AnmVm__run` `0x479f56`–`0x479f8f`
   （`color` 指令：arg0→`vm+0x526`、arg1→`+0x525`、arg2→`+0x524`），而 D3DCOLOR `0xAARRGGBB`
   在内存里是 `BB GG RR AA` ⇒ **arg0 = 红**。`season_core.c` 的 `RGB888` 原为 `(b<<16)|(g<<8)|r`
   ⇒ 春的内圈 (255,128,128) 画成蓝、夏的内圈 (128,192,255) 画成橙红。改成 `(r<<16)|(g<<8)|b` ✓；
   `test_season_core` 里那 7 条「化石期望值」同步订正（89 passed）。
   ⚠️ 颜色值本身没错（照 TH16 脚本字节），错的只是打包顺序。
2. **★ 敌方弹出擦弹圈后「极其透明」** —— LoLK 弹染色的还原把**整条 color_1（含 alpha）**写回
   「染色前那一帧的快照」，而弹的 alpha 由脚本 `alphaTime`/`colorTime` 逐帧在变 ⇒ 快照里的
   alpha 常常偏低（刚出生淡入时更低），一还原就永久钉死。现在**只改/只还原 RGB**（`(*p & 0xff000000) | rgb`），
   alpha 完全交给引擎插值器 ✓。季节点的黄染同理（保留当前 alpha，不再每帧写死 255）。
3. **残影颜色**（用户录屏对账 + 一手 `0x442ccc`–`0x442d33`）：TH16 **冬**在残影 VM 出生后写一次
   `color_1 = 0xffff0000`（红）；**秋不写残影颜色**、只把**自机本体**染黄（`player+0x538 = 0xffffff00`）
   ⇒ 秋残影保持脚本自带的 `color(60,0,192)` = **蓝紫**（RGB 顺序）＝ 用户视频里的「秋天蓝色虚影」。
   我们原来把黄写到了残影上 ✗ → 现在只在冬写红，且**只在出生写一次**（每帧重写会按掉脚本的淡出，
   残影变成一整块不透明 —— 这也是「冬天那个残影像盖在自机上面」的观感来源：脚本 layer(13) 本来就
   在自机（layer 14）下面，但一块不透明的残影盖在自机上看着就像在上面）。
4. **季节道具同一结算点掉两次**：去重环 4→8 槽、键加位置，判据 ① 90 帧内同敌人 ② **同位置 300 帧内**
   （兜 boss 的「死亡路径 + `BossDead` 子程序里的 `dropItems`」隔几百帧那种；相位切换 boss 会位移
   ≥50 px，日志实测，所以不会误吃「每相位一掉」）。抑制时都打日志（boss/mid 分别标）。
5. **★★ 季节解放是「两个同心圆」，不是「一个沿 r 渐变的圆」**（用户口径）。一手反编译
   `BombSubSpring__begin` `0x411460`：脚本 3 的 VM 拿 `%F0 = %F1 = RELEASE_LEVEL_RADIUS_<季节>[level]`，
   脚本 4 的 VM 拿 **`RELEASE_LEVEL_RADIUS_<季节>[level] + 16.0`**（`$I2` 才是 `RELEASE_LEVEL_DURATION_*`）。
   ⇒ 第二个圆半径 = 第一个 **+16**：亮（加算、alpha 128）的那个在 r+16、暗（减算）的在 r，
   两层叠出 TH16 那个「更粗的描边」。我们原来两圈同尺寸（= 一个渐变圆）✗ → `ring_tick` 里
   `si = sc + 16.0f` 给 `ring_in` ✓。
6. **★★ 掉两次：不用去重了 —— 改用「引擎自己的计数表已清零」当判据**（用户：「除了去重，没有别的方法吗？」）。
   一手：`Enemy__drop_items_and_notify_cards` `0x430510` 的**尾段**（反编译）
   `for (卡链表) card->vt[0x30](pos, this+4); _memset(this+4, 0, 0x50);` —— **引擎自己在通知完卡之后
   把 20 个 type 的计数表清零**。所以同一个「击破/结算事件」的第二次回调（ECL `dropItems` → `0x42d0c0`）
   看到的计数表**必然全零**（除非 ECL 又显式 `dropClear/dropExtra` 配了掉落，那是一次真掉落）。
   ⇒ `on_enemy_drop_pre` 开头把 20 个计数加一遍，`<= 0` 直接 return ✓；去重环（时间窗 + 位置）整段删掉。
   比去重干净：不猜窗口、不会被虫群顶掉记录、不会误吃真掉落。
   代价：ECL 完全没配掉落的敌人不再给季节点（TH16 也是「ECL 里的 `dropSeason` 决定掉不掉」✓）。
   日志：`boss drop … t=` 保留帧号，去重相关的 `drop dup suppressed` 行不再出现。
   ★ 但用户实跑反馈「小怪掉得变少了、boss 还是掉两次」⇒ 这一版再订正成**计数表 + 每个敌人的闩锁**：
   - 计数表非零 → 真掉落 ✓（boss 每攻击配一次掉落 = 一攻击一掉）；
   - 计数表全零 **且是小怪** 且该敌人还没掉过 → 兜底掉一次（TH18 有些敌人压根没配掉落表，
     但它们该和 TH16 的小妖精一样掉 8 个）。闩锁 = **`enemy+0x6838`**（zEnemy 的死字段，
     全 exe 只有 `0x42e427` 初始化时写 0，见 `engine.h` 的 `CE_ENEMY_DDC_UNUSED`）；
   - 计数表全零 **且是 boss**（或已掉过）→ 跳过（= 同一次事件的第二次回调）。
   ⚠️ boss **不做**兜底：否则它会在「没配掉落的死亡」上多掉一次 = 用户看到的「boss 掉两次」。
   - 新增诊断：每 600 帧一条 `season: drops in N frames: mid x, boss y, skipped-dup z, points p`
     （静态统计，见 `points_tick`），下一跑可直接对「哪些敌人没掉 / boss 掉几次」。
   - ★★ 用户下一跑日志（02:12）读下来：小怪已恢复（每 10 s 掉 20~25 次、每次 8 点 ⇒ 与 TH16 同量级），
     `boss 1` 每窗口、**没有同一结算点的两条 boss drop**（t=3820/7104/8303/12367，两两相隔 1200~3300 帧）。
     但日志里有一条关键线索：`boss drop … spawned 93, queued 107` —— **池只有 96，boss 一次 200
     装不下**，剩下的 107 颗要等玩家把场上收空才一颗颗补出来（`burst_tick` 池满时生不出来），
     看着就像「boss 又掉了一次」。⇒ `SEASON_POINT_POOL` **96 → 512**（TH16 原生道具池是 4696 槽，
     200 颗本来是一口气全出）；`SEASON_DROP_CAP` 400 → 512。

**2026-09-10 实跑第三十四轮（用户：「小怪是每次掉 8 点吗？我记得不是有一个什么小怪的出场时间推移打死掉得会更少？」+「这回是 boss 一次性全掉完了」）**：

1. **★★ 用户记忆正确：TH16 的小怪掉落按「出场计时」线性衰减** —— 一手（`engine/season/th16/01` §5.2）：
   `dropItems`（opcode 509，handler `th16:0x420585`）在**结算那一刻**算
   `n = (N−M) × bonus_timer.current / T + M`（`current < 1` → `M`），而 `bonus_timer` 由
   `dropSeason(T,N,M)`（`th16:0x422a26`）在敌人子程序开头设成 `T`
   ⇒ **出场 T 帧内击破给满 N，之后线性降到 M**。
   TH16 道中几乎清一色 `(60,10,1)`、小妖精 `(60,8,1)`、大妖精 `(120,13,1)`/`(120,20,1)`/`(200,30,1)`、
   st06 的大只 `(600,100,1)`、st07 `(60,40,1)`；boss `(1200,200,10)`、终盘 `(2400,400,400)`（不衰减）。
   我们原来**恒掉 8** ✗ → 现在按 TH16 小妖精那套 **`(60,8,1)`** 结算（TH18 没有 `dropSeason`、
   逐敌人的 (T,N,M) 表也拿不到 ⇒ 取道中最常见的一族；这一条记在 `engine/season/th18/01` §4）。
2. **计时器 = 引擎自己的 `zEnemyData.time_alive`** —— 一手：敌人每帧入口 `FUN_0042ff80`（`this` = zEnemyData 基址）
   尾部 `0x430209` `LEA ECX,[ESI+0x2d0]; CALL 0x405990 (Timer::increment)`（同段 `[ESI+0x50f0]` 就是
   invuln_timer，可交叉核对）⇒ `time_alive.current` 在
   **zEnemyData+0x2d4** = **enemy+0x1500**（新常量 `CE_ED_TIME_ALIVE`，随游戏速度缩放，每帧 +1）。
   公式搬进纯逻辑 `season_core.c` 的 `season_drop_count_decay()`，单测 +16 断言
   （`test_season_core` 89 → **105 passed**）。
3. **诊断**：小怪掉落也打一行 `season: mid drop N points @(x, y) (enemy id …, alive …)`，
   实跑可直接对「alive ≤ 60 时 N 从 8 线性降到 1」。
4. **boss 的 200 颗仍是一次性全出（不是 bug）** —— TH16 的 `dropItems` 调 `EnemyDrop__eject_all_drops`
   就是**一口气全弹出来**，每颗再按 `spawn_seq & 3` 错 1..4 帧、随机角度 + 0.2..2.1 初速散开。
   之前看到的「一阵雨」其实是池只有 96 时的溢出排队（第 33 轮第 6 条），池给到 512 后回到原版行为 ✓。
   boss **不**按计时衰减：TH16 每个相位各调一次 `dropSeason(1200,200,10)`（计时器每相位重置），
   所以每个相位的击破恒 200 颗。

**2026-09-10 实跑第三十五轮（用户：「th16 有不同档位吗？」→ 选「原生 T + 原生计数映射到 TH16 量级」）**：

1. **TH16 的档位不是表，是逐敌人手写的 `dropSeason(T,N,M)`**（普查见 `engine/season/th16/01` §5.3）：
   小妖精 `(60,8,1)`、妖精 `(60,10,1)`、大妖精 `(120,13,1)`/`(120,20,1)`、大只 `(200,30,1)`、
   st06 巨物 `(600,100,1)`、st07 `(60,40,1)`、boss `(1200,200,10)`、终盘 `(2400,400,400)`（不衰减）。
2. **★★ 顺带挖到：TH18 自己有一套同形的逐敌人档位，而且是活的**（一手，`engine/season/th18/01` §1b′）：
   - ECL **opcode 574** → `0x433a70` → `FUN_0042d040(drop, T)`：写 `zEnemyDrop.timed_ex_drop_time(+0xa4)` + 复位计时器；
   - ECL **opcode 573** → `0x433a4a` → `FUN_0042d020(drop, type, count)`：写 `timed_ex_drop[type](+0x54…)`；
   - 结算（`0x430510` 一手）：`n = ex_drop[i] + timed_ex_drop[i] × (T − timer)/T`；
   - 计时器（`FUN_0042ff80` `0x430161` 段**内联**，不是 `Timer::increment` 调用）每帧 +1。
   ⇒ **和 TH16 的 `dropSeason` 同形同向**（出场 T 帧内给满、之后衰减到 `ex_drop`）。
   零售 22 个 `*.ecl` 全扫出 **64 处 `574`**：小怪 T=40..120、大只 200..400、道中 boss 600/900，
   计数 1..20（`*bs` 里一处都没有 ⇒ boss 不走这条）。
   ⚠️ 顺手订正本仓旧结论：`engine/season/th18/01` §1b 早前写成「随时间线性补足」——**方向写反了**，
   已改（`iVar4 − iVar4×timer/T` 是递减）。
3. **卡侧改法**：小怪掉落的 `T` 直接读敌人自己的 `timed_ex_drop_time`（`enemy+0x623c+0xa4`），
   数量用「原生 timed 计数」当档位键映射回 TH16 量级（`season_core.c` 的 `season_drop_native_tier`：
   1→8 / 2..4→10 / 5..10→13 / ≥11→30），`M` 恒 1；**没配原生窗口的敌人（T=0，含所有 boss）
   退回 TH16 小妖精 `(60,8,1)`**。boss 仍恒 200（每相位一次）。
   ⚠️ 映射本身是**自定**（两边的数字都一手，但跨作对应关系无原版依据）—— `season_core.h` 已标。
   单测 105 → **120 passed**；日志加 `T … native …` 两个字段，实跑可逐个敌人对账。

**2026-09-10 第三十六轮（用户指出边界情况：长持续效果撞上倒吊人删卡 —— 商店里那一下）**：

用户问：「我一个持续时间很长的效果，可能在商店还触发着，此时碰到倒吊人怎么办」。查下来是**真 bug，且不止一张卡**。

1. **为什么这一下特别危险**：倒吊人（74）是**商店一次性购买**的即时卡 —— 干活的是它的 `ctor`
   （`hanged.c`：随机挑一张别的卡 + `ce_destroy_card` + 给钱），也就是说**删卡发生在商店里**。
   而商店期间 `AbilityManager__on_tick__stub` `0x408a90` 被 `ABILITY_SHOP_PTR != 0` **冻住**
   （一手见 `engine/card/th18/05-shop-and-money.md` §「冻结靠一个指针」）⇒ 被删那张卡的
   `on_tick_2` / `on_active_tick` 当场停摆、`on_load` 也不会再来、`on_run_reset` 要等局末。
   **唯一还能跑的收尾点 = `on_delete`（虚表 `+0x50`）**：`ce_destroy_card` 直接调
   `[vt+0x50](victim, 1)` → SDK 的 `opdelete` 桩 → `on_delete` → 才 `ce_state_free`。
2. **审计结果**：凡是「自己建 VM / 占子机槽 / 建 HUD 数字」的卡，只要**没写 `on_delete`** 就会漏。
   逐卡扫 `ce_anm_spawn*` / `ce_option_alloc|claim` / `ce_hud_digit_set` 后命中 7 张：

   | 卡 | 漏什么 | 处置 |
   | --- | --- | --- |
   | 67 青眼白龙 | 龙脚本 78 + 两条血条 86/87 都是 `stop()`（不自己 delete）⇒ 龙和血条永久钉在屏幕上 | 新增 `on_delete = dismiss(c, "card destroyed")` |
   | 72 炎魔之王 | 炎魔本体 + 血条 VM（同款 `stop()` 脚本）| 同上 |
   | 71 破损核心 | 子机 **options 槽**（`Player__allocate_option` 的 12 格**没有别的回收者** ⇒ 每漏一次少一格）| `on_delete` → `ce_option_drop` |
   | 77 豌豆射手 | 同上 | 同上 |
   | 75 真银圣剑 | HUD 耐久数字 VM（`ce_hud_digit_set` 建的是**独立 VM**，删卡的 `create_all_card_lists_for_hud` 带不走它）| `on_delete` → `ce_hud_digit_clear` |
   | 78 衔尾蛇 | 同上（层数数字）| 同上 |
   | 115 绀珠传 | 面板 6 条 VM + 五行数字（`on_delete` 里原来只收擦弹圈）| `on_delete` 补 `panel_hide(s)` |
3. **没动的**：64 反转牌 / 66 神之宣告只 spawn 一个**瞬时闪光** VM（脚本 68/77 尾部都是 `+10: delete()`，自己收），
   不需要 `on_delete`；写全局旋钮的卡（60 黑桃 Q 的一次性道具半径、59/61/76 的伤害倍率等）要么每帧重写、
   要么走 SDK 的按卡聚合（删卡即退出）✓。
4. **实跑验收（下一跑顺手做）**：商店买倒吊人，让它删掉正在生效的卡 ——
   - 67/72：日志应出现 `blue_eyes: dismissed (card destroyed)` / `firelord: dismissed (card destroyed)`，屏幕上**不留**龙 / 炎魔 / 血条；
   - 71/77：删卡后子机消失，且**再买同卡还能正常出子机**（不在 12 格池里留尸）；
   - 75/78：HUD 小卡上的数字跟着图标一起没；
   - 115：波次结算面板（及其五行数字）当场收掉。

**没做（下一步）**：绿分数道具（TH18 无此概念）、土用「消弹圈向后移动」、子机按档位换弹型；
**动物灵卡（id 120）** 只有设计（击杀点撒灵 / 收集槽 / 咆哮抬伤害 / `on_death_before_deathbomb` +
`cancel_impending_death` 实现「中弹只退咆哮」），待实现。

## 1v. 鬼形兽（id 117，2026-09-10，Stage 2 待实跑）—— 动物灵 / 咆哮

TH17 动物灵 / 咆哮的卡侧复刻。一手 [`engine/animal/th17/01`](../../../engine/animal/th17/01-animal-spirits.md)
（本次会话补反编译了 `0x410af0` token tick / `0x40f980` 收集 / `0x410640` 鱼灵换道具 / `0x4491b0` 真死）；
TH18 无残骸审计 [`engine/animal/th18/01`](../../../engine/animal/th18/01-animal-residuals.md)。
实现 `native/cards/animal.c` + `animal_core.c`（纯逻辑，主机单测 **116 断言**）；
资源 `assets/ability/make_animal_art.py`（零售 `beast1.png` 13 单元 + `beastf.png` 槽框 + 脚本 137–145）；
审计 AUDIT §AL。**零新断点 / binhack / codecave**，只新增 `th18:0x45d590`。

**同时登记的空壳卡**（id 106–113，只有 JSON、无行为）：红魔乡 / 妖妖梦 / 永夜抄 / 花映塚 / 风神录 / 地灵殿 / 星莲船 / 神灵庙
—— 见 [`CARDS.md`](CARDS.md) 的新表；下一步按 [`IDEAS.md`](IDEAS.md) 的批次表逐张填。

**叠 `_255` + DLL 启动**（`_test` 的起手卡组 = **`[117, 21, 22]`** —— 引擎编成上限 3，
所以 117 必须排第一；21/22 = 道具回收范围 / 回收线两张原版被动卡），日志应出现：

```
sdk: 117 bound (.active_recharge = 1, .on_activate = …, .on_tick_2 = …, .on_draw = …, .on_enemy_drop_pre = …, .on_death_before_deathbomb = …, .on_death_frame2 = …)
wba_wc: hyper ready (type 1 rush, color 1, 1980 frames [goast +480], goast 0, queue 1 1 1 1 1)
wba_wc: roar auto-start (type 1, 1980 frames, beast effect = wolf shot set)     ← ★ 2026-09-11：满槽即自动
wba_wc: shot set -> wolf (main 0..9 = slots 30..34, sht …)
wba_wc: hyper #1 by key (X bomb / C card; type 1)   ← 按 X（或 C）把咆哮用成 40 帧霊撃才有
wba_wc: hyper end #1 (normal; 2 fish -> items; release anim …, burst in 120 frames)
wba_wc: miss -> 2 non-cycling animals (type 1)
```

体感：杀敌掉出会**弹跳**的动物灵 / 鱼灵（贴图 = TH17 零售切图）→ 飞过去收 → 左下角 5 格灵槽逐格亮 →
**满 5 立刻自动起咆哮**：暴走（同类 ≥3）时**主炮当场换成兽灵专属弹幕**（狼 = 7 发散射、鹫 = 5 发诱导，
獭 = 3 颗旋转光球），自机左下角亮咆哮条、身上挂兽灵光环；咆哮期间中弹**不扣命**（拿咆哮抵消）。
**按 X（炸弹键，TH17 原位置；C 也行）**= 把这股咆哮**用掉**（40 帧消弹 + 伤害 + 无敌，手感 = 「中弹但只丢咆哮」）；
不按就等它跑完 → 释放动画 + 爆散动物灵。

**★ 调试键（2026-09-10，用户要求）**：本卡在卡组里时，**拍空格**从画面中央 (ECL x=0, y=224)
召唤一只随机**动物灵**（type 1..3，变色版）。日志 `wba_wc: DEBUG spawn animal type N at (0, 224) by SPACE`。
（2026-09-11 用户口径：把这行定成**随机 1..3** —— `ANIMAL_DEBUG_TYPE_MIN/MAX = 1/3`；同时把 Shift+空格的
资源灵循环从借用的 MIN/MAX 改成独立的 `ANIMAL_DEBUG_FISH_MIN/MAX = 4/7`，此前 MIN=MAX=1 时它只会出狼灵。）
`ce_key_down` 走 `GetKeyboardState`（user32），空格不在游戏配置里、**不进 replay 输入流**。
发布前把 `animal.c` 顶部的 `ANIMAL_DEBUG_KEY` 改成 0。

**要调的地方（实跑第一遍）**：

1. **掉落已完成三轮修正（2026-09-11）**：
   - boss / 中 boss 死亡（`BossDead` / `MBossDead`）→ `token 4 + token 5` 各 1（2 资源灵），日志 `wba_wc: boss death -> 2 resource spirits (4+5)`；
   - 符卡相位结束 → 1 只随机动物灵（`type 1..3`，不再 13% 点鱼灵；TH17 `@BossItem(33)` 对应 site 没有点鱼）；
   - 普通 / 血条相位结束 → 2 资源灵（4+5），日志 `wba_wc: phase drop normal -> 2 resource spirits (4+5) (sig A -> B)`；
   - 同一相位只结一次（TH18 多条 `BossItem` / 相位边界 + drop 事件双触发都用 `phase_drop_seen` 去重）；
   实跑看一关够不够攒满 5 格，不够再调别处；
2. 灵槽几何 `ANIMAL_HUD_X0 / Y / DX`（300 / 924 / 36，表面 px）—— TH17 是 `pos(-360+36i, 872)`，
   坐标系不同，待实跑量；
3. 灵速（`token_spawn` 的 `spd = 1.0` + 60 px 减速 ×0.5 + 速度下限 0.5）—— 用户要的「DVD 约 45°」；
4. 延长表 `animal_next_extend`（用户口径 90/60/42/…）与 TH17 代码的 180×2/3（120/80/53）二选一；
5. 霊撃伤害源 40/帧、半径 `(1−f⁴)×200`；
6. **音效 id 对照**（2026-09-11 二次重解，见 AUDIT §AL29）：TH18 的 `id -> wav`
   看的是 `u32[0x4c9b80 + id*0x14 + 0x04]`，不是 `+0x00`。因此
   `0x3f` = se_lgods4（撒道具）、`0x43` = se_pldead01（咆哮正常退出）；
   旧代码的 `0x34`/`0x38` 实际是 se_don00/se_ch03，已改。

**X 炸弹拦截（2026-09-11）**：咆哮 `ready || hypering` 时，SDK 新增断点 `BP_ce_bomb_gate` @ `th18:0x45c048`
（`Player__on_tick__body` 普通炸弹分支的 `test BYTE PTR ds:0x4ca434,0x2`）询问本卡；本卡返回 1 时
SDK 把执行从 `0x45c048` 改到 `0x45c056`，**跳过 `0x45c051` 的 `call do_bomb`**。不咆哮时返回 0，
真实炸弹不受影响。首次拦截打印一次 `wba_wc: bomb suppressed while roaring`；实跑请确认「咆哮中 X 只出霊撃、不扣 B」。

**咆哮期间的自机特效（2026-09-10 Stage 3 已接）**：

- **类型光环**：零售 `beastAURA.png` 的 24 个 64×64 单元 + 脚本 146–148（照 TH17 script41–43 生成），
  layer 13 跟随自机；红狼 / 绿水獭 / 紫鹫。日志无关键字，看画面；
- **水獭 3 消弹体（2026-09-11 初相/转轴修正；同日 §AL34 修 ready 消弹）**：零售
  `beastot.png` sprite70（80×64 → 128×64 补边）+ 手写脚本 149。视觉按 TH17 一手：
  **VM 留在自机位置**，脚本补 `anchorOffset(48,64)`，C 在生成时写 `rotation.z = 0 / -120° / +120°`，
  之后由 `angleVel(-0.06981317f,…)` 自转成轨。
  消弹（§AL34）：TH17 是在**满槽咆哮的 ready 期**每帧跑 `0x40f200`–`0x40f372`，判定点
  `player + polar(rotation.z - 0.34906584f, 96.0f)`，**消弹半径 28.0f**（`th17:0x4a3ce0`，
  不是 96；96 只是点离自机的距离），mode=1；TH17 按 X/C 进 40 帧霊撃时会在 `0x40f7f7` 删掉
  3 个 orb，本卡照做。日志：`wba_wc: otter orbs spawn`、每 60 帧 `wba_wc: otter orbs pos`
  （含 `rot=`/`hit=`/`cancel r=28`），每次咆哮另有一条
  `wba_wc: otter orb cancel armed (3 points, r=28, mode=1, state=ready, orbit=96)` 可直接证伪；
- **自机弹染色**：咆哮期间按类型染红 / 绿 / 紫（`on_bullet_created` 写弹 VM 的 color）；
- **就绪窗口条**（2026-09-10 Stage 3 已接）：TH17 `beast.anm` script30/31（= 0x1e/0x1f）—— 空心外框 + 红/黄闪烁填充，
  槽满就在自机上方亮起、按幂等比例朝**中点**收缩（脚本 157/158 + `animal.c` 的 `bar_*`；一手见 AUDIT §AL10）。
- **射击强化（标准版，已被 Stage 4 取代）**：狼 / 鹫暴走把自机弹伤害抬到 ×1.5（`ce_set_bullet_damage_permille`）。

**Stage 4（未做，别当已复刻）**：

- ~~射击强化逐发复刻~~ —— **已接**（2026-09-10）：TH17 `set 10..14`（狼）/ `15..19`（鹫）解码后追加进
  TH18 `pl0X.sht` 的 `0x1e..0x27`（`engine/sht/th17/01`、`02`），咆哮中每帧把主炮 0..9 槽的指针改指过去
  （聚焦档忽略 ⇒ 0..4 与 5..9 同组），结束 / 卸载时还原 —— 只改数据指针、零代码改动。🟡 实跑要确认
  咲夜 / 早苗的弹贴图替身已按 TH17 源 sprite 追加（2026-09-11 三修）；`hit6→hit4` 的常数差、
  `mode==2` 在主炮路径下会不会从 (0,0) 出弹（见 AUDIT §AL11）；
- 稀有动物灵（8..14）**不做**；「+480 加成」**2026-09-11 定案**：TH18 当「主凭依灵不存在」⇒ 默认不加，
  卡组带「弱肉强食之理」(25) /「鬼杰组长的恐吓」(30) 时视为主凭依灵恒存在 ⇒ 任何暴走都 +480（AUDIT §AL25）；
- ~~就绪光效（0x1e/0x1f）~~ —— 就是上面那条**就绪窗口条**（已接）；
  原版 15 子 VM 槽的「就绪光圈」仍缺（霊撃三色光圈脚本 150–153 已接）；
- 灵槽的 TH17 原版 15 个子 VM（scripts 14–28）逐脚本移植 —— 现在是「槽框 + 图标」两层简化版；
- **HUD 布局分配器**（跨卡）：现在季节 116 / 鬼形兽 117 的左下角 UI 各写各的坐标、允许重叠；
  最终要「竖直堆叠 / 部分改右下角」，方案见 [`SDK.md`](SDK.md) §17。

> ★ **2026-09-11 七修（实跑前）**：pl01 兽灵组收尾，见 AUDIT §AL37。
> - **EAGLE** 改 data-only 安全激光：五条 `mode=2 / anm=6 / hit_fx=8`，
>   低 4 位 option 槽 `1/1/2/3/4`（raw `0x11/0x21/0x32/0x43/0x54`）；高 4 位是
>   `mode=2` 的 `player+0x476fc` 状态槽 1..5，避免低 4 位重复时第二行被
>   `Player__shoot_one_bullet` `0x45e96e` 整行跳过。保留 TH17 `damage 80/60/60/50/50`、
>   `hit_h=34`、方向角；不再使用 TH17 row0 的 `mode=2 + opt_slot=0`。
> - **WOLF** 用 Task 2 反汇编复核后放回 TH17 五行（`anm=13 / mode=0`）：`PlayerBullet__create`
>   `0x45e723` 取 `player+0x10`，中断子 VM 在 `scriptNew/scriptNewPos` 里按父 VM `+0x1c`
>   继承同一 anm 文件；`player+0x10` 全 exe 只有初始化 + ability 卡两个写者，且都还原
>   `player+0xc`。测试卡组 `[117,21,22]` 无装备卡 ⇒ 主炮时 `player+0x10 == player+0xc`，
>   「ability.anm 串位」不成立；旧「爆炸后出自机」按旧 `pl01.sht` 指向已不存在的追加
>   script50/51、`0x407420` 无边界拷贝处理。`check_player_anm_bounds.py` 现在把
>   `sht anm/hit_fx` 存在性变成构建门。
> - 兜底：`hyper_shot_apply()` 每咆哮帧检查并修复 `player+0x10 == player+0xc`，
>   只在真串位时打 `wba_wc: player anm context repaired`。
> - 实跑看：pl01 WOLF 命中应是橙色 `script14` 爆散、不是自机本体；EAGLE 五条激光不闪退。
>   若 WOLF 仍出自机且日志没有 `repaired`，下一步不要再猜 anm 上下文，保留 WOLF 回退并查
>   旧 patch / sht 指针。

### 1v-补. 咆哮 shot 组的「机体差异」与咲夜 / 早苗待定（2026-09-10 夜记录，等用户次日定）

**TH17 一手事实**（[`engine/sht/th17/02`](../../../engine/sht/th17/02-hyper-shot-groups.md) §1/§2，逐字节）：

- **分机体**：`pl00*/pl01*/pl02*`（霊夢 / 魔理沙 / 妖夢）的狼组、鹫组**字节全不同**；
- **不分火力档**：同一文件里 set 11/12/13/14 逐字节相同（set10 / set15 是空组 = 档 0 没子机就没弹）；
- **不分 shot type**：`pl00a`/`b`/`c` 的咆哮组逐字节相同；
- TH17 的暴走组**复用该机体自己的弹贴图**（霊夢的 7 发用她自己的 anm 5）——「换的是阵型与数量，不是弹的皮」；
- TH17 只有 3 个自机 ⇒ **TH18 的 4 个机体里必然有一个拿不到同源数据**。

**TH18 四个机体的原生主炮**（`local/th18.v1.00a/dat/pl0X.sht` set4 = 非聚焦火力4，一手解码）：

| 机体 | 条数 | 伤害 | 速度 | 角度 | anm | func | 特色 |
| --- | ---: | --- | --- | --- | --- | --- | --- |
| `pl00` 霊夢 | 6（聚焦 10）| 14–20 | 4 / 24 | −1.92..−1.22 | 5 / 7 | (1,1) 寻的 | 直射大弹 + 少量追踪 |
| `pl01` 魔理沙 | 6 | 9–28 | **0** / 24 | −1.68..−1.47 | 5 / 6 / 9 | (2,2,..,2) 激光 | 激光 + 高伤单发 |
| `pl02` 咲夜 | 10 | 8–13 | 16 / 24 | −1.57..**+0.05** | 7 / 9 | (3,0,0,0) | 多条**斜射** + 侧向散布 |
| `pl03` 早苗 | 10 | 10–18 | 5 / 24 | **−2.09..−1.05** | 5 / 8 | (0,0,0,4)/(4,4,0,5) | 宽扇 + 命中型 |

**现在四个机体真的发出的咆哮**（本卡追加槽 `0x1e..0x27`）：

| 机体 | 狼（set `0x1f..0x22`）| 鹫（set `0x24..0x27`）| 来源 |
| --- | --- | --- | --- |
| `pl00` 霊夢 | 7 发大弹扇形 dmg 20–40 / speed 25 / **anm 10**（零售兽灵 `script10`，sprite33）| 五向散 dmg 20 / speed 10 / **anm 11**（零售 `script11`，sprite38/39/40/41）| 直接用 TH18 零售脚本 ✅ |
| `pl01` 魔理沙 | 5 条激光 dmg 8–18 / speed 0 + `tick3` / **anm 13**（零售 `script13`，sprite42/43 + 命中 `script14` 爆炸）| 5 发重弹 dmg 50–80 / `mode=2` 子机槽 16..64 / **anm 16**（零售 `script16`，sprite29 激光条）/ `hit_fx=8` | 直接用 TH18 零售脚本 ✅ |
| `pl02` 咲夜 | **安全回退**：anm 5（零售普通自机弹；不崩、不来自机图）| **安全回退**：anm 6（零售普通自机弹）| identity 回退 ⚠️ 兽灵视觉未达成 |
| `pl03` 早苗 | **安全回退**：anm 8（零售普通自机弹）| **安全回退**：anm 11（零售普通自机弹）/ `hit_fx=7` | identity 回退 ⚠️ 兽灵视觉未达成 |
> ★ 2026-09-11 四修（最终）：TH17 `PlayerBullet__fire`（`th17:0x44a180`）实际读
> **`pl0X.anm script(anm+5)`**（一手 `th17:0x44a535`），不是 `bullet.anm`。而且
> **TH18 零售 `pl00.anm`/`pl01.anm` 里逐字节就有这些 TH17 兽灵脚本**（pl00 8/10/11、
> pl01 8/13/16；entry0 贴图/sprite 表也与 TH17 同名自机一致）⇒ **不再追加**任何
> entry / 脚本，`HYPER_ANM_MAP` 直接把 `anm` 指过去即可。上一轮的「追加 + 抄简」才是
> 魔理沙鹫闪退、狼无爆炸、命中出自机图的根因（见 AUDIT §AL33；AL27–AL31 是前三轮失败记录）。
> `pl02/pl03` 没有 TH17 对应自机，本轮安全回退 identity，**兽灵专属视觉未达成**。


**用户 2026-09-10 口径（次日定案）**：「两人的**得有特色**，比如**咲夜的斜射会影响分布**」
⇒ 形状不能只是照搬别人的，要保住本人弹幕的几何特征（咲夜 = 斜射 / 非对称分布、早苗 = 宽扇）。

**三条候选**（等用户挑）：

| 方案 | 做法 | 结果 | 状态 |
| --- | --- | --- | --- |
| A | 保持替身（咲夜 = 霊夢形 + 咲夜贴图；早苗 = 魔理沙形 + 早苗贴图）| 4 机体只有 2 套形状 | **当前线上**（🟡）|
| B（推荐）| 用她们**自己的原生 set4/9 记录**当底放大：咲夜 = 斜射多排（保留 `+0.05` 侧向）、早苗 = 宽扇双排 + 命中型 | 4 机体 4 套形状，且都是「本人弹幕的暴走版」 | 待做（8 组 shooter，走 `append_shooterset.py`）|
| C | 借 TH17 妖夢 `pl02a`（狼 = 8 条长刀、鹫 = 15 向弧）给其中一人 | 三套 TH17 形状全用上 | 待定 |

**其它待实跑项**（与上面同一批）：`hit6→hit4` / `hit5→hit2` 的常数差、TH17 `mode=1/2` 与 TH18 是否逐位等价、
`mode==2` 在**主炮路径**下会不会因为子机槽为空而从 (0,0) 出弹（若如此把那几条改 `mode=0`）。AUDIT §AL11。

> **2026-09-11 实跑修**：大鹫灵（霊夢 pl00 / 替身 pl02）命中后不消失、无 39/40/41 碰撞动画。
> 根因不是 .sht 的 `damage/mode/hit_fx`，而是 `assets/build_player_anm.py` 生成 slot6 时丢了
> TH17 `pl00.anm` script11 的 `interruptLabel(1)`；`th18:0x45f6f0` 发 interrupt 时没有收尾 VM。
> 已补回命中 interrupt（本体 `base+38`，命中 `base+39/40/41`，30 帧后 delete）；sht 行未改。
> 见 AUDIT §AL31。

> ★ **2026-09-11 四修（用户实跑：魔理沙大鹫灵闪退 + 狼灵无爆炸 + 命中点出自机图）**：
> AL27–AL31 的「TH18 零售 pl0X.anm 没有 TH17 兽灵脚本/贴图，必须追加」**前提是错的**。
> 一手逐字节对拍：TH18 零售 `pl00.anm`/`pl01.anm` 的 entry0 与 TH17 同名自机一致，且
> **兽灵脚本 pl00 8/10/11、pl01 8/13/16 与 TH17 逐字节相同**。所以正确做法是
> **一个字节都不追加，sht 直接指零售 `script(anm+5)`**（`HYPER_ANM_MAP`：pl00/pl01 `5→10 6→11
> 7→8 8→13 11→16`）；`build_player_anm.py` 改成「拷零售 + 硬校验（sprite 越界、脚本存在、
> `scriptNew` 目标、会飞脚本主路径无 `delete`、与 TH17 逐字节一致）」。上一轮的伤害是「为满足
> 『脚本不含 scriptNew/jmp』把 TH17 脚本抄简」：pl01 鹫 `script51` 丢了 `scriptNew(script7)` +
> 全部 `interruptLabel`（真 `script16` 是 mode=2 激光），pl01 狼 `script50` 丢了
> `interruptLabel(1)→scriptNewPos(script14)`（爆炸）——这才对应「闪退 / 无爆炸 / 命中原图」。
> **pl02 咲夜 / pl03 早苗**没有 TH17 对应自机，本轮**安全回退 identity**（`anm` 指各自零售普通
> 自机弹，不崩、不来自机图），**兽灵专属视觉未达成**。见 AUDIT §AL33；DLL 未改，
> md5 `06c88611892f9e104d2b6483e16ea0b3`。

> ★ **2026-09-11 六修（安全优先：魔理沙 pl01 兽灵组整组回退）**：
> 用户实跑仍报「魔理沙大鹫灵闪退 / 狼灵爆炸出魔理沙」。已定位大鹫闪退不是 `hit_fx=8` 或
> `script16`，而是 TH17 EAGLE row0 的 `mode=2 + opt_slot=0` 被 TH18 主炮 `tick2`
> 无条件解引用 NULL option（`th18:0x45fcc8`）。**先保命**：`pl01.sht` 的追加槽 0x1e..0x27
> 不再使用 TH17 wolf/eagle row，改整组拷 TH18 零售聚焦主炮 set 5..9；`pl00` 不变。
> 魔理沙暂时没有兽灵专属弹幕，但不会崩、不会来自机图。狼灵爆炸 sprite 证据（script13/14 的
> `sprite42..51` 不是角色帧，TH17/TH18 字节一致；`hit6→hit4` 也正确对应 interrupt）见 AUDIT §AL36。
> 下一步实跑：(a) 确认 pl01 咆哮不再闪退；(b) 命中处记录 `vm+0xe0` 的 anm/entry/script，
> 若真是角色帧再查 `player+0x10` 是否与其他 anm 串位；(c) 若要把大鹫激光做回来，需把
> `mode=2` row 改成 opt_slot 1..4，或补 C 侧主炮自机激光分支。

### 1v-补2. 鬼形兽（117）八修：shot 组不覆盖零售 + EAGLE 身体激光 + 动物灵 flicker（2026-09-11）

> 对应 AUDIT §AL38。上一版 1v 里「咆哮期间把主炮 0..9 槽的指针改指过去」的说明**已作废**。

- **弹幕组**：不再改 `pl0X.sht` 的 0..9 槽。新增 SDK 断点 `BP_ce_shot_set_index`
  @ `th18:0x45ebfb`（`mov edi,[edi+edx*4+0xe0]`）。卡在 `ready || hypering` 且
  `hyper_type==1/3` 时返回 `火力档 + 0x1e/0x23`；SDK 同时改 `edx` 与 `[ebp-8]`，
  让「本帧取哪组 row」和「弹里记哪组 set」一致。零售 0..9 一个字节不动 ⇒ 咆哮前出的弹
  命中时不会变自机/特效贴图。
  日志：`wba_c: shot set -> wolf/eagle (... retail 0..9 untouched)`、
  `wba_c: shot set index -> wolf/eagle base 0x1e/0x23 + pwr N = 0x…`。
- **EAGLE 身体激光**：选实现 (a) 合成 option，不用矩形。现有 `mode=2` EAGLE 数据不变；
  `on_bullet_created` 给本卡 EAGLE set 的弹写 `bullet+0xe4 = &s_body_opt`，伪 option 只填
  `+0x5c/+0x60`（`tick2`/`hit2` 都只读这两格），每帧写自机坐标 ×128。低火力 / 无 option
  也按 Z 从身体出激光；松开 Z 走引擎自己的收束。
  日志：`wba_c: eagle body laser active (... synthetic option …)` /
  `wba_c: eagle body laser off`。
- **动物灵 flicker**：补 TH17 `0x410c8c` 的 `timer_life.current == 60 → interrupt 2`；
  离开 60 px 时按剩余寿命发 `2`（<=60）或 `3`（>60），靠近冻结保留。
  日志：`wba_wc: token flicker (was near=0/1)`，每个 token 每轮变色前最多一行；
  `was near=0` 必须也出现，才能证明「没靠近过也闪」修好。
- **验证**：`native && make check` 全绿；`python3 sites.py verify` 210 条全对；
  `python3 assets/check_player_anm_bounds.py` 全过。
- **部署**：`NO_GIT=1 make release` 后，`th18.v1.00a.js` / `th18/*.sht` /
  `th18/*.anm` 与 `native/build/` 逐文件 `cmp` 相同；DLL md5
  **`1c6484c54d584336d1867320888397f6`**，`bash local/sync_modkit_dll.sh`
  四份一致（未被游戏锁）。

### 1v-补3. 鬼形兽（117）九修：pl01 EAGLE 卡侧矩形束 + shot-index 请求探针 + 正常退出 SE 试听（2026-09-11）

> 对应 AUDIT §AL39。`1v-补2` 的「EAGLE 合成 option」作废。

- **pl01 EAGLE 不再走 shooter 行**：`append_shooterset.py` 把 `pl01.sht` 的 EAGLE
  `0x23..0x27` 写成空组；`on_shot_set_index` 仍返回 `0x23 + 火力档`，所以引擎选的还是
  EAGLE 空组，不会创建「带激光贴图的移动弹」。
- **卡侧矩形束**：`animal.c` 的 `eagle_beam_tick()` 在 `roar + hyper_type==3 +
  Z（CE_INPUT_HELD & 1）` 时每帧挂/刷新 `ability.anm` **script163**（TH17
  `pl01.anm` entry0 sprite29 = 512×32 激光条），伤害走 `ce_damage_rect`
  （中心自机正前方 256、life 2、dmg 20、宽 34、高 512），消弹走 5 点
  `ce_cancel_radius` 近似矩形柱；松开 Z / 咆哮结束 / 对话暂停收掉。不依赖火力档和 option。
- **请求探针**：每次 roar ready / hyper_begin 后，前 5 次 `on_shot_set_index` 请求逐条打
  `wba_c: shot set index req: card=117 ready=… hyper=… power=… -> wolf/eagle-empty-main/unchanged`；
  30 帧内没人来问打 `wba_c: shot set index req: … none in first 30 frames (shoot_held=…)`。
  实跑请分别验 (a) 咆哮前按住 Z、(b) 松开 Z 进咆哮再按 Z。
- **正常咆哮退出 SE 试听**：debug build 每个 `!by_hyper` 结束抑制原音，30 帧一声试听
  6 个候选并打印 `wba_wc: SE probe N/6 id 0x.. (name)`；名单 = `0x34 se_lgods4` /
  `0x38 se_pldead01` / `0x3e se_tan03` / `0x43 se_release` / `0x42 se_item01` /
  `0x39 se_heal`。用户要回报「哪个 probe 号最像 TH17 的 roar-exit」。
- **门**：`make check`、`sites.py verify`、`assets/check_player_anm_bounds.py` 全绿；
  未新增 `BP_*`，`.def` 仍 24 exports。部署结果见 AUDIT §AL39 与最终会话报告。


## 1t-补. 绀珠传（115）2026-09-09 实跑修的三处

1. **碎片条贴图**：面板第 6 条原来固定用 `sprite150`——它是**绿色**（符卡碎片条），
   而卡掉残机碎片时也用它 → 「掉残机碎片却显示绿色符卡碎片条」。现按奖励类型二选一：
   `LOLK_FRAG_LIFE` → `script116`（`PANEL_7` = TH15 `sprite151` 粉 `(255,119,221)`），
   否则 → `script112`（`PANEL_6` = `sprite150` 绿 `(34,255,85)`）。像素实测见 `engine/chapter/th15/01` §4。
2. **本章擦弹数被道具冲爆**：原来 `on_item_collect` 给消弹转换点（type 9–0xe）各 +5 擦弹，
   而 TH18 的击破/消弹会把弹幕成片转成这些点 → 一章擦弹数冲到 ~1000。
   现**只认原生擦弹计数**（`th18:0x4ccd0c` 的增量），不再补道具擦弹（该钩子已摘掉）。
3. **开局空波**：原来「章号变了」就开波 → 开局先开一个空波、下一帧第一个刷出就把它结掉，
   屏幕上一上来弹一块「击破率 0%」的结算面板。现改为**只由刷出 / boss 出现开波**
   （`!wave_open && spawn_delta > 0` 也能开第一波，这样 `stage_sub < 0` 的退路也起得来）。
4. **道中切波**：同一 `MainSub` 里排了几波（st01 `MainSub00` 的三波阴阳玉）原来被并成一章。
   新增 `LOLK_MID_BURST_GAP 100`：同子程序内静默 ≥100 帧后再刷出 = 新一波
   （burst 内 `wait(10)` 不切、burst 间 `wait(120)` 切）。单测 **134 断言**全过。
   ⚠️ 用户 2026-09-09：**这块先搁置**（需要大量实机测试）——已知「三只大蝴蝶前面多切了一波」，
   阈值待定，别在没实机数据前继续调。

## 1b. 商店走两遍（2026-09-04，待实跑）

每关过关后商店开 **2 次**（`shop_core.h` 的 `CE_SHOP_VISITS_DEFAULT`），第二次商品重抽（已买的自动排除）、仍是零售流程（必须买一张）。
实现：`native/shop.c` 两个放行断点（`ce_shop_bought` `0x4183ea` 记成交；`ce_shop_reopen` `0x443b05` 在 GameThread 里把 `0x20000` 位加回 eax），
状态机 `shop_core.c`（主机单测）。引擎链：[`engine/card/th18/05-shop-and-money.md`](../../../engine/card/th18/05-shop-and-money.md) §3.5；审计 AUDIT §P。

日志应有：`shop: 2 visits per stage` → 过关 `shop: opened by msg (visit 1/2 …)` → `shop: bought (…)` → **同一帧** `shop: reopen (visit 2/2 …)`
→ 第二家店（进场动画再来一次，商品不含刚买的）→ `shop: bought` → 正常进下一关。`reopen` 行必须是 `bought + 31`，出现 `(late, gap!)` 就是 P4′ 的空档帧真的发生了。练习模式 / replay 回放里不该出现 `reopen`。
空白卡、买不起、暂停后退到标题都不该多开店（AUDIT P6 / P8）。崩溃优先怀疑：`0x443b05` 处 esi 不是 GameThread（P3）。

通过后：MAP §5 与 AUDIT §P 顶部记「实跑通过」。想改次数就改常量（以后可挂 `cards.js`）；
**关卡中间开店**（MSG opcode 36 或 DLL 直接置 `GameThread+0xb0 |= 0x20000`）技术上可行，但 Stage / Spellcard 不冻结，只该在对话里做——见 §3.5。

## 1r. 商店随机池上限 560 → 8192（2026-09-08，待实跑）

把 `pick_weighted_random_offer` 那块 560 格的栈缓冲搬进 codecave，9 处站点原地等长改写；
栈帧与金丝雀一个字节不动。原理与逐条证据见 [`AUDIT.md`](AUDIT.md) §AH。

日志应多一行（在 `cards:` 之前）：

```
pool: 9 sites verified, 8192 slots at XXXXXXXX (retail stack buffer was 560)
```

体感应当**完全没有变化** —— 这一改只挪了池子的住处，抽卡的算法一个字没动。
所以验收要点是「商店照常出货、权重感觉没变、多逛几家店不崩」。任何 `pool: site … NOT patched` 或
`FAIL: shop pool relocation` 都是回归。想压力测一把：把 `cards_dev.js` 的 `new_weight` 调到 200
（23 张 × 205 = 4715 份，远超零售的 560、仍在 8192 内），商店应当照常工作 —— 这是这次改动的直接证据。

**天花板不再是瓶颈**：零售 362 + 23 张新卡（权重 2）161 = 523，现在离 8192 还很远。

## 1q. 卡组编成崩溃（2026-09-08 已修，待复跑）

**症状**：只挂 `_test` 时进「卡组编成」看到空内容、按 Z 崩在 `0x414b7e`。根因与两道修复见 [`AUDIT.md`](AUDIT.md) §AG。
一句话：dev 的 `new_weight: 20` × 23 张新卡 = 575 份 > 商店随机池上限 560 ⇒ 卡表装载被拒 ⇒ 菜单没建起来 ⇒ 空指针。

复跑要看的：日志里**不该**再出现 `FAIL: cards:` 与 `mitigation:`；`cards_dev:` 那行的 `new_weight` 应是 15。
再把 `cards_dev.js` 的 `new_weight` 故意调回 20 跑一次，应看到

```
cards_dev: new_weight 20 would overflow the shop pool (> 560 slots) — clamped to 19
```

且游戏一切正常 —— 这条验证的是「dev 配置再也压不爆容量」。
**加新卡时留意**：池子是 `Σ(weight+5)`，`new_weight 15` 到 28 张新卡就正好 560，之后钳位会自动接手并在日志里说明。

## 1s. 倒吊人删不掉选中的主动卡（2026-09-09 已修，待复跑）

实跑：卡组 { 旋风斩, 股市 }，第一次买倒吊人删掉旋风斩，第二次买**什么也没发生**（钱照扣）。
根因与修法见 [`AUDIT.md`](AUDIT.md) §AJ —— 当前选中的主动卡原先既不进候选也不许删。

复跑：卡组里只留**一张主动卡**，买倒吊人，应看到

```
destroy: card 80 was the selected active card; reselect -> none left
destroy: card 80 (flags 0x9) removed; now total 0 = 0 active + 0 equipment + 0 passive
hanged: destroyed card 80, money … -> …
```

带**两张以上**主动卡时删掉正选中的那张，`reselect -> ok`，HUD 的主动卡图标应换成另一张、C 键随即对新卡生效。
**空卡组 / 无候选**（删到 0 张后立刻再买一张）：只应看到日志 `hanged: no other card to destroy, nothing gained`——不删、不加 300、不崩。守卫：hanged ctor `!victim` 短路；底层 `ce_pick_index(·, 0) == -1` 已主机测（test_sdk_core）。
崩溃优先怀疑：`mgr+0x38` 摘空的时点（必须早于虚表 `+0x50`，AJ4）。

## 1p. 股市（id 80，2026-09-08，待实跑）

主动卡，不开新断点。`cards_dev.js` 起手最后一格已换成 80（原 67）。实现 `native/cards/stock.c` + `stock_core.c`（主机单测 10 组）；审计 AUDIT §AF。

进关卡后日志应有：

```
sdk: 80 bound (.active_recharge = 30, .ctor = ctor, .on_activate = on_activate, .on_tick_2 = on_tick_2, .on_draw = on_draw, .on_run_reset = on_run_reset, .on_delete = on_delete)
stock: opened at 50.00 (seed XXXXXXXX)
stock: panel spawned (bg id … at 1056,862 size 432x180; candles x 848..1264 y 776..872; 2x UI space)
```

体感：**HUD 右上角**（原总分 / 最高分那块）一块近乎不透明的黑底板，**顶边与弹幕区上边框齐平**，里面一排红绿蜡烛每 4 秒多一根、最右那根每 0.25 秒变形；**现价用 ANM 数字画在 K 线图右上角**（不是文字 —— 文字压不住底板）；**买了之后**金钱数正下方两行 `2x 240` / `+18`（**股数 × 持仓总价**、本局盈亏；比金钱大一圈、黑描边，亮金 + 红涨绿跌，空仓不画）；
K 线图上还会多一条**暖黄的横线 = 买入均价**，蜡烛在线上方就是赚 —— 「该不该卖」不用读数字。
**自机头顶**两行居中，与侧栏**同一个格式**：上行 `2x 156`（股数 × 持仓总价）、下行本局盈亏；
空仓时那两个数没意义，改报一股的价钱。颜色全卡统一 **红涨绿跌**：红 = 现价高于均价（赚，该卖）、
绿 = 低于均价（亏，该买）、白 = 空仓。蜡烛与所有数字共用同一对常量，不会两边跑偏。
**居中用的是 `alignment_mode_h = 0`，这个值是推的**（2 = 右已确认、1 实测是左）—— 实跑第一眼就看得出来，
不居中就把 `CE_ASCII_ALIGN_CENTER` 改成另一个值。
分数与最高分被底板盖住是**故意的**（AUDIT §AI）—— HUD 文字永远画在 UI 层图元之前，读数与面板只能不重叠，所以读数放在面板底与残机行之间那条空隙里。
选中股市（D 键切到它）→ 场上有敌人时按 C：金钱 −50、`HOLD` 变 `0.50`、`se_cardget` 音、日志 `stock: buy 50 @ 100.00 -> hold 0.5000 (money …)`；
充能条空了约半秒回满。按住 Shift 再按 C：金钱回来（差 ≤ 1）、`se_bonus` 音、`stock: sell …`。钱 < 50 按 C / 空仓按 Shift+C → 无效音 + `card: refused (no cash / no holding)`，充能不掉。
过关进店：面板停住（行情冻结）、持仓还在；下一关接着走。局末 `stock: run reset (…)`，面板消失。
倒吊人摧毁它 → 面板立刻消失（`on_delete`）。

**要调的**（全在 `stock.c` 顶部常量）：底板 / 蜡烛位置若压到卡组第三行图标（装备行 y≈398..430）或超出窗口，改 `UI_*`；
三行字若重叠 / 太大，改 `TEXT_Y*`（HUD 字高按 Momoyo 的 `x1.5` 目测）；蜡烛矩形若以左上角为锚而不是中心，几何要整体偏 `(w/2, h/2)`（AF6）。
崩溃优先怀疑：34 个 UI VM 起不出来（日志 `FAILED to spawn`，AF4）、`ce_anm_set_scale` 被脚本插值器覆盖（脚本里没有 scaleTime，不该）。
行为怀疑：Shift+C 被当成买（AF2：`INPUT_HELD` 与 `INPUT_PRESSED` 同帧一致性）。
反过来的情形（2026-09-08 复审 I1，AUDIT AF14）：**C 被当成 Shift+C**——如果玩家开着「射击时自动低速」这个选项，
按住射击键（几乎全程）会让低速位被录制器强行 OR 成 1，于是按 C 永远走卖出分支，怎么按都买不进。
测试时如果发现「C 键一直在卖、从没买过」，先去关掉这个选项再下结论，不要以为卡坏了。
**已知的日志不精确**（2026-09-08 复审留的观察，不是 bug）：卖出那行日志里的金额永远印 `STOCK_LOT`（50），
而实际成交额在两种情况下会更小 —— 持仓不足 50 金钱等值时是全卖，以及尾数被向下取整吃掉的那部分。
看日志对账时以 `money` 那个字段的前后差为准，别信行首那个 50。

**行情模型（2026-09-08 第二版）**：隐藏参数 + 动量，取代原来的三档 regime（那版一关只赚 5 金，等于没有）。
现在**趋势会延续**，看得懂 K 线就能赚：机器人跑 300 关，趋势跟随一关中位 +589、反向 −190、完美抄底逃顶 2.18 倍。
**基准价 50.00**（一股 50 金 = 一张便宜卡的价），实际走到 19–118;波动率静息值 80，相邻步反向率约 23％。
实跑时重点看**体感**：价格是不是「一段一段地走」而不是纯抖动；影线变长之后是不是真的更颠（波动率聚集，相关系数 0.76）。
随机数改走卡自带的 LCG、只在开局向引擎 RNG 播一次种 —— 所以**带不带这张卡不该再影响同一关的弹幕**，
这条可以用同一个 replay 对拍验证。设计与平衡数据见 spec §3。


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
不改任何 exe 字节、不加断点；新写入面只有 `pl0X.sht` 尾部的两组 shooterset。审计 [`AUDIT.md`](AUDIT.md) §AJ。
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

通过后：CARDS.md 🔧 → ✅；AUDIT §AJ 顶部记「实跑通过」；MAP.md §5 那条记一行。

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

## 1s. 辉针城（id 114，2026-09-08 —— 实跑通过 2026-09-09 第九跑）

TH14 Collect Point 收点系统的 1:1 复刻（占位名 = TH14 三字名，用户定案）+ 被动二：高速射击命中掉 P（2026-09-08 实跑变更：原「掉钱」未出现 → 用户拍板改 P）。审计 [`AUDIT.md`](AUDIT.md) §Z；设计 `docs/superpowers/specs/2026-09-08-collect-2x-design.md`。
机制/数值/卡数据见 [`CARDS.md`](CARDS.md) 辉针城补充；`price_tier` 10、sprite 116/117（透明占位）都是**占位，待用户确认/供图**。

**实跑要看的事（五件事 + 一个敞口）**：

1. **四断点与事件**：启动日志 `sdk:` 要报 `ce_item_frame`/`ce_item_collect`/`ce_item_commit`/`ce_shot_damage` 已应用（少一条门就 `FAIL`）；`_test` trace 首次命中行应出现四个 `on_*` 各一次。`cards_dev.js` 起手卡组实跑时临时换一格进 114。
2. **批次结算**：开局站桩吃道具 → 收点流真停后 ~8 游玩帧（0.13 s；presence 续批 §Z21，窗口 `C2X_BATCH_SETTLE_IDLE` = 8）日志 `collect2x: settle N items, batch score … -> x1.200, +… score, frag type 0x11 x1 (t=…)`（**hex type = 新 DLL 记号**：0x10 EXTEND2 命碎卡 / 0x11 BOMB2 符卡碎卡——虹龙洞原生卡道具；旧版打 `frag type 4/6` = 别作宝石残留，见 AUDIT §Z12）；**延迟 20 帧后**应另有一行 `collect2x: frag card type 0x11 x1 spawn @(px,sy) t=…`（2026-09-08 节奏变更：结算帧只出弹字/分值，卡延迟到点才从**当下**自机位弹——TH14「先结算后弹碎片」；**两行 `t=` 差 ≈ 20**，若体感不对调 `C2X_FRAG_SPAWN_DELAY_FRAMES`）。★ 2026-09-08 第二跑根因（AUDIT §Z14）：碎片延迟曾**完全不弹** + 掉 P 三轮零出现 = 帧钟选错——`card+0x20` zTimer 只被**主动卡** tick（sdk.c `ce_sdk_active_tick` 首行 `active_recharge` 门），被动卡读它是死值。已换接线层自计数（on_item_frame 分派 = `0x446ee3` 门卫 CMP，每正常游玩帧一次）→ **若此轮 t 不涨或 spawn 仍不出，回查分派与计数**。HUD 分数应出现**负补差**（×0.5/×0.8 档分数比收集前低一点）与 **×2.0 档大跳**。碎片从自机上方 ~76px 弹、1.2 速短弧（~1.4s）后被原生 swoosh 收回，**外观应是 TH18 卡**；4:1 循环：4 次 0x11 后第 5 次 0x10，≥60 批恒 0x10。★ **批边界 = TH14 原生 timer 语义 + presence 续批（§Z18 结构定案 + §Z21 收窗；Z20 的 30 帧窗已撤）**：原生重载常量 8（一手 `[0x4f5884]` @ th14 `0x43898e`/`0x439d60`、帧尾 `0x438dad` 递减归零即结）——续期门在 item state-3 飞行 body、距离判定之前 → **在飞批集道具每帧重载 timer**，批闭 = 收点流真停后 ~8 帧（DDC 同拍）。TH18 过线只吸引（POC 线 `player+0x47998` 在、不批结算）、70px 环滴入 → 波内吸收空洞 8–25 帧常态（第六跑把「一次收点」腰斩成 ≥20+≥20 双结算、20 帧 pending 窗内二次 settle 覆盖 → 基本一次碎片）；Z20 把 idle 放大到 30（0.5 s）兜空洞 → run-7 实跑拆批不再现、但「体感慢了」（0.5 s 拖拍）→ **Z21：idle 收回原生 8 + presence 续批**：环内道具在飞（TH18 state {3,4}，flip 点 `0x445f6f`/`0x445f63` 置 3、`0x446647` 置 4）每帧续期 → 空洞被在飞散布盖掉，只有「环内无在飞且无新收 ≥8 帧」的真停段才关批。TH15/LoLK 对照 census：批记账块只写不读、timer 无消费端 → 原生没有批结算可对齐（机制变了，§Z21 证据 1–2）。结构照旧：timer idle 门、**在飞残余点不挡批**、迟到拆下一批（原生原义，DDC 连锁如此断）；不再查池空（池空门实跑三症状：钟声比 TH14 急 ~7 帧 / NO BONUS 偶缺 / 躲弹幕时突然冒过期 ×0.5——全部同根，见 §Z18）。★ **批集 = {P, 钱}（2026-09-08 §Z19：TH14 官方「红点与蓝点」→ TH18 无蓝点生态位）**：碎片/卡/转换点 9–0xe 一律不入批 —— 击破转换（boss 破招强制收集的转分点潮）**不得**触发碎片/bonus（DDC 原义，TH14 `0x43898e` 计时门 type≠9/0xa/0xb）。★ **批分值 = 批内 Σ(P/钱自带小分)**（Z19 尾段订正：P 例程 `0x446870` 尾 `0x446ac0` 与钱例程 `0x446b00` 尾 `0x446d03` 都无条件 `SCORE += 身价/10`，cap 1e9）——settle 日志 `batch score` 字段与 HUD 补差应**非零**；若站桩吃一串 P 日志里 `batch score` 显示 0，回查 stash 窗。★ **第八跑焦点（run-7 = Z20 的 30 帧窗：拆批不再现但 0.5 s 拖拍「体感慢了」→ Z21 处置，AUDIT §Z21）**：a) **钟声 `0x39` = `se_pin00`**（第五跑确认；TH14 原生真值：`0x438dfe` 系 ≥20 五档弹字前 `push 0x39 → play_sound_at_position`；th14/th15/th18 的 0x39 行同 wav 同调校 −2dB/prio 100）应在收点流真停后 ~0.13 s （≈8 帧）内响 —— **不再 0.5 s 拖拍**；比 8 帧还钝 = presence 没续上（回查 `ce_item_pool_any_attracting` 谓词与接线），太急 = 结算打断收点流（钟声响起时环里还有道具在飞）。b) **一次收点 = 一次结算**：攒波上线收点 + 持续击杀补充应并成一批（单条 settle、弹字完整、碎片一枚）；若再拆 = 连续 ≥8 帧环内无在飞且无新收的真空洞（细尾到达散布），兜底 = `C2X_BATCH_SETTLE_IDLE` 16（Z21 证据残留 6）——先报日志再动。躲弹幕期（无收点动作）**不再凭空冒结算**。音/弹字都在 settle 帧，不存在挪进 `spawn_pending_frags` 的分歧。★ **第九跑焦点（run-8 = 无全屏收点事件也误弹灰 NO BONUS + 下收点线后新掉 P 仍入批拉长 → Z22 成员门处置，AUDIT §Z22；DLL 22:56 重建 md5 `f8d9496c` 已同步 mods/）**：开批/计数/续批三处全收窄到「**真空飞行（state 3）**」—— 接线层 `on_item_collect` 成员门只认**分发时 state==3** 的 {1,2} 收集（collect2x.c:220）；presence 谓词 `ce_item_pool_any_attracting` 只扫 state 3（sdk.c:426）；70px 环收 state 4 = 普通游玩磁吸，不开批也不续批。三件事：a) **非全屏收点期 collect2x 零活动**——躲弹幕/站桩时触收、环收掉落的 P 只走原生计分：不得开批、不得 settle、不得灰 NO BONUS、钟不响（掉落节流照旧，被动不受成员门影响）。b) **三种全屏收点（放 B / 上线 / 对话自动全屏收）回归**——批照开、≥20 按档结算、钟声在流真停后 ~0.13 s、弹字/碎片/批分非零都不变（Z19–Z21 成果不得被成员门误伤；一次收点 = 一次结算）。c) **下收点线瞬间（用户语义：飞过来的 P 收完再结算）**——已在飞的残余 P 继续飞到自机、分发时 state 仍 = 3 → 并入本批、续批；**最后一枚收完 ~8 帧后 settle**（钟声在残余收完时响，不是下线瞬间）；此后线下新掉落、只坠不飞的 P（state 1 坠落 → 环收 4）不开批 —— 预期日志 = 一条 `settle` 后 collect2x 静默。残余迟到分簇（到达间隙 >8 帧）= 原生拆批语义（DDC 连锁如此断）；若迟到小批 5–19 枚会再弹一次灰 NO BONUS = **真批原义、不是 run-8 的 bug**（run-8 bug = 无任何全屏事件凭空开批）——先报 settle 行与 `t=` 序列再定性。Z21 残留 6 的「idle 16 兜底」已随成员门作废，窗口与谓词都不再动。
3. **掉 P（被动二，2026-09-08 第三跑：落点已定案，见 §Z15）**：非聚焦射击命中 → 掉 **P 火力道具 type 1**。每发成功打一行 `collect2x: P drop (power …, t=…) @(ex,ey)`；节流 = `t` 差 ≥ 间隔（火力 4.00 → 32 帧，火力越高越密）；聚焦 / bomb 命中**不掉**。★ 落点（§Z15 定案）：`0x45f0f0` 是栈帧别名帧（`push ebx; mov esp,ebx`），handler 当年 `pos = ebx+8` 读的是**参数槽**——槽里是指针（指针值当 float ≈ 0 → 屏幕外原点）。已改**解引用**：`[ebx+8]` → 指向本帧被结算敌人（身体）的 {x,y}（callee 圆形分支以它减各源中心判重叠，arg1 全扫描期恒定 = 调用方那个敌人；调用方 `0x42ee75` 压 edi=敌结构内偏移、`0x43955b` 压自家帧缓冲）。**探针已删**（静态已定案，不再追 collect2x-dbg 行）。→ **P 应从被打中的敌人身上顶出**（初速 2.2 向上小抛 = 原生 main-drop，重力带回自机行/swoosh）——请确认外观与敌人位置贴合。★ **新增观测点（命中门控未定案）**：停火几秒（弹池未空、敌人仍在场）看**敌人身上还掉不掉 P**——award 路径（`0x45f2a2`）不读任何「本调用开火过」标志，`[player+0x47984]` 是每帧额度常量（仅 3 处引用：GameThread 从 `[[player+0x47940]+0x28]` 复位、`0x40f48e` 创建矩形源时置 300、`0x45f28b` 读），静态无法排除「没打中也掉」。掉 = 需命中门：下轮在 `0x45f171` BP 捕初始预算、ebp 键控比较剩余预算做「本调用真开火」判定。P ∈ 批集 {1,2}（§Z19；9–0xe 转换点不入批、不计数）→ 掉 P 被吃在批闭 idle 窗口（8 帧 + presence 续期，§Z21）内会并入本批（timer 门）；超窗 = 拆下一批。P 满 4.00 后引擎转分路径在例程内（§Z19 尾段），不影响批分统计。帧源 = 接线层游玩帧计数（run 内单调；**不再读 card+0x20**——被动卡没那钟，见 §2 的 Z14 注）。
4. **stash 干净性**：批集收集后日志**不**应出现碎片提交混账（没有 `commit +N after batch closed` 行以外的入账噪声）。
5. **早结敞口（Z7）**：留意 `collect2x: commit +… after batch closed (lagging item)` —— 出现即「收集了、分值提交滞后到批闭之后」的道具真实存在，按 AUDIT Z7 扩判定/收尾。整关结束前把批留在打开状态 → 过关日志应无残留 settle（`on_stage_start` 清批）或有一行明确说明。
6. **倍率弹字（Task 8 原生环，Z10）**：settle 帧自机位弹两行浮字 = 数值行（结算分）+ `BONUS 1.6` 式行（**无 × 字符**——draw 格式串 `0x4b71f8` = "BONUS %.1f" 一手反汇编）；色随档（×2.0 黄 → ×0.5 蓝紫）；age>60 帧后 α 每帧 −4 淡出、原生回收槽。纯视觉核对：弹字出现前后分数/碎片/档位分毫不差（replay 双跑画面同步）；连刷 5+ 批才会见 `popup rate ring full` 日志。

**体感**：一次性吃一大片 → 分数跳一截 + 自机位弹值/BONUS 两行浮字（色随倍率）+ 碎片弹出；高速擦弹输出节奏感（P 点雨）。**崩溃优先怀疑**：collect/commit 断点处寄存器假设（Z3）、spawn 的 8 栈参 ABI（`0x446f40`）、stash 有效窗口穿越卡链表广播顺序。

**第九跑实跑通过（2026-09-09 整机回归，Z22 修复后首跑）**：无 114 五局（起手 [76,71,70]，正常过关/进店/boss 战）collect2x **零日志** —— 成员门静默侧 ✓；局中拿到 114 后掉 P 成行、三笔 settle 全 `x2.000`（263/103/82 items，`t=3837/4056/4278`），碎片 spawn 各延迟恰 20 帧（`t=3857/4076/4298`）、frag 0x10；0 条 `FAIL`/`mitigation`。用户验收：「有就起效、没有不起效」闭环。三笔 settle 期间自机在上收点线附近（py≈118–172、x≈−137 = TH18 中心原点系合法站位）—— 坐标读取正常。未复验（想测再测）：钟声/弹字体感、下收点线瞬间残余并入本批的专项观察、空卡组买倒吊人（见 §1s 倒吊人节）。

## 2. 后续开发（按优先级）

| 块 | 内容 | 接缝 / 依据 |
| --- | --- | --- |
| 其他系统卡点子 | 东方各作系统 → TH18 卡的排队交接文档：[`IDEAS.md`](IDEAS.md)（2026-09-09；底稿 `docs/superpowers/plans/2026-09-08-wishlist-feasibility.md`）。id 81–126 降序拿，从 126 起 | 用户愿望单 `系统卡.txt`；G 锚点复核见 IDEAS §1 |
| ~~皇家同花顺~~ | 🔧 已做（`cards/royal.c`，五张共用 ctor，不开断点；金钱 +800 / 命 +2 / bomb +2），待实跑 | AUDIT O26；`ce_owned()` / `ce_add_life()` / `ce_add_bomb()` 进了 sdk.h |
| ~~主动卡基类~~ | ✅ C 键 / 充能 / HUD / replay 计时器透传已进入 SDK，反转牌等主动卡已接入，待实跑 | `04-active-cards.md` §3–§6；实现与契约见 [`SDK.md`](SDK.md) §3、§9 |
| 辅助函数提炼 | 动作出现第二次就进 `sdk.h` | 方案 3 的约定 |
| 图鉴上限 | 新卡 > 71 时才需要：两处 `cmp r,imm8` → cave | AUDIT §M3 |
| ANM 特效 | 🔧 已有第一例：`ce_anm_spawn` + `assets/ability/`（反转牌 script68 亮牌），待实跑；透视真假当场看 | AUDIT O24；`engine/anm/th18/01-vm-instantiate.md` |
| 更多事件 | 按需：击破、符卡开始 / 结束、擦弹… | 每个 = 一个断点 + AUDIT 一条 |
| ~~`truesilver.c` / `ouroboros.c` 补 `on_delete`~~ | ✅ **已补**（2026-09-12 复核：两张卡现在都有 `on_delete`，体内各自 `ce_hud_digit_clear(&s->hud)`，HUD 数字 VM 在删卡路径上会消）| 2026-09-08 复审记录，2026-09-12 收口 |

## 1t. 致敬卡批次 106–113（2026-09-12「总开工」）

一次性把 8 张致敬系统卡（106 红魔乡 … 113 神灵庙）从空壳推到「卡侧代码 + 纯逻辑单测」。
**每张卡的最终口径都在 `docs/superpowers/specs/`**；本节只记状态与共享件。

### 共享件（这一轮新增/改动的 SDK 面，全部有主机单测）

| 件 | 语义 | 位置 |
| --- | --- | --- |
| `on_enemy_death` | 敌人死亡/击破共享事件（**撒主掉落之前**），`kind` 分打死/炸弹/ECL die/killAll | 断点 `0x430240`；spec `2026-09-12-shared-enemy-death-event-design.md` |
| 声明层五件 | 无敌帧（取 max，SDK 每帧落笔）/ 得分（Σ + 落笔选举）/ 消弹帧闩锁 / 子机姿态（选举覆盖）/ 掉落乘数（先加后乘） | `sdk_core.*` + `SDK.md` §6.1；`EVENTS-CONTRACT.md` |
| HUD 槽位分配器 | 左下角「底部货架」（按 card_id 升序确定性排布，认领幂等、删卡不重排） | `sdk_core.*` + `HUD-LAYOUT.md` §2 |
| `ce_spawn_enemy()` | 自建真敌人载体（`0x42d7d0`），spawn 字段表 `CE_SPAWN_*` | `sdk.h` / `engine.h` |
| 绿点共用池 | 「一套实体 + 两个记账槽」（110/111 共用，按 `slot_mask` 分账） | `cards/faith.*` / `faith_core.*` |

### 卡状态

| id | 卡 | 卡侧文件 | 状态 |
| --- | --- | --- | --- |
| 106 | 红魔乡 PlayRank | `th06rank.c` | 🔧 已实装待实跑（40188 断言；`rank→难度` 用户拍板搁置）|
| 107 | 妖妖梦 樱点/森罗结界 | `senro.c` | 🔧 已实装待实跑（12258 断言；结界特效 VM 待资产）|
| 108 | 永夜抄 妖机体/使魔 | `youkai.c` | 🔧 已实装待实跑（220 断言；发射口精确闸已接；**资产未到 ⇒ 低速打不出弹**）|
| 109 | 花映塚 爆风/蓄力 | `pofv.c` | 🔧 已实装待实跑（527 断言；CA 无敌帧数已订正）|
| 110 | 风神录 信仰/灵击 | `mof.c` + `faith.c` | 🔧 已实装待实跑（868 断言；按 TH10 一手订正：无擦弹倍率 / 无逐帧衰减 / 连击 +120）|
| 111 | 地灵殿 信号槽 | `signal.c` | 🔧 已实装待实跑（233 断言；绿点来源已补、每关归零已补、乘数通道已接）|
| 112 | 星莲船 飞碟 | `ufo.c` | 🔧 已实装待实跑（925 断言；金=蓝、两同一异照抄、**HUD 第一版在层 25**；**真 ECL 已从 TH12 移植**，缺贴图/anm ⇒ 暂不发布）|
| 113 | 神灵庙 灵界 | `trance.c` | 🔧 已实装待实跑（**1103** 断言；**中弹救援 = 救援层最后一家（延时不是免死；落点 = 卡虚表 `+0x0c` + `return 1`）**；**神灵实体池已落地**：不下落/走过去才吃/越缩越小；计分载体仍搁置）|

### 本轮（2026-09-12 第三轮：对抗审计 + 收口）

八张卡交付后，用 **5 个独立审计/研究 agent** 做了「代码 vs spec」与「删卡收尾」的证伪，抓出的都是**让功能失效**级的：

| 类别 | 抓到的 | 处置 |
| --- | --- | --- |
| 卡侧数值/判据错 | 106 的 `T06_INC_GRAZE` 小 **100 倍**、`T06_DEC_OFFSCREEN` 小 10 倍、蓝点分档**读错对象**（道具 y vs 自机 y）；110 的「全避」判据错（每张正常收掉的符卡都 −30000） | 已修（`th06rank_core.h` 加 14 条量纲静态断言；`mof_core_spell_watch()` + 6 组单测）|
| **整张卡是死的** | 108 的发射口偏移**整体偏 `0x122c`**（加在裸 `zEnemy*` 上，实际是 `zEnemyData` 相对）⇒ 槽枚举恒 0、使魔建不出来；111 的大/中绿点**没有任何来源**（`signal_on_pickup` 是死回调）；112 的令牌颜色恒 0 + 轮转表顺序使红/绿奖励永不触发 | 已修（108→`0x17c4/0x6084/0x6144`；111 补 `on_enemy_drop_pre`+`on_item_collect`；112 改枚举轮转表）|
| **自己写引擎逻辑写错** | 112 的 `ufo_item_relink` 把一手基址 `*(head+4)` 写成 `*(item+4)`（自赋值 = 空操作 = 链表损坏）| 已修（逐寄存器对照写进注释）|
| 删卡收尾 | 70 腐化的 `MAX_BOMBS` 跨关不还原；58–62 皇家同花顺的文件级 static 演出倒计时；110×111 **共用绿点池连坐**；108 子机释放漏 `ptr` guard；107 HUD 坐标用认领时的缓存 | 全部已修（详见 `AUDIT.md` §AX）|
| SDK 面 | `on_item_score` **只覆盖一半分支**（线下收的金钱绕过）⇒ 加第二站点；`ce_hud_slots_reset()` 事实上从不按关调用 + `reserved` 死路径 ⇒ 加**关卡纪元**（`ce_hud_slot_claim_card()`）；`BP_ce_enemy_death` 漏导出、`sites.py` 按下标取址 | 全部已修（`AUDIT.md` §AZ / §AV / §AM-补）|

**本轮新增的 SDK 面**：
- `on_shot_damage_any`（`0x45f2a2` 同一断点、**不按聚焦过滤**）—— 107 的低速命中通道（§AV）；
- **`on_enemy_shot_slot` + 断点 `0x433ffa`** —— 敌人 ECL `etOn` 的「发弹三步」闸：返回非 0 ⇒ 本帧这一槽不发射（108 的「使魔死了 ⇒ 关掉发射口」，§AY）；
- **乘数附加通道** `ce_score_mul_declare/aggregate` —— 110×111 同装时把 111 的信号强度交给落笔者（§3.1）；
- **`ce_item_score_below` 第二站点**（`0x446caa`）—— 线下收的金钱也走 `on_item_score`（§AZ）；
- **关卡纪元** `ce_hud_stage_begin()` / `ce_hud_slot_claim_card()` —— 「下关开场重排」真正落地（`HUD-LAYOUT.md` §4.4）。

### 门（`make check` exit 0 实测）

- 主机单测 **27 组、58631 条断言**全绿（含 106 的 40188、107 的 12258、113 的 950、110 的 868 —— 大数主要来自不变式循环）；
- `make dllx87`：我们的 object 里 **x87 = 0**（新增的 8 张卡各自 0 条，靠「core 整数化 + `static inline float`」维持）；
- `make dllverify`：**27 exports**（新增 `BP_ce_enemy_death` / `BP_ce_enemy_shot_slot` / `BP_ce_item_score_below`，24 → 27）、PE32/i386、依赖不变；
- `python3 sites.py check`：25 个实例两条不变式成立 + **断点 ↔ `.def` 导出双向对账 26/26**；
- `python3 sites.py verify`：210 条 binhack 与 exe 字节全对上；
- `python3 tooling/check-docs.py`：206 个文件全绿。

### 这一轮踩到并上了门的坑（都写进 AUDIT）

1. `sites.py` 生成 `CE_BP_*_RVA` 原来用**下标** ⇒ 插一条断点全体错位（`CE_BP_SHOT_SET_INDEX_RVA` 变成死亡断点地址）。
   已改成**按名字**取址。
2. patch 声明了断点但 `th18_card_expand.def` 忘导出 `BP_*` ⇒ 游戏里**静默不生效**。
   已补导出（24 → 25），并把「断点 ↔ 导出」双向对账做进 `sites.py check`（`make check` 会跑）。
3. 掉落乘法层原来收的是 `counts`（= 基址 +0x04）而不是 `zEnemyDrop` 基址 ⇒ 整体错开一个 type 格；
   且漏了 `+0x54 timed_ex_drop[20]` 那张表（引擎撒 `ex[i] + timed[i]×(T−timer)/T`）。两处都修了。
4. i686 上的 **x87 三来源**（`float` 返回值走 `st(0)`、`float/double ↔ int` 转换、变参 `printf` 的浮点）
   ⇒ `make dllx87` 门变红。已写进 `SDK.md` §6.1 与 `CARD-CHECKLIST.md` §0.4（core 一律整数化）。
5. **`ce_state` 上限 496 字节**（`512 − 16` 保留），超了 `ce_state_user()` **静默返回 NULL**
   ⇒ 结构体一律加 `_Static_assert(sizeof(T) <= 496)`。113 踩过（队列按 64 格存位置 = 600 字节）。
6. **「自己写引擎逻辑」必须逐寄存器对照**：112 的 `ufo_item_relink` 一开始把一手基址 `*(head+4)`
   写成 `*(item+4)`（自赋值 = 空操作 = 链表损坏），三轮对抗审查才抓到 —— 照抄引擎片段时**寄存器也要照抄**。

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

## 9z. ★ 待用户拍板（2026-09-12 本轮实装后新增；每条都有一手依据，答复后即可落地）

| # | 事项 | 现状 / 一手依据 | 影响 |
| --- | --- | --- | --- |
| 1 | **107 结界「持续期」要不要补视觉** | ★ 已订正：原版持续期**有**视觉，只是不是环 —— ①**全屏 sprite 变暗到 37.5 %**（`th07:0x44162a`，绘制侧 `0x406b52`）②**自机红白每 2 帧闪**（`th07:0x441553`），两条覆盖整段 540 帧；环只在发动瞬间 ~33 帧 | 本轮两者**都没搬**；另零售数据自相矛盾：发动时给环设的缩放插值时间基是 **540 帧**（`th07:0x441b56`）而脚本 33 帧就收槽；详见 `engine/spirit/th07/01-barrier-visual.md` |
| 2 | ~~108 低速子机「数量」+ 子口径~~ **✅ 全部已定并落地**：数量 = 各机体在各自作品的数（灵梦→紫 1 / 魔理沙→爱丽丝 1 / 咲夜→蕾米 4 / 早苗→文 `floor(P/100)`）；用户追加「**0 子机得不压原生弹**」也已实现 | ✅ 一手：`th08:0x4c7d40[shottype][4]` 非 0 项数 / `th16:0x444106 idiv`；口径见 `AUDIT` §BO-8 / §BO-10 | 实跑验收未做（早苗 P<100 低速应看到 `NOT suppressing native shots` + 原生弹）|
| 3 | **108 妖侧本体换图走哪条路** | spec §11.3 给了四条候选 + 六条待决。已查明：(a) 注入 `pl0X.anm` 撞「一个字节都不改」铁律，且**一套 24 格要 33120px，四张 256×256 贴图页都放不下**；(b) 叠跟随 VM + 藏本体，但**藏法全无一手指实**（藏不干净＝双人叠影）；(c) 维持现状；(d) hook 跳过本体绘制（最贵） | 108 的「整个人换人」能不能落实；spec §11.3 |
| 4 | **112 彩虹令牌收进去算哪一色** | 一手：TH12 里彩虹令牌按**当时显示的那一色**入账（`0x426c3f/0x426c4d/0x426c5e` = case 10/11/12 与 13/14/15 分别记 红/蓝/绿），**不是万能色**；本卡当独立一色 | 要改成一手语义得动 `ufo_core`；TUNING A8 |
| 5 | **HUD 的 layer 24 → 25** | 只为「P 键截图里能看到」；**机制未坐实**（`0x3b` 合成四边形的覆盖面拿到反证 ⇒ 🟡），且用户当前不能开游戏做判决实验 | 13 个脚本（全是我们自己的卡）批量改一次；AUDIT §BD.1 |
| 6 | **SDK 的 near-corner 门限要不要参数化** | SDK 用 **TH16** 的 400/384（`sdk_core.c:508`），而 TH10 一手是 416/400 + x −128/−112、TH11 是 416/400 + x −64 | 110/111 已按各自原作自算（共享件没动） |
| 7 | **111 的三行数字读数** | 零售 SA **没有**数字读数 ⇒ 那是我们加的（已按老口径标 ❓自定）；块因此 208×56，会把 116/117 往右推（算过仍在同一行） | 要不要保留 / 精简 |
| 9 | **108 子机脚本的中断标签口径** | TH18 零售本体子机是 **2 出场 / 3 进店收起 / 1 销毁**，而 TH08 那三支是 TH08 约定（1/2 循环、3/4 起爆、5 散场）。引擎清槽发的是**标签 1**（`th18:0x45d310`/`0x45d2d0`/`0x45dbf0`；零售装备卡同），而 **2/3 的发送点本轮没找到** | 现照抄零售 ⇒ 收到 1 只是「切到另一段循环」（无害，随后 SDK 删 VM）；若引擎还会发 3，紫那支会切到 46×46 的「隙间」形态并 `stop()` ⇒ 收起动画丢；AUDIT §BO-9 |
| 10 | **108 要不要接 TH16 满火力另一形态** | `th16:0x492c10 = {9,8,12,13}`（文 = `pl03` **script 12**，8 帧同图 + `blendMode(1)` + `type(3)` + 不同帧序）| 要接得**按火力档切脚本**（会触发「先收再要」的重申请）；一手数据已备齐，补一支 228 即可 |
| 8 | **110 要不要进货架** | 本轮把坐标从 ❓ 自定换成**零售原值**（落在 TH10 的 (32..91,448..464)，**在货架左锚点 257 之左**，与原版「那块本来就空着」一致） | `HUD-LAYOUT` §5 的迁移清单 |

### 9z.1 还没做、但**不需要**用户决策的（下一轮可直接做）

- ~~**108 使魔可见性**~~ **✅ 已落地**（2026-09-12）：四色零售切图 + 跟随 VM（`yk_fam_vm_spawn/follow/kill_all`，层 7），
  删卡路收得干净；句柄表在文件级静态区（不占 496 B 预算）。
- **`youkai.c` 的本体 sprite 写法要改**：一手证明 `vm+0x24` 直写**不够**（UV/尺寸缓存由 `0x477b00` 填）
  ⇒ 正确形状是 `ce_anm_set_sprite(*(void**)(player+CE_PLAYER_ANM), player+CE_PLAYER_VM, N)`，先例 `season.c:995-1003`。
- **`engine/anm/th07/` 待补**：TH07 `AnmManager::loadAnm` `0x44df90` 的 base「每个 entry 前进上一 entry 的 sprite 数」
  （`0x44e05b`，三处交叉印证）—— 现在只写在 `make_senro_art.py` 的注释里。
- **`185_faith_dot.anm.txt` 的注释要订正**：`ins_63` = `stop`（不是 `delete`）、`ins_53` = `angleVel`（不是 `rotate`）、
  `ins_66`/`ins_67` 可搬；订正块已写在 `make_mof_art.py` 文件头，脚本本体未动。
- **`tooling/thtk/unpack.py` 的 `GAMES` 加 th11**（本轮 111 的资产是手工解的；`local/` 已就位）。
