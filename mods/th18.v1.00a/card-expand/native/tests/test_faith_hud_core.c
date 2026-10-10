/* test_faith_hud_core.c —— `cards/faith_hud_core.c` 的主机单测（`make test-host` 的口径：
 *   cc -std=c11 -Wall -Wextra -Werror -o tests/build/test_faith_hud_core \
 *      tests/test_faith_hud_core.c cards/faith_hud_core.c cards/signal_core.c \
 *      cards/faith_core.c cards/mof_core.c && ./tests/build/test_faith_hud_core）。
 *
 * ★ 本测试**同时链 `signal_core.c`**：组合 UI 的行 1（信号条宽 / 强度）与 111 自己的 HUD 用的是
 *   **同一个一手式子**（`th11:0x41b6a2` 的档、`th11:0x41b707`–`0x41b711` 的宽、`th11:0x41f640` 的强度）
 *   ⇒ 这里逐值断言两个实现**恒等** —— 谁哪天只改一处，这个测试立刻红（比注释可靠）。
 *   ⚠ 本仓 `NEXT.md` §2A 的「组合 UI」把 111 原来的 3 支 HUD VM 交给共用件驱动，但那几支的**几何**
 *   仍是 TH11 的；这条等式就是「搬过去没搬错」的机器判据。
 *
 * 一手出处（逐条见 `cards/faith_hud_core.h` 的表）：
 *   `engine/player/th10/02-hud-bottom-left.md`（TH10 块：连击条 38 px / 满 130 帧）
 *   `engine/player/th11/03-hud-bottom-left.md`（TH11 块：`%.6d` 截 10 / 条宽 `9+4×档` / 档位闪色四档）
 *   `engine/player/th11/04-merged-hud-spec.md`（合并块的坐标与宽度预算表）
 *   `mods/th18.v1.00a/card-expand/NEXT.md` §2A（用户口径 ①②③④）
 */
#include <stdio.h>
#include <string.h>

#include "../cards/faith_hud_core.h"
#include "../cards/signal_core.h"   /* 对账用：同一个一手式子的另一份实现 */
#include "../cards/mof_core.h"      /* 擦弹倍率 `mof_core_graze_mult()`（组合 UI 行 2 的擦弹项）*/
#include "../cards/hud_dim.h"       /* ★ §H：本块压暗的**共享实现**（2026-09-15h 并进来；换算 320）*/

static int s_fail, s_pass;
#define CHECK(cond) do { if (cond) ++s_pass; else { ++s_fail; printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } } while (0)
#define NEAR_F(a, b) (((a) - (b) < 1e-4f) && ((b) - (a) < 1e-4f))

/* `FAITH_FIXED` 定点 → 显示单位（= 各槽 `delta` 的换算；见 `faith_core.h`）。*/
#define FX(u) ((u) * FAITH_FIXED)

int main(void)
{
    int d[6];
    char txt[5];
    int32_t v;

    /* ================= §A 行 2：Δ_10 + Δ_11（用户 ④：基础不进这块）================= */
    {
        faith_slot_t s10, s11;

        faith_slot_init_110(&s10);
        faith_slot_init_111(&s11, FAITH_DIFF_NORMAL);

        /* 一局开头：两个 Δ 都是 0 ⇒ 行 2 显示 **0**（用户 ④ 明说「可以显示 0」）。*/
        CHECK(faith_hud_maxpoint(s10.delta, s11.delta) == 0);
        faith_hud_digits6(0, d);
        CHECK(d[0] == 0 && d[1] == 0 && d[2] == 0 && d[3] == 0 && d[4] == 0 && d[5] == 0);

        /* ★ 反面钉子：**基础值一个都不许进这块** —— 这就是「用 `faith_slot_total()` 会错」的地方。
         * 110 的基础恒 50000、111 的基础按难度（NORMAL = 50000），两个 total 都是 50000，
         * 而行 2 必须仍然是 0。*/
        CHECK(faith_slot_total(&s10) == FAITH_BASE_110);
        CHECK(faith_slot_total(&s11) == FAITH_BASE_111_NORMAL);
        CHECK(faith_hud_maxpoint(s10.delta, s11.delta) == 0);

        /* 收一颗大信仰点（+5000）+ 一颗大绿点（+100）⇒ 行 2 = 5100 → 截 10 = 5100 → "005100"。*/
        (void)faith_slot_add(&s10, FAITH_EV_BIG, FAITH_MOF_BIG_EN);
        (void)faith_slot_add(&s11, FAITH_EV_BIG, FAITH_SA_BIG);
        CHECK(faith_hud_delta_display(s10.delta, s11.delta) == 5100);
        CHECK(faith_hud_maxpoint(s10.delta, s11.delta) == 5100);
        faith_hud_digits6(5100, d);
        CHECK(d[0] == 0 && d[1] == 0 && d[2] == 5 && d[3] == 1 && d[4] == 0 && d[5] == 0);

        /* 单装 110（111 槽恒 0）/ 单装 111（110 槽恒 0）都只画自己那一份。*/
        CHECK(faith_hud_maxpoint(s10.delta, 0) == 5000);
        CHECK(faith_hud_maxpoint(0, s11.delta) == 100);
    }

    /* ============ §B 10 的倍数截断（**复刻** `th11:0x41ce37` / `0x41ce43`）============ */
    {
        CHECK(faith_hud_trunc10(0) == 0);
        CHECK(faith_hud_trunc10(9) == 0);
        CHECK(faith_hud_trunc10(10) == 10);
        CHECK(faith_hud_trunc10(50012) == 50010);   /* 一手文档里的例子（`"%.6d"` → `050010`）*/
        CHECK(faith_hud_trunc10(50010) == 50010);
        CHECK(faith_hud_trunc10(99999) == 99990);
        CHECK(faith_hud_trunc10(-7) == 0);          /* x86 idiv 向零取整 ⇒ 与 C 的 `/` `%` 同语义 */
        CHECK(faith_hud_trunc10(-15) == -10);
        /* 截断发生在**求和之后**（本块自己的值 = 两份额外之和，照 TH11 对「那一行」的规则处理）。*/
        CHECK(faith_hud_maxpoint(FX(50005), 0) == 50000);   /* 两槽 50005 ⇒ 截 10 */
        CHECK(faith_hud_maxpoint(FX(50003), FX(4)) == 50000);   /* 50007 ⇒ 截 10 */
    }

    /* ============ §C 6 位零填充与上限（**复刻** `%.6d` + TH10 前导零照画）============ */
    {
        faith_hud_digits6(1, d);
        CHECK(d[5] == 1 && d[0] == 0 && d[1] == 0 && d[2] == 0 && d[3] == 0 && d[4] == 0);
        faith_hud_digits6(999990, d);
        CHECK(d[0] == 9 && d[1] == 9 && d[2] == 9 && d[3] == 9 && d[4] == 9 && d[5] == 0);
        /* 6 个固定 VM ⇒ 超 6 位必须钳（❓ 自定，见头注）；钳在 999990（6 位里最大的 10 的倍数）。*/
        CHECK(faith_hud_maxpoint(FX(1000000), FX(1000000)) == FAITH_HUD_MAXPOINT_MAX);
        faith_hud_digits6(faith_hud_maxpoint(FX(1000000), FX(1000000)), d);
        CHECK(d[0] == 9 && d[5] == 0);
        /* 负数（不可能，防御）：按 0。*/
        faith_hud_digits6(-5, d);
        CHECK(d[0] == 0 && d[5] == 0);
        CHECK(faith_hud_delta_display(-FX(10), -FX(10)) == 0);
    }

    /* ============ §D 行 1：档 / 条宽（**复刻**，并与 `signal_core.c` 逐值对账）============ */
    {
        int32_t sig;

        CHECK(faith_hud_signal_pct(0) == 0);
        CHECK(faith_hud_signal_pct(999) == 0);
        CHECK(faith_hud_signal_pct(1000) == 1);
        CHECK(faith_hud_signal_pct(100000) == 100);
        CHECK(faith_hud_signal_pct(FAITH_SIGNAL_MAX) == 130);          /* 硬上限 130.00 */
        CHECK(faith_hud_signal_pct(FAITH_SIGNAL_MAX + 50000) == 130);  /* 越界也钳到 130 */

        CHECK(faith_hud_signal_level(0) == 0);
        CHECK(faith_hud_signal_level(9999) == 0);        /* 9.99 显示 ⇒ 档 0 */
        CHECK(faith_hud_signal_level(10000) == 1);       /* 10.00 ⇒ 档 1 */
        CHECK(faith_hud_signal_level(99000) == 9);       /* 99.xx ⇒ 档 9 */
        CHECK(faith_hud_signal_level(100000) == 10);     /* 100.00 ⇒ 档 10（满）*/
        CHECK(faith_hud_signal_level(130000) == 10);     /* 130.00 仍只到档 10（一手钳 100）*/

        CHECK(faith_hud_signal_width_px(0) == 9);        /* 起点 9 px（首块）*/
        CHECK(faith_hud_signal_width_px(10000) == 13);
        CHECK(faith_hud_signal_width_px(100000) == 49);  /* 满 = 49 px（> 48 会向右溢 1 px，原版如此）*/
        CHECK(faith_hud_signal_width_px(130000) == 49);

        /* ★ 与 111 自己的实现逐值相等（同一手一式的两份实现 —— 搬进共用件不许搬错）。*/
        for (sig = 0; sig <= FAITH_SIGNAL_MAX + 1000; sig += 137) {
            CHECK(faith_hud_signal_level(sig) == signal_core_bar_level(sig));
            CHECK(faith_hud_signal_width_px(sig) == signal_core_bar_width_px(sig));
            CHECK(faith_hud_signal_strength(sig) == signal_core_strength(sig));
        }
        CHECK(faith_hud_signal_strength(0) == 0);
        CHECK(faith_hud_signal_strength(99999) == 990);     /* 99.999 ⇒ 0.99 */
        CHECK(faith_hud_signal_strength(100000) == 1000);   /* 满 ⇒ 1.00（一手 `th11:0x41f64c`）*/
        CHECK(faith_hud_signal_strength(130000) == 1000);

        /* 档位闪色四档（**复刻** `th11:0x41b593`–`0x41b66e`）：分界在**显示值** 100/110/120。*/
        CHECK(faith_hud_signal_flash(0) == 0);
        CHECK(faith_hud_signal_flash(99999) == 0);
        CHECK(faith_hud_signal_flash(100000) == 1);
        CHECK(faith_hud_signal_flash(109999) == 1);
        CHECK(faith_hud_signal_flash(110000) == 2);
        CHECK(faith_hud_signal_flash(119999) == 2);
        CHECK(faith_hud_signal_flash(120000) == 3);
        CHECK(faith_hud_signal_flash(130000) == 3);
    }

    /* ============ §E 行 2 的倍率（擦弹项 + 信号项，**复刻** TH11 一手）============ */
    {
        int32_t graze_step;

        /* 擦弹项来自共用件 `mof_core_graze_mult()`（111 也借它）；一手：每 100 擦弹 +0.01、上限 8.99。*/
        CHECK(mof_core_graze_mult(0) == 0);
        CHECK(mof_core_graze_mult(99) == 0);
        CHECK(mof_core_graze_mult(100) == 10);
        CHECK(mof_core_graze_mult(89900) == 8990);
        CHECK(mof_core_graze_mult(1000000) == 8990);

        /* 两项相加（一手 `th11:0x41ce1f add edx,ebx`）。*/
        CHECK(faith_hud_mult_permille(0, 0) == 0);
        CHECK(faith_hud_mult_permille(0, 1000) == 1000);          /* 擦弹 0 + 满信号 ⇒ 1.00 */
        CHECK(faith_hud_mult_permille(10, 1000) == 1010);         /* 擦弹 100 颗 + 满信号 ⇒ 1.01 */
        CHECK(faith_hud_mult_permille(8990, 1000) == 9990);       /* 上限 9.99 */
        CHECK(faith_hud_mult_permille(8990, 1000 + 5000) == FAITH_HUD_MULT_MAX);
        CHECK(faith_hud_mult_permille(-10, -10) == 0);

        /* 文本 = `"%1.2f"` 的 4 个字符（值域 0.00..9.99 ⇒ 恒 4 字符）。*/
        faith_hud_mult_text(0, txt);
        CHECK(strcmp(txt, "0.00") == 0);
        faith_hud_mult_text(1000, txt);
        CHECK(strcmp(txt, "1.00") == 0);
        faith_hud_mult_text(1234, txt);
        CHECK(strcmp(txt, "1.23") == 0);        /* 截断；两项都是 10 千分点的整数倍 ⇒ 与 %.2f 等价 */
        faith_hud_mult_text(9990, txt);
        CHECK(strcmp(txt, "9.99") == 0);
        faith_hud_mult_text(99999, txt);
        CHECK(strcmp(txt, "9.99") == 0);
        faith_hud_mult_text(-5, txt);
        CHECK(strcmp(txt, "0.00") == 0);
        /* ★ 与「printf 的 %.2f」逐值等价（对**合法值域**内的每个 0.01 步进；第 3 位恒 0 ⇒ 无舍入差）。*/
        for (v = 0; v <= FAITH_HUD_MULT_MAX; v += 10) {
            char ref[8];
            snprintf(ref, sizeof ref, "%d.%02d", (int)(v / 1000), (int)((v % 1000) / 10));
            faith_hud_mult_text(v, txt);
            CHECK(strcmp(txt, ref) == 0);
        }
        graze_step = mof_core_graze_mult(12345);   /* 擦弹 12345 ⇒ 1.23 项 */
        CHECK(graze_step == 1230);
        CHECK(faith_hud_mult_permille(graze_step, 1000) == 2230);
    }

    /* ============ §F 行 3：连击槽宽（❓ 自定：满 75 px，φ 比例仍线性）============ */
    {
        CHECK(faith_hud_combo_width_milli(0) == 0);
        CHECK(faith_hud_combo_width_milli(-1) == 0);
        CHECK(faith_hud_combo_width_milli(FAITH_COMBO_FULL) == FAITH_HUD_BAR_W_MILLI_FULL);
        CHECK(faith_hud_combo_width_milli(FAITH_COMBO_FULL + 10) == FAITH_HUD_BAR_W_MILLI_FULL);
        CHECK(faith_hud_combo_width_milli(65) == 37500);      /* 半槽 ⇒ 37.5 px */
        CHECK(faith_hud_combo_width_milli(1) == 576);         /* 1 帧 ⇒ 0.576 px（线性，同 TH10 的 k×current）*/
        /* 单调不减（进度条的基本不变式）。*/
        for (v = 1; v <= FAITH_COMBO_FULL; ++v)
            CHECK(faith_hud_combo_width_milli(v) >= faith_hud_combo_width_milli(v - 1));
        /* ❓ 自定的那一个数：满槽 = 行 2 的整宽 75 px（6×7 + 1 + 7 + 25）—— 改这里就是改那个旋钮。*/
        CHECK(FAITH_HUD_BAR_W_MILLI_FULL / 1000 == 75);
    }

    /* ============ §G 缺卡时的中性值（用户 ③）============ */
    {
        /* 只装 110：信号强度显示 1.00、槽满格（49 px）、**档位闪色 = 0**（不闪烁）。
         * ⚠ 判「卡在不在场」的判据**不在本层**（走 `faith_registered_mask()`，`faith.h`）；
         *   本层只保证「中性读数本身」是对的。*/
        CHECK(faith_hud_signal_strength(FAITH_HUD_SIGNAL_NEUTRAL) == 1000);
        CHECK(faith_hud_signal_width_px(FAITH_HUD_SIGNAL_NEUTRAL) == 49);
        CHECK(faith_hud_signal_level(FAITH_HUD_SIGNAL_NEUTRAL) == 10);
        /* ⚠ 中性槽的**闪色档**是 1（100.00 落在 100..109 那一档）—— 所以「110 单装不闪烁」
         *   靠的是**卡侧不发那个中断**（`faith_hud.c` 里按 presence 门掉），不是靠这里的值。
         *   这条断言把这个坑钉在明面上：谁把「不闪」实现成「让 flash() 返回 0」就会看见它红。*/
        CHECK(faith_hud_signal_flash(FAITH_HUD_SIGNAL_NEUTRAL) == 1);
        /* 只装 111：连击条填满 = 130 帧 ⇒ 满宽。*/
        CHECK(FAITH_HUD_COMBO_NEUTRAL == 130);
        CHECK(faith_hud_combo_width_milli(FAITH_HUD_COMBO_NEUTRAL) == FAITH_HUD_BAR_W_MILLI_FULL);
    }

    /* ============ §H ★ 110/111 那块的压暗门限：**换算订正 224 → 320**（2026-09-15h）============
     * 本文件（`faith_hud.c`）原先自己写了一份 `fh_dim_limit()`，把「世界 x → 640 空间」写成
     * `x640 = 224 + 世界x` ⇒ 门限 x ∈ [−95.5, −1.5]（偏 96 自机 px）。**现在并进跨卡统一件**
     * （`cards/hud_dim.{c,h}`），换算用定式 `自机 = 表面/2 − (224, 16)`（★ z29 订正：原钉 320 是**离屏 surface 1** 的口径，
     * 会让 113 那种靠左的带永远够不着 —— owner 实跑「只有碰到主动卡位置才变暗」的直因）
     * （一手：`engine/anm/th18/01-vm-instante.md` §3b「游玩区左边缘 257 ⇐ 640−192×2」）⇒ **x ∈ [−191.5, −97.5]**。
     * 这里逐值钉死，并留一条**反面钉子**：320 那套算出来的值必须**不等于**现在这套。*/
    {
        hud_dim_rect_t r;
        hud_dim_t d;
        const float pp_in[2]  = { -120.0f, 400.0f };   /* 贴着块（矩形 y 上沿 416 之上 16）⇒ 进 */
        const float pp_out[2] = { -120.0f, 370.0f };   /* 出到「矩形 + 44」之外（416 − 44 = 372）*/

        /* 块 = 货架槽左下角 (257, 924) + 188×60（表面 px）= 640 空间的 94×30 */
        hud_dim_rect_surface(&r, 257.0f, 924.0f, 188.0f, 60.0f);
        CHECK(NEAR_F(r.x0, -95.5f) && NEAR_F(r.x1, -1.5f));   /* ★ z29：320→224 订正后的两个沿 */
        CHECK(NEAR_F(r.y0,  416.0f) && NEAR_F(r.y1, 446.0f));
        /* ★ 反面钉子：224 那套的两个沿（−95.5 / −1.5）与现在这套差 96 —— 谁把 224 写回来，这里就红 */
        CHECK(!NEAR_F(r.x0, -191.5f) && !NEAR_F(r.x1, -97.5f));   /* ★ z29：320 那套的反面钉子 */
        CHECK(NEAR_F(r.x0 - (-191.5f), 96.0f));

        /* 进 = 矩形 + 28 ⇒ x ∈ [−219.5, −69.5]、y ∈ [388, 474]；出 = 矩形 + 44 */
        CHECK(hud_dim_gate(0, &r, pp_in) == 1);            /* y 400 ∈ [388, 474] ⇒ 进（旧口径下 400 < 416 ⇒ 不进）*/
        CHECK(hud_dim_gate(0, &r, pp_out) == 0);           /* y 370 < 388 ⇒ 不进 */
        CHECK(hud_dim_gate(1, &r, pp_out) == 0);           /* 370 < 372 = 416 − 44 ⇒ 出 */
        {
            const float edge_in[2]  = { -95.5f, 388.0f };  /* 贴「矩形 + 28」的左上角 */
            const float edge_out[2] = { -95.5f, 372.0f };  /* 贴「矩形 + 44」的上沿 */
            const float be_in[2]    = { -123.6f, 430.0f };
            const float be_out[2]   = { -139.6f, 430.0f };

            CHECK(hud_dim_gate(0, &r, edge_in) == 1);      /* 边界含 */
            CHECK(hud_dim_gate(0, &r, be_in) == 0);        /* 差 0.1 px 就不进 */
            CHECK(hud_dim_gate(1, &r, edge_out) == 1);     /* 滞回带内保持（16 px 带）*/
            CHECK(hud_dim_gate(1, &r, be_out) == 0);       /* 越 0.1 px ⇒ 出 */
        }

        /* 与共享件的动态连起来跑一遍（★ **2026-09-22 起统一值 = 终点 64 / 8 帧线性** =
         *   TH18 原生那块「当前卡」UI 的一手值，见 `cards/hud_dim.h` 的文件头与
         *   `engine/card/th18/15-selected-card-hud.md` §4.2）：
         *   255 → 64 逐帧 = `255 + (−191)·t/8` ⇒ 232,208,184,160,136,112,88,64，第 8 帧精确到位；
         *   闩锁与 alpha 同步；反向同长。历史：±56（4 帧 / 终点 32）→ 128 / 20 帧 → 本值。*/
        hud_dim_init(&d);
        CHECK(hud_dim_tick(&d, &r, pp_in) == 232);
        CHECK(hud_dim_tick(&d, &r, pp_in) == 208);
        CHECK(hud_dim_tick(&d, &r, pp_in) == 184);
        CHECK(hud_dim_tick(&d, &r, pp_in) == 160);
        {
            int k;
            for (k = 4; k < HUD_DIM_FRAMES; ++k) (void)hud_dim_tick(&d, &r, pp_in);
            CHECK(hud_dim_tick(&d, &r, pp_in) == HUD_DIM_ALPHA_DIM);   /* 第 8 帧精确到位 */
        }
        CHECK(d.dim == 1);
        {
            int32_t a = hud_dim_tick(&d, &r, pp_out);                  /* 出：当帧起淡回（不跳变）*/
            CHECK(d.dim == 0);
            CHECK(a > HUD_DIM_ALPHA_DIM && a < HUD_DIM_ALPHA_FULL);
        }
    }

    printf("test_faith_hud_core: %d passed, %d failed\n", s_pass, s_fail);
    return s_fail ? 1 : 0;
}
