/* test_trance_bgm_core.c —— 灵界版 BGM 纯逻辑半边的主机单测（`cards/trance_bgm_core.c`）。
 *
 * 跑法（Makefile 的 test-host 里有一行）：
 *   cc -std=c11 -Wall -Wextra -Werror -o tests/build/test_trance_bgm_core \
 *      tests/test_trance_bgm_core.c cards/trance_bgm_core.c && ./tests/build/test_trance_bgm_core
 *
 * 这里钉的都是**会静默出错**的东西：结构体布局（错一个字节 = 引擎按错位读 fmt）、
 * 分段长度（错 = buffer 长度不对/越界）、淡化端点（错 = 留下残余音量或爆音）、
 * 占位 PCM（错 = 越界或削顶）。**不钉"好不好听"。**
 */
#include <stdio.h>
#include <string.h>

#include "../cards/trance_bgm_core.h"

static int g_fail;
static int g_asserts;

#define CHECK(cond, ...)                                                       \
    do {                                                                       \
        ++g_asserts;                                                           \
        if (!(cond)) {                                                         \
            ++g_fail;                                                          \
            printf("  FAIL %s:%d: ", __FILE__, __LINE__);                      \
            printf(__VA_ARGS__);                                               \
            printf("\n");                                                      \
        }                                                                      \
    } while (0)

/* ---------------- §A 结构体布局（0x34 字节，逐字段） ---------------- */
static void test_layout(void)
{
    printf("§A thbgm.fmt 记录布局\n");
    CHECK(sizeof(trance_bgm_fmt_t) == 0x34, "sizeof = %zu, want 0x34", sizeof(trance_bgm_fmt_t));
    CHECK(offsetof(trance_bgm_fmt_t, fn) == 0x00, "fn");
    CHECK(offsetof(trance_bgm_fmt_t, track_offset) == 0x10, "track_offset");
    CHECK(offsetof(trance_bgm_fmt_t, preload_size) == 0x14, "preload_size");
    CHECK(offsetof(trance_bgm_fmt_t, intro_size) == 0x18, "intro_size");
    CHECK(offsetof(trance_bgm_fmt_t, track_size) == 0x1c, "track_size");
    /* WAVEFORMATEX 起于 +0x20（一手：本机直读 thbgm.fmt，tag/ch/rate 逐个落在 0x20/0x22/0x24）*/
    CHECK(offsetof(trance_bgm_fmt_t, wfx_tag) == 0x20, "wfx_tag");
    CHECK(offsetof(trance_bgm_fmt_t, channels) == 0x22, "channels");
    CHECK(offsetof(trance_bgm_fmt_t, samples_per_sec) == 0x24, "samples_per_sec");
    CHECK(offsetof(trance_bgm_fmt_t, avg_bytes_per_sec) == 0x28, "avg_bytes_per_sec");
    CHECK(offsetof(trance_bgm_fmt_t, block_align) == 0x2c, "block_align");
    CHECK(offsetof(trance_bgm_fmt_t, bits_per_sample) == 0x2e, "bits_per_sample");
    CHECK(offsetof(trance_bgm_fmt_t, cb_size) == 0x30, "cb_size");
}

/* ---------------- §B fmt 记录构造 ---------------- */
static void test_fmt_init(void)
{
    trance_bgm_fmt_t f;
    char name[64];

    printf("§B fmt 记录构造\n");
    trance_bgm_fmt_init(&f, "trance_bgm_placeholder.wav", 705600u, 2u, 44100u, 16u);

    CHECK(f.wfx_tag == 1, "tag = %u", f.wfx_tag);
    CHECK(f.channels == 2, "ch = %u", f.channels);
    CHECK(f.samples_per_sec == 44100u, "rate = %u", f.samples_per_sec);
    CHECK(f.avg_bytes_per_sec == 176400u, "avg = %u", f.avg_bytes_per_sec);
    CHECK(f.block_align == 4, "block = %u", f.block_align);
    CHECK(f.bits_per_sample == 16, "bits = %u", f.bits_per_sample);
    CHECK(f.cb_size == 0, "cbSize = %u", f.cb_size);
    /* ★ 内存流的两个字段：整段循环（intro 0）+ 读窗口 = 全长 */
    CHECK(f.intro_size == 0, "intro = %d（必须 0 = 整段回卷）", f.intro_size);
    CHECK(f.track_size == 705600, "track = %d", f.track_size);
    CHECK(f.preload_size == f.track_size, "preload(%d) != track(%d)（引擎自己的口径是相等）",
          f.preload_size, f.track_size);
    CHECK(f.track_offset == 0, "内存流不看 track_offset，应为 0");

    /* 名字必须 NUL 终止（且截断到 15 字符），否则引擎侧/日志侧读越界 */
    memcpy(name, f.fn, sizeof f.fn);
    name[sizeof f.fn] = '\0';
    CHECK(strcmp(name, "trance_bgm_plac") == 0, "截断后的名字 = \"%s\"", name);
    CHECK(f.fn[15] == 0, "fn[15] 必须是 NUL");

    /* 超长名字 + 空名字都不许越界 */
    trance_bgm_fmt_init(&f, "0123456789abcdefghijklmnop", 100u, 1u, 22050u, 8u);
    CHECK(f.fn[15] == 0, "超长名字后 fn[15] = %u", f.fn[15]);
    CHECK(f.block_align == 1 && f.avg_bytes_per_sec == 22050u, "8bit 单声道：block %u avg %u",
          f.block_align, f.avg_bytes_per_sec);
    trance_bgm_fmt_init(&f, NULL, 100u, 2u, 44100u, 16u);
    CHECK(f.fn[0] == 0, "NULL 名字 ⇒ fn 全 0");

    /* 同样的入参两次 ⇒ 逐字节相同（纯函数，写进契约） */
    {
        trance_bgm_fmt_t a, b;
        trance_bgm_fmt_init(&a, "x.wav", 1234u, 2u, 44100u, 16u);
        trance_bgm_fmt_init(&b, "x.wav", 1234u, 2u, 44100u, 16u);
        CHECK(memcmp(&a, &b, sizeof a) == 0, "fmt_init 非确定性");
    }
}

/* ---------------- §C 分段长度（引擎公式） ---------------- */
static void test_segment(void)
{
    printf("§C 分段长度（引擎公式 0x476ec3）\n");
    /* 一手实测：TH18 fmt 全表 44100/2ch/16bit ⇒ seg 352800 B = 2.0 s、buffer 16 s */
    CHECK(trance_bgm_segment_bytes(176400u, 4u) == 352800u,
          "44100/2ch/16bit seg = %u, want 352800", trance_bgm_segment_bytes(176400u, 4u));
    /* 22050/2ch/16bit（TH13 的 b 轨格式）= 88200*4/2 = 176400 */
    CHECK(trance_bgm_segment_bytes(88200u, 4u) == 176400u, "22050 seg");
    /* 必须能被 block_align 整除（DirectSound 要求，否则最后一帧被切一半）*/
    CHECK(trance_bgm_segment_bytes(176401u, 4u) % 4u == 0, "seg 必须对齐 block");
    CHECK(trance_bgm_segment_bytes(12345u, 0u) == 0, "block 0 ⇒ 0（不能除 0）");
    CHECK(trance_bgm_segment_bytes(0u, 4u) == 0, "avg 0 ⇒ 0");
    /* 大值不许溢出（一手那条 IMUL 带 &0x3fffffff 的钳，我们不许更差）*/
    CHECK(trance_bgm_segment_bytes(0xffffffffu, 4u) > 0u, "大 avg 不许溢出成 0/负数");
}

/* ---------------- §D 交叉淡化 ---------------- */
static void test_xfade(void)
{
    int32_t m, s;
    int i, prev_m = 0, prev_s = -10001;
    const int total = TRANCE_BGM_XFADE_FRAMES;

    printf("§D 交叉淡化\n");
    /* 端点必须精确 */
    trance_bgm_xfade_mix(0, total, &m, &s);
    CHECK(m == 0 && s == -10000, "p=0 ⇒ main %d second %d", m, s);
    trance_bgm_xfade_mix(total, total, &m, &s);
    CHECK(m == -10000 && s == 0, "p=total ⇒ main %d second %d", m, s);
    /* 越界钳位 */
    trance_bgm_xfade_mix(-5, total, &m, &s);
    CHECK(m == 0 && s == -10000, "p<0 钳到起点");
    trance_bgm_xfade_mix(total + 99, total, &m, &s);
    CHECK(m == -10000 && s == 0, "p>total 钳到终点");
    /* 单调 + 两端都在合法范围 */
    for (i = 0; i <= total; ++i) {
        trance_bgm_xfade_mix(i, total, &m, &s);
        CHECK(m <= 0 && m >= -10000, "main 越界 %d", m);
        CHECK(s <= 0 && s >= -10000, "second 越界 %d", s);
        CHECK(m <= prev_m, "main 不单调：%d -> %d", prev_m, m);
        CHECK(s >= prev_s, "second 不单调：%d -> %d", prev_s, s);
        CHECK(m + s <= 0 && m + s >= -10000, "两路之和越界 %d", m + s);
        prev_m = m; prev_s = s;
    }
    /* 退化：total <= 0 不能除 0，且结果应是"已淡完"*/
    trance_bgm_xfade_mix(0, 0, &m, &s);
    CHECK(m == -10000 && s == 0, "total=0 ⇒ 终态");
    trance_bgm_xfade_mix(7, -3, &m, &s);
    CHECK(m == -10000 && s == 0, "total<0 ⇒ 终态");
    /* 大 total 不许溢出（10000*200000 仍在 int32 内）*/
    trance_bgm_xfade_mix(100000, 200000, &m, &s);
    CHECK(m == -5000, "中点 half = %d, want -5000", m);
    /* NULL 出参不许崩 */
    trance_bgm_xfade_mix(1, 2, NULL, NULL);
}

/* ---------------- §E 曲名 → b 变体（TH13 的命名法） ---------------- */
static void test_variant_name(void)
{
    char out[64];

    printf("§E 曲名 → b 变体（TH13 `th13:0x405b1d` 的规则）\n");

    /* 一手：TH13 把扩展名的 `.` 覆盖成 `b`，再补 ".wav" ⇒ th13_01.wav → th13_01b.wav */
    CHECK(trance_bgm_variant_name("th13_01.wav", out, sizeof out) == 0, "返回值非 0");
    CHECK(strcmp(out, "th13_01b.wav") == 0, "得到 \"%s\"", out);

    /* TH18 的两种写法必须落到**同一个文件名**上（0x56d104 带扩展名，引擎命令里两种都出现过）*/
    CHECK(trance_bgm_variant_name("th18_03.wav", out, sizeof out) == 0, "带扩展名失败");
    CHECK(strcmp(out, "th18_03b.wav") == 0, "带扩展名得到 \"%s\"", out);
    CHECK(trance_bgm_variant_name("th18_03", out, sizeof out) == 0, "不带扩展名失败");
    CHECK(strcmp(out, "th18_03b.wav") == 0, "不带扩展名得到 \"%s\"", out);

    /* 多点 / 别的前缀都只看**第一个**点 */
    CHECK(trance_bgm_variant_name("th128_08.wav", out, sizeof out) == 0 &&
          strcmp(out, "th128_08b.wav") == 0, "128 曲：\"%s\"", out);
    CHECK(trance_bgm_variant_name("a.b.c", out, sizeof out) == 0 &&
          strcmp(out, "ab.wav") == 0, "多点：\"%s\"", out);

    /* ★ 安全门：会被拼进文件路径 ⇒ 路径分隔符/盘符/控制字符一律拒绝 */
    CHECK(trance_bgm_variant_name("../../x.wav", out, sizeof out) != 0, "路径穿越没被拒");
    CHECK(trance_bgm_variant_name("C:\\x.wav", out, sizeof out) != 0, "盘符没被拒");
    CHECK(trance_bgm_variant_name("a/b.wav", out, sizeof out) != 0, "斜杠没被拒");
    /* 退化入参 */
    CHECK(trance_bgm_variant_name("", out, sizeof out) != 0, "空串没被拒");
    CHECK(trance_bgm_variant_name(NULL, out, sizeof out) != 0, "NULL 名字没被拒");
    CHECK(trance_bgm_variant_name("th18_03.wav", NULL, 64) != 0, "NULL 出参没被拒");
    CHECK(trance_bgm_variant_name(".wav", out, sizeof out) != 0, "只有扩展名没被拒");
    CHECK(trance_bgm_variant_name("th18_03.wav", out, 4) != 0, "cap 太小没被拒");
    /* 超长名字（> 31）拒绝，而不是截断成另一个文件 */
    {
        char big[80];
        memset(big, 'x', sizeof big - 1);
        big[sizeof big - 1] = 0;
        CHECK(trance_bgm_variant_name(big, out, sizeof out) != 0, "超长名字没被拒");
    }
    /* 15 字符（= fmt 的 fn[16] 能装下的上限）必须能过 */
    CHECK(trance_bgm_variant_name("0123456789abcd.", out, sizeof out) == 0 &&
          strcmp(out, "0123456789abcdb.wav") == 0, "15 字符边界：\"%s\"", out);
}

/* ---------------- §F WAV 解析（引擎只吃 fmt + data） ---------------- */

/* 拼一个 44 B 的 canonical WAV 头 + `pcm` 个字节（`pcm` 必须是 block 的整数倍）。 */
static uint32_t make_wav(uint8_t *buf, uint32_t cap, uint32_t pcm,
                         uint16_t ch, uint32_t rate, uint16_t bits, uint16_t tag)
{
    uint32_t block = (uint32_t)ch * (bits / 8u);
    uint32_t avg   = rate * block;
    uint32_t i;

    if (cap < 44u + pcm) return 0;
    memset(buf, 0, 44u + pcm);
    memcpy(buf + 0, "RIFF", 4);
    buf[4] = (uint8_t)(36u + pcm); buf[5] = (uint8_t)((36u + pcm) >> 8);
    buf[6] = (uint8_t)((36u + pcm) >> 16); buf[7] = (uint8_t)((36u + pcm) >> 24);
    memcpy(buf + 8, "WAVEfmt ", 8);
    buf[16] = 16;                                   /* fmt 块长度 = 16 就行 */
    buf[20] = (uint8_t)tag; buf[21] = 0;
    buf[22] = (uint8_t)ch;  buf[23] = 0;
    buf[24] = (uint8_t)rate; buf[25] = (uint8_t)(rate >> 8);
    buf[26] = (uint8_t)(rate >> 16); buf[27] = (uint8_t)(rate >> 24);
    buf[28] = (uint8_t)avg; buf[29] = (uint8_t)(avg >> 8);
    buf[30] = (uint8_t)(avg >> 16); buf[31] = (uint8_t)(avg >> 24);
    buf[32] = (uint8_t)block; buf[33] = 0;
    buf[34] = (uint8_t)bits;  buf[35] = 0;
    memcpy(buf + 36, "data", 4);
    buf[40] = (uint8_t)pcm; buf[41] = (uint8_t)(pcm >> 8);
    buf[42] = (uint8_t)(pcm >> 16); buf[43] = (uint8_t)(pcm >> 24);
    for (i = 0; i < pcm; ++i) buf[44 + i] = (uint8_t)(i + 1);   /* 能认出来的样本 */
    return 44u + pcm;
}

static void test_wav_parse(void)
{
    static uint8_t buf[4096];
    trance_bgm_wav_t w;
    uint32_t n;

    printf("§F WAV 解析（只认 fmt + data；PCM/16bit/1–2ch）\n");

    /* 正常：22050/2ch/16bit（= 生成器默认的输出格式）*/
    n = make_wav(buf, sizeof buf, 400u, 2u, 22050u, 16u, 1u);
    CHECK(trance_bgm_wav_parse(buf, n, &w) == 0, "正常 WAV 被拒");
    CHECK(w.pcm_bytes == 400u, "pcm_bytes = %u", w.pcm_bytes);
    CHECK(w.rate == 22050u && w.channels == 2u && w.bits == 16u, "格式字段错");
    CHECK(w.block_align == 4u, "block_align = %u", w.block_align);
    CHECK(w.pcm == buf + 44, "pcm 指针没指向 data 载荷");
    CHECK(w.pcm[0] == 1 && w.pcm[399] == (uint8_t)400, "载荷内容错");

    /* 44100/2ch/16bit（= 不降采样时）与单声道都要能过 */
    n = make_wav(buf, sizeof buf, 400u, 2u, 44100u, 16u, 1u);
    CHECK(trance_bgm_wav_parse(buf, n, &w) == 0 && w.rate == 44100u, "44100 被拒");
    n = make_wav(buf, sizeof buf, 400u, 1u, 22050u, 16u, 1u);
    CHECK(trance_bgm_wav_parse(buf, n, &w) == 0 && w.channels == 1u, "单声道被拒");

    /* 拒绝面：每一种都得给出**不同的**负码（日志里靠它定位）*/
    CHECK(trance_bgm_wav_parse(NULL, 100, &w) == TRANCE_BGM_WAV_E_ARG, "NULL");
    CHECK(trance_bgm_wav_parse(buf, 8, &w) == TRANCE_BGM_WAV_E_SMALL, "太短");
    memcpy(buf, "RIFX", 4);
    CHECK(trance_bgm_wav_parse(buf, 100, &w) == TRANCE_BGM_WAV_E_NOTWAVE, "非 RIFF");
    n = make_wav(buf, sizeof buf, 400u, 2u, 22050u, 16u, 1u);
    buf[40] = 0xff; buf[41] = 0xff;                 /* data 声明 65535 > 实际 */
    CHECK(trance_bgm_wav_parse(buf, n, &w) == TRANCE_BGM_WAV_E_TRUNC, "data 越界没被拒");
    n = make_wav(buf, sizeof buf, 400u, 2u, 22050u, 16u, 1u);
    buf[40] = buf[41] = buf[42] = buf[43] = 0;       /* data 长度 0 */
    CHECK(trance_bgm_wav_parse(buf, n, &w) == TRANCE_BGM_WAV_E_NODATA, "空 data 没被拒");
    n = make_wav(buf, sizeof buf, 400u, 2u, 22050u, 16u, 1u);
    memcpy(buf + 12, "junk", 4);                     /* 没有 fmt 块 */
    CHECK(trance_bgm_wav_parse(buf, n, &w) == TRANCE_BGM_WAV_E_NOFMT, "缺 fmt 没被拒");
    n = make_wav(buf, sizeof buf, 400u, 2u, 22050u, 16u, 3u);   /* IEEE float */
    CHECK(trance_bgm_wav_parse(buf, n, &w) == TRANCE_BGM_WAV_E_UNSUPPORTED, "非 PCM 没被拒");
    n = make_wav(buf, sizeof buf, 400u, 2u, 22050u, 8u, 1u);    /* 8 bit */
    CHECK(trance_bgm_wav_parse(buf, n, &w) == TRANCE_BGM_WAV_E_UNSUPPORTED, "8bit 没被拒");
    n = make_wav(buf, sizeof buf, 400u, 6u, 22050u, 16u, 1u);   /* 6 声道 */
    CHECK(trance_bgm_wav_parse(buf, n, &w) == TRANCE_BGM_WAV_E_UNSUPPORTED, "6ch 没被拒");
    n = make_wav(buf, sizeof buf, 402u, 2u, 22050u, 16u, 1u);   /* 半帧（402 不是 4 的倍数）*/
    CHECK(trance_bgm_wav_parse(buf, n, &w) == TRANCE_BGM_WAV_E_UNSUPPORTED, "半帧没被拒");
    n = make_wav(buf, sizeof buf, 400u, 2u, 22050u, 16u, 1u);
    buf[24] = buf[25] = buf[26] = buf[27] = 0;       /* 采样率 0 */
    CHECK(trance_bgm_wav_parse(buf, n, &w) == TRANCE_BGM_WAV_E_UNSUPPORTED, "rate 0 没被拒");
    /* 非法入参不许写坏出参结构 */
    memset(&w, 0xab, sizeof w);
    CHECK(trance_bgm_wav_parse(NULL, 0, &w) != 0 && w.pcm == NULL && w.rate == 0u,
          "失败时没把出参清零");
}

/* ---------------- §G 自检 ---------------- */
static void test_selfcheck(void)
{
    printf("§G core selfcheck\n");
    CHECK(trance_bgm_core_selfcheck() == 0, "selfcheck = %d（应为 0）", trance_bgm_core_selfcheck());
    CHECK(TRANCE_BGM_CB_MUTE == -10000 && TRANCE_BGM_CB_FULL == 0, "音量常量漂了");
    /* 缓冲上限必须够装**最大的那条**循环段：14 首素材里最大的是 6 面关底 `th18_14b`
     * = 18.79 MiB = 19701056 B（生成器里有一份同名常量：`tools/make_trance_bgm.py`，
     * 它会读这个头文件对账、并且拒绝生成任何超过它的素材）*/
    CHECK(TRANCE_BGM_MAX_PCM >= 19701056u,
          "TRANCE_BGM_MAX_PCM = %u 装不下 th18_14b 的循环段（19701056 B）", TRANCE_BGM_MAX_PCM);
    CHECK(sizeof(TRANCE_BGM_ASSET_SUBDIR) == 4, "素材子目录名 = \"%s\"", TRANCE_BGM_ASSET_SUBDIR);
}

int main(void)
{
    printf("trance_bgm_core 单测\n");
    test_layout();
    test_fmt_init();
    test_segment();
    test_xfade();
    test_variant_name();
    test_wav_parse();
    test_selfcheck();
    printf("\n%d 断言，%d 失败\n", g_asserts, g_fail);
    return g_fail ? 1 : 0;
}
