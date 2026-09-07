#pragma once

enum {
    CE_ROLLBACK_NOT_NEEDED = 0,
    CE_ROLLBACK_RESTORED = 1,
    CE_ROLLBACK_FAILED = -1,
};

typedef int (*ce_rollback_cb)(void *ctx);
typedef struct { int finished; int result; } ce_failure_guard_t;
#define CE_FAILURE_GUARD_INIT { 0, CE_ROLLBACK_NOT_NEEDED }

int ce_failure_guard_fail(ce_failure_guard_t *guard, ce_rollback_cb rollback, void *ctx);
void ce_failure_guard_success(ce_failure_guard_t *guard);
