/* season_core.c —— 季节卡的纯逻辑实现。表值全部来自 TH16 v1.00a 的 `.rdata`（一手，见 season_core.h 头注）。*/
#include "season_core.h"

/* —— 档位阈值（累计季节点）—— TH16 `SEASON_POWER_LEVEL_REQUIREMENTS` 运行时值 ✅ */
static const int32_t k_req[SEASON_LEVEL_MAX] = { 100, 230, 390, 590, 840, 1140 };

/* —— 每档增量（`SEASON_POWER_LEVEL_DELTAS[1..6]`）—— 夏/土用只扣一档时用 ✅ */
static const int32_t k_delta[SEASON_LEVEL_MAX + 1] = { 0, 100, 130, 160, 200, 250, 300 };

/* —— 释放档位表：7 项，下标 = 档位 0..6（[0] = 0 = 没槽不能放）—— ✅ 一手字节 */
static const int32_t k_dur[SEASON_SUB_COUNT][SEASON_LEVEL_MAX + 1] = {
    /* 春 Spring */ { 0, 10, 13, 16, 20, 20, 20 },
    /* 夏 Summer */ { 0, 30, 30, 30, 30, 30, 30 },
    /* 秋 Fall   */ { 0, 40, 55, 70, 90, 110, 130 },
    /* 冬 Winter */ { 0, 30, 70, 110, 150, 170, 190 },
    /* 土用 Doyou*/ { 0, 40, 40, 40, 40, 40, 40 },
};

static const float k_rad[SEASON_SUB_COUNT][SEASON_LEVEL_MAX + 1] = {
    /* 春 */ { 0.0f, 60.0f, 100.0f, 140.0f, 190.0f, 240.0f, 300.0f },
    /* 夏 */ { 0.0f, 40.0f, 42.0f, 44.0f, 46.0f, 48.0f, 50.0f },
    /* 秋 */ { 0.0f, 60.0f, 60.0f, 60.0f, 60.0f, 60.0f, 60.0f },
    /* 冬 */ { 0.0f, 30.0f, 35.0f, 40.0f, 45.0f, 50.0f, 60.0f },
    /* 土用 */ { 0.0f, 50.0f, 52.0f, 54.0f, 56.0f, 58.0f, 60.0f },
};

/* 每帧伤害：春/夏/土用 100、秋 30、冬 9（`BombSub*__on_tick` 的 create_damage_source 第 4 参）✅ */
static const int32_t k_dmg[SEASON_SUB_COUNT] = { 100, 100, 30, 9, 100 };

/* 无敌帧：春 = 持续 + 60（攻略口径），其余固定（夏 9 / 秋 30 / 冬 30 / 土用 10）*/
#define SEASON_SPRING_TAIL_INVULN 60
static const int32_t k_invuln[SEASON_SUB_COUNT] = { -1, 9, 30, 30, 10 };   /* -1 = 用 duration + 60 */

/* 只给 HUD / 日志用：游戏 ASCII 字体是 ASCII/SJIS，**别在这里放 UTF-8 中文**（会画成乱码）*/
static const char *k_name[SEASON_SUB_COUNT] = { "Spring", "Summer", "Fall", "Winter", "Doyou" };

static int32_t clamp_level(int32_t level)
{
    if (level < 0) return 0;
    if (level > SEASON_LEVEL_MAX) return SEASON_LEVEL_MAX;
    return level;
}

static int32_t clamp_sub(int subseason)
{
    if (subseason < 0) return 0;
    if (subseason >= SEASON_SUB_COUNT) return SEASON_SUB_COUNT - 1;
    return subseason;
}

int32_t season_level_for_points(int32_t points)
{
    int32_t i;
    if (points < 0) points = 0;
    for (i = 0; i < SEASON_LEVEL_MAX; ++i) {
        if (points < k_req[i]) return i;
    }
    return SEASON_LEVEL_MAX;                 /* ≥ 1140 = 满档 */
}

int32_t season_points_for_level(int32_t level)
{
    level = clamp_level(level);
    return (level == 0) ? 0 : k_req[level - 1];
}

void season_profile(int subseason, int level, season_profile_t *out)
{
    int32_t sub;
    if (!out) return;
    sub = clamp_sub(subseason);
    level = clamp_level(level);

    out->duration = k_dur[sub][level];
    out->radius = k_rad[sub][level];
    out->damage = k_dmg[sub];
    out->consume_one_level = (sub == SEASON_SUB_SUMMER || sub == SEASON_SUB_DOYOU) ? 1 : 0;
    out->invuln = (k_invuln[sub] < 0) ? out->duration + SEASON_SPRING_TAIL_INVULN : k_invuln[sub];
}

/* —— 子机阵位（TH16 副 sht 的 option_pos，21 项，相对自机的 px）✅
 * 值直接从 `local/th16.v1.00a/dat/pl0Xsub.sht` 的 +0x40 读（TH16 `th16:0x4446c6` 的读点），
 * 文件按 SUBSEASON 排（`th16:0x492cb8` = {pl00sub, pl02sub, pl03sub, pl01sub, pl04sub}）。
 * 形态：**都在自机身后（y = 26..52 px 下方）**，横向扇开；秋最宽（±48）、冬是一条竖线（x 恒 0）。*/
#define SEASON_OPT_POS_MAX 21
static const season_opt_pos_t k_opt_pos[SEASON_SUB_COUNT][SEASON_OPT_POS_MAX] = {
    /* SPRING */ { { 0.0f, 32.0f }, { -5.0f, 32.0f }, { 5.0f, 32.0f }, { -6.0f, 28.0f }, { 6.0f, 28.0f }, { 0.0f, 32.0f }, { -15.0f, 28.0f }, { 15.0f, 28.0f }, { -5.0f, 32.0f }, { 5.0f, 32.0f }, { -16.0f, 28.0f }, { 16.0f, 28.0f }, { -8.0f, 30.0f }, { 8.0f, 30.0f }, { 0.0f, 32.0f }, { -20.0f, 26.0f }, { 20.0f, 26.0f }, { -12.0f, 30.0f }, { 12.0f, 30.0f }, { -4.0f, 32.0f }, { 4.0f, 32.0f } },
    /* SUMMER */ { { 0.0f, 32.0f }, { 6.0f, 32.0f }, { -6.0f, 32.0f }, { 0.0f, 32.0f }, { 12.0f, 30.0f }, { -12.0f, 30.0f }, { 5.0f, 32.0f }, { -5.0f, 32.0f }, { 15.0f, 30.0f }, { -15.0f, 30.0f }, { 0.0f, 32.0f }, { 10.0f, 30.0f }, { -10.0f, 30.0f }, { 20.0f, 26.0f }, { -20.0f, 26.0f }, { 5.0f, 32.0f }, { -5.0f, 32.0f }, { 15.0f, 30.0f }, { -15.0f, 30.0f }, { 25.0f, 26.0f }, { -25.0f, 26.0f } },
    /* FALL   */ { { 0.0f, 36.0f }, { -36.0f, 36.0f }, { 36.0f, 36.0f }, { 0.0f, 36.0f }, { -36.0f, 32.0f }, { 36.0f, 32.0f }, { -34.0f, 36.0f }, { 34.0f, 36.0f }, { -43.0f, 32.0f }, { 43.0f, 32.0f }, { 0.0f, 36.0f }, { -33.0f, 36.0f }, { 33.0f, 36.0f }, { -45.0f, 32.0f }, { 45.0f, 32.0f }, { -30.0f, 36.0f }, { 30.0f, 36.0f }, { -39.0f, 32.0f }, { 39.0f, 32.0f }, { -48.0f, 28.0f }, { 48.0f, 28.0f } },
    /* WINTER */ { { 0.0f, 32.0f }, { 0.0f, 40.0f }, { 0.0f, 32.0f }, { 0.0f, 48.0f }, { 0.0f, 40.0f }, { 0.0f, 32.0f }, { 0.0f, 44.0f }, { 0.0f, 40.0f }, { 0.0f, 36.0f }, { 0.0f, 32.0f }, { 0.0f, 48.0f }, { 0.0f, 44.0f }, { 0.0f, 40.0f }, { 0.0f, 36.0f }, { 0.0f, 32.0f }, { 0.0f, 52.0f }, { 0.0f, 48.0f }, { 0.0f, 44.0f }, { 0.0f, 40.0f }, { 0.0f, 36.0f }, { 0.0f, 32.0f } },
    /* DOYOU  */ { { 0.0f, 32.0f }, { 5.0f, 32.0f }, { -5.0f, 32.0f }, { 0.0f, 32.0f }, { 6.0f, 28.0f }, { -6.0f, 28.0f }, { 5.0f, 32.0f }, { -5.0f, 32.0f }, { 15.0f, 28.0f }, { -15.0f, 28.0f }, { 0.0f, 32.0f }, { 8.0f, 30.0f }, { -8.0f, 30.0f }, { 16.0f, 28.0f }, { -16.0f, 28.0f }, { 4.0f, 32.0f }, { -4.0f, 32.0f }, { 12.0f, 30.0f }, { -12.0f, 30.0f }, { 20.0f, 26.0f }, { -20.0f, 26.0f } },
};

/* 档位 1..6 的起始下标 = 三角数 L(L-1)/2 */
static const int32_t k_opt_start[SEASON_LEVEL_MAX] = { 0, 1, 3, 6, 10, 15 };

int season_option_pos(int subseason, int level, int i, season_opt_pos_t *out)
{
    int32_t idx;

    if (!out) return 0;
    subseason = clamp_sub(subseason);
    if (level < 1 || level > SEASON_LEVEL_MAX) return 0;
    if (i < 0 || i >= level) return 0;
    idx = k_opt_start[level - 1] + i;
    if (idx >= SEASON_OPT_POS_MAX) return 0;
    *out = k_opt_pos[subseason][idx];
    return 1;
}

const char *season_name(int subseason) { return k_name[clamp_sub(subseason)]; }

/* —— 解放特效的两圈外观 —— 一手：TH16 各副季节 anm 的释放脚本字节。
 * 脚本号（`BombSub*__begin` 里 `AnmLoaded__create_40e5c0` 的实参）：
 *   春 `pl00sub` 3/4、夏 `pl02sub` 3/4、秋 `pl03sub` 3/4、**冬 `pl01sub` 20/21**、土用 `pl04sub` 3/4。
 * 颜色一律 `color(r,g,b)` / `color2(r,g,b)`（外圈减算、内圈加算），自转 = `angleVel(±0.17453292)`。
 * ★★ 2026-09-10 第三十三轮**修了一个把红蓝写反的 bug**：anm 的 `color(r,g,b)` 是 **RGB**
 *   ——一手 `AnmVm__run` `0x479f56`–`0x479f8f`：arg0→`vm+0x526`、arg1→`+0x525`、arg2→`+0x524`，
 *   而 D3DCOLOR 0xAARRGGBB 在内存里是 BB GG RR AA ⇒ **arg0 = 红、arg2 = 蓝**。
 *   原来这个宏写成 `(b<<16)|(g<<8)|r`（BGR）⇒ 春的内圈 (255,128,128) 画成蓝、夏的内圈
 *   (128,192,255) 画成橙红（用户实跑 + 录屏对不上，正是这个）。*/
#define RGB888(r, g, b)  (0xff000000u | ((uint32_t)(r) << 16) | ((uint32_t)(g) << 8) | (uint32_t)(b))
#define SPIN_TH16        0.17453292f     /* `angleVel(0.17453292f, 0, 0.17453292f)` = 10°/帧 */

static const season_ring_t k_ring[SEASON_SUB_COUNT] = {
    /* 春 pl00sub script3/4：外 (128,128,128)→(0,128,0) 灰→绿；内 (255,128,128)→(255,0,0) 粉→红；两圈 32 边形、不自转 */
    { RGB888(128, 128, 128), RGB888(0, 128, 0), RGB888(255, 128, 128), RGB888(255, 0, 0),
      32, 0.0f, 255, 255, 0.0f },
    /* 夏 pl02sub script3/4：外 (255,128,128)→(255,128,0) 粉→橙；内 (128,192,255)→(0,192,255) 蓝→青（12 边、alpha 128）*/
    { RGB888(255, 128, 128), RGB888(255, 128, 0), RGB888(128, 192, 255), RGB888(0, 192, 255),
      12, SPIN_TH16, 255, 128, 0.0f },
    /* 秋 pl03sub script3/4：外 (128,128,255)→(0,128,255) 蓝→青；内 (255,192,128)→(255,192,0) 橙→黄 */
    { RGB888(128, 128, 255), RGB888(0, 128, 255), RGB888(255, 192, 128), RGB888(255, 192, 0),
      12, SPIN_TH16, 255, 128, 0.0f },
    /* 冬 pl01sub script20/21：外 (255,128,128)→(255,128,0) 粉→橙；内 (128,192,255)→(0,192,255) 蓝→青
     *（★ 用户 2026-09-10 实测口径「冬天是蓝色的」= 这圈蓝→青的内圈；原来卡里五季都写死了春的粉色）*/
    { RGB888(255, 128, 128), RGB888(255, 128, 0), RGB888(128, 192, 255), RGB888(0, 192, 255),
      12, SPIN_TH16, 255, 128, 0.0f },
    /* 土用 pl04sub script3/4：同夏/冬配色 + `posTime(60,1,0,48,0)` 上浮 48 px */
    { RGB888(255, 128, 128), RGB888(255, 128, 0), RGB888(128, 192, 255), RGB888(0, 192, 255),
      12, SPIN_TH16, 255, 128, 48.0f },
};

void season_ring(int subseason, season_ring_t *out)
{
    if (!out) return;
    *out = k_ring[clamp_sub(subseason)];
}

int32_t season_points_after_release(int subseason, int32_t points)
{
    int32_t sub = clamp_sub(subseason);
    int32_t level;

    if (points < 0) points = 0;
    if (points > SEASON_POINTS_MAX) points = SEASON_POINTS_MAX;
    level = season_level_for_points(points);
    if (level == 0) return points;                       /* 没档不能放；调用方应已拦 */

    if (sub == SEASON_SUB_SUMMER || sub == SEASON_SUB_DOYOU)
        points -= k_delta[level];                        /* 只扣一档 = 退回上一档阈值 */
    else
        points = 0;                                      /* 春/秋/冬：槽清零 */
    return (points < 0) ? 0 : points;
}

int32_t season_drop_count_decay(int32_t alive_frames, int32_t time_frames,
                                int32_t max_count, int32_t min_count)
{
    int32_t rem;

    if (time_frames < 1) return min_count;               /* T = 0：原版就是「计时走完」那一支 */
    if (max_count < min_count) max_count = min_count;    /* 防呆：N < M 时恒 M */
    if (alive_frames < 0) alive_frames = 0;
    rem = time_frames - alive_frames;                    /* TH16 的计时器是**倒计时** */
    if (rem < 1) return min_count;
    return (max_count - min_count) * rem / time_frames + min_count;
}

int32_t season_drop_native_tier(int32_t native_timed_count)
{
    if (native_timed_count <= 1) return 8;               /* TH16 小妖精 (60,8,1) */
    if (native_timed_count <= 4) return 10;              /* TH16 妖精 (60,10,1) */
    if (native_timed_count <= 10) return 13;             /* TH16 大妖精 (120,13,1) / (120,20,1) */
    return 30;                                           /* TH16 大只 (200,30,1)；道中 boss 的原版 15/20 也落这里 */
}
