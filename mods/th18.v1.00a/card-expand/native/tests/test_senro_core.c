/* test_senro_core.c —— senro_core.c 的主机单测（107 妖妖梦 / 樱点 / 森罗结界）。
 *
 * 跑法（Linux gcc，无引擎依赖）：
 *   cc -std=c11 -Wall -Wextra -Werror -o /tmp/t107 tests/test_senro_core.c cards/senro_core.c && /tmp/t107
 * 语义来源（一手 = TH07 v1.00b `local/_scratch/th07.asm`；台账 = `local/_scratch/research-107-th07-remaining.md`
 * 与 `docs/superpowers/specs/2026-09-11-system-card-spec-review.md` §9；逐条地址见 `cards/senro_core.h` 头注）：
 *   · 樱点加法器 `0x42f5a2`（夹 ≤ 最大樱点 / +C 只在 delta>0 时涨 / 满 50000 开结界 `0x42f652`）
 *   · 只加樱点 `0x42f69f`/`0x42f736`、只加最大樱点 `0x42f789`（夹 `0x989676`）
 *   · 难度表初值 `0x42eb7a`–`0x42ec57`、结界时长 540 帧（一手 `0x4419fa` 写 `[+0x16a08] = 0x21c`，9 s）
 *   · 自然结束 `0x441670`（+10000/+10000、分数 += 樱点、Plus = 基准）、打断 `0x441bd0`（40 帧无敌）
 *   · 结界中擦弹 `0x43ed5f`–`0x43eda9`（高速 80 / 低速 30，两条都加到樱点与最大樱点）
 *   · 道具三档 `0x433886` / `0x43370b` / `0x433742`+`0x43374e`；蓝点分值 `0x4330db`–`0x4331e5`
 *   · Miss `0x440e84`–`0x440ef0`、Bomb `0x440ad9`
 * 覆盖：难度表 / 取整 / +C 比例 / 初始化 / 加法器三兄弟 / 每帧与自然结束 / 排队 / 擦弹 / 道具 / 命中 /
 *       蓝点分值 / 灵击 / Miss 与 Bomb 扣减 / 状态不变式。
 * 设计文档：`docs/superpowers/specs/2026-09-12-senro-107-design.md`。
 */
#include <stdio.h>
#include <string.h>

#include "../cards/senro_core.h"

static int s_fail, s_pass;
#define CHECK(cond) do { if (cond) ++s_pass; else { ++s_fail; printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } } while (0)

/* 开一次结界（= 加法器把 +C 顶满那条路；返回事件位）。 */
static int open_border(senro_state_t *s)
{
    int ev;
    senro_take_events(s);
    ev = senro_add_cherry(s, SENRO_PLUS_FULL);
    return ev;
}

int main(void)
{
    senro_state_t st;
    int ev, i;

    /* ==================== A. 难度表（一手 0x42eb7a–0x42ec57）==================== */
    CHECK(SENRO_DIFF_EASY == 0 && SENRO_DIFF_PHANTASM == 5);
    /* 最大樱点初值 */
    CHECK(senro_cherry_max_init(SENRO_DIFF_EASY) == 200000);      /* 0x42eb86 */
    CHECK(senro_cherry_max_init(SENRO_DIFF_NORMAL) == 200000);    /* 0x42eba5 */
    CHECK(senro_cherry_max_init(SENRO_DIFF_HARD) == 250000);      /* 0x42ebc4 */
    CHECK(senro_cherry_max_init(SENRO_DIFF_LUNATIC) == 300000);   /* 0x42ebe3 */
    CHECK(senro_cherry_max_init(SENRO_DIFF_EXTRA) == 400000);     /* 0x42ebff */
    CHECK(senro_cherry_max_init(SENRO_DIFF_PHANTASM) == 400000);  /* 0x42ec35 */
    CHECK(senro_cherry_max_init(99) == 200000);                   /* 越界兜底 */
    CHECK(senro_cherry_max_init(-1) == 200000);
    /* 樱点初值：只有 EX / PH 那一支另写 `+0x961c` */
    CHECK(senro_cherry_init(SENRO_DIFF_EASY) == 0);
    CHECK(senro_cherry_init(SENRO_DIFF_NORMAL) == 0);
    CHECK(senro_cherry_init(SENRO_DIFF_HARD) == 0);
    CHECK(senro_cherry_init(SENRO_DIFF_LUNATIC) == 0);
    CHECK(senro_cherry_init(SENRO_DIFF_EXTRA) == 200000);         /* 0x42ec19 */
    CHECK(senro_cherry_init(SENRO_DIFF_PHANTASM) == 300000);      /* 0x42ec4f */
    CHECK(senro_cherry_init(99) == 0);

    /* ==================== B. 取整到 10（th07 的 `idiv 10` + `sub`）==================== */
    CHECK(senro_round10(0) == 0);
    CHECK(senro_round10(9) == 0);
    CHECK(senro_round10(10) == 10);
    CHECK(senro_round10(19) == 10);
    CHECK(senro_round10(20) == 20);
    CHECK(senro_round10(70) == 70);
    CHECK(senro_round10(50000) == 50000);
    CHECK(senro_round10(999999) == 999990);
    CHECK(senro_round10(-1) == -10);       /* 负数向下（只在中间量上出现）*/
    CHECK(senro_round10(-10) == -10);

    /* ==================== C. 结界中 +C 的每帧比例（0x4414db）==================== */
    CHECK(senro_plus_for_timer(0) == 0);
    CHECK(senro_plus_for_timer(-5) == 0);
    CHECK(senro_plus_for_timer(SENRO_BORDER_FRAMES) == SENRO_PLUS_FULL);   /* 满格 */
    CHECK(senro_plus_for_timer(270) == 25000);                             /* 270×50000/540 */
    CHECK(senro_plus_for_timer(108) == 10000);                             /* 108×50000/540 */
    CHECK(senro_plus_for_timer(135) * 2 == senro_plus_for_timer(270));     /* 线性 */
    CHECK(senro_plus_for_timer(539) == 49907);                             /* 539×50000/540 截断 */
    CHECK(senro_plus_for_timer(540) == 50000);
    CHECK(senro_plus_for_timer(1000) == 50000);                            /* 夹到总时长 */
    CHECK(senro_plus_for_timer(1) == 92);                                  /* 1×50000/540 截断 */

    /* ==================== D. 初始化 / 每关重置 ==================== */
    memset(&st, 0x5a, sizeof st);
    senro_init(&st, SENRO_DIFF_NORMAL);
    CHECK(st.cherry == 0);
    CHECK(st.cherry_max == 200000);
    CHECK(st.plus == 0);
    CHECK(st.active == 0);
    CHECK(st.queued == 0);
    CHECK(st.timer == 0);
    CHECK(st.invuln == 0);
    CHECK(st.graze_last == -1);            /* 差分基准撤销 */
    CHECK(st.spell_bonuses == 0);
    CHECK(st.stat_open == 0 && st.stat_natural == 0 && st.stat_libyou == 0);
    CHECK(senro_border_active(&st) == 0);
    senro_init(&st, SENRO_DIFF_EXTRA);
    CHECK(st.cherry == 200000);            /* EX 的樱点初值非 0 */
    CHECK(st.cherry_max == 400000);
    CHECK(st.cherry <= st.cherry_max);
    /* 每关重置：加过东西之后回到初值，但统计保留 */
    senro_init(&st, SENRO_DIFF_NORMAL);
    (void)open_border(&st);
    CHECK(st.active == 1 && st.stat_open == 1);
    st.spell_bonuses = 4;
    senro_stage_reset(&st, SENRO_DIFF_NORMAL);
    CHECK(st.active == 0 && st.timer == 0 && st.plus == 0);
    CHECK(st.cherry == 0 && st.cherry_max == 200000);
    CHECK(st.spell_bonuses == 0);
    CHECK(st.stat_open == 1);              /* 统计跨关累计 */
    CHECK(senro_border_active(&st) == 0);

    /* ==================== E. 樱点加法器（0x42f5a2）==================== */
    senro_init(&st, SENRO_DIFF_NORMAL);
    CHECK(senro_take_events(&st) == SENRO_EV_NONE);
    CHECK(senro_add_cherry(&st, 100) == SENRO_EV_NONE);
    CHECK(st.cherry == 100);
    CHECK(st.plus == 100);                 /* 樱点与 +C 同加 */
    CHECK(st.active == 0);
    CHECK(st.cherry_max == 200000);        /* 加法器不动最大樱点 */
    /* 负 delta：樱点照减（0x42f5bf 先加了才判符号），+C 不动（0x42f5f1 `jle`）*/
    (void)senro_add_cherry(&st, -40);
    CHECK(st.cherry == 60);
    CHECK(st.plus == 100);
    (void)senro_add_cherry(&st, 0);
    CHECK(st.cherry == 60 && st.plus == 100);
    /* 樱点被最大樱点夹住 */
    (void)senro_add_cherry(&st, 1000000);
    CHECK(st.cherry == 200000);
    CHECK(st.plus == SENRO_PLUS_FULL);     /* +C 顶满并被夹 */
    CHECK(st.active == 1);                 /* …并当场开结界 */
    CHECK(st.timer == SENRO_BORDER_FRAMES);
    CHECK(st.stat_open == 1);
    ev = senro_take_events(&st);                   /* 取一次：清空，两个位都在 */
    CHECK((ev & SENRO_EV_BORDER_OPEN) != 0);
    CHECK((ev & SENRO_EV_CHERRY_MAXED) != 0);      /* 满值提示 */
    CHECK(senro_take_events(&st) == SENRO_EV_NONE);                 /* 取走即清零 */
    /* 满值提示只在真的到顶时出 */
    senro_init(&st, SENRO_DIFF_NORMAL);
    (void)senro_add_cherry(&st, 199990);
    CHECK((senro_take_events(&st) & SENRO_EV_CHERRY_MAXED) == 0);
    (void)senro_add_cherry(&st, 10);
    CHECK(st.cherry == 200000);
    CHECK((senro_take_events(&st) & SENRO_EV_CHERRY_MAXED) != 0);
    /* 结界展开中再攒满：不重开、不排队，+C 停在满值（等下一帧按计时比例重写）*/
    ev = senro_add_cherry(&st, 10);
    CHECK((ev & SENRO_EV_BORDER_OPEN) == 0);
    CHECK(st.active == 1);
    CHECK(st.plus == SENRO_PLUS_FULL);

    /* 只加樱点（0x42f69f / 0x42f736）：+C 与最大樱点都不动 */
    senro_init(&st, SENRO_DIFF_NORMAL);
    CHECK(senro_add_cherry_only(&st, 500) == SENRO_EV_NONE);
    CHECK(st.cherry == 500);
    CHECK(st.plus == 0);
    CHECK(st.cherry_max == 200000);
    (void)senro_add_cherry_only(&st, 1000000);
    CHECK(st.cherry == 200000);

    /* 只加最大樱点（0x42f789）：夹 0x989676 */
    senro_init(&st, SENRO_DIFF_NORMAL);
    senro_add_cherry_max(&st, 10000);
    CHECK(st.cherry_max == 210000);
    CHECK(st.cherry == 0);
    CHECK(st.plus == 0);
    senro_add_cherry_max(&st, SENRO_CHERRY_MAX_CAP * 2);
    CHECK(st.cherry_max == SENRO_CHERRY_MAX_CAP);
    CHECK(SENRO_CHERRY_MAX_CAP == 9999990);        /* 0x42f7b1 的 0x989676 */
    senro_add_cherry_max(&st, -SENRO_CHERRY_MAX_CAP * 2);
    CHECK(st.cherry_max == 0);
    CHECK(st.cherry == 0);                          /* 樱点不会被最大樱点的收缩带下去 */

    /* ==================== F. 每帧 / 自然结束（0x441670）==================== */
    senro_init(&st, SENRO_DIFF_NORMAL);
    CHECK(senro_tick(&st, 0) == SENRO_EV_NONE);     /* 没结界：什么都没发生 */
    CHECK(st.timer == 0 && st.plus == 0);
    ev = open_border(&st);
    CHECK((ev & SENRO_EV_BORDER_OPEN) != 0);
    CHECK(st.timer == SENRO_BORDER_FRAMES);
    CHECK(st.plus == SENRO_PLUS_FULL);
    CHECK(st.cherry == SENRO_PLUS_FULL);
    /* 开结界那一帧就算第 1 帧（±1 帧判定见本卡 spec §6.1）*/
    (void)senro_tick(&st, 0);
    CHECK(st.timer == 539);
    CHECK(st.plus == senro_plus_for_timer(539));
    CHECK(st.plus == 49907);
    (void)senro_tick(&st, 0);
    CHECK(st.timer == 538);
    CHECK(st.plus == 49814);                        /* 538×50000/540 */
    /* 一路跑到 1 帧剩余都还在结界里 */
    for (i = 0; i < 537; ++i) (void)senro_tick(&st, 0);
    CHECK(st.timer == 1);
    CHECK(st.active == 1);
    CHECK(st.plus == senro_plus_for_timer(1));
    CHECK(st.plus == 92);
    /* 第 540 帧：自然结束 */
    ev = senro_tick(&st, 0);
    CHECK((ev & SENRO_EV_END_NATURAL) != 0);
    CHECK(st.active == 0);
    CHECK(st.timer == 0);
    CHECK(st.plus == 0);                            /* Plus = 基准（0x4416ee）*/
    CHECK(st.cherry_max == 210000);                 /* +10000（0x44167a）*/
    CHECK(st.cherry == 60000);                      /* 50000 + 10000（0x441689；夹 ≤ 210000）*/
    CHECK(st.last_bonus == 60000);                  /* 分数 Bonus = 樱点（×10 与 ÷10 相消，0x4416af/0x4416c4）*/
    CHECK(st.stat_natural == 1);
    CHECK(senro_tick(&st, 0) == SENRO_EV_NONE);     /* 结束后不再重复结算 */
    CHECK(st.cherry == 60000);
    /* 结界帧数常量 = 9 s @60fps（一手 0x4419fa `[结界对象+0x16a08] = 0x21c`）*/
    CHECK(SENRO_BORDER_FRAMES == 540);
    CHECK(SENRO_PLUS_FULL == 50000);
    CHECK(SENRO_INVULN_FRAMES == 40);
    CHECK(SENRO_BORDER_END_ADD == 10000);

    /* 排队：满槽时正在放炸弹 ⇒ 不开门，等炸弹结束（0x441983）*/
    senro_init(&st, SENRO_DIFF_NORMAL);
    (void)senro_tick(&st, 1);                       /* 本帧在放炸弹 */
    ev = senro_add_cherry(&st, SENRO_PLUS_FULL);
    CHECK((ev & SENRO_EV_BORDER_QUEUE) != 0);
    CHECK((ev & SENRO_EV_BORDER_OPEN) == 0);
    CHECK(st.active == 0 && st.queued == 1);
    CHECK(st.stat_open == 0);
    (void)senro_tick(&st, 1);
    CHECK(st.active == 0 && st.queued == 1);        /* 还在放：继续等 */
    ev = senro_tick(&st, 0);                        /* 炸弹结束 */
    CHECK((ev & SENRO_EV_BORDER_OPEN) != 0);
    CHECK(st.active == 1 && st.queued == 0);
    CHECK(st.stat_open == 1);
    CHECK(st.timer == 539);                         /* 开的那一帧就计第 1 帧（540 − 1）*/

    /* ==================== G. 结界中擦弹（0x43ed5f–0x43eda9）==================== */
    senro_init(&st, SENRO_DIFF_NORMAL);
    CHECK(senro_graze(&st, 5, 0) == 0);             /* 没结界：不给（0x43ec8a 的门）*/
    CHECK(st.cherry == 0 && st.cherry_max == 200000);
    (void)open_border(&st);
    (void)senro_tick(&st, 0);
    {
        int32_t cherry0 = st.cherry, max0 = st.cherry_max, plus0 = st.plus;
        CHECK(senro_graze(&st, 5, 0) == 5 * SENRO_GRAZE_FAST);
        CHECK(senro_graze(&st, 0, 0) == 0);
        CHECK(senro_graze(&st, -3, 0) == 0);
        CHECK(st.cherry == cherry0 + 400);           /* 高速 80 × 5 */
        CHECK(st.cherry_max == max0 + 400);          /* 最大樱点也加同样的量 */
        CHECK(st.plus == plus0);                     /* ★ 两条都不动 +C（一手）*/
        CHECK(senro_graze(&st, 2, 1) == 2 * SENRO_GRAZE_SLOW);
        CHECK(st.cherry == cherry0 + 400 + 60);      /* 低速 30 × 2 */
        CHECK(st.cherry_max == max0 + 400 + 60);
        CHECK(SENRO_GRAZE_FAST == 80 && SENRO_GRAZE_SLOW == 30);
    }
    /* 结界结束后擦弹不再给 */
    (void)senro_libyou(&st, 0);
    CHECK(senro_graze(&st, 10, 0) == 0);
    /* 樱点被最大樱点夹住时不越界 */
    senro_init(&st, SENRO_DIFF_NORMAL);
    (void)open_border(&st);
    (void)senro_add_cherry(&st, 1000000);            /* cherry = 200000 = max */
    (void)senro_graze(&st, 10, 0);
    CHECK(st.cherry == st.cherry_max);
    CHECK(st.cherry_max == 200800);

    /* ==================== H. 道具（0x43370b ★ / 0x433742+0x43374e 小樱点；2026-09-30 第四轮改口径）====
     * owner 口径：「**一手应该是收金钱（蓝点）/ P 不加**」—— TH18 type 2 = 金钱 = 本 mod 的
     * 「点道具 / 蓝点」对位（`cards/trance.c` 同口径），TH07 的蓝点 handler 不进加法器 ⇒ 0；
     * 金钱原来的「大樱点 1000+100×符卡奖励」那一条**已撤**（大樱点改由自建实体那条路记）。 */
    senro_init(&st, SENRO_DIFF_NORMAL);
    CHECK(senro_item_cherry(&st, SENRO_TH18_ITEM_MONEY) == 0);   /* 金钱/蓝点：不给樱点 */
    CHECK(senro_item_cherry(&st, 2) == 0);
    CHECK(st.cherry == 0 && st.plus == 0);
    CHECK(st.stat_items == 0);
    st.spell_bonuses = 3;                         /* 符卡奖励次数不再影响道具口径 */
    CHECK(senro_item_cherry(&st, SENRO_TH18_ITEM_SCORE_LO) == SENRO_STAR_CHERRY);   /* ★ = 20 */
    CHECK(senro_item_cherry(&st, SENRO_TH18_ITEM_SCORE_HI) == SENRO_STAR_CHERRY);
    CHECK(st.cherry == 40);
    CHECK(st.plus == 40);                         /* ★ 走加法器 ⇒ +C 同加 */
    CHECK(senro_item_cherry(&st, 1) == 0);        /* P 点：th07 不给樱点 */
    CHECK(senro_item_cherry(&st, 3) == 0);
    CHECK(senro_item_cherry(&st, 4) == 0);        /* 命碎 */
    CHECK(senro_item_cherry(&st, 6) == 0);        /* 符卡碎 */
    CHECK(senro_item_cherry(&st, 8) == 0);
    CHECK(senro_item_cherry(&st, 0) == 0);
    CHECK(senro_item_cherry(&st, 0xf) == 0);
    CHECK(senro_item_cherry(&st, SENRO_TH18_ITEM_SCORE_LO - 1) == 0);
    CHECK(st.cherry == 40);
    CHECK(st.stat_items == 2);                    /* 只有给过樱点的那 2 颗计数 */
    CHECK(SENRO_STAR_CHERRY == 20);
    /* 大樱点那套值仍是一手（供自建实体那条路用） */
    CHECK(SENRO_ITEM_BIG_BASE == 1000 && SENRO_ITEM_BIG_PER_BONUS == 100);

    /* ============ I. 命中（2026-09-30 第四轮：按一手公式重写，入参 = 关卡号）============
     * 一手：`p = stage >= 5 ? 10 : stage × 2`、`k = 10 − p/3`、`gain = (damage / k) × 10`、
     * `gain == 0 → 10`、上限 70（地址见 `senro_core.h` 的 `SENRO_HIT_*`）。 */
    senro_init(&st, SENRO_DIFF_NORMAL);
    CHECK(SENRO_HIT_DIV == 10 && SENRO_HIT_MIN == 10 && SENRO_HIT_CAP == 70);
    /* stage 1 ⇒ p = 2 ⇒ k = 10 − 0 = 10 */
    CHECK(senro_hit(&st, 5, 1) == 10);            /* 5/10 = 0 ⇒ **抬到 10**（小怪命中也有）*/
    CHECK(senro_hit(&st, 20, 1) == 20);           /* (20/10)×10 = 20 */
    CHECK(st.cherry == 30);
    CHECK(st.plus == st.cherry);                  /* 命中走加法器 ⇒ +C 同加 */
    CHECK(senro_hit(&st, 100, 1) == 70);          /* (100/10)×10 = 100 ⇒ 夹 70 */
    CHECK(senro_hit(&st, 100000, 1) == SENRO_HIT_CAP);
    CHECK(senro_hit(&st, 0, 1) == 0);
    CHECK(senro_hit(&st, -50, 1) == 0);
    CHECK(st.stat_hits == 4);
    /* 关卡分档：stage 3 ⇒ p = 6 ⇒ k = 10 − 2 = 8 ⇒ (20/8)=2 ⇒ 20；stage 6 ⇒ p = 10 ⇒ k = 7 */
    CHECK(senro_hit(&st, 20, 3) == 20);
    CHECK(senro_hit(&st, 20, 6) == 20);           /* (20/7)=2 ⇒ 20 */
    CHECK(senro_hit(&st, 6, 6) == 10);            /* (6/7)=0 ⇒ 抬到 10 */
    CHECK(senro_hit(&st, 700, 6) == 70);          /* (700/7)×10 = 1000 ⇒ 夹 70 */
    /* 关卡号传 0/负数按 stage 1 那一支兜底（不影响上面各条的边界） */
    CHECK(senro_hit(&st, 20, 0) == 20);

    /* ======= K. 定宽字段「画哪几位」（一手 0x404a25–0x404a3c 的 started 闸 + 个位恒画）=======
     * 2026-09-30 第五轮修 bug：旧实现「凡是 0 就跳过」把 200000 画成 `2    0`。 */
    {
        int d[7], dr[7], i;
        /* 200000（最大樱点初值，6 位）：第一位非零 ⇒ **六位全画** */
        senro_field_layout(200000, 6, d, dr);
        CHECK(d[0] == 2 && d[1] == 0 && d[2] == 0 && d[3] == 0 && d[4] == 0 && d[5] == 0);
        for (i = 0; i < 6; ++i) CHECK(dr[i] == 1);
        /* 1234（6 位字段）：前两位是**前导**零 ⇒ 不画；后四位全画 */
        senro_field_layout(1234, 6, d, dr);
        CHECK(dr[0] == 0 && dr[1] == 0);
        CHECK(dr[2] == 1 && dr[3] == 1 && dr[4] == 1 && dr[5] == 1);
        CHECK(d[2] == 1 && d[3] == 2 && d[4] == 3 && d[5] == 4);
        /* 100001：中间那四个 0 在 started 之后 ⇒ 必须画（旧 bug 就丢在这） */
        senro_field_layout(100001, 6, d, dr);
        CHECK(dr[0] == 1 && dr[1] == 1 && dr[2] == 1 && dr[3] == 1 && dr[4] == 1 && dr[5] == 1);
        CHECK(d[1] == 0 && d[2] == 0 && d[3] == 0 && d[4] == 0 && d[5] == 1);
        /* 值 0 / 5（5 位 +C 字段）：个位恒画 ⇒ 只画最后一位 */
        senro_field_layout(0, 5, d, dr);
        CHECK(dr[0] == 0 && dr[1] == 0 && dr[2] == 0 && dr[3] == 0 && dr[4] == 1);
        CHECK(d[4] == 0);
        senro_field_layout(5, 5, d, dr);
        CHECK(dr[4] == 1 && dr[3] == 0 && d[4] == 5);
        /* 7 位（最大樱点 ≥ 1000000 那一支）全画 */
        senro_field_layout(1000000, 7, d, dr);
        for (i = 0; i < 7; ++i) CHECK(dr[i] == 1);
        CHECK(d[0] == 1);
        /* 负数按 0 处理（与卡侧原来的 value < 0 归一一致） */
        senro_field_layout(-5, 5, d, dr);
        CHECK(d[4] == 0 && dr[4] == 1);
    }

    /* ======= L. 最大樱点的「基础值」（HUD 显示 `cherry_max − cherry_max_base`，2026-09-30 第六轮）====
     * owner：「最大樱点基础值不应该画 —— 它对应我们额外最大得点，得去掉基础值」。 */
    {
        senro_state_t b;
        senro_init(&b, SENRO_DIFF_NORMAL);
        CHECK(b.cherry_max == 200000 && b.cherry_max_base == 200000);   /* NORMAL：开场显示 0 */
        CHECK(senro_cherry_max_init(SENRO_DIFF_LUNATIC) == 300000);
        senro_init(&b, SENRO_DIFF_LUNATIC);
        CHECK(b.cherry_max_base == 300000);
        CHECK(b.cherry_max - b.cherry_max_base == 0);
        /* 结界自然结束 +10000 ⇒ 显示值 = 10000（base 不动） */
        (void)open_border(&b);
        while (senro_border_active(&b)) (void)senro_tick(&b, 0);
        CHECK(b.cherry_max == 310000 && b.cherry_max_base == 300000);
        CHECK(b.cherry_max - b.cherry_max_base == 10000);
        /* 换关重设：base 跟着难度表重取 */
        senro_stage_reset(&b, SENRO_DIFF_HARD);
        CHECK(b.cherry_max == 250000 && b.cherry_max_base == 250000);
    }

    /* ======= M. 敌人自带掉落槽的 32 项轮转表（一手 th07:0x49efa0 逐字节，2026-10-01）======= */
    {
        int32_t cur = 0, i, n7 = 0, n0 = 0, n1 = 0, n2 = 0, first[32];
        for (i = 0; i < 32; ++i) first[i] = senro_rot_drop_next(&cur);
        for (i = 0; i < 32; ++i) {
            if (first[i] == 7) ++n7;
            else if (first[i] == 0) ++n0;
            else if (first[i] == 1) ++n1;
            else if (first[i] == 2) ++n2;
        }
        CHECK(n0 == 14 && n1 == 14 && n7 == 3 && n2 == 1);        /* 一手分布 0×14 / 1×14 / 7×3 / 2×1 */
        CHECK(first[7] == 7 && first[12] == 7 && first[26] == 7); /* 三个 7 的确切位置（0 基）*/
        CHECK(cur == 0);                                          /* 转满一圈回到起点 */
        CHECK(senro_rot_drop_next(NULL) == -1);                   /* 空指针兜底 */
        cur = -5; CHECK(senro_rot_drop_next(&cur) == 0);          /* 越界游标归一 */
        cur = 31; CHECK(senro_rot_drop_next(&cur) == 2);          /* 最后一项 = 2（大P）*/
        CHECK(cur == 0);                                          /* 回绕 */
    }

    /* ==================== J. 蓝点（点道具）分值（0x4330db–0x4331e5）==================== */
    senro_init(&st, SENRO_DIFF_NORMAL);
    CHECK(senro_point_item_value(&st) == SENRO_POINT_BASE);        /* 樱点 0 ⇒ 基线 50000 */
    CHECK(SENRO_POINT_BASE == 50000);
    st.cherry = 49999;
    CHECK(senro_point_item_value(&st) == 50000);                   /* 低于基线：不加成 */
    st.cherry = 50000;
    CHECK(senro_point_item_value(&st) == 50000);
    st.cherry = 50004;
    CHECK(senro_point_item_value(&st) == 50000);                   /* +0 → 取整到 10 */
    st.cherry = 50005;
    CHECK(senro_point_item_value(&st) == 50000);                   /* +1 → 50001 → 50000 */
    st.cherry = 50010;
    CHECK(senro_point_item_value(&st) == 50000);                   /* +2 */
    st.cherry = 100000;
    CHECK(senro_point_item_value(&st) == 60000);                   /* +10000 */
    st.cherry = 200000;
    CHECK(senro_point_item_value(&st) == 80000);                   /* +30000（E/N 的最大樱点满值）*/
    CHECK(senro_point_item_value(NULL) == SENRO_POINT_BASE);

    /* ==================== K. 灵击（0x441bd0）==================== */
    senro_init(&st, SENRO_DIFF_NORMAL);
    CHECK(senro_libyou(&st, 100) == 0);            /* 没结界：什么都不做 */
    CHECK(st.invuln == 0);
    CHECK(st.cherry == 0);
    (void)open_border(&st);
    (void)senro_tick(&st, 0);
    CHECK(st.active == 1);
    CHECK(senro_libyou(&st, 100) == 1);
    CHECK(st.active == 0);
    CHECK(st.timer == 0);
    CHECK(st.queued == 0);
    CHECK(st.invuln == SENRO_INVULN_FRAMES);       /* 0x441d8e 的 0x28 = 40 帧 = 0.67 s */
    CHECK(st.cherry == 50000 + 100 * 100);         /* 每颗 = 小樱点：+30 走加法器、+70 只加樱点 */
    CHECK(st.cherry_max == 200000);                /* 小樱点不涨最大樱点（一手）*/
    CHECK(st.plus == 100 * SENRO_SMALL_CHERRY_ADD);/* +C = 每颗 30（清零在前、入账在后）*/
    CHECK(st.stat_libyou == 1);
    CHECK(st.stat_cancel == 100);
    CHECK(senro_libyou(&st, 100) == 0);            /* 已经不在结界里了 */
    CHECK(st.stat_libyou == 1);
    /* 0 颗也照样收尾（结界被打断，只是没有弹可消）*/
    (void)open_border(&st);                        /* 上一段结束时樱点是 60000 ⇒ 这里 110000 */
    CHECK(senro_libyou(&st, 0) == 1);
    CHECK(st.invuln == SENRO_INVULN_FRAMES);
    CHECK(st.cherry == 110000);                    /* 没有小樱点入账 ⇒ 樱点不动 */
    CHECK(st.plus == 0);
    /* 小樱点常量：type 8 那两条调用 */
    CHECK(SENRO_SMALL_CHERRY_ADD == 30 && SENRO_SMALL_CHERRY_EXTRA == 70);
    /* 小樱点单列（中弹灵击的「下一帧补做」路径直接调它；不再受结界门约束）*/
    senro_init(&st, SENRO_DIFF_NORMAL);
    CHECK(senro_small_cherry(&st, 0) == 0);
    CHECK(senro_small_cherry(&st, -1) == 0);
    CHECK(senro_small_cherry(&st, 3) == 3 * (SENRO_SMALL_CHERRY_ADD + SENRO_SMALL_CHERRY_EXTRA));
    CHECK(st.cherry == 300);
    CHECK(st.plus == 3 * SENRO_SMALL_CHERRY_ADD);  /* +C 每颗只拿 30 */
    CHECK(st.cherry_max == 200000);                /* 小樱点不涨最大樱点 */
    CHECK(st.stat_cancel == 3);
    CHECK(senro_small_cherry(NULL, 3) == 0);
    /* 灵击入账能再次顶满 +C（1667 颗 × 30 = 50010 ≥ 50000）*/
    senro_init(&st, SENRO_DIFF_NORMAL);
    (void)open_border(&st);
    CHECK(senro_libyou(&st, 1667) == 1);
    CHECK(st.plus == SENRO_PLUS_FULL);
    CHECK(st.active == 1);                         /* 当场又开了一轮 */

    /* ==================== L. Miss / Bomb 扣樱点（0x440e84 / 0x440ad9）==================== */
    senro_init(&st, SENRO_DIFF_NORMAL);
    st.cherry = 60000;
    CHECK(senro_penalty_miss(&st) == 30000);       /* 1/2，取整到 10 */
    CHECK(st.cherry == 30000);
    CHECK(st.plus == 0);                           /* 扣减不动 +C */
    CHECK(st.cherry_max == 200000);                /* 也不动最大樱点 */
    st.cherry = 30001;
    CHECK(senro_penalty_miss(&st) == 15000);       /* 30001/2 = 15000（截断）→ 15000 */
    CHECK(st.cherry == 15001);
    st.cherry = 30;
    CHECK(senro_penalty_miss(&st) == 10);          /* 15 → 取整到 10 */
    CHECK(st.cherry == 20);
    st.cherry = 10;
    CHECK(senro_penalty_miss(&st) == 0);           /* 5 → 0：不扣 */
    CHECK(st.cherry == 10);
    st.cherry = 0;
    CHECK(senro_penalty_miss(&st) == 0);
    CHECK(st.cherry == 0);                         /* 不会扣成负数 */
    /* 扣减上限 CAP = 100000（0x440eb2 / 0x440ec4；咲夜 60000）*/
    senro_init(&st, SENRO_DIFF_EXTRA);             /* 最大樱点 400000 */
    st.cherry = 300000;
    CHECK(senro_penalty_miss(&st) == SENRO_MISS_CAP);
    CHECK(SENRO_MISS_CAP == 100000);
    CHECK(st.cherry == 200000);
    CHECK(senro_penalty_miss(&st) == 100000);      /* 200000/2 = 100000 = CAP */
    CHECK(st.cherry == 100000);
    CHECK(senro_penalty_miss(&st) == 50000);       /* 到 CAP 以下就按比例 */
    CHECK(st.cherry == 50000);
    /* Bomb：1/4（❓ 自定），夹回基准（不会变负）*/
    senro_init(&st, SENRO_DIFF_NORMAL);
    st.cherry = 100000;
    CHECK(senro_penalty_bomb(&st) == 25000);
    CHECK(st.cherry == 75000);
    CHECK(st.plus == 0 && st.cherry_max == 200000);
    st.cherry = 30;
    CHECK(senro_penalty_bomb(&st) == 0);           /* 7 → 取整到 10 = 0 */
    CHECK(st.cherry == 30);
    st.cherry = 41;
    CHECK(senro_penalty_bomb(&st) == 10);          /* 10 → 10 */
    CHECK(st.cherry == 31);
    st.cherry = 3;
    CHECK(senro_penalty_bomb(&st) == 0);
    CHECK(st.cherry == 3);
    CHECK(senro_penalty_bomb(NULL) == 0);
    CHECK(senro_penalty_miss(NULL) == 0);
    CHECK(SENRO_BOMB_NUM == 1 && SENRO_BOMB_DEN == 4);
    CHECK(SENRO_MISS_NUM == 1 && SENRO_MISS_DEN == 2);

    /* ==================== M. 不变式（整条链路跑一遍）==================== */
    senro_init(&st, SENRO_DIFF_NORMAL);
    for (i = 0; i < 2000; ++i) {
        (void)senro_tick(&st, (i % 97) == 0);
        (void)senro_add_cherry(&st, (i % 7) * 137);
        (void)senro_graze(&st, i % 3, i & 1);
        (void)senro_item_cherry(&st, (uint32_t)(i % 16));
        (void)senro_hit(&st, i * 13, i & 1);
        if ((i % 211) == 0) (void)senro_libyou(&st, i % 50);
        if ((i % 307) == 0) (void)senro_penalty_miss(&st);
        if ((i % 401) == 0) (void)senro_penalty_bomb(&st);
        CHECK(st.cherry >= 0 && st.cherry <= st.cherry_max);
        CHECK(st.cherry_max >= 0 && st.cherry_max <= SENRO_CHERRY_MAX_CAP);
        CHECK(st.plus >= 0 && st.plus <= SENRO_PLUS_FULL);
        CHECK(st.timer >= 0 && st.timer <= SENRO_BORDER_FRAMES);
        CHECK(st.invuln >= 0 && st.invuln <= SENRO_INVULN_FRAMES);
        CHECK(st.active == 0 || st.active == 1);
    }
    CHECK(st.stat_open > 0);
    CHECK(st.stat_items > 0);

    printf("test_senro_core: %d passed, %d failed\n", s_pass, s_fail);
    return s_fail ? 1 : 0;
}
