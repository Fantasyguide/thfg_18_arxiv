/*
 * th18_bgwarp —— TH18 v1.00a「boss 背景扭曲」**顶点效果验证 mod**（临时独立 DLL）
 *
 * 为什么单独拆一支 DLL（而不是并进 card-expand）：
 *   这份工单（HANDOFF-109-BOSS-BG-WARP §5「WP-C 真·局域背景扭曲」）要动的是
 *   **卡侧从未有过的顶点管线**；card-expand 那支 DLL 背着 35 个站点、卡表搬迁、
 *   共享件，任何一次失败都分不清是"顶点设施错了"还是"卡片环境错了"。
 *   本 DLL **零 patch、零站点、零共享件**：自己装两个内联 hook 就够，
 *   装/卸 = 把 th18_bgwarp.dll 放进/拿出 <thcrap>/bin。
 *
 * 它验的是什么（对应工单 §5.1 的 M1/M2）：
 *   ① **看点**：原版那个"半径 112 的 boss 背景扭曲"到底有没有真的上屏 ——
 *      mesh 顶点在「上传给 VM 顶点缓冲」那一刻是什么样、在「引擎逐顶点重算之
 *      后」又是什么样。两条日志一比就知道工单 §9.24 的结论成立不成立。
 *   ② **能驱动**：在"上传"那一刻把 mesh 顶点改成我们自己的 (x,y)→(x',y')，
 *      引擎会照单全收地拷进 17 段 VM 的顶点缓冲 ⇒ 我们自己就能造出这个扭曲。
 *      这是 109 要的**顶点效果**能不能做的最小验证。
 *
 * 一手依据（全部本机核对过；地址/RVA 与字节见 ../TARGET.md）：
 *   · `0x42b40`(上传) 的调用者只有 `0x42b3e0`(建网格) 和菜单里一处；
 *     `0x42b3e0` 又被 `0x41e7e7`／`0x41eb27`（StageInner::on_tick_part_2，
 *     非符分支）和 `0x42f954`（敌人 ECL-629 那条残迹）调用。
 *   · `0x42b540` 把 `mesh+0x14`（28 字节/顶点）逐段拷进 `vm+0x5c8`
 *     （special_vertex_buffer_data），`AnmManager__draw_vm` 再把它推给 D3D
 *     （`0x481643`／`0x48166d`／`0x481697` 三处 push）。
 *   · 顶点形态 = {x, y, z, w=1.0f, D3DCOLOR, u, v}（7 dword = 28 B），
 *     每 16 段 VM 拿两行（= 一条三角带，17 列 × 2 行）。
 *
 * 纪律：本文件**不调用任何引擎函数**、不构造引擎栈帧、不碰 FPU 栈；
 *       唯一的手写机器码是 install_hook() 的跳板（逐字节注释在下面）。
 */

#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

/* ================================================================== *
 * 1. 死绑量（TH18 v1.00a；md5 9969cac756098c1da05a81de45437a70）
 * ================================================================== */

#define RVA_MESH_UPLOAD   0x2b540u  /* FUN_0042b540(mesh) 网格顶点 → 各 VM 的 +0x5c8 */
#define RVA_MESH_BUILD    0x2b3e0u  /* FUN_0042b3e0(mesh, xmm1, xmm2, xmm3, 栈参) 建网格并上传 */
#define RVA_SWIRL_EXIT    0x1ee38u  /* on_tick_part_2：[mode==2] 逐顶点重算循环的出口 */
#define RVA_UPLOAD_CALLER 0x2b52eu  /* build_grid 内 `call 0x42b540` 的下一条指令（= 我们的返回地址） */

/* 签名校验（任一不匹配 ⇒ thcrap_plugin_init 返回 1，自我卸载） */
static const BYTE SIG_UPLOAD[] = {
    0x55, 0x8b, 0xec, 0x51, 0x57, 0x8b, 0xf9, 0x8b, 0x07, 0x85, 0xc0
};
static const BYTE SIG_BUILD[] = {
    0x55, 0x8b, 0xec, 0x83, 0xec, 0x10, 0x57, 0x8b, 0xf9, 0x0f, 0x28, 0xc2
};
static const BYTE SIG_TICK[] = {          /* StageInner::on_tick_part_2 的非符判定 */
    0xa1, 0xc0, 0xf2, 0x4c, 0x00, 0x85, 0xc0, 0x74, 0x0a, 0xf6, 0x40, 0x78, 0x01
};
static const BYTE SIG_SWIRL[] = {         /* [mode==2] 循环出口 */
    0x8b, 0x7c, 0x24, 0x1c, 0x33, 0xc9, 0xf3, 0x0f, 0x10, 0x8f, 0x34, 0x34, 0x00, 0x00
};

/* mesh 对象（0x1c 字节；ctor = 0x42b1f0） */
#define OFF_MESH_N        0x00u   /* int  行数 = 0x11(17)，ctor 里写死 */
#define OFF_MESH_M        0x04u   /* int  列数 = ctor 第 3 参 */
#define OFF_MESH_SPRITE   0x08u   /* int  sprite id（0x7fff = 屏幕面特例） */
#define OFF_MESH_IDS      0x0cu   /* int[17] 每段的 sprite id */
#define OFF_MESH_VMS      0x10u   /* zAnmVm*[17] 每段一台 VM */
#define OFF_MESH_VERT     0x14u   /* 28 B/顶点 {x,y,z,w,color,u,v} */
#define OFF_MESH_POS      0x18u   /* 12 B/顶点 {x,y,z}（同一份网格的位置副本） */

/* 舞台对象（StageInner；run_std = 0x41d260 建的） */
#define OFF_STAGE_MESH    0x3410u /* void*  那份带贴图的网格 */
#define OFF_STAGE_RADIUS  0x3414u /* float  半径阈值 = 112.0f（一手 `0x41da0d`） */
#define OFF_STAGE_R2      0x340cu /* float  半径²/距离² 一类 */
#define OFF_STAGE_GROW    0x3418u /* float  当前半径（>阈值时回退 2.0） */
#define OFF_STAGE_AMPX    0x3434u /* float  波形相位/幅度（`0x41e78a` 读） */
#define OFF_STAGE_AMPY    0x3438u /* float  同上（`0x41e797` 读） */
#define OFF_STAGE_MODE    0x343cu /* int    1=波浪模式 2=旋涡圆盘模式（`0x41e7ab` 分派） */

#define VERT_STRIDE       28u

/* 一手：模式的旋涡算法里的常量（`0x41eb07`~/`0x41ec82`~） */
#define ONE_CX            224.0f   /* 0x4b9348：圆盘中心 x（网格自身空间） */
#define ONE_CY            240.0f   /* 0x4b934c：圆盘中心 y */
#define ONE_AMPPERSTEP    8.0f     /* 0x4b9268：sin 项 ×8 */
#define ONE_PHASE_STEP_X  1.5707963f /* 0x4b91dc：π/2，逐顶点推进 */
#define ONE_PHASE_STEP_Y  0.6981317f /* 0x4b9150：0.698，逐顶点推进 */
#define ONE_ALPHA_INSIDE  0x60u    /* `0x41ebfe`：圆内 alpha = 0x60 */
#define ONE_ALPHA_OUTSIDE 0x00u    /* `0x41ed5a`：圆外 alpha = 0 */

/* ================================================================== *
 * 2. thcrap 断点 ABI 的寄存器帧（我们自己 pushad/pushfd 造出来的）
 * ================================================================== */

typedef struct {
    uint32_t eflags;
    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax;
    uint32_t retaddr;
} x86_reg_t;

/* ================================================================== *
 * 3. 全局
 * ================================================================== */

static BYTE    *g_base;          /* th18.exe 基址（不假设 0x400000） */
static FILE    *g_log;
static char     g_ini[MAX_PATH];
static int      g_ok;            /* 签名与 hook 都过了 */

static int      g_mode = 0;      /* 0=只看 1=恒等 2=复刻旋涡 3=放大旋涡 */
static int      g_toggle_vk = VK_F5;
static int      g_dump_vk   = VK_F6;
static int      g_mode_vk   = VK_F7;

/* 旋涡参数（默认值全部是一手量或一手公式的直译；见 ../TARGET.md） */
static float    g_radius   = 112.0f;
static float    g_cx       = ONE_CX;
static float    g_cy       = ONE_CY;
static float    g_amp      = 1.0f;    /* 对应一手的 [stage+0x3438] 初值（❓ 运行时值本 DLL 会打出来） */
static float    g_amp_mul  = 1.0f;
static float    g_phase0_x = 0.0f;
static float    g_phase0_y = 0.0f;
static float    g_phase_step_x = ONE_PHASE_STEP_X;
static float    g_phase_step_y = ONE_PHASE_STEP_Y;
static int      g_hide_outside = 1;   /* 一手：圆外 alpha=0 */
static int      g_touch_enemy_mesh = 0; /* 敌人 ECL-629 那份（残迹）默认只记不改 */
static float    g_phase_drift = 0.02f;
static int      g_log_every = 64;     /* 每 N 次上传记一行（0 = 只记变化） */

static unsigned g_frames;             /* 上传 hook 命中次数 */
static float    g_phase;              /* 自由运行的相位（仅注入用） */

/* ================================================================== *
 * 4. 日志
 * ================================================================== */

static void bgw_log(const char *fmt, ...)
{
    va_list ap;
    char buf[1024];
    if (!g_log) return;
    va_start(ap, fmt);
    _vsnprintf(buf, sizeof buf - 1, fmt, ap);
    va_end(ap);
    buf[sizeof buf - 1] = 0;
    fputs(buf, g_log);
    fputc('\n', g_log);
    fflush(g_log);            /* 崩了也要留下最后一行 */
}

/* 读之前先问操作系统能不能读（关卡切换时这些指针可能是野值）。
 * 用 VirtualQuery 而不是 __try/__except：i686 上 clang 的 SEH 不可靠。 */
static BOOL readable(const void *p, SIZE_T len)
{
    MEMORY_BASIC_INFORMATION mbi;
    const BYTE *cur = (const BYTE *)p;
    const BYTE *end = cur + len;
    if (!p) return FALSE;
    if ((UINT_PTR)p < 0x10000u) return FALSE;
    while (cur < end) {
        if (!VirtualQuery(cur, &mbi, sizeof(mbi))) return FALSE;
        if (mbi.State != MEM_COMMIT) return FALSE;
        if (mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD)) return FALSE;
        cur = (const BYTE *)mbi.BaseAddress + mbi.RegionSize;
    }
    return TRUE;
}

/* ================================================================== *
 *5. 内联 hook（x86 32 位；唯一的手写机器码）
 * ================================================================== */

/*
 * 跳板布局（i686）：
 *   60                pushad                      ; 压 edi,esi,ebp,esp,ebx,edx,ecx,eax
 *   9c                pushfd                      ; 压 eflags → [esp] 正好是 x86_reg_t 的头
 *   54                push esp                    ; 参数 = &x86_reg_t
 *   b8 <fn32>         mov eax, handler
 *   ff d0             call eax                    ; cdecl
 *   83 c4 04          add esp, 4                  ; 弹掉参数
 *   9d                popfd
 *   61                popad                       ; 恢复全部通用寄存器
 *   <原 N 字节>                                    ; 必须是完整指令、且不含相对寻址
 *   e9 <rel32>        jmp target+N
 *
 * 入口补丁：e9 <rel32> + (N-5) 个 0x90。
 * 为什么 pushad 帧就是 x86_reg_t：thcrap 的布局同样来自 pushad
 * （thcrap/src/expression.h；本仓 thcrap_bp.h 亦有复刻）——
 * 内存低→高 = edi,esi,ebp,esp,ebx,edx,ecx,eax，前面再放 eflags。
 */

#define STUB_MAX 64

static BYTE *make_stub(BYTE *target, int len, void *handler)
{
    BYTE *stub = (BYTE *)VirtualAlloc(NULL, STUB_MAX, MEM_COMMIT | MEM_RESERVE,
                                      PAGE_EXECUTE_READWRITE);
    int i = 0;
    UINT_PTR back = (UINT_PTR)target + len;
    UINT_PTR fn = (UINT_PTR)handler;
    if (!stub) return NULL;

    stub[i++] = 0x60;                       /* pushad */
    stub[i++] = 0x9c;                       /* pushfd */
    stub[i++] = 0x54;                       /* push esp */
    stub[i++] = 0xb8;                       /* mov eax, imm32 */
    memcpy(stub + i, &fn, 4); i += 4;
    stub[i++] = 0xff; stub[i++] = 0xd0;     /* call eax */
    stub[i++] = 0x83; stub[i++] = 0xc4; stub[i++] = 0x04;  /* add esp,4 */
    stub[i++] = 0x9d;                       /* popfd */
    stub[i++] = 0x61;                       /* popad */
    memcpy(stub + i, target, len); i += len;
    stub[i++] = 0xe9;
    {   INT32 rel = (INT32)(back - ((UINT_PTR)(stub + i) + 4));
        memcpy(stub + i, &rel, 4); i += 4; }
    if (i > STUB_MAX) return NULL;
    FlushInstructionCache(GetCurrentProcess(), stub, i);

    {   BYTE patch[16];
        DWORD old;
        INT32 rel = (INT32)((UINT_PTR)stub - ((UINT_PTR)target + 5));
        if (len < 5 || len > (int)sizeof patch) return NULL;
        memset(patch, 0x90, len);
        patch[0] = 0xe9;
        memcpy(patch + 1, &rel, 4);
        if (!VirtualProtect(target, len, PAGE_EXECUTE_READWRITE, &old)) {
            bgw_log("[hook] VirtualProtect 失败 @%p err=%lu", target, GetLastError());
            return NULL;
        }
        memcpy(target, patch, len);
        VirtualProtect(target, len, old, &old);
        FlushInstructionCache(GetCurrentProcess(), target, len);
    }
    return stub;
}

/* ================================================================== *
 * 6. 小工具
 * ================================================================== */

/* 自带 sin：省一类 CRT 依赖（Vista 之后的 msvcrt 才有 sinf）。
 * 先把相位折到 [-π,π]，再用 11 阶泰勒（|err| < 5e-6）。 */
static float bgw_sin(float x)
{
    const float PI = 3.14159265358979f;
    float x2, r;
    while (x >  PI) x -= 2.0f * PI;
    while (x < -PI) x += 2.0f * PI;
    x2 = x * x;
    r = -1.0f / 39916800.0f;
    r = r * x2 + 1.0f / 362880.0f;
    r = r * x2 - 1.0f / 5040.0f;
    r = r * x2 + 1.0f / 120.0f;
    r = r * x2 - 1.0f / 6.0f;
    r = r * x2 + 1.0f;
    return r * x;
}


typedef struct {
    int   n, m;
    int   n_alpha_ff, n_alpha_60, n_alpha_00, n_alpha_other;
    float xmin, xmax, ymin, ymax;
    float step_dev_x, step_dev_y;   /* 与"第一列/第一行步长"的最大偏差（规则网格 ⇒ 0） */
    int   n_nonfinite;
} mesh_stats;

static void scan_mesh(const BYTE *mesh, mesh_stats *st)
{
    int n = *(const int *)(mesh + OFF_MESH_N);
    int m = *(const int *)(mesh + OFF_MESH_M);
    const BYTE *v = *(const BYTE * const *)(mesh + OFF_MESH_VERT);
    int i, row, col;

    memset(st, 0, sizeof *st);
    st->n = n; st->m = m;
    if (n <= 0 || m <= 0 || n > 64 || m > 64) return;
    if (!readable(v, (SIZE_T)n * m * VERT_STRIDE)) { st->n = -1; return; }

    st->xmin = st->ymin =  1e30f;
    st->xmax = st->ymax = -1e30f;
    for (i = 0; i < n * m; i++) {
        const BYTE *p = v + (SIZE_T)i * VERT_STRIDE;
        float x, y;
        unsigned col;
        BYTE a;
        memcpy(&x, p + 0, 4);
        memcpy(&y, p + 4, 4);
        memcpy(&col, p + 16, 4);
        a = (BYTE)(col >> 24);
        if (a == 0xff) st->n_alpha_ff++;
        else if (a == 0x60) st->n_alpha_60++;
        else if (a == 0x00) st->n_alpha_00++;
        else st->n_alpha_other++;
        if (!(x == x) || !(y == y) || fabsf(x) > 1e9f || fabsf(y) > 1e9f) st->n_nonfinite++;
        if (x < st->xmin) st->xmin = x;
        if (x > st->xmax) st->xmax = x;
        if (y < st->ymin) st->ymin = y;
        if (y > st->ymax) st->ymax = y;
    }
    /* 规则性：同一行内 x 的步长、同一列内 y 的步长是否恒定 */
    for (row = 0; row < n; row++) {
        float base = 0.0f;
        for (col = 1; col < m; col++) {
            float x0, x1, d;
            memcpy(&x0, v + ((SIZE_T)row * m + col - 1) * VERT_STRIDE + 0, 4);
            memcpy(&x1, v + ((SIZE_T)row * m + col    ) * VERT_STRIDE + 0, 4);
            d = x1 - x0;
            if (col == 1) base = d;
            else if (fabsf(d - base) > st->step_dev_x) st->step_dev_x = fabsf(d - base);
        }
    }
    for (col = 0; col < m; col++) {
        float base = 0.0f;
        for (row = 1; row < n; row++) {
            float y0, y1, d;
            memcpy(&y0, v + ((SIZE_T)(row - 1) * m + col) * VERT_STRIDE + 4, 4);
            memcpy(&y1, v + ((SIZE_T)row       * m + col) * VERT_STRIDE + 4, 4);
            d = y1 - y0;
            if (row == 1) base = d;
            else if (fabsf(d - base) > st->step_dev_y) st->step_dev_y = fabsf(d - base);
        }
    }
}

static void log_mesh(const char *tag, const BYTE *mesh, const mesh_stats *st, unsigned retaddr)
{
    int n_vm = 0, n_vm_buf = 0;
    const BYTE *vms = *(const BYTE * const *)(mesh + OFF_MESH_VMS);
    int k;
    if (readable(vms, 4 * 17)) {
        for (k = 0; k < 17; k++) {
            const BYTE *vm = *(const BYTE * const *)(vms + k * 4);
            const BYTE *buf;
            if (!vm || !readable(vm, 4)) continue;
            n_vm++;
            buf = *(const BYTE * const *)(vm + 0x5c8);
            if (buf && readable(buf, 4)) n_vm_buf++;
        }
    }
    bgw_log("%s #%u ret=%08x mesh=%p n=%d m=%d sprite=%08x VMs=%d(有顶点缓冲 %d)"
            " | alpha ff/60/00/other = %d/%d/%d/%d | x[%.2f,%.2f] y[%.2f,%.2f]"
            " | 步长偏差 %.3f/%.3f%s",
            tag, g_frames, retaddr, mesh, st->n, st->m,
            *(const unsigned *)(mesh + OFF_MESH_SPRITE), n_vm, n_vm_buf,
            st->n_alpha_ff, st->n_alpha_60, st->n_alpha_00, st->n_alpha_other,
            st->xmin, st->xmax, st->ymin, st->ymax,
            st->step_dev_x, st->step_dev_y,
            st->n_nonfinite ? "  ★非有限值!" : "");
}

/* ================================================================== *
 * 7. 注入：一手旋涡公式的复刻（圆内 = 拉 + 正弦；圆外 alpha=0）
 * ================================================================== */

static void inject_swirl(BYTE *mesh, float mul)
{
    int n = *(int *)(mesh + OFF_MESH_N);
    int m = *(int *)(mesh + OFF_MESH_M);
    BYTE *v = *(BYTE **)(mesh + OFF_MESH_VERT);
    float R2 = g_radius * g_radius;
    int row, col;

    if (n <= 0 || m <= 0 || n > 64 || m > 64) return;
    if (!readable(v, (SIZE_T)n * m * VERT_STRIDE)) return;

    for (row = 0; row < n; row++) {
        for (col = 0; col < m; col++) {
            BYTE *p = v + ((SIZE_T)row * m + col) * VERT_STRIDE;
            float x, y, z, dx, dy, dz, r2, t, inv, nx, ny, phx, phy;
            unsigned col_u32;
            memcpy(&x, p + 0, 4);
            memcpy(&y, p + 4, 4);
            memcpy(&z, p + 8, 4);
            dx = x - g_cx;
            dy = y - g_cy;
            dz = z - R2;                    /* 一手：0x41ebbf `v.z - 半径²` */
            r2 = dx * dx + dy * dy;
            if (r2 >= R2) {
                if (g_hide_outside) {
                    memcpy(&col_u32, p + 16, 4);
                    col_u32 = (col_u32 & 0x00ffffffu) |
                              ((unsigned)ONE_ALPHA_OUTSIDE << 24);
                    memcpy(p + 16, &col_u32, 4);
                }
                continue;
            }
            t = (R2 - r2) / R2;             /* 圆心 1 → 边缘 0（一手 0x41ebf3 divss） */
            inv = 1.0f / sqrtf(dx * dx + dy * dy + dz * dz);
            nx = dx * inv;
            ny = dy * inv;
            phx = g_phase0_x + (float)(row * m + col) * g_phase_step_x;
            phy = g_phase0_y + (float)(row * m + col) * g_phase_step_y;
            /* 一手：v.x += t*n.x + sin(phase_x)*amp*8 ；v.y 同形（0x41ecc3 / 0x41ed22） */
            x += t * nx + bgw_sin(phx + g_phase) * g_amp * ONE_AMPPERSTEP * mul;
            y += t * ny + bgw_sin(phy + g_phase) * g_amp * ONE_AMPPERSTEP * mul;
            z = 0.0f;                        /* 一手 0x41ed45 `[edi+8] = 0` */
            memcpy(p + 0, &x, 4);
            memcpy(p + 4, &y, 4);
            memcpy(p + 8, &z, 4);
            memcpy(&col_u32, p + 16, 4);
            col_u32 = (col_u32 & 0x00ffffffu) | ((unsigned)ONE_ALPHA_INSIDE << 24);
            memcpy(p + 16, &col_u32, 4);
        }
    }
}

/* ================================================================== *
 * 8. hook ①：网格 → VM 顶点缓冲（FUN_0042b540 入口）
 * ================================================================== */

static void hook_upload(x86_reg_t *r)
{
    BYTE *mesh = (BYTE *)(UINT_PTR)r->ecx;
    mesh_stats st;
    int is_stage;

    if (!readable(mesh, 0x1c)) return;
    is_stage = (r->retaddr == (UINT32)(UINT_PTR)(g_base + RVA_UPLOAD_CALLER));

    g_frames++;
    if ((GetAsyncKeyState(g_toggle_vk) & 1)) {
        g_mode = (g_mode + 1) % 4;
        bgw_log("[键] 模式 → %d（0=只看 1=恒等 2=复刻旋涡 3=放大旋涡）", g_mode);
    }
    if ((GetAsyncKeyState(g_mode_vk) & 1)) {
        g_hide_outside = !g_hide_outside;
        bgw_log("[键] 圆外 alpha=0 → %d", g_hide_outside);
    }

    /* ---- 注入必须在引擎拷贝之前（我们就在拷贝函数的入口） ---- */
    if (g_mode >= 2 && (is_stage || g_touch_enemy_mesh)) {
        inject_swirl(mesh, g_mode >= 3 ? g_amp_mul : 1.0f);
        g_phase += g_phase_drift;
        if (g_phase > 6.2831853f) g_phase -= 6.2831853f;
    }

    /* ---- 观测 ---- */
    scan_mesh(mesh, &st);
    if (g_frames == 1) {
        bgw_log("★ hook① 首次命中：mesh=%p 来自%s（ret=%08x）",
                mesh, is_stage ? "舞台 build_grid" : "别的调用者",
                (unsigned)r->retaddr);
        log_mesh("上传前", mesh, &st, r->retaddr);
        bgw_log("  提示：alpha 全是 ff = 这一刻拿到的是**刚建好的规则网格**；"
                "若出现 60/00 说明引擎的逐顶点重算已经写进来了");
    } else if (g_log_every > 0 && (g_frames % (unsigned)g_log_every) == 0) {
        log_mesh("上传前", mesh, &st, r->retaddr);
    }
    if ((GetAsyncKeyState(g_dump_vk) & 1)) {
        int k, n = st.n, m = st.m;
        BYTE *v = *(BYTE **)(mesh + OFF_MESH_VERT);
        bgw_log("---- 顶点转储（%d×%d，28B/顶点）----", n, m);
        if (n > 0 && m > 0 && n <= 64 && m <= 64 && readable(v, (SIZE_T)n * m * VERT_STRIDE)) {
            for (k = 0; k < n * m && k < 24; k++) {
                float x, y, z, w, u, vv;
                unsigned c;
                BYTE *p = v + (SIZE_T)k * VERT_STRIDE;
                memcpy(&x, p + 0, 4); memcpy(&y, p + 4, 4); memcpy(&z, p + 8, 4);
                memcpy(&w, p + 12, 4); memcpy(&c, p + 16, 4);
                memcpy(&u, p + 20, 4); memcpy(&vv, p + 24, 4);
                bgw_log("  [%2d] x=%9.3f y=%9.3f z=%7.3f w=%.1f c=%08x u=%.4f v=%.4f",
                        k, x, y, z, w, c, u, vv);
            }
        }
    }
}

/* ================================================================== *
 * 9. hook ②：[mode==2] 逐顶点重算循环出口 —— 证明"引擎写在了上传之后"
 * ================================================================== */

static void hook_swirl_exit(x86_reg_t *r)
{
    BYTE *stage = *(BYTE **)(UINT_PTR)(r->esp + 0x1c);   /* on_tick_part_2 把 stage 存这儿 */
    BYTE *mesh;
    mesh_stats st;
    static int done;

    if (!readable(stage + OFF_STAGE_MESH, 8)) return;
    mesh = *(BYTE **)(stage + OFF_STAGE_MESH);
    if (!readable(mesh, 0x1c)) return;
    scan_mesh(mesh, &st);
    if (!done) {
        done = 1;
        bgw_log("★ hook② 首次命中：stage=%p mesh=%p mode=%d 半径阈值=%.3f 当前半径=%.3f"
                " ampX=%.4f ampY=%.4f",
                stage, mesh, *(int *)(stage + OFF_STAGE_MODE),
                *(float *)(stage + OFF_STAGE_RADIUS), *(float *)(stage + OFF_STAGE_GROW),
                *(float *)(stage + OFF_STAGE_AMPX), *(float *)(stage + OFF_STAGE_AMPY));
        bgw_log("  ⇒ 这是**同一帧内、上传完成之后**的 mesh 顶点状态：");
    }
    if (done < 4 || (g_frames % 64) == 0)
        log_mesh("重算后", mesh, &st, (unsigned)r->retaddr);
    if (done < 4) done++;
}

/* ================================================================== *
 *10. 装载
 * ================================================================== */

static int sig_ok(const BYTE *rva, const BYTE *sig, size_t n)
{
    return memcmp(g_base + (UINT_PTR)rva, sig, n) == 0;
}

static float ini_f(const char *key, float dflt)
{
    char buf[64], *end;
    double v;
    if (!GetPrivateProfileStringA("warp", key, "", buf, sizeof buf, g_ini) || !buf[0])
        return dflt;
    v = strtod(buf, &end);
    return (end == buf) ? dflt : (float)v;
}

static void load_ini(void)
{
    g_mode          = GetPrivateProfileIntA("warp", "mode", 0, g_ini);
    g_radius        = ini_f("radius", 112.0f);
    g_cx            = ini_f("cx", ONE_CX);
    g_cy            = ini_f("cy", ONE_CY);
    g_amp           = ini_f("amp", 1.0f);
    g_amp_mul       = ini_f("amp_mul", 1.0f);
    g_phase_drift   = ini_f("phase_drift", 0.02f);
    g_hide_outside  = GetPrivateProfileIntA("warp", "hide_outside", 1, g_ini);
    g_touch_enemy_mesh = GetPrivateProfileIntA("warp", "touch_enemy_mesh", 0, g_ini);
    g_log_every     = GetPrivateProfileIntA("warp", "log_every", 64, g_ini);
    g_toggle_vk     = GetPrivateProfileIntA("warp", "toggle_vk", VK_F5, g_ini);
    g_dump_vk       = GetPrivateProfileIntA("warp", "dump_vk", VK_F6, g_ini);
    g_mode_vk       = GetPrivateProfileIntA("warp", "hide_vk", VK_F7, g_ini);
    if (g_mode < 0 || g_mode > 3) g_mode = 0;
    if (g_radius <= 0.0f) g_radius = 112.0f;
}

int __stdcall thcrap_plugin_init(void)
{
    char exe[MAX_PATH], *slash;

    g_base = (BYTE *)GetModuleHandleW(NULL);
    if (!g_base) return 1;

    if (!GetModuleFileNameA(NULL, exe, MAX_PATH)) return 1;
    slash = strrchr(exe, '\\');
    if (slash) *slash = 0;
    _snprintf(g_ini, sizeof g_ini, "%s\\th18_bgwarp.ini", exe);
    {   char lp[MAX_PATH];
        _snprintf(lp, sizeof lp, "%s\\th18_bgwarp.log", exe);
        g_log = fopen(lp, "w");
    }

    bgw_log("=== th18_bgwarp（顶点效果验证 mod）===");
    bgw_log("exe 基址 = %p（不假设 0x400000）", g_base);

    if (!sig_ok((const BYTE *)RVA_MESH_UPLOAD, SIG_UPLOAD, sizeof SIG_UPLOAD) ||
        !sig_ok((const BYTE *)RVA_MESH_BUILD,  SIG_BUILD,  sizeof SIG_BUILD)  ||
        !sig_ok((const BYTE *)0x1e7b4u,        SIG_TICK,   sizeof SIG_TICK)   ||
        !sig_ok((const BYTE *)RVA_SWIRL_EXIT,  SIG_SWIRL,  sizeof SIG_SWIRL)) {
        bgw_log("★ 签名不匹配：这不是本 DLL 认的 th18 v1.00a（md5 9969cac7…）"
                " ⇒ 自我卸载，一个字节都不改");
        return 1;
    }
    bgw_log("签名校验 4/4 通过");

    load_ini();
    bgw_log("配置：mode=%d radius=%.2f center=(%.1f,%.1f) amp=%.3f amp_mul=%.3f "
            "hide_outside=%d enemy_mesh=%d log_every=%d 键 F5=模式 F6=转储 F7=圆外",
            g_mode, g_radius, g_cx, g_cy, g_amp, g_amp_mul,
            g_hide_outside, g_touch_enemy_mesh, g_log_every);

    if (!make_stub(g_base + RVA_MESH_UPLOAD, 7, (void *)hook_upload)) {
        bgw_log("★ hook① 安装失败 ⇒ 自我卸载");
        return 1;
    }
    bgw_log("hook① 0x%x（网格 → VM 顶点缓冲）已装",
            (unsigned)(UINT_PTR)(g_base + RVA_MESH_UPLOAD));

    if (!make_stub(g_base + RVA_SWIRL_EXIT, 6, (void *)hook_swirl_exit)) {
        bgw_log("★ hook② 安装失败（hook① 仍在）—— 继续跑，只是少了这条对照日志");
    } else {
        bgw_log("hook② 0x%x（[mode==2] 重算循环出口）已装",
                (unsigned)(UINT_PTR)(g_base + RVA_SWIRL_EXIT));
    }

    bgw_log("已激活。跑一局带 boss 的战斗，进非符，看日志与本文件同目录的 "
            "th18_bgwarp.log。按 F5 切模式（2/3 = 注入），F6 转储顶点。");
    g_ok = 1;
    return 0;
}

void __stdcall bgwarp_mod_exit(void *param)
{
    (void)param;
    if (g_ok) bgw_log("=== 退出（共 %u 次上传命中）===", g_frames);
    if (g_log) { fclose(g_log); g_log = NULL; }
}
