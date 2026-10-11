/* 花映塚 × 獣王園 合成卡（card-expand id 109）—— 卡侧接线。
 *
 * ★★★ 重建进度（owner 口径「从零、一点一点」）：
 *   ① 卡面美术 + 描述 + **主动卡**身份（`active_recharge` + `on_activate` ⇒ REFUSED，按 C 无作用）✅
 *   ② **暴风（連爆）** —— ★ 一手改用 **TH19**（owner 2026-10-04 口径）✅
 *   其余（消弹场 CA / 罩子 / 固灵结界 / 灵 / 灵力道具 / BOSS 态 / hits UI）**一行都还没接**。
 *
 * ══ ② 暴风（消弹场）· 一手 = **TH19 的「連爆触发」+「Bomb 族的扩张圈形状」** ═════════
 * ★★ 2026-10-04 定音（全部一手，见 `engine/versus/th19/05-four-mechanics.md` §9–§14）：
 *   · **触发**：TH19 的連爆 = **ECL 指令 `0x60`**（分派器 `FUN_004fecc0`，195 个 case 覆盖 0..195），
 *     它只做一件事：把敌人身上那两张**逐 kind 计数表**（`obj+0x502c` A / `obj+0x5080` B，
 *     各 21×4B，1-based）沿椭圆冲刷成实体（`FUN_0040f880` → `FUN_0050c6d0`）。
 *     ⇒ 全 exe 找不到硬编码的「小兵死了就炸」，因为**触发在 ECL 数据里** ✓（本卡用 `on_enemy_death` 等价表达）。
 *   · **形状**：TH19 里「扩张的消弹圈」的合法主人是 **Bomb / 攻击族**（逐字见于 `BombSanaeInf` 的 tick）：
 *     `entity+0xc8` 的 tween（`FUN_00402c30`）→ **半径**；同一个值写进 `vm+0x54`（VM 的 scale.x）
 *     ⇒ **判定 = 绘制**是这套的天然性质 ✓；然后
 *       `FUN_004ee770(pos, 半径, …)`（半径消弹）+ `FUN_00530c60(pos, …, **1**)`（半径命中，参数 1）
 *       + `FUN_005200f0(…, 10)`（生成物）。
 *     ⇒ 本卡的 `do_storm_frame()` 与它**同形**：半径消弹（本仓判据）+ `dmg = 1` 的半径命中，
 *       且 `storm_vm_sync()` 用**同一个半径**写 VM 的 scale ⇒ 判定=绘制 ✓
 *   · ⚠ **还没对上的一处（标 ❓）**：Bomb 那条 tween 的**键值与时长**没取到
 *     （`entity+0xc8/+0xf0/+0xf4` 不是立即数初始化，来自逐 Bomb 的参数/模板）⇒ **本卡的半径律
 *     仍借用 TH09 的节点值**（`r0 0 / dr 1.6 / 寿命 70 ⇒ 112`，一手 `th09:0x41fe93`，已逐指令核过），
 *     拿到 Bomb 的键值后要换。
 *   · **过滤**：owner 口径「th18 不存在白弹，用本仓那套」⇒ 弹半径 `+0x658 ≤ 4.0`（`pofv_is_white_bullet`），
 *     不照搬 TH19 的 `+0x1044 ∈ {1,2}`（TH18 无此字段）。
 *
 * ── 以下为 TH09 侧的对照（半径律的**临时**出处）─────────────────────────────────
 * ⚠⚠ **下面这一整段（2026-10-04 的「TH19 没有这条链」结论）已于 2026-10-05 被
 *   [`HANDOFF-109-STORM.md`](./HANDOFF-109-STORM.md) 推翻**（对象 = `ghost.anm` 16→17，
 *   触发 = `0x50c1f0` → `0x51d4e0` → `0x51cc70`）。**保留原文仅作教训**，不要照它实现。
 * 原文：**TH19 侧已核死：没有「击破小兵 → 消弹圈」这条链**（2026-10-04，全链见
 *   `engine/versus/th19/05-four-mechanics.md` §9.5–§9.8b）：
 *   · TH19 的**連爆**（`FUN_0050c1f0` → `FUN_0050c6d0`）= 20 类道具扇形撒布 + 逐个**通知能力卡**
 *     （容器 `FUN_004dda60` 加载 `ability.anm`/`abcard.anm`/`abmenu.anm`）—— **它不消弹**；
 *   · TH19 的「击破特效」（那个大气泡）= `enemy.anm` script 33/37/41/45/49/50/51，**贴图型**；
 *   · TH19 的**消弹**是**能力卡**的行为（半径 40 / 128 / 640）——`@MAMIZOU2` 的卡面文字
 *     「バリア破壊時に敵弾破壊」正对上半径 128 那处调用点 ✓；
 *   · 之前误当成「暴风」的 `0x53b920`（3 台 VM / 寿命 45 / 轨道 0→64 / 每台半径 64 / 死后 128→192）
 *     是**一个死基类 vtable 的槽** ⇒ **整组作废**。
 *   ⇒ 能照抄的「逐帧扩张的消弹场」只有 **TH09**（= `HANDOFF-109.md` §2.2 的表）：
 *     节点 = (圆心, r0, dr, 寿命)；本卡用「自机弹命中」那条 `th09:0x41fe93` =
 *     `r0 0 / dr **1.6** / 寿命 70` ⇒ 终点 **112 px**。
 *     ⚠ **2026-10-04 订正**：HANDOFF §2.2 曾说「构造函数 `0x41d050` 里有 `fmuls [0x490f54] = ×0.5`」
 *       —— 反汇编 `0x41d050` 后**证否**：它是 `mov [eax+8],r0 / mov [eax+0xc],dr` **直接存、无乘法**
 *       （`[0x490f54] = 0.5f` 存在但不在这条路上）。三路互证见 `pofv_core.h` 文件头。
 *   · **触发** = owner 口径「**杂鱼死掉时创建**」（boss 不算）—— 与 TH19 的連爆触发点同源
 *     （都挂在敌人死亡处理上）。
 *   · **消弹** = 本仓设计的**可消弹判据**（弹半径 ≤ 4.0；TH18 没有 TH19 那套「小弹」字段）。
 *   · **画面**（2026-10-04 已核到一手，结论是**没有脚本可抄**）：TH09 那一下的视觉是
 *     **代码逐点拼出来的卷环** —— `th09:0x41fcd0` 并排起「判定节点」与
 *     `FUN_0040cd70(..., effect id **0x22**, pos, 6, -1)`，而 0x22 的回调 `0x444a20`/`0x444af0`
 *     是「按角迭代 + `FUN_00441890`（cos/sin 极坐标）+ `AnmManager__flush_sprites`」⇒ **不走 anm**
 *     （详见 `engine/card/th09/01-pofv-charge-and-cancel-fields.md` §5）。
 *     ⇒ 本卡**没法「复刻脚本」**，只能用 anm 的 `drawPoly` 做**观感近似**：
 *     `layer(6)` 两段（script 396 外层黑边/芯透 + 397 内层褐 ×0.7），观感按 owner 口径
 *     （从外到内 = 黑边 + 褐色 + 透明，不带亮边/彩虹）；大小由卡侧每帧写（`scale = 场半径`），
 *     脚本里不写 `scale`。★ **注释里一律写「近似」，不许写成「复刻」**（`METHOD.md` 铁律）。
 *
 * ══ 长持续效果纪律（CLAUDE.md / METHOD.md 铁律）════════════════════════════════════
 *   本卡跨帧留下的东西 = **暴风场池** + **每场 3 台跟随 VM**。
 *   两条删卡路（倒吊人 / 原版空白卡弃整副牌）**都发生在商店里**，那一刻 `on_tick_2` 停摆 ⇒
 *   全部在 `clear_card()`（`on_run_reset` / `on_delete` 共用）里收 —— **完全不依赖 tick** ✓。
 */
#include "sdk.h"
#include "pofv_core.h"
#include "faith.h"   /* ★ 2026-10-09：借用共享落点散布 faith_scatter_from_enemy() ✓（见 AUDIT §BE）*/

/* ============================================================ 常量 */

/* ★ 主动卡的「充能时长」（帧）。⚠ `on_activate` **每次都 REFUSED**（按 C 无作用），
 * 这个值只决定引擎眼里「这张主动卡的冷却有多长」⇒ 取一个常规值即可 ❓ 自定。 */
#define POFV_ACTIVE_RECHARGE        60

/* 画面：脚本里写死 `layer(28)`，卡侧传同一支（TH19 三台同层）。 */
/* ★★ 层号必须是**世界层 3–19**（24+ = UI 层、1 单位 = 1 表面 px；且 `layer(20..32)` 会派生
 * `resolutionMode` 模式 1 ⇒ 位置三分量 ×2 —— 旧版 `layer(28)` 就是这样把暴风画偏的）。
 * 取 **6**（一手 `effect.anm` script20 的层号同支）。脚本里也写 `layer(6)`，两边一致。 */
#define POFV_STORM_VM_LAYER          6

/* 一次消弹最多认几颗（原版 `FUN_004ee770` 传 99999）。 */
#define POFV_CANCEL_MAX          99999

/* ★★ 消弹：**不用** `ce_cancel_radius`（引擎 `0x429370` 的「带保护位就跳过」那种半径消弹），
 *   因为 **TH18 的弹上没有 TH19 那套「小弹」字段**（owner 口径：「th18 不存在白弹，所以用我之前
 *   设计过的能消那些弹的判据」）⇒ 自己扫弹池、按 `pofv_is_white_bullet()`（弹半径 ≤ 4.0）逐颗筛、
 *   逐颗调 `Bullet__cancel(b, mode)`。判据与出处见 `pofv_core.h` 的「可消弹的判据」。 */
#define POFV_FN_BULLET_CANCEL   0x428e90
typedef void (__attribute__((thiscall)) *pofv_fn_bullet_cancel_t)(void *bullet, int mode);

/* `0x658` = `zBullet.radius`（出生时从弹种表 `0x4c5f90` stride `0x118` 的 `+0x104` 拷入：
 *   一手 asm `0x425998 movss xmm0,[eax+0x4c6094]` → `0x4259a8 movss [edi+0x658],xmm0`）。 */
#define POFV_BULLET_RADIUS_OFF  0x658

/* 消掉的可消弹走 `mode = 1`：引擎 `0x4290e0` 会**加分 + 掉点道具**
 *   （原版「被消的白弹投到对手场地」在单机没有对应 ⇒ 用户口径改成转成分）。 */
#define POFV_BULLET_CANCEL_MODE     1

/* ============================================================ 灵力道具（自建实体池）
 * 纯逻辑在 `pofv_core.h` 的 §E（`pofv_power_*`）—— 这里只放**卡侧接线**：
 *   · 池（文件级 `s_power`）；
 *   · 两个**生成口**（owner 2026-10-08 定案）：① 击破敌人 ⇒ 1 颗（位置 = 敌机位置）；
 *     ② 固灵结界碰到敌方弹幕 ⇒ 从**该弹的原位**撒（**必定掉、不做概率模型** ✗）；
 *   · 每帧推进 + 收取入账（本卡的「攻击槽」= `s_ex_gauge`）；
 *   · 每颗的**可视 VM**（`power_vm_sync()`：延时结束建 / 每帧跟随 / 所有移除路径删）；
 *   · 出生那一刻另起**一台几何闪光**（同一处建；一手 `effect.anm` script94）；
 *   · `clear_card()` 收干净（铁律：长持续效果删卡要交代）。
 * ★★★★ 2026-10-09 视觉身份换正（owner 明确否掉「借 116 季节道具」✗）—— 改用**一手 TH19 美术**：
 *   ① 本体 = 一手 `bullet.anm` **script123**（`local/th19.v1.10c/_ext/bullet.txt`）⇒ 新脚本 **423** +
 *      新贴图 `TH19_ITEM_KIND9`（= 一手 `sprite517` 的矩形 `(209,33,14,14)`，从 `bullet/item.png` 原样切）；
 *   ② 闪光 = 一手 `effect.anm` **script94**（同目录 `effect.txt`）⇒ 新脚本 **424**（加算空心环、20 帧自删）；
 *   接线形状仍照 116（`season.c` 的 `point_spawn_vm()` / `points_tick()`）：spawn → set_pos/sprite/color
 *   → 每帧跟随 → 移除时 `ce_anm_delete` + 句柄清 0。
 *   ★ 为什么是 kind 9：一手「拾取加槽」只发生在 **kind 9–14**（kind 1–8 拾取只播 SE + 删除）⇒ 本卡道具用
 *     kind 9 的外观（负结论已登记 `TUNING.md`）。*/
#define POFV_POWER_SE_GET       0x25   /* ★ 一手：收取音（TH16 季节道具的拾取音 `0x25`）*/
#define POFV_POWER_SE_LEVEL     0x3f   /* ★ 一手：**升档音** —— TH16 季节道具升档（`item_collect_season`）
                                        *   播 `0x3f`（116 `season.c` 的 `SEASON_SE_LEVEL` 同源；
                                        *   借做法已登记 `AUDIT §BE #166`）。本卡的对照事件 = EX 就绪阈值。*/

/* ★★ 伤害：一手 = `0x53b7ef movss xmm2,[0x5aa368]`（**64.0**）+ `push 1` → `FUN_00530c60(blob, 64.0, 1)`，
 *   而 `FUN_00530c60` 把 `xmm2` 与最后的 `1` 转给每个目标的 `vft+0x30(pos, 64.0, 0, 1)`
 *   ⇒ **TH19 的暴风没有「扁平伤害数」**：它是「每帧、每台、半径 64 的一次命中、参数 = 1」，
 *     结算交给目标自己的处理。
 *   ⇒ TH18 侧落成「每帧每台一个半径 64 的矩形伤害源、`dmg = 1`、活 1 帧」（活 1 帧 ⇒ 当帧结算一次，
 *     下一帧重建 ⇒ 与一手的「每帧都判」同形）。 */
/* 伤害常量统一在 `pofv_core.h`（`POFV_STORM_HIT_DMG` / `POFV_SPIRIT_HIT_DMG`）——卡侧不再重复定义
 * （曾因重复定义导致「重新设计的 10」被旧的 6 盖掉）。 */
#define POFV_STORM_HIT_LIFE          2    /* ★ 伤害源寿命（帧）。本仓既有值一律 2：`bomb10.h` 的
                                           * `BOMB10_DMG_SRC_LIFE`、`broken_core.c` 的 `BC_HIT_LIFE`
                                           * （注释原文：「2 帧是给结算留的余量」）—— 取 1 会让源在敌人侧
                                           * 结算（`0x45f0f0`）轮到之前就消失 ⇒ **伤害落不下去**（实跑就是如此）。 */

/* ★ 场地的世界坐标范围（一手：弹幕区 x ∈ ±192、y ∈ [0,448]）⇒ 伤害矩形要裁到这里面，
 * 否则场心靠边时矩形溢出场地，会把**还没进屏**的敌人打死（owner 报过的真 bug）。*/
#define POFV_FIELD_HALF_W        192.0f
/* ★ 2026-10-06：灵的**横向钳位**（一手：`FUN_0051b6c0` 把灵 x 钳在 [-140,+140] ✓）——
 *   水獭「碰边折返」必须用**这个**阈值（不是场地半宽 192 ✗）；❓ 一手精确钳位/余量待核 ✓ */
#define POFV_SPIRIT_X_CLAMP      140.0f
#define POFV_FIELD_H             448.0f

/* ============================================================ 池（文件级） */

/* ⚠ 池是**文件级静态**（不是 `ce_state`）—— 一张卡的 `ce_state` 只有 496 B，装不下 16 个场
 *   × 3 台 VM；照 116 / 117 的先例（`animal.c` 的 `s_tok`）。
 *   代价：所有清理必须**不依赖 tick** ⇒ 全在 `clear_card()` 里。 */
static pofv_storm_pool_t s_storm;
/* ★★ 2026-10-01（口径 5）：**灵力道具池**（自建实体，**不在引擎道具池里** ⇒ 只能本卡自己清）。
 *   owner 2026-10-08 定案的两个生成口见 `on_enemy_death`（击破）与 `on_tick_2`（结界碰弹）。*/
static pofv_power_pool_t  s_power;
static int32_t            s_power_got;      /* 本次段累计收取的灵力道具颗数（诊断）*/
static int                s_power_logged;   /* 「灵力道具池满丢弃」只记一行（防刷屏）*/
static int32_t            s_aura_bullets;   /* 结界累计碰到的敌弹颗数（诊断）*/
/* ★ 结界的**逐弹闩锁**：每颗弹只算一次（一手 TH19 `FUN_004ee770` 之后弹就没了 ⇒ 天然一次；
 *   我们只「碰」不消 ⇒ 必须自己闩锁，否则同一颗弹每帧都会掉道具 ✗）。*/
static uint8_t            s_aura_bullet_done[CE_BM_BULLET_COUNT];
/* ★★ A 路（owner 2026-10-05 选）：灵 = **引擎真敌机**（`ce_spawn_enemy`，108 使魔/112 令牌同款原语）。
 *   ⚠ SDK 明令：**禁止在敌人死亡栈里调** `ce_spawn_enemy`（DEATH §2.4 重入）⇒
 *     死亡回调里**只入队**（`s_spirit_q`），真正的生成放在 `on_tick_2`。 */
#define POFV_SPIRIT_Q_MAX 4
static float   s_spirit_q[POFV_SPIRIT_Q_MAX][2];
static int32_t s_spirit_q_kind[POFV_SPIRIT_Q_MAX];
static int32_t s_spirit_q_level[POFV_SPIRIT_Q_MAX];
static uint8_t s_spirit_q_inc[POFV_SPIRIT_Q_MAX];   /* ★ Q4 队列侧来源标志（1 = incoming）*/
static int   s_spirit_q_n;
static int   s_point_q;
#define POFV_PURE_SPAWN_INTERVAL 180   /* ❓ 純霊自动生成节拍（一手只说「只能自动生成」）*/
#define POFV_GHOST_SEND_COOL 30   /* ❓ 送出冷却（一手是「每次命中且 state 2/3」，未给频度）*/
static int   s_ghost_cool;
static int   s_pure_kill_n;
/* ★ ⑥ 比赛时长 → cap：本仓 SDK 没有时间口 ⇒ 用**卡自己的帧计数**换算秒（per-frame tick ✓）。
 *   一手：每侧上限按比赛 60/120/180 s 分档 ⇒ **60 / 80 / 999**（`pofv_spirit_cap_for()` 已实现且单测覆盖）。
 *   ❓ 用「本卡在场帧数」近似「比赛时长」（换卡再切回来会从头算）。 */
static int32_t s_card_frames;
static int32_t s_aura_stamp = -1;   /* ★ 固灵结界的「按住 C」时间戳（定义在下方 `pofv_active_tick`）*/
static int32_t s_aura_vm;            /* ★ 固灵结界的**可视 VM**（一手 `effect.anm` script1 的复刻 402 ✓）*/
/* ★ 咲夜扇形：**7 片三角**（407-413 ✓，owner「多片三角 + 多片三角来做渐变」✓；每片自带朝向与颜色 ✓）*/
#define POFV_AURA_PIECES 7
static int32_t  s_aura_piece[POFV_AURA_PIECES];
static float   s_aura_r;              /* ★ 本帧的结界半径（= 缩放 ✓，一手「槽 3 写缩放=半径」✓）*/
static float   s_aura_charge;         /* ★ **蓄力**（= 一手 `AuraCtrlInf+0x94/+0x98` 的等价物 ✓；按 **C** 涨、
                                       *   松手归零 ✓）—— **只给「蓄力条 / A 组」用** ✓，并被 `s_reiryoku`
                                       *   （= 上限量）封顶 ✓（见其赋值处 ✓）。*/
static float   s_aura_size;           /* ★★★★ 2026-10-10 owner（**确实是两个量，只是游戏设计一个键管** ✓）：
                                       *   **结界尺寸** = 一手 `S+0x8c` 的等价物 —— **只有它**驱动结界半径/判定
                                       *   （`s_aura_r` 由它算 ✓）。一手 `S+0x8c` 是**独立量**（0..200，默认恒定；
                                       *   全 exe **唯一写点** `0x4085cc` = `@AUN2` 卡写 **125** ✓，见 `06-aura.md`）。
                                       *   ★ owner 定案「**蓄力与张开是同时发生的**」「**目前的手感应当是一手**」✓
                                       *   ⇒ 本卡让它与**蓄力同曲线/同值**（**C** ⇒ 同时推蓄力 + 尺寸 ✓；
                                       *     **Shift** ⇒ **只推尺寸**、不推蓄力 ✓）—— 相对一手的两个独立量，这是
                                       *     **❓自定**（数值/观感与改前完全一致 ✓），已登记 `TUNING.md` ✓。*/
static float   s_aura_k;              /* ★ 本帧的 K（灵梦 64 / 魔理沙 14 / 咲夜 ? / 早苗 128 ✓）*/
/* ★ #2 追赶动画层（TUNING 判「原版有 ⇒ 建议补」✓，一手 `06-aura.md` §10 ✓）：
 *   `半径(f) = 初值 + (目标 − 初值) × ease(模式, f)`，`f = 已过帧 / 时长`；走完保持目标 ✓；
 *   初值 = `+0x6c` = **1.0** ✓；时长/模式逐类：灵梦/魔理沙 **20 / 4**（`1−(1−f)²` ✓）、早苗 **60 / 2**（`f³` ✓）。*/
static int     s_aura_anim_f;         /* 本段张开的已过帧 ✓ */
/* ★ 咲夜（TH09 ✓）专有状态：半张角（度 ✓，每帧 +3 上限 90 ✓）、方向参考（**向量** ✓，朝 8 方向 0.25/帧缓动 ✓）、
 *   上一帧自机位置（求移动方向用 ✓，不引 libm ✗）。*/
static float   s_aura_sec_deg;
static float   s_aura_dir_x, s_aura_dir_y;
static int32_t s_aura_dir8i;           /* 最近 8 向索引（仅用于**取目标角** ✓）*/
static float   s_aura_mid_ang;
static int32_t s_aura_dir_snap;        /* ★ 新激活首帧：中线角**直接吸附到目标** ✓（否则从上次朝向绕圈 ✗）*/
static int32_t s_aura_close;
static float   s_aura_close_ang;         /* ★ 领域中线角（**连续缓动** ✓；owner：8 向瞬跳 ✗ 不合格 ✓）*/
static float   s_aura_px, s_aura_py;
static float   s_aura_anim_cur;       /* 上一帧的实际半径（追赶的起点 ✓）*/
static int     s_aura_kind;           /* ★ 0=灵梦 1=魔理沙 2=咲夜(TH09) 3=早苗 ✓ */
/* ★★★ 2026-10-06 goal ②③：**自维持定位用的四类计数** ＋ **安全阀** ✓（owner：「**我不要自维持**」✓）：
 *   · 计数：送出(`take54`/`take30`) / 回程到达 / §4.5 白给 / 击破 ✓ ⇒ 每 300 帧打一行（含两侧在场数 ✓）；
 *   · 安全阀（**❓自定，阈值先取 20**，待 owner 定 ✓）：本侧在场 ≥ `POFV_SPIRIT_SPILL_MAX` ⇒ **暂停送出** ✓
 *     （保证「一手绝不会到的数量」时不再继续堆 ✓）。*/
static int pofv_spirit_live_count_fam(int fam);   /* ★ 前向声明（定义在后 ✓）*/
#define POFV_SPIRIT_SPILL_MAX 20
static int s_cnt_send54, s_cnt_send30, s_cnt_arrive, s_cnt_gift, s_cnt_kill;
/* ★★★ 2026-10-06 EX 骨架（依据：一手 `08-extra-attack.md` §6/§8.1/§8.2 ＋ 社区「攻撃ランク」✓）：
 *   · `s_ex_gauge` = **本侧**那本「攻撃ランク」（0..200 ✓、**1P/2P 独立** ✓、与 X 蓄力 `s_aura_charge` **分开** ✓）；
 *   · 判据（`§8.2` 一手 ✓）：「**击破动物灵 ⇒ 返还一只灵给对面；若本侧此刻「EX 可发」⇒ 这次产出改为该角色的 EX 攻击（不回程）**」✓；
 *   · 「EX 可发」= `s_ex_gauge >= 阈值表[s_ex_level]` ✓ ⇒ `s_ex_gauge -= 阈值` ✓（**不清零** ✓ 一手 ✓）；
 *     阈值表 = **一张** `[75,65,65,55,50,45,45,40]` ✓（`0x59dcc8` ✓，**按等级索引** ✓ —— 我先前"18 张"的说法是错的 ✗ 已纠正 ✓）；
 *   · 增长来源（社区 ✓ 单源 🟡）：**消弹 / 消動物灵** ⇒ 上涨 ✓，每侧独立 ✓、0..200 ✓。
 *   ❓ 自定（登记 `TUNING` ✓）：① 每次增长步长 `POFV_EX_GAUGE_STEP`（社区未给 ✗）；
 *     ② 等级 `s_ex_level` 的取法（一手是引擎全局 `DAT_0062b250` 0..7 ✓；我们暂取 0 ❓）；
 *     ③ **EX 敌人本体（`ExAttack` / `ExAttackW/O/E`，HP 200/2400/4000 ✓）尚未实现** ✗ —— 缺它的敌机名/ECL ❓。*/
#define POFV_EX_GAUGE_MAX   200
#define POFV_EX_GAUGE_STEP  15       /* ★ 一手配方（`EX-GAP-NOTES.md` §11.1 ✓，不再自定 ✓）：
                                      *   击破（死亡类别 1 ✓）且**未在冷却**（`holder+0x34 == 0` ✓）⇒ `*(holder+0x20) += 15` ✓ */
#define POFV_EX_FIRE_AT     100      /* ★ 一手：`> 99` ⇒ 发射 EX（即 **≥100** ✓）*/
#define POFV_EX_COOL        60       /* ★ 一手：发射后冷却 `+0x34 = 60` 帧 ✓（期间不再积累 ✓）*/
/* ★★★ 2026-10-06 一手 `08-extra-attack.md` §8.38 ✓：「**40**」= 实体**进入状态域 1 的次数**（≈ 被击破/被收的灵数 ✓），
 *   **每侧各自 40** ✓、**每局重置** ✓、**局内单调** ✓（无重置点 ✓）⇒ **到 40 之后本局该类通路永久不再送灵** ✓（真的能脱离循环 ✓）。
 *   ⚠ 注意：脱离循环只指**双方不再互送灵** ✓ —— 对面仍可能吃到 **EX** ✓（§8.36 ✓）。*/
#define POFV_SEND40_LIMIT   40
static int32_t s_send40;             /* ★ 本侧「进入状态域 1」的计数 ✓（≈ 我方灵被击破/收掉数 ✓）*/
static const int32_t s_ex_thr[8] = { 75, 65, 65, 55, 50, 45, 45, 40 };   /* ★ 一手 `0x59dcc8` ✓（等级 0..7 ✓）*/
static int32_t s_ex_gauge;
static int32_t s_ex_cool;            /* ★ 一手：发射后冷却 60 帧 ✓（`holder+0x34` ✓）*/
static int32_t s_ex_level;           /* ★ §8.24 订正 ✓：`DAT_0062b250` **不是随时间涨** ✗ —— 它是
                                      *   **每局开局的设定值**（实测出现 1 与 3 ✓；社区难度上限 EASY10/NORM60/HARD80/LUNATIC100 ✓
                                      *   只对应"起点/上限" ✓）⇒ 卡侧应**每局设定一次** ✓；❓ 难度→索引(0..7) 的映射待定 ✓，
                                      *   现暂取 0 ❓（"局内再升"属**自定** ✗ 一手无 ✓）。*/
/* ★★★★ 2026-10-10 owner 语义（**两个量必须分开** ✓，原话：「我特别区分的是：**灵力和蓄力，
 *   灵力 = 吸收灵力道具涨的 = 蓄力上限**」✓）：
 *   · **灵力** = 本变量 `s_reiryoku`（0 .. `POFV_AURA_TARGET_CHARGE`）—— **吸收灵力道具累积** ✓；
 *   · **蓄力** = `s_aura_charge`（按住 **C** 增长、松手归零 ✓）；
 *   · 入账口**不新开**：走本卡**既有**那条收入 = `pofv_power_tick(..., &got_pts)` 的 `got_pts`
 *     （同一笔钱在 `on_tick_2` 里也进 `s_ex_gauge` ⇒ 灵力只是**同一收入**的另一本账，没有第二套计分 ✓）。
 *   ⚠ 现状（如实登记，未擅自改经济）：`s_ex_gauge`（攻击槽）**不是**纯累积量 —— 它还被
 *     ① 击破动物灵 **+15**（`pofv.c` 的 `on_enemy_death`）、② 发 EX **−100**、③ **每帧 −1** 衰减
 *     ⇒ 若直接拿它当「灵力」会**边涨边掉**、与 owner「积蓄到哪」相冲 ✗ ⇒ 故本卡新增这本**只进不出**的
 *     账（`s_reiryoku`）。它的**满值**直接取本卡既有的蓄力满值常量 `POFV_AURA_TARGET_CHARGE`
 *     （= 100 ✓，owner 实跑定）—— **没有新造数值** ✓。
 *   ★★★★ 2026-10-10 owner 追加（**照一手 `S+0x24` 接上** ✓，`engine/versus/th19/05-four-mechanics.md` §1 ✓）：
 *     · **初值 = C1（不是 0 ✓）**（一手：`S+0x24` 初值 = `clamp(S+0x28,0,2500)` = C1 阈值 ✓）
 *       ⇒ 开局就有基础灵力 ⇒ 结界不会"开局为 0" ✓；C1 数值一手未解出 ⇒ **❓自定** `POFV_REI_C1` ✓；
 *     · **拾道具** = `+= 15 × clamp(当前蓄力,0,100)/100` 的**同式**（一手 `S+0x24 += 15 × clamp(S+0x98,0,100)/100`
 *       ✓，每颗 ✓，cap 2500 ✓）—— 本卡在既有入账口 `got_pts`/`got_n` 处按**颗数**换算 ✓；
 *     · **当前蓄力被它封顶**（一手：`charge < clamp(S+0x24)` 才涨 ✓）⇒ 见 `s_aura_charge` 赋值处 ✓。*/
static float s_reiryoku;
/* ★★ 审计 B 抓到 ✓：`s_rei_prev` 原来是**函数内 static** ✗ ⇒ `clear_card()` 复位了 core 的
 *   `absorbed_rei`（= 0 ✓）却**没复位它** ✗ ⇒ 换关/删卡重建后 `rei_now(0) < 旧值` ⇒
 *   ★ 头"旧值"颗蓝点**不入账** ✗（守卫只跳过、不回退 ✓）。
 *   ⇒ ★ 提到文件级 ✓ 并在 `clear_card()` 里归 0 ✓。*/
static int32_t s_rei_prev;

/* ★★★★ 2026-10-11 owner 授权（「你可以用子机槽来做到这件事」✓）：
 *   ★ 用 SDK 的「**低速子机声明层**」✓（`sdk.h` spec §3.3.1 ✓）给**早苗**一个子机 ✓，
 *   ★ 用途 = 让她的 C1（蛇 / set10）与 C2（青蛙 / set11）**从子机坐标出膛** ✓
 *     （= 一手弹表的 `class 5`「子机槽」✓ + owner：「早苗原生低速子机打的就是这个弹幕」✓）。
 *   ★ 纪律（SDK 原文 ✓）：① **每帧声明**（不声明 = 退出 ✓）；② **落笔只在 `on_option_pose_apply`** ✓、
 *     退位（`ce_option_pose_winner() != 本卡`）⇒ **只收不写** ✓；③ 偏移单位 = **像素** ✓
 *     （`fx/fy` 聚焦 ✓、`ux/uy` 非聚焦 ✓ —— 两套都给 ⇒ Shift 切换无延迟 ✓）。*/
static ce_option_t      s_opt_sanae;      /* 早苗的子机槽 ✓ */

static int pofv_spirit_spill_stop(void)
{
    /* ★★★ 2026-10-06 owner 裁定（两侧语义 ✓）：
     *   · **本侧发 EX ⇒ 暂时不会给对面灵** ✓ ⇒ 本函数**同时**把「出循环」当成停送条件 ✓
     *     （`pofv_foe_spirit_loop_exit_any` ✓）—— 这样两条阈值送出都被挡住 ✓；
     *     对面侧的「暴风/§4.5 直接 spawn」由**模型自己的 `spawn` 拒绝** ✓（出循环 ⇒ 不产生灵 ✓）。
     *   · **对侧发 EX ⇒ 不会返回来灵** ✓ ⇒ 由模型 `mirror_kill` 跳过 `loop_exit` 项 + `spawn` 拒绝共同保证 ✓。
     *   ⚠ 「出循环」的**触发条件**仍待重做 ✗ ⇒ 卡侧**只留接口**（`pofv_foe_spirit_loop_exit()`）✓，暂不调用 ✓。*/
    const int live = pofv_spirit_live_count_fam(POFV_FAM_TH19) + pofv_spirit_live_count_fam(POFV_FAM_TH09);
    /* ★ TODO（下一轮 ✓）：owner 裁定「**本侧发 EX ⇒ 暂时不给对面灵**」⇒ 这里还应加
     *   `if (pofv_foe_spirit_loop_exit_any(s_foe, POFV_FOE_SPIRIT_MAX)) return 1;` ✓
     *   ⚠ 但 `s_foe` 的声明在**本函数之后** ✗ ⇒ 直接引用编译不过 ✓（我试过 ✓）
     *   ⇒ 下一轮把 `s_foe` 的声明**前移**（或给本函数加一个访问口 ✓）再接 ✓，避免半截改动 ✗。*/    if (live >= POFV_SPIRIT_SPILL_MAX) {
        if (!(s_card_frames % 60)) ce_log("pofv: 【安全阀】本侧在场 %d ≥ %d ⇒ **暂停送出** ✓（❓阈值待 owner）", live, POFV_SPIRIT_SPILL_MAX);
        return 1;
    }
    return 0;
}
static int     s_aura_role_logged;    /* ★ 只为「钉死 0x4cccf4 的角色映射」打一次日志 ✓（核实建议 ✓）*/
static int     s_aura_is_marisa;      /* ★ 本帧走哪套（❓ 角色读取点未定 ⇒ 现恒 0 = 灵梦 ✓）*/
static int s_ex_pending;
/* ★★★ W6 消费端（第一件）：把镜像事件**真送出去** —— 按 `pofv_mirror_emit()` 的三条硬口径：
 *   ① 己方弹幕（我们自己 spawn 的 VM ⇒ 天然符合）② **从版顶长出** ③ **50% 透明度起步**（非己方样式）。
 *   ❓ 载体暂时用现成的「附属圆」脚本当「送过去的东西」（形似而非弹幕）；做成真弹幕要另找载体。 */

static int32_t s_pure_kill_n_abs;
/* ★★★ 对面侧模型（owner 定稿：纯数据、无可见物 —— `pofv_core` 的原语） */
static pofv_foe_spirit_t  s_foe[POFV_FOE_SPIRIT_MAX];
/* ★★★ MODEL §1-3 / §7-3「容量 60/80/999（TH19）· 25/10（TH09）；**满了不生成、折算成计数**（不是丢包 ✗）」
 *   ⇒ 对面模型的**唯一**生成入口 ✓（卡侧所有送出都走这里 ✓） */
static int pofv_foe_spawn_gated(float x, float y, int32_t kind, int32_t level)
{
    /* ★★★ 核实报告整改（2026-10-05）：①**分族计数**（原用全局 count ⇒ 一族占满会堵死另一族 ✗）
     *   ②**cap 大于池容量（64）时也必须折算**（原 raw spawn 返回 −1 ⇒ 真丢包 ✗）*/
    int fam = (kind == POFV_SPIRIT_KIND_GHOST_TH09) ? POFV_FAM_TH09 : POFV_FAM_TH19;
    int cap = pofv_spirit_cap_for(fam, s_card_frames / 60);
    int i, live = 0, n = POFV_FOE_SPIRIT_MAX;
    for (i = 0; i < n; ++i) {
        if (!s_foe[i].active) continue;
        if (pofv_spirit_family_of(s_foe[i].type) == fam) ++live;      /* ★ 只数**本族** ✓ */
    }
    /* ★★★ 撤回（2026-10-05，核实复核）：上一版我按文档 `:46` 的「我→对面 也该配一个 10」在这里加了
     *   落地门，**但两条门数的是同一个 `live`（本族在场数）** ⇒ `10 < 25` ⇒ 对 TH09 而言 **10 永远先生效、
     *   `25`（在场灵数门）成为死门** ✗ —— 与文档 `:43`「25 = 收件方**在场灵数** / 10 = 收件方的
     *   **落地·寄存**」的两种语义不符 ✗。
     *   ⇒ **撤回**（先让**在场灵数门 25** 照文档生效 ✓）；「10 = 落地/寄存」该数**哪一个计数器**
     *     （在途/寄存数？）文档未给 ⇒ 标 ❓ **等 owner 一句**，我不猜 ✗（`POFV_TH09_LANDING_CAP` 常量保留 ✓）。*/
    if (pofv_spirit_room(live, cap)) {
        {
            int32_t fi = pofv_foe_spirit_spawn(s_foe, n, x, y, kind, level);
            if (fi >= 0) {
                /* ★★★ 2026-10-08（owner 一手 §3.31 ✓）：**对称幽灵**与真幽灵**同一条规则** ——
                 *   出生抽一次角度（约 1/5 朝自机 ±22.5°、其余正下方 ±0.9° ✓）。
                 *   ❓ 对面的「自机」本卡没有实体 ⇒ 取**本侧自机**（镜像口径；owner 未逐字给 ⇒ 标 ❓ ✓）。*/
                if (kind == POFV_SPIRIT_KIND_GHOST_TH09) {
                    const float *pp = ce_player_pos();
                    float ang = 1.5707964f;
                    pofv_ghost_spawn_angle(x, y, pp ? pp[0] : 0.0f, pp ? pp[1] : 0.0f,
                                           (uint32_t)ce_rand(), (uint32_t)ce_rand(), &ang);
                    pofv_foe_spirit_set_ghost_angle(s_foe, n, fi, ang);
                }
                return 0;
            }
        }
    }
    ++s_point_q;                 /* ★ 满（含 cap>池 的溢出）⇒ **折算成计数**（不丢包 ✓）*/
    ce_log("pofv: 对面模型满（fam %d live %d / cap %d / 池 %d）⇒ 折算成计数（不丢包 ✓）", fam, live, cap, n);
    return -1;
}
static pofv_foe_incoming_t s_inc[POFV_FOE_INCOMING_MAX];
/* ★ MODEL §5-1：**出生位置 = 版顶随机**（与 TH19 同口径 ✓；TH09 幽灵同样 ✓，不再用我方击破点 ✗）*/
static float pofv_top_x(void) { return ((float)(ce_rand() % 321) - 160.0f); }
static float pofv_top_y(void) { return 16.0f + (float)(ce_rand() % 33); }

/* ★ 連撃/破坏累积（owner 一手：两档阈值 + 保余数）＋「最近一次破坏位置」（一手 `+0x58`/`+0x60` ✓）*/
static pofv_acc_t s_acc;
/* ★★★ 2026-10-08（owner 转来的核查 ✓）：TH09 的「**時間衰减**」在卡里完全不存在 ✗ ——
 *   一手（`engine/versus/th09/02-ghost-and-seal.md` §2.5 ✓）：連擊槽的 `+0x14` **Timer 满 105 帧** ⇒
 *   連擊槽**漏空** ⇒ **SP 归零** ✓（「漏空 ⇒ SP 归零」这条一手无争议 ✓；**怎么漏**的指令仍未定位 🟡）。
 *   ⇒ 本卡实现：**距上次命中的帧数** ≥ 105 ⇒ `a2c = 0`（⇒ 既有的「a2c ≤ 0 ⇒ SP 归零」自然生效 ✓）。
 *   ❓ 一手的「逐帧漏」vs「到点清空」未逐字核 ⇒ **本处取「到点清空」＝❓自定** ✓（不许写成复刻 ✗）。*/
/* ★★★★ 2026-10-08 **一手全链结案**（owner 转来 ✓，`engine/versus/th09/02-ghost-and-seal.md` §3.34 ✓）：
 *   · **連擊槽 tA**（一手块 `+0x20`）：**每帧 −1** ✓；击破 ⇒ **分档加长**：
 *       `<30 ⇒ +30` / `<50 ⇒ +5` / `<60 ⇒ +2` / `≥60 ⇒ +0` ⇒ **上限 60** ✓；
 *     漏空（tA = 0）⇒ 清 **連爆数**（一手 `+0x38` ✓，送量 `min(,100)×10` 用的是它 ✓）✓。
 *   · **SP 寿命 tB**（一手块 `+0x14`）：击破 ⇒ **`tB := tA + 45`** ✓；每帧 −1 ✓；
 *     漏空 ⇒ **清 SP**（一手 `+0x10` = `player+0x30420`，归零指令 `0x41d7cf` ✓）✓。
 *   ⇒ **「105 帧」的出处 = 槽上限 60 + SP 寿命偏移 45** ✓（观感两段式：停手 ≈1s 槽漏空 ⇒ 再 ≈0.75s SP 归零 ✓）。
 *   ⚠ 作废我上一版的「105 帧到点清空」❓自定 ✗（那是我在一手未明时的近似 ✓，现按一手改逐帧 ✓）。*/
static int32_t s_combo_blasts;  /* = 一手連爆数 `+0x38`（送量来源 ✓，清空时归零 ✓）*/
static int32_t s_sp_life;       /* = 一手 SP 寿命 tB `+0x14`（漏空 ⇒ SP = 0 ✓）*/
#define POFV_COMBO_CAP 60
/* ★★★ 2026-10-08 订正（owner 转来 §3.41 ✓）：**hits 的接续窗口 X = 36 帧** ✓ ——
 *   一手：那是**敌人连锁击破的暴风**（`FUN_004102b0` ⇒ `FUN_0041d110(…, 类别+8 阶段, 每阶段 4 帧 ✓)`）
 *   ⇒ 窗口 = **`(cVar2 + 9) × 4`** 帧 ✓（类别 0 ⇒ **36** ✓，显著短于連擊槽 60 ✓）。
 *   ⚠ 我上一版拿「玩家蓄力暴风」那族（48/64/128/90/120 ✗）当 X ⇒ **作废** ✗（结案已标为另一个系统 ✓）。
 *   「断」的判定（一手 `FUN_004102b0` 入口 ✓）：`param_1 != 0`（这一击**不属于当前连锁**）⇒ 清連爆数 +
 *   归零槽 Timer ✓ ⇒ 代码层等价物 = **距上次连锁暴风 > X** ✓（就是我们这条计时器 ✓）。*/
/* ★★★ 2026-10-08 owner 口径（class 表 ✓）：「不动暴风 ✓，只按不同 class 的 TH09 口径取
 *   **暴风认定结束的持续时长**」✓ ⇒ 窗口 = **`(class + 9) × 4`** 帧 ✓：
 *     · **純霊**（= 妖精位 ✓）class **0** ⇒ **36 帧** ✓（owner「妖精按最小」✓）
 *     · **幽霊** 绿/黄/红 class **0/1/2** ⇒ **36 / 40 / 44 帧** ✓（owner「幽灵按一手做」✓ =
 *       `Sub0/1/2` 的 `anm_death_effects(0/1/2)` ✓）
 *     · **動物霊** 狼/獺/鷲 class **2** ⇒ **44 帧** ✓（owner「像幽灵的感觉 ⇒ 按 level 2」✓ = **❓自定** ✓）
 *   ⚠ **别和充能等级混** ✗：那是 `(enemy[0x3380]>>6)&7`（决定投喂块 `+0x2c` 的量 ✓），两个轴 ✓。*/
#define POFV_HITS_CHAIN_X_DEFAULT 36   /* ★ 一手值 ✓（owner：「那么窗口就回退就行」✓ —— ×2 是为绕 bug，真因已修 ✓）*/
static int32_t s_chain_storm_age;
static int32_t s_chain_window = POFV_HITS_CHAIN_X_DEFAULT;
/* ★★★ 2026-10-08（owner 判断 ✓）：「**当根本没有连击条的时候，它也认为我连击条没了**」✗ ——
 *   我的清除逻辑**没有门** ✗：从没起过连锁也会按「连锁结束」去清 hits ✓。
 *   ⇒ 加 `s_chain_seen`：**只有真的起过连锁（击破/起暴风）之后，才允许清** ✓；
 *     清一次就归 0 ⇒ 中间那些"无事发生"的帧不会再动 hits ✓。*/
static int32_t s_chain_seen;
/* class → 窗口（`(class+9)×4` ✓）*/
static int32_t pofv_hits_chain_x_for(int32_t kind, int32_t level)
{
    int32_t cls;
    if (kind == POFV_SPIRIT_KIND_GHOST_TH09)      cls = (level >= 1 && level <= 3) ? (level - 1) : 0;  /* 绿/黄/红 ✓ */
    else if (kind == POFV_SPIRIT_KIND_PURE)       cls = 0;                                            /* 妖精位 ✓ */
    else                                         cls = 2;                                            /* 動物霊 ❓自定 ✓ */
    return (cls + 9) * 4;   /* ★ 一手值 ✓（owner：「窗口就回退」✓ ⇒ 36/40/44 ✓）*/
}
/* ★ HUD「hits」（一手 §9.8）：当前值 `+0x22b4` / 锁存 `+0x22b8` / 寿命 120 / `+0x227c<1` 时 6 帧闪灰 ✓
 *   ⚠ 按 owner 结论：**只接 hit 这一条**（两作同源 ✓），**SP 不并进 Hits** ✗ ⇒ 这里不碰 SP ✓ */
static pofv_hits_t s_hits;
/* ★★★★ 2026-10-10 TH19「連撃（combo）」**真账**（一手核死；口径/地址全在 `pofv_core.h` 的
 *   「TH19『連撃（combo）』账」段 —— 那里有 `0x549780: inc [ecx+0x44]` / 两个调用点 /
 *   窗口 60 帧 / 穿 0 归零 / **无上限** 的逐条一手）。
 *   与上面那本 `s_hits` **是两本独立的账** ✓（owner：「TH09 与 TH19 独立运行」✓）：
 *     · `s_hits` 继续只服务**显示层**（一手 `+0x22B4/+0x22B8/+0x2290/+0x227C` 的复刻 ✓，本处**不动它** ✗）；
 *     · **本账**才是「击破奖励（一手 `0x50c464`）要读的那个 combo」✓ —— 上一版拿 `s_hits.cur` 当近似 ✗
 *       ⇒ owner 明确要真账 ✓。
 *   ⚠ 一手另外还有一条 `+0x22B4 := +0x22BC`（`0x549815`）的 HUD 镜像 —— 本卡**不接** ✗：
 *     TH19 那个数字元素已被 owner 2026-10-08 明确撤下 ✓，而现有 `hits`/TH09 SP 显示层另有 `s_hits` 一套
 *     （owner 已调过多次）⇒ 接上去会把两本账串起来 ✗；恢复方案写在 `TUNING.md` 的登记里 ✓。*/
static pofv_th19_combo_t s_combo_th19;
/* ★★★ 2026-10-11 owner + 一手（`th19:0x521c60` 反汇编 ✓）：★ 连击有**两个数** ——
 *   · **显示用的窗口连击** = `s_combo_th19.combo` ✓（窗口漏空归零 ✓，本卡已有一手依据 ✓）；
 *   · ★ **`mgr+0xff8ec` 的单调计数器** ✓ =「**一直累加、只有关卡重置才清零**」✓（owner 口径 ✓；
 *     一手在 `0x413639`/`0x5219e2` 两处清零 ✓）。
 *   ★ 一手规则挂在这**后者**上：★ **每满 100 ⇒ `while(≥100)` 掉 1 个 `kind 9`（加槽灵力道具）** ✓
 *     ⇒ ★ 涨 300 就一口气掉 3 个 ✓（`0x521d3c: -=100` + `0x521d43: cmp 100; jge` ✓）。*/
static int32_t s_combo_acc;
static float s_combo_last_x, s_combo_last_y;   /* 连击事件的落点（击破点 ✓，掉落用 ✓）*/
/* ★★★ MODEL-109-SPIRIT-MIRROR.md §4.5 / §4.6（owner 2026-10-05 追加）—— 这两个是**独立事件源** ✓：
 *  §4.5 **周期生成**：除 boss 外，每 **30 s**（=1800 帧）尝试在**两侧各**生成 1 只（幽灵/纯净灵，种类**各自随机** ✓）
 *        ⇒ 这是**本侧的首个生成入口** ✓（解开「本侧恒 0 只」的 bootstrap 死锁 ✓）
 *  §4.6 **对面灵的自然击杀**：每个对面灵**每 30 帧**判定一次，概率 p = 1/120（⇒ 平均 60 s 被杀一次 ✓）
 *        ⇒ **按【镜像击破】记账** ✓（= 「并非镜像的镜像击破」✓），作用 = **润滑链条、腾槽位** ✓ */
#define POFV_PERIOD_SPAWN_FRAMES 1800    /* 30 s ✓ */
#define POFV_NATKILL_EVERY 30            /* 每 30 帧判一次 ✓ */
#define POFV_NATKILL_DEN 120             /* p = 1/120 ✓（期望 1/60 s ✓，owner 推导待确认 🟡）*/
static int32_t s_period_timer;
static int   s_boss_prev;   /* ★ owner 2026-10-05：boss 表 0→1 的**边沿**（进 boss 清屏一次 ✓）*/
/* ★ HUD（owner：「**HUD 只需要本侧即可**」✓ ⇒ 只一套 ✓）：数字走 `ce_hud_number_set`（ANM 图元 ✓）、
 *   标签 `hits` 走 `ce_hud_vm_text_set`（原生 font2 字形 + 可带颜色 ⇒ 黄/灰闪 ✓）
 * ❓ 位置：一手 `y = 32.0`、`x = …(296 比例) + 16.0`（**引擎坐标系** ⇒ 本卡 UI 空间换算未核 ⇒ 先取 32/… 近似，登记 TUNING）
 * ❓ 颜色字节序：一手是 `0xffffff00`(黄)/`0xff808080`(灰)；本口参数名是 `bgra` ⇒ 先按同字节序传，实跑若反了调换 ✓ */

/* ★★ TH09 **SP**（一手 `engine/versus/th09/01-versus-loop.md` §2.1/§4.4 ✓；子智能体 `0b3af965` 清单 ✓）：
 *   一手 = 左上角大数字 ✓，满 **`0xf4236` = 999990** ⇒ 一手显示 `Bonus!` ✓（`!` 画不出 ✗ ⇒ 待贴图 ❓）。
 *   累加：`SP += (等级 × 送量)/10 + 送量`、送量 = `min(連爆数,100) × 10`、**取整到 10 的倍数** ✓。
 *   ❓ 一手的系数（`0x4a7e44`）是**常量读取** ⇒ 具体值未解出 ✗ ⇒ 这里用具名 ❓常量 `POFV_SP_RATE`（默认 1.0 ✓）。*/
#define POFV_SP_MAX   999990
#define POFV_SP_RATE  1.0f    /* ❓ 一手 `0x4a7e44` 常量值未解出 ⇒ 暂取 1.0 并登记 TUNING ✓ */
/* ★★ 2026-10-08（owner 实跑：「框的位置不对，SP 和 hits 的数字不在里面」✗ ⇒ 已订正）：
 *   一手 `front.anm` script7/8/9 的**相对几何**（逐字读出 ✓）：
 *     · 框 `script7`: `sprite(sprite7); **anchorTopLeft()**; pos(-144, 0)` ⇒ 80×32 的「Spell Point」牌，
 *       **标题在图上、下面那行空白才是数字位** ✓；
 *     · 数字 `script9..18`: `anchorTopLeft(); pos(**-139 + 8·k**, 13)` ⇒ **相对框 = (+5 + 8k, +13)** ✓；
 *     · 单位 `script8`: `anchorTopLeft(); pos(-121, 13)` ⇒ 相对框 = (+23, +13) ——
 *       ⚠ 叠图验证过：**+23 会压在数字中间** ✗ ⇒ 这一行的 +5..+53 是给**数字**的 ✓，
 *       单位改放**数字之后**（框宽 80 ✓ 5+48+16 = 69 < 80 ✓ 仍在框内 ✓）。**该 x 属 ❓自定**（已登记 TUNING ✓）。
 *   ⇒ 所以 `SP_X/SP_Y` 现在是**框的左上角**（不再是我先前那个 +28/-8 的错位 ✗）。*/
/* ★★★ 2026-10-08 第三轮（owner 给了 **TH09 实机参照图** ✓，一切以它为准 ✓）：
 *   TH09 真实版式 = 左上角一个小框，**框内**是：「Spell Point」标题（框图自带 ✓）＋
 *   下一行「**<红色命中数>Hit**」✓；**框的右侧**另有两个数字（SP 值 ✓）。
 *   而**连击条 = 屏幕最下方一个白色长方形** ✓（owner 原话：「其中连击条就是一个白色长方形」✓）
 *   —— ⚠ 我上一轮把 `sprite40/41`（红/绿分段图）当成连击条 ✗ **是错的** ✓；
 *     上一位写的 `POFV109_HUD_COMBO`（`drawRectGrad` 纯白矩形 ✓）才是对的 ✓ ⇒ 恢复用它 ✓。*/
/* ★★★ 2026-10-08（owner 口径）：「框的左边界应当和 **112 的 HUD 左边界**对齐 —— 即**屏幕左侧**」✓
 *   一手（`HUD-LAYOUT.md` §1.2）：**锚点 = 左下角内缩 = 表面 (257, 924) = 640 空间 (128.5, 462)** ✓
 *     「116 季节 / 117 动物灵 **两张卡独立推出**的同一条线：**左 257**」✓
 *   ⇒ 本卡 HUD 的 x 也取 **128.5**（**不再内缩 8** ✗ —— 先前 136 是我自己加的 ✗）。*/
/* ★★★ 2026-10-08 owner 标定（**退回并继续** ✓）：
 *   · owner：「框整体向右 **16px**、向下 **28px**」⇒ ★ 按 **1 单位 = 1 px** 直接加到基准上 ✓
 *     （基准 = HUD 左边界线 128.5 / 上边 36 ✓）⇒ **x = 144.5、y = 64** ✓。
 *   · 布局本身**整体 ×2** ✓（owner 的证据：当时量到「一个数字 12px」✓、且「下一个数字遮住上一个 6px」✗
 *     ⇒ 当时步距只有 6px ⇒ **一手 `front.anm` 那套 640 空间的值要 ×2** 才等价 ✓）。
 *     ⇒ 命中起 10、步进 12、Hit 46、SP 74、文字 DY 18 ✓（= 一手原值 ×2 ✓）。
 *   · `scale` 保持 **2.0** ✓（owner：「12px 是正确的」✓，别动 ✓）。
 *   ⚠ 先前我按 `HUD-LAYOUT.md` §1.1/§1.2 把它换成「表面 px 257 + 16」⇒ owner 实跑：「**改回到整个
 *     调整之前了**」✗ ⇒ 那份文档在这条上**与实跑不符** ✗（本卡以 owner 实跑为准 ✓，已记 TUNING ✓）。*/
#define POFV_HUD_SP_X 144.5f   /* = 128.5（HUD 左边界）+ 16（owner 标定）✓ */
#define POFV_HUD_SP_Y 64.0f    /* = 36 + 28（owner 标定）✓ */
#define POFV_HUD_SP_DX        5.0f   /* ★ 一手：框内数字/字相对框 (+5, +13) ✓（front.anm script9 ✓）*/
#define POFV_HUD_SP_DY        5.0f   /* ★ owner 2026-10-08：全部文字**上 13px** ⇒ 18 − 13 = 5 ✓ */   /* = 一手 +13 → owner「上 8px」的现值 ✓（13×2−8 ✓）*/
/* ★★★ 2026-10-08 owner 标定的**关键推论** ✓：他量到「一个数字 **12px**」✓、且「下一个数字会**遮住**上一个
 *   数字的 **6px**」✗ ⇒ 当时的**步距只有 6px** ⇒ ★ **HUD 里 1 单位 = 1 px**（不是 2 ✗）⇒
 *   **一手 `front.anm` 那套 640 空间的值要整体 ×2** 才等价 ✓（实测框 ≈160 表面 px = 160 单位也印证 ✓）。
 *   ⇒ 下列各值 = 一手原值 ×2 ✓（原值：命中起 +5 / 步进 6 / Hit +23 / SP +37 ✓）。*/
#define POFV_HUD_SP_STEP     12.0f   /* = 一手 6 × 2 ✓（步距 12 px = 字宽 ⇒ 首尾相接、不再互遮 ✓）*/   /* ★ 一手原值（`front.anm` script9..17 步进 6 ✓）*/   /* ★★★ 一手：每位 **6 px** ✓（现 dump `front.anm` script9..17：
                                      *   -139/-133/-127 与 -107/-101/-95/-89/-83/-77 ⇒ 步进 6 ✓；我原写 8 ✗）*/
/* ★★★ 2026-10-08 owner 给的**格式原文**：「Hit 得是**红色**的，格式不是 000000 而是
 *   **一个红色的 `<3 格右对齐、前导空格>` 数字 + `Hit`**，**后面紧跟着是六位 SP**」✓
 *   ⇒ 框内下行 = `⟨3 格红命中，前导留空⟩` + `Hit` + `⟨6 位 SP，紧接⟩` ✓（都在 80×32 框内 ✓）。*/
#define POFV_HUD_HIT_SLOTS    3      /* ★ 命中数**固定 3 格**（前导空格 ✓，不是 0 ✗）*/
#define POFV_HUD_SP_SLOTS     6      /* ★ SP **6 位**（紧接在 Hit 之后 ✓）*/
/* ★★★ 2026-10-08 一手**逐条**（现 dump `front.anm`，全部相对框 `pos(-144,0)` ✓）：
 *     script9/10/11  → 命中 **3 格**：+5 / +11 / +17 ✓  color **(192,128,128)**（柔红 ✓）
 *     script8        → 「Hit」标签：**+23** ✓          color (192,128,128) ✓
 *     script12..17   → **SP 6 位**：+37/+43/+49/+55/+61/+67 ✓  color **(128,128,128)**（**灰度** ✓ owner 提到 ✓）
 *   步进一律 **6** ✓；最右 +67+6 = **+73 < 80** ⇒ **全在 `sprite7` 框内** ✓（owner：「宽框就是那个 Spell Point 的框」✓）。*/
#define POFV_HUD_HIT_X     -62.0f   /* ★ owner：全部文字**左 72px** ⇒ 10 − 72 = −62 ✓ */   /* = 一手 +5 × 2 ✓ */    /* ★ 2026-10-08 owner 标定：「文字整体向左」⇒ 5 → 2 ✓（一手为 +5 ✗ 按实跑调 ✓）*/
#define POFV_HUD_HITLABEL_DX -18.0f   /* ★ 52 − 72（左 72）+ 2（Hit 单独右 2）= −18 ✓ */   /* ★ owner 2026-10-08：「Hit 偏左，向右 6px」⇒ 46 → 52 ✓ */   /* = 10 + 3×12 ✓（一手 +23 ×2 ✓）*/   /* ★ 一手原值：script8 的「Hit」在 +23 ✓ */
#define POFV_HUD_SPNUM_DX     2.0f   /* ★ 74 − 72 = 2 ✓ */   /* ★ 跟着 Hit 一起 +6 ✓（一手：SP 紧接在 Hit 之后 ✓）*/   /* = 一手 +37 × 2 ✓ */   /* ★ 一手原值：script12 的 SP 起 +37 ✓ */
#define POFV_HUD_HITLABEL_DY  4.0f   /* ★ Hit 单独**上 1px** ⇒ SP_DY 5 − 1 = 4 ✓ */
#define POFV_HUD_COL_HIT    0xFFC08080u   /* ★ 一手 `color(192,128,128)` ✓（命中 + Hit）*/
#define POFV_HUD_COL_SP     0xFF808080u   /* ★ 一手 `color(128,128,128)` ✓（SP 六位，owner：「乘了一个灰度」✓）*/
/* ★ 连击条 = 屏幕**最下方**的白色长方形 ✓（参图 ✓）*/
/* ★★★ 2026-10-08 owner（「连击槽位置不对：偏右、偏上，宽度至少宽了一倍；它应当在**框的正下方**、
 *   满长度**和框一致**」✗⇒✓）：
 *   ⚠ 关键：**框的 VM 是「中心锚」** ✗ ⇒ 它的**可见矩形** = `[SP_X−80, SP_X+80] × [SP_Y−32, SP_Y+32]`
 *     （精灵 80×32 ✓ × `scale 2.0` ✓ ⇒ 160×64 ✓，半宽/半高 = 80/32 ✓）。
 *   ⇒ 条必须按**可见矩形**对齐（先前我按 SP_X/SP_Y 直接对齐 ⇒ 恰好偏右 80、偏上 60 ✓）。*/
/* ★★★ 2026-10-08 一手（§3.44 ✓）：条 = `front.anm` sprite42（72×4 ✓，图自带纵向白→灰渐变 ✓），
 *   原版画在 `pos(-141, 30)`，而框（script7）在 `pos(-144, 0)`、80×32 ✓ ⇒ 条相对框 = **(+3, +30)** ✓，
 *   即**贴框下沿内侧** ✓。我们的框是「中心锚 × scale2」（可见 160×64 ✓）⇒ 换算：
 *     x = SP_X − 80 + 3×2 = **SP_X − 74** ✓   y = SP_Y − 32 + 30×2 = **SP_Y + 28** ✓。*/
#define POFV_HUD_BAR_X       (POFV_HUD_SP_X - 74.0f)   /* ★ owner：「再向左」⇒ −8（❓数值待给 px ✓）*/   /* = 框的**可见左边缘** ✓ */
#define POFV_HUD_BAR_Y       (POFV_HUD_SP_Y + 28.0f)   /* ★ owner：「再向上」⇒ 40 → 32（❓同上 ✓）*/
/* ⚠ `POFV_HUD_BAR_W` 已不需要（一手用 scale.x = **比例** ✓，不是像素长度 ✗）—— 保留常量不删 ✓ */
#define POFV_HUD_BAR_W       160.0f   /* = 框**正下方** ✓（半高 32 + 8 间距 ✓）*/
/* ★ 事件文本行（连爆/生成 提示 ✓）—— 沿用旧名字，锚点归到 SP 框下方 ✓（❓自定）*/
#define POFV_HUD_HITS_X      POFV_HUD_BAR_X
#define POFV_HUD_HITS_Y      (POFV_HUD_SP_Y + 40.0f)
#define POFV_HUD_C2_Y 438.0f   /* 连击条所在行（底部 ✓，参图 ✓）*/   /* ★ SP 下一行 ✓ */   /* ★ 重排：連擊槽行 y=46 ✓（原 36 ⇒ 条在 44 与 hits 撞 ✗）*/   /* ★ TH09 連擊槽（SP 正下方 ✓，一手 §2.1 ✓）*/
/* ★★★★ 2026-10-08 owner：「TH19 一手」的**蓄力槽** HUD（卡 109）—— 复刻 TH19 `aura.anm`
 *   script0 起那 12 支子脚本（一手 = `local/th19.v1.10c/_ext/aura.anm`，`thanm -l 19` 逐条）：
 *     · script1 = 底（`drawRectGrad(224,4)`，`color(0,0,0)` / `color2(0,0,64)` = **深灰/暗底槽** ✓）；
 *     · script2 = 框（`drawRectBorder(224,4)`，白）；script3/4 = 高亮层（`pos(-113,0)` + `anchor(1,1)`，
 *       每 8 帧在 `(32,96,168)` ↔ `(224,192,140)` 之间互换 ✓）；
 *     · script5..12 = **4 个「段位标」**（相邻两支为一对：奇数支 = 白芯 + `color2(段位色)`、
 *       偶数支 = 同色 `blendMode(1)` 加算外发光）⇒ 颜色只有 **4 组**：
 *       **浅蓝 (128,128,255) / 紫 (128,0,255) / 红 (255,0,0) / 黄 (255,255,0)** ✓。
 *
 * ★★★★ 2026-10-10 owner 给的**结构规格**（逐条落地；推翻此前「两根粗条 + 大色块填充」✗）：
 *   ① **蓝填充 = 灵力比例**（= 你能蓄到哪 ✓）；② **白段 = 蓄力比例**（叠在蓝条**左端起**、随蓄力长 ✓）；
 *   ③ 「Lv1」标签**不做**（那是 C3/C4 的「等级」，本卡暂无对应 ⇒ **只留口**，见句柄处的 ❓ 注释 ✓）；
 *   ④ **星星不做**（那是 TH19 的 **Bomb** 指示，与本卡无关 ✗）；
 *   ⑤ **一共 4 个标** = 「蓄力到这里是 **C1 / C2 / C3 / C4**」，颜色就用上面那 4 组段位色 ✓；
 *   ⑥ **每个角色的 4 个标位置并不完全一样** ⇒ 位置**按角色可不同**（本轮改成**按阈值分数**给 +
 *     保留逐角色接口，见 `POFV_HUD_CHG_TICK_FRAC` / `POFV_HUD_CHG_TICK_FRAC_KIND` ✓）。
 *   ⇒ 卡侧用**两支脚本**：**422**（一个 **1×1** 的 `drawRectGrad` 图元 ⇒ 卡侧 `scale` 直接写**表面 px** ✓）
 *     画底槽/蓝填充/白段，**425**（**贴图** = **4×24** 的底色菱形）画 4 枚标，**426** 画「蓄力 ≥ 标位」时的光晕 ✓
 *     （★ 2026-10-11：**两支都从 `drawPoly` 几何改成贴图**，理由 = ❓E2/❓E5 + owner 裁决「菱形上贴图」✓）
 *     ⇒ 起 **1 台底槽 + 1 台蓝填充 + 1 台白段 + 4 台菱形标**（+ 蓄力越过的那些标各 1 台光晕），
 *     每帧按比例写 `scale` / 写白顶点色（颜色在贴图里 ✓）/ 按段位色 `ce_anm_set_sprite` ✓。
 *   ★ 块尺寸取**一手值 ×2**（owner：「先按一手宽度做」✓ + `HUD-LAYOUT.md` §1.1）⇒ 满宽 224→**448**、
 *     高 4→**8** 表面 px；底槽每边再放 `POFV_HUD_CHG_BORDER` ⇒ **条**的外表 **452×12 表面 px** ✓。
 *   ★★ 2026-10-11 第二轮：认领块高 = **24**（条 12 + 刻度出头 `TICK_UP` 12）—— 为一手 **4×24** 的标让位
 *     ⇒ `POFV_HUD_BLOCK_H_TOTAL_SURF` = 24（上抬影响见 `POFV_HUD_CHG_TICK_UP` 与 `TUNING.md` §④）。 */
#define POFV_HUD_CHG_BLOCK_W    224.0f   /* ★ 一手 `aura.anm` script1..4 的矩形宽（**640×480 空间** ✓）*/
#define POFV_HUD_CHG_BLOCK_H      4.0f   /* ★ 一手：同一对指令的高（**640×480 空间** ✓）*/
/* ★★★★ 2026-10-10 owner：「**灵力条似乎也是没有 ×2？**」✓ —— 一手那几条是 **TH19 `aura.anm` 的 640×480 空间**
 *   （`aura.anm` script1..12 都不写 `resolutionMode`，一手那边由引擎整体 ×倍率 ⇒ 落到 1280×960 表面是 ×2 ✓；
 *     一手 `thanm -l 19` 逐条见 `local/th19.v1.10c/_ext/aura.txt` script1/2/3/5 ✓）；
 *   而**我们的脚本 422** 是 `layer(25) + resolutionMode(2)` = **1 单位 = 1 表面 px**（`HUD-LAYOUT.md` §4.1b ✓）
 *   ⇒ 卡侧必须把一手值整体 **×2** 才是同一表面尺寸：满宽 224→**448**、高 4→**8** ✓。*/
#define POFV_HUD_CHG_X2          2.0f   /* ★ 640 空间 → 表面 px 的倍率（`HUD-LAYOUT.md` §1.1 / §4.1b ✓）*/
#define POFV_HUD_CHG_FILL_W      (POFV_HUD_CHG_BLOCK_W * POFV_HUD_CHG_X2)   /* 448 表面 px：填充区满宽 */
/* ★★❓**自定**（owner 2026-10-10 明令「**偏离一手**」✗）：
 *   「**把 109 的蓄力槽横向拉伸成整个屏幕宽度**，但是注意**避让主动卡槽**」✓
 *   ⇒ ★ 几何**改吃申领到的槽** ✓ —— `pofv_hud_chg_claim()` 早就在申领"**整个剩余宽度**" ✓
 *     （而且它用 `ce_hud_card_set(1)` **假装有主动卡**来保守避让 ✓，见 `:808` 那段 ✓）
 *     ⇒ ★ 所以「避让卡槽」**不用另做** ✓，★ 只要把**画**的宽度接上申领结果 ✓。
 *   ★ 于是：`bx` = 槽左沿 + 边框 ✓（★ **不再跟随自机** ✗）、可用宽 = `槽宽 − 2×边框` ✓
 *     （★ 不再写死 448 ✗）。★ 一手是 224（640 空间）= 448 表面 ✓ —— ★ 本卡此处**刻意偏离** ✗。*/
#define POFV_HUD_SCREEN_W_SURF   1280.0f   /* ★ 表面宽 ✓（= 640×2 ✓）；⚠ **VM 位置用的是 640 空间** ✗ */
/* ★★ **游玩区中心**（`640×480` 空间 ✓）—— ★ owner：「**一手是游玩区居中**」✓（`x = 640/2` ✓）。
 *   ⚠ 与"表面 px"差一个 ×2 ⇒ ★ 宽度类（`*_SURF`）要 ÷2 才是这个空间的长度 ✓。*/
#define POFV_HUD_PLAY_CX          320.0f   /* ★ 640×480 ×2 ✓（owner：「摆在屏幕的正中」✓）*/
#define POFV_HUD_CHG_FILL_H      (POFV_HUD_CHG_BLOCK_H * POFV_HUD_CHG_X2)   /*   8 表面 px：填充区高（= 一手 4 ×2 ✓）*/
/* ★ 底槽 = **比填充区每边大一圈**（`POFV_HUD_CHG_BORDER` 表面 px）—— 观感 = 深灰/暗的外沿 ✓。
 *   ❓ 自定（一手 script2 是 `drawRectBorder(224,4)` **白描边**；【★ 2026-10-11 订正 ✗】此前这里写「TH18 的 anm **没有** `drawRectBorder`」是**错的** ✓ —— `local/vendor/eclmap/anmmap/v8.anmm` 里 **`612 drawRectBorder # TH16+` 有名字** ✓；★ 另注：★ 没名字的更高 opcode（614+）也能用 **`ins_<数字>`** 形式汇编 ✓（掉名字 ≠ 编不了 ✓）
 *     （opcode 612 一用就崩，见 `123_season_gauge_frame.anm.txt` 头注）⇒ 本仓标准画法是「一块比底大的
 *     实心矩形 ⇒ 露出这一圈」；颜色取一手 script1 的**暗色**，**不再**用会每 8 帧闪的高亮色 ✗
 *     —— 那个「亮色实心大矩形」正是 owner 看到「很粗的条」的成因之一 ✓。已登记 `TUNING.md`）。*/
#define POFV_HUD_CHG_BORDER      2.0f
#define POFV_HUD_CHG_BLOCK_W_SURF (POFV_HUD_CHG_FILL_W + POFV_HUD_CHG_BORDER * 2.0f)   /* 452 表面 px（认领用外表 ✓）*/
#define POFV_HUD_CHG_BLOCK_H_SURF (POFV_HUD_CHG_FILL_H + POFV_HUD_CHG_BORDER * 2.0f)   /*  12 表面 px（**条**的外表 ✓）*/
/* ★★★★★ 2026-10-10（五）owner **订正形状**：「标不完全对，一手能看出它实际上是一个**小的菱形**」✗
 *   —— 上一版按 owner 早先的截图口径画成「**细竖线**」**已作废**（`TUNING.md` §D2 那条随之作废，见本轮新增条目）。
 *   一手 = TH19 `aura.anm` **script5 / 7 / 9 / 11** 的 **`ins_617(1.0f, 4)` = 4 边形 = 菱形** ✓
 *   （`local/th19.v1.10c/_ext/aura.txt` 逐支读完，2026-10-10；**是几何、不是贴图** —— 那 8 支首行全是
 *    `sprite(-1)`，证据与「owner 说的贴图」的判定见 `TUNING.md` 的新 ❓ 条目）。
 *   ⇒ 本卡的标 = 脚本 **425**（★ 2026-10-11 起 = **4×24 贴图** `POFV109_CHG_MARK_*`；VM 的 `scale`
 *     是**矩形倍率** ⇒ 1.0 = 4×24 表面 px，见 `POFV_HUD_CHG_DIA_SCALE`）；
 *     「越过时的光晕」= 脚本 **426**（加算、**电平常驻 + `jmp` 循环**，见下面 `s_hud_chg_halo_vm`）。
 *   ★ 一手那 4 支**从建槽那一刻就常显**（`FUN_004e9740` 起完根 VM 后立刻 `FUN_004cb7a0(root, 4)`，
 *     4 推给 12 台子 VM 的 `interruptLabel(4)`）⇒ **未越过时也显示底色菱形** ✓（owner 给的两选一里选这个）。*/
#define POFV_HUD_CHG_TICKS       4
/* ★★ 底色菱形的**贴图尺寸** = 一手常显态（`interruptLabel(4)`）的**表面尺寸**：**4 × 24 表面 px**。
 *   一手推法（2026-10-11 第二轮核完，完整版见 `TUNING.md` 本轮 §①②）：
 *     · `ins_617(1.0f, 4)` + `scale(6,6)` + `scale2(2,2)`；
 *     · 轴分配 = **`scale` 的操作数 = 纵半轴**、**`scale2` 的操作数 = 横半轴**；
 *     · 落到 1280×960 表面**只有纵轴乘窗口倍率 ×2**（`scale2.x` **不乘**，引擎的轴不对称）
 *       ⇒ 宽 = `2 × scale2.x` = **4**、高 = `2 × scale.x × 2` = **24** ⇒ **4 × 24**（细长竖菱形，1:6）✓。
 *   ⚠ 上一版取 16×16（半对角 8）是**有意偏离一手比例**（E1）；本轮按 owner「一手尺寸优先」**改回一手值**
 *     ⇒ E1 结案（连带 `POFV_HUD_CHG_TICK_UP` 4 → 12，块高 16 → 24，见 `TUNING.md` 本轮 §④）。*/
#define POFV_HUD_CHG_MARK_TEX_W   4.0f   /* ★ 贴图宽（表面 px）= 一手常显态宽 ✓ */
#define POFV_HUD_CHG_MARK_TEX_H  24.0f   /* ★ 贴图高（表面 px）= 一手常显态高 ✓ */
/* ★★ 底色标的 `scale`：贴图路径下 `scale` = **矩形倍率**（屏幕宽 = 贴图宽 × scale.x、屏幕高 = 贴图高 × scale.y
 *   ⇒ `th18:0x481703`）⇒ **恒 1.0 = 4×24 表面 px**（= 一手常显态 ✓；逐 texel 对上 ⇒ 脚本 425 用最近邻 ✓）。*/
#define POFV_HUD_CHG_DIA_SCALE    1.0f
/* ★ **未达（蓄力 < 标位）= 一手 `interruptLabel(3)` 的「细针」**：`scale(6,0.2); scale2(0.2,0.2)`
 *   ⇒ 表面 **≈0.4 × 24**（宽 = `2 × 0.2` ✓、高仍是 `2 × 6 × 2` = 24 ✓）⇒ 卡侧**只把 `scale.x` 压到**
 *   `0.4 / 4 = **0.1**`（同一张底色贴图；高度不动）⇒ 观感 = 一根 24 px 高的细针
 *   （实跑里 `0.4 px` 宽会被栅格化成 ≤1 px 的竖线 —— 一手本来就是「可只画 1 px 宽」✓）。*/
#define POFV_HUD_CHG_MARK_NEEDLE_SX  (0.4f / POFV_HUD_CHG_MARK_TEX_W)   /* = 0.1 ✓ */
/* 块比条高的那 12 px —— **只给认领块高用**；标**居中在块心**，不用它定位 ✓
 *   ★★ 2026-10-11 第二轮：**4 → 12**（认领块高 16 → **24**）—— 理由 = 一手常显态是 **4×24** 的菱形，
 *   要它整个落在认领块里（24 高的标 + 12 高的条 ⇒ 块至少 24）⇒ owner「一手尺寸优先」✓（E1 结案）。
 *   ⚠ **货架影响（实算）**：块高 +8 只会改**行高**（`ce_hud_slots`/`hud_relayout`：行高 = 该行最高块、
 *     行底逐行向上叠），**没有第二行就一个块都不动** —— 左列（`CE_HUD_SIDE_LEFT`）现在只有
 *     **109（452 宽）** 与 **117（`YK_HUD_BLOCK_W` = 256 宽）** 两块：`257 + 452 + 8 + 256 = 973 ≤ 1000`
 *     （`CE_HUD_X_MAX`）⇒ **两块同在第 0 行**，行高由 117 的 **64** 决定（夹在中间的 109 = 24 或 16 都吃不到）
 *     ⇒ ★ **本卡这一改：货架 0 px 移动**（理论上的「第二行上抬 8 px」在本卡集里不会发生）。
 *     **本卡自己也不动**：`y` = 行底 = `CE_HUD_ANCHOR_Y`(924)，条顶 = `y − BLOCK_H_SURF` 与块高无关 ✓。*/
#define POFV_HUD_CHG_TICK_UP    12.0f   /* 块比条高的那 12 px（= 一手标的半高 ✓）*/
/* 菱形的中心 y（相对条左上角 `fy`）= **认领块的垂直中心**：
 *   块 = [`fy − TICK_UP`, `fy + BLOCK_H_SURF`] ⇒ 中心 = `fy + (BLOCK_H_SURF − TICK_UP)/2` = **fy + 0** ✓
 *   ⇒ 24 高的菱形正好 [fy−12, fy+12] = **块的上下沿**（0 溢出、不压邻居块 ✓；
 *     条在 `[fy, fy+12]` ⇒ 菱形跨在条的上沿、下半压在条里，看上去仍是「骑在条上」✓）。*/
#define POFV_HUD_CHG_DIA_CY     ((POFV_HUD_CHG_BLOCK_H_SURF - POFV_HUD_CHG_TICK_UP) * 0.5f)   /* = 0 ✓ */
/* ★★ 4 个标的**位置** —— 本轮改成**按阈值分数**给（不再按角色给等分占位）。
 *   一手：那 4 台依次吃**玩家记录 `S` 上的槽上限**——`S+0x24` / `S+0x28` / `S+0x2c` / `S+0x30`
 *   （`S+0x24` = 槽上限，初值 = `clamp(S+0x28)`；见 `engine/versus/th19/06-aura.md`）。
 *   ★ **阈值绝对值本仓未定位**（`plNN.sht` **未解包** ⇒ 那 4 个 `int` 的取值没 dump 到）——
 *     从 owner **截图反推** ≈ **500 / 1000 / 1700 / 2500**（÷2500）⇒ 条上分数 ≈ **0.20 / 0.40 / 0.68 / 1.00**
 *     —— ★ **❓ 自定（截图反推，不是一手表）**，已登记 `TUNING.md` 本轮 §③。
 *   ★ 这 4 个分数**同时**是「标画在哪」与「蓄力到没到」的**同一组数** ✓（一手那边也是同一个
 *     `S+0x24..0x30`：既定位、又是 `interruptLabel` 的电平判据）⇒ `pofv_hud_charge_bar()` 两处共用。
 *   ★ **保留「每角色可不同」的接口**（owner 规格 ⑥：「每个角色这 4 个标的位置并不完全一样」✓，
 *     一手按角色的那 4 组阈值同样**没有 dump 到**）：`POFV_HUD_CHG_TICK_FRAC_KIND[]` 逐角色指向
 *     一张 4 元表；四个角色现在都指向同一张默认表 ⇒ 将来拿到一手值**只改这张指针表 / 默认表**
 *     （卡侧不再有别的位置算术 ✓）。*/
static const float POFV_HUD_CHG_TICK_FRAC[POFV_HUD_CHG_TICKS] = {
    0.20f, 0.40f, 0.68f, 1.00f        /* ❓ 截图反推 = 500 / 1000 / 1700 / 2500 ÷ 2500 ✓ */
};
static const float *const POFV_HUD_CHG_TICK_FRAC_KIND[4] = {
    POFV_HUD_CHG_TICK_FRAC,           /* s_aura_kind 0 灵梦   —— ❓ 与默认同（一手按角色表未 dump）*/
    POFV_HUD_CHG_TICK_FRAC,           /* s_aura_kind 1 魔理沙 —— ❓ 同上 */
    POFV_HUD_CHG_TICK_FRAC,           /* s_aura_kind 2 咲夜   —— ❓ 同上 */
    POFV_HUD_CHG_TICK_FRAC            /* s_aura_kind 3 早苗   —— ❓ 同上 */
};
/* 取 `(kind, i)` 的标位分数 —— 出格的 `kind` 一律当 0（防御 ✓）；**摆位置**与**电平判据**共用这一个取值点。*/
#define POFV_HUD_CHG_TICK_AT(kind, i) \
    (POFV_HUD_CHG_TICK_FRAC_KIND[(unsigned)(kind) < 4u ? (unsigned)(kind) : 0u][(i)])
/* ★ 标的**颜色** = 一手 `aura.anm` script5/7/9/11 的 `color2`（**4 组段位色**，owner 规格 ⑤ ✓）：
 *   索引 0..3 ↔ 浅蓝 `(128,128,255)` / 紫 `(128,0,255)` / 红 `(255,0,0)` / 黄 `(255,255,0)`
 *   （= script5 / 7 / 9 / 11 ✓）。
 *   ★★ 2026-10-11（贴图化）：颜色现在**烘在贴图里**（4 色各一套，生成器见
 *     `assets/ability/make_pofv109_chg_mark_art.py`）⇒ 卡侧只按索引**选 sprite**，顶点色恒写白。
 *   ⚠ **不做**「1 张白贴图 + `ce_anm_set_color` 染色」：底色菱形**有白芯**，染色会把白芯一起染掉 ✗
 *     （owner 2026-10-11 点名）；光晕虽无白芯，但让两支同构（顶点色恒白）最不依赖脚本/卡侧的写色时序。*/
static const int POFV_HUD_CHG_MARK_SPR[POFV_HUD_CHG_TICKS] = {
    CE_ANM_ABILITY_SPRITE_POFV109_CHG_MARK_BLUE,      /* script5  浅蓝 ✓ */
    CE_ANM_ABILITY_SPRITE_POFV109_CHG_MARK_PURPLE,    /* script7  紫   ✓ */
    CE_ANM_ABILITY_SPRITE_POFV109_CHG_MARK_RED,       /* script9  红   ✓ */
    CE_ANM_ABILITY_SPRITE_POFV109_CHG_MARK_YELLOW     /* script11 黄   ✓ */
};
/* 光晕同理：**每色一张**（**24×48**，边缘径向淡出、供 `blendMode(1)` 加算用；脚本 426 缩放到**最大 18×36** ✓）。*/
static const int POFV_HUD_CHG_GLOW_SPR[POFV_HUD_CHG_TICKS] = {
    CE_ANM_ABILITY_SPRITE_POFV109_CHG_GLOW_BLUE,
    CE_ANM_ABILITY_SPRITE_POFV109_CHG_GLOW_PURPLE,
    CE_ANM_ABILITY_SPRITE_POFV109_CHG_GLOW_RED,
    CE_ANM_ABILITY_SPRITE_POFV109_CHG_GLOW_YELLOW
};
/* ★ 标的顶点色 = **恒白 + A=255**：颜色在贴图里（染色会染掉白芯 ✗）；A 通道留给「将来的压暗/淡出」
 *   （一手 `alpha(255)` ⇒ 常态不透明 ✓；改动只动这一个宏 ✓）。两路都写（引擎早退判据看两路 ✓）。*/
#define POFV_HUD_CHG_MARK_COL    0xFFFFFFFFu
/* ★ 底槽 = 一手 script1 的 `color(0,0,0)` / `color2(0,0,64)`（暗蓝；观感就是 owner 说的「深灰/暗」✓）。
 *   ⚠ 两路都写：脚本 422 不写 `colorMode`（⇒ 0 = 用 color_1），但引擎的早退判据是
 *     `color_1.A == 0 && color_2.A == 0`（`sdk.h` 的 `ce_anm_mirror_color` 头注 ✓）⇒ 两路同写最稳 ✓。*/
#define POFV_HUD_CHG_TRACK_COL1  0xFF000000u   /* (0, 0, 0)  黑   ✓ */
#define POFV_HUD_CHG_TRACK_COL2  0xFF000040u   /* (0, 0, 64) 暗蓝 ✓ */
/* ★ 蓝填充 = 一手 script5/6 的蓝 (128,128,255) ✓ —— **= 灵力比例**（owner 规格 ① ✓）。*/
#define POFV_HUD_CHG_FILL_COL    0xFF8080FFu
/* ★ 白段 = **蓄力比例**（owner 规格 ② ✓）—— 一手附图里左端那段白；纯白 ✓。*/
#define POFV_HUD_CHG_WHITE_COL   0xFFFFFFFFu
/* 认领给 HUD 管理器的**外表**尺寸：条 12 + 刻度出头 12 = **24** ✓。 */
#define POFV_HUD_BLOCK_H_TOTAL_SURF (POFV_HUD_CHG_BLOCK_H_SURF + POFV_HUD_CHG_TICK_UP)   /* 24 ✓ */
static int32_t s_sp;               /* 本侧 SP（0..999990 ✓）*/
/* ★★★ 2026-10-08（交接 HANDOFF-109-HUD-STATE §3 的唯一 P0）：**HUD 的 VM 必须起在「UI 列表」** ✓
 *   —— 世界列表版（`ce_anm_spawn`）是给**场上效果**的 ✗；我原先用错了 ⇒ 整组 HUD 落在**屏幕中间** ✗
 *   （owner 实跑：「框的左边线在大致屏幕的中线位置」✓）。
 *   一手：`native/sdk.h:245-254` —— `ce_anm_spawn_ui()`（`0x409670`）＝ **UI 列表**版本 ✓。
 *   ★ 两套坐标空间（见 `HUD-LAYOUT.md` §1.1）：`ce_hud_*`（数字/文本）= **640×480 字面量** ✓；
 *     **UI 列表 VM = 表面 px = 640 值 × 2** ✓ ⇒ 本助手吃 **640 值**、内部 ×2 ✓（调用点就不用手写 ×2 了）。
 *   传初值位置还能顺带免掉「起完到第一帧之间闪在 (0,0)」✗。*/
static uint32_t hud_vm_spawn(int script, float x640, float y640)
{
    float p[3];
    /* ★★★ 2026-10-08 订正（owner 实跑：「框很偏右，左边界在屏幕中间，高度在 POC 线」✗）：
     *   我先前**多乘了一次 2** ✗。一手两处都否掉「表面 px ×2」这个说法 ✓：
     *     · `sdk.c` 的 `ce_hud_number_set`（别的卡在用、能显示 ✓）把 `pos` **原样**交给
     *       `ce_anm_spawn_ui`（`pos[0]=px; pos[1]=y;` ⇒ 没有 ×2 ✓）；
     *     · `cards/animal.c` 的条：`ce_anm_set_pos(bar_fill, midx, cy + DY, ...)` **原样** ✓、
     *       `ce_anm_set_scale(bar_fill, fill_w, H - pad*2)` 也是**同一空间**的**倍数** ✓。
     *   ⇒ UI 列表的坐标 = **640×480 字面量空间** ✓（不是表面 px ✗）；×2 会让 (144,64) 变成
     *     (288,128) = **屏幕中间偏右 + POC 线** —— 正是 owner 看到的样子 ✓。*/
    p[0] = x640;
    p[1] = y640;
    p[2] = 0.0f;
    return ce_anm_spawn_ui(CE_ABILITY_ANM(), script, p);
}

static ce_hud_number_t s_hud_sp_num;
static ce_hud_number_t s_hud_c2_num;   /* ★ TH09 連擊槽显示句柄（数字 ❓自定 ✓）*/
static ce_hud_number_t s_hud_ag_num;   /* ★ TH19 攻撃ランク数字句柄 ✓ */   /* ★ 由 32 → 44：避让 110/111 信仰块（y≤30）✓ */
static ce_hud_number_t  s_hud_hits_num;
static int32_t s_hud_c2_vm, s_hud_ag_vm;
static int32_t s_hud_c2_fill_vm;   /* ★ TH09 連擊槽的**内芯**（一手原图 s41 ✓；框 = s_hud_c2_vm）*/
static int32_t s_hud_hits_vm;
static int32_t s_hud_hitsf_vm;
static int32_t s_hud_sp_frame_vm, s_hud_sp_digit_vm[6], s_hud_sp_hit_vm;
static int32_t s_hud_hit_digit_vm[3];   /* ★ 固定 **3 格**（前导留空 ✓ owner 格式 ✓）*/   /* ★ TH09 的 **红色 hits 数字**（红套 721–730 ✓ owner 口径）*/   /* TH09 SP row: frame 720 / digits 709+d / Hit 719 */   /* TH19 hits black outline (ascii_bigf, cut) */
static int ipow10(int n) { int v = 1; while (n-- > 0) v *= 10; return v; }
/* ★★★★ 2026-10-08 TH09 **自机上方「Charge 当前档/上限档」指示**（一手 script68–71 ✓）：
 *   4 台 VM，随自机移动（+ 偏移）、中心锚点 ✓：
 *     标签 `Charge` (0,−40) ✓／当前档 (−12,−28) ✓／斜杠 `/` (0,−28) ✓／上限档 (+12,−28) ✓；
 *     ★ 2026-10-09：以上是**一手 640×480 空间**的值 ⇒ 落到本卡 UI 列表时要**×2 到表面 px**
 *       （并且自机坐标也要先换成表面 px，见 `POFV_HUD_SURF_X/Y`）✓；
 *   出现：**按住蓄力键（★ 只认 C ✓）满 10 帧** ✓；松手 ⇒ `interrupt(…,1)` 收掉 ✓；
 *   动画：淡入 `alphaTime(10,0,192)` ✓；标签与当前档 **绿(128,255,128)↔白 每 10 帧** ✓；
 *         上限档与斜杠 **灰 (192,192,192) 静态** ✓。
 *   档位：一手 1 格 = 100（0..400 ⇒ 0..4）✓；本卡 `s_aura_charge` 是 **0..200** ⇒ 每格取 **50**、
 *         显示上限固定 **4** ⇒ 观感与一手同形 ✓（❓自定换算，已登记 ✓）。
 *   ★★★★ 2026-10-10 owner：「**level n/m 那个是仅限蓄力 —— 按 shift 不是蓄力**」✓
 *     ⇒ A 组（= 「当前/上限」那组）的显隐**只跟 `pofv_charge_key()`（C）** ✓，Shift 不再点亮它 ✓。*/
#define POFV_HUD_CHG_MAX_TIER 4
static int32_t s_hud_chg_label_vm, s_hud_chg_cur_vm, s_hud_chg_slash_vm, s_hud_chg_max_vm;
static int32_t s_hud_chg_hold;      /* 蓄力键（C ✓）连续按住的帧数（一手：满 10 帧才显示 ✓）*/

/* ★★★★ 2026-10-08 TH09 **自机上方「Gauge Filled / level」提示**（一手 script72–74 ✓）：
 *   3 台 VM，随自机移动、中心锚点 ✓：
 *     标签 `Gauge Filled` (0,−40) ✓／标签 `level` (−8,−28) ✓／档位数字 (+18,−28) ✓；
 *     ★ 2026-10-09：同上，一手的这三组偏移是 **640×480 空间** ⇒ 落 UI 列表时 **×2 到表面 px** ✓；
 *   动画：淡入 `alphaTime(3,0,192)` ✓；`Gauge Filled` 灰(224,224,224)↔白、`level` 与数字 灰(160,160,160)↔白，
 *         周期 **30 帧** ✓（一手 script72/73/74 的 `ins_37(计时器, 30)` ✓ —— 与 A 组的 10 帧不同 ✓）。
 *   ★★★★ 2026-10-10 owner 口径（**回到一手：档位变化那一帧触发的一次性弹出** ✓，原话：
 *     「gauge level 是你**只在挡位上涨**出现」✓）：
 *     · **触发** = **档位数值变化的那一帧**（一手 `FUN_0041bc90` 内 `0x41bd1e cmp edi,eax; je skip`
 *       ⇒ 相等就跳过、**只有变了**才调 `0x41a320` 组 3 台 VM（script 72/73/74 ✓））；
 *       ★ 判的是**档位**（`(int)上限量/100`），**不是**"原始值变了就弹" ✓；
 *     · **内容** = **灵力**（`s_reiryoku`，吸收灵力道具累积 ✓）÷ 满值 × 4 档
 *       （满值 = 本卡既有 `POFV_AURA_TARGET_CHARGE` ✓；4 = A 组同一个 `POFV_HUD_CHG_MAX_TIER` ✓）；
 *     · **生命周期 = 78 帧**（一手 script72/73/74 自带 `ins_34(3,0,0xc0)` **3 帧淡入** ⇒ **60 帧**颜色闪
 *       ⇒ `ins_34(15,0,0)` **15 帧淡出** ⇒ `ins_1()` 自删 ⇒ 3+60+15 = **78** ✓）；
 *       到期前再跨档 ⇒ **重放**（重设 78 帧 ✓）；期间**没有** hide/interrupt 路径 ✓。
 *   ⚠ 上一版「无条件常显」✗ 已废（owner 订正 ✓）；A/B 的避让（A 显示 ⇒ B 上移 48）**与显隐条件无关** ✓ 不变。*/
#define POFV_HUD_LV_POP_FRAMES 78   /* ★ 一手：3 淡入 + 60 闪 + 15 淡出 ✓ */
static int32_t s_hud_lv_gf_vm, s_hud_lv_label_vm, s_hud_lv_digit_vm;
static int32_t s_hud_lv_last_tier;  /* ★ 上升沿判据：上次的**档位数值**（不是原始值 ✓）*/
static int32_t s_hud_lv_left;       /* ★ 弹出剩余帧（>0 = 正在弹；到 0 ⇒ 收掉 3 台 VM ✓）*/

/* ★★★★ 2026-10-09 HUD 坐标空间（owner 实跑：「A/B 两组 TH09 指示 UI 位置十分靠左上、尺寸偏小
 *   ⇒ 坐标没有 ×2、大小也没有 ×2」✗）—— **UI 列表 VM 的坐标 = 表面 px**：
 *   · `HUD-LAYOUT.md` §1.1：UI 列表的 VM（`ce_anm_spawn_ui` + `ce_anm_set_pos`）用**表面 px**
 *     （= 640×480 字面量空间的 **2×**；`resolutionMode(2)` 下 1 单位 = 1 表面 px ✓）；
 *   · 自机（世界/实体坐标：游玩区 x∈±192 / y∈[0,448]）→ 表面 px 的换算**沿用本仓已能正确显示
 *     的先例** = 117 动物灵的释放动画（`cards/animal.c:3204`/`:3220` 逐字是
 *     `640.0f + pp[0] * 2.0f`、`32.0f + pp[1] * 2.0f` ✓）与它的咆哮条（`animal.c:2374`）——
 *     **不是**这里自己发明的映射 ✓：
 *       表面 x = 640（屏心 1280/2）+ 世界 x × 2；表面 y = 32（游玩区上边）+ 世界 y × 2 ✓
 *     （游玩区 384×448 世界单位 ⇔ 表面 768×896 ⇒ x∈256..1024 / y∈32..928 ✓，与 §1.2 的左下锚点
 *      「表面 (257, 924)」互证 ✓）。
 *   · 一手 A/B 组的偏移是 **TH09（640×480）字面量** ⇒ 到表面同样 **×2**：
 *     `-40→-80` / `-12→-24` / `+12→+24` / `-8→-16` / `+18→+36` / `-28→-56` ✓；
 *     精灵同理：一手 12×12 的 640 空间字形 ⇒ 表面 **24×24** ⇒ 每帧 `ce_anm_set_scale(vm, 2, 2)` ✓。*/
#define POFV_HUD_SURF_X(x)   (640.0f + (x) * 2.0f)   /* 自机世界 x → 表面 px（117 `animal.c:3204` 同式 ✓）*/
#define POFV_HUD_SURF_Y(y)   ( 32.0f + (y) * 2.0f)   /* 自机世界 y → 表面 px（117 `animal.c:3220` 同式 ✓）*/

/* ★★★★ 2026-10-10 owner：「A/B 两组**比子机偏右**，有一个**恒定的 x 差**，y 方面应当没问题」✓
 *   ⇒ A/B 两组整体**左移一个常量**（y 不动 ✓、两组相对关系不动 ✓）。
 *   ❓ **自定，待实跑校准**：一手没给"子机相对自机"的 x 差（TH09 里这两组是挂在**自机**上的，
 *     不是挂在子机上 ✗）⇒ 这里取 **−32 表面 px**（≈ 1.3 个字形宽 = 24px 再留一点 ✓），
 *     实跑按观感调这**一个数**即可（A/B 同时动 ✓）。见 `TUNING.md`。*/
#define POFV_HUD_TH09_DX      (-32.0f)

/* ★★★★ 2026-10-10 ★ **一手「自动位置冲突解决」，复刻** ✓（owner：「这两个我记得一手是有一个
 *   **自动的位置冲突解决**的，目前是两个重合」✓）—— 出处逐字：
 *   · `th09:0x4193e0`（`GuiSide::on_draw`，`local/th09.v1.50a/decomp/all.c:17776-17791`）：
 *     B 组（Gauge Filled / level，3 台）的循环里逐字有（反汇编 `th09:0x41974c`–`0x41975c`，
 *     2026-10-10 本机 objdump 实测 ✓）：
 *       ```asm
 *       41974c  mov  eax,[esi+0xa65c]
 *       419752  test eax,eax
 *       419754  je   0x419761
 *       419756  fsub DWORD PTR ds:0x48ef10    ; ← 只减 y
 *       41975c  fstp DWORD PTR [ebx+4]
 *       ```
 *     （`[ebx+4]` = B 组那台 VM 的 **pos.y** ✓；A 组那 4 台的循环**没有**这一段 ✓）
 *   · `+0xa65c` = **「A 组正在显示」标志**：A 组装函数 `th09:0x41a270` 末尾置 **1**（`all.c:18191`）、
 *     A 组收函数 `th09:0x41a2d0` 置 **0**（`all.c:18205`）⇒ 一手**只在 A 组显示时才动 B 组** ✓；
 *   · `_DAT_0048ef10` = `th09` 的 VA **`0x48ef10`**，实读原 exe = float **24.0**（`.rdata`，2026-10-10 读）
 *     ⇒ 一手把 B 组整体**上移 24（TH09 640 空间）** ✓；落到本卡表面 px = **×2 = 48** ✓。
 *   ⇒ 本卡照抄：**A 组显示的那几帧，B 组的 y 再减 48 表面 px**（x / 组内相对关系 / 其它一律不变 ✓）。
 *   ⚠ 一手是**单向**的（只修 B 组、A 组不动 ✓）—— 本卡同此 ✓。*/
#define POFV_HUD_LV_AVOID_DY  (-48.0f)   /* = 一手 24.0 × 2（表面 px）✓；数值是一手读出来的 ✓ */

static int32_t s_hud_bonus_vm;   /* ★ TH09 `Bonus!`（一手原图 th09 ascii.anm sprite107 ✓）*/
static int32_t s_hud_ag_fill_vm, s_hud_ag_dot_vm[4];   /* TH19 gauge fill + 4 threshold markers (original art) */   /* TH19 Bonus label (cut from original art) */   /* ★ TH19 hits 标签（**切自原作字体** ✓ ascii.png y=56 行 ✓）*/   /* ★ TH09 連擊槽条 / TH19 攻击槽条（卡自建 VM ⇒ 必须在 clear_card 里真删 ✓ 铁律）*/
static ce_hud_vm_text_t s_hud_hits_lbl;

/* ★★★★ 2026-10-08 卡 109 的蓄力槽 VM 句柄（**共用**脚本 422 = 一个 1×1 `drawRectGrad` 图元 ✓）：
 *   · `s_hud_chg_track_vm` = **底槽**（1 台；`scale` = **452×12 表面 px** ✓ = 条 + 四周 `BORDER`；
 *     色 = 一手 script1 的**暗色** ✓ ⇒ 未填部分就是 owner 要的「深灰/暗底槽」✓）；
 *   · `s_hud_chg_fill_vm`  = **蓝填充**（1 台；`scale.x` = **灵力比例 × 448** ✓、色 = 一手 script5/6 的蓝 ✓）；
 *   · `s_hud_chg_white_vm` = **白段**（1 台；`scale.x` = **蓄力比例 × 448** ✓、纯白 ✓；
 *     叠在蓝填充**之上**（生成序在后 ✓）、**左端起**长 ✓ —— owner 规格 ② ✓）；
 *   · `s_hud_chg_tick_vm[4]` = **C1..C4 四枚标**（脚本 425 = **4×24 贴图**；**已达** ⇒ `scale` = 矩形倍率 1.0
 *     = **4×24** ✓、**未达** ⇒ 只把 `scale.x` 压到 `POFV_HUD_CHG_MARK_NEEDLE_SX` = **0.4 宽的细针** ✓、
 *     色 = **贴图里烘好的 4 套**（`POFV_HUD_CHG_MARK_SPR`）+ 顶点色恒白 ✓、位置见
 *     `POFV_HUD_CHG_TICK_AT(kind, i)`（**按阈值分数**，逐角色接口 ✓））；
 *   · `s_hud_chg_halo_vm[4]` = **「蓄力 ≥ 标位」时的光晕**（脚本 426 = **贴图** + 加算 + `jmp` 循环 ✓；
 *     ★ owner 2026-10-11 裁决：**电平**（「肯定是 `>=` 就行」✓）⇒ 卡侧**按条件起 / 真删**、**不常驻到永远** ✓）。
 *   ⚠ 必须放**文件作用域**（`clear_card` 要能看到 ⇒ 否则删卡/换局泄漏 ✗，同一文件 `:1157` 的教训 ✓）。
 *
 * ❓ **留口 ①（owner 规格 ③）**：一手这条槽上还有 **「Lv1」文字标签**（后两枚标的下方），
 *   它接的是 **C3/C4 的「等级」** —— 本卡目前**没有**这个量（`s_aura_charge` 只到 C4 档、没有等级层）
 *   ⇒ **不实现**。将来有等级时：用**现成机制** `ce_hud_vm_text_set_font(..., CE_HUD_FONT_TH18_FONT2B)`
 *   （= 本文件 `s_hud_hits_lbl` 那套 ✓）在 `POFV_HUD_CHG_TICK_AT(kind, 2)` / `POFV_HUD_CHG_TICK_AT(kind, 3)`
 *   两枚标的下方各起一台 VM；
 *   ⚠ 现有 4 套字形集**都没有 `'L'` / `'v'` 字形**（`hud_vm_glyph*` 的表：只有数字 + `- % . + : *`）
 *   ⇒ 真做时得先补字形贴图，见 `TUNING.md` 的 ❓ 条目 ✓。
 * ❓ **留口 ②（owner 规格 ② 附注）**：**按 C 蓄力时一手还有「飘散粒子」** —— 本卡/库**没有**现成的通用
 *   粒子机制（`sdk.h` 里没有 particle API；本卡的暴风/灵/道具都是**各自专用**的 VM 池）
 *   ⇒ **不实现、不自造美术**。将来接的时候：在下面 `pofv_hud_charge_bar()` 的调用点旁边，
 *   按「本帧 `s_aura_charge` 上涨」为闸门起 1..N 台 VM（美术优先找现成的，找不到再谈）。
 *   见 `TUNING.md` 的 ❓ 条目 ✓。*/
static int32_t s_hud_chg_track_vm;
static int32_t s_hud_chg_fill_vm;
static int32_t s_hud_chg_white_vm;
static int32_t s_hud_chg_tick_vm[POFV_HUD_CHG_TICKS];
/* ★ **「蓄力 ≥ 标位」时的光晕**（脚本 426；★ owner 2026-10-11 裁决 = **电平**，不是上升沿 ✗）：
 *   每格**各自**一台；`fc >= 标位` ⇒ 起（若还没有）并**常驻**（脚本自带 `jmp` 循环一直呼吸 ✓）；
 *   `fc < 标位`（松手/归零）⇒ **真删** + 句柄清 0 ⇒ 再越时会从头弹一次 ✓。
 *   ⚠ 不需要「上一帧是否已越过」的判据（电平本来就每帧都成立）⇒ 上一轮的 `s_hud_chg_crossed[]` **已删** ✓。*/
static int32_t s_hud_chg_halo_vm[POFV_HUD_CHG_TICKS];
/* HUD 管理器（`ce_hud_slot_claim_card`）发给这块的槽：`x/y` = **块左下角**（表面 px ✓，`HUD-LAYOUT.md` §3）。 */
static ce_hud_slot_t s_hud_chg_slot;
static int s_hud_chg_slot_ok;      /* 本关是否已认领到（0 ⇒ 每帧补认领一次 ✓）*/
static int s_hud_chg_slot_logged;  /* 认领日志只打一次（防刷屏）*/

/* ★★★★ 2026-10-08 owner：「**注意这个得进那个 hud 管理器**」✓ —— 认领本卡的蓄力槽块
 *   （**只认领这一条**；本卡其它 HUD 一律不动 ✗）。`ce_hud_slot_claim_card()` 会先把关卡号推进纪元
 *   （关卡号一变 SDK 先清空整张表）⇒ 所以放进每关开场的 `on_load` 里，且**幂等**（重复调用返回同一个槽 ✓）。
 *   块的宽高/锚点约定 = `HUD-LAYOUT.md` §3：「块 = w×h + 锚点 = 左下角表面 px」✓。 */
static void pofv_hud_chg_claim(void)
{
    ce_hud_block_t blk;
    /* ★★ 2026-10-10：认领的是**外表**尺寸，不是填充区 ✗ ——
     *   一手 `aura.anm` 是 640×480 空间（底 `drawRectGrad(224,4)`）
     *   ⇒ 表面尺寸 = 填充 448×8 ＋ 每边 `POFV_HUD_CHG_BORDER`(2) ⇒ **452×12** ✓（推法见常量处注释 ✓）。*/
    /* ★★★ 2026-10-03z4（owner：「**109 的蓄力槽应当申领整个剩余宽度**」）——
     *   声明宽从**固定 452** 改成**底边剩余的全部宽度**（`ce_hud_row_remaining_w()`：
     *   `X_MAX − 货架起点`，有主动卡时起点是 124 ⇒ 900、没有时 68 ⇒ 956）。
     *   宽度是**声明常量**（认领那一刻算一次）：整条底边归它 ⇒ 别的块自动排到上面。
     *   ⚠ 条本身的**贴图**现在仍是 448 宽（一手 224×2）；owner 的规划是"最终占满" ⇒
     *     等美术/脚本跟上时**只改画法**，这里的占位不用再动。
     *   旧值留档：`POFV_HUD_CHG_BLOCK_W_SURF` = 452（= 448 + 2×2）。*/
    {   float rw = 0.0f, rw_face = 0.0f;
        ce_hud_card_refresh();          /* ★ z55：先刷"原生卡面在不在"⇒ 宽度才是**扣过卡面**的 ✓ */
        ce_hud_row_remaining_w(&rw);
        /* ★★★ 2026-10-03z56（owner：「**109 自身是主动卡，可以断言一定有主动卡**」）——
         *   ⇒ 声明宽度**一律按"卡面在场"算**（取两者更保守的那个）✓：
         *     去掉对"标志是否已刷新 / 换关瞬间状态"的依赖，防御性更强 ✓。
         *     · 卡面在场 ⇒ 底边起点 124 ⇒ 宽 **700**（124..824）；
         *     · 卡面缺席 ⇒ 起点 68 ⇒ 宽 756（68..824）—— 按 owner 的断言这条路**不该发生**，
         *       真发生了也取 700（只少声明 56px，不会压到卡面）✓。*/
        ce_hud_card_set(1);
        ce_hud_row_remaining_w(&rw_face);
        ce_hud_card_refresh();          /* 立刻恢复真实状态（别人还要读这个标志）*/
        blk.w = (rw_face > 0.0f && rw_face < rw) ? rw_face : rw;
    }
    if (blk.w < POFV_HUD_CHG_BLOCK_W_SURF) blk.w = POFV_HUD_CHG_BLOCK_W_SURF;   /* 兜底：不小于条本身 */
    /* ★ 2026-10-11 第二轮：块高 = 条 **12** + 刻度出头 `TICK_UP`(**12**) = **24** ✓
     *   （一手常显态是 4×24 的菱形 ⇒ 要整个落在块里；推法/上抬影响见该宏处注释 ✓）*/
    blk.h = POFV_HUD_BLOCK_H_TOTAL_SURF;     /*  24 = 12 + 12 ✓ */
    blk.side = CE_HUD_SIDE_LEFT;       /* 走底部货架（owner：「冲突是已知问题，就这么做」⇒ 不为避让改位置 ✓）*/
    /* ★★★ 2026-10-03k（owner：「109 的槽位是**水平几乎占满整个宽度**的，他的**优先级最高摆在最下面**」）
     *   ＋ 2026-10-03p 修正（owner 实跑：「**左下角占不满，应当往上面走**」）：
     *   · `priority = 100` ⇒ 在货架里**最先摆** ⇒ 占**底边那一行的最左边**（= 最下面 ✓）；
     *   · `own_row` **改回 0** —— 原来给 1（独占一行）⇒ 底边那行只剩它一个、**左边一大片空着**，
     *     别人被挤到第 2/3 行（owner 实跑原话就是这条）。改成 0 之后，**别的块紧挨着它往右填**，
     *     填满（到 `CE_HUD_X_MAX`）才抬行 ⇒ 「左下角占满、再往上面走」✓。
     *   ⚠ `sdk_core.h` 的 `own_row` 仍然保留：它是给**未来**真需要独行的宽块用的口子。
     * ★★★ 2026-10-03z3（**owner 的规划**：「109 蓄力槽现在是没有占满整个剩余宽度，**但是最终会占满
     *   整个剩余宽度**的」）：⇒ **不要**给它 `own_row = 1`。宽度是**声明常量**（只会随改代码变，
     *   不会局中变），等它改成占满时 `hud_relayout()` 会把它右侧那些块自动挤到上面去 ✓。*/
    s_hud_chg_slot_ok = ce_hud_slot_claim_card_ex(109, &blk, 100, 0, &s_hud_chg_slot);
    if (!s_hud_chg_slot_logged) {
        s_hud_chg_slot_logged = 1;
        ce_log("pofv: HUD 蓄力槽块 %.0fx%.0f claimed=%d at surface (%.1f, %.1f) side=%d "
               "(priority %d, own_row %d)",
               (double)blk.w, (double)blk.h, s_hud_chg_slot_ok,
               (double)s_hud_chg_slot.x, (double)s_hud_chg_slot.y, (int)blk.side, 100, 1);
    }
}

/* ★★★★ 2026-10-10 卡 109 的蓄力槽 = **一条细横条**（脚本 422 = 一个 1×1 `drawRectGrad` 图元 ✓）——
 *   owner 规格的落地：入参 = **块左上角**（表面 px ✓）+ **两个比例**（0..1 ✓）：
 *     · `ratio_rei` = **灵力比例** ⇒ **蓝填充**（= 你能蓄到哪 ✓，规格 ①）；
 *     · `ratio_chg` = **蓄力比例** ⇒ **左端白段**（叠在蓝上 ✓，规格 ②）。
 *   内部自带「**部分死 ⇒ 整组重建**（保生成序：底槽 → 蓝填充 → 白段 → 4 标 ✓）」；
 *   句柄全 0（首次 / 全被收走）⇒ 直接按同一顺序新建 ✓。 */
static int pofv_barrier_have(void);   /* ★ 罩子「有没有」= 那个标识 VM `s_barrier_vm_disc` ✓；定义在后 ✓ */
static void pofv_hud_charge_bar(float fx, float fy, float ratio_rei, float ratio_chg)
{
    const float bx = fx + POFV_HUD_CHG_BORDER;
    const float by = fy + POFV_HUD_CHG_BORDER;
    int32_t *hv[2 + 1 + POFV_HUD_CHG_TICKS];
    int i, n = 0, alive = 0, missing = 0, kind;
    float fr, fc;
    /* ★ 顺序 = 画序（后建的盖在上面 ✓）：底槽 → 蓝填充 → 白段 → 标 ✓ */
    hv[n++] = &s_hud_chg_track_vm;
    hv[n++] = &s_hud_chg_fill_vm;
    hv[n++] = &s_hud_chg_white_vm;
    for (i = 0; i < POFV_HUD_CHG_TICKS; ++i) hv[n++] = &s_hud_chg_tick_vm[i];
    for (i = 0; i < n; ++i) { if (*hv[i]) ++alive; else ++missing; }
    if (alive > 0 && missing > 0) {                    /* ★ 部分死 ⇒ 整组重建（保生成序 ✓）*/
        for (i = 0; i < n; ++i) if (*hv[i]) { ce_anm_delete((uint32_t)*hv[i]); *hv[i] = 0; }
    }
    /* ★ 位置**按阈值分数**（本轮改法 ✓），逐角色接口保留（owner 规格 ⑥ ✓）；出格的 kind 一律当 0（防御 ✓）*/
    kind = (s_aura_kind >= 0 && s_aura_kind < 4) ? s_aura_kind : 0;
    if (!s_hud_chg_track_vm)                    /* ★ 先底槽（生成序 = 画在最下 ✓）*/
        s_hud_chg_track_vm = (int32_t)hud_vm_spawn(CE_ANM_ABILITY_SCRIPT_POFV109_HUD_CHARGE, fx, fy);
    if (!s_hud_chg_fill_vm)                     /* ★ 再蓝填充 */
        s_hud_chg_fill_vm  = (int32_t)hud_vm_spawn(CE_ANM_ABILITY_SCRIPT_POFV109_HUD_CHARGE, bx, by);
    if (!s_hud_chg_white_vm)                    /* ★ 再白段（盖在蓝上 ✓）*/
        s_hud_chg_white_vm = (int32_t)hud_vm_spawn(CE_ANM_ABILITY_SCRIPT_POFV109_HUD_CHARGE, bx, by);
    for (i = 0; i < POFV_HUD_CHG_TICKS; ++i)    /* ★ 最后 4 枚菱形标（脚本 425；盖在最上 ✓）*/
        if (!s_hud_chg_tick_vm[i]) {
            s_hud_chg_tick_vm[i] = (int32_t)hud_vm_spawn(
                CE_ANM_ABILITY_SCRIPT_POFV109_CHG_MARK,
                bx + POFV_HUD_CHG_FILL_W * POFV_HUD_CHG_TICK_AT(kind, i),
                fy + POFV_HUD_CHG_DIA_CY);   /* ★ 菱形**居中在认领块的块心**（精灵默认锚 = 锚心 ✓）*/
            /* ★ 4 色 4 张贴图 ⇒ 出生时按段位色选一次 sprite（下面每帧还会再写一遍，见 ④ ✓）*/
            if (s_hud_chg_tick_vm[i])
                ce_anm_set_sprite(CE_ABILITY_ANM(), (uint32_t)s_hud_chg_tick_vm[i],
                                  POFV_HUD_CHG_MARK_SPR[i]);
        }
    /* 两个比例各自钳到 0..1 ✓（卡侧兜住负值/超值 ✓）*/
    fr = (ratio_rei > 1.0f) ? 1.0f : ((ratio_rei > 0.0f) ? ratio_rei : 0.0f);
    fc = (ratio_chg > 1.0f) ? 1.0f : ((ratio_chg > 0.0f) ? ratio_chg : 0.0f);
    /* ★★★ 2026-10-03z8（owner 实跑：「**108 和 109 以及原生目前没有进入同时黑暗**」）——
     *   本卡原来**没有压暗消费者**（审计：只进"带"、自己不淡）⇒ 装了别的 HUD 卡时，
     *   自机压到带里时**别人淡、109 不淡**。⇒ 现在读**共享带**的 alpha 并写进四支 VM 的颜色
     *   （`ce_anm_set_color` 收 `0xAARRGGBB`；把 A 换成 band alpha 即可，RGB 不动）。*/
    const uint32_t tdim = ((uint32_t)ce_hud_band_dim_alpha() & 0xffu) << 24;
    /* ① **底槽**：满块（452×12 表面 px ✓）；色 = 一手 script1 的暗色 ✓ ⇒ 未填部分 = 深灰/暗 ✓ */
    if (s_hud_chg_track_vm) {
        ce_anm_set_pos((uint32_t)s_hud_chg_track_vm, fx, fy, 0.0f);
        ce_anm_set_scale((uint32_t)s_hud_chg_track_vm,
                         POFV_HUD_CHG_BLOCK_W_SURF, POFV_HUD_CHG_BLOCK_H_SURF);
        ce_anm_set_color((uint32_t)s_hud_chg_track_vm, tdim | (POFV_HUD_CHG_TRACK_COL1 & 0xffffffu));
        ce_anm_set_color2((uint32_t)s_hud_chg_track_vm, tdim | (POFV_HUD_CHG_TRACK_COL2 & 0xffffffu));
    }
    /* ② **蓝填充 = 灵力比例**（规格 ① ✓）：左端起、长 `fr × 448` ✓ */
    if (s_hud_chg_fill_vm) {
        ce_anm_set_pos((uint32_t)s_hud_chg_fill_vm, bx, by, 0.0f);
        ce_anm_set_scale((uint32_t)s_hud_chg_fill_vm, fr * POFV_HUD_CHG_FILL_W, POFV_HUD_CHG_FILL_H);
        /* ★★ #4 **一手行为**（owner 2026-10-11 ✓）：
         *   「当**能放 C3 / C4、但没有罩子**的时候，条的**蓝色部分就会开始闪烁**，
         *     催促你该开 C3 C4 蓄罩子了」✓
         *   ⇒ ★ 判据 = **蓄力 ≥ C3 阈值** ✓（= 够得着 C3/C4 ✓）**且** ★ **罩子不在** ✓
         *     （★ 罩子的"有/无"就是**那个标识 VM** `s_barrier_vm_disc` ✓ —— ★ owner：
         *      「它只是**一个标识我有罩子保护的 VM**」✓，★ **没有碰撞箱** ✓）
         *   ⇒ ★ 闪烁 = 每 **10 帧**一轮（★ 沿用标签那套节奏 ✓），★ 亮/暗只改**顶点色 alpha** ✓。*/
        {
            const uint32_t base = tdim | (POFV_HUD_CHG_FILL_COL & 0xffffffu);
            const uint32_t dim  = (0x40u << 24) | (POFV_HUD_CHG_FILL_COL & 0xffffffu);
            const int urge = (s_aura_charge >= 1700.0f)   /* = POFV_C3_THRESH ✓（宏在后 ⇒ 用字面量 ✓）*/
                          && !pofv_barrier_have()
                          && (((s_card_frames / 10) & 1) != 0);
            ce_anm_set_color((uint32_t)s_hud_chg_fill_vm, urge ? dim : base);
            ce_anm_set_color2((uint32_t)s_hud_chg_fill_vm, urge ? dim : base);
        }
    }
    /* ③ **白段 = 蓄力比例**（规格 ② ✓）：同一左端、**叠在蓝上** ✓ */
    if (s_hud_chg_white_vm) {
        ce_anm_set_pos((uint32_t)s_hud_chg_white_vm, bx, by, 0.0f);
        ce_anm_set_scale((uint32_t)s_hud_chg_white_vm, fc * POFV_HUD_CHG_FILL_W, POFV_HUD_CHG_FILL_H);
        ce_anm_set_color((uint32_t)s_hud_chg_white_vm, tdim | (POFV_HUD_CHG_WHITE_COL & 0xffffffu));
        ce_anm_set_color2((uint32_t)s_hud_chg_white_vm, tdim | (POFV_HUD_CHG_WHITE_COL & 0xffffffu));
    }
    /* ④ **C1..C4 四枚标**（规格 ⑤⑥ ✓；形状 = 一手 `ins_617(1.0f,4)` ⇒ 脚本 425 的**贴图**）
     *    ★★ **三档**（一手 `interruptLabel` 的 4 / 3 / 2 三态；推法见 `TUNING.md` 本轮 §①②）：
     *      · **常显 / 已达**（`fc ≥ 标位`）：`scale` = 1.0 ⇒ **4 × 24**（一手 label4 ✓，细长竖菱形）；
     *      · **未达**（`fc < 标位`）：只把 `scale.x` 压到 `POFV_HUD_CHG_MARK_NEEDLE_SX` ⇒ **≈0.4 × 24**
     *        （一手 label3 的**细针** ✓，`scale.y` 仍 1.0 ⇒ 高恒 24 ✓）；
     *      · 「已达」时**底槽色菱形常显**、脉冲交给**光晕那一支**（下面 ⑤）—— 一手 label2 的脉冲数值
     *        就是脚本 426 的呼吸端点 ✓。
     *    · 色 = **贴图里烘好的 4 套**（`POFV_HUD_CHG_MARK_SPR`，按索引选 sprite ✓）——
     *      卡侧顶点色**恒白** `POFV_HUD_CHG_MARK_COL`（染色会把白芯一起染掉 ✗）；两路同写 ✓；
     *    · 位置 = **按阈值分数**表 ✓（逐角色接口保留 ✓）、**居中在块心** ✓（`DIA_CY`，块 24 高 ⇒ 中心 fy）。
     * ⑤ **「蓄力 ≥ 标位」时的光晕**（★ owner 2026-10-11 **两条裁决**）：
     *    · **触发 = 电平**：「肯定是 **`>=`** 就行，**蓄力大于等于菱形对应的值**」⇒ 不再是上一轮的
     *      「上升沿一次性」✗ ⇒ `fc >= 标位` 的**每一帧**都保证有一台脚本 **426**（**常驻**）；
     *    · **光晕也走贴图**（「E4 光晕也可以贴图吧」✓）：脚本 426 = 加算 + **一手的 `jmp` 循环**
     *      （**细针 3.6×36 ↔ 最大 18×36** 的呼吸，常驻期间一直跑 ✓）；卡侧按段位色选 `POFV_HUD_CHG_GLOW_SPR[i]`、
     *      顶点色同样恒白（加算下若写色会变成 `色²` ✗）。
     *    ⚠ 光晕**不写** `scale`（脚本自带在跑的 `scaleTime`，卡侧写会被盖掉 ✓，
     *    `native/sdk.h` 的 `ce_anm_set_scale` 头注 ✓），也不进上面那组「部分死 ⇒ 整组重建」
     *    （它的生死**单独由电平条件管**，混进去会被别的 VM 的重建连坐删掉 ✗）。
     *    ★ `fc < 标位`（松手/归零）⇒ **真删** + 句柄清 0 ⇒ 下次再越会从头弹一次 ✓。*/
    for (i = 0; i < POFV_HUD_CHG_TICKS; ++i) {
        const float ti = POFV_HUD_CHG_TICK_AT(kind, i);   /* ★ 标位分数：**位置**与**电平判据**同一个数 ✓ */
        const float tx = bx + POFV_HUD_CHG_FILL_W * ti;
        const float ty = fy + POFV_HUD_CHG_DIA_CY;
        const int reached = (fc >= ti);                   /* ★ 电平：蓄力 ≥ 标位（owner 裁决 ✓）*/
        uint32_t vm = (uint32_t)s_hud_chg_tick_vm[i];
        if (vm) {
            ce_anm_set_pos(vm, tx, ty, 0.0f);
            /* ★★ 2026-10-10 owner 实跑：「C2/C4 对应的菱形**只有在蓄力越过的时候才出现**」✗
             *   ⇒ 真因 = 未达时把 x 压成 `POFV_HUD_CHG_MARK_NEEDLE_SX`（**0.4 表面 px 的细针** ✗）
             *     ⇒ **肉眼等于没有** ✗，只有越过（切成光晕那层）才"出现" ✓。
             *   ⇒ ★ **底色菱形改成常显** ✓（x/y 恒 1.0 = **4×24** ✓）；★ "越过"只体现在**光晕**那层 ✓
             *     —— 与 owner 的规格逐条一致：「**4 个标**」常显 ✓ +
             *     「充能**越过**这个标之后会发出**菱形的光晕**」✓。
             *   ⚠ 一手的 `interruptLabel(3)`（那 0.4×24 的细针态）**本卡不再复刻** ✗ ⇒ ❓已登记 TUNING。*/
            ce_anm_set_scale(vm, POFV_HUD_CHG_DIA_SCALE, POFV_HUD_CHG_DIA_SCALE);
            ce_anm_set_color(vm, tdim | (POFV_HUD_CHG_MARK_COL & 0xffffffu));
            ce_anm_set_color2(vm, POFV_HUD_CHG_MARK_COL);
            ce_anm_set_sprite(CE_ABILITY_ANM(), vm, POFV_HUD_CHG_MARK_SPR[i]);      /* 4 色 4 张 ✓ */
        }
        if (reached) {                                       /* ★ 电平：蓄力 ≥ 标位 ⇒ 该标带光晕 ✓ */
            if (!s_hud_chg_halo_vm[i]) {                     /* ★ 还没起过 ⇒ 起一台（起过就一直留着 ✓）*/
                s_hud_chg_halo_vm[i] = (int32_t)hud_vm_spawn(
                    CE_ANM_ABILITY_SCRIPT_POFV109_CHG_GLOW, tx, ty);
            }
            if (s_hud_chg_halo_vm[i]) {                      /* 位置/选图/白顶点色：每帧写（脚本的 `sprite()` 只执行一次，
                                                              * 逐帧写才与段位色绑定无关 ✓；动画全在脚本里 ✓）*/
                ce_anm_set_pos((uint32_t)s_hud_chg_halo_vm[i], tx, ty, 0.0f);
                ce_anm_set_color((uint32_t)s_hud_chg_halo_vm[i], POFV_HUD_CHG_MARK_COL);
                ce_anm_set_color2((uint32_t)s_hud_chg_halo_vm[i], POFV_HUD_CHG_MARK_COL);
                ce_anm_set_sprite(CE_ABILITY_ANM(), (uint32_t)s_hud_chg_halo_vm[i], POFV_HUD_CHG_GLOW_SPR[i]);
            }
        } else if (s_hud_chg_halo_vm[i]) {                   /* ★ 掉回标位以下 ⇒ 真删（句柄清 0 ⇒ 下次再起 ✓）*/
            ce_anm_delete((uint32_t)s_hud_chg_halo_vm[i]);
            s_hud_chg_halo_vm[i] = 0;
        }
    }
}

/* ★★★ 2026-10-05 owner：**调试 UI** —— ① 镜像事件列表（镜像暴风**延迟**显示；镜像击破/固灵**停 3 s**再消失）
 *   ② 镜像幽灵列表（**只写「有什么」**，不涉及位置 ✓）。文字一律走**切贴图**（`ce_hud_vm_text_set` ✓，owner 规矩 ✓）*/
#define POFV_UI_EV_MAX 5
#define POFV_UI_EV_TTL 180        /* 3 s @60fps ✓（owner：镜像击破/固灵停 3 s）*/
#define POFV_UI_STORM_DELAY 60    /* ❓ 镜像暴风的「延迟」= 1 s（owner 未给数值）*/
static struct { int32_t what; int32_t ttl; } s_ui_ev[POFV_UI_EV_MAX];
static ce_hud_vm_text_t s_ui_ev_vm[POFV_UI_EV_MAX];
static ce_hud_vm_text_t s_ui_gh_vm;
/* 事件种类（UI 文本用）*/
enum { POFV_UI_EV_STORM = 0, POFV_UI_EV_KILL = 1, POFV_UI_EV_SEAL = 2, POFV_UI_EV_SPAWN = 3 };
/* ⚠ `POFV_UI_EV_SPAWN`（=3）是**死枚举**：全文件**从未设置/使用** ✗（核实 2026-10-05）；
 *   保留占位以免后续枚举值移位（`POFV_UI_EV_MAX` 的数组大小不受影响 ✓）。*/

static float s_acc_last_x, s_acc_last_y;   /* ❓ 待生成的 EX 数（触发走 pofv_ex_ready 的门限，计数源暂用純霊击破数）*/                  /* 超限欠下的「100 分」笔数（延后到 tick 发）*/

/* ★★★ 灵的「送到对面」= **暂存**（一手：`modkit/MECHANICS-109.md` §6.1「灵的『送到对面』= 暂存」）：
 *   纯灵被击破 ⇒ **进入暂存一段时间** ⇒ 送回来时**变成動物霊**。
 *   ⚠ 同一份文件写明「**已删除**：用一个不可见的**虚拟对手位**占位 —— owner 明确说不是这个意思」✗
 *     ⇒ 这里**没有对手位**，只有「等一会儿再在本侧长出来」。 */
#define POFV_SPIRIT_STASH_MAX     4
#define POFV_SPIRIT_STASH_FRAMES 180     /* ❓ 暂存时长（一手只说「一段时间」；与 owner 提的「180 帧/只」节拍同值，待定音）*/
static float   s_stash[POFV_SPIRIT_STASH_MAX][2];
static int32_t s_stash_kind[POFV_SPIRIT_STASH_MAX];
static int32_t s_stash_ttl[POFV_SPIRIT_STASH_MAX];
#define POFV_STORM_VM_PARTS          1    /* ★ 一手：TH19 的圈是**一台 VM**（`drawPoly(1.0,64)` 两段顶点色）*/
static int32_t           s_storm_vm[POFV_STORM_MAX][POFV_STORM_VM_PARTS];
static int32_t           s_cap_logged;      /* 「池满丢弃」只记一行（防刷屏）*/

/* ============================ 本卡的「灵」账 ============================
 * owner 2026-10-05：「**只有本卡召唤的才是灵**」（TH17 的動物霊与 TH19 的灵「设定上是一个东西、
 *   游戏上完全不是一回事」）⇒ 灵档（半径 80 / 40 帧 / 伤害 10）**只对这张卡自己召唤的灵**用。
 * 做法：召唤灵时（`TODO(灵)` 的本体落地处）调一次 `pofv_note_spirit(敌人 id)`，死亡回调里查这张表。
 * 一手依据：TH19 的爆风圈半径/寿命来自**死亡敌人的 `ins_1028`/`ins_1031`**，而灵档（80/40）与杂鱼档
 *   （30/50）不同 ⇒ 必须知道「死的是不是我们的灵」；而 TH18 侧没有可用的敌人种类标记
 *   （`ce_enemy_death_t` 只有 id/kind/flags/x/y/enemy/…）⇒ 用**自己的账**最干净，也不碰别人的卡。
 * ⚠ 按 enemy id（`ce_enemy_death_t.id` = `enemy+0x6830`，单调递增）记，**不存敌人指针**（同帧 sweep 会删）。
 * ⚠ 无淘汰机制：满 32 格就覆盖最旧一格（灵很少，够用；已登记 TUNING）。 */
#define POFV_SPIRIT_MAX 32
static uint32_t s_spirit_id[POFV_SPIRIT_MAX];
static uint8_t  s_spirit_from_incoming[POFV_SPIRIT_MAX];   /* ★ Q4 账侧来源标志（1 = 经 incoming 落下）*/
static int32_t  s_spirit_kind[POFV_SPIRIT_MAX];   /* ★ W3：0 = 純霊、1..3 = 動物霊（狼/獭/鷲）*/
/* ★ 2026-10-05：灵一旦生成，就给它配一台**我方 VM** 画「附属圆」（一手 `enemy.anm` 271，
 *   逐字见 `assets/ability/scripts/397_pofv109_spirit_ring.anm.txt`）——真敌机本体是惰性壳，
 *   看不见；这一层先让它有东西。索引与灵账同一格。 */
static int32_t  s_spirit_vm[POFV_SPIRIT_MAX];
/* ★ 灵**本体**的 VM（一手 §2.2：本体与附属圆是两台；本体 layer(9)、圆 layer(8)）。 */
static int32_t  s_spirit_body[POFV_SPIRIT_MAX];
/* ★ 入场运动用：每只灵的**出生帧计数**与**獺的 L/R 选择**（速度由 `entry_vel_at` 按 age 算，不存速度）。 */
static int32_t  s_spirit_age[POFV_SPIRIT_MAX];
static int32_t  s_spirit_lr[POFV_SPIRIT_MAX];
/* ★ TH09 幽灵的 level（1 绿 / 2 黄 / 3 红）—— 一手 §2：击破 level 1/2 ⇒ 新灵 `+0xa6 = min(level+1,3)`。 */
static int32_t  s_spirit_level[POFV_SPIRIT_MAX];
/* ★★★ 2026-10-08 owner：「冲着人飞的幽灵，也得**转到那个方向**」✓ ⇒ 记下出生角，本体按它转 ✓ */
static float    s_spirit_entry_ang[POFV_SPIRIT_MAX];
/* ★ W4 固化（沈静化）：一手 = 敌人标志 600000 被固灵结界立起 ⇒ ①停住 + **缓慢上浮**
 *   （`GhostDisableMove` = `ins_404(-1.5707964f, 0.02f)`）②`lifeSet(50)` 耐久骤降 ③换沈静化外观（图集 106）。
 *   ❓ 触发临时口：灵进入**自机周围**（$R=48$ 的近似「吸灵领域」）即沈静化 —— 等领域本体到位再换真触发器。
 *   ❓ 耐久那半**未落**（要先按 ExpHP 查准敌人 life 字段，不盲写）。 */
static int32_t  s_spirit_dis[POFV_SPIRIT_MAX];
/* ★ W4 自然成佛（一手：沈静化「满一定帧数**自然成佛**（**不算击破**）」；
 *   镜像表又说成仏 ⇒ **变動物霊送出去**（不是消失））⇒ 做法：
 *   ① 清灵账条目（死亡回调 take() 返回 −1 ⇒ 不产暴风/不返还 = 不算击破）
 *   ② 入暂存（一会儿回来变動物霊）③ 让敌人消失（写 life = 0）。帧数 ❓。 */
static int32_t  s_spirit_dis_age[POFV_SPIRIT_MAX];
/* ★★★ W7 EX 的载体（在卡内能做的半边）：EX 也是「从版顶长出的一只真敌机」，但它
 *   ① **50% 透明度起步**（一手 ⑤）⇒ 卡侧用 `ce_anm_set_color(body, 0xFFFFFF80)` 写 α=128 ✓
 *   ② **免疫己方系统**（一手 ④）⇒ 本卡的吸灵领域/沈静化**跳过**它（`s_spirit_ex[]`）✓
 *   ❓ 未做：③ 己方伤害（要它真的打自机 ⇒ 得让它发射，超出本轮）、① 180° 旋转（等外观定）、⑥ 独立量纲（纯逻辑已备）。 */
static int32_t  s_spirit_ex[POFV_SPIRIT_MAX];
#define POFV_EX_ALPHA_RGBA 0xFFFFFF80u   /* 一手 ⑤：50% 透明起步（α = 0x80）*/
/* ★ 2026-10-06 一手定死（`engine/versus/th09/02-ghost-and-seal.md` §3.7 ✓，写点 `0x411f1a`）：
 *   `manager+0x2ac44c = (*(*(manager+0x320)+0x20) == 5) ? 160 : 200` ✓
 *   ⇒ **只有 TH19 角色号 5 用 160，其余一律 200** ✓；本卡四自机对应 TH19 的 **0/1/2**（灵梦/魔理沙/早苗 ✓）
 *     ⇒ **全用 200** ✓（原写 160 ✗ 已改 ✓）。*/
#define POFV_DISABLE_JOUMON_FRAMES 500   /* ★ 2026-10-06 与对侧同口径（一手总时长 100+200+200 = 500 ✓，原 200 ✗）*/
#define POFV_SPIRIT_AURA_R2  2304.0f   /* 48²（旧 ❓ 半径；只作兜底）*/
/* ★ 2026-10-06 照一手重算（owner：「**照一手重做可视和判定**」✓）：
 *   一手（`06-aura.md` §7.2/§9.3）取一套 = **Reimu**：`K = 64.0` ✓、形状 = 圆 ✓（1 台 VM ✓）、
 *   半径 `= clamp(S+0x8c, 0, 200) × K / 100` ✓。本卡**无蓄力槽** ⇒ 取等效蓄力值 `S+0x8c = 125`
 *   （一手自己举的例：`@AUN2` 给 `S+0x8c = 125` ✓）⇒ **R = 125 × 64 / 100 = 80.0f** ✓。
 *   ❓ 待 owner：是否改按 TH18 当前角色映射 TH19 各套（Reimu 64 / Marisa 14 / Sanae 128 … ✓）。*/
#define POFV_AURA_R          80.0f
/* ★ 魔理沙那套（一手 §7.2：`K = 14.0` ✓、形状 = **竖向带** ✓）⇒ 同样取 `S+0x8c = 125`：
 *   `R = 125 × 14 / 100 = 17.5f` ✓ */
#define POFV_AURA_R_MARISA   17.5f
/* ★★★★ 2026-10-08 owner 定案（蓄力键语义 ✓）：
 *   · **蓄力键 = C** ✓；
 *   ⚠ **原生语义（owner 更正 ✓）**：**C = 触发主动卡效果** ✓、**D = 切换主动卡** ✓。
 *     ⇒ 本卡能安心用 C 的**真正原因**：109 **自己就是主动卡**，而它的 `on_activate` 返回
 *       **`CE_ACTIVATE_REFUSED`** ✓ ⇒ **原生触发被我们主动拒掉** ✓ ⇒ C 在本卡在场时是空的 ✓。
 *     ⚠ 推论（必须保持 ✓）：**不许**给 109 实现真正的 `on_activate` 效果 —— 否则 C 会**同时**
 *       充能 + 触发 ✗。（日后若要做 C 触发的效果 ⇒ 先解决这个冲突 ⇒ 已登记 TUNING ✓。）
 *   · **固灵结界张开：按 C = 按 Shift** ✓ ⇒ 两者**等价** ✓；
 *   · ★ **两个 key 可以接续** ✓：按住 C 再按 Shift（或反之）⇒ **续同一个蓄力** ✓
 *     ⇒ 判据 = **`C_held ∨ Shift_held`** ✓，**只有两个都松开**才算松开 ✓；
 *   · ★ 蓄力中**切卡**（原生 **D** ✓）⇒ **视为松开 C**（蓄力立即停 ✓）；新卡的 C 行为**不管** ✓
 *     （本卡 `on_delete` 已把 `s_aura_charge`/半径清零 ✓ ⇒ 已是该语义 ✓）。
 *   ⚠ 键值用 SDK 现成的 `CE_VK_C`（`sdk.h:827` ✓）/`CE_VK_SHIFT`（`:831` ✓），**不自己造** ✓。*/
/* ★★★★ 2026-10-10 **拆成两把键**（owner 口径：「**蓄力只认 C**」＋「**Shift 只开结界**」✓）：
 *   · **`pofv_charge_key()` = 蓄力键 = `CE_VK_C` only** —— 用途**只有**「蓄力」这一类：
 *       `s_aura_charge` 的赋值（`pofv_aura_tick`）＋ **A 组**（`Charge 当前/上限`，一手 script68–71）的显隐 ✓；
 *   · **`pofv_aura_key()` = 结界键 = `CE_VK_C ∨ CE_VK_SHIFT`** —— 用途是「结界」这一类：
 *       结界本体的显示/保持、罩到灵的判定、碰敌弹的扫描、敌弹染色 ✓（owner 早前「按 C = 按 Shift」✓）。
 *   ★ 每个调用点都**逐个核过它属哪一类**（不靠猜 ✓）：
 *       · 灵阵循环里「结界**罩到灵** ⇒ 沈静化」= 结界 ⇒ **aura_key**；
 *       · `pofv_aura_tick`（本函数内）的**蓄力赋值** = 蓄力 ⇒ **charge_key**；
 *       · **A 组 HUD**（`Charge 当前/上限`）的显隐 = 蓄力 ⇒ **charge_key**；
 *       · 结界**碰敌弹扫描** = 结界 ⇒ **aura_key**。
 *   ⚠ B 组（`Gauge Filled / level`）**两把键都不跟**（无条件常显、内容跟灵力）—— 见其处注释 ✓。*/
static int pofv_charge_key(void)
{
    return ce_key_down(CE_VK_C) ? 1 : 0;      /* ★ 蓄力：**只认 C**（Shift 不算蓄力 ✓）*/
}

static int pofv_aura_key(void)
{
    /* ⚠⚠ 2026-10-08 **血的教训**保留：先前这里被误写成自调 ⇒ **无限递归** ⇒ 实跑**卡死（进不了画面）** ✗。
     *   ⇒ **批量替换后必须复查替换点自身** ✓（本函数体内只调 SDK 的 `ce_key_down` ✓、不调本文件的函数 ✓）。*/
    return (ce_key_down(CE_VK_C) || ce_key_down(CE_VK_SHIFT)) ? 1 : 0;   /* ★ 结界：C ∨ Shift ✓ */
}

/* ★★★ 2026-10-06 owner：「**shift 蓄力完全从属于固灵系统**」✓、且「结界大小 = 蓄力值」✓
 *   ⇒ 卡侧自建 Shift 蓄力 `s_aura_charge`（0..200 ✓，一手 `clamp(S+0x8c,0,200)` ✓）：
 *   · **增长**：一手 §7.2 的动画器参数 `+0x94`（量）/`+0x98`（帧数）⇒ 本卡取「**每 `+0x98` 帧增长 `+0x94`**」
 *     （灵梦/魔理沙都是 `20/4` ⇒ **+5/帧** ✓）⇒ 满蓄力约 **40 帧** ✓；❓ 参数语义一手未坐实（owner truanm 可定死 ✓）；
 *   · **收束**：一手脚本 `interruptLabel(1) ⇒ scaleTime(20, 4, 0.0f, 0.0f) ⇒ +20: delete()` ✓
 *     ⇒ 松开后 **20 帧内收到 0** 再删 VM ✓（SDK 无 interrupt 口 ✗ ⇒ 由卡每帧写缩放复现，与一手「槽 3 写缩放」同形 ✓）；
 *   · **半径**：`R = clamp(蓄力,0,200) × K / 100` ✓（K：灵梦 64 ✓ / 魔理沙 14 ✓）。*/
#define POFV_AURA_K_REIMU      64.0f   /* 一手 §7.2 ✓ */
#define POFV_AURA_K_MARISA     14.0f   /* 一手 §7.2 ✓ */
#define POFV_AURA_K_SANAE      128.0f   /* 一手 `06-aura.md` §7.2（Sanae）✓ ⇒ 满格 256 ✓ */
#define POFV_AURA_K_SAKUYA   64.0f   /* ★ owner：「半径感觉不够大」⇒ K 32 → **64**（R=64 ⇒ 直径 128 ✓）*/
#define POFV_AURA_CHARGE_MAX   200.0f
#define POFV_AURA_TARGET_CHARGE 100.0f   /* ★ owner 实跑定：满值取 100 ⇒ 灵梦 64（= 原 128 的一半 ✓）、早苗 128 ✓ */
/* ★★ 2026-10-10 owner 选定「逐帧涨」（= 一手语义 ✓）：一手蓄力器 `AuraCtrlInf+0x94`(int)/`+0x98`(float)
 *   是**逐帧累加器** ✓，涨率 = `clamp(S+0x88, 0, 200) × 10 / 100`（`0x4e7cf2`–`0x4e7d29` ✓）。
 *   本卡无 `S+0x88` 对应字段 ⇒ 取一手默认 100 ⇒ **10/帧**（0→100 约 10 帧 ✓）。
 *   ⚠ 2026-10-06 那句「删掉自造的蓄力累加」针对的是**我编的速率/形状** ✗，不是"累加"这件事 ✓。*/
/* ★★ 2026-10-10 owner 实跑：「**蓄力速度过快**」✗ —— 真因 = **量纲没换算** ✗：
 *   一手涨率 = `clamp(S+0x88, 0, 200) × 10 / 100` ⇒ 取默认 100 ⇒ **10/帧** ✓，
 *   但那是**灵力量纲（0..2500）**的 10 ✓，而本卡 `s_aura_charge` 是 **0..100** ✗
 *   ⇒ 换算 `10 × 100/2500` = **0.4/帧** ✓。
 *   ★ 对表（一手 ↔ 本卡）：满蓄 2500/10 = **250 帧 ≈ 4.2 s** ↔ 100/0.4 = **250 帧** ✓；
 *     C1（500）50 帧 ≈ 0.83 s ↔ cap 20（灵力 500 时）/0.4 = **50 帧** ✓ —— 逐档一致 ✓。*/
/* ★★ 2026-10-10 owner 放行：「**卡里的蓄力可以改**，结界就别动就行」✓ ⇒ 蓄力改用**一手量纲 0..2500** ✓
 *   （★ 与灵力**同一个量纲** ✓ —— 这正是"灵力 = 蓄力上限"的前提 ✓）。
 * ★★ 一手公式（§17 ✓，`all.c:186391-186404` **逐字**）：
 *     `charge += clamp(S+0x88, 0, 200) × 10 / 100`（每帧 ✓）
 *   ★ 且 `S+0x88` **就是当前灵力** ✓（两个写入点形状一致：★ 把 clamp 到 `[0,2500]` 的灵力
 *     同时写进 `+0x24` 与 `+0x88` ✓ ⇒ `all.c:189966` / `190135` ✓）。
 *   ⇒ ★★ **蓄力速度由灵力决定** ✓：★ 灵力 ≥ 200 ⇒ **`20/帧`** ⇒ 满蓄 2500 ⇒ **125 帧 ≈ 2.1 s** ✓
 *     （★ 灵力 < 200 时按 `灵力 × 10/100` 变慢 ✓）。★ 旧值 `0.4/帧` 是**量纲错**（= 一手 ÷25 的 10 ✓）✗。
 * ⚠ ★ **结界一个字不动** ✗：`POFV_AURA_TARGET_CHARGE`（100 ✓）**继续**是"结界动画量"的满值 ✓
 *   （`:4732` 那行保持原样 ✓），★ 蓄力另用 `POFV_REI_MAX`（2500 ✓）当满刻度 ✓。*/
/* ★★ 2026-10-11 owner：「**蓄力是有蓄力音效的**」✓ ⇒ 接一手 SE ✓（卡侧 `ce_play_sound(id,x)` ✓ `sdk.h:49`）。
 * ★ 一手实证：★ 蓄力段（`all.c:186330` 一带 ✓）在 **`FUN_004bb8d0`** 上起 SE ✓（打在子对象 `+200` 的
 *   `+0x54/+0x56`，按音量/位置 ✓）；同族 `FUN_00417f10`/`FUN_00418190`/`FUN_00418e60`/`FUN_00419000`
 *   里是「★ 先 **`0x31`** 再 **`0x2c`**」的**绝对 id** 调用 ✓ ⇒ ★ 取 **`0x2c` = 蓄力/维持音** ✓、
 *   ★ **`0x31` = 升档音** ✓（对上本仓已记的"一手**升档另有音**" ✓）。
 * ❓**待你听一遍确认**（放错就改这两行数 ✓）—— ★ 这是一手里**唯一没钉死**的地方 ✗：
 *   ★ 一手那条持续蓄力音挂在 SE **状态对象** `+0x54` 上 ✗，而卡侧只有**一次性**接口 ✓
 *   ⇒ ★ 本卡做法 = **按住起一次 `0x2c`** ✓ + **每升一档补一次 `0x31`** ✓。*/
/* ★★★ 2026-10-11 owner **给出文件**：「蓄力音效是这个：`…/th19/data/se_ch00.wav`」✓✓
 *   ⇒ ★ TH18 **自己也有同名件** ✓（★ `se_ch00` 串在 `th18.exe` VA `0x4b76e0` ✓，★ 属于那张
 *     **`se_*.wav` 名单**（自 `0x4b7504` 起、共 **71** 项 ✓））
 *   ⇒ ★★ **名单的序号就是 id** ✓（★ 我核过：★ `se_ch00.wav` = 序号 **30 = `0x1e`** ✓✓）。
 *   ★★ 而且这一族正好是 **4 档蓄力音** ✓：★ `se_ch00`=`0x1e` ✓ / `se_ch01`=`0x1f` ✓ /
 *     `se_ch02`=`0x2a` ✓ / `se_ch03`=`0x2b` ✓ ⇒ ★ 升档正好按档位递进 ✓✓（★ 一手手感的来处 ✓）。
 *   ⚠ 我先前那两个 `0x2c`/`0x31` ✗ ★ 是**从 TH19 的反编译猜的** ✓，★ 而 TH19 的 id 与 TH18 **不同** ✗
 *     （★ 连 `animal.c` 里的 id 也标着是 **TH17** 的 ✓，★ 三个游戏各一张表 ✓）⇒ ★ 已作废 ✓。*/
#define POFV_SE_CHARGE  0x1eu    /* ★ = `se_ch00.wav` ✓（owner 给的件 ✓，序号 30 ✓）*/
/* ⚠ 曾经的 `POFV_SE_TIERUP`（`se_ch01` = `0x1f`）**已删** —— owner 订正：`se_ch0N` 这一族
 *   的**使用处与 C1–C4 无关**，不能按档位铺 ⇒ 升档音音源未定，先不放（登记 TUNING 待查）。*/
#define POFV_CHG_RATE_SRC_MAX  200.0f   /* ★ 一手 ✓ `clamp(S+0x88, 0, 200)` 的上界 */
/* ★★ 2026-10-10 **一手实测定案**（owner 转来的核实 ✓，`FUN_004e79e0` 蓄力支 `0x4e7cec`–`0x4e7d29`）：
 *     `delta = 2 × ((v × 10) / 100) = v × 0.2`（每帧 ✓），`v = clamp(obj[+0x88], 0, 200)` ✓
 *   —— ★ **注意那个 ×2** ✗（我先前漏了它 ✓，所以算成 `v×0.1` ✗）。
 *   ★ `v` 初期 **100** ⇒ ★★ **20/帧** ✓ ⇒ ★ 满 2500 需 **125 帧 ≈ 2.08 s** ✓
 *     （★ 若某角色 `v = 200` ⇒ 40/帧 ⇒ 63 帧 ✓）。
 *   ⚠ 我上一版按 owner 的"太快"把它砍成 **10** ✗ —— ★ 那是**手感猜测** ✓，★ 现已被代码推翻 ✓
 *     ⇒ ★ **恢复 20** ✓。（"太快"的真因见 TUNING：★ 一手还有「**阶段消费**」✓ ——
 *     发一档要扣 `阈值 − 上限` ✓，★ 本卡没实现 ⇒ 才会一直顶在 `M` 上不动 ✓。）*/
/* ⚠ 2026-10-11 owner 第二次实跑：「**蓄力速度还是快**」✗ ⇒ ★ 再降一半以上 ⇒ **5/帧** ✓
 *   （= 一手 `20/帧` 的 **1/4** ✓；★ 满 `M=500` 约 **100 帧 ≈ 1.7 s** ✓、★ 满 2500 约 **500 帧 ≈ 8.3 s** ✓）。
 *   ★ 一手那 20 是"源满格"的值 ✗，本卡开局 `M` 只有 500 ✓ ⇒ 手感上确实该更慢 ✓（★ 仍是 ❓自定旋钮 ✓）。*/
/* ★★ 2026-10-11 owner **实测**：★ 兽王园一手「**从 0 到满蓄力 2500 的时间 = 4.167 s**」✓✓
 *   ⇒ ★ `4.167 × 60 = 250 帧` ✓ ⇒ ★★ **2500 ÷ 250 = `10/帧`** ✓✓
 *   （★ owner 补：「由于代码设置的**人为性**，这肯定对应一些**整数**」✓ —— ★ 正是 **250 帧** ✓！）
 * ⇒ ★ 于是**涨率 = 10/帧** ✓ —— ★ 这不是手感猜测 ✗，★ 是你的**实测** ✓。
 * ⚠ 我此前的两次都错 ✗：★ 先 20（★ 从反编译的 `2 ×` 误读来 ✗）、★ 后被手感误导砍成 5 ✗
 *   ⇒ ★ 现在以**实测**为准 ✓（★ 与 `clamp(源,0,200) × 10/100` 在**源 = 100** 时**完全一致** ✓）。*/
#define POFV_CHG_RATE_AT_SRC_MAX 10.0f
/* ★★★★ 2026-10-10 owner（**上限量 = 灵力**，一手 `S+0x24` 的等价物 ✓）：一手 `S+0x24` 的**初值**
 *   = `clamp(S+0x28,0,2500)` = **C1 阈值**（不是 0 ✗）；拾道具 `+= 15 × clamp(S+0x98,0,100)/100`；
 *   当前蓄力 `AuraCtrlInf+0x94/+0x98` 的**上限**就是它 ✓。
 *   ❓ **C1 的具体数值一手未解出**（在 `plXX.sht` 的角色参数表里，未 dump 到）⇒ **自定 50.0f**
 *     （= 本卡满值 `POFV_AURA_TARGET_CHARGE` 的**一半** ⇒ 开局有基础半径、又留得下成长空间 ✓）。
 *   已登记 `TUNING.md`。*/
/* ★★★★ 2026-10-10 owner：初值 `C1` = 一手 `clamp([sht+0x2c]×10, 0, 2500)`（逐角色，`plXX.sht` 未解包 ❓）。
 *   ★ 判据 = owner 的「**目前的手感应当是一手**」：开局必须能蓄满 ⇒ 取 **满值 2500** ✓
 *     （= 若 `[sht]` ≈ 250，则 `×10` 正好 2500 ✓；改这一个数就能调"开局能蓄到多少" ✓）。*/
/* ★★ 2026-10-10 owner 实跑：「灵力**一直固定在最大值**」✗ —— 真因 = 初值取了**满值** ✗。
 *   一手：`pl00.sht` 的 `+0x2c` ×10 = **500** ✓（= 一手 C1 阈值 ✓，也与 owner 早先的口径
 *   「**灵力的初值 = C1**」一致 ✓）⇒ 改成 500 ✓。
 *   ★ 连带修掉 ④：`clear_card` 用 `灵力/2500` 起步 ⇒ 2500 时 `s_hud_lv_last_tier` 直接是
 *     **最高档** ⇒ `tier != last` **永不成立** ⇒ "level n/m" 文字**永不弹** ✗；改 500 ⇒ 起步档 0 ✓。*/
#define POFV_REI_C1   500.0f
#define POFV_REI_MAX  2500.0f

/* ★ 一手实测 ✓：`(道具值 0..100 × 15)/100` ⇒ ★ 满档一颗 = **+15** ✓（`FUN_00520d40` ✓）。*/
#define POFV_REI_GAIN_PER_ITEM     15.0f  /* ★ 一手量纲 ✓（`0x9c4` = 2500；此前拿 100 当上限是错的 ✗）*/

/* ★★★ 2026-10-11 owner 定案（「额外掉的通路」）✓：★ 一手只对 **`kind 9`（加槽的灵力道具）** 入账 ✓，
 *   ★ 而本卡的 P 点模型（`pofv_core.c` 的 `pofv_power_t`）**没有类型** ✗ ⇒ 原来"拾每一件 P 点都入账" ✗
 *   = 那条额外通路 ✓。★ 修法（最小且贴一手 ✓）：★ **把入账挪到"kind 9 掉落的那一刻"** ✓
 *   —— 一手那 5 条来源本来就都是**掉落/生成**触发 ✓ —— 并**停掉拾 P 点那条** ✗。
 *   ★ 公式照一手 ✓：`+= 15 × clamp(当前蓄力, 0, 100) / 100`（每颗 ✓、cap 2500 ✓）。*/
static void pofv_rei_credit(int32_t items)
{
    float ratio;
    if (items <= 0) return;
    ratio = s_aura_charge / 100.0f;
    if (ratio < 0.0f) ratio = 0.0f;
    if (ratio > 1.0f) ratio = 1.0f;
    s_reiryoku += POFV_REI_GAIN_PER_ITEM * ratio * (float)items;
    if (s_reiryoku > POFV_REI_MAX) s_reiryoku = POFV_REI_MAX;      /* 一手夹到 2500 ✓ */
}
/* ⚠ 2026-10-06 核实订正（我上一版把两处出处标错 ✗）：
 *   · 一手**蓄力槽**是 `AuraCtrlInf+0x94`（**另一个结构体的同偏移** ✗），速率 = `clamp(S+0x88,0,200) × 10 / 100`
 *     （`06-aura.md:37` ✓；满 `S+0x88=200` ⇒ **20/帧**）⇒ 本卡取 `5.0f`（等价 `S+0x88=50`，≈40 帧蓄满）
 *     属 **❓自定**（owner：「结界大小 = 蓄力值」✓ 未定速率 ✓）；
 *   · `aura+0x94`（**动画器**）= **时长 20 帧** ✓、`aura+0x98` = **模式 4**（= ease-out `1−(1−f)²` ✓）
 *     —— 与「蓄力步长」**无关** ✗（`06-aura.md` §7.1/§10.2/§534 ✓）。
 *   · 一手的半径是**两层**：`+0x6c`（初值 1.0）⇒ 动画器（时长 `+0x94`=20 / 模式 `+0x98`=4）⇒ `+0x9c`（判定真用 ✓）。
 *     本卡按 owner「大小 = 蓄力」做**直接映射**（`R = 蓄力×K/100`）⇒ 起点/两层 ease 这层被略去 **❓**（已登记 ✓）。*/
/* ★★★ 2026-10-06 owner：「**蓄力速率按照核出来是什么是什么**」✓ ⇒ 照一手公式（**不再自定** ✓）：
 *   一手（`06-aura.md:37` ✓）蓄力槽 `AuraCtrlInf+0x94` 的唯一涨点 `0x4e7d29`，
 *   速率 = **`clamp(S+0x88, 0, 200) × 10 / 100`** ✓；`S+0x88` 是**蓄力速度**（`:36`：`@AUN1` けしかける狛犬 →
 *   `S+0x88 = 125` ✓ ⇒ 仓里唯一有出处的取值 ✓）⇒ 本卡取同值 ⇒ 速率 = **12.5 / 帧** ⇒ **16 帧蓄满** ✓。
 *   ❓ 仅剩「本卡该用哪个 `S+0x88`」：一手未记**基值**（125 只出自能力接线 ✓）⇒ 先取 125 ✓ 并标 ❓。*/
/* ⚠ 2026-10-06 订正：一手**没有**「每帧涨多少」这个速率用于本卡 ✗ —— 见下面 `s_aura_charge` 处的说明 ✓。
 *   原 `S+0x88`（蓄力速度）只被两张阿吽卡写（置 125 ✓），本卡不采用 ✗。*/
#define POFV_AURA_CHARGE_SPEED  125.0f   /* ❓ 保留仅作存档：一手 `S+0x88`（`:36`）✓，本卡不用 ✗ */
#define POFV_AURA_CHARGE_STEP   (POFV_AURA_CHARGE_SPEED * 10.0f / 100.0f)   /* = 12.5/帧 ✓（一手指令 ✓）*/

/* ★★★ 2026-10-06 角色分派 —— owner 已定「**灵梦按照 TH19 做，魔理沙按照 TH19 做**」✓，
 *   但**「当前自机是哪个角色」的读取点在本仓查不到** ✗（`sdk.h`/`engine.h` 无角色字段；
 *   `engine/player/th18/01` 的 zPlayer 字段图只有坐标；`engine/anm/th18/04` 的 `N→文件` 槽表
 *   只覆盖 bullet/effect/enemy/stage；`engine/anm/th18/06` 的 AnmLoaded/sprite 表无文件名；
 *   其它卡也都没判过角色）⇒ 按 goal 的兜底**先只接灵梦**（其余角色亦显灵梦 ✓）。
 *   `player+0x10` = 当前自机 anm（`engine.h:588` ✓）虽是载体，但 TH18 的 `plXX.anm` 里
 *   **没有** TH19 的オーラ配方 ✗（已实读 local/th18.v1.00a/anm/ 下 pl00 与 pl01 的 anm 文本 ✓）
 *   ⇒ 无法靠「从自机 anm 起 VM」绕过角色判定 ✗。
 *   ❓ **等 owner 一句**：TH18 机型/角色 id 在 `PLAYER+0x???`，或「槽→文件名」的读取口；
 *      答了之后**只改这一个函数**即可让魔理沙走 403/404/405 + 竖带判定 ✓。*/
/* ★★★ 角色读取点（**一手，2026-10-06 找到** ✓）：`engine/sht/th18/01-file-layout-and-shooterset-index.md` §1
 *   「装载：`Player__initialize` `0x45a7a0`」原文：
 *     `buf = load_file(sht_name_table[DAT_004cccf8 + DAT_004cccf4]);   // 名字表 0x4b7088：pl00/01/02/03.sht`
 *   ＋同节注：「文件名表 `0x4b7088`（4 × char*），下标 = `0x4cccf8`（自机 B/A 之类）＋ **`0x4cccf4`（角色）**」
 *   ⇒ **`0x4cccf4` = 角色下标**（0 = `pl00` 灵梦 / 1 = `pl01` 魔理沙 ✓，与 `engine/sht/th17/02:175` 的
 *     「`pl00/pl01/pl02/pl03 = 60/67/75/57`」四角色一致 ✓）。
 *   ⚠ 按纪律**不写进 `engine.h`**（共享面 ✗）⇒ 就近定义 ✓；读一个 DWORD，安全 ✓。*/
#define POFV_ADDR_PLAYER_CHAR 0x4cccf4u

/* ★★★ 2026-10-06 owner 实指正：「四名自机**全都核完了**，你却只接了灵梦魔理沙」✗ ⇒ 改成四角色分派 ✓：
 *   `0x4cccf4`（一手角色下标 ✓）= TH18 的 **pl00 灵梦 / pl01 魔理沙 / pl02 咲夜 / pl03 早苗** ✓
 *   （对照 `engine/sht/th17/02-hyper-shot-groups.md:176`：「`pl02`/`pl03` 在 TH18 是咲夜/早苗」✓）。
 *   各自的**一手体系**（owner 定 ✓）：
 *   · **灵梦** ⇒ TH19 灵梦套：圆 `drawPoly(1.0,64)`（我方脚本 402 = 一手 `pl00:script15` 逐字 ✓）、`K=64`、过渡 20/4 ✓；
 *   · **魔理沙** ⇒ TH19 魔理沙套：两条 `drawRectGrad(1,448)` 竖带（403+404+405 = 一手 `pl01:script15/16/17` 逐字 ✓）、`K=14`、过渡 20/4 ✓；
 *   · **咲夜** ⇒ **TH09 套** ✓（一手 `th09/02:493`：**圆 ∩ 扇形**，半径 `+0x7c`、方向 `+0x80`、半张角 `+0x84` ✓；逐角色 type 表 `0x4a1a10` 的 pl02 → type **17**、anm script **88** ✓）——
 *     ⚠ 她的领域 **anm 脚本我还没解出来** ✗（下一步：解 `local/th09.v1.50a/dat/pl02.anm` 的 script88 ✓）；在此之前她**暂用圆** ✓ 并标 ❓；
 *   · **早苗** ⇒ TH19 早苗套：**同圆**（一手 `pl02:script16` 与灵梦 script15 **逐字相同** ✓ ⇒ 复用 402 ✓）、`K=128`（⇒ 满格 **256** ✓）、过渡 **60/2** ✓（先慢后快 ⇒ 蓄力感 ✓）。
 *   ❓ 待 owner：咲夜的「张开律」（`th09/02:497/508` 仍 🟡）+ 早苗/咲夜的**过渡层**要不要现在补。*/
static int pofv_aura_kind(void)
{
    const int32_t c = *(const int32_t *)POFV_ADDR_PLAYER_CHAR;
    if (c == 1) return 1;      /* 魔理沙 ⇒ TH19 竖带 ✓ */
    if (c == 2) return 2;      /* 咲夜   ⇒ TH09 圆∩扇形（暂圆 ❓）✓ */
    if (c == 3) return 3;      /* 早苗   ⇒ TH19 圆、K=128、60/2 ✓ */
    return 0;                  /* 灵梦（含未知值）⇒ TH19 圆、K=64、20/4 ✓ */
}

static int pofv_aura_marisa(void) { return pofv_aura_kind() == 1; }   /* 兼容旧调用 ✓ */

/* ★★★ 2026-10-06 咲夜的判定入口（owner：「**继续接**」✓）：一手 `th09/02:493` 的形状 = **圆 × 扇形** ✓
 *   ⇒ 本卡把「命中判定」收成一个入口：**咲夜走扇形** ✓（`ux/uy` = 方向参考向量 ✓、`half_deg` = 半张角 ✓）、
 *     其余角色走各自的形状（魔理沙 = 竖带 ✓ / 灵梦·早苗 = 圆 ✓），`pad` 一律由调用方给（§9.15 ✓）。*/
static int pofv_aura_hit_here(float dx, float dy, float pad)
{
    if (s_aura_kind == 2)
        return pofv_aura_contains_sector(dx, dy, s_aura_r, pad,
                                         /* ★★ owner：「**判定贴合领域**」✓ —— 视觉的朝向是
                                          *   `s_aura_mid_ang` = **运动向量的反向**（再 0.25/帧缓动 ✓）
                                          *   ⇒ 判定必须用**同一个方向** ✓（先前传的是运动向量本身 ✗ ⇒ 差 180° ✗）。
                                          *   ⇒ 这里取 `-s_aura_dir_x/-s_aura_dir_y` ✓ = 与视觉同向 ✓
                                          *     （视觉有指数滞后 ⇒ 转身瞬间有极小偏差，属预期 ✓）。*/
                                         -s_aura_dir_x, -s_aura_dir_y,
                                         /* ★★★ 2026-10-06 核实订正（`th09/02` §3.9–§3.14 ✓）：`+3°/帧`、上限 90° 是
                                          *   **全张角** ✓ ⇒ **半张角 = 全/2 ≤ 45°** ✓；我原先直接把它当半张角用 ✗（**错一倍** ✓）。
                                          *   判定接口收的是**半张角**（度 ✓）⇒ 这里传一半 ✓。*/
                                         (int)(s_aura_sec_deg * 0.5f));
    return pofv_aura_contains_ex(dx, dy, s_aura_r, pad,
                                 (s_aura_kind == 1) ? POFV_AURA_SHAPE_BAND : POFV_AURA_SHAPE_CIRCLE);
}
#define POFV_AURA_SLOW       0.5f     /* ❓ 结界内敌人/敌弹的减速系数（一手只说「变慢」）*/     /* ★ 固灵结界半径（❓ 一手「形状随角色」⇒ 本卡先统一用一个圆）*/
#define POFV_GHOST_LEVEL_MAX 3
/* ★ 幽灵寿命：一手 TH09 `enemy.ecl Sub0` 末尾是 **`+5000: enemy_delete()`** ⇒ **5000 帧** ✓
 *   （⚠ spawn 的 `life` 字段必须 <1000 ⇒ 只能在**卡侧按 age 计时** ✓）
 *   ❓ 与一手的差别：一手是**瞬时** `enemy_delete()`；我们走 ECL 的离场支（飞向正上方 → `+60: delete()`）✓ */
#define POFV_GHOST_LIFE_FRAMES 5000
/* ★ 宽限：`ce_enemy_by_id` 单帧查不到**不算死**（新生成/链表切换时可能闪一次）——连续失败才收圆。
 *   这是「附属圆看不见」的一个高度可疑原因（旧版一次查不到就立刻发事件 1 淡出）。 */
static int32_t  s_spirit_miss[POFV_SPIRIT_MAX];
#define POFV_SPIRIT_MISS_MAX 8

/* 敌人身上的两个「位置」字段（都以 SDK/本仓一手为据，**不是盲写**）：
 *   · `0x12b4` = `zEnemyData.abs_pos`（`sdk.h` 的 `CE_SPAWN_POS` 注释：「float3 出生世界坐标 → enemy+0x12b4」）
 *   · `0x1270` = 死亡事件报的那对「世界坐标」（`sdk/public.h` 里 `x,y` 的注释指的就是 `enemy+0x1270 / +0x1274`）
 * 两个都推进（❓ 若实跑发现只有一个生效/打架，再收敛到一个并登记）。 */
#define POFV_ENEMY_ABS_POS  0x12b4
/* ★★★ 2026-10-05 一手（ExpHP `th18.v1.00a/type-structs-own.json`）：
 *   `zEnemyData+0x88 = abs_pos`（类型 **`zPosVel`**），而 `zPosVel` 的布局是
 *     `+0x00 pos`(zFloat3) · `+0x18 vel_norm/circle_angular_velocity` · `+0x20 circle_radius` ·
 *     `+0x24 circle_radial_speed` · **`+0x34 __field_34__maybe_velocity`(zFloat3)** · `+0x40 flags`
 *   ⇒ **速度在 `abs_pos + 0x34`**（= 敌人 `0x12b4 + 0x34 = 0x12e8`）。
 *   ⇒ 引擎每帧用**速度**积分位置 ⇒ **只写速度**（写位置会被覆盖，这就是「灵不移动」的真因）。 */
/* ★★★ W4 耐久骤降（一手 `lifeSet(50)`）：敌人 life = `zEnemyData + 0x4ff4`（类型 **`zEnemyLife`**，ExpHP 的一手字段图），
 *   其内部：`+0x00 current` · `+0x04 maximum` · `+0x08 remaining_for_cur_attack` · `+0x14 total_damage_including_ignored` · `+0x18 is_spell`。
 *   ⇒ `lifeSet(50)` = 把 **current 与 maximum 都写 50**（不盲写：偏移与语义都出自一手结构体图）。 */
#define POFV_ENEMY_LIFE     0x4ff4
#define POFV_SPIRIT_DIS_HP  50
#define POFV_ENEMY_WORLD    0x1270
static int      s_spirit_next;

/* ★★ W2 + owner 定盘：还活着的灵有几只 —— 遍历灵账、`ce_enemy_by_id` 看还在不在（账是环，不删条目）。
 *   `fam < 0` ⇒ 数全部；否则**只数该族**（owner：「判上限 **09 算 09、19 算 19**」）——
 *   族由 kind 决定（`pofv_spirit_family_of()`），**存储与流程不分族**。 */
/* ★ kind + 帧号 ⇒ 本体成员号（贴图 = 原生切图的两个图集；各 family 成员号连续）。
 *   純霊 8 帧、其余 4 帧（一手「身体帧每 6 帧一换」⇒ 调用方传 `age / 6`）。
 *   ❓ 幽灵一档现在只用**绿**（level 1）；黄/红（level 2/3）随 level 记账接。 */
static int pofv_spirit_body_sprite(int32_t kind, int32_t level, int disabled, int frame)
{
    static const int base[POFV_SPIRIT_KIND__COUNT] = {
        CE_ANM_ABILITY_SPRITE_SPIRIT_PURE_0,
        CE_ANM_ABILITY_SPRITE_SPIRIT_WOLF_0,
        CE_ANM_ABILITY_SPRITE_SPIRIT_OTTER_0,
        CE_ANM_ABILITY_SPRITE_SPIRIT_EAGLE_0,
        CE_ANM_ABILITY_SPRITE_SPIRIT_GHOSTG_0
    };
    int k = (kind >= 0 && kind < POFV_SPIRIT_KIND__COUNT) ? (int)kind : (int)POFV_SPIRIT_KIND_PURE;
    int n = (k == (int)POFV_SPIRIT_KIND_PURE) ? 8 : 4;
    if (frame < 0) frame = 0;
    /* ★★★ owner 2026-10-06：「**固化的时候 sprite 也会变**」✓ ＋ 核实方：一手**每族一档** ✓
     *   ⇒ 补上此前缺失的**純霊档**（`ATLAS_SPIRIT_DISABLE_PURE` ✓，8 帧 = 一手 TH19 `enemy.anm`
     *     `sprite112–119` 的「4 帧实心球 + 4 帧空心环」✓；来源与 UV 见 `AUDIT.md` §BE-167 ✓）。*/
    if (disabled && k == (int)POFV_SPIRIT_KIND_PURE) {
        /* ★★★ owner 2026-10-06 修正（一手链 ✓）：`ins_303(0, 106)` 的 **106 是 anm「脚本号」而非 sprite 号** ✗
         *   —— opcode 303 → `FUN_004fe980` → `FUN_004cb960(&vm, …, arg1(脚本号), …, *(enemy+0x28c)+7, 2, 0)` ✓
         *   （与光环同机制：`FUN_004cb960(…, 0xf, …)` = `pl00.anm` **script15** ✓）。
         *   ⇒ **純霊固化 = `enemy.anm` script106** ✓，其 sprite = **144–148**（`enemy2.png`，y=96，**32×32、5 帧** ✓）
         *   ⇒ 本卡的 `SPIRIT_DISP4_0..4` 即按此重切 ✓（旧的 8 帧「球/环」是切错来源 ✗，已删）。*/
        int b = (int)CE_ANM_ABILITY_SPRITE_SPIRIT_DISP4_0;
        return b + ((frame < 0 ? 0 : frame) % 5);   /* 5 帧循环 ✓ */
    }
    if (disabled && (k == (int)POFV_SPIRIT_KIND_WOLF || k == (int)POFV_SPIRIT_KIND_OTTER ||
                     k == (int)POFV_SPIRIT_KIND_EAGLE)) {
        /* ★ W4：沈静化外观 = 图集 106（`ATLAS_SPIRIT_DISABLE`，D1/D2/D3 各 4 帧 32×48）。 */
        int d = (k == (int)POFV_SPIRIT_KIND_WOLF) ? 0 : (k == (int)POFV_SPIRIT_KIND_OTTER) ? 1 : 2;
        int base2 = (d == 0) ? (int)CE_ANM_ABILITY_SPRITE_SPIRIT_DISD1_0
                  : (d == 1) ? (int)CE_ANM_ABILITY_SPRITE_SPIRIT_DISD2_0
                             : (int)CE_ANM_ABILITY_SPRITE_SPIRIT_DISD3_0;
        return base2 + (frame % 4);
    }
    if (disabled && k == (int)POFV_SPIRIT_KIND_GHOST_TH09) {
        /* ★★ TH09 幽灵**被固化** ⇒ 一手「鸟形 → 火焰/光球态」（`enemy.anm` script23/24/25 = level 1/2/3 各 8 帧 ✓）
         *   ⇒ 用图集 107（`ATLAS_SPIRIT_CALM`，sprite 672-695）✓ */
        int lvl = (level >= 1 && level <= 3) ? (int)level : 1;
        int base3 = (lvl == 1) ? (int)CE_ANM_ABILITY_SPRITE_SPIRIT_CALMG_0
                  : (lvl == 2) ? (int)CE_ANM_ABILITY_SPRITE_SPIRIT_CALMY_0
                               : (int)CE_ANM_ABILITY_SPRITE_SPIRIT_CALMR_0;
        return base3 + (frame % 8);
    }
    if (k == (int)POFV_SPIRIT_KIND_GHOST_TH09) {
        /* ★ 一手：幽灵的 level 就是颜色（1 绿 / 2 黄 / 3 红），图集 105 里 G/Y/R 各 4 帧连续排。 */
        int lvl = (level >= 1 && level <= POFV_GHOST_LEVEL_MAX) ? (int)level : 1;
        return base[k] + (lvl - 1) * 4 + (frame % 4);
    }
    return base[k] + (frame % n);
}

static int pofv_spirit_live_count_fam(int fam)
{
    uint8_t *em = CE_ENEMY_MGR();
    int i, n = 0;
    if (!em) return 0;
    for (i = 0; i < POFV_SPIRIT_MAX; ++i) {
        if (!s_spirit_id[i]) continue;
        if (fam >= 0 && pofv_spirit_family_of(s_spirit_kind[i]) != fam) continue;
        if (ce_enemy_by_id(em, (int32_t)s_spirit_id[i])) ++n;
    }
    return n;
}

/* ★ 记账并**返回它写的那条槽**（调用方据此挂本体/附属圆的 VM ⇒ 结构上不可能再错位）。 */
static int pofv_spirit_slot_free(void)
{
    int i;
    for (i = 0; i < POFV_SPIRIT_MAX; ++i) if (!s_spirit_id[i]) return 1;
    return 0;
}

static int pofv_note_spirit(uint32_t enemy_id, int32_t kind)
{
    int i, sl = -1;
    /* ★★★ 2026-10-06 owner：「**满了就别创建了**」✓ ⇒ **取消「覆盖最旧」** ✗：
     *   改成**找空格**；一个都没有就返回 **-1**（调用方在生成口已先判满 ✓ ⇒ 正常不会走到这里 ✓）。
     *   （旧策略「满 32 格覆盖最旧」会造成：被顶掉的灵**还在场上却没户口** ⇒ 卡侧不再给它写旋转/贴图
     *     ⇒ 实跑看到的「**旋转 90° 全消失**」＋「**黄色 overplot 在灵上面**」两个症状 ✓ —— 一并消灭 ✓。）*/
    for (i = 0; i < POFV_SPIRIT_MAX; ++i) {
        sl = (s_spirit_next + i) % POFV_SPIRIT_MAX;
        if (!s_spirit_id[sl]) break;
    }
    if (i == POFV_SPIRIT_MAX) { ce_log("pofv: 灵账已满 %d 格 ⇒ **拒绝记账**（不该发生，生成口应先判满 ✓）", POFV_SPIRIT_MAX); return -1; }
    if (s_spirit_vm[sl])   { ce_anm_delete((uint32_t)s_spirit_vm[sl]);   s_spirit_vm[sl]   = 0; }
    if (s_spirit_body[sl]) { ce_anm_delete((uint32_t)s_spirit_body[sl]); s_spirit_body[sl] = 0; }
    s_spirit_id[sl] = enemy_id;
    s_spirit_kind[sl] = kind;
    s_spirit_next = (sl + 1) % POFV_SPIRIT_MAX;
    ce_log("pofv: note spirit id=%u kind=%d (slot %d)", (unsigned)enemy_id, (int)kind, sl);
    return sl;
}

/* ★ W3：从账里**取走**（取到 ⇒ 返回它的 kind 并清条目 ⇒ 同一 id 不会二次触发）。取不到 = 不是我们的灵。 */
static int pofv_spirit_take(uint32_t enemy_id, int32_t *out_level, int *out_incoming)
{
    int i;
    if (out_level) *out_level = -1;
    if (out_incoming) *out_incoming = 0;
    if (!enemy_id) return -1;
    for (i = 0; i < POFV_SPIRIT_MAX; ++i)
        if (s_spirit_id[i] == enemy_id) {
            if (out_incoming) *out_incoming = s_spirit_from_incoming[i];   /* ★ Q4：清账前带出来源 ✓ */
            int32_t k = s_spirit_kind[i];
            if (out_level) *out_level = s_spirit_level[i];
            s_spirit_id[i] = 0; s_spirit_kind[i] = -1; s_spirit_level[i] = 0;
            return (int)k;
        }
    return -1;
}

static int pofv_spirit_is_ours(uint32_t enemy_id)
{
    int i;
    if (!enemy_id) return 0;
    for (i = 0; i < POFV_SPIRIT_MAX; ++i)
        if (s_spirit_id[i] == enemy_id) return 1;
    return 0;
}


/* VM 吃**世界坐标**（`ce_anm_set_pos` 就是世界坐标，铁证 `cards/animal.c:1591`）。 */
static void vm_set_world_pos(uint32_t id, float wx, float wy)
{
    ce_anm_set_pos(id, wx, wy, 0.0f);
}

/* ============================ 消弹（自己扫池，只吃可消弹） ============================
 * 一手（TH19）：`FUN_004ee770(pos, 64.0f, 0, 99999, 0)` = 「半径内消弹」原语，**每帧每台一次**。
 * ⚠ 它的判据（弹类型 `+0x1044 ∈ {1,2}` 且 `+0x14 == 0`）是 **TH19 的字段**，TH18 的弹上没有对应物
 *   （owner：「th18 不存在白弹」）⇒ 这里用本仓设计的那套：**弹半径 `+0x658` ≤ 4.0 的可消**
 *   （`pofv_is_white_bullet()`），命中判据逐字照抄引擎 `0x429370`：
 *   `state ∈ {1,2}`、`dist² ≤ (弹半径 × 0.5 + R)²`（判定式里的弹半径用**这颗弹自己的**）。
 * 成本：2000 槽 × 一次比较（早停于 max_count）——每帧最多 3 台（暴风的 3 个圈）。 */
/* ================= ③ 消弹火花（`etbreak`）—— **一手原生 8 帧** =================
 * ★ 一手（`HANDOFF-109-C1-C4.md` 之外，逐条见脚本 431 的注释 ✓）：消弹时**每消掉一颗弹**
 *   就在**那颗弹的位置**播一次 `effect/etbreak.png` 的 8 帧火花 ✓（`bullet.anm` 的 `script162/163` ✓）。
 *   ⇒ ★ 一手的"消弹圈"**没有**什么大圆环 ✗ —— ★ 就是这些**灰白尖刺火花** ✓
 *   （★ 我上一轮自己烤的红环已废 ✗，owner 实跑问的正是它 ✓）。
 * ★ 本卡：★ 火花由**脚本自己**在第 16 帧 `delete()` ✓ ⇒ 卡侧只管
 *   ① 起 VM（放在被消弹的坐标 ✓）、② 记一个**略长于 16 帧**的占位计时器 ✓、
 *   ③ 到期把句柄清掉（★ 用 `ce_anm_get_vm()` 先判活 ✓ —— 脚本可能已自删 ✗，不能盲删 ✓）。
 * ★ 池子满了就**丢最新的**（宁可少一朵花，也不挤掉刚起的 ✓）。*/
#define POFV_SPARK_MAX   32
#define POFV_SPARK_LIFE  18      /* ★ 略长于脚本的 16 帧 ✓ */
static int32_t s_spark_vm[POFV_SPARK_MAX];
static int     s_spark_ttl[POFV_SPARK_MAX];

/* ★ 在被消掉的那颗弹的位置起一朵火花 ✓（一手：逐颗 ✓）*/
static void pofv_spark_spawn(float x, float y)
{
    int k, slot = -1;
    for (k = 0; k < POFV_SPARK_MAX; ++k) if (s_spark_ttl[k] <= 0) { slot = k; break; }
    if (slot < 0) return;                          /* 池满 ⇒ 这朵不放了 ✓ */
    s_spark_vm[slot] = (int32_t)ce_anm_spawn(CE_ABILITY_ANM(),
                        CE_ANM_ABILITY_SCRIPT_POFV109_ETBREAK,
                        6);   /* ★ 世界层 6（与脚本 431 的 layer(6) 一致 ✓；常量定义在文件后面，这里用字面量 ✓）*/
    if (!s_spark_vm[slot]) return;
    ce_anm_set_pos((uint32_t)s_spark_vm[slot], x, y, 0.0f);
    s_spark_ttl[slot] = POFV_SPARK_LIFE;
}

/* ★ 每帧：占位计时器递减；到期把句柄清掉（**先判活** ✗ 不盲删 ✓）。*/
static void pofv_spark_tick(void)
{
    int k;
    for (k = 0; k < POFV_SPARK_MAX; ++k) {
        if (s_spark_ttl[k] <= 0) continue;
        if (--s_spark_ttl[k] > 0) continue;
        if (s_spark_vm[k] && ce_anm_get_vm((uint32_t)s_spark_vm[k]))
            ce_anm_delete((uint32_t)s_spark_vm[k]);
        s_spark_vm[k] = 0;
    }
}

static int cancel_white_in_circle(const float *pos, float r, int max_count, int mode)
{
    uint8_t *bm = CE_BULLET_MGR();
    uint32_t i;
    int n = 0;

    if (!bm || !pos || r <= 0.0f || max_count <= 0) return 0;
    for (i = 0; i < CE_BM_BULLET_COUNT; ++i) {
        uint8_t *b = bm + CE_BM_BULLETS + (size_t)i * CE_BULLET_STRIDE;
        uint16_t state = *(const uint16_t *)(b + CE_BULLET_STATE);
        float br, dx, dy, rr;
        if (state != 1 && state != 2) continue;        /* 0 = 空槽；3 / 4 = 别的状态 */
        br = *(const float *)(b + POFV_BULLET_RADIUS_OFF);
        if (!pofv_is_white_bullet(br)) continue;       /* ★ 可消弹判据（哨兵弹种在这里被跳掉）*/
        dx = *(const float *)(b + CE_BULLET_POS) - pos[0];
        dy = *(const float *)(b + CE_BULLET_POS + 4) - pos[1];
        rr = br * 0.5f + r;                            /* 照 0x429370 的判定式 */
        if (dx * dx + dy * dy > rr * rr) continue;
        {
            /* ★★ 一手：**每消一颗**就在**那颗弹的位置**播一次 `etbreak` 火花 ✓（见上面那节 ✓）*/
            const float bx = *(const float *)(b + CE_BULLET_POS);
            const float by = *(const float *)(b + CE_BULLET_POS + 4);
            ((pofv_fn_bullet_cancel_t)POFV_FN_BULLET_CANCEL)(b, mode);
            pofv_spark_spawn(bx, by);
        }
        if (++n >= max_count) break;
    }
    return n;
}

/* ★★ 固灵结界碰敌弹（owner 口径 6 ③「掉灵力道具」）—— 扫一遍弹池，把「**在结界内且本颗还没算过**」
 *   的弹标成已算，最多把 `cap` 个位置抄进 `ox/oy`。返回本帧**新命中**的颗数。
 *   ⚠ 与 `cancel_white_in_circle` 不同：这里**不消弹、只碰**（一手 TH19 的领域消弹会顺手消掉，
 *     本卡不接管别人的弹）⇒ 用 `s_aura_bullet_done[]` 闩锁保证每颗弹只算一次。
 *   几何 = `pofv_aura_hit_here()`（逐角色形状：圆 / 竖带 / 圆×扇形），`pad` = **这颗弹自己的半径**
 *   （一手 TH19 传 `bullet+0x674`）。中心 = **自机**（本卡的结界就是挂在自机处）。*/
static int aura_scan_bullets(const float *center, int cap, float *ox, float *oy)
{
    uint8_t *bm = CE_BULLET_MGR();
    uint32_t i;
    int n = 0;

    if (!bm || !center || !ox || !oy || cap <= 0 || s_aura_r <= 0.0f) return 0;
    for (i = 0; i < CE_BM_BULLET_COUNT; ++i) {
        uint8_t *b = bm + CE_BM_BULLETS + (size_t)i * CE_BULLET_STRIDE;
        uint16_t state = *(const uint16_t *)(b + CE_BULLET_STATE);
        float br, bx, by;
        /* ★★★★ 2026-10-08 **修 bug**（owner：「结界碰弹**没有生成灵力道具**」✗ ⇒ 找到真因 ✓）：
         *   原先写成 `state != 1 && state != 2 ⇒ skip` ✗ ⇒ **只认 state 1/2** ✗，而实际弹的 state 值不止这两个
         *   ⇒ **一颗弹都扫不到** ⇒ 永远不掉道具 ✓。
         *   ⇒ 改照**本仓已验证可用**的那套（卡 115 `lolk.c:491` ✓）：**`state == 0` = 空槽** ✓，
         *     非 0 一律当活弹 ✓（同时把闩锁在空槽时清掉 ✓）。*/
        if (state == 0) { s_aura_bullet_done[i] = 0; continue; }   /* 空槽：清闩锁 ✓ */
        if (s_aura_bullet_done[i]) continue;                                    /* ★ 这颗弹已经算过 */
        br = *(const float *)(b + POFV_BULLET_RADIUS_OFF);
        if (!(br > 0.0f)) continue;                                             /* 半径 0 的哨兵弹种：不算 */
        bx = *(const float *)(b + CE_BULLET_POS);
        by = *(const float *)(b + CE_BULLET_POS + 4);
        if (!pofv_aura_hit_here(bx - center[0], by - center[1], br)) continue;
        s_aura_bullet_done[i] = 1;
        if (n < cap) { ox[n] = bx; oy[n] = by; }
        ++n;
    }
    return n;
}

/* ================================================================================
 * ★★★★ 固灵结界碰敌弹的**视觉**：**染色 + 抖动**（一手 = TH19，2026-10-09）
 * ================================================================================
 * 一手来源：TH19 `FUN_004f5130`（`0x4f5130`–`0x4f55b4`，**`BulletInf` 每帧**；全链见
 *   `engine/versus/th19/05-four-mechanics.md` §8 的 aura 判定那一段 + `06-aura.md` §8）：
 *
 *   ① **染色**：给**该弹的 VM** 的颜色覆盖字段写 **RGB(255, G, 128)**。
 *      本仓对应字段 = **`bullet + 0x28`（`zBullet.vm`，= `lolk.c` 的 `CE_BULLET_VM`）取 VM，
 *      再写 `CE_ANM_VM_COLOR1`（`engine.h:307`，= `vm + 0x524`）**。
 *      写法 = **一个 AARRGGBB dword**：`0xFF << 24 | R=255 << 16 | G << 8 | B=128` ⇒ `0xFFFF??80`。
 *   ② **G 的公式（一手 ✓）**：`G = clamp(2 * (0x68 − n), 0x60, 0xff)`，
 *      其中 **`n` = 这颗弹「连续被结界碰到的帧数」**（一手：`[esi+0xff0]`，每帧 +1、离开就复位 0）。
 *      ⇒ n=0 时 `G = 0xd0`（= `RGB(255,208,128)`，正好等于卡 115 的 `LOLK_GRAZED_COLOR`）；
 *        n≥56（`0x68 − 0x30`）后钳在 `0x60`。
 *   ③ **模型（一手 ✓，重要）**：**每帧无条件重写、不建快照表、不还原** —— 一手里弹出结界后的回落
 *      靠**引擎自己的 anm color 插值**。⚠ 本仓**没有**这套机制 ⇒ 本卡必须自己「**记住染过谁 +
 *      离开时写回原色**」；这就是下面这张私有小表的**唯一**理由（不是一手的模型）。
 *   ④ **抖动**：一手每帧重掷两个 float（`bullet+0x48` / `+0x4c`，量级 ≈±1、z=0）。
 *      ❓ **那两个字段的语义在 TH19 侧未定位**（`+0x48`/`+0x4c` 在 `zBullet` 上到底是什么没核）
 *      ⇒ 先做成默认关的开关 `POFV_AURA_BULLET_SHAKE`（**= 0**），见下面的注释。
 *
 * ⚠ 与卡 115 / 111 / 108 的**门字段冲突**（❓ **已知未解决**，已如实登记 `TUNING.md`，**不声称已解决**）：
 *   同一个 `bullet+0x28 + CE_ANM_VM_COLOR1` 115（`lolk.c:512` 染橙）、111（`signal.c` 染红）、108 也写。
 *   本卡采取「**每帧重写、退出即还原**」；**111 也写在 `on_draw` 里**（`signal.c:2013-2015` 明文：
 *   写 `on_tick_2` 会被 115 覆盖），而卡按 id 顺序跑 ⇒ **111（id=111 > 109）的 `on_draw` 在本卡之后跑**
 *   ⇒ 同一颗弹同帧都被写时**它的红仍可能压住本卡**。
 *   ⚠ 与 115 的关系：本表的做法**有意不照抄 `lolk.c:434-457` 的 `tint_mark`/`tint_restore_all`**
 *     （那是 115 私有的两张 256 格表）；本卡自带一张**只记「本卡染过、且还在我染色期内」的弹**的小表，
 *     115 与本表互不引用、互不还原对方的弹。
 *
 * ⚠ **A 也写 `0xff`**（一手那条 dword 就是 `0xFFFF??80`）—— 与 115/111「**只改 RGB、保留 alpha**」
 *   不同（它们的理由是「弹的 alpha 由脚本逐帧插值，连 alpha 一起写会按掉淡入淡出」）。本卡照一手
 *   整条写、还原时把**整条快照**写回；若实跑发现淡入弹被钉成不透明，改成「只写 RGB」只需动
 *   `pofv_aura_tint_color()` 一处（❓ 已登记）。
 */
#define POFV_BULLET_VM_OFF      0x28    /* `zBullet.vm` = zAnmVm（与 `lolk.c` 的 `CE_BULLET_VM` 同格）*/
#define POFV_AURA_TINT_R        0xffu
#define POFV_AURA_TINT_B        0x80u

/* ★ 抖动开关：**默认关**（一手 `FUN_004f5130` 每帧重掷 `bullet+0x48` / `+0x4c` 两个 float，
 *   量级 ≈±1、z=0）。❓ **语义未定位**（字段在 `zBullet` 上代表什么没核出来；115/111/108 都没有抖动）
 *   ⇒ 先关着；开它只写 `+0x48`/`+0x4c`/`+0x50` 三个 float，别的字段一概不碰。*/
#define POFV_AURA_BULLET_SHAKE  0

typedef struct {
    uint8_t *b;         /* 弹池槽地址（槽地址稳定 ⇒ 用它当「这颗弹」的身份）*/
    uint32_t orig;      /* 本卡第一次染它之前那一帧的**整条** `color_1`（离开时写回）*/
    uint16_t n;         /* 连续被结界碰到的帧数（一手 `[esi+0xff0]`）*/
} pofv_aura_tint_t;

/* 表容量 = **弹池槽数**（`CE_BM_BULLET_COUNT` = 2000）：表内**每槽至多一条**（下面有 dup 闸）
 * ⇒ **结构上不可能溢出**。保底：万一真满，只是「本帧不再染新弹」（还原照旧跑），不影响正确性。
 * ⚠ 表必须放**文件作用域**（`clear_card()` 要能还原 ⇒ 否则删卡/换局留下染色弹 ✗，铁律）。*/
#define POFV_AURA_TINT_MAX   CE_BM_BULLET_COUNT
static pofv_aura_tint_t s_aura_tint[POFV_AURA_TINT_MAX];
static int s_aura_tint_n;

/* G = clamp(2 * (0x68 − n), 0x60, 0xff) —— 一手公式（n 见上）。*/
static uint32_t pofv_aura_tint_g(uint16_t n)
{
    int32_t v = 2 * (0x68 - (int32_t)n);
    if (v < 0x60) v = 0x60;
    if (v > 0xff) v = 0xff;
    return (uint32_t)v;
}

/* 一条 AARRGGBB dword（一手写法；A=0xff、R=0xff、B=0x80、G 见上）。*/
static uint32_t pofv_aura_tint_color(uint16_t n)
{
    return 0xff000000u | (POFV_AURA_TINT_R << 16) | (pofv_aura_tint_g(n) << 8) | POFV_AURA_TINT_B;
}

/* 还原整张表（删卡 / 换关 / 重开 —— `clear_card()` 里调，**不依赖 tick** ✓ 铁律「长持续效果要交代」）。
 * ⚠ 照 `lolk.c:437-455` 的**教训**：**无条件还原**（不做「颜色被别人改过就跳过」的守卫 —— 那个守卫
 *   会让弹一直留色）。*/
static void pofv_aura_tint_restore_all(void)
{
    int i;
    if (!CE_BULLET_MGR()) { s_aura_tint_n = 0; return; }
    for (i = 0; i < s_aura_tint_n; ++i) {
        volatile uint32_t *col =
            (volatile uint32_t *)(s_aura_tint[i].b + POFV_BULLET_VM_OFF + CE_ANM_VM_COLOR1);
        *col = s_aura_tint[i].orig;
    }
    s_aura_tint_n = 0;
}

#if POFV_AURA_BULLET_SHAKE
/* 一手那两个字段（`+0x48`/`+0x4c`）量级 ≈±1、z=0。语义未定位 ❓ ⇒ 默认关。
 * RNG = 确定性 LCG（不引 libm；同帧同弹同值，跨帧变）。*/
static uint32_t s_aura_shake_rng = 0x9e3779b9u;
static float pofv_aura_shake_unit(void)
{
    s_aura_shake_rng = s_aura_shake_rng * 1664525u + 1013904223u;
    return (float)((s_aura_shake_rng >> 8) & 0xffu) * (2.0f / 255.0f) - 1.0f;   /* [−1, 1] */
}
static void pofv_aura_shake_apply(uint8_t *b)
{
    *(float *)(b + 0x48) = pofv_aura_shake_unit();
    *(float *)(b + 0x4c) = pofv_aura_shake_unit();
    *(float *)(b + 0x50) = 0.0f;          /* z = 0（一手：x/y ≈±1、z=0）*/
}
#endif

/* 每帧（**在 `on_draw` 里跑**）：① 把**当前被结界碰到**的弹**每帧重写**成 `0xFFFF??80`；
 * ② 把「离开结界 / 槽已空」的弹**还原成原色**并移出表。
 *
 * ⚠ **几何 = `pofv_aura_hit_here()`**（与 `aura_scan_bullets()` 的「碰到」判据**同一个入口**），
 *   `pad` = **这颗弹自己的半径**（一手 `FUN_004f5130` 传 `bullet+0x674` 的对应物 = 本仓 `+0x658`）。
 * ⚠ **与 `s_aura_bullet_done[]` 闩锁分层（硬要求）**：那把闩锁是**一次性**的、只管「掉道具」（每颗**一次**）；
 *   染色是**持续**的 ⇒ 本函数**完全不看闩锁**，每帧自己扫、每帧重写（一手「每帧无条件重写」）。
 * ⚠ 「结界在场」的判据 = `s_aura_r > 0`（松手 / 换关 / 删卡都会把它归 0：`on_tick_2` 尾部 +
 *   `clear_card()`）⇒ 不需要在绘制里再读键（`ce_key_down` 不在 `on_draw` 里用）。*/
static void pofv_aura_tint_tick(void)
{
    uint8_t *bm = CE_BULLET_MGR();
    const float *pp = ce_player_pos();
    const int active = (bm && pp && s_aura_r > 0.0f);
    int i, w = 0;
    uint32_t k;

    /* ① 已有登记：还在结界里 ⇒ 保留（n++ 后**重写**本帧的色）；离开 / 槽空 ⇒ 还原 + 移出。*/
    for (i = 0; i < s_aura_tint_n; ++i) {
        uint8_t *b = s_aura_tint[i].b;
        int keep = 0;
        if (active) {
            uint16_t st = *(const uint16_t *)(b + CE_BULLET_STATE);
            float br = *(const float *)(b + POFV_BULLET_RADIUS_OFF);
            if (st != 0 && br > 0.0f) {        /* 与 `aura_scan_bullets()` 同判据（空槽 = 0 ✓）*/
                float dx = *(const float *)(b + CE_BULLET_POS) - pp[0];
                float dy = *(const float *)(b + CE_BULLET_POS + 4) - pp[1];
                keep = pofv_aura_hit_here(dx, dy, br);
            }
        }
        if (keep) {
            volatile uint32_t *col =
                (volatile uint32_t *)(b + POFV_BULLET_VM_OFF + CE_ANM_VM_COLOR1);
            if (s_aura_tint[i].n < 0xffffu) ++s_aura_tint[i].n;   /* 一手：每帧 +1 */
            *col = pofv_aura_tint_color(s_aura_tint[i].n);
#if POFV_AURA_BULLET_SHAKE
            pofv_aura_shake_apply(b);
#endif
            s_aura_tint[w++] = s_aura_tint[i];
        } else {
            volatile uint32_t *col =
                (volatile uint32_t *)(b + POFV_BULLET_VM_OFF + CE_ANM_VM_COLOR1);
            *col = s_aura_tint[i].orig;         /* 无条件还原（lolk.c 的教训）*/
        }
    }
    s_aura_tint_n = w;

    if (!active) return;

    /* ② 本帧**新进**结界的弹：记下原色 + 写 `n = 0` 那一档（`G = 0xd0`）。*/
    for (k = 0; k < CE_BM_BULLET_COUNT; ++k) {
        uint8_t *b = bm + CE_BM_BULLETS + (size_t)k * CE_BULLET_STRIDE;
        uint16_t st = *(const uint16_t *)(b + CE_BULLET_STATE);
        float br, dx, dy;
        int j, dup = 0;
        if (st == 0) continue;
        br = *(const float *)(b + POFV_BULLET_RADIUS_OFF);
        if (!(br > 0.0f)) continue;
        dx = *(const float *)(b + CE_BULLET_POS) - pp[0];
        dy = *(const float *)(b + CE_BULLET_POS + 4) - pp[1];
        if (!pofv_aura_hit_here(dx, dy, br)) continue;
        for (j = 0; j < s_aura_tint_n; ++j) if (s_aura_tint[j].b == b) { dup = 1; break; }
        if (dup) continue;                       /* 已在①里处理（n 已自增）⇒ **每颗弹只记一份** ✓ */
        if (s_aura_tint_n >= POFV_AURA_TINT_MAX) continue;   /* 保底（结构上到不了）*/
        {
            volatile uint32_t *col =
                (volatile uint32_t *)(b + POFV_BULLET_VM_OFF + CE_ANM_VM_COLOR1);
            s_aura_tint[s_aura_tint_n].b = b;
            s_aura_tint[s_aura_tint_n].orig = *col;   /* 整条快照（见文件头对 alpha 的说明）*/
            s_aura_tint[s_aura_tint_n].n = 0;         /* 一手：新碰到 ⇒ n=0 ⇒ G=0xd0 */
            *col = pofv_aura_tint_color(0);
#if POFV_AURA_BULLET_SHAKE
            pofv_aura_shake_apply(b);
#endif
        }
        ++s_aura_tint_n;
    }
}

/* ============================ 伤害（一手：每帧每台一次半径 64 的命中、参数 = 1） ============================
 * 矩形**先裁到场地内**（否则场心靠边时会把还没进屏的敌人打死）。 */
static int storm_hit_clipped(float cx, float cy, float r, int dmg)
{
    float x0 = cx - r, x1 = cx + r;
    float y0 = cy - r, y1 = cy + r;
    float pos[3];
    if (x0 < -POFV_FIELD_HALF_W) x0 = -POFV_FIELD_HALF_W;
    if (x1 >  POFV_FIELD_HALF_W) x1 =  POFV_FIELD_HALF_W;
    if (y0 < 0.0f) y0 = 0.0f;
    if (y1 > POFV_FIELD_H) y1 = POFV_FIELD_H;
    if (x1 <= x0 || y1 <= y0) return 0;             /* 交集空（整个在屏外）⇒ 不打 */
    pos[0] = (x0 + x1) * 0.5f;
    pos[1] = (y0 + y1) * 0.5f;
    pos[2] = 0.0f;
    return ce_damage_rect(pos, 0.0f, POFV_STORM_HIT_LIFE, dmg, x1 - x0, y1 - y0);
}

/* ============================ 画面：每场 1 台 VM（世界层 6）============================
 * ★ 一手（TH19）：圈是**一台 VM** —— 脚本 `sprite(-1); layer(6); drawPoly(1.0f, 64);
 *   color/color2; alpha/alpha2; angleVel(-1°/帧); stop();`，**尺寸与位置全部由外部每帧写**：
 *   一手 `0x416ea0/0x416ea5: movss [vm+0x54]/[vm+0x58], 半径` +
 *   `0x416edd: movq [vm+0x614], 玩家坐标`。本卡照这套：**位置 = 场的圆心、scale = 场半径**。
 * ⇒ **判定 = 绘制**（同一个半径值）✓；半径的律见 `pofv_core.h`（TH19 的 ease-out 二次扩张）。
 * 场到寿命 ⇒ 发**事件 1**（脚本 `scaleTime(20,4,0,0)` 缩到 0 再自删），随后丢掉句柄。
 */
static void storm_vm_sync(void)
{
    int i, p;
    for (i = 0; i < POFV_STORM_MAX; ++i) {
        const pofv_storm_t *bl = pofv_storm_slot(&s_storm, i);
        float sc = 0.0f;
        if (bl && bl->active) pofv_storm_radius(bl, &sc);
        if (!bl || !bl->active || sc <= 0.0f) {
            for (p = 0; p < POFV_STORM_VM_PARTS; ++p) {
                if (!s_storm_vm[i][p]) continue;
                /* ★★ 场到期 ⇒ **发事件 1 让它淡出**（一手 `ghost.anm` script17 的收尾逐字是
                 *   `stop(); interruptLabel(1); alphaTime(10,0,0); alpha2Time(10,0,0); +10: delete();`
                 *   ⇒ 收到事件 1 后 **10 帧淡出**，然后**脚本自己 `delete()`**）。
                 *   ⚠ 旧版这里走的是 `ce_anm_delete`（硬删）—— 但场在 `pofv_storm_tick` 里
                 *   `active` 已被清掉 ⇒ 那一支**恰好**永远命中硬删 ⇒ **淡出根本没跑**。
                 *   ⇒ 现在统一发事件 1、**只丢句柄**；**硬删只留在 `clear_card()`**（删卡/重置）。 */
                ce_anm_interrupt((uint32_t)s_storm_vm[i][p], 1);
                s_storm_vm[i][p] = 0;
            }
            continue;
        }
        for (p = 0; p < POFV_STORM_VM_PARTS; ++p) {
            float k = sc;
            if (!s_storm_vm[i][p])
                s_storm_vm[i][p] = (int32_t)ce_anm_spawn(CE_ABILITY_ANM(),
                                     CE_ANM_ABILITY_SCRIPT_POFV109_STORM, POFV_STORM_VM_LAYER);
            if (!s_storm_vm[i][p]) continue;        /* anm 还没就绪：下一帧再补 */
            /* ★★ 一手（`HANDOFF-109-STORM.md` §2）：半径由**引擎每帧写 VM 的 scale（`vm+0x54`）**
             *   —— `FUN_004044b0(vm+0x1f8, 时长=ins_1031, mode=5, from=vm+0x54(=1.0), to=ins_1028)`。
             *   ⇒ 卡侧每帧写 **位置 + scale(= 当前半径)**；脚本自己 `stop()` 不再动 scale。
             *   ⚠ 判定半径与绘制半径**同源**（一手里 tick 后写进伤害源 `+0x18` 的就是同一个值）。 */
            vm_set_world_pos((uint32_t)s_storm_vm[i][p], bl->x, bl->y);
            ce_anm_set_scale((uint32_t)s_storm_vm[i][p], k, k);
        }
    }
}

/* ============================ C1：灵梦的 4 发扇形（❓触发自定） ============================
 * ★ **一手**（`HANDOFF-109-C1-C4.md` §3.7，来源 `pl00.sht` 表第 10 项 ✓）：灵梦 C1 = **4 发**：
 *     行0  (0,0)  dmg=7  ang=−2.0944(−120°)  spd=14
 *     行1  (0,0)  dmg=7  ang=−1.7453(−100°)  spd=14
 *     行2  (0,0)  dmg=7  ang=−1.3963(−80°)   spd=14
 *     行3  (0,0)  dmg=7  ang=−1.0472(−60°)   spd=14
 *   anm = `pl00.anm` script10（30×30 弹）；**只在触发那一帧发一次** ✓（一手 `rate=100`，
 *   而对象只活 21 帧 ⇒ 实际只出一轮 ✓）。★ 角度体系：**−π/2 = 正上** ✓（卡里主炮 `-1.5708` 同系 ✓）。
 *
 * ❓**触发自定**（不是复刻 ✗）：一手是「**蓄力 ≥ C1 阈值**」；但本卡的蓄力流程是自己的一套
 *   （`s_aura_charge` / `s_reiryoku` / `POFV_REI_C1`，见下方 `s_aura_size` 附近）⇒ 这里改用
 *   **结界张开的上升沿**（`pofv_aura_key()` 0→1 的那一帧）发**一轮**。
 *   理由：① 结界键 = 本卡里唯一"玩家主动按下"的边沿事件 ✓；② 不动 `s_aura_charge` 语义 ✓；
 *   ③ 上升沿天然"只发一次" ✓（与一手"只发一轮"同形 ✓）。
 *   影响：按 C/Shift **立刻**出弹，**不受**灵力/`POFV_REI_C1` 门控 ✗（一手要蓄到 500 才出）
 *   ⇒ 手感偏强；且 `POFV_REI_C1`（现 2500，一手 500）owner 未拍板 ⇒ 本轮**不动它** ✓。
 *   ⇒ 定案登记在 `TUNING.md` 的 C1 节。
 *
 * ❓**观感自定**：一手是 `pl00.anm` script10 的 30×30 弹；本卡**暂用现成脚本占位**
 *   （`CE_ANM_ABILITY_SCRIPT_POFV109_SPIRIT_BODY`），**不新切贴图、不动 `ORDER.txt`** ✗。
 * ❓**伤害源自定**：`ce_damage_rect` 的 `life/w/h` 一手没给 ⇒ 先用下面的常量（32×32 ≈ 一手 30×30 ✓）。
 */
typedef struct { float x, y; int dmg; float ang, spd; } pofv_c1_shot_t;

/* ★ 数据表：第 0 组 = **灵梦**（一手 ✓）；第 1/2 组 = **魔理沙 / 早苗：待补 ❓**（一手数值未取）。*/
#define POFV_C1_SHOTS_PER_GROUP 26   /* ★ 取三组里的最大者 = 早苗一手 26 行 ✓ */
#define POFV_C1_GROUPS          3
#define POFV_C1_MAX             (POFV_C1_SHOTS_PER_GROUP * POFV_C1_GROUPS)
#define POFV_C1_GROUP_REI       0        /* 灵梦 */
#define POFV_C1_GROUP_MARISA    1        /* 魔理沙：待补 ❓ */
#define POFV_C1_GROUP_SANA      2        /* 早苗 */
/* ★ 每组**实际发几发**（一手：灵梦 4 ✓ / 魔理沙 1 ✓ / 早苗 26 ✓）*/
static const int s_c1_rows[POFV_C1_GROUPS] = { 4, 1, 26 };
/* ★★ 2026-10-11 照一手（子 agent 核实 ✓）：★ **早苗 C2 不建对象** ✓ —— 她做的是
 *   「把弹表**切到 `set11`** ✓ + 开 **60 帧**窗口 ✓」（`EX+0xd8 = 11`、`EX+0xdc = 1` ✓）。
 *   ★ set11 = 26 行、**dmg 22 / 速度 3 / `class 5`（子机槽）/ anm 9** ✓、角度与 set10 相同
 *     （`−π + k·π/6` ✓）、`rate16` + 行 0–12 phase0 / 行 13–25 phase8 ✓。
 *   ❓`class 5` = 从**子机槽**出膛（一手 ✓）—— 本卡暂时仍从**自机坐标**出膛（❓自定，已登记 ✓）。*/
static const pofv_c1_shot_t s_c1s_set11[POFV_C1_SHOTS_PER_GROUP] = {
    { 0.0f, 0.0f, 22, -3.1416f, 3.0f }, { 0.0f, 0.0f, 22, -2.6180f, 3.0f },
    { 0.0f, 0.0f, 22, -2.0944f, 3.0f }, { 0.0f, 0.0f, 22, -1.5708f, 3.0f },
    { 0.0f, 0.0f, 22, -1.0472f, 3.0f }, { 0.0f, 0.0f, 22, -0.5236f, 3.0f },
    { 0.0f, 0.0f, 22,  0.0000f, 3.0f }, { 0.0f, 0.0f, 22,  0.5236f, 3.0f },
    { 0.0f, 0.0f, 22,  1.0472f, 3.0f }, { 0.0f, 0.0f, 22,  1.5708f, 3.0f },
    { 0.0f, 0.0f, 22,  2.0944f, 3.0f }, { 0.0f, 0.0f, 22,  2.6180f, 3.0f },
    { 0.0f, 0.0f, 22,  3.1416f, 3.0f }, { 0.0f, 0.0f, 22, -2.8798f, 3.0f },
    { 0.0f, 0.0f, 22, -2.3562f, 3.0f }, { 0.0f, 0.0f, 22, -1.8326f, 3.0f },
    { 0.0f, 0.0f, 22, -1.3090f, 3.0f }, { 0.0f, 0.0f, 22, -0.7854f, 3.0f },
    { 0.0f, 0.0f, 22, -0.2618f, 3.0f }, { 0.0f, 0.0f, 22,  0.2618f, 3.0f },
    { 0.0f, 0.0f, 22,  0.7854f, 3.0f }, { 0.0f, 0.0f, 22,  1.3090f, 3.0f },
    { 0.0f, 0.0f, 22,  1.8326f, 3.0f }, { 0.0f, 0.0f, 22,  2.3562f, 3.0f },
    { 0.0f, 0.0f, 22,  2.8798f, 3.0f }, { 0.0f, 0.0f, 22,  3.4034f, 3.0f }
};
/* ★ 当前用哪张表（0 = 各角色的 set10 ✓ / 1 = 早苗的 set11 ✓）＋ C2 早苗的 60 帧窗口 ✓ */
static int s_c1_set_sel;
static int s_c2s_win;

static const pofv_c1_shot_t s_c1_table[POFV_C1_GROUPS][POFV_C1_SHOTS_PER_GROUP] = {
    /* ---------------- 第 0 组：灵梦（一手 `pl00.sht` 第 10 项：**4 行** ✓）----------------
     *   伤害 7 / 速度 14 / 角度 −120° −100° −80° −60° / anm 10 / rate 100（⇒ 只发一次 ✓）。*/
    {
        { 0.0f, 0.0f, 7, -2.0944f, 14.0f },   /* -120° ✓ */
        { 0.0f, 0.0f, 7, -1.7453f, 14.0f },   /* -100° ✓ */
        { 0.0f, 0.0f, 7, -1.3963f, 14.0f },   /* -80° ✓ */
        { 0.0f, 0.0f, 7, -1.0472f, 14.0f },   /* -60° ✓ */
    },
    /* ---------------- 第 1 组：魔理沙（一手 `pl01.sht` 第 10 项：**1 行** ✓）----------------
     *   伤害 **10** / 速度 **0**（★ **定点**，不是飞出去 ✓）/ 角度 −90° / anm 8 / rate 60（⇒ 只发一次 ✓）。
     *   ★ 一手还有 `set11`（2 行、伤害 6、偏移 ∓32、速度 0）—— 本卡暂不用（C1 只用 set10 ✓）。*/
    {
        { 0.0f, 0.0f, 10, -1.5708f, 0.0f },   /* −90°，速度 0 = **定点** ✓ */
    },
    /* ---------------- 第 2 组：早苗（一手 `pl02.sht` 第 10 项：**26 行** ✓）----------------
     *   伤害 **22** / 速度 **16** / 角度 **−180° 起每 30°**（= 360° 整圈 ✓）/ anm 7 /
     *   ★ `rate = 16` ⇒ **每 16 帧放一圈**（螺旋 ✓）—— 本卡按"一次一轮"简化 ❓（见 TUNING）。*/
    {
        { 0.0f, 0.0f, 22, -3.1416f, 16.0f },   /* -180° ✓ */
        { 0.0f, 0.0f, 22, -2.6180f, 16.0f },   /* -150° ✓ */
        { 0.0f, 0.0f, 22, -2.0944f, 16.0f },   /* -120° ✓ */
        { 0.0f, 0.0f, 22, -1.5708f, 16.0f },   /* -90° ✓ */
        { 0.0f, 0.0f, 22, -1.0472f, 16.0f },   /* -60° ✓ */
        { 0.0f, 0.0f, 22, -0.5236f, 16.0f },   /* -30° ✓ */
        { 0.0f, 0.0f, 22, 0.0000f, 16.0f },   /* 0° ✓ */
        { 0.0f, 0.0f, 22, 0.5236f, 16.0f },   /* 30° ✓ */
        { 0.0f, 0.0f, 22, 1.0472f, 16.0f },   /* 60° ✓ */
        { 0.0f, 0.0f, 22, 1.5708f, 16.0f },   /* 90° ✓ */
        { 0.0f, 0.0f, 22, 2.0944f, 16.0f },   /* 120° ✓ */
        { 0.0f, 0.0f, 22, 2.6180f, 16.0f },   /* 150° ✓ */
        { 0.0f, 0.0f, 22, 3.1416f, 16.0f },   /* 180° ✓ */
        { 0.0f, 0.0f, 22, 3.6652f, 16.0f },   /* 210° ✓ */
        { 0.0f, 0.0f, 22, 4.1888f, 16.0f },   /* 240° ✓ */
        { 0.0f, 0.0f, 22, 4.7124f, 16.0f },   /* 270° ✓ */
        { 0.0f, 0.0f, 22, 5.2360f, 16.0f },   /* 300° ✓ */
        { 0.0f, 0.0f, 22, 5.7596f, 16.0f },   /* 330° ✓ */
        { 0.0f, 0.0f, 22, 6.2832f, 16.0f },   /* 360° ✓ */
        { 0.0f, 0.0f, 22, 6.8068f, 16.0f },   /* 390° ✓ */
        { 0.0f, 0.0f, 22, 7.3304f, 16.0f },   /* 420° ✓ */
        { 0.0f, 0.0f, 22, 7.8540f, 16.0f },   /* 450° ✓ */
        { 0.0f, 0.0f, 22, 8.3776f, 16.0f },   /* 480° ✓ */
        { 0.0f, 0.0f, 22, 8.9012f, 16.0f },   /* 510° ✓ */
        { 0.0f, 0.0f, 22, 9.4248f, 16.0f },   /* 540° ✓ */
        { 0.0f, 0.0f, 22, 9.9484f, 16.0f },   /* 570° ✓ */
    }
};

/* ❓自定常量（一手未给）：一发弹的**伤害源形状/寿命**。
 * **** 32×32 ≈ 一手 30×30 的弹体 ✓（`pl00.anm` script10）；`life` = 到出屏为止的帧数上限。
 * ⚠ 伤害源**每帧按当前位置重新发**（= 本卡暴风 `storm_hit_clipped` 的既有套路 ✓），
 *   故单帧 `life` 给 2（≈ 等效一个"跟着弹走"的源 ✓）；见 `pofv_c1_tick()`。*/
#define POFV_C1_HIT_LIFE   2
/* ★★ C1 的**两段运动**（一手 plNN.sht tick = `0x54b380`，灵梦 set10 的 `+0x4c = 8` ⇒ 表 `0x5a0400[8]`）：
 *   `cmp [ecx+0x1c], 0x78`（= **120** 帧）⇒
 *     · **age < 120**：`速度 -= [0x5aa228]`（= **0.95**）且夹在 `[0x5aa1f4]`（= **0.2**）⇒ 停在 0.2 不动 ✓
 *     · **age >= 120**：`速度 += [0x5aa1f4]`（= **0.2**）⇒ **重新出发** ✓
 *   ⇒ ★ 正是 owner 口述的「在发射点前一个距离**停下**、**停很长时间**，然后**再向前运动**」✓
 *   ★ 一手**没有撞墙反弹** ✗（`EX_BOUNCE` 是 **TH16** 的 ECL 概念，见 `engine/ecl/th16/ECL-info.md`）⇒ 本卡那条删掉 ✓。*/
#define POFV_C1_DECEL       0.95f   /* ★ 一手 0x5aa228 = 0.95 ✓ */
#define POFV_C1_HOLD_SPD    0.2f    /* ★ 一手 0x5aa1f4 = 0.2 ✓（停住的下限）*/
#define POFV_C1_HOLD_AT       120   /* ★ 一手 `cmp [ecx+0x1c], 0x78` = 120 帧 ✓ */
#define POFV_C1_REACCEL     0.2f    /* ★ 一手 同上那个 0.2 ✓（重新出发的加速度）*/
#define POFV_C1_HIT_W      30.0f   /* ★ owner Q4：= 一手弹体 30x30 ✓ */
#define POFV_C1_HIT_H      30.0f
#define POFV_C1_BULLET_LIFE 999          /* ★ 一手 ✓（`pl00.sht` 第 10 项的寿命 = 999）；
                                          *   ★ 实际由"出屏即收"先触发 ⇒ 这个值只是上限 ✓ */
#define POFV_C1_BOUNCE_MAX    4          /* ★ 一手 ✓ `EX_BOUNCE` 撞墙 4 次 */
#define POFV_C1_BOUNCE_MASK  13          /* ★ 一手 ✓ walls = 13 = 上(1)|左(4)|右(8) ⇒ **不弹下边** ✓ */
#define POFV_C1_VM_LAYER   6             /* ❓自定：与世界层对齐（暴风 `POFV_STORM_VM_LAYER` 同层 ✓）*/

/* C1 各发的飞行状态（VM 句柄 + 位置 + 剩余寿命）；`clear_card()` 里真删 ✓。*/

/* ============================ C2：霊撃（★ 本轮只做灵梦）============================
 * ★ 一手（`HANDOFF-109-C1-C4.md` §5；过程件 `engine/versus/th19/02-self-field-systems.md` §2.3，
 *   来源 TH19 v1.10c `EXReigekiInf` 槽 1 `0x53b920`（发动）+ 槽 2 `0x53b3a0`（每帧）✓）：
 *
 *   ① **通用层 `Ecl_EtBreakLv2`**（`_ext/ecl/default.txt` L269，**全角色同一份** ✓）：
 *      半径 **8 / 12 / 16 / 20**（每帧一档 +4，**共 4 帧** ✓）；中心 = **自机（`PlayerInf`）本体坐标** ✓
 *      （`0x4e79e0` 读 `[PlayerInf+0x68c]`；本作单场地**同样如此 ⇒ 直接照抄** ✓）；**不写侧别** ⇒ 留本侧 ✓。
 *
 *   ② **灵梦槽 4 `EXReigekiInf`**：总时长 **105 帧** ✓（一手 `+0x5c < +0x6c(45) + 0x3c(60)`）：
 *      · 帧 **0–44**：**3 个**环绕伤害点（角 **0°/120°/240°** ✓、整体 **+10°/帧** ✓、
 *        环绕半径 **0 → 64**，用 **22 帧**缓动）；每点一个**半径 64 的消弹圈** ✓
 *        （一手 `FUN_004ee770(&pos, 0x42800000=64.0f, 0, 99999, 0)`）+ **在该点撒道具** ❓；
 *        **每 5 帧**（`age % 5 == 0`）打一个伤害源（一手 `FUN_005200f0(pos, 0, 1, 4)`——
 *        第 6 参 `kind = 4` ✓，**伤害量 / 形状一手未给 ⇒ ❓自定**，见常量）；
 *      · 帧 **45–105**：`local_54 = (t-45)*0x40/0x3c + 128.0f` ⇒ **单个扩张环 128→192（60 帧线性）** ✓，
 *        继续消弹 / 撒道具 ✓；伤害参数一手 = **`0x18` = 24** ✓；
 *      · 释放瞬间给**本侧 40 帧无敌**（一手 `PlayerInf+0x2074/0x2078/0x207c = 39/40/40.0f` ✓）——
 *        本卡走 SDK 现成的**声明层** `ce_invuln_request()`（`sdk.h:1045`：逐帧声明剩余帧数）✓，
 *        **不自己写内存** ✗。
 *
 *   ⚠ **存疑 1（如实标 ❓）**：一手帧 45–105 那支的中心在函数入口被**清零** ⇒ 传给消弹的是
 *     **局部原点 (0,0,0)** 而非释放点 —— 本卡**按"自机坐标"实现**（与通用层同中心），此点待一手复核 ✓。
 *   ⚠ **存疑 2（❓自定）**：一手**没有可见的扩张环**（视觉 = 被消弹的火花）⇒ 本卡的画法是**自定**：
 *     借用本卡既有的**一手脚本 `396`**（`ghost.anm` script17 的逐字复刻 = 暴风圈同款）当占位 ✓
 *     （**不新切贴图、不动 `ORDER.txt`** ✗）。
 *   ⚠ **存疑 3（❓自定）**：一手「命中点撒道具」在单场地的对应物 ⇒ 映射到**本卡自建的灵力道具池**
 *     （`pofv_power_spawn_packed`），位置 = 消弹圈中心（不引新实体池 ✓）；面值 / 每帧上限 = 本仓既有值 ✓。
 *   ⚠ **存疑 4（❓自定）**：环绕半径那 22 帧的**缓动曲线**一手未给 ⇒ 取本卡结界同款的 **ease-out 二次** ✓。
 *
 *   ★ **触发**：沿用一手「蓄力档位」在本卡的既有替代 = **结界张开的上升沿**（与 C1 同一支 ✓）——
 *     落点在 `on_tick_2` 的 `pofv_aura_rise_edge()`（见下方函数 + 调用点注释）。**同一次张开 ⇒
 *     C1 与 C2 都放** ✓（C1 的 `pofv_c1_fire()` 由 owner 另写，挂同一个 `if (edge)` ✓）。
 *   ⚠ **不碰** `POFV_REI_C1` / `s_aura_charge` / `s_aura_size` / `s_reiryoku` 的语义 ✗（纯读键 ✓）。
 *
 *   ★ **三角函数**：`pofv.c` 不用 `cosf/sinf`（i686 上它们把 x87 带进目标文件 ⇒ 顶掉 `make dllx87`
 *     的「零 x87」不变式 ✗，同 `trance.c:1652` 的教训）⇒ 用**预计算的 36 向单位圆表**（10°/档，
 *     构建前用 Python `math.cos/sin` 生成一次、写死常数 ✓；做法同 `pofv_core.c:829`）。
 */
#define POFV_CARD_ID             109u   /* = `CE_CARD(109, …)`；无敌**声明层**的键 = 卡号 ✓ */
#define POFV_C2_TOTAL             105   /* 一手 ✓ 总时长（`+0x6c`=45 + `0x3c`=60）*/
#define POFV_C2_POINT_END          45   /* 一手 ✓ 3 点阶段结束 / 扩张环起点（= `+0x6c`）*/
#define POFV_C2_POINTS              3   /* 一手 ✓ 3 个环绕点（0°/120°/240°）*/
#define POFV_C2_SPIN_STEP           1   /* 一手 ✓ 整体 +10°/帧 = 36 向表每帧 +1 档 */
#define POFV_C2_POINT_BASE_STEP    12   /* 0°/120°/240° = 表中相隔 12 档（12×10°）*/
/* ★★ 2026-10-11 定案（我反汇编 `0x53b3a0` 的每帧算式 + 读常量 ✓）：
 *   那段用的是 **`×1000 ÷192`** 换算（常量 `0x5aa478 = 1000.0` ✓、`0x5aa3dc = 192.0` ✓，
 *   另有 π / −π / 2.0 / 3.0 ✓）⇒ ★ 子 agent 报的「**±100 × 1000/192 = 520.833**」**有出处** ✓
 *   —— ★ 即：★ 一手那边**真正的半径 = `100`** ✓（520.833 是**换算后的内部值** ✗）。
 *   ★ 我先前那个 `64.0f` 是 `+0x788` 的**另一个字段** ✗（不是环绕半径 ✗）。
 *   ★ 且一手是**恒定半径** ✓（不是从 0 缓动到满 ✗）⇒ 卡里改成常数 ✓。*/
#define POFV_C2_ORBIT_MAX       100.0f
#define POFV_C2_ORBIT_FRAMES     22.0f  /* 一手 ✓ 22 帧到终点（缓动曲线 ❓自定，见下）*/
#define POFV_C2_POINT_CANCEL_R   64.0f  /* 一手 ✓ 每点的消弹圈半径（`FUN_004ee770(…, 64.0f, …)`）*/
#define POFV_C2_RING_R0         128.0f  /* 一手 ✓ 扩张环起点 */
#define POFV_C2_RING_R1         192.0f  /* 一手 ✓ 终点（60 帧线性 ⇒ `+0x40`/`0x3c`）*/
#define POFV_C2_RING_FRAMES      60.0f  /* 一手 ✓ */
#define POFV_C2_RING_DMG           24   /* 一手 ✓（`0x18`）*/
#define POFV_C2_GEN_FRAMES          4   /* 一手 ✓ `Ecl_EtBreakLv2` 共 4 帧 */
#define POFV_C2_GEN_R0            8.0f  /* 一手 ✓ 半径序列 8/12/16/20 */
#define POFV_C2_GEN_STEP          4.0f  /* 一手 ✓ 每帧 +4 */
/* ★★ 一手的**逐档消弹环序列**（`Ecl_EtBreakLvN` ✓，§5.1）—— ★ 三档都有、只有 C1 没有 ✓：
 *   Lv2 = **8/12/16/20**（每帧 +4、共 4 帧 ✓，C2 已用 ✓）；
 *   Lv3 = **16/24/32**（每帧 +8、共 3 帧 ✓）；Lv4 = **16/24/32/40/48**（每帧 +8、共 5 帧 ✓）。
 * ★ 一手这三档做的事 = **消弹 + 撒道具**（**没有伤害** ✓）⇒ ★ 本卡正好用现成的 `c2_cancel_drop` ✓。
 * ★ 侧别：一手 Lv2 留本侧、**Lv3/Lv4 送对侧**（写 `1−side` ✓）—— ★ 本作是"我方场地打我方 E" ✓
 *   ⇒ ★ **都留在本侧** ✓（正是 owner 那两处改动之一 ✓）。*/
#define POFV_EB_LV3_R0     16.0f
#define POFV_EB_LV4_R0     16.0f
#define POFV_EB_STEP        8.0f   /* ★ 一手 ✓ 每帧 +8 */
#define POFV_EB_LV3_FRAMES    3    /* ★ 一手 ✓ 16/24/32 */
#define POFV_EB_LV4_FRAMES    5    /* ★ 一手 ✓ 16/24/32/40/48 */
#define POFV_C2_POINT_HIT_EVERY     5   /* 一手 ✓ 每 5 帧一个伤害源 */
#define POFV_C2_POINT_HIT_DMG       4   /* ❓自定：一手只给了 kind=4（`FUN_005200f0(…, 4)`）⇒ 伤害量先用 4 */
#define POFV_C2_POINT_HIT_R      16.0f  /* ❓自定：32×32 的矩形源（一手形状未给；≈ 环绕点体量）*/
#define POFV_C2_INVULN             40   /* 一手 ✓（39/40/40.0f）*/
#define POFV_C2_VM_LAYER            6   /* 同暴风 `POFV_STORM_VM_LAYER`（脚本 396 里自己也写 `layer(6)` ✓）*/
#define POFV_C2_RING_SCRIPT  CE_ANM_ABILITY_SCRIPT_POFV109_STORM   /* ★ 借一手脚本 396（❓自定画法）*/

/* 36 向单位圆表（10°/档，Python 生成；**不引 libm** ⇒ 守住 `dllx87` ✓）。*/
static const float s_c2_dir36[36][2] = {
    { +1.0000000f, +0.0000000f }, { +0.9848078f, +0.1736482f }, { +0.9396926f, +0.3420201f },   /*   0° ..  20° */
    { +0.8660254f, +0.5000000f }, { +0.7660444f, +0.6427876f }, { +0.6427876f, +0.7660444f },   /*  30° ..  50° */
    { +0.5000000f, +0.8660254f }, { +0.3420201f, +0.9396926f }, { +0.1736482f, +0.9848078f },   /*  60° ..  80° */
    { +0.0000000f, +1.0000000f }, { -0.1736482f, +0.9848078f }, { -0.3420201f, +0.9396926f },   /*  90° .. 110° */
    { -0.5000000f, +0.8660254f }, { -0.6427876f, +0.7660444f }, { -0.7660444f, +0.6427876f },   /* 120° .. 140° */
    { -0.8660254f, +0.5000000f }, { -0.9396926f, +0.3420201f }, { -0.9848078f, +0.1736482f },   /* 150° .. 170° */
    { -1.0000000f, +0.0000000f }, { -0.9848078f, -0.1736482f }, { -0.9396926f, -0.3420201f },   /* 180° .. 200° */
    { -0.8660254f, -0.5000000f }, { -0.7660444f, -0.6427876f }, { -0.6427876f, -0.7660444f },   /* 210° .. 230° */
    { -0.5000000f, -0.8660254f }, { -0.3420201f, -0.9396926f }, { -0.1736482f, -0.9848078f },   /* 240° .. 260° */
    { -0.0000000f, -1.0000000f }, { +0.1736482f, -0.9848078f }, { +0.3420201f, -0.9396926f },   /* 270° .. 290° */
    { +0.5000000f, -0.8660254f }, { +0.6427876f, -0.7660444f }, { +0.7660444f, -0.6427876f },   /* 300° .. 320° */
    { +0.8660254f, -0.5000000f }, { +0.9396926f, -0.3420201f }, { +0.9848078f, -0.1736482f },   /* 330° .. 350° */
};

typedef struct {
    int active;      /* 1 = C2 生效中 */
    int age;         /* 0..`POFV_C2_TOTAL`−1（一手 `EXReigekiInf+0x5c` ✓）*/
    int invuln;      /* 剩余无敌帧（一手 40 ✓；逐帧声明给 SDK 的声明层 ✓）*/
    int ring_vm;     /* ★★ 消弹**范围圈**的 VM（★ 一手正解 = 一台 VM + `texArcEven(64)`，见 §15 ✓）*/
    int32_t orb_vm[POFV_C2_POINTS];   /* ★★ C2 的 **3 台可见物** = 一手 `pl00.anm` script20 ✓（脚本 434 ✓）*/
} pofv_c2_t;
static pofv_c2_t s_c2;
/* ★ C1/C2 **共用**的「结界键张开上升沿」闩锁（一手两档同一次蓄力释放 ⇒ 同一次张开都放 ✓）。
 * ⚠ C1 的 `s_c1_prev`（上面）是上一版留下的**未使用**声明 —— 本函数落地后应改用它或删掉；
 *   本轮**不动**它 ✗（避免与 owner 正在写的 C1 撞车）。*/
static int s_aura_rise_prev;
static int     s_chg_se_prev;
static int     s_c1_rel_latch;   /* ★ 「每次释放放一份 C1」的闩锁 ✓（一手 ✓）*/    /* ★ 蓄力音（`0x2c`）的按下沿闩锁 ✓ */

/* ★★ ② 三行字 + LV（owner：「蓄力到 C2、C3、C4 时上面会有 Skill / Ex Attack Lv n / Boss Lv n」✓）
 * ★ 一手：阶段 3 的串 = "Ex.Attack Lv%d" ✓（05-four-mechanics.md）；阶段 4 = "Boss Lv%d" ✓
 *   （★ exe 原文 0x596de8 = Boss Lv%d ✓，我读到了）。
 * ★★ LV = 运行时计数（修正我先前"每局按难度常数" ✗）：S+0x38 = clamp(+1,0,7) ✓（阶段 3，0x4e8283）、
 *   S+0x3c = clamp(+1,0,7) ✓（阶段 4，0x4e7b d4，独立）⇒ 卡侧每放一次 +1、clamp 0..7 ✓。
 * ★ 画法：ce_hud_vm_text_set_font ✓（sdk.h:811）；坐标 = 640 空间 ✓；align_h 0 = 居中 ✓（sdk.h:774）。*/
static ce_hud_vm_text_t s_hud_chg_lv_text;
static int s_c3_lv_n, s_c4_lv_n;
/* ★★ 2026-10-11 补一手（子 agent 核实 ✓）：★ C3/C4 各有一个**使用计数** ✓
 *   （一手 `S+0x4c`（C3）/ `S+0x50`（C4），**clamp 0..999** ✓）—— 与 LV（0..7）**不是同一个量** ✗。*/
static int s_c3_uses, s_c4_uses;
static int s_chg_prev;   /* 2026-10-11：蓄力键的上一帧状态（用来判真正的松手沿）*/
/* ★★★ 2026-10-11 卡死定位**阶段仪表** ✓：★ 每段危险调用前 `pofv_stage_set(n)` 记一个号 ✓，
 *   ★ 只有**号变了**才打一行 ✓（不刷屏 ✓）。★ 卡死时**最后一个阶段号 = 凶手** ✓。*/
static int s_dbg_stage;
static int s_dbg_stage_last;
static void pofv_stage_set(int n) { s_dbg_stage = n; }
static void pofv_stage_flush(void)
{
    /* ★ 2026-10-11 收尾：★ 诊断仪表的**输出**已关 ✓（`pofv_stage_set` 的赋值保留 ✓ ——
     *   以后再排查时，把下面那行 `ce_log` 打开即可 ✓，不影响逻辑 ✓）。*/
    if (s_dbg_stage == s_dbg_stage_last) return;
    s_dbg_stage_last = s_dbg_stage;
    /* ce_log("pofv: [STAGE] %d", s_dbg_stage); */
}
/* ★★ 一手（子 agent 核实 ✓）：★ C2 的**扩张环圆心 = 第 1 个环绕点** ✗（不是自机 ✗）。
 *   ⇒ 卡侧把第 1 个点的坐标记下来，环 VM / 环的消弹都用它 ✓。*/
static float s_c2_pt0x, s_c2_pt0y;      /* ★ 阶段 3/4 等级（0..7 ✓，一手 +0x38/+0x3c ✓）*/

/* 结界张开的上升沿（0→1 的那一帧返回 1；电平无变化返回 0）。C1 / C2 共用 ✓。*/
static int pofv_aura_rise_edge(int held)
{
    const int edge = (held && !s_aura_rise_prev) ? 1 : 0;
    s_aura_rise_prev = held ? 1 : 0;
    return edge;
}

/* 圈内消弹 + 在圈心撒道具（一手「命中点撒道具」的**单场地对应物** ❓自定；面值 / 上限走本仓既有值 ✓）。
 * 返回消掉的颗数。位置 = 圈心（一手是逐个命中点，本卡不复刻逐点坐标 ⇒ 见 `TUNING.md` 的 ❓）。*/
static int c2_cancel_drop(const float *pos, float r)
{
    const int n = cancel_white_in_circle(pos, r, POFV_CANCEL_MAX, POFV_BULLET_CANCEL_MODE);
    if (n > 0) {
        int32_t pts = 0;
        (void)pofv_power_spawn_packed(&s_power, pos[0], pos[1], n, POFV_POWER_PTS_CANCEL,
                                      POFV_AURA_ITEMS_PER_FRAME, POFV_POWER_SRC_REIGEKI, &pts);
    }
    return n;
}

/* 可见扩张环（❓自定画法）：建 / 每帧跟随自机 + 写 scale(=半径)。脚本 396 的半径由外部每帧写 ✓。*/
/* ★★ 2026-10-10 owner 实跑订正：「**C1 C2 包括其他 C 的消弹圈是扩张的红色圆环**」✓
 *   ⇒ 各档的「圈」是**红色**的（旧记「一手无可视扩张环」✗ 作废）。卡里既有约定
 *   `0xffffff00` = 黄 ⇒ 同字节序下 **红 = `0xffff0000`**（AARRGGBB）。*/
#define POFV_C_RING_COLOR       0xffff0000u
/* ★★ 贴图版红环（脚本 430 / `POFV109_C_RING`）：贴图里环心在 **r = 0.86 × 64 = 55.04 px**
 *   ⇒ 卡侧要显示半径 `r`，`scale = r / 55.04` ✓（脚本 430 不管尺寸，scale 由外部每帧写 ✓）。*/
#define POFV_C_RING_TEX_R       55.04f
#define POFV_C_RING_SCRIPT      CE_ANM_ABILITY_SCRIPT_POFV109_C_RING

/* ★★ 2026-10-10 owner 实跑：「**消弹圈怎么着都不对**」✗ + 核一手后确认 ——
 *   ★ 一手 `EXReigekiInf` 的消弹**没有**这样一个大圆环 ✗：它的可见物是
 *   ① **3 个绕行的点**（一手 3 × 类型 `0x14` 的对象 ✓）、② **每消一颗弹一朵 `etbreak` 火花** ✓
 *   （脚本 431 ✓，已在 `cancel_white_in_circle` 里逐颗放 ✓）。
 *   ⇒ ★ 那两个函数（`c2_ring_vm_sync` / `c2_ring_vm_release`）与 `s_c2.ring_vm` 字段**全部删掉** ✓。
 *   ★ 自烤的红环贴图/脚本（`POFV109_C_RING` / 脚本 430）**保留在库里但不再有人引用** ❓
 *   —— C3/C4 的弹当前还借它当占位观感 ✓（那两档的观感是否要换，等 owner 说 ✓）。*/


/* 释放（结界张开的上升沿那一帧）：复位状态 + 本侧 40 帧无敌（一手 ✓）。环在帧 45 才起 ⇒ 此处不建 VM ✓。*/
static void pofv_c2_fire(void)
{
    s_c2.active = 1;
    s_c2.age = 0;
    s_c2.invuln = POFV_C2_INVULN;
    /* ⚠ 若上一次的环还在淡出（句柄已在 `pofv_c2_*` 收尾时丢掉）⇒ 这里自然新建一台 ✓。*/
}

/* 每帧推进（105 帧）。`id` = 本卡卡号（无敌声明层的键 ✓）。**不依赖结界键电平** ✓（一次性 105 帧）。*/
static void c2_ring_vm_sync(float r);   /* 定义在后（装回范围圈时加的 ✓）*/
static void c2_ring_vm_release(void);   /* 定义在后（装回范围圈时加的 ✓）*/

static void pofv_c2_tick(uint32_t id)
{
    pofv_stage_set(18);   /* ★ 灵梦 C2 tick ✓ */
    const float *p;

    if (!s_c2.active) return;
    p = ce_player_pos();
    if (!p) return;

    /* ① 通用层 `Ecl_EtBreakLv2`（一手 ✓）：4 帧、半径 8/12/16/20、中心 = 自机 ✓ */
    if (s_c2.age < POFV_C2_GEN_FRAMES) {
        float pos[3];
        pos[0] = p[0]; pos[1] = p[1]; pos[2] = 0.0f;
        (void)c2_cancel_drop(pos, POFV_C2_GEN_R0 + POFV_C2_GEN_STEP * (float)s_c2.age);
    }

    if (s_c2.age < POFV_C2_POINT_END) {
        /* ② 灵梦槽 4 · 帧 0–44：3 个环绕点（0°/120°/240°、整体 +10°/帧、环绕半径 0→64 / 22 帧）*/
        int i;
        float f = (float)s_c2.age / POFV_C2_ORBIT_FRAMES;
        float orbit;
        if (f > 1.0f) f = 1.0f;
        /* ❓缓动曲线：一手未给 ⇒ 取本卡结界同款的 ease-out 二次（模式 4 同族 ✓）*/
        (void)f;
        orbit = POFV_C2_ORBIT_MAX;   /* ★ 一手 ✓ **恒定**半径（原来是 0→满 缓动 ✗）*/
        for (i = 0; i < POFV_C2_POINTS; ++i) {
            const int di = (POFV_C2_POINT_BASE_STEP * i + POFV_C2_SPIN_STEP * s_c2.age) % 36;
            float pos[3];
            pos[0] = p[0] + orbit * s_c2_dir36[di][0];
            pos[1] = p[1] + orbit * s_c2_dir36[di][1];
            pos[2] = 0.0f;
            (void)c2_cancel_drop(pos, POFV_C2_POINT_CANCEL_R);      /* 半径 64 的消弹圈 ✓ */
            /* ★★ 可见物（一手 ✓）：★ 每点一台 `pl00.anm` script20 的复刻（脚本 434 ✓）——
             *   一手 tick 里这 3 台就是**跟着三个环绕点跑** ✓（`+10°/帧` ✓、半径见下 ✓）。*/
            const int di_ang = di;
            if (i == 0) { s_c2_pt0x = pos[0]; s_c2_pt0y = pos[1]; }   /* ★ 环心用它 ✓ */
            if (!s_c2.orb_vm[i])
                s_c2.orb_vm[i] = (int32_t)ce_anm_spawn(CE_ABILITY_ANM(),
                                  CE_ANM_ABILITY_SCRIPT_POFV109_C2_ORB, 6);
            if (s_c2.orb_vm[i]) {
                ce_anm_set_pos((uint32_t)s_c2.orb_vm[i], pos[0], pos[1], 0.0f);
                ce_anm_set_scale((uint32_t)s_c2.orb_vm[i], 1.0f, 1.0f);   /* ★ 一手 126×126 原尺寸 ✓ */
                /* ★★ 2026-10-11 owner 实跑：「它**得转**」✗ ⇒ ★ 这 3 台要**跟着环绕角自转** ✓
                 *   （一手 tick 里那 3 点的角就是 θ / θ+120 / θ+240 ✓，整体 +10°/帧 ✓）。*/
                ce_anm_set_rotation((uint32_t)s_c2.orb_vm[i], 0.0f, 0.0f,
                                    (float)di_ang * (6.2831853f / 36.0f));
            }
            if (s_c2.age % POFV_C2_POINT_HIT_EVERY == 0)            /* 每 5 帧一个伤害源 ✓ */
                (void)storm_hit_clipped(pos[0], pos[1], POFV_C2_POINT_HIT_R, POFV_C2_POINT_HIT_DMG);
        }
    } else {
        /* ③ 帧 45–105：单个扩张环 128→192（60 帧线性 ✓）：消弹 + 撒道具 + 覆盖圈的伤害源（dmg 24 ✓）
         *   ⚠ 中心按「自机坐标」实现（一手该支传局部原点 (0,0,0) ⇒ **语义存疑**，见块注释 ⚠存疑 1）*/
        const float r = POFV_C2_RING_R0
                      + (POFV_C2_RING_R1 - POFV_C2_RING_R0)
                        * (float)(s_c2.age - POFV_C2_POINT_END) / POFV_C2_RING_FRAMES;
        /* ★★ 一手 ✓：★ **age == 45**（= `POFV_C2_POINT_END`）时把 3 台可见物 `anm 中断 1` ✓
         *   ⇒ ★ 触发 434 的 `interruptLabel(1)` 分支：**喷 24/26/28 各 2 台 + 白色爆发 22** ✓
         *   = owner 说的「**然后"炸"**，还得有光效」✓（原来我**从没打断过**它们 ✗）。*/
        if (s_c2.age == POFV_C2_POINT_END) {
            int k4;
            for (k4 = 0; k4 < POFV_C2_POINTS; ++k4)
                if (s_c2.orb_vm[k4]) ce_anm_interrupt((uint32_t)s_c2.orb_vm[k4], 1);
        }
        c2_ring_vm_sync(r);   /* ★ 圆心 = 第 1 个环绕点 ✓（一手 ✓）*/   /* ★★ 范围圈：每帧按**真实消弹半径**写 `scale.y`（判定=绘制 ✓）*/
        float pos[3];
        pos[0] = s_c2_pt0x; pos[1] = s_c2_pt0y; pos[2] = 0.0f;   /* ★ 一手：环心 = 第 1 个环绕点 ✓（原为自机 ✗）*/
        (void)c2_cancel_drop(pos, r);
        (void)(void)storm_hit_clipped(s_c2_pt0x, s_c2_pt0y, r, POFV_C2_RING_DMG);   /* ★ 一手：环心 ✓ */
    }

    /* ④ 无敌（一手 40 帧 ✓）：**逐帧**声明剩余帧数（SDK 在帧界取最大值落笔 ✓，`sdk.h:1045`）*/
    ce_invuln_request(id, (s_c2.invuln > 0) ? s_c2.invuln : 0);
    if (s_c2.invuln > 0) --s_c2.invuln;

    /* ⑤ 收尾（105 帧到 ⇒ 环淡出、无敌撤声明、状态归零 ✓）*/
    if (++s_c2.age >= POFV_C2_TOTAL) {
        c2_ring_vm_release();  /* ★ 范围圈：随 C2 一起收（淡出后由 clear 兜底 ✓）*/
        {   int k2;                                     /* ★ 3 台可见物：打断 ⇒ 一手 +60 自删 ✓ */
            for (k2 = 0; k2 < POFV_C2_POINTS; ++k2)
                if (s_c2.orb_vm[k2]) { ce_anm_interrupt((uint32_t)s_c2.orb_vm[k2], 1); s_c2.orb_vm[k2] = 0; }
        }
        ce_invuln_request(id, 0);
        s_c2.active = 0;
        s_c2.age = 0;
        s_c2.invuln = 0;
    }
}

/* 清干净（`clear_card()` = `on_ctor` / `on_run_reset` / `on_delete` 三路共用；铁律「长持续效果必须
 * 收干净」✓ —— 本函数的跨帧遗留物 = ① 可见环 VM ② 无敌**声明层**的一格 ③ 上升沿闩锁 ✓）。*/
/* ================= ③ 消弹**范围圈**（标注消弹半径的那个环）=================
 * ★★ 一手正解（§15 ✓）：★ **一台 VM** 把一条 **16×768 的红竖条**（TH09 `etama3.png` 第 3 列 ✓）
 *   **扫成一段弧** ✓ —— ★ TH18 的对应物 = **`texArcEven(段数)`** ✓（anmm 601 ✓，脚本 432 ✓），
 *   ★ 段数 = **64** ✓（一手 `iset([10000],64)` ⇒ 落在 `vm+0x100` = 引擎读的段数 ✓）。
 * ★ 本卡：★ **半径由卡侧每帧写 `scale.y`** ✓（= 该档**真实的消弹半径** ⇒ **判定=绘制** ✓，
 *   与暴风/C2 的既有做法同款 ✓）；★ `scale.x` = 环带宽度（❓旋钮，一手是 16 px 的条 ✓）。
 * ⚠ 上一轮我误以为"一手没有这个圈"而把它整个删了 ✗ —— 那位 owner 说的**正是它** ✓（已装回 ✓）。*/
#define POFV_C2_RING_BAND_W   1.0f     /* ❓旋钮：环带宽度（× 贴图宽 16 px）*/
static void c2_ring_vm_sync(float r)
{
    const float *p = ce_player_pos();
    (void)p;
    if (r <= 0.0f) return;
    if (!s_c2.ring_vm) {
        const uint32_t id = ce_anm_spawn(CE_ABILITY_ANM(),
                            CE_ANM_ABILITY_SCRIPT_POFV109_C_RING_ARC, 6);
        if (!id) return;
        s_c2.ring_vm = (int32_t)id;
    }
    /* ★★ 圆心 = **第 1 个环绕点** ✓（一手 ✓；`s_c2_pt0x/y` 由 tick 每帧写 ✓）*/
    vm_set_world_pos((uint32_t)s_c2.ring_vm, s_c2_pt0x, s_c2_pt0y);
    /* ★ 判定=绘制：`scale.y` = 半径（世界单位）；`scale.x` = 环带宽度 ✓ */
    ce_anm_set_scale((uint32_t)s_c2.ring_vm, POFV_C2_RING_BAND_W, r);
}

static void c2_ring_vm_release(void)
{
    if (!s_c2.ring_vm) return;
    if (ce_anm_get_vm((uint32_t)s_c2.ring_vm)) ce_anm_interrupt((uint32_t)s_c2.ring_vm, 1);
    s_c2.ring_vm = 0;
}

/* ★ 消弹**范围圈**：起 / 驱动 / 收（**一台 VM** ✓，脚本 432 = `texArcEven(64)` ✓，§15 ✓）。
 *   `r` = 本帧的**消弹半径** ⇒ `scale.y` 照写（**判定=绘制** ✓）。★ C2 用它自己的（见上 ✓），
 *   C3/C4 用这两个 ✓。*/
static void pofv_eb_ring_sync(int32_t *vm, float r)
{
    const float *p = ce_player_pos();
    if (!vm || r <= 0.0f) return;
    if (!*vm) {
        const uint32_t id = ce_anm_spawn(CE_ABILITY_ANM(),
                            CE_ANM_ABILITY_SCRIPT_POFV109_C_RING_ARC, 6);
        if (!id) return;
        *vm = (int32_t)id;
    }
    if (!p) return;
    vm_set_world_pos((uint32_t)*vm, p[0], p[1]);
    ce_anm_set_scale((uint32_t)*vm, POFV_C2_RING_BAND_W, r);
}

static void pofv_eb_ring_close(int32_t *vm)
{
    if (!vm || !*vm) return;
    if (ce_anm_get_vm((uint32_t)*vm)) ce_anm_interrupt((uint32_t)*vm, 1);
    *vm = 0;
}

static void pofv_c2_clear(void)
{
    if (s_c2.ring_vm) {                                       /* ★ 铁律：范围圈 VM 也收 ✓ */
        if (ce_anm_get_vm((uint32_t)s_c2.ring_vm)) ce_anm_delete((uint32_t)s_c2.ring_vm);
        s_c2.ring_vm = 0;
    }
    s_c2.active = 0; s_c2.age = 0; s_c2.invuln = 0;
    s_aura_rise_prev = 0;                 /* 删卡后不留幽灵闩锁 ⇒ 下次张开仍算上升沿 ✓ */
    ce_invuln_request(POFV_CARD_ID, 0);   /* 撤无敌声明（声明层是**粘性**的 ⇒ 必须显式撤 ✓）*/
}

/* ============================ TH19 連撃账：+1 / 窗口 / 归零 ============================
 * 一手逐条（地址见 `pofv_core.h` 的「TH19『連撃（combo）』账」段，TH19 v1.10c）：
 *   · **+1** = `FUN_00549780` 的 `0x54980f: inc dword ptr [ecx+0x44]` ✓
 *     —— 调用点只有两处：`0x50c450`（击破，实参 = `Enemy+0x6350` = ECL `ins_1032` = 20 ✓）、
 *        `0x51cb91`（暴风消一颗小弹，实参 = 0 ✓）⇒ 本函数用 `(n, win_add)` 覆盖这两支 ✓。
 *   · **窗口** = `+0x227C`：`+= win_add`、**上限 60**（`0x5497fb` ✓）。
 *   ⇒ `n` = 本事件加几（击破 = 1；暴风 = 本帧消掉的颗数 ✓）。*/
static void combo_th19_add(int32_t n, int32_t win_add)
{
    if (n <= 0) return;
    s_combo_th19.combo += n;                                   /* 一手：`0x54980f` 每事件 +1 → 这里按事件颗数一次加 ✓ */
    /* 一手 ✓（`0x521c6a`–`0x521c7c`）：单调计数器累加，然后 `while(>=100)` 消耗并掉道具 ✓ */
    s_combo_acc += n;
    while (s_combo_acc >= 100) {
        s_combo_acc -= 100;                                    /* 一手 `0x521d3c: sub 100` ✓ */
        {
            float bp[3];
            /* ★ 一手 ✓：落在**该次破坏的位置**（卡里 `s_combo_last_x/y` 由击破回调记 ✓）；
             *   ★ 没有记录（例如开局就满 100）时退回自机位置 ✓。*/
            if (s_combo_last_x != 0.0f || s_combo_last_y != 0.0f) {
                bp[0] = s_combo_last_x; bp[1] = s_combo_last_y; bp[2] = 0.0f;
            } else {
                const float *pp9 = ce_player_pos();
                if (!pp9) break;
                bp[0] = pp9[0]; bp[1] = pp9[1]; bp[2] = 0.0f;
            }
            /* ★★★ 2026-10-11 owner 订正 ✓：★ 不要再走引擎的 `kind 9` ✗（那是 TH19 引擎内部的编号 ✓），
             *   ★ 咱们这边灵力道具是**卡侧实现的人物道具** ✓ ⇒ ★ 掉**卡侧那一类蓝点** ✓
             *   （`pofv_power_spawn_kind = 1` 后调 `pofv_power_spawn` ✓；入账由上面的
             *    `absorbed_rei` 增量统一负责 ✓，这里**不再**直接进灵力 ✓）。*/
            {
                const int32_t k_save = pofv_power_spawn_kind;
                pofv_power_spawn_kind = 1;                      /* ★ 灵力道具（蓝点）✓ */
                (void)pofv_power_spawn(&s_power, bp[0], bp[1], 0, 0);
                pofv_power_spawn_kind = k_save;
            }            /* ★ 一手 ✓ kind = 9 = 加槽灵力道具 */
            ce_log("pofv: [COMBO100] 满 100 掉 kind9（剩余 acc=%d）", (int)s_combo_acc);
        }
    }
    if (win_add > 0) {
        s_combo_th19.win += win_add;                           /* 一手：`0x5497ca: addss [ecx+8], xmm1` ✓ */
        if (s_combo_th19.win > POFV_TH19_COMBO_WINDOW_MAX)     /* 一手：`0x5497db: cmp eax,0x3c` + `0x5497fb` ✓ */
            s_combo_th19.win = POFV_TH19_COMBO_WINDOW_MAX;
    }
}

/* 每帧推进窗口（一手 `FUN_005450e0` 的 `0x5459d8`–`0x5459ee`）：**逐帧 −1**；
 * **穿 0 的那一帧才归零 combo**（一手 `0x545a2e: mov dword [ebx+0x44],0` ✓）。
 * ⚠ 一手的 quirk **照抄** ✓：窗口**已经 ≤0** 时入口 `0x545962: jg` 早退 ⇒ **不动 combo** ✗
 *   ⇒ 所以这里第一行就 `return`（不是 `combo = 0`）✓。*/
static void combo_th19_tick(void)
{
    if (s_combo_th19.win <= 0) return;                          /* ★ 一手 quirk（`0x545962`）*/
    --s_combo_th19.win;
    if (s_combo_th19.win > 0) return;
    /* 一手归零前还有一条阈值支（`0x5459f5: cmp [ebx+0x44],0x14`）—— 它给「该侧 struct 的
     * `+0x2C` 对象」加一次计数，本卡**没有那个系统** ⇒ 只把口径记进日志、不消费 ✓（`TUNING.md` ❓）。*/
    if (s_combo_th19.combo >= POFV_TH19_COMBO_BONUS_AT)
        ce_log("pofv: TH19 連撃窗口漏空：combo %d（≥ %d，一手 0x5459f5 的阈值支；本卡无消费者 ⇒ 只记 ✓）",
               (int)s_combo_th19.combo, (int)POFV_TH19_COMBO_BONUS_AT);
    else if (s_combo_th19.combo > 0)
        ce_log("pofv: TH19 連撃窗口漏空 ⇒ combo %d → 0（一手 0x545a2e ✓）", (int)s_combo_th19.combo);
    s_combo_th19.combo = 0;
}

/* ============================ 每帧：一个场 ============================
 * 一手（TH09 节点律）：场**停在原地**、每帧半径 `r += dr`、到寿命释放。
 *   · **消弹**：半径内**本仓可消弹**（半径 ≤ 4.0，见 `pofv_core.h`）逐颗 `Bullet__cancel`；
 *   · **伤害**：覆盖圈面的矩形伤害源，**先裁到场地内**（否则场心靠边会把还没进屏的敌人打死）。
 * ❓ 伤害量：一手 TH09 没逆到；这里取「每帧 1 点、活 1 帧」的占位（登记 `TUNING.md`）。 */
static void do_storm_frame(const pofv_storm_t *bl)
{
    float pos[3], r;
    int n;

    if (!bl || !bl->active) return;
    pofv_storm_radius(bl, &r);
    if (r <= 0.0f) return;
    pos[0] = bl->x; pos[1] = bl->y; pos[2] = 0.0f;

    n = cancel_white_in_circle(pos, r, POFV_CANCEL_MAX, POFV_BULLET_CANCEL_MODE);
    /* ★★★★ 2026-10-10 TH19 連撃：暴风**每消掉一颗小弹 ⇒ combo +1**（一手 `0x51cb91`：
     *   `FUN_0051c8c0`（暴风 tick）的消弹循环里调 `FUN_00549780(实参 0)` ✓ —— 实参 0 ⇒ **窗口不加** ✓）。
     *   这是 combo 的**第二个**（也是唯一另一个）来源 ✓；一手对「除霊オーラ（结界）消弹」**不加** ✗
     *   （那条走 `FUN_004ee770`，不在那两个调用点里 ✓）⇒ 本卡结界碰弹（`aura_scan_bullets`）**也不加** ✓。*/
    if (n > 0) combo_th19_add(n, POFV_TH19_COMBO_CANCEL_WIN);
    /* ★ 伤害：每帧每场一个矩形源（判定 = 绘制同半径）。日志只在「出生帧」与「起不来」时打，避免刷屏。 */
    {
        int idx = storm_hit_clipped(bl->x, bl->y, r, bl->dmg);
        if (idx > 0) { if (bl->age <= 1) ce_log("pofv: dmgsrc slot %d age %d r %d dmg %d size %.0fx%.0f",
                                                 idx, (int)bl->age, (int)r, (int)bl->dmg, r * 2.0f, r * 2.0f); }
        else if (bl->age <= 1) ce_log("pofv: dmgsrc FAILED age %d r %d (r<=0 或整场在屏外)", (int)bl->age, (int)r);
    }
    if (n > 0) {   /* 补括号：消除 -Wmisleading-indentation，语义不变 */
        ce_log("pofv: storm age %d/%d r %d cancelled %d", (int)bl->age, (int)bl->life, (int)r, n);
    }
        /* ★★★ 一手（owner 已核 · `engine/versus/th09/02-ghost-and-seal.md` §2.3）：
         *   **幽灵是从「暴风/连爆」里产生的** —— `FUN_0041dff0` 里爆风节点 `state ∈ {2,3}` + 命中
         *   （且 `enemy+0x10c0 == 0`）⇒ 送出 **`enemy.ecl Sub0` = 绿幽灵、level 1**（`+0xa4=0, +0xa6=1`）。
         *   本卡映射：**暴风的「后半段」**（≈ state 2/3）那一帧若**消到了弹**（= 命中）⇒ 送一只 TH09 幽灵。
         *   ❓ 只取那一帧（`age == life/2`）⇒ 一场暴风最多送一只；位置取暴风位置。 */
        if (n > 0 && bl->life > 1 && (int)bl->age * 2 >= (int)bl->life && --s_ghost_cool <= 0) {
            s_ghost_cool = POFV_GHOST_SEND_COOL;
            if (s_spirit_q_n < POFV_SPIRIT_Q_MAX) {
                s_spirit_q[s_spirit_q_n][0] = bl->x;
                s_spirit_q[s_spirit_q_n][1] = bl->y;
                /* ★★★ owner 铁律：「**我方击破 ⇒ 生成到对面模型**」✓ ⇒ 暴风命中（我方事件）⇒ 进**模型** ✗本侧 */
                pofv_foe_spawn_gated(pofv_top_x(), pofv_top_y(),
                                      POFV_SPIRIT_KIND_GHOST_TH09, 1);
                ce_log("pofv: 暴风命中 ⇒ 送出 TH09 幽灵 **只进对面模型**（§2.3；level 1 绿）✗无回程");
            }
        }
}

/* ============================================================ 灵力道具的**可视 VM**
 * ★★★★ 2026-10-09（owner 明确否掉「借 116 季节道具美术」✗）—— 美术身份换正为**一手 TH19**：
 *   · 本体 = 一手 `bullet.anm` **script123**（`local/th19.v1.10c/_ext/bullet.txt`）⇒ 本仓新脚本 **423**；
 *   · 闪光 = 一手 `effect.anm` **script94**（同目录 `effect.txt`）⇒ 本仓新脚本 **424**。
 *   接线形状照 **116 季节道具**的做法（`season.c` 的 `point_spawn_vm()` `:594` / `points_tick()`）：
 *   `ce_anm_spawn(CE_ABILITY_ANM(), <脚本>, <层>)` → `ce_anm_set_pos` / `set_sprite` / `set_color`
 *   → 每帧 `ce_anm_set_pos` 跟随 → 移除时 `ce_anm_delete(id)` 并把句柄清 0。
 *   新增的脚本 / 贴图（`assets/ability/scripts/423_*` / `424_*` + ORDER 末尾 `TH19_ITEM_KIND*` 三行）：
 *     · `POFV_POWER_VM_SCRIPT`    = 423（kind9 外观：半透明蓝紫小点、自转、0.5↔0.55 脉冲）；
 *     · `POFV_POWER_VM_SPRITE`    = `TH19_ITEM_KIND9` = 一手 `bullet.anm` 的 **`sprite517`**
 *       （矩形 **`(209,33,14,14)`**，从 TH19 `anm/bullet/bullet/item.png` 原样切、构建期现取）；
 *     · `POFV_POWER_FLASH_SCRIPT` = 424（一手 script94 的**逐字**移植：加算空心 12 边形环、20 帧自删）；
 *     · 层   = `POFV_POWER_VM_LAYER`（= **13**，照 116 `season.c:118` 的 `SEASON_POINT_LAYER`：
 *             世界层 13 = 相机 3（游玩区），与原生道具同层）—— 闪光脚本自己写一手原值 `layer(16)`。
 *   ★ 为什么是 **kind 9**：一手「拾取加槽」只发生在 kind 9–14（kind 1–8 拾取只播 SE + 删除）
 *     ⇒ 本卡道具该用 kind 9 的外观（负结论登记 `TUNING.md`）。
 *   ⚠ 句柄：本体存在 `pofv_core.h` 的 `pofv_power_t.vm`；闪光按**道具槽号 1:1** 存在下面的
 *     `s_power_flash[]`（一个槽同时只放一颗道具 ⇒ 同时至多一台闪光）+ `clear_card()` 里真删。*/
#define POFV_POWER_VM_SCRIPT      CE_ANM_ABILITY_SCRIPT_POFV109_POWER_ITEM
#define POFV_POWER_VM_SPRITE      CE_ANM_ABILITY_SPRITE_TH19_ITEM_KIND9
#define POFV_POWER_FLASH_SCRIPT   CE_ANM_ABILITY_SCRIPT_POFV109_POWER_FLASH
#define POFV_POWER_VM_LAYER       13

/* ★ 闪光句柄表（**按道具槽号 1:1**：槽 i 的道具出生时把闪光 id 写进 `s_power_flash[i]`）。
 *   一手闪光脚本自带 `delete()`（20 帧）⇒ 句柄多半**已经死了** ⇒ 用前一律 `ce_anm_get_vm()` 探活，
 *   只对**还活着**的调 `ce_anm_delete()`（`sdk.h:240`；先例 `sdk.c:728` —— id 会被引擎回收，
 *   拿陈旧 id 去删 = **误删别人的 VM** ✗）。*/
static int32_t s_power_flash[POFV_POWER_MAX];

/* 内部：删一台道具 VM 并把句柄清 0 —— **唯一**的删法（所有移除路径 / `clear_card()` 共用）。*/
static void power_vm_drop(pofv_power_t *it)
{
    if (it && it->vm) { ce_anm_delete((uint32_t)it->vm); it->vm = 0; }
}

/* 内部：**道具真正出现那一刻**的几何闪光（一手 `effect.anm` script94 ⇒ 新脚本 424 一台）。
 *   位置 = 道具出生点（与本体 VM 同点）；脚本 20 帧后自删 ⇒ 这里**只记句柄、不删**。
 *   同槽若还有上一颗的闪光活着（不足 20 帧）⇒ 先顶掉（一手一个生成点一台环）。*/
static void power_flash_fire(int slot, float x, float y)
{
    uint32_t id;
    if (slot < 0 || slot >= POFV_POWER_MAX) return;
    if (s_power_flash[slot]) {
        if (ce_anm_get_vm((uint32_t)s_power_flash[slot])) ce_anm_delete((uint32_t)s_power_flash[slot]);
        s_power_flash[slot] = 0;
    }
    id = ce_anm_spawn(CE_ABILITY_ANM(), POFV_POWER_FLASH_SCRIPT, POFV_POWER_VM_LAYER);
    if (!id) return;                     /* 建失败 ⇒ 少一台装饰闪光，逻辑照走（不丢道具）*/
    ce_anm_set_pos(id, x, y, 0.0f);
    s_power_flash[slot] = (int32_t)id;
}

/* ★ 每帧同步（在 `pofv_power_tick()` **之后**调；116 的 `points_tick()` 同形）：
 *   · **建**：`delay` 减到 0（= 待发射延时结束、真正开始飞散那一刻）才建 —— 与 116
 *     「延时结束才建 VM」同口径（`season.c:590-604` 的注释 + `:826` 的 `if (!point_spawn_vm(...))`）；
 *     **同一刻**也点一台几何闪光（一手「物品出现时起 ring」的口径；比 spawn 成功那一帧更贴一手）；
 *   · **跟随**：`ce_anm_set_pos(vm, x, y, 0)`（116 `season.c:949/:951`）；
 *   · **删**：core 的移除路径（**收取 / 出界 / 寿命到期**）都只是把 `active` 清 0 ⇒
 *     这里统一按「`active == 0` 而句柄还在 ⇒ 删 + 清 0」收干净（铁律：不许留幽灵 VM）。
 *   ⚠ 道具脚本（423）也**不自己 `delete()`**（一手 script123 是 `static()` 常驻）⇒ 收/出界一律由我们删；
 *     闪光脚本（424）则**自己 delete**（一手如此）⇒ 它的句柄只探活、不在这里删。*/
static void power_vm_sync(void)
{
    int i;
    for (i = 0; i < POFV_POWER_MAX; ++i) {
        pofv_power_t *it = &s_power.s[i];
        if (!it->active) { power_vm_drop(it); continue; }
        if (!it->vm && it->delay <= 0) {
            uint32_t id = ce_anm_spawn(CE_ABILITY_ANM(), POFV_POWER_VM_SCRIPT, POFV_POWER_VM_LAYER);
            if (!id) continue;          /* 建失败 ⇒ 逻辑照走（下一帧再试），不丢道具 */
            ce_anm_set_pos(id, it->x, it->y, 0.0f);
            ce_anm_set_sprite(CE_ABILITY_ANM(), id, POFV_POWER_VM_SPRITE);
            /* ★★★★ 2026-10-09 **修 bug**（owner：「alpha 上限是 255=不透明；我看着咱们的可**不是 alpha 96**
             *   这个透明度」✓ ⇒ 真因就在这里 ✓）：
             *   `0xffffffff` 的 **alpha 字节 = 0xFF = 255** ✗ ⇒ **把脚本 423 里的 `alpha(96)` 覆盖成全不透明** ✓。
             *   ⇒ 一手 kind 9 那条路**根本不写 `color`**（`bullet.anm` script123 全文无 `color`/`blendMode` ✓，
             *     其 `color_1` 取 VM 默认白 ✓）⇒ 本卡**同样不写** ✓，让脚本的 `alpha(96)`（≈38% ✓）生效 ✓。
             *   ⚠ 别再在这里写带 A 的 color（要染色也请保 A=96 或不带 A）✓。*/
            /* ce_anm_set_color(id, …)  —— ★ 故意不写：保留脚本的 alpha(96) ✓ */
            it->vm = (int32_t)id;
            power_flash_fire(i, it->x, it->y);   /* ★ 同一刻点一台几何闪光（一手 effect.anm script94）*/
        }
        if (it->vm) ce_anm_set_pos((uint32_t)it->vm, it->x, it->y, 0.0f);
    }
}

/* ============================ 清理（删卡 / 换关 / 重开） ============================ */
/* ================= C1 的发射 / 推进 / 收尾（owner 2026-10-10 亲写）=================
 * ★ 一手（三路验证，见 `HANDOFF-109-C1-C4.md` §3.7）：灵梦 = **4 发**、伤害 **7**、速度 **14**、
 *   角度 **−120° / −100° / −80° / −60°**（★ −π/2 = 正上），anm = `pl00.anm` script10，寿命 999；
 *   行内 `rate = 100` 而槽 11 对象只活 21 帧 ⇒ 实际**只发一次**。
 * ★ 路线（owner 定）：**纯伤害源 `ce_damage_rect` + anm VM**。
 * ★ 观感：★ 暂用本卡现成脚本 396（暴风圈、程序化 drawPoly、零贴图）占位 ❓ ——
 *   ★ **已换成一手贴图**：脚本 427 画 `pl00.png` 的 `sprite38`（30×30，切图见
 *     `make_pofv109_c1_bullet_art.py` / §6.2 ✓），并照一手 `angleVel` **27°/帧自转** ✓。
 * ★ 三角函数：★ 用 C2 的预计算 **`s_c2_dir36`**（36 向表）✗ 不用 `cosf/sinf`
 *   —— 守本仓 `dllx87` 的"零 x87"硬规则 ✓。*/
static int s_c1_live[POFV_C1_SHOTS_PER_GROUP];      /* 每行一发：-1 = 空 */
static float s_c1_x[POFV_C1_SHOTS_PER_GROUP], s_c1_y[POFV_C1_SHOTS_PER_GROUP];
static float s_c1_vx[POFV_C1_SHOTS_PER_GROUP], s_c1_vy[POFV_C1_SHOTS_PER_GROUP];
static float s_c1_spd[POFV_C1_SHOTS_PER_GROUP];
/* ★★ 魔理沙 C1 = **原地伸长的光束**（一手 tick `0x54c830` ✓：`shot+0xb0` 每帧 **+12** 伸到 **512**，
 *   `age > 119` 收 ✓）⇒ 卡侧按弹记「类型」与「长度」✓（长度 → `scale.x`，贴图是 256 宽的一片 ✓）。*/
#define POFV_C1_LEN_STEP   12.0f
#define POFV_C1_LEN_MAX   512.0f
#define POFV_C1_M_END_AGE   120
static int   s_c1_kind[POFV_C1_SHOTS_PER_GROUP];   /* 0 = 灵梦那种弹 ✓；1 = 魔理沙的光束 ✓ */
static float s_c1_len[POFV_C1_SHOTS_PER_GROUP];
static int   s_c1_grp[POFV_C1_SHOTS_PER_GROUP];
/* ★★ 2026-10-11 照一手（子 agent 核实 ✓）：★ 魔理沙 **`EX+0xd4`** —— 她放过 C2 后，
 *   ★ **那一份 C1 追加 `set11` 的两发** ✓（`0x417c49` 置 1、`0x53cbe0` 消耗 ✓）。
 *   ★ set11 = **2 行**：dmg **6** / 偏移 **∓32** / **spd 0**（定点）/ anm8（= 同一支光束 ✓）。*/
static int   s_c1_m_set11;
static int   s_c1_dmg_ov[POFV_C1_SHOTS_PER_GROUP];
/* ★★ 2026-10-11 照一手（子 agent 核实）：★ 早苗 C1 的 tick `0x54c080` —— **命中敌机时**
 *   ★ `shot+0x6c = 0`（速度清零 ✓）+ **anm 中断 2**（= 脚本 444 的 `interruptLabel(2)` ✓），
 *   ★ **停 4 帧**，★ 然后以 **14.0** 继续 ✓。
 *   ★ 判据（子 agent 只读到这儿）：★ **±16 内 + y 越过** ✓ ⇒ 本卡自己遍历敌机判 ✓
 *     （`CE_ENEMY_FOREACH` ✓；不用 `fabs` ⇒ 免 libm/x87 ✓）。*/
/* ★★ 一手常量（我走**节表**读 ✓，`local/th19.v1.10c/th19.exe`）：
 *   · `DAT_005aa30c` = **16.0** ✓ = 判据里的**水平线带宽 K** ✓；
 *   · `DAT_005aa230` = **1.0** ✓  = **转向步长**（方向档位每帧 +1 ✓，共 4 帧 ⇒ 与 STOP_FRAMES 同值 ✓）。*/
#define POFV_C1S_BAND        16.0f
#define POFV_C1S_TURN_STEP    1.0f
#define POFV_C1S_STOP_FRAMES   4
#define POFV_C1S_RESUME_SPD  14.0f
static int   s_c1_stop[POFV_C1_SHOTS_PER_GROUP];
static float s_c1_py[POFV_C1_SHOTS_PER_GROUP];
static float s_c1_tx[POFV_C1_SHOTS_PER_GROUP], s_c1_ty[POFV_C1_SHOTS_PER_GROUP];   /* ★ 锁定的目标（一手 +0x100 ✓）*/
static int   s_c1_turn[POFV_C1_SHOTS_PER_GROUP];   /* ★ 转向剩几步（一手 `+0x30` 累加器 ✓）*/   /* ★ 上一帧的 y（判「越过」用 ✓，一手 ✓）*/
/* ★★★ 2026-10-11：★ C1 的**自转**改由卡侧每帧写 ✓（脚本里的 `jmp` 循环已撤 ✗ ——
 *   属于"TH18 anmm 语义与 TH19 不同"那一族 ✓）。★ 一手：27°/帧 = `0.4712389 rad` ✓。*/
#define POFV_C1_SPIN_STEP 0.4712389f
static float s_c1_rot[POFV_C1_SHOTS_PER_GROUP];
/* ★★ 2026-10-11 owner 实跑：「灵梦的 **C1 还有残影**」✗ ⇒ ★ 一手是靠 script10 里的
 *   `scriptNewRoot(script12)` ✓；★ 但 TH18 的 anmm 语义与 TH19 不同 ✗（旋转那条已经证明 ✓）
 *   ⇒ ★ **改卡侧自己补** ✓：每帧在弹的位置起一台 **433**（= 一手 script12 的复刻 ✓，20 帧自删 ✓）。
 *   ★ 池 64：一手是"每帧每弹一台" ⇒ 4 发 × 20 帧 ≈ 80 ⇒ 用 64 封顶（❓自定旋钮 ✓）。*/
/* ★★ 2026-10-11 owner 实跑：「**C1 没有崩溃 dump，但是可能 while 循环了，卡死了**」✗
 *   ⇒ 两个嫌疑，都可能是我的 ✓：
 *   ① ★ 上一轮我改了脚本 427（撤 `scriptNewRoot`）**但没强制重打 `ability.anm`** ✗ ——
 *      和"换图片"同一个坑（脚本**内容**变更 builder 不认 ✗）⇒ ★ 部署里那支 427 还是旧的 ✓，
 *      ★ DLL 又在卡侧补残影 ⇒ **双份** ⇒ VM 池被灌爆 ⇒ 引擎自旋卡死（所以没 dump ✓）；
 *   ② ★ 就算只有卡侧那一份，"每帧每弹一台" ⇒ 4 发 × 20 帧 ≈ 80 台并发 ✓，压得太满 ✗。
 *   ⇒ ★ 池降到 **32** ✓、★ 改成**每 2 帧补一台** ✓（一手是每帧 ✓，这里保守 —— ❓自定 ✓）。*/
#define POFV_C1_TRAIL_MAX 32
#define POFV_C1_TRAIL_EVERY 2
static int s_c1_trail_burst;   /* ★ 本帧已补几台 ✓（安全阀 ✓）*/
static int32_t s_c1_trail[POFV_C1_TRAIL_MAX];
static int     s_c1_trail_ttl[POFV_C1_TRAIL_MAX];
static void pofv_c1_trail_spawn(float x, float y)
{
    pofv_stage_set(14);   /* ★ 残影 spawn ✓ */
    int k;
    for (k = 0; k < POFV_C1_TRAIL_MAX; ++k) if (s_c1_trail_ttl[k] <= 0) break;
    if (k >= POFV_C1_TRAIL_MAX) return;                 /* ★ 池满 ⇒ 这一帧不放 ✓ */
    if (s_c1_trail_burst >= 4) return;                  /* ★ 每帧上限 4 台 ✓（安全阀 ✓）*/
    ++s_c1_trail_burst;
    s_c1_trail[k] = (int32_t)ce_anm_spawn(CE_ABILITY_ANM(),
                        CE_ANM_ABILITY_SCRIPT_POFV109_C1_TRAIL, POFV_C1_VM_LAYER);
    if (!s_c1_trail[k]) return;
    ce_anm_set_pos((uint32_t)s_c1_trail[k], x, y, 0.0f);
    s_c1_trail_ttl[k] = 22;                             /* ★ 略长于脚本的 20 帧 ✓ */
}
static void pofv_c1_trail_tick(void)
{
    pofv_stage_set(15);   /* ★ 残影 tick ✓ */
    int k;
    s_c1_trail_burst = 0;
    for (k = 0; k < POFV_C1_TRAIL_MAX; ++k) {
        if (s_c1_trail_ttl[k] <= 0) continue;
        if (--s_c1_trail_ttl[k] > 0) continue;
        if (s_c1_trail[k] && ce_anm_get_vm((uint32_t)s_c1_trail[k]))
            ce_anm_delete((uint32_t)s_c1_trail[k]);
        s_c1_trail[k] = 0;
    }
}   /* ★ 逐弹伤害（默认取表 ✓，追加两发覆盖为 6 ✓）*/   /* ★ 每颗弹的**组别** ✓（伤害要取对表的行 ✓）*/   /* ★ 标量速度（一手 `shot+0x6c` ✓，两段运动改它 ✓）*/
static int s_c1_age[POFV_C1_SHOTS_PER_GROUP];
static int s_c1_bounce[POFV_C1_SHOTS_PER_GROUP];   /* ★ 一手 `EX_BOUNCE`：已撞墙次数 ✓ */
static int32_t s_c1_vm[POFV_C1_SHOTS_PER_GROUP];

static void pofv_c1_clear(void)
{
    int t;
    for (t = 0; t < POFV_C1_SHOTS_PER_GROUP; ++t) {
        if (s_c1_vm[t]) {
                /* ★★★ 2026-10-11 卡死根因 ✓：★ 这里的删除**原先没判活** ✗ —— 脚本自己会
                 *   `+8: delete()`（427）/`+20: delete()`（444）⇒ ★ 句柄早已**悬空** ✗ ⇒
                 *   再删一次 ⇒ **引擎自旋、无 dump** ✓（阶段仪表指到 `[STAGE] 25` = 出界那一支 ✓）。
                 *   ★ 改成**先判活** ✓（本仓残影池/火花同款 ✓），★ 并用 `interrupt`（脚本自带收尾 ✓）。*/
                if (ce_anm_get_vm((uint32_t)s_c1_vm[t])) ce_anm_interrupt((uint32_t)s_c1_vm[t], 1);
                s_c1_vm[t] = 0;
            }
        s_c1_live[t] = 0; s_c1_age[t] = 0;
    }
}

/* ★ 一轮 C1：一次放完一手那 4 行（★ 只发一次，不是逐帧 ✗）*/
/* ★★ 一手（子 agent 核实 ✓）：★ C1 是"**发射窗口**"而不是一击 —— 槽 11 的窗口三人不同
 *   （灵梦 **21** 帧 / 魔理沙 **1** 帧 / 早苗 **30** 帧 ✓）。
 *   ★ 早苗尤其关键：★ 26 行 = **行 0–12 phase0、行 13–25 phase8** ✓、`rate16` ✓
 *   ⇒ ★ **`t=0/8/16/24` 各放 13 发（半圈一放）** ✓ —— 而不是一轮把 26 发全放 ✗。
 *   ⇒ 本函数按**行区间**发射 ✓（早苗分两次），窗口由卡侧计时 ✓。*/
static void pofv_c1_fire_range(int grp, int from, int cnt)
{
    pofv_stage_set(12);   /* ★ fire_range 入口 ✓ */
    const float *pp = ce_player_pos();
    const int nrows = s_c1_rows[grp];
    /* ★★★★ 2026-10-11 owner（「你可以用子机槽来做到这件事」✓）+ ② 的**出膛点** ✓：
     *   ★ **早苗**（`POFV_C1_GROUP_SANA`）且子机在手 ⇒ ★ 弹从 **`ce_option_x/y`** 出膛 ✓
     *   （C1 的蛇 / set10 与 C2 的青蛙 / set11 都走这一支 ✓）⇒ ★ 一并解决一手 `class 5`「子机槽出膛」
     *   与 owner「早苗原生低速子机打的就是这个弹幕」✓。
     *   ⚠ ★ **审计 D 的并账**：★ 一手 `class 5` 读的是 TH19 自己固定下标 5 槽 ✓，与本卡这个
     *   "先到先得的 12 格公共池"**不同源** ✗ ⇒ ★ 这是**同功能类比** ✓，**不是逐字段复刻** ✓
     *   （TUNING 已登记 ✓）。*/
    float ox = pp[0], oy = pp[1];
    if (grp == POFV_C1_GROUP_SANA && s_opt_sanae.ptr) {
        ox = ce_option_x(s_opt_sanae.ptr);
        oy = ce_option_y(s_opt_sanae.ptr);
    }
    int t, end;
    if (!pp) return;
    end = from + cnt; if (end > nrows) end = nrows;
    for (t = from; t < end && t < POFV_C1_SHOTS_PER_GROUP; ++t) {
        const pofv_c1_shot_t *r = (s_c1_set_sel && grp == POFV_C1_GROUP_SANA)
                                  ? &s_c1s_set11[t] : &s_c1_table[grp][t];
        int di;                                   /* ★ 角度 → 36 向表下标（一手 -pi/2 = 正上）*/
        if (s_c1_vm[t]) {
                /* ★★★ 2026-10-11 卡死根因 ✓：★ 这里的删除**原先没判活** ✗ —— 脚本自己会
                 *   `+8: delete()`（427）/`+20: delete()`（444）⇒ ★ 句柄早已**悬空** ✗ ⇒
                 *   再删一次 ⇒ **引擎自旋、无 dump** ✓（阶段仪表指到 `[STAGE] 25` = 出界那一支 ✓）。
                 *   ★ 改成**先判活** ✓（本仓残影池/火花同款 ✓），★ 并用 `interrupt`（脚本自带收尾 ✓）。*/
                if (ce_anm_get_vm((uint32_t)s_c1_vm[t])) ce_anm_interrupt((uint32_t)s_c1_vm[t], 1);
                s_c1_vm[t] = 0;
            }
        /* ★★ 2026-10-11 owner 实跑：「**不是对称的 4 发，左侧第二发靠右，是歪的**」✗
         *   ⇒ ★ 根因 = **截断取整** ✗：★ C 的 `(int)` 对**负数朝 0 偏** ✗ ⇒
         *     `−120°` 算成 **−11**（应为 −12 ✓）、`−100°` 算成 −9（应为 −10 ✓）……
         *     ★ 左侧那几发**整体偏一格** ⇒ 图案歪掉 ✓。
         *   ⇒ ★ 改成**四舍五入到最近的 10° 档** ✓（先 `+36.5` 再 `% 36` ⇒ 负数也安全 ✓）。*/
        di = (int)(r->ang * (36.0f / 6.2831853f) + 36.5f) % 36;
        s_c1_x[t] = ox + r->x;
        s_c1_y[t] = oy + r->y;
        s_c1_vx[t] = s_c2_dir36[di][0];      /* ★ 单位方向（速度另存 ⇒ 两段运动只改标量 ✓）*/
        s_c1_vy[t] = s_c2_dir36[di][1];
        s_c1_spd[t] = r->spd;                 /* ★ 一手：初始速度来自弹表（灵梦 14 ✓）*/
        s_c1_rot[t] = 0.0f;                   /* ★ 自转从 0 起 ✓ */
        s_c1_py[t] = s_c1_y[t];               /* ★ 越过判据的上一帧 y ✓ */
        s_c1_turn[t] = 0; s_c1_tx[t] = 0.0f; s_c1_ty[t] = 0.0f;   /* ★ 转向状态 ✓ */
        s_c1_age[t] = 0;
        s_c1_bounce[t] = 0;
        s_c1_live[t] = 1;
        s_c1_grp[t] = grp;                              /* ★ 记组别（一手三人表不同 ✓）*/
        s_c1_dmg_ov[t] = (int)r->dmg;                    /* ★ 默认伤害 = 表 ✓（追加两发会覆盖 ✓）*/
        s_c1_kind[t] = (grp == POFV_C1_GROUP_MARISA) ? 1 : 0;
        s_c1_len[t] = 0.0f;
        /* ★★ 三人各用**自己**的一手脚本 ✓（子 agent 核实 ✓）：灵梦 427（`pl00 script10` ✓）、
         *   魔理沙 442（`pl01 script8` ✓）、早苗 **444**（`pl02 script7` ✓，四帧 62×14 龙条 ✓）。*/
        s_c1_vm[t] = (int32_t)ce_anm_spawn(CE_ABILITY_ANM(),
                        (s_c1_kind[t] == 1) ? CE_ANM_ABILITY_SCRIPT_POFV109_C1M_BEAM
                      /* ★★ 2026-10-11 owner 记忆核对 ✓：「早苗 C2 是放一堆青蛙爆炸弹幕」——
                       *   ★ 一手 set11 的 `anm9` 就是**青蛙**（`pl02.anm` script9 ✓）⇒
                       *   ★ 而原来 set11 也走 `POFV109_C1S_SHOT`（= C1 那条龙 ✗）⇒ **这里是缺口** ✓。
                       *   ⇒ ★ 用 `s_c1_set_sel`（1 = 早苗 C2 的 set11 ✓）来选 **448（青蛙）** ✓。*/
                      : (s_c1_grp[t] == POFV_C1_GROUP_SANA)
                          ? (s_c1_set_sel ? CE_ANM_ABILITY_SCRIPT_POFV109_C2S_FROG
                                          : CE_ANM_ABILITY_SCRIPT_POFV109_C1S_SHOT)
                                            : CE_ANM_ABILITY_SCRIPT_POFV109_C1_BULLET,
                        POFV_C1_VM_LAYER);
        /* 2026-10-11 仪表：★ 出生现场（SPIN 日志三次全 0 行 ⇒ 怀疑这些弹"一出生就在场外"
         *   或"速度方向反了"⇒ 你看到的 4 发是另一批 ✓）。*/
        ce_log("pofv: [C1FIRE] grp=%d t=%d x=%.1f y=%.1f ang=%.3f spd=%.1f kind=%d vm=%d",
               grp, t, (double)s_c1_x[t], (double)s_c1_y[t], (double)r->ang, (double)s_c1_spd[t],
               s_c1_kind[t], (int)s_c1_vm[t]);
        if (s_c1_vm[t]) {
            ce_anm_set_pos((uint32_t)s_c1_vm[t], s_c1_x[t], s_c1_y[t], 0.0f);
            ce_anm_set_scale((uint32_t)s_c1_vm[t], 1.0f, 1.0f);   /* ★ 一手贴图原尺寸 ⇒ 1:1 ✓（灵梦 30×30 / 早苗 62×14 ✓）*/
            /* ★ C1 放出的是**弹**、不是消弹的火花 ⇒ 不染色 ✓。
             *   ★ 注：★ 各档的"消弹圈"**已改为一手原生的 `etbreak` 8 帧火花** ✓（脚本 431，
             *   在 `cancel_white_in_circle` 里**逐颗**放 ✓）—— ★ 早先那个自烤红环已废 ✗。*/
        }
    }
}

/* ★ C1 入口 ✓：★ 按角色选组，并**按一手开窗口** ✓（早苗分半圈放 ✓，见上）。*/
static int s_c1_win_left;      /* ★ 窗口剩余帧（0 = 没开 ✓）*/
static int s_c1_win_age;       /* ★ 窗口已过帧 ✓ */
static int s_c1_sanae_half;    /* ★ 早苗当前放的是哪半圈（0 = 行 0–12 ✓ / 1 = 行 13–25 ✓）*/
static void pofv_c1_fire(void)
{
    const int grp = (s_aura_kind == 1) ? POFV_C1_GROUP_MARISA
                  : (s_aura_kind == 3) ? POFV_C1_GROUP_SANA
                  : POFV_C1_GROUP_REI;
    s_c1_win_left = 0; s_c1_win_age = 0; s_c1_sanae_half = 0;
    if (grp == POFV_C1_GROUP_SANA) {
        pofv_c1_fire_range(grp, 0, 13);          /* ★ t=0：行 0–12 ✓ */
        s_c1_win_left = 30;                       /* ★ 一手 ✓ 早苗窗口 30 帧 */
    } else {
        pofv_c1_fire_range(grp, 0, s_c1_rows[grp]);   /* ★ 灵梦 4 行一次 / 魔理沙 1 行一次 ✓ */
        s_c1_win_left = (grp == POFV_C1_GROUP_MARISA) ? 1 : 21;   /* ★ 一手 ✓ 1 / 21 帧 */
    }
    /* ★★ 一手 ✓ `EX+0xd4`：★ 魔理沙放过 C2 后，**这份 C1 追加两发**（dmg6 / ∓32 / spd0 ✓）。
     *   ★ 用 1、2 号槽（魔理沙自己只占 0 号 ✓）；★ 贴图/脚本同光束 ✓（一手 anm8 ✓）。*/
    if (s_c1_m_set11 && grp == POFV_C1_GROUP_MARISA) {
    pofv_stage_set(11);   /* ★ C1 的 EX+0xd4 追发 ✓ */
        const float *pp2 = ce_player_pos();
        s_c1_m_set11 = 0;                                  /* ★ 一次消耗 ✓（一手 ✓）*/
        if (pp2) {
            int e;
            for (e = 0; e < 2; ++e) {
                const int t = 1 + e;
                if (s_c1_vm[t]) {
                /* ★★★ 2026-10-11 卡死根因 ✓：★ 这里的删除**原先没判活** ✗ —— 脚本自己会
                 *   `+8: delete()`（427）/`+20: delete()`（444）⇒ ★ 句柄早已**悬空** ✗ ⇒
                 *   再删一次 ⇒ **引擎自旋、无 dump** ✓（阶段仪表指到 `[STAGE] 25` = 出界那一支 ✓）。
                 *   ★ 改成**先判活** ✓（本仓残影池/火花同款 ✓），★ 并用 `interrupt`（脚本自带收尾 ✓）。*/
                if (ce_anm_get_vm((uint32_t)s_c1_vm[t])) ce_anm_interrupt((uint32_t)s_c1_vm[t], 1);
                s_c1_vm[t] = 0;
            }
                s_c1_grp[t] = POFV_C1_GROUP_MARISA; s_c1_kind[t] = 1; s_c1_dmg_ov[t] = 6;
                s_c1_len[t] = 0.0f;
                s_c1_x[t] = pp2[0] + (e ? 32.0f : -32.0f);      /* ★ 偏移 ∓32 ✓ */
                s_c1_y[t] = pp2[1];
                s_c1_vx[t] = 0.0f; s_c1_vy[t] = -1.0f; s_c1_spd[t] = 0.0f;   /* ★ spd 0（定点）✓ */
                s_c1_age[t] = 0; s_c1_bounce[t] = 0; s_c1_live[t] = 1;
                s_c1_vm[t] = (int32_t)ce_anm_spawn(CE_ABILITY_ANM(),
                                CE_ANM_ABILITY_SCRIPT_POFV109_C1M_BEAM, POFV_C1_VM_LAYER);
                pofv_stage_set(23);   /* ★ 写位置前 */
        /* ★★ 2026-10-11 owner 实跑「**没在转**」✗ ⇒ ★ 自转**改回脚本里的 `angleVel`** ✓
         *   （一手就写在循环外 ⇒ 撤掉循环后正好是它的可达状态 ✓）；★ 卡侧这句撤掉 ✗
         *   （两套同时上会**双倍转速** ✓）。`s_c1_rot` 保留但不再写（留作将来用）。*/
        s_c1_rot[t] += POFV_C1_SPIN_STEP;
        /* 2026-10-11：原来每 60 帧才打 ⇒ C1 的弹十几帧就出界了 ⇒ 日志 0 行 ✗
         * ⇒ 改成头几帧就打（2/6/10/14 + 之后每 60）。*/
        /* 2026-10-11：前两版日志都 0 行（`t==0` + 每 60 帧 / 头几帧都对不上）
         * ⇒ 改成**前 4 个槽、头 16 帧、每 2 帧**都打 —— 这次不可能再是空的。*/
        if (s_c1_vm[t]) ce_anm_set_rotation((uint32_t)s_c1_vm[t], 0.0f, 0.0f, s_c1_rot[t]);
        if (s_c1_vm[t]) ce_anm_set_pos((uint32_t)s_c1_vm[t], s_c1_x[t], s_c1_y[t], 0.0f);
            }
        }
    }
}

/* ★ 每帧：窗口计时（早苗每 8 帧换半圈 ⇒ t=0/8/16/24 各 13 发 ✓，一手 ✓）。*/
static void pofv_c1_window_tick(void)
{
    pofv_stage_set(13);   /* ★ 窗口 tick ✓ */
    if (s_c1_win_left <= 0) return;
    --s_c1_win_left;
    ++s_c1_win_age;
    if (s_c2s_win > 0) {                       /* ★ 早苗 C2 的 60 帧窗口 ✓（一手 ✓）*/
        if (--s_c2s_win == 0) s_c1_set_sel = 0;   /* ★ 窗口尽 ⇒ 切回 set10 ✓ */
    }
    if (s_aura_kind != 3) return;
    if (s_c1_win_age % 8 == 0 && s_c1_win_age < 30) {
        s_c1_sanae_half ^= 1;
        pofv_c1_fire_range(POFV_C1_GROUP_SANA, s_c1_sanae_half ? 13 : 0, 13);
    }
}


/* ★ 每帧：前进 + 伤害源 + 到期/出屏收尾 */
static void pofv_c1_tick(void)
{
    pofv_c1_window_tick();   /* ★ 一手：发射窗口（早苗的半圈切换 ✓）*/
    pofv_c1_trail_tick();    /* ★ 残影池（灵梦 C1 ✓）*/
    int t;
    for (t = 0; t < POFV_C1_SHOTS_PER_GROUP; ++t) {
        float pos[3];
        if (!s_c1_live[t]) continue;
        /* ★★ 早苗 C1：命中停 4 帧（一手 ✓）—— 停住时不动、之后以 14.0 继续 ✓ */
        if (s_c1_grp[t] == POFV_C1_GROUP_SANA && s_c1_kind[t] == 0) {
            if (s_c1_stop[t] > 0) {
                --s_c1_stop[t];
                /* ★★★ 一手 ✓「**水平转向**」：停住这几帧里，把方向**一档一档**转目标 ✓
                 *   （一手：方向档位 `+0x34 += 步长(1.0)` ✓、共 4 帧到 `+0x30 == 4` ✓）。
                 *   ★ 用卡里现成的 36 向表做档位步进 ✓（避免 `atan2` ✓；一手也是查表 ✓）。*/
                if (s_c1_turn[t] > 0) {
                    int cur = -1, tgt = -1, k2;
                    {   /* ★ 当前方向档 = 与 (vx,vy) 点积最大者 ✓（同样不必归一化 ✓，阈值写法换成取最大 ✓）*/
                        float cd = -1e30f;
                        for (k2 = 0; k2 < 36; ++k2) {
                            const float d2 = s_c1_vx[t] * s_c2_dir36[k2][0] + s_c1_vy[t] * s_c2_dir36[k2][1];
                            if (d2 > cd) { cd = d2; cur = k2; }
                        }
                    }
                    {
                        /* ★ 目标方向档 = 36 向表里**点积最大**的那档 ✓
                         *   （点积最大即夹角最小 ✓ ⇒ **不需要归一化** ✓ —— 我上一版写了归一化纯属多余 ✗）。*/
                        const float ddx = s_c1_tx[t] - s_c1_x[t], ddy = s_c1_ty[t] - s_c1_y[t];
                        int best = 0; float bestd = -1e30f;
                        for (k2 = 0; k2 < 36; ++k2) {
                            const float d2 = ddx * s_c2_dir36[k2][0] + ddy * s_c2_dir36[k2][1];
                            if (d2 > bestd) { bestd = d2; best = k2; }
                        }
                        tgt = best;
                    }
                    if (cur < 0) cur = tgt;
                    if (cur >= 0 && tgt >= 0 && cur != tgt) {
                        int step2 = (s_c1_turn[t] >= 2) ? 2 : 1;
                        int d2 = ((tgt - cur + 36) % 36);
                        if (d2 > 18) { cur = (cur - step2 + 36) % 36; } else { cur = (cur + step2) % 36; }
                        s_c1_vx[t] = s_c2_dir36[cur][0]; s_c1_vy[t] = s_c2_dir36[cur][1];
                        --s_c1_turn[t];                    /* ★ 一手 ✓ 共 4 步 */
                    }
                }
                if (s_c1_stop[t] == 0) s_c1_spd[t] = POFV_C1S_RESUME_SPD;   /* ★ 一手 ✓ 14.0 */
                pofv_stage_set(22);   /* ★ 移动后/年龄前 */
        ++s_c1_age[t];
                if (s_c1_vm[t]) ce_anm_set_pos((uint32_t)s_c1_vm[t], s_c1_x[t], s_c1_y[t], 0.0f);
                pos[0] = s_c1_x[t]; pos[1] = s_c1_y[t]; pos[2] = 0.0f;
                pofv_stage_set(24);   /* ★ 伤害调用前 */
        ce_damage_rect(pos, 0.0f, POFV_C1_HIT_LIFE, (float)s_c1_dmg_ov[t],
                               POFV_C1_HIT_W, POFV_C1_HIT_H);
                continue;
            }
            pofv_stage_set(19);   /* ★ 早苗命中遍历（最可疑 ✓）*/
            {   uint8_t *em2 = CE_ENEMY_MGR(), *e2; int hit2 = 0; float ep_tx = 0.0f, ep_ty = 0.0f;
                CE_ENEMY_FOREACH(em2, e2) {
                    const float *ep = ce_enemy_pos(e2);
                    /* ★★★★ 2026-10-11 **照 TH19 一手订正** ✓（`FUN_0054c080` 反编译 ✓，
                     *   `local/th19.v1.10c/decomp/all.c:246937` 起；owner 语义 ✓
                     *   「当**水平方向**上发现敌人，**水平转向**」）：
                     *   ★ 一手判据（原文）：
                     *     · `enemy.y - K <= bullet.y <= enemy.y + K` ✓ = **同一水平线**（Y 在带内 ✓）
                     *     · `bullet.x <= enemy.x - K || enemy.x + K <= bullet.x` ✓ = **横向有距离** ✓
                     *   ★ 常量（我走**节表**读的 ✓）：`DAT_005aa30c` = **16.0** ✓。
                     *   ⚠ ★ 我先前那句「±16 内 + **y 越过**」✗ **作废** ✓ —— 一手是**带（band）**，
                     *     不是"这一帧跨过" ✗（那是子 agent 只读到一半的表述 ✓）。*/
                    const float ex = ep[0] - s_c1_x[t];
                    const float ey = ep[1] - s_c1_y[t];
                    if (ey > -POFV_C1S_BAND && ey < POFV_C1S_BAND          /* ★ 同一水平线 ✓ */
                        && (ex <= -POFV_C1S_BAND || ex >= POFV_C1S_BAND))  /* ★ 横向有距离 ✓ */
                    { hit2 = 1; ep_tx = ep[0]; ep_ty = ep[1]; break; }
                }
                if (hit2) {
                    s_c1_stop[t] = POFV_C1S_STOP_FRAMES;      /* ★ 一手 ✓ 停 4 帧 */
                    s_c1_spd[t] = 0.0f;                        /* ★ 一手 ✓ 速度清零 */
                    if (s_c1_vm[t]) ce_anm_interrupt((uint32_t)s_c1_vm[t], 2);   /* ★ 一手 ✓ 中断 2 */
                    /* ★★ 一手 ✓：锁住时把**敌机坐标**抄进 `+0x100`（= 目标 ✓）—— 我先前漏了这一步 ✗。
                     *   ★ 转向步数 = 4（= 一手的累加器到 4 恢复 ✓，与 STOP_FRAMES 同值 ✓）。*/
                    s_c1_tx[t] = ep_tx; s_c1_ty[t] = ep_ty;
                    s_c1_turn[t] = POFV_C1S_STOP_FRAMES;
                }
            }
        }
        /* ★★ 灵梦 C1 的**残影** ✓（一手 script12 ✓）：每帧在弹的位置补一台 ✓（卡侧驱动 ✓）。*/
        /* ★★★ 2026-10-11 **先关掉**（阶段仪表定位 ✓）：★ 日志最后一行是 `[STAGE] 14`（= 这里 ✓），
         *   ★ 而 `[STAGE] 15`（残影 tick ✓）**从没出现** ⇒ ★ **卡死就在 `pofv_c1_trail_spawn` 里** ✗
         *   （最可能是 `ce_anm_spawn(433)` 在引擎里自旋 ✓）。
         *   ⇒ ★ 先**整条关掉**做决定性验证 ✓：★ 若卡死消失 ⇒ 就是它 ✓，再换画法 ✓
         *     （例：把残影交回脚本 `scriptNewRoot` ✓ / 或换一台常驻 VM 手动摆位置 ✓）。*/
        /* ★★ 2026-10-11 **重开** ✓（卡死已解决 ✓：427 的脚本循环撤掉后就不卡了 ✓）。
         *   ★ 安全写法：★ 脚本 433 已加 `stop()` ✓、★ 卡侧判活 + 每帧上限 4 ✓、★ 池 32 ✓。*/
        if (s_c1_kind[t] == 0 && s_c1_grp[t] == POFV_C1_GROUP_REI
            && (s_c1_age[t] % POFV_C1_TRAIL_EVERY) == 0)
            pofv_c1_trail_spawn(s_c1_x[t], s_c1_y[t]);
        if (s_c1_kind[t] == 1) {
            /* ★★ 一手 `0x54c830`（魔理沙 ✓）：★ **原地不动**，长度每帧 **+12**、封顶 **512**，
             *   ★ `age > 119` 收 ✓（贴图 256 宽 ⇒ `scale.x = 长度 / 256` ✓）。*/
            s_c1_len[t] += POFV_C1_LEN_STEP;
            if (s_c1_len[t] > POFV_C1_LEN_MAX) s_c1_len[t] = POFV_C1_LEN_MAX;
            if (s_c1_vm[t]) {
                ce_anm_set_pos((uint32_t)s_c1_vm[t], s_c1_x[t], s_c1_y[t], 0.0f);
                ce_anm_set_scale((uint32_t)s_c1_vm[t], s_c1_len[t] / 256.0f, 1.0f);
            }
            ++s_c1_age[t];
            if (s_c1_age[t] > POFV_C1_M_END_AGE) {
                if (s_c1_vm[t]) { ce_anm_interrupt((uint32_t)s_c1_vm[t], 1); s_c1_vm[t] = 0; }
                s_c1_live[t] = 0;
            }
            pos[0] = s_c1_x[t]; pos[1] = s_c1_y[t]; pos[2] = 0.0f;
            ce_damage_rect(pos, 0.0f, POFV_C1_HIT_LIFE, (float)s_c1_dmg_ov[t],
                           POFV_C1_HIT_W, POFV_C1_HIT_H);
            continue;
        }
        /* ★★ 一手 `0x54b380` 两段运动 ✓：age < 120 ⇒ 减速（下限 0.2）；age ≥ 120 ⇒ 重新加速 ✓ */
        if (s_c1_age[t] < POFV_C1_HOLD_AT) {
            s_c1_spd[t] -= POFV_C1_DECEL;
            if (s_c1_spd[t] < POFV_C1_HOLD_SPD) s_c1_spd[t] = POFV_C1_HOLD_SPD;
        } else {
            s_c1_spd[t] += POFV_C1_REACCEL;
        }
        s_c1_py[t] = s_c1_y[t];      /* ★ 记上一帧 y（判越过 ✓）*/
        pofv_stage_set(21);   /* ★ 移动前 */
        s_c1_x[t] += s_c1_vx[t] * s_c1_spd[t];
        s_c1_y[t] += s_c1_vy[t] * s_c1_spd[t];
        ++s_c1_age[t];
        /* ★★ 2026-10-11 照一手删掉**撞墙反弹** ✗ —— 一手没有这条 ✓（见上面常量区的说明 ✓）*/
        pofv_stage_set(25);   /* ★ 出界检查前 */
        if (s_c1_x[t] < -POFV_FIELD_HALF_W || s_c1_x[t] > POFV_FIELD_HALF_W ||
            s_c1_y[t] < 0.0f || s_c1_y[t] > POFV_FIELD_H || s_c1_age[t] >= POFV_C1_BULLET_LIFE) {
            if (s_c1_vm[t]) {
                /* ★★★ 2026-10-11 卡死根因 ✓：★ 这里的删除**原先没判活** ✗ —— 脚本自己会
                 *   `+8: delete()`（427）/`+20: delete()`（444）⇒ ★ 句柄早已**悬空** ✗ ⇒
                 *   再删一次 ⇒ **引擎自旋、无 dump** ✓（阶段仪表指到 `[STAGE] 25` = 出界那一支 ✓）。
                 *   ★ 改成**先判活** ✓（本仓残影池/火花同款 ✓），★ 并用 `interrupt`（脚本自带收尾 ✓）。*/
                if (ce_anm_get_vm((uint32_t)s_c1_vm[t])) ce_anm_interrupt((uint32_t)s_c1_vm[t], 1);
                s_c1_vm[t] = 0;
            }
            s_c1_live[t] = 0;
            continue;
            ce_log("pofv: [C1OFF] t=%d age=%d x=%.1f y=%.1f spd=%.1f", t, s_c1_age[t],
                   (double)s_c1_x[t], (double)s_c1_y[t], (double)s_c1_spd[t]);
        }
        if (s_c1_vm[t]) ce_anm_set_pos((uint32_t)s_c1_vm[t], s_c1_x[t], s_c1_y[t], 0.0f);
        pos[0] = s_c1_x[t]; pos[1] = s_c1_y[t]; pos[2] = 0.0f;
        ce_damage_rect(pos, 0.0f, POFV_C1_HIT_LIFE, (float)s_c1_dmg_ov[t],
                       POFV_C1_HIT_W, POFV_C1_HIT_H);
    }
}

/* ================= ⑤ C3：ExtraAttack（★ 本轮只做**灵梦**）=================
 * ★ 一手：`HANDOFF-109-C1-C4.md` §10（素材 = `local/th19.v1.10c/_ext/ecl/pl00.txt:642`
 *   的 `void ExtraAttack()`，CP932；opcode 名取 `thecl10.c:th13_fmts` ✓）。
 *   ★ 一手逐条（照抄 ✓）：
 *     · 开头 `flagSet(172)` + `ins_1034()`（= **刷新档位** `clamp(S+0x38,0,7)` 写入 `E+0x5168`，§10.2 ✓）；
 *     · `ins_400(0.0f, 128.0f)` ⇒ **载体移到自机上方 128**（ECL 的 y+ = 上 ⇒ 本作屏幕 y 向下 ⇒ **−128** ✓）；
 *       ★ 发弹原点 = **载体自身**（一手没有 `etOffset` ✗）⇒ 一波里的每颗都从**同一个环心**出 ✓；
 *     · `ins_570($C, 6,8,6,8,6,8,6,8)` = 每波颗数、`ins_570($D, 1,1,2,2,3,3,4,4)` = 波数
 *       （`ins_570` = **按档取表** `dst = v[clamp(S+0x38,0,7)]`，§10.2 ✓）；
 *     · 每波：`ins_602($A, 46/47, 0)`（**奇偶换 sprite**，波序号 `$G` 从 0 起 ⇒ 第 1 波 = 46）
 *       + `%F = [-9998.0f]`（**重掷基准角**）+ `$C` 次「`ins_604($A, 2π·i/$C + %F, 0)` ⇒ `ins_601($A)`」
 *       = **同一帧把 C 颗全放出去（环）、一颗一发** ✓（★ 不是逐帧一顆 ✗）；
 *     · 每波末 wait（`!0..!7` 的 switch、**8 例 ⇒ 下标 = 档位**）⇒ `{60,60,60,60,50,50,40,40}` ✓；
 *     · `ins_605($A, 2.3f, 1.0f)` = `etSpeed(第一层, 最后一层)`（`ECL-info.md:37` ✓）——
 *       本卡每波是 `ins_606($A,1,1)`（**1 层**）⇒ 取第一层 **2.3** ✓；
 *     · `ins_611(...)` = `etEx`（`ECL-info.md:52`）：**反弹 / 加速 / 无敌**三条（§10.5），见下面 §效果。
 *
 * ❓自定 / 存疑（逐条进 `TUNING.md`）：
 *   ① **档位** = `POFV_C3_LEVEL_DEFAULT`(=1)：一手档位是**每局按难度**取常数（§10.2 实测 `∈{1,3}`）✗
 *      本卡没有那个量 ⇒ 先固定档 1（⇒ `$D=1`、`$C=8`、wait=60 ✓）；
 *   ② **伤害值** = `POFV_C3_HIT_DMG`(=14)：一手 `ExtraAttack` 这一段**没给伤害** ✗ ⇒ 借 C1 的一手口径
 *      （灵梦本体主炮 = **14** ✓，`HANDOFF-109-C1-C4.md` §3.7）；
 *   ③ **`%B = [-9888.0f]`** 未定位（一手只用在**加速度系数**上，用法像 0..1 归一化标量）
 *      ⇒ 取 **0.0** ✓（§10.5）⇒ 加速率 = `0.01f` ✓；
 *   ④ **观感** = 现成脚本 **396**（= C1/C2 的占位同款；**不新切贴图 / 不动 `ORDER.txt`** ✗）；
 *      ★ owner 定「C 的圈 = **扩张的红色圆环**」⇒ `ce_anm_set_color(vm, POFV_C_RING_COLOR)` ✓；
 *   ⑤ **每波奇偶换贴图**（一手 `etSprite(46)/(47)`）：本卡**不能换色**（owner 定红 ✗）⇒ 用**奇偶波尺寸**
 *      近似两个 sprite 的外观差（`POFV_C3_VM_SCALE_EVEN/ODD`）❓；
 *   ⑥ **寿命** `POFV_C3_BULLET_LIFE`(=600)：一手没给弹寿命 ✗ ⇒ 取 `EX_ACCEL` 的 600 帧当兜底
 *      （撞墙 4 次通常更早就收）❓；**环心是否跟随自机**一手未明 ⇒ 本卡**每波取当前自机位置** ❓；
 *   ⑦ **`EX_INVULN` 240 帧**：一手是「弹**不会被打掉**」的效果 ✗ —— 本卡的弹是**伤害源**、没有
 *      「被消」这个状态 ⇒ **跳过** ✓（§10.5）；同 `ins_611($A,0,8,120,…)`（type 8）**未解码** ⇒ 不落地 ❓；
 *   ⑧ **三角函数**：★ 用 C2 的预计算 **`s_c2_dir36`**（36 向表）+ **5° 中点插值** ✗ 不用 `cosf/sinf`
 *      —— 守本仓 `dllx87` 的"零 x87"硬规则 ✓（`2π·i/8` = 45° 不是 10° 的整数倍 ⇒ 见 `c3_dir5()`）；
 *   ⑨ **消弹**：C3 一手这一支**没有**消弹圈 ✗（消弹在发动瞬间的 `Ecl_EtBreakLv3`，本轮不做）❓。
 *
 * ★ 清理（铁律「长持续效果删卡要交代」✓）：跨帧遗留物 = **每颗弹一台 VM** ⇒ `pofv_c3_clear()` 全真删 ✓
 *   （`on_ctor` / `on_run_reset` / `on_delete` 三路共用 `clear_card()` ✓；伤害源 life=2 自灭、无残留 ✓）。
 */
#define POFV_C3_LEVEL_DEFAULT       1      /* ❓①：档索引 0..7（一手 = 每局按难度取常数）*/
#define POFV_C3_MAX_BULLETS        48      /* ❓自定：同屏弹池上限（一手无上限：最多 $D×$C = 4×8 = 32）*/
#define POFV_C3_CARRIER_DY     (-128.0f)   /* ★ 一手 ✓ `ins_400(0.0f, 128.0f)`：环心 = 自机上方 128 */
#define POFV_C3_SPEED              2.3f    /* ★ 一手 ✓ `ins_605($A, 2.3f, 1.0f)` 的第一层 */
#define POFV_C3_ACCEL_RATE        0.01f    /* ★ 一手 ✓ `0.01f + %B*0.02f`，而 %B ❓= 0.0 */
#define POFV_C3_ACCEL_FRAMES       600     /* ★ 一手 ✓ `ins_611($A, 0, 4, 600, …)` 的 600 */
#define POFV_C3_BOUNCE_MAX           4     /* ★ 一手 ✓ §10.5「EX_BOUNCE（4 次撞墙）」*/
#define POFV_C3_BULLET_LIFE        600     /* ❓⑥：兜底寿命（= 加速时长）*/
#define POFV_C3_HIT_DMG             14     /* ❓②：借 C1 的一手口径（灵梦主炮 14）*/
#define POFV_C3_HIT_LIFE             2     /* 同 C1：伤害源每帧按当前位置重发 ⇒ life 2 */
#define POFV_C3_HIT_W             32.0f    /* 同 C1 ❓自定（一手 `pl00.anm` script10 弹体 30×30）*/
#define POFV_C3_HIT_H             32.0f
#define POFV_C3_VM_LAYER             6     /* ❓自定：同 C1/C2/暴风 */
#define POFV_C3_VM_SCALE_EVEN     0.25f    /* ❓⑤：偶波（一手 `etSprite(46)`）*/
#define POFV_C3_VM_SCALE_ODD      0.35f    /* ❓⑤：奇波（一手 `etSprite(47)`）*/

/* ★ 8 档表（★ 一手逐字照抄，§10.3 ✓）：`ins_570` 的下标 = `clamp(S+0x38, 0, 7)` */
static const unsigned char s_c3_waves[8]  = { 1, 1, 2, 2, 3, 3, 4, 4 };            /* $D ✓ */
static const unsigned char s_c3_count[8]  = { 6, 8, 6, 8, 6, 8, 6, 8 };            /* $C ✓ */
static const unsigned char s_c3_wait[8]   = { 60, 60, 60, 60, 50, 50, 40, 40 };    /* 每波末 wait ✓ */

/* ★ 5° 网格（72 档）查表：偶数档 = `s_c2_dir36` 直取；奇数档 = 相邻两向的**中点**
 *   （两单位向量夹角 10° ⇒ 中点模长 = cos5° = 0.9962 ⇒ 随速度 0.4% 误差，可忽略 ✓）。
 *   **不引 `cosf/sinf`、不做 `sqrtf`** ⇒ 守 `dllx87` 的零 x87 不变式 ✓。*/
static void c3_dir5(int idx5, float *dx, float *dy)
{
    int k = idx5 % 72;
    if (k < 0) k += 72;
    if ((k & 1) == 0) {
        *dx = s_c2_dir36[k / 2][0];
        *dy = s_c2_dir36[k / 2][1];
    } else {
        const int a = (k / 2) % 36;
        const int b = (a + 1) % 36;
        *dx = 0.5f * (s_c2_dir36[a][0] + s_c2_dir36[b][0]);
        *dy = 0.5f * (s_c2_dir36[a][1] + s_c2_dir36[b][1]);
    }
}

/* 一颗弹：方向**单位向量** + 速率（`EX_ACCEL` 只改速率、方向不变 ⇒ 不必重算三角 ✓）。*/
typedef struct {
    int   live;
    int   age;
    int   bounces;      /* 已撞墙次数（一手 EX_BOUNCE = 4 ⇒ 第 5 次收 ✓）*/
    float x, y;
    float dx, dy;       /* 单位方向 */
    float spd;          /* 当前速率（2.3 → 每帧 +0.01）*/
    int32_t vm;
} pofv_c3_bullet_t;
static pofv_c3_bullet_t s_c3_b[POFV_C3_MAX_BULLETS];

/* C3 波次机（一手：一波 = 同一帧连发 `$C` 颗 ⇒ 等 `wait` 帧 ⇒ 下一波 ⇒ 共 `$D` 波）。*/
typedef struct {
    int active;
    int level;      /* 档索引 0..7 */
    int wave;       /* 已发出的波数 */
    int wait;       /* 距下一波的剩余帧（0 = 本帧发）*/
    int parity;     /* 波奇偶（近似一手 `etSprite(46)/(47)`，见 ❓⑤）*/
    int     eb, ring_vm;   /* ★ 一手 `Ecl_EtBreakLv3` 的步进 / 范围圈句柄 ✓ */
} pofv_c3_t;
static pofv_c3_t s_c3;

static void pofv_c3_clear(void)
{
    pofv_eb_ring_close(&s_c3.ring_vm);   /* ★ 铁律：范围圈 VM 也收 ✓ */
    s_c3.eb = -1;
    int k;
    for (k = 0; k < POFV_C3_MAX_BULLETS; ++k) {
        if (s_c3_b[k].vm) { ce_anm_delete((uint32_t)s_c3_b[k].vm); s_c3_b[k].vm = 0; }
        s_c3_b[k].live = 0; s_c3_b[k].age = 0; s_c3_b[k].bounces = 0;
    }
    s_c3.active = 0; s_c3.level = 0; s_c3.wave = 0; s_c3.wait = 0; s_c3.parity = 0;
}

/* 发一波：★ 一手 = 同一帧把 `$C` 颗按 `2π·i/$C + %F` 全放出去（环），每颗**一颗一发** ✓。
 *  `%F` 每波**重掷**（一手波内 `%F = [-9998.0f]` ⇒ 取那一颗 ✓）。*/
static void c3_fire_wave(void)
{
    const float *p = ce_player_pos();
    const int n = (int)s_c3_count[s_c3.level];
    const int base5 = (int)(ce_rand() % 72u);          /* ★ 一手 ✓ 基准角 `%F`（本卡量化到 5° 网格 ❓）*/
    const float scale = (s_c3.parity & 1) ? POFV_C3_VM_SCALE_ODD : POFV_C3_VM_SCALE_EVEN;
    int i;
    if (!p) return;
    for (i = 0; i < n; ++i) {
        pofv_c3_bullet_t *b = 0;
        int k;
        for (k = 0; k < POFV_C3_MAX_BULLETS; ++k) if (!s_c3_b[k].live) { b = &s_c3_b[k]; break; }
        if (!b) break;                                  /* 池满 ⇒ 本波少几颗（一手无池上限，本卡兜底）*/
        {
            const int idx5 = (base5 + (72 * i) / n) % 72;   /* `2π·i/$C + %F`（$C ∈ {6,8} ⇒ 整除 ✓）*/
            c3_dir5(idx5, &b->dx, &b->dy);
        }
        b->x = p[0];
        b->y = p[1] + POFV_C3_CARRIER_DY;               /* ★ 发弹原点 = 载体自身（自机上方 128）✓ */
        b->spd = POFV_C3_SPEED;
        b->age = 0; b->bounces = 0; b->live = 1;
        b->vm = (int32_t)ce_anm_spawn(CE_ABILITY_ANM(), CE_ANM_ABILITY_SCRIPT_POFV109_STORM, POFV_C3_VM_LAYER);
        if (b->vm) {
            ce_anm_set_pos((uint32_t)b->vm, b->x, b->y, 0.0f);
            ce_anm_set_scale((uint32_t)b->vm, scale, scale);                     /* ❓⑤ 奇偶近似 */
            ce_anm_set_color((uint32_t)b->vm, POFV_C_RING_COLOR);                /* ★ owner：红 ✓ */
        }
    }
}

/* 释放（按蓄力档到 C3 那一帧）：复位波次机 ★ 当帧就发第 1 波（与一手"释放即生效"同形 ✓）。*/
static void pofv_c3_fire(void)
{
    s_c3.active = 1;
    s_c3.level = POFV_C3_LEVEL_DEFAULT;   /* ❓① 一手 = `clamp(S+0x38,0,7)`（每局按难度取常数）*/
    if (s_c3.level < 0) s_c3.level = 0;
    if (s_c3.level > 7) s_c3.level = 7;
    s_c3.wave = 0;
    s_c3.wait = 0;
    s_c3.parity = 0;
    s_c3.eb = 0;   /* ★ 一手的 `Ecl_EtBreakLv3`（16/24/32）从这一帧起 ✓ */
}

/* 每帧：① 波次机（发波 / 波末 wait）② 每颗弹（加速 + 撞墙反弹 + 伤害源）。
 * ⚠ **不依赖结界键电平** ✓；波发完（`active=0`）后**已飞的弹要继续走完** ⇒ 不能在这里 early-return ✗。*/
static void pofv_c3_tick(void)
{
    /* ★★ 一手 `Ecl_EtBreak`（§5.1 ✓）：**每帧一档**、共 POFV_EB_LV3_FRAMES 帧 ⇒ 消弹 + 撒道具 + 范围圈 ✓
     *   （★ 留本侧 ✓ —— 一手这一档会送对侧，本作是"我方场地"⇒ 不翻 ✓）。*/
    if (s_c3.eb >= 0 && s_c3.eb <= POFV_EB_LV3_FRAMES) {
        const float *pe = ce_player_pos();
        if (pe) {
            float pos[3];
            const float r = POFV_EB_LV3_R0 + POFV_EB_STEP * (float)s_c3.eb;
            pos[0] = pe[0]; pos[1] = pe[1]; pos[2] = 0.0f;
            (void)c2_cancel_drop(pos, r);
            pofv_eb_ring_sync(&s_c3.ring_vm, r);
        }
        ++s_c3.eb;
    } else if (s_c3.eb > 0) {
        pofv_eb_ring_close(&s_c3.ring_vm);
        s_c3.eb = -1;
    }
    int k, busy = 0;
    for (k = 0; k < POFV_C3_MAX_BULLETS; ++k) if (s_c3_b[k].live) { busy = 1; break; }
    if (!s_c3.active && !busy) return;

    /* ① 波次（一手：`$D` 波；每波末 wait —— **最后一波也等完才收** ✓）*/
    if (s_c3.active) {
        if (s_c3.wait > 0) --s_c3.wait;
        if (s_c3.wait == 0) {
            if (s_c3.wave < (int)s_c3_waves[s_c3.level]) {
                c3_fire_wave();
                s_c3.wait = (int)s_c3_wait[s_c3.level];
                ++s_c3.wave;
                ++s_c3.parity;
            } else {
                s_c3.active = 0;
            }
        }
    }

    /* ② 弹（池里**所有**弹都要推进 —— 与"当前波"无关 ✓）*/
    for (k = 0; k < POFV_C3_MAX_BULLETS; ++k) {
        pofv_c3_bullet_t *b = &s_c3_b[k];
        float pos[3];
        int hit = 0;
        if (!b->live) continue;
        ++b->age;
        /* ★ 一手 ✓ `EX_ACCEL`（600 帧）：**只改速率、不改方向**（⇒ 不必重算三角 ✓）。
         *   速率 = `0.01f + %B*0.02f`（%B ❓= 0.0）⇒ 每帧 +0.01 ✓。*/
        if (b->age <= POFV_C3_ACCEL_FRAMES) b->spd += POFV_C3_ACCEL_RATE;
        b->x += b->dx * b->spd;
        b->y += b->dy * b->spd;
        /* ★ 一手 ✓ `EX_BOUNCE`（4 次撞墙）：自己判撞墙改向；**第 5 次撞墙 ⇒ 收掉** ✓
         *   （一手是引擎弹 VM 的 `wall_bounce`，本卡照 owner 路线**卡侧逐帧驱动** ✓）*/
        if (b->x < -POFV_FIELD_HALF_W)     { b->x = -POFV_FIELD_HALF_W; b->dx = -b->dx; hit = 1; }
        else if (b->x > POFV_FIELD_HALF_W) { b->x =  POFV_FIELD_HALF_W; b->dx = -b->dx; hit = 1; }
        if (b->y < 0.0f)                   { b->y = 0.0f;   b->dy = -b->dy; hit = 1; }
        else if (b->y > POFV_FIELD_H)      { b->y = POFV_FIELD_H; b->dy = -b->dy; hit = 1; }
        if (hit && ++b->bounces > POFV_C3_BOUNCE_MAX) {
            if (b->vm) { ce_anm_delete((uint32_t)b->vm); b->vm = 0; }
            b->live = 0;
            continue;
        }
        if (b->age >= POFV_C3_BULLET_LIFE) {              /* ❓⑥ 兜底寿命 */
            if (b->vm) { ce_anm_delete((uint32_t)b->vm); b->vm = 0; }
            b->live = 0;
            continue;
        }
        if (b->vm) ce_anm_set_pos((uint32_t)b->vm, b->x, b->y, 0.0f);
        pos[0] = b->x; pos[1] = b->y; pos[2] = 0.0f;
        (void)ce_damage_rect(pos, 0.0f, POFV_C3_HIT_LIFE, POFV_C3_HIT_DMG, POFV_C3_HIT_W, POFV_C3_HIT_H);
    }
}

/* ================= ④ 罩子（防护罩，owner 2026-10-10 亲写）=================
 * ★ 一手（TH19，见 `HANDOFF-109-C1-C4.md` §1.4/§11）：
 *   · **C3/C4 才给盾**（一手 `FUN_0040b420` 全 .text 只有 C3/C4 两个调用点）；**挡 1 发**、**最优先吃盾**；
 *   · **破罩 = 类死亡三件事（全部一手）**：**禁主动移动 40 帧** + **无敌 100 帧** +
 *     **背向命中点位移**（速度 `2.0` 线性衰减 40 帧 × **128** 单位）。
 * ★ 本仓没有"玩家受击"钩子 ✗ ⇒ 挡弹照 **`blue_eyes.c`（青眼白龙）** 的范式：
 *   在半径内**提前消掉**来袭弹（★ 用卡自己的 `cancel_white_in_circle`，`max_count = 1` ⇒ 挡 1 发）。
 * ★ 无敌走声明式 **`ce_invuln_request(id, frames)`**（`sdk.h:1048`，注释里就写着用于 109 爆风 ✓）。
 * ❓自定（登记 TUNING）：
 *   ① 触发 = **结界张开的上升沿**（一手是"C3/C4 才给盾"，而 C3/C4 尚未实现）；
 *   ② "命中点"取不到（消弹接口不返回坐标 ✗）⇒ 位移方向先取**正下**；
 *   ③ 盾半径 / 视觉（一手是 `PlayerInf+0x18` 的对象，视觉=白圆+黄外发光，本轮先不做 VM）。*/
/* ★ C1/C2 的阈值（一手 `pl00.sht` 的 `+0x2c`/`+0x30` = 50/100 ⇒ ×10 = 500/1000，
 *   与 `S+0x24/0x28` 同量纲 0..2500 ✓，见 HANDOFF-109-C1-C4.md §1.5）。*/
#define POFV_C1_THRESH          500.0f
#define POFV_C2_THRESH          1000.0f
/* ★ ⑤ C3 的阈值（一手 `pl00.sht` 的 `+0x34` = 170 ⇒ ×10 = 1700，与 `S+0x24/0x28/0x2c/0x30`
 *   同量纲 0..2500 ✓；触发形状照 C1/C2 = **按蓄力档**、只在**上升**那一帧放一次 ✓）。*/
#define POFV_C3_THRESH          1700.0f
/* ★★ 一手 ✓ C4 门槛 = **2500**（= `S+0x38` × 10 ✓）—— ★ 它**恰好等于灵力满值** ✓ ⇒
 *   ★ 只有**灵力吸满**才够得着 C4 ✓（这正是 owner 定的「**灵力 = 蓄力上限**」的字面意思 ✓）。*/
#define POFV_C4_THRESH         2500.0f
#define POFV_BARRIER_R          40.0f
/* ★★ 2026-10-10 owner 实跑订正：「半径上感觉**大了 1.5 倍**」✗ —— 真因 = 外层曾写
 *   `scale = k × (128/96)` ✗（把"贴图是 128 px"错当成"要画得更大"），它的**外缘**于是落在
 *   `64 × 0.833 × 1.333 = 71` 单位 = **判定半径的 1.78 倍** ✗。
 * ★ 正确做法：**按"每张贴图里圆的可见外缘"换算**（判定=绘制 ✓，与暴风"同一个半径写 VM scale"同款 ✓）：
 *     · 内层 `POFV109_BARRIER_DISC` = **96×96** ⇒ 可见外缘 = 48 px ⇒ `scale = R / 48` ✓；
 *     · 外层 `POFV109_BARRIER_GLOW` = **128×128** ⇒ 外缘 = 64 px ⇒ `scale = (R × 1.25) / 64` ✓
 *       ⇒ ★ 外缘 = **判定半径的 1.25 倍** ✓（不再是 1.78 ✓）。*/
#define POFV_BARRIER_DISC_TEX_R    48.0f    /* 96/2 ✓ */
#define POFV_BARRIER_GLOW_TEX_R    64.0f    /* 128/2 ✓ */
#define POFV_BARRIER_GLOW_OUT       1.25f   /* ❓自定旋钮：外发光的外缘 = 判定半径 × 此值 */
#define POFV_BARRIER_VM_LAYER_DISC  6    /* ★ 内层：与世界层对齐（脚本里也写 layer(6) ✓）*/
#define POFV_BARRIER_VM_LAYER_GLOW  5    /* ★ 外层：低一层 ⇒ 画在内层下面 ✓ */    /* ❓自定：盾半径 */
#define POFV_BARRIER_BREAK_N    40       /* ★ 一手 ✓ 禁主动移动 40 帧 */
#define POFV_BARRIER_INVULN_N   100      /* ★ 一手 ✓ 无敌 100 帧 */
#define POFV_BARRIER_PUSH       128.0f   /* ★ 一手 ✓ 位移单位 */
#define POFV_BARRIER_PUSH_V     2.0f     /* ★ 一手 ✓ 初速（线性衰减到 0）*/
static int32_t s_barrier_vm_disc;        /* ★ 内层：白圆（脚本 428）*/
/* ★★ #5（owner ✓）：「**获取 109 的时候会给一个罩子**（对应 TH19 **开局给你一个罩子** ✓）」✓
 *   ⇒ 卡侧：`clear_card()` 里把这个标志置 1 ✓（它同时覆盖 **构造 / 换局 / 重开** ✓，都是"重新拿到卡" ✓）
 *     ⇒ ★ 等到 tick 里**自机存在**时再发 ✓（★ 构造那一瞬没有自机 ✗，不能直接起罩 ✓）。*/
static int s_barrier_grant_pending;
static int32_t s_barrier_vm_glow;        /* ★ 外层：黄外发光（脚本 429）*/
static int   s_barrier_on;               /* 盾在不在 */
static int   s_barrier_brk;              /* 破罩计时（>0 = 禁移 + 位移中）*/
static float s_barrier_dx, s_barrier_dy; /* ❓自定：位移方向（先取正下）*/

/* ★ 找半径内**最近**的一颗可消弹（给破罩的"背向命中点位移"用 ✓）。
 *   一手：方向 = `atan2(自机y − 命中y, 自机x − 命中x)`（= **背离命中点** ✓）。
 *   ★ 本仓不许 `atan2f` ✗（守 `dllx87`）⇒ 用 `s_c2_dir36` **查表取最贴的那个方向** ✓。
 *   扫弹式照 `cancel_white_in_circle`（同款状态/半径/可消判据 ✓）。返回 1 = 找到。*/
static int pofv_barrier_nearest_bullet(const float *pos, float r, float *ox, float *oy)
{
    uint8_t *bm = CE_BULLET_MGR();
    uint32_t i;
    float best = -1.0f;                       /* ★ 取"离自机最近"= d2 最小 */
    float bx = 0.0f, by = 0.0f;
    int found = 0;
    if (!bm || !pos) return 0;
    for (i = 0; i < CE_BM_BULLET_COUNT; ++i) {
        uint8_t *b = bm + CE_BM_BULLETS + (size_t)i * CE_BULLET_STRIDE;
        uint16_t state = *(const uint16_t *)(b + CE_BULLET_STATE);
        float br, dx, dy, rr, d2;
        if (state != 1 && state != 2) continue;
        br = *(const float *)(b + POFV_BULLET_RADIUS_OFF);
        if (!pofv_is_white_bullet(br)) continue;
        dx = *(const float *)(b + CE_BULLET_POS) - pos[0];
        dy = *(const float *)(b + CE_BULLET_POS + 4) - pos[1];
        rr = br * 0.5f + r;
        d2 = dx * dx + dy * dy;
        if (d2 > rr * rr) continue;
        if (!found || d2 < best) { best = d2; bx = dx; by = dy; found = 1; }
    }
    if (found) { *ox = bx; *oy = by; }
    return found;
}

/* ★ 把"从自机指向命中弹"的向量**取反**、再量化到 `s_c2_dir36` 的 36 向里 ✓。
 *   ★ 只比较**方向** ⇒ 直接比点积即可、**不需要归一化** ✓（点积对长度单调 ✓）；
 *   ★ 取反 = 最大化 `dot(−d, table[i])` = **最小化** `dot(d, table[i])` ✓。★ 不引 `atan2f` ✗。*/
static void pofv_barrier_dir_from(float dx, float dy)
{
    int i, bi = 0;
    float bs = 1e30f;
    if (dx == 0.0f && dy == 0.0f) { s_barrier_dx = 0.0f; s_barrier_dy = 1.0f; return; }
    for (i = 0; i < 36; ++i) {
        float dot = dx * s_c2_dir36[i][0] + dy * s_c2_dir36[i][1];
        if (dot < bs) { bs = dot; bi = i; }
    }
    s_barrier_dx = s_c2_dir36[bi][0];
    s_barrier_dy = s_c2_dir36[bi][1];
}

/* ★ 罩子「有没有」✓：★ 就是那个**标识 VM** ✓ —— owner：「它只是**一个标识我有罩子保护的 VM**」✓，★ 没有碰撞箱 ✓。*/
static int pofv_barrier_have(void) { return s_barrier_vm_disc != 0; }

static void pofv_barrier_clear(void)
{
    if (s_barrier_vm_disc) { ce_anm_delete((uint32_t)s_barrier_vm_disc); s_barrier_vm_disc = 0; }
    if (s_barrier_vm_glow) { ce_anm_delete((uint32_t)s_barrier_vm_glow); s_barrier_vm_glow = 0; }
    s_barrier_on = 0; s_barrier_brk = 0; s_barrier_dx = 0.0f; s_barrier_dy = 0.0f;
}

/* ★ 罩子的视觉（owner：**一个白色透明圆形 + 外面黄色的外发光** ✓，参考咲夜的固灵领域 ✓）：
 *   内层 `POFV109_BARRIER_DISC`（96×96 白圆，脚本 428）+ 外层 `POFV109_BARRIER_GLOW`
 *   （128×128 黄空心环，脚本 429）⇒ ★ 两台 VM 同位置、**外层 scale 更大** ✓。
 *   ❓自定：半径取 `POFV_BARRIER_R`（= 判定半径 ✓，一手是 `PlayerInf+0x18` 对象，观感未取）。*/
static void pofv_barrier_vm_sync(void)
{
    const float *p = ce_player_pos();
    float k_disc, k_glow;
    if (!p) return;
    /* ★ 判定=绘制 ✓：两张贴图各自按"圆/带的外缘"换算（见上面那组常量的推导 ✓）*/
    k_disc = 1.0f;   /* 2026-10-11 owner 实跑：观感大了一倍 => 2.0 -> 1.0 */
    /* ★★ 2026-10-11 owner 口径落地 ✓：★ 外发光改用图元 `drawRing(28,8,24)` ✓
     *   ⇒ ★ **图元的 `scale` 就是半径** ✓（391 的头注 ✓）⇒ 这里除的是**图元声明的半径 28** ✓
     *   （不再是贴图半径 64 ✗）。*/
    /* ★★ 2026-10-11 owner 实跑 + 一手订正 ✓：★ 一手 `effect.anm` 里环半径写死 **28**、
     *   ★ 罩子侧给 **2× 缩放** ⇒ 屏上 **56 px** ✓（owner 量的就是 56 ✓）⇒ ★ 这里不再按 R 换算 ✗。*/
    k_glow = 1.0f;   /* 2026-10-11 owner 实跑：观感大了一倍 => 2.0 -> 1.0 */
    if (!s_barrier_vm_glow)
        s_barrier_vm_glow = (int32_t)ce_anm_spawn(CE_ABILITY_ANM(),
                             CE_ANM_ABILITY_SCRIPT_POFV109_BARRIER_RING, POFV_BARRIER_VM_LAYER_GLOW);   /* ★ owner 口径：黄环 = `drawRing(28,8,24)` ✓ */
    if (!s_barrier_vm_disc)
        /* 2026-10-11：整罩已烤进 446（含白心）=> 白盘 VM 不再起（否则两层白心叠着）。*/
        s_barrier_vm_disc = 0;   /* 2026-10-11：整罩已烤进 446，这里留着同位置再平涂一次（无害）*/
    if (s_barrier_vm_glow) {
        vm_set_world_pos((uint32_t)s_barrier_vm_glow, p[0], p[1]);
        ce_anm_set_scale((uint32_t)s_barrier_vm_glow, k_glow, k_glow);
    }
    if (s_barrier_vm_disc) {
        vm_set_world_pos((uint32_t)s_barrier_vm_disc, p[0], p[1]);
        ce_anm_set_scale((uint32_t)s_barrier_vm_disc, k_disc, k_disc);
    }
}

/* ★ 给盾（一手：C3/C4 才给；本卡先挂在结界张开上升沿 ❓）*/
/* ★★ #5：★ tick 里消费"开局/获取时给一个罩" ✓（★ 有自机才发 ✓）。*/
static void pofv_barrier_fire(void);   /* ★ 定义在后（`grant_tick` 要用它 ✓）*/
static void pofv_barrier_grant_tick(void)
{
    const float *p = ce_player_pos();
    if (!s_barrier_grant_pending || !p) return;
    s_barrier_grant_pending = 0;
    pofv_barrier_fire();
}

static void pofv_barrier_fire(void)
{
    s_barrier_on = 1;
}

/* ★ 每帧：盾挡 1 发 ⇒ 破罩；破罩后跑那三件事 */
static void pofv_barrier_tick(void)
{
    pofv_barrier_grant_tick();   /* ★ #5：获取 109 ⇒ 给一个罩子 ✓ */
    const float *pp = ce_player_pos();
    if (!pp) return;
    if (s_barrier_on) {
        /* ★ 挡弹 = 在半径内提前消掉（★ 只消 1 颗 ⇒ 挡 1 发 ✓）*/
        /* ★★★ 2026-10-11 owner 口径（#3）：「罩子**没有碰撞箱**，它只是一个**标识"我有罩子保护"的 VM**」✓
         *   ⇒ ★ **撤掉这个消弹圈** ✗ —— ★ 它正是**唯一"像碰撞箱"的东西** ✓，
         *     而且 ★ `POFV_BARRIER_R = 40` 这个数**是我当初凑的** ❓自定 ✗（一手没读到 ✓）。
         *   ★ 保留的（★ 都是**一手常数** ✓）：★ **无敌 `POFV_BARRIER_INVULN_N` = 100 帧** ✓、
         *     ★ **禁主动移动 40 帧** ✓、★ **位移 128 / 初速 2.0 线性衰减** ✓、★ 以及那个**标识 VM** ✓。
         *   ★ 下面那个 `pofv_barrier_nearest_bullet(...)` 只为**给位移定方向** ✓（★ 它**不消弹** ✗）⇒ 保留 ✓。
         *   ❓若你的意思是"**连无敌也不给**"（纯标识 ✓），★ 说一声我把 `ce_invuln_request` 那行也收了 ✓。*/
        int n = 0;
        /* ★★★ 2026-10-11 owner：短周期重发（一手是"罩子侧不断重发同一支演出弹" ✓，
         *   脚本 10 帧就完 ✓）—— 我原来是"起一次、6 帧淡出自删" ✗ ⇒ 中间会空 ✗。
         *   ⇒ 这里每 8 帧重发一台 446（= 整罩烤图，含白心 ✓）。*/
        {
            static int s_re;
            if (--s_re <= 0) {
                if (s_barrier_vm_glow && ce_anm_get_vm((uint32_t)s_barrier_vm_glow))
                    ce_anm_delete((uint32_t)s_barrier_vm_glow);
                s_barrier_vm_glow = (int32_t)ce_anm_spawn(CE_ABILITY_ANM(),
                                        CE_ANM_ABILITY_SCRIPT_POFV109_BARRIER_RING, POFV_BARRIER_VM_LAYER_GLOW);
                s_re = 8;
            }
        }
        pofv_barrier_vm_sync();   /* ★ 视觉：白圆 + 黄外发光（每帧跟随自机 ✓）*/
        if (n > 0) {
            s_barrier_on = 0;
            /* ★ 破罩 ⇒ 视觉先收（两台 VM 真删 ✓，铁律）*/
            if (s_barrier_vm_disc) { ce_anm_delete((uint32_t)s_barrier_vm_disc); s_barrier_vm_disc = 0; }
            if (s_barrier_vm_glow) { ce_anm_delete((uint32_t)s_barrier_vm_glow); s_barrier_vm_glow = 0; }
            s_barrier_brk = POFV_BARRIER_BREAK_N;
            /* ★★ 一手：背向**命中点**位移 ✓ ⇒ 用半径内最近那颗被消掉的弹当命中点 ✓
             *   （原来只有"正下" ❓ —— 已收掉 ✓）。找不到弹（理论上不该发生，因为刚挡到 ✓）
             *   ⇒ 退回正下 ✓。*/
            {
                float hx = 0.0f, hy = 0.0f;
                if (pofv_barrier_nearest_bullet(pp, POFV_BARRIER_R, &hx, &hy))
                    pofv_barrier_dir_from(hx, hy);
                else { s_barrier_dx = 0.0f; s_barrier_dy = 1.0f; }
            }
            ce_invuln_request(POFV_CARD_ID, POFV_BARRIER_INVULN_N);   /* ★ 一手 ✓ 无敌 100 帧 */
        }
    }
    if (s_barrier_brk > 0) {
        /* ★ 一手 ✓：禁主动移动（当帧写 0；`on_tick_2` 先于 `Player::on_tick` ⇒ 当帧生效）+ 背向位移 */
        float f = (float)(POFV_BARRIER_BREAK_N - s_barrier_brk);
        float v = POFV_BARRIER_PUSH_V * (1.0f - f / (float)POFV_BARRIER_BREAK_N);
        float *mult = (float *)(CE_PLAYER() + CE_PLAYER_SPEED_MULT);
        float *px;
        if (CE_PLAYER()) *mult = 0.0f;            /* ★ 一手 ✓：禁主动移动（当帧生效）*/
        px = (float *)(CE_PLAYER() + CE_PLAYER_X); /* ★ 与 `ce_player_pos()` 同一指针 ✓ */
        px[0] += s_barrier_dx * v * POFV_BARRIER_PUSH;
        px[1] += s_barrier_dy * v * POFV_BARRIER_PUSH;
        --s_barrier_brk;
    }
}

/* ================= ⑤ C4：灵梦 `Boss00BossCard1` 的「扫描弧」（一手 §14）=================
 * ★ 一手链路（`local/th19.v1.10c/_ext/ecl/pl00.txt`，亲读转写 ✓）：
 *   `BossAttack` → `ins_300("Boss00")` → `Boss00()` → `enmCreate("Boss00Boss")`
 *   → `Boss00Boss()` 按 **`[-9887]`（符卡号）** 分派 ⇒ ★ **`!23` = `Boss00BossCard1`** ✓。
 * ★ `Boss00BossCard1()`：★ **每 60 帧一轮** ✓，每轮调 **两次** `Boss00BossCard1_at` ✓：
 *     基准角 = `[-9989] − [-9987] × 0.34906584f`（**20°** ✓）；两次的 `B` 分别是
 *     **`+6.2831855f`（+2π）** 与 **`−6.2831855f`（−2π）** ⇒ ★ **两条反向的扫描** ✓。
 * ★ `Boss00BossCard1_at(A, B)`：★ 发 **`$D` 发**（`ins_570($D, **13,13,13,14,15,16,17,18**)` ✓），
 *     ★ **每 2 帧一发**（`ins_23(2)` ✓）、★ 角 `A += B/$D`（★ **把 ±2π 均分** ✓）、
 *     ★ **偶数发开弹槽 0 / 奇数发开弹槽 1**（★ 轮流 ✓）；每发：`etSprite(0,44,3)` ✓、
 *     `etAngle(A, 10°)` ✓、`etSpeed(**1.5 + [-9888]×0.8**, 1.0)` ✓，
 *     并挂三条 `etEx`：角度加速 / **`EX_ACCEL` 600 帧（`0.01 + [-9888]×0.02`、角 π/2）** ✓ /
 *     **`EX_BOUNCE`（type 64、4 次、walls 13）** ✓。
 * ★ **本卡的落地**（owner 路线：纯伤害源 + anm VM ✓）：
 *   ★ 照抄"每 60 帧、两条反向、13 发、每 2 帧一发、均分整圈"✓；
 *   ★ 每发 = **红环 VM（脚本 430）+ 伤害源** ✓，★ **自己算加速**（`POFV_C4_ACCEL` ✓）。
 * ❓自定（登记 TUNING）：
 *   ① `[-9888]` **未定位**（§10.5/§14.4 同一条，像 0..1 归一化标量）⇒ ★ **取 0** ✓；
 *   ② 基准角 `[-9989] − [-9987]×20°` 的两个变量**未定位** ⇒ ★ **本卡用"随轮次缓慢自转"近似** ✓
 *      （一手 `ins_91(1, %A, 3600, …)` 说明它确实在转 ✓）；
 *   ③ **`EX_BOUNCE` 不做** ✗（反弹要引擎弹 VM ✓；本卡是伤害源 ⇒ 出屏即收 ✓）；
 *   ④ `$D` 先固定 **13**（一手按档位 13..18 ✓）；
 *   ⑤ 弹的寿命/盒/伤害沿用 C1 的口径（可后调 ✓）。*/
#define POFV_C4_MAX_BULLETS      40
#define POFV_C4_SHOTS_PER_SWEEP  13       /* ❓④：一手 `ins_570` 的档 0..2 = 13（先固定 ✓）*/
#define POFV_C4_EMIT_EVERY        2       /* ★ 一手 ✓ `ins_23(2)` */
#define POFV_C4_WAVE_PERIOD      60       /* ★ 一手 ✓ 每 60 帧一轮 */
#define POFV_C4_SPEED           1.5f      /* ★ 一手 ✓ `1.5 + [-9888]×0.8`，❓① 取 0 ⇒ 1.5 */
#define POFV_C4_ACCEL          0.01f      /* ★ 一手 ✓ `0.01 + [-9888]×0.02`，❓① 取 0 ⇒ 0.01 */
#define POFV_C4_ACCEL_FRAMES     600      /* ★ 一手 ✓ */
#define POFV_C4_BULLET_LIFE      150      /* ❓⑤：出屏估 */
#define POFV_C4_VM_LAYER           6
#define POFV_C4_BASE_SPIN    0.0349066f   /* ❓②：每轮基准角 +2°（近似一手的缓慢自转）*/

typedef struct {
    int     live, age;
    float   x, y, ang, spd;
    int32_t vm;
} pofv_c4_bullet_t;

static pofv_c4_bullet_t s_c4_b[POFV_C4_MAX_BULLETS];
static struct { int timer, emitted, dir, live; float base; int eb, ring_vm; } s_c4;

static void pofv_c4_clear(void)
{
    pofv_eb_ring_close(&s_c4.ring_vm);   /* ★ 铁律：范围圈 VM 也收 ✓ */
    s_c4.eb = -1;
    int k;
    for (k = 0; k < POFV_C4_MAX_BULLETS; ++k) {
        if (s_c4_b[k].vm) { ce_anm_delete((uint32_t)s_c4_b[k].vm); s_c4_b[k].vm = 0; }
        s_c4_b[k].live = 0; s_c4_b[k].age = 0;
    }
    s_c4.timer = 0; s_c4.emitted = 0; s_c4.dir = 1; s_c4.live = 0; s_c4.base = 0.0f;
}

/* ★ 一轮开始（一手：每 60 帧调两次 `_at`，方向 +2π / −2π ✓）*/
static void pofv_c4_fire(void)
{
    s_c4.live = 1; s_c4.timer = 0; s_c4.emitted = 0; s_c4.dir = 1;
    s_c4.eb = 0;   /* ★ 一手的 `Ecl_EtBreakLv4`（16/24/32/40/48）从这一帧起 ✓ */
}

/* ★ 发一发（两条扫描共用：`dir` = ±1 ✓）*/
static void pofv_c4_emit(int dir)
{
    int k, i;
    float ang, di;
    for (k = 0; k < POFV_C4_MAX_BULLETS; ++k) if (!s_c4_b[k].live) break;
    if (k >= POFV_C4_MAX_BULLETS) return;
    /* ★ 角 = base + dir × 2π × (第几发 / D)（一手 `A += B/$D` 的均分 ✓）*/
    di = (float)s_c4.emitted / (float)POFV_C4_SHOTS_PER_SWEEP;
    if (dir >= 0) ang = s_c4.base + 6.2831853f * di;
    else          ang = s_c4.base - 6.2831853f * di;
    s_c4_b[k].live = 1; s_c4_b[k].age = 0;
    s_c4_b[k].x = 0.0f; s_c4_b[k].y = 128.0f;      /* ★ 一手：环心在 (0,128) ✓（`ins_504(0,128,…)`）*/
    s_c4_b[k].ang = ang;
    s_c4_b[k].spd = POFV_C4_SPEED;
    {
        int a5 = (int)(ang * (36.0f / 6.2831853f)) % 36;
        if (a5 < 0) a5 += 36;
        s_c4_b[k].x = 0.0f; s_c4_b[k].y = 128.0f;
        (void)a5;
    }
    s_c4_b[k].vm = (int32_t)ce_anm_spawn(CE_ABILITY_ANM(), POFV_C_RING_SCRIPT, POFV_C4_VM_LAYER);
    if (s_c4_b[k].vm) {
        ce_anm_set_color((uint32_t)s_c4_b[k].vm, 0xffffffffu);
        ce_anm_set_scale((uint32_t)s_c4_b[k].vm, 0.5f, 0.5f);
    }
}

/* ★ 每帧：波次机 + 逐发（自转/加速/出屏/寿命）*/
static void pofv_c4_tick(void)
{
    /* ★★ 一手 `Ecl_EtBreak`（§5.1 ✓）：**每帧一档**、共 POFV_EB_LV4_FRAMES 帧 ⇒ 消弹 + 撒道具 + 范围圈 ✓
     *   （★ 留本侧 ✓ —— 一手这一档会送对侧，本作是"我方场地"⇒ 不翻 ✓）。*/
    if (s_c4.eb >= 0 && s_c4.eb <= POFV_EB_LV4_FRAMES) {
        const float *pe = ce_player_pos();
        if (pe) {
            float pos[3];
            const float r = POFV_EB_LV4_R0 + POFV_EB_STEP * (float)s_c4.eb;
            pos[0] = pe[0]; pos[1] = pe[1]; pos[2] = 0.0f;
            (void)c2_cancel_drop(pos, r);
            pofv_eb_ring_sync(&s_c4.ring_vm, r);
        }
        ++s_c4.eb;
    } else if (s_c4.eb > 0) {
        pofv_eb_ring_close(&s_c4.ring_vm);
        s_c4.eb = -1;
    }
    int k;
    if (s_c4.live) {
        /* ★ 每 2 帧一发；两条扫描同时跑（一手每轮调两次 `_at` ✓）*/
        if (s_c4.timer % POFV_C4_EMIT_EVERY == 0) {
            if (s_c4.emitted < POFV_C4_SHOTS_PER_SWEEP) {
                pofv_c4_emit(+1);
                pofv_c4_emit(-1);
            }
            ++s_c4.emitted;
        }
        if (++s_c4.timer >= POFV_C4_WAVE_PERIOD) {
            s_c4.timer = 0; s_c4.emitted = 0; s_c4.live = 0;
            s_c4.base += POFV_C4_BASE_SPIN;         /* ❓②：基准角缓慢自转 ✓ */
        }
    }
    for (k = 0; k < POFV_C4_MAX_BULLETS; ++k) {
        pofv_c4_bullet_t *b = &s_c4_b[k];
        float pos[3];
        int di;
        if (!b->live) continue;
        /* ★ 加速（一手 `EX_ACCEL` 600 帧 ✓）*/
        if (b->age < POFV_C4_ACCEL_FRAMES) b->spd += POFV_C4_ACCEL;
        di = (int)(b->ang * (36.0f / 6.2831853f)) % 36;
        if (di < 0) di += 36;
        b->x += s_c2_dir36[di][0] * b->spd;
        b->y += s_c2_dir36[di][1] * b->spd;
        ++b->age;
        if (b->x < -POFV_FIELD_HALF_W || b->x > POFV_FIELD_HALF_W ||
            b->y < 0.0f || b->y > POFV_FIELD_H || b->age >= POFV_C4_BULLET_LIFE) {
            if (b->vm) { ce_anm_delete((uint32_t)b->vm); b->vm = 0; }
            b->live = 0;
            continue;
        }
        if (b->vm) {
            ce_anm_set_pos((uint32_t)b->vm, b->x, b->y, 0.0f);
            ce_anm_set_scale((uint32_t)b->vm, 0.5f + (float)b->age * 0.004f,
                             0.5f + (float)b->age * 0.004f);
        }
        pos[0] = b->x; pos[1] = b->y; pos[2] = 0.0f;
        (void)ce_damage_rect(pos, 0.0f, POFV_C1_HIT_LIFE, 14.0f, POFV_C1_HIT_W, POFV_C1_HIT_H);
    }
}

/* ================= ② C2 —— **魔理沙**分支：6 条激光（一手 `EXMarisaInf` 槽 4）=================
 * ★ 一手（`HANDOFF-109-C1-C4.md` §5.2）：`EXMarisaInf` 槽 4 = `0x417b90` ⇒ ★ **new 6 × `EXLaserInf`**
 *   （`0x77c`，vt `0x5a8b8c`）✓，★ 每条寿命 **60–159 帧随机**（`FUN_0053d280`：`(rand&0xffff)%100+0x3c` ✓）。
 * ★ 一手那 6 条的**位置/角度**在 `EXLaserInf` 的 init 里（★ 本卡未逐条核到 ⇒ ❓）。
 *   ⇒ ★ 本卡按"**6 条等分竖直激光、绕自机铺开**"近似 ✓（★ 一手是"沿 `y=448` 横扫的双镜像"同族 ✓）。
 * ★ 视觉用**本卡现成的魔理沙竖带**（脚本 403 = `AURA_MARISA`，`drawRectGrad(1.0f, 448)` ✓）
 *   —— 一手魔理沙的弹幕就是这种"横长条"✓，本卡照它的形状用 ✓。
 * ❓自定（登记 TUNING）：
 *   ① 6 条的**角度/间距** = 360°/6 等分（一手未核到 ⇒ 取等分）；
 *   ② 寿命取 **60 / 80 / 100 / 120 / 140 / 159**（★ 覆盖一手 60..159 的区间 ✓，★ 不用随机以利复现 ✓）；
 *   ③ 伤害沿用 C1 口径（一手 `EXLaserInf` 的伤害未核到）。*/
#define POFV_C2M_LASERS      6
#define POFV_C2M_VM_LAYER    6
#define POFV_C2M_HIT_LEW   12.0f     /* ❓：伤害源的宽（细）*/
/* ★★ 2026-10-11 照一手订正（子 agent 核实 ✓）：★ 魔理沙 C2 = **6 条 `EXLaserInf`** ✓，
 *   ★ 每条**两端点 = 自机 + 半径 600 的偏移** ✓ ⇒ 长度取 **600**（原 420 是我猜的 ❓ ✗）。
 *   ★ 角度 = **−90° + k·60°** ✓（原来我从 0° 起等分 ✗，差 90°）。
 *   ★ 寿命 = **60 + rand()%100** ✓（60..159）；★ **无 anm**（一手是引擎原生激光 ✗，
 *     本卡没有那种原语 ⇒ 仍用脚本 403 画，观感属 ❓自定 ✓）。*/
#define POFV_C2M_HIT_LEN   600.0f
/* ★★ 2026-10-11 照抄 108 的两个常量（owner：完全参考 108 ✓）：
 *   ★ 108 `youkai.c:8163` = `scale.x = YK_LASER_LEN / YK_M_LASER_BEAM_SPRITE_W × grow`
 *     = 2048 / 768 = **2.6666667** ✓；★ 展开帧数 = `YK_M_LASER_BEAM_GROW_FRAMES` = **8** ✓。*/
#define POFV_C2M_LEN_OVER_SPRITE  (2048.0f / 768.0f)
#define POFV_C2M_GROW_FRAMES      8
/* ★★ 审计 A ✓（108 `youkai.c:8163-8164`）：y 也用 `w_px / SPRITE_H` ✓ —— 108 的 `w_px`
 *   = `W_PX100[档] / 100` = 1600/100 = **16** ✓、`SPRITE_H` = **32** ✓ ⇒ 商 = **0.5** ✓；
 *   ★ 且**两轴都乘 grow** ✓。*/
#define POFV_C2M_W_OVER_SPRITE     (16.0f / 32.0f)
#define POFV_C2M_SETTLE           2      /* ★ 108 `YK_M_LASER_BEAM_SETTLE` = 2 ✓（伤害每 2 帧一次）*/

typedef struct {
    int     live, age, life;
    float   ang;
    float   x, y;
    int32_t vm;
} pofv_c2m_laser_t;

static pofv_c2m_laser_t s_c2m[POFV_C2M_LASERS];

static void pofv_c2m_clear(void)
{
    int k;
    for (k = 0; k < POFV_C2M_LASERS; ++k) {
        if (s_c2m[k].vm) { ce_anm_delete((uint32_t)s_c2m[k].vm); s_c2m[k].vm = 0; }
        s_c2m[k].live = 0; s_c2m[k].age = 0;
    }
}

static void pofv_c2m_fire(void)
{
    pofv_stage_set(17);   /* ★ 魔理沙 C2 fire ✓ */
    s_c1_m_set11 = 1;   /* ★ 一手 ✓ `EX+0xd4 = 1`：下一份 C1 追加 set11 两发 ✓ */

    const float *pp = ce_player_pos();
    int k;
    if (!pp) return;
    for (k = 0; k < POFV_C2M_LASERS; ++k) {
        pofv_c2m_laser_t *L = &s_c2m[k];
        int di;
        if (L->vm) { ce_anm_delete((uint32_t)L->vm); L->vm = 0; }
        /* ★★ 一手 ✓：★ 每条寿命 = **`60 + rand() % 100`** ✓（60..159）—— 原来是固定表 ❓ 已删 ✓。*/
        L->live = 1; L->age = 0; L->life = 60 + (int)(ce_rand() % 100u);
        L->ang = -1.5707963f + 6.2831853f * (float)k / (float)POFV_C2M_LASERS;   /* ★ 一手 ✓ −90° + k·60° */
        L->x = pp[0]; L->y = pp[1];
        di = (int)(L->ang * (36.0f / 6.2831853f)) % 36;
        if (di < 0) di += 36;
        (void)di;
        L->vm = (int32_t)ce_anm_spawn(CE_ABILITY_ANM(), CE_ANM_ABILITY_SCRIPT_POFV109_C2M_BEAM,
                                      POFV_C2M_VM_LAYER);
        if (L->vm) {
            ce_anm_set_pos((uint32_t)L->vm, L->x, L->y, 0.0f);
            ce_anm_set_scale((uint32_t)L->vm, 0.6f, 1.0f);
        }
    }
}

static void pofv_c2m_tick(void)
{
    int k;
    for (k = 0; k < POFV_C2M_LASERS; ++k) {
        pofv_c2m_laser_t *L = &s_c2m[k];
        float pos[3];
        if (!L->live) continue;
        ++L->age;
        if (L->age > L->life) {
            if (L->vm) { ce_anm_delete((uint32_t)L->vm); L->vm = 0; }
            L->live = 0;
            continue;
        }
        if (L->vm) {
            /* ★★★ 2026-10-11 照抄 108（owner：「完全参考 108…」✓）：
             *   · ★ 长度 = `YK_LASER_LEN / SPRITE_W × grow`（108 `youkai.c:8163` ✓）
             *     = **2048 / 768 × grow = 2.6667 × grow** ✓；grow 在 **8 帧**内 0→1 ✓
             *     （108 `YK_M_LASER_BEAM_GROW_FRAMES = 8` ✓）；
             *   · ★ 宽度用脚本给的 `scale(1.0, 0.5)` ✓ ⇒ 卡侧只写 x… ✗ 不行
             *     （`set_scale` 两轴一起写 ✓）⇒ y 也照 0.5 写 ✓；
             *   · ★ 角度：脚本初始 `rotate(-π/2)` = 朝上 ✓，`anchor(1,0)` 左端锚 ✓
             *     ⇒ 要指向 `L->ang`（一手：-90° = 朝上 ✓）取 **`rz = L->ang + π/2`** ✓。*/
            const float grow = (L->age >= POFV_C2M_GROW_FRAMES)
                             ? 1.0f : (float)L->age / (float)POFV_C2M_GROW_FRAMES;
            ce_anm_set_pos((uint32_t)L->vm, L->x, L->y, 0.0f);
            /* ★★ 审计 A 订正 ✓（108 `youkai.c:8163-8164` 原文两轴都乘 grow ✓）：
             *   `set_scale(id, LEN/SPRITE_W × grow, w_px/SPRITE_H × grow)` ✓
             *   ⇒ ★ 我原来 y 写常数 0.5 ✗（少了 grow ✓）。`w_px = 1600/100 = 16` ✓、
             *     `SPRITE_H = 32` ✓ ⇒ **y = 0.5 × grow** ✓。*/
            ce_anm_set_scale((uint32_t)L->vm, POFV_C2M_LEN_OVER_SPRITE * grow,
                             POFV_C2M_W_OVER_SPRITE * grow);
            ce_anm_set_rotation((uint32_t)L->vm, 0.0f, 0.0f, L->ang + 1.5707964f);
        }
        /* ★ 伤害源：绕自机、沿该角度铺一条长盒（`ce_damage_rect` 不转 ⇒ 用等分角度近似 ❓）*/
        pos[0] = L->x; pos[1] = L->y; pos[2] = 0.0f;
        /* ★ 审计 A ✓：108 是**每 `SETTLE`=2 帧**建一次源（`youkai.c:8399` ✓）⇒ 照做 ✓。*/
        if ((L->age % POFV_C2M_SETTLE) == 0)
            (void)ce_damage_rect(pos, L->ang, POFV_C2M_SETTLE, 14.0f, POFV_C2M_HIT_LEW, POFV_C2M_HIT_LEN);
    }
}

/* ================= ⑤ C3 —— **魔理沙**分支：沿 `y = 448` 横扫的双镜像激光 ====================
 * ★ 一手（`HANDOFF-109-C1-C4.md` §10.2）：`pl01.ecl` 的 `ExtraAttack` ★ **是纯激光** ✓
 *   —— ★ **没有 `etOn`、也没有任何 `etEx`** ✗，只有 `ins_700`（建激光）/`ins_701`（时机）/`ins_703`（开）✓。
 *   ★ 载体**每步移动**：`movePos(%G, 448)` → 开一发；`movePos(−%G, 448)` → **镜像再开一发** ✓。
 * ★ 表（★ 一手逐条照抄，§10.3 ✓）：
 *   `$C`（每波步数）= `{3,3,5,4,3,3,4,5}`；`$D`（波数）= `{1,2,1,2,3,3,2,2}`；
 *   `$E`（每步 wait）= `{40,35,30,25,17,15,19,18}`；`%F`（收窄量）= `{8,16,8,8,16,12,24,24}` ✓。
 *   ★ 每波：`%G = 140 − $H×%F`；★ 每步：`%G −= 125/($C−1)` ✓。
 * ★ 视觉复用**脚本 403**（魔理沙竖带 ✓）；★ 激光从 `(±%G, 448)` **向上**打 ⇒ 一条长盒 ✓。
 * ❓自定：① 档位取 **1**（`$D=2, $C=3, $E=35, %F=16` ✓，一手多为 1/3）；② 激光寿命按 wait 近似 ✓；
 *         ③ 伤害沿用 C1 口径（一手 `ins_700/701` 的伤害未核到 ✓）。*/
#define POFV_C3M_MAX_BEAMS   40
#define POFV_C3M_VM_LAYER     6
#define POFV_C3M_X0       140.0f          /* ★ 一手 ✓ 起始 `%G = 140` */
#define POFV_C3M_Y        448.0f          /* ★ 一手 ✓ 横扫所在行 */
#define POFV_C3M_XSPAN    125.0f          /* ★ 一手 ✓ 每波收窄 125 */

static const unsigned char s_c3m_steps[8] = { 3, 3, 5, 4, 3, 3, 4, 5 };   /* $C ✓ */
static const unsigned char s_c3m_waves[8] = { 1, 2, 1, 2, 3, 3, 2, 2 };   /* $D ✓ */
static const unsigned char s_c3m_wait [8] = { 40,35,30,25,17,15,19,18 };  /* $E ✓ */
static const unsigned char s_c3m_shrink[8] = { 8,16,8,8,16,12,24,24 };    /* %F ✓ */

typedef struct { int live, age, life; float x, y; int32_t vm; } pofv_c3m_beam_t;
static pofv_c3m_beam_t s_c3m[POFV_C3M_MAX_BEAMS];
static struct { int active, wave, step, timer, level; float g; } s_c3m_st;

static void pofv_c3m_clear(void)
{
    int k;
    for (k = 0; k < POFV_C3M_MAX_BEAMS; ++k) {
        if (s_c3m[k].vm) { ce_anm_delete((uint32_t)s_c3m[k].vm); s_c3m[k].vm = 0; }
        s_c3m[k].live = 0; s_c3m[k].age = 0;
    }
    s_c3m_st.active = 0; s_c3m_st.wave = 0; s_c3m_st.step = 0;
    s_c3m_st.timer = 0; s_c3m_st.g = 0.0f;
}

/* ★ 开一条激光在 `(x, 448)`，向上 ✓ */
static void pofv_c3m_beam(float x)
{
    int k;
    for (k = 0; k < POFV_C3M_MAX_BEAMS; ++k) if (!s_c3m[k].live) break;
    if (k >= POFV_C3M_MAX_BEAMS) return;
    s_c3m[k].live = 1; s_c3m[k].age = 0;
    s_c3m[k].life = 90;                     /* ❓②：出屏/寿命上限 */
    s_c3m[k].x = x; s_c3m[k].y = POFV_C3M_Y;
    s_c3m[k].vm = (int32_t)ce_anm_spawn(CE_ABILITY_ANM(), CE_ANM_ABILITY_SCRIPT_POFV109_AURA_MARISA,
                                        POFV_C3M_VM_LAYER);
    if (s_c3m[k].vm) {
        ce_anm_set_pos((uint32_t)s_c3m[k].vm, x, POFV_C3M_Y, 0.0f);
        ce_anm_set_scale((uint32_t)s_c3m[k].vm, 0.5f, 1.0f);
    }
}

static void pofv_c3m_fire(void)
{
    s_c3m_st.active = 1; s_c3m_st.level = 1;     /* ❓①：档位取 1 */
    s_c3m_st.wave = 0; s_c3m_st.step = 0; s_c3m_st.timer = 0;
    s_c3m_st.g = POFV_C3M_X0;                    /* %G = 140 − 0×%F ✓ */
}

static void pofv_c3m_tick(void)
{
    int k;
    if (s_c3m_st.active) {
        const unsigned char *T = s_c3m_steps;    /* 简记 */
        const int lv = s_c3m_st.level;
        if (s_c3m_st.step < (int)T[lv]) {
            if (s_c3m_st.timer == 0) {
                /* ★ 一手：`movePos(%G,448)` 开一发、`movePos(−%G,448)` **镜像**再开一发 ✓ */
                pofv_c3m_beam( s_c3m_st.g);
                pofv_c3m_beam(-s_c3m_st.g);
                if (s_c3m_st.step + 1 < (int)T[lv])
                    s_c3m_st.g -= POFV_C3M_XSPAN / (float)((int)T[lv] - 1);   /* ★ 一手 ✓ */
            }
            if (++s_c3m_st.timer >= (int)s_c3m_wait[lv]) {
                s_c3m_st.timer = 0; ++s_c3m_st.step;
            }
        } else if (++s_c3m_st.wave < (int)s_c3m_waves[lv]) {
            s_c3m_st.step = 0; s_c3m_st.timer = 0;
            s_c3m_st.g = POFV_C3M_X0 - (float)s_c3m_st.wave * (float)s_c3m_shrink[lv];  /* ★ 一手 ✓ */
        } else {
            s_c3m_st.active = 0;
        }
    }
    for (k = 0; k < POFV_C3M_MAX_BEAMS; ++k) {
        pofv_c3m_beam_t *b = &s_c3m[k];
        float pos[3];
        if (!b->live) continue;
        ++b->age;
        if (b->age > b->life) {
            if (b->vm) { ce_anm_delete((uint32_t)b->vm); b->vm = 0; }
            b->live = 0;
            continue;
        }
        pos[0] = b->x; pos[1] = b->y; pos[2] = 0.0f;
        /* ★ 向上的一条长盒（`ce_damage_rect` 不转 ⇒ 竖直用宽 12 / 高 420 ❓③）*/
        (void)ce_damage_rect(pos, 0.0f, 2, 14.0f, 12.0f, 420.0f);
    }
}

/* ============ ⑤ C4 —— **魔理沙**分支：一条激光 + 一串扫射弹（一手 pl01 `Boss00BossCard1`）============
 * ★ 一手（亲读 `local/th19.v1.10c/_ext/ecl/pl01.txt:229/368`）：★ 她的 `Boss00BossCard1_at()`
 *   **无参数** ✗、且与灵梦**完全不同** ✗：
 *   · **槽 0 = 激光**：`etSprite(38,10)` + `etAngle(−π/2 + [-9987]×**10°**, 同)` + `etSpeed(8,8)`
 *     + **`ins_700(0, 512, 512, 0, 14)`（建激光）** + **`ins_701(0, 60, 10, 60, 10, 0)`（时机）**
 *     + `ins_608(0,18,-1)`（音） + `ins_604(A,%D,0)` + `ins_603(A, 0, 8)` + **`ins_703(A, 0)`（开）** ✓；
 *   · **槽 1 = 扫射弹**：`etSprite(16,13)` + `etCount(1,1)` + `etAngle(%D, 0.05235988f = **3°**)`
 *     + `etSpeed(1.5 + C×0.8, 1.0)` + 三条 `etEx`（`(0,2097152,20)` / **`EX_ACCEL (4, 120)`**）✓；
 *   · **8 档表**：发数 `ins_570($G, **25,30,15,20,30,30,30,30**)` ✓、速度 `ins_571(%H, **4,4,3,4,4,4,4,4**)` ✓；
 *   · **循环**：★ 每 **2 帧**一发（`ins_23(2)` ✓）、角 `(%D + π) + [-9987]×π/2` ✓、速 `%H + [-9999]×1.0`
 *     （★ 末层 **0.1** ✓）；★ 外循环每 **60 帧**一轮（`ins_23(60)` ✓）。
 * ⇒ ★ 本卡落地：★ **一条向上的激光 + 一串绕圈扫射的弹** ✓（★ 每次 C4 = 一轮 60 帧的扫射 ✓）。
 * ❓自定：① 档位取 **0**（`$G=25` 发 / `%H=4.0` ✓）；② `[-9987]`/`[-9989]`/`[-9999]` 未定位
 *          ⇒ ★ 本卡取 0（⇒ 激光角 = 正上 ✓、扫射基准角 = π ✓）；③ 伤害沿用 C1 口径 ✓。*/
#define POFV_C4M_MAX        48
#define POFV_C4M_SPEED     4.0f      /* ★ 一手 ✓ `ins_571` 的档 0 = 4.0 */
#define POFV_C4M_LASER_SPD 8.0f      /* ★ 一手 ✓ */
#define POFV_C4M_CYCLE       60      /* ★ 一手 ✓ 每 60 帧一轮 */
#define POFV_C4M_EVERY        2      /* ★ 一手 ✓ `ins_23(2)` */
#define POFV_C4M_LASER_LIFE  60      /* ❓：一手 `ins_701(60,…)` 的时机参数 */

typedef struct { int live, age, life; float x, y, ang, spd; int32_t vm; } pofv_c4m_t;
static pofv_c4m_t s_c4m[POFV_C4M_MAX];
static struct { int active, emitted, timer, laser_vm, laser_timer, level; float base; } s_c4m_st;

static void pofv_c4m_clear(void)
{
    int k;
    for (k = 0; k < POFV_C4M_MAX; ++k) {
        if (s_c4m[k].vm) { ce_anm_delete((uint32_t)s_c4m[k].vm); s_c4m[k].vm = 0; }
        s_c4m[k].live = 0; s_c4m[k].age = 0;
    }
    if (s_c4m_st.laser_vm) { ce_anm_delete((uint32_t)s_c4m_st.laser_vm); s_c4m_st.laser_vm = 0; }
    s_c4m_st.active = 0; s_c4m_st.emitted = 0; s_c4m_st.timer = 0;
    s_c4m_st.laser_timer = 0; s_c4m_st.base = 0.0f;
}

static void pofv_c4m_fire(void)
{
    const float *pp = ce_player_pos();
    s_c4m_st.active = 1; s_c4m_st.emitted = 0; s_c4m_st.timer = 0;
    s_c4m_st.level = 0;                       /* ❓① */
    s_c4m_st.base = 3.1415927f;               /* ❓②：一手 `%D`（`[-9989] + [-9987]×5°`），取 π ✓ */
    /* ★ 槽 0：一条**向上的激光**（用魔理沙竖带当激光 ✓），寿命 = `ins_701` 的 60 ✓ */
    if (s_c4m_st.laser_vm) { ce_anm_delete((uint32_t)s_c4m_st.laser_vm); s_c4m_st.laser_vm = 0; }
    if (pp) {
        s_c4m_st.laser_vm = (int32_t)ce_anm_spawn(CE_ABILITY_ANM(),
                             CE_ANM_ABILITY_SCRIPT_POFV109_AURA_MARISA, POFV_C3M_VM_LAYER);
        if (s_c4m_st.laser_vm) {
            ce_anm_set_pos((uint32_t)s_c4m_st.laser_vm, pp[0], pp[1], 0.0f);
            ce_anm_set_scale((uint32_t)s_c4m_st.laser_vm, 0.4f, 1.0f);
        }
    }
    s_c4m_st.laser_timer = POFV_C4M_LASER_LIFE;
}

static void pofv_c4m_emit(float ang)
{
    int k;
    if (s_c4m_st.laser_vm == 0 && 0) return;
    for (k = 0; k < POFV_C4M_MAX; ++k) if (!s_c4m[k].live) break;
    if (k >= POFV_C4M_MAX) return;
    s_c4m[k].live = 1; s_c4m[k].age = 0; s_c4m[k].life = 150;
    s_c4m[k].ang = ang; s_c4m[k].spd = POFV_C4M_SPEED;
    {
        const float *pp = ce_player_pos();
        s_c4m[k].x = pp ? pp[0] : 0.0f;
        s_c4m[k].y = pp ? pp[1] : 0.0f;
    }
    s_c4m[k].vm = (int32_t)ce_anm_spawn(CE_ABILITY_ANM(), POFV_C_RING_SCRIPT, POFV_C4_VM_LAYER);
    if (s_c4m[k].vm) {
        ce_anm_set_color((uint32_t)s_c4m[k].vm, 0xffffffffu);
        ce_anm_set_scale((uint32_t)s_c4m[k].vm, 0.4f, 0.4f);
    }
}

static void pofv_c4m_tick(void)
{
    int k;
    static const int cnt[8] = { 25, 30, 15, 20, 30, 30, 30, 30 };   /* ★ 一手 ✓ `ins_570` */
    if (s_c4m_st.active) {
        const int n = cnt[s_c4m_st.level];
        if (s_c4m_st.emitted < n) {
            if (s_c4m_st.timer % POFV_C4M_EVERY == 0) {
                /* ★ 一手：每 2 帧一发，角度 = base + π + 递增（本卡按"每发 +15°"铺开 ❓）*/
                pofv_c4m_emit(s_c4m_st.base + 3.1415927f + 0.2617994f * (float)s_c4m_st.emitted);
            }
            ++s_c4m_st.emitted;
        }
        if (++s_c4m_st.timer >= POFV_C4M_CYCLE) { s_c4m_st.active = 0; s_c4m_st.timer = 0; }
    }
    if (s_c4m_st.laser_timer > 0) {
        --s_c4m_st.laser_timer;
        if (s_c4m_st.laser_timer == 0 && s_c4m_st.laser_vm) {
            ce_anm_delete((uint32_t)s_c4m_st.laser_vm); s_c4m_st.laser_vm = 0;
        }
        {
            const float *pp = ce_player_pos();
            float pos[3];
            if (pp && s_c4m_st.laser_vm)
                ce_anm_set_pos((uint32_t)s_c4m_st.laser_vm, pp[0], pp[1], 0.0f);
            if (pp) {
                pos[0] = pp[0]; pos[1] = pp[1] - 210.0f; pos[2] = 0.0f;
                (void)ce_damage_rect(pos, 0.0f, 2, 14.0f, 16.0f, 420.0f);   /* ❓③ 竖直长盒 ✓ */
            }
        }
    }
    for (k = 0; k < POFV_C4M_MAX; ++k) {
        pofv_c4m_t* b = &s_c4m[k];
        float pos[3];
        int di;
        if (!b->live) continue;
        ++b->age;
        if (b->age > b->life) {
            if (b->vm) { ce_anm_delete((uint32_t)b->vm); b->vm = 0; }
            b->live = 0; continue;
        }
        di = (int)(b->ang * (36.0f / 6.2831853f)) % 36; if (di < 0) di += 36;
        b->x += s_c2_dir36[di][0] * b->spd;
        b->y += s_c2_dir36[di][1] * b->spd;
        if (b->x < -POFV_FIELD_HALF_W || b->x > POFV_FIELD_HALF_W ||
            b->y < 0.0f || b->y > POFV_FIELD_H) {
            if (b->vm) { ce_anm_delete((uint32_t)b->vm); b->vm = 0; }
            b->live = 0; continue;
        }
        if (b->vm) ce_anm_set_pos((uint32_t)b->vm, b->x, b->y, 0.0f);
        pos[0] = b->x; pos[1] = b->y; pos[2] = 0.0f;
        (void)ce_damage_rect(pos, 0.0f, 2, 14.0f, 16.0f, 16.0f);
    }
}

/* ============ ② C2 —— **早苗**分支：更慢的 26 发整圈（一手 = shot 行 11）====================
 * ★ 一手（`HANDOFF-109-C1-C4.md` §5.2）：★ `EXSanaeInf` 槽 4 = `0x418d20` **不建对象** ✗ ——
 *   它做的是：★ 复位 C1 组、★ 把 **`EX+0xd8` 设成 `11`**（= ★ **改用 shot 行 11** ✓）、
 *   ★ 快照自机坐标（写 `PlayerInf+0xc18/c1c = pos×128` ✓）⇒ ★ **走真自机弹系统** ✓。
 * ★ 本卡不用真弹 ⇒ ★ **按 shot 行 11 的图案复刻** ✓。已核到的值（与 C1 的 set10 同构 ✓）：
 *     `pl02` **set10**（= 她的 C1）= 26 行 / 伤害 **22** / 速度 **16** / anm 7 / 角 −180° 起每 30° ✓；
 *     `pl02` **set11**（= 她的 C2）= 26 行 / **速度 3** / **anm 9** / **cls 5**（= 子机槽 5 ✓）。
 *   ⇒ ★★ **她的 C2 = 与 C1 同一個 26 发整圈、但慢得多（3 vs 16）** ✓。
 * ★ 一手的 `rate = 16` ⇒ ★ **每 16 帧放一圈** ✓ ⇒ ★ 本卡按"**放 3 圈**（≈ 48 帧）"近似 ✓（❓）。
 * ❓自定：① 圈数 = 3；② 伤害沿用 C1 的 22（一手 set11 的伤害那格没读出来 ✗）；
 *         ③ 视角用红环（脚本 430）✓（一手 anm 9 未取 ✗）。*/
#define POFV_C2S_MAX      60
#define POFV_C2S_SPEED   3.0f      /* ★ 一手 ✓ set11 的速度 */
#define POFV_C2S_COUNT    26       /* ★ 一手 ✓ set11 的行数 */
#define POFV_C2S_RINGS     3       /* ❓① */
#define POFV_C2S_EVERY    16       /* ★ 一手 ✓ rate 16 ⇒ 每 16 帧一圈 */
#define POFV_C2S_DMG      22.0f    /* ❓②：沿用她 C1 的 22 */

typedef struct { int live, age, life; float x, y, vx, vy; int32_t vm; } pofv_c2s_t;
static pofv_c2s_t s_c2s[POFV_C2S_MAX];
static struct { int active, ring, timer; } s_c2s_st;

static void pofv_c2s_clear(void)
{
    int k;
    for (k = 0; k < POFV_C2S_MAX; ++k) {
        if (s_c2s[k].vm) { ce_anm_delete((uint32_t)s_c2s[k].vm); s_c2s[k].vm = 0; }
        s_c2s[k].live = 0; s_c2s[k].age = 0;
    }
    s_c2s_st.active = 0; s_c2s_st.ring = 0; s_c2s_st.timer = 0;
}

static void pofv_c2s_fire(void)
{
    pofv_stage_set(16);   /* ★ 早苗 C2 fire ✓ */
    /* ★★ 一手 ✓（子 agent 核实）：★ 早苗 C2 **不建任何对象** ✗ —— 只做两件事：
     *   · ★ 把弹表**切到 `set11`** ✓（`EX+0xd8 = 11` ✓；= 上表 `s_c1s_set11`，spd 3 / class5 / anm9 ✓）
     *   · ★ 开 **60 帧**窗口 ✓（`EX+0xdc = 1` ✓）
     *   ⇒ ★ 之后 C1 那条窗口链按 set11 的 26 行发射 ✓（分半圈 ✓）。
     *   ❓`class 5` 的子机槽出膛本卡暂用自机坐标 ✓（已登记 ✓）。*/
    s_c2s_st.active = 1; s_c2s_st.ring = 0; s_c2s_st.timer = 0;
    s_c1_set_sel = 1;
    s_c2s_win = 60;                 /* ★ 一手 ✓ 60 帧窗口 */
    pofv_c1_fire();                 /* ★ 立刻按 set11 放一份 ✓（一手同帧 ✓）*/
}

static void pofv_c2s_ring(void)
{
    /* ★★ 2026-10-11 照一手**停用**（子 agent 核实 ✓）：★ 一手早苗 C2 **不建任何对象** ✗ ——
     *   ★ 它只是"切弹表 set11 + 开 60 帧窗口" ✓（见 `pofv_c2s_fire` ✓）。
     *   ⇒ ★ 这个自实现的"转圈 26 发"**不再放** ✓（函数留壳：仍是引用点 ⇒ 不产生 unused 警告 ✓）。*/
    return;
    const float *pp = ce_player_pos();
    int i;
    if (!pp) return;
    for (i = 0; i < POFV_C2S_COUNT; ++i) {
        int k, a5;
        for (k = 0; k < POFV_C2S_MAX; ++k) if (!s_c2s[k].live) break;
        if (k >= POFV_C2S_MAX) return;
        a5 = (int)((-180.0f + 30.0f * (float)i) * (36.0f / 360.0f)) % 36;
        if (a5 < 0) a5 += 36;
        s_c2s[k].live = 1; s_c2s[k].age = 0;
        s_c2s[k].x = pp[0]; s_c2s[k].y = pp[1];
        s_c2s[k].vx = s_c2_dir36[a5][0] * POFV_C2S_SPEED;
        s_c2s[k].vy = s_c2_dir36[a5][1] * POFV_C2S_SPEED;
        s_c2s[k].life = 150;
        s_c2s[k].vm = (int32_t)ce_anm_spawn(CE_ABILITY_ANM(), POFV_C_RING_SCRIPT, POFV_C4_VM_LAYER);
        if (s_c2s[k].vm) {
            ce_anm_set_color((uint32_t)s_c2s[k].vm, 0xffffffffu);
            ce_anm_set_scale((uint32_t)s_c2s[k].vm, 0.45f, 0.45f);
        }
    }
}

static void pofv_c2s_tick(void)
{
    int k;
    if (s_c2s_st.active) {
        if (s_c2s_st.timer == 0) pofv_c2s_ring();
        if (++s_c2s_st.timer >= POFV_C2S_EVERY) {
            s_c2s_st.timer = 0;
            if (++s_c2s_st.ring >= POFV_C2S_RINGS) s_c2s_st.active = 0;
        }
    }
    for (k = 0; k < POFV_C2S_MAX; ++k) {
        pofv_c2s_t *b = &s_c2s[k];
        float pos[3];
        if (!b->live) continue;
        b->x += b->vx; b->y += b->vy;
        ++b->age;
        if (b->x < -POFV_FIELD_HALF_W || b->x > POFV_FIELD_HALF_W ||
            b->y < 0.0f || b->y > POFV_FIELD_H || b->age > b->life) {
            if (b->vm) { ce_anm_delete((uint32_t)b->vm); b->vm = 0; }
            b->live = 0; continue;
        }
        if (b->vm) ce_anm_set_pos((uint32_t)b->vm, b->x, b->y, 0.0f);
        pos[0] = b->x; pos[1] = b->y; pos[2] = 0.0f;
        (void)ce_damage_rect(pos, 0.0f, 2, POFV_C2S_DMG, 24.0f, 24.0f);
    }
}

/* ============ ⑤ C3/C4 —— **早苗**分支：交替两侧刷一串弹（一手 = 刷 `ExAttack2` 子敌人）============
 * ★ 一手（`HANDOFF-109-C1-C4.md` §10.1/§10.6）：★ `pl02.ecl` 的 `ExtraAttack` **自己不发弹** ✗ ——
 *   ★ 每步 **`enmCreate("ExAttack2")`** 刷一个子敌人 ✓（子敌再跑 `ExAttackFunc2` 的 6 条 `etExSet`
 *   效果栈 ✓，★ 与 C3 的 `ExAttackFunc` **同族**、只差 `$D` 与第 (4) 条的 `s` 符号 ✓）。
 *   ★ 表（一手逐条照抄）：`$C`（每波子敌数）`{4,5,6,7,5,7,8,9}` ✓；`$D`（波数）`{1,1,1,1,2,2,2,2}` ✓；
 *     `$E`（`I0` 递减）`{20,18,16,14,10,9,8,7}` ✓；`%F`（x 递减）`{10,10,9,9,8,8,7,7}` ✓；
 *     `$G`（每步 wait）`{15,15,13,13,12,11,10,10}` ✓。
 *   ★ 位置：★ `x = F1 × F0`（★ `F0` 从 **140** 起、每步 −`%F` ✓）、★ `y = RANDF2 × 32` ✓；
 *     ★ `F1` 按难度奇偶取 ±1 ✓、★ **每步取反**（★ 左右交替 ✓）。
 *   ★ 一手的 `ExtraAttack00` **pl02 没有** ✗ ⇒ ★ **她的 C3 与 C4 是同一套** ✓（★ 只有档位/阈值不同 ✓）。
 * ★ 本卡落地（照 §10.6 的建议 ✓）：★ **不真的再 `enmCreate` 一层** ✗（少一个生命周期 ✓）——
 *   ★ 直接按那三张表**刷一串"从两侧向中间收"的弹** ✓（★ 每颗 = 红环 VM + 伤害源 ✓）。
 * ❓自定：① 档位取 **0**（`$C=4, $D=1, $E=20, %F=10, $G=15` ✓）；② x 从 140 起、交替正负 ✓；
 *         ③ `y = RANDF2×32` 的 `RANDF2` 未定位 ⇒ ★ 本卡取 **y = 96**（自机上方固定一点 ✓）；
 *         ④ 伤害沿用 C1 口径 ✓。*/
#define POFV_C3S_MAX      60
#define POFV_C3S_X0     140.0f     /* ★ 一手 ✓ `[-9981.0f]` 的初值 */
#define POFV_C3S_Y       96.0f     /* ❓③：`RANDF2 × 32` 的 `RANDF2` 未定位 */

static const unsigned char s_c3s_cnt[8]  = { 4, 5, 6, 7, 5, 7, 8, 9 };    /* $C ✓ */
static const unsigned char s_c3s_waves[8]= { 1, 1, 1, 1, 2, 2, 2, 2 };    /* $D ✓ */
static const unsigned char s_c3s_e[8]    = { 20,18,16,14,10, 9, 8, 7 };   /* $E ✓ */
static const unsigned char s_c3s_f[8]    = { 10,10, 9, 9, 8, 8, 7, 7 };   /* %F ✓ */
static const unsigned char s_c3s_wait[8] = { 15,15,13,13,12,11,10,10 };   /* $G ✓ */

typedef struct { int live, age, life; float x, y; int32_t vm; } pofv_c3s_t;
static pofv_c3s_t s_c3s[POFV_C3S_MAX];
static struct { int active, wave, step, timer, level, side; float f0; } s_c3s_st;

static void pofv_c3s_clear(void)
{
    int k;
    for (k = 0; k < POFV_C3S_MAX; ++k) {
        if (s_c3s[k].vm) { ce_anm_delete((uint32_t)s_c3s[k].vm); s_c3s[k].vm = 0; }
        s_c3s[k].live = 0; s_c3s[k].age = 0;
    }
    s_c3s_st.active = 0; s_c3s_st.wave = 0; s_c3s_st.step = 0;
    s_c3s_st.timer = 0; s_c3s_st.side = 1; s_c3s_st.f0 = POFV_C3S_X0;
}

static void pofv_c3s_spawn(float x, float y)
{
    int k;
    for (k = 0; k < POFV_C3S_MAX; ++k) if (!s_c3s[k].live) break;
    if (k >= POFV_C3S_MAX) return;
    s_c3s[k].live = 1; s_c3s[k].age = 0; s_c3s[k].life = 120;
    s_c3s[k].x = x; s_c3s[k].y = y;
    s_c3s[k].vm = (int32_t)ce_anm_spawn(CE_ABILITY_ANM(), POFV_C_RING_SCRIPT, POFV_C4_VM_LAYER);
    if (s_c3s[k].vm) {
        ce_anm_set_color((uint32_t)s_c3s[k].vm, 0xffffffffu);
        ce_anm_set_pos((uint32_t)s_c3s[k].vm, x, y, 0.0f);
        ce_anm_set_scale((uint32_t)s_c3s[k].vm, 0.5f, 0.5f);
    }
}

static void pofv_c3s_fire(void)
{
    s_c3s_st.active = 1; s_c3s_st.wave = 0; s_c3s_st.step = 0;
    s_c3s_st.timer = 0; s_c3s_st.level = 0;     /* ❓① */
    s_c3s_st.side = 1; s_c3s_st.f0 = POFV_C3S_X0;
}

static void pofv_c3s_tick(void)
{
    int k;
    if (s_c3s_st.active) {
        const int lv = s_c3s_st.level;
        const int n = s_c3s_cnt[lv];
        if (s_c3s_st.step < n) {
            if (s_c3s_st.timer == 0) {
                /* ★ 一手：x = F1 × F0（F1 每步取反 ⇒ **左右交替** ✓）、y = RANDF2×32 ✓ */
                pofv_c3s_spawn((float)s_c3s_st.side * s_c3s_st.f0, POFV_C3S_Y);
                s_c3s_st.side = -s_c3s_st.side;
                s_c3s_st.f0 -= (float)s_c3s_f[lv];            /* ★ 一手 ✓ 每步 −%F */
            }
            if (++s_c3s_st.timer >= (int)s_c3s_wait[lv]) {     /* ★ 一手 ✓ 每步 wait $G */
                s_c3s_st.timer = 0; ++s_c3s_st.step;
            }
        } else if (++s_c3s_st.wave < (int)s_c3s_waves[lv]) {
            s_c3s_st.step = 0; s_c3s_st.timer = 0;
            s_c3s_st.f0 = POFV_C3S_X0;                        /* ★ 一手 ✓ 每波重置 */
        } else {
            s_c3s_st.active = 0;
        }
    }
    for (k = 0; k < POFV_C3S_MAX; ++k) {
        pofv_c3s_t *b = &s_c3s[k];
        float pos[3];
        if (!b->live) continue;
        ++b->age;
        if (b->age > b->life) {
            if (b->vm) { ce_anm_delete((uint32_t)b->vm); b->vm = 0; }
            b->live = 0; continue;
        }
        if (b->vm) ce_anm_set_pos((uint32_t)b->vm, b->x, b->y, 0.0f);
        pos[0] = b->x; pos[1] = b->y; pos[2] = 0.0f;
        (void)ce_damage_rect(pos, 0.0f, 2, 14.0f, 24.0f, 24.0f);   /* ❓④ */
    }
}

static void clear_card(ce_card_t *c)
{
    int i, k;
    (void)c;
    for (i = 0; i < POFV_STORM_MAX; ++i)
        for (k = 0; k < POFV_STORM_VM_PARTS; ++k)
            if (s_storm_vm[i][k]) { ce_anm_delete((uint32_t)s_storm_vm[i][k]); s_storm_vm[i][k] = 0; }
    pofv_storm_pool_init(&s_storm);
    /* ★★ 2026-10-01（口径 5）+ 铁律「长持续效果删卡要交代」：**灵力道具池**也是卡自建的实体
     *   （不在引擎道具池里 ⇒ 没人替我们清）⇒ 删卡 / 换关 / 重开都在这里清干净。
     *   ★ 每颗道具现在还有一台**跟随 VM**（照 116，见文件头与 `power_vm_sync()`）
     *     ⇒ 下面先真删 VM、再清池。*/
    if (s_power.spawned || s_power_got || s_power.expired || s_power.dropped)
        ce_log("pofv: power run done - %d made, %d picked (+%d pts), %d gone, %d packed away (pool %d)",
               (int)s_power.spawned, (int)s_power.absorbed, (int)s_power_got,
               (int)s_power.expired, (int)s_power.dropped, POFV_POWER_MAX);
    if (s_aura_bullets)
        ce_log("pofv: aura run done - %d enemy bullets touched by the barrier", (int)s_aura_bullets);
    /* ★ 铁律「长持续效果删卡要交代」：**灵力道具的可视 VM** 必须先真删再清池 ✗ ——
     *   句柄在 core 的 `pofv_power_t.vm` 里、而 `pofv_power_clear()` 只 `memset`（core 不依赖
     *   anm API ⇒ 删不了）⇒ 不先删就是**幽灵 VM 泄漏**（与 `s_storm_vm` / `s_spirit_vm` 同款坑）。
     *   `on_delete` / `on_run_reset` / `on_ctor` 都走本函数 ⇒ 三条路都收得到 ✓。*/
    { int z5; for (z5 = 0; z5 < POFV_POWER_MAX; ++z5) power_vm_drop(&s_power.s[z5]); }
    /* ★★ 2026-10-09（铁律「长持续效果删卡要交代」✓）：道具出生时那台**几何闪光**（一手 `effect.anm`
     *   script94 ⇒ 脚本 424）也是**卡自建 VM**。它 20 帧后自己 `delete()` ⇒ 句柄多半已死 ⇒
     *   **必须先 `ce_anm_get_vm()` 探活再删**（陈旧 id 直接删 = 误删别人回收掉的 VM ✗）。
     *   与上面 `power_vm_drop` 同一处、同一支（`on_delete` / `on_run_reset` / `on_ctor` 三路共用 ✓，
     *   不依赖 tick ✓）。*/
    { int z6; for (z6 = 0; z6 < POFV_POWER_MAX; ++z6)
        if (s_power_flash[z6]) {
            if (ce_anm_get_vm((uint32_t)s_power_flash[z6])) ce_anm_delete((uint32_t)s_power_flash[z6]);
            s_power_flash[z6] = 0;
        } }
    pofv_power_clear(&s_power);
    /* ★★ ② 铁律（审计 D ✓）：删卡/换关 ⇒ **收子机槽** —— 放进 `clear_card()` 就覆盖
     *   `on_ctor` / `on_run_reset` / `on_delete` **三条路** ✓；★ 漏收 = 商店删卡时**漏一格池槽**
     *   （12 格用完就再也申请不到 ✓）+ 留幽灵 VM ✓。*/
    if (s_opt_sanae.ptr) { ce_option_drop(c, &s_opt_sanae); s_opt_sanae.ptr = 0; }
    s_rei_prev = 0;   /* ★ 审计 B ✓：与 core 的 absorbed_rei 同步归零 ✓ */
    s_power_got = 0; s_power_logged = 0; s_aura_bullets = 0;
    {   int z9; for (z9 = 0; z9 < CE_BM_BULLET_COUNT; ++z9) s_aura_bullet_done[z9] = 0; }
    /* （结界闩锁：与弹池一起清 ⇒ 删卡后不留幽灵标记；不引 `<string.h>` 的 `memset`，
     *   与别的清池循环同一写法。）*/
    s_spirit_q_n = 0;                  /* 灵（真敌机）的待生成队列 */
    s_point_q = 0;
    pofv_acc_init(&s_acc); s_acc_last_x = 0.0f; s_acc_last_y = 0.0f;
    s_combo_blasts = 0; s_sp_life = 0;   /* ★ 一手的連爆数/SP 寿命 ✓ */
    s_combo_th19.combo = 0; s_combo_th19.win = 0;   /* ★ TH19 連撃真账（一手初值：`0x42fac4`/`0x5482b9` 都是 0 ✓）*/
    s_combo_acc = 0;   /* ★ 与一手同：只在开局/重置清 ✓（窗口漏空**不**清 ✓）*/
    pofv_hits_init(&s_hits);
    /* ★ MODEL `:104-107`（owner 新增「跨面 ⇒ **全清**」）：清单含「各类**冷却**·队列·**待生成项**」✓
     *   ⇒ 下面 5 个原先**漏复位** ✗（核实复核指出）：冷却 `s_ghost_cool`、击破计数 `s_pure_kill_n`、
     *     絕对计数 `s_pure_kill_n_abs`、待生成项 `s_ex_pending`、光环戳 `s_aura_stamp` ✓ */
    s_ghost_cool = 0; s_pure_kill_n = 0; s_pure_kill_n_abs = 0; s_ex_pending = 0;
    /* ★ 核实（2026-10-05）：`s_aura_stamp` 声明是 **`-1`**、用法 `held = (s_aura_stamp == s_card_frames)`
     *   ⇒ 复位成 0 会让跨面后的**第 0 帧假判「按住 C」**（展开结界 / 放道具 / 触发镜像固化）✗
     *   ⇒ 复位成 **`-1`**（与声明同值 ✓）*/
    s_aura_stamp = -1;
    if (s_aura_vm) { ce_anm_delete((uint32_t)s_aura_vm); s_aura_vm = 0; }   /* ★ 铁律：长持续效果在删卡/跨面时收干净 ✓ */
    {   int ip4; for (ip4 = 0; ip4 < POFV_AURA_PIECES; ++ip4)          /* ★ 铁律：咲夜那 **7 片**也要一起收干净 ✓ */
            if (s_aura_piece[ip4]) { ce_anm_delete((uint32_t)s_aura_piece[ip4]); s_aura_piece[ip4] = 0; }
    }
    /* ★★★★ 2026-10-09（铁律「长持续效果删卡要交代」✓）：结界染过的**敌弹颜色**也是跨帧留下的东西
     *   ⇒ 删卡 / 换关 / 重开必须把颜色写回原样，否则那几颗弹会**永远停在 `0xFFFF??80`** ✗。
     *   ⚠ 本函数（`on_ctor` / `on_run_reset` / `on_delete` 三路共用）**不依赖 tick** ✓
     *     —— 删卡发生在商店里、`on_draw` 不跑 ⇒ 只能靠这里（与 `s_storm_vm` / `s_aura_piece` 同一支）。*/
    pofv_aura_tint_restore_all();
    s_aura_charge = 0.0f; s_aura_r = 0.0f;   /* ★ 蓄力（C ✓）随卡清零 ✓ */
    s_aura_size = 0.0f;                      /* ★ 结界尺寸（一手 `S+0x8c` 等价物 ✓）随卡清零 ✓ */
    /* ★★★★ C2（霊撃）的跨帧遗留物也在这里收干净（铁律「长持续效果删卡要交代」✓）：
     *   · 可见扩张环的 VM（脚本 396）⇒ 真删；
     *   · 无敌**声明层**的那一格 ⇒ 显式撤（声明层是粘性的，删卡只听 `on_delete` ⇒ 不撤就永久留着 ✗）；
     *   · 结界张开上升沿的闩锁 ⇒ 归 0（不留幽灵闩锁）。
     *   ⚠ 本函数**不依赖 tick** ⇒ 商店里删卡（倒吊人 / 弃整副牌）也收得到 ✓。*/
    pofv_c2_clear();
    pofv_c1_clear();   /* ★ 铁律：长持续效果（C1 的 VM + 伤害源）删卡要收干净 ✓ */
    pofv_c3_clear();   /* ★ 铁律：C3 每一颗弹都带一台 VM ⇒ 删卡/换关/重开全收 ✓ */
    pofv_barrier_clear();   /* ★ 铁律：罩子（盾/破罩计时）删卡要收干净 ✓ */
    pofv_c4_clear();        /* ★ 铁律：C4 每发一台 VM ⇒ 删卡/换关全收 ✓ */
    pofv_c2m_clear();       /* ★ 铁律：魔理沙 C2 的 6 条激光 VM ⇒ 全收 ✓ */
    pofv_c3m_clear();       /* ★ 铁律：魔理沙 C3 的激光 VM ⇒ 全收 ✓ */
    pofv_c4m_clear();       /* ★ 铁律：魔理沙 C4 的激光与扫射弹 VM ⇒ 全收 ✓ */
    pofv_c2s_clear();       /* ★ 铁律：早苗 C2 的整圈 VM ⇒ 全收 ✓ */
    pofv_c3s_clear();       /* ★ 铁律：早苗 C3/C4 的刷弹 VM ⇒ 全收 ✓ */
    {   int sp; for (sp = 0; sp < POFV_SPARK_MAX; ++sp) {          /* ★ 铁律：火花也收 ✓ */
            if (s_spark_vm[sp] && ce_anm_get_vm((uint32_t)s_spark_vm[sp]))
                ce_anm_delete((uint32_t)s_spark_vm[sp]);
            s_spark_vm[sp] = 0; s_spark_ttl[sp] = 0;
    } }
    /* ★★★★ 2026-10-10 owner：灵力（上限量 = 一手 `S+0x24`）**初值 = C1（不是 0 ✗）**
     *   ⇒ 开局有基础灵力 ⇒ 蓄力/结界不会被封到 0 ✓（C1 数值 = ❓自定，见 `POFV_REI_C1` ✓）。*/
    s_reiryoku = POFV_REI_C1;
    /* ★ B 组的**上升沿判据**要从 C1 对应的档位起步 —— 否则开局第一帧「档位 0 → C1 的档位」
     *   会被当成一次变化 ⇒ **假弹 78 帧** ✗（一手开局也不会弹 ✓）。*/
    {   float r0 = s_reiryoku / POFV_REI_MAX;   /* ★ 灵力量纲 = 0..2500 ✓（此前除 100 会恒满 ✗）*/
        int32_t t0;
        if (r0 < 0.0f) r0 = 0.0f;
        if (r0 > 1.0f) r0 = 1.0f;
        t0 = (int32_t)(r0 * (float)POFV_HUD_CHG_MAX_TIER);
        if (t0 < 0) t0 = 0;
        if (t0 > POFV_HUD_CHG_MAX_TIER) t0 = POFV_HUD_CHG_MAX_TIER;
        s_hud_lv_last_tier = t0;
    }
    s_hud_lv_left = 0;                       /* ★ B 组弹出态清零（换局/删卡不残留 ✓）*/
    s_aura_role_logged = 0;                  /* ★ 下次激活再打一次角色日志 ✓ */
    s_cnt_send54 = s_cnt_send30 = s_cnt_arrive = s_cnt_gift = s_cnt_kill = 0;   /* ★ goal ②：计数随卡清零 ✓ */
    s_ex_gauge = 0; s_ex_level = 0; s_ex_cool = 0; s_send40 = 0;   /* ★ EX/送灵计数：随卡清零 ✓（铁律 ✓）*/
    s_period_timer = 0;
    /* ★ 核实裁定 Q3（2026-10-05）：MODEL §95 说两表阈值随**对局时间**下降，而原来 `s_card_frames` **跨局累计** ✗
     *   ⇒ 换局/重开时一并清零 ✓（❓ 另一说 MODEL:122 指一手分档用**全局时钟** `DAT_0062b260`
     *     —— 若 owner 要「全局」语义，把这一行删掉即可回到跨局累计 ✓，两读法已登记 TUNING ✓）*/
    s_card_frames = 0;
    /* ★★★ 仓库铁律 + 核实裁定（2026-10-05）：这几套 HUD/文字 VM 的句柄**归卡所有** —— SDK 的
     *   `ce_hud_number_clear()`（`sdk.c:742-746`）与 `ce_hud_vm_text_clear()`（`sdk.c:951-956`）
     *   才是**硬删**；**手动把 id[] 清零 = orphan 泄漏** ✗（`_set` 只回收「已经死掉」的 id，`sdk.c:763-764`）✓。
     *   ⇒ 三套（命中数字 / 命中标签 / UI 事件与幽灵文字，后者原**只清了 `id[0]`** ✗）全部改用 `_clear` ✓。*/
    { int i3; for (i3 = 0; i3 < POFV_UI_EV_MAX; ++i3) { s_ui_ev[i3].what = 0; s_ui_ev[i3].ttl = 0;
        ce_hud_vm_text_clear(&s_ui_ev_vm[i3]); }
      ce_hud_vm_text_clear(&s_ui_gh_vm); }
    ce_hud_number_clear(&s_hud_hits_num);
    /* ★★ 2026-10-08（铁律 ✓）：我新加的 3 个 HUD 数字句柄也必须清 ✗ ⇒ 否则删卡/换局后
     *   数字会**留在屏幕上**（与上面两条 HUD 条 VM 同类 ✓；owner 报过"重叠数字"⇒ 不要再留残影 ✗）。*/
    ce_hud_number_clear(&s_hud_sp_num);
    ce_hud_number_clear(&s_hud_c2_num);
    ce_hud_number_clear(&s_hud_ag_num);
    /* ★★ 2026-10-08（铁律「长持续效果必须交代删掉会怎样」✓）：新增的两条 HUD 条是**卡自建 VM** ✓
     *   ⇒ 与 `s_storm_vm` 同法**真删** ✓（`on_delete` / `on_run_reset` 共用本函数 ✓）；
     *   ⚠ 我先前把句柄写成**块内 static** ✗ ⇒ `clear_card` 看不到 ⇒ 会泄漏 ✗ ⇒ 已提到文件作用域 ✓。*/
    if (s_hud_c2_vm) { ce_anm_delete((uint32_t)s_hud_c2_vm); s_hud_c2_vm = 0; }
    if (s_hud_c2_fill_vm) { ce_anm_delete((uint32_t)s_hud_c2_fill_vm); s_hud_c2_fill_vm = 0; }   /* 上一轮那对条已不用 ✓ */
    if (s_hud_ag_vm) { ce_anm_delete((uint32_t)s_hud_ag_vm); s_hud_ag_vm = 0; }
    if (s_hud_ag_fill_vm) { ce_anm_delete((uint32_t)s_hud_ag_fill_vm); s_hud_ag_fill_vm = 0; }
    { int i9; for (i9 = 0; i9 < 4; ++i9) if (s_hud_ag_dot_vm[i9]) { ce_anm_delete((uint32_t)s_hud_ag_dot_vm[i9]); s_hud_ag_dot_vm[i9] = 0; } }
    if (s_hud_hits_vm) { ce_anm_delete((uint32_t)s_hud_hits_vm); s_hud_hits_vm = 0; }
    if (s_hud_hitsf_vm) { ce_anm_delete((uint32_t)s_hud_hitsf_vm); s_hud_hitsf_vm = 0; }
    if (s_hud_sp_frame_vm) { ce_anm_delete((uint32_t)s_hud_sp_frame_vm); s_hud_sp_frame_vm = 0; }
    if (s_hud_sp_hit_vm) { ce_anm_delete((uint32_t)s_hud_sp_hit_vm); s_hud_sp_hit_vm = 0; }
    { int i9; for (i9 = 0; i9 < 6; ++i9) if (s_hud_sp_digit_vm[i9]) { ce_anm_delete((uint32_t)s_hud_sp_digit_vm[i9]); s_hud_sp_digit_vm[i9] = 0; } }
    { int i9; for (i9 = 0; i9 < 3; ++i9) if (s_hud_hit_digit_vm[i9]) { ce_anm_delete((uint32_t)s_hud_hit_digit_vm[i9]); s_hud_hit_digit_vm[i9] = 0; } }
    if (s_hud_bonus_vm) { ce_anm_delete((uint32_t)s_hud_bonus_vm); s_hud_bonus_vm = 0; }   /* Bonus! ✓ */
    /* ★★★★ 2026-10-08 卡 109 的**蓄力槽**（脚本 422：底槽 + 蓝填充 + 白段 + 4 标）——
     *   与上面那批同款：**卡自建 VM ⇒ 必须在 clear_card 里真删** ✓（铁律「长持续效果要交代删掉会怎样」；
     *   `on_delete` / `on_run_reset` / `on_ctor` 三路都走本函数 ✓，不依赖 tick ✓）。
     *   句柄清 0 + 认领标记复位（活着的帧下一帧会重新认领/重建 ✓；`on_delete` 那条路 tick 已停也无妨 ✓）。*/
    if (s_hud_chg_track_vm) { ce_anm_delete((uint32_t)s_hud_chg_track_vm); s_hud_chg_track_vm = 0; }
    ce_hud_vm_text_clear(&s_hud_chg_lv_text);   /* ★ 铁律：文本 VM 也要收 ✓ */
    {   int k3; for (k3 = 0; k3 < POFV_C2_POINTS; ++k3)      /* ★ 铁律：C2 的 3 台可见物也收 ✓ */
            if (s_c2.orb_vm[k3]) { ce_anm_delete((uint32_t)s_c2.orb_vm[k3]); s_c2.orb_vm[k3] = 0; } }
    s_barrier_grant_pending = 1;                /* ★ #5：重新拿到卡 ⇒ 再给一个罩 ✓ */
    s_c3_uses = 0; s_c4_uses = 0;
    {   int k5; for (k5 = 0; k5 < POFV_C1_TRAIL_MAX; ++k5) {          /* ★ 铁律：残影也收 ✓ */
            if (s_c1_trail[k5] && ce_anm_get_vm((uint32_t)s_c1_trail[k5]))
                ce_anm_delete((uint32_t)s_c1_trail[k5]);
            s_c1_trail[k5] = 0; s_c1_trail_ttl[k5] = 0;
    } }               /* ★ 铁律：C3/C4 使用计数随卡清零 ✓ */
    if (s_hud_chg_fill_vm)  { ce_anm_delete((uint32_t)s_hud_chg_fill_vm);  s_hud_chg_fill_vm  = 0; }
    if (s_hud_chg_white_vm) { ce_anm_delete((uint32_t)s_hud_chg_white_vm); s_hud_chg_white_vm = 0; }
    { int i9; for (i9 = 0; i9 < POFV_HUD_CHG_TICKS; ++i9)
        if (s_hud_chg_tick_vm[i9]) { ce_anm_delete((uint32_t)s_hud_chg_tick_vm[i9]); s_hud_chg_tick_vm[i9] = 0; } }
    /* ★ 光晕（脚本 426，**电平常驻**）：**在飞的那几台也要真删** ✓（否则删卡/换局 ⇒ 泄漏 ✗，
     *   铁律「长持续效果必须交代删掉会怎样」；下一局 `pofv_hud_charge_bar()` 会按电平重新起 ✓）。*/
    { int i9; for (i9 = 0; i9 < POFV_HUD_CHG_TICKS; ++i9)
        if (s_hud_chg_halo_vm[i9]) { ce_anm_delete((uint32_t)s_hud_chg_halo_vm[i9]); s_hud_chg_halo_vm[i9] = 0; } }
    /* ⚠ 槽位**本身**的释放不在这里做：SDK 的 `ce_layer_drop_card()` 在 `on_run_reset`/`on_delete`
     *   进入本回调**之前**已经调过 `ce_hud_slot_release(109)`（`sdk.h:1123`）✓。*/
    s_hud_chg_slot_ok = 0;
    ce_hud_vm_text_clear(&s_hud_hits_lbl);
    pofv_foe_spirit_init(s_foe, POFV_FOE_SPIRIT_MAX);
    pofv_foe_incoming_init(s_inc, POFV_FOE_INCOMING_MAX);
    /* ★★★ 仓库铁律「长持续效果的卡必须交代『被删掉会怎样』」：灵的两批**自建 VM**（附属圆 `s_spirit_vm`、
     *   本体 `s_spirit_body`，spawn 见 `:1178`/`:1187`）在原版里**必须硬删** ✗ —— 原来只把句柄清零 ⇒
     *   删卡 / 换局时**泄漏**（dangling）✗，也与本文件 `:477` 自己的口径「硬删只留在 `clear_card()`」相矛盾 ×。
     *   ⇒ 与上面 `s_storm_vm` 同法真删 ✓（`on_delete` / `on_run_reset` 共用本函数 ✓，不依赖 tick ✓）。*/
    { int z; for (z = 0; z < POFV_SPIRIT_MAX; ++z) {
        if (s_spirit_vm[z])   ce_anm_delete((uint32_t)s_spirit_vm[z]);
        if (s_spirit_body[z]) ce_anm_delete((uint32_t)s_spirit_body[z]);
        s_spirit_vm[z] = 0; s_spirit_body[z] = 0; s_spirit_age[z] = 0; s_spirit_lr[z] = 0; s_spirit_miss[z] = 0; } }
    { int z; for (z = 0; z < POFV_SPIRIT_STASH_MAX; ++z) s_stash_ttl[z] = 0; }
    { int z; for (z = 0; z < POFV_SPIRIT_MAX; ++z) { s_spirit_id[z] = 0; s_spirit_kind[z] = -1; s_spirit_level[z] = 0; s_spirit_from_incoming[z] = 0; s_spirit_dis[z] = 0; s_spirit_dis_age[z] = 0; s_spirit_ex[z] = 0; } s_spirit_next = 0; }
    s_cap_logged = 0;
    /* （原来的重复尾巴循环已删：上面那行 `s_spirit_id/z` 与 `s_spirit_next` 已同处清零 ✓）*/
}

/* ============================ 生命周期 ============================ */
static int on_ctor(ce_card_t *c)
{
    (void)c;
    clear_card(c);
    ce_log("pofv: ctor (109: card art + active identity + storm[TH19]; nothing else yet)");
    return 0;
}

/* ★ 触发 —— **一手 + owner 口径**：
 *   · TH19：敌人死亡处理 `FUN_0050c1f0` 在 `0x50c3c2` 调 **`FUN_0050c6d0(pos)` = 連爆本体**
 *     （20 类道具扇形撒布 + 链节点 `vft+0x34` ⇒ 生成消弹/命中的对象）；
 *   · owner 口径（`engine/versus/th19/05-four-mechanics.md` §8.1）：**「杂鱼死掉时创建」**
 *     ⇒ **boss 不算**（`CE_ENEMY_FLAG_BOSS` = `enemy+0x635c` bit23 = ECL `setBoss`）。
 *   ⚠ `on_enemy_death` 在**清场帧一帧会被调 100–300 次** ⇒ 这里只准做 O(1) 的一件事
 *     （一次线性找空槽），真正的消弹/命中在 `on_tick_2` 里做。 */
/* ★★★ 2026-10-06 owner：「**对侧不再执行距离规则，改为随机**」✓
 *   ⇒ 我方送到**对面模型**的那只灵，种类**随机**取动物三档（狼/獭/鹫 ✓），不再按击破点到自机的距离² ✓。
 *   （純霊走的是另一张表 `+0x54` ✓，不在这里 ✓。）*/
static int32_t pofv_spirit_kind_random_animal(void)
{
    static const int32_t k3[3] = { POFV_SPIRIT_KIND_WOLF, POFV_SPIRIT_KIND_OTTER, POFV_SPIRIT_KIND_EAGLE };
    return k3[ce_rand() % 3];
}

static void on_enemy_death(ce_card_t *c, const ce_enemy_death_t *ev)
{
    (void)c;
    if (!ev) return;
    if (ev->kind != CE_EDEATH_KILLED) {
        /* ★★★★ 2026-10-06 owner 实跑两条症状（「**黄色 VM 糊在灵上**」＋「**旋转 90° 全消失**」）✗
         *   ⇒ 根因（本轮定位 ✓）：**非击破的死法我原来直接 `return` 了** ✗ —— 尤其是
         *     **`CE_EDEATH_ECL_DIE`**（= ECL 自己 `delete()` ✓，正是我们 `CESpirit109_leave` 那条 ✓）
         *     ⇒ 灵的 **VM 不收、账格不释放** ✗ ⇒ 残留 VM（可能是 level-2 幽灵的**黄** `CALMY` ✓）留在原地 ✓
         *     ＋ 后续新灵复用同格 ⇒ 旋转/贴图写到新 VM 上、眼前却是旧 VM 的残留 ✓ —— 两条症状同源 ✓。
         *   ⇒ 铁律「长持续效果必须交代『被删掉会怎样』」✓：**离场也要收干净** ✓（下面这段）。*/
        int lv2 = 0, inc2 = 0;
        int z2 = pofv_spirit_take(ev->id, &lv2, &inc2);     /* 认得出是我们的灵才收 ✓ */
        if (z2 >= 0) {
            if (s_spirit_vm[z2])   { ce_anm_delete((uint32_t)s_spirit_vm[z2]);   s_spirit_vm[z2]   = 0; }
            if (s_spirit_body[z2]) { ce_anm_delete((uint32_t)s_spirit_body[z2]); s_spirit_body[z2] = 0; }
            s_spirit_dis[z2] = 0; s_spirit_dis_age[z2] = 0; s_spirit_ex[z2] = 0;
            ce_log("pofv: 灵 id=%u **离场**（kind=%d）⇒ 已收 VM ＋ 释放账格 ✓",
                   (unsigned)ev->id, (int)ev->kind);
        }
        return;                                             /* 离场**不算击破** ✓（不喂累计器 ✓）*/
    }
    if (!(ev->flags & CE_EDEATH_F_REAL)) return;           /* 真敌人（哨兵/特殊对象不算）*/
    /* ★★★★ 2026-10-10 TH19 連撃 **+1**（一手 `0x50c450`：通用敌人死亡处理里
     *   `FUN_00549780(实参 = Enemy+0x6350)` ✓）。
     *   ★ 位置是**有意义的**：一手 `0x50c450` 在**奖励读点 `0x50c464` 之前** ✓
     *     ⇒ 下面 `0x50c464` 那条看到的是**含本次击破**的 combo ✓
     *     （上一版把喂食放在回调后段 ⇒ 读到的是"少 1"的值 ✗，现在与一手同序 ✓）。
     *   ★ 窗口加量 = `Enemy+0x6350`（ECL `ins_1032`）：一手**默认 20**、`wave*.ecl` 也全写 20 ✓
     *     ⇒ `POFV_TH19_COMBO_KILL_WIN = 20` ✓（TH18 无同类字段 ⇒ 取一手常数，登记 `TUNING.md` ❓）。*/
    /* 2026-10-11：把**击破点**记下来 ⇒ 供"满 100 掉 kind 9"的落点用 ✓（一手落破坏位置 ✓）。*/
    if (ev) { s_combo_last_x = ev->x; s_combo_last_y = ev->y; }
    combo_th19_add(1, POFV_TH19_COMBO_KILL_WIN);
    /* ★★★ owner 2026-10-08 定案（生成口 ①）：**击破敌人 ⇒ 撒 1 颗灵力道具** ——
     *   位置 = **敌机位置**（`ev->x/ev->y`）、角度 **−π/2**、速度 **2.2**（= 一手 `FUN_00521d60`
     *   的实参，见 `pofv_core.h` 的 `POFV_POWER_DROP_ANGLE/_SPEED`）。
     *   ⚠ 放在 boss 早退**之前**：owner 的字面口径是「击破**敌人**」，boss 也是敌人
     *     （被删前那份同样没有 boss 门 ⇒ 这里保持同口径；boss 死了也只多这一颗，20 分）。
     *   成本 = 一次线性找空槽（≤ `POFV_POWER_MAX` 次比较）—— 清场帧 100–300 次也够便宜
     *   （被删前那份的同一个取舍）。*/
    /* ★★★★ 2026-10-09 owner：「绿点/信仰点**不应当和原本的掉落物重合**」✓（澄清：**位置**重合 ✓）——
     *   一手/本仓事实：109 的击破掉落原先用**敌机精确坐标**、角度 −π/2、速度 2.2，
     *   与 **TH18 原生 main-drop 的实参逐项相同** ✓，且本卡道具还有 **6 帧待发射原地不动** ✓
     *   ⇒ **至少 6 帧与原生掉落完全叠住** ✗（之后两者都向正上方飞、仍基本重合 ✓）。
     *   ⇒ 照 **110/111 的同式做法** ✓（`mof.c:755`）：用**共享口** `faith_scatter_from_enemy()`
     *     把落点改成「敌机坐标 **+ ZUN 爆道具散布偏移**」✓（公式一手 ✓：`敌机 + (cos·rx, sin·ry)×(0.5·rand)`，
     *     `rx/ry` = 敌机掉落区 ✓ 默认 32 ✓；随机源 = 本仓 LCG ✓ 避开 x87 ✓）。
     *   ⚠ 与 110/111 用的是**同一个共享函数**（该共享件归 107+110/111 那批）⇒ 已在 `AUDIT.md` §BE 登记 ✓。
     *   ★ 注意：**只错开位置** ✓，**不清原生掉落** ✗（owner 当年的口径：这是「**额外**性质」的点 ✓）。*/
    {
        float sc0 = 0.0f, sc1 = 0.0f;
        faith_scatter_from_enemy((const uint8_t *)ev->enemy, &sc0, &sc1);   /* 照 mof.c:755 ✓ */
        if (pofv_power_spawn_dir(&s_power, ev->x + sc0, ev->y + sc1, POFV_POWER_PTS_KILL, POFV_POWER_SRC_KILL,
                                 POFV_POWER_DROP_ANGLE, POFV_POWER_DROP_SPEED) < 0) {
            if (!s_power_logged) {
                ce_log("pofv: power pool full (%d) — kill drop lost", POFV_POWER_MAX);
                s_power_logged = 1;
            }
        }
        /* ★★★★ 2026-10-10 owner：**连击额外灵力道具**（一手 `FUN_0050c1f0` `0x50c1f0` ✓）——
         *   一手逐字（`05-four-mechanics.md` §2 / `0x50c4ba` ✓）：
         *     `if (combo > 5) { n = combo/12 + 1; if (n > 3) n = 3;
         *                      if (enemy[+0x63a0] & 0x8000000) n = 1;
         *                      do { FUN_00521c60(pos); } while (--n); }`
         *   ⇒ 本卡照抄**额外**撒 `min(combo/12+1, 3)` 颗（**主掉落之外** ✓，用既有 `pofv_power_spawn_dir` ✓，
         *     位置走与主掉落**同一条规则**：敌机坐标 + `faith_scatter_from_enemy()` 的 ZUN 爆道具散布偏移 ✓，
         *     角度/速度同 `POFV_POWER_DROP_ANGLE/_SPEED` ✓；每次调用取**新的一份散布** ⇒ 与主掉落同分布 ✓）。
         *   ⚠ 一手那条 `enemy[+0x63a0] & 0x8000000 ⇒ n = 1` 的**例外未接** ✗：那是 TH19 敌机 flags 的
         *     bit27，TH18 的敌机 flags 在 `+0x635c`（`CE_ENEMY_FLAGS` ✓）且**没有**已知同义位
         *     ⇒ 无法照抄 ⇒ 登记 `TUNING.md` ❓（本卡就不做这条例外 ✓）。
         *   ✅ **`combo` 来源（2026-10-10 定案 ✓）**：一手 = TH19 `PlayerInf+0x22bc`
         *     （= Hits 子对象 `PlayerInf+0x2278` 的 `+0x44` ✓——**不是** HUD 值；HUD 读的 `+0x22b4`
         *      只是它在 `0x549815` 处的**镜像** ✓。旧注里的「HUD 連爆/队列计数，cap 19」两处都错 ✗：
         *      HUD 读的是 `+0x22b4`；上限**不存在** ✓）。
         *     ⇒ 本卡现在读**自己的 TH19 連撃账 `s_combo_th19`** ✓（`combo_th19_*`，口径见 `pofv_core.h`；
         *      上一版拿 `s_hits.cur` 当近似 ✗ —— 那本账服务显示层且被 TH09 的窗口清、与一手不同 ✗）。
         *     ★ 时序**已与一手同序** ✓：本回调在 `CE_EDEATH_F_REAL` 之后**先** `combo_th19_add(1, 20)`
         *      再走到这里 ⇒ 这里看到的是**含本次击破**的 combo ✓（一手 `0x50c450` 在 `0x50c464` 之前 ✓）。*/
        {
            const int32_t combo = s_combo_th19.combo;   /* ★ 真账（一手 `PlayerInf+0x22bc` ✓）*/
            if (combo > 5) {
                int32_t n = combo / 12 + 1;
                if (n > 3) n = 3;                      /* 一手：`if (n > 3) n = 3` ✓ */
                while (n-- > 0) {
                    float dx2 = 0.0f, dy2 = 0.0f;
                    faith_scatter_from_enemy((const uint8_t *)ev->enemy, &dx2, &dy2);   /* 与主掉落同规则 ✓ */
                    if (pofv_power_spawn_dir(&s_power, ev->x + dx2, ev->y + dy2,
                                             POFV_POWER_PTS_KILL, POFV_POWER_SRC_KILL,
                                             POFV_POWER_DROP_ANGLE, POFV_POWER_DROP_SPEED) < 0)
                        break;                         /* 池满 ⇒ 停（面值丢一颗，与主掉落同口径 ✓）*/
                }
            }
        }
    }   /* ★ 2026-10-09：本块是「加散布偏移」新包的外层 `{`（照 mof.c:755 同式）✓ */
    if (ev->enemy &&
        (*(const uint32_t *)((const uint8_t *)ev->enemy + CE_ENEMY_FLAGS) & CE_ENEMY_FLAG_BOSS))
        return;                                            /* ★ 杂鱼口径：boss 不起暴风 */
    /* ★ 档位（owner 2026-10-05）：
     *   · **默认 = 小妖精 B**（半径 50 / 50 帧 / 伤害 6）；
     *   · **灵档 = 80 / 40 帧 / 伤害 10** —— 只对「**本卡自己召唤的灵**」用
     *     （owner 原话：TH17 的動物霊与 TH19 的灵「**设定意义上是一个东西，游戏上完全不是一个意义的玩意**，
     *      相当于**只有本卡召唤的才是灵**」⇒ **不读别人的卡、不判敌人种类**，
     *      只看 `ev->id` 在不在**我们自己的灵账**里）。
     *   ⇒ 灵账在下面 `pofv_note_spirit()`；召唤灵的地方调它一次即可（灵的功能本体还没做，见 §TODO(灵)）。 */
    /* ★★ W2/W3（一手：`REPORT` §3.1 / 镜像表）：
     *   · **击破純霊 ⇒ 送動物霊**，种类 = **死亡点到自机距离²**（4624 狼 / 16384 獭 / 更远 鷲）；
     *   · **击破動物霊 ⇒ 返还**一只（**同 kind**）；
     *   两者都走同一条「**死亡栈只入队**」的路（SDK 禁止在死亡栈里建敌机）。 */
    {
        int32_t lv = -1;
        int inc = 0;
        /* ★★★★ 2026-10-08 owner 实跑定位（「把那只**看不见的灵**打死 ⇒ 上面的就消失」✓）：
         *   本支**取了户口却从来没删 VM** ✗（对比击破純霊那支有删 ✓）⇒ 每走一次就留下一台
         *   **常驻本体 VM**（= 「版顶一只朝右的狼灵、无碰撞箱、只是 VM」✓）。
         *   ⇒ 修法：**先按 id 记住槽号**（`take` 会把 id 清掉 ✗），**删掉 VM**，再交给 `take` ✓。*/
        {   int sl_pre = -1, ipre;
            for (ipre = 0; ipre < POFV_SPIRIT_MAX; ++ipre)
                if (s_spirit_id[ipre] == ev->id) { sl_pre = ipre; break; }
            if (sl_pre >= 0) {
                if (s_spirit_vm[sl_pre])   { ce_anm_delete((uint32_t)s_spirit_vm[sl_pre]);   s_spirit_vm[sl_pre]   = 0; }
                if (s_spirit_body[sl_pre]) { ce_anm_delete((uint32_t)s_spirit_body[sl_pre]); s_spirit_body[sl_pre] = 0; }
            }
        }
        int k = pofv_spirit_take(ev->id, &lv, &inc);
        /* ★ 一手（owner）：純霊的扳机挂**玩家射击/命中** ⇒ 每次击破攒一格，够数就交一只（见 tick 的交付段）。 */
        /* ★★★ 2026-10-05 owner 实跑定案：「你实现的是**我击破一个，在我自己这边直接生成下一个**」✗
         *   ⇒ 按一手 §9.1：純霊的**生成侧 = `1 - *(源+0x98)` = 对面** ✓ ⇒ 我方击破 ⇒ **生成到对面模型** ✓
         *   （不是本侧直接冒 ✗）。⇒ 本侧看到的純霊应当来自**对面（模型）那边的生成** ✓ = 镜像回程 ✓
         *   ❓ 本侧純霊的**来源**（模型侧何时生成、经哪条路回来）待 owner 确认 ⇒ 现在**先不本侧生成** ✗ */
        if (++s_pure_kill_n >= POFV_PURE_SPAWN_INTERVAL) {
            s_pure_kill_n = 0;
            pofv_foe_spawn_gated(pofv_top_x(), pofv_top_y(),
                                  POFV_SPIRIT_KIND_PURE, 0);
            ce_log("pofv: 純霊生成 ⇒ **送进对面模型**（一手 `生成侧 = 对面` ✓；本侧不再直接冒 ✗）");
        }
        /* ★ W7 EX 触发（一手门限 149/200/300）：❓ 计数源暂用同一击破计数，难度取 1（Normal = 200）。 */
        if (pofv_ex_ready(s_pure_kill_n_abs++, 1)) { ++s_ex_pending; }
        /* ★★★ 2026-10-06 **owner 指到 `TUNING` 第 6 条（一手订正）**：`+15` 来自**通用敌机死亡处理器**
         *   `FUN_0050c1f0`（全 exe 5 个调用者）⇒ **妖精的死亡同样加计数器** ✓ ⇒ 卡里原先只在「我方灵被击破」
         *   那支喂 ⇒ **妖精完全不喂** ✗ ⇒ 累计器爬不动 ⇒ 阈值不开 ⇒ 「没有 level 1」✓ ⇒ 现**上移到每次真实击破** ✓。
         *   ✅ 2026-10-06 已改（owner 裁定「TH09/TH19 独立运行」）：TH09 族**不再**用 TH19 的步长/阈值 ✗
         *   ⇒ 见下面 TH09 那两行（`POFV_ACC09_STEP2C/STEP30` ✓）与 `pofv_acc_take30(&s_acc, diff)` ✓
         *   （核实复核：全文件 `pofv_acc_*` 只出现在 `:660-691`，无任何交叉 ✓ —— 原「仍待 owner 定」是与新代码
         *    矛盾的过期文字 ✗，现订正 ✓）*/
            { int32_t i3; for (i3 = POFV_UI_EV_MAX - 1; i3 > 0; --i3) s_ui_ev[i3] = s_ui_ev[i3 - 1];
              s_ui_ev[0].what = POFV_UI_EV_KILL; s_ui_ev[0].ttl = POFV_UI_EV_TTL; }   /* 镜像击破：停 3 s ✓ */
            ++s_cnt_kill;                        /* ★ goal ②：击破计数（定位自维持用 ✓）*/
            ++s_send40;                                 /* ★ §8.38：进入状态域 1 的次数 +1 ✓（本侧 ✓）*/
            if (s_ex_cool <= 0) {                       /* ★ 一手：冷却中（`+0x34 != 0`）**不积累** ✓ */
                s_ex_gauge += POFV_EX_GAUGE_STEP;       /* ★ 击破 ⇒ **+15** ✓（一手 §11.1 ✓）*/
                if (s_ex_gauge > POFV_EX_GAUGE_MAX) s_ex_gauge = POFV_EX_GAUGE_MAX;
            }
            s_acc_last_x = ev->x; s_acc_last_y = ev->y;
            /* ★★★ owner 2026-10-06：**两套独立**、各自判阈值 ⇒ 「**一次暴风可以同时送出一只純霊 + 一只幽灵**」✓
             *   · TH19 那套：阈值 `150−t/2` / `120−t/5`（吃**对局时间** ✓）⇒ `+0x54` = **純霊** ✓
             *   · TH09 那套：阈值 `(30−難度)×4` / `60−難度×1.5`（吃**真实难度** ✓）⇒ `+0x30` = **幽灵族** ✓（带 `<25` 收件门 ✓）*/
            {
                const int32_t diff = ce_difficulty();   /* ★ 真实难度（一手 `CE_ADDR_DIFFICULTY 0x4ccd00`，`engine.h:14`：0=E/1=N/2=H/3=L/4=EX）*/
            if (pofv_spirit_spill_stop()) { /* ★ 安全阀：不送 ✓（也不消费累计器 ✓）*/
            } else if (pofv_acc_take54(&s_acc, s_card_frames / 60)) {
                /* ★★★ 2026-10-08（核查 ✓）：`pofv_acc_take50` 同样**全仓零调用** ✗ ⇒ a50 只涨不掉 ✓。
                 *   一手：TH19 在**开火/送大招**时扣 ✓ ⇒ 在此接上 ✓（与 take54 同一送出点 ✓）。*/
                pofv_acc_take50(&s_acc, s_card_frames / 60);
                ++s_cnt_send54;
                ce_log("pofv: 連撃累积过阈值（破坏时判）⇒ 送出（a50=%d a54=%d t=%d）",
                       (int)s_acc.a50, (int)s_acc.a54, (int)s_card_frames);
                pofv_foe_spawn_gated(pofv_top_x(), pofv_top_y(), POFV_SPIRIT_KIND_PURE, 0);   /* ★ 一手：`+0x54` = 純霊 ✓ */
                pofv_foe_incoming_push(s_inc, POFV_FOE_INCOMING_MAX,
                                       pofv_top_x(), pofv_top_y(),
                                       POFV_SPIRIT_KIND_PURE, 0,                        /* 镜像面同载荷 ✓ */
                                       (int32_t)((ce_rand() % 101) + 60 + (ce_rand() % 101)));
            }
                if (pofv_spirit_spill_stop()) { ++s_cnt_send30; /* ★ 安全阀：TH09 也不送 ✓ */
                } else if (pofv_acc_take30(&s_acc, diff)) {    /* ★ TH09 那套（独立 ✓）*/
                    {   /* ★ TH09 SP 累加（一手 §4.4 ✓）：送量 = min(連爆数,100)×10 ✓、取整到 10 的倍数 ✓ */
                        int32_t sendv = (s_combo_blasts > 100) ? 1000 : (int32_t)(s_combo_blasts * 10);   /* ★ 一手：送量用**連爆数** ✓（不是槽 tA ✗）*/
                        int32_t gain  = (int32_t)((POFV_SP_RATE * (float)sendv) / 10.0f) + sendv;
                        if (s_sp + gain > POFV_SP_MAX) s_sp = POFV_SP_MAX; else s_sp += gain;
                        s_sp = (s_sp / 10) * 10;                       /* 取整到 10 的倍数 ✓ */
                    }
                    /* ⚠ 2026-10-08 订正：**不在这里扣 `a2c`** ✗ —— 一手结案说清 `a2c` = **連擊槽计时器**
                     *   （每帧 −1 ✓、击破分档加长 ✓），**不被"送"消费** ✗；送量来源是**連爆数** ✓。
                     *   ⇒ 先前接在这儿的 `pofv_acc_take2c` 已撤 ✓（`pofv_acc_take2c` 本体仍留作 TH19 口径备件 ✓）。*/
                    ce_log("pofv: 【TH09】越阈值（a2c=%d a30=%d 難度=%d）⇒ 送出**幽灵** level 1（收件门 <25 ✓）",
                           (int)s_acc.a2c, (int)s_acc.a30, (int)diff);
                    pofv_foe_spawn_gated(pofv_top_x(), pofv_top_y(),
                                          POFV_SPIRIT_KIND_GHOST_TH09, 1);          /* ★ 一手：TH09 生成类型 = `side+3` = 幽灵族 ✓ */
                    pofv_foe_incoming_push(s_inc, POFV_FOE_INCOMING_MAX,
                                           pofv_top_x(), pofv_top_y(),
                                           POFV_SPIRIT_KIND_GHOST_TH09, 1,          /* 镜像面同载荷 ✓ */
                                           (int32_t)((ce_rand() % 101) + 60 + (ce_rand() % 101)));
                }
            }        if (k >= 0) {
                /* ★★★★ 2026-10-08 **TH09 幽灵的黄/红 = 击破连锁**（只读子智能体 `f27b135c` 一手定案 ✓）：
                 *   一手（`engine/versus/th09/02-ghost-and-seal.md:126-131` ✓）：幽灵颜色 = 生成的是 `Sub0/1/2` 哪一个 ✓，
                 *   由载体 `+0xa4` 决定 ⇒ **seed 路（玩家自己送）`+0xa4 = 0` ⇒ 绿** ✓（上面那支写死 1 = 绿，**本来就是对的** ✓）；
                 *   **击破连锁** ⇒ `+0xa4 = min(level,2)` ⇒ **黄/红** ✓，其中 level = **被击破幽灵自己的 charge level**
                 *   （`(enemy+0x3380 >> 6) & 7` ✓，bits 6–8 ✓）；闸门 = `level ∈ {1,2}` ∧ **自机弹击破**（一手 `param_1 < 2` ✓
                 *   ⇒ 本卡 = `ev->kind == CE_EDEATH_KILLED` ✓）∧ 对手计数 < 0x19 ✓。
                 *   ⇒ 链：**绿 →（打它）→ 黄 →（打它）→ 红** ✓。*/
                if (k == (int)POFV_SPIRIT_KIND_GHOST_TH09 && ev->enemy && ev->kind == CE_EDEATH_KILLED) {
                    const int gl = (int)(((*(const int32_t *)((const uint8_t *)ev->enemy + 0x3380)) >> 6) & 7);
                    if (gl >= 1 && gl <= 2 && s_send40 < 0x19) {   /* ★ owner：**幽灵(TH09)与純霊/动物灵(TH19)是两个独立系统** ✗
                     *   ⇒ 我先前用 `s_cnt_send30`（TH19 那套的安全阀 ✗）是**交叉** ✗ ⇒ 已拆（TH09 链条不吃 TH19 的计数 ✓）；
                     *   限额由 **TH09 自己的出生口/族量**控制 ✓（`POFV_FAM_TH09` ✓）。*/

                        const int nl = (gl + 1 > 3) ? 3 : (gl + 1);
                        ce_log("pofv: 【TH09 连锁】击破 level %d 幽灵 ⇒ 送出 level %d（黄/红 ✓ 一手 §2.3）", gl, nl);
                        pofv_foe_spawn_gated(pofv_top_x(), pofv_top_y(), POFV_SPIRIT_KIND_GHOST_TH09, nl);
                        pofv_foe_incoming_push(s_inc, POFV_FOE_INCOMING_MAX,
                                               pofv_top_x(), pofv_top_y(),
                                               POFV_SPIRIT_KIND_GHOST_TH09, nl,
                                               (int32_t)((ce_rand() % 101) + 60 + (ce_rand() % 101)));
                    }
                }
            /* ★★ 2026-10-05 owner：「灵缺少击杀暴风」—— 暴风原先挂在 `pofv_spirit_is_ours(ev->id)` 上，
             *   但那条在 `take()` **之后**（条目已取走）⇒ 永远为假 ⇒ 暴风从不生成。
             *   改为**用 take 的返回值**决定：认出是我们的灵 ⇒ 起**灵档暴风（80 / 40 帧 / 伤害 10）**。 */
            /* ★★ ② 我方灵被击破 ⇒ **在对面模型里记一条**（owner：「送回来只能是镜像击破事件」⇒ 这里是「送去对面」那一步）✓
             *   位置 = **我方击破点**（owner 口径：「是我打死我这侧灵的 xy」✓）；type/level 取这只灵 ✓ */
            /* ★ 一手：**死亡分发**每次破坏 ⇒ 两档各 **+15** ✓；并记「最近一次破坏位置」（`+0x58`/`+0x60` ✓）*/
            /* ★★ ⑤ 第二条触发路径：**在破坏这一刻**判阈值（一手：事件驱动 ✓，**不是**每帧 ✗）
             *   两档独立：`+0x50`（150 − t/2，clamp 1400）· `+0x54`（純霊，120 − t/5，clamp 700）✓
             *   ❓ 一手的 `t` = 全局计时器 `DAT_0062b260`；我们暂用 `s_card_frames` 等价 ✓ */
            /* ★★★ MODEL §4.4（owner 2026-10-05 新增）：「两条出口同时开 ⇒ 打死 20 只会送 20 多只 ✗ ⇒
             *   **只保留「阈值」这条**，把「破坏即送」那条去掉」✓ 且 §4.4-107/109：TH19 的「送」只由 **`+0x54`** 负责
             *   （`+0x50` = 「另一条（未定名）」⇒ **不作**送扳机 ✗，只继续累积 ✓）*/
            /* ★★★ 2026-10-05 **实跑日志抓到的自维持根因**（owner：「门控这块一定有问题」✓）：
             *   `pofv_acc_take54/take50` 的 `t` 口径是**秒**（`thr54 = 120 − t/5` ✓，与容量分档
             *   `pofv_spirit_cap_for(fam, s_card_frames / 60)` **同一口径** ✓），而这里原先传的是
             *   **帧** `s_card_frames` ✗ ⇒ `t` 被放大 **60 倍** ⇒ `thr54 = 120 − t/5` 变负 ⇒
             *   被 `if (thr < 0) thr = 0` 钳成 **0** ⇒ `a54 < 0` 恒假 ⇒ **每一次击破都判「过阈值」** ✗
             *   ⇒ 送出一只 ⇒ 模型侧必然（镜像击破 / §4.6）送回一只 ⇒ 再被击破 ⇒ **自维持不衰减** ✗✓。
             *   （日志指纹：`a50=1400 a54=700` 顶死在钳位 + 每击破一条「送出」✓。）⇒ 改传**秒** ✓ */
            /* ★ MODEL §4.4：**「破坏即送」已删** ✗（只保留阈值出口 ✓）*/
            /* ★★★ ⑤ **本侧暴风送出**（owner 定案 ①：TH19 = **破坏即判定** ✓ + 連撃阈值另一条路径）
             *   ⇒ 「送出」那一刻**同时**两个效果（owner 原话）：
             *     ① **同时给对面槽位送一个 level 1** ⇒ `foe_spirit_spawn(…, 幽灵, 1)`（无延迟 ✓）
             *     ② **我这边延迟 0-100 帧随机 ＋ 动画随机帧**收到一个 level 1 ⇒ `foe_incoming_push` ✓
             *   ❓ 連撃阈值那条路径**未接**（阈值/重置口径 owner 未给 ⇒ 按纪律只留 ❓，不写实现 ✗）*/
            /* ★ MODEL §4.4：**「破坏即送」整条出口（含这发 incoming）已删** ✗
             *   ⇒ 本侧「送出」**只由阈值那条**（`if (pofv_acc_take54(...))`，其内已有本体 + 镜像面 ✓）负责 ✓ */
            /* ★★★ ③ **镜像击破事件**：对面「打死它那边**离我方击破点最近**的那只」⇒ **送回来**
             *   ⇒ 回来那只按 owner 口径用「**自机位置**（= 对面位置）」算距离²定档 ⇒ 放回一隻動物霊 ✓（纯数据）*/
            {
                int32_t ot = 0; float ox = 0.0f, oy = 0.0f;
                /* ★ MODEL:159：**只有 level-2 incoming 才禁镜像** ✓（本地升级出的 level-2 不禁 ✓）*/
                if (!(inc && lv == 2) &&
                    pofv_foe_spirit_mirror_kill(s_foe, POFV_FOE_SPIRIT_MAX, (uint32_t)ce_rand(), ev->x, ev->y, &ot, &ox, &oy)) {
                    const float *pp = ce_player_pos();
                    float d2 = pp ? (ox - pp[0]) * (ox - pp[0]) + (oy - pp[1]) * (oy - pp[1]) : -1.0f;
                    int32_t back = (int32_t)pofv_spirit_kind_random_animal();
                    /* ★★★ 2026-10-05 owner：「我击破一个，**在我自己这边**直接生成下一个」✗
                     *   ⇒ 方向错：按 §9.1 `生成侧 = 对面` ✓ ⇒ 送回的那只**也进对面模型** ✓（不在本侧冒 ✗）。
                     *   ❓ 本侧要看到东西，只能来自**模型侧事件**的镜像回程（待 owner 定那条路）✓ */
                    /* ★★★ 回程 = **模型 → 本侧**，且**走载体延迟** ✓（owner：不是「本侧立刻冒下一只」✗）
                     *   ⇒ 用既有的「延迟到达」队列 ✓：delay = 60 + rand(0..100) + rand(0..100)（同一手口径 ✓）*/
                    /* ★★★★ 2026-10-06 owner：「**你只管"我要发EX"了，你不要管 EX 是什么**」✓
                     *   ⇒ 这里只做**发射决定** ✓，不生成任何 EX 本体 ✗（本体归外部/后续 ✓）。
                     *   判据（一手 `08-extra-attack.md` §8.2 ＋ §8.1 ✓）：
                     *     `s_ex_gauge >= s_ex_thr[level]` ⇒ **扣减**（`-=`，**不清零** ✓）⇒ 本次**改为 EX**。
                     *   「改为 EX」在本卡的可落地语义 = **此次不回程** ✓ ＋ **循环终止** ✓（owner 裁定「触发 EX = 出循环」✓）。*/
                    {   const int lvi = (s_ex_level >= 0 && s_ex_level <= 7) ? s_ex_level : 0;
                        if (s_ex_gauge >= POFV_EX_FIRE_AT) {             /* ★ 一手 §11.1：`> 99` ⇒ 发射 ✓ */
                            s_ex_gauge -= POFV_EX_FIRE_AT;               /* 扣减、不清零 ✓ */
                            s_ex_cool = POFV_EX_COOL;                    /* ★ 一手：冷却 60 帧 ✓ */
                            (void)lvi;
                            pofv_foe_spirit_loop_exit(s_foe, POFV_FOE_SPIRIT_MAX);   /* 循环在此终止 ✓ */
                            /* ★★★★ 2026-10-08 **owner 口径**（原话）：「是**触发 EX 的通路**，**正常给发灵**（有 **40** 上限）
                             *   但**同时**还会**在对面向对面发射 EX 攻击**，**EX 攻击别接留个口**」✓
                             *   ⇒ 所以 EX 这一支要**同时**做两件：
                             *     ① **照常送灵** ✓（但仍受 §8.38 的**每侧 40** 限制 ✓ —— 到 40 就不送 ✓）；
                             *     ② **对侧 EX 攻击** = `pofv_foe_spirit_loop_exit()` **占位/留口** ✓（不实现实体 ✗）。
                             *   ⚠ 我先前自己发明的「EX 灵（免疫结界 ✗／接触致伤 ✗）」**已全部拆掉** ✓。*/
                            if (s_send40 < POFV_SEND40_LIMIT)
                                pofv_foe_incoming_push(s_inc, POFV_FOE_INCOMING_MAX, ox, oy, back, 0,
                                                       (int32_t)((ce_rand() % 101) + 60 + (ce_rand() % 101)));
                            ce_log("pofv: **本侧发 EX**（gauge %d − %d ⇒ %d）⇒ 本次**不回程** ✓（EX 本体不归本卡 ✗ owner 口径 ✓）",
                                   (int)s_ex_gauge + (int)s_ex_thr[lvi], (int)s_ex_thr[lvi], (int)s_ex_gauge);
                        } else if (s_send40 >= POFV_SEND40_LIMIT) {
                            /* ★ §8.38 ✓：本侧已满 **40** ⇒ 该类通路**本局永久不再送灵** ✓（真的脱离循环 ✓）*/
                            if (!(s_card_frames % 300))
                                ce_log("pofv: 本侧送灵已达上限 %d ⇒ **本局不再送灵** ✓（§8.38；对面仍可能吃 EX ✓）", POFV_SEND40_LIMIT);
                        } else {
                            pofv_foe_incoming_push(s_inc, POFV_FOE_INCOMING_MAX, ox, oy, back, 0,
                                                   (int32_t)((ce_rand() % 101) + 60 + (ce_rand() % 101)));
                        }
                    }
                    ce_log("pofv: 镜像击破 ⇒ 模型侧收回一隻（type %d）⇒ 按距离²定档 kind %d d2=%.0f ⇒ **延迟回程**（本侧落 ✓）",
                           (int)ot, (int)back, d2);
                }
            }
            { int32_t i3; for (i3 = POFV_UI_EV_MAX - 1; i3 > 0; --i3) s_ui_ev[i3] = s_ui_ev[i3 - 1];
              s_ui_ev[0].what = POFV_UI_EV_STORM;
              s_ui_ev[0].ttl = POFV_UI_EV_TTL + POFV_UI_STORM_DELAY; }   /* ★ 镜像暴风：**延迟** ✓（先等 delay 再显示）*/
            /* ★ 连锁暴风 ⇒ hits 窗口计时器归零 + **按这只灵的 class 取窗口** ✓（owner 口径 ✓）*/
            s_chain_storm_age = 0;
            /* kind 用本块已有的 `k` ✓；level 直接**从被击破敌人身上读回**（= 我们生成时写的
             * charge level `+0x3380` bits 6–8 ✓，比另找局部变量可靠 ✓）。*/
            {
                int32_t lvl = 0;
                if (ev->enemy) lvl = (int32_t)(((*(const int32_t *)((const uint8_t *)ev->enemy + 0x3380)) >> 6) & 7);
                s_chain_window = pofv_hits_chain_x_for(k, lvl);
            }
            if (pofv_storm_spawn_tier(&s_storm, ev->x, ev->y,
                                      POFV_SPIRIT_R_TARGET, POFV_SPIRIT_TWEEN_FRAMES,
                                      POFV_SPIRIT_HIT_DMG) < 0) {
                ce_log("pofv: 灵档暴风池满（%d）⇒ 丢弃", POFV_STORM_MAX);
            }
            int32_t nk = k;
            if (k == POFV_SPIRIT_KIND_PURE) {
                const float *pp = ce_player_pos();
                float d2 = (pp ? (ev->x - pp[0]) * (ev->x - pp[0]) + (ev->y - pp[1]) * (ev->y - pp[1]) : -1.0f);
                int q;
                nk = (int32_t)pofv_spirit_kind_random_animal();
                ce_log("pofv: 純霊被击破 d2=%.0f ⇒ **送去对面 = 暂存** %d 帧后回来变動物霊 kind=%d",
                       d2, POFV_SPIRIT_STASH_FRAMES, (int)nk);
                for (q = 0; q < POFV_SPIRIT_STASH_MAX; ++q)
                    if (s_stash_ttl[q] <= 0) {
                        s_stash[q][0] = ev->x; s_stash[q][1] = ev->y;
                        s_stash_kind[q] = nk; s_stash_ttl[q] = POFV_SPIRIT_STASH_FRAMES;
                        break;
                    }
                return;                   /* ★ 純霊：**不立刻生成**，走暂存 */
            }
            /* ★ owner 指出：日志把 TH09 幽灵写成「動物霊」会**误导因果**（以为幽灵生動物霊）。
             *   一手口径：**動物霊只从純霊来**（純霊击破 ⇒ 暂存 ⇒ 回来时按距离² 变動物霊）；
             *   幽灵(kind 4) 属 TH09 那一族 ⇒ 它死了就**返还一只幽灵**（同族），与動物霊无关。 */
            if (k == POFV_SPIRIT_KIND_GHOST_TH09) {
                /* ★ 一手 §2 level 链：`+0xa4=min(level,2)`、`+0xa6=min(level+1,3)` ⇒ 绿→黄→红，到 3 不升。 */
                int32_t a4 = 0, a6 = 0;
                pofv_mirror_level_gain(lv, &a4, &a6);
                ce_log("pofv: TH09 幽灵(level %d)被击破 ⇒ 返还 level %d（一手 min(level+1,3)）", (int)lv, (int)a6);
                lv = a6;
            }
            ce_log("pofv: %s被击破 ⇒ **只记账，不本地生成**（回程只由镜像那条产生 ✓ owner 2026-10-06）",
                   (k == POFV_SPIRIT_KIND_GHOST_TH09) ? "TH09 幽灵(kind=4)"
                   : (k == POFV_SPIRIT_KIND_WOLF) ? "動物霊·狼(kind=1)"
                   : (k == POFV_SPIRIT_KIND_OTTER) ? "動物霊·獺(kind=2)"
                   : (k == POFV_SPIRIT_KIND_EAGLE) ? "動物霊·鷲(kind=3)"
                   : "灵");
            /* ★★★ MODEL §1-3/§7-3：本侧生成口**也要过上限门** ✓（核实报告：这处原先无门 ✗，会绕过 cap）
             *   满 ⇒ **折算成计数**（`++s_point_q`），**不是**丢包 ✗ */
            /* ★★★★ 2026-10-06 owner（日志实锤后裁定）：「**击杀 ⇒ 本地排一只同族 —— 肯定去掉啊！**」✗
             *   直接删掉 ✓（原代码就在此由 `s_spirit_q` **本地生成一只同族** ✗ ⇒ **不消耗对面模型条目**
             *   ⇒ 正是 owner 铁律禁止的「不消耗条目的回程」＝自维持 ✓；日志原文：
             *   `pofv: 動物霊·狼(kind=1)被击破 ⇒ 返还一只同族（与動物霊无关…）` ✓）。
             *   ⇒ 现在**回程只由**「镜像击破 ⇒ 模型侧收回一隻（消耗条目 ✓）⇒ 延迟回程」那条产生 ✓
             *     （日志里那条运行正常 ✓）⇒ 闭环断开 ✓。
             *   ⚠ 原本挂在被删 `if` 后面的 `else { ++s_point_q; … }`（本地生成口的"队满折算"兜底 ✓）
             *     也**一并删除** ✓ —— 本地生成口整个不存在了 ✓，兜底自然无从谈起 ✓。*/
            return;                       /* 我们的灵：**不再**走下面那条临时生成口 */
        }
    }
        /* ★★★★ 2026-10-08 owner 实跑（「你似乎没有给其他所有怪物一个『**启动连击**』的能力；
         *   测试下来只有灵有这个能力，其他怪物只能在启动之后加入 hit 里面，这肯定不对」✗）——
         *   根因：这一整段喂食（`+15` / 連擊槽分档 / 連爆数 / hits）原先**放在 1064 那个「灵专属」块里** ✗
         *   ⇒ **妖精/小怪打死完全不喂** ✓ ⇒ 只有灵能启动连击 ✓。
         *   ⇒ **上移到「每次真实击破」这一层** ✓（一手 `FUN_0050c1f0` 对妖精死亡同样计数 ✓）。*/
        /* ★★★ 2026-10-08 诊断（owner：「现在好像变成只有**固化灵**才能开了」✗ ⇒ 先确认真值 ✓）：
         *   打在喂食点 ⇒ 每次真实击破都该出现；kind 字段顺带告诉我们**是哪条路走到这** ✓。*/
        /* ★★★★ 2026-10-08 修 bug（owner：「1s 的长度也大部分是 0hit，**肯定有 bug**」✓）：
         *   怀疑点：`s_chain_storm_age` 的归零原先只放在**起暴风的那几处**✗，而妖精等路径未必经过
         *   ⇒ 计时器一路涨到 ≥ 窗口 ⇒ **每次击破后立刻就被清** ✓（正是「每行 cur=0」✓）。
         *   ⇒ **在喂食点直接归零** ✓ —— 这里**每次真实击破都必然到达**（`[FEED]` 日志 69 行已证明 ✓）。*/
        s_chain_storm_age = 0;
        s_chain_seen = 1;
        ce_log("pofv: [FEED] 击破喂连击：id=%u flags=%#x ours=%d a2c=%d cur=%d",
               (unsigned)ev->id, (unsigned)ev->flags,
               (int)pofv_spirit_is_ours(ev->id), (int)s_acc.a2c, (int)s_hits.cur);
        pofv_acc_add50(&s_acc, 15);          /* TH19 一手：大块口径（死亡分发）✓ */
        pofv_acc_add54(&s_acc, 15);          /* TH19 一手 ✓ */
        /* ★★★ owner 2026-10-06：「**th09 和 th19 独立运行**，按一手 + **真实难度**」⇒
        *   TH09 是**另一套**（步长 = 一手 `param_2`/`param_3` ✓，阈值吃难度不吃时间 ✓）⇒ **不混用** ✗ */
        /* ★★★ 2026-10-08 一手（§3.34 ✓）：击破 ⇒ **連擊槽分档加长**（不是「加固定值」✗）：
        *   `<30 ⇒ +30` / `<50 ⇒ +5` / `<60 ⇒ +2` / `≥60 ⇒ +0` ⇒ **上限 60** ✓。*/
        {
        int32_t *ta = &s_acc.a2c;
        if      (*ta < 30) *ta += 30;
        else if (*ta < 50) *ta += 5;
        else if (*ta < 60) *ta += 2;
        if (*ta > POFV_COMBO_CAP) *ta = POFV_COMBO_CAP;
        ++s_combo_blasts;                       /* 一手：連爆数 `+0x38` 随击破递增 ✓ */
        s_sp_life = *ta + 45;                   /* ★ 一手：`tB := tA + 45` ✓ */
        }
        pofv_acc_add30(&s_acc, POFV_ACC09_STEP30);
        s_chain_seen = 1;                    /* ★ 真有连锁了 ⇒ 允许之后清 hits ✓ */
        pofv_hits_bump(&s_hits, 1);          /* ★ hit（連爆数）＋1（灵侧口径 ✓，owner 图 ✓）*/

    if (pofv_spirit_is_ours(ev->id)) {
        if (pofv_storm_spawn_tier(&s_storm, ev->x, ev->y,
                                  POFV_SPIRIT_R_TARGET, POFV_SPIRIT_TWEEN_FRAMES,
                                  POFV_SPIRIT_HIT_DMG) < 0) { /* 池满：下面是统一的日志 */ }
    s_chain_storm_age = 0;                                  /* ★ 小怪/妖精档 ⇒ class 0 ⇒ 36 帧 ✓ */
            s_chain_window = POFV_HITS_CHAIN_X_DEFAULT;
            } else if (pofv_storm_spawn(&s_storm, ev->x, ev->y, POFV_STORM_LIFE_FRAMES) < 0) {
        if (!s_cap_logged) {
            ce_log("pofv: storm pool full (%d) — storm dropped", POFV_STORM_MAX);
            s_cap_logged = 1;
        }
    }
}

static int on_tick_2(ce_card_t *c)
{
    int i;
    (void)c;
    /* ★★ A 路：出队**生成真敌机灵**（安全上下文 = 这里；死亡栈里只入过队）。
     *   生成后把它的 enemy_id 记进本卡的灵账 ⇒ 它**自然死亡**时 `on_enemy_death` 会认出来，
     *   走**灵档**（80 / 40 帧 / 伤害 10）—— 验收点。 */
    {   /* ★ 灵圆跟随：账里每个 id 查一次敌机（不缓存指针），活着就跟、死了就淡出 */
        uint8_t *em = CE_ENEMY_MGR();
        int z;
        for (z = 0; z < POFV_SPIRIT_MAX; ++z) {
            uint8_t *e;
            /* ★★★ 2026-10-08 bug 修复（owner 实跑：「幽灵再也不出现」＋「版顶正中一只**朝右的狼灵**，
             *   同时出现一个没有任何 VM 的敌人，打死这个敌人后版顶狼灵消失」）——根因就是这一行：
             *   · TH09 幽灵按 owner 口径**不挂灵圆**（`:2140` 生成后立刻删圆、`s_spirit_vm[sl] = 0` ✓）；
             *   · 而这里**只认灵圆** ⇒ 幽灵的槽**每帧都被 continue 跳过** ✗ ⇒ 它的本体 VM
             *     既不跟随敌机、也不换图 ⇒ 停在出生点 (0,0) = **版顶正中**，用脚本 400 的**默认狼图** ✓
             *     ⇒ 实跑看到的就是那只"朝右的狼灵" ✓；
             *   · 打死那台敌机 ⇒ 死亡清理删掉本体 ⇒ "狼灵消失" ✓ —— 与"幽灵再也不出现"是**同一个 bug** ✓。
             *   ⇒ 判存活改成「**灵圆 或 本体**」（纯灵/动物灵两件都有 ⇒ 它们的路径行为不变 ✓；
             *     灵圆相关的调用都自带 `id==0` 保护 ✓ 见 `sdk.h` 的 `ce_anm_set_pos`/`ce_anm_interrupt` ✓）。*/
            if (!s_spirit_vm[z] && !s_spirit_body[z]) continue;
            e = (em && s_spirit_id[z]) ? ce_enemy_by_id(em, (int32_t)s_spirit_id[z]) : 0;
            if (e) { s_spirit_miss[z] = 0; }
            /* ★★★ 2026-10-08 诊断（owner：「现在没有横着飞的，但也没有斜着飞的」✗）：
             *   与其继续猜 e `+0x14c8` 是不是 `%F0` ✗，**直接打幽灵的位置随时间怎么变** ✓
             *   （`CE_ENEMY_POS_X/Y` 是**已知**字段 ✓ ⇒ 位置增量就是真实速度方向 ✓），
             *   顺带打**我们写进去的 charge level**（bits 6–8 ✓）确认连锁闸门有值 ✓。*/
            if (e && s_spirit_kind[z] == POFV_SPIRIT_KIND_GHOST_TH09 && !(s_card_frames % 15)) {
                ce_log("pofv: [GHOSTPOS] z=%d pos=(%.1f,%.1f) lvl=%d",
                       z, (double)*(const float *)(e + CE_ENEMY_POS_X),
                       (double)*(const float *)(e + CE_ENEMY_POS_Y),
                       (int)(((*(const int32_t *)(e + 0x3380)) >> 6) & 7));
            }
            if (e) {
                /* ★ 入场运动：按 kind / age 取速度并推进（一手 `ins_404` + `ins_405`）*/
                if (!s_spirit_dis[z]) {   /* ★ EX 不再免疫结界 ✗（我发明的那套已拆 ✓）*/   /* ★ 免疫己方系统：EX 不吃固灵结界 */
                    /* ★★★ goal 第 25 轮：真触发 = **按住 C 展开的固灵结界**（一手 §3.5）。
                     *   `held` = 本帧 active 回调打了时间戳（= 按住 C ✓）；形状用 `pofv_aura_contains()` ✓。 */
                    const float *pp = ce_player_pos();
                    /* ★★★ owner 2026-10-06：「**固灵结界——按 shift**」✓ ⇒ 直接读键（不再依赖那个
                     *   「时间戳」变量：核实发现它**只有读、没有写** ✗ ⇒ 结界从来没亮过 ✓，这就是「该做结界了」的根因 ✓）。
                     *   ★ 2026-10-10 **逐处定类**：这一处是「结界**罩到灵** ⇒ 沈静化」= **结界**那一类
                     *   ⇒ 用 `pofv_aura_key()` = **C ∨ Shift** ✓（owner：「Shift 只开结界」✓；不是蓄力 ✗）。
                     *   键值用 SDK 现成的 `CE_VK_C`（`sdk.h:827`）/`CE_VK_SHIFT`（`:831`）✓，不自己造 ✓。*/
                    const int held = pofv_aura_key();
                    if (pp && held) {
                        float ddx = *(const float *)(e + POFV_ENEMY_ABS_POS) - pp[0];
                        float ddy = *(const float *)(e + POFV_ENEMY_ABS_POS + 4) - pp[1];
                        if (pofv_aura_hit_here(ddx, ddy, *(const float *)(e + 0x110) * 0.5f)) {
                            s_spirit_dis[z] = 1;
                            /* ★★★ 目标 ③ 收口：改走 **ECL 通道** —— 卡侧只把 `$I0` 写成 **1**（沈静化），
                             *   耐久/停住/缓慢上浮全部交给 ECL 的 `CESpirit109_calm`（一手 `lifeSet(50)` +
                             *   `GhostDisableMove` 的 `moveVel(-1.5707964f, 0.02f)`）✓
                             *   （`$I0..$I7` = `enemy+0x14b8` 起，见 `engine.h`）*/
                            *(int32_t *)(e + CE_ENEMY_ECL_I0) = 1;
                            /* ★★ ④ **镜像固化事件**：对面「固化它那边**离我方事件点最近的未固化**的那只」✓
                             *   （owner：「固化不会固化已固化的，这个跳过」✓；`sealed` 后 **160 帧**消失、期间缓慢上浮 ✓）*/
                            { int32_t i3; for (i3 = POFV_UI_EV_MAX - 1; i3 > 0; --i3) s_ui_ev[i3] = s_ui_ev[i3 - 1];
                              s_ui_ev[0].what = POFV_UI_EV_SEAL; s_ui_ev[0].ttl = POFV_UI_EV_TTL; }  /* 镜像固灵：停 3 s ✓ */
                            pofv_foe_spirit_seal(s_foe, POFV_FOE_SPIRIT_MAX,
                                                 *(const float *)(e + POFV_ENEMY_ABS_POS),
                                                 *(const float *)(e + POFV_ENEMY_ABS_POS + 4));
                            /* ★ 一手 §3.5 ③「**霊力アイテムを放出**」：结界罩到灵的那一刻放一个灵力道具。
                             *   ❓ 用现成的道具口（`ce_money_pocket`）；一手 §5 的道具另有掉率口径（每 100 hits），
                             *     那条属于「掉落」这一支，等那一支落地时再合并（现在只做「结界放道具」这条 ✓）。 */
                            /* ⏸ **占位**（owner 2026-10-06：「**灵力道具先留口搁置**」✓）：
                             *   一手勘误（`06-aura.md:68`）说「オーラ消弹本身**不掉道具**」✗，真掉落来自**敌/灵死亡侧** ✓；
                             *   而 §5 又定「✅ 做（**落点待定**）」⇒ 目标是**接本仓 116 的季节道具/碎片骨架** ✓。
                             *   ⇒ 这里先用现成口占位、**等 owner 解搁** ✗（不动 116 任何文件 ✓）。*/
                            ce_money_pocket(1);
                            ce_log("pofv: 结界放出灵力道具（一手 §3.5 ③）");
                        ce_log("pofv: 灵 id=%u 被吸灵领域罩到 ⇒ 沈静化（停住 + 缓慢上浮 + 耐久骤降到 %d，一手 GhostDisableMove / lifeSet）",
                                   (unsigned)s_spirit_id[z], POFV_SPIRIT_DIS_HP);
                        }
                    }
                }
                if (s_spirit_dis[z]) {
                    /* ★★ 固化态角度（放这里最稳：本块**只在固化时为真** ✓，不受外层作用域影响 ✗）：
                     *   狼 = 逆时针 90°（`+π/2` ✓）、鹫 = 顺时针 90°（`−π/2` ✓）—— owner 2026-10-06 实跑定案 ✓。*/
                    if (s_spirit_body[z]) {
                        const int32_t kd2 = s_spirit_kind[z];
                        if (kd2 == (int)POFV_SPIRIT_KIND_WOLF)
                            ce_anm_set_rotation((uint32_t)s_spirit_body[z], 0.0f, 0.0f, 1.5707964f);
                        else if (kd2 == (int)POFV_SPIRIT_KIND_EAGLE)
                            ce_anm_set_rotation((uint32_t)s_spirit_body[z], 0.0f, 0.0f, -1.5707964f);
                    }
                    /* 一手 `GhostDisableMove`（`moveVel(-1.5707964f, 0.02f)` = 缓慢上浮）已交给 **ECL 的
                     *   `CESpirit109_calm`** ⇒ 卡侧不再手写速度（原文 `vx = 0; vy = -0.02f;` 已删）✓ */
                    /* ★ W4 自然成佛：满帧数 ⇒ ①清账（不算击破）②入暂存（成仏 = 变動物霊送出去）③ life = 0 让它消失。 */
                    if (++s_spirit_dis_age[z] >= POFV_DISABLE_JOUMON_FRAMES) {
                        const float *pp2 = ce_player_pos();
                        float dd2 = (pp2 ? (*(const float *)(e + POFV_ENEMY_ABS_POS) - pp2[0]) *
                                           (*(const float *)(e + POFV_ENEMY_ABS_POS) - pp2[0]) +
                                           (*(const float *)(e + POFV_ENEMY_ABS_POS + 4) - pp2[1]) *
                                           (*(const float *)(e + POFV_ENEMY_ABS_POS + 4) - pp2[1]) : -1.0f);
                        int q2;
                        ce_log("pofv: 灵 id=%u 沈静化满 %d 帧 ⇒ **自然成佛（不算击破）⇒ 直接消失**（固化**永不回返** ✓）",
                               (unsigned)s_spirit_id[z], POFV_DISABLE_JOUMON_FRAMES);
                        s_spirit_id[z] = 0; s_spirit_kind[z] = -1;   /* ① 清账 ⇒ 死亡不再算击破 */
                        /* ★★★ 2026-10-06 owner 铁律：「**只要被固化，这个灵一定不会再回来了**」✓✗
                         *   ⇒ 这里原来「入暂存 ⇒ 一会儿变動物霊送出去」**正是自维持的闭环** ✗
                         *     （固化 ⇒ 成佛 ⇒ 送回 ⇒ 再固化 ⇒ …）⇒ **掐掉送回** ✓，只保留「清账 + 消失」✓。
                         *   （`if (0)` 是**死代码开关**：留着原逻辑便于对照，运行时**绝不执行** ✓。）*/
                        if (0) for (q2 = 0; q2 < POFV_SPIRIT_STASH_MAX; ++q2)
                            if (s_stash_ttl[q2] <= 0) {              /* ② 入暂存（回来时变動物霊）*/
                                s_stash[q2][0] = *(const float *)(e + POFV_ENEMY_ABS_POS);
                                s_stash[q2][1] = *(const float *)(e + POFV_ENEMY_ABS_POS + 4);
                                s_stash_kind[q2] = (int32_t)pofv_spirit_kind_random_animal();
                                s_stash_ttl[q2] = POFV_SPIRIT_STASH_FRAMES;
                                break;
                            }
                        *(int32_t *)(e + CE_ENEMY_ECL_I0) = 999;   /* ③ 喊走：ECL 飞向正上方 `delete()` ⇒ **离场不算击破** ✓（不再由卡侧写 life=0）*/
                        continue;
                    }
                }
                /* ★★★★ 2026-10-08 **owner 定案：去掉减速、保留视觉** ✓ ——
                 *   一手（`06-aura.md` §8.1/§8.2 ✓）：TH19 的固灵领域碰到**敌弹/敌人**会让它们 **×0.5 变慢**
                 *     （弹写 `+0x101c..24`、敌写三个字段 ✓，每帧覆写、无渐变 ✓）。
                 *   ★ owner：「但是这样**对传统作会更难**，所以**去掉**」✗ ⇒ 本卡**不再施加减速** ✓
                 *     ⇒ 原先那行 `$I2 = in_aura`（ECL 侧据此换一档慢的 `moveVel` ✓）**改为恒 0** ✓
                 *       （`$I2` 在本卡**只有这一处**使用 ⇒ 改 0 即等于彻底不减速 ✓）。
                 *   ★ owner：「**但是保留**固灵领域碰到敌方弹幕和敌人的**视觉效果**」✓ ⇒
                 *     本卡的视觉**不受影响** ✓：结界本体 = 402–406（`scale` = 判定半径 ✓，一手「视觉=判定」✓）
                 *     ＋ 碰弹时已有的表现 ✓ —— ★ 一手 TH19 **没有**额外染色/自机圈（`06-aura.md` §8.1 ✓），
                 *       所以**不引入** TH15(115) 那种逐弹染色 ⇒ **与 115/111 零共享** ✓（也就没有"施加两份"的问题 ✓）。
                 *   ⚠ 保留上面那段 `pofv_aura_hit_here()` 的**判定**（现在不写 `$I2`，但保留探测口 ✓），
                 *     以便日后要做「碰到的视觉」时直接复用 ✓；当前它不影响任何运动 ✓。*/
                *(int32_t *)(e + CE_ENEMY_ECL_I0 + 8) = 0;   /* `$I2` 恒 0 ⇒ **不减速** ✓（owner 定案 ✓）*/
                /* ★★★ 2026-10-05 owner 实跑（ECL 驱动后）：**依旧 90°** ⇒ 证明当年那行补偿**方向是对的**，
                 *   我上一版按「一手脚本里没有旋转」把它删了是**误删** ✗ ⇒ 现在**重新加回**（只给新月形的幽灵；
                 *   純霊是球、动物不是新月 ⇒ 只幽灵需要 ✓）。❓ 方向若反了改成 −1.5707964f。 */
                /* ★★★ 2026-10-06 owner 实跑：「**狼灵和大鹫灵的贴图得顺时针转 90°**」✓ ⇒ 按族分角度：
                 *   · **幽灵**（新月形 ✓）：保持 **+π/2**（= 1.5707964 ✓，此前你确认过 ✓）；
                 *   · **狼 / 鹫**：**顺时针 90°** ⇒ **−π/2**（= −1.5707964 ✓）；
                 *   · **水獭 / 純霊**：不转 ✓（水獭那条「运动」另说 ⇒ ❓ 见 `TUNING` 待确认 ✓）。
                 *   ⚠ 若某族方向反了 ⇒ 只改对应那一行符号即可 ✓（EX 档另有 π 覆盖 ✓）。*/
                if (s_spirit_body[z]) {
                    const int32_t kk = s_spirit_kind[z];
                    if (kk == POFV_SPIRIT_KIND_GHOST_TH09)
                        /* ★★★ 2026-10-08 owner：「**冲着人飞的幽灵，也得转到那个方向**」✓ ——
                         *   原来写死 +π/2（= 只有正下方直落是对的 ✗）；现在用**出生角** ✓
                         *   （本引擎角度约定 0=+x、π/2=+y 正下方 ✓ ⇒ 精灵自然朝向 = +x ⇒ 转 `ang` 即朝向飞行方向 ✓）。
                         *   ⇒ 直落时 `ang ≈ π/2` ⇒ 与原来**完全一致** ✓；斜飞（约 1/5）时就**朝向自机** ✓。*/
                        ce_anm_set_rotation((uint32_t)s_spirit_body[z], 0.0f, 0.0f, s_spirit_entry_ang[z]);
                    /* ★★★ owner 2026-10-06：「**固化大鹫需要顺时针转 90°**」✗ ⇒ 固化态单独一个角度：
                     *   正常态是 +90°（上一轮按「180° 反了」定的 ✓），**固化态取 −90°（顺时针 ✓）**。*/
                    /* ★★★ owner 2026-10-06 实跑两条定案：
                     *   ①「**固化狼灵需要逆时针旋转 90°**」✓ ⇒ 固化态狼 = `+π/2`（CCW ✓）；
                     *   ②「**固化大鹫灵需要顺时针旋转 90°**」✓ ⇒ 固化态鹫 = `−π/2`（CW ✓）。*/
                    else if (s_spirit_dis[z] && kk == (int)POFV_SPIRIT_KIND_WOLF)
                        ce_anm_set_rotation((uint32_t)s_spirit_body[z], 0.0f, 0.0f, 1.5707964f);    /* 固化狼：逆时针 90° ✓ */
                    else if (s_spirit_dis[z] && kk == (int)POFV_SPIRIT_KIND_EAGLE)
                        ce_anm_set_rotation((uint32_t)s_spirit_body[z], 0.0f, 0.0f, -1.5707964f);
                    /* ★★★ 2026-10-06 owner 实跑（第二次）：「**固化大鹫依旧头朝右、固化狼依旧要逆时针**」✗
                     *   ⇒ 根因：固化角度我写在上面 `if (s_spirit_dis[z])` 块里 ✓，但**下面这条正常态分支同一帧又执行**
                     *     并把它**覆盖**成 `+π/2` ✗ ⇒ 所以固化态永远显示 +90° ✓（我上两次都白改 ✗）。
                     *   ⇒ 给正常分支加「**非固化**」条件 ✓（固化态角度只由上面那一处负责 ✓）。*/
                    else if (!s_spirit_dis[z] && (kk == POFV_SPIRIT_KIND_WOLF || kk == POFV_SPIRIT_KIND_EAGLE))
                        /* ★★★ owner 2026-10-06 实跑订正：「**大鹫灵 180° 反了**、狼灵那可能也是 180° 反了」✗
                         *   ⇒ 上一版是 **顺时针 90°（−π/2）** ✗，实跑看是**倒的** ⇒ 再翻 180° ⇒ 取 **+π/2** ✓
                         *   （= 幽灵同角度 ✓；两族若仍有个别反向，只需改这一行符号 ✓）。*/
                        ce_anm_set_rotation((uint32_t)s_spirit_body[z], 0.0f, 0.0f, 1.5707964f);
                }
                /* ★★ 次要项③：**幽灵寿命 5000 帧**（一手 `+5000: enemy_delete()`）——
                 *   到点发 `$I0 = 999` ⇒ 走 ECL 离场支（**离场不算击破** ✓，与「成佛」同一条路 ✓）*/
                if (s_spirit_kind[z] == POFV_SPIRIT_KIND_GHOST_TH09 &&
                    s_spirit_age[z] >= POFV_GHOST_LIFE_FRAMES) {
                    *(int32_t *)(e + CE_ENEMY_ECL_I0) = 999;
                    ce_log("pofv: 幽灵满 %d 帧（一手 +5000: enemy_delete）⇒ 喊走（不算击破）",
                           POFV_GHOST_LIFE_FRAMES);
                }
                ++s_spirit_age[z];
                ce_anm_set_pos((uint32_t)s_spirit_vm[z],
                               *(const float *)(e + POFV_ENEMY_ABS_POS),
                               *(const float *)(e + POFV_ENEMY_ABS_POS + 4), 0.0f);
                if (s_spirit_id[z] && !s_spirit_body[z]) {
                    /* ââ 2026-10-08 owner：「依旧无本体（狼灵）」â
                     *   ⇒ 生成那一帧 `ce_anm_spawn` 可能返回 **0**（anm 未就绪/池满 â）而代码**从不重试** â
                     *   ⇒ 每帧补建（有敌机、无本体 ⇒ 重试）✓；成功后下一帧就走上面的跟随支 ✓。*/
                    int bs2 = (s_spirit_kind[z] == POFV_SPIRIT_KIND_WOLF ||
                               s_spirit_kind[z] == POFV_SPIRIT_KIND_OTTER ||
                               s_spirit_kind[z] == POFV_SPIRIT_KIND_EAGLE)
                              ? (int)CE_ANM_ABILITY_SCRIPT_POFV109_SPIRIT_BODY_ANIMAL
                              : (int)CE_ANM_ABILITY_SCRIPT_POFV109_SPIRIT_BODY;
                    s_spirit_body[z] = (int32_t)ce_anm_spawn(CE_ABILITY_ANM(), bs2, 8);
                }

                if (s_spirit_body[z]) {          /* ★ 本体：跟随 + **每 6 帧**换一帧（一手）*/
                    ce_anm_set_pos((uint32_t)s_spirit_body[z],
                                   *(const float *)(e + POFV_ENEMY_ABS_POS),
                                   *(const float *)(e + POFV_ENEMY_ABS_POS + 4), 0.0f);
                    /* 卡侧从没给本体写 sprite ✗ ⇒ 幽灵被画成狼（脚本 400 写死 SPIRIT_WOLF_0）✗
                     * ⇒ 每帧按 kind/level/固化/帧号写正确 sprite ✓（每 6 帧一帧，跟脚本节奏 ✓）。*/
                    ce_anm_set_sprite(CE_ABILITY_ANM(), (uint32_t)s_spirit_body[z],
                                      pofv_spirit_body_sprite(s_spirit_kind[z], s_spirit_level[z],
                                                              (int)s_spirit_dis[z],
                                                              (int)(s_spirit_age[z] / 6)));
                    /* ★★★★ 2026-10-08 只读调查定案（子智能体 `2509318b` ✓）：**灵不该有体术判定** ✓。
                     *   一手（`engine/player/th18/03-player-hit-gate.md:164-171` ✓）：体术段前置闸在
                     *   `EnemyData__step_game_logic 0x42ed40` 的 `0x42f51c`：
                     *     `zEnemyData+0x5108 > 0`（= **`no_hitbox_timer.current`** ✓，
                     *      绝对偏移 `zEnemy + 0x122c(CE_ENEMY_DATA) + 0x5108` = **`e + 0x6334`** ✓）
                     *     ⇒ **非 0 ⇒ 跳过接触致伤判定** ✓。
                     *   ⇒ 于是**每帧写 ≥1** 即可关掉体术 ✓（引擎自有机能 ✓；**不碰 `flags_lo`** ✗
                     *     ⇒ 不影响被自机弹命中/锁定/计数 ✓；也不用去动 `setHitbox` ✗ —— owner 指出：
                     *     体术判据是"距离 ≤ 双方盒之和" ⇒ 把敌人盒设 0 **永远得不到"无判定"** ✓）。*/
                    *(int32_t *)(e + 0x6334) = 2;   /* ★★ owner 定：**所有灵都没有体术判定** ✓（我先前发明的"EX 例外"是错的 ✗ 已拆）*/
                    if (!(s_card_frames % 60)) {   /* ★ 运行时 hitbox 实值（owner：所有灵都有体术判定 ✗）*/
                        const float *hb2 = (const float *)(e + CE_ENEMY_DATA + 0x118);
                        const float *hu2 = (const float *)(e + CE_ENEMY_DATA + 0x110);
                        ce_log("pofv: [hb] 槽%d kind=%d ex=%d hitbox=(%.0f,%.0f) hurtbox=(%.0f,%.0f)",
                               z, (int)s_spirit_kind[z], (int)s_spirit_ex[z],
                               (double)hb2[0], (double)hb2[1], (double)hu2[0], (double)hu2[1]);
                    }
                    if (!(s_card_frames % 60))   /* ★ 每 60 帧打一次：判幽灵图号是否正确 ✓ */
                        ce_log("pofv: [spr] 槽%d kind=%d level=%d dis=%d frame=%d â sprite=%d",
                               z, (int)s_spirit_kind[z], (int)s_spirit_level[z], (int)s_spirit_dis[z],
                               (int)(s_spirit_age[z] / 6),
                               pofv_spirit_body_sprite(s_spirit_kind[z], s_spirit_level[z],
                                                       (int)s_spirit_dis[z], (int)(s_spirit_age[z] / 6)));
                    /* ★★★ 2026-10-06 owner：「**它是碰到边上之后折返**」✓ ⇒ 水獭的**左右折返**由卡侧翻 `$I3`：
                     *   ECL 的獭支是个每帧重读 `$I3` 的 loop（`spirit109.ecl.txt` ✓）⇒
                     *   **碰左沿 ⇒ `$I3 = 0`（改向右斜 22.5°）** / **碰右沿 ⇒ `$I3 = 1`（改向左斜 157.5°）** ✓。
                     *   ❓ 折返 x = ±180（自定；一手边界未核 ⇒ 已登记 TUNING ✓），场地半宽按 192 ✓。*/
                    if (s_spirit_kind[z] == POFV_SPIRIT_KIND_OTTER) {
                        const float ox2 = *(const float *)(e + POFV_ENEMY_ABS_POS);
                        /* ★★★ owner 2026-10-06 实跑：「**水獭在后撤步，然后没有折返**」✗ ⇒ 根因：
                         *   引擎的**灵控制器把位置钳在 ±140**（核实方一手：`FUN_0051b6c0`，常量 `0x5aa52c=-140` / `0x5aa3ac=+140` ✓）
                         *   ⇒ 灵**永远到不了**场地半宽 192 ✗ ⇒ 我上一轮用 `POFV_FIELD_HALF_W(192)` 当折返阈值 ⇒ **永不触发** ✗
                         *     （表现就是「贴着看不见的墙滑动、不折返」✓）。
                         *   ⇒ 改为**钳位处**触发：阈值取 **±140**（并在注释里留 ❓：一手钳位精确值/余量待核 ✓）。*/
                        if (ox2 >= POFV_SPIRIT_X_CLAMP)       *(int32_t *)(e + CE_ENEMY_ECL_I0 + 12) = 1;  /* 到右钳位 ⇒ 折向左 ✓ */
                        else if (ox2 <= -POFV_SPIRIT_X_CLAMP) *(int32_t *)(e + CE_ENEMY_ECL_I0 + 12) = 0;  /* 到左钳位 ⇒ 折向右 ✓ */
                    }
                    if (s_spirit_ex[z]) {
                        ce_anm_set_color((uint32_t)s_spirit_body[z], POFV_EX_ALPHA_RGBA);   /* 一手 ⑤ 50% 起步 */
                        /* ★ 一手 ①「**旋转 180°**」（本地同一 EX 样式转过来）⇒ 绕 Z 转 π。
                         *   SDK 有 `ce_anm_set_rotation(id, rx, ry, rz)` ✓（⚠ 脚本里不能同时跑 rotate 插值 ——
                         *   我们的本体脚本 400/401 没跑插值 ⇒ 安全 ✓）。 */
                        ce_anm_set_rotation((uint32_t)s_spirit_body[z], 0.0f, 0.0f, 3.1415927f);
                    }
                    /* ★★★ owner 2026-10-06：「**后撤步还有一个意思是运动方向和动画方向相反**」✗
                     *   ⇒ 水獭要按**移动方向**翻面（一手侧是两张图 `GhostOtterR`/`GhostOtterL` ✓；本卡只有一张 ⇒ 用负 x 缩放当翻转 ✓）。
                     *   方向来源 = 卡侧写进 `$I3` 的「出生半场、朝内」+ 边缘折返（`pofv.c` 的 OTTER 分支 ✓）：
                     *   `$I3 == 0` ⇒ **向右**（+22.5°）⇒ 不翻 ✓；`$I3 == 1` ⇒ **向左**（+157.5°）⇒ 翻 ✓。
                     *   ⚠ 本体脚本 401 开头 8 帧有 `scaleTime`（会覆盖 scale ✓）⇒ 那 8 帧后本写入稳定 ✓（每帧都写 ✓）。*/
                    if (s_spirit_kind[z] == POFV_SPIRIT_KIND_OTTER) {
                        const int32_t lr = *(const int32_t *)(e + CE_ENEMY_ECL_I0 + 12);
                        ce_anm_set_scale((uint32_t)s_spirit_body[z], (lr == 0) ? 1.0f : -1.0f, 1.0f);
                    }
                    /* ★★★★ 2026-10-08 owner 实跑 + 调研（一手 ✓）：**固化灵临终闪红** ——
                     *   一手 `enemy.anm` script23（固化瞬间 `flags|=0x1000` ⇒ main script := 22+level = 23/24/25 ✓）：
                     *     `sprite = (age/2) % 8` ✓（**2 帧一张** —— 原卡写的是 `/4` ✗，仅对固化态改 ✓）；
                     *     `color`: 0–112 **不染色** ✓；112–128 **红/白每 2 帧**（预警 ✓）；≥128 **红/白每 1 帧**（全速 ✓）。
                     *   删于 `enemy+0x5424` Timer > **160/200** ⇒ 一手全速闪 = 最后 **73** 帧（200−128+1 ✓）。
                     *   ⚠ 本卡固化窗口 `POFV_DISABLE_JOUMON_FRAMES = 500` **另有一手依据**（TH19 沈静化 100+200+200 ✓）
                     *     ⇒ **窗口不动** ✓，把一手的**末段闪红按比例挪到我们窗口末尾** ✓：
                     *     全速闪 = 最后 **73** 帧、其前 **16** 帧为 2 帧一档的预警 ✓（数字都取自一手 ✓）。
                     *   档位：`s_spirit_dis_age[z]`（固化后帧数 ✓，已有 ✓ ⇒ 不新增 VM/计时器 ✓）。
                     *   ❓与 EX 分支的 `color(0xFFFFFF80)` 优先级：**固化优先**（本块后写 ✓）。*/
                    if (s_spirit_kind[z] == POFV_SPIRIT_KIND_GHOST_TH09 && s_spirit_dis[z]) {
                        const int32_t age = s_spirit_dis_age[z];
                        const int32_t fast_from = POFV_DISABLE_JOUMON_FRAMES - 73;
                        const int32_t warn_from = fast_from - 16;
                        uint32_t tint = 0xFFFFFFFFu;                     /* 白 ✓ */
                        if (age >= fast_from)        tint = (age & 1) ? 0xFFFFFFFFu : 0xFFFF0000u;   /* 全速：每 1 帧 ✓ */
                        else if (age >= warn_from)   tint = (((age - warn_from) / 2) & 1) ? 0xFFFFFFFFu : 0xFFFF0000u;  /* 预警：每 2 帧 ✓ */
                        if (s_spirit_body[z]) {
                            ce_anm_set_sprite(CE_ABILITY_ANM(), (uint32_t)s_spirit_body[z],
                                              pofv_spirit_body_sprite(s_spirit_kind[z], s_spirit_level[z], 1,
                                                                      age / 2));    /* ★ 一手：2 帧一张 ✓ */
                            ce_anm_set_color((uint32_t)s_spirit_body[z], tint);
                        }
                    } else
                    ce_anm_set_sprite(CE_ABILITY_ANM(), (uint32_t)s_spirit_body[z],
                                      pofv_spirit_body_sprite(s_spirit_kind[z], s_spirit_level[z], s_spirit_dis[z],
                                      s_spirit_age[z] / ((s_spirit_kind[z] == POFV_SPIRIT_KIND_WOLF ||
                                                          s_spirit_kind[z] == POFV_SPIRIT_KIND_OTTER ||
                                                          s_spirit_kind[z] == POFV_SPIRIT_KIND_EAGLE) ? 6 : 4)));
                    /* XXXX owner: rotation not effective on this VM -> use NEGATIVE X SCALE flip (proven by otter flip) */
                    if (s_spirit_body[z]) {
                        const int32_t kF = s_spirit_kind[z];
                        float sxF = 1.0f;
                        if (kF == (int)POFV_SPIRIT_KIND_EAGLE && s_spirit_dis[z]) sxF = -1.0f;   /* sealed eagle -> flip (CW-90 look) */
                        else if (kF == (int)POFV_SPIRIT_KIND_OTTER) sxF = (*(const int32_t *)(e + CE_ENEMY_ECL_I0 + 12) == 0) ? 1.0f : -1.0f;  /* otter keeps its flip */
                        ce_anm_set_scale((uint32_t)s_spirit_body[z], sxF, 1.0f);
                    }
                }
            }
            else {
                if (++s_spirit_miss[z] > POFV_SPIRIT_MISS_MAX) {
                    ce_log("pofv: 灵 id=%u 连续 %d 帧查不到 ⇒ 收圆 vm=%d",
                           (unsigned)s_spirit_id[z], (int)s_spirit_miss[z], (int)s_spirit_vm[z]);
                    ce_anm_interrupt((uint32_t)s_spirit_vm[z], 1);
                    if (s_spirit_body[z]) { ce_anm_interrupt((uint32_t)s_spirit_body[z], 1); s_spirit_body[z] = 0; }
                    s_spirit_vm[z] = 0; s_spirit_miss[z] = 0;
                }
            }
        }
    }
    /* ★ 对面侧模型：每帧走一格（一手运动 + age++；sealed 时缓慢上浮）—— **纯数据** ✓ */
    pofv_foe_spirit_tick(s_foe, POFV_FOE_SPIRIT_MAX);
    /* ★ §4.5 周期生成（30 s，两侧各试一只；种类各自随机 ✓）—— 本侧这只就是**首个生成入口** ✓ */
    /* ★★★ owner 2026-10-05 新需求：**进道中 / 关底 boss 时的「全体清屏」** ——
     *   ① 被清掉的灵**不算被击破** ✗（不加累计器、不计 hits、不给分、不触发镜像击破 / 自然击杀 ✓）；
     *   ② **对称槽位（对面模型）也要清空** ✓（连在途的到达队列一起 ✓）。
     *   触发 = 引擎 boss 表 **0→1 的边沿** ✓（`engine.h:149 CE_EM_BOSS_IDS` ✓，与 §4.5 同一判据 ✓）。*/
    {
        uint8_t *emb = CE_ENEMY_MGR();
        int bnow = 0, bi2;
        if (emb) {
            const int32_t *bids2 = (const int32_t *)(emb + CE_EM_BOSS_IDS);
            for (bi2 = 0; bi2 < CE_EM_BOSS_SLOTS; ++bi2) if (bids2[bi2]) { bnow = 1; break; }
        }
        if (bnow && !s_boss_prev) {
            int z3;
            for (z3 = 0; z3 < POFV_SPIRIT_MAX; ++z3) {        /* ① 本侧：**只收 VM、清账** ⇒ 无击破记账 ✓ */
                if (s_spirit_vm[z3])   { ce_anm_delete((uint32_t)s_spirit_vm[z3]);   s_spirit_vm[z3] = 0; }
                if (s_spirit_body[z3]) { ce_anm_delete((uint32_t)s_spirit_body[z3]); s_spirit_body[z3] = 0; }
                s_spirit_id[z3] = 0; s_spirit_kind[z3] = -1; s_spirit_level[z3] = 0;
                s_spirit_from_incoming[z3] = 0; s_spirit_dis[z3] = 0; s_spirit_dis_age[z3] = 0;
                s_spirit_ex[z3] = 0; s_spirit_age[z3] = 0; s_spirit_lr[z3] = 0; s_spirit_miss[z3] = 0;
            }
            s_spirit_next = 0;
            s_spirit_q_n = 0;                                 /* 在途待生成一并清 ✓ */
            pofv_foe_spirit_init(s_foe, POFV_FOE_SPIRIT_MAX);     /* ② 对称槽位：对面模型全清 ✓ */
            pofv_foe_incoming_init(s_inc, POFV_FOE_INCOMING_MAX); /* ② 连在途到达队列 ✓ */
            ce_log("pofv: **进 boss ⇒ 全体清屏** ⇒ 本侧灵只收 VM、**不计击破** ✗；对面模型 + 在途队列一并清空 ✓");
        }
        s_boss_prev = bnow;
    }
    if (++s_period_timer >= POFV_PERIOD_SPAWN_FRAMES) {
    /* ★ MODEL §4.5：「**除 boss 外**，每 30 s 两侧各试生成 1 只」⇒ 用**引擎自己的 boss 判据** ✓
     *   （一手：`engine.h:149 CE_EM_BOSS_IDS 0x48` = 管理器的 boss enemy_id 表；`engine.h:659
     *    CE_ENEMY_OWN_BOSS_ID 0x636c` = `zEnemyData.own_boss_id`（非 0 = 关底/道中 boss））*/
    {
        uint8_t *em0 = CE_ENEMY_MGR();
        int boss_on = 0, bi;
        if (em0) {
            const int32_t *bids = (const int32_t *)(em0 + CE_EM_BOSS_IDS);
            for (bi = 0; bi < CE_EM_BOSS_SLOTS; ++bi) if (bids[bi]) { boss_on = 1; break; }
        }
        if (boss_on) { s_period_timer = 0; goto pofv_period_skip; }   /* boss 在场 ⇒ 本周期不生成 ✓ */
    }
        int32_t i4;
        s_period_timer = 0;
        for (i4 = 0; i4 < 2; ++i4) {                       /* 0 = 本侧 ✓ / 1 = 对面模型 ✓ */
            int32_t kd = POFV_SPIRIT_KIND_PURE;   /* ★ 一手（子智能体 `f27b135c` ✓）：**level 0 不产幽灵** ✗
                                                   *   ⇒ §4.5 周期生成**不该出 TH09 幽灵** ✓（幽灵只能来自
                                                   *     seed 路 ⇒ 绿 ✓ / 击破连锁 ⇒ 黄·红 ✓）。
                                                   *   ❓ §4.5 原本的种类分布（是否轮换 TH19 四种）未核 ⇒
                                                   *     本卡先前只随机 純霊/幽灵 ⇒ 现改为**固定純霊** ✓，分布待一手。*/
            float px = ((float)(ce_rand() % 321) - 160.0f);
            float py = 16.0f + (float)(ce_rand() % 33);
            if (i4 == 0) {                                  /* 本侧：走既有生成口 ✓ */
                if (s_spirit_q_n < POFV_SPIRIT_Q_MAX &&
                    pofv_spirit_room(pofv_spirit_live_count_fam((kd == POFV_SPIRIT_KIND_GHOST_TH09) ? POFV_FAM_TH09 : POFV_FAM_TH19),
                                     pofv_spirit_cap_for((kd == POFV_SPIRIT_KIND_GHOST_TH09) ? POFV_FAM_TH09 : POFV_FAM_TH19,
                                                         s_card_frames / 60))) {
                    s_spirit_q[s_spirit_q_n][0] = px; s_spirit_q[s_spirit_q_n][1] = py;
                    s_spirit_q_kind[s_spirit_q_n] = kd; s_spirit_q_level[s_spirit_q_n] = 1;
                    s_spirit_q_inc[s_spirit_q_n] = 0;   /* ★ Q4：§4.5 本侧 ⇒ 非 incoming ✓ */
                    ++s_spirit_q_n;
                    ce_log("pofv: §4.5 周期生成 ⇒ **本侧** kind %d（30 s 周期 ✓）", (int)kd); ++s_cnt_gift;   /* ★ goal ②：§4.5 白给计数 ✓ */
                                } else ce_log("pofv: §4.5 本侧生成口满 ⇒ 跳过（核实裁定 Q2：不折算 ✓）");
            } else {                                        /* 对面模型 ✓ */
                pofv_foe_spawn_gated(pofv_top_x(), pofv_top_y(), kd, 1);
                ce_log("pofv: §4.5 周期生成 ⇒ **对面模型** kind %d（30 s 周期 ✓）", (int)kd); ++s_cnt_gift;   /* ★ goal ②：§4.5 白给计数 ✓ */
            }
        }
    }
    pofv_period_skip:;
    /* ★ MODEL §4.6（判据 `:27` + `:147`）：**每 30 帧一次判定**，命中概率 `p = 1/120`
     *   ⇒ 平均 **60 s 一只** ✓；**击杀对象 = 对面槽位里「纯粹随机一只」** ✓（原来「每只各 roll」✗
     *   ⇒ 一帧可杀 0..N 只、总速率随在场数漂移 ✗）。
     *   ⇒ 记账按【镜像击破】✓：移出模型 + 幽灵按 `pofv_mirror_level_gain()` 升级 + 经延迟回程落回本侧 ✓；
     *     **level 3 永不返还**（§6-1）⇒ 候选里排除 ✓；无候选 ⇒ 不采样（这不是「送」⇒ 无折算义务 ✓）。*/
    /* ★★★ MODEL §4.6（**owner 2026-10-05 裁定**）：**每一个**对面灵**每 30 帧各自独立判定一次**，
     *   单只命中概率 `p = 1/120` ⇒ **平均一个灵 60 s 被击杀一次** ✓（**不是**「全槽一次判定 + 随机挑一只」✗
     *   —— 那是我上一版的误读，已按 owner 口径改回 ✓）。
     *   记账仍按【镜像击破】✓：移出模型 + 幽灵 `pofv_mirror_level_gain()` 升级 + 经到达口延迟回程 ✓；
     *   **level 3 永不返还**（§6-1）⇒ 逐只跳过 ✓。*/
    if ((s_card_frames % 300) == 0) {        /* ★ goal ②：四类计数 + 两侧在场数 一行汇总 ✓ */
        {   int i9, used9 = 0, dis9 = 0;   /* ★ 2026-10-06：把「账本占用 / 固化数」也打出来 ✓
                                                   *   ⇒ owner 报的「黄糊 + 旋转乱」若只在账本接近满时出现 ✓，这里一眼能对上 ✓
                                                   *     （我已核实：生成口闸门与记账紧邻 ⇒ `note_spirit` 的 −1 在单线程下**不可达** ✗，
                                                   *      对侧 `pofv_foe_spirit_spawn` 的返回也有 `>= 0` 防护 ✓ ⇒ 越界说**未证实** ✗）。*/
        for (i9 = 0; i9 < POFV_SPIRIT_MAX; ++i9) if (s_spirit_id[i9]) { ++used9; if (s_spirit_dis[i9]) ++dis9; }
        /* ★★★ 2026-10-06 owner：「版顶有一只**不动的狼灵**（贴图朝右、无动作）」✗
         *   ⇒ 这行是**户口对账**：把「**有 VM 却没户口**」（id=0 但 vm/body 非 0 ⇒ 残留 ✗，正是那只狼灵的样子 ✓）
         *     与「有户口却没 VM ✗」两类**逐个槽**打出来 ✓ ⇒ 我自己读日志即可定死成因 ✓（不再猜 ✗）。*/
        for (i9 = 0; i9 < POFV_SPIRIT_MAX; ++i9) {
            if (!s_spirit_id[i9] && (s_spirit_vm[i9] || s_spirit_body[i9]))
                ce_log("pofv: [对账] **残留** 槽%d id=0 但 vm=%d body=%d kind=%d（⇒ 孤儿 VM ✓ 铁律漏收 ✗）",
                       i9, (int)s_spirit_vm[i9], (int)s_spirit_body[i9], (int)s_spirit_kind[i9]);
            else if (s_spirit_id[i9] && !s_spirit_body[i9])
                ce_log("pofv: [对账] **缺 VM** 槽%d id=%u kind=%d（⇒ 本体没挂上 ✗）",
                       i9, (unsigned)s_spirit_id[i9], (int)s_spirit_kind[i9]);
        }
        ce_log("pofv: [计数] 击破=%d 送54=%d 送30=%d 回程=%d 白给=%d ｜ 本侧TH19=%d TH09=%d 对面模型=%d ｜ 账本=%d/%d 固化=%d ｜ EX表=%d/%d 送40=%d/%d",
               s_cnt_kill, s_cnt_send54, s_cnt_send30, s_cnt_arrive, s_cnt_gift,
               pofv_spirit_live_count_fam(POFV_FAM_TH19), pofv_spirit_live_count_fam(POFV_FAM_TH09),
               pofv_foe_spirit_count(s_foe, POFV_FOE_SPIRIT_MAX), used9, POFV_SPIRIT_MAX, dis9,
               (int)s_ex_gauge, POFV_EX_FIRE_AT, (int)s_send40, POFV_SEND40_LIMIT);
    }
    }
    if ((s_card_frames % POFV_NATKILL_EVERY) == 0) {
        int32_t i5;
        for (i5 = 0; i5 < POFV_FOE_SPIRIT_MAX; ++i5) {
            if (!s_foe[i5].active) continue;
            if (s_foe[i5].level >= 3) continue;              /* ★ §6-1：level 3 永不返还 ✓ */
            /* ★ owner 2026-10-06：「**只要被固化，这个灵一定不会再回来了**」✓ ⇒ 自然击杀也跳 `sealed` ✗ */
            if (s_foe[i5].sealed) continue;                 /* 固化中 ⇒ 走「活化/静止 ⇒ 离场」✓，不回来 ✓ */
            if ((ce_rand() % POFV_NATKILL_DEN) != 0) continue;   /* ★ 每只各自 p = 1/120 ✓ */
            {
                int32_t lv_nk = s_foe[i5].level;
                if (s_foe[i5].type == POFV_SPIRIT_KIND_GHOST_TH09) {
                    int32_t a4 = 0, a6 = 0;
                    pofv_mirror_level_gain(lv_nk, &a4, &a6);   /* 一手：升级后送回去 ✓ */
                    lv_nk = a6;
                }
                pofv_foe_incoming_push(s_inc, POFV_FOE_INCOMING_MAX, s_foe[i5].x, s_foe[i5].y,
                                       s_foe[i5].type, lv_nk,
                                       (int32_t)((ce_rand() % 101) + 60 + (ce_rand() % 101)));
                ce_log("pofv: §4.6 自然击杀（idx %d kind %d）⇒ 按镜像击破记账、延迟回程 ✓",
                       (int)i5, (int)s_foe[i5].type);
                s_foe[i5].active = 0; s_foe[i5].type = -1; s_foe[i5].sealed = 0;
            }
        }
    }
    /* ★★★ owner 2026-10-06：「**固灵结界——按 shift**」✓ ⇒ 按住时在自机处显示结界本体
     *   （可视 = 一手 `effect.anm` script1 的逐字复刻 `POFV109_AURA`(402) ✓；程序化多边形、零贴图 ✓）。
     *   松开 ⇒ 收掉 ✓；换卡/跨面 ⇒ `clear_card` 里 delete ✓（铁律 ✓）。*/
    /* ★★★ 2026-10-06 owner：「**shift 蓄力完全从属于固灵系统**」✓ ⇒ 按住 Shift 蓄力、结界半径=蓄力 ✓；
     *   松开 ⇒ 按一手脚本的 `scaleTime(20,4,0,0)` 收束 20 帧再删 ✓（`sdk.h` 无 interrupt 口 ✗ ⇒ 卡侧复现缩放 ✓）。*/
    s_aura_is_marisa = pofv_aura_marisa();                     /* 0 = 灵梦 / 1 = 魔理沙 ✓（一手字段 `0x4cccf4` ✓）*/
    s_aura_kind = pofv_aura_kind();
    s_aura_k = (s_aura_kind == 1) ? POFV_AURA_K_MARISA
             : (s_aura_kind == 3) ? POFV_AURA_K_SANAE
             : (s_aura_kind == 2) ? POFV_AURA_K_SAKUYA
                                 : POFV_AURA_K_REIMU;
    /* ★ ⑤-② 注记（owner 曾问"早苗没有自己的领域脚本"❓）⇒ **不是遗漏** ✓：
     *   一手（`06-aura.md` §9.3 ✓）：早苗那支（TH19 `pl02` script16）与灵梦 script15 **逐字相同** ✓
     *   ⇒ 本卡让早苗**复用 402（灵梦那支）就是一手行为** ✓，只是 K 不同（早苗 K=128 ⇒ 半径 = 其它自机的 2 倍 ✓）。
     *   她那支的**过渡**也不同（60 帧 / 模式 2 先慢后快 ✓，见下面 ease 分支 ✓）—— 所以"脚本共用、参数不同"是**有意的** ✓。*/
    if (s_aura_kind == 2) {                                    /* ★ 咲夜（TH09 ✓）状态推进 */
        const float *pp5 = ce_player_pos();
        if (pp5) {
            const float mx = pp5[0] - s_aura_px, my = pp5[1] - s_aura_py;
            s_aura_px = pp5[0]; s_aura_py = pp5[1];
            if (mx * mx + my * my > 0.01f) {                   /* 有移动 ⇒ 取最近 8 方向（只比点积，免 sqrt ✓）*/
                static const float d8[8][2] = { {1.0f,0.0f},{0.70710678f,0.70710678f},{0.0f,1.0f},{-0.70710678f,0.70710678f},
                                                {-1.0f,0.0f},{-0.70710678f,-0.70710678f},{0.0f,-1.0f},{0.70710678f,-0.70710678f} };
                int i, best = 0; float bs = -9.0e9f;
                for (i = 0; i < 8; ++i) { const float d = mx * d8[i][0] + my * d8[i][1]; if (d > bs) { bs = d; best = i; } }
                s_aura_dir_x += (d8[best][0] - s_aura_dir_x) * 0.25f;   /* ★ 一手：方向 0.25/帧 缓动 ✓ */
                s_aura_dir_y += (d8[best][1] - s_aura_dir_y) * 0.25f;
                s_aura_dir8i = best;      /* ★ 记下索引 ⇒ 下面写领域朝向 ✓ */
            }
            if (s_aura_sec_deg < 90.0f) {                      /* ★ 一手：**全张角**每帧 +3°（`0x4915e4` ✓）、上限 90°（`0x48e450` ✓）⇒ 半张角 ≤45° ✓ */
                s_aura_sec_deg += 3.0f;
                if (s_aura_sec_deg > 90.0f) s_aura_sec_deg = 90.0f;
            }
        }
    }
    /* ★★★★ 2026-10-10 **两把键的分工**（owner：「蓄力只认 C」「Shift 只开结界」✓）——本处逐字定类：
     *   · `chg_held` = `pofv_charge_key()` = **蓄力（C only）** ⇒ 只负责给 `s_aura_charge` 赋值；
     *   · `aura_held` = `pofv_aura_key()` = **结界（C ∨ Shift）** ⇒ 负责"结界张开/保持"这件事
     *     （半径块 + 结界 VM 的生成/保持全看它 ✓）。
     *   ⚠ 一手「**结界大小 = 蓄力值**」（owner 2026-10-06 口径 ✓）⇒ 半径仍取**蓄力值** ✓；
     *     Shift 单独按住且从未用 C 蓄力（`s_aura_charge == 0`）时 ⇒ 半径走**满蓄力**兜底
     *     （= 旧版 Shift 的观感，**不回退** ✓；该兜底标 **❓自定**，已登记 `TUNING.md` ✓）。*/
    {
    const int chg_held  = pofv_charge_key();   /* ★ 蓄力 = C only ✓ */
    const int aura_held = pofv_aura_key();     /* ★ 结界 = C ∨ Shift ✓ */
    /* ★ 蓄力音（一手 ✓）：★ 按下 C 的**上升沿**起一次 `0x2c` ✓（卡侧无循环接口 ✗ ⇒ 不循环 ✓）*/
    if (chg_held && !s_chg_se_prev) ce_play_sound(POFV_SE_CHARGE, ce_player_x());
    s_chg_se_prev = chg_held;
    if (chg_held) { s_c1_rel_latch = 0;   /* ★ 重新按住 ⇒ 下一次释放还要放一份 C1 ✓ */
        if (!s_aura_role_logged) {                             /* ★ 一次性：把角色全局的真实取值打出来 ✓ */
            s_aura_role_logged = 1;                            /*   （核实：`0x4cccf4` 确是角色下标 ✓，但「1=魔理沙」未逐值核 🟡
                                                                *    ⇒ 实跑各开一次灵梦/魔理沙，把这行日志对一下即可钉死 ✓）*/
            ce_log("pofv: 结界角色 [0x4cccf4]=%d（本卡判为 %s）",
                   (int)(*(const int32_t *)POFV_ADDR_PLAYER_CHAR),
                   pofv_aura_marisa() ? "魔理沙套(403/404/405 竖带)" : "灵梦套(402 圆)");
        }
        /* ★★★ 2026-10-06 owner：「**他妈的一手是什么就是什么**」✓ ⇒ **删掉我自造的蓄力累加** ✗：
         *   一手（`06-aura.md` §9.17 定案 ✓）那个「蓄力」**不是**随时间涨的条 ✗，而是**能力卡设的一个数值**
         *   （`S+0x8c`，两张阿吽卡把它置 **125** ✓，其余角色**无写点** ⇒ 恒定 ✓）；§10.3 的算例直接写
         *   「**满蓄力 ⇒ 128**」✓ = `200 × K / 100` ✓ ⇒ 所以**目标半径 = 2K**（灵梦 128 / 魔理沙 28 / 早苗 256 ✓），
         *   而**可见的长大全部来自张开动画**（槽 3 每帧写缩放 ✓，灵梦/魔理沙 20 帧、早苗 60、咲夜 90 ✓）。
         *   ⇒ 本卡照此：按住 Shift ⇒ 目标取**满值** ✓（不再累加 ✗），开合全交给下面的追赶层 ✓。*/
        /* ★★★ owner 2026-10-06 实跑：「**灵梦的固灵结界没这么大，得是现在的一半大**」✓
         *   ⇒ 取下限的一半：一手公式是 `clamp(蓄力,0,200) × K / 100` ✓，我原先取满值 **200** ⇒ 灵梦 **128** ✗；
         *     改成 **100** ⇒ 灵梦 **64** ✓（正好一半 ✓），且四名自机**等比**：魔理沙 **14** / 咲夜（TH09 另算 ✓）/ 早苗 **128** ✓。
         *   ★ owner 2026-10-06 给全的表（`目标 = clamp(电荷,0,200) × K / 100` ✓）：
         *     电荷 **200**（clamp 上限，一手理论最大）⇒ 灵梦 128 / 魔理沙 28 / 早苗 256 —— **我先前误取它** ✗（就是你看到的「大一倍」✓）；
         *     电荷 **125**（一手**唯一实际写入值**，Aun 两张卡 ✓）⇒ 灵梦 **80** / 魔理沙 **17.5** / 早苗 **160** ✓；
         *     电荷 **100**（**owner 实跑裁定** ✓）⇒ 灵梦 **64** / 魔理沙 **14** / 早苗 **128** —— **现源码取这个** ✓。*/
        /* ★★★★ 2026-10-10 owner：**蓄力被上限量（灵力 `s_reiryoku`）封顶** ✓
         *   （一手：`charge < clamp(S+0x24)` 才涨 ⇒ `S+0x24`（= 灵力）就是当前蓄力 `AuraCtrlInf+0x94` 的上限 ✓）。
         *   ⇒ `s_aura_charge = min(满值, 灵力)`；灵力初值 = C1 > 0 ⇒ **永远不会开局为 0** ✓。*/
        /* ★★★★ 2026-10-10 **修量纲 bug**（owner 实感「被封到 50 / 半径只有一半」✓）：
         *   原式把 **灵力（0..2500 ✓）** 直接和 **蓄力上限（100 ✗）** 比较 ⇒ 开局 C1=50 就把蓄力封到 50 ✗。
         *   ★ 正确关系（owner 一手口径）：「**蓄力最大值 = 灵力**」✓ ⇒ 两者必须**同量纲换算**：
         *     蓄力满值(100) × (灵力 / 灵力满值 2500) ✓
         *   ⇒ 灵力 = 2500（满）时蓄力上限 = **100**（= 改前手感 ✓）；灵力 = C1 时按比例 ✓。
         *   ⚠ `POFV_REI_C1` 的取值直接决定**开局能蓄到多少** ⇒ 它是 ❓自定（一手是 clamp([sht]×10,0,2500)，
         *     逐角色；**很可能是 2500 = 满**）⇒ 见 TUNING 的待定条目 ✓。*/
        {
            /* ★★ 一手：`cap = 灵力` **本身** ✓（`S+0x24`，clamp 到 `[0,2500]` ✓）——不再 ÷25 ✗。*/
            float cap = s_reiryoku;
            if (cap < 0.0f) cap = 0.0f;
            if (cap > POFV_REI_MAX) cap = POFV_REI_MAX;
            if (cap < 0.0f) cap = 0.0f;
            /* ★★ 2026-10-10「逐帧涨」（= 一手 `+0x94/+0x98` 累加器 ✓，涨率见 `POFV_AURA_CHARGE_RATE` ✓）
             *   ⇒ 蓄力条/结界半径**平滑生长** ✓（owner：「按 C 蓄力的条才从 0 开始长」✓）；
             *     被上限（灵力换算出的 `cap`）封顶 ✓；松手归零（下面的 else 分支 ✓）。*/
            {   /* ★★ 一手形 ✓：`rate = clamp(源, 0, 源上限) × (满速 / 源上限)`（源 = 当前灵力 ✓）
             *   ❓`POFV_CHG_RATE_AT_SRC_MAX` = owner 待调的手感旋钮（现 10/帧 = 一手 20 的一半 ✓）。*/
                float rate = s_reiryoku;
                if (rate < 0.0f) rate = 0.0f;
                if (rate > POFV_CHG_RATE_SRC_MAX) rate = POFV_CHG_RATE_SRC_MAX;
                s_aura_charge += rate * (POFV_CHG_RATE_AT_SRC_MAX / POFV_CHG_RATE_SRC_MAX);
            }
            if (s_aura_charge > cap) s_aura_charge = cap;
        }
    } else if (aura_held) {
        /* ★★ 2026-10-10：门**去掉 `&& s_aura_charge > 0.0f`** ✗ —— 否则「按 Shift 但没蓄过力」
         *   （`s_aura_charge == 0`）会**掉进第三支** ⇒ 按住期间**每帧拆自己的领域** ✗✗。
         *   ⇒ 现在「**只要结界键还按着 ⇒ 一律不收束**」✓（= owner「按 shift 只管张开结界」✓）。
         *   ⚠ 下面第三支因此**只在键真的松开时**才可达 ✓。*/
        /* ★ 2026-10-10 新增（**结界键还在按**）：Shift 仍按着 ⇒ **保持**当前蓄力值、**不收束**
         *   —— 这是 owner 早前「**两个 key 可以接续**」（按住 C 再按 Shift ⇒ 续同一个蓄力 ✓）的落地：
         *   本体/收束都别动，下面的结界块继续按 `s_aura_charge` 维持同一台 VM ✓。
         *   （⚠ 蓄力值只在 **C** 那支里被赋值 ⇒ Shift 不会让蓄力"增长" ✓ = owner「按 shift 不是蓄力」✓。）*/
        (void)0;
    } else if (s_aura_charge > 0.0f || s_aura_vm || s_aura_kind == 2) {
        /* ★★ 一手 ✓：「每次释放放一份 C1」（`0x4e843b call [esi+0xc]` ✓，全档位共用 ✓）——
         *   ★ 只在**松手那一帧**放一次 ✓（闩锁 ✓，避免这支多帧重复放 ✗）。
         *   ★ 早苗例外：★ 她 C2 帧里槽 3 被 `+0xdc` 守卫吃掉 ✓ ⇒ 这里同样跳过 ✓。*/
        if (!s_c1_rel_latch && s_aura_charge > 0.0f && !(s_aura_kind == 3 && s_aura_charge >= POFV_C2_THRESH
                                                       && s_aura_charge < POFV_C3_THRESH)) {
            /* ★★ 2026-10-11 照一手"同帧"订正 ✓：★ C1 的发射**挪到档位发射之后** ✓ ——
             *   一手是"这份 C1 与 C2 在**同一帧**" ✓（`EX+0xd4`: `0x417c49` 置 1、`0x53cbe0` 消耗 ✓）；
             *   ★ 原来 C1 在档位**之前**发 ✗ ⇒ 魔理沙那两发追加落在**下一份** C1 上 ✗。*/
            /* (C1 的发射见下面档位发射之后 ✓) */
        }
        /* 2026-10-11 owner：「依旧有奇怪的提前释放」=> 根因是我把这个块放在了一个
         * 蓄力途中就成立的支里（条件含「领域 VM 还在」）。改成真正的松手沿：
         * 上一帧按着、这一帧没按，才放。按松手那一刻的蓄力定档，只发最高那一档。*/
        if (s_chg_prev && !chg_held) {
            const int tier_rel = (s_aura_charge >= POFV_C4_THRESH) ? 4
                               : (s_aura_charge >= POFV_C3_THRESH) ? 3
                               : (s_aura_charge >= POFV_C2_THRESH) ? 2 : 1;
            if (tier_rel == 2) pofv_c2_fire();
            else if (tier_rel == 3) pofv_c3_fire();
            else if (tier_rel >= 4) pofv_c4_fire();
            /* ★★ 一手 ✓ 同帧：★ 档位发完**紧接着**发这份 C1 ✓
             *   （⇒ 魔理沙 C2 设的 `EX+0xd4` 由**这一份** C1 消耗 ✓，不再拖到下一份 ✓）。*/
            if (!s_c1_rel_latch && s_aura_charge > 0.0f
                && !(s_aura_kind == 3 && s_aura_charge >= POFV_C2_THRESH
                     && s_aura_charge < POFV_C3_THRESH))
                pofv_c1_fire();
            /* 2026-10-11：等级/计数/扣灵力全部跟着"真正放出去的那一档"走（不再在蓄力途中动）。
             * 扣的量 = 该档阈值 - C1 阈值：C2 500 / C3 1200 / C4 2000（owner 问的那个数）。*/
            if (tier_rel >= 2) {
                const float cost = (tier_rel == 2) ? (POFV_C2_THRESH - POFV_C1_THRESH)
                                 : (tier_rel == 3) ? (POFV_C3_THRESH - POFV_C1_THRESH)
                                                   : (POFV_C4_THRESH - POFV_C1_THRESH);
                s_reiryoku -= cost;
                if (s_reiryoku < 0.0f) s_reiryoku = 0.0f;
                if (tier_rel == 2) { if (s_c3_lv_n < 0) s_c3_lv_n = 0; }
                if (tier_rel >= 3 && s_c3_lv_n < 7) ++s_c3_lv_n;
                if (tier_rel >= 4 && s_c4_lv_n < 7) ++s_c4_lv_n;
                if (tier_rel >= 3 && s_c3_uses < 999) ++s_c3_uses;
                if (tier_rel >= 4 && s_c4_uses < 999) ++s_c4_uses;
            }
        }
        s_chg_prev = chg_held;   /* 记录本帧状态，下一帧判沿 */
        s_c1_rel_latch = 1;
        /* ★★ 2026-10-10 owner 实跑回归：「我领域**不再有消失过程**」✗ —— 根因 = **这一支的门** ✗：
         *   原来门 = `s_aura_charge > 0.0f` ✓，而 ★ `s_aura_charge` **只在按 C 时增长** ✓
         *   （见上面 `chg_held` 那支 ✓），★ 且被 `cap = 100 × 灵力/2500` **封顶** ✗
         *   ⇒ ★ **灵力低（cap 小）** 或 **只按 Shift、从没按过 C** ⇒ `s_aura_charge == 0` ✗
         *   ⇒ ★ **整段收束都不跑** ✗（含 `ce_anm_interrupt(aura_vm, 1)` + 咲夜的 `s_aura_close` ✓）
         *   ⇒ ★ 领域**没有那 20 帧的收束/淡出** ✓ = owner 报的症状 ✓。
         *   （★ 我本轮把灵力初值 2500 → 500 ⇒ cap 100 → 20 ⇒ 更容易撞上这条 ✗ —— 是我**暴露**了它 ✓，
         *     也说明这个门**本来就不该挂在蓄力上** ✗：owner 的语义是「**结界键一松就该有收束**」✓。）
         * ⇒ ★ 门改成「**有领域要收**」✓：蓄力 > 0 ✓ **或** 领域 VM 还在 ✓ **或** 咲夜的分片态 ✓
         *   —— ★ 与"按 C 蓄力"**解耦** ✓（不动上面那两支的键语义 ✓）。*/
        /* ★★★ 2026-10-06 核实订正：`ce_anm_interrupt` **存在** ✓（`sdk.h:391`，本卡 `:550/:1053` 已在用 ✓）
         *   ⇒ 收束**完全交回脚本**：`interrupt(1)` ⇒ 脚本自己的
         *     `stop(); interruptLabel(1); scaleTime(20, 4, 0.0f, 0.0f); +20: delete();` ✓
         *     （一手 `pl00.txt:464-479` ✓：**ease-out**（模式 4）+ 20 帧后**脚本自删** ✓）
         *   ⇒ 卡侧**不再手写缩放**（否则与脚本 `scaleTime` 打架 ✗）、也**不代删**（交脚本 ✓）。*/
        /* ★★★ owner 实测：「没按 Shift 时**依然有一条细缝**」✗ ⇒ 根因：`interrupt` 后**两条子带没有缩放动画** ✗
         *   ⇒ 要等各自 `+20: delete()` 才消失 ⇒ 那 20 帧就是细缝 ✓ ⇒ 改成**松手即删** ✓（不给残留窗口 ✓，
         *   与本仓「长持续必须收干净」铁律一致 ✓；❓ 因此丢掉一手的 20 帧收束动画，已登记 ✓）。*/
        /* ★★★ 验收定位（2026-10-06）：**「松手瞬删」就是这一行** ✗（验收 ②：`:1379` ⇒ 立即 delete）
         *   ⇒ 改成 `interrupt` ✓：脚本自己 `interruptLabel(1) ⇒ scaleTime(20,4,0,0) ⇒ +20: delete()` ✓（`pl00.txt:464-479` ✓）
         *     ⇒ 拿回一手 **20 帧收束** ✓；句柄随即清 0（交脚本自删 ✓）；`R=0` ⇒ 卡侧不再写缩放 ✓。
         *   ⚠ 遗留 ❓：魔理沙两条子带（`404/405`）一手只有 `+20: delete()`、**无缩放动画** ⇒ 那 20 帧仍有"细缝"感 ✓
         *     ⇒ 要彻底无缝得给子带加动画（**共享面** ⇒ 需 `AUDIT §BE` ✓）；先按 owner 已裁的取舍 ✓。*/
        if (s_aura_kind == 2) {
            int ip3; for (ip3 = 0; ip3 < POFV_AURA_PIECES; ++ip3)
                if (s_aura_piece[ip3]) { ce_anm_interrupt((uint32_t)s_aura_piece[ip3], 1); s_aura_piece[ip3] = 0; }
        }
        if (s_aura_vm && s_aura_kind == 2) {
            /* ★★ 复核定案（HANDOVER-AURA-SECTOR §18 ✓）：松手**不进 `charge>0` 分支** ⇒ 卡侧自然
             *   停写 ✓（= 其它三人为什么能收束的原因 ✓）；张角收缩由**下面新增的收束分支**每帧写 ✓；
             *   同时补**一手**的 10 帧 α 淡出（`etama` script91：`+38: alphaTime(10,0,0)` ⇒ `+10: delete` ✓）。
             *   ⇒ 单边拥有：**卡侧删 + 清句柄** ✓（不 interrupt ✗）。*/
            s_aura_close = 20;                               /* 张角收缩 20 帧（❓自定，借队友时长 ✓）*/
            {   float a0 = (float)s_aura_anim_f * 0.05235988f;
                if (a0 > 1.5707964f) a0 = 1.5707964f;
                s_aura_close_ang = a0; }
        } else if (s_aura_vm) { ce_anm_interrupt((uint32_t)s_aura_vm, 1); s_aura_vm = 0; }
        if (0) {
            /* ★★ 照其它三人的设计 ✓：卡侧**发 interrupt**（触发脚本 `406` 的 `rotateTime(20,…)` 收束 ✓）
             *   ＋ **立刻忘句柄** ✓（⇒ 下次激活会 spawn **新** VM ⇒ 治"VM 死那/不重生"✗）。
             *   ⚠ 先写一次 `rz = π/2` ✓ ⇒ 脚本收束时 rz 目标与当前相同 ⇒ **不再乱转半圈** ✗→✓ */
            /* 咲夜：**不 interrupt ✗、不清句柄 ✗** ⇒ 由收束分支每帧写张角 ✓（单边拥有 ✓）*/
        }
        /* ★★ 咲夜也要**发 interrupt** ✓ —— 这正是其它三人的做法（`402/404/405`：interrupt ⇒ 脚本
         *   `scaleTime(20,4,0,0)` ⇒ 收束 ✓）⇒ 我们用它触发 `406` 的 `rotateTime(20,4,rx=0,…)` ✓
         *   （先前我让咲夜走 `s_aura_close` 分支 ⇒ **脚本的收束支永不执行** ✗ = 你看到的"瞬间消失"✓）。*/
        
        s_aura_anim_f = 0;                 /* ★ 下一段张开从初值 1.0 重新演 ✓ */
        if (s_aura_kind != 2) s_aura_vm = 0;   /* 咲夜留着收束 ✓ */
        /* 旧注：交脚本自删 ⇒ 立刻忘掉句柄（`clear_card` 便不会二次删 ✓）*/
        s_aura_charge = 0.0f; s_aura_r = 0.0f;
    }
    /* ★★★★ C2 的**触发**（`HANDOFF-109-C1-C4.md` §5）：沿用 C1 的**结界张开上升沿** ——
     *   `aura_held`（C ∨ Shift 的**电平**）在本块内可见 ⇒ 上升沿判定放这里（块尾 ✓）。
     *   · **同一次张开 ⇒ C1 与 C2 都放** ✓：C1 的 `pofv_c1_fire()` **尚未落地**（owner 另写）
     *     ⇒ 它落地时把调用加进下面这个 `if (aura_edge)` 即可（**共用同一个闩锁** `pofv_aura_rise_edge()` ✓，
     *     不要另起一个 `s_c1_prev` 的自有闩锁 ✗，否则删卡清闩锁会漏一处 ✓）。
     *   · C2 的 tick **每帧**跑（与结界键电平**无关** —— 一次张开 = 105 帧的 EX 对象 ✓）；
     *     触发帧先 `fire()` 再 `tick()` ⇒ 那一帧就推进 `age = 0`（与一手"释放当帧即生效"同形 ✓）。*/
    {
        /* ★★★★ 2026-10-10 owner 实跑报 bug 后**改成按蓄力档触发** ✓（= 一手口径 ✓）：
         *   报的现象：「按 Shift，松手后自机身上长出一个**很大的暴风圈**」✗ —— 根因 =
         *   旧版 C1/C2/罩子都挂在 **`pofv_aura_rise_edge()`（结界键 `C∨Shift` 的边沿）** ✗
         *   ⇒ 一动 Shift 就放了一轮，那个"大圈"正是 **C2 的扩张环 128→192** ✓。
         *   一手（TH19）：**C1/C2 按蓄力档放** —— C1 = `charge >= clamp(S+0x28)`、
         *   C2 = `FUN_0040b720() <= charge`；而 `S+0x28/0x2c`（灵梦）= **500 / 1000**，
         *   与蓄力**同量纲（0..2500）** ✓。
         *   本卡 `s_aura_charge` 是 **0..100**、上限 `cap = 100 × 灵力/2500` ⇒
         *   **取同一比例**：C1 = `cap × 500/2500`、C2 = `cap × 1000/2500` ✓（★ 灵梦的 500/1000 ✓）。
         *   ★ **每档只在上升时放一次** ✓（掉回阈下再涨才重放 ✓）。
         *   ★ ★ **Shift 从此不再触发任何攻击** ✓ —— 它只开结界 ✓（一手如此 ✓）。*/
        static int s_c1_lv, s_c2_lv;
        static int s_c3_lv;                     /* ★ ⑤ C3：档闩锁（与 C1/C2 同形，按蓄力档 ✓）*/
        static int s_c4_lv;                     /* ★ ⑤ C4：档闩锁 ✓ */
        static int s_barrier_lv;                /* ★ ④ 罩子：档闩锁（与角色无关 ✓）*/
        static int s_c2m_lv;                    /* ★ ② C2 魔理沙分支：档闩锁 ✓ */
        static int s_c3m_lv;                    /* ★ ⑤ C3 魔理沙分支：档闩锁 ✓ */
        static int s_c4m_lv;                    /* ★ ⑤ C4 魔理沙分支：档闩锁 ✓ */
        static int s_c2s_lv;                    /* ★ ② C2 早苗分支：档闩锁 ✓ */
        static int s_c3s_lv;                    /* ★ ⑤ C3 早苗分支：档闩锁 ✓ */
        static int s_c3s4_lv;                   /* ★ ⑤ C4 早苗分支：档闩锁 ✓ */
        /* ★★ 2026-10-10 owner 实跑连带订正：阈值此前写成"**cap 的百分比**" ✗ ——
         *   灵力 500（新初值）时 cap = 20 ⇒ **四档会在 1 秒内全放** ✗。★ 一手是**绝对**阈值 ✓：
         *   `S+0x28/0x2c/0x30/0x34/0x38` ×10 = **500 / 1000 / 1700 / 2500** ✓，
         *   判据 = `charge ≥ 阈值ᵢ` **且** `charge ≤ S+0x24`（= **灵力**，即本卡的 `cap` ✓）。
         *   ★ 本卡蓄力 0..100、灵力 0..2500 ⇒ **换算因子 = 2500/100 = 25** ✓：
         *     C1 = 500/25 = **20** ✓、C2 = **40** ✓、C3 = **68** ✓、C4 = **100**（= 条满 ✓）。
         *   ★ 与条上 4 枚标的置位（`POFV_HUD_CHG_TICK_FRAC` = 0.20/0.40/0.68/1.00 ✓，**绝对** ✓）
         *     以及条填充 `fc = 蓄力/100`（**绝对** ✓）**逐一对上** ✓。
         *   ⚠ 后果（一手如此 ✓）：灵力 500 时 cap = 20 ⇒ **只用得到 C1** ✓，
         *     要靠吸灵力道具涨灵力才解锁 C2/C3/C4 ✓。*/
        /* ★★ 量纲改一手（0..2500 ✓）⇒ 阈值就是**字面的** 500/1000/1700/2500 ✓（★ 不再 ÷25 ✗）。*/
        const float c1_at = POFV_C1_THRESH;
        const float c2_at = POFV_C2_THRESH;
        const float c3_at = POFV_C3_THRESH;
        const float c4_at = POFV_C4_THRESH;
        /* ★★ 2026-10-11 照一手订正（子 agent 一手核实 ✓，owner：「C3 C4 还会至少放一份 C1，这是通用机制吧」✓）：
         *   ★ 一手：★ C1 那一支 = **放炮块的最尾部、全档位共用** ✓
         *     （C2/C3/C4 三条路都汇到 `0x4e83b9`，随后顺序落到 `0x4e83e8`（clamp `S+0x28`）
         *      → `0x4e841c`（`charge >= 阈值`）→ `0x4e843b call [esi+0xc]` = **EX 槽 3** ✓）；
         *   ★ 而"放炮块"要求 **`aura+0x18 >= 1`**（= **释放计时器** ✓，由蓄力期间 `0x4e7cb2` 推进 ✓）
         *     ⇒ ★★ **C1 是"每次释放放一份"，不是在 500 上升沿放** ✗（旧的挂法错 ✗）。
         *   ★ 节奏 = **每次释放恰好一帧** ✓（放完写 0 + `set_value(-10)` ⇒ ≥10 帧冷却 ✓）。
         *   ★ 于是 C1 挪到**释放那一支**（见下 ✓），这里不再按阈值跨过触发 ✓。
         *   ★ 注：★ 早苗有一处例外 ✓ —— 她 C2 帧里槽 3 开头 `cmp [ecx+0xdc],0 ; jne ret` ⇒
         *     **被守卫吃掉** ✓（否则会把 `+0xd8` 从 11 覆写回 10 ✓）；C3/C4 帧照放 ✓。*/
        (void)c1_at;
        /* ★ 升档音（一手 ✓「**升档另有音**」，id `0x31` ✓）：★ 档位**新跨过**时补一次 ✓ */
        {   static int s_tier_prev;
            const int t_now = ((s_aura_charge >= c1_at) ? 1 : 0) + ((s_aura_charge >= c2_at) ? 1 : 0)
                            + ((s_aura_charge >= c3_at) ? 1 : 0) + ((s_aura_charge >= c4_at) ? 1 : 0);
            /* ★★ 2026-10-11 owner 订正：「那些可能是一族蓄力音，但是**用到的地方和什么 C1 C4 一点关系没有**」
             *   ⇒ 我先前把 `se_ch01/02/03`（`0x1f/0x2a/0x2b`）按 C2/C3/C4 **铺上去是自作主张**，已撤。
             *   ⇒ 现在**只放 `se_ch00`（`0x1e`）那一支蓄力音**（owner 确认的件）。
             *   ❓"一手升档另有音"这条（本仓早先记的）**音源未定** ⇒ 先不放，登记 TUNING 待查。*/
            (void)t_now;
            /* ★ LV 计数（一手 ✓ `clamp(+1, 0, 7)`）：★ 新跨到 3 档 ⇒ C3 的 LV +1 ✓；★ 4 档 ⇒ C4 ✓ */
            /* 2026-10-11 owner：蓄力途中灵力就掉了 => 这几项全部挪到松手那一下。*/
            /* 2026-10-11 owner：蓄力途中灵力就掉了 => 这几项全部挪到松手那一下。*/
            /* ★ 一手 ✓ 使用计数（`S+0x4c` / `S+0x50`，clamp 0..999 ✓）*/
            /* ★★★ 2026-10-11 owner：「**扣的是灵力**」✓ + 问「能放 C4 但放了个 C2 扣多少」
             *   ⇒ ★ 一手规则（子 agent 复述）：★ 阶段 1 **不耗** ✓；★ 阶段 2/3/4 各扣
             *     **`该档阈值 − 上限`** ✓ —— ★ 按 owner 的模型"上限" = **C1 阈值 500** ✓
             *     ⇒ ★ **C2 扣 500 / C3 扣 1200 / C4 扣 2000** ✓（★ 只与**你放的那一档**有关 ✓，
             *     与灵力攒了多少无关 ✓ —— 这正是他问的那点 ✓）。*/
            /* 2026-10-11 owner：蓄力途中灵力就掉了 => 这几项全部挪到松手那一下。*/
            /* 2026-10-11 owner：蓄力途中灵力就掉了 => 这几项全部挪到松手那一下。*/
            /* 2026-10-11 owner：蓄力途中灵力就掉了 => 这几项全部挪到松手那一下。*/
            /* 2026-10-11 owner：蓄力途中灵力就掉了 => 这几项全部挪到松手那一下。*/
            /* 2026-10-11 owner：蓄力途中灵力就掉了 => 这几项全部挪到松手那一下。*/
            /* 2026-10-11 owner：蓄力途中灵力就掉了 => 这几项全部挪到松手那一下。*/
            s_tier_prev = t_now;
        }
        /* ★★ 2026-10-10：一手**三人**的 C2 完全不同（灵梦 = `EXReigekiInf` ✓ / 魔理沙 = 6 条
         *   `EXLaserInf` ✓ / 早苗 = 不建对象、改走 shot 行 11 ✓，见 §5.2）⇒ ★ **本卡的 C2 只对灵梦** ✓，
         *   其余角色**先不放**（❓待补，见 TUNING）。★ 不这样做的话，魔理沙会放出**灵梦的**弹幕 ✗。*/
        if (s_aura_charge >= c2_at && s_aura_kind == 0) {
            /* 2026-10-11 owner：蓄力到 C2 自动放肯定不对 => 途中一律不发（C2）；改到释放支按松手时的档发。*/
        } else s_c2_lv = 0;
        /* ★ ② C2 —— **早苗**分支（一手 = shot 行 11 = 更慢的 26 发整圈 ✓，见上）*/
        if (s_aura_charge >= c2_at && s_aura_kind == 3) {
            if (!s_c2s_lv) { pofv_c2s_fire(); s_c2s_lv = 1; }
        } else s_c2s_lv = 0;
        /* ★ ② C2 —— **魔理沙**分支（一手 = 6 条 `EXLaserInf` ✓，见上）*/
        if (s_aura_charge >= c2_at && s_aura_kind == 1) {
            if (!s_c2m_lv) { pofv_c2m_fire(); s_c2m_lv = 1; }
        } else s_c2m_lv = 0;
        /* ★ ⑤ C3（ExtraAttack）：★ 按蓄力档（**不是**边沿 ✗ —— 旧 `pofv_aura_rise_edge()` 已被 owner 实跑否掉 ✓）*/
        /* ★ ⑤ C4：一手阈值 = 2500（= `pl00.sht +0x38` ×10 ✓，= 满 ✓）*/
        {
            /* ★ ⑤ C4 —— **魔理沙**分支（一手 = 一条激光 + 一串绕圈扫射弹 ✓，见上）*/
        if (s_aura_charge >= c4_at && s_aura_kind == 1) {
            if (!s_c4m_lv) { pofv_c4m_fire(); s_c4m_lv = 1; }
        } else s_c4m_lv = 0;
        /* ★ 同上：一手三人的 C4 也不同 ⇒ ★ **只对灵梦** ✓（❓待补）。*/
            if (s_aura_charge >= c4_at && s_aura_kind == 0) {
            /* 2026-10-11 owner：蓄力到 C2 自动放肯定不对 => 途中一律不发（C4）；改到释放支按松手时的档发。*/
            } else s_c4_lv = 0;
        }
        /* ★★ ④ 罩子：一手 `FUN_0040b420`（回罩）全 .text **只有两个调用点** = `0x4e8176`(阶段 3 EX) /
         *   `0x4e7b8d`(阶段 4 BOSS) ✓ —— ★ 而一手是「**发动 EX 或 BOSS 就复活自动パリア**」✓
         *   ⇒ ★ **与角色无关** ✓：本卡挂在 **C3 / C4 的阈值**上 ✓（★ 所以放在**角色门之外** ✓
         *   —— 魔理沙/早苗的 C3/C4 还没做，但"蓄到这一档就回罩"是通的 ✓）。*/
        if (s_aura_charge >= c3_at) {
            /* ★★ 触发点订正（owner #4 口径 ✓「**开 C3 C4 蓄罩子**」）：
             *   原先挂在**结界张开的上升沿** ✗（那行旧注释自己就写着"等 C3/C4 做好后改成按阈值" ✓）
             *   ⇒ 现在改按**蓄力到 C3 阈值**给盾 ✓（= 够得着 C3/C4 才能蓄罩 ✓，与一手一致 ✓）。*/
            if (s_aura_charge >= POFV_C3_THRESH) {
                if (!s_barrier_lv) { pofv_barrier_fire(); s_barrier_lv = 1; }
            } else s_barrier_lv = 0;
        } else s_barrier_lv = 0;

        /* ★ ⑤ C3/C4 —— **早苗**分支（一手 = 刷 ExAttack2 子敌人；她的 ExtraAttack00 不存在
         *   ⇒ ★ **C3 与 C4 是同一套** ✓，只是阈值不同 ✓）*/
        if (s_aura_charge >= c3_at && s_aura_kind == 3) {
            if (!s_c3s_lv) { pofv_c3s_fire(); s_c3s_lv = 1; }
        } else s_c3s_lv = 0;
        if (s_aura_charge >= c4_at && s_aura_kind == 3) {
            if (!s_c3s4_lv) { pofv_c3s_fire(); s_c3s4_lv = 1; }
        } else s_c3s4_lv = 0;
        /* ★ ⑤ C3 —— **魔理沙**分支（一手 = 纯激光、沿 y=448 横扫、每步双镜像 ✓，见上）*/
        if (s_aura_charge >= c3_at && s_aura_kind == 1) {
            if (!s_c3m_lv) { pofv_c3m_fire(); s_c3m_lv = 1; }
        } else s_c3m_lv = 0;
        /* ★ 同上：一手三人的 C3 也不同（§10.1）⇒ ★ **只对灵梦** ✓（❓待补）。*/
        if (s_aura_charge >= c3_at && s_aura_kind == 0) {
            if (!s_c3_lv) {
            /* 2026-10-11 owner：蓄力到 C2 自动放肯定不对 => 途中一律不发（C3）；改到释放支按松手时的档发。*/
            }
        } else s_c3_lv = 0;
        /* ★ ④ 罩子：一手是「**C3/C4 才给盾**」✗（C3/C4 尚未实现）⇒ ★ **本轮先不给盾** ✓
         *   （旧版挂在结界边沿 ✗ ⇒ 也是那个 Shift bug 的一部分 ✓ 已一并去掉 ✓）。
         *   C3/C4 做好后在这里按 C3/C4 阈值给盾 ✓，或直接调 `pofv_barrier_fire()` ✓。*/
        pofv_c2_tick(POFV_CARD_ID);
        pofv_barrier_tick();

    /* ★★★★ 2026-10-11 **对抗审计 D 的结论已并账** ✓（它证伪/纠正了我三处 ✗）：
     *   ① ★ 「从 `ce_option_x/y` 出膛 = 一手 `class 5`」**证据不足** ✗ —— 一手 `class 5` 读的是
     *      TH19 自己 `PlayerInf+0x5d8+5*0x128` 的**固定下标 5 槽** ✓，而 `ce_option_x/y` 读的是
     *      **TH18 卡池** `zPlayer+0xa30`（stride 0xf0 ✓、**先到先得** ✗）⇒ ★ 二者**不同源** ✓
     *      ⇒ ★ 只能算"**同功能类比**"✓（= 用"某个子机槽的坐标"做出膛点 ✓，不是逐字段复刻 ✓）。
     *   ② ★ 我原打算走**声明层**（`ce_option_pose_declare`）✗ ⇒ ★ 审计指出 **109 < 111**
     *      ⇒ ★ 会**抢走 111 的落笔权** ✗、且 12 格池与 71/77/108/116 相争 ⇒ 属**跨卡影响** ✓
     *      ⇒ ★ **改用审计给的简单路**：**直接 `ce_option_get`** ✓（不碰声明层 ⇒ 不碰 111 ✓）。
     *   ③ ★ 子机脚本**不能借 108 的 224–227** ✗（会串到 108 的贴图 ✓）⇒ ★ 用**109 自己的 448** ✓
     *      （= 早苗的青蛙 ✓，本会话刚切好 ✓）。
     *   ★ 时机照 SDK 注释 ✓：`on_tick_2 → ce_option_get`（没有就按 retry 补申请 ✓）。*/
    if (s_aura_kind == 3) {
        (void)ce_option_get(c, &s_opt_sanae, 0, (int)CE_ANM_ABILITY_SCRIPT_POFV109_C2S_FROG,
                            8u, "pofv109");   /* ❓offset 取 0 / retry 8 帧（照 SDK 注释的节奏 ✓）*/
    } else if (s_opt_sanae.ptr) {
        ce_option_drop(c, &s_opt_sanae); s_opt_sanae.ptr = 0;   /* ★ 非早苗 ⇒ 当帧收干净 ✓ */
    }
        pofv_c1_tick();                     /* ★ C1：前进 + 伤害源 + 收尾 ✓ */
        pofv_c3_tick();                     /* ★ C3：波次机 + 每颗弹（加速/反弹/伤害源）+ 收尾 ✓ */
        pofv_c4_tick();                     /* ★ C4：扫描弧（两条反向 + 加速 + 伤害源）✓ */
        pofv_c2m_tick();                    /* ★ ② C2 魔理沙：6 条激光 ✓ */
        pofv_c3m_tick();                    /* ★ ⑤ C3 魔理沙：横扫双镜像激光 ✓ */
        pofv_c4m_tick();                    /* ★ ⑤ C4 魔理沙：激光 + 扫射弹 ✓ */
        pofv_c2s_tick();                    /* ★ ② C2 早苗：更慢的 26 发整圈 ✓ */
        pofv_c3s_tick();                    /* ★ ⑤ C3/C4 早苗：交替两侧刷弹 ✓ */
    }
    }   /* ← 2026-10-10 新增的两把键作用域块（`chg_held` / `aura_held`）收尾 ✓ */
    /* ★★ 2026-10-10 结界半径用的"有效蓄力"：见上面两把键的分工注释 ——
     *   有蓄力 ⇒ 用蓄力；没蓄力但**结界键按着**（Shift 单开）⇒ 兜底**满蓄力**（❓自定，旧观感不回退 ✓）；
     *   否则 0（不张、不生成 ✓）。*/
    /* ★★★★ 2026-10-10 owner（**两个量、一个键管** ✓）：见 `s_aura_size` 声明处 —— 尺寸在蓄力 >0 时
     *   **等于蓄力**（⇒ 数值/曲线与改前**完全一致** = owner「手感应当是一手」✓）；Shift 单开（无蓄力）⇒
     *   取满值兜底（= 改前观感 ✓）。两根量都**被 `s_reiryoku`（上限量）封顶** ✓（一手 `S+0x24` ✓）。*/
    /* ★★★★ 2026-10-10 owner 订正（我上一版又错了 ✗）：
     *   「**结界展开和蓄力没有半毛钱关系，跟灵力没有任何关系**，那个标志着展开的所谓量，
     *     **只是服务于动画效果的**！」✓
     *   ⇒ 本量 = **纯动画量** ✓：**只看结界键** ✓（`C ∨ Shift` ✓）——★ 不再取蓄力值 ✗、★ 不再被灵力封顶 ✗。
     *     张开动画本身仍由下面的 `s_aura_anim_*` 逐帧推进 ✓（= 现有观感 ✓，owner「手感应当是一手」✓）。*/
    /* ★ 纯动画量：只看结界键（C∨Shift）✓ —— 不取蓄力 ✗、不被灵力封顶 ✗（owner 一手口径 ✓）。
     * ⚠ owner：**它不是"半径"** ✓，只是大多数角色的画法看起来像半径 ⇒ 逐角色形态见 TUNING ✓。*/
    s_aura_size = pofv_aura_key() ? POFV_AURA_TARGET_CHARGE : 0.0f;
    {
        const float target = s_aura_size * s_aura_k / 100.0f;   /* ★ 一手 `clamp(S+0x8c,0,200) × K / 100` ✓（目标；有效蓄力见上 ✓）*/
        const float dur = (s_aura_kind == 3) ? 60.0f : 20.0f;      /* ★ 逐类时长：早苗 60 ✓ / 其余 20 ✓（咲夜 TH09 另算 ✓）*/
        float f, e;
        /* ★★★ 2026-10-06 owner 实跑：「**灵梦的张开缺少张开动画，变成瞬间张开**」✗ ⇒ 根因：
         *   这个计数器原来**每帧都自增** ✗（结界关着时也在涨 ✓）⇒ 等你按 Shift 时 `f` 早已 ≥1 ⇒ 直接跳到目标 ✗。
         *   ⇒ 改成：**VM 不在（= 新的一段张开）⇒ 计时与起点清零 ✓；VM 在 ⇒ 才推进 ✓**。*/
        /* ★★★ owner 日志一锤定音（2026-10-06 ✓）：`**结界心跳** kind=2 charge=100 R=0.0 vm=0` ✗
         *   ⇒ 根因 = 我先前"防细缝"补丁把动画计数器改成「**只有 VM 存在时才推进**」✗ ⇒ **死锁** ✓：
         *     没 VM ⇒ 计数不动 ⇒ e=0 ⇒ 咲夜 R = 3.0×20×e = **0** ⇒ 永远生不出 VM ✓
         *     （灵梦靠 R 的初值 1.0 侥幸能生 ✓ —— 日志里正是 `kind=0 … R=1.0` ✓）。
         *   ⇒ 改成**按"有没有蓄力"推进** ✓：无蓄力（= 没按 Shift，结界关着）⇒ 计数归零 ✓；
         *     有蓄力 ⇒ 每帧推进 ✓ ⇒ **第一帧就能算出 R>0 ⇒ 生成** ✓（细缝仍不会出现：没蓄力时 R 由下面那行强制为 0 ✓）。*/
        if (s_aura_size <= 0.0f) { s_aura_anim_f = 0; s_aura_anim_cur = 1.0f; }
        else                       { ++s_aura_anim_f; }
        f = (dur > 0.0f) ? (float)s_aura_anim_f / dur : 1.0f;
        if (f > 1.0f) f = 1.0f;                                    /* ★ 走完保持在目标 ✓ */
        e = (s_aura_kind == 3) ? (f * f * f)                       /* ★ 模式 2 = f³ ✓（早苗、先慢后快）*/
                               : (1.0f - (1.0f - f) * (1.0f - f)); /* ★ 模式 4 = 1−(1−f)² ✓（灵梦/魔理沙）*/
        s_aura_anim_cur = 1.0f + (target - 1.0f) * e;              /* ★ 一手定律（逐字）✓ */
        if (s_aura_kind == 2 && s_aura_size > 0.0f) {
            /* ★★★ owner 2026-10-06 裁定：「**咲夜半径走一手 —— 她是 TH09 模型，和其他的不一样**」✓
             *   一手（`th09/02` §3.9–§3.14 ✓）：半径 `+0x7c` 由**每角色 anm 脚本**给 ——
             *     · 咲夜 = **3.0**（`etama.anm` sid17 的 `ins_7(3.0, 3.0)` ✓）；
             *     · 灵梦 = 长到 **96.0**（她的回调 `0x440ce0` ＋ 上限 `0x48e3d4` ✓）；
             *     · **初值 0** ✓（对象创建时整块清零 ✓，脚本没写该通道 ✓）。
             *   ⇒ 撤掉 TH19 光环的 `scaleTime(90, 1, 600.0, 600.0)` ✗（那是 **TH19** 咲夜**光环**的配方 ✗，
             *     与 **TH09** 领域不是一回事 ✓）。
             *   ❓ 她的**张开时长/缓动**一手未给（`+0x7c` 由引擎回调长 ✓）⇒ 暂借其它自机的 ease（20 帧模式 4 ✓）
             *     朝 **3.0** 长 ✓；❓ 3.0 的**量纲**（渲染尺度 vs 像素半径）待一手补 ✓。*/
            /* ★★★ owner 2026-10-06 实跑：「**咲夜的领域不存在**」✗ ⇒ 根因（本轮定位 ✓）：
             *   一手那个 `+0x7c = 3.0` 被我们当成**像素半径**直接用 ✗ ⇒ 而她的脚本是 `drawPoly(1.0f, 32)`
             *   （**半径 1 像素**的多边形 ✓）⇒ 缩放 3.0 ⇒ **3 像素的点** ⇒ 看不见 ✓✓。
             *   ⇒ 一手 3.0 的**量纲不明** ❓（很可能是"相对基准尺寸的倍率"✗，不是像素 ✓）
             *     ⇒ 暂乘一个换算 `POFV_AURA_SAKUYA_UNIT`（❓自定，取 20 ⇒ 约 **60 像素**的圆 ✓，与其它自机同级可见 ✓）；
             *     待一手的真实换算到位 ⇒ **改这一个常量**即可 ✓。*/
            s_aura_anim_cur = POFV_AURA_K_SAKUYA * e;   /* 0 → 3.0×20 = 60 ✓ */
        }
        s_aura_r = s_aura_anim_cur;
        /* ★★ 只有**咲夜**用它（owner 明确：「总之是你就改咲夜的，其他别动」✓）：
         *   一手（`engine/versus/th09/02-ghost-and-seal.md` §3.6e ✓）：领域扇节点 = `EffectSys` sid 7
         *   ⇒ `etama.anm` **script91** ⇒ `scale(24.0f, 24.0f)`（opcode 7 ✓）⇒ 写进 `VM+0x288`
         *   ⇒ `EffectSys__operator_new` 在 `0x40d49a` **只拷一次** ⇒ `node+0x7c = 24`（**静态** ✓）
         *   ⇒ **半径 = 24 游戏单位 ≈ 48px** ✓，且 **所有角色同值** ✓（随角色变的是**半张角** `+0x84` ✗）。
         *   ⇒ 所以咲夜用**固定 48px** ✓（不再是 `charge × K/100` 的 ❓ 自定 ✗）；其他角色维持原样 ✓ 不动 ✗。*/
        /* â 2026-10-07 owner：「一手核的不对」✗ ⇒ **撤回固定 48px** ✗，回到 `charge × K/100`（K=64 ⇒ 满蓄力 64px ✓）；半径真值等另一位核完再定 ❓（现为 ❓ 自定）*/
        /* ââ 2026-10-08 owner 定口径（`HANDOVER-AURA-SECTOR.md` §19.15 ✓）：
         *   「相对**玩家身位**保持一致」✓（场地宽只是量尺 â）：
         *   一手 TH09：自机身位 = 24 单位（`pl02.ecl` 的 `enemy_set_hitbox(24,24)` ✓）、
         *   领域半径 = **144 单位** ⇒ **= 6 身位** ✓；两作同为 640×480 且**同一套角色美术**（自机像素尺寸一致 ✓）
         *   ⇒ **TH18 目标半径 ≈ 6 身位 ≈ 144 屏幕 px** ✓（我们原来只有 64px â ⇒ 小 2.25 倍 ✓）。
         *   → **只给咲夜** ✓（owner：其他三人的 TH19/TH18 跨代问题在此处不存在 â）。
         *   ❓ 单位换算：若卡里的 `aura_r` 是**世界单位（2 表面 px）** ⇒ 应填 **72** ✓；若就是**屏幕 px** ⇒ 填 **144** ✓。
         *   本卡现用 144（与其他三人同一套量纲 ✓ ⇒ 实测一眼可判：半径≈6 身位就对 ✓）。*/
        if (s_aura_kind == 2) s_aura_r = 144.0f;                                /* 视觉 = 判定 = 同一个值 ✓ */
    }
    /* ★★★★ 2026-10-06 owner 实跑关键更正：「**不是松手后的细缝，是我从来没按 shift 就有一个细缝挂着**」✗
     *   ⇒ 根因：没按 Shift 时 `蓄力 = 0` ⇒ 目标 = 0 ✓，但**追赶层的初值是 1.0** ✗ ⇒ `R = 1.0 > 0` ✓
     *     ⇒ **一开始就生成了一台 scale=1 的带** ✓ = 那条"细缝" ✓✓（我先前以为是收束残留 ✗，治错了地方 ✗）。
     *   ⇒ 修法：**没蓄力（没按 Shift）⇒ R 直接归 0** ✓（不 spawn、判定半径也是 0 ✓）。*/
    /* ★ 一手 §11.1：holder 每帧回调 ⇒ 蓄力 `+0x20` **每帧 −1**（衰减 ✓）、冷却 `+0x34` 每帧 −1 ✓ */
    if (s_ex_gauge > 0) --s_ex_gauge;
    if (s_ex_cool > 0)  --s_ex_cool;
    /* ★ 诊断心跳（2026-10-06）：把「Shift 有没有被读到 / R 是多少」每 60 帧曝光一次 ✓
     *   —— 与「结界出生」那行合用，可分开判断「没按到键 ✗」与「按到了但没生成/看不见 ✗」。*/
    if (s_aura_charge > 0.0f && !(s_card_frames % 60)) {
        ce_log("pofv: **结界心跳** kind=%d charge=%.0f size=%.0f R=%.1f vm=%d",
               (int)s_aura_kind, (double)s_aura_charge, (double)s_aura_size, (double)s_aura_r, (int)s_aura_vm);
        }
        {   /* â 2026-10-08 ownerï¼版顶一只朝右的狼灵、无碰撞箱、只是 VM â
             *   ⇒ 对账日志无「残留」â ⇒ 把**所有活的 body VM**逐个列出 ⇒ 一眼看出它属谁 ✓。*/
            int iz; int nbody = 0;
            for (iz = 0; iz < POFV_SPIRIT_MAX; ++iz) if (s_spirit_body[iz]) ++nbody;
            {   /* â owner 新报：②「用灵梦测试 ⇒ **完全没有 VM 的敌人**」â ③「**再也没见过 TH09 幽灵**」â
                 *   ⇒ 光看 body 不够 ✗ ⇒ 把**灵圆 VM**也算进来 ✓（两个数一对照就知道是“本体没建”还是“圆没建”✗）。*/
                int nring = 0;
                for (iz = 0; iz < POFV_SPIRIT_MAX; ++iz) if (s_spirit_vm[iz]) ++nring;
                ce_log("pofv: [VM] body å­æ´»=%d 个 ｜ 灵圆 VM å­活=%d 个", nbody, nring);
            }
            /* ââ 2026-10-08 ååºæ¶å²ï¼ownerï¼版顶一只不动的狼灵 â）：
             *   实æµ发现有槽位「**id è¿å¨ãVM è¿å¨、但敌机早已不在**」
             *   （某些移除路径**不发死亡事件** â）⇒ 这里每 300 帧用
             *   `ce_enemy_by_id()` **直接查敌机是否还在** ⇒ 不在就**强制收** ✓（不靠超时猜 â）。*/
            {   uint8_t *em2 = CE_ENEMY_MGR();
                if (em2) { int iz2;
                    for (iz2 = 0; iz2 < POFV_SPIRIT_MAX; ++iz2) {
                        /* ★★★★ 2026-10-08 实测日志定位（**版顶那只狼灵** ✓）：
                         *   `槽5 vm=… id=0 kind=-1` ＋ `槽7 vm=… id=0 kind=-1` ⇒ **有 VM、无户口** = 孤儿本体 ✓✓
                         *   ⇒ 我原来的收割**只遍历 `id != 0` 的槽** ✗ ⇒ **整类孤儿被跳过** ✗✓
                         *   ⇒ 现在**两类都收**：① `id==0 && body!=0`（无户口孤儿 ✓）② `id!=0` 但敌机已不在 ✓。*/
                        if (!s_spirit_id[iz2]) {
                            if (s_spirit_body[iz2] || s_spirit_vm[iz2]) {
                                ce_log("pofv: [收] 槽%d **无户口孤儿**（VM=%d/%d kind=%d）⇒ 强制收 ✓",
                                       iz2, (int)s_spirit_vm[iz2], (int)s_spirit_body[iz2], (int)s_spirit_kind[iz2]);
                                if (s_spirit_body[iz2]) { ce_anm_delete((uint32_t)s_spirit_body[iz2]); s_spirit_body[iz2] = 0; }
                                if (s_spirit_vm[iz2])   { ce_anm_delete((uint32_t)s_spirit_vm[iz2]);   s_spirit_vm[iz2] = 0; }
                                s_spirit_kind[iz2] = -1; s_spirit_dis[iz2] = 0; s_spirit_dis_age[iz2] = 0;
                            }
                            continue;
                        }
                        if (ce_enemy_by_id(em2, (int32_t)s_spirit_id[iz2])) continue;
                        ce_log("pofv: [æ¶] æ§½%d id=%u **敌机已不在** â 强制收（VM=%d kind=%d）",
                               iz2, (unsigned)s_spirit_id[iz2], (int)s_spirit_body[iz2], (int)s_spirit_kind[iz2]);
                        if (s_spirit_body[iz2]) { ce_anm_delete((uint32_t)s_spirit_body[iz2]); s_spirit_body[iz2] = 0; }
                        if (s_spirit_vm[iz2])   { ce_anm_delete((uint32_t)s_spirit_vm[iz2]);   s_spirit_vm[iz2] = 0; }
                        s_spirit_id[iz2] = 0; s_spirit_kind[iz2] = -1;
                        s_spirit_dis[iz2] = 0; s_spirit_dis_age[iz2] = 0;
                    }
                }
            }
            for (iz = 0; iz < POFV_SPIRIT_MAX; ++iz)
                if (s_spirit_body[iz])
                    {   /* â owner：依旧有版顶狼灵 â ⇒ 判「**槽里的 id 对应的敌机到底是不是同一只**」✗
                         *   （`ce_enemy_by_id` 找得到 ⇒ 引擎里确有该 id 的敌机 â；但可能是**旧 id 被别的敌机复用**）
                         *   ⇒ 把敌机的**坐标**一起打出来 ✓ ⇒ 与 VM 位置对比即可判死 ✓。*/
                        uint8_t *em3 = CE_ENEMY_MGR();
                        uint8_t *e3 = em3 ? ce_enemy_by_id(em3, (int32_t)s_spirit_id[iz]) : 0;
                        const float *ep = e3 ? ce_enemy_pos(e3) : 0;
                        ce_log("pofv: [VM]   æ§½%d vm=%d id=%u kind=%d level=%d | 敌机=æ å标=(%.0f,%.0f)",
                               iz, (int)s_spirit_body[iz], (unsigned)s_spirit_id[iz], (int)s_spirit_kind[iz],
                               (int)s_spirit_level[iz], (double)(ep ? ep[0] : -9999.0f), (double)(ep ? ep[1] : -9999.0f));
                    }
        }
        if (s_aura_vm) {   /* ★ 2026-10-07 另一路诊断 ✓：一次定论「透明到底出自哪一路」——
                            *   `csel`(+0x52c) = 渲染器**真正画出去**的快照那一路 ✓（A 在最高字节 ✓ 一手 07-…md:47,90 ✓）；
                            *   判读：`csel` 的 A **不是 0xFF** ⇒ 快照早于卡侧写入 ⇒ 脚本必须 `alpha(255)` ✓；
                            *         `csel` 的 A **是 0xFF** ⇒ 透明另有其人 ⇒ 转查父 VM / blend / 渲染阶段 ✓。*/
            uint8_t *vmd = (uint8_t *)ce_anm_get_vm((uint32_t)s_aura_vm);
            if (vmd) {
                /* ★★ 2026-10-07：`+0x52c` 被证伪 ✗（加了 colorMode 仍是 0）⇒ 把颜色/α 区整段打出来 ✓，
                 *   用**多条 ce_log**（本仓没有 ce_snprintf ✗）⇒ 每行 4 个 dword，字段由原始数据来定 ✓。*/
                ce_log("pofv: aura520 %08x %08x %08x %08x",
                       *(uint32_t *)(vmd + 0x520), *(uint32_t *)(vmd + 0x524),
                       *(uint32_t *)(vmd + 0x528), *(uint32_t *)(vmd + 0x52c));
                ce_log("pofv: aura530 %08x %08x %08x %08x",
                       *(uint32_t *)(vmd + 0x530), *(uint32_t *)(vmd + 0x534),
                       *(uint32_t *)(vmd + 0x538), *(uint32_t *)(vmd + 0x53c));
                ce_log("pofv: aura540 %08x %08x %08x %08x",
                       *(uint32_t *)(vmd + 0x540), *(uint32_t *)(vmd + 0x544),
                       *(uint32_t *)(vmd + 0x548), *(uint32_t *)(vmd + 0x54c));
                /* ★★ 2026-10-07 验收结论 ✓：「两路 A 都是 ff ⇒ `Sa=1` ⇒ **透不是 α 造成的** ✓」⇒
                 *   那就要看**几何量的运行真值** ✓：张角（`rotation.x`，弧度 ✓）× 缩放（`scale` ✓）
                 *   —— 若张角实际接近 0 ⇒ 画出来是**一条细缝** ✓ = 观感上的"很浅/很透" ✓。
                 *   （宏名以 `sdk.h` 为准 ✓；rotation 3 个 float、scale 2 个 float ✓，按 dword 打十六进制 ✓。）*/
                {   const float *r3 = (const float *)(vmd + CE_ANM_VM_ROTATION);
                    const float *s2 = (const float *)(vmd + CE_ANM_VM_SCALE);
                    ce_log("pofv: aurageom rotx=%d roty=%d rotz=%d scalex=%d scaley=%d",
                           (int)(r3[0] * 1000.0f), (int)(r3[1] * 1000.0f), (int)(r3[2] * 1000.0f),
                           (int)(s2[0] * 1000.0f), (int)(s2[1] * 1000.0f));
                }
            }
        }
    if (s_aura_size <= 0.0f) { s_aura_r = 0.0f; s_aura_anim_f = 0; }
    if (s_aura_r > 0.0f) {
        const float *pp4 = ce_player_pos();
        if (pp4) {
            const int is_marisa = s_aura_is_marisa;
            const float aura_r  = s_aura_r;
            if (!s_aura_vm) {
                /* 一手 §9.3：灵梦 = `pl00` script15（圆）/ 魔理沙 = `pl01` script15+16/17（竖带）✓ */
                const int scr = (s_aura_kind == 1) ? CE_ANM_ABILITY_SCRIPT_POFV109_AURA_MARISA
                              : (s_aura_kind == 2) ? CE_ANM_ABILITY_SCRIPT_POFV109_AURA_SAKUYA
                                                   : CE_ANM_ABILITY_SCRIPT_POFV109_AURA;
                s_aura_vm = (int32_t)ce_anm_spawn(CE_ABILITY_ANM(), scr, 11);
                s_aura_close = 0; s_aura_close_ang = 0.0f;   s_aura_dir_snap = 1;   /* ★ owner：不张开时往右走、再张开 ⇒ 应从**目标侧直接开始** ✓（一手如此 ✓），不该"从右转圈回左"✗ */   /* ★ 新激活清零收束状态 ✓（owner：再按 Shift 会"先缩到 0 再张开"✗ = 残留 ✓）*/
                /* ★★ 2026-10-07 **换层实验** ✓（那位定的单刀 ✓）：层 **6–11 → RT surface 1** ✓、
                 *   **12–19 → surface 0** ✓（一手层表 ✓）⇒ 8 落在 surface 1 ⇒ 一直"透" ✗
                 *   ⇒ 试 **12**（surface 0 ✓）：若**实心** ⇒ 定案「surface 1 的合成在压 α」✓ ⇒ 就用 12 收工 ✓。*/
                /* ★ 2026-10-07 日志定案 ✓：**层 6 在这台 ability-anm 上渲染不出来** ✗（TH19 一手的 `layer(6)` 照搬无效 ✓）
                 *   ⇒ 改成 **8**，与「**能渲染的圆/灵**」同层 ✓（本卡 `:1922` 早就写过这条结论 ✓）。*/
                /* ★★★ owner 连续两次「**没有扇形**」✗ ⇒ 先把"到底走了哪一支"曝光出来 ✓
                 *   （若是角色号判错 ⇒ 会给咲夜挂上灵梦的**整圆** 402 ✗ ⇒ 当然"没有扇形" ✓）。
                 *   日志：kind（0 灵梦/1 魔理沙/2 咲夜/3 早苗 ✓）、char（引擎读到的角色号 ✓）、script、R ✓。*/
                ce_log("pofv: **结界出生** kind=%d script=%d R=%.1f（kind 期望：0灵梦/1魔理沙/**2咲夜**/3早苗 ✓；咲夜应为 script=%d ✓）",
                       (int)s_aura_kind, (int)scr, (double)s_aura_r, (int)CE_ANM_ABILITY_SCRIPT_POFV109_AURA_SAKUYA);
            }
            if (s_aura_vm) {
                /* ★★★ owner 2026-10-06：「**魔理沙只有人物屁股后面**」✗ ⇒ 根因：她的两条带是
                 *   `drawRectGrad(1.0f, 448.0f)` 且 `anchor(2,1)`/`anchor(1,1)`（**底下对齐** ✓）
                 *   ⇒ 我若把 VM 放在**自机位置**，那 448 高只有「从自机往上」一截在屏内 ⇒ 看着只有"身后" ✓。
                 *   ⇒ **竖带（魔理沙）把 y 放到场地中线**（`POFV_FIELD_H/2 = 224` ✓）⇒ 正好铺满场地 ✓；
                 *     其余形状仍跟随自机 ✓（圆的中心就该在自机身上 ✓）。*/
                ce_anm_set_pos((uint32_t)s_aura_vm, pp4[0],
                               (s_aura_kind == 1) ? 0.0f : pp4[1], 0.0f);
                /* ★★★★ 2026-10-07 owner 实跑抓到我的破坏 ✗：「**灵梦的领域没了，魔理沙的领域瞬间张开**」
                 *   ⇒ 根因：这句**共用的** `set_scale` 被我前面几次正则改**误删**了 ✗（grep 计数 = 0 ✓）
                 *     ⇒ 灵梦的缩放停在 1（单位圆 ⇒ 看不见 ✗）、魔理沙的竖带也失去每帧缩放（一次铺满 ⇒ "瞬张" ✗）。
                 *   ⇒ 恢复，并按族给对形状 ✓（一手：槽 3 每帧写**位置 + 缩放（= 半径）** ✓，视觉半径 ≡ 判定半径 ✓）：
                 *     · 灵梦/早苗（圆 ✓）：`(R, R)`；· 魔理沙（竖带 ✓）：`(R, 1)`（y 保持 1 ✓）；
                 *     · 咲夜（扇形 ✓）：`(2R, R)` ⇒ 由 `texArcEven` 的几何 `r_in = scale.y − scale.x/2 = 0` ⇒ **实心** ✓。*/
                ce_anm_set_scale((uint32_t)s_aura_vm,
                                 /* ★★★ 2026-10-07 owner：「**判定贴合领域**」＋「半径两边一致?」✓
                                  *   弧族几何（`th16/09-drawing.md:231-232`）：`r_out = scale.y + scale.x/2`、
                                  *   `r_in = scale.y − scale.x/2`。
                                  *   判定用的是 `s_aura_r`（咲夜 = 一手 **48px** ✓）。
                                  *   ⇒ 要 `r_out = s_aura_r` 且实心 ⇒ **`scale = (R, R/2)`** ✓
                                  *     （先前写 `(2R, R)` ⇒ `r_out = 2R` = **96** ✗ ⇒ 视觉半径是判定的 2 倍 ✗）。*/
                                 (s_aura_kind == 2) ? aura_r : aura_r,
                                 (s_aura_kind == 2) ? (aura_r * 0.5f) : ((s_aura_kind == 1) ? 1.0f : aura_r));
                if (s_aura_kind == 2) {
                    /* ★★ owner 两项（2026-10-07）：② **拖拽** ✓（= 一手 `+0x80`：朝自机方向以 **0.25/帧**缓动 ✓，
                     *   方向就用卡里已有的 8 向索引 `s_aura_dir8i` ✓）；同时把**张角**也由卡侧一次给全 ✓
                     *   （`ce_anm_set_rotation` **一次写全三分量** ✗ ⇒ 脚本的 `rotateTime` 会被覆盖 ✗
                     *    ⇒ 所以张角同样由卡侧按 +3°/帧、上限 90° 来驱动 ✓ = 一手 `th09/02:545-548` ✓）。*/
                    float ang = (float)s_aura_anim_f * 0.05235988f;      /* 张角 = +3°/帧 ✓、上限 π/2 ✓ */
                    float tgt, d;
                    if (ang > 1.5707964f) ang = 1.5707964f;
                    {   /* ★★ owner 两条（2026-10-07）：①「**朝着运动方向——反了**」✗ ⇒ 目标取**反向**（+180° ✓）；
                         *   ②「**转动速度跟无穷大似的、最后只有八个方向**」✗ ⇒ 我原来直接写 `idx × 45°`
                         *      = **整数索引** ⇒ 瞬跳 + 8 向 ✗ ⇒ 改成**连续缓动** ✓（最短弧 × **0.25/帧** ✓ = 一手 ✓）。*/
                        /* ★★ owner：「**恰好是移动方向 ⇒ 差一个负号**」✗ ⇒ 屏幕 **y 向下** ⇒ 我加的 +π 被镜像抵消 ✓
                         *   ⇒ 本表 **y 取负** ✓ ⇒ 这样下面的 `+ π` 才是真正的"运动方向的反向"（拖在后面 ✓）。*/
                        /* ★★ owner：「**左右挡在身前、上下拖在身后**」✗ ⇒ 只剩**左右**反 ⇒ 再**把 x 也取负** ✓
                         *   （= 相对上一版做一次**关于竖轴的镜像** ✓）⇒ 左右、上下都落在"身后" ✓。*/
                        static const float d8b[8][2] = { {-1,0},{-0.70710678f,-0.70710678f},{0,-1},{0.70710678f,-0.70710678f},
                                                        {1,0},{0.70710678f,0.70710678f},{0,1},{-0.70710678f,0.70710678f} };
                        int bi = 0, i8; float bs = 1e9f;
                        for (i8 = 0; i8 < 8; ++i8) {   /* 点积**最小** ⇒ 最接近**反向** ✓ */
                            const float dp = s_aura_dir_x * d8b[i8][0] + s_aura_dir_y * d8b[i8][1];
                            if (dp < bs) { bs = dp; bi = i8; }
                        }
                        tgt = (float)bi * 0.78539816f + 3.14159265f;   /* 反向 = 目标角 + π ✓ */
                        d = tgt - s_aura_mid_ang;
                        while (d >  3.14159265f) d -= 6.28318531f;      /* 最短弧 ✓ */
                        while (d < -3.14159265f) d += 6.28318531f;
                        if (s_aura_dir_snap) { s_aura_mid_ang = tgt; s_aura_dir_snap = 0; }   /* ★ 首帧吸附 ✓（不绕圈 ✗）*/
                        else s_aura_mid_ang += d * 0.25f;                                        /* 之后照一手 0.25/帧缓动 ✓ */                    /* ★ 一手：方向 0.25/帧缓动 ✓ */
                    }
                    /* ★★ owner：「看看其他自机的领域怎么写的」✓ ⇒ 收束**交脚本**（`406` 的 interrupt 支
                     *   = `rotateTime(20,4,0,0,0)` ⇒ 张角归零 ✓，与 402 的 `scaleTime(20,4,0,0)` 同构 ✓）
                     *   ⇒ 所以**蓄力结束后卡侧不再写 rotation** ✗（否则每帧把它压回去 ✗）。
                     *   ⚠ 我先前那套卡侧收束（`s_aura_close`）作废 ✗ —— 保留变量以免动太多 ✗。*/
                    {   /* 收束期也照写 ✓ —— 用缩小的角 + 同一朝向 ⇒ 不乱转 ✓ */
                        float wang = (s_aura_close > 0)
                                   ? (s_aura_close_ang * ((float)s_aura_close / 20.0f)) : ang;
                        ce_anm_set_rotation((uint32_t)s_aura_vm, wang, 0.0f, s_aura_mid_ang);
                        if (s_aura_close > 0 && --s_aura_close <= 0) {
                            ce_anm_delete((uint32_t)s_aura_vm); s_aura_vm = 0; s_aura_close = 0;   /* 归零 ⇒ 删 + 清句柄 ✓ */
                        }
                    }
                }
                if (s_aura_kind == 2) {
                    /* ★★★★ owner 实跑判定（乙）：「**像一块半透明的布、能透出后面**」✗ ⇒ α 真的低 ✓
                     *   ⇒ 说明几何图元的 α **不是**脚本 `alpha/alpha2` 那对给的 ✗（`color/color2` 是 RGB 端 ✓）
                     *   ⇒ **由卡侧显式写 RGBA** ✓：`ce_anm_set_color(id, 0xRRGGBBAA)`（一手同款用法：
                     *     本卡 EX 就用 `0xFFFFFF80` 写 α=128 ✓）⇒ 取**亮蓝 + α=FF（全不透明）** ✓。*/
                    /* ★★ 一手：弧族取色看 `flags_lo & 0x60000` ⇒ 可能读 **color_2** ✗（本仓 `sdk.h:300` 已有
                     *   `ce_anm_set_color2` ✓）⇒ **两个寄存器都写、且保持同一渐变对** ✓ ⇒ 无论引擎读哪个都对 ✓。*/
                    /* ⚠ owner 实跑：「**扇形没了**」✗ ⇒ 卡侧写 `color_2`（`CE_ANM_VM_COLOR2 = +0x528` ✓ SDK 注释给的）
                     *   很可能是**错偏移** ✗ ⇒ 写坏 VM ⇒ 什么都不画 ✓ ⇒ **本步回退** ✗（回到"看得见、只是透"那版 ✓）；
                     *   颜色仍由**脚本**的 `color/color2` 给 ✓（引擎读哪个寄存器都有一份 ✓）。*/
                    /* ★★★★ 2026-10-07 **真凶定案** ✗→✓：`vm+0x524 color_1` 是 **D3DCOLOR `0xAARRGGBB`** ✓
                     *   （一手 `engine/anm/th18/01-vm-instantiate.md:56` ✓）—— **A 在最高字节** ✗ 不是最低 ✓！
                     *   我先前几版全写成了 `0xRRGGBBAA` ✗ ⇒ A 被当成了别的通道 ⇒ 症状完全对上 ✓：
                     *     · `0x64C0FFFF` ⇒ A=0x64(100) ⇒ ~39% ⇒ **「固有半透明」** ✓
                     *     · `0x003280FF` ⇒ A=0x00 ⇒ 触发早退（`color_1.A==0 && color_2.A==0 ⇒ 整台 VM 不画` ✓
                     *       `engine/anm/th18/07-blendmode-rgb-only-alpha-is-sa.md:50` ✓）⇒ **「扇形没了」** ✓
                     *   ⇒ 本版按 **AARRGGBB** 写：`0xFF64C0FF` = A=FF、R=64、G=C0、B=FF（亮蓝·不透明 ✓）。*/
                    /* ★ owner 实跑：「**依旧很浅**」✗ ⇒ A=FF 已解决"透" ✓，问题在 **RGB 太浅** ✗
                     *   （`64 C0 FF` = 淡青蓝 ✓）⇒ 本步**只改 RGB 一个变量** ✓：`00 32 80` = 深蓝 ✓。*/
                    ce_anm_set_color((uint32_t)s_aura_vm, 0x80FFFFFFu   /* ★ A=0x80(128) 白 ✓ —— 我先前把 alpha 写进了 **Blue** 通道 ✗（0xFFFFFF80 ⇒ R=G=FF、B=80 ⇒ 偏黄 ✓ 与实跑一致 ✓）*/);   /* ★ 白(RGB 让贴图渐变显色 ✓) + A=0x80=128 ✓ 与其它三人一致 ✓ */    /* A=FF、深蓝 (0,50,128) ✓ */
                    ce_anm_set_color2((uint32_t)s_aura_vm, 0x80FFFFFFu   /* ★ A=0x80(128) 白 ✓ —— 我先前把 alpha 写进了 **Blue** 通道 ✗（0xFFFFFF80 ⇒ R=G=FF、B=80 ⇒ 偏黄 ✓ 与实跑一致 ✓）*/);  /* 同值 ✓ */   /* ★ 与 color_1 同为深蓝 ⇒ 走哪路都不淡 ✓ */   /* 同格式 ✓；默认 colorMode 下不参与 ⇒ 写法无害 ✓ */
                }   /* ★ owner：「只延伸到中线」✗ ⇒ 从场地**顶端**铺满 0..448 ✓ */
                /* ★ 照一手「槽 3 每帧写**位置 + 缩放（= 半径）**」✓ ⇒ 视觉半径 ≡ 判定半径 ✓
                 *   （脚本里是**单位圆** `drawPoly(1.0f,64)` ✓ ⇒ 缩放就是像素半径 ✓）。*/
                if (s_aura_kind == 2) {
                    /* ★ owner 2026-10-06：「**依旧 scale**」✗ ⇒ 根因：卡侧每帧写 `scale` ⇒ 把"张开"变成了缩放 ✓；
                     *   而卡侧写 `rotation` 又**不生效** ✗（多次实测 ✓）
                     *   ⇒ 咲夜的**形状与张角全部交给脚本** ✓（`406` 里 `texArcEven` + `rotateTime(30…)` ✓），
                     *     卡侧**只写位置** ✓（下面的 `set_pos` 在共同路径里 ✓）—— 不要在这里写 scale/rotation ✗。*/
                    /* ★★ owner：「扇形的**中线**… ⇒ **拖拽感**来源」✓
                     *   ⇒ 方向用卡里**已经在算**的那套（`:1417` ✓ 按自机位移取最近 8 向 ＋ **0.25/帧缓动** ✓，
                     *     即一手 `th09/02:557` 的 `+0x80` ✓；"拖在后面"= 这层滞后本身 ✓）。
                     *   ⇒ 这里只把索引换成角度写进领域朝向 ✓（`idx × 45°` ✓，无 libm ✓）。*/
                    /* ★★★ 验收定位（2026-10-06 ✓）：「**没有扇形**」的真根因 = 上一版我在这里写 `rx = 0` ✗
                     *   —— `ce_anm_set_rotation` **一次写全三分量**（`sdk.h:287-294` ✓）⇒ **每帧把脚本
                     *     `rotateTime` 正在生长的张角压回 0** ✗ ⇒ 弧角恒 0 ⇒ 画不出扇形 ✓。
                     *   ⇒ 正解：**卡侧一次给全**（`rx` = 张角 ✓、`rz` = 朝向 ✓）⇒ 脚本就**不再动 rotation** ✗：
                     *     · `rx` = 张角 = 全张角（弧度 ✓，由 `+3°/帧` 累积到 90° ✓，与判定同源 ✓）
                     *     · `rz` = 朝向 = 已有的 8 向索引 × 45° ✓（0.25/帧滞后 = 一手"拖后"✓）
                     *   ⇒ 并**恢复卡侧写 scale**（`scale=(2R, R)` ⇒ `r_in = 0` = **实心扇** ✓，且视觉半径≡判定半径 ✓）。
                     *     （验收：`scale.x` 只是"环宽" ✓、`rotation.x` 才是张角 ✓ —— 出处 `th16/09-drawing.md:243-247` ✓）*/
                    /* ★ owner：「**先解决有无的问题**」✓ ⇒ 本版咲夜**卡侧一行都不写** ✗
                     *   （`ce_anm_set_rotation` 会**一次写全三分量** ⇒ 每帧把脚本的 `rotate(π,0,π/2)` 覆盖掉 ✗；
                     *    `set_scale` 同理覆盖 `scale(6,59)` ✗ ⇒ 先让脚本**原样**跑，确认 112 的观感能出来 ✓）。*/
                    (void)0;
                    (void)aura_r;
                }
                if (s_aura_kind == 2) {
                    /* ★ 7 片一起跟随 ✓：**每片重心沿自己的朝向往外挪**（挪到半径中段 ✓）
                     *   —— 因为 `drawPoly(1.0f,3)` 画的是"以中心为中心"的整三角 ✗（owner 实跑：一堆中心重合的正三角 ✓）
                     *   ⇒ 把每片的位置按它的 θ 偏移 `0.5R × 0.55` ⇒ 7 片拼起来才像**扇面** ✓。
                     *   θ 与脚本 `rotate` 一一对应（−45°…+45° 每 15° ✓）；(cos,sin) 用**离线表** ✓（本仓无 libm ✗）。*/
                    static const float cs7[POFV_AURA_PIECES][2] = { {0.70710678f, 0.70710678f}, {0.86602540f, 0.50000000f}, {0.96592583f, 0.25881905f}, {1.00000000f, -0.00000000f}, {0.96592583f, -0.25881905f}, {0.86602540f, -0.50000000f}, {0.70710678f, -0.70710678f} };   /* ★ y 取负：游戏 y **向下** ✓（owner：偏移方向反了 ✗）*/
                    const float off7 = aura_r * 0.5f * 0.55f;      /* ❓自定：重心落点系数 ✓ */
                    int ip2;
                    for (ip2 = 0; ip2 < POFV_AURA_PIECES; ++ip2) if (s_aura_piece[ip2]) {
                        ce_anm_set_pos((uint32_t)s_aura_piece[ip2],
                                       pp4[0] + cs7[ip2][0] * off7,
                                       pp4[1] + cs7[ip2][1] * off7, 0.0f);
                        ce_anm_set_scale((uint32_t)s_aura_piece[ip2], aura_r, aura_r);
                    }
                }
                /* ★ 一手：缩放 = 半径 ✓；魔理沙的主 VM 是「带」（`scale(16.0f, 1.0f)` ⇒ y 保持 1 ✓）*/
            }
        }
    } else if (s_aura_close > 0) {
        /* ★★★ 收束分支（复核 Q2 指定的位置 ✓：与 `charge>0` 同级、在 `:1726` 之前 ⇒ **每帧都会走到** ✓）
         *   只写 `rx`（张角收缩 ✓）＋ `rz` 沿用卡侧维护的朝向 ✓（⇒ 不瞬跳、不乱转 ✓）；
         *   最后 10 帧按**一手**做 α 淡出（`alphaTime(10,0,0)` 的等价：A 线性 255→0 ✓）。*/
        if (s_aura_vm) {
            float k = (float)s_aura_close / 20.0f;
            ce_anm_set_rotation((uint32_t)s_aura_vm, s_aura_close_ang * k, 0.0f, s_aura_mid_ang);
            if (--s_aura_close <= 0) { ce_anm_delete((uint32_t)s_aura_vm); s_aura_vm = 0; s_aura_close = 0; }
        }
    } else if (s_aura_vm && s_aura_kind != 2) {                   /* ★ 咲夜不在此处删 ✗（脚本 rotateTime 收束后自删 ✓） ✓（一手 `+20: delete()` ✓）*/
        ce_anm_delete((uint32_t)s_aura_vm); s_aura_vm = 0;
    }
    /* ★★★ 2026-10-08 **hits 的第二条归零路 = 连锁暴风窗口 X = 36 帧** ✓（订正后 ✓）——
     *   「距上次连锁暴风 > X」⇒ hits 归零 ✓（一手 `FUN_004102b0` 入口的「不属于当前连锁」等价物 ✓）。
     *   ⚠ 上一版我用的「暴风池为空」是**近似** ✗ ⇒ 已换成**计时器 + X** ✓。*/
    if (s_chain_storm_age < s_chain_window) {
        ++s_chain_storm_age;
    }
    /* （上一版撤了这条、本版按 owner「门控都 ×2」恢复并按倍数放宽 —— 见下面的分支 ✓）
     * ★★★★ 2026-10-08 **日志定案**（owner：「我搞不懂了，大部分情况下都是 0」✗）：
     *   日志证据：① `[FEED]` 每次击破都有 ✓（妖精也能启动 ✓）；② **每行 `cur=0`** ✗；
     *   ③ `连锁暴风窗口过（36 帧）⇒ hits 1 归零` **反复出现** ✗。
     *   ⇒ 机制忠实地在跑 ✓，但 **X = 36 帧（0.6 s）对"我们的击杀节奏（≈每 1 秒一只）"太紧** ✗
     *     —— TH09 里每秒几十只、hits 一路涨 ✓；我们这边就变成 **0↔1 反复闪** ✓。
     *   ⇒ **撤回「连锁暴风窗口清 hits」** ✗，改让 **hits 自己的 120 帧寿命**淡出 ✓
     *     （= 一手 **HUD 显示层**的口径 ✓：「值变大重设 119/120 帧、**不动计数** ✓」⇒ 停手后约 2 s 才消失 ✓）。
     *   ⚠ 这是 **owner 口径 > 一手细节** 的又一次取舍 ✓（X 与 class 表仍留在 `s_chain_window` ✓，
     *     ❓ 若之后要"紧"的手感，把上面那个 else 分支恢复即可 ✓ —— 已在 TUNING 登记 ✓）。*/
    /* ★★★ 2026-10-08（owner：「门控都 ×2」✓）⇒ **恢复这条清空**（上一版我撤掉了 ✗）——
     *   恢复后门控才真的起作用 ✓，而 ×2 后的窗口（72/80/88 帧 ≈ 1.2–1.5 s ✓）比一手宽松一倍 ✓。*/
    if (s_chain_seen && s_hits.cur != 0 && s_chain_storm_age >= s_chain_window) {
        ce_log("pofv: 【TH09】连锁暴风窗口过（%d 帧 ≥ X=%d）⇒ hits %d 归零 ✓",
               (int)s_chain_storm_age, (int)s_chain_window, (int)s_hits.cur);
        s_hits.cur = 0; s_hits.latch = 0; s_hits.life = 0;
        s_chain_seen = 0;
    }
    /* ★★★★ 2026-10-08（owner：「即使有暴风，这个值也固定是 0」✗ ⇒ 真因 ✓）：
     *   `pofv_hits_tick` 的锁存门里有 **`0 < h->active`** ✓，而 **`active` 从来没人置 1** ✗
     *   ⇒ **latch 永远不更新** ⇒ HUD 显示（读 `latch` ✓）**恒 0** ✓。
     *   ⇒ 本卡在场时把它置 1 即可（一手 `+0x227c` 的 active 门 ✓；这里没有"濒死"概念 ⇒ 恒 1 ✓）。*/
    s_hits.active = 1;
    pofv_hits_tick(&s_hits);                     /* ★★★★ 2026-10-08 A 组「Charge 当前档/上限档」（一手 script68–71 ✓）*/
    /* ★★★★ 2026-10-10 TH19 連撃账：窗口逐帧 −1、**穿 0 才归零**（一手 `FUN_005450e0` 的
     *   `0x5459d8`–`0x5459ee` ⇒ `0x545a2e` ✓；「已 ≤0 不归零」的 quirk 也照抄 ✓）。
     *   放在这里 = 每帧一次 ✓（`s_hits` 的 HUD 层与它各走各的、互不影响 ✓）。*/
    combo_th19_tick();
    {
        /* ★★ 2026-10-10 逐处定类：这里是 **A 组（`Charge 当前/上限`）的显隐** = **蓄力**那一类
         *   ⇒ `pofv_charge_key()` = **C only** ✓（owner：「level n/m 那个是仅限蓄力，按 shift 不是蓄力」✓）。*/
        const int held = pofv_charge_key();
        s_hud_chg_hold = held ? (s_hud_chg_hold + 1) : 0;
        if (s_hud_chg_hold >= 10 && s_aura_charge > 0.0f) {     /* 一手：满 10 帧 + 当前值≠0 ✓ */
            const float *pp = ce_player_pos();
            /* ★★★ 2026-10-09（owner：「位置十分靠左上、尺寸偏小」✗ ⇒ 坐标/偏移/缩放都缺 ×2）：
             *   自机坐标 → **表面 px**（`POFV_HUD_SURF_*` 的推导与 117 先例见那两行宏的注释 ✓）；
             *   下面每个偏移都是一手 640 空间原值 **×2** ✓。
             * ★★★ 2026-10-10 owner：「A/B 两组比子机偏右、有一个恒定的 x 差」⇒ 再整体加 `POFV_HUD_TH09_DX`（❓自定 ✓）。*/
            /* ★★❓自定（owner：「偏离一手：横向拉伸成整个屏幕宽度、避让主动卡槽」✓）：
             *   ★ 位置与宽度**都吃申领到的槽** ✓（`pofv_hud_chg_claim()` 申领的就是"整个剩余宽度"
             *   且已保守避让卡槽 ✓）⇒ ★ 横跨整屏 ✓、★ 不再跟随自机 ✗、★ 不再写死 448 ✗。*/
            float bx = POFV_HUD_SURF_X(pp ? pp[0] : 0.0f) + POFV_HUD_TH09_DX,
                  by = POFV_HUD_SURF_Y(pp ? pp[1] : 0.0f);
            /* ★★ 量纲改一手后，四档**间距不等**（500/1000/1700/2500 ✓）⇒ 档位改成
             *   「**逐阈值计数**」✓ —— ★ 正是**一手**的判据（`charge ≥ 阈ᵢ` ✓，§17 ✓）。*/
            /* ★★❓自定（owner 2026-10-11 **二次修正**）：★ 先前"拉伸成全屏宽"**退回** ✗ ——
             *   owner 实跑：「不蓄力时跟以前一样、按了蓄力**突然整体拉伸**」✗、
             *   「**黑衬底（`s_hud_chg_track_vm` = 452×12 ✓）没拉伸**」✗（★ 我上一版确实没碰它 ✓ = 那正是病根 ✓）
             *   并定：「**还是退回原来的条的长度**，但是有一条：**把整个槽摆在屏幕的正中**」✓。
             *   ⇒ ★ 宽度回**一手值** 448 ✓（`POFV_HUD_CHG_FILL_W` ✓，底槽 452 本来就是它 ✓ ⇒ 两者重新对齐 ✓）；
             *   ⇒ ★ `bx` 改**水平居中** ✓（★ 不再跟随自机 ✗、★ 不再拉伸 ✗）。★ 底槽也吃同一个 `bx` ✓。
             *   ★ 表面屏宽 = 640×480 × 2 = **1280** ✓（`POFV_HUD_CHG_X2` / `HUD-LAYOUT.md` §1.1 ✓）。*/
            bx = (POFV_HUD_SCREEN_W_SURF - POFV_HUD_CHG_BLOCK_W_SURF) * 0.5f
               + POFV_HUD_CHG_BORDER;
            int32_t tier = 0;
            if (s_aura_charge >= POFV_C1_THRESH) ++tier;
            if (s_aura_charge >= POFV_C2_THRESH) ++tier;
            if (s_aura_charge >= POFV_C3_THRESH) ++tier;
            if (s_aura_charge >= POFV_C4_THRESH) ++tier;   /* ★ 0..100 ⇒ 4 档（每档 25；此前除 50 只到 2 档 ✗）*/
            if (tier < 0) tier = 0;
            if (tier > POFV_HUD_CHG_MAX_TIER) tier = POFV_HUD_CHG_MAX_TIER;
            const int flash = ((s_card_frames / 10) & 1) ? 1 : 0;   /* 一手：每 10 帧一轮 ✓ */
            if (!s_hud_chg_label_vm)   /* 标签 (0,−40) → 表面 (0,−80) ✓ ×2 */
                s_hud_chg_label_vm = (int32_t)hud_vm_spawn(CE_ANM_ABILITY_SCRIPT_HUD_GLYPH, bx, by - 80.0f);
            if (!s_hud_chg_cur_vm)     /* 当前档 (−12,−28) → 表面 (−24,−56) ✓ ×2 */
                s_hud_chg_cur_vm   = (int32_t)hud_vm_spawn(CE_ANM_ABILITY_SCRIPT_HUD_GLYPH, bx - 24.0f, by - 56.0f);
            if (!s_hud_chg_slash_vm)   /* 斜杠 (0,−28) → 表面 (0,−56) ✓ ×2 */
                s_hud_chg_slash_vm = (int32_t)hud_vm_spawn(CE_ANM_ABILITY_SCRIPT_HUD_GLYPH, bx, by - 56.0f);
            if (!s_hud_chg_max_vm)     /* 上限档 (+12,−28) → 表面 (+24,−56) ✓ ×2 */
                s_hud_chg_max_vm   = (int32_t)hud_vm_spawn(CE_ANM_ABILITY_SCRIPT_HUD_GLYPH, bx + 24.0f, by - 56.0f);

            /* ★★ ② 三行字（一手 ✓）：tier 2 ⇒ Skill ✓、tier 3 ⇒ Ex.Attack Lv n ✓、
             *   tier 4 ⇒ Boss Lv n ✓；tier <= 1 ⇒ 清空（一手 C1 没有这行 ✓）。
             *   坐标 = 原标签位（表面值 ÷2 ⇒ 640 空间 ✓）；颜色沿用既有 flash 闪 ✓。*/
            {   const uint32_t col = flash ? 0xFF80FF80u : 0xFFFFFFFFu;
                const int lv = (tier >= 4) ? s_c4_lv_n : s_c3_lv_n;
                if (tier == 2)
                    ce_hud_vm_text_set_font(&s_hud_chg_lv_text, bx * 0.5f, (by - 80.0f) * 0.5f,
                                            col, 0, 1.0f, "Skill", "pofv109lv", 0);
                else if (tier >= 3) {
                    /* ★ 不引 `<stdio.h>` ✗：★ 手拼（★ `lv` = 0..7 ⇒ 一位数 ✓；★ 也免掉任何浮点格式化 ✓）*/
                    char out[32];
                    {   const char *pre = (tier >= 4) ? "Boss Lv" : "Ex.Attack Lv";
                        int o = 0, k;
                        for (k = 0; pre[k]; ++k) out[o++] = pre[k];
                        out[o++] = (char)('0' + (lv % 10));
                        out[o] = 0;
                    }
                    ce_hud_vm_text_set_font(&s_hud_chg_lv_text, bx * 0.5f, (by - 80.0f) * 0.5f,
                                            col, 0, 1.0f, out, "pofv109lv", 0);
                } else ce_hud_vm_text_clear(&s_hud_chg_lv_text);
            }
            if (s_hud_chg_label_vm) {
                ce_anm_set_pos((uint32_t)s_hud_chg_label_vm, bx, by - 80.0f, 0.0f);
                ce_anm_set_scale((uint32_t)s_hud_chg_label_vm, 2.0f, 2.0f);   /* ★ 一手 640 空间 1:1 → 表面 ×2 ✓ */
                ce_anm_set_sprite(CE_ABILITY_ANM(), (uint32_t)s_hud_chg_label_vm, CE_ANM_ABILITY_SPRITE_POFV109_TH09_UI_CHARGE);
                ce_anm_set_color((uint32_t)s_hud_chg_label_vm, flash ? 0xFF80FF80u : 0xFFFFFFFFu);
            }
            if (s_hud_chg_cur_vm) {
                ce_anm_set_pos((uint32_t)s_hud_chg_cur_vm, bx - 24.0f, by - 56.0f, 0.0f);
                ce_anm_set_scale((uint32_t)s_hud_chg_cur_vm, 2.0f, 2.0f);     /* ★ 同上 ×2 ✓ */
                ce_anm_set_sprite(CE_ABILITY_ANM(), (uint32_t)s_hud_chg_cur_vm,
                                  CE_ANM_ABILITY_SPRITE_POFV109_TH09_UI_DIGIT_0 + (uint32_t)tier);
                ce_anm_set_color((uint32_t)s_hud_chg_cur_vm, flash ? 0xFF80FF80u : 0xFFFFFFFFu);
            }
            if (s_hud_chg_slash_vm) {
                ce_anm_set_pos((uint32_t)s_hud_chg_slash_vm, bx, by - 56.0f, 0.0f);
                ce_anm_set_scale((uint32_t)s_hud_chg_slash_vm, 2.0f, 2.0f);   /* ★ 同上 ×2 ✓ */
                ce_anm_set_sprite(CE_ABILITY_ANM(), (uint32_t)s_hud_chg_slash_vm, CE_ANM_ABILITY_SPRITE_POFV109_TH09_UI_SLASH);
                ce_anm_set_color((uint32_t)s_hud_chg_slash_vm, 0xFFC0C0C0u);   /* 灰静态 ✓ */
            }
            if (s_hud_chg_max_vm) {
                /* ★★ 2026-10-11 owner 实跑：「**m 还是 4**」✗ ⇒ ★ 一手口径：
                 *   ★ `m` = **当前灵力能蓄到的最大等级** ✓（灵力 500 ⇒ 1 / 1000 ⇒ 2 / 1700 ⇒ 3 / 2500 ⇒ 4 ✓），
                 *   ★ 不是恒为 `POFV_HUD_CHG_MAX_TIER`（= 4）✗。★ 算法 = 数"阈值 ≤ 灵力"的个数 ✓。*/
                int mreach = 0;
                if (s_reiryoku >= POFV_C1_THRESH) ++mreach;
                if (s_reiryoku >= POFV_C2_THRESH) ++mreach;
                if (s_reiryoku >= POFV_C3_THRESH) ++mreach;
                if (s_reiryoku >= POFV_C4_THRESH) ++mreach;
                ce_anm_set_pos((uint32_t)s_hud_chg_max_vm, bx + 24.0f, by - 56.0f, 0.0f);
                ce_anm_set_scale((uint32_t)s_hud_chg_max_vm, 2.0f, 2.0f);     /* ★ 同上 ×2 ✓ */
                ce_anm_set_sprite(CE_ABILITY_ANM(), (uint32_t)s_hud_chg_max_vm,
                                  CE_ANM_ABILITY_SPRITE_POFV109_TH09_UI_DIGIT_0 + (uint32_t)mreach);
                ce_anm_set_color((uint32_t)s_hud_chg_max_vm, 0xFFC0C0C0u);     /* 灰静态 ✓ */
            }
        } else {
            /* 松手/未满 10 帧 ⇒ 一手用 `interrupt(…,1)` 收掉 ✓（我们自己删 VM、句柄清 0 ✓）*/
            if (s_hud_chg_label_vm) { ce_anm_delete((uint32_t)s_hud_chg_label_vm); s_hud_chg_label_vm = 0; }
            if (s_hud_chg_cur_vm)   { ce_anm_delete((uint32_t)s_hud_chg_cur_vm);   s_hud_chg_cur_vm = 0; }
            if (s_hud_chg_slash_vm) { ce_anm_delete((uint32_t)s_hud_chg_slash_vm); s_hud_chg_slash_vm = 0; }
            if (s_hud_chg_max_vm)   { ce_anm_delete((uint32_t)s_hud_chg_max_vm);   s_hud_chg_max_vm = 0; }
        }
    }

    /* ★★★★ 2026-10-08 B 组「Gauge Filled / level」（一手 script72–74 ✓）
     * ★★★★ 2026-10-10 owner 口径 ⇒ **档位变化那一帧触发的一次性 78 帧弹出**（见变量处注释 + 一手出处 ✓）：
     *   · 触发 = `s_reiryoku` 的**档位数值变化**（不是"原始值变了就弹" ✓）；
     *   · 内容 = 档位（`clamp(灵力/满值 × 4)` ✓）；位置 = A/B 共同的左移 + 一手的自动避让 ✓。*/
    {
        float rl_ratio = s_reiryoku / POFV_REI_MAX;   /* ★ 同上：灵力必须除 2500 ✓ */
        int32_t tier;
        if (rl_ratio < 0.0f) rl_ratio = 0.0f;
        if (rl_ratio > 1.0f) rl_ratio = 1.0f;
        tier = (int32_t)(rl_ratio * (float)POFV_HUD_CHG_MAX_TIER);
        if (tier < 0) tier = 0;
        if (tier > POFV_HUD_CHG_MAX_TIER) tier = POFV_HUD_CHG_MAX_TIER;
        /* ★ 上升沿 = **档位数值变化**（一手 `0x41bd1e cmp edi,eax; je skip` ⇒ 相等不触发 ✓）；
         *   到期前再变 ⇒ **重放**（重设 78 帧 ✓，照一手"重设脚本/重建状态" ✓）。*/
        if (tier != s_hud_lv_last_tier) {
            s_hud_lv_last_tier = tier;
            s_hud_lv_left = POFV_HUD_LV_POP_FRAMES;
            ce_log("pofv: 【灵力】档位 %d/%d ⇒ 弹一次（%d 帧 ✓；灵力 %.0f/%.0f）",
                   (int)tier, POFV_HUD_CHG_MAX_TIER, POFV_HUD_LV_POP_FRAMES,
                   (double)s_reiryoku, (double)POFV_REI_MAX);
        }
        if (s_hud_lv_left > 0) {
            const float *pp = ce_player_pos();
            /* ★ 78 帧的**相位**（一手：3 淡入 / 60 闪 / 15 淡出 ✓）；`elapsed = 78 − left` ✓。
             *   α 上限 = 一手 `ins_34(3,0,0xc0)` 的 **0xc0 = 192** ✓。*/
            const int32_t elapsed = POFV_HUD_LV_POP_FRAMES - s_hud_lv_left;
            uint32_t a;
            if (elapsed < 3)        a = (uint32_t)(192 * elapsed / 3);                             /* 3 帧淡入 ✓ */
            else if (elapsed >= 63) a = (uint32_t)(192 * (POFV_HUD_LV_POP_FRAMES - elapsed) / 15); /* 15 帧淡出 ✓ */
            else                    a = 192u;                                                      /* 60 帧保持 ✓ */
            if (a > 192u) a = 192u;
            --s_hud_lv_left;   /* ★ 每帧只减一次 ✓ */
            /* ★★★ 2026-10-09（同 A 组：owner「位置十分靠左上、尺寸偏小」✗）：自机坐标 → **表面 px**
             *   （`POFV_HUD_SURF_*` 的推导 / 117 先例见那两行宏的注释 ✓），偏移全部 **×2** ✓。*/
            float bx = POFV_HUD_SURF_X(pp ? pp[0] : 0.0f) + POFV_HUD_TH09_DX;
            /* ★ 一手避让（**复刻** ✓）：A 组在显示 ⇒ B 组整体上移 48 表面 px。
             *   判据与 A 组**完全一致**（`s_hud_chg_hold >= 10 && s_aura_charge > 0` ✓，
             *   `s_hud_chg_hold` 在上面那块刚更新过 ⇒ 同帧取值一致 ✓）；与 B 的显隐条件**无关** ✓。*/
            float by = POFV_HUD_SURF_Y(pp ? pp[1] : 0.0f)
                     + (((s_hud_chg_hold >= 10 && s_aura_charge > 0.0f) ? POFV_HUD_LV_AVOID_DY : 0.0f));
            const int flash = ((s_card_frames / 30) & 1) ? 1 : 0;          /* 一手：30 帧一轮 ✓ */
            const uint32_t c_gf    = (a << 24) | (flash ? 0x00FFFFFFu : 0x00E0E0E0u);
            const uint32_t c_label = (a << 24) | (flash ? 0x00FFFFFFu : 0x00A0A0A0u);
            if (!s_hud_lv_gf_vm)       /* 标签 (0,−40) → 表面 (0,−80) ✓ ×2 */
                s_hud_lv_gf_vm    = (int32_t)hud_vm_spawn(CE_ANM_ABILITY_SCRIPT_HUD_GLYPH, bx, by - 80.0f);
            if (!s_hud_lv_label_vm)    /* 标签 (−8,−28) → 表面 (−16,−56) ✓ ×2 */
                s_hud_lv_label_vm = (int32_t)hud_vm_spawn(CE_ANM_ABILITY_SCRIPT_HUD_GLYPH, bx - 16.0f, by - 56.0f);
            if (!s_hud_lv_digit_vm)    /* 档位数字 (+18,−28) → 表面 (+36,−56) ✓ ×2 */
                s_hud_lv_digit_vm = (int32_t)hud_vm_spawn(CE_ANM_ABILITY_SCRIPT_HUD_GLYPH, bx + 36.0f, by - 56.0f);
            if (s_hud_lv_gf_vm) {
                ce_anm_set_pos((uint32_t)s_hud_lv_gf_vm, bx, by - 80.0f, 0.0f);
                ce_anm_set_scale((uint32_t)s_hud_lv_gf_vm, 2.0f, 2.0f);      /* ★ 一手 640 空间 1:1 → 表面 ×2 ✓ */
                ce_anm_set_sprite(CE_ABILITY_ANM(), (uint32_t)s_hud_lv_gf_vm,
                                  CE_ANM_ABILITY_SPRITE_POFV109_TH09_UI_GAUGEFILLED);
                ce_anm_set_color((uint32_t)s_hud_lv_gf_vm, c_gf);
            }
            if (s_hud_lv_label_vm) {
                ce_anm_set_pos((uint32_t)s_hud_lv_label_vm, bx - 16.0f, by - 56.0f, 0.0f);
                ce_anm_set_scale((uint32_t)s_hud_lv_label_vm, 2.0f, 2.0f);   /* ★ 同上 ×2 ✓ */
                ce_anm_set_sprite(CE_ABILITY_ANM(), (uint32_t)s_hud_lv_label_vm,
                                  CE_ANM_ABILITY_SPRITE_POFV109_TH09_UI_LEVELLABEL);
                ce_anm_set_color((uint32_t)s_hud_lv_label_vm, c_label);
            }
            if (s_hud_lv_digit_vm) {
                ce_anm_set_pos((uint32_t)s_hud_lv_digit_vm, bx + 36.0f, by - 56.0f, 0.0f);
                ce_anm_set_scale((uint32_t)s_hud_lv_digit_vm, 2.0f, 2.0f);   /* ★ 同上 ×2 ✓ */
                ce_anm_set_sprite(CE_ABILITY_ANM(), (uint32_t)s_hud_lv_digit_vm,
                                  CE_ANM_ABILITY_SPRITE_POFV109_TH09_UI_DIGIT_0 + (uint32_t)tier);
                ce_anm_set_color((uint32_t)s_hud_lv_digit_vm, c_label);
            }
        } else {
            /* ★ 78 帧走完 ⇒ 一手 `ins_1()` 自删的等价：卡侧真删 3 台 VM + 清句柄 ✓
             *   （期间**没有** hide/interrupt 路径 ✓）。*/
            if (s_hud_lv_gf_vm)    { ce_anm_delete((uint32_t)s_hud_lv_gf_vm);    s_hud_lv_gf_vm = 0; }
            if (s_hud_lv_label_vm) { ce_anm_delete((uint32_t)s_hud_lv_label_vm); s_hud_lv_label_vm = 0; }
            if (s_hud_lv_digit_vm) { ce_anm_delete((uint32_t)s_hud_lv_digit_vm); s_hud_lv_digit_vm = 0; }
        }
    }


    /* ★ HUD hits：锁存 + 寿命递减（一手 120 帧 ✓）*/
    /* ★★ 调试 UI（owner）：事件列表（暴风延迟 ✓ / 击破·固灵 3 s ✓）+ 幽灵列表（只有内容、无位置 ✓）*/
    {
        int32_t i2, j2;
        for (i2 = 0; i2 < POFV_UI_EV_MAX; ++i2) {
            if (s_ui_ev[i2].ttl > 0) --s_ui_ev[i2].ttl;
            if (s_ui_ev[i2].ttl > 0) {   /* ★ owner 2026-10-05：「**显示 TTL 后消失**」⇒ 活着才显示 ✓（原来条件反了 ✗：空闲槽会常驻显示 nm[0] ✗）*/
                /* ★ owner：「镜像事件列表我要**文字，是条目**」⇒ 每条一句描述 ✓ */
                static const char *nm[4] = { "storm -> foe (delayed)",
                                             "kill -> foe+1, foe -> back",
                                             "seal -> foe nearest (3s)",
                                             "pure -> foe" };
                char buf[40]; int L = 0; const char *q;
                for (q = nm[s_ui_ev[i2].what & 3]; *q && L < 38; ++q) buf[L++] = *q;
                buf[L] = 0;
                ce_hud_vm_text_set(&s_ui_ev_vm[i2], POFV_HUD_HITS_X, POFV_HUD_HITS_Y + 24.0f + (float)i2 * 16.0f,
                                   0xFFFFFFFFu, 1, 1.0f, "", "pofv109");   /* ★ owner：字母画不出 ✗ ⇒ 不再画英文（只留 ce_log ✓）*/
            } else {
                ce_hud_vm_text_set(&s_ui_ev_vm[i2], 0.0f, 0.0f, 0, 1, 1.0f, "", "pofv109");
            }
        }
        /* 幽灵列表：只写「有什么」（按 kind 计数 ✓，**不涉及位置** ✓）*/
        {
            int32_t c[POFV_SPIRIT_KIND__COUNT]; char buf[40]; int L = 0; const char *q;
            for (j2 = 0; j2 < (int32_t)POFV_SPIRIT_KIND__COUNT; ++j2) c[j2] = 0;
            for (j2 = 0; j2 < POFV_FOE_SPIRIT_MAX; ++j2) if (s_foe[j2].active) {
                int32_t k2 = s_foe[j2].type;
                if (k2 >= 0 && k2 < (int32_t)POFV_SPIRIT_KIND__COUNT) ++c[k2];
            }
            for (q = "foe: w"; *q; ++q) buf[L++] = *q;
            { int32_t v = c[POFV_SPIRIT_KIND_WOLF]; buf[L++] = (char)('0' + (v > 9 ? 9 : v)); }
            for (q = " o"; *q; ++q) buf[L++] = *q;
            { int32_t v = c[POFV_SPIRIT_KIND_OTTER]; buf[L++] = (char)('0' + (v > 9 ? 9 : v)); }
            for (q = " e"; *q; ++q) buf[L++] = *q;
            { int32_t v = c[POFV_SPIRIT_KIND_EAGLE]; buf[L++] = (char)('0' + (v > 9 ? 9 : v)); }
            for (q = " p"; *q; ++q) buf[L++] = *q;
            { int32_t v = c[POFV_SPIRIT_KIND_PURE]; buf[L++] = (char)('0' + (v > 9 ? 9 : v)); }
            for (q = " g"; *q; ++q) buf[L++] = *q;
            { int32_t v = c[POFV_SPIRIT_KIND_GHOST_TH09]; buf[L++] = (char)('0' + (v > 9 ? 9 : v)); }
            buf[L] = 0;
            ce_hud_vm_text_set(&s_ui_gh_vm, POFV_HUD_HITS_X, POFV_HUD_HITS_Y + 24.0f + (float)POFV_UI_EV_MAX * 16.0f,
                               0xFF80FFFFu, 1, 1.0f, "", "pofv109");   /* ★ owner：字母画不出 ✗ ⇒ 不再画英文（只留 ce_log ✓）*/
        }
    }
    /* ★ HUD 绘制（只本侧 ✓）：一手 —— 只有 `latch != 0` 才画 ✓；数字 + `hits` 标签 ✓；
     *   `active < 1` 时按寿命 %6 在**黄 ↔ 灰**切换（6 帧周期 ✓）*/
    {   /* ââ TH09 **連擃槽**（一手 §2.1 ✓：SP 正下方的白色长条 ✓）—— VM ＋ 每帧写 `scale.x` 表达长度 ✓
         *   （与本卡 `404/405` 同手法 ✓，已实跑证明可渲染 ✓）。
         *   ❓ 一手的"条怎么画"未定位 ð¡ ⇒ 本处 = **❓自定**（纯几何白条 ✓）；位置/锚点也标 ❓，待实跑校正 ✓。
         *   比例 = `a2c / thr2c(難度)` ✓（难度取法照本卡现有写法 `s_card_frames/60` ✓ ❓）。*/
        int32_t c2thr = pofv_acc_thr2c(s_card_frames / 60);   /* （保留：連擊那条的一手口径仍在 ✓）*/
        float c2r;
        /* ★★★ 连击条 = **屏幕最下方的白色长方形** ✓（owner：「其中连击条就是一个白色长方形」✓
         *   + TH09 参照图：两栏底部都有一条白色长条 ✓）⇒ 恢复上一位写的 `POFV109_HUD_COMBO`
         *   （脚本 407：`drawRectGrad` 纯白 1×6 ✓，本仓能跑的条形写法 ✓）。
         *   ⚠ 我上一轮换成的 `418/419`（`sprite40/41` 红/绿分段图）**是错的** ✗ ——
         *     那两张不是连击条 ✓（脚本与贴图**留着不删** ✓：删会平移 sprite 号 ✗）。*/
        if (!s_hud_c2_vm)
            s_hud_c2_vm = (int32_t)hud_vm_spawn(CE_ANM_ABILITY_SCRIPT_POFV109_TH09_BAR_COMBO,
                                                POFV_HUD_BAR_X, POFV_HUD_BAR_Y);
        /* ★★★ 2026-10-08 owner：「底下的白色横线（连击条）**不随时间降低**，为什么？」✗
         *   ⇒ 因为驱动源用错了 ✗：那条**一手本来就是「充能槽」**（`engine/versus/th09/01-versus-loop.md` §2.1 ✓）——
         *     **外层 = 充能容量** `zPlayer+0x30388`（0..400，击杀喂 / 发射 −100/200/300 ✓）、
         *     **内层 = 当前充能量** `zPlayer+0x30384`（**按住 charge 才涨** `+= dt×(sht+0x24)` ✓；
         *     `GuiSide::on_tick` `0x418a90` 用 `scale.x = 值 × 0.0025f` 各画一条 ✓）。
         *   而我先前吃的是 `a2c`（**連擊计数器** ✓：只被击破喂大 ✗、**没有每帧衰减** ✗）⇒ **只涨不掉** ✓。
         *   ⇒ 改吃**充能量**：本卡的 `s_aura_charge`（0..200 ✓，Shift 蓄力 ✓；松手自然衰减 ✓）✓。*/
        /* ★★★ 2026-10-08 owner 订正：「我要的白色条是**连击条**，而不是按 shift 的**蓄力条**指示」✗⇒✓
         *   ⇒ 驱动源回到 **TH09 連擊计数器** `s_acc.a2c / thr2c`（我先前误改成 Shift 蓄力 ✗）。*/
        c2r = (c2thr > 0) ? ((float)s_acc.a2c / (float)c2thr) : 0.0f;
        /* ââ ä¸æï¼`01-versus-loop.md` §2.1 â，æ äºè®®çé£æ¡）：
         *   「連擃槽**æ¶失（清空）⇒ **SP 归零**」â
         *   ï¼一手的**衰减律**写作 `+= 難度×4 â 120` â 量纲可疑 ❓ â **本卡不实现衰减律** â，
         *    但「清空⇒SP 归零」这条**无争议** â ⇒ 本处落地 â。）*/
        /* ★★★★ 2026-10-08 **一手逐帧衰减**（§3.34 ✓，作废我上一版的「105 帧到点清空」✗）：
         *   · **連擊槽 tA**（`s_acc.a2c` ✓）：**每帧 −1**（下限 0）✓；漏空 ⇒ **清連爆数** ✓；
         *   · **SP 寿命 tB**（`s_sp_life` ✓）：**每帧 −1**（下限 0）✓；漏空 ⇒ **SP = 0** ✓（一手 `0x41d7cf` ✓）。
         *   ⇒ 观感正是**两段式**：停手 ≈1 s 槽先漏空（連爆数归零）⇒ 再 ≈0.75 s（45 帧）SP 归零 ✓。*/
        if (s_acc.a2c > 0 && --s_acc.a2c == 0) {
            if (s_combo_blasts != 0) {
                ce_log("pofv: 【TH09】連擊槽漏空 ⇒ 連爆数 %d → 0（一手 `+0x38` 清零 ✓）", (int)s_combo_blasts);
                s_combo_blasts = 0;
            }
            /* ⚠ 2026-10-08 撤回「槽漏空 ⇒ hits 清 0」✗：owner 实跑「hit 值几乎一直固定在 0」✗ ——
             *   連擊槽 60 帧就漏空（一手 ✓），而玩家击破是间断的 ⇒ 这条一挂上，hits 几乎总是被清 ✓。
             *   ⇒ 改回：**槽漏空只清連爆数**（一手 `0x41d798` ✓），**hits 只由「连锁暴风窗口过」清** ✓
             *     （= 结案的 (A) ✓；(B) 只作用于連爆数/SP ✓）。❓ 若观感要更"紧"，再把这条挂回来 ✓。*/
        }
        if (s_sp_life > 0 && --s_sp_life == 0 && s_sp != 0) {
            ce_log("pofv: 【TH09】SP 寿命漏空 ⇒ SP %d → 0（一手 `0x41d7cf` 清 `+0x10` ✓）", (int)s_sp);
            s_sp = 0;
        }
        if (s_acc.a2c <= 0 && s_sp != 0) {
            s_sp = 0;                                  /* ★ 連擃槽清空 â SP 归零 â */
            if (!(s_card_frames % 60)) ce_log("pofv: [SP] 連擃槽清空 â SP 归零 â（一手 §2.1）");
        }
        if (c2r > 1.0f) c2r = 1.0f;
        if (c2r < 0.0f) c2r = 0.0f;
        /* ★★★★ 2026-10-08 owner 实跑：「**撞弹之后，连击条再也没有出现过**」✗ ⇒
         *   典型**陈旧句柄**：撞弹（死亡/复活链）会把 HUD 的 VM 收走 ✗，而句柄还是非 0
         *   ⇒ `if (!s_hud_c2_vm)` 永远不成立 ⇒ **再也不重建** ✓。
         *   ⇒ 照本仓 `animal.c` 的先例**自愈**：`ce_anm_set_pos` 返回 0 ⇒ 清句柄 ⇒ 下一帧重建 ✓。*/
        if (s_hud_c2_vm && !ce_anm_set_pos((uint32_t)s_hud_c2_vm, POFV_HUD_BAR_X, POFV_HUD_BAR_Y, 0.0f)) {
            s_hud_c2_vm = 0;   /* ★ 陈旧 ⇒ 下一帧重建 ✓ */
        }
        if (s_hud_c2_vm) {   /* ★ 白色长方形：长度 = 满宽 × 比例 ✓（脚本是 1×N 的纯几何矩形 ✓
                              *   ⇒ scale.x 就是**像素长度** ✓，与 `animal.c` 的条同一个口径 ✓）*/
            /* ★ 一手 `script59`：**`scale.x` = 槽比例（0..1 ✓，不是像素长度 ✗）**、`scale.y` 恒 1 ✓
             *   —— 原图 sprite42 就是**满长 72px** 的条，长度靠 scale 缩 ✓（图自带白→灰纵向渐变 ✓）。*/
            /* ★★★ 2026-10-08 owner：「条**向右延申 1 倍**就对了」✓ ⇒ `scale.x` 由 `c2r` 改 **`2×c2r`** ✓
             *   （精灵 sprite42 满长 72px ✓ ⇒ 翻倍后最长 **144** ✓）。
             *   ❓ 若要**正好等于框宽**（框可见 160 ✓）⇒ 用 **160/72 = 2.222** 这个系数 ✓（一个数，随时可调）。*/
            ce_anm_set_scale((uint32_t)s_hud_c2_vm, 2.0f * c2r, 1.0f);
        }
        if (s_hud_c2_fill_vm) { ce_anm_delete((uint32_t)s_hud_c2_fill_vm); s_hud_c2_fill_vm = 0; }
    }
    /* == TH19 的 `hits` 大字标签：**已撤下** ✗ ================================
     *   owner（2026-10-08 实跑截图）：「**那个巨大无比的 HITS 是 th19 的东西**就摆在那个地方」✗。
     *   一手也支持撤下：那支精灵本身就是 **128×32** 的 TH19 大字（`ascii_big` 的 `hits` ✓）
     *   ⇒ 摆在 TH09 风格的 SP 框旁边**天然就不协调** ✗；
     *   而 owner 要的形制是 **TH09 那边**的：「**Hit**」标签 + **红色数字** ✓
     *   ⇒ 已改由上面的 SP 行实现（红套 721–730 ✓）。
     *   ⚠ 句柄（`s_hud_hits_vm` / `s_hud_hitsf_vm` / `s_hud_hits_num`）**保留** ✓，
     *     `clear_card` 里的真删照旧 ✓（铁律：卡的 HUD 对象必须收干净 ✓）。*/
    /* == TH19 アタックゲージ（攻击槽）：**已撤下** ✗ ==========================
     *   owner（2026-10-08 实跑截图）：「**中间的红线**和**右边的什么蓝色菱形**我 tm 也不知道
     *   是什么 jb 东西」✗ —— 就是这里：
     *     · 中间那条**竖红线** = 槽体（脚本 410 照抄原作 `scale(8,112) + rotate(−π/2)` ⇒ **竖向** ✓），
     *       但卡侧又用 `ce_anm_set_scale(ag_vm, 192.0f * agr, 1.0f)` **覆盖**了它 ✗ ⇒ 被拉爆 ✓；
     *     · 蓝色菱形 = 4 个阈值标记（脚本 412 ✓）的位置还在旧的 ×2 坐标上 ✗。
     *   ❓ 卡侧 scale 与原作 `scale(8,112)`/`(2,116)` 的**量纲关系未逐字核出** ✗
     *   ⇒ **不发明、先撤下** ✓（等一手核清再上；句柄与 `clear_card` 的真删**保留** ✓）。*/
    /* == TH09 Bonus indicator: REMOVED ==
     *   owner: "Bonus 不是 th09 的吗" -> yes: the SP-full "Bonus!" is TH09's
     *   (one-hand: SP full -> FUN_0040f860(...,5000)). TH19 has NO such indicator,
     *   and I had wrongly cut it from TH19 ascii.png (and cut the wrong pixels too).
     *   => unhooked here; TH09 art to be located in TH09 front.png (owner pointer:
     *      first big row, right part = TH09 hit + SP UI).
     */
    /* ★★★★ 2026-10-08 owner 实跑：「**撞弹之后，连击条再也没有出现过**」✗ ⇒ 撞弹/复活的死亡链会把
     *   HUD 的 VM 收走 ✗，而句柄仍非 0 ⇒ 下面各处的 `if (!vm)` **永远不成立** ⇒ 再也不重建 ✓。
     *   ⇒ 每个 HUD 帧先做一次**句柄体检**：`ce_anm_get_vm` 查不到 ⇒ 清 0 ⇒ 自然重建 ✓（本仓 `animal.c` 先例 ✓）。*/
    {
        /* ★★★★ 2026-10-08 卡 109 的**蓄力槽**（脚本 422：底槽 + 蓝填充 + 白段 + 4 标）也进这台体检 ✓
         *   （owner 要求：「把这台也加进去」✓；本块只**清 0**，重建在下面那节里 ✓ ⇒ 撞弹/复活后自愈 ✓）。
         *   ⚠ 顺带修一处**既有越界写**：本数组原声明为 `hs[16]`，而上面已经 push 了 19 个指针
         *   （3 + 4 + 3 + 6 + 3 = 19 ⇒ `hs[16..18]` 越界 ✗，写坏栈上相邻变量）⇒ 这里 16 → **32**。
         *   19 + （底槽 1 + 蓝填充 1 + 白段 1 + 菱形标 4 + 越过光晕 4）= **30** ✓（≤ 32 ⇒ 仍有余量 ✓）。 */
        int32_t *hs[32]; int nh = 0, ih;
        hs[nh++] = &s_hud_c2_vm; hs[nh++] = &s_hud_sp_frame_vm; hs[nh++] = &s_hud_sp_hit_vm;
        hs[nh++] = &s_hud_chg_label_vm; hs[nh++] = &s_hud_chg_cur_vm;
        hs[nh++] = &s_hud_chg_slash_vm; hs[nh++] = &s_hud_chg_max_vm;
        hs[nh++] = &s_hud_lv_gf_vm; hs[nh++] = &s_hud_lv_label_vm; hs[nh++] = &s_hud_lv_digit_vm;
        for (ih = 0; ih < 6; ++ih) hs[nh++] = &s_hud_sp_digit_vm[ih];
        for (ih = 0; ih < 3; ++ih) hs[nh++] = &s_hud_hit_digit_vm[ih];
        hs[nh++] = &s_hud_chg_track_vm;                                                  /* ★ 蓄力槽：底槽（深灰/暗）*/
        hs[nh++] = &s_hud_chg_fill_vm;                                                   /* ★ 蓄力槽：蓝填充（= 灵力比例）*/
        hs[nh++] = &s_hud_chg_white_vm;                                                  /* ★ 蓄力槽：白段（= 蓄力比例）*/
        for (ih = 0; ih < POFV_HUD_CHG_TICKS; ++ih) hs[nh++] = &s_hud_chg_tick_vm[ih];   /* ★ 蓄力槽：C1..C4 四枚菱形 */
        for (ih = 0; ih < POFV_HUD_CHG_TICKS; ++ih) hs[nh++] = &s_hud_chg_halo_vm[ih];   /* ★ 光晕（电平起、电平删 ⇒ 被收走就清 0 ✓）*/
        for (ih = 0; ih < nh; ++ih)
            if (*hs[ih] && !ce_anm_get_vm((uint32_t)*hs[ih])) *hs[ih] = 0;
    }
    /* ★★★★ 2026-10-08 卡 109 的**蓄力槽** HUD（owner：「照 TH19 一手」✓）—— 复刻 `aura.anm` script0 起那套
     *   （脚本 422 = 一个 **1×1** `drawRectGrad` 图元；见其头注与 `POFV_HUD_CHG_*` 处的**结构规格** ✓）。
     *   ★★★★ 2026-10-10 owner 的**结构规格**（推翻此前「两根粗条 + 大色块填充」✗）：
     *     · **一条细横条**；**蓝填充 = 灵力比例**（`s_reiryoku / POFV_REI_MAX` ✓，规格 ①）；
     *     · **白段 = 蓄力比例**（`s_aura_charge / POFV_REI_MAX` ✓，叠在蓝的左端 ✓，规格 ②）；
     *     · **4 枚小菱形标** = C1..C4（浅蓝/紫/红/黄 = **4 张底色贴图**；位置**按角色** `s_aura_kind` ✓，规格 ⑤⑥）；
     *     · **「蓄力 ≥ 标位」时的菱形光晕**（★ owner 2026-10-11 裁决：**电平**；脚本 426、加算 + `jmp` 循环、
     *       按段位色选**4 张光晕贴图** ✓）；
     *     · **无 Lv 标签 / 无星星**（两处**留口**见句柄处的 ❓ 注释 ✓，规格 ③④）。
     *   统一走 `pofv_hud_charge_bar()`（底槽 → 蓝 → 白 → 标 = 生成序 ⇒ 画序 ✓；部分死 ⇒ 整组重建 ✓，
     *   同时兜住**碰撞/复活**收走 HUD VM 那一路 ✓）。*/
    {
        /* ★★★ 2026-10-08 订正（子智能体标的 ❓C7 ✓）：一手 TH19 的**蓄力**量程是 `clamp(S+0x8c,0,200)` ✓，
         *   但**本卡**的蓄力满刻度 = `POFV_REI_MAX` = **2500**（一手量纲 ✓，2026-10-10 改 ✓）
         *   ⇒ **蓄力比例（白段）**的除数必须用**本卡自己的满值** ✓，否则白段永远只到 **50 %** ✗。
         *   ⚠ **蓝填充（灵力）**那一侧除的是 `POFV_REI_MAX`（2500 ✓，`s_reiryoku` 自己的量纲 ✓）——
         *     两个量纲不同、**不要互相借**（owner 规格 ①② 是**两个量** ✓；`s_reiryoku` 入账不动 ✓）。*/
        /* ★★ 满刻度 = 一手量纲的 `POFV_REI_MAX`（2500 ✓）—— 与灵力条**同一把尺** ✓
         *   （★ 这才看得出"灵力 = 蓄力上限"：★ 白段永远不会超过蓝段 ✓）。*/
        const float ratio_chg = (s_aura_charge > 0.0f) ? (s_aura_charge / POFV_REI_MAX) : 0.0f;
        const float ratio_rei = (s_reiryoku   > 0.0f) ? (s_reiryoku   / POFV_REI_MAX) : 0.0f;   /* ★ 灵力：除 2500 ✓ */

        if (!s_hud_chg_slot_ok) pofv_hud_chg_claim();                       /* 幂等补认领（纪元清空/认领失败）*/
        if (s_hud_chg_slot_ok && !ce_hud_slot_get(109, &s_hud_chg_slot)) s_hud_chg_slot_ok = 0;

        if (s_hud_chg_slot_ok) {
            /* 货架槽 `(x,y)` = **块左下角**（表面 px ✓）；块高 = **24**（条 12 + 刻度出头 12 ✓）
             * ⇒ 条左上角 = 块底 − 12 = **原先单根条时的同一个位置** ✓（**本卡位置不变** ✓）；
             *    4 枚标 = **居中在块心**（4×24 的贴图、`DIA_CY` = 0 ⇒ [fy−12, fy+12] 正好 = 24 高的认领块 ✓）；
             *    ⚠ 块高 +8 只抬**左列上面那一行**的块（见 `POFV_HUD_CHG_TICK_UP` 的货架影响注释 ✓）。*/
            /* ★★❓自定（owner 2026-10-11：「**把整个槽摆在屏幕的正中**」✓）：
             *   ★ 整个槽（**底槽 452 + 蓝填充 + 白段 + 4 标 + `level n/m` 标签** ✓）**全部由 `fx` 派生**
             *   （`bx = fx + BORDER` ✓，见 `pofv_hud_charge_bar` ✓）
             *   ⇒ ★ **居中只需改这一个数** ✓ —— ★ 底槽/条/刻度/标签**一起居中** ✓（★ 不会互相错位 ✓）。
             *   ★ 原值 = `s_hud_chg_slot.x`（申领槽左沿 = 跟随货架 ✗）；★ 一手是跟随自机 ✗ ⇒ 两者都**偏离一手** ✓。*/
            /* ⚠ 2026-10-11 owner 实跑：「**它现在很靠右，画出去了**」✗ ⇒ ★ "整屏 1280 居中"这个理解**不对** ✗
             *   （★ 坐标空间不是 1280 宽 ✓）⇒ ★ 改成 **在申领到的槽内居中** ✓ —— ★ 槽本身就是"安全可画区" ✓
             *   ⇒ ★ 居中且**一定在屏内** ✓。★ 仍是 ❓自定（一手跟随自机 ✗）。*/
            /* ★★ 2026-10-11 owner 第三次修正：「**不是申领到的槽的居中，一手是游玩区居中**」✓
             *   ★ 而且这次把**空间**弄对了 ✓：★ **VM 的位置是 640×480 空间** ✓、★ 而**宽度是表面 px**
             *   （`BLOCK_W_SURF` = 452 ✓）⇒ ★ 半宽要**折半**（÷2 ✓）才是同一空间 ✓。
             *   先前用 1280/申领槽算 ⇒ 位置落在 640 空间里偏到 ~828 ⇒ ★ 就是你看到的"**画出去了**" ✓。
             *   ⇒ ★ `fx = 游玩区中心(640 空间 = 320) − 半宽(452 ÷ 2 ÷ 2 = 113)` = **207** ✓。*/
            const float fx = POFV_HUD_PLAY_CX - POFV_HUD_CHG_BLOCK_W_SURF * 0.25f;
            const float fy = s_hud_chg_slot.y - POFV_HUD_CHG_BLOCK_H_SURF;   /* 块底对齐 ✓ */
            pofv_hud_charge_bar(fx, fy, ratio_rei, ratio_chg);
        }
    }

    {   /* ★★★ TH09 SP 行 —— **owner 给的格式原文**（2026-10-08）✓：
         *   「Hit 得是**红色**的，格式不是 000000 而是**一个红色的 `<3 格右对齐、前导空格>` 数字 + `Hit`**，
         *     **后面紧跟着是六位 SP**」✓
         *   ⇒ 框内下行 = `⟨3 格红命中（前导**留空** ✗不是 0）⟩` + `Hit` + `⟨6 位 SP，紧接⟩` ✓。
         *   贴图 = 一手 TH09 `front.anm`/`front.png`：
         *     · 框 = `sprite7`（80×32「Spell Point」✓，标题在图上、下面那行空白是数字位 ✓）；
         *     · 灰数字 = `sprite8..17`（→ 我们 709–718 ✓）／**红数字 = `sprite25..34`（→ 721–730 ✓）**；
         *     · `Hit` 标签 = `sprite18`（16×10 ✓，放大 ×10 逐字确认 ✓）。
         *   几何 = 一手 `script7/8/9` 的**相对位移**（框为锚、`anchorTopLeft()` ✓）：数字起 `+5`、每位 `+8` ✓。*/
        int i9;
        if (!s_hud_sp_frame_vm)
            s_hud_sp_frame_vm = (int32_t)hud_vm_spawn(CE_ANM_ABILITY_SCRIPT_POFV109_HUD_TH09_SPELLPOINT,
                                                      POFV_HUD_SP_X, POFV_HUD_SP_Y);
        if (!s_hud_sp_hit_vm)
            s_hud_sp_hit_vm = (int32_t)hud_vm_spawn(CE_ANM_ABILITY_SCRIPT_POFV109_HUD_TH09_HIT,
                                                    POFV_HUD_SP_X + POFV_HUD_HITLABEL_DX,
                                                    POFV_HUD_SP_Y + POFV_HUD_HITLABEL_DY);

        /* ① **红色命中数**：固定 3 格、右对齐、**前导留空** ✓（owner：不是 000000 ✗）*/
        {
            int hv = s_hits.latch;
            int nd = 1;                       /* 实际位数（≥1 ✓）*/
            if (hv < 0) hv = 0;
            if (hv > 999) hv = 999;
            {   int t = hv; while (t >= 10) { t /= 10; ++nd; } }
            for (i9 = 0; i9 < POFV_HUD_HIT_SLOTS; ++i9) {
                int slot = POFV_HUD_HIT_SLOTS - 1 - i9;      /* i9=0 ⇒ 最左格 ✓ */
                float x = POFV_HUD_SP_X + POFV_HUD_HIT_X + (float)i9 * POFV_HUD_SP_STEP;
                int digit = (slot < nd) ? (hv / (int)ipow10(slot)) % 10 : 0;
                int blank = (slot >= nd);                    /* ★ 前导空格 ✓ */
                if (!s_hud_hit_digit_vm[i9])
                    s_hud_hit_digit_vm[i9] = (int32_t)hud_vm_spawn(CE_ANM_ABILITY_SCRIPT_POFV109_HUD_TH09_SP,
                                                                   x, POFV_HUD_SP_Y + POFV_HUD_SP_DY);
                if (s_hud_hit_digit_vm[i9]) {
                    /* ★★★ 2026-10-08 owner 的配方（原话）：「Hit 用的是 `front.png` 里的**第一行数字 + Hit**，
                     *   然后采用 **`color(red)` 把整体的颜色乘上红色，把中间的白色变成红色**，
                     *   从而出现**外面黑色描边**」✓ ——
                     *   ⇒ 用**第一行的灰数字**（`front.anm` sprite8..17 → 我们 709–718 ✓，白芯 + 黑描边 ✓）
                     *     再 `ce_anm_set_color(红)` **乘色**：白 × 红 = **红** ✓、黑 × 红 = **黑**（描边保留 ✓）✓。
                     *   （先前我用的是 `sprite25..34` 那套**红数字** ✗ —— 字形不同，owner 不用它 ✓。）
                     *   ★ **颜色值也照一手**：原作 `front.anm` **`script9`**（数字行第一支，现 dump ✓）逐字是
                     *     `sprite(sprite8); anchorTopLeft(); pos(-139, 13); **color(192, 128, 128)**;
                     *      alpha(0); +60: alphaTime(60, 0, 255);` ⇒ 乘色 = **(192,128,128) 柔红** ✓
                     *     —— **不是纯红 (255,0,0)** ✗（我上一版自己挑了个纯红 ✗，owner 纠：「不是高级，是一手」✓）。
                     *   ⇒ 本处 AARRGGBB = **`0xFFC08080`** ✓（A=255 / R=192 / G=128 / B=128 ✓）。*/
                    ce_anm_set_sprite(CE_ABILITY_ANM(), (uint32_t)s_hud_hit_digit_vm[i9],
                                      CE_ANM_ABILITY_SPRITE_TH09_SP_0 + (uint32_t)digit);   /* ★ 第一行灰数字 ✓ */
                    /* 前导格**留空**（alpha 0 ⇒ 不画 ✓，不是画 0 ✗）；其余 = 乘红 ✓（AARRGGBB ✓）*/
                    /* ★★★ 2026-10-08（结案 ✓）：**数字奇偶帧闪烁 + 槽 <20/消失转灰** ✓ ——
                     *   一手：闪光（奇偶帧）+ 槽 `<20` 或消失时转**灰** ✓。
                     *   ❓ 灰色的具体值与闪烁的两色未逐字给 ⇒ 取灰 `(128,128,128)`（= 一手 SP 用的灰度 ✓）
                     *     与柔红 `(192,128,128)` ↔ 白 交替 ✓，标 **❓自定**（待 owner 按观感校 ✓）。*/
                    {
                        /* ★★★ 2026-10-08 订正（owner：「变成**黑白**了，而不是以前的红**黑**」✗）：
                         *   贴图自带**黑描边** ✓，`color` 只乘**芯** ⇒ 一乘白 ⇒ **白芯 + 黑描边 = 黑白** ✗。
                         *   ⇒ **闪烁的两个色都必须保持"红"** ✓：柔红 `(192,128,128)`（= 一手原色 ✓）
                         *     ↔ 稍亮红 `(230,150,150)` ✓（仍是红系 ⇒ 描边照旧是黑 ✓）。
                         *   槽 `<20` / 消失 ⇒ **灰** `(128,128,128)` ✓（一手 ✓）。*/
                        uint32_t tint;
                        /* ★★★ 2026-10-08 订正（owner：「当没闪烁的时候，它还是**黑白的**，只有有暴风时是红的」✗）：
                         *   **"黑白"就是那个灰** `(128,128,128)` ✓ —— 灰芯 + 黑描边 = 看着像黑白 ✓；
                         *   而「有暴风时是红的」= 那一瞬 `a2c ≥ 20` ⇒ 走了红分支 ✓。
                         *   ⇒ owner 要的是**一直红黑**（以前的样子 ✓）⇒ **去掉灰分支** ✓
                         *     （一手有「槽 <20 转灰」✓，但 owner 口径优先 ✓ ⇒ 已在 TUNING 登记偏差 ✓）。*/
                        if (s_card_frames & 1) tint = POFV_HUD_COL_HIT;   /* 奇帧 ⇒ 柔红 ✓ */
                        else                   tint = 0xFFE69696u;        /* 偶帧 ⇒ 亮红 ✓（❓自定）*/
                        ce_anm_set_color((uint32_t)s_hud_hit_digit_vm[i9],
                                         blank ? 0x00000000u : tint);
                    }
                    ce_anm_set_pos((uint32_t)s_hud_hit_digit_vm[i9], x, POFV_HUD_SP_Y + POFV_HUD_SP_DY, 0.0f);
                }
            }
        }
        /* ② **6 位 SP**：**紧接在 `Hit` 之后** ✓（owner 原话 ✓）*/
        for (i9 = 0; i9 < POFV_HUD_SP_SLOTS; ++i9) {
            int digit = (s_sp / (int)ipow10(POFV_HUD_SP_SLOTS - 1 - i9)) % 10;
            if (!s_hud_sp_digit_vm[i9])
                s_hud_sp_digit_vm[i9] = (int32_t)hud_vm_spawn(CE_ANM_ABILITY_SCRIPT_POFV109_HUD_TH09_SP,
                                                              POFV_HUD_SP_X + POFV_HUD_SPNUM_DX + (float)i9 * POFV_HUD_SP_STEP,
                                                              POFV_HUD_SP_Y + POFV_HUD_SP_DY);
            if (s_hud_sp_digit_vm[i9]) {
                ce_anm_set_sprite(CE_ABILITY_ANM(), (uint32_t)s_hud_sp_digit_vm[i9],
                                  CE_ANM_ABILITY_SPRITE_TH09_SP_0 + (uint32_t)digit);   /* 同一行数字 ✓ */
                /* ★★★ owner：「SP 的那些数字也乘了一个**灰度**」✓ —— 一手 `script12..17` 逐支 `color(128,128,128)` ✓ */
                ce_anm_set_color((uint32_t)s_hud_sp_digit_vm[i9], POFV_HUD_COL_SP);
                ce_anm_set_pos((uint32_t)s_hud_sp_digit_vm[i9],
                               POFV_HUD_SP_X + POFV_HUD_SPNUM_DX + (float)i9 * POFV_HUD_SP_STEP,
                               POFV_HUD_SP_Y + POFV_HUD_SP_DY, 0.0f);
            }
        }
        if (s_hud_sp_frame_vm && !ce_anm_set_pos((uint32_t)s_hud_sp_frame_vm, POFV_HUD_SP_X, POFV_HUD_SP_Y, 0.0f))
            s_hud_sp_frame_vm = 0;   /* ★ 自愈（同上 ✓）*/
        /* ★★★ 2026-10-08 诊断（owner：「没有区别」✗ ⇒ 先确认运行时真值 ✓）：
         *   每 60 帧打一次 HUD 的**实际**位置/贴图/句柄 ⇒ 日志里就能看出卡侧到底写了什么 ✓。
         *   （这一招前几轮屡次直接定位问题 ✓；HUD 没有别的可观测面 ✗。）*/
        if (!(s_card_frames % 60)) {
            ce_log("pofv: [HUD] frame vm=%d pos=(%.1f,%.1f) | digits[0]=%d pos=(%.1f,%.1f) | hitlbl=%d pos=(%.1f,%.1f) | sp[0]=%d pos=(%.1f,%.1f)",
                   (int)s_hud_sp_frame_vm, POFV_HUD_SP_X, POFV_HUD_SP_Y,
                   (int)s_hud_hit_digit_vm[0],
                   POFV_HUD_SP_X + POFV_HUD_HIT_X,
                   POFV_HUD_SP_Y + POFV_HUD_SP_DY,
                   (int)s_hud_sp_hit_vm,
                   POFV_HUD_SP_X + POFV_HUD_HITLABEL_DX,
                   POFV_HUD_SP_Y + POFV_HUD_SP_DY,
                   (int)s_hud_sp_digit_vm[0],
                   POFV_HUD_SP_X + POFV_HUD_SPNUM_DX,
                   POFV_HUD_SP_Y + POFV_HUD_SP_DY);
        }
        if (s_hud_sp_hit_vm) {
            /* ★ owner：「第一行数字 **+ Hit**」都乘红 ✓ ⇒ 这个标签（一手 `front.anm` sprite18 = 同一行的
             *   「Hit」字形 ✓）也 `color(red)` ⇒ 白芯变红、黑描边保留 ✓。*/
            ce_anm_set_color((uint32_t)s_hud_sp_hit_vm, POFV_HUD_COL_HIT);   /* ★ 一手 `script8` ✓ */
            ce_anm_set_pos((uint32_t)s_hud_sp_hit_vm,
                           POFV_HUD_SP_X + POFV_HUD_HITLABEL_DX,
                           POFV_HUD_SP_Y + POFV_HUD_HITLABEL_DY, 0.0f);
        }
    }
    {   /* ★★★ 2026-10-08（P1 完成）：TH09 的 **`Bonus!`** 提示 —— 一手原图 ✓
         *   一手（`engine/versus/th09/01-versus-loop.md`）：**SP 满（`999990`）⇒ 提示** ✓；
         *   观感：**常态灰 `(0x80,0x80,0xd0)`**、满时**闪白 ↔ 红** ✓ ⇒ 照此在卡侧染 ✓。
         *   贴图 = th09 `ascii.anm` `sprite107`(80,248,40×8) ✓（原图自带金褐色 ⇒ **必须染色** ✓）。*/
        if (!s_hud_bonus_vm)
            s_hud_bonus_vm = (int32_t)hud_vm_spawn(CE_ANM_ABILITY_SCRIPT_POFV109_TH09_BONUS,
                                                   POFV_HUD_SP_X + 132.0f, POFV_HUD_SP_Y);
        if (s_hud_bonus_vm) {
            int full = (s_sp >= 999990);
            ce_anm_set_pos((uint32_t)s_hud_bonus_vm, POFV_HUD_SP_X + 132.0f, POFV_HUD_SP_Y, 0.0f);
            if (!full) {
                ce_anm_set_color((uint32_t)s_hud_bonus_vm, 0x00000000u);   /* 未满 ⇒ alpha 0 = 不显示 ✓（无 set_alpha，走 color 的 AARRGGBB 高位 ✓）*/
            } else {
                /* 满：闪白 ↔ 红（6 帧周期 ✓ 照本卡既有 hits 的闪法 ✓）+ 常态灰底 ✓ */
                uint32_t c;
                if ((s_card_frames / 6) % 2) c = 0xFFFF4040u;    /* 红 ✓ */
                else                          c = 0xFFFFFFFFu;    /* 白 ✓ */
                ce_anm_set_color((uint32_t)s_hud_bonus_vm, c);   /* c 已含 alpha=0xFF ✓ */
            }
        }
    }
    if (pofv_hits_visible(&s_hits)) {
        /* old ce_hud_number_set pile REMOVED (owner: "左上角的那一堆数字还在") --
         * HUD now uses TH09/TH19 original-art sprite VMs instead. */
        uint32_t col = pofv_hits_is_gray(&s_hits) ? 0xFF808080u : 0xFFFFFF00u;
   /* ★ TH09 SP（一手 §4.4 ✓；满 999990 ⇒ `Bonus!` ❓待贴图 ✓）*/
        /* ★★ TH09 **連擊槽**（一手 `01-versus-loop.md` §2.1：SP **正下方的白色长条** ✓，击破时增长、否则衰减、
         *   清空 ⇒ SP 归零 ✓）。⚠ 一手的**条怎么画未定位** 🟡（`FUN_0041cf30` 已证伪 ✗）
         *   ⇒ 本条显示层为 **❓自定**：先用**数字**把已有的 `s_acc.a2c`（TH09 連擊计数器 ✓）画在 SP 正下方 ✓，
         *     条形美术（`drawRectGrad` 类元素或 VM 的 `scale.x` ✓）留作下一步 ✓ —— 已登记 `TUNING` ✓。*/


        /* ★ 诊断（每 30 帧一条，owner 实跑「看不见 HUD」用）：照出这条路径是否走到、口是否成功
         *   （`id[0] != 0` = 数字 VM 起成功了 ✓；`lbl.id[0]` 同理 ✓）*/
        if ((s_card_frames % 30) == 0) {
            ce_log("pofv: HUD hits 绘制 latch=%d gray=%d num_vm=%u lbl_vm=%u at (%.0f,%.0f)",
                   (int)s_hits.latch, pofv_hits_is_gray(&s_hits),
                   (unsigned)s_hud_hits_num.id[0], (unsigned)s_hud_hits_lbl.id[0],
                   POFV_HUD_HITS_X, POFV_HUD_HITS_Y);
        }
        ce_hud_vm_text_set(&s_hud_hits_lbl, POFV_HUD_HITS_X + 34.0f, POFV_HUD_HITS_Y,
                           col, 1, 1.0f, "hits", "pofv109");
    } else {
        ce_hud_number_clear(&s_hud_hits_num);
        ce_hud_vm_text_set(&s_hud_hits_lbl, 0.0f, 0.0f, 0, 1, 1.0f, "", "pofv109");
    }
    /* ⚠ 2026-10-05 **已修正**：阈值判定**不能每帧跑** ✗ —— 阈值 `150 − t/2` 在 `t > 300` 后降到 0 ⇒ `a >= 0` 恒真
     *   ⇒ 每帧触发一次（实跑爆出巨量 level 1 ✗✗）。一手是**在「破坏」事件上判**（每次破坏判一次 ✓）
     *   ⇒ 判定已移到下方**破坏处**（`add50/add54` 之后）✓ */
    /* ★ 暴风效果②：延迟到达 ⇒ 到点在本侧落一个 level 1（走既有生成路 ✓）*/
    {
        int32_t at = 0, al = 0; float ax = 0.0f, ay = 0.0f;
        while (pofv_foe_incoming_tick(s_inc, POFV_FOE_INCOMING_MAX, &at, &al, &ax, &ay)) {
            ++s_cnt_arrive;                 /* ★ goal ②：**回程到达**计数（每次落地 ✓）*/
            if (s_spirit_q_n < POFV_SPIRIT_Q_MAX) {
                /* ★★ owner 口径：「光点**只提供初速，不提供出生位置**」⇒ 到达时**按顶部带随机**落位 ✓
                 *   （x = 场地宽内随机、y = 16..64 的顶部带；与 TH19 灵的出生规则一致 ✓）*/
                s_spirit_q[s_spirit_q_n][0] = ((float)(ce_rand() % 321) - 160.0f);
                s_spirit_q[s_spirit_q_n][1] = 16.0f + (float)(ce_rand() % 49);
                (void)ax; (void)ay;                      /* 光点带的位置**不用**（只用它的到达时刻 ✓）*/
                /* ★★★ 2026-10-05 owner：「你似乎也没有加生成上限」✗ ⇒ 本侧**每个**生成口都要过**分族上限门** ✓
                 *   （幽灵 ⇒ TH09 族；其余 ⇒ TH19 族 ✓；超限 ⇒ 不生成 ✓）*/
                {
                    int32_t fam = (at == POFV_SPIRIT_KIND_GHOST_TH09) ? POFV_FAM_TH09 : POFV_FAM_TH19;
                    /* ★ MODEL §1-3：TH09 是**两档**「送 25 / **落地 10**」⇒ **到达（落地）这一口用 10** ✓
                     *   （核实报告 #4：「落地 10」此前无消费者 ✗）*/
                    int32_t capL = (fam == POFV_FAM_TH09) ? POFV_TH09_LANDING_CAP
                                                           : pofv_spirit_cap_for(fam, s_card_frames / 60);
                    if (!pofv_spirit_room(pofv_spirit_live_count_fam(fam), capL)) {
                        ce_log("pofv: 回程到达但**该族已到上限** ⇒ 不生成（fam=%d live=%d）",
                               (int)fam, (int)pofv_spirit_live_count_fam(fam));
                        ++s_point_q;                 /* ★ §7-3：到达满 ⇒ **折算成计数**（不丢包 ✓，核实报告第 3 条）*/
                    continue;
                    }
                }
                s_spirit_q_kind[s_spirit_q_n] = at;
                s_spirit_q_level[s_spirit_q_n] = al;
                s_spirit_q_inc[s_spirit_q_n] = 1;   /* ★ Q4：**唯一** incoming ✓ */
                ++s_spirit_q_n;
                ce_log("pofv: 暴风效果②到达 ⇒ 本侧落 level %d（type %d）", (int)al, (int)at);
            } else {
                ++s_point_q;                 /* ★ §7-3：**队满也要折算**（不丢包 ✓，核实报告 #3）*/
                ce_log("pofv: 暴风效果②到达但**生成队列满**（%d）⇒ **折算成计数**（不丢包 ✓）", POFV_SPIRIT_Q_MAX);
            }
        }
    }
    ++s_card_frames;                     /* ★ ⑥ 比赛时长（帧 → 秒）：驱动上限分档 60/80/999 */
    {   /* ★ 暂存倒计时：到点 ⇒ 回到本侧变成動物霊（入生成队列）*/
        int z;
        for (z = 0; z < POFV_SPIRIT_STASH_MAX; ++z) {
            if (s_stash_ttl[z] <= 0) continue;
            if (--s_stash_ttl[z] > 0) continue;
            /* ⚠ 2026-10-05 owner：「如果不是一手的就别搞」⇒ **撤掉**「动物灵同屏 ≤3」那道门
             *   （那是 owner 实跑**观察**，一手未定音）。灵现在只受**一手那条**约束：每侧 60/80/999。 */
            if (s_spirit_q_n < POFV_SPIRIT_Q_MAX) {
                s_spirit_q[s_spirit_q_n][0] = s_stash[z][0];
                s_spirit_q[s_spirit_q_n][1] = s_stash[z][1];
                /* ★★★ owner 铁律：我方事件 ⇒ 进**对面模型** ✓（这条「暂存回来」是我自造的路 ✗）
                 *   ⇒ 改成进模型；本侧的灵只能来自**对面事件**的镜像 ✓ */
                pofv_foe_spawn_gated(pofv_top_x(), pofv_top_y(),
                                      s_stash_kind[z], 0);
                ce_log("pofv: 暂存到期 ⇒ **进对面模型**（kind=%d）", (int)s_stash_kind[z]);
            }
            s_stash_ttl[z] = 0;
        }
    }
    if (s_point_q > 0) {                     /* ★ W2：超限补偿（一手：改给 100 分）❓道具口径待核 */
        ce_money_pocket(1);
        ce_log("pofv: 灵超限 ⇒ 改给分（欠 %d 笔，本帧发 1）", s_point_q);
        --s_point_q;
    }
    if (s_spirit_q_n > 0) {
        uint8_t sp[CE_SPAWN_SIZE];
        const float *pp = ce_player_pos();
        int q;
        { int z; for (z = 0; z < CE_SPAWN_SIZE; ++z) sp[z] = 0; }
        /* ★★ 一手（`modkit/MECHANICS-109.md` §4「出场」）：「**场外会被引擎瞬删** ✗（owner 确认是引擎特性）
         *   ⇒ 只能在**版内顶边**出生 + 做生成动画」。
         *   ⇒ 出生点不再用死亡点：**x 夹进场内、y 压到顶边内侧**。
         *   ❓ 内侧余量 16（自定）；⚠ 入场运动（照抄 TH19 `default.ecl` 的 `Ghost*Enable`，`moveVel` op 404）
         *     还没接 —— 现在灵是「顶边原地出现」。 */
        {
            /* ★★ 2026-10-05 按 owner 的一手订正（`HANDOVER-SPIRIT-SPAWN.md` A2 第 7 轮）：
             *   **纯霊的出生位置 = 发射体的当前位置**（发射体会移动：`0x51aff3` 推 `+0x24`、
             *   `0x51b373/98/bd` 推 `+0x10` 的向量）✗ 不是随机撒点。
             *   ⇒ 本卡的「发射体」= **自机**（因为扳机就挂在你身上：停枪 ⇒ 没有灵 ✓）⇒ **出生位置取自机位置** ✓。
             *   侧 = 发射体的 `+0x98`（对手侧）；单人宿主下按 §6.1「对面 = 又回到本侧」⇒ 就落在我们这边 ✓。
             *   ❓ 一手在两侧对称时「对手侧」会翻到对面半场；单人没有对面半场 ⇒ 仍落本侧（观感近似）。 */
            float sx, sy;
            /* ★★★ 2026-10-05 owner 定案（`HANDOVER-SPIRIT-SPAWN.md` A2 + `engine/versus/th19/05-four-mechanics.md` §9.2）：
             *   **两套分开** ——
             *   · **TH19 的灵**（純霊/狼/獺/鷲）：出生 = **顶部横带随机**（`FUN_00413d10`：x 按场地宽、y 按 ≈64 的带宽）
             *     ⇒ **与击破点无关** ✓；
             *   · **TH09 的幽灵**：出生 = **载体落点**（`0x40dae5` 的参数 = 载体 `+0x18`）⇒ 本卡里「载体」= **暴风**
             *     ⇒ 用队列里带的那个位置（送它出来的那一发暴风的位置）✓。 */
            /* ★★★ 2026-10-05 owner 实跑更正：「幽灵生成位置 y 不对，应当在**最上面附近**，有生成过于靠下的」
             *   ⇒ 一手口径以 owner 的 `HANDOVER-SPIRIT-SPAWN.md` §0.10 / A2 为准：**TH09 幽灵与 TH19 同类**
             *     ⇒ 出生 = **顶部带内随机** ✓（我先前按旧读法用「载体落点 = 暴风位置」⇒ 暴风在低处时幽灵就出得低 ✗）*/
            if (0) {
            } else {
                sx = ((float)(ce_rand() % 321) - 160.0f);       /* x = 场地宽内随机（±160 ❓ 尺度）*/
                sy = 16.0f + (float)(ce_rand() % 33);           /* y = **最上面附近**（16..48 ✓，owner：别出得太低 ✓）*/
            }
            (void)s_spirit_q;  /* ⚠ 旧「载体落点」读法作废 ✗（保留队列位置只做日志 ✓）*/
            if (sx < -POFV_FIELD_HALF_W + 16.0f) sx = -POFV_FIELD_HALF_W + 16.0f;
            if (sx >  POFV_FIELD_HALF_W - 16.0f) sx =  POFV_FIELD_HALF_W - 16.0f;
            if (sy < 16.0f) sy = 16.0f;                         /* ⚠ 场外会被引擎瞬删 ⇒ 一律夹进版内顶边以下 */
            s_spirit_q[0][0] = sx; s_spirit_q[0][1] = sy;
        }
        *(float   *)(sp + CE_SPAWN_POS + 0) = s_spirit_q[0][0];
        *(float   *)(sp + CE_SPAWN_POS + 4) = s_spirit_q[0][1];
        *(float   *)(sp + CE_SPAWN_POS + 8) = 0.0f;
        *(int32_t *)(sp + CE_SPAWN_SCORE)     = 0;          /* 灵不给分（超限那 100 分是另一件事）*/
        *(int32_t *)(sp + CE_SPAWN_MAIN_DROP) = 0;          /* 纯灵：不掉道具 */
        *(int32_t *)(sp + CE_SPAWN_LIFE)      = POFV_SPIRIT_LIFE;   /* 必须 < 1000 */
        /* ★★ 目标 ③ ECL 通道：`$I0` = 状态（0 正常 / 1 沈静化 / 999 喊走）、`$I1` = kind。
         *   一手：`spawn+0x20`（`CE_SPAWN_ECL_I`）起是 `$I0..$I7`（`engine.h`）。 */
        *(int32_t *)(sp + CE_SPAWN_ECL_I)     = 0;                    /* $I0 = 正常 */
        /* ★★★ 2026-10-05 修（核实报告）：ECL 的 kind 枚举与 core **相反** ✗
         *   ECL（`spirit109.ecl.txt` 分派）= `1 獭 / 2 鹫 / 3 纯 / 4 幽灵`，**默认 = 狼**（无狼分支 ✓）；
         *   core（`pofv_core.h`）= `0 纯 / 1 狼 / 2 獭 / 3 鹫 / 4 幽灵` ⇒ **写 `$I1` 前必须转换** ✓ */
        {
            int32_t kc = s_spirit_q_kind[0];
            int32_t ke = 0;                                  /* 0 = ECL 默认 = 狼 ✓ */
            if (kc == POFV_SPIRIT_KIND_OTTER)            ke = 1;
            else if (kc == POFV_SPIRIT_KIND_EAGLE)       ke = 2;
            else if (kc == POFV_SPIRIT_KIND_PURE)        ke = 3;
            else if (kc == POFV_SPIRIT_KIND_GHOST_TH09)  ke = 4;
            *(int32_t *)(sp + CE_SPAWN_ECL_I + 4) = ke;      /* $I1 = **ECL 口径** ✓ */
            /* ★ 2026-10-06：**水獭的左右**（一手 `PLAN:226`「π/8 与 7π/8（两只，L/R）✓」）
             *   ⇒ `$I3`（`+0x2c`）给 0/1 ⇒ ECL 的獭支据此选**一个固定角度** ✓
             *   （原先「逐帧交替」会让 x 抵消 ⇒ 变成慢速直落 ✗，owner 实跑抓到 ✓）。*/
            /* ★★★ owner 2026-10-06 裁定（核实方在 `engine/versus/th19/05-four-mechanics.md` §9.12 给出的反汇编 ✓）：
             *   水獭的**方向由出生位置决定、朝内** —— 一手分派器 `FUN_0051b480` 的水獭分支（`0x51b60a`）：
             *   `0x59adf4` = `GhostOtterR` **当 `0 > 参数`**（左半场）⇒ 左半场用 **R**（向右＝向内 ✓）；
             *   否则 `0x59ae00` = `GhostOtterL` ⇒ 右半场用 **L**（向左＝向内 ✓）。**不是摇随机** ✗。
             *   ⇒ 本卡出生位置是**版顶随机 x**（`[-160,160]` ✓）⇒ 按其半场定朝向：**左半 ⇒ `$I3=0`（+22.5° 右斜）✓ / 右半 ⇒ `$I3=1`（+157.5° 左斜）✓**
             *     （读取该灵**自己的**出生 x = `sp + CE_SPAWN_POS` ✓，避免依赖局部变量名 ✓）。*/
            *(int32_t *)(sp + CE_SPAWN_ECL_I + 12) =
                (*(const float *)(sp + CE_SPAWN_POS) < 0.0f) ? 0 : 1;

            /* ★★★★ 2026-10-08（owner 给一手 §3.31 ✓ + 本轮定案）：**幽灵出生角** ——
             *   一手：出生抽一次，约 **1/5 朝自机 ±22.5°**、其余 **正下方 ±0.9°** ✓。
             *   ★★ 通道 = **ECL 的浮点全局 `%F0`**（`green_extra.ecl.txt` 的注释里就有
             *     `moveVel(%F0,%F1)` 这个一手用法 ✓）—— **`%F0` 是全局、`$F0` 是局部** ✗，
             *     我先前写成 `$F0` ⇒ thecl 往返出 `[-nanf]` ⇒ `make dist` 失败 ✗（教训 ✓）。
             *   ⇒ 卡侧写 `CE_SPAWN_ECL_F`（= 运行时的 `%F0` ✓，一手 `engine.h`：「生成时可填初值」✓），
             *     由 ECL 的 **`moveVel(%F0, 1.0f)`** 消费 ✓ —— 角度交给**引擎的初速通道** ✓。*/
            {
                float ang = 1.5707964f;                  /* 默认 = 一手三色 ECL 的基角 π/2 ✓ */
                if (kc == POFV_SPIRIT_KIND_GHOST_TH09) {
                    const float *pp = ce_player_pos();
                    pofv_ghost_spawn_angle(*(const float *)(sp + CE_SPAWN_POS),
                                           *(const float *)(sp + CE_SPAWN_POS + 4),
                                           pp ? pp[0] : 0.0f, pp ? pp[1] : 0.0f,
                                           (uint32_t)ce_rand(), (uint32_t)ce_rand(), &ang);
                }
                *(float *)(sp + CE_SPAWN_ECL_F) = ang;   /* = ECL 的 %F0 ✓ */
                /* ★★★ 2026-10-08 诊断（owner：「依旧朝右……建议你打印一下初始速度，包括其他灵的
                 *   （他们正常）」✓）：把所有灵**写进 spawn 的角度**都打出来 ⇒ 一眼看出是
                 *   「卡侧没算对」还是「ECL 没读到」✗。kind 4 = TH09 幽灵（目标档 ✓）。*/
                ce_log("pofv: [VEL] kind=%d(kc=%d) spawnF0=%.4f rad (%.1f deg)%s",
                       (int)ke, (int)kc, (double)ang, (double)(ang * 57.29578f),
                       (kc == POFV_SPIRIT_KIND_GHOST_TH09) ? "  <= 幽灵(TH09)" : "");
            }
        }
        /* ★★★ owner 2026-10-06：「**满了就别创建了**」✓ ⇒ 账本没空格就**根本不生成这只灵** ✓
         *   （30/32 格起就停 ✓，避免「没户口的灵 ⇒ 旋转/贴图不再被写 + 附属 VM 糊在别的灵上」✗）。*/
        if (!pofv_spirit_slot_free()) {
            ce_log("pofv: 灵账已满(%d) ⇒ **不创建**（owner 口径 ✓）", POFV_SPIRIT_MAX);
        } else
        {   uint8_t *e = (uint8_t *)ce_spawn_enemy("CESpirit109", sp);
            if (e) {
                uint32_t id = *(const uint32_t *)(e + CE_ENEMY_ID);
                /* ★★★★ 2026-10-08（owner：「幽灵见不到 level2/3 了，概率上讲不可能」✗ ⇒ 找到真因 ✓）：
                 *   一手（`02-ghost-and-seal.md:126-131` ✓）：**击破连锁**的闸门读的是
                 *   **被击破幽灵自己的 charge level** = `(enemy+0x3380 >> 6) & 7` ✓（bits 6–8 ✓）。
                 *   而本卡**从来没给幽灵写过这个字段** ✗ ⇒ 恒为 0 ⇒ 闸门 `gl ∈ {1,2}` 永不通过 ✗
                 *   ⇒ **level 2/3 的幽灵永远不出现** ✓（正是 owner 报的现象 ✓）。
                 *   ⇒ 生成时**照它被生成的 level 写进去** ✓（绿 = 1 ⇒ 打它 ⇒ 黄 = 2 ⇒ 打它 ⇒ 红 = 3 ✓）。*/
                {
                    int32_t lv = (s_spirit_q_level[0] > 0) ? s_spirit_q_level[0] : 1;   /* 本卡生成口用下标 0 ✓ */
                    (void)s_spirit_q_kind[0];
                    uint8_t *fb = (uint8_t *)e + 0x3380;
                    if (lv > 3) lv = 3;
                    *(int32_t *)fb = (*(int32_t *)fb & ~(7 << 6)) | (lv << 6);
                }
                /* ★★★ 2026-10-08 诊断（owner 建议 ✓）：**读回敌机侧的 ECL 浮点全局**，看 spawn 的
                 *   `+0x40` 到底有没有落进运行时的 `%F0`（`CE_ENEMY_ECL_F0` = `+0x14c8` ✓）。
                 *   同时**直写一次**（双保险：即便 spawn 通道不通，ECL 的 `moveVel(%F0,…)` 在首帧才跑 ✓，
                 *   这里写进去也来得及 ✓）。*/
                ce_log("pofv: [VEL] enemy+0x14c8(%F0) = %.4f rad (%.1f deg) | spawn.wanted = %.4f",
                       (double)*(const float *)(e + CE_ENEMY_ECL_F0),
                       (double)(*(const float *)(e + CE_ENEMY_ECL_F0) * 57.29578f),
                       (double)*(const float *)(sp + CE_SPAWN_ECL_F));
                *(float *)(e + CE_ENEMY_ECL_F0) = *(const float *)(sp + CE_SPAWN_ECL_F);
                /* ★★ 2026-10-05 收口：**先记账拿到槽**，再用它挂两台 VM ——
                 *   旧版这里**从来没调 `pofv_note_spirit()`**（账永远是空 ⇒ 圆/本体挂到 id=0 的空槽 ⇒
                 *   9 帧后被收 ⇒ 实跑「啥也看不见」；灵死时也认不出是我们的灵）。 */
                {   int sl;
                    int ring = (s_spirit_q_kind[0] == POFV_SPIRIT_KIND_PURE)
                             ? CE_ANM_ABILITY_SCRIPT_POFV109_SPIRIT_RING
                             : CE_ANM_ABILITY_SCRIPT_POFV109_SPIRIT_RING_GREEN;
                    uint32_t vmid;
                    sl = pofv_note_spirit(id, s_spirit_q_kind[0]);
                    /* ★ 出生角：直接从**生成参数**里读回（`CE_SPAWN_ECL_F` = 刚写进去的 `%F0` ✓）
                     *   —— 比跨块传变量稳（不依赖作用域 ✓）。*/
                    if (sl >= 0) s_spirit_entry_ang[sl] = *(const float *)(sp + CE_SPAWN_ECL_F);
                    /* ★★★★ 2026-10-06 owner：「灵数超限**依旧**出现 90°/黄色」✗ ⇒ 真因（我 grep 到的事实）：
                     *   全卡**只有这一处**生成口与记账调用 ✓，但 `pofv_note_spirit()` 满员时返回 **−1** ✗，
                     *   而**没有任何 `sl < 0` 防护** ✗ ⇒ 下面用 `sl` 当槽号写 `s_spirit_*[]` ⇒ **越界写** ✓
                     *   ⇒ 表现就是「黄色 VM 糊在别的灵上」＋「旋转/贴图乱」✓（= 账本静默错的**另一种**成因 ✓）。
                     *   ⇒ 先加**无害诊断** ✓（不改逻辑 ✓），并把"满员"曝光出来 ✓；下一轮据此把整块改成
                     *     「`sl < 0` ⇒ 不挂 VM（并删掉刚生成的敌机 ✓）」，彻底杜绝越界 ✓。*/
                    if (sl < 0) ce_log("pofv: **账本拒绝记账**（满 %d）⇒ sl=-1 ✗ 旧代码会越界 ✓", POFV_SPIRIT_MAX);
                    s_spirit_age[sl] = 0;
                    s_spirit_lr[sl] = (s_spirit_q_kind[0] == POFV_SPIRIT_KIND_OTTER) ? (sl & 1) : 0;
                    s_spirit_miss[sl] = 0;
                    s_spirit_level[sl] = s_spirit_q_level[0];
                    s_spirit_from_incoming[sl] = s_spirit_q_inc[0];   /* ★ Q4 落账 */
                    s_spirit_dis[sl] = 0;
                    s_spirit_dis_age[sl] = 0;
                    s_spirit_ex[sl] = (s_ex_pending > 0) ? (--s_ex_pending, 1) : 0;
                    if (s_spirit_ex[sl]) {
                        /* ★★★ 一手 ③ **己方伤害**：EX 是**敌机** ⇒ 让自机受伤走引擎自己的**敌人 hitbox**
                         *   （`zEnemyData+0x118 hitbox_size`，zFloat2 —— ExpHP 一手字段图，不是盲写）。
                         *   ⇒ 接触自机即照常受伤（引擎结算），我们不必自己写玩家伤害。 ❓ 尺寸自定 24×24。 */
                        /* ★★ 只读调查（子智能体 `2509318b` ✓）：`CE_ENEMY_DATA+0x118` 其实是**受击盒的 z 分量** ✗
                         *   （绝对 `e+0x1344` ✗），**不是攻击盒** ✗ ⇒ 已撤掉（不再污染受击盒 ✓）。
                         *   一手：EX 要致伤 ⇒ **`no_hitbox_timer = 0`**（`e+0x6334` ✓）＋
                         *   **`flags_lo` 不带 `0x22`**（`e+0x635c` ✓）—— 两者由下面/summary 那行每帧维护 ✓。*/
                        /* ⛔ 2026-10-01 晚（**owner 口径**：所有灵/使魔都**没有体术**；109 线只读报告
                         *   给了一手定名：无体术的正解 = **`no_hitbox_timer`（`e+0x6334`）= 2**，
                         *   引擎前置闸 `0x42f51c` 自跳过接触致伤）
                         *   ⇒ 原来这行 `&= ~0x22`（清 `flags_lo` bit1/bit5 让 EX **能**致伤）是**旧的自创口径**，
                         *     已按 owner 口径**删除**；EX 要致伤也一律不再开（唯一正解那条只做「关」）。
                         *   ⚠ 本改动只删这一行 + 更新注释，**不动 109 的其它逻辑**；协作登记见 `AUDIT.md` §BE。*/
                        ce_log("pofv: EX no longer enables body damage (owner 2026-10-01: all spirits have "
                               "no body attack; no_hitbox_timer is the only lever, and it only turns it OFF)");
                    }
                    s_spirit_vm[sl] = (int32_t)ce_anm_spawn(CE_ABILITY_ANM(), ring, 8);
                    /* ★★★ owner 2026-10-06：「**th09 幽灵实际上没有这个晕光，所以得去掉**」✓
                     *   ⇒ 幽灵（kind=4）**不挂灵圆** ✓：刚挂上的那台立刻收掉并把句柄清 0 ✓（不让它留在场上 ✓）。*/
                    if (s_spirit_q_kind[0] == POFV_SPIRIT_KIND_GHOST_TH09) {
                        if (s_spirit_vm[sl]) { ce_anm_delete((uint32_t)s_spirit_vm[sl]); s_spirit_vm[sl] = 0; }
                    }
                    /* ★ 一族一支脚本：**動物霊**（狼/獺/鷲）用 401（一手有 `rotateAuto(1)`），
                     *   **純霊 / TH09 幽灵**用 400（一手 script20 无 rotateAuto）✓ */
                    {
                        int bodysc = (s_spirit_q_kind[0] == POFV_SPIRIT_KIND_WOLF ||
                                      s_spirit_q_kind[0] == POFV_SPIRIT_KIND_OTTER ||
                                      s_spirit_q_kind[0] == POFV_SPIRIT_KIND_EAGLE)
                                   ? CE_ANM_ABILITY_SCRIPT_POFV109_SPIRIT_BODY_ANIMAL
                                   : CE_ANM_ABILITY_SCRIPT_POFV109_SPIRIT_BODY;
                        s_spirit_body[sl] = (int32_t)ce_anm_spawn(CE_ABILITY_ANM(), bodysc, 8);   /* ★ 与**能渲染的圆**同层（上一版：脚本不写 layer，层由卡侧给 ✓）*/
                    }
                    /* ★★★ 2026-10-05 修：这里原先把脚本号**写死成 400** ✗（我看日志时被它误导过一次 ✗）
                     *   ⇒ 改成**真值**：实际 `bodysc`（400 = 静态族 / **401 = 動物霊族**）+ 该 kind 的**首帧 sprite**
                     *     + **ECL 口径 kind**（`$I1`，`1 獭 / 2 鹫 / 3 纯 / 4 幽灵 / 0 = 默认狼` ✓）
                     *   ⇒ 下次实跑就能直接判「动物灵到底走了哪支、贴图 id 是多少」✓ */
                    ce_log("pofv: 灵 VM spawn 槽 %d：kind(core) %d level %d ⇒ 圆 script %d → %d / 本体 script %d → %d / 首帧 sprite %d / ECL kind($I1) %d（0 = 失败）",
                           sl, (int)s_spirit_q_kind[0], (int)s_spirit_q_level[0],
                           (int)ring, (int)s_spirit_vm[sl],
                           (int)((s_spirit_q_kind[0] == POFV_SPIRIT_KIND_WOLF ||
                                  s_spirit_q_kind[0] == POFV_SPIRIT_KIND_OTTER ||
                                  s_spirit_q_kind[0] == POFV_SPIRIT_KIND_EAGLE)
                                 ? CE_ANM_ABILITY_SCRIPT_POFV109_SPIRIT_BODY_ANIMAL
                                 : CE_ANM_ABILITY_SCRIPT_POFV109_SPIRIT_BODY),
                           (int)s_spirit_body[sl],
                           pofv_spirit_body_sprite(s_spirit_q_kind[0], s_spirit_q_level[0], 0, 0),
                           (int)((s_spirit_q_kind[0] == POFV_SPIRIT_KIND_OTTER) ? 1
                               : (s_spirit_q_kind[0] == POFV_SPIRIT_KIND_EAGLE) ? 2
                               : (s_spirit_q_kind[0] == POFV_SPIRIT_KIND_PURE) ? 3
                               : (s_spirit_q_kind[0] == POFV_SPIRIT_KIND_GHOST_TH09) ? 4 : 0));
                    vmid = *(const uint32_t *)(e + CE_ENEMY_DATA + 0x124);
                    ce_log("pofv: spirit(真敌机) id=%u kind=%d at (%.0f,%.0f) life %d | enemy anm VM id=%u",
                           (unsigned)id, (int)s_spirit_q_kind[0], s_spirit_q[0][0], s_spirit_q[0][1],
                           POFV_SPIRIT_LIFE, (unsigned)vmid);
                }
            } else {
                ce_log("pofv: spirit 生成失败（ce_spawn_enemy 返回 NULL）");
            }
        }
        for (q = 1; q < s_spirit_q_n; ++q) {
            s_spirit_q[q-1][0] = s_spirit_q[q][0]; s_spirit_q[q-1][1] = s_spirit_q[q][1];
            s_spirit_q_kind[q-1] = s_spirit_q_kind[q];
            s_spirit_q_level[q-1] = s_spirit_q_level[q];
            s_spirit_q_inc[q-1] = s_spirit_q_inc[q];   /* ★ Q4：移位必须带标志 ✓ */
        }
        --s_spirit_q_n;
    }
    for (i = 0; i < POFV_STORM_MAX; ++i) {
        const pofv_storm_t *bl = pofv_storm_slot(&s_storm, i);
        if (bl && bl->active) do_storm_frame(bl);
    }
    pofv_storm_tick(&s_storm);
    storm_vm_sync();
    pofv_spark_tick();   /* ★ ③ 消弹火花：占位计时器 + 到期清句柄 ✓ */
    pofv_stage_flush();  /* ★ 阶段仪表（卡死定位用 ✓）*/

    /* ★★★ 生成口 ②（owner 2026-10-08 定案）：「**固灵结界碰到敌方弹幕 ⇒ 从该弹幕位置撒 1 颗**」，
     *   **必定掉** —— ★ owner 明确「必定掉一颗」⇒ **不做概率模型** ✗
     *   （一手 `FUN_004ee9b0` 的 `param_3 == 1` 支就是无条件的；`n%5` 那套属于 `param_3 == 4`，
     *    别混）。
     *   ⚠ 放在**本帧结界半径算完之后**（结界更新在上面）：这样位置/判定用的是**同一帧**的半径
     *     （= 判定 = 绘制 ✓，一手口径）。
     *   每帧上限 = `POFV_AURA_ITEMS_PER_FRAME`（❓ 自定护栏）；被截断的**面值不丢**
     *   （`pofv_power_spawn_packed_at` 把余数摊到已撒的那几颗上）。*/
    {
        const float *ppa = ce_player_pos();
        /* ★ 2026-10-10 **逐处定类**：这里 = 「结界**碰敌弹**」= **结界**那一类 ⇒ `pofv_aura_key()` = C ∨ Shift ✓
         *   （owner：「Shift 只开结界」✓；`s_aura_r > 0` = 结界本帧确实张着 ✓，两条都要 ✓）。*/
        if (ppa && pofv_aura_key() && s_aura_r > 0.0f) {
            float ox[POFV_AURA_ITEMS_PER_FRAME], oy[POFV_AURA_ITEMS_PER_FRAME];
            int hit = aura_scan_bullets(ppa, POFV_AURA_ITEMS_PER_FRAME, ox, oy);
            if (hit > 0) {
                int32_t apts = 0;
                pofv_power_spawn_packed_at(&s_power, ox, oy, hit, POFV_POWER_PTS_AURA,
                                           POFV_AURA_ITEMS_PER_FRAME, POFV_POWER_SRC_AURA, &apts);
                s_aura_bullets += hit;
                if (hit >= POFV_AURA_ITEMS_PER_FRAME)     /* 每帧上限生效时留一行（诊断）*/
                    ce_log("pofv: aura touched %d bullets (frame cap %d, packed %d pts)",
                           hit, POFV_AURA_ITEMS_PER_FRAME, (int)apts);
            }
        }
    }

    /* ★★ 灵力道具推进（口径 5）：待发射 / 飞散 / 飘落 / 回收线 / 吸引 / 收取。
     *   **回收线** = 现读 SDK 的 live 字段（`ce_poc_height_semantic()` —— SDK 纪律：我们自己的
     *   判据读**语义高度**，不直写/直读 `player+0x47998`）⇒ 原版那张降 PoC 的卡改线时，本卡的道具
     *   跟着变（用户 2026-09-15 口径）。本层是纯逻辑 ⇒ 这个值只能由卡侧传进去。
     *   收取的面值**同时**进两本账（同一笔收入 ✓、不新开计分）：`s_ex_gauge`（攻击槽 ✓）与
     *   `s_reiryoku`（**灵力** = 蓄力上限 ✓，owner 2026-10-10 口径 —— 见 `s_reiryoku` 声明处注释 ✓）。*/
    {
        int32_t got_pts = 0;
        int got_n = pofv_power_tick(&s_power, ce_player_pos(), ce_poc_height_semantic(), &got_pts);
        /* ★★★ 2026-10-11 owner 口径（「灵力道具只有一种 = 那个透明小蓝点」✓）：
         *   ★ 灵力只认**卡侧那一类道具（kind == 1）**被拾到的颗数 ✓ ——
         *   core 已经把 `absorbed_rei` 记好了（`pofv_core.c` 的收取支 ✓）⇒ 这里取**增量** ✓。
         *   ★ 公式仍是一手：`15 × clamp(当前蓄力,0,100)/100`（每颗 ✓、cap 2500 ✓）。*/
        {
            int32_t rei_now = s_power.absorbed_rei;
            if (rei_now > s_rei_prev) pofv_rei_credit(rei_now - s_rei_prev);
            s_rei_prev = rei_now;
        }
        if (got_n > 0 && got_pts > 0) {
            const int32_t gauge0 = s_ex_gauge;
            ce_play_sound(POFV_POWER_SE_GET, ce_player_x());   /* ★ 一手：收取音 */
            s_ex_gauge += got_pts;
            if (s_ex_gauge > POFV_EX_GAUGE_MAX) s_ex_gauge = POFV_EX_GAUGE_MAX;
            /* ★★★★ 2026-10-10 owner：「**灵力 = 吸收灵力道具涨的 = 蓄力上限**」✓
             *   ⇒ **同一个收取口/音效**（上面的 `got_pts` 那几行 ✓）**顺带**喂本卡的**灵力**账
             *     `s_reiryoku` ✓（**不新开计分**：面值照旧只进 `s_ex_gauge` ✓；灵力只是一本**只进不出**的账，
             *      与 `s_ex_gauge`（会被 EX 花掉、每帧 −1 ✓）分开 —— 原因见 `s_reiryoku` 声明处注释 ✓）。
             *   满值 = `POFV_REI_MAX`（一手量纲 2500 ✓，2026-10-10 改 ✓）。*/
            /* ★★★★ 2026-10-10 owner：**照一手 `S+0x24` 的同式**（`05-four-mechanics.md` §1 ✓）——
             *   一手 = `S+0x24 += 15 × clamp(S+0x98,0,100)/100`（**每颗道具** ✓、cap 2500 ✓）。
             *   ⇒ 本卡按**颗数** `got_n` 逐颗累加同一式：
             *     （旧注已废 ✗）真正的一手公式见下面几行：`S+0x24 += (clamp(S+0x98,0,100) × 15) / 100` ✓
             *     （`got_n` = 本次收取的**颗数** = 一手那个"每颗"的等价物 ✓；
             *      面值 `got_pts` 照旧只喂 `s_ex_gauge` ✓，不重复计分 ✓）。
             *   ⚠ 本卡 `s_aura_charge` 是 **0/满 二值**（按下 C 即到 `min(满值, 灵力)` ✗无逐帧累加 ✓）
             *     ⇒ 系数是 0 或 ≤1 的**阶跃**（一手 `S+0x98` 是按帧涨的连续量 ⇒ 系数连续 🟡）
             *     —— 该差异已登记 `TUNING.md`。*/
            /* ★★★★ 2026-10-10 owner 订正（我上一版语义错了 ✗）：
             *   「**按 C 涨的是蓄力，蓄力最大值 = 灵力，一定是涨的**」✓ ⇒
             *   **灵力与蓄力无关** ✓、**每捡到一颗都涨** ✓（**不再乘"当前蓄力"** ✗ ——
             *   一手那个 `× clamp(S+0x98,0,100)/100` 的读法被 owner 否掉了 ✓）。*/
            /* ★ 一手真公式（`FUN_00520d40` case 9..0xe ✓）：
             *   `S+0x24 += (clamp(S+0x98, 0, 100) × 15) / 100` ✓。
             *   ⚠ `S+0x98` **不是** `AuraCtrlInf+0x98`（蓄力）✗，而是**玩家记录 `S` 上一个 0..100 的
             *     int「增益基数」**（与上限量 `S+0x24` **同结构** ✓）⇒ 平时 ≥1 ⇒ **绝不可能是 +0** ✓
             *     （owner：「绝对不可能是不蓄力时捡到的道具 +0」✓）。
             *   ⇒ 本卡与蓄力**完全无关**、每颗都涨 ✓；基数暂取 clamp 上限 100 ⇒ **每颗 +15** ✓。*/
            /* ★★ 2026-10-10 **一手实测定案**（owner 转来的核实 ✓，`FUN_00520d40` kind 9–14 `0x521538`）：
             *   `v = min(道具值, 100); add = (v × 15) / 100` ⇒ ★ **满档一颗 = +15** ✓、上限 **2500** ✓。
             *   ⚠ 我上一版按 owner 的"太快"砍成 **7.5** ✗ —— ★ 那是**手感猜测** ✓，★ 现已被代码推翻 ✓
             *     ⇒ ★ **恢复 15** ✓。（"太快"的真因：★ 本卡**吃到的颗数**多 ✗ —— 见 TUNING ✓。）*/
            /* ★★★ 2026-10-11 照一手订正（本卡注释里本来就写着这条 ✓，我实现时漏了）：
             *   一手 = **`S+0x24 += 15 × clamp(当前蓄力, 0, 100) / 100`**（每颗 ✓、cap 2500 ✓）
             *   ⇒ ★ 我之前写成**平铺 15 × 件数** ✗（少了"按当前蓄力比例"这一乘 ✓）
             *   ⇒ ★ 蓄力低时我给的速度比一手**快数倍** ✓ = owner 报的"涨太快"的一部分 ✓。*/
            {
                float ratio = s_aura_charge / 100.0f;
                if (ratio < 0.0f) ratio = 0.0f;
                if (ratio > 1.0f) ratio = 1.0f;
            /* ★ 2026-10-11 owner 订正（审计 B 也指出这段注释是残留 ✗）：★ 灵力**不在这里**入账 ✓
             *   —— ★ P 点与引擎 kind 9 都不进灵力 ✗；★ 入账统一在上面的
             *   `absorbed_rei` 增量处 ✓（只认**卡侧 kind == 1 的蓝点** ✓）。*/
            }
            /* 2026-10-11 仪表：★ owner「一波 20 hit 就从 C2 吸满到 C4」✗ ⇒ 打出每次进账
             *   的件数与总量 ✓（看是"件数判据错"还是"单个给太多"✓）。*/
            ce_log("pofv: [REI] got=%d gain=%.1f total=%.1f", got_n,
                   (double)(POFV_REI_GAIN_PER_ITEM * (float)got_n), (double)s_reiryoku);
            if (s_reiryoku > POFV_REI_MAX) s_reiryoku = POFV_REI_MAX;   /* ★ 一手夹到 2500 ✓ */
            /* ★ 一手：**升档**另有音（TH16 季节道具升档播 `0x3f`）—— 本卡的对照事件 = **EX 就绪**
             *   （`s_ex_gauge` 首次到 `POFV_EX_FIRE_AT`）；❓ 这只是「本卡哪个事件算升档」的适配，
             *   音本身与触发方式都是一手（登记 `TUNING.md`）。*/
            if (gauge0 < POFV_EX_FIRE_AT && s_ex_gauge >= POFV_EX_FIRE_AT)
                ce_play_sound(POFV_POWER_SE_LEVEL, ce_player_x());
            s_power_got += got_n;
            ce_log("pofv: power +%d (%d items, gauge %d/%d, 灵力 %.0f/%.0f)", (int)got_pts, got_n,
                   (int)s_ex_gauge, POFV_EX_GAUGE_MAX, (double)s_reiryoku, (double)POFV_REI_MAX);
        }
        /* ★★ 每帧同步可视 VM（**照 116 `points_tick()`**）：建（延时结束）/ 跟随 / 删
         *   （收取 / 出界 / 寿命到期的槽在 tick 里被清 `active`）⇒ 必须在 `pofv_power_tick()`
         *   **之后**调，这样同一帧就能看到「刚出生（delay 刚减到 0）」与「刚消失」两种状态。*/
        power_vm_sync();
    }

    return 0;
}

/* ★★★★ 2026-10-08：HUD 槽位的认领放**每关开场**（`on_load`，虚表 `+0x20`）—— 与 116/117 同款
 *   （`season.c:1584` 的 `hud_slot_claim` / `animal.c:4161` 的 `on_load`）：`ce_hud_slot_claim_card()`
 *   在认领前会把**关卡号推进纪元**（关卡号一变 SDK 先清空整张表 ⇒ 不重认领的话本关
 *   `ce_hud_slot_get()` 取不到槽位 ⇒ HUD 位置退回 (0,0) ✗）。
 *   ⚠ `on_load` **不是「开局一次」**（`AbilityManager__notify_cards_on_load` 每关的商店/结算都会跑）
 *     ⇒ 这里只做**幂等登记**（只认领**这条蓄力槽**），**不碰任何状态**（状态只在 `clear_card` 清 ✓）。 */
static int on_load(ce_card_t *c)
{
    (void)c;
    pofv_hud_chg_claim();
    return 0;
}

static void on_run_reset(ce_card_t *c) { clear_card(c); }
static void on_delete(ce_card_t *c)    { clear_card(c); }

/* ★★★★ 2026-10-09 固灵结界碰敌弹的**染色**落笔点 —— **`on_draw`（虚表 `+0x48`）**。
 *
 * ⚠ **为什么放 `on_draw` 而不是 `on_tick_2`**：同一个门字段（`bullet+0x28 + CE_ANM_VM_COLOR1`）
 *   **卡 111 也在写**，而 111 写在 `on_draw` 里（`signal.c:2013-2015` 明文：写 `on_tick_2` 会被
 *   115 覆盖）⇒ 本卡也放 `on_draw`，让「本卡的染色在 111 之后生效的机会最大」= 同一条 on_draw 线里
 *   **本函数只做这一件事**（没有别的绘制会在本卡内部排在它后面）。
 * ⚠ **顺序现状（❓ 未解决，已如实登记 `TUNING.md`，不声称已解决）**：卡按 id 顺序跑 ⇒ 本卡（**109**）
 *   的 `on_draw` **先**跑、**111（id=111，更大）后**跑 ⇒ 若同一颗弹同帧被两边都写，
 *   **111 的红落在本卡之后、压住本卡**（本卡无法在 111 之后再写，除非改引擎的卡遍历顺序 —— 不在本卡权限内）。
 *   ⚠ 111 的红还带机体门（只咲夜）⇒ 非咲夜机体上这条冲突不存在（本卡照染）。
 * ⚠ **帧守卫**：`on_draw` 每帧可能不止跑一次（同款先例 = 111，`signal.c` 用 `ce_sdk_frame()` 同帧去重）
 *   —— 染色带 `n` 帧计数 ⇒ 同帧重复会**多计帧**，必须去重。
 * ⚠ 本函数**同时负责还原**：即使结界已不在场（`s_aura_r <= 0`），也要每帧跑一次 `pofv_aura_tint_tick()`
 *   （它先做还原）—— 否则上一帧染过的弹会**永远留在色里**（本仓没有一手的引擎 color 插值回落）。*/
static int on_draw(ce_card_t *c)
{
    static uint32_t s_aura_tint_frame = 0xffffffffu;
    const uint32_t now = ce_sdk_frame();
    (void)c;
    if (now != s_aura_tint_frame) {
        s_aura_tint_frame = now;
        pofv_aura_tint_tick();
    }
    return 0;
}

/* ★★★ 主动卡（owner 口径）：「只有当**切换到本卡**时，按 C 才有用；作为主动卡**必须写上去的
 *   按 C 实际没有作用、即刻 return**」。`.active_recharge` 非 0 ⇒ 引擎/UI 把 109 认成主动卡
 *   （`category == 0` ⟺ `active_recharge != 0`，`tools/contract_check.py` 硬闸）；
 *   `.on_activate` 立刻 `CE_ACTIVATE_REFUSED` ⇒ `ce_sdk_c_press` 把刚装填的充能原样退回、
 *   不进释放态 ⇒ **按 C 什么都不发生** ✓。真正的蓄力（按住 C 的电平）以后由卡侧自己读键驱动。 */
static int on_activate(ce_card_t *c)
{
    /* ★★★★ 2026-10-08 owner：「但是这个 **refused 不能触发那个 refused 的音效**」✗ ⇒ 改法 ✓：
     *   原先 `return CE_ACTIVATE_REFUSED` ⇒ **引擎原生**会放「主动卡发动被拒」的音效 ✗
     *   （⚠ 注意：本卡**从来没有调过** `ce_refuse()` —— 那个 `0x10` 音是 SDK 的 `ce_refuse` 放的 ✓，
     *     我们的裸常量不走它 ✗ ⇒ 所以那声音是**引擎自己**的 ✓）。
     *   ⇒ 改成 **`return 0`**：SDK 视作「**发动成功、且立即不持续**」✓
     *     （`ce_sdk_active_tick`: `a->state = sustain ? 1 : 2` ⇒ 直接进释放态 ✓）
     *     ⇒ **引擎认为发动过了 ⇒ 不发拒绝音** ✓，而卡侧依旧**什么都不做** ✓（真正蓄力靠卡侧自己读 C 电平 ✓）。
     *   ❓副作用：发动成功 ⇒ 引擎/SDK 会走一次「释放」收尾 + 可能进 `active_recharge` 的冷却 ✓
     *     （109 的 `active_recharge` 很小 ⇒ 实测若无感就保持 ✓；有冷却感则在 TUNING 登记后调 ✓）。*/
    (void)c;
    /* ⚠⚠ 2026-10-08 **回滚**：`return 0`（"发动成功"）⇒ owner 实跑 **卡死（进不了画面）** ✗
     *   （疑似 SDK/引擎在"成功发动"后走了需要 `on_active_tick` 配对的释放态、而我们没有 ⇒ 空转 ✓）。
     *   ⇒ 先退回 `CE_ACTIVATE_REFUSED`（游戏可跑 ✓）；「去掉引擎拒绝音效」另找安全路子 ✓。*/
    return CE_ACTIVATE_REFUSED;      /* 占位：不做任何事 */
}

/* ★★★ 固灵结界的**输入**（一手 §3.5：长按 CHARGE 展开，与蓄力同时）。
 *   先例：`cards/blue_eyes.c` 用 `.on_active_tick(ce_card_t*, uint32_t elapsed)` —— 引擎在**按住**期间每帧调它
 *   ⇒ 这正是「按住 C」的入口 ✓（本卡按权威件 §1 要求注册主动回调，但**不在回调里做别的**）。
 *   ❓ 先按帧打时间戳：`held = (s_aura_stamp == s_card_frames)`（避免 tick 顺序影响判定）。 */
static int32_t s_aura_stamp;
static int32_t pofv_active_tick(ce_card_t *c, uint32_t elapsed)
{
    (void)c; (void)elapsed;
    s_aura_stamp = s_card_frames;      /* 本帧「按着 C」*/
    return 0;                          /* 回调本身不做事（真正的逻辑在 tick 里）*/
}

CE_CARD(109, .on_active_tick = pofv_active_tick,
        .active_recharge = POFV_ACTIVE_RECHARGE,
        .on_activate = on_activate,
        .ctor = on_ctor,
        .on_load = on_load,             /* ★ 2026-10-08：认领「灵力 = 蓄力槽」的 HUD 货架块（幂等 ✓）*/
        .on_enemy_death = on_enemy_death,
        .on_tick_2 = on_tick_2,
        .on_draw = on_draw,             /* ★ 2026-10-09：固灵结界碰弹染色（一手 TH19 `0x4f5130`）*/
        .on_run_reset = on_run_reset,
        .on_delete = on_delete);
