/* test_truesilver_core.c —— truesilver_core.c 的主机单测（make test-host）。 */
#include <stdio.h>
#include <string.h>
#include "../cards/truesilver_core.h"

static int s_fail, s_pass;
#define CHECK(cond) do { if (cond) ++s_pass; else { ++s_fail; printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } } while (0)

static int drain(ts_state_t *s)            /* 跑到爆发结束，返回总伤害 */
{
    int total = 0, d, n = 0;
    while ((d = ts_tick(s)) != 0 && n++ < 100) total += d;
    return total;
}

int main(void)
{
    ts_state_t s; memset(&s, 0, sizeof s);
    CHECK(!ts_on_bomb(&s));                            /* 还没开场：耐久 0，不触发 */
    ts_stage_start(&s);
    CHECK(s.durability == TS_DURABILITY);
    CHECK(ts_on_bomb(&s) && s.durability == 1);
    CHECK(drain(&s) == TS_BURST_FRAMES * TS_BURST_DMG);   /* 400 */
    CHECK(ts_tick(&s) == 0);
    CHECK(ts_on_bomb(&s) && s.durability == 0);
    CHECK(drain(&s) == 400);
    CHECK(!ts_on_bomb(&s) && s.durability == 0);       /* 第三发：耐久用尽 */
    CHECK(ts_tick(&s) == 0);
    CHECK(s.swings == 2);
    ts_on_bomb(&s);                                    /* 无效 */
    ts_stage_start(&s);                                /* 下一关回满 */
    CHECK(s.durability == 2 && s.burst_left == 0);
    /* 爆发中过关：清掉，不带到下一关 */
    ts_on_bomb(&s); ts_tick(&s);
    ts_stage_start(&s);
    CHECK(ts_tick(&s) == 0 && s.durability == 2);
    printf("test_truesilver_core: %d passed, %d failed\n", s_pass, s_fail);
    return s_fail ? 1 : 0;
}
