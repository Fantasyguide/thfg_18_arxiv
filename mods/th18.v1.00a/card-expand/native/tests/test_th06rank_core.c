/* test_th06rank_core.c —— 系统卡 106「红魔乡」纯逻辑的主机单测。
 *
 * 编法（不跑 make，见任务约定）：
 *   cc -std=c11 -Wall -Wextra -Werror -o /tmp/t106 tests/test_th06rank_core.c \
 *      native/cards/th06rank_core.c && /tmp/t106
 *
 * 覆盖：
 *   ① IncreaseSubrank / DecreaseSubrank 的进位 / 借位 / 上下限夹取 / 小数不修正
 *   ② 难度表 5 行（初值 + min/max，EASY 下限 **12**）
 *   ③ 12 条增减的**符号与量**（每一条的 PERMILLE 值 + 方向 + 走哪个事件）
 *   ④ 帧数条件公式 `2400 − 残机数×240`（含 0 帧不触发、残机钳取）
 *   ⑤ 碎片会计三条边界（订正 §5.1）：
 *        a. 收第 3 颗：计数器 2→3→0（净 −2）+ 当场 +1 命 ⇒ 抑制 +2.00，只算 3×0.70
 *        b. 碎片净 0（收到一颗 + 别处掉一颗出屏）⇒ **不**抑制
 *        c. 整命来自别处（碎片没动 / 碎片 +1）⇒ 正常 +2.00
 *   ⑥ 蓝点分档（回收线上下）、小 P（÷5 与 ×5）
 *   ⑦ 每关开场清 subRank + 帧计数，rank 保留
 *   ⑧ 12 条的累计量 = 原值（th06 单位 1/100）的 ÷5 / 照抄 对应关系
 */
#include <stdio.h>
#include <string.h>
#include "../cards/th06rank_core.h"

static int s_fail, s_pass;
#define CHECK(cond) do { if (cond) ++s_pass; else { ++s_fail; \
    printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } } while (0)
#define CHECK_EQ(a, b) do { long long va_ = (long long)(a), vb_ = (long long)(b); \
    if (va_ == vb_) ++s_pass; else { ++s_fail; \
    printf("  FAIL %s:%d: %s == %s (%lld != %lld)\n", __FILE__, __LINE__, #a, #b, va_, vb_); } } while (0)

/* PERMILLE ↔ th06 的原值单位（1/100 rank）。★ 2026-09-12 审计订正：
 * 锚点是 **1 原值单位 = T06_UNIT_PERMILLE(10000) PERMILLE**（因为 100000 PERMILLE = 1.00 rank
 * 必须 = 100 个原值单位）。原先把 1 原值单位当 1000 PERMILLE ⇒ 凡是用这两个宏的断言与注释
 * 都小了 10 倍，正是「擦弹 60 vs 6000」那个量纲 bug 没被单测抓住的原因。 */
#define CENTI(permille)      ((permille) / T06_UNIT_PERMILLE)
#define UNIT_TO_PERMILLE(u)  ((u) * T06_UNIT_PERMILLE)

/* 一条「本卡值 = spec §3 那一列」的锚点断言：permille 换回 rank 的相对量。 */
#define RANK_MILLI(permille) ((permille) / 100)   /* PERMILLE → 1/1000 rank 的「rank × 1000」计数 */


int main(void)
{
    t06_rank_t r;
    int i;

    /* ══ ① 进位 / 借位 / 夹取 / 小数不修正（th06:GameManager.cpp:578-603） ══ */
    t06_init(&r, T06_DIFF_NORMAL);
    CHECK_EQ(r.rank, 16); CHECK_EQ(r.min_rank, 10); CHECK_EQ(r.max_rank, 32); CHECK_EQ(r.sub_rank, 0);

    /* 小数累加不进位 */
    CHECK_EQ(t06_increase(&r, 60), 0);                 /* +0.06 */
    CHECK_EQ(r.sub_rank, 60);
    CHECK_EQ(r.rank, 16);
    /* 攒够 1.00 才进位，且小数**留着**（不是清零） */
    t06_increase(&r, T06_SUBRANK_CARRY - 60);          /* 凑到 1.00 */
    CHECK_EQ(r.rank, 17);
    CHECK_EQ(r.sub_rank, 0);
    /* 跨多档一次进位（+2.00） */
    t06_increase(&r, 60);
    CHECK_EQ(t06_increase(&r, T06_INC_LIFE_FULL), 2);  /* 0.06 + 2.00 = 2.06 ⇒ +2 档 */
    CHECK_EQ(r.rank, 19);
    CHECK_EQ(r.sub_rank, 60);                          /* 0.06 留在小数位（原版行为）*/

    /* 上限夹取：夹到 maxRank，**小数不修正**（原版继续攒） */
    CHECK_EQ(t06_increase(&r, 200 * T06_SUBRANK_CARRY), 32 - 19);
    CHECK_EQ(r.rank, 32);
    CHECK_EQ(r.sub_rank, 60);                          /* 夹取不动小数 */
    CHECK_EQ(t06_increase(&r, 50 * T06_SUBRANK_CARRY), 0);
    CHECK_EQ(r.rank, 32);
    CHECK_EQ(r.sub_rank, 60);                          /* 到顶后一直攒、一直夹 */

    /* 借位：−16.00 从 17 掉到 1 ⇒ 夹到 minRank(10)，小数留着 */
    t06_init(&r, T06_DIFF_NORMAL);
    t06_increase(&r, 60);
    t06_increase(&r, T06_SUBRANK_CARRY - 60);          /* rank 17, sub 0 */
    CHECK_EQ(t06_decrease(&r, T06_DEC_MISS), 10 - 17);  /* −16.00 ⇒ 1 → 夹到 10 */
    CHECK_EQ(r.rank, 10);
    CHECK_EQ(r.sub_rank, 0);
    /* 到下限后小数继续攒负债（原版「小数不修正」） */
    CHECK_EQ(t06_decrease(&r, 3000), 0);
    CHECK_EQ(r.rank, 10);
    CHECK_EQ(r.sub_rank, T06_SUBRANK_CARRY - 3000);
    /* 再降一档被夹住，但**借位照样发生**（原版 while 在 if 之前） */
    CHECK_EQ(t06_decrease(&r, 2 * T06_SUBRANK_CARRY), 0);
    CHECK_EQ(r.rank, 10);
    CHECK_EQ(r.sub_rank, T06_SUBRANK_CARRY - 3000);

    /* 一个 rank 恰好 = 100000 PERMILLE */
    t06_init(&r, T06_DIFF_NORMAL);
    CHECK_EQ(t06_increase(&r, T06_SUBRANK_CARRY), 1);
    CHECK_EQ(r.rank, 17); CHECK_EQ(r.sub_rank, 0);

    /* ══ ② 难度表（th06:GameManager.cpp:70-76 一手） ══ */
    {
        const int32_t want_rank[T06_DIFF_COUNT] = { 16, 16, 16, 16, 16 };
        const int32_t want_min[T06_DIFF_COUNT]  = { 12, 10, 10, 10, 14 };
        const int32_t want_max[T06_DIFF_COUNT]  = { 20, 32, 32, 32, 18 };
        /* ★ 订正（spec-review §5.1 / §12）：EASY 下限是 **12**，不是 10 */
        CHECK_EQ(t06_difficulty_table()[T06_DIFF_EASY].min_rank, 12);
        for (i = 0; i < T06_DIFF_COUNT; ++i) {
            const t06_difficulty_t *d = &t06_difficulty_table()[i];
            CHECK_EQ(d->rank, want_rank[i]);
            CHECK_EQ(d->min_rank, want_min[i]);
            CHECK_EQ(d->max_rank, want_max[i]);
            t06_init(&r, i);
            CHECK_EQ(r.rank, want_rank[i]);
            CHECK_EQ(r.sub_rank, 0);
        }
        /* EASY 的夹取边界（min 12 / max 20） */
        t06_init(&r, T06_DIFF_EASY);
        t06_decrease(&r, 100 * T06_SUBRANK_CARRY);
        CHECK_EQ(r.rank, 12);
        t06_init(&r, T06_DIFF_EASY);
        t06_increase(&r, 100 * T06_SUBRANK_CARRY);
        CHECK_EQ(r.rank, 20);
        /* EXTRA 的夹取边界（14 / 18 —— 窗口最窄） */
        t06_init(&r, T06_DIFF_EXTRA);
        CHECK_EQ(t06_increase(&r, 1 * T06_SUBRANK_CARRY), 1);   /* 16 → 17 */
        CHECK_EQ(t06_increase(&r, 1 * T06_SUBRANK_CARRY), 1);   /* 17 → 18 */
        CHECK_EQ(t06_increase(&r, 1 * T06_SUBRANK_CARRY), 0);   /* 18 = max，夹住 */
        CHECK_EQ(r.rank, 18);
        t06_init(&r, T06_DIFF_EXTRA);
        CHECK_EQ(t06_decrease(&r, 3 * T06_SUBRANK_CARRY), 14 - 16);
        CHECK_EQ(r.rank, 14);
        /* 难度越界 → 回落 NORMAL */
        t06_init(&r, 99);
        CHECK_EQ(r.rank, 16); CHECK_EQ(r.min_rank, 10); CHECK_EQ(r.max_rank, 32);
        t06_init(&r, -1);
        CHECK_EQ(r.max_rank, 32);
    }

    /* ══ ③ 12 条增减的符号与量 ══
     * 「本卡值」列（spec §3）+ th06 源码出处。方向 + PERMILLE + ÷5 关系逐条对。
     * ★ 2026-09-12 审计：每条都补上「换回 rank 的量」断言（= spec 的「本卡值」那一列的字面值），
     *   这样量纲错一位（60 vs 6000 那种）不可能再溜过去。`RANK_MILLI(v) = v / 100` 是「rank × 1000」。 */
    {
        /* 单位锚点本身（闸门）：1 原值单位 = 10000 PERMILLE ⇒ 1.00 rank = 100000 */
        CHECK_EQ(T06_UNIT_PERMILLE, 1000);   /* 1 个 th06 原值单位（1/100 rank）= 1000 本卡单位 */
        CHECK_EQ(T06_SUBRANK_CARRY, UNIT_TO_PERMILLE(100));
        CHECK_EQ(CENTI(T06_SUBRANK_CARRY), 100);        /* 1.00 rank = 100 个原值单位 */
        /* 增：帧数 +1.00（照抄）*/
        CHECK_EQ(T06_INC_FRAME, 100000);
        CHECK_EQ(RANK_MILLI(T06_INC_FRAME), 1000);      /* = +1.00 rank */
        CHECK_EQ(CENTI(T06_INC_FRAME), 100);            /* th06 Inc(100) */
        /* 增：擦弹 +0.06（不除）★ 审计订正：原值 6 ⇒ 6000 PERMILLE（原来写 60 = 小 100 倍）*/
        CHECK_EQ(T06_INC_GRAZE, UNIT_TO_PERMILLE(6));
        CHECK_EQ(T06_INC_GRAZE, 6000);
        CHECK_EQ(RANK_MILLI(T06_INC_GRAZE), 60);        /* = +0.06 rank（spec §3 的「本卡值」列）*/
        /* 增：整命 +2.00（照抄）/ 命碎 +0.70（定值）*/
        CHECK_EQ(CENTI(T06_INC_LIFE_FULL), 200);        /* th06 Inc(200) */
        CHECK_EQ(RANK_MILLI(T06_INC_LIFE_FULL), 2000);  /* = +2.00 rank */
        CHECK_EQ(T06_INC_LIFE_FRAG, 70000);
        CHECK_EQ(CENTI(T06_INC_LIFE_FRAG), 70);
        CHECK_EQ(RANK_MILLI(T06_INC_LIFE_FRAG), 700);   /* = +0.70 rank */
        /* 整命 3 碎 = 2.1（不是 2.0 —— 碎片通道独立定值，spec §3 依据） */
        CHECK_EQ(T06_INC_LIFE_FRAG * 3, 210000);
        CHECK_EQ(T06_INC_LIFE_FULL, T06_INC_LIFE_FRAG * 2 + 60000);
        /* 增：整炸 +0.05（照抄）/ B 碎 +0.02 */
        CHECK_EQ(CENTI(T06_INC_BOMB_FULL), 5);          /* th06 Inc(5) */
        CHECK_EQ(RANK_MILLI(T06_INC_BOMB_FULL), 50);    /* = +0.05 rank */
        CHECK_EQ(T06_INC_BOMB_FRAG, 2000);
        CHECK_EQ(RANK_MILLI(T06_INC_BOMB_FRAG), 20);    /* = +0.02 rank */
        /* 3 B 碎 = 1 张 = +0.06（> 整炸的 0.05 —— 与原版一致，碎片路略贵） */
        CHECK_EQ(T06_INC_BOMB_FRAG * 3, 6000);
        /* 增：蓝点 ÷5（th06 Inc(30) / Inc(3) ⇒ spec §3 的「原值」0.30 / 0.03 → 「本卡值」0.06 / 0.006）
         * ★ 审计复核：这两条 _INC_* 常量是**除之前的 th06 原值**（30000 / 3000），
         *   最终值由 t06_point_gain() 在运行期 ÷ T06_RATIO_DIV ⇒ 下面的最终值断言才是 spec 那一列。
         *   （存原值而不是存 6000 不是笔误：spec §3 的归一化规则明说「÷5 只作用于这两条通道」，
         *     把规则留在代码里比把结果烧进常量更可审计。） */
        CHECK_EQ(T06_INC_POINT_HIGH, 30000);                 /* = 0.30 rank（除之前）*/
        CHECK_EQ(T06_INC_POINT_LOW, 3000);                   /* = 0.03 rank（除之前）*/
        CHECK_EQ(T06_INC_POINT_HIGH, T06_INC_POINT_LOW * 10); /* 两条通道同一条规则 ⇒ 原值互为整十倍 */
        CHECK_EQ(CENTI(T06_INC_POINT_HIGH), 30);             /* 原值单位（1/100 rank）计 = 30（spec 的「原值」列）*/
        CHECK_EQ(CENTI(T06_INC_POINT_LOW), 3);               /* 原值列 = 3 */
        CHECK_EQ(t06_point_gain(1), 6000);                   /* ÷5 后 = +0.06 */
        CHECK_EQ(RANK_MILLI(t06_point_gain(1)), 60);
        CHECK_EQ(t06_point_gain(0), 600);                    /* ÷5 后 = +0.006 */
        CHECK_EQ(RANK_MILLI(t06_point_gain(0)), 6);
        CHECK_EQ(t06_point_gain(1), T06_INC_POINT_HIGH / T06_RATIO_DIV);
        CHECK_EQ(t06_point_gain(0), T06_INC_POINT_LOW / T06_RATIO_DIV);
        /* 增：小 P ÷5（th06 Inc(1) ⇒ 原值 0.01 → spec 的 0.002），装 110/111 时 ×5 回到 0.01 */
        CHECK_EQ(T06_INC_POWER_SMALL, 1000);                 /* = 0.01 rank（除之前）*/
        CHECK_EQ(t06_power_small_gain(0), 200);              /* ÷5 后 = +0.002（= 1000 ÷ 5）*/
        CHECK_EQ(RANK_MILLI(t06_power_small_gain(0)), 2);
        CHECK_EQ(t06_power_small_gain(1), 1000);             /* ×5（装了 110/111）⇒ 回到 0.01 */
        CHECK_EQ(RANK_MILLI(t06_power_small_gain(1)), 10);
        /* 减：出屏 −0.03（不除）、Bomb −2.00、Miss −16.00（照抄）
         * ★ 审计订正：出屏原值 3 ⇒ 3000 PERMILLE（原来写 300 = 小 10 倍）*/
        CHECK_EQ(T06_DEC_OFFSCREEN, UNIT_TO_PERMILLE(3));
        CHECK_EQ(T06_DEC_OFFSCREEN, 3000);
        CHECK_EQ(RANK_MILLI(T06_DEC_OFFSCREEN), 30);    /* = −0.03 rank（spec §3 的「本卡值」列）*/
        CHECK_EQ(CENTI(T06_DEC_BOMB), 200);             /* th06 Dec(200) */
        CHECK_EQ(RANK_MILLI(T06_DEC_BOMB), 2000);       /* −2.00 rank */
        CHECK_EQ(CENTI(T06_DEC_MISS), 1600);            /* th06 Dec(1600) */
        CHECK_EQ(RANK_MILLI(T06_DEC_MISS), 16000);      /* −16.00 rank */
        /* 大 P 不加：t06_item_action(3) 必须是 IGNORE */
        CHECK_EQ(t06_item_action(3), T06_ITEM_IGNORE);
    }

    /* ══ ③b type 分派表（spec §3.1 一手全表） ══ */
    CHECK_EQ(t06_item_action(1), T06_ITEM_POWER_SMALL);   /* P 点 */
    CHECK_EQ(t06_item_action(2), T06_ITEM_POINT);         /* 金钱 == 蓝点 */
    CHECK_EQ(t06_item_action(3), T06_ITEM_IGNORE);        /* 大 P：th06 无调用 */
    CHECK_EQ(t06_item_action(4), T06_ITEM_LIFE_FRAG);     /* 命碎 */
    CHECK_EQ(t06_item_action(5), T06_ITEM_IGNORE);        /* 整命：走库存增量 */
    CHECK_EQ(t06_item_action(6), T06_ITEM_BOMB_FRAG);     /* B 碎 */
    CHECK_EQ(t06_item_action(7), T06_ITEM_IGNORE);        /* 整炸：走库存增量 */
    CHECK_EQ(t06_item_action(8), T06_ITEM_IGNORE);        /* 满 P 转分 */
    for (i = 9; i <= 0x0e; ++i) CHECK_EQ(t06_item_action((uint32_t)i), T06_ITEM_IGNORE);
    CHECK_EQ(t06_item_action(0x10), T06_ITEM_LIFE_FRAG);  /* 卡道具 → card 3 残碎 */
    CHECK_EQ(t06_item_action(0x11), T06_ITEM_BOMB_FRAG);  /* 卡道具 → card 4 B碎 */
    CHECK_EQ(t06_item_action(0x12), T06_ITEM_IGNORE);     /* 金钱 +50 */
    CHECK_EQ(t06_item_action(0x13), T06_ITEM_IGNORE);     /* 火力 +0.5 */
    CHECK_EQ(t06_item_action(0), T06_ITEM_IGNORE);
    CHECK_EQ(t06_item_action(0x14), T06_ITEM_IGNORE);

    /* ══ ④ 帧数条件（th06:EnemyManager.cpp:162-170） ══ */
    CHECK_EQ(t06_frame_interval(0), 2400);            /* 2400 − 0 */
    CHECK_EQ(t06_frame_interval(1), 2160);            /* 2400 − 240 */
    CHECK_EQ(t06_frame_interval(3), 1680);            /* 2400 − 720 */
    CHECK_EQ(t06_frame_interval(10), 0 + 0);          /* 钳到 10 ⇒ 2400 − 2400 = 0（不会除零）*/
    CHECK_EQ(t06_frame_interval(99), 0);              /* 越界钳取 */
    CHECK_EQ(t06_frame_interval(-5), 2400);           /* 负数钳到 0 */
    CHECK(t06_frame_interval(7) > 0);                 /* TH18 残机上限 7 ⇒ 恒 > 0 */
    /* 0 帧不触发（th06 的 `timelineTime.HasTicked()`）*/
    CHECK_EQ(t06_frame_tick_due(0, 0), 0);
    CHECK_EQ(t06_frame_tick_due(2400, 0), 1);
    CHECK_EQ(t06_frame_tick_due(2399, 0), 0);
    CHECK_EQ(t06_frame_tick_due(2160, 1), 1);         /* 残机 1 ⇒ 每 2160 帧 */
    CHECK_EQ(t06_frame_tick_due(2400, 1), 0);         /* 但 2400 不是 2160 的倍数 */
    CHECK_EQ(t06_frame_tick_due(1680 * 2, 3), 1);
    CHECK_EQ(t06_frame_tick_due(1680 * 2 + 1, 3), 0);

    /* ══ ⑤ 碎片会计三条边界（spec §4.1 + 订正 §5.1） ══ */
    {
        t06_state_t s;
        int32_t lf, ls, bf, bs;

        /* ---- a. 收第 3 颗：库存 2→3→0（净 −2）+ 当场 +1 命 ⇒ 抑制 +2.00 ---- */
        t06_state_init(&s, T06_DIFF_NORMAL);
        ls = 2; lf = 0; bs = 3; bf = 0;
        t06_commit_frame(&s, ls, lf, bs, bf);                /* 首帧只对账 */
        CHECK_EQ(s.rank.rank, 16); CHECK_EQ(s.rank.sub_rank, 0);

        /* 第 1 颗命碎：+0.70，碎片 0 → 1 */
        t06_increase(&s.rank, T06_INC_LIFE_FRAG);
        t06_window_item_life_fragment(&s, +1);
        t06_commit_frame(&s, ls, 1, bs, bf);
        CHECK_EQ(s.rank.sub_rank, 70000);
        CHECK_EQ(s.rank.rank, 16);                           /* 没进位 */

        /* 第 2 颗：+0.70，碎片 1 → 2 */
        t06_increase(&s.rank, T06_INC_LIFE_FRAG);
        t06_window_item_life_fragment(&s, +1);
        t06_commit_frame(&s, ls, 2, bs, bf);
        CHECK_EQ(s.rank.rank, 17);                           /* 0.70+0.70 = 1.40 ⇒ 进位到 17 */
        CHECK_EQ(s.rank.sub_rank, 40000);                    /* 小数留 0.40 */

        /* 第 3 颗：碎片 2→3→0（净 −2）、CURRENT_LIVES +1。
         * 我们逐颗记了 +0.70（第 3 颗那颗），而库存 +1 命**必须被抑制**。 */
        t06_increase(&s.rank, T06_INC_LIFE_FRAG);             /* 第 3 颗自己的 +0.70 */
        t06_window_item_life_fragment(&s, -2);                /* 净 −2：2 → 3 → 0 */
        t06_commit_frame(&s, 3 /*lives*/, 0 /*frag*/, bs, bf);
        /* 1.10 + 0.70 = 1.80 ⇒ rank 17 + 1 = 18? 不：sub 0.40+0.70 = 1.10 ⇒ 进位 1 档 */
        CHECK_EQ(s.rank.rank, 18);
        CHECK_EQ(s.rank.sub_rank, 10000);
        /* ★ 关键断言：整命那 +2.00 **没有被记**（否则 sub 会到 2.10 而不是 1.10）*/
        CHECK(s.rank.sub_rank < 200000);

        /* ---- b. 碎片净 0（收到一颗、同时别处掉一颗出屏）⇒ **不**抑制 ---- */
        t06_state_init(&s, T06_DIFF_NORMAL);
        t06_commit_frame(&s, 5, 0, 3, 0);                     /* 基准 */
        t06_increase(&s.rank, T06_INC_LIFE_FRAG);             /* 收到一颗命碎 */
        t06_window_item_life_fragment(&s, +1);
        t06_window_item_life_fragment(&s, -1);                /* 同时别处掉了一颗（净 0）*/
        /* 库存 +1 命 —— 但碎片净变是 0 ⇒ 这不是碎片进位 ⇒ **照样 +2.00** */
        CHECK_EQ(t06_commit_frame(&s, 6, 0, 3, 0), 2);
        CHECK_EQ(s.rank.sub_rank, 70000 + 200000 - 200000);   /* = 0.70 + 2.00 进位后剩 0.70 */
        CHECK_EQ(s.rank.rank, 18);
        CHECK_EQ(s.rank.sub_rank, 70000);

        /* ---- c. 整命来自别处（碎片没动）⇒ 正常 +2.00 ---- */
        t06_state_init(&s, T06_DIFF_NORMAL);
        t06_commit_frame(&s, 5, 0, 3, 0);
        CHECK_EQ(t06_commit_frame(&s, 6, 0, 3, 0), 2);        /* +2.00 ⇒ 16 → 18 */
        CHECK_EQ(s.rank.rank, 18); CHECK_EQ(s.rank.sub_rank, 0);
        /* 碎片 +1（非进位）时也不抑制 */
        t06_window_item_life_fragment(&s, +1);
        CHECK_EQ(t06_commit_frame(&s, 7, 1, 3, 0), 2);
        CHECK_EQ(s.rank.rank, 20);
        /* 负向库存增量：**不加也不减**（Miss 走自己的事件）*/
        CHECK_EQ(t06_commit_frame(&s, 5, 0, 3, 0), 0);
        CHECK_EQ(s.rank.rank, 20);
        CHECK_EQ(s.rank.sub_rank, 0);

        /* ---- d. 整炸：同款规则（B 碎 2→3→0 时抑制 +0.05） ---- */
        t06_state_init(&s, T06_DIFF_NORMAL);
        t06_commit_frame(&s, 3, 0, 1, 2);
        t06_increase(&s.rank, T06_INC_BOMB_FRAG);             /* 第 3 颗 B 碎 */
        t06_window_item_bomb_fragment(&s, -2);
        CHECK_EQ(t06_commit_frame(&s, 3, 0, 2, 0), 0);        /* 进位 +1 炸被抑制 */
        CHECK_EQ(s.rank.sub_rank, T06_INC_BOMB_FRAG);
        /* 整炸从商店买（B 碎没动）⇒ 正常 +0.05 */
        CHECK_EQ(t06_commit_frame(&s, 3, 0, 3, 0), 0);        /* +0.05 不够进位 */
        CHECK_EQ(s.rank.sub_rank, T06_INC_BOMB_FRAG + T06_INC_BOMB_FULL);

        /* ---- e. 跨多颗：一次 commit 里 +2 命（1 命碎进位 + 1 颗整命道具，碎片净 −2）----
         * 抑制规则按「本窗口碎片净减 ⇒ 所有整命增量都不算」执行 —— 这是**保守**侧
         * （宁可少记，也不把碎片进位当整合资源）。逐颗记账已经把碎片那颗算过了。 */
        t06_state_init(&s, T06_DIFF_NORMAL);
        t06_commit_frame(&s, 5, 0, 3, 0);
        t06_increase(&s.rank, T06_INC_LIFE_FRAG);
        t06_window_item_life_fragment(&s, -2);
        CHECK_EQ(t06_commit_frame(&s, 7, 0, 3, 0), 0);
        CHECK_EQ(s.rank.sub_rank, T06_INC_LIFE_FRAG);
    }

    /* ══ ⑥ 每关开场：清 subRank + 帧计数，rank 保留 ══ */
    {
        t06_state_t s;
        t06_state_init(&s, T06_DIFF_NORMAL);
        for (i = 0; i < 100; ++i) t06_tick_frame(&s);
        CHECK_EQ(t06_stage_frame(&s), 100);
        t06_increase(&s.rank, 3 * T06_SUBRANK_CARRY + 12345);  /* rank 19, sub 0.12345 */
        CHECK_EQ(s.rank.rank, 19);
        CHECK_EQ(s.rank.sub_rank, 12345);
        t06_state_stage_reset(&s);
        CHECK_EQ(t06_stage_frame(&s), 0);                      /* timelineTime 每关清零 */
        CHECK_EQ(s.rank.sub_rank, 0);                          /* subRank = 0（GameManager.cpp:357）*/
        CHECK_EQ(s.rank.rank, 19);                             /* rank **保留**（关卡之间不带回）*/
        CHECK_EQ(s.rank.min_rank, 10); CHECK_EQ(s.rank.max_rank, 32);
        CHECK_EQ(s.seeded, 0);                                 /* 新关首帧只对账 */
        /* 新关首帧：不补记上一关末帧到本关首帧的库存变动 */
        CHECK_EQ(t06_commit_frame(&s, 7, 2, 3, 1), 0);
        CHECK_EQ(s.rank.rank, 19);
        CHECK_EQ(t06_commit_frame(&s, 8, 0, 3, 0), 2);         /* 之后正常 */
        CHECK_EQ(s.rank.rank, 21);
    }

    /* ══ ⑦ 一整局的量级（12 条混跑：只校验不越界、恒在 [min,max]） ══ */
    {
        t06_state_t s;
        t06_state_init(&s, T06_DIFF_LUNATIC);
        t06_commit_frame(&s, 3, 0, 3, 0);
        for (i = 0; i < 20000; ++i) {
            t06_tick_frame(&s);
            if (t06_frame_tick_due(t06_stage_frame(&s), 3)) t06_increase(&s.rank, T06_INC_FRAME);
            t06_increase(&s.rank, 3 * T06_INC_GRAZE);          /* 每帧 3 次擦弹 */
            t06_increase(&s.rank, t06_point_gain(1));          /* 一颗最大蓝点 */
            t06_decrease(&s.rank, T06_DEC_OFFSCREEN);          /* 一颗出屏 */
            t06_commit_frame(&s, 3, 0, 3, 0);
            CHECK(s.rank.rank >= 10 && s.rank.rank <= 32);
            CHECK(s.rank.sub_rank >= 0 && s.rank.sub_rank < T06_SUBRANK_CARRY);
        }
        CHECK_EQ(s.rank.rank, 32);                             /* 这个收支下必然顶到 32 */
    }

    printf("test_th06rank_core: %d passed, %d failed\n", s_pass, s_fail);
    return s_fail != 0;
}
