/* test_double_core.c —— double_core.c 的主机单测（make test-host）。 */
#include <stdio.h>
#include <string.h>
#include "../cards/double_core.h"

static int s_fail, s_pass;
#define CHECK(cond) do { if (cond) ++s_pass; else { ++s_fail; printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } } while (0)

int main(void)
{
    int32_t ex[DBL_DROP_TYPES];
    /* 小怪：dropMain(1)，附加表空 → 主掉落补 1 颗进 ex[0]（引擎已撒 1，共 2）*/
    memset(ex, 0, sizeof ex);
    CHECK(dbl_apply(1, ex) == 0 && ex[0] == 1);
    for (int i = 1; i < DBL_DROP_TYPES; ++i) CHECK(ex[i] == 0);
    /* 主掉落 2（点数）+ dropExtra(1, 3) dropExtra(2, 2)：附加 ×2，主补 1 */
    memset(ex, 0, sizeof ex); ex[0] = 3; ex[1] = 2;
    CHECK(dbl_apply(2, ex) == 1 && ex[0] == 6 && ex[1] == 5);
    /* 无主掉落：只翻附加表 */
    memset(ex, 0, sizeof ex); ex[15] = 1;
    CHECK(dbl_apply(0, ex) == -1 && ex[15] == 2);
    /* 主掉落 19（最后一格）/ 16 */
    memset(ex, 0, sizeof ex);
    CHECK(dbl_apply(19, ex) == 18 && ex[18] == 1);
    CHECK(dbl_apply(16, ex) == 15 && ex[15] == 1 && ex[18] == 2);   /* 再调一次：之前补的也被翻 */
    /* 越界类型不碰表 */
    memset(ex, 0, sizeof ex);
    CHECK(dbl_apply(20, ex) == -1 && dbl_apply(21, ex) == -1 && dbl_apply(-1, ex) == -1);   /* 第 20 格引擎不撒，不补 */
    for (int i = 0; i < DBL_DROP_TYPES; ++i) CHECK(ex[i] == 0);
    /* 负数（不该出现）不翻 */
    memset(ex, 0, sizeof ex); ex[3] = -2;
    dbl_apply(0, ex); CHECK(ex[3] == -2);
    printf("test_double_core: %d passed, %d failed\n", s_pass, s_fail);
    return s_fail ? 1 : 0;
}
