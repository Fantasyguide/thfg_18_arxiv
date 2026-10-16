/* test_sdk_color.c —— P14a 的**等价性判据**（主机单测，不碰引擎 ✓）。
 *   证明 `ce_color_keep_alpha()` / `ce_color_set_rgb()` 与各卡原来手写的那条表达式
 *   **逐位等价** ✓（这正是本批"行为零差异"的判据 ✓）。
 *   ⚠ id 版 `ce_anm_set_color_rgb()` 走 `ce_anm_get_vm()`（要引擎）⇒ 主机测不了 ✗，
 *     它的正确性由"内部就是这两个"保证 ✓（见 sdk.h 的实现 ✓）。*/
#include <stdio.h>
#include <stdint.h>
#include "../sdk_color.h"    /* ★ 只 include 纯函数那个头（sdk.h 在主机上编不过 ✓）*/

static int fails;
static void ok(int cond, const char *what)
{
    if (!cond) { ++fails; printf("  FAIL %s\n", what); }
}

int main(void)
{
    static const uint32_t colors[] = {
        0x00000000u, 0xffffffffu, 0x80112233u, 0x00123456u, 0xffabcdefu,
        0x7f000000u, 0x0100ff00u, 0xfe00ff00u, 0x00ffffffu, 0x40ff0000u
    };
    static const uint32_t rgbs[] = {
        0x00000000u, 0x00ffffffu, 0xff0000u, 0x00ff00u, 0x0000ffu,
        0xffd080u, 0xffffffu, 0x123456u, 0xffffffu, 0x00ff00u
    };
    unsigned i, j;
    int n = 0;

    for (i = 0; i < sizeof colors / sizeof colors[0]; ++i) {
        for (j = 0; j < sizeof rgbs / sizeof rgbs[0]; ++j) {
            uint32_t c = colors[i], r = rgbs[j];
            /* ① 纯函数 vs 各卡原来手写的字面写法（逐位相同 ✓）*/
            ok(ce_color_keep_alpha(c, r) == ((c & 0xff000000u) | (r & 0x00ffffffu)),
               "keep_alpha == literal");
            /* ② alpha 必须**一位不动** ✓（这正是这条词汇存在的理由 ✓）*/
            ok((ce_color_keep_alpha(c, r) & 0xff000000u) == (c & 0xff000000u), "alpha preserved");
            /* ③ RGB 必须**完全等于**给的 rgb 的低 24 位 ✓（高 8 位被丢弃 ✓）*/
            ok((ce_color_keep_alpha(c, r) & 0x00ffffffu) == (r & 0x00ffffffu), "rgb written");
            /* ④ 指针版与纯函数一致 ✓（且只动那一个 uint32 ✓）*/
            {
                volatile uint32_t cell = c;
                uint32_t before_hi = cell & 0xff000000u;
                ce_color_set_rgb(&cell, r);
                ok(cell == ce_color_keep_alpha(c, r), "set_rgb == keep_alpha");
                ok((cell & 0xff000000u) == before_hi, "set_rgb keeps alpha");
            }
            n += 4;
        }
    }
    printf("test_sdk_color: %d assertions, %d failed\n", n, fails);
    return fails ? 1 : 0;
}
