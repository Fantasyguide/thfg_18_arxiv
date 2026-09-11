/*
 * hook.c —— 单文件 x64 内联 hook + 每帧「读世界 → 策略 → 写输入」。
 *
 * ★ 设计要点（照抄 mods/th06nc/mouse-control 那条已实机验证的路线）：
 *   **只写 INPUT_HELD 一个 dword 的低 7 位，玩家对象零写入。**
 *   移动、钳位、低速、炸弹的按下沿判定，全部由游戏自己的代码完成 ——
 *   我们只替它决定「这一帧按了哪些键」。
 *
 * 不调用任何引擎函数、不碰引擎的栈、不碰 FPU/SSE 状态：
 *   AUDIT-checklist 的 A 节整节不适用，C 节只剩「偏移是否有一手出处」。
 * 唯一的手写机器码是那段跳板，逐字节说明见 build_trampoline()。
 *
 * 与 mouse-control 的差别：回调里跑的是 extract + policy；写入的是全部 7 个逻辑按钮位
 * （开启时键盘完全让位，C 键关掉即恢复）；另外每帧往 AsciiManager 的字符串表追加一条
 * 「AI Control On/Off」——写法照抄引擎自己画 FPS 的那段内联 AddString（engine/anm/th06nc/03）。
 */
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "th06nc.h"
#include "extract.h"
#include "policy.h"

static uint8_t *g_base;
static uint8_t *g_stub;
/* 三种模式，C 键轮换：MANUAL（不插手）→ AUTO（AI 接管 + 左键按住时朝光标引导）→ BYPASS（AI 完全接管） */
enum { MODE_MANUAL = 0, MODE_AUTO = 1, MODE_BYPASS = 2 };
static int      g_mode = MODE_MANUAL;
static const char *MODE_NAME[3] = { "MANUAL MODE", "AUTO MODE", "BYPASS MODE" };
static const uint32_t MODE_COLOR[3] = { 0xffa0a0a0u, 0xffffe040u, 0xffff5050u };   /* 灰 / 黄 / 红（AARRGGBB） */
static int      g_disabled;
static int      g_toggle_vk = 'C';    /* NC 不用 C 键（只认 Z/X/Shift/方向/Esc），拿来当开关 */
static int      g_log_every = 60;     /* 每 N 帧记一行状态；0 = 不记 */
static int      g_show_text = 1;      /* 屏幕上显示当前模式 */
static float    g_text_scale = 0.75f;
static float    g_text_dx = 4.0f, g_text_dy = 20.0f;   /* 相对关卡区左上角 */
static FILE    *g_log;
static const char *g_tag;
static void       *g_self;
static ap_world_t  g_world;           /* ~23 KB，静态，不在回调里分配 */
static ap_policy_t g_policy;
static uint32_t    g_frames;

void autoplay_set_identity(void *hmodule, const char *tag) { g_self = hmodule; g_tag = tag; }

/* 日志同时走 OutputDebugStringA（DebugView 可见），不依赖文件权限 */
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
    OutputDebugStringA("[th06nc-autoplay] ");
    OutputDebugStringA(buf);
    OutputDebugStringA("\n");
    if (!g_log) return;
    fputs(buf, g_log);
    fputc('\n', g_log);
    fflush(g_log);
}

static int d_entry, d_badstate, d_ok;
#define ONCE(flag, ...) do { if (!(flag)) { (flag) = 1; rk_log(__VA_ARGS__); } } while (0)

/* 协议动作位 → th06nc 逻辑按钮位（engine/menu/th06nc/02 §8） */
static uint32_t map_buttons(uint32_t b)
{
    uint32_t o = 0;
    if (b & BTN_UP)    o |= IN_UP;
    if (b & BTN_DOWN)  o |= IN_DOWN;
    if (b & BTN_LEFT)  o |= IN_LEFT;
    if (b & BTN_RIGHT) o |= IN_RIGHT;
    if (b & BTN_SHOT)  o |= IN_SHOOT;
    if (b & BTN_BOMB)  o |= IN_BOMB;
    if (b & BTN_SLOW)  o |= IN_FOCUS;
    return o;
}

/* ------------------------------------------------------------------ *
 * HUD 文字：往 g_AsciiManager 的字符串表追加一条记录（engine/anm/th06nc/03 §3）。
 * 与引擎画 FPS 的内联 AddString（0x14007a3a0）逐字段相同；isGui / isSelected 照引擎
 * 的做法拷管理器当前值。draw 回调画完会把 numStrings 清零，所以每帧调一次。
 * ------------------------------------------------------------------ */
static void ascii_add(float x, float y, float scale, uint32_t color, const char *text)
{
    uint8_t *mgr = g_base + RVA_ASCII_MANAGER;
    int32_t *n = (int32_t *)(mgr + ASCII_MGR_NUM_STRINGS);
    uint8_t *rec;
    size_t len;
    if (*n < 0 || *n >= ASCII_MAX_STRINGS) return;
    rec = mgr + ASCII_MGR_STRINGS + (size_t)(*n) * ASCII_STRING_SIZE;
    *(float *)(rec + ASCII_STR_SCALE_X) = scale;
    *(float *)(rec + ASCII_STR_SCALE_Y) = scale;
    *(uint32_t *)(rec + ASCII_STR_COLOR) = color;
    rec[ASCII_STR_IS_GUI] = mgr[ASCII_MGR_IS_GUI];
    *(float *)(rec + ASCII_STR_POS + 0) = x;
    *(float *)(rec + ASCII_STR_POS + 4) = y;
    *(float *)(rec + ASCII_STR_POS + 8) = 0.0f;
    len = strlen(text);
    if (len > ASCII_STR_TEXT_CAP - 1) len = ASCII_STR_TEXT_CAP - 1;
    memcpy(rec + ASCII_STR_TEXT, text, len);
    rec[ASCII_STR_TEXT + len] = 0;
    rec[ASCII_STR_IS_SELECTED] = mgr[ASCII_MGR_IS_SELECTED];
    (*n)++;
}

/* 关卡区左上角在虚拟屏里的位置：高固定 480，宽随客户区宽高比伸展，关卡区水平居中、顶边 16
 * （mouse-control/TARGET.md §6 实测；FPS 画在 (758,464) 也印证宽 > 640）。 */
static void overlay_draw(HWND hwnd, int mode)
{
    RECT rc;
    float w = 640.0f, left;
    if (GetClientRect(hwnd, &rc) && rc.bottom > 0)
        w = ASCII_VIRT_H * (float)rc.right / (float)rc.bottom;
    left = (w - PLAYFIELD_W) * 0.5f;
    ascii_add(left + g_text_dx, g_text_dy, g_text_scale, MODE_COLOR[mode], MODE_NAME[mode]);
    /* 下面一行小字：字号 0.6 倍，行距按上一行的行高（scale × 16） */
    ascii_add(left + g_text_dx, g_text_dy + g_text_scale * ASCII_LINE_HEIGHT, g_text_scale * 0.6f,
              0xffc0c0c0u, "(press c to switch)");
}

/* 光标 → 协议坐标（x 中轴为 0、y 顶边为 0）。布局模型照 mouse-control/TARGET.md §6（两次实测标定）：
 * 虚拟屏高 480、宽随宽高比；关卡区 384×448 水平居中、顶边 16；缩放 = 客户区高 / 480。
 * 返回 0 = 取不到 / 客户区无效。非 16:9 窗口未验证（🟡）。 */
static int cursor_to_field(HWND hwnd, float *gx, float *gy)
{
    POINT pt; RECT rc; float s, left, top;
    if (!GetCursorPos(&pt) || !ScreenToClient(hwnd, &pt) || !GetClientRect(hwnd, &rc)) return 0;
    if (rc.right <= 0 || rc.bottom <= 0) return 0;
    s = (float)rc.bottom / ASCII_VIRT_H;
    left = ((float)rc.right - PLAYFIELD_W * s) * 0.5f;
    top = 16.0f * s;
    *gx = ((float)pt.x - left) / s - PLAYFIELD_W * 0.5f;
    *gy = ((float)pt.y - top) / s;
    return 1;
}

/* ------------------------------------------------------------------ *
 * 每帧回调：在 Player__OnUpdate 真正开始之前跑
 * ------------------------------------------------------------------ */
void agent_tick(void)
{
    uint32_t *held;
    uint32_t  buttons, bits;
    HWND      hwnd;

    ONCE(d_entry, "[diag] agent_tick 首次触发");
    if (g_disabled) return;

    hwnd = GetForegroundWindow();
    if (!hwnd || hwnd != GetActiveWindow()) return;   /* 不是前台就彻底不插手 */

    if (GetAsyncKeyState(g_toggle_vk) & 1) {
        g_mode = (g_mode + 1) % 3;
        rk_log("[autoplay] -> %s", MODE_NAME[g_mode]);
    }
    if (g_show_text) overlay_draw(hwnd, g_mode);   /* 三种模式都画 */
    if (g_mode == MODE_MANUAL) return;

    th06nc_extract(g_base, &g_world);
    g_frames++;

    /* AUTO：左键按住 → 朝光标引导（光标钳到可动区内）；否则与 BYPASS 相同 */
    if (g_mode == MODE_AUTO && (GetAsyncKeyState(VK_LBUTTON) & 0x8000)) {
        float gx, gy;
        if (cursor_to_field(hwnd, &gx, &gy)) {
            if (gx < g_world.area.xmin) gx = g_world.area.xmin; else if (gx > g_world.area.xmax) gx = g_world.area.xmax;
            if (gy < g_world.area.ymin) gy = g_world.area.ymin; else if (gy > g_world.area.ymax) gy = g_world.area.ymax;
            ap_policy_set_guide(&g_policy, 1, gx, gy);
        } else ap_policy_set_guide(&g_policy, 0, 0.0f, 0.0f);
    } else ap_policy_set_guide(&g_policy, 0, 0.0f, 0.0f);

    if (!(g_world.phase & AP_PHASE_PLAYER_CONTROLLABLE)) {
        ONCE(d_badstate, "[diag] playerState=%u —— 本帧不接管", g_world.player.state);
        return;
    }

    buttons = ap_policy_decide(&g_policy, &g_world);
    bits = map_buttons(buttons);

    ONCE(d_ok, "[diag] 首次生效 frame=%u pos=(%.1f,%.1f) r=%.2f speed=%.1f/%.1f bullets=%d enemies=%d lasers=%d area=[%.0f,%.0f]x[%.0f,%.0f] -> held=0x%02x",
         g_world.frame, g_world.player.x, g_world.player.y, g_world.player.hit_radius,
         g_world.player.speed, g_world.player.speed_focus, g_world.nbullets, g_world.nenemies, g_world.nlasers,
         g_world.area.xmin, g_world.area.xmax, g_world.area.ymin, g_world.area.ymax, bits);
    if (g_log_every > 0 && (g_frames % (uint32_t)g_log_every) == 0)
        rk_log("[tick] %s frame=%u pos=(%.1f,%.1f) st=%u b=%d e=%d l=%d lives=%u bombs=%u phase=0x%x F=(%.2f,%.2f) guide=%d(%.0f,%.0f) held=0x%02x",
               MODE_NAME[g_mode], g_world.frame, g_world.player.x, g_world.player.y, g_world.player.state,
               g_world.nbullets, g_world.nenemies, g_world.nlasers, g_world.player.lives, g_world.player.bombs,
               g_world.phase, g_policy.last_fx, g_policy.last_fy, g_policy.guide_on, g_policy.guide_x, g_policy.guide_y, bits);
    if (buttons & BTN_BOMB) rk_log("[bomb] frame=%u %d 帧内九向全部会撞，用炸弹", g_world.frame, g_policy.vo_frames);

    /* 唯一的一次写：7 个逻辑按钮位整体覆盖，其余位（菜单等）一位不碰 */
    held = (uint32_t *)(g_base + RVA_INPUT_HELD);
    *held = (*held & ~(uint32_t)IN_ALL_MASK) | bits;
}

/* ------------------------------------------------------------------ *
 * 跳板 —— 本 mod 唯一的手写机器码（与 mouse-control 逐字节相同，只换了 fn）
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
 *   0e   48 b8 <imm64>            mov  rax, agent_tick
 *   18   ff d0                    call rax                 压 8 → 被调方 ≡8 ✅
 *   1a   48 83 c4 28              add  rsp,0x28
 *   1e   41 5b / 41 5a / 41 59 / 41 58 / 5a / 59   逆序 pop
 *   28   <13 字节原序言>          MOV RAX,RSP / PUSH RDI / PUSH R12 / SUB RSP,0xd8
 *   35   ff 25 00 00 00 00        jmp qword ptr [rip+0]    不碰任何寄存器
 *   3b   <8 字节绝对地址 = OnUpdate+13>
 *
 * 三条关键性质：
 *  1. **0x28 = 40 ≥ 32 字节 shadow space**，且让 rsp 在 call 前 16 字节对齐。
 *  2. **RAX 可以随便用**：入口处 RAX 是 scratch，且原序言第一条就是 MOV RAX,RSP。
 *     返回跳转用 `jmp [rip+0]` 而不是 `mov rax;jmp rax`，正是为了不破坏刚建立的 RAX。
 *  3. 非易失寄存器我们一个都不碰；易失的 RCX/RDX/R8/R9/R10/R11 全部保存恢复。
 *     XMM0-5 在入口处是 scratch（OnUpdate 只有一个指针参数，没有浮点参数）。
 *     ★ agent_tick 会大量用 SSE 做浮点 —— 它只碰 XMM0-5（易失）与经 ABI 保存的 XMM6-15
 *       由编译器在 agent_tick 序言里保存 / 还原，跳板不必管。
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
    uint64_t fn   = (uint64_t)&agent_tick;
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
 * ini：<游戏目录>/th06nc_autoplay.ini，[autoplay] 节，全部可选
 * ------------------------------------------------------------------ */
static void load_ini(const char *dir)
{
    char ini[MAX_PATH];
    snprintf(ini, sizeof ini, "%s\\th06nc_autoplay.ini", dir);
    g_mode      = GetPrivateProfileIntA("autoplay", "start_mode", MODE_MANUAL, ini);
    if (g_mode < 0 || g_mode > 2) g_mode = MODE_MANUAL;
    g_toggle_vk = GetPrivateProfileIntA("autoplay", "toggle_vk", 'C',      ini);
    g_log_every = GetPrivateProfileIntA("autoplay", "log_every", 60,       ini);
    g_show_text = GetPrivateProfileIntA("autoplay", "show_text", 1,        ini) ? 1 : 0;
    g_text_scale = (float)GetPrivateProfileIntA("autoplay", "text_scale_pct", 75, ini) / 100.0f;
    g_text_dx = (float)GetPrivateProfileIntA("autoplay", "text_x", 4,  ini);
    g_text_dy = (float)GetPrivateProfileIntA("autoplay", "text_y", 20, ini);
    if (g_text_scale <= 0.0f) g_text_scale = 0.75f;
    g_policy.horizon       = GetPrivateProfileIntA("autoplay", "horizon",       g_policy.horizon, ini);
    g_policy.bomb_cooldown = GetPrivateProfileIntA("autoplay", "bomb_cooldown", g_policy.bomb_cooldown, ini);
    g_policy.vo_frames     = GetPrivateProfileIntA("autoplay", "vo_frames",     g_policy.vo_frames, ini);
    g_policy.tau        = (float)GetPrivateProfileIntA("autoplay", "tau",         (int)g_policy.tau, ini);
    g_policy.safe_r     = (float)GetPrivateProfileIntA("autoplay", "safe_r",      (int)g_policy.safe_r, ini);
    g_policy.vortex     = (float)GetPrivateProfileIntA("autoplay", "vortex_pct",  (int)(g_policy.vortex * 100.0f), ini) / 100.0f;
    g_policy.slow_dist  = (float)GetPrivateProfileIntA("autoplay", "slow_dist",   (int)g_policy.slow_dist, ini);
    g_policy.k_att      = (float)GetPrivateProfileIntA("autoplay", "att_x_pct",   (int)(g_policy.k_att * 100.0f), ini) / 100.0f;
    g_policy.k_att_y    = (float)GetPrivateProfileIntA("autoplay", "att_y_pct",   (int)(g_policy.k_att_y * 100.0f), ini) / 100.0f;
    g_policy.home_y     = (float)GetPrivateProfileIntA("autoplay", "home_y",      (int)g_policy.home_y, ini);
    g_policy.wall_margin        = (float)GetPrivateProfileIntA("autoplay", "wall_margin",        (int)g_policy.wall_margin, ini);
    g_policy.wall_margin_bottom = (float)GetPrivateProfileIntA("autoplay", "wall_margin_bottom", (int)g_policy.wall_margin_bottom, ini);
    g_policy.wall_k_bottom      = (float)GetPrivateProfileIntA("autoplay", "wall_bottom_pct",    (int)(g_policy.wall_k_bottom * 100.0f), ini) / 100.0f;
    g_policy.k_guide       = (float)GetPrivateProfileIntA("autoplay", "guide_pct",     (int)(g_policy.k_guide * 100.0f), ini) / 100.0f;
    g_policy.guide_dead    = (float)GetPrivateProfileIntA("autoplay", "guide_dead",    (int)g_policy.guide_dead, ini);
    g_policy.guide_release = (float)GetPrivateProfileIntA("autoplay", "guide_release", (int)g_policy.guide_release, ini);
    if (g_policy.guide_release < g_policy.guide_dead) g_policy.guide_release = g_policy.guide_dead;
    g_policy.smooth      = (float)GetPrivateProfileIntA("autoplay", "smooth_pct",      (int)(g_policy.smooth * 100.0f), ini) / 100.0f;
    g_policy.switch_hyst = (float)GetPrivateProfileIntA("autoplay", "switch_hyst_pct", (int)(g_policy.switch_hyst * 100.0f), ini) / 100.0f;
    g_policy.hold_frames = GetPrivateProfileIntA("autoplay", "hold_frames", g_policy.hold_frames, ini);
    g_policy.slow_exit   = (float)GetPrivateProfileIntA("autoplay", "slow_exit",   (int)g_policy.slow_exit, ini);
    g_policy.slow_hold   = GetPrivateProfileIntA("autoplay", "slow_hold",   g_policy.slow_hold, ini);
    if (g_policy.smooth <= 0.0f || g_policy.smooth > 1.0f) g_policy.smooth = 1.0f;
    if (g_policy.slow_exit < g_policy.slow_dist) g_policy.slow_exit = g_policy.slow_dist;
    if (g_policy.horizon < 1) g_policy.horizon = 1;
    if (g_policy.horizon > 240) g_policy.horizon = 240;
    if (g_policy.vo_frames < 1) g_policy.vo_frames = 1;
    if (g_policy.tau < 1.0f) g_policy.tau = 1.0f;
    if (g_policy.safe_r < 8.0f) g_policy.safe_r = 8.0f;
}

/* ------------------------------------------------------------------ *
 * 装载
 * ------------------------------------------------------------------ */
int autoplay_install(void)
{
    char dir[MAX_PATH];
    uint8_t *target;

    g_base = (uint8_t *)GetModuleHandleW(NULL);
    GetModuleFileNameA(NULL, dir, sizeof dir);
    { char *p = strrchr(dir, '\\'); if (p) *p = 0; }

    { char lp[MAX_PATH]; snprintf(lp, sizeof lp, "%s\\th06nc_autoplay.log", dir);
      g_log = fopen(lp, "w");
      if (!g_log) { char t[MAX_PATH]; GetTempPathA(sizeof t, t);
                    strncat(t, "th06nc_autoplay.log", sizeof t - strlen(t) - 1);
                    g_log = fopen(t, "w"); } }

    rk_log("th06nc autoplay —— imagebase=%p", (void *)g_base);
    {
        char self[MAX_PATH] = {0};
        GetModuleFileNameA((HMODULE)g_self, self, sizeof self);
        rk_log("[who] 代理 = %s", g_tag ? g_tag : "(未标注)");
        rk_log("[who] 本 DLL  = %s", self);
        rk_log("[who] 游戏目录 = %s", dir);
    }
    ap_policy_init(&g_policy);
    load_ini(dir);
    rk_log("[ini] start_mode=%s toggle_vk=0x%02x horizon=%d tau=%.0f safe_r=%.0f vortex=%.2f vo_frames=%d slow_dist=%.0f att=(%.2f,%.2f) home_y=%.0f wall=(%.0f,%.0f x%.2f) smooth=%.2f hyst=%.2f hold=%d slow_exit=%.0f bomb_cooldown=%d log_every=%d show_text=%d scale=%.2f at=(+%.0f,%.0f)",
           MODE_NAME[g_mode], g_toggle_vk, g_policy.horizon, g_policy.tau, g_policy.safe_r, g_policy.vortex, g_policy.vo_frames,
           g_policy.slow_dist, g_policy.k_att, g_policy.k_att_y, g_policy.home_y, g_policy.wall_margin, g_policy.wall_margin_bottom,
           g_policy.wall_k_bottom, g_policy.smooth, g_policy.switch_hyst, g_policy.hold_frames, g_policy.slow_exit,
           g_policy.bomb_cooldown, g_log_every, g_show_text, g_text_scale, g_text_dx, g_text_dy);

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
    rk_log("[guard] 序言签名匹配，跳板就绪（vk=0x%02x 轮换 MANUAL / AUTO / BYPASS）", g_toggle_vk);
    return 1;
}
