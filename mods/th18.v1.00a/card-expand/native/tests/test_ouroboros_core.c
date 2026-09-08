/* 衔尾蛇（id 78）纯逻辑单测：miss 计数 → 层数 → 倍率。
 *   cc -std=c11 -Wall -Wextra -Werror -o t tests/test_ouroboros_core.c cards/ouroboros_core.c && ./t */
#include <stdio.h>

#include "../cards/ouroboros_core.h"

static int fails;
#define CHECK(cond, ...) do { if (!(cond)) { printf("FAIL %s:%d ", __FILE__, __LINE__); \
    printf(__VA_ARGS__); printf("\n"); fails++; } } while (0)

/* 第 N 层要累计 N(N+1)/2 次 miss：1 / 3 / 6 / 10 / 15 … */
static void test_thresholds(void)
{
    ob_state_t s;
    ob_reset(&s);
    CHECK(s.stacks == 0 && s.need == 1 && ob_permille(&s) == OB_BASE_PERMILLE, "初值不对");
    CHECK(ob_remaining(&s) == 1, "开局距第一层应该是 1 次，得到 %u", ob_remaining(&s));

    unsigned expect_at[] = {1, 3, 6, 10, 15, 21, 28, 36, 45};   /* 第 1..9 层各在第几次 miss 达成 */
    unsigned next = 0;
    for (unsigned m = 1; m <= 50; ++m) {
        int up = ob_on_miss(&s);
        int should = (next < 9 && m == expect_at[next]);
        CHECK(up == should, "第 %u 次 miss：升层 %d，期望 %d（当前 %u 层）", m, up, should, s.stacks);
        if (should) next++;
        CHECK(s.stacks == next, "第 %u 次 miss 之后层数 %u，期望 %u", m, s.stacks, next);
        CHECK(s.misses == m, "misses=%u", s.misses);
    }
    CHECK(s.stacks == OB_MAX_STACKS, "50 次 miss 该到顶 %d 层，得到 %u", OB_MAX_STACKS, s.stacks);
}

static void test_permille(void)
{
    ob_state_t s;
    ob_reset(&s);
    CHECK(ob_permille(&s) == 1000, "0 层 = ×1");
    ob_on_miss(&s);                       /* 第 1 次 → 1 层 */
    CHECK(s.stacks == 1 && ob_permille(&s) == 1200, "1 层应为 1200，得到 %d", ob_permille(&s));
    ob_on_miss(&s); ob_on_miss(&s);       /* 第 2、3 次 → 2 层 */
    CHECK(s.stacks == 2 && ob_permille(&s) == 1400, "2 层应为 1400，得到 %d", ob_permille(&s));
    ob_on_miss(&s); ob_on_miss(&s); ob_on_miss(&s);   /* 第 4..6 次 → 3 层 */
    CHECK(s.stacks == 3 && ob_permille(&s) == 1600, "3 层应为 1600，得到 %d", ob_permille(&s));
    /* 加法叠：不是 1.2^3 = 1728 */
    CHECK(ob_permille(&s) != 1728, "叠加算成乘法了");
}

static void test_remaining(void)
{
    ob_state_t s;
    ob_reset(&s);
    ob_on_miss(&s);                        /* 1 层，need = 2 */
    CHECK(ob_remaining(&s) == 2, "距第 2 层应差 2 次，得到 %u", ob_remaining(&s));
    ob_on_miss(&s);
    CHECK(ob_remaining(&s) == 1, "再 miss 一次应差 1 次，得到 %u", ob_remaining(&s));
    ob_on_miss(&s);
    CHECK(s.stacks == 2 && ob_remaining(&s) == 3, "到 2 层后应差 3 次，得到 %u", ob_remaining(&s));
}

/* ce_state 给的是清零的内存：need = 0 是不可能的合法值，第一次 miss 要能自愈 */
static void test_zero_state(void)
{
    ob_state_t s = {0, 0, 0, 0};
    CHECK(ob_remaining(&s) == 1, "零状态下 remaining 应为 1，得到 %u", ob_remaining(&s));
    CHECK(ob_on_miss(&s) == 1, "零状态下第一次 miss 就该升到 1 层");
    CHECK(s.stacks == 1 && s.need == 2 && s.misses == 1, "自愈之后 stacks=%u need=%u misses=%u",
          s.stacks, s.need, s.misses);
}

static void test_reset(void)
{
    ob_state_t s;
    ob_reset(&s);
    for (int i = 0; i < 10; ++i) ob_on_miss(&s);
    CHECK(s.stacks == 4, "10 次 miss 该到 4 层，得到 %u", s.stacks);
    ob_reset(&s);
    CHECK(s.stacks == 0 && s.need == 1 && s.progress == 0 && s.misses == 0 &&
          ob_permille(&s) == OB_BASE_PERMILLE, "ob_reset 没清干净");
}

int main(void)
{
    test_thresholds();
    test_permille();
    test_remaining();
    test_zero_state();
    test_reset();
    printf(fails ? "ouroboros_core: %d FAILED\n" : "ouroboros_core: OK\n", fails);
    return fails != 0;
}
