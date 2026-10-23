/* trance_bgm_core.c —— 灵界版 BGM 的纯逻辑半边（见 `trance_bgm_core.h` 的一手依据）。
 *
 * 全部整数运算、零引擎地址 ⇒ 能直接 `cc` 出来在主机上跑单测。
 */
#include <string.h>

#include "trance_bgm_core.h"

/* ------------------------------------------------------------------ fmt 记录 */

void trance_bgm_fmt_init(trance_bgm_fmt_t *f, const char *name, uint32_t pcm_bytes,
                         uint32_t intro_bytes,
                         uint32_t channels, uint32_t rate, uint32_t bits)
{
    uint32_t ba, avg;

    if (!f) return;
    memset(f, 0, sizeof *f);

    /* 名字：`fn[16]` 装不满就 NUL 补，超了**截断**（末字节留 NUL，保证是合法 C 串；
     * 内存流路径其实不查名，这一步只为日志与"万一日后被 grep"）。*/
    if (name) {
        size_t n = strlen(name);
        if (n > sizeof f->fn - 1) n = sizeof f->fn - 1;
        memcpy(f->fn, name, n);
    }

    /* WAVEFORMATEX（PCM，cbSize = 0）。块对齐 = 声道数 × 位深/8 —— 一手 TH18 fmt 全表是
     * 44100/2ch/16bit ⇒ block 4、avg 176400，用同一公式复算得上。*/
    ba  = channels * (bits / 8u);
    avg = rate * ba;

    f->wfx_tag            = 1;          /* WAVE_FORMAT_PCM */
    f->channels           = (uint16_t)channels;
    f->samples_per_sec    = rate;
    f->avg_bytes_per_sec  = avg;
    f->block_align        = (uint16_t)ba;
    f->bits_per_sample    = (uint16_t)bits;
    f->cb_size            = 0;

    /* ★ 内存流路径的两个字段（一手 `th18:0x48acc0` 的 preload 分支）：
     *   `track_size` = **读取窗口长度** ⇒ 给整段 PCM 全长（= intro + 循环体）；
     *   `intro_size` = **循环回卷点** ⇒ 给素材里 intro 段的字节数（**TH13 的 b 轨口径**：
     *   带 intro，播完一整条回到 `intro_size` 处而不是回到 0）。
     *   `preload_size` 照引擎自己的口径与 track_size 取同值（TH18 全 18 条实测相等）。
     *
     * ★★ 2026-10-24（方案 A，对齐 TH13）：素材**改成带 intro**了 —— 以前 `intro_size = 0`
     *   （素材只装循环体），主曲还在 intro 段时只能从循环起点起播（差一整个 intro 剩余长度，
     *   一手日志实测 17.1 s）。现在 `intro_bytes` 由卡侧按**主曲 fmt 的 intro_size**（一手值）
     *   用 avg 比例换算后传进来 ⇒ 回绕点 = 原曲的循环点。
     *   ⚠ 合法性：`0 <= intro_bytes < pcm_bytes` 且按 `block_align` 对齐（半帧会让回绕点串道）；
     *   越界/半帧 ⇒ 退回 0（= 老行为：整段循环），**绝不写一个引擎会读越界的值**。*/
    f->track_offset = 0;               /* 内存流不看它（没有文件句柄）*/
    f->preload_size = (int32_t)pcm_bytes;
    if (ba != 0u && intro_bytes > 0u && intro_bytes < pcm_bytes) {
        intro_bytes -= intro_bytes % ba;              /* 对齐到整帧（block_align）*/
        if (intro_bytes >= pcm_bytes) intro_bytes = 0u;
    } else {
        intro_bytes = 0u;
    }
    f->intro_size   = (int32_t)intro_bytes;
    f->track_size   = (int32_t)pcm_bytes;
}

/* ------------------------------------------------------------------ 曲名：TH13 的 b 命名法 */

/* 最长可接受的名字（thbgm.fmt 的 `fn[16]` 装 15 个字符 + NUL；留一倍余量给"别处来的名字"）。*/
#define TRANCE_BGM_NAME_MAX 31

int trance_bgm_variant_name(const char *cur, char *out, size_t cap)
{
    size_t base = 0, n = 0;

    if (!cur || !out) return -1;
    while (cur[n]) {
        unsigned char c = (unsigned char)cur[n];
        /* 安全门：这个名字会被拼进文件路径 ⇒ 路径分隔符/盘符/控制字符一律拒绝 */
        if (c == '/' || c == '\\' || c == ':' || c < 0x20) return -1;
        if (++n > TRANCE_BGM_NAME_MAX) return -1;
    }
    if (n == 0) return -1;

    /* 截到第一个 `.`（= TH13 覆盖的那一位）；没有 `.` 就是全长 */
    while (base < n && cur[base] != '.') ++base;
    if (base == 0) return -1;

    /* 需要 base + 'b' + ".wav" + NUL */
    if (cap < base + 6) return -1;

    memcpy(out, cur, base);
    out[base + 0] = 'b';
    out[base + 1] = '.';
    out[base + 2] = 'w';
    out[base + 3] = 'a';
    out[base + 4] = 'v';
    out[base + 5] = 0;
    return 0;
}

/* ------------------------------------------------------------------ WAV（只用 fmt + data 两个块） */

static uint32_t rd32(const uint8_t *p) { return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24); }
static uint32_t rd16(const uint8_t *p) { return (uint32_t)p[0] | ((uint32_t)p[1] << 8); }

int trance_bgm_wav_parse(const uint8_t *data, uint32_t size, trance_bgm_wav_t *out)
{
    uint32_t off, end, pcm_off = 0, pcm_len = 0;
    uint32_t rate = 0, avg = 0, tag = 0, ch = 0, bits = 0, block = 0;
    int      have_fmt = 0;

    if (!out) return TRANCE_BGM_WAV_E_ARG;
    /* ★ 先清零：失败时出参也必须是"干净的空" —— 否则调用方会拿着半截旧值去拼 fmt 记录 */
    memset(out, 0, sizeof *out);
    if (!data) return TRANCE_BGM_WAV_E_ARG;

    if (size < 12u + 8u) return TRANCE_BGM_WAV_E_SMALL;
    if (memcmp(data, "RIFF", 4) != 0 || memcmp(data + 8, "WAVE", 4) != 0) return TRANCE_BGM_WAV_E_NOTWAVE;
    /* RIFF 的声明长度只做**弱**校验（有些工具写得不准）：不匹配只算可疑，不当场拒绝 ——
     * 真正管边界的是下面逐块的 `off + 8 + sz <= size`。*/
    off = 12;
    while (off + 8u <= size) {
        const uint8_t *ck = data + off;
        uint32_t sz = rd32(ck + 4);
        uint32_t body = off + 8u;
        if (sz > size - body) return TRANCE_BGM_WAV_E_TRUNC;      /* 无符号减法：不会回绕 */
        end = body + sz;
        if (memcmp(ck, "fmt ", 4) == 0) {
            if (sz < 16u) return TRANCE_BGM_WAV_E_FMT;
            tag   = rd16(data + body + 0);
            ch    = rd16(data + body + 2);
            rate  = rd32(data + body + 4);
            avg   = rd32(data + body + 8);
            block = rd16(data + body + 12);
            bits  = rd16(data + body + 14);
            have_fmt = 1;
        } else if (memcmp(ck, "data", 4) == 0) {
            if (sz == 0) return TRANCE_BGM_WAV_E_NODATA;          /* 空 data 直接判死 */
            pcm_off = body;
            pcm_len = sz;
            /* ★ 找到 data 就**停**：再往下扫会把 PCM 字节当块头解析（`data` 载荷里
             *   随机出现的 "data"/"fmt " 四字节会被当真），那就是个静默的解析漏洞。*/
            break;
        }
        off = end + (sz & 1u);                                    /* 奇数长度块有 1 字节填充 */
    }
    if (!have_fmt)  return TRANCE_BGM_WAV_E_NOFMT;
    if (!pcm_len)   return TRANCE_BGM_WAV_E_NODATA;
    /* 只吃未压缩 PCM / 16 bit / 1–2 声道（引擎的内存流构造成器只认 PCM）*/
    if (tag != 1u || bits != 16u) return TRANCE_BGM_WAV_E_UNSUPPORTED;
    if (ch != 1u && ch != 2u)     return TRANCE_BGM_WAV_E_UNSUPPORTED;
    if (rate == 0u || rate > 192000u) return TRANCE_BGM_WAV_E_UNSUPPORTED;
    if (block != ch * 2u)         return TRANCE_BGM_WAV_E_UNSUPPORTED;   /* block = ch × bits/8 */
    if (avg != rate * block)      return TRANCE_BGM_WAV_E_UNSUPPORTED;
    if (pcm_len % block)          return TRANCE_BGM_WAV_E_UNSUPPORTED;   /* 半帧 ⇒ 引擎会读越界 */

    out->pcm         = data + pcm_off;
    out->pcm_bytes   = pcm_len;
    out->rate        = rate;
    out->channels    = (uint16_t)ch;
    out->bits        = (uint16_t)bits;
    out->block_align = (uint16_t)block;
    return 0;
}

/* ------------------------------------------------------------------ 分段长度 */

uint32_t trance_bgm_segment_bytes(uint32_t avg_bytes_per_sec, uint32_t block_align)
{
    uint64_t half;

    if (block_align == 0) return 0;
    /* 一手 `th18:0x476ec3`–`0x476ece`：
     *   MOVZX ESI, word [fmt+0x2c]     ; block align
     *   MOV   EDI,       [fmt+0x24]    ; nAvgBytesPerSec
     *   IMUL  EDI, ESI / SHL EDI,2 / SHR EDI,3      ; = avg*ba/2（*4/8）
     *   DIV ESI / SUB EDI, EDX                       ; 向下取整到 ba 的整数倍
     * ⇒ buffer 总长 = 这个值 × 8（8 段环）。*/
    half = ((uint64_t)avg_bytes_per_sec * block_align) >> 1;
    return (uint32_t)((half / block_align) * block_align);
}

/* ------------------------------------------------------------------ 交叉淡化 */

void trance_bgm_xfade_mix(int32_t progress, int32_t total,
                          int32_t *out_main_cb, int32_t *out_second_cb)
{
    int32_t p, done;

    /* 退化（total <= 0）：没有过渡可言 ⇒ 直接给终态。
     * ⚠ 不能在这里除 0 —— 卡侧的 `TRANCE_BGM_XFADE_FRAMES` 万一被改成 0，也只是"瞬间切换"。*/
    if (total <= 0) {
        if (out_main_cb)   *out_main_cb   = TRANCE_BGM_CB_MUTE;
        if (out_second_cb) *out_second_cb = TRANCE_BGM_CB_FULL;
        return;
    }

    p = progress;
    if (p < 0)        p = 0;
    else if (p > total) p = total;

    /* `10000 * total` 不能溢出：卡侧的 total 是 20 帧这种量级；这条钳位是防"被误设成 30 万"。*/
    if (total > 200000) total = 200000;

    done = (10000 * p) / total;                 /* 0..10000，整数除法的方向 = 向下取整 */
    if (out_main_cb)   *out_main_cb   = -done;              /* 0 → -10000 */
    if (out_second_cb) *out_second_cb = done - 10000;       /* -10000 → 0 */
}

/* ------------------------------------------------------------------ 自检 */

int trance_bgm_core_selfcheck(void)
{
    uint32_t seg, ba, avg;
    int32_t  m = 0, s = 0;
    char     name[64];
    static const uint8_t fake_wav[44] = {
        'R','I','F','F', 36,0,0,0, 'W','A','V','E',
        'f','m','t',' ', 16,0,0,0, 1,0, 2,0, 0x44,0xac,0,0, 0x10,0xb1,2,0, 4,0, 16,0,
        'd','a','t','a', 0,0,0,0
    };
    trance_bgm_wav_t w;

    /* ① 分段长度：必须能被 block_align 整除（= 一手 `avg×block/2` 再向下取整到 block）。
     *    44100/2ch/16bit ⇒ 352800 B = 2.0 s（buffer 总长 = 它 × 8 = 16 s）。*/
    ba  = 2u * 2u;
    avg = 44100u * ba;
    seg = trance_bgm_segment_bytes(avg, ba);
    if (seg != (avg * ba) / 2u)  return 2;
    if (seg == 0 || (seg % ba) != 0) return 3;
    /* 素材默认是 22050/2ch/16bit（TH13 的 b 轨格式）⇒ 88200*4/2 = 176400 B = 2.0 s */
    if (trance_bgm_segment_bytes(88200u, 4u) != 176400u) return 7;

    /* ② 淡化两端点必须精确落在 FULL / MUTE 上（不然会留下一点残余音量）。*/
    trance_bgm_xfade_mix(0, TRANCE_BGM_XFADE_FRAMES, &m, &s);
    if (m != TRANCE_BGM_CB_FULL || s != TRANCE_BGM_CB_MUTE) return 4;
    trance_bgm_xfade_mix(TRANCE_BGM_XFADE_FRAMES, TRANCE_BGM_XFADE_FRAMES, &m, &s);
    if (m != TRANCE_BGM_CB_MUTE || s != TRANCE_BGM_CB_FULL) return 5;

    /* ③ 过渡帧数必须是正数（0 会让淡化变成"瞬间硬切"——那也该是显式选择，不是手滑）。*/
    if (TRANCE_BGM_XFADE_FRAMES <= 0) return 6;

    /* ④ b 变体命名法（TH13 一手规则）：两种写法都必须落到同一个文件名上。*/
    if (trance_bgm_variant_name("th18_03.wav", name, sizeof name) != 0) return 8;
    if (strcmp(name, "th18_03b.wav") != 0) return 9;
    if (trance_bgm_variant_name("th18_03", name, sizeof name) != 0) return 10;
    if (strcmp(name, "th18_03b.wav") != 0) return 11;
    if (trance_bgm_variant_name("../../x.wav", name, sizeof name) == 0) return 12;  /* 路径注入必须被拒 */

    /* ⑤ WAV 解析器：夹一个 44 B 的空 WAV（data 长度 0 ⇒ 必须被拒），
     *    再把 data 长度改成 4 ⇒ 必须成功且字段逐个对。*/
    if (trance_bgm_wav_parse(fake_wav, sizeof fake_wav, &w) != TRANCE_BGM_WAV_E_NODATA) return 13;
    {
        uint8_t buf[48];
        memcpy(buf, fake_wav, sizeof fake_wav);
        buf[40] = 4;                                  /* data 长度字段的低字节 = 4（一帧 2ch/16bit）*/
        if (trance_bgm_wav_parse(buf, sizeof buf, &w) != 0) return 14;
        if (w.pcm_bytes != 4u || w.rate != 44100u || w.channels != 2u ||
            w.bits != 16u || w.block_align != 4u) return 15;
        buf[40] = 2;                                  /* 半帧 ⇒ 必须被拒 */
        if (trance_bgm_wav_parse(buf, sizeof buf, &w) == 0) return 16;
        buf[40] = 4; buf[41] = 0xff;                  /* data 越界 ⇒ 必须被拒 */
        if (trance_bgm_wav_parse(buf, sizeof buf, &w) != TRANCE_BGM_WAV_E_TRUNC) return 18;
    }

    /* ⑥ 缓冲上限必须**够装最大的那条素材**（否则那条曲子会被静默拒绝）。
     *    ★ 2026-10-24 起素材带 intro ⇒ 最大的一条 = 6 面关底 `th18_14b`
     *    = `track_size / 2` = 43956512 / 2 = **21978256 B = 20.96 MiB**（见头文件那条注释）。*/
    if (TRANCE_BGM_MAX_PCM < 21978256u) return 17;

    return 0;
}
