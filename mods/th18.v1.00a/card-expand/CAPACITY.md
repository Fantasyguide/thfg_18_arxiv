# card-expand 容量台账 —— 每个「界」是谁砌的、我们怎么长大、现在离墙多远

> **版本**：TH18 v1.00a（`th18.exe`，imagebase `0x400000`，md5 `9969cac756098c1da05a81de45437a70`）。
> 本文裸地址默认属该版本；引用其他版本须写成 `th16:0x…`。
> **方法**：机器对账（`native/tools/capcheck.py` 从仓库源码现算）+ 一手反汇编（
> [`engine/anm/th18/06-entry-limit-and-multi-sprite.md`](../../../engine/anm/th18/06-entry-limit-and-multi-sprite.md)）
> + 实跑日志的装载自报块（§2）。
> **可信度**：✅ 为主（每行的「界」都有一手出处）；🟡 = 只普查了一半；❌ = 已知会撞；⚠ = 满了静默、看不出来。
> **★最紧 = 卡侧最先撞的那条墙**（不是余量% 最小的那条，是「每加一张卡就吃 2 格」的那条）。
> **口径**：余量 < 10% 且没有登记豁免 ⇒ `capcheck.py` 判红。豁免 = **有计划**，不是放行。

## 0. 一句话结论

1. ★ **卡的安全用量 = 33 张**（2026-09-19 起由 `capcheck.py` **现算**并打印；算式与口径见 §1.0，
   数字同时写进附录 A0）。瓶颈 = `abcard.anm` 的 256 entry（**每张卡 2 条**）；其余各墙折算成张数
   都 ≥ 46，最近的三条是 `ability.anm` entry 65、id 相关三条 160。
2. **shooterset 组数组已不再是瓶颈**（2026-09-14 扩容落地）：**宣布可用 96 格、数组区预留 256 格**，
   余量 56 格（58.3%）⇒ **豁免已撤销**（`NEXT.md` §1w 的 1–4 已完成，见 `AUDIT.md` §CQ）。
   从 96 往 256 加格子**只改文件**（数据区基点已一次让够），exe 不用再动。
3. **下一次加卡会先撞 `abcard.anm` 的 256 entry**（还能进 **33 张**），比 `CE_CARD_MAX_NEW` 给的 197 更紧。
   2026-09-14 已给它补上**构建期硬门**（`build_abcard.py` 的 `MAX_ENTRIES` + 印余量，§5-①）——
   从此它是「构建期报红」而不是「实跑里看到卡图错位」。
4. **扩容的死因从来不是「数组满了」，是「有一个读者没改」**。所以每条「长大」都配一份读者清单，
   而 §4 的读者普查是**可重跑**的机器实现 —— 它的第一个结果就推翻了 §1w 的站点数（6 → 14，§5-②）。
5. 数量分两半：**能从仓库现算的**（本表 33 行，`make check` 每次重算）、**只有运行期才知道的**（§2 的自报块）。
   后者一律标 `—`，不进余量闸。

## 1. 台账总表

### 1.0 ★ 卡的安全用量（一个数：**还能进几张卡**）

余量表把「格」混着写（`entry` / `id` / 条目 / `slot`），看表的人得自己换算，于是「还能加多少张卡」
每次都要现场推。所以这里把**每张卡的成本**写死成一份口径，**取最小的那个 = 安全用量**：

| 墙（行 key） | 每张卡吃多少 | 出处 | 现在折算成张数 |
| --- | --- | --- | --- |
| `abcard.anm` entry | **2 条/张**（`_max` / `_min`）| `assets/build_abcard.py` 给 `cards/ORDER.txt` 每行追加 2 条 | **33 ← 瓶颈** |
| `ability.anm` entry | 3 条/张（保守上界；实测 36 张共追加 54 条 ≈ 1.5）| `assets/build_ability.py` + 追加记录 | 65 |
| 卡 id 空间 / `owned[]` / 影子数组 | 1 个 id/张（**从最小的空 id 起连续分配**）| §1 的分配纪律 + `CE_CARD_ID_MAX` = 254 | 160 |
| 图鉴 / 编成条目数 | 1 格/张 | §1 的 `menu_rows` 行（127 那道墙 2026-09-19 已拆）| 160 |
| 文案缓冲 / 显示顺序表 | 1 格/张 | §1 的两行 | 161 / 218 |
| 商店随机池 | `weight + 5` /张（按最小 `weight` = 1 折成 6）| `cards.js` 的 `weight` + `pick_weighted_random_offer` 的池 | 1261 |

**分配纪律（决定 id 那三条怎么算）**：新卡 id **从最小的空 id 起连续分配**（现在 = 119 起），
**不要**从「最高在用 id + 1」往上贴。理由：`owned[]` / `unlocked_cards` 的用量口径是
**最高在用 id + 1** ⇒ 把新卡贴在 200（id 200 的测试卡）之上，那三条各自只剩
`CE_CARD_ID_MAX − 200` = **54** 个位置；而从 119 起连续分配时它们只按「还剩几个空 id」走（160）。
⇒ **跳号分配 = 白扔余量**；测试卡（200）是唯一例外，用完即撤。

**机器对账**：`python3 native/tools/capcheck.py` 每次打印这一段（口径写在它的 `CARD_COST` 里），
`--gen-appendix` 把「还能进 N 张」写进附录 A0 ⇒ **数字随源码变，别手抄进正文当常量**。

机器对账：`python3 native/tools/capcheck.py`（现在是 `make check` 的一环）。下表的每一行都与它的输出
**逐行对过**（用量/上限/余量%，33 行全中），所以**改一行要两边一起改** —— 对不上 `make check` 就红。

| 容量 | 界是谁定的（引擎的哪条指令/常量/地址） | 我们怎么长大 | 用量/上限/余量（现在） | 谁在读它 | 加满时怎么办 | 风险 |
| --- | --- | --- | --- | --- | --- | --- |
| 卡 id 空间（行表 / 分配器 / 跳转表） | `allocate_new_card` 的 `cmp ebx,0x38`（`0x411479`）+ 57 项跳转表（`0x412dac`） | `CE_MAX_ROWS`=255：行表搬 `th18_card_table`、跳转表搬 `th18_card_jumptable` | 35 / 254，余量 219（86.2%） | `sites.py check`（100 处）+ `selfcheck` 的 `table:` / `jumptable:` 自报行 | 抬 `CE_CARD_ID_MAX` 前先过 `abcard.anm` 那道 256 墙 | ✅ |
| `zAbilityManager.owned[]` | 原生 `int[56]`（`+0xc84`..`+0xd64`）；整个对象止于 `+0xd70` | 12 条等长 binhack：owned[] → `+0xd70`、对象 `0xd70 → 0xd70+rows*4` | 118 / 255，余量 137（53.7%） | `native/selfcheck.c` 的 `check_grow()`（`grow:` 自报行） | 跟着 `CE_MAX_ROWS` 自动走（12 处全部由 rows 现算） | ✅ |
| 图鉴 / 编成条目数（`num_total_cards`） | 原生 56 条目写死在 7 处立即数；两处循环上界原是 `cmp r,imm8`（符号扩展 ⇒ 上界 127） | ① patch 把那两处**原地换成**无符号字节比较（`sites.py` 的 `MENU_COUNT_BCMP`，上界 → 255）；② 其余 5 处仍由 DLL 按「56 + 已注册数」现写 | 36 / 197，余量 161（81.7%） | `CE_MENU_COUNT`（7 处）+ `CE_MENU_COUNT_OP`（2 处编码改写的**回读护栏**）+ `CE_MENU`（`__card_ids`，20 处） | 条目数 > 255 才需要再动表行（`CE_MAX_ROWS`）；见 AUDIT §BE-ED / `engine/card/th18/10-extensibility-limits.md` §8.11 | ✅ |
| 卡文案缓冲（`zAbilityText` 重定向） | 原生 `zAbilityText` 只有 57 × `0x1c0`；末张的位置已是尾部字段，原地扩不了 | 不扩对象：`native/text.c` 自带 `s_ext[198][0x1c0]`，3 处断点现算偏移 | 35 / 198，余量 163（82.3%） | `ce_text_off()` + 3 条 `文案重定向 → BP_ce_text_*` | 抬 `CE_MAX_ROWS` 即可（缓冲按 `CE_MAX_ROWS − 57` 自动长） | ✅ |
| 存档影子数组（`unlocked_cards`） | 原生 `zScoreFile.unlocked_cards` = `uint8_t[57]`（`+0x5f588`），其后是未知区 | codecave `th18_card_unlocked`（256 字节，下标 = card id） | 118 / 256，余量 138（53.9%） | 9 条 `unlocked_cards 读 → 影子数组`（`CE_UNLOCK` 9 项）+ 3 个断点 | 影子大小 = id 空间；抬 `CE_MAX_ROWS` 要同步 cave size 与 `sites.py` 的 `0x100` | ✅ |
| 图鉴显示顺序表 | 原生 57 dword（`0x4b3600`）；尾界 `0x4b36e4` 只被 `0x414b54` 的比较用 | 整表搬 codecave `th18_card_order`（6 处引用 + 1 处尾界，全部只换 imm32） | 35 / 255，余量 220（86.3%） | 6 条 `显示顺序表 → codecave（引用）` + 1 条尾界 | cave = `CE_MAX_ROWS` 项；与第 1 行同步抬 | ✅ |
| 商店随机池（`pick_weighted_random_offer`） | 原生是 `ebp-0x8c4` 的 `0x8c0` 字节栈缓冲 = 560 个 dword，末尾紧贴栈金丝雀 | 整段搬 codecave `th18_card_pool`（9 处等长改写），`sub esp` 一个字节不动 | 607 / 8192，余量 7585（92.6%） | `ce_shop_capacity_check`（每次装载跑）+ 9 条 `商店随机池 → codecave` | 先调权重；再抬 cave（`cards_def.h` 与 `sites.py` 两个常量，`_Static_assert` 钉着） | ✅ |
| 商店保证位（必出资源卡 + 关卡卡） | `AbilityShop__initialize` 的 `[ebp-0xe4]` = `CE_SHOP_OFFER_SLOTS` = 57 | 不长大（这是「每次进店摆几个」的名额，不是池子） | 未普查 / 57，不进余量闸 | `ce_shop_capacity_check` 的第二个出口（`guaranteed > 57` ⇒ 装载 FAIL） | 改卡数据（少放必出 / 关卡卡），不改界 | 🟡 |
| 音效 cfg 表 / slot 数组 | 原生 84 行（`0x4c9b80`，stride `0x14`）+ 84 个 slot（`0x56c804`，stride `0x18`） | 四张表整搬 codecave；新 id 从 `0x54` 起预留 32 行 | 87 / 116，余量 29（25.0%） | `native/sound.c` 的初始化自检（I1/I2）+ 51 条 `音效表扩容：…` + `make snd-check` | 抬 `NEW_N`（4 个 cave 的尺寸都由它算）；只加 id 不加行不用动界 | ✅ |
| wav 名表 / blob 指针数组 | 原生 72 项（`0x4b47a0` / `0x56cfe4`） | 同上（cave `th18_snd_names` / `th18_snd_blobs`） | 75 / 104，余量 29（27.9%） | `ce_snd_selfcheck()` + `make snd-check` | 抬 `NEW_N` 即可；⚠ 释尾界刻意只到原生 72（语音 blob 来自 thcrap 堆） | ✅ |
| 每局同时持有的卡对象数（`ce_state` 槽） | 引擎 `cmp [edi+0x28],0x100`（`0x411469`）⇒ 一局最多 256 个卡对象 | 不长大：`sdk_core.h` 的静态槽表按 256 开（256 × 512 B = 128 KiB） | 运行期才知道 / 256，不进余量闸 | `ce_state_alloc()` 满时返回 NULL（调用方必须按「拿不到状态」处理） | 比 id 空间宽（256 > 255），现实中不会先撞；真撞了要改引擎立即数 | ✅ |
| 单卡私有状态字节数（`ce_state_user`） | `CE_STATE_BYTES` 512 − `CE_STATE_RESERVED` 16（块头留给 SDK） | 2026-09-08 已从 256 抬到 512（股市 id 80 要 430+ 字节） | 每卡不同 / 496，不进余量闸 | 各卡文件尾的 `_Static_assert(sizeof(x_card_t) <= 496)`（**11 处**，编译期闸） | 抬 `CE_STATE_BYTES`（静态表 = 256 × 字节数，注意 DLL 体积） | ✅ |
| `abcard.anm` entry（卡图：大图 + 小图各一条） | 引擎硬顶 256：贴图键 = `(槽号 << 8) \| entry 下标`，绑贴图只取低 8 位（`0x486d38` / `0x47e1a4`）；构建期门 = `build_abcard.py` 的 `MAX_ENTRIES` | 只追加（`build_abcard.py`）：每张新卡 2 条 entry | 190 / 256，余量 66（25.8%）⇒ **还能进 33 张** | `build_abcard.py` 的 `MAX_ENTRIES` 硬门（构建期 `SystemExit` + 印余量）+ `make anm-verify` | ①大小图并进一个 entry（一 entry 两 sprite）⇒ 69 → 138 张；②拆到别的空闲 anm 槽 | ✅ ★最紧 |
| `ability.anm` entry（卡特效 / HUD 图集） | 同 256（每个 anm 文件 entry 下标 0..255） | `ORDER.txt` 的图集段：多张小图并进一个 entry（只许追加，`ORDER.frozen.txt` 是硬门） | 54 / 256，余量 202（78.9%） | `build_ability.py` 的 `MAX_ENTRIES` 硬门（构建期 `SystemExit`）+ `make anm-verify` | 继续并图集（`ATLAS-MERGE-AUDIT.md`） | ✅ |
| `ability.anm` sprite 表 | **无上限**：sprite 号是 dword（`vm+0x24`），解析器 `0x477b00` 按 `idx*0x44` 取 | 不适用（当成 255 上限是**旧的误读**） | 435 / 无上限 | `check_ability_anm_bounds.py`（脚本引用的 sprite 必须存在） | 不适用 | ✅ |
| `ability.anm` 脚本表 | **无上限**：脚本号是 dword（`anm+0x120` 指针表 / `anm+0x10c` 脚本体） | 不适用 | 211 / 无上限 | `make anm-verify`（脚本号必须连续、`@NAME@` 必须都能解析） | 不适用 | ✅ |
| `pl0X.anm` entry（自机弹贴图） | 同 256 | `build_player_anm.py` 只追加（原生部分逐字节不变，`--verify-only` 回读校验） | 18 / 256，余量 238（93.0%） | `check_player_anm_bounds.py`（sprite 越界 + sht 的 anm/hit_fx 必须存在） | 同一份 anm 里继续并 entry（117 的兽灵已经这么做） | ✅ |
| shooterset 组数组（`pl0X.sht` 影像 `+0xe0`） | **96 格宣布可用**（数组区 `[+0xe0,0x4e0)` **预留 256 格**，末尾正好顶到数据区）；计数 = 影像头 `+0x02` 的 u16 | **2026-09-14 已扩容**（`AUDIT.md` §CQ / `NEXT.md` §1w）：数组区原地长到预留格数、数据区基点 `0x180 → 0x4e0`，exe 侧 **1 条** 6 字节等长 binhack | **55 / 96，余量 41 格（42.7%）**（2026-09-21 起：108 妖侧逐角色×逐档占 `0x48`–`0x56` 共 **15** 格；逐宏名见本表下方「shooterset 组的具名清单」）| `sht-verify` + **读者普查**（15 处引用 = 14 读者 + 1 加载器，§4）+ `capcheck` 的 `sht_ids.h ↔ patch ↔ 恒等式` 三方对账 | 再要更多格：抬 `CE_SHT_SLOTS`（到 `0xff` 为止，**只改文件**）| ✅ |
| 卡本地池 `TRANCE_SPIRIT_POOL` | 我们自己定的（`trance_core.h`） | 改常量即可 —— `_Static_assert` 会拦住静态区超限 | 运行期才知道 / 128 | 运行时计数 `s_stat_spirit_dropped` | 满 → 丢尾并计数（汇总日志 `pool dropped`） | ✅ |
| 108 卡侧**妖侧激光束 VM**（★ 2026-09-27 第二十八批；卡 108）| 自己定的（`YK_M_LASER_BEAM_UNITS` × `BARS_MAX` = 4 台子机 × 各 ≤12 支残影）| **峰值 ≤13 常驻**（每台子机 ≤3 支实际在画） + **≤10 一次性**（束端命中特效）| 探针 `youkai/probe beam spawn: resident W/G, fx W/G, dropped N` + 累计失败进 600 帧 ledger | `ce_anm_spawn` 失败 ⇒ **只少观感**：伤害按 `bars` 分摊、与 VM 起没起来解耦（单测钉着） | ✅ |
| 卡本地池 `YK_ORB_MAX`（108 的**永点/刻符**：每颗一支 anm VM）| 我们自己定的（`cards/youkai.c`；TH08 那边的 `ItemManager` 道具池**不共享**）| ❓ 自定。**2026-09-27 第三十八批：64 → 128**（owner 口径「扩到 ~1e2，至少把击破 boss 的螺旋渲染出来」）—— 不是拍的：按 `yk_spiral_live_peak()` 反推（推导见 `TUNING.md` §2.1ccc-18）| 运行期才知道 / **128**| 台账行 `peak=`（本局同时在飞的峰值）+ `full=`（池满/VM 起不来而丢掉的颗数）；回归单测钉着 `yk_spiral_live_peak(13, 8, 65) == 119 ≤ 128`| 满 → 这一颗**直接丢**（不排队、不崩）并计入 `full=`（首次起 VM 失败同理）| ✅ |
| 卡本地池 `TRANCE_POPUP_MAX` | 我们自己定的（`trance.c`） | 同上 | 运行期才知道 / 24 | 运行时计数 `s_popup_overflow` | 满 → 顶掉最老的一支并计数（`popup overflow`） | ✅ |
| 卡本地池 `TRANCE_KILL_QUEUE_MAX` | 我们自己定的（`trance_core.h`） | 同上 | 运行期才知道 / 16 | 运行时计数 `trance_kill_queue_dropped` | 满 → 丢尾并计数（`queue dropped`） | ✅ |
| 卡本地池 `TRANCE_BOSS_SLOTS` | 我们自己定的（`trance.c`） | 同上 | 运行期才知道 / 8 | **无运行时余量断言** | 满 → LRU 淘汰最久没受伤的那个（**无计数**） | ⚠ |
| 卡本地池 `TRANCE_SHT_LEDGER` | 我们自己定的（`trance.c`） | 同上 | 运行期才知道 / 4 | 运行时计数 `s_tr_ledger_full` | 满 → 宁可不借（退回原生主炮）并记一行 | ✅ |
| 卡本地池 `TRANCE_PHASE_QUEUE_MAX` | 我们自己定的（`trance.c`） | 同上 | 运行期才知道 / 4 | 运行时计数 `s_stat_phase_full` | 满 → 丢掉这批「相位结束」事件并计数 | ✅ |
| 卡本地池 `ANIMAL_TOKEN_MAX` | 我们自己定的（`animal.c`；TH17 `zGoastManager+0x10` 的 0x28） | 同上 | 运行期才知道 / 40 | 日志 `token pool full`（只有大怪掉落被拦时才记） | 满 → `token_spawn_heading` 返回 0（不生成令牌） | ✅ |
| 卡本地池 `ANIMAL_BIG_DROP_QUEUE` | 我们自己定的（`animal.c`） | 同上 | 运行期才知道 / 32 | 运行时计数 `s_big_drop_lost` | 满 → 丢尾并计数（首次记一行 `queue full`） | ✅ |
| 卡本地池 `UFO_TOKEN_CAP` | 我们自己定的（`ufo_core.h` = `UFO_TOKEN_MAX` 3 + `SLACK` 2） | 同上 | 运行期才知道 / 5 | 运行时计数 `s_tokpool.dropped` | 满 → 丢新令牌并计数（汇总行 `token pool dropped`） | ✅ |
| 卡本地池 `POFV_SPIRIT_MAX` | 我们自己定的（`pofv_core.h`，❓ 自定） | 同上 | 运行期才知道 / 32 | 日志 `spirit pool full` | 满 → 丢掉这支神灵并记一行（`cap_logged` 只记一次） | ✅ |
| 卡本地池 `POFV_STORM_MAX` | 我们自己定的（`pofv_core.h`，❓ 自定） | 同上 | 运行期才知道 / 8 | 运行时计数 + 门 `cap_logged` | 满 → 丢暴风并计数（`blast queue full` 只记一行）。★ 2026-09-22 P5：池从「爆风队列」（`POFV_BLAST_QUEUE_MAX` 64）换成「暴风场池」（每帧判的锚定场，见 `AUDIT.md` §BE-EU） | ✅ |
| 卡本地池 `SEASON_POINT_POOL` | 我们自己定的（`season.c`） | 同上 | 运行期才知道 / 512 | **无运行时余量断言** | 满 → `spawn_point_at` 返回 0（调用方丢）—— 满了一片点到不了手 | ⚠ |
| 卡本地池 `LOLK_TINT_MAX` | 我们自己定的（`lolk.c`） | 同上 | 运行期才知道 / 256 | **无运行时余量断言** | 满 → 直接不染（注释口径「只影响观感」） | ⚠ |
| 卡本地池 `FAITH_MAX_USERS` | 我们自己定的（`faith.h`） | 同上 | 运行期才知道 / 8 | **无运行时余量断言** | 满 → `faith_register` 返回 0（这张卡不计分） | ⚠ |
| 道具池（`ItemManager`，**我们不长大**） | 原生：构造器把 `mgr+0x14` 起 4696 个槽（stride `0xc94`）挂成双向空链表；循环 `cmp eax,0x1258`（`0x4467fc`） | **不长大**（改它会连带几十处链表/遍历假设）；卡侧只**扫**它 | 运行期才知道 / 4696，不进余量闸 | `CE_ITEM_SLOTS` 的读者：`sdk.c`、`sdk.h`、`cards/ufo.c`、`cards/th06rank.c` | 撞顶的表现是**静默 spawn 失败**（空链表返回 0）：分批撒 / 排队补撒，**别去抬这个界** | 🟡 |

### shooterset 组的具名清单

★ `capcheck` 的「有界消费者」闸是**按名字**在 `CAPACITY.md` 正文里找的（见 `native/tools/capcheck.py` 的 `name_gate()`）
⇒ 组宏**必须逐个名字出现**，写缩写（`{M,R,S}0..4`）或只写「见 `NEXT.md`」都会判红。加组的人往这里追加一行。

| 卡 / 用途 | 占格 |
| --- | --- |
| 108 妖侧逐角色×逐档（2026-09-21，P4 第 1 批） | `0x48`–`0x56`（15 格） |
| 108 妖侧**早苗那一组**（★ 2026-09-26：TH16 `pl03sub` 秋/文） | `0x63`–`0x67`（5 格，接在 S4 之后） |

组宏逐条（`sht_ids.h` 里的号）：

- `CE_SHT_SET_YOUKAI_R0` `0x48` · `CE_SHT_SET_YOUKAI_R1` `0x49` · `CE_SHT_SET_YOUKAI_R2` `0x4a` ·
  `CE_SHT_SET_YOUKAI_R3` `0x4b` · `CE_SHT_SET_YOUKAI_R4` `0x4c`
- `CE_SHT_SET_YOUKAI_M0` `0x4d` · `CE_SHT_SET_YOUKAI_M1` `0x4e` · `CE_SHT_SET_YOUKAI_M2` `0x4f` ·
  `CE_SHT_SET_YOUKAI_M3` `0x50` · `CE_SHT_SET_YOUKAI_M4` `0x51`
- `CE_SHT_SET_YOUKAI_S0` `0x52` · `CE_SHT_SET_YOUKAI_S1` `0x53` · `CE_SHT_SET_YOUKAI_S2` `0x54` ·
  `CE_SHT_SET_YOUKAI_S3` `0x55` · `CE_SHT_SET_YOUKAI_S4` `0x56`
- ★ **早苗（TH16 秋/文）** `CE_SHT_SET_YOUKAI_A0` `0x63`（**空组** = 0 台子机那一档）·
  `CE_SHT_SET_YOUKAI_A1` `0x64` · `CE_SHT_SET_YOUKAI_A2` `0x65` · `CE_SHT_SET_YOUKAI_A3` `0x66` ·
  `CE_SHT_SET_YOUKAI_A4` `0x67` —— 一手 = TH16 `pl03sub.sht` 的 set1..set4（秋 = 射命丸文；
  `engine/season/th16/01-…md` §5.5），槽号取自 `CE_SHT_SET_YOUKAI_S4 + 1`（**只在末尾追加**，
  R/M/S 的号不变 —— `youkai.c` 有 `_Static_assert` 钉死），见 `AUDIT.md` §BE #96。

#### 108 使魔的**魔法阵环**（2026-09-22，P4）—— 具名登记

- **资产**：`ability.anm` **单图行** `YOUKAI_FAM_RING`（`ability/youkai_familiar/YOUKAI_FAM_RING.png` 64×64）
  ⇒ sprite **479**；脚本 **309**（`CE_ANM_ABILITY_SCRIPT_YOUKAI_FAMILIAR_RING`）。
  它是**单图行、不是图集 entry** ⇒ 不吃 `ATLAS_YK_FAM` 那一段的画布，也不改 `abcard.anm` entry 数
  （本表的「新图集 entry 必须具名出现」这条由上面那条单图行满足；`capcheck` 实跑仍全绿）。
- **运行时（★ 2026-09-22 夜 owner 二次订正后）**：环**不再常驻在使魔身上** —— 它是**切换那一下的
  短命特效**（`youkai.c` 的 `yk_ring_blink` / `yk_ring_tick`，池 `YK_RING_MAX 4`，存活
  `YK_RING_LIFE_FRAMES` = 14 帧后发 interrupt 3，脚本再淡出 10 帧自删）⇒ 常驻 VM 峰值**回到 ≤32**
  （只有使魔本体）。引擎的 ANM VM 池不是本表那种「有界且满了会静默改行为」的定长数组
  ⇒ 这一条**不进**上表的容量行；实跑判据 = 按 Shift 时**使魔位置各闪一圈、很快消失**，
  而不是使魔身上一直挂着环（起不出来 = 日志 `youkai: switch ring VM spawn FAILED`）。
- 一手：TH08 建使魔时 `AllocEffectSlot(0x20, …)` → 模板表 `0x4c6d30[0x20]` = `etama.anm` script48
  （`th08:0x41b003` / `:0x41e589`）；人/妖 = `SetInterrupt(环, IsYoukai() ? 2 : 1)`。

#### 108 使魔的 Shift **爆闪**（2026-09-22，#22 的另一半）—— 具名登记

- 资产：`ability.anm` **单图行** `YOUKAI_FAM_FLASH`（`ability/youkai_familiar/YOUKAI_FAM_FLASH.png` 32×32）
  ⇒ sprite **480**；脚本 **310**（人→妖，TH08 `etama.anm` script59）/ **311**（妖→人，script60）。
  单图行、不是图集 entry ⇒ 不吃任何图集画布，`abcard.anm` entry 数也不动。
- 运行时：**瞬时** VM（脚本自己 `+10: delete()` ⇒ ≤10 帧），一次形态切换至多 2 支
  ⇒ 不计入常驻容量，也不需要卡侧跟踪（`on_delete` 不必收尾）。
- 一手：`th08:0x42c44e` / `0x42c4ef` 的 `SpawnEffect(0x1f / 0x1e, &enemy+0x2d88, 1, …)` → 模板表
  `0x4c6d30[0x1f]` = script59、`[0x1e]` = script60；两支共用 `etama2.png (65,209,30,30)`。

#### 108 **本体**的 Shift 光环 + 两下本体爆闪（2026-09-22 夜，#23）—— 具名登记

- 资产：`ability.anm` **单图行** `YOUKAI_FAM_AURA`（`ability/youkai_familiar/YOUKAI_FAM_AURA.png` 64×64）
  ⇒ sprite **481**；脚本 **312**（TH08 `etama.anm` script54，**零翻译**照抄）。单图行、不是图集 entry
  ⇒ 不吃任何图集画布，`abcard.anm` entry 数不动。
- 另外两支复用**同一张爆闪图**（`YOUKAI_FAM_FLASH`，sprite480）⇒ **不新增贴图**：
  脚本 **313**（TH08 script58，人→妖，色 `0x80ff8080`）/ **314**（script57，妖→人，色 `0x808080ff`）。
- 运行时：光环**常态最多 1 支**（自机身上；`yk_aura_tick` 每帧跟随 + 自愈）；爆闪是**瞬时**
  （脚本自己 `+10: delete()`）。⇒ 常驻 VM 增量 = 1，不进上表的容量行，只在此具名。
  实跑判据 = 低速时自机身上应当有那一圈（日志 `youkai: body Shift aura ON`）。
- 一手：`Player::move`（`th08:0x44aec0`）`0x44b194 SpawnEffect(0x16, &player+0x2b4, slot 2, color -1)`
  ⇒ 模板表 `0x4c6d30[0x16]` = script54；妖→人 `0x44b3e3 SetInterrupt(光环, 1)`。

#### 108 的字形图集扩容：**第 14 格 = '+'**（2026-09-22 夜，#27 弹字）—— 具名登记

- 资产：`pl0X.anm` 每机的 `yk_gaugefont` entry（`player/yk_gaugefont.png`）从 **13 格 → 14 格**
  （画布 104×12 → **112×12**），新增格 = TH08 `ascii.anm` entry0 **sprite149** `(104,208,8,12)` = **'+'**
  （逐像素确认十字形）；每机 sprite 区间相应 +1（pl00 的 `CE_PL_ANM_YKGAUGEFONT_BASE_PL00` 仍是 102，
  区间变成 **102..116**，第 13 号 = '+'）。
- 走既有路径：`assets/build_player_anm.py` **只追加** + `verify_rebuilt` 回读闸 + `pl_anm_ids.h` + 本行具名。
- 用途 = #27 的**刻符弹字**（「+1」那支要用 '+'）；`ability.anm` / `pl0X.sht` / wav / 站点**本批未动**。

#### 音效表扩容：新 SE 槽的具名登记（2026-09-22）

- **`0x5d` `YOUKAI_FAM_SHIFT`**（`assets/voice/ORDER.txt` 第 9 个新行；`patch/th18/voice.js` 里 id 显式写 93）
  —— 一手 = TH08 `se_ophide.wav`（**`th08:0x42c489 push 0x28`** → 声音表 `0x4c8040+id*8` 的 `+0x00` = wav 32），
  **人→妖（高速→低速）**那一支的 Shift 音；TH18 71 枚里没有它 ⇒ 构建期从 `local/th08.v1.00d/dat/` 现取
  （`derive_retail_se.py` 的 TABLE）。★ **2026-09-26 订正**：上一版把声号/方向都记反了
  （旧文写「妖→人 + `0x4c8046+id*8` 第二格」= 表基准错位一格所致，两处都已改；卡侧现在也只播人→妖那一侧）。
- 反方向（**妖→人**，`th08:0x42c529 push 0x27` → wav 31 = `se_opshow.wav`）TH18 也没有，但按 owner 口径
  2026-09-26「只要人→妖那一声」⇒ **不占新槽、不播**。以后要补就得再开一个扩表槽（余量见下）。
- 旧注「人→妖 直接用 TH18 `0x0c`（= se_invalid，字节相同）」**作废**：直读 TH18 自己的表
  （`th18:0x4c9b80` stride `0x14`、名表 `th18:0x4b47a0`）`0x0c` = `se_select00.wav`，TH18 的 `se_invalid` 是 `0x05`。
- 余量：新 id 空间 `0x54..0x73` 共 32 格 ⇒ 已用 10 格（`0x54`–`0x5d`）、**余 22 格**。

**余量最小的四条**（按余量% 排序）：shooterset 组数组 **0.0%**（40/40，有 §1w 计划）·
音效 cfg 表 **25.0%**（87/116）· wav 名表 **27.9%**（75/104）· `abcard.anm` entry **74.2%**（190/256）。
前三条都是「界，但还没到」；**卡侧下一道真的墙是 `abcard.anm`** —— 它每加一张卡就吃 2 格，
比任何一个 id 空间的界都先到（§5-①）。

## 2. 运行时自报块（数量只有运行期才知道的那些）

表里标 `—` 的行，用量静态推不出来。**真值在实跑日志里**（本机路径见 `local/GAME-LOG.md` 铁律；
日志开头 `=== th18_card_expand <时间> ===` 是本次启动的分界）。装载时 DLL 会把每一界自报一遍 ——
这就是台账的**运行期对账**，与本表的静态数字必须一致，不一致就是 DLL 没同步或 patch 没打上：

```
table: 255 rows filled at 03ba0000 (58 retail + 197 NULL copies); NULL/BACK shop weight := 6
jumptable: 255 entries at 03ba33d0 (57 retail + 198 -> case56 @ 00411489); allocator bound = 254
grow: zAbilityManager 0xd70 -> 0x116c, owned[] at +0xd70 (255 entries), shop loops 255 ids
unlocked: shadow @ 03ba37d0, 9 read sites + 3 breakpoints verified; side-car = <存档 side-car 路径>
text: ids 57..254 redirected to ext buffer @ 6df5a598 (198 entries x 0x1c0), 3 breakpoints verified
pool: 9 sites verified, 8192 slots at 03ba3ce0 (retail stack buffer was 560)
cards: 35 registered from cards.js; shop pool 700/8192 slots, guaranteed offers <= 17/57
```

逐行读法（左 = 日志的标签，右 = 本表哪一行）：

| 标签 | 报的是哪条容量 | 报出来的界（与仓库现算对得上吗） |
| --- | --- | --- |
| `table:` | 卡 id 空间的行表部分 | 255 行 = `CE_MAX_ROWS`；`58 retail` = `CE_ROW_COUNT` |
| `jumptable:` | 卡 id 空间的分配器 + 跳转表 | `allocator bound = 254` = `CE_CARD_ID_MAX` |
| `grow:` | `zAbilityManager.owned[]` | `0xd70 -> 0x116c` = `CE_MGR_SIZE → CE_MGR_SIZE+255*4`；`owned[] at +0xd70` |
| `unlocked:` | 存档影子数组 | `9 read sites` = `CE_UNLOCK` 9 项（★ 曾经在某处写成 6，已订正） |
| `text:` | 卡文案缓冲 | `198 entries x 0x1c0` = `CE_MAX_ROWS − 57` |
| `pool:` | 商店随机池 | `8192 slots` = `CE_SHOP_POOL_SLOTS`；`retail stack buffer was 560` |
| `cards:` | 商店池实时用量 + 保证位 | `700/8192`、`guaranteed offers <= 17/57` |
| `snd:` | 音效表 / wav 名表 | `116 rows` = `CE_SND_ROWS_TOTAL`；`I1/I2 hold` = 两条不变式 |

⚠ **`cards:` 那一行的池用量与本表静态值不同是正常的**：静态值按每个 id **都给满** Σ(weight+5) 算
（最坏情况），运行期那行是**本次真正注册的**权重之和。两者都在，才既能防撞顶、又能看现状。

## 3. 逐行注（出处 / 风险，表里放不下的）

### 3.1 卡 id 侧

- **卡 id 空间**：三处一起动才算「长大」——行表（`th18_card_table`）、分配器立即数（`cmp ebx,0x38`）、
  跳转表（`th18_card_jumptable`）。行表与跳转表都按 `CE_MAX_ROWS` 开，现用 **255** 项（下标 0..254）；
  跳转表多出来的项全部指向 `case56`（`0x411489`）。id 上界因此是 `CE_CARD_ID_MAX` = 254。
  出处：`native/sites.py` L46-80、`native/sites_gen.h` L4-15；patch `allocate_new_card: cmp ebx, 0x38 → 0xfe`。
- **`owned[]`**：native 的 owned[] 是 `int[56]`，id ≥ 56 时 `allocate_new_card` 会写到对象之外 ——
  这是「255 行」这条战线最早的一条硬伤。12 条 binhack 把它搬到对象尾，并且**全部由运行时 rows 现算**，
  所以 `selfcheck` 的 `check_grow()` 能在每次装载时逐条重算比对（这也是为什么它不用登记豁免）。
- **图鉴 / 编成条目数**（✅，2026-09-19 拆墙）：原本「条目数」被两处循环上界 `cmp r,imm8`（**符号**立即数）
  卡在 127 以内 ⇒ 新卡上限 71。现改成「无符号字节比较 + `jb`」（原地换 2 字节、立即数位置不变），
  上界 → **255**，于是这条不再紧于 `CE_CARD_ID_MAX`；实跑验证见 `10-extensibility-limits.md` §8.12。
- **存档影子数组**：native 只有 57 字节，其后紧跟别的存档字段 ⇒ **原地扩不了**，只能影子数组 + 9 处读改指。
  抬 id 空间时影子要同步（`sites.py` 里的 cave size `0x100`）。
- **显示顺序表**：`0x4337f7` 引用了紧挨着的**另一张表**（同值不同表）——`ORDER_EXCLUDE` 里明确 **不许改**。

### 3.2 anm 侧（entry 上限 256 的真身）

- **上限是什么**：贴图键 = `(anm 槽号 << 8) | entry 下标`，绑贴图时**只取低 8 位**
  ⇒ 一个 anm 文件最多 **256 个 entry**（下标 0..255）。sprite 号与脚本号是 dword、**没有上限**。
  出处：`engine/anm/th18/06-entry-limit-and-multi-sprite.md` §1.1/§1.2（打包 `0x486d38`、解包 `0x47e1a4`）。
- **一个 entry 多个 sprite 是原生的常态**（原生单 entry 最高 368 个 sprite）⇒ 这是我们的**主要长大手段**：
  把相关的十几张小图并进一张画布/一个 entry。
- **`ability.anm` 现在 54 entry**（原生 7 + 追加 47；2026-09-15 加了 `ATLAS_TH13ASCII` 与
  `ATLAS_FAITH_HUD`，2026-09-16 加了 `ATLAS_FLAN`）。数法（可复跑）：
  `thanm -l 18 native/build/ability.anm | grep -c '^entry'`（现算 54 = `capcheck.py` 的「图集 entry」行）。
- **★ 2026-09-15 新增 `ATLAS_TH13ASCII`（`ability.anm` 的 entry 51）**：卡 113 的灵界剩余时间用
  TH13 大字体（`0..9 . s` 共 12 个字形，画布 128×120@4x3）。**追加在 ORDER.txt 末尾** ⇒
  `anm_ids.h` 的 diff 只有新增（新 sprite = 418..429），**既有 sprite 号一个都没动**（已核对）。
  只用掉 1 个 entry（50/256），sprite 号无上限所以不心疼；这是"必须说清它占哪一格"的登记，
  出处与切图依据见 `assets/ability/make_th13_ascii_art.py` 与 `AUDIT` §CZ。
  ★ 为什么不复用 TH18 的 `ATLAS_FONT2`：用户 2026-09-15 口径 —— 灵界的字符要是 TH13 那张
  `ascii.png` 的字形（TH18 里没有那套图），走"原版贴图 → ability.anm → UI 层 VM"（同 115/TH15 卡 UI）。
- **★ 2026-09-15 新增 `ATLAS_FAITH_HUD`（`ability.anm` 的 entry 52）**：110/111 的**组合 UI**
  （`NEXT.md` §2A）行 2 里那个「×」字形 = **TH18 自己 font 2 的 `'*'`**
  （`local/th18.v1.00a/anm/ascii1280/ascii/ascii_1280.png` 的 sprite209 `(208,320,16,20)`，**形状就是 ×**，
  `make_faith_hud_art.py --dump` 逐像素复核）—— 它是「同一个字符 + 同一套字体同一基线」，
  ★ 2026-09-15f 换过字形（第一版借的是 TH11 `ascii.anm` entry0 sprite10 = **四角菱形**，不是 ×，已废）。
  画布 **16×32**、成员一条 **`@0,0,16x32`**（= 与 `ATLAS_FONT2` 同一套「16×32 格 + 字形居中」约定，
  别按墨迹裁：`scale.x` 乘的是 sprite 矩形宽，`th18:0x481703`）；
  **追加在 ORDER.txt 末尾** ⇒ 新 sprite = **430**、同批新脚本 = **275/276**，
  **既有 sprite / 脚本号一个都没动**（`ORDER.frozen.txt` 的前 186 行逐行未变，构建期已核）。
- **★ 2026-09-24 新增 `ATLAS_FONT2B`（`CE_ANM_ABILITY_ATLAS_ATLAS_FONT2B`，`ability.anm` 的 entry **69**；
  ⚠ 补宏名与现值的是 110/111 这条线，只为让 `capcheck` 的硬门认得出 —— 正文与口径都是 112 那位写的，
  见 `AUDIT.md` §BE 详录 #85）**：卡 112 的 HUD 文字字形
  —— **16 个成员**（TH18 font 2 的完整字形集 + `:` 与 `*`）。为什么要另起一格而不是往
  `ATLAS_FONT2` 里加两行：`ce_hud_vm_text_*` 的每个字形 VM 都绑在一支**固定脚本**上，而引擎按
  **该脚本所属 entry 的精灵表**解析 `vm->sprite` ⇒ 想让 112 用上 `:` / `*`，它们就必须来自**同一张表**；
  而 `ATLAS_FONT2` 在 `ORDER.frozen.txt` 的**已定稿段**里（插行 = 全库 sprite 号平移，构建期会拒）。
  ⇒ 复制整套字形另起一格 + 脚本 **332**（`332_font2b_glyph.anm.txt` = 脚本 136 的孪生）
  + SDK 第三档字体 `CE_HUD_FONT_TH18_FONT2B`（度量与第 0 档逐值相同，只多认两个字符）。
  画布 **128×64@8x2**（16 格刚好放下）；**append 在最末尾** ⇒ 新 sprite 在文件末尾追加、
  **既有号一个没动**。切图 = `assets/ability/make_font2_art.py`（同一批字形同时落 `font2/` 与 `font2b/`）。
  只占 1 个 entry（53/256，余量 203）；生成器 `assets/ability/make_faith_hud_art.py`，
  合并块的几何与逐项判定见 `engine/player/th11/04-merged-hud-spec.md` 与 `AUDIT` §DB/§DC。
- **★ 2026-09-26 新增 `ATLAS_LOLK_RING_RED`（`ability.anm` 的 entry 76）**：111 咲夜×大小姐那条支援的
  **擦弹圈贴图**（owner 亲手 P 的图，内容**青蓝** ⇒ 脚本 360 是 `blendMode(2)` 反向减算 ⇒ 看起来是红）。
  画布 **128×128**、**1 个成员**（`LOLK_RING_RED`）；**追加在 ORDER.txt 末尾** ⇒ 新 sprite = **539**；
  115 自己那支仍用原色 `RING.png`（同一个形状、两个 sprite，互不影响）。
  只占 1 个 entry（**76/256，余量 180**；`anm_ids.h` 里 = `CE_ANM_ABILITY_ATLAS_ATLAS_LOLK_RING_RED` = **76**、
  `CE_ANM_ABILITY_SPRITE_LOLK_RING_RED` = **539**）。
- **★ 2026-09-25 新增 `ATLAS_FLAN_RING`（`ability.anm` 的 entry 75）**：111 咲夜蓄力光圈的
  **外圈粗环**（owner：「我不想要细环，我就想要粗环…满充能才是彩虹变色，充能中是白的」）。
  **自绘**（不是原生切图）：128×128 纯白粗环，外径 62 / 内径 42 ⇒ 环带厚 **20 px = 外径的 32%**；
  生成器 `assets/ability/make_flan_art.py` 同目录的 **`make_flan_ring_art.py`**（可复现）。
  画布 **128×128**、**1 个成员**（`FLAN_RING`）；**追加在 ORDER.txt 末尾** ⇒ 新 sprite = **538**、
  同批新脚本 = **359**（`CE_ANM_ABILITY_SCRIPT_FLAN_RING_BAND`）；既有 sprite / 脚本号一个都没动。
  只占 1 个 entry（**75/256，余量 181**；`anm_ids.h` 里 = `CE_ANM_ABILITY_ATLAS_ATLAS_FLAN_RING` = **75**、
  `CE_ANM_ABILITY_SPRITE_FLAN_RING` = **538**）。
  白色贴图 ⇒ 脚本用 `color` 就能染色（充能中纯白、满蓄力走 6 等份色相循环）。
  ⚠ 契约：可见外径 = `64 × VM scale` ⇒ 卡侧 `scale = 半径 / 64`（`signal.c` 的 `SIGNAL_FLAN_RING_TEX_R`）。
- **★ 2026-09-16 新增 `ATLAS_FLAN`（`ability.anm` 的 entry 53）**：111 的二小姐蓄力特技
  （`NEXT.md` §2N）「松手清屏」那一下的爆闪 = **TH18 原生卡 42（小槌）挥击的 4 个 sprite 的 1:1 切图**
  （原生 `ability.anm` entry0 `player/option/op00.png` 的 sprite41 `(32,80,32,16)` 与
  sprite85/86/87 `(0/48/96,160,48x47)`；一手 = 该文件 script28 正文 + 卡侧 `set_sprite(0x29)` `0x40f161`）。
  画布 **144×64**、4 个成员（`FLAN_MALLET_IDLE` + `FLAN_MALLET_SWING_0/1/2`）；
  **追加在 ORDER.txt 末尾** ⇒ 新 sprite = **431..434**、同批新脚本 = **277/278**（蓄力光环 script277 走
  `drawPoly`、`sprite(-1)` ⇒ **一个 sprite 都不占**；爆闪 script278 才用这 4 个），
  **既有 sprite / 脚本号一个都没动**（`ORDER.frozen.txt` 的前 186 行逐行未变，构建期已核）。
  只占 1 个 entry（54/256，余量 202；`anm_ids.h` 里 = `CE_ANM_ABILITY_ATLAS_ATLAS_FLAN` = **53**、
  `CE_ANM_ABILITY_SPRITE_FLAN_MALLET_IDLE/_SWING_0/_SWING_1/_SWING_2` = 431..434、
  `CE_ANM_ABILITY_SCRIPT_FLAN_CHARGE_RING` = 277 / `…_FLAN_CLEAR_BURST` = 278）；
  生成器 `assets/ability/make_flan_art.py`，
  一手全链与「为什么不直接写 `sprite(85)`」见该文件头与 `engine/card/th18/13-mallet-clear-and-th17-charge.md` §1.4/§3.2。
  - ★ **2026-10-01（P7）在 277 上加了「蓄满 ⇒ 换色闪烁」的 `interruptLabel(2)`**（TH12 `bullet.anm`
    script204 的 4 色 × 每色 2 帧无限循环 + 卡侧蓄满那一帧发中断 2）：**只加脚本正文、不加 entry/sprite、
    不加脚本号、不加音频** ⇒ 本台账的每一个用量数字**都没变**（`capcheck` 全绿，见 `AUDIT.md` §BE-FI）。
- **★ 2026-09-16k 新增 `ATLAS_MOF_BOMB`（`ability.anm` 的 entry 54）**：110/111 的**灵击视觉**
  （`NEXT.md` §2N-补2）里那颗**火星** = **TH10 `bullet.anm` entry2 的 `sprite330` 的 1:1 切图**
  （`(0,224,32,32)`，一颗纯白四芒星；一手 = TH10 `bullet.anm` script398/399 正文第一行
  `sprite(sprite330)`，两支脚本已逐条移植成 script280/281）。画布 **32×32**、1 个成员
  （`MOF_BOMB_SPARK`）；**追加在 ORDER.txt 末尾** ⇒ 新 sprite = **435**、同批新脚本 = **279/280/281**
  （279 = 消弹范围环，走 `drawPolyBorder` + `sprite(-1)` ⇒ **一个 sprite 都不占**，而且 **110 与 111 共用**），
  **既有 sprite / 脚本号一个都没动**（`ORDER.frozen.txt` 的前 186 行逐行未变，构建期已核）。
  只占 1 个 entry（55/256，余量 201；`anm_ids.h` 里 = `CE_ANM_ABILITY_ATLAS_ATLAS_MOF_BOMB` = **54**、
  `CE_ANM_ABILITY_SPRITE_MOF_BOMB_SPARK` = **435**、
  `CE_ANM_ABILITY_SCRIPT_BOMB_CLEAR_RING` = 279 / `…_MOF_BOMB_SPARK` = 280 / `…_MOF_BOMB_SPARK_BLUE` = 281）；
  生成器 `assets/ability/make_mof_bomb_art.py`；一手与「⚠ 读 TH10 必须换 `v4.anmm`」见该文件头与
  `AUDIT.md` §BE 详录 #31。
- **★ 2026-09-16m 新增 `ATLAS_MARISA_BULLET`（`ability.anm` 的 entry 55）**：111 的**魔理沙子机弹**
  （用户实跑「魔理沙发弹样式不对」→「**弹的贴图**」）= **TH18 原生 `pl01.anm` entry0 的
  `sprite24/25/26/27` 的 1:1 切图**（一手：装备卡子机的弹贴图来自 `ability.anm`，`th18:0x40aa03`
  换 `player+0x10`；矩形见 `pl01.anm.txt` 第 40–43 行）。画布 **120×14**、4 个成员
  （`MARISA_BULLET_0..3`）；**追加在 ORDER.txt 末尾** ⇒ 新 sprite = **436..439**、
  同批新脚本 = **282**（停掉旧的「借 116 季节点弹」那一步）；
  **既有 sprite / 脚本号一个都没动**。只占 1 个 entry（56/256，余量 200；
  `anm_ids.h` 里 = `CE_ANM_ABILITY_ATLAS_ATLAS_MARISA_BULLET` = **55**、
  `CE_ANM_ABILITY_SPRITE_MARISA_BULLET_0..3` = 436..439、
  `CE_ANM_ABILITY_SCRIPT_MARISA_BULLET` = 282）；生成器 `assets/ability/make_marisa_bullet_art.py`。

- **★ 2026-09-16n 新增 `ATLAS_MARISA_ELEM`（entry 56）+ `ATLAS_MARISA_ELEM_BULLET`（entry 57）**：
  111 的**魔理沙元素子机与元素子机弹**（用户口径：「**你得用 th11 的魔理沙子机贴图和弹幕样式呀**」
  +「**切换子机的部分是帕秋莉的特效啊！**」）= **TH11 `pl01.anm` entry0 的 1:1 切图**
  （球 = `sprite38..42` `(64/80/96/112/128,160,16,16)`，弹 = `sprite49/45/24/53/57`
  `(0,208/192/144/224/240,32,16)`；一手：`th11:0x433465`/`0x433470` 选脚本 34..38、
  `th11:0x4341fb`/`0x4341ff` 取弹脚本）。画布 **80×16**（5 球）+ **160×16**（5 弹）；
  **追加在 ORDER.txt 末尾** ⇒ 新 sprite = **440..444** 与 **445..449**、同批新脚本 = **283..292**，
  **既有 sprite / 脚本号一个都没动**。两个 entry（58/256，余量 198；
  `anm_ids.h` 里 = `CE_ANM_ABILITY_ATLAS_ATLAS_MARISA_ELEM` = **56** / `…_MARISA_ELEM_BULLET` = **57**）；
  生成器 `assets/ability/make_marisa_elem_art.py`（`CE_ANM_ABILITY_ATLAS_ATLAS_MARISA_ELEM_BULLET` = **57**，
  `CE_ANM_ABILITY_SPRITE_MARISA_ELEM_BULLET_*` = 445..449）。
- **★ 2026-09-16p 新增 `ATLAS_MOF_BOMB_RING`（entry 58，`CE_ANM_ABILITY_ATLAS_ATLAS_MOF_BOMB_RING`）**：
  110/111 灵击环的**电流边**竖条 = **TH10 `bullet.anm` entry3 的 `sprite362`（65,0,14,128）1:1 切图**
  （一手：四支柱环 script394–397 的 `sprite(sprite362)` + `texCircle(32)`）。
  画布 **14×128**、1 个成员；**append 在最末尾** ⇒ 新 sprite = **450**、同批新脚本 = **293**
  （`CE_ANM_ABILITY_SCRIPT_MOF_BOMB_RING_STRIP`），既有号一个没动（entry 59/256，余量 197）。
  生成器 `assets/ability/make_mof_bomb_art.py`（本批扩了它）。
- **★ 2026-09-16q 新增 `ATLAS_NITORI_BUBBLE`（entry 59，`CE_ANM_ABILITY_ATLAS_ATLAS_NITORI_BUBBLE`）**：
  111 的**荷取（河童）罩子水泡**（用户口径：「放B是消弹，给一个**防护罩（有贴图特效）**…对于原生的B，
  河童特效也会引入…一个罩子」）= **TH11 `pl01.anm` entry3 的 `sprite76`（1,1,254,254）1:1 切图**
  （一手：`th11:0x4076ec push 0x3a` 起的是 `pl01.anm` **script58**，正文第一行 `sprite(sprite76)`；
  `player+0x10` = 已加载的角色 anm，`th11:0x42f8c9`）。画布 **254×254**、1 个成员
  （`NITORI_BUBBLE`；254 非 2 次幂，`build_ability.py` 已明确放开这条，一手 `th18:0x486488`）；
  **append 在最末尾** ⇒ 新 sprite = **451**、同批新脚本 = **294**（`CE_ANM_ABILITY_SCRIPT_NITORI_BUBBLE`），
  既有号一个没动（entry 60/256，余量 196）。生成器 `assets/ability/make_nitori_bubble_art.py`。

- **★ 2026-09-18 新增 `ATLAS_UFO_DROP`（entry **61**，`CE_ANM_ABILITY_ATLAS_ATLAS_UFO_DROP`）**：
  卡 112 的**掉落物实体**贴图（用户 2026-09-18：「击破还会掉落一个**被自动吸收的压缩包**（**应该是有实体的**）」
  + P6「整颗 Bomb …有没有可能额外建一个掉落 Bomb 整卡的 VM（会小题大做吗？）」—— 答复：不小题大做，
  两者共用同一套自建掉落物实体）= **TH12 `bullet.anm` entry5（`bullet/etama8.png` 256×256）的 4 张 1:1 切图**
  （一手：道具自身 anm 脚本 = type+0xa5，`th12:0x4275b2`/`0x427712`）：
  `BOUNTY_BAG` = sprite459 (225,113,30,30)、`DROP_ICON_P` = sprite426 (0,96,16,16)、
  `DROP_ICON_POINT` = sprite427 (16,96,16,16)、`DROP_BOMB` = sprite432 (96,96,16,16)。
  画布 **64×64 → 80×80**（★ 2026-09-23 第二轮：`DROP_BOMB` 的画布 32×32 → **40×44**，
  = 一手 TH18 `abcard/ability/abitem.png` 的矩形；贴图源同时换成 `abcard/ability/BOMB_max.png`
  = **有 3 px 纯黑边**那版，见 `SINGLE-CARD-TEST.md` §本批-5）、4 个成员；
  **append 在最末尾** ⇒ 新 sprite = **453..456**、同批新脚本 = **296..301**
  （`UFO_BOUNTY_BAG{,_P,_MONEY}` / `UFO_DROP_ICON_{P,POINT}` / `UFO_BOMB_ITEM`），
  既有号一个没动（entry 61/256，余量 195）。生成器 `assets/ability/make_ufo_drop_art.py`。
  ★★ **2026-09-24 第 22 轮（owner「和 B 碎一样大」「各方面 == B 碎卡」）**：本图集长成
  **128×128、5 个成员**（`DROP_BOMB` 20×25 → **64×80** = 原生 `BOMB_min.png` sprite9 **1:1**；
  `CARD_GLOW` 28×35 → **40×44** = 原生 `abitem.png` sprite1 **1:1**；倍率 0.3 / 1.0 写进脚本 345/331，
  逐值同原生 `abcard.anm` script15/16）⇒ 满 B 与 B 碎卡**同尺寸同配方**。
  sprite 号仍是 **453..457**（行序没动，只是画布变大）、entry 61 不变；明细见 `TUNING.md` §2.0zzx-2。

- **2026-09-26 已撤出：`ATLAS_YK_BULLET`（原 entry 79）+ 脚本 364..370** —— 卡 108 的妖侧弹体贴图
  **不在 `ability.anm` 里了**（一手：行里 `+0x22` 按 **`pl0X.anm`** 解析，见 `AUDIT.md` §BE #105）。
  现在：成员图 `assets/yk_bullet/*.png` + 7 支 spec `assets/yk_bullet/scripts/`，
  由 `assets/build_player_anm.py` 的 `bullet` 追加件摆进**四个 `pl0X.anm` 各一张小画布**
  （128×16 / 320×32 / 80×32 / 256×64，脚本号 54/55 · 60/61 · 43/44 · 45）。
  ⇒ `ability.anm` 的 entry/sprite/脚本号**全部回到撤出前的状态**（那 7 支本来是最后的 7 支脚本）。

- **★ 2026-09-24 第 23 轮新增 `ATLAS_UFO_FRAME`（entry **74**，`CE_ANM_ABILITY_ATLAS_ATLAS_UFO_FRAME`）**：
  卡 112 的**满 B 卡框**（owner 实跑「**缺少一个黑色的边框**」）= TH18 `abcard/ability/abframe.png`
  sprite0 (0,0,64,80) 的 **1:1 切图**（原生卡道具那棵树的 script17 画的就是它；画布上**只有外圈 4 px
  是白的**、内部 alpha=0，乘 `color(0,32,0)` ⇒ 那圈近黑细边）。
  画布 **64×80**、1 个成员 `DROP_FRAME`；**append 在最末尾**（**单独一个图集**，不塞进 `ATLAS_UFO_DROP`
  的中间行 —— 行序 = 编号，插中间会让后面所有卡的 sprite 号平移）⇒ 新 sprite = **537**、
  同批新脚本 = **358**（`CE_ANM_ABILITY_SCRIPT_UFO_CARD_FRAME`），既有号一个没动（entry 74/256，余量 182）。
  生成器 `assets/ability/make_ufo_drop_art.py`（片段 `entries/ORDER.ufo_frame.txt`）。

- **★ 2026-09-25 第 27 轮新增 `ATLAS_FONT_TH12`（entry **77**，`CE_ANM_ABILITY_ATLAS_ATLAS_FONT_TH12`）**：
  卡 112 的**击破弹字字形**（owner 口径：「×n.0 和点数的字体应该用 TH12 `ascii/ascii.png`
  **倒数第三行的数字**」）= TH12 `ascii/ascii.png` 里 **y=216（从底下数第三行）那族 16×16** 的 13 格 1:1 切图
  （一手：`0-9` = sprite 243..252、`.` = 254、`,` = 257、乘号 = 256 —— 逐格渲染核对）。
  画布 **128×32@8x2**、13 个成员（`F12_D0..F12_D9` / `F12_DOT` / `F12_COMMA` / `F12_X`）；
  **append 在最末尾** ⇒ 新 sprite = **540..552**、同批新脚本 = **361**
  （`CE_ANM_ABILITY_SCRIPT_FONT_TH12_GLYPH`），既有号一个没动（entry 77/256，余量 179）。
  生成器 `assets/ability/make_th12_font_art.py`（片段 `entries/ORDER.font_th12.txt`）；
- **★ 2026-09-26 第 33 轮新增 `ATLAS_UFO_BURST`（entry **78**，`CE_ANM_ABILITY_ATLAS_ATLAS_UFO_BURST`）**：
  卡 112 的 **UFO 召唤爆闪/雷**（owner：「把 TH12 的 script134/112 原样搬过来」）= TH12
  `enemy.anm` entry1 **`enemy/enemy2.png` 的 sprite197..204**（一行 8 格 **32×32**，y=96）的 1:1 切图。
  画布 **256×32@8x1**、8 个成员（`UFO_BURST_0..7`）；**append 在最末尾** ⇒ 新 sprite = **553..560**、
  同批新脚本 = **362 / 363**（`CE_ANM_ABILITY_SCRIPT_UFO_SUMMON_BURST` / `_SPARK`），
  既有号一个没动（entry 78/256，余量 178）。生成器 `assets/ability/make_ufo_summon_burst_art.py`。
  SDK 侧 = `CE_HUD_FONT_TH12_DIGITS`（`native/sdk.{h,c}` 的第 4 档字体，登记见 `AUDIT.md` §BE #91）。

- **★ 2026-09-18 新增 `ATLAS_UFO_HUD`（entry **62**，`CE_ANM_ABILITY_ATLAS_ATLAS_UFO_HUD`）**：
  卡 112 的**飞碟槽 HUD** 贴图（用户 2026-09-18 实跑 ⑤：「**飞碟槽 HUD 你没按照原版做**」）
  = **TH12 `front.anm` 的 10 张 1:1 切图**：令牌槽四态 `front/front00.png` sprite59/60/61/62
  = (336/360/384/408, **336**, 24×16)（逐像素量过：**59 = 纯黑框 = 空槽**，60/61/62 = 红/蓝/绿）、
  四态的**亮版** sprite63..66 = 同 x、**y=352**、24×16（闪烁时换这一组，一手 `th12:0x421690`）、
  以及记录面板 `front/front_ufo.png` sprite102 (0,0,96×96) / sprite103 (96,0,96×96)。
  画布 **256×192 → 512×224**（★ 2026-09-23 第二轮加 **2 个成员**：召唤大横幅 `SUMMON_BANNER`
  = TH12 `front.anm` entry1 `front/front02.png` 的 sprite68 (0,0,**304×32**)、擦除窗 `SUMMON_STRIP`
  = 同 entry 的 sprite69 (0,0,**32×32**)；一手 = `th12:0x44a8a0` 建的 `front.anm` script82/83，
  见 `SINGLE-CARD-TEST.md` §本批-3）、**10 → 12 个成员**（8 个 32×16 槽：两行 y=0 / y=18，
  行间留 2 px 防线性采样竖渗色；2 个 128×128 面板在 y=40；96×16 毛笔横幅在 y=170；
  新两条在 x=192,y=0 与 x=0,y=192 —— 与既有成员零重叠）。
  **append 在最末尾** ⇒ 既有 sprite 号一个没动、新 sprite = **468..469**，
  同批新脚本 = **302..305** + **315** + **324..326**（`UFO_HUD_SLOT` / `_FLASH` / `_PANEL` / `_PANEL_INNER` /
  `UFO_HUD_BANNER` / `UFO_SUMMON_BANNER{,_STRIP,_ECHO}`）。生成器 `assets/ability/make_ufo_hud_art.py`。
  ⚠ **槽的四态在 ORDER 里必须连续、亮版那一组正好 +4**（卡侧 `ce_anm_set_sprite` 直接做加法，
  与一手 `COLLECTED_UFOS[i] + 59 + (闪烁 ? 4 : 0)` 同形）⇒ 生成器里 assert 了 ORDER 顺序、
  `ufo.c` 里另有一条 `_Static_assert` 断死号段。⚠ 面板脚本**暂未接线**（位置与原生卡组图标列冲突）。

- **★ 2026-09-19 新增 `ATLAS_SENRO_HUD`（entry **63**，`CE_ANM_ABILITY_ATLAS_ATLAS_SENRO_HUD`）**：
  卡 107（妖妖梦）的 **HUD 数字字形 + 森罗结界指示**（P8 工单；用户实跑口径见 `SINGLE-CARD-TEST.md`
  §th07 #1/#2）= **TH07 `ascii.anm` entry0 的 11 张 1:1 切图**：数字 `sprite132..141`（各 8×12，
  画数字时 `sprite = 数字 + 0x84`：`th07:0x404a41` / `0x404d7d`）+ **结界中才画**的那块
  `sprite143`（48×16，VM4 = `th07:0x404de1` 判 `ds:0x4bfee5 == 1`）。
  画布 **128×16**、**11 个成员**；**append 在最末尾** ⇒ 新 sprite = **467..477**、
  同批新脚本 = **306 / 307**（`SENRO_HUD_DIGIT` / `SENRO_HUD_BORDER`），既有号一个没动
  （entry 63/256，余量 193）。生成器 `assets/ability/make_senro_hud_art.py`（矩形从 `thanm -l` 断死）。
  ⚠ 数字字形在 ORDER 里**必须连续升序**（卡侧 `ce_anm_set_sprite` 直接做 `D0 + 数字` 的加法）；
  脚本 306/307 里另有 `scale(2.0, 2.0)`（TH07 是 640 原生、TH18 UI 层 1 texel = 1 表面 px 的换算，
  先例 = 302 的 Δ4）。
  ~~原版数字**左边**那块固定标签（sprite142）本轮**不切**~~ ← **2026-09-22 作废**，见下一条。

- **★ 2026-09-22 新增 `ATLAS_SENRO_HUD_LABEL`（entry **64**，`CE_ANM_ABILITY_ATLAS_ATLAS_SENRO_HUD_LABEL`）**：
  同一个生成器、**同一张源图**（TH07 `ascii.anm` entry0）—— 就是上一条里「本轮不切」的那张固定标签
  `sprite142`（96×16）：`CE_ANM_ABILITY_SPRITE_SENRO_HUD_LABEL` = **478**，脚本 **308**。
  画布 **96×16**、**1 个成员**；**为什么另起一段 ATLAS**：`ORDER.txt` 只许追加，
  `ATLAS_SENRO_HUD` 的画布行（128×16 已被 11 个成员填满）不许改 ⇒ 新成员只能新开一段。
  **占的格子 = `ability.anm` 的 entry 索引 64（1 格）、sprite 号 478**，既有 entry / sprite 号一个没动
  （`git diff native/anm_ids.h` = 4 行新增、0 删除）⇒ entry 用量 **65 / 256**（余量 191）。
  一手：`th07:0x401be3` 给 VM2（`AsciiManager +0x498`）装 script4，正文 = `ins_3(sprite142)` + `ins_22()`
  （**左上角锚**）；内容（逐像素读出）= 单词 `Cherry` + 粉色加号 `+`（+C 前缀）+ 粉色斜杠 `/`
  （樱点 / 最大樱点之间的分隔符）+ 软投影。

- **★ 2026-09-23 新增 `ATLAS_MOF_BOMB_RING_BENT`（entry **68**、sprite **486**，
  `CE_ANM_ABILITY_ATLAS_ATLAS_MOF_BOMB_RING_BENT`；⚠ 这两个号是**每次构建现算**的 ——
  本行写下来时它是 entry 70 / sprite 487，中间别的会话又往 ORDER.txt 补了条目就前移了两位。
  脚本里一律用**名字**（`@MOF_BOMB_RING_BENT@`）引用，不受影响）**：
  110/111 灵击柱环的**连续环**贴图（用户口径「**它不是32段**」⇒ 不再用「卡侧摆 32 片竖条」近似一手的
  `texCircle(32)`，改成**构建期把那条 14×128 竖条极坐标弯环、并沿圆周铺 32 份**（= 一手 `texCircle(32)` 的那个 32），
  见 `assets/ability/make_mof_bomb_ring_bent.py`）。
  画布 **512×512**、**1 个成员**（★ 2026-09-24 从 256×256 重烘：**基准半径 100 → 200 px**、厚 14 → 28 px。
  理由见 `TUNING.md` §2.0zzh —— 256 版沿圆周是**高频噪点**（旋转 0.5° 与 60° 的画面差异一样大），
  用户看不出环在转。⇒ 环脚本里 `scale = 半径 / 200`）；
  **append 在最末尾** ⇒ 新 sprite = **486**、同批新脚本 = **327–330**（脚本号原本取 302–305，
  撞上另一张会话的 `302_ufo_hud_slot.anm.txt`，已让位重编；见 `AUDIT.md` §BE 的登记）。
  **占的格子 = `ability.anm` 的 entry 索引 **68**（1 格）、sprite 号 **486****，既有 entry / sprite 号一个没动。
  （本行落笔时的读数 = entry 70 / sprite 487 / 余量 185；现值见 `native/anm_ids.h`。）一手：TH10 `bullet.anm` script394–397 的
  `sprite(sprite362) + texCircle(32)`（竖条 = `etama3.png` 的 (65,0,14,128)，与 `ATLAS_MOF_BOMB_RING` 同源）。
  ⚠ **2026-09-24 本图集本身退役**：柱环改回一手原样（`sprite(竖条切图) + iset($I0,32); iset($I1,3);
  texCircle($I0)`，TH18 引擎原生就在用 texCircle）⇒ 这张 512×512 的烘环**没有脚本引用**了，
  但**行不删**（删了会让后面的 entry/sprite 号前移）。见 `TUNING.md` §2.0zzj。
  ⚠ 另一批退役但**不删**：脚本 279（实心盘占位）与 293（32 片竖条）留在 `assets/ability/scripts/` 里 ——
  脚本号不许回收（删文件会让后面所有脚本号整体前移，撞别的会话的宏）。

- **`abcard.anm` 是卡侧最紧的一道墙**（★最紧）：原生 118 条 + 每张新卡 2 条（大图/小图各一 entry），
  现在 190 条 ⇒ **只能再进 33 张**。它现在有**构建期硬门**（`MAX_ENTRIES`，与 `build_ability.py` 同名同口径）：
  越限直接 `SystemExit`、**先于任何写盘**，并且每次构建都印一行余量
  （`★ entry 余量：用到 190 / 上限 256 ⇒ 还剩 66 个 entry = 还能进 33 张卡`）。
  这一门是 2026-09-14 补的，缘由见 §5-①。
- **`pl0X.anm`**：四份用量 pl00 14 / pl01 15 / pl02 13 / pl03 18（最大 18，余量 238）—— 还非常松。
  ⚠ 它**还没有**自己的 256 门（`build_player_anm.py` 里没有 `MAX_ENTRIES`）：余量 238，不着急，
  但等某一份 pl0X 逼近 256 时要照 `build_abcard.py` 的样子补一个 —— **别只记在台账里**。

### 3.3 shooterset 组数组（**2026-09-14 已从唯一的余量 0 变成 58.3%**）

- **界在哪**：`pl0X.sht` 影像里 `[+0xe0, +0x4e0)` 是数组区（**预留 256 格**）；宣布可用格数不是常量，
  而是**影像头 `+0x02` 的 u16**（现在 **96**）。加载器（`0x45a81d` `mov esi,0xe0`）逐格把
  文件内偏移**就地重定位**成绝对指针（加数 = `影像 + 数据区基点`，`0x45a830`）。
- **零售的样子**：40 格、`[+0xe0,+0x180)`，数据区 `+0x180`；零售用掉 `0x00`–`0x16`，
  空的 `0x17`–`0x27`（17 格）本 mod 在 2026-09-13 全占（豌豆 2 + 116 季节 5 + 117 兽灵 10）⇒ 40/40。
- **落地方案**（[NEXT.md](NEXT.md) §1w、一手取证与审计见 [AUDIT.md](AUDIT.md) §CQ）：
  **数组区原地长到预留格数、数据区跟着后移** `(256-40)×4 = 0x360` 字节 ⇒ 基点 `0x180 → 0x4e0`。
  偏移项是**相对**数据区的 ⇒ 零售 40 项的值与每条记录的字节一个都没改；14 个读者的位移 `+0xe0`
  一个都没改 ⇒ exe 侧只需 **1 条** 6 字节等长 binhack。
  - 这推翻了本节最早「数组搬文件尾」那版计划（要改 **15** 处引用，见 §5-②）：搬数组的风险是
    「漏一个读者 = 静默错位」，而原地长只在**一处常量**上有风险，且那处被三方对账 + 门里回读钉死。
  - **影像容量不是墙**（一手）：影像内存 = `malloc(GetFileSize())` 的堆块（`0x402108` 分配 →
    `0x40213b` 尺寸取自档案索引/散文件），文件长大 N 字节缓冲区就长大 N。**真正的约束**是
    「计数 ≤ 文件里真实存在的格数」——否则加载器会**越界写堆**。
- **架构收益**：基点已让到 `0x4e0` ⇒ `96 → 256` 之间加格子**只改文件**（抬 `CE_SHT_SLOTS` + 写槽位），
  **补丁与 DLL 常量都不用动**。
- **`0x28` 起的新占用（2026-09-15 起，卡 111 落地；★ 2026-09-16zb 扩到 31 格）**：按 [NEXT.md](NEXT.md) §1w 的第 5/6 步，
  **`0x28` 留给 113**（灵界主炮收尾换成永久专属槽）；**111 魔理沙B 现在是 43 组落在 `0x29`–`0x53`**
  = 子机 **35 格**（`0x29`–`0x4b`，`MARISA_OPT_*`）+ 主炮 **8 格**（`0x4c`–`0x53`，`MARISA_MAIN_*`）。
  - ★ **为什么 31 → 43**（2026-09-23，owner 口径「th11 都是老弱病残 ⇒ DPS 对齐 TH18 原生同机体」）：
    子机行的伤害按档缩放后**不再跨档去重**（每档数值都不同）⇒ 组数 +12；`CE_SHT_SLOTS` 96 → **128**
    （数组区仍预留 256、数据区基点 `0x4e0` 不动 ⇒ 只改文件）。见 `TUNING.md` §2.0zt。
  - ★ **为什么从 5 组变 31 组**（2026-09-16zb，用户口径「尽可能完全对齐」+「主炮别换弹幕了」）：
    整套改成 TH11 `pl01b.sht` 的**逐字段复刻**（25 组 = 5 元素 × 5 档）—— 子机的率/角度/伤害**逐格**不同，
    主炮伤害**逐元素逐档**不同（16→8..16）⇒ 去重后 23 + 8 = 31。推导与一手表见
    `assets/sht/append_shooterset.py` 的 `MARISA_*` 段（唯一出处）+ `TUNING.md` §2.1i「六」。
  - `0x28` 在 `append_shooterset.py` 的生成序列里用**洞**占位：**偏移项保持 0**（= 没分配，
    读者拿到 set 0 的别名，幂等），`check_new_slot_placement()` 每次生成都断言「它是洞、且位置对」。
    113 落地时把 `RESERVED_113_SLOT` 从生成序列里去掉、按正常 `APPEND` 追加即可 ⇒ **不用改 exe**。
  - 111 的 31 组排在生成序列**最后**（`sets_for_file()` 的第三段）：槽号 = `N_RETAIL + 序号`，
    插在中间会把 117 的 `WOLF`/`EAGLE` 十格整体顶掉（`animal_core.h` 的槽号写死 ⇒ 静默错弹）——
    脚本里的锚点断言 `WOLF_0 = 0x1e` / `EAGLE_4 = 0x27` 就是为这条钉的。
  - 用量随之从 40 涨到 **71 格**（`0x00`–`0x27` 零售+本 mod 40 格，加 `0x28` 1 格 + `0x29`–`0x47` 31 格）
    ⇒ **计数内还剩 25 格**（`0x48`–`0x5f`）；111 的 31 格全在宣布可用的 96 格内，**没抬 `CE_SHT_SLOTS`**。

### 3.4 卡本地池

- 判据只有一条：**满的时候有没有计数**。有计数 ⇒ ✅（实跑日志看得见）；没有 ⇒ ⚠（满了静默退让，
  实跑里看不出来）。本表按 `capcheck.py` 的 `POOLS` 表自动判（扫源码里有没有那个计数标识）。
- **4 个 ⚠**：`TRANCE_BOSS_SLOTS`（LRU 淘汰）、`SEASON_POINT_POOL`（点直接丢）、
  `LOLK_TINT_MAX`（不染色）、`FAITH_MAX_USERS`（这张卡不计分）。四个都是**我们自己定的界**，
  改常量就行 —— 但先加一个计数（动别人的文件要走 [`AUDIT.md`](AUDIT.md) 的 §BE 登记流程）。
- **改池子常量的硬闸**：各卡文件尾的 `_Static_assert(sizeof(x_card_t) <= 496)`
  （`CE_STATE_BYTES` 512 − `CE_STATE_RESERVED` 16）。池子长大挤的是这一块，编译期就会红。

### 3.5 我们不长的界

- **道具池（`ItemManager`，4696 槽）**：`CE_ITEM_SLOTS` = `0x1258` = **4696**，不是 8192
  （8192 是**商店随机池**；两个数常被写混，这里以 `native/engine.h` L493 为准）。
  原生构造器把 `mgr+0x14` 起 4696 个槽（stride `0xc94`）挂成**双向空链表**，循环上界 `cmp eax,0x1258`（`0x4467fc`）。
  池满的表现是**静默 spawn 失败（返回 0）**，不崩不报错 ⇒ 卡侧的配方是「跨帧分批撒 / 排队补撒」，
  **不要抬这个界**（会连带几十处链表与遍历假设）。
- **每局卡对象数（256）/ 单卡状态字节（496）**：都是「比 id 空间宽」的界，不是当前的瓶颈，
  但它们是**运行期**的，静态推不出来 ⇒ 只在 §2 的自报块与那些 `_Static_assert` 里可见。

## 4. 读者普查（每条「长大」的必做工序）

**规则（一句话）**：对每一条「长大」，**每一个读者都必须「已 patch」或「可证不受影响」**，
两种状态都不占的读者 = 漏改 = 不崩不报错的静默错位。人工找读者一定会漏，
所以第 ① ② 步交给机器，且**可重跑**：`native/tools/reader_census.py`。

方法四步（`reader_census.py` 的文件头有完整交代）：

1. **找锚点**：数组挂在哪个字段上（shooterset 是 `zPlayer+0x47940` 的 sht 影像指针，`CE_PLAYER_SHT_PTR`）；
2. **扫全部引用**：拿到锚点寄存器之后**跟着寄存器**找 `+0xe0` 的 `[base + idx*4 + disp]` 形态
   —— 不能整段线性反汇编（x86 线性扫会跑飞：`0x45ebfb` 的 `mov edi,[edi+edx*4+0xe0]` 就漏了）；
3. **逐个分类**：每个读者写清「它是谁 / 干什么」；
4. **对账**：登记表里**没有的新读者 ⇒ 判红**（这就是「漏一个读者」的那道闸）。

### 实战：shooterset 组数组的 `+0xe0`

```bash
cd mods/th18.v1.00a/card-expand
python3 native/tools/reader_census.py          # 普查 + 与登记表对账
python3 native/tools/reader_census.py --all    # 连锚点的 31 处引用一起打出来
```

（没有样本 `local/th18.v1.00a/th18.exe` 或没装 `capstone` 时**自动跳过并 exit 0** —— 新 clone 上不能因为缺样本就挂。）

结果：**15 处引用 = 14 个读者 + 1 个加载器**（2026-09-14 订正：原先是 13 + 1，见下表最后一行）。

| 地址 | 是什么 | 在 §1w 的 binhack 清单里？（2026-09-14 起 = 全部 14 处）|
| --- | --- | --- |
| `0x40a9f7` | 装备卡子机路径取组（`Player__tick_shooters_for_ability_card`）| ✅ 有 |
| `0x40c335` | 卡侧按 set 取行（索引来自运行期对象字段 `arg+0xac`）| ✅ 有 |
| `0x40c464` | 卡侧按 set 取行（同上，另一条支路）| ✅ 有 |
| `0x411047` | 当前主炮组 | ✅ 有 |
| `0x45ebfb` | `Player__tick_shooting_state` 主炮取组（★ 已挂 `BP_ce_shot_set_index`）| ✅ 有 |
| `0x45e347` | `PlayerBullet__create` `0x45e320` 序言取行 | ✅ 有（2026-09-14 补进）|
| `0x45e94d` | 主炮开火路径取行（`0x45e930` 一带）| ✅ 有（2026-09-14 补进）|
| `0x45ee16` | 开火路径取行（`0x45ede0` 一带）| ✅ 有（2026-09-14 补进）|
| `0x45fcbe` | 命中/伤害路径取行（`0x45fca0` 一带）| ✅ 有（2026-09-14 补进）|
| `0x46035b` | 取组（`0x460300` 一带；索引 = 打包的 `set<<8\|row`）| ✅ 有（2026-09-14 补进）|
| `0x4609f4` | 取组（`0x4609c0` 一带）| ✅ 有（2026-09-14 补进）|
| `0x460ba8` | 取组（`0x460b60` 一带）| ✅ 有（2026-09-14 补进）|
| `0x460cdd` | 取组（`0x460cb0` 一带）| ✅ 有（2026-09-14 补进）|
| `0x45a81d` | **加载器**：`mov esi,0xe0` + 就地重定位（循环上界 = 影像头 `+0x02`）| ✅ 有 |
| ★ `0x410609` | **折叠位移读者**：`mov edi,[eax+0x138]`（`eax` = sht 影像，`0x138 = 0xe0 + 0x16*4` = set `0x16`）；随后是 `cmp byte [edi],0`/`jl` + `lea edi,[edi+0x5c]` 行游标，`call 0x45e930` | ✅ 有（2026-09-14 补进；**本方案下无需改指**）|

**这就是当时的问题**：上表里那 8 个读者**原来不在 §1w 的计划里**（§1w 最早只列了 6 处：
加载器 + 5 个读者）。字节级普查把 14 处挖出来，`NEXT.md` §1w 已按本表改正；
**2026-09-14 又补出第 15 处**（`0x410609` 的折叠位移形态）⇒ 现在 **15 / 15 都在清单里**。
好消息是「数组搬到文件尾、读者全部改指」这个形态**依然成立**：这 13 处的位移/立即数
**全是 32 位**（`…e0000000` / `bee0000000`）⇒ 等长替换、无相对寻址。

**会话教训（★ 通用，不只 shooterset）**：**「加载时会就地重定位」这条理由，不能豁免任何一个读者被改指。**
重定位搬的是**指针的值**；读者**取指针的位置**（`sht + 0xe0 + set*4` 那个位移）仍然写死在每条指令里。
把这两件事混起来，就会得出「读者不用改」的结论 —— 而 §1w 最早那版 6 个站点的清单**正是这么来的**：
它把「读者拿到绝对指针」误当成了「读者不需要知道数组在哪」。所以规则写成一句：

> **凡是有位移/立即数落在被搬走的数组区间里的指令，就是读者，必须出现在清单里 —— 除非能证明它读的不是这条数组。**

这条规则的机器实现就是 `reader_census.py`（可重跑），清单就是上面这张表。

**本条普查的推论**：扩容落地时必须把这 14 处**逐条**改指新数组；
`0x45ebfb` 那条还额外挂着断点 `BP_ce_shot_set_index`（`native/sites.py` 的 7 字节期望 + `native/sdk.c` 的
`CE_BP_SHOT_SET_INDEX_RVA`），改指令时**断点表要同步**。
**权威清单 = 上面这张 14 行表**；[NEXT.md](NEXT.md) 的 §1w 已按它改正（2026-09-14），
两边**必须一起改**（`reader_census.py` 的 `READERS` 登记表是第三份副本，改了也要同步）。

## 5. 已知的洞（按危险度排）

### ① `abcard.anm` 的 256 门 —— **2026-09-14 已补上**（原 ❌）

**原来的病**：`build_abcard.py` 只检查「cards.js 的贴图索引」与「`--verify-only` 时 entry 条数 == `118 + 2×N`」，
**没有「entry 总数 ≤ 256」这道门**。而 `ability.anm` 那边一直有（`build_ability.py` 的 `MAX_ENTRIES`）。
于是越过 256 之后的表现是**贴图回绕**（贴图键低 8 位截断）：不崩、不报错，
只在实跑里看到「卡图错位」—— 正是 `AUDIT.md` §CC 那个失败模式，也正是本台账存在的理由
（**能做成的错，一定会在实跑里做成**）。

**现在的样子**（`assets/build_abcard.py`）：

1. `MAX_ENTRIES = 256`，与 `build_ability.py` 同名同口径，注释里写明它的一手出处（`0x486d38` / `0x47e1a4`）；
2. 越限 ⇒ `SystemExit`，**先于任何写盘**（既不产出坏 anm，也不去改 `cards.js`），错误信息给两条改法
   （并 entry / 换 anm 槽）；
3. 每次构建印一行余量：`★ entry 余量：用到 190 / 上限 256 ⇒ 还剩 66 个 entry = 还能进 33 张卡`；
4. **`capcheck.py` 的上限不再是自己写的 256**，而是**从生成器源码读 `MAX_ENTRIES`**
   （`gen_anm_caps()`）—— 门加严/被删/被放宽到 > 256，都会让 `make check` 立刻红，
   台账没法跟门各自漂移。

⇒ 这条容量从「❌ 已知会撞」变成「✅ 构建期就拦」。但**墙还是这条墙**：33 张的余量是卡侧最短的，
加满时的配方见 §1 表与 §3.2。

### ② §1w 的站点数少了一半以上 → **该方案已被更好的方案取代（✅ 2026-09-14）**

**当时的发现**：`NEXT.md` §1w 说「6 条 binhack」，字节级普查是 **15 处引用**（14 读者 + 1 加载器）。
§1w 的理由「加载之后所有读者拿到的都是绝对指针，再没人需要知道数组在哪」**只对了一半**：
读者拿到的是**指针值**，但他们**取指针的位置**（`sht+0xe0+set*4`）仍然写死在指令里 —— 数组一搬，
`+0xe0` 处就是数据区字节 ⇒ 这 14 个读者全都要改指。

**两次订正**：① §1w 最早只列 6 处 → 按普查补到 14 处（2026-09-14）；② 后来又发现**折叠位移**
形态（`0x410609 mov edi,[eax+0x138]`，`0x138 = 0xe0+0x16*4`）⇒ **15 处**。
`reader_census.py` 已把该形态纳入扫描，并把旧的 `PATCH_PLANNED`（§1w 计划镜像）换成两道新闸：
**闸 A**（读者/加载器的位移不许被 patch 改指）+ **闸 B**（数据区常量 `0x180` 在全 `.text` 只许
有一处与 sht 相关）。

**落地时改成了另一条路**（见 §3.3 与 `AUDIT.md` §CQ）：**数组区原地长到预留格数、数据区基点后移**，
只改 **1 处** 常量 ⇒ 15 处引用**一个都不用改**。所以「计划清单漏项」这件事对本实现已不构成风险，
但那 15 处仍全部登记在 §4 的表里 —— **将来真要搬数组时，起点就是那张表**（外加折叠位移这一课）。

**断点那一课仍然成立**：`0x45ebfb` 挂着 thcrap 断点 `BP_ce_shot_set_index`，而 thcrap 的顺序是
**binhacks → breakpoints**；断点把原指令拷进 source cave 回放 ⇒ 谁要改这条读者指令，
必须同时确认「cave 里回放的是改过的版本」，断点表里的 7 字节期望（对的是**磁盘原版 exe**）
**不要跟着改**（改了会让 `sites.py check/gen/verify` 报 ShapeError）。

### ③ 4 个卡本地池满了静默（⚠）

`TRANCE_BOSS_SLOTS` / `SEASON_POINT_POOL` / `LOLK_TINT_MAX` / `FAITH_MAX_USERS`。
它们是「我们自己定的界」，撞顶的表现都是**悄悄退让**（LRU / 丢 / 不染 / 不计分），
实跑里看不出来 ⇒ 建议各加一个计数（动别人的卡要先按 [`AUDIT.md`](AUDIT.md) §BE 登记）。

### ④ 文案只有一半普查过（🟡）

商店保证位 `CE_SHOP_OFFER_SLOTS` = 57：**新卡侧**每次装载都现算（`cards:` 自报行 `guaranteed offers <= 17/57`），
**零售那一半**没有普查 ⇒ 只报增量、不判余量。想彻底关掉这条 🟡 就普查 55 张零售卡的
`weight == 0` 与 `dmode ∈ 1..5` 的个数。

## 附录 A —— 有界消费者清单（机器生成）

<!-- capcheck:appendix:begin（本段由 `native/tools/capcheck.py --gen-appendix` 生成，**别手改**）-->

**A0. 卡的安全用量：还能进 33 张**（瓶颈 = `abcard.anm` entry，每张 2 格；口径 = 本文件 `CARD_COST`，见 `CAPACITY.md` §1 的「安全用量」）

**A1. 卡 id（`CE_CARD(` 的落点，37 张）** —— 出处 `native/cards/*.c`

```
58 59 60 61 62 63 64 65 66 67 68 69 70 71 72 73 74 75 76 77 78 79 80 106 107 108 109 110 111 112 113 114 115 116 117 118 200
```

**A2. shooterset 组（`native/sht_ids.h`，50 个宏）**

```
CE_SHT_SET_YOUKAI 0x0016
CE_SHT_SET_PEA_NORMAL 0x0017
CE_SHT_SET_PEA_GATLING 0x0018
CE_SHT_SET_SEASON_SPRING 0x0019
CE_SHT_SET_SEASON_SUMMER 0x001a
CE_SHT_SET_SEASON_FALL 0x001b
CE_SHT_SET_SEASON_WINTER 0x001c
CE_SHT_SET_SEASON_DOYOU 0x001d
CE_SHT_SET_WOLF_0 0x001e
CE_SHT_SET_WOLF_1 0x001f
CE_SHT_SET_WOLF_2 0x0020
CE_SHT_SET_WOLF_3 0x0021
CE_SHT_SET_WOLF_4 0x0022
CE_SHT_SET_EAGLE_0 0x0023
CE_SHT_SET_EAGLE_1 0x0024
CE_SHT_SET_EAGLE_2 0x0025
CE_SHT_SET_EAGLE_3 0x0026
CE_SHT_SET_EAGLE_4 0x0027
CE_SHT_SET_TRANCE 0x0028
CE_SHT_SET_MARISA_OPT_00 0x0029
CE_SHT_SET_MARISA_OPT_01 0x002a
CE_SHT_SET_MARISA_OPT_02 0x002b
CE_SHT_SET_MARISA_OPT_03 0x002c
CE_SHT_SET_MARISA_OPT_04 0x002d
CE_SHT_SET_MARISA_OPT_05 0x002e
CE_SHT_SET_MARISA_OPT_06 0x002f
CE_SHT_SET_MARISA_OPT_07 0x0030
CE_SHT_SET_MARISA_OPT_08 0x0031
CE_SHT_SET_MARISA_OPT_09 0x0032
CE_SHT_SET_MARISA_OPT_10 0x0033
CE_SHT_SET_MARISA_OPT_11 0x0034
CE_SHT_SET_MARISA_OPT_12 0x0035
CE_SHT_SET_MARISA_OPT_13 0x0036
CE_SHT_SET_MARISA_OPT_14 0x0037
CE_SHT_SET_MARISA_OPT_15 0x0038
CE_SHT_SET_MARISA_OPT_16 0x0039
CE_SHT_SET_MARISA_OPT_17 0x003a
CE_SHT_SET_MARISA_OPT_18 0x003b
CE_SHT_SET_MARISA_OPT_19 0x003c
CE_SHT_SET_MARISA_OPT_20 0x003d
CE_SHT_SET_MARISA_OPT_21 0x003e
CE_SHT_SET_MARISA_OPT_22 0x003f
CE_SHT_SET_MARISA_OPT_23 0x0040
CE_SHT_SET_MARISA_OPT_24 0x0041
CE_SHT_SET_MARISA_OPT_25 0x0042
CE_SHT_SET_MARISA_OPT_26 0x0043
CE_SHT_SET_MARISA_OPT_27 0x0044
CE_SHT_SET_MARISA_OPT_28 0x0045
CE_SHT_SET_MARISA_OPT_29 0x0046
CE_SHT_SET_MARISA_OPT_30 0x0047
CE_SHT_SET_MARISA_OPT_31 0x0048
CE_SHT_SET_MARISA_OPT_32 0x0049
CE_SHT_SET_MARISA_OPT_33 0x004a
CE_SHT_SET_MARISA_OPT_34 0x004b
CE_SHT_SET_MARISA_MAIN_00 0x004c
CE_SHT_SET_MARISA_MAIN_01 0x004d
CE_SHT_SET_MARISA_MAIN_02 0x004e
CE_SHT_SET_MARISA_MAIN_03 0x004f
CE_SHT_SET_MARISA_MAIN_04 0x0050
CE_SHT_SET_MARISA_MAIN_05 0x0051
CE_SHT_SET_MARISA_MAIN_06 0x0052
CE_SHT_SET_MARISA_MAIN_07 0x0053
```

**A3. 图集 entry（`CE_ANM_ABILITY_ATLAS_*`，32 个）** —— 每个 = `ability.anm` 一个 entry

```
CE_ANM_ABILITY_ATLAS_ATLAS_ANIMAL_64
CE_ANM_ABILITY_ATLAS_ATLAS_ANIMAL_BEAM
CE_ANM_ABILITY_ATLAS_ATLAS_ANIMAL_FX
CE_ANM_ABILITY_ATLAS_ATLAS_ANIMAL_SMALL
CE_ANM_ABILITY_ATLAS_ATLAS_ANIMAL_TOKEN
CE_ANM_ABILITY_ATLAS_ATLAS_FAITH_HUD
CE_ANM_ABILITY_ATLAS_ATLAS_FLAN
CE_ANM_ABILITY_ATLAS_ATLAS_FONT2
CE_ANM_ABILITY_ATLAS_ATLAS_FONT2B
CE_ANM_ABILITY_ATLAS_ATLAS_HUD_DIGIT
CE_ANM_ABILITY_ATLAS_ATLAS_LOLK
CE_ANM_ABILITY_ATLAS_ATLAS_MARISA_BULLET
CE_ANM_ABILITY_ATLAS_ATLAS_MARISA_ELEM
CE_ANM_ABILITY_ATLAS_ATLAS_MARISA_ELEM_BULLET
CE_ANM_ABILITY_ATLAS_ATLAS_MOF_BOMB
CE_ANM_ABILITY_ATLAS_ATLAS_MOF_BOMB_RING
CE_ANM_ABILITY_ATLAS_ATLAS_MOF_FAITH
CE_ANM_ABILITY_ATLAS_ATLAS_NITORI_BUBBLE
CE_ANM_ABILITY_ATLAS_ATLAS_RETIRED
CE_ANM_ABILITY_ATLAS_ATLAS_SEASON_BULLET
CE_ANM_ABILITY_ATLAS_ATLAS_SEASON_UI
CE_ANM_ABILITY_ATLAS_ATLAS_SENRO
CE_ANM_ABILITY_ATLAS_ATLAS_SENRO_HUD
CE_ANM_ABILITY_ATLAS_ATLAS_SIGNAL
CE_ANM_ABILITY_ATLAS_ATLAS_TH13ASCII
CE_ANM_ABILITY_ATLAS_ATLAS_TRANCE_ASTRAL
CE_ANM_ABILITY_ATLAS_ATLAS_TRANCE_GAUGE
CE_ANM_ABILITY_ATLAS_ATLAS_TRANCE_POPUP
CE_ANM_ABILITY_ATLAS_ATLAS_TRANCE_SPIRIT
CE_ANM_ABILITY_ATLAS_ATLAS_TRANCE_SWITCH
CE_ANM_ABILITY_ATLAS_ATLAS_UFO_BODY
CE_ANM_ABILITY_ATLAS_ATLAS_UFO_DROP
CE_ANM_ABILITY_ATLAS_ATLAS_UFO_HUD
CE_ANM_ABILITY_ATLAS_ATLAS_UFO_TOKEN
CE_ANM_ABILITY_ATLAS_ATLAS_YK_FAM
CE_ANM_ABILITY_ATLAS_ATLAS_YK_OPT
```

**A4. 追加脚本（`CE_ANM_ABILITY_SCRIPT_*`，228 个）**

```
CE_ANM_ABILITY_SCRIPT_ANIMAL_EAGLE
CE_ANM_ABILITY_SCRIPT_ANIMAL_OTTER
CE_ANM_ABILITY_SCRIPT_ANIMAL_WOLF
CE_ANM_ABILITY_SCRIPT_AURA_EAGLE
CE_ANM_ABILITY_SCRIPT_AURA_OTTER
CE_ANM_ABILITY_SCRIPT_AURA_RAINBOW
CE_ANM_ABILITY_SCRIPT_AURA_WOLF
CE_ANM_ABILITY_SCRIPT_BEASTAP
CE_ANM_ABILITY_SCRIPT_BEASTAP_A
CE_ANM_ABILITY_SCRIPT_BEASTAP_B
CE_ANM_ABILITY_SCRIPT_BLUE_EYES_BAR
CE_ANM_ABILITY_SCRIPT_BLUE_EYES_BAR_BG
CE_ANM_ABILITY_SCRIPT_BLUE_EYES_BEAM
CE_ANM_ABILITY_SCRIPT_BLUE_EYES_BEAM_CORE
CE_ANM_ABILITY_SCRIPT_BLUE_EYES_BEAM_FLARE
CE_ANM_ABILITY_SCRIPT_BLUE_EYES_BEAM_LAYER_A
CE_ANM_ABILITY_SCRIPT_BLUE_EYES_BEAM_LAYER_B
CE_ANM_ABILITY_SCRIPT_BLUE_EYES_BEAM_LAYER_C
CE_ANM_ABILITY_SCRIPT_BLUE_EYES_BEAM_LAYER_D
CE_ANM_ABILITY_SCRIPT_BLUE_EYES_DRAGON
CE_ANM_ABILITY_SCRIPT_BOMB_CLEAR_RING
CE_ANM_ABILITY_SCRIPT_BROKEN_CORE_BEAM
CE_ANM_ABILITY_SCRIPT_BROKEN_CORE_ORB
CE_ANM_ABILITY_SCRIPT_BROKEN_CORE_SPARK
CE_ANM_ABILITY_SCRIPT_EAGLE_BEAM
CE_ANM_ABILITY_SCRIPT_FAITH_DOT
CE_ANM_ABILITY_SCRIPT_FAITH_HUD_MUL
CE_ANM_ABILITY_SCRIPT_FAITH_HUD_MULT
CE_ANM_ABILITY_SCRIPT_FIRELORD_AURA
CE_ANM_ABILITY_SCRIPT_FIRELORD_BLAST
CE_ANM_ABILITY_SCRIPT_FIRELORD_BODY
CE_ANM_ABILITY_SCRIPT_FIRELORD_EMBER
CE_ANM_ABILITY_SCRIPT_FIRELORD_FIREBALL
CE_ANM_ABILITY_SCRIPT_FIRELORD_GATHER
CE_ANM_ABILITY_SCRIPT_FIRELORD_PARTICLE
CE_ANM_ABILITY_SCRIPT_FIRELORD_TRAIL
CE_ANM_ABILITY_SCRIPT_FISH_BOMB
CE_ANM_ABILITY_SCRIPT_FISH_LIFE
CE_ANM_ABILITY_SCRIPT_FISH_POINT
CE_ANM_ABILITY_SCRIPT_FISH_POWER
CE_ANM_ABILITY_SCRIPT_FLAN_CHARGE_RING
CE_ANM_ABILITY_SCRIPT_FLAN_CLEAR_BURST
CE_ANM_ABILITY_SCRIPT_GAUGE_FRAME
CE_ANM_ABILITY_SCRIPT_GAUGE_ICON
CE_ANM_ABILITY_SCRIPT_HUD_DIGIT
CE_ANM_ABILITY_SCRIPT_HUD_GLYPH
CE_ANM_ABILITY_SCRIPT_HUD_NUMBER
CE_ANM_ABILITY_SCRIPT_HYPER_BAR_FILL
CE_ANM_ABILITY_SCRIPT_HYPER_BAR_FRAME
CE_ANM_ABILITY_SCRIPT_JUDGMENT_FLASH
CE_ANM_ABILITY_SCRIPT_LOLK_PANEL1
CE_ANM_ABILITY_SCRIPT_LOLK_PANEL2
CE_ANM_ABILITY_SCRIPT_LOLK_PANEL3
CE_ANM_ABILITY_SCRIPT_LOLK_PANEL4
CE_ANM_ABILITY_SCRIPT_LOLK_PANEL5
CE_ANM_ABILITY_SCRIPT_LOLK_PANEL6
CE_ANM_ABILITY_SCRIPT_LOLK_PANEL7
CE_ANM_ABILITY_SCRIPT_LOLK_RING
CE_ANM_ABILITY_SCRIPT_MARISA_BULLET
CE_ANM_ABILITY_SCRIPT_MARISA_ELEM_EARTH_BULLET
CE_ANM_ABILITY_SCRIPT_MARISA_ELEM_EARTH_OPT
CE_ANM_ABILITY_SCRIPT_MARISA_ELEM_FIRE_BULLET
CE_ANM_ABILITY_SCRIPT_MARISA_ELEM_FIRE_OPT
CE_ANM_ABILITY_SCRIPT_MARISA_ELEM_METAL_BULLET
CE_ANM_ABILITY_SCRIPT_MARISA_ELEM_METAL_OPT
CE_ANM_ABILITY_SCRIPT_MARISA_ELEM_WATER_BULLET
CE_ANM_ABILITY_SCRIPT_MARISA_ELEM_WATER_OPT
CE_ANM_ABILITY_SCRIPT_MARISA_ELEM_WOOD_BULLET
CE_ANM_ABILITY_SCRIPT_MARISA_ELEM_WOOD_OPT
CE_ANM_ABILITY_SCRIPT_MOF_BOMB_RING_STRIP
CE_ANM_ABILITY_SCRIPT_MOF_BOMB_SPARK
CE_ANM_ABILITY_SCRIPT_MOF_BOMB_SPARK_BLUE
CE_ANM_ABILITY_SCRIPT_MOF_CONV_BURST
CE_ANM_ABILITY_SCRIPT_MOF_FAITH_DIGIT
CE_ANM_ABILITY_SCRIPT_MOF_FAITH_DOT
CE_ANM_ABILITY_SCRIPT_MOF_FAITH_DOT_MICRO
CE_ANM_ABILITY_SCRIPT_MOF_FAITH_ICON
CE_ANM_ABILITY_SCRIPT_NITORI_BUBBLE
CE_ANM_ABILITY_SCRIPT_OTTER_ORB
CE_ANM_ABILITY_SCRIPT_PEA
CE_ANM_ABILITY_SCRIPT_PEASHOOTER_OPT
CE_ANM_ABILITY_SCRIPT_PEA_BURST
CE_ANM_ABILITY_SCRIPT_PEA_SMALL
CE_ANM_ABILITY_SCRIPT_POPUP_LIFE1
CE_ANM_ABILITY_SCRIPT_POPUP_LIFE2
CE_ANM_ABILITY_SCRIPT_POPUP_POINT10
CE_ANM_ABILITY_SCRIPT_POPUP_POINT100
CE_ANM_ABILITY_SCRIPT_POPUP_SPELL1
CE_ANM_ABILITY_SCRIPT_POPUP_SPELL2
CE_ANM_ABILITY_SCRIPT_REVERSE_FLASH
CE_ANM_ABILITY_SCRIPT_ROYAL_CARD_10
CE_ANM_ABILITY_SCRIPT_ROYAL_CARD_A
CE_ANM_ABILITY_SCRIPT_ROYAL_CARD_J
CE_ANM_ABILITY_SCRIPT_ROYAL_CARD_K
CE_ANM_ABILITY_SCRIPT_ROYAL_CARD_Q
CE_ANM_ABILITY_SCRIPT_ROYAL_FLUSH
CE_ANM_ABILITY_SCRIPT_ROYAL_GOLD
CE_ANM_ABILITY_SCRIPT_ROYAL_SHOW
CE_ANM_ABILITY_SCRIPT_SAKURA_POINT
CE_ANM_ABILITY_SCRIPT_SAKURA_SMALL
CE_ANM_ABILITY_SCRIPT_SEASON_BAR_FILL
CE_ANM_ABILITY_SCRIPT_SEASON_BAR_FRAME
CE_ANM_ABILITY_SCRIPT_SEASON_BULLET_DOYOU
CE_ANM_ABILITY_SCRIPT_SEASON_BULLET_FALL
CE_ANM_ABILITY_SCRIPT_SEASON_BULLET_SPRING
CE_ANM_ABILITY_SCRIPT_SEASON_BULLET_SUMMER
CE_ANM_ABILITY_SCRIPT_SEASON_GAUGE_FRAME
CE_ANM_ABILITY_SCRIPT_SEASON_GAUGE_GLYPH
CE_ANM_ABILITY_SCRIPT_SEASON_GAUGE_GRAD
CE_ANM_ABILITY_SCRIPT_SEASON_GAUGE_SOLID
CE_ANM_ABILITY_SCRIPT_SEASON_GAUGE_STRIP
CE_ANM_ABILITY_SCRIPT_SEASON_GLYPH
CE_ANM_ABILITY_SCRIPT_SEASON_LASER
CE_ANM_ABILITY_SCRIPT_SEASON_LASER_BEAM
CE_ANM_ABILITY_SCRIPT_SEASON_LASER_CHILD
CE_ANM_ABILITY_SCRIPT_SEASON_LASER_HIT
CE_ANM_ABILITY_SCRIPT_SEASON_OPT
CE_ANM_ABILITY_SCRIPT_SEASON_POINT
CE_ANM_ABILITY_SCRIPT_SEASON_RING_IN
CE_ANM_ABILITY_SCRIPT_SEASON_RING_IN12
CE_ANM_ABILITY_SCRIPT_SEASON_RING_OUT
CE_ANM_ABILITY_SCRIPT_SEASON_SEED
CE_ANM_ABILITY_SCRIPT_SELF_AURA
CE_ANM_ABILITY_SCRIPT_SENRO_FIELD
CE_ANM_ABILITY_SCRIPT_SENRO_RING
CE_ANM_ABILITY_SCRIPT_SIGNAL_GAUGE_BG
CE_ANM_ABILITY_SCRIPT_SIGNAL_GAUGE_ICON
CE_ANM_ABILITY_SCRIPT_SIGNAL_GAUGE_RETICLE
CE_ANM_ABILITY_SCRIPT_SLOT_FLAME
CE_ANM_ABILITY_SCRIPT_SLOT_SOUL
CE_ANM_ABILITY_SCRIPT_SPIRIT_PLAIN
CE_ANM_ABILITY_SCRIPT_SPIRIT_PLAIN1
CE_ANM_ABILITY_SCRIPT_SPIRIT_PLAIN1_CORE
CE_ANM_ABILITY_SCRIPT_SPIRIT_PLAIN1_RING
CE_ANM_ABILITY_SCRIPT_SPIRIT_PLAIN2
CE_ANM_ABILITY_SCRIPT_SPIRIT_PLAIN2_CORE
CE_ANM_ABILITY_SCRIPT_SPIRIT_PLAIN2_RING
CE_ANM_ABILITY_SCRIPT_SPIRIT_PLAIN3
CE_ANM_ABILITY_SCRIPT_SPIRIT_PLAIN3_CORE
CE_ANM_ABILITY_SCRIPT_SPIRIT_PLAIN3_RING
CE_ANM_ABILITY_SCRIPT_SPIRIT_PLAIN_CORE
CE_ANM_ABILITY_SCRIPT_SPIRIT_PLAIN_RING
CE_ANM_ABILITY_SCRIPT_SPIRIT_TRANCE
CE_ANM_ABILITY_SCRIPT_SPIRIT_TRANCE1
CE_ANM_ABILITY_SCRIPT_SPIRIT_TRANCE1_GLOW
CE_ANM_ABILITY_SCRIPT_SPIRIT_TRANCE1_RING
CE_ANM_ABILITY_SCRIPT_SPIRIT_TRANCE2
CE_ANM_ABILITY_SCRIPT_SPIRIT_TRANCE2_GLOW
CE_ANM_ABILITY_SCRIPT_SPIRIT_TRANCE2_RING
CE_ANM_ABILITY_SCRIPT_SPIRIT_TRANCE3
CE_ANM_ABILITY_SCRIPT_SPIRIT_TRANCE3_GLOW
CE_ANM_ABILITY_SCRIPT_SPIRIT_TRANCE3_RING
CE_ANM_ABILITY_SCRIPT_SPIRIT_TRANCE_GLOW
CE_ANM_ABILITY_SCRIPT_SPIRIT_TRANCE_RING
CE_ANM_ABILITY_SCRIPT_STOCK_RECT
CE_ANM_ABILITY_SCRIPT_STRIKE
CE_ANM_ABILITY_SCRIPT_STRIKE_GREEN
CE_ANM_ABILITY_SCRIPT_STRIKE_PURPLE
CE_ANM_ABILITY_SCRIPT_STRIKE_RED
CE_ANM_ABILITY_SCRIPT_TH13_BIG_GLYPH
CE_ANM_ABILITY_SCRIPT_TPOINT
CE_ANM_ABILITY_SCRIPT_TRANCE_ALPHA_ZERO
CE_ANM_ABILITY_SCRIPT_TRANCE_ASTRAL
CE_ANM_ABILITY_SCRIPT_TRANCE_GAUGE
CE_ANM_ABILITY_SCRIPT_TRANCE_GAUGE_FILL
CE_ANM_ABILITY_SCRIPT_TRANCE_GAUGE_FRAME
CE_ANM_ABILITY_SCRIPT_TRANCE_GAUGE_FRAME_B
CE_ANM_ABILITY_SCRIPT_TRANCE_GAUGE_MARK
CE_ANM_ABILITY_SCRIPT_TRANCE_MASK
CE_ANM_ABILITY_SCRIPT_TRANCE_TIERUP1
CE_ANM_ABILITY_SCRIPT_TRANCE_TIERUP2
CE_ANM_ABILITY_SCRIPT_TRANCE_TIERUP_MAX
CE_ANM_ABILITY_SCRIPT_TRANCE_WIPE1
CE_ANM_ABILITY_SCRIPT_TRANCE_WIPE2
CE_ANM_ABILITY_SCRIPT_TRANCE_WIPE3
CE_ANM_ABILITY_SCRIPT_TRANCE_WIPE4
CE_ANM_ABILITY_SCRIPT_TRANCE_WIPE_COVER
CE_ANM_ABILITY_SCRIPT_TRANCE_WIPE_IN1
CE_ANM_ABILITY_SCRIPT_TRANCE_WIPE_IN2
CE_ANM_ABILITY_SCRIPT_TRANCE_WIPE_IN3
CE_ANM_ABILITY_SCRIPT_TRANCE_WIPE_IN4
CE_ANM_ABILITY_SCRIPT_UFO_BLUE
CE_ANM_ABILITY_SCRIPT_UFO_GAUGE_ARC
CE_ANM_ABILITY_SCRIPT_UFO_GAUGE_ARC_BORDER_IN
CE_ANM_ABILITY_SCRIPT_UFO_GAUGE_ARC_BORDER_OUT
CE_ANM_ABILITY_SCRIPT_UFO_GAUGE_BAR
CE_ANM_ABILITY_SCRIPT_UFO_GAUGE_BAR_SHADOW
CE_ANM_ABILITY_SCRIPT_UFO_GREEN
CE_ANM_ABILITY_SCRIPT_UFO_PENTAGON
CE_ANM_ABILITY_SCRIPT_UFO_RAINBOW
CE_ANM_ABILITY_SCRIPT_UFO_RED
CE_ANM_ABILITY_SCRIPT_UFO_TOKEN_BLUE
CE_ANM_ABILITY_SCRIPT_UFO_TOKEN_GREEN
CE_ANM_ABILITY_SCRIPT_UFO_TOKEN_RAINBOW_BLUE
CE_ANM_ABILITY_SCRIPT_UFO_TOKEN_RAINBOW_GREEN
CE_ANM_ABILITY_SCRIPT_UFO_TOKEN_RAINBOW_RED
CE_ANM_ABILITY_SCRIPT_UFO_TOKEN_RBBLUE
CE_ANM_ABILITY_SCRIPT_UFO_TOKEN_RBGREEN
CE_ANM_ABILITY_SCRIPT_UFO_TOKEN_RBRED
CE_ANM_ABILITY_SCRIPT_UFO_TOKEN_RED
CE_ANM_ABILITY_SCRIPT_YOUKAI_ALICE_S5
CE_ANM_ABILITY_SCRIPT_YOUKAI_ALICE_S6
CE_ANM_ABILITY_SCRIPT_YOUKAI_ALICE_S7
CE_ANM_ABILITY_SCRIPT_YOUKAI_ALICE_S8
CE_ANM_ABILITY_SCRIPT_YOUKAI_ALICE_S9
CE_ANM_ABILITY_SCRIPT_YOUKAI_AYA_S0
CE_ANM_ABILITY_SCRIPT_YOUKAI_AYA_S1
CE_ANM_ABILITY_SCRIPT_YOUKAI_AYA_S2
CE_ANM_ABILITY_SCRIPT_YOUKAI_AYA_S3
CE_ANM_ABILITY_SCRIPT_YOUKAI_AYA_S4
CE_ANM_ABILITY_SCRIPT_YOUKAI_FAMILIAR_S53
CE_ANM_ABILITY_SCRIPT_YOUKAI_FAMILIAR_S54
CE_ANM_ABILITY_SCRIPT_YOUKAI_FAMILIAR_S55
CE_ANM_ABILITY_SCRIPT_YOUKAI_FAMILIAR_S56
CE_ANM_ABILITY_SCRIPT_YOUKAI_OPT_ALICE
CE_ANM_ABILITY_SCRIPT_YOUKAI_OPT_AYA
CE_ANM_ABILITY_SCRIPT_YOUKAI_OPT_REMILIA
CE_ANM_ABILITY_SCRIPT_YOUKAI_OPT_YUKARI
CE_ANM_ABILITY_SCRIPT_YOUKAI_REMILIA_S5
CE_ANM_ABILITY_SCRIPT_YOUKAI_REMILIA_S6
CE_ANM_ABILITY_SCRIPT_YOUKAI_REMILIA_S7
CE_ANM_ABILITY_SCRIPT_YOUKAI_REMILIA_S8
CE_ANM_ABILITY_SCRIPT_YOUKAI_REMILIA_S9
CE_ANM_ABILITY_SCRIPT_YOUKAI_YUKARI_S5
CE_ANM_ABILITY_SCRIPT_YOUKAI_YUKARI_S6
CE_ANM_ABILITY_SCRIPT_YOUKAI_YUKARI_S7
CE_ANM_ABILITY_SCRIPT_YOUKAI_YUKARI_S8
CE_ANM_ABILITY_SCRIPT_YOUKAI_YUKARI_S9
```

**A5. sprite 汇总（`CE_ANM_ABILITY_SPRITE_*`）** —— sprite 号是 dword、**不是容量**，所以这里只登记总数与号段（加一张美术这两行就变 ⇒ 台账必须跟着刷新）

```
CE_ANM_ABILITY_SPRITE 宏 721 个，号 109..452
```

<!-- capcheck:appendix:end -->












- **★ 2026-09-24 新增 `ATLAS_UFO_DEBRIS`（entry **70**，`CE_ANM_ABILITY_ATLAS_ATLAS_UFO_DEBRIS`）**：
  卡 112 的**召唤闪电碎屑**贴图（owner 2026-09-24：「召唤动画的**闪电**…拿了一个**蓝色的会有残影的像素，
  随机地让它向外运动**画出来的」+「中间是洋红色」）= **TH18 自己 `bullet.anm` 的 etbreak 家族素材**
  （`local/th18.v1.00a/anm/bullet/effect/etbreak.png`，256×128）里 **`sprite540..547` 八张 64×64 的 1:1 切图**
  —— 这一族是**本作引擎自己在敌人击破时**用的那套（脚本 `script160..313`），所以是"借全套"，不是自绘。
  成员 **8 个**（`DEBRIS_0..7`，矩形各 `64x64`）、画布 **256×128**（4×2 格刚好）；
  **append 在最末尾** ⇒ 新 sprite = **503..510**、同批新脚本 = **346..350**
  （`UFO_SUMMON_DEBRIS` 驱动器 + `..._DEBRIS_{DIM,GLOW}` 蓝 + `..._CORE_{DIM,GLOW}` 洋红），
  既有号一个没动（entry **70/256**，余量 **185**；`ability.anm` 合计 entry 71、sprite 511、script 351）。
  生成器 `assets/ability/make_ufo_debris_art.py`（矩形从原生 anm 文本读 + 逐张断言 64×64）；
  层号 5/6 与"向外飞"那一段的理由见 `TUNING.md` §2.0zzl。

- **★ 2026-09-24 新增 `ATLAS_UFO_CARRIER_BFLY` / `_FAIRY` / `_MARK`（entry **71 / 72 / 73**）；
  宏名 **`CE_ANM_ABILITY_ATLAS_ATLAS_UFO_CARRIER_BFLY`**、**`CE_ANM_ABILITY_ATLAS_ATLAS_UFO_CARRIER_FAIRY`**、**`CE_ANM_ABILITY_ATLAS_ATLAS_UFO_CARRIER_MARK`****：
  卡 112 的**令牌宿主皮肤**（owner 2026-09-24：「掉飞碟的大蝴蝶的贴图是 `enemy.png` 的第一二行；
  掉飞碟的杂鱼妖精是 `enemy.png` 妖精贴图的第 4 行；并且她们头上会有 `enemy.png` 第二行的右半部分的
  旋转飞碟特效……**得改谁能飞碟掉落，并且还得给掉的那个改帖图**」）—— 全部是 TH12 `enemy.anm`
  （`enemy/enemy.png`）的 1:1 切图：

  | 图集 | 画布 | 成员 | 一手 | 用途 |
  | --- | --- | --- | --- | --- |
  | `ATLAS_UFO_CARRIER_BFLY` | 512×64 | `CARRIER_BFLY_0..7`（各 64×64）| `sprite125..132` | 大蝴蝶走位（脚本 351，用前 4 张 = 一手 script45）|
  | `ATLAS_UFO_CARRIER_FAIRY` | 128×32 | `CARRIER_FAIRY_0..3`（各 32×32）| `sprite84..87`（= 妖精贴图第 4 行）| 杂鱼妖精走位（脚本 352 = 一手 script35）|
  | `ATLAS_UFO_CARRIER_MARK` | 384×32 | `CARRIER_MARK_0..5`（各 64×32）| `sprite167..172`（= 第二行右半）| 头顶旋转飞碟（脚本 353 = 一手 script98；灰影子 354/355 = 一手 script100/101）|

  **append 在最末尾** ⇒ 新 sprite = **511..529**、同批新脚本 = **351..355**；既有号一个没动
  （entry **71/72/73**，合计 74/256、余量 **182**）。切图 = `assets/ability/make_ufo_carrier_art.py`。
  宿主表（哪些敌人换皮 / 谁掉令牌）= `native/cards/ufo_carrier_table.h`，生成器
  `native/tools/gen_ufo_carrier_table.py`；一手依据见 `TUNING.md` §2.0zzm。
