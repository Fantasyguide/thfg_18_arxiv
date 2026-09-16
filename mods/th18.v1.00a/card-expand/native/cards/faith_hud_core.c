/* faith_hud_core.c —— 组合 UI 的纯逻辑实现。接口与逐条出处见 `faith_hud_core.h`。
 *
 * 本文件**刻意不依赖任何引擎**（也不 include `sdk.h`）：它要能在主机 gcc 上直接跑单测
 * （`tests/test_faith_hud_core.c`，同一支测试里还链 `signal_core.c` 对账两处同式子的函数）。
 * 「画」的那一半在 `faith_hud.c`。
 */
#include "faith_hud_core.h"

int32_t faith_hud_delta_display(int32_t delta_110_fixed, int32_t delta_111_fixed)
{
    /* 各自出定点再相加（用户 ④ 的口径）—— 不是「先加定点再除」（两者在 FAITH_FIXED=100 下
     * 只差一个余数，但**口径必须写死成一个**：本仓一律「各自 ÷FIXED 再相加」）。*/
    int32_t a = delta_110_fixed / FAITH_FIXED;
    int32_t b = delta_111_fixed / FAITH_FIXED;

    if (a < 0) a = 0;                       /* Δ 下限本来就是 0（`faith_slot_add/sub` 夹过），防御性再夹一次 */
    if (b < 0) b = 0;
    return a + b;
}

int32_t faith_hud_trunc10(int32_t v)
{
    /* **复刻** `th11:0x41ce37`–`th11:0x41ce43`：
     *     idiv 10        ; eax = v/10、edx = v%10
     *     sub edi,edx    ; v -= v%10
     * 对负数，x86 的 idiv 是**向零取整**、`v - v%10` 也向零 —— C 的 `/` `%` 同语义 ⇒ 逐值等价。*/
    return v - (v % 10);
}

int32_t faith_hud_maxpoint(int32_t delta_110_fixed, int32_t delta_111_fixed)
{
    int32_t v = faith_hud_trunc10(faith_hud_delta_display(delta_110_fixed, delta_111_fixed));

    if (v < 0) v = 0;
    if (v > FAITH_HUD_MAXPOINT_MAX) v = FAITH_HUD_MAXPOINT_MAX;
    return v;
}

void faith_hud_digits6(int32_t value, int d[6])
{
    int i;

    if (!d) return;
    if (value < 0) value = 0;
    if (value > FAITH_HUD_MAXPOINT_MAX) value = FAITH_HUD_MAXPOINT_MAX;
    for (i = 5; i >= 0; --i) {              /* 低位先取；前导零是**照画**的（不是跳过）*/
        d[i] = (int)(value % 10);
        value /= 10;
    }
}

int32_t faith_hud_signal_pct(int32_t signal_permille)
{
    int32_t v;

    if (signal_permille <= 0) return 0;
    v = signal_permille / 1000;             /* 千分点 → 显示值（⌊槽存储/100⌋ 的对位）*/
    if (v > FAITH_SIGNAL_MAX / 1000) v = FAITH_SIGNAL_MAX / 1000;   /* 130（一手硬上限）*/
    return v;
}

int32_t faith_hud_signal_level(int32_t signal_permille)
{
    int32_t v = faith_hud_signal_pct(signal_permille);

    if (v > 100) v = 100;                   /* 一手 `th11:0x41b67f/0x41b687` 的钳（100 以上不再涨）*/
    return v / 10;                          /* 档 0..10 */
}

int32_t faith_hud_signal_width_px(int32_t signal_permille)
{
    /* 一手 `th11:0x41b70b` 的 `fadd 9.0` + `th11:0x41b6c3` 的 `lea edx,[eax*4]`。*/
    return 9 + 4 * faith_hud_signal_level(signal_permille);
}

int32_t faith_hud_signal_strength(int32_t signal_permille)
{
    int32_t a;

    if (signal_permille <= 0) return 0;
    a = faith_hud_signal_pct(signal_permille);
    if (a > 100) a = 100;
    return a * 10;                          /* 0.01 单位 → 千分点（满 = 1000 = 1.00）*/
}

int faith_hud_signal_flash(int32_t signal_permille)
{
    /* **复刻** `th11:0x41b593`–`0x41b66e` 的四档（分界用**显示值**，不是强度）；
     * 那四条比较写的是「pct < 100 → 中断 7（常态白）」，所以 0 档 = 不闪。*/
    const int32_t pct = faith_hud_signal_pct(signal_permille);

    if (pct < 100) return 0;
    if (pct < 110) return 1;
    if (pct < 120) return 2;
    return 3;
}

int32_t faith_hud_mult_permille(int32_t graze_mult_permille, int32_t signal_strength_permille)
{
    int32_t m;

    if (graze_mult_permille < 0) graze_mult_permille = 0;
    if (signal_strength_permille < 0) signal_strength_permille = 0;
    m = graze_mult_permille + signal_strength_permille;   /* 一手 `th11:0x41ce1f add edx,ebx` */
    if (m > FAITH_HUD_MULT_MAX) m = FAITH_HUD_MULT_MAX;   /* 9.99（两项各自的上限之和 = 9990）*/
    return m;
}

void faith_hud_mult_text(int32_t mult_permille, char out[5])
{
    int32_t m = mult_permille;
    int32_t whole, frac;

    if (!out) return;
    if (m < 0) m = 0;
    if (m > FAITH_HUD_MULT_MAX) m = FAITH_HUD_MULT_MAX;
    whole = m / 1000;                       /* 0..9 */
    frac  = (m % 1000) / 10;                /* 0..99（第 3 位小数恒 0：两项都是 10 千分点的整数倍）*/
    out[0] = (char)('0' + whole);
    out[1] = '.';
    out[2] = (char)('0' + (frac / 10));
    out[3] = (char)('0' + (frac % 10));
    out[4] = '\0';
}

int32_t faith_hud_combo_width_milli(int32_t combo)
{
    if (combo <= 0) return 0;
    if (combo > FAITH_COMBO_FULL) combo = FAITH_COMBO_FULL;
    /* 满槽宽 = `FAITH_HUD_BAR_W_MILLI_FULL`（75000 千分 px = 75 px，❓ 自定见头注）。
     * 130 × 75000 = 9,750,000 远小于 int32 上限 ⇒ 不会溢出。*/
    return combo * FAITH_HUD_BAR_W_MILLI_FULL / FAITH_COMBO_FULL;
}
