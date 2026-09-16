/* test_collect2x_core.c —— collect2x_core.c 的主机单测（make test-host）。
 * 批闭语义（§Z18/§Z20/§Z21）：presence+timer 门 = 批集道具吸附中（接线层 c2x_batch_presence 续期）
 * 或无新收使 idle ≥ C2X_BATCH_SETTLE_IDLE（8，TH14 原版值）游玩帧才关批。批闭只发生在
 * 「吸附空 + 无新收」的真停段；落地休止残余迟到 = 拆下一批（TH14 原义）。 */
#include <stdio.h>
#include <string.h>
#include "../cards/collect2x_core.h"

static int s_fail, s_pass;
#define CHECK(cond) do { if (cond) ++s_pass; else { ++s_fail; printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } } while (0)

/* 收集 n 个 type（同一帧 f，模拟一帧内连续吸收），返回已开批状态。 */
static void collect_n(c2x_state_t *s, uint32_t type, int n, int32_t f)
{
    for (int i = 0; i < n; ++i) CHECK(c2x_collect_batch(s, type, f) == 1);
}

/* presence 续批 n 帧（模拟 n 帧里都有批集道具吸附中：接线层每帧扫池命中就调）。 */
static void presence_n(c2x_state_t *s, int n, int32_t f0)
{
    for (int i = 0; i < n; ++i) CHECK(c2x_batch_presence(s, f0 + i) == 1);
}

int main(void)
{
    c2x_state_t s;
    c2x_settle_out_t out;
    memset(&out, 0xcc, sizeof out);                     /* 哨兵：确认每路都清零 */

    /* 批未开：settle / presence 都不动，返回 0 */
    memset(&s, 0, sizeof s);
    CHECK(c2x_maybe_settle(&s, 1000, &out) == 0);
    CHECK(out.mult_permille == 0 && out.fragment_type == 0 && out.fragment_count == 0 && out.batch_score == 0);
    CHECK(c2x_batch_presence(&s, 1000) == 0);           /* 批未开：无事可续、不开批 */
    CHECK(s.batch_open == 0 && s.batch_count == 0 && s.last_event_at == 0);
    CHECK(c2x_maybe_settle(&s, 1000 + C2X_BATCH_SETTLE_IDLE, &out) == 0);   /* presence 不开批，settle 仍不动 */

    /* 0–19：关批但无奖（out 全零、状态清零、不再开） */
    memset(&s, 0, sizeof s);
    collect_n(&s, 1, 15, 1000); s.batch_score = 150;
    CHECK(c2x_maybe_settle(&s, 1000 + C2X_BATCH_SETTLE_IDLE, &out) == 1);
    CHECK(out.mult_permille == 0 && out.fragment_type == 0 && out.fragment_count == 0);
    CHECK(s.batch_open == 0 && s.batch_count == 0);
    CHECK(c2x_maybe_settle(&s, 1000 + C2X_BATCH_SETTLE_IDLE + 1, &out) == 0);  /* 关掉的批不会再结 */

    /* idle 门：最后一收（f=1000）后 idle 不满不结、状态保持；idle 满才结（×0.5） */
    memset(&s, 0, sizeof s);
    collect_n(&s, 2, 25, 1000); s.batch_score = 500;
    CHECK(c2x_maybe_settle(&s, 1001, &out) == 0);        /* idle 1 */
    CHECK(out.mult_permille == 0 && s.batch_open == 1 && s.batch_count == 25 && s.batch_score == 500);
    CHECK(c2x_maybe_settle(&s, 1000 + C2X_BATCH_SETTLE_IDLE - 1, &out) == 0);  /* idle 满−1 仍不结 */
    CHECK(c2x_maybe_settle(&s, 1000 + C2X_BATCH_SETTLE_IDLE, &out) == 1);      /* idle 满 → ×0.5 */
    CHECK(out.mult_permille == C2X_TABLE_M1 && out.fragment_type == C2X_FRAG_BOMB);

    /* presence 续批（§Z21 核心）：最后一收（f=1000）后窗口内任意帧有批集道具吸附中 → 续 idle，
     * settle 被顶到 presence 之后 idle 满才结 —— TH18 波内吸收空洞 8–25 帧 = 在飞散布 →
     * 续批盖掉，空洞多长都不拆批 */
    memset(&s, 0, sizeof s);
    collect_n(&s, 1, 21, 1000);
    CHECK(c2x_maybe_settle(&s, 1000 + C2X_BATCH_SETTLE_IDLE - 1, &out) == 0); /* idle 满−1：无 presence 也不结 */
    presence_n(&s, 1, 1000 + C2X_BATCH_SETTLE_IDLE);                          /* 窗口临界一帧有在飞 → 续 */
    CHECK(c2x_maybe_settle(&s, 1000 + C2X_BATCH_SETTLE_IDLE, &out) == 0);     /* 续了 → 本帧不结（idle 0）*/
    CHECK(s.batch_open == 1 && s.batch_count == 21);
    presence_n(&s, 1, 1040);                                                  /* 空洞 40 帧：中途续一次 */
    CHECK(s.batch_open == 1 && s.batch_count == 21);
    presence_n(&s, 1, 1100);
    CHECK(c2x_maybe_settle(&s, 1100 + C2X_BATCH_SETTLE_IDLE - 1, &out) == 0); /* idle 满−1（presence 之后）*/
    CHECK(c2x_maybe_settle(&s, 1100 + C2X_BATCH_SETTLE_IDLE, &out) == 1);     /* idle 满 → ×0.5，count 21 */
    CHECK(out.mult_permille == C2X_TABLE_M1 && s.batch_open == 0 && s.batch_count == 0);

    /* presence 不计数：续批只顶 idle，count/score 不被污染 */
    memset(&s, 0, sizeof s);
    collect_n(&s, 1, 3, 1000); s.batch_score = 30;
    presence_n(&s, 3, 1001);
    CHECK(s.batch_count == 3 && s.batch_score == 30 && s.batch_open == 1);

    /* §Z20（run-6 拆分症状）模型化：第 1 波 21 枚 f=1000 收完，空洞 20 帧期间第 2 波 21 枚
     * 仍在吸附中（swoosh/真空在飞 = presence 续批）→ f=1020 落定 → 并入同一批（count 42 → ×1.2）；
     * 同一空洞若没有在飞（收点流真停，残余 = 落地休止的场道具）→ idle 满 8 即切批（TH14 原义） */
    memset(&s, 0, sizeof s);
    collect_n(&s, 1, 21, 1000);
    presence_n(&s, 19, 1001);                                   /* 空洞 20 帧：1001–1019 有在飞 */
    collect_n(&s, 1, 21, 1000 + 20);
    CHECK(s.batch_open == 1 && s.batch_count == 42);
    CHECK(c2x_maybe_settle(&s, 1000 + 20 + C2X_BATCH_SETTLE_IDLE, &out) == 1);
    CHECK(out.mult_permille == C2X_TABLE_M3 && out.fragment_type == C2X_FRAG_BOMB);
    CHECK(s.batch_open == 0 && s.batch_count == 0);

    memset(&s, 0, sizeof s);
    collect_n(&s, 1, 21, 1000);                                 /* 波 1；空洞期无在飞（流真停）*/
    CHECK(c2x_maybe_settle(&s, 1000 + C2X_BATCH_SETTLE_IDLE, &out) == 1);
    CHECK(out.mult_permille == C2X_TABLE_M1 && s.batch_open == 0);             /* 8 帧后先关 21 枚 */
    collect_n(&s, 1, 21, 1000 + 20);                            /* 波 2 是下一批 */
    CHECK(c2x_maybe_settle(&s, 1000 + 20 + C2X_BATCH_SETTLE_IDLE, &out) == 1);
    CHECK(out.mult_permille == C2X_TABLE_M1 && s.batch_open == 0);

    /* idle 窗口合并（纯 timer 语义仍保留）：收点间隔 < 窗口 = 同一批；窗口帧后无新收才闭 */
    memset(&s, 0, sizeof s);
    collect_n(&s, 1, 5, 1000); collect_n(&s, 1, 3, 1005);        /* 第二批在 idle 5 时收 → 并入 */
    CHECK(c2x_maybe_settle(&s, 1006, &out) == 0);                /* idle 1（相对 1005）*/
    CHECK(c2x_maybe_settle(&s, 1005 + C2X_BATCH_SETTLE_IDLE, &out) == 1);   /* idle 满 → 关，count 8（无奖）*/
    CHECK(s.batch_open == 0 && s.batch_count == 0);

    /* 迟来残余 = 拆下一批：批闭后再收的孤点开新批，自己走完 idle 静默关（无奖、out 清零）。
     * 残余 = 批闭判定后仍落地休止、不在吸附中的场道具（在飞的会被 presence 续住，到不了这里） */
    memset(&s, 0, sizeof s);
    collect_n(&s, 1, 6, 1000);
    CHECK(c2x_maybe_settle(&s, 1000 + C2X_BATCH_SETTLE_IDLE, &out) == 1);    /* 先关 6 枚批 */
    collect_n(&s, 1, 1, 1000 + C2X_BATCH_SETTLE_IDLE + 5);                  /* 残余点在批闭 5 帧后才到 */
    CHECK(s.batch_open == 1 && s.batch_count == 1);
    CHECK(c2x_maybe_settle(&s, 1000 + C2X_BATCH_SETTLE_IDLE + 5 + C2X_BATCH_SETTLE_IDLE, &out) == 1);
    CHECK(out.mult_permille == 0 && s.batch_open == 0 && s.batch_count == 0);

    /* 五档倍率 + batch_score 快照与清零 */
    const int32_t tiers[][2] = {
        {C2X_TABLE_T1, C2X_TABLE_M1}, {C2X_TABLE_T2, C2X_TABLE_M2},
        {C2X_TABLE_T3, C2X_TABLE_M3}, {C2X_TABLE_T4, C2X_TABLE_M4},
        {C2X_TABLE_T5, C2X_TABLE_M5},
    };
    for (unsigned i = 0; i < sizeof tiers / sizeof tiers[0]; ++i) {
        memset(&s, 0, sizeof s);
        collect_n(&s, 1, tiers[i][0], 1000);
        s.batch_score = 12345;
        CHECK(c2x_maybe_settle(&s, 1000 + C2X_BATCH_SETTLE_IDLE, &out) == 1);
        CHECK(out.mult_permille == tiers[i][1]);
        CHECK(out.batch_score == 12345 && s.batch_score == 0 && s.batch_count == 0 && s.batch_open == 0);
    }
    /* 档内任意值同档：29/39/49/59/65 */
    memset(&s, 0, sizeof s); collect_n(&s, 1, 29, 1000);
    CHECK(c2x_maybe_settle(&s, 1000 + C2X_BATCH_SETTLE_IDLE, &out) == 1 && out.mult_permille == C2X_TABLE_M1);
    memset(&s, 0, sizeof s); collect_n(&s, 1, 39, 1000);
    CHECK(c2x_maybe_settle(&s, 1000 + C2X_BATCH_SETTLE_IDLE, &out) == 1 && out.mult_permille == C2X_TABLE_M2);
    memset(&s, 0, sizeof s); collect_n(&s, 1, 49, 1000);
    CHECK(c2x_maybe_settle(&s, 1000 + C2X_BATCH_SETTLE_IDLE, &out) == 1 && out.mult_permille == C2X_TABLE_M3);
    memset(&s, 0, sizeof s); collect_n(&s, 1, 59, 1000);
    CHECK(c2x_maybe_settle(&s, 1000 + C2X_BATCH_SETTLE_IDLE, &out) == 1 && out.mult_permille == C2X_TABLE_M4);
    memset(&s, 0, sizeof s); collect_n(&s, 1, 65, 1000);
    CHECK(c2x_maybe_settle(&s, 1000 + C2X_BATCH_SETTLE_IDLE, &out) == 1 && out.mult_permille == C2X_TABLE_M5);

    /* cycle 4:1：4 次符卡碎 → 第 5 次命碎归零，之后从头再来；每次恒 1 枚 */
    memset(&s, 0, sizeof s);
    const int32_t want[5] = { C2X_FRAG_BOMB, C2X_FRAG_BOMB, C2X_FRAG_BOMB, C2X_FRAG_BOMB, C2X_FRAG_LIFE };
    for (int i = 0; i < 5; ++i) {
        collect_n(&s, 1, 20, 1000);
        CHECK(c2x_maybe_settle(&s, 1000 + C2X_BATCH_SETTLE_IDLE, &out) == 1);
        CHECK(out.fragment_type == want[i] && out.fragment_count == 1);
    }
    CHECK(s.fragment_cycle == 0);
    collect_n(&s, 1, 20, 1000);
    CHECK(c2x_maybe_settle(&s, 1000 + C2X_BATCH_SETTLE_IDLE, &out) == 1 && out.fragment_type == C2X_FRAG_BOMB && s.fragment_cycle == 1);

    /* ≥60：命碎、不入循环游标（人为把游标推到中间也纹丝不动） */
    memset(&s, 0, sizeof s); s.fragment_cycle = 2;
    collect_n(&s, 2, 60, 1000);
    CHECK(c2x_maybe_settle(&s, 1000 + C2X_BATCH_SETTLE_IDLE, &out) == 1 && out.fragment_type == C2X_FRAG_LIFE && out.fragment_count == 1);
    CHECK(s.fragment_cycle == 2);
    collect_n(&s, 2, 60, 1000);
    CHECK(c2x_maybe_settle(&s, 1000 + C2X_BATCH_SETTLE_IDLE, &out) == 1 && out.fragment_type == C2X_FRAG_LIFE && s.fragment_cycle == 2);

    /* 批集只认 {1,2}（§Z19：TH14「红点与蓝点」→ TH18 P/钱；9–0xe 分数转换点族 = 强制收集产物不入）：
     * 碎片 4/6、卡 0x10–0x13、转换点 9–0xe 及其它 type 一律不入、不开批 */
    memset(&s, 0, sizeof s);
    CHECK(c2x_collect_batch(&s, 4, 0) == 0 && c2x_collect_batch(&s, 6, 0) == 0);
    CHECK(c2x_collect_batch(&s, 3, 0) == 0 && c2x_collect_batch(&s, 5, 0) == 0 && c2x_collect_batch(&s, 7, 0) == 0);
    CHECK(c2x_collect_batch(&s, 8, 0) == 0 && c2x_collect_batch(&s, 0xf, 0) == 0 && c2x_collect_batch(&s, 0x10, 0) == 0);
    CHECK(c2x_collect_batch(&s, 0x11, 0) == 0 && c2x_collect_batch(&s, 0x12, 0) == 0 && c2x_collect_batch(&s, 0x13, 0) == 0);
    CHECK(c2x_collect_batch(&s, 9, 0) == 0 && c2x_collect_batch(&s, 0xa, 0) == 0 && c2x_collect_batch(&s, 0xe, 0) == 0);
    CHECK(c2x_collect_batch(&s, 0, 0) == 0 && c2x_collect_batch(&s, 0xffff, 0) == 0);
    CHECK(s.batch_open == 0 && s.batch_count == 0);
    /* 混收：非批集混在批集里也不计数 —— 9–0xe 转换点贴着 P/钱被吃也不进批（强制收集场景）*/
    collect_n(&s, 1, 1, 0); CHECK(c2x_collect_batch(&s, 4, 0) == 0 && c2x_collect_batch(&s, 6, 0) == 0);
    collect_n(&s, 2, 1, 0); CHECK(c2x_collect_batch(&s, 9, 0) == 0);
    CHECK(c2x_collect_batch(&s, 0xa, 0) == 0 && c2x_collect_batch(&s, 0xe, 0) == 0);
    CHECK(s.batch_open == 1 && s.batch_count == 2);

    /* 掉 P 节流：power 0 → 96 帧一发 */
    memset(&s, 0, sizeof s);
    CHECK(c2x_maybe_drop(&s, 0, 0) == 1 && s.next_drop_at == 96);
    CHECK(c2x_maybe_drop(&s, 95, 0) == 0 && s.next_drop_at == 96);
    CHECK(c2x_maybe_drop(&s, 96, 0) == 1 && s.next_drop_at == 192);
    /* power 4.00 → 32 帧一发 */
    memset(&s, 0, sizeof s);
    CHECK(c2x_maybe_drop(&s, 0, 400) == 1 && s.next_drop_at == 32);
    CHECK(c2x_maybe_drop(&s, 31, 400) == 0);
    CHECK(c2x_maybe_drop(&s, 32, 400) == 1 && s.next_drop_at == 64);

    /* 间隔 = 96 − power×100×16/100：单调降 + 截断 + 钳 1 */
    CHECK(c2x_drop_interval(0) == 96 && c2x_drop_interval(100) == 80);
    CHECK(c2x_drop_interval(200) == 64 && c2x_drop_interval(300) == 48 && c2x_drop_interval(400) == 32);
    CHECK(c2x_drop_interval(50) == 88);
    CHECK(c2x_drop_interval(700) == 1 && c2x_drop_interval(10000) == 1);
    CHECK(c2x_drop_interval(0) > c2x_drop_interval(100)
          && c2x_drop_interval(100) > c2x_drop_interval(200)
          && c2x_drop_interval(200) > c2x_drop_interval(300)
          && c2x_drop_interval(300) > c2x_drop_interval(400));

    /* ==================== ★★ 2026-09-15（`NEXT.md` §1x.2 成员通道）：自建实体入批 ====================
     * 110/111 的大 / 中绿点是自建 VM（`cards/faith.c` 的池），成员门（type{1,2}+state 3）进不来
     * ⇒ 走 `c2x_collect_virtual()`：**只加 count、不碰 batch_score**（用户口径「只 +1」，绿点走信仰、
     * 不进 SCORE ⇒ 也不进批分）。接线级的合成（层 + core 对跑）在 `tests/test_vitem_batch.c`。*/
    memset(&s, 0, sizeof s);
    CHECK(c2x_collect_virtual(&s, 0, 1000) == 0);            /* n <= 0：不做事、不开批 */
    CHECK(c2x_collect_virtual(NULL, 3, 1000) == 0);          /* 空状态指针：不崩 */
    CHECK(s.batch_open == 0 && s.batch_count == 0 && s.batch_score == 0);
    CHECK(c2x_collect_virtual(&s, 1, 1000) == 1);
    CHECK(s.batch_open == 1 && s.batch_count == 1 && s.last_event_at == 1000);
    CHECK(s.batch_score == 0);                               /* ★ 不碰批分 */
    CHECK(c2x_collect_virtual(&s, 24, 1002) == 1);           /* 同一批里继续累加，并续 idle */
    CHECK(s.batch_count == 25 && s.last_event_at == 1002 && s.batch_score == 0);
    /* 纯绿点批（25 颗）到 20 档 ⇒ ×0.5；**批分 0 ⇒ 补差 0**（档位 / 碎片逻辑一字未改）*/
    CHECK(c2x_maybe_settle(&s, 1002 + C2X_BATCH_SETTLE_IDLE - 1, &out) == 0);
    CHECK(c2x_maybe_settle(&s, 1002 + C2X_BATCH_SETTLE_IDLE, &out) == 1);
    CHECK(out.mult_permille == C2X_TABLE_M1 && out.batch_score == 0);
    CHECK(out.batch_score * (int64_t)(out.mult_permille - 1000) / 1000 == 0);   /* 补差 = 0 */
    CHECK(out.fragment_type == C2X_FRAG_BOMB && out.fragment_count == 1);
    CHECK(s.batch_open == 0 && s.batch_count == 0 && s.batch_score == 0);
    /* 绿点自己**能开批**（第一颗就开）—— 接线层把它放在 `batch_open` 门之前 */
    memset(&s, 0, sizeof s);
    CHECK(c2x_maybe_settle(&s, 5000, &out) == 0);            /* 空批不动 */
    CHECK(c2x_collect_virtual(&s, 3, 5000) == 1 && s.batch_open == 1 && s.batch_count == 3);
    /* 混批：原生（P/钱）与自建实体并进同一批 ⇒ count 相加，**批分只含原生的那些小分** */
    memset(&s, 0, sizeof s);
    collect_n(&s, 1, 10, 1000);
    s.batch_score = 700;                                     /* 原生 10 枚的小分（stash 差量累加）*/
    CHECK(c2x_collect_virtual(&s, 15, 1005) == 1);
    CHECK(s.batch_count == 25 && s.batch_score == 700);      /* 只加 count：绿点不进批分 */
    CHECK(c2x_maybe_settle(&s, 1005 + C2X_BATCH_SETTLE_IDLE, &out) == 1);
    CHECK(out.mult_permille == C2X_TABLE_M1 && out.batch_score == 700);
    CHECK(out.batch_score * (int64_t)(out.mult_permille - 1000) / 1000 == -350);  /* 补差只按原生小分算 */
    /* 自建实体与 presence 同一条 idle 门：收完绿点后（生产者不再声明在飞）idle 满才结 */
    memset(&s, 0, sizeof s);
    CHECK(c2x_collect_virtual(&s, 4, 2000) == 1);
    presence_n(&s, 1, 2000 + C2X_BATCH_SETTLE_IDLE);         /* 绿点还在飞 ⇒ 续 idle */
    CHECK(c2x_maybe_settle(&s, 2000 + C2X_BATCH_SETTLE_IDLE, &out) == 0);
    CHECK(s.batch_open == 1 && s.batch_count == 4);
    CHECK(c2x_maybe_settle(&s, 2000 + C2X_BATCH_SETTLE_IDLE + C2X_BATCH_SETTLE_IDLE - 1, &out) == 0);
    CHECK(c2x_maybe_settle(&s, 2000 + C2X_BATCH_SETTLE_IDLE + C2X_BATCH_SETTLE_IDLE, &out) == 1);
    CHECK(out.mult_permille == 0 && out.batch_score == 0);   /* 4 颗 < 20：关批无奖（绿点不改档位门）*/

    printf("test_collect2x_core: %d passed, %d failed\n", s_pass, s_fail);
    return s_fail ? 1 : 0;
}
