/* 冰淇淋（id 90）纯逻辑单测：快照只在正常态采、miss 之后写回「生前火力 + 符卡 +3（钳上限）」、
 * 同一次 miss 只写一遍。语义与一手对位见 `cards/ice_cream_core.h`。
 *   cc -std=c11 -Wall -Wextra -Werror -o t tests/test_ice_cream_core.c cards/ice_cream_core.c && ./t */
#include <assert.h>
#include <stdio.h>

#include "../cards/ice_cream_core.h"

static int fails;
#define CHECK(cond, ...) do { if (!(cond)) { printf("FAIL %s:%d ", __FILE__, __LINE__); \
    printf(__VA_ARGS__); printf("\n"); fails++; } } while (0)

static void test_cache_only_when_alive(void)
{
    ic_state_t s;
    int32_t pw = 0, bm = 0;

    ic_reset(&s);
    ic_note_miss(&s);
    CHECK(!ic_apply_after_miss(&s, 7, &pw, &bm), "没采过快照就不该写");
    CHECK(s.miss == 0, "miss 闩该被消费掉（一次性）");

    ic_reset(&s);
    CHECK(!ic_cache(&s, 0, 300, 2), "非正常态不该采快照");
    CHECK(!s.have, "非正常态却把 have 置上了");
    CHECK(ic_cache(&s, 1, 400, 1), "正常态该采快照");
    CHECK(s.power == 400 && s.bombs == 1 && s.have, "快照值不对");

    /* ★ 关键口径：死亡态里再喂新值**不能**覆盖快照（否则会把被扣过的值当生前值）*/
    CHECK(!ic_cache(&s, 0, 100, 0), "死亡态不该刷新");
    CHECK(s.power == 400 && s.bombs == 1, "快照被死亡态的值覆盖了（%d, %d）", (int)s.power, (int)s.bombs);

    ic_note_miss(&s);
    CHECK(ic_apply_after_miss(&s, 7, &pw, &bm), "真死之后该写回去");
    CHECK(pw == 400, "火力该写回生前的 400，得到 %d", (int)pw);
    CHECK(bm == 1 + IC_BOMB_BONUS, "符卡该是 1+3=4，得到 %d", (int)bm);
}

static void test_bomb_clamp_and_one_shot(void)
{
    ic_state_t s;
    int32_t pw = 0, bm = 0;

    /* 生前满符卡：+3 要钳到 MAX_BOMBS（引擎处处这个口径）*/
    ic_reset(&s);
    ic_cache(&s, 1, 200, 7);
    ic_note_miss(&s);
    CHECK(ic_apply_after_miss(&s, 7, &pw, &bm), "该写");
    CHECK(bm == 7, "7+3 该钳到 7，得到 %d", (int)bm);
    CHECK(pw == 200, "火力 = 200");

    /* 同一次 miss 只写一遍 */
    pw = bm = -1;
    CHECK(!ic_apply_after_miss(&s, 7, &pw, &bm), "同一次 miss 不该写第二遍");
    CHECK(pw == -1 && bm == -1, "第二遍把输出改动了");

    /* 生前 0 张符卡：+3 = 3（正常 miss 也是 3，所以这一档没差，但口径要一致）*/
    ic_reset(&s);
    ic_cache(&s, 1, 100, 0);
    ic_note_miss(&s);
    CHECK(ic_apply_after_miss(&s, 7, &pw, &bm) && bm == 3, "0+3 该是 3，得到 %d", (int)bm);

    /* 上限只有 2（异常配置）：钳到 2 */
    ic_reset(&s);
    ic_cache(&s, 1, 100, 1);
    ic_note_miss(&s);
    CHECK(ic_apply_after_miss(&s, 2, &pw, &bm) && bm == 2, "1+3 钳 2 该是 2，得到 %d", (int)bm);

    /* 输出指针允许为 NULL（诊断路径）*/
    ic_reset(&s);
    ic_cache(&s, 1, 100, 1);
    ic_note_miss(&s);
    CHECK(ic_apply_after_miss(&s, 7, 0, 0), "NULL 输出也该返回 1");
}

int main(void)
{
    test_cache_only_when_alive();
    test_bomb_clamp_and_one_shot();
    printf(fails ? "ice_cream_core: %d FAILED\n" : "ice_cream_core: ok\n", fails);
    return fails ? 1 : 0;
}
