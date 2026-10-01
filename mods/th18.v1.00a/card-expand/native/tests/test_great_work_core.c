/* 伟大之作（id 92）纯逻辑单测：血量 → 金钱枚数（至少 1、至多 GW_MONEY_MAX）、
 * 冷却 = 底价 + 弹幕数·单价 + 血量·单价（单调、钳上下限、整数不溢出）。
 *   cc -std=c11 -Wall -Wextra -Werror -o t tests/test_great_work_core.c cards/great_work_core.c && ./t */
#include <assert.h>
#include <stdio.h>

#include "../cards/great_work_core.h"

static int fails;
#define CHECK(cond, ...) do { if (!(cond)) { printf("FAIL %s:%d ", __FILE__, __LINE__); \
    printf(__VA_ARGS__); printf("\n"); fails++; } } while (0)

static void test_money_for_hp(void)
{
    CHECK(gw_money_for_hp(0) == 1, "0 血也该给 1 枚，得到 %d", gw_money_for_hp(0));
    CHECK(gw_money_for_hp(-500) == 1, "负血也给 1 枚");
    CHECK(gw_money_for_hp(1) == 1, "1 血 = 1 枚");
    CHECK(gw_money_for_hp(GW_MONEY_PER_HP) == 1, "%d 血 = 1 枚", GW_MONEY_PER_HP);
    CHECK(gw_money_for_hp(GW_MONEY_PER_HP * 2) == 2, "两倍血量 = 2 枚");
    CHECK(gw_money_for_hp(GW_MONEY_PER_HP * 3 - 1) == 2, "不到三倍 = 2 枚（向下取整）");
    CHECK(gw_money_for_hp(GW_MONEY_PER_HP * GW_MONEY_MAX) == GW_MONEY_MAX, "正好封顶");
    CHECK(gw_money_for_hp(GW_MONEY_PER_HP * GW_MONEY_MAX * 10) == GW_MONEY_MAX,
          "超了要钳到 %d，得到 %d", GW_MONEY_MAX, gw_money_for_hp(GW_MONEY_PER_HP * GW_MONEY_MAX * 10));
    CHECK(gw_money_for_hp(2000000000) == GW_MONEY_MAX, "极大血量不溢出（钳顶）");
}

static void test_recharge(void)
{
    int32_t a, b;

    CHECK(gw_recharge(0, 0) == GW_MIN, "什么都没转 = 下限，得到 %d", gw_recharge(0, 0));
    CHECK(gw_recharge(-5, -5) == GW_MIN, "负数当 0");

    /* 单调：弹幕多 ⇒ 冷却长；血量多 ⇒ 冷却长 */
    a = gw_recharge(100, 0);
    b = gw_recharge(200, 0);
    CHECK(b > a, "弹幕翻倍该更长（%d vs %d）", a, b);
    CHECK(a == GW_BASE + 100 * GW_PER_BULLET, "弹幕项 = 数量·单价，得到 %d", a);
    a = gw_recharge(0, 100);
    b = gw_recharge(0, 200);
    CHECK(b > a, "血量翻倍该更长");
    CHECK(a == GW_BASE + 100 * GW_PER_HP, "血量项 = 血量·单价，得到 %d", a);

    /* 上限：一屏弹幕 + 一只大 boss 不该超过 GW_MAX */
    CHECK(gw_recharge(2000, 1000000) == GW_MAX, "该被钳到 %d，得到 %d", GW_MAX,
          gw_recharge(2000, 1000000));
    /* 典型一屏：200 颗弹 + 2000 血（该落在区间里、没顶上限）*/
    a = gw_recharge(200, 2000);
    CHECK(a > GW_MIN && a < GW_MAX, "典型值该落在区间里，得到 %d", a);
    CHECK(a == GW_BASE + 200 * GW_PER_BULLET + 2000 * GW_PER_HP, "公式三项都要在（得到 %d）", a);
    /* 顶到上限那一档必须真的更大 */
    CHECK(gw_recharge(2000, 20000) == GW_MAX, "超大转换该顶到上限");

    /* 进度换算单调、钳 0..1000 */
    CHECK(gw_recharge_permille(0) == 0 && gw_recharge_permille(-1) == 0, "0 / 负 = 0");
    CHECK(gw_recharge_permille(GW_MAX) == 1000 && gw_recharge_permille(GW_MAX * 2) == 1000, "封顶 1000");
    CHECK(gw_recharge_permille(GW_MAX / 2) >= 490 && gw_recharge_permille(GW_MAX / 2) <= 510, "半程 ≈500");
}

int main(void)
{
    test_money_for_hp();
    test_recharge();
    printf(fails ? "great_work_core: %d FAILED\n" : "great_work_core: ok\n", fails);
    return fails ? 1 : 0;
}
