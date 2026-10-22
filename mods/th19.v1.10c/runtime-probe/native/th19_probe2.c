/* th19_probe2 —— TH19 v1.10c 只读运行时探针（段表探索）
 *
 * 目的：把「609 的 r/s 写进段表之后，运行时到底变成了什么」抓下来。
 * 一手依据（engine/bullet/th18/01-laser-subsystem.md §26）：
 *   · TH19 的 609/610/611/612 共用 handler = 0x5027a3；
 *   · 段表挂 [ctx+0x5ac] → 元素 → +0x28；r → +0x00、s → +0x04；
 *   · ctx 是 ECL 运行上下文，敌机对象 +0x1230 就是它（vtable 桩 0x5058b0 = add ecx,0x1230）。
 *
 * 设计：**地址可配置**（th19_probe2.ini），不必重编译就能换锚点 ——
 *   因为锚点（哪个全局是"当前 ctx / 当前敌机"）还没定，先用一跑把它试出来。
 *
 * 只读：不写游戏内存、不装 binhack、不挂钩子。每 POLL_MS 读一次，写 th19_probe2.log。
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#define POLL_MS      100
#define DUMP_DWORDS  16

/* 版本守卫：609 handler 开头 5 字节（一手：0x5027a3 = 6a 00 8b cf e8） */
#define RVA_SIG      0x1027a3u
static const BYTE SIG[] = { 0x6a, 0x00, 0x8b, 0xcf, 0xe8 };

static HANDLE g_thread;
static volatile LONG g_running;
static FILE *g_log;
static BYTE *g_base;
static DWORD g_anchor_rva = 0;     /* ini 里配；0 = 未配置 */
static int   g_scan_mode  = 0;     /* ini: scan=1 ⇒ 主动搜 ctx */
static DWORD g_scan_next  = 0;     /* 下次扫描时间 */

static void probe_log(const char *fmt, ...)
{
    va_list ap;
    if (!g_log) return;
    va_start(ap, fmt); vfprintf(g_log, fmt, ap); va_end(ap);
    fputc('\n', g_log); fflush(g_log);
}

static BOOL readable(const void *p, SIZE_T len)
{
    MEMORY_BASIC_INFORMATION mbi;
    const BYTE *cur = (const BYTE *)p, *end = cur + len;
    if (!p) return FALSE;
    while (cur < end) {
        if (!VirtualQuery(cur, &mbi, sizeof(mbi))) return FALSE;
        if (mbi.State != MEM_COMMIT) return FALSE;
        if (mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD)) return FALSE;
        if (!(mbi.Protect & (PAGE_READONLY | PAGE_READWRITE | PAGE_WRITECOPY |
                             PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE |
                             PAGE_EXECUTE_WRITECOPY))) return FALSE;
        cur = (const BYTE *)mbi.BaseAddress + mbi.RegionSize;
    }
    return TRUE;
}

/* 打印一块内存：dword + 当浮点看 */
static void dump(const char *tag, DWORD va)
{
    const BYTE *p = (const BYTE *)(uintptr_t)va;
    int i;
    if (!readable(p, DUMP_DWORDS * 4)) { probe_log("  %-18s %08x 不可读", tag, va); return; }
    probe_log("  %-18s %08x :", tag, va);
    for (i = 0; i < DUMP_DWORDS; i++) {
        DWORD a = *(const DWORD *)(p + i * 4);
        probe_log("    +%02x  %08x  %11d  %12.4f", i * 4, a, (int)a, *(const float *)(p + i * 4));
    }
}


/* ------------------------------------------------------------------ *
 * scan 模式：不动锚点，直接在可写内存里找"像 ECL 上下文"的对象。
 * 判据（一手倾向，见 engine/bullet/th18/01-laser-subsystem.md §27）：
 *   · +0x4 是 -1 或小整数（句柄）
 *   · +0x4e2c 起 16 个 dword 都是小整数，且至少一个非 0（变换序号表）
 *   · +0x55c8 是一个可读指针（参数 helper 0x50c1b0 会读它）
 * 找到就把它当 ctx，打 +0x5ac（弹型记录）与 +0x4e2c（序号表）。
 * ------------------------------------------------------------------ */
#define SCAN_MAX_STEPS  (12u * 1024u * 1024u)

static BOOL looks_like_ctx(DWORD p)
{
    DWORD h, i, any = 0, ptr;
    if ((p & 3) != 0) return FALSE;
    if (!readable((const void *)(uintptr_t)p, 0x55d0)) return FALSE;

    h = *(const DWORD *)(uintptr_t)(p + 4);
    if (h != 0xFFFFFFFFu && h > 0x1000u) return FALSE;

    for (i = 0; i < 16; i++) {
        DWORD v = *(const DWORD *)(uintptr_t)(p + 0x4e2c + i * 4);
        if (v > 0x10000u) return FALSE;
        if (v) any = 1;
    }
    if (!any) return FALSE;

    ptr = *(const DWORD *)(uintptr_t)(p + 0x55c8);
    if (!ptr) return FALSE;
    if (!readable((const void *)(uintptr_t)ptr, 0x10)) return FALSE;

    /* ★ 关键判据：+0x5ac 是"弹型记录"的指针（handler 用 0x40ebd0 从它取元素） */
    ptr = *(const DWORD *)(uintptr_t)(p + 0x5ac);
    if (!ptr) return FALSE;
    if (!readable((const void *)(uintptr_t)ptr, 0x40)) return FALSE;

    /* 序号表里应至少有一个像计数器的值 */
    any = 0;
    for (i = 0; i < 16; i++) {
        DWORD v = *(const DWORD *)(uintptr_t)(p + 0x4e2c + i * 4);
        if (v >= 1u && v <= 0x1000u) any = 1;
    }
    if (!any) return FALSE;
    return TRUE;
}

static void scan_for_ctx(void)
{
    MEMORY_BASIC_INFORMATION mbi;
    const BYTE *addr = (const BYTE *)0x10000;
    const BYTE *limit = (const BYTE *)0x7fff0000;
    unsigned steps = 0, found = 0;

    probe_log("[scan] 开始找 ctx ...");
    while (addr < limit && steps < SCAN_MAX_STEPS && found < 6) {
        if (!VirtualQuery(addr, &mbi, sizeof(mbi))) break;
        if (mbi.State == MEM_COMMIT &&
            (mbi.Protect & (PAGE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_READWRITE)) &&
            !(mbi.Protect & (PAGE_GUARD | PAGE_NOACCESS))) {
            const BYTE *p = (const BYTE *)mbi.BaseAddress;
            const BYTE *end = p + mbi.RegionSize;
            for (; p + 0x55d0 <= end && steps < SCAN_MAX_STEPS; p += 4, steps++) {
                if (looks_like_ctx((DWORD)(uintptr_t)p)) {
                    probe_log("[scan] 候选 ctx = %08x", (DWORD)(uintptr_t)p);
                    dump("ctx+0x5ac(弹型记录)", (DWORD)(uintptr_t)p + 0x5ac);
                    dump("ctx+0x4e2c(序号表)", (DWORD)(uintptr_t)p + 0x4e2c);
                    found++;
                    if (found >= 6) break;
                }
            }
            addr = end;
            continue;
        }
        addr = (const BYTE *)mbi.BaseAddress + mbi.RegionSize;
    }
    probe_log("[scan] 结束：步数 %u，候选 %u", steps, found);
}

static void poll_once(void)
{
    DWORD anchor, v;
    if (g_scan_mode) {
        DWORD now = GetTickCount();
        if ((LONG)(now - g_scan_next) >= 0) { g_scan_next = now + 5000; scan_for_ctx(); }
        return;
    }
    if (!g_anchor_rva) return;
    anchor = (DWORD)(uintptr_t)(g_base + g_anchor_rva);

    if (!readable((void *)(uintptr_t)anchor, 4)) { probe_log("anchor %08x 不可读", anchor); return; }
    v = *(const DWORD *)(uintptr_t)anchor;

    probe_log("[anchor] rva=%x  va=%08x  *va=%08x", g_anchor_rva, anchor, v);

    /* 三种解释都打：① anchor 处本体；② 把 *va 当 ctx；③ 把 *va 当敌机（+0x1230 = ctx） */
    dump("at-anchor", anchor);
    if (readable((void *)(uintptr_t)v, 0x10)) {
        if (readable((void *)(uintptr_t)(v + 0x5ac), 0x10)) {
            probe_log("-- 当 ctx：%08x", v);
            dump("ctx+0x5ac(弹型记录)", v + 0x5ac);
            dump("ctx+0x4e2c(变换表)", v + 0x4e2c);
        }
        if (readable((void *)(uintptr_t)(v + 0x1230 + 0x5ac), 0x10)) {
            probe_log("-- 当敌机：ctx = %08x", v + 0x1230);
            dump("enemy+0x1230+0x5ac", v + 0x1230 + 0x5ac);
            dump("enemy+0x1230+0x4e2c", v + 0x1230 + 0x4e2c);
        }
    }
}

static DWORD WINAPI poll_thread(LPVOID unused)
{
    DWORD t0 = GetTickCount();
    (void)unused;
    while (InterlockedCompareExchange(&g_running, 1, 1)) {
        probe_log("[%7u ms] ----", GetTickCount() - t0);
        poll_once();
        Sleep(POLL_MS);
    }
    return 0;
}

int __stdcall thcrap_plugin_init(void)
{
    char exe[MAX_PATH], log_path[MAX_PATH], ini_path[MAX_PATH];
    char *slash, line[128];
    FILE *ini;
    SYSTEMTIME st;

    g_base = (BYTE *)GetModuleHandleW(NULL);
    if (!GetModuleFileNameA(NULL, exe, MAX_PATH)) return 1;

    lstrcpynA(log_path, exe, MAX_PATH);
    slash = strrchr(log_path, '\\');
    lstrcpynA(slash ? slash + 1 : log_path, "th19_probe2.log",
              MAX_PATH - (int)(slash ? slash + 1 - log_path : 0));
    g_log = fopen(log_path, "a");
    if (!g_log) {
        DWORD n = GetTempPathA(MAX_PATH, log_path);
        if (n == 0 || n >= MAX_PATH - 16) return 1;
        lstrcatA(log_path, "th19_probe2.log");
        g_log = fopen(log_path, "a");
        if (!g_log) return 1;
    }

    /* ini 与 exe 同目录：anchor_rva=0x12345678 */
    lstrcpynA(ini_path, exe, MAX_PATH);
    slash = strrchr(ini_path, '\\');
    lstrcpynA(slash ? slash + 1 : ini_path, "th19_probe2.ini",
              MAX_PATH - (int)(slash ? slash + 1 - ini_path : 0));
    if ((ini = fopen(ini_path, "r")) != NULL) {
        while (fgets(line, sizeof(line), ini)) {
            unsigned long x;
            if (sscanf(line, "anchor_rva=0x%lx", &x) == 1) g_anchor_rva = (DWORD)x;
            else if (sscanf(line, "anchor_rva=%lx", &x) == 1) g_anchor_rva = (DWORD)x;
            else if (sscanf(line, "scan=%d", &g_scan_mode) == 1) { /* scan 模式开关 */ }
        }
        fclose(ini);
    }

    GetLocalTime(&st);
    probe_log("");
    probe_log("=== th19_probe2 起 %04d-%02d-%02d %02d:%02d:%02d ===",
              st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    probe_log("exe  = %s", exe);
    probe_log("base = %p", (void *)g_base);

    if (!readable(g_base + RVA_SIG, sizeof(SIG)) ||
        memcmp(g_base + RVA_SIG, SIG, sizeof(SIG)) != 0) {
        probe_log("[guard] 0x%x 签名不匹配 —— 不是 th19.v1.10c，探针自我卸载。", RVA_SIG);
        fclose(g_log); g_log = NULL; return 1;
    }
    probe_log("[guard] 609 handler 签名匹配，确认 th19.v1.10c。");
    probe_log("anchor_rva = 0x%x / scan = %d（th19_probe2.ini；都为 0 时只打心跳）",
              g_anchor_rva, g_scan_mode);
    probe_log("字段：段表 = ctx+0x5ac → 元素 → +0x28；r=+0x00、s=+0x04；ctx = enemy+0x1230");

    InterlockedExchange(&g_running, 1);
    g_thread = CreateThread(NULL, 0, poll_thread, NULL, 0, NULL);
    if (!g_thread) {
        probe_log("[fatal] CreateThread 失败：%lu", GetLastError());
        InterlockedExchange(&g_running, 0);
        fclose(g_log); g_log = NULL; return 1;
    }
    return 0;
}
