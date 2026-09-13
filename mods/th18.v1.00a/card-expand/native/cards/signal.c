/* 地灵殿（TH11，card-expand id 111）—— **装备卡**：信号槽 + 绿点信仰 + 蓝点分 + 灵击。
 *
 * ## 设计依据（**先读这两篇，本文注释只指路、不重复推理**）
 *   · `docs/superpowers/specs/2026-09-10-sa-signal-design.md`（111 的设计定稿：§2.1 信号槽 /
 *     §2.2 绿点信仰 / §2.3 蓝点分 / §2.4 B 双选 / §2.5 HUD / §3 数值表）
 *   · `docs/superpowers/specs/2026-09-11-system-card-spec-review.md` §3.2.1（★ 灵击门槛 TH18 ≥ 2.00 档、
 *     扣 1 档、**下限 1.05 档**；小绿点每 100 个 +10、余数跨关保留）/ §7.1（绿点 = 同一个东西，
 *     分槽记账）/ §7.2（Δ_111 上限 = 原版基础）/ §7.4（PV 总式）/ §7.5（Δ_111 明细）/ §7.8（计分接法）
 *   · `mods/th18.v1.00a/card-expand/EVENTS-CONTRACT.md` §3（计分层：声明 → 聚合 → **id 最小者落笔**）
 *   · 实装期新结论与偏差：`docs/superpowers/specs/2026-09-12-signal-111-impl.md`
 *
 * ## 分工：本文件只是「一具身体」，逻辑都在共用件里（**一行都没重写**）
 *
 *   | 件 | 在哪 | 管什么 |
 *   | --- | --- | --- |
 *   | 绿点实体池（110/111 **共用**） | `faith.c` | 撒点、飞散 / 飘落 / 吸附、收集判定、按 `slot_mask` 回调 |
 *   | 绿点记账（按槽） | `faith_core.c` | `Δ_111` 加值 / 上限 = 基础 / 小绿点余数 / 信号槽夹取 |
 *   | 灵击门槛与扣法（110/111 **共用**） | `mof_core.c` | `mof_core_power_spend()`（≥2.00 档、扣 1 档、下限 1.05） |
 *   | 擦弹倍率（**111 用**；110 单装时不参与 PV）| `mof_core.c` | `mof_core_graze_mult()` = `min(graze, 8.99)` 千分点 |
 *   | PV 总式（110/111 共用，含三态） | `mof_core.c` | `mof_core_pv()` = 计分层契约 §3 的那条公式 |
 *   | 信号槽的 111 私有逻辑 | `signal_core.c` | 阈值 ≥100、强度 0.00–1.00、蓝点分乘数、HUD 格填充 |
 *   | **本文件** | — | 读引擎、调上面四层、`ce_score_declare` / HUD |
 *
 * ## 本卡在计分层里的角色（EVENTS-CONTRACT §3）
 *
 * 落笔者 = 声明者里 **id 最小** 的那张。110 在场 ⇒ **110 落笔**（它按契约公式读**聚合值**）；
 * 111 单装 ⇒ **111 落笔**。⇒ 111 的 `on_item_score` 只在 `ce_score_applier() == 111` 时动手。
 * `mof_core_pv()` 覆盖三态（只 110 / 只 111 / 两个都装），而契约的「两卡同装」式子是
 * `PV = (基础均值 + Δ_110 + Δ_111) × (擦弹倍率 + 信号强度)` —— 也就是 `mof_core_pv(…, 0)` × 乘数。
 * ⇒ 本文件传 `signal_value = 0` 拿到**基础 PV**，自己再乘 `(擦弹倍率 + 信号强度)`，
 *    与 110 的落笔路径（`signal_value = 0` 那一支）**完全一致**，两张卡谁落笔结果都一样。
 *
 * ## 明确不做（spec §6）
 *   · **残机碎片改制**（不覆盖 TH18 自己的命碎制，`ce_add_life_fragment()` 一律不调）；
 *   · 写 `MONEY`（蓝点分只加分数、不加钱）；
 *   · 伙伴主动技能 / 子机层（spec §9.1：子机位置走「低速子机声明层」，属另一批活）。
 *     ⚠ **2026-09-12 更新（108 落地的声明层，别再走老路）**：本卡今天**没有任何子机代码**
 *     （全文件不写 `option+0x64/+0x68/+0x6c/+0x70`；唯一相关调用是 `signal_bomb()` 里的
 *     `ce_repopulate_options()` —— 那是「扣了 1 档火力 ⇒ 池按新档重建」的原生要求，不是子机阵型写入
 *     ⇒ 本次「111 迁移」**没有可迁移的直写**）。将来做 §9.1 那个子机层时**只许声明**：
 *       `ce_option_pose_declare(c, &pose)`（每帧，逐格偏移）+ `on_option_pose_apply` 落笔
 *       （spec §3.3.1 / sdk.h 那一段）；**108(108) < 111(111)** ⇒ 同装时 108 落笔，
 *       本卡会在「退位通知」里被叫一次（此时 `ce_option_pose_winner() != 111`）⇒ 只收子机、不写字段。
 *       ⚠ 重建池之后本卡的槽指针会作废（`option+0x00` 被清）⇒ 落笔里用 `ce_option_get` 重新认领即可
 *       （门式：每帧重写偏移，不依赖任何「池刚重建」的通知）。
 *
 * ## 长持续效果的自查（`METHOD.md` 铁律二）
 * 本卡跨帧留下的东西：① 绿点 VM 池（共用件，静态区）；② HUD 槽位认领；③ 6+3 个 UI 层 VM。
 * **`on_delete` 与 `on_run_reset` 都走同一个 `signal_clear()`**：收池 + 退声明 + 收 VM + 释放槽位。
 * 删卡发生在**商店**里、那一刻 AbilityManager 冻住 ⇒ 只能靠 `on_delete`（虚表 `+0x50`），不能等下一帧。
 * ⚠ 共用池的收法（2026-09-12 审计改）：`faith_clear_entities()` 会清**整个**池（110 的绿点也在里面）
 *   ⇒ 改成 `faith_clear_entities_mask(1 << FAITH_SLOT_111)`（只收本卡槽里的点，`faith.h` 那一支的注释
 *   写了为什么必须这样）。原来那条「两侧都不该留下飘着的点」的口径已作废。
 * ★ `on_stage_start`（`+0x34`，每关开场）**只归零信号槽**（§2.1）：不动信仰、不动 VM、不动槽位。
 */
#include <stddef.h>          /* sdk.h 的 ce_item_collect_all 用了 size_t（内置 Linux 头不带）*/
#include <string.h>
#include <stdio.h>
#include "sdk.h"
#include "faith.h"
#include "signal_core.h"
#include "mof_core.h"        /* ★ 共用件：借 `mof_core_power_spend`（灵击）与 `mof_core_graze_mult`（擦弹倍率），
                              * **不改它**。⚠ 它的 `mof_core_pv()` 有两个分支写反（见 `on_item_score` 的注释与
                              * `docs/superpowers/specs/2026-09-12-signal-111-impl.md` §1）⇒ 本卡不用它算 PV。*/

/* ---- 本卡常量 ---- */
#define SIGNAL_ID_NUM          111      /* ⚠ `CE_CARD` 对 ID 做 ## 拼接 ⇒ 必须给**字面量**，
                                         * 不能写 `CE_CARD(SIGNAL_ID_NUM, …)`（会拼出 ce_hooks_SIGNAL_ID_NUM）*/
/* engine.h 没给这两个宏 ⇒ 就地补（**只读**，与 `mof.c` / `lolk.c` / `season.c` 同址同口径）。*/
#define SIGNAL_ADDR_GRAZE      0x4ccd0c   /* 原生擦弹计数（GLOBALS.inner+0x30）*/
#define SIGNAL_ADDR_DIFFICULTY 0x4ccd00   /* 难度（0 EASY..3 LUNATIC、4 EXTRA）*/
/* `CE_ADDR_MAX_POWER` 0x4ccd3c = GlobalsInner+0x60（engine.h 一手；`mof.c` 的 `CE_MAX_POWER_VAL()` 同址）。*/
#define SIGNAL_MAX_POWER_VAL() (*(int32_t *)CE_ADDR_MAX_POWER)

/* ---- 绿点的**三档来源**（§2.2 / §7.3 的表；实体层共用 `faith.c`，这里只给值与自己那一槽）----
 * ★ 2026-09-12 审计补：原来本卡**只有「小绿点（每 100 个消弹 +10）」这一条**通道 ——
 *   全卡没有任何 `faith_spawn()` 调用 ⇒ 撒出来的点永远不带 `1 << FAITH_SLOT_111`
 *   ⇒ `signal_on_pickup()` 是个**永不触发的死回调**。下面两条钩子就是补齐缺口：
 *     · 大绿点（+100 = `SIGNAL_GREEN_BIG`）= **击破掉落**（判据与 110 的大信仰点共用
 *       `mof_core_wants_big_*` —— §7.1 定案「绿点就是同一个东西，只是数值表不同」；**boss 不掉**，§7.3）；
 *     · 中绿点（+10 = `SIGNAL_GREEN_MID`）= **满 P 后吃 P 点**（`on_item_collect`，判据 `mof_core_pick_p_item`）；
 *     · 小绿点（每 100 个 +10）= 消弹通道（`signal_on_cancel`，**不建实体**）。
 * ⚠ `value` 传的是**本卡的值**，且 `slot_mask` 必须是 `1 << FAITH_SLOT_111`；**不要**用
 *   `faith_spawn_big()` —— 那是 110 的便捷包装（写死 110 的槽与值）。*/

/* 灵击（§2.4 / §3.2.1）：X 键上升沿 + 炸弹库存 == 0 ⇒ 本卡的灵击。
 * 门槛 / 消耗 / 下限**全部来自共用件** `faith_core.h` 的 `FAITH_POWER_*`（= `mof_core.h` 的别名），
 * 本文件一个数都不另立。❓ 自定（原版 TH11 的灵击伤害 / 震屏参数我们没抄到，照 110 的占位量级）。*/
#define SIGNAL_INPUT_BOMB      0x002u     /* `CE_ADDR_INPUT_PRESSED` bit 1 = X（炸弹）*/
#define SIGNAL_BOMB_CANCEL_R   4000.0f    /* 全屏量级（与 110 同款上界）*/
#define SIGNAL_BOMB_CANCEL_MAX 99999      /* 数量上限（不限）*/
#define SIGNAL_BOMB_DMG        300        /* ❓ 自定占位：全场伤害源每帧伤害 */
#define SIGNAL_BOMB_DMG_LIFE   2          /* ❓ 自定占位：伤害源存活帧数 */
#define SIGNAL_BOMB_SHAKE_TIME 20         /* ❓ 自定占位：震屏帧数 */
#define SIGNAL_BOMB_SHAKE_START 12        /* ❓ 自定占位：震屏起始强度 */
#define SIGNAL_BOMB_SHAKE_END  0          /* ❓ 自定占位：震屏结束强度 */

/* ---- HUD（**原版 TH11 信号槽那一块**；几何逐条给一手出处）--------------------------------
 * ★ 2026-09-12 本轮（审计 A3 的 111 部分）：**原来的 5 个 `drawRect` 格子整段删掉**，
 *   改成 TH11 `front.anm` script32..35 那 **4 个 VM** 里的 3 个（第 4 个原版自己把它写成零宽）：
 *
 *   | 元素 | 脚本 | 贴图 | 几何（640×480，`anchor(1,2)`=（左，底））| 一手 |
 *   | --- | --- | --- | --- | --- |
 *   | 槽底 | 211 | `SIGNAL_GAUGE_BG`（sprite57）| `pos(42,456)`，48×16 | script32 |
 *   | 增长条 | 212 | `SIGNAL_GAUGE_BAR`（sprite58）| 同 pos；宽 = `9 + 4×档` px | 引擎 `th11:0x41b70b` |
 *   | 图标 | 213 | `SIGNAL_GAUGE_ICON`（sprite26）| `pos(30,468)`，16×16 | script35 |
 *
 *   · 槽底与图标的位置**相对块左下角**（= 原生图标的左下角 (30,468)）：
 *     图标 (0,0)、槽 (+12,+12)；块的**外沿** = 640 空间 `x 30..90`、`y 440..468` = **60×28**。
 *   · **增长条按档点亮**（原生：`[vm1+0x4c] = 9 + 4×档`，档 = `pct/10` ∈ 0..10）⇒ 信号每 +10
 *     点亮一根 4 px 宽的条；`pct = 信号/1000`（= 原生 `sig*100/1000` 的同量纲，`sig` 上限 12000+）。
 *   · **档位闪色**（原版 `th11:0x41b593`–`0x41b66e` 写 `[vm+0x37c]`）：`pct<100 → 中断 7`（常态白）、
 *     `<110 → 8`（5 帧闪）、`<120 → 9`（3 帧闪）、`≥120 → 10`（2 帧闪）。**只在档位变化时发**
 *     （原生那边也是靠 `gui+0x4420` 的两个状态位做边沿）。
 *   · **整块压暗**（原版 `th11:0x41b4a1`–`0x41b55a`）：显示态遇到「自机 `y > 416` 且 `x < −64`」⇒
 *     发中断 3（`alphaTime(10,0,64)`）；压暗态遇到「`y < 400` 或 `x > −64`」⇒ 发中断 2（淡回）。
 *     ⚠ 这套门限是 **TH11 自己的**，与 SDK 的 `ce_hud_near_corner()`（TH16 栅格 400/384）**不是同一组**
 *     ⇒ 本卡自算（共享件不动）。
 *   · ❓ **文字读数三行是我们自己加的**（原生 SA 的读数就是那排竖条，没有数字）—— 商品化成
 *     「信号 0.00–1.30 / 信仰 / 蓝点分预览」，摆在原生那一块的右边（块因此宽到 104，640 空间）。
 *     这一条 2026-09-10 起就在 `TUNING.md` §2 的 111 行登记为 ❓ 自定，本次只挪位置、不改口径。
 *
 * ⚠ 块尺寸（表面 px）= 原生外沿 + 读数列：`104 × 28`（640 空间）= **208 × 56**。
 *   块一变宽变高，货架会把它后面的块（116/117 …）往右推 —— 这是「按原生尺寸报块」的必然结果，
 *   报告里登记了影响面。*/

/* 640×480 字面量空间 → 表面 px（VM 脚本里是 `resolutionMode(2)`）。*/
#define SIGNAL_U640_TO_SURF    2.0f
#define SIGNAL_HUD_BLOCK_W     208.0f     /* 表面 px：104（640 空间）×2 */
#define SIGNAL_HUD_BLOCK_H      56.0f     /* 表面 px：28（640 空间）×2 */

/* 元素相对**块左下角**的偏移（640 空间；块左下角 = 原生图标左下角 (30,468)）
 * ⚠ 屏幕坐标 **y 向下增长** ⇒ 「在块底边之上 N 像素」在算式里是 **减** N（`by - N`）。
 *   原生几何：图标 (30,468) 16×16（y 452..468）、槽 (42,456) 48×16（y 440..456）
 *   ⇒ 槽的底边比图标的底边**高 12**（468−456），块的外沿 = y 440..468 = **28 高**。*/
#define SIGNAL_ICON_DX          0.0f      /* 原生 sprite26 `pos(30,468)` − (30,468) */
#define SIGNAL_ICON_UP          0.0f      /* 图标的底边 = 块的底边 */
#define SIGNAL_GAUGE_DX        12.0f      /* 原生 `pos(42,456)`：x +12、底边**向上** 12 */
#define SIGNAL_GAUGE_UP        12.0f

/* 增长条的宽度（原版 `th11:0x41b6c3/0x41b70b`）：`9 + 4×档`，档 = `min(pct,100)/10` ∈ 0..10。
 * 分母 = sprite58 的**内容宽 48 px**（画布 64 px，内容靠左 48 —— 见 make_signal_hud_art.py）。*/
#define SIGNAL_BAR_BASE_PX      9.0f      /* 0x41b70b 的 `fadd 9.0` */
#define SIGNAL_BAR_STEP_PX      4.0f      /* `lea edx,[eax*4]` */
#define SIGNAL_BAR_CONTENT_PX  48.0f      /* sprite58 的宽（front.anm entry0 一手）*/
#define SIGNAL_BAR_MAX_STEP      10       /* pct 封顶 100 ⇒ 档 0..10 */

/* 档位中断号（= 脚本 212 的 `interruptLabel`；一手 `th11:0x41b5a0/0x41b5d1/0x41b608/0x41b644`）*/
#define SIGNAL_INT_IDLE          7        /* pct < 100：常态白 */
#define SIGNAL_WARMUP_FRAMES    20        /* 一手 `th11:0x41b593` `cmp [gui+0x43cc],0x14; jl`：
                                           * 槽亮出来后的前 **20 帧不发档位中断**（那个 zTimer 的
                                           * `current` ≥ 20 才让档位状态机跑）—— 否则 interrupt 7 的
                                           * `stop()` 会把脚本的 `+20: alphaTime(20,0,255)` 淡入跳过去，
                                           * VM 永远停在 `alpha(0)` 上（= 看不见）。我们的等价实现 =
                                           * 「VM 起用后的帧数」（🟡 字段语义未坐实成「关卡计时」，
                                           * 但两者在开场那一刻同时归零 ⇒ 效果一致）。*/
#define SIGNAL_INT_L1            8        /* 100 ≤ pct < 110：5 帧闪 */
#define SIGNAL_INT_L2            9        /* 110 ≤ pct < 120：3 帧闪 */
#define SIGNAL_INT_L3           10        /* pct ≥ 120：2 帧闪 */
#define SIGNAL_INT_DIM           3        /* 压暗（原生 4 个 VM 都发）*/
#define SIGNAL_INT_SHOW          2        /* 淡回 */

/* 压暗/淡回的门限（一手：`th11:0x41b4a1`–`0x41b55a`；[0x498010] = 416.0、
 * [0x4982d8] = 400.0、[0x4982e0] = −64.0）*/
#define SIGNAL_DIM_Y_HI        416.0f     /* 压暗：y > 416 */
#define SIGNAL_DIM_X_LO       (-64.0f)    /* 压暗：x < −64 */
#define SIGNAL_SHOW_Y_LO       400.0f     /* 淡回：y < 400 */
#define SIGNAL_SHOW_X_HI      (-64.0f)    /* 淡回：x > −64 */

/* 读数列（❓ 自定，见上）：x = 块左下角 +62（= 原生块右缘 60 + 2），
 * 三行的**中心**在块底边之上 5 / 14 / 23（行距 9，字形在 640 空间里约 10 高 ⇒ 正好填满 28 高的块）。*/
#define SIGNAL_TEXT_DX         62.0f
#define SIGNAL_TEXT1_UP        23.0f      /* 信号强度 0.00–1.30 */
#define SIGNAL_TEXT2_UP        14.0f      /* 信仰（本卡这份）*/
#define SIGNAL_TEXT3_UP         5.0f      /* 蓝点分预览 */

/* 颜色（D3DCOLOR = 0xAARRGGBB）。**槽底/增长条的色不再由这里给**：它们写在脚本 211/212 的
 * `color()` 里（原生原值：槽底 `color(32,32,64)`、增长条 `color(255,255,255)` + 闪色由中断切），
 * 与原生一致 ⇒ 卡侧一个颜色都不写（只有文字读数是我们自己的配色，❓ 自定）。*/
#define SIGNAL_C_TEXT          0xffffffffu
#define SIGNAL_C_TEXT_MULT     0xff80a0ffu

#define SIGNAL_LOG_EVERY       600        /* 每 600 帧一条汇总（实跑对账）*/

/* ---- 卡私有状态（`ce_state`，含 `CE_CARD` 的 16 字节保留头）---- */
typedef struct {
    faith_slot_t slot;            /* Δ_111（定点 ×100）/ 连击槽 / **signal（千分点 0..130000）** / 小绿点余数
                                   * / base（按难度）/ delta_cap（= 原版基础）—— 全在共用件里 */
    int32_t      last_graze;      /* 上一帧原生擦弹计数；-1 = 未初始化 */
    int32_t      graze_mult;      /* 擦擦倍率（千分点，`min(graze, 8990)`）*/
    int32_t      last_lives;      /* 上一帧残机（被弹检测的兜底：命数减少 ⇒ 归零）。-1 = 未初始化 */
    int32_t      frame;
    int32_t      stat_big;        /* 诊断：**收到**的大绿点颗数 */
    int32_t      stat_small;      /* 诊断：**收到**的中绿点颗数 */
    int32_t      stat_big_spawn;  /* 诊断：**撒出**的大绿点颗数（§2.2 击破通道）*/
    int32_t      stat_mid_spawn;  /* 诊断：**撒出**的中绿点颗数（满 P 吃 P 点）*/
    int32_t      stat_pool_full;  /* 诊断：共用池满导致撒点失败的次数（`faith_spawn()` 返回 0）*/
    int32_t      stat_cancel;     /* 诊断：消弹总数（小绿点来源）*/
    int32_t      stat_sweep;      /* 诊断：满槽全收触发次数 */
    int32_t      stat_bomb;       /* 诊断：灵击次数 */
    int32_t      stat_reset;      /* 诊断：被弹归零次数 */
    int32_t      log_frame;
} signal_card_t;

_Static_assert(sizeof(signal_card_t) <= 496, "ce_state 上限 496 字节（超了会静默返回 NULL）");

static signal_card_t *st(ce_card_t *c) { return ce_state(c, signal_card_t); }

static int     signal_difficulty(void) { return *(volatile int32_t *)SIGNAL_ADDR_DIFFICULTY; }
static int32_t signal_graze(void)      { return *(volatile int32_t *)SIGNAL_ADDR_GRAZE; }

/* ---- 计分层：声明「本卡这份信仰」（§7.2 / §7.8 / EVENTS-CONTRACT §3）----
 * 声明值 = `基础_111(难度) + Δ_111`（**不是增量**）：这是聚合公式要的那一项
 * （两卡同装时聚合 = 两份声明之和 = `（基础均值 + ΣΔ）× 2` 的前一半，见 `mof_core_pv()`）。*/
static void signal_declare_score(signal_card_t *s)
{
    if (!s) return;
    ce_score_declare(SIGNAL_ID_NUM, faith_score_unit(&s->slot));
    /* ★ 乘数附加通道（EVENTS-CONTRACT §3.1）：两卡同装时**落笔者恒是 110**（id 更小），
     * 它拿不到我们的信号强度，而契约禁止读别的卡的状态 ⇒ 我们只**声明**自己这一项。
     * ⚠ **量纲 = 千分点**（`ce_score_mul_aggregate()` 直接进 `mof_core_pv()` 的 `mult`，
     *   那里是 `被乘数 × mult / 1000`）；**不能**声明裸的 `s->slot.signal`（那是 0..130000 的定点，
     *   差 100 倍 ⇒ 乘数会被放大到 ×130）。`signal_core_strength()` 正好把它换成 0..1000。
     * 粘性语义：信号掉到 0 时要**照样声明 0**（否则旧值留在层里）；删卡由 `ce_layer_drop_card()` 清。*/
    ce_score_mul_declare(SIGNAL_ID_NUM, signal_core_strength(s->slot.signal));
}

/* ---- 绿点收集回调（`faith.c` 的 `dot_collect()` 发出来；只回调认领了本掩码的卡）----
 * 111 的槽 = `1 << FAITH_SLOT_111`。三档里：
 *   · BIG（大绿点，击破通道）= +100（`FAITH_SA_BIG` → 由 `signal_on_enemy_drop_pre` 撒）；
 *   · SMALL 档实体（本卡用作「中绿点」= 满 P 后吃 P 点）= +10（`FAITH_SA_MID` → `signal_on_item_collect` 撒）；
 *   · 小绿点（每 100 个 +10）**不建实体** ⇒ 走 `on_cancel`（消弹计数），不在这里。
 * 记账（加值 / Δ_111 上限 / 连击槽）**全在 `signal_core_pickup()`**（纯逻辑，单测覆盖），
 * 本回调只做「这颗粒子是不是我的槽 + 诊断 + 重新声明」这三件卡侧的事。*/
static void signal_on_pickup(void *user, int slot_mask, int tier, int32_t value)
{
    ce_card_t *c = (ce_card_t *)user;
    signal_card_t *s = c ? st(c) : 0;
    faith_ev_t ev;

    if (!s || !(slot_mask & (1 << FAITH_SLOT_111))) return;   /* 不是本卡的槽（110 的点不碰）*/
    if (value <= 0) return;

    /* 档位决定连击槽的 +f：大绿点 = 大档（+130f）、其余走绿点通道（+100f）。§7.5 的表。*/
    ev = (tier == (int)FAITH_TIER_BIG) ? FAITH_EV_BIG : FAITH_EV_GREEN;
    (void)signal_core_pickup(&s->slot, ev, value);
    if (tier == (int)FAITH_TIER_BIG) ++s->stat_big; else ++s->stat_small;
    signal_declare_score(s);
}

/* ---- 消弹回调（`faith.c` 的 `faith_cancel_account()` 发；颗数 = `ce_cancel_radius()` 的返回值）----
 * 111 侧 = **小绿点「每 100 个 +10」**，余数走 `faith_slot_t.small_rem`（**跨关保留**，§3.2.1）。
 * 110 侧同一入口按自己的表记微信仰点 —— **各槽自己算自己的**，不做跨卡耦合（共用件负责）。*/
static void signal_on_cancel(void *user, int32_t bullets)
{
    ce_card_t *c = (ce_card_t *)user;
    signal_card_t *s = c ? st(c) : 0;
    int32_t add111 = 0;

    if (!s || bullets <= 0) return;
    s->stat_cancel += bullets;
    faith_slot_add_cancel(0, &s->slot, bullets, 0, &add111);   /* s110 传 0：110 的槽不归本卡管 */
    if (add111 > 0) {
        /* 每满 100 颗进一笔 +10 ⇒ 连击槽也照绿点通道走（§7.5：小绿点走蓝点通道）。*/
        (void)faith_combo_add(&s->slot, FAITH_EV_GREEN);
        signal_declare_score(s);
    }
}

/* ---- 这只敌人是不是 boss（§7.3「大绿点 boss 不掉」的判据）----
 * 与 `mof.c` 的 `mof_is_boss()` **同口径**：读 `EnemyManager` 的 boss id 表（`em + CE_EM_BOSS_IDS`，
 * stride 4；一手 = `0x4237ff` 按它走链表、写点 `0x42d4aa`，先例 `season.c` 的 `enemy_is_boss`）。
 * ⚠ 槽数 4 来自 `engine.h:82`（🟡 asm 里证不了表大小：没有边界检查，只有一个调用者探过 0/1/2）
 *   —— 读的是管理器结构体内的偏移，多读一格不会越界。⚠ 两个文件的这份判据是**有意重复**的：
 *   它是 12 行只读代码，而共用的落点是 `faith.*`（那是实体池，不该塞引擎判据）。*/
static int signal_is_boss(const uint8_t *e)
{
    uint8_t *em = CE_ENEMY_MGR();
    int32_t id;
    int i;

    if (!e || !em) return 0;
    id = *(const int32_t *)(e + CE_ENEMY_ID);
    for (i = 0; i < CE_EM_BOSS_SLOTS; ++i) {
        if (*(const int32_t *)(em + CE_EM_BOSS_IDS + i * 4) == id) return 1;
    }
    return 0;
}

/* ---- 大绿点（§2.2 / §7.3「击破掉落」= +100）：撒点钩子 ----
 * 判据与 110 的大信仰点**完全一样**（§7.1 定案「绿点就是同一个东西，只是数值表不同」⇒ 触发点共享）：
 *   · `drop_main == 2`（TH18 的金钱 = 本作的点道具）⇒ `mof_core_wants_big_main()`（判据①）；
 *   · 或 ex_drop 里 type 1 与 type 2 **都 > 0**（「同时掉钱与 P」）⇒ `mof_core_wants_big_dual()`（判据②）；
 *   · **boss 不掉**（§7.3 / §3.2.1）。
 * ⚠ 为什么挂 `on_enemy_drop_pre` 而不是 `on_enemy_death`：本卡既不改掉落表、也不抢先机，
 *   而这个钩子的入参 `counts` **同时**给出 `main_drop` 与 ex_drop 计数 ⇒ 两条判据一次拿全
 *   （110 分两个钩子是因为它还要在死亡回调里做别的账）。
 * ⚠ 回调里只 `faith_spawn()`：O(1)、只写 `faith.c` 的静态池；**ANM VM 由 `faith_pool_tick()`
 *   隔 1–4 帧再建**（`faith.c` 的 `delay`）⇒ 不在敌人的死亡栈里建任何引擎对象、不碰敌人。*/
static void signal_on_enemy_drop_pre(ce_card_t *c, int32_t *counts)
{
    signal_card_t *s = st(c);
    uint8_t *e;
    const float *ep;
    int32_t main_drop, drops = 0, p_count = 0;
    int is_boss, want;
    int i;

    if (!s || !counts) return;
    /* `counts` = `zEnemyDrop + 0x04`（`CE_ENEMY_DROP_COUNTS`）；`zEnemyDrop` = `enemy + 0x623c`。
     * 一手（asm `0x430499`：`lea ecx,[edi+0x623c]` 之后 `call 0x430510`）⇒ 反推敌人指针成立。*/
    e = (uint8_t *)counts - CE_ENEMY_DROP_COUNTS - CE_ENEMY_DROP;
    is_boss = signal_is_boss(e);
    main_drop = *ce_drop_main(counts);
    for (i = 0; i < CE_ENEMY_DROP_TYPES; ++i) {          /* ex_drop[(type-1)]（engine.h:44）*/
        if (i + 1 == MOF_DROP_TYPE_MONEY) drops = counts[i];
        if (i + 1 == MOF_DROP_TYPE_P)     p_count = counts[i];
    }
    want = mof_core_wants_big_main(main_drop, is_boss)
        || mof_core_wants_big_dual(main_drop, drops, p_count, is_boss);
    if (!want) return;
    ep = ce_enemy_pos(e);
    if (!ep) return;

    /* 值用**本卡的**「大」档（`SIGNAL_GREEN_BIG` = 100），槽写本卡自己的掩码。
     * 与 110 同一次死亡各撒各的（池按 `slot_mask` 分账、回调只发认领该掩码的卡）⇒ 互不干扰。*/
    if (faith_spawn(ep[0], ep[1], FAITH_TIER_BIG, 1 << FAITH_SLOT_111, SIGNAL_GREEN_BIG)) {
        ++s->stat_big_spawn;
    } else {
        ++s->stat_pool_full;
    }
}

/* ---- 中绿点（§2.2「满 power 后红点转换」= +10）：`on_item_collect` 收 type 1（P 点）----
 * 判据与 110 的小信仰点同源（`mof_core_pick_p_item()`：满 P、且这颗确实是 P 点、非 boss）。
 * ⚠ 本卡**只在「吃到」这一刻**撒中绿点（`mof.c` 在 `on_enemy_drop_pre` 里也撒一次小信仰点
 *   ⇒ 那颗 P 点会被 110 记两次，见审计报告；本卡不复制那个行为）。
 * ⚠ 不动原生道具：`type` 不改、分派表不碰，撒出来的**一律是自建 VM**（§11 #48 的口径）。*/
static void signal_on_item_collect(ce_card_t *c, uint32_t type, const void *item)
{
    signal_card_t *s = st(c);
    const float *ip;
    mof_pick_t pick;

    if (!s || !item) return;
    if (CE_CURRENT_POWER() < SIGNAL_MAX_POWER_VAL()) return;        /* 不满 P 不转化 */
    if (type != (uint32_t)MOF_DROP_TYPE_P) return;                  /* 只认 P 点（type 1）*/
    pick = mof_core_pick_p_item((int32_t)MOF_DROP_TYPE_P, 1, CE_CURRENT_POWER(),
                                SIGNAL_MAX_POWER_VAL(), /*is_boss=*/0);
    if (pick != MOF_PICK_ADD_SMALL) return;
    ip = (const float *)((const uint8_t *)item + CE_ITEM_POS);
    if (faith_spawn(ip[0], ip[1], FAITH_TIER_SMALL, 1 << FAITH_SLOT_111, SIGNAL_GREEN_MID)) {
        ++s->stat_mid_spawn;
    } else {
        ++s->stat_pool_full;
    }
}

/* ---- 蓝点分落笔（§2.3 / §7.4 / §7.8 / EVENTS-CONTRACT §3）----
 * `PV = 信仰 × (擦弹倍率 + 信号强度)`，作用在**金钱道具的分数**上（**不改 MONEY**）。
 * 引擎随后 `SCORE += *value / 10` ⇒ 写 `*value = PV × 10` 就是「精确拿到 PV 分、天然不双计分」。
 *
 * ⚠ **不调共用件的 `mof_core_pv()`**：它的「只装 111」与「两卡同装」两支写反了（实装期一手证伪，
 *   见 `docs/superpowers/specs/2026-09-12-signal-111-impl.md`「偏差」一节）。共用件归 110、
 *   本卡**不改它** ⇒ 这里按契约原文算（`signal_core_pv_111()`，纯逻辑有主机单测）。
 *   110 落笔时仍走 `mof_core_pv()` 的「只装 110」那一支（那支是正确的），两条路同式同值。*/
static void on_item_score(ce_card_t *c, int32_t *value)
{
    signal_card_t *s = st(c);
    int32_t pv;

    if (!s || !value) return;
    if (ce_score_applier() != (int)SIGNAL_ID_NUM) return;   /* 不是落笔者 ⇒ 什么都不做 */

    pv = signal_core_pv_111_scaled(&s->slot, ce_score_aggregate(), ce_owned(110) ? 1 : 0,
                                   s->graze_mult, s->slot.signal);
    /* 写 `*value = pv × 10`（引擎随后 `SCORE += value/10`，§7.8）⇒ 先钳到 int32/10 以内，
     * 不然极端数值会在这里回绕成负分。 */
    if (pv > 200000000) pv = 200000000;
    if (pv <= 0) return;
    *value = pv * 10;                                       /* 引擎会 /10 ⇒ 精确 PV 分 */
}

/* ---- 满槽全收（§2.1 / §5.6 订正）----
 * ★ 「像上线回收一样自动收道具」= 把道具池里所有活道具的 state 置 3（真空飞行），
 *   引擎自己把它们飞向自机、走**原生**收集（计分 / HUD / 计数 / 磁吸全原生）——
 *   用 SDK 的共享件 `ce_item_collect_all()`（107 森罗结界同一件）。
 * ⚠ 不是 `0x445ffc`（那是 `comiss` 指令，spec §5.6 已订正）。*/
static int signal_sweep_items(signal_card_t *s)
{
    int n = ce_item_collect_all();
    if (n > 0) {
        ++s->stat_sweep;
        ce_log("signal: gauge full (signal %d >= %d) -> swept %d items",
               (int)s->slot.signal, (int)SIGNAL_FULL_AT, n);
    }
    return n;
}

/* ---- 灵击（§2.4 「B 双选」/ §3.2.1；与 110 **同一套**门槛与消耗）----
 * 触发：`on_tick_2` 读炸弹键**上升沿**（不是 `on_bomb_spent` —— 它在炸弹**成功花掉之后**才触发，
 *      承载不了「库存 == 0」这条分支，review §5.5 订正 2 / 110 同款）。
 * 库存 == 0 → 本卡自干：自写扣火力（≥2 档、扣 1 档、**下限 1.05**）+ 全屏消弹 + 伤害源 + 震屏；
 * 库存 > 0  → 交给原生 `do_bomb`（本卡不动）。
 * ⚠ 扣完**必须** `ce_repopulate_options()`（Tsukasa / 商店先例）。*/
static void signal_bomb(signal_card_t *s)
{
    const float *pp = ce_player_pos();
    float pos[3];
    mof_power_t pw = mof_core_power_spend(CE_CURRENT_POWER());
    int n = 0;

    if (!pp || !pw.ok) return;                           /* 火力不够 ⇒ 什么都不做（照原作）*/

    *(int32_t *)CE_ADDR_CURRENT_POWER = pw.new_power;    /* ★ 自写扣法（原生会拒绝、且只能整档扣）*/
    ce_repopulate_options();                             /* 子机按新档重建（必须） */

    pos[0] = pp[0]; pos[1] = pp[1]; pos[2] = pp[2];
    /* 全屏消弹：**用 `ce_cancel_radius` 的返回值**（`ce_cancel_all_bullets()` 是 void、数不出来），
     * 并且先认领「本帧已消过弹」的闩锁（同帧 107 / 117 也要消弹时只有第一家动手，
     * EVENTS-CONTRACT §5.2 细则 2）。*/
    if (ce_cancel_latch_claim((uint32_t)SIGNAL_ID_NUM)) {
        n = ce_cancel_radius(pos, SIGNAL_BOMB_CANCEL_R, SIGNAL_BOMB_CANCEL_MAX, 0);
        if (n > 0) faith_cancel_account(n);              /* → 小绿点每 100 个 +10（进 Δ_111）*/
    }
    /* 全场伤害 + 震屏（❓ 参数是占位常量，见文件头）。*/
    ce_raise_damage_cap(SIGNAL_BOMB_DMG);
    ce_damage_rect(pos, 0.0f, SIGNAL_BOMB_DMG_LIFE, SIGNAL_BOMB_DMG,
                   SIGNAL_BOMB_CANCEL_R, SIGNAL_BOMB_CANCEL_R);
    ce_screen_shake(SIGNAL_BOMB_SHAKE_TIME, SIGNAL_BOMB_SHAKE_START, SIGNAL_BOMB_SHAKE_END);

    ++s->stat_bomb;
    signal_declare_score(s);
    ce_log("signal: 灵击 -1 档 (power %d -> %d), cancelled %d, signal %d, faith %d",
           (int)CE_CURRENT_POWER(), (int)pw.new_power, n, (int)s->slot.signal,
           (int)faith_slot_total(&s->slot));
}

/* ---- 被弹归零（§2.1 / §4）----
 * 挂虚表 `+0x0c`（`on_death_after_deathbomb`）：死亡序列里**决死窗口耗尽之后、引擎扣命之前**
 * 的广播（一手：`double.c` 的文件头 + `engine/card/th18/03-hooks.md` §4）。
 * 为什么不用 `+0x14`：那是 respawn 之后的钩子，那时归零已经太晚（SA 原版是**被弹那一刻**归零）。
 * ⚠ 它覆盖的是「真被弹 / 真死」这条路径；**河童 bomb（不扣命的中弹）** 这一档要读敌人类型才能区分，
 *   **本卡未做**（登记在 `docs/superpowers/specs/2026-09-12-signal-111-impl.md` §4 第 2 条）。*/
static int on_death_after_deathbomb(ce_card_t *c, uint32_t acc)
{
    signal_card_t *s = st(c);

    (void)acc;
    if (!s) return 0;
    if (signal_core_reset(&s->slot)) {
        ++s->stat_reset;
        ce_log("signal: hit -> gauge reset to 0 (lives %d)", (int)CE_CURRENT_LIVES());
    }
    return 0;                                            /* 不救命，只归零 */
}

/* ---- 每帧 ---- */
static int on_tick_2(ce_card_t *c)
{
    signal_card_t *s = st(c);
    int32_t g, dg;
    int lives, pstate, above, offline;

    if (!s || !CE_PLAYER()) return 0;
    ++s->frame;

    /* ① 擦弹帧差 → 擦弹倍率 + 信号 +8/graze（§2.1 / §7.5）*/
    g = signal_graze();
    if (s->last_graze < 0 || g < s->last_graze) s->last_graze = g;
    dg = g - s->last_graze;
    s->last_graze = g;
    s->graze_mult = mof_core_graze_mult(g);

    /* ② 回收线读数（共用件的判据：`player+0x47998` + 死亡 / 复活态不看线）*/
    pstate = *(int32_t *)(CE_PLAYER() + CE_PLAYER_STATE);
    above = faith_player_above_line();
    /* ⚠ 死亡 / 复活（state 2 / 4）时**既不加也不减**（原版那两帧同样不看回收线）⇒ 两参都 0。*/
    offline = (pstate == 2 || pstate == 4) ? 0 : !above;

    /* ③ 信号槽推进（四段全在共用件里：线上 +50/f、≥100 后 +0.09/f、擦弹 +8、线下 −0.3/f）*/
    (void)signal_core_apply_step(&s->slot, above, offline, dg > 0 ? dg : 0);

    /* ④ 被弹归零的**兜底**：命数减少 ⇒ 重置（`+0x0c` 钩子没抓到的路径，例如某个非死亡流程扣命）*/
    lives = CE_CURRENT_LIVES();
    if (s->last_lives >= 0 && lives < s->last_lives) {
        if (signal_core_reset(&s->slot)) ++s->stat_reset;
    }
    s->last_lives = lives;

    /* ⑤ 满槽（≥100）⇒ 像上线回收一样自动收道具（§2.1）*/
    if (signal_core_is_full(s->slot.signal)) signal_sweep_items(s);

    /* ⑥ 灵击（库存 == 0 且炸弹键上升沿）*/
    if (CE_CURRENT_BOMBS() <= 0 && (*(uint32_t *)CE_ADDR_INPUT_PRESSED & SIGNAL_INPUT_BOMB)) {
        signal_bomb(s);
    }

    /* ⑦ 绿点池推进（共用件：撒点 / 吸附 / 收集 → 回调 `signal_on_pickup`）*/
    (void)faith_pool_tick();

    /* ⑧ 计分层：每帧重新声明（信仰会因绿点 / 小绿点 / miss 变化）*/
    signal_declare_score(s);

    /* ⑨ 诊断汇总 */
    if (s->frame - s->log_frame >= SIGNAL_LOG_EVERY) {
        s->log_frame = s->frame;
        ce_log("signal: sig %d (str %d), faith %d (Δ %d), graze %d (x%d), big %d/%d small %d/%d full %d cancel %d sweep %d bomb %d reset %d",
               (int)s->slot.signal, (int)signal_core_strength(s->slot.signal),
               (int)faith_slot_total(&s->slot), (int)(s->slot.delta / FAITH_FIXED),
               (int)s->last_graze, (int)s->graze_mult,
               (int)s->stat_big, (int)s->stat_big_spawn,      /* 收到 / 撒出（大绿点）*/
               (int)s->stat_small, (int)s->stat_mid_spawn,    /* 收到 / 撒出（中绿点）*/
               (int)s->stat_pool_full, (int)s->stat_cancel,
               (int)s->stat_sweep, (int)s->stat_bomb, (int)s->stat_reset);
    }
    return 0;
}

/* ============================ HUD（左下角：**原版 TH11 信号槽**）============================
 * 走 **`ce_hud_slot_claim(111, …)`**（`HUD-LAYOUT.md` §3）：底部货架按 card_id 升序排布 ⇒
 * 与季节 116 / 动物灵 117 / 信仰 110 同装也不会重叠。
 * ★ `ce_hud_slot_claim()` 给的是「认领那一刻」的坐标，同一开场批次里后来的认领会重排
 *   ⇒ 每帧用 `ce_hud_slot_get()` 取最新值，VM 每帧按它摆（`sdk_core.h` 的注释这么要求的）。
 *
 * 三个**原版 VM**（脚本 211/212/213，`layer(25)`；贴图是 TH11 `front.anm` entry0 的切图）：
 *   槽底（sprite57）+ 增长条（sprite58）+ 图标（sprite26）—— 位置/宽度/闪色/压暗的出处见
 *   文件上半部分常量块的注释块。**我们只加了三行文字读数**（❓ 自定）。
 * ❓ 原生那第 4 个 VM（script34 / sprite59）**不出**：与 sprite58 同矩形，且原生每帧把它的
 *   宽度写 0（`th11:0x41b71f` `fldz`）⇒ 正常游玩不可见（报告里登记）。*/

static uint32_t s_gauge_bg;             /* 原生 VM[0]（脚本 211）*/
static uint32_t s_gauge_bar;            /* 原生 VM[1]（脚本 212）*/
static uint32_t s_gauge_icon;           /* 原生 VM[3]（脚本 213）*/
static int32_t  s_gauge_state;          /* 原生 `gui+0x4420` 的 bit6..8：0=空闲 / 1=L1 / 2=L2 / 3=L3。
                                         * ⚠ **初值 0 = 「空闲但没发过 7」**（原生只有当这三位由非 0
                                         * 变 0 时才写中断 7）⇒ 起 VM 的那一刻**不发**中断 7。*/
static int32_t  s_gauge_age;            /* VM 起用后的帧数（喂上面那个 20 帧门；`_kill` 里归零）*/
static int32_t  s_gauge_dim;            /* 原生 `gui+0x4420` bit0：1 = 已压暗 */
static ce_hud_vm_text_t s_sig_txt;      /* 信号强度 0.00..1.30（❓ 自定）*/
static ce_hud_vm_text_t s_faith_txt;    /* 信仰（基础 + Δ_111）（❓ 自定）*/
static ce_hud_vm_text_t s_bonus_txt;    /* 本帧蓝点分（PV）预览（❓ 自定）*/

/* 起一个 HUD VM：`pos` 用 640×480 字面量空间写，内部 ×2 换成表面 px（脚本 `resolutionMode(2)`）。*/
static uint32_t signal_hud_spawn(int script, float x640, float y640)
{
    float pos[3];

    pos[0] = x640 * SIGNAL_U640_TO_SURF;
    pos[1] = y640 * SIGNAL_U640_TO_SURF;
    pos[2] = 0.0f;
    return ce_anm_spawn_ui(CE_ABILITY_ANM(), script, pos);
}

static int signal_hud_alive(uint32_t id) { return id != 0 && ce_anm_get_vm(id) != 0; }

static void signal_hud_kill(void)
{
    if (s_gauge_bg)   { ce_anm_delete(s_gauge_bg);   s_gauge_bg = 0; }
    if (s_gauge_bar)  { ce_anm_delete(s_gauge_bar);  s_gauge_bar = 0; }
    if (s_gauge_icon) { ce_anm_delete(s_gauge_icon); s_gauge_icon = 0; }
    s_gauge_state = 0;
    s_gauge_age = 0;
    s_gauge_dim = 0;
    ce_hud_vm_text_clear(&s_sig_txt);
    ce_hud_vm_text_clear(&s_faith_txt);
    ce_hud_vm_text_clear(&s_bonus_txt);
}

/* 起 VM（幂等：已有的不动）。VM 会被引擎在别处摧毁 ⇒ 每帧自愈检查（`stock.c` 同款）。
 * ⚠ 起用顺序 = 前后关系（四件都在 25 层，原版那套 layer 18/19 是 TH11 的层号，不能照搬）：
 *   槽底 → 增长条 → 图标。*/
static void signal_hud_ensure(void)
{
    int spawned = 0;

    if (!signal_hud_alive(s_gauge_bg)) {
        s_gauge_bg = signal_hud_spawn(CE_ANM_ABILITY_SCRIPT_SIGNAL_GAUGE_BG, 0.0f, 0.0f);
        spawned = 1;
    }
    if (!signal_hud_alive(s_gauge_bar)) {
        s_gauge_bar = signal_hud_spawn(CE_ANM_ABILITY_SCRIPT_SIGNAL_GAUGE_RETICLE, 0.0f, 0.0f);
        spawned = 1;
    }
    if (!signal_hud_alive(s_gauge_icon)) {
        s_gauge_icon = signal_hud_spawn(CE_ANM_ABILITY_SCRIPT_SIGNAL_GAUGE_ICON, 0.0f, 0.0f);
        spawned = 1;
    }
    /* 新起（或重建）过 VM ⇒ 脚本的 +20 帧淡入从这一刻算起 ⇒ 20 帧门也要跟着重来。*/
    if (spawned) { s_gauge_age = 0; s_gauge_state = 0; }
    ++s_gauge_age;
}

/* 档位（一手 `th11:0x41b593`–`0x41b66e`）：`pct = 信号/1000`（原版 `sig*100/1000` 的同量纲），
 *   `pct < 100 → 空闲`、`< 110 → L1(中断 8)`、`< 120 → L2(9)`、`≥ 120 → L3(10)`。
 * 原生的实现是**两个状态位**（`gui+0x4420` 的 bit6/bit7）+ 三条「位型变了才写中断」的比较：
 *     · 回到空闲时**只有当位非 0** 才写中断 7（初次「空闲」不写 ⇒ 不打断淡入）；
 *     · 升到 Lk 时只有位型不等于该档才写中断 7+k。
 * 逐条照抄 ⇒ 只在**边沿**发（每帧重发会把 `jmp(…, 20/30/36/40)` 的闪色循环反复重头开始）。*/
static int signal_gauge_level_int(int32_t pct)
{
    if (pct < 100) return 0;
    if (pct < 110) return 1;
    if (pct < 120) return 2;
    return 3;
}

/* 整块压暗 / 淡回 —— 逐条照抄原版 `th11:0x41b4a1`–`0x41b55a`（门限见常量块的注释）。*/
static void signal_gauge_dim_tick(void)
{
    const float *pp = ce_player_pos();
    int intno = 0;

    if (!pp) return;
    if (!s_gauge_dim) {
        if (pp[1] > SIGNAL_DIM_Y_HI && pp[0] < SIGNAL_DIM_X_LO) { intno = SIGNAL_INT_DIM; s_gauge_dim = 1; }
    } else {
        if (pp[1] < SIGNAL_SHOW_Y_LO || pp[0] > SIGNAL_SHOW_X_HI) { intno = SIGNAL_INT_SHOW; s_gauge_dim = 0; }
    }
    if (!intno) return;
    if (s_gauge_bg)   ce_anm_interrupt(s_gauge_bg, intno);
    if (s_gauge_bar)  ce_anm_interrupt(s_gauge_bar, intno);
    if (s_gauge_icon) ce_anm_interrupt(s_gauge_icon, intno);
}

static void signal_hud_draw(signal_card_t *s)
{
    ce_hud_slot_t slot;
    char buf[40];
    int32_t pct, step, str, bonus, lvl;
    float bx, by, gx, gy, bar_px;

    if (!s) return;
    /* ⚠ 一手（`sdk_core.c` 的 `ce_hud_slot_get()`）：它**只填 x/y/w/h/side，不填 x640/y640**
     *   （认领时填、`get` 时不填）⇒ 直接读 `slot.x640/y640` 是**未初始化的栈值**。
     *   这里自己按 SDK 给的口径换算（`CE_HUD_SURFACE_TO_640` = 0.5）—— 反过来说：
     *   `bx/by` 就是块**左下角**在 640 空间里的坐标，原生那一块的全部几何都相对它写。*/
    if (!ce_hud_slot_get(SIGNAL_ID_NUM, &slot)) return;   /* 还没认领到（开场批次未跑完）*/
    bx = slot.x * CE_HUD_SURFACE_TO_640;
    by = slot.y * CE_HUD_SURFACE_TO_640;
    signal_hud_ensure();

    /* ① 三个原生 VM 的落位（640 空间 → VM 用表面 px；脚本里的 `anchor(1,2)` = （左，底））
     *    gx/gy = 槽的左下角（原生 (42,456) 相对块左下角 (30,468)：x +12、底边向上 12）。*/
    gx = bx + SIGNAL_GAUGE_DX;
    gy = by - SIGNAL_GAUGE_UP;
    if (s_gauge_bg)
        ce_anm_set_pos(s_gauge_bg, gx * SIGNAL_U640_TO_SURF, gy * SIGNAL_U640_TO_SURF, 0.0f);
    if (s_gauge_icon)
        ce_anm_set_pos(s_gauge_icon, (bx + SIGNAL_ICON_DX) * SIGNAL_U640_TO_SURF,
                       (by - SIGNAL_ICON_UP) * SIGNAL_U640_TO_SURF, 0.0f);

    /* ② 增长条的宽度 = `9 + 4×档` px（原版 `th11:0x41b70b`），档 = `min(pct,100)/10`；
     *    贴图内容 48 px 靠左 ⇒ 写成 scale.x = 目标宽/48（左端因 `anchor(1,2)` 钉住）。
     *    ⚠ 脚本 212 里**没有 scaleTime** ⇒ `ce_anm_set_scale` 不会被插值器覆盖。*/
    pct = s->slot.signal / 1000;                      /* 0..130（原生 `pct` 的同量纲）*/
    step = (pct > 100 ? 100 : pct) / 10;              /* 0..SIGNAL_BAR_MAX_STEP */
    bar_px = SIGNAL_BAR_BASE_PX + SIGNAL_BAR_STEP_PX * (float)step;
    if (s_gauge_bar) {
        ce_anm_set_pos(s_gauge_bar, gx * SIGNAL_U640_TO_SURF, gy * SIGNAL_U640_TO_SURF, 0.0f);
        ce_anm_set_scale(s_gauge_bar, bar_px / SIGNAL_BAR_CONTENT_PX, 1.0f);
    }

    /* ③ 档位闪色：**前 20 帧不发**（原版 `th11:0x41b593` 的门；理由见常量块的注释），
     *    之后只在档位变化时发（原生的状态位语义，逐条对应见 `signal_gauge_level_int`）。*/
    lvl = signal_gauge_level_int(pct);
    if (s_gauge_age >= SIGNAL_WARMUP_FRAMES && lvl != s_gauge_state) {
        if (lvl == 0) {
            if (s_gauge_state != 0 && s_gauge_bar) ce_anm_interrupt(s_gauge_bar, SIGNAL_INT_IDLE);
        } else if (s_gauge_bar) {
            ce_anm_interrupt(s_gauge_bar, SIGNAL_INT_IDLE + lvl);   /* 1→8 / 2→9 / 3→10 */
        }
        s_gauge_state = lvl;
    }

    /* ④ 整块压暗 / 淡回（自机贴到左下角时；门限是一手的 TH11 值）*/
    signal_gauge_dim_tick();

    /* ⑤ 三行文字读数（❓ 自定，见常量块的注释）。蓝点分预览用**本卡这份信仰**（本卡的贡献），
     *    不是落笔公式的完整聚合值 —— 两卡同装时完整值要读 `ce_score_aggregate()` 那份和，
     *    而那是**落笔者**的活（`on_item_score`）。这里只求「玩家看得懂的数」，标注为预览、不当账。*/
    str = signal_core_strength(s->slot.signal);
    snprintf(buf, sizeof buf, "%d.%02d", (int)(str / 1000), (int)((str % 1000) / 10));
    ce_hud_vm_text_set(&s_sig_txt, bx + SIGNAL_TEXT_DX, by - SIGNAL_TEXT1_UP, SIGNAL_C_TEXT,
                       CE_ASCII_ALIGN_LEFT, 0.0f, buf, "signal111");
    snprintf(buf, sizeof buf, "%d", (int)faith_slot_total(&s->slot));
    ce_hud_vm_text_set(&s_faith_txt, bx + SIGNAL_TEXT_DX, by - SIGNAL_TEXT2_UP, SIGNAL_C_TEXT,
                       CE_ASCII_ALIGN_LEFT, 0.0f, buf, "signal111");
    bonus = signal_core_bonus(faith_slot_total(&s->slot), s->graze_mult, s->slot.signal);
    snprintf(buf, sizeof buf, "%d", (int)bonus);
    ce_hud_vm_text_set(&s_bonus_txt, bx + SIGNAL_TEXT_DX, by - SIGNAL_TEXT3_UP, SIGNAL_C_TEXT_MULT,
                       CE_ASCII_ALIGN_LEFT, 0.0f, buf, "signal111");
}

static int on_draw(ce_card_t *c)
{
    signal_card_t *s = st(c);

    /* ★ 2026-09-12 审计：`on_draw`（虚表 `+0x48`）在**主菜单 / 标题**里也会被调，而 `on_tick_2`
     *   （`+0x2c`）不会 ⇒ 不设门会在菜单上留下 3 个原生 VM + 3 行读数，而且 `signal_hud_ensure()`
     *   会在菜单里**反复重建**那些 VM（117 的 bug3 就是这个坑；115 / 116 / 117 三张卡同款门）。
     *   门关着时把 VM 收掉；HUD **槽位**不动 —— 槽位的账归 `on_load` / `on_delete`。*/
    if (!s) return 0;
    if (!CE_PLAYER() || !CE_GAME_THREAD()) { signal_hud_kill(); return 0; }
    signal_hud_draw(s);
    return 0;
}

/* ---- 生命周期 ---- */
static void signal_clear(ce_card_t *c)
{
    signal_card_t *s = st(c);

    /* ★ 顺序要紧：先退出计分层（否则删掉的卡还在往聚合里声明），再收实体 / VM / 槽位。*/
    ce_score_declare(SIGNAL_ID_NUM, 0);
    faith_unregister(SIGNAL_ID_NUM);
    /* ★ 只收**自己槽**的绿点（`1 << FAITH_SLOT_111`）。原来是 `faith_clear_entities()`
     * （清整个共用池，文件头「自查」里登记过这个偏差）⇒ 111 被删时会把 110 同行绿点一起收掉。
     * 2026-09-12 审计改成按 `slot_mask` 筛的一支 —— 这就是那条偏差的补丁。*/
    faith_clear_entities_mask(1 << FAITH_SLOT_111);
    signal_hud_kill();
    ce_hud_slot_release(SIGNAL_ID_NUM);   /* 声明层由 CE_CARD 的 ce_layer_drop_card() 兜底，这里显式一次 */
    if (!s) return;
    memset(s, 0, sizeof *s);
    s->last_graze = -1;
    s->last_lives = -1;
}

static int on_load(ce_card_t *c)
{
    signal_card_t *s = st(c);
    faith_user_t u;
    ce_hud_block_t blk;

    if (!s) return 0;
    s->last_graze = -1;
    s->last_lives = -1;

    memset(&u, 0, sizeof u);
    u.card_id = SIGNAL_ID_NUM;
    u.slot_mask = 1 << FAITH_SLOT_111;    /* 只消费 111 槽（实体层零改动）*/
    u.on_pickup = signal_on_pickup;
    u.on_cancel = signal_on_cancel;
    u.user = c;
    if (!faith_register(&u)) ce_log("signal: faith_register failed (table full?)");

    /* ★ 2026-09-12 本轮：块尺寸从「❓ 自定 96×20」改成**按原生那一块的实际外沿报**
     *   （`HUD-LAYOUT.md` §6.1 的口径）：图标左下角 (30,468) → 槽右缘 (90,440) = 60×28（640 空间）
     *   = 120×56（表面 px），再加右边那 44×28 的**我们自己的读数列** ⇒ **208×56**。
     *   ⚠ 影响面：货架按 card_id 升序排，111 变宽会把后面的块（116/117 …）往右推
     *   （报告里登记；这是「按原生尺寸报块」的必然结果，不是回归）。*/
    memset(&blk, 0, sizeof blk);
    blk.w = SIGNAL_HUD_BLOCK_W;
    blk.h = SIGNAL_HUD_BLOCK_H;
    blk.side = CE_HUD_SIDE_LEFT;
    /* ★ 用 `_card` 入口：认领前先把关卡号推进纪元 ⇒ 上一关的占位在本关开场收敛（HUD-LAYOUT §4.4）。*/
    if (!ce_hud_slot_claim_card(SIGNAL_ID_NUM, &blk, 0)) {
        ce_log("signal: ce_hud_slot_claim failed");
    } else {
        ce_hud_slot_t probe;
        if (ce_hud_slot_get(SIGNAL_ID_NUM, &probe))
            ce_log("signal: HUD block %.0fx%.0f claimed at surface (%.0f, %.0f)",
                   (double)blk.w, (double)blk.h, (double)probe.x, (double)probe.y);
    }

    signal_declare_score(s);
    return 0;
}

static int ctor(ce_card_t *c)
{
    signal_card_t *s = st(c);

    if (!s) return 0;
    faith_slot_init_111(&s->slot, signal_difficulty());   /* 基础按难度、Δ 上限 = 原版基础（§7.2）*/
    s->last_graze = -1;
    s->last_lives = -1;
    s->log_frame = 0;
    signal_declare_score(s);
    return 0;
}

/* ---- 关卡开场归零（§2.1「被弹（含河童 bomb） / **关卡结束** → 归 0」的第二半）
 * ★ 2026-09-12 审计修（任务 3）：spec 明写「关卡结束归 0」，而本卡原来**只有** `on_run_reset`
 *   ⇒ 跨关不归零（`on_run_reset` 是**局末**，不是每关）。两个钩子的语义差别（`SDK.md` §2 的 21 槽虚表）：
 *     · `+0x34` **`on_stage_start`** = **每关开场**（一手：`GameThread__thread_start 0x442F98` 广播，
 *       `engine/card/th18/03-hooks.md` §「+0x34」；`CARD-CHECKLIST.md` 也把它列为「每关开始（含换关）」）；
 *     · `+0x4c` **`on_run_reset`** = **局末重置**（「局中被倒吊人摧毁只走 `+0x50 on_delete`，
 *       `on_run_reset` 不会来」）⇒ 它一个局只来一次。
 *   ⇒ spec 的「关卡结束」在引擎里没有独立钩子，落地点就是**下一关开场**这一钩子；
 *     `on_run_reset` 那边的归零**保留**（局末也归零没坏处，且两条路语义上不冲突）。
 *
 * ⚠ 归零的**只有信号槽**（`slot.signal` 这一个量）。**保留**（跨关）：
 *   · `Δ_111` 绿点信仰（`slot.delta`）与 `base` / `delta_cap` —— §3.2.1「信仰跨关保留」、
 *     §7.5「Δ_111 不会消失」（信仰只有局末/换局才回初值）；
 *   · `small_rem` 小绿点余数（§3.2.1 明写「跨关保留」）；
 *   · 连击槽（`slot.combo`）—— §2.1 的归零范围里没有它，本卡不碰。
 *   实现上这由 `signal_core_reset()` → 共用件 `faith_signal_reset()` 保证（它只写 `s->signal`），
 *   单测 `tests/test_signal_core.c` §5.1 逐项钉住「归零什么 / 保留什么」。
 *
 * ⚠ 这里**不动 HUD**（`on_run_reset` 会 `signal_hud_kill()`）：`ce_hud_vm_text_set()` 自带
 *   「VM 没了就重建」的自愈（`sdk.c`），三个原生 VM 有 `signal_hud_ensure()` 每帧自愈
 *   ⇒ 不需要像青眼 / 绀珠传那样在开场杀 VM（少一次无谓的 VM 重建）。
 *   ⚠ 但**压暗态**（`s_gauge_dim`）在开场要归零：原生的 flag 在 `zGui` 上、随局重置，
 *   我们放在静态区 ⇒ 留给 `signal_hud_kill()` / `signal_hud_ensure()` 那条路
 *   （开场不杀 VM 时它不会被复位，最多是「上一关压暗着、这一关第一帧立刻淡回」，无副作用）。
 * ⚠ 也**不碰**共用绿点池（残留的点由 `faith_pool_tick()` 自然出界）。*/
static void on_stage_start(ce_card_t *c)
{
    signal_card_t *s = st(c);

    if (!s) return;
    if (signal_core_reset(&s->slot)) {
        ++s->stat_reset;
        ce_log("signal: stage start -> gauge 0 (kept: faith %d, Δ %d, small_rem %d)",
               (int)faith_slot_total(&s->slot), (int)(s->slot.delta / FAITH_FIXED),
               (int)s->slot.small_rem);
    }
    /* 信号强度是**声明层**的一项（乘数附加通道）⇒ 归零后必须重新声明（粘性语义：
     * 信号掉到 0 时要照样声明 0，否则旧值留在层里；见 `signal_declare_score()`）。*/
    signal_declare_score(s);
}

static void on_run_reset(ce_card_t *c)
{
    /* 信号归 0（§2.1「关卡结束」的**局终**那一半；**每关**那一半现在是 `on_stage_start`，
     * 见上面那个函数的注释）+ 收 HUD VM。
     * ⚠ `on_run_reset`（虚表 `+0x4c`）的语义是**局末重置**（`SDK.md` §2 / §6：
     *   「局中被倒吊人摧毁只走 `on_delete`，`on_run_reset` 不会来」）。
     * ⚠ 这里**不用 signal_clear()**：那会把「信仰槽位注册 / HUD 槽位认领」也退掉，而 `on_load`
     *   每关都会再跑一次（幂等）；信仰本身**不清**（跨关保留，§3.2.1 只点名 small_rem，
     *   而 `faith_slot_reset()` 也不动 base / cap，见 `faith_core.h`）。
     * ⚠ 也**不碰**共用池：残留的点由 `faith_pool_tick()` 自然出界，比硬删温和（`on_delete` 那条路
     *   才硬收，而且现在只收 `1 << FAITH_SLOT_111` 自己的）。*/
    signal_card_t *s = st(c);

    if (!s) return;
    faith_signal_reset(&s->slot);
    signal_hud_kill();
    signal_declare_score(s);
}

static void on_delete(ce_card_t *c) { signal_clear(c); }

CE_CARD(111,
        .ctor = ctor,
        .on_load = on_load,
        .on_tick_2 = on_tick_2,
        .on_draw = on_draw,
        .on_item_score = on_item_score,
        /* ★ 2026-09-12 审计补：绿点的两条**来源**（大 = 击破掉落 +100 / 中 = 满 P 吃 P 点 +10）。
         *   补之前本卡一条来源都没有 ⇒ `signal_on_pickup()` 永不触发。小绿点走 `on_cancel`。*/
        .on_enemy_drop_pre = signal_on_enemy_drop_pre,
        .on_item_collect = signal_on_item_collect,
        .on_death_after_deathbomb = on_death_after_deathbomb,
        /* ★ 任务 3：`+0x34` = **每关开场**（§2.1 的「关卡结束归 0」落在关卡边界上）。*/
        .on_stage_start = on_stage_start,
        .on_run_reset = on_run_reset,
        .on_delete = on_delete);
