/* test_give12_core.c —— 118「挑战・十二合一」发放计划的主机单测（`make test-host`）。
 *
 * 语义来源 = `cards/give12_core.h` 的头注释 ✓（一手：`AbilityManager__allocate_new_card` `0x411460`
 * **不查 `owned[]`** ⇒ 已持有的卡再发就是真的多一个卡对象 ✗ —— owner 2026-10-19 实跑报告）。
 *
 * 这里钉住四件事：
 *   ① 区间 = 106..117 **十二张全发**（含 109 —— owner 2026-10-19：「继续开发 109」⇒ 那个
 *      「109 本版摘除」的跳过已删 ✗）；
 *   ② `owned[id] != 0` ⇒ **不发**（2026-10-19 的修法本身）；
 *   ③ 顺序 = id 升序、`cap` 截断、`owned == NULL` 不挡；
 *   ④ 越界下标（`id >= owned_len` / 区间外）不读内存、当未持有。*/
#include <stdio.h>
#include <string.h>

#include "../cards/give12_core.h"

static int s_fail, s_pass;
#define CHECK(cond) do { if (cond) ++s_pass; else { ++s_fail; printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } } while (0)

static int32_t owned[CE_GIVE12_OWNED_LEN];
static uint32_t plan[CE_GIVE12_PLAN_MAX + 4];

static int plan_has(const uint32_t *p, unsigned n, uint32_t id)
{
    unsigned i;
    for (i = 0; i < n; ++i) if (p[i] == id) return 1;
    return 0;
}

int main(void)
{
    unsigned n, i;

    /* ① 一张都没有 ⇒ 发 12 张（106..117 全发，含 109），升序 */
    memset(owned, 0, sizeof owned);
    n = ce_give12_plan(owned, CE_GIVE12_OWNED_LEN, plan, CE_GIVE12_PLAN_MAX);
    CHECK(n == 12);
    CHECK(plan[0] == 106);
    CHECK(plan[1] == 107);
    CHECK(plan[2] == 108);
    CHECK(plan[3] == 109);                    /* 109 恢复发放 ⇒ 第 4 个就是它 ✓ */
    CHECK(plan[n - 1] == 117);
    CHECK(plan_has(plan, n, 109));
    for (i = 0; i < n; ++i) {
        CHECK(plan[i] >= CE_GIVE12_FIRST && plan[i] <= CE_GIVE12_LAST);
        if (i) CHECK(plan[i] > plan[i - 1]);   /* 严格升序、无重复 ✓ */
    }

    /* ② 已经持有 106 ⇒ 只剩 11 张，且 106 不在里面（★ owner 报的那条 bug 的直接回归） */
    memset(owned, 0, sizeof owned);
    owned[106] = 1;
    n = ce_give12_plan(owned, CE_GIVE12_OWNED_LEN, plan, CE_GIVE12_PLAN_MAX);
    CHECK(n == 11);
    CHECK(!plan_has(plan, n, 106));
    CHECK(plan_has(plan, n, 107));

    /* ③ 十二张全都有 ⇒ 一张都不发（2199 那种"全装挑战"再按一次 C 不会翻倍） */
    for (i = CE_GIVE12_FIRST; i <= CE_GIVE12_LAST; ++i) owned[i] = 1;
    n = ce_give12_plan(owned, CE_GIVE12_OWNED_LEN, plan, CE_GIVE12_PLAN_MAX);
    CHECK(n == 0);

    /* ④ 只差 117 ⇒ 只发 117 */
    memset(owned, 0, sizeof owned);
    for (i = CE_GIVE12_FIRST; i <= CE_GIVE12_LAST; ++i) owned[i] = 1;
    owned[117] = 0;
    n = ce_give12_plan(owned, CE_GIVE12_OWNED_LEN, plan, CE_GIVE12_PLAN_MAX);
    CHECK(n == 1);
    CHECK(plan[0] == 117);

    /* ⑤ 逐张谓词：区间外恒不发；109 未持有 ⇒ 该发 ✓ */
    memset(owned, 0, sizeof owned);
    CHECK(ce_give12_should_give(owned, CE_GIVE12_OWNED_LEN, 105) == 0);
    CHECK(ce_give12_should_give(owned, CE_GIVE12_OWNED_LEN, 118) == 0);
    CHECK(ce_give12_should_give(owned, CE_GIVE12_OWNED_LEN, 109) == 1);
    CHECK(ce_give12_should_give(owned, CE_GIVE12_OWNED_LEN, 106) == 1);
    owned[106] = 1;
    CHECK(ce_give12_should_give(owned, CE_GIVE12_OWNED_LEN, 106) == 0);

    /* ⑥ owned == NULL（拿不到管理器）⇒ 不挡：十二张全发（老行为，不能让一张都不发 ✗）*/
    n = ce_give12_plan(NULL, CE_GIVE12_OWNED_LEN, plan, CE_GIVE12_PLAN_MAX);
    CHECK(n == 12);
    CHECK(plan_has(plan, n, 109));

    /* ⑦ cap 截断：cap = 3 ⇒ 只写 3 个，且从 106 起 */
    memset(owned, 0, sizeof owned);
    n = ce_give12_plan(owned, CE_GIVE12_OWNED_LEN, plan, 3);
    CHECK(n == 3);
    CHECK(plan[0] == 106 && plan[1] == 107 && plan[2] == 108);

    /* ⑧ owned_len 越界（模拟"引擎数组比 id 短"）⇒ 不读、当未持有（与 `ce_owned()` 的 `id < 255` 同口径）*/
    memset(owned, 0, sizeof owned);
    n = ce_give12_plan(owned, 110 /* 只有 106..109 在界内 */, plan, CE_GIVE12_PLAN_MAX);
    CHECK(n == 12);                            /* 106..109 界内未持有 ⇒ 发；110..117 界外当未持有 ⇒ 发 */
    CHECK(plan_has(plan, n, 106));
    CHECK(plan_has(plan, n, 109));
    CHECK(plan_has(plan, n, 117));

    /* ⑨ 参数护栏：out == NULL / cap == 0 ⇒ 0 个（不写内存）*/
    CHECK(ce_give12_plan(owned, CE_GIVE12_OWNED_LEN, NULL, CE_GIVE12_PLAN_MAX) == 0);
    CHECK(ce_give12_plan(owned, CE_GIVE12_OWNED_LEN, plan, 0) == 0);

    printf("test_give12_core: %d passed, %d failed\n", s_pass, s_fail);
    return s_fail ? 1 : 0;
}
