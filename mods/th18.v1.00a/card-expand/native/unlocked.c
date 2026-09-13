/* unlocked.c —— 战线 D：unlocked_cards 的影子数组 + side-car 持久化。
 *
 * 原生 `zScoreFile.unlocked_cards` 是 uint8_t[57]，后面没有余量，而它在存档里——
 * 动它就动存档格式。所以 9 处读全部改成读 codecave `th18_card_unlocked`（256 字节，
 * 下标 = card id），由本文件维护它与两份真相的关系：
 *
 *   id < 57   真相在 scoreth18.dat（原生数组）。影子在存档加载后从它拷；
 *             mark_obtained 的写**放行原指令**，原生数组照常更新，存档逐字节与香草一致。
 *   id >= 57  真相在 side-car `th18_card_expand.sav`（存档目录下）。mark_obtained 的写
 *             被断点截住（不让它写进原生数组之后的未知区），改写影子 + 立刻落盘。
 *
 * 三个断点（都跑在游戏线程；不碰 x87；不调 thcrap API）：
 *   BP_ce_save_loaded   ScoreFile__load 尾段 0x46398a，SCOREFILE_PTR 刚写好、ebx = 存档。
 *                       影子[0..56] ← 原生，[57..254] ← side-car。
 *   BP_ce_unlock_write  mark_obtained 0x418e04 `mov byte [esi+edi+0x5f588], 1`。
 *                       edi = id，esi = 存档。影子[id]=1；id<57 返回 1 放行原指令，否则写 side-car 返回 0。
 *   BP_ce_unlock_all    ScoreFile__unlock_all 0x4648fe，下一条是 memset(原生,1,0x38)。镜像到影子[0..55]。
 *
 * side-car 只承载 id >= 57 的位；文件里 0..56 那段只是写入时的快照，读回来**不用**。
 */
#include <stdio.h>
#include <string.h>
#include "card_expand.h"
#include "thcrap_bp.h"

#define SIDECAR_NAME    "th18_card_expand.sav"
#define SIDECAR_MAGIC   "TH18CEXP"
#define SIDECAR_VERSION 1
#define SHADOW_SIZE     256

typedef struct {
    char     magic[8];
    uint32_t version;
    uint32_t count;                 /* = 255 */
    uint8_t  unlocked[CE_MAX_ROWS];
} sidecar_t;                        /* 16 + 255 = 271 字节，无对齐填充（成员都是 ≤4 字节且顺序自然对齐） */

static uint8_t *s_shadow;           /* codecave；NULL = 战线 D 的 patch 不在栈里 */
static char     s_path[MAX_PATH];   /* side-car 绝对路径 */

/* side-car 放哪：游戏自己 chdir 用的存档目录缓冲（%APPDATA%\ShanghaiAlice\th18\，尾带反斜杠）。
 * 缓冲为空（APPDATA 没设）时游戏把存档写在 exe 目录，我们也跟着写在日志旁边。*/
static void sidecar_path_init(uint8_t *base)
{
    const char *dir = (const char *)(base + CE_SAVEDIR_RVA);
    size_t n = IsBadReadPtr(dir, 1) ? 0 : strnlen(dir, MAX_PATH);
    int plausible = n > 3 && n + sizeof SIDECAR_NAME < sizeof s_path
                 && (dir[1] == ':' || dir[0] == '\\') && dir[n - 1] == '\\';
    if (plausible) {
        memcpy(s_path, dir, n);
        strcpy(s_path + n, SIDECAR_NAME);
        return;
    }
    ce_log_dir(s_path, sizeof s_path);          /* exe 目录 + '\'，与日志同处 */
    strncat(s_path, SIDECAR_NAME, sizeof s_path - strlen(s_path) - 1);
    ce_log("unlocked: save dir buffer not usable (len %u) — side-car falls back to exe dir", (unsigned)n);
}

/* 读 side-car；返回 >=57 段里置 1 的个数，文件不在 / 不认识返回 0 且不动影子。*/
static unsigned sidecar_load(void)
{
    sidecar_t sc;
    FILE *f = fopen(s_path, "rb");
    if (!f) return 0;
    size_t got = fread(&sc, 1, sizeof sc, f);
    fclose(f);
    if (got != sizeof sc || memcmp(sc.magic, SIDECAR_MAGIC, 8) != 0
        || sc.version != SIDECAR_VERSION || sc.count != CE_MAX_ROWS) {
        ce_verdict("unlocked: side-car %s unreadable (got %u bytes) — ignored, will be overwritten on next unlock",
                   s_path, (unsigned)got);
        return 0;
    }
    unsigned set = 0;
    for (unsigned i = CE_RETAIL_UNLOCKED; i < CE_MAX_ROWS; ++i)
        if (sc.unlocked[i]) { s_shadow[i] = 1; ++set; }
    return set;
}

/* 写 side-car：先写 .tmp 再原子替换，半截文件不会留下来。*/
static int sidecar_save(void)
{
    sidecar_t sc;
    memcpy(sc.magic, SIDECAR_MAGIC, 8);
    sc.version = SIDECAR_VERSION;
    sc.count   = CE_MAX_ROWS;
    memcpy(sc.unlocked, s_shadow, CE_MAX_ROWS);
    char tmp[MAX_PATH + 8];
    snprintf(tmp, sizeof tmp, "%s.tmp", s_path);
    FILE *f = fopen(tmp, "wb");
    if (!f) { ce_verdict("unlocked: cannot open %s for writing", tmp); return 0; }
    size_t put = fwrite(&sc, 1, sizeof sc, f);
    fclose(f);
    if (put != sizeof sc || !MoveFileExA(tmp, s_path, MOVEFILE_REPLACE_EXISTING)) {
        ce_verdict("unlocked: write to %s failed (err %lu)", s_path, GetLastError());
        DeleteFileA(tmp);
        return 0;
    }
    return 1;
}

/* 由 BP_ce_gate 调：找影子 codecave、算 side-car 路径。返回影子地址（NULL = D 不在栈里）。*/
uint8_t *ce_unlock_init(uint8_t *base)
{
    if (!s_shadow && ce_func_get)
        s_shadow = (uint8_t *)ce_func_get(CE_UNLOCKED_CAVE_NAME);
    if (s_shadow && !s_path[0])
        sidecar_path_init(base);
    return s_shadow;
}

/* 自检：9 处读的改后字节 == pre + 影子地址 + post；3 个断点已挂上（call + nop 补位）。*/
static int bp_applied(const uint8_t *p, unsigned len)
{
    if (p[0] != 0xe8) return 0;
    for (unsigned i = 5; i < len; ++i) if (p[i] != 0x90) return 0;
    return 1;
}

int ce_unlock_check(uint8_t *base)
{
    if (!s_shadow) { ce_verdict("FAIL: %s not found — patch predates front D?", CE_UNLOCKED_CAVE_NAME); return 0; }
    uint32_t sh = (uint32_t)(uintptr_t)s_shadow;
    unsigned bad = 0;
    for (unsigned i = 0; i < CE_NUNLOCK; ++i) {
        const ce_unlock_t *u = &CE_UNLOCK[i];
        const uint8_t *p = base + u->rva;
        int ok = memcmp(p, u->pre, u->pre_len) == 0
              && memcmp(p + u->pre_len, &sh, 4) == 0
              && memcmp(p + u->pre_len + 4, u->post, u->post_len) == 0;
        if (!ok) { ++bad; ce_log("unlocked: read site 0x%08x NOT patched (%02x %02x %02x %02x %02x %02x %02x %02x)",
                                  0x400000u + u->rva, p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7]); }
    }
    struct { const char *name; uint32_t rva; unsigned len; } bps[] = {
        { "ce_unlock_write", CE_BP_UNLOCK_WRITE_RVA, 8 },
        { "ce_save_loaded",  CE_BP_SAVE_LOADED_RVA,  6 },
        { "ce_unlock_all",   CE_BP_UNLOCK_ALL_RVA,   6 },
    };
    for (unsigned i = 0; i < 3; ++i)
        if (!bp_applied(base + bps[i].rva, bps[i].len)) {
            ++bad; ce_log("unlocked: breakpoint %s @ 0x%08x NOT applied", bps[i].name, 0x400000u + bps[i].rva);
        }
    if (bad) { ce_verdict("FAIL: unlocked_cards shadow — %u of %u sites/breakpoints not in place", bad, (unsigned)CE_NUNLOCK + 3); return 0; }
    ce_log("unlocked: shadow @ %p, %u read sites + 3 breakpoints verified; side-car = %s",
           s_shadow, (unsigned)CE_NUNLOCK, s_path);
    return 1;
}

/* ---- 断点 ---- */

int __cdecl BP_ce_save_loaded(x86_reg_t *regs, void *bp_info)
{
    (void)bp_info;
    uint8_t *base = (uint8_t *)GetModuleHandleA(NULL);
    if (!ce_unlock_init(base)) return BP_EXEC_ORIGINAL;      /* D 不在栈里：什么都不做 */
    const uint8_t *scorefile = (const uint8_t *)(uintptr_t)regs->ebx;
    const uint8_t *global    = *(const uint8_t *const *)(base + CE_SCOREFILE_PTR_RVA);
    if (scorefile != global) {
        ce_verdict("unlocked: ebx (%p) != SCOREFILE_PTR (%p) at save_loaded — using the global", scorefile, global);
        scorefile = global;
    }
    memset(s_shadow, 0, SHADOW_SIZE);
    memcpy(s_shadow, scorefile + CE_UNLOCKED_OFF, CE_RETAIL_UNLOCKED);
    unsigned retail = 0;
    for (unsigned i = 0; i < CE_RETAIL_UNLOCKED; ++i) retail += s_shadow[i] != 0;
    unsigned extra = sidecar_load();
    /* initial_unlocked 的新卡：原生的 +0x24 由新档创建循环拷进 unlocked_cards[0..57]，新 id 不在循环里；
     * 这里每次读档直接置位。解锁单调、新档也该解禁，语义与原生等价，side-car 格式不动（DATA.md §3）。*/
    unsigned init = 0;
    for (unsigned i = 0; i < ce_new_card_count(); ++i)
        if (ce_new_card_initial_unlocked(i)) {
            uint32_t id = ce_new_card_id(i);
            if (id < SHADOW_SIZE && !s_shadow[id]) { s_shadow[id] = 1; ++init; }
        }
    ce_log("unlocked: shadow @ %p, 57 retail (%u set) + side-car (%u new ids set) + %u initial_unlocked from %s",
           s_shadow, retail, extra, init, s_path);
    return BP_EXEC_ORIGINAL;
}

int __cdecl BP_ce_unlock_write(x86_reg_t *regs, void *bp_info)
{
    (void)bp_info;
    uint32_t id = regs->edi;
    if (!s_shadow) return BP_EXEC_ORIGINAL;                  /* 不该发生：读已改而影子没有；放行 = 香草行为 */
    if (id >= CE_MAX_ROWS) {
        ce_verdict("unlocked: mark_obtained(id=%u) out of range — ignored", id);
        return 0;                                           /* 也别让原指令去写未知区 */
    }
    s_shadow[id] = 1;
    if (id < CE_RETAIL_UNLOCKED) {
        ce_log("unlock: id=%u (retail; original write allowed)", id);
        return BP_EXEC_ORIGINAL;                            /* 原指令写原生数组，存档照常 */
    }
    int ok = sidecar_save();
    ce_log("unlock: id=%u (NEW; shadow + side-car %s)", id, ok ? "saved" : "SAVE FAILED");
    return 0;                                               /* 跳过原指令：它会写到 unlocked_cards[57] 之后 */
}

int __cdecl BP_ce_unlock_all(x86_reg_t *regs, void *bp_info)
{
    (void)regs; (void)bp_info;
    if (s_shadow) {
        memset(s_shadow, 1, 0x38);                          /* 与紧接着的 memset(原生,1,0x38) 同范围 */
        ce_log("unlock: unlock_all mirrored to shadow[0..55]");
    }
    return BP_EXEC_ORIGINAL;
}
