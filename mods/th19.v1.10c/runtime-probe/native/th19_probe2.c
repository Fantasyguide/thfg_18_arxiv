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
#define SCAN_MAX_STEPS (64u * 1024u * 1024u)
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
static DWORD g_pat[4]      = {0, 0, 0, 0};  /* ini: pattern / pattern2 / pattern3 */
static DWORD g_pat_rel     = 0;
static DWORD g_pat_rel2    = 0;
static DWORD g_pat_rel3    = 0;
static DWORD g_pat_rel4    = 0;          /* ini: pattern_rel=0x… ⇒ pattern = base + 该 RVA（找 vtable 用）*/
static int   g_pat_n       = 0;
static DWORD g_follow      = 0;    /* scan=2 找到的第一个命中：之后每轮盯着它打 */
static int   g_follow_on   = 0;    /* ini: follow=1 */


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
static void dump_n(const char *tag, DWORD va, int n)
{
    static DWORD buf[256];
    SIZE_T got = 0;
    int i, cnt;
    /* ★ 同样走 ReadProcessMemory：readable() 通过之后该页仍可能被释放（竞态），
     * 直接解引用会访问违例崩游戏（三次 minidump 都指向本 DLL，见 HANDOFF §10）。 */
    if (n > 256) n = 256;
    if (!ReadProcessMemory(GetCurrentProcess(), (const void *)(uintptr_t)va, buf, (SIZE_T)n * 4, &got)
        || got < 4) {
        probe_log("  %-18s %08x 不可读", tag, va);
        return;
    }
    cnt = (int)(got / 4);
    probe_log("  %-18s %08x :", tag, va);
    for (i = 0; i < cnt; i++) {
        float fv;
        memcpy(&fv, &buf[i], 4);
        probe_log("    +%02x  %08x  %11d  %12.4f", i * 4, buf[i],
                  (int)buf[i], (double)fv);
    }
}

static void dump(const char *tag, DWORD va) { dump_n(tag, va, DUMP_DWORDS); }

static void scan_for_pattern(void)
{
    MEMORY_BASIC_INFORMATION mbi;
    const BYTE *addr = (const BYTE *)0x10000;
    const BYTE *limit = (const BYTE *)0x7fff0000;
    unsigned steps = 0;
    int hits = 0;
    static BYTE buf[0x10000];

    probe_log("[pat ] 扫 %d 个特征：%08x %08x %08x %08x", g_pat_n, g_pat[0], g_pat[1], g_pat[2], g_pat[3]);
    while (addr < limit && steps < SCAN_MAX_STEPS && hits < 8) {
        DWORD rbase, rsize, off;
        if (!VirtualQuery(addr, &mbi, sizeof(mbi))) break;
        rbase = (DWORD)(uintptr_t)mbi.BaseAddress;
        rsize = (DWORD)mbi.RegionSize;
        if (mbi.State == MEM_COMMIT && mbi.Type != MEM_IMAGE &&
            (mbi.Protect & (PAGE_READONLY | PAGE_READWRITE | PAGE_WRITECOPY |
                            PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY)) &&
            !(mbi.Protect & (PAGE_GUARD | PAGE_NOACCESS))) {
            for (off = 0; off + 4 <= rsize && hits < 8; off += sizeof(buf)) {
                SIZE_T got = 0;
                DWORD n, i;
                /* ★ 一律走 ReadProcessMemory：扫描途中该页被释放/解提交时它只返回 FALSE，
                 * 不会像直接解引用那样就地访问违例把游戏打崩（首跑闪退的嫌疑）。 */
                if (!ReadProcessMemory(GetCurrentProcess(), (const void *)(uintptr_t)(rbase + off),
                                       buf, sizeof(buf), &got) || got < 4) {
                    n = 0;
                } else {
                    n = (DWORD)got & ~3u;
                }
                for (i = 0; i + 4 <= n; i += 4) {
                    DWORD dv = *(const DWORD *)(buf + i);
                    int pi, hitpi = -1;
                    steps++;
                    if (steps >= SCAN_MAX_STEPS) break;
                    for (pi = 0; pi < g_pat_n && pi < 4; pi++) {
                        if (!g_pat[pi]) continue;
                        if (dv == g_pat[pi] || dv == (g_pat[pi] ^ 0x80000000u)) { hitpi = pi; break; }
                    }
                    if (hitpi < 0) continue;
                    {
                        DWORD va = rbase + off + i;
                        probe_log("[pat%d] 命中 %08x  = %g  (region %08x len %08x prot %08x type %08x)",
                                  hitpi, va, *(const float *)&dv,
                                  rbase, rsize, mbi.Protect, mbi.Type);
                        dump_n("hit-0x40", va - 0x40, 32);
                        if (g_follow_on && hitpi == 0 && !g_follow) g_follow = va;
                        hits++;
                        if (hits >= 8) break;
                    }
                }
            }
        }
        addr = (const BYTE *)(uintptr_t)(rbase + rsize);
        if (rsize == 0) break;
    }
    probe_log("[pat ] 结束：步数 %u，命中 %d", steps, hits);
}

static void poll_once(void)
{
    DWORD anchor, v;
    if (g_scan_mode == 2) {
        /* now 不再需要 */
        if (g_follow) {
            /* 命中值是 entry+0x04（一手：§26.2 的 r=+0x00 / s=+0x04），entry 步长 0x30 */
            dump_n("obj-0x08", g_follow - 0x08, 96);   /* 覆盖 obj+0x00..+0x170：位置/计数器/段数全在里头 */
            return;
        }
        if ((LONG)(now - g_scan_next) >= 0) { g_scan_next = now + 500; scan_for_pattern(); }
        return;
    }
    if (g_scan_mode == 1) {
        /* now 不再需要 */
        return;   /* scan=1（找 ctx）已废弃，统一用 scan=2 */
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
            else if (sscanf(line, "pattern=0x%lx", &x) == 1)  { g_pat[0] = (DWORD)x; }
            else if (sscanf(line, "pattern2=0x%lx", &x) == 1) { g_pat[1] = (DWORD)x; }
            else if (sscanf(line, "pattern3=0x%lx", &x) == 1) { g_pat[2] = (DWORD)x; }
            else if (sscanf(line, "pattern_rel=0x%lx", &x) == 1)  { g_pat_rel = (DWORD)x; }
            else if (sscanf(line, "pattern2_rel=0x%lx", &x) == 1) { g_pat_rel2 = (DWORD)x; }
            else if (sscanf(line, "pattern3_rel=0x%lx", &x) == 1) { g_pat_rel3 = (DWORD)x; }
            else if (sscanf(line, "pattern4_rel=0x%lx", &x) == 1) { g_pat_rel4 = (DWORD)x; }
            else if (sscanf(line, "follow=%d", &g_follow_on) == 1) { /* 命中后持续跟踪 */ }
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
    if (g_pat_rel)  g_pat[0] = (DWORD)(uintptr_t)(g_base + g_pat_rel);
    if (g_pat_rel2) g_pat[1] = (DWORD)(uintptr_t)(g_base + g_pat_rel2);
    if (g_pat_rel3) g_pat[2] = (DWORD)(uintptr_t)(g_base + g_pat_rel3);
    if (g_pat_rel4) g_pat[3] = (DWORD)(uintptr_t)(g_base + g_pat_rel4);
    g_pat_n = 0;
    { int i; for (i = 0; i < 4; i++) if (g_pat[i]) g_pat_n = i + 1; }
    probe_log("anchor_rva = 0x%x / scan = %d / pat = %08x %08x %08x / follow = %d",
              g_anchor_rva, g_scan_mode, g_pat[0], g_pat[1], g_pat[2], g_follow_on);
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
