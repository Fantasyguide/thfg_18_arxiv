/* 「种下去的植物」（向日葵 84 / 金盏花 85）共用纯逻辑的单测：种下 / 挡弹扣血 / 死亡 / 产出节拍 / 扇形角度。
 *   cc -std=c11 -Wall -Wextra -Werror -o t tests/test_plant_core.c cards/plant_core.c && ./t */
#include <stdio.h>

#include "../cards/plant_core.h"

#define SF_HP     75      /* 向日葵 / 金盏花当前的血量与周期，改卡里的数值时这里跟着改（9b623ff1：150 → 75） */
#define SF_PERIOD 300
#define SF_ITEMS  25

static int fails;
#define CHECK(cond, ...) do { if (!(cond)) { printf("FAIL %s:%d ", __FILE__, __LINE__); \
    printf(__VA_ARGS__); printf("\n"); fails++; } } while (0)

static void test_plant(void)
{
    plant_state_t s = { 1, 1, 2, 3, 4, 5, 6, 9.0f, 9.0f, 9.0f, 7, 8, 9 };
    plant_sow(&s, SF_HP, SF_PERIOD, 10.0f, 300.0f, 0.5f);
    CHECK(s.hp == SF_HP, "血量应为 %d，实为 %d", SF_HP, s.hp);
    CHECK(s.timer == (uint32_t)SF_PERIOD, "首轮计时应为 %d，实为 %u", SF_PERIOD, s.timer);
    CHECK(s.frames == 0 && s.blocked == 0 && s.harvests == 0, "计数器应清零");
    CHECK(s.x == 10.0f && s.y == 300.0f && s.z == 0.5f, "坐标应取自机当下位置");
    CHECK(s.anm_id == 0 && s.bar_id == 0 && s.bar_bg_id == 0, "VM id 应清零（由引擎侧重新填）");
}

/* 每存活 SF_PERIOD 帧产一轮，产完重新计时 */
static void test_harvest_cadence(void)
{
    plant_state_t s;
    plant_sow(&s, SF_HP, SF_PERIOD, 0.0f, 300.0f, 0.0f);
    int produced = 0;
    for (int i = 1; i <= SF_PERIOD * 3; ++i) {
        plant_step_t st = plant_step(&s, 0);
        if (st.produce) {
            produced++;
            CHECK(i % SF_PERIOD == 0, "第 %d 帧不该产出（应当只在 %d 的整数倍）", i, SF_PERIOD);
        }
    }
    CHECK(produced == 3, "3 个周期应产 3 轮，实为 %d", produced);
    CHECK(s.harvests == 3, "harvests 应为 3，实为 %u", s.harvests);
    CHECK(s.frames == (uint32_t)SF_PERIOD * 3, "帧数应为 %d，实为 %u", SF_PERIOD * 3, s.frames);
}

/* 挡弹逐发扣血；血尽当帧 died，且**不再产出** */
static void test_block_and_die(void)
{
    plant_state_t s;
    plant_sow(&s, SF_HP, SF_PERIOD, 0.0f, 300.0f, 0.0f);
    plant_step_t st = plant_step(&s, 10);
    CHECK(s.hp == SF_HP - 10, "挡 10 发应扣 10 血，实为 %d", s.hp);
    CHECK(s.blocked == 10, "累计挡弹应为 10，实为 %u", s.blocked);
    CHECK(!st.died, "还没死");

    st = plant_step(&s, SF_HP);                 /* 一口气打光 */
    CHECK(st.died, "血尽当帧应报 died");
    CHECK(s.hp == 0, "血量应钳到 0，实为 %d", s.hp);
    CHECK(!st.produce, "死的那帧不产出");
    st = plant_step(&s, 0);
    CHECK(st.died && !st.produce, "死了之后每帧仍报 died、永不产出");
    CHECK(s.frames == 1, "死后不再计帧（实为 %u）", s.frames);
}

/* 死亡与产出撞在同一帧：先死，不产 */
static void test_death_beats_harvest(void)
{
    plant_state_t s;
    plant_sow(&s, SF_HP, SF_PERIOD, 0.0f, 300.0f, 0.0f);
    for (int i = 1; i < SF_PERIOD; ++i) plant_step(&s, 0);      /* 差一帧就产 */
    CHECK(s.timer == 1, "此刻应当只差一帧，实为 %u", s.timer);
    plant_step_t st = plant_step(&s, SF_HP + 100);                 /* 同一帧被打死 */
    CHECK(st.died && !st.produce, "同帧死亡应当不产出");
    CHECK(s.harvests == 0, "不该记这一轮");
}

/* 扇形角度：中心 −π/2、两端 ±PLANT_ITEM_SPREAD、单调、对称 */
static void test_item_angles(void)
{
    const int n = SF_ITEMS;
    float first = plant_item_angle_at(0, n), last = plant_item_angle_at(n - 1, n);
    CHECK(first < PLANT_ITEM_UP && last > PLANT_ITEM_UP, "首尾应分列正上方两侧（%.3f / %.3f）", (double)first, (double)last);
    float d0 = PLANT_ITEM_UP - first, d1 = last - PLANT_ITEM_UP;
    CHECK(d0 > d1 - 0.001f && d0 < d1 + 0.001f, "首尾应对称（%.3f vs %.3f）", (double)d0, (double)d1);
    CHECK(d0 > PLANT_ITEM_SPREAD - 0.001f && d0 < PLANT_ITEM_SPREAD + 0.001f,
          "半角应为 %.3f，实为 %.3f", (double)PLANT_ITEM_SPREAD, (double)d0);
    float mid = plant_item_angle_at(n / 2, n);
    CHECK(mid > PLANT_ITEM_UP - 0.001f && mid < PLANT_ITEM_UP + 0.001f, "奇数个时中间那个应正上方（实为 %.3f）", (double)mid);
    for (int i = 1; i < n; ++i)
        CHECK(plant_item_angle_at(i, n) > plant_item_angle_at(i - 1, n), "角度应严格递增（i = %d）", i);
    CHECK(plant_item_angle_at(0, 1) == PLANT_ITEM_UP, "只有一个时就是正上方");
    CHECK(plant_item_angle_at(-5, n) == first && plant_item_angle_at(999, n) == last, "下标越界应钳住，不该算出野值");
}

int main(void)
{
    test_plant();
    test_harvest_cadence();
    test_block_and_die();
    test_death_beats_harvest();
    test_item_angles();
    printf("test_plant_core: %s\n", fails ? "FAIL" : "OK");
    return fails ? 1 : 0;
}
