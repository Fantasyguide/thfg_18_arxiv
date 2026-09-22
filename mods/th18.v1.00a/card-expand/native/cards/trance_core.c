/* trance_core.c —— 神灵庙 / 灵界（card-expand id 113）的纯逻辑实现。
 * 语义、一手出处、明确不做的部分全部写在 `trance_core.h` 的文件头（那里是唯一口径来源）。
 *
 * ★ **全整数**（`int32_t`），**一个 `float` 都没有**：
 *   i686-w64-mingw32 的 ABI 让 `float` 返回值走 x87 `st(0)`，而本仓有 `make dllx87` 门
 *   （「我们的 object 里 0 条 x87 指令」）。这条卡没有任何需要浮点的量：
 *   槽 / 奖励 / 弧长 / 计数全是整数，`astral.anm` 的脚本号是整数，最大得点也是定点整数。
 *   ⇒ 本文件与 `trance_core.h` 里都**不出现 float**（后续要加角度 / 半径之类请照
 *   `cards/animal_core.h` 的 `animal_hyper_radius(…, float *out)` 形状走出参）。
 */
#include "trance_core.h"
#include <string.h>   /* memset：只在 trance_core_selfcheck 里用（局部状态清零，无引擎依赖）*/

/* 待撒队列被丢弃的颗数（诊断）。⚠ 文件级：卡侧不必清 —— 它只增不减、只进日志。 */
int32_t trance_kill_queue_dropped;

/* ============================ 槽 ============================ */

int32_t trance_gauge_tier(int32_t gauge)
{
    int32_t tier;
    if (gauge <= 0) return 0;
    if (gauge >= TRANCE_GAUGE_MAX) return TRANCE_GAUGE_TIERS;
    tier = gauge / TRANCE_GAUGE_TIER;     /* 一手：`th13:0x43d530` 用 旧值/200 与新值/200 比 */
    if (tier > TRANCE_GAUGE_TIERS) tier = TRANCE_GAUGE_TIERS;
    return tier;
}

void trance_gauge_reset(trance_gauge_t *g)
{
    if (!g) return;
    g->gauge = 0;                          /* 一手：关卡开始写 0（`th13:0x42bbfd`）*/
    g->tier = 0;
}

/* ============================ 状态初始化 ============================
 * ⚠ `pending_frame = -1` = 「没有待处理的死亡」。卡侧**必须**在初始化时调本函数
 *   （`memset` 出来的 0 会被读成「第 0 帧报到过一次死亡」⇒ 立刻误触发救援）。
 *   `cards/trance.c` 的 `ctor` / `on_stage_start` / `trance_clear` 三条路都走它。*/
void trance_state_init(trance_state_t *s)
{
    if (!s) return;
    memset(s, 0, sizeof *s);      /* 含 `gauge`（= `trance_gauge_reset` 的效果）与相位 */
}
void trance_gauge_set(trance_gauge_t *g, int32_t v)
{
    if (!g) return;
    if (v < 0) v = 0;
    if (v > TRANCE_GAUGE_MAX) v = TRANCE_GAUGE_MAX;
    g->gauge = v;
    g->tier = trance_gauge_tier(v);
}

static trance_gauge_add_t gauge_move(trance_gauge_t *g, int32_t delta)
{
    trance_gauge_add_t out;
    int32_t before_tier, before, want;
    out.gauge = 0; out.tier = 0; out.crossed_tier = 0; out.delta = 0;
    if (!g || delta == 0) return out;

    before = g->gauge;
    before_tier = g->tier;
    want = before + delta;
    if (want < 0) want = 0;
    if (want > TRANCE_GAUGE_MAX) want = TRANCE_GAUGE_MAX;
    g->gauge = want;
    g->tier = trance_gauge_tier(want);

    out.gauge = g->gauge;
    out.tier = g->tier;
    out.delta = g->gauge - before;
    /* 跨格反馈只在**上升**时报（一手：`0x43d530` 比较新旧 /200 的商，跨一格播 0x44 + 效果 0x6b+格数）*/
    out.crossed_tier = (delta > 0 && g->tier > before_tier) ? 1 : 0;
    return out;
}

trance_gauge_add_t trance_gauge_add(trance_gauge_t *g, int32_t delta) { return gauge_move(g, delta); }
trance_gauge_add_t trance_gauge_sub(trance_gauge_t *g, int32_t delta) { return gauge_move(g, -delta); }

/* ==================== ★★ 机体特技表（2026-09-13；表头注释在 .h）====================
 *
 * 三行照一手/原版、一行（咲夜）是本仓按用户口径新立的**合成规则**（TH13 没有咲夜可选）。 */
static const trance_trait_t s_traits[] = {
    /* character, name,     drain_div, life‰, reimu_slow, sanae_bonus, marisa_buff, small_hitbox */
    { 0, "reimu",          1, 1000, 1, 0, 0, 1 },   /* 官方：灵界小神灵自动吸收 + 判定点较小 */
    { 1, "marisa",         1, 1000, 0, 0, 1, 0 },   /* 官方：灵界攻击力↑ + 移速↑（+ 道具更易回收，见 §BW）*/
    { 2, "sakuya",         2, 2000, 0, 0, 0, 0 },   /* ★ 用户口径：灵界时间 ×2 + 神灵寿命 ×2（合成规则）*/
    { 3, "sanae",          1, 1000, 0, 1, 0, 0 },   /* 官方：灵界槽恢复速度快（一手 = type1 额外 +1 槽）*/
};
#define TRANCE_TRAIT_DEFAULT 1                /* 越界角色 ⇒ 回落到"全中性"那一行（魔理沙）*/

const trance_trait_t *trance_traits(int32_t character)
{
    for (size_t i = 0; i < sizeof s_traits / sizeof s_traits[0]; ++i)
        if (s_traits[i].character == character) return &s_traits[i];
    return &s_traits[TRANCE_TRAIT_DEFAULT];
}

void trance_set_character(trance_state_t *s, int32_t character)
{
    if (!s) return;
    if (s->character == character) return;     /* 幂等：卡侧可以每帧写 */
    s->character  = character;
    s->drain_phase = 0;                        /* 换人 ⇒ 相位重来（避免"半格"余量带过去）*/
}

int32_t trance_spirit_life_frames(const trance_state_t *s)
{
    const trance_trait_t *t = trance_traits(s ? s->character : -1);
    int64_t life = (int64_t)TRANCE_SPIRIT_LIFE_FRAMES * t->spirit_life_permille / 1000;
    if (life < 1) life = 1;
    if (life > 100000) life = 100000;           /* 防呆上限（10 万帧 ≈ 27 分钟）*/
    return (int32_t)life;
}

int32_t trance_remaining_centisec(const trance_state_t *s)
{
    const trance_trait_t *t = trance_traits(s ? s->character : -1);
    int32_t gauge = s ? s->gauge.gauge : 0;
    int64_t cs;
    if (gauge < 0) gauge = 0;
    /* ★ 2026-09-15（用户："灵界剩余时间显示不一致"）：TH13 显示的是 `printf("%.2fs", 槽/60.0)`
     *   （一手 `th13:0x4059b0`–`0x4059ca`，格式串 `0x4aa3b4`、除数 `0x4ae898` = 60.0）
     *   —— 那是**四舍五入**到 2 位小数，而这里原来是截断 ⇒ 同一槽值会差一个字（例：槽=1
     *   TH13 显示 `0.02s`、截断显示 `0.01s`）⇒ 改成四舍五入（`+30`，整数运算，不引浮点）。*/
    cs = ((int64_t)gauge * t->drain_div * 100 + 30) / 60;   /* 槽 × 除数 = 剩余帧数（四舍五入到厘秒）*/
    if (cs > 1000000) cs = 1000000;
    return (int32_t)cs;
}

trance_gauge_tick_t trance_gauge_drain(trance_gauge_t *g)
{
    trance_gauge_tick_t out;
    out.gauge = 0;
    out.floor = 0;
    if (!g) return out;
    if (g->gauge > 0) {
        g->gauge -= TRANCE_GAUGE_DRAIN;    /* 一手：−1/帧（`th13:0x43d4f0`），不是一次性扣 200 */
        if (g->gauge < 0) g->gauge = 0;
    }
    g->tier = trance_gauge_tier(g->gauge);
    out.gauge = g->gauge;
    out.floor = (g->gauge < 1) ? 1 : 0;    /* 一手 `0x43d4f0`：`if (GAUGE < 1) end()` */
    return out;
}

/* ============================ 相位 / 状态机 ============================ */

/* 激活条件表（`th13:0x4057f0` 八条）。`auto_mode` 是 SPEC §9 #1 的兜底口径：
 * 「满槽自动进」= 把「按键」那一条换成「自动」（其余七条一条不少）。
 * ★ 第 8 条 = `(GAME_FLAGS & 0x30) != 0x20`（`th13:0x405859`）—— 由调用方读引擎面后填
 *   `c->game_flags_ok`（本文件不认识地址；TH18 侧那个全局的定位见 `trance_cond_t` 的注释）。*/
int trance_can_activate(const trance_state_t *s, const trance_cond_t *c)
{
    if (!s || !c) return 0;
    if (s->phase != TRANCE_PHASE_IDLE) return 0;          /* `0x4057fa` 不在灵界（也 = c->in_trance）*/
    if (c->in_trance) return 0;                           /* 调用方可能另有一份口径，两条都尊重 */
    if (c->bombing) return 0;                             /* `0x405806`–`0x405813` 没在放 B */
    if (!c->full) return 0;                               /* `0x405815` CURRENT_SPIRIT_GAUGE >= 600 */
    if (!c->enemies) return 0;                            /* `0x40582a` 场上有敌 */
    if (!c->cooldown) return 0;                           /* `0x405842` this+0x20 >= 0x1e */
    if (c->dialogue) return 0;                            /* `0x40584d` GUI->msg == 0 */
    if (!c->key && !c->auto_mode) return 0;               /* `0x405833` 输入位 0x800（auto 兜底见上）*/
    if (!c->game_flags_ok) return 0;                      /* `0x405859` (GAME_FLAGS & 0x30) != 0x20 ★第 8 条 */
    return 1;
}

static void begin_common(trance_state_t *s, int death_free)
{
    s->phase = death_free ? TRANCE_PHASE_DEATH_FREE : TRANCE_PHASE_MANUAL;
    s->ended_death_free = 0;
    s->cooldown = 0;                        /* 一手：begin 里重置 this+0x1c/0x20/0x24 计时 */
    ++s->activations;
    if (death_free) ++s->deaths_rescued;
}

int trance_begin(trance_state_t *s, int death_free, const trance_cond_t *c)
{
    int32_t gauge;
    if (!s) return 0;
    /* ★★★ 2026-09-22 **一手复核（capstone 直读 `local/th13.v1.00c/th13.exe`）—— 上一版这里读错了**：
     *   一手 `0x405a36 test byte ptr [esi+0x14],1` + `0x405a3a jne 0x405b64` ⇒ 「已在灵界」那条路
     *   落在 **`0x405b64`**，那里只有
     *     `mov ecx,[ebp-4] / xor ecx,ebp / xor eax,eax / pop esi / call __security_check_cookie / ret 4`
     *   = **返回 0（成功）且一个字节都不碰 `this+0x14`**。
     *   上一版把这条早退路读成「仍然写 `this+0x14 |= 1` 且把 bit1 设成 `param_1`」，并据此把
     *   **手动窗升格成死亡强制档**（⇒ 灵界里中弹 ⇒ 结束时补死）。它引的
     *   `0x405a4c`–`0x405a50` 其实是**另一条路**：`0x405a40 cmp [0x4be808],0xc8` / `jge 0x405a60`
     *   之后的 `or eax,-1; pop esi; …; ret 4` = **槽 < 200 ⇒ 返回 −1（失败）且什么都不改**。
     *   真正写那两个位的在 `0x405a60`–`0x405a7e`（**正常进入**那条路：`or ecx,1` 置 bit0，
     *   bit1 由 `param_1` 决定）—— 只有「本来不在灵界」时才走得到。
     *   ⇒ 一手语义 = **「已在灵界 ⇒ 成功 + 原样不动」**：手动灵界里中弹**不会**变成延时死。
     *   owner 2026-09-22 的实跑口径（th13 #6）与之相符：灵界里中弹连 hit 音都没有（闸门按设计），
     *   但**结束后不该死**。*/
    if (s->phase != TRANCE_PHASE_IDLE) {
        return 1;   /* 一手 `0x405b64`：返回 0（成功），**不改档、不重开窗、不记账** */
    }
    gauge = s->gauge.gauge;
    /* 一手 `0x405a40`：`if (CURRENT_SPIRIT_GAUGE < 200) return -1;` ⇒ 失败。
     * ★ 中弹救援那条路**也吃这个门**（一手 `SPIRIT` §7：第二次 die 时槽已扣到 0 ⇒ 必然失败）。
     *   第二版起我们**照抄**这一点 ⇒ 「槽不满就不救、照常死」正是这条门（无需另开分支）。*/
    if (gauge < TRANCE_GAUGE_MIN_ENTER) return 0;
    begin_common(s, death_free);
    if (c) {                                /* 条件表只是为了「是否允许」，进来后不用它 */
        (void)c;
    }
    return 1;
}

void trance_end(trance_state_t *s, trance_tick_t *out)
{
    int was_death_free;
    if (!s) return;
    if (s->phase == TRANCE_PHASE_IDLE) return;
    was_death_free = (s->phase == TRANCE_PHASE_DEATH_FREE);
    s->phase = TRANCE_PHASE_IDLE;
    s->ended_death_free = was_death_free;
    if (!was_death_free) {
        /* 一手 `0x405cdd`：**非死亡触发**的灵界结束再 −200（夹到 0）。
         * ★ 死亡触发那条**不补扣**：一手 `th13:0x405b80` 里 `if (!(this+0x14 & 2))` 只包住扣槽那一步，
         *   而它上面那句 `if (this+0x14 & 2) { … if (PLAYER_PTR) FUN_00444a10(); }`（`0x405c3e`）
         *   才是这一档的真动作 —— **再调一次 `Player::die`** ⇒ 那次保命是**延时**而不是免死。
         *   ⚠ 本文件**不调** `Player::die`（纯逻辑层不认识引擎）：只把「本帧结束后欠一次死亡」报进
         *     `trance_tick_t.re_die`，由 `trance_should_redie()` 交给调用方执行。一手同一形状：
         *     此时槽已被逐帧扣到 0 ⇒ 第二次 `die` 里的 `begin` 必然撞 `0x405a40` 那道门
         *     ⇒ 不会再被救 ⇒ 正常扣命。★ 但**不禁止别人救**（用户定案允许链式）。*/
        trance_gauge_sub(&s->gauge, TRANCE_GAUGE_END_PENALTY);
    }
    if (out) {
        out->ended = 1;
        out->ended_death_free = was_death_free;
        /* ★ 中弹救下的窗口结束时**欠一次死亡**（一手 `th13:0x405c3e`：`end` 里再调一次 `Player::die`）。
         *   卡侧用 `trance_should_redie()` 取走这个信号并真的去调 `th18:0x45d3a0`。
         *   ⚠ 这里**只报信号**，不阻止任何救援（用户定案允许链式）。*/
        out->re_die = was_death_free;
        out->phase = s->phase;
    }
}

trance_tick_t trance_tick(trance_state_t *s, int32_t frame)
{
    trance_tick_t out;
    int dropped;

    out.phase = TRANCE_PHASE_IDLE;
    out.started = 0;
    out.ended = 0;
    out.ended_death_free = 0;
    out.re_die = 0;
    out.drain_floor = 0;
    if (!s) return out;

    dropped = trance_chain_tick(s, frame);   /* 连锁窗口在两个相位下都要走 */
    (void)dropped;

    if (s->phase == TRANCE_PHASE_IDLE) {
        if (s->cooldown < TRANCE_COOLDOWN_FRAMES) ++s->cooldown;   /* 冷却到 30 就停（不必再涨）*/
        if (s->cooldown > TRANCE_COOLDOWN_FRAMES) s->cooldown = TRANCE_COOLDOWN_FRAMES;
        out.phase = s->phase;
        return out;
    }

    /* 灵界中：−1/帧；归零 ⇒ 收尾（一手 `0x43d4f0`：`if (GAUGE < 1) end()`）。
     * ⚠ 槽已经是 0（卡侧手工改过槽）时**也**走这条路并报 floor —— 一手同一形状（`GAUGE < 1` 恒真），
     *   所以这里没有「异常态」分支：归零判定是幂等的。*/
    {
        /* ★ 2026-09-13：**机体特技**介入这一点 —— 一手的消耗是 `−1/帧`（`th13:0x43d4f0`），
         *   而"时间系"的咲夜按用户口径要「**灵界时间延长**」⇒ 用**除数**实现：
         *   `drain_div == 2` 时每 2 帧才扣 1 ⇒ 满槽窗口 600 帧 → **1200 帧（10 s → 20 s）**。
         *   为什么用除数而不是"进灵界时给双倍槽"：槽上限是 600（HUD 三段条就按 600 画），
         *   给双倍会撑破上限；除数则**一切保持原样，只有"每格值多少帧"变了**。
         *   相位累加器 `drain_phase` 存在 state 里（不是文件级 static）⇒ 删卡/局末自动归零。*/
        const int32_t div = trance_traits(s->character)->drain_div;
        trance_gauge_tick_t gt;
        int do_drain = 1;

        if (div > 1) {
            if (++s->drain_phase >= div) s->drain_phase = 0;
            else do_drain = 0;               /* 本帧不扣（凑够 div 帧才扣一次）*/
        } else {
            s->drain_phase = 0;
        }

        if (do_drain) {
            gt = trance_gauge_drain(&s->gauge);
        } else {
            gt.gauge = s->gauge.gauge;       /* 一手 `trance_gauge_drain` 的对外形状照给 */
            gt.floor = (gt.gauge < TRANCE_GAUGE_DRAIN) ? 1 : 0;   /* 槽本来就 0 ⇒ 仍然报归零 */
        }
        out.drain_floor = gt.floor;
        if (gt.floor) {
            /* 逐帧耗尽也走 `end`：一手 `0x43d4f0` 调的就是它，而 `end` 里
             * `if (!(flags & 2)) GAUGE -= 200` 照跑（此时槽已 0 ⇒ 夹到 0，等效于没扣）。*/
            /* ★ 直接把 `out` 交给 `trance_end`：它一并填 `ended` / `ended_death_free` / **`re_die`**
             *   （`re_die` = 「中弹救下的窗口结束了 ⇒ 欠一次死亡」）。手抄这几个字段是上一版的写法，
             *   漏抄一个就是「卡侧永远收不到补死信号」⇒ 改成**单一来源**。*/
            trance_end(s, &out);
        }
    }
    out.phase = s->phase;
    return out;
}

/* 死亡强制档的条件表。**只有一条硬门**：槽 ≥ 200（一手 `0x405a40`）。
 * 一手的 `Player::die` 里确实还有 `TEST GAME_FLAGS,0x30` + `gui->msg`（`th13:0x444a14`），
 * 但那是**死亡流程自己的门**（决定「要不要试免费灵界」）；我们这条路的触发者已经过了那道门
 * （能走到 `+0x10` 就说明死亡流程在跑）⇒ 照 `begin(ptr,1)` 的语义走，不再复制第二遍。
 * ⚠ 其余条件**都要显式填**：本函数不 memset 整张表，漏填就是未初始化读（哪怕 `begin` 现在不看它）。*/
static void fill_cond_death_free(trance_cond_t *c, int32_t gauge)
{
    memset(c, 0, sizeof *c);
    c->full = (gauge >= TRANCE_GAUGE_MAX);
    c->enemies = 1;      /* 死亡强制的路**不查**「场上有敌」/ 键 / 冷却（一手 `0x444a1d` 直接 begin(1)）*/
    c->cooldown = 1;
    c->key = 1;
    c->auto_mode = 1;
    c->game_flags_ok = 1;
}

int trance_force_on_death(trance_state_t *s)
{
    trance_cond_t c;
    if (!s) return 0;
    /* ★★ **2026-09-12 第二版定案：这里没有「禁止救援」的闸。**
     *   用户原话：「**就这样，允许链式**（灵界期间攒到咆哮 / 结界开着 ⇒ 补死被救）」——
     *   113 的补死是一次**普通死亡**：引擎照常广播 `+0x10`，107/117 照常有机会救，
     *   **113 自己也能再救一次**（前提是没人认领 + 槽又攒到 ≥ 200）。
     *   ⇒ 唯一的天然上限是下面 `trance_begin` 那道 **`槽 >= 200`** 的门（一手 `th13:0x405a40`）：
     *     窗口 = 进入时的槽值帧、槽 −1/帧 ⇒ 自然结束时槽已被扣光 ⇒ 必然救不成。
     *   ⚠ 曾经写在这里的 `if (s->re_dying) return 0;`（防「自己救自己」）**已删**：
     *     它的前提（补死会立刻再触发一次救援）不成立 —— 补死要走完一整个决死窗口（8 帧）
     *     才轮到 113 的 `+0x0c`，而那时槽早已被那道门挡住。链式是**用户允许**的行为，不许堵。*/
    /* 已在一档真灵界里（手动 / 死亡强制）：`begin` 走「已在灵界」那条路 ——
     * ★ 2026-09-22 一手复核后：那条路**什么都不改**（`0x405b64` 的 `xor eax,eax; ret 4`），
     * 所以这里**照旧走 `begin` 拿它的返回值**，但语义变成：
     *   返回 1 = 「这次中弹被吃下了」（本来不在灵界 ⇒ 真开了救下窗；已经在灵界 ⇒ 原样不动 +
     *            成功，**不升格、不欠死**）；返回 0 = 槽 < 200 且本来不在灵界 ⇒ 救不成。*/
    fill_cond_death_free(&c, s->gauge.gauge);
    return trance_begin(s, 1, &c);
}

/* ============================ ★ 中弹救援 / 补死（第二版·落点订正版）============================
 * 完整的一手依据（`+0x0c` 的广播位次、`edi` 累加器语义、Eirin 的 `return 1` 先例、三道门、
 * 为什么**没有**排队状态、补死那一刻的 state）见 `trance_core.h` 的「救援层」与「重新死亡」两段
 * —— 那里是唯一口径来源。本文件只放**能算错的纯判断**，本段一共两个函数。*/

int trance_should_redie(const trance_tick_t *t)
{
    if (!t) return 0;
    return t->re_die ? 1 : 0;
}

/* ============================ 连锁 ============================ */

int trance_chain_note(trance_state_t *s, int32_t type)
{
    int fired;
    if (!s) return 0;
    if (type == 3 || type == 4) return 0;      /* 一手：`if (type != 3 && type != 4)` 才推进连锁 */
    s->chain_count += TRANCE_CHAIN_STEP;
    s->chain_timer = TRANCE_CHAIN_WINDOW;      /* 一手 `0x4391a0`：每颗重置 60 帧窗口 */
    /* ★ **第 10 颗触发**（2026-09-12 订正）：一手是 `this+0x8838++ ; if (this+0x8838 > 9) spawn_spirit(…,3,…)`
     * ⇒ 判据是「**加完之后 ≥ 10**」= 本次这颗就是窗口内第 10 颗。
     * ⚠ 旧实现写的 `> TRANCE_CHAIN_THRESHOLD`（10）等于「第 11 颗才触发」——比一手晚一颗，已改。
     * ★★ 2026-09-14：**调用粒度**也已订正 —— 一手这个 inc 长在 `spawn_spirit`（`th13:0x439232`）里，
     *   即「**掉**一颗灵」时推进，与"吃没吃到"无关。卡侧唯一调用点是 `trance_spirit_spawn`
     *   （见 .h 的粒度订正段）。这里只做算术，不做时机的判断。*/
    fired = (s->chain_count >= TRANCE_CHAIN_THRESHOLD) ? 1 : 0;
    if (fired) ++s->chain_bonus_spawned;
    return fired;
}

int trance_chain_tick(trance_state_t *s, int32_t frame)
{
    (void)frame;                                /* frame 只作接口一致性：窗口是「每帧 −1」的计时器，
                                                 * 一手 `SPIRIT` §3.6 就是这么实现的（不依赖绝对帧号）*/
    if (!s) return 0;
    if (s->chain_timer > 0) {
        --s->chain_timer;
        if (s->chain_timer == 0) {
            s->chain_count = 0;                 /* 一手：窗口归零 ⇒ `+0x8838 = 0` */
            return 1;
        }
    }
    return 0;
}

/* ============================ 四类神灵：收集 / 奖励 ============================ */

trance_reward_t trance_reward(int32_t type, int in_trance, int is_sanae)
{
    trance_reward_t r;
    int32_t t = type;

    r.life_frag = 0; r.max_point = 0; r.spell_frag = 0; r.gauge = 0; r.gauge_type3 = 0;
    if (t == 4) t = 1;                          /* 一手 `SPIRIT:75` 注：type 4 在 `0x438ed8` 与 1 同奖 */
    if (t < 0 || t >= TRANCE_TYPE_COUNT) return r;
    (void)is_sanae;

    /* 统一槽账（一手 `th13:0x438b3d`：所有类型 +2；灵界中不再加槽 `SPIRIT:326`）。
     * type 3 的额外 +1（`0x438c3d`）同样受灵界门控。*/
    r.gauge = in_trance ? TRANCE_REWARD_GAUGE_TRANCE : TRANCE_REWARD_GAUGE_ALWAYS;
    r.gauge_type3 = 0;

    switch (t) {
    case 0:   /* 命碎（一手 普通 +1 `0x43d330` / 灵界 +2；效果 id 0x60 / 0x61）*/
        r.life_frag = in_trance ? 2 : 1;
        break;
    case 1:   /* 最大得点（一手 普通 +10 / 灵界 +100；常量 `0x4aeea4` / `0x4aeaf0`）*/
        r.max_point = in_trance ? 100 : 10;
#if TRANCE_ENABLE_SANAE_TYPE1_BONUS
        if (is_sanae && !in_trance) r.gauge += TRANCE_GAUGE_SANAE_TYPE1;  /* 一手 `0x438bea` */
#else
        (void)is_sanae;
#endif
        break;
    case 2:   /* 符卡碎（一手 普通 +1 `0x43d480` / 灵界 +2）*/
        r.spell_frag = in_trance ? 2 : 1;
        break;
    case 3:   /* 灵界槽 +1 且加分（`0x43d530`）；灵界中**只加分**（不加槽）*/
        r.gauge_type3 = in_trance ? 0 : TRANCE_GAUGE_TYPE3_BONUS;
        break;
    default:
        break;
    }
    return r;
}

int trance_collect(trance_state_t *s, int32_t type, trance_collect_t *out)
{
    trance_reward_t r;
    trance_gauge_add_t ga;
    int32_t t = type;

    if (out) { out->gauge_add = 0; out->tier_crossed = 0; }
    if (!s) return 0;
    if (t == 4) t = 1;                          /* 与 trance_reward 同一口径 */
    if (t < 0 || t >= TRANCE_TYPE_COUNT) return 0;

    /* ① 奖励表（相位决定倍率）*/
    /* ★ 2026-09-13：早苗特例（收 type 1 额外 +1 槽）现在**按角色号自动生效** ——
     *   角色从 state 读（卡侧 `trance_set_character()` 写），所以这里不用改签名。*/
    r = trance_reward(type, s->phase != TRANCE_PHASE_IDLE,
                      trance_traits(s->character)->sanae_type1_bonus);

    /* ② 槽：统一 +2（灵界中 0）+ type 3 的 +1；两笔都要**跨格反馈**，所以分两次加 */
    ga = trance_gauge_add(&s->gauge, r.gauge);
    if (out) {
        out->gauge_add = ga.delta;
        out->tier_crossed = ga.crossed_tier;
    }
    if (r.gauge_type3) {
        trance_gauge_add_t g3 = trance_gauge_add(&s->gauge, r.gauge_type3);
        if (out) {
            out->gauge_add += g3.delta;
            if (g3.crossed_tier) out->tier_crossed = 1;
        }
    }

    /* ③ 记账（**按 type 重建**，不是「蓝 / 白」二分，SPEC §2.2）
     * ⚠ **连锁不在这里推进**（一手：`0x439232` 的 `inc` 长在 `spawn_spirit` 里 ⇒ "掉"的那一刻；
     *   放在收集上会让"玩家在连续吸灵"把 60 帧窗口一直撑住 ⇒ 白灵刷屏，2026-09-14 订正）。*/
    ++s->spirits_seen[t];
    return 1;
}

int32_t trance_max_point_field_addends(int32_t display_delta, int32_t field_unit_per_display)
{
    int64_t v;
    if (field_unit_per_display <= 0) return display_delta;   /* 单位未知 ⇒ 原样返回（宁可少乘）*/
    v = (int64_t)display_delta * (int64_t)field_unit_per_display;
    if (v > 2147483647LL) return 2147483647;
    if (v < -2147483648LL) return (-2147483647 - 1);
    return (int32_t)v;
}

int32_t trance_type3_popup_from_piv(int32_t piv)
{
    int32_t q, rem;
    if (piv <= 0) return 0;
    /* 一手 `th13:0x438c3d` 起汇编（逐条对上）：
     *   eax = 0x51eb851f ; imul DWORD PTR ds:0x4be7dc ; sar edx,5 ; (shr ecx,0x1f / add) ⇒ edx = piv / 100
     *   mov ecx,0xa ; idiv ecx ⇒ 商 eax = (piv/100)/10、余数 edx = (piv/100) % 10
     *   ⇒ 「向下取整到 10 的倍数」= (piv/100) − 余数 （`SPIRIT:84` 的口径）。*/
    q = piv / 100;
    rem = q % 10;
    return q - rem;
}

int32_t trance_anm_script(int32_t type, int in_trance)
{
    int32_t t = type;
    if (t == 4) t = 1;                       /* 一手：type 4 与 1 同外形（`0x438ed8`）*/
    if (t < 0 || t >= TRANCE_TYPE_COUNT) return 0;
    return t * 6 + (in_trance ? 4 : 1);      /* 一手 `th13:0x43907e`：type*6+1（普通）/ type*6+4（灵界）*/
}

/* ============================ 救援层 ============================ */

/* 契约顺序：**107 → 117**（用户 2026-09-12 定案，`EC` §5.2）。静态常数组 —— 调用方只能读。*/
static const int32_t s_rescue_order[TRANCE_RESCUE_COUNT] = { TRANCE_RESCUE_107, TRANCE_RESCUE_117 };

const int32_t *trance_rescue_order(void) { return s_rescue_order; }

int32_t trance_rescue_invuln_frames(int32_t id)
{
    /* `EC` §5.2 细则 3 一手口径：107 森罗结界结束给 0.67 s ≈ 40 帧、117 霊撃 40 帧
     * （117 的一手在 `engine/animal/th17/01-animal-spirits.md` §5.3：40 帧 / 半径 (1−f⁴)×200）。
     * ⇒ 两家都是 40 ⇒ 汇总 max 恰好 40（不叠加、也不互相截短）。*/
    if (id == TRANCE_RESCUE_107) return 40;
    if (id == TRANCE_RESCUE_117) return 40;
    return 0;
}

int trance_rescue_resolve(int32_t active_mask, int32_t card_id, trance_rescue_report_t *out)
{
    int i;

    /* ★ 分层硬拒（SPEC §7 第一条 / `EC` §4.2）：113 是**事后层**（`+0x14`），
     *   **禁止**参与救援层（`+0x10`）的决定。这条拒绝是可执行的，不是注释。*/
    if (card_id == TRANCE_ID) return 0;
    if (!out) return 0;

    out->any = 0;
    out->death_cancelled = 0;
    out->invuln_frames = 0;
    out->cancel_taken = -1;
    for (i = 0; i < TRANCE_RESCUE_COUNT; ++i) {
        out->slots[i].id = s_rescue_order[i];
        out->slots[i].was_active = ((active_mask >> i) & 1) ? 1 : 0;
        out->slots[i].consumed = 0;
        out->slots[i].invuln = 0;
        out->slots[i].cancel_all = 0;
        out->slots[i].burst = 0;
    }

    /* 按契约顺序逐个结算：**一次中弹把救援者全部烧掉**（不挑一个），动作都要执行。*/
    for (i = 0; i < TRANCE_RESCUE_COUNT; ++i) {
        trance_rescue_slot_t *sl = &out->slots[i];
        if (!sl->was_active) continue;
        sl->consumed = 1;
        sl->burst = 1;
        sl->invuln = trance_rescue_invuln_frames(sl->id);
        out->any = 1;
        /* 细则 2：**全屏消弹只发生一次**，由先执行且带消弹效果的那张做 ⇒ 第一家有 burst 的拿账。
         * 107 在顺序里排第一，所以两家都在场时账落在 107（一手契约：107 的结界灵击 = 消弹 + 小樱点）。*/
        if (out->cancel_taken < 0) {
            sl->cancel_all = 1;
            out->cancel_taken = sl->id;
        }
        /* 细则 3：无敌走「声明 + 取最大值」（不是求和）*/
        if (sl->invuln > out->invuln_frames) out->invuln_frames = sl->invuln;
    }

    /* 细则 4 的反面：**只有至少烧掉一家**才谈得上取消死亡；一家都不在 ⇒ 死亡照走，
     * 那次死亡之后才会轮到 113 的 `+0x14`（SPEC §4.2 ③ 事后层）。*/
    out->death_cancelled = out->any ? 1 : 0;
    return 1;
}

/* ============================ 击破 → 神灵（记账层） ============================ */

/* ★★ 一次击破掉几颗 —— TH13 通道 B 的逐字复刻。一手、入参口径、以及**唯一一处 ❓ 自定**
 * （窗口过期时保底 1 颗）全部写在 `trance_core.h` 的函数注释里（那里是唯一口径来源）。*/
int32_t trance_kill_spirit_count(int32_t alive_frames, int32_t window_T, int32_t native_count,
                                 int32_t difficulty)
{
    int32_t step, n;

    if (alive_frames < 0) alive_frames = 0;                  /* 入参防御：存活帧数不可能为负 */
    if (window_T < 1 || native_count < 1) return TRANCE_KILL_SPIRIT_MIN;   /* 没配窗口 ⇒ 保底 */
    /* 一手：`if (enemy_timer <= max) { … }` —— 窗口过期（`timer > max`）时**整段不执行**（0 颗）。
     * 本卡在这里保底 1 颗（❓ 自定，理由见头注）。⚠ 一手的 `+1` 与 H/L 的 `+1` 都在 `if` **里面**
     * ⇒ 过期这一档**不加**难度奖励（照抄控制流），也正好让「拖到窗口末尾」平滑地停在 1。*/
    if (alive_frames > window_T) return TRANCE_KILL_SPIRIT_MIN;

    /* 一手 `max / div`（整数除）。⚠ `div > max` 时它是 0 ⇒ 一手会整数除零；这里按步长 1 处理。*/
    step = window_T / native_count;
    if (step < 1) step = 1;

    n = (window_T - alive_frames) / step + 1;                /* 瞬杀 = native_count + 1（步长恰为 T/N）*/
    /* 一手：`if (DIFFICULTY == 2 || DIFFICULTY == 3) n += 1` —— Hard / Lunatic 各 +1。
     * TH18 的难度号同一套 0..3（+4 EXTRA）⇒ 原值直传即可；EXTRA **不加**（一手没这一支）。*/
    if (difficulty == 2 || difficulty == 3) n += 1;
    return n;
}

int32_t trance_kill_spirit_clamp(int32_t n)
{
    if (n < TRANCE_KILL_SPIRIT_MIN) return TRANCE_KILL_SPIRIT_MIN;
    if (n > TRANCE_KILL_SPIRIT_MAX) return TRANCE_KILL_SPIRIT_MAX;
    return n;
}

void trance_kill_note(trance_state_t *s, int32_t frame, float x, float y, int32_t n)
{
    int32_t i;
    if (!s) return;
    i = s->kill_pending;
    if (i >= TRANCE_KILL_QUEUE_MAX) {
        ++trance_kill_queue_dropped;
        return;
    }
    /* 只抄**标量**：`ev->enemy` 指针同帧就被 sweep 删掉（`DEATH` §2.4 禁止缓存）⇒ 队列里不放指针。
     * 整个函数 O(1)：一次数组写 + 计数 ++（清场帧会被调 100–300 次，见 `DEATH` §1.7）。
     * ★ 一次击破 = 一条（**不按颗数展开**）—— 展开会让 16 格的队列瞬间溢出，
     *   帧末也就没法按「一次击破」打出一条能对账的日志。*/
    s->kill_x[i] = x;
    s->kill_y[i] = y;
    s->kill_n[i] = (n <= 0) ? TRANCE_KILL_SPIRIT_MIN : n;    /* 脏数据保底；正常路径恒 ≥ 1 */
    s->kill_pending = i + 1;
    s->kill_last_frame = frame;
}

int32_t trance_kill_take(trance_state_t *s, int32_t frame, float *out_x, float *out_y,
                         int32_t *out_n, int32_t cap)
{
    int32_t n, i, c;
    (void)frame;
    if (!s) return 0;
    n = s->kill_pending;
    s->kill_pending = 0;
    if (n <= 0) return 0;
    if (cap > 0) {
        c = (n < cap) ? n : cap;
        for (i = 0; i < c; ++i) {          /* 按入队顺序抄（帧末撒落要的就是这个顺序）*/
            if (out_x) out_x[i] = s->kill_x[i];
            if (out_y) out_y[i] = s->kill_y[i];
            if (out_n) out_n[i] = s->kill_n[i];
        }
    }
    return n;
}

/* ==================== ★★ boss 相位击破的灵批次（2026-09-13 第二轮；09-14 改为逐关固定档）====================
 * 一手规则（符卡 vs 非符的门、每关的颗数表）、TH18 落点、判据、以及 🟡 近似**全部**在
 * `trance_core.h` 的那一段（那里是唯一口径来源）。这两个函数只做查表 + 选型 ⇒ 能被主机单测逐值钉死。*/

/* 一手 `thecl -d 13` 全量普查的**颗数**（`dropExtra(10,·)+dropExtra(12,·)+dropExtra(13,·)` 之和）。
 * 索引 = 关卡号 − 1（st01..st07；st07 = EX）。表本身与出处见 `trance_core.h` 的那张表。*/
static const int32_t s_phase_batch_boss[TRANCE_PHASE_BATCH_TYPES]    = { 22, 22, 22, 32, 37, 32, 47 };
static const int32_t s_phase_batch_midboss[TRANCE_PHASE_BATCH_TYPES] = {  5,  9, 24, 19, 19, 19, 25 };

int32_t trance_boss_phase_spirit_count(int32_t stage, int32_t is_midboss, int32_t phase_items)
{
    int32_t i;

    /* 0 = "这不是一次相位奖励"（计数值全零：重复回调 / 该敌人没配掉落）——调用方应当直接返回。
     * ⚠ 一手那一支永远有东西掉（`dropExtra(10,1)+dropExtra(12,1)+dropExtra(13,20)`），
     *   所以这里把 0 当"事件无效"是安全的：TH18 侧它同时也正是**去重判据**（见卡侧注释）。
     * ★ 2026-09-14 起 `phase_items` **只当去重判据**，不再参与颗数（原来那条 `items/2` 的标定已废）。*/
    if (phase_items <= 0) return 0;
    i = stage - 1;                                   /* CE_CURRENT_STAGE() 是 1-based */
    if (i < 0) i = 0;
    if (i >= TRANCE_PHASE_BATCH_TYPES) i = TRANCE_PHASE_BATCH_TYPES - 1;
    return is_midboss ? s_phase_batch_midboss[i] : s_phase_batch_boss[i];
}

int32_t trance_boss_phase_spirit_type(int32_t index, int32_t total)
{
    if (total < 3) return TRANCE_PHASE_TYPE_REST;       /* 防呆：一手没有只掉 1~2 颗的相位批次 */
    if (index == 0) return TRANCE_PHASE_TYPE_A;         /* 一手 `dropExtra(10, 1)` */
    if (index == 1) return TRANCE_PHASE_TYPE_B;         /* 一手 `dropExtra(12, 1)` */
    return TRANCE_PHASE_TYPE_REST;                      /* 一手 `dropExtra(13, N)`（91%~96%）*/
}

/* ============================ ★ 神灵实体（VM 池）的策略 ============================
 *
 * 用户裁决 / 一手数值 / 「为什么策略放这里」见 `trance_core.h` 的那一段（唯一口径来源）。
 * 本段**全整数**：千分点的缩放、整数像素的方波与平方比较 ⇒ 与文件头「一个 float 都没有」同一条纪律。 */

int32_t trance_spirit_scale_permille(int32_t age)
{
    int64_t d;
    int32_t t;

    if (age <= TRANCE_SPIRIT_SCALE_DELAY) return TRANCE_SPIRIT_SCALE_PERMILLE_BASE;
    t = age - TRANCE_SPIRIT_SCALE_DELAY;                       /* 进插值段之后的帧数 */
    if (t >= TRANCE_SPIRIT_SCALE_FRAMES) return TRANCE_SPIRIT_SCALE_END_PERMILLE;
    /* 线性：BASE + (END − BASE) × t / FRAMES（整数除**向零截断**）。
     * 一手是 VM 里的浮点插值（`+200: scaleTime(200, 0, 0.4, 0.4)`），我们只要「前 200 帧不变、
     * 之后单调变小 + 两个端点逐字照抄」这一条语义；千分点的取整误差 ≤ 1/1000（肉眼不可见）。
     * 例：age=200 ⇒ 1000；age=300 ⇒ 1000 − 600×100/200 = 700；age=399 ⇒ 403；age=400 ⇒ 400。*/
    d = (int64_t)(TRANCE_SPIRIT_SCALE_END_PERMILLE - TRANCE_SPIRIT_SCALE_PERMILLE_BASE) * (int64_t)t;
    return TRANCE_SPIRIT_SCALE_PERMILLE_BASE + (int32_t)(d / TRANCE_SPIRIT_SCALE_FRAMES);
}

int32_t trance_spirit_bob_offset(int32_t age)
{
    int32_t h, q;

    if (age < 0) return 0;
    h = age % TRANCE_SPIRIT_BOB_PERIOD;          /* 0 .. PERIOD−1 */
    q = TRANCE_SPIRIT_BOB_PERIOD / 4;            /* 一季 = 8 帧（周期 32 帧的 1/4）*/
    if (q <= 0) return 0;
    if (h < q) return 0;                         /* 第 1 季：不动（= 出生那一刻就在正位）*/
    if (h < q * 2) return -TRANCE_SPIRIT_BOB_PX; /* 第 2 季：上浮 1 px */
    if (h < q * 3) return 0;                     /* 第 3 季：回正 */
    return TRANCE_SPIRIT_BOB_PX;                 /* 第 4 季：下沉 1 px */
}

int trance_spirit_expired(int32_t life)
{
    return (life <= 0) ? 1 : 0;
}

int trance_spirit_in_pickup_range(int32_t dx, int32_t dy)
{
    int64_t r = (int64_t)TRANCE_SPIRIT_PICK_RADIUS;
    /* 整数平方比较（**边界含**：dist² ≤ r²）。用 int64 中间量 ⇒ 不会在极端入参上溢出
     * （dx/dy 是自机与神灵的位移，正常 ≤ 几百 px；这里只是不给自己留坑）。*/
    return ((int64_t)dx * (int64_t)dx + (int64_t)dy * (int64_t)dy <= r * r) ? 1 : 0;
}

/* ★ 神灵运动的 state 迁移（一手 `th13:0x438730`；见 `trance_core.h` 的表）。
 *
 * 逐条照抄一手的三条边界，**不自己发明平滑**：
 *   · state 2（被吸）：`dist² ≥ 128²` 退回漂移（**迟滞上界**，不是"离开 96 就退"）；
 *   · state 1（漂移）：`dist² < 96²` 进被吸；否则**只有灵梦 + 灵界态**才进 state 4；
 *   · state 4（灵梦慢吸）：`dist² < 112²` 转快吸，否则继续慢吸（任意距离都不掉回漂移）。
 * 其它值（含 0/3/5）一律归一到漂移 —— 我们的池不走 state 3（"进奖励分支"）与 state 5（死代码）。*/
int32_t trance_spirit_state_step(int32_t state, int32_t dist2, int32_t in_trance, int32_t is_reimu)
{
    if (dist2 < 0) dist2 = 0;                 /* 入参防御：平方距离不可能为负 */

    if (state == TRANCE_STATE_ATTRACT)
        return (dist2 >= TRANCE_ATTRACT_OUT2) ? TRANCE_STATE_DRIFT : TRANCE_STATE_ATTRACT;

    if (state == TRANCE_STATE_REIMU_SLOW) {
        if (dist2 < TRANCE_ATTRACT_REIMU_IN2) return TRANCE_STATE_ATTRACT;
        return TRANCE_STATE_REIMU_SLOW;       /* ★ 慢吸**不会**因为太远而放弃（一手：任意距离）*/
    }

    /* 漂移（含所有异常 state）*/
    if (dist2 < TRANCE_ATTRACT_IN2) return TRANCE_STATE_ATTRACT;
    if (is_reimu && in_trance) return TRANCE_STATE_REIMU_SLOW;
    return TRANCE_STATE_DRIFT;
}

/* 出界（一手框 x∈[−224,224]、y∈[−32,480]）：`th13:0x438a8a`–`0x438ad4`。
 * ★ 与"墙反弹"（x∈[−192,192]、y∈[0,448]，且**只对 flags2 bit1 置位的神灵**生效）是**两个不同的框**：
 *   反弹是可选的窄框、出界删除是无条件的宽框 ⇒ 我们只做后者（不做反弹）。*/
int trance_spirit_out_of_field(int32_t x, int32_t y)
{
    return (x < -224 || x > 224 || y < -32 || y > 480) ? 1 : 0;
}

int32_t trance_spirit_pool_room(int32_t used)
{
    if (used < 0) used = 0;
    if (used >= TRANCE_SPIRIT_POOL) return 0;
    return TRANCE_SPIRIT_POOL - used;
}

int32_t trance_spirit_gauge_value(int32_t type, int in_trance)
{
    /* 复用奖励表（唯一一份槽账就在那里）⇒ 这里不重写「+2 / type3 +1 / 灵界中 0」。*/
    trance_reward_t r = trance_reward(type, in_trance, 0);
    return r.gauge + r.gauge_type3;
}

/* ============================ 自检 ============================ */

int trance_core_selfcheck(void)
{
    int bad = 0;
    /* 编译期能断言的都写在这里，让「改了常量忘了改语义」在单测里立刻红。*/
    if (TRANCE_GAUGE_MAX != 600) ++bad;
    if (TRANCE_GAUGE_TIER != 200) ++bad;
    if (TRANCE_GAUGE_TIERS != 3) ++bad;
    if (TRANCE_GAUGE_PER_SPIRIT != 2) ++bad;
    if (TRANCE_GAUGE_END_PENALTY != 200) ++bad;
    if (TRANCE_GAUGE_MIN_ENTER != 200) ++bad;
    if (TRANCE_COOLDOWN_FRAMES != 30) ++bad;
    if (TRANCE_CHAIN_WINDOW != 60) ++bad;
    if (TRANCE_CHAIN_THRESHOLD != 10) ++bad;
    if (TRANCE_KILL_SPIRIT_TYPE != 1) ++bad;        /* SPEC §2.2：击破 = 通道 B ⇒ type 1 */
    if (TRANCE_KILL_SPIRIT_MIN != 1) ++bad;
    if (TRANCE_KILL_SPIRIT_MAX != 64) ++bad;        /* ❓ 自定（防脏数据；见 .h 的注释）*/
    if (TRANCE_PHASE_BATCH_TYPES != 7) ++bad;
    if (TRANCE_PHASE_TYPE_A != 0 || TRANCE_PHASE_TYPE_B != 2 || TRANCE_PHASE_TYPE_REST != 3) ++bad;
    if (TRANCE_DURATION_FRAMES_MAX != 600) ++bad;
    if (TRANCE_TYPE_COUNT != 4) ++bad;
    if (TRANCE_RESCUE_COUNT != 2) ++bad;
    if (s_rescue_order[0] != 107 || s_rescue_order[1] != 117) ++bad;   /* 顺序契约不许改 */
    if (TRANCE_RESCUE_107 == TRANCE_ID || TRANCE_RESCUE_117 == TRANCE_ID) ++bad;  /* 113 不在这层 */
    /* ★ 第二版（落点订正版）：相位只有三个取值 —— 多一个就说明第一版的排队相位没删干净。*/
    if (TRANCE_PHASE_IDLE != 0 || TRANCE_PHASE_MANUAL != 1 || TRANCE_PHASE_DEATH_FREE != 2) ++bad;

    /* ★ 行为不变式（2026-09-13，**TH13 通道 B 的颗数公式**）：常量写对了但**边界**最容易错
     *   （瞬杀 / 拖满 / 过期 / H-L / EXTRA），所以直接用一手普查里的真实数据钉住
     *   （`engine/spirit/th13/01-spirit-and-trance.md` §3.2 的 `thecl -d 13` 全量普查）。
     *   纯计算、不改全局；卡侧 `on_load` 每次都会调本函数。*/
    {
        if (trance_kill_spirit_count(0, 120, 5, 1) != 6) ++bad;      /* st01 GirlB01 (5,120) ⇒ 6 */
        if (trance_kill_spirit_count(0, 120, 5, 2) != 7) ++bad;      /* Hard：+1 */
        if (trance_kill_spirit_count(0, 120, 5, 3) != 7) ++bad;      /* Lunatic：+1 */
        if (trance_kill_spirit_count(0, 120, 5, 4) != 6) ++bad;      /* EXTRA(4)：一手是 ==2||==3 ⇒ **不加** */
        if (trance_kill_spirit_count(120, 120, 5, 1) != 1) ++bad;    /* 拖满窗口 ⇒ 1（一手同一公式）*/
        if (trance_kill_spirit_count(121, 120, 5, 1) != 1) ++bad;    /* 过期 ⇒ 保底 1（❓ 自定，见 .h）*/
        if (trance_kill_spirit_count(0, 350, 16, 1) != 17) ++bad;    /* st05 GirlD01 (16,350) ⇒ 17 */
        if (trance_kill_spirit_count(0, 400, 40, 1) != 41) ++bad;    /* st07 GirlD02 (40,400) ⇒ 41 */
        if (trance_kill_spirit_count(0, 0, 0, 1) != TRANCE_KILL_SPIRIT_MIN) ++bad;   /* 没配窗口 ⇒ 保底 */
        if (trance_kill_spirit_clamp(1000) != TRANCE_KILL_SPIRIT_MAX) ++bad;         /* 上界夹取 */
        if (trance_kill_spirit_clamp(0) != TRANCE_KILL_SPIRIT_MIN) ++bad;
        if (trance_kill_spirit_clamp(6) != 6) ++bad;
    }

    /* ★ 行为不变式（2026-09-13 第二轮；09-14 改成**逐关固定档**）：
     *   期望值 = 一手 TH13 `thecl -d 13` 普查里**同一关、同一类 boss** 的实测颗数（见 .h 的表）。
     *   ⚠ 这里刻意把表里的**每一个值**都钉一遍 —— 表改了而 `.h` 的注释没跟着改，单测就红。*/
    {
        if (trance_boss_phase_spirit_count(1, 0, 0) != 0) ++bad;    /* 计数全零 = 事件无效（去重判据）*/
        if (trance_boss_phase_spirit_count(1, 0, -5) != 0) ++bad;
        /* stage boss（一手 `Boss2..`：st01..st03 22 / st04 32 / st05 37 / st06 32 / st07 47）*/
        if (trance_boss_phase_spirit_count(1, 0, 21) != 22) ++bad;
        if (trance_boss_phase_spirit_count(2, 0, 31) != 22) ++bad;
        if (trance_boss_phase_spirit_count(3, 0, 41) != 22) ++bad;
        if (trance_boss_phase_spirit_count(4, 0, 66) != 32) ++bad;   /* `dropExtra(13,30)` ⇒ 32 */
        if (trance_boss_phase_spirit_count(5, 0, 20) != 37) ++bad;   /* `dropExtra(13,35)` ⇒ 37 */
        if (trance_boss_phase_spirit_count(6, 0, 20) != 32) ++bad;
        if (trance_boss_phase_spirit_count(7, 0, 20) != 47) ++bad;   /* st07 = EX：`10×1+12×2+13×44` */
        /* 道中 boss（一手 `MBossDead`：5 / 9 / 24 / 19 / 19 / 19 / 25）*/
        if (trance_boss_phase_spirit_count(1, 1, 21) != 5) ++bad;
        if (trance_boss_phase_spirit_count(2, 1, 21) != 9) ++bad;
        if (trance_boss_phase_spirit_count(3, 1, 21) != 24) ++bad;
        if (trance_boss_phase_spirit_count(4, 1, 21) != 19) ++bad;
        if (trance_boss_phase_spirit_count(5, 1, 21) != 19) ++bad;
        if (trance_boss_phase_spirit_count(6, 1, 21) != 19) ++bad;
        if (trance_boss_phase_spirit_count(7, 1, 21) != 25) ++bad;
        /* 关卡号越界一律夹取（`CE_CURRENT_STAGE()` 在菜单/商店里可能是 0 或垃圾）*/
        if (trance_boss_phase_spirit_count(0, 0, 21) != 22) ++bad;
        if (trance_boss_phase_spirit_count(-3, 1, 21) != 5) ++bad;
        if (trance_boss_phase_spirit_count(99, 0, 21) != 47) ++bad;
        if (trance_boss_phase_spirit_count(99, 1, 21) != 25) ++bad;
        /* 构成（一手：22 颗里 1×type0 + 1×type2 + 20×type3）*/
        if (trance_boss_phase_spirit_type(0, 22) != TRANCE_PHASE_TYPE_A) ++bad;
        if (trance_boss_phase_spirit_type(1, 22) != TRANCE_PHASE_TYPE_B) ++bad;
        if (trance_boss_phase_spirit_type(2, 22) != TRANCE_PHASE_TYPE_REST) ++bad;
        if (trance_boss_phase_spirit_type(21, 22) != TRANCE_PHASE_TYPE_REST) ++bad;
        if (trance_boss_phase_spirit_type(0, 2) != TRANCE_PHASE_TYPE_REST) ++bad;    /* 防呆分支 */
    }

    /* ★ 行为不变式（2026-09-22 **一手复核后改写**）：**已在灵界 ⇒ begin 成功且什么都不改**
     *   （一手 `0x405b64` 的 `xor eax,eax; ret 4`）⇒ 手动窗里中弹**不升格、不欠死**。
     *   上一版这段钉的是「升格成死亡强制档」——那是把 `0x405a4c`（其实是「槽 < 200 ⇒ return −1」）
     *   误读成位写，见 `trance_begin` 的长注释。纯计算、不改全局；卡侧 `on_load` 每次都会调。*/
    {
        trance_state_t t2;
        trance_cond_t c2;
        trance_tick_t tk2;
        int r0, r1, r2;
        trance_state_init(&t2);
        memset(&c2, 0, sizeof c2);
        trance_gauge_set(&t2.gauge, TRANCE_GAUGE_MAX);
        r0 = trance_begin(&t2, 0, &c2);                          /* 先开一个**手动**窗 */
        if (r0 != 1) ++bad;
        if (t2.phase != TRANCE_PHASE_MANUAL) ++bad;
        if (t2.activations != 1) ++bad;
        r1 = trance_force_on_death(&t2);                         /* 窗里中弹 ⇒ 一手：成功 + 原样不动 */
        if (r1 != 1) ++bad;
        if (t2.phase != TRANCE_PHASE_MANUAL) ++bad;              /* ★ 不升格 */
        if (t2.deaths_rescued != 0) ++bad;                       /* ★ 不算救下（不记账）*/
        if (t2.activations != 1) ++bad;                          /* ★ 不另算一次 begin */
        trance_end(&t2, &tk2);
        if (tk2.re_die != 0) ++bad;                              /* ★ 手动窗结束**不欠死**（owner th13 #6）*/
        if (tk2.ended_death_free != 0) ++bad;
        if (t2.gauge.gauge != TRANCE_GAUGE_MAX - TRANCE_GAUGE_END_PENALTY) ++bad;  /* 非死亡档 ⇒ −200 */
        /* 已经在死亡强制档里再被中弹（同一帧第二次撞弹 / 补死链）⇒ 同样「成功 + 原样不动」。*/
        trance_state_init(&t2);
        trance_gauge_set(&t2.gauge, TRANCE_GAUGE_MAX);
        if (trance_force_on_death(&t2) != 1) ++bad;
        if (t2.phase != TRANCE_PHASE_DEATH_FREE || t2.deaths_rescued != 1 || t2.activations != 1) ++bad;
        r2 = trance_force_on_death(&t2);
        if (r2 != 1) ++bad;
        if (t2.deaths_rescued != 1 || t2.activations != 1) ++bad;   /* 幂等，不重复记账 */
        /* ★ 链式是**允许**的（用户 2026-09-12 定案「就这样，允许链式」）⇒ 本卡**没有**任何
         *   「禁止再救」的状态位（第一版的 `re_die_note` 已删）：补死就是一次普通死亡，
         *   槽够就照样能再救。这里钉住「救过之后（诊断计数已涨）仍然能再救一次」。*/
        trance_state_init(&t2);
        trance_gauge_set(&t2.gauge, TRANCE_GAUGE_MAX);
        r2 = trance_force_on_death(&t2);
        if (r2 != 1) ++bad;
        if (t2.deaths_rescued != 1) ++bad;
        trance_end(&t2, &tk2);
        if (tk2.re_die != 1) ++bad;                              /* 欠一次死亡 */
        trance_gauge_set(&t2.gauge, TRANCE_GAUGE_MAX);           /* 窗口里又攒回 ≥200（链式）*/
        r2 = trance_force_on_death(&t2);
        if (r2 != 1) ++bad;                                      /* ★ 照样能救（允许链式）*/
        if (t2.phase != TRANCE_PHASE_DEATH_FREE) ++bad;
        if (t2.deaths_rescued != 2) ++bad;
    }
    /* ★ 行为不变式：连锁是「第 10 颗触发」（一手 `+0x8838++ ; if (> 9)`）——
     * 常量对了但比较符写错（> 而不是 >=）只有跑一遍才看得出来 ⇒ 在这里跑一遍。
     * 纯计算、不改全局；卡侧 `on_load` 每次都会调本函数，成本 = 10 次加法。*/
    {
        trance_state_t t;
        int32_t i;
        memset(&t, 0, sizeof t);
        for (i = 1; i < TRANCE_CHAIN_THRESHOLD; ++i) {
            if (trance_chain_note(&t, 1) != 0) ++bad;      /* 第 1..9 颗都不许触发 */
        }
        if (i != TRANCE_CHAIN_THRESHOLD) ++bad;
        if (trance_chain_note(&t, 1) != 1) ++bad;          /* 第 10 颗必须触发 */
        if (t.chain_count != TRANCE_CHAIN_THRESHOLD) ++bad;
        if (trance_chain_note(&t, 3) != 0) ++bad;          /* type 3 自身不推进（一手同一句）*/
    }

    /* ★ 行为不变式：**神灵实体池的策略**（用户 2026-09-12 裁决的四条）——
     * 常量对了但曲线 / 边界的写法错了（例如「到点」判成 `life < 0`、半径判成 `<` 而不是 `<=`）
     * 只有跑一遍才看得出来 ⇒ 在这里各跑一次端点。
     *   ① 缩小曲线：**前 200 帧保持 1000‰**、200→400 线性缩到 400‰（一手 `astral.anm` script1）；
     *   ② 到期：剩余生命 ≤ 0 才算到点（寿命 480 帧照抄一手 `th13:0x438a61`）。*/
    if (TRANCE_SPIRIT_SCALE_START_PERMILLE != 1500) ++bad;
    if (TRANCE_SPIRIT_SCALE_PERMILLE_BASE != 1000) ++bad;
    if (TRANCE_SPIRIT_SCALE_END_PERMILLE != 400) ++bad;
    if (TRANCE_SPIRIT_SCALE_DELAY != 200) ++bad;           /* ★ 一手 `+200:` 的相位（2026-09-14 订正）*/
    if (TRANCE_SPIRIT_SCALE_FRAMES != 200) ++bad;
    if (TRANCE_SPIRIT_LIFE_FRAMES != 480) ++bad;
    if (trance_spirit_scale_permille(0) != TRANCE_SPIRIT_SCALE_PERMILLE_BASE) ++bad;
    if (trance_spirit_scale_permille(TRANCE_SPIRIT_SCALE_DELAY) != TRANCE_SPIRIT_SCALE_PERMILLE_BASE) ++bad;
    if (trance_spirit_scale_permille(1) != TRANCE_SPIRIT_SCALE_PERMILLE_BASE) ++bad;  /* 前 200 帧不许变小 */
    if (trance_spirit_scale_permille(199) != TRANCE_SPIRIT_SCALE_PERMILLE_BASE) ++bad;
    if (trance_spirit_scale_permille(TRANCE_SPIRIT_SCALE_DELAY + 1) >= TRANCE_SPIRIT_SCALE_PERMILLE_BASE) ++bad;
    if (trance_spirit_scale_permille(TRANCE_SPIRIT_SCALE_DELAY + TRANCE_SPIRIT_SCALE_FRAMES)
        != TRANCE_SPIRIT_SCALE_END_PERMILLE) ++bad;
    if (trance_spirit_scale_permille(TRANCE_SPIRIT_SCALE_DELAY + TRANCE_SPIRIT_SCALE_FRAMES / 2)
        >= trance_spirit_scale_permille(TRANCE_SPIRIT_SCALE_DELAY)) ++bad;    /* 中途必须已经变小 */
    if (trance_spirit_scale_permille(TRANCE_SPIRIT_LIFE_FRAMES) != TRANCE_SPIRIT_SCALE_END_PERMILLE) ++bad;
    if (trance_spirit_scale_permille(100000) != TRANCE_SPIRIT_SCALE_END_PERMILLE) ++bad;
    if (trance_spirit_expired(TRANCE_SPIRIT_LIFE_FRAMES) != 0) ++bad;
    if (trance_spirit_expired(1) != 0) ++bad;
    if (trance_spirit_expired(0) != 1) ++bad;              /* ← 边界：0 就是到点 */
    if (trance_spirit_expired(-1) != 1) ++bad;
    if (trance_spirit_bob_offset(0) != 0) ++bad;           /* 出生那一刻不许有偏移 */
    if (trance_spirit_in_pickup_range(0, 0) != 1) ++bad;
    if (trance_spirit_in_pickup_range(TRANCE_SPIRIT_PICK_RADIUS, 0) != 1) ++bad;      /* 边界含 */
    if (trance_spirit_in_pickup_range(TRANCE_SPIRIT_PICK_RADIUS + 1, 0) != 0) ++bad;
    if (trance_spirit_pool_room(0) != TRANCE_SPIRIT_POOL) ++bad;
    if (trance_spirit_pool_room(TRANCE_SPIRIT_POOL) != 0) ++bad;
    if (trance_spirit_gauge_value(TRANCE_KILL_SPIRIT_TYPE, 0) != TRANCE_GAUGE_PER_SPIRIT) ++bad;
    if (trance_spirit_gauge_value(TRANCE_KILL_SPIRIT_TYPE, 1) != 0) ++bad;             /* 灵界中不加槽 */
    return bad;
}
