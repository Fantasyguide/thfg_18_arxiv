/* test_ufo_core.c —— ufo_core.c 的主机单测（112 星莲船 / UFO）。
 *
 * 语义来源（TH18 v1.00a）：
 *   `docs/superpowers/specs/2026-09-12-ufo-112-design.md`（§0 定案 / §3.1–3.5 引擎面 / §4 数值 / §7 AUDIT）
 *   `engine/ufo/th12/01-ufo-system.md`（TH12 原版 §3 令牌槽 / §4 字段分工 / §7 吸收链 / §8 击破奖励）
 *   `docs/superpowers/plans/2026-09-10-ufo-card-feasibility.md`（§1.1 spawn / §2 磁吸模型 / §2.3 无目标点字段）
 *   `docs/superpowers/specs/2026-09-12-shared-enemy-death-event-design.md`（§1.3 死因表 / §2.1 事件契约）
 *   本次实装补的一手（objdump th18.exe，见 ufo.c 文件头的地址表）：`0x445e38` 出界删除判定、
 *   `0x445bf5`–`0x445c55` state-1 位移式、`0x44658a` 道具清尾、`0x446830` type 跳表。
 *
 * 覆盖：颜色编码（**含金**）/ 3 槽状态机 / 三同色与三互不相同 / 彩虹补位 /
 *       **「两同一异 ⇒ 丢最旧」（用户 2026-09-12 裁决 ②；6 种排列逐条 + 字面一手条件对照）** /
 *       合成消费与 last_group / **五边形框触发条件（裁决 ③，纯逻辑判据）** /
 *       UFO 生命周期（召唤-单例-存活帧-收尾）/ 吸收门（type & state 过滤）/
 *       拉拽速度曲线与方向单位向量（含位技巧精度）/ 围栏三件（屏内 / 30 px 自机圈）/
 *       填充率（一手公式 + 关卡阈值表 34/36/39/42/46/51/56）/ 两个吸收计数与彩虹倒置 /
 *       吸满吐奖励表（红/绿/蓝/彩虹 + 金）/ 击破颜色加成 / 赏金 type 选择与份额 /
 *       HP 硬约束 + **血条千分比 `ufo_hp_permille`（裁决 ④ 的 HUD 用）** /
 *       死亡事件 kind 过滤（必须排除 KILL_ALL）。
 *
 * ⚠ 占位项（一律断言「不做」这个语义，不假装是复刻）：HP、令牌 TTL/数量、
 *   赏金分值权重、BOMB_KILL 算不算击破、ECL 子程序名。
 * ★ 金（用户 2026-09-12 裁决 ①）：本文件用**对照表**断言「金与蓝逐项相同」；「金在凑组里算不算
 *   独立一色」是 ❓ 自定（`UFO_GOLD_COUNTS_AS_BLUE`）⇒ 两种取法各写一份断言（改宏必红一半）。
 *
 * ★★ 2026-09-12「逐条对账 TH12 一手」这一轮改了三条**语义**（都对着反汇编订正，见各段注释）：
 *   ① `ufo_absorb_item`：fill 满了之后**不再开奖、不再累加**（一手 `0x44acd8` 的早退）——
 *      旧版每颗都返回 1 ⇒ 红 UFO 的命碎片能无限刷（漏洞）。
 *   ② `ufo_group_keep2_literal_th12`：KEEP2 的条件是**三条**（漏抄了 `0x422ed4`/`0x422ed6`）
 *      ⇒ `{A,B,A}` 首尾同色**也**走 KEEP2 ⇒ 我们与字面一手一致，「有意偏离」那条作废。
 * ③ 新增：`ufo_break_token_state` / `ufo_spit_token_state`（掉落表：每色恰好 1 枚令牌）、
 *      `ufo_tok_motion_init_at/step`（令牌的直线飞行 1.5 + 四边反弹 + 1200 帧后不反弹 + 出界即撤）。
 *
 * ★★ 2026-09-12（第二轮，用户拍板「**token 得是自建 VM 池子**」）—— 令牌不再是真敌人：
 *   ① 运动结构体 `ufo_tok_motion_t` **带上了位置**（`x/y`）⇒ `ufo_tok_motion_init_at(&m, serial, x, y)`
 *      与 `ufo_tok_motion_step(&m)`（旧签名吃 `float *x, *y` 指针，因为位置当年在敌机字段里）。
 *      行为逐条不变（本文件那一整块断言因此只改了调用形式）。
 *   ② 令牌 HP 常量与「令牌被子弹打掉也算收」那个判据函数**已删**（那条路只服务旧载体）
 *      ⇒ 对应的断言换成池容量判据 `ufo_token_pool_room()` 的真值表。
 *   ③ 令牌的**来源**那条门没变（仍是 `ufo_body_kill_counts()` + 掉落标志位）—— 见卡侧 `on_enemy_death`。
 */
#include <stdio.h>
#include <string.h>

#include "../cards/ufo_core.h"

static int s_fail, s_pass;
#define CHECK(cond) do { if (cond) ++s_pass; else { ++s_fail; printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } } while (0)

/* 浮点近似比较（不链 libm）*/
static int feq(float a, float b, float tol)
{
    float d = a - b;
    if (d < 0.0f) d = -d;
    return d <= tol;
}

/* 依次把 3 个颜色塞进槽（模拟三次入槽 + 合成判定入口）*/
static int32_t group_of(int32_t a, int32_t b, int32_t c, ufo_group_t *g)
{
    ufo_group_reset(g);
    ufo_slot_set(g, a);
    ufo_slot_set(g, b);
    ufo_slot_set(g, c);
    return ufo_group_classify(g);
}

int main(void)
{
    ufo_group_t g, g2;
    ufo_ufo_t u;
    int32_t c;
    float dx, dy, px, py, sp;
    int i;

    /* =========================================================== 颜色编码（一手 `th12:0x44a985`）
     * 一手：`1 红 / 2 蓝 / 3 绿 / −1 彩虹`（`UFO:132-133`）—— 数值本身是**可照抄的一手**。 */
    CHECK(UFO_COLOR_RED == 1);
    CHECK(UFO_COLOR_BLUE == 2);
    CHECK(UFO_COLOR_GREEN == 3);
    CHECK(UFO_COLOR_RAINBOW == -1);
    CHECK(UFO_COLOR_NONE == 0);
    CHECK(ufo_color_valid(UFO_COLOR_RED));
    CHECK(ufo_color_valid(UFO_COLOR_BLUE));
    CHECK(ufo_color_valid(UFO_COLOR_GREEN));
    CHECK(ufo_color_valid(UFO_COLOR_RAINBOW));
    CHECK(!ufo_color_valid(UFO_COLOR_NONE));
    CHECK(!ufo_color_valid(5));
    CHECK(!ufo_color_valid(-2));
    /* ★ 金（用户 2026-09-12 裁决 ①）：**是合法色**，且**行为与蓝逐项相同**（对照表见下）。 */
    CHECK(UFO_COLOR_GOLD == 4);                        /* ❓ 自定编号（1/2/3/−1 已占、0 = 无）*/
    CHECK(ufo_color_valid(UFO_COLOR_GOLD));
    CHECK(ufo_color_is_gold(UFO_COLOR_GOLD));
    CHECK(!ufo_color_is_gold(UFO_COLOR_BLUE));
    CHECK(!ufo_color_is_gold(UFO_COLOR_NONE));
    /* —— 金 / 蓝逐项对照（★ 这几条就是「金 = 蓝的调色版」的可执行定义）—— */
    CHECK(ufo_spit_reward(UFO_COLOR_GOLD) == ufo_spit_reward(UFO_COLOR_BLUE));           /* 吸满：都不吐 */
    CHECK(ufo_break_color_bonus(UFO_COLOR_GOLD) == ufo_break_color_bonus(UFO_COLOR_BLUE)); /* 击破：都无额外 */
    CHECK(ufo_color_art_index(UFO_COLOR_GOLD) == ufo_color_art_index(UFO_COLOR_BLUE));   /* ★ 贴图借蓝那一件 */
#if UFO_GOLD_COUNTS_AS_BLUE
    CHECK(ufo_color_norm(UFO_COLOR_GOLD) == ufo_color_norm(UFO_COLOR_BLUE));             /* 桶归并 */
#else
    /* 金当第 4 色（非默认取值）⇒ 桶**不**归并；但「贴图借蓝那一件」与取值无关（上面那条）*/
    CHECK(ufo_color_norm(UFO_COLOR_GOLD) != ufo_color_norm(UFO_COLOR_BLUE));
#endif
    CHECK(ufo_color_art_index(UFO_COLOR_GOLD) == 1);   /* 手算：一手 idx=(color==−1)?3:color−1 ⇒ 红0/蓝1/绿2/虹3 */
    CHECK(ufo_color_art_index(UFO_COLOR_RED) == 0);
    CHECK(ufo_color_art_index(UFO_COLOR_BLUE) == 1);
    CHECK(ufo_color_art_index(UFO_COLOR_GREEN) == 2);
    CHECK(ufo_color_art_index(UFO_COLOR_RAINBOW) == 3);
    CHECK(ufo_color_art_index(UFO_COLOR_NONE) == -1);
    CHECK(ufo_color_art_index(99) == -1);
    /* 金/蓝两个计数（吸收链）也必须一致：颜色 ≠ 彩虹 ⇒ 两个计数按 type 走（金不触发「彩虹倒置」）*/
    ufo_reset(&u);
    CHECK(ufo_spawn(&u, 4242u, UFO_COLOR_GOLD, 999));
    CHECK(ufo_absorb_item(&u, UFO_ITEM_TYPE_P, 1, 1000) == 0);
    CHECK(u.cnt_p == 1);
    CHECK(u.cnt_money == 0);
    ufo_reset(&u);
    CHECK(ufo_spawn(&u, 4242u, UFO_COLOR_BLUE, 999));
    CHECK(ufo_absorb_item(&u, UFO_ITEM_TYPE_P, 1, 1000) == 0);
    CHECK(u.cnt_p == 1);
    CHECK(u.cnt_money == 0);
    ufo_reset(&u);
    CHECK(UFO_SLOT_COUNT == 3);                        /* 一手：`COLLECTED_UFOS[0..2]`（`th12:0x4b0c4c`）*/

    /* ============================================================ 3 槽状态机 */
    ufo_group_reset(&g);
    CHECK(ufo_slots_filled(&g) == 0);
    CHECK(g.filled == 0);
    CHECK(g.last_group == UFO_COLOR_NONE);
    CHECK(!ufo_group_has_match(&g));                   /* 空槽不合成 */
    CHECK(ufo_group_classify(&g) == UFO_COLOR_NONE);

    CHECK(ufo_slot_set(&g, UFO_COLOR_RED));            /* 第一个空槽 */
    CHECK(g.slot[0] == UFO_COLOR_RED);
    CHECK(g.slot[1] == UFO_COLOR_NONE);
    CHECK(g.filled == 1);
    CHECK(!ufo_group_has_match(&g));                   /* 未满 3 格不判（凑组注） */
    CHECK(ufo_slot_set(&g, UFO_COLOR_BLUE));
    CHECK(g.slot[1] == UFO_COLOR_BLUE);
    CHECK(g.filled == 2);
    CHECK(!ufo_group_has_match(&g));
    CHECK(ufo_slot_set(&g, UFO_COLOR_GREEN));
    CHECK(g.slot[2] == UFO_COLOR_GREEN);
    CHECK(g.filled == 3);
    CHECK(ufo_group_has_match(&g));

    /* `ufo_slot_set`（低层）槽满时**不做任何事**：不丢最旧、不替换、不召唤 ——
     * 「槽满怎么办」这条决策**只**在 `ufo_group_put` 里（见下），`ufo_slot_set` 是纯放置。 */
    CHECK(!ufo_slot_set(&g, UFO_COLOR_RAINBOW));
    CHECK(g.filled == 3);
    CHECK(g.slot[0] == UFO_COLOR_RED);
    CHECK(g.slot[1] == UFO_COLOR_BLUE);
    CHECK(g.slot[2] == UFO_COLOR_GREEN);

    /* ================= ★★ 令牌的**场上颜色域**（用户 2026-09-18 裁决「新增 2」）=================
     * 用户原话：「没有彩虹色 token，彩虹色当且仅当，收了三色 token == 飞碟，开了一个彩色 UFO（敌人）」
     * ⇒ **令牌域 = 红/蓝/绿**（金 ≡ 蓝是同一个东西的调色版）；**彩虹只是合成结果**（UFO 的颜色）。
     * 一手对照：令牌 type 只有 10/11/12（红/蓝/绿）与 13/14/15（同一个变色令牌的三种起始显示态）；
     * 进槽时按**当时显示的那一色**记账（`th12:0x426c3f`/`0x426c4d`/`0x426c5e`）。 */
    {
        int i2, seen3[4] = { 0, 0, 0, 0 };             /* 索引 1=红 2=蓝 3=绿 */
        CHECK(UFO_TOKEN_FIELD_COLORS_N == 3);
        for (i2 = 0; i2 < UFO_TOKEN_FIELD_COLORS_N; ++i2)
            CHECK(ufo_token_color_is_field(k_ufo_token_field_colors[i2]));
        /* ★ 场上的三色必须**恰好**是红/蓝/绿：不许有彩虹，也不许有 NONE */
        for (i2 = 0; i2 < UFO_TOKEN_FIELD_COLORS_N; ++i2) {
            int32_t c2 = k_ufo_token_field_colors[i2];
            CHECK(c2 != UFO_COLOR_RAINBOW);            /* ★ 「没有彩虹色 token」——这一条就是那句裁决 */
            CHECK(c2 != UFO_COLOR_NONE);
            if (c2 == UFO_COLOR_RED) seen3[1] = 1;
            if (c2 == UFO_COLOR_BLUE) seen3[2] = 1;
            if (c2 == UFO_COLOR_GREEN) seen3[3] = 1;
        }
        CHECK(seen3[1] && seen3[2] && seen3[3]);
        CHECK(!ufo_token_color_is_field(UFO_COLOR_RAINBOW));   /* 彩虹**不是**令牌颜色 */
        CHECK(!ufo_token_color_is_field(UFO_COLOR_NONE));
        CHECK(ufo_token_color_is_field(UFO_COLOR_GOLD));       /* 金是（≡ 蓝的调色版）*/
    }

    /* ================= ★★ （P2）随机取色（用户 2026-09-18 裁决：「P2 随机吧」）=================
     * 一手那条「颜色写死在敌人的 ECL 掉落表里」在 TH18 不可移植（TH18 没有这张表）⇒ 用户裁决随机。
     * 判据：① 永远落在那三色里（**这条是 2026-09-12 那次「颜色恒 NONE」事故的回归网**）；
     *      ② 走一遍 LCG 能把三色都取到（否则某一色的分支是死代码）；
     *      ③ 同一个种子可复现（单测能对账）；④ 种子不同 ⇒ 序列不同（不是常数）。 */
    {
        int i2, seen[4] = { 0, 0, 0, 0 };
        uint32_t seed_a = 0x12345678u, seed_c = 0x9e3779b9u;
        int diff = 0;
        for (i2 = 0; i2 < 600; ++i2) {
            int32_t c2;
            seed_a = ufo_rand_next(seed_a);
            c2 = ufo_token_color_for_roll(seed_a);
            CHECK(ufo_token_color_is_field(c2));       /* ① 永远在域内 */
            CHECK(c2 != UFO_COLOR_NONE && c2 != UFO_COLOR_RAINBOW);
            if (c2 == UFO_COLOR_RED) seen[1] = 1;
            if (c2 == UFO_COLOR_BLUE) seen[2] = 1;
            if (c2 == UFO_COLOR_GREEN) seen[3] = 1;
            /* ④ 两个不同种子的同一拍必须能不同（否则是常数、不是随机）*/
            seed_c = ufo_rand_next(seed_c);
            if (ufo_token_color_for_roll(seed_c) != c2) diff = 1;
        }
        CHECK(seen[1] && seen[2] && seen[3]);          /* ② 三色都取得到 */
        CHECK(diff);                                   /* ④ 真的在随机（不是常数列）*/

        /* ③ 可复现：同一个种子跑两遍必须逐拍相同 */
        {
            uint32_t s1 = 0xdeadbeefu, s2 = 0xdeadbeefu;
            int same = 1;
            for (i2 = 0; i2 < 64; ++i2) {
                s1 = ufo_rand_next(s1); s2 = ufo_rand_next(s2);
                if (ufo_token_color_for_roll(s1) != ufo_token_color_for_roll(s2)) same = 0;
            }
            CHECK(same);
        }
        /* ⚠ 取的是**高位**那一段（LCG 低位图案明显）⇒ **只动低 16 位不该改变取到的颜色**。
         *   （反过来若哪天改成 `roll % 3`（吃低位），这一条会红 —— 它就是那个改法的报警器。） */
        CHECK(ufo_token_color_for_roll(0x00000000u) == ufo_token_color_for_roll(0x0000ffffu));
        CHECK(ufo_token_color_for_roll(0x00050000u) != ufo_token_color_for_roll(0x00060000u));
        /* ★ 金令牌只是**蓝的调色版**（裁决「新增 3」）：它只替换蓝，且两种都出得来 */
        {
            int seen_gold = 0, seen_blue = 0, bad = 0;
            CHECK(UFO_GOLD_CYCLE >= 2);                /* 1 = 每颗都金 ⇒ 蓝色令牌再也不出现 */
            for (i2 = 0; i2 < 512; ++i2) {
                uint32_t roll = ufo_rand_next((uint32_t)(0x51ed2701u + (uint32_t)i2 * 2654435761u));
                int32_t base = ufo_token_color_for_roll(roll);
                int32_t with = ufo_token_color_gold_variant(base, roll);
                CHECK(ufo_token_color_is_field(with));
                if (base != UFO_COLOR_BLUE && with != base) bad = 1;      /* 只动蓝 */
                if (with == UFO_COLOR_GOLD && base != UFO_COLOR_BLUE) bad = 1;
                if (with == UFO_COLOR_GOLD) { seen_gold = 1; CHECK(ufo_color_norm(with) == UFO_COLOR_BLUE); }
                if (with == UFO_COLOR_BLUE) seen_blue = 1;
            }
            CHECK(bad == 0);
            CHECK(seen_gold && seen_blue);             /* ★ 实机上两种都能见到（否则 P 图那件事是死代码）*/
        }
    }

    /* ================= ★★ 令牌的**显示色**（一手：变色令牌按当时显示的那一色入账）=================
     * 变色令牌 = 一手 type 13/14/15（每 150 帧换色的那一只）。判据：
     *   ① 非变色令牌 = `ufo_color_norm(color)`（金 → 蓝）；② 变色令牌 = 第 `rb_state` 态对应的那一色
     *   （0/1/2 ↔ 红/蓝/绿，与脚本 215/216/217 同序）；③ 越界态夹到 0（不许读表外）。 */
    {
        int st;
        CHECK(UFO_TOK_RB_STATES_REF == 3);
        CHECK(ufo_token_display_color(UFO_COLOR_RED, 0, 0) == UFO_COLOR_RED);
        CHECK(ufo_token_display_color(UFO_COLOR_BLUE, 0, 2) == UFO_COLOR_BLUE);   /* 非变色：忽略 rb_state */
        CHECK(ufo_token_display_color(UFO_COLOR_GOLD, 0, 1) == UFO_COLOR_BLUE);   /* 金 ≡ 蓝 */
        CHECK(ufo_token_display_color(UFO_COLOR_RAINBOW, 1, 1) == UFO_COLOR_BLUE);/* 变色令牌：按态取值 */
        for (st = 0; st < UFO_TOK_RB_STATES_REF; ++st) {
            int32_t c2 = ufo_token_display_color(UFO_COLOR_GREEN, 1, st);
            CHECK(ufo_token_color_is_field(c2));       /* ★ 显示色永远在令牌域里（**没有彩虹**）*/
            CHECK(c2 != UFO_COLOR_RAINBOW);
            CHECK(c2 == k_ufo_cycler_colors[st]);
        }
        CHECK(ufo_token_display_color(UFO_COLOR_GREEN, 1, -1) == k_ufo_cycler_colors[0]);  /* 越界夹 0 */
        CHECK(ufo_token_display_color(UFO_COLOR_GREEN, 1, 99) == k_ufo_cycler_colors[0]);
        /* 三色各一那条规则仍然够得着（三个**不同**的显示色 ⇒ 彩虹 UFO） */
        {
            ufo_group_reset(&g);
            g.slot[0] = ufo_token_display_color(UFO_COLOR_RED, 0, 0);
            g.slot[1] = ufo_token_display_color(UFO_COLOR_BLUE, 0, 0);
            g.slot[2] = ufo_token_display_color(UFO_COLOR_GREEN, 1, 2);
            g.filled = UFO_SLOT_COUNT;
            CHECK(ufo_group_classify(&g) == UFO_COLOR_RAINBOW);
        }
    }

    /* ================= 有状态的自然序列（随机取色 + 凑组状态机）=================
     * 用 LCG 连喂 3N 颗**随机色**令牌（走 `ufo_group_put` 的返回码、成组就消费），判据：
     *   · 状态机一直在界内（`filled` 自洽）；· **两种凑组规则都够得着**（三同色 + 三互不相同）；
     *   · 至少出过几次成组（否则整条路是死的）。 */
    {
        int got_same = 0, got_rainbow = 0, steps = 0, i2;
        uint32_t seed = 0xc0ffee01u;
        ufo_group_reset(&g);
        for (i2 = 0; i2 < 900; ++i2) {
            int32_t c2;
            seed = ufo_rand_next(seed);
            c2 = ufo_token_color_for_roll(seed);
            if (ufo_group_put(&g, c2, NULL) == UFO_GROUP_PUT_READY) {
                int32_t made = ufo_group_classify(&g);
                if (made == UFO_COLOR_RAINBOW) got_rainbow = 1; else got_same = 1;
                (void)ufo_group_take_ready(&g);
                ufo_group_clear(&g);        /* P1：槽留到 UFO 结束才清，这里手动模拟「结束」*/
                ++steps;
            }
            CHECK(g.filled >= 0 && g.filled <= UFO_SLOT_COUNT);
            CHECK(ufo_slots_filled(&g) == g.filled);
        }
        CHECK(steps > 0);                              /* 900 颗里必然出过成组 */
        CHECK(got_same);                               /* ★ 三同色那条规则够得着 */
        CHECK(got_rainbow);                            /* ★ 三互不相同那条规则也够得着 */
    }

    /* ================================ 金的**凑组**位置（用户裁决 ① 的 ❓ 那一条）====================
     * 一手 `th12:0x422e80` 的颜色域只有 {1,2,3,−1}，**没有金** ⇒ 这一格是**决策不是一手**：
     *   `UFO_GOLD_COUNTS_AS_BLUE` = 1（本卡取值）金 ≡ 蓝；= 0 金当第 4 色。两种各写一份断言。 */
#if UFO_GOLD_COUNTS_AS_BLUE
    CHECK(ufo_color_norm(UFO_COLOR_GOLD) == UFO_COLOR_BLUE);
    CHECK(ufo_color_norm(UFO_COLOR_RED) == UFO_COLOR_RED);      /* 其它色原样 */
    CHECK(group_of(UFO_COLOR_GOLD, UFO_COLOR_GOLD, UFO_COLOR_GOLD, &g) == UFO_COLOR_BLUE);
    CHECK(group_of(UFO_COLOR_GOLD, UFO_COLOR_GOLD, UFO_COLOR_RAINBOW, &g) == UFO_COLOR_BLUE);
    /* 金+蓝+红 ≡ 蓝+蓝+红 = **两同一异**（不是「三色各一」！）—— 排列无关 */
    CHECK(group_of(UFO_COLOR_GOLD, UFO_COLOR_BLUE, UFO_COLOR_RED, &g) == UFO_COLOR_NONE);
    CHECK(group_of(UFO_COLOR_RED, UFO_COLOR_GOLD, UFO_COLOR_BLUE, &g) == UFO_COLOR_NONE);
    CHECK(group_of(UFO_COLOR_BLUE, UFO_COLOR_RED, UFO_COLOR_GOLD, &g) == UFO_COLOR_NONE);
#else
    CHECK(ufo_color_norm(UFO_COLOR_GOLD) == UFO_COLOR_GOLD);
    CHECK(group_of(UFO_COLOR_GOLD, UFO_COLOR_GOLD, UFO_COLOR_GOLD, &g) == UFO_COLOR_GOLD);
    CHECK(group_of(UFO_COLOR_GOLD, UFO_COLOR_BLUE, UFO_COLOR_RED, &g) == UFO_COLOR_RAINBOW);
#endif

    /* 非法颜色：拒绝且不改状态 */
    ufo_group_reset(&g2);
    CHECK(!ufo_slot_set(&g2, UFO_COLOR_NONE));
    CHECK(!ufo_slot_set(&g2, 99));
    CHECK(ufo_slots_filled(&g2) == 0);
    CHECK(!ufo_slot_set(NULL, UFO_COLOR_RED));         /* 空指针不崩 */
    ufo_group_reset(NULL);                             /* 空指针不崩 */
    CHECK(ufo_slots_filled(NULL) == 0);
    CHECK(ufo_group_classify(NULL) == UFO_COLOR_NONE);
    CHECK(!ufo_group_has_match(NULL));
    CHECK(ufo_group_take_ready(NULL) == UFO_COLOR_NONE);
    ufo_group_clear(NULL);                             /* 空指针不崩 */

    /* ==================================== 凑组规则（spec §0 定案 1：三同色 或 三互不相同） */
    CHECK(group_of(UFO_COLOR_RED, UFO_COLOR_RED, UFO_COLOR_RED, &g) == UFO_COLOR_RED);
    CHECK(group_of(UFO_COLOR_BLUE, UFO_COLOR_BLUE, UFO_COLOR_BLUE, &g) == UFO_COLOR_BLUE);
    CHECK(group_of(UFO_COLOR_GREEN, UFO_COLOR_GREEN, UFO_COLOR_GREEN, &g) == UFO_COLOR_GREEN);
    CHECK(group_of(UFO_COLOR_RAINBOW, UFO_COLOR_RAINBOW, UFO_COLOR_RAINBOW, &g) == UFO_COLOR_RAINBOW);
    /* 三互不相同 ⇒ 彩虹 UFO */
    CHECK(group_of(UFO_COLOR_RED, UFO_COLOR_BLUE, UFO_COLOR_GREEN, &g) == UFO_COLOR_RAINBOW);
    CHECK(group_of(UFO_COLOR_GREEN, UFO_COLOR_RED, UFO_COLOR_BLUE, &g) == UFO_COLOR_RAINBOW);
    CHECK(group_of(UFO_COLOR_BLUE, UFO_COLOR_GREEN, UFO_COLOR_RED, &g) == UFO_COLOR_RAINBOW);
    /* 彩虹 + 两个不同固定色 ⇒ 也是「三互不相同」⇒ 彩虹 */
    CHECK(group_of(UFO_COLOR_RAINBOW, UFO_COLOR_RED, UFO_COLOR_BLUE, &g) == UFO_COLOR_RAINBOW);
    CHECK(group_of(UFO_COLOR_RED, UFO_COLOR_RAINBOW, UFO_COLOR_GREEN, &g) == UFO_COLOR_RAINBOW);
    CHECK(group_of(UFO_COLOR_BLUE, UFO_COLOR_GREEN, UFO_COLOR_RAINBOW, &g) == UFO_COLOR_RAINBOW);
    /* 彩虹当任意色补位 ⇒ 两同色 + 彩虹 = 该色 */
    CHECK(group_of(UFO_COLOR_RED, UFO_COLOR_RED, UFO_COLOR_RAINBOW, &g) == UFO_COLOR_RED);
    CHECK(group_of(UFO_COLOR_BLUE, UFO_COLOR_RAINBOW, UFO_COLOR_BLUE, &g) == UFO_COLOR_BLUE);
    CHECK(group_of(UFO_COLOR_RAINBOW, UFO_COLOR_GREEN, UFO_COLOR_GREEN, &g) == UFO_COLOR_GREEN);
    CHECK(group_of(UFO_COLOR_RAINBOW, UFO_COLOR_RAINBOW, UFO_COLOR_GREEN, &g) == UFO_COLOR_GREEN);
    CHECK(group_of(UFO_COLOR_RED, UFO_COLOR_RAINBOW, UFO_COLOR_RAINBOW, &g) == UFO_COLOR_RED);
    /* ★ spec §9 #2 未拍板：「两同一异」⇒ **不召唤**（本实装取不做）*/
    CHECK(group_of(UFO_COLOR_RED, UFO_COLOR_RED, UFO_COLOR_BLUE, &g) == UFO_COLOR_NONE);
    CHECK(group_of(UFO_COLOR_RED, UFO_COLOR_BLUE, UFO_COLOR_RED, &g) == UFO_COLOR_NONE);
    CHECK(group_of(UFO_COLOR_GREEN, UFO_COLOR_RED, UFO_COLOR_GREEN, &g) == UFO_COLOR_NONE);
    CHECK(group_of(UFO_COLOR_BLUE, UFO_COLOR_BLUE, UFO_COLOR_GREEN, &g) == UFO_COLOR_NONE);
    CHECK(group_of(UFO_COLOR_RED, UFO_COLOR_BLUE, UFO_COLOR_RAINBOW, &g) == UFO_COLOR_RAINBOW);

    /* 未满 3 格 / 空格脏数据：都不合成 */
    ufo_group_reset(&g);
    ufo_slot_set(&g, UFO_COLOR_RED);
    ufo_slot_set(&g, UFO_COLOR_RED);
    CHECK(!ufo_group_has_match(&g));
    CHECK(ufo_group_classify(&g) == UFO_COLOR_NONE);
    ufo_group_reset(&g);
    g.slot[0] = UFO_COLOR_RED;
    g.slot[1] = UFO_COLOR_RED;
    g.slot[2] = UFO_COLOR_RED;
    CHECK(ufo_group_classify(&g) == UFO_COLOR_NONE);   /* filled 被篡改成 0（坏状态）⇒ 不合成 */
    g.filled = 3;
    CHECK(ufo_group_classify(&g) == UFO_COLOR_RED);
    g.slot[1] = 77;                                    /* 非法色 ⇒ 不合成 */
    CHECK(ufo_group_classify(&g) == UFO_COLOR_NONE);

    /* ================================ ufo_group_put：常规「先攒满 3 格」路径 */
    CHECK(UFO_GROUP_PUT_NONE == 0);
    CHECK(UFO_GROUP_PUT_HELD == 1);
    CHECK(UFO_GROUP_PUT_READY == 2);
    ufo_group_reset(&g);
    CHECK(ufo_group_put(&g, UFO_COLOR_RED, NULL) == UFO_GROUP_PUT_HELD);      /* 1 格：攒着 */
    CHECK(g.filled == 1);
    CHECK(ufo_group_put(&g, UFO_COLOR_RED, NULL) == UFO_GROUP_PUT_HELD);      /* 2 格：攒着 */
    CHECK(g.filled == 2);
    CHECK(ufo_group_put(&g, UFO_COLOR_RED, NULL) == UFO_GROUP_PUT_READY);     /* 3 同色：成组 */
    CHECK(g.filled == 3);
    CHECK(ufo_group_classify(&g) == UFO_COLOR_RED);
    /* 成组但还没被消费：再来一个令牌 ⇒ 仍然报 READY（**不能**把它丢掉，否则那个组合白攒）*/
    CHECK(ufo_group_put(&g, UFO_COLOR_GREEN, NULL) == UFO_GROUP_PUT_READY);
    CHECK(g.filled == 3);
    CHECK(g.slot[0] == UFO_COLOR_RED && g.slot[1] == UFO_COLOR_RED && g.slot[2] == UFO_COLOR_RED);
    /* ★★ P1（用户 2026-09-18「按一手」）：**取走"成组"这个结果时不清槽** ——
     *   一手在 UFO 结束时才清（`th12:0x44a326`-`0x44a344`），召唤只置自己的闩。
     *   ⇒ `take_ready` 之后 `filled` 仍是 3（"UFO 在场期间槽是满的"），只有 `clear` 才清空。 */
    CHECK(ufo_group_take_ready(&g) == UFO_COLOR_RED);
    CHECK(g.filled == 3);                              /* ★ 槽还在（= 收不进新令牌）*/
    CHECK(g.last_group == UFO_COLOR_RED);
    CHECK(g.last_slot2 == UFO_COLOR_RED);              /* 一手 `COLLECTED_UFOS[2]` 的快照 */
    CHECK(!ufo_slot_set(&g, UFO_COLOR_BLUE));          /* 满格 ⇒ 放不进去（P1 的核心可见行为）*/
    ufo_group_clear(&g);                               /* = UFO 结束 */
    CHECK(g.filled == 0);
    CHECK(g.last_group == UFO_COLOR_NONE && g.last_slot2 == UFO_COLOR_NONE);
    /* 三色各一 ⇒ READY */
    ufo_group_reset(&g);
    CHECK(ufo_group_put(&g, UFO_COLOR_RED, NULL) == UFO_GROUP_PUT_HELD);
    CHECK(ufo_group_put(&g, UFO_COLOR_BLUE, NULL) == UFO_GROUP_PUT_HELD);
    CHECK(ufo_group_put(&g, UFO_COLOR_GREEN, NULL) == UFO_GROUP_PUT_READY);
    CHECK(ufo_group_take_ready(&g) == UFO_COLOR_RAINBOW);
    CHECK(g.filled == 3);                          /* P1：仍然满着 */
    ufo_group_clear(&g);
    /* 非法颜色 / 空指针 */
    ufo_group_reset(&g);
    CHECK(ufo_group_put(&g, UFO_COLOR_NONE, NULL) == UFO_GROUP_PUT_NONE);
    CHECK(ufo_group_put(&g, 42, NULL) == UFO_GROUP_PUT_NONE);
    CHECK(ufo_group_put(NULL, UFO_COLOR_RED, NULL) == UFO_GROUP_PUT_NONE);
    CHECK(g.filled == 0);

    /* ======== ufo_group_put：★ 用户 2026-09-12 裁决 ② —— 「两同一异 ⇒ 丢最旧」照抄一手 ========
     * 一手 `th12:0x422e80`（本次用 objdump 复核过，指令原文：`0x422ecb mov [esi+8],edi` 放第 3 格；
     * `0x422ed4 cmp ecx,edi` / `0x422ed8 cmp eax,edi` 判三同色；`0x422ee3 cmp eax,edi` /
     * `0x422ee7 cmp ecx,eax` 判 KEEP2；`0x422eeb or eax,-1` ⇒ 彩虹；
     * KEEP2 体 `0x422ef8`–`0x422f12`：`slots[0]=slots[1]; slots[2]=0; cnt18=2; cnt1c=4; slots[1]=color`）：
     * **第三格填上那一刻**就判 —— 三同色 ⇒ 该色；三色各一 ⇒ 彩虹；
     * **两同一异 ⇒ 丢掉最旧的一格、保留最近两个**（不召唤）。
     * ⚠ 改之前这里是「先攒满 3 格、不成组就干等下一颗令牌」⇒ 槽里会长时间停在一个永远成不了组的
     *   3 格上（玩家看到 3 格满着、却什么都没发生）。 */
    {
        int32_t dropped = -1;
        int guard;
        /* ---- ★ 判定表：「两同一异」的**全部 6 种排列**（3 个位置 × 两种「谁当异色」）----
         * 记 A/B 为两个不同颜色：期望**逐条相同** —— 第 3 枚放下之后
         *     dropped = 1（丢掉最旧那一枚）、filled = **2**（保留最近两个）、
         *     slot[0] = 原来的第 2 枚、slot[1] = 刚放下的第 3 枚、**不成组**（不召唤）。
         * 也就是「手上永远只剩最近两个」（一手 KEEP2 的 `cnt18 = 2`）。 */
        static const int32_t k_perm[6][3] = {
            /* A A B */ { UFO_COLOR_RED,  UFO_COLOR_RED,  UFO_COLOR_BLUE },
            /* B B A */ { UFO_COLOR_BLUE, UFO_COLOR_BLUE, UFO_COLOR_RED  },
            /* A B A */ { UFO_COLOR_RED,  UFO_COLOR_BLUE, UFO_COLOR_RED  },
            /* B A B */ { UFO_COLOR_BLUE, UFO_COLOR_RED,  UFO_COLOR_BLUE },
            /* B A A */ { UFO_COLOR_BLUE, UFO_COLOR_RED,  UFO_COLOR_RED  },
            /* A B B */ { UFO_COLOR_RED,  UFO_COLOR_BLUE, UFO_COLOR_BLUE },
        };
        int pi;
        for (pi = 0; pi < 6; ++pi) {
            int32_t keep0, keep1, probe;
            ufo_group_reset(&g);
            dropped = -1;
            CHECK(ufo_group_put(&g, k_perm[pi][0], &dropped) == UFO_GROUP_PUT_HELD);   /* 1 枚：攒着 */
            CHECK(g.filled == 1);
            CHECK(dropped == 0);
            CHECK(ufo_group_put(&g, k_perm[pi][1], &dropped) == UFO_GROUP_PUT_HELD);   /* 2 枚：攒着 */
            CHECK(g.filled == 2);
            CHECK(dropped == 0);
            /* 第 3 枚 = 两同一异 ⇒ **当场**丢最旧、保留最近两个 */
            CHECK(ufo_group_put(&g, k_perm[pi][2], &dropped) == UFO_GROUP_PUT_HELD);   /* ★ 不召唤 */
            CHECK(dropped == 1);                          /* ★ 丢掉最旧那一枚 */
            CHECK(g.filled == 2);                         /* ★ 只剩「最近两个」 */
            CHECK(ufo_slots_filled(&g) == 2);
            CHECK(!ufo_group_has_match(&g));              /* 2 格判不出组 */
            CHECK(ufo_group_classify(&g) == UFO_COLOR_NONE);
            CHECK(g.slot[0] == k_perm[pi][1]);            /* 原来的第 2 枚 */
            CHECK(g.slot[1] == k_perm[pi][2]);            /* 刚放下的第 3 枚 */
            CHECK(g.slot[2] == UFO_COLOR_NONE);
            /* 丢完**不是死锁**：再补一枚「能补上」的颜色就成组（这条就是裁决 ② 要的效果）。
             * 手算补哪一枚：留下的两枚 `{x,y}` —— x == y ⇒ 补 x（三同色）；x ≠ y ⇒ 补第 3 色（三色各一）。
             * 六个排列里 {B,A,A}/{A,B,B} 留下的两枚同色（补自身），其余四个留下的两枚不同色（补绿）。 */
            keep0 = g.slot[0];
            keep1 = g.slot[1];
            probe = (keep0 == keep1) ? keep0 : UFO_COLOR_GREEN;
            dropped = -1;
            CHECK(ufo_group_put(&g, probe, &dropped) == UFO_GROUP_PUT_READY);
            CHECK(dropped == 0);                          /* 这一枚是成组的那一枚，不是丢 */
            CHECK(keep0 == keep1 ? ufo_group_classify(&g) == keep0
                                 : ufo_group_classify(&g) == UFO_COLOR_RAINBOW);
            CHECK(ufo_group_take_ready(&g) != UFO_COLOR_NONE);
            CHECK(g.filled == 3);                     /* ★ P1：取走结果不清槽（一手在 UFO 结束时清）*/
            ufo_group_clear(&g);
            CHECK(g.filled == 0);
        }

        /* ---- 具体一格手算一遍（红 蓝 红 = 首尾同色 ⇒ **一手也走 KEEP2 ⇒ 丢最旧**）----
         * ★ 2026-09-12 订正：这一格过去被写成「我们偏离一手」，原因是漏读了一手 KEEP2 的
         *   第一条分支（`0x422ed4`/`0x422ed6`：`slots[0]==color` 且 `slots[1]!=color` 也跳 `0x422ef3`）。
         *   三条分支抄全之后，「两同一异」的三种排列（含首尾同色）**全部**走 KEEP2 ⇒ 我们的口径
         *   与字面一手**一致**。 */
        ufo_group_reset(&g);
        CHECK(ufo_group_put(&g, UFO_COLOR_RED, &dropped) == UFO_GROUP_PUT_HELD);
        CHECK(dropped == 0);
        CHECK(ufo_group_put(&g, UFO_COLOR_BLUE, &dropped) == UFO_GROUP_PUT_HELD);
        CHECK(dropped == 0);
        dropped = -1;
        CHECK(ufo_group_put(&g, UFO_COLOR_RED, &dropped) == UFO_GROUP_PUT_HELD);
        CHECK(dropped == 1);                              /* ★ 丢掉最旧的红 */
        CHECK(g.filled == 2);
        CHECK(g.slot[0] == UFO_COLOR_BLUE);               /* 红 蓝 红 → 蓝 红 */
        CHECK(g.slot[1] == UFO_COLOR_RED);
        CHECK(ufo_group_classify(&g) == UFO_COLOR_NONE);

        /* ★★ 「字面一手条件」对照表（钉住：**两同一异的三条排列全部 KEEP2**，只有三色互不相同才彩虹）：
         * 一手三条分支 —— ① `slots[0]==color && slots[1]!=color`（`0x422ed4`+`0x422ed6`）
         *                ② `slots[1]==color`（`0x422ee3`）
         *                ③ `slots[0]==slots[1]`（`0x422ee7`）
         * 三条都不成立 ⇒ `0x422eeb or eax,-1` = 彩虹。 */
        {
            ufo_group_t k2;
            ufo_group_reset(&k2);
            k2.slot[0] = UFO_COLOR_RED;  k2.slot[1] = UFO_COLOR_BLUE; k2.slot[2] = UFO_COLOR_RED;
            k2.filled = 3;
            CHECK(ufo_group_keep2_literal_th12(&k2, UFO_COLOR_RED));    /* ★ 首尾同色：① 命中 ⇒ KEEP2 */
            CHECK(ufo_group_keep2_literal_th12(&k2, UFO_COLOR_BLUE));   /* slots[1]==color ⇒ ② 走 KEEP2 */
            k2.slot[0] = UFO_COLOR_RED; k2.slot[1] = UFO_COLOR_RED; k2.slot[2] = UFO_COLOR_BLUE;
            CHECK(ufo_group_keep2_literal_th12(&k2, UFO_COLOR_GREEN));  /* slots[0]==slots[1] ⇒ ③ 走 KEEP2 */
            /* 三条都不成立 = 三色互不相同（例：槽里 红/蓝、新来 绿）⇒ **不**是 KEEP2 ⇒ 彩虹 */
            k2.slot[0] = UFO_COLOR_RED; k2.slot[1] = UFO_COLOR_BLUE;
            CHECK(!ufo_group_keep2_literal_th12(&k2, UFO_COLOR_GREEN));
            CHECK(!ufo_group_keep2_literal_th12(&k2, UFO_COLOR_RAINBOW));
            /* 金令牌彼此相等（与「金算不算独立一色」这个 ❓ 取值无关 —— 两枚金始终同桶）*/
            k2.slot[0] = UFO_COLOR_GOLD; k2.slot[1] = UFO_COLOR_GOLD; k2.slot[2] = UFO_COLOR_RED;
            CHECK(ufo_group_keep2_literal_th12(&k2, UFO_COLOR_GREEN));  /* slots[0]==slots[1] ⇒ 走 KEEP2 */
            CHECK(!ufo_group_keep2_literal_th12(NULL, UFO_COLOR_RED));  /* 空指针不崩 */
        }

        /* 从「两同一异」的序列连续喂同色：必须在**有限步**内重新成组（不是死循环）*/
        ufo_group_reset(&g);
        CHECK(ufo_group_put(&g, UFO_COLOR_RED, NULL) == UFO_GROUP_PUT_HELD);
        CHECK(ufo_group_put(&g, UFO_COLOR_BLUE, NULL) == UFO_GROUP_PUT_HELD);
        CHECK(ufo_group_put(&g, UFO_COLOR_RED, NULL) == UFO_GROUP_PUT_HELD);  /* 第 3 枚：丢最旧 ⇒ {蓝,红} */
        CHECK(g.filled == 2);
        CHECK(g.slot[0] == UFO_COLOR_BLUE && g.slot[1] == UFO_COLOR_RED);
        guard = 0;
        dropped = 0;
        while (!ufo_group_has_match(&g) && guard < 32) {
            (void)ufo_group_put(&g, UFO_COLOR_RED, &dropped);
            ++guard;
        }
        CHECK(guard < 32);                             /* ★ 有限步内一定成组 */
        CHECK(guard == 2);                             /* 手算：① {蓝,红}+红 ⇒ {红,红}；② 再来红 ⇒ 红红红 */
        CHECK(ufo_group_has_match(&g));
        CHECK(ufo_group_classify(&g) == UFO_COLOR_RED);       /* 红红红 */
        CHECK(ufo_group_take_ready(&g) == UFO_COLOR_RED);     /* ★ P1：取结果不清槽 */
        CHECK(g.filled == 3);
        ufo_group_clear(&g);                                  /* = UFO 结束那一刻（一手 0x44a326）*/
        CHECK(g.filled == 0);
    }

    /* ==================== ★ 五边形框触发条件（用户 2026-09-12 裁决 ③；纯逻辑判据）====================
     * 用户原话：「玩家手上**有两个令牌**（= 再来一枚就能成组）时，屏幕上**这类令牌**外面会有一圈
     * 五边形框」。两级判据：
     *   ① `ufo_group_pentagon_hint(g)`：槽里此刻**差一枚就成组**（∃ 某个颜色能补上）；
     *   ② `ufo_pentagon_hint_for_color(g, c)`：**场上某一枚**令牌（收它 = 放颜色 c 进槽）够不够格被画框。
     * ⚠ 本卡**画不画**这个框不在本层（本层只给判据）；★ 2026-09-18 订正：框**已实装**
     *   （`ufo.c` 的 `ufo_pentagon_hint_update()` + 脚本 223）——原文「缺 anm 脚本 / 只有预留钩子」过期；
     *   这一节只钉「什么时候该画」。 */
    ufo_group_reset(&g);
    CHECK(!ufo_group_pentagon_hint(&g));               /* 0 枚 ⇒ 没有 */
    CHECK(ufo_group_put(&g, UFO_COLOR_RED, NULL) == UFO_GROUP_PUT_HELD);
    CHECK(!ufo_group_pentagon_hint(&g));               /* 1 枚 ⇒ 还不够一枚就成组 */
    CHECK(ufo_group_put(&g, UFO_COLOR_RED, NULL) == UFO_GROUP_PUT_HELD);
    CHECK(ufo_group_pentagon_hint(&g));                /* ★ 2 枚 ⇒ 来 */
    CHECK(ufo_pentagon_hint_for_color(&g, UFO_COLOR_RED));      /* 收了红 ⇒ 红红红 ⇒ 成组 */
    /* ⚠ 下面这条是**防御支测试**（★ 2026-09-18 新增 2 之后，生产路径上不存在彩虹令牌）：
     *   它守的是「坏数据进来了也别锁死状态机」，不是「玩家会收到彩虹令牌」。 */
    CHECK(ufo_pentagon_hint_for_color(&g, UFO_COLOR_RAINBOW));  /* 防御支：红红彩 ⇒ 红 UFO */
    CHECK(!ufo_pentagon_hint_for_color(&g, UFO_COLOR_GREEN));   /* 收了绿 ⇒ 红红绿 ⇒ 不成组 */
    CHECK(!ufo_pentagon_hint_for_color(&g, UFO_COLOR_BLUE));
    CHECK(!ufo_pentagon_hint_for_color(&g, UFO_COLOR_NONE));    /* 非法色 ⇒ 不算 */
    /* 两枚**异色**：任意第三色里只有「凑齐三色」的那些能补上（红蓝 ⇒ 绿/彩虹行，红/蓝/金不行）*/
    ufo_group_reset(&g);
    CHECK(ufo_group_put(&g, UFO_COLOR_RED, NULL) == UFO_GROUP_PUT_HELD);
    CHECK(ufo_group_put(&g, UFO_COLOR_BLUE, NULL) == UFO_GROUP_PUT_HELD);
    CHECK(ufo_group_pentagon_hint(&g));                         /* 存在补得上的颜色（绿）*/
    CHECK(!ufo_pentagon_hint_for_color(&g, UFO_COLOR_RED));     /* 红 ⇒ {红,蓝,红} 两同一异 */
#if UFO_GOLD_COUNTS_AS_BLUE
    CHECK(!ufo_pentagon_hint_for_color(&g, UFO_COLOR_GOLD));    /* ★ 金 ≡ 蓝 ⇒ 也是两同一异（默认取值下）*/
#else
    CHECK(ufo_pentagon_hint_for_color(&g, UFO_COLOR_GOLD));     /* 金当第 4 色 ⇒ {红,蓝,金} 三色各一 */
#endif
    CHECK(ufo_pentagon_hint_for_color(&g, UFO_COLOR_GREEN));    /* 三色各一 ⇒ 彩虹 */
    CHECK(ufo_pentagon_hint_for_color(&g, UFO_COLOR_RAINBOW));  /* 三色各一（彩虹算第 4 色）*/
    /* 3 枚（成组待兑现）时**不是**「差一枚」状态 */
    ufo_group_reset(&g);
    CHECK(ufo_group_put(&g, UFO_COLOR_RED, NULL) == UFO_GROUP_PUT_HELD);
    CHECK(ufo_group_put(&g, UFO_COLOR_RED, NULL) == UFO_GROUP_PUT_HELD);
    CHECK(ufo_group_put(&g, UFO_COLOR_RED, NULL) == UFO_GROUP_PUT_READY);
    CHECK(!ufo_group_pentagon_hint(&g));
    CHECK(!ufo_pentagon_hint_for_color(&g, UFO_COLOR_RED));
    (void)ufo_group_take_ready(&g);
    ufo_group_clear(&g);
    /* 空指针 */
    CHECK(!ufo_group_pentagon_hint(NULL));
    CHECK(!ufo_pentagon_hint_for_color(NULL, UFO_COLOR_RED));

    /* ==================== 合成取走（`ufo_group_take_ready`）与 last_group / last_slot2 ====================
     * ★★ P1（2026-09-18 用户裁决「按一手」）：**取走结果不清槽**；清槽是**另一个**函数
     *   `ufo_group_clear()`，唯一的调用点是 UFO 结束（一手 `0x44a326`-`0x44a344`）。 */
    CHECK(group_of(UFO_COLOR_GREEN, UFO_COLOR_GREEN, UFO_COLOR_GREEN, &g) == UFO_COLOR_GREEN);
    c = ufo_group_take_ready(&g);
    CHECK(c == UFO_COLOR_GREEN);
    CHECK(g.filled == 3);                              /* ★ 槽还在（P1：UFO 在场期间槽是满的）*/
    CHECK(g.slot[0] == UFO_COLOR_GREEN && g.slot[1] == UFO_COLOR_GREEN && g.slot[2] == UFO_COLOR_GREEN);
    CHECK(g.last_group == UFO_COLOR_GREEN);            /* 彩虹 UFO 的「按最后令牌取色」要用它 */
    CHECK(g.last_slot2 == UFO_COLOR_GREEN);            /* 一手 `COLLECTED_UFOS[2]` */
    CHECK(ufo_group_take_ready(&g) == UFO_COLOR_GREEN);/* 槽还满着 ⇒ 再取一次仍拿得到同一个组 */
    CHECK(g.last_group == UFO_COLOR_GREEN);
    ufo_group_clear(&g);                               /* = UFO 结束那一刻 */
    CHECK(g.filled == 0);                              /* 三个槽清 0（一手 `0x44a326`-`0x44a344`）*/
    CHECK(g.slot[0] == UFO_COLOR_NONE);
    CHECK(g.slot[1] == UFO_COLOR_NONE);
    CHECK(g.slot[2] == UFO_COLOR_NONE);
    CHECK(g.last_group == UFO_COLOR_NONE);             /* clear 也清快照（与一手"结束分支清计数"同款）*/
    CHECK(ufo_group_take_ready(&g) == UFO_COLOR_NONE); /* 没得合 ⇒ 状态不变 */
    /* 第二次合成覆盖 last_group */
    CHECK(group_of(UFO_COLOR_RED, UFO_COLOR_BLUE, UFO_COLOR_GREEN, &g) == UFO_COLOR_RAINBOW);
    c = ufo_group_take_ready(&g);
    CHECK(c == UFO_COLOR_RAINBOW);
    CHECK(g.last_group == UFO_COLOR_RAINBOW);
    /* 合成后能继续攒 —— ★ P1：得**先清槽**（= UFO 结束），否则槽还满着（3 格）放不进去 */
    ufo_group_clear(&g);
    CHECK(ufo_slot_set(&g, UFO_COLOR_BLUE));
    CHECK(g.filled == 1);
    CHECK(g.last_group == UFO_COLOR_NONE);             /* clear 把 last_group 清了（一手结束分支同款）*/

    /* ==================================================== UFO 生命周期（§3.3） */
    ufo_reset(&u);
    CHECK(!ufo_present(&u));
    CHECK(u.ufo_id == 0);
    CHECK(u.frames == 0);
    CHECK(u.color == UFO_COLOR_NONE);
    CHECK(u.fill_millionths == 0);
    CHECK(u.cnt_p == 0 && u.cnt_money == 0);
    CHECK(ufo_can_spawn(&u));
    CHECK(!ufo_can_absorb(&u));                        /* 场上没有 ⇒ 不吸 */
    CHECK(!ufo_spawn(&u, 0, UFO_COLOR_RED, 999));           /* 非法 id */
    CHECK(!ufo_spawn(NULL, 1, UFO_COLOR_RED, 999));         /* 空指针 */
    CHECK(ufo_spawn(&u, 4242u, UFO_COLOR_RED, 999));
    CHECK(ufo_present(&u));
    CHECK(u.ufo_id == 4242u);
    CHECK(u.frames == 0);
    CHECK(u.color == UFO_COLOR_RED);
    CHECK(u.fill_millionths == 0);
    CHECK(!ufo_can_spawn(&u));                         /* 单例：场上有一个就不许再召唤 */
    CHECK(!ufo_spawn(&u, 4243u, UFO_COLOR_BLUE, 999));      /* 被拒且不改状态 */
    CHECK(u.ufo_id == 4242u);
    CHECK(u.color == UFO_COLOR_RED);

    /* 吸收门：一手是「存活帧数 > 59」（`th12:0x4271c0`，`UFO:204`）*/
    CHECK(UFO_ABSORB_PAUSE == 59);
    CHECK(UFO_ABSORB_FIRST_FRAME == 60);
    for (i = 0; i <= UFO_ABSORB_PAUSE; ++i) {          /* frames = 0..59：一律不吸 */
        CHECK(!ufo_can_absorb(&u));
        CHECK(!ufo_tick_advance(&u));                  /* 也没到存活上限 */
    }
    CHECK(u.frames == UFO_ABSORB_FIRST_FRAME);         /* = 60 */
    CHECK(ufo_can_absorb(&u));                         /* 第 60 帧起能吸（一手 > 59）*/
    CHECK(!ufo_tick_advance(&u));                      /* 60 帧远没到存活上限 720 */
    CHECK(u.frames == 61);
    CHECK(ufo_can_absorb(&u));
    ufo_reset(&u);
    CHECK(ufo_spawn(&u, 7u, UFO_COLOR_RAINBOW, 999));
    u.frames = UFO_ABSORB_FIRST_FRAME;                 /* 正好 60 ⇒ 可以吸 */
    CHECK(ufo_can_absorb(&u));
    u.frames = UFO_ABSORB_PAUSE;                       /* 59 ⇒ 不行（边界）*/
    CHECK(!ufo_can_absorb(&u));

    /* 存活上限 720（✅ 一手 `zUfo+0x28`，`th12:0x44a8a0`）*/
    CHECK(UFO_LIFE_MAX == 720);
    u.frames = UFO_LIFE_MAX - 1;
    CHECK(ufo_tick_advance(&u));                       /* 到点：返回非 0 = 本帧达到上限 */
    CHECK(u.frames == UFO_LIFE_MAX);
    CHECK(ufo_tick_advance(&u));                       /* 到顶后不再涨，且仍然报「到点」 */
    CHECK(u.frames == UFO_LIFE_MAX);
    CHECK(!ufo_tick_advance(NULL));
    ufo_ufo_t empty;
    ufo_reset(&empty);
    CHECK(!ufo_tick_advance(&empty));                  /* 场上没有 ⇒ 不推进 */
    ufo_reset(NULL);                                   /* 空指针不崩 */
    ufo_finish(NULL);
    CHECK(!ufo_present(NULL));
    CHECK(!ufo_can_spawn(NULL));
    CHECK(!ufo_can_absorb(NULL));

    /* 击破 / 飞离 **共用**收尾（spec §3.5 `ufo_finish()`）*/
    ufo_reset(&u);
    CHECK(ufo_spawn(&u, 99u, UFO_COLOR_GREEN, 999));
    u.frames = 300;
    u.cnt_p = 12;
    u.cnt_money = 34;
    u.fill_millionths = 750000;
    u.absorbed = 46;
    ufo_finish(&u);
    CHECK(!ufo_present(&u));
    CHECK(u.ufo_id == 0);
    CHECK(u.frames == 0);
    CHECK(u.color == UFO_COLOR_NONE);
    CHECK(u.fill_millionths == 0);
    CHECK(u.cnt_p == 0 && u.cnt_money == 0);           /* 照 TH12 结束分支「清计数」（`UFO:104-107`）*/
    CHECK(ufo_can_spawn(&u));                          /* 收尾后可以再召唤 */
    CHECK(ufo_spawn(&u, 100u, UFO_COLOR_BLUE, 999));
    CHECK(u.absorbed == 0);                            /* spawn 也清观测计数 */
    CHECK(u.color == UFO_COLOR_BLUE);

    /* ==================================================== 吸收门（§3.4，一手 type 1/2） */
    CHECK(UFO_ITEM_TYPE_P == 1);
    CHECK(UFO_ITEM_TYPE_MONEY == 2);
    CHECK(UFO_ABSORB_TYPES_N == 2);
    CHECK(ufo_absorb_type_index(1) == 0);
    CHECK(ufo_absorb_type_index(2) == 1);
    CHECK(ufo_absorb_type_index(0) == -1);
    CHECK(ufo_absorb_type_index(3) == -1);
    CHECK(ufo_absorb_type_index(4) == -1);             /* 命碎片不吸（一手只吸 1/2）*/
    CHECK(ufo_absorb_type_index(6) == -1);             /* 符卡碎不吸 */
    CHECK(ufo_absorb_type_index(0x10) == -1);          /* 卡道具不吸 */
    CHECK(ufo_absorb_type_index(0x13) == -1);
    /* state 过滤：0 空槽 / 3 真空 / 4 环吸 一律不碰；1 / 2 / 5 可以 */
    CHECK(!ufo_absorbable(1, 0, 7u, 1));
    CHECK(!ufo_absorbable(1, 3, 7u, 1));               /* 栅栏③：别抢引擎的真空飞行 */
    CHECK(!ufo_absorbable(1, 4, 7u, 1));               /* 栅栏③：别抢 70 px 环吸 */
    CHECK(ufo_absorbable(1, 1, 7u, 1));
    CHECK(ufo_absorbable(1, 2, 7u, 1));
    CHECK(ufo_absorbable(1, 5, 7u, 1));
    CHECK(ufo_absorbable(2, 1, 7u, 1));
    CHECK(!ufo_absorbable(3, 1, 7u, 1));               /* type 不在 {1,2} */
    CHECK(!ufo_absorbable(1, 1, 0u, 1));               /* 场上没有 UFO */
    CHECK(!ufo_absorbable(1, 1, 7u, 0));               /* 槽指针没取到（坏槽）*/
    CHECK(!ufo_absorbable(1, 3, 0u, 0));               /* 多条件同时不满足 */

    /* 捕获半径 18 px（✅ 一手 `th12:0x426879`；spec §4「可作候选照抄」）*/
    CHECK(UFO_CAPTURE_DIST == 18.0f);
    CHECK(UFO_CAPTURE_DIST_SQ == 324.0f);
    CHECK(ufo_within_capture(0.0f, 0.0f));
    CHECK(ufo_within_capture(17.9f, 0.0f));
    CHECK(!ufo_within_capture(18.0f, 0.0f));           /* 严格 <：边界不算 */
    CHECK(!ufo_within_capture(18.1f, 0.0f));
    CHECK(ufo_within_capture(10.0f, 10.0f));           /* d² = 200 < 324 */
    CHECK(!ufo_within_capture(15.0f, 15.0f));          /* d² = 450 > 324 */
    CHECK(ufo_within_capture_sq(323.9f));
    CHECK(!ufo_within_capture_sq(324.0f));
    CHECK(!ufo_within_capture_sq(400.0f));

    /* =============================== 拉拽速度曲线（一手 TH12 公式，spec §4 🟡 待实跑标定） */
    CHECK(UFO_DRAG_SPEED_REF_FRAMES == 600.0f);
    CHECK(UFO_DRAG_SPEED_BASE == 0.02f);
    CHECK(UFO_DRAG_SPEED_SLOPE == 0.05f);
    CHECK(UFO_DRAG_SPEED_MAX == 6.0f);
    CHECK(feq(ufo_drag_speed(0), 0.02f, 1e-6f));       /* timer 0 ⇒ 只有 +0.02 */
    CHECK(feq(ufo_drag_speed(600), 0.05f + 0.02f, 1e-6f));
    CHECK(feq(ufo_drag_speed(1200), 0.05f + 0.02f, 1e-6f));   /* timer 比值夹到 1（与「上限 6.0」一致）*/
    CHECK(feq(ufo_drag_speed(300), 0.025f + 0.02f, 1e-6f));
    CHECK(ufo_drag_speed(100000) <= UFO_DRAG_SPEED_MAX);
    CHECK(ufo_drag_speed(100000) >= UFO_DRAG_SPEED_BASE);
    for (i = 0; i < 700; i += 50) {
        sp = ufo_drag_speed((uint32_t)i);
        CHECK(sp >= UFO_DRAG_SPEED_BASE && sp <= UFO_DRAG_SPEED_MAX);   /* 单调且在界内 */
    }
    /* 单调不减 */
    {
        float prev = ufo_drag_speed(0);
        int mono = 1;
        for (i = 1; i <= 600; ++i) {
            float s = ufo_drag_speed((uint32_t)i);
            if (s < prev - 1e-7f) mono = 0;
            prev = s;
        }
        CHECK(mono);
    }

    /* 方向：未归一化位移（比例步进不需要单位向量；`ufo_drag_step` 自己按距离缩放）*/
    CHECK(ufo_drag_delta(0.0f, 0.0f, 10.0f, 0.0f, &dx, &dy));
    CHECK(feq(dx, 10.0f, 1e-6f));
    CHECK(feq(dy, 0.0f, 1e-6f));
    CHECK(ufo_drag_delta(0.0f, 0.0f, 0.0f, 10.0f, &dx, &dy));
    CHECK(feq(dx, 0.0f, 1e-6f));
    CHECK(feq(dy, 10.0f, 1e-6f));
    CHECK(ufo_drag_delta(0.0f, 0.0f, -10.0f, 0.0f, &dx, &dy));
    CHECK(feq(dx, -10.0f, 1e-6f));
    CHECK(ufo_drag_delta(100.0f, 50.0f, 103.0f, 54.0f, &dx, &dy));
    CHECK(feq(dx, 3.0f, 1e-6f));
    CHECK(feq(dy, 4.0f, 1e-6f));
    CHECK(ufo_drag_delta(100.0f, 50.0f, 100.0f, 50.0f, &dx, &dy) == 0);   /* 重合 ⇒ 无方向 */
    CHECK(!ufo_drag_delta(0.0f, 0.0f, 100.0f, 0.0f, NULL, &dy));          /* 空出参 */
    CHECK(!ufo_drag_delta(0.0f, 0.0f, 100.0f, 0.0f, &dx, NULL));

    /* 比例步进：`d > speed` ⇒ 每帧正好走 speed；`d <= speed` ⇒ 这一帧压上去，**绝不越过** */
    px = 0.0f; py = 0.0f;
    ufo_drag_step(&px, &py, 100.0f, 0.0f, 2.5f);       /* d = 100 > 2.5 */
    CHECK(feq(px, 2.5f, 1e-4f));
    CHECK(feq(py, 0.0f, 1e-6f));
    ufo_drag_step(&px, &py, 100.0f, 0.0f, 2.5f);
    CHECK(feq(px, 5.0f, 1e-4f));
    /* 3-4-5 方向：每帧位移的模 = speed */
    px = 0.0f; py = 0.0f;
    ufo_drag_step(&px, &py, 300.0f, 400.0f, 5.0f);
    CHECK(feq(px * px + py * py, 25.0f, 1e-2f));        /* 位移长度² = speed² */
    /* 贴脸：距离小于 speed ⇒ 正好落到 UFO 位置（不过冲）*/
    px = 0.0f; py = 0.0f;
    ufo_drag_step(&px, &py, 1.0f, 0.0f, 6.0f);
    CHECK(feq(px, 1.0f, 1e-6f));
    CHECK(feq(py, 0.0f, 1e-6f));
    /* 已经重合：不动 */
    ufo_drag_step(&px, &py, 1.0f, 0.0f, 6.0f);
    CHECK(feq(px, 1.0f, 1e-6f));
    /* 多次步进必须收敛到 UFO 且不越过（不过冲是比例步进的全部意义）。
     * 起点 (-140,100) → 目标 (20,30)：距离 ≈ 172 px；每步只走 speed（0.02..0.07）
     * ⇒ 需要 ~2500 步（前 60 帧 speed 只有 0.02，几乎不动）。 */
    px = -140.0f; py = 100.0f;
    for (i = 0; i < 3200; ++i) ufo_drag_step(&px, &py, 20.0f, 30.0f, ufo_drag_speed((uint32_t)i));
    CHECK(feq(px, 20.0f, 1e-2f));
    CHECK(feq(py, 30.0f, 1e-2f));
    /* speed <= 0 / 空指针：不动 */
    px = 5.0f; py = 5.0f;
    ufo_drag_step(&px, &py, 0.0f, 0.0f, 0.0f);
    CHECK(feq(px, 5.0f, 1e-6f));
    ufo_drag_step(&px, &py, 0.0f, 0.0f, -3.0f);
    CHECK(feq(px, 5.0f, 1e-6f));
    ufo_drag_step(NULL, &py, 10.0f, 10.0f, 1.0f);        /* 空指针不崩 */
    ufo_drag_step(&px, NULL, 10.0f, 10.0f, 1.0f);
    CHECK(UFO_ITEM_VEL_DAMP == 0.0f);                  /* vel 清零（spec §3.4 硬要求）*/

    /* ============================ 围栏三件（spec §3.4；一手复核见 ufo.c 的地址表） */
    /* ① 屏内：|x| >= 200 或 y > 472 ⇒ 引擎当帧删（`0x445e38`，常量 `0x4b9340`/`0x4b93a4`）*/
    CHECK(ufo_target_on_screen(0.0f, 0.0f));
    CHECK(ufo_target_on_screen(140.0f, 100.0f));
    CHECK(ufo_target_on_screen(-140.0f, 100.0f));
    CHECK(ufo_target_on_screen(199.9f, 471.9f));
    CHECK(!ufo_target_on_screen(200.0f, 0.0f));        /* 边界即出界（`jb` 才留 ⇒ >= 就删）*/
    CHECK(!ufo_target_on_screen(-200.0f, 0.0f));
    CHECK(!ufo_target_on_screen(200.1f, 0.0f));
    CHECK(!ufo_target_on_screen(-500.0f, 0.0f));
    CHECK(!ufo_target_on_screen(0.0f, 472.1f));
    CHECK(!ufo_target_on_screen(0.0f, 1000.0f));
    /* ② 自机 30 px 收点圈（一手 `0x44625f` + `player+0x4798c` = 30.0）*/
    CHECK(UFO_FRAME_HALF_W == 140.0f);                 /* 一手活动框 280×128 ⇒ 半宽 140 */
    CHECK(UFO_FRAME_Y_TOP == 64.0f);
    CHECK(UFO_FRAME_Y_BOTTOM == 192.0f);
    CHECK(!ufo_target_clear_of_player(0.0f, 0.0f, 0.0f, 0.0f));      /* 就压在自机上 */
    CHECK(!ufo_target_clear_of_player(29.9f, 0.0f, 0.0f, 0.0f));
    CHECK(ufo_target_clear_of_player(21.3f, 21.3f, 0.0f, 0.0f));      /* d² ≈ 907 > 900 ⇒ 安全 */
    CHECK(ufo_target_clear_of_player(25.0f, 25.0f, 0.0f, 0.0f));      /* d² = 1250 > 900 ⇒ 安全 */
    CHECK(ufo_target_clear_of_player(31.0f, 0.0f, 0.0f, 0.0f));
    CHECK(ufo_target_clear_of_player(0.0f, -31.0f, 0.0f, 0.0f));
    CHECK(!ufo_target_clear_of_player(18.0f, 18.0f, 0.0f, 0.0f));     /* d² = 648 < 900 ⇒ 危险 */
    CHECK(!ufo_target_clear_of_player(0.0f, 30.0f, 0.0f, 0.0f));      /* 正好 30 ⇒ 不严格 > ⇒ 不安全 */
    CHECK(ufo_target_clear_of_player(0.0f, 30.1f, 0.0f, 0.0f));
    CHECK(ufo_target_clear_of_player(-40.0f, 0.0f, 0.0f, 0.0f));
    /* ★ 2026-09-21（P2 #4）：合成判据**只剩屏内那一条** —— 自机 30 px 收点圈那一条已撤
     *   （一手 state 7 每帧会落回 `th12:0x4260af` = 自机收点矩形判定 ⇒ 一手允许自机半途收走；
     *    照抄那条栅栏会让道具永久卡在圈边上）。`ufo_target_clear_of_player` 本身仍可用（下面照测）。 */
    CHECK(ufo_step_allowed(0.0f, 0.0f));                       /* 屏内 ⇒ 放行 */
    CHECK(ufo_step_allowed(10.0f, 10.0f));                     /* ★ 与自机同格/贴脸也放行（栅栏已撤）*/
    CHECK(!ufo_step_allowed(250.0f, 0.0f));                    /* 出屏仍挡 */
    CHECK(!ufo_step_allowed(0.0f, 500.0f));                    /* 出屏（下方）仍挡 */
    CHECK(ufo_target_clear_of_player(31.0f, 0.0f, 0.0f, 0.0f));/* ⚠ 导出保留：这条判据本身没变 */

    /* ========================= 填充率（一手公式 `0.032 − 关卡号×0.002`；严格 > 1.0） */
    CHECK(UFO_FILL_PER_ITEM_MILLIONTHS == 32000);   /* 一手 0.032 的百万分比 */
    CHECK(UFO_FILL_PER_STAGE_MILLIONTHS == 2000);   /* 一手 0.002 的百万分比 */
    CHECK(UFO_FILL_PER_TOKEN_MILLIONTHS == 100000); /* 一手 0.1 的百万分比 */
    CHECK(UFO_FILL_SPIT_AT == UFO_FILL_UNIT);
    CHECK(UFO_FILL_UNIT == 1000000);
    CHECK(UFO_FILL_SCALE_PERMILLE == 1000);            /* 🟡 占位：1000 = 不缩放 */
    CHECK(ufo_inc_millionths(1, 1000) == 30000);      /* 0.030 */
    CHECK(ufo_inc_millionths(2, 1000) == 28000);      /* 0.028 */
    CHECK(ufo_inc_millionths(6, 1000) == 20000);      /* 0.020 */
    CHECK(ufo_inc_millionths(16, 1000) == 0);         /* 关卡号大到吃光增量 */
    CHECK(ufo_inc_millionths(99, 1000) == 0);
    CHECK(ufo_inc_millionths(1, 0) == 0);             /* 缩放 0 = 关掉 */
    CHECK(ufo_inc_millionths(1, -5) == 0);
    CHECK(ufo_inc_millionths(1, 2000) == 60000);      /* ×2 缩放 */
    CHECK(ufo_inc_millionths(1, 500) == 15000);       /* ×0.5 */
    /* spec §4 那张关卡表：34/36/39/42/46/51/56 —— 本函数必须逐格复现（这就是「一手可复算」）*/
    CHECK(ufo_items_to_full(1, 1000) == 34);
    CHECK(ufo_items_to_full(2, 1000) == 36);
    CHECK(ufo_items_to_full(3, 1000) == 39);
    CHECK(ufo_items_to_full(4, 1000) == 42);
    CHECK(ufo_items_to_full(5, 1000) == 46);
    CHECK(ufo_items_to_full(6, 1000) == 51);
    CHECK(ufo_items_to_full(0, 1000) == 32);           /* stage 0：0.032 ⇒ floor(31.25)+1 = 32 */
    CHECK(ufo_items_to_full(15, 1000) == 501);         /* 0.002 ⇒ floor(500)+1 */
    CHECK(ufo_items_to_full(16, 1000) == -1);          /* 增量 0 ⇒ 不可达 */
    CHECK(ufo_items_to_full(1, 0) == -1);
    CHECK(ufo_items_to_full(1, -1) == -1);
    CHECK(ufo_items_to_full(1, 2000) == 17);           /* 0.06 ⇒ floor(16.66)+1 */

    /* 记一颗被吸收的道具：两个计数 + fill；返回「本帧吸满」*/
    ufo_reset(&u);
    CHECK(ufo_absorb_item(&u, 1, 1, 1000) == 0);       /* 场上没有 UFO ⇒ 不记 */
    CHECK(u.absorbed == 0);
    CHECK(ufo_spawn(&u, 1u, UFO_COLOR_RED, 999));
    CHECK(ufo_absorb_item(&u, 1, 1, 1000) == 0);       /* 这颗还没满 */
    CHECK(u.cnt_p == 1);
    CHECK(u.cnt_money == 0);
    CHECK(u.absorbed == 1);
    CHECK(u.fill_millionths == 30000);          /* 0.030 */
    CHECK(ufo_absorb_item(&u, 2, 1, 1000) == 0);       /* 金钱走另一个计数 */
    CHECK(u.cnt_money == 1);
    CHECK(u.cnt_p == 1);
    CHECK(u.absorbed == 2);
    CHECK(u.fill_millionths == 60000);          /* 0.060 */
    CHECK(ufo_absorb_item(&u, 3, 1, 1000) == 0);       /* type 不吸 */
    CHECK(u.absorbed == 2);
    CHECK(ufo_absorb_item(&u, 0x10, 1, 1000) == 0);
    CHECK(u.absorbed == 2);
    CHECK(u.fill_millionths == 60000);          /* 0.060 */
    /* 吸满：33 颗 + 1 = 34（stage 1）*/
    for (i = 0; i < 31; ++i) CHECK(ufo_absorb_item(&u, 1, 1, 1000) == 0);
    CHECK(u.absorbed == 33);
    CHECK(ufo_absorb_item(&u, 1, 1, 1000) == 1);       /* 第 34 颗：**这一颗把它吸满了** */
    CHECK(u.cnt_p == 33);                              /* 1（第 1 颗）+ 31 + 1 */
    CHECK(u.fill_millionths == UFO_FILL_UNIT);         /* 夹回 1.0（一手 `0x44ad74`）*/
    /* ★★ 满着期间再吸（一手 `0x44acd8 fld1; fcom fill; test ah,0x41; jnp 0x44ae98`）：
     *   **返回值 0**（不再开奖）、**fill 不再涨**、但**计数照加**（`0x44acc8` 的 add 在检查之前）。
     *   这是本轮订正的漏洞：旧版这里返回 1 ⇒ 卡侧每颗都 `ufo_spit_reward()` 一次，
     *   而红 UFO 的那件奖励是**命碎片** ⇒ 吸满之后可以无限刷资源。 */
    CHECK(ufo_absorb_item(&u, 1, 1, 1000) == 0);
    CHECK(ufo_absorb_item(&u, 1, 1, 1000) == 0);
    CHECK(u.cnt_p == 35);                              /* 计数照加（满着也数）*/
    CHECK(u.absorbed == 36);
    CHECK(u.fill_millionths == UFO_FILL_UNIT);         /* 仍是 1.0（满着不加）*/
    CHECK(feq(ufo_fill_ratio(&u), 1.0f, 1e-6f));       /* 唯一浮点出口，只给 HUD 用 */
    CHECK(u.cnt_money == 1);
    /* 满着时吸**另一种** type 也一样：计数 +1、不再开奖 */
    CHECK(ufo_absorb_item(&u, 2, 1, 1000) == 0);
    CHECK(u.cnt_money == 2);

    /* 彩虹 UFO 的两个计数**倒置**（一手：颜色 == −1 时 `cntA`/`cntB` 互换）*/
    ufo_reset(&u);
    CHECK(ufo_spawn(&u, 2u, UFO_COLOR_RAINBOW, 999));
    CHECK(ufo_absorb_item(&u, 1, 1, 1000) == 0);
    CHECK(u.cnt_money == 1);
    CHECK(u.cnt_p == 0);
    CHECK(ufo_absorb_item(&u, 2, 1, 1000) == 0);
    CHECK(u.cnt_p == 1);
    CHECK(u.cnt_money == 1);

    /* ============ 吸满吐奖励表（一手语义 + TH18 载体；★ 2026-09-21 P2 #5 改载体）
     * 一手（`th12:0x44aca0` 的 switch）：红 → 道具 4（命碎）、绿 → 道具 5（B 碎）。
     * TH18 侧承载「命碎 / B 碎」的还是**卡道具** `0x10` EXTEND2 / `0x11` BOMB2（= 残碎卡 / 符卡碎卡）
     * —— 4 / 6 是别作的遗留槽（道具分派里连 case 都没有）⇒ **不许再写回 4 / 6**。 */
    CHECK(UFO_REWARD_LIFE_FRAG == 0x10);               /* TH18 = EXTEND2 卡道具（残碎卡）*/
    CHECK(UFO_REWARD_BOMB_FRAG == 0x11);               /* TH18 = BOMB2 卡道具（符卡碎片卡）*/
    CHECK(UFO_REWARD_NONE == 0);
    CHECK(ufo_spit_reward(UFO_COLOR_RED) == UFO_REWARD_LIFE_FRAG);
    CHECK(ufo_spit_reward(UFO_COLOR_GREEN) == UFO_REWARD_BOMB_FRAG);
    CHECK(ufo_spit_reward(UFO_COLOR_BLUE) == UFO_REWARD_NONE);      /* 一手：switch 无 case 2 */
    CHECK(ufo_spit_reward(UFO_COLOR_GOLD) == UFO_REWARD_NONE);      /* ★ 金照蓝（用户裁决 ①）*/
    CHECK(ufo_spit_reward(UFO_COLOR_RAINBOW) == UFO_REWARD_NONE);   /* 一手吐令牌，TH18 无对应 type */
    CHECK(ufo_spit_reward(UFO_COLOR_NONE) == UFO_REWARD_NONE);
    CHECK(ufo_spit_reward(999) == UFO_REWARD_NONE);
    /* 击破的颜色加成（一手 `th12:0x44af00`：红 → 道具 4；绿 → 道具 5；蓝 / 彩虹无额外）
     * ⇒ TH18 载体同上（`0x10` 残碎卡 / `0x11` 符卡碎片卡）*/
    CHECK(ufo_break_color_bonus(UFO_COLOR_RED) == UFO_REWARD_LIFE_FRAG);
    CHECK(ufo_break_color_bonus(UFO_COLOR_GREEN) == UFO_REWARD_BOMB_FRAG);
    CHECK(ufo_break_color_bonus(UFO_COLOR_BLUE) == UFO_REWARD_NONE);
    CHECK(ufo_break_color_bonus(UFO_COLOR_GOLD) == UFO_REWARD_NONE);   /* ★ 金照蓝 */
    CHECK(ufo_break_color_bonus(UFO_COLOR_RAINBOW) == UFO_REWARD_NONE);
    CHECK(ufo_break_color_bonus(UFO_COLOR_NONE) == UFO_REWARD_NONE);
    CHECK(ufo_break_color_bonus(-7) == UFO_REWARD_NONE);

    /* ============ ★★ 掉落表：**每色恰好返还 1 枚令牌**（一手 `th12:0x44af00` 跳表 `0x44b590`）
     * 索引 = color+1；红→`0x44b23a push 0xd`(13)、蓝→`0x44b32a push 0xe`(14)、绿→`0x44b409 push 0xf`(15)、
     * 彩虹→`0x44b416` 按 `COLLECTED_UFOS[2]`（`0x4b0c54`）选 13/14/15。
     * 相位 0/1/2 = 道具 13/14/15 = 彩虹令牌的红/蓝/绿起始态。 */
    CHECK(UFO_TOK_DROP_NONE == -1);
    CHECK(ufo_break_token_state(UFO_COLOR_RED, UFO_COLOR_NONE) == 0);      /* 13 = 红起始 */
    CHECK(ufo_break_token_state(UFO_COLOR_BLUE, UFO_COLOR_NONE) == 1);     /* 14 = 蓝起始 */
    CHECK(ufo_break_token_state(UFO_COLOR_GREEN, UFO_COLOR_NONE) == 2);    /* 15 = 绿起始 */
    CHECK(ufo_break_token_state(UFO_COLOR_GOLD, UFO_COLOR_NONE) == 1);     /* ★ 金照蓝（用户裁决 ①）*/
    /* 彩虹 UFO：按「开碟时第 3 枚令牌的颜色」选相位（一手 `sub eax,1; je…`） */
    CHECK(ufo_break_token_state(UFO_COLOR_RAINBOW, UFO_COLOR_RED) == 0);
    CHECK(ufo_break_token_state(UFO_COLOR_RAINBOW, UFO_COLOR_BLUE) == 1);
    CHECK(ufo_break_token_state(UFO_COLOR_RAINBOW, UFO_COLOR_GREEN) == 2);
    CHECK(ufo_break_token_state(UFO_COLOR_RAINBOW, UFO_COLOR_NONE) == 0);  /* 没记到 ⇒ 退回红那一态 */
    CHECK(ufo_break_token_state(UFO_COLOR_NONE, UFO_COLOR_RED) == UFO_TOK_DROP_NONE);
    CHECK(ufo_break_token_state(99, UFO_COLOR_RED) == UFO_TOK_DROP_NONE);
    /* 吸满那条只有彩虹吐令牌（一手 `0x44ad99` 的 switch：红/绿走道具 4/7、蓝无 case）*/
    CHECK(ufo_spit_token_state(UFO_COLOR_RED, UFO_COLOR_RED) == UFO_TOK_DROP_NONE);
    CHECK(ufo_spit_token_state(UFO_COLOR_GREEN, UFO_COLOR_GREEN) == UFO_TOK_DROP_NONE);
    CHECK(ufo_spit_token_state(UFO_COLOR_BLUE, UFO_COLOR_BLUE) == UFO_TOK_DROP_NONE);
    CHECK(ufo_spit_token_state(UFO_COLOR_GOLD, UFO_COLOR_BLUE) == UFO_TOK_DROP_NONE);  /* ★ 金照蓝 */
    CHECK(ufo_spit_token_state(UFO_COLOR_RAINBOW, UFO_COLOR_GREEN) == 2);
    CHECK(ufo_spit_token_state(UFO_COLOR_NONE, UFO_COLOR_GREEN) == UFO_TOK_DROP_NONE);

    /* ---- `last_slot2` = 一手 `COLLECTED_UFOS[2]`（`0x4b0c54`）的快照（彩虹掉落表要读它）---- */
    ufo_group_reset(&g);
    CHECK(ufo_group_put(&g, UFO_COLOR_BLUE, NULL) == UFO_GROUP_PUT_HELD);
    CHECK(ufo_group_put(&g, UFO_COLOR_RED, NULL) == UFO_GROUP_PUT_HELD);
    CHECK(ufo_group_put(&g, UFO_COLOR_GREEN, NULL) == UFO_GROUP_PUT_READY);
    CHECK(ufo_group_take_ready(&g) == UFO_COLOR_RAINBOW);
    CHECK(g.last_slot2 == UFO_COLOR_GREEN);            /* ★ 第 3 枚的颜色留下来 */
    CHECK(g.last_group == UFO_COLOR_RAINBOW);
    CHECK(g.filled == 3);                              /* ★ P1：取结果不清槽 */
    CHECK(ufo_break_token_state(g.last_group, g.last_slot2) == 2);          /* 彩虹 UFO ⇒ 相位 2 */
    ufo_group_clear(&g);
    CHECK(g.filled == 0);
    CHECK(g.last_slot2 == UFO_COLOR_NONE);             /* clear 也清快照 */
    ufo_group_reset(&g);
    CHECK(g.last_slot2 == UFO_COLOR_NONE);             /* reset 清掉（不会串到下一组）*/

    /* ================= ★★ 令牌运动（一手 `th12:0x425c20` state 6；用户口述的「飞行」） */
    {
        ufo_tok_motion_t m;
        /* 一手：`0x4a4270` = 1.5f、角度 `0x4a4274` = π/4 / `0x4a4278` = 3π/4 按奇偶交替，
         * 且计数在自增之后判 ⇒ **序号 0（本关第 1 枚）= 左下 45°**。 */
        CHECK(feq(UFO_TOK_FLY_SPEED, 1.5f, 1e-6f));
        CHECK(feq(UFO_TOK_ANGLE_DOWN_RIGHT, 0.785398185f, 1e-6f));   /* π/4 */
        CHECK(feq(UFO_TOK_ANGLE_DOWN_LEFT, 2.3561945f, 1e-6f));      /* 3π/4 */
        CHECK(feq(UFO_TOK_VEL_COMP, 1.060660124f, 1e-5f));           /* 1.5 × √2/2 */
        /* ★★ 2026-09-12 换池：位置/速度**都在运动结构体自己身上**（以前位置在敌机字段里、
         *  `init/step` 只吃 `float *x, *y` 指针）⇒ 签名变成 `uo_tok_motion_init_at(&m, serial, x, y)`
         *  与 `ufo_tok_motion_step(&m)`，**行为逐条不变**（下面每条断言照旧）。 */
        ufo_tok_motion_init_at(&m, 0u, 0.0f, 0.0f);                  /* 第 1 枚 */
        CHECK(m.vx < 0.0f && feq(m.vy, UFO_TOK_VEL_COMP, 1e-6f));    /* 左下 45° */
        CHECK(m.age == 0);
        CHECK(m.x == 0.0f && m.y == 0.0f);                           /* 出生点 = 传入坐标 */
        ufo_tok_motion_init_at(&m, 1u, 0.0f, 0.0f);                  /* 第 2 枚 */
        CHECK(m.vx > 0.0f && feq(m.vy, UFO_TOK_VEL_COMP, 1e-6f));    /* 右下 45° */
        ufo_tok_motion_init_at(&m, 2u, 0.0f, 0.0f);
        CHECK(m.vx < 0.0f);                                          /* 再交替回来 */
        /* 出生点被原样记下（一手：落点 = 那只敌人的死亡坐标；调用方已经夹取过）*/
        ufo_tok_motion_init_at(&m, 3u, -120.0f, 96.5f);
        CHECK(m.x == -120.0f && m.y == 96.5f);
        CHECK(ufo_tok_motion_step(&m) == 0);
        CHECK(feq(m.x, -120.0f + m.vx, 1e-5f) && feq(m.y, 96.5f + m.vy, 1e-5f));

        /* 位移：每帧 vx/vy（一手 `0x4263a1`–`0x4263cb`）*/
        ufo_tok_motion_init_at(&m, 1u, 0.0f, 0.0f);                  /* 右下 */
        CHECK(ufo_tok_motion_step(&m) == 0);
        CHECK(feq(m.x, UFO_TOK_VEL_COMP, 1e-6f) && feq(m.y, UFO_TOK_VEL_COMP, 1e-6f));
        CHECK(m.age == 1);

        /* 反弹：右边界 176（一手 `0x4a4198`）⇒ vx 取反；下边界 370（`0x4a4408`）⇒ vy 取反 */
        ufo_tok_motion_init_at(&m, 1u, 175.5f, 0.0f);
        CHECK(ufo_tok_motion_step(&m) == 0);
        CHECK(m.vx < 0.0f);                                          /* 撞右边界 ⇒ 反射（`0x4a4168` = −1.0）*/
        CHECK(feq(m.x, 175.5f + UFO_TOK_VEL_COMP, 1e-5f));           /* 位置不被夹，只有速度翻 */
        ufo_tok_motion_init_at(&m, 0u, -175.5f, 0.0f);
        CHECK(ufo_tok_motion_step(&m) == 0);
        CHECK(m.vx > 0.0f);                                          /* 撞左边界（−176）*/
        ufo_tok_motion_init_at(&m, 1u, 0.0f, 369.5f);
        CHECK(ufo_tok_motion_step(&m) == 0);
        CHECK(m.vy < 0.0f);                                          /* 撞下边界 370 */
        ufo_tok_motion_init_at(&m, 0u, 0.0f, 160.5f);                /* 左下 ⇒ vy > 0 */
        m.vy = -UFO_TOK_VEL_COMP;                                    /* 手工构造「向上撞上边界」*/
        CHECK(ufo_tok_motion_step(&m) == 0);
        CHECK(m.vy > 0.0f);                                          /* 撞上边界 160（`0x4a3f4c`）*/

        /* 方向守卫：朝外走才翻（一手四条反弹都带 `vel.x < 0` / `vel.x > 0` 的守卫）*/
        ufo_tok_motion_init_at(&m, 1u, 176.0f, 0.0f);                /* 右下：vx > 0、vy > 0 */
        m.vx = -m.vx;                                                /* 已经朝左，却贴在右边界 */
        CHECK(ufo_tok_motion_step(&m) == 0);
        CHECK(m.vx < 0.0f);                                          /* 不被翻回来 */

        /* 1200 帧之后**不再反弹**（一手 `0x4263d1 cmp [edi+0x98c],0x4b0; jge`），只判出界 */
        ufo_tok_motion_init_at(&m, 1u, 176.5f, 380.0f);
        m.age = UFO_TOK_NO_BOUNCE_FRAMES;
        CHECK(ufo_tok_motion_step(&m) == 0);                         /* 外框内 ⇒ 还在 */
        CHECK(m.vx > 0.0f && m.vy > 0.0f);                           /* 速度没被翻 */
        CHECK(UFO_TOK_NO_BOUNCE_FRAMES == 1200);                     /* ✅ 一手 `0x4b0` = 20 s = 8 次换色 */

        /* 出界即撤：外框 208 / −208（`0x4a43a0`/`0x4a4400`）、y −16（`0x4a43f8`）/ 464（`0x4a3e90`）*/
        ufo_tok_motion_init_at(&m, 1u, 207.5f, 0.0f);
        CHECK(ufo_tok_motion_step(&m) == 1);                         /* x 越过 208 ⇒ 撤 */
        ufo_tok_motion_init_at(&m, 0u, -207.5f, 0.0f);
        CHECK(ufo_tok_motion_step(&m) == 1);
        ufo_tok_motion_init_at(&m, 1u, 0.0f, 463.5f);
        CHECK(ufo_tok_motion_step(&m) == 1);                         /* y 越过 464 ⇒ 撤 */
        ufo_tok_motion_init_at(&m, 1u, 0.0f, 0.0f);
        CHECK(ufo_tok_motion_step(&m) == 0);                         /* 场内 ⇒ 活 */
        CHECK(ufo_tok_motion_step(NULL) == 1);                       /* 空指针 ⇒ 报「撤」（防御）*/
    }


    /* 赏金 type 选择（一手规则本身保留；TH18 侧三个 type 都不可用 ⇒ 不接线）*/
    CHECK(UFO_BOUNTY_TYPE_BOTH == 18);
    CHECK(UFO_BOUNTY_TYPE_MONEY == 17);
    CHECK(UFO_BOUNTY_TYPE_P == 16);
    CHECK(UFO_BOUNTY_TYPES_USABLE == 0);               /* ★ TH18 无可用 type（spec §2.2 末两行）*/
    ufo_reset(&u);
    CHECK(ufo_bounty_item_type(&u) == 0);              /* 真值表：两个都 0 ⇒ 不生成 */
    ufo_reset(&u);
    u.cnt_p = 3;
    CHECK(ufo_bounty_item_type(&u) == UFO_BOUNTY_TYPE_P);
    ufo_reset(&u);
    u.cnt_money = 2;
    CHECK(ufo_bounty_item_type(&u) == UFO_BOUNTY_TYPE_MONEY);
    ufo_reset(&u);
    u.cnt_p = 1;
    u.cnt_money = 1;
    CHECK(ufo_bounty_item_type(&u) == UFO_BOUNTY_TYPE_BOTH);
    CHECK(ufo_bounty_item_type(NULL) == 0);
    /* 份额（★ spec §9 #5：仓里没有一手 ⇒ 占位权重 0 ⇒ 加 0 分）*/
    CHECK(UFO_BOUNTY_SCORE_PER_P == 0);
    CHECK(UFO_BOUNTY_SCORE_PER_MONEY == 0);
    ufo_reset(&u);
    u.cnt_p = 34;
    u.cnt_money = 99;
    CHECK(ufo_break_bounty_score(&u) == 0);            /* 占位：不编公式 */
    CHECK(ufo_break_bounty_score(NULL) == 0);

    /* ================= ★★ 掉落物实体（压缩包 / 整颗 Bomb，2026-09-18 用户口径）=================
     * 三条都要能离线对账：① 倍率（一手 `0x44b149`/`0x44b24d`/`0x44b33f`/`0x44b49c`）；
     * ② 结算值（一手 `0x426c9a`/`0x426cd2`/`0x426d27` 三条式子 + 下限 10）；
     * ③ 运动（压缩包 = 一手 state 8 的速度门；整颗 Bomb = 普通道具的重力）。 */
    {
        /* ① 倍率（千分点；1000 = ×1.0） */
        CHECK(ufo_drop_mult_permille(UFO_COLOR_RED, 1, 0) == 2000);          /* 满火力 2.0 */
        CHECK(ufo_drop_mult_permille(UFO_COLOR_RED, 0, UFO_FILL_UNIT) == 1000); /* 否则 1.0（fill 无关）*/
        CHECK(ufo_drop_mult_permille(UFO_COLOR_GREEN, 0, 0) == 2000);        /* 绿恒 2.0 */
        CHECK(ufo_drop_mult_permille(UFO_COLOR_GREEN, 1, UFO_FILL_UNIT) == 2000);
        CHECK(ufo_drop_mult_permille(UFO_COLOR_BLUE, 0, 0) == 2000);         /* 蓝：2+8·fill */
        CHECK(ufo_drop_mult_permille(UFO_COLOR_BLUE, 0, UFO_FILL_UNIT / 2) == 6000);  /* fill=0.5 ⇒ 6.0 */
        CHECK(ufo_drop_mult_permille(UFO_COLOR_BLUE, 0, UFO_FILL_UNIT) == 8000);      /* fill=1.0 ⇒ 8.0 */
        CHECK(ufo_drop_mult_permille(UFO_COLOR_BLUE, 0, UFO_FILL_UNIT / 4) == 4000);  /* fill=0.25 ⇒ 4.0 */
        CHECK(ufo_drop_mult_permille(UFO_COLOR_RAINBOW, 0, 0) == 2000);      /* 彩虹：2+2·fill */
        CHECK(ufo_drop_mult_permille(UFO_COLOR_RAINBOW, 0, UFO_FILL_UNIT / 2) == 3000);
        CHECK(ufo_drop_mult_permille(UFO_COLOR_RAINBOW, 0, UFO_FILL_UNIT) == 4000);
        CHECK(ufo_drop_mult_permille(UFO_COLOR_GOLD, 0, UFO_FILL_UNIT) == 8000);      /* ★ 金 ≡ 蓝 */
        CHECK(ufo_drop_mult_permille(UFO_COLOR_BLUE, 0, -5) == 2000);        /* 越界夹取 */
        CHECK(ufo_drop_mult_permille(UFO_COLOR_BLUE, 0, UFO_FILL_UNIT + 99) == 8000);

        /* ② 结算值：红 = trunc(cntA·PV·mult) + cntB·PV（**不**取整到 10）；蓝/绿/彩虹 = 取整到 10 */
        /*    取 PV=1000、mult=2000（×2）、cntA=3、cntB=5：
         *    红 ⇒ 3·1000·2 + 5·1000 = 6000 + 5000 = 11000（不取整）*/
        CHECK(ufo_drop_settle_value(UFO_COLOR_RED, 3, 5, 2000, 1000) == 11000);
        /*    蓝 ⇒ trunc(1000·5·2) = 10000 ⇒ /10×10 = 10000 */
        CHECK(ufo_drop_settle_value(UFO_COLOR_BLUE, 3, 5, 2000, 1000) == 10000);
        /*    取整真的在动：PV=1007、cntB=1、mult=1000 ⇒ 1007 ⇒ 1000（向下取到 10 的倍数）*/
        CHECK(ufo_drop_settle_value(UFO_COLOR_GREEN, 0, 1, 1000, 1007) == 1000);
        CHECK(ufo_drop_settle_value(UFO_COLOR_RAINBOW, 0, 1, 1000, 1007) == 1000);
        /*    红那支**不取整**：同样的数 ⇒ 1007 */
        CHECK(ufo_drop_settle_value(UFO_COLOR_RED, 0, 1, 1000, 1007) == 1007);
        /*    下限 10（一手 `0x426d81 mov esi,0xa`） */
        CHECK(ufo_drop_settle_value(UFO_COLOR_RED, 0, 0, 1000, 1000) == 10);
        CHECK(ufo_drop_settle_value(UFO_COLOR_BLUE, 0, 0, 1000, 1000) == 10);
        /*    红那支的 cntA 项**乘倍率**（蓝那支不看 cntA —— 这是三条式子的关键差异）*/
        CHECK(ufo_drop_settle_value(UFO_COLOR_RED, 1, 0, 3000, 100) == 300);
        CHECK(ufo_drop_settle_value(UFO_COLOR_BLUE, 99, 1, 3000, 100) == 300);

        /* ③ PV：一手 = round10(PIV/100)；TH18 对位 = 身价/100 再取到 10 的倍数 */
        CHECK(ufo_drop_pv_from_base(0) == 10);                 /* 引擎没初始化 ⇒ 下限 */
        CHECK(ufo_drop_pv_from_base(1000000) == 10000);        /* 身价 1,000,000 ⇒ 10000（再取整不变）*/
        CHECK(ufo_drop_pv_from_base(100700) == 1000);          /* 1007 ⇒ 1000 */
        CHECK(ufo_drop_pv_from_base(999) == 0);                /* 9 ⇒ 取整到 0（下限 10 在结算那一支）*/

        /* ④ 运动：压缩包 —— 一手 state 8（`th12:0x426189`–`0x426231`）：
         *   **先位移、后加速**；年纪 < 60 不加速（`0x4261f6`），之后每帧 +0.2、上限 12.0（`0x4261ff`）；
         *   spawn 那一刻 speed = 0（`0x4276f9 fstp [esi+0x984]`）⇒ 前 60 帧原地不动。 */
        CHECK(UFO_DROP_HOME_DELAY_FRAMES == 60);           /* ✅ 一手 `cmp [edi+0x98c],0x3c` */
        CHECK(UFO_DROP_HOME_ACCEL_MILLI == 200);           /* ✅ 一手 `fadd [0x4a3fb8]` = double 0.2 */
        CHECK(UFO_DROP_HOME_SPEED_MAX_MILLI == 12000);     /* ✅ 一手 `fcomp [0x4a404c]` = 12.0 */
        CHECK(UFO_DROP_HOME_FULL_SPEED_START == 0);        /* ★ 默认 = 一手（「满速启动」是 ❓ 自定开关）*/
        {
            ufo_drop_t d;
            int i2, moved_early = 0;
            memset(&d, 0, sizeof d);
            d.kind = UFO_DROP_KIND_BAG;
            d.alive = 1;
            d.x = 0.0f; d.y = 0.0f;
            for (i2 = 0; i2 < UFO_DROP_HOME_DELAY_FRAMES; ++i2) {
                (void)ufo_drop_step(&d, 0.0f, 300.0f);         /* 自机在正下方 300 */
                if (d.x != 0.0f || d.y != 0.0f) moved_early = 1;
            }
            CHECK(!moved_early);                               /* ★ 60 帧内一动不动（原地）*/
            CHECK(d.speed_milli == 0);
            CHECK(d.age == (uint32_t)UFO_DROP_HOME_DELAY_FRAMES);
            (void)ufo_drop_step(&d, 0.0f, 300.0f);             /* 第 61 帧：先位移（speed 还是 0）再加速 */
            CHECK(d.speed_milli == UFO_DROP_HOME_ACCEL_MILLI); /* +0.2 */
            CHECK(d.y == 0.0f);                                /* ★ 这一帧位移用的仍是**旧**速度 ⇒ 还没动 */
            (void)ufo_drop_step(&d, 0.0f, 300.0f);             /* 第 62 帧起真的开始走 */
            CHECK(d.y > 0.0f);                                 /* 朝自机（+y）走了 */
            for (i2 = 0; i2 < 200; ++i2) (void)ufo_drop_step(&d, 0.0f, 300.0f);
            CHECK(d.speed_milli == UFO_DROP_HOME_SPEED_MAX_MILLI);     /* 夹到 12.0 */
            /* 近距离**不过冲**：与自机 3 px 时下一步正好压在自机上 */
            d.x = 0.0f; d.y = 297.0f;
            (void)ufo_drop_step(&d, 0.0f, 300.0f);
            CHECK(d.x == 0.0f && d.y == 300.0f);
        }
        /* ⑤ 运动：整颗 Bomb —— 出生上抛（调方给 vy = −2.2），每帧 +0.03 重力 ⇒ 先升后落 */
        {
            ufo_drop_t d;
            int i2, rose = 0;
            memset(&d, 0, sizeof d);
            d.kind = UFO_DROP_KIND_BOMB;
            d.alive = 1;
            d.vy_milli = -UFO_DROP_TOSS_SPEED_MILLI;
            for (i2 = 0; i2 < 200; ++i2) {
                float y0 = d.y;
                (void)ufo_drop_step(&d, 0.0f, 0.0f);
                if (d.y < y0) rose = 1;                        /* 有一帧在上升 */
            }
            CHECK(rose);                                       /* ★ 上抛段真的存在（2.2/帧 起）*/
            CHECK(d.vy_milli > 0);                             /* 之后重力把它压成下落（+0.03/帧）*/
            CHECK(d.y > 0.0f);                                 /* 200 帧后已经落到起点下方 */
            CHECK(d.vy_milli == -UFO_DROP_TOSS_SPEED_MILLI + (int32_t)d.age * UFO_DROP_GRAVITY_MILLI);
        }
        /* ⑥ 收集判定：到自机距离² ≤ r²（r = 本作自己的收点半径，与令牌共用）*/
        {
            ufo_drop_t d;
            memset(&d, 0, sizeof d);
            d.alive = 1; d.x = 10.0f; d.y = 0.0f;
            CHECK(ufo_drop_within_pick(&d, 0.0f, 0.0f, 30.0f));
            CHECK(!ufo_drop_within_pick(&d, 0.0f, 0.0f, 5.0f));
            CHECK(!ufo_drop_within_pick(&d, 0.0f, 0.0f, 0.0f));    /* r 非正 ⇒ 不收 */
            CHECK(!ufo_drop_within_pick(NULL, 0.0f, 0.0f, 30.0f));
        }
    }

    /* ================= ★★ P1 的满槽补偿（一手 `th12:0x4270b0` 开头）=================
     * 判据 = 已填槽数 >= 3；加值 = `+100000`（×100 定点 = 显示 +1000）；弹字 = 1000 / `0xff80ff80`；
     * 令牌被消耗。TH18 侧没有 PIV 状态量 ⇒ 载体是映射（见卡侧 `ufo_slot_full_bonus()` 的注）。 */
    CHECK(UFO_SLOT_FULL_PIV_ADD == 100000);
    CHECK(UFO_SLOT_FULL_POPUP_VALUE == 1000);
    CHECK(UFO_SLOT_FULL_POPUP_COLOR == 0xff80ff80u);
    CHECK(UFO_SLOT_FULL_PIV_ADD / 100 == UFO_SLOT_FULL_POPUP_VALUE);   /* 一手两个数是同一个量的两种单位 */

    /* ============================================= §4 HP 硬约束（spawn life < 1000） */
    CHECK(UFO_LIFE_LIMIT == 1000);
    CHECK(UFO_LIFE_MAX_ALLOWED == 999);
    /* ★★ P4（2026-09-18）+ P2 #6（2026-09-21「感觉飞碟太脆了」）：HP = 一手表
     *   `{1600,1900,2100,2300,2500}`（`th12:0x4b33f0`，按火力档索引）**原值**（不再缩放到 999）；
     *   写进 `spawn+0x14` 的才是钳制值（≤999，避开位 30 黑名单），真血量在 spawn 后回写敌机。 */
    CHECK(UFO_HP_TABLE_N == 5);
    CHECK(UFO_HP_MAX_ONEHAND == 2500);
    CHECK(UFO_HP_ONEHAND_SCALE_PERMILLE == 1000);      /* 🟡 旋钮默认 = 一手原值（不缩放）*/
    CHECK(k_ufo_hp_by_power[0] == 1600 && k_ufo_hp_by_power[1] == 1900 && k_ufo_hp_by_power[2] == 2100);
    CHECK(k_ufo_hp_by_power[3] == 2300 && k_ufo_hp_by_power[4] == 2500);   /* 一手五档逐值 */
    /* 档位映射：100 火力 = 一档（`CE_POWER_PER_LEVEL` 的原生值） */
    CHECK(ufo_hp_tier(0, 100) == 0);
    CHECK(ufo_hp_tier(99, 100) == 0);
    CHECK(ufo_hp_tier(100, 100) == 1);
    CHECK(ufo_hp_tier(400, 100) == 4);                 /* 满火力 */
    CHECK(ufo_hp_tier(999, 100) == 4);                 /* 超出 ⇒ 夹到最高档（表只有五格）*/
    CHECK(ufo_hp_tier(-50, 100) == 0);                 /* 负值 ⇒ 最低档 */
    CHECK(ufo_hp_tier(300, 0) == 0);                   /* per_level 没初始化 ⇒ 最低档（不除零）*/
    CHECK(ufo_hp_tier(300, -1) == 0);
    /* 逐档真血量 = **一手原值逐格相同**（这就是「复刻」的判据：不再有缩放比）*/
    {
        int t;
        int32_t prev = 0;
        for (t = 0; t < UFO_HP_TABLE_N; ++t) {
            int32_t hp = ufo_hp_for_power(t * 100, 100);
            CHECK(hp > prev);                          /* ★ 档间单调（「按火力」这句话的判据）*/
            CHECK(hp == k_ufo_hp_by_power[t]);         /* ★★ 原值（P2 #6 的核心断言）*/
            CHECK(hp >= 1);
            prev = hp;
        }
        CHECK(ufo_hp_for_power(400, 100) == 2500);     /* 满档 = 一手最高档（> 999 也合法）*/
        CHECK(ufo_hp_for_power(0, 100) == 1600);       /* 最低档 = 一手最低档 */
        CHECK(ufo_hp_onehand_for_tier(0) == 1600 && ufo_hp_onehand_for_tier(4) == 2500);
        CHECK(ufo_hp_onehand_for_tier(-1) == 1600 && ufo_hp_onehand_for_tier(9) == 2500);
    }
    CHECK(ufo_clamp_spawn_life(1) == 1);
    CHECK(ufo_clamp_spawn_life(500) == 500);
    CHECK(ufo_clamp_spawn_life(999) == 999);
    CHECK(ufo_clamp_spawn_life(1000) == 999);          /* 硬界：>= 1000 ⇒ 钳（一手 `0x42d8f6`）*/
    CHECK(ufo_clamp_spawn_life(1600) == 999);          /* ★ P2 #6：一手最低档会被钳（**只是 spawn 值**）*/
    CHECK(ufo_clamp_spawn_life(2500) == 999);          /* ★ 一手最高档同理 ⇒ 真血量另走回写 */
    CHECK(ufo_clamp_spawn_life(0) == 1);               /* 出生即死 ⇒ 抬到 1 */
    CHECK(ufo_clamp_spawn_life(-42) == 1);
    CHECK(ufo_clamp_spawn_life(UFO_CLAMP_OUTPUT_MAX) == UFO_CLAMP_OUTPUT_MAX);  /* 上界本身合法 */
    /* ★ P2 #6 的两步落笔：真血量恒 >= spawn 值（回写只会加厚，不会变薄）*/
    {
        int t;
        for (t = 0; t < UFO_HP_TABLE_N; ++t) {
            int32_t real = ufo_hp_for_power(t * 100, 100);
            int32_t sp = ufo_clamp_spawn_life(real);
            CHECK(real >= sp);
            CHECK(real > UFO_LIFE_MAX_ALLOWED);        /* 一手五档全部 > 999 ⇒ 必须靠回写 */
        }
    }

    /* ---- 血条千分比（★ 用户 2026-09-12 裁决 ④ 的 HUD 第一版用它画本体 HP 进度）----
     * 纯整数（本层零浮点）⇒ 期望值可以直接手算，全部是精确值。 */
    CHECK(ufo_hp_permille(100, 100) == 1000);          /* 满血 ⇒ 1000‰ = 进度条画满 */
    CHECK(ufo_hp_permille(50, 100) == 500);
    CHECK(ufo_hp_permille(1, 100) == 10);
    CHECK(ufo_hp_permille(33, 100) == 330);            /* 33×1000/100 = 330 */
    CHECK(ufo_hp_permille(99, 100) == 990);
    CHECK(ufo_hp_permille(0, 100) == 0);               /* 死了（或还没挂上）⇒ 不画 */
    CHECK(ufo_hp_permille(-5, 100) == 0);              /* 负值（引擎可能写）⇒ 0 */
    CHECK(ufo_hp_permille(200, 100) == 1000);          /* 超上限 ⇒ 夹到满 */
    CHECK(ufo_hp_permille(50, 0) == 0);                /* 上限 0 ⇒ 不给除零 */
    CHECK(ufo_hp_permille(50, -1) == 0);
    CHECK(ufo_hp_permille(1, 3) == 333);               /* 截断（不是四舍五入）：1×1000/3 = 333 */

    /* 令牌池的容量常量（★ 2026-09-12 换池：令牌是**自建 VM**，不再有 HP —— `UFO_TOKEN_LIFE`
     * 连同「令牌被子弹打掉也算收」那条路一起删了，见 `AUDIT.md` §BO-11 的 P3）。 */
    CHECK(UFO_TOKEN_MAX >= 1);
    CHECK(UFO_TOKEN_TTL_FRAMES > 0);
    CHECK(UFO_TOKEN_POOL_SLACK >= 0);
    CHECK(UFO_TOKEN_CAP == UFO_TOKEN_MAX + UFO_TOKEN_POOL_SLACK);
    /* 池容量判据（`ufo.c` 的分配器**唯一**的上限入口）—— 逐格真值表 */
    CHECK(ufo_token_pool_room(0) == UFO_TOKEN_CAP);
    CHECK(ufo_token_pool_room(UFO_TOKEN_MAX) == UFO_TOKEN_POOL_SLACK);
    CHECK(ufo_token_pool_room(UFO_TOKEN_CAP - 1) == 1);
    CHECK(ufo_token_pool_room(UFO_TOKEN_CAP) == 0);            /* 满 ⇒ 一枚都放不下 */
    CHECK(ufo_token_pool_room(UFO_TOKEN_CAP + 100) == 0);      /* 越界（坏状态）也不给负数 */
    CHECK(ufo_token_pool_room(-1) == 0);                       /* 负数（坏状态）⇒ 0，绝不放行 */

    /* ============================== 死亡事件 kind 过滤（spec §7 AUDIT：必须排除 KILL_ALL） */
    CHECK(UFO_EDEATH_KILLED == 0u);                    /* 值对账（DEATH §2.1；ufo.c 另有 _Static_assert）*/
    CHECK(UFO_EDEATH_BOMB_KILL == 1u);
    CHECK(UFO_EDEATH_ECL_DIE == 2u);
    CHECK(UFO_EDEATH_OTHER == 3u);
    CHECK(UFO_EDEATH_KILL_ALL == 4u);
    /* ★ 2026-09-12 换池：`ufo_token_kill_counts()` **已删**（令牌不是敌人 ⇒ 没有死亡事件）。
     * 现在**只有** `ufo_body_kill_counts`，而且它的用途变成了**两处**：
     *   ① 本体自己的击破（`ev->id == ufo->id`）；② **令牌的来源门**（小怪击破 ⇒ 登记一枚令牌）。 */
    CHECK(ufo_body_kill_counts(UFO_EDEATH_KILLED));
    CHECK(ufo_body_kill_counts(UFO_EDEATH_BOMB_KILL));
    CHECK(!ufo_body_kill_counts(UFO_EDEATH_KILL_ALL));   /* ★ 过场清场不算 */
    CHECK(!ufo_body_kill_counts(UFO_EDEATH_ECL_DIE));
    CHECK(!ufo_body_kill_counts(UFO_EDEATH_OTHER));
    CHECK(!ufo_body_kill_counts(99u));

    /* ================================ 端到端小场景：三次令牌 → 合成 → 召唤 → 吸 → 收尾 */
    ufo_group_reset(&g);
    ufo_reset(&u);
    CHECK(ufo_slot_set(&g, UFO_COLOR_RED));
    CHECK(ufo_slot_set(&g, UFO_COLOR_RED));
    CHECK(ufo_slot_set(&g, UFO_COLOR_RAINBOW));        /* 彩虹补位 */
    CHECK(ufo_group_has_match(&g));
    CHECK(ufo_can_spawn(&u));
    c = ufo_group_take_ready(&g);
    CHECK(c == UFO_COLOR_RED);
    CHECK(ufo_spawn(&u, 555u, c, 999));
    CHECK(ufo_present(&u));
    CHECK(g.filled == 3);                              /* ★ P1：召唤后槽仍满（UFO 结束时才清）*/
    ufo_group_clear(&g);
    CHECK(g.filled == 0);
    u.frames = 100;                                    /* 过了 60 帧暂停 */
    CHECK(ufo_can_absorb(&u));
    CHECK(ufo_absorb_item(&u, 2, 1, 1000) == 0);
    CHECK(ufo_absorb_item(&u, 2, 1, 1000) == 0);
    CHECK(u.cnt_money == 2);
    CHECK(ufo_break_color_bonus(u.color) == UFO_REWARD_LIFE_FRAG);
    CHECK(ufo_bounty_item_type(&u) == UFO_BOUNTY_TYPE_MONEY);
    ufo_finish(&u);
    CHECK(!ufo_present(&u));
    CHECK(ufo_can_spawn(&u));

    /* ================= 穷举不变量（把「手算期望」独立写一遍，跟实现对照）=================
     * ① classify 的所有 4³ = 64 种槽组合，逐格与「两条规则的字面语义」对照；
     * ② put 的所有 4⁴ 种喂入序列之后，再补喂 8 个令牌必须在**有限步**内出现 READY
     *    （= 不会永久卡住），且 `filled` 永远在 0..3。 */
    {
        static const int32_t kc[4] = { 1, 2, 3, -1 };   /* 红 蓝 绿 彩虹 */
        int ai, bi, ci, di, mism = 0, all_ok = 1;
        for (ai = 0; ai < 4; ++ai) for (bi = 0; bi < 4; ++bi) for (ci = 0; ci < 4; ++ci) {
            int32_t got, want, fixed = 0;
            int diff, same = 1, i2;
            ufo_group_reset(&g);
            g.slot[0] = kc[ai]; g.slot[1] = kc[bi]; g.slot[2] = kc[ci]; g.filled = 3;
            got = ufo_group_classify(&g);
            diff = (g.slot[0] != g.slot[1] && g.slot[1] != g.slot[2] && g.slot[0] != g.slot[2]);
            for (i2 = 0; i2 < 3; ++i2)
                if (g.slot[i2] != UFO_COLOR_RAINBOW && fixed == UFO_COLOR_NONE) fixed = g.slot[i2];
            for (i2 = 0; i2 < 3; ++i2)
                if (g.slot[i2] != UFO_COLOR_RAINBOW && g.slot[i2] != fixed) same = 0;
            /* 手算期望：三色互不相同 ⇒ 彩虹；三同色（彩虹补位）⇒ 该色（全彩虹 ⇒ 彩虹）；否则不召唤 */
            if (diff) want = UFO_COLOR_RAINBOW;
            else if (same) want = (fixed == UFO_COLOR_NONE) ? UFO_COLOR_RAINBOW : fixed;
            else want = UFO_COLOR_NONE;
            if (got != want) ++mism;
        }
        CHECK(mism == 0);                              /* 64 种组合逐格对上 */

        for (ai = 0; ai < 4; ++ai) for (bi = 0; bi < 4; ++bi)
        for (ci = 0; ci < 4; ++ci) for (di = 0; di < 4; ++di) {
            static const int32_t spare[8] = { 1, 1, 1, 2, 2, 2, 1, 1 };
            int32_t col[4];
            int i2, reached = 0;
            ufo_group_reset(&g);
            col[0] = kc[ai]; col[1] = kc[bi]; col[2] = kc[ci]; col[3] = kc[di];
            for (i2 = 0; i2 < 4; ++i2) {
                int32_t dropped = -1;
                int32_t put = ufo_group_put(&g, col[i2], &dropped);
                /* ★★ 新规则（用户裁决 ②）的核心不变式：**不存在「满 3 格却不成组」的稳态** ——
                 * 第三格填上时不成组就当场丢最旧 ⇒ 槽里要么是 0..2 格，要么是「满且成组」。 */
                if (g.filled == UFO_SLOT_COUNT && ufo_group_classify(&g) == UFO_COLOR_NONE) all_ok = 0;
                if (dropped < 0) all_ok = 0;               /* `out_dropped` 是**个数**，不会是负 */
                if (dropped > 2) all_ok = 0;               /* 一次 put 最多丢 2 枚（常规 1 + 防御支 1）*/
                if (ufo_slots_filled(&g) != g.filled) all_ok = 0;
                if (put == UFO_GROUP_PUT_READY) { ufo_group_take_ready(&g); ufo_group_clear(&g); }
            }
            for (i2 = 0; i2 < 8 && !reached; ++i2)
                if (ufo_group_put(&g, spare[i2], NULL) == UFO_GROUP_PUT_READY) {
                    reached = 1;
                    ufo_group_take_ready(&g);
                    ufo_group_clear(&g);               /* P1：取结果不清槽 ⇒ 这里模拟 UFO 结束 */
                }
            if (!reached) all_ok = 0;                  /* ★ 永久卡住 */
            if (g.filled < 0 || g.filled > UFO_SLOT_COUNT) all_ok = 0;
            if (ufo_slots_filled(&g) != g.filled) all_ok = 0;
        }
        CHECK(all_ok);                                 /* 256 条喂入序列：不卡死、filled 自洽、
                                                        * 「满格却不成组」的稳态不存在 */
    }

    printf("ufo_core: %d checks, %d failed\n", s_pass + s_fail, s_fail);
    return s_fail ? 1 : 0;
}
