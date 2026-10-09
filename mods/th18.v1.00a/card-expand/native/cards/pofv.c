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
static float   s_aura_charge;         /* ★ Shift 蓄力（0..200 ✓ = 结界动态的唯一驱动 ✓）*/
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
/* ★★★★ 2026-10-08 owner：「TH19 一手」的**灵力 = 蓄力槽** HUD（卡 109）—— 复刻 TH19 `aura.anm`
 *   script0 起那 12 支子脚本（一手 = `local/th19.v1.10c/_ext/aura.anm`，`thanm -l 19` 逐条）：
 *     · script1..4 = 底/框：`drawRectGrad(224,4)` / `drawRectBorder(224,4)`、`pos(-113,0)`、
 *       颜色 `(0,0,64)` / `(32,96,168)` / `(224,192,140)`；
 *     · script5..12 = **8 个段位片**（每片一组「白芯 + 外发光」色，见 `POFV_HUD_CHG_SEG_COL2`）。
 *   ⇒ 本卡用**一支脚本 422**（= 一个段位片；理由见该脚本头注②「一台 VM 只能画一个图元」）
 *     起 **1 台底/框 + 8 台段位片**，每帧按比例写 `scale.x`、按段位写颜色 ✓。
 *   ★ 块尺寸取**一手值**（owner：「TH19 单侧宽度实际低于 TH18，**先按一手宽度做**」✓）：
 *     `224 × 4` = 一手 `drawRectGrad(224,4)` 的 w/h ✓（**没做**「单侧折半」❓ 见 `TUNING.md`）。 */
#define POFV_HUD_CHG_BLOCK_W    224.0f   /* ★ 一手：`aura.anm` script1..4 的矩形宽 ✓ */
#define POFV_HUD_CHG_BLOCK_H      4.0f   /* ★ 一手：同一对指令的高 ✓ */
#define POFV_HUD_CHG_SEGMENTS       8    /* ★ 一手：script5..12 共 **8** 片 ✓ */
/* ★ 每片的宽 = 满宽 ÷ 8 片 —— ❓自定**排布**：一手那 12 台的位置/缩放由 TH19 的游戏代码逐帧写
 *   （未 dump 到）⇒ 这里按「8 片等分满宽」铺开（登记 `TUNING.md`）。 */
#define POFV_HUD_CHG_SEG_W       (POFV_HUD_CHG_BLOCK_W / (float)POFV_HUD_CHG_SEGMENTS)   /* = 28 */
/* ★ 一手 `aura.anm` script5..12 的 `color2`（外发光；相邻两片同色）—— 段位色序，卡侧每帧照抄 ✓。
 *   索引 0..7 ↔ script5/6/7/8/9/10/11/12 = 蓝 蓝 紫 紫 红 红 黄 黄。 */
static const uint32_t POFV_HUD_CHG_SEG_COL2[POFV_HUD_CHG_SEGMENTS] = {
    0xFF8080FFu, 0xFF8080FFu,   /* script5/6   (128,128,255) 蓝 ✓ */
    0xFF8000FFu, 0xFF8000FFu,   /* script7/8   (128,0,255)   紫 ✓ */
    0xFFFF0000u, 0xFFFF0000u,   /* script9/10  (255,0,0)     红 ✓ */
    0xFFFFFF00u, 0xFFFFFF00u    /* script11/12 (255,255,0)   黄 ✓ */
};
/* ★ 一手 script5/7/9/11 的 `color(255,255,255)`（每片的**白芯** ✓）—— 卡侧每帧照抄 ✓。 */
#define POFV_HUD_CHG_SEG_COL1   0xFFFFFFFFu
/* ★ 一手 script1/3 的底/框色（`color2(0,0,64)` + script3 的 `color(32,96,168)` ✓）。 */
#define POFV_HUD_CHG_BASE_COL1  0xFF000040u   /* (0, 0, 64)   暗蓝 ✓ */
#define POFV_HUD_CHG_BASE_COL2  0xFF2060A8u   /* (32, 96, 168) 蓝  ✓ */
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
 *   出现：**按住蓄力键（C∨Shift ✓）满 10 帧** ✓；松手 ⇒ `interrupt(…,1)` 收掉 ✓；
 *   动画：淡入 `alphaTime(10,0,192)` ✓；标签与当前档 **绿(128,255,128)↔白 每 10 帧** ✓；
 *         上限档与斜杠 **灰 (192,192,192) 静态** ✓。
 *   档位：一手 1 格 = 100（0..400 ⇒ 0..4）✓；本卡 `s_aura_charge` 是 **0..200** ⇒ 每格取 **50**、
 *         显示上限固定 **4** ⇒ 观感与一手同形 ✓（❓自定换算，已登记 ✓）。*/
#define POFV_HUD_CHG_MAX_TIER 4
static int32_t s_hud_chg_label_vm, s_hud_chg_cur_vm, s_hud_chg_slash_vm, s_hud_chg_max_vm;
static int32_t s_hud_chg_hold;      /* 蓄力键连续按住的帧数（一手：满 10 帧才显示 ✓）*/

/* ★★★★ 2026-10-08 TH09 **自机上方「Gauge Filled / level」提示**（一手 script72–74 ✓）：
 *   3 台 VM，随自机移动、中心锚点 ✓：
 *     标签 `Gauge Filled` (0,−40) ✓／标签 `level` (−8,−28) ✓／档位数字 (+18,−28) ✓；
 *     ★ 2026-10-09：同上，一手的这三组偏移是 **640×480 空间** ⇒ 落 UI 列表时 **×2 到表面 px** ✓；
 *   出现：★ **档位上涨那一帧** ✓（一手 `FUN_0041bc90` ⇒ `0x41bd26` 调 `0x41a320` 装这 3 支脚本 ✓）；
 *   动画：淡入 `alphaTime(3,0,192)` ✓；`Gauge Filled` 灰(224,224,224)↔白、`level` 与数字 灰(160,160,160)↔白，
 *         周期 **30 帧** ✓（一手 script72/73/74 的 `ins_37(计时器, 30)` ✓ —— 与 A 组的 10 帧不同 ✓）。
 *   ❓显示时长一手没给（脚本是无限循环 ✓）⇒ 本卡取 **60 帧**（= 1 s 提示 ✓）**❓自定**，已登记 ✓。*/
#define POFV_HUD_LV_SHOW 60
static int32_t s_hud_lv_gf_vm, s_hud_lv_label_vm, s_hud_lv_digit_vm;
static int32_t s_hud_lv_left;       /* 提示剩余帧 ✓ */
static int32_t s_hud_lv_last_tier;  /* 上次看到的档位（检测上涨沿 ✓）*/

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

static int32_t s_hud_bonus_vm;   /* ★ TH09 `Bonus!`（一手原图 th09 ascii.anm sprite107 ✓）*/
static int32_t s_hud_ag_fill_vm, s_hud_ag_dot_vm[4];   /* TH19 gauge fill + 4 threshold markers (original art) */   /* TH19 Bonus label (cut from original art) */   /* ★ TH19 hits 标签（**切自原作字体** ✓ ascii.png y=56 行 ✓）*/   /* ★ TH09 連擊槽条 / TH19 攻击槽条（卡自建 VM ⇒ 必须在 clear_card 里真删 ✓ 铁律）*/
static ce_hud_vm_text_t s_hud_hits_lbl;

/* ★★★★ 2026-10-08 卡 109 的「灵力 = 蓄力槽」VM 句柄（共享脚本 422）：
 *   · `s_hud_chg_track_vm` = **底/框**（1 台；`scale.x` 固定 = 8 ⇒ 满宽 224 ✓，颜色 = 一手底/框色 ✓）；
 *   · `s_hud_chg_seg_vm[8]` = **8 个段位片**（各 28 px；`scale.x` = 该片的填充比例 0..1 ✓、颜色 = 一手段位色 ✓）。
 *   ⚠ 必须放**文件作用域**（`clear_card` 要能看到 ⇒ 否则删卡/换局泄漏 ✗，同一文件 `:1157` 的教训 ✓）。*/
static int32_t s_hud_chg_track_vm;
static int32_t s_hud_chg_seg_vm[POFV_HUD_CHG_SEGMENTS];
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
    blk.w = POFV_HUD_CHG_BLOCK_W;      /* 224（一手 `drawRectGrad(224,4)` 的宽 ✓）*/
    blk.h = POFV_HUD_CHG_BLOCK_H;      /* 4   （同一对指令的高 ✓）*/
    blk.side = CE_HUD_SIDE_LEFT;       /* 走底部货架（owner：「冲突是已知问题，就这么做」⇒ 不为避让改位置 ✓）*/
    s_hud_chg_slot_ok = ce_hud_slot_claim_card(109, &blk, &s_hud_chg_slot);
    if (!s_hud_chg_slot_logged) {
        s_hud_chg_slot_logged = 1;
        ce_log("pofv: HUD 灵力槽块 %.0fx%.0f claimed=%d at surface (%.1f, %.1f) side=%d",
               (double)blk.w, (double)blk.h, s_hud_chg_slot_ok,
               (double)s_hud_chg_slot.x, (double)s_hud_chg_slot.y, (int)blk.side);
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
static int pofv_charge_key(void)
{
    /* ⚠⚠ 2026-10-08 **血的教训**：先前这里被误写成 `pofv_charge_key()` 自调 ⇒ **无限递归** ⇒
     *   实跑**卡死（进不了画面）** ✗。原因：我批量替换 `ce_key_down(CE_VK_SHIFT)` 时把
     *   **本函数体内**那一处也换成了新函数名 ✓。⇒ **批量替换后必须复查替换点自身** ✓。*/
    return (ce_key_down(CE_VK_C) || ce_key_down(CE_VK_SHIFT)) ? 1 : 0;
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
        ((pofv_fn_bullet_cancel_t)POFV_FN_BULLET_CANCEL)(b, mode);
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
            ce_anm_set_color(id, 0xffffffffu);   /* 出生无染色（116 同：`0xffffffff`；一手也不染色）*/
            it->vm = (int32_t)id;
            power_flash_fire(i, it->x, it->y);   /* ★ 同一刻点一台几何闪光（一手 effect.anm script94）*/
        }
        if (it->vm) ce_anm_set_pos((uint32_t)it->vm, it->x, it->y, 0.0f);
    }
}

/* ============================ 清理（删卡 / 换关 / 重开） ============================ */
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
    s_power_got = 0; s_power_logged = 0; s_aura_bullets = 0;
    {   int z9; for (z9 = 0; z9 < CE_BM_BULLET_COUNT; ++z9) s_aura_bullet_done[z9] = 0; }
    /* （结界闩锁：与弹池一起清 ⇒ 删卡后不留幽灵标记；不引 `<string.h>` 的 `memset`，
     *   与别的清池循环同一写法。）*/
    s_spirit_q_n = 0;                  /* 灵（真敌机）的待生成队列 */
    s_point_q = 0;
    pofv_acc_init(&s_acc); s_acc_last_x = 0.0f; s_acc_last_y = 0.0f;
    s_combo_blasts = 0; s_sp_life = 0;   /* ★ 一手的連爆数/SP 寿命 ✓ */
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
    s_aura_charge = 0.0f; s_aura_r = 0.0f;   /* ★ Shift 蓄力随卡清零 ✓ */
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
    /* ★★★★ 2026-10-08 卡 109 的「灵力 = 蓄力槽」（脚本 422：1 台底/框 + 8 台段位片）——
     *   与上面那批同款：**卡自建 VM ⇒ 必须在 clear_card 里真删** ✓（铁律「长持续效果要交代删掉会怎样」；
     *   `on_delete` / `on_run_reset` / `on_ctor` 三路都走本函数 ✓，不依赖 tick ✓）。
     *   句柄清 0 + 认领标记复位（活着的帧下一帧会重新认领/重建 ✓；`on_delete` 那条路 tick 已停也无妨 ✓）。*/
    if (s_hud_chg_track_vm) { ce_anm_delete((uint32_t)s_hud_chg_track_vm); s_hud_chg_track_vm = 0; }
    { int i9; for (i9 = 0; i9 < POFV_HUD_CHG_SEGMENTS; ++i9)
        if (s_hud_chg_seg_vm[i9]) { ce_anm_delete((uint32_t)s_hud_chg_seg_vm[i9]); s_hud_chg_seg_vm[i9] = 0; } }
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
    /* ★★★ owner 2026-10-08 定案（生成口 ①）：**击破敌人 ⇒ 撒 1 颗灵力道具** ——
     *   位置 = **敌机位置**（`ev->x/ev->y`）、角度 **−π/2**、速度 **2.2**（= 一手 `FUN_00521d60`
     *   的实参，见 `pofv_core.h` 的 `POFV_POWER_DROP_ANGLE/_SPEED`）。
     *   ⚠ 放在 boss 早退**之前**：owner 的字面口径是「击破**敌人**」，boss 也是敌人
     *     （被删前那份同样没有 boss 门 ⇒ 这里保持同口径；boss 死了也只多这一颗，20 分）。
     *   成本 = 一次线性找空槽（≤ `POFV_POWER_MAX` 次比较）—— 清场帧 100–300 次也够便宜
     *   （被删前那份的同一个取舍）。*/
    if (pofv_power_spawn_dir(&s_power, ev->x, ev->y, POFV_POWER_PTS_KILL, POFV_POWER_SRC_KILL,
                             POFV_POWER_DROP_ANGLE, POFV_POWER_DROP_SPEED) < 0) {
        if (!s_power_logged) {
            ce_log("pofv: power pool full (%d) — kill drop lost", POFV_POWER_MAX);
            s_power_logged = 1;
        }
    }
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
                     *   `pofv_charge_key()`（`sdk.h:832`；别的卡也用 `CE_VK_SHIFT` ✓，不自己造键值 ✗）。*/
                    const int held = pofv_charge_key();
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
    if (pofv_charge_key()) {
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
        s_aura_charge = POFV_AURA_TARGET_CHARGE;               /* = 100 ✓ ⇒ 半径减半 ✓ */
    } else if (s_aura_charge > 0.0f) {
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
    {
        const float target = s_aura_charge * s_aura_k / 100.0f;    /* ★ 一手 `clamp(S+0x8c,0,200) × K / 100` ✓（目标）*/
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
        if (s_aura_charge <= 0.0f) { s_aura_anim_f = 0; s_aura_anim_cur = 1.0f; }
        else                       { ++s_aura_anim_f; }
        f = (dur > 0.0f) ? (float)s_aura_anim_f / dur : 1.0f;
        if (f > 1.0f) f = 1.0f;                                    /* ★ 走完保持在目标 ✓ */
        e = (s_aura_kind == 3) ? (f * f * f)                       /* ★ 模式 2 = f³ ✓（早苗、先慢后快）*/
                               : (1.0f - (1.0f - f) * (1.0f - f)); /* ★ 模式 4 = 1−(1−f)² ✓（灵梦/魔理沙）*/
        s_aura_anim_cur = 1.0f + (target - 1.0f) * e;              /* ★ 一手定律（逐字）✓ */
        if (s_aura_kind == 2 && s_aura_charge > 0.0f) {
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
        ce_log("pofv: **结界心跳** kind=%d charge=%.0f R=%.1f vm=%d", (int)s_aura_kind, (double)s_aura_charge, (double)s_aura_r, (int)s_aura_vm);
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
    if (s_aura_charge <= 0.0f) { s_aura_r = 0.0f; s_aura_anim_f = 0; }
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
    {
        const int held = pofv_charge_key();
        s_hud_chg_hold = held ? (s_hud_chg_hold + 1) : 0;
        if (s_hud_chg_hold >= 10 && s_aura_charge > 0.0f) {     /* 一手：满 10 帧 + 当前值≠0 ✓ */
            const float *pp = ce_player_pos();
            /* ★★★ 2026-10-09（owner：「位置十分靠左上、尺寸偏小」✗ ⇒ 坐标/偏移/缩放都缺 ×2）：
             *   自机坐标 → **表面 px**（`POFV_HUD_SURF_*` 的推导与 117 先例见那两行宏的注释 ✓）；
             *   下面每个偏移都是一手 640 空间原值 **×2** ✓。*/
            float bx = POFV_HUD_SURF_X(pp ? pp[0] : 0.0f), by = POFV_HUD_SURF_Y(pp ? pp[1] : 0.0f);
            int32_t tier = (int32_t)(s_aura_charge / 50.0f);    /* ❓自定：0..200 ⇒ 每格 50 ✓ */
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
                ce_anm_set_pos((uint32_t)s_hud_chg_max_vm, bx + 24.0f, by - 56.0f, 0.0f);
                ce_anm_set_scale((uint32_t)s_hud_chg_max_vm, 2.0f, 2.0f);     /* ★ 同上 ×2 ✓ */
                ce_anm_set_sprite(CE_ABILITY_ANM(), (uint32_t)s_hud_chg_max_vm,
                                  CE_ANM_ABILITY_SPRITE_POFV109_TH09_UI_DIGIT_0 + POFV_HUD_CHG_MAX_TIER);
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

    /* ★★★★ 2026-10-08 B 组「Gauge Filled / level」（一手 script72–74 ✓）*/
    {
        int32_t tier = (int32_t)(s_aura_charge / 50.0f);
        if (tier < 0) tier = 0;
        if (tier > POFV_HUD_CHG_MAX_TIER) tier = POFV_HUD_CHG_MAX_TIER;
        if (tier > s_hud_lv_last_tier) s_hud_lv_left = POFV_HUD_LV_SHOW;   /* ★ 上涨沿 ⇒ 提示 60 帧 ✓ */
        s_hud_lv_last_tier = tier;
        if (s_hud_lv_left > 0) {
            const float *pp = ce_player_pos();
            /* ★★★ 2026-10-09（同 A 组：owner「位置十分靠左上、尺寸偏小」✗）：自机坐标 → **表面 px**
             *   （`POFV_HUD_SURF_*` 的推导 / 117 先例见那两行宏的注释 ✓），偏移全部 **×2** ✓。*/
            float bx = POFV_HUD_SURF_X(pp ? pp[0] : 0.0f), by = POFV_HUD_SURF_Y(pp ? pp[1] : 0.0f);
            const int flash = ((s_card_frames / 30) & 1) ? 1 : 0;          /* 一手：30 帧一轮 ✓ */
            --s_hud_lv_left;
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
                ce_anm_set_color((uint32_t)s_hud_lv_gf_vm, flash ? 0xFFFFFFFFu : 0xFFE0E0E0u);
            }
            if (s_hud_lv_label_vm) {
                ce_anm_set_pos((uint32_t)s_hud_lv_label_vm, bx - 16.0f, by - 56.0f, 0.0f);
                ce_anm_set_scale((uint32_t)s_hud_lv_label_vm, 2.0f, 2.0f);   /* ★ 同上 ×2 ✓ */
                ce_anm_set_sprite(CE_ABILITY_ANM(), (uint32_t)s_hud_lv_label_vm,
                                  CE_ANM_ABILITY_SPRITE_POFV109_TH09_UI_LEVELLABEL);
                ce_anm_set_color((uint32_t)s_hud_lv_label_vm, flash ? 0xFFFFFFFFu : 0xFFA0A0A0u);
            }
            if (s_hud_lv_digit_vm) {
                ce_anm_set_pos((uint32_t)s_hud_lv_digit_vm, bx + 36.0f, by - 56.0f, 0.0f);
                ce_anm_set_scale((uint32_t)s_hud_lv_digit_vm, 2.0f, 2.0f);   /* ★ 同上 ×2 ✓ */
                ce_anm_set_sprite(CE_ABILITY_ANM(), (uint32_t)s_hud_lv_digit_vm,
                                  CE_ANM_ABILITY_SPRITE_POFV109_TH09_UI_DIGIT_0 + (uint32_t)tier);
                ce_anm_set_color((uint32_t)s_hud_lv_digit_vm, flash ? 0xFFFFFFFFu : 0xFFA0A0A0u);
            }
        } else {
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
        /* ★★★★ 2026-10-08 卡 109 的「灵力 = 蓄力槽」（脚本 422：1 台底/框 + 8 台段位片）也进这台体检 ✓
         *   （owner 要求：「把这台也加进去」✓；本块只**清 0**，重建在下面那节里 ✓ ⇒ 撞弹/复活后自愈 ✓）。
         *   ⚠ 顺带修一处**既有越界写**：本数组原声明为 `hs[16]`，而上面已经 push 了 19 个指针
         *   （3 + 4 + 3 + 6 + 3 = 19 ⇒ `hs[16..18]` 越界 ✗，写坏栈上相邻变量）⇒ 这里 16 → **32**。
         *   现在再加 9 个（1 底/框 + 8 片）= **28 ≤ 32** ✓。 */
        int32_t *hs[32]; int nh = 0, ih;
        hs[nh++] = &s_hud_c2_vm; hs[nh++] = &s_hud_sp_frame_vm; hs[nh++] = &s_hud_sp_hit_vm;
        hs[nh++] = &s_hud_chg_label_vm; hs[nh++] = &s_hud_chg_cur_vm;
        hs[nh++] = &s_hud_chg_slash_vm; hs[nh++] = &s_hud_chg_max_vm;
        hs[nh++] = &s_hud_lv_gf_vm; hs[nh++] = &s_hud_lv_label_vm; hs[nh++] = &s_hud_lv_digit_vm;
        for (ih = 0; ih < 6; ++ih) hs[nh++] = &s_hud_sp_digit_vm[ih];
        for (ih = 0; ih < 3; ++ih) hs[nh++] = &s_hud_hit_digit_vm[ih];
        hs[nh++] = &s_hud_chg_track_vm;                                                  /* ★ 灵力槽：底/框 */
        for (ih = 0; ih < POFV_HUD_CHG_SEGMENTS; ++ih) hs[nh++] = &s_hud_chg_seg_vm[ih];  /* ★ 灵力槽：8 片 */
        for (ih = 0; ih < nh; ++ih)
            if (*hs[ih] && !ce_anm_get_vm((uint32_t)*hs[ih])) *hs[ih] = 0;
    }
    /* ★★★★ 2026-10-08 卡 109 的「**灵力 = 蓄力槽**」HUD（owner：「照 TH19 一手」✓）——
     *   复刻 `aura.anm` script0 起那套（1 底/框 + 8 段位片；脚本 422 = 一个段位片，见其头注）。
     *   每帧：
     *     ① 补认领 + 取**最新**锚点（货架按 card_id 排 ⇒ 同批后来的认领会重排 ⇒ 必须每帧 `get`，
     *        与 `season.c` 的 `hud_slot_sync` 同口径 ✓）；
     *     ② `ratio = s_aura_charge / POFV_AURA_CHARGE_MAX`（= 一手 `clamp(S+0x8c,0,200)` 的比例 ✓）；
     *     ③ 底/框 `scale.x` 恒 **8**（8 片 ⇒ 满宽 224）；第 i 片 `scale.x = clamp(ratio×8 − i, 0, 1)`
     *        （该片填充比例 0..1）⇒ **整条长度 = ratio × 224 px**，即要求里的「`scale.x` = `s_aura_charge/200`」
     *        在整条上的等价量 ✓；
     *     ④ 颜色照一手那组段位色（`POFV_HUD_CHG_SEG_COL2` ✓、白芯 `POFV_HUD_CHG_SEG_COL1` ✓）。
     *   ★ 自愈：上面的体检块已把 9 个句柄都查过 ⇒ 这里发现「**部分死**」就**整组重建**
     *     （底先、片后 ⇒ 同层生成序 = 底在下、片在上 ✓），同时兜住**碰撞/复活**收走 HUD VM 那一路 ✓。 */
    {
        /* ★★★ 2026-10-08 订正（子智能体标的 ❓C7 ✓）：一手 TH19 的槽范围是 `clamp(S+0x8c,0,200)` ✓，
         *   但**本卡**的蓄力现值只到 `POFV_AURA_TARGET_CHARGE` = **100**（owner 实跑定，`pofv.c:2112` ✓）
         *   ⇒ 除数必须用**本卡自己的满值** ✓，否则条永远只到 **50 %** ✗。*/
        const float ratio = (s_aura_charge > 0.0f) ? (s_aura_charge / POFV_AURA_TARGET_CHARGE) : 0.0f;
        float bx, by;
        int i9, alive = 0, missing = 0;

        if (!s_hud_chg_slot_ok) pofv_hud_chg_claim();                       /* 幂等补认领（纪元清空/认领失败）*/
        if (s_hud_chg_slot_ok && !ce_hud_slot_get(109, &s_hud_chg_slot)) s_hud_chg_slot_ok = 0;

        if (s_hud_chg_slot_ok) {
            if (s_hud_chg_track_vm) ++alive; else ++missing;
            for (i9 = 0; i9 < POFV_HUD_CHG_SEGMENTS; ++i9) {
                if (s_hud_chg_seg_vm[i9]) ++alive; else ++missing;
            }
            if (alive > 0 && missing > 0) {                                 /* ★ 部分死 ⇒ 整组重建（保生成序）*/
                if (s_hud_chg_track_vm) { ce_anm_delete((uint32_t)s_hud_chg_track_vm); s_hud_chg_track_vm = 0; }
                for (i9 = 0; i9 < POFV_HUD_CHG_SEGMENTS; ++i9)
                    if (s_hud_chg_seg_vm[i9]) { ce_anm_delete((uint32_t)s_hud_chg_seg_vm[i9]); s_hud_chg_seg_vm[i9] = 0; }
                missing = POFV_HUD_CHG_SEGMENTS + 1;
            }
            /* 块左下角 = `s_hud_chg_slot.(x,y)`；脚本 422 用 `anchor(1,1)`（左·顶，一手口径 ✓）
             * ⇒ VM 位置取**块顶边**，条身正好压在块底边上（y−4 .. y ✓）。 */
            bx = s_hud_chg_slot.x;
            by = s_hud_chg_slot.y - POFV_HUD_CHG_BLOCK_H;
            if (missing > 0) {
                if (!s_hud_chg_track_vm)
                    s_hud_chg_track_vm = (int32_t)hud_vm_spawn(CE_ANM_ABILITY_SCRIPT_POFV109_HUD_CHARGE, bx, by);
                for (i9 = 0; i9 < POFV_HUD_CHG_SEGMENTS; ++i9)
                    if (!s_hud_chg_seg_vm[i9])
                        s_hud_chg_seg_vm[i9] = (int32_t)hud_vm_spawn(CE_ANM_ABILITY_SCRIPT_POFV109_HUD_CHARGE,
                                                                     bx + POFV_HUD_CHG_SEG_W * (float)i9, by);
            }
            /* ① 底/框：满宽（8 片）+ 一手底/框色（script1/3 的 (0,0,64) / (32,96,168) ✓）*/
            if (s_hud_chg_track_vm) {
                ce_anm_set_pos((uint32_t)s_hud_chg_track_vm, bx, by, 0.0f);
                ce_anm_set_scale((uint32_t)s_hud_chg_track_vm, (float)POFV_HUD_CHG_SEGMENTS, 1.0f);
                ce_anm_set_color((uint32_t)s_hud_chg_track_vm, POFV_HUD_CHG_BASE_COL1);
                ce_anm_set_color2((uint32_t)s_hud_chg_track_vm, POFV_HUD_CHG_BASE_COL2);
            }
            /* ② 8 个段位片：第 i 片 `scale.x` = 该片填充比例（0..1）+ 一手段位色 ✓ */
            for (i9 = 0; i9 < POFV_HUD_CHG_SEGMENTS; ++i9) {
                float seg = ratio * (float)POFV_HUD_CHG_SEGMENTS - (float)i9;
                if (seg < 0.0f) seg = 0.0f;
                if (seg > 1.0f) seg = 1.0f;
                if (!s_hud_chg_seg_vm[i9]) continue;
                ce_anm_set_pos((uint32_t)s_hud_chg_seg_vm[i9], bx + POFV_HUD_CHG_SEG_W * (float)i9, by, 0.0f);
                ce_anm_set_scale((uint32_t)s_hud_chg_seg_vm[i9], seg, 1.0f);
                ce_anm_set_color((uint32_t)s_hud_chg_seg_vm[i9], POFV_HUD_CHG_SEG_COL1);
                ce_anm_set_color2((uint32_t)s_hud_chg_seg_vm[i9], POFV_HUD_CHG_SEG_COL2[i9]);
            }
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
                        *(int32_t *)(e + 0x635c) &= ~0x22;   /* 清掉伤害闸的 bit1/bit5 ⇒ 允许体术致伤 ✓ */
                        ce_log("pofv: EX 装上 hitbox 24×24 ⇒ 接触自机会致伤（一手 ③ 己方伤害）");
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
        if (ppa && pofv_charge_key() && s_aura_r > 0.0f) {
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
     *   收取的面值进本卡的**攻击槽** `s_ex_gauge`（= TH19「灵力」在本卡的对应物；
     *   ⚠ 这是**适配**，被删前那份进的是旧的「气槽」，当前架构已没有那个量 —— 见 `TUNING.md`）。*/
    {
        int32_t got_pts = 0;
        int got_n = pofv_power_tick(&s_power, ce_player_pos(), ce_poc_height_semantic(), &got_pts);
        if (got_n > 0 && got_pts > 0) {
            const int32_t gauge0 = s_ex_gauge;
            ce_play_sound(POFV_POWER_SE_GET, ce_player_x());   /* ★ 一手：收取音 */
            s_ex_gauge += got_pts;
            if (s_ex_gauge > POFV_EX_GAUGE_MAX) s_ex_gauge = POFV_EX_GAUGE_MAX;
            /* ★ 一手：**升档**另有音（TH16 季节道具升档播 `0x3f`）—— 本卡的对照事件 = **EX 就绪**
             *   （`s_ex_gauge` 首次到 `POFV_EX_FIRE_AT`）；❓ 这只是「本卡哪个事件算升档」的适配，
             *   音本身与触发方式都是一手（登记 `TUNING.md`）。*/
            if (gauge0 < POFV_EX_FIRE_AT && s_ex_gauge >= POFV_EX_FIRE_AT)
                ce_play_sound(POFV_POWER_SE_LEVEL, ce_player_x());
            s_power_got += got_n;
            ce_log("pofv: power +%d (%d items, gauge %d/%d)", (int)got_pts, got_n,
                   (int)s_ex_gauge, POFV_EX_GAUGE_MAX);
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
