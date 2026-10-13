# CARD-BEHAVIOR-AUDIT —— 逐卡行为审计（P18 / W3）

> **版本**：TH18 v1.00a（mod 侧 `th18:0x400000`）；文中跨作地址一律带 `thNN:` 前缀 ✓。
> 工单 **P18**（见 [`WORK-PACKAGES.md`](WORK-PACKAGES.md) §0.1）：**只读审计，不改任何代码** ✓（R1/R4）。
> **目的**：给每张系统卡一份「**游戏内行为 ↔ 代码站点 ↔ 一手出处**」的对照账，供 P15 拆成
> `CARD-CODE-<id>.md`（代码端）与 `PLAYER-GUIDE.md`（玩家端）时直接引用 ✓。

## 方法与口径

- **一卡一个只读审计员**（12 张卡），每人必须给 `文件:行号` 级站点 + 一手出处（`thNN:0x…` 或文档章节）；
  按用户 2026-10-14 的建议用 **workflow 前台执行**调度（`await parallel` 栅栏 ⇒ 全部结算才继续 ✓）。
- **等级**只能取：`一手`（注释/文档里有具体地址）/ `复刻`（照一手语义、落点是 TH18 引擎实现）/
  `自定`（代码或注释自认 ❓自定、或 owner 口径）/ `未测` / `不确定` ✓。
- **不确定就写不确定** ✗ 不猜；审计员**不给改进建议** ✓（那是后续批次的事）。

> ⚠ 本台账是**审计原文的浓缩**：12 份完整报告里的「字段清单 / 泄漏分级 / 长论证」不在这里 ✗；
> P18 的姊妹篇是 [`LIFECYCLE-AUDIT.md`](LIFECYCLE-AUDIT.md)（生命周期未测面）与 [`VM-IDIOM-MAP.md`](VM-IDIOM-MAP.md)（VM 直写普查）✓。

---

### 106

**一句话**：红魔乡（EOSD_PLAYRANK）是被动整蛊卡：把 th06 的 PlayRank 十二路增减照 1:1 记账（用户定案：**不写任何引擎字段、不产生任何游戏效果**）、聚焦时不显示自机判定点、不满 P 时过收点线不吸道具且整局不画那条收点指示线，另在 DLL 自己的 rodata 里留一处零影响的「冴月麟」字符串彩蛋。

| 行为 | 何时触发 | 代码站点 | 一手出处 / 理由 | 等级 |
| --- | --- | --- | --- | :-: |
| 聚焦/低速时不显示判定点（把判定点指示 VM 的 color_1 alpha 写 0） | 每帧绘制阶段 on_draw 被调、游戏线程在、自机指针非空、自机处于聚焦、且 player+0x476b0 的 anm 句柄非 0 | native/cards/th06rank.c:491, native/cards/th06rank.c:495, native/cards/th06rank.c:497 | th18:0x45b433（聚焦且 player+0x476b0==0 时建 script 0x1a / layer 0xe 的 VM 并写回句柄，0x45b45d 每帧写 scale）；隐藏手段「alpha=0」= … | 复刻 |
| PlayRank 的每帧上升通道：帧数条件 +1.00 与擦弹 +0.06 | 每个正常游玩帧（ce_item_frame 0x446ee3）且无对话框 ⇒ stage_frame++；当 stage_frame % (2400−残机×240) == 0 且 stage_frame != 0 时 … | native/cards/th06rank.c:213, native/cards/th06rank.c:398, native/cards/th06rank_core.c:94 | th06:EnemyManager.cpp:162-170（`subrankIncreaseFrame = 2400 − livesRemaining*240`、`Inc(100)`，整段被 `g_HasCurrent… | 复刻 |
| 逐颗道具的 rank 增：小 P / 蓝点（金钱） / 命碎 / B 碎 | on_item_collect（0x44625f，type 分派前）逐颗道具；type 1 = 小 P、2 = 蓝点、4/0x10 = 命碎、6/0x11 = B 碎；type 3 大 P 与其它 type 不加 | native/cards/th06rank.c:263, native/cards/th06rank.c:288, native/cards/th06rank_core.c:124 | th06:ItemManager.cpp:205 `Inc(1)`（小 P）/ 234 `Inc(30)` / 238 `Inc(3)`（蓝点两档）；th18 道具 type 分派表 = 0x44625f + 跳表 0… | 复刻 |
| 出屏 −0.03 与「整合资源」正向库存增量（整命 +2.00 / 整炸 +0.05，含同窗口碎片净减抑制） | on_item_frame 每帧扫道具池：槽 state != 0 且（\|pos.x\| ≥ 200.0 或 pos.y > 472.0）即记一次 −0.03；on_tick_2 每帧对 CURRENT_LIVES … | native/cards/th06rank.c:244, native/cards/th06rank.c:411, native/cards/th06rank_core.c:196 | th06:ItemManager.cpp:145 `Dec(3)`（出屏）、291 `Inc(5)`（整炸）、299 `Inc(200)`（整命）；出屏几何一手 = th18:0x445e13/0x445e20/0x4… | 复刻 |
| 放 Bomb −2.00（含决死 B）与非决死 Miss −16.00（看救援层 acc 让位） | on_bomb_spent（0x4203bc）每次炸弹消耗；on_death_after_deathbomb（虚表 +0x0c）在决死窗口后仍死时触发，且传入的 acc == 0（前面没有卡取消这次死亡） | native/cards/th06rank.c:422, native/cards/th06rank.c:438, native/cards/th06rank.c:442 | th06:Player.cpp:185 `Dec(200)`、225 `Dec(1600)`；救援层 acc 契约一手 = th18:0x45c116 `push edi` / 0x45c121 `or edi,eax… | 复刻 |
| 每关开场按 th06 重置 rank 状态（subRank = 0、关卡帧计数 = 0、min/max 按难度重设并撤销库存差分基准） | on_stage_start；另在 on_tick_2 每帧检测难度位 0x4ccd00 变化，变了就按新难度表重设（练习/特别模式切档保险） | native/cards/th06rank.c:455, native/cards/th06rank.c:180, native/cards/th06rank_core.c:155 | th06:GameManager.cpp:357 `mgr->subRank = 0;`（每关开场）+ 345-347（rank/min/max 只在开新 run/通关时按难度表重设，关卡之间保留）；spec §2.2… | 复刻 |
| 收点线控制：不满 P 时过线不吸道具（对 PoC 高度层声明「遮」，落笔值 = min(语义高度, 自机 y)）+ 106 在场整局不画那条开局收点指示线 | on_tick_2 每帧算 gate = (CURRENT_POWER < [0x4ccd3c] MAX_POWER)，gate=1 时 ce_poc_declare_mask(c,1) 并 ce_full_p_gat… | native/cards/th06rank.c:380, native/cards/th06rank.c:157, native/cards/th06rank.c:521 | ① 一手：TH06 ItemManager.cpp 的道具 tick「state==1 或（128 ≤ currentPower 且 positionCenter.y < 128.0）⇒ 进吸引态」⇒ 满 P + 在线… | 自定 |
| 冴月麟彩蛋：往本 DLL 自己的静态字符串上再写一次同值「冴月麟」 | on_load 一次（sakuyue_write_once），零逻辑影响、不进任何判定、卡面文案不提 | native/cards/th06rank.c:82, native/cards/th06rank.c:91, native/cards/th06rank.c:167 | spec §2.4（用户 2026-09-10 新加：选一块不被读取、不参与任何判定的区域，优先 = 本卡 DLL 的 static const char）——无原作一手（TH06 无此物） | 自定 |

**成本 / 冷却**：
- 发动成本 / 冷却：无（被动卡，CE_CARD 只挂回调，没有 on_activate / active_recharge / 充能字段）（触发：—（被动常在，无发动动作）；站点：native/cards/th06rank.c:534, native/cards/th06rank.c:536；实现即此（CE_CARD(106, …) 的钩子集合里无主动卡字段）；卡表 patch/th18/cards.js 的 "106"（internal_name EOSD_PLAYRANK、price_tier 10））
- Miss（非决死）代价：rank −16.00（触发：决死窗口过后仍死亡、且前面的卡没有取消这次死亡（acc == 0）；站点：native/cards/th06rank.c:442, native/cards/th06rank_core.c:69；th06:Player.cpp:225 `DecreaseSubrank(1600)`；挂槽语义 = 虚表 +0x0c on_death_after_deathbomb（非决死），spec §4）
- 放 Bomb（含决死 B）代价：rank −2.00（触发：每次炸弹消耗（ce_bomb_spent 0x4203bc）；站点：native/cards/th06rank.c:422, native/cards/th06rank_core.c:69；th06:Player.cpp:185 `DecreaseSubrank(200)`（决死 B 与普通 B 走同一条），spec §3）
- 不满 P 的代价：过收点线不吸道具，且 111「线上补满信号强度」同时被关（协同效应的负面侧）（触发：CURRENT_POWER < MAX_POWER（随 sht 变，如 128/400）时的每一帧；站点：native/cards/th06rank.c:377, native/cards/th06rank.c:380, native/cards/signal.c…；TH06 ItemManager.cpp 吸引态的充要条件（满 P + 线上）；TUNING §2.8 ②（用户口径「除非满 P，不加信号强度」）；AUDIT §BE-ET）

| 数值 | 值 | 单位 | 站点 | 出处 |
| --- | --- | --- | --- | --- |
| 难度表：每关开场 rank 初值 / minRank / maxRank | EASY 16 / 12 / 20；NORMAL、HARD、LUNATIC 16 / 10 / 32；EXTRA 16 / 14 / 18 | 存储（rank 整数；min/max 即夹取上下限） | native/cards/th06rank_core.c:14, native/cards/th06rank_core.c:42, native/cards/th06rank_c… | th06:GameManager.cpp:70-76 `g_DifficultyInfo` 逐行；spec §3（并订正 EASY 下限是 12 不是 10） |
| 帧数条件的间隔 | 2400 − 残机数 × 240 | 帧（关卡时间轴帧；残机钳 0..10 防除零） | native/cards/th06rank_core.c:86, native/cards/th06rank_core.h:59, native/cards/th06rank_c… | th06:EnemyManager.cpp:162-170（`10*4*60 − livesRemaining*4*60`） |
| 擦弹 rank 加成 | +0.06 rank（存储 6000，单位 1/100000 rank） | 存储（PERMILLE 常量 T06_INC_GRAZE）；显示观感 = 每 16 次擦弹进 1 档 | native/cards/th06rank_core.h:72, native/cards/th06rank.c:402 | th06:Player.cpp:1394 `Inc(6)`（原值 6 = 0.06 rank）；用户 2026-09-11 定「不除」；AUDIT §AW2 订正过一次（原写 60，小 100 倍） |
| 蓝点（item type 2 ≙ 金钱）rank 加成：线上 / 线下两档 | 线上 +0.06 rank（存 30000 ÷ 5）；线下 +0.006 rank（存 3000 ÷ 5） | 存储（÷5 前是 th06 原值 30 / 3，运行期 ÷ T06_RATIO_DIV=5） | native/cards/th06rank_core.h:78, native/cards/th06rank_core.h:80, native/cards/th06rank_c… | th06:ItemManager.cpp:234 `Inc(30)` / 238 `Inc(3)`；÷5 归一化 = spec §3 用户 2026-09-10 定（TH18 道具量 ≈5×）；分档读自机 y 对回收线… |
| 小 P（item type 1）rank 加成，及 110/111 在场合的 ×5 | 基础 +0.002 rank（存 1000 ÷ 5 = 200）；装了 110 或 111 时 ×5 ⇒ +0.01 rank | 存储（PERMILLE） | native/cards/th06rank_core.h:82, native/cards/th06rank_core.c:111, native/cards/th06rank.… | th06:ItemManager.cpp:205 `Inc(1)`；×5 = 用户 2026-09-11 定、spec §8（判据 ce_owned(110)\|\|ce_owned(111)） |
| 碎片通道（TH18 自有，不读难度表）：命碎 / B 碎的逐颗 rank 加成 | 命碎 +0.70 rank（存 70000）；B 碎 +0.02 rank（存 2000） | 存储（PERMILLE）；命碎 3 颗 = +2.1 rank，B 碎 3 颗 = 1 张 | native/cards/th06rank_core.h:75, native/cards/th06rank_core.h:77, native/cards/th06rank.c… | spec §3 / §3.1（用户 2026-09-10 定的定值；「每命碎片数 = 3」一手见 spec §3 的 `0x4b40c8`/`0x4b3ff4` 表）；type 4/0x10 与 6/0x11 同通道 … |
| 12 条增减里的其余六条（整命 / 整炸 / 出屏 / Miss / Bomb 的量） | 整命 +2.00（存 200000）；整炸 +0.05（存 5000）；出屏 −0.03（存 3000）；Miss −16.00（存 1600000）；Bomb −2.00（存 … | 存储（PERMILLE；单位锚点 = th06 原值 × 1000） | native/cards/th06rank_core.h:74, native/cards/th06rank_core.h:85, native/cards/th06rank_c… | th06:ItemManager.cpp:291 `Inc(5)` / 299 `Inc(200)` / 145 `Dec(3)`；th06:Player.cpp:185 `Dec(200)` / 225 `Dec(1… |
| 出屏的几何判据（决定 −0.03 何时记一次） | \|pos.x\| ≥ 200.0 或 pos.y > 472.0 | 世界坐标 float（常量 0x4b9340 = 200.0f、0x4b93a4 = 472.0f） | native/cards/th06rank.c:71, native/cards/th06rank.c:238, native/cards/th06rank.c:240 | th18:0x445e13 `comiss`(y vs 472.0) + `ja 0x445e38`；th18:0x445e20 `movss xmm0,[edi+0xc2c]` + `andps`（清符号位）+ 0x… |

**跨卡协同**：
- **110 风神录 / 111 地灵殿（小 P 的 rank 加成 ×5）**：两张卡把 P 点改成 +0.05 火力 = 5× ⇒ 106 的小 P rank 也 ×5（+0.002 → +0.01）；判据只用 SDK 存在性查询 ce_owned(110) \|\| ce_owned(111)（站点：native/cards/th06rank.c:263, native/cards/th06rank_core.c:112；spec §8「小 P 的 ×5 规则」（用户 2026-09-11 定）；spec-review §3.1 把它列为「不算跨卡耦合」的例外条）
- **111 地灵殿（共享件「未满 P」声明层）**：106 每帧 ce_full_p_gate_declare(106, gate)；111 读 ce_full_p_gate_active() 置 signal_step_mod_t.no_line_topup ⇒ 关掉「线上未满就补满并钳到 100.00」那一支（擦弹 / 衰减照旧）；声明是粘性的、按卡 id 记账，删卡/局末 106 显式撤（站点：native/cards/th06rank.c:377, native/cards/signal.c:2194, native/sdk_core.h:560；TUNING §2.8（用户 2026-09-16za 口径「协同效应：除非满 P，不加信号强度」+ 边界表）；AUDIT §BE 详录 #38 / §BE-ET）
- **原生卡 22（Kanako）与任何 PoC 高度声明者（收点线 / 指示线）**：两条：① 不满 P 时 106 的「遮」是覆盖位，无条件压过别家（原生卡 22 / 111）声明的收点线高度，落笔 min(语义高度, 自机 y)；② 106 在场整局清 `Gui+0x194` bit 0x2000 ⇒ 不画那条开局指示线（该 bit 的唯一原生置位者就是原生卡 22 的 on_load，且只置不清；106 每帧的落笔会把它重新清掉，幂等）（站点：native/cards/th06rank.c:380, native/cards/th06rank.c:157, native/sdk_core.h:211；WORK-PACKAGES §7-A / A2 / A4；一手 = th18:0x40c288（原生卡 22 置位，engine.h 记「只置不清」）与 th18:0x43af0b（唯一读点）；AUDIT §BE-ET）
- **36 CardNarumi / 37 CardPatchouli（每关开场的碎片补货）**：不经过道具池、on_item_collect 抓不到 ⇒ on_stage_start 里按 ce_owned(36) 补 +0.70（命碎）、ce_owned(37) 补 +0.02（B 碎），每次关卡开场各一次（站点：native/cards/th06rank.c:459, native/cards/th06rank.c:460；spec §8「每关开场补货」（用户 2026-09-11 定：算获得、给 rank）；无 th06 一手（TH18 侧接缝））
- **原生救援卡 23 Eirin / 35 ShikiEiki（虚表 +0x0c 的 acc 契约）**：106 的 on_death_after_deathbomb 收到 acc != 0 时不记 Miss、不扣 −16.00（引擎把前面卡的返回值 OR 进 acc）；残余 = 排在 106 之后的救援者救下时 106 仍会记一笔假 Miss（原生 35 不看 acc）（站点：native/cards/th06rank.c:438, native/cards/th06rank.c:439；th18:0x45c116 `push edi` / 0x45c121 `or edi,eax` / 0x45c127 `test edi,edi`；原生样板 th18:0x40a4f3（CardEirin 第一条 `cmp [ebp+8…）

**已知未测**：
- 判定点 alpha=0 的实跑观感：ANM 脚本若每帧重设颜色需靠 on_draw 每帧重写压住，AUDIT §AN6 只标 🟡 待实跑，仓库里没有 106 的实跑回报。
- 满 P 闸改走「遮」之后的四条实跑验收（不满 P 过线不吸 / 满 P 照吸 / 计分逐位不变 / 开局看不到那条线）—— AUDIT §BE-ET §4 明文交给 owner，未见回报；SINGLE-CARD-TEST §th06 #1 仍写着旧的 §BE-EP 修法，状态陈旧。
- 每帧扫 CE_ITEM_SLOTS = 4696 槽道具池做「已越界未删」预测的帧耗：无任何实测数据（同池的 112 在 AUDIT §AT 里也标「帧耗未测」）。
- 「遮」的两个自定值本身没有实跑验证：CE_POC_TINY_BITS = 0x00800000（最小正规 float）与「钳位上限 = 自机 y、余量 0」都只在 TUNING §2.0t 自标 ❓自定 + 一条帧内载荷假设上成立。
- 擦弹差分里 `if (d > 200) d = 200` 的阈值 200 无一手来源，代码注释自承是「一场对话/一帧挤进太多」的防溢出护栏。
- 帧内顺序的未知量：ItemManager 的 UpdateFunc 优先级全仓无记载（th06rank.c:319-320 自承）⇒「碎片窗口」跨帧收口正确性只有推理；碎片净减抑制的边界（3 碎 = 2.1、净 0 不抑制）只在主机单测里验过，没有实跑对账。

---

### 107

**一句话**：收道具（原生消弹小分道具 = ★）、结界中擦弹与自机弹命中一起攒**樱点**与 **+C**；+C 满 50000 自动开**森罗结界 540 帧**（结界中每帧全屏收点、压暗背景、自机红白闪），中弹或按 B 打**灵击**（崩结界 + 全屏消弹由引擎撒原生小分道具 + 40 帧无敌，中弹时取消本次死亡），自然到期则樱点/最大樱点各 +10000 并结算 Border Bonus；樱点本身是该卡的「最大得点」（吃一颗点道具的得分 = 樱点），HUD 为原版两行计数器。

| 行为 | 何时触发 | 代码站点 | 一手出处 / 理由 | 等级 |
| --- | --- | --- | --- | :-: |
| 收下原生消弹小分道具（★ 通道）与自机弹命中 → 樱点与 +C 上涨 | on_item_collect 收到 TH18 type 9–0xe；on_shot_damage_any（断点 0x45f2a2） | cards/senro.c:1424, cards/senro_core.c:219, cards/senro.c:1448 | th07:0x43370b→0x433712（★ = 樱点/+C 各 +20，type 号 2026-09-15 按跳表块归属订正）；命中 th07:0x420638–0x420660 定 p、0x4211bd–0x4… | 一手 |
| 自建樱点实体（挂共享点池）：满 P 转换与敌机掉落槽轮转表出「大樱点」，符卡结束那一大笔消弹出「小樱点」 | faith_convert_full_p_items() 满 P 每帧；on_enemy_death 且 32 项轮转表轮到 7；符卡结束下降沿 + 本帧无人认领消弹 ≥ 8 | cards/senro.c:1913, cards/senro.c:1847, cards/senro.c:1376 | 大樱点值 th07:0x433886/0x433895（1000+100×符卡奖励）；掉落轮转表 th07:0x49efa0 逐字节（engine/item/th07/01-sakura-items.md §4.3）；… | 复刻 |
| +C 满 50000 自动展开森罗结界（540 帧）；正在放炸弹则排队到炸弹结束 | 樱点加法器里 plus ≥ 50000；或已排队且本帧不在放炸弹 | cards/senro_core.c:120, cards/senro_core.c:172, cards/senro.c:1341 | th07:0x42f5f7 闸门 / 0x42f623 cmp +C,0xc350 / 0x42f647 夹 / 0x42f652 call 0x441960；排队 th07:0x44196c/0x441983；时长 … | 一手 |
| 结界持续期三件：每帧全屏自动回收道具（含自建绿点）、背景层压暗、自机贴图红/白每 2 帧闪 | senro_border_active() 为真时每帧（on_tick_2 / on_item_frame / on_draw） | cards/senro.c:1342, cards/senro.c:941, cards/senro.c:979 | 压暗曲线 th07:0x44158d–0x44162f（engine/spirit/th07/01-barrier-visual.md §4.3）；红白闪 th07:0x441553–0x44158a（同文 §4.2）… | 复刻 |
| 结界中擦弹 → 樱点与最大樱点各 +80（高速）/ +30（低速），不动 +C | 原生擦弹计数（0x4ccd0c）差分 > 0 且结界展开中 | cards/senro.c:1353, cards/senro_core.c:205, cards/senro_core.c:207 | th07:0x43ed5f–0x43eda9（高速 0x50、低速 0x1e，两条分别走 0x42f789 / 0x42f736，都不动 plus）；低速位 = [player+0x240b]（senro_core.h… | 一手 |
| 中弹 / 按 B = 灵击：结束结界 + 全屏消弹（引擎 mode 1 撒原生小分道具）+ 40 帧无敌；中弹时取消本次死亡 | on_death_before_deathbomb（卡虚表 +0x10）或 X 键上升沿（CE_ADDR_INPUT_PRESSED bit 0x002），且当时在结界中 | cards/senro.c:1195, cards/senro.c:1211, cards/senro.c:1224 | th07:0x441bd0（打断收尾）/0x441d8e（[player+0x23fc]=0x28）/0x440d1d（中弹门）/0x4419d2（按 B 门）；消弹 mode 1 的真值表 = engine/bull… | 一手 |
| 结界自然到期结算：樱点与最大樱点各 +10000、得分 += 樱点、播公告 `Border Bonus %7d` 与 SE | 结界计时递减到 0（非中弹、非按 B 的收结界） | cards/senro_core.c:180, cards/senro.c:1083, cards/senro.c:1090 | th07:0x441670（0x44167a 最大樱点 +10000 / 0x441689 樱点 +10000 / 0x44169d–0x4416c6 分数 / 0x4416ee Plus=基准）；公告 kind 4 … | 一手 |
| 左下角原版版式 HUD：上行 +C（5 位）、下行 樱点 / 最大樱点（6 位，≥1000000 时 7 位）+ 固定标签贴图；结界中 +C 变色放大 + 脉动花；樱点满值弹 `CherryPoint Max!` | 每游玩帧 on_draw（主菜单/商店里先收 VM 再返回） | cards/senro.c:1606, cards/senro.c:1620, cards/senro.c:1644 | th07:0x404690（AsciiManager::sub_404690）逐指令版式 / 定宽右对齐 / 结界指示三件：engine/spirit/th07/02-cherry-counter-hud.md §2、… | 复刻 |

**成本 / 冷却**：
- Miss（真死）扣樱点：按 1/2 扣、上限 100000、向下取整到 10；不动 +C、不动最大樱点（触发：on_death_frame2（卡虚表 +0x14，死亡结算之后）；站点：cards/senro.c:1494, cards/senro_core.c:360, cards/senro_core.h:150；th07:0x440e84–0x440ef0（比例来自 [SHT+0x1c]：灵梦/魔理沙 0.5、咲夜 0.33；CAP 0x440eb2/0x440ec4）——TH18 不读 SHT 表，本卡 ❓自定取 1/2（senro_core.…）
- Bomb 扣樱点：❓自定按 1/4 扣（一手是十二个炸弹各自 0.17–0.41）（触发：on_bomb_spent（断点 0x4203bc，consume_bomb 之后）；站点：cards/senro.c:1461, cards/senro_core.c:377, cards/senro_core.h:155；th07:0x440ad9（扣完夹回基准）；各炸弹 ratio 表见 review §9.3 —— 本卡未逐炸弹取值，senro_core.h:155-157 标 ❓自定 / TODO(th07)）
- 灵击的代价：整条结界被消耗（结界结束、计时清零、+C 归零）（触发：中弹 / 按 B 灵击；站点：cards/senro_core.c:338, cards/senro_core.c:341, cards/senro.c:1184；th07:0x441bd0（0x441d4f [player+0x240d]=0 / 0x441d5c [player+0x2408]=3 / 0x441da4 Plus=基准）；无结界时按 B 只放原生炸弹（senro.c:1184））
- 救援层的代价：中弹那一刻无条件烧掉结界（不看是否抢到任何闩锁），与 117 同时、顺序 107 → 117（触发：on_death_before_deathbomb（救援层）；站点：cards/senro.c:1481, cards/senro.c:1482, cards/senro.c:1475；EVENTS-CONTRACT.md §5.2（用户 2026-09-12 定案「一次中弹把救援者全部烧掉、顺序 107 → 117」，:212–:253）；代码里必须丢弃 ce_rescue_note_claim 返回值的理由见 sen…）

| 数值 | 值 | 单位 | 站点 | 出处 |
| --- | --- | --- | --- | --- |
| +C 满槽阈值（开结界门） | 50000 | 存储（+C 绝对值，0..50000） | cards/senro_core.h:106, cards/senro_core.c:120 | th07:0x42f623 的 0xc350；senro_core.h:57-58 记全 exe 只有 4 处 |
| 森罗结界时长 | 540 | 帧（= 9 s @60fps） | cards/senro_core.h:107, cards/senro_core.c:98 | th07:0x4419fa mov [结界对象+0x16a08],0x21c（senro_core.h:107-119 逐指令；同处推翻旧的 420 帧口径） |
| 灵击后的无敌 | 40 | 帧（≈0.67 s） | cards/senro_core.h:120, cards/senro_core.c:342 | th07:0x441d8e [player+0x23fc]=0x28；走 SDK 无敌声明层（取 max） |
| 结界自然结束加值 | 10000（樱点与最大樱点各一次） | 存储 | cards/senro_core.h:121, cards/senro_core.c:180 | th07:0x44167a（最大樱点）/ 0x441689（樱点） |
| ★（消弹小分道具）加值 | 20 | 存储（樱点与 +C 各 +20） | cards/senro_core.h:127, cards/senro_core.c:222 | th07:0x43370b→0x433712（type 号按跳表块归属订正为 type 6） |
| 大樱点（type 7）加值 | 1000 + 100×符卡奖励次数 | 存储（樱点与 +C 各一份） | cards/senro_core.h:125, cards/senro.c:1878 | th07:0x433886 起、0x433895 imul ecx,ecx,0x64 |
| 结界中擦弹加值 | 80（高速）/ 30（低速） | 存储（同时加到樱点与最大樱点） | cards/senro_core.h:130, cards/senro_core.c:205 | th07:0x43ed96 的 0x50 / th07:0x43ed7c 的 0x1e |
| 点道具（蓝点）分值 = 本卡的「最大得点」 | 50000 + max(0, 樱点−50000)/5，向下取整到 10 | 显示 / 得分 | cards/senro_core.h:134, cards/senro_core.c:315 | th07:0x43310a 的 0xc350（线上满额）/ 0x4331c8 idiv 5 / 0x4331db 起 idiv 10 取整；不做 th07 的线下按高度递减（本卡 spec §4.2 偏差表） |

**跨卡协同**：
- **110 风神录 / 111 地灵殿（共享信仰点池 cards/faith.c）**：107 以 FAITH_SLOT_107=3 注册成第三个消费者：满 P 转换时一颗 P 道具同帧给每个 conv_value>0 的注册者各撒一颗（107 撒大樱点）；结界全屏收点也调 faith_collect_all(~0) 顺手收绿点；删卡/换关按「先 faith_unregister 再清自己那一槽」的顺序收（站点：cards/senro.c:1895, cards/senro.c:1913, cards/senro.c:1698；faith_core.h:29（FAITH_SLOT_107 = 3）；SENRO-107-CHERRY-ENTITY.md §2.3/§3；用户口径「同时转化出来」）
- **112 星莲船（ufo_bounty.h 等价计数广播）**：112 压缩包被吃下时广播代号 + 逐颗账；107 每帧按自己槽（1<<FAITH_SLOT_107）读该代日志、用原值走本卡自己的收下回调折进本卡账（按 owner 定案这是唯一入账路径）（站点：cards/senro.c:1292, cards/senro.c:1299, cards/senro.c:1314；ufo_bounty.h:31-62（代数广播协议）；AUDIT.md §BE 详录 #225/#226）
- **117 动物灵（霊撃）/ 113 灵界（救援层）**：中弹走救援层：107 在自己的 +0x10 里无条件烧掉结界并要 cancel_impending_death，与 117 同时、顺序 107 → 117；全屏消弹只给第一家（107 拿账）、无敌走声明层取 max；113 排在这之后（站点：cards/senro.c:1481, cards/senro.c:1194, cards/senro.c:1211；EVENTS-CONTRACT.md §5.2（:212–:253，用户 2026-09-12 定案）；AUDIT.md §AS4）
- **同帧消弹的其他卡（117 霊撃 / mof.c 灵击 / pofv.c / signal.c）**：共用 SDK 的「本帧已消过弹」闩锁：ce_cancel_latch_claim(107) 先到先做、后到只收结界；消到 0 颗时 release 交回，不白挡同帧后面的卡（站点：cards/senro.c:1194, cards/senro.c:1199, cards/senro.c:1241；sdk.h:1070 的用法示例；EVENTS-CONTRACT.md §5.2 细则 2（:250-:251）；CARD-NATIVE-CONFLICT-AUDIT.md §G）
- **计分层（SDK ce_score_apply + 110/111）**：107 只声明自己那一份未耦合加数 G_07 = senro_point_item_value()，并按 CE_COEFF_FAMILY_1 归一（th07 樱点项不吃收分高度折扣），由 SDK 一次合成，不再与 110/111 争落笔权（站点：cards/senro.c:1124, cards/senro.c:1129, cards/senro.c:1138；SCORE-LAYER-REWORK.md §0/§1；EVENTS-CONTRACT.md:31、:102；CARD-NUMERIC-INTERACTION-AUDIT.md §1.6/§6（声明-合成机制本身是本仓工程口径，数值公式才…）

**已知未测**：
- 「结界中每帧全屏自动收点」在 cards/senro.c:1141 只引到区间 th07:0x4414xx + 用户口径，没有逐指令地址 ⇒ 一手性未核。
- 注释与代码/文档漂移，未对账：头注「7 s」（cards/senro.c:20）vs cards/senro_core.h:107 的 540 帧 = 9 s；HUD 块宽注释仍写 200/286（cards/senro.c:303-306、:1746）vs 代码 SENRO_HUD_W 280（:308）；SENRO_HUD_PIN_Y 注释史与 AUDIT.md §BE #241 记的 932 对不上代码的 928（cards/senro.c:324）。
- SENRO_DIM_ENABLE 代码为 1（cards/senro.c:198），而同文件 :902-905 注释仍写「默认关」；且压暗一手曲线的触发值 ds:0x4d44e0 全 exe 找不到写点、原文自标 🟡（engine/spirit/th07/01-barrier-visual.md §4.3）⇒ 遮罩是否等于原版未实跑验证。
- ❓自定数值无一手：小樱点触发的 ≥8 颗门与 24 颗封顶（cards/senro.c:108-109）、樱点满值公告 30 帧节流（cards/senro.c:424）、震屏 20/12/0（cards/senro.c:149-153）、Miss 1/2 与 Bomb 1/4（cards/senro_core.h:150-157）。
- cards/senro.c（1999 行）没有卡侧单测：tests/test_senro_core.c 只覆盖纯逻辑 senro_core.c（:1-35）；HUD VM 池、faith 池、世界层特效、公告通道的引擎侧行为无主机证据。
- 中弹灵击的消弹推迟一帧、且 claim 失败只记日志（cards/senro.c:1230-1250）——当前靠「117 只在后续帧消弹」的现状成立，AUDIT.md §AX3 已登记为跨卡缺口，未实跑验证将来有卡在 +0x10 就消弹的情形。

---

### 108

**一句话**：低速（按 Shift 聚焦）时整机切成 TH08 妖侧——本体外观、低速子机、逐（机体×火力档）的妖侧出弹组一起换；高速回原机体；同时把敌人的每个发射口按 1:1 长成可打的「使魔」真敌人，打死它那个出点就哑火，使魔挨的伤害转嫁给母机，另带 TH08 的人妖槽（含逢魔）、永点（刻符）与夜点系统。

| 行为 | 何时触发 | 代码站点 | 一手出处 / 理由 | 等级 |
| --- | --- | --- | --- | :-: |
| 人/妖形态整机切换（本体脚本 + 出弹组 + 低速子机一起换） | 按/松 Shift（真实聚焦 = player+0x476cc）；放真 B 的持续期内、或 113 灵界 / 117 暴走咆哮进行中时侧别被锁住 | youkai.c:7251, youkai.c:7127, youkai.c:4351 | 一手：换本体脚本 th08:0x44b12b push 5 / th08:0x44b385 push 0；按 player+3 选 plyNNas.sht = th08:0x450f6f；youkai_core.h §… | 一手 |
| 敌人发射口 1:1 映射成「使魔」真敌人（NoEnemyDebug 惰性空壳 + 卡侧跟随 VM 画四色原生切图） | ECL etOn 把一个发射口槽写活（站点 th18:0x433ffa，人/妖两侧都建） | youkai.c:1247, youkai.c:7484, youkai.c:1392 | spawn 参数表 docs/superpowers/plans/2026-09-10-ufo-card-feasibility.md §1.1；壳 = NoEnemyDebug(wait(1000))、免出框删除 t… | 复刻 |
| 打死使魔 ⇒ 那个发射口哑火（夹闸 + 主复位口） | on_enemy_death 收到 KILLED / BOMB_KILL 且死者是我们建的壳；放开只在波次边沿（enmKillAll）、etOn 间隔边沿、符卡结束下降沿、或 600 帧 TTL 到期 | youkai.c:7791, youkai.c:7834, youkai.c:7667 | ❓自定：闸本身是 owner 2026-10-01 口径，一手只给了 th08:0x42adc4「使魔数 == 0 就不吐点」这条参照；TTL 600 帧见 youkai_core.h:1046（标 ❓自定） | 自定 |
| 使魔挨的伤害转嫁给母机（现行 100%，一手 1/2 旋钮保留） | 每帧差分使魔 life.current(enemy+0x6220)；时符/耐久符卡/母机无敌时整笔丢进 dropped 不转 | youkai.c:3805, youkai.c:3898, youkai.c:3938 | ❓自定：youkai_core.h:1455 明写「TH08 没有这条管线，不是复刻，spec §3.7」；1/2 出处 local/_scratch/research-108-damage-transfer.md，🟡… | 自定 |
| 给没有发射口的大怪补「只绕转、不发弹、无体术」的使魔 | 扫场见到 HP 档/面数达标的大怪；台数 = clamp(HP 档 + 面加成, 1, 18) | youkai.c:2506, youkai.c:2362, youkai.c:2671 | owner 2026-09-28 口径（youkai.c:1720 起长注）；壳/「打得掉但无体术」照一手 familiarCreate（th08:0x41a840 hitboxSet 24.0、无 hurtboxSe… | 自定 |
| 人妖槽（走槽/击破/收刻符/擦弹/死亡清零）+ 逢魔（+100 分/帧、掉 1 颗刻符、自机弹命中掉 1 颗、妖侧弹染红）+ HUD | 每帧（聚焦↔高速边沿、按住/松开射击、放 B）；gauge ≤ −8000 或 ≥ +8000 = 逢魔 | youkai.c:5797, youkai.c:6074, youkai.c:9396 | 一手：漂移状态机 th08:0x44bdf0–0x44bffb；击破 ±200 = th08:0x42da2a；收刻符 ±111 = th08:0x4413cb；擦弹 +100 = th08:0x44aa81；逢魔 1… | 一手 |
| 永点（刻符）：击破撒环 / 死亡点吸收区消弹吐符 / 收集 → PIV → 额外最大得点 | 击破敌机、敌人死亡点、擦弹、極人时自机弹命中（经 on_bullet_hit 队列）、刻符被自机抓取 | youkai.c:6099, youkai.c:5967, youkai.c:9412 | 一手：击破批 = th08:0x42b048（环 R=128、state 3 先上抛）；吸收区 = th08:0x42b1b8 push 7/0x10（R=32.5、16 帧）；收集 = th08:0x4412b0；P… | 一手 |
| 人侧撞到使魔 = 一次普通 MISS（妖侧可穿梭） | 人侧、自机存活态（player state 1）、与映射使魔的距离进判定半径；同一只只触发一次 | youkai.c:3150, youkai.c:3174, youkai.c:3181 | owner 口径（TH08-ITEMS-AND-SC.md §2b）；一手半径 = player+0x4799c 非聚焦 3.0 + 使魔 hurtbox 24.0 的一半 12.0；调 th18:0x45d3a0 P… | 一手 |

**成本 / 冷却**：
- 共享低速子机池：12 格公共池里本卡最多占 4 格，拿不到槽就等 60 帧重试（触发：低速时按机体/档声明 1..4 台子机；池被别人占满或关卡中途装卡；站点：youkai.c:169, youkai.c:170, youkai.c:3991；上界 4 = 数量表最大值（youkai_core.h:144–149）；「12 格公共池要和 71/77/116 分」是 youkai.c:170 的登记（🟡 未实跑）；重试 60 帧 = youkai.c:168（照 77））
- 各文件级静态表上限：映射使魔 32 / 绕转使魔 48 / 外部使魔采样 32 / 刻符 1024，满即静默放弃（触发：同时存在的实体数超过表容量；站点：youkai_core.h:947, youkai_core.h:2402, youkai.c:3687；YK_FAM_MAX 注里明写「工程护栏，不是游戏性数值，spec 没给上限」；YK_ORBIT_MAX/YK_XFAM_MAX 同为 ❓自定；表满路径分别在本文件里（绕转=宁可不补 youkai_core.h:1279、xfam=只放弃…）
- 击破魔使的代价：人妖槽 −gauge/12、掉 1 颗刻符、之后 50 帧内收刻符不给槽（触发：我们的使魔被玩家弹/炸弹打死（且妖侧因免伤位打不动 ⇒ 只在人侧发生）；站点：youkai.c:7852, youkai.c:7854, youkai_core.h:2220；一手：th08:0x42b213 neg / :0x42b216 mov ecx,0xc / idiv ⇒ delta = −gauge/12（没有 10%/5% 分支）；th08:0x42b29b SpawnItem(type7,sta…）
- 转嫁落空：读不到母机或 boss 时整笔 pending 进 dropped（这部分伤害白打）（触发：使魔掉血但母机已消失/查不到，且全局 boss 也取不到；站点：youkai.c:3704, youkai.c:3698, youkai.c:3699；一手实跑证据写在 youkai.c:3699：owner 那局 `xfer[tick 5706 map 0/0 orb 538/11391 ext 0/0 rect 0 … noboss 275]` ⇒ 11391 点伤害全被 nobos…）

| 数值 | 值 | 单位 | 站点 | 出处 |
| --- | --- | --- | --- | --- |
| 低速子机数量表（机体 × 火力档） | 霊夢 {1,1,1,1,1} / 魔理沙 {1,1,1,1,1} / 咲夜 {4,4,4,4,4} / 早苗 {0,1,1,1,1} | 台（存储） | youkai_core.h:144, youkai_core.h:145, youkai_core.h:148 | 各机体妖侧各自的原作数量：TH08 pl00/pl01/pl02 妖侧（八雲紫/アリス/レミリア）与 TH16 pl03（射命丸文）；位置另取自本作 pl0X.sht 原生阵型（youkai_core.h:157 起） |
| 人妖槽量程 / 上限 | ±10000（span 10000） | 槽值（存储；HUD 条宽 112px、图标 56px 相对它算） | youkai_core.h:1521, youkai_core.h:1522, youkai_core.h:1523 | th08:0x44d9ee 0xd8f0（−10000）/ th08:0x44da09 0x2710（+10000） |
| 逢魔（极人/极妖）判定门 | 极人 gauge ≤ −8000 / 极妖 gauge ≥ +8000 | 槽值（阈值，含等号） | youkai_core.h:1524, youkai_core.h:1525 | th08:0x44d9f7 0xe0c0 / th08:0x44da12 0x1f40；谓词 th08:0x406d10 / 0x406d70 |
| 击破敌机的人妖槽增量 | 聚焦 +200 / 非聚焦 −200 | 槽值（每只） | youkai.c:6074, youkai.c:6071 | 一手 decomp EnemyManager.cpp:670-677 + th08:0x42da2a 一族；youkai.c:6071 说明 AddToYoukaiGauge 第一道闸读的全局恒 0 ⇒ 不做闸 |
| 收一颗刻符的人妖槽增量 | ±111（聚焦 + / 否则 −） | 槽值（每颗） | youkai_core.h:1529, youkai.c:5324 | th08:0x44139e :0x4413b0 → th08:0x4413cb；被击破魔使后的 50 帧静默门挡（th08:0x44138e–0x44139c） |
| 击破魔使的人妖槽增量 | −gauge/12（除数立即数 0xc，向零截断） | 槽值（每台魔使） | youkai_core.h:2218, youkai.c:7852 | th08:0x42b213 neg / :0x42b216 mov ecx,0xc / :0x42b21b idiv —— owner 的「10%（妖梦 5%）」口径与一手不同，裁「按一手」 |
| 伤害转嫁比例 | 100（%）；一手 1/2 的旋钮值 50 保留可切回 | %（存储；YK_TRANSFER_DEN=2 是一手分母） | youkai_core.h:1467, youkai_core.h:1456, youkai_core.c:1132 | 100 = owner 2026-10-01 定案（「使魔多 ⇒ 不能减伤」）；50 = 一手 1/2，出处 local/_scratch/research-108-damage-transfer.md，🟡 th08:… |
| 发射口哑火后的自动重建期限（TTL） | 600 | 帧（≈10 s @60fps，存储） | youkai_core.h:1046, youkai.c:9674 | ❓自定（owner 2026-10-01）；一手环境里使魔是每波 familiarCreate 重建，没有「按时间自动放开」这一条 |

**跨卡协同**：
- **113 灵界（Trance）/ 117 暴走咆哮（Animal）**：这两张卡「在进行中」时 108 把侧别钉住：读共享跨卡标志位 ce_card_flag_any(CE_CARD_FLAG_TRANCE / _ROAR) 决定是否放/收妖子机、换不换妖侧出弹组（站点：youkai.c:413, youkai.c:414, sdk_core.h:1011；sdk_core.h:1002 起写明「谁的状态谁写（113/117 置位），别人只读聚合」；owner 2026-10-01 口径 ④ 在 youkai.c:353 起）
- **111（魔理沙机体卡）**：共享「原生低速子机」声明层：108 低速时 ce_native_option_hide_declare + ce_native_option_keep_records，111 也声明隐藏时按 OR 语义、且 108 id 小者在声明层选举中优先（站点：youkai.c:9449, youkai.c:9456, youkai.c:9459；sdk.h 的 ce_native_option_* 一族（出处 AUDIT §BE #32）；选举/优先级 = sdk/public.h 的阶梯表（108 默认 0、111 −1））
- **106（红魔乡）**：卡组里存在 106 时，决死 B 多扣 1 个（总消耗 2，clamp ≥ 0、bombs=1 也可决死）；无 106 则一个字节都不写（站点：youkai.c:7914, youkai.c:7940, youkai.c:7943；owner 2026-10-11 口径「当有 106 的时候，决死 B 消耗两个」＋「不会扣到负数」「最后一个 B 也可以决死」；106 判据 = 只读 AbilityManager 卡链表（不改共享件））
- **全屏消弹的共享仲裁（109/112/113/116/117…）**：死亡点吸收区吃弹前先看 ce_cancel_latch_owner()：本帧别人已经拿了那一笔 ⇒ 108 整帧不吃弹（只观察、不认领闩锁），避免同一批弹被两方各算一次（站点：youkai.c:5976, youkai.c:5977, youkai.c:5978；youkai.c:5921 起的仲裁说明；闩锁语义/认领归属见 SDK 的 ce_cancel_latch_owner）
- **88（Jera 杰拉）刻符复制代数闸**：108 每帧 ce_dup_publish 上报活着的刻符数；Jera 的代数一变，ce_dup_should_run 为真 ⇒ 108 把每个活刻符按同坐标/同出生态/同「逢魔直接吃掉」标志各重撒一颗（站点：youkai.c:8697, youkai.c:8698, youkai.c:8673；youkai.c:8666 起的说明：语义见 sdk_core.h 末节与 cards/jera.c 文件头；唯一出生口是 yk_orb_spawn_ex）

**已知未测**：
- 使魔皮肤的两个偏移 enemy+0x1490 / +0x149c 全反汇编里一条内存引用都没有，且 0x149c 与本仓记的 ECL 局部变量 I1 撞址 ⇒ 语义未证实（当前是死代码，占位 −1，youkai.c:99–116）。
- 文件头写明的 abs_pos 双写（每帧同时钉 enemy+0x1270/+0x1274 与 +0x12b4/+0x12b8）：哪一格才是可见位置的源头没有一手证据，🟡 待实跑（youkai.c:24–28、youkai.c:1392–1401）。
- 使魔跟随 VM 的层号 7 与「VM 位置 = 实体 final_pos」只在代码层对过账，未实跑（文件尾占位清单 4，youkai.c:9758–9762）。
- 绕转使魔的台数阈值表与 HP 缩放旋钮是 ❓自定（YK_ORBIT_HP_SCALE=1，表 {100,100,300,600} 取自 TH08 familiarCreate 实参），阈值本身待 TH08 一手统计替换（youkai_core.h:2420–2512）。
- 发射口闸的两条自定时间参数未实跑：600 帧自动重建 TTL 与波次间隔阈值（YK_EMIT_WAVE_GAP 24 帧 / YK_EMIT_WAVE_RATIO 3×）（youkai_core.h:1046、youkai_core.h:1072、youkai_core.h:1087）。
- 时符抑制转嫁的名字闸（符卡名以「時符/时符」开头）实测 TH18 的 *.ecl 里 0 命中 ⇒ 这条路径从未触发过，判据未验（youkai.c:3748–3754，登记 AUDIT §BE #212）。

---

### 109（东方花映塚）

**一句话**：花映塚 × 獣王園 合成卡——按住 `C` 蓄力并展开固灵结界，松手按档放 C1–C4 攻击；击破/消弹掉灵力道具攒槽，场上循环生成自建灵。

| 行为 | 何时触发 | 代码站点 | 一手出处 / 理由 | 等级 |
| --- | --- | --- | --- | :-: |
| 按住 C 蓄力（Shift 单开结界），结界半径 = 蓄力 × K/100 | 按 C / Shift 的电平 | `pofv.c:1219` `:1224` `:6631` | K 取一手 §7.2（灵梦 64 / 魔理沙 14 / 咲夜 64 / 早苗 128）；角色读点 `0x4cccf4` | 复刻 |
| 松手放档 C1–C4，按角色分派（c1 / c2m / c2s / c3m / c3s / c4） | 松 C 且蓄力 ≥ 阈值 | `pofv.c:6733` `:6821` `:4144` | 一手 `pl00.sht` 的 `+0x2c/+0x30/+0x34/+0x38` × 10 | 复刻 |
| 杂鱼/灵被击破 ⇒ 暴风（逐帧扩张消弹场 + 半径伤害 dmg 1） | `on_enemy_death`（非 boss） | `pofv.c:6121` `:6035` `:2979` `:8326` | 一手 `th19:0x50c3c2` 連爆；半径律暂借 `th09:0x41fe93` | 一手 |
| 击破 / 结界碰弹 / 消弹 ⇒ 撒灵力道具；拾取入账 +15 灵力 | 死亡回调 + 结界扫描 | `pofv.c:5782` `:1327` | 一手 `FUN_00521d60` 撒布实参（−π/2、2.2）；`+15` 一手 | 复刻 |
| 灵（純霊 / TH09 幽灵 / 动物灵）周期生成；结界罩到 ⇒ 沈静化 500 帧 | 每 1800 帧；结界命中 | `pofv.c:6493` `:6202` `:6242` | 自写 ECL `CESpirit109`；沈静化一手 `GhostDisableMove` / `lifeSet` | 复刻 |
| 罩子 = 一次撞弹豁免（撤销中弹 + 反向位移 + 100 帧无敌） | `on_death_before_deathbomb` | `pofv.c:8639` `:8652` `:8668` | 撤销一手 `th11:0x4078c4`；破罩一手 `th19:0x544c90` | 一手 |

**成本 / 冷却**：
- 主动充能 60 帧（`pofv.c:74`/`:8680`）；`on_activate` 恒 `CE_ACTIVATE_REFUSED`（`:8526`）⇒ 按 C 本身不发动、不消耗。
- 灵力 = 攻击槽兼蓄力上限（满 2500）：拾 1 颗道具 +15（`:1320`）；放 C2/C3/C4 扣 500/1200/2000（`:6831`）。
- 灵力初值 = C1 = 500（`:1316`）；EX 发射门槛 ≥ 100、冷却 60 帧（`:251`/`:252`）。

| 数值 | 值 | 单位 | 站点 | 出处 |
| --- | --- | --- | --- | --- |
| `.active_recharge` | 60 | 帧 | `pofv.c:74` | ❓自定（只满足主动卡契约） |
| 灵力上限 | 2500 | 灵力 | `pofv.c:1317` | 一手（`0x9c4` = 2500） |
| 每颗灵力道具 | +15 | 灵力 | `pofv.c:1320` | 一手 `FUN_00520d40` |
| C1/C2/C3/C4 阈值 | 500/1000/1700/2500 | 灵力 | `pofv.c:4144` | 一手 `pl00.sht` 偏移 × 10 |
| 结界半径 | 蓄力 × K/100，K = 64/14/64/128 | px | `pofv.c:1238` | 一手 §7.2（咲夜暂用圆 ❓） |
| 灵循环周期 | 1800 | 帧 | `pofv.c:479` | ❓自定（owner 未定数） |

**跨卡协同**：
- 106 整蛊卡：在场 ⇒ 打开 `s_z_th09_mode`，Z 走 TH09 续命模型（`pofv.c:294` / `:7017`）。
- 111 地灵殿：与 109 同写 `bullet+0x28 + CE_ANM_VM_COLOR1`；109 的 `on_draw` 先跑 ⇒ 111 的红压本卡（`pofv.c:8482`，❓未解决）。
- 108 红魔乡：共用 `on_native_shot_suppress` 断点 `th18:0x45ebfb`；109 在 `:7042` 清 Z 位前先留 `ce_input_z_frozen` 给 108。
- 共享件：`bomb10_cancel_impending_death()`（`:8652`）、`faith_scatter_from_enemy()`（`:5782`）、`ce_hud_slot_claim_card_ex()`（`:942`）。

**已知未测（代码里没有一手/没有实跑证据的点）**：
- 暴风半径律仍借 TH09 节点值；一手 TH19 Bomb tween 的键值未取到（`pofv.c:21`）。
- 一手「enemy flags bit27 ⇒ 额外道具 n = 1」的例外未接（`pofv.c:5798`）。
- 破罩音效 `0x1b` 是 TH19 SE 号 ⇒ 未落地（`pofv.c:8658`）。
- 按 C 的引擎拒绝音效未去掉：`return 0` 曾实跑卡死 ⇒ 已退回 `REFUSED`（`pofv.c:8523`）。

---

### 110（东方风神录）

**一句话**：装备卡——吃绿点养「信仰」（= 最大得点与计分基），击破指定宿主妖精撒大信仰点；炸弹库存为 0 时按 B（或主动卡 C）发动「灵击」消弹 + 伤害，决死窗口内可救命。

| 行为 | 何时触发 | 代码站点 | 一手出处 / 理由 | 等级 |
| --- | --- | --- | --- | :-: |
| 收绿点 → 信仰 + 连击槽 + 蓝点分入卡内奖残 | 点被自机收集 | mof.c:460-510、517-528 | 一手 th10:0x41b5ae / 0x41b602（值、绿色）；蓝点分按 §7.6 口径 | 复刻 |
| 击破宿主表敌人 → 撒大信仰点 + 绿色 `+N` 弹窗 | 敌人真死亡 | mof.c:679-779、411-453 | 一手 th10:0x41b8cc 值表；判据换成 TH18 宿主表 `(anm 源, 主脚本)` | 复刻 |
| 逐帧信仰衰减（三条门 + 有/无击破两档速率） | 每帧 | mof.c:1280-1300、354-367 | 一手 th10:0x418740-0x41875d；门③帧源 TH18 无对位（🟡 自定） | 自定 |
| 灵击：库存 0 按 B（上升沿）/ 主动卡 C | 按键上升沿 | mof.c:1355-1363、1728-1745 | 一手 th10:0x425964 链；救命 th18:0x45d590 | 复刻 |
| 决死窗口内发动 ⇒ 决死形态（救命 + 不扣信仰） | 窗口 `state==4` | mof.c:1355-1361、1140 | 一手 th18:0x45d4e4 窗口；决死支无 `call 0x412e70` | 复刻 |
| P 点 +0.05 档火力（两卡同装 id 最小者落笔） | 道具 commit | mof.c:891-917、900 | 用户 2026-09-15 裁决；门槛/下限非 TH10 原版 | 自定 |

**成本 / 冷却**：
- 灵击火力：常态门槛 ≥2.00 档 / 扣 1 档 / 下限 1.05 档；111 在场 + 魔理沙 ⇒ 门槛 1.50 档 / 扣 50 单位 / 下限 1.05（`mof.c:1075-1102`、`mof_core.h:743-746`）
- 灵击信仰：非决死 −3000；决死形态 0；miss −(信仰−50000)/3；符卡超时全避 −30000（`mof.c:1140`、`1306`、`1337`；`faith_core.h:497-499`）
- 冷却：无自定冷却；`.active_recharge = 1` 只是 SDK 主动卡入场券（`mof.c:1767`）；X 路径只受 `CE_CURRENT_BOMBS() <= 0` 约束（`mof.c:1355`）

| 数值 | 值 | 单位 | 站点 | 出处 |
| --- | --- | --- | --- | --- |
| 信仰基础 | 50000 | 显示值 | faith_core.h:37 | 一手 th10 存储钳 5000 的显示形态 |
| 大信仰点 | 5000 / 8000 / 10000 | 信仰（E/N · H · L/EX） | mof_core.h:119 | 一手 th10:0x41b57f |
| 小信仰点 | 100 | 信仰 | faith_core.h:259 | 一手 th10:0x41b602 |
| 连击槽满 | 130 | 帧 | faith_core.h:308 | 一手 th10:0x413035 |
| 灵击持续 | 296 | 帧 | mof_core.h:683 | 🟡 脚本 `time` 累加，非逐条 |
| 灵击半档门槛 | 150 | 火力单位（1 档 = 100） | mof_core.h:746 | owner 2026-09-24；mof.c:1091 文案写 1.55，与常量不符 |

**跨卡协同**：
- 111（地灵殿）：共用绿点实体池 `faith.{h,c}`；耦合计分 `ce_score_declare_coupled` + 擦弹倍率/信号强度由层合成（`mof.c:375-389`）
- 111：灵击只有一份实现 `bomb10.{h,c}`，两卡同装 id 小者驱动/落笔（`mof.c:1171-1178`、`1361-1363`、`900`）
- 111：组合 HUD 共用 `faith_hud.{h,c}`（`mof.c:1473`、`1517`）；P 点补值走共享策略 `faith_p_topup_take()`（`mof.c:903`）
- 112（星莲船）：压缩包等价计数按本卡槽折算入账（`mof.c:1202-1221`）；绿妖精换皮/撒点载体共用 `faith.c`，110 或 111 单装都生效（`mof.c:1240`、`1550`）

**已知未测（代码里没有一手/没有实跑证据的点）**：
- 灵击伤害在 TH18 用矩形独立伤害源（`ce_damage_rect`）近似一手「伤害累加器」，量纲未与 TH10 逐条对账 🟡（`mof.c:1012-1032`）
- 门③「关卡帧 ≥ 90」用卡自数帧（TH18 无 `zGameThread` 字段图），待实跑 `sf=` 栏钉 🟡（`mof.c:114-138`）
- 灵击持续 296 帧来自脚本文本累加 🟡；尾部另有 80×30 帧计数循环，真寿命可能更长（`mof.c:1038-1039`）
- 灵击无视觉、不消激光；微档信仰点不建实体（`ce_cancel_radius()` 不给位置）——均登记「未做」（`mof.c:80-84`、`544-549`）

---

### 111（东方地灵殿）

**一句话**：把 TH11 的通信强度槽 + 信仰/蓝点分 + 灵击/命碎 + 12 伙伴支援特技整层搬进 TH18：擦弹与站位养一条 0–130 的信号槽，满槽全收点，槽值当计分乘数。

| 行为 | 何时触发 | 代码站点 | 一手出处 / 理由 | 等级 |
| --- | --- | --- | --- | :-: |
| 信号槽每帧推进 / 满槽全收 | 每帧按擦弹帧差 + 自机相对回收线档位推进；槽 ≥100.00 ⇒ 全部道具切 state 3 | `signal.c:3351-3398`、`signal.c:3433` | 档位 `th11:0x420a70`、擦弹倍率 `th11:0x41f4e0`、满槽 `th11:0x42364a` | 一手 |
| miss 归零 + 信仰惩罚 | 真 miss（`+0x0c`，acc==0）→ 槽清零 + 信仰显示 −100,000 | `signal.c:3225-3250`、兜底 `signal.c:3404-3408` | `th11:0x4327d0` 同一个函数：`0x432807` 清零、`0x4327e7` 减 100000 | 一手 |
| 灵击（X / C 双入口，含决死与河童罩子） | 库存 == 0 且 X 上升沿，或主动卡 C 键；魔理沙额外起泡 | `signal.c:3443-3450`、`signal.c:4069-4086`、`signal.c:1055-1097` | 入口 `th18:0x45c048`；灵击体 = 110 那份（`bomb10.h` 逐条 th10 地址）；泡 `th11:0x407680` | 复刻 |
| 绿点三档入账 | 击破掉落撒大绿点 +100；满 P 吃 P 点撒中绿点 +10；消弹每 100 颗 +10 | `signal.c:640-713`、`signal.c:731-765`、`signal.c:493-507` | `th11:0x4240f0` 满 P 转换表、`th11:0x40b0a2` 小绿点档 | 一手 |
| 命碎段机 | 每 2 个「没死过」的 boss 攻击段（相位）撒 1 颗 `type 0x10` 道具 | `signal.c:3417-3430`、`signal.c:606-628` | 位置/角度 −π/2/速 2.2 = 一手 `th11:0x4101b7`；「每 2 段」= 用户口径 | 自定 |
| 支援特技 12 伙伴 + 帕秋莉子机层 | 按机体门每帧分派（灵梦/魔理沙/咲夜/早苗各 3 伙伴）；魔理沙子机格数随火力档 | `signal.c:3452-3468`、`signal.c:2592-2624`、`signal.c:3093` | 用户拍板的 Σ 映射与格数/DPS 归一（非复刻）；子机贴图走原生 `th18:0x40a790` 第 5 栈参 | 自定 |

**成本 / 冷却**：
- 灵击：门槛 ≥2.00 档 / 扣 1 档（100 单位）/ 下限 1.05 档；魔理沙（爱丽丝）= 0.5 档（50 单位）——`signal.c:1560-1573`、`faith_core.h:326-328`。TH11 一手是 per-shot 20 单位且**无下限** ⇒ 这条是映射，不是复刻。
- C 主动卡：`.active_recharge = 1`（`signal.c:4094`）——SDK 主动卡机器的入场券，与 116/118 同值，不是新定的冷却；返回 0 = 瞬发。
- 河童罩子：寿命 920 暴露帧（无敌帧不推进）、破罩再 30 帧演出 + 40 帧无敌；没被撞退半档火力——`signal_core.h:796`、`signal_core.h:810`、`signal.c:1094-1096`。

| 数值 | 值 | 单位 | 站点 | 出处 |
| --- | --- | --- | --- | --- |
| 信号槽上限 | 130.00（130000） | 千分点 | `faith_core.h:371` | 一手 `th11:0x40bab5` |
| 满槽阈值 | 100.00（100000） | 千分点 | `faith_core.h:372` | 一手 `th11:0x42364a` |
| 擦弹入槽 | E +8.00 / N·H·L·EX +5.00 | 显示点/颗 | `faith_core.h:398-402` | 一手 `th11:0x4a7950` |
| 线下每帧 | −0.30（−300，下限 0） | 显示点/帧 | `faith_core.h:390` | 一手 `th11:0x420a85` |
| 绿点档值 | 大 +100 / 中 +10 | 显示点/颗 | `faith_core.h:260-261` | 一手 th11 item type 3 / type 9 |
| 灵击无敌 | 270（普通）/ 200（决死） | 帧 | `bomb10.h:138-139` | 一手 `th10:0x4259be` / `th10:0x425c2d` |

**跨卡协同**：
- 110（TH10 信仰）：共用 `faith.c` 实体池 / `faith_core` / 组合 UI `faith_hud`（188×60）/ `bomb10` 唯一一份灵击状态；同装时 111 一律让位（撒点 `signal.c:694`、P 点火 `signal.c:804`、灵击推进 `signal.c:3492`）。
- 112（压缩包）：吃下时 `ufo_bounty` 广播等价计数，本卡按自己槽折算入账，不与实体重复——`signal.c:3290-3305`。
- 108（妖妖梦低速子机声明层）：108 < 111 ⇒ 同装时 108 落笔，111 只收子机不写偏移——`signal.c:65`、`signal.c:2592`。
- 救援类（113/117/原生 23/35）：`+0x0c` 的 acc ≠ 0 ⇒ 本卡不按 miss 记账——`signal.c:3237`；106（遮）只改 110 那条，本卡读 `ce_poc_height_semantic()`——`signal.c:3372`。

**已知未测（代码里没有一手/没有实跑证据的点）**：
- 得分奖残的阈值表只取到第 0 项（10^9）⇒ TH18 分数上限低于它，`ext` 现状打不到——`signal.c:3255-3257`、TUNING 111-1。
- 命碎段机边角：相邻相位指纹逐位相同认不出、间隔 <5 帧的第二边沿被吞、道中妖精波不计段——TUNING 111-10/11/12。
- 过场 `killAll` 清场时，场上宿主妖精会多掉一颗大绿点（该钩子不带 `kind`）——`signal.c:669-674`。
- 原版「每颗小绿点另给 +10 分」未做；对白中 / 一局前 90 帧不推进槽的两个窗口未实现——`signal.c:491-492`、TUNING 111-5。

---

### 112（东方星莲船）

**一句话**：宿主小怪/Boss 击破掉令牌 → 自机碰到收入左下角三格槽 → 三枚成组即召唤一只 ECL 真敌机 UFO → 它吸走全场 P / 金钱道具与 110/111 信仰点 → 击破后按吸收量开奖（压缩包 + 返还令牌 + 加成道具）；超时飞走无奖。

| 行为 | 何时触发 | 代码站点 | 一手出处 / 理由 | 等级 |
| --- | --- | --- | --- | :-: |
| 宿主击破掉令牌 | `on_enemy_death`（排除 `KILL_ALL`）登记落点，帧末建 VM | `ufo.c:5734`、`ufo.c:5803`、`ufo.c:6245` | 触发点一手 `th12:0x4179cc`（`dropExtra` op407）；「谁掉」= 宿主表（自定） | 复刻 |
| 碰触收令牌入槽 | 每游玩帧按自机半径判距 | `ufo.c:3885`、`ufo.c:3694`、`ufo.c:3684` | 半径读本作 `player+0x4798c`（`th18:0x44624a`）；TH12 是收点矩形 `th12:0x426100` | 一手 |
| 三枚成组 ⇒ 召唤 | 入槽那一刻判成组（三同色 / 三色各一；两同一异丢最旧） | `ufo_core.h:214`、`ufo.c:3715`、`ufo.c:3765` | 一手 KEEP2 `th12:0x422e80`；HP 档表 `th12:0x4b33f0` | 复刻 |
| 吸收道具 | UFO 存活 >59 帧起，每帧 18 px 内吸 type1 / type2 | `ufo.c:6080`、`ufo.c:3542` | 一手 `th12:0x4271c0`、`th12:0x42696d` | 一手 |
| 吸 110/111 信仰点 | 同一拍扫声明式列表 | `ufo.c:3602` | 跨卡耦合；TH12 无对位物（`NEXT.md` §1x.7） | 自定 |
| 击破开奖 / 超时无奖 | 本体死 → 帧末 `ufo_break`；敌机 id 消失（ECL 飞走）→ 不收尾 | `ufo.c:6236`、`ufo.c:5080`、`ufo.c:6269` | 一手 `th12:0x44af00`（跳表 `0x44b590`）、`UFO:238-239` | 一手 |

**成本 / 冷却**：
- 无发动成本、不占键：被动卡，`CE_CARD(112)` 只挂 8 个回调（`ufo.c:6388`），无 `on_activate` / `active_recharge`；`CARDS.md:36` 登记「（无）」。
- 令牌掉落节流（❓自定）：开局 180 帧不掉，两枚间隔 ≥300 帧，建失败 120 帧重试（`ufo.c:974`–`976`）；一手是「表里有就掉」，无冷却。
- 吸收门 / 单例：UFO 存活 >59 帧才能吸（`ufo_core.h:271`）；场上同时只有一只（`ufo_core.c:282`），槽里压着的成组结果等它结束才兑现。

| 数值 | 值 | 单位 | 站点 | 出处 |
| --- | --- | --- | --- | --- |
| 凑组槽位 | 3 | 格 | `ufo_core.h:137` | 一手：三同色 / 三色各一 |
| 令牌同时上限 | 6 | 枚 | `ufo_core.h:759` | ❓自定（一手道具池 16 格） |
| 令牌 TTL | 2400 | 帧 | `ufo_core.h:764` | ❓自定（一手 1200 帧只是停止反弹） |
| UFO 存活上限 | 720 | 帧 | `ufo_core.h:266` | 一手 `th12:0x44a8a0`（`zUfo+0x28`） |
| 首吸帧 | 60 | 帧 | `ufo_core.h:271` | 一手 `th12:0x4271c0` |
| 真血量（五档） | 1600–2500 | HP | `ufo_core.h:707` | 一手 `th12:0x4b33f0`（按火力档） |

**跨卡协同**：
- 110/111 信仰点 + 107 樱点（`faith.h` 共享点池通道）：`ufo_absorb_faith_tick`（`ufo.c:3602`）吸走；开包按 `mask` 经 `ufo_bounty_eq_*` 折算回各自账（`ufo.c:4700` 一带）。
- 杰拉 / 复制系统：`on_tick_2` 的 `ce_dup_claim` / `ce_dup_publish`（`ufo.c:6228`–`6229`）认领满 B 原生道具，并复制令牌（复制品水平反向）。
- 92 清扫系统：`ce_sweep_should_run`（`ufo.c:6232`）⇒ `ufo_sweep_to_money`（`ufo.c:1509`）把场上每只 token 变 10 枚金钱。
- 110/111 妖精护栏：`ce_fairy_group_claimed`（`ufo.c:2824`）每帧只读，被认领的宿主让开（不换皮、不掉令牌）。

**已知未测（代码里没有一手/没有实跑证据的点）**：
- 金在凑组里算不算独立一色：`UFO_GOLD_COUNTS_AS_BLUE`（`ufo_core.h:56`，默认 1 = 金≡蓝）；一手颜色域无金。
- 令牌「谁掉 / 什么颜色」无 TH18 一手：宿主表与轮转色是 ❓ 自定占位（`ufo.c` 尾 #10 / #24）。
- 五边形框 / 令牌的层号（12 / 13）TH12↔TH18 层轴是否逐档等价未实测（`ufo.c:613`、`ufo.c:429`）。
- 击破得分权重 `UFO_BOUNTY_SCORE_PER_*` = 0（`ufo_core.h:563`）⇒ 击破本身加 0 分；一手公式已查到但 TH18 无承载 type。

---

### 113（东方神灵庙）

**一句话**：收神灵充灵界槽；槽满按 C 开灵界（强化机体/主炮）；中弹时取消这次死亡先进灵界，窗口结束再补死。

| 行为 | 何时触发 | 代码站点 | 一手出处 / 理由 | 等级 |
| --- | --- | --- | --- | :-: |
| 神灵实体：帧末撒出，有漂移/被吸、480 帧内缩小消失；走到 ≤24 px 才吃掉 | 击破/boss 相位批次后帧末生成；自机靠近 | `trance.c:1697` spawn、`trance.c:2016` tick、`trance.c:3807` 收集 | 一手 `th13:0x438730`（state 机）、`th13:0x438b3d`（+2）；半径 24 px ❓自定 | 一手 |
| 手动开灵界 | D 切本卡 + C；槽满 600 且 8 条条件全过，否则 `ce_refuse` | `trance.c:2401` `on_activate` | 一手 `th13:0x405815`（满槽）、`th13:0x405859`（第 8 条） | 一手 |
| 中弹救援：取消死亡 + 窗口结束时补死（延时，不是免死） | 卡虚表 `+0x0c`；`acc==0`、无原生救援、槽 ≥ 200 | `trance.c:2583`；cancel `trance.c:2511` | 一手 `th18:0x45c114`（广播）/`0x45d590`（取消）；补死 `th13:0x405c3e` | 一手 |
| 灵界窗口：每帧 −1；到 0/超时结束；手动结束 −200，救援档结束报 `re_die` | 灵界相位中每帧 | `trance.c:2991` → `trance_core.c:240` tick、`:210` end | 一手 `th13:0x43d4f0`（−1/帧）、`0x405cdd`（−200）、`0x405c3e`（补死） | 一手 |
| 掉灵三条通道：击破按敌人 T/N 衰减（快杀多掉）+ boss 相位批次 + boss 伤害阈值 | 敌人死亡 / `on_enemy_drop_pre` / `on_enemy_damage` | `trance.c:4655`、`trance.c:4733`、`trance.c:4851` | 一手 `th13:0x419f41`、`0x4391a0`、`0x419e3a`；HP 分档兜底与 20 帧冷却 ❓自定 | 一手 |
| 灵界「中弹无伤」：判定照跑、弹被吃掉，但不扣命/不记 miss/不出特效 | 灵界相位每帧声明闸门 | `trance.c:3140` `ce_no_hit_declare` | 一手 `th13:0x444ca6`；闸门站点 `th18:0x45d3a0` | 一手 |

**成本 / 冷却**：
- 手动发动：槽必须满 600（3 格）；另有 30 帧激活冷却（`trance_core.h:103`，一手 `th13:0x405842`）。
- 灵界中：槽 −1/帧（满槽 = 600 帧 = 10.00 s）；手动结束再 −200（夹到 0）。
- 中弹救援：硬门 = 槽 ≥ 200；救援档结束不扣这 200，改为补一次死亡（延时代价）。

| 数值 | 值 | 单位 | 站点 | 出处 |
| --- | --- | --- | --- | --- |
| 灵界槽上限 | 600 | 点 | `trance_core.h:95` | 一手 `th13:0x405815` |
| 每颗神灵入槽 | +2（type3 再 +1） | 点 | `trance_core.h:98-99` | 一手 `th13:0x438b3d` / `0x438c3d` |
| 灵界中扣槽 | 1 | 点/帧 | `trance_core.h:100` | 一手 `th13:0x43d4f0` |
| 手动结束再扣 | 200 | 点 | `trance_core.h:101` | 一手 `th13:0x405cdd` |
| 进灵界硬门 | 200 | 点 | `trance_core.h:102` | 一手 `th13:0x405a40` |
| 击破兜底档 | HP<1000→(T60,N1)；HP≥1000→(T120,N4) | 帧/颗 | `trance_core.h:641-645` | ❓自定（档位取 TH13 普查） |

**跨卡协同**：
- 救援层 107 → 117 → 决死 B → 113：113 落 `+0x0c`（`trance.c:2583`），先查 `acc`（`:2593`）与 `ce_rescue_note_owner()`（`:2620`）让位。
- 原生救援卡 23（Eirin）/ 35（ShikiEiki）：`trance_native_rescue_would_fire()`（`trance.c:2555`）让位；35 不看 `acc`，抢先救会白扣 200 金。
- 111 河童罩子：补死用 `ce_forced_death_begin/end` 通报「卡自己造的死」（`trance.c:2535`），罩子让位。
- 共享系统：117 主炮（`trance.c:4955`，`CE_SHT_PRIO_113`=10 < 117=20）、108（`trance.c:2998` 声明 `CE_CARD_FLAG_TRANCE`）、110/111 计分层与 PIV 池（`:2950`、`:3836`；113 已停发 `ce_score_declare`，见 `:598`）。

**已知未测（代码里没有一手/没有实跑证据的点）**：
- 「从帧点（`on_tick_2`）调 `Player__die` 无额外时序副作用」= 代码自评 🟡 推断，依据是同族调用 + 卡回调先例（`trance.c:236`）。
- 通道 E 头注（`trance.c:171` 写「仍未接线」）与实现（`:3775` 置 `death_echo`、帧末消费）矛盾，哪一版为准需实跑对账。
- st02 终符用 `0x11` 卡道具，与普通相位同型，只能靠「`+60` 帧 > 符卡门 3 帧」挡（`trance.c:396-397`），🟡 残留。
- 计分载体 SPEC §9 #5 未拍：type1 最大得点只进卡内账 + 共享 PIV，不写计分层（`trance.c:3821`、`:3931`）。

---

### 114（东方辉针城）

**一句话**：TH14 式收点批 —— 把「真空飞行收点」的 P / 钱（外加 110/111 的自建绿点）攒成一批，批闭时按批大小走档位倍率给分数补差、每批弹 1 枚碎片卡；另有一座被动：非聚焦命中敌人掉 P。

| 行为 | 何时触发 | 代码站点 | 一手出处 / 理由 | 等级 |
| --- | --- | --- | --- | :-: |
| 开批 / 计数 | 收集 type∈{1,2} 且道具 state==3（V 门真空飞行） | collect2x.c:449-453 | TH18 分派 0x44625f + V 门 0x445f63（注释内） | 一手 |
| 原生在飞续批 | 批开期每游玩帧池内有 state==3 的批集道具 | collect2x.c:284-296 | TH14 每帧重载 timer 0x8：th14:0x43898e | 一手 |
| 自建绿点入批 / 续批 | 绿点被「真空飞行中收」、或在飞 | collect2x.c:256-267 / 285-296 | 生产者 faith.c；引擎无对应物 ⇒ 用户 B1 口径 | 自定 |
| 批闭 → 结算 | 无在飞且无新收后 8 游玩帧 | collect2x.c:300；core.h:112 | th14:0x4f5884=8 / 0x439d60 归零即结 | 一手 |
| 档位倍率补差 + 碎片 | 批闭时按 count 20/30/40/50/60 | collect2x.c:340-360；core.h:36-48 | 档位/倍率照 TH14 计划真值（站点无地址）；碎片载体一手 0x4b4020 | 复刻 |
| 非聚焦命中掉 P | 高速射击命中（聚焦/bomb 被断点挡掉） | collect2x.c:480-487 | 断点 0x45f2a2；落点＝该敌人中心 0x45f0f0 | 一手 |

**成本 / 冷却**：
- 被动卡（cards.js 无 `category` / `active_recharge`）：无发动成本、无充能，唯一金钱开销 = 商店价 300。
- 「冷却」= 批闭 idle 门：无在飞、无新收满 8 游玩帧才结算；批内不定时、不按次数。
- 二被动节流：命中掉 P 间隔 96 − 火力×16/100 帧（4.00 → 32 帧）；碎片弹出另延迟 20 帧（自定节奏）。

| 数值 | 值 | 单位 | 站点 | 出处 |
| --- | --- | --- | --- | --- |
| 售价档 | 10（=300 金钱） | 档 | patch/th18/cards.js:493 | 一手 engine/card/th18/08-catalog.md:6 |
| 批闭 idle | 8 | 游玩帧 | collect2x_core.h:112 | 一手 th14:0x4f5884 / 0x439d60 |
| 掉落间隔 | 96 − 火力×16/100（钳 ≥1） | 帧 | collect2x_core.h:114-116 | ❓自定（2026-09-08 设计变更） |
| 碎片弹出延迟 | 20 | 游玩帧 | collect2x.c:195 | ❓自定（TUNING §2.9b #4） |
| 档位 → 倍率 | 20/30/40/50/60 → ×0.5/0.8/1.2/1.6/2.0 | 批数 → 倍率 | collect2x_core.h:36-46 | 复刻（TH14 计划真值） |

**跨卡协同**：
- 共享 PIV 池（sdk_core.h）：每帧 `ce_piv_claim_auto` 声明在场，满 P 收 item type 8 ⇒ `ce_piv_add(114,100)` —— collect2x.c:234、425-439；与 112/113/115 同一池，闸由层管。
- 110/111/107 共用绿点 VM（faith.c / faith_core.h）：114 只读「虚拟收集 / 虚拟在飞」声明层 —— collect2x.c:256、285；生产者 = faith.c 的 `dot_collect()`。
- 碎片走原生卡道具 0x10/0x11 → CardLifeFragment(3) / CardBombFragment(4) 库存链 —— collect2x.c:342-348；与 117 的碎片卡叠加会真的多发卡（TUNING.md:2146）。
- 倍率弹字共用 PopupManager 倍率槽 k13–17 —— collect2x.c:165-191；与 107 的数字槽各占各槽，不重叠（TUNING.md:3120）。

**已知未测（代码里没有一手/没有实跑证据的点）**：
- 自建绿点的批位写入受 SDK 声明层 2 帧容差影响，可能比绿点被收晚 ≤2 帧 —— collect2x.c:259-264（自定，无实跑）。
- 绿点那一档 PIV（TH14 +2/+20）够不着 ⇒ 本轮没接 —— collect2x.c:385-394。
- 早结敞口：分值提交滞后到批闭之后的道具会漏出本批 —— collect2x.c:74-75、469-471（文件自述待实跑观察）。
- 2026-09-21 改动（碎片生成点＝批位快照 −48、初速 2.2f）状态 = 已改·待实跑（SINGLE-CARD-TEST.md §th14 #1）。

---

### 115（东方绀珠传）

**一句话**：把 TH15 的章节系统搬到 TH18——擦弹判定区里有敌弹时该弹染黄、自机周围起擦弹圈、场上掉落物减速 ×0.3；boss 战按「一段攻击 = 一波」切波，波末按擦弹数 × 击破率结算章节奖，左侧一块 TH15 风格面板显示实时读数。

| 行为 | 何时触发 | 代码站点 | 一手出处 / 理由 | 等级 |
| --- | --- | --- | --- | :-: |
| 判定区（TH15 式圆/矩形两模式）内有敌弹 → 本帧把掉落物减速因子写成 **0.3** | 每帧 tick；擦弹帧或框内有弹 | `cards/lolk.c:326-337`（写点 `:336`）、判定 `:460-517`、调用 `:955-957` | TH15 一手 `th15:0x4cff28`/`th15:0x4cffb8`；TH18 `+0xe6bb14` 无人写，引擎 `th18:0x446807` 每帧 +0.1 回 1.0 | 一手 |
| 同帧框内有弹 → 该弹 VM **只改 RGB** 染黄（`0xffffd080`）并在自机处起擦弹圈（anm script 106） | 每帧；进圈起、出圈 `interruptLabel(1)` 渐隐 | `cards/lolk.c:451-458`、`:526-541`、`:513` | 染色一手 `th15:0x419d3d`；圈 = TH15 `effect.anm` script27 的 1:1 复刻（`assets/ability/scripts/` 的 106） | 一手 |
| boss 波切分：boss 出现 / 相位指纹变 → 开新波；boss 消失 → 结算 | 每帧 | `cards/lolk_core.c:177-289`、相位读 `cards/lolk.c:292-322` | TH18 一手：`SPELLCARD+0x78`、中断槽 `zEnemy+0x6374`、超时位 `0x42f3a0`（见 `engine/chapter/th18/02`） | 一手 |
| 道中波切分：一章 = 关卡脚本 `MainSubNN` 编号（`01` 与 `01b` 同章），编号变 + 本帧有刷出 → 新波 | 刷出帧 | `cards/lolk_core.c:84-96`、`cards/lolk.c:203-217`、`:224-252` | TH15 「一章 = 一个 MainSub 组」有 `setChapter` 一手；TH18 道中无 ECL 524 ⇒ 拿编号当键、**去掉尾部字母是用户 2026-09-13 口径**，非原版规则 | 复刻 |
| 波末：`bonus = trunc10(擦弹 × 击破率％ × 50)`，`score += bonus/10`，`PIV += (bonus/50000)×10`，`bonus ≥ 1e6` 发命碎 | 波关闭（`close_wave`） | `cards/lolk_core.c:122-157`、`:30-38`、`cards/lolk.c:616-670` | TH15 一手 `0x43ab70`/`0x43ac94`/`0x43ad19`/`0x43ae42`/`0x4cffc0`；擦弹 = `0x4ccd0c` 差分（`cards/lolk.c:38`） | 一手 |
| TH15 波次面板：六条贴图条（碎片条二选一）+ 五行数值（擦弹/击破率/击破数/章节奖/PIV 增量） | `on_draw`，结算后 300 帧内 | `cards/lolk.c:696-726`、`:1011-1046`、`:1048-1082` | 几何/字体/右对齐/淡出 = TH15 `Gui::on_draw_2e` `th15:0x436a6f` + script241（一手）；玩家贴近压暗不属原版，见下「跨卡协同」 | 一手 |

**成本 / 冷却**：
- 被动卡：无发动费、无冷却、不占 C 键——`CE_CARD(115)` 只注册 tick/draw/换关/局末/删卡/收道具回调（`cards/lolk.c:1132-1138`）。
- 面板结算后保留 300 帧（≈5 s）后自动关；跳动从第 90 帧起、每 4 帧一声 SE，退场五行由 C 淡 20 帧、六条由脚本淡 10 帧。
- 减速因子每帧被本卡写 0.3，出框后交引擎 +0.1/帧 回 1.0（≈7 帧 ≈0.12 s），本卡不参与恢复。

| 数值 | 值 | 单位 | 站点 | 出处 |
| --- | --- | --- | --- | --- |
| price_tier | 10 | 档 | `patch/th18/cards.js:509` | 卡表 `LOLK_CHAPTER`（占位，见 `CARDS.md` §注册） |
| weight | 2 | — | `patch/th18/cards.js:510` | 同上（`initial_unlocked: 1`，sprite 180/181） |
| 掉落物减速因子 | 0.3 写 / +0.1 每帧恢复 | float | `cards/lolk.c:336`；`engine/player/th18/02-graze-and-item-slowdown.md:80` | TH15 一手 0.3（`0x3e99999a`）与恢复 `0x4cfdf0` |
| 面板保留 | 300 | 帧 | `cards/lolk.c:607` | TH15 原生 `gui+0x1b4` = 290 帧（`th15:0x435561`）；差 10 帧已知未改，`TUNING.md` §7.5 |
| 数值跳动起始 | 90 | 帧 | `cards/lolk.c:604` | TH15 `gui+0x1a4 >= 0x5a`；每 4 帧 `se_kira01`(`0x27`)、收尾 `se_bonus`(`0x2f`) |
| 五行退场淡出 | 20 | 帧线性 | `cards/lolk.c:805` | TH15 面板父 VM 的 `alphaTime(20, 0, 0)`（`engine/chapter/th15/02:25`） |

**跨卡协同**：
- 共享 PIV 池：`ce_piv_claim_auto` / `ce_piv_add`（`cards/lolk.c:913`、`:647-650`、`:388`）——与 110/114 等写同一池，上限夹在 `sdk_core.h`。
- 擦弹族声明 `ce_graze_family_claim`（`cards/lolk.c:914`）——与 106/107/110/111/116 同族；113 缩判定点会连带缩小本卡自判框（`cards/lolk.c:474-481`，`CARD-NUMERIC-INTERACTION-AUDIT.md:590`）。
- 共享站点 `ce_item_collect`（`th18:0x44625f`）：`cards/lolk.c:1138` 订阅它，与 110/114 同站点，不新增断点、不改字节。
- 共享压暗件 `hud_dim_tick`（`cards/lolk.c:859`）：与 116/117/107/113/110+111 同一档（终点 64 / 8 帧线性）；那块矩形按本卡面板几何每帧现算（`:824-840`）。触发行为本身 = ❓ 用户口径，不是复刻。

**已知未测（代码里没有一手/没有实跑证据的点）**：
- 击破率分母用 `enemy_count_real` 差分近似（`cards/lolk_core.h:57-62`、`cards/lolk.c:640-644`）：TH18 没有 TH15 的章内 SPAWNED/DESTROYED 计数，非一手、无实跑校准。
- 碎片口径自相矛盾：`cards/lolk_core.h:70` 的 `LOLK_FRAG_CYCLE=5` 注释仍写「每 5 波第 5 波命碎」，实现恒发 `LOLK_FRAG_LIFE`（`cards/lolk_core.c:145-153`），`LOLK_FRAG_BOMB` 全仓未被使用。
- 「道具变黄」那半没接：TH15 的黄来自擦弹道具 type 13（橄榄黄方块），TH18 无该生成链 ⇒ 待 owner 拍（`SINGLE-CARD-TEST.md` §th15-P9b）。
- 面板几何/淡出（终点 64、8 帧、五行 20 帧）与擦弹圈半径取 TH15「变大」态系数 3.6（`cards/lolk.c:66-68`、`:476-481`）都标「已改·待实跑」，无实跑验收记录。

---

### 116（东方天空璋）

**一句话**：TH16 季节系统复刻——擦弹/击破/解放清弹产出「季节点」，累计成 0..6 档，档 = 季节子机数，按 C 发动季节解放（清弹 + 伤害 + 无敌），双击 Z 切副季节，左下角画季节槽。

| 行为 | 何时触发 | 代码站点 | 一手出处 / 理由 | 等级 |
| --- | --- | --- | --- | :-: |
| 季节点掉落（擦弹 / 击破 / 消弹）| 擦到弹、敌人被击破、解放清弹 | season.c:687, 1058, 728 | 擦弹 `th16:0x444e03`、击破 `th16:0x420585`、消弹 `th16:0x416bbb`；击破数量映射 ❓自定 | 一手 |
| 季节点运动 / 磁吸 / 收集 / 升档弹字 | 每帧 | season.c:805-934, 767 | TH16 `th16:0x42f4e0` 逐条；弹字借原生 `th18:0x4645f0` | 一手 |
| 档位 → 季节子机（阵位/贴图/脚本）| 收点跨阈 / 换副季节 | season.c:550, 1597; season_core.c:5 | 阈值 `th16:0x43deb0`；脚本表 `th16:0x492be0`、阵位读 `pl0Xsub.sht` | 一手 |
| 季节解放（主动卡 C 键）| `on_activate`（SDK 充能就绪）| season.c:1291, 1359, 707 | 释放表 `th16:0x491e70..0x491fd4`；消弹点 `th16:0x416bbb` | 一手 |
| 副季节切换（完整双击 Z）| 不按 Z→按→放开→再按，8 帧内 | season.c:1530; season_core.c:205 | TH16 无切季键 ⇒ 用户口径；TUNING.md:49 仍写 Shift+D（过期）| 自定 |
| HUD 左下角季节槽（框/进度/字/数字）| `on_draw` 每帧 | season.c:1813, 1703, 177 | TH16 `front.anm` script113–118、`th16:0x42c600` 配方；pin 68/924 = owner 口径 | 复刻 |

**成本 / 冷却**：
- 发动门：档 0 或正在解放 ⇒ `ce_refuse`；`active_recharge = 1` 不是真冷却，只是 SDK 主动卡入场券（season.c:1295-1296, 2078）。
- 消耗：春/秋/冬 季节点清零，夏/土用 只扣一档（season_core.c:77；season.c:1345）。
- 释放期间：秋 自机移速 ×1.5、冬 自机弹伤 ×1.5、按档写无敌（season.c:1399, 1340, 1334）。

| 数值 | 值 | 单位 | 站点 | 出处 |
| --- | --- | --- | --- | --- |
| 档位阈值 | 100/230/390/590/840/1140 | 季节点 | season_core.c:5 | `th16:0x43deb0` 一手 |
| 季节点池上限 | 512 | 个 | season.c:117 | ❓自定（TH16 道具池 4696 槽）|
| 擦弹点初速 | 1.9 | px/帧 | season.c:687 | `th16:0x444e03` 一手 |
| 消弹点 角度 / 速度 | −π/2±10° / 2.2 | rad / px每帧 | season.c:727-728 | `th16:0x416bbb` 一手 |
| 双击窗口 | 8 | 帧 | season_core.h:100 | ❓自定（照 TH11 `th11:0x430e50`）|
| HUD 块 | 134×36 @ (68,924) | 表面 px | season.c:177, 180, 212 | ❓自定（owner 口径）|

**跨卡协同**：
- 92「伟大之作」清扫：`ce_sweep_should_run` → 每颗季节点换 1 枚金钱（season.c:1510, 604）。
- 88「杰拉」：`ce_dup_publish` / `ce_dup_should_run` 把场上点数复制一代（season.c:1501-1505）。
- 原生卡 21/22（ITEM_CATCH / ITEM_LINE）：点数磁吸读自机 `+0x47988..0x47994` 字段（season.c:831-838）。
- 共享 HUD：`ce_hud_slot_*` 占位 + `hud_dim_*` 压暗（season.c:1652, 1692, 1841）；109 灵力道具借本卡模型（TUNING.md:4316）。

**已知未测（代码里没有一手/没有实跑证据的点）**：
- 无敌帧非一手：夏9/秋30/冬30/土用10 是攻略口径，春 = duration+60 且「每帧刷 50」未复刻（season_core.c:32；AUDIT-116 §7-3）。
- 升档弹字的字形 = sprite299 (80,248,48,8)，未肉眼确认（AUDIT-116 §7-1）。
- `cancel_spawn_points` 命中判据按 engine.h 对 `th18:0x429370` 的描述镜像，未反汇编复核（season.c:719-725）。
- TH16 的 45 帧释放冷却未复刻；夏 `type(3)` / 冬 `type(2)` / 秋 `type(0)` 在 TH18 的渲染等价未实跑（AUDIT-116 §7-4/§7-5）。

---

### 117（东方鬼形兽）

**一句话**：TH17 动物灵/咆哮的 TH18 卡侧复刻——敌人掉兽灵，收满 5 格自动起咆哮（就绪窗口），按 X 用成 40 帧霊撃，中弹时可用咆哮抵命。

| 行为 | 何时触发 | 代码站点 | 一手出处 / 理由 | 等级 |
| --- | --- | --- | --- | :-: |
| 道中敌死掉动物灵 | 非 boss 敌机死亡且过判据（贴图≥64×64 或同波同类≤3） | animal.c:3671；判据 animal.c:1398 | 一手 TH17 zGoastManager 掉落链；判据为用户 2026-09-13 口径（TUNING §2.2） | 复刻 |
| Boss / 相位掉灵 | `dropItems` 事件或相位边界 | animal.c:3775、3483、3584 | 一手 TH17 `BossItem` 表（`dropMain`/`dropExtra`） | 复刻 |
| 收 5 格 → 满槽自动就绪 | 24 px 内收集、第 5 颗入队 | animal.c:3353、3113；animal_core.c:23 | 一手 `th17:0x40fb60`（数 1/2/3，≥3 暴走） | 一手 |
| 咆哮效果：狼/鹫换主炮组、獭 3 光球 | 就绪或霊撃期每帧 | animal.c:2494、3057、2675 | 一手 TH17 火力档 +10/+15（`th17:0x44a710`）、`0x40f200` | 复刻 |
| X = 用掉咆哮（40 帧霊撃） | `ready` 且输入掩码 bit1 上升沿 | animal.c:3983、3195 | 一手 `th17:0x40f6d0` / `0x411d40` | 一手 |
| 中弹抵命 / 真死掉 2 灵 | `on_death_before_deathbomb` / `on_death_frame2` | animal.c:3828、3850 | 一手爆散基座 `th17:0x40e920`；类型随机为口径 | 复刻 |

**成本 / 冷却**：
- 无 C 键、无充能：`.active_recharge`/`.on_activate` 已删（animal.c:4216–4223）；`price_tier` 10 / `weight` 2（cards.js:544）。
- 就绪窗口有时限：满槽起 840/1140/1500 帧；就绪期每帧 −1、有存活弹再 −1（animal.c:4076）。
- 霊撃固定 40 帧（animal_core.h:36）；结束即清队列并回灌 pending，无重用冷却（animal_core.c:142）。

| 数值 | 值 | 单位 | 站点 | 出处 |
| --- | --- | --- | --- | --- |
| 槽上限 | 5 | 格 | animal_core.h:24 | 一手 TH17 `TOKENS[5]` |
| 就绪时长（同类 3/4/5） | 840/1140/1500 | 帧 | animal_core.c:16 | 一手表 `0x4a13cc` |
| 凭依加成 | 480 | 帧 | animal_core.h:37 | 一手 `th17:0x40fbe5`（本卡恒 NONE，不吃） |
| 咆哮中收灵延长 | 90/60/42… | 帧 | animal_core.c:99 | 用户口径（TH17 为 120/80/53，🟡 两说） |
| 霊撃本体 | 40 | 帧 | animal_core.h:36 | 一手 `th17:0x40f6d0` |
| 霊撃半径 | (1−f⁴)×200 | px | animal_core.c:169 | 一手 `th17:0x40f61a` |

**跨卡协同**：
- 消弹闩锁：霊撃半径圈先 `animal_cancel_claim`（animal.c:3208），同帧 107/111 已消则让位（EVENTS-CONTRACT §5.2）。
- 救援层：`on_death_before_deathbomb` 无条件 `ce_rescue_note_claim(117)`（animal.c:3844），与 107/113 并存破。
- 共享件：HUD 压暗走 `hud_dim.{h,c}`（animal.c:64/2103）、HUD 槽位表 `ce_hud_slot_claim_pinned_card`（animal.c:2014）。
- 跨卡闸：`on_option_shot_gate`（animal.c:2549）拦咆哮期装备子机组；`CE_CARD_FLAG_ROAR`（animal.c:3969）与 108 共享；Jera 复制（animal.c:3953）、92 清扫（animal.c:3955）。

**已知未测**：
- HUD 排位 (88,924) 与可见整排 (64,928) 差 24 px，代码内两说、未拍板（animal.c:141–179，AUDIT §DD-8）。
- 稀有动物灵 8..14 只打一行日志、不发（`beast2*` 资产缺，animal.c:3563 / TUNING §2.3）。
- 咲夜 / 早苗的兽灵咆哮全为 ❓ 自定（TH17 无对应行，animal.c:360–399 / TUNING §2.1b）。
- 调试键 `ANIMAL_DEBUG_KEY=0`（animal.c:242）：空格刷灵 / 强制就绪路径无发布版实跑证据。

---
