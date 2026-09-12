# MAP —— 为了加一张新卡，我们到底改了游戏的哪些东西

> **版本**：TH18 v1.00a（`th18.exe`，imagebase `0x400000`）。本文裸地址默认属该版本；引用其他版本须写成 `th16:0x…`。
> 这是**追溯表**：`patch/th18.v1.00a.js` 里 201 条 binhack、19 个断点、10 个 codecave，加上 DLL 在运行时写的
> 立即数，每一条都能从这里找到「为什么改、谁生成的、在哪审计的」。改动只增不减，**每加一块就在这里补一行**。

## 0. 一张卡要经过的路，和每一段卡在哪

游戏里「一张卡」不是一个对象，而是散在十来个地方的 57 项定长数组 / 立即数。零售上限 57（id 0–56，
56 = `NULL` 空槽，57 = `BACK` 卡背），我们把它抬到 255。按一张卡从出生到出现在玩家面前的顺序：

| # | 一张卡需要… | 游戏里是什么 | 零售上限 | 我们怎么改 | 状态 |
| --- | --- | --- | --- | --- | --- |
| 1 | 一行注册数据（名字指针、id、类别、价格、权重、sprite…） | `zTableCardData[]` `0x4c53c0`，stride `0x34` | 58 行，被两个热全局堵死 | 整表搬进 codecave，100 处内联查表改指向 | ✅ |
| 2 | 能被 `allocate_new_card(id)` 造出来 | 跳转表 `0x412dac` 57 项 + `cmp ebx,0x38` | 57 | 跳转表搬迁（新 id → case 56 的函数体），上界 → 254 | ✅ |
| 3 | 本局「已拥有」记录 | `zAbilityManager.owned[]` `+0xc84` int[56] | 56，对象止于 `0xd70` | 对象扩到 `0x116c`，数组搬到 `+0xd70` | ✅ |
| 4 | 解锁状态可读、可写、可存档 | `zScoreFile.unlocked_cards` `+0x5f588` uint8[57] | 57，在存档里 | 影子数组 codecave + side-car 文件；零售 id 仍写存档 | ✅ |
| 5 | 名字与说明 | `zAbilityText` `0x63e0` = 57 × `0x1c0` | 57，对象尾部就是别的字段 | 不扩对象，3 处读按 id 重定向到 DLL 缓冲 | ✅ |
| 6 | 在图鉴 / 卡组编成里出现 | 显示顺序表 `0x4b3600` 57 项；`zAbilityMenu.__card_ids[56]`；条目数 `0x38` ×7 | 57 / 56 / 56 | 顺序表搬迁重排（新 id 按类别插进零售同类别区段末尾，`ce_build_order`）；对象扩容；条目数由 DLL 现写 56+N | ✅ |
| 7 | 在商店里出现 | `AbilityShop` 三处循环只看前 56 个 id；随机池 560 份、offer 57 槽 | 56 | 上界 → rows；cave 里 NULL/BACK 行 `+0x14 := 6` 排除幻影；容量装载时现算 | 🔧 待实跑 |
| 8 | 卡图 | `abcard.anm` 的 sprite，行里 `+0x2c/+0x30` | 零售 118 个 sprite（= entry 号） | [`assets/`](assets/README.md)：两张 PNG + ORDER → `make anm` 重编 `abcard.anm`（118 起追加）→ 随 `_255` 整文件替换；JSON 填索引；DLL 不碰 | 🔧（工具就位，黑桃五张 118–127 待实跑）|
| 9 | 行为 | 跳转表指向的构造器 + 虚表 | — | 跳转表不动：断点 `ce_card_bind`（`0x412cec`）把登记了行为的 id 的对象虚表换成 DLL 里的拷贝，槽是 C 写的 `thiscall` 桩（[`SDK.md`](SDK.md)）| 🔧 待实跑 |
| 10 | 数据从哪来 | — | — | thcrap 栈里每个 patch 的 `th18/cards.js` 深合并 → 门里逐张校验、写 cave 行 + 文案 + 注册表（[`DATA.md`](DATA.md)）| 🔧 待实跑 |
| 11 | 一句语音 | 音效表：cfg `0x4c9b80` 84 行 / wav 名 `0x4b47a0` 72 项 / slot `0x56c804` 84 × `0x18` / blob `0x56cfe4` 72 项 | 84 个 id（`0x00`–`0x53`），**一个空闲的都没有** | 四张表整体搬进 codecave 加长到 116 行，51 处站点改常量；`_patch_init` 从用户的 exe 拷零售内容 + 写新行骨架；DLL 在 `0x476410` 的门里填语音 blob（[`assets/voice/`](assets/voice/README.md)）| 🔧 待实跑 |
| 12 | 一组**子机弹幕**（装备卡连射）| `pl0X.sht` 的 `+0xe0` shooterset 偏移数组 | 40 项，零售用 23 项 | 占尾部空位（第 `0x17` 起，17 个）：整文件替换四个 `.sht`，纯追加（`assets/sht/append_shooterset.py`）。子机本身走零售 `Player__allocate_option`，贴图与子弹脚本都放 `ability.anm`（[`engine/sht/th18/`](../../../engine/sht/th18/README.md)）| 🛠 工具就位、**当前没有卡在用**（破损核心第一版用过，改成定点伤害源了；AUDIT U16）|

第 1–6 段跑通的标志（✅ 2026-09-02）：用 `_test` 把 id 58 塞进空槽 → 开局分配到它 → 「获得」→ 重启仍解锁 → 图鉴里有它 → 编成里能选它。
第 7 + 10 段的标志：`patch/th18/cards.js` 的黑桃五张在图鉴 / 编成 / 商店里出现、能买；名字 / 说明是 JSON 里的。

## 1. 机制只有四种

| 机制 | 什么时候用 | 特征 | 谁生成 / 谁核对 |
| --- | --- | --- | --- |
| **同长 binhack** | 只换一条指令里的常量（imm32 / disp32 / imm8） | `expected` 与 `code` 等长，opcode 不动（战线 D 例外：改 ModRM 去 SIB，仍等长）| `sites.py` 生成，`make verify` 拿回 exe 对账，DLL 开机回读 |
| **codecave（数据）** | 需要比零售更大的数组 | patch 只声明 size，内容由 `_patch_init` 保险填、DLL 权威填 | `emit_codecaves` / `selfcheck.c` |
| **thcrap 断点** | 需要在某条指令处跑 C 逻辑 | `cavesize` = 整条指令，无相对寻址；返回 1 放行 / 0 跳过 | patch 声明，DLL 导出 `BP_<名>` |
| **DLL 运行时写代码** | 值到运行时才知道（新卡数量）| 门里 `VirtualProtect` 写，写前核对原值 | `menu.c`；`restore_alloc_bound` 同类 |
| **thcrap 栈 JSON** | 数据由写卡的人给、可被别的 patch 叠加 | `stack_game_json_resolve` 深合并全栈的 `th18/cards.js`；DLL 只读、逐张校验、全有或全无 | `cards.c` ← `cards_def.c`（主机单测）|
| **断点换虚表** | 新卡要有行为 | 对象由游戏 `new`，尾段断点把虚表指针换成 DLL 里的 21 槽拷贝；槽 = 编译器生成的 `thiscall` 桩 | `sdk.h` / `sdk.c` ← `sdk_core.c`（主机单测）；AUDIT §O |
| **整文件替换（资源）** | 卡图 / 特效 / 子机弹幕要新数据 | patch 里放一份重建的游戏资源文件，thcrap 在装载时顶掉零售的：`th18/abcard.anm`、`th18/ability.anm`（`th18/pl0[0-3].sht` 的工具也就位，当前没发）。一律「取零售 + 纯追加 + 回读校验原内容逐字节不变」 | `assets/build_abcard.py` / `build_ability.py` / `assets/sht/append_shooterset.py`；AUDIT §U |

**没有一个字节手写机器码**（除两个 `_patch_init` 里的 `rep movsd` 拷表 / 骨架循环，和测试钩子的 5 条指令）。

## 2. patch 里每一条的出处（按 binhack 名前缀）

`patch/th18.v1.00a.js` 是生成物（`make step3`），键名前缀就是追溯的钥匙：

| 前缀 | 条数 | 干什么 | 生成函数（`native/sites.py`） | 死绑登记 | 审计 |
| --- | --- | --- | --- | --- | --- |
| `cardtable_{start,end,fallback,hit}_` | 100 | 内联查表的四条臂 → 新表 | `emit()` ← `shape.py` 骨架匹配 | TARGET「100 处」 | AUDIT §B–§F |
| `alloc_bound_` / `alloc_jumptable_` | 2 | 分配器上界 254；跳转表 → cave | `emit_alloc_binhacks` | TARGET「战线 B」 | §I |
| `grow_` | 12 | `zAbilityManager` 大小 / `owned[]` 搬迁 / 商店循环起止（上界 = rows）| `emit_grow_binhacks` | TARGET「战线 C」 | §J、§N |
| `unlock_` | 9 | `unlocked_cards` 读 → 影子数组（改 ModRM） | `emit_unlock_binhacks` ← `find_unlock_sites` | TARGET「战线 D」 | §K、§K′ |
| `order_` | 7 | 显示顺序表 6 引用 + 尾界 → cave | `emit_order_binhacks` ← `find_order_sites` | TARGET「E 第二块」 | §M、§M′ |
| `menu_` | 20 | `zAbilityMenu` 大小 ×3、`__card_ids` ×16、清理上界 ×1 | `emit_menu_binhacks` ← `MENU_SITES` | 同上 | §M |

第 1 步 patch（`th18_card_expand`，58 行）只有 `cardtable_`；其余全在 `_255`。
该 patch 2026-09-07 退役、不再发布（复现：`make ROWS=58 gen`），现在发出去的只有 `_255`。

### 断点（`breakpoints`，DLL 导出同名 `BP_*`）

| 名 | 地址 | 长 | 干什么 | 源文件 | 审计 |
| --- | --- | --- | --- | --- | --- |
| `ce_gate` | `0x4637d0` `ScoreFile__load` 入口 | 5 | 开机自检门：填表、回读全部站点、跑下面所有 setup | `dll_main.c` / `selfcheck.c` | §H、§H′ |
| `ce_save_loaded` | `0x46398a` | 6 | 影子 ← 零售存档 + side-car | `unlocked.c` | §K10 |
| `ce_unlock_write` | `0x418e04` `mark_obtained` | 8 | 影子[id]=1；id<57 放行，否则写 side-car 并跳过 | `unlocked.c` | §K6–K9 |
| `ce_unlock_all` | `0x4648fe` | 6 | 作弊解锁镜像到影子 | `unlocked.c` | §K12 |
| `ce_text_name` / `_desc` / `_notify` | `0x416694` / `0x416779` / `0x41926a` | 6/7/6 | id≥57 的文案指向 DLL 缓冲 | `text.c` | §L |
| `ce_card_bind` | `0x412cec` 分配公共尾段 | 6 | esi = 卡对象、ebx = id：有行为的换虚表 | `sdk.c` | §O1–O2 |
| `ce_item_score` | `0x446cf6` `collect_money_item` | 6 | esi = 道具身价，沿卡链表调 `on_item_score` | `sdk.c` | §O10 |
| `ce_item_money` | `0x446d28` `collect_money_item` 尾 | 6 | 金钱入账前，沿卡链表调 `on_item_money`，`MONEY`/`MONEY_TOTAL` 一起 += bonus | `sdk.c` | §O14 |
| `ce_enemy_drop` | `0x430510` `Enemy__drop_items_and_notify_cards` 入口 | 6 | 撒附加表之前：翻敌人 `+0x04` 起的掉落数（加倍 ×2） | `sdk.c` ← `double.c` | §S |
| `ce_bomb_spent` | `0x4203bc` `do_bomb` 里 `consume_bomb` 刚返回 | 5 | 炸弹已扣、HUD 已刷：沿卡链表调 `on_bomb_spent` | `sdk.c` ← `corruption.c` | §T |
| `ce_shop_bought` | `0x4183ea` `AbilityShop__on_tick` 成交分支 | 12 | 只记「本次进店成交了」（关卡 + 帧） | `shop.c` | §P1–P2 |
| `ce_shop_reopen` | `0x443b05` `GameThread__on_tick` `test eax,0x20000` | 5 | 店刚关、成交过、还有名额 → `eax |= 0x20000`，零售建店代码再跑一遍（每关 2 次） | `shop.c` ← `shop_core.c` | §P3–P8 |
| `ce_item_frame` | `0x446ee3` `ItemManager__on_tick` `0x446ec0` 的最后一道门卫 CMP | 7 | 每帧、道具池 body 前一次：沿卡链表调 `on_item_frame`（辉针城：判「无 state-3 在飞」→ 关批结算，Z22） | `sdk.c` | §Z |
| `ce_item_collect` | `0x44625f` 收集 type 分派前一条 | 6 | 本道具副作用未发生：调 `on_item_collect`（type = `*(uint32_t*)(item+0xc78)`；辉针城：批集计数 + stash SCORE） | `sdk.c` | §Z |
| `ce_item_commit` | `0x44658a` 共享收尾起点 | 6 | 本道具分值已入 SCORE、公共收尾前：调 `on_item_commit`（辉针城：累加分值增量） | `sdk.c` | §Z |
| `ce_shot_damage` | `0x45f2a2` `enm_compute_damage_sources` 计分路径 | 7 | 每帧每敌一次可计分命中（dmg 钳后 ≥1）：调 `on_shot_damage`（辉针城：非聚焦命中掉 P，Z13） | `sdk.c` | §Z |
| `ce_snd_gate` | `0x476410` `SoundManager::init` 入口 | 5 | 引擎还没读表：扩表装载 + 新行填充、回读验证 | `sound.c`（生成 `sound_emit.py`）| §Q |

### codecave

| 名 | 大小 | 内容 | 谁填 |
| --- | --- | --- | --- |
| `th18_card_table` | 255 × `0x34` | 卡表 | `_patch_init` 保险 + DLL 权威（零售 58 行 + NULL 副本）；装载器再写新卡行（行号 = id）、把 56/57 行 `+0x14` 改 6 |
| `th18_card_jumptable` | 255 × 4 | 分配器跳转表 | 同上（57 原样 + case 56）|
| `th18_card_unlocked` | 256 | 解锁影子 | DLL（`ce_save_loaded`）|
| `th18_card_order` | 255 × 4 | 显示顺序 | `_patch_init` 保险（57 + BACK 填充）+ DLL 重排追加新卡 |
| `th18_card_table_patch_init` | 代码 | 上面三张表的开机拷贝 | thcrap `patch_func_init` 自动调 |

### DLL 运行时写的立即数（**不在 patch 里**，日志 `menu:` 行可见）

| 地址 | 原 | 写成 | 为什么不能进 patch |
| --- | --- | --- | --- |
| `0x4137bb` `0x414394` `0x41439e` `0x41570d` `0x415817`（imm32）、`0x4145e2` `0x4157cb`（imm8）| `0x38` | 56 + 已注册新卡数 | 数量来自注册表；imm8 ⇒ ≤ 127 |
| `0x411479`（兜底） | — | 写回 `0x38` | 任何 FAIL 时把分配器上界还原 |

## 3. 测试钩子（`patch-test/`，只在验证时叠上）

| 项 | 地址 | 干什么 |
| --- | --- | --- |
| 断点 `ce_test_deck` → `BP_ce_test_deck` | `0x407ee3` | 初始卡组的空槽(56) 依次改发 `th18/cards_dev.js` 的 `start_deck`（§O11）|
| 断点 `ce_trace_alloc` → `BP_ce_trace_alloc` | `0x411469` | 记录每次 `allocate_new_card`；新 id 第一次出现时调 `mark_obtained(id,1)` |
| `th18/cards_dev.js` | — | 开发配置（起手卡组、`trace`）。卡池本身在 `_255` 的 `th18/cards.js` |

`_test` 现在是**开发环境**：起手直接拿到要测的卡、每个桩第一次被调记日志。不再有手写 cave。

## 4. 换 build 要重取什么

全部死绑量在 [`TARGET.md`](TARGET.md)。原则：**地址不是抄来的，是扫出来的**——`make check`
在新 exe 上跑，锚点数不是 25、`0x5f588` 的读不是 9、顺序表引用不是 6+1，都会停下来。
人工必须重看的只有两类：SIB 里存档指针放哪一格（AUDIT §K′）、游标相对寻址的位移（§M6）。

## 5. 还没做的段落各自卡在哪

- **行为**：已做（第 9 段，[`SDK.md`](SDK.md)），待实跑。主动卡状态机、C 键、充能与 HUD/replay 计时器透传已经进入 SDK。
- **卡图**：不是代码活。`assets/build_abcard.py` 重编；余量结论见 [`engine/card/th18/10-extensibility-limits.md`](../../../engine/card/th18/10-extensibility-limits.md) §5（运行时上限 🟡）。
- ~~商店~~、~~数据~~：已做（第 7 / 10 段），实跑通过（2026-09-04）。
- **商店走两遍**（每关进店 2 次，2026-09-04）：不碰店内状态机，两个放行断点让零售建店代码再跑一遍；AUDIT §P，待实跑。
- **HUD 图标上叠字**（2026-09-06，真银圣剑）：不改字节。虚表 `+0x44` 拿图标 id，UI 列表建 VM（`0x409670`）+ `set_sprite`（`0x477b00`）；加残机碎片走引擎 `0x457570`。AUDIT §X，待实跑。
- **局中删卡 / 过关结算事件**（2026-09-06，倒吊人 + 黄金小丑）：不加断点、不改字节。`ce_destroy_card` 复刻 recount `0x4080e0` 的删卡分支，HUD 交给引擎的 `0x408d00`；`on_shop_open` 挂在 `ce_shop_reopen` 已有断点的 MSG 开店分支。AUDIT §W，待实跑。
- **终符判定**（2026-09-07，古锭刀）：不改字节、无新断点、无新引擎调用。`on_tick_2` 读 boss 活动中断槽的子程序名（`BossDead` / `MBossDead` = 最后一张符卡，ECL 一手全覆盖），翻自机弹 + 抬 `sht+0x28` 基值 ×2。AUDIT §Y，待实跑。
- **收点系统 / 高速掉 P**（2026-09-08，辉针城 id 114；✅ 实跑通过 2026-09-09，Z22 成员门「有就起效、没有不起效」闭环）：被动卡。四个新 SDK 事件 + 断点（表见上）：`on_item_collect`/`on_item_commit` 两断点夹引擎分值加法器、stash SCORE 差量入批（有效窗口只在批集收集后——批集 = V 门真空飞行 state 3 的 {P,钱}，普通触收/环收不入批，Z22）；`on_item_frame` 里判「无 state-3 在飞」关批、按批量档位给倍率分数补差 + 碎片 spawn（延迟 20 帧从当下自机位弹，Z12）；`on_shot_damage` 按帧节流掉 P。批/档位/碎片循环纯逻辑在 `collect2x_core`（主机单测）；倍率弹字 = 原生 5 槽环（`[0x4cf420]`+`0x9cc`，tick `0x464020`/draw `0x464180`，AUDIT §Z10，Task 8）。AUDIT §Z。
- **SHT shooterset 连射**（2026-09-08，豌豆射手）：不改 exe 字节、无新断点。资源侧**新增一处写入面**——往四个 `pl0X.sht` 的 `+0xe0` 偏移数组的空位（零售用 23/40）追加两组 shooterset（索引 `0x17` / `0x18`）并在文件尾接数据；`assets/sht/append_shooterset.py` 每次回读校验三条不变式（头部 40、set 0 的 func 全 0、前 23 组逐字节不变）。代码侧只调早已复刻好的 `ce_tick_shooters_for_card`（`0x40a9c0` stdcall ret 0x10），**short_timer 自己数**（`param_2` 只进模运算，一手 AUDIT AJ3）。AUDIT §AJ，待实跑。
- **自机弹伤害倍率层**（2026-09-08，衔尾蛇触发的提炼）：不改字节、无新断点、无新引擎入口。`.bullet_damage_permille` + `ce_dmg_*`（`sdk_core.c`，主机单测）把「加伤害」收成一件事：聚合连乘、由 id 最小的参与者在 `+0x28` 里**乘一次**（单次取整）、并在 `+0x2c` 里按同一倍率抬 `player+0x47984`。K♠ 与古锭刀已迁移（K♠ 顺手修了漏抬上限）。AUDIT §AA，待实跑。
- **章节系统 + 擦弹减速**（2026-09-09，绀珠传 id 115）：**零断点、零字节改动**——只读三个字段、写一个 float。读 `GRAZE` `0x4ccd0c`（差分）、符卡位 `SPELLCARD+0x78` bit0（波边界）、`ENEMY_MGR+0x198`（击破率近似）；写 `ITEM_MANAGER+0xe6bb14`（ExpHP `lolk_item_slowdown_factor`，TH15 的残骸：引擎每帧 +0.1 恢复、TH18 没人写它）= 0.3。章节奖公式照 TH15（擦弹 × 击破率 × 50）。AUDIT §AK，待实跑；一手见 `engine/chapter/th15|th18`、`engine/player/th15|th18`。
- **HUD 文字原语**（2026-09-08，倍率读数）：不改字节、无新断点。`ce_hud_text` 照零售 `CardMomoyo__on_draw` `0x4110a0` 复刻——`AsciiManager__drawf_debug` `0x41a110`（cdecl varargs，我们只用固定的 `"%s"`）+ `ASCII_MANAGER_PTR` `0x4cf2ac` + 字色 `+0x19228`（BGRA），只在虚表 `+0x48 on_draw` 里调。第一个用户：伤害层把聚合倍率写在火力值左边。AUDIT §AC，待实跑。
- **鬼形兽（2026-09-10，id 117，Stage 2）**：不改字节、**无新断点 / codecave**——动物灵实体是自己的 `ability.anm` VM 池（`ce_enemy_drop` 的 `on_enemy_drop_pre` 里撒），死亡取消复用虚表 `+0x10`（新增引擎地址 `th18:0x45d590`）。资源侧纯追加 `ability.anm`：13 个 token 单元（零售 `beast1.png`）+ 槽框（`beastf.png`）+ 脚本 137–145。审计 AUDIT §AL。
