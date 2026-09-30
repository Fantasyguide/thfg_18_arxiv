/* top_indicator.h —— **顶部三角指示**（对象在画面顶边之外时，顶边画一个三角）。
 *
 * ============================ 这是什么 ============================
 * owner 2026-10-01 需求：「当点处于画面正上方外的时候，会在画面顶部有一个三角形指示上面还有，
 *   107 的自建 VM、110-111 的信仰点都得有那个三角形」；行为口径：**一个对象一个三角 / x 跟对象 /
 *   不做夹边距 / 不是淡出（对象进画面就消失）**。
 *
 * ============================ 一手（照 TH07 活版）============================
 * **TH18 自己那条是死代码**（零件全在、扳机拿不到输入），唯一活的一手在 **TH07**（= 卡 107 的原作）：
 *   完整五段链与两条否证见 `engine/player/th18/06-offscreen-top-indicator-absent.md`。
 *
 *   | 项 | TH07 一手 | 本实现 |
 *   | --- | --- | --- |
 *   | 判据 | `item.pos.y < -8.0f`（常量 `th07:0x498c70`）| `CE_TOPI_Y_THRESHOLD` |
 *   | 固定 y | `y = 8.0 + 全局`（`th07:0x498a7c`）| `CE_TOPI_Y_POS`（TH18 的场地顶 = 0，无需全局）|
 *   | alpha | `clamp(255 − (8.0 − y) × 255/128, ≥64)`（`th07:0x433daa`–`0x433dfa`）| `ce_topi_alpha()` |
 *   | 贴图 | `etama.anm` entry1 `etama2.png` 的 `(240,64,16,16)`（近白 16×16）| `CE_ANM_ABILITY_SPRITE_TOP_INDICATOR` |
 *   | 脚本 | TH18 原生 134..141 的正文 = `sprite(); originMode(1); type(0); static();` | `375_top_indicator.anm.txt` |
 *   | 颜色 | 出上沿那帧先写 `0xffffffff`，再由 alpha 覆盖 | 白 + alpha（`ce_anm_set_color`）|
 *
 * ⚠ **不需要 TH07 的 `item+0x27e` 翻转闩锁**：那一位是「同一个 VM 在**正常贴图**与**三角贴图**
 *   之间来回换」的记账；我们给三角**单独一个 VM**，存在即出界、删掉即回画面内 ⇒ 没有可翻转的状态。
 *
 * ============================ 用法 ============================
 *   · 每个对象存一个 `uint32_t` 句柄（初值 0），每帧对象位置算完后调一次 `ce_topi_tick()`；
 *   · 对象消亡时调 `ce_topi_kill()`；整池复位（换关/删卡）时对每颗调一次 kill；
 *   · **只能在主线程**（桩 / tick 回调）调 —— 内部走 `ce_anm_spawn` / `ce_anm_delete`（与 112/107 同纪律）。
 *   · 「被删掉会怎样」：VM 由本模块记录、由调用方在对象消亡时 kill（见 METHOD.md 的长持续效果铁律）。
 */
#ifndef CE_TOP_INDICATOR_H
#define CE_TOP_INDICATOR_H

#include "sdk.h"          /* ce_anm_spawn / ce_anm_set_pos / ce_anm_set_color / ce_anm_delete */

/* 一手常量（见文件头表）*/
#define CE_TOPI_Y_THRESHOLD   (-8.0f)   /* 判据：对象 y < 它 ⇒ 出界（th07:0x498c70）*/
#define CE_TOPI_Y_POS          (8.0f)   /* 三角固定 y（th07:0x498a7c / th18:0x446e17）*/
#define CE_TOPI_ALPHA_SPAN     (128.0f) /* alpha 的斜距（th07:0x498b24）*/
#define CE_TOPI_ALPHA_MIN      (64u)    /* alpha 地板（th07 的 0x40）*/
#define CE_TOPI_LAYER          (13)     /* 世界层：与其它场上特效同层（脚本未写 layer）*/
#define CE_TOPI_RGB_WHITE      (0x00ffffffu)  /* 一手：出上沿那帧写 0xffffffff（白）*/

static inline uint32_t ce_topi_alpha(float y)
{
    float a = 255.0f - (CE_TOPI_Y_POS - y) * (255.0f / CE_TOPI_ALPHA_SPAN);
    if (a < (float)CE_TOPI_ALPHA_MIN) a = (float)CE_TOPI_ALPHA_MIN;
    if (a > 255.0f) a = 255.0f;
    return (uint32_t)(int32_t)a;
}

/* 每帧一次：y < 阈值 ⇒ 保证三角存在并摆到 (x, 8.0)（带距离式 alpha）；回到画面内 ⇒ 立刻删（无淡出）。*/
static inline void ce_topi_tick(uint32_t *handle, float x, float y, uint32_t rgb)
{
    if (!handle) return;

    if (!(y < CE_TOPI_Y_THRESHOLD)) {          /* 回到画面内 / 已消亡 ⇒ 立刻消失 */
        if (*handle) { ce_anm_delete(*handle); *handle = 0; }
        return;
    }
    if (!*handle) {
        *handle = ce_anm_spawn(CE_ABILITY_ANM(), CE_ANM_ABILITY_SCRIPT_TOP_INDICATOR, CE_TOPI_LAYER);
        if (!*handle) return;                  /* 起不来就这一帧不画（下帧再试）*/
    }
    ce_anm_set_pos(*handle, x, CE_TOPI_Y_POS, 0.0f);
    ce_anm_set_color(*handle, (ce_topi_alpha(y) << 24) | (rgb & 0x00ffffffu));
}

static inline void ce_topi_kill(uint32_t *handle)
{
    if (handle && *handle) { ce_anm_delete(*handle); *handle = 0; }
}

#endif /* CE_TOP_INDICATOR_H */
