/* senro_core.c —— 107「妖妖梦」的纯逻辑实现（逐条照 th07 一手；依据与地址全在 senro_core.h 头注）。
 *
 * 三条实现纪律：
 *   ① **无浮点、无索引表以外的东西** —— TH18 的 `make dllx87` 门要求 0 条 x87 指令，且本文件要在主机上跑单测；
 *   ② 每个数值都能追到 `0x…` 或明确标 `❓ 自定`（后者只在头文件里以宏出现，见头注「❓ 自定占位」）；
 *   ③ **不夹带引擎语义** —— 「谁在什么事件里调哪一条」是 `senro.c` 的事，这里只有状态与算术。
 */
#include "senro_core.h"

/* ============================ 难度表（一手 0x42eb7a–0x42ec57） ============================ */

int32_t senro_cherry_max_init(int difficulty)
{
    switch (difficulty) {
    case SENRO_DIFF_EASY:    return 200000;   /* 0x42eb86 `add eax,0x30d40` */
    case SENRO_DIFF_NORMAL:  return 200000;   /* 0x42eba5 同上 */
    case SENRO_DIFF_HARD:    return 250000;   /* 0x42ebc4 `add eax,0x3d090` */
    case SENRO_DIFF_LUNATIC: return 300000;   /* 0x42ebe3 `add eax,0x493e0` */
    case SENRO_DIFF_EXTRA:   return 400000;   /* 0x42ebff `add eax,0x61a80` */
    case SENRO_DIFF_PHANTASM:return 400000;   /* 0x42ec35 同上 */
    default:                 return 200000;   /* 越界兜底 = NORMAL 档 */
    }
}

int32_t senro_cherry_init(int difficulty)
{
    switch (difficulty) {
    case SENRO_DIFF_EXTRA:    return 200000;  /* 0x42ec19 `add eax,0x30d40` → 写 `+0x961c`（樱点）*/
    case SENRO_DIFF_PHANTASM: return 300000;  /* 0x42ec4f `add eax,0x493e0` → 同上 */
    default:                  return 0;       /* E/N/H/L：那一支只写最大樱点，樱点保持「基准」= 0 */
    }
}

/* ============================ 工具 ============================ */

int32_t senro_round10(int32_t v)
{
    /* th07 的写法：`idiv 10` 取余、再用被除数减余数（0x440ed8/0x440ee2、0x4331db/0x4331e5）。
     * C 的 `%` 对负数给负余数 ⇒ 先归一到 [0,10) 才是同一件事（负数在本卡只出现在中间量上）。*/
    int32_t r = v % 10;
    if (r < 0) r += 10;
    return v - r;
}

int32_t senro_plus_for_timer(int32_t timer)
{
    /* th07:0x4414db `imul eax,eax,0xc350` → `idiv [player+0x16a14]` → `0x4414e5 mov ds:0x62f890,eax`
     * （Plus = 剩余计时 / 总时长 × 50000；随后 0x4414ea/0x4414f3 夹 ≥ 0）。*/
    if (timer <= 0) return 0;
    if (timer > SENRO_BORDER_FRAMES) timer = SENRO_BORDER_FRAMES;
    return (int32_t)(((int64_t)timer * SENRO_PLUS_FULL) / SENRO_BORDER_FRAMES);
}

int senro_border_active(const senro_state_t *s)
{
    return (s && s->active) ? 1 : 0;
}

/* ============================ 初始化 ============================ */

static void senro_reset_common(senro_state_t *s, int difficulty, int keep_stats)
{
    int32_t o = 0, n = 0, l = 0, c = 0, it = 0, h = 0;

    if (!s) return;
    if (keep_stats) {
        o = s->stat_open; n = s->stat_natural; l = s->stat_libyou;
        c = s->stat_cancel; it = s->stat_items; h = s->stat_hits;
    }
    s->cherry = senro_cherry_init(difficulty);
    s->cherry_max = senro_cherry_max_init(difficulty);
    if (s->cherry > s->cherry_max) s->cherry = s->cherry_max;
    s->plus = 0;                 /* th07:0x4416ee / 0x441da4：两种收尾都把 Plus 打回基准 */
    s->active = 0;
    s->queued = 0;
    s->timer = 0;
    s->invuln = 0;
    s->graze_last = -1;          /* 差分基准撤销：新关 / 新 run 的第一帧不补记擦弹 */
    s->spell_bonuses = 0;
    s->last_bonus = 0;
    s->ev_pending = 0;
    s->bomb_active = 0;
    s->stat_open = o; s->stat_natural = n; s->stat_libyou = l;
    s->stat_cancel = c; s->stat_items = it; s->stat_hits = h;
}

void senro_init(senro_state_t *s, int difficulty)       { senro_reset_common(s, difficulty, 0); }
void senro_stage_reset(senro_state_t *s, int difficulty){ senro_reset_common(s, difficulty, 1); }

/* ============================ 樱点加法器 ============================ */

/* 开结界（th07:0x441960 的正常展开路径：写计时长度 0x21c、`[player+0x240d]=1`、`[player+0x2408]=4`）。*/
static void senro_border_open(senro_state_t *s)
{
    s->active = 1;
    s->queued = 0;
    s->timer = SENRO_BORDER_FRAMES;
    s->plus = SENRO_PLUS_FULL;
    ++s->stat_open;
    s->ev_pending |= SENRO_EV_BORDER_OPEN;
}

int senro_add_cherry(senro_state_t *s, int32_t delta)
{
    if (!s) return SENRO_EV_NONE;

    /* th07:0x42f5bf 起：**先无条件** `cherry += delta` 并夹 ≤ cherry_max（0x42f5d7 `jle` 跳过夹取）。*/
    s->cherry += delta;
    if (s->cherry > s->cherry_max) s->cherry = s->cherry_max;
    if (s->cherry < 0) s->cherry = 0;         /* 基准 = 0（th07 的基准在 [globals+0x88]，本卡取 0）*/

    /* th07:0x42f65d–0x42f696：加了之后「樱点 ≥ 最大樱点 且 值变了」⇒ 飘一条满值提示。*/
    if (s->cherry >= s->cherry_max) s->ev_pending |= SENRO_EV_CHERRY_MAXED;

    /* +C 与开门**只在 delta > 0 时**（th07:0x42f5f1 `cmp [ebp+0x8],0x0; jle` 跳过整段；
     * 闸门 `[0x4bfee5] != 0` 时也不涨 +C —— 该标志全 exe 无写点，本卡按默认 0 实现，见头注 ③）。*/
    if (delta <= 0) return s->ev_pending;
    s->plus += delta;
    if (s->plus >= SENRO_PLUS_FULL) {
        s->plus = SENRO_PLUS_FULL;            /* 0x42f647：夹到基准 + 50000 */
        if (!s->active && !s->bomb_active) {
            senro_border_open(s);             /* 0x42f652 `call 0x441960` */
        } else if (s->bomb_active && !s->queued) {
            /* 0x44196c `cmp [eax+0x16a20],0x0`（炸弹计时非 0）→ 0x441983 `[player+0x240d] = 2`。
             * ⚠ 另两处排队写点（0x4419b2 / 0x4419df）的门是**主状态 ∈ {1,3}**（Miss 待复活 / 结界收尾动画），
             * **不是**结界展开中（=4）⇒ 本卡只在「正在放炸弹」时排队；结界展开中再攒满 ⇒ +C 停在满值、
             * 由下一帧的计时比例重写（不重开、不排队），见本卡 spec §6.2。 */
            s->queued = 1;
            s->ev_pending |= SENRO_EV_BORDER_QUEUE;
        }
    }
    return s->ev_pending;
}

int senro_add_cherry_only(senro_state_t *s, int32_t delta)
{
    /* 照 0x42f69f / 0x42f736 的函数体 —— 只有 `+0x961c` 一个写点（0x42f6c2 / 0x42f759）。 */
    if (!s) return SENRO_EV_NONE;
    s->cherry += delta;
    if (s->cherry > s->cherry_max) s->cherry = s->cherry_max;
    if (s->cherry < 0) s->cherry = 0;
    if (s->cherry >= s->cherry_max) s->ev_pending |= SENRO_EV_CHERRY_MAXED;   /* 0x42f6f4–0x42f72d 的飘字 */
    return s->ev_pending;
}

void senro_add_cherry_max(senro_state_t *s, int32_t delta)
{
    if (!s) return;
    s->cherry_max += delta;                   /* 0x42f799 `add eax,[ebp+0x8]` */
    if (s->cherry_max > SENRO_CHERRY_MAX_CAP) s->cherry_max = SENRO_CHERRY_MAX_CAP;  /* 0x42f7b1 */
    if (s->cherry_max < 0) s->cherry_max = 0;
}

/* ============================ 每帧 ============================ */

int senro_take_events(senro_state_t *s)
{
    int ev;
    if (!s) return SENRO_EV_NONE;
    ev = s->ev_pending;
    s->ev_pending = 0;
    return ev;
}

int senro_tick(senro_state_t *s, int bomb_active)
{
    if (!s) return SENRO_EV_NONE;
    s->bomb_active = bomb_active ? 1 : 0;

    /* 排队的那一轮：炸弹结束（或本来就没在放炸弹）就开门。 */
    if (s->queued && !s->active && !s->bomb_active) senro_border_open(s);

    if (s->active) {
        if (s->timer > 0) --s->timer;
        if (s->timer <= 0) {
            /* 自然结束（th07:0x441670，唯一调用点 0x441546 = `[player+0x16a08] <= 0` 分支）：
             * 最大樱点 +10000（0x44167a）→ 樱点 +10000（0x441689）→ 分数 += (樱点 − 基准)
             * （0x44169d–0x4416c6；末尾向下取整到 10）→ Plus = 基准（0x4416ee）。 */
            senro_add_cherry_max(s, SENRO_BORDER_END_ADD);
            senro_add_cherry_only(s, SENRO_BORDER_END_ADD);
            s->last_bonus = senro_round10(s->cherry);
            s->active = 0;
            s->queued = 0;
            s->timer = 0;
            s->plus = 0;
            ++s->stat_natural;
            s->ev_pending |= SENRO_EV_END_NATURAL;
        } else {
            /* 结界中每帧把 +C 重写成「剩余计时比例」（th07:0x4414db–0x44150e）——「持续下降」的实装。 */
            s->plus = senro_plus_for_timer(s->timer);
        }
    }
    return senro_take_events(s);
}

/* ============================ 擦弹（结界中） ============================ */

int32_t senro_graze(senro_state_t *s, int32_t n, int slow)
{
    int32_t v, total;

    /* th07:0x43ec83 / 0x43ec8a 的门：`[player+0x240d] == 1`（结界中）才走这一段。*/
    if (!s || !s->active || n <= 0) return 0;
    v = slow ? SENRO_GRAZE_SLOW : SENRO_GRAZE_FAST;
    total = v * n;
    senro_add_cherry_max(s, total);        /* 0x43ed7d/0x43ed9d → 0x42f789 */
    senro_add_cherry_only(s, total);      /* 0x43ed88/0x43eda2 → 0x42f736 */
    return total;                          /* 两条都不动 +C（一手） */
}

/* ============================ 道具 / 命中 ============================ */

int32_t senro_item_cherry(senro_state_t *s, uint32_t type)
{
    int32_t d;

    if (!s) return 0;
    if (type >= SENRO_TH18_ITEM_SCORE_LO && type <= SENRO_TH18_ITEM_SCORE_HI) {
        /* th07 **type 6**（★，`0x43370b`/`0x433712`，落在 type 6 块 `0x433691` 内）= 樱点 / +C 各 +20。
         * ⚠ 原注写作 type 7，2026-09-15 按跳表块归属订正（见 `senro_core.h`）。*/
        d = SENRO_STAR_CHERRY;
    } else {
        /* ⛔ **2026-09-30 第四轮（owner 口径，逐字）：「一手应该是收金钱（蓝点）/ P **不加**」**
         *   —— TH18 `type 2 = 金钱` 在本 mod 里就是「点道具 / 蓝点」的对位（`cards/trance.c` 同口径：
         *   `TRANCE_ITEM_TYPE18_POINT 2u`「= 本 mod 的『点道具/蓝点』对位」），而 TH07 的**蓝点**
         *   （type 1，handler `0x4330d2`）**根本不进樱点加法器**（它只算分）⇒ 收钱不该给樱点/+C。
         *   原来那条「金钱 → 大樱点 `1000 + 100×符卡奖励次数`」是**旧自定**（把金钱当樱点道具），
         *   本轮**撤掉**：大樱点仍由**自建樱点实体**（满 P 转换 `faith_convert_full_p_items()`）承担，
         *   收下时走 `senro_on_cherry_pickup()` 记同一笔值（`cards/senro.c`）。
         *   P（type 1）本来就不映射 ✓；命碎 / 符卡碎 / 残机 / 0xf 等一律 0（一手：TH07 那些 handler 都不调加法器）。*/
        (void)d;
        return 0;
    }
    ++s->stat_items;
    senro_add_cherry(s, d);
    return d;
}

void senro_field_layout(int32_t value, int digits, int *out_digit, int *out_draw)
{
    int32_t div = 1;
    int i, started = 0;

    if (digits <= 0) return;
    if (value < 0) value = 0;
    for (i = 1; i < digits; ++i) div *= 10;
    for (i = 0; i < digits; ++i, div /= 10) {
        int d = (div > 0) ? (int)((value / div) % 10) : 0;
        if (d != 0) started = 1;                      /* 一手 0x404a25/0x404a2b：只有非零位置 started */
        if (out_digit) out_digit[i] = d;
        if (out_draw)  out_draw[i]  = (started || div <= 1) ? 1 : 0;   /* 一手 0x404a32–0x404a3c */
    }
}

int32_t senro_hit(senro_state_t *s, int32_t damage, int32_t stage)
{
    int32_t p, k, v;

    if (!s || damage <= 0) return 0;
    /* 一手公式（逐条见 senro_core.h 的 `SENRO_HIT_*` 段）：
     *   p = stage >= 5 ? 10 : stage × 2（0x420638–0x420660）；k = 10 − p/3（`ds:0x4bfee3 == 0` 那支）；
     *   gain = (damage / k) × 10；0 → 10（0x421259）；上限 70（0x42121f）。*/
    if (stage < 0) stage = 0;
    p = (stage >= 5) ? 10 : stage * 2;
    k = SENRO_HIT_DIV - p / 3;
    if (k < 1) k = 1;
    v = (damage / k) * 10;
    if (v == 0) v = SENRO_HIT_MIN;
    if (v > SENRO_HIT_CAP) v = SENRO_HIT_CAP;
    ++s->stat_hits;
    senro_add_cherry(s, v);
    return v;
}

int32_t senro_point_item_value(const senro_state_t *s)
{
    int32_t v;

    if (!s) return SENRO_POINT_BASE;
    /* th07 type 1（蓝点，`0x4330db`–`0x4331e5`）：线上 = 50000（0x43310a），
     * 再 `+= (樱点 − 基准 − 50000) / 5`（0x4331c8 `idiv 5`），末尾向下取整到 10（0x4331db 起）。
     * ⚠ 本卡**不做** th07 的「线下按高度递减」（`0x43311f`–`0x433130` 的 `50000 − (y−线)×100`）——
     * TH18 的高度系数由原生身价承担，而改写身价会绕过它，见本卡 spec §4.2 的偏差表。 */
    v = SENRO_POINT_BASE;
    if (s->cherry > SENRO_POINT_BASE) v += (s->cherry - SENRO_POINT_BASE) / SENRO_POINT_DIV;
    return senro_round10(v);
}

/* ============================ 灵击 ============================ */

int32_t senro_small_cherry(senro_state_t *s, int32_t bullets)
{
    if (!s || bullets <= 0) return 0;
    /* 每颗的账 = th07 type 8 那两条调用：+30 走加法器（樱点与 +C 同加）、+70 只加樱点。 */
    s->stat_cancel += bullets;
    senro_add_cherry(s, bullets * SENRO_SMALL_CHERRY_ADD);
    senro_add_cherry_only(s, bullets * SENRO_SMALL_CHERRY_EXTRA);
    return bullets * (SENRO_SMALL_CHERRY_ADD + SENRO_SMALL_CHERRY_EXTRA);
}

int senro_libyou(senro_state_t *s, int32_t bullets)
{
    if (!s || !s->active) return 0;

    /* `0x441bd0` 的三件收尾：清结界状态（0x441d4f `[player+0x240d]=0`、0x441d5c `[player+0x2408]=3`）、
     * 40 帧无敌（0x441d8e `[player+0x23fc]=0x28`）、Plus = 基准（0x441da4）。 */
    s->active = 0;
    s->queued = 0;
    s->timer = 0;
    s->plus = 0;
    s->invuln = SENRO_INVULN_FRAMES;
    ++s->stat_libyou;

    /* 「全屏弹 → 小樱点」：th07 是先清弹、小樱点随后才被收走 ⇒ **清零在前、入账在后**
     *（否则这笔 +C 会被 0x441da4 的清零吃掉）。 */
    (void)senro_small_cherry(s, bullets);
    return 1;
}

/* ============================ Miss / Bomb 扣樱点 ============================ */

int32_t senro_penalty_miss(senro_state_t *s)
{
    int32_t d;

    if (!s) return 0;
    /* th07:0x440e84–0x440ef0：`cherry −= round10(min(trunc(cherry_rel × [SHT+0x1c]), CAP))`。
     * **不夹回基准**、**不动 +C / 最大樱点**（用户 2026-09-10 口径与一手一致）。 */
    d = (int32_t)(((int64_t)s->cherry * SENRO_MISS_NUM) / SENRO_MISS_DEN);
    if (d > SENRO_MISS_CAP) d = SENRO_MISS_CAP;      /* 0x440eb2 / 0x440ec4 的夹取（在取整之前）*/
    d = senro_round10(d);                            /* 0x440ed8 / 0x440ee2 */
    if (d > s->cherry) d = s->cherry;
    s->cherry -= d;
    if (s->cherry < 0) s->cherry = 0;
    return d;
}

int32_t senro_penalty_bomb(senro_state_t *s)
{
    int32_t d;

    if (!s) return 0;
    /* th07:0x440ad9：按额度 `[player+0x16a2c]`（= `(樱点 − 基准) × ratio`，ratio 0.17–0.41）扣，
     * 扣完**夹回基准**（0x440acc `jl` → 0x440aec）、**不动 +C / 最大樱点**。
     * ❓ 自定：TH18 不区分十二种炸弹，统一取 1/4（TODO(th07) 逐炸弹取值 —— review §9.3 的 0.17–0.41 表）。 */
    d = (int32_t)(((int64_t)s->cherry * SENRO_BOMB_NUM) / SENRO_BOMB_DEN);
    d = senro_round10(d);
    if (d > s->cherry) d = s->cherry;
    s->cherry -= d;
    if (s->cherry < 0) s->cherry = 0;
    return d;
}
