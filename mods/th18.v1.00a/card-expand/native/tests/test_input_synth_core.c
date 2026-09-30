/* test_input_synth_core.c —— `cards/input_synth.h` 的主机单测（无需游戏）。
 *
 * 覆盖：
 *   ① 模式序列（默认 N=1,M=1 ⇒ 左 / 松 / 右 / 松）与"含松开帧"这条硬要求；
 *   ② ★ **半周期镜像不变量** `mask(t + P/2) == mirror(mask(t))` —— 这是"整周期净位移恰好 0"的
 *      结构性理由（论证见 input_synth.h 文件头第一条），对**任意** N/M 都必须成立；
 *   ③ 位域卫生：只写四个方向位，绝不碰 Z(0x001)/X(0x002)/Shift(0x008)/C(0x400)/D(0x800)；
 *   ④ 离线位移模型（两种动力学各跑 600 帧）：
 *        A = **一手反汇编出来的 TH18 真模型**（每帧速度由输入直接选表 ⇒ 无惯性，
 *            `Player__sub_45b170` `0x45b5a2`–`0x45b608`）；
 *        B = **反事实**的一阶惯性模型（`v += (target - v) * alpha`）—— 用来证明
 *            "即使有惯性，对称循环在稳态下每周期也恰好归零"（父任务口径里的惯性前提）。
 *
 * 编译（不跑 make，直接用工程同款开关；主机器上 gcc/clang 都行）：
 *   cc -std=c11 -Wall -Wextra -Werror -o /tmp/t_isynth tests/test_input_synth_core.c
 *   /tmp/t_isynth
 */
#include <stdio.h>
#include <string.h>

#include "../cards/input_synth.h"

static int s_fail, s_pass;
#define CHECK(cond) do { if (cond) ++s_pass; else { ++s_fail; printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } } while (0)

/* 左右互换（= 文件头里的 "-u"）；上/下不动（本模式不用，但保持完备）。 */
static uint32_t mirror(uint32_t m)
{
    uint32_t r = m;
    if (m & CE_INPUT_SYNTH_DIR_LEFT)  r = (r & ~CE_INPUT_SYNTH_DIR_LEFT)  | CE_INPUT_SYNTH_DIR_RIGHT;
    if (m & CE_INPUT_SYNTH_DIR_RIGHT) r = (r & ~CE_INPUT_SYNTH_DIR_RIGHT) | CE_INPUT_SYNTH_DIR_LEFT;
    return r;
}

/* ---- 离线位移模型 ----
 * 单位：1/128 px（与引擎的 `player+0x62c` 定点同量纲）。V = 一帧位移：
 *   · 普通自机（`+0x477b4` 由 SHT `+0x10 × 128` 装入）取 4 px/frame ⇒ V = 512
 *   · 原生卡 29「文」超速（`0x40ca81` 写死 `0x400`）⇒ V = 1024
 * `dir` = +1（左 → x 减小，取 -1）/ -1 / 0；为了可读，这里用"模式返回 LEFT 就当 -1"。 */
#define IS_V_NORMAL   512
#define IS_V_AYA      1024

/* A：一手 TH18 模型 —— 每帧 dx 只由**当前**输入决定（无加速度项）。 */
static int32_t modelA_step(int32_t dir, int32_t v)
{
    /* 对应 `edx = dirX*speed; dx = (int)((float)edx * mult)`，mult = 1.0 */
    return dir * v;
}

/* B：反事实一阶惯性 —— v 每帧朝目标速度按 alpha 靠拢（浮点/双精度，主机侧只用于论证）。 */
static double modelB_step(double v, double target, double alpha)
{
    return v + (target - v) * alpha;
}

static char dir_char(uint32_t m)
{
    return (m & CE_INPUT_SYNTH_DIR_LEFT) ? 'L' : (m & CE_INPUT_SYNTH_DIR_RIGHT) ? 'R'
         : (m & CE_INPUT_SYNTH_DIR_UP)   ? 'U' : (m & CE_INPUT_SYNTH_DIR_DOWN)  ? 'D' : '-';
}

int main(void)
{
    const unsigned P  = ce_input_synth_period();
    const unsigned HP = ce_input_synth_half_period();

    /* ---------- ① 默认模式的序列 ---------- */
    printf("pattern default: N=%u M=%u period=%u\n", (unsigned)CE_INPUT_SYNTH_LEFT_FRAMES,
           (unsigned)CE_INPUT_SYNTH_RELEASE_FRAMES, P);
    {
        char buf[64];
        unsigned i, n = 0;
        for (i = 0; i < P; ++i) buf[n++] = dir_char(ce_input_synth_mask(i));
        buf[n] = 0;
        printf("pattern frames : %s\n", buf);
    }
#if (CE_INPUT_SYNTH_LEFT_FRAMES == 1 && CE_INPUT_SYNTH_RIGHT_FRAMES == 1 && CE_INPUT_SYNTH_RELEASE_FRAMES == 1)
    CHECK(P == 4);
    CHECK(ce_input_synth_mask(0) == CE_INPUT_SYNTH_DIR_LEFT);
    CHECK(ce_input_synth_mask(1) == 0);                       /* ★ 松开方向键的帧 */
    CHECK(ce_input_synth_mask(2) == CE_INPUT_SYNTH_DIR_RIGHT);
    CHECK(ce_input_synth_mask(3) == 0);                       /* ★ 松开方向键的帧 */
    CHECK(ce_input_synth_mask(4) == CE_INPUT_SYNTH_DIR_LEFT); /* 周期 */
#endif

    /* 周期里必须**存在**"松开方向键"的帧（用户硬要求）。 */
    {
        unsigned i, rel = 0, l = 0, r = 0;
        for (i = 0; i < P; ++i) {
            uint32_t m = ce_input_synth_mask(i);
            if (m == 0) ++rel;
            if (m & CE_INPUT_SYNTH_DIR_LEFT)  ++l;
            if (m & CE_INPUT_SYNTH_DIR_RIGHT) ++r;
        }
        CHECK(rel > 0);                 /* 有松开帧 */
        CHECK(l > 0 && r > 0);          /* 左右都有 */
        CHECK(l == r);                  /* 左右帧数相等（净零的必要条件之一）*/
        CHECK(rel == 2u * CE_INPUT_SYNTH_RELEASE_FRAMES);
    }

    /* ---------- ② 半周期镜像不变量（对任意 N/M） ---------- */
    {
        unsigned i, bad = 0;
        for (i = 0; i < P; ++i)
            if (ce_input_synth_mask(i + HP) != mirror(ce_input_synth_mask(i))) ++bad;
        CHECK(bad == 0);
    }

    /* ---------- ③ 位域卫生 ---------- */
    {
        const uint32_t forbidden = 0x001u | 0x002u | 0x008u | 0x400u | 0x800u;   /* Z / X / Shift / C / D */
        unsigned i;
        for (i = 0; i < 4u * P + 3u; ++i) {
            uint32_t m = ce_input_synth_mask(i);
            CHECK((m & ~(uint32_t)CE_INPUT_SYNTH_DIR_MASK) == 0);   /* 只有方向位 */
            CHECK((m & forbidden) == 0);                            /* 绝不含 Z/X/Shift/C/D */
        }
    }

    /* ---------- ④ 离线位移：600 帧 ---------- */
    {
        const unsigned FRAMES = 600;
        unsigned i;

        /* A：一手 TH18 模型（无惯性）。两种速度各跑一遍。 */
        const int32_t vs[2] = { IS_V_NORMAL, IS_V_AYA };
        for (int k = 0; k < 2; ++k) {
            int32_t xq = 0, peak = 0, at600 = 0;
            for (i = 0; i < FRAMES; ++i) {
                uint32_t m = ce_input_synth_mask(i);
                int32_t dir = (m & CE_INPUT_SYNTH_DIR_LEFT) ? -1 : (m & CE_INPUT_SYNTH_DIR_RIGHT) ? 1 : 0;
                xq += modelA_step(dir, vs[k]);
                if (xq > peak) peak = xq;
                if (-xq > peak) peak = -xq;
            }
            at600 = xq;
            printf("modelA V=%4d: net dx(600f)=%d/128px (%.2f px), peak |dx|=%d/128px (%.2f px)\n",
                   (int)vs[k], (int)at600, (double)at600 / 128.0, (int)peak, (double)peak / 128.0);
            CHECK(at600 == 0);                                  /* ★ 600 = 整周期倍 ⇒ 精确归零 */
            CHECK(peak <= vs[k]);                               /* 峰值 ≤ 一帧位移（N=1 的最小性）*/
        }

        /* B：反事实一阶惯性（alpha = 0.05 / 0.25）。
         *   命题不是"Δx 恰好 0"（从静止起步时，对称周期解在位置上有一个**一次性的常值偏置**），
         *   而是**不累积**：跑 600 帧与跑 1200 帧的 Δx **完全相等**，且最后一周期的增量 = 0。 */
        {
            const double alphas[2] = { 0.05, 0.25 };
            for (int k = 0; k < 2; ++k) {
                double v = 0.0, x = 0.0, x600 = 0.0, x1196 = 0.0, x1200 = 0.0, peak = 0.0;
                for (i = 0; i < 1200; ++i) {
                    uint32_t m = ce_input_synth_mask(i);
                    double target = (m & CE_INPUT_SYNTH_DIR_LEFT) ? -(double)IS_V_NORMAL
                                  : (m & CE_INPUT_SYNTH_DIR_RIGHT) ? (double)IS_V_NORMAL : 0.0;
                    v = modelB_step(v, target, alphas[k]);
                    x += v;
                    if (i + 1 == 600)  x600  = x;
                    if (i + 1 == 1196) x1196 = x;            /* 1200 − P（P = 4）*/
                    if (i + 1 == 1200) x1200 = x;
                    if (x > peak) peak = x;
                    if (-x > peak) peak = -x;
                }
                const double eps = 1e-6;                     /* 浮点噪声量级（真机是整数定点，不存在这一项）*/
                printf("modelB alpha=%.2f: dx(600f)=%.4f dx(1200f)=%.4f (Δ=%+.3e)  last-full-period Δ=%+.3e /128px, peak |dx|=%.1f/128px (%.2f px)\n",
                       alphas[k], x600, x1200, x1200 - x600, x1200 - x1196, peak, peak / 128.0);
                CHECK(x1200 - x600 < eps && x600 - x1200 < eps);      /* ★ 不累积 */
                CHECK(x1200 - x1196 < eps && x1196 - x1200 < eps);    /* ★ 稳态下整周期净位移 = 0 */
                CHECK(peak / 128.0 < 8.0);                            /* 峰值有界 */
            }
        }
    }

    printf("%s: %d checks passed, %d failed\n", s_fail ? "FAIL" : "OK", s_pass, s_fail);
    return s_fail ? 1 : 0;
}
