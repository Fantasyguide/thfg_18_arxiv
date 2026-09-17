/* test_vitem_batch.c —— ★★ 2026-09-15（`NEXT.md` §1x.2）：**自建实体（110/111 的大 / 中绿点）
 * → 114（辉针城）的收点批** 的**接线级**单测（`make test-host`）。
 *
 * ## 为什么另开一个文件
 * 这项改动横跨两个模块，两边各自还有自己的单测（`test_sdk_core` 钉层的契约、
 * `test_collect2x_core` 钉批的算法），但**「两条通道合起来到底对不对」只有把它们接起来跑才看得见**：
 *   · 生产者 = SDK 的「虚拟收集 / 虚拟在飞」层（`sdk_core.c`）+ `cards/faith.c` 的两行调用；
 *   · 消费者 = 114 的批 core（`cards/collect2x_core.c`）+ `cards/collect2x.c` 的两行读取。
 * ⇒ 本文件**同时**链接 `sdk_core.c` 与 `cards/collect2x_core.c`，按两边**接线层里同一段代码的形状**
 * 一帧一帧对跑（helper 的注释逐行标明它复刻的是哪一段）。
 *
 * ## 引擎侧那一半为什么不在 host 上
 * `cards/faith.c` / `cards/collect2x.c` 的接线靠引擎（断点、道具池、VM），host 上跑不了
 * ⇒ 本文件里**没有**的、由引擎侧保证的两件事，列在这里当契约（各自有归属的回归门）：
 *   ① `faith.c` 只在**大 / 中档**上报（微档 → `CE_VITEM_HALF`，`faith_vitem_kind()`）；
 *   ② `collect2x.c` 里 presence 的条件是 `ce_item_pool_any_attracting() || ce_vitem_flying() > 0`
 *      —— 原生池那一支由 `test_collect2x_core` 的既有 presence 用例覆盖（本文件只跑自建那一支）。
 *
 * ## 钉住的四条（用户验收）
 *   A. **成员 + 面值**（★ 2026-09-18 B11 改口径）：自建实体让批 **count 涨**、并把**面值**加进
 *      `batch_score`（TH14 的成员加入点 `0x439c90` 就是 `count++` 与 `batch_score += 身价` 成对）
 *      ⇒ 结算补差**非零**；`ce_add_score_delta()` 那一笔由接线层做（host 上不在本文件的范围内），
 *      但 `out.batch_score` 必须等于这些面的和 —— 旧口径「只 +1、批分恒 0」会让弹字的价值行
 *      在纯绿点批上显示 0（用户 2026-09-17 实跑报的就是这一条）；
 *   B. **微档不入批**（§1x.3）：半道具两条通道都不认 ⇒ 一颗都不进批；
 *   C. **在飞期间 presence 为真、收完为假**：点在飞（声明 > 0）时批**跨过 idle 窗**都不结算；
 *      声明 0（收完 / 落地 / 出界）之后 idle 走完才结算；
 *   D. **114 不在场 = 零副作用**：没有消费者时两条通道自过期，消费者一上来看到的计数是 0。
 */
#include <stdio.h>
#include <string.h>
#include "../sdk_core.h"
#include "../cards/collect2x_core.h"

static int s_fail, s_pass;
#define CHECK(cond) do { if (cond) ++s_pass; else { ++s_fail; printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } } while (0)

static int32_t g_frame;          /* 本 run 的游玩帧（= 114 的 `s_play_frame`），与 `ce_sdk_frame()` 同步推进 */

/* 帧界：`sdk.c` 的 `BP_ce_item_frame`（`0x446ee3`）每游玩帧到一次 —— 它**同时**是 SDK 帧号的
 * 唯一递增处（`ce_sdk_frame_bump`）与 114 的 `on_item_frame` 分派点 ⇒ 两者天然同号，本文件照此对齐。*/
static void frame_begin(void)
{
    ce_sdk_frame_bump();
    ++g_frame;
    CHECK((int32_t)ce_sdk_frame() == g_frame);
}

/* ---- 生产者（`cards/faith.c`）-------------------------------------------- */
/* `faith.c` 的 `faith_pool_tick()` 尾部：声明本帧还有几颗大 / 中点在飞（`kind` 恒 FULL —— 微档
 * 走 `faith_vitem_kind()` → HALF，那一条在测试 C 里单独调）。*/
static void producer_flying(int kind, int n) { ce_vitem_declare_flying(CE_VITEM_SRC_SHARED, kind, n); }
/* `faith.c` 的 `dot_collect()`：点被收的那一刻报一颗 + **这颗点的信仰面值**（`faith_dot_t.value`；
 * **一台机器只报一次**，110/111 共用池天然去重）。默认面值 0 的版本给「只关心计数」的用例用。*/
static void producer_collect(int kind, int n)            { ce_vitem_note_collect(kind, n, 0); }
static void producer_collect_value(int kind, int n, int32_t v) { ce_vitem_note_collect(kind, n, v); }

/* ---- 消费者（`cards/collect2x.c` 的 `on_item_frame` 核心段）---------------- */
/* 逐行复刻接线层里的那一段（顺序也一致）：
 *   ① 抽干自建实体成员 → `c2x_collect_virtual()`（**在 `batch_open` 门之前** ⇒ 第一颗绿点就能开批）；
 *   ② 批开时才看 presence —— 原生真空（host 上不可得，见文件头）**或**自建在飞 ⇒ 续 idle；
 *   ③ 都不在飞才轮到 `c2x_maybe_settle()` 判批闭。
 * 返回 1 = 本帧结算了（与接线层的 `c2x_maybe_settle` 同返回值）。*/
static int consumer_frame(c2x_state_t *s, c2x_settle_out_t *out)
{
    int32_t vval = 0;
    const int vn = ce_vitem_take_collect(&vval);
    if (vn > 0) c2x_collect_virtual(s, vn, vval, g_frame);   /* ★ 面值一并进批分（B11）*/
    if (!s->batch_open) return 0;
    if (ce_vitem_flying() > 0) { c2x_batch_presence(s, g_frame); return 0; }
    return c2x_maybe_settle(s, g_frame, out);
}

/* ---- A：成员 + 面值（绿点入批**并把面值算进批分**；B11 改口径），在飞期间续命、收完后 idle 满才结 ----
 * 剧情：25 颗大绿点（用户口径：大 / 中两档计入，面值 5000 = TH10 大信仰点在 N 难度的值）
 *       分 25 帧被收，每帧都还有点在飞；收完后（生产者声明 0）再等 idle 窗 ⇒ 结算。*/
static void test_A_members_and_presence(void)
{
    c2x_state_t s;
    c2x_settle_out_t out;
    int settled_at = -1;

    ce_vitem_reset();
    ce_score_reset();
    ce_score_bridge(50000, 1);                       /* 身价 50000 ⇒ G_0 = 5000 点（计分层在场）*/
    const int32_t score0 = ce_score_total();
    memset(&s, 0, sizeof s);
    g_frame = (int32_t)ce_sdk_frame();                /* 与 SDK 帧号对齐（同一个进程里唯一会 bump 的就是本文件）*/

    /* 25 帧：每帧收一颗，剩下的都在飞（生产者先声明、消费者后跑 —— 现实中两个断点的顺序不保证，
     * 顺序的另一半在测试 E 里跑）*/
    for (int i = 0; i < 25; ++i) {
        frame_begin();
        producer_flying(CE_VITEM_FULL, 25 - i);
        producer_collect_value(CE_VITEM_FULL, 1, 5000);
        CHECK(consumer_frame(&s, &out) == 0);         /* 批开着、点在飞：本帧必然不结 */
        CHECK(s.batch_open == 1 && s.batch_count == i + 1);
        CHECK(s.batch_score == (int64_t)(i + 1) * 5000);   /* ★ 面值逐颗进批分（B11）*/
    }
    CHECK(s.batch_count == 25 && s.batch_score == 25 * 5000);

    /* 点在飞但**一颗都不收**：跨过 idle 窗（8 帧）好几倍也不许结算 —— 这条就是「在飞期间
     * presence 为真」的判据（没有它，批会在我们这些点还在飞的时候被判「收点流停了」）。*/
    for (int i = 0; i < 30; ++i) {
        frame_begin();
        producer_flying(CE_VITEM_FULL, 3);
        CHECK(consumer_frame(&s, &out) == 0);
        CHECK(s.batch_open == 1 && s.batch_count == 25);
    }

    /* 收完（生产者声明 0）⇒ 从**最后一次 presence** 起算 idle，窗内不许结 */
    frame_begin();
    producer_flying(CE_VITEM_FULL, 0);
    producer_collect_value(CE_VITEM_FULL, 2, 2 * 5000);   /* 最后两颗（同帧一批：面值合计 10000）*/
    CHECK(consumer_frame(&s, &out) == 0);
    CHECK(s.batch_count == 27 && s.batch_score == 27 * 5000);

    for (int i = 0; i < C2X_BATCH_SETTLE_IDLE - 1; ++i) {
        frame_begin();
        producer_flying(CE_VITEM_FULL, 0);
        CHECK(consumer_frame(&s, &out) == 0);         /* idle 满−1：不结 */
        CHECK(s.batch_open == 1);
    }
    frame_begin();
    producer_flying(CE_VITEM_FULL, 0);
    CHECK(consumer_frame(&s, &out) == 1);             /* idle 满 ⇒ 结 */
    settled_at = g_frame;

    CHECK(out.mult_permille == C2X_TABLE_M1);         /* 27 颗 ⇒ 20 档 ×0.5（档位逻辑一字未改）*/
    CHECK(out.batch_score == 27 * 5000);              /* ★ 纯绿点批的批分 = Σ面值（B11）*/
    /* ⇒ 补差非零（这就是用户抱怨的那条：「×1.6 但分数 0」不再出现）*/
    CHECK(out.batch_score * (int64_t)(out.mult_permille - 1000) / 1000 == -(27 * 5000) / 2);
    CHECK(out.fragment_type == C2X_FRAG_BOMB && out.fragment_count == 1);       /* 碎片逻辑不变 */
    CHECK(s.batch_open == 0 && s.batch_count == 0);
    CHECK(settled_at > 0);
    /* ★ 计分层/SCORE 一分没动：**本层只声明事实**，写 SCORE 的是接线层的
     *   `ce_add_score_delta(batch_score × (mult−1000)/1000)`（B11 之后那一笔不再恒 0）。*/
    CHECK(ce_score_total() == score0 && ce_score_participants() == 0);
}

/* ---- B：微档（消弹那档）不入批（§1x.3）---------------------------------------------
 * 半道具「实体只为动画」⇒ 两条通道都不认：收 30 颗微档 + 一直在飞，批**一次都不开**。*/
static void test_B_half_items_never_enter(void)
{
    c2x_state_t s;
    c2x_settle_out_t out;

    ce_vitem_reset();
    memset(&s, 0, sizeof s);

    for (int i = 0; i < 30; ++i) {
        frame_begin();
        producer_flying(CE_VITEM_HALF, 12);           /* 微档在飞：不进 presence */
        producer_collect(CE_VITEM_HALF, 1);           /* 微档被收：不进成员 */
        CHECK(consumer_frame(&s, &out) == 0);
        CHECK(s.batch_open == 0 && s.batch_count == 0 && s.batch_score == 0);
    }
    for (int i = 0; i < C2X_BATCH_SETTLE_IDLE + 2; ++i) {     /* 微档停手：批照样是关的（从来没开过）*/
        frame_begin();
        producer_flying(CE_VITEM_HALF, 0);
        CHECK(consumer_frame(&s, &out) == 0);
    }
    CHECK(s.batch_open == 0 && s.batch_count == 0);

    /* 紧接着来一颗**大 / 中**档（完整道具）⇒ 它才开批、只算它一颗（微档那 30 颗一颗都没混进来）*/
    frame_begin();
    producer_flying(CE_VITEM_FULL, 1);
    producer_collect(CE_VITEM_FULL, 1);
    CHECK(consumer_frame(&s, &out) == 0);
    CHECK(s.batch_open == 1 && s.batch_count == 1);
}

/* ---- C：114 不在场（没人抽干 / 没人读）= 零副作用 --------------------------------
 * 生产者照常干活（收点 + 声明在飞），但**没有 114 的帧点**在跑 —— 两条通道都必须自过期。*/
static void test_C_no_consumer_zero_side_effect(void)
{
    c2x_state_t s;
    c2x_settle_out_t out;

    ce_vitem_reset();
    ce_score_reset();
    ce_score_bridge(10000, 1);
    const int32_t score0 = ce_score_total();
    memset(&s, 0, sizeof s);

    for (int i = 0; i < 10; ++i) {                    /* 生产者跑 10 帧：收 5 颗、一直有 5 颗在飞 */
        frame_begin();
        producer_flying(CE_VITEM_FULL, 5);
        producer_collect(CE_VITEM_FULL, i < 5 ? 1 : 0);
        if (i == 4) {
            /* 消费者还没上来：层里挂着刚刚那 5 颗（容差窗内）——「挂着」本身不算副作用 */
            CHECK(ce_vitem_collect_pending() == 5);
            CHECK(ce_vitem_flying() == 5);
        }
    }

    for (int i = 0; i < CE_VITEM_HOLD_FRAMES + 1; ++i) frame_begin();   /* 空跑：生产者退出 / 没有消费者 */

    /* 114 现在才上来（例如刚装卡）：陈年计数必须已经作废 —— 不会「突然多出 5 颗」*/
    CHECK(ce_vitem_collect_pending() == 0);
    CHECK(ce_vitem_flying() == 0);
    CHECK(consumer_frame(&s, &out) == 0);
    CHECK(s.batch_open == 0 && s.batch_count == 0 && s.batch_score == 0);
    CHECK(ce_score_total() == score0 && ce_score_participants() == 0);   /* 引擎侧的分一分没动 */

    /* 消费者上线后才收的那一颗 ⇒ 才计 */
    frame_begin();
    producer_flying(CE_VITEM_FULL, 1);
    producer_collect(CE_VITEM_FULL, 1);
    CHECK(consumer_frame(&s, &out) == 0);
    CHECK(s.batch_open == 1 && s.batch_count == 1);
}

/* ---- D：帧内顺序无关（生产者**晚于**消费者跑）--------------------------------------
 * 现实中生产者 = 卡的 `on_tick_2`、消费者 = `ce_item_frame` 断点，谁先谁后没有保证
 * ⇒ 层带 `CE_VITEM_HOLD_FRAMES` 容差：比消费者早一帧报的那一颗**不许丢**。*/
static void test_D_frame_order_tolerance(void)
{
    c2x_state_t s;
    c2x_settle_out_t out;

    ce_vitem_reset();
    memset(&s, 0, sizeof s);

    /* 第 1 帧：消费者先跑（此时什么都没收到），生产者后跑（收 1 颗 + 1 颗在飞）*/
    frame_begin();
    CHECK(consumer_frame(&s, &out) == 0);
    producer_flying(CE_VITEM_FULL, 1);
    producer_collect(CE_VITEM_FULL, 1);

    /* 第 2 帧：生产者先跑（再收 1 颗）——消费者这一帧应当看到**两颗**（第 1 帧那颗没丢）*/
    frame_begin();
    producer_flying(CE_VITEM_FULL, 2);
    producer_collect(CE_VITEM_FULL, 1);
    CHECK(consumer_frame(&s, &out) == 0);
    CHECK(s.batch_open == 1 && s.batch_count == 2);

    /* 在飞声明也同理：生产者上一帧声明的「还有 2 颗在飞」，消费者这一帧照读得到 */
    frame_begin();
    CHECK(ce_vitem_flying() == 2);
    CHECK(consumer_frame(&s, &out) == 0);
    CHECK(s.batch_open == 1);
}

int main(void)
{
    test_A_members_and_presence();
    test_B_half_items_never_enter();
    test_C_no_consumer_zero_side_effect();
    test_D_frame_order_tolerance();
    printf("test_vitem_batch: %d passed, %d failed\n", s_pass, s_fail);
    return s_fail ? 1 : 0;
}
