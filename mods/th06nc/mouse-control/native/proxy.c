/*
 * proxy.c —— 把自己伪装成 xinput1_4.dll，转发给 System32 的真身。
 *
 * 为什么选 XInput（TARGET.md §4 有完整论证）：
 *   · NC 是**运行时** LoadLibrary("xinput1_4.dll")，不在静态导入表里；
 *   · XInput **不是 KnownDLL**，所以应用程序目录优先于 System32；
 *   · exe 没有 SetDefaultDllDirectories（只有 SetDllDirectoryA，那是加路径不是收紧）。
 * 于是把本 DLL 命名成 xinput1_4.dll 丢进游戏目录即可，**不需要注入器**。
 *
 * 刻意**不**代理 steam_api64.dll：那是 Steam 的 API，动它既不必要、
 * 也容易被误读成绕过什么。XInput 是干净的选择。
 */
#include <windows.h>

extern int  mouse_install(void);
extern void mouse_set_identity(void *hmodule, const char *tag);

typedef DWORD (WINAPI *pfnGetState)(DWORD, void *);
typedef DWORD (WINAPI *pfnSetState)(DWORD, void *);
typedef DWORD (WINAPI *pfnGetCaps)(DWORD, DWORD, void *);
typedef void  (WINAPI *pfnEnable)(BOOL);

static HMODULE   g_real;
static pfnGetState real_GetState;
static pfnSetState real_SetState;
static pfnGetCaps  real_GetCaps;
static pfnEnable   real_Enable;

static void load_real(void)
{
    char path[MAX_PATH];
    UINT n;
    if (g_real) return;
    n = GetSystemDirectoryA(path, MAX_PATH);
    if (!n || n > MAX_PATH - 20) return;
    lstrcatA(path, "\\xinput1_4.dll");
    g_real = LoadLibraryA(path);
    if (!g_real) return;
    real_GetState = (pfnGetState)GetProcAddress(g_real, "XInputGetState");
    real_SetState = (pfnSetState)GetProcAddress(g_real, "XInputSetState");
    real_GetCaps  = (pfnGetCaps) GetProcAddress(g_real, "XInputGetCapabilities");
    real_Enable   = (pfnEnable)  GetProcAddress(g_real, "XInputEnable");
}

#define ERROR_DEVICE_NOT_CONNECTED_ 1167

__declspec(dllexport) DWORD WINAPI XInputGetState(DWORD i, void *s)
{
    load_real();
    return real_GetState ? real_GetState(i, s) : ERROR_DEVICE_NOT_CONNECTED_;
}
__declspec(dllexport) DWORD WINAPI XInputSetState(DWORD i, void *v)
{
    load_real();
    return real_SetState ? real_SetState(i, v) : ERROR_DEVICE_NOT_CONNECTED_;
}
__declspec(dllexport) DWORD WINAPI XInputGetCapabilities(DWORD i, DWORD f, void *c)
{
    load_real();
    return real_GetCaps ? real_GetCaps(i, f, c) : ERROR_DEVICE_NOT_CONNECTED_;
}
__declspec(dllexport) void WINAPI XInputEnable(BOOL e)
{
    load_real();
    if (real_Enable) real_Enable(e);
}

BOOL WINAPI DllMain(HINSTANCE h, DWORD reason, LPVOID reserved)
{
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(h);
        load_real();
        /* 装 hook。失败只写日志，游戏照常跑 —— 手柄转发不受影响。 */
        mouse_set_identity(h, "xinput1_4.dll");
        mouse_install();
    }
    return TRUE;
}
