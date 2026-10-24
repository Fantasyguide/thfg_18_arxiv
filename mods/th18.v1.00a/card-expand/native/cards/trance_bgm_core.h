/* trance_bgm_core.h —— 灵界版 BGM（卡 113）的**纯逻辑**半边：fmt 记录 / 分段长度 /
 * 交叉淡化曲线 / 曲名 b 变体规则 / WAV 解析。无引擎地址、无浮点、有主机单测。
 *
 * 引擎侧接线在 `cards/trance_bgm.c`；方案与一手依据见
 * `engine/_shared/th13-th18-trance-bgm-research.md`（TH13 是"换一整条 b 轨 + 硬切"，
 * TH18 只有一路 BGM ⇒ 路线 A = DLL 侧建**第二路内存流**）。
 *
 * ## 本文件的两条硬约束（都是本仓已流血换来的门）
 *
 * 1. **零浮点**：`make dllx87` 要求我们的目标文件里 `fld/fst/fmul…` 出现 **0 次**
 *    （`CARD-CHECKLIST.md` §0.4）。所以这里全部是整数运算。
 * 2. **零引擎地址**：本文件要能在 Linux 主机上直接 `cc` 编出来跑单测
 *    （`native/tests/test_trance_bgm_core.c`）。任何 `0x48xxxx` 都不许出现在这里。
 */
#pragma once
#include <stddef.h>
#include <stdint.h>

/* ============================ 1. thbgm.fmt 的单条记录（0x34 字节） ============================
 *
 * 一手布局（`local/vendor/thtk/extlib/thtypes/bgm_types.h` 的 `bgm_fmt_t` + 本机直读
 * `local/th18.v1.00a/dat/thbgm.fmt` 两条独立来源一致）：
 *
 *   +0x00  fn[16]             曲名，NUL 终止（`fn[0] == 0` = 表尾哨兵）
 *   +0x10  track_offset       文件内偏移（`thbgm.dat`；第一条是 0x10 = 跳过 16 B 的 "ZWAV" 头）
 *   +0x14  preload_size       **内存流路径下与 track_size 逐条相等**（TH18 全部 18 条实测相等）
 *   +0x18  intro_size         **循环点**：播完一整条后回到这里
 *   +0x1c  track_size         **读取窗口长度**（= 曲目总字节数）
 *   +0x20  WAVEFORMATEX（18 B）：tag / channels / samples_per_sec / avg_bytes_per_sec /
 *                                block_align / bits_per_sample / cbSize
 *   +0x32  pad[2]
 *
 * ★ 两条**引擎侧**语义（一手反汇编，`th18:0x48acc0` 的 preload 分支，逐条对照）：
 *   · 读取窗口 = `[base, base + track_size)`，`base` = 我们交给构造成器的 PCM 指针；
 *   · 循环回卷 = `cur = base + intro_size`。
 *   ⇒ `intro_size = 0` + `track_size = PCM 全长` = 整段无缝循环；
 *     `intro_size = intro 段字节数` + `track_size = intro + 循环体` = **TH13 的 b 轨口径**
 *     （播完一整条回到循环点，而不是回到文件开头）。★★ 2026-10-24 起我们走的就是后者
 *     （见 `trance_bgm_fmt_init` 的 `intro_bytes`：素材带 intro，一手值取自主曲 fmt）。
 */
typedef struct {
    uint8_t  fn[16];
    int32_t  track_offset;
    int32_t  preload_size;
    int32_t  intro_size;
    int32_t  track_size;
    uint16_t wfx_tag;
    uint16_t channels;
    uint32_t samples_per_sec;
    uint32_t avg_bytes_per_sec;
    uint16_t block_align;
    uint16_t bits_per_sample;
    uint16_t cb_size;
    uint8_t  pad[2];
} trance_bgm_fmt_t;

_Static_assert(sizeof(trance_bgm_fmt_t) == 0x34, "thbgm.fmt 记录必须是 0x34 字节");
_Static_assert(offsetof(trance_bgm_fmt_t, track_offset)      == 0x10, "track_offset 必须在 +0x10");
_Static_assert(offsetof(trance_bgm_fmt_t, preload_size)      == 0x14, "preload_size 必须在 +0x14");
_Static_assert(offsetof(trance_bgm_fmt_t, intro_size)        == 0x18, "intro_size 必须在 +0x18");
_Static_assert(offsetof(trance_bgm_fmt_t, track_size)        == 0x1c, "track_size 必须在 +0x1c");
_Static_assert(offsetof(trance_bgm_fmt_t, samples_per_sec)   == 0x24, "WAVEFORMATEX 从 +0x20 起");
_Static_assert(offsetof(trance_bgm_fmt_t, block_align)       == 0x2c, "block_align 必须在 +0x2c");
_Static_assert(offsetof(trance_bgm_fmt_t, bits_per_sample)   == 0x2e, "bits_per_sample 必须在 +0x2e");

/* 素材：**真音频**（不再是占位音）。
 *
 * 卡侧从 `bgm/<当前曲名>b.wav` 读一条 WAV（三条候选目录，见 `trance_bgm.c`），解析出 PCM + 格式，
 * 再喂给引擎的内存流构造成器。生成器 = `tools/make_trance_bgm.py`
 * （TH18 自己的曲子 + naive 低音放大 + TH13 的 b 命名）。
 *
 * ★ 单个 PCM 缓冲的上限：**24 MiB**。理由：我们**一次只装一条**曲子的**整条素材**
 *   （intro + 循环体，22050 Hz）。★ 2026-10-24 方案 A 起素材**带 intro**（对齐 TH13 的 b 轨），
 *   所以最大的一条变成 **6 面关底 `th18_14b` = 20.96 MiB = 21,978,256 B**
 *   （`th18_14`：`track_size` = 43956512 B @44100 ÷2 降采样 = intro 569716 帧 + 循环体 5496246 帧）
 *   ⇒ 旧的 16 MiB 装不下它，这里抬到 24 MiB（余量 ~22%）。
 *   ⚠ 抬高的**安全性**：这块内存是 `VirtualAlloc` **按需分配**的（只在起流那一刻按实际文件大小申请、
 *   `finish_release()` 立刻 `VirtualFree`），**不是** DLL 的静态数据 ⇒ DLL 文件体积/常驻内存不受影响；
 *   而且素材大小 = **表内的固定数据**（生成期就定死并逐个核对），不是运行期算出来的。
 *   超限 ⇒ `ce_trance_bgm_start()` 直接拒绝（**不动主 BGM**），日志写明是哪一步。
 *   ⚠ 生成器里有一份同名常量（`DLL_MAX_PCM`）——`make_trance_bgm.py` 会读这个头文件对账，
 *   并且**拒绝生成**任何超过它的素材（生成期就红，不留到运行期才静默降级）。*/
#define TRANCE_BGM_MAX_PCM      (24u * 1024u * 1024u)
/* 素材目录名（相对 DLL 所在目录，或退一步：相对游戏 exe 目录）。 */
#define TRANCE_BGM_ASSET_SUBDIR "bgm"

/* 交叉淡化的总帧数（❓ 自定：原版 TH13 根本没有淡化，它是硬切 —— 我们加的是"加料"，
 * 见 TUNING.md。20 帧 ≈ 0.33 s，比 TH13 硬切柔和、又不至于拖到听得出重叠）。 */
#define TRANCE_BGM_XFADE_FRAMES 20

/* 音量单位：DirectSound 的**百分之一分贝**，0 = 用户设定的满音量，-10000 = 静音。
 * 一手：`th18:0x48a5a0` 的公式 `(1-(1-v/100)^2) * (cb+5000) - 5000` ⇒ cb=-10000 时约等于静音。 */
#define TRANCE_BGM_CB_FULL  0
#define TRANCE_BGM_CB_MUTE  (-10000)

/* ---- fmt 记录 ---- */
/* 按 (channels, rate, bits, pcm_bytes, intro_bytes) 填一条**自己用**的 fmt。
 * `intro_bytes` = 素材里 intro 段的字节数（**循环回绕点**）：
 *   · `0`（或越界/半帧） ⇒ 整段循环（2026-10-24 之前的老口径）；
 *   · `intro_game × avg`（卡侧按**主曲 fmt 的 intro_size** 用 avg 比例换算） ⇒ TH13 的 b 轨口径。
 * `name` 只用于日志/调试（内存流路径**不查表**，名字不进 thbgm.fmt 索引）。*/
void trance_bgm_fmt_init(trance_bgm_fmt_t *f, const char *name, uint32_t pcm_bytes,
                         uint32_t intro_bytes,
                         uint32_t channels, uint32_t rate, uint32_t bits);

/* ============================ 2. 口径（素材 ⟷ 主曲）============================
 *
 * ★★ 2026-10-24（`th18_06b` 错位事故的正面防线）：**素材的长度是由"哪个主曲口径"决定的**。
 *   主曲 fmt 的 `track_size` / `intro_size` 是 44100 口径的字节数，我们的素材是 22050 口径
 *   ⇒ 换算 `ours = engine × our_avg ÷ e_avg`。这一条是**独立量**：左边是文件真实字节数
 *   （生成期定死），右边是引擎当前那路 BGM 的 fmt（运行期一手）⇒ 两边对不上就说明
 *   **这份文件不是给这个主曲做的**，此时接管一定会错位（循环点/总长差多少，每圈就差多少）。
 *
 * 事故账（一手）：零售 `th18_06`（leveledit **关**）= intro 6.5074 s + loop 105.5463 s
 *   = **112.0537 s**；leveledit **开**时主曲被换成 `th18_06.ogg` + `.loop.ogg`
 *   = 30.9678 + 74.3226 = **105.2904 s**（thcrap bgmmod 的 `intro_size` = 第一个文件的解码字节）。
 *   `th18_06b_rearr` 一份文件服务两个口径 ⇒ 拿 105.29 s 的素材按 6.5074 s 的循环点播，
 *   每圈往回跳 **24.46 s**。现在两份文件（`…wav` 零售 / `…le.wav` leveledit）+ 这道闸。
 *
 * 单位：字节。`tol` 传 **1 帧**（= block_align，2ch/16bit ⇒ 4 B）；换算里有向下取整，
 *   一手的两条口径在实测里差 0–1 帧（leveledit 的 ogg 解码帧数取整），所以给 1 帧余量。
 */

/* `engine` 口径的字节数 → 我们的字节数（`e_avg` = 主曲 fmt 的 avg，`our_avg` = 素材的 avg）。
 * `e_avg`/`our_avg` 任一为 0 ⇒ 0（不猜）。*/
uint32_t trance_bgm_caliber_bytes(uint32_t e_bytes, uint32_t e_avg, uint32_t our_avg);

/* 口径闸：`mat_bytes`（素材真实字节数，可传全长或 intro 长度）与主曲换算值是否相符（±tol）。
 * 返回 1 = 相符；0 = 不符。**不符 ⇒ 不接管**（保持主曲），绝不"先放上去再说"。*/
int trance_bgm_caliber_match(uint32_t mat_bytes, uint32_t e_bytes,
                             uint32_t e_avg, uint32_t our_avg, uint32_t tol);

/* intro 闸（`trance_bgm.c` 的 `bgm_seek_to_engine` 用；也是"别把 intro_size 写成 0"的最后一道）：
 *   `intros`    = 本次要写进我们自己 fmt 的 intro 字节数（主曲 fmt 换算出来的）
 *   `cal_intro` = 加载时**口径匹配**给出的 intro 字节数（= 素材自带 intro 的正身）
 *   `tol`       = 1 帧
 * 返回 0 = 通过；
 *     -1 = 素材自带 intro（cal_intro ≠ 0）而这次算出来是 **0** ⇒ **拒绝接管**：
 *          写 `intro_size = 0` 会让引擎每播完一圈就回到文件开头 = **每圈重放 intro**；
 *     -2 = 两边差 > tol（主曲 fmt 在这几帧里被换过 / 读到了别的口径）⇒ 拒绝接管。*/
int trance_bgm_intro_gate(uint32_t intros, uint32_t cal_intro, uint32_t tol);

/* ---- 曲名：TH13 的 b 变体命名法 ----
 * 一手（`th13:0x405b1d`）：把当前曲名里**扩展名前的那个 `.` 覆盖成 `'b'` + NUL**
 *   ⇒ `"th13_01.wav"` → `"th13_01b"`，紧接着 `FUN_0043afc0`（`th13:0x43b004`）在末尾补 `".wav"`
 *   ⇒ 最终查 `th13_01b.wav` 这条 fmt 记录。
 *
 * 我们**同一条规则**，只是更宽容地处理"有没有扩展名"（TH18 的 `0x56d104` 存的是
 * `.wav` 带扩展名的名字，见 `trance_bgm.c` 的文件头；但引擎命令里的名字两种都出现过）：
 *   `"th18_03.wav"` → `"th18_03b.wav"`；`"th18_03"` → `"th18_03b.wav"`。
 *
 * 返回 0 = 成功；-1 = 名字不可用（空 / 以 `.` 开头 / 含路径分隔符或控制字符 / 太长 / `cap` 不够）。
 * ★ 拒绝路径分隔符是**安全**要求：这个名字会被拼进文件路径。*/
int trance_bgm_variant_name(const char *cur, char *out, size_t cap);

/* ---- 读一条 WAV（只用 RIFF 的 `fmt ` + `data` 两个块）----
 * 纯逻辑、零分配：`out->pcm` 直接指向传进来的缓冲内部（调用方负责那块内存的寿命）。
 * 只接受 **未压缩 PCM / 16 bit**（TH18 自己的 fmt 全表就是这个形状；其余一律拒绝，
 * 因为引擎的内存流构造成器只吃 PCM）。
 * 返回 0 = 成功；非 0 = 拒绝码（负值，见下表的 `TRANCE_BGM_WAV_E*`）。*/
typedef struct {
    const uint8_t *pcm;          /* `data` 块的有效载荷 */
    uint32_t       pcm_bytes;
    uint32_t       rate;
    uint16_t       channels;
    uint16_t       bits;
    uint16_t       block_align;
} trance_bgm_wav_t;

/* 拒绝码（**负数** ⇒ 卡侧日志里 `wav=%d` 一眼能对上是哪一种坏文件）*/
#define TRANCE_BGM_WAV_E_ARG         (-1)   /* 空指针 */
#define TRANCE_BGM_WAV_E_SMALL       (-2)   /* 连 RIFF 头都不够 */
#define TRANCE_BGM_WAV_E_NOTWAVE     (-3)   /* 不是 "RIFF"/"WAVE" */
#define TRANCE_BGM_WAV_E_TRUNC       (-4)   /* 块的声明长度越过缓冲末尾 */
#define TRANCE_BGM_WAV_E_FMT         (-5)   /* 有 `fmt ` 块但太短 */
#define TRANCE_BGM_WAV_E_NOFMT       (-6)   /* 没有 `fmt ` 块 */
#define TRANCE_BGM_WAV_E_NODATA      (-7)   /* 没有 `data` 块 / 长度 0 */
#define TRANCE_BGM_WAV_E_UNSUPPORTED (-8)   /* 非 PCM / 非 16 bit / 声道数或采样率不合规 */

int trance_bgm_wav_parse(const uint8_t *data, uint32_t size, trance_bgm_wav_t *out);

/* ---- 分段长度（引擎公式，一手 `th18:0x476ec3`–`0x476ece`） ----
 *   `seg = (avg_bytes_per_sec * block_align / 2)` 向下取整到 `block_align` 的整数倍；
 *   DirectSound buffer 长度 = `seg * 8`（8 段环）。引擎自己就是这么算的：
 *   44100/2ch/16bit ⇒ avg 176400、block 4 ⇒ seg 352800 B（= 2.0 s）、buffer 16 s。 */
uint32_t trance_bgm_segment_bytes(uint32_t avg_bytes_per_sec, uint32_t block_align);

/* ---- 交叉淡化 ----
 * `progress` 0 → 完全主 BGM（第二路静音）；>= total → 完全第二路（主 BGM 静音）。
 * 线性（分贝域线性 = 听感上近似等功率），两端点**精确**落在 FULL / MUTE 上。*/
void trance_bgm_xfade_mix(int32_t progress, int32_t total,
                          int32_t *out_main_cb, int32_t *out_second_cb);

/* ---- 占位 PCM：**已删除**（2026-09-13）
 *
 * 旧版这里有一个 `trance_bgm_fill_placeholder()`（4 秒 A3/E4 三角波），用途是"在真素材到位前
 * 把第二路流的机制跑通"。真素材（`tools/make_trance_bgm.py` 生成的 WAV）落地后它成了死代码，
 * 且**留着有害**：万一素材缺失时用它顶替，玩家听到的是"占位音"而不是"主 BGM 继续放"——
 * 本卡的纪律是**素材缺失 ⇒ 完全不接管**（主 BGM 一点不动，见 `AUDIT` §CO）。*/

/* 常量与语义脱钩自检（0 = ok）。接在卡侧 on_load 的 selfcheck 后面打日志用。 */
int trance_bgm_core_selfcheck(void);
