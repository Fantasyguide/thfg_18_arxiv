/* sdk_color.h —— ★★ 2026-10-14（P14a，owner 授权「抽取那个 set color 的 sdk 可以做」✓）：
 *   **I1 词汇的纯函数部分** ✓ —— 「只改 `color_1` 的 **RGB**、**保住 alpha**」✓
 *   （普查见 `VM-IDIOM-MAP.md` §1 的 I1 ✓；本批把各卡**各自手写**的那条表达式收成一个词汇 ✓）。
 *
 *   ⚠ **为什么单独一个头文件**：这一段是**主机可测**的 ✓（`tests/test_sdk_color.c` ✓），
 *   而 `sdk.h` 里有 `thiscall` / `fastcall` 那类平台属性 ⇒ 主机 gcc 直接 include 会报错 ✗
 *   （`-Werror=attributes` ✓）。所以纯函数拆到这里 ✓，`sdk.h` 再 include 本文件 ✓
 *   ⇒ **卡侧的用法完全不变** ✓（照样 `ce_color_keep_alpha(...)` ✓）。
 *
 *   ⚠ **绝不碰 alpha** ✓：alpha 归引擎的插值器 / 各卡自己的 alpha 通道管 ✓；
 *     若写成整条 `color_1` 会把它钉死（历史教训见 `lolk.c` 的「第三十三轮」注释 ✓）。
 *   ⚠ 与 `ce_anm_set_color()`（写**整条** `0xAARRGGBB` ✓）**不是**替代关系 ✗：
 *     那个要调用者自己带 alpha ✓，这个**从当前值里取** alpha ✓。*/
#ifndef CE_SDK_COLOR_H
#define CE_SDK_COLOR_H

#include <stdint.h>

/* 把 `color` 的 RGB 换成 `rgb` 的低 24 位，**alpha（高 8 位）原样保留** ✓。 */
static inline uint32_t ce_color_keep_alpha(uint32_t color, uint32_t rgb)
{
    return (color & 0xff000000u) | (rgb & 0x00ffffffu);
}

/* 指针版 ✓：**子弹的内嵌 VM** 只有 `color_1` 指针、**没有 VM id** ⇒ 只能走这一支 ✓。
 *   （id 版 `ce_anm_set_color_rgb()` 在 `sdk.h` 里 ✓，内部调本函数 ✓。）*/
static inline void ce_color_set_rgb(volatile uint32_t *color1, uint32_t rgb)
{
    *color1 = ce_color_keep_alpha(*color1, rgb);
}

#endif /* CE_SDK_COLOR_H */
