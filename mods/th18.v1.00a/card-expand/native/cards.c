/* cards.c —— 战线 E 第 10 段：从 thcrap 栈里的 th18/cards.js 装新卡。
 *
 * 门里（BP_ce_gate，填表之后、菜单 setup 之前）调一次 ce_cards_load：
 *   1. stack_game_json_resolve("cards.js")：thcrap 把栈里每个 patch 的 th18/cards.js 深合并成一个对象。
 *      NULL = 栈里没有这个文件 = 0 张新卡，不是错。
 *   2. 顶层每个键 = id 的十进制（58..254），值 = 对象；字段见 ../DATA.md §2。
 *      全量解析 / 校验 / 查重 → 暂存表编码与容量检查 → 一次性提交 cave、文案和注册表。
 *   3. ce_shop_capacity_check：随机池 ≤ 560 份、保证卡 ≤ 57（AUDIT §N2、边界 #34）。
 *      （幻影 id 的商店排除——NULL/BACK 行 +0x14 := 6——在 selfcheck.c 的 fill_table 里，不依赖本文件成功。）
 * 任何一条错 → FAIL verdict、返回 0，selfcheck 还原分配器上界——全有或全无。
 *
 * thcrap / jansson 的函数全部 GetProcAddress（不链接导入库，同 func_get）。json_t 只用到 ->type：
 * jansson 2.x 的 `struct json_t { json_type type; volatile size_t refcount; }` 与枚举顺序
 * OBJECT ARRAY STRING INTEGER REAL TRUE FALSE NULL 自 2.0 起没变过；根对象的 type 必须是 OBJECT，
 * 否则当作布局假设失效 FAIL（而不是把别的字段读成类型）。
 *
 * internal_name（表行 +0x00）指向 DLL 里复制出来的串，DLL 与游戏同生命周期，不释放。
 * ★ 串前面多放一个 '\n'：AbilityText__parse_ability_txt（0x4160b0）会拿 ability.txt 里每个 @NAME 与
 *   全表 255 行的 +0x00 逐字节比对，命中就往 zAbilityText + id*0x1c0 写 7 行——新 id 在对象之外（AUDIT §N4）。
 *   token 不可能含换行，所以带 '\n' 前缀的名字永远匹配不上，那条写入路径对新卡永远关着。
 */
#include <stdio.h>
#include <stdlib.h>             /* strtoul */
#include <string.h>
#include "card_expand.h"

/* ---- jansson 最小映射 ---- */
typedef struct json_t { int type; size_t refcount; } json_t;
enum { J_OBJECT = 0, J_ARRAY, J_STRING, J_INTEGER, J_REAL, J_TRUE, J_FALSE, J_NULL };

static json_t     *(*p_stack_game_json_resolve)(const char *, size_t *);
static json_t     *(*p_json_decref_safe)(json_t *);
static void       *(*p_json_object_iter)(json_t *);
static const char *(*p_json_object_iter_key)(void *);
static json_t     *(*p_json_object_iter_value)(void *);
static void       *(*p_json_object_iter_next)(json_t *, void *);
static json_t     *(*p_json_object_get)(const json_t *, const char *);
static long long   (*p_json_integer_value)(const json_t *);
static const char *(*p_json_string_value)(const json_t *);
static size_t      (*p_json_array_size)(const json_t *);
static json_t     *(*p_json_array_get)(const json_t *, size_t);

static int resolve_api(void)
{
    struct { const char *dll; const char *name; void **slot; } tab[] = {
        { "thcrap.dll",  "stack_game_json_resolve", (void **)&p_stack_game_json_resolve },
        { "thcrap.dll",  "json_decref_safe",        (void **)&p_json_decref_safe },
        { "jansson.dll", "json_object_iter",        (void **)&p_json_object_iter },
        { "jansson.dll", "json_object_iter_key",    (void **)&p_json_object_iter_key },
        { "jansson.dll", "json_object_iter_value",  (void **)&p_json_object_iter_value },
        { "jansson.dll", "json_object_iter_next",   (void **)&p_json_object_iter_next },
        { "jansson.dll", "json_object_get",         (void **)&p_json_object_get },
        { "jansson.dll", "json_integer_value",      (void **)&p_json_integer_value },
        { "jansson.dll", "json_string_value",       (void **)&p_json_string_value },
        { "jansson.dll", "json_array_size",         (void **)&p_json_array_size },
        { "jansson.dll", "json_array_get",          (void **)&p_json_array_get },
    };
    for (unsigned i = 0; i < sizeof tab / sizeof tab[0]; ++i) {
        HMODULE m = GetModuleHandleA(tab[i].dll);
        void *f = m ? (void *)GetProcAddress(m, tab[i].name) : NULL;
        if (!f) { ce_verdict("FAIL: cards: %s missing from %s — cannot read cards.js", tab[i].name, tab[i].dll); return 0; }
        *tab[i].slot = f;
    }
    return 1;
}

/* ---- 注册表 ---- */
static uint32_t s_ids[CE_CARD_MAX_NEW];
static uint8_t  s_initial_unlocked[CE_CARD_MAX_NEW];
static char     s_internal_names[CE_CARD_MAX_NEW][sizeof ((ce_card_def_t *)0)->internal_name + 1];
static unsigned s_count;

unsigned ce_new_card_count(void)               { return s_count; }
uint32_t ce_new_card_id(unsigned i)            { return i < s_count ? s_ids[i] : 0; }
int      ce_new_card_initial_unlocked(unsigned i) { return i < s_count && s_initial_unlocked[i]; }

/* ---- 取字段 ---- */
static const char *const KNOWN[] = {
    "name", "desc", "internal_name", "price_tier", "weight", "dmode", "repeatable",
    "deck_visible", "initial_unlocked", "hud_show", "category", "f08", "sprite_large", "sprite_small", NULL
};

/* 整数字段：缺省 → 不动 out；类型不是整数或负 → 0 并填 err */
static int get_uint(const json_t *obj, const char *key, uint32_t *out, char *err, unsigned cap)
{
    const json_t *v = p_json_object_get(obj, key);
    if (!v) return 1;
    if (v->type != J_INTEGER) { snprintf(err, cap, "%s: must be a JSON integer (not a string/float)", key); return 0; }
    long long n = p_json_integer_value(v);
    if (n < 0 || n > 0xffffffffLL) { snprintf(err, cap, "%s: out of range", key); return 0; }
    *out = (uint32_t)n;
    return 1;
}

/* 字串字段：缺省 → 不动 out；类型不对 → 0 */
static int get_str(const json_t *obj, const char *key, char *out, unsigned cap_out, char *err, unsigned cap)
{
    const json_t *v = p_json_object_get(obj, key);
    if (!v) return 1;
    if (v->type != J_STRING) { snprintf(err, cap, "%s: must be a JSON string", key); return 0; }
    const char *s = p_json_string_value(v);
    if (!s || strlen(s) >= cap_out) { snprintf(err, cap, "%s: too long (max %u bytes)", key, cap_out - 1); return 0; }
    strcpy(out, s);
    return 1;
}

static int parse_key(const char *key, uint32_t *id)
{
    if (!key[0] || strlen(key) > 3) return 0;
    for (const char *p = key; *p; ++p) if (*p < '0' || *p > '9') return 0;
    if (key[0] == '0' && key[1]) return 0;                 /* 无前导零 */
    *id = (uint32_t)strtoul(key, NULL, 10);
    return 1;
}

static int parse_card(const char *key, const json_t *obj, ce_card_def_t *d, char *err, unsigned cap)
{
    uint32_t id;
    if (!parse_key(key, &id)) { snprintf(err, cap, "key \"%s\": must be the card id in decimal (58..254)", key); return 0; }
    ce_card_def_defaults(d, id);
    if (obj->type != J_OBJECT) { snprintf(err, cap, "value must be an object"); return 0; }

    for (void *it = p_json_object_iter((json_t *)obj); it; it = p_json_object_iter_next((json_t *)obj, it)) {
        const char *k = p_json_object_iter_key(it);
        unsigned known = 0;
        for (unsigned i = 0; KNOWN[i]; ++i) if (strcmp(k, KNOWN[i]) == 0) { known = 1; break; }
        if (!known) ce_log("cards: %s: unknown field \"%s\" ignored", key, k);
    }
    if (!get_str(obj, "name", d->name, sizeof d->name, err, cap)) return 0;
    if (!get_str(obj, "internal_name", d->internal_name, sizeof d->internal_name, err, cap)) return 0;
    const json_t *desc = p_json_object_get(obj, "desc");
    if (desc) {
        if (desc->type != J_ARRAY) { snprintf(err, cap, "desc: must be an array of strings"); return 0; }
        size_t n = p_json_array_size(desc);
        if (n > CE_CARD_DESC_LINES) { snprintf(err, cap, "desc: at most %u lines", (unsigned)CE_CARD_DESC_LINES); return 0; }
        for (size_t i = 0; i < n; ++i) {
            const json_t *line = p_json_array_get(desc, i);
            const char *s = line && line->type == J_STRING ? p_json_string_value(line) : NULL;
            if (!s) { snprintf(err, cap, "desc[%u]: must be a string", (unsigned)i); return 0; }
            if (strlen(s) >= CE_CARD_TEXT_LINE) { snprintf(err, cap, "desc[%u]: too long (max 63 bytes)", (unsigned)i); return 0; }
            strcpy(d->desc[i], s);
        }
        d->ndesc = (unsigned)n;
    }
    struct { const char *k; uint32_t *p; } ints[] = {
        { "price_tier", &d->price_tier }, { "weight", &d->weight }, { "dmode", &d->dmode },
        { "repeatable", &d->repeatable }, { "deck_visible", &d->deck_visible },
        { "initial_unlocked", &d->initial_unlocked }, { "hud_show", &d->hud_show },
        { "category", &d->category }, { "f08", &d->f08 },
        { "sprite_large", &d->sprite_large }, { "sprite_small", &d->sprite_small },
    };
    for (unsigned i = 0; i < sizeof ints / sizeof ints[0]; ++i)
        if (!get_uint(obj, ints[i].k, ints[i].p, err, cap)) return 0;
    return ce_card_validate(d, err, cap);
}

/* ---- 开发配置：th18/cards_dev.js（只放 _test）----
 *   { "trace": true, "retail_weight": 6, "new_weight": 15 }
 *   trace：桩被调时记日志（含 `BP_ce_test_deck` 的诊断行）。
 *   retail_weight / new_weight：改商店池权重（见 AUDIT §O27）。
 * ★ 起手卡组**不在这里** —— 它是 `\mods\start_deck.js`（显式、只改一次），见 `start_deck.c`。*/
static int s_dev_trace;

int ce_dev_trace(void)                { return s_dev_trace; }

/* 门禁失败时调（selfcheck.c）：新卡 id 马上要被禁掉，`start_deck.c` 那边也不许再动卡组
 * —— 否则会把分配不出来的 id 写进存档（AUDIT §AG / §DL）。*/
void ce_dev_deck_disable(void)        { ce_start_deck_disable(); }

static void load_dev_config(uint8_t *cave, unsigned rows)
{
    s_dev_trace = 0;
    int retail_w = -1, new_w = -1;
    json_t *root = p_stack_game_json_resolve("cards_dev.js", NULL);
    if (root) {
        if (root->type == J_OBJECT) {
            const json_t *tr = p_json_object_get(root, "trace");
            s_dev_trace = tr && tr->type == J_TRUE;
            const json_t *rw = p_json_object_get(root, "retail_weight");
            if (rw && rw->type == J_INTEGER) retail_w = (int)p_json_integer_value(rw);
            const json_t *nw = p_json_object_get(root, "new_weight");
            if (nw && nw->type == J_INTEGER) new_w = (int)p_json_integer_value(nw);
            if (p_json_object_get(root, "start_deck"))
                ce_log("cards_dev: 老键 start_deck 已废弃 —— 起手卡组改用 \\mods\\start_deck.js（显式、只改一次）");
        } else
            ce_log("cards_dev: cards_dev.js root is not an object — ignored");
        p_json_decref_safe(root);
    }
    unsigned changed = 0;
    int used_w = ce_weight_override_fit(cave, rows, retail_w, new_w, s_ids, s_count, &changed);
    ce_log("cards_dev: trace=%d, retail_weight=%d new_weight=%d (%u rows changed)",
           s_dev_trace, retail_w, used_w, changed);
    if (used_w != new_w)
        ce_log("cards_dev: new_weight %d would overflow the shop pool (> %u slots) — clamped to %d",
               new_w, (unsigned)CE_SHOP_POOL_SLOTS, used_w);
}

_Static_assert(CE_SHOP_POOL_SLOTS == CE_POOL_SLOTS,
               "cards_def.h 的池上限与 sites.py 生成的 codecave 大小不一致");

/* ---- 入口 ---- */
int ce_cards_load(uint8_t *base, uint8_t *cave, unsigned rows)
{
    (void)base;
    char err[160];
    s_count = 0;
    if (!resolve_api()) return 0;

    json_t *root = p_stack_game_json_resolve("cards.js", NULL);
    if (!root) {
        ce_log("cards: no th18/cards.js in the patch stack — 0 new cards");
        return 1;
    }
    if (root->type != J_OBJECT) {
        ce_verdict("FAIL: cards: cards.js root is not an object (type %d) — or jansson layout differs", root->type);
        p_json_decref_safe(root);
        return 0;
    }

    ce_card_batch_t batch;
    ce_card_batch_init(&batch);
    int ok = 1;
    for (void *it = p_json_object_iter(root); it && ok; it = p_json_object_iter_next(root, it)) {
        const char *key = p_json_object_iter_key(it);
        json_t *val = p_json_object_iter_value(it);
        ce_card_def_t d;
        if (!parse_card(key, val, &d, err, sizeof err)) {
            ce_verdict("FAIL: cards: card \"%s\": %s", key, err); ok = 0; break;
        }
        if (d.id >= rows) { ce_verdict("FAIL: cards: card %u: id >= table rows (%u)", d.id, rows); ok = 0; break; }
        if (!ce_card_batch_add(&batch, &d, err, sizeof err)) {
            ce_verdict("FAIL: cards: card \"%s\": %s", key, err); ok = 0; break;
        }
    }
    p_json_decref_safe(root);
    if (!ok) { s_count = 0; return 0; }

    static uint8_t staged[CE_MAX_ROWS * CE_ROW_BYTES];
    memcpy(staged, cave, rows * CE_ROW_BYTES);
    s_count = batch.count;
    for (unsigned i = 0; i < batch.count; ++i) {
        const ce_card_def_t *d = &batch.defs[i];
        s_internal_names[i][0] = '\n';
        strcpy(s_internal_names[i] + 1, d->internal_name);
        ce_card_encode_row(d, (uint32_t)(uintptr_t)s_internal_names[i], staged + d->id * CE_ROW_BYTES);
        s_ids[i] = d->id;
        s_initial_unlocked[i] = (uint8_t)d->initial_unlocked;
    }

    /* ★ 发版卡池（owner 2026-10-24）：原生卡 id 0..56 退场 —— 一次性从
     *   「初始可携带」/「贪欲之壶发牌池」/「商店可购买」三处摘掉，例外 = 商店固定上架的资源卡。
     * 只改 staged 拷贝：容量检查通过才 memcpy 回 cave（与其它所有步骤同一个「全有或全无」）。
     * ★ 放在 cards.js 成功解析之后：栈里没有本 mod 的卡池却把原生卡全摘了 = 空商店。
     * 语义 / 为什么 weight 是 6 不是 0 见 cards_def.h 的 ce_native_retire 注释。*/
    unsigned retired = ce_native_retire(staged, rows);
    ce_log("retire: native 0..%u — %u row(s) retired (weight:=%u = never in pool, deck_visible:=0, "
           "initial_unlocked:=0, dmode 1..5 -> 0); kept 0 BLANK / 1,2,5,6 fixed shop stock / 56 NULL",
           (unsigned)CE_RETAIL_LAST_ID, retired, (unsigned)CE_SHOP_NEVER_WEIGHT);

    load_dev_config(staged, rows);                 /* 权重覆盖要在容量检查之前 */
    unsigned pool = 0, guaranteed = 0;
    if (!ce_shop_capacity_check(staged, rows, &pool, &guaranteed, err, sizeof err)) {
        ce_verdict("FAIL: cards: %s", err); s_count = 0; return 0;
    }
    memcpy(cave, staged, rows * CE_ROW_BYTES);
    for (unsigned i = 0; i < batch.count; ++i) {
        const ce_card_def_t *d = &batch.defs[i];
        ce_text_set(d->id, d->name, (const char (*)[CE_CARD_TEXT_LINE])d->desc, d->ndesc);
        ce_log("cards: %u \"%s\" (%s) tier %u weight %u dmode %u sprites %u/%u%s",
               d->id, d->name, d->internal_name, d->price_tier, d->weight, d->dmode, d->sprite_large, d->sprite_small,
               d->initial_unlocked ? " initial_unlocked" : "");
    }
    ce_log("cards: %u registered from cards.js; shop pool %u/%u slots, guaranteed offers <= %u/%u",
           s_count, pool, (unsigned)CE_SHOP_POOL_SLOTS, guaranteed, (unsigned)CE_SHOP_OFFER_SLOTS);
    return 1;
}
