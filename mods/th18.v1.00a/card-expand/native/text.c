/* text.c —— 战线 E 第一块：id ≥ 57 的卡牌文案重定向到 DLL 自己的缓冲。
 *
 * `zAbilityText`（ABILITY_TXT_PTR）是 0x63e0 字节的对象：57 张 × 0x1c0（每张 7 行 × 0x40：
 * 行 0 = 名字，行 1..6 = 说明），紧接着 +0x63c0.. 是 7 个 vm id 字段。第 57 张的位置就是
 * 那些字段，第 58 张已在对象之外——解锁一张新卡，名字栏读到的就是堆里的东西。
 *
 * 不扩对象（尾部字段的访问点还没数全，AUDIT G6），而是**重定向**：三处 `imul r, id, 0x1c0`
 * 后面都紧跟一条 `add r, 基址`，断点里把 r 改成
 *     id < 57 ? id*0x1c0 : (ext + (id-57)*0x1c0) - ABILITY_TXT_PTR
 * 于是 `基址 + r` 落进本文件的 s_ext（同样 7 行 × 0x40 的布局）。对象一个字节不动。
 *
 *   BP_ce_text_name    0x416694  imul ecx, ebx, 0x1c0        → ecx     （卡列表里的名字）
 *   BP_ce_text_desc    0x416779  imul eax, [ebp+0xc], 0x1c0  → eax     （说明 6 行；基址是 txt+0x40）
 *   BP_ce_text_notify  0x41926a  imul eax, ebx, 0x1c0        → eax     （「获得卡牌」通知）
 *
 * flags：imul 设 CF/OF，三处紧接着的 add 都会覆盖，无人读。返回 0 跳过原 imul。
 * 名字会被当成 printf 格式串（FUN_004873f0 → FUN_00404e40），所以文案里**不能有 '%'**。
 * 编码：UTF-8。thcrap 的 textdisp 把 TextOutA 一类换成先按 UTF-8 解、失败再退 Shift-JIS
 * （win32_utf8 MultiByteToWideCharU），base_tsa 是本 patch 的依赖，所以 UTF-8 一定走得通。
 *
 * 未注册 id 的内容是占位（「测试卡牌 N」）；已注册的新卡由 cards.c 经 ce_text_set 覆盖成 cards.js 里的文案。
 */
#include <stdio.h>
#include <string.h>
#include "card_expand.h"
#include "thcrap_bp.h"

#define EXT_ENTRIES (CE_MAX_ROWS - CE_TEXT_ENTRIES)      /* 57..254 → 198 张 */

static uint8_t s_ext[EXT_ENTRIES][CE_TEXT_ENTRY];
static int     s_filled;

static void fill_placeholders(void)
{
    if (s_filled) return;
    s_filled = 1;
    memset(s_ext, 0, sizeof s_ext);
    for (unsigned id = CE_TEXT_ENTRIES; id < CE_MAX_ROWS; ++id) {
        char *e = (char *)s_ext[id - CE_TEXT_ENTRIES];
        if (id == CE_TEXT_ENTRIES)
            snprintf(e, 0x40, "BACK");                          /* id 57 = 卡背，原生也没有文案 */
        else
            snprintf(e, 0x40, "测试卡牌 %u", id);            /* 已注册的卡以后由数据文件覆盖 */
        snprintf(e + 0x40 * 1, 0x40, "card-expand 占位文案");
        snprintf(e + 0x40 * 2, 0x40, "id %u 还没有真正的卡牌数据", id);
        snprintf(e + 0x40 * 3, 0x40, "战线 E 会把它换掉");
    }
}

/* 装载器（cards.c）用：覆盖一张已注册新卡的文案。行 0 = 名字，行 1..ndesc = 说明。
 * 只清这一条目，其他 id 仍是占位。id 越界 / 长度超 0x3f 由 cards_def 的校验挡在前面，这里只兜底截断。*/
void ce_text_set(uint32_t id, const char *name, const char (*desc)[CE_CARD_TEXT_LINE], unsigned ndesc)
{
    if (id < CE_TEXT_ENTRIES || id >= CE_MAX_ROWS) { ce_log("text: ce_text_set(id=%u) out of range — ignored", id); return; }
    fill_placeholders();
    char *e = (char *)s_ext[id - CE_TEXT_ENTRIES];
    memset(e, 0, CE_TEXT_ENTRY);
    snprintf(e, CE_CARD_TEXT_LINE, "%s", name);
    if (ndesc > CE_CARD_DESC_LINES) ndesc = CE_CARD_DESC_LINES;
    for (unsigned i = 0; i < ndesc; ++i)
        snprintf(e + CE_CARD_TEXT_LINE * (i + 1), CE_CARD_TEXT_LINE, "%s", desc[i]);
}

/* 断点核心：算出让 `基址 + r` 落到正确条目的 r。*/
static uint32_t text_offset(uint32_t id)
{
    if (id < CE_TEXT_ENTRIES) return id * CE_TEXT_ENTRY;
    fill_placeholders();
    if (id >= CE_MAX_ROWS) {
        ce_verdict("text: id=%u out of range — falling back to entry 0", id);
        return 0;
    }
    uint8_t *base = (uint8_t *)GetModuleHandleA(NULL);
    uint32_t txt = *(uint32_t *)(base + CE_ABILITY_TXT_PTR_RVA);
    return (uint32_t)(uintptr_t)s_ext[id - CE_TEXT_ENTRIES] - txt;
}

int __cdecl BP_ce_text_name(x86_reg_t *regs, void *bp_info)
{
    (void)bp_info;
    regs->ecx = text_offset(regs->ebx);
    return 0;
}

int __cdecl BP_ce_text_desc(x86_reg_t *regs, void *bp_info)
{
    (void)bp_info;
    const uint32_t *frame = (const uint32_t *)(uintptr_t)regs->ebp;   /* [ebp+0xc] = param_2 = id */
    regs->eax = text_offset(frame[3]);
    return 0;
}

int __cdecl BP_ce_text_notify(x86_reg_t *regs, void *bp_info)
{
    (void)bp_info;
    regs->eax = text_offset(regs->ebx);
    return 0;
}

/* 自检：三个断点都挂上了（call + nop 补位）。*/
int ce_text_check(uint8_t *base)
{
    struct { const char *name; uint32_t rva; unsigned len; } bps[] = {
        { "ce_text_name",   CE_BP_TEXT_NAME_RVA,   6 },
        { "ce_text_desc",   CE_BP_TEXT_DESC_RVA,   7 },
        { "ce_text_notify", CE_BP_TEXT_NOTIFY_RVA, 6 },
    };
    unsigned bad = 0;
    for (unsigned i = 0; i < 3; ++i) {
        const uint8_t *p = base + bps[i].rva;
        int ok = p[0] == 0xe8;
        for (unsigned k = 5; ok && k < bps[i].len; ++k) ok = p[k] == 0x90;
        if (!ok) { ++bad; ce_log("text: breakpoint %s @ 0x%08x NOT applied", bps[i].name, 0x400000u + bps[i].rva); }
    }
    if (bad) { ce_verdict("FAIL: text redirect — %u/3 breakpoints not applied (new-card names would read past zAbilityText)", bad); return 0; }
    fill_placeholders();
    ce_log("text: ids %u..%u redirected to ext buffer @ %p (%u entries x 0x%x), 3 breakpoints verified",
           (unsigned)CE_TEXT_ENTRIES, (unsigned)CE_MAX_ROWS - 1, (void *)s_ext, (unsigned)EXT_ENTRIES, (unsigned)CE_TEXT_ENTRY);
    return 1;
}
