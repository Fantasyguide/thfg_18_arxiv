#include <stdio.h>
#include "../sdk_actions.h"

static int s_fail, s_pass, s_marks;
#define CHECK(cond) do { if (cond) ++s_pass; else { ++s_fail; printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } } while (0)

typedef struct { int result; uint32_t marked_id, marked_notify; } fixture_t;

static int allocate_card(void *ctx, uint32_t id, uint32_t mode)
{
    fixture_t *f = ctx;
    CHECK(id == 77 && mode == 2);
    return f->result;
}

static void mark_card(void *ctx, uint32_t id, uint32_t notify)
{
    fixture_t *f = ctx;
    ++s_marks;
    f->marked_id = id;
    f->marked_notify = notify;
}

int main(void)
{
    fixture_t f = { -1, 0, 0 };
    s_marks = 0;
    CHECK(ce_give_card_transaction(77, 2, 9, allocate_card, mark_card, &f) == -1);
    CHECK(s_marks == 0);

    f.result = 4;
    CHECK(ce_give_card_transaction(77, 2, 9, allocate_card, mark_card, &f) == 4);
    CHECK(s_marks == 1 && f.marked_id == 77 && f.marked_notify == 9);
    printf("test_sdk_actions: %d passed, %d failed\n", s_pass, s_fail);
    return s_fail ? 1 : 0;
}
