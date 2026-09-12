/*
 * proxy_vcr.c —— 把自己伪装成 VCRUNTIME140_1.dll。
 *
 * ★ 为什么换到这里（xinput1_4.dll 那条路实测没被加载）：
 *
 *   | | xinput1_4.dll | VCRUNTIME140_1.dll |
 *   | 在导入表里 | ❌ 运行时 LoadLibraryW | ✅ **静态导入** |
 *   | 会不会被跳过 | 会 —— th06nc:0x1400ab7b4 有个 JNZ 闸门 | **不会**，加载器在 exe 入口点前必须加载 |
 *   | KnownDLL | 否 | 否 |
 *   | 要转发几个导出 | 4 | **1** |
 *
 *   即：这条路不依赖游戏跑到哪一段代码。exe 导入了它，它就必须被加载，
 *   否则进程根本起不来。
 *
 * 唯一的导出是 __CxxFrameHandler4 —— C++ 异常展开时由系统 unwinder 调用的
 * language-specific handler。**不能用 C 包装函数转发**：它的调用现场由
 * unwinder 建立，包装函数会插进自己的栈帧。所以用一条 naked jmp 直接跳过去，
 * 寄存器与栈原样保留（逐字节审计见 AUDIT.md §B.5）。
 */
#include <windows.h>

extern int  autoplay_install(void);
extern void autoplay_set_identity(void *hmodule, const char *tag);

/* jmp 的目标。必须是非 static，下面的汇编按符号名引用它。 */
void *g_real_CxxFrameHandler4;

/* 6 字节：ff 25 <rel32>  =  jmp qword ptr [rip + disp32]
 * 不碰任何寄存器、不动栈，等价于「这次调用本来就发生在真身里」。 */
__asm__(
    ".globl __CxxFrameHandler4\n"
    "__CxxFrameHandler4:\n"
    "    jmp *g_real_CxxFrameHandler4(%rip)\n"
);

BOOL WINAPI DllMain(HINSTANCE h, DWORD reason, LPVOID reserved)
{
    char path[MAX_PATH];
    UINT n;
    HMODULE real;

    (void)reserved;
    if (reason != DLL_PROCESS_ATTACH) return TRUE;
    DisableThreadLibraryCalls(h);

    /* 先把真身接上。接不上就必须让进程**响亮地**失败（返回 FALSE →
     * 0xc0000142 启动错误框），而不是留一个 NULL 指针等异常展开时才炸。 */
    n = GetSystemDirectoryA(path, MAX_PATH);
    if (!n || n > MAX_PATH - 24) return FALSE;
    lstrcatA(path, "\\VCRUNTIME140_1.dll");
    real = LoadLibraryA(path);
    if (!real) return FALSE;
    g_real_CxxFrameHandler4 = (void *)GetProcAddress(real, "__CxxFrameHandler4");
    if (!g_real_CxxFrameHandler4) return FALSE;

    /* 到这里转发已经就绪，游戏一定能正常跑。hook 装不上只写日志，不影响游戏。 */
    autoplay_set_identity(h, "VCRUNTIME140_1.dll");
    autoplay_install();
    return TRUE;
}
