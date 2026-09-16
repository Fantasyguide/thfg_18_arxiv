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
3. **波次切分不准** —— 绀珠传（115）的「一段攻击 = 一波」曾对不上（道中同 `MainSub` 多波）。
   **2026-09-13 已按用户洞察改写**：章 = **MainSub 编号（去掉尾部字母，`01`+`01b` 同章）**，
   删掉旧的 `LOLK_MID_BURST_GAP` 静默窗口；boss 非符/符卡边界不变。**待实跑**（判据见 §1t-补2）。

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

> ★ **2026-09-15 追加一条实跑必看**：110 的**逐帧信仰衰减已装回**（2026-09-12 那次
>   「TH10 没有这个机制」是**绝对地址 grep** 造成的误判 —— 真身全在 `eax` 相对寻址里）。
>   新日志行：`mof/faith: gate=… boss=… sf=… near=… rate=… drain=…`（每 600 帧）。
>   要看三件：① `sf` 每次**换关后从 0 重新爬**；② 进关前 ~90 帧 `gate` 第 3 位 = 0 且 `drain=0`；
>   ③ `drain` 换算成 /s 与 `rate` 栏对账。门/速率逐项与**「有敌人 −600/s」和一手冲突的那条 🟡**
>   见 [`TUNING.md`](TUNING.md) **§2.5h**；一手逐指令见 `engine/faith/th10/01-faith-and-points.md` §3.2b。

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

**灵界版 BGM（113，2026-09-13 第二轮 —— 真素材）**：素材是**本机派生品**（TH18 各面曲子 + naive 低音放大），
**不在发布包里**，要手动拷一次（每次重新生成或换机器都要做）：

```bash
python3 tools/make_trance_bgm.py   # 默认 14 首（7 道中 + 7 关底，168.5 MiB）→ local/th18.v1.00a/trance_bgm/
bash local/sync_modkit_dll.sh      # ★ 一次同步到三处：mods/bgm、thcrap/bin/bgm、game/bgm
```

★ **三处都要放**（2026-09-14 首日实跑教训，`AUDIT` §CO-9）：真正被加载的是
`<modkit>/thcrap/bin/mod_th18_card_expand.dll`（启动器开始游戏那一刻从 `mods/` 复制的那一份），
只放 `mods/bgm` 会让日志一直报 `trance-bgm: 素材不在`。素材范围也已成 **14 首**（用户口径
「BGM 得包括 7 首道中和 7 首关底」；关底战期间引擎会切到 boss 曲）。

进灵界听：**低音明显变厚**、0.33 s 短淡入、出灵界回到原曲（音量不跳）；日志一行
`trance-bgm: OK 第二路起播：素材 "th18_03b.wav" …`。**boss 战期间进灵界不换 BGM 是预期的**
（只生成了道中曲；要 boss 曲 ⇒ `--set stage+boss`）。素材没拷/路径不对 ⇒ 日志打出**两条候选路径**、
BGM **照常放**（这是设计，不是 bug）。判据、回退、二分口径见 [`AUDIT.md`](AUDIT.md) §CO-4/§CO-7。

通过后：MAP 第 7/9/10 段 🔧 → ✅；AUDIT §N / §O 顶部各记一行「实跑通过」；[`CARDS.md`](CARDS.md) 状态列改 ✅。

## 1t. 绀珠传（id 115，2026-09-09，待实跑）—— 章节系统 + 擦弹减速

被动卡，**不加断点、不改字节、不改 ECL**：只读引擎现成字段、写一个 float。实现 `native/cards/lolk.c` +
`lolk_core.c`（主机单测 **136 组**）；设计 `docs/superpowers/specs/2026-09-09-lolk-chapter-design.md`；
一手 `engine/chapter/th15/01`、`engine/player/th15/01`、`engine/chapter/th18/01`、
**`engine/chapter/th18/02`（波次 / 相位信号）**、**`engine/chapter/th18/03`（道中章 = MainSub 编号，2026-09-13 改写）**、
`engine/player/th18/02`。

**用户定案**：
① 波次 = **一段攻击 = 一波**（非符一波、符卡一波），**进关底 / 道中 boss 也算一波**；
   道中章 = **MainSub 编号（去掉尾部字母，`01`+`01b`+`01c` 同章）**（2026-09-13 用户洞察，替换 2026-09-09 的「按 ECL 524 切章」/ 静默窗口）；
② 减速 = **擦弹判定框里有敌方子弹就一直慢**（不是「擦弹那一刻」）；
③ 击破率三来源：道中 = 击破数/刷出总数（分母 0 → 100％）；boss 非符 / 符卡 = 击破 100％、超时 0％；
   **时符（耐久符卡）恒 100％**。

启动应有 `sdk: 115 bound (.on_tick_2 = …, .on_draw = …, .on_stage_start = …, .on_run_reset = …)`。

进关：

```
lolk: wave 1 start (mid nonspell, chapter 0, sub 7, mainsub 0 [rule mainsub], sig 0, graze/slow 100%)
lolk: wave 1 (mid nonspell) settled: graze N, killed K/S = R%, bonus B, score +B/10, frag 0x11
lolk: wave 2 start (boss nonspell, chapter 41, sub 5, mainsub -1 [rule boss], sig 131073, graze/slow 100%)
lolk: wave 2 (boss nonspell) settled: …
```

`mainsub` = 接线层解析出的 MainSub 编号（-1 = 当前不在 MainSub 段）；`rule` = 这一波用的边界规则
（`mainsub` = 主信号；`quiet-fallback` = 认不出关卡脚本的退路；`boss` = boss 相位）。

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
- 结算从不出现 → 波边界信号（boss：`ENEMY_MGR+0x48` boss_ids + `zEnemy+0x6374` 中断槽；
  道中：关卡脚本主上下文 MainSub 编号 —— 日志里的 `mainsub K [rule …]`；退路才用静默间隔）。
  道中 `mainsub -1 [rule none]` 说明接线层没认出 MainSub（查 `zEnemy+0xc`/`+0x1218` 读法）；
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
  **敌人掉落**（`on_enemy_drop_pre`）、**解放消弹**（`ce_cancel_radius` 返回值**每 1 颗 1 点**
  —— 2026-09-15 按 TH16 一手改成复刻比例，原值 8 是 ❓ 自定，已撤销）；
  季节点是**本卡自己的 anm VM**（`ability.anm` script 115 + **TH16 原版切图** `bullet/item.png`
  右上 2×2 = 春/夏/秋/冬 + `(224,32)` = 土用，各 16×16，与 P 点/蓝点同尺寸，按副季节换 sprite；
  出场动画照 TH16 script129：加算 / 10 帧淡入 / 16 帧 2 倍→1 倍 / 随机自转 / 之后 300 帧缩到 0.7 淡到 alpha128），
  落体→靠近被吸→16 px 内收；**不再借 TH18 的分数转换点**；
- ② 按 TH16 阈值 **100/230/390/590/840/1140** 分 0..6 档，
  满槽不收、升档才响（照 `item_collect_season` `th16:0x43de50`）；
- ③ **季节子机**：档位 = 子机数（最多 3 颗），**阵位照 TH16 副 sht 的 option_pos**（每档一套独立阵型：
  秋 ±48 最宽、冬竖线、其余小扇），写 option 记录的 `+0x64/+0x68/+0x6c/+0x70`（原生卡 `th18:0x40b8c1` 同款写法），
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
2. ✅ **左下角样式**：改成 TH16 原版三件套 —— **季节字**（原版 `front00.png` sprite82+SUBSEASON，
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
   收集半径 / 吸引半径 / 磁速**直接读 `player+0x47988..94`** ⇒ 原生卡 ITEM_CATCH(21) / ITEM_LINE(22)
   改参数时季节点跟着变。收集 +10 分（TH16 `case 0x10` 尾 `add_to_score(10)`）。
4. ✅ **掉落量对齐 TH16 + boss 一大把** —— 一颗点 = **1** 点（原来 1 颗折 8 点）；小怪掉 **8** 颗、
   boss/道中 boss 掉 **200** 颗（判据 = EnemyManager 的 boss id 表 `em+0x48`）。池放到 **DLL 静态区 96 槽**
   （`ce_state` 只有 496 B 装不下），池满进队列、每帧撒 8 颗（boss 那 200 个 ≈ 0.4 s 撒完）。
5. ✅ **过回收线会被吸** —— 每帧**读** `player+0x47998`（不写死 128）⇒ 原生卡 ITEM_LINE(22, Kanako)
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
   现在照原生 `stock_rect` / `front.anm script20` 的写法补齐 `sprite(-1) + zWriteDisable(1) +
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
4. **`drawRectBorder`（opcode 612）在 TH18 不可用** —— 原生 anm 零使用；季节槽外框已改用
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

**2026-09-10 实跑第二十八轮（★ 照原生「ミニ八卦炉」把冬的激光做出来）**：
- 用户点名：魔理沙的能力卡 **迷你八卦炉** 发的就是激光。查 `dat/ability.txt`：`@MARISA_OP` = ミニ八卦炉
  = 卡 **10** = `pl0X.sht` **set 0xb**（四个角色逐字节相同）：
  `rate=5 delay=0 dmg=13 ang=−π/2 **speed=0** opt=0x00 **mode=2** **anm=3** sfx=−1 **hit_fx(+0x26)=5**
   func=**init2/tick2/—/hit2**`。
- `ability.anm` 里 **script3 = 激光束**（`scriptNew(script4)` + `layer(15)` + `sprite(sprite8 = op00.png
  (0,16) 512×16)` + `anchor(1,0)` + `scale(1,0)→scaleTime(4,1,1,0.5)` + `blendMode(1)` + `rotateAuto`
  + `scrollX(−1/21)`），**script5 = 命中特效**（由 hit2 每 2 帧在束端起）。`script4` = 束根光斑。
  ⇒ **激光不是「弹」，是挂在子机上每帧重画的束**；`fire_rate` 只是重新起束的节奏。
- ★ 结论修正：上次「激光损坏内存」**不是这条路的锅**，是我们没照原生填参数（当时 `mode=0`、没有 `hit_fx`、
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
   但**原生脚本还在**：`pl0X.anm` 的 `sprite(sprite0)` + `color(60,0,192)` + `alphaTime(20,0,0)`，
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
1. **字形用原生切图**（铁律）：`assets/ability/make_font2_art.py` 从
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
     看着就像「boss 又掉了一次」。⇒ `SEASON_POINT_POOL` **96 → 512**（TH16 原版道具池是 4696 槽，
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

**2026-09-10 实跑第三十五轮（用户：「th16 有不同档位吗？」→ 选「原版 T + 原版计数映射到 TH16 量级」）**：

1. **TH16 的档位不是表，是逐敌人手写的 `dropSeason(T,N,M)`**（普查见 `engine/season/th16/01` §5.3）：
   小妖精 `(60,8,1)`、妖精 `(60,10,1)`、大妖精 `(120,13,1)`/`(120,20,1)`、大只 `(200,30,1)`、
   st06 巨物 `(600,100,1)`、st07 `(60,40,1)`、boss `(1200,200,10)`、终盘 `(2400,400,400)`（不衰减）。
2. **★★ 顺带挖到：TH18 自己有一套同形的逐敌人档位，而且是活的**（一手，`engine/season/th18/01` §1b′）：
   - ECL **opcode 574** → `0x433a70` → `FUN_0042d040(drop, T)`：写 `zEnemyDrop.timed_ex_drop_time(+0xa4)` + 复位计时器；
   - ECL **opcode 573** → `0x433a4a` → `FUN_0042d020(drop, type, count)`：写 `timed_ex_drop[type](+0x54…)`；
   - 结算（`0x430510` 一手）：`n = ex_drop[i] + timed_ex_drop[i] × (T − timer)/T`；
   - 计时器（`FUN_0042ff80` `0x430161` 段**内联**，不是 `Timer::increment` 调用）每帧 +1。
   ⇒ **和 TH16 的 `dropSeason` 同形同向**（出场 T 帧内给满、之后衰减到 `ex_drop`）。
   原生 22 个 `*.ecl` 全扫出 **64 处 `574`**：小怪 T=40..120、大只 200..400、道中 boss 600/900，
   计数 1..20（`*bs` 里一处都没有 ⇒ boss 不走这条）。
   ⚠️ 顺手订正本仓旧结论：`engine/season/th18/01` §1b 早前写成「随时间线性补足」——**方向写反了**，
   已改（`iVar4 − iVar4×timer/T` 是递减）。
3. **卡侧改法**：小怪掉落的 `T` 直接读敌人自己的 `timed_ex_drop_time`（`enemy+0x623c+0xa4`），
   数量用「原版 timed 计数」当档位键映射回 TH16 量级（`season_core.c` 的 `season_drop_native_tier`：
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
资源 `assets/ability/make_animal_art.py`（原生 `beast1.png` 13 单元 + `beastf.png` 槽框 + 脚本 137–145）；
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

体感：杀敌掉出会**弹跳**的动物灵 / 鱼灵（贴图 = TH17 原版切图）→ 飞过去收 → 左下角 5 格灵槽逐格亮 →
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
6. **音效 id 对照 / 撒道具 A/B**：⚠ **2026-09-12 最新口径见 §1v-补5（AUDIT §AL41）**：
   - 正常咆哮退出 = **`0x42`**（用户听完 6 候选回报第 5 声 + 一手 TH17 `th17:0x40f4fc`
     `push 0x42`）；旧 `0x38` 是早期错误读数，作废。
   - 撒道具 release 默认仍 `0x34`（名表 `se_lgods4`）；debug build 下一轮 A/B `0x34` / `0x43`
     （名表 `se_release`），等用户耳朵裁决。
   - 用户解包 `th17/data/` 的 wav 文件名可能与 exe 内建 name table 不一致
     ⇒ **创造性音效选择以用户耳朵实听的 id 为准**，表名只随日志显示。
   - 早期 `+0x04` / `+0x00` 的 SE 行解码线索见 AUDIT §AL26 / §AL29 / §AL35，但不再直接决定这里的 id。

**X 炸弹拦截（2026-09-11）**：咆哮 `ready || hypering` 时，SDK 新增断点 `BP_ce_bomb_gate` @ `th18:0x45c048`
（`Player__on_tick__body` 普通炸弹分支的 `test BYTE PTR ds:0x4ca434,0x2`）询问本卡；本卡返回 1 时
SDK 把执行从 `0x45c048` 改到 `0x45c056`，**跳过 `0x45c051` 的 `call do_bomb`**。不咆哮时返回 0，
真实炸弹不受影响。首次拦截打印一次 `wba_wc: bomb suppressed while roaring`；实跑请确认「咆哮中 X 只出霊撃、不扣 B」。

**咆哮期间的自机特效（2026-09-10 Stage 3 已接）**：

- **类型光环**：原版 `beastAURA.png` 的 24 个 64×64 单元 + 脚本 146–148（照 TH17 script41–43 生成），
  layer 13 跟随自机；红狼 / 绿水獭 / 紫鹫。日志无关键字，看画面；
- **水獭 3 消弹体（2026-09-11 初相/转轴修正；同日 §AL34 修 ready 消弹）**：原生
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
| `pl00` 霊夢 | 7 发大弹扇形 dmg 20–40 / speed 25 / **anm 10**（原生兽灵 `script10`，sprite33）| 五向散 dmg 20 / speed 10 / **anm 11**（原生 `script11`，sprite38/39/40/41）| 直接用 TH18 原生脚本 ✅ |
| `pl01` 魔理沙 | 5 条激光 dmg 8–18 / speed 0 + `tick3` / **anm 13**（原生 `script13`，sprite42/43 + 命中 `script14` 爆炸）| 5 发重弹 dmg 50–80 / `mode=2` 子机槽 16..64 / **anm 16**（原生 `script16`，sprite29 激光条）/ `hit_fx=8` | 直接用 TH18 原生脚本 ✅ |
| `pl02` 咲夜 | **安全回退**：anm 5（原生普通自机弹；不崩、不来自机图）| **安全回退**：anm 6（原生普通自机弹）| identity 回退 ⚠️ 兽灵视觉未达成 |
| `pl03` 早苗 | **安全回退**：anm 8（原生普通自机弹）| **安全回退**：anm 11（原生普通自机弹）/ `hit_fx=7` | identity 回退 ⚠️ 兽灵视觉未达成 |
> ★ 2026-09-11 四修（最终）：TH17 `PlayerBullet__fire`（`th17:0x44a180`）实际读
> **`pl0X.anm script(anm+5)`**（一手 `th17:0x44a535`），不是 `bullet.anm`。而且
> **TH18 原生 `pl00.anm`/`pl01.anm` 里逐字节就有这些 TH17 兽灵脚本**（pl00 8/10/11、
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
> AL27–AL31 的「TH18 原生 pl0X.anm 没有 TH17 兽灵脚本/贴图，必须追加」**前提是错的**。
> 一手逐字节对拍：TH18 原生 `pl00.anm`/`pl01.anm` 的 entry0 与 TH17 同名自机一致，且
> **兽灵脚本 pl00 8/10/11、pl01 8/13/16 与 TH17 逐字节相同**。所以正确做法是
> **一个字节都不追加，sht 直接指原生 `script(anm+5)`**（`HYPER_ANM_MAP`：pl00/pl01 `5→10 6→11
> 7→8 8→13 11→16`）；`build_player_anm.py` 改成「拷原生 + 硬校验（sprite 越界、脚本存在、
> `scriptNew` 目标、会飞脚本主路径无 `delete`、与 TH17 逐字节一致）」。上一轮的伤害是「为满足
> 『脚本不含 scriptNew/jmp』把 TH17 脚本抄简」：pl01 鹫 `script51` 丢了 `scriptNew(script7)` +
> 全部 `interruptLabel`（真 `script16` 是 mode=2 激光），pl01 狼 `script50` 丢了
> `interruptLabel(1)→scriptNewPos(script14)`（爆炸）——这才对应「闪退 / 无爆炸 / 命中原图」。
> **pl02 咲夜 / pl03 早苗**没有 TH17 对应自机，本轮**安全回退 identity**（`anm` 指各自原版普通
> 自机弹，不崩、不来自机图），**兽灵专属视觉未达成**。见 AUDIT §AL33；DLL 未改，
> md5 `06c88611892f9e104d2b6483e16ea0b3`。

> ★ **2026-09-11 六修（安全优先：魔理沙 pl01 兽灵组整组回退）**：
> 用户实跑仍报「魔理沙大鹫灵闪退 / 狼灵爆炸出魔理沙」。已定位大鹫闪退不是 `hit_fx=8` 或
> `script16`，而是 TH17 EAGLE row0 的 `mode=2 + opt_slot=0` 被 TH18 主炮 `tick2`
> 无条件解引用 NULL option（`th18:0x45fcc8`）。**先保命**：`pl01.sht` 的追加槽 0x1e..0x27
> 不再使用 TH17 wolf/eagle row，改整组拷 TH18 原生聚焦主炮 set 5..9；`pl00` 不变。
> 魔理沙暂时没有兽灵专属弹幕，但不会崩、不会来自机图。狼灵爆炸 sprite 证据（script13/14 的
> `sprite42..51` 不是角色帧，TH17/TH18 字节一致；`hit6→hit4` 也正确对应 interrupt）见 AUDIT §AL36。
> 下一步实跑：(a) 确认 pl01 咆哮不再闪退；(b) 命中处记录 `vm+0xe0` 的 anm/entry/script，
> 若真是角色帧再查 `player+0x10` 是否与其他 anm 串位；(c) 若要把大鹫激光做回来，需把
> `mode=2` row 改成 opt_slot 1..4，或补 C 侧主炮自机激光分支。

### 1v-补2. 鬼形兽（117）八修：shot 组不覆盖原生 + EAGLE 身体激光 + 动物灵 flicker（2026-09-11）

> 对应 AUDIT §AL38。上一版 1v 里「咆哮期间把主炮 0..9 槽的指针改指过去」的说明**已作废**。

- **弹幕组**：不再改 `pl0X.sht` 的 0..9 槽。新增 SDK 断点 `BP_ce_shot_set_index`
  @ `th18:0x45ebfb`（`mov edi,[edi+edx*4+0xe0]`）。卡在 `ready || hypering` 且
  `hyper_type==1/3` 时返回 `火力档 + 0x1e/0x23`；SDK 同时改 `edx` 与 `[ebp-8]`，
  让「本帧取哪组 row」和「弹里记哪组 set」一致。原生 0..9 一个字节不动 ⇒ 咆哮前出的弹
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


### 1v-补4. 鬼形兽（117）十修：鹫束 = 每颗原生子机一条 + SE 试听 3 s 间距（2026-09-12）

> 对应 AUDIT §AL40。`1v-补3` 的「单条自机体束」按用户 2026-09-12 实跑反馈作废。

- **Item 1（普通子机）**：用户原话「魔理沙的应该是普通子机激光，不是从（灵梦）机体本体射出」
  ⇒ `eagle_beam_tick()` 不再只挂一条自机体束，而是遍历引擎**原生子机数组** `inner+0x50`
  （= `player+0x670`，4 格、stride `0xf0`、`option+0x00 != 0` 判活），每个子机位置
  （`option+0x5c/+0x60` 定点 ×1/128）各挂一条 `ability.anm` **script163**，各自带
  `ce_damage_rect` + 5 点 `ce_cancel_radius`；没有子机（火力 <1.00）才回退**一条本体束**。
  松 Z / 咆哮结束 / 对话暂停全收。
- **数据侧不动**：`pl01.sht` 的 EAGLE `0x23..0x27` 仍是合法空组（只有 `ff ff ff ff`），
  主炮照旧不建 sprite-bullet。
- **Item 2（SE 试听）**：6 候选间距 **30 → 180 帧（3 s）**；候选顺序 / 抑制正常退出 SE /
  每条一行日志都不动。用户暂定 `0x39 se_heal`，**未写死**，等 3 s 版实跑回报。
- **实跑要 grep**：
  `wba_c: eagle beam mode = option (n=N)`（有子机；N=1..4）/
  `wba_c: eagle beam mode = body (no options)`（无子机兜底）/
  `wba_c: eagle beam on (N beams, len 512, width 34, dmg 20/frame cap 300)` /
  `wba_c: eagle beam off (N beams, len 512, width 34, dmg 20/frame cap 300, fade=…)`；
  `wba_wc: SE probe active (6 candidates, 180 frames apart); normal roar-end SE 0x38 suppressed`
  + `wba_wc: SE probe N/6 id 0x.. (name)`。
- **要用户检查**：① 灵梦 / 魔理沙在不同火力档：束应从**每颗子机位置**出来（0 子机时从本体一条）、
  数量随子机数；② 松开 Z 束立即消失、咆哮结束 / 对话暂停不残留；③ 3 s 一档听 6 个候选，
  回报哪一号最像 TH17 正常 roar-exit（暂定 6 号 `0x39 se_heal`）。
- **门**：`make check`、`sites.py verify`、`assets/check_player_anm_bounds.py` 全绿；
  未新增 `BP_*`，`.def` 仍 27 exports（26 个 `BP_*` + `thcrap_plugin_init`）。
  部署 DLL md5 见 AUDIT §AL40 与会话报告。

### 1v-补5. 鬼形兽（117）十一修：roar-exit = 0x42 + 试听探针停用 + 撒道具音 A/B（2026-09-12）

> 对应 AUDIT §AL41。`1v-补4` 的「暂定 6 号 `0x39 se_heal`」按用户 2026-09-12 实跑作废：
> 用户回报第 5 声（id `0x42`）才是 TH17 的 roar-exit。

- **roar-exit 锁定 0x42**：`ANIMAL_SE_ROAR_END` `0x38` → `0x42`。证据 = 用户听完 6 候选
  （180 帧一档）回报第 5 声 + 一手 TH17 `th17:0x40f4fc` 在 roar end `push 0x42`。
  旧 `0x38` 是早期候选探查阶段的错误读数，保留备注。
- **试听探针停用**：新增 `ANIMAL_SE_PROBE 0`；试听表 / `se_probe_tick()` / `hyper_end()` 分支 /
  `on_tick_2()` 调用全部改成 `#if ANIMAL_DEBUG_KEY && ANIMAL_SE_PROBE`。默认不编译、不打任何
  probe 日志；正常咆哮退出恢复播 `0x42`。
- **撒道具 A/B（仅 debug）**（★ **已由 §1v-补8 定案：`0x3e`，轮换/`se_scatter_abc` 已删**——本条是当时状态）：
  新增 `scatter_se_select()`，每段连续 `se_left` 序列（一次听得出的
  “撒道具声”）在 `0x34` / `0x43` 间轮换，段首打
  `wba_wc: scatter SE A/B -> id 0x34 (table name se_lgods4)` 或
  `wba_wc: scatter SE A/B -> id 0x43 (table name se_release)`；release 恒 `0x34`，与改动前一致。
- **要用户听 / 回报**：下一轮多跑几次咆哮正常结束，回报 `0x34`（名表 `se_lgods4`）还是
  `0x43`（名表 `se_release`）才是「大量道具」的音。
- **命名口径**：用户解包 `th17/data/` 的 wav 文件名可能与 exe 内建 name table 不一致
  ⇒ **创造性音效选择以用户耳朵实听的 id 为准**，表名只随日志显示，不作最终裁决依据。
- **实跑 grep**：
  - 不再出现 `wba_wc: SE probe active` / `wba_wc: SE probe N/6 id ...`；
  - 正常咆哮结束听到 `0x42`；
  - 撒道具时看到 `wba_wc: scatter SE A/B -> id 0x.. (table name ...)`。
- **门**：`make check`、`sites.py verify`、`assets/check_player_anm_bounds.py`、
  `tooling/check-docs.py` 全绿；未新增 `BP_*`，`.def` 仍 27 exports；部署 md5 见 §AL41 D / 会话报告。



### 1v-补6. 鬼形兽（117）十二修：SPACE 1..7 + 撒道具 A/B/C + 咆哮拦装备子机 + 魔理沙鹫 5 连扇形激光（2026-09-12）

> 对应 AUDIT §AL42。用户 2026-09-12 复听 + 实跑口径：倒数第 4 个 = 第 3 候选 `0x3e`；
> roar-exit 仍是 `0x42`（§1v-补5 已锁）。本轮新增**一个断点** `BP_ce_option_shot_gate`。

- **SPACE 调试键随机 1..7**：`ANIMAL_DEBUG_TYPE_MAX` `3` → `7` ⇒ 一拍空格动物灵 1..3 + 鱼灵
  4..7 都可能出；Shift+空格（4..7 循环）/ Ctrl+空格（强制就绪）不变。日志改口径
  `wba_wc: DEBUG spawn spirit type N at (0, 224) by SPACE`。
- **撒道具 SE A/B/C**（★ **已由 §1v-补8 定案：C = `0x3e`，轮换已删**——本条是当时状态）：
  `scatter_se_select()` 从 A/B 扩成 A/B/C（`0x34` / `0x43` / `0x3e`，
  名表 `se_lgods4` / `se_release` / `se_tan03`），每段连续 `se_left` 序列轮换一次、段首打
  `wba_wc: scatter SE A/B/C -> id 0x.. (table name ...)`。release 恒 `0x34`。
  **要用户听 / 回报**：多撒几次道具，A / B / C 哪个才是「大量道具」的音。
- **咆哮期间装备子机普通弹被拦**：一手定位 = 主炮路径 `Player__tick_shooting_state` `0x45ea00`
  的行循环会按 row `+0x20` 低半字节从**原生子机** `player+0x580+slot*0xf0` 出弹（咆哮换 set 时一起换）；
  但**装备 option 卡**走另一条 `Player__tick_shooters_for_ability_card` `0x40a9c0`（set `0x0a..0x15`），
  咆哮管不到 ⇒ 用户看到「灵梦 / 魔理沙子机照旧打普通激光、松 Z 才停」。新增断点
  `BP_ce_option_shot_gate` `0x40a9c3`：`ready || hypering` 时对原生组 `0x0a..0x15` 返回 1，
  SDK 跳到该函数空路径收尾 `0x40aa9e` 跳过整段（option / anm / 其它卡广播不动）。日志
  `wba_c: option shot suppressed while roaring (retail equip option set 0x.., type .., char ..)`。
  不碰 `0x16`（108）/ `0x17+`（77 / 119）。
- **鹫按角色分派（父任务补充口径）**：pl01 魔理沙的鹫 = **本体发出的 5 条扇形粗激光**
  （TH17 `pl01a.sht` EAGLE 5 行：角度 -90°/-75°/-105°/-60°/-120°、`opt_slot` 低 4 位全 0 =
  本体、`mode=2`）。数据路径的 func 映射核过是对的（init2/tick2/hit2），但与原生真激光行
  逐字段比：原版用 `anm=6`/`hit_h=16`/真子机，TH17 行是 `anm=16`/`hit_h=34`/body+合成
  option；上一轮实跑只出「贴激光图的移动弹」⇒ 按 fallback 走卡侧：`eagle_beam_tick()` 在
  `hyper_type==3` + Z 按住 + 魔理沙时，从本体按上述 5 个角度各挂一条脚本 163 束（视觉 +
  `ce_damage_rect` + 消弹），松 Z / 结束收掉。灵梦 / 咲夜仍走数据路径的 5 颗 body 追尾弹
  （pl00 EAGLE `0x24..0x27`）；早苗原来借 pl01a 模板的 `mode=2` body 行会在 `0x45fcc8`
  空指针崩溃，已把 `HYPER_SOURCE["pl03.sht"]["EAGLE"]` 改成 `pl00a.sht`（安全回退，
  pl03.sht 已随发布同步）。
- **ABI**：新增 `BP_ce_option_shot_gate @28`；`dllverify` 27 → **28 exports**。
- **要用户实跑验证**：
  1. 拍空格随机出 1..7（4..7 = 鱼灵）；
  2. A/B/C 撒道具音选一个；
  3. 灵梦狼咆哮 + 装备 option 卡：普通激光停、出现 suppressed 日志；
  4. 魔理沙狼咆哮：普通激光停；魔理沙鹫咆哮：本体 5 条扇形粗激光（日志
     `eagle beam on (5 body fan lasers, ...)`）；
  5. 灵梦鹫咆哮：5 颗 body 追尾弹，不再有 pl01 激光条。
- **门 / 部署**：`make check`、`sites.py verify`、`assets/check_player_anm_bounds.py`、
  `tooling/check-docs.py` 全绿；DLL md5 见部署后会话报告，四个 modkit 加载路径已
  `sync_modkit_dll.sh` 同步。


### 1v-补7. 鬼形兽（117）十三修：`BP_ce_option_shot_gate` 3 字节站点被拒 → 门失败降级腐化卡对象（2026-09-12）

> 对应 AUDIT §AL43。用户新报告：「卡又成滚木了，撞到槽里会导致 slot 消失」；并补充与更早 id=254
> （0xfe，有符号字节为负）同型。**不是** 224–227 那批美术造成的 sprite 漂移 —— 117 的
> 脚本 137–163 / sprite 映射本轮一对一核过，部署文件里也没漂。

- **根因**：`BP_ce_option_shot_gate` 上一版站点 `0x40a9c3` 只有 **3 字节**（`mov edx,[ebp+8]`），
  低于 thcrap sourcecave 下限 5 ⇒ 断点被拒。部署日志有：
  `FAIL: sdk: breakpoint ce_option_shot_gate not applied (3 bytes at +0000a9c3: 8b 55 08 )` →
  `FAIL: sdk: 1/15 behavior breakpoints not applied` →
  `cards_dev: start_deck disabled` → `mitigation: allocator bound restored to retail (56)`；
  但存档 / `_test` 卡组里仍有 id 117，门失败后引擎继续 `allocate_new_card(id=117, mode=1)`，
  新 id 被拒后在卡表 / HUD 槽侧留下垃圾 ⇒ 用户看到的「滚木 / slot 消失」。
- **修法**：站点改到 `0x40a9c3` 的 `mov edx,[ebp+8]; sub esp,0xc`（6 字节，`8b550883ec0c`，无相对寻址），门回读能过；
  同时新增 `CE_OPTION_SHOT_GATE_ENABLE 0`，**功能默认关**（断点挂上但 handler 直接放行原 `je`）。
  要重开「咆哮中拦装备 option 普通弹」需另开一轮实机验证。
- **新诊断**：`ce_integrity_check()` 每关开场在 `BP_ce_item_frame` 里 dump
  `integrity[stage N]: card[i] id=.. flags=..` 与 `num_total/active/equip/passive + owned[128..254]`；
  异常继续打 `BAD card` / `CARD MANAGER INTEGRITY FAILURE`。下一份日志能直接区分「id 腐烂」还是「纯美术」。
- **新构建硬门**：`assets/check_ability_anm_bounds.py`（进 `make anm-verify`）回读最终
  `ability.anm`，硬校验 117 的脚本 137–163、sprite 范围 / entry 归属、`scriptNew` 目标、
  C 侧 `CE_ANM_ABILITY_SPRITE_*` 同名 entry。
- **ORDER 幂等**：`make_youkai_option_art.py` 的 `check_names()` 修了绝对 Path / 相对主表路径的
  比较 bug，现在主 ORDER 已合并后重跑也不误报「重名」；`make_animal_art.py` 仍只打印、不写
  `ORDER.txt`，224–227 只写 `ORDER.youkai_option.txt`，两边不会互相改表。
- **用户实跑验收**：
  1. 日志不再出现 `FAIL: sdk: breakpoint ce_option_shot_gate not applied` / `mitigation: allocator bound restored`；
  2. 每关开场出现 `integrity[stage ..]` 的逐卡 id/flags + 汇总行；
  3. 收满 5 格 / 咆哮 / 正常结束，左下角 5 个 HUD 槽全程都在；动物灵 / 鱼灵贴图正确；
  4. 咆哮中装备 option 子机普通激光仍会打（功能默认关，已知、非崩坏）；如果要试专项，
     把 `native/sdk.c` 的 `CE_OPTION_SHOT_GATE_ENABLE` 改 1 后回报 `wba_c: option shot suppressed ...`。
- **门**：`make check`（含新 `check_ability_anm_bounds.py`）、`sites.py verify`、`tooling/check-docs.py` 全绿；
  `.def` 仍 28 exports。


### 1v-补8. 鬼形兽（117）十四修：撒道具 SE 定案 = `0x3e`（用户耳朵锁定 C 候选，2026-09-12）

> 对应 AUDIT §AL44。用户口径（原话）：「应该是这个里面的最后一个音效是对的：`…/game/th18_card_expand.log`
> 也就是 C」。依据 = 该日志里 A/B/C 三段试听的**最后一段** `wba_wc: scatter SE C -> id 0x3e (table name se_tan03)`
> ⇒ §1v-补6 的 A `0x34` / B `0x43` 均被否。

- **定案**：`ANIMAL_SE_FISH_ITEMS` `0x34` → **`0x3e`**（试听表名 `se_tan03`）。
  ⚠ 它与 `ANIMAL_SE_RELEASE`（咆哮自然结束放灵那一声，一手 TH17 `th17:0x40e920`）**同 id**
  ⇒ 本卡「放灵 / 撒道具」共用同一个「释放」音（观察，非 RE 结论，与用户两次耳听的口径一致）。
- **试听机构删除**：`s_scatter_se[]` / `ANIMAL_SE_SCATTER_N` / `se_scatter_abc` 字段与 run-reset 初始化全删；
  `scatter_se_select()` 只剩「段首（`se_left <= 0`）认一次 `ANIMAL_SE_FISH_ITEMS`」+ 一行
  `wba_wc: scatter SE -> id 0x3e (locked by user ear 2026-09-12; candidates were A 0x34 se_lgods4 / B 0x43 se_release / C 0x3e se_tan03)`。
  ⇒ **没有 `#if ANIMAL_DEBUG_KEY` 分支了**：debug / release 同音，用户继续用 debug build 复跑也不会再听到被否的 A / B
  （旧写法 release 恒 `0x34`、debug 还轮换，已不适用）。
- **`se_volume_fix()`** 跟着改查 `0x3e` 行的 `+0x0a`（原 `0x34`）；priority 非 0 时不写、不打日志。
- **要用户听 / 回报**：不用再听了；复跑时日志应只剩一种撒道具 SE（`id 0x3e`），耳朵听到的就是 TH17 那一声。
- **门**：`make check` 全绿；**未新增 `BP_*`** ⇒ `.def` 仍 28 exports；部署 md5 / 四路径同步见会话报告。

### 1v-补9. 鬼形兽（117）十五修：bug2 静态对账 + bug6 鹫束变宽/外流 + bug7/8 渐变圆按路径收回 & 每路播 0x42 + 鱼灵不换种类（2026-09-13）

> 对应 AUDIT §AL45 / §AL46 / §AL47 / §AL48。**未新增 `BP_*`**（`on_bullet_created` 走既有 vtable `+0x28`），`.def` 仍 28 exports。

- **bug2（灵梦鹫打不掉血 / 无击中动画 / 穿过去）**：静态对账结论 = **数据没坏**。
  `native/build/sht/pl00.sht` set `0x24..0x27` 与 TH17 `pl00a.sht` set16 逐字段一致（dmg 20 / 16×16 /
  speed 10 / mode 1 / func 1,1,0）；`hit func 0` 在 TH17 与 TH18 都是 NULL ⇒ **无额外命中特效是原生行为**；
  伤害走引擎伤害源管线、与 `func_on_hit` 无关。新增日志
  `wba_c: eagle body bullet created (char 0 set 0x24 row 0, dmg 20; hit data-path auto)`（封顶 3 条）。
  **实机要看的**：① 这行有没有（= 弹真的发出来）；② 敌血条掉不掉（判伤害，别用爆炸判断）；
  ③ 若仍追不到，下一步查追踪半径（TH18 tick1 = 256）。
- **bug6（魔理沙鹫束）**：视觉宽 32 → **128 px**（TH17 script16 `scale y=4`；判定宽保持 34 = TH17 `hit_h`）；
  脚本 163 加 `scrollX(-1/21)` 让贴图沿束外流；C 侧 `eagle_beam_vis_len()` 按 `TRAVEL/SHOT_FRAMES/LOOP`
  做「每 15 帧一发」的延伸（远端往外扫）。**实机要看的**：① 束明显变粗、远端在动；② 判定/伤害与改动前一致；
  ③ 若嫌「锯齿」太跳，把 `ANIMAL_EAGLE_BEAM_LOOP` 改 0（拉满一次保持）。
- **bug7/8（渐变圆收回 + 0x42）**：X / 中弹起手补发渐变圆 **interrupt 2**（TH17 `0x40f7bf`），
  咆哮结束仍发 **interrupt 1**（`0x40f508`）；`ANIMAL_SE_ROAR_END`(0x42) 提到自然 / 霊撃两路公共尾部播放。
  **实机要看的**：① X / 中弹退出时渐变圆「先缩到半程再慢慢没」（≈70 帧，比自然结束慢）；② 自然结束时长不变；
  ③ **三条退出路（自然 / X / 中弹）都恰好听到一次 0x42**（不再只有自然结束有）。
- **追加（用户 2026-09-13「鱼灵从一开始就不应该换种类」）**：`token_spawn()` 对鱼灵无条件清零
  `long_life` ⇒ 鱼灵不进 `tokens_tick` 的变色块（不 re-VM、不闪、不重置 life、不播 `0x4e`）；
  `token_change_type()` 恢复无条件播 `0x4e`（鱼灵到不了）。一手：TH17 `spawn_token` 只有 type 15..17
  稀有分支写 `[token+0x6c] |= 2`；type 4..7 非稀有分支保持 0 ⇒ `sub_410af0` 整块 life/变色跳过。
  **实机判据**：SPACE 连撒鱼灵后日志**不再**出现 `wba_wc: token flicker`，耳朵**不再**听到 0x4e；
  鱼灵只走「被收 → 换道具」；一手没有别的到期行为 ⇒ 继续游到被收或离屏。

### 1v-补10. 鬼形兽（117）十六修：灵梦鹫追尾补正 / 魔理沙鹫激光整体 ×4 + 不消弹 / 对话音效不推迟 / 换面清实体 / 暴走放灵类型（2026-09-13）

> 对应 AUDIT §AL49。**未新增 `BP_*`**（`on_stage_start` / `on_tick_2` 全是既有 vtable），`.def` 仍 28 exports。

- **(1) 灵梦鹫追尾**：一手对账证明 TH17 tick1 ≡ TH18 tick1（半径都 256、转角 0.08、60 帧窗口、速度三档全同；
  pl00a EAGLE tail 全 0 且 tick1 不读 shooter）⇒ func 映射 `1→1` **是对的**。缺口 = 转弯半径 125 px 太大，
  侧翼行进极限环、60 帧后直飞。**卡侧补正**：`eagle_homing_assist()` 每帧对 EAGLE body 弹（set 0x24..0x27）
  再叠 `ANIMAL_EAGLE_HOMING_EXTRA = 0.08f` 转角（总 ~0.16/帧）；TH17 数据一个不改。
  **实机要看的**：侧翼行不再绕半圈直飞、命中显著改善；不想要就把该常量改 `0.0f`。
- **(2) 魔理沙鹫激光**：`ANIMAL_EAGLE_BEAM_SCALE = 4.0` 是**唯一旋钮**，两轴共用 ⇒ 视觉 **2048×128 px**
  （远端出屏），判定长跟视觉、判定宽仍一手 34；`MIN_FRAC=0.5` ⇒ 最短 1024 px 仍出屏。
  **删掉** `eagle_beam_tick` 的 5 圆 `ce_cancel_radius` + `ANIMAL_EAGLE_BEAM_CANCEL_*`（TH17 不消弹）。
  **实机要看的**：① 束远端顶出画面（还看见束尾就把 `SCALE` 往大调，日志会打实际值）；
  ② 按住 Z 不再消弹；③ `hyper_frame` 的霊撃全屏消弹不变。
- **(3) 对话音效**：`fish_to_items()` 入队即按 `dialogue_open()` 拦掉；`on_tick_2` 的 `paused` 分支兜底丢弃
  （`se_left=0/se_gap=0` + 日志）。`release_tick` 释放动画**不动**（对话中照旧冻结）。
  **实机判据**：对话期吃 P/金钱鱼灵，对话结束后**不应**再补播那串 `0x3e`。
- **(4) 换面清实体**：挂 `on_stage_start`（`+0x34` = `GameThread__thread_start` `th18:0x442F98`），
  `clear_card(c, keep_hud_slot=1)` 清 token 实体 + 咆哮 VM + core + HUD 绘制；`token_clear_all()` 升级成
  「扫满 40 只 + memset + `s_n_tok=0`」。**商店走 `on_load`、不经过 `+0x34`** ⇒ 商店路径一个字节不动。
  **实机判据**：1面→2面后旧动物灵不再在换种类时冒出来；进商店前后动物灵/槽位照旧保留。
- **(5) 暴走放灵类型**：一手 TH17 `0x40ee03` 是 `rand()%3+15`（**随机**，不读 `hyper_type`）；
  按用户口径覆盖成「暴走 1..3 ⇒ 2 只都 = 本咆哮类型」，混合 4 / 未知 0 ⇒ 回退一手随机（注释标明非一手）。
  `s->burst_type` 在 `hyper_end` 清 core 前捕获（原占位字段改名）；日志打 `type X Y, roar_type Z`。
  `long_life=1`（变色版）保持。**实机判据**：狼暴走结束后日志/画面应是 **2 只狼**（不是 2 只随机）。


### 1v-补11. 鬼形兽（117）十七修：狼咆哮子机仍在开火 = 装备子机路径拿咆哮组当 set（2026-09-13）

> 对应 AUDIT §AL50。用户 2026-09-13 实跑：「按 Z 进魔理沙狼咆哮子机还发射激光；进大鹫咆哮子机不发射激光了」。

- **根因**：`BP_ce_option_shot_gate` 原来只对 `set ∈ 0x0a..0x15`（原生装备子机组）询问，但咆哮期间
  `Player__tick_shooters_for_ability_card` 会用**咆哮组**当 set：WOLF `0x1e..0x22` 是**非空组** ⇒ 子机照旧开火；
  EAGLE `0x23..0x27` 是空组 ⇒ 看不出问题（所以上一轮只测鹫时误判修好了）。
- **修法**：询问区间放宽成 `0x0a..0x15 ∪ 0x1e..0x27`；常量集中到 `native/sdk/public.h`
  （`CE_SHT_SET_OPTION_MIN/MAX` + `CE_SHT_SET_BEAST_MIN/MAX`，注释钉死与 `sht_ids.h` 同源），
  `sdk.c` 与 `animal.c` 的二次校验同步。`0x16..0x1d`（108/77/116 的组）不进区间 ⇒ 同卡组不受影响。
- **为什么拦而不复刻**：TH17 是**一套**射击组（本体+子机共用、换组一起换、行里 `slot` 决定从哪只子机出弹）；
  `pl01a` WOLF 5 行 `slot` 全 0 ⇒ TH17 狼咆哮子机本来不出弹，我们拦掉后观感一致；EAGLE 行 1..4 才是子机出弹，
  但 TH18 的 mode=2 激光在无真子机时会崩（§AL36/§AL39）⇒ 鹫改卡侧束。
- **回退方案（用户提的纯工程解，未采用）**：咆哮期间直接吞 Z 输入 —— 副作用全局，先用窄区间精确拦。
- **实机判据**：狼咆哮应看到 `wba_c: option shot suppressed while roaring (… set 0x1e …)`，子机不出弹。


### 1v-补12. 鬼形兽（117）十八修（真身）：咆哮漏的是**咆哮前已经打出来的旧 `mode=2` 激光**（2026-09-13）

> 对应 AUDIT §AL51，**订正 §1v-补11 / §AL50 的根因**。

- **§1v-补11 错在哪**：11 个 `call 0x40a9c0`（`Player__tick_shooters_for_ability_card`）的调用点
  全部推硬编码 set `0x0a..0x15`，**没有**「咆哮组当 set」这回事；而 dev 卡组 `[117,21,22]` 没装备子机卡，
  所以 `8ad1a9b2` 日志里的 0 条 `option shot path set` 证明不了用户实战没走那条路。
- **真身**：`Player__tick_shooting_state 0x45ea00` 的 set 查询只有 `0x45ebfb` 一处（聚焦/非聚焦都走它，
  hook 已覆盖两条通道，§H1 的「第二处查询」不存在）；重定向对**新**弹生效（日志/字节 dump 都对齐），
  但 `mode=2` 激光的存活**不跟 set**：`tick2 0x45fc80` 的 `0x4601e0 cmp [player+0x476d4],0 / jl 自杀`
  只看射击短计时 ⇒ 咆哮前打出的普通子机激光在咆哮里**一直钉在原处**。聚焦 set 5..9 没有 `mode=2` 行，
  所以按住 shift 进咆哮看不到 ⇒ 与用户观察逐字吻合。
- **修法**：新增 `ce_kill_retail_option_lasers()`（`native/sdk.c`），`hyper_shot_apply()` 每咆哮帧收掉
  `packed set ≤ 0x15` 的 `mode=2` 活动弹（state=2 + 束 VM interrupt 1）；不碰 108/77/116（0x16..0x1d）
  与 WOLF/EAGLE（0x1e..0x27）。**无新 BP / 无新 cave / 不改 TH17 数据**；WOLF 5 发本体导弹原样。
- **实机判据**：进狼/鹫咆哮、按住 Z ⇒ 日志出现 `wba_c: retail option lasers killed (n=…)`（n≥1 就是真身），
  且画面不再有旧束；`roar bullet set census #N: set 0x…` 应只出现 `0x1f..0x22` / `0x24..0x27`
  （出现 `0x00..0x09` = 主炮重定向失效；出现 `0x0a..0x15` = 装备子机路径在发，另一路）。
- **部署**：用户退游戏后重跑 `bash local/sync_modkit_dll.sh`（本次 `thcrap/bin` 那份被占用，仍是旧的
  `8ad1a9b2`）；四路径应统一到 `b30a42845cb469e0f1ff42b0ad323c09`。

### 1v-补13. 鬼形兽（117）二十修：鹫束展开改用收回的倒放 + 中弹掉落的灵改成会变色（2026-09-13）

> 对应 AUDIT §AL52。用户 2026-09-13 实跑两条：
> ① 鹫束「应当是如收回般（倒放）展开，实测瞬间出现、收回反而正常」；
> ② 「中弹后生成的是固定两个水獭灵、不会切换；按契约从头到尾都不该有不切换的灵」。

- **鹫束展开**：新增 `eagle_beam_grow()`（每发前 **6 帧** 0→1），**两轴共用**（长 `TRAVEL×g`、宽 `SCALE×g`）；
  6 = 脚本 163 `interruptLabel(1)` 的 `scaleTime(6,1,1→0)` 帧数 ⇒ 展开就是收回的倒放。
  删掉 `ANIMAL_EAGLE_BEAM_MIN_FRAC`（不再半长起步）。日志 `… grow 6 frames …`。
- **中弹掉落**：`animal_miss_burst()` 的 `token_spawn(type, 0, …)` ×2 → `(type, 1, …)` ⇒ 会变色/换种类
  （类型仍是本机体凭依兽灵：魔理沙 = 2 獭，随后 2→3→1 循环）。日志改 `miss -> 2 cycling animals (type N, long_life=1)`。
  ⚠ 与「鱼灵永不换种类」不冲突：那条只管 type 4..7。
- **实机判据**：按 Z 时束从 0 展开（约 6 帧）；中弹后 2 只会闪、会换种类的獭灵；狼 5 发本体导弹不变。


### 1v-补14. 鬼形兽（117）二十一修：灵只走四个对角线；中弹掉落类型取随机（2026-09-13）

> 对应 AUDIT §AL53。用户口径：① 空格召唤的灵「似乎不是 45°」；② 「凭依灵在 TH17 里是可选的配置，
> 我在这里设定为她们**没有被凭依**」（魔理沙 ≠ 獭）。

- **出场方向**：`token_spawn()` 的 `ang` 由「8 个 45° 方向」改成**四个对角线**
  `((rand % 4) * 2 + 1) × 45°`。理由与一手自洽：速度下限重掷是**逐轴** `±(0.5+rand×0.3)`，
  只有对角线能一直满足 `|vx|,|vy| ≥ 0.2` ⇒ 灵永远斜着走。
- **中弹掉落类型**：`animal_miss_burst()` 不再用 `1 + 自机序号 % 3`（那是"固定凭依"假设），
  改 `1 + rand%3`；`long_life = 1`（会变色）不变。
- **实机判据**：新灵只走四个对角线；中弹 2 只类型随机且会闪/换种类。

### 1v-补15. 鬼形兽（117）二十三修：道中「仅大怪掉」= 大怪击破掉 1 动物灵（2026-09-13，待实机）

> 对应 AUDIT §AL55。用户口径：「算了，先别动 ECL 了，按照仅大怪掉吧暂时」；触发必须是这只怪
> **被打死掉**（不用波清空）；没有这张卡不能掉（卡侧实现天然满足）。不改 ECL。

- **判据**：stage 1 一手标定后，六种 `Cir*`（README 逐项求和 = 58 只；用户口径写 68，按 dump 实际为 58）为杂鱼，其余 `GirlB/GirlD/GirlC/GirlRed/GirlBlue`、
  Boss/MBoss 为大怪。杂鱼前缀表 = `animal_big_trash_prefixes[]`（默认 `{"Cir"}`，
  见 `TUNING.md` §2.2 唯一旋钮）；HP / hurtbox 单独都不能分开这套名单（队长 HP 100 < 杂鱼 120）。
- **触发**：`on_enemy_death`（`th18:0x430240`）+ `CE_EDEATH_F_REAL` + `KILLED/BOMB_KILL`；
  死亡栈只入 `s_big_drop[32]` 标量队列，`on_tick_2` 帧首 `animal_big_drop_drain()` 建 VM、打掉落行。
- **掉落**：`token_spawn(1 + rand%3, 1, enemy.x, enemy.y − 8)` = 1 只变色动物灵。
- **去重**：Boss / 中 Boss 仍走既有 `on_enemy_drop_pre` 的 `*Dead → 2 资源灵(4+5)`；本路径只打
  skip 行，不重复给动物灵。
- **实机判据**：打 58 只 `Cir*` ⇒ `big-enemy drop` 行 **0**；打 `GirlB01`/`GirlD01`/`GirlD02`/
  `GirlC01`/`GirlRedA01`/`GirlBlueA01`/`GirlRedA01n`/`GirlBlueA01n` ⇒ 每只各 1 行
  `wba_wc: big-enemy drop -> 1 animal spirit (type N, hp=…/max…, drop_id …, sub …) at (x, y)`；
  中/boss 每只各 1 行 skip；若 `Cir*` 出掉落行就往前缀表加一项。

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
4. ~~**道中切波**：同一 `MainSub` 里排了几波（st01 `MainSub00` 的三波阴阳玉）原来被并成一章。
   新增 `LOLK_MID_BURST_GAP 100`：同子程序内静默 ≥100 帧后再刷出 = 新一波
   （burst 内 `wait(10)` 不切、burst 间 `wait(120)` 切）。单测 **134 断言**全过。~~
   ⚠️ **2026-09-13 已被 §1t-补2 取代并删除**：用户洞察「章 = MainSub 编号」，`MainSub00`
   的三段 burst 本来就该是**同一章**，`LOLK_MID_BURST_GAP` 静默窗口不再使用。

## 1b. 商店走两遍（2026-09-04，待实跑）

每关过关后商店开 **2 次**（`shop_core.h` 的 `CE_SHOP_VISITS_DEFAULT`），第二次商品重抽（已买的自动排除）、仍是原生流程（必须买一张）。
实现：`native/shop.c` 两个放行断点（`ce_shop_bought` `0x4183ea` 记成交；`ce_shop_reopen` `0x443b05` 在 GameThread 里把 `0x20000` 位加回 eax），
状态机 `shop_core.c`（主机单测）。引擎链：[`engine/card/th18/05-shop-and-money.md`](../../../engine/card/th18/05-shop-and-money.md) §3.5；审计 AUDIT §P。

日志应有：`shop: 2 visits per stage` → 过关 `shop: opened by msg (visit 1/2 …)` → `shop: bought (…)` → **同一帧** `shop: reopen (visit 2/2 …)`
→ 第二家店（进场动画再来一次，商品不含刚买的）→ `shop: bought` → 正常进下一关。`reopen` 行必须是 `bought + 31`，出现 `(late, gap!)` 就是 P4′ 的空档帧真的发生了。练习模式 / replay 回放里不该出现 `reopen`。
空白卡、买不起、暂停后退到标题都不该多开店（AUDIT P6 / P8）。崩溃优先怀疑：`0x443b05` 处 esi 不是 GameThread（P3）。

通过后：MAP §5 与 AUDIT §P 顶部记「实跑通过」。想改次数就改常量（以后可挂 `cards.js`）；
**关卡中间开店**（MSG opcode 36 或 DLL 直接置 `GameThread+0xb0 |= 0x20000`）技术上可行，但 Stage / Spellcard 不冻结，只该在对话里做——见 §3.5。

## 1t-补2. 绀珠传（115）道中「章 = MainSub 编号」改写（2026-09-13，待实跑）

**用户洞察**：「th15 搁置的章节似乎就是 MainSub 的编号，按编号排，一个编号是一波
（01+01b 是一波）」→ TH18 的 st ECL 里 `MainSub00 / MainSub01 / MainSub01b / MainSub02 …`
的**编号是章/波的天然键**，去掉尾部字母的数字是一章。

**一手定位**：接线层读关卡脚本**主上下文**的子程序名（`zEnemy+0xc` → `ctx+0x4` 子程序号；
`zEnemy+0x1218` → `fm+0x8c` 子程序表，每项 8 字节 `{name, bytecode}`），
`parse_mainsub_number()` 把 `MainSub01b` 解析成 `1`。一手结构：所有字母后缀都是基号的
`@…() async`（8 关全量复核），主上下文名字天然是基号。

**改动**（零新断点、零字节改动、零 ECL 改动）：`lolk_core.c` 的 `mid_boundary()` 改成
「MainSub 编号变了（或没开章）+ 刷出 → 新章」；`main`/`MainFront`/`MainLatter` 段不开章；
删掉旧的 `LOLK_MID_BURST_GAP` 静默窗口与 ECL 524 `chapter_changed` 道中路径；
认不出关卡脚本时才退到 `LOLK_MID_QUIET_GAP`。日志加 `mainsub %d [rule %s]`
（`mainsub` / `quiet-fallback` / `boss`）。

**实机判据**：st01 道中应恰好切成 `MainSub00 / 01(+01b+01c) / 02(含02b) / 03 / 04 / 05 / 06(含06b)`
七章（对照 `dump-st01/README.md` 的出怪表：00=Cir00p/m、01(+01b+01c)=GirlRed/BlueA+GirlC、
02(+02b)=GirlB01+GirlRed/BlueA01n、03=GirlD01、04=Cir01a/Cir02a、05=Cir00bp/m、
06(+06b)=GirlD02+GirlB01）；boss 战仍按非符/符卡（`rule boss`）。
细节/证据见 [`AUDIT.md`](AUDIT.md) §AK9 与 [`engine/chapter/th18/03-midstage-waves.md`](../../../engine/chapter/th18/03-midstage-waves.md)。

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
（23 张 × 205 = 4715 份，远超原生的 560、仍在 8192 内），商店应当照常工作 —— 这是这次改动的直接证据。

**天花板不再是瓶颈**：原生 362 + 23 张新卡（权重 2）161 = 523，现在离 8192 还很远。

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

**装备卡，本 mod 第一张走原生装备卡机制的卡**：身边一颗电球子机（`Player__allocate_option`，引擎管位置 /
聚焦位移 / 进店收起），每 1 秒朝最近的敌人（512 px 内）**瞬间**劈一道电弧，那一个敌人吃 120 伤害
（连续 4 帧 × 30 的定点伤害源 `ce_damage_rect` 钉在目标上 + 电弧 / 火花两条特效 VM；2026-09-06 平衡：原 80 / 2 s 只有原生子机卡的三分之一）。实现 `native/cards/broken_core.c` +
`broken_core_core.c`；设计 `docs/superpowers/specs/2026-09-05-broken-core-design.md`；审计 AUDIT §U。
**不改任何游戏资源文件**（只多 `ability.anm` 的两个 entry + 三个脚本）。`category: 1`：图鉴 / 编成里应排在原生子机卡 REIMU_OP…MAGATAMA **那一段之后**、而不是所有新卡的末尾（2026-09-06 顺序表按类别插入，AUDIT M12；日志 `menu: new card 71 (category 1) at order[…], after id 51`）。
图鉴整体应是：第 2 行 MOKOU 后接强欲之壶、方片 2；第 4 行 MAGATAMA 后接破损核心；第 7–8 行 MUKADE 后接黑桃 ×5、黄昏、加倍、腐化；
第 10 行 RICEBALL 后接反转牌、神之宣告、青眼白龙、炎魔之王；最后一行只有 MAGATAMA2（原生就把它放在最后）。

**顺带解决了 `engine/card/th18/OPEN-questions.md` §1**（装备卡的子机 shooter 数据存在哪）：就在四个 `pl0X.sht`
的 `+0xe0` 偏移数组里，40 项、原生用 23 项、尾部 17 项空着。新开的 `engine/sht/th18/` 两篇是布局与字段图；
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
`on_power_level_change`（破损核心会在此重申请子机——原生装备卡同款）、`ce_rand` 的 thiscall、V17 震屏工厂的 fastcall + `ret 0x10`（第一颗火球落地那帧崩就是它）。
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
真没应用的话开火时会去解析原生留空的第 `0x17` 项 → 指向 set 0 → fire_rate 0 → **除零**）。
`dist/patch-step3/th18/pl0[0-3].sht` 四个文件必须在包里（`files.js` 应列出它们）。
视觉怀疑：豌豆太大 / 太小（script100 / 101 的 `scale`）、子机位置（`PS_OPTION_OFFSET`）、
机枪那堵墙太宽 / 太窄（`append_shooterset.py` 里 `PEA_GATLING` 的七个 `spawn_dx`）。

通过后：CARDS.md 🔧 → ✅；AUDIT §AJ 顶部记「实跑通过」；MAP.md §5 那条记一行。

## 1l. 古锭刀（id 76，2026-09-07，待实跑，致敬三国杀）

被动「武器」：boss 的最后一张符卡（终符）期间自机弹伤害 ×2，每敌每帧上限同步抬到 sht 基值 ×2；炸弹 / 其他卡不翻。
不开新断点、不改字节、不调新引擎函数。判据：符卡进行中 && boss 活动中断槽子程序名含 `Dead`（ECL 一手：七关 + Extra 道中全覆盖，
[`engine/ecl/th18/01`](../../../engine/ecl/th18/01-boss-interrupts-and-spellcard.md) §6）。审计 [`AUDIT.md`](AUDIT.md) §Y。
`cards_dev.js` 起手卡组没带它——`_test` 商店里买，或临时把起手一格换成 76。**要在原生关看**（devstage 空壳 ECL 的 boss 只有一段攻击）。

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
数字变 **1**，boss 血条比单放炸弹多掉一截（devstage ÷100 一发就死，换原生关看）；第二发 → 数字 **0**；第三发 → `truesilver: bomb without durability`，无额外效果。
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
编成里**不该**出现它（`deck_visible: 0`）。删装备卡（破损核心 / 原生子机卡）→ 子机留到关店、出店时按剩余卡重建。
崩溃优先怀疑 W3 / W4（`+0x50` thiscall、`0x408d00` ret 4）；幽灵图标 / HUD 不重排看 W4。

通过后：CARDS.md 两行 🔧 → ✅；AUDIT §W 顶部记「实跑通过」。

## 1e. 音效表扩容 / 语音（2026-09-05，待实跑）

原生音效表是写死的 84 槽（`0x00`–`0x53`），**一个空闲 id 都没有**。四张表（cfg `0x4c9b80`、
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
2. **批次结算**：开局站桩吃道具 → 收点流真停后 ~8 游玩帧（0.13 s；presence 续批 §Z21，窗口 `C2X_BATCH_SETTLE_IDLE` = 8）日志 `collect2x: settle N items, batch score … -> x1.200, +… score, frag type 0x11 x1 (t=…)`（**hex type = 新 DLL 记号**：0x10 EXTEND2 命碎卡 / 0x11 BOMB2 符卡碎卡——虹龙洞原版卡道具；旧版打 `frag type 4/6` = 别作宝石残留，见 AUDIT §Z12）；**延迟 20 帧后**应另有一行 `collect2x: frag card type 0x11 x1 spawn @(px,sy) t=…`（2026-09-08 节奏变更：结算帧只出弹字/分值，卡延迟到点才从**当下**自机位弹——TH14「先结算后弹碎片」；**两行 `t=` 差 ≈ 20**，若体感不对调 `C2X_FRAG_SPAWN_DELAY_FRAMES`）。★ 2026-09-08 第二跑根因（AUDIT §Z14）：碎片延迟曾**完全不弹** + 掉 P 三轮零出现 = 帧钟选错——`card+0x20` zTimer 只被**主动卡** tick（sdk.c `ce_sdk_active_tick` 首行 `active_recharge` 门），被动卡读它是死值。已换接线层自计数（on_item_frame 分派 = `0x446ee3` 门卫 CMP，每正常游玩帧一次）→ **若此轮 t 不涨或 spawn 仍不出，回查分派与计数**。HUD 分数应出现**负补差**（×0.5/×0.8 档分数比收集前低一点）与 **×2.0 档大跳**。碎片从自机上方 ~76px 弹、1.2 速短弧（~1.4s）后被原版 swoosh 收回，**外观应是 TH18 卡**；4:1 循环：4 次 0x11 后第 5 次 0x10，≥60 批恒 0x10。★ **批边界 = TH14 原版 timer 语义 + presence 续批（§Z18 结构定案 + §Z21 收窗；Z20 的 30 帧窗已撤）**：原版重载常量 8（一手 `[0x4f5884]` @ th14 `0x43898e`/`0x439d60`、帧尾 `0x438dad` 递减归零即结）——续期门在 item state-3 飞行 body、距离判定之前 → **在飞批集道具每帧重载 timer**，批闭 = 收点流真停后 ~8 帧（DDC 同拍）。TH18 过线只吸引（POC 线 `player+0x47998` 在、不批结算）、70px 环滴入 → 波内吸收空洞 8–25 帧常态（第六跑把「一次收点」腰斩成 ≥20+≥20 双结算、20 帧 pending 窗内二次 settle 覆盖 → 基本一次碎片）；Z20 把 idle 放大到 30（0.5 s）兜空洞 → run-7 实跑拆批不再现、但「体感慢了」（0.5 s 拖拍）→ **Z21：idle 收回原生 8 + presence 续批**：环内道具在飞（TH18 state {3,4}，flip 点 `0x445f6f`/`0x445f63` 置 3、`0x446647` 置 4）每帧续期 → 空洞被在飞散布盖掉，只有「环内无在飞且无新收 ≥8 帧」的真停段才关批。TH15/LoLK 对照 census：批记账块只写不读、timer 无消费端 → 原版没有批结算可对齐（机制变了，§Z21 证据 1–2）。结构照旧：timer idle 门、**在飞残余点不挡批**、迟到拆下一批（原版原义，DDC 连锁如此断）；不再查池空（池空门实跑三症状：钟声比 TH14 急 ~7 帧 / NO BONUS 偶缺 / 躲弹幕时突然冒过期 ×0.5——全部同根，见 §Z18）。★ **批集 = {P, 钱}（2026-09-08 §Z19：TH14 官方「红点与蓝点」→ TH18 无蓝点生态位）**：碎片/卡/转换点 9–0xe 一律不入批 —— 击破转换（boss 破招强制收集的转分点潮）**不得**触发碎片/bonus（DDC 原义，TH14 `0x43898e` 计时门 type≠9/0xa/0xb）。★ **批分值 = 批内 Σ(P/钱自带小分)**（Z19 尾段订正：P 例程 `0x446870` 尾 `0x446ac0` 与钱例程 `0x446b00` 尾 `0x446d03` 都无条件 `SCORE += 身价/10`，cap 1e9）——settle 日志 `batch score` 字段与 HUD 补差应**非零**；若站桩吃一串 P 日志里 `batch score` 显示 0，回查 stash 窗。★ **第八跑焦点（run-7 = Z20 的 30 帧窗：拆批不再现但 0.5 s 拖拍「体感慢了」→ Z21 处置，AUDIT §Z21）**：a) **钟声 `0x39` = `se_pin00`**（第五跑确认；TH14 原版真值：`0x438dfe` 系 ≥20 五档弹字前 `push 0x39 → play_sound_at_position`；th14/th15/th18 的 0x39 行同 wav 同调校 −2dB/prio 100）应在收点流真停后 ~0.13 s （≈8 帧）内响 —— **不再 0.5 s 拖拍**；比 8 帧还钝 = presence 没续上（回查 `ce_item_pool_any_attracting` 谓词与接线），太急 = 结算打断收点流（钟声响起时环里还有道具在飞）。b) **一次收点 = 一次结算**：攒波上线收点 + 持续击杀补充应并成一批（单条 settle、弹字完整、碎片一枚）；若再拆 = 连续 ≥8 帧环内无在飞且无新收的真空洞（细尾到达散布），兜底 = `C2X_BATCH_SETTLE_IDLE` 16（Z21 证据残留 6）——先报日志再动。躲弹幕期（无收点动作）**不再凭空冒结算**。音/弹字都在 settle 帧，不存在挪进 `spawn_pending_frags` 的分歧。★ **第九跑焦点（run-8 = 无全屏收点事件也误弹灰 NO BONUS + 下收点线后新掉 P 仍入批拉长 → Z22 成员门处置，AUDIT §Z22；DLL 22:56 重建 md5 `f8d9496c` 已同步 mods/）**：开批/计数/续批三处全收窄到「**真空飞行（state 3）**」—— 接线层 `on_item_collect` 成员门只认**分发时 state==3** 的 {1,2} 收集（collect2x.c:220）；presence 谓词 `ce_item_pool_any_attracting` 只扫 state 3（sdk.c:426）；70px 环收 state 4 = 普通游玩磁吸，不开批也不续批。三件事：a) **非全屏收点期 collect2x 零活动**——躲弹幕/站桩时触收、环收掉落的 P 只走原生计分：不得开批、不得 settle、不得灰 NO BONUS、钟不响（掉落节流照旧，被动不受成员门影响）。b) **三种全屏收点（放 B / 上线 / 对话自动全屏收）回归**——批照开、≥20 按档结算、钟声在流真停后 ~0.13 s、弹字/碎片/批分非零都不变（Z19–Z21 成果不得被成员门误伤；一次收点 = 一次结算）。c) **下收点线瞬间（用户语义：飞过来的 P 收完再结算）**——已在飞的残余 P 继续飞到自机、分发时 state 仍 = 3 → 并入本批、续批；**最后一枚收完 ~8 帧后 settle**（钟声在残余收完时响，不是下线瞬间）；此后线下新掉落、只坠不飞的 P（state 1 坠落 → 环收 4）不开批 —— 预期日志 = 一条 `settle` 后 collect2x 静默。残余迟到分簇（到达间隙 >8 帧）= 原生拆批语义（DDC 连锁如此断）；若迟到小批 5–19 枚会再弹一次灰 NO BONUS = **真批原义、不是 run-8 的 bug**（run-8 bug = 无任何全屏事件凭空开批）——先报 settle 行与 `t=` 序列再定性。Z21 残留 6 的「idle 16 兜底」已随成员门作废，窗口与谓词都不再动。
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

### 2.1 ★ 计分层重做后剩下的 5 条 🟡 小活（用户 2026-09-13 交办，**已列进待办**）

> 层本身已重做（`SCORE-LAYER-REWORK.md`，`make check` 绿）；下面这 5 条是**各作「额外得点」还缺的数字**，
> 不影响层跑，但影响「那张卡算得对不对」。**每条都还缺一手，别按印象填。**

| # | 缺什么 | 现状 / 怎么接 | 去哪查 |
| --- | --- | --- | --- |
| 1 | **`G_13` 神灵庙的基础与上限** —— 蓝灵 +10（灵界 +100）、白灵 ≡ th18 金钱 | 113 的计分声明**仍停用**（`cards/trance.c` 的「计分声明：已停用」段）；层现在有 `ce_score_declare_delta/coupled` 两条通道可接 | `engine/spirit/th13/`、`docs/superpowers/specs/` 的 113 spec §9 #5 |
| 2 | **`G_15` 绀珠传的基础与上限** —— 绿分数道具 `1C/2C +10` … `6C +50`、每 10 擦弹 +10、绿点每 5 个 +10（与擦弹共用计数器）、章节结算 | 115 目前**不进计分层**（它只动 HUD 与擦弹减速） | `cards/pofv.c`、115 的 spec |
| 3 | **`G_07` 妖妖梦的基础与上限** —— 樱点道具 `+1000+100×符卡奖励`、小樱点 `+30`、★ `+20`（B 中 +5）、结界中擦弹 `+80/+30`、结界正常结束 `+10000` | **分档加值已有一手**（`cards/senro_core.h` 的数值表），缺的是「基础与上限」这条口径 | `cards/senro_core.h`、`local/_scratch/research-107-th07-remaining.md` |
| 4 | **`G_08` 永夜抄的刻符怎么涨夜点** —— 原「人妖槽 ×2」是错的，真实是**开关式翻倍** | 108 目前**不进计分层**（妖机体/使魔那条线） | `th08:0x440f03`（`local/th08.v1.00d/th08.exe`） |
| 5 | **`coeff_06` 那条验证** —— TH18 的「身价」是否**就是**按收取高度算的 | **一半已结**：判据（回收线 `th18:0x47998`）与「系数乘在最终值上」已钉死；**线下那条折扣曲线的精确形状没钉死**（两种整数复刻都对不上引擎现场值） | `th18:0x446bf9`–`0x446cf3`；结论与两个反例在 `SCORE-LAYER-REWORK.md` §4 |

⚠ 接这 5 条时先读 `SCORE-LAYER-REWORK.md` §5 的三条坑：**基础只能出现一次**（层出 `G_0`，卡只交 Δ）、
**别自己乘耦合系数**（只有 `ce_score_declare_coupled` 那一族会被 `(擦弹倍率 + 信号强度)` 缩放）、
**`0` 在 legacy 通道里是「不参与」而不是「声明了 0」**。

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

> **★ 2026-09-15b 待办：微档点还是不建实体 ⇒ 微档那张新图在实跑里看不见。**
> 三档外观已经接正（`TUNING.md` §2.6b：大 = `FAITH_DOT_BIG.png` / 小 = `FAITH_DOT.png`
> / 微 = `FAITH_DOT_MICRO.png`，且微档另用出生脚本 273），但 `faith_spawn()` 对
> `FAITH_TIER_MICRO` 仍然直接 `return 0` ⇒ **画面上只有两档**。要让它显形需要三件事：
> ① 定微档点进哪个池与上限（现在只分了 大 / 小 两区，见 `faith.h` 的 `FAITH_BIG_POOL` /
> `FAITH_SMALL_POOL`；消弹一次几十颗，得先算容量）；② 让消弹侧（`mof.c` / `signal.c` 的
> `faith_cancel_account()` 调用点）按颗或按上限撒微档实体 —— **那两张是别人的卡**，
> 动之前先问 + 登记 `AUDIT.md` §BE；③ 实跑看三档是否真「一眼可分」
> （微 = 10px 素绿点 + 加算 + `alpha(16)` + 从 0 涨出来）。

### 卡状态

| id | 卡 | 卡侧文件 | 状态 |
| --- | --- | --- | --- |
| 106 | 红魔乡 PlayRank | `th06rank.c` | 🔧 已实装待实跑（40188 断言；`rank→难度` 用户拍板搁置）|
| 107 | 妖妖梦 樱点/森罗结界 | `senro.c` | 🔧 已实装待实跑（12258 断言；结界特效 VM 待资产）|
| 108 | 永夜抄 妖机体/使魔 | `youkai.c` | 🔧 已实装待实跑（220 断言；发射口精确闸已接；**资产未到 ⇒ 低速打不出弹**）|
| 109 | 花映塚 爆风/蓄力 | `pofv.c` | 🔧 已实装待实跑（527 断言；CA 无敌帧数已订正）|
| 110 | 风神录 信仰/灵击 | `mof.c` + `faith.c` | 🔧 已实装待实跑（**921** 断言；★ 2026-09-15 灵击按 TH10 一手重写：会长到 160/112/64 的消弹圈、每敌每帧伤害、绿点绿色 `+N`、作废符卡奖励、RANK、**删震屏** ⇒ 旋钮与实跑看点见 `TUNING.md` §2.5）|
| 111 | 地灵殿 信号槽 | `signal.c` | 🔧 已实装待实跑（233 断言；绿点来源已补、每关归零已补、乘数通道已接）|
| 112 | 星莲船 飞碟 | `ufo.c` | 🔧 已实装待实跑（**992** 断言；金=蓝、两同一异照抄、**HUD 第一版在层 25**；**真 ECL 已从 TH12 移植**，缺贴图/anm ⇒ 暂不发布）|
| 113 | 神灵庙 灵界 | `trance.c` + `trance_bgm.*` + `trance_core.*` + `assets/sht/append_shooterset.py` + `assets/build_player_anm.py` | 🔧 已实装待实跑（**1169** 断言 + BGM 160；救援层最后一家 = 延时不是免死（落点 `+0x0c`+`return 1`）；实体池 = 原版切图 + 原版脚本；BGM 路线 A 已实装；ABI 缺陷已修 §BP-1；第三轮 §CA；计分载体搁置）；★★ 2026-09-13 **灵界主炮组**（TH13 set10 复刻）⇒ `AUDIT` §CJ |

> **113 的「BGM 变体」本轮已单独做了一手可行性研究**（研究/方案，未改代码与数据）：
> [`engine/_shared/th13-th18-trance-bgm-research.md`](../../../engine/_shared/th13-th18-trance-bgm-research.md)。
> 结论速览：TH13 灵界 BGM **不是双轨**，是切到 `thbgm.fmt` 里独立的 `NNb` track；TH18 只有一路 BGM，但保留了单流 fade 状态机和 `cmd 9` 切轨实现（cmd 9 入口在 TH18 无调用者）。真·交叉淡化建议在 DLL 侧用 `FUN_004898f0` 建第二路内存 `CStreamingSound`（路线 A），最忠实 TH13 的硬切路线要补 `thbgm.dat` + `th18.dat` 里的 fmt（路线 B）。

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

### 1t.3 113 第三轮（2026-09-13）：BGM 路线 A + 原版美术 + **ABI 缺陷订正** + 8 条新口径

用户 2026-09-13 拍板（原文与逐条拆解：`docs/superpowers/specs/2026-09-13-trance-113-round2-requirements.md`）。

**本轮做完的两件**（细节与门见 `AUDIT.md` §BP）：

1. ★★ **修掉一处 ABI 缺陷**：`cards/trance.c` 的 `trance_fn_player_t` 漏了 `__attribute__((thiscall))`
   ⇒ 旧代码把 `zPlayer*` **压栈**、`ecx` 里留着函数地址（= 第一次中弹救援就会写坏 exe 的 `.text`）。
   **静态可复现**（两份 3 行源码 + `objdump -d` 对比，AUDIT §BP-1）。
2. **灵界版 BGM（路线 A）**：DLL 侧用 `th18:0x4898f0` 开第二路内存流 + 与主 BGM 交叉淡化；
   **不改 `thbgm.dat` / `th18.dat`**。占位 PCM 走与真素材同一条路，用户那段 BGM 到位后**只换数据源**。
3. **神灵换原版美术**：`astral.anm` 逐型脚本 + `littlegods.png` 切图（`TUNING` A3 的一半销账）；
   灵界中**整批换样式**（口径 ⑥ 的前半）。

**本轮做完的第四、五件**（细节见 `AUDIT.md` §BQ）：④ **对话强制退出灵界**（一手 = **固定扣 200**，
**不是**用户以为的"等比"）、⑤ **「中弹无伤」闸门**（新增断点 `BP_ce_player_die`，导出 28 → **29**）。

**本轮又加了三件**：**神灵吸引**（`AUDIT` §BU）、**机体特技表**（`AUDIT` §BV：
灵梦=任意距离慢吸 / 魔理沙=无 / **咲夜=灵界时间×2 + 神灵寿命×2** / 早苗=收 type1 额外 +1 槽），
外加**空格调试键**（爆 40 颗：蓝×32/白×6/绿×1/红×1）与 **5 个原生神灵音效**（`AUDIT` §BS-1）。
★ 2026-09-13 续：**数值类效果的跨卡互作**摸清并写进 `AUDIT` §BW（伤害=连乘、移速=每帧 `*=`、无敌=max…）；
**魔理沙的灵界伤害/移速**接在既有通道上（§BW-4）；**灵梦判定点缩小**（3.0/1.5 → 2.0/1.0，§BX）。

★ 2026-09-13 续二（用户抄来 THBWiki 玩法口径，逐条对账见 `AUDIT` §BY）：
**就地补了**「连锁白灵真的撒出来」（#1，此前只记账）与「主动灵界结束 1 秒无敌」（#13）。
★ 2026-09-13 续三（用户给了 TH13 安装路径，解包 + 落地，见 `AUDIT` §BZ）：
**md5 对账**（`th13.exe` / `th13.dat` 与我们样本逐字节相同 ⇒ 地址结论继续有效；`thbgm.dat` 也拿到了）；
**左下角指示槽接通**（4 支叶子 VM：框亮/暗 + 总条 + 游标 + 淡出，逐字段照 `th13:0x42b030`）；
**满格图形提示**（`front.anm` 108/109/110 切图 + 脚本 252–254）。
★ 2026-09-13 续四（**实跑首日 6 条反馈**，逐条处置见 `AUDIT.md` §CA）：
**收集反馈补全** —— 一手重读确认原版收集时神灵本体**没有动画**（`th13:0x418330` 当场销毁 VM），
玩家看到的是「音 + 槽 +2 + **数字弹窗 VM**」⇒ 切 TH13 `front00.png` 的
sprite66/67/69/70/72/73 + 写脚本 **255..260**（type0/1/2 × 普通/灵界），`trance.c` 在收集那一刻
按 `p->style` 起弹窗并记账（删卡时 `trance_popup_clear_all()` 收）；
**指示槽基点真 bug** —— 旧 `(-392,896)` 漏了场地原点（x 为负 ⇒ 4 支 VM 全在屏幕外），
改 `(248,928)`（= `640 + x×2, 32 + y×2`，用户 1280 截图实测的游玩区左下角 257/928 反推）、
尺寸按 UI 层规则 ×2、淡出判据改回一手原值（`x < −64 / y > 384`，自机坐标）；
**满格提示**与**吃灵弹窗**的 `scale` 改 2.0（TH18 同类弹窗贴图是 TH13 的 2 倍）。
★★★ **2026-09-13 续五（贴图错位的根因）**：一手 + 实跑合证 —— **TH18 的 anm entry 序号只能到 255**（原生最大 `abcard.anm` 118 entry；而 sprite 序号 >255 是好的：`enemy.anm` 有 344 sprite、脚本引用 `sprite342`）。我们的 `ability.anm` 有 **367 entry** ⇒ 后 111 个 entry 的贴图全错（画出来是别的图的一个窗口 = 用户看到的"色块/卡片"）。处置：113 的美术整块搬到 `ORDER.txt` 最前（entry 7..32）✅；其它卡的 ≥256 entry 要**合并成多 sprite 的少数 entry**（`build_ability.py` 的"一 entry 一 sprite"硬门要扩）—— 见 `AUDIT.md` §CC。
★★★ **2026-09-13 续六（已做完 ✅）**：图集合并落地 —— `ORDER.txt` 换 v2 语法（`ATLAS` 段 = 一个 entry 多个 sprite）、全库并成 **21 个图集 + 19 单图**，`ability.anm` **367 entry → 48**（余量 208）、sprite 469 → 415、脚本 262 一个没动、111 个坏 entry 全修好；搬序作废（新增 `ORDER.frozen.txt` 重排检测硬门 = 此后只许 append）；108 妖侧本体 55 张按用户裁决**不进图集但源图不删**（走 `REF` → 1×1 `RETIRED_BLANK`）。方案 + 实装记录 + 逐像素自检见 `ATLAS-MERGE-AUDIT.md` §9；协作须知见 `AUDIT.md` §BE-2。
★ 顺手把两条**可复用结论**写进 `engine/anm/th18/01-vm-instantiate.md`：
绘制位置 = `pos + entity_pos + [+0x4e4]`（§3a）、世界层 1 art px = 1 ECL 单位 vs UI 层 = 表面 px（§3b）。
**仍在**：灵界中自机贴图变化（#10，机制未定）、灵界结束时灵体大小复原（#19）、
boss 掉灵通道 A/B（#2/#3，数字已全）、**灵界中"全体"攻击力↑**（#11 —— 见 `AUDIT` §BZ-3a：
灵界位在玩家弹伤害路径上**一次都没出现**，怀疑落在"攻击方式改变"= 换 shot 那一侧，要另解 `pl0Xa.sht`）。

**仍未做（用户同日新点，一手已齐、只差实装）**：见 spec §1 —— 其中
**口徑 ⑤ 的一手**（`engine/player/th18/03-player-hit-gate.md`）**已实装**（`AUDIT` §BQ-1）。
**下一轮（一手都已就绪，按用户口径编号）**：
~~①⑥ 神灵吸引~~ → ★ **2026-09-13 已落地**（96 px 进 / 128 px 出迟滞、`+0.3/帧` 上限 12.0、
漂移衰减、灵梦 state 4 任意距离慢吸、出界删除；判定在 core、积分在卡侧）—— 见 `AUDIT` §BU；
⑦ 掉灵通道（**三条都已实装**，2026-09-13）：
   · **通道 A** = boss 按累计伤害跨阈值掉 1 颗 type 1（`on_enemy_damage`）；
   · **通道 B** = 小怪被击破按 TH13 的 `(T−alive)/(T/N)+1` 衰减掉 2~64 颗 —— TH18 **自带**同形的逐敌人档位
     （`timed_ex_drop_time` + `timed_ex_drop[20]`，ECL opcode 573/574 写在敌人身上）⇒ 直接读敌人自己的字段，
     **不需要**解析 ECL（`AUDIT` §CK）；
   · **通道 C** = **boss 相位击破掉一大批**（用户口径「击破符卡/非符也掉大量灵」）⇒ 挂
     `on_enemy_drop_pre`（与 110/111/116/117 共用断点 `0x430510`），批次 = TH18 那批道具件数 ÷ 2（10~48 颗），
     「击破 vs 超时」由 TH18 `@BossItem*` 模板里那道 `$TIMEOUT` 门决定（`AUDIT` §CL）；
②③⑧ 收神灵弹窗 / 满格与 max 提示 / **顶部半透明倒计时**（在查）；**灵界背景**：一手已查清
（= TH13 effect 回调的**自定义 D3D 全屏四边形**，`th13:0x4032d0`）+ 贴图/脚本/接线全部就位，
**默认关**（开关 `TRANCE_BG_ENABLE`，见 `AUDIT` §BR）。

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
| 1 | **107 结界「持续期」要不要补视觉** | ★ 已订正：原版持续期**有**视觉，只是不是环 —— ①**全屏 sprite 变暗到 37.5 %**（`th07:0x44162a`，绘制侧 `0x406b52`）②**自机红白每 2 帧闪**（`th07:0x441553`），两条覆盖整段 540 帧；环只在发动瞬间 ~33 帧 | 本轮两者**都没搬**；另原版数据自相矛盾：发动时给环设的缩放插值时间基是 **540 帧**（`th07:0x441b56`）而脚本 33 帧就收槽；详见 `engine/spirit/th07/01-barrier-visual.md` |
| 2 | ~~108 低速子机「数量」+ 子口径~~ **✅ 全部已定并落地**：数量 = 各机体在各自作品的数（灵梦→紫 1 / 魔理沙→爱丽丝 1 / 咲夜→蕾米 4 / 早苗→文 `floor(P/100)`）；用户追加「**0 子机得不压原版弹**」也已实现 | ✅ 一手：`th08:0x4c7d40[shottype][4]` 非 0 项数 / `th16:0x444106 idiv`；口径见 `AUDIT` §BO-8 / §BO-10 | 实跑验收未做（早苗 P<100 低速应看到 `NOT suppressing native shots` + 原版弹）|
| 3 | **108 妖侧本体换图走哪条路** | spec §11.3 给了四条候选 + 六条待决。已查明：(a) 注入 `pl0X.anm` 撞「一个字节都不改」铁律，且**一套 24 格要 33120px，四张 256×256 贴图页都放不下**；(b) 叠跟随 VM + 藏本体，但**藏法全无一手指实**（藏不干净＝双人叠影）；(c) 维持现状；(d) hook 跳过本体绘制（最贵） | 108 的「整个人换人」能不能落实；spec §11.3 |
| 4 | **112 彩虹令牌收进去算哪一色** | 一手：TH12 里彩虹令牌按**当时显示的那一色**入账（`0x426c3f/0x426c4d/0x426c5e` = case 10/11/12 与 13/14/15 分别记 红/蓝/绿），**不是万能色**；本卡当独立一色 | 要改成一手语义得动 `ufo_core`；TUNING A8 |
| 5 | **HUD 的 layer 24 → 25** | 只为「P 键截图里能看到」；**机制未坐实**（`0x3b` 合成四边形的覆盖面拿到反证 ⇒ 🟡），且用户当前不能开游戏做判决实验 | 13 个脚本（全是我们自己的卡）批量改一次；AUDIT §BD.1 |
| 6 | **SDK 的 near-corner 门限要不要参数化** | SDK 用 **TH16** 的 400/384（`sdk_core.c:508`），而 TH10 一手是 416/400 + x −128/−112、TH11 是 416/400 + x −64 | 110/111 已按各自原作自算（共享件没动） |
| 7 | **111 的三行数字读数** | 原生 SA **没有**数字读数 ⇒ 那是我们加的（已按老口径标 ❓自定）；块因此 208×56，会把 116/117 往右推（算过仍在同一行） | 要不要保留 / 精简 |
| 9 | **108 子机脚本的中断标签口径** | TH18 原生本体子机是 **2 出场 / 3 进店收起 / 1 销毁**，而 TH08 那三支是 TH08 约定（1/2 循环、3/4 起爆、5 散场）。引擎清槽发的是**标签 1**（`th18:0x45d310`/`0x45d2d0`/`0x45dbf0`；原生装备卡同），而 **2/3 的发送点本轮没找到** | 现照抄原生 ⇒ 收到 1 只是「切到另一段循环」（无害，随后 SDK 删 VM）；若引擎还会发 3，紫那支会切到 46×46 的「隙间」形态并 `stop()` ⇒ 收起动画丢；AUDIT §BO-9 |
| 10 | **108 要不要接 TH16 满火力另一形态** | `th16:0x492c10 = {9,8,12,13}`（文 = `pl03` **script 12**，8 帧同图 + `blendMode(1)` + `type(3)` + 不同帧序）| 要接得**按火力档切脚本**（会触发「先收再要」的重申请）；一手数据已备齐，补一支 228 即可 |
| 8 | **110 要不要进货架** | 本轮把坐标从 ❓ 自定换成**原版原值**（落在 TH10 的 (32..91,448..464)，**在货架左锚点 257 之左**，与原版「那块本来就空着」一致） | `HUD-LAYOUT` §5 的迁移清单 |

### 9z.2 112 的载体：用户已拍板（2026-09-12）

用户原话：「**是不是因为语义的偏差导致了 token 按照真敌人走？得是自建 VM 池子**」
⇒ **token 改成自建 VM 池**（照 117 动物灵 `s_tok[40]` / 113 神灵 `s_spirit[128]` 的写法）—— **✅ 已落地**
（一只 token 一个 VM、位置运动在自己字段上；`ce_spawn_enemy` / `UFO_TOKEN_LIFE` / 「被弹打掉也算收」整条链已删；
两个副作用消失：不再被 `enemy_count_real` 照计、token 打不掉只能碰；`sizeof(ufo_card_t)` 488 → 144；断言 992）。
**被召唤出来的 UFO 敌人仍然必须是真敌人**（有血量/受击盒/要被子弹打死、走 `on_enemy_death`）—— 只换 token。

**为什么它当初走了真敌人**（一句话）：引进 `NoEnemyDebug` 惰性壳是为了「要一个能每帧钉坐标、能挂 anm 的实体，
又不用写 ECL」；但后来 token 的贴图改成**卡侧跟随 VM** 画、位置也由卡侧每帧写 ⇒ **壳变成纯多余**，
却带来三条副作用：① **血量 1 ⇒ 任何子弹都能「收」走 token**（一手是道具、**打不掉**、只能接触收）
⇒ 「碰到才收」形同虚设、**风险收益手感被抹掉**；② 被 `enemy_count_real` 照计 ⇒ **炸弹门/决死门被放行**（§BO-3）；
③ 每次 spawn 有分配成本 + 依赖一个占位 ECL 名。

### 9z.1 还没做、但**不需要**用户决策的（下一轮可直接做）

- ~~**108 使魔可见性**~~ **✅ 已落地**（2026-09-12）：四色原生切图 + 跟随 VM（`yk_fam_vm_spawn/follow/kill_all`，层 7），
  删卡路收得干净；句柄表在文件级静态区（不占 496 B 预算）。
- **`youkai.c` 的本体 sprite 写法要改**：一手证明 `vm+0x24` 直写**不够**（UV/尺寸缓存由 `0x477b00` 填）
  ⇒ 正确形状是 `ce_anm_set_sprite(*(void**)(player+CE_PLAYER_ANM), player+CE_PLAYER_VM, N)`，先例 `season.c:995-1003`。
- **`engine/anm/th07/` 待补**：TH07 `AnmManager::loadAnm` `0x44df90` 的 base「每个 entry 前进上一 entry 的 sprite 数」
  （`0x44e05b`，三处交叉印证）—— 现在只写在 `make_senro_art.py` 的注释里。
- **`185_faith_dot.anm.txt` 的注释要订正**：`ins_63` = `stop`（不是 `delete`）、`ins_53` = `angleVel`（不是 `rotate`）、
  `ins_66`/`ins_67` 可搬；订正块已写在 `make_mof_art.py` 文件头，脚本本体未动。
- **`tooling/thtk/unpack.py` 的 `GAMES` 加 th11**（本轮 111 的资产是手工解的；`local/` 已就位）。

## 1v-补16. 鬼形兽（117）二十三修补：道中「仅大怪掉」前半面 0 掉落 —— 诊断 + 正判化（2026-09-13，待实机）

> 对应 `AUDIT.md` §AL55bis。用户实跑 `8ae8c449…`：中 Boss 前一个都没掉，日志有
> `trace: card 117 on_enemy_death (+0x84) first hit` 但没有 `big-enemy drop` / skip 行。

- **诊断**：`on_enemy_death` 在判据前对每关前 **16** 只死亡打一行
  `wba_wc: death diag kind=… raw_flags=… real_bit=… id=… sub=… hp=…/… hurtbox=… main_drop=… anm_main=…`。
  实机先看这行：`sub` 是 `(null)`、读到别族名、还是字段正常但判据返 0，一眼可证伪。
- **判据**：通用族名前缀表 = `{"Cir", "ECir"}`（表外 = 大怪，三只 `GirlD01` 大蝴蝶因此必掉，不是特例）；
  sub 名读不到时改用 `anm_main` / `hurtbox.x` / `main_drop` 正判，不再用 HP≥500；
  `F_REAL` 不够时用「`KILLED/BOMB_KILL` + 具名 ECL 敌人（排除 `Main*`/`main`/`Ecl_*`）」兜底。
- **验收表**：见 `AUDIT.md` §AL55bis 的 16 行表（1 面所有敌人类型）。实机判据：
  `MainSub03` 三只 `GirlD01` 各 1 行；58 只 `Cir*` 0 行；前半段 `Girl*` 每只 1 行。
- **未改**：ECL、`sdk.c` / `sdk/public.h` / `sites.py` / `.def`（无新 `BP_*`，`ce_integrity_check()` 保留）。

## 1v-补17. 鬼形兽（117）二十四修：TH17 中 Boss 掉落接入（2026-09-13，待实机）

> 对应 `AUDIT.md` §AL56。用户口径：「中 boss 得看 TH17 怎么掉，我们就怎么掉」。

- **一手表**：主线 `st01–st06` 中 Boss 死亡 = `BossItemPhase(...)` 主 token + raw `dropMain(19)`→token4 +
  `dropMain(20)`→token5；st05/st06 无条件追加 `dropMain(30)`→token15（长寿命狼）；
  st01–st04 的 `BossItemPhase` 主掉落是条件稀有 item23..26；Extra `st07` 另有 `MBoss2`/`MBoss3`
  相位边界 = item33 随机长寿命动物 + token4+token5。文件/子程序/行号见 `AUDIT.md` §AL56 表。
- **卡侧**：`MBoss*` 不再走 `*Dead → 2 资源灵` 旧结账；中 Boss 死亡发 token4+token5（st05/06 再发 token1
  `long_life=1` 表达 token15），相位边界发 1 随机长寿命动物+token4+token5；旧 `boss death -> …` 行不再给中 Boss。
- **已知缺口**：稀有动物 token8..14 无 `beast2*` 贴图/脚本，卡侧只打
  `midboss rare conditional ... not spawned`，不发错种类；这是资产缺口，不是判据回退。
- **实机判据**：中 Boss 死亡应见 `wba_wc: midboss drop -> 2 resource spirits (4+5) … (TH17 stXXmbs: …)`，
  st05/06 带 `+ 1 long-life wolf`；Extra 相位边界应见 `midboss phase drop -> 1 long-life animal … + 2 resource`；
  且**不应**再看到 `wba_wc: boss death -> 2 resource spirits (4+5)`（中 Boss）。

### 1v-补18. 稀有动物灵按「不存在」处理（用户口径 2026-09-13）

> ★ **2026-09-13 用户口径：稀有动物灵（token8..14 / item23–29）「当不存在」** —— 不实现、不做资产、
> 条目也不进列表；中boss 的条件稀有主掉落分支**命中时什么都不生成**（当前只留一行
> `midboss rare conditional … not spawned` 作可证伪痕迹）。⇒ 中boss 实得掉落 =
> **token4 + token5**（st05/06 再 +1 长寿命狼）；Extra 相位边界 = 1 随机长寿命动物 + 2 资源灵。

### 1t-补3. 115 道中 boss 单独成章：用户裁决 B（保留）

> ★ **2026-09-13 用户裁决（选 B）：中boss 入场也单独开一章——保留现状。**
> 即 st01 的 `MBoss` 与 `MainSub05/06` 时间重叠时，会先出现一个 `rule boss` 的**短 boss 章**
> （≈260 帧 / 4.3 秒），随后 `MainSub05` 开第 5 章 ⇒ 道中实际显示 **7 个 MainSub 章 + 1 个短 boss 章**。
> 关底 boss（`MainBoss`）不受影响，仍按非符 / 符卡切章。**代码无需改动**（现状即 B）。

## 1v-补18. 鬼形兽（117）二十三/二十五修补：死因 kind 解码 + 出场防重叠（2026-09-13，待实机）

> 对应 `AUDIT.md` §AL55ter / §AL57。

- **`kind=OTHER` 根因**：thcrap `regs->retaddr` 是断点续跑 EIP，不是调用点返回地址；改用 `[regs->esp]`
  后实测读到 `0x430245`（断点+5 蹦床地址）也不是。`native/sdk.c` 新 `edeath_call_site()` 从
  `regs->esp` 起**扫前 12 个 dword**，命中原七条 call-site 表（KILLED `0x42f3b7` / BOMB_KILL
  `0x42ee90` / killAll 三兄弟 / ECL_DIE `0x430f59` / 标志驱动 OTHER `0x42f3dd`）。事件追加
  `retaddr / esp / ret_slot` 字段（仅 DLL 内 ABI，已同步 public.h）。
- **诊断覆盖**：前 16 只任意死亡 + **额外前 4 只 `is_big`（非 Cir/ECir 族）**各一行；
  行内增加 `kind/retaddr/ret_slot/esp/raw_flags/big`。下一份日志应能直接看到三只 `GirlD01` 的
  `sub/retaddr/ret_slot/kind/hp/hurtbox/anm_main`。
- **兜底**：若扫描未命中（`retaddr=0, ret_slot=-1`）而 `real_bit=1 + hp<=0 + 非 Cir/ECir`
  ⇒ 卡侧按 KILLED 处理并打 `death kind fallback -> KILLED`；已知 `KILL_ALL/ECL_DIE` 不走这条。
- **防重叠**：`token_spawn_heading()` 按 TH17 一手 `0x410570` 做距离²<576 且点积≥0.96 时
  `+3.883222 rad`、最多 10 次；初速仍四对角线（用户观感 🟡），重试允许离开对角线。
  中弹/真死 2 只强制一左（135°）一右（45°）；日志 `token spawn ... retry N` 与
  `miss -> 2 cycling animals (..., dir L135/R45)`。
- **实机判据**：1 面前半段 58 只 `Cir*` 0 行；`MainSub03` 三只 `GirlD01` 各 1 行
  `big-enemy drop`；`GirlB01/GirlC01/GirlD02/GirlRedA01/GirlBlueA01/A01n` 各 1 行。
  中弹/真死应见方向日志一左一右且无重叠；中 Boss 仍按 §AL56 的 `midboss drop` 口径。

## 1v-补19. 鬼形兽（117）二十六修：道中掉落改为「贴图体型 + 同波鹤立鸡群」（2026-09-13，待实机）

> 对应 `AUDIT.md` §AL58；原 AL55 的族名黑名单主判据已退役，死因解码与 fallback 继续用。

- **体型阈值**：`ANIMAL_BIG_SPRITE_MIN_W/H = 64`。来源 = 用户给的
  `enemy.png` + `thanm -l 18 enemy.anm`：右下角最大 sprite 类 `sprite96..107` 全部 64×64，
  `script40..44` 引用它们；ECL `GirlD01/GirlD02` 写 `anmSetMain(0,40)`。
- **运行时取尺寸**：slot0 VM id = `enemy+0x1350`（`zEnemyData+0x124`）→ `ce_anm_get_vm()` →
  `vm+0x74/+0x78` 原始 sprite W/H；退路 = `enemy+0x6204` 的 `final_sprite_size`。
  `anm_main`（`enemy+0x149c`）是脚本号不是 sprite id；Cir 不写 anm_main，必须读 slot0。
- **波键/统计**：关卡脚本主上下文 MainSub 数字（`MainSub01/01b/01c` 同键）。
  组 = `(行为体名, anm_main 外观脚本, W×H)`；按 MainSub 波维护 **census**：
  `on_tick_2` 每帧 + `on_enemy_death` 死亡当帧把当前可计入敌人按 `enemy_id` 去重记入本波，
  波键变化整表清零。判定用 census 累积值，不是死亡当帧同屏数。
  - 累积数量 ≤ `ANIMAL_ODD_GROUP_MAX(3)` ⇒ 全组掉（用户「<=3 也可以叫鹤立鸡群」）；
  - 累积数量 >3 且存在严格过半多数派、本组不是它 ⇒ 少数派掉；
  - 纯杂鱼波 `Cir`/`ECir` singleton guard 防首只误掉；非 Cir 不再整片放行。
- **1 面预期**：`GirlD01`×3 / `GirlD02`×3 / `GirlC01` / Red+Blue 对 / Redn+Bluen 对 ⇒ 掉；
  `GirlB01`×8 / `Cir00p/m` 10/10 / `Cir00bp/m` 10/10 / `Cir01a/Cir02a` 9/9 ⇒ 不掉。
- **日志**：`death diag` 增列 `sprite=WxH big_by_size=… wave=… odd=… drop=… census=n total=m`；
  掉落行 `reason=size` 或 `reason=odd-one-out`，带 `census/total`。
- **待实机重点**：
  1. `census` 表 256 项、按实际见到的 `enemy_id` 累积；实机核对 `census=n total=m` 与波内预期刷出数
     是否对得上（未见过就离场的成员会漏计）。
  2. `MainSub01/01b` 的 `$A=8` 循环波：census 按运行时实际成员数累积；核对 `reason=odd-one-out` 是否
     与 `census` 一致，而不是看同屏瞬间。
  3. 纯杂鱼波 singleton guard 是否挡住首只误掉；看 `Cir*` 是否出现 `odd=1 reason=odd-one-out`。
- **未改**：ECL 文件、`sdk.c` / `sites.py` / `.def`（无新 `BP_*`）；`ce_integrity_check()` 保留；
  Boss/中 Boss 既有掉落去重不变。

## 1v-补20. 鬼形兽（117）二十六修补：整波否决（用户 2026-09-13 更正版，待实机）

> 对应 `AUDIT.md` §AL58b。用户用自己看到的波内容定案：第二波那只上半贴图妖精掉、第三波整波不掉、
> 第四波三只大蝴蝶都掉、道中 boss 后第二波所有大蝴蝶都掉。

- **用户波名 → MainSub**：第一波 `MainSub00`；第二波 `MainSub01(+01b+01c)`；第三波
  `MainSub02(+02b)`；第四波 `MainSub03`；第五波 `MainSub04`；道中 boss 后第一波 `MainSub05`；
  道中 boss 后第二波 `MainSub06(+06b)`。
- **最终规则**：
  - 第二波：`GirlC01`（48×48、census 1）掉；红蓝队长按 census/odd。
  - 第三波：整波 veto（`ANIMAL_WAVE_VETO_THIRD`、`ANIMAL_THIRD_WAVE_KEY=2`）⇒ 全不掉。
  - 第四波：`GirlD01`×3 64×64 都掉（size）。
  - 道中 boss 后第二波：`GirlD02`×3 64×64 都掉（size）；`GirlB01`×8 不掉。
- **last-wave veto 已作废**：代码删除了 `ANIMAL_WAVE_VETO_LAST` 和自动最大 MainSub 编号查找；
  不再打 `wave veto = MainSub<max>`。日志只保留 `veto=third|none`。
- **日志**：`death diag`/掉落行带 `veto=third|none`、`census=/total=`。
- **实机重点**：第二波红蓝队长在 `MainSub01/01b` 的 `$A=8` 循环下 census 分布是否符合预期；
  第三波整波 `veto=third` 应无任何掉落行；第四波/道中 boss 后第二波大蝴蝶应 `reason=size` 掉。

## 1v-补21. 鬼形兽（117）二十七修：静态组表取代运行时 census（AL58c，2026-09-13，待实机）

> 实机三处误掉（第二波第一只杂鱼、道中 boss 前随机、道中 boss 后 `GirlB01`）根因 = 运行时
> census 首杀只有 1~2，命中 `≤3 odd`。改用“这一波这个组本来总共有几只”的静态表。

- **生成器**：`native/tools/gen_animal_wave_table.py` → `native/cards/animal_wave_table.h`。
  从 `st01..st07.ecl` 反编译解析 `MainSubNN`，**展开 `$A--`/`deci`+`jmpNeq` 循环**；
  用 `thanm -l 18 enemy.anm` 映射 `anmSetMain/anmSetSprite(0,S)` → sprite W×H。
- **运行时**：`animal_enemy_drop_measure()` 优先查静态表取 `same/total/max`；表未命中才退 census 兜底。
  `census=` 降级为日志；`group_total=` 是判据值。`veto=third` 保留；`last-wave` veto 已删/作废。
- **1 面动态数**：wave0=20；wave1=红16+蓝16+GirlC1；wave2=GirlB8+红n12+蓝n12；wave3=GirlD3；
  wave4=9+18；wave5=20+20；wave6=GirlD2+GirlB8。
- **最终验收**：只有第二波 `GirlC01`×1、第四波 `GirlD01`×3、道中 boss 后第二波 `GirlD02`×3 掉；
  `GirlB01`、`Cir*` 全程 0 行。
- **日志**：`death diag`/掉落行 `veto=third|none`、`group_total=`、`census=`、`total=`。
- **待实机**：看 `group_total` 是否命中静态表（`-1` = 未命中退回 census）；第三波应无任何掉落行。

## 1v-补22. 鬼形兽（117）二十八修：波号绑定到敌机（AL58d，用户 2 面实机反馈）

- 现象：2 面第二波 3 只 `anm_main 214`/48×48 中妖精没掉；日志出现 `wave=-1`，静态表与 census
  双失效。
- 原因：`animal_stage_mainsub()` 实时读在主上下文不是 `MainSubNN` 时返回 -1。
- 修法：新增 `enemy_id → wave` 绑定表；每帧首次见到敌机时绑定，sticky last wave 兜底，表满环淘汰。
  死亡优先用绑定波号；census 也按绑定波号分组；日志加 `wave_src=bound|live|sticky|none`。
- 验收：2 面 `wave 1` 三只 48×48 中妖精各掉 1；2 面 `wave 0` 相对不掉；1 面验收不变
  （GirlC01×1、GirlD01×3、GirlD02×3）。

## 1v-补23. 鬼形兽（117）二十九修：咲夜/早苗兽灵咆哮（AL59，2026-09-13，待实机）

> 设计原文 / 一手素材 / 实现细节见 [`AUDIT.md`](AUDIT.md) §AL59；旋钮见 [`TUNING.md`](TUNING.md) §2.1b。
> 一句话：**替掉 pl02/pl03 的「安全回退」**，四套咆哮全部做实。

| 角色 | 兽灵 | 实现 | 目标 DPS | 一手 / 自定 |
| --- | --- | --- | ---: | --- |
| 咲夜 pl02 | WOLF | 数据路径：7 行仿灵梦，角度朝自机前方 `FOCUS_DY=320` 汇聚；pl02.anm 追加 script30 | 63.33 | 参数照灵梦一手，**汇聚角度自定** |
| 咲夜 pl02 | EAGLE | 卡侧：6 个摆动发弹点 `θ=0.6·sin(0.03t+φᵢ)` + 外射短束 | 30.00 | 全自定 |
| 早苗 pl03 | WOLF | 数据路径：5 行大蛇（`pl03.anm` script6 放大 ×2）`func tick=4` 90° 折线 | 40.00 | 机制一手，**数值/大小自定** |
| 早苗 pl03 | EAGLE | 数据路径：5 发扇形大青蛙（script8 放大 ×2）+ `hit4` 命中爆炸（script 内 `sprite40..43`） | 40.00 | 机制一手，**数值/大小自定** |

**实跑清单（一条一句、怎么一眼验证）**
1. 早苗 EAGLE：前方 5 只**更大的扇形青蛙**，命中时炸出 `pl03c.png` 的 128×128 四帧**早苗爆炸**（不是魔理沙碎片）。
   日志 `eagle body bullet created … dmg 24`（set 0x24..0x27）。
2. 早苗 WOLF：5 条**更粗的蛇**；敌人与蛇同高且横向错开时，蛇**拐 90° 水平冲**（只折一次、不追尾）。
3. 咲夜 WOLF：7 颗**灵梦同款大弹**从自机周围发出，朝正前方一点**汇聚**。
4. 咲夜 EAGLE：自机前方 6 个激光条根部**左右摆动**、每个点朝其 θ 方向射短束；松 Z 立即收。
   日志 `wba_c: sakuya eagle on (6 oscillating points, theta0=0.60 omega=0.030 R=96 … DPS target 30.0)`。
5. pl00/pl01 不变（灵梦 7 行平行大弹 + 5 发追尾；魔理沙 5 行 + 本体 5 连扇形粗束）。
6. 掉落 / 中 boss / 水獭光球 / 霊撃 / HUD / 消弹口径全不变。

**实跑要看的**
- 青蛙爆炸是否**真的用了早苗贴图**（`pl03c.png` 的 128×128，而非 `pl01` 的三颗碎片）——若看到自机图/碎片，说明
  pl03.anm 追加脚本或 sht 的 `anm` 指错了，回看 `check_player_anm_bounds.py`。
- 咲夜 WOLF 是否会「汇聚后发散」过头 —— 调 `SAKUYA_WOLF_FOCUS_DY`（TUNING §2.1b）。
- 咲夜 EAGLE 的摆动幅度/快慢/点数是否对味 —— 调 `THETA0` / `OMEGA` / `N`（改 `OMEGA` 必须同步 `OMEGA_SIN/COS`）。
- 早苗青蛙/蛇的 2× 大小是否过大 —— 改 `build_player_anm.py` 的 `APPEND_PLAN` 缩放 k，重跑 `make anm sht`。
- 对话中 / 松 Z / 咆哮结束，咲夜 EAGLE 的 6 个 VM 必须**立刻没有**（`sakuya eagle off …` 一行）。

## 1v-补24. 鬼形兽（117）三十修：1 面 GirlD01 回归 + AL59 实机五条（AL60，2026-09-13，待实机）

> 用户实机反馈（四个自机都测）后的回归 + 五条修改。细节/一手见 [`AUDIT.md`](AUDIT.md) §AL60。

- **#1（回归，最优先）1 面 GirlD01×3 不掉**：日志里根本没有 GirlD01 的 `death diag`（被「非 Cir 诊断
  要 drop=1 才打」吞了）。根因 = st01 `MainSub03` 是「spawn 3 只后立刻 return」的同步子程序 ⇒ 卡片 tick 时
  sub 名已回到 `MainFront` ⇒ `live=-1` ⇒ AL58d 的 sticky 兜底绑到上一波 2（= veto 波）⇒ `drop=0`。
  修法 = sticky 绑定用静态表修正到「sticky 之后第一个含 (type,w,h) 的波」；诊断放开门槛。
  **验收**：1 面仍只有 `GirlC01×1 + GirlD01×3 + GirlD02×3` 掉；2 面 `wave 1` 三只 48×48 中妖精仍掉。
  日志认：`wave bind sticky-fix: sticky=2 -> wave=3`（每关前 8 条）+ GirlD01 `death diag ... wave=3 drop=1`。
- **#2 咲夜大鹫 = 她的刀（紫）且贴身**：卡侧 VM 由激光条（script163）换成紫刀（ability.anm script233，
  源=原生 `pl02.png` sprite25 hue-shift）；`R` 96→16 px。θ 公式/出弹方向/节奏保留。
- **#3 咲夜狼 = 她的刀（红）**：`anm` 从借灵梦狼弹改成原生 `pl02.anm` script5（sprite24 红刀）；数值/汇聚不动。
- **#4 早苗大鹫频率 ×0.5、DPS 不变**：`rate 3→6`、`damage 24→48`；并把命中路径从 hit4 改成 **hit0**
  （默认 handler 仍 `interrupt(1)` 播爆炸，但**不再建 20 帧每帧多判的爆炸伤害源**）。真实 DPS = **40**。
- **#5 早苗狼出膛偏移参考灵梦狼**：`(0,0)/(±20,20)/(±40,30)`（灵梦 7 行的对称 5 个）。
- **#6 颜色**：狼=红（咲夜用原生红刀）；大鹫=紫（咲夜紫刀）。早苗狼/鹫的蛇/青蛙本体本轮**未染色**（🟡，见 TUNING）。

**实机判据（一眼验证）**
1. 1 面三只大蝴蝶**各掉 1**（`death diag ... sub=GirlD01 wave=3 drop=1`；`wave bind sticky-fix` 可见）。
   2 面 wave1 三只 48×48 中妖精仍掉；第三波（wave2）整波仍不掉。
2. 咲夜大鹫：自机前方**贴身处**出现 **6 把紫色刀**左右摆动（不再是激光条），各朝其 θ 外刺。
3. 咲夜狼：7 颗**红色刀**从周围发出、朝正前方一点汇聚。
4. 早苗大鹫：青蛙发弹明显变慢（rate 6），命中仍炸**早苗 `pl03c` 爆炸**，但伤害不再暴涨（≈40 DPS 量级）。
5. 早苗狼：5 条蛇的出膛位置 = 中心 + 内/中圈（和灵梦狼一样的一圈），90° 折线不变。
6. pl00/pl01 不变；掉落/中 boss/水獭/霭撃/HUD 口径不变；未改 ECL。

**待实机重点**
- GirlD01 的 `wave bind sticky-fix` 是否出现（应 `sticky=2 -> wave=3`），以及 `death diag` 的 `wave=3 drop=1`。
- 咲夜大鹫「紫刀」观感 / 贴身距离（`ANIMAL_SAKUYA_EAGLE_R`，TUNING §2.1b）。
- 早苗大鹫真实 DPS 是否与灵梦/魔理沙同量级（hit4→hit0 后不再多判）。
- 早苗狼/鹫的**颜色**目前只有咲夜侧做了紫/红；早苗侧标 🟡（蛇/青蛙本体未染色）。

**AL60 构建/部署**：`make check` rc=0；`make release` → `sync_modkit_dll.sh` ⇒ **四路径同 md5
`aabb2611b7f099606825eaecd34071ea`**；pl00/pl01 未动，pl02.anm 回退成原生。

## 1v-补25. 鬼形兽（117）三十一修：咲夜大鹫真发弹 + 实机五条（AL61，2026-09-13，待实机）

> 延续 AL60 实机反馈。细节/一手见 [`AUDIT.md`](AUDIT.md) §AL61。

- **#1 咲夜大鹫 = 真引擎自机弹**（不再激光式远程判定）：卡侧直接调
  `Player__shoot_one_bullet` `th18:0x45e930`（与 `ce_damage_rect` 同级，**无新 BP_*/site**），
  每点每 4 帧发一颗 `pl02.sht` EAGLE 组的刀弹（`anm=31` 紫刀 / `func_on_init=5` 定向 / `dmg=20` / `speed=18`）。
  `on_shot_set_index` 对咲夜 EAGLE 钉回空的 `EAGLE_0`（引擎不自动发）。
  **退出清理**：松 Z/咆哮结束/对话/删卡 都扫弹池把在飞的该组弹收掉（state=2+interrupt 1），
  日志 `sakuya eagle kill (path=…, in-flight_bullets=N)`。**不再有会在退出后冻结的卡侧 VM。**
- **#2 咲夜狼发暗的根因** = 原生 script5/7/9 的 `alpha(192)`（75%）。狼/鹫改用 `alpha(255)` 的副本
  （script30 红刀 / script31 紫刀）。
- **#3 早苗狼出膛点往前**：`(0,0)/(±20,-20)/(±40,-30)`（灵梦那组 +y 是下方；早苗要身前）。
- **#4 早苗也染**：script29 大青蛙注入 `color(204,0,255)`（紫）、script30 大蛇注入 `color(255,0,0)`（红）
  （乘法染色近似；爆炸被罩成紫/暗蓝，未另做纯紫爆炸副本 🟡）。

**AL61 构建/部署**：`make check` rc=0；`make release` + `sync_modkit_dll.sh` ⇒ 四路径同 md5
`78d8435f84c6ff1865ca967c103425e7`；pl00/pl01 未写。

**实机判据（一眼验证）**
1. 咲夜大鹫：自机前方贴身处出现摆动发弹点，**真刀弹一颗颗沿 θ 飞出去**（不是原地常驻的 220 长激光条）；
   打中敌人有正常自机弹命中/伤害；**按 X / 松 Z / 咆哮结束，在飞的刀弹全部消失**（日志 `kill(...in-flight_bullets=N)`）。
2. 咲夜狼：红刀不再发暗（alpha 255）。
3. 早苗狼：5 条蛇从**身前**（不是身后）发出，90° 折线不变、颜色偏红。
4. 早苗大鹫：大青蛙偏紫、命中爆炸仍可辨认（白→紫/青→暗蓝）。
5. pl00/pl01 不变；掉落/中boss/其它口径不变；未改 ECL。

**待实机重点**
- 真发弹是否 100% 稳定（`Player__shoot_one_bullet` 直接调用；若闪退，回退「卡侧飞行弹」近似方案）。
- 退出后弹是否真的一条不剩（看 `sakuya eagle kill` 的 `in-flight_bullets`）。
- 早苗狼偏移/颜色观感；早苗大鹫爆炸色（🟡 近似）。

## 1v-补26. 鬼形兽（117）三十二修：正牌改色 + 源图切出/回流 + 咲夜刀放大（AL62，2026-09-13，待实机）

> 细节/一手见 [`AUDIT.md`](AUDIT.md) §AL62。

- **正牌颜色（构建期改像素，不再 color() 乘法）**：早苗青蛙→纯紫、蛇→纯红、爆炸→单独一份纯紫；
  咲夜红刀=零售原样、紫刀=蓝刀色相旋转。实现在 `assets/player_art_src/player_art_lib.py`
  （`mono` = 目标色+原亮度；`hue` = 只换色相），图作为新 entry 嵌进 pl02/pl03.anm。
- **源图切出 + 回流**：Windows 侧 `D:\项目\幻想指南\18\art-src\`（13 张 PNG + README，原始尺寸）；
  仓库规范输入 = `assets/player_art_src/`；改完跑 `bash assets/sync_player_art_src.sh` 再 `make anm`；
  **缺图会自动用零售+默认色补，不红**。
- **咲夜刀放大 ×2**：`KNIFE_SCALE=2.0`（pl02 script30/31 注入 `scale(2,2)`）；命中框跟着放大
  （狼 128×32 / 鹫 96×16）。
- **早苗狼出膛点再往前**：`(0,0)/(±20,-28)/(±40,-42)`（具名旋钮 `SANAE_WOLF_FORWARD_1/2`）。

**实机判据**
1. 早苗大鹫：大青蛙**纯紫**、命中爆炸**纯紫**（不再有暗蓝角）；早苗狼：蛇**纯红**。
2. 咲夜狼/鹫：刀明显比 AL61 大一圈（×2），命中范围同步变大（狼 128×32 / 鹫 96×16）。
3. 早苗狼出膛点比 AL61 更靠前；90° 折线/伤害/rate 不变。
4. 改 `18/art-src/*.png` → `sync_player_art_src.sh` → `make anm` 后，改动应体现在弹上；
   删掉某张源图再 `make anm` 应回退到默认色（不报错）。
5. pl00/pl01 / 掉落 / 中boss / 其它咆哮口径不变；未改 ECL。

**待实机重点**：纯紫/纯红的观感；咲夜刀放大后 6 摆动点是否仍贴身、弹速/间距合理；源图回流是否生效。

**AL62 构建/部署**：`make check` rc=0；`make release` + `sync_modkit_dll.sh` ⇒ 四路径同 md5
`78d8435f84c6ff1865ca967c103425e7`（只改 anm/sht，DLL 与 AL61 同）；pl00/pl01 未写。

## 1v-补27. 鬼形兽（117）三十三修：源图改零售原图 + 用户图原样使用（AL63，2026-09-13）

> 用户：「我看到的都是已经乘 color 的图，我要原图切出来」。细节见 [`AUDIT.md`](AUDIT.md) §AL63。

- **`D:\项目\幻想指南\18\art-src\` 根目录 = 零售原图**（原样裁切、未调色、原始尺寸，12 张）；
  **`art-src/_auto_default/` = 构建缺图兜底的已上色默认图**（README 明说不是原图）。
- **构建语义**：`assets/player_art_src/<key>.png` 存在 ⇒ **原样使用，不再套任何 color/hue/mono**；
  不存在 ⇒ 用「零售原图 + 默认色」在**内存里**兜底（**不再回写**规范目录，用户不会再被"默认色固化成源图"绕进去）。
- **三步流程**：原图 → 你在 art-src 里上色 → `bash assets/sync_player_art_src.sh` → `cd native && make anm`。
  `sync` 只动根目录 `*.png`，`_auto_default/` 不同步；脚本会提醒「构建原样使用、没上色=没上色」。
- 没有「紫刀」原图：鹫的紫是缺图兜底把**蓝刀** hue→紫；要自定义就把蓝刀涂成紫。
- 缺图仍能出包（实测删光 `player_art_src/*.png` 后构建 rc=0）。

**实机判据**
1. 打开 `18\art-src\` 根目录的原图，应与游戏里一致（青蛙白/肤、蛇橙/绿、爆炸白/青、刀红/蓝）——不是紫/红。
2. 把某张原图随便涂一色 → `sync_player_art_src.sh` → `make anm`，弹上应**原样**出现你涂的色（构建不再自动调色）。
3. 不涂、直接 `make anm`（规范目录为空）：出包仍是紫/红兜底色（= `_auto_default/`）。
4. pl00/pl01 / 掉落 / 中boss / 其它口径不变；未改 ECL。

**待实机重点**：用户按三步改图后颜色是否 100% 原样生效；`_auto_default/` 与根目录不要混用。

**AL63 构建/部署**：`make check` rc=0；`make release` 写 **0 处改动**（默认输出与 AL62 逐字节同 ⇒ modkit 无需更新）；
`sync_modkit_dll.sh` ⇒ 四路径同 md5 `78d8435f84c6ff1865ca967c103425e7`（DLL 与 AL61 同）。

## 1v-补28. 鬼形兽（117）三十四修：早苗狼命中光斑接进源图流程（AL64，2026-09-13）

- 一手：光斑 = `pl03.anm` 原生 **script7**（单帧 `sprite25` 16×16，scale 0→3→0）+ 父蛇 VM 的 `rotateAuto` 继承。
- 新源图 **`sanae_wolf_glint_s25_f0.png`**（art-src 根 = 零售原图；`_auto_default/` = 兜底浅红 mono H0/S0.65）。
- pl03.anm 新增 **script31=PL03_GLINT**（sprite25→67），script30 的 `scriptNew(script7)→scriptNew(script31)`；
  entry16 扩成 32×16（66 青蛙闪光紫 / 67 狼光斑浅红）。
- 改图三步：`art-src/sanae_wolf_glint_s25_f0.png` 改 → `sync_player_art_src.sh` → `make anm`。

**实机判据**：早苗狼命中时那个小光斑仍正常出现/缩放（旋转由父蛇 VM 继承），默认是浅红；改源图后颜色/形状原样生效。

**AL64 构建/部署**：`make check` rc=0；`make release` 写 pl03.anm + files.js；`sync_modkit_dll.sh` ⇒
四路径同 md5 `78d8435f84c6ff1865ca967c103425e7`（DLL 与 AL61 同）。


## 1v-补29. 鬼形兽（117）三十五修：早苗狼对齐灵梦大鹫 / 早苗鹫降频 / 咲夜狼汇聚贴板底 / 咲夜鹫加正中直射（AL65，2026-09-13，待实机）

> 用户四条一句话口径；第 1 条中途订正为「对齐**灵梦大鹫**」（33.33），不是灵梦狼（63.33）。

- ① **早苗狼**：`SANAE_WOLF_DMG 32→20`、`RATE 4→3` ⇒ `5×20/3 = 33.33`（对齐灵梦 EAGLE、偏差 0%）；
  5 行 / 视觉 ×2 / `tick4` 90° 折线保留。**「蛇要不要加诱导」只报建议：不建议加** —— `tick4` 本身就是
  零售早苗蛇的「锁敌→水平冲刺」一次性诱导，再叠逐帧追踪会覆盖折线口径 + 要新 hook，等用户拍板。
- ② **早苗鹫**：`SANAE_EAGLE_DMG 48→96`、`RATE 6→12` ⇒ DPS 仍 40；hit0 / 纯紫四帧爆炸不变。
- ③ **咲夜狼**：`SAKUYA_WOLF_FOCUS_DY 320→−432`（一手：自机 y `[32,432]`、弹幕区顶 y=0 ⇒ 汇聚点 = 0−432）。
  用户希望「贴板底恰好聚焦到板头」；聚焦/低速不改本体 y，body 行 ⇒ 无像素差。
- ④ **咲夜鹫**：新增正中正前口（1 点、`(x, y−16)`、θ=正上、无摆动、每 2 帧一颗、dmg20 ⇒ **10 DPS**）；
  旧 6 点扫荡公式/视觉不动，只把 `_PERIOD 4→6`（dmg20）⇒ **20 DPS**；**总 30 不变**。

**实机判据**：① 早苗狼 DPS 与灵梦鹫同量级、90° 折线手感不变；② 早苗鹫明显变慢但总伤害不变；
③ 咲夜狼在板底时所有刀弹恰好收在板头一点（正中/最外行都收拢）、在高位时汇聚点在板外；
④ 咲夜鹫正中多出一条正向直射刀弹，6 点扫荡摆动/视觉完全没变形，整体总 DPS 不变。

**AL65 构建/部署**：`make check` rc=0；`MODKIT_DIR=… NO_GIT=1 make release` 写 4 处（DLL + pl02.sht + pl03.sht + files.js）；
`sync_modkit_dll.sh` ⇒ 四路径同 md5 `279ee8e5b17dc8e4fbdd493bbdca784b`；
sht md5：pl02 `87bd90b1b923636a254e43500328453f`、pl03 `6b8c19d5ced800566d4187ddc6ac2e12`。

## 1v-补30. 鬼形兽（117）三十六修：咲夜大鹫总 DPS 30 → 40（+1/3）（AL66，2026-09-13，待实机）

> 用户原话：「咲夜大鹫得加强，整体 dps 上升 1/3」。按用户建议的最省事整数方案实现。

- 扫荡 6 点：`ANIMAL_SAKUYA_EAGLE_PERIOD 6→4`（回到 AL64 节奏）、`dmg 20` 不变 ⇒ `6×20/4 = **30 DPS**`。
- 正中正面口：`_FRONT_PERIOD=2` / `dmg 20` ⇒ **10 DPS**（不动）。
- 合计 **40**（旧 30 ⇒ +1/3，全整数 0 误差）。摆动公式/视觉/命中框/真发弹路径/退出清理不动。
- 本轮只动 C 侧卡内计数 ⇒ **pl02.sht / pl03.sht 逐字节不变**；`animal.c` 与 `append_shooterset.py` 的镜像常量/注释同步。

**实机判据**：① 咲夜鹫扫荡火力回到 AL64 的密度（不再像 AL65 那么稀）；② 正中正面直射刀弹仍在正中央、无摆动；
③ 总伤害体感比 AL65 高 1/3，颜色/命中框/退出清理不变。

**AL66 构建/部署**：`make check` rc=0；`MODKIT_DIR=… NO_GIT=1 make release` 只写 **1 处**（`mods/th18_card_expand.dll`；sht 无 diff ⇒ `files.js` 未变）；
`sync_modkit_dll.sh` ⇒ 新 md5 `56ffd591c1331e6ce17e02a509a241e9`；4 条路径 **3 条已更新**，
`thcrap/bin/mod_th18_card_expand.dll` 因游戏运行被占用仍是 AL65 的 `279ee8e5b17dc8e4fbdd493bbdca784b`
⇒ **关闭游戏后重跑 `bash local/sync_modkit_dll.sh`**；
sht 仍与 AL65 同：pl02 `87bd90b1b923636a254e43500328453f`、pl03 `6b8c19d5ced800566d4187ddc6ac2e12`。


## 1v-补31. 鬼形兽（117）三十七修：咲夜大鹫分布改成扫荡 20 + 正面 20（AL67，2026-09-13，待实机）

> 用户原话：「换成 dps 分布扫荡 20+正面 20」。总 40 不变（AL66 的 +1/3 保留），只重新分配。

- 扫荡：`ANIMAL_SAKUYA_EAGLE_PERIOD 4→6`、dmg 20 ⇒ `6×20/6 = **20 DPS**`。
- 正面：`_FRONT_PERIOD=2` 不变，改走新的 sht **row 1（dmg 40）** ⇒ `40/2 = **20 DPS**`（选方案 (b)：
  弹流节奏与 AL65 相同、不增加弹数，只把每发伤害翻倍）。
- 合计 **20 + 20 = 40**。摆动/视觉/命中框/真发弹路径/退出清理/贴身 R 不动；`pl02.sht` 多一行（row1），`pl03.sht` 不变。
- 同批结案：早苗狼「一次性诱导 ≈ 完整诱导」用户接受（边界：被锁定怪中途死亡时，`tick4` 用锁定瞬间
  写进 `bullet+0xd4` 的快照 x 决定冲刺方向、不重读敌机 ⇒ 仍朝原方向冲）；**不加逐帧追踪**。

**实机判据**：① 咲夜鹫扫荡明显变稀（回到 AL65 的周期 6）；② 正中正面直射仍在正中央、无摆动，
但每发更重（dmg 40）；③ 总 DPS 与 AL66 相同（都是 40）；④ 其它口径不回归。

**AL67 构建/部署**：`make check` rc=0；`MODKIT_DIR=… NO_GIT=1 make release` 写 3 处（DLL + pl02.sht + files.js）；
`sync_modkit_dll.sh` ⇒ **4/4 路径同 md5 `9ba315a6a04dfb1cd95e03e37d0c1c87`**（AL66 被游戏占用的
`thcrap/bin` 本轮已一并更新）；
sht md5：pl02 `6ed0a1fe20bdb3c4249c9b5183c32bef`、pl03 `6b8c19d5ced800566d4187ddc6ac2e12`；
`files.js` `f135d1ab99c02fbd2caa3718e7c1c98c`。


## 1w. ★★★ SHT 容量扩容（用户 2026-09-13 拍板「96 先记下」）—— **步骤 1–4 已落地（2026-09-14）**，5–6 待做

**结果一句话**：`pl0X.sht` 的组偏移数组从 **40 格** 扩到「**预留 256 格 / 宣布可用 96 格**」，
exe 侧只改**一处 6 字节常量**（数据区基点 `0x180 → 0x4e0`），**15 处引用一个都没改**。
全部一手取证、改动清单、对抗审计与实跑判据见 [`AUDIT.md`](AUDIT.md) **§CQ**；容量台账口径见
[`CAPACITY.md`](CAPACITY.md) §3.3（现在是 ✅、余量 56 格）。

**为什么必须做**：零售数组 40 项，零售用掉 `0x00`–`0x16`、本 mod 又占满 `0x17`–`0x27`
（豌豆 2 + 116 季节 5 + 117 兽灵 10）⇒ **40/40、余量 0**。用户口径：「最终形态下不可能实际没人用」
+「th11 还得有魔理沙特技 5 种子机切换」⇒ 一次把容量做到 **96**（保守估计最终要 50–64 格）。

**落地方案（与本节最早那版不同，★ 重要）**：不是「把数组搬到文件尾、改 14 处引用」，
而是「**数组区原地长到预留格数、数据区跟着后移**」：

| | 数组搬文件尾（原计划）| **数据区基点后移（已落地）** |
| --- | --- | --- |
| exe 侧改动 | 15 处 binhack（14 读者 + 加载器）+ 断点表同步 | **1 处** 6 字节等长常量（`th18:0x45a830`）|
| 漏一个引用的后果 | **静默错位**（实跑才看得见，§CC/§AH 的失败模式）| 结构上不可能（没有读者被改指）|
| 为什么成立 | —— | 偏移项是**相对**数据区的 ⇒ 记录与基点同时后移，值与字节都不用改；读者位移仍是 `+0xe0` |

**⚠ 普查订正**：引用是 **15 处（14 读者 + 1 加载器）**，不是 14 —— 多出来的
`0x410609 mov edi,[eax+0x138]`（`0x138 = 0xe0 + 0x16*4`）是**折叠位移**形态（常量 set 号被编译进
disp），原有的「变址 + disp==0xe0」扫描整类漏掉。`reader_census.py` 已补该形态 + 两道新闸
（读者位移不许被改指 / 数据区常量只许有一处）。**这条漏项只对「搬数组」那条路致命**，本方案不受影响。

**★ 架构收益（回答「以后怎么不再撞容量」）**：数据区基点是硬编码立即数，所以让它**一次让够**
（预留 256 格）⇒ `96 → 256` 之间再加格子**只改文件**（抬 `CE_SHT_SLOTS` + 写槽位 + 接行数据），
**补丁与 DLL 里的常量都不用动**。⚠ 用 `0x60` 及以后的预留槽时必须同时抬计数：计数外的槽不会被
重定位（值仍是 0），谁读它谁拿到空指针。

**分配现状（落点 = 生成物 `native/sht_ids.h`，别再手抢）**

| 段 | 归属 |
| --- | --- |
| `0x00`–`0x09` | 原生主炮组（不可动） |
| `0x0a`–`0x15` | 原生 12 张装备卡的子机组（不可动） |
| `0x16` | 108 永夜抄妖侧 set（覆盖零售那一格） |
| `0x17` / `0x18` | 豌豆射手 普通 / 加特林 |
| `0x19`–`0x1d` | 116 季节 春/夏/秋/冬/土用 |
| `0x1e`–`0x27` | 117 兽灵 狼 0–4 / 鹫 0–4 |
| `0x28`–`0x5f` | **计数内空位 56 格**：113 灵界主炮（1，四角色共用）；111 地灵殿魔理沙特技 5 种子机切换；108 使魔；106/107/109/110/112/114/115 各自子系统；58–80 那批备用 |
| `0x60`–`0xff` | **预留区 160 格**：抬 `CE_SHT_SLOTS` 即可用，**不用改 exe** |

**已完成的 1–4（2026-09-14）**

| # | 落地物 | 门 |
| --- | --- | --- |
| 1 | `assets/sht/append_shooterset.py`：预留数组区 + 数据后移 + 三条新不变式；四份 `pl0X.sht` 重生成 | `make sht` 回读校验（计数 / 恒等式 / 逐字节不变 / 预留槽全 0）|
| 2 | `native/sites.py`：1 条等长 binhack（`8d8280010000` → `8d82e0040000`）+ 专用对账分支 | `sites.py verify` 211 条全对 |
| 3 | `native/sht_ids.h`：`CE_SHT_SLOTS` / `CE_SHT_SLOTS_RESERVED` / `CE_SHT_DATA_BASE` / `CE_SHT_FREE_*` + 分配表 | `capcheck` 三方对账（`sht_ids.h` ↔ patch ↔ 恒等式）|
| 4 | `native/sdk.c` 门里**回读**该 binhack（imm32 ≠ `CE_SHT_DATA_BASE` ⇒ FAIL + 禁用本局新卡）；`reader_census.py` 两道闸 | `make check` 全绿 |

**待做**

5. ✅ **113 收尾（2026-09-15 已做，待发布）**：永久专属槽 `CE_SHT_SET_TRANCE = 0x28` =
   生成器**正常追加组**；`trance.c` 的借格/还原/4 格账本/满账本退让整段删掉（净减约 60 行），
   `on_shot_set_index` 只返回槽号；另加**只读**体检（不合法 ⇒ 退回原生主炮 + 一行日志）。
   见 `AUDIT` §CX。⚠ 发布被挡：`cards/faith.c` 当前编不过（另一位 agent 在改）⇒ 等树恢复后整批发。
6. ✅ **111 的 5 组**：已落在 `0x29`–`0x2d`（`MARISA_FORMATION_*`，由那位 agent 落地），
   生成器里有断言钉住起点（`MARISA_FORMATION_SET_BASE`）。
7. **回归**：四角色各打一关 + 全部装备卡子机 + 117 兽灵 + 豌豆/季节 + 关卡重开/读档。

**★ 另一道更近的墙（2026-09-14 普查）：`abcard.anm` 的 256 entry** —— 现 164，**只能再进 46 张卡**，
比 `CE_CARD_MAX_NEW`(71) 更紧。这道门 **2026-09-14 已补上**（`build_abcard.py` 的 `MAX_ENTRIES`），
见 `CAPACITY.md` §5-①。

**顺序**：① 用户先把 113 这轮（遮罩 / 进度条 / 主炮组 / 闪退修复）**验完**；② 1–4 已完成并已发布；
③ 再做 5（113 收尾）与 6（111）。—— 一次只动一个变量，是本仓的既定纪律。

---

## 1x. 灵界 BGM **素材配对订正**（2026-09-15，用户实跑「道中播放的是关底的曲子」）

**一句话**：`th18_NN` 的**内部编号有洞**（fmt 里没有 `th18_02`），与音乐室/同人 MIDI 的**连续编号**
在 `th18_14` 之后错开（`th18_17`=No.14、`th18_18`=No.15、`th18_15`=No.16、`th18_16`=No.17）
⇒ 我按"编号相等"取 MIDI，把 1 面道中的 `th18_03` 配成了 `3. 大吉キトゥン`（= 1 面 boss）。详见
[`AUDIT.md`](AUDIT.md) **§CU**。

- **一手对照表 = `dat/musiccmt.txt`**（游戏自带，`@bgm/th18_NN` + `No. N` + 注释写用途）；
  `tools/make_trance_rearr.py` 现在**直接解析它**（此前是 `index+1` 推断，对 Ex/结局/Staff 也是错的）。
- 已重生成 **17 首** `_rearr`（`local/th18.v1.00a/trance_bgm/`，gitignored），逐首用 DLL 自己的
  WAV 解析器回读（`rc=0`、长度 == fmt 的循环体）；`sync_modkit_dll.sh` 已同步 **35 wav × 3 目录，0 不同**。
- 本轮**零 C 改动**（DLL md5 未变）⇒ 唯一变量 = 素材内容。

**★ 2026-09-15 追加（一次部署事故，见 AUDIT §CV）**：同一天为了推素材跑了一次
`local/sync_modkit_dll.sh`，它把 `native/` 里**另一位 agent 为 111 构建、还没发布**的 DLL
一起推到了游戏 —— 那份 DLL 期望的灵界行偏移是 `0x33c0`（111 的五组把孤儿段顶了 `+0x1E0`），
而游戏里的 `pl0X.sht` 还是发布批（`0x31e0`）⇒ 行指针越界 ⇒ 一进灵界 `call eax`(eax=1) 崩。
**已回滚到发布批**（4 路径 md5 `63d827a9…`），并给同步脚本加了配套性闸
`tools/check_deploy_pair.py`。⇒ 110/111 的收尾**必须整批 `make release`**（DLL + `pl0X.sht` + `files.js`），
不要单独推 DLL。

**★ 2026-09-15 第二轮：时间对齐（AUDIT §CW）** —— 用户「配对没问题，还是时间对不上」。
根因两条：① 生成器的 `--corr-align` 写法**搜不到正确延迟**（锚点错了）；② 更关键 ——
「先 poke 游标再 `play`」是**无效顺序**（`play` 内部 `reset(rewind=0)` 会把 `cur` 抹回 `base`），
所以 seek **从来没落地**（日志里只是"算过"）。现在：`fine_align`（chroma 判据、±0.8 s）+ 顺序改为
**`play` → poke → `bgm_refill`×8**（§8.6 有一手全文），暂停恢复同样重新对齐。已整批发布
（DLL md5 `891f241e…`，配套闸 ✓）。看日志：`起播位置 = …` 与 `[refill #N 后] cur−base ≈ so+16 s`。

**待做（下一轮，单独一个变量）**

1. ✅ **DLL 侧防御（§CV-5）已随 §CX 落地**：`trance.c` 的 `trance_sht_group_probe()/ok()` 每帧**只读**体检
   （槽偏移项非 0 / 落在影像内 / 首行 4 个 func 全 0），不合法就返回 −1（原生主炮）+ 一行
   `shot group UNAVAILABLE`。现在最坏情况是"主炮没换"，不再有"越界读崩"。
2. **`trance: load — selfcheck` 那行 `ce_log` 的格式串/参数不匹配**（日志里数字串位，例
   `spirit life 40 f + script tail 520 f = 200 f`）—— 属 C 代码，不与纯素材轮混跑。
2. 113 → 永久槽 `0x28`（§1w 第 5 步）、111 → `0x29`–`0x2d`（第 6 步）。

## 1x. 110/111 信仰点「生成语义」收口 —— 用户 2026-09-15 定案，**✅ 已实装**（2026-09-15）

> **状态回填**：六条（A–E，E 拆成 E-1/E-2）**全部落地**，门全绿（`make test-host` /
> `make check` / `check-docs.py` / `make dllx87` = 0 条 x87）。逐条的「改前 → 改后 → 一手出处 →
> 复刻/自定」登记在 [`TUNING.md`](TUNING.md) **§2.5c**；下面**只回填本轮新查到的一手**与
> 与原计划的出入（原计划的文字**不删**，作为当时的判断留痕）。
>
> ★★ **B 条一手已查到**（原计划把它列为开工阻塞项）：**`th10:0x41ba50`** ——
> `ItemManager::on_tick__body` 尾部 `0x41b889` 调它，条件是 P 收集分支 `0x41b44b`（type 1）/
> `0x41b6b3`（type 4）置的「刚满」标志 `[esp+0x14]`。体：150 槽循环（`edi = mgr+0x14`、
> 步长 `0x3f0`），`[item+0x3dc] != 0`（在用）且 `[item+0x3e0] ∈ {1,4}` ⇒ 清在用位 +
> `spawn_item(type = 9, pos = **道具自己的位置** (ecx = `item+0x3ac`), `0xffffffff`,
> angle = `0xbfc90fdb` = **−π/2**, speed = `0x400ccccd` = **2.2**)`，逐颗另播一个音
> （`0x448db0` 参 `0x189`）。`type 10/11 → 5` 那两支是**死代码**（前面的类型过滤已挡住）。
> **TH11 同款 = `th11:0x4240f0`**：同形（stride `0x478`、150 槽），`[esi] ∈ {1,4,10,11}` ⇒
> 一律 `type 9`、`pos = esi−0x34`（道具自己的位置）、角 `th11:0x49842c` = −π/2、
> 速 `th11:0x498430` = **2.2**。⇒ **B 是复刻**（不是 ❓ 自定），常量就是「角 −π/2、速 2.2、原位」。
>
> 与原计划的**三处出入**（以一手为准）：
> 1. **E-2 的速度不是 2.2**：`th10:0x4081e7`（消弹撒 type 8）的实参是 `0x3f19999a` = **0.6**；
>    2.2 是**击破 / 满 P 转换族**的值（`0x400ccccd`）。代码里按档分开（`faith_core_throw_speed()`）。
> 2. **A 的落点不是「改 type」**：TH18 **没有信仰点 type**（它的 type 1/3 是 P、9–0xe 是引擎
>    自己的「分数转换点」）⇒ 把 type 改成 9 只会变成另一种原生道具，不是绿点。⇒ 按一手的
>    **不变式**实现（「满 P 期间场上没有 P 类道具」）：共用件每帧扫原生道具池，满 P 时把 P 类道具
>    **原位删掉**（照引擎自己的清尾四步）+ 在**它自己的位置**撒 type 9 档的自建点。
>    A（出生即替换）与 B（满的那帧全场转换）由**同一条规则**同时满足。
> 3. **转换的 SE 没播**（一手逐颗播；编号是两作各自 SE 表的，TH18 无同一份音源）⇒ ❓ 自定，
>    登记 `TUNING.md` §2.5c #9。

用户原话（拆成五条）与已查到的现状/依据：

| # | 用户要求（一句话）| 现状一句话 | 一手依据（详证见下）|
| --- | --- | --- | --- |
| A | 满 P 时**替换** P 点，不是加撒 | 现在是「吃到时额外加撒」 | `th10:0x41bbf8` 跳表 = 改写道具 type |
| B | 满 P 那刻**全场存量 P 点原位重生为绿点 + 上抛动作** | 完全没做 | ⚠ 待查（见下「B 待补一手」）|
| C | **删掉出生保护** | `faith.h:35` `FAITH_INTANGIBLE 32` 挡住吸/收 | TH18 原生 P/金钱无保护 |
| D | 术语统一（同一 type 两名）| 110 叫大/小/微、111 叫大/中/小 | 两作同一套 type：3/9/8 |
| E | 「生成语义一致吗」| 我们「随机散」| 原版是**固定角 −π/2 + 速度 2.2**、撒在弹位置 |

### A. 满 P 替换（一手）

TH10 是**改写而不是加撒**：`th10:0x41bbf8` 的跳表（`0x41be1c` + 索引表 `0x41be28`）=
**type 1/4 → 9、type 10/11 → 5**，改的是**道具自己的 type** ⇒ 那份 P 点**不再存在**，只出信仰点。
现状落点：`mof.c:714–744` 的 `mof_core_full_p_item()` 是「吃到时额外撒一颗小信仰点」。

### B 待补一手（这是开工前的阻塞项）—— ★ **已补上（见本节开头的状态回填）**

「满 P 那一刻全场转换 + 向上抛」这条链**本仓还没有地址**。候选落点：`Globals::collect_power`
返回「跨档 / 满了」那条分支的调用者（`local/_scratch/research-th10-faith-and-graze.md` §3.5）。
**没有它就只能标 `❓ 自定`** —— 用户明确说这条「最好复刻」。
⇒ 2026-09-15 已查到：**`th10:0x41ba50`**（触发 = collect 侧 `0x41b44b`/`0x41b6b3` 置的
`[esp+0x14]`）与 **`th11:0x4240f0`**；常量 = 角 −π/2、速 2.2、**道具自己的位置**。**是复刻。**

### C. 删出生保护（一手 + 实测）

`faith.h:35` 的 `FAITH_INTANGIBLE 32` 卡住三条路：`faith.c:438`（过回收线→state 3）、
`:495`（收集判定）、`:462`/`:513`（飘落与减速）⇒ 我们自建的点**出生 32 帧内既不被吸也不被收**，
**连全屏吸收（`ce_item_collect_all()`）也吸不走刚撒的点**。
一手：TH18 原生 **P 点 / 金钱无出生保护** —— `th18:0x446981` 的 `cmp CURRENT_POWER,MAX_POWER`
只拦**「加火力」**那一支，**不拦掉落**（用户 2026-09-15 实测：满 P 仍然掉 P 点）⇒ 线上生成即被吃。

### D. 术语（两作同一套 type，名字不同）

| 道具 type | TH10《风神录》| TH11《地灵殿》|
| --- | --- | --- |
| 3 | **大**信仰点 `+5000/8000/10000`（按难度）| **大**绿点 `+100` |
| 9 | **小**信仰点 `+100` | **中**绿点 `+10` |
| 8 | **微**信仰点 `+10` | **小**绿点 `+0.1`（每 100 颗 `+10`）|

⇒ 「110 的小信仰点」与「111 的中绿点」是**同一个槽（type 9）**，只是名字与量纲不同。
建议 `faith_core.h` 改成**「type 编号 + 各作别名」双列注释**。

### E-1. ★ 「生成语义」= **哪一档由哪个来源生成** —— 两作**一致**（用户 2026-09-15 补充口径）

用户口径：「两边**中**都是满 P 转换、**微**都是消弹」—— 即**档位 ↔ 来源的对应关系在两作是同一套**：

| 档（道具 type）| 来源 | TH10 落点（一手）| TH11 落点（一手）|
| --- | --- | --- | --- |
| **大**（3）| **击破掉落**（大信仰点 / 大绿点）| 大信仰点难度表 `th10:0x41b8cc` = {5000,5000,8000,10000,10000} | 绿点三档之一（`th11:0x423c82` 系，显示 +100）|
| **中 / 小**（9）| ★ **满 P 转换**（P 点 → 信仰点）| 跳表 `th10:0x41bbf8`：**type 1/4 → 9** | 表 `th11:0x424578` = `[0,1,1,0,1,1,1,1,1,0,0]` ⇒ **改写为 type 9** |
| **微 / 小**（8）| ★ **消弹**（每颗被消的弹）| `th10:0x4081e7` `push 8` → `spawn_item`（角 −π/2、速 **0.6** = `0x3f19999a`）| `th11:0x40b0a2` 写死 `mov ecx,0x8` → `call 0x424230` |

⇒ **结论：档位与来源的映射两作相同**（大 = 击破、中/小 = 满 P 转换、微/小 = 消弹），
这也正是共用池「**一套实体 + 各自数值表**」成立的前提。⇒ **本卡应当保住这条对应关系**：
满 P 转换只能产出 type 9 那一档、消弹只能产出 type 8 那一档（**不许串档**）。

### E-2. 「生成的**运动学**」不一致（待改）

`faith.c` 头注自述「角度与速度**由调用方给（随机散得更自然）**」；原版撒点用**固定角 −π/2 + 定速**，
且撒在**实体自己的位置**。⇒ 已按一手对齐（`faith_core.h` §E-2）：
   · **击破 / 满 P 转换族**（大 / 中）：`th10:0x41bac1` 等的实参 `0xbfc90fdb` / `0x400ccccd` = −π/2 / **2.2**
     （TH11 侧 `0x49842c` / `0x498430` 同值；TH18 自己的 main-drop `0x430481` 同参）；
   · **消弹族**（微）：`th10:0x4081d9` = −π/2 / **0.6**（`0x3f19999a`）—— ★ 原计划写的「2.2」是**错的**
     （那是转换/掉落族的值），以一手 0.6 为准。
⚠ 「撒在**弹自己的位置**」这条微档**还差一手**：SDK 的 `ce_cancel_radius()` 只给颗数、给不了坐标
（可拿到位置的只有道具侧 —— 道具/转换通道已经是**原位**）。登记 `TUNING.md` §2.5c #10。

**实跑验收（用户按此判）**
1. 满 P 时打怪：**掉下来的应该是绿点（信仰点），不是 P 点**；且**场上原有的 P 点在满 P 那一刻消失、原地变绿点并向上抛一下**。
2. 站在回收线上撒点/消弹：**新点当帧就被吸走**（不再有 32 帧窗口）。
3. 触发全屏吸收（满信号 / 萃香特技）时，**刚生成的点也一起被吸走**。

**开工时要一并跑的**：`make test-host`（`test_mof_core` / `test_faith_core` / `test_signal_core`）、`make check`、`check-docs.py`；改 `FAITH_INTANGIBLE` 与满 P 判据都在**共用件**里 ⇒ 报告里单开「对既有卡影响面」（110 与 111 都吃这套池）。

**开工前必须先补的一手**：B 那条的地址（满 P 全场转换 + 上抛动作）—— ✅ **已补**：
`th10:0x41ba50`（trigger = `0x41b44b`/`0x41b6b3` 的 `[esp+0x14]`）+ `th11:0x4240f0`；
常量 = type 9、角 −π/2、速 2.2、**原位**（见本节开头的状态回填）。

**实装结果（2026-09-15）**：见本节开头的状态回填 + `TUNING.md` §2.5c。

### §1x 实跑回填（用户 2026-09-15：「似乎，没有问题」）

用户实跑后报「似乎没有问题」⇒ **本轮的最高风险路径已在真机验证**：
`faith_convert_full_p_items()` 对**引擎原生道具池**做的「原位删除」四步
（`state=0` → `ce_anm_delete([item+0xc28])` → id 清 0 → **freelist 前插**）**没有把道具池写坏**
—— 反证判据原本是「打怪不再掉任何道具」（槽回不到分配链），**未出现**。

⇒ 风险等级从「未实跑 / 全轮最高」降为「✅ 实跑通过（2026-09-15）」。
仍需下一轮实跑确认的三条（写在 `TUNING.md` §2.5c 末尾）：
① 满 P 时掉的是**绿点**、场上有 P 点时**当帧被转换**（600 帧行的 `conv N` 颗颗不落）；
② 站在回收线上撒点/消弹 ⇒ **当帧被吸走**（不再有 32 帧窗口）；
③ 触发全屏吸收时，**刚生成的自建点与原生道具一起被吸走**。
（用户本轮只报了「没问题」的总体结论，这三条的逐条判据还没有逐项对账，留待下一轮。）

### §1x.2 ★ 与 114（辉针城）的联动：**大/中绿点要计入 114 的收卡计数**（用户 2026-09-15，**✅ 已实装** 2026-09-15）

> **状态回填（2026-09-15，本轮实装）**：两条通道**全部落地**，门全绿（`make test-host` 全绿 / `make dllx87` = **0 条 x87** / `check-docs.py` 全绿；`make check` 红在**别人的既有项** —— `capcheck` 的 `CE_ANM_ABILITY_ATLAS_ATLAS_TH13ASCII` 未登记进 `CAPACITY.md`）。下面原文**不删**，作为当时的判断留痕；**以本节开头的回填为准**。
>
> | # | 要做 | 落点（文件）| 一句话 |
> | --- | --- | --- | --- |
> | ① | **成员通道**：大 / 中被收 ⇒ 批计数 +1 | `cards/faith.c` `dot_collect()` → `ce_vitem_note_collect()`（SDK 层）→ `cards/collect2x.c` `on_item_frame` 抽干 → `c2x_collect_virtual()`（core）| **只 +1**：不碰 `batch_score` ⇒ 结算补差对纯绿点批恒 **0**；档位 / 倍率 / 碎片 / 4:1 循环**一个字没改** |
> | ② | **presence 通道**：在飞期间续命 | `cards/faith.c` `faith_pool_tick()` 尾 → `ce_vitem_declare_flying()` → `collect2x.c` 的 `ce_item_pool_any_attracting() \|\| ce_vitem_flying() > 0` | 只**续**已开的批、**不**开批（不会凭空冒 NO BONUS）；批不再在绿点还在飞时提前结算 |
> | ③ | 微档排除（§1x.3）| `cards/faith.c` `faith_vitem_kind()` → `CE_VITEM_HALF`（层内两条通道都拒）| **双保险**：微档今天根本不建实体（`faith_spawn` 对微档返回 0），将来补实体也天然不入批 |
> | ④ | 声明式（不是卡间直连）| `native/sdk_core.{c,h}` **末尾追加**「虚拟收集 / 虚拟在飞」层（`ce_vitem_*` 八个入口 + `ce_vitem_kind_t`）| 「114 不在场 / 被删卡 = **零副作用**」由**自过期**保证（容差窗 2 帧、不写引擎内存、不进 `ce_sdk_setup` 复位表 ⇒ `sdk.c` / `sites.py` / `.def` / patch **零变化**）|
> | ⑤ | 复用意图（116 季节点 / 117 动物灵）| 同上（`sdk_core.h` 长注写明）| 它们接 114 = **各加两行声明**（收点处 `ce_vitem_note_collect`、池 tick 尾 `ce_vitem_declare_flying(自己的 card_id, …)`），**不必认识 114**；源号 `0`（`CE_VITEM_SRC_SHARED`）已被绿点池占用 |
>
> **单测**（`make test-host`）：新增 `tests/test_vitem_batch.c`（**接线级**，370 断言 —— 必须**同时**链 `sdk_core.c` 与 `cards/collect2x_core.c`，已加进 `native/Makefile` 的 `test-host`）+ `test_sdk_core.c` 的 `test_vitem_channel`（1023 断言）+ `test_collect2x_core.c` 的自建实体组（1003 断言）。钉住：**只 +1 / 不进 SCORE**、**微档不入批**、**在飞期间 presence 为真、收完为假**、**114 不在场零副作用**、**帧内顺序无关**。
>
> **取舍（为什么声明式）**：见 `AUDIT.md` §DA-2 的对照表；一句话 =「114 不在场 / 被删卡」这两种情形由**层的自过期**免费解决，而**下一对耦合**（116 / 117 同样要入批）不必再写一遍。
>
> **实跑待验的三条**（判据全文见 `AUDIT.md` §DA-8 / `TUNING.md` §2.5e）：① settle 帧多一行 `collect2x: batch had N virtual (green-dot) members of M, virtual-presence=0|1`；② 只靠绿点续命的那一帧多一行 `collect2x: batch held by virtual items in flight (K flying, count M)`；③ **没有 110/111 时** collect2x 行为与日志逐字节不变（三条新日志一条都不出）。⚠ **反例**：若「没做全屏收点却弹灰 NO BONUS」⇒ 翻 `cards/faith.c` 的 `FAITH_VITEM_NOTE_VACUUM_ONLY`（默认 0 = 用户口径字面；1 = 只报真空飞行中被收的，与引擎 §Z22 同口径），**别动** `C2X_BATCH_SETTLE_IDLE`。
>
> ⚠ **本节的「绿 UFO / 红 UFO 等价性」那一块（用户同日追加、见下）不在本轮范围**：那是 **112（星莲船 UFO）** 的耦合，原文自己写着「机制怎么实现留到 112 有人接手时再设计」⇒ 本轮只做了 114 的批，**等价表原样保留**。
> ⚠ **并发写同一文件的提醒**：本轮 §1x.2 与 §1x.4 两个会话**同时**改 `cards/faith.{h,c}`（共用池的同一个文件）—— 两边的改动已确认**共存**（§1x.2 的四个标记行 + §1x.4 的 `faith_item_slowdown_permille()` / `faith_dot_slow_style()` 都在）。

用户口径：「和 th14 卡的联动 —— **大、中绿点会计入 th14 收卡的计数**」。

**为什么现在不计入（现状）**：114 的批成员门只看**引擎原生道具**——
`cards/collect2x.c` 的 `on_item_collect`：`type ∈ {1,2}` **且** `item+CE_ITEM_STATE == 3`（真空飞行收）才入批。
而 110/111 的绿点是**我们自建的 VM 实体**（`faith.c` 的池），**根本不经过 `ce_item_collect` 断点** ⇒ 天然不计入。

**★★ 用户 2026-09-15 补充：先记「等价性」，机制后设计**（原话：「具体的语义**等价于 P 点对于红 UFO**；
特别地，由于**中绿点是从 P 点转换过来的**，所以它**对于红 UFO 和彩虹 UFO 都等价于 P 点**」）
⇒ 先把下面这张**等价表**钉住（机制怎么实现留到 112 有人接手时再设计）：

| 我们的点 | 对**绿** UFO | 对**红** UFO | 对**彩虹** UFO | 依据 |
| --- | --- | --- | --- | --- |
| **大绿点**（type 3，击破掉落）| **计 2 个计数** | **等价于 P 点** | 未定 | 用户口径（「绿点是绿的 ⇒ 绿 UFO 计 2」；「等价于 P 点对于红 UFO」）|
| **中绿点**（type 9，**由 P 点转换而来**）| **计 2 个计数** | **等价于 P 点** | ★ **也等价于 P 点** | 「特别地，**由于中绿点是从 P 点转换过来的**」|
| 微绿点（type 8，消弹）| — | — | — | 不入批（§1x.3）、也不参与本耦合（半道具）|

语义要点：**等价性的来源是「它原本是什么」** —— 中绿点是 P 点被满 P 转换的产物，
所以它**继承了 P 点在 UFO 侧的待遇**（红 + 彩虹）；大绿点是击破掉落，只按「绿」这条走（绿 UFO 计 2、红 UFO 等价 P）。
⚠ 「等价于 P 点」在 112 侧**具体是哪一档计数/哪一条分支**，要等 112 的一手读出来才定（见下面「开工前提」）。

**要做**：给「自建收集实体 → 批成员」开一条**声明式**通道（照本仓既有惯例：卡只声明、层选举/落笔，
见 `EVENTS-CONTRACT.md`）。两条候选路，开工前先定：
1. **SDK 层**：加一个「虚拟收集事件」（例：`ce_collect_note_virtual(type, value)`），114 在 `on_item_frame` 里读它并入批
   —— 好处是别的自建实体卡（116 季节点 / 117 动物灵）也能复用；
2. **卡间直连**：`faith.c` 收集时调 114 暴露的函数（需要 114 导出、并处理「114 不在场」与「删卡」两种情形）
   —— 面窄，但要多一道「对方在不在场」的判断。

**★★ 用户 2026-09-15 追加的第二件事：绿点也得**被当作「**正在飞行的道具**」**。
用户原话：「th14 联动还有一件事情：**绿点也得当作正在飞行的道具**」。
含义（照 114 的实现）：114 的批**靠「有批集道具在真空飞行」续命** —— `collect2x.c` 的 `on_item_frame`
每帧调 `ce_item_pool_any_attracting()`（扫**原生道具池**里 `state == 3` 的道具）→ `c2x_batch_presence()` 续 idle。
我们的绿点是**自建 VM 池**，114 扫不到 ⇒ **一批可能在我们这些点还在飞的时候就被判「收点流停了」而提前结算**。
⇒ 这条要的不只是「收进来算一个成员」，还要「**在飞期间让 114 知道还有东西在飞**」（presence）——
两个通道（**成员** + **presence**）都要给，否则批的**边界**会错。

**★★ 用户 2026-09-15 裁决（语义已定，可开工）**：
- **分值不进批**（「如果不进 score 那无所谓」）⇒ 绿点**只让批计数 +1**，**不参与 114 的批分**
  （114 的批分 = 每颗道具的 SCORE 差量累加；绿点不进 SCORE、进信仰 ⇒ 两条账不混）。
- **presence（续命）必须有**（「续命这一块肯定得要」）⇒ 大/中绿点在飞期间必须让 114 知道「还有东西在飞」，
  否则批的边界会错（提前结算 → 倍率与碎片都不对）。
- 微档不入批（§1x.3 已定）。

**开工范围**：成员（+1）+ presence（在飞）+ 大/中两档；**微档排除**；**不动 SCORE**。

**以下为原先待定的说明（保留作背景）**：
- **计入的粒度**：只让批**计数 +1**（影响档位 20/30/40/50/60 与倍率），还是**连分值也进批**
  （114 的批分 = 每颗道具的 SCORE 差量 stash 累加；我们的绿点**不进 SCORE**、进的是**信仰**⇒ 要进分值就得给一个折算口径）？
- **哪几档**：用户说的是**大、中**；**微档**（消弹每颗那一档）算不算？
- **去重**：同一颗点被 110 与 111 同时认领时（共用池按 `slot_mask` 分账）只能算**一次**。

**开工时要一并跑的**：`make test-host` + `make check` + `check-docs.py`；若动 SDK 面（`sdk.*` / `sdk_core.*`），
报告里要单开「**对既有卡的影响面 + 回归**」（114 与所有自建实体卡）。

### §1x.3 微档不入 114 的批（用户 2026-09-15 定案）

用户口径：「**微档不计入**，因为它是消弹点，算是某种**半道具**，所谓的实体 ≈ **只是为了动画**」。
⇒ §1x.2 的范围收窄为：**只有大、中两档计入 114 的收卡计数**；微档（type 8 = 消弹每颗那一档）
**不入批** —— 它的实体只为动画存在，价值仍走既有的「消弹按颗数直接记账」。

### §1x.4 ★ 115（绀珠传）擦弹时：**大/中绿点要变黄、变慢、抖动**（用户 2026-09-15，**✅ 已实装** 2026-09-15d）

> **状态回填（2026-09-15d）**：三层**全部落地**在共用件 `native/cards/faith.{h,c}`
> （**卡侧 `mof.c` / `signal.c` 一行都没改** ⇒ 110 与 111 一起拿到）；落点 = 实体 tick
> `faith_pool_tick()` 里新增的一支 `faith_dot_slow_style()`（门 / 抖动分布 / 只改 RGB 与 116 季节点同形）。
> 逐条的性质、一手与取舍登记在 [`TUNING.md`](TUNING.md) **§2.5d**；协作登记在 `AUDIT.md` §BE。
> 与原计划**三处出入**（都以一手为准）：
> 1. **「变黄」116 那套里确实有**（`season.c` 第三十三轮补的，门 = `slow < 1.0 && age > 31`），
>    它用的黄是 **TH15 给「弹」的 `0xffffd080`**；本轮把 th18 **原生道具**那一支也逆出来了 ——
>    引擎自己在减速中给道具 VM 写的黄是 **`0xffa0ffff`**（一手 `th18:0x445d12`；关掉时
>    `0x445ebe` **无条件**写回白）。我先照引擎那支落地，随后 **2026-09-15 用户裁决：统一成 TH15 那支**
>    （原话「**肯定得统一成 th15，包括到时候 th07 的樱点**」）⇒ **`FAITH_SLOW_TINT` 已改成
>    `0xffffd080`**，与 116 季节点**同色**；引擎那支留作**已知差异**（我们的点与原生 P 点 / 金钱
>    会有 G 通道 d0 vs a0 的一点差色，**刻意如此**）。
>    **机制 = 复刻（一手逐指令）；「哪一支黄」= `❓ 用户口径`，不是复刻。**
>    ★ 待办：**将来 th07（107 妖妖梦）的樱点做减速视觉时也要用同一支黄**（用户点名，见 `TUNING.md` §2.5d 末尾）。
> 2. **「变慢」原生只乘「重力在跑」那一段**（一手：`0x445c6d` 乘重力增量、`0x445c2d`/`0x445c31` 乘位移，
>    都在 `item+0xc74 == 2` 段里；前一段 `item+0xc74 == 1` = 出生的上抛是**裸** `vel × GAME_SPEED`），
>    而 116 季节点从第三十三轮起**上抛段也乘** ⇒ 按「跟 116 一样」的口径**跟 116**
>    （绿点的飞散态就是出生直上 2.2 那一小段；要回一手口径改一个局部常量 `sl`）。
>    ⚠ 两套段号**不可逐值对照**：引擎的上抛 = `item+0xc74 == 1`，本池的飞散 = `p->state == 2`。
> 3. **不需要另补一手**：原计划把「去 `local/th15.v1.00b` 找减速期的颜色写入点」列为退路 ——
>    实际上 **th18 自己的道具 tick 里就有完整三件套的逐指令**（门 /抖动 /染色 + 还原），
>    而且它正是「原生道具同屏时发生的事」⇒ 按 th18 一手实现，**没开 `th15.exe`**。
>    ⚠ 仍如实注明：这套的**源头是 TH16 的季节点，而它复刻的是 TH15**（用户 2026-09-15 定案），
>    116 那份实现（`season.c`）本轮**刻意没动**（不在本卡所有权内）—— 提炼接法写在 `faith.h` 的声明注释里。
>
> 实跑判据（`TUNING.md` §2.5d 末尾）：减速中绿点**慢 + 黄 + 抖**，出圈**颜色自己变回绿**
> （116 在第三十三轮踩过「变黄不回来」）；日志出现
> `faith: 减速三件套 —— 近 N 帧有 M 颗点被染黄/抖动`（`M > 0` 才算三层真的生效）；
> 微档不参与（今天也不在池里）。

用户口径：「th15 擦弹时候**大中绿点也得变黄、变慢、抖动**」。

- **变慢**：有现成接缝 —— `engine.h` 的 `CE_ITEM_SLOWDOWN 0xe6bb14`（TH15 擦弹减速的残骸：引擎每帧把
  飘落态位移与重力增量乘它、并 +0.1/帧 回 1.0；115 每帧写 0.3）。**116 的季节点已经在读它**
  （`engine.h` 的注里明确写了「免得同一屏上 P 点慢了、季节点不慢」）⇒ 绿点应按同一口径读。
- **变黄 + 抖动**：★ **用户 2026-09-15 定案：不必再逆 th15** ——
  「**th16 的季节点的抖动复刻了 th15**」⇒ 直接**照 116 季节点已经做好的那套**搬到绿点上
  （落点在 `season.c` / `faith.c` 的实体 tick，两处是同源代码的两次出现，本仓已有「出现第二次就提炼」的约定）。
  ⚠ 仍需在代码注释里写明「这一套的源头是 th16 的季节点，而它复刻的是 th15」并给出处；
  若 116 那套里**颜色（变黄）**或缺、或只在释放期生效，则缺的那一层再补一手或标 `❓ 自定`。
- 落点：`faith.c` 的实体 tick（读 slowdown 的时机与 `CE_ITEM_SLOWDOWN` 的既有先例一致），
  卡侧（110 `mof.c` / 111 `signal.c`）不需要动。

### §1x.5 ★ 「大绿点与金钱完全重合」的成因（用户 2026-09-15 观察）

**现象**：用户观察到「现在 TH18 掉的**相当一部分大绿点**，和掉的**金钱完全重合**」。

**成因（我们自己定的规则，非原版行为）**：110 的大信仰点判据走的是本仓 spec 的 §7.9 口径 ——
**「按这敌实际掉什么触发」**：`on_enemy_death` / `on_enemy_drop_pre` 里 **`drop_main == 2`（TH18 的金钱）
且非 boss** ⇒ 就撒一颗大信仰点，位置就是**敌人的位置**。而金钱也是从同一个位置撒出来的
⇒ 两者**同点同时**出现，看起来「完全重合」。
**原版 TH10 不是这样**：大信仰点是**具体敌人在 ECL 掉落表里声明 type 3** 才有（`th10` 的
`ItemManager` 分派里 type 3 = 大信仰点），**稀疏得多、也不是每只掉钱的敌人都给**。
⇒ 若要更像原版，得把「掉钱的敌人都给」换成「**按 ECL 掉落表里真的声明了信仰点的那批敌人给**」
（TH18 侧没有信仰点 type ⇒ 需要一张「哪些敌人的对应物在原作掉 type 3」的映射表，属**新预研**）。
**★★ 用户 2026-09-15 裁决：「金钱和绿点总之别重合」** ⇒ 必须改掉「与钱 1:1 同点」的现状。
两条候选（开工前选一条，或两条并用）：
1. **判据收窄（更贴原版）**：不再用 `drop_main == 2`，改成「**只给原版真的声明了 type 3 的那批敌人的对应物**」
   ⇒ 需要一张「TH10 里掉大信仰点的敌人 ↔ TH18 对应敌人」的映射表（属**新预研**，稀疏、自然不重合）；
2. **位置错开（成本最低）**：判据不动，但**大绿点不在敌人位置撒**（例：抬高/侧偏一个固定量，或按弹幕/道具的散布随机）
   ⇒ 视觉上不再重合，但**数量上仍是 1:1**（「与钱同生同灭」这件事没解决）。
**★★ 用户 2026-09-15 裁决：走 1（判据收窄）** —— 「确实得 1」。
⇒ 开工内容 = ① 预研：在 `local/th10.v1.00a` 里**穷举「掉落表里声明了 type 3 的敌人」**
（ECL 掉落表 + 敌人的 drop 字段；TH10 样本在 `local/th10.v1.00a/dat/default.ecl` 与各 `st0N.ecl`），
② 建「TH10 敌人 ↔ TH18 对应敌人」映射（TH18 侧按 st0N.ecl 的敌人编号/外观/位置对；
对不上的**按「不给」处理**，宁可稀疏不可乱给），③ 把 `mof.c` 的判据从 `drop_main == 2` 换成这张表。
⚠ 注意这同时会让**大绿点变稀**（正是用户要的）；**中绿点（满 P 转换）不受影响**。

**★★ 2026-09-15 已实装**（代码：`native/cards/mof_big_faith_table.h`（新建，表 + 全套预研）、
`mof_core.{h,c}`、`mof.c`；登记：`TUNING.md` §2.5f；单测：`tests/test_mof_core.c` 的宿主表那一块）。
下面 ① / ② / ③ 三步的产出落在这里（一手脚本 = `local/_scratch/th10_drop_scan.py`、
`th10_type3_droppers.py`、`th18_script_colors.py`、`th18_fairy_roster.py`）。

**① 一手预研：TH10 v1.00a 掉 type 3（大信仰点）的敌人（穷举）**

口径：drop opcode 由 exe 分派表读出（`th10:0x40e7c9 jmp [ecx*4+0x411d30]`，索引表 `0x411ee0`）⇒
**326 dropClear / 327 dropExtra / 328 dropArea / 329 dropItems / 330 dropMain / 256–257 enmCreate(A)**；
`330` 写 `enemy+0x2408`（主掉落 type）、`327` 写 `enemy+0x2408+type*4`（计数表）、
`enmCreate*` 第 6 实参也落 `enemy+0x2408`（`th10:0x40d003` → `th10:0x40d00c`）；
死亡时 `th10:0x40e63e` 读它，`>0` 就 `spawn_item(type)` ⇒ **主掉落 type = 3 = 掉一颗大信仰点**。

| 关 | 掉 type 3 的敌人（子程序名，全部一手反编译名） | 声明点 | 有效创建点 |
| --- | --- | --- | --- |
| 1 | GGirl00 GGirl00B GGirl00C GGirl00D GGirl00E ＋ Ecl_GGirl00 Girl01 | 5 | 3 |
| 2 | GGirl00 GGirl00B GGirl00C GGirl00D GSpCir00 GSpCir01 | 6 | 0 |
| 3 | GGirl00 GGirl00B GGirl00C GGirl00D GSpCir00 GSpCir01 ＋ Girl02 | 6 | 2 |
| 4 | GGirl00 GGirl01 GGirl03 GGirl04 ＋ Girl02 | 5 | 0 |
| 5 | GCir00 GCir01 GCir02 GGirl00 GGirl00L GGirl02 GGirl04 ＋ Girl01 Girl03 | 7 | 7 |
| 6 | GCir00 GCir01 GCir01b GGirl00 ＋ Girl02 | 4 | 1 |
| 7 | GCir00 GCir00b GCir00c GGirl00 GGirl05 GGirl06 GGirl06b | 7 | 0 |
| 合计 | **40 个子程序（`dropMain(3)`）＋ 13 个创建点 = 53 个敌机位** | 40 | 13 |

- **`dropExtra(3, n)` 全 exe 零条**；**EX 面没有独立 ECL**（复用 stage07 的子程序集，第 7 关那列即覆盖 EX）。
- **是清一色绿妖精吗？** —— **40/53 是**：`G` 前缀一族（绿妖精）100% 掉 type 3。
  余下 13 个创建点是**大妖精** `Girl01/02/03`（`anmSetMain(0, 40)` = 橙系精灵，不是绿妖精）。
- **前缀 ↔ 主掉落 type 在全量 142 个声明点上 100% 同现**：`B`=2（钱，39 条）、`G`=**3**（39+1 条）、
  `R`=1（P，39 条）、`Y`=0（不掉，24 条）、`M`=7（1UP，2 条）。
- **绿是像素级验过的**：`GGirl00` 的 `anmSetMain(1, 47)`（stage01 `+0x5814`）→ `enemy.anm` script47
  → sprite113，抽图实测**纯绿 (0,96,0) / hue 120**；对照 `B`→script45→sprite114 = 蓝 240、
  `R`→script46→sprite112 = 红 0。
- **boss 一个都没有**（`Boss*` 声明的是 1；`M` 前缀的道中 boss 辅助体是 7）。

**② 映射：TH10 ↔ TH18（TH18 侧 = 「本来就是绿的那批敌人」，实测色）**

方法：`thecl -d 18 -x` 反编译 22 个 ECL + `thanm -l/-x` 抽贴图，对 `enemy.anm` **每个扁平脚本**
取 sprite 不透明像素的中位 hue。**校准**：`GirlBlueA01`(S=0) → 228（蓝）、`GirlRedA01`(S=5) → 12（红）
—— 与 ZUN 命名逐条吻合。

| 关 | TH10 掉 type 3 的敌人 | TH18 侧对应（实测为绿者） | 判据 | 可信度 |
| --- | --- | --- | --- | --- |
| 1 | 绿妖精 ×5＋Ecl_GGirl00＋大妖精 Girl01 | `Cir00m`(S68) `Cir00bm`(S80) `Cir01a`/`Cir02a`(S56) | 双方都实测为绿；都是本关小妖精杂兵 | ✅ |
| 2 | 绿妖精 ×4＋GSpCir00/01 | `Cir00` `Cir01`(S56) | 同上 | ✅ |
| 3 | 绿妖精 ×4＋GSpCir00/01＋大妖精 Girl02 | **无**（st03 敌人脚本无绿：`Cir00` 是 S62 = 紫 292）| 全关对不上 | ❌ |
| 4 | 绿妖精 ×4＋大妖精 Girl02 | `Cir02` `Cir03`(S56) | 双方实测为绿 | ✅ |
| 5 | 绿妖精 ×7＋大妖精 Girl01×6/Girl03 | **`GirlBlueA03`(S10)** ＋ `Cir01`(S56) | S10 = 蓝 0 / 红 5 / **绿 10** 那一族的第三色 | ✅✅ |
| 6 | 绿妖精 ×4＋大妖精 Girl02 | **`GirlA04`(S10)** ＋ `ECir01`(S56) | 同上 | ✅✅ |
| 7 | 绿妖精 ×7 | `Cir01`(S56) | 双方实测为绿 | ✅ |

**对不上、按「不给」处理的清单**（这条预研的已知缺口）：
① **TH18 stage 3 的绿妖精**：**待用户实跑后定**（原话「stage3 我得玩一下」）⇒ 不添绿宿主
（现况：st03 一个绿的都没有；⚠ 但下面 ② 的大妖精会把 st03 的 5 个站点点亮）；

② ~~大妖精暂不入表~~ ⇒ ★ **2026-09-15 已按用户口径纳入**（原话：「**大妖精掉什么都不奇怪，可以直接大方往里加**」）：
  TH10 `Girl01/02/03` 的 **11 个创建点** ↔ TH18 `GirlD01/D02/D03`（anm 脚本 **40 / 224**，实测 hue 32/46 橙，
  与 TH10 script40→sprite96-99 **结构同形**）⇒ 表里加 `{2,40}` `{2,224}` 两行（S=40 31 站点 + S=224 22 站点）。
  **这是用户口径、不是复刻**；宿主总站点 62（绿）＋ 53（大妖精）= **115**。副作用：**stage 3 也因此有 5 个站点**
  （`GirlD01`×4＋`GirlD02`×1）—— 要「纯绿版」得给表加关卡掩码（`CE_CURRENT_STAGE()` 现成），见 `TUNING.md` §2.5f #6；
③ **TH10 的 40 个绿妖精子程序里有 16 个在自家关卡零引用**（没有任何 `enmCreate*` 站点；逐条见 `mof_big_faith_table.h` §4-3）⇒ 无对应物可言。
⚠ 另有两条**方法缺口**：TH18 侧「绿」是按**贴图底色**测的，**运行时 `anmColor` 改色测不出来**
（st06 `ECir01` 就是「贴图绿 + 染暗紫」，且它零引用 ⇒ 未入表）；st02 的 `Boss*`/`MBoss` 用的
`st02enm` script0 也发绿（hue 134），靠「键只收 N=2」+「非 boss 门」双重挡掉。

**③ 判据改动点（改前 → 改后）**

| 项 | 改前 | 改后 |
| --- | --- | --- |
| `on_enemy_death` 的判据 | `drop_main == MOF_DROP_TYPE_MONEY`（＋非 boss）| `mof_core_wants_big_host(kind, flags, N, S, is_boss, diff, &v)` |
| 键 | 这敌掉什么（`drop_main`）| 这敌**画什么**（`enemy+0x1494` = N、`enemy+0x1498` = S）|
| 宿主范围 | （无）| **绿妖精族 4 个身份 ＋ 大妖精族 2 个身份**（后者 = 用户 2026-09-15 口径）|
| 门 | `mof_core_death_qualifies()`（含「必须有掉落表」）| `mof_core_death_is_real()`（去掉掉落表那一条）|
| `on_enemy_drop_pre` | 挂着（旧判据②「同时掉钱与 P」）| **整条删除、不再挂**（判据②随之作废）|
| 中绿点（满 P 转换）| `mof_core_full_p_replace()` + 池扫描 | **一个字没改** |

新偏移的一手：`th18:0x436c85`/`0x436c91`（`anmSetSprite` 303）与 `th18:0x431659`/`0x431675`
（`anmSetMain` 306）**只在槽号 K==0 时**写 `zEnemyData+0x26c`（= S）/`+0x268`（= N）；
`zEnemyData` = `enemy+0x122c` ⇒ **`enemy+0x1494`/`+0x1498`**。

**实跑该看什么**：① **大绿点明显变稀**（只有上表那 10 个子程序死时才给）；
② **不再与金钱 1:1** —— 宿主里多数原生 item = 1（P）或 0（不掉），只有 st02 `Cir00/Cir01` 与
st05 `GirlBlueA03` 各有一点是钱；③ 日志行换成
`mof: big faith +N via host table @(x,y) (anm N/S, kind …, drop_main …, boss …, enemy …)`
（`drop_main` 只作记录）；④ **中绿点照旧**（满 P 的 `conv N` 不掉）。
**2026-09-15 用户追加裁决两条**：① 大妖精族 **已纳入**（`{2,40}`/`{2,224}`，见 ② 末段）；
② TH18 stage 3 **待用户实跑后定**（绿宿主不添；大妖精那 5 个站点随 ① 生效）。两处都登记 `TUNING.md` §2.5f #5/#6。

### §1x.6 ★ 绿点飘字要显示「**两卡合计**」（用户 2026-09-15 定案，待开工）

用户口径：「110 绿点是 **+10000**、111 绿点是 **+100**，所以**只有 110 时显示 10000、只有 111 时显示 100、
同装时应该显示 +10100**（尽管两侧的含义不完全相同）」。

**现状不满足**：飘字在 110 侧（`mof.c` 的 `mof_on_pickup_at` → `mof_popup_number(pos, value, 绿)`），
显示的是**110 自己的那份 value**；**111 侧完全没有飘字**（`grep popup cards/signal.c` = 0）。
⇒ 同装时**既没有 111 的 100，也没有合计**。

**要做（建议的落点）**：把飘字**从卡侧搬到共用池**（`faith.c`），因为只有池知道「这颗点被哪些槽认领」：
1. `faith.c` 在收点那一刻：**向每个认领该点的注册者索取它自己那一档的值**（给 `faith_user_t` 末尾加一个
   「按 tier 报值」的回调，或让注册者在注册时给一张**档位→值**的小表）；**求和**；
2. 用**合计值**画**一次**飘字（位置仍是道具自身坐标）；**微档不弹**（一手：TH10 type 8 无弹窗，见 §1x 的
   `mof_on_pickup_at` 注释）；
3. `mof_popup_number()` 目前是 `mof.c` 的 **static** ⇒ 要**提炼进共用件**（放 `faith.c` 内部或
   `sdk.h` 的 static inline；后者属改共享件，要在报告里单开影响面）。
   ⚠ 顺带：它写的是**引擎弹字环的数字槽**（`mgr+0x624`、stride `0x48`），与 `collect2x.c` 的倍率槽
   （`mgr+0x9cc`）**不同槽、不冲突**，但**槽满会丢**（丢时打 `mof: green popup dropped`）。

**同装的语义缺口（要写进注释）**：两侧含义不同（110 = 信仰、111 = 最大得点），合计只是**显示口径**，
**不代表数值可以互相换算**；这句话必须写进注释，否则下一个人会把 10100 当成一个有效的信仰值。

### §1x.7 ★ 与 112（星莲船 / TH12 飞碟）的耦合：**绿点会被飞碟吸走，且绿点给绿 UFO 计 2 个计数**（用户 2026-09-15，**计划**）

用户口径（原话拆解）：
- 「绿点**也会被飞碟吸走**」；
- 「因为绿点是绿的，所以 **1 个大/中绿点会给绿色 UFO 计 2 个计数**」；
- 附注：**112 目前尚未有人介入**（这条是给接手 112 的人的计划）。

**为什么现在做不到（与 §1x.2 同形的问题）**：绿点是 `faith.c` 的**自建 VM 池**；
112 的飞碟吸收走的是**原生道具链**（扫描 / 吸附 / 计数都在引擎那道），
**看不到我们的池** ⇒ 绿点**不会被吸、也不计数**。

**要做**：
1. **可被吸**：给「自建实体 → 飞碟吸收」开通道（形状参考 §1x.2：**声明式**优先，便于 116 季节点 / 117 动物灵复用）；
2. **按颜色计 2 个计数**：绿点对**绿 UFO** = **2 计数**（用户口径）；对其它颜色 UFO 的行为**未定**
   （按 112 既有规则推：同色 / 异色的待遇不同 —— 需要 112 侧一手定案后再写）；
3. **吸走之后算谁的账**：**未定** —— 绿点被飞碟吸走时，它**还算不算 110/111 的信仰**
   （即：飞碟吃掉点是纯收益，还是把信仰也让渡出去）？这条必须先定，否则同一颗点会被两处记。

**开工前提**：
- **112 的既有机制要读一手**：它的吸收**判据 / 计数单位**（「1 个计数」到底是「1 颗道具」还是「1 点吸收进度」）、
  颜色匹配规则（社区口径的「金 = 蓝」「两同一异」是否仍在代码里成立）、以及吸收 vs 击破开奖的关系；
- 定「吸走后的账」与「异色 UFO 的计数」两条语义；
- ⚠ 112 现在**没有 owner** ⇒ 动它之前先照 `CLAUDE.md` 的协作纪律确认归属，并在 `AUDIT.md` §BE 登记。

## 1z. 六作消弹口径一手核查收尾（TH06–TH17）—— 研究已回流 `engine/bullet/`

> 用户口径（2026-09-15）：「**对于系统卡，得核实其作品原本的消弹是什么，其他的你别管**」。
> 本轮把这句做完了：**TH06–TH17 十二作**逐作一手核过「消弹时原版到底发生什么」。

**结论落在哪**：

- 一手过程 → `engine/bullet/th06/` … `engine/bullet/th17/`（逐作一篇「消弹 → 出不出实体道具」）；
- 跨版本断言 → [`engine/bullet/OVERVIEW.md`](../../../engine/bullet/OVERVIEW.md) 的
  **「消弹口径 × 逐作」**节（13 行逐作真值表 + 六条跨作断言）；
- 原始研究记录（gitignored，不入库）：`local/_scratch/research-cancel-original-th06-th11.md`、
  `local/_scratch/research-cancel-original-th12-th17.md`。

**最重要的一条**：**`mode` 号在作品之间完全不可移植**，而且**多数作连 `mode` 都没有**
（TH06 = `ZunBool`、TH08 = `arg`、TH10/TH12/TH13 = bool、TH11 一个参数都没有、TH09 **连道具系统都没有**）。
同一个数字语义可以相反：**TH16 的 `mode 1` 一颗不撒，TH17/TH18 的 `mode 1` 撒**。
⇒ 以后任何「把某作的 mode 数字搬到另一作」的写法都要先回它自己的 exe 核。

**本轮采纳并落地的一条**（其余见下）：

| 卡 | 改了什么 | 一手依据 | 登记 |
| --- | --- | --- | --- |
| 116 | `SEASON_CANCEL_PER_POINT` **8 → 1**（复刻） | TH16 `th16:0x416a00` 的 `mode 4` = 季节道具**每颗弹 1 个** | [`TUNING.md`](TUNING.md) §2.6e |

**三条「刻意不动」**（一手在，但本轮不改；写在这里防止丢失）：

1. **109 `pofv.c` 蓝圈传 `mode = 1`**：TH09 一手**一颗道具都不掉**（连道具系统都没有，收益是
   数值条）。让它继续撒 TH18 的 type 9 点道具 = 用宿主里**原版不存在的东西**当"转成分"的载体。
   改成 `0` 就要求我们自己记分（数值会变成 ❓ 自定）⇒ 用户 2026-09-15 **挂起**，未采纳。
   红圈传 `0` 是本作一手（「红圈消掉的白弹直接消失」）✅。
2. **112 `ufo.c` 不消弹**：TH12 一手**飞碟召唤与击破各带一圈扩圈 `etCancel`**（半径 16→112 / 16→640），
   扫过的每颗弹 → 1 个 `type 9`。我们**一次都没调** ⇒ 与原版行为不同。**未采纳**；
   且 112 现在**没有 owner** ⇒ 真要动，先按 `CLAUDE.md` 协作纪律确认归属并在 `AUDIT.md` §BE 登记。
3. **110 / 111 的两条**：①原版消弹撒的是**真实体** `type 8`（掉在弹自己的位置、`state=5`、
   **要走过去收**），我们是 `mode 0` + 按颗数直接记账；②原版**灵击消激光**（`th10:0x405b33` → `th10:0x41c800`、
   `th11:0x424fe0`），我们登记为"不消激光"。两条都是**已知差异**（已登记），用户 2026-09-15 **挂起**。

**一条主动订正**（研究 → 一手复核后推翻）：研究把 `animal.c` 的两处消弹判成「卡自己的设计，非一手」，
**父 agent 复核后推翻**：`ANIMAL_ORB_CANCEL_MODE 1`（半径 28 = 原版 `[0x4a3ce0]` 的 `28.0f`）与
`hyper_frame` 的 `by_hit ? 0 : 1`（中弹取消 → 0 / 按键 → 1）**都与 TH17 一手对得上**
（`th17:0x447bfe` 的 `push 1` 紧跟 `Player__cancel_impending_death`；`th17:0x40f386` 的 `push 0` 门是输入位）
⇒ **117 不改**。

## 1y. ★★★ 110 / 111 的 HUD「看不见」（用户 2026-09-15 口径）—— 2026-09-15c 已落地

> 用户口径（转述给本轮的口径）：「**110 与 111 的 HUD 全都看不见**」。上一轮（§2.6 的第 8 条）
> 已经修过 110 的**坐标**（脚本 `pos()` 与卡侧 `entity_pos` 相加 ⇒ 图标/数字画到 1800 那一带，
> 屏面只有 1280×960）；本轮从**同一次实跑的日志**继续往下查，结果是**两条独立的根因**。

### §1y.1 ★★★ 111 的真根因：HUD 槽**从来就没认领成功过** ⇒ 3 支 VM 一次都没起

- `signal.c` 的 `on_load` 原来写的是 `ce_hud_slot_claim_card(SIGNAL_ID_NUM, &blk, 0)`
  —— **第三个实参是 NULL**，而 `sdk_core.c` 的 `hud_register()` 第一行
  `if (card_id > CE_SDK_CARD_ID_MAX || !blk || !out) return 0;` ⇒ **认领永远失败**。
- 后果不是「摆错位置」而是「**整块没建**」：`s_hud[111].used` 恒 0 ⇒ `ce_hud_slot_get(111, …)` 恒 0
  ⇒ `signal_hud_draw()` 的**第一句就 return** ⇒ 三个原版 VM（脚本 212/213/214）一次都没 `spawn`、
  三行读数一笔没画。**这解释了用户看到的「什么都没有」**（也解释了为什么颜色怎么调都不会有变化）。
- 日志判据（本次实跑 `th18_card_expand.log`，`local/GAME-LOG.md` 的路径）：
  `signal: ce_hud_slot_claim failed` 出现 **4 次**（= 4 次 `on_load`）、而
  `signal: HUD block … claimed at surface` **一次都没有**（那一行只在成功分支里打）。
- 旁证：别的卡都传了实参（116 `&s_slot` / 117 `&slot` / 109 `&s->slot`）—— **只有 111 传 0**。
- ✅ 修法（本轮，`signal.c` 单点）：给一个真的 `ce_hud_slot_t`，顺便拿它打坐标（删掉原来的 probe）。
  回归写进 `native/tests/test_sdk_core.c` 的 `test_hud_slots()`：**按卡侧真实参数**（208×56、LEFT、id 111）
  认领必须成功、`_get` 读得回同一个槽；`out = NULL` 断言成「一定失败」⇒ 谁再写成 0 立刻红。
- ⚠️ 影响面：只动 `signal.c` 的 `on_load`（本卡文件）+ 一条测试；**不碰 `sdk_core.*`**
  （`hud_register` 的 `!out` 门槛是**正确的防御**，不改它 —— 错的是调用方）。

### §1y.2 ★★ 顶点色的两路：`c2=00000000` **不是**判决字段，但两路都写仍要做（110 + 111）

- 日志里 9 支 VM 清一色 `c1=ffffffff c2=00000000`（连**一直被看见**的连击条也是）⇒ 单看 `c2=0`
  不能推出「看不见」，**这一点本轮用一手钉住了**：
  - 指令分派表 `0x47ba4c`/`0x47bb98` 直读：**403 `alpha` → handler `0x479efd` 写 `+0x527`（color_1.A）**、
    405 `alpha2` → `0x479fa1` 写 `+0x52b`（color_2.A）、404 `color` → `0x479f44` 写 `+0x524..0x526`；
    每帧提交插值的 `0x47c7f2` 也写 `+0x527` ⇒ **脚本的 `color()`/`alpha()`/`alphaTime()` 全落在 color_1**，
    本仓这些「原版转写」脚本没有一个写 `color2()` ⇒ `color_2` 恒 0 是**结构性**的，不是异常。
  - `colorMode`（opcode 423 → `0x47b69b`）决定绘制取哪一路：`0x47e202` 的 `mode 0 → +0x524`、
    `mode≠0 → +0x528`，取中的写进 `+0x52c`。本仓脚本都不写 `colorMode` ⇒ mode 0 ⇒ 取 color_1 ⇒
    **颜色本身是可见的**（这解释了为什么连击条与 111 的槽底那种"写了 color()"的 VM 也是 `c2=0`）。
- ⇒ **订正本仓两处文档**：`engine/anm/th18/07-blendmode-rgb-only-alpha-is-sa.md` §2 与 `AUDIT.md` §CT-7
  把「`alpha()` 写 color_2 的 A」归到了 handler `0x479fb9` —— **`0x479fb9` 是 `alpha2`(405)**。
  ⚠️ 它**不改**那两处既有修法的正确性（`trance.c` 的 `wipe_set_dual_color` / `faith.c` 的两路同值写
  都是"两路都写" ⇒ 与 colorMode 无关，照旧有效）；**本轮没有动 `engine/**` 与别人的卡**
  （`engine/**` 不在本轮文件所有权内 ⇒ 只登记在这里 + `TUNING.md` §2.6c，请 113 的 owner 顺手改文档）。
- ✅ 本轮落地（**镜像**而不是硬写常数）：新原语 `ce_anm_mirror_color()`（`sdk.h` 的 static inline）
  = 把 `color_1` 原样写进 `color_2`；110 的 9 支（`mof_hud_dual_color()`）与 111 的 3 支
  （`signal_hud_dual_color()`）**每帧**各调一次：
  - **淡入不被覆盖**（取舍的关键）：210/211 的 `alpha(0)` + `+20: alphaTime(20,0,255)` 跑在 color_1 上
    ⇒ 硬写 `0xffffffff` 会抹平那 40 帧；镜像逐帧跟着插值走 ⇒ 观感与一手一致，**不需要"只写一次"的折中**；
  - **不覆盖既有配色**：213 的档位闪色（`color(255,255,0)`↔`(255,0,0)`，中断切）、212 的基色
    `color(32,32,64)`、连击条的白（卡侧 `ce_anm_set_color`）全在 color_1 上 ⇒ 镜像不碰 color_1；
  - **不弄坏可见的**：color_1 一个字节都不动 ⇒ 现下可见的那条路逐字节不变。
- 诊断补强（❓ 自定，只读）：110 的 `mof: hud <tag>` 行**新增** `flags=`（`+0x534`：含 colorMode、
  渲染模式、绘制门槛位）与 `eff=`（`+0x52c` = 真正画出去的顶点色）；111 侧**首次**加同组
  （`signal: hud up` + 每 600 帧三行）。

### §1y.3 ★★ 下一跑要看的（110 的可见性**还没有**一手根因，别把它当已修）

- **110 侧本轮没有找到"必然看不见"的根因**：日志实况里 sprite/pos/layer 全对、`c1.A=255`（淡入早已走完）、
  colorMode 0 ⇒ 按理应当可见。**本轮做的是"把可疑面收窄"**，判据落在新增的两列上：
  1. `flags=` 的 bit0/bit1（绘制门槛，一手 `0x481248`：**两位都得是 1**）、bits17–19（colorMode，应为 0）、
     bits26–30（渲染模式：**纯 sprite = 0、`drawRect` = 16**）—— 若图标/数字的 bit1 是 0，那就是**绘制门槛**
     的问题（op 316/317 没人给我们置位 ⇒ 要找引擎里谁对 UI 列表 VM 置位，或卡侧显式写）；
  2. `eff=`（`+0x52c`）—— 若它非 0 而画面仍空，问题就不在颜色，而在**贴图/条目/合成**那一层
     （届时按 `engine/anm/th18/05-snapshot-and-surfaces.md` 的层号/组号往下查）。
- **第一跑必看的 111 行（本轮新增）**：`signal: HUD block 208x56 claimed at surface (x, 924) — 槽位左下角`
  （左列 x 从 257 起算；同装有别的左列块时按 card_id 升序右移 / 抬行，所以 x 不必是 257）
  → `signal: hud up —— 3 支原版 VM 全在` → 每 600 帧 `signal: hud bg/bar/icon …`（`c2 == c1`、
  `eff` 与 `c1` 同色）。画面：TH11 原版的**深蓝底槽 + 白/闪色增长条 + 图标** + 右侧三行读数。

### §1x.5-B ★ 替代方案：**把一部分妖精换成「绿妖精」**（用户 2026-09-15 提议）

用户原话：「th10 是有**绿色妖精**的，但是后面就没有绿色妖精了，这个就很烦。
所以我们能不能**替换掉其中一些妖精，变成绿色妖精**？」

**为什么这条比映射表更对路**：TH10 的「**绿妖精 = 大信仰点宿主**」是一条**视觉语言**（玩家一眼知道该打谁）；
§1x.5 那条「TH10 敌人 ↔ TH18 敌人」的映射表既脆（编号/波次对不上）又**丢掉了这条语言**。
⇒ **如果由我们指定「哪些妖精变绿」，那么「谁是宿主」就变成我们自己定义、且当场可见的规则**，
映射表可以整个省掉。

**三种候选实现**（开工前先读一手再选）：

| # | 做法 | 代价 / 风险 |
| --- | --- | --- |
| A | **给原生敌人染色**：敌人出生后找到它的 anm VM，写 `color_1`（每帧或出生一次） | 要一手：TH18 的 `zEnemy` → anm VM 的路径 + 颜色字段；ECL 可能每帧重置 ⇒ 可能需要每帧写 |
| B | **换成自建绿妖精**：用 `ce_spawn_enemy()`（SDK 已有，带 color/art 参数，见 `cards/ufo.c:1001`）+ 干掉原体 | 最可控，但会动到 ECL 波次/掉落/计数（106–117 有卡片依赖敌人死亡与掉落层）⇒ 风险最高 |
| C | **只换贴图**：给那几只妖精的 anm 脚本换一支绿版脚本（原生是同一个 VM 原地换脚本） | 要改资产（`enemy.anm`），且「哪些敌人用哪支脚本」在 ECL 里 ⇒ 仍需一张表 |

**★★ 用户 2026-09-15 答复**：
- **② 绑定成立**：「**这个是绑定的**，因为哪个妖精掉哪个颜色的点是**视觉语言**」
  （并附注：「虽然这作**蓝妖精掉黄色的钱**」⇒ 原版的配色语言**本来也不是严格的**，
  但用户要的是「**绿妖精 ↔ 绿点**」这条绑定）。
  ⇒ 判据与掉落**同一个开关**：**变绿的妖精 = 掉大绿点的妖精**，两者一起定义、不许分开。
- ③ 默认写死：**不改变它们的行为**（血量 / 弹幕 / 掉落总量 / 与其它卡的联动）。
**仍待定**：
- ① **「哪些妖精变绿」的判据** —— 这是唯一剩下的决定，且它**同时**决定了谁是宿主。
  候选（三选一）：
  (a) **按 TH10 的原版宿主对应**：先做完 §1x.5 的「TH10 掉 type 3 的敌人」预研，再映射到 TH18 的妖精
      ⇒ 最贴原版，但要那张映射表（就是 §1x.5 的活）；
  (b) **按我们自己的投放节奏定**（例：每关 N 只、按波次/外观挑）⇒ **是设计不是复刻**，必须标 `❓ 自定`
      并写清理由（好处：完全不依赖映射表，且能主动避开「与金钱重合」的敌人）；
  (c) **按「现在会掉大绿点的那些」反推**（= `drop_main == 2` 的妖精）⇒ 实现最省，**但会与金钱继续 1:1 重合**，
      与用户「金钱和绿点总之别重合」的裁决**冲突** ⇒ **不建议**。
**★★ 用户 2026-09-15 最终口径：先做 (a) 的统计，再谈替换**（原话：「**先统计 (a)，这样才能够说怎么替换**」）
⇒ 顺序钉死为：**① 先统计 TH10 里掉 type 3 的敌人（逐关 × 数量 × 是否都是绿妖精）⇒ ② 据此决定「把 TH18 的哪几只妖精变绿」**。
(b) 不再是首选（它只是不依赖映射表的省事路），(a) 的统计是**决策依据**，必须先有数。
正在跑的 §1x.5 会话（`b8857825`）已按此加码：**逐关统计表 + 汇总数 + 外观确认**列为第一交付项。

### §1x.8 ★ 减速期的「黄」统一成 TH15 那支（用户 2026-09-15 裁决，含 th07 待办）

用户原话：「**肯定得统一成 th15，包括到时候 th07 的樱点**」。

- **统一目标色 = `0xffffd080`**（TH15 给**被擦到的弹**的黄，一手 `th15:0x419d3d`；116 季节点在用的
  `LOLK_GRAZED_COLOR` 就是它）⇒ 绿点（110/111）、116 季节点、**将来 107 的樱点** 三者同色。
- **被放弃的那支**：引擎自己在 `th18:0x445d12` 给**减速中的道具 VM** 写 `0xffa0ffff`（还原在 `0x445ebe`）。
  ⇒ 我们**刻意不用**引擎那支，理由 = **用户要求与 TH15 / 季节点统一**。⚠ 这是**用户口径**，
  **不许**在注释里写成「复刻」；引擎那支作为**已知差异**保留记录。
- **落点**：`cards/faith.c` 的 `FAITH_SLOW_TINT`（一个常量）；`TUNING.md` §2.5d #5 同步。
- ★ **连带待办（用户点名）**：**th07（107 妖妖梦）的樱点将来也要用同一支 `0xffffd080`** ——
  届时 107 的减速视觉必须**复用同一常量**，不许再挑第二支黄（避免同屏多色）。

## 2A. ★★★ 110/111 的「组合 UI」（用户 2026-09-15 规格，**待实装**）

> 用户原话（两段，逐字）：
>
> ①「采用**合体 UI**。**th10 的 UI** 是，一个蓝点，右侧是分为**两行**，**最大得点**，下面是**连击槽**。
> **th11 的 UI** 是，一个蓝点，右侧直接是**最大得点 6 位「×信号强度」**，而**最大得点上面是信号强度**。」
>
> ②「**组合 UI** 就是：一个**稍大的蓝点**，右侧分为**三行**，从上到下：**信号强度槽**、
> **最大得点×信号强度数值**、**连击槽**；**连击槽长一些以托住最大得点×信号强度数值**。
> **最大得点显示的数值 = 基础最大得点 + 风神录额外最大得点（会消失）+ 地灵殿额外最大得点（不会消失）**。」
>
> ③「这样的话，**只有 110 或只有 111 都显示这一套 UI**，只是对应的那部分**不起作用**。
> **只有 110 的时候**，信号强度显示 **1.00**，信号**满格（不闪烁）**。
> **只有 111 的时候**，连击条显示**填满的**。」
>
> ④ ★★ **订正 ② 的读法**（用户 2026-09-15 追加）：「这里**只显示风神录 + 地灵殿的额外最大得点**，
> 因为我们说过，**系统卡只加额外最大得点** ⇒ **所以这个地方是可以显示 0 的**。」
> ⇒ **行内数字 = `Δ_10 + Δ_11`（两份"额外"之和），不是 `基础 + Δ_10 + Δ_11`**。
> 基础（110 恒 50000、111 按难度）是**宿主自己那份最大得点**的事，**不进这张块**。
> ⇒ 一局开头、还没吃到点时这一行显示 **0**（不是 50000）—— 那是**正确显示**，不是没数据。
> ⚠ 实现上别把 `faith_slot_total()`（基础 + Δ）直接画上去；要画的是**两个槽的 `delta` 之和**
>   （各自 `/FAITH_FIXED` 出定点后相加）。

**★ 这条规格与我们已有的槽模型是同一件事**（不用新造概念，只要把它画出来）：
`native/cards/faith_core.h` 的三个槽 ——

| UI 元素 | 对应我们的东西 | 出处 |
| --- | --- | --- |
| **基础最大得点** | 槽的**基础**（110 恒 50000；111 按难度 25000/50000/100000/200000） | `faith_core.h` §基础值、`faith_base_111()` |
| **风神录额外（会消失）** | **SLOT_110** 的 `delta`（信仰点；会 drain 掉） | `faith_slot_drain_empty()` |
| **地灵殿额外（不会消失）** | **SLOT_111** 的 `delta`（SA 绿点，不衰减） | 同上（111 侧不调 drain） |
| **最大得点数值（块内显示）** | ★ **只画 `Δ_10 + Δ_11`**（两个槽的 `delta` 之和，可为 0）；**基础不进这块**（见 ④） | `faith_core.h` 的 `delta` 字段 |
| **信号强度（槽 + 数值）** | **SLOT_SIGNAL**（0..130）+ `faith_combo_value` 之外的 `faith_signal_*` | `faith_core.h` §信号强度 |
| **连击槽** | **TH10 的连击计时器**（130 帧满；事件加值 大信仰点 120/蓝点 100/小P 60/大P 20/微 3） | `faith_combo_*`、`mof_core.h` 的 `MOF_COMBO_*` |
| ⚠ **TH11 一侧没有连击槽** | 一手 2026-09-15 已定：111 侧一律**不调** `faith_combo_*` | `faith_core.h:172-176` |

**待实装的部分（纯 UI）**：
1. **一手布局考证**（agent `c7327c49` 在跑）：TH10 左下角那块（蓝点 sprite / 数字位数与字体 / 那条槽的**真身** /
   填充是**裁切**还是分段）、TH11 左下角那块（蓝点 / 最大得点 6 位 /「×信号强度」怎么画 / 信号条的**一手填充写法**）、
   以及**可复用的原版切片清单**；产出 `engine/player/th10|th11/…` + 一份可直接照做的布局规格。
2. **「缺卡」时的三行状态**（用户 ③，2026-09-15）：**只装 110** ⇒ 信号强度显示 **`1.00`**、信号槽**满格且不闪烁**
   （= 中性乘数、不参与）；**只装 111** ⇒ **连击条填满**（110 不在，连击槽无驱动 ⇒ 画满、同样不闪）。
   ⚠ 实现上要能**判"这张卡在不在场"**（同一套 UI 由谁驱动 / 谁负责画）—— 落点建议：共用件里查
   `faith_slot_mask` 或卡注册表（`ce_hud_slot_*` 同一族），**别用"槽值恰好为 0"当判据**。
3. 按 ② 的三行布局建 **anm 脚本 + 卡侧摆放**（`cards/mof.c` 的 `mof_hud_*` / `cards/signal.c` 的 `signal_hud_*`
   目前是**两套各自独立**的 HUD ⇒ 要合到**一张块**里；槽位仍走 `ce_hud_slot_*` 与 `HUD-LAYOUT.md`）。
4. **信号强度活动部分的填充必须是「截断」不是「伸缩」** —— ✅ 2026-09-15 已修（共享件
   `ce_anm_set_clip_x()`，`sdk.h`；111 的美术缩放 = 1.0）。
5. 数字：6 位最大得点 + 「×」+ 信号强度值（`1.00` 这种两位小数）—— **能借原版数字字体就借**
   （本仓铁律：UI 一律先从原代码找）。

### §2A 状态回填：**✅ 已实装 2026-09-15f**（等实跑）

**做了什么**（一条命令能复跑的都在括号里）：

| 件 | 位置 | 说明 |
| --- | --- | --- |
| 纯逻辑（可主机测）| `native/cards/faith_hud_core.{h,c}` + `native/tests/test_faith_hud_core.c` | Δ 求和、10 的倍数截断、6 位零填充、档→条宽、信号强度、闪色四档、倍率文本、连击条宽、中性值；**4087 断言**，其中**逐值对账** `signal_core.c` 的同式函数（档/条宽/强度必须恒等）|
| 引擎侧共享件 | `native/cards/faith_hud.{h,c}` | **16 支 VM**（图标 1 + 信号槽底/条 2 + 6 位数字 + 「×」+ 倍率 4 + 连击条 2）的唯一持有者；块认领 / 在场判定 / 中性态 / 裁切填充 / 档位闪色 / 压暗 / 四处诊断 / 回收 |
| 在场判定 | `native/cards/faith.{h,c}` 新增 `faith_registered_mask()` | **只读**查询（注册表的并集）—— 用户③ 的判据，**明确不用「槽值 = 0」** |
| 资产 | `assets/ability/entries/ORDER.txt` 末尾 +`ATLAS_FAITH_HUD 16x32` / `SIGNAL_MUL` + 脚本 **275/276** + 生成器 `make_faith_hud_art.py` | 用掉的号 = entry **52** / sprite **430**；下一组空闲号仍是 entry 53 / sprite 431 / 脚本 277 |
| 退役 | `mof.c` 的 `mof_hud_*`（9 支）+ `signal.c` 的 `signal_hud_*`（3 支原版 VM + 3 行读数）| 脚本 211/212/213/214/104 **复用**（不删资产；210 变成孤儿脚本）；VM 的回收统一到 `faith_hud_departed()`（`on_delete` 跑得到）|

★ **「×」这一格的字形换过一次（2026-09-15f，`AUDIT` §DC-3）**：第一版借 TH11 `ascii.anm` entry0 的
sprite10（`(140,0,14,14)`）= **四角菱形**，不是 ×；现在改用 **TH18 自己 font 2 的 `'*'`**
（`ascii_1280.png` sprite209 `(208,320,16,20)`，形状就是 ×）居中补进 **16×32** 画布 ——
**同一个字符 + 同一套字体同一条基线**，画的仍是 TH11 一手格式串 `"%.6d*%1.2f"` 里的那个星号
（`ORDER.txt` 只改自己上一轮追加的那两行：`16x16` → `16x32`、`@0,0,14x14` → `@0,0,16x32`，
**sprite 号 430 不变**，冻结段未动）。

**块的几何与 ❓ 自定旋钮**：见 `TUNING.md` §2.6f（块 94×30（640）= 188×60 表面；蓝点放大 2.5；
连击条满槽 75 px；字形选择；**压暗门限不抄某一作的常数 —— 由本块矩形每帧推**（进 = 落在块内 /
出 = 外扩 16 px）、终点 alpha 32、C 每帧 ±51）。逐条一手见
`engine/player/th10/02-hud-bottom-left.md`、`th11/03-hud-bottom-left.md`、`th11/04-merged-hud-spec.md`。

★ **顺带订正一处「复刻被写错」**：两块的原生美术在 `resolutionMode(2)` 下会被折半，旧代码的缩放
写 1.0 ⇒ 实际只有原版相对尺寸的一半（旧 110 的 6 位数字 ≈4×4.5 px、旧 111 的槽 24 px 宽）；
本轮按规格表 §1 的「640 的 48×16 → 表面 96×32」改成 **2.0**（FONT2 那套例外，它本来就是表面像素）。

**实跑要看**（照 §2A 的用户口径逐条）：① 三种卡组（只 110 / 只 111 / 都装）都出现同一块 UI；
② 只 110 ⇒ 行 1 `1.00` + 满格**不闪**；③ 只 111 ⇒ 行 3 **填满**；④ 一局开头行 2 = `000000`（**正确**）；
⑤ 日志两行一次性（`faith_hud: 在场 = …` / `组合 UI 16 支 VM 全在`）+ 每 600 帧一行汇总。

**闸门**：`make test-host` / `make check` **rc=0**（`x87 in our objects: 0`、`capcheck 全绿`、
`engine_abi_check 通过`、`check-docs.py 293 文件全过`）。⚠ **DLL 与 `ability.anm` 必须同批发布**
（新的 sprite/script 号烘在 DLL 的 `anm_ids.h` 里）。

#### §2A 第二轮实跑修（用户 2026-09-15f 七条，**已落地**）

①「信号强度掉下去了 / 乘号是叉子」②「擦弹一点也不涨信号强度」③④「击破条左端固定 + 有阴影 / 偏左」
⑤「击破条衰减速度有问题」⑥「阴影得是右下角」⑦「压暗这个涉及到 UI 统一，这个不能复刻。是那个靠近透明吧」
⇒ 逐条根因与改法：**`TUNING.md` §2.6f-2**（对用户的那份）+ **`AUDIT.md` §DC**（工程侧 DC-1…DC-6；
含两条订正声明：脚本 104 的 `pos` = 矩形**中心**（`AUDIT` §AL20 的「矩形默认左上角」那行**是错的**）、
以及 `faith_core.h` 的 `FAITH_SIGNAL_LOSS_OFFLINE` **3000 → 300**（10× 量纲手误，**请衰减会话复核**））。
⑥ = 影的垂直偏移被中心摆位算式吃掉（两条矩形中心重合 ⇒ 影被整条盖住）；⑦ = 压暗改 **C 驱动**
（**门限由本块矩形每帧推**（默认锚点 ⇒ x −95.5..−1.5 / y 416..446）、进 = 落在块内 / 出 = 出到外扩 16 px、
终点 32 / 每帧 ±51；❓ 用户口径的**跨卡**统一**口径** —— ⚠ 第一版把它读成「抄 TH11 的 −64/400/384」是错的，
那三条是 TH11 自己块的沿、与我们块无关；脚本里那几条 `alphaTime` 保留为一手值记录、已不被触发；
§2.6f-3 有 vs 两作一手的对照表）。
下一跑看五行新日志：`signal/sig:`（读擦弹 → 差分 → 档位 → 推进 → 谁在画）、
`mof/combo:`（连击槽前后值 + 三条门 + 是否走中性）、`faith_hud:`（画出去的行 1/2/3）、
`faith_hud: 行3 矩形 亮@… 影@… ⇒ 影−亮 = (+2,+2)`、`faith_hud: 压暗 L… a=…`。


## 1w. ★★ 110 / 111 的「灵击 → 决死」（**决死 B 形态**；用户 2026-09-15 口径，✅ 已实装 2026-09-15c）

用户口径（逐字）：「**基本语义上灵击==B**」「**所以灵击也得决死**」。

**做了什么**：两张卡的灵击（**库存 == 0** 时自干那一支）在**自机处于决死窗口**（`player+0x476ac == 4`，
一手 `th18:0x45d4e4` —— 全 exe 对 `+0x476ac` 的写点里**只有它写 4**）内按 B ⇒ 走**决死形态**：
**照常发动**（扣 1 档火力 / 消弹 / 伤害源 / 节奏一字不改）+ **调 `Player__cancel_impending_death`
`th18:0x45d590` 救命**。原生自己的「决死 B」= `th18:0x45c2a9` 那一支（`can_bomb 0x420420` →
`do_bomb 0x420360` → `0x45c2d7 call 0x45d590`），而库存 == 0 时 `can_bomb` 返回 0 ⇒ 原生整条不走 ——
本卡补的就是这个缺口。

| 落点 | 110 | 111 |
| --- | --- | --- |
| 形态状态（进 core，可单测）| `mof_bomb_form_t` + `mof_core_deathbomb_window()`（`mof_core.{h,c}`）| `signal_bomb_form_t` + `signal_core_deathbomb_window()`（`signal_core.{h,c}`）|
| 救命调用 | `mof_cancel_impending_death()`（`mof.c`）| `signal_cancel_impending_death()`（`signal.c`）|
| 日志 | `mof: 灵击发动（决死）…` / `mof: 决死窗口内按 B …` | `signal: 灵击（决死）-1 档 …` / `signal: 决死窗口内按 B …` |
| 诊断计数 | `stat_bomb_dbf`（诊断行 `bomb %d (dbf %d)`）| `stat_bomb_dbf`（诊断行同款）|
| 主机单测 | `tests/test_mof_core.c` 的「灵击 → 决死」块（含 `sim_bomb_faith()` 可执行模型）| `tests/test_signal_core.c` 同款（`sim_signal_bomb()`）|

**两条代价口径（★ 两作的一手不同，别互相搬）**：

1. **不扣 −3000 信仰** ✅ 一手：TH10 的 `Player::on_tick` 里 `call 0x412e70`（信仰减益）**只有
   `th10:0x425a1f` 一处**、在 **state 1（存活）** 支里；决死支（state 4 → `th10:0x425bd9`–`0x425c8e`）
   通篇没有它 ⇒ 与卡里既有的「**非决死**灵击 −3000」注释互为印证。111 侧本来就没有信仰扣除逻辑
   （TH11 的信仰惩罚只走 miss），救下来 ⇒ 窗口不耗尽 ⇒ 引擎**不广播 `+0x0c`** ⇒ 那条 miss 账不触发
   （一手同向：TH11 决死**不触发 `$MISS_COUNT++`、也不清 `$CAPTURE`**）。
2. **消弹入不入账**：**110 = 不记账**（`MOF_BOMB_DEATHBOMB_ACCOUNT` = 0）、
   **111 = 照常记账**（`SIGNAL_BOMB_BY_HIT_ACCOUNT` = 1，✅ 一手）。
   ⚠⚠ **这不是「两作相反」**：110 取 0 是 **❓ 自定**（任务/用户口径的保守取值，避免决死严格优于主动灵击），
   因为它引的那对地址（`th10:0x4258d4`/`th10:0x4258f0`）在本轮复核里**被推翻**：
   - `th10:0x408100` 的三个栈参是 `[esp+0x28]` 半径 / **`[esp+0x2c]` 撒不撒** / `[esp+0x30]` 条件消，
     撒的判据在 `th10:0x4081cc`–`0x4081e7`（≠0 ⇒ `call 0x41bb00` 撒 **type 8**）。两处第二刀**确实都压 0** ✔，
   - **但它们不在决死分支里**：`Player::on_tick` 的状态跳表 `th10:0x426344` = `[0]=0x425751`（**state 0 = 复活上升**）
     / `[1]=0x425964` / `[2]=0x425aa9` / `[3]=0x425d25` / **`[4]=0x425a96`（决死）**，
     而那两处落在 index 0 支（该支末尾 `0x425902: cmp [player+0x478],0x3c` → `mov [player+0x458],1` = 复活计时）⇒ 它们是**复活清弹**；
   - 真决死支 `0x425bd9` **自己一句消弹都没有**，只调 `Bomb::start`（`th10:0x425c3e`）⇒ 消弹由
     `Bomb::on_tick` → `th10:0x405ac0` 做，那里第二刀 = **`(~[player+0x378c]) & 1`**（`th10:0x405af1`–`0x405b18`）
     = **与普通灵击同一句** ⇒ **「TH10 决死不撒」不成立**，两边的一手**同向**（都照常）。
   ⇒ 想与一手对齐：把 `mof_core.h` 的 `MOF_BOMB_DEATHBOMB_ACCOUNT` 改成 1（111 侧不用动）。

**❓ 自定（111）**：决死形态**不开**荷取的退款窗口（`SIGNAL_BOMB_BY_HIT_REFUND` = 0）。那条特技的语义是
「用了灵击**只要不中弹**就不算用过」，判负条件是窗口内自机 state ∈ {2,4}；而决死形态的前提就是
**刚刚被打中** ⇒ 前提不成立。⚠ 注意：本条路里救命（把 state 写回 1）发生在**开窗之前** ⇒ 若照常开窗，
同帧的 `signal_support_nitori()`（⑥b）读到的是 `pstate == 1`、**判不负** ⇒ 窗口会活满 75 帧并真的退 1 档火力
（= 因为被打中而拿到退款）。所以不生效的那一档**也不收保险费**。

**明确不做**：不挂 `on_death_before_deathbomb`（卡虚表 `+0x10`）自动救人 —— 按
[`EVENTS-CONTRACT.md`](EVENTS-CONTRACT.md) §4.2/§5.2 的救援层顺序（**107 → 117 → 决死 B → 113**），
本卡扮演的是「**决死 B**」，语义就是**玩家在窗口内按 B**，只做 B 键这条路。

**第一跑要看的日志**（`make release` 之后）：
1. 中弹的那一帧：`mof: 决死窗口内按 B（state 4 == 4，库存 0）⇒ 本发走决死形态`（111 同款 `signal:` 前缀）
   → 紧接着 `mof: 灵击发动（决死）(power …)`；
2. 救下来了 ⇒ **命数不掉**、无敌 60 帧、灵击照常消弹/掉 boss 血；
3. 每 `MOF_LOG_EVERY` / `SIGNAL_LOG_EVERY` 帧的诊断行里 **`dbf` 计数 +1**（110 的 `bomb %d (dbf %d)`）；
4. 110 的决死形态下**信仰不出现 −3000**（信仰值只随绿点变）。

## 1y. 111 的**命碎撒道具**：每 2 段没死过 ⇒ 1 颗 `type 0x10`（用户口径；**✅ 已实装** 2026-09-15e）

> **用户口径（2026-09-15 逐字）**：「**算了，数值平衡一下，每两次这样掉 1 个命碎**」
> ⇒ 一手（TH11）的「每段攻击没死过 ⇒ 掉 1 个」改成 **每 2 段掉 1 个**（余数跨段保留）。
> 一手 = 1:1；**想改回 1:1 只改一个宏**：`native/cards/signal_core.h` 的 `SIGNAL_SEG_DROP_EVERY`（2 → 1）。

**这条同时订正了 111 的一条错注释**（原文：`native/cards/signal.c` 的文件头「明确不做」第 1 条）：

1. **载体错了**：命碎道具是 **item type `0x10`**（EXTEND2 卡道具 → `CardLifeFragment` id 3），
   **不是 type 4** —— type 4 连 TH18 的道具分派 switch 里都没有（`engine/card/th18/05-shop-and-money.md` §8）；
   `cards/collect2x_core.h` 早就把 type 4/6 记作「旧引擎血统残留」。
2. **「原生已经在发」不成立**：把 TH18 全部 21 个 `st*.ecl` 解出来扫掉落指令（`thecl -d 18 -x`），
   **`dropMain(0x10)` 全游戏只有一处** —— `st04mbs.ecl` 的 `MBossEscape()`（`0x15a4`），
   **8 个 `st0Xbs.ecl`（关底 boss）里一次都没有**。顺带同形证据：TH11 的 `MBossEscape` 也在收尾 `dropMain(5)`。

**实装**（卡侧只接线，判据全在 core）：

| 件 | 位置 | 管什么 |
| --- | --- | --- |
| 段机 | `native/cards/signal_core.c` 的 `signal_seg_tick()` / `signal_seg_init()` / `signal_seg_rebase()` | 段边界去抖 / 死亡归属 / 余数 / 满 N 出账（纯逻辑，主机单测）|
| 读引擎 + 撒道具 | `native/cards/signal.c`：`signal_boss_enemy()` / `signal_phase_sig()` / `signal_seg_drop()`，`on_tick_2` 的 ④b | boss 指针 / 相位槽指纹 / 符卡位 / 命数差分 → `ce_spawn_item(0x10, pos, −π/2, 2.2)` |

**段边界（❓ 自定：TH18 没有「段」这一级对象，边界是我们重建的）**= 三个原生只读信号的任一跳变：
**boss 在场 1→0**（最后一段收尾）/ **符卡位 `CE_SPELL_FLAGS` bit0 边沿**（`0x42a320` 置、`0x42a780` 清）/
**相位槽指纹变化**（活动 `zEnemyInterrupt`，`zEnemy+0x6374`、stride `0x88`；本轮在 `th18_full.asm` 上
复核了 `0x42ef33`/`0x42ef46`）—— 5 帧去抖合并同一次切换的多个边沿。
⚠ **章号（ECL 524）切不出这一级**：TH18 关底 boss 全程只有 `43` 一个值
（[`../engine/chapter/th18/01-chapter-residuals.md`](../../../engine/chapter/th18/01-chapter-residuals.md) §3/§4）；
**粒度与一手同**：本轮把 TH11 的 46 处 `dropMain(5)` 按子程序归类 —— **全在 boss / 道中 boss 段，普通妖精波一个都没有**。

**「本段没死过」**= 本段内**命数减少过**（复用 `signal.c` 既有的 `last_lives` 差分）。
⚠ 救援层语义天然吻合：**107 / 117 / 113 救你一命 ⇒ 命数不减 ⇒ 记为「没死」**
（与 TH11 一手同向：`$CAPTURE` 只在决死窗口过期的**真死**那一路被清，灵击/决死救回都不清）。

**第一跑要看什么**：

1. 每撒一颗打一行 `signal: 命碎 -> item type 0x10 @ boss 位置（第 N 段没死过 / 已结算 M 段 / 本颗是第 K 颗，余 R）`；
   ⚠ 出现 `@ **自机位置（boss 指针取不到）**` = boss 刚被打死那一类边角（不是 bug，如实记的）。
2. 每 600 帧一行 `signal/frag: segs … (clean …) drops … spawn … picked …` ——
   **`drops` 应当 ≈ `clean / 2`**、`spawn == drops`、`picked` 是实际捡到的颗数。
3. 逐关对段数：关底 boss 一段非符 + 一张符卡各算 1 段（`st01` = Boss1/BossCard1/Boss2/BossCard2 → 4 段 + Dead）。
4. 捡到之后**全走原生**：3 个碎片 = 1 残（阈值表 `0x4b40c8` 本轮 read 复核 = 全 3），卡侧一个字节都不写。

**闸门**：`cd native && make test-host`（`test_signal_core` **724 断言**，其中 §17 是段机）
+ `make check`（`x87 in our objects: 0` / `capcheck` / `engine_abi_check` / `check-docs` 全绿）。
**❓ 自定项登记**：[`TUNING.md`](TUNING.md) §2.1e（111-10…111-14 五条近似的代价）。

## 2C. ★★★ TH10（110）**信仰 / 连击**全表（用户 2026-09-15 口径，**权威**）

> 用户原话（逐字，分两段）：
>
> 「**信仰**：左下角数字为当前信仰值，信仰值下方为连击条。
> **信仰初始值为 50000**，增加方法为以下 3 种道具：
> · **大信仰点**，击破固定敌人获得，E,N +5000，H +8000，L +10000
> · **小信仰点**，满 p 击破道中带 p 敌人，+100
> · **微信仰点**，消弹获得，+10
>
> **微信仰点的获取有以下 4 种方法**：
> 1. 道中与 boss 战非符灵击消弹
> 2. 击破 boss 非符与符卡的消弹
> 3. 全避 boss 非符的消弹
> 4. 二、三、五面中 boss 登场消弹
>
> **信仰在以下情况会减少（下限值为 50000）**：
> 1. miss，减（当前信仰−50000）/3
> 2. **连击槽为空时**：若屏幕上无敌人：减 **10800/s**，若屏幕上有敌人：减 **600/s**
> 3. 使用非决死灵击：减 3000
> 4. **全避 boss 非符、符卡：减 30000（时符必减）**
>
> **连击**：完整的信仰槽 = **130f**（60f = 1s）；
> 大信仰点 **+130f**；蓝点 +100f；小p、小信仰点 +60f；大p、击杀非boss单位 +20f；微信仰点 +3f；1UP、击杀boss +0f；
> **boss 战中连击槽不会减少**。」

### ★ 「下限 50000」与「下限 0」是同一件事的两种说法（用户 2026-09-15 追加口径）
> 「**下限是 0，这里是额外最大得点的显示**。」

- **信仰**（游戏内部那个值）= 基础 **50000** + Δ ⇒ 它的下限是 **50000**（本表"减少"那一节说的就是它）；
- **我们在左下角块里显示的数字 = 额外最大得点** = `信仰 − 50000`（= **Δ_10**，下限 **0**）+ **Δ_11**
  ⇒ 它的下限是 **0**（与 §2A ④ 一致：只画两份"额外"之和，**基础不进这块**）；
- ⇒ 两个"下限"**不矛盾**：基础永远在，显示的是"在基础之上多出来的部分"。
  ⚠ 实现时**不要**去用 `faith_slot_total()`（那是"基础 + Δ"）；要画的是 **`delta` 之和**。

### 与一手 / 代码的对照（哪些已经对上、哪些要改）

| 口径 | 我们现状 | 判定 |
| --- | --- | --- |
| 初始 50000；大 +5000/8000/10000（难度表）；小 +100；微 +10 | `FAITH_BASE_110` / `faith_big_mof()` / 两档加值 | ✅ 全部一致（2026-09-15 复核） |
| miss ⇒ 减 (信仰−50000)/3 | `FAITH_MISS_DIV = 3`（一手 `th10:0x4269d0`） | ✅ 一致 |
| 连击槽空 + 无敌人 ⇒ **−10800/s** | ❓ **缺**（旧实现只做了"有敌人 −600/s"） | ⏳ 补 |
| 连击槽空 + 有敌人 ⇒ **−600/s** | `FAITH_EMPTY_ENEMY_PER_SEC 600`（注释自称"❓ 自定无一手"） | ✅ **数值一致**；注释要改成"用户口径 + 一手速率字段两写点（1 vs 18 存储/帧 = 600 vs 10800 显示/s）" |
| 非决死灵击 ⇒ −3000 | `FAITH_REDUCE_BOMB`（一手 `th10:0x425a15`） | ✅ 一致 |
| **全避 boss 非符/符卡 ⇒ −30000（时符必减）** | ❓ 未实现；🟡 悬项 `th10:0x412932` 的**触发条件**由此**被解** | ⏳ 补 |
| 连击满槽 130f；大 +130f；蓝点 +100f；小p/小信仰 +60f；大p/击杀非boss +20f；微 +3f；1UP/击杀boss +0 | `mof_core` 连击表（一手：大 `0x41b5c5 push 0x78` = **120**） | ⚠ **130 vs 120** —— 疑为「大信仰点 = **击破固定敌人**掉 ⇒ 拾取 +120 **加** 击破 `0x40e673` 的 +10 = 130」，**待核** |
| boss 战中连击槽不减少 | `mof_core_combo_tick()` 的 `!in_boss_fight` | ✅ 一致（但**对话中 / 关卡前 90 帧**也多一条冻结，见 §2B 的衰减门复核） |

### 连带影响
- **UI（§2A）**：左下角 = 数字（当前信仰）+ 下方连击条 → 与本表逐条一致；组合 UI 里"行2 = Δ_10+Δ_11"不改，
  只是 Δ_10 的**衰减**要按本表实现（含两档速率与四条扣减）。
- **微信仰点的 4 条来源**：我们现在只接得到"我们自己放灵击"那条（见 `faith.c` 头注）。
  第 2/3/4 条（击破非符/符卡的消弹、全避的消弹、二三五面中 boss 登场消弹）**都是引擎/ECL 侧事件**，
  要接就得找对应断点 —— **登记为待办**（不是 ❓ 自定，是"还没接线"）。


## 2D. ★★ 跨卡 UI 统一：HUD「压暗」—— **动态统一、区域按几何**（2026-09-15g 落地 / **2026-09-15h 收尾订正**，✅ 等实跑）

**一句话**：6 块 HUD（116 天空璋 / 117 鬼形兽 / 107 妖妖梦 / 113 神灵庙 / 115 绀珠传 / **110+111 组合 UI**）原来各写各的
「自机贴近 HUD 就压暗」，现在**动态**统一（终点 alpha **32** / 起始 255 / 每帧 **±51** / 滞回带 **16 px**），
**区域**按**每块自己那块 HUD 的矩形**推（**进 = 矩形 + 28**、**出 = 矩形 + 44**；换算 `自机 = 表面/2 − (320, 16)`）。

- **性质 = ❓ 用户口径**（用户 2026-09-15：「压暗这个涉及到 UI 统一，这个不能复刻。是那个靠近透明吧」；
  父 agent 的读法：「本质 = 别让 HUD 挡住自机；HUD 画在哪判据就跟着哪」）⇒ **不是复刻**，各卡原一手值全部留档在文件注释里。
- **2026-09-15h 父 agent 两处订正（本轮已落地）**：
  ① **进 = 矩形 + 边距 28**（15g 那版「进 = 矩形**之内**」裁错 —— 块在游玩区下沿之下，只剩 4–10 px 高；
     一手各作的门都落在它们自己那块上沿之上 24–32 px ⇒ 语义本来就是「矩形 + 边距」）；
  ② **换算钉死 320**（`224` 是过时记法）⇒ 110/111 那块的 x 门从 `−95.5..−1.5` 订正为 **`−191.5..−97.5`**；
  ③ 110/111 那块**并进共享件**（`faith_hud.c` 里自己那份 `fh_dim_limit()` + `FH_DIM_*` 已删）。
- **实现**：`native/cards/hud_dim.{h,c}`（**唯一实现**，纯逻辑；`HUD_DIM_MARGIN 28` / `HUD_DIM_BAND 16` 都是具名常量）
  + `native/tests/test_hud_dim_core.c`（**298 断言**，含六块真块的「可触发区」不变式）
  + `native/tests/test_faith_hud_core.c` §H（**4106 断言**，钉 110/111 的 320 换算与门限）。
- **登记**：`TUNING.md` §7（统一项 + 各卡块矩形→门限表 + 原一手值表 + 待办）、`AUDIT.md` §DD（15g 的改动表 + **§DD-7 收尾订正**）。
- **闸门**：`make test-host` / `make dll`（0 警告）/ `make check` **rc=0**（`x87 in objects: 0`、33 exports、
  `capcheck` 全绿、`engine_abi_check` 7 条 ✅、`check-docs` 全绿）。**没跑** `make release` / DLL 同步（父 agent 统一发布）。

**实跑时看这几条**（每条都有机器判据日志）：

1. 自机**靠近** 116/107 那块（左下角，进 = 矩形 + 28）⇒ 3–5 帧内整块淡到 32；离开（出到矩形 + 44 之外）⇒ 5 帧内回 255。
   日志里 `faith_hud: 压暗 L… a=…` / 各卡的诊断行会打**本帧推出来的矩形**与边距。
2. **左下角那几块现在真的点得亮**（进 = 矩形 + 28 ⇒ 116 ≈ 72×32 px、117 ≈ **32.5×38 px**、107 ≈ 120×34 px 的可触发区）。
   若实跑觉得「太早就淡了」，改的是 **`HUD_DIM_MARGIN`**（28 → 24，一个宏改所有卡）；别再退回「矩形之内」。
3. **115 的面板**按面板自己的矩形 + 28 淡（进 = 靠到面板矩形外扩 28 之内、出到 +44 之外）：
   进的一侧比原来的自定带（矩形各放 32）**略紧 4–19 px**、出的一侧宽 12 px（滞回 0 → 16）；「站面板后面看不清弹」应该仍然成立。
4. **113 的指示槽**：门 = 矩形 + 28（进 `y ≥ 376`）/ 出 = 矩形 + 44（`y < 360`）；**深度 32 早就是统一的**，
   但**过渡仍是脚本的 10 / 30 帧**（assets 那批脚本归另一会话）⇒ 实跑会觉得它比别的卡慢半拍，见 §DD-6 第 1 条。
5. **★ 待办（卡 80 的 owner 走 `AUDIT.md` §BE）：`cards/stock.c` 那处 `224`** —— 
   `PLAYER_TEXT_CX = 224.0f` / `PLAYER_TEXT_CY = 16.0f` 与 `faith_hud.c` 原先那处**同源同错**（都按旧视口
   `(32,16,384,448)` 算的），它把这套换算用在**自机头顶那两行字**上 ⇒ 按已钉死的 **320**，那两行字现在
   **偏左 96 自机 px（= 192 表面 px）**。本会话（压暗 owner）**只读判断、一个字节没动**：
   换算 320 本身是一手钉死的 ✅，但「实跑看起来偏左」没在实机上量过（该处注释自己标着「**待实跑微调**」）⇒ **🟡**。
   建议改法：`PLAYER_TEXT_CX` 改 320（`PLAYER_TEXT_CY` 16 不变），改完实跑核对字是否落在自机正中上方。
   登记：本行 + `AUDIT.md` §DD-7-4 + `TUNING.md` §7.4 第 5 条。
6. ★★★ **2026-09-15i/j 收尾（本会话 = 唯一 owner）：117 压暗矩形 + 过渡帧根治 + 运行时判据** ——
   ① **117 的压暗矩形（i 勘误 → j 改运行时值）**：`hud_fade_tick()` 现在**每帧从运行时槽位值推**：
      `(s_slot.x − 24, s_slot.y + DY + 24) + 192×48`。**不许再从 `ANIMAL_HUD_PIN_X` 纸面推** ——
      PIN_X 是**首格的格心**（历史注释写成「整排左下角」），而且它只是**认领时交给槽位表的请求值**；
      卡真正画的是 `ce_hud_slot_get(117)` 还回来的 `s_slot.x`。pin = 88 时可见整排 = **(64, 928)+192×48**。
      **顺带修洞**：`hud_slot_sync()` 原来读的 `s->hud_slot_ok` 全仓没有写点 ⇒ `ce_hud_slot_get()` 从没被调用过
      （「每帧取最新锚点」是空注释），现在读文件级 `s_slot_ok` + get 失败显式退回兜底。
   ② **★ 实跑要对账的一条（用户口径「重叠」与代码 64..256 对不上）**：父 agent 问过用户
      「117 是压在 116/113 身上还是在其左边」，**用户答「压在它们身上（重叠）」**；
      而按代码 pin=88 ⇒ 可见 64..256（与 113 的 248 只擦边 8 px、与 116 的 263 有 7 px 缝）。
      ⇒ 不再拍脑袋改数值：**加了一行运行时日志当唯一判据**（认领当场 + 每 300 帧）：
      `wba_wc: hud geom [claim|tick] claim_ok=… path=tbl|fallback | slot (x, y) + w×h |
       cells …（x min..max）| vis(dim) (…) + 192x48 | consts (…) | drift=0`
      **请用户实跑后拿 `vis(dim)` 的两个 x 与他在画面里看到的 5 格左右两端比一下**：
      · 一致（64..256）⇒ 是**摆位口径**（`ANIMAL_HUD_PIN_X` / 2026-09-10 的「左移 193」）要重定，`hud_dim` 这层不动；
      · 日志 `slot (248, …)`（`path=tbl`）⇒ 矩形会**自动跟着表走**（已改成运行时值），再核对 116/113 的矩形；
      · 日志 64..256 而用户坚持看到重叠 ⇒ 是**画出去的位置**与 `s_slot` 不符 ⇒ 查 anm 坐标空间/实机（别改矩形凑）。
      ✅ 附注：**117 与 116/113 的压暗矩形互相重叠是正常的**（各卡只按自己那块判）——别当 bug 修。
   ③ **过渡帧根治**：脚本 123–127（116）+ 175–178（113，**四支一起**，`trance.c:3658` 对四支发 4/5）→
      `alphaTime(4, 0, 32)` / `alphaTime(4, 0, 255)`；`season.c` 的 `min(脚本 α, 32)` 兜底**已删**，
      替身 = `native/tools/anm_hud_alpha_check.py`（`make check`）。
      ★ 2026-09-15j：**`HUD_DIM_STEP` 51 → 56**（255↔32 恰好 **4 帧**）⇒ C 驱动与脚本驱动**同帧数**（父 agent 指令）。
      `make anm` 已重建 `ability.anm`（`ORDER.txt` 前 186 行逐行未变 ✅）⇒ **DLL 与 `ability.anm` 必须同批发布**。
      登记：本条 + `AUDIT.md` §DD-9（DD-9-1..5）+ `TUNING.md` §7.1/§7.2/§7.4 第 1/2/4/6/7 条。
   ④ **闸门 `make check` rc=0**（2026-09-15j 复跑；含 `x87: 0`、33 exports、`capcheck`/`engine_abi_check`/
      `check-docs`(293)/`anm_hud_alpha_check`(24) 与全部宿主单测）。

## 2E. ★★ 111 的第 6 条支援特技（魔理沙×爱丽丝）：**灵击 = 别人的一半 + P 点 +0.02/+0.03 交替**（2026-09-15g，✅ 已落地，等实跑）

**一句话**：111 的六条支援特技原来只实装了五条，第 6 条（爱丽丝）被判成「结构上无可表达物」而搁置
（当时以为是「子机上限 8」，而 TH18 原生上限是 4）。用户 2026-09-15 给了真口径 ⇒ 那条判断**作废**：
**子机仍是 4 个**，爱丽丝真正给的是**两条数值**：**灵击消耗减半**、**P 点收益 = +0.025 档**。

- **口径（用户逐字）**：「**还是 4 个子机**，只是**灵击改成 0.5p 一次**；但此时需要**把火力变成 +0.025**，
  其中 .5 需要额外存储，或者按照 **+0.02 +0.03 交替**」+「不知道 P 点能不能改成 +0.025 一颗，能自然行，
  **不能的话 3 最干净**」。**父子 agent 已裁**：引擎火力字段是 1/100 档的整数 ⇒ 0.025 档 = 2.5 单位
  **无法精确表示** ⇒ 走「**+0.02 / +0.03 交替**」（平均 2.5 单位 = 0.025 档，**不需要额外存储**）。
- **性质分两层（★ 父 agent 2026-09-15g 裁，别整条写成「用户口径」）**：
  · **比例那层 = ✅ 一手复刻**：本轮**自己解包核过** TH11 的 sht —— 组合表 `th11:0x4a7a90[3]` = `"pl01a.sht"`
    （= 魔理沙 A = 爱丽丝），该文件 `+0x24` = **12** / `+0x20` = **8** ⇒ 满槽 **96** ⇒ 一发 **1/8**；
    其余五机 20/4 ⇒ 满槽 80 ⇒ 一发 **1/4** ⇒ **恰好是别人的一半**（满槽 8 发 vs 4 发）。
  · **绝对档位那层 = ❓ 我们的映射**：TH11 的消耗是 per-shot 字段、HUD 自己按 `+0x24` 归一（爱丽丝自己的刻度里
    一发就是 1.00、满火力显示 8.00），而 TH18 的一档是跨机体统一的 100 单位 ⇒ 只能落成「**1 档的一半 = 50 单位**」。
  · **P 点减半 = 🟡 用户口径**（一手 TH11 的小 P 是 +0.05 档）。
  ⚠ 收窄后的说法：**「绝对数 0.50」不在表里**（表里是 12；12/20 = 0.60 是**跨机体**比值，别当近邻），
  但 **「一半」这个比例是一手**。全过程在 `TUNING.md` §2.1f 与 `native/cards/signal_core.h` 的 ④ 段。
- **实现**：`native/cards/signal_core.{h,c}`（`signal_core_alice_power_spend()` / `signal_alice_p_total()` /
  `signal_alice_p_topup()`，交替相位住在 core 状态 `signal_alice_t.alt` ⇒ 主机单测钉得住）
  + `native/cards/signal.c`（**两处既有钩子各加一个机体门**：`signal_bomb()` 与 `signal_on_item_commit()`；
  **没新开断点**、没动 `sdk.*`、没动 110 的 `mof_core.*`）。单测 `native/tests/test_signal_core.c` §18（+73 断言）。
- **两处 ❓ 自定（登记 `TUNING.md` §2.1f 的 111-15/16/17/18）**：① 那条灵击的**门槛 = 「能扣就扣」**
  （`power ≥ 155` = 1.55 档 = 50 + 下限 105）—— **父 agent 2026-09-15g 已拍板**：这与本卡**既有**的普通灵击
  门槛**同构**（常态门槛恒等于「扣款 + 下限」= 100 + 105 = 2.00 档，所以 105 永不被触发），爱丽丝只换了**扣款**；
  不取 `≥50` 是因为那样会**被下限反向抬起来 = 白嫖灵击**；② **下限照旧 1.05 档**（门槛已保证它永不触发）；
  ③ 交替**第 1 颗走 LO(+0.02)**（顺序不影响平均值）；④ 想改成用户追加口径里的「平 +0.03」= 把
  `SIGNAL_ALICE_P_TOTAL_LO/HI` 都改成 3（一处两行）。
- **★ 110 同装：双倍？没有。减益被绕过？2026-09-15h 已修 ✅**
  · **不会双倍**（结构性）：这一层是「**每颗道具全局只补一次**」，落笔者由 `mof_core_p_owner()` 选举，
    111 在 110 在场时**直接让位、一个字节都不写**（它**连策略都不问**）。
  · **减益照样生效**：补值改成走**共享策略** —— 落点 = `faith_core.h` 的 `faith_core_p_topup_total()`
    （纯逻辑：常态 5 / 爱丽丝 2,3 交替）+ `faith.h`/`faith.c` 的 `faith_p_topup_take()`（判据 =
    `faith_registered_mask()` 的 111 位 ∧ `CE_ADDR_CHARACTER == 1`，**不用槽值**；**相位只有这一份**
    `s_p_alt`）+ `mof_core.h` 的 `mof_core_p_topup_units()`（旧签名 `mof_core_p_topup()` 行为逐字节不变）。
    接线：`mof.c`（110，恒落笔者）与 `signal.c`（111）都只做「问策略 → 用它当补值」，**谁都不自己算 2/3**；
    111 原来那份相位（`signal_alice_t.alt`）**已退役**。
  · **不变式**：策略**只有落笔者调** ⇒ **每颗道具只推进一次相位**（`test_mof_core.c` 的 `sim_p_commit_policy()`
    钉 `takes == 10` / `alt.items == takes`；`test_faith_core.c` 钉平均 2.5 与「不生效 / 满火力都不吃相位格」）。
- **顺手订正**：`signal_bomb()` 给荷取退款窗的「在保金额」原来传 `CE_POWER_PER_LEVEL()`（100），
  现在传**真实扣款**（爱丽丝 50；常态在 2.00 档边界上被下限夹成 95）—— 否则爱丽丝那条会**多退一倍**。
- **闸门**：`make test-host`（`test_signal_core` 772 断言）/ `make dll`（0 警告）/ `make check` **rc=0**
  （`x87 in our objects: 0`、`capcheck` 全绿、`engine_abi_check` 7 条 ✅、`check-docs` 293 文件 ✅）。
  **没跑** `make release` / `sync_modkit_dll.sh`（发布归父 agent）。

**实跑时看这四条**（机器判据都在日志里）：

1. **机体必须是 TH18 魔理沙(1)**，且**只装 111**（装了 110 ⇒ P 点那条让位、只剩减半的灵击）。
2. 火力 ≥1.55 档时按 B（库存 0）⇒ `signal: 灵击 -0.50 档 (power 155 -> 105), …`；**1.54 档时按 B 一条日志都没有**。
3. 吃 10 颗小 P ⇒ 火力总共只涨 **0.25 档**（+0.02/+0.03 交替）；600 帧那行 `signal/support: … alice N (palt 10: 5/5)`
   的 `lo/hi` 之差恒 ≤ 1。
4. **双卡同装（110 + 111）**：P 点照样按爱丽丝减半（落笔者是 110，但 110 问的是**同一个**共享策略）
   ⇒ 单装 111 与双装的画面应当**一模一样**（这是本轮修好的那一条）。

## 2F. ★★ 111 的后六条支援特技：**咲夜(2) = 大小姐 + 二小姐 + 红美铃** / **早苗(3) = 诹访子 + 神奈子 + 恋恋**（2026-09-15i，✅ 已落地，等实跑）

**一句话**：111 的支援特技原来只做了灵梦/魔理沙两组的六条（那两份是 TH11 一手），本轮补齐用户拍板的
另外两组**各三条**；这两组伙伴**在 TH11 里不存在**（组合号只有 4 机 × 3 子机），除标明一手的几处外**全部是用户口径**。

- **口径（用户 2026-09-15 逐字）**：
  「**咲夜**的话，**大小姐二小姐红美铃**：· **大小姐**：擦弹会持续触发（除了激光本来就持续触发，单科弹幕擦弹是有"无敌帧"的）
   · **二小姐**：按住 shift 时会蓄力，连续蓄力 5s 后放开 shift 可消除 2×擦弹圈半径范围的屏幕
   · **红美铃**：信号强度不会低于 ×0.5，信号强度增长速度 ×1.5
   **早苗**的话，**诹访子神奈子恋恋**：· **诹访子**：贴紧板底的时候不会扣除信号强度
   · **神奈子**：收点线下降 · **恋恋**：放开全键位的时候（除移动键）给 1 帧无敌（**参考 th18 原生文文卡**）」
  ＋ 父 agent 当天追加（同一口径的细化）：「**擦弹 1 帧，然后不计算擦弹 2 帧吧，对于每个子弹执行这个操作**」。
- **一手（本轮自己在 `local/th18.v1.00a/th18.exe` 上取到的四处）**：
  ① 擦弹**一生一次**（`0x42496f` 判 / `0x424984 or [bullet+0x20],4`）+ 两种判定式（圆 `0x45cd90` / 矩形 `0x45cc20`）
  + **激光不在弹池里**（三个激光擦弹点 `0x44ab35`/`0x44cfef`/`0x44fa53`）；
  ② **板底 = 引擎自己的自机 y 下界 `0xd800`**（432.0 px @1/128，`0x45b7e7`）——不是我们挑的常数；
  ③ **收点线** `player+0x47998` 的全 exe 写点只有两个（`0x45ae8f` = 128.0 / `0x40c275` = **224.0 = 原生卡**）；
  ④ ★★ **恋恋那条的参考 = 原生 29 号卡 Aya（SPEEDQUEEN）**：`CardAya__on_tick` **`0x40c7f0`** 的
  `0x40ca95`–`0x40cb2a` —— 条件 = `last_dir == 0 && move_dir != 0 && !focusing`（而 `last_dir` 在**按住射击**时
  被每帧强制 0，`0x40c853`），效果 = **给 `player+0x47774` 的无敌计时器 `+4.0` 帧**（`0x4b9248` = 4.0）。
  ⚠ 任务口径里提到的 `0x40D2E0` **不是文文卡**（那是原生 31 Kaguya 的 `spawn_item`）。
  ★★ **2026-09-16 补完（用户追问「所以理论上原生文文卡可以刷出一堆无敌时间是吗」时取到的）**：
  那条「按住 Z ⇒ `last_dir` 被钉成 0」的**上游**找到了 —— `0x40c80a` 读的 `player+0x4779c` bit `0x200`
  由 `Player::move` 的 `0x45ea17` 置（门 = 输入 `0x4ca428` **bit0 = Z**），且 `Player::on_tick` 每帧先清
  （`0x45c921`）⇒ 那个位 = 「这一帧按着 Z」；位置着时卡片**本帧不加无敌、只返回**。
  ⇒ 原生**可刷**：**点 Z**（按住方向、每 2 帧按一下 Z）或**抖方向**，都是 +4/2 帧 ⇒ **净 +2 帧/帧**、
  计时器无上限（`addss`，全段无闸）；**且攒到之后开火 / 按 Shift 都不会取消**（写点普查见那篇 §5）。
  完整一手回流到新篇 `engine/card/th18/12-card29-aya-speedqueen.md`（条件 / 量 / 两条刷法 / 写点普查 / 社区对照）。
- **实现**：`native/cards/signal_core.{h,c}`（六条的状态与纯逻辑：`signal_remilia_tick()` / `signal_flandre_tick()` /
  `signal_core_step_mod()` / `signal_core_at_board_bottom()` / `signal_kanako_tick()` / `signal_koishi_tick()`）
  + `native/cards/signal.c`（`signal_step_mod_fill()` 扫弹池 + 两条修饰；`signal_support_sakuya()` /
  `signal_support_sanae()`；⑥b 分派补两行；`signal/support2:` 诊断行）。
  **没动** `sdk.h` / `sdk_core.*` / `engine.h` / `faith_core.*` / `mof_core.*`（新入口 `m == NULL` ⇒ 与旧路逐值相同，有单测对拍）。
- **★ 两处关键工程选择（父 agent 专门点了名）**：
  · **① 的收益落法 = 不写引擎的共享擦弹计数 `0x4ccd0c`**（写了会替 HUD 与**别的卡**改账；SDK 也没有擦弹声明层）
    ⇒ 额外次数**直接加进喂给信号槽的擦弹数**（与原生帧差同一本账）+ 让 `mof_core_graze_mult()` 吃「原生计数 + 累积额外」。
    **实跑分辨**：`signal/sig:` 的 `graze`（原生计数）**不涨是预期**，`signal/support2: remilia K` 会涨。
  · **⑤ 神奈子暂时直写 `player+0x47998`**，且**收在一处**，旁边标 `⏳ 待迁移到共享 PoC 层（会话 2e02b810）`
    （用户当天追加了口径：要抽共享 PoC 层 + 开局指示线也跟着变）。迁移点只有三个（读现值 / 写生效值 / 写回基准）。
- **❓ 自定 / 用户口径（全表在 `TUNING.md` §2.1g）**：逐弹周期 1+2 帧（用户口径）、300 帧蓄力与 2× 半径、
  86.00 px 的半径算法、红美铃的 ×1.5 与 50000 下限、下移 32 px（恋恋那条的无敌量**照原生 4 帧**，见下）、
  ② 的中断条件（中弹清零 / 方向键不参与）、**六条不另收费**（用户口径里没有价）。
- **闸门**：`make check` **rc=0**（`x87 in our objects: 0`、`capcheck` 全绿、`engine_abi_check` 7 条 ✅、
  `check-docs` 293 文件 ✅）；`test_signal_core` **1175 断言 0 失败**（+403）。
  **没跑** `make release` / `sync_modkit_dll.sh`（发布归父 agent）。
  ⚠ 卡状态 `sizeof(signal_card_t)` = **480 / 496** 字节 ⇒ 余量只剩 16 字节（`_Static_assert` 兜住了上限）。

- **★ 用户 2026-09-15 追加订正（本轮第二次）**：⑥ 恋恋的无敌量改成**原生的 4 帧**（用户原话
  「**原生量是 4 帧吗？那就按照原生量**」）⇒ 这一条的载体 / 条件 / **量**现在**全是复刻**
  （`CardAya__on_tick` `0x40c7f0` 的 `0x40ca95`–`0x40cb2a`，`0x40cb16` 的 `addss xmm1,[0x4b9248]` = +4.0 帧）。
  唯一保留的 ❓ = 我们**多覆盖**了一条「六个动作键全放开的下降沿」（原生覆盖不到「站着不动把键全放开」），
  那一路的计数单独印在 `signal/support2:` 的 `usr`。

**实跑时看这六条**（详细判读在 `TUNING.md` §2.1g 末尾）：

1. **咲夜(2)**：冲进弹幕贴着子弹飞 ⇒ `remilia` 涨、`signal/sig:` 的 `graze` 不涨（预期）；
2. **二小姐**：按住 Shift **5 s** 再松开 ⇒ `signal: 二小姐·蓄力满 300 帧后松手 ⇒ 清屏 r=86.00 px … cancelled N`；
3. **红美铃**：信号槽掉不到 50.00 以下，线上增速 1.5×（<50.00 时一帧到 75.00）；
4. **早苗(3)**：拉到板底（y = 432）⇒ `suwako 1`，线下不再 −0.30/帧；
5. **神奈子**：`kanako` 每帧 +1、`resnap 0`；收点线 128 → 160 px（删卡/换关写回 128）；
6. **恋恋**：按住方向 + Z，再**松开 Z/Shift** ⇒ `signal: 恋恋·放开键 ⇒ 4 帧无敌`（`nat` 计数；量 = 原生 Aya 卡的 `+4.0` 帧）。
7. **恋恋的验收点（本轮新增）**：**以 1 帧精度抖「起步」**（按住方向、反复点按 Z / Shift 制造起步沿）⇒ **不应**出现永久无敌 —— 无敌**只有 4 帧**、抖不动、计时器**不单调上升**。这就是「我们不累加」这条生效的证据（原生 Aya 卡在同样操作下会累加成永久无敌；用户 2026-09-15 拍板「保持现状（不累加，安全）」，登记在 `TUNING.md` §2.1g / `AUDIT.md` §DE-3b）。

## 2G. ★★★ **PoC 高度层**（收点线 `player+0x47998`）= 新共享件（2026-09-15j，✅ 已落地，等实跑）

> **用户口径（2026-09-15 逐字）**：「有一张**原生卡直接是降低 PoC 高度**，并且 **th14 卡也得追这个高度，
> 包括信号系统**。**我觉得得抽出一个东西来了。** 特别地，**PoC 高度有一个指示线，在开局显示，
> 如果开局 PoC 高度变了，显示也得变**。」—— 本会话是这次改动的唯一 owner。

- **一手（新篇）**：[`engine/player/th18/05-poc-height-and-indicator.md`](../../../engine/player/th18/05-poc-height-and-indicator.md)。
  一句话：字段只有**两个写点**（`Player__reset` 128.0 `0x45ae8f` / 原生卡 **22 ITEM_LINE = CardKanako** 的
  `on_load` 224.0 `0x40c275`，**没有每帧写点**）、**四个引擎读点**（全是「自机在线上还是线下」）、
  **一条开局指示线**（`front.anm` **script 0x39**，位置 `0x43afe1` **现读字段**：`y = poc×2 − 80`，
  可见性 = `Gui+0x194` bit **0x2000**，唯一写者 = 那张原生卡 `0x40c288`；script57 自己 198 帧后 `delete()`）。
- **共享件 API**（`sdk_core.h` 的 PoC 块 + `sdk.h` 的包装；纯逻辑在 `sdk_core.c`，有主机单测）：
  `ce_poc_declare(id, bits)` / `ce_poc_declare_px(c, h)` / `ce_poc_declare_px_id(id, h)`（**粘性**声明，绝对高度 px）、
  `ce_poc_height()`（**只读访问器** = 字段当下值，含原生卡 22 的 224）、`ce_poc_apply()`（帧界落笔）、
  `ce_poc_release(id)`（删卡出口）、诊断 `ce_poc_winner()` / `ce_poc_target_bits()` / `ce_poc_base()`。
- **聚合 = 取最大**（线最低那条；并列取 id 小者记名）：收益对 y 单调 ⇒ 谁的收益都不打折；
  「相加」没有语义；「取小」会让低线卡白声明。**声明是粘性的**（照原生卡：开局写一次管到改口）。
  上限夹 448（游玩区高，一手）。**没有声明时一个字都不写** ⇒ 不会把原生卡的 224 顶回 128。
- **落笔**：`BP_ce_item_frame`（`0x446ee3`，道具池 body **之前**）每帧一次写字段 + 置 `Gui+0x194` bit `0x2000`；
  卡在自己的开局路径里也可以再调一次（幂等）—— 即 **「开局那一刻就已经生效」**。
- **★ 指示线怎么跟的**：**不用我们画、不用改资产** —— 引擎那条线是**引擎自己**建的 VM，位置每建一次就现读字段。
  要做的只有两件：写字段 + 置 `0x2000`（原生卡 22 的两条指令，逐条复刻，含它的空指针检查与「只置不清」）。
  ⚠ 已知边界：引擎**只在建线的那两次**（关卡第 0 帧 / 第 30 帧）读字段 ⇒ **中途**改高度不保证让已画出的线动
  （这正是用户口径强调「**开局**」的原因）。
- **消费者迁移清单**（逐个改完）：`sdk.c`（`BP_ce_item_score_in` 的系数判据）、`cards/faith.h`
  （`faith_player_poc_height()` → 转发访问器 ⇒ `cards/mof.c` / `cards/faith.c` / `cards/signal.c` 一并跟着走）、
  `cards/th06rank.c`（**106**）、`cards/signal.c`（**111** 神奈子，见下）。
  ⚠ **`cards/collect2x.c`（114）经查不直读字段**：它的收点批走 `ce_item_pool_any_attracting()`（读道具 `state == 3`），
  那是引擎**自己**的判据结果 ⇒ 自动跟随，无需改。
  ⚠ **`cards/season.c:605`（116）仍是直读** —— 本会话被明确要求不碰该文件（另一会话在改压暗矩形）⇒
  保持原样（行为等价：它就是字段本身）。**下一个碰 116 的会话顺手换成 `ce_poc_height()` 即可**。
- **111 神奈子已迁移**（原「⏳ 待迁移」标记全清）：`signal_support_kanako()` = 读 `ce_poc_height()` +
  `signal_kanako_tick()` 算目标值 + `ce_poc_declare_px_id()` 声明 + `ce_poc_apply()`；
  `signal_support_reset()` 的写回改成 `ce_poc_release()`（层写回的是**我们动手前**的字段原值，
  比本卡原来写回自己的快照基准更准）；`on_load` 里也调一次（开局那一刻）；
  `signal_core.{h,c}` 的纯逻辑**一个字没改**（只改注释，`signal_kanako_restore()` 保留为纯逻辑 + 单测）。
- **闸门**：`make check` **rc=0**（`x87 in our objects: 0`、`capcheck`、`engine_abi_check`、`check-docs.py`）；
  `test_sdk_core` 新增 `test_poc_layer()`（位型常量与一手机器码逐位相同 / 无声明不写 / 取最大 / 夹取 /
  拒收 NaN·Inf·负·0·越界 / 删卡写回基准而非写死 128 / 别人中途改字段 ⇒ 基准刷新）。
- **🟡 未钉死**：①「`Player__reset`（写 128）与 `on_load`（写 224）在关卡第 0 帧里的先后」没有实跑探针
  （原生卡实机生效 ⇒ 事实上是「reset 先、on_load 后」；本层的做法与原生卡同形，所以不劣于它）；
  ② 线自删（198 帧）与第 30 帧那次重建的先后；③ 线的最终屏幕像素（`pos(0,48,0)` + `entity_pos` 的和）
  没逐像素对过 —— 三条都写在 `engine/player/th18/05-poc-height-and-indicator.md` §5。
- **实跑看什么**：带 111（早苗机体）进关 ⇒ 开局那条**橙白闪烁的横线**应当出现在**降低后的高度**上
  （128 → 160 px）；`signal/support2: … kanako N (resnap M)` 的 `M` 应当**基本为 0**
  （>0 = 有人在我们之后改了字段）。原生卡 22 也在卡组里时：线应当是「224 再下移 32」= 256，删卡写回 224（不是 128）。







### 2N-补. 卡侧接线（2026-09-16，✅ 已落地，等实跑）

`native/cards/signal.c` 的 `signal_support_flandre()`（一行都没动资产）：

| 时机 | 落笔 |
| --- | --- |
| 蓄力中（`charge > 0`）| 起 **277** 范围环（层 13），每帧写 `entity_pos` + `ce_anm_set_scale(id, R, R)` —— 细节见下方「落笔 A」|
| 蓄力掉了（中弹打断 / 没满松手）| `ce_anm_interrupt(id, 1)`（脚本 20 帧淡出后自删）+ 清 id |
| 蓄满松手 ⇒ 清屏 | 范围环 `interrupt(1)`（**20 帧淡出跨过清屏帧**）→ 再在自机位置起 **278** 爆闪（层 13，**不写 scale** —— 脚本自己在跑 `scaleTime`），15 帧后自删、不记 id |
| 删卡 / 换关 / 复位 | `signal_support_reset()` 里收范围环（长持续效果铁律）|

**落笔 A（蓄力中那一行）**：`ce_anm_spawn(CE_ABILITY_ANM(), CE_ANM_ABILITY_SCRIPT_FLAN_CHARGE_RING, 13)`，
层 13 = 脚本自己的 `layer(13)`；`R` = **清屏半径 px（86.00）**，来自 `signal_flandre_cancel_radius_x100()`
（**与清屏判定共用一个函数**，不另算一份；❓ 自定项见 `TUNING.md` §2.1h ①）。

音效同时接上（见上面 2N §3 的一手表）：起手 `se_cat00`（TH18 `0x21`）、循环 `se_release`（`0x4d`，30 帧节流 ❓ 自定）、蓄满 `0x29`（借原生卡 42 的挥击音）。

**实跑看什么**：① 按住 Shift 蓄力时自机周围有没有那圈**青蓝色界线**（半径 86 px，`drawPolyBorder` + 卡侧 px 半径）；
② 松手清屏时那一记 **爆闪**（15 帧）位置对不对、会不会和结算错位；③ 界线淡出的 20 帧里能不能一眼看出「消到哪」。

### 2N-补2. ★★ 灵击（110 / 111）**加视觉**（2026-09-16k，用户报「灵击依然没有特效」）

> **用户逐字**：「**灵击依然没有特效**」（上一轮我承诺补，但当时只有计划、没落盘 ⇒ 这一轮真做）。

**已经自己核实的一手（不用再挖）**：

| 事实 | 一手 |
| --- | --- |
| TH10 的灵击**消弹半径就是它那个 VM 的 `scale.y`**：`Bomb::on_tick` 每帧 `[bomb+0x3c] = vm.scale.y` | `th10:0x40580e` / `th10:0x405813`（见 `engine/player/th10/01-bomb-full-chain.md` §3）|
| TH10 那条曲线 = `bullet.anm` **script400**（灵梦）= `scale(16,16)` + `scaleTime(120, 6, 16.0 → 160.0)`；**script407**（魔理沙）→ 112.0；**script441**（符卡中）→ 64.0 | 同篇；`local/th10.v1.00a/anm/bullet/bullet.anm.txt:16330` 起 |
| script400 本体**自己不画东西**：它 `scriptNew` 出 394–399 六支子脚本（子脚本才带 `sprite(sprite362)` / `sprite(sprite330)` + `scaleTime`）⇒ **要照抄视觉就得把 TH10 `bullet.anm` 的 sprite 330/362 那几格 1:1 抠过来** | 同上（`ins_88` = `scriptNew`）|
| TH11 的灵击是**逐帧长大的**消弹圈（`[bomb+0x490] = 32.0` 初值、`[bomb+0x494] = 10.0`/帧），特效实体存在 `bomb+0x40` | `th11:0x407680` / `th11:0x407928`（见 `signal.c` 的荷取段注释）|
| 我们两张卡的现状：**110 已经是「半径逐帧长大」**（`mof_core_bomb_radius_centi()`，16.0 → 目标、120 帧线性 ⇒ 与 TH10 script400 **同一条曲线**）；**111 是一次性全屏**（`SIGNAL_BOMB_CANCEL_R` = 4000.0f ❓ 自定，因为我们没有逐帧 bomb 主体）| `native/cards/mof_core.c` / `native/cards/signal.c:152` |

⇒ **两条视觉的形态由此定死（不是拍脑袋）**：
- **110（风神录）** = 一圈**跟着消弹半径长的环**（半径 = `mof_core_bomb_radius_centi()` px）——
  这正是 TH10 原版的机制（半径就是 VM 的 scale），画出来玩家看到的**就是**真被消掉的范围。
- **111（地灵殿）** = 一圈跟着 **TH11 的半径曲线**长的环（`32.0 + 10.0×帧`、75 帧）——
  不能画 4000 px 的圈（我们那一下是全屏消弹），所以环复现的是**原版的半径曲线**，不是消弹范围。

**★ 已落地（2026-09-16k）**：

| # | 动作 | 落点 | 说明 |
| --- | --- | --- | --- |
| ① | **范围环**（110/111 共用） | 脚本 **279** `bomb_clear_ring`（`drawPolyBorder(1.0f, 64)` + `layer(13)`）| 半径**由卡侧逐帧给 px**；两卡**同一条 TH10 曲线**（见下 ⚠ 2026-09-16l）|
| ② | **TH10 火星**（110/111 共用） | 脚本 **280/281**（= TH10 `bullet.anm` script398/399 逐条移植）+ `ATLAS_MOF_BOMB`（entry **54** / sprite **435**，`sprite330` 1:1 切图）| TH10 那支灵击本体一帧起 12 颗（红 6 蓝 6）⇒ 我们**每 6 帧一红一蓝**摊开（❓ 自定，两卡同值） |
| ③ | 卡侧 | `cards/mof.c`（`mof_bomb_start/tick` + 新 `mof_bomb_fx_kill()`）、`cards/signal.c`（新 `signal_bomb_ring_kill/tick()`）| 两条都在 `mof_clear()` / `signal_support_reset()` / `on_stage_start` 里收干净（**长持续效果铁律**）|

**★ 2026-09-16l 用户改口径：111 的灵击也照 TH10**

> **用户逐字**：「**我才发现 th11 的灵击每个机体都完全不一样** ⇒ **都改成 th10 的样式吧，
> 装 th11 的时候也是 th10 灵击**」

⇒ `signal.c` 的环半径从 TH11 的 `32.0 + 10.0×帧`（`[bomb+0x490] += [+0x494]`）**换成 TH10 的
`16.0 → 160.0（灵梦）/ 112.0（其他）、120 帧线性`**（`bullet.anm` script400/407），并**同时起 280/281**。
⇒ 279/280/281 现在各有 **2 个调用点**（110 + 111），**没有新增任何资产**。
110 侧算式 = `mof_core_bomb_radius_centi()`、111 侧 = `signal_bomb_ring_radius_centi()`，
**两处各留一份**（卡之间不互相 include）⇒ 改一处必须同步另一处。
⚠ 111 的环只是**视觉**：它的消弹仍是一次性全屏（`SIGNAL_BOMB_CANCEL_R` = 4000.0f ❓ 自定）。

**★ 2026-09-16l 文·超高速 = 残像拖影（不是闪黄）**

> **用户逐字**：「**超高速移动不是闪黄，而是背后有玩家的拖影，参考 th16 的实现方式，以及反汇编资料**」

- **一手（两条独立链）**：TH11 `th11:0x43066e`–`0x4306c2`（计数 > 10 后**每帧**建一个 VM、
  用 `[player+0x3b0]` = **自机当下那帧的 sprite** 调 `set_sprite`、摆到自机坐标 = `[player+0x87c]`）；
  TH16 `Player__on_tick` `0x442c76`/`0x442d33`（加速态每帧建残像 + `set_sprite([player+0x34])`）。
- **落地**：= 卡 116 那套（`cards/season.c` 的 `afterimage_tick()`，本仓已实跑过）——
  `ce_anm_spawn(自机 anm, 脚本[CHARACTER], 13)` + `ce_anm_set_sprite(自机 anm, id, 自机当下 sprite)`
  + `ce_anm_set_pos(自机坐标)`；脚本号按机体 = **pl00 17 / pl01 19 / pl02 14 / pl03 14**（TH18 原生残像脚本，
  零新增资产）。调用点 = `signal_support_aya()`（持续期内每帧一个；脚本 20 帧自灭 ⇒ 场上约 20 个）。
- **明确不做**：给自机本体染黄（TH16 的「加速态」那一半）。
- ⚠ **颜色差异**（登记 `TUNING.md` §2.1d 那张表）：TH11 的残像脚本是「红→蓝、15 帧」
  （`pl00.anm` script19，原文见 `AUDIT` §BE 详录 #31），TH16/TH18 那支是「蓝紫 `color(60,0,192)`、20 帧」
  ⇒ 我们用**原生脚本**（颜色即原生值）。

⚠ **诚实边界**：环的**图元**是 ❓ 自定 —— TH10 的四支柱环画的是 `sprite(sprite362)` + `texCircle(32)`
（把 14×128 的竖条弯成圈，`etama3.png`），本轮**没有移植那条竖条**，改用本仓实跑过的 `drawPolyBorder`。
要完全对齐就再把 sprite362 抠一次（`make_mof_bomb_art.py` 已留了同一套管线）。

## 2M. ★ 恋恋（早苗×古明地恋）改口径：**「放开 Shift 和 Z」**（2026-09-16，✅ 已落地）

> **用户逐字**：「**恋恋改成放开 shift 和 Z 吧**」

原口径 = 「放开**全**键位（除移动键）」（掩码 = 六个动作键 `SIGNAL_IN_ACTION_MASK`：
射击/炸弹/低速/暂停/发动卡/切卡）。新口径 = **只认 Z 与 Shift 两个键的下降沿**：

- 判据（b 路，`cards/signal_core.c` 的 `signal_koishi_tick()`）：`now = (input & SIGNAL_IN_Z_OR_SHIFT) != 0`
  ⇒ `prev && !now` ⇒ 发动一次。
- ★★ **同日用户补充澄清（逐字）**：「**不是 or，是上一帧按着 shift or 按着 Z，这一帧不按 shift and
  不按着 Z**」⇒ 「or」在**上一帧那一半**（其中一个按着就够），「and」在**这一帧那一半**
  （**两个都得松**）。**不是「放开任意一个」** ⇒ 「上一帧 Z+Shift 都按着、这一帧只松其中一个」**不发动**
  （单测 §17.6 ④ 两个方向 + ⑦ 都钉着）。
- 宏 `SIGNAL_IN_ACTION_MASK` **保留**（只作旧口径的留档，恋恋不再读它）。
- (a) 路（原生 Aya 卡的「起步」）不动 ⇒ 两路仍是**并集**（同一帧都成立也只发动一次，4 帧无敌取 max）。
- 单测：§17.6 重写为 7 条（放开 Z / 放开 Shift / 两键都放 / 只剩一个没放 / 放开 X·卡键不算 /
  只按方向键不算 / 与原生并集同帧只算一次）。

## 2L. ★★ 用户 2026-09-16 的四条实跑反馈（本会话逐条处置）

> 用户逐字：「**1、魔理沙特技缺乏子机贴图 2、魔理沙特技子机不发弹 3、灵击缺乏贴图
> 4、原版魔理沙特技子机应当是同时按下 shift+Z，而不是按着 Z，再按 shift 就切换了，你查一下**」

### ④ 换档判据：改成「两键都在刚按下的窗口内」（✅ 已落地）

一手（本轮新读到的一环，`th11:0x435fe0` = 每帧输入函数）：`0x400` 的**默认产生支**是
`0x43605a`–`0x43607b`：`test al,0x1`（射击）∧ `cmp [0x4c933c],5 / jae 跳过` ∧ `test al,0x8`（低速）
∧ `cmp [0x4c9348],5 / jae 跳过` ⇒ `or eax,0x400`。
⚠ 那两个槽在本 build 是**零初始化的 BSS、全 exe 无写点**（PE 节表逐节解析确认）⇒ 两个 `< 5` 门恒过，
但**形状**是「两个各自的值都 < 5」= 「**两个键都刚按下**」⇒ 按它落地：
`signal_core_patchouli_fresh(input, z_held, shift_held)` + `SIGNAL_PATCHOULI_FRESH_FRAMES` = **5**。
另一支（配置位 = 1）只看低速位电平、另有「射击按住 ≥ 8 帧 ⇒ 派生低速位」的计数器（`0x4c93bc`）。
⚠ 读取端仍 🟡：帕秋莉那条读的是 **`0x4c93cc`**（回放路径由 `0x40d65d`–`0x40d662` 从 `0x4c92b4` 抄来；
实时游玩下 `0x4c92b4` 的写入者仍未找到）。
⇒ 效果：**同时按下 Z+Shift 才切档**；「按着 Z 很久再按 Shift」不再切（用户报的那一种）。
单测 +10 条（窗口边界 / 两侧超窗 / 双长按 / 缺键）。

### ①② 子机贴图与开火：加了定位用的诊断（🟡 等一次实跑）

两条都在这条链上：`signal_option_declare()`（声明）→ `on_option_pose_apply()`（落笔 + `ce_option_get`）
→ `on_tick_shooters()`（`ce_tick_shooters_for_card(o, s->frame, 0, set)`，set = 0x29–0x2d）。
日志现状：`signal: option allocated (ptr …)` 出现过（槽申请到了）、
`wba_c: option shot path set 0x2a` 也出现过（**说明我们的开火调用确实到了引擎的 `th18:0x40a9c0`**）
⇒ 缺的那一环在「子机 VM 本身」或「那几组 shooterset 的行」上。新增一行诊断（每 600 帧）：
`signal/opt: on=… win=… ptr=… vm_id=… alive=… sprite=… scale=… pos(…) set=0x… tick=… power=…/…`
——`sprite=0 / scale=0 / alive=0` ⇒ 贴图那一半；`tick=0` ⇒ 开火那条路没跑。

### ③ 灵击贴图：**确认缺失**（❓ 待做，需要资产层）

`mof.c` 的注释早就记着「(f) 视觉：TH10 的灵击 VM 自己有贴图（`bullet.anm` **script400** 的扩散圈），
本卡**没有**视觉（TH18 无对应贴图；要做需要资产层）」⇒ 111 侧同理。做它需要：
① 从 TH10 `bullet.anm` 取出 script400 用的那张图（我们 `assets/ability/mof/` 已有同源管线
`make_mof_art.py`，绿点就是这么来的）；② 在 `ability.anm` 加一条脚本（扩散 + 淡出 + 自删）；
③ 卡侧在 `mof_bomb_start()` / `signal_bomb()` 里起/撤这个 VM。
**本轮未做**（任务边界：先落 ④ 与 ①② 的诊断）。

## 2K. ★★ 「全屏收点」看不见自建的信仰点 + V 门补全（2026-09-16，✅ 已落地，等实跑）

> **用户实跑反馈（2026-09-16 逐字）**：「**信仰点没有被信号满格全屏吸收（擦弹）和灵梦伊吹特技全屏吸收**，
> 以及**检查一下进对话的全屏吸收**」—— 本会话是这次改动的唯一 owner。

### ① 全屏收点漏掉信仰点（真 bug）

三条「全屏收点」都只调 SDK 的 `ce_item_collect_all()`，而**那个函数只扫原生道具池**
（`ItemManager` @ `0x4cf2ec`，池 +0x14 / stride 0xc94 —— `sdk.h` ①）；本池的点是**自建 VM**
（TH18 没有信仰点 item type）⇒ **那三条根本看不见它们**。

一手：TH10 / TH11 的信仰点**就是道具**（type 3 大 / type 9 小）⇒ 满槽那一条（`th11:0x42364a` → `0x423805`）
与萃香那一条（`th11:0x4245e0`，`ItemManager::reimu_b_autocollect`）扫的是**同一个道具池**，
判据只看 `state == 1 → 3`、**与 type 无关** ⇒ 原版天然包含信仰点。

**改法**：新增 `faith_collect_all(uint32_t slot_mask)`（`faith.{h,c}`）= 把整池活着的点切成
本池 state 3（= 引擎 state 3 的对位；`state ∉ {0,3}` 的才切，与那个 SDK 函数同规则、只动 state 不动 pos/vel），
接进三个调用点：

| 调用点 | 文件 | 说明 |
| --- | --- | --- |
| 111 满槽全收（⑤）| `signal.c` 的 `signal_sweep_items()` | **一次接线覆盖两条**：满槽 + 萃香（都走这个函数）|
| 灵梦·萃香的全屏回收 | 同上 | 同上 |
| 107 森罗结界「自动回收道具」| `senro.c` 的 `senro_border_collect()` | ★ 这是 107 的行为变更（同根因一起修）；不想要就删那一行 |

日志：`signal: 全屏收点 -> 原生道具 N 颗 + 自建信仰点 M 颗（当前信号 X）`。

### ② 「进对话的全屏吸收」复核（结论：本来就有，但门少一条、作用态少两种）

引擎的道具「真空飞行」门有**两处**（一手，**逐条同形**）：状态 1 那处 `0x445b9a`–`0x445be0`、
状态 4 那处 `0x445fe4`–`0x44602a`：

| 条 | 地址 | 内容 |
| --- | --- | --- |
| ① | `0x445ba0`/`0x445ba5` | 自机 `state == 2 \|\| == 4`（死亡中 / 被判中弹）|
| ② | `0x445baa`–`0x445bb9` | 自机 y < 收点线（`player+0x47998`）|
| ③ | `0x445bc4`–`0x445bce` | 放 B 且帧钟 < 60（`bomb_mgr+0x30 == 1 ∧ +0x38 < 0x3c`）|
| ④ | `0x445bd4`–`0x445be0` | 对话 VM 非 0（`Gui+0x1b0`）|

命中 ⇒ 道具 state → 3（`0x445f63`）。本池原来：**④ 对话那一条本来就有** ✅（所以「进对话收绿点」是通的），
但 **① 少了**、且**只在 `state == 1` 判**（引擎两处都是「非 0 非 3 就查」；本池 state 2 = 引擎 state 1 的
出生上抛，两套段号不可逐值对照）⇒ 补成 `faith_vacuum_gate()`（四条一处写）+ 作用态 `state ∉ {0,3}`。
⇒ 修好之后「刚击破撒出的点还在上抛时就进对话 / 上线 / 放 B」也会被吸（原来要等它落到飘落态）。

- **`make check`** / **干净重编** 见发布说明；**实跑看什么**：满槽 / 松手萃香 / 进对话时
  **绿点也要跟着朝自机飞**，日志里 `全屏收点 … 自建信仰点 M 颗` 的 `M` 非 0。

## 2J. ★★ 110 的信仰点**收集动画**：删掉自造的「放大淡出」，改成 TH18 原生的「播音 + 立刻删」（2026-09-16，✅ 已落地，等实跑）

> **用户实跑反馈（2026-09-16 逐字）**：「拾取到大信仰点，**吸收那一刻动画是一个放大减淡**，
> 或许 th10 原生是这个，但是**和 th18 的吸收 p 点金钱的动画不符**」—— 本会话是这次改动的唯一 owner。

**用户是对的，而且一手连 TH10 都不是「放大淡出」**：

- **TH18（收集分派的共享尾，所有 type 都走）** `th18:0x446584`–`0x4465a5`：
  `lea esi,[edi+0xc2c]`（道具坐标）→ `movss xmm2,[esi]` → `push 0x25` → `call 0x476c70`
  （**原位播音** = `se_item00`）→ `mov [edi+0xc74],0`（**state 立刻归 0**）→ `call 0x488cf0`
  + 清 `[edi+0xc28]`（**释放道具的 anm VM**）。收集分派 `0x44625f`–`0x446584` 全程**不给道具 VM
  发任何中断**（对 `+0xc28` 只有那一处释放）⇒ **原生没有脚本级收尾动画**。
- **TH10 同形**：道具被吃到时引擎直接删 VM（`assets/ability/scripts/272_…` 的文件头原本就这么写着）
  ⇒ 我们原来那段「加算 + 放大 1.8× + 8 帧淡出 + `delete()`」是从**别的卡的实体脚本**借形状借来的
  ❓ 自定（`115_season_point` / `114_season_seed` / `101_pea_small`），不是任何一作的原样。

- **改法**：
  · `native/cards/faith.c`：`dot_collect()` 里 `ce_anm_interrupt(id, 1)` → 换成
    `ce_play_sound(FAITH_PICKUP_SE = 0x25, p->x)`（一手 `xmm2 = pos.x`）+ `ce_anm_delete(id)`；
  · `assets/ability/scripts/272_mof_faith_dot.anm.txt` 与 `273_mof_faith_dot_micro.anm.txt`：
    删掉尾部的 `interruptLabel(1)/blendMode(1)/scaleTime(8,0,1.8,1.8)/alphaTime(8,0,0)/+8: delete()`
    （脚本本体回到「TH10 script377/script382 逐条」——与 `script383` 的 `stop()` 收尾一致）；
  · 资产随之重编（`make anm`）⇒ **`ability.anm` 与 DLL 必须同批发布**（本轮已同批）。
- **本池记账不受影响**：调用点本来就是 `dot_collect(p); dot_remove(tier, i);`（当场回收槽，
  不依赖脚本 8 帧后自删）。
- **实跑看什么**：吃到信仰点时**绿点立刻消失**（不再有放大淡出的 8 帧），并且**响 `se_item00`**
  —— 与吃 P 点、吃金钱的收尾**完全同一形状**。

## 2I. ★★ 110 的**连击槽进账**：补上「引擎道具 + 击破」四条（2026-09-16，✅ 已落地，等实跑）

> **用户实跑反馈（2026-09-16 逐字）**：「**然后连击值一点也不涨，击破妖精是涨连击值的啊**」
> —— 本会话是这次改动的唯一 owner。

**根因**：一手 th10 的连击槽共 **9 个充能调用点**（表见
[`engine/player/th10/02-hud-bottom-left.md`](../../../engine/player/th10/02-hud-bottom-left.md) §2.2），
我们只接了其中**两条** —— 「自建的信仰点被收走」（+120/+60，`mof_on_pickup()`）与
「消弹微档」（+3，`mof_on_cancel()`）—— **引擎自己的道具与敌人侧那几条一条都没接**
⇒ 吃 P 点、吃钱、吃大 P、击破小怪**全都不涨连击**（连击槽只会自己 −1 掉到 0）。

**补的四条**（值全部 = 一手 `th10:0x412ff0` 调用点前的 `push` 立即数）：

| 来源 | 帧数 | 一手 | 落点 |
| --- | --- | --- | --- |
| 小 P（TH18 type 1）| +60 | `0x41b46c push 0x3c` | `mof.c` 的 `on_item_collect()` |
| 金钱（TH18 type 2）= **蓝点生态位** | +100 | `0x41b56e push 0x64` | 同上（`AUDIT.md` §Z19：TH18 无蓝点生态位）|
| 大 P（TH18 type 3）| +20 | `0x41b6bd`/`0x41b6ef push 0x14` | 同上 |
| **击破非 boss** | **+10** | `0x40e681 push 0xa` | `mof.c` 的 `on_enemy_death()`（boss = +0f）|

- **纯逻辑层**：`mof_core_item_combo(int32_t type18)`（`mof_core.{h,c}`）+ 四个具名常量
  `MOF_COMBO_BLUE/SMALL_P/BIG_P/KILL`；`MOF_ITEM_TYPE18_MONEY/BIG_P` 与既有的 `MOF_P_ITEM_TYPE18` 配套。
- **单测**：`tests/test_mof_core.c` 从 494 → **515 断言**（逐 type 对账 + 满槽夹取 + 「120+10 = 用户口径的 130」）。
- ⚠ **与用户口径的一处差异（已登记 `AUDIT.md` §BC9）**：用户 2026-09-15 的表述是
  「大p、击杀非boss单位 **+20f**」，一手拆开是「大 P +20 / **击杀 +10**」⇒ 取一手 10，
  因为同一张表里用户写的「大信仰点 **+130f**」= 拾取 120 + 击杀 10 正好自洽。要 20 只改 `MOF_COMBO_KILL`。
- **实跑看什么**：吃 P 点 / 吃钱 / **吃信仰点** / 击破小怪时**行 3 的连击条应当明显回涨**；
  对账用 `mof:` 行尾新增的 `comboIn N (pickup A + item B + kill C + cancel D)` —— **四条通道各一个累计量**
  （`pickup` = 自建信仰点被收：大 +120 / 小 +60，**这一条本来就有**；`cancel` = 消弹每颗 +3，也本来就有）。
  某一项恒 0 就是那条断了：`kill` = `on_enemy_death` 的门（KILL_ALL / 真敌人）挡了；
  `item` = 道具 type 不在 {1,2,3}（照日志里的 `type` 对一下）；`pickup` = 池没收到点。
  ⚠ 四个数是**夹取前**的进账和（连击槽上限 130，所以 `comboIn` 涨得比 `combo` 快是正常的）。

## 2H. ★★ 111 的三条灵梦特技：**撤掉「买不起就不发动」的硬门 + 萃香改成每帧收**（2026-09-16，✅ 已落地，等实跑）

> ⚠ 本节末尾的「全免费」是**当天最终形态**（价那一层整层删除）——见文中的「★ 最终形态」与「★ 为什么最后是全免费」。

> **用户实跑反馈（2026-09-16 逐字）**：「**灵梦机体的松开全部按键全收集没有生效**」
> —— 本会话是这次改动的唯一 owner。

**根因两条，都是我们自己加的，跟效果代码无关**（一手侧见
[`engine/player/th11/02-support-special-effects.md`](../../../engine/player/th11/02-support-special-effects.md) §2
与 [`engine/faith/th11/02-koushin-signal.md`](../../../engine/faith/th11/02-koushin-signal.md) §1/§4）：

1. **硬门**：`SIGNAL_SUPPORT_MIN`（信号 < 10.00 ⇒ 紫/萃香/帕秋莉整条不发动）。一手**不存在**这个门 ——
   交信度槽 `0x4a56f4` 的写点**穷举 7 处，全是加值或清零**，没有任何「特技花掉交信度」。
   实跑日志（`th18_card_expand.log` 2026-09-16 09:55 一局）里信号在 miss 归零后有一段是 `0`，
   松手触发时连打六行 `萃香·全屏回收 被门槛挡下（信号 0 < 10000）`（行 1667/1672/1675/1691/1694/1695）
   ⇒ 玩家看到的正是「这条特技坏了」。
2. **只收一帧**：`0x43078f inc` / `0x430795 cmp,0xa` **都不清计数器**（`+0x8ba4` 全 exe 只有
   inc / cmp / 按键归零三个访问点）⇒ 计数器过 10 之后**每帧**都走到 `0x43079e` —— 松手期间是**连续**全收。
   我们原来只在「进入持续态」那一帧收，松手后新落下的道具一颗都不动。

- **改法（★ 最终形态，用户当天追加拍板「全免费 = 完全一手」）**：`native/cards/signal.c`，纯卡侧，**没动任何共享件**。
  · **价那一层整层删除**：`signal_spend()`、五条价（紫 20.00 / 萃香 10.00 / 文 0.25-每帧 / 帕秋莉 15.00 /
    荷取 5.00）、`SIGNAL_SUPPORT_MIN` 硬门、诊断字段 `cost_spent` 全部删掉；特技只按一手判据与频率发动；
  · 萃香：`signal_hold_tick()` 返回持续态时**每帧**调 `ce_item_collect_all()`（复刻 `0x43079e` 的到达频率）；
  · 诊断消息订正：`signal_sweep_items()` 那行原来固定写 `gauge full (signal X >= 100000)`，
    而萃香那条是在**扣费之后**打印 ⇒ 日志里出现过自相矛盾的「signal 54900 >= 100000」（行 1055）。
    现在只报「本帧几颗 + 当前信号」。
- **`make check`**：全绿（`dllx87` 零 x87 / `engine_abi_check` 7 条 / `anm_hud_alpha_check` 24 项 / `check-docs` 293 文件）。
- **实跑看什么**：灵梦机体下**信号 0 时松开全部按键**（>10 帧）⇒ 画面上的道具**持续**被吸走
  （日志 `signal: 萃香·全屏回收 持续态开始…` 之后应能看到多行 `signal: 全屏收点 -> 本帧 N 颗`）；
  另外四条（紫的缺口 / 文的速度 / 帕秋莉的换档 / 荷取的退款）也应当**与信号高低无关**地发动。
  ⚠ 诊断行改了：`signal/support:` 里原来的 `| cost N (sig M) |` 现在是 `| sig M |`（价那一层没了）。
- **★ 为什么最后是「全免费」而不是「软价」**：判决性的一手事实是**槽里根本没有「消费」入口** ——
  在 `local/th11.v1.00a/th11.exe` 的完整反汇编上重做普查（不只引文档）：加值入口 `0x40baa0` 的 **5 个调用点
  全在推擦弹收益表**（`0x4a7950`/`0x4a79d4`/`0x4a7a68`/`0x4a7a7c`，难度索引 `0x4a5720`，无负参数）；
  每帧函数 `0x420a70` 的分支是 `+0x1388`（不足 10000 一帧补满）/`+0x10`/`+0xc`/`+0x9`，线下
  `0x420b03 add [edi+0x14],esi`（`esi = −0x1e`）—— **全 exe 唯一的减法**；其余 5 个 store 全是清零
  （`0x41fb24` 一局开始 / `0x431097`·`0x431227`·`0x4312c0` 玩家状态 0·2·4 / `0x432807` miss）。
  ⇒ 「价」在一手里没有任何对应物，留着只是让信号槽（= 计分乘数）被特技偷偷吃掉。删干净后本卡的信号槽
  语义 **100% 等于一手**：擦弹加 / 线上补 / 线下 −0.30 衰减 / 死亡与登场清零 / 满槽全收 / 计分乘数那一项。

## ★ 2026-09-16b：113 的灵界形态（白剪影，本轮四条里的 ③）已发布

- **一手结论**：TH13 的"灵界形态"就是**换 sprite 号**（`Player::begin` 拷脚本 5，而脚本 5..9 与 0..4 同构），
  贴图在 `pl00f.png` / `pl01d.png` / `pl02c.png`（平均亮度 142 / 117 / 127，普通图 80 / 46 / 62）。
- **实现（用户拍板的路线 1）**：不换脚本，`on_draw` 里每帧把本体 `vm+0x24` 的 0..23 换成
  `0..23 + CE_PL_ANM_FORM_BASE_*`（`AnmLoaded__set_sprite` `0x477b00`，anm 必须传 `player+0x0c`）。
- **顺手订正**：上一轮我把三张形态图切错了源图（用了普通贴图，矩形网格相同 ⇒ 门看不出来）；
  现在装配图亮度 139.3 / 116.4 / 126.2 与源图一致，与普通图逐帧平均色差 65–72。详见 `AUDIT` §DG-4。
- **已发布**：DLL 4 路径同 md5 `9d49c88aa19f2fd0b51480068089449a` + `pl00/pl01/pl03.anm` 同批；配套闸 ✓。
- **实跑判据**：三机体进灵界自机变白、出灵界还原；**咲夜无变化**（不是漏做）；日志 ON/OFF 各一行。
- **本轮剩余**：② 蓝灵最大得点（+10 / 灵界内 +100，`SPEC §9 #5` 载体未拍）／④ 槽初值 = 200。
- **★ 咲夜（用户 2026-09-16 追加定案）**：TH13 没有这个自机 ⇒ 没有成品可切。做法 = **她自己的 TH18 那 24 帧**
  + 从 TH13 三对原版图**量出来**的「普通 → 灵界」色调变换（`assets/trance_form_transform.py`；
  验证 MAE 69.7→20.5 / 88.4→25.5 / 82.5→26.5）。**❓自定**，登记 `AUDIT` §DG-7；
  差异明写：色调对，但她没有 TH13 那种辉光。诊断图 `anm_compare/咲夜_灵界形态_24帧对比.png`。

## ★ 2026-09-16c：咲夜形态改用**用户手绘**的辉光版（已发布）

- 用户把 `咲夜_pl02_灵界形态_变换生成.png` 覆盖成手绘版（辉光 110 px/帧、中位 1.0px、平均 alpha 99 ⇒ 与原版同档）；
  入库到 `local/th18.v1.00a/trance_form/pl02form.png`（gitignored），生成器用 `src_override=` 优先读它。
- **坑**：自机贴图是 16 位（每通道 4 位）⇒ 手绘的 8 位灰阶会被量化；构建期现在**先量化**（`_quantize16`），
  保证"你画的 = 游戏显示的"且回读门精确。详见 `AUDIT` §DG-8（含 32 位那条试过没走的路）。
- 发布：只动 `pl02.anm`（DLL 未变，仍是 `f9a626dcd4de5d4561b54f27deaa5e2b`）；配套闸 ✓。

## 2N. ★ 二小姐蓄力光环 + 消弹特效（**资产**；2026-09-16，代码侧接线还没做）

> 任务书 = [`engine/card/th18/13-mallet-clear-and-th17-charge.md`](../../../engine/card/th18/13-mallet-clear-and-th17-charge.md)
> （TH18 原生卡 42「小槌」的挥击 + TH17 妖梦蓄力的一手考证）。**本轮只动 `assets/`**：
> 两条新脚本 + 一个新图集 + 重编 `native/build/ability.anm` / `native/anm_ids.h`。
> **没有碰任何 `.c` / `.h`**（卡侧接线见本节末尾「**还差什么**」）。

### 1. 两条新脚本（`assets/ability/scripts/`，编号 = 原生 68 个脚本之后的连续号）

| 脚本 | 文件 | `anm_ids.h` 宏 | 干什么 |
| --- | --- | --- | --- |
| **script277** | `assets/ability/scripts/277_flan_charge_ring.anm.txt` | `CE_ANM_ABILITY_SCRIPT_FLAN_CHARGE_RING` | 蓄力**光环**（`drawPoly` 32 边形；卡侧逐帧给位置 + 缩放）|
| **script278** | `assets/ability/scripts/278_flan_clear_burst.anm.txt` | `CE_ANM_ABILITY_SCRIPT_FLAN_CLEAR_BURST` | 松手清屏那一帧的**爆闪**（原生卡 42「挥击」的压缩版，VM 存活 15 帧）|

**一手出处（逐条写在两个脚本的文件头里）**

- **277 光环**：底本 = **TH17 `pl02.anm` script30**（妖梦蓄力那支；`sprite(-1); layer(5); drawPoly(1.0f, 32);
  alpha(64); blendMode(2); color(32,224,255); color2(128,224,255); alpha2(0); scaleTime(60,1,600,600);
  …interruptLabel(1): alphaTime(20,0,0); +20: delete();`）。TH18 **同一份 anm** 里有一支几乎逐字同形的
  **原生 `ability.anm` script49**。⇒ 静态属性逐条照抄 TH17；**层改用 `layer(6)` = 照抄 TH18 script49**
  （原生 `drawPoly` 只出现在层 3/5/6/18，任务书 §3.3 建议的 13 **没有原生先例**）。
- **278 爆闪**：底本 = **TH18 原生 `ability.anm` script28**（卡 42 的挥击 VM；卡侧状态机 `0x40eed0`）。
  静态属性逐条照抄（`layer(13); blendMode(0); type(1); alpha(255); zWriteDisable(1);`），
  淡出段逐条照抄它的 `interruptLabel(1)`（`scaleTime(8, 4, 2.0f, 2.0f)` + `alphaTime(8, 0, 0)` + `+8: delete()`）。
  丢掉的只有「20 帧下降段」（`scale(0,2.8)`+`scaleTime(20,4,1,1)`）与无限三帧循环 —— 我们是一次性结算。

**sprite（谁引了什么）**

- **277 一个 sprite 都不要**：`sprite(-1)` + `drawPoly`（TH17 script30 与 TH18 script49 **都**写 `sprite(-1)`
  ⇒ 「光环要从哪抠图」这一问本身不成立）。
- **278 用 4 个新 sprite**（= 原生 `ability.anm` entry0 `player/option/op00.png` 那 4 格的 **1:1 切图**）：
  `sprite41 (32,80,32,16)`（卡 42 建 VM 时 `set_sprite(0x29)` 那一格）+ `sprite85/86/87`
  `(0/48/96,160,48x47)`（script28 的三帧循环）。宏 = `CE_ANM_ABILITY_SPRITE_FLAN_MALLET_IDLE` /
  `_SWING_0` / `_SWING_1` / `_SWING_2`。
- ⚠ **也可以不抠图**：sprite 号是整份 anm 的全局表，直接写 `sprite(85)` 同样指到那一格；
  抠图只为跟本仓既有管线一致（`assets/ability/scripts/` 里**没有任何一条**追加脚本引原生 sprite 号）。
  想省这 4 个 sprite：把 278 里四个 `@…@` 换成 `sprite(41)/sprite(85)/sprite(86)/sprite(87)`、
  删掉 `ORDER.txt` 末尾那段即可，**时序一字不动**。
- 顺带排除的疑虑：**entry0 不会按机体换贴图** —— TH18 整包 `.dat` 里只有**一张** `player/option/op00.png`，
  而 entry0 同时供 8 支 option 脚本取图（`Player__allocate_option` `0x40a790` 的 15 个调用点传的脚本号 =
  2/6/7/14/16/18/20/22/23，各取 sprite 0 / 7 / 21..28+35..30 / 52 / 59..61 / 64..67 / 68..71 / 76..78，
  全在 entry0 的 sprite0..99 段内）。

### 2. `ORDER.txt` 末尾 **append** 了什么（只追加，一行未改）

新图集 **entry 53**、成员 sprite **431 / 432 / 433 / 434**（`ORDER.frozen.txt` 的 186 行冻结段逐行未变）：

```
ATLAS ATLAS_FLAN 144x64
FLAN_MALLET_IDLE       ability/flan/FLAN_MALLET_IDLE.png              @0,0,32x16
FLAN_MALLET_SWING_0    ability/flan/FLAN_MALLET_SWING_0.png           @0,16,48x47
FLAN_MALLET_SWING_1    ability/flan/FLAN_MALLET_SWING_1.png           @48,16,48x47
FLAN_MALLET_SWING_2    ability/flan/FLAN_MALLET_SWING_2.png           @96,16,48x47
```

切图生成器 = **新增** `assets/ability/make_flan_art.py`（照 `make_signal_hud_art.py` 的既有管线：
只 `crop`/`paste`、不缩放不补边、幂等、`--dump` 出 ASCII 复核），产物落 `assets/ability/flan/*.png`；
源 = `local/th18.v1.00a/anm/ability/player/option/op00.png`（原生贴图，只在 `local/`，不入库）。

### 3. 生成物（`make anm` 已跑）

- `native/build/ability.anm`（md5 `c2e6b12eb9dc64d0fbdff41b28f3bd6f`，9,449,260 B）；
  自检全过：`entry 54 / sprite 435 / script 279`、原生 spec 与贴图逐项一致、
  **图集 26 个 / 305 个成员从 anm 回读逐像素等于原图** ✅、entry 余量 202。
- `native/anm_ids.h` 新宏：`CE_ANM_ABILITY_SCRIPT_FLAN_CHARGE_RING` = **277**、
  `CE_ANM_ABILITY_SCRIPT_FLAN_CLEAR_BURST` = **278**、
  `CE_ANM_ABILITY_SPRITE_FLAN_MALLET_IDLE/_SWING_0/_SWING_1/_SWING_2` = **431..434**、
  `CE_ANM_ABILITY_ATLAS_ATLAS_FLAN` = **53**。
- 回读复核（`thanm -l 18 build/ability.anm`）：277 的 **`drawPolyBorder(1.0f, 64)` / 层 13 / 两个中断号**都在，
  278 的四格推进落在 `sprite431..434`、标签 0/1/3/5/7/15 与设计一致。
  （⚠ 2026-09-16j 重做前那一版的回读是 `drawPoly(1.0f, 32)` / 层 6 —— 那一版**实跑看不见**，见下。）

### 4. ❓ 自定（**必须进 `TUNING.md`**，也在各自脚本头里逐条标了）

| # | 自定项 | 为什么 | 现值 |
| --- | --- | --- | --- |
| ① | 范围环的**半径** | 原生 script35 把半径写死成 112；写死就与 `signal_core.c` 的清屏公式脱钩 | 卡侧逐帧 `ce_anm_set_scale(id, R, R)`，`R` = 清屏半径 px（86.00），**与清屏判定共用一个函数** |
| ② | 范围环淡出中断号 **1 与 2 都能收** | 原生 script35 只定义 2 | 两个 `interruptLabel` 同义（**推荐发 1**）；淡出 20 帧（原生 10 帧，改长的理由：松手那一帧环正好等于清屏半径） |
| ③ | 爆闪四格的**帧长** | 原生循环是每 6 帧一张、且无限循环；我们是一次性结算 | 起手格 1 帧 + 三格各 2 帧（推进 7 帧）+ 一手 8 帧淡出 |
| ④ | 范围环**层号 / 颜色 / alpha** | 原生 `layer(15)` 只在主菜单那条路出现（世界列表无实证）| `layer(13)`（与爆闪 278 同层）/ 青蓝 `(120,220,255)` / `alpha(64)` |

### 5. ★ 卡侧接线（✅ 2026-09-16 已全部落地，见 §2N-补）

> **⚠ 2026-09-16j 重做的根因（务必记住）**：第一版把 `scale` 当**半径**用（`0.4→2.0`），
> 但一手是「可见半径(px) = `drawPoly/drawPolyBorder` 的 radius × `scale.x`」（`th16:0x4626eb` / `th16:0x468722`）
> ⇒ 环只有 **0.4–2.0 px**，实跑就是用户报的「shift蓄力依然啥都没有，只有消弹结果」
> （日志里有 `二小姐·蓄力光环起 VM` ⇒ VM 起了，是尺寸错）。**任何「卡侧给半径」的环都要按 px 给。**

## 2O. ★★★ 111 的**魔理沙侧**：隐藏原生子机 + 子机格数随火力档 + DPS 归一（2026-09-16l，✅ 已落地，等实跑）

> **用户口径（2026-09-16 逐字，两条）**：
> ① 「**原生子机需要弄掉**」；
> ② 「火力随 P 完全依赖原生关系，但是，得根据这一作的具体 dps 进行调整。**保证平均 dps 一致即可**，
>    比如可以相邻两次发射的弹幕攻击力不一样，以符合平均 dps 关系。
>    **大概是 18魔理沙/13魔理沙 = 18灵梦/13灵梦**」。

> **本节是「补登记」**：代码由那一轮的子代理全部写完、`make check` 全绿，**它在写文档之前挂了**。
> 共享件登记见 [`AUDIT.md`](AUDIT.md) §BE 详录 **#32**，旋钮表见 [`TUNING.md`](TUNING.md) §2.1i。

### 1. 落点（函数名 + 宏名 —— 照抄即改）

| 要做的事 | 落点 |
| --- | --- |
| 声明「这台机体不要原生子机」| `ce_native_option_hide_declare(card_id, 1, SIGNAL_CHAR_MARISA)`（**粘性**；**机体门在层里**，不在卡里）|
| 每帧自愈收掉幽灵子机 | `ce_native_option_sweep()`（`native/sdk.c`）|
| 原生子机**不建 / 已建的收掉** | 断点 `BP_ce_repopulate_native_option`（站点 `0x45d610`：把本次 `lvl` 压成 0）|
| 原生子机**不出弹** | 断点 `BP_ce_native_option_muzzle`（站点 `0x45e930`，判据 = **膛口指针区间**）|
| 子机格数 = 火力档 | `signal_power_cells()`（`native/cards/signal_core.c`）+ `signal_power_level()`（`signal.c` 的薄封装）|
| 声明层格数上限 | `SIGNAL_OPTION_MAX` = `CE_OPTION_POSE_CELLS`（`native/sdk_core.h`，= 4）|
| 逐格位置 | `signal_formation_cell()`（**每格自己的偏移**；单位 = 像素，卡侧 ×128 落笔）|
| 单发伤害 / 射速 / 满档格数 | `MARISA_FORMATION_ROW_DMG` = **20** / `_ROW_RATE` = 12 / `_CELLS` = 4（`assets/sht/append_shooterset.py`）|
| 归一对账（**硬断言**）| `MARISA_FORMATION_DPS_TARGET` = `(28+28+22+22)/15` = **6.6667** —— 不等就**直接停构建** |

### 2. 「为什么是 20」一行算式

本卡把**原生子机层替换掉** ⇒ 我们 4 格的**合计每帧输出**必须逐值等于原生满档子机层：
`D = fire_rate × V18_opt(4) / 4 = 12 × 6.667 / 4 = **20**`。
于是自机总输出与原生**逐值相同**：`6.667（子机）+ 6.000（主炮）= 12.667` ✅。

### 3. 「平均 dps 一致」逐档核对（原生 vs 我们，非聚焦）

`1.867 / 2.400 / 5.600 / 6.667` vs `1.667 / 3.333 / 5.000 / 6.667`
⇒ 档 1/3 各 **−11%**、档 2 **+39%**、档 4 **精确 0%**；**四档均值 4.133 vs 4.167 = +0.8%**
—— 这就是用户说的「**平均** dps 一致即可」。偏差来自**原生自己的非单调**（档 2 的四条激光每条只有 18，
比档 1 的单条 28 还低）；要逐档拟合就得**每档一个 damage**（或用户允许的「相邻两发交替」），
生成器已按 `(lo, hi)` 对留好那个形状，本轮取 `(20, 20)`（= 不用交替），改法与代价见 `TUNING.md` §2.1i。

### 4. ★ 实跑要看的（验收清单）

1. **魔理沙场上不该再出现原生子机**：满火力（档 4）站定 ⇒ 自机两侧那 4 个**原生子机贴图一个都不该有**；
   日志里应出现 `sdk: 原生子机自愈清理：本帧收掉 %d 支 VM（声明者 id %d）`（**只在第一次非 0 时打一行**）。
2. **弹不能从看不见的位置飞出来**：同样满火力下，不该有「凭空从自机两侧冒出来的子弹」；
   日志里应有 `sdk: 原生子机弹已拦（第 %u 发，膛口 = PLAYER+0x%x，机体 %d）`
   （限流：前 3 发 + 之后每 600 发一行 —— **一发不发的时候，这是唯一能看出「这条路在跑」的行**）。
3. **档 4 ⇒ 子机 4 格**（不是旧版的 1 格）；档 1/2/3 ⇒ 1/2/3 格；**档 0 ⇒ 一格都没有**。
4. **DPS 与灵梦同档一致**：把 111 分别装给魔理沙与灵梦，同档位打同一台敌机同样的时间 ⇒
   总伤害**量级**应当接近（魔理沙子机层 = 原生满档子机层）。⚠ 是「量级一致」，**不是**逐帧相同（见下）。
5. **换机体立刻恢复**（机体门的验收）：在**商店里**换成灵梦 → 回关 ⇒ 原生子机**当场**回来
   （**不依赖 111 再跑一次 tick** —— 换机体发生在商店里、那一刻卡 tick 冻着，所以门必须写在层里）。
6. **删卡 / 换关收干净**：买**倒吊人**删掉 111 ⇒ 原生子机**回来**（`ce_layer_drop_card()` 撤声明）；
   换关（`on_stage_start`）**不**撤（卡还在卡组里，声明应当继续生效）。
7. **其它三个机体一条都不该有**：灵梦 / 咲夜 / 早苗机体下，上面第 1、2 条的两行日志**一行都不该出现**。

### 5. 已知偏差（明写，**别当成复刻**）

| # | 偏差 | 量 / 为什么 |
| --- | --- | --- |
| ① | 逐档不等于原生（原生自己非单调）| 档 1/3 −11%、档 2 +39%、档 4 0%；四档均值 **+0.8%** |
| ② | 聚焦 / 非聚焦给的是**同一套**（格数与伤害都不分聚焦）| 相对原生聚焦档（5.250/帧）高 **+27%** |
| ③ | 原生子机行是**激光**（对同一条束每帧给 `damage`），我们是一发一发飞出去的**平弹** | Σdmg/rate 账下等价，但打一个**具体**敌人时「束一直贴着」与「弹一颗颗飞」**不等价**；这一层没有一手能对齐 ⇒ 登记为**已知近似** |
| ④ | 子机贴图仍是**借** 116 的（`SIGNAL_OPTION_SCRIPT`）| 旧账，未变（`TUNING.md` §2.1d）|
| ⑤ | 五档阵型的**几何**是 ❓ 自定 | 一手五张表在 TH11 的 `zPlayerOption+0x74`（`th11:0x75e4`），全 exe 只有 2 处**读**、**没有写点**、也没有数据镜像 ⇒ **取不到**（`TUNING.md` §2.1d）|
| ⑥ | 「原生子机要弄掉」这件事本身**不是复刻** | 是我们按用户口径做的**整层抑制**；一手只能定「原生子机从哪来、谁从它出弹」 |

### 6. 一手出处

`native/engine.h` 的 `CE_NATIVE_OPT_*` 一节（原生子机数组 4 格 / stride / 两个数组不相交）
与 `native/sdk.c` 的 `BP_ce_repopulate_native_option` / `BP_ce_native_option_muzzle` 上面那一整段
（站点字节、栈序、为什么**必须两条**）—— **这两处是唯一出处**。
⚠ 代码注释里许诺的跨层回流（`engine/sht/th18/03-func-tables-timers-and-dead-fields.md` §7）
**目前还不存在**（该文只到 §6）⇒ 原生子机数组这一手**暂时只活在代码注释里**，🟡 挂账。


## 2P. ★★★ 一轮实跑反馈的六条（2026-09-16m，✅ 已落地，等实跑）

> 用户一次给了六条（原话逐条抄在下面），本节是**根因 → 落点**的落地记录；共享件登记见
> `AUDIT.md` §BE 详录 #33，旋钮见 `TUNING.md` §2.5 / §2.1g / §2.1h。

| # | 用户原话 | 根因（查到的） | 落点 |
| --- | --- | --- | --- |
| ① | 「**灵击缺少特效**」 | 脚本 277/279 用的是 **`drawPolyBorder`(605)**：TH18 里它**没有一条实跑可见的实例**（原生 script35 只在主菜单那条路），而 604 有本仓实跑确认可见的先例（116 的 120/121）| 277/279 **换回 `drawPoly`(604)** + 补齐 `zWriteDisable` / `color2` / `alpha2`（照抄 120/121）；另加 `mof/fx:` / `signal/fx:` 一行 VM 字段诊断（下次一眼分辨「字段错」还是「图元不画」）|
| ② | 「**魔理沙还是有染红**」 | 大小姐的擦弹圈**红染**只在 `signal_graze_scan()` 那一支判了机体，而**落笔点**（`on_draw` 里的 `signal_tint_tick()`）**没有门** ⇒ 任何机体的圈内弹都被染红 | `signal_tint_tick()` 加**机体门**：非咲夜(2) ⇒ 只跑①（把染过的还回原色）、不再染新的 |
| ③ | 「**魔理沙子机的位置摆放不对，左倾严重，低 p 不对称**」 | 声明子机时取的是**阵型表的前 n 格**，而表是左右成对排的（0 = 最左）⇒ n<4 时必然偏左 | 新增纯逻辑 `signal_formation_cell_sym()`：**按对称轴取**（1 = 轴上 / 2 = 最内一对 / 3 = 最内一对 + 轴上 / 4 = 原表）；单测断言「任何 n 的 x 之和 = 0」|
| ④ | 「**魔理沙的子机缺乏贴图**」 | 子机贴图写死借 116 的 `SEASON_OPT`（**季节点**那一格图）⇒ 子机长得像一颗点道具 | 按**机体**选伙伴子机脚本（224 紫 / 225 爱丽丝 / 226 大小姐 / 227 文）—— 魔理沙 → 爱丽丝那套；❓ 两处自定（只有 4 位伙伴的切图；Σ 映射没有「当前是哪位伙伴」）|
| ⑤ | 「**依旧对话结束后发残碎**」 | 对话结束后 boss/相位重新出现 ⇒ 机器按设计开一段，几帧内又来一次边沿 ⇒ 这个**几帧的假段**被当成一整段结算、且必然「没死过」⇒ 攒够 2 段白撒一颗 | 段机加**最短段长门** `SIGNAL_SEG_MIN_FRAMES`（100 帧）：太短的段**只记诊断、不记账**；诊断行加 `lastlen`；新增假段回归用例 |
| ⑥ | 「**除了魔理沙河童特技的额外效果，其他的灵击就 == 110 灵击 即可**」 | 111 的灵击原来是 TH11 口径的自造物（一次性全屏消弹 4000 px + TH11 伤害源 + 爱丽丝 0.5 档）| 111 的灵击改成 **110 型**：消耗 / 半径曲线 / 逐帧消弹圈 / 每敌每帧伤害 / 时长 / 音效 / 视觉**全部与 110 同形**（纯数学直接调 `mof_core_*`）；**唯一保留** = 荷取（河童）退款窗口；**爱丽丝 0.5 档不再调用**（函数与单测留在 `signal_core.c`）|

**实跑要看什么**：① 按灵击 → 场上出现**一圈半透明的冷白蓝圆盘**（半径 16 → 160/112 px、120 帧）
+ **红/蓝四芒星**（每 6 帧一对），110 与 111 **长得一模一样**；② 魔理沙擦弹时弹**不再变红**；
③ 低 P 时子机**左右对称**（1 只 = 正前、2 只 = 左右各一）、且有**伙伴的子机贴图**（不是季节点）；
④ 关底对话结束后**不再掉命碎**（`signal/frag:` 行的 `lastlen` 应当是几百，不该出现十几帧的段）。
若还有看不见的，把 `mof/fx:` / `signal/fx:` 那两行发我 —— 它给出 VM 的 sprite / scale / pos / 颜色两路。

⚠ **仍未做（诚实边界）**：① 环的图元仍是 TH18 原生 `drawPoly`，**不是** TH10 那支 `sprite(sprite362)`
+ `texCircle(32)` 的竖条环（要完全对齐得再抠一次 sprite362，管线已在 `make_mof_bomb_art.py`）；
② 逐帧擦弹「完全调用引擎擦弹函数（含音效/掉点）」这一条**正在调研** `0x45e160` 的签名与副作用，
本轮**还没改**（用户口径见 `TUNING.md` §2.1g ①）。


## 2Q. ★★★ 111 的**河童罩子**：放 B 起水泡 + 没被撞退半档 + 原生 B 也带罩子（2026-09-16q，✅ 已落地，等实跑）

> **用户口径（2026-09-16 逐字）**：「放B是消弹，给一个**防护罩（有贴图特效）**，只要防护罩不撞认为是 no miss。
> **然后加强一下吧，对于原生的B，河童特效也会引入不撞破罩子 no miss 和一个罩子**」
> 一手全链（唯一出处）：[`engine/player/th11/02-support-special-effects.md`](../../../engine/player/th11/02-support-special-effects.md) §6；
> 逐项复刻/自定对照表与旋钮：`TUNING.md` §2.1j；共享资源登记：`AUDIT.md` §BE 详录 #34。

**这一轮同时**订正**了上一版的旧口径**（旧版把「用了灵击不中弹就不算用过」实现成「75 帧窗口 + 扣多少退多少」，
并把它标成 ❓ 自定）——一手真身不是标志位，而是**一支水泡特效 + 两条出路**：

| 事项 | 一手（TH11，逐条核过）| 上一版（❌ 本轮订正）| 本轮落点 |
| --- | --- | --- | --- |
| 罩子本体 | `pl01.anm` **script58** 水泡（`sprite76` = 254×254）| 无 | 新脚本 **294**（逐条移植，`layer(5)`→13 ❓）+ 新图集 entry 59 `NITORI_BUBBLE` |
| 寿命 | **920 帧**（脚本 `+900: delete()`）| 「窗口 75 帧」 | 脚本自己自删；卡侧每帧 `ce_anm_get_vm()` |
| 没被撞 | `collect_power(10)`（`th11:0x40784d`）= `BOMB_COST`(20) 的**一半** | 退**全额**（100）| 主动灵击**扣 100 退 50**；原生 B **不退**（它花的是库存、没花 P ⇒ 没有可退的东西）|
| 判据 | 「**泡的 VM 还在不在**」（`th11:0x407828`）| 「75 帧内 state ∈ {2,4}」| 每帧查 VM + `step()` 寿命兜底 |
| 被撞 | `mov [player+0x928],1` = **撤销中弹**（不扣命）+ 破罩 30 帧 | 无 | `+0x10` 里 `signal_cancel_impending_death()` + 30 帧外圈（32→322）/ 每帧 40 帧无敌 / 75 帧伤害源 |
| 破罩那一下的信仰槽 | `0x4a56f4` 被 state-4 分支清零 | 无 | `signal_core_reset(&s->slot)`（**不**写信仰 −100000 —— 那是真扣命那条路）|
| 原生 B | — | — | 新钩子 `on_bomb_spent`（`th18:0x4203bc`，位置与一手「bomb 起点起泡」同相）+ 机体门 = 魔理沙；**只给罩子、不退款**（花的是库存不是火力，用户 2026-09-16t 裁决）|

**实跑要看什么**

1. **魔理沙按灵击（或按原生 B）** ⇒ 自机身上出现一圈**半透明青色水泡**（约 254 px，缓慢自转），
   日志 `signal: 荷取·起泡（水泡脚本 294，寿命 920 帧…）` + `signal/shield: 泡 vm=… alive=1`。
   若日志说 `vm=0` ⇒ 是 VM 池满/脚本没起来（报我）；若 `alive=1` 但看不见 ⇒ 层/贴图问题（报我）。
2. **泡活着的 920 帧里一直没被打中** ⇒ 泡自己消失那帧出现
   `signal/shield: 泡活了 N 帧**没被撞** ⇒ 退 50 单位火力`，HUD 的 P 值 **+0.50**。
3. **泡活着时被弹打中** ⇒ **命数不掉**（罩子吃掉这一下）、泡播 30 帧胀大淡出、周身一圈 30 帧外扩的消弹、
   短暂无敌，日志 `signal/shield: **破罩**…`；这一发**不退款**（周期行 `denials` +1）。
4. **周期行**（每 `SIGNAL_LOG_EVERY` 帧一行）：`signal/shield: tick vm=… alive=… frames=… break_left=… breaks=… | 窗 active=… broken=… left=… pending=… refunds=… denials=…`

**★★ 2026-09-16u 追加的四条口径（用户逐字；同批已落地）**

1. **寿命按「暴露帧」计**：`invuln_cur > 0`（**原生 B** 的 120/40 帧、107/117 救援无敌、复活无敌）、
   `ce_no_hit_active()`（113 灵界闸门）、`player_state ∈ {2,3,4}` 的帧**不推进**罩子寿命
   —— 泡冻结（不缩小不淡出）但**仍跟着自机**。实跑看 `signal/shield: 罩子**暂停计时**（暴露 N 帧、暂停 M 帧）`。
   ⚠ 曲线（alpha/scale）因此**从脚本搬到卡侧**：anm 的时间标签是绝对的、没法暂停
   （`+900: delete()` 是第 920 帧这个**绝对时刻**）⇒ 一手那三条指令（`alphaTime(10,0,255)` /
   `scaleTime(20,0,1,1)` / `scaleTime(900,0,0.1,0.1)`）变成 `signal_nitori_bubble_alpha/scale_milli(age)`
   （纯函数、有单测；脚本 294 改成 `stop()` + 只管图元与破罩）。
2. **无敌期撞弹不算撞**（泡不破、寿命照停）：结构上本来就成立 —— 破罩的唯一入口是 `+0x10`，
   而无敌期引擎**根本不判弹**（判定函数在 `state ∈ {2,3,4}` / 无敌计时器 > 0 时早退）。
3. **两条路的「罩子没破时能不能再放」分开**（2026-09-16v 补）：
   · **灵击那条（库存 == 0、本卡自干）⇒ 挡掉**：用户逐字「如果放的是灵击，那么**罩子没破不让放**」
     ⇒ 泡还完好时按 X 什么都不做（**不扣火力**、不消弹、不响音效），一手同构
     （`th11:0x40651b` 的 `[bomb+0x3c] != 0` ⇒ `do_bomb` 返回 −1）；泡一破（被撞）就解禁。
   · **原生 B 那条 ⇒ 自动破掉换新的**：旧泡**按自己的记录结算**（没被撞过且当时真扣过 P ⇒ 退那一半；
     原生 B 起的泡没扣 P ⇒ 不退），**不计入「被撞」**（不动 no miss 判定）；旧罩子没演完的破罩演出
     会被新的一发 B 提前结束（否则新泡那几帧没人驱动 ⇒ 看起来「没出现」）。
4. **罩子不吃「卡自己造的死」**：113 的「中弹灵界」结束时会补一次 `Player__die`
   （`trance.c` 的 `trance_player_die()`），那一下会广播 `+0x10`、与撞弹同形 ⇒ 新增共享通报位
   `ce_forced_death_begin/end/active`（`sdk_core`，登记 `AUDIT.md` §BE 详录 #35）：113 在调用前后成对包住，
   111 的罩子在 `+0x10` 里查它、为真则**不吃也不破**。

**⚠ 诚实边界（❓ 自定 / 未做）**

- **再按一次灵击会换一个新泡**（旧窗作废、不退款）：一手 `Bomb::do_bomb` 在 `[bomb+0x3c] != 0` 时**返回 −1**
  ⇒ 原版罩子没结束前放不出第二发 B。我们的灵击是火力制的，选择允许换泡 —— 若实跑觉得「太强/太黏」，
  这一条是最该改的旋钮（改法：`signal_shield_arm()` 里现有泡就 return，即照一手锁住）。
- 泡的**255 alpha 照抄一手**（254 px 实心泡在弹幕里可能偏糊）——要降只降脚本 294 那条 `alphaTime` 的终值。
- 泡的**层**取 13（TH18 层号体系与 TH11 的 22 不同号）⇒ 若被弹幕盖住/盖住 HUD，改脚本 294 的第一行。
- 原生 B 那条**不退款**（一手那条退款的前提是「这次花了火力」，而原生 B 花的是库存）。
- **换泡**那条与一手不同（一手根本不允许连放）：`do_bomb` 在 `[bomb+0x3c] != 0` 时返回 −1 —— 想照一手锁住，改 `signal_shield_arm()` 一行。


## 2R. ★★★ 一轮实跑后的四条（2026-09-16u/v：罩子看不见、旋转/音效、残像渐变、罩子挡灵击）

> 用户原话（逐条）：①「放原生B的时候，**不消耗P**」②「**原来罩子是有持续时间的**」
> ③「B的无敌时间不算罩子的持续时间，此时撞任何弹幕都应该认为是没撞；其他原因的无敌**依旧不算**在
> 罩子的持续时间，**但是撞弹也不能就把罩子破了**」④「如果是能放B，那么罩子**自动破掉换新的**…得**追踪这个
> 罩子是消耗没消耗P**，看情况是否返还，**这个破掉不应影响 no miss 判定**」⑤「罩子原则上是**撞弹**的保护，
> 所以**不应当能抵消中弹灵界的结束灵界击杀**」⑥「**如果放的是灵击，那么罩子没破不让放**」
> ⑦（本条补充）「破罩演出中的 30 帧也**按照一手的来**（仍然不让放）」⑧实跑三条 bug：
> 「**魔理沙子机没有发弹**」/「**没有罩子贴图**——持续中的罩子应当是**逆时针旋转**并且逐渐缩小的，
> 返还东西的罩子返还是有**音效**的」/「**灵梦的残影**看起来是红色，原版更贴**洋红**并且有**时间渐变**
> （等价于不同残影的位置渐变），目前缺乏渐变」。

**实跑日志（`game/th18_card_expand.log`）给出的两个「看不见」的根因**（都修了）：

1. **原生 B 起泡时自机正在无敌期 ⇒ 寿命不推进 ⇒ 出场动画也停在原地**：
   日志 `signal/shield: 罩子**暂停计时**（暴露 0 帧、暂停 300 帧）…`，
   而 scale/alpha 是**第 0 帧**的值（0 / 0）⇒ 泡**根本画不出来**。
   修法：出场动画（淡入 10 帧 + 长大 20 帧）走 **real 帧**（`s_shield.intro`，封顶 20），
   视觉年龄 = `intro + 暴露帧` ⇒ 无敌期里泡**长大到 1.0 后冻住**，一按 B 就看得见。
2. **账本的 `active` 被当成了寿命**：原生 B 那条**不开退款窗** ⇒ `s_nitori.active == 0`
   ⇒ 第一帧就命中「`!active` ⇒ 结算」分支，日志 `罩子结束（寿命尽头；暴露 1 帧 / 暂停 381 帧）`
   ⇒ 泡在无敌一结束就被删掉。
   修法：寿命搬进 `s_shield.life_left`（**暴露帧**计数，与账本彻底解耦），账本只管「退不退钱」。

**其余改动**

- **旋转**：一手 `angleVel(0,0,-0.05235988f)`（−3°/帧 = 逆时针）改由**卡侧每 real 帧写**
  `ce_anm_set_rotation`（`stop()` 之后脚本角速度还有没有效没实证）；脚本 294 另补 `zWriteDisable(1)`
  （照本仓实跑确认可见的 279 / 120 / 121）。
- **音效**：退款 = `se_bonus2.wav`（TH18 **0x30**）、破罩 = `se_slash.wav`（TH18 **0x2c**）。
  一手 id（TH11 **0x33** / **0x26**）→ wav 名字的换算 = SE 表 `0x4a34f6`（8 字节/项 `{WORD 音量; WORD wav 下标}`）
  + 名字表 `0x4a36b0`：`0x33 → 42 → se_bonus2`、`0x26 → 30 → se_slash`。
- **灵击被罩子挡住**：判据 = 「这一发**还在进行中**」（泡在 / 要起泡 / **破罩演出那 30 帧**）
  —— 一手 `th11:0x40651b` 的 `[bomb+0x3c] != 0`（要等 `[bomb+0x2c] > 30` 才清）；拦在扣火力之前 ⇒ **不扣任何代价**。
  原生 B 那条**不走这个闸**（用户口径「能放 B ⇒ 自动破掉换新」）。
- **残像渐变**：每支残像入池记年龄（`s_trail[]`，40 格），每帧按**自己的年龄**写
  `signal_trail_rgb(age)`（TH11 `pl00.anm` script19 的 `color(255,64,64)` → `colorTime(15,0,64,64,255)` 整数等价式，
  纯函数 + 单测），**只换 RGB、保留 A** ⇒ 脚本自己的 `alphaTime` 淡出照跑。

**★ 2026-09-16w（实跑三 bug 的第二轮修）**

1. **罩子看不见的两个根因**（都从实跑日志坐实）：
   · 出场动画被寿命的暂停**冻在第 0 帧**（`暴露 0 帧、暂停 300+ 帧` 而 scale/alpha 还是 0/0）；
   · 账本的 `active` 被当成寿命（原生 B 那发**不开退款窗** ⇒ 第一帧就命中结算分支
     `罩子结束（寿命尽头；暴露 1 帧 / 暂停 381 帧）`）。
   ⇒ 出场走 **real 帧**（`intro`）、寿命走**暴露帧**（`life_left`）、旋转每 real 帧写
   （逆时针 −3°）、退款/破罩各加一声 SE（`se_bonus2` / `se_slash`）。
2. **残像渐变**：`s_trail[]` 池按**每支自己的年龄**写 RGB（TH11 `pl00.anm` script19 的红→蓝斜坡），
   只换 RGB、保留脚本的 A（淡出照跑）⇒ 拖影呈现「最新红 → 旧洋红/蓝」。
3. **子机不发弹 —— 根因已坐实（子 agent 独立复核一致）：弹的 VM 层 = 0 ⇒ 画在 3D 背景那一趟**。
   卡子机弹的 VM 进世界列表、绘制按 `vm+0x18` 分桶（`render_layer` `0x488260`；游玩区桶 = 层 12–19），
   而**这条路没人替弹设层**（`AnmManager__instantiate` `0x407420` 两参、无层参数、显式保留 `vm+0x18`；
   空闲池 `memset` 0）⇒ 脚本不写 `layer()` 的弹层 = 0 = **弹幕区里一颗都看不见**（弹对象/伤害源都正常）。
   对照：**子机本体**由引擎强设层 14（`Player__allocate_option` `0x40a8bc`）⇒ 子机不写层是对的，
   「弹也不用写」这个推广就是坑。TH11 的引擎自己传层（`th11:0x4341e3 push 0x16`）⇒ 移植时把这条丢了。
   ⇒ 修法：288..292 补 `layer(13)`；并**加硬门**（`assets/sht/append_shooterset.py`：
   shooter 行 `+0x22` 指向的脚本必须写 `layer(12..19)`，负向验证已过）。
   同时新增两行诊断：`signal/opt2: … 本组弹 VM 的**层** = N（应当 13；0 = 脚本没写 layer）` 与
   `signal/opt2: 引擎调用 on_tick_shooters(+0x1c) 时给的计时器 short=… long=…；本卡喂…` ——
   ⚠ **「在飞几颗」不是可见性的判据**（弹对象/伤害源在 create 里就建好了，layer 错只是不画）；
   真判据 = 弹 VM 的**层**读数。
   ~~另注：**继续喂 `s->frame`**，别改喂引擎的 short（它只有 0..14，而我们的 `fire_rate=60`
   ⇒ 会变成「每行每 15 帧一发」，节奏与伤害 ×4）。~~
   ★ **2026-09-16zb 撤销上面那一条**：`fire_rate = 60` / 「每格 4 行 stagger」本身就是**误读**（那几条取自
   `pl01a.sht`，而元素弹在 `pl01b.sht`）⇒ 现在**就喂引擎给的 short**（取值域 0..14、周期 15），
   行的 `fire_rate` 改成 TH11 元素行的 **3**、每格 **1 行** ⇒ 「15 帧 5 发/格 = 满档 20 发/15 帧」，
   与 TH11 的发弹数逐值相同。一手表与对账：`assets/sht/append_shooterset.py` 的 `MARISA_FORMATION_*`、
   `TUNING.md` 111 行、`AUDIT.md` §BE 2026-09-16zb 那条。

**实跑要看什么**

- 按 B（原生或灵击）⇒ **立刻**出现一个半透明青色水泡（约 254 px），**缓慢逆时针自转**、随时间**逐渐收缩**
  （920 暴露帧里 1.0 → 0.1）；原生 B 期间日志打 `罩子**暂停计时**…` 而泡**看得见且不缩小**。
- 泡没被撞到期 ⇒ 一声 `se_bonus2` + `退 50 单位火力`（只有灵击那条）；被撞 ⇒ 一声 `se_slash` + 破罩演出。
- 泡还在进行中时按灵击 ⇒ `signal: 灵击**被罩子挡住**（… 破罩演出剩 N 帧）⇒ 这一下不放`，**P 值不动**。
- 文·超高速 ⇒ 拖影从**红**（最新）渐变到**洋红/蓝**（最旧）。


## 2S. ★★ **覆盖原则阶梯**：117 > 113 > 108 > 111（2026-09-16z；✅ 已落地）

> **用户口径（逐字）**：「改子机和改主体发弹的覆盖原则：1、**特殊大于常态** 2、**专用大于通用**
> 108，**仅低速妖机体 > 111 魔理沙**；113 灵界 or 117 咆哮 > 108 & 111；**人为规定 117 > 113**」。
> 契约与阶梯表：[`EVENTS-CONTRACT.md`](EVENTS-CONTRACT.md) §4b；共享件登记：`AUDIT.md` §BE 详录 #37。

**四个争用面与落点**

| 争用面 | 参与方 | 谁赢（实现点）|
| --- | --- | --- |
| 主炮 shooterset（fire-time set index）| 117 咆哮 / 113 灵界 | **117 > 113**：`shot_set_index_priority`（`CE_SHT_PRIO_117` 20 > `CE_SHT_PRIO_113` 10）+ `sdk.c` 取最大 |
| 原生主炮「本帧不发」| 108 低速妖机体 / 109 | 只在没人覆盖 set 时才被问到 ⇒ **117/113 > 108** |
| 子机外观落笔（pose 层）| 108 低速 / 111 魔理沙 | **108 > 111**（SDK 选举 **id 最小者**；108 仅低速声明 ⇒ 「仅低速」）|
| 子机开火闸（`th18:0x40a9c0`）| 113 灵界 / 117 咆哮 | 两家都是「窗口内一律停火」⇒ 结果一致 |

**实跑对账**：同帧两家都要换主炮组时会打一行
`ce: shot set contested: N claimers, winner id=… prio=… set=…`（现在应当是 **id=117 prio=20**）。


## 2T. ★★ 106 红魔乡两条新设计（2026-09-16za；✅ 已落地）

> **用户口径（逐字）**：「红魔乡卡再加一条设计：**上线收点当且仅当满P**
> 协同效应：**除非满P，不加信号强度**」。
> 一手（TH06 反编译 `ItemManager.cpp`）：`state==1 || (128 <= currentPower && player.y < 128.0f)` → 进吸引态
> ⇒ **满 P + 在线上** 才是过线收点的充要条件 ✅。旋钮与边界：`TUNING.md` §2.8；共享件登记：`AUDIT.md` §BE #38。

| # | 规则 | 落点 | 消费者 |
| --- | --- | --- | --- |
| ① | 上线收点当且仅当满 P | 两条 `ja 0x445f63`（`th18:0x445bb9` / `0x446003`）在声明开着时**不执行** | `sdk.c` 的 `BP_ce_poc_gate_a/b` |
| ② | 除非满 P，不加信号强度（**只停「线上补满」那一条**）| `signal_step_mod_t.no_line_topup` → `faith_signal_step_gated(..., 0)` | 111 的 `signal_step_mod_fill()` |

**实跑看什么**：不满 P ⇄ 满 P 各一行 `T06R: 上线收点闸 **开**/**关**（power N/M）…`。
不满 P 时：**站到回收线上方道具不会被吸走**（照常下落）、**信号槽不会被「线上补满」推到 1.00**
（擦弹照涨、已满档的小额加值照给、线下衰减照旧）；满 P 时一切照旧。

**边界（写清楚免得下一个人误读）**：回收线**本身照旧可见**（我们不碰 `player+0x47998`，是「精确门」而不是
「把线抬走」）；这条门**与 111 神奈子的降线 / 原生 22 号卡无关**（它们改的是线高，我们改的是「过线算不算」）。
