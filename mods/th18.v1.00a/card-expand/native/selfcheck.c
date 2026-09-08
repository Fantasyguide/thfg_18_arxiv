/* selfcheck.c —— 开机自检②：全有或全无的门。
 *
 * 起因是一种「日志一切正常」的静默失败：patch 里的 *_patch_init codecave
 * 是否被自动调用取决于 thcrap 版本；没被调用时新表全零、100 处 binhack
 * 指向一张空表，而 thcrap 日志里每条 binhack 都是 OK。
 *
 * 一个 DLL 配所有 patch：行数与「是否搬了分配器」都在运行时从 patch 已写入的
 * 字节里反推，不烤进编译期。
 *
 * 由断点 BP_ce_gate（ScoreFile__load 入口，全部 init stage 已应用）调一次，做：
 *   0. 读配置：rows 由第一处 END 站点已写入的尾界反推；alloc 看跳转表 codecave 在不在。
 *   1. 填表：零售 58 行 memcpy 进 codecave，多出的行填 NULL 副本。幂等。
 *   2. （alloc）填跳转表：57 项原样拷，其余指向 case 56；核对两处分配器 binhack。
 *      再依次：扩容核对 → 影子数组 → 文案断点 → cards.js 装载 → 菜单 setup（各自的文件）。
 *   3. 回读 100 处搬表站点：改后 4 字节 == cave + 基偏移 + 字段，前缀 opcode 不变。
 *   4. 一行结论。
 */
#include <string.h>
#include "card_expand.h"
#include "selfcheck_core.h"

/* rows 反推：END 站点写的是 cave + rows*0x34 + field。 */
static unsigned derive_rows(const uint8_t *base, const uint8_t *cave)
{
    for (unsigned i = 0; i < CE_NSITES; ++i) {
        const ce_site_t *s = &CE_SITES[i];
        if (s->kind != CE_K_END) continue;
        uint32_t v; memcpy(&v, base + s->rva + s->prefix_len, 4);
        uint32_t d = v - (uint32_t)(uintptr_t)cave - s->field;
        if (v < (uint32_t)(uintptr_t)cave || d % CE_ROW_SIZE) return 0;   /* 还是零售值,或不整除 */
        unsigned rows = d / CE_ROW_SIZE;
        return (rows >= CE_ROW_COUNT && rows <= CE_MAX_ROWS) ? rows : 0;
    }
    return 0;
}

static uint32_t site_want(const ce_site_t *s, const uint8_t *cave, unsigned rows)
{
    uint32_t b = (uint32_t)(uintptr_t)cave;
    switch (s->kind) {
    case CE_K_FALLBACK: b += CE_NULL_ROW * CE_ROW_SIZE; break;
    case CE_K_END:      b += rows * CE_ROW_SIZE;        break;
    default: break;                                     /* start / hit：表基 */
    }
    return b + s->field;
}

static int fill_table(uint8_t *base, uint8_t *cave, unsigned rows)
{
    memcpy(cave, base + CE_TABLE_RVA, CE_ROW_COUNT * CE_ROW_SIZE);
    for (unsigned r = CE_ROW_COUNT; r < rows; ++r)
        memcpy(cave + r * CE_ROW_SIZE, cave + CE_NULL_ROW * CE_ROW_SIZE, CE_ROW_SIZE);
    uint32_t id0  = *(uint32_t *)(cave + 4);
    uint32_t id56 = *(uint32_t *)(cave + CE_NULL_ROW * CE_ROW_SIZE + 4);
    if (id0 != 0 || id56 != CE_NULL_ROW) {
        ce_verdict("FAIL: table sanity after copy — row0.id=%u row56.id=%u", id0, id56);
        return 0;
    }
    /* 商店：_255 把三处循环上界抬到 rows，幻影 id 查表回落到 NULL 行 56、BACK 57 按自己 id 命中。
     * 两行 +0x14（权重）:= 6 让三条筛选（≠0&&≠6 / ==0 / dmode 1-5）都过不了。放在这里而不是装载器里，
     * 是为了后面任何一步 FAIL 时商店也不会被 ~199 个幻影灌爆 57 槽的 offer 数组。+0x14 只有商店读：AUDIT §N1/N5。*/
    uint32_t six = CE_SHOP_NEVER_WEIGHT;
    memcpy(cave + CE_NULL_ROW * CE_ROW_SIZE + 0x14, &six, 4);
    memcpy(cave + (CE_NULL_ROW + 1) * CE_ROW_SIZE + 0x14, &six, 4);
    ce_log("table: %u rows filled at %p (58 retail + %u NULL copies); NULL/BACK shop weight := 6",
           rows, cave, rows - CE_ROW_COUNT);
    return 1;
}

/* 兜底：任何 FAIL 之后把分配器上界写回零售值 0x38。
 * 场景：两个不同行数的 patch 同时进栈——搬表 binhack 只有先到的那个生效（后到的
 * expected 不匹配被跳过），但分配器上界 binhack 两边都能打上（原字节没被前者动过），
 * 于是上界撑到 254 而跳转表只有 58 项 → 未注册 id 会 jmp 到 cave 之外。
 * 上界回到 56 后，只有 id 0..56 会被分派，任何行数的跳转表都放得下。*/
/* 商店随机池搬迁（AUDIT §AH）：9 处站点逐条按 sites_gen.h 的模板重算应有字节再比对。
 * 零售那 560 格是 `pick_weighted_random_offer` 里 `ebp-0x8c4` 的栈缓冲，末尾紧贴金丝雀、
 * 填池的两处写没有越界检查 —— 所以这些站点只要有一处没打上，超过 560 份就会踩返回地址。
 * 全有或全无：任何一处不符就 FAIL，别让「一半搬走一半还在栈上」跑起来。*/
static int check_pool(uint8_t *base)
{
    uint32_t cave = ce_func_get ? (uint32_t)(uintptr_t)ce_func_get(CE_POOL_CAVE_NAME) : 0;
    if (!cave) { ce_verdict("FAIL: %s not found — pool relocation incomplete", CE_POOL_CAVE_NAME); return 0; }
    unsigned bad = 0;
    for (unsigned i = 0; i < CE_NPOOL; ++i) {
        const ce_pool_t *q = &CE_POOL[i];
        uint8_t want[8];
        memcpy(want, q->want, sizeof want);
        uint32_t disp = q->uses_cave ? cave + q->off : q->off;
        memcpy(want + q->disp_at, &disp, 4);
        if (memcmp(base + q->rva, want, q->len) != 0) {
            ++bad;
            ce_log("pool: site 0x%08x NOT patched", 0x400000u + q->rva);
        }
    }
    if (bad) { ce_verdict("FAIL: shop pool relocation — %u/%u sites not patched", bad, (unsigned)CE_NPOOL); return 0; }
    ce_log("pool: %u sites verified, %d slots at %08x (retail stack buffer was 560)",
           (unsigned)CE_NPOOL, (int)CE_POOL_SLOTS, cave);
    return 1;
}

static int restore_alloc_bound(void *ctx)
{
    uint8_t *base = ctx;
    uint8_t *cmp = base + CE_ALLOC_CMP_RVA;
    if (cmp[0] == 0x83 && cmp[1] == 0xfb && cmp[2] != 0x38) {
        DWORD old;
        if (VirtualProtect(cmp, 3, PAGE_EXECUTE_READWRITE, &old)) {
            cmp[2] = 0x38;
            VirtualProtect(cmp, 3, old, &old);
            ce_verdict("mitigation: allocator bound restored to retail (56) — new ids disabled");
            return CE_ROLLBACK_RESTORED;
        }
        ce_verdict("mitigation: FAILED to restore allocator bound — DO NOT PLAY");
        return CE_ROLLBACK_FAILED;
    }
    return CE_ROLLBACK_NOT_NEEDED;
}

static int fill_jumptable(uint8_t *base, uint32_t *jt, unsigned rows)
{
    memcpy(jt, base + CE_JT_RVA, CE_JT_COUNT * 4);
    uint32_t case56 = (uint32_t)(uintptr_t)(base + CE_CASE56_RVA);
    for (unsigned i = CE_JT_COUNT; i < rows; ++i) jt[i] = case56;
    if (jt[CE_NULL_ROW] != case56 || jt[0] < (uint32_t)(uintptr_t)base) {
        ce_verdict("FAIL: jumptable sanity — [56]=%08x expected %08x", jt[CE_NULL_ROW], case56);
        return 0;
    }
    const uint8_t *cmp = base + CE_ALLOC_CMP_RVA;       /* 83 fb <rows-1> */
    const uint8_t *jmp = base + CE_ALLOC_JMP_RVA;       /* ff 24 9d <jt>  */
    uint32_t jt_addr = (uint32_t)(uintptr_t)jt;
    int ok_cmp = cmp[0] == 0x83 && cmp[1] == 0xfb && cmp[2] == (uint8_t)(rows - 1);
    int ok_jmp = jmp[0] == 0xff && jmp[1] == 0x24 && jmp[2] == 0x9d && memcmp(jmp + 3, &jt_addr, 4) == 0;
    if (!ok_cmp || !ok_jmp) {
        ce_verdict("FAIL: allocator binhacks — bound %s (%02x %02x %02x), jumptable %s",
                   ok_cmp ? "ok" : "NOT patched", cmp[0], cmp[1], cmp[2],
                   ok_jmp ? "ok" : "NOT patched");
        return 0;
    }
    ce_log("jumptable: %u entries at %p (57 retail + %u -> case56 @ %08x); allocator bound = %u",
           rows, jt, rows - CE_JT_COUNT, case56, rows - 1);
    return 1;
}

/* 战线 C：zAbilityManager 扩容的 12 处，改后字节按运行时 rows 现算再比对 */
static int check_grow(uint8_t *base, unsigned rows)
{
    uint32_t new_size = CE_MGR_SIZE + rows * 4;
    uint32_t shop_end = CE_OWNED_NEW + rows * 4;             /* 商店三处循环上界 = rows（AUDIT §N）*/
    unsigned bad = 0;
    for (unsigned i = 0; i < CE_NGROW; ++i) {
        const ce_grow_t *g = &CE_GROW[i];
        const uint8_t *p = base + g->rva;
        uint32_t v, want;
        int ok;
        switch (g->kind) {
        case CE_G_SIZE:       memcpy(&v, p + 1, 4); want = new_size;     ok = p[0] == 0x68 && v == want; break;
        case CE_G_OWNED_LEA:  memcpy(&v, p + 2, 4); want = CE_OWNED_NEW; ok = p[0] == 0x8d && v == want; break;
        case CE_G_STOSD:      memcpy(&v, p + 1, 4); want = rows;         ok = p[0] == 0xb9 && v == want; break;
        case CE_G_OWNED_DISP: memcpy(&v, p + 3, 4); want = CE_OWNED_NEW; ok = p[0] == 0xc7 && v == want; break;
        case CE_G_SHOP_START: memcpy(&v, p + 1, 4); want = CE_OWNED_NEW; ok = (p[0] == 0xb9 || p[0] == 0xbb) && v == want; break;
        default:              memcpy(&v, p + 2, 4); want = shop_end;     ok = p[0] == 0x81 && v == want; break;
        }
        if (!ok) { ++bad; ce_log("grow: site 0x%08x NOT patched (kind %u, read %08x want %08x)", 0x400000u + g->rva, g->kind, v, want); }
    }
    if (bad) { ce_verdict("FAIL: zAbilityManager growth — %u/%u sites not patched", bad, (unsigned)CE_NGROW); return 0; }
    ce_log("grow: zAbilityManager 0x%x -> 0x%x, owned[] at +0x%x (%u entries), shop loops %u ids",
           (unsigned)CE_MGR_SIZE, new_size, (unsigned)CE_OWNED_NEW, rows, rows);
    return 1;
}

int ce_selfcheck(uint8_t *base)
{
    ce_failure_guard_t guard = CE_FAILURE_GUARD_INIT;
    if (!ce_func_get) {
        ce_verdict("FAIL: func_get unavailable — cannot locate codecave; table NOT filled");
        goto fail;
    }
    uint8_t *cave = (uint8_t *)ce_func_get(CE_CAVE_NAME);
    if (!cave) {
        ce_verdict("FAIL: %s not found — patch not in the stack?", CE_CAVE_NAME);
        goto fail;
    }
    ce_log("gate: BP_ce_gate fired at ScoreFile__load");
    /* 0. 从 patch 反推配置 */
    unsigned rows = derive_rows(base, cave);
    if (!rows) {
        ce_verdict("FAIL: cannot derive row count — END site not patched or out of range");
        goto fail;
    }
    uint32_t *jt = (uint32_t *)ce_func_get(CE_JT_CAVE_NAME);
    int alloc = jt != NULL;
    ce_log("config from patch: rows=%u alloc=%d", rows, alloc);

    if (!fill_table(base, cave, rows)) goto fail;
    if (alloc && !fill_jumptable(base, jt, rows)) goto fail;
    if (alloc && !check_grow(base, rows)) goto fail;
    /* 战线 D 与 B/C 同进退：_255 patch 里一定带影子数组。核对 9 处读 + 3 个断点。 */
    if (alloc) { ce_unlock_init(base); if (!ce_unlock_check(base)) goto fail; }
    if (alloc && !ce_text_check(base)) goto fail;
    /* 战线 E 第 10 段：cards.js → 表行 + 文案 + 注册表；顺序表 / 图鉴条目数（menu）消费注册表，所以在它前面 */
    if (alloc && !check_pool(base)) goto fail;   /* 池上限依赖它，必须早于容量检查 */
    if (alloc && !ce_cards_load(base, cave, rows)) goto fail;
    if (alloc && !ce_menu_setup(base)) goto fail;
    /* 行为 SDK：基类虚表守卫、两断点、与 cards.js 对账 */
    if (alloc && !ce_sdk_setup(base, ce_dev_trace())) goto fail;
    /* 商店走两遍：两个断点已挂（AUDIT §P）*/
    if (alloc && !ce_shop_setup(base)) goto fail;

    unsigned ok = 0, bad = 0, first_bad = 0;
    for (unsigned i = 0; i < CE_NSITES; ++i) {
        const ce_site_t *s = &CE_SITES[i];
        const uint8_t *p = base + s->rva;
        uint32_t want = site_want(s, cave, rows);
        int match = s->prefix_len + 4 == s->len
                 && memcmp(p, s->prefix, s->prefix_len) == 0
                 && memcmp(p + s->prefix_len, &want, 4) == 0;
        if (match) ++ok; else { if (!bad) first_bad = i; ++bad; }
    }
    if (bad == 0) {
        ce_verdict("OK: table filled (%u rows @ %p)%s, %u/%u sites verified",
                   rows, cave, alloc ? ", allocator relocated, manager grown, unlocked shadowed, text redirected, cards loaded, menu extended" : "", ok, (unsigned)CE_NSITES);
        ce_failure_guard_success(&guard);
        return 1;
    }
    const ce_site_t *s = &CE_SITES[first_bad];
    const uint8_t *p = base + s->rva;
    ce_verdict("FAIL: %u/%u sites verified, %u NOT patched. first bad @ 0x%08x: "
               "%02x %02x %02x %02x %02x %02x %02x — partial application, DO NOT PLAY",
               ok, (unsigned)CE_NSITES, bad, 0x400000u + s->rva,
               p[0], p[1], p[2], p[3], p[4], p[5], p[6]);
fail:
    ce_dev_deck_disable();                       /* 新卡 id 马上要被禁掉，别再往空槽里塞它们（AUDIT §AG）*/
    ce_failure_guard_fail(&guard, restore_alloc_bound, base);
    return 0;
}
