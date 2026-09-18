/* menu.c —— 战线 E 第二块：让新卡出现在图鉴与卡组编成里。
 *
 * 两个菜单（同一个 zAbilityMenu 对象，模式不同）都按**显示顺序表**（原生 0x4b3600，57 个 id）走：
 *   图鉴   for i in 0..COUNT-1: __card_ids[i] = order[i]      COUNT 是写死的 0x38（7 处立即数）
 *   编成   for 每个 order[] 项 直到尾界: 可见(+0x20) 且已解锁 → 收进 __card_ids[n++]
 *
 * patch 已把顺序表搬进 codecave（255 项，尾界 = 255 项处）、把 __card_ids 从 +0x304
 * 搬到对象尾部 +0x13fc（255 项）、把编成前的清理循环抬到 255。这里在门里做剩下的：
 *   1. 重排顺序表：[原生 0..55 原序，新 id 按类别插进同类别区段末尾（ce_build_order）, 56(NULL=空槽), 其余填 57(BACK)]
 *      57 在编成里不可见（+0x20 = 0），图鉴按条目数走不到它。
 *   2. 把图鉴条目数的 7 处立即数写成 56 + N。
 *      ★ 2026-09-19：其中两处是**循环上界**（`cmp eax,0x38` / `cmp edx,0x38`），原来是
 *        **有符号** imm8 ⇒ 条目数 ≥ 128 时立即数变负、循环零圈、图鉴整表空掉（AUDIT §AG 的链）。
 *        patch 现在把这两处**原地换编码**成「无符号字节比较 + jb」（`sites.py` 的
 *        `MENU_COUNT_BCMP`），上界 127 → 255；立即数位置与宽度不变，所以本文件那 7 处写入点
 *        一个都不用改。下面 `check_count_ops()` 是这两条 binhack 的**回读护栏** ——
 *        binhack 的 `expected` 对不上时 thcrap 只打一行日志就跳过，没有回读就等于没护栏。
 *   3. 核对 patch 的 27 处（7 顺序表 + 20 zAbilityMenu）都打上了。
 *
 * 只在门里跑一次，早于任何菜单对象的创建；写代码段用 VirtualProtect，与 restore_alloc_bound 同一做法。
 */
#include <string.h>
#include "card_expand.h"
#include "engine.h"       /* CE_FN_TABLE_GET：按 id 取表行（类别在 +0x0c）*/

static int write_code(uint8_t *p, const void *src, unsigned n)
{
    DWORD old;
    if (!VirtualProtect(p, n, PAGE_EXECUTE_READWRITE, &old)) return 0;
    memcpy(p, src, n);
    VirtualProtect(p, n, old, &old);
    return 1;
}

static int check_sites(uint8_t *base, uint32_t *order)
{
    unsigned bad = 0;
    uint32_t cave = (uint32_t)(uintptr_t)order, want_end = cave + CE_MAX_ROWS * 4;
    for (unsigned i = 0; i < CE_NORDER; ++i) {
        const ce_order_t *o = &CE_ORDER[i];
        uint32_t v; memcpy(&v, base + o->rva + o->pre_len, 4);
        if (v != (o->is_end ? want_end : cave)) {
            ++bad; ce_log("menu: order-table site 0x%08x NOT patched (read %08x)", 0x400000u + o->rva, v);
        }
    }
    for (unsigned i = 0; i < CE_NMENU; ++i) {
        const ce_menu_t *m = &CE_MENU[i];
        if (memcmp(base + m->rva, m->want, m->len) != 0) {
            ++bad; ce_log("menu: zAbilityMenu site 0x%08x NOT patched", 0x400000u + m->rva);
        }
    }
    if (bad) ce_verdict("FAIL: ability menu — %u/%u sites not patched", bad, (unsigned)(CE_NORDER + CE_NMENU));
    return bad == 0;
}

/* 顺序表重排。原生表 [56] 必须是 56（NULL），否则这不是我们认识的表。
 * 新卡不再一股脑排在原生之后，而是按类别（表行 +0x0c）插进原生同类别**最长连续段**的末尾（ce_build_order，主机单测）：
 * 资源跟着 MOKOU、装备卡跟着 MAGATAMA、被动跟着 MUKADE（而不是掉队在最后的 MAGATAMA2）、主动跟着 RICEBALL。
 * 类别从已填好的表里读（门里此时表已装载）。类别字段在引擎里唯一的消费者是 0x416940（≠ 4 才显示说明面板），不碰玩法。*/
typedef uint8_t *(__attribute__((fastcall)) *menu_table_get_t)(uint32_t id);
static uint32_t card_category(uint32_t id)
{
    const uint8_t *row = ((menu_table_get_t)CE_FN_TABLE_GET)(id);
    return row ? *(const uint32_t *)(row + 0x0c) : 4u;         /* 4 = 哨兵类别：原生表里没有 → 排最后 */
}

static int rebuild_order(uint8_t *base, uint32_t *order)
{
    const uint32_t *retail = (const uint32_t *)(base + CE_ORDER_RVA);
    if (retail[CE_ORDER_COUNT - 1] != CE_NULL_ROW) {
        ce_verdict("FAIL: retail order table [%u] = %u, expected %u", CE_ORDER_COUNT - 1, retail[CE_ORDER_COUNT - 1], CE_NULL_ROW);
        return 0;
    }
    static uint32_t retail_cat[CE_ORDER_COUNT], new_ids[CE_MAX_ROWS], new_cat[CE_MAX_ROWS];
    unsigned N = ce_new_card_count();
    for (unsigned i = 0; i < CE_ORDER_COUNT - 1; ++i) retail_cat[i] = card_category(retail[i]);
    for (unsigned i = 0; i < N; ++i) {
        uint32_t id = ce_new_card_id(i);
        if (id < CE_RETAIL_UNLOCKED || id >= CE_MAX_ROWS) { ce_verdict("FAIL: registered new card id %u out of range", id); return 0; }
        new_ids[i] = id;
        new_cat[i] = card_category(id);
    }
    unsigned vis = ce_build_order(order, CE_MAX_ROWS, retail, retail_cat, CE_ORDER_COUNT - 1, new_ids, new_cat, N, CE_NULL_ROW, CE_ORDER_COUNT);
    if (vis != CE_ORDER_COUNT - 1 + N) { ce_verdict("FAIL: order table overflow (%u visible)", vis); return 0; }
    for (unsigned i = 0; i < N; ++i) {
        unsigned pos = 0;
        while (pos < vis && order[pos] != new_ids[i]) ++pos;
        ce_log("menu: new card %u (category %u) at order[%u], after id %u", new_ids[i], new_cat[i], pos, pos ? order[pos - 1] : 0);
    }
    return 1;
}

/* 两处循环上界的编码改写必须真打上（`sites.py` 的 `MENU_COUNT_BCMP` / `emit_count_binhacks`）。
 * 为什么非要回读：thcrap 对 `expected` 对不上的 binhack **只打一行日志就跳过**（binhack.cpp:1420），
 * 而这两条一旦缺失，条目数 > 127 时那两处会**静默**按有符号比（imm8 ≥ 0x80 = 负）⇒
 * 图鉴 fill 循环一圈不跑、整表空掉（AUDIT §AG 的症状链：条目数 0 → 菜单 +0xfec 空 → 空指针崩）。*/
static int check_count_ops(uint8_t *base)
{
    unsigned bad = 0;
    for (unsigned i = 0; i < CE_NMENU_COUNT_OP; ++i) {
        const ce_count_op_t *o = &CE_MENU_COUNT_OP[i];
        for (unsigned k = 0; k < 2; ++k) {
            uint8_t got = base[o->rva + o->off[k]];
            if (got != o->want[k]) {
                ++bad;
                ce_log("menu: count opcode 0x%08x+%u reads %02x, expected %02x (cmp r,imm8 → cmp r8,imm8 + jb rewrite NOT applied)",
                       0x400000u + o->rva, o->off[k], got, o->want[k]);
            }
        }
    }
    if (bad) {
        ce_verdict("FAIL: encyclopedia entry-count upper bound still signed at %u/%u bytes — 图鉴条目数 > 127 会空表",
                   bad, 2u * (unsigned)CE_NMENU_COUNT_OP);
        return 0;
    }
    ce_log("menu: count opcode rewrite ok (%u sites → unsigned byte compare, entries <= %u)",
           (unsigned)CE_NMENU_COUNT_OP, (unsigned)CE_MENU_COUNT_MAX);
    return 1;
}

/* 图鉴条目数：7 处立即数从 0x38（或上次写的值）改成 count。 */
static int set_menu_count(uint8_t *base, unsigned count)
{
    if (count > CE_MENU_COUNT_MAX) {
        ce_verdict("FAIL: %u encyclopedia entries > %u (table rows / byte-count limit) — register fewer new cards", count, CE_MENU_COUNT_MAX);
        return 0;
    }
    for (unsigned i = 0; i < CE_NMENU_COUNT; ++i) {
        const ce_count_site_t *s = &CE_MENU_COUNT[i];
        uint8_t *p = base + s->rva + s->imm_off;
        uint32_t cur = 0; memcpy(&cur, p, s->width);
        if (cur != CE_MENU_COUNT_RETAIL) {
            ce_verdict("FAIL: menu count site 0x%08x reads %u, expected %u", 0x400000u + s->rva, cur, CE_MENU_COUNT_RETAIL);
            return 0;
        }
        uint32_t v = count;
        if (!write_code(p, &v, s->width)) { ce_verdict("FAIL: cannot write menu count at 0x%08x", 0x400000u + s->rva); return 0; }
    }
    return 1;
}

int ce_menu_setup(uint8_t *base)
{
    uint32_t *order = ce_func_get ? (uint32_t *)ce_func_get(CE_ORDER_CAVE_NAME) : NULL;
    if (!order) { ce_verdict("FAIL: %s not found — patch predates front E?", CE_ORDER_CAVE_NAME); return 0; }
    if (!check_sites(base, order)) return 0;
    if (!rebuild_order(base, order)) return 0;
    unsigned N = ce_new_card_count();
    if (!check_count_ops(base)) return 0;
    if (!set_menu_count(base, CE_MENU_COUNT_RETAIL + N)) return 0;
    ce_log("menu: order table @ %p rebuilt (56 retail + %u new + NULL, rest BACK); encyclopedia entries = %u; __card_ids at +0x%x",
           order, N, CE_MENU_COUNT_RETAIL + N, (unsigned)CE_CARD_IDS_NEW);
    return 1;
}
