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
#define SCAN_MAX_STEPS (200u * 1024u * 1024u)
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
static DWORD g_scan_next  = 0;
static int   g_scan_left  = 30;
static int   g_hook_on     = 0;
static int   g_hook_sel    = 0;    /* ini: hook_sel=1..4 ⇒ 只装那一个；0 = 全装 */    /* ini: hook=1 ⇒ 在 0x502999 装钩子，记录段表字段地址 */
static DWORD g_hook_field  = 0;
static DWORD g_hook_r      = 0;
static DWORD g_hook_s      = 0;
static DWORD g_w0 = 0, g_w4 = 0, g_w8 = 0, g_wc = 0;

static void probe_log(const char *fmt, ...);   /* 前置声明（restore_all_patches 会用到） */

/* 卸载路径：记录装过的补丁点与原字节，能原样写回 */
#define MAX_PATCH 8
static struct { DWORD rva; int len; BYTE orig[16]; } g_patched[MAX_PATCH];
static int g_patch_n = 0;

static void remember_patch(DWORD rva, const BYTE *orig, int len)
{
    if (g_patch_n >= MAX_PATCH) return;
    g_patched[g_patch_n].rva = rva;
    g_patched[g_patch_n].len = len;
    memcpy(g_patched[g_patch_n].orig, orig, (size_t)len);
    g_patch_n++;
}

static void restore_all_patches(void)
{
    int i;
    for (i = g_patch_n - 1; i >= 0; i--) {
        BYTE *at = g_base + g_patched[i].rva;
        DWORD old;
        if (VirtualProtect(at, (SIZE_T)g_patched[i].len, PAGE_EXECUTE_READWRITE, &old)) {
            memcpy(at, g_patched[i].orig, (size_t)g_patched[i].len);
            VirtualProtect(at, (SIZE_T)g_patched[i].len, old, &old);
            FlushInstructionCache(GetCurrentProcess(), at, (SIZE_T)g_patched[i].len);
        }
    }
    probe_log("[hook] 已恢复 %d 处原字节", g_patch_n);
    g_patch_n = 0;
}   /* 四个 float store 各自收到的值 */    /* hook3：0x5029a8 处 xmm0 = 第 2 个 float 参数（s）*/    /* hook2：call 0x50c1d0 返回的 float（首个参数 r）*/    /* 钩子捕获到的 eax（= 段表字段地址，s 写入处） */
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
    int regions = 0;
    DWORD last = 0;
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
        last = rbase + rsize;
        regions++;
        addr = (const BYTE *)(uintptr_t)last;
        if (rsize == 0) break;
    }
    probe_log("[pat ] 结束：步数 %u，region %d，最后地址 %08x，命中 %d",
              steps, regions, last, hits);
}

static void poll_once(void)
{
    DWORD anchor, v;
    if (g_hook_on) {
        probe_log("[arg1] %08x  [arg2] %08x  | [w4] %08x  [w8] %08x  [wc] %08x",
                  g_hook_r, g_hook_s, g_w4, g_w8, g_wc);
        return;
    }
    if (g_hook_on && g_hook_field) {
        dump_n("segtbl(r 在 +0x00)", g_hook_field - 0x20, 24);
        return;
    }
    if (g_scan_mode == 2) {
        DWORD now = GetTickCount();
        if (g_follow) {
            /* 命中值是 entry+0x04（一手：§26.2 的 r=+0x00 / s=+0x04），entry 步长 0x30 */
            dump_n("obj-0x08", g_follow - 0x08, 96);   /* 覆盖 obj+0x00..+0x170：位置/计数器/段数全在里头 */
            return;
        }
        if (g_scan_next == 0) g_scan_next = now + 3000;   /* ★ 头 3 秒不动，先让游戏启动起来 */
        if (g_scan_left > 0 && (LONG)(now - g_scan_next) >= 0) {
            g_scan_next = now + 500;
            g_scan_left--;
            scan_for_pattern();
            if (g_scan_left == 0) probe_log("[pat ] 扫描预算用完（30 次），停止扫描。");
        }
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


/* ------------------------------------------------------------------ *
 * 钩子：TH19 v1.10c 的 609 handler 里
 *   th19:0x502999  f3 0f 11 00    movss [eax], xmm0     ← eax = 段表字段地址（s）
 *   th19:0x50299d  89 4c 24 1c    mov [esp+0x1c], ecx
 * 把这两条（共 8 字节）搬到 trampoline 里重放，同时用 A3 记下 eax。
 * 入口先 pushfd/pushad、出口 popad/popfd ⇒ 通用寄存器全保；xmm 不碰 ⇒ xmm0 不变。
 * 只读语义不变（原指令照样执行），唯一副作用是多记一个 DWORD。
 * ------------------------------------------------------------------ */
#define HOOK_RVA   0x102999u
static const BYTE HOOK_EXPECT[8] = { 0xf3,0x0f,0x11,0x00, 0x89,0x4c,0x24,0x1c };

static BOOL install_hook(void)
{
    BYTE *at = g_base + HOOK_RVA;
    BYTE *stub;
    BYTE *p;
    DWORD old;
    SIZE_T rel;
    int i;

    if (!readable(at, 8) || memcmp(at, HOOK_EXPECT, 8) != 0) {
        probe_log("[hook] 0x%x 处原字节不匹配，拒绝安装。", HOOK_RVA);
        return FALSE;
    }

    stub = (BYTE *)VirtualAlloc(NULL, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!stub) { probe_log("[hook] VirtualAlloc 失败：%lu", GetLastError()); return FALSE; }

    p = stub;
    *p++ = 0x9C;                                  /* pushfd */
    *p++ = 0x60;                                  /* pushad */
    *p++ = 0xA3;                                  /* mov [imm32], eax */
    *(DWORD *)p = (DWORD)(uintptr_t)&g_hook_field; p += 4;
    *p++ = 0x61;                                  /* popad */
    *p++ = 0x9D;                                  /* popfd */
    for (i = 0; i < 8; i++) *p++ = HOOK_EXPECT[i]; /* 原两条指令 */
    rel = (SIZE_T)(at + 8) - (SIZE_T)(p + 5);      /* 回到 0x5029a1 */
    *p++ = 0xE9;
    *(DWORD *)p = (DWORD)(int)rel; p += 4;

    remember_patch(HOOK_RVA, HOOK_EXPECT, 8);
    if (!VirtualProtect(at, 8, PAGE_EXECUTE_READWRITE, &old)) {
        probe_log("[hook] VirtualProtect 失败：%lu", GetLastError());
        VirtualFree(stub, 0, MEM_RELEASE);
        return FALSE;
    }
    at[0] = 0xE9;
    *(DWORD *)(at + 1) = (DWORD)((SIZE_T)stub - ((SIZE_T)at + 5));
    at[5] = 0x90; at[6] = 0x90; at[7] = 0x90;
    VirtualProtect(at, 8, old, &old);
    FlushInstructionCache(GetCurrentProcess(), at, 8);

    probe_log("[hook] 已安装 @ base+0x%x → stub %p（记录 eax = 段表字段地址）", HOOK_RVA, (void *)stub);
    return TRUE;
}


/* ★ hook2：0x50296c 处（`call 0x50c1d0` 之后第一条）xmm0 = 取到的第 1 个 float 参数（r）。
 * 原 10 字节：ff 74 24 20 (push [esp+0x20]) + 8d 8f ac 05 00 00 (lea ecx,[edi+0x5ac])
 * stub：movss [g_hook_r], xmm0 ; 重放这 10 字节 ; jmp 0x502976
 * 只写自己的全局，不碰任何寄存器/xmm ⇒ 语义不变。 */
#define HOOK2_RVA  0x10296cu
static const BYTE HOOK2_EXPECT[10] = { 0xff,0x74,0x24,0x20, 0x8d,0x8f,0xac,0x05,0x00,0x00 };

static BOOL install_hook2(void)
{
    BYTE *at = g_base + HOOK2_RVA;
    BYTE *stub, *p;
    DWORD old;
    int i;

    if (!readable(at, 10) || memcmp(at, HOOK2_EXPECT, 10) != 0) {
        probe_log("[hook2] 0x%x 原字节不匹配，拒绝安装。", HOOK2_RVA);
        return FALSE;
    }
    stub = (BYTE *)VirtualAlloc(NULL, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!stub) return FALSE;

    p = stub;
    *p++ = 0xF3; *p++ = 0x0F; *p++ = 0x11; *p++ = 0x05;      /* movss [imm32], xmm0 */
    *(DWORD *)p = (DWORD)(uintptr_t)&g_hook_r; p += 4;
    for (i = 0; i < 10; i++) *p++ = HOOK2_EXPECT[i];
    *p++ = 0xE9;
    *(DWORD *)p = (DWORD)((SIZE_T)(at + 10) - (SIZE_T)(p + 4));

    remember_patch(HOOK2_RVA, HOOK2_EXPECT, 10);
    if (!VirtualProtect(at, 10, PAGE_EXECUTE_READWRITE, &old)) { VirtualFree(stub, 0, MEM_RELEASE); return FALSE; }
    at[0] = 0xE9;
    *(DWORD *)(at + 1) = (DWORD)((SIZE_T)stub - ((SIZE_T)at + 5));
    for (i = 5; i < 10; i++) at[i] = 0x90;
    VirtualProtect(at, 10, old, &old);
    FlushInstructionCache(GetCurrentProcess(), at, 10);
    probe_log("[hook2] 已安装 @ base+0x%x → stub %p（记录 r）", HOOK2_RVA, (void *)stub);
    return TRUE;
}


/* ★ hook3：0x5029a8（`call 0x50c1d0` @0x5029a3 之后）xmm0 = 第 2 个 float 参数（s）。
 * 原 10 字节与 hook2 同形：ff 74 24 20 + 8d 8f ac 05 00 00。 */
#define HOOK3_RVA  0x1029a8u
static const BYTE HOOK3_EXPECT[10] = { 0xff,0x74,0x24,0x20, 0x8d,0x8f,0xac,0x05,0x00,0x00 };

static BOOL install_hook3(void)
{
    BYTE *at = g_base + HOOK3_RVA;
    BYTE *stub, *p;
    DWORD old;
    int i;

    if (!readable(at, 10) || memcmp(at, HOOK3_EXPECT, 10) != 0) {
        probe_log("[hook3] 0x%x 原字节不匹配，拒绝安装。", HOOK3_RVA);
        return FALSE;
    }
    stub = (BYTE *)VirtualAlloc(NULL, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!stub) return FALSE;
    p = stub;
    *p++ = 0xF3; *p++ = 0x0F; *p++ = 0x11; *p++ = 0x05;
    *(DWORD *)p = (DWORD)(uintptr_t)&g_hook_s; p += 4;
    for (i = 0; i < 10; i++) *p++ = HOOK3_EXPECT[i];
    *p++ = 0xE9;
    *(DWORD *)p = (DWORD)((SIZE_T)(at + 10) - (SIZE_T)(p + 4));

    remember_patch(HOOK2_RVA, HOOK2_EXPECT, 10);
    remember_patch(HOOK3_RVA, HOOK3_EXPECT, 10);
    if (!VirtualProtect(at, 10, PAGE_EXECUTE_READWRITE, &old)) { VirtualFree(stub, 0, MEM_RELEASE); return FALSE; }
    at[0] = 0xE9;
    *(DWORD *)(at + 1) = (DWORD)((SIZE_T)stub - ((SIZE_T)at + 5));
    for (i = 5; i < 10; i++) at[i] = 0x90;
    VirtualProtect(at, 10, old, &old);
    FlushInstructionCache(GetCurrentProcess(), at, 10);
    probe_log("[hook3] 已安装 @ base+0x%x → stub %p（记录 s）", HOOK3_RVA, (void *)stub);
    return TRUE;
}


/* ★ 通用 5 字节钩子：目标指令恰好 5 字节（movss [eax+disp], xmm0）。
 * stub：movss [slot], xmm0（8 字节）→ 重放原 5 字节 → jmp 回 rva+5。
 * 不碰任何寄存器、不碰其他 xmm；只写自己的全局。 */
static BOOL hook_store5(DWORD rva, const BYTE exp[5], DWORD *slot, const char *tag)
{
    BYTE *at = g_base + rva;
    BYTE *stub, *p;
    DWORD old;
    int i;

    if (!readable(at, 5) || memcmp(at, exp, 5) != 0) {
        probe_log("[store:%s] 0x%x 原字节不匹配，跳过。", tag, rva);
        return FALSE;
    }
    stub = (BYTE *)VirtualAlloc(NULL, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!stub) return FALSE;
    p = stub;
    *p++ = 0xF3; *p++ = 0x0F; *p++ = 0x11; *p++ = 0x05;
    *(DWORD *)p = (DWORD)(uintptr_t)slot; p += 4;
    for (i = 0; i < 5; i++) *p++ = exp[i];
    *p++ = 0xE9;
    *(DWORD *)p = (DWORD)((SIZE_T)(at + 5) - (SIZE_T)(p + 4));

    remember_patch(rva, exp, 5);
    if (!VirtualProtect(at, 5, PAGE_EXECUTE_READWRITE, &old)) { VirtualFree(stub, 0, MEM_RELEASE); return FALSE; }
    at[0] = 0xE9;
    *(DWORD *)(at + 1) = (DWORD)((SIZE_T)stub - ((SIZE_T)at + 5));
    VirtualProtect(at, 5, old, &old);
    FlushInstructionCache(GetCurrentProcess(), at, 5);
    probe_log("[store:%s] 已安装 @ base+0x%x → %p", tag, rva, (void *)stub);
    return TRUE;
}

static DWORD WINAPI poll_thread(LPVOID unused)
{
    DWORD t0 = GetTickCount();
    int space_down = 0;
    (void)unused;
    while (InterlockedCompareExchange(&g_running, 1, 1)) {
        /* 调试键标记（只读）：C3 是 **X 键松手（电平）** 触发 ⇒ 记录 X 的**下降沿**。
         * 只写日志，不写游戏内存。 */
        {
            int down = (GetAsyncKeyState('X') & 0x8000) != 0;
            if (!down && space_down) probe_log("[key] X-up  %u ms", GetTickCount() - t0);
            space_down = down;
        }
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
            else if (sscanf(line, "hook=%d", &g_hook_on) == 1) { /* 装钩子 */ }
            else if (sscanf(line, "hook_sel=%d", &g_hook_sel) == 1) { /* 1..4 只装一个 */ }
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

    if (g_hook_on) {
        static const BYTE E4[5] = {0xf3,0x0f,0x11,0x40,0x04};
        static const BYTE E8[5] = {0xf3,0x0f,0x11,0x40,0x08};
        static const BYTE EC[5] = {0xf3,0x0f,0x11,0x40,0x0c};
        probe_log("[hook] hook_sel=%d（0=全装）", g_hook_sel);
        if (g_hook_sel == 0 || g_hook_sel == 1) hook_store5(0x1029d4u, E4, &g_w4, "+04");
        if (g_hook_sel == 0 || g_hook_sel == 2) hook_store5(0x102a24u, E8, &g_w8, "+08");
        if (g_hook_sel == 0 || g_hook_sel == 3) hook_store5(0x102a60u, EC, &g_wc, "+0c");
        if (g_hook_sel == 0 || g_hook_sel == 4) install_hook();
    }

    InterlockedExchange(&g_running, 1);
    g_thread = CreateThread(NULL, 0, poll_thread, NULL, 0, NULL);
    if (!g_thread) {
        probe_log("[fatal] CreateThread 失败：%lu", GetLastError());
        InterlockedExchange(&g_running, 0);
        fclose(g_log); g_log = NULL; return 1;
    }
    return 0;
}
