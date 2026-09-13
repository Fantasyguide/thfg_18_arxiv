/* sound.c —— 音效表扩容的 DLL 侧：填语音 blob、写新行配置、自检。
 *
 * 时序（docs/superpowers/specs/2026-09-05-voice-expand-design.md §3.4）：
 *   codecaves_apply 末尾  th18_snd_patch_init  拷原生 84 行 cfg + 72 个 wav 名 + 新行骨架
 *   binhacks_apply        51 处站点改写
 *   BP_ce_snd_gate        ← 本文件。0x476410 入口，引擎还没读表
 *   放行后                循环1 初始化 116 槽、循环2 逐行建 buffer
 *
 * ★ blob 的字节直接用 thcrap stack_game_file_resolve 的返回，不拷贝、不释放：
 *   WinMain 的释放循环尾界刻意只到原生 72（binhack snd_4713d8），跨堆 free 会崩。
 *   进程此刻正要退出，这个「泄漏」到此为止。
 *
 * ★ 两条不变式（engine/_shared/th18-sound-table.md §6），破了不是崩而是挂死/跑飞：
 *   I1  116 行的 +0 两两不同且恰好覆盖 0..0x73 —— 循环 1 的扫描没有上界
 *   I2  116 行的 +4 指向的 blob 槽都非 NULL —— 0x4776f0 遇 NULL 进 Sleep(10) 死等。
 *       原生 0..71 由预加载线程异步填（那个等待循环就是为它们写的）；72.. 只有 DLL 会填，
 *       所以自检只对 >= 72 的下标断言。
 */
#include <stdio.h>
#include <string.h>

#include "card_expand.h"
#include "sound.h"

/* jansson 的 json_t：只用到 ->type，布局同 cards.c 的注释。 */
typedef struct json_t { int type; size_t refcount; } json_t;

static json_t     *(*p_stack_game_json_resolve)(const char *, size_t *);
static void       *(*p_stack_game_file_resolve)(const char *, size_t *);
static json_t     *(*p_json_decref_safe)(json_t *);
static void       *(*p_json_object_iter)(json_t *);
static const char *(*p_json_object_iter_key)(void *);
static json_t     *(*p_json_object_iter_value)(void *);
static void       *(*p_json_object_iter_next)(json_t *, void *);
static json_t     *(*p_json_object_get)(const json_t *, const char *);
static long long   (*p_json_integer_value)(const json_t *);
static const char *(*p_json_string_value)(const json_t *);

static int resolve_imports(void)
{
    static const struct { const char *dll, *sym; void **slot; } tab[] = {
        { "thcrap.dll",  "stack_game_json_resolve", (void **)&p_stack_game_json_resolve },
        { "thcrap.dll",  "stack_game_file_resolve", (void **)&p_stack_game_file_resolve },
        { "thcrap.dll",  "json_decref_safe",        (void **)&p_json_decref_safe },
        { "jansson.dll", "json_object_iter",        (void **)&p_json_object_iter },
        { "jansson.dll", "json_object_iter_key",    (void **)&p_json_object_iter_key },
        { "jansson.dll", "json_object_iter_value",  (void **)&p_json_object_iter_value },
        { "jansson.dll", "json_object_iter_next",   (void **)&p_json_object_iter_next },
        { "jansson.dll", "json_object_get",         (void **)&p_json_object_get },
        { "jansson.dll", "json_integer_value",      (void **)&p_json_integer_value },
        { "jansson.dll", "json_string_value",       (void **)&p_json_string_value },
    };
    for (unsigned i = 0; i < sizeof tab / sizeof tab[0]; ++i) {
        HMODULE m = GetModuleHandleA(tab[i].dll);
        *tab[i].slot = m ? (void *)GetProcAddress(m, tab[i].sym) : NULL;
        if (!*tab[i].slot) {
            ce_verdict("snd: FAIL missing %s!%s", tab[i].dll, tab[i].sym);
            return 0;
        }
    }
    return 1;
}

/* wav 名表要指向长期有效的字符串，放 DLL 自己的静态区。 */
static char s_names[CE_SND_NEW_N][64];
static int  s_voice_count;

int ce_sound_voice_count(void) { return s_voice_count; }

static uint8_t *cave(const char *name)
{
    return ce_func_get ? (uint8_t *)ce_func_get(name) : NULL;
}

static uint8_t *cfg_row(uint8_t *cfg, int j) { return cfg + j * CE_SND_CFG_ROW; }
static uint32_t rd32(const uint8_t *p, int off) { return *(const uint32_t *)(p + off); }
static void     wr32(uint8_t *p, int off, uint32_t v) { *(uint32_t *)(p + off) = v; }

/* 把 32 个新行退回 patch_init 写的骨架：+4 = 0（指原生 wav 0，I2 仍成立）、其余清零。 */
static void rollback(uint8_t *cfg, uint8_t **blobs, char **names)
{
    for (int k = 0; k < CE_SND_NEW_N; ++k) {
        uint8_t *row = cfg_row(cfg, CE_SND_CFG_ROWS + k);
        wr32(row, 0x4, 0);
        wr32(row, 0x8, 0);
        wr32(row, 0xc, 0);
        wr32(row, 0x10, 0);
        blobs[CE_SND_NAMES_N + k] = NULL;
        names[CE_SND_NAMES_N + k] = NULL;
    }
    s_voice_count = 0;
}

/* 自检：I1、I2、R8（新槽的 +4 初值）、槽 20 仍是 se_lazer02。 */
static int selfcheck(uint8_t *cfg, uint8_t *slots, uint8_t **blobs)
{
    /* R8：pre-main（0x401100，界 0x401139）已经跑过，新槽的 +4 必须是「空闲」−1。
     * 不是 −1 = 那处字节界没改对，play_sound 一进来就把新 id 当忙的处理。 */
    for (int k = CE_SND_CFG_ROWS; k < CE_SND_ROWS_TOTAL; ++k) {
        uint32_t v = rd32(slots + k * CE_SND_SLOT_SIZE, 4);
        if (v != 0xFFFFFFFFu) {
            ce_verdict("snd: FAIL R8 slot %d .+4 = 0x%08x, expected -1 (0x401139 bound?)", k, v);
            return 0;
        }
    }

    unsigned char seen[CE_SND_ROWS_TOTAL];
    memset(seen, 0, sizeof seen);
    int lazer2 = -1;
    for (int j = 0; j < CE_SND_ROWS_TOTAL; ++j) {
        const uint8_t *row = cfg_row(cfg, j);
        uint32_t slot = rd32(row, 0), wav = rd32(row, 4);
        if (slot >= (uint32_t)CE_SND_ROWS_TOTAL || seen[slot]) {
            ce_verdict("snd: FAIL I1 row %d has slot %u (out of range or duplicate)", j, slot);
            return 0;
        }
        seen[slot] = 1;
        if (wav >= (uint32_t)CE_SND_NAMES_TOTAL) {
            ce_verdict("snd: FAIL row %d wav index %u out of range", j, wav);
            return 0;
        }
        /* I2 只对**我们负责**的槽成立：原生 0..71 由预加载线程异步填，此刻可能还没好
         * —— 0x4776f0 的 Sleep(10) 等待循环正是为它们准备的。而 72.. 只有 DLL 会填，
         * 没填就永远等不到，那才是真的挂死。 */
        if (wav >= (uint32_t)CE_SND_NAMES_N && !blobs[wav]) {
            ce_verdict("snd: FAIL I2 row %d -> blob[%u] is NULL and nobody will fill it "
                       "(0x4776f0 would hang)", j, wav);
            return 0;
        }
        if (slot == CE_SND_LAZER2_SLOT) lazer2 = (int)wav;
    }
    /* 0x45ff38 硬编码引用槽 20 的 buffer（se_lazer02 常驻激光音）。指错了不崩，
     * 只是玩家激光没声音 —— 最容易漏检的一处，所以单独断言。 */
    if (lazer2 != CE_SND_LAZER2_WAV) {
        ce_verdict("snd: FAIL slot %d -> wav %d, expected 0x%02x (se_lazer02)",
                   CE_SND_LAZER2_SLOT, lazer2, CE_SND_LAZER2_WAV);
        return 0;
    }
    return 1;
}

static void load_voices(uint8_t *cfg, uint8_t **blobs, char **names)
{
    size_t sz = 0;
    json_t *root = p_stack_game_json_resolve("voice.js", &sz);
    if (!root) { ce_log("snd: no voice.js in the patch stack (0 voices)"); return; }

    /* ★ 按条目里的 `id` 定位，不按迭代顺序：thcrap 会把栈里每个 patch 的 voice.js
     * 深合并成一个对象，合并后的迭代顺序不由我们决定。id 由 assets/build_voice.py
     * 在构建期与 ORDER.txt 的行号对账后写死在 JSON 里。 */
    unsigned char taken[CE_SND_NEW_N];
    memset(taken, 0, sizeof taken);

    for (void *it = p_json_object_iter(root); it; it = p_json_object_iter_next(root, it)) {
        const char *key = p_json_object_iter_key(it);
        if (!key) key = "?";
        json_t *obj = p_json_object_iter_value(it);
        json_t *jw = obj ? p_json_object_get(obj, "wav") : NULL;
        json_t *ji = obj ? p_json_object_get(obj, "id") : NULL;
        const char *wav = jw ? p_json_string_value(jw) : NULL;
        if (!wav || !ji) { ce_log("snd: skip \"%s\": needs both \"wav\" and \"id\"", key); continue; }

        long long id = p_json_integer_value(ji);
        if (id < CE_SND_FIRST_ID || id >= CE_SND_FIRST_ID + CE_SND_NEW_N) {
            ce_log("snd: skip \"%s\": id %lld out of range 0x%02x..0x%02x",
                   key, id, CE_SND_FIRST_ID, CE_SND_FIRST_ID + CE_SND_NEW_N - 1);
            continue;
        }
        int k = (int)(id - CE_SND_FIRST_ID);
        if (taken[k]) { ce_log("snd: skip \"%s\": id 0x%02llx already taken", key, id); continue; }

        char path[64];
        if (snprintf(path, sizeof path, "voice/%s.wav", wav) >= (int)sizeof path) {
            ce_log("snd: skip \"%s\": name too long", key); continue;
        }
        size_t bytes = 0;
        void *buf = p_stack_game_file_resolve(path, &bytes);
        if (!buf || bytes < 44) { ce_log("snd: skip \"%s\": %s not found", key, path); continue; }
        if (memcmp(buf, "RIFF", 4) != 0 || memcmp((char *)buf + 8, "WAVE", 4) != 0) {
            ce_log("snd: skip \"%s\": %s is not RIFF/WAVE", key, path); continue;
        }

        /* cfg 行 +8：**低 word = 音量（DirectSound 百分之一 dB，≤ 0 的衰减）**、
         * **高 word = 优先级 0..100**。消费者 0x477611 取的是 movsx word [行+8]，
         * 播放音量 = (1 − (1 − 设置/100)³) × (低word + 5000) − 5000；设置拉满时就等于低word。
         * 声像**不在表里** —— 它由 play_sound 的 x 参数在运行时算（0x4775d9 SetPan）。 */
        json_t *jv = p_json_object_get(obj, "volume_db");
        json_t *jr = p_json_object_get(obj, "priority");
        long long vol = jv ? p_json_integer_value(jv) : 0;      /* 0 = 不衰减 */
        long long pri = jr ? p_json_integer_value(jr) : 100;
        if (vol > 0) vol = 0;
        else if (vol < -5000) vol = -5000;                      /* 公式的下限就是 -5000 */
        if (pri < 0) pri = 0;
        else if (pri > 100) pri = 100;

        memcpy(s_names[k], path, strlen(path) + 1);
        blobs[CE_SND_NAMES_N + k] = (uint8_t *)buf;
        names[CE_SND_NAMES_N + k] = s_names[k];

        uint8_t *row = cfg_row(cfg, CE_SND_CFG_ROWS + k);
        wr32(row, 0x4,  (uint32_t)(CE_SND_NAMES_N + k));
        wr32(row, 0x8,  ((uint32_t)(pri & 0xffff) << 16) | (uint32_t)(vol & 0xffff));
        wr32(row, 0x10, 1);
        taken[k] = 1;
        ++s_voice_count;
        ce_log("snd: voice id 0x%02llx \"%s\" -> %s (%u bytes, wav slot %d, %+lld dB/100, pri %lld)",
               id, key, path, (unsigned)bytes, CE_SND_NAMES_N + k, vol, pri);
    }
    p_json_decref_safe(root);
}

int ce_sound_init(void)
{
    uint8_t  *cfg   = cave(CE_SND_CAVE_CFG);
    uint8_t  *slots = cave(CE_SND_CAVE_SLOTS);
    uint8_t **blobs = (uint8_t **)cave(CE_SND_CAVE_BLOBS);
    char    **names = (char **)cave(CE_SND_CAVE_NAMES);
    if (!cfg || !slots || !blobs || !names) {
        ce_verdict("snd: FAIL codecave lookup (cfg=%p slots=%p blobs=%p names=%p)",
                   (void *)cfg, (void *)slots, (void *)blobs, (void *)names);
        return 0;
    }
    ce_log("snd: caves cfg=%p names=%p slots=%p blobs=%p",
           (void *)cfg, (void *)names, (void *)slots, (void *)blobs);

    /* patch_init 生效了没有：原生第 0 行的 +0 应为 0，最后一个新行的 +0 应为 0x73。 */
    if (rd32(cfg_row(cfg, 0), 0) != 0 ||
        rd32(cfg_row(cfg, CE_SND_ROWS_TOTAL - 1), 0) != (uint32_t)(CE_SND_ROWS_TOTAL - 1)) {
        ce_verdict("snd: FAIL patch_init did not fill the cave (row0.+0=%u last.+0=%u)",
                   rd32(cfg_row(cfg, 0), 0), rd32(cfg_row(cfg, CE_SND_ROWS_TOTAL - 1), 0));
        return 0;
    }

    if (!resolve_imports()) { rollback(cfg, blobs, names); return 0; }
    load_voices(cfg, blobs, names);

    if (!selfcheck(cfg, slots, blobs)) {
        rollback(cfg, blobs, names);
        ce_verdict("snd: rolled back to retail behaviour (new ids unusable)");
        return 0;
    }
    ce_verdict("snd: OK %d voices, %d rows, I1/I2 hold", s_voice_count, CE_SND_ROWS_TOTAL);
    return 1;
}
