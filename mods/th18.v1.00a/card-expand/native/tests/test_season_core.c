/* test_season_core.c —— season_core.c 的主机单测（make test-host）。
 * 语义来源：TH16 v1.00a 一手（engine/season/th16/01-season-data-and-tables.md）
 *   档位阈值 100/230/390/590/840/1140；五副季节释放档位表；夏/土用只扣一档、其余清零。
 * 覆盖：阈值边界 / 档位夹取 / 五张表的时长与半径 / 每帧伤害 / 无敌帧 / 消耗规则 / 名字与循环。 */
#include <stdio.h>
#include <string.h>

#include "../cards/season_core.h"

static int s_fail, s_pass;
#define CHECK(cond) do { if (cond) ++s_pass; else { ++s_fail; printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } } while (0)

int main(void)
{
    season_profile_t p;

    /* —— 档位阈值（TH16 requirements[0..5]）—— */
    CHECK(season_level_for_points(0) == 0);
    CHECK(season_level_for_points(99) == 0);
    CHECK(season_level_for_points(100) == 1);
    CHECK(season_level_for_points(229) == 1);
    CHECK(season_level_for_points(230) == 2);
    CHECK(season_level_for_points(389) == 2);
    CHECK(season_level_for_points(390) == 3);
    CHECK(season_level_for_points(589) == 3);
    CHECK(season_level_for_points(590) == 4);
    CHECK(season_level_for_points(839) == 4);
    CHECK(season_level_for_points(840) == 5);
    CHECK(season_level_for_points(1139) == 5);
    CHECK(season_level_for_points(1140) == 6);
    CHECK(season_level_for_points(99999) == 6);
    CHECK(season_level_for_points(-5) == 0);

    CHECK(season_points_for_level(0) == 0);
    CHECK(season_points_for_level(1) == 100);
    CHECK(season_points_for_level(6) == 1140);
    CHECK(season_points_for_level(99) == 1140);      /* 夹取 */

    /* —— 春：半径一路涨到 300、时长 10→20 —— */
    season_profile(SEASON_SUB_SPRING, 1, &p);
    CHECK(p.duration == 10 && p.radius == 60.0f && p.damage == 100 && p.consume_one_level == 0);
    CHECK(p.invuln == 10 + 60);                       /* 春 = 持续 + 60 */
    season_profile(SEASON_SUB_SPRING, 6, &p);
    CHECK(p.duration == 20 && p.radius == 300.0f && p.invuln == 80);
    season_profile(SEASON_SUB_SPRING, 0, &p);
    CHECK(p.duration == 0 && p.radius == 0.0f);       /* 档 0 不能放 */

    /* —— 夏：恒 30 帧、半径 40→50、只扣一档、无敌 9 —— */
    season_profile(SEASON_SUB_SUMMER, 1, &p);
    CHECK(p.duration == 30 && p.radius == 40.0f && p.damage == 100 && p.consume_one_level == 1);
    CHECK(p.invuln == 9);
    season_profile(SEASON_SUB_SUMMER, 6, &p);
    CHECK(p.radius == 50.0f);

    /* —— 秋：半径恒 60、时长 40→130、每帧 30、无敌 30、加速（清零消耗）—— */
    season_profile(SEASON_SUB_FALL, 1, &p);
    CHECK(p.duration == 40 && p.radius == 60.0f && p.damage == 30 && p.invuln == 30);
    CHECK(p.consume_one_level == 0);
    season_profile(SEASON_SUB_FALL, 6, &p);
    CHECK(p.duration == 130 && p.radius == 60.0f);

    /* —— 冬：时长最长 30→190、半径 30→60、每帧 9、无敌 30 —— */
    season_profile(SEASON_SUB_WINTER, 1, &p);
    CHECK(p.duration == 30 && p.radius == 30.0f && p.damage == 9 && p.invuln == 30);
    season_profile(SEASON_SUB_WINTER, 6, &p);
    CHECK(p.duration == 190 && p.radius == 60.0f);

    /* —— 土用：恒 40 帧、半径 50→60、每帧 100、只扣一档、无敌 10 —— */
    season_profile(SEASON_SUB_DOYOU, 1, &p);
    CHECK(p.duration == 40 && p.radius == 50.0f && p.damage == 100 && p.consume_one_level == 1);
    CHECK(p.invuln == 10);
    season_profile(SEASON_SUB_DOYOU, 6, &p);
    CHECK(p.radius == 60.0f);

    /* 越界输入夹取；NULL 不炸 */
    season_profile(99, 99, &p);
    CHECK(p.duration == 40 && p.radius == 60.0f);     /* 夹到土用 6 档 */
    season_profile(-3, -3, &p);
    CHECK(p.duration == 0 && p.radius == 0.0f);       /* 夹到春 0 档 */
    season_profile(0, 0, NULL);

    /* —— 消耗规则 —— */
    /* 春/秋/冬：清零 */
    CHECK(season_points_after_release(SEASON_SUB_SPRING, 100) == 0);
    CHECK(season_points_after_release(SEASON_SUB_SPRING, 1140) == 0);
    CHECK(season_points_after_release(SEASON_SUB_FALL, 500) == 0);
    CHECK(season_points_after_release(SEASON_SUB_WINTER, 999) == 0);
    /* 夏/土用：只扣一档 = 退回上一档阈值 */
    CHECK(season_points_after_release(SEASON_SUB_SUMMER, 100) == 0);
    CHECK(season_points_after_release(SEASON_SUB_SUMMER, 130) == 30);
    CHECK(season_points_after_release(SEASON_SUB_SUMMER, 230) == 100);
    CHECK(season_points_after_release(SEASON_SUB_SUMMER, 390) == 230);
    CHECK(season_points_after_release(SEASON_SUB_SUMMER, 1140) == 840);
    CHECK(season_points_after_release(SEASON_SUB_DOYOU, 1140) == 840);
    CHECK(season_points_after_release(SEASON_SUB_DOYOU, 9999) == 840);   /* 夹到 1140 再扣 */
    /* 没档：原样返回 */
    CHECK(season_points_after_release(SEASON_SUB_SUMMER, 99) == 99);
    CHECK(season_points_after_release(SEASON_SUB_DOYOU, 0) == 0);
    CHECK(season_points_after_release(SEASON_SUB_DOYOU, -7) == 0);

    /* —— 名字与循环 —— */
    CHECK(strcmp(season_name(SEASON_SUB_SPRING), "Spring") == 0);
    CHECK(strcmp(season_name(SEASON_SUB_SUMMER), "Summer") == 0);
    CHECK(strcmp(season_name(SEASON_SUB_FALL), "Fall") == 0);
    CHECK(strcmp(season_name(SEASON_SUB_WINTER), "Winter") == 0);
    CHECK(strcmp(season_name(SEASON_SUB_DOYOU), "Doyou") == 0);
    CHECK(strcmp(season_name(99), "Doyou") == 0);
    CHECK(season_next(SEASON_SUB_SPRING) == SEASON_SUB_SUMMER);
    CHECK(season_next(SEASON_SUB_WINTER) == SEASON_SUB_DOYOU);
    CHECK(season_next(SEASON_SUB_DOYOU) == SEASON_SUB_SPRING);


    /* —— 子机阵位（TH16 副 sht option_pos）—— */
    {
        season_opt_pos_t q;
        /* 档 1：唯一一个在自机正下方 32 px */
        CHECK(season_option_pos(SEASON_SUB_SPRING, 1, 0, &q) && q.x == 0.0f && q.y == 32.0f);
        /* 档 2：两个 —— 起始下标 = 三角数[1] = 1（不是从表头开始，每档一套独立阵型）*/
        CHECK(season_option_pos(SEASON_SUB_SPRING, 2, 0, &q) && q.x == -5.0f && q.y == 32.0f);
        CHECK(season_option_pos(SEASON_SUB_SPRING, 2, 1, &q) && q.x == 5.0f && q.y == 32.0f);
        /* 档 6 的最后一个 = 表项 20 */
        CHECK(season_option_pos(SEASON_SUB_SPRING, 6, 5, &q) && q.x == 4.0f && q.y == 32.0f);
        /* 秋最宽 */
        CHECK(season_option_pos(SEASON_SUB_FALL, 4, 0, &q) && q.x == -34.0f && q.y == 36.0f);
        CHECK(season_option_pos(SEASON_SUB_FALL, 6, 5, &q) && q.x == 48.0f && q.y == 28.0f);
        /* 冬是一条竖线（x 恒 0），且 y 最深到 52 */
        CHECK(season_option_pos(SEASON_SUB_WINTER, 3, 0, &q) && q.x == 0.0f && q.y == 48.0f);
        CHECK(season_option_pos(SEASON_SUB_WINTER, 3, 1, &q) && q.x == 0.0f && q.y == 40.0f);
        CHECK(season_option_pos(SEASON_SUB_WINTER, 6, 0, &q) && q.x == 0.0f && q.y == 52.0f);
        /* 夏 / 土用 */
        CHECK(season_option_pos(SEASON_SUB_SUMMER, 2, 1, &q) && q.x == -6.0f && q.y == 32.0f);
        CHECK(season_option_pos(SEASON_SUB_DOYOU, 3, 2, &q) && q.x == -6.0f && q.y == 28.0f);
        /* 越界一律 0 */
        CHECK(season_option_pos(SEASON_SUB_SPRING, 0, 0, &q) == 0);
        CHECK(season_option_pos(SEASON_SUB_SPRING, 7, 0, &q) == 0);
        CHECK(season_option_pos(SEASON_SUB_SPRING, 3, 3, &q) == 0);      /* i >= level */
        CHECK(season_option_pos(SEASON_SUB_SPRING, 3, -1, &q) == 0);
        CHECK(season_option_pos(SEASON_SUB_SPRING, 2, 0, NULL) == 0);
        CHECK(season_option_pos(99, 1, 0, &q) == 1 && q.x == 0.0f);      /* 副季节夹取 */
    }

    /* —— 解放特效两圈的外观（TH16 各副季节释放脚本字节）—— */
    {
        season_ring_t r;
        /* 春：外灰→绿、内粉→红，两圈 32 边形、不自转、alpha 255/255。
         * ★ 第三十三轮订正：anm 的 `color(r,g,b)` 是 **RGB**（一手 `AnmVm__run` `0x479f56`：
         *   arg0→`vm+0x526`、arg1→`+0x525`、arg2→`+0x524`；D3DCOLOR 0xAARRGGBB 在内存里
         *   是 BB GG RR AA ⇒ **arg0 = 红**）。所以 (255,128,128) 打包成 0xFFFF8080（粉红），
         *   不是 0xFF8080FF（蓝）—— 原测试的期望值就是那个 BGR bug 的化石。*/
        season_ring(SEASON_SUB_SPRING, &r);
        CHECK(r.outer_c1 == 0xff808080u && r.outer_c2 == 0xff008000u);
        CHECK(r.inner_c1 == 0xffff8080u && r.inner_c2 == 0xffff0000u);
        CHECK(r.inner_poly == 32 && r.spin == 0.0f && r.alpha_in == 255 && r.drift_y == 0.0f);
        /* 冬：外粉→橙、内蓝→青、12 边形、自转 10°/帧、内圈 alpha 128 */
        season_ring(SEASON_SUB_WINTER, &r);
        CHECK(r.outer_c1 == 0xffff8080u && r.outer_c2 == 0xffff8000u);
        CHECK(r.inner_c1 == 0xff80c0ffu && r.inner_c2 == 0xff00c0ffu);
        CHECK(r.inner_poly == 12 && r.alpha_in == 128);
        CHECK(r.spin > 0.17f && r.spin < 0.18f);                  /* angleVel 0.17453292 */
        /* 秋：外蓝→青、内橙→黄（与冬正好互换） */
        season_ring(SEASON_SUB_FALL, &r);
        CHECK(r.outer_c1 == 0xff8080ffu && r.outer_c2 == 0xff0080ffu);
        CHECK(r.inner_c1 == 0xffffc080u && r.inner_c2 == 0xffffc000u);
        /* 土用：同夏/冬配色 + 上浮 48 px */
        season_ring(SEASON_SUB_DOYOU, &r);
        CHECK(r.drift_y == 48.0f && r.inner_c1 == 0xff80c0ffu);
        season_ring(SEASON_SUB_SUMMER, &r);
        CHECK(r.drift_y == 0.0f && r.outer_c1 == 0xffff8080u);
        /* 越界夹取 + NULL 不炸 */
        season_ring(99, &r);
        CHECK(r.drift_y == 48.0f);
        season_ring(-1, &r);
        CHECK(r.inner_poly == 32);
        season_ring(SEASON_SUB_WINTER, NULL);
    }

    /* —— 掉落计时衰减（TH16 `dropItems` `th16:0x420585` + `dropSeason` `th16:0x422a26`）——
     * 公式 n = (N−M)×剩余/T + M，剩余 = T − 已活帧数，整数除法（向零截断），剩余 < 1 → M。
     * 小妖精 (T,N,M) = (60,8,1)：出场即杀 8 → 活 30 帧 4 → 活 59 帧 1 → 活 60+ 帧 1。*/
    {
        const int32_t T = 60, N = 8, M = 1;
        CHECK(season_drop_count_decay(0, T, N, M) == 8);      /* 剩余 60 → 7*60/60+1 */
        CHECK(season_drop_count_decay(1, T, N, M) == 7);      /* 剩余 59 → 7*59/60 = 6 → 7 */
        CHECK(season_drop_count_decay(10, T, N, M) == 6);     /* 剩余 50 → 7*50/60 = 5 → 6 */
        CHECK(season_drop_count_decay(30, T, N, M) == 4);     /* 剩余 30 → 7*30/60 = 3 → 4 */
        CHECK(season_drop_count_decay(59, T, N, M) == 1);     /* 剩余 1  → 0 → 1 */
        CHECK(season_drop_count_decay(60, T, N, M) == 1);     /* 剩余 0  → M */
        CHECK(season_drop_count_decay(600, T, N, M) == 1);
        CHECK(season_drop_count_decay(-5, T, N, M) == 8);     /* 负值按 0 算 */
        /* TH16 boss (1200,200,10)：20 秒内击破给 200，拖满只剩 10 */
        CHECK(season_drop_count_decay(0, 1200, 200, 10) == 200);
        CHECK(season_drop_count_decay(600, 1200, 200, 10) == 105);
        CHECK(season_drop_count_decay(1200, 1200, 200, 10) == 10);
        CHECK(season_drop_count_decay(5000, 1200, 200, 10) == 10);
        /* st07 终盘 `(2400,400,400)` = 不衰减 */
        CHECK(season_drop_count_decay(0, 2400, 400, 400) == 400);
        CHECK(season_drop_count_decay(2399, 2400, 400, 400) == 400);
        /* 防呆：T = 0 / N < M 不越界 */
        CHECK(season_drop_count_decay(0, 0, 8, 1) == 1);
        CHECK(season_drop_count_decay(0, 60, 1, 8) == 8);
    }

    /* —— 逐敌人档位：TH18 原生 timed 计数 → TH16 量级（season_core.h 的注释 / engine/season/th18/01 §1b′）——
     * 原生计数来自零售 ECL 的 opcode 573（1..20），映射：1→8 / 2..4→10 / 5..10→13 / ≥11→30。*/
    {
        CHECK(season_drop_native_tier(0) == 8);          /* 没配 → 小妖精档 */
        CHECK(season_drop_native_tier(1) == 8);          /* Cir00/GirlA0x 的 `unknown573(2,1)` */
        CHECK(season_drop_native_tier(2) == 10);
        CHECK(season_drop_native_tier(4) == 10);         /* GirlB01 st01 `(2,4)` */
        CHECK(season_drop_native_tier(5) == 13);         /* GirlD01 st02 `(2,5)` */
        CHECK(season_drop_native_tier(10) == 13);        /* GirlC01/GirlD01 `(2,10)` */
        CHECK(season_drop_native_tier(15) == 30);        /* 道中 boss st01mbs `(2,15)` */
        CHECK(season_drop_native_tier(20) == 30);        /* 道中 boss st02mbs `(2,20)` */
        CHECK(season_drop_native_tier(999) == 30);       /* 越界夹住 */
        /* 组合：原生 T=120、计数 10 → 出场 120 帧内 13 颗，拖满 1 颗 */
        CHECK(season_drop_count_decay(0, 120, season_drop_native_tier(10), 1) == 13);
        CHECK(season_drop_count_decay(60, 120, season_drop_native_tier(10), 1) == 7);
        CHECK(season_drop_count_decay(120, 120, season_drop_native_tier(10), 1) == 1);
        /* 小怪档：原生 T=40、计数 1 → 出场 40 帧内 8 颗 */
        CHECK(season_drop_count_decay(0, 40, season_drop_native_tier(1), 1) == 8);
        CHECK(season_drop_count_decay(20, 40, season_drop_native_tier(1), 1) == 4);
        CHECK(season_drop_count_decay(40, 40, season_drop_native_tier(1), 1) == 1);
    }

    printf("test_season_core: %d passed, %d failed\n", s_pass, s_fail);
    return s_fail ? 1 : 0;
}
