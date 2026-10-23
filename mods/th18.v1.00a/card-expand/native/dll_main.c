/* dll_main.c —— th18_card_expand.dll 的入口、日志与开机自检①。
 *
 * thcrap 的两个回调：
 *   thcrap_plugin_init              插件加载时（早于 patch 应用）。自检①：
 *                                   原生表还在预期的地方、长预期的样子。
 *   BP_ce_gate                      断点，ScoreFile__load 入口。全部 init stage 已应用，
 *                                   游戏还没碰卡表。自检②（selfcheck.c）。
 *
 * 不链接 thcrap.dll 的导入库：func_get / log_printf 都用 GetProcAddress 取，
 * 拿不到就降级，不崩。
 */
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include "card_expand.h"

static void (*s_log_printf)(const char *, ...);
uintptr_t (*ce_func_get)(const char *);
static char s_logpath[MAX_PATH];

/* ★★★ 2026-10-24（owner：「所有的 log 行，统一收敛到开发态开关是否 dump」）——
 *   下面这一层（打开/追加/关闭 + `ce_log_impl`）**只在 `CE_DEV_DUMP >= 1` 时进二进制**。
 *   0 档（发布默认）时：`ce_log`/`ce_log_key` 是 `sdk/public.h` 里的 `((void)0)`，
 *   连本文件的 `open_log`/`vlog` 都不编 ⇒ **一个调用、一条格式化串、一次文件 I/O 都没有**。*/
#if CE_DEV_DUMP >= 1

static FILE *open_log(const char *mode)
{
    FILE *f = fopen(s_logpath, mode);
    if (!f) {
        char tmp[MAX_PATH];
        if (GetTempPathA(sizeof tmp, tmp)) {
            strncat(tmp, "th18_card_expand.log", sizeof tmp - strlen(tmp) - 1);
            f = fopen(tmp, mode);
            if (f) strncpy(s_logpath, tmp, sizeof s_logpath - 1);
        }
    }
    return f;
}

static void vlog(const char *fmt, va_list ap)
{
    FILE *f = open_log("a");
    if (!f) return;
    vfprintf(f, fmt, ap);
    fputc('\n', f);
    fclose(f);
}

/* 旧 `ce_log` 的本体（名字换了：`ce_log` 现在是 sdk/public.h 里的宏）。不参与闸门 ——
 * 它只被 `ce_log`（2 档）/`ce_log_key`（≥1 档）两个宏引用。*/
void ce_log_impl(const char *fmt, ...)
{
    va_list ap; va_start(ap, fmt); vlog(fmt, ap); va_end(ap);
}

#endif  /* CE_DEV_DUMP >= 1 */

void ce_verdict(const char *fmt, ...)
{
    char buf[512];
    va_list ap; va_start(ap, fmt); vsnprintf(buf, sizeof buf, fmt, ap); va_end(ap);
#if CE_DEV_DUMP >= 1
    ce_log_impl("%s", buf);        /* 硬失败（guard/FAIL）：1 档起进我们自己的日志文件 */
#endif
    /* ★ thcrap 那条镜像**永远在**（0 档也是）—— 它是"本 DLL 被卸掉/没装上"的唯一现场，
     *   且不写我们自己的日志文件，所以不违反「0 = 全关」。*/
    if (s_log_printf) s_log_printf("[th18_card_expand] %s\n", buf);
}

/* 自检①：原生表签名。三个字段就够把「这不是 th18 v1.00a」拦下来：
 *   行 0  id == 0
 *   行 56 id == 56 且 internal_name 指向 "NULL"
 * 不 md5 整个 exe——那要读文件，而这三个字段就是我们要搬的东西本身。*/
static int retail_table_looks_right(const uint8_t *base)
{
    const uint8_t *t = base + CE_TABLE_RVA;
    uint32_t id0  = *(const uint32_t *)(t + 4);
    uint32_t id56 = *(const uint32_t *)(t + CE_NULL_ROW * CE_ROW_SIZE + 4);
    const char *name56 = *(const char *const *)(t + CE_NULL_ROW * CE_ROW_SIZE);
    if (id0 != 0 || id56 != CE_NULL_ROW) {
        ce_verdict("guard: retail table signature mismatch (row0.id=%u row56.id=%u)", id0, id56);
        return 0;
    }
    if (IsBadReadPtr(name56, 5) || memcmp(name56, "NULL", 5) != 0) {
        ce_verdict("guard: row56.internal_name is not \"NULL\"");
        return 0;
    }
    return 1;
}

/* 日志路径必须是**绝对**的、挂在游戏 exe 所在目录：
 * thcrap 注入时先 SetCurrentDirectory(thcrap/bin) 再 LoadLibrary、跑完整个 init
 * （含 plugin_init 与 post_init）才恢复 CWD（inject.cpp:355-390）。
 * 相对路径会把日志写进 thcrap/bin/ —— 第一版就是这么丢的。*/
static void init_log_path(void)
{
    DWORD n = GetModuleFileNameA(NULL, s_logpath, sizeof s_logpath);
    char *slash = (n && n < sizeof s_logpath) ? strrchr(s_logpath, '\\') : NULL;
    if (slash && (size_t)(slash + 1 - s_logpath) + sizeof "th18_card_expand.log" <= sizeof s_logpath)
        strcpy(slash + 1, "th18_card_expand.log");
    else
        strcpy(s_logpath, "th18_card_expand.log");     /* 兜底：只会在 exe 路径异常时发生 */
}

void ce_log_dir(char *out, size_t cap)
{
    const char *slash = strrchr(s_logpath, '\\');
    size_t n = slash ? (size_t)(slash + 1 - s_logpath) : 0;
    if (n >= cap) n = 0;
    memcpy(out, s_logpath, n);
    out[n] = 0;
}

int __stdcall thcrap_plugin_init(void)
{
    init_log_path();
#if CE_DEV_DUMP >= 1
    FILE *f = open_log("w");                       /* 每次启动新开一份（0 档不建文件：全关）*/
    if (f) {
        time_t t = time(NULL);
        char ts[32]; strftime(ts, sizeof ts, "%Y-%m-%d %H:%M:%S", localtime(&t));
        fprintf(f, "=== th18_card_expand %s ===\n", ts);
        fclose(f);
    }
#endif
    HMODULE th = GetModuleHandleA("thcrap.dll");
    if (th) {
        s_log_printf = (void (*)(const char *, ...))GetProcAddress(th, "log_printf");
        ce_func_get  = (uintptr_t (*)(const char *))GetProcAddress(th, "func_get");
    }
    ce_log_key("thcrap exports: log_printf=%s func_get=%s",
               s_log_printf ? "ok" : "MISSING", ce_func_get ? "ok" : "MISSING");
#if CE_DEV_DUMP >= 1
    if (s_log_printf) s_log_printf("[th18_card_expand] loaded; log -> %s\n", s_logpath);
#else
    if (s_log_printf) s_log_printf("[th18_card_expand] loaded; 日志 dump = OFF (CE_DEV_DUMP=0)\n");
#endif

    if (!retail_table_looks_right((const uint8_t *)GetModuleHandleA(NULL))) {
        ce_verdict("guard: not th18 v1.00a as expected — unloading myself");
        return 1;                                   /* 非 0 = 让 thcrap 卸掉本 DLL */
    }
    ce_log_key("guard: retail table signature ok");
    return 0;
}

/* ★ 自检门走断点，不走 *_mod_post_init。
 * thcrap 的 plugin_init（plugin.cpp:301-308）把插件的 _mod_ 钩子用
 * std::unordered_map::merge 并进全局表 —— 已存在的 key 不会被合并。
 * 而 init.cpp:327 先把 thcrap.dll 自己的导出（steam_mod_post_init、motd_mod_post_init）
 * 注册进去了，post_init 这个 key 已被占，我们的被静默丢弃。第一版就是这么没声的。
 * 2024-11-06 stable 与当前 master 都如此，升级无用。
 *
 * 断点 ce_gate 挂在 ScoreFile__load 0x4637d0 入口：只被调一次，且是最早碰卡表的函数。
 * 它声明在本 patch（最后一个 init stage）里，能触发即证明所有 stage 都已应用。*/
#include "thcrap_bp.h"
int __cdecl BP_ce_gate(x86_reg_t *regs, void *bp_info)
{
    (void)regs; (void)bp_info;
    static int done;
    if (!done) {
        done = 1;
        ce_selfcheck((uint8_t *)GetModuleHandleA(NULL));
    }
    return BP_EXEC_ORIGINAL;
}

/* 音效表扩容的门：0x476410 = SoundManager::init 入口。
 * 语音 blob 必须在引擎建 buffer 之前就位 —— 这里正好在循环 1 / 循环 2 之前。
 * 与 ce_gate 分开是因为两者时点不同：ce_gate 在 ScoreFile__load，音效初始化比它早。 */
#include "sound.h"
int __cdecl BP_ce_snd_gate(x86_reg_t *regs, void *bp_info)
{
    (void)regs; (void)bp_info;
    static int done;
    if (!done) { done = 1; ce_sound_init(); }
    return BP_EXEC_ORIGINAL;
}
