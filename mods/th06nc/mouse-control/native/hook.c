/*
 * hook.c —— 单文件 x64 内联 hook + 鼠标控制。
 *
 * ★ 设计要点（照抄 mods/th18.v1.00a/mouse-control 那条已验证的思路）：
 *   **只写 INPUT_HELD 一个 dword，玩家对象零写入。**
 *   钳位、低速倍率、倾斜动画、判定全部由游戏自己的代码完成 ——
 *   我们只是替它决定「这一帧按了哪个方向」。
 *
 * 所以本模块不调用任何引擎函数、不碰引擎的栈、不碰 FPU/SSE 状态：
 *   AUDIT-checklist 的 A 节（ABI/栈平衡）整节不适用，
 *   C 节只剩「偏移是否有一手出处」这一条。
 *
 * 唯一的手写机器码是那段跳板，逐字节说明见 build_trampoline()。
 */
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "th06nc.h"

static uint8_t *g_base;
static uint8_t *g_stub;
static int      g_on = 1;      /* 默认开启（用户要求）；F9 仍可随时关掉 */
static int      g_cursor_hidden, g_disabled;
static float    g_origin_x = DEFAULT_ORIGIN_X, g_origin_y = DEFAULT_ORIGIN_Y;
                                /* origin_x < 0 = 水平居中（NC 的实际布局） */
static int      g_toggle_vk = VK_F9;
static int      g_calib_vk = VK_F10;      /* 瞄自机法：两次 */
static int      g_corner_vk = VK_F11;     /* 点关卡区角法：两次（更好按） */
static char     g_ini[MAX_PATH];

/* 标定出来的仿射映射：客户区像素 = off + scale * positionCenter。
 * g_cal_w/h 是标定时的客户区尺寸 —— 窗口改了尺寸就按比例缩放，不用重标。
 * g_cal == 0 表示没标定，退回 640x480 等比信箱的初始猜测。 */
static int      g_cal;
static float    g_cal_sx, g_cal_ox, g_cal_sy, g_cal_oy;
static int      g_cal_w, g_cal_h;
static int      g_cal_pending;                       /* 已记下第一个点 */
static float    g_cal_c1x, g_cal_c1y, g_cal_p1x, g_cal_p1y;
static int      g_corner_pending;
static float    g_corner_c1x, g_corner_c1y;
static FILE    *g_log;
static const char *g_tag;      /* 哪个代理把我们带进来的 */
static void       *g_self;     /* 本 DLL 的模块句柄 */

void mouse_set_identity(void *hmodule, const char *tag)
{
    g_self = hmodule;
    g_tag  = tag;
}

/* 日志必须在「文件写不出去」时仍然可见 —— 这一轮就是栽在「什么都没有」上：
 * 分不清是 DLL 没被加载，还是加载了但日志写不出来。所以每行同时走
 * OutputDebugStringA，用 DebugView（管理员运行）能直接看到，不依赖任何文件权限。 */
static void rk_log(const char *fmt, ...)
{
    char buf[1024];
    int  n;
    va_list ap;
    va_start(ap, fmt);
    n = vsnprintf(buf, sizeof buf - 2, fmt, ap);
    va_end(ap);
    if (n < 0 || n > (int)sizeof buf - 2) n = (int)sizeof buf - 2;
    buf[n] = 0;
    OutputDebugStringA("[th06nc-mouse] ");
    OutputDebugStringA(buf);
    OutputDebugStringA("\n");
    if (!g_log) return;
    fputs(buf, g_log);
    fputc('\n', g_log);
    fflush(g_log);
}

/* 每条路径只记一次，否则每帧刷屏 */
static int d_entry, d_badstate, d_cursor, d_ok, d_btn;
#define ONCE(flag, ...) do { if (!(flag)) { (flag) = 1; rk_log(__VA_ARGS__); } } while (0)

static void calib_point(float cx, float cy, float px, float py, int cw, int ch);
static void calib_solve(float c1x, float c1y, float p1x, float p1y,
                        float c2x, float c2y, float p2x, float p2y,
                        int cw, int ch, const char *how);

static void set_cursor_hidden(int hide)
{
    if (hide == g_cursor_hidden) return;
    ShowCursor(hide ? FALSE : TRUE);   /* 计数式 API，保证成对 */
    g_cursor_hidden = hide;
}

/* ------------------------------------------------------------------ *
 * 每帧回调：在 Player__OnUpdate 真正开始之前跑
 * ------------------------------------------------------------------ */
void mouse_tick(void)
{
    uint32_t *held;
    uint8_t  *player;
    HWND      hwnd;
    POINT     pt;
    RECT      rc;
    float     tx, ty, px, py, dx, dy, ax, ay, dead;
    uint32_t  bits = 0, add = 0;
    int       cw, ch;

    ONCE(d_entry, "[diag] mouse_tick 首次触发");
    if (g_disabled) return;

    hwnd = GetForegroundWindow();
    if (!hwnd || hwnd != GetActiveWindow()) {
        /* 不是前台就彻底不插手，alt-tab 出去点别的窗口不会走火 */
        if (g_cursor_hidden) set_cursor_hidden(0);
        return;
    }

    if (GetAsyncKeyState(g_toggle_vk) & 1) {
        g_on = !g_on;
        rk_log("[mouse] %s", g_on ? "开" : "关");
        if (!g_on) set_cursor_hidden(0);
    }

    held   = (uint32_t *)(g_base + RVA_INPUT_HELD);
    player = g_base + RVA_G_PLAYER;

    /* 关卡区角标定：光标点在关卡区**可见边框**的左上角 / 右下角各按一次。
     * 这两个点的游戏坐标是已知的：positionCenter 的原点就是关卡区左上角，
     * 右下角即 arcadeRegionSize =（384,448）—— 见 engine/player/th06nc/01 §2.11。
     * 比瞄自机好按得多，也不受自机贴图中心偏移影响。 */
    if (GetAsyncKeyState(g_corner_vk) & 1) {
        POINT cp;
        RECT  cr;
        if (GetCursorPos(&cp) && ScreenToClient(hwnd, &cp) && GetClientRect(hwnd, &cr)
            && cr.right > 0 && cr.bottom > 0) {
            if (g_on) {
                /* 开着鼠标控制时光标是隐藏的，看不见自然也点不准。
                 * 两种标定统一要求先关掉——关掉光标就回来了。 */
                rk_log("[calib] 请先按 %s 关掉鼠标控制 —— 开着时光标是隐藏的，点不准角。",
                       g_toggle_vk == VK_F9 ? "F9" : "开关键");
            } else if (!g_corner_pending) {
                g_corner_c1x = (float)cp.x; g_corner_c1y = (float)cp.y;
                g_corner_pending = 1;
                rk_log("[calib] 角标定第 1 点（关卡区左上角）：光标=(%ld,%ld)。"
                       "再把光标点到关卡区右下角按一次。", cp.x, cp.y);
            } else {
                g_corner_pending = 0;
                /* 左上角 = 游戏坐标 (0,0)，右下角 = (PLAYFIELD_W, PLAYFIELD_H) */
                calib_solve(g_corner_c1x, g_corner_c1y, 0.0f, 0.0f,
                            (float)cp.x, (float)cp.y, PLAYFIELD_W, PLAYFIELD_H,
                            cr.right, cr.bottom, "角标定");
            }
        }
    }

    /* 标定键要在 !g_on 之前处理：标定的做法是「关掉鼠标控制，用键盘把自机
     * 摆到关卡区一角，再把光标压到自机上按一下」。开着鼠标控制时自机跟着光标跑，
     * 两者绑死，根本取不到两个独立的点。 */
    if (GetAsyncKeyState(g_calib_vk) & 1) {
        POINT cp;
        RECT  cr;
        if (GetCursorPos(&cp) && ScreenToClient(hwnd, &cp) && GetClientRect(hwnd, &cr)
            && cr.right > 0 && cr.bottom > 0) {
            if (g_on)
                rk_log("[calib] 请先按 %s 关掉鼠标控制 —— 开着时自机跟着光标跑，取不到独立的点。",
                       g_toggle_vk == VK_F9 ? "F9" : "开关键");
            else
                calib_point((float)cp.x, (float)cp.y,
                            *(float *)(player + OFF_POSITION_X),
                            *(float *)(player + OFF_POSITION_Y),
                            cr.right, cr.bottom);
        }
    }

    if (!g_on) return;

    /* 出生/死亡动画期间不插手：那时游戏不读方向位，插手只会让状态难以推理。
     * 1 = SPAWNING, 2 = DEAD（engine/player/th06nc/01 §2.9，枚举值与 TH06 相同）。 */
    if (player[OFF_PLAYER_STATE] == 1 || player[OFF_PLAYER_STATE] == 2) {
        ONCE(d_badstate, "[diag] playerState=%u —— 本帧不接管", player[OFF_PLAYER_STATE]);
        goto buttons;
    }

    if (!GetCursorPos(&pt) || !ScreenToClient(hwnd, &pt) || !GetClientRect(hwnd, &rc)) {
        ONCE(d_cursor, "[diag] 取光标/客户区失败 err=%lu", GetLastError());
        return;
    }
    cw = rc.right; ch = rc.bottom;
    if (cw <= 0 || ch <= 0) return;

    px = *(float *)(player + OFF_POSITION_X);
    py = *(float *)(player + OFF_POSITION_Y);

    if (g_cal) {
        /* 实测标定：客户区像素 = off + scale * pos，取逆。
         * 窗口尺寸变了就整体按比例缩放，不必重标。 */
        float k = (g_cal_w > 0) ? (float)cw / (float)g_cal_w : 1.0f;
        float ky = (g_cal_h > 0) ? (float)ch / (float)g_cal_h : 1.0f;
        tx = ((float)pt.x - g_cal_ox * k)  / (g_cal_sx * k);
        ty = ((float)pt.y - g_cal_oy * ky) / (g_cal_sy * ky);
    } else {
        /* 默认布局 —— **由两次实测标定确认**（1280x720 + 1920x1080），见 TARGET.md §6：
         *   虚拟屏高度固定 480，宽度随窗口宽高比扩展（不做 4:3 信箱）；
         *   关卡区 384x448 **水平居中**，顶边在虚拟 y=16（与 TH06 相同）。
         * 注意这和 TH06 不一样：TH06 关卡区左边界固定在 x=32（右侧留 HUD），
         * NC 是居中的。照抄 TH06 会偏右约 200px —— 这就是那个 bug 的根因。 */
        float s = (float)ch / VIRT_H;
        float lx, ly;
        if (s <= 0.0f) return;
        lx = (g_origin_x >= 0.0f) ? g_origin_x * s
                                  : ((float)cw - PLAYFIELD_W * s) * 0.5f;  /* <0 = 居中 */
        ly = g_origin_y * s;
        tx = ((float)pt.x - lx) / s;
        ty = ((float)pt.y - ly) / s;
    }

    /* 钳到自机可动区：**用游戏自己的边界**，不再硬编码 384x448。
     * 这四个 f32 就是 Player__OnUpdate 钳位时读的那四个
     * （engine/player/th06nc/01 §2.11）。运行时才填，读到 0 就退回常量。 */
    {
        const float *area = (const float *)(g_base + RVA_MOVEMENT_AREA);
        float lx = area[0], ly = area[1], sx = area[2], sy = area[3];
        if (sx <= 0.0f || sy <= 0.0f) { lx = 0.0f; ly = 0.0f; sx = PLAYFIELD_W; sy = PLAYFIELD_H; }
        if (tx < lx) tx = lx;  else if (tx > lx + sx) tx = lx + sx;
        if (ty < ly) ty = ly;  else if (ty > ly + sy) ty = ly + sy;
    }

    dx = tx - px;
    dy = ty - py;

    /* 死区 = 本帧的合法步长，够不上一步就不动，否则会在光标附近来回震荡。
     * 速度从 characterData 读：+0 正交、+4 低速（engine/player/th06nc/01 §2.10）。 */
    dead = *(float *)(player + OFF_CHAR_DATA + (player[OFF_IS_FOCUSED] ? 4 : 0));
    if (dead <= 0.0f) dead = 1.0f;

    if (dx * dx + dy * dy >= dead * dead) {
        /* 八向量化：|dy| < |dx|*tan22.5 -> 纯水平，反之纯垂直，否则斜向。
         * y 轴向下，dy > 0 = 目标在下方 = 按「下」。 */
        ax = dx < 0.0f ? -dx : dx;
        ay = dy < 0.0f ? -dy : dy;
        if (ay < ax * 0.414214f)       bits = (dx > 0.0f) ? IN_RIGHT : IN_LEFT;
        else if (ax < ay * 0.414214f)  bits = (dy > 0.0f) ? IN_DOWN : IN_UP;
        else bits = ((dx > 0.0f) ? IN_RIGHT : IN_LEFT) | ((dy > 0.0f) ? IN_DOWN : IN_UP);
    }

    ONCE(d_ok, "[diag] 首次生效 %s client=%dx%d cursor=(%ld,%ld) tgt=(%.1f,%.1f) pos=(%.1f,%.1f) dead=%.2f",
         g_cal ? "[已标定]" : "[未标定,用初始猜测——会偏,见 README 标定]",
         cw, ch, pt.x, pt.y, tx, ty, px, py, dead);

    /* 唯一的一次写：只动方向位，射击/炸弹/低速原样保留 */
    *held = (*held & ~(uint32_t)IN_DIR_MASK) | bits;
    set_cursor_hidden(1);

buttons:
    /* 按键只 OR、不清位，所以键盘 Z/X 与鼠标左右键同时有效 */
    if (GetAsyncKeyState(VK_LBUTTON) & 0x8000) add |= IN_SHOOT;
    if (GetAsyncKeyState(VK_RBUTTON) & 0x8000) add |= IN_FOCUS;   /* 右键 = 低速 */
    if (GetAsyncKeyState(VK_MBUTTON) & 0x8000) add |= IN_BOMB;    /* 中键 = bomb  */
    if (add) {
        *(uint32_t *)(g_base + RVA_INPUT_HELD) |= add;
        ONCE(d_btn, "[diag] 鼠标按键首次生效 add=0x%x", add);
    }
}

/* ------------------------------------------------------------------ *
 * 跳板 —— 本 mod 唯一的手写机器码
 *
 * 到达时栈状态 = Player__OnUpdate 入口：调用方的 call 刚压了返回地址，
 * 所以 rsp ≡ 8 (mod 16)。
 *
 *   off  字节                     指令                     rsp mod 16
 *   00   51                       push rcx
 *   01   52                       push rdx
 *   02   41 50                    push r8
 *   04   41 51                    push r9
 *   06   41 52                    push r10
 *   08   41 53                    push r11                 6*8=48≡0 → 仍 8
 *   0a   48 83 ec 28              sub  rsp,0x28            8-40 ≡ 0  ✅ 对齐
 *   0e   48 b8 <imm64>            mov  rax, mouse_tick
 *   18   ff d0                    call rax                 压 8 → 被调方 ≡8 ✅
 *   1a   48 83 c4 28              add  rsp,0x28
 *   1e   41 5b / 41 5a / 41 59 / 41 58 / 5a / 59   逆序 pop
 *   28   <13 字节原序言>          MOV RAX,RSP / PUSH RDI / PUSH R12 / SUB RSP,0xd8
 *   35   ff 25 00 00 00 00        jmp qword ptr [rip+0]    不碰任何寄存器
 *   3b   <8 字节绝对地址 = OnUpdate+13>
 *
 * 三条关键性质：
 *  1. **0x28 = 40 ≥ 32 字节 shadow space**，且让 rsp 在 call 前 16 字节对齐。
 *  2. **RAX 可以随便用**：入口处 RAX 是 scratch，且原序言第一条就是 MOV RAX,RSP，
 *     会把它重新设成正确值。返回跳转用 `jmp [rip+0]` 而不是 `mov rax;jmp rax`，
 *     正是为了不破坏刚刚建立的 RAX。
 *  3. 非易失寄存器（RBX/RBP/RDI/RSI/R12-R15、XMM6-15）我们一个都不碰；
 *     易失的 RCX/RDX/R8/R9/R10/R11 全部保存恢复。XMM0-5 在入口处是 scratch
 *     （OnUpdate 只有一个指针参数，没有浮点参数），无需保存。
 *
 * 入口补丁（13 字节，正好等于原序言长度）：
 *   48 b8 <stub 8 字节>  mov rax, stub
 *   ff e0                jmp rax
 *   90                   nop
 * ------------------------------------------------------------------ */
#define STUB_SIZE 0x43

static int build_trampoline(uint8_t *target)
{
    uint8_t patch[PROLOGUE_LEN];
    uint8_t *s;
    uint64_t back = (uint64_t)(target + PROLOGUE_LEN);
    uint64_t fn   = (uint64_t)&mouse_tick;
    DWORD old;
    int i = 0;

    g_stub = (uint8_t *)VirtualAlloc(NULL, STUB_SIZE, MEM_COMMIT | MEM_RESERVE,
                                     PAGE_EXECUTE_READWRITE);
    if (!g_stub) { rk_log("[hook] VirtualAlloc 失败 err=%lu", GetLastError()); return 0; }
    s = g_stub;

    s[i++]=0x51; s[i++]=0x52;
    s[i++]=0x41; s[i++]=0x50;
    s[i++]=0x41; s[i++]=0x51;
    s[i++]=0x41; s[i++]=0x52;
    s[i++]=0x41; s[i++]=0x53;
    s[i++]=0x48; s[i++]=0x83; s[i++]=0xec; s[i++]=0x28;
    s[i++]=0x48; s[i++]=0xb8; memcpy(s+i,&fn,8); i+=8;
    s[i++]=0xff; s[i++]=0xd0;
    s[i++]=0x48; s[i++]=0x83; s[i++]=0xc4; s[i++]=0x28;
    s[i++]=0x41; s[i++]=0x5b;
    s[i++]=0x41; s[i++]=0x5a;
    s[i++]=0x41; s[i++]=0x59;
    s[i++]=0x41; s[i++]=0x58;
    s[i++]=0x5a; s[i++]=0x59;
    memcpy(s+i, target, PROLOGUE_LEN); i += PROLOGUE_LEN;
    s[i++]=0xff; s[i++]=0x25; s[i++]=0x00; s[i++]=0x00; s[i++]=0x00; s[i++]=0x00;
    memcpy(s+i,&back,8); i += 8;
    if (i != STUB_SIZE) { rk_log("[hook] 跳板长度 %d != %d，设计与实现不符", i, STUB_SIZE); return 0; }

    /* 入口补丁 */
    memset(patch, 0x90, sizeof patch);
    patch[0]=0x48; patch[1]=0xb8;
    { uint64_t st=(uint64_t)g_stub; memcpy(patch+2,&st,8); }
    patch[10]=0xff; patch[11]=0xe0;

    if (!VirtualProtect(target, PROLOGUE_LEN, PAGE_EXECUTE_READWRITE, &old)) {
        rk_log("[hook] VirtualProtect 失败 err=%lu", GetLastError()); return 0;
    }
    memcpy(target, patch, PROLOGUE_LEN);
    VirtualProtect(target, PROLOGUE_LEN, old, &old);
    FlushInstructionCache(GetCurrentProcess(), target, PROLOGUE_LEN);
    return 1;
}

/* ------------------------------------------------------------------ *
 * 装载
 * ------------------------------------------------------------------ */
static float ini_float(const char *key, float dflt)
{
    char buf[64], *end;
    double v;
    if (!GetPrivateProfileStringA("mouse", key, "", buf, sizeof buf, g_ini) || !buf[0])
        return dflt;
    v = strtod(buf, &end);
    return (end == buf) ? dflt : (float)v;
}

static void ini_save_float(const char *key, float v)
{
    char buf[64];
    snprintf(buf, sizeof buf, "%.6f", (double)v);
    WritePrivateProfileStringA("mouse", key, buf, g_ini);
}

static void load_ini(const char *dir)
{
    snprintf(g_ini, sizeof g_ini, "%s\\th06nc_mouse.ini", dir);
    g_origin_x  = ini_float("origin_x", DEFAULT_ORIGIN_X);
    g_origin_y  = ini_float("origin_y", DEFAULT_ORIGIN_Y);
    g_toggle_vk = GetPrivateProfileIntA("mouse", "toggle_vk", VK_F9,  g_ini);
    g_calib_vk  = GetPrivateProfileIntA("mouse", "calib_vk",  VK_F10, g_ini);
    g_corner_vk = GetPrivateProfileIntA("mouse", "corner_vk", VK_F11, g_ini);
    g_on        = GetPrivateProfileIntA("mouse", "enabled",   1,      g_ini) != 0;

    g_cal_w = GetPrivateProfileIntA("mouse", "cal_client_w", 0, g_ini);
    g_cal_h = GetPrivateProfileIntA("mouse", "cal_client_h", 0, g_ini);
    g_cal_sx = ini_float("cal_scale_x", 0.0f);
    g_cal_sy = ini_float("cal_scale_y", 0.0f);
    g_cal_ox = ini_float("cal_off_x", 0.0f);
    g_cal_oy = ini_float("cal_off_y", 0.0f);
    g_cal = (g_cal_w > 0 && g_cal_h > 0 && g_cal_sx != 0.0f && g_cal_sy != 0.0f);
}

/* 两点求解仿射映射：客户区像素 = off + scale * 游戏坐标。
 * 两种标定方式（瞄自机 / 点关卡区角）都汇到这里。 */
static void calib_solve(float c1x, float c1y, float p1x, float p1y,
                        float c2x, float c2y, float p2x, float p2y,
                        int cw, int ch, const char *how)
{
    float dpx = p2x - p1x, dpy = p2y - p1y;
    if ((dpx < 0 ? -dpx : dpx) < 40.0f || (dpy < 0 ? -dpy : dpy) < 40.0f) {
        rk_log("[calib] %s：两点太近（游戏坐标只差 %.0f,%.0f），各需 >=40。重来。",
               how, dpx, dpy);
        return;
    }
    g_cal_sx = (c2x - c1x) / dpx;
    g_cal_sy = (c2y - c1y) / dpy;
    g_cal_ox = c1x - g_cal_sx * p1x;
    g_cal_oy = c1y - g_cal_sy * p1y;
    g_cal_w = cw; g_cal_h = ch;
    g_cal = 1;
    ini_save_float("cal_scale_x", g_cal_sx);
    ini_save_float("cal_scale_y", g_cal_sy);
    ini_save_float("cal_off_x",   g_cal_ox);
    ini_save_float("cal_off_y",   g_cal_oy);
    {   char b[32];
        snprintf(b, sizeof b, "%d", cw); WritePrivateProfileStringA("mouse", "cal_client_w", b, g_ini);
        snprintf(b, sizeof b, "%d", ch); WritePrivateProfileStringA("mouse", "cal_client_h", b, g_ini); }
    rk_log("[calib] %s 完成并已写入 ini：scale=(%.4f,%.4f) off=(%.1f,%.1f) client=%dx%d",
           how, g_cal_sx, g_cal_sy, g_cal_ox, g_cal_oy, cw, ch);

    /* ---- 给逆向侧看的：把结果换算成「布局」而不是「两个数」，
     * 只有这样才能判断能不能推广到别的分辨率、进而烤成默认值。 ---- */
    {
        const float *area = (const float *)(g_base + RVA_MOVEMENT_AREA);
        float l = g_cal_ox, t = g_cal_oy;                 /* 游戏坐标 (0,0) 处 */
        float w = g_cal_sx * PLAYFIELD_W, h = g_cal_sy * PLAYFIELD_H;
        RECT wr = {0,0,0,0};
        GetWindowRect(GetForegroundWindow(), &wr);
        rk_log("[layout] movementArea = topLeft(%.1f,%.1f) size(%.1f,%.1f)  <- 游戏自己的值",
               area[0], area[1], area[2], area[3]);
        rk_log("[layout] 关卡区在客户区里 = left=%.1f top=%.1f w=%.1f h=%.1f (right=%.1f bottom=%.1f)",
               l, t, w, h, l + w, t + h);
        rk_log("[layout] 占客户区比例 = left=%.5f top=%.5f w=%.5f h=%.5f",
               l / cw, t / ch, w / cw, h / ch);
        rk_log("[layout] 每游戏单位像素 sx=%.5f sy=%.5f 各向同性=%s",
               g_cal_sx, g_cal_sy,
               (g_cal_sx > g_cal_sy * 0.999f && g_cal_sx < g_cal_sy * 1.001f) ? "是" : "否");
        rk_log("[layout] 若无黑边则虚拟屏 = %.1f x %.1f （640x480 的倍数？）",
               cw / g_cal_sx, ch / g_cal_sy);
        rk_log("[layout] 关卡区原点(虚拟屏单位) = (%.2f, %.2f)  TH06 是 (32,16)",
               l / g_cal_sx, t / g_cal_sy);
        rk_log("[layout] client=%dx%d window=%ldx%ld screen=%dx%d",
               cw, ch, wr.right - wr.left, wr.bottom - wr.top,
               GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN));
    }
}

/* 瞄自机法：按一次记一个「光标正压在自机上」的点。 */
static void calib_point(float cx, float cy, float px, float py, int cw, int ch)
{
    if (!g_cal_pending) {
        g_cal_c1x = cx; g_cal_c1y = cy; g_cal_p1x = px; g_cal_p1y = py;
        g_cal_pending = 1;
        rk_log("[calib] 第 1 点：光标=(%.0f,%.0f) 自机=(%.1f,%.1f)。"
               "把自机挪远（x,y 各至少 40），光标再压上去按一次。", cx, cy, px, py);
        return;
    }
    g_cal_pending = 0;
    calib_solve(g_cal_c1x, g_cal_c1y, g_cal_p1x, g_cal_p1y, cx, cy, px, py, cw, ch, "瞄自机");
}

int mouse_install(void)
{
    char dir[MAX_PATH];
    uint8_t *target;

    g_base = (uint8_t *)GetModuleHandleW(NULL);
    GetModuleFileNameA(NULL, dir, sizeof dir);
    { char *p = strrchr(dir, '\\'); if (p) *p = 0; }

    { char lp[MAX_PATH]; snprintf(lp, sizeof lp, "%s\\th06nc_mouse.log", dir);
      g_log = fopen(lp, "w");
      if (!g_log) { char t[MAX_PATH]; GetTempPathA(sizeof t, t);
                    strncat(t, "th06nc_mouse.log", sizeof t - strlen(t) - 1);
                    g_log = fopen(t, "w"); } }

    rk_log("th06nc mouse-control —— imagebase=%p", (void *)g_base);
    {   /* 把「哪个代理、DLL 在哪、宿主 exe 是谁」写清楚：
         * 换投递点之后，这三行是判断「到底哪一个被加载了」的唯一依据。 */
        char self[MAX_PATH] = {0};
        GetModuleFileNameA((HMODULE)g_self, self, sizeof self);
        rk_log("[who] 代理 = %s", g_tag ? g_tag : "(未标注)");
        rk_log("[who] 本 DLL  = %s", self);
        rk_log("[who] 游戏目录 = %s", dir);
    }
    load_ini(dir);
    rk_log("[ini] origin=(%.0f,%.0f) toggle_vk=0x%02x", g_origin_x, g_origin_y, g_toggle_vk);

    target = g_base + RVA_PLAYER_ONUPDATE;
    /* 守门：序言不是我们记的那 13 字节 = 不是这个 build，什么都不做。 */
    if (memcmp(target, k_expected_prologue, PROLOGUE_LEN) != 0) {
        rk_log("[guard] Player__OnUpdate 序言与预期不符，永久停用。实际:");
        rk_log("        %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x",
               target[0],target[1],target[2],target[3],target[4],target[5],target[6],
               target[7],target[8],target[9],target[10],target[11],target[12]);
        g_disabled = 1;
        return 0;
    }
    if (!build_trampoline(target)) { g_disabled = 1; return 0; }
    rk_log("[guard] 序言签名匹配，跳板就绪（%s 开关鼠标控制）",
           g_toggle_vk == VK_F9 ? "F9" : "配置的按键");
    return 1;
}
