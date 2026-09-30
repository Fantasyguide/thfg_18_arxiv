/* 妖妖梦（TH07 系统卡，card-expand id 107）—— 樱点 / 森罗结界的 TH18 卡侧接线。
 *
 * 纯逻辑在 `cards/senro_core.{h,c}`（主机单测 `tests/test_senro_core.c`，**无引擎依赖、无浮点**）；
 * 本文件只做**引擎面**：消弹调用、无敌声明、道具 / 得分事件、HUD、生命周期清理。
 *
 * 一手依据（th07 侧的逐条地址与逐指令结论全在 `cards/senro_core.h` 头注，这里不重复）：
 *   · 机制记录（用户口径）：`mods/th18.v1.00a/card-expand/IDEAS.md` §「107 妖妖梦」（约 513 行起）
 *   · 体检结论 + 用户裁决：`docs/superpowers/specs/2026-09-11-system-card-spec-review.md` §9（含 §9.5.1 的
 *     VA→offset 勘误、§9.7 森罗结界机制、§9.7.1 断点核对）
 *   · 一手研究（**最新，冲突时以它为准，但本卡逐指令复核了两处，见 senro_core.h 头注的订正 ①②**）：
 *     `local/_scratch/research-107-th07-remaining.md`；反汇编全文 `local/_scratch/th07.asm`
 *   · 多卡同装：`mods/th18.v1.00a/card-expand/EVENTS-CONTRACT.md`（§4 玩家保护层、§5.2 救援层定案）
 *   · SDK 面：`mods/th18.v1.00a/card-expand/SDK.md` §6 / §6.1（事件与声明层）
 *   · 本卡 spec：`docs/superpowers/specs/2026-09-12-senro-107-design.md`（数值表 / 偏差表 / 未定项）
 *
 * ★ 地址写法：本文件的**裸地址 = TH18 v1.00a**（`th18.exe`，imagebase `0x400000`）；
 *   引 th07 一律带前缀 `th07:0x…`。
 *
 * ── 这张卡怎么玩（一句话）─────────────────────────────────────────────────────────────
 * 收道具 / 结界中擦弹 / 自机弹命中 → **樱点**与 **+C** 一起涨；**+C 到 50000 自动开森罗结界**（7 s）。
 * 结界中：**每帧全屏收点**、**擦弹同时涨樱点与最大樱点**、+C 按剩余计时比例**持续下降**。
 * **中弹或按 B ⇒ 灵击**：结界中断 + **全屏弹转原生小分道具**（= TH07 的 ★，每颗收下 +20 樱点/+C）
 * + **0.67 s 无敌**（40 帧）。
 * **自然到期** ⇒ 樱点与最大樱点各 **+10000** + **樱点分**的 Border Bonus。**Miss / Bomb 扣樱点**（不动 +C、不动最大樱点）。
 * 樱点是本卡的「最大得点」：吃一颗点道具（TH18 金钱）的得分 = 樱点。
 * ★ 2026-09-13 计分层重做后，本卡在 `on_item_score` 里只**声明**这份 `G_07`（未耦合通道），
 *   由 SDK 一次合成（`ce_score_apply`；公式与一手见 `../SCORE-LAYER-REWORK.md`）。
 *
 * ── 长持续效果的自查（`METHOD.md` 铁律二 / `SDK.md` §6.1 尾）────────────────────────────
 * 本卡跨帧留下的东西：① 无敌声明层的一格（`ce_invuln_request`）；② 计分声明层的一格（`ce_score_declare_delta`）；
 * ③ HUD 槽位 + **17 台数字 VM + 1 台结界指示花 VM + 1 台固定标签 VM**（2026-09-19 P8 工单：原版版式；
 *   2026-09-22 补标签 —— 用户口径「107的UI按照原贴图做」，见 `SENRO_HUD_*` 长注）；
 * ④ **★ 结界特效的两个世界层 VM**（甲 = 那个环的句柄 + 乙 = 32 颗的句柄，
 * 2026-09-12 补，见文件中部长注）。①② 由 `CE_CARD` 的 `on_run_reset` / `opdelete` 自动 `ce_layer_drop_card()` 清，
 * ③ 的槽位也自动 release，但**这 18 台 VM 是本卡自己的** ⇒ `on_delete` / `on_run_reset` / `on_stage_start`
 * 与非游玩帧都要走 `senro_hud_clear()`（不收 = 永久钉在屏幕上）。
 * ④ 同理（`ce_layer_drop_card` 只清声明层）⇒ `senro_clear()` 里的 `senro_fx_clear()` 把 33 个句柄全 `ce_anm_delete`。
 * ⑤ ★★ 2026-09-18 新增：**共享点池里属于本卡的樱点实体**（`1 << FAITH_SLOT_107`）——
 *   `senro_clear()` 里 `faith_unregister(SENRO_ID)` + `faith_clear_entities_mask(…)`（顺序是硬的）。
 * ⚠ 删卡发生在**商店里**（倒吊人 / 原版空白卡清空整副牌），那一刻 AbilityManager 冻着、卡的 tick 全停
 * ⇒ 只能靠 `on_delete`（虚表 `+0x50`），不能等下一帧；`on_stage_start` 也收一次（特效不跨关）。
 * ⚠ 甲的环与乙的 32 颗都是**脚本自己 `delete()` 的一次性特效**（第 33 / 第 30 帧），但句柄仍必须记着：
 * 删除卡时要能提前掐掉，起不来时只报日志不崩（`ce_anm_set_pos` 返回 0 ⇒ 句柄自愈）。
 */
#include <stddef.h>          /* sdk.h 的 ce_item_collect_all 用了 size_t（内置 Linux 头不带）*/
#include <string.h>
#include <stdio.h>

#include "sdk.h"
#include "senro_core.h"
#include "faith.h"       /* ★ 2026-09-16：`faith_collect_all()`（结界全收也要收自建绿点）*/
#include "hud_dim.h"           /* ★ 跨卡统一件：左下角 HUD 压暗（❓ 用户口径，见文件头）*/

#define SENRO_ID                    107

/* ============================ 引擎面：本卡自己补的裸地址 / 常量 ============================
 * 0x4ccd0c  原生擦弹计数（`GLOBALS.inner+0x30`）。engine.h 没有提炼这个地址，先例 = `cards/lolk.c:33`、
 *           `cards/season.c:70`、`cards/mof.c:54`（三处同址）⇒ 本卡就地补一份，值一样。
 * 0x45d590  `Player__cancel_impending_death` —— thiscall(玩家)，取消本次死亡。
 *           先例 = `cards/animal.c:243`（117 咆哮「中弹不扣命」的同一调用）。本卡用它实现
 *           「结界中弹 = 消耗结界救命」（EVENTS-CONTRACT §4.2 的**救援层**）。
 */
#define SENRO_ADDR_GRAZE            0x4ccd0c
#define SENRO_FN_CANCEL_IMPENDING_DEATH 0x45d590
/* 0x4ccd00  难度（0 EASY .. 3 LUNATIC；`GlobalsInner` 首字段区，**只读**）。
 *           先例 = `cards/mof.c:52` 的 `MOF_ADDR_DIFFICULTY`、`cards/th06rank.c` 同址。
 *           本卡只用它查 th07 的最大樱点难度表（`th07:0x42eb7a`–`0x42ec57`）。*/
#define SENRO_ADDR_DIFFICULTY       0x4ccd00

/* X（炸弹）键的上升沿位：`CE_ADDR_INPUT_PRESSED` 0x4ca434 bit 0x002（先例 `cards/animal.c:1830`、`cards/mof.c:412`）。
 * ⚠ 只**读**、绝不写输入位（回放安全：这份掩码就是 replay 录的那一份，review §4.4 / §6.7）。*/
#define SENRO_INPUT_BOMB            0x002u

/* 灵击的全屏消弹：半径 / 上限 / 模式。半径 4000 = 全屏量级（`sdk.h` §6.1 的示例值、`cards/mof.c:179` 同款）。*/
#define SENRO_CANCEL_R              4000.0f
#define SENRO_CANCEL_MAX            9999
#define SENRO_CANCEL_MODE           1        /* 1 = 每颗被消的弹撒一颗 **type 9 小分道具**（`Bullet__cancel`
                                              * 0x428e90 的 mode；一手真值表 = `engine/bullet/th18/01-cancel-to-item.md` §2.1：
                                              * `0x4291a5 push 0x9`、角度 = rand×10° − π/2、速度 2.2）
                                              *
                                              * ★★ 2026-09-18 从 **0（纯删除）改成 1**，依据是**用户口径**（逐字）：
                                              *   「**本作有消弹小点…消弹小点得兼用星星**」——
                                              *   即 TH07 的 ★（`th07` type 6）在我们侧**不另建实体**，
                                              *   直接**兼用 TH18 原生的消弹小分道具**（type 9–0xe 那一族，
                                              *   在 `senro_item_cherry()` 里本来就是本卡的「★ 通道」：收一颗 +20 樱点/+C 各 +20）。
                                              *
                                              * ⇒ 旧注释（原样留档，**已作废**）：「选 0 不选 1 的理由：mode 1 会让引擎撒出
                                              *   type 9–0xe 的点道具，而那一族在本卡是「★ 通道」⇒ 与小樱点的 +30/+70 双计」
                                              *   —— 那条成立的前提是**本卡自己再按颗记一遍小樱点**；现在那一笔已经撤掉
                                              *   （见 `senro_do_libyou`），所以不再是双计，而正是「兼用」。
                                              *
                                              * ⚠ 两个已知副作用（登记在 `TUNING.md` §2.0k / 本卡设计件 `SENRO-107-CHERRY-ENTITY.md`）：
                                              *   ① 每颗弹推 1 次 `REPLAY_SAFE_RNG`（一手 §2.4）⇒ 装卡后老 replay 有必然 desync 点；
                                              *   ② 场地门（一手 §2.3：`|x| ≥ 224`、`y ≤ −32`、`y ≥ 480` 的弹零产出）
                                              *       ⇒ 「消了 n 颗」≠「拿到 n 颗 ★」。*/

/* ★★ 2026-09-18（用户口径：加）：TH07 的**小樱点**（type 8，樱点 +100 / +C +30）来源 ——
 *   TH07 那一档**只**来自 boss 死亡 / 符卡结束序列里那颗 `mode 0xa` 的消弹
 *   （一手 `th07:0x420131`，前置门 = `enemy[+0x2e2a] >> 6 & 1` + 状态机计数 `ds:0x12fe0c8`）。
 *   TH18 在同一位置走 `etClearAll`（子弹侧 **mode 0**：一颗实体都不撒，但**计数器照加**，
 *   见 `engine/bullet/th18/01-cancel-to-item.md` §1.4/§2.1）⇒ 判据 =「**符卡刚结束**（下降沿）
 *   + 本帧有一大笔**无人认领**的消弹」（无人认领 = 不是任何卡自己的灵击/炸弹消的）。
 *   ⚠ 位置：TH07 是「每颗弹在自己原位变成一颗小樱点」，TH18 在这一刻**弹已经没了**（引擎先清）
 *     ⇒ 位置取**自机为中心的一圈**（❓ 自定 —— 唯一与一手不同的地方；**值**与**总数**照一手给足）。
 *   ⚠ 颗数封顶 `SENRO_SMALL_MAX_SPAWN`：几百颗会撞共享池小档上限（256）且观感糊
 *     ⇒ 每颗带**打包值**（总值不变）。*/
#define SENRO_SMALL_TRIGGER_MIN   8      /* 本帧消弹 ≥ 这么多颗才算「boss 死亡那一大笔」（❓自定）*/
#define SENRO_SMALL_MAX_SPAWN     24     /* 一次最多撒几颗实体（❓自定；总值按一手给足）*/

/* ★★ 2026-09-18：**死敌掉 ★** 用的原生道具 type 与撒落实参。
 *   一手（TH07 §3.2，`th07:0x423144`）：死亡批处理对每只「已死且未结算」的敌机
 *   `spawn_item(enemy.pos, 6, 1)` **1 颗 ★**（樱点 / +C 各 +20）—— 撒在**敌人原位**、**无抖动**。
 *   本作的 ★ 已定为「兼用原生小分道具」（见 `SENRO_CANCEL_MODE` 的注）⇒ 用同一族 type 9，
 *   角 −π/2（`0xbfc90fdb`）/ 速 2.2（`0x400ccccd`）= 本作消弹撒点的一手实参
 *   （`engine/bullet/th18/01-cancel-to-item.md` §2.1）。*/
#define SENRO_TH18_ITEM_STAR        SENRO_TH18_ITEM_SCORE_LO   /* 9 = 分数转换点族（★ 的载体）*/
#define SENRO_ITEM_SPAWN_ANGLE      (-1.5707964f)              /* = 0xbfc90fdb（−π/2，直上）*/
#define SENRO_ITEM_SPAWN_SPEED      2.2f                       /* = 0x400ccccd */

/* ★★★ 2026-09-20（P1「缺音效一批」th07 #2/#3）：TH07 的 id → wav 表已逆出（直读 `local/th07.v1.00b/th07.exe`：
 *   名表 `th07:0x49ebb8` 30 项、SE 配置表 `th07:0x49ea88` 行距 8 / 38 行（`+0x00` = 名表下标、`+0x04` = i16 音量）、
 *   唯一播放入口 `th07:0x44c930`）⇒ 下面三条按 **wav 名**对回 TH18，不再是「借一个像的」。
 *   · **结界展开**：一手**有两枚**（同一支连播）—— `th07:0x441b91 push 0x20` ⇒ TH07 `0x20` = **`se_border.wav`**
 *     （**TH07 独有**，TH18 71 枚里没有、也无替身 ⇒ 走扩表 `CE_VOICE_PCB_BORDER` = `0x5c`，构建期从
 *     `local/th07.v1.00b/dat/se_border.wav` 逐字节现取，见 `assets/voice/derive_retail_se.py`）
 *     ＋ `th07:0x441b9f push 0x24` ⇒ TH07 `0x24` = `se_bonus2.wav` ⇒ TH18 同一枚 = **`0x30`**（md5 相同）。
 *     ⚠ **旧注释「开结界那一支没找到播音调用」是错的** —— 那两条就在 `th07:0x441960` 的尾段。
 *   · **结界自然结束（540 帧到期结算）**：`th07:0x4416fd push 0x21` ⇒ TH07 `0x21` = `se_bonus.wav`
 *     ⇒ TH18 `0x2f` 同名同字节 ⇒ **原来就是对的**（旧注释里的「❓ 自定」作废）。
 *   · **灵击 / 中弹时结界顶掉那一支**（同一个函数 `th07:0x441bd0`，`ret 4` 但从不读参数）：
 *     `th07:0x441e43 push 7` ⇒ TH07 `0x07` = `se_tan00.wav`，`th07:0x441e51 push 0x21` ⇒ `se_bonus.wav`。
 *     TH18 有三格 `se_tan00`（`0x15` −19/50、`0x18` −3/20、`0x1b` −11/50），**没有 TH07 那格 −15/50**
 *     ⇒ 取 prio 相同的两格里较响的 **`0x1b`**（差 4 dB，登记 `TUNING.md` §4.1 #4；要更贴就换 `0x15`）。*/
#define SENRO_SE_BORDER_OPEN        CE_VOICE_PCB_BORDER /* ✅ 一手 `th07:0x441b91` → `se_border.wav`（0x5c）*/
#define SENRO_SE_BORDER_OPEN2       0x30     /* ✅ 一手 `th07:0x441b9f` → `se_bonus2.wav`（TH18 `0x30` 同 md5）*/
#define SENRO_SE_BORDER_END         0x2f     /* ✅ 一手 `th07:0x4416fd` → `se_bonus.wav`（TH18 `0x2f` 同 md5）*/
#define SENRO_SE_LIBYOU             0x1b     /* ✅ 一手 `th07:0x441e43` → `se_tan00.wav`（TH18 取 `0x1b`）*/
#define SENRO_SE_LIBYOU2            0x2f     /* ✅ 一手 `th07:0x441e51` → `se_bonus.wav`（同帧第二声）*/

/* 震屏：th07 的打断在 `th07:0x441d35`/`0x441dd9` 起 32 个环形特效单元（没有全屏震动的一手证据）
 * ⇒ 参数是**表现占位**（与 `cards/mof.c:183` 的灵击同款）。TODO(th07)*/
#define SENRO_SHAKE_TIME            20
#define SENRO_SHAKE_START           12
#define SENRO_SHAKE_END             0

/* ==================== ★★★ 2026-09-30：结界「持续期」的三件原版视觉 ====================
 *
 * owner 实跑（逐字）：「森罗结界只能看到进入结界的那一瞬，之后就看不见了」／
 * 「进入森罗结界缺少文字的那个提示」／「森罗结界似乎缺少特效」。
 * 一手全部在 `engine/spirit/th07/01-barrier-visual.md`（§2–§5 逐条给了地址），三件分别是：
 *
 *   ① **全屏压暗**（TH07 `th07:0x44158d`–`0x44162f` + 绘制侧 `0x442299`/`0x406b52`）：
 *      ⛔ **2026-09-30 实跑被 owner 否掉，默认关**（`SENRO_DIM_ENABLE 0`）—— owner 原话：
 *      「**森罗结界整个画面暗下来了，肯定不太对**」。回头核一手：这条压暗的**触发值**
 *      `ds:0x4d44e0`（= g 曲线的自变量）在 TH07 全 exe **找不到写点**（8 个读点、0 个写点，
 *      `engine/spirit/th07/01-barrier-visual.md` §4.3 当时就标 🟡）⇒ 它很可能**恒 0**
 *      ⇒ `g = 0x80 − 0 = 0x80` = 乘子 1.0 = **原版根本不压暗**（与 TH18 那条同类机制「休眠」互相印证）。
 *      ⇒ 本轮的结论：**这段视觉在原版实际不生效**，我们照它做反而错。机制留档（脚本 369 + 下面的代码），
 *      要重新试就把开关改成 1（一手曲线：`x<30 → 0x80 − x·80/30`、`x≥510 → 0x80 − (540−x)·80/30`、
 *      **其余恒 0x30** ⇒ 稳态乘子 48/128 = 0.375）。
 *      载体说明（若重开）：TH18 同机制（`AnmManager+0x39724b0` + 使能位 `+0x39724b4`）的使能位
 *      每帧在**绘制相位**被清两次（优先级 1 的 `0x4554da`/`0x4554e4` + `Stage::on_draw_03` 的 `0x41c68d`），
 *      而本 mod 的站点全在 update 相位 ⇒ 够不到；改用**数学等价**的全屏黑底矩形 + alpha
 *      （脚本 369，层 19），`dst·(1 − α/255)`、`α = 255 − 255·g/128`（稳态 **159** = 37.6 %）。
 *      层 19 = 游玩区最上层 ⇒ 自机也被压暗；**不用 24/25**（UI 列表）—— 那会把 HUD 一起压暗。
 *   ② **自机贴图每 2 帧红/白闪**（`th07:0x441553`–`0x44158a` tick 侧、`0x44225c`–`0x442296`
 *      draw 侧同码）：`(timer.value % 4) < 2` ⇒ `player+0x1b8 = 0xffff0000`（红），否则
 *      `0xffffffff`（白）—— `player+0x1b8` 就是自机**内嵌 AnmVm 的颜色**。覆盖整段 540 帧。
 *      TH18 对应字段 = `zPlayer.vm`（`player+0x14`）的 `color_1`/`color_2`
 *      （`+0x524`/`+0x528` ⇒ 绝对 `player+0x538`/`+0x53c`；引擎自己的无敌闪色写的就是 color_2，
 *      一手 `th18:0x45c6b6` 等）。**相位**：引擎在 Player tick（优先级 0x17）里写体色，所以
 *      我们必须在它**之后**、Player draw（0x1d）**之前**写 ⇒ 挂 `on_item_frame`
 *      （`BP_ce_item_frame` `0x446ee3`，优先级 0x1e）。照抄 `cards/youkai.c` 的同一相位用法。
 *   ③ **进入结界的文字 `Supernatural Border!!`**（`th07:0x441b7a` push kind=2 → `0x427c81`；
 *      字串 `th07:0x4983c8`；颜色 `ds:0x13542d8 = 0xffe0b0ff` @ `0x428432`）：
 *      · 位置：TH07 640×480 空间、**左上角锚**，`y = 168`（常量 `0x498b30`）恒定；
 *        `x` 从 **416**（`0x498a74`）用 **30 帧**滑到 **104**（`0x498bb8`）= 每帧 −10.4
 *        （`0x42b278`–`0x42b2b2`）。
 *      · 寿命 **180 帧**（`0x42b2cc` 的 `cmp [..+0x209fc],0xb4`），**无淡出**、无缩放/上浮；
 *        公告互斥：新公告直接顶掉旧的（我们只有这一条）。
 *      · 字形 = TH07 `data/ascii.anm`（16×16 字格，ASCII 码 − 1 索引），步进 = 14×0.9 = **12.6 单位**。
 *      · TH18 落点：`ce_hud_text_ex`（`sdk.h`，**只能在 `+0x48 on_draw` 里调**）—— 它背后就是
 *        TH18 的 AsciiManager 字符串队列（TH07 那条的直系）。font **0** 是唯一含小写与 `!` 的字形集
 *        （`sprite(char − 0x20)`，`engine/anm/th18/03-ascii-text.md` §2）；group **0** 的绘制趟跑在
 *        卡的 on_draw 之后（§2d）。**scale**：TH18 font 0 的步进 = `9 × scale` ⇒ `12.6/9 = 1.4`
 *        （整串宽度与 TH07 相同；❓ 待实跑微调，登记 `TUNING.md` §2.0u）。
 *        ⚠ TH07 的 `y=168` 是**字形上沿**，而 TH18 的 ASCII 垂直锚默认就是「顶」（`+0x134 = 1`）
 *        ⇒ **直接写 168**（若实跑发现偏高/偏低，只需调 `SENRO_TEXT_Y` 一个数）。*/
#define SENRO_DIM_ENABLE            1       /* ★ 2026-09-30 第三轮：**重新打开**（owner 逐字：「好像确实压暗，
                                             *   但是压暗的层数是**只压暗背景**，目前是包括敌人和子弹也压暗了」）
                                             *   ⇒ 开回来，并把载体从「游玩区最上层」挪到**背景组最上层（层 5）**，
                                             *   见 `SENRO_DIM_LAYER`。第二轮那句「整个画面暗下来不太对」由此收口：
                                             *   原版确实压暗，只是**只压背景**。*/
#define SENRO_DIM_RAMP              30      /* 两头各 30 帧的斜坡（一手 0x44158d 段的 30/510/540）*/
#define SENRO_DIM_LAYER             5       /* ★★ **只压背景**：TH18 的层 3/4/5 = **背景组**（绘制桩优先级
                                             *   0xa/0xb/0xd，画进 surface 0），随后优先级 **0xf** 才把这片
                                             *   「游戏区域」拷进游玩区面；**敌机/子弹/自机在层 6–19**（0x10 起，
                                             *   画在拷贝之后）⇒ 层 5 的遮罩正好只盖背景、不盖它们（层表见
                                             *   `engine/anm/th18/02-render-stages.md` §1）。
                                             *   ⚠ 旧值 19（= 游玩区最上层）会把敌人与子弹一起压暗 = owner 第三轮
                                             *   报的现象；层 24/25 更不行（会连 HUD 一起压暗）。*/
#define SENRO_DIM_G_FULL            0x80    /* g 的满值 = 128（乘子 1.0）*/
#define SENRO_DIM_G_HOLD            0x30    /* 稳态 g = 48（0x30）⇒ 乘子 0.375 */
#define SENRO_TEXT_LIFE             180     /* 一手 0x42b2cc 的 0xb4 */
#define SENRO_TEXT_SLIDE            30      /* 一手 0x498b00（滑入帧数）*/
#define SENRO_TEXT_Y                168.0f  /* 一手 0x498b30 */
#define SENRO_TEXT_X_START          416.0f  /* 一手 0x498a74 */
#define SENRO_TEXT_X_END            104.0f  /* 一手 0x498bb8 */
#define SENRO_TEXT_COLOR            0xffe0b0ffu  /* 一手 ds:0x13542d8（kind 2 = 展开文字 / kind 4 同色）*/
#define SENRO_TEXT_COLOR_MAXED      0xffc0b0ffu  /* 一手 0x428480（kind 3 = `CherryPoint Max!`）*/
#define SENRO_TEXT_SCALE            1.4f    /* = 12.6 / 9（TH07 步进 ÷ TH18 font0 步进）；❓ 待实跑微调 */
#define SENRO_TEXT_STR              "Supernatural Border!!"
/* 公告 kind（TH07 是**单槽互斥**：新公告直接顶掉旧的，`th07:0x427c81` 每次都重置 pos/计时器）。
 *   kind 2 = 展开（`th07:0x441b7a`）· kind 3 = `CherryPoint Max!`（`0x42f696`/`0x42f72d`）
 *   · kind 4 = `Border Bonus %7d`（`0x4416dd`）。三者共用同一套位置/滑入/寿命（`0x42b278`–`0x42b2cc`）。*/
#define SENRO_MSG_NONE              0
#define SENRO_MSG_BORDER            1
#define SENRO_MSG_BONUS             2
#define SENRO_MSG_MAXED             3
#define SENRO_FLASH_RED             0xffff0000u  /* 一手 th07:0x441574（`% 4 < 2` 取红）*/
#define SENRO_FLASH_WHITE           0xffffffffu

/* ==================== ★★ HUD：**原版版式**（2026-09-19，P8 工单）====================
 *
 * 老实现是「左下角三行 ASCII 文字」（`+N` / 樱点 / 最大樱点，行距 12），那是**自定**的版式 ——
 * 与 TH07 原作差得远（用户实跑口径见 `SINGLE-CARD-TEST.md` §th07 #1「HUD 样式错误」）。
 * 现在的版式**逐条复刻** `AsciiManager::sub_404690`（TH07 画樱点计数器的那个函数，见
 * `engine/spirit/th07/02-cherry-counter-hud.md`）：
 *
 *   ① **两行**，不是一个数字一行：
 *        · 上：**+C（樱点 Plus）** 5 位定宽字段，锚 `T + (53, 2)`（`th07:0x404bc4`–`0x404bfa`）
 *        · 下：**樱点** 6 位定宽字段 `T + (46, 11)`（`th07:0x404918`–`0x404945`），
 *              紧跟 **+9 单位**（`th07:0x404ac8`，`ds:0x498c2c`）是 **最大樱点** 6 位字段
 *              （值 ≥ 1000000 时 **7 位**：`th07:0x404ae0` 那一支）
 *   ② **定宽右对齐 + 前导零不画**：每一位都 `x += 7`（`th07:0x404a7a`，`ds:0x498c30`），
 *      **含被跳过的前导零位**（`th07:0x404a32` 的 `started` 闸）⇒ 字段宽固定、数字靠右。
 *   ③ 字形 = TH07 原版 8×12 数字贴图（`ascii.anm` sprite132..141，`sprite = 数字 + 0x84`：
 *      `th07:0x404a41` / `th07:0x404d7d`）⇒ 本卡 `assets/ability/senro_hud/D0..D9.png`
 *      （原生切图，`make_senro_hud_art.py`）+ VM 脚本 306。
 *   ④ **森罗结界指示**（本卡原来**完全没有**）—— 结界中（`ds:0x4bfee5 == 1`）三件：
 *        (a) +C 行**变色**：`R = 0xFF` 固定、`G = B = plus×0xc0/50000 + 呼吸`（`th07:0x404c27`–`0x404ca2`）
 *        (b) +C 行**放大 1.41 倍**（`0x3fb47ae1`，`th07:0x404cab`/`0x404cb8`）、字距 **7 → 10**
 *            （`th07:0x404ccc` 的 `[ebp-0x30]` = 0xa），整体 **+2 / −2**（`th07:0x404ccc`/`0x404cea`）
 *        (c) 数字行上盖一块**脉动花**（TH07 的 VM4 = `ascii.anm` sprite143 = **两朵樱花**，脚本 **307** =
 *            TH07 script5 的逐条移植：30 帧缩到 1.2 → 30 帧缩到 0.8 → 跳回，**无限**），
 *            位置 = `T + (24, 8)`（`th07:0x404df0`–`0x404e36`），只在结界中画（`th07:0x404de1`）
 *      ⚠ 另两件「结界中」的全局视觉（全屏压暗 37.5 %、自机红白闪）**不是 HUD 元件**，本卡不做
 *        （一手与理由见 `engine/spirit/th07/01-barrier-visual.md` §4.2/§4.3）。
 *   ⑤ ★★ **数字左边那块固定标签**（2026-09-22 P8 收口：**照原贴图画**，owner 口径「107的UI按照原贴图做」）
 *      —— 就是 VM2 自己那张 **`ascii.anm` sprite142（96×16）**，脚本 **308**（TH07 script4 的稳态移植）。
 *      内容（本模型不能看图 ⇒ 用 alpha/luminance 字符画**逐像素读出来**的，✅ 一手）——
 *      **三件东西 + 一层软投影**，正好对上本卡的三个字段：
 *        · 单词 **`Cherry`**（sprite 内 `x1..36, y5..13`）= 樱点字段的左标签（`Cherry` 右侧那串数字）
 *        · 粉色加号 **`+`**（`x41..45, y1..5`）= **+C 字段的前缀**（+C 首字框 = `T.x+49` ⇒ 加号在它左 4 px）
 *        · 粉色斜杠 **`/`**（`x86..90, y6..13`）= **樱点 / 最大樱点之间的分隔符**
 *          （樱点末字右沿 `T.x+85`、最大字段首字左沿 `T.x+93` ⇒ 斜杠落在 8 px 空档正中，中心 88.5 vs 89）
 *        · 软投影（`x1..90, y3..15`）：数字压在上面仍读得清（贴图自带）
 *      交叉印证：THBWiki 对这块的描述「**Cherry** 右侧的分子 = 樱点、分母 = 樱点最大值」逐条对上。
 *      ⚠ 这三条对位**只有把数字按「居中锚 + 代码里那些死偏移」摆才同时成立** ⇒ 它同时**反证**了
 *        §锚点 那一节的读法（这也就是 306/307 一行都不用改的原因）。
 *   ⑥ ★★ **锚点（2026-09-22 落到 opcode 级，✅）**—— TH07 那三台 VM 的锚点**不一样**：
 *        · VM2（标签 / script4）**左上角**：script4 正文第 3 句 = `ins_22()`；opcode 22 的实现
 *          `th07:0x451b51` = `mov eax,[vm+0x1c0]; or eax,0xc00; mov [vm+0x1c0],eax`（置 flags 位 10/11），
 *          而 `AnmVm::Draw` `th07:0x44f770` 正是用这两位选锚（位 10 ⇒ 左 = pos.x，`0x44f849`；
 *          位 11 ⇒ 上 = pos.y，`0x44f8d9`）⇒ 本卡脚本 308 **显式** `anchor(1, 1)`。
 *        · VM3（数字 / script3）与 VM4（脉动花 / script5）**居中**：两支脚本都**没有** `ins_22`，
 *          而 `AnmLoaded::load_script_into_vm` `th07:0x44ea20` 装脚本时会把它复位
 *          （`AnmVm::reset` `th07:0x4010f0` 把 `[vm+0x1c0]` 写成 0x7 ⇒ 位 10/11 = 0）。
 *          **恰好等于 TH18 的精灵默认锚 = 锚心**（实跑证据 = `AUDIT.md` §AL20 的火焰/图标）
 *          ⇒ 306 / 307 **不写 `anchor`** 就是对的（写了也一样；不写 = 少一处将来的偏离）。
 *
 * 坐标：全部以「原版的锚 **T**」为单位表达（就是 TH07 里标签 VM2 的 pos，**原版 = (32,449)**）。
 *   ★★ 2026-09-30（owner 第 2 条）之后，本卡的 `T` **逐值落在原版那一格**：
 *   块钉在 (68,928) 表面 = 640 空间的 (34,464)，`T_OFF_X/Y = 0/−15` ⇒ **T = (34,449)**
 *   （`T.y` = 原版值本身；`T.x` 比原版 32 多 2 单位 = 4 表面 px —— 见 `SENRO_HUD_PIN_X` 的取舍）。
 *
 * ★★ 整块计数器的**真实包围盒**（2026-09-22 算出来，用于把🟡「7 位越块 2 px」收口）——
 *   一律 640 字面量单位（1 单位 = 2 表面 px），锚 T 起算；数字按**居中锚**算（见 ⑥）：
 *
 *   | 元素 | x 区间（单位）| y 区间（单位）| 一手来源 |
 *   | --- | --- | --- | --- |
 *   | 标签 sprite142（96×16，左上角锚）| `T.x .. T.x+96` | `T.y .. T.y+16` | script4 + `0x44f849`/`0x44f8d9` |
 *   | +C 字段（5 位×7，居中）| `T.x+49 .. T.x+89` | `T.y−4 .. T.y+8` | 首字 `T.x+53`（`0x404bd9`）、字高 12 |
 *   | 樱点字段（6 位×7，居中）| `T.x+42 .. T.x+89` | `T.y+5 .. T.y+17` | 首字 `T.x+46`（`0x404924`）|
 *   | 最大樱点 6 位 | `T.x+93 .. T.x+136` | 同上 | 起点 = 樱点字段末 **+9**（`0x404ad1`）|
 *   | 最大樱点 **7 位**（值 ≥ 1000000）| `T.x+93 .. T.x+143` | 同上 | `th07:0x404ae0` 那一支（位数 7）|
 *   | 脉动花 sprite143（48×16，居中）| `T.x .. T.x+48` | `T.y .. T.y+16` | `T+(24,8)`（`0x404df0`–`0x404e36`）|
 *   ⇒ **整块 = x `T.x .. T.x+143`、y `T.y−4 .. T.y+17` = 143 × 21 单位 = 286 × 42 表面 px**
 *     （旁证：社区那张计数器裁图就是 **143×21** —— `engine/spirit/th07/02-cherry-counter-hud.md` §4）。
 *   ⇒ 7 位的**宽度一手怎么算**：字段起点是死的（`T.x+97` = 樱点字段 6 位定宽 42 + 间隔 9），
 *     每一位**推进 7**（含被跳过的前导零位），字形盒宽 8（`sprite132..141` 的墨迹**满 8 列**、
 *     居中锚 ⇒ 盒 = 中心 ±4）⇒ 第 7 位的右沿 = `T.x+97 + 6×7 + 4 = T.x+143`。
 *
 * 块尺寸报给槽位分配器（表面 px，HUD-LAYOUT.md §2/§3）：**仍是 200 × 42**。
 *   ⚠ 2026-09-30 owner 口径：「**应当处于最左侧（UI互相覆盖目前可接受，最后统一处理）**」
 *     ⇒ 本卡**不再**为了「不压别人的块」而左移或缩块：真实宽 286 > 认领的 200，
 *     右半会压到右邻块，owner 已接受（**块宽/货架的统一处理留到后面**）。
 *     旧口径（T 左移 43 让「右沿贴住块右沿」）**已作废** —— 那会把整块推到游玩区以左的空白里。*/
#define SENRO_HUD_W                 200.0f
#define SENRO_HUD_H                 42.0f   /* = 21 单位 × 2（整块计数器的真实高度，含 +C 行上沿）*/
/* ★★ 2026-09-30（owner 第 2 条，见 `senro_claim_hud` 的长注）：**钉住**在真正的左下角。
 *   `X = 68` = **112 的块左沿**（`ufo.c` 的 `UFO_HUD_PIN_X = 248 − 180 = 68`）= **原生「当前卡」UI 的左沿**
 *   （`HUD-LAYOUT.md` §1.3b：原生当前卡 UI = 表面 x 68..116）—— 同一轮 owner 给 108 的口径
 *   （「参考 117/112/113 的左下角位置……**你就按照那个调就是了**」）在 `youkai.c` 里算出的也是 **68**
 *   ⇒ 两张卡共用这条竖线。117 的可见左沿是 64（`ANIMAL_HUD_PIN_X 88` 是格心）、113 是 248。
 *   ⚠ **64 才是 TH07 `T.x` 的逐值换算**（640 空间 32）；取 68 = 与那条共用线对齐（差 4 表面 px），
 *     **一行可回退**（改回 64.0f 即回到「逐值复刻 TH07」）。
 *   `Y = 928` = **游玩区下沿**（与 112 的 `UFO_HUD_PIN_Y`、113 的 `TRANCE_HUD_BASE_Y` 同一条底线）。*/
#define SENRO_HUD_PIN_X             68.0f
#define SENRO_HUD_PIN_Y            928.0f
/* 原版锚 T 在本块里的位置（640 字面量空间，+y = 屏幕向下 ⇒ 负 = 往上）。
 * ★★ 2026-09-30（owner 第 2 条：「**左下角 HUD 位置不对，得再往左，参考 117、112、113 的左下角位置**」）
 *   —— 块改成**钉住 (68,928)**（见 `SENRO_HUD_PIN_X` / `senro_claim_hud` 的长注），于是：
 *     · X = **0**：T.x = 块左沿 = **34**（640 空间；= 钉住位 68 表面 ÷2）。
 *       ⚠ TH07 原版是 `T.x = 32`（= 表面 64）—— 差 **4 表面 px**，是 owner 那条「与 112 的块左沿对齐」
 *       换来的（要逐值复刻 TH07 就把 `SENRO_HUD_PIN_X` 改回 64）。
 *     · Y = **−15**：T.y = 464（= 钉住位 928 表面 ÷2）− 15 = **449** = 原版值 ✔（计数器盒
 *       `T.y−4 .. T.y+17` = 445..466，与原版一样骑在游玩区底边 464 上）。
 *   ⇒ 整个计数器与原版那一格（640 空间 `T = (32,449)`）只差 x 上那 4 表面 px。
 *   ⚠ 代价（owner 已知情并接受）：计数器真实宽 = 143 单位 = **286 表面 px**，而本块仍报 **200**
 *     ⇒ 右半（最大樱点那一段）会压到右邻块（112/117 都在这一带）。owner 2026-09-30：
 *     「UI互相覆盖目前可接受，最后统一处理」⇒ 本轮**不动块宽**、不动货架算法、不动别人的卡。
 *
 * 旧值留档（2026-09-22～09-29）：−43 / −17（右沿贴块）→ 2026-09-30 第一版 0/−13（走货架、块在 257）
 *   → 第二版 (64,924) + 0/−13（钉住；owner 同日的口径随后精确成「跟 112 的块左沿同一条线」= 68）。*/
#define SENRO_HUD_T_OFF_X            0.0f
#define SENRO_HUD_T_OFF_Y          (-15.0f)
/* 原版的版式常数（全部一手 = `th07:0x404690`；括号里是它读的常量地址）。*/
#define SENRO_HUD_TX_CHERRY         46.0f   /* T.x + 40(`0x498b7c`) + 6(`0x498b54`) */
#define SENRO_HUD_TY_CHERRY         11.0f   /* T.y + 11（`0x498c34`）*/
#define SENRO_HUD_TX_PLUS           53.0f   /* T.x + 40 + 6 + 7（`th07:0x404bd9`）*/
#define SENRO_HUD_TY_PLUS            2.0f   /* T.y + 2（`0x498a70`）*/
#define SENRO_HUD_PITCH              7.0f   /* 字距（`0x498c30`）*/
#define SENRO_HUD_PITCH_BORDER      10.0f   /* 结界中 +C 行的字距（`th07:0x404ccc` 的 0xa）*/
#define SENRO_HUD_GAP_MAX            9.0f   /* 樱点字段与最大樱点字段之间（`0x498c2c`）*/
#define SENRO_HUD_FIELD_CHERRY         6    /* 樱点字段位数（`th07:0x4049eb` 的 `cmp 6`）*/
#define SENRO_HUD_FIELD_PLUS           5    /* +C 字段位数（`th07:0x404d47` 的 `cmp 5`）*/
#define SENRO_HUD_PLUS_SCALE      1.41f     /* 结界中 +C 行放大（`0x3fb47ae1` = `th07:0x404cab`）*/
#define SENRO_HUD_PLUS_SHIFT       2.0f     /* 结界中 +C 行整体 +2 / −2（`0x498a70`；`th07:0x404ccc` / `0x404cea`）*/
#define SENRO_HUD_SCALE            2.0f     /* 原版 8×12 texel 在 TH18 UI 层的换算倍率（见脚本 296 头注）*/
#define SENRO_HUD_BORDER_DX        24.0f    /* 脉动花 = T + (24, 8)（`0x498c28` / `0x498a7c`），**居中锚**
                                             * ⇒ 盒 = `T .. T+48` × `T.y .. T.y+16`（与标签左半重合处）*/
#define SENRO_HUD_BORDER_DY         8.0f
#define SENRO_HUD_VM_SLOTS        17        /* 5(+C) + 6(樱点) + 6(最大樱点) —— 全是定宽字段 */
/* ★★ 2026-09-15g：**压暗这一路改走跨卡统一件 `cards/hud_dim.{h,c}`**（唯一实现）。
 * 性质 = ❓ **用户口径（UI 统一），不是复刻**（用户 2026-09-15：「压暗这个涉及到 UI 统一，
 * 这个不能复刻。是那个靠近透明吧」）⇒ **动态**统一：终点 alpha **32**（原 64）、起始 255、
 * ★ 2026-09-22 起：**8 帧线性**、终点 **64**（25%；= TH18 原生一手，见 `engine/card/th18/15-selected-card-hud.md` §4.2）；
 *   历史：当帧硬切（TH07 原样）→ ±56 = 4 帧 / 终点 64 → 20 帧 / 128（09-21）→ 本值。原注继续：
 * 滞回带 **16 px**；**区域**按**本卡自己那块 HUD 的矩形**推（取值全在共享件 `hud_dim.h`）。
 *
 * ★★ 2026-09-15h：**进/出的口径订正**（父 agent：15g 那版「进 = 矩形**之内**」裁错了）——
 *   **进 = 自机落在「块矩形外扩 `HUD_DIM_MARGIN`(28)」之内**、**出 = 离开「矩形 + 44」（28 + 滞回 16）之外**。
 *   理由：本块的**下沿 446 在自机 y 钳位 432 之下** ⇒ 「站进矩形」只剩 **6 px 高**；一手各作的门本来就落在
 *   它们自己那块上沿之上 24–32 px（TH16 的 `400` 在它那块沿之上 28）⇒ 边距是**原本就有的语义**。
 *
 * 本卡的块矩形 → 门限（**自机坐标**；换算 `自机 = 表面/2 − (320, 16)`，见 `hud_dim.h`）：
 *   · 块 = 货架槽（`s->slot`，表面 (257, 924) + **200 × 42** = `SENRO_HUD_W/H`）⇒ 表面 x 257..457、y 882..924
 *     （★ 2026-09-22：高 40 → **42** —— 计数器的真实高度 21 单位 × 2；行高取本行最大块、本行最大的是
 *      112 的 64 ⇒ 不影响别人的行。门限那一组随之各差 **2 px**，下面几行是旧值，留档不动）
 *   · ⇒ 自机矩形 = x ∈ [−191.5, −91.5]、y ∈ [426, 446]（上沿 426 / 下沿 446）
 *   · ⇒ **进** = 自机落在「矩形 + 28」之内：x ∈ [−219.5, −63.5] ∧ y ∈ [398, 474]
 *        （自机钳位 x ∈ [−184, 184]、y ∈ [32, **432**] ⇒ 实际可触发区 x ∈ [−184, −63.5]、y ∈ [398, 432]）
 *   · ⇒ **出** = 离开「矩形 + 44」：x < −235.5 ∨ x > −47.5 ∨ y < 382 ∨ y > 490（**16 px 滞回带**）
 *   ⚠ 原来的样子（= **原一手值 / 原实现**，留档，不再生效）：
 *     `SENRO_HUD_DIM 0x40u`（= 64，`HUD-LAYOUT.md` §4.5 的 64/255）；门读的是 SDK 那份
 *     `ce_hud_player_near_corner()`（= 400/384/−64，TH16 的栅格）—— 本卡是它**唯一**的卡侧调用点，
 *     `sdk_core.h` 也写明「卡片只读 `ce_hud_near_corner()`，别自己再推一次」⇒ 现在本卡不再调它
 *     （SDK 那条链自己仍在 `sdk.c` 每帧推进，别人要读照样读得到）。*/

/* ★★ 2026-09-16m 订正：th07 的 HUD 颜色字段是**一个 D3DCOLOR**（`obj+0x89c` 起 4 字节，
 *   第 4 字节 `+0x89f` = alpha，原生 `th07:0x404969` 从 `+0x653` 拷过去）⇒ 三字节的**字节序是
 *   `+0x89e = R`、`+0x89d = G`、`+0x89c = B`**，不是上一版注释写的「`0x89c..0x89e` = R,G,B」。
 *   判据：TH18 的 anm `color` 指令写 `arg0→vm+0x526`(R) / `+0x525`(G) / `+0x524`(B)
 *   （`sdk.h` §404 一手），而 TH18 的 `vm+0x524` color_1 与 `mgr+0x19224` 都是
 *   **D3DCOLOR 0xAARRGGBB**（Tenshi `0xff0080ff` = 蓝、`0x19224 = 0xff8080ff` = 蓝紫，都实跑认过）
 *   ⇒ 同一族的 th07 字段同理。旁证：用户口径「樱点满值变**金黄**」只有这个读法成立
 *   （另一个读法会得到浅蓝）。
 *
 *   th07 的两处判色（`th07:0x40496f`–`0x4049eb`，画**樱点数**那个 6 位计数器，值 = `樱点 − 基准`）：
 *     · `樱点 ≥ 最大樱点` → `(0xFF,0xD0,0x80)` **金黄**（`0x404984`–`0x4049a2`）
 *     · 否则 `樱点 − 基准 ≥ 0xc350` → `(0xFF,0xFF,0x80)` **亮黄**（`0x4049ad`–`0x4049c9`）
 *     · 否则 → `(0xFF,0xFF,0xFF)` 白（`0x4049cd`–`0x4049e9`）
 *   另有一个 **+C 指示器**（`th07:0x404c00` 起，值 = `Plus − 基准`，5 位数）：
 *     · 平时固定 `(0xC0,0x80,0xB0)` **樱粉**（`0x404cfe`/`0x404d08`/`0x404d12`）
 *     · **结界中**：`R = 0xFF` 固定（`0x404c27`），`G = B = Plus×0xc0/50000 + w×64/2000`
 *       （两次除法 `0x404c5a`/`0x404c86` 各带一个呼吸项；写 `+0x89d`/`+0x89c` 时是**按字节截断**）
 *       其中 `w = (Plus % 4000)`，`≥ 2000` 就折回 `4000 − w`（三角波，`0x404c24`–`0x404c4d`）
 *       ⇒ 随剩余时间由 `(0xFF,0xC0,0xC0)` 退到 `(0xFF,0,0)`，叠一个 4000 周期（≈66.7 s）的呼吸。
 *   TH18 侧收的也是**同一个 D3DCOLOR 0xAARRGGBB**（`sdk.c` 的 HUD 文字写 `mgr+0x19224`、
 *   `ce_anm_set_color` 写 `vm+0x524` color_1）⇒ 数值可以直接沿用，不用换序。*/
#define SENRO_HUD_COLOR_CHERRY_MAXED 0xffffd080u   /* RGB(0xFF,0xD0,0x80) 金黄 */
#define SENRO_HUD_COLOR_CHERRY_READY 0xffffff80u   /* RGB(0xFF,0xFF,0x80) 亮黄（樱点 − 基准 ≥ 50000）*/
#define SENRO_HUD_COLOR_CHERRY       0xffffffffu   /* 白 */
#define SENRO_HUD_COLOR_MAX          0xfff0d0e0u   /* RGB(0xF0,0xD0,0xE0) 最大樱点那一行的色（`th07:0x404aad`–`0x404ac1`）*/
#define SENRO_HUD_COLOR_PLUS_IDLE    0xffc080b0u   /* RGB(0xC0,0x80,0xB0) 樱粉（结界外的 +C 指示器）*/
/* 诊断日志间隔（游玩帧）。*/
#define SENRO_LOG_EVERY             600
/* 「樱点满值」公告（`CherryPoint Max!`，kind 3）的最小间隔：TH07 每加一次都发（`0x42f679`），
 * 这里节流防刷屏（❓自定，30 帧；登记 `TUNING.md` §2.0u-2）。*/
#define SENRO_MAXED_POPUP_GAP       30

/* ==================== ★ 结界的「看得见的那一半」：两个世界层 VM（2026-09-12 补）====================
 *
 * 背景：本卡原来只有「机制 + SE + 震屏」(`TUNING.md` §3 的 A4「107 结界特效 VM 未做」)。
 * 一手表明 **TH07 的结界视觉也不是 C 侧画的，而是两个 EffectManager 的 effect type**：
 *
 *   ① **结界展开 / 崩结界的大环** = effect **type 0x1c**
 *      开结界 `th07:0x441aa4`、灵击崩结界 `th07:0x441c19` 都是
 *      `push 0x1c; push <自机坐标 player+0x930>; push 4; push 1; push -1; ecx=EffectManager(0x12fe250);
 *       call 0x41c610`（`0x41c610` = 起一个 effect VM）。崩结界那一路**先杀旧的**（`th07:0x441be5`
 *      把 `[player+0xb7e6c]` 那个 VM 的存活位 `+0x2cc` 置 0）**再起新的** —— 本卡照这个顺序。
 *   ② **灵击时飞出去的那 32 颗** = effect **type 0x1d**
 *      `th07:0x441dd9`–`0x441e38` 的循环（`cmp i,0x20` = **32 次**）每轮 `push 0x1d; call 0x41c1c0`；
 *      角度与速度**是 C 侧算好写进 VM 的**（一手逐指令）：
 *          起始角 `0xC0490FDB` = **-π**，每轮 `ang += [0x498cfc]` = **0.19634954 = 2π/32 = 11.25°**
 *          `[vm+0x294] = cos(ang)`（`call 0x48bbf0` = **fcos**）
 *          `[vm+0x298] = sin(ang)`（`call 0x48bb40` = **fsin**）
 *      运动在 type 0x1d 的 update `th07:0x41ad10` 里：
 *          `pos[vm+0x24c] = home[vm+0x288] + v[+0x294..] × (帧 × 256 / 30)`
 *      ⇒ **从自机位置线性外扩**，30 帧走满 **256 单位**（640 空间；游玩区宽 384）。本卡照抄这个公式。
 *
 * type → 脚本号的表 = `th07:0x49efc0`（`{u16 id; u16 pad; void* update; void* draw}`，stride 0xc）：
 *   **type 0x1c → id 731**、**type 0x1d → id 690**。两个 id 都落在原生 `data/etama.anm` 上
 *   （`AnmManager::loadAnm` `th07:0x44df90` 的 base **每个 entry 前进「上一 entry 的 sprite 数」**；
 *   etama base 512 + entry0 的 168 sprites = 680（entry1）→ +50 = 730（entry2）→ +1 = **731**（entry3））：
 *     731 = entry3（`data/etama/etama4.png` 整张 256×256）的文件内脚本 0 → **本卡 `204`**
 *     690 = entry1（`data/etama/etama2.png`）的文件内 id 10（`thanm -l` 的 script35）→ **本卡 `205`**
 *   ⇒ 两个脚本**逐条照抄**（唯一差异 = `sprite(...)` 换成 ORDER 名字），见
 *     `assets/ability/scripts/204_senro_field.anm.txt` / `205_senro_ring.anm.txt` 的 diff 块与
 *     `assets/ability/make_senro_art.py` 的完整一手推导。
 *     交叉印证：道具脚本 = `0x2c4 + type`（`th07:0x4328ea`）= 708+type，正好落在同一段 base 上
 *     （`186_sakura_point.anm.txt` 已独立复核过那十条）。
 *
 * ⚠️ 与 `ufo.c` 的跟随 VM 的**结构差异**（照抄它、但这卡没有「载体」）：
 *   ufo 的两个 VM 是**跟着一个敌机实体**跑的（每帧抄 `abs_pos`）；107 的两个**没有载体** ——
 *   甲（204）是「就地起一次、脚本自己 `wait(32); delete()`」，乙（205）是「32 个 VM 各带一个固定方向、
 *   每帧按公式摆位置、脚本第 30 帧自己 `delete()`」。⇒ 生命周期管理与 ufo 同形
 *   （`*_vm_kill` 句柄清零幂等 + `on_delete` 全收），但**没有 follow**（位置由本文件算）。
 *
 * ⚠️⚠️ **建 VM 只能在 tick / 帧末**（`ufo.c` 的死亡栈纪律）：`on_death_before_deathbomb` 跑在
 *   **死亡栈**里（本卡文件头的长注 + spec §6.3）⇒ 那里**只置 pending 位**，
 *   真正的 `ce_anm_spawn` 一律发生在 `on_tick_2` 尾部的 `senro_fx_flush()`。
 *   中弹那一路因此晚 **1 帧**（≈16 ms）出现 —— 与既有「中弹消弹推迟一帧」同一口径，spec §9 第 3 条。 */
/* 脚本号**一律走 `anm_ids.h` 的宏**（脚本名不变 ⇒ 号跟着构建自动走）。
 * ★ 2026-09-13（贴图合并 PR）：这两条原来是**裸数字 204/205** —— 全库唯一一处「活的裸脚本号」
 *   （`ATLAS-MERGE-AUDIT.md` §5.2 #1 点名）。本次合并**不动脚本号**（只动 entry/sprite），
 *   所以值仍是 204/205、行为零变化；换成宏纯粹是防止下一个人重编号脚本时这里**静默指错**。*/
#define SENRO_ANM_SCRIPT_FIELD      CE_ANM_ABILITY_SCRIPT_SENRO_FIELD
#define SENRO_ANM_SCRIPT_RING       CE_ANM_ABILITY_SCRIPT_SENRO_RING
/* 层：**原版脚本不写 `layer()`**（TH07 的 effect VM 由 EffectManager 自己排）⇒ 由本文件（挂它的那一方）
 * 补世界层（3–19；HUD 那两层的 24/25 不适用）。❓ 自定：**两个都取 19**（世界列表最高的那一层，
 * `engine/anm/th18/02-render-stages.md` §1 的 0x2b 桩），理由 = 原生这两半都盖在弹幕之上：
 *   · 甲是「结界展开 / 崩结界」的全屏闪光（全屏 ⇒ 必须在最上面）；
 *   · 乙是 32 颗（原版是「被打掉的弹变成小樱点飞出去」，看得见才有意义）。
 * 旁边的口径（都可作参照，但都不是同一个东西）：`lolk.c:425` 的擦弹圈（自机位置上的环）取 **12**、
 * `ufo.c` 的令牌 / 本体取 **13 / 7**（与原生道具 / 敌机同层）。**实跑若发现被弹幕盖住，改这两行即可。** */
#define SENRO_FIELD_ANM_LAYER       19
#define SENRO_RING_ANM_LAYER        19
/* 32 颗的参数：全部一手（见上面的逐指令）。
 *   `SENRO_RING_UNITS` = `th07:0x441dee cmp [ebp-0x8],0x20`；
 *   `SENRO_RING_LIFE`  = 脚本 205 第 30 帧 `delete()`（`+20: // 30`），与公式分母的 30 同源；
 *   `SENRO_RING_SPAN / _DEN` = `th07:0x41ad30 fmul 256.0 (0x498a98) / 0x41ad36 fdiv 30.0 (0x498b00)`。 */
#define SENRO_RING_UNITS            32
#define SENRO_RING_LIFE             30
#define SENRO_RING_SPAN             256.0f
#define SENRO_RING_DEN              30.0f
/* 甲的自杀帧数（脚本 204：`iset($I0,32); +1: wait($I0); delete()` ⇒ 第 33 帧）。C 侧只用来清句柄。 */
#define SENRO_FIELD_LIFE            33
/* ★★ 2026-09-30 第三轮（owner）：「**进森罗结界会有一个粉色的六芒星围绕在自机旁边，随着时间逐渐缩小**」
 *   —— 开结界走**长命环**（脚本 **371**，= 204 去掉自杀），卡侧每帧写四个量。
 *   一手参数全部在 `th07:0x441ad5`–`0x441b41`（发动时 C 侧给这台 effect VM 的插值通道）：
 *     · `[vm+0xb4]`（插值计时）= **结界全长 540**、`[+0x218/+0x21c] = 1.0f`（from）、
 *       `[+0x220/+0x224] = **0.25f**`（to）、`[+0xc8] = 540` ⇒ 线性 **1.0 → 0.25 / 540 帧**；
 *     · 自转反向（`[+0x14] ×= -1.0f`，`0x498b5c` = −1.0）⇒ 脚本 204 那个正 π/1000 取负；
 *     · 位置：`Player::sub_441330` 每帧写 `[player+0xb7e6c]+0x24c`（`0x4414a2`–`0x4414bd`）
 *       ⇒ **环跟着自机走**（观感 = 绕在自机旁边）；
 *     · 淡入：脚本 `alphaTime(30,0,160)`（0 → 160 / 30 帧）——TH18 侧同样由卡侧逐帧写
 *       （不依赖 `stop()` 之后插值器还跑不跑，见脚本 371 的注）。*/
#define SENRO_FIELD_LONG_SCALE_FROM   1.0f
#define SENRO_FIELD_LONG_SCALE_TO     0.25f
#define SENRO_FIELD_LONG_ALPHA         160
#define SENRO_FIELD_LONG_ALPHA_FRAME    30
#define SENRO_FIELD_LONG_SPIN       (-0.0031415927f)   /* −π/1000 per 帧（反向）*/
/* pending 位（死亡栈里只置位、tick 里兑现）。 */
#define SENRO_FX_PEND_FIELD         1
#define SENRO_FX_PEND_RING          2
#define SENRO_FX_PEND_FIELD_LONG    4      /* ★ 第三轮：开结界的**长命环**（371）；崩结界仍走 204 */

/* 32 颗的方向表：`(cos(ang), sin(ang))`，`ang = -π + i × (2π/32)` —— **编译期常量**，不在运行时算三角函数
 * （原生那两条是 C 侧 `fcos`/`fsin`；我们这边照数值抄成表 ⇒ 卡侧零 x87，`make dllx87` 的不变式不动）。
 * 逐项 = `cos(-3.1415927 + i*0.19634954)` / `sin(同上)`（i = 行号×2 + 列号）。 */
static const float s_ring_v[SENRO_RING_UNITS][2] = {
    {-1.0f, 0.0f}, {-0.9807853f, -0.1950903f},
    {-0.9238795f, -0.3826834f}, {-0.8314696f, -0.5555702f},
    {-0.7071068f, -0.7071068f}, {-0.5555702f, -0.8314696f},
    {-0.3826834f, -0.9238795f}, {-0.1950903f, -0.9807853f},
    {0.0f, -1.0f}, {0.1950903f, -0.9807853f},
    {0.3826834f, -0.9238795f}, {0.5555702f, -0.8314696f},
    {0.7071068f, -0.7071068f}, {0.8314696f, -0.5555702f},
    {0.9238795f, -0.3826834f}, {0.9807853f, -0.1950903f},
    {1.0f, 0.0f}, {0.9807853f, 0.1950903f},
    {0.9238795f, 0.3826834f}, {0.8314696f, 0.5555702f},
    {0.7071068f, 0.7071068f}, {0.5555702f, 0.8314696f},
    {0.3826834f, 0.9238795f}, {0.1950903f, 0.9807853f},
    {0.0f, 1.0f}, {-0.1950903f, 0.9807853f},
    {-0.3826834f, 0.9238795f}, {-0.5555702f, 0.8314696f},
    {-0.7071068f, 0.7071068f}, {-0.8314696f, 0.5555702f},
    {-0.9238795f, 0.3826834f}, {-0.9807853f, 0.1950903f},
};

_Static_assert(sizeof(s_ring_v) / sizeof(s_ring_v[0]) == SENRO_RING_UNITS, "32 颗 = 32 个方向");

typedef struct {
    senro_state_t    st;                /* 纯逻辑状态（senro_core.h）*/
    ce_hud_slot_t    slot;              /* HUD 槽（左下角货架；认领在 on_load）*/
    int32_t          slot_ok;           /* 1 = 认领成功 */
    int32_t          frame;             /* 本关游玩帧计数（节流 / 日志）*/
    int32_t          log_frame;
    hud_dim_t        hud;               /* ★ 2026-09-15g：压暗状态（闩锁 + alpha，跨卡统一件
                                         * `cards/hud_dim.{h,c}`；门限由本卡块矩形推）*/
    int32_t          maxed_popup_at;    /* 上次「樱点满值」飘字的帧号 */
    int32_t          spell_was;         /* 上一帧符卡进行中 */
    int32_t          spell_bonus_alive; /* 本张符卡期间奖励位还活着（= 收掉，不是超时）*/
    int32_t          collect_log_at;    /* 结界自动收点的日志节流 */
    int32_t          hit_cancel_pending;/* 中弹灵击的消弹待补做（下一帧 flush；理由见 senro_do_libyou）*/
    /* ---- ★ 结界特效（世界层 VM，见文件中部那段长注）---- */
    uint32_t         fx_field_id;       /* 甲：结界那个大环的 VM 句柄（开结界 = 371 长命 / 崩结界 = 204）*/
    int32_t          fx_field_frame;    /* 甲的帧计数（长命环用它推 scale/alpha；204 用它到 33 帧清句柄）*/
    int32_t          fx_field_long;     /* ★ 1 = 长命环（371，跟自机 + 缩放到 0.25）/ 0 = 一次性 204 */
    uint32_t         fx_ring_id[SENRO_RING_UNITS];  /* 乙：32 颗（脚本 205）*/
    int32_t          fx_ring_frame;     /* 乙的帧计数（32 颗同帧起 ⇒ 共用一个）*/
    int32_t          fx_ring_live;      /* 1 = 这一批还活着 */
    int32_t          fx_pending;        /* 死亡栈里只置位，`senro_fx_flush()` 在 tick 尾兑现 */
    float            fx_home[3];        /* 乙的起点（= 起的那一刻的自机坐标，照 th07:0x441df8）*/
    int32_t          stat_fx_field;     /* 诊断：起过几次甲 */
    int32_t          stat_fx_ring;      /* 诊断：起过几批乙（×32 颗）*/
    /* ---- ★★ 2026-09-30：结界「持续期」的三件（见文件上方那段长注）---- */
    uint32_t         dim_id;            /* 全屏压暗 VM（脚本 369；结界外 = 0）*/
    int32_t          dim_alpha;         /* 上一帧写进去的 alpha（-1 = 尚无）*/
    int32_t          dim_warned;        /* 「压暗 VM 起不来」只报一次 */
    uint8_t          body_tinted;       /* 自机本体色被本卡改过（退出结界要还原）*/
    uint32_t         body_c1, body_c2;  /* 改动前的原始色（还原用）*/
    int32_t          text_age;          /* 公告的年龄；< 0 = 不画（TH07 是**单槽**：新公告顶掉旧的）*/
    int32_t          text_kind;         /* SENRO_MSG_*（0 = 无）*/
    int32_t          text_value;        /* kind 4（`Border Bonus %7d`）的那个数 */
    int32_t          text_count;        /* 诊断：本局播了几次 */
    int32_t          stat_collect;      /* 诊断：结界期间自动收点总数 */
    int32_t          stat_gain_item;    /* 诊断：道具通道加的樱点总数 */
    int32_t          stat_gain_graze;   /* 诊断：擦弹通道 */
    int32_t          stat_gain_hit;     /* 诊断：命中通道 */
    int32_t          stat_cherry_conv;  /* 诊断：满 P 转换撒出来的**大樱点实体**颗数（★ 2026-09-18）*/
    int32_t          stat_star;         /* 诊断：死敌掉 ★（原生小分道具）成功颗数（★ 2026-09-18）*/
    int32_t          stat_star_dropped; /* 诊断：★ 撒失败（道具池满）*/
    int32_t          stat_small;        /* 诊断：boss 死亡消弹撒出来的**小樱点实体**颗数（★ 2026-09-18）*/
    int32_t          stat_small_bullets;/* 诊断：那几笔消弹按一手折算的**颗数**（= 值 / 100）*/
    int32_t          star_off;          /* ★ 关断位（**0 = 开**，非 0 = 关）——「0 即默认」是刻意的：
                                         *   `senro_clear()` 的 `memset` 与 `on_run_reset` 都不会把它关掉
                                         *   （写成「1 = 开」的话每局重开就静默失效）。见 TUNING §2.0m */
} senro_card_t;

_Static_assert(sizeof(senro_card_t) <= 496, "ce_state 上限 496 字节（超了会静默返回 NULL）");

/* ==================== HUD：数字 VM 池（TH07 原版字形的贴图 VM）====================
 *
 * 版式与出处见文件上方 `SENRO_HUD_*` 那一大段；这里只放**实现**。
 * 生命周期的三条纪律（与旧的三行文字版相同，另见 `SENRO_HUD_*` 注）：
 *   ① VM 是本卡自己的 ⇒ `on_delete` / `on_run_reset` / `on_stage_start` 与「不在游玩帧」
 *      （主菜单、商店）都要收干净 —— 收不干净 = 永久钉在屏幕上（`METHOD.md` 铁律二）。
 *   ② 句柄可能被引擎在别处删掉 ⇒ 每帧 `ce_anm_get_vm()` 自愈（先例 = `ufo.c` 的 HUD 槽）。
 *   ③ 定宽字段 ⇒ 槽位号是**固定**的（0..4 = +C、5..10 = 樱点、11..16 = 最大樱点），
 *      数字变了只换 sprite，不重排 VM。
 *   ④ ★ 2026-09-22：另有两台**不成字段**的 VM —— **固定标签**（`label_id`，TH07 的 VM2 / sprite142）
 *      与**脉动花**（`border_id`，TH07 的 VM4 / sprite143）。两台都是「有就摆、没有就起」的常驻 VM。
 *
 * ⚠ **层内叠放 = 生成顺序**（`AnmManager__insert_in_ui_list_back` `0x488490` 尾插、渲染从表头到表尾
 *   ⇒ 后生成的压在上面，见 `engine/anm/th18/01-vm-instantiate.md` §2b.4）。TH07 的三台是
 *   **每帧**按 标签 → 数字 → 脉动花 的顺序调的（`th07:0x404910` / `0x404a6c` / `0x404e60`）
 *   ⇒ 这里靠**生成次序**复现同一件事：`on_draw` 里**先确保标签**、再发数字、最后才确保脉动花。
 *   （数字槽是惰性补的 ⇒ 中途新补的位数会排在那一刻之后，比 TH07 的「数字总在花之下」略松一点点；
 *     实际只影响标签左半那 48 单位里与数字重叠的几 px，登记在 `TUNING.md` §2.0u。）
 *
 * ⚠ 静态区、不是 `ce_state`（与旧实现同口径：一张卡同时只在场一个对象；`season.c`/`mof.c` 也这么写）。*/
typedef struct {
    uint32_t id[SENRO_HUD_VM_SLOTS];        /* 每个数字位一台 VM（0 = 空）*/
    int32_t  shown[SENRO_HUD_VM_SLOTS];     /* 该 VM 当前挂的 sprite（省一次 set_sprite；-1 = 未知）*/
    int32_t  n;                             /* 上一帧用到的槽数（本帧末尾收掉多余的）*/
    int32_t  warned;                        /* 起不来只报一次日志（防刷屏）*/
    uint32_t border_id;                     /* 森罗结界指示的脉动花（TH07 的 VM4 / sprite143）*/
    uint32_t label_id;                      /* ★ 数字左边那块固定标签（TH07 的 VM2 / sprite142，脚本 308）*/
} senro_hud_t;
static senro_hud_t s_hud;

static senro_card_t *st(ce_card_t *c) { return ce_state(c, senro_card_t); }

/* `DAT_0056aca0`（表面 px / 640 单位；1280×960 下 = 2.0）—— `sdk.c` 的 `hud_vm_ui_scale()` 同款守卫。
 * 卡侧不能调那个 static，就地补一份（先例 = `cards/season.c` / `mof.c` 都自己读这个地址）。*/
static float senro_hud_dat(void)
{
    float s = *(const float *)CE_ADDR_UI_SCALE;
    return (s > 0.1f && s < 16.0f) ? s : 2.0f;
}

static void senro_hud_vm_kill(uint32_t *id)
{
    if (*id) { ce_anm_delete(*id); *id = 0; }
}

/* 全收（`on_delete` / `on_run_reset` / `on_stage_start` / 非游玩帧）。幂等。*/
static void senro_hud_clear(void)
{
    int i;
    for (i = 0; i < SENRO_HUD_VM_SLOTS; ++i) { senro_hud_vm_kill(&s_hud.id[i]); s_hud.shown[i] = -1; }
    s_hud.n = 0;
    senro_hud_vm_kill(&s_hud.border_id);
    senro_hud_vm_kill(&s_hud.label_id);        /* ★ 标签也是本卡的 VM（漏收 = 永久钉在屏幕上）*/
}

/* 起一台 UI 层 VM（收的是 640 字面量空间的点 ⇒ 这里 ×DAT 换表面 px，同 `sdk.c` 的做法）。*/
static uint32_t senro_hud_spawn(int script, float x640, float y640)
{
    void *anm = CE_ABILITY_ANM();
    float pos[3];
    if (!anm || !script) return 0;
    pos[0] = x640 * senro_hud_dat();
    pos[1] = y640 * senro_hud_dat();
    pos[2] = 0.0f;
    return ce_anm_spawn_ui(anm, script, pos);
}

/* 一个数字位：放 / 摆 / 换字形 / 上色（只有变了才真动手，写法照 `sdk.c` 的 `ce_hud_number_set`）。*/
static void senro_hud_digit(int slot, float x640, float y640, float scale, uint32_t argb, int digit)
{
    void *anm = CE_ABILITY_ANM();
    float dat = senro_hud_dat();
    uint32_t *id;

    if (slot < 0 || slot >= SENRO_HUD_VM_SLOTS || digit < 0 || digit > 9 || !anm) return;
    id = &s_hud.id[slot];
    if (*id && !ce_anm_get_vm(*id)) { *id = 0; s_hud.shown[slot] = -1; }   /* 自愈：句柄被别处删了 */
    if (!*id) {
        *id = senro_hud_spawn(CE_ANM_ABILITY_SCRIPT_SENRO_HUD_DIGIT, x640, y640);
        s_hud.shown[slot] = -1;
        if (!*id) {
            if (!s_hud.warned) {
                s_hud.warned = 1;
                ce_log("senro: HUD 数字 VM 起不来（脚本 %d）—— HUD 这一轮只画得出的那几位",
                       CE_ANM_ABILITY_SCRIPT_SENRO_HUD_DIGIT);
            }
            return;
        }
    } else {
        (void)ce_anm_set_pos(*id, x640 * dat, y640 * dat, 0.0f);
    }
    (void)ce_anm_set_scale(*id, scale, scale);        /* 脚本里没有 scaleTime ⇒ 不会被插值器覆盖 */
    (void)ce_anm_set_color(*id, argb);
    if (s_hud.shown[slot] != digit) {
        if (ce_anm_set_sprite(anm, *id, (int)(CE_ANM_ABILITY_SPRITE_SENRO_HUD_D0 + (uint32_t)digit)))
            s_hud.shown[slot] = digit;
    }
    if (slot + 1 > s_hud.n) s_hud.n = slot + 1;
}

/* 定宽字段（一手：`th07:0x4049eb`–`0x404a89`）——
 * **每一位都推进 x**（`x += pitch`，含被跳过的前导零位），前导零只是**不画**
 * ⇒ 字段宽度固定、数字靠右对齐；`divisor == 1`（个位）永远画，所以值 0 显示 "0"。
 * `*slot` 每帧从 0 起走，返回值 = 字段结束后的 x（给下一个字段用）。*/
static float senro_hud_field(int *slot, float x640, float y640, float pitch, float scale,
                            uint32_t argb, int32_t value, int digits)
{
    int32_t div = 1;
    int i;

    if (value < 0) value = 0;
    for (i = 1; i < digits; ++i) div *= 10;
    for (i = 0; i < digits; ++i, div /= 10) {
        int d = (div > 0) ? (int)((value / div) % 10) : 0;
        if (d != 0 || div <= 1)
            senro_hud_digit(*slot, x640, y640, scale, argb, d);
        ++*slot;
        x640 += pitch;
    }
    return x640;
}

static int32_t senro_graze_now(void) { return *(volatile int32_t *)SENRO_ADDR_GRAZE; }

static void senro_cancel_impending_death(void)
{
    void *p = CE_PLAYER();
    if (p) ((void (__attribute__((thiscall)) *)(void *))SENRO_FN_CANCEL_IMPENDING_DEATH)(p);
}

/* 难度（0 EASY..3 LUNATIC；= 0x4ccd00，`cards/mof.c` / `cards/th06rank.c` 同址，**只读**）。
 * th07 的最大樱点初值按难度查表（`th07:0x42eb7a`–`0x42ec57`），本卡照搬同一张表。*/
static int senro_difficulty(void) { return *(volatile int32_t *)SENRO_ADDR_DIFFICULTY; }

/* ---- ⛔ 旧「自机位置飘字」（TH18 PopupManager 的数字槽）已于 2026-09-30 第三轮**整套撤掉** ----
 * 历史：`bonus 1.0` 两次实跑 ⇒ 2026-09-18 从倍率槽改成数字槽（只画一行数字）——但那是「TH18 画不了文字」
 * 那个前提下的 ❓自定 权宜。第三轮 owner 点出原版那一刻的**文字**（`Border Bonus []` 从右侧飞过来），
 * 而 `ce_hud_text_ex` 这条路已经打通（`SENRO_MSG_*`）⇒ 两处飘字全部换成一手公告：
 *   · 自然结束 = kind 4 `Border Bonus %7d`（`th07:0x4416dd`）→ `senro_text_post(s, SENRO_MSG_BONUS, …)`
 *   · 樱点满值 = kind 3 `CherryPoint Max!`（`0x42f696`/`0x42f72d`）→ `SENRO_MSG_MAXED`
 * 复核记录：`TUNING.md` §2.0u-2 / `AUDIT.md` §BE #151 追加。*/
/* ============================ ★ 结界特效：两个世界层 VM ============================
 * 全部一手依据（逐指令 + 地址）在文件中部那段长注里，这里只写「怎么落地」。
 * ⚠️ 三个铁律：① **建** VM 只在 tick / 帧末（死亡栈里只置 pending）；
 *             ② 每一条路都要能把句柄收干净（`senro_fx_clear()` 在 `on_delete` / `on_run_reset` 也跑）；
 *             ③ 脚本自己 `delete()` 掉之后句柄要能自愈（`ce_anm_set_pos` 返回 0）。 */

/* 甲：收掉结界的那个环（句柄清零 ⇒ 幂等）。 */
static void senro_fx_field_kill(senro_card_t *s)
{
    if (s->fx_field_id) { ce_anm_delete(s->fx_field_id); s->fx_field_id = 0; }
    s->fx_field_frame = 0;
    s->fx_field_long = 0;
}

/* 甲：在 `pos` 起一个（照 `th07:0x441aa4`/`0x441c19`：**先杀旧的再起新的**，见 `th07:0x441be5`）。
 * 立刻摆一次位置（世界层 (0,0,0) = 场地正中 ⇒ 不摆会先在正中闪一帧）。
 * ★ 2026-09-30 第三轮：`long_mode` 选脚本 —— **开结界 = 371（长命环，跟自机 + 缩到 0.25）**、
 *   **崩结界 = 204（原脚本，33 帧自杀的那一闪）**；两条路都保持「先杀旧的再起新的」原样。*/
static void senro_fx_field_spawn(senro_card_t *s, const float *pos, int long_mode)
{
    uint32_t id;
    int script = long_mode ? CE_ANM_ABILITY_SCRIPT_SENRO_FIELD_LONG : SENRO_ANM_SCRIPT_FIELD;

    senro_fx_field_kill(s);
    id = ce_anm_spawn(CE_ABILITY_ANM(), script, SENRO_FIELD_ANM_LAYER);
    if (!id) {
        ce_log("senro: 结界环 VM 起不来（脚本 %d、层 %d）—— 机制不受影响，只是这一半看不见",
               script, SENRO_FIELD_ANM_LAYER);
        return;
    }
    s->fx_field_id = id;
    s->fx_field_frame = 0;
    s->fx_field_long = long_mode ? 1 : 0;
    ++s->stat_fx_field;
    if (pos) (void)ce_anm_set_pos(id, pos[0], pos[1], 0.0f);
}

/* 长命环每帧（脚本 371）：pos **跟自机** / scale **1.0 → 0.25** / rotation **−π/1000 per 帧** /
 * color **alpha 0 → 160**（全部一手，见 `SENRO_FIELD_LONG_*` 那段注）。返回 0 = VM 已不在（调用方清句柄）。*/
static int senro_fx_field_long_tick(senro_card_t *s)
{
    uint32_t id = s->fx_field_id;
    const float *pp;
    float k, sc;
    int32_t f = s->fx_field_frame;
    uint32_t a;

    if (!id || !ce_anm_get_vm(id)) return 0;
    pp = ce_player_pos();
    if (pp) (void)ce_anm_set_pos(id, pp[0], pp[1], 0.0f);
    k = (float)f / (float)SENRO_BORDER_FRAMES;
    if (k > 1.0f) k = 1.0f;
    sc = SENRO_FIELD_LONG_SCALE_FROM + (SENRO_FIELD_LONG_SCALE_TO - SENRO_FIELD_LONG_SCALE_FROM) * k;
    (void)ce_anm_set_scale(id, sc, sc);
    (void)ce_anm_set_rotation(id, 0.0f, 0.0f, SENRO_FIELD_LONG_SPIN * (float)f);
    a = (f >= SENRO_FIELD_LONG_ALPHA_FRAME)
            ? (uint32_t)SENRO_FIELD_LONG_ALPHA
            : (uint32_t)((SENRO_FIELD_LONG_ALPHA * f) / SENRO_FIELD_LONG_ALPHA_FRAME);
    (void)ce_anm_set_color(id, (a << 24) | 0x00ffffffu);   /* 色 = 白（一手 spawn 的色参 = −1）*/
    return 1;
}

/* 乙：收掉那 32 颗（**只看句柄**：`on_ctor` / `on_run_reset` 之后颜色之类的字段已经没意义）。 */
static void senro_fx_ring_kill(senro_card_t *s)
{
    int i;
    for (i = 0; i < SENRO_RING_UNITS; ++i) {
        if (s->fx_ring_id[i]) { ce_anm_delete(s->fx_ring_id[i]); s->fx_ring_id[i] = 0; }
    }
    s->fx_ring_frame = 0;
    s->fx_ring_live = 0;
}

/* 乙：在 `pos` 起 32 颗（照 `th07:0x441df4` 的循环：同一个起点、32 个方向由 `s_ring_v` 给）。
 * ⚠️ 方向/速度在原生里是 C 侧写进 VM 的（`[vm+0x294] = cos(ang)`、`[vm+0x298] = sin(ang)`），
 *    本卡没有 effect type 那一套 ⇒ 方向留在本文件、每帧用公式摆位置（见 `senro_fx_tick`）。 */
static void senro_fx_ring_spawn(senro_card_t *s, const float *pos)
{
    int i, n = 0;

    senro_fx_ring_kill(s);
    if (!pos) return;
    s->fx_home[0] = pos[0];
    s->fx_home[1] = pos[1];
    s->fx_home[2] = 0.0f;
    for (i = 0; i < SENRO_RING_UNITS; ++i) {
        uint32_t id = ce_anm_spawn(CE_ABILITY_ANM(), SENRO_ANM_SCRIPT_RING, SENRO_RING_ANM_LAYER);
        if (!id) continue;
        s->fx_ring_id[i] = id;
        (void)ce_anm_set_pos(id, s->fx_home[0], s->fx_home[1], 0.0f);
        ++n;
    }
    s->fx_ring_frame = 0;
    s->fx_ring_live = (n > 0);
    ++s->stat_fx_ring;
    if (n != (int)SENRO_RING_UNITS)
        ce_log("senro: 灵击环只起了 %d/%d 颗（ANM VM 池满？）", n, (int)SENRO_RING_UNITS);
}

/* pending 兑现：**只能在 `on_tick_2` 里调**（建 VM = 分配；死亡栈里绝不建）。 */
static void senro_fx_flush(senro_card_t *s)
{
    const float *pp;

    if (!s || !s->fx_pending) return;
    pp = ce_player_pos();                       /* 一手：原生取起效那一刻的 `player+0x930` */
    if (s->fx_pending & SENRO_FX_PEND_FIELD_LONG) senro_fx_field_spawn(s, pp, /*long_mode=*/1);
    else if (s->fx_pending & SENRO_FX_PEND_FIELD) senro_fx_field_spawn(s, pp, /*long_mode=*/0);
    if (s->fx_pending & SENRO_FX_PEND_RING) senro_fx_ring_spawn(s, pp);
    s->fx_pending = 0;
}

/* 每帧：甲清句柄（脚本 204 第 33 帧自杀）；乙按 `pos = home + v × (帧 × 256 / 30)` 外扩
 * （一手公式 = `th07:0x41ad10`：`fmul [0x498a98]=256.0` / `fdiv [0x498b00]=30.0`，再与
 *  `[vm+0x288]`（起点）相加写回 `[vm+0x24c]`）。 */
static void senro_fx_tick(senro_card_t *s)
{
    int i;

    if (!s) return;
    if (s->fx_field_id) {
        ++s->fx_field_frame;
        if (s->fx_field_long) {
            /* 长命环（371）：跟自机 + 缩到 0.25 / 540 帧；**结界一结束就收**（原版那台 VM 的插值
             * 本来就是按结界长度配的，结界没了它也就没意义了）。*/
            if (!senro_border_active(&s->st) || !senro_fx_field_long_tick(s))
                senro_fx_field_kill(s);
        } else if (s->fx_field_frame >= SENRO_FIELD_LIFE || !ce_anm_get_vm(s->fx_field_id)) {
            senro_fx_field_kill(s);      /* 204 第 33 帧自杀 ⇒ 这里清句柄（自愈）*/
        }
    }
    if (!s->fx_ring_live) return;
    ++s->fx_ring_frame;
    if (s->fx_ring_frame > SENRO_RING_LIFE) {    /* 脚本第 30 帧 delete() ⇒ 这里收句柄 */
        senro_fx_ring_kill(s);
        return;
    }
    {
        float r = (float)s->fx_ring_frame * SENRO_RING_SPAN / SENRO_RING_DEN;
        for (i = 0; i < SENRO_RING_UNITS; ++i) {
            uint32_t id = s->fx_ring_id[i];
            if (!id) continue;
            if (!ce_anm_set_pos(id, s->fx_home[0] + s_ring_v[i][0] * r,
                                    s->fx_home[1] + s_ring_v[i][1] * r, 0.0f))
                s->fx_ring_id[i] = 0;            /* 脚本已经自杀 ⇒ 清句柄（自愈）*/
        }
    }
}

/* 收尾（`on_tick` 之外的一切路：`on_delete` / `on_run_reset` / `on_stage_start`）。 */
static void senro_fx_clear(senro_card_t *s)
{
    if (!s) return;
    senro_fx_field_kill(s);
    senro_fx_ring_kill(s);
    s->fx_pending = 0;
}

/* ==================== ★★ 2026-09-30：结界「持续期」的三件（压暗 / 红白闪 / 展开文字）====================
 * 一手、曲线与取舍逐条写在文件上方 `SENRO_DIM_*` / `SENRO_FLASH_*` / `SENRO_TEXT_*` 那一段。 */

static void senro_dim_kill(senro_card_t *s)
{
    if (!s) return;
    if (s->dim_id) { ce_anm_delete(s->dim_id); s->dim_id = 0; }
    s->dim_alpha = -1;
}

#if SENRO_DIM_ENABLE
/* TH07 的压暗曲线（`th07:0x44158d`–`0x44162f`）→ **等价**黑底遮罩的 alpha。
 * 原版乘子 `m = g/128`；遮罩 `dst·(1 − α/255)` ⇒ `α = 255 − 255·g/128`（g=128 ⇒ 0、g=48 ⇒ 159）。
 * ⛔ 现在整段默认关（`SENRO_DIM_ENABLE 0`，owner 2026-09-30 裁决）；重开时才编进来。*/
static int32_t senro_dim_alpha_at(int32_t elapsed)
{
    int32_t x = elapsed, g;

    if (x < 0) x = 0;
    if (x > SENRO_BORDER_FRAMES) x = SENRO_BORDER_FRAMES;
    if (x < SENRO_DIM_RAMP)
        g = SENRO_DIM_G_FULL - (x * (SENRO_DIM_G_FULL - SENRO_DIM_G_HOLD)) / SENRO_DIM_RAMP;
    else if (x >= SENRO_BORDER_FRAMES - SENRO_DIM_RAMP)
        g = SENRO_DIM_G_FULL - ((SENRO_BORDER_FRAMES - x) * (SENRO_DIM_G_FULL - SENRO_DIM_G_HOLD)) / SENRO_DIM_RAMP;
    else
        g = SENRO_DIM_G_HOLD;
    if (g < 0) g = 0;
    if (g > SENRO_DIM_G_FULL) g = SENRO_DIM_G_FULL;
    /* ★ 四舍五入（+半个除数）：g=48 ⇒ `(12240+64)/128 = 96` ⇒ α = **159**（= round(255×0.625)，
     *   与 0.375 的原版乘子差 0.15 %；直接截断会给 160）。g=128 ⇒ 0、g=0 ⇒ 255，两头仍精确。*/
    return 255 - (255 * g + SENRO_DIM_G_FULL / 2) / SENRO_DIM_G_FULL;
}
#endif /* SENRO_DIM_ENABLE */

static void senro_dim_tick(senro_card_t *s)
{
    if (!s) return;
#if !SENRO_DIM_ENABLE
    /* ⛔ 默认关（owner 2026-09-30 实跑判定「整个画面暗下来肯定不太对」；理由见文件上方 ① 那段）。
     *   关着时连 VM 都不起，但**仍然收掉可能残留的那台**（改开关后中途出结界/删卡不会留罩子）。*/
    senro_dim_kill(s);
    return;
#else
    {
    int32_t a;

    if (!senro_border_active(&s->st)) { senro_dim_kill(s); return; }   /* 出结界 = 撤遮罩 */
    a = senro_dim_alpha_at(SENRO_BORDER_FRAMES - s->st.timer);
    if (!s->dim_id) {
        s->dim_id = ce_anm_spawn(CE_ABILITY_ANM(), CE_ANM_ABILITY_SCRIPT_SENRO_DIM, SENRO_DIM_LAYER);
        if (!s->dim_id) {
            if (!s->dim_warned) {
                s->dim_warned = 1;
                ce_log("senro: 结界压暗 VM 起不来（脚本 %d）—— 持续期这一件不显示",
                       CE_ANM_ABILITY_SCRIPT_SENRO_DIM);
            }
            return;
        }
        s->dim_alpha = -1;                  /* 刚起：下一行一定写一次色 */
    }
    if (a != s->dim_alpha) {
        /* 只写 color_1 的 RGB+alpha：脚本里那条 `color(0,0,0)` 已经把 RGB 定成黑，
         * 这里每帧连 alpha 一起写（脚本**不写** alpha，理由见 261 的注：写了就是插值器）。*/
        if (!ce_anm_set_color(s->dim_id, (uint32_t)a << 24)) s->dim_id = 0;   /* VM 没了 ⇒ 自愈 */
        else s->dim_alpha = a;
    }
    }
#endif
}

/* 自机本体色（TH07 `player+0x1b8` = 自机内嵌 AnmVm 的颜色）。
 * TH18：`zPlayer.vm` = `player+0x14`（engine.h `CE_PLAYER_VM`），color_1/2 = `vm+0x524/0x528`
 * ⇒ 绝对 `player+0x538/0x53c`。**两个都写**（引擎自己的无敌闪色写的是 color_2，见头的注）。
 * ⚠ 相位：必须晚于 `Player__on_tick`(0x17)、早于 `Player::on_draw`(0x1d) ⇒ 只在 `on_item_frame`
 *   （`BP_ce_item_frame` 0x446ee3，优先级 0x1e）里调，见 `cards/youkai.c` 的同一用法。*/
static void senro_player_flash(senro_card_t *s)
{
    uint8_t *p;
    volatile uint32_t *c1, *c2;

    if (!s) return;
    p = CE_PLAYER();
    if (!p) { s->body_tinted = 0; return; }          /* 自机不在（重生间隙 / 菜单）⇒ 记录作废 */
    c1 = (volatile uint32_t *)(p + CE_PLAYER_VM + CE_ANM_VM_COLOR1);
    c2 = (volatile uint32_t *)(p + CE_PLAYER_VM + CE_ANM_VM_COLOR2);
    if (senro_border_active(&s->st)) {
        int32_t x = SENRO_BORDER_FRAMES - s->st.timer;
        uint32_t col = ((x & 3) < 2) ? SENRO_FLASH_RED : SENRO_FLASH_WHITE;   /* 一手 `% 4 < 2` 取红 */
        if (!s->body_tinted) { s->body_c1 = *c1; s->body_c2 = *c2; s->body_tinted = 1; }
        *c1 = col;
        *c2 = col;
    } else if (s->body_tinted) {                     /* 出结界：把原始色还回去（只还一次）*/
        *c1 = s->body_c1;
        *c2 = s->body_c2;
        s->body_tinted = 0;
    }
}

/* 出结界/删卡/换关的统一收尾（`senro_clear` 在 memset 之前调它）。*/
static void senro_period_clear(senro_card_t *s)
{
    if (!s) return;
    senro_dim_kill(s);
    if (s->body_tinted) {
        uint8_t *p = CE_PLAYER();
        if (p) {
            *(volatile uint32_t *)(p + CE_PLAYER_VM + CE_ANM_VM_COLOR1) = s->body_c1;
            *(volatile uint32_t *)(p + CE_PLAYER_VM + CE_ANM_VM_COLOR2) = s->body_c2;
        }
        s->body_tinted = 0;
    }
    s->text_age = -1;
    s->text_kind = SENRO_MSG_NONE;
}

/* 展开文字（TH07 GUI 公告 kind 2）—— ⚠ **只能在 `on_draw` 里调**（`ce_hud_text_ex` 的硬约束：
 * 字符串只活一帧，必须与它所属组的绘制趟同帧创建）。年龄由调用方推进。*/
static void senro_text_draw(const senro_card_t *s)
{
    float x;

    if (!s || s->text_age < 0 || s->text_kind == SENRO_MSG_NONE) return;
    x = (s->text_age < SENRO_TEXT_SLIDE)
            ? SENRO_TEXT_X_START
                  - (SENRO_TEXT_X_START - SENRO_TEXT_X_END) * (float)s->text_age / (float)SENRO_TEXT_SLIDE
            : SENRO_TEXT_X_END;
    /* font 0 = `sprite(char − 0x20)`：唯一含小写与 `!` 的字形集；group 0 的绘制趟跑在本卡 on_draw 之后
     * （`engine/anm/th18/03-ascii-text.md` §2d）。三种 kind 共用同一套位置/滑入（一手 `0x42b278`–`0x42b2cc`）。*/
    switch (s->text_kind) {
    case SENRO_MSG_BONUS:
        /* kind 4（`th07:0x4416dd` → `0x4284b1`）：`Border Bonus %7d`，色同 kind 2（`0x4284cf` = 0xffe0b0ff）。*/
        ce_hud_text_ex(x, SENRO_TEXT_Y, SENRO_TEXT_COLOR, 0u, 0, 0, CE_ASCII_ALIGN_LEFT, SENRO_TEXT_SCALE,
                       "Border Bonus %7d", (int)s->text_value);
        break;
    case SENRO_MSG_MAXED:
        /* kind 3（`0x42f696`/`0x42f72d` → `0x428480`）：`CherryPoint Max!`，色 `0xffc0b0ff`。*/
        ce_hud_text_ex(x, SENRO_TEXT_Y, SENRO_TEXT_COLOR_MAXED, 0u, 0, 0, CE_ASCII_ALIGN_LEFT, SENRO_TEXT_SCALE,
                       "CherryPoint Max!");
        break;
    default:
        /* kind 2（`th07:0x441b7a` → `0x428414`）：`Supernatural Border!!`。*/
        ce_hud_text_ex(x, SENRO_TEXT_Y, SENRO_TEXT_COLOR, 0u, 0, 0, CE_ASCII_ALIGN_LEFT, SENRO_TEXT_SCALE,
                       SENRO_TEXT_STR);
        break;
    }
}

/* 发一条公告（TH07 `0x427c81` 的语义：**单槽**——新公告直接顶掉旧的，pos/计时器重置）。
 * 只允许在「非绘制相位」调（它只写本卡状态）；真正画在 `on_draw`（`senro_text_draw`）。*/
static void senro_text_post(senro_card_t *s, int kind, int32_t value)
{
    if (!s) return;
    s->text_kind = kind;
    s->text_value = value;
    s->text_age = 0;
    ++s->text_count;
}

/* 把纯逻辑累积的事件变成表现 + 日志（每次入账之后与每帧各调一次；事件取走即清）。*/
static void senro_handle_events(ce_card_t *c, senro_card_t *s, int ev)
{
    (void)c;
    if (!ev || !s) return;
    if (ev & SENRO_EV_BORDER_OPEN) {
        /* ★ 结界展开的视觉（原版 effect type 0x1c，`th07:0x441aa4`）——
         * ⚠️ 这里只**置 pending**，真正的 `ce_anm_spawn` 在 `on_tick_2` 尾部兑现：
         *    本函数也会从断点回调（`on_item_collect` / `on_shot_damage_any`）里被调到，
         *    而「建 VM = 分配」这件事只在 tick / 帧末做（ufo.c 的死亡栈纪律）。 */
        /* ★ 第三轮：开结界起的是**长命环**（脚本 371：跟自机、540 帧里从 1.0 缩到 0.25），
         *   不再是那个 33 帧就自杀的 204（204 留给崩结界那一闪）。*/
        s->fx_pending |= SENRO_FX_PEND_FIELD_LONG;
        /* 展开文字（TH07 GUI 公告 kind 2 = `Supernatural Border!!`；一手 `th07:0x441b7a` 就在同一个
         * 尾段里、紧跟环的句柄）——`senro_text_post` 会按「单槽」语义顶掉上一条公告。*/
        senro_text_post(s, SENRO_MSG_BORDER, 0);
        ce_play_sound(SENRO_SE_BORDER_OPEN, ce_player_x());
        /* ★ 2026-09-20：一手在**同一支**里连播两枚（`th07:0x441b91` 紧接着 `0x441b9f`）⇒ 这里照抄时序。*/
        ce_play_sound(SENRO_SE_BORDER_OPEN2, ce_player_x());
        ce_log("senro: 森罗结界 OPEN (%d frames, cherry %d/%d, +C %d) —— 持续期视觉：压暗 %s + "
               "自机红白闪 + 文字 \"%s\"（%d 帧）",
               (int)SENRO_BORDER_FRAMES, (int)s->st.cherry, (int)s->st.cherry_max, (int)s->st.plus,
#if SENRO_DIM_ENABLE
               "α 0→159→0（30 帧入 / 480 帧稳 / 30 帧出）",
#else
               "off（owner 2026-09-30 裁决：整个画面暗下来不对）",
#endif
               SENRO_TEXT_STR, (int)SENRO_TEXT_LIFE);
    }
    if (ev & SENRO_EV_BORDER_QUEUE)
        ce_log("senro: +C 满但正在放炸弹 → 排队（th07 [player+0x240d]=2 口径）");
    if (ev & SENRO_EV_END_NATURAL) {
        /* 结算表现照一手（`th07:0x441670`）：分数 += (樱点 − 基准)、Border Bonus 飘字、SE。*/
        ce_add_score_delta(s->st.last_bonus);
        ce_play_sound(SENRO_SE_BORDER_END, ce_player_x());
        /* ★ 第三轮（owner）：「正常退出森罗结界还会从右侧飞过来一个 `Border Bonus []`」——
         *   一手 = TH07 GUI 公告 **kind 4**（`th07:0x4416dd` push 0x4 → `0x4284b1` 的 `Border Bonus %7d`，
         *   色 `0xffe0b0ff`、位置/滑入/180 帧寿命与另外三条公告同一套）。
         *   ⚠ 原来是「自机位置一个数字飘字」（`senro_popup_at_player`，❓自定 的老权宜）⇒ **撤掉**：
         *   原版这一刻只有这一条公告，留着会变成两个数字。*/
        senro_text_post(s, SENRO_MSG_BONUS, s->st.last_bonus);
        ce_log("senro: 结界自然结束 → 樱点 %d / 最大樱点 %d，Border Bonus +%d",
               (int)s->st.cherry, (int)s->st.cherry_max, (int)s->st.last_bonus);
    }
    if (ev & SENRO_EV_CHERRY_MAXED) {
        /* ★ 第三轮顺带订正：这里原来也是「自机位置的数字飘字」（❓自定）；一手是 TH07 GUI 公告
         *   **kind 3** = `CherryPoint Max!`（`0x42f696`/`0x42f72d` → `0x428480`，色 `0xffc0b0ff`）
         *   ⇒ 换成同一条文字通道（节流沿用原来的 30 帧，防刷屏；节流值登记在 `TUNING.md` §2.0u-2）。*/
        if (s->frame - s->maxed_popup_at >= SENRO_MAXED_POPUP_GAP) {
            s->maxed_popup_at = s->frame;
            senro_text_post(s, SENRO_MSG_MAXED, 0);
        }
    }
}

/* ---- 计分层（★ 2026-09-13 重做：声明自己的那一份 G_i）----
 * 本卡是 TH07 那一族：**「最大得点」= 蓝点分值**（`th07:0x4330db`–`0x4331e5` 的一手公式，
 * `senro_point_item_value()`），对用户公式而言它就是 `G_07`，**加法族**（不与任何乘数耦合）。
 *
 * ★ 重做前这里的问题（`CARD-NUMERIC-INTERACTION-AUDIT.md` §1.6 / §6 第 1 条）：
 *   107 的 id 最小 ⇒ 它**永远**是「落笔者」，而它的落笔公式是 `pv = Σ所有声明`（把聚合当最终 PV）
 *   ⇒ 110/111 的同装公式**整条不跑**，两份 base 还被双计。
 *   现在：卡只**声明自己的 G_i**（`ce_score_declare_delta`，未耦合通道），由 SDK 顶层
 *   `ce_score_apply()` **一次**合成 `G = G_0 + Σ未耦合 + coupling × Σ耦合`（见 SCORE-LAYER-REWORK.md）。
 *   ⇒ 本卡不再需要「我是不是落笔者」这一问——**没人再争落笔权**。
 *
 * ⚠ **coeff_06 不在本卡、也不在本层**：引擎自己按收取高度打折（一手 `th18:0x446bf9`–`0x446cf3`，
 *   判据 = 回收线 `player+0x47998`）。所以「线上 = 满额、线下按高度递减」对**所有**声明者
 *   （含本卡）自动成立，本卡不必、也不该自己再乘一次。
 * ⚠ 本卡值**恒 ≥ SENRO_POINT_BASE(50000)**（`senro_point_item_value()` 的下限），
 *   与「0 = 不参与」不冲突；删卡时由 `senro_clear()` 显式退场。*/
static void senro_declare_score(senro_card_t *s)
{
    if (!s) return;
    ce_score_declare_delta(SENRO_ID, senro_point_item_value(&s->st));
    /* ★ 2026-09-18（计分层换版，路线 A）：本卡是 **th07** 那一族 —— 樱点项在原生里
     *   **不吃收分高度折扣**（一手：线上整段替换 base、线下加在折扣之后，见 `engine/item/th07/01-sakura-items.md`）
     *   ⇒ `coeff_107 ≡ 1`。声明族之后，层会把它按 `1/coeff_0` 归一，引擎再乘 `coeff_0`
     *   ⇒ **显示与分数都不打折**（这正是路线 A 的意义）。*/
    ce_score_family_declare(SENRO_ID, CE_COEFF_FAMILY_1);
}

static void on_item_score(ce_card_t *c, int32_t *value)
{
    senro_card_t *s = st(c);

    (void)value;                       /* ★ 重做后 `*value` 由 SDK 统一写（`ce_score_apply()`）*/
    if (!s) return;
    senro_declare_score(s);            /* 每颗道具当场声明一次（值随樱点变，粘性但便宜）*/
}

/* ---- ③ 结界中：自动回收道具（用户口径 + `th07:0x4414xx` 段的结界行为）----
 * 用 SDK 的共享件 `ce_item_collect_all()`（把池里所有活道具 state 置 3 = 真空飞行 ⇒ 引擎原生收点：
 * 计分 / HUD / 计数 / 磁吸全走原生）。⚠ 只动 state、不动 pos/vel ⇒ 视觉上道具朝自机飞。 */
static void senro_border_collect(senro_card_t *s)
{
    int n = ce_item_collect_all();
    /* ★★ 2026-09-16：**自建的信仰点（110/111 的绿点）也一起收** —— 与 111 的两条全屏收同一个根因
     *   （`ce_item_collect_all()` 只扫原生道具池，而绿点是自建 VM）。一手里 TH10/TH11 的绿点
     *   **就是道具** ⇒ 「结界自动回收道具」在语义上包含它们。详见 `faith.h` 的 `faith_collect_all()`。
     *   ⚠ 这一行是 107 的行为变更（本轮与 111 的同类 bug 一起修）；不想要就删掉它一行。*/
    n += faith_collect_all(~0u);
    if (n) {
        s->stat_collect += n;
        if (s->frame - s->collect_log_at >= SENRO_LOG_EVERY) {
            s->collect_log_at = s->frame;
            ce_log("senro: 结界全收 %d 个道具（含绿点；累计 %d）", n, (int)s->stat_collect);
        }
    }
}

/* ---- 灵击（中弹 / 按 B）：结束结界 + 全屏消弹转小樱点 + 0.67 s 无敌 ----
 * 执行体 = th07 的 `0x441bd0`（中弹入口 `0x440d1d`、按 B 入口 `0x4419d2`）。
 *
 * ⚠ 全屏消弹走**「本帧已消过弹」闩锁**（EVENTS-CONTRACT §5.2 细则 2：107 的结界灵击与 117 的霊撃
 *   同帧都触发时只有第一家动手）。117 的消弹发生在霊撃的**后续帧**（`animal.c` 的 `hyper_frame`
 *   每帧 `ce_cancel_radius`），`+0x10` 那一帧不 claim ⇒ 「107 拿这笔账」在现况下天然成立；
 *   若将来有卡在 `+0x10` 就消弹，就需要 SDK 提供层内顺序（已登记为跨卡缺口）。
 *
 * ★★ `by_hit` 那一路**不能在死亡栈里扫弹池**（本卡 spec §6.3 的一手依据）：
 *   中弹的调用链是「弹自己的更新函数 `FUN_004248a0` → `0x42490b`/`0x42493f` call `0x45cc20`
 *   （自机受击）→ `Player__die` `0x45d3a0` → `0x45d574 call [vt+0x10]`」——
 *   也就是说我们的 `+0x10` 回调**跑在某颗弹的更新中途**（`esi` = 那颗弹，回调返回后同函数还在用它）。
 *   在这里 `ce_cancel_radius` 会把整池弹标成已消（含当前那颗）⇒ 与调用方的栈帧抢同一块状态。
 *   ⇒ **死亡路只做「不收尾就会丢」的事**（结束结界 / 40 帧无敌 / `cancel_impending_death` 救命），
 *   把全屏消弹+小樱点入账推迟到**下一帧的 `on_tick_2`**（管理器 tick，非弹池循环内，1 帧 ≈ 16 ms，
 *   玩家不可能察觉；`mof.c` / `pofv.c` / `animal.c` 的消弹也都只在 tick 里做）。
 *   实跑要点见本卡 spec §9 第 3 条。 */
static void senro_do_libyou(ce_card_t *c, senro_card_t *s, int by_hit)
{
    const float *pp = ce_player_pos();
    float pos[3];
    int n = 0;

    if (!senro_border_active(&s->st)) return;             /* 没结界：不动作（按 B 照常放原生炸弹）*/
    if (!pp) return;
    pos[0] = pp[0]; pos[1] = pp[1]; pos[2] = pp[2];

    if (by_hit) {
        /* 中弹路：当场收尾（不算弹数），消弹交给下一帧 `senro_tick_2` 的 pending 分支。 */
        (void)senro_libyou(&s->st, 0);
        s->hit_cancel_pending = 1;
    } else {
        /* 按 B 路：tick 上下文，直接扫弹池（先例 = mof.c 的灵击 / animal.c 的霊撃）。 */
        if (ce_cancel_latch_claim(SENRO_ID)) {
            n = ce_cancel_radius(pos, SENRO_CANCEL_R, SENRO_CANCEL_MAX, SENRO_CANCEL_MODE);
            /* ★ 2026-09-13（原生卡冲突审计 G 通道）：**消到 0 颗 ⇒ 交回闩锁**（原生 47/19/20/51
             *   之类先清了池、或池本来就空）。闩锁 = 「本帧有人真的消过弹」；空认领不该把同帧
             *   后面的卡白挡一帧（见 `sdk_core.h` 的 ce_cancel_frame_* 块注释）。*/
            if (n <= 0) (void)ce_cancel_latch_release(SENRO_ID);
        } else {
            ce_log("senro: 本帧消弹已被别的卡认领（卡 %d；本层本帧共消 %d 颗）→ 灵击只收结界",
                   ce_cancel_latch_owner(), ce_cancel_frame_count());
        }
        /* ★★ 2026-09-18：樱点那一笔**不再在这里记** —— mode 1 已经让引擎为每颗弹撒了 type 9
         *   小分道具（= 本卡的 ★ 通道，收下时 `on_item_collect` 给 +20 樱点/+C）。这里只留下
         *   **诊断颗数**（`stat_cancel` 原来由 `senro_small_cherry()` 顺带累加，现在自己累）。*/
        s->st.stat_cancel += n;
        (void)senro_libyou(&s->st, 0);
    }

    ce_invuln_request(SENRO_ID, s->st.invuln);
    ce_invuln_apply();                                   /* 中弹路发生在 Player tick，晚于 SDK 的落笔点 ⇒ 当场补一笔 */
    senro_handle_events(c, s, senro_take_events(&s->st));
    /* ★ 灵击的视觉（原生 effect type 0x1c「环」+ type 0x1d「32 颗」，见文件中部长注）：
     *   原版在同一个函数里连做三件 —— `th07:0x441c19`（环）+ `th07:0x441dd9`–`0x441e38`（32 颗）
     *   + 两条 SE；本卡同款，但**只置 pending**（`by_hit` 那一路跑在死亡栈里，不许建 VM）：
     *     · 按 B 路：本帧尾部 `senro_fx_flush()` 兑现 ⇒ **同帧**看得见；
     *     · 中弹路：下一帧兑现（≈16 ms，与既有的「中弹消弹推迟一帧」同一口径，spec §9 第 3 条）。 */
    s->fx_pending |= SENRO_FX_PEND_FIELD | SENRO_FX_PEND_RING;
    ce_play_sound(SENRO_SE_LIBYOU, pp[0]);
    /* ★ 2026-09-20：一手那一支是**两枚**（`th07:0x441e43` se_tan00 紧接着 `0x441e51` se_bonus）⇒ 照抄。*/
    ce_play_sound(SENRO_SE_LIBYOU2, pp[0]);
    ce_screen_shake(SENRO_SHAKE_TIME, SENRO_SHAKE_START, SENRO_SHAKE_END);
    if (by_hit) senro_cancel_impending_death();           /* 救援：取消本次死亡（EVENTS-CONTRACT §4.2 ②层）*/
    ce_log("senro: 灵击（%s）消弹 %d 颗 → 原生小分道具（★，收下每颗 +%d 樱点/+C）；当前樱点 %d / 最大樱点 %d，无敌 %d 帧",
           by_hit ? "中弹" : "按 B", n, (int)SENRO_STAR_CHERRY, (int)s->st.cherry, (int)s->st.cherry_max,
           (int)SENRO_INVULN_FRAMES);
}

/* 中弹那一笔的**下一帧补做**：全屏消弹 + 小樱点入账（理由见 `senro_do_libyou` 的长注）。*/
static void senro_hit_cancel_flush(ce_card_t *c, senro_card_t *s)
{
    const float *pp = ce_player_pos();
    float pos[3];
    int n = 0;

    if (!s->hit_cancel_pending) return;
    s->hit_cancel_pending = 0;
    if (!pp) return;
    pos[0] = pp[0]; pos[1] = pp[1]; pos[2] = pp[2];
    if (ce_cancel_latch_claim(SENRO_ID)) {
        n = ce_cancel_radius(pos, SENRO_CANCEL_R, SENRO_CANCEL_MAX, SENRO_CANCEL_MODE);
        if (n <= 0) (void)ce_cancel_latch_release(SENRO_ID);   /* ★ 空认领交回（G 通道，同上注）*/
    } else {
        /* ★ 2026-09-13：让位 = **只让「这次全屏消弹由我做」**，**不是**「本帧没消过弹」。
         *   本帧确实有人先动手（`ce_cancel_frame_count()` 是实打实的颗数）⇒ 我们的账 = 0 颗；
         *   但**状态机（事件队列）照常推进**，不能因为没抢到闩锁就把这一笔静默丢掉
         *   （原实现在这里 `return`，于是 `senro_handle_events()` 整段被跳过）。*/
        ce_log("senro: 中弹灵击的补做消弹让位（本帧已由卡 %d 认领，本层本帧共消 %d 颗）",
               ce_cancel_latch_owner(), ce_cancel_frame_count());
    }
    /* ★★ 2026-09-18：同按 B 路 —— 樱点由引擎撒出来的 type 9 小分道具（★）自己带，
     *   这里只累诊断颗数（`add` 那笔小樱点入账已撤，见 `senro_do_libyou` 的注）。*/
    s->st.stat_cancel += n;
    senro_handle_events(c, s, senro_take_events(&s->st));
    ce_log("senro: 中弹灵击补做消弹 %d 颗 → 原生小分道具（★，收下每颗 +%d 樱点/+C）（樱点 %d / +C %d）",
           n, (int)SENRO_STAR_CHERRY, (int)s->st.cherry, (int)s->st.plus);
}

/* ---- 每帧（管理器侧；菜单 / 商店里不跑）---- */
/* ★ 2026-09-18：`on_tick_2` 每帧补调一次（中盘拿卡时 `on_load` 不会被调），定义在文件后半。*/
static void senro_claim_hud(senro_card_t *s);
/* ★ 2026-09-18：樱点实体（共享点池的注册 / 满 P 转换 / 池推进）—— 定义在文件后半（`on_load` 之前）。*/
static void senro_faith_tick(ce_card_t *c, senro_card_t *s);
static void senro_faith_claim(ce_card_t *c);
/* ★ 2026-09-18：小樱点爆发（boss 死亡消弹），定义在文件后半。*/
static void senro_small_burst(senro_card_t *s, int32_t bullets);

static int on_tick_2(ce_card_t *c)
{
    senro_card_t *s = st(c);
    int32_t g, dg;
    int spell_now;

    if (!s || !CE_PLAYER()) return 0;
    ++s->frame;
    /* ★★ 2026-09-18（用户实跑）：**中盘拿到卡（十二合一）时引擎只调 `ctor`、不调 `on_load`**
     *   ⇒ 这里从没认领过 HUD 槽 ⇒ 107 的 UI 不出现，要等换面（`on_load`）才补上。
     *   `senro_claim_hud()` 幂等（`_card` 入口只在关卡号变化时重排）⇒ 每帧补一次直到成功。*/
    if (!s->slot_ok) senro_claim_hud(s);
    /* ★★ 2026-09-18：共享点池（樱点实体）—— 注册是幂等的（中盘拿卡时 `on_load` 不会被调），
     *   转换与池推进放在最前面：本帧转出来的大樱点当帧就建 VM、就受物理与收集判定。*/
    senro_faith_tick(c, s);

    /* ⓿ 中弹灵击的补做（上一帧在死亡栈里记下的）——必须在 tick 之前，消弹与入账同一帧完成。 */
    senro_hit_cancel_flush(c, s);

    /* ① 无敌声明：窗口内每帧声明**剩余**帧数（层取最大值），窗口外退掉声明。 */
    if (s->st.invuln > 0) {
        ce_invuln_request(SENRO_ID, s->st.invuln);
        ce_invuln_apply();
        --s->st.invuln;
    } else {
        ce_invuln_request(SENRO_ID, 0);
    }

    /* ② 结界计时 / +C 比例 / 自然结束。对话中冻结（th07 的结界更新挂在 Player tick 上，
     *    对话里引擎同样不推进游玩；口径见本卡 spec §6.1）。 */
    if (!CE_GUI_MSG_ACTIVE()) {
        senro_handle_events(c, s, senro_tick(&s->st, CE_BOMB_ACTIVE() ? 1 : 0));
        if (senro_border_active(&s->st)) senro_border_collect(s);   /* ③ 结界中自动回收道具 */
    }

    /* ④ 擦弹差分 → 结界中同时涨樱点与最大樱点（th07:0x43ed5f–0x43eda9，高速 80 / 低速 30）。
     *    差分读原生擦弹计数（先例：`cards/lolk.c` / `cards/season.c` / `cards/mof.c` 都这么做；
     *    review §9.7 结论 2 已确认 107 **不需要**逐弹擦弹断点）。 */
    g = senro_graze_now();
    if (s->st.graze_last < 0 || g < s->st.graze_last) s->st.graze_last = g;
    dg = g - s->st.graze_last;
    s->st.graze_last = g;
    if (dg > 0) {
        int32_t add = senro_graze(&s->st, dg, CE_PLAYER_FOCUSED() ? 1 : 0);
        if (add) {
            s->stat_gain_graze += add;
            senro_handle_events(c, s, senro_take_events(&s->st));
        }
    }

    /* ⑤ 按 B（X 上升沿）⇒ 灵击（th07 的按 B 入口 `0x4419d2`）。原版炸弹照放，本卡只做结界那一半。 */
    if ((*(const uint32_t *)CE_ADDR_INPUT_PRESSED & SENRO_INPUT_BOMB) && senro_border_active(&s->st))
        senro_do_libyou(c, s, /*by_hit=*/0);

    /* ⑥ 符卡奖励次数（th07 樱点（大）道具那项的 `[globals+0x1c]`）：
     *    照 `cards/mof.c` 的符卡跟踪写法 —— 奖励位（`CE_SPELL_FLAG_BONUS`，engine.h 一手：
     *    「bit1 奖励存活」）在符卡结束时还活着 = 收掉了这张符卡 ⇒ 记一次。 */
    spell_now = ce_spell_active();
    if (spell_now && !s->spell_was) s->spell_bonus_alive = 0;
    if (spell_now && (ce_spell_flags() & CE_SPELL_FLAG_BONUS)) s->spell_bonus_alive = 1;
    /* ★★ 2026-09-18：**小樱点**的触发（见上面常量处的长注）——符卡结束的**下降沿** + 本帧有一大笔
     *   **无人认领**的消弹（`ce_cancel_latch_owner()` = −1 ⇒ 不是任何卡自己消的）。
     *   ⚠ 必须在下面那段符卡奖励记账**之外**判：奖励位是否活着与这一笔无关。*/
    if (!spell_now && s->spell_was) {
        const int32_t cn = ce_cancel_frame_count();
        if (cn >= SENRO_SMALL_TRIGGER_MIN && ce_cancel_latch_owner() < 0)
            senro_small_burst(s, cn);
    }
    if (!spell_now && s->spell_was && s->spell_bonus_alive) {
        ++s->st.spell_bonuses;
        ce_log("senro: 符卡奖励第 %d 次（樱点（大）道具从此 +%d/颗）",
               (int)s->st.spell_bonuses, (int)(SENRO_ITEM_BIG_BASE + SENRO_ITEM_BIG_PER_BONUS * s->st.spell_bonuses));
    }
    s->spell_was = spell_now;

    /* ⑦ 计分声明（粘性 ⇒ 每帧刷新一次即可；樱点变了当帧就生效）。 */
    senro_declare_score(s);

    /* ⑧ 诊断汇总（实跑对账用）。 */
    if (s->frame - s->log_frame >= SENRO_LOG_EVERY) {
        s->log_frame = s->frame;
        ce_log("senro: cherry %d/%d, +C %d, border %s%s (%d 帧剩余), 开 %d 自然结 %d 灵击 %d 消弹 %d 道具 +%d 擦弹 +%d 命中 +%d 特效 环%d 灵击%d",
               (int)s->st.cherry, (int)s->st.cherry_max, (int)s->st.plus,
               s->st.active ? "on" : "off", s->st.queued ? " (queued)" : "",
               (int)s->st.timer, (int)s->st.stat_open, (int)s->st.stat_natural, (int)s->st.stat_libyou,
               (int)s->st.stat_cancel, (int)s->stat_gain_item, (int)s->stat_gain_graze,
               (int)s->stat_gain_hit, (int)s->stat_fx_field, (int)s->stat_fx_ring);
        if (s->stat_star || s->stat_star_dropped || s->stat_cherry_conv || s->stat_small)
            ce_log("senro: ★掉落实体 %d 颗（失败 %d）、大樱点实体 %d 颗、小樱点实体 %d 颗（折 %d 颗弹）",
                   (int)s->stat_star, (int)s->stat_star_dropped, (int)s->stat_cherry_conv,
                   (int)s->stat_small, (int)s->stat_small_bullets);
    }

    /* ⑨ 结界特效：先推进（老的一批按公式外扩 / 到点收句柄），再兑现本帧新置的 pending——
     *    这样「本帧起的」下一帧才开始动（与原生 effect VM 的帧计数从 0 起同形）。
     *    ⚠️ 位置：**必须**在 ②（开结界事件）与 ⑤（按 B 灵击）之后，否则当帧的 pending 要等下一帧。 */
    senro_fx_tick(s);
    senro_fx_flush(s);
    /* ⑩ ★ 2026-09-30：结界持续期的**压暗遮罩**（脚本 369）。放在最后 = 本帧的开关门事件
     *    （②）已经被处理过 ⇒ 开结界那一帧就起、出结界那一帧就撤。*/
    senro_dim_tick(s);
    return 0;
}

/* ---- 道具收集（TH18 item type；映射表见本卡 spec §4.1）---- */
static void on_item_collect(ce_card_t *c, uint32_t type, const void *item)
{
    senro_card_t *s = st(c);
    int32_t d;

    (void)item;
    if (!s) return;
    d = senro_item_cherry(&s->st, type);
    if (!d) return;
    s->stat_gain_item += d;
    senro_handle_events(c, s, senro_take_events(&s->st));
}

/* ---- 自机弹命中 → 樱点（断点 0x45f2a2）----
 * 用 `on_shot_damage_any`（`public.h` 的 `ce_hooks_t`；同一个断点、同一份实参，**不按聚焦过滤**）：
 * TH07 那条命中公式（`th07:0x4211bd`–`0x42132d`）**没有聚焦项**，聚焦/非聚焦的差别只体现在
 * `damage` 本身 ⇒ 本卡不再按 `focused` 分流（旧版的「高速 ×3 / 低速 ×1」是 ❓自定，第四轮撤掉），
 * 入参改成**关卡号**（一手 `p = (stage >= 5) ? 10 : stage × 2`，`0x420638`）。
 * ⚠ 两个钩子**会各收一次**（public.h 明写）⇒ 这里**只**声明 `on_shot_damage_any`，
 *   `on_shot_damage` 这个函数体**不再存在** —— 从构造上排除「同一帧同一敌记两次账」。
 * ⚠ bomb 生效帧两个钩子都不发（SDK 边界）；本函数不读输入位（回放安全）。 */
static void on_shot_damage_any(ce_card_t *c, int32_t damage, const float *pos, int focused)
{
    senro_card_t *s = st(c);
    int32_t add;

    (void)pos;
    if (!s) return;
    /* ★ 2026-09-30 第四轮：入参从「聚焦」改成**关卡号**（一手公式按 `0x62f85c` 分档，见 core 的注）；
     *   `focused` 不再参与折算（一手那条公式里没有聚焦项），此处显式丢弃。*/
    (void)focused;
    add = senro_hit(&s->st, damage, CE_CURRENT_STAGE());
    if (!add) return;
    s->stat_gain_hit += add;
    senro_handle_events(c, s, senro_take_events(&s->st));
}

/* ---- Bomb 用掉（断点 0x4203bc，`consume_bomb` 之后）⇒ 扣樱点（th07:0x440ad9）---- */
static void on_bomb_spent(ce_card_t *c)
{
    senro_card_t *s = st(c);
    int32_t d;

    if (!s) return;
    d = senro_penalty_bomb(&s->st);
    senro_declare_score(s);
    ce_log("senro: Bomb −樱点 %d → %d（+C 与最大樱点不动）", (int)d, (int)s->st.cherry);
}

/* ---- 中弹（虚表 +0x10：死亡已发起、可取消）----
 * 用户 2026-09-12 裁决（EVENTS-CONTRACT §4.1 / §5.2）：森罗结界属**救援层**，
 * 中弹 = 消耗结界救命；一次中弹把当前所有救援型提供者都烧掉（107 结束结界、117 结束咆哮），
 * 动作顺序 107 → 117。本卡只负责自己那一半（117 在自己的 +0x10 里做它那一半）。 */
static int on_death_before_deathbomb(ce_card_t *c)
{
    senro_card_t *s = st(c);
    if (!s) return 0;
    if (!senro_border_active(&s->st)) return 0;
    /* ★ 救援层报到（用户 2026-09-12 定案：顺序 107 → 117 → 决死 B → 113）。
     * ⚠️⚠️ **返回值必须丢弃**（`(void)` 不是手滑）：用户 2026-09-12 重申
     *   「**107 和 117 同时触发的时候，得要 107 和 117 同时破除**」⇒ 两家都必须**无条件**破自己的资源，
     *   谁也不能靠「先 claim 到账」把另一家挡掉。
     * 这个闩锁的**唯一消费者是 113**（它是最后一家，只在 `ce_rescue_note_owner() < 0` 时才动手）。
     * ⇒ 谁把这里改成 `if (ce_rescue_note_claim(...))`，就是让先到的那家独占救援 = 违规。*/
    (void)ce_rescue_note_claim(SENRO_ID);
    senro_do_libyou(c, s, /*by_hit=*/1);
    return 0;   /* 返回值被 Player__die 忽略；救命靠 cancel_impending_death 的副作用 */
}

/* ---- 真死（虚表 +0x14，死亡结算之后）⇒ Miss 扣樱点（th07:0x440e84–0x440ef0）----
 * 走到这里说明灵击没能救（结界本来就没开）⇒ Miss 与 Bomb 的扣减各走各的。 */
static int on_death_frame2(ce_card_t *c)
{
    senro_card_t *s = st(c);
    int32_t d;

    if (!s) return 0;
    d = senro_penalty_miss(&s->st);
    senro_declare_score(s);
    ce_log("senro: Miss −樱点 %d → %d（+C 与最大樱点不动；th07 的 miss 链不夹回基准）",
           (int)d, (int)s->st.cherry);
    return 0;
}

/* +C 指示器的颜色（`th07:0x404c00`–`0x404ca2`）：结界外是固定的樱粉；**结界中**退成
 * `R = 0xFF` 固定、`G = B = Plus×0xc0/50000 + w×64/2000` ⇒ 槽越空越红（这是「槽还剩多少」的唯一视觉），
 * 另叠一个 4000 周期的呼吸（`w` = `Plus % 4000` 的三角波，≥2000 折回）。
 * 一手：`R = 0xff` 在 `0x404c27`；三角波在 `0x404c2e`–`0x404c4d`（`idiv 0xfa0`，`cmp 0x7d0`）；
 * 两个加项各一条 `imul 0xc0` + `idiv 0xc350`（G：`0x404c50`–`0x404c73`；B：`0x404c7c`–`0x404ca2`），
 * 两处都 `shl eax,6` / `idiv 0x7d0` 加呼吸项。⚠ 原版是**按字节写**（`mov cl,[+0x89d]`）⇒ 和可能到 256
 * 就截断成 0（`plus == 50000` 那一帧正好命中）—— 这里照抄这个截断（`& 0xff`），不做 clamp。*/
static uint32_t senro_hud_plus_color(const senro_state_t *s, int border)
{
    int32_t v, w, ch;

    if (!s || !border) return SENRO_HUD_COLOR_PLUS_IDLE;
    v = s->plus;
    if (v < 0) v = 0;
    if (v > SENRO_PLUS_FULL) v = SENRO_PLUS_FULL;
    w = (int32_t)(v % 4000);
    if (w >= 2000) w = 4000 - w;
    ch = (int32_t)(((int64_t)v * 0xc0) / SENRO_PLUS_FULL) + (w * 64) / 2000;
    ch &= 0xff;                                   /* 原版的字节截断，见上 */
    return 0xff000000u | (0xffu << 16) | ((uint32_t)ch << 8) | (uint32_t)ch;
}

/* ---- HUD（原版版式：上一行 +C、下一行「樱点 + 最大樱点」；版式出处见文件上方长注）----
 * 用**贴图 VM 字形**（TH07 `ascii.anm` 的 8×12 数字，走 UI 层 25 ⇒ 暂停时被模糊罩盖住；
 * 先例 = `cards/season.c` / `cards/mof.c` 的贴图 VM 文字）。坐标是 640×480 字面量空间，
 * 由 HUD 槽位分配器给的 x/y 推（HUD-LAYOUT.md §1.1 的两套空间）。
 * ★ 坐标**每帧取最新值**（2026-09-12 审计补）：`ce_hud_slot_claim()` 填给 `out` 的是**认领那一刻**的
 *   排布，同一开场批次里后来的认领会按 card_id 重排（`sdk_core.c` 的 `hud_relayout`；`sdk_core.h:180`
 *   原话「卡片应…用本函数取最新值，或每帧取一次」）⇒ 只认领时取一次、之后一直用缓存的那份，会让
 *   数字画到**别人的块**上（比本卡 id 小的卡后认领时本卡的 x 会右移）。 */
static int on_draw(ce_card_t *c)
{
    senro_card_t *s = st(c);
    uint32_t a, col_plus, col_cherry;
    ce_hud_slot_t cur;
    float bx, by, tx, ty, x, y, scale_plus, pitch_plus;
    int slot = 0, i, n_was, border, digits_max;

    /* ★★ 2026-09-12 对抗审计补的门：`on_draw`（虚表 +0x48）**在主菜单 / 标题里也会被调**
     *   （`AbilityManager__on_draw` `0x408ab0` 每帧遍历卡链表调它；`on_tick_2` 才是「菜单/商店不跑」）。
     *   实跑先例：117 `animal.c:1953`「用户在**主菜单**看到 5 个空槽灵魂」、115 `lolk.c:708` 主菜单冒出 `+0`。
     *   本卡原来的第一道门是 `if (!s || !s->slot_ok)` —— 而 `slot_ok` 一旦在 `on_load`/`on_run_reset` 置上
     *   就再也不会清 ⇒ **通关回主菜单后三行会照画**（同 lolk 那个洞）。⇒ 补 `CE_ADDR_GAME_THREAD_PTR`
     *   那道门（与 `lolk.c:708` / `season.c:1347` / `th06rank.c` 同款）。**顺手把 VM 收掉**
     *   （`senro_hud_clear()` 只 delete 已有的 VM，无 VM 时是空操作）—— 不收的话会留着上一帧的字。
     *   ⚠ 只在「不是正常游玩帧」时收：正常帧每帧都会重画，不必每帧 delete+spawn。 */
    if (!*(volatile uint32_t *)CE_ADDR_GAME_THREAD_PTR) {
        senro_hud_clear();
        return 0;
    }
    if (!s || !s->slot_ok) { senro_hud_clear(); return 0; }
    /* ⚠ `ce_hud_slot_get()` **只填 x/y/w/h/side，不填 x640/y640**（`sdk_core.c`）⇒ 拿不到时退回认领时的
     * 缓存副本，换算一律走 `CE_HUD_SURFACE_TO_640`（= 0.5），不读那两格。*/
    cur = s->slot;
    (void)ce_hud_slot_get(SENRO_ID, &cur);
    /* ★★ 2026-09-15g：压暗 alpha 改由**跨卡统一件**给（本卡唯一的一处压暗路 = 这堆 HUD 数字；
     * 原来这里是 `ce_hud_player_near_corner()` 的硬切 64/255）。放在这一行 = 已经过了上面两道门
     * （游戏线程 + `slot_ok`）⇒ 每帧正好推进一次。*/
    {
        hud_dim_rect_t r;
        hud_dim_rect_surface(&r, cur.x, cur.y, SENRO_HUD_W, SENRO_HUD_H);
        a = (uint32_t)hud_dim_tick(&s->hud, &r, ce_player_pos()) & 0xffu;
    }

    border = senro_border_active(&s->st) ? 1 : 0;
    bx = cur.x * CE_HUD_SURFACE_TO_640;             /* 本块左下角（640 字面量空间）*/
    by = cur.y * CE_HUD_SURFACE_TO_640;
    tx = bx + SENRO_HUD_T_OFF_X;                    /* 原版锚 T（= 标签 VM2 的 pos）在本卡里的落点 */
    ty = by + SENRO_HUD_T_OFF_Y;

    n_was = s_hud.n;
    s_hud.n = 0;

    /* ⓪ ★★ **数字左边那块固定标签**（TH07 的 VM2 / `ascii.anm` sprite142，脚本 308）——
     *    **必须先于数字**：TH07 是每帧「标签 → 数字 → 脉动花」这个次序调的（`th07:0x404910`
     *    → `0x404a6c` → `0x404e60`），而 TH18 的同层叠放 = **生成顺序**（尾插、后来者压在上面）
     *    ⇒ 标签早一拍起，才会被数字压住（原版的观感就是「Cherry / 加号 / 斜杠在数字底下」）。
     *    位置 = 锚 `T` 本身（原版 script4 把它 posTime 到 (32,449)，其余全部按它算）；
     *    **左上角锚**（script4 的 `ins_22()`，脚本 308 里写死 `anchor(1,1)`）⇒ 盒 = `T .. T+96`。
     *    颜色：白 + 压暗 alpha —— 原版 VM2 的 alpha 就是**整块计数器的 alpha**（数字那位是
     *    `mov al,[VM2+0x653]; mov [VM3+0x89f],al` 抄过去的，`th07:0x404963`）⇒ 这里让标签与
     *    数字共用同一个 `a`，等于照抄那条"抄 alpha"的链。 */
    if (s_hud.label_id && !ce_anm_get_vm(s_hud.label_id)) s_hud.label_id = 0;      /* 自愈 */
    if (!s_hud.label_id)
        s_hud.label_id = senro_hud_spawn(CE_ANM_ABILITY_SCRIPT_SENRO_HUD_LABEL, tx, ty);
    else {
        float dat = senro_hud_dat();
        (void)ce_anm_set_pos(s_hud.label_id, tx * dat, ty * dat, 0.0f);
    }
    if (s_hud.label_id)
        (void)ce_anm_set_color(s_hud.label_id, 0x00ffffffu | (a << 24));

    /* ① 下一行：**樱点**（6 位定宽，前导零留空）—— 颜色三档（`th07:0x40496f`–`0x4049eb`）：
     *    满最大樱点 ⇒ 金黄；否则「樱点 ≥ 50000」⇒ 亮黄；否则白。
     *    （本卡的樱点本身就是「相对基准」的读数 ⇒ 直接比 50000。）*/
    col_cherry = (s->st.cherry >= s->st.cherry_max)  ? SENRO_HUD_COLOR_CHERRY_MAXED
                 : (s->st.cherry >= SENRO_PLUS_FULL) ? SENRO_HUD_COLOR_CHERRY_READY
                                                     : SENRO_HUD_COLOR_CHERRY;
    x = tx + SENRO_HUD_TX_CHERRY;
    y = ty + SENRO_HUD_TY_CHERRY;
    x = senro_hud_field(&slot, x, y, SENRO_HUD_PITCH, SENRO_HUD_SCALE,
                        (col_cherry & 0x00ffffffu) | (a << 24), s->st.cherry, SENRO_HUD_FIELD_CHERRY);

    /* ② 同行的**最大樱点**：紧跟樱点字段 **+9 单位**（`th07:0x404ac8`）⇒ 字段起点固定
     *    （樱点字段是定宽的），位数 = 6，值 ≥ 1000000 时 7（`th07:0x404ae0` 那一支）。*/
    digits_max = (s->st.cherry_max >= 1000000) ? 7 : SENRO_HUD_FIELD_CHERRY;
    (void)senro_hud_field(&slot, x + SENRO_HUD_GAP_MAX, y, SENRO_HUD_PITCH, SENRO_HUD_SCALE,
                          (SENRO_HUD_COLOR_MAX & 0x00ffffffu) | (a << 24), s->st.cherry_max, digits_max);

    /* ③ 上一行：**+C**（5 位定宽）。结界外的样子与结界中完全不同（这就是「森罗结界指示」的主体）：
     *    · 结界外：樱粉、字距 7、倍率 1.0、位置 T+(53,2)
     *    · 结界中：`R=0xFF` + 呼吸的 G/B（见 `senro_hud_plus_color`）、字距 10、倍率 1.41、
     *      整体 +2 / −2（`th07:0x404cab`–`0x404cf9`）*/
    col_plus = senro_hud_plus_color(&s->st, border);
    pitch_plus = border ? SENRO_HUD_PITCH_BORDER : SENRO_HUD_PITCH;
    scale_plus = SENRO_HUD_SCALE * (border ? SENRO_HUD_PLUS_SCALE : 1.0f);
    x = tx + SENRO_HUD_TX_PLUS + (border ? SENRO_HUD_PLUS_SHIFT : 0.0f);
    y = ty + SENRO_HUD_TY_PLUS - (border ? SENRO_HUD_PLUS_SHIFT : 0.0f);
    (void)senro_hud_field(&slot, x, y, pitch_plus, scale_plus,
                          (col_plus & 0x00ffffffu) | (a << 24), s->st.plus, SENRO_HUD_FIELD_PLUS);

    /* ④ 结界指示的**脉动花**（TH07 的 VM4 = `ascii.anm` sprite143，脚本 307 自己无限脉动）。
     *    `ds:0x4bfee5 == 1` 才画；位置 = `T + (24, 8)`（**居中锚** ⇒ 盒 = `T .. T+48` × `T.y .. T.y+16`，
     *    正好压在标签左半那三件东西上 —— 原版画在**最后**，所以花在最上面）。
     *    颜色跟着压暗 alpha 走（贴图是粉色半透明花，脚本里没写 color ⇒ 这里每帧写一次）。*/
    if (border) {
        float px = tx + SENRO_HUD_BORDER_DX, py = ty + SENRO_HUD_BORDER_DY;
        if (s_hud.border_id && !ce_anm_get_vm(s_hud.border_id)) s_hud.border_id = 0;   /* 自愈 */
        if (!s_hud.border_id)
            s_hud.border_id = senro_hud_spawn(CE_ANM_ABILITY_SCRIPT_SENRO_HUD_BORDER, px, py);
        else {
            float dat = senro_hud_dat();
            (void)ce_anm_set_pos(s_hud.border_id, px * dat, py * dat, 0.0f);
        }
        if (s_hud.border_id)
            (void)ce_anm_set_color(s_hud.border_id, 0x00ffffffu | (a << 24));
    } else {
        senro_hud_vm_kill(&s_hud.border_id);
    }

    /* ⑤ 收掉本帧没用到的槽（定宽字段 ⇒ 正常总是 17 个；这里是「池满 / VM 起不来」时的兜底）。*/
    for (i = s_hud.n; i < n_was; ++i) { senro_hud_vm_kill(&s_hud.id[i]); s_hud.shown[i] = -1; }

    /* ⑥ ★ 2026-09-30：进入结界的公告（TH07 GUI kind 2 = `Supernatural Border!!`）。
     *    年龄在**这里**推进 —— `on_draw` 就是「本帧画一次」的相位（TH07 同样每帧重发这条公告，
     *    见 `engine/spirit/th07/01-barrier-visual.md` 与 `senro.c` 头的 `SENRO_TEXT_*` 段）。*/
    if (s->text_age >= 0) {
        senro_text_draw(s);
        if (++s->text_age > SENRO_TEXT_LIFE) s->text_age = -1;
    }
    return 0;
}

/* ---- 生命周期 ---- */

static void senro_clear(ce_card_t *c, int keep_slot)
{
    senro_card_t *s = st(c);

    /* ★ 顺序要紧：先退出两个声明层（否则被删的卡还在声明无敌 / 得分），再收 HUD 数字 VM。 */
    ce_invuln_request(SENRO_ID, 0);
    ce_score_declare_delta(SENRO_ID, 0);
    senro_hud_clear();
    if (!s) return;
    /* ★ 世界层 VM 是本卡自己的（`ce_layer_drop_card` 只管声明层）⇒ 必须在 memset 之前收干净。
     *   删卡发生在**商店里**（倒吊人 / 原版空白卡清空整副牌），那一刻 tick 全停、只有 `on_delete` 跑得到
     *   —— 这条 memset 就是最后一道网（句柄一旦被 memset 掉，那些 VM 就永远留在场上了）。 */
    senro_fx_clear(s);
    /* ★ 2026-09-30：持续期那三件的收尾（压暗 VM / 自机本体色还原 / 公告）——同样必须在 memset 之前
     *   （句柄一被 memset 掉，那个全屏 VM 就永远留在场上了，`METHOD.md` 铁律二）。*/
    senro_period_clear(s);
    /* ★ 2026-09-18：樱点实体（共享池里**自己槽**的点）。顺序同 110/111 —— **先退注册、再收实体**，
     *   且只收自己那一槽（`faith_clear_entities()` 会连 110/111 的点一起清掉）。*/
    faith_unregister(SENRO_ID);
    faith_clear_entities_mask(1 << FAITH_SLOT_107);
    memset(s, 0, sizeof *s);
    s->st.graze_last = -1;
    s->spell_was = 0;
    /* ★★ 2026-09-30 实跑修：「进入结界的那行字在**刚进游戏**就出现一次」——根因 = **memset 把它写成 0**
     *   （0 = 年龄 0 = 正在播），而 `senro_period_clear()` 在 memset **之前**跑（它设的 −1 被抹掉了）。
     *   ⇒ 「不播」的哨兵必须在 memset **之后**再写一次（与上面 `graze_last` / `spell_was` 同一条纪律）。*/
    s->text_age = -1;
    s->dim_alpha = -1;
    hud_dim_init(&s->hud);   /* ★ 2026-09-15g：压暗状态复位 = **满亮 255**（共享件；memset 出来是 0）*/
    (void)keep_slot;         /* 槽位由 `CE_CARD` 的 on_run_reset / opdelete 自动 release（§6.1）*/
}

static int on_ctor(ce_card_t *c)
{
    senro_card_t *s = st(c);
    if (!s) return 0;
    senro_clear(c, 0);
    senro_init(&s->st, senro_difficulty());
    senro_declare_score(s);
    ce_log("senro: ctor (difficulty %d, cherry_max 初值 %d, 结界 %d 帧, 满槽 %d)",
           senro_difficulty(), (int)s->st.cherry_max, (int)SENRO_BORDER_FRAMES, (int)SENRO_PLUS_FULL);
    return 0;

}

/* HUD 槽认领（`on_load` 与 `on_run_reset` 共用 —— 后者刚 memset 掉整块状态，
 * 而 `on_draw` 要靠槽位坐标定位，不重认领就会把数字画到屏幕左上角）。
 *
 * ★★ 2026-09-30（owner 实跑第 2 条）：「**左下角 HUD 位置不对，得再往左，参考 117、112、113 的左下角
 *   位置，那个才是真正的画面左下角**」—— 原来走**货架分配器**（`ce_hud_slot_claim_card`），
 *   而货架按 card_id 升序从锚点 `x = 257` 起排 ⇒ 107 落在 **257**（实跑日志
 *   `senro: on_load (hud slot ok @(257,924) 表面px)`），比那三张卡（112 = 68 / 110+111 = 64 /
 *   117 = 88，全是 owner 按截图量过、走 `ce_hud_slot_claim_pinned_card` 的**钉住**位）右了近 200 px。
 *   ⇒ 改成**钉住**（与 112/117/110+111 同一条路），钉在 **(68, 928)**：
 *     · `X = 68` = **112 的块左沿**（`ufo.c`：`UFO_HUD_PIN_X = 248 + (−180) = 68`）= **原生「当前卡」UI
 *       的左沿**（`HUD-LAYOUT.md` §1.3b：68..116）—— owner 同一天给 108 的同一句口径
 *       （「参考 117/112/113 的左下角位置……你就按照那个调就是了」）在 `youkai.c` 里算出的也是 **68**
 *       ⇒ 两张卡共用这条竖线。（TH07 的 `T.x` 逐值换算是 64 = 640 空间的 32，差 4 表面 px；
 *       要回到「逐值复刻」就把 `SENRO_HUD_PIN_X` 改回 64。）
 *     · `Y = 928` = **游玩区下沿**（与 112 的 `UFO_HUD_PIN_Y`、113 的 `TRANCE_HUD_BASE_Y` 同一条底线）
 *       ⇒ `T_OFF_Y = −15` 让 `T.y = 449` = **原版值本身**。
 *     ★ 钉住位**不由货架重排**（owner 量过的固定位置，先例 = 112/117）。
 *   ⚠ 块宽仍报 **200**（真实 286）：右半会压到右邻块 —— owner 2026-09-30：「UI互相覆盖目前可接受，
 *     最后统一处理」⇒ 本轮不动块宽、不动货架算法。*/
static void senro_claim_hud(senro_card_t *s)
{
    ce_hud_block_t blk;

    blk.w = SENRO_HUD_W;
    blk.h = SENRO_HUD_H;
    blk.side = CE_HUD_SIDE_PINNED;                             /* ★ 必须：非 pinned 会被 pinned 入口拒掉 */
    s->slot_ok = ce_hud_slot_claim_pinned_card(SENRO_ID, &blk, SENRO_HUD_PIN_X, SENRO_HUD_PIN_Y, &s->slot);
    if (!s->slot_ok) {
        /* 认领失败 ⇒ SDK 不会写 `out`，自己把兜底常量填进去（否则每帧读到的都是垃圾值）。
         * 兜底 = 钉住位本身（与 117 的失败分支同款）。*/
        s->slot.x = SENRO_HUD_PIN_X;
        s->slot.y = SENRO_HUD_PIN_Y;
        s->slot.w = blk.w;
        s->slot.h = blk.h;
        s->slot.side = CE_HUD_SIDE_PINNED;
    }
}

/* ==================== ★★ 樱点道具实体（自建 VM，挂在共享点池上，2026-09-18）====================
 *
 * 用户口径（逐字）：「**th07是有一个我叫做樱点的掉落实体…肯定有一个类似于信仰点的仅在 th07 的
 *   类 P 点掉落实体，似乎没有自建 VM 实现**」＋「**吸附半径得统一到 th18 的吸附**」＋
 *   「**大樱点就得在与 th10-11 同装的时候被同时转化出来，并且如果大樱点只有这一个来源的话，
 *   也被认为是等价于 P 点**」。
 *
 * 为什么**不另写一层运动学**：共享点池 `cards/faith.c`（110/111 已在用）的飞散 / 飘落 / 吸附 / 收取
 *   逐条都是 **TH18 自机字段**口径（收集半径 `player+0x4798c`、吸引半径 `+0x47990|94`、磁速 `+0x47988`、
 *   回收线 `+0x47998`）⇒ 用户口径「吸附统一到 th18」直接成立，且**只有一份实现**。
 *
 * 三个来源（一手 `engine/item/th07/01-sakura-items.md` §2/§3.2）：
 *   · **大樱点**（type 7 = `1000 + 100×符卡奖励次数`）⇒ 本卡的实体：**满 P 转换**那条共享通道
 *     （`faith_convert_full_p_items()`）撒 1 颗 —— 与 110/111 的产物同帧、同源（用户口径「同时转化出来」）；
 *   · ★（type 6，+20）⇒ **兼用 TH18 原生消弹小分道具**（`SENRO_CANCEL_MODE = 1`），本卡不建实体；
 *   · 小樱点（type 8，+100）⇒ TH07 的真实来源是 boss 死亡那颗 `mode 0xa` 消弹，TH18 侧没有对应实体
 *     ⇒ 本卡当前**没有**这个来源（`senro_small_cherry()` 暂留，见 `TUNING.md` §2.0k）。
 *
 * 外观 = TH07 原版切图（`SAKURA_POINT`：sprite 287 / 脚本 186）—— 走共享件 2026-09-18 新增的
 *   `faith_user_t.art_sprite/art_script`（**自带外观**；老注册者不设 ⇒ 行为逐字节不变）。
 *
 * ⚠ 长持续效果的自查（`METHOD.md` 铁律二）：本卡跨帧留下的东西多了一项 —— **共享池里自己槽的点**
 *   ⇒ `senro_clear()` 里 `faith_unregister(SENRO_ID)` + `faith_clear_entities_mask(1 << FAITH_SLOT_107)`
 *   （顺序同 110/111：**先退注册、再收实体**；删卡发生在商店里，那一刻只有 `on_delete` 跑得到）。*/
/* ---- ★ 小樱点（TH07 type 8）：boss 死亡 / 符卡结束那一大笔消弹 ----
 * 值（一手 `th07:0x433742`+`0x43374e`）：樱点 +30 **且** +C +30（加法器）、再只加樱点 +70 ⇒ 每颗 **+100**；
 * 本函数把它按「总值不变、颗数封顶」打包到 `SENRO_SMALL_MAX_SPAWN` 颗实体上（见常量处的注）。*/
static void senro_small_burst(senro_card_t *s, int32_t bullets)
{
    const float *pp = ce_player_pos();
    const int32_t per = SENRO_SMALL_CHERRY_ADD + SENRO_SMALL_CHERRY_EXTRA;   /* 100 */
    int32_t total, bundle;
    int k, i, made = 0;

    if (!pp || bullets <= 0) return;
    total = bullets * per;                                  /* 一手的**总**樱点量 */
    k = (bullets > SENRO_SMALL_MAX_SPAWN) ? SENRO_SMALL_MAX_SPAWN : (int)bullets;
    bundle = total / k;                                     /* 每颗带的打包值（余数忽略，≤ k−1 点）*/

    for (i = 0; i < k; ++i) {
        /* 自机为中心撒成一圈（❓ 自定：TH18 到这一刻弹已经没了，拿不到逐颗原位）——与灵击那 32 颗同形。*/
        const float ang = SENRO_ITEM_SPAWN_ANGLE + (6.2831853f * (float)i) / (float)k;
        if (faith_spawn_art(pp[0], pp[1], FAITH_TIER_SMALL, 1 << FAITH_SLOT_107, bundle, ang,
                            CE_ANM_ABILITY_SPRITE_SAKURA_SMALL,
                            CE_ANM_ABILITY_SCRIPT_SAKURA_SMALL)) {
            ++made;
        }
    }
    s->stat_small += made;
    s->stat_small_bullets += bullets;
    ce_log("senro: boss 死亡消弹 %d 颗 → 小樱点实体 %d 颗（每颗打包 %d；樱点总量 +%d）",
           (int)bullets, made, (int)bundle, (int)(bundle * made));
}

/* ---- ★ 死敌 ×1 ★（TH07 §3.2 一手）----
 * 门 = `CE_EDEATH_F_REAL`（真敌人）—— 一手那条「`enemy[+0x2e29] >> 5 & 1` 才算掉」在我们侧的对位；
 * **不按 kind 过滤**：TH07 的死亡批处理对「被打死 / 炸弹杀 / ECL `enmKillAll`」一视同仁
 * （§3.2 的四个调用者：`EnemyManager::on_tick_0a` / 符卡结束 / op94 / 关卡收尾）。
 * ⚠ 副作用（登记在 `TUNING.md` §2.0m）：type 9 是原生「分数转换点族」⇒ 除了本卡的 +20 樱点/+C，
 *   还会给玩家它自己那点分（原生行为，我们不改 type 分派表）。*/
static void on_enemy_death(ce_card_t *c, const ce_enemy_death_t *ev)
{
    senro_card_t *s = st(c);
    float pos[3];

    if (!s || !ev) return;
    if (s->star_off) return;
    if (!(ev->flags & CE_EDEATH_F_REAL)) return;
    pos[0] = ev->x; pos[1] = ev->y; pos[2] = 0.0f;         /* 敌人原位、无抖动（一手 §3.2）*/
    if (!ce_spawn_item((uint32_t)SENRO_TH18_ITEM_STAR, pos,
                       SENRO_ITEM_SPAWN_ANGLE, SENRO_ITEM_SPAWN_SPEED)) {
        ++s->stat_star_dropped;                            /* 道具池满：丢弃（与原生撒落同口径）*/
        return;
    }
    ++s->stat_star;
}

static void senro_on_cherry_pickup(void *user, int slot_mask, int tier, int32_t value,
                                   float x, float y)
{
    ce_card_t *c = (ce_card_t *)user;
    senro_card_t *s = st(c);
    int32_t d;

    (void)tier; (void)value; (void)x; (void)y;   /* 值由本卡自己算（随符卡奖励次数变）*/
    if (!s) return;
    if (!(slot_mask & (1 << FAITH_SLOT_107))) return;      /* 只认自己槽 */

    if (tier != (int)FAITH_TIER_BIG) {
        /* ★ 小樱点档（TH07 type 8）：**打包值**在 `value` 里（见 `senro_small_burst`），
         * 一手拆法是「+30 走加法器（樱点与 +C 同加）、+70 只加樱点」⇒ 按比例拆同一笔。*/
        int32_t plus_part = value * SENRO_SMALL_CHERRY_ADD
                          / (SENRO_SMALL_CHERRY_ADD + SENRO_SMALL_CHERRY_EXTRA);
        s->stat_gain_item += value;
        senro_add_cherry(&s->st, plus_part);
        senro_add_cherry_only(&s->st, value - plus_part);
        senro_handle_events(c, s, senro_take_events(&s->st));
        return;
    }

    /* th07 type 7（`th07:0x433886`–`0x4338d9`）：樱点与 +C **各** `1000 + 100×[globals+0x1c]`。
     * 走 `senro_add_cherry()` ⇒ 两个计数器一起加（与既有的金钱通道同一个加法器）。*/
    d = SENRO_ITEM_BIG_BASE + SENRO_ITEM_BIG_PER_BONUS * s->st.spell_bonuses;
    s->stat_gain_item += d;
    senro_add_cherry(&s->st, d);
    senro_handle_events(c, s, senro_take_events(&s->st));
    ce_log("senro: 收下大樱点（th07 type 7）→ 樱点与 +C 各 +%d（符卡奖励 %d 次）",
           (int)d, (int)s->st.spell_bonuses);
}

/* 注册（幂等；同 110 的 `mof_ensure_registered()` 口径 —— 中盘拿卡时 `on_load` 不会被调）。*/
static void senro_faith_claim(ce_card_t *c)
{
    senro_card_t *s = st(c);
    faith_user_t u;

    if (!s) return;
    memset(&u, 0, sizeof u);
    u.card_id      = SENRO_ID;
    u.slot_mask    = 1 << FAITH_SLOT_107;
    u.on_pickup_at = senro_on_cherry_pickup;
    /* 满 P 转换通道的**开关**（判据 = `conv_value > 0`）。值本身不用：本卡在回调里自己算
     * `1000 + 100×符卡奖励次数`（随符卡推进变，而池的 `value` 是每注册者一个常数）。*/
    u.conv_value   = 1;
    u.art_sprite   = CE_ANM_ABILITY_SPRITE_SAKURA_POINT;   /* 287 = TH07 etama2 sprite11（大樱点）*/
    u.art_script   = CE_ANM_ABILITY_SCRIPT_SAKURA_POINT;   /* 186 = TH07 script60 的转写 */
    u.user         = c;
    if (!faith_register(&u)) ce_log("senro: faith_register failed (table full?)");
}

/* 满 P 转换 + 共享池推进（**顺序是硬的**：转换放在池 tick 之前 ⇒ 当帧撒的点当帧就建 VM / 受物理）。
 * 两个函数都自带帧守卫（共享件的口径）⇒ 与 110/111 同装时**一帧只跑一遍**。*/
static void senro_faith_tick(ce_card_t *c, senro_card_t *s)
{
    int n;

    senro_faith_claim(c);                     /* 幂等：每帧补一次，直到注册成功 */
    n = faith_convert_full_p_items();
    if (n > 0) {
        s->stat_cherry_conv += n;
        ce_log("senro: 满 P 转换 %d 颗原生 P 道具 → 大樱点实体 ×%d（累计 %d；与 110/111 同源同帧）",
               n, n, (int)s->stat_cherry_conv);
    }
    (void)faith_pool_tick();
}

static int on_load(ce_card_t *c)
{
    senro_card_t *s = st(c);

    if (!s) return 0;
    senro_claim_hud(s);
    senro_faith_claim(c);            /* ★ 2026-09-18：注册进共享点池（樱点实体）*/
    s->st.graze_last = -1;
    senro_declare_score(s);
    ce_log("senro: on_load (hud slot %s @(%.0f,%.0f) 表面px)", s->slot_ok ? "ok" : "FAIL",
           (double)s->slot.x, (double)s->slot.y);
    return 0;
}

static void on_stage_start(ce_card_t *c)
{
    senro_card_t *s = st(c);
    if (!s) return;
    /* th07 每关重设基准（`th07:0x4043b9` 樱点 = 基准；最大樱点按难度表重取）⇒ 本卡每关从 0 起。 */
    senro_stage_reset(&s->st, senro_difficulty());
    senro_fx_clear(s);       /* ★ 上一关留下的结界特效（环 / 32 颗）不跨关 */
    senro_period_clear(s);   /* ★ 2026-09-30：压暗 VM / 本体色 / 公告同样不跨关 */
    faith_clear_entities_mask(1 << FAITH_SLOT_107);   /* ★ 樱点实体同样不跨关（2026-09-18）*/
    s->frame = 0;
    s->log_frame = 0;
    s->maxed_popup_at = 0;
    s->spell_was = 0;
    s->spell_bonus_alive = 0;
    s->stat_collect = s->stat_gain_item = s->stat_gain_graze = s->stat_gain_hit = 0;
    s->stat_fx_field = s->stat_fx_ring = 0;
    senro_declare_score(s);
}

static void on_run_reset(ce_card_t *c)
{
    senro_card_t *s = st(c);
    ce_log("senro: run reset");
    senro_clear(c, 0);
    if (s) {
        senro_init(&s->st, senro_difficulty());
        senro_claim_hud(s);                          /* 见 senro_claim_hud 的注：状态被 memset 过，必须重认领 */
    }
}

/* ⚠ 删对象（局末 recount / 局中倒吊人 / 原版空白卡）：SDK 先调它、再释放私有状态。
 *   那一刻 AbilityManager 冻住 ⇒ 只有这里能收 HUD 文字与两个声明层（`CE_CARD` 的 opdelete 会
 *   顺手 `ce_layer_drop_card`，但文字 VM 是本卡自己的）。 */
static void on_delete(ce_card_t *c)
{
    ce_log("senro: on_delete（收 HUD 文字 + 退出无敌 / 计分声明）");
    senro_clear(c, 0);
}

/* ---- 每游玩帧（道具池 body 前一次；`BP_ce_item_frame` 0x446ee3，优先级 0x1e）----
 * ★ 2026-09-30：**只有自机本体红白闪挂在这里**，理由是**相位**：引擎自己在 Player tick（0x17）
 *   里写体色（无敌闪色），而 Player::on_draw 在 0x1d ⇒ 本卡的写入必须落在两者之间才不被覆盖，
 *   这一站（0x1e）正好在那个窗口里（先例 = `cards/youkai.c` 的同一相位用法）。*/
static void on_item_frame(ce_card_t *c)
{
    senro_player_flash(st(c));
}

CE_CARD(107,
        .ctor = on_ctor,
        .on_load = on_load,
        .on_stage_start = on_stage_start,
        .on_tick_2 = on_tick_2,
        .on_item_frame = on_item_frame,
        .on_item_collect = on_item_collect,
        .on_item_score = on_item_score,
        .on_shot_damage_any = on_shot_damage_any,
        .on_bomb_spent = on_bomb_spent,
        .on_death_before_deathbomb = on_death_before_deathbomb,
        .on_death_frame2 = on_death_frame2,
        .on_enemy_death = on_enemy_death,      /* ★ 2026-09-18：死敌掉 ★（TH07 §3.2）*/
        .on_draw = on_draw,
        .on_run_reset = on_run_reset,
        .on_delete = on_delete);
