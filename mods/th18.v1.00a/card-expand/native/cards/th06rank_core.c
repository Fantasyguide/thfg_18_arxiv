/* th06rank_core.c —— 系统卡 106「红魔乡」的纯逻辑实现。单位与出处见 th06rank_core.h 文件头。 */
#include "th06rank_core.h"

/* ── 难度表 ────────────────────────────────────────────────────────────────
 * th06:GameManager.cpp:70-76 `DIFFABLE_STATIC_ARRAY_ASSIGN(DifficultyInfo, 5, g_DifficultyInfo)` 逐行照抄：
 *     // rank, minRank, maxRank
 *     EASY    {16, 12, 20}
 *     NORMAL  {16, 10, 32}
 *     HARD    {16, 10, 32}
 *     LUNATIC {16, 10, 32}
 *     EXTRA   {16, 14, 18}
 * ⚠ EASY 下限是 **12**：用户早期来源写 10 是错的（spec-review §5.1 与 §12 都订正过）。
 * 另有 `g_DifficultyInfoForReplay`（同值）供 replay 用 —— 本卡不分 replay（rank 不进录像）。 */
static const t06_difficulty_t k_diff[T06_DIFF_COUNT] = {
    /* EASY    */ { 16, 12, 20 },
    /* NORMAL  */ { 16, 10, 32 },
    /* HARD    */ { 16, 10, 32 },
    /* LUNATIC */ { 16, 10, 32 },
    /* EXTRA   */ { 16, 14, 18 },
};

const t06_difficulty_t *t06_difficulty_table(void) { return k_diff; }

static int diff_index(int32_t difficulty)
{
    if (difficulty < 0 || difficulty >= T06_DIFF_COUNT) return T06_DIFF_NORMAL;
    return (int)difficulty;
}

/* ── rank 增减：逐字照 th06:GameManager.cpp:578-603 ─────────────────────────
 *   void IncreaseSubrank(amount) { subRank += amount;
 *       while (subRank >= 100) { rank++; subRank -= 100; }
 *       if (rank > maxRank) rank = maxRank; }              // 小数部分不修正
 *   void DecreaseSubrank(amount) { subRank -= amount;
 *       while (subRank < 0) { rank--; subRank += 100; }
 *       if (rank < minRank) rank = minRank; }              // 小数部分不修正
 * 唯一改动 = 进位阈值 100 → T06_SUBRANK_CARRY（PERMILLE，见头文件「单位」）。
 * 「小数部分不修正」是照抄的：跨过上下限时 `subRank` **原样留着**
 * （原版在 max 处继续攒小数、在 min 处继续攒负债），我们不去动它。 */
void t06_init(t06_rank_t *r, int32_t difficulty)
{
    const t06_difficulty_t *d = &k_diff[diff_index(difficulty)];
    r->rank = d->rank;
    r->min_rank = d->min_rank;
    r->max_rank = d->max_rank;
    r->sub_rank = 0;
}

int32_t t06_increase(t06_rank_t *r, int32_t amount)
{
    const int32_t before = r->rank;
    r->sub_rank += amount;
    while (r->sub_rank >= T06_SUBRANK_CARRY) {
        r->rank++;
        r->sub_rank -= T06_SUBRANK_CARRY;
    }
    if (r->rank > r->max_rank) r->rank = r->max_rank;
    return r->rank - before;
}

int32_t t06_decrease(t06_rank_t *r, int32_t amount)
{
    const int32_t before = r->rank;
    r->sub_rank -= amount;
    while (r->sub_rank < 0) {
        r->rank--;
        r->sub_rank += T06_SUBRANK_CARRY;
    }
    if (r->rank < r->min_rank) r->rank = r->min_rank;
    return r->rank - before;
}

/* ── 帧数条件 ─────────────────────────────────────────────────────────────
 * th06:EnemyManager.cpp:162-170（一手）：
 *     subrankIncreaseFrame = 10*4*60;                              // 2400
 *     subrankIncreaseFrame -= g_GameManager.livesRemaining * 4*60; // − 残机数 × 240
 *     if (timelineTime.HasTicked() && timelineTime.AsFrames() % subrankIncreaseFrame == 0)
 *         IncreaseSubrank(100);
 * 且整段被 `g_Gui.HasCurrentMsgIdx() == 0`（没有对话框时才算）门控。
 * 残机钳 [0,10]：`livesRemaining` 在 th06 上限 8、TH18 上限 7，取 10 是防 `interval <= 0`
 * 的整数除零（原版没这条保护，因为它的残机上限恒小于 10）。 */
int t06_frame_interval(int32_t lives)
{
    if (lives < 0) lives = 0;
    if (lives > 10) lives = 10;
    return T06_FRAME_BASE - lives * T06_FRAME_PER_LIFE;
}

int t06_frame_tick_due(int32_t stage_frame, int32_t lives)
{
    const int interval = t06_frame_interval(lives);
    if (interval <= 0) return 0;                 /* 不可能（钳过），防御性 */
    if (stage_frame == 0) return 0;              /* `HasTicked()`：第 0 帧不算 */
    return (stage_frame % interval) == 0;
}

/* ── 蓝点 / 小 P ─────────────────────────────────────────────────────────── */
int32_t t06_point_gain(int collected_at_poc)
{
    /* th06 ItemManager.cpp:234/238 = Inc(30) / Inc(3)（单位 1/100）→ 本卡 ÷5（用户 2026-09-10 定：
     * TH18 道具量 ≈ 5× th06）。PERMILLE 下 0.30/0.03 → **6000 / 600**（= 0.06 / 0.006 rank）。 */
    return (collected_at_poc ? T06_INC_POINT_HIGH : T06_INC_POINT_LOW) / T06_RATIO_DIV;
}

int32_t t06_power_small_gain(int owned_110_or_111)
{
    /* 小 P 是 th06 Inc(1) = +0.01 rank = 1000 PERMILLE → ÷5 = +0.002（= **200 PERMILLE**）。
     * spec §8：装了 110（风神录）或 111（地灵殿）时那两张卡把 P 点改成 +0.05 火力 = 5×
     * ⇒ 本卡的小 P rank 也 ×5（+0.002 → +0.01）。判据是 SDK 现成的 `ce_owned()` 存在性查询，
     * 不算跨卡耦合（spec-review §3.1 的例外条）。 */
    int32_t v = T06_INC_POWER_SMALL / T06_RATIO_DIV;
    if (owned_110_or_111) v *= T06_RATIO_DIV;
    return v;
}

/* ── item type 分派（TH18 item type；spec §3.1 一手全表） ────────────────────
 * type 1 小 P / 2 金钱（== 蓝点）/ 3 大 P / 4 命碎 / 5 整命 / 6 B碎 / 7 整炸 /
 * 8 满 P 转分 / 9–0xe 分数转换点族 / 0x10 卡道具残碎 / 0x11 卡道具 B碎 /
 * 0x12 金钱+50 / 0x13 火力+0.5 / 其余未知。
 * ⚠ 4/6 与 0x10/0x11 在这里是**同一个动作**：0x10/0x11 是「卡道具」，掉出来变成
 * 卡 id 3/4（残碎 / B碎），进的是同一条碎片通道 ⇒ 同一颗 +0.70 / +0.02（spec §3.1）。
 * ⚠ 5 整命 / 7 整炸**不在这里**：它们走库存增量（商店买 card 1/2 不过 on_item_collect，
 * 见 spec §4.1），所以这里返回 IGNORE。3 大 P 是 th06 源码确认的「无调用」⇒ 不加。 */
t06_item_action_t t06_item_action(uint32_t type)
{
    switch (type) {
    case 1:    return T06_ITEM_POWER_SMALL;   /* P 点 */
    case 2:    return T06_ITEM_POINT;         /* 金钱 == 蓝点 */
    case 4:                                   /* 命碎 */
    case 0x10: return T06_ITEM_LIFE_FRAG;     /* 卡道具 → card id 3 残碎，同一条通道 */
    case 6:                                   /* B 碎 */
    case 0x11: return T06_ITEM_BOMB_FRAG;     /* 卡道具 → card id 4 B碎 */
    default:   return T06_ITEM_IGNORE;
    }
}

/* ── 状态 ────────────────────────────────────────────────────────────────── */

static void window_clear(t06_state_t *s)
{
    s->life_frag_delta = 0;
    s->bomb_frag_delta = 0;
}

void t06_state_init(t06_state_t *s, int32_t difficulty)
{
    t06_init(&s->rank, difficulty);
    s->stage_frame = 0;
    window_clear(s);
    s->life_stock_last = s->life_frag_last = 0;
    s->bomb_stock_last = s->bomb_frag_last = 0;
    s->seeded = 0;
}

void t06_state_stage_reset(t06_state_t *s)
{
    /* th06:GameManager.cpp:357 `mgr->subRank = 0;`（每关开场）+ `timelineTime` 是新对象（从 0 起）。
     * rank 本身**不带**回来：th06 的 rank/minRank/maxRank 只在「通关 / 开新 run」时按难度表重设
     * （GameManager.cpp:345-347 在 stage-clear 分支里），关卡之间**保留**。 */
    s->rank.sub_rank = 0;
    s->stage_frame = 0;
    window_clear(s);
    s->seeded = 0;   /* 新关首帧 commit 只对账：不把上一关末帧到本关首帧的库存变动补记 */
}

void t06_tick_frame(t06_state_t *s)
{
    ++s->stage_frame;
}

int32_t t06_stage_frame(const t06_state_t *s) { return s->stage_frame; }

void t06_window_item_life_fragment(t06_state_t *s, int delta) { s->life_frag_delta += delta; }
void t06_window_item_bomb_fragment(t06_state_t *s, int delta) { s->bomb_frag_delta += delta; }

int32_t t06_commit_frame(t06_state_t *s, int life_stock, int life_frag, int bomb_stock, int bomb_frag)
{
    int32_t gained = 0;
    if (!s->seeded) {           /* 首帧 / 每关首帧：只建立基准 */
        s->seeded = 1;
    } else {
        int32_t d_life = life_stock - s->life_stock_last;
        int32_t d_bomb = bomb_stock - s->bomb_stock_last;
        /* ★ 2026-09-12 审计：`d_* × T06_INC_*_FULL` 是 int32 乘法，库存读数来自引擎全局
         * ⇒ 异常大的一次跳变（商店批量 / 局末 recount / 读到的内存不干净）会**有符号溢出（UB）**。
         * 钳到 ±100 档就是 ±200 rank：语义上早已被夹到 min/max，绝不改变可观察行为，
         * 只是把 UB 换成饱和。正常游玩下 d 恒为 ±1。 */
        if (d_life > 100) d_life = 100;
        if (d_life < -100) d_life = -100;
        if (d_bomb > 100) d_bomb = 100;
        if (d_bomb < -100) d_bomb = -100;
        /* ★ 碎片会计（spec §4.1 + 订正 §5.1）：整命 / 整炸只在**本窗口碎片没有净减**时才算
         * 「收到整合资源」。碎片净减 = 这一窗口里发生了进位（计数器 2→3→0，净 −2，同时
         * CURRENT_LIVES +1）⇒ 那一命是碎片换来的，+2.00 要抑制；碎片那颗自己的 +0.70
         * 已经在 on_item_collect 里逐颗计过（含进位的那一颗）。 */
        if (d_life > 0 && s->life_frag_delta >= 0) gained += t06_increase(&s->rank, d_life * T06_INC_LIFE_FULL);
        if (d_bomb > 0 && s->bomb_frag_delta >= 0) gained += t06_increase(&s->rank, d_bomb * T06_INC_BOMB_FULL);
        /* 负向增量（Miss 扣命 / 商店扣炸 / 引擎回收）不在这里扣：Miss 走 on_death_after_deathbomb
         * 的 −16.00（th06 是「死亡结算的那一刻」，不是「库存少了一」），Bomb 走 on_bomb_spent 的
         * −2.00。两边都扣会双记。所以这里只看正向。 */
    }
    s->life_stock_last = life_stock;
    s->life_frag_last = life_frag;
    s->bomb_stock_last = bomb_stock;
    s->bomb_frag_last = bomb_frag;
    window_clear(s);
    return gained;
}
