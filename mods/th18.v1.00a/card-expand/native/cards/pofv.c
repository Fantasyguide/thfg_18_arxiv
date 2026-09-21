/* 花映塚（TH09 系统卡，card-expand id 109）—— 卡侧接线。
 * 纯逻辑在 pofv_core.{h,c}（主机单测 `tests/test_pofv_core.c`）；本文件是**引擎面**：
 * 输入、消弹调用点、无敌声明、伤害源、灵魂池、生命周期清理。
 *
 * 机制与设计依据
 *   · 机制记录（用户口径一手抄录）：`mods/th18.v1.00a/card-expand/IDEAS.md` §「109 花映塚」
 *   · 白弹阈值 T = **半径 ≤ 4.0**（一手）：`docs/superpowers/specs/2026-09-11-system-card-spec-review.md` §5.4
 *   · 拦 Z / 死亡事件 / 消弹接口：同文 §11 #11 #47、§6.4；契约见 `EVENTS-CONTRACT.md`
 *   · 本卡 spec：`docs/superpowers/specs/2026-09-12-pofv-109-design.md`（数值表 / 未定项都在那）
 *
 * 键位（用户 2026-09-10 定，一手抄录）
 *   · **Shift = 低速 / 固灵**（展开吸灵领域；低速本身是原生的，本卡只加「领域」这一层）
 *   · **Z 点一下 = 发一发**（不蓄力 ⇒ 原生行循环照跑）；**长按 Z = 蓄力**
 *   · **B = QCA**（BOMB / X 键）—— ⚠ **未实装**：拦下 X 会吃掉原生炸弹，
 *     `EVENTS-CONTRACT.md` §5 的「`on_bomb_gate` 代价归属」还没拍板（spec §未定项第 1 条）
 *
 * ★ 拦 Z（用户 2026-09-12：「得拦 Z，这是花映塚的魅力」）
 *   走 SDK 的 `on_native_shot_suppress`（断点 `0x45ebfb`，与 `on_shot_set_index` 同点）：
 *   SDK 先问 set index（117 咆哮覆写 ⇒ 本帧不抑制），再问 suppress，**本帧跳过原生行循环**
 *   但保留 `0x45eca0` 起的卡广播（77 / 116 不受影响）。**绝不改 Z 输入位**（`0x4ca428` bit `0x001`）。
 *   时序：AbilityManager tick（优先级 0x16）先于 Player（0x17）⇒ `on_tick_2` 里算出的蓄力态
 *   当帧就能影响射击状态机（同 SDK.md §7 J♠ 的结论）。
 *
 * ★ 性能（review §4.1 强制）：`on_enemy_death` 在**清场帧**一次被调 100–300 次，
 *   回调里扫弹池 = O(n²) ⇒ 回调**只登记**（连爆档 + 入队 + 掉一颗灵魂，全 O(1)），
 *   真正的消弹在 `on_tick_2` 帧末按 `POFV_BLASTS_PER_FRAME` 的预算做。
 *
 * ★ 长持续效果（CLAUDE.md / METHOD.md 铁律）：本卡跨帧留下的东西 = 爆风队列 + 灵魂池 +
 *   无敌声明层的一格。三条都在 `clear_card()`（`on_run_reset` / `on_delete` 共用）里收。
 *   ⚠ 本卡当前**不建任何 ANM VM**（活化灵的视觉需要 ability.anm 新增脚本 ⇒ 属于资产改动，
 *   不在本次允许范围）⇒ `on_delete` 里没有 VM 要删；**一旦加上 VM 必须在这里删**（见 clear_card 注释）。
 */
#include "sdk.h"
#include "pofv_core.h"
#include <string.h>

/* ============================ 引擎面：engine.h / sdk.h 里没有包装的裸地址 ============================
 * 0x428e90  Bullet__cancel —— ★thiscall(弹指针; mode)，一手 review §6.4（「全 EXE 的消弹入口只有四个」
 *           其中之一）。mode 原样给被调的 `0x4290e0`：**0 = 纯删除**（`test edx,edx / je` 直接返回，
 *           什么都不撒）、**1 / 4 = 掉点道具 + `inc [mgr+0x7a41d0]`**（1 → push 9、4 → push 2，
 *           都走 `0x446f40` 撒原生道具）。「消一堆但放过某些」只能自己扫池逐弹调它。
 *           ⚠ **它不认 `+0x24` 的不可消位**（★ 2026-09-12 asm 复核：`+0x24` 只被 `0x429370` 查——
 *           `0x4293c8 cmp DWORD PTR [esi+0x24],0x0 / jne`；**`0x429250` 全函数体一次都没读 `+0x24`**，
 *           `0x4297a0` 全屏版同样不查 ⇒ 原注释「0x429250 / 0x429370 才认」是错的，只有后者认）。
 *           ⇒ 逐弹调它的**蓝圈**能消掉带 `+0x24` 的弹，而走 `0x429370` 的**红圈反而消不掉**这类弹
 *           （半径型「不可消」没有这个位，所以照旧会被红圈消掉 —— 见 `cancel_all_in_circle` 的注释）。
 * 0x658     zBullet.radius（★弹半径；弹种表 `0x4c5f90` stride `0x118`、出生时从**表项 +0x104 = 0x4c6094**
 *           拷入，一手 asm `0x425998 movss xmm0,[eax+0x4c6094]` → `0x4259a8 movss [edi+0x658],xmm0`）
 * 0x24      zBullet 的不可消位（列在这里只为说明上面那条差异，本卡**不写**它）
 * 扫池常量全部来自 engine.h：槽 0 = `CE_BM_BULLETS`、2000 槽、stride `0xfa0`、state `+0xf68`、pos `+0x638`。
 */
#define POFV_FN_BULLET_CANCEL   0x428e90
#define POFV_BULLET_RADIUS      0x658
#define POFV_BULLET_PROTECT     0x24

/* 输入位（`0x4ca428` 的掩码 = replay 录的那一份 ⇒ 卡侧读它**回放安全**；engine.h 的注释）。*/
#define POFV_INPUT_SHOT        0x001u   /* Z（= 射击）—— 只读，绝不写 */
#define POFV_INPUT_FOCUS       0x008u   /* Shift（= 低速 / 固灵）*/

/* 消弹调用的 max_count（照 Tenshi 的 0x1869f 量级；本卡两处都不想被截断）。*/
#define POFV_CANCEL_MAX        9999

/* 蓝圈的「有攻击力」：照 Remilia `0x40f478` / season.c 的矩形伤害源写法。
 * ❓ 自定（TODO(th09)：原版爆风伤害未逆到）+ 抬每帧上限（照 Remilia 写 300）。*/
#define POFV_DMG_CAP           300
#define POFV_BLAST_DMG           40
#define POFV_BLAST_DMG_LIFE       2

/* ★★ 2026-09-20 改（P1 #th09-2「一直会播放奇怪的 Extend 音效」）：
 *   原值 = `CE_SE_EXTEND`（TH18 `0x11` = `se_extend.wav`）—— 那只是"听着像叮"的 ❓自定借用，
 *   每升一级响一次残机奖励音，用户实跑听成"一直在响的奇怪 Extend 音效"。
 *   **一手（已逆出 TH09 v1.50a 的音效表与播放站点，直读 `local/th09.v1.50a/th09.exe`）**：
 *   · id 表 = `th09:0x4a1f80`（54 项 × 8 字节：`+0` = wav 名表下标、`+4` = 音量 word）、
 *     名表 = `th09:0x4a2130`（39 项）、播放器 = `th09:0x43e2f0`（id + 裸声像）/ `th09:0x43e380`（id + 世界 x）。
 *   · 蓄力 gauge = `player+0x30384`（float，每 100 帧一档：`th09:0x41fa88 imul 0x51eb851f; sar edx,5`）。
 *   · **起手那一声**：`th09:0x41fa5e push 0x2b` + `0x41fa65 call 0x43e2f0`，条件 = 「gauge == 0」
 *     ⇒ id `0x2b` = **`se_charge00.wav`**。
 *   · **每升一级那一声**：`th09:0x41fb31 push 0x32` + `0x41fb38 call 0x43e2f0`，条件 = 「
 *     `(int)gauge/100` 比上一帧大」（`th09:0x41fb15 cmp edi,eax` / `jge`）⇒ id `0x32` = **`se_chargeup.wav`**。
 *   ⇒ 本卡的「升级 → 叮一声」= 一手的**每升一级**那一声 ⇒ 取 `se_chargeup`（TH18 71 枚里**没有**这枚，
 *     走音效表扩容通道：`CE_VOICE_CHARGEUP` = `0x5a`，源 `local/th09.v1.50a/th09.dat` 里单抽，
 *     `assets/voice/derive_retail_se.py` 的 `TABLE` 有完整一手出处）。
 *   ⚠ 一手里这一格的音量是 `-900`（百分之一 dB），我们**不衰减**（`voice.js` 里 `volume_db: 0`）
 *     —— 原值太轻，听不出"叮"；这是唯一一处刻意偏离一手的量（登记 `TUNING.md` §2.0t）。
 *   ⚠ 一手还有另外两声（起手 `se_charge00`、放出去 `se_exattack` `0x2d` `th09:0x41fa5e` 附近），
 *     本卡**没接**（用户报的只是"升级音不对"）—— 要补的话各一行，出处已记在上面的表里。*/
#define POFV_SE_CHARGE_DING     CE_VOICE_CHARGEUP

/* 自爆占位（⚠ 待定）：TH18 **没有卡侧敌弹原语** ⇒ 先按 mandate 用 `ce_damage_rect` 顶替。
 * 语义差异写在 spec §未定项：「自机狙小玉」本该打向自机；ce_damage_rect 是**自机侧**伤害源（伤敌）。
 * 3 颗 = 一手；方向 = 朝自机的 **8 方向近似**（Chebyshev 归一化，**避免 libm / atan2**），
 * 距离与扩散是 ❓ 自定占位。*/
#define POFV_SD_BULLETS           3
#define POFV_SD_STEP           24.0f
#define POFV_SD_DMG             200
#define POFV_SD_DMG_LIFE          2
#define POFV_SD_SIZE            8.0f

/* ============================ 卡侧状态 ============================ */

/* 池是**文件级**：`ce_state` 一张卡只有 496 字节（`CE_STATE_BYTES` 512 − `CE_STATE_RESERVED` 16），
 * 装不下 64 颗爆风 + 32 颗灵魂 ⇒ 照 116 / 117 的先例放文件级静态池（`animal.c` 的 `s_tok`）。
 * ⚠ 代价：清理必须**完全不依赖 tick**（商店里 tick 停着）⇒ 两条删卡路都在 `clear_card()` 里清。*/
static pofv_blast_queue_t s_blast;
static pofv_spirit_pool_t s_spirit;
static int32_t            s_frame;      /* 游玩帧计数（连爆窗口的帧源；只在 on_tick_2 前进）*/
/* ★ 验证批次判据（`native/tools/verify.py` 的 «创建灵魂»）：本次段累计的**灵魂账**。
 *   与两个池同样放文件级（`ce_state` 只有 480 字节可用），在 `clear_card()` 里清。
 *   没有这行账，"击破 ⇒ 掉灵魂"在日志上**不可判读**（池不满就一声不响）。*/
static int32_t            s_spawned, s_absorbed, s_blasted, s_dropped;

typedef struct {
    ce_card_t  *card;
    pofv_state_t st;
    int32_t     prev_shot;      /* 上一帧 Z（边沿检测）*/
    int32_t     prev_focus;     /* 上一帧 Shift（固灵领域开关；日志用）*/
    pofv_invuln_t invuln;       /* CA 无敌窗口（声明层：每帧声明剩余量 ⇒ 不会被别人截短；
                                 * 状态机在 `pofv_core.c` 的 `pofv_invuln_*`，有主机单测）*/
    int32_t     spirit_logged;  /* 本次「池满丢弃」只记一行（防刷屏）*/
    int32_t     cap_logged;     /* 本次「爆风队满丢弃」只记一行 */
} pofv_card_t;

_Static_assert(sizeof(pofv_card_t) <= 496 - 16, "ce_state 用户区上限 496 字节");

static pofv_card_t *st(ce_card_t *c)
{
    pofv_card_t *s = ce_state(c, pofv_card_t);
    if (s) s->card = c;
    return s;
}

/* ============================ 消弹调用点（两条路） ============================ */

typedef void (__attribute__((thiscall)) *pofv_fn_bullet_cancel_t)(void *bullet, int mode);

/* 路 ①：**只消白弹**（蓝圈）—— 引擎没有带 exclude 的半径消弹接口（review §6.4）⇒ 自己扫池。
 * 命中判据**逐字照抄** `0x429370`：state ∈ {1,2}、dist² ≤ (弹半径 × 0.5 + R)²
 * —— 这里的「弹半径」用的是**这颗弹自己的** `+0x658`（`br * 0.5f + r`），与引擎一致；
 * T = 4.0（一手 §5.4：半径 ≤ 4.0 才算白弹）只作**筛选门**（`pofv_is_white_bullet`），
 * **不**替换判定式里的弹半径。⚠ 2026-09-12 审计订正：本注释原来写「把弹半径 × 0.5 换成 T」，
 * 与下面的代码不符（代码是对的、注释是错的）。
 * ⚠ 跳过 `+0x24` 的不可消位**不适用于这里**：本函数只按半径筛（红圈才需要「消一切」）。
 * 成本：2000 槽 × 1 次比较（早停于 max_count）——每帧最多 POFV_BLASTS_PER_FRAME 次。 */
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
        if (state != 1 && state != 2) continue;        /* 0 = 空槽；3 / 4 = 别的状态（★ 2026-09-12 asm：
                                                        * 三个消弹入口写的是 **4**，3 来自另一条路
                                                        * `0x4249be`；本卡只认 1/2，两种写法都安全）*/
        br = *(const float *)(b + POFV_BULLET_RADIUS);
        if (!pofv_is_white_bullet(br)) continue;       /* ★ 白弹阈值：0.0 的哨兵弹种在这里被跳掉 */
        dx = *(const float *)(b + CE_BULLET_POS) - pos[0];
        dy = *(const float *)(b + CE_BULLET_POS + 4) - pos[1];
        rr = br * 0.5f + r;                            /* 照 0x429370 的判定式 */
        if (dx * dx + dy * dy > rr * rr) continue;
        ((pofv_fn_bullet_cancel_t)POFV_FN_BULLET_CANCEL)(b, mode);
        if (++n >= max_count) break;
    }
    return n;
}

/* 路 ②：**圈内所有弹幕**（红圈）—— 调引擎的 `ce_cancel_radius`（`0x429370`），
 * ★ **消弹颗数用它自己的返回值**（sdk.h：调前清 `CE_BM_CANCEL_COUNTER` `0x7a41e8`、调后读 = 消掉几颗）。
 * mode=0 = 纯删除（一手「红圈消掉的白弹**直接消失**」）。
 * ⚠ 覆盖范围**不是「真的所有」**：`0x429370` 查 `bullet+0x24`（`0x4293c8 cmp [esi+0x24],0 / jne`）⇒
 *   带这个「保护位」的弹**红圈消不掉**（要消它只能自己扫池逐弹调 `0x428e90`，见路 ①）。
 *   半径型「不可消」弹（`+0x658 > 4.0`）没有这个位 ⇒ 照旧会被红圈消掉，spec 的「C4 全屏清干净」成立。
 *   一手引擎面依据见本文件头的裸地址段（2026-09-12 asm 复核）。*/
static int cancel_all_in_circle(const float *pos, float r, int max_count, int mode)
{
    return ce_cancel_radius(pos, r, max_count, mode);
}

static int pofv_cancel_circle(const float *pos, float r, int white_only, int max_count, int mode)
{
    return white_only ? cancel_white_in_circle(pos, r, max_count, mode)
                      : cancel_all_in_circle(pos, r, max_count, mode);
}

/* ============================ 效果 ============================ */

/* 无敌：走 EVENTS-CONTRACT §4 的**声明层**（各卡声明、层取最大值、只在更长时写 `player+0x47774`），
 * 不直接写计时器 —— 否则会把 107 结界 / 117 霊撃 给的长无敌截短。
 * 窗口内**每帧声明剩余帧数**（层取 max ⇒ 既不会截短别人，也不会被别人截短）。
 * ★ 2026-09-12 审计修（任务 4）：**帧数改由 `pofv_core` 的状态机算**（`pofv_invuln_*`）。
 *   旧实现在 `on_tick_2` 里写 `if (invuln_left > 0) { request(left); --left; }`，而起窗帧
 *   （下面的 `ca_invuln_grant()`）**也**声明了一次 `frames` ⇒ 起窗帧之外又多声明了 N 帧
 *   ⇒ C1 实际护住 F..F+8 = **9 帧**，比一手（spec §5.1「C1 无敌 = 8 帧」）多 1 帧。
 *   现在的契约：**声明量 > 0 的帧数 = 档位值**（起窗帧 = 第 1 帧，之后 N−1 … 1，第 N+1 帧退声明）。*/
static void ca_invuln_grant(pofv_card_t *s, int level)
{
    int32_t frames;

    if (!s) return;
    frames = pofv_invuln_grant(&s->invuln, level);   /* 起窗帧 = 第 1 帧 */
    if (frames <= 0) return;                         /* 等级非法 ⇒ 不开窗 */
    ce_invuln_request(ce_card_id(s->card), frames);
    ce_invuln_apply();
}

/* 蓝色爆风（一手：**击破妖精 / 幽灵时产生**、**有攻击力**、消掉圈内**白弹**、按连爆质量回气）。
 * ⚠ 原版「被消的白弹以同样速度投到对手场地」在单机没有对应 ⇒ 用户口径改成**转成分**：
 *   逐弹 `Bullet__cancel(b, mode = 1)`（引擎 `0x4290e0`：加分 + 掉点道具），不另造道具。 */
static void do_blast(pofv_card_t *s, const pofv_blast_t *bl)
{
    float pos[3], r;
    int32_t slots[POFV_SPIRIT_MAX];
    float sx[POFV_SPIRIT_MAX], sy[POFV_SPIRIT_MAX];
    int n, killed, i;

    if (!s || !bl) return;
    pofv_blast_radius(bl->chain, &r);
    pos[0] = bl->x; pos[1] = bl->y; pos[2] = 0.0f;

    n = pofv_cancel_circle(pos, r, /*white_only=*/1, POFV_CANCEL_MAX, POFV_BULLET_CANCEL_MODE_BLAST);
    pofv_account_blast(&s->st, n, bl->chain);

    /* 一手：蓝圈**有攻击力** ⇒ 一个覆盖圈面的矩形伤害源（照 Remilia / season.c）。*/
    ce_raise_damage_cap(POFV_DMG_CAP);
    ce_damage_rect(pos, 0.0f, POFV_BLAST_DMG_LIFE, POFV_BLAST_DMG, r * 2.0f, r * 2.0f);

    /* 一手：**普通幽灵很硬（3 枪打不掉）、只有被连爆波及时才爆** ⇒ 灵魂只在这里吃伤害。
     * 击破的幽灵自己也会产生爆风（一手：击破妖精 / 幽灵时产生蓝圈）⇒ 再入队一颗（连锁）。
     * ⚠ 位置必须在命中判定**之前**抄下来（命中即清槽，槽会被下一颗灵魂复用）。*/
    for (i = 0; i < POFV_SPIRIT_MAX; ++i) { sx[i] = s_spirit.s[i].x; sy[i] = s_spirit.s[i].y; }
    killed = pofv_spirit_blast_hit(&s_spirit, bl->x, bl->y, r, POFV_BLAST_SPIRIT_DMG,
                                   slots, POFV_SPIRIT_MAX);
    for (i = 0; i < killed && i < POFV_SPIRIT_MAX; ++i) {
        int32_t k = slots[i];
        int chain = pofv_chain_register_kill(&s->st, s_frame);
        pofv_account_kill(&s->st);
        if (!pofv_blast_enqueue(&s_blast, sx[k], sy[k], chain)) {
            if (!s->cap_logged) { ce_log("pofv: blast queue full — chain blasts dropped"); s->cap_logged = 1; }
        }
    }
    s_blasted += killed;
    if (n > 0)
        ce_log("pofv: blast chain %d r %d cancelled %d white (gauge %d, sp %d)",
               (int)bl->chain, (int)r, n, (int)s->st.gauge, (int)s->st.sp);
}

/* 红色消弹圈（一手：**使用蓄力攻击时产生**、消掉圈内**所有弹幕**（含不可消）、
 * 被消的白弹**直接消失**且**不回复气**）。*/
static void ca_effect(pofv_card_t *s, int level, int grant_invuln)
{
    const float *pp = ce_player_pos();
    float pos[3], r;
    int n;

    if (!s || !pp) return;
    pofv_ca_circle_radius(level, &r);
    pos[0] = pp[0]; pos[1] = pp[1]; pos[2] = pp[2];
    n = pofv_cancel_circle(pos, r, /*white_only=*/0, POFV_CANCEL_MAX, POFV_BULLET_CANCEL_MODE_CA);
    pofv_account_ca(&s->st, n);                    /* ★ 一手：红圈**不回复气**（只填 SP）*/
    if (grant_invuln) ca_invuln_grant(s, level);   /* 一手：CA 给无敌（C1 = 8 帧）*/
    ce_log("pofv: CA L%d r=%d cancelled %d bullets (sp %d, gauge %d)",
           level, (int)r, n, (int)s->st.sp, (int)s->st.gauge);
}

/* SP 到档（一手：10 万 / 30 万 / 50 万）→ 原版发动 **boss attack**（把 CCA 压到**对手场地**）。
 * ⚠ 单机没有对手 ⇒ **占位**：无代价发动一次最高级红圈（全屏消弹 + 转分），**不给无敌**。
 * 数值与形态待用户拍板（spec §未定项第 2 条：「CCA / Boss 攻击 → 打击 boss / 全屏攻击 / 场地滞留弹幕」）。*/
static void do_boss_attack(pofv_card_t *s)
{
    if (!s) return;
    ce_log("pofv: SP %d crossed stage %d -> boss attack (placeholder: free C%d circle)",
           (int)s->st.sp, (int)s->st.sp_stage, POFV_CA_LEVEL_MAX);
    ca_effect(s, POFV_CA_LEVEL_MAX, /*grant_invuln=*/0);
}

/* 活化灵自爆（一手：一段时间没被击破 → 自爆，生成 **3 颗自机狙小玉**）。
 * ⚠ **占位（待定）**：TH18 没有卡侧**敌弹**原语（造不出打向自机的弹），按 mandate 先用
 *   `ce_damage_rect` 顶替 —— 它是**自机侧**伤害源（伤敌，伤不到自机），与原版语义不同。
 *   方向 = 朝自机的 8 方向近似（Chebyshev 归一化，不引 libm；真狙击角待 th18 的 atan2 包装）。*/
static void spirit_selfdestruct(pofv_card_t *s, float x, float y)
{
    const float *pp = ce_player_pos();
    float dirx = 0.0f, diry = -1.0f;      /* 自机不在时朝上（保底，不崩）*/
    int i;

    if (!s) return;
    if (pp) {
        float dx = pp[0] - x, dy = pp[1] - y;
        float ax = dx < 0.0f ? -dx : dx, ay = dy < 0.0f ? -dy : dy;
        float m = ax > ay ? ax : ay;
        if (m > 0.0001f) { dirx = dx / m; diry = dy / m; }   /* 8 方向近似（不引 libm）*/
    }
    ce_raise_damage_cap(POFV_SD_DMG);
    for (i = 0; i < POFV_SD_BULLETS; ++i) {
        float d = POFV_SD_STEP * (float)(i + 1);
        float p[3];
        p[0] = x + dirx * d;
        p[1] = y + diry * d;
        p[2] = 0.0f;
        ce_damage_rect(p, 0.0f, POFV_SD_DMG_LIFE, POFV_SD_DMG, POFV_SD_SIZE, POFV_SD_SIZE);
    }
    ce_log("pofv: activated spirit self-destruct at (%d,%d) -> %d placeholder rects",
           (int)x, (int)y, POFV_SD_BULLETS);
}

/* ============================ 钩子 ============================ */

/* 敌人死亡（断点 `0x430240`，**在引擎撒主掉落之前**、链表仍挂着）。共享广播，只读。
 * 四张卡要的「击破」= `kind == CE_EDEATH_KILLED`（一手：`retaddr 0x42f3b7`，life ≤ 0 的主路）。
 * ⚠ 回调里**只做 O(1) 的事**：连爆档 + 入队 + 掉一颗灵魂（review §4.1）；
 *   不许析构敌人、不许缓存 `ev->enemy`、不改掉落表（110 才需要改）。 */
static void on_enemy_death(ce_card_t *c, const ce_enemy_death_t *ev)
{
    pofv_card_t *s = st(c);
    int chain;

    if (!s || !ev) return;
    if (ev->kind != CE_EDEATH_KILLED) return;
    if (!(ev->flags & CE_EDEATH_F_REAL)) return;

    chain = pofv_chain_register_kill(&s->st, s_frame);
    pofv_account_kill(&s->st);
    if (!pofv_blast_enqueue(&s_blast, ev->x, ev->y, chain)) {
        if (!s->cap_logged) { ce_log("pofv: blast queue full — extra blasts dropped"); s->cap_logged = 1; }
    }
    /* 一手（用户口径）：拿卡后场上像 hyz 一样**掉灵魂**（自建池，照 116 / 117 先例）。
     * ★ 两支都记账（verify.py 的 «创建灵魂» 判据就吃这行账）。*/
    if (pofv_spirit_spawn(&s_spirit, ev->x, ev->y, (int32_t)ev->id) >= 0) {
        ++s_spawned;
    } else {
        ++s_dropped;
        if (!s->spirit_logged) {
            ce_log("pofv: spirit pool full (%d) — spirit dropped", POFV_SPIRIT_MAX);
            s->spirit_logged = 1;
        }
    }
}

/* ★ 拦 Z：按住 Z 且已起码蓄到 C1 ⇒ 本帧跳过原生行循环（卡广播照跑）。
 * 还没蓄到 C1 的那几帧返回 0 ⇒ 点一下 Z 照旧发一发（原版「Z 点一下 = 发一发」）。*/
static int on_native_shot_suppress(ce_card_t *c)
{
    pofv_card_t *s = st(c);
    return s ? pofv_charge_suppress(&s->st) : 0;
}

static int on_tick_2(ce_card_t *c)
{
    pofv_card_t *s = st(c);
    int shot, focus, lvl = 0;
    int32_t budget, i, n_sd;
    int32_t sd_slots[POFV_SPIRIT_MAX];
    float sd_x[POFV_SPIRIT_MAX], sd_y[POFV_SPIRIT_MAX];
    pofv_blast_t bl;

    if (!s) return 0;
    ++s_frame;

    /* 对话中：蓄力打断、不发动（照 117 的处理）；商店里 AbilityManager 整个冻着，进不到这里。
     * 无敌窗口也在对话里冻结（引擎自己的计时器同样不走）。*/
    if (CE_GUI_MSG_ACTIVE()) {
        pofv_charge_cancel(&s->st);
        s->prev_shot = 1;   /* 当成「一直按着」⇒ 对话结束后要重新按才会开始蓄力（不误发）*/
        return 0;
    }

    /* 无敌声明：窗口内每帧声明剩余帧数（层取最大值），窗口外退掉声明。
     * ★ 任务 4：剩余量由 `pofv_invuln_tick()` 算（**先递减再声明**）⇒ 声明量 > 0 的帧数
     *   恰好 = `pofv_ca_invuln_frames(level)`（旧写法多 1 帧）。返回 0 = 窗口结束 ⇒ 退声明。*/
    {
        int32_t decl = pofv_invuln_tick(&s->invuln);
        ce_invuln_request(ce_card_id(c), decl);
        if (decl > 0) ce_invuln_apply();
    }

    shot  = (CE_INPUT_HELD() & POFV_INPUT_SHOT) != 0;
    focus = (CE_INPUT_HELD() & POFV_INPUT_FOCUS) != 0;

    if (shot && !s->prev_shot) pofv_charge_begin(&s->st);
    if (shot) {
        if (pofv_charge_hold(&s->st))                     /* 升级 →「叮」一声（一手：每升一级）*/
            ce_play_sound(POFV_SE_CHARGE_DING, ce_player_x());
    } else if (s->prev_shot) {
        if (pofv_charge_release(&s->st, &lvl)) ca_effect(s, lvl, /*grant_invuln=*/1);
        /* 返回 0 = 没蓄到 C1（点射）⇒ 什么都不做，原生那一发在本帧之前已经出去了 */
    }
    s->prev_shot = shot;
    s->prev_focus = focus;

    /* 固灵（一手：按低速键 → 展开暗色吸灵领域）：领域圆心 = 自机、半径 = POFV_DOMAIN_R。
     * 「进入领域的幽灵 → 变圆形、HP 减半、缓慢上浮 → 活化灵」全在 core 里。*/
    if (focus) {
        int32_t n_act = 0;
        const float *pp = ce_player_pos();
        if (pp && pofv_spirit_activate_in_domain(&s_spirit, pp[0], pp[1], &n_act) > 0) {
            s_absorbed += n_act;
            ce_log("pofv: guzei activated %d spirits", (int)n_act);
        }
    }

    /* 活化灵每帧推进 + 自爆。位置要在 tick 之前抄下来（tick 到点即清槽）。*/
    for (i = 0; i < POFV_SPIRIT_MAX; ++i) {
        sd_x[i] = s_spirit.s[i].x;
        sd_y[i] = s_spirit.s[i].y;
    }
    n_sd = pofv_spirit_tick(&s_spirit, sd_slots, POFV_SPIRIT_MAX);
    for (i = 0; i < n_sd && i < POFV_SPIRIT_MAX; ++i)
        spirit_selfdestruct(s, sd_x[sd_slots[i]], sd_y[sd_slots[i]]);

    /* 帧末统一消弹（review §4.1）：预算内把队列里的爆风做掉。*/
    budget = POFV_BLASTS_PER_FRAME;
    while (pofv_blast_take_budget(&s_blast, &bl, &budget)) do_blast(s, &bl);

    /* SP 到档 → boss attack（一手：10 万 / 30 万 / 50 万）。*/
    if (pofv_sp_take_attack(&s->st)) do_boss_attack(s);

    return 0;
}

/* ============================ 生命周期 ============================ */

/* 收干净：爆风队列 + 灵魂池 + 无敌声明。
 * ⚠ 删卡发生在**商店里**（倒吊人 / 原版空白卡清空整副牌），那一刻 AbilityManager 冻着、
 *   卡的 tick 全停 ⇒ 只能靠 `on_delete`（虚表 `+0x50`）收。
 * ⚠ 本卡当前**没有 ANM VM**（活化灵视觉待 ability.anm 资产）；**将来加了必须在这里删**，
 *   否则会和 116 / 117 的「VM 永久钉在屏幕上」同款翻车。*/
static void clear_card(ce_card_t *c)
{
    pofv_card_t *s = st(c);

    /* ★ 验证批次判据（`verify.py` 的 «创建灵魂»）：**清池之前**把本次段累计打出来。
     *   本函数同时服务三条路（每关重挂 / 一局重开 / 删卡）⇒ 这行是「本段账」，
     *   关卡换段也会打一次（数据更多，不是错）；全部为 0 时不出声（不刷屏）。*/
    if (s_spawned || s_absorbed || s_blasted || s_dropped)
        ce_log("pofv: run done - %d spirits spawned, %d absorbed, %d blasted, %d dropped (pool %d)",
               (int)s_spawned, (int)s_absorbed, (int)s_blasted, (int)s_dropped, POFV_SPIRIT_MAX);
    s_spawned = s_absorbed = s_blasted = s_dropped = 0;

    pofv_blast_queue_init(&s_blast);
    pofv_spirit_pool_init(&s_spirit);
    s_frame = 0;
    if (!s) return;
    pofv_state_init(&s->st);
    ce_invuln_request(ce_card_id(c), 0);   /* 退出无敌声明层（不截短别人已给的计时器）*/
    pofv_invuln_init(&s->invuln);          /* 窗口清零（声明已在上一条退掉）*/
    s->prev_shot = 0;
    s->prev_focus = 0;
    s->spirit_logged = 0;
    s->cap_logged = 0;
}

static int on_ctor(ce_card_t *c)
{
    pofv_card_t *s = st(c);
    if (!s) return 0;
    clear_card(c);                          /* 新对象 / 每关重挂：清成初始态（含文件级池）*/
    ce_log("pofv: ctor (T = %.1f white-bullet radius; charge on hold-Z, suppress at C1)", POFV_WHITE_R_MAX);
    return 0;
}

static void on_run_reset(ce_card_t *c) { clear_card(c); }
static void on_delete(ce_card_t *c)    { clear_card(c); }

CE_CARD(109,
        .ctor = on_ctor,
        .on_enemy_death = on_enemy_death,
        .on_native_shot_suppress = on_native_shot_suppress,
        .on_tick_2 = on_tick_2,
        .on_run_reset = on_run_reset,
        .on_delete = on_delete);
