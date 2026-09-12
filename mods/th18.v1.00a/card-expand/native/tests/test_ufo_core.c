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
 *   ③ 新增：`ufo_break_token_state` / `ufo_spit_token_state`（掉落表：每色恰好 1 枚令牌）、
 *      `ufo_tok_motion_init/step`（令牌的直线飞行 1.5 + 四边反弹 + 1200 帧后不反弹 + 出界即撤）。
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
    CHECK(ufo_spawn(&u, 4242u, UFO_COLOR_GOLD));
    CHECK(ufo_absorb_item(&u, UFO_ITEM_TYPE_P, 1, 1000) == 0);
    CHECK(u.cnt_p == 1);
    CHECK(u.cnt_money == 0);
    ufo_reset(&u);
    CHECK(ufo_spawn(&u, 4242u, UFO_COLOR_BLUE));
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

    /* ============ 占位期令牌颜色轮转表（★ 这张表的「值必须合法」是踩过坑的，见 impl.md §2.4）============
     * 曾经把 `[0,1]` 的散列 cast 成 int 当颜色 ⇒ 几乎恒为 `UFO_COLOR_NONE` ⇒ 令牌对所有代码路径
     * 不可见、每 300 帧泄漏一个真敌人、且永远召唤不出 UFO。⇒ 这里把「值合法 + 四色都覆盖」钉成断言。 */
    {
        int i2, seen[5] = { 0, 0, 0, 0, 0 };           /* 索引 0=未用, 1=红, 2=蓝, 3=绿, 4=彩虹 */
        CHECK(UFO_TOKEN_COLOR_ROTATION_N == 7);
        for (i2 = 0; i2 < UFO_TOKEN_COLOR_ROTATION_N; ++i2)
            CHECK(ufo_color_valid(k_ufo_token_color_rotation[i2]));
        /* 轮转函数：前 12 个 serial 必须四色齐、周期 = 表长、且**每一个**都是合法色 */
        for (i2 = 0; i2 < 12; ++i2) {
            int32_t c2 = ufo_token_color_for_serial((uint32_t)i2);
            CHECK(ufo_color_valid(c2));
            CHECK(c2 != UFO_COLOR_NONE);               /* ★ 就是这一条当年会红 */
            if (c2 == UFO_COLOR_RED) seen[1] = 1;
            else if (c2 == UFO_COLOR_BLUE) seen[2] = 1;
            else if (c2 == UFO_COLOR_GREEN) seen[3] = 1;
            else if (c2 == UFO_COLOR_RAINBOW) seen[4] = 1;
        }
        CHECK(seen[1] && seen[2] && seen[3] && seen[4]); /* ★ 红蓝绿彩虹都出现（否则对应分支是死代码）*/
        for (i2 = 0; i2 < UFO_TOKEN_COLOR_ROTATION_N; ++i2)
            CHECK(ufo_token_color_for_serial((uint32_t)i2) ==
                  ufo_token_color_for_serial((uint32_t)(i2 + UFO_TOKEN_COLOR_ROTATION_N)));  /* 周期 = 表长 */
        /* ★★ 表的**顺序**是设计（踩过第二个坑）：把每个「连续 3 个 serial」窗口**直接**做成一个组
         * （纯分类，不带跨窗口状态），6 个窗口里**两种凑组规则都必须出得来** —— 否则那一种的奖励
         * 永远走不到。反例（曾经用的表）：周期 4 的 `红 蓝 绿 彩虹` 是 4 个互异值上的连续段 ⇒
         * 每个窗口都「三色互不相同」⇒ **每次召唤都是彩虹 UFO**，红/绿奖励成死代码。
         * ⚠ 穷举结论：**周期 ≤ 6 无解**（4^4..4^6 全跑过）；7 是最短的能全覆盖的长度。⇒ 断言里
         * `UFO_TOKEN_COLOR_ROTATION_N >= 7` 也是这条结论的一部分，改短表会红。 */
        {
            int got_same = 0, got_rainbow = 0;
            for (i2 = 0; i2 < UFO_TOKEN_COLOR_ROTATION_N; ++i2) {
                int32_t made;
                ufo_group_reset(&g);
                g.slot[0] = ufo_token_color_for_serial((uint32_t)i2);
                g.slot[1] = ufo_token_color_for_serial((uint32_t)(i2 + 1));
                g.slot[2] = ufo_token_color_for_serial((uint32_t)(i2 + 2));
                g.filled = UFO_SLOT_COUNT;
                made = ufo_group_classify(&g);
                if (made == UFO_COLOR_RAINBOW) got_rainbow = 1;
                else if (made != UFO_COLOR_NONE) got_same = 1;   /* 红/蓝/绿 三同色 */
            }
            CHECK(got_same);                            /* ★ 三同色那条规则够得着 */
            CHECK(got_rainbow);                         /* ★ 三互不相同那条规则也够得着 */
            CHECK(UFO_TOKEN_COLOR_ROTATION_N >= 7);     /* ★ 穷举下界：周期 ≤ 6 没有任何一张表能全覆盖 */
        }
        /* 再走一遍**有状态**的自然序列（连续喂 3N 个令牌，用 `ufo_group_put` 的返回码驱动、
         * 成组就消费掉）——
         * ⚠ **这条只断言「一直在推进 + 至少能出成组」**，不要求彩虹也在这里出现：
         *   `ufo_group_put` 满格且不成组时会**脱困丢最旧**，实际窗口与「连续 3 个」并不一一对应
         *   （实测这条序列 21 步只出三同色，彩虹要靠玩家挑色 / 令牌被跳过才出现）。
         *   彩虹那条规则的可达性由上面「纯分类遍历所有窗口」那条断言保证。 */
        {
            int got_same = 0, got_rainbow = 0, steps = 0;
            ufo_group_reset(&g);
            CHECK(g.filled == 0);                      /* 从前一段的状态里干净出来 */
            for (i2 = 0; i2 < UFO_TOKEN_COLOR_ROTATION_N * 3; ++i2) {
                if (ufo_group_put(&g, ufo_token_color_for_serial((uint32_t)i2), NULL) == UFO_GROUP_PUT_READY) {
                    int32_t made = ufo_group_classify(&g);
                    if (made == UFO_COLOR_RAINBOW) got_rainbow = 1; else got_same = 1;
                    (void)ufo_group_consume(&g);
                    ++steps;
                }
                CHECK(g.filled >= 0 && g.filled <= UFO_SLOT_COUNT);   /* 状态机一直在界内 */
                CHECK(ufo_slots_filled(&g) == g.filled);
            }
            CHECK(steps > 0);                          /* 21 颗令牌里至少出过几次成组 */
            CHECK(got_same);                           /* 自然序列里「三同色」够得着 */
            (void)got_rainbow;                         /* 见上：彩虹的可达性由纯分类那条断言保证 */
        }
        CHECK(ufo_color_valid(ufo_token_color_for_serial(123456789u)));
    }

    /* ============ ★ 金令牌的出现规则（`ufo_token_color_for_serial_gold`，用户裁决 ①）============
     * 金 ≡ 蓝 ⇒ 金**只替换表里轮到「蓝」的那一格**（每隔 `UFO_GOLD_CYCLE` 个周期一次）
     * ⇒ 判据：① 值永远合法、永不为 NONE；② 只在「表里的蓝」那些 serial 上出金；
     *   ③ 在 `2 × 表长` 个 serial 之内两种都出现（否则「金 P 图」是死代码）。 */
    {
        int i2, seen_gold = 0, seen_blue = 0, bad = 0;
        CHECK(UFO_GOLD_CYCLE >= 2);                    /* 1 = 每个周期都金 ⇒ 蓝色令牌再也不出现 */
        for (i2 = 0; i2 < UFO_TOKEN_COLOR_ROTATION_N * (UFO_GOLD_CYCLE + 2); ++i2) {
            int32_t base = ufo_token_color_for_serial((uint32_t)i2);
            int32_t with = ufo_token_color_for_serial_gold((uint32_t)i2);
            CHECK(ufo_color_valid(with));
            CHECK(with != UFO_COLOR_NONE);
            /* ① 只动「蓝」那一格：其它色必须原样 */
            if (base != UFO_COLOR_BLUE && with != base) bad = 1;
            /* ② 出了金 ⇒ 表里那一格必须本来是蓝 */
            if (with == UFO_COLOR_GOLD && base != UFO_COLOR_BLUE) bad = 1;
            if (with == UFO_COLOR_GOLD) seen_gold = 1;
            if (with == UFO_COLOR_BLUE) seen_blue = 1;
            /* ③ 把它当令牌颜色喂进组里：默认取值下金 ≡ 蓝（不进「三色各一」的桶）*/
#if UFO_GOLD_COUNTS_AS_BLUE
            if (with == UFO_COLOR_GOLD)
                CHECK(ufo_color_norm(with) == UFO_COLOR_BLUE);
#endif
        }
        CHECK(bad == 0);
        CHECK(seen_gold);                              /* ★ 实机上真的能见到金（否则整段是死代码）*/
        CHECK(seen_blue);                              /* ★ 蓝也还在（金只是「蓝的调色版」）*/
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
    CHECK(ufo_group_consume(NULL) == UFO_COLOR_NONE);

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
    CHECK(ufo_group_consume(&g) == UFO_COLOR_RED);
    CHECK(g.filled == 0);
    /* 三色各一 ⇒ READY */
    ufo_group_reset(&g);
    CHECK(ufo_group_put(&g, UFO_COLOR_RED, NULL) == UFO_GROUP_PUT_HELD);
    CHECK(ufo_group_put(&g, UFO_COLOR_BLUE, NULL) == UFO_GROUP_PUT_HELD);
    CHECK(ufo_group_put(&g, UFO_COLOR_GREEN, NULL) == UFO_GROUP_PUT_READY);
    CHECK(ufo_group_consume(&g) == UFO_COLOR_RAINBOW);
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
            CHECK(ufo_group_consume(&g) != UFO_COLOR_NONE);
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
        CHECK(ufo_group_consume(&g) == UFO_COLOR_RED);
    }

    /* ==================== ★ 五边形框触发条件（用户 2026-09-12 裁决 ③；纯逻辑判据）====================
     * 用户原话：「玩家手上**有两个令牌**（= 再来一枚就能成组）时，屏幕上**这类令牌**外面会有一圈
     * 五边形框」。两级判据：
     *   ① `ufo_group_pentagon_hint(g)`：槽里此刻**差一枚就成组**（∃ 某个颜色能补上）；
     *   ② `ufo_pentagon_hint_for_color(g, c)`：**场上某一枚**令牌（收它 = 放颜色 c 进槽）够不够格被画框。
     * ⚠ 本卡**不画**这个框（缺 anm 脚本，卡侧只有 `ufo_pentagon_hint_update()` 预留钩子）；
     *   这一节只钉「什么时候该画」。 */
    ufo_group_reset(&g);
    CHECK(!ufo_group_pentagon_hint(&g));               /* 0 枚 ⇒ 没有 */
    CHECK(ufo_group_put(&g, UFO_COLOR_RED, NULL) == UFO_GROUP_PUT_HELD);
    CHECK(!ufo_group_pentagon_hint(&g));               /* 1 枚 ⇒ 还不够一枚就成组 */
    CHECK(ufo_group_put(&g, UFO_COLOR_RED, NULL) == UFO_GROUP_PUT_HELD);
    CHECK(ufo_group_pentagon_hint(&g));                /* ★ 2 枚 ⇒ 来 */
    CHECK(ufo_pentagon_hint_for_color(&g, UFO_COLOR_RED));      /* 收了红 ⇒ 红红红 ⇒ 成组 */
    CHECK(ufo_pentagon_hint_for_color(&g, UFO_COLOR_RAINBOW));  /* 收了彩虹 ⇒ 红红彩 ⇒ 红 UFO */
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
    (void)ufo_group_consume(&g);
    /* 空指针 */
    CHECK(!ufo_group_pentagon_hint(NULL));
    CHECK(!ufo_pentagon_hint_for_color(NULL, UFO_COLOR_RED));

    /* ============================================ 合成消费（consume）与 last_group */
    CHECK(group_of(UFO_COLOR_GREEN, UFO_COLOR_GREEN, UFO_COLOR_GREEN, &g) == UFO_COLOR_GREEN);
    c = ufo_group_consume(&g);
    CHECK(c == UFO_COLOR_GREEN);
    CHECK(g.filled == 0);                              /* 三个槽清 0（spec §8 实跑清单） */
    CHECK(g.slot[0] == UFO_COLOR_NONE);
    CHECK(g.slot[1] == UFO_COLOR_NONE);
    CHECK(g.slot[2] == UFO_COLOR_NONE);
    CHECK(g.last_group == UFO_COLOR_GREEN);            /* 彩虹 UFO 的「按最后令牌取色」要用它 */
    CHECK(ufo_group_consume(&g) == UFO_COLOR_NONE);    /* 没得合 ⇒ 状态不变 */
    CHECK(g.last_group == UFO_COLOR_GREEN);
    /* 第二次合成覆盖 last_group */
    CHECK(group_of(UFO_COLOR_RED, UFO_COLOR_BLUE, UFO_COLOR_GREEN, &g) == UFO_COLOR_RAINBOW);
    c = ufo_group_consume(&g);
    CHECK(c == UFO_COLOR_RAINBOW);
    CHECK(g.last_group == UFO_COLOR_RAINBOW);
    /* 合成后能继续攒 */
    CHECK(ufo_slot_set(&g, UFO_COLOR_BLUE));
    CHECK(g.filled == 1);
    CHECK(g.last_group == UFO_COLOR_RAINBOW);          /* 入槽不动 last_group */

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
    CHECK(!ufo_spawn(&u, 0, UFO_COLOR_RED));           /* 非法 id */
    CHECK(!ufo_spawn(NULL, 1, UFO_COLOR_RED));         /* 空指针 */
    CHECK(ufo_spawn(&u, 4242u, UFO_COLOR_RED));
    CHECK(ufo_present(&u));
    CHECK(u.ufo_id == 4242u);
    CHECK(u.frames == 0);
    CHECK(u.color == UFO_COLOR_RED);
    CHECK(u.fill_millionths == 0);
    CHECK(!ufo_can_spawn(&u));                         /* 单例：场上有一个就不许再召唤 */
    CHECK(!ufo_spawn(&u, 4243u, UFO_COLOR_BLUE));      /* 被拒且不改状态 */
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
    CHECK(ufo_spawn(&u, 7u, UFO_COLOR_RAINBOW));
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
    CHECK(ufo_spawn(&u, 99u, UFO_COLOR_GREEN));
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
    CHECK(ufo_spawn(&u, 100u, UFO_COLOR_BLUE));
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
    /* 合成判定：两件都要过 */
    CHECK(ufo_step_allowed(0.0f, 0.0f, 300.0f, 300.0f));
    CHECK(!ufo_step_allowed(0.0f, 0.0f, 0.0f, 0.0f));       /* 自机太近 */
    CHECK(!ufo_step_allowed(250.0f, 0.0f, 0.0f, 0.0f));     /* 出屏 */
    CHECK(!ufo_step_allowed(0.0f, 500.0f, 0.0f, 0.0f));     /* 出屏（下方）*/

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
    CHECK(ufo_spawn(&u, 1u, UFO_COLOR_RED));
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
    CHECK(ufo_spawn(&u, 2u, UFO_COLOR_RAINBOW));
    CHECK(ufo_absorb_item(&u, 1, 1, 1000) == 0);
    CHECK(u.cnt_money == 1);
    CHECK(u.cnt_p == 0);
    CHECK(ufo_absorb_item(&u, 2, 1, 1000) == 0);
    CHECK(u.cnt_p == 1);
    CHECK(u.cnt_money == 1);

    /* ================================== 吸满吐奖励表（一手表抄；type 号映射到 TH18，★⏳ 待拍） */
    CHECK(UFO_REWARD_LIFE_FRAG == 4);                  /* TH18 type 4 = 命碎片（一手语义）*/
    CHECK(UFO_REWARD_BOMB_FRAG == 6);                  /* TH18 type 6 = 符卡碎（一手语义）*/
    CHECK(UFO_REWARD_NONE == 0);
    CHECK(ufo_spit_reward(UFO_COLOR_RED) == UFO_REWARD_LIFE_FRAG);
    CHECK(ufo_spit_reward(UFO_COLOR_GREEN) == UFO_REWARD_BOMB_FRAG);
    CHECK(ufo_spit_reward(UFO_COLOR_BLUE) == UFO_REWARD_NONE);      /* 一手：switch 无 case 2 */
    CHECK(ufo_spit_reward(UFO_COLOR_GOLD) == UFO_REWARD_NONE);      /* ★ 金照蓝（用户裁决 ①）*/
    CHECK(ufo_spit_reward(UFO_COLOR_RAINBOW) == UFO_REWARD_NONE);   /* 一手吐令牌，TH18 无对应 type */
    CHECK(ufo_spit_reward(UFO_COLOR_NONE) == UFO_REWARD_NONE);
    CHECK(ufo_spit_reward(999) == UFO_REWARD_NONE);
    /* 击破的颜色加成（一手 `th12:0x44af00`：红 → 道具 4；绿 → 道具 5；蓝 / 彩虹无额外）*/
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
    CHECK(ufo_group_consume(&g) == UFO_COLOR_RAINBOW);
    CHECK(g.last_slot2 == UFO_COLOR_GREEN);            /* ★ 第 3 枚的颜色留下来 */
    CHECK(g.last_group == UFO_COLOR_RAINBOW);
    CHECK(g.filled == 0);
    CHECK(ufo_break_token_state(g.last_group, g.last_slot2) == 2);          /* 彩虹 UFO ⇒ 相位 2 */
    ufo_group_reset(&g);
    CHECK(g.last_slot2 == UFO_COLOR_NONE);             /* reset 清掉（不会串到下一组）*/

    /* ================= ★★ 令牌运动（一手 `th12:0x425c20` state 6；用户口述的「飞行」） */
    {
        ufo_tok_motion_t m;
        float x, y;
        /* 一手：`0x4a4270` = 1.5f、角度 `0x4a4274` = π/4 / `0x4a4278` = 3π/4 按奇偶交替，
         * 且计数在自增之后判 ⇒ **序号 0（本关第 1 枚）= 左下 45°**。 */
        CHECK(feq(UFO_TOK_FLY_SPEED, 1.5f, 1e-6f));
        CHECK(feq(UFO_TOK_ANGLE_DOWN_RIGHT, 0.785398185f, 1e-6f));   /* π/4 */
        CHECK(feq(UFO_TOK_ANGLE_DOWN_LEFT, 2.3561945f, 1e-6f));      /* 3π/4 */
        CHECK(feq(UFO_TOK_VEL_COMP, 1.060660124f, 1e-5f));           /* 1.5 × √2/2 */
        ufo_tok_motion_init(&m, 0u);                                 /* 第 1 枚 */
        CHECK(m.vx < 0.0f && feq(m.vy, UFO_TOK_VEL_COMP, 1e-6f));    /* 左下 45° */
        CHECK(m.age == 0);
        ufo_tok_motion_init(&m, 1u);                                 /* 第 2 枚 */
        CHECK(m.vx > 0.0f && feq(m.vy, UFO_TOK_VEL_COMP, 1e-6f));    /* 右下 45° */
        ufo_tok_motion_init(&m, 2u);
        CHECK(m.vx < 0.0f);                                          /* 再交替回来 */

        /* 位移：每帧 vx/vy（一手 `0x4263a1`–`0x4263cb`）*/
        ufo_tok_motion_init(&m, 1u);                                 /* 右下 */
        x = 0.0f; y = 0.0f;
        CHECK(ufo_tok_motion_step(&m, &x, &y) == 0);
        CHECK(feq(x, UFO_TOK_VEL_COMP, 1e-6f) && feq(y, UFO_TOK_VEL_COMP, 1e-6f));
        CHECK(m.age == 1);

        /* 反弹：右边界 176（一手 `0x4a4198`）⇒ vx 取反；下边界 370（`0x4a4408`）⇒ vy 取反 */
        ufo_tok_motion_init(&m, 1u);
        x = 175.5f; y = 0.0f;
        CHECK(ufo_tok_motion_step(&m, &x, &y) == 0);
        CHECK(m.vx < 0.0f);                                          /* 撞右边界 ⇒ 反射（`0x4a4168` = −1.0）*/
        CHECK(feq(x, 175.5f + UFO_TOK_VEL_COMP, 1e-5f));             /* 位置不被夹，只有速度翻 */
        ufo_tok_motion_init(&m, 0u);
        x = -175.5f; y = 0.0f;
        CHECK(ufo_tok_motion_step(&m, &x, &y) == 0);
        CHECK(m.vx > 0.0f);                                          /* 撞左边界（−176）*/
        ufo_tok_motion_init(&m, 1u);
        x = 0.0f; y = 369.5f;
        CHECK(ufo_tok_motion_step(&m, &x, &y) == 0);
        CHECK(m.vy < 0.0f);                                          /* 撞下边界 370 */
        x = 0.0f; y = 100.0f;
        ufo_tok_motion_init(&m, 0u);                                 /* 左下 ⇒ vy > 0 */
        m.vy = -UFO_TOK_VEL_COMP;                                    /* 手工构造「向上撞上边界」*/
        y = 160.5f;
        CHECK(ufo_tok_motion_step(&m, &x, &y) == 0);
        CHECK(m.vy > 0.0f);                                          /* 撞上边界 160（`0x4a3f4c`）*/

        /* 方向守卫：朝外走才翻（一手四条反弹都带 `vel.x < 0` / `vel.x > 0` 的守卫）*/
        ufo_tok_motion_init(&m, 1u);                                 /* 右下：vx > 0、vy > 0 */
        m.vx = -m.vx;                                                /* 已经朝左，却贴在右边界 */
        x = 176.0f; y = 0.0f;
        CHECK(ufo_tok_motion_step(&m, &x, &y) == 0);
        CHECK(m.vx < 0.0f);                                          /* 不被翻回来 */

        /* 1200 帧之后**不再反弹**（一手 `0x4263d1 cmp [edi+0x98c],0x4b0; jge`），只判出界 */
        ufo_tok_motion_init(&m, 1u);
        m.age = UFO_TOK_NO_BOUNCE_FRAMES;
        x = 176.5f; y = 380.0f;
        CHECK(ufo_tok_motion_step(&m, &x, &y) == 0);                 /* 外框内 ⇒ 还在 */
        CHECK(m.vx > 0.0f && m.vy > 0.0f);                           /* 速度没被翻 */
        CHECK(UFO_TOK_NO_BOUNCE_FRAMES == 1200);                     /* ✅ 一手 `0x4b0` = 20 s = 8 次换色 */

        /* 出界即撤：外框 208 / −208（`0x4a43a0`/`0x4a4400`）、y −16（`0x4a43f8`）/ 464（`0x4a3e90`）*/
        ufo_tok_motion_init(&m, 1u);
        x = 207.5f; y = 0.0f;
        CHECK(ufo_tok_motion_step(&m, &x, &y) == 1);                 /* x 越过 208 ⇒ 撤 */
        ufo_tok_motion_init(&m, 0u);
        x = -207.5f; y = 0.0f;
        CHECK(ufo_tok_motion_step(&m, &x, &y) == 1);
        ufo_tok_motion_init(&m, 1u);
        x = 0.0f; y = 463.5f;
        CHECK(ufo_tok_motion_step(&m, &x, &y) == 1);                 /* y 越过 464 ⇒ 撤 */
        ufo_tok_motion_init(&m, 1u);
        x = 0.0f; y = 0.0f;
        CHECK(ufo_tok_motion_step(&m, &x, &y) == 0);                 /* 场内 ⇒ 活 */
        CHECK(ufo_tok_motion_step(NULL, &x, &y) == 1);               /* 空指针 ⇒ 报「撤」（防御）*/
        CHECK(ufo_tok_motion_step(&m, NULL, &y) == 1);
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

    /* ============================================= §4 HP 硬约束（spawn life < 1000） */
    CHECK(UFO_LIFE_LIMIT == 1000);
    CHECK(UFO_LIFE_MAX_ALLOWED == 999);
    CHECK(UFO_HP_PLACEHOLDER == 100);                  /* ★❓ 自定占位；不是复刻 */
    CHECK(UFO_HP_PLACEHOLDER < UFO_LIFE_LIMIT);
    CHECK(ufo_clamp_spawn_life(1) == 1);
    CHECK(ufo_clamp_spawn_life(500) == 500);
    CHECK(ufo_clamp_spawn_life(999) == 999);
    CHECK(ufo_clamp_spawn_life(1000) == 999);          /* 硬界：>= 1000 ⇒ 钳（一手 `0x42d8f6`）*/
    CHECK(ufo_clamp_spawn_life(1600) == 999);          /* 原作 HP 最低档 1600 会被钳 */
    CHECK(ufo_clamp_spawn_life(2500) == 999);          /* 原作 HP 最高档 */
    CHECK(ufo_clamp_spawn_life(0) == 1);               /* 出生即死 ⇒ 抬到 1 */
    CHECK(ufo_clamp_spawn_life(-42) == 1);
    CHECK(ufo_clamp_spawn_life(UFO_HP_PLACEHOLDER) == UFO_HP_PLACEHOLDER);

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

    /* 令牌 / 载体占位常量本身也要过硬约束 */
    CHECK(UFO_TOKEN_LIFE < UFO_LIFE_LIMIT);
    CHECK(UFO_TOKEN_TTL_FRAMES > 0);
    CHECK(UFO_TOKEN_MAX >= 1);

    /* ============================== 死亡事件 kind 过滤（spec §7 AUDIT：必须排除 KILL_ALL） */
    CHECK(UFO_EDEATH_KILLED == 0u);                    /* 值对账（DEATH §2.1；ufo.c 另有 _Static_assert）*/
    CHECK(UFO_EDEATH_BOMB_KILL == 1u);
    CHECK(UFO_EDEATH_ECL_DIE == 2u);
    CHECK(UFO_EDEATH_OTHER == 3u);
    CHECK(UFO_EDEATH_KILL_ALL == 4u);
    CHECK(ufo_token_kill_counts(UFO_EDEATH_KILLED));
    CHECK(ufo_token_kill_counts(UFO_EDEATH_BOMB_KILL));
    CHECK(!ufo_token_kill_counts(UFO_EDEATH_KILL_ALL));   /* ★ 过场清场不算 */
    CHECK(!ufo_token_kill_counts(UFO_EDEATH_ECL_DIE));
    CHECK(!ufo_token_kill_counts(UFO_EDEATH_OTHER));
    CHECK(!ufo_token_kill_counts(99u));
    CHECK(ufo_body_kill_counts(UFO_EDEATH_KILLED));
    CHECK(ufo_body_kill_counts(UFO_EDEATH_BOMB_KILL));
    CHECK(!ufo_body_kill_counts(UFO_EDEATH_KILL_ALL));
    CHECK(!ufo_body_kill_counts(UFO_EDEATH_ECL_DIE));
    CHECK(!ufo_body_kill_counts(UFO_EDEATH_OTHER));

    /* ================================ 端到端小场景：三次令牌 → 合成 → 召唤 → 吸 → 收尾 */
    ufo_group_reset(&g);
    ufo_reset(&u);
    CHECK(ufo_slot_set(&g, UFO_COLOR_RED));
    CHECK(ufo_slot_set(&g, UFO_COLOR_RED));
    CHECK(ufo_slot_set(&g, UFO_COLOR_RAINBOW));        /* 彩虹补位 */
    CHECK(ufo_group_has_match(&g));
    CHECK(ufo_can_spawn(&u));
    c = ufo_group_consume(&g);
    CHECK(c == UFO_COLOR_RED);
    CHECK(ufo_spawn(&u, 555u, c));
    CHECK(ufo_present(&u));
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
                if (put == UFO_GROUP_PUT_READY) ufo_group_consume(&g);
            }
            for (i2 = 0; i2 < 8 && !reached; ++i2)
                if (ufo_group_put(&g, spare[i2], NULL) == UFO_GROUP_PUT_READY) {
                    reached = 1;
                    ufo_group_consume(&g);
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
