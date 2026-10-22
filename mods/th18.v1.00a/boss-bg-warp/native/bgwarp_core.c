/*
 * route_b.c —— 「路线 B：我们自己画」自包含实现（TH18 v1.00a）
 *
 * 为什么单独一个文件：主文件 th18_bgwarp.c 由**另一个并行 agent** 维护；
 * 这里走"自己的文件 + 自己的日志 + 自己的桩"，只在 Makefile 里加一行源文件，避免互相覆盖。
 *
 * 做法（一手依据见 boss-bg-warp/AUDIT.md §O 与 README §5.2b~§5.3b）：
 *   ① 替换「层 6 绘制桩」`0x487780`（叶子函数 `6A 06 E8 rel32 C3`，被调 `render_layer=0x488260`）——
 *      用**替换型桩**：不搬任何含相对寻址的字节，桩尾用 `mov eax,imm32; call eax` 复刻原动作；
 *      它是"背景已合成、精灵层尚未绘制"的最早时机 ⇒ 采到的就是背景。
 *   ② 自己 `DrawPrimitiveUP` 画：
 *      · SetTexture   → device vtable +0x104（index 65）
 *      · SetFVF(0x144)→ +0x164（index 89；XYZRHW|DIFFUSE|TEX1）
 *      · 状态         → +0x10c(0,6,0)（引擎原样）＋ 临时关 ZENABLE/ZWRITEENABLE（画完恢复）
 *      · DrawPrimitiveUP → +0x14c（index 83）
 *      顶点格式 = 28 B {x,y,z=0,rhw=1,color,u,v}（与 build_grid 一手一致）。
 *   ③ 采样源 = 离屏面（`[0x4ccfa0]` / `[0x4ccf9c]`），`GetContainer`（vtable +0x2C）拿纹理。
 *   ④ 世界→屏幕：用**实测视口** `GetViewport`（+0xC0）；世界 x∈[−192,192]、y∈[0,448]。
 *
 * 按键：8 = 循环模式（0 关 / 1 M1 整屏平移 / 2 M2 扭曲网格 / 3 光晕贴图测试）；9 = 双份（第二份偏移）
 * ini ：同目录 th18_routeb.ini（没有就用内置默认值）。日志：同目录 th18_routeb.log。
 */

#include "bgwarp_core.h"

#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <math.h>


/* ---- 地址（全部一手） ---- */
#define RVA_DEVICE        0xccdf8u    /* [0x4ccdf8] = IDirect3DDevice9* */
#define RVA_SURF_A        0xccf9cu    /* 离屏面 A */
#define RVA_SURF_B        0xccfa0u    /* 离屏面 B（默认为采样源） */
#define RVA_PLAYER_PTR    0xcf410u    /* [0x4cf410] = 自机对象 */
#define RVA_ANM_MGR       0x11f65cu   /* [0x51f65c]：anm 管理器（VM 链表与贴图表都在它身上） */
#define RVA_LAYER6_STUB   0x87780u    /* 层 6 绘制桩：6A 06 E8 rel32 C3 */
#define RVA_RENDER_LAYER  0x88260u    /* 桩里被调用的 render_layer */

#define PL_OFF_X          0x620u      /* 自机世界坐标 x */
#define PL_OFF_Y          0x624u

#define VM_LIST_1         0x6f0u      /* [mgr+0x6f0] / [mgr+0x6f8]：两条 anm VM 链表（节点 {vm,+4=next}） */
#define VM_LIST_2         0x6f8u

#define STUB_MAX          256   /* ★ 替换型桩 ≈120 字节；64 是旧"跳板"尺寸，会默默失败 */

/* ---- 状态 ---- */
static BYTE    *rb_base;

static int      rb_mode;              /* 键 8 循环：0 关 1 M1 2 M2 3 光晕贴图 */
static int      rb_two;               /* 键 9：是否再画第二份（偏移由 ini 给，用来验证"两边各画一份"） */
static int      rb_surf;              /* ini surf：0=[0x4ccfa0] 1=[0x4ccf9c] */
static int      rb_n        = 33;     /* 网格分辨率（奇数 3..65） */
static float    rb_shift    = 64.0f;  /* M1 平移像素 */
static float    rb_fade     = 0.15f;  /* 圆盘外圈羽化比例 */
static float    rb_radius   = 160.0f; /* 网格半径（世界单位） */
/*
 * ini radius_auto：**默认 0**。
 *   ★ 为什么默认关（owner 2026-10-21 指出）：boss 的 VM 网格**随时可能创建/销毁**（光环出现/消失），
 *     若我方半径跟着它走，半径会随光环出现而**跳变** ✗。我方那个盘必须**只由自己的 ini 决定** ⇒
 *     "boss 不出现也照常工作"。
 */
static int      rb_radius_auto;
static float    rb_power    = 1.0f;   /* 位移倍数 */
static unsigned rb_alpha    = 255;    /* 圆内 alpha */
static float    rb_two_off  = 256.0f;
/*
 * ★★ 重叠区怎么"融合好"（owner 的口径：两者**本质独立**，只在**同时出现**时要融合）：
 *   我们会采到"已经被 boss 扭过"的画面 ⇒ 若两个盘都全强度，重叠处就是**两次采样**（双影/糊）。
 *   做法：**软性分域** —— 靠近 boss 那个盘时，我们的**位移与 alpha 平滑收到 0**（让 boss 主导），
 *   离开它的边界后再回到全强度。两个效果各自独立成立，重叠处只留一份主导 ✓
 *   `overlap = 0` 关掉（完全不看 boss），`blend` = 过渡带宽度（世界单位）。
 */
static int      rb_overlap   = 1;
static float    rb_blend     = 40.0f;
static float    rb_ov_floor  = 0.4f;
/*
 * ★ avoid_rt（默认 **0**）：是否在"采的那张 == 当前渲染目标"时自动换另一张。
 *   owner 实测：开了这个之后**模式 2 出现重影** ✗ —— 因为换过去的那张面装的是别的内容/上一帧合成
 *   ⇒ 圆盘里显示的画面与周围错开 ⇒ 双影。默认关掉后回到"很自然"那版的观感（只采 [0x4ccfa0]）✓
 *   只有在确实看到"越套越小"的递归时才把它设 1。
 */
static int      rb_avoid_rt;    /* 重叠区内保留的比例（0=完全让位，1=不让位） */
static int      rb_ov_ok;             /* 缓存：敌方盘是否有效 */
static float    rb_ov_cx, rb_ov_cy, rb_ov_r; /* 第二份的圆心相对偏移（屏幕像素，仅用于验证） */
static unsigned rb_log_every = 300;   /* 每隔多少次画一行日志 */

static int      rb_stub_ok;
static int      rb_logged_desc, rb_logged_grid, rb_logged_halo, rb_logged_enemy, rb_logged_pts, rb_logged_entry, rb_logged_vms, rb_logged_rt, rb_logged_nohalo, rb_logged_baddev, rb_logged_ov, rb_logged_src, rb_logged_lost;
static unsigned rb_rescan_fail;
static int      rb_dumped_all;
static int      rb_test_no_overlap;   /* 测试模式（3/4/5）临时关掉重叠分域，免得遮住要看的画面 */
static unsigned rb_frames, rb_drawn;
static void    *rb_last_dev;
static unsigned rb_dev_settle;          /* 设备指针稳定了多少帧（Reset 后先冷却） */
static void    *rb_last_good_surf;      /* 上次成功取过纹理的那张面 */
static unsigned rb_vp_ok;
static float    rb_vp_x = 436.0f, rb_vp_y = 244.0f, rb_vp_w = 408.0f, rb_vp_h = 472.0f;
static void    *rb_scene_tex;         /* 场景面纹理（缓存） */
static void    *rb_halo_tex;          /* ★ 光晕 sprite 的纹理（从引擎那份网格的 VM 解出来） */
static float    rb_phx, rb_phy;       /* 我们的相位累加器 */

#define RB_PI   3.14159265f
#define RB_2PI  6.28318531f
#define RB_K_RAD 32.0f                /* 一手 [0x4b92b4] */
#define RB_K_SIN  8.0f                /* 一手 [0x4b9268] */
#define RB_PH_VX 0.098175f            /* 一手 [0x4b90d4] π/32 */
#define RB_PH_VY 0.049087f            /* 一手 [0x4b90c0] π/64 */
#define RB_PH_FX 0.196350f            /* 一手 [0x4b90ec] π/16 */
#define RB_PH_FY 0.098175f

/* ================= 小工具 ================= */

/* ★ 日志交给宿主（试验台写文件；卡侧接自己的日志）。默认没有回调 ⇒ 静默。 */
static bgwarp_log_fn bw_logcb;
static void (*bw_tick)(void);   /* 宿主的每帧回调（试验台用它轮询热键） */
static void rb_logf(const char *fmt, ...)
{
    va_list ap;
    if (!bw_logcb) return;
    va_start(ap, fmt);
    {
        char buf[1024];
        _vsnprintf(buf, sizeof buf, fmt, ap);
        buf[sizeof buf - 1] = 0;
        bw_logcb("%s", buf);
    }
    va_end(ap);
}



static BOOL rb_ok(const void *p, SIZE_T len, int need_write)
{
    MEMORY_BASIC_INFORMATION mbi;
    const BYTE *cur = (const BYTE *)p;
    UINT_PTR end;
    if (!p) return FALSE;
    if ((UINT_PTR)p < 0x10000u) return FALSE;
    if ((UINT_PTR)p + len < (UINT_PTR)p) return FALSE;      /* 回绕 */
    end = (UINT_PTR)p + len;
    while ((UINT_PTR)cur < end) {
        if (!VirtualQuery(cur, &mbi, sizeof(mbi))) return FALSE;
        if (mbi.State != MEM_COMMIT) return FALSE;
        if (mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD)) return FALSE;
        if (need_write && !(mbi.Protect & (PAGE_READWRITE | PAGE_WRITECOPY |
                                           PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY)))
            return FALSE;
        cur = (const BYTE *)mbi.BaseAddress + mbi.RegionSize;
    }
    return TRUE;
}
static BOOL rb_rd(const void *p, SIZE_T n) { return rb_ok(p, n, 0); }

/*
 * ★★ 判断"这个函数指针能不能 call"：必须是**已映射模块（MEM_IMAGE）里的可执行页**。
 *   为什么必须有这一条（2026-10-17 血案，dump th18.exe.37324.dmp）：
 *   加载/标题阶段的 `[0x4ccdf8]` 里可能是**过期/无效的设备指针**，它的"vtable"是堆上的垃圾；
 *   我只校验了"vtable 内存可读"就调了 `vt[0x3a]` ⇒ **跳进堆地址执行 ⇒ 闪退**
 *   （Eip=0x90a66dc 不在任何模块、Ebp 落在本 DLL 里、栈上是层 6 桩）。
 *   加这一条后，任何"设备/接口已失效"的调用都会被挡在门外（宁可不画）。
 */
static BOOL rb_code(const void *p)
{
    MEMORY_BASIC_INFORMATION mbi;
    DWORD exe = PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;
    if (!p || (UINT_PTR)p < 0x10000u) return FALSE;
    if (!VirtualQuery(p, &mbi, sizeof(mbi))) return FALSE;
    if (mbi.State != MEM_COMMIT) return FALSE;
    if (mbi.Type != MEM_IMAGE) return FALSE;                 /* ★ 必须属于某个映射进来的模块（d3d9/nvd3dum/exe…） */
    if (!(mbi.Protect & exe)) return FALSE;
    return TRUE;
}
static BOOL rb_wr(const void *p, SIZE_T n) { return rb_ok(p, n, 1); }







/* ================= 替换型桩（层 6） ================= */

static BYTE  bw_orig[16];             /* 替换前的原始字节（供 bgwarp_restore） */
static BYTE *bw_target;
static int   bw_patch_len;

static BYTE *rb_make_replace_stub(BYTE *target, int len, int layer, void *render_layer, void *fn)
{
    BYTE *stub = (BYTE *)VirtualAlloc(NULL, STUB_MAX, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    int i = 0, k;
    UINT_PTR lay = (UINT_PTR)render_layer, f = (UINT_PTR)fn;
    if (!stub) { rb_logf("★ VirtualAlloc 失败 err=%lu", GetLastError()); return NULL; }

    stub[i++] = 0x60;                                              /* pushad */
    stub[i++] = 0x9c;                                              /* pushfd */
    stub[i++] = 0x89; stub[i++] = 0xe3;                            /* mov ebx,esp */
    stub[i++] = 0x8d; stub[i++] = 0x6c; stub[i++] = 0x24; stub[i++] = 0x80;  /* lea ebp,[esp-0x80] */
    stub[i++] = 0x89; stub[i++] = 0xec;                            /* mov esp,ebp */
    for (k = 0; k < 8; k++) {                                      /* 保存 xmm0-7 */
        stub[i++] = 0x0f; stub[i++] = 0x11;
        stub[i++] = (BYTE)(0x44 | (k << 3));
        stub[i++] = 0x24;
        stub[i++] = (BYTE)(k * 0x10);
    }
    stub[i++] = 0x83; stub[i++] = 0xe4; stub[i++] = 0xf0;          /* and esp,-16 */
    stub[i++] = 0x53;                                              /* push ebx */
    stub[i++] = 0xb8; memcpy(stub + i, &f, 4); i += 4;             /* mov eax, fn */
    stub[i++] = 0xff; stub[i++] = 0xd0;                            /* call eax */
    stub[i++] = 0x83; stub[i++] = 0xc4; stub[i++] = 0x04;          /* add esp,4 */
    for (k = 0; k < 8; k++) {                                      /* 还原 xmm0-7 */
        stub[i++] = 0x0f; stub[i++] = 0x10;
        stub[i++] = (BYTE)(0x45 | (k << 3));
        stub[i++] = (BYTE)(k * 0x10);
    }
    stub[i++] = 0x89; stub[i++] = 0xdc;                            /* mov esp,ebx */
    stub[i++] = 0x9d;                                              /* popfd */
    stub[i++] = 0x61;                                              /* popad */
    stub[i++] = 0x6a; stub[i++] = (BYTE)layer;                     /* push layer（复刻原桩） */
    stub[i++] = 0xb8; memcpy(stub + i, &lay, 4); i += 4;           /* mov eax, render_layer */
    stub[i++] = 0xff; stub[i++] = 0xd0;                            /* call eax */
    stub[i++] = 0xc3;                                              /* ret */
    if (i > STUB_MAX) {
        rb_logf("★ 桩太长（%d > %d）⇒ 放弃（把 STUB_MAX 调大）", i, (int)STUB_MAX);
        VirtualFree(stub, 0, MEM_RELEASE);
        return NULL;
    }
    FlushInstructionCache(GetCurrentProcess(), stub, i);

    {   BYTE patch[16]; DWORD old0;
        INT32 rel = (INT32)((UINT_PTR)stub - ((UINT_PTR)target + 5));
        if (len < 5 || len > (int)sizeof patch) {
            rb_logf("★ 替换长度不合法（%d）", len);
            VirtualFree(stub, 0, MEM_RELEASE);
            return NULL;
        }
        memcpy(bw_orig, target, len);      /* ★ 先存原始字节 */
        bw_target = target; bw_patch_len = len;
        memset(patch, 0x90, len);
        patch[0] = 0xe9;
        memcpy(patch + 1, &rel, 4);
        if (!VirtualProtect(target, len, PAGE_EXECUTE_READWRITE, &old0)) {
            rb_logf("[桩] VirtualProtect 失败 @%p err=%lu", target, GetLastError());
            VirtualFree(stub, 0, MEM_RELEASE);
            return NULL;
        }
        memcpy(target, patch, len);
        VirtualProtect(target, len, old0, &old0);
        FlushInstructionCache(GetCurrentProcess(), target, len);
        rb_logf("[桩] 替换 @%p -> %p（层 %d, render_layer=%p）回读=%02x %02x %02x %02x %02x",
                target, stub, layer, render_layer, target[0], target[1], target[2], target[3], target[4]);
    }
    return stub;
}

/* ================= 纹理解析 ================= */

/* 从离屏面拿纹理（IDirect3DSurface9::GetContainer = vtable +0x2C） */
static const unsigned char RB_IID_TEX[16] = {
    0x27, 0x12, 0xc3, 0x85, 0xe5, 0x3d, 0x00, 0x4f,
    0x9b, 0x3a, 0xf1, 0x1a, 0xc3, 0x8c, 0x18, 0xb5
};
/*
 * ★★ 面的 vtable 也必须过 `rb_code()` —— 这是 2026-10-21 血案的直接原因：
 *   上一版这里只做了 `rb_rd(vt[11], 1)`（可读）就 `call`，而"过期面"的 vtable 槽是**堆上的值** ⇒
 *   `Eip` 跳到堆地址（dump th18.exe.84840.dmp：Eip=0x8e43efc 不在模块、栈上是我们的层 6 桩）。
 */
static BOOL rb_surf_ready(void *surf)
{
    void **svt;
    if (!surf || !rb_rd(surf, 0x40)) return FALSE;
    svt = *(void ***)surf;
    if (!svt || !rb_rd(svt + 13, 4)) return FALSE;
    if (!rb_code((const void *)svt[11])) return FALSE;   /* GetContainer */
    if (!rb_code((const void *)svt[12])) return FALSE;   /* GetDesc */
    return TRUE;
}

static void *rb_surface_tex(void *surf)
{
    void *tex = NULL;
    if (!rb_surf_ready(surf)) return NULL;
    {   void **vt = *(void ***)surf;
        ((long (__stdcall *)(void *, const void *, void **))vt[11])(surf, RB_IID_TEX, &tex);
    }
    return tex;
}

/*
 * ★ 把「精灵序号」解成 `IDirect3DTexture9*`（一手链，见 AUDIT §O / 0x4807c4）：
 *   group = [vm+0x20] ; idx = [vm+0x24]
 *   p1 = [[0x51f65c] + group*4 + 0x312072c] ; p2 = [p1+0x11c]
 *   handle = [p2 + idx*17*4 + 8]                    （条目步长 68）
 *   g2 = handle>>8 ; i2 = handle&0xff
 *   p3 = [[0x51f65c] + g2*4 + 0x312072c] ; p4 = [p3+0x124]
 *   tex = [p4 + i2*0x18]
 * 每一步都先校验可读；解不出来就把卡在哪一步写进日志（绝不硬调）。
 */
static float rb_uv_u0, rb_uv_v0, rb_uv_du = 1.0f, rb_uv_dv = 1.0f;   /* sprite 的 UV 矩形（条目 +36..+48） */
static int   rb_uv_ok;

static void *rb_tex_of_vm(BYTE *vm, const char *tag)
{
    BYTE *mgr;
    BYTE *p1, *p2, *p3, *p4;
    unsigned grp, idx, handle, g2, i2;
    void *tex;
    if (!vm || !rb_rd(vm, 0x600)) { rb_logf("[贴图] %s：VM 不可读", tag); return NULL; }
    if (!rb_rd(rb_base + RVA_ANM_MGR, 4)) return NULL;
    mgr = *(BYTE **)(rb_base + RVA_ANM_MGR);
    if (!mgr) return NULL;
    grp = *(unsigned *)(vm + 0x20);
    idx = *(unsigned *)(vm + 0x24);
    if (!rb_rd(mgr + (SIZE_T)grp * 4 + 0x312072c, 4)) {
        rb_logf("[贴图] %s：第1步不可读（组表 mgr+%08x；组=%u 索引=%u）",
                tag, (unsigned)((SIZE_T)grp * 4 + 0x312072c), grp, idx);
        return NULL;
    }
    p1 = *(BYTE **)(mgr + (SIZE_T)grp * 4 + 0x312072c);
    if (!p1 || !rb_rd(p1 + 0x11c, 4)) { rb_logf("[贴图] %s：第2步 p1=%p", tag, (void *)p1); return NULL; }
    p2 = *(BYTE **)(p1 + 0x11c);
    if (!p2 || !rb_rd(p2 + (SIZE_T)idx * 68 + 8, 4)) {
        rb_logf("[贴图] %s：第3步 p2=%p 索引=%u", tag, (void *)p2, idx);
        return NULL;
    }
    handle = *(unsigned *)(p2 + (SIZE_T)idx * 68 + 8);
    if (!handle) { rb_logf("[贴图] %s：第4步 handle=0", tag); return NULL; }
    g2 = handle >> 8; i2 = handle & 0xff;
    if (!rb_rd(mgr + (SIZE_T)g2 * 4 + 0x312072c, 4)) {
        rb_logf("[贴图] %s：第5步不可读（handle=%08x 组=%u）", tag, handle, g2);
        return NULL;
    }
    p3 = *(BYTE **)(mgr + (SIZE_T)g2 * 4 + 0x312072c);
    if (!p3 || !rb_rd(p3 + 0x124, 4)) { rb_logf("[贴图] %s：第6步 p3=%p", tag, (void *)p3); return NULL; }
    p4 = *(BYTE **)(p3 + 0x124);
    if (!p4 || !rb_rd(p4 + (SIZE_T)i2 * 0x18, 4)) { rb_logf("[贴图] %s：第7步 p4=%p", tag, (void *)p4); return NULL; }
    tex = *(void **)(p4 + (SIZE_T)i2 * 0x18);
    rb_logf("[贴图] %s：组=%u 索引=%u → handle=%08x → 纹理 %p", tag, grp, idx, handle, tex);
    /* ★ 顺手记下 sprite 的 UV 矩形：条目 +36/+40/+44/+48 是**预计算好**的 u0,v0,du,dv
     *   （实跑一手：x=116 y=4 w=524 h=476、图集 960×1280 ⇒ u0=116/1280 v0=4/960 du=524/1280 dv=476/960） */
    {   BYTE *e = p2 + (SIZE_T)idx * 68;
        float u0 = *(float *)(e + 0x24), v0 = *(float *)(e + 0x28);
        float du = *(float *)(e + 0x2c), dv = *(float *)(e + 0x30);
        if (du > 0.0f && du <= 1.0f && dv > 0.0f && dv <= 1.0f &&
            u0 >= 0.0f && u0 < 1.0f && v0 >= 0.0f && v0 < 1.0f) {
            rb_uv_u0 = u0; rb_uv_v0 = v0; rb_uv_du = du; rb_uv_dv = dv; rb_uv_ok = 1;
            rb_logf("[光晕] sprite UV 矩形 = u0=%.4f v0=%.4f du=%.4f dv=%.4f", (double)u0, (double)v0, (double)du, (double)dv);
        }
    }
    /*
     * ★ 把那条 68 字节的 sprite 条目**原样转储**：贴图里的 u/v/w/h（sprite 矩形）就在里面，
     *   字段位置未知 ⇒ 先看数（float 与 int 各解一遍），才能把"光晕 sprite 的 UV 矩形"写对。
     */
    if (!rb_logged_entry) {
        int off;
        rb_logged_entry = 1;
        for (off = 0; off + 4 <= 68; off += 4) {
            unsigned u32v = *(unsigned *)(p2 + (SIZE_T)idx * 68 + off);
            float f;
            memcpy(&f, &u32v, 4);
            rb_logf("[条目] +%02d: u32=%08x (%u)  float=%g", off, u32v, u32v, (double)f);
        }
    }
    return tex;
}

/*
 * ★ 在两条 anm VM 链表里找一台"网格段 VM"（判据：`+0x5c8` 非空、`+0x5cc == 行数*56`），
 *   用它去解光晕贴图。链表是引擎的 ⇒ **必须封顶**（曾经因为不封顶卡死过）。
 *   返回找到的 VM 指针（同时把网格的 `+0x10` 数组里第一台留作样本）。
 */
/* 把所有"网格段 VM"的 handle 都解一遍（最多 8 台），用来挑出真正的那张 sprite */
static void rb_dump_all_candidates(void)
{
    BYTE *mgr;
    int li, shown = 0;
    if (!rb_rd(rb_base + RVA_ANM_MGR, 4)) return;
    mgr = *(BYTE **)(rb_base + RVA_ANM_MGR);
    if (!mgr) return;
    for (li = 0; li < 2; li++) {
        BYTE *node;
        int budget = 4096;
        if (!rb_rd(mgr + VM_LIST_1 + li * 8, 4)) continue;
        node = *(BYTE **)(mgr + VM_LIST_1 + li * 8);
        while (node && budget-- > 0 && shown < 8) {
            BYTE *vm;
            if (!rb_rd(node, 8)) break;
            vm   = *(BYTE **)(node + 0);
            node = *(BYTE **)(node + 4);
            if (!vm || !rb_rd(vm, 0x600)) continue;
            {   unsigned buf = *(unsigned *)(vm + 0x5cc);
                if (*(void **)(vm + 0x5c8) && buf >= 34 * 28 && buf <= 64 * 28 && (buf % 56) == 0) {
                    char tag[48];
                    float su0 = rb_uv_u0, sv0 = rb_uv_v0, sdu = rb_uv_du, sdv = rb_uv_dv;
                    int   sok = rb_uv_ok;
                    shown++;
                    _snprintf(tag, sizeof tag, "候选%d", shown);
                    rb_tex_of_vm(vm, tag);            /* 会打 [贴图] 与 [光晕] sprite UV 矩形 */
                    rb_uv_u0 = su0; rb_uv_v0 = sv0; rb_uv_du = sdu; rb_uv_dv = sdv; rb_uv_ok = sok;
                }
            }
        }
    }
}

static BYTE *rb_find_mesh_vm(int *out_cols)
{
    BYTE *mgr;
    int li;
    if (out_cols) *out_cols = 0;
    if (!rb_rd(rb_base + RVA_ANM_MGR, 4)) return NULL;
    mgr = *(BYTE **)(rb_base + RVA_ANM_MGR);
    if (!mgr) return NULL;
    for (li = 0; li < 2; li++) {
        BYTE *node;
        int budget = 8192;
        if (!rb_rd(mgr + VM_LIST_1 + li * 8, 4)) continue;
        node = *(BYTE **)(mgr + VM_LIST_1 + li * 8);
        while (node && budget-- > 0) {
            BYTE *vm;
            if (!rb_rd(node, 8)) break;
            vm   = *(BYTE **)(node + 0);
            node = *(BYTE **)(node + 4);
            if (!vm || !rb_rd(vm, 0x600)) continue;
            {   unsigned buf = *(unsigned *)(vm + 0x5cc);
                if (*(void **)(vm + 0x5c8) && buf >= 34 * 28 && buf <= 64 * 28 && (buf % 56) == 0) {
                    unsigned h  = *(unsigned *)(vm + 0x544);      /* 认领/贴图句柄 */
                    unsigned sp = *(unsigned *)(vm + 0x560);      /* sprite 序号 */
                    float   sc  = *(float *)(vm + 0x568);         /* 缩放 ≈ 光环半径 */
                    unsigned is_screen = ((h & 0x7fffu) == 0x7fffu);
                    if (rb_logged_vms < 8) {
                        rb_logged_vms++;
                        rb_logf("[候选%d] VM=%p cols=%u handle=%08x(屏幕特供=%u) sprite=%u scale=%.1f",
                                rb_logged_vms, vm, buf / 56, h, is_screen, sp, (double)sc);
                    }
                    /* ★ 屏幕特供那支 = 关卡那份网格（它的"纹理"就是屏幕 ⇒ 采自己 ⇒ 递归）⇒ 跳过 */
                    if (!is_screen && sc > 16.0f && sc < 2048.0f) {
                        if (out_cols) *out_cols = (int)(buf / 56);
                        return vm;
                    }
                }
            }
        }
    }
    return NULL;
}

/*
 * ★★ 找"敌方那个圆盘"在哪、多大 —— **完全不碰引擎内部结构**：
 *   网格段 VM 的 `+0x5c8` 缓冲里装的就是它**这一帧的顶点**（28 B/顶点，前两个 float 是位置，
 *   坐标是"网格空间" = 世界 + (640,256)）⇒ 把所有这类 VM 的缓冲扫一遍取包围盒
 *   ⇒ 中心/半径就出来了。链表是引擎的 ⇒ **必须封顶**。
 */
static int rb_enemy_disc(float *cx, float *cy, float *R, int *n_vm)
{
    BYTE *mgr;
    float minx = 1e30f, miny = 1e30f, maxx = -1e30f, maxy = -1e30f;
    int li, cnt = 0;
    if (n_vm) *n_vm = 0;
    if (!rb_rd(rb_base + RVA_ANM_MGR, 4)) return 0;
    mgr = *(BYTE **)(rb_base + RVA_ANM_MGR);
    if (!mgr) return 0;
    for (li = 0; li < 2; li++) {
        BYTE *node;
        int budget = 8192;
        if (!rb_rd(mgr + VM_LIST_1 + li * 8, 4)) continue;
        node = *(BYTE **)(mgr + VM_LIST_1 + li * 8);
        while (node && budget-- > 0) {
            BYTE *vm;
            if (!rb_rd(node, 8)) break;
            vm   = *(BYTE **)(node + 0);
            node = *(BYTE **)(node + 4);
            if (!vm || !rb_rd(vm, 0x600)) continue;
            {   unsigned sz = *(unsigned *)(vm + 0x5cc);
                BYTE *buf = *(BYTE **)(vm + 0x5c8);
                int k, nv;
                if (!buf || sz < 34 * 28 || sz > 64 * 28 || (sz % 56) != 0) continue;
                nv = (int)(sz / 28);
                if (!rb_rd(buf, (SIZE_T)nv * 28)) continue;
                for (k = 0; k < nv; k++) {
                    float x = *(float *)(buf + k * 28 + 0);
                    float y = *(float *)(buf + k * 28 + 4);
                    if (!(x == x) || !(y == y)) continue;
                    if (fabsf(x) > 4000.0f || fabsf(y) > 4000.0f) continue;
                    if (x < minx) minx = x;
                    if (x > maxx) maxx = x;
                    if (y < miny) miny = y;
                    if (y > maxy) maxy = y;
                }
                cnt++;
            }
        }
    }
    if (!cnt || maxx <= minx) return 0;
    *cx = 0.5f * (minx + maxx) - 640.0f;         /* 网格空间 → 世界 */
    *cy = 0.5f * (miny + maxy) - 256.0f;
    *R  = 0.5f * ((maxx - minx) > (maxy - miny) ? (maxx - minx) : (maxy - miny));
    if (n_vm) *n_vm = cnt;
    return 1;
}

/*
 * ★★ 更稳的取样路径：**用 anm 的"屏幕特供" VM 的纹理**，而不是 `GetContainer(离屏面)`。
 *   为什么：设备 Reset/重建后，`[0x4ccfa0]` 那两张**面指针会过期**（已被释放），
 *   而 `GetContainer` 在过期面上会让 d3d9 访问空指针 ⇒ 闪退（dump th18.exe.35256.dmp：Eip 在 d3d9 内部）。
 *   屏幕特供 VM（`handle & 0x7fff == 0x7fff`）的纹理由**游戏自己**维护、Reset 后自动换成新纹理 ✓
 *   所以优先走它；找不到再退回 `GetContainer`（那边有"设备稳定期"保护）。
 */
static void *rb_screen_tex_from_vm(void)
{
    BYTE *mgr;
    int li;
    if (!rb_rd(rb_base + RVA_ANM_MGR, 4)) return NULL;
    mgr = *(BYTE **)(rb_base + RVA_ANM_MGR);
    if (!mgr) return NULL;
    for (li = 0; li < 2; li++) {
        BYTE *node;
        int budget = 4096;
        if (!rb_rd(mgr + VM_LIST_1 + li * 8, 4)) continue;
        node = *(BYTE **)(mgr + VM_LIST_1 + li * 8);
        while (node && budget-- > 0) {
            BYTE *vm;
            if (!rb_rd(node, 8)) break;
            vm   = *(BYTE **)(node + 0);
            node = *(BYTE **)(node + 4);
            if (!vm || !rb_rd(vm, 0x600)) continue;
            {   unsigned h = *(unsigned *)(vm + 0x544);
                if ((h & 0x7fffu) == 0x7fffu)
                    return rb_tex_of_vm(vm, "屏幕特供VM");
            }
        }
    }
    return NULL;
}

/*
 * ★ 诊断：把两条 anm VM 链里**实际有什么**打出来（不按网格判据过滤）。
 *   用途：boss 光环在场却"找不到网格段 VM"时，分清是
 *     ① 链里根本没有那些 VM（⇒ 得从 mesh 的 `+0x10` 数组找，不是链）
 *     ② 在链里，但 `+0x5c8/+0x5cc` 的样子和我猜的不同
 *   封顶 4096，绝不无界。
 */
static void rb_dump_vms(const char *why)
{
    BYTE *mgr;
    int li, shown = 0, total = 0;
    if (!rb_rd(rb_base + RVA_ANM_MGR, 4)) return;
    mgr = *(BYTE **)(rb_base + RVA_ANM_MGR);
    if (!mgr) return;
    for (li = 0; li < 2; li++) {
        BYTE *node;
        int budget = 4096;
        if (!rb_rd(mgr + VM_LIST_1 + li * 8, 4)) continue;
        node = *(BYTE **)(mgr + VM_LIST_1 + li * 8);
        while (node && budget-- > 0) {
            BYTE *vm;
            if (!rb_rd(node, 8)) break;
            vm   = *(BYTE **)(node + 0);
            node = *(BYTE **)(node + 4);
            if (!vm || !rb_rd(vm, 0x600)) continue;
            total++;
            if (shown < 10) {
                shown++;
                rb_logf("[VM链%d] vm=%p buf=%p len=%u handle=%08x sprite=%u scale=%.1f pos=(%.1f,%.1f)",
                        li, vm, *(void **)(vm + 0x5c8), *(unsigned *)(vm + 0x5cc),
                        *(unsigned *)(vm + 0x544), *(unsigned *)(vm + 0x560),
                        (double)*(float *)(vm + 0x568),
                        (double)*(float *)(vm + 0x230), (double)*(float *)(vm + 0x234));
            }
        }
    }
    rb_logf("[VM链] %s：共 %d 台（上面列了前 %d 台）", why, total, shown);
}

/* ================= 绘制 ================= */

static void rb_get_vp(void *dev, void **vt)
{
    if (vt && rb_rd(vt + 0x30, 4)) {
        struct { unsigned x, y, w, h; float mnz, mxz; } vp;
        memset(&vp, 0, sizeof vp);
        ((void (__stdcall *)(void *, void *))vt[0x30])(dev, &vp);
        if (vp.w > 16 && vp.h > 16 && vp.w < 4096 && vp.h < 4096) {
            rb_vp_ok = 1;
            rb_vp_x = (float)vp.x; rb_vp_y = (float)vp.y;
            rb_vp_w = (float)vp.w; rb_vp_h = (float)vp.h;
        }
    }
}

/* 世界 → 屏幕（用实测视口；playfield 中心 = 视口中心） */
static void rb_world_to_screen(float wx, float wy, float *sx, float *sy)
{
    *sx = rb_vp_x + rb_vp_w * 0.5f + wx * (rb_vp_w / 384.0f);
    *sy = rb_vp_y + rb_vp_h * 0.5f + (wy - 224.0f) * (rb_vp_h / 448.0f);
}

/* 取自机世界坐标；不在舞台里就返回 0 */
static int rb_player(float *px, float *py)
{
    BYTE *pl;
    if (!rb_rd(rb_base + RVA_PLAYER_PTR, 4)) return 0;
    pl = *(BYTE **)(rb_base + RVA_PLAYER_PTR);
    if (!pl || !rb_rd(pl, PL_OFF_Y + 4)) return 0;
    *px = *(float *)(pl + PL_OFF_X);
    *py = *(float *)(pl + PL_OFF_Y);
    if (!(*px == *px) || !(*py == *py)) return 0;
    if (fabsf(*px) > 2000.0f || fabsf(*py) > 2000.0f) return 0;
    return 1;
}

/* 我们的场：D(P)（世界单位）。两项都乘 t ⇒ 圆边缘位移→0（不撕裂的结构性保证之一） */
static void rb_field(float wx, float wy, float cx, float cy, float R,
                     float phx, float phy, float *dx, float *dy)
{
    float rx = wx - cx, ry = wy - cy;
    float r2 = rx * rx + ry * ry, t, len, nx, ny;
    *dx = 0.0f; *dy = 0.0f;
    if (R <= 1.0f || r2 >= R * R) return;
    t = (R * R - r2) / (R * R);
    len = (float)sqrt((double)(rx * rx + ry * ry));
    nx = (len > 1e-6f) ? rx / len : 0.0f;
    ny = (len > 1e-6f) ? ry / len : 0.0f;
    *dx = t * (RB_K_RAD * nx + RB_K_SIN * (float)sin((double)phx)) * rb_power;
    *dy = t * (RB_K_RAD * ny + RB_K_SIN * (float)sin((double)phy)) * rb_power;
}

/*
 * 画一份网格（世界坐标下的圆盘，采 screen 纹理）：
 *   halo_tex != NULL 时，改用「光晕贴图 + sprite 自身 UV」来画（= 复刻引擎那份网格的观感）
 *   off_x/off_y：屏幕像素偏移（"第二份"用）
 */
static void rb_draw_grid(void *dev, void **vt, void *tex, int use_halo,
                         float cx, float cy, float R, float off_x, float off_y)
{
    static float pts[65 * 65 * 7];
    static float tri[64 * 64 * 6 * 7];
    int NN = rb_n, i, j, t = 0;
    float sx0, sy0, step, phx, phy;

    phx = rb_phx; phy = rb_phy;
    step = 2.0f * R / (float)(NN - 1);
    sx0 = cx - R; sy0 = cy - R;

    for (i = 0; i < NN; i++) {
        for (j = 0; j < NN; j++) {
            float wx = sx0 + (float)i * step;              /* 世界坐标 */
            float wy = sy0 + (float)j * step;
            float dx, dy, pxs, pys, u, v, a, ov = 1.0f;
            unsigned col;
            int n = i * NN + j;

            rb_field(wx, wy, cx, cy, R, phx, phy, &dx, &dy);
            /* ★ 重叠区软性分域：进入"敌方那个盘"的范围 ⇒ 我们的位移平滑收到 0（让 boss 主导） */
            if (rb_overlap && rb_ov_ok && !rb_test_no_overlap) {
                float dxb = wx - rb_ov_cx, dyb = wy - rb_ov_cy;
                float db = (float)sqrt((double)(dxb * dxb + dyb * dyb)) - rb_ov_r;
                /* ★ 让位**不归零**：重叠区内仍保留 `floor` 比例（默认 0.4）——
                 *   owner 实测「boss 附近我们的改动依旧失效」= 我把让位写成了 0 ✗
                 *   现在：里面 = floor，过渡带 = floor → 1，外面 = 1 ⇒ 两边都看得见、boss 仍主导 ✓ */
                if (db <= 0.0f) ov = rb_ov_floor;
                else if (db < rb_blend) ov = rb_ov_floor + (1.0f - rb_ov_floor) * (db / rb_blend);
                dx *= ov; dy *= ov;
            }
            rb_world_to_screen(wx + dx, wy + dy, &pxs, &pys);
            pxs += off_x; pys += off_y;

            if (use_halo == 3 && rb_uv_ok) {
                /* ★ 光晕测试：把 sprite 的 UV 矩形**铺满**圆盘 ⇒ 直接看清那张贴图是什么
                 *   （一手：条目 +36/+40/+44/+48 是预计算好的 u0,v0,du,dv） */
                u = rb_uv_u0 + (float)i / (float)(NN - 1) * rb_uv_du;
                v = rb_uv_v0 + (float)j / (float)(NN - 1) * rb_uv_dv;
            } else if (use_halo == 2) {
                /* ★ 整图 UV（0..1）：用来回答"这张纹理到底是什么" —— 若是光晕，会看到完整的环/光斑 */
                u = (float)i / (float)(NN - 1);
                v = (float)j / (float)(NN - 1);
            } else {
                /* ★ UV 一律照引擎那套：`u = 屏幕位置 / 源面尺寸`（一手 build_grid；源面实测 1280x960）。
                 *   上一版模式 3 我写成 `/(视口×2)` ⇒ 采样频率减半 ⇒ 画面缩到 1/2（用户实跑一眼看出）。
                 *   模式 3 与模式 2 的区别**只应该是纹理**（光晕 vs 场景），UV 规则不该变。 */
                float bu, bv;
                rb_world_to_screen(wx, wy, &bu, &bv);
                u = bu / 1280.0f; v = bv / 960.0f;
            }
            if (u < 0.0f) u = 0.0f;
            if (v < 0.0f) v = 0.0f;
            {   float rx = wx - cx, ry = wy - cy;
                float rr = (float)sqrt((double)(rx * rx + ry * ry));
                a = (float)rb_alpha;
                if (rr >= R) a = 0.0f;
                else if (rb_fade > 0.0f) {
                    float e0 = R * (1.0f - rb_fade);
                    if (rr > e0) a *= 1.0f - (rr - e0) / (R - e0);
                }
                a *= ov;                         /* 重叠区连透明度一起让位 ⇒ 边界不留硬边 */
            }
            col = (((unsigned)a) << 24) | 0x00FFFFFFu;
            pts[n * 7 + 0] = pxs; pts[n * 7 + 1] = pys; pts[n * 7 + 2] = 0.0f; pts[n * 7 + 3] = 1.0f;
            *(unsigned *)(pts + n * 7 + 4) = col;
            pts[n * 7 + 5] = u; pts[n * 7 + 6] = v;
        }
    }
    if (!rb_logged_pts) {
        rb_logged_pts = 1;
        {   float mnx = 1e30f, mny = 1e30f, mxx = -1e30f, mxy = -1e30f;
            int k;
            for (k = 0; k < NN * NN; k++) {
                float x = pts[k * 7 + 0], y = pts[k * 7 + 1];
                if (x < mnx) mnx = x; if (x > mxx) mxx = x;
                if (y < mny) mny = y; if (y > mxy) mxy = y;
            }
            rb_logf("[格点] 屏幕包围盒 x[%.1f,%.1f] y[%.1f,%.1f]（跨度 %.1f×%.1f，视口 %.0fx%.0f）",
                    (double)mnx, (double)mxx, (double)mny, (double)mxy,
                    (double)(mxx - mnx), (double)(mxy - mny), (double)rb_vp_w, (double)rb_vp_h);
            rb_logf("[格点] #0=(%.1f,%.1f) uv=(%.4f,%.4f) a=%u ｜ #1=(%.1f,%.1f) uv=(%.4f,%.4f) ｜ #%d(中心)=(%.1f,%.1f)",
                    (double)pts[0], (double)pts[1], (double)pts[5], (double)pts[6],
                    (unsigned)(*(unsigned *)(pts + 4) >> 24),
                    (double)pts[7], (double)pts[8], (double)pts[12], (double)pts[13],
                    (NN * NN) / 2, (double)pts[((NN * NN) / 2) * 7], (double)pts[((NN * NN) / 2) * 7 + 1]);
        }
    }
    for (i = 0; i < NN - 1; i++) {
        for (j = 0; j < NN - 1; j++) {
            int a0 = i * NN + j, b0 = (i + 1) * NN + j, c0 = i * NN + (j + 1), d0 = (i + 1) * NN + (j + 1);
            int seq[6];
            int q;
            seq[0] = a0; seq[1] = b0; seq[2] = c0;
            seq[3] = b0; seq[4] = d0; seq[5] = c0;
            for (q = 0; q < 6; q++) {
                memcpy(tri + (SIZE_T)t * 7, pts + (SIZE_T)seq[q] * 7, 7 * sizeof(float));
                t++;
            }
        }
    }
    (void)tex;
    ((void (__stdcall *)(void *, unsigned, unsigned, const void *, unsigned))vt[0x53])
        (dev, 4u, (unsigned)((NN - 1) * (NN - 1) * 2), tri, 28u);
    (void)phx; (void)phy;
}

/* M1：整屏（只覆盖平移后的那块），UV 平移 */
static void rb_draw_quad(void *dev, void **vt, void *tex, float off_x, float off_y)
{
    float v[4 * 7];
    int k;
    float xs[4] = { rb_shift, 1280.0f, rb_shift, 1280.0f };
    float ys[4] = { 0.0f, 0.0f, 960.0f, 960.0f };
    for (k = 0; k < 4; k++) {
        v[k * 7 + 0] = xs[k] + off_x;
        v[k * 7 + 1] = ys[k] + off_y;
        v[k * 7 + 2] = 0.0f;
        v[k * 7 + 3] = 1.0f;
        *(unsigned *)(v + k * 7 + 4) = 0xffffffffu;
        v[k * 7 + 5] = (xs[k] - rb_shift) / 1280.0f;
        v[k * 7 + 6] = ys[k] / 960.0f;
    }
    (void)tex;
    ((void (__stdcall *)(void *, unsigned, unsigned, const void *, unsigned))vt[0x53])(dev, 5u, 2u, v, 28u);
}

/* D3D9 常量（标准值） */
#define RB_RS_ALPHABLENDENABLE 27u
#define RB_RS_SRCBLEND         19u
#define RB_RS_DESTBLEND        20u
#define RB_BLEND_SRCALPHA       5u
#define RB_BLEND_INVSRCALPHA    6u
#define RB_TSS_COLOROP          1u
#define RB_TSS_COLORARG1        2u
#define RB_TSS_ALPHAOP          4u
#define RB_TSS_ALPHAARG1        5u
#define RB_TSS_ALPHAARG2        6u
#define RB_TOP_SELECTARG1       2u
#define RB_TA_TEXTURE           2u
#define RB_TA_DIFFUSE           0u

/*
 * ★★ 递归（画面里出现"自己"）的根因：我们**采的那张面就是我们正在画的当前渲染目标**
 *   ⇒ 里面留着我们上一帧/上一次调用画的东西 ⇒ 反馈。这里用 `GetRenderTarget`(vtable +0x98, index 38)
 *   把"当前 RT"查出来；若它正好是我们要采的那张，就**自动换成另一张**（引擎本来就是双面交替）。
 */
static void *rb_pick_surface(void *dev, void **vt)
{
    void *surf = rb_rd(rb_base + (rb_surf ? RVA_SURF_A : RVA_SURF_B), 4)
               ? *(void **)(rb_base + (rb_surf ? RVA_SURF_A : RVA_SURF_B)) : NULL;
    void *rt = NULL;
    if (vt && rb_rd(vt + 0x27, 4) && rb_code((const void *)vt[0x26])) {
        /* ★ IDirect3DDevice9::GetRenderTarget(RenderTargetIndex, ppRenderTarget) —— **两个参数**。
         *   这里曾经只传了 (dev, &rt) ⇒ 少一个 ⇒ d3d9 把栈上的垃圾当 ppSurface 写 ⇒ 崩
         *   （三次 dump 的 Eip 全在 d3d9+0x11c00c，就是这么来的）。 */
        ((long (__stdcall *)(void *, unsigned, void **))vt[0x26])(dev, 0u, &rt);
    }
    if (rb_avoid_rt && rt && surf && rt == surf) {
        void *other = rb_rd(rb_base + (rb_surf ? RVA_SURF_B : RVA_SURF_A), 4)
                    ? *(void **)(rb_base + (rb_surf ? RVA_SURF_B : RVA_SURF_A)) : NULL;
        if (other && other != rt) {
            if (!rb_logged_rt) {
                rb_logged_rt = 1;
                rb_logf("[递归] 采的那张(%p)就是当前 RT ⇒ 自动改用另一张 %p（否则会采到自己画的）",
                        surf, other);
            }
            return other;
        }
        if (!rb_logged_rt) {
            rb_logged_rt = 1;
            rb_logf("[递归] ★ 采的那张就是当前 RT，且另一张不可用（%p）⇒ 递归可能仍在（把 ini surf 换一下试）", other);
        }
    }
    return surf;
}

static void rb_draw(void)
{
    void *dev, *surf, *tex;
    void **vt;
    unsigned zold = 1, zwold = 1;
    unsigned beold = 0, sbold = 0, dbold = 0;
    unsigned co_old = 0, ca1_old = 0, ao_old = 0, aa1_old = 0, aa2_old = 0;

    /*
     * ★★ 闲置路径必须**绝对惰性**：mode=0 时只读一个全局量就返回。
     *   为什么不是小事：崩溃 dump（th18.exe.84012.dmp / .105936.dmp）都是
     *   `0xc0000005 执行 0x1480968b`（堆地址）+ 栈上全是 **nvspcap.dll**（NVIDIA ShadowPlay 覆盖层）
     *   + 没有任何 th18.exe / 本 DLL 的帧 ⇒ 是**覆盖层/驱动**那一层的崩溃签名，且**两次 Eip 完全相同**。
     *   我们的桩每帧都在渲染链里跑，所以这里把"会从渲染链里调 user32（GetAsyncKeyState）"这件事
     *   挪到**确认在舞台里且模式非 0**之后 ⇒ 标题/加载/菜单阶段一个系统调用都不发。
     */
    /* ★★ 顺序不能反：**先判舞台 → 再轮询按键 → 最后按 mode 决定画不画**。
     *   旧顺序把 `rb_keys()` 放在 `mode==0` 的提前返回**之后** ⇒ mode=0 时永远收不到按键
     *   ⇒ ini 默认 0 时整个 mod「完全没有任何效果」（2026-10-21 实跑即此现象）。
     *   不在舞台（加载/标题/菜单）时仍然只做两次内存读，一个调用都不发 ✓ */
    {   float px, py;
        if (!rb_player(&px, &py)) return; }         /* 不在舞台：什么都不做（含按键） */
    if (bw_tick) bw_tick();                          /* 宿主的每帧回调（热键/外部联动） */
    if (rb_mode == 0) return;                       /* 关着：不碰 D3D */
    dev = rb_rd(rb_base + RVA_DEVICE, 4) ? *(void **)(rb_base + RVA_DEVICE) : NULL;
    if (!dev || !rb_rd(dev, 0x40)) return;
    vt = *(void ***)dev;
    if (!vt || !rb_rd(vt + 0x60, 4)) return;
    /* ★ 我们要用的每一个槽，都必须是"模块里的可执行代码"（否则设备已失效 ⇒ 宁可不画） */
    if (!rb_code((const void *)vt[0x3a]) || !rb_code((const void *)vt[0x39]) ||   /* Get/SetRenderState */
        !rb_code((const void *)vt[0x41]) || !rb_code((const void *)vt[0x42]) ||   /* Set/GetTextureStageState */
        !rb_code((const void *)vt[0x43]) || !rb_code((const void *)vt[0x53]) ||   /* SetTextureStageState / DrawPrimitiveUP */
        !rb_code((const void *)vt[0x59]) || !rb_code((const void *)vt[0x26]) ||   /* SetFVF / GetRenderTarget */
        !rb_code((const void *)vt[0x30])) {                                       /* GetViewport */
        if (!rb_logged_baddev) {
            rb_logged_baddev = 1;
            rb_logf("[设备] ★ 设备 %p 的 vtable 里有非代码项（多半是加载/标题阶段的过期指针）"
                    "⇒ 本次及后续不画，等真正进舞台再说", dev);
        }
        return;
    }
    /*
     * ★★ 设备指针一变（重建/Reset）⇒ 旧纹理与旧"面"都可能已被释放 ⇒ 必须清缓存。
     *   不清的后果：拿旧 texture 去 SetTexture（D3D 会拒绝）或拿旧面去 GetContainer（可能直接崩）。
     */
    if (dev != rb_last_dev) {
        rb_last_dev = dev;
        rb_dev_settle = 0;
        rb_scene_tex = NULL;
        rb_halo_tex  = NULL;
        rb_last_good_surf = NULL;
        rb_logged_desc = 0;
        rb_logged_rt = 0;
        rb_logf("[设备] 设备指针变化 → %p ⇒ 清空缓存并进入冷却期", dev);
        return;                                  /* ★ 本帧直接退出：别在新设备刚建好时碰旧资源 */
    }
    if (rb_dev_settle < 30) { rb_dev_settle++; return; }   /* ★ 复位后的冷却期：30 帧内不碰任何 D3D 资源 */
    /*
     * ★★ Reset 期间**设备指针不变**（游戏对同一个 device 调 Reset）⇒ 上面"指针变化"的检查抓不到 ✗
     *   必须问设备自己：`IDirect3DDevice9::TestCooperativeLevel()`（vtable index 3）——
     *   非 S_OK（D3DERR_DEVICELOST / DEVICENOTRESET）就说明正在丢/待重置 ⇒ 立刻停、清缓存、进冷却。
     *   这一条是 2026-10-21 两次 d3d9 内部崩溃（Eip=d3d9+0x11c00c）换来的。
     */
    if (rb_code((const void *)vt[0x03])) {
        long hr = ((long (__stdcall *)(void *))vt[0x03])(dev);
        if (hr != 0) {
            rb_dev_settle = 0;
            rb_scene_tex = NULL; rb_halo_tex = NULL; rb_last_good_surf = NULL;
            if (!rb_logged_lost) {
                rb_logged_lost = 1;
                rb_logf("[设备] TestCooperativeLevel=0x%08x ⇒ 设备丢失/待重置，停止绘制并清缓存", (unsigned)hr);
            }
            return;
        }
    }
    rb_get_vp(dev, vt);                             /* ★ 必须放在所有门闸之后：冷却期里绝不调设备 */

    if (!rb_logged_desc) {
        rb_logged_desc = 1;
        surf = rb_pick_surface(dev, vt);
        if (surf && rb_rd(surf, 0x40)) {
            struct { unsigned fmt, type, usage, pool, multi, msq, w, h; } d;
            memset(&d, 0, sizeof d);
            if (rb_surf_ready(surf))
                ((long (__stdcall *)(void *, void *))(*(void ***)surf)[12])(surf, &d);
            rb_logf("[面] 源面 %s = %p → %ux%u format=%u", rb_surf ? "A[0x4ccf9c]" : "B[0x4ccfa0]",
                    surf, d.w, d.h, d.fmt);
        }
        rb_logf("[视口]%s x=%.0f y=%.0f w=%.0f h=%.0f（世界 x∈[-192,192] y∈[0,448]）",
                rb_vp_ok ? "" : "（读失败，用默认）",
                (double)rb_vp_x, (double)rb_vp_y, (double)rb_vp_w, (double)rb_vp_h);
        {   int cols = 0;
            BYTE *mvm = rb_find_mesh_vm(&cols);
            if (!mvm) rb_dump_vms("首帧扫描失败");
            if (mvm) {
                void *ht = rb_tex_of_vm(mvm, "网格段VM");
                if (ht) rb_halo_tex = ht;
                if (!rb_logged_halo) {
                    rb_logged_halo = 1;
                    rb_logf("[光晕] 网格段 VM=%p（cols=%d）→ 光晕纹理 %p（找到就用它画「光晕测试」）",
                            mvm, cols, rb_halo_tex);
                }
            } else {
                rb_logf("[光晕] 两条 VM 链上没找到「网格段 VM」（可能这局没有那种网格）");
            }
        }
    }

    /* ★ 光晕/敌方盘"随时可能出现"（boss 出场、符卡开始…）⇒ 未解到时定期重扫（不是只扫第一帧） */
    if ((!rb_halo_tex || !rb_ov_ok) && (rb_frames % 60) == 0) {
        int cols = 0;
        BYTE *mvm = rb_find_mesh_vm(&cols);
        if (mvm) {
            void *ht = rb_tex_of_vm(mvm, "网格段VM(重扫)");
            if (ht) {
                if (!rb_halo_tex) { rb_halo_tex = ht; rb_logf("[光晕] 重扫拿到光晕纹理 %p（第 %u 帧）", ht, rb_frames); }
                if (!rb_dumped_all) { rb_dumped_all = 1; rb_dump_all_candidates(); }
            }
        } else {
            rb_rescan_fail++;
            if (rb_rescan_fail == 1 || (rb_rescan_fail % 5) == 0) rb_dump_vms("重扫仍失败");
        }
    }

    surf = rb_pick_surface(dev, vt);
    if (!surf || !rb_rd(surf, 0x40)) return;
    {   void **svt = *(void ***)surf;
        if (!svt || !rb_rd(svt + 12, 4)) return;
        if (!rb_code((const void *)svt[11]) || !rb_code((const void *)svt[12])) return; }
    if (!rb_scene_tex || (surf != rb_last_good_surf)) {
        void *t2 = rb_screen_tex_from_vm();            /* ★ 优先：游戏自己维护的屏幕纹理 */
        if (!rb_logged_src) {
            rb_logged_src = 1;
            rb_logf("[纹理] 屏幕纹理来源 = %s", t2 ? "屏幕特供VM（游戏维护，最稳）" : "面容器 GetContainer（兜底）");
        }
        if (!t2) {
            if (rb_last_good_surf && surf != rb_last_good_surf) return;  /* 面变了但没新纹理 ⇒ 本帧先不画 */
            t2 = rb_surface_tex(surf);                 /* 兜底：GetContainer（此时设备已稳定 30 帧） */
            if (!t2) return;
        }
        rb_scene_tex = t2;
        rb_last_good_surf = surf;
    }
    tex = rb_scene_tex;
    if (!tex) return;

    /* 状态：关深度（存/恢复）+ 引擎同款 + 贴图/FVF */
    ((void (__stdcall *)(void *, unsigned, unsigned *))vt[0x3a])(dev, 7u, &zold);
    ((void (__stdcall *)(void *, unsigned, unsigned *))vt[0x3a])(dev, 14u, &zwold);
    ((void (__stdcall *)(void *, unsigned, unsigned))vt[0x39])(dev, 7u, 0u);
    ((void (__stdcall *)(void *, unsigned, unsigned))vt[0x39])(dev, 14u, 0u);
    /*
     * ★★ "画出来是正方形"的根因：**混合没开** ⇒ 我们给圆外顶点的 alpha=0 被忽略，它们照样不透明地画出来
     *    ⇒ 显示成圆的外接正方形。所以这里显式打开 alpha 混合，并把 alpha 来源钉在**顶点漫反射**上：
     *      COLOROP = SELECTARG1(COLORARG1=TEXTURE)     颜色 = 纹理
     *      ALPHAOP = SELECTARG1(ALPHAARG1=DIFFUSE)     alpha = 我们的顶点 alpha
     *    全部**先存后设、画完恢复**（不与引擎抢状态）。
     */
    ((void (__stdcall *)(void *, unsigned, unsigned *))vt[0x3a])(dev, RB_RS_ALPHABLENDENABLE, &beold);
    ((void (__stdcall *)(void *, unsigned, unsigned *))vt[0x3a])(dev, RB_RS_SRCBLEND, &sbold);
    ((void (__stdcall *)(void *, unsigned, unsigned *))vt[0x3a])(dev, RB_RS_DESTBLEND, &dbold);
    ((void (__stdcall *)(void *, unsigned, unsigned, unsigned *))vt[0x42])(dev, 0u, RB_TSS_COLOROP, &co_old);
    ((void (__stdcall *)(void *, unsigned, unsigned, unsigned *))vt[0x42])(dev, 0u, RB_TSS_COLORARG1, &ca1_old);
    ((void (__stdcall *)(void *, unsigned, unsigned, unsigned *))vt[0x42])(dev, 0u, RB_TSS_ALPHAOP, &ao_old);
    ((void (__stdcall *)(void *, unsigned, unsigned, unsigned *))vt[0x42])(dev, 0u, RB_TSS_ALPHAARG1, &aa1_old);
    ((void (__stdcall *)(void *, unsigned, unsigned, unsigned *))vt[0x42])(dev, 0u, RB_TSS_ALPHAARG2, &aa2_old);
    ((void (__stdcall *)(void *, unsigned, unsigned))vt[0x39])(dev, RB_RS_ALPHABLENDENABLE, 1u);
    ((void (__stdcall *)(void *, unsigned, unsigned))vt[0x39])(dev, RB_RS_SRCBLEND, RB_BLEND_SRCALPHA);
    ((void (__stdcall *)(void *, unsigned, unsigned))vt[0x39])(dev, RB_RS_DESTBLEND, RB_BLEND_INVSRCALPHA);
    ((void (__stdcall *)(void *, unsigned, unsigned, unsigned))vt[0x43])(dev, 0u, RB_TSS_COLOROP, RB_TOP_SELECTARG1);
    ((void (__stdcall *)(void *, unsigned, unsigned, unsigned))vt[0x43])(dev, 0u, RB_TSS_COLORARG1, RB_TA_TEXTURE);
    ((void (__stdcall *)(void *, unsigned, unsigned, unsigned))vt[0x43])(dev, 0u, RB_TSS_ALPHAOP, RB_TOP_SELECTARG1);
    ((void (__stdcall *)(void *, unsigned, unsigned, unsigned))vt[0x43])(dev, 0u, RB_TSS_ALPHAARG1, RB_TA_DIFFUSE);
    rb_phx += RB_PH_FX;
    rb_phy += RB_PH_FY;
    while (rb_phx >  RB_PI) rb_phx -= RB_2PI;
    while (rb_phx < -RB_PI) rb_phx += RB_2PI;
    while (rb_phy >  RB_PI) rb_phy -= RB_2PI;
    while (rb_phy < -RB_PI) rb_phy += RB_2PI;

    {   float px, py, pxs, pys;
        void *use_tex = tex;
        if (rb_mode == 3 || rb_mode == 5) {
            if (!rb_halo_tex) {
                if (rb_logged_nohalo != rb_mode) {
                    rb_logged_nohalo = rb_mode;
                    rb_logf("[光晕] ★ 模式 %d 需要光晕纹理，但这局没解到（多半没在 boss 战里/那局没有那种网格）"
                            "⇒ 这一档不画（以免用场景纹理伪装成光晕）", rb_mode);
                }
                goto done_states_early;
            }
            use_tex = rb_halo_tex;
        }
        ((void (__stdcall *)(void *, unsigned, void *))vt[0x41])(dev, 0, use_tex);   /* SetTexture */
        ((void (__stdcall *)(void *, unsigned))vt[0x59])(dev, 0x144u);               /* SetFVF(0x144) */
        ((void (__stdcall *)(void *, unsigned, unsigned, unsigned))vt[0x43])(dev, 0, 6, 0);
        if (rb_mode == 1) {
            rb_draw_quad(dev, vt, use_tex, 0.0f, 0.0f);
            if (rb_two) rb_draw_quad(dev, vt, use_tex, 0.0f, 0.0f);
        } else if (rb_player(&px, &py)) {
            /* 重叠分域要用"敌方那个盘"的位置/半径：每 30 帧刷一次（不每帧走 VM 链） */
            if (rb_overlap && (rb_frames % 30) == 0) {
                int nv = 0;
                float ecx, ecy, eR;
                rb_ov_ok = rb_enemy_disc(&ecx, &ecy, &eR, &nv) && eR > 8.0f;
                if (rb_ov_ok) { rb_ov_cx = ecx; rb_ov_cy = ecy; rb_ov_r = eR; }
                if (!rb_logged_ov) {
                    rb_logged_ov = 1;
                    rb_logf("[重叠] 软性分域：%s（敌方盘 %s 圆心=(%.1f,%.1f) R=%.1f 过渡带=%.0f）",
                            rb_overlap ? "开" : "关", rb_ov_ok ? "有" : "无",
                            (double)rb_ov_cx, (double)rb_ov_cy, (double)rb_ov_r, (double)rb_blend);
                }
            }
            rb_world_to_screen(px, py, &pxs, &pys);
            {   /* ★ radius_auto：把"敌方那个盘的半径"当作我方的半径 ⇒ 放大倍率与敌方一致 */
                float eR2;
                if (rb_radius_auto) {
                    float ex2, ey2;
                    int nv2 = 0;
                    if (rb_enemy_disc(&ex2, &ey2, &eR2, &nv2) && eR2 > 8.0f) {
                        if (!rb_logged_enemy) {
                            rb_logged_enemy = 1;
                            rb_logf("[敌方] 半径跟敌方：R=%.1f（由 %d 台 VM 反推）", (double)eR2, nv2);
                        }
                        rb_radius = eR2;
                    }
                }
            }
            if (rb_mode == 2) {
                rb_draw_grid(dev, vt, use_tex, 0, px, py, rb_radius, 0.0f, 0.0f);
            } else if (rb_mode == 4) {
                /* ★ 4 = 平面网格（位移临时归零）：若这块是"干净的背景方块"，则几何/贴图/状态都没问题，
                 *   问题只在位移场；若它仍然破碎 ⇒ 几何或状态有问题。 */
                float pwsave = rb_power;
                rb_power = 0.0f;
                rb_draw_grid(dev, vt, use_tex, 0, px, py, rb_radius, 0.0f, 0.0f);
                rb_power = pwsave;
            } else if (rb_mode == 5) {   /* mode 5：光晕纹理 + 整图 UV（"这张纹理是什么"） */
                rb_draw_grid(dev, vt, use_tex, 2, px, py, rb_radius, 0.0f, 0.0f);
            } else {   /* mode 3：光晕纹理 + sprite 矩形 UV（看清这张贴图） */
                rb_draw_grid(dev, vt, use_tex, 3, px, py, rb_radius, 0.0f, 0.0f);
            }
            if (rb_two) {                     /* ★ 第二份：优先画在**敌方那个圆盘**上 */
                int uv = (rb_mode == 5) ? 2 : (rb_mode == 3);
                float ex, ey, eR;
                int nvm = 0;
                if (rb_enemy_disc(&ex, &ey, &eR, &nvm) && eR > 8.0f) {
                    if (!rb_logged_enemy) {
                        rb_logged_enemy = 1;
                        rb_logf("[敌方] 由 %d 台网格段 VM 的缓冲反推：世界圆心=(%.1f,%.1f) R=%.1f ⇒ 第二份画在那儿",
                                nvm, (double)ex, (double)ey, (double)eR);
                    }
                    rb_draw_grid(dev, vt, use_tex, uv, ex, ey, eR, 0.0f, 0.0f);
                } else {
                    rb_draw_grid(dev, vt, use_tex, uv, px, py, rb_radius, rb_two_off, 0.0f);
                }
            }
        }
    }
done_states_early:
    ((void (__stdcall *)(void *, unsigned, unsigned))vt[0x39])(dev, RB_RS_ALPHABLENDENABLE, beold);
    ((void (__stdcall *)(void *, unsigned, unsigned))vt[0x39])(dev, RB_RS_SRCBLEND, sbold);
    ((void (__stdcall *)(void *, unsigned, unsigned))vt[0x39])(dev, RB_RS_DESTBLEND, dbold);
    ((void (__stdcall *)(void *, unsigned, unsigned, unsigned))vt[0x43])(dev, 0u, RB_TSS_COLOROP, co_old);
    ((void (__stdcall *)(void *, unsigned, unsigned, unsigned))vt[0x43])(dev, 0u, RB_TSS_COLORARG1, ca1_old);
    ((void (__stdcall *)(void *, unsigned, unsigned, unsigned))vt[0x43])(dev, 0u, RB_TSS_ALPHAOP, ao_old);
    ((void (__stdcall *)(void *, unsigned, unsigned, unsigned))vt[0x43])(dev, 0u, RB_TSS_ALPHAARG1, aa1_old);
    ((void (__stdcall *)(void *, unsigned, unsigned, unsigned))vt[0x43])(dev, 0u, RB_TSS_ALPHAARG2, aa2_old);
    ((void (__stdcall *)(void *, unsigned, unsigned))vt[0x39])(dev, 7u, zold);
    ((void (__stdcall *)(void *, unsigned, unsigned))vt[0x39])(dev, 14u, zwold);
    {   /* 反缓存：引擎缓存了"当前贴图/FVF"，我们改过真状态 ⇒ 置成不可能值逼它重设 */
        BYTE *mgr = rb_rd(rb_base + RVA_ANM_MGR, 4) ? *(BYTE **)(rb_base + RVA_ANM_MGR) : NULL;
        if (mgr) {
            if (rb_wr(mgr + 0x3120e04, 4)) *(unsigned *)(mgr + 0x3120e04) = 0xFFFFFFFFu;
            if (rb_wr(mgr + 0x3120e0a, 1)) *(BYTE *)(mgr + 0x3120e0a) = 0xFFu;
        }
    }
    rb_frames++;
    rb_drawn++;
    if (rb_log_every && (rb_drawn % rb_log_every) == 0)
        rb_logf("[画] 第 %u 次（模式 %d，源纹理 %p，光晕纹理 %p，两份=%d，N=%d，R=%.0f 强度=%.2f）",
                rb_drawn, rb_mode, tex, rb_halo_tex, rb_two, rb_n, (double)rb_radius, (double)rb_power);
    if (!rb_logged_grid) {
        rb_logged_grid = 1;
        rb_logf("[网格] N=%d R=%.0f fade=%.2f 视口=%.0f,%.0f %.0fx%.0f", rb_n, (double)rb_radius,
                (double)rb_fade, (double)rb_vp_x, (double)rb_vp_y, (double)rb_vp_w, (double)rb_vp_h);
    }
}

/* ================= 热键（挂在桩里，每帧轮询） ================= */




/* 每帧热键轮询：桩里也会调（桩装上了才有），另外给主文件一个入口备用 */


/* ================= 公开接口（见 bgwarp_core.h） ================= */

int bgwarp_install(bgwarp_log_fn logf)
{
    const BYTE *p;
    bw_logcb = logf;
    rb_base = (BYTE *)GetModuleHandleW(NULL);
    if (!rb_base) { rb_logf("[core] 拿不到 exe 基址"); return 0; }
    p = rb_base + RVA_LAYER6_STUB;
    if (!rb_rd(p, 8) || !(p[0] == 0x6a && p[1] == 0x06 && p[2] == 0xe8)) {
        rb_logf("[core] ★ 层 6 桩签名不符（%02x %02x %02x）⇒ 不装桩，功能保持关闭", p[0], p[1], p[2]);
        return 0;
    }
    if (!rb_make_replace_stub(rb_base + RVA_LAYER6_STUB, 8, 6,
                              (void *)(rb_base + RVA_RENDER_LAYER), (void *)rb_draw)) {
        rb_logf("[core] ★ 替换层 6 桩失败 ⇒ 功能保持关闭");
        return 0;
    }
    rb_stub_ok = 1;
    rb_logf("[core] 层 6 桩已替换，功能就绪（bgwarp_set 开/关）");
    return 1;
}

void bgwarp_set(int on, float radius, float power, float alpha)
{
    if (!on) {
        rb_mode = 0;
        rb_logf("[core] 关闭");
        return;
    }
    if (radius > 1.0f) rb_radius = radius;
    if (power > 0.0f) rb_power = power;
    if (alpha > 0.0f) rb_alpha = (alpha > 255.0f) ? 255u : (unsigned)alpha;
    rb_mode = 2;                                  /* 生产观感 = 扭曲网格 */
    rb_logf("[core] 开启：R=%.1f 强度=%.2f alpha=%u", (double)rb_radius, (double)rb_power, rb_alpha);
}

void bgwarp_tune(int grid_n, float fade, int overlap, float blend)
{
    if (grid_n >= 3) {
        if (grid_n > 65) grid_n = 65;
        if ((grid_n & 1) == 0) grid_n++;
        rb_n = grid_n;
    }
    if (fade >= 0.0f) rb_fade = (fade > 0.9f) ? 0.9f : fade;
    rb_overlap = overlap ? 1 : 0;
    if (blend >= 1.0f) rb_blend = blend;
}

void bgwarp_set_test(int surf, int two, float shift, float two_off, int radius_auto)
{
    rb_surf = surf ? 1 : 0;
    rb_two = two ? 1 : 0;
    if (shift > 0.0f) rb_shift = shift;
    if (two_off > 0.0f) rb_two_off = two_off;
    rb_radius_auto = radius_auto ? 1 : 0;
}

void bgwarp_set_mode(int mode) { rb_mode = mode; }
void bgwarp_set_tick(void (*fn)(void)) { bw_tick = fn; }
int  bgwarp_mode(void) { return rb_mode; }

void bgwarp_restore(void)
{
    if (!bw_target || bw_patch_len <= 0) return;
    {   DWORD old0;
        if (VirtualProtect(bw_target, bw_patch_len, PAGE_EXECUTE_READWRITE, &old0)) {
            memcpy(bw_target, bw_orig, bw_patch_len);
            VirtualProtect(bw_target, bw_patch_len, old0, &old0);
            FlushInstructionCache(GetCurrentProcess(), bw_target, bw_patch_len);
            rb_logf("[core] 已还原层 6 桩（stub_ok=%d）", rb_stub_ok);
        }
    }
    bw_target = NULL; bw_patch_len = 0;
    rb_stub_ok = 0; rb_mode = 0;
}
