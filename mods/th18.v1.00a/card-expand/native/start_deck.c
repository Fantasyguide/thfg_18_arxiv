/* start_deck.c —— **显式起手卡组**：读 `<modkit>\mods\start_deck.js`，按它改初始卡组，**只改一次**。
 *
 * 用户口径（2026-09-16t/u）原话：「相当于它会自动尝试读取 \mods 里面的一个 js，然后根据那个里面的
 * 去改初始卡组 —— 只改一次」。所以要的是这三件事：
 *   1. 文件放在 **modkit 的 mods\ 目录**（与 `th18_card_expand.dll` 同处，不是 thcrap 的 patch 栈里）；
 *   2. 按里面的内容去改**存档里的初始卡组**（写 scorefile，不是每帧假读）；
 *   3. **只改一次** ⇒ 之后你在「卡组编成」里怎么改就是什么，DLL 不再插手。
 *
 * 「只改一次」怎么落地：文件内容算 FNV-1a 32 位 hash，写在同目录的 `start_deck.applied` 里。
 *   · hash 与 marker 里的一致 ⇒ **什么都不做**（编成菜单说了算）；
 *   · hash 变了（你改了 js / 首次部署）⇒ 改一遍，然后把新 hash 写回 marker。
 *   ⇒ 想让它再生效：改 js（任一字节）或删掉 `start_deck.applied`。
 *
 * 文件格式（故意做成最小 JSON，手写解析，不依赖 jansson）：
 *   {
 *     "deck": [113, 110, 111],     // 按槽位顺序的卡 id；上限 3 = CE_DECK_CAP_MAX（初始卡组上限）
 *     "cap": 12,                   // ★ opt-in：写存档的容量字段（默认不写）—— 见下
 *     "mode": "exact" | "seed",    // exact（默认）= 三个槽就是这张表；seed = 只补空槽
 *     "shot": "all" | 0..3         // 默认 all（四份机体存档的初手卡组都改）
 *   }
 *
 * 写点（一手，与 `reset_cards` 的读点同一结构）：
 *   scorefile + 0x5f608 + shot*0x10 + i  = 该机体初手第 i 格的 card_id（**字节**）
 *   scorefile + 0x5f678 + shot*4         = 该机体的初手格数上限（**只读**：2026-09-08 写它会开局卡死，
 *                                          见 `bp_trace.c` 的注释；表比它长时只写前 cap 个并报一行）
 *
 * 入口是 `BP_ce_test_deck`（`0x407ee3` 的桩，`esi == 0` 时 = 每次 reset_cards 的第一格）：
 * 那一刻存档已加载、`regs->eax` 就是「本机体的初手第 0 格」地址 ⇒ 由它反推 scorefile，四份机体一起改。
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "card_expand.h"
#include "engine.h"        /* CE_TEST_DECK_SAVE_OFF / CE_DECK_CAP_OFF */

#define SD_MAX_IDS 16
#define SD_FILE         "start_deck.js"
#define SD_MARKER       "start_deck.applied"
#define SD_MARKER_MAGIC "# th18_card_expand start_deck"

enum { SD_UNSEEN = 0, SD_ABSENT, SD_LOADED };
enum { SD_MODE_EXACT = 0, SD_MODE_SEED = 1, SD_MODE_OFF = 2 };

static int      s_state = SD_UNSEEN;
static uint32_t s_hash;
static uint32_t s_ids[SD_MAX_IDS];
static unsigned s_n;
static int      s_mode = SD_MODE_EXACT;
static unsigned s_shot_mask = 0xf;      /* bit0..3 = shot 0..3 */
static int      s_cap;                  /* ★ opt-in：js 里写了 "cap": N 才动存档的容量字段；0 = 不碰 */
static char     s_js_path[MAX_PATH + 32];
static char     s_marker_path[MAX_PATH + 32];

static void sd_paths(void)
{
    char dir[MAX_PATH];
    ce_log_dir(dir, sizeof dir);                        /* exe 目录 + 尾 '\' */
    snprintf(s_js_path, sizeof s_js_path, "%s..\\mods\\" SD_FILE, dir);
    snprintf(s_marker_path, sizeof s_marker_path, "%s..\\mods\\" SD_MARKER, dir);
}

static uint32_t sd_hash(const char *p, size_t n)
{
    uint32_t h = 2166136261u;
    for (size_t i = 0; i < n; ++i) { h ^= (uint8_t)p[i]; h *= 16777619u; }
    return h;
}

/* 极简扫描：找 `"key"`，返回它后面第一个非空白字符的指针（找不到返回 NULL）。*/
static const char *sd_key(const char *s, const char *key)
{
    char pat[32];
    snprintf(pat, sizeof pat, "\"%s\"", key);
    const char *p = strstr(s, pat);
    if (!p) return NULL;
    p += strlen(pat);
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') ++p;
    return (*p == ':') ? p + 1 : NULL;
}

static const char *sd_skip_ws(const char *p)
{
    while (p && (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')) ++p;
    return p;
}

static void sd_parse(const char *text)
{
    const char *p;

    if ((p = sd_key(text, "deck"))) {                   /* [ 1, 2, 3 ] */
        p = sd_skip_ws(p);
        if (*p == '[') {
            ++p;
            while (*p && *p != ']' && s_n < SD_MAX_IDS) {
                p = sd_skip_ws(p);
                if (*p == ']') break;
                if (*p < '0' || *p > '9') { ++p; continue; }
                long v = strtol(p, (char **)&p, 10);
                if (v >= 0 && v < CE_MAX_ROWS) s_ids[s_n++] = (uint32_t)v;
                else ce_log("start_deck: id %ld 越界 — 忽略", v);
            }
        } else
            ce_log("start_deck: deck 不是数组 — 忽略");
    }
    if ((p = sd_key(text, "mode"))) {                   /* "exact" | "seed" */
        p = sd_skip_ws(p);
        if (*p == '"') {
            if (!strncmp(p + 1, "seed", 4)) s_mode = SD_MODE_SEED;
            else if (!strncmp(p + 1, "exact", 5)) s_mode = SD_MODE_EXACT;
            else if (!strncmp(p + 1, "off", 3) || !strncmp(p + 1, "none", 4)) s_mode = SD_MODE_OFF;
            else ce_log("start_deck: mode 只认 exact / seed / off — 用 exact");
        }
    }
    /* ★ opt-in 容量（2026-09-17）：写了 cap 才**往存档里写**容量字段 —— 引擎自己的递增链
     * 只到 3（`th18:0x4449d4` 无 3→4 分支），要一次装 >3 张做总验收只能这样绕。
     * 不写这个键 = 老行为（只读、不改存档的进度数据）。*/
    if ((p = sd_key(text, "cap"))) {
        p = sd_skip_ws(p);
        if (*p >= '0' && *p <= '9') {
            long v = strtol(p, (char **)&p, 10);
            if (v > 0 && v <= CE_DECK_CAP_HARD_MAX) s_cap = (int)v;
            else ce_log("start_deck: cap %ld 越界（1..%d）— 忽略（不改存档容量）", v, CE_DECK_CAP_HARD_MAX);
        }
    }
    if ((p = sd_key(text, "shot"))) {                   /* "all" | 0..3 */
        p = sd_skip_ws(p);
        if (*p == '"') s_shot_mask = 0xf;
        else if (*p >= '0' && *p <= '9') s_shot_mask = 1u << (unsigned)(*p - '0');
    }
}

/* 读 js（一次），失败/不在就置 ABSENT。*/
static void sd_load(void)
{
    s_state = SD_ABSENT;
    sd_paths();
    FILE *f = fopen(s_js_path, "rb");
    if (!f) return;                                     /* 没这个文件 = 不干预（默认）*/
    char text[2048];
    size_t got = fread(text, 1, sizeof text - 1, f);
    fclose(f);
    text[got] = 0;
    if (!got) { ce_log("start_deck: %s 是空文件 — 忽略", s_js_path); return; }
    s_hash = sd_hash(text, got);
    sd_parse(text);
    if (s_mode == SD_MODE_OFF) {
        ce_log("start_deck: %s 里 mode=off ⇒ 不干预卡组（编成菜单说了算）", SD_FILE);
        return;                                        /* s_state 仍是 ABSENT ⇒ tick 里直接返回 */
    }
    s_state = SD_LOADED;
    ce_log("start_deck: %s → %u 张 %s，shot mask 0x%x，hash %08x", SD_FILE, s_n,
           s_mode == SD_MODE_SEED ? "(seed 只补空槽)" : "(exact = 卡组就是这张表)", s_shot_mask, s_hash);
}

/* marker 里的 hash（没有/读不出来返回 0）。*/
static uint32_t sd_marker_hash(void)
{
    FILE *f = fopen(s_marker_path, "rb");
    if (!f) return 0;
    char line[128];
    uint32_t h = 0;
    while (fgets(line, sizeof line, f)) {
        unsigned v;
        if (sscanf(line, "hash=%x", &v) == 1) { h = (uint32_t)v; break; }
    }
    fclose(f);
    return h;
}

static void sd_marker_save(void)
{
    char tmp[MAX_PATH + 40];
    snprintf(tmp, sizeof tmp, "%s.tmp", s_marker_path);
    FILE *f = fopen(tmp, "wb");
    if (!f) { ce_log("start_deck: 写 %s 失败 — 下次会再改一遍", tmp); return; }
    fprintf(f, "%s marker — 删掉本文件 ⇒ %s 下次再生效一遍\n", SD_MARKER_MAGIC, SD_FILE);
    fprintf(f, "hash=%08x\n", s_hash);
    fprintf(f, "deck=");
    for (unsigned i = 0; i < s_n; ++i) fprintf(f, i ? ",%u" : "%u", s_ids[i]);
    fprintf(f, "\n");
    fclose(f);
    if (!MoveFileExA(tmp, s_marker_path, MOVEFILE_REPLACE_EXISTING)) {
        ce_log("start_deck: marker 落盘失败（err %lu）", GetLastError());
        DeleteFileA(tmp);
    }
}

/* 门禁失败时由 `ce_dev_deck_disable()` 调：新卡 id 分配不出来，别再往存档里写它们。*/
static int s_disabled;
void ce_start_deck_disable(void)
{
    if (!s_disabled) { s_disabled = 1; ce_log("start_deck: disabled（门禁失败；新 id 分配不出来）"); }
}

/* 每次 reset_cards 的第一格调一次。`deck0` = 本机体初手第 0 格的地址（由桩的 regs 反推）。*/
void ce_start_deck_tick(uint8_t *deck0, int shot)
{
    if (s_disabled) return;
    if (s_state == SD_UNSEEN) sd_load();
    if (s_state != SD_LOADED) return;

    /* 只要**一次**：hash 与 marker 一致 ⇒ 这局什么都不动（编成里改的说了算）。
     * 首次部署 / 你改了 js ⇒ hash 变 ⇒ 改一遍并把 hash 记下。*/
    if (sd_marker_hash() == s_hash) {
        static int told;
        if (!told) { told = 1; ce_log("start_deck: js 未变（hash %08x 已在 marker 里）⇒ 不动卡组，编成菜单说了算", s_hash); }
        return;
    }

    /* 一手（0x407ed4–0x407ee3）：eax = `[ebp-?]+0xc5c`（shot）× 0x10 + `[0x4cf41c]`（scorefile）
     * ⇒ **eax = scorefile + shot*0x10**，被钩的那条指令读的是 `[eax + esi + 0x5f608]`。*/
    uint8_t *sf = deck0 - (size_t)shot * 0x10;
    unsigned shots = 0, cells = 0;
    for (int s = 0; s < 4; ++s) {
        if (!(s_shot_mask & (1u << s))) continue;
        int32_t cap = *(int32_t *)(sf + CE_DECK_CAP_OFF + s * 4);
        if (s_cap > 0) {                       /* ★ opt-in：把容量字段本身写大（默认不做）*/
            cap = s_cap;
            *(int32_t *)(sf + CE_DECK_CAP_OFF + s * 4) = cap;
            ce_log("start_deck: shot %d 容量字段被**写成 %d**（opt-in cap；引擎递进链本来只到 3）",
                   s, (int)cap);
        } else {
            if (cap <= 0) continue;
            if (cap > CE_DECK_CAP_MAX) cap = CE_DECK_CAP_MAX;
        }
        if (cap > CE_DECK_CAP_HARD_MAX) cap = CE_DECK_CAP_HARD_MAX;
        uint8_t *deck = sf + s * 0x10 + CE_TEST_DECK_SAVE_OFF;
        if ((unsigned)cap < s_n)
            ce_log("start_deck: shot %d 初手只有 %d 格（表要 %u 张）⇒ 只写前 %d 张；"
                   "容量是游戏自己的递增链，本卡不写它", s, (int)cap, s_n, (int)cap);
        for (int i = 0; i < cap; ++i) {
            uint32_t want;
            if ((unsigned)i < s_n) want = s_ids[i];
            else want = CE_NULL_ROW;                    /* exact：表短 ⇒ 余下的槽清空 */
            if (s_mode == SD_MODE_SEED && deck[i] != CE_NULL_ROW) continue;
            if (deck[i] != (uint8_t)want) ++cells;
            deck[i] = (uint8_t)want;
        }
        ++shots;
    }
    sd_marker_save();
    ce_log("start_deck: **只改这一次** —— %u 份机体存档、共 %u 格已按 %s 写入（hash %08x）；"
           "之后编成菜单里怎么改就是什么", shots, cells, SD_FILE, s_hash);
}
