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
 * ③ HUD 槽位 + 三行贴图 VM 文字；④ **★ 结界特效的两个世界层 VM**（甲 = 那个环的句柄 + 乙 = 32 颗的句柄，
 * 2026-09-12 补，见文件中部长注）。①② 由 `CE_CARD` 的 `on_run_reset` / `opdelete` 自动 `ce_layer_drop_card()` 清，
 * ③ 的槽位也自动 release，但**文字 VM 是本卡自己的**，必须在 `on_delete` / `on_run_reset` 里 `ce_hud_vm_text_clear()`。
 * ④ 同理（`ce_layer_drop_card` 只清声明层）⇒ `senro_clear()` 里的 `senro_fx_clear()` 把 33 个句柄全 `ce_anm_delete`。
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
                                              *       ⇒ 「消了 n 颗」≠「拿到 n 颗 ★」。

/* 音效：两作的 SE 表 id **不同义** ⇒ 老实标 ❓ 自定（th07 用的是 0x21 与 7）。
 * TH18 侧名字与调校见 `engine/_shared/th18-sound-table.md`。*/
#define SENRO_SE_BORDER_OPEN        0x2f     /* ❓ 自定：`se_bonus.wav`（th18-sound-table.md:204）——
                                              * th07 开结界那一支（`th07:0x441960` 尾段）没找到播音调用。TODO(th07)*/
#define SENRO_SE_BORDER_END         0x2f     /* ❓ 自定：同上。th07 自然结束播 SE 0x21（`th07:0x4416f4`）；
                                              * TH18 的 0x21 = `se_cat00`（th18-sound-table.md:190）语义不符 ⇒ 用 bonus。TODO(th07)*/
#define SENRO_SE_LIBYOU             0x09     /* ❓ 自定：`se_cancel00.wav`（th18-sound-table.md:166）。
                                              * th07 灵击播 SE 7 + 0x21（`th07:0x441e43` / `th07:0x441e51`）。TODO(th07)*/

/* 震屏：th07 的打断在 `th07:0x441d35`/`0x441dd9` 起 32 个环形特效单元（没有全屏震动的一手证据）
 * ⇒ 参数是**表现占位**（与 `cards/mof.c:183` 的灵击同款）。TODO(th07)*/
#define SENRO_SHAKE_TIME            20
#define SENRO_SHAKE_START           12
#define SENRO_SHAKE_END             0

/* HUD：左下角三行（+C / 樱点 / 最大樱点）。块尺寸报给槽位分配器（表面 px，HUD-LAYOUT.md §2/§3）。*/
#define SENRO_HUD_W                 200.0f
#define SENRO_HUD_H                 40.0f
#define SENRO_HUD_ROW_DY            12.0f   /* 行距（640×480 字面量空间；行高 = scale_y × 10）*/
/* ★★ 2026-09-15g：**压暗这一路改走跨卡统一件 `cards/hud_dim.{h,c}`**（唯一实现）。
 * 性质 = ❓ **用户口径（UI 统一），不是复刻**（用户 2026-09-15：「压暗这个涉及到 UI 统一，
 * 这个不能复刻。是那个靠近透明吧」）⇒ **动态**统一：终点 alpha **32**（原 64）、起始 255、
 * 每帧 **±56**（4 帧；原来**没有过渡**，当帧硬切）、滞回带 **16 px**；**区域**按**本卡自己那块 HUD 的矩形**推。
 *
 * ★★ 2026-09-15h：**进/出的口径订正**（父 agent：15g 那版「进 = 矩形**之内**」裁错了）——
 *   **进 = 自机落在「块矩形外扩 `HUD_DIM_MARGIN`(28)」之内**、**出 = 离开「矩形 + 44」（28 + 滞回 16）之外**。
 *   理由：本块的**下沿 446 在自机 y 钳位 432 之下** ⇒ 「站进矩形」只剩 **6 px 高**；一手各作的门本来就落在
 *   它们自己那块上沿之上 24–32 px（TH16 的 `400` 在它那块沿之上 28）⇒ 边距是**原本就有的语义**。
 *
 * 本卡的块矩形 → 门限（**自机坐标**；换算 `自机 = 表面/2 − (320, 16)`，见 `hud_dim.h`）：
 *   · 块 = 货架槽（`s->slot`，表面 (257, 924) + **200 × 40** = `SENRO_HUD_W/H`）⇒ 表面 x 257..457、y 884..924
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
 *     · **结界中**：`R = 0xFF` 固定（`0x404c27`），`G = B = Plus×0xc0/50000`（两次除法 `0x404c5a`/`0x404c86`
 *       → 写 `+0x89d`/`+0x89c`）⇒ 随剩余时间由 `(0xFF,0xC0,0xC0)` 退到 `(0xFF,0,0)`
 *       （还叠一个 `%4000` 的三角波呼吸，`0x404c24`–`0x404c4d`）。
 *   TH18 的 `ce_hud_vm_text_set` 收的也是**同一个 D3DCOLOR**（`sdk.c` 原样写 `mgr+0x19224`）
 *   ⇒ 数值可以直接沿用，不用换序。*/
#define SENRO_HUD_COLOR_CHERRY_MAXED 0xffffd080u   /* RGB(0xFF,0xD0,0x80) 金黄 */
#define SENRO_HUD_COLOR_CHERRY_READY 0xffffff80u   /* RGB(0xFF,0xFF,0x80) 亮黄（樱点 − 基准 ≥ 50000）*/
#define SENRO_HUD_COLOR_CHERRY       0xffffffffu   /* 白 */
#define SENRO_HUD_COLOR_PLUS_IDLE    0xffc080b0u   /* RGB(0xC0,0x80,0xB0) 樱粉（结界外的 +C 指示器）*/
/* 樱点满最大樱点 / 自然结束 Bonus 的飘字：用原生那个「金黄」。 */
#define SENRO_HUD_COLOR_GOLD         0xffffd080u   /* RGB(0xFF,0xD0,0x80)（= 原生满值色）*/

/* 诊断日志间隔（游玩帧）。*/
#define SENRO_LOG_EVERY             600
/* 「樱点满值」飘字的最小间隔（th07 每加一次都飘，0x42f679；这里节流防刷屏）。*/
#define SENRO_MAXED_POPUP_GAP       30
/* ~~倍率弹字的第二行恒 1.0~~ —— **2026-09-18 已删**：那会让屏幕出现「BONUS 1.0」
 * （用户两次实跑都报「bonus 1.0」）。现在改走**数字槽**（只一行数字），见 `senro_popup()` 的注释。*/

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
/* pending 位（死亡栈里只置位、tick 里兑现）。 */
#define SENRO_FX_PEND_FIELD         1
#define SENRO_FX_PEND_RING          2

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
    uint32_t         fx_field_id;       /* 甲：结界展开那个大环（脚本 204）的 VM 句柄 */
    int32_t          fx_field_frame;    /* 甲的帧计数（≥ SENRO_FIELD_LIFE 就清句柄）*/
    uint32_t         fx_ring_id[SENRO_RING_UNITS];  /* 乙：32 颗（脚本 205）*/
    int32_t          fx_ring_frame;     /* 乙的帧计数（32 颗同帧起 ⇒ 共用一个）*/
    int32_t          fx_ring_live;      /* 1 = 这一批还活着 */
    int32_t          fx_pending;        /* 死亡栈里只置位，`senro_fx_flush()` 在 tick 尾兑现 */
    float            fx_home[3];        /* 乙的起点（= 起的那一刻的自机坐标，照 th07:0x441df8）*/
    int32_t          stat_fx_field;     /* 诊断：起过几次甲 */
    int32_t          stat_fx_ring;      /* 诊断：起过几批乙（×32 颗）*/
    int32_t          stat_collect;      /* 诊断：结界期间自动收点总数 */
    int32_t          stat_gain_item;    /* 诊断：道具通道加的樱点总数 */
    int32_t          stat_gain_graze;   /* 诊断：擦弹通道 */
    int32_t          stat_gain_hit;     /* 诊断：命中通道 */
} senro_card_t;

_Static_assert(sizeof(senro_card_t) <= 496, "ce_state 上限 496 字节（超了会静默返回 NULL）");

/* HUD 的三行文字（DLL 静态区；与 season.c / mof.c 同口径 —— 一张卡同时只在场一个对象）。*/
static ce_hud_vm_text_t s_hud_plus;
static ce_hud_vm_text_t s_hud_cherry;
static ce_hud_vm_text_t s_hud_max;

static senro_card_t *st(ce_card_t *c) { return ce_state(c, senro_card_t); }

static int32_t senro_graze_now(void) { return *(volatile int32_t *)SENRO_ADDR_GRAZE; }

static void senro_cancel_impending_death(void)
{
    void *p = CE_PLAYER();
    if (p) ((void (__attribute__((thiscall)) *)(void *))SENRO_FN_CANCEL_IMPENDING_DEATH)(p);
}

/* 难度（0 EASY..3 LUNATIC；= 0x4ccd00，`cards/mof.c` / `cards/th06rank.c` 同址，**只读**）。
 * th07 的最大樱点初值按难度查表（`th07:0x42eb7a`–`0x42ec57`），本卡照搬同一张表。*/
static int senro_difficulty(void) { return *(volatile int32_t *)SENRO_ADDR_DIFFICULTY; }

/* ---- 飘字（TH18 原生 PopupManager 的**数字槽** k0–12；字段语义一手见 engine.h 的
 * `CE_POPUP_*` 注，逐字段照 `cards/mof.c` 的 `mof_popup_number()` 同族抄）----
 *
 * ★★ 2026-09-18（用户实跑「又出现 bonus 1.0」）：**原来这里写的是倍率槽（k13–17）**，
 *   理由是「TH18 只有 `%d` + `BONUS %.1f` 两种格式，表达不出 th07 的 `Border Bonus %7d`
 *   （`th07:0x4416dd` kind=4）⇒ Bonus 分放进 `%d` 行、第二行恒 1.0」——**那是错的**：
 *   倍率槽**永远画两行**，于是屏幕上出现「22220 / **BONUS 1.0**」这种读不通的东西
 *   （×1.0 的 bonus = 没有 bonus），用户两次报的「bonus 1.0」都是它。
 *   ⇒ 改用**数字槽**：只画一行数字（= 那笔奖励分），不再有第二行。
 *   ⚠ th07 原版那一行是**文字 + 数字**（`Border Bonus %7d`）；TH18 这条弹字路径画不了文字
 *     ⇒ 这里只保留数字（❓ 自定：要完全复刻得把 "Border Bonus" 做成 anm 贴图另起一台 VM，
 *     登记在 `TUNING.md` §2.7 的待办里）。*/
static int senro_popup(const float *pos, int32_t value, uint32_t color)
{
    uint8_t *mgr = CE_POPUP_MGR();
    int k;

    if (!mgr || !pos) return 0;
    for (k = 0; k < CE_POPUP_NUM_SLOTS; ++k) {
        uint8_t *s2 = mgr + CE_POPUP_RING + (size_t)k * CE_POPUP_STRIDE;
        if (*(uint8_t *)(s2 + CE_POPUP_BUSY)) continue;              /* 占着：引擎自己会回收 */
        *(int32_t *)(s2 + CE_POPUP_AGE - 4) = -1;
        *(int32_t *)(s2 + CE_POPUP_AGE) = 0;
        *(int32_t *)(s2 + CE_POPUP_AGE + 4) = 0;
        if (!(*(uint32_t *)(s2 + 0x30) & 1u)) {                       /* 动画类标志 bit0（照 adder）*/
            *(uint32_t *)(s2 + 0x30) |= 1u;
            *(int32_t *)(s2 + 0x2c) = 0;
        }
        *(float *)(s2 + CE_POPUP_POS) = pos[0];
        *(float *)(s2 + CE_POPUP_POS + 4) = pos[1];
        *(float *)(s2 + CE_POPUP_POS + 8) = pos[2];
        *(float *)(s2 + 0x18) = 1.0f;                                 /* scale（弹出动画起点）*/
        *(uint32_t *)(s2 + CE_POPUP_COLOR) = color;
        *(int32_t *)(s2 + CE_POPUP_VALUE) = value;   /* 数字槽：draw 只画这一行（`%d`）*/
        *(uint8_t *)(s2 + CE_POPUP_BUSY) = 1;
        return 1;
    }
    return 0;
}

static void senro_popup_at_player(int32_t value, uint32_t color)
{
    const float *pp = ce_player_pos();
    if (pp) (void)senro_popup(pp, value, color);
}

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
}

/* 甲：在 `pos` 起一个（照 `th07:0x441aa4`/`0x441c19`：**先杀旧的再起新的**，见 `th07:0x441be5`）。
 * 立刻摆一次位置（世界层 (0,0,0) = 场地正中 ⇒ 不摆会先在正中闪一帧）。 */
static void senro_fx_field_spawn(senro_card_t *s, const float *pos)
{
    uint32_t id;

    senro_fx_field_kill(s);
    id = ce_anm_spawn(CE_ABILITY_ANM(), SENRO_ANM_SCRIPT_FIELD, SENRO_FIELD_ANM_LAYER);
    if (!id) {
        ce_log("senro: 结界环 VM 起不来（脚本 %d、层 %d）—— 机制不受影响，只是这一半看不见",
               SENRO_ANM_SCRIPT_FIELD, SENRO_FIELD_ANM_LAYER);
        return;
    }
    s->fx_field_id = id;
    s->fx_field_frame = 0;
    ++s->stat_fx_field;
    if (pos) (void)ce_anm_set_pos(id, pos[0], pos[1], 0.0f);
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
    if (s->fx_pending & SENRO_FX_PEND_FIELD) senro_fx_field_spawn(s, pp);
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
        if (s->fx_field_frame >= SENRO_FIELD_LIFE || !ce_anm_get_vm(s->fx_field_id))
            senro_fx_field_kill(s);
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
        s->fx_pending |= SENRO_FX_PEND_FIELD;
        ce_play_sound(SENRO_SE_BORDER_OPEN, ce_player_x());
        ce_log("senro: 森罗结界 OPEN (%d frames, cherry %d/%d, +C %d)",
               (int)SENRO_BORDER_FRAMES, (int)s->st.cherry, (int)s->st.cherry_max, (int)s->st.plus);
    }
    if (ev & SENRO_EV_BORDER_QUEUE)
        ce_log("senro: +C 满但正在放炸弹 → 排队（th07 [player+0x240d]=2 口径）");
    if (ev & SENRO_EV_END_NATURAL) {
        /* 结算表现照一手（`th07:0x441670`）：分数 += (樱点 − 基准)、Border Bonus 飘字、SE。*/
        ce_add_score_delta(s->st.last_bonus);
        ce_play_sound(SENRO_SE_BORDER_END, ce_player_x());
        senro_popup_at_player(s->st.last_bonus, SENRO_HUD_COLOR_GOLD);
        ce_log("senro: 结界自然结束 → 樱点 %d / 最大樱点 %d，Border Bonus +%d",
               (int)s->st.cherry, (int)s->st.cherry_max, (int)s->st.last_bonus);
    }
    if (ev & SENRO_EV_CHERRY_MAXED) {
        if (s->frame - s->maxed_popup_at >= SENRO_MAXED_POPUP_GAP) {
            s->maxed_popup_at = s->frame;
            senro_popup_at_player(s->st.cherry, SENRO_HUD_COLOR_GOLD);
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
    }

    /* ⑨ 结界特效：先推进（老的一批按公式外扩 / 到点收句柄），再兑现本帧新置的 pending——
     *    这样「本帧起的」下一帧才开始动（与原生 effect VM 的帧计数从 0 起同形）。
     *    ⚠️ 位置：**必须**在 ②（开结界事件）与 ⑤（按 B 灵击）之后，否则当帧的 pending 要等下一帧。 */
    senro_fx_tick(s);
    senro_fx_flush(s);
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

/* ---- 自机弹计分命中（断点 0x45f2a2）----
 * ★ 2026-09-12：SDK 补了 `on_shot_damage_any`（`public.h` 的 `ce_hooks_t` 末尾；同一个断点 0x45f2a2、
 *   同一份实参，**不按聚焦过滤**，`focused` 由 SDK 传）⇒ 本卡的**两条通道都在它上面分流**：
 *     高速（!focused）与低速（focused）各走一次 `senro_hit`，「低速 ≈ 高速的 1/3」那半支不再是死路。
 * ⚠ 两个钩子**会各收一次**（public.h 明写）⇒ 这里**只**声明 `on_shot_damage_any` 并从
 *   `CE_CARD` 的挂表里撤掉 `on_shot_damage`，`on_shot_damage` 这个函数体**不再存在** ——
 *   从构造上排除「同一帧同一敌记两次账」。
 * ⚠ 数值语义与迁移前逐字相同：迁移前高速那半支走老钩子、`slow = CE_PLAYER_FOCUSED() ? 1 : 0`
 *   ——而老钩子的策略门恰好保证「到得了它 ⇒ !focused」⇒ 老路上 `slow` 恒 0。现在高速支仍传 0、
 *   低速支传 1（= `SENRO_HIT_SLOW_NUM/SENRO_HIT_FAST_NUM` 那对常量，senro_core.c 未改一个字）。
 * ⚠ 只读 `focused` 这一个入参、不读引擎输入位（回放安全）；bomb 生效帧两个钩子都不发（SDK 边界）。 */
static void on_shot_damage_any(ce_card_t *c, int32_t damage, const float *pos, int focused)
{
    senro_card_t *s = st(c);
    int32_t add;

    (void)pos;
    if (!s) return;
    add = senro_hit(&s->st, damage, focused ? 1 : 0);   /* 1 = 低速（聚焦）⇒ 走慢支；0 = 高速 */
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

/* +C 指示器的颜色（`th07:0x404c00`–`0x404cf9`）：结界外是固定的樱粉；**结界中**退成
 * `R = 0xFF` 固定、`G = B = Plus×0xc0/50000` ⇒ 槽越空越红（这是「槽还剩多少」的唯一视觉）。
 * 一手：两次除法 `0x404c5a` / `0x404c86`（`imul eax,eax,0xc0` 后 `idiv 0xc350`）→
 * 写 `+0x89d`(=G，`0x404c73`) / `+0x89c`(=B，`0x404ca2`)；`R = 0xff` 在 `0x404c27`。 */
static uint32_t senro_hud_plus_color(const senro_state_t *s)
{
    int32_t v;

    if (!s || !s->active) return SENRO_HUD_COLOR_PLUS_IDLE;
    v = (int32_t)(((int64_t)s->plus * 0xc0) / SENRO_PLUS_FULL);
    if (v < 0) v = 0;
    if (v > 0xc0) v = 0xc0;
    return 0xff000000u | (0xffu << 16) | ((uint32_t)v << 8) | (uint32_t)v;
}

/* ---- HUD（左下角三行：+C / 樱点 / 最大樱点）----
 * 用**贴图 VM 字形**（`ce_hud_vm_text_set`：TH18 font 2 的原生切图，走 UI 层 25 ⇒ 暂停时被模糊罩盖住；
 * 先例 = `cards/season.c` 的档位数字 / `cards/mof.c` 的三行）。坐标是 640×480 字面量空间，
 * 由 HUD 槽位分配器给的 `x640/y640` 推（HUD-LAYOUT.md §1.1 的两套空间）。
 * ★ 坐标**每帧取最新值**（2026-09-12 审计补）：`ce_hud_slot_claim()` 填给 `out` 的是**认领那一刻**的
 *   排布，同一开场批次里后来的认领会按 card_id 重排（`sdk_core.c` 的 `hud_relayout`；`sdk_core.h:180`
 *   原话「卡片应…用本函数取最新值，或每帧取一次」）⇒ 只认领时取一次、之后一直用缓存的那份，会让
 *   三行画到**别人的块**上（比本卡 id 小的卡后认领时本卡的 x 会右移）。111 一直是每帧取的，本卡原来漏了。 */
static int on_draw(ce_card_t *c)
{
    senro_card_t *s = st(c);
    char buf[32];
    uint32_t a, col_plus, col_cherry;
    ce_hud_slot_t cur;
    float x, y;

    /* ★★ 2026-09-12 对抗审计补的门：`on_draw`（虚表 +0x48）**在主菜单 / 标题里也会被调**
     *   （`AbilityManager__on_draw` `0x408ab0` 每帧遍历卡链表调它；`on_tick_2` 才是「菜单/商店不跑」）。
     *   实跑先例：117 `animal.c:1953`「用户在**主菜单**看到 5 个空槽灵魂」、115 `lolk.c:708` 主菜单冒出 `+0`。
     *   本卡原来的第一道门是 `if (!s || !s->slot_ok)` —— 而 `slot_ok` 一旦在 `on_load`/`on_run_reset` 置上
     *   就再也不会清 ⇒ **通关回主菜单后三行会照画**（同 lolk 那个洞）。⇒ 补 `CE_ADDR_GAME_THREAD_PTR`
     *   那道门（与 `lolk.c:708` / `season.c:1347` / `th06rank.c` 同款）。**顺手把三行 VM 收掉**
     *   （`ce_hud_vm_text_clear` 只 delete 已有的 VM，无 VM 时是空操作）—— 不收的话会留着上一帧的字。
     *   ⚠ 只在「不是正常游玩帧」时收：正常帧每帧都会 `ce_hud_vm_text_set` 重画，不必每帧 delete+spawn。 */
    if (!*(volatile uint32_t *)CE_ADDR_GAME_THREAD_PTR) {
        ce_hud_vm_text_clear(&s_hud_plus);
        ce_hud_vm_text_clear(&s_hud_cherry);
        ce_hud_vm_text_clear(&s_hud_max);
        return 0;
    }
    if (!s || !s->slot_ok) return 0;
    /* ★★ 2026-09-15g：压暗 alpha 改由**跨卡统一件**给（本卡唯一的一处压暗路 = 这三行文字；
     * 原来这里是 `ce_hud_player_near_corner()` 的硬切 64/255）。放在这一行 = 已经过了上面两道门
     * （游戏线程 + `slot_ok`）⇒ 每帧正好推进一次；矩形用**本帧**的 `cur`（上面刚 `ce_hud_slot_get()`）。*/
    {
        hud_dim_rect_t r;
        hud_dim_rect_surface(&r, cur.x, cur.y, SENRO_HUD_W, SENRO_HUD_H);
        a = (uint32_t)hud_dim_tick(&s->hud, &r, ce_player_pos()) & 0xffu;
    }
    /* ⚠ `ce_hud_slot_get()` **只填 x/y/w/h/side，不填 x640/y640**（`sdk_core.c`）⇒ 拿不到时退回认领时的
     * 缓存副本，换算一律走 `CE_HUD_SURFACE_TO_640`（= 0.5），不读那两格。*/
    cur = s->slot;
    (void)ce_hud_slot_get(SENRO_ID, &cur);
    x = cur.x * CE_HUD_SURFACE_TO_640;
    y = cur.y * CE_HUD_SURFACE_TO_640;
    col_plus = senro_hud_plus_color(&s->st);
    /* 樱点数（= 樱点 − 基准）：满最大樱点 ⇒ 金黄；否则「樱点 ≥ 50000」⇒ 亮黄；否则白。
     * （`th07:0x40496f`–`0x4049eb`；本卡的樱点本身就是「相对基准」的读数 ⇒ 直接比 50000。）*/
    col_cherry = (s->st.cherry >= s->st.cherry_max)  ? SENRO_HUD_COLOR_CHERRY_MAXED
                 : (s->st.cherry >= SENRO_PLUS_FULL) ? SENRO_HUD_COLOR_CHERRY_READY
                                                     : SENRO_HUD_COLOR_CHERRY;

    snprintf(buf, sizeof buf, "+%d", (int)s->st.plus);
    ce_hud_vm_text_set(&s_hud_plus, x, y - 2.0f * SENRO_HUD_ROW_DY,
                       (col_plus & 0x00ffffffu) | (a << 24), CE_ASCII_ALIGN_LEFT, 0.0f, buf, "senro");
    snprintf(buf, sizeof buf, "%d", (int)s->st.cherry);
    ce_hud_vm_text_set(&s_hud_cherry, x, y - SENRO_HUD_ROW_DY,
                       (col_cherry & 0x00ffffffu) | (a << 24), CE_ASCII_ALIGN_LEFT, 0.0f, buf, "senro");
    snprintf(buf, sizeof buf, "%d", (int)s->st.cherry_max);
    ce_hud_vm_text_set(&s_hud_max, x, y,
                       (SENRO_HUD_COLOR_CHERRY & 0x00ffffffu) | (a << 24), CE_ASCII_ALIGN_LEFT, 0.0f, buf, "senro");
    return 0;
}

/* ---- 生命周期 ---- */

static void senro_clear(ce_card_t *c, int keep_slot)
{
    senro_card_t *s = st(c);

    /* ★ 顺序要紧：先退出两个声明层（否则被删的卡还在声明无敌 / 得分），再收 HUD 文字。 */
    ce_invuln_request(SENRO_ID, 0);
    ce_score_declare_delta(SENRO_ID, 0);
    ce_hud_vm_text_clear(&s_hud_plus);
    ce_hud_vm_text_clear(&s_hud_cherry);
    ce_hud_vm_text_clear(&s_hud_max);
    if (!s) return;
    /* ★ 世界层 VM 是本卡自己的（`ce_layer_drop_card` 只管声明层）⇒ 必须在 memset 之前收干净。
     *   删卡发生在**商店里**（倒吊人 / 原版空白卡清空整副牌），那一刻 tick 全停、只有 `on_delete` 跑得到
     *   —— 这条 memset 就是最后一道网（句柄一旦被 memset 掉，那些 VM 就永远留在场上了）。 */
    senro_fx_clear(s);
    memset(s, 0, sizeof *s);
    s->st.graze_last = -1;
    s->spell_was = 0;
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
 * 而 `on_draw` 要靠 `slot.x640/y640` 定位，不重认领就会把三行画到屏幕左上角）。*/
static void senro_claim_hud(senro_card_t *s)
{
    ce_hud_block_t blk;

    blk.w = SENRO_HUD_W;
    blk.h = SENRO_HUD_H;
    blk.side = 0;                                              /* 左下角（HUD-LAYOUT §2 的货架）*/
    s->slot_ok = ce_hud_slot_claim_card(SENRO_ID, &blk, &s->slot);   /* 幂等：每关都会再调一次（§4.4）。
     * ★ 用 `_card` 入口：它会先把关卡号推进纪元 ⇒ 上一关被删卡留下的占位在**本关开场**收敛（HUD-LAYOUT §4.4）。*/
}

static int on_load(ce_card_t *c)
{
    senro_card_t *s = st(c);

    if (!s) return 0;
    senro_claim_hud(s);
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

CE_CARD(107,
        .ctor = on_ctor,
        .on_load = on_load,
        .on_stage_start = on_stage_start,
        .on_tick_2 = on_tick_2,
        .on_item_collect = on_item_collect,
        .on_item_score = on_item_score,
        .on_shot_damage_any = on_shot_damage_any,
        .on_bomb_spent = on_bomb_spent,
        .on_death_before_deathbomb = on_death_before_deathbomb,
        .on_death_frame2 = on_death_frame2,
        .on_draw = on_draw,
        .on_run_reset = on_run_reset,
        .on_delete = on_delete);
