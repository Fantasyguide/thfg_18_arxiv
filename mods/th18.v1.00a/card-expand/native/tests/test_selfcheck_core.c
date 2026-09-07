#include <stdio.h>
#include "../selfcheck_core.h"

static int s_fail, s_pass;
#define CHECK(cond) do { if (cond) ++s_pass; else { ++s_fail; printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } } while (0)

static int rollback(void *ctx)
{
    int *calls = ctx;
    ++*calls;
    return CE_ROLLBACK_RESTORED;
}

int main(void)
{
    ce_failure_guard_t guard = CE_FAILURE_GUARD_INIT;
    int calls = 0;
    CHECK(ce_failure_guard_fail(&guard, rollback, &calls) == CE_ROLLBACK_RESTORED);
    CHECK(calls == 1);
    CHECK(ce_failure_guard_fail(&guard, rollback, &calls) == CE_ROLLBACK_RESTORED);
    CHECK(calls == 1);

    guard = (ce_failure_guard_t)CE_FAILURE_GUARD_INIT;
    ce_failure_guard_success(&guard);
    CHECK(ce_failure_guard_fail(&guard, rollback, &calls) == CE_ROLLBACK_NOT_NEEDED);
    CHECK(calls == 1);
    printf("test_selfcheck_core: %d passed, %d failed\n", s_pass, s_fail);
    return s_fail ? 1 : 0;
}
