/*
 * route_b.c —— 「路线 B：我们自己画」的**试验台**（TH18 v1.00a）
 *
 * 真正的实现在 bgwarp_core.c/.h（可移植、无 ini/无日志/无热键）。
 * 本文件只负责：日志落盘、读 ini、按键切换、以及调用 core 的公开接口。
 *
 * 为什么拆两层：owner 的口径 —— 这个 DLL 只是**方便测试**，最终要把它**并进卡侧**（109 C4）。
 * 合并时只需搬 bgwarp_core.c/.h，卡里三行：
 *     bgwarp_install(card_log);
 *     bgwarp_set(1, radius, power, alpha);   // C4 发动
 *     bgwarp_set(0, 0, 0, 0);                // C4 结束 / on_delete
 *
 * ini：同目录 th18_routeb.ini   日志：同目录 th18_routeb.log
 * 按键：8 = 循环测试模式（0 关 / 1 M1 / 2 扭曲网格 / 3 光晕[屏幕UV] / 4 平面网格 / 5 光晕[整图UV]）
 *       9 = 开关第二份（敌方那个盘）
 */

#include "bgwarp_core.h"

#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define RB_MAX_PATH 512

static FILE *rb_log;
static char  rb_ini[RB_MAX_PATH];

static void rb_host_log(const char *fmt, ...)
{
    va_list ap;
    if (!rb_log) return;
    va_start(ap, fmt);
    vfprintf(rb_log, fmt, ap);
    va_end(ap);
    fputc('\n', rb_log);
    fflush(rb_log);
}

static float rb_inif(const char *key, float dflt)
{
    char buf[64];
    if (!GetPrivateProfileStringA("routeb", key, "", buf, sizeof buf, rb_ini) || !buf[0]) return dflt;
    return (float)atof(buf);
}
static int rb_inii(const char *key, int dflt)
{
    if (!rb_ini[0]) return dflt;
    return GetPrivateProfileIntA("routeb", key, dflt, rb_ini);
}

/* 每帧（在舞台里）由 core 回调：热键 8/9 */
static int  g_two;
static float g_two_off = 256.0f;
static void rb_keys(void)
{
    if (GetAsyncKeyState('8') & 1) {
        int m = (bgwarp_mode() + 1) % 6;
        bgwarp_set_mode(m);
        rb_host_log("[键] 测试模式 → %d（0 关 / 1 M1 / 2 扭曲网格 / 3 光晕[屏幕UV] / 4 平面网格 / 5 光晕[整图UV]）", m);
    }
    if (GetAsyncKeyState('9') & 1) {
        g_two = !g_two;
        bgwarp_set_test(0, g_two, 64.0f, g_two_off, 0);
        rb_host_log("[键] 第二份 → %s", g_two ? "开" : "关");
    }
}

__attribute__((constructor)) static void rb_boot(void)
{
    char exe[RB_MAX_PATH], path[RB_MAX_PATH], *slash;
    int  enable, mode, surf, two, radius_auto;
    float radius, power, alpha, shift, two_off, fade, blend;
    int  n, overlap;

    if (!GetModuleFileNameA(NULL, exe, RB_MAX_PATH)) return;
    slash = strrchr(exe, '\\');
    if (slash) *slash = 0;
    _snprintf(path, sizeof path, "%s\\th18_routeb.log", exe);
    rb_log = fopen(path, "w");
    _snprintf(rb_ini, sizeof rb_ini, "%s\\th18_routeb.ini", exe);
    rb_host_log("=== route_b 试验台（core = bgwarp_core.c）===");

    enable      = rb_inii("enable", 1);
    mode        = rb_inii("mode", 2);
    surf        = rb_inii("surf", 0);
    two         = rb_inii("two", 0);
    radius_auto = rb_inii("radius_auto", 0);
    n           = rb_inii("n", 33);
    overlap     = rb_inii("overlap", 1);
    radius      = rb_inif("radius", 160.0f);
    power       = rb_inif("power", 1.0f);
    alpha       = rb_inif("alpha", 255.0f);
    shift       = rb_inif("shift", 64.0f);
    two_off     = rb_inif("two_off", 256.0f);
    g_two_off   = two_off;
    fade        = rb_inif("fade", 0.15f);
    blend       = rb_inif("blend", 40.0f);

    rb_host_log("配置：enable=%d mode=%d surf=%d n=%d radius=%.0f power=%.2f alpha=%.0f fade=%.2f overlap=%d blend=%.0f two=%d",
                enable, mode, surf, n, (double)radius, (double)power, (double)alpha,
                (double)fade, overlap, (double)blend, two);

    if (!enable) { rb_host_log("enable=0 ⇒ 不装桩（一个字节都不打补丁）"); return; }
    if (!bgwarp_install(rb_host_log)) { rb_host_log("★ 安装失败 ⇒ 功能关闭"); return; }

    bgwarp_tune(n, fade, overlap, blend);
    bgwarp_set_test(surf, two, shift, two_off, radius_auto);
    if (mode != 0) bgwarp_set(1, radius, power, alpha);
    bgwarp_set_mode(mode);
    bgwarp_set_tick(rb_keys);
    rb_host_log("★ 就绪：mode=%d（按键 8 循环 / 9 第二份）", mode);
}
