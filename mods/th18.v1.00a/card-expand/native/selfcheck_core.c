#include "selfcheck_core.h"

int ce_failure_guard_fail(ce_failure_guard_t *guard, ce_rollback_cb rollback, void *ctx)
{
    if (guard->finished) return guard->result;
    guard->finished = 1;
    guard->result = rollback ? rollback(ctx) : CE_ROLLBACK_FAILED;
    return guard->result;
}

void ce_failure_guard_success(ce_failure_guard_t *guard)
{
    guard->finished = 1;
    guard->result = CE_ROLLBACK_NOT_NEEDED;
}
