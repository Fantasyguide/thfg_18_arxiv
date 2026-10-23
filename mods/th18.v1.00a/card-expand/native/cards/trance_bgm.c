/* trance_bgm.c —— 灵界版 BGM（卡 113）的**引擎侧接线**：用引擎自己的内存流构造成器
 * 再开一路 `CStreamingSound`，与主 BGM 交叉淡化；素材是 `tools/make_trance_bgm.py`
 * 生成的 **TH18 自己的曲子 + naive 低音放大** 的 WAV（TH13 的 `NNb` 命名法）。
 *
 * 纯逻辑（fmt 记录 / 分段长度 / 淡化曲线 / 曲名 b 规则 / WAV 解析）在 `cards/trance_bgm_core.c`，
 * 有主机单测。**本文件只做"找文件 → 读进内存 → 把字节交给引擎 → 音量每帧写回去 → 出事收干净"。**
 *
 * 方案与三条路线的代价对照：[`engine/_shared/th13-th18-trance-bgm-research.md`]
 * （../../../engine/_shared/th13-th18-trance-bgm-research.md）。
 * 一句话：TH13 原版**不是双轨**（单条 buffer + 换一整条 `NNb` 轨 + 同时间点 seek 的硬切）；
 * TH18 数据里**没有 b 轨**、`cmd 9` 是死代码 ⇒ 走"自己再开一路"的路线 A，
 * 素材侧则由我们在 `thbgm.dat` 之外另造（不改任何游戏数据文件）。
 *
 * ## 一手地址链（本机 Capstone / Ghidra 反汇编 `local/th18.v1.00a/th18.exe`，逐条可复核）
 *
 * | 件 | 地址 | 一手事实 |
 * | --- | --- | --- |
 * | DirectSound 全局 | `0x56ad8c` | 引擎自己的调用点是 `MOV ECX,[0x56ad8c]`（值即构造成器的 `this`）|
 * | 引擎 BGM 流指针 | `0x570484` | 唯一一路 BGM；构造成器把它当**输出**参数写 |
 * | **当前曲名** | `0x56d104` | cmd 2 的 case 把**槽里存的名字**拷进来（`th18:0x477406`：源 = `0x56f384 + slot*0x100`）；而槽里的名字是 `FUN_00454620`（`th18:0x454620`）补过 `.wav` 的 ⇒ 运行期形如 `"th18_03.wav"` |
 * | **关卡表** | `0x4c9410` | 步长 `0xd4`、索引 = 关卡号；`+0x0c` = 道中曲名、`+0x10` = boss 曲名（`th18:0x443121`/`0x443130` 在关卡开始时把它们分别送进 BGM 槽 0/1）|
 * | `CUR_STAGE_DATA` | `0x4cf428` | 指向当前关卡表条目（`imul eax,eax,0xd4; add eax,0x4c9410`，9 处）|
 * | 内存流构造成器 | `0x4898f0` | 13 参 thiscall（见下 §1）|
 * | 起播 | `0x48a440` | `Play(this, 0, 1, 0)`；顺带清 fade 状态并置 `+0x58 = 1` |
 * | 音量 | `0x48a5a0` | `cb = 0` 用户满音量、`cb = -10000` 静音 |
 * | 停止 | `0x48a620` | `param_1 != 0` 会去 `CloseHandle(+0x10 → +0x8c)` ⇒ 内存流**必须传 0** |
 * | 补 buffer | `0x48a730` | fastcall(this)；引擎的补 buffer 线程只认 `0x570484` |
 * | 释放 | 虚表 `+0` | 引擎自己的收尾形状：`MOV EAX,[ECX]; PUSH 1; CALL [EAX]`（`0x477481`）|
 *
 * ### §1 构造成器 `0x4898f0` 的精确签名（★ 逐个 `[EBX+n]` 读出来的，不是猜的）
 *
 * ```
 * 004898f1  MOV EBX,ESP                     ; 参数基址
 * 00489924  MOV EAX,[EBX+0x8]               ; arg1  → 出参（新流对象指针）
 * 0048998a  MOV EAX,[EBX+0xc]  / [EDI+0x80] ; arg2  → PCM 基址（+0x80 与 +0x84 都写它）
 * 0048999a  MOV ECX,[EBX+0x10] / [EDI+0x88] ; arg3  → PCM 长度
 * 0048997f  MOV ECX,[EBX+0x14] / [EDI+0x90] ; arg4  → fmt 记录指针（0x34 B）
 * 00489990  MOVUPS XMM0,[EBX+0x1c]          ; arg6..arg9 → DSBUFFERDESC 的 guid3DAlgorithm
 * 00489934  MOV ESI,[EBX+0x30]              ; arg11 → 分段长度 ⇒ dwBufferBytes = arg11 × 8
 * 00489937  MOVD XMM0,[EBX+0x34]            ; arg12 → CreateEventA 的事件句柄
 * ```
 * arg5 与 arg10 **函数体一次都没读**（引擎自己那处也传的是寄存器残值 / 窗口句柄）⇒ 传 0。
 * 引擎自己的调用点（`th18:0x476e42` 一带，preload 路径）逐参对照：
 * `ctor([0x56ad8c], &[0x570484], 预载PCM, 长度, fmt记录, …, 分段, 事件)`。
 *
 * ### §2 内存流的循环语义（一手 `0x48acc0` 的 preload 分支）
 *
 * ```c
 * if (block->preload_flag(+0x7c) != 0) {          // 我们走这一支（构造成器把它置 1）
 *     block->cur(+0x84)  = block->base(+0x80);    // 当前读指针 = PCM 基址
 *     if (fmt->track_size(+0x1c) > 0) block->size(+0x88) = fmt->track_size;
 *     if (rewind && fmt->intro_size(+0x18) > 0) block->cur = base + intro_size;   // ★ 回卷点
 * }
 * ```
 * ⇒ **`intro_size = 0` + `track_size = PCM 全长` = 整段无缝循环**；
 *   **`intro_size = intro 段字节数` + `track_size = intro + 循环体` = TH13 的 b 轨口径**
 *   （播完一整条回到循环点）。★★ 2026-10-24（方案 A）起我们走**后者**：
 *   素材本身带 intro（生成器按原曲 fmt 的 `intro_size` 切），`trance_bgm_core.c` 的
 *   `trance_bgm_fmt_init(..., intro_bytes, ...)` 把回绕点填成"主曲 intro 按 avg 换算"的值。
 *   以前 `intro_size = 0` + 素材只装循环体 ⇒ 主曲还在 intro 段时只能从循环起点起播
 *   （一手日志实测两路差 **17.1 s**，见 `AUDIT` §CS-4 与 `local/_scratch/bgmcheck/`）。
 *
 * ## 素材：从哪读、读不到怎么办
 *
 * 文件名 = TH13 的规则：把**当前曲名**扩展名前的 `.` 换成 `b`（一手 `th13:0x405b1d`），
 * 再补 `.wav` ⇒ `"th18_03.wav"` → `"th18_03b.wav"`（见 `trance_bgm_core.c` 的
 * `trance_bgm_variant_name()`；TH18 的 `0x56d104` 存的是带扩展名的名字）。
 * 目录 = **DLL 自己所在目录下的 `bgm\`**（首选；`GetModuleHandleExA(FROM_ADDRESS)` +
 * `GetModuleFileNameA` 取自己的路径），读不到再试**游戏 exe 目录下的 `bgm\`**（`ce_log_dir()`）。
 * 两条候选路径**每次失败都会打进日志**（首日排查的第一件事就是看这两条路径对不对）。
 *
 * ★★ **失败一律"不接管"**（本模块最重要的纪律）：找不到文件 / 文件太大 / WAV 不合法 /
 * 构造成器失败 / 起播失败 —— 全都**不动主 BGM 一个字节**（不静音、不换轨、不改音量），
 * 只是本次灵界没有灵界版 BGM。**绝不允许出现"第二路起不来把主 BGM 留成静音"的状态**：
 * 所以主 BGM 的音量只在**淡化**里被动过，而淡化的起点就是"主 BGM 满音量"，
 * 任何一步失败都发生在写音量之前。
 *
 * ## 为什么"补 buffer"要我们自己喂
 *
 * 引擎的补 buffer 线程是 `FUN_004779a0`（preload 路径建的那个），它 `MsgWaitForMultipleObjects`
 * 等的**只有一个事件全局 `0x57048c`**、被唤醒后调的也只有 `FUN_0048a730(0x570484)` ——
 * 即"只服务它自己那一条 BGM"。我们的第二路**它看不见**，所以每一帧自己问一次自己的事件。
 *
 * ## 生命周期（`METHOD.md` 铁律二：跨帧留下的东西必须交代"被删掉会怎样"）
 *
 * 本模块跨帧持有 **① 一个 `CStreamingSound` 对象 ② 一个事件句柄 ③ 一块 VirtualAlloc 的 PCM
 * ④ 主 BGM 的音量**：四者都由 `finish_release()` 一处收掉，而 `ce_trance_bgm_release(1)`
 * （立即收）被 `trance_clear()` 调用 ⇒ 删卡（商店里 tick 全停）也收得掉。
 * ★ 还有一条**只在这里能防**的：引擎自己换曲/重载关卡会把 `0x570484` 指向**新对象**、
 *   旧对象已被 free ⇒ `tick` 每帧比一次指针，变了就立刻撤（绝不去碰可能已释放的指针）。
 *
 * ## 与内存分配的关系（本仓此前的纪律是"这个 DLL 里没有任何 malloc"）
 *
 * 素材 8–15 MiB，**不能**塞进 `.bss`（那等于让每个玩家一开机就 commit 16 MiB，
 * 而灵界一年也开不了几次）。这里用 **`VirtualAlloc`/`VirtualFree`**（不是 CRT 的 malloc
 * ⇒ 不碰 CRT 堆、不进堆锁），只在**起流那一刻**分配、`finish_release()` 里释放。
 * 分配失败 = 走"不接管"那条路，日志写明。
 */
#include <stddef.h>
#include <string.h>
#include <stdio.h>

#include "card_expand.h"
#include "trance_bgm.h"
#include "trance_bgm_core.h"

/* 环 = 8 段（引擎构造成器给的就是 8；见 AUDIT §CV/§CW 与 `engine/_shared` §8.2）*/
#define TRANCE_BGM_RING_SEGS 8

/* ---- 引擎面 ---- */
#define TBGM_ADDR_DSOUND      0x56ad8cu   /* 全局：DirectSound 侧对象（引擎自己 `MOV ECX,[0x56ad8c]`）*/
#define TBGM_ADDR_BGM_PTR     0x570484u   /* 全局：引擎唯一一路 BGM 流对象指针 */
#define TBGM_ADDR_CUR_NAME    0x56d104u   /* 全局：**当前曲名**（内联字符串，形如 "th18_03.wav"）*/
#define TBGM_ADDR_STAGE_DATA  0x4cf428u   /* 全局：CUR_STAGE_DATA（指向关卡表条目）*/
#define TBGM_OFF_STAGE_BGM0   0x0cu       /* 关卡表条目 +0x0c = 道中曲名（`th18:0x443121` 读它）*/
#define TBGM_FN_STREAM_CTOR   0x4898f0u   /* 内存流构造成器（13 参 thiscall，见文件头 §1）*/
#define TBGM_FN_PLAY          0x48a440u   /* CStreamingSound::play（thiscall）*/
#define TBGM_FN_SET_VOLUME    0x48a5a0u   /* SetVolume(this, cb)（thiscall）*/
#define TBGM_FN_STOP          0x48a620u   /* stop/pause（thiscall；param_1 必须 0）*/
#define TBGM_FN_REFILL        0x48a730u   /* 补 buffer（fastcall(this)）*/
#define TBGM_OFF_PLAYING      0x58        /* 流对象的"在播"标志：play 置 1、stop 置 0（一手两个函数各写一次）*/
/* ★★ 2026-09-14 订正（实跑日志 `读不到主曲播放位置（base=00000000 …）`）：这些字段
 *   **不在外层流对象上，而在它的缓冲描述符里** —— 一手 `0x48a730`（补 buffer）：
 *     `0x48a871 mov esi,[ebx+0x10]`   ← esi = 描述符
 *     `0x48a87a cmp dword [esi+0x7c],0`（preload 标志，构造成器写 1）
 *     `0x48a886 mov edx,[esi+0x84]`（cur）/ `0x48a89a [esi+0x80]`（base）/ `0x48a8a3 [esi+0x88]`（size）
 *   而构造成器 `0x4898f0` 的 `[edi+0x80/+0x84/+0x88/+0x90/+0x7c]` 写的就是这个描述符（edi 不是外层对象）。
 *   ⇒ 读/写位置一律走 `*(stream + 0x10)`。上一版读外层对象的 +0x80 ⇒ 恒为 0 ⇒ seek 从未生效
 *     （用户："音频完全对不上…进度完全一样"）。*/
#define TBGM_OFF_DESC         0x10        /* 缓冲描述符指针（位置字段都在它上面）*/
/* ★ 描述符 +0x08 = **剩余字节数**（一手：起播 `0x48ac9e [esi+8] = track_size − pos`；
 *   读盘后 `0x48a8f6 [esi+8] −= 实读字节`；回绕后重置成循环体长度 `0x48ad5e`）
 *   ⇒ **曲内当前位置 = track_size − remaining**（含 intro），但那是**读盘位置**（跑在播放前面）
 *   ⇒ 只当**日志对照**用；seek 的位置来源是墙钟（`s_watch_frames`，见 `bgm_seek_to_engine`）。*/
#define TBGM_OFF_REMAIN       0x08
#define TBGM_OFF_BASE         0x80        /* （描述符上）PCM 基址 */
#define TBGM_OFF_CUR          0x84        /* （描述符上）当前读指针（补 buffer 从这里续）*/
#define TBGM_OFF_SIZE         0x88        /* （描述符上）PCM 长度（= fmt->track_size）*/
#define TBGM_OFF_FMT          0x90        /* （描述符上）fmt 记录（0x34 B）指针 */
#define TBGM_FMT_OFF_INTRO    0x18        /* fmt: intro_size（= 回卷点 = 循环起点）*/
#define TBGM_FMT_OFF_TRACK    0x1c        /* fmt: track_size（读取窗口长度）*/
#define TBGM_FMT_OFF_AVG      0x28        /* fmt: nAvgBytesPerSec（44100/2ch/16bit ⇒ 176400）*/
/* ⚠ 2026-09-14：这里曾经有一条"调 DS 虚表第 13 槽 = SetCurrentPosition"的猜法 —— **它是错的**，
 *   实跑直接崩（转储 `88756`：`info=[8, 0x007cc020]`、`edi` = dsound 虚表）。**已删除，别再猜槽号**：
 *   要动 DS 就得先一手把那只接口的虚表逐槽对出来（含 `QueryInterface` 到底给的是哪只接口）。*/
/* 引擎影像范围（一手：`th18.exe` ImageBase 0x400000、SizeOfImage 0x174000 —— 与崩溃转储的模块表
 * 逐字一致）⇒ 用来判定"这个指针还指在引擎的代码/虚表里吗"（见下面 `bgm_chain_ok()` 的缘由）。*/
#define TBGM_EXE_BASE         0x00400000u
#define TBGM_EXE_END          0x00574000u

/* `GetModuleHandleExA` 的两个 flag：老 SDK 头里可能没有 ⇒ 自己兜一份（值来自 winbase.h）。*/
#ifndef GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT
#define GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT 0x00000002
#endif
#ifndef GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS
#define GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS       0x00000004
#endif

typedef int  (__attribute__((thiscall)) *tbgm_fn_ctor_t)(void *ds, int *out_stream,
                                                         uint32_t pcm, uint32_t pcm_size, uint32_t fmt,
                                                         uint32_t unused5,
                                                         uint32_t g0, uint32_t g1, uint32_t g2, uint32_t g3,
                                                         uint32_t unused10,
                                                         int segment, int ev);
typedef int  (__attribute__((thiscall)) *tbgm_fn_play_t)(void *s, uint32_t p1, uint32_t p2, uint32_t seek);
typedef void (__attribute__((thiscall)) *tbgm_fn_setvol_t)(void *s, int cb);
typedef uint32_t (__attribute__((thiscall)) *tbgm_fn_stop_t)(void *s, int flag);
/* ⚠⚠ 2026-09-14（实跑闪退的**真因**）：`0x48a730` 的收尾是 **`ret 4`** —— 它吃一个**栈上参数**
 *   （函数体一次都没读它）。我们原来按 `fastcall(void*)`（0 个栈参）调 ⇒ **每调一次就少吃 4 字节栈**，
 *   于是后面某次 `ret` 直接跳进垃圾/0：转储 `120072` 的 EIP = 0x3EC7AE14、转储 `59844` 的 EIP = 0
 *   （`ExceptionInformation[0] = 8` = 取指错）。补 buffer 每两秒来一次 ⇒ 表现就是"进灵界几秒后闪退"，
 *   而"关掉 BGM 素材就不崩"也对得上（不起第二路 ⇒ 不走这条）。
 *   引擎自己的调用点（`0x477a1f push ecx` → `0x477a20 mov ecx,[0x570484]` → `call 0x48a730`）
 *   同样证明它吃一个栈参 ⇒ 声明成 thiscall(this, 1 个栈参)，参数给 0（callee 不读）。*/
typedef int  (__attribute__((thiscall)) *tbgm_fn_refill_t)(void *s, int unused);
typedef void (__attribute__((thiscall)) *tbgm_fn_dtor_t)(void *s, int flags);

/* ---- 状态机 ---- */
enum { TBGM_IDLE = 0, TBGM_FADE_IN, TBGM_HOLD, TBGM_FADE_OUT };

static int      s_phase;
static int      s_progress;                 /* 0..TRANCE_BGM_XFADE_FRAMES（两个方向共用）*/
static void    *s_stream;                   /* 第二路 CStreamingSound（NULL = 没有）*/
static HANDLE   s_event;                    /* 我们自己的通知事件（auto-reset）*/
static void    *s_engine_bgm_seen;          /* 起流那一刻的引擎 BGM；每帧比对，变了就撤 */
static void    *s_stream_vtbl;              /* **我们自己的流**的类别指纹 —— 校验时的参照物（已知好对象）*/
static int      s_restart_block;            /* 换曲撤流后的冷却帧数（防"起-撤"每帧打转）*/
static int      s_rearr;                     /* 当前素材是**重编曲**版（`_rearr`）—— 只影响日志措辞，不影响 seek */
static int      s_probe_left;                /* 起播期探针剩余次数（每次补 buffer 前后各一行）*/
static int      s_noseek;                    /* `mode.txt` 里写了 noseek ⇒ 不做同时间点 seek（应急开关）*/
static void    *s_watch_obj;                 /* 观察到的引擎 BGM 流对象（与"接管"那个分开）*/
static uint32_t s_watch_frames;              /* 自它换新以来的帧数 ≈ 这首曲子播了多久（暂停不推进）*/
static int      s_paused_ours;              /* 镜像引擎暂停：我们把第二路停了 */
static uint32_t s_seek_pos;                  /* 起播位置（PCM 内字节）= 同时间点 seek 的结果；0 = 不 seek */
static uint32_t s_ring_bytes = 176400u * 8u; /* 环 = 8 段 × 2 s（22050/2ch/16bit ⇒ 176400 B/2s）*/
static int      s_selfcheck = -1;           /* -1 = 还没跑 */

/* 素材（真音频）：PCM 由 VirtualAlloc 拿，`s_wav` 指向它内部 ⇒ 生命周期与 `s_stream` 绑在一起 */
static uint8_t  *s_pcm;
static uint32_t  s_pcm_bytes;
static trance_bgm_wav_t s_wav;
static char      s_dll_dir[MAX_PATH];       /* **DLL 自己所在目录**（尾带反斜杠），解析一次后复用 */
static int       s_asset_dir_state;         /* 0 = 没试过，1 = 成功，-1 = 失败 */
static char      s_variant[40];             /* 本次用的变体名（日志）*/
static char      s_last_fail[40];           /* 上一次失败的变体名 ⇒ 同一种失败只详报一次 */
static trance_bgm_fmt_t s_fmt;

static int32_t  s_stat_start, s_stat_release, s_stat_refill, s_stat_fail, s_stat_pause, s_stat_swap;
static int32_t  s_stat_nofile, s_stat_badwav, s_stat_toobig, s_stat_alloc, s_stat_noname;
static int32_t  s_stat_engine_lost;         /* 引擎那路被释放/换掉、我们主动撒手的次数 */

/* ------------------------------------------------------------------ 小工具 */

static void *ds_object(void)      { return *(void **)(uintptr_t)TBGM_ADDR_DSOUND; }
static void *engine_bgm(void)     { return *(void **)(uintptr_t)TBGM_ADDR_BGM_PTR; }

static int engine_bgm_playing(void *bgm)
{
    return bgm && *(int32_t *)((uint8_t *)bgm + TBGM_OFF_PLAYING) != 0;
}

/* ★★ 2026-09-15（用户："配对没问题，还是时间对不上"）：`play(this, p1, p2, seek)` 的**第三个参数
 *   就是"起播位置"**（PCM 内字节偏移）——一手：`0x48a440` 内 `0x48a22c push dword [ebp+0x10]`
 *   → `ecx = [stream+0x10]`（描述符）→ `push 0` → `call 0x48acc0`，即 `reset(desc, rewind=0, pos)`，
 *   而 `reset` 把 `cur = base + pos`（预载支还会**按这个位置按顺序**把 8 段环填好）。
 *   ⇒ 用它起播，比"自己戳描述符 `+0x84`"更贴引擎语义（引擎自己也是这么调的：`0x4773aa`
 *   `push 0/push 1/push 0/call 0x48a440`），而且**连"环里还是上一段数据"的隐患一起解决**。*/
static void bgm_play_at(void *s, uint32_t pos)
{
    ((tbgm_fn_play_t)(uintptr_t)TBGM_FN_PLAY)(s, 0, 1, pos);
}
static void bgm_set_volume(void *s, int cb) { ((tbgm_fn_setvol_t)(uintptr_t)TBGM_FN_SET_VOLUME)(s, cb); }
/* ⚠ `param_1` 恒为 0：传 1 时会走 `CloseHandle(block+0x8c)`，而内存流的那个字段是**未初始化**的
 *   （构造成器只写 +0x80/+0x84/+0x88/+0x90/+0x7c/+0x2c/+0，见文件头 §1）⇒ 等于关一个野句柄。*/
/* ⚠ 暂停路径 2026-09-15 改成"释放 + 重起"之后，`stop` 暂时**没有调用点**了 —— 但保留它：
 *   ① `tbgm_fn_stop_t` 的签名仍要被 `native/tools/engine_abi_check.py` 对账（`ret 4` ↔ 2 参）；
 *   ② 它的两处一手（`SetCurrentPosition(0)`、参数非 0 时把文件指针倒回 `[desc+0x8c]`）
 *      是上面暂停段落那些结论的出处，留着便于下次直接复用。*/
static void bgm_stop(void *s) __attribute__((unused));
static void bgm_stop(void *s)        { (void)((tbgm_fn_stop_t)(uintptr_t)TBGM_FN_STOP)(s, 0); }
static int  bgm_refill(void *s)      { return ((tbgm_fn_refill_t)(uintptr_t)TBGM_FN_REFILL)(s, 0); }
/* 引擎自己的释放形状（`0x477481`：`MOV EAX,[ECX]; PUSH 1; CALL [EAX]`）= deleting destructor
 * （虚表第 0 项、栈参 1 = "连对象一起释放"）。*/
static void bgm_destroy(void *s)
{
    void **vtbl = *(void ***)s;
    ((tbgm_fn_dtor_t)(uintptr_t)vtbl[0])(s, 1);
}

/* 从固定地址读一个 NUL 终止的短名字（最多 cap-1 个字符；找不到 NUL / 空串 ⇒ -1）。
 * ★ 一定要有上界：这两个全局都不是"我们的"内存，写错一处就是读越界。*/
static int read_fixed_name(uintptr_t addr, char *out, size_t cap)
{
    const char *p = (const char *)addr;
    size_t i;

    for (i = 0; i + 1 < cap; ++i) {
        char c = p[i];
        if (!c) { out[i] = 0; return i ? 0 : -1; }
        out[i] = c;
    }
    out[cap - 1] = 0;
    return -1;
}

/* 当前在放的是哪条曲 —— 返回 0 = 用了引擎的"当前曲名"，1 = 走了关卡表兜底，-1 = 都没拿到。 */
static int cur_track_name(char *out, size_t cap)
{
    /* 首选 `0x56d104`：一手（`th18:0x477406`）是 cmd 2 的 case 把**槽里存的名字**
     * （`0x56f384 + slot*0x100`）拷进来的，而那份名字由 `FUN_00454620` 补过 `.wav`
     * ⇒ 运行期形如 `"th18_03.wav"`。*/
    if (read_fixed_name(TBGM_ADDR_CUR_NAME, out, cap) == 0 && strncmp(out, "th18", 4) == 0)
        return 0;

    /* 兜底：关卡表 +0x0c 的道中曲名（一手：`th18:0x443121` 就是拿它去 cmd 1 的）。
     * 什么时候用得上：`0x56d104` 是空的、或是 `"dummy"`（引擎在菜单/结算那几条路会用占位名，
     * 一手 `0x4546a0` 就是 `push "dummy"`）。*/
    {
        void *stage = *(void **)(uintptr_t)TBGM_ADDR_STAGE_DATA;
        if (stage) {
            const char *p = *(const char *const *)((const uint8_t *)stage + TBGM_OFF_STAGE_BGM0);
            if (p && read_fixed_name((uintptr_t)p, out, cap) == 0) return 1;
        }
    }
    return -1;
}

/* DLL 自己所在目录（尾带 '\\'）。0 = ok。只解析一次。*/
static int asset_dir(void)
{
    char buf[MAX_PATH];
    HMODULE hm = NULL;
    const void *anchor = (const void *)&s_dll_dir;     /* 本模块内的任意一个地址 */

    if (s_asset_dir_state) return s_asset_dir_state > 0 ? 0 : -1;
    s_asset_dir_state = -1;
    if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                            GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            (LPCSTR)anchor, &hm) || !hm) {
        ce_log("trance-bgm: GetModuleHandleExA failed (%lu) - 素材目录只能用 exe 目录那条候选",
               (unsigned long)GetLastError());
        return -1;
    }
    if (!GetModuleFileNameA(hm, buf, sizeof buf)) return -1;
    {
        char *slash = strrchr(buf, '\\');
        if (!slash) return -1;
        *(slash + 1) = 0;
    }
    snprintf(s_dll_dir, sizeof s_dll_dir, "%s", buf);   /* 已带尾部 '\\' */
    s_asset_dir_state = 1;
    return 0;
}

/* 素材目录的**三条**候选（返回条数；`dirs[i][0] == 0` 的跳过）。
 *
 * | # | 目录 | 什么时候命中 |
 * | --- | --- | --- |
 * | ① | `<DLL 自己所在目录>\bgm\` | DLL 被加载的那一份在哪，素材就跟到哪 —— **首选** |
 * | ② | `<DLL 自己所在目录>\..\..\mods\bgm\` | modkit 布局：DLL 在 `<modkit>\thcrap\bin\` ⇒ 退两级 = `<modkit>` ⇒ 这就是 `<modkit>\mods\bgm\` |
 * | ③ | `<游戏 exe 目录>\bgm\` | `ce_log_dir()` 给的目录（`<modkit>\game\`）|
 *
 * ★★ **为什么要三条（2026-09-14 实跑订正）**：首日实跑日志里反复出现
 * `trance-bgm: 素材不在。找过：…\thcrap\bin\bgm\… / …\game\bgm\…` —— 因为
 * **真正被加载的是 `<modkit>\thcrap\bin\mod_th18_card_expand.dll`**（启动器在"开始游戏那一刻"
 * 从 `mods/` 复制过去的那一份），而当时的部署说明只写了"拷到 `<modkit>\mods\bgm\`"。
 * 现在三条都试：**任何一条命中都能出声**；三条都不中时**完整路径全部打进日志**（这才是首日的第一件事）。*/
#define TBGM_MAX_ASSET_DIRS 3U

static int asset_dirs(char dirs[TBGM_MAX_ASSET_DIRS][MAX_PATH])
{
    char exe_dir[MAX_PATH];
    int  n = 0;
    unsigned i;

    for (i = 0; i < TBGM_MAX_ASSET_DIRS; ++i) dirs[i][0] = 0;

    if (asset_dir() == 0 && s_dll_dir[0]) {
        snprintf(dirs[n++], MAX_PATH, "%s%s\\", s_dll_dir, TRANCE_BGM_ASSET_SUBDIR);          /* ① */
        snprintf(dirs[n++], MAX_PATH, "%s..\\..\\mods\\%s\\",                            /* ② */
                 s_dll_dir, TRANCE_BGM_ASSET_SUBDIR);
    }
    exe_dir[0] = 0;
    ce_log_dir(exe_dir, sizeof exe_dir);                       /* exe 目录 + '\\' */
    if (exe_dir[0] && strlen(exe_dir) + strlen(TRANCE_BGM_ASSET_SUBDIR) + 2 < sizeof exe_dir)
        snprintf(dirs[n++], MAX_PATH, "%s%s\\", exe_dir, TRANCE_BGM_ASSET_SUBDIR);            /* ③ */
    return n;
}

/* ★★ 2026-09-14（用户口径）：**素材模式**由 `<bgm>\mode.txt` 的一行决定 —— 目的是"不用等我重新
 *   发布就能 A/B"：
 *     music（默认，文件不存在/读不到）= 现在的加强低音版 `<曲名>b.wav`
 *     se                             = 把 TH13 的 `se_pldead00.wav` 粗暴循环当灵界音频（`<曲名>b_se.wav`）
 *     both                           = 音乐 + 那个音效一起（`<曲名>b_both.wav`）
 *     rearr                          = **重编曲版**（`<曲名>b_rearr.wav`，`tools/make_trance_rearr.py`
 *                                      用同人 MIDI 重配器：主旋律压低、伴奏抬高 ⇒ 听感是"另一首曲子"）
 *   变体文件由 `tools/make_trance_marker.py` 生成（与主素材同目录、同采样率）。*/
static void read_material_mode(char *out, size_t cap)
{
    char dirs[TBGM_MAX_ASSET_DIRS][MAX_PATH];
    int  n = asset_dirs(dirs), i;
    snprintf(out, cap, "music");
    for (i = 0; i < n; ++i) {
        char path[MAX_PATH], buf[64];
        DWORD got = 0;
        HANDLE h;
        snprintf(path, sizeof path, "%smode.txt", dirs[i]);
        h = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (h == INVALID_HANDLE_VALUE) continue;
        if (ReadFile(h, buf, (DWORD)(sizeof buf - 1), &got, NULL) && got) {
            char *p;
            buf[got] = 0;
            for (p = buf; *p && *p != '\r' && *p != '\n' && *p != ' ' && *p != '\t'; ++p) {
                if (*p >= 'A' && *p <= 'Z') *p = (char)(*p - 'A' + 'a');
            }
            *p = 0;
            if (strstr(buf, "noseek")) s_noseek = 1;
            else s_noseek = 0;
            s_rearr = 0;                              /* 由 apply_material_mode 按模式重新置位 */
            if (buf[0]) snprintf(out, cap, "%s", buf);
        }
        CloseHandle(h);
        ce_log("trance-bgm: 素材模式取自 \"%s\" ⇒ %s", path, out);
        return;
    }
}

/* 把 `_se` / `_both` 插进 `<曲名>b.wav` 的扩展名之前（`mode.txt` 的开关就用它落地）。*/
static void apply_material_mode(char *fname, size_t cap, const char *mode)
{
    const char *suffix = NULL;
    char buf[64];
    char *dot;
    if (mode[0] == 0 || strcmp(mode, "music") == 0) return;
    if (strcmp(mode, "se") == 0)           suffix = "_se";
    else if (strcmp(mode, "both") == 0)    suffix = "_both";
    else if (strcmp(mode, "rearr") == 0) { suffix = "_rearr"; s_rearr = 1; }
    else { ce_log("trance-bgm: mode.txt 里是 \"%s\"（只认 music/se/both）⇒ 按 music 走", mode); return; }
    dot = strrchr(fname, '.');
    if (!dot) return;
    snprintf(buf, sizeof buf, "%.*s%s%s", (int)(dot - fname), fname, suffix, dot);
    snprintf(fname, cap, "%s", buf);
}

/* 读一条 WAV 进内存 + 解析。返回 0 = ok（`*out_pcm` 要调 VirtualFree），否则负的拒绝码。*/
static int load_variant(const char *fname, uint8_t **out_alloc, trance_bgm_wav_t *out_wav,
                        char *used_path, size_t used_cap)
{
    char   path[MAX_PATH * 2];
    char   dirs[TBGM_MAX_ASSET_DIRS][MAX_PATH];
    HANDLE h = INVALID_HANDLE_VALUE;
    DWORD  sz, got = 0;
    uint8_t *buf;
    int    rc, ndirs, i;

    if (used_path && used_cap) used_path[0] = 0;
    ndirs = asset_dirs(dirs);

    for (i = 0; i < ndirs; ++i) {
        if (!dirs[i][0]) continue;
        snprintf(path, sizeof path, "%s%s", dirs[i], fname);
        h = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
                        FILE_ATTRIBUTE_NORMAL, NULL);
        if (h != INVALID_HANDLE_VALUE) {
            if (used_path && used_cap) snprintf(used_path, used_cap, "%s", path);
            break;
        }
    }
    if (h == INVALID_HANDLE_VALUE) {
        char list[MAX_PATH * TBGM_MAX_ASSET_DIRS + 64];
        size_t used = 0;
        list[0] = 0;
        for (i = 0; i < ndirs && used + MAX_PATH + 8 < sizeof list; ++i) {
            int w = snprintf(list + used, sizeof list - used, "%s    %s%s\n", i ? "" : "\n", dirs[i], fname);
            if (w > 0) used += (size_t)w;
        }
        ce_log("trance-bgm: 素材不在（%d 条候选全试过）：%s"
               "  （生成 = tools/make_trance_bgm.py；部署 = 三处之一，见 AUDIT §CO-6）", ndirs, list);
        return -1;
    }

    sz = GetFileSize(h, NULL);
    if (sz == INVALID_FILE_SIZE && GetLastError() != NO_ERROR) {
        CloseHandle(h);
        return -2;
    }
    if (sz == 0 || sz > TRANCE_BGM_MAX_PCM) {
        ce_log("trance-bgm: 素材大小 %lu B 越界（0 < size <= %u）", (unsigned long)sz,
               (unsigned)TRANCE_BGM_MAX_PCM);
        CloseHandle(h);
        return -3;
    }
    buf = (uint8_t *)VirtualAlloc(NULL, sz, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!buf) {
        ce_log("trance-bgm: VirtualAlloc(%lu) 失败 (%lu)", (unsigned long)sz,
               (unsigned long)GetLastError());
        CloseHandle(h);
        return -4;
    }
    if (!ReadFile(h, buf, sz, &got, NULL) || got != sz) {
        ce_log("trance-bgm: ReadFile 只读到 %lu / %lu B", (unsigned long)got, (unsigned long)sz);
        VirtualFree(buf, 0, MEM_RELEASE);
        CloseHandle(h);
        return -5;
    }
    CloseHandle(h);

    rc = trance_bgm_wav_parse(buf, (uint32_t)sz, out_wav);
    if (rc != 0) {
        ce_log("trance-bgm: WAV 解析失败 wav=%d（-1 空指针 / -3 非 RIFF / -4 块越界 / "
               "-5..-6 fmt 块 / -7 data 块 / -8 非 PCM 或非 16bit）", rc);
        VirtualFree(buf, 0, MEM_RELEASE);
        return -6;
    }
    *out_alloc = buf;          /* ★ 要 free 的是**分配到的基址**；引擎要用的是 out_wav->pcm（载荷）*/
    return 0;
}

/* ------------------------------------------------------------------ 对象可用性校验（★ 2026-09-14 实跑闪退定位） */

/* `0x48a5a0` 不是"写个音量就完事"：它的形状是 `eax = [this+8]` → `ecx = [eax]`（内层对象）
 * → `edx = [ecx]`（虚表）→ `call [edx+0x3c]`（虚调用）。
 * 引擎换曲（道中 → boss）时**先释放旧流、几帧之后才把新指针写回 `0x570484`** ⇒
 * 「指针变了就撤」这条防御在那几帧里看不见任何变化，于是我们把 -10000 写进**已释放的对象**，
 * 崩在 `call [edx+0x3c]` 上：EIP = 垃圾（转储 `120072` 的 0x3EC7AE14）或 0
 * （转储 `59844` 的 `ExceptionInformation[0] = 8` = **取指**错误）。
 * ⇒ 每次用之前把这条链**逐级验一遍**（都必须在引擎影像里），再对一次"类"（起流时记的虚表）。
 *    验不过就当"引擎那路没了"：**不再碰它**，也不去恢复音量（对象已经不在了）。*/
static int tbgm_in_exe(uint32_t p) { return p >= TBGM_EXE_BASE && p < TBGM_EXE_END; }

/* 链条三级取值（一手 `0x48a5b3`/`0x48a5c6`/`0x48a5d0`）：
 *   p1 = *(this+8) → p2 = *p1 → vtbl = *p2 → 调 `[vtbl+0x3c]`（★ 那是**内层对象的虚表**，
 *   实测落在 **dsound.dll** 里 = `IDirectSoundBuffer8::SetVolume` —— 一代手：日志里
 *   `vtbl=0x59541568`、而转储的模块表里 `dsound.dll` 基址正是 `0x59540000`）。
 *   ⚠ 2026-09-14 两个错都出在这里：第一版少一跳；第二版又要求 vtbl 落在 **th18.exe 影像内**
 *   ⇒ 把合法对象全判死，`s_engine_bgm_seen` 永远为 NULL ⇒ tick 把"没接管"当成"引擎变了"
 *   ⇒ **每帧起一次又撤一次**（日志 `started 440 / released 440 / swap 440`，用户听不到第二路）。
 *   ⇒ 现在改成**与"我们自己的流"同类**判据（自己的流是已知好对象，拿它当参照物）。*/
static uint32_t chain_p1(void *s)   { return s ? *(uint32_t *)((uint8_t *)s + 8) : 0; }
static uint32_t chain_p2(void *s)   { uint32_t a = chain_p1(s); return a ? *(uint32_t *)a : 0; }
static uint32_t chain_vtbl(void *s) { uint32_t b = chain_p2(s); return b ? *(uint32_t *)b : 0; }
static uint32_t chain_slot(void *s) { uint32_t v = chain_vtbl(s); return v ? *(uint32_t *)(v + 0x3c) : 0; }

/* 结构上还像不像一个流对象：三级都非 0、要调的槽也非 0。 */
static int bgm_chain_ok(void *s)
{
    return s && chain_p2(s) && chain_vtbl(s) && chain_slot(s);
}

/* 引擎那路能不能用：链要通，而且**类要与我们自己的流一致**（同一个构造成器造出来的 ⇒
 * 同一个 COM 虚表）。参照物 `s_stream_vtbl` 取自**已知好对象**（我们自己的流），
 * **不假设虚表落在哪个模块里** —— 实测它在 dsound.dll（`0x59541568`，见上面注释）。
 * 兜底：vtbl 恰好在引擎影像内（其它实现形态）也认。*/
static int bgm_obj_usable(void *s)
{
    uint32_t v;
    if (!bgm_chain_ok(s)) return 0;
    v = chain_vtbl(s);
    if (s_stream_vtbl && v == (uint32_t)(uintptr_t)s_stream_vtbl) return 1;
    return tbgm_in_exe(v);
}

/* 引擎那路没了 ⇒ 丢弃缓存（之后不再写它，也不恢复音量）。只在状态真的变化时打一行。*/
static void drop_engine_bgm(const char *why)
{
    if (!s_engine_bgm_seen) return;
    ++s_stat_engine_lost;
    ce_log("trance-bgm: 引擎那路 BGM 已经不在了（%s）⇒ 丢弃缓存 %p，不再写它的音量"
           "（对象已释放/被换，恢复音量这一步也跳过）", why, s_engine_bgm_seen);
    s_engine_bgm_seen = NULL;
}

/* ------------------------------------------------------------------ 与主 BGM 同步（TH13「同时间点 seek」的复刻） */

/* ★ 2026-09-14（用户："暂停正常了，但似乎时序还是不对"）：光看日志里的"seek 目标"不足以判断
 *   —— 得看**游标到底有没有被动过、第一段数据是从哪开始的**。所以起播期打三段/前几次补 buffer
 *   各打一行：`cur`（描述符 +0x84）、`base`（+0x80）、`size`（+0x88）、`remaining`（+0x08）。
 *   判读：`cur − base` 应当 ≈ 我们算出的 `so`（WAV 字节）；若它恒 0（或先对后被改回 0）
 *   ⇒ 有东西在起播路径里重设了游标（那 seek 就得挪到它之后）。*/
static uint32_t rdu32(const void *p, uint32_t off);      /* 定义在下面（探针先用到）*/

static void bgm_probe(const char *tag, void *stream)
{
    uint32_t desc, base, cur, size, remain;
    if (!stream) { ce_log("trance-bgm: [%s] stream=NULL", tag); return; }
    desc   = rdu32(stream, TBGM_OFF_DESC);
    base   = desc ? rdu32((const void *)(uintptr_t)desc, TBGM_OFF_BASE)   : 0;
    cur    = desc ? rdu32((const void *)(uintptr_t)desc, TBGM_OFF_CUR)    : 0;
    size   = desc ? rdu32((const void *)(uintptr_t)desc, TBGM_OFF_SIZE)   : 0;
    remain = desc ? rdu32((const void *)(uintptr_t)desc, TBGM_OFF_REMAIN) : 0;
    ce_log("trance-bgm: [%s] desc=%p base=%p cur=%p size=%u remaining=%u ⇒ cur-base=%u B (%.2f s)",
           tag, (void *)(uintptr_t)desc, (void *)(uintptr_t)base, (void *)(uintptr_t)cur,
           size, remain, (cur >= base) ? (cur - base) : 0xffffffffu,
           (double)((cur >= base) ? (cur - base) : 0) / 88200.0);
}

static uint32_t rdu32(const void *p, uint32_t off) { return *(const uint32_t *)((const uint8_t *)p + off); }
static void     wru32(void *p, uint32_t off, uint32_t v) { *(uint32_t *)((uint8_t *)p + off) = v; }

/* 读**主曲 fmt 的 intro**（一手 `th18:0x48acc0` 的 preload 分支用的那个字段），
 * 按 avg 比例换算成**我们素材的 intro 字节数** → 循环点；读不到/不自洽 ⇒ 0。
 *
 * 为什么要换算：主曲 fmt 是 44100/2ch/16bit（avg 176400），我们素材是 22050/2ch/16bit（avg 88200）
 * ⇒ `intro_ours = intro_engine × our_avg ÷ e_avg`（本批 = 恰好 ÷2）。
 * ★ 这是**一手值**：生成器切的 intro 段用的就是同一个 `thbgm.fmt` 的 `intro_size`，
 *   所以两边算出来的循环点逐采样一致（差 ≤ 1 帧 = 0.045 ms）。*/
static uint32_t engine_intro_bytes(void *engine, uint32_t our_avg, uint32_t our_block)
{
    uint32_t edesc, efmt, eintro, etrack, eavg, intro;

    if (!engine || !our_avg) return 0;
    edesc = rdu32(engine, TBGM_OFF_DESC);
    if (edesc < 0x10000u || edesc > 0x7fffffffu) return 0;
    efmt = rdu32((const void *)(uintptr_t)edesc, TBGM_OFF_FMT);
    if (efmt < 0x10000u || efmt > 0x7fffffffu) return 0;
    eintro = rdu32((const void *)(uintptr_t)efmt, TBGM_FMT_OFF_INTRO);
    etrack = rdu32((const void *)(uintptr_t)efmt, TBGM_FMT_OFF_TRACK);
    eavg   = rdu32((const void *)(uintptr_t)efmt, TBGM_FMT_OFF_AVG);
    /* 与 `trance_bgm_wav_parse` 同一条一致性门：intro/track/avg 必须自洽（否则不猜）*/
    if (!eintro || etrack <= eintro || etrack > (64u << 20) || eavg < 8000u || eavg > 1000000u)
        return 0;
    intro = (uint32_t)((uint64_t)eintro * our_avg / eavg);
    if (our_block) intro -= intro % our_block;        /* 对齐整帧（半帧会让回绕点串道）*/
    return intro;
}

/* 把第二路 seek 到"主曲此刻对应的位置"。
 *
 * 一手依据：TH13 `FUN_00476ea0` 在换轨**之前**把新轨 seek 到与旧轨**相同的播放时刻**，
 * 而 `th13_NN` / `th13_NNb` 的 intro/loop 时长**逐一严格相等** —— "硬切接得上"靠的是这两条，
 * 不是交叉淡化（见 `engine/_shared/th13-th18-trance-bgm-research.md` §1.2/§1.5）。
 *
 * ★★ 2026-10-24（方案 A，对齐 TH13）：素材**带 intro** 了 ⇒ 我们的时间轴与主曲**同构**
 *    （都是 `[intro][循环体]`，intro 与循环体的秒数逐个相等）⇒ 映射就是**绝对位置**：
 *
 *        S_ours = S_engine × (our_avg ÷ e_avg)        （采样率不同，按**一手 fmt 的 avg** 换算）
 *        S_ours ≥ intro_ours 时按循环体卷回：intro_ours + (S_ours − intro_ours) % (pcm − intro_ours)
 *
 *    旧写法（已删除）是 `S_loop = max(S_engine − intro_engine, 0)` + 乘 `our_pcm/(track−intro)`
 *    —— 那是"素材只有循环体"年代的写法：主曲还在 intro 段时**被 clamp 成 0**（从循环起点起播），
 *    一手日志实测两路差 **17.1 s**（主曲 7.50 s vs 我们 24.61 s 的循环起点）。
 *
 * 落地一条（`play` 之后）：把补 buffer 游标 `cur` 挪到 `base + S_ours`，再整环重填 8 段。
 * ⚠ **不要碰 DS 虚表**：2026-09-14 那次闪退就是猜 vtable 槽号猜错了（见文件头 §2 与 AUDIT §CR）。*/
static uint32_t bgm_seek_to_engine(void *engine, void *ours, uint32_t our_pcm,
                                   uint32_t our_avg, uint32_t our_block)
{
    uint32_t edesc, ebase, ecur, efmt, eintro = 0u, etrack = 0u, eavg = 176400u, eremain;
    uint32_t pos_abs = 0u, pos_read = 0u, so, intros = 0u, odesc, obase, osize;
    int      clock_ok, have_pos = 0, have_fmt = 0;
    const char *pos_src = "描述符";

    if (!engine || !ours || !our_pcm || !our_avg) return 0;
    edesc = rdu32(engine, TBGM_OFF_DESC);
    ebase = edesc ? rdu32((const void *)(uintptr_t)edesc, TBGM_OFF_BASE) : 0;
    ecur  = edesc ? rdu32((const void *)(uintptr_t)edesc, TBGM_OFF_CUR)  : 0;
    efmt  = edesc ? rdu32((const void *)(uintptr_t)edesc, TBGM_OFF_FMT)  : 0;

    /* ① 主曲 fmt：intro（= 循环点）/ track / avg 三个数都要（一手值，不猜）。*/
    if (edesc >= 0x10000u && edesc <= 0x7fffffffu &&
        efmt  >= 0x10000u && efmt  <= 0x7fffffffu) {
        uint32_t i = rdu32((const void *)(uintptr_t)efmt, TBGM_FMT_OFF_INTRO);
        uint32_t t = rdu32((const void *)(uintptr_t)efmt, TBGM_FMT_OFF_TRACK);
        uint32_t a = rdu32((const void *)(uintptr_t)efmt, TBGM_FMT_OFF_AVG);
        if (t > i && t <= (64u << 20) && a >= 8000u && a <= 1000000u) {
            eintro = i; etrack = t; eavg = a; have_fmt = 1;
            eremain = rdu32((const void *)(uintptr_t)edesc, TBGM_OFF_REMAIN);
            if (eremain <= etrack) pos_read = etrack - eremain;   /* 读盘位置（**只作对照**）*/
        }
    }

    /* ② 位置来源：**墙钟**优先（≈ 耳朵听到的位置，含 intro）。
     *    引擎是"读盘跑在播放前面"的 —— `remaining` 减的是**读盘位置**，拿它 seek 会偏后
     *    一个已缓冲量（最多十几秒）⇒ 只在拿不到帧时钟时才退回描述符。*/
    clock_ok = (s_watch_obj != NULL && s_watch_frames > 0);
    if (clock_ok) {
        pos_abs = (uint32_t)((uint64_t)s_watch_frames * eavg / 60u);   /* 秒 × 176400 */
        pos_src = "帧时钟(≈听感位置)";
        have_pos = 1;
    } else if (ebase && ecur >= ebase) {
        pos_abs = ecur - ebase;
        have_pos = 1;
    }
    if (!have_pos) {
        ce_log("trance-bgm: 读不到主曲播放位置（desc=%p base=%p cur=%p，帧时钟 %u 帧）⇒ 不 seek",
               (void *)(uintptr_t)edesc, (void *)(uintptr_t)ebase, (void *)(uintptr_t)ecur,
               (unsigned)s_watch_frames);
        return 0;
    }
    if (!have_fmt) {
        ce_log("trance-bgm: 拿不到/读不通主曲 fmt 记录（desc=%p fmt=%p）⇒ 不 seek",
               (void *)(uintptr_t)edesc, (void *)(uintptr_t)efmt);
        return 0;
    }

    /* ③ **绝对位置**（★ 2026-10-24：不再减 intro）：
     *    22050/2ch/16bit ⇒ 每帧 60 分之一秒 = 88200/60 = **1470 B**（7.50 s ⇒ 661500 B）。
     *    公式写成 `pos_abs × our_avg ÷ eavg` = "按 fmt 的 avg 换算"，与本批的 `帧 × 1470` 同一个数。*/
    so = (uint32_t)((uint64_t)pos_abs * our_avg / eavg);

    /* ④ 循环点 / 卷回：intro_ours = 主曲 intro × avg 比例（与生成器切的 intro 段逐采样一致）。
     *    主曲播超过一整条（intro + 循环体）后引擎自己回到 intro ⇒ 我们按同一条规则卷回
     *    （否则墙钟一直涨，位置会跑到循环体之外）。*/
    intros = engine_intro_bytes(engine, our_avg, our_block);
    if (intros >= our_pcm) intros = 0u;               /* 素材比主曲 intro 还短 ⇒ 不设循环点 */
    if (intros) {
        if (so >= intros) so = intros + (so - intros) % (our_pcm - intros);
        /* so < intros ⇒ 主曲还在 intro 段 ⇒ 我们也在 intro 段的同一时刻（原样）*/
    } else {
        so %= our_pcm;
    }

    /* ⑤ 循环点写回**我们自己的 fmt**：描述符 `+0x90` 存的是**指针**（一手），
     *    ctor 之后改这个字段照样生效 ⇒ 引擎下次"播完一整条"时 `cur = base + intro_size`
     *    落回原曲的循环点（而不是回到文件开头的 intro）。
     *    读不到一手 intro ⇒ 0 = 老口径（整段循环）。*/
    s_fmt.intro_size = (int32_t)intros;

    odesc = rdu32(ours, TBGM_OFF_DESC);
    if (!odesc) { ce_log("trance-bgm: 拿不到第二路的缓冲描述符 ⇒ 不做 seek"); return 0; }
    obase = rdu32((const void *)(uintptr_t)odesc, TBGM_OFF_BASE);
    osize = rdu32((const void *)(uintptr_t)odesc, TBGM_OFF_SIZE);
    /* 写之前的三条一致性校验（任何一条不过就只记一笔、绝不写）：
     *   ① 基址像个指针；② 窗口长度 == 我们素材的 PCM 全长（构造成器从 fmt->track_size 写进去的）；
     *   ③ 目标偏移落在 [0, PCM 全长] 内。*/
    if (!obase || obase < 0x10000u || obase > 0x7fffffffu || osize != our_pcm || so > our_pcm) {
        ce_log("trance-bgm: seek 前校验没过（desc=%p base=%p size=%u 期望 %u，so=%u）⇒ 不写游标",
               (void *)(uintptr_t)odesc, (void *)(uintptr_t)obase, osize, our_pcm, so);
        return 0;
    }
    wru32((void *)(uintptr_t)odesc, TBGM_OFF_CUR, obase + so);
    /* ★ 日志必须打**真实位置**（`pos_abs`），不能打 `se + eintro` ——
     *   旧写法在"减 intro 再 clamp"之后恒等于 intro 长度，把真实的 7.50 s 藏成了 24.61 s，
     *   正是这次查因被耽误最久的地方。*/
    /* ★ 判据行（A3-4）：`两路差` = 第二路游标 − 主曲**此刻的逻辑位置**，按设计应当 ≈ 0。
     *   逻辑位置 = 墙钟位置按主曲自己的循环规则卷回（`>= track` ⇒ `intro + (pos−intro) % (track−intro)`）：
     *   主曲播过一整条之后墙钟一直在涨，直接拿墙钟秒数比会打出一个假的 −90 s。
     *   进灵界时主曲还在 intro 段 ⇒ 这一行**不该再出现 clamp 过的 "0.00 s"**；
     *   两路差 ≤ 50 ms 就算对上了（chroma hop 20 ms + 60 fps 帧量子 16.7 ms）。*/
    ce_log("trance-bgm: 同时间点 seek（位置来源：%s）：主曲 %.2f s（含 intro %.2f s）"
           "⇒ 第二路游标 %.2f s / %.2f s；两路差 = %+.0f ms；"
           "对照：墙钟 %.2f s / 读盘位置 %.2f s（差 = 已缓冲量，环 %u B）",
           pos_src, pos_abs / (double)eavg, eintro / (double)eavg,
           so / (double)our_avg, our_pcm / (double)our_avg,
           ((double)so / (double)our_avg
            - (double)((pos_abs >= etrack)
                       ? (eintro + (pos_abs - eintro) % (etrack - eintro))
                       : pos_abs) / (double)eavg) * 1000.0,
           (double)s_watch_frames / 60.0, pos_read / (double)eavg, (unsigned)s_ring_bytes);
    ce_log("trance-bgm: 循环点 = 素材 %u B = %.3f s（主曲 fmt intro %u B = %.3f s，按 avg %u→%u 换算；%s）",
           (unsigned)intros, intros / (double)our_avg,
           (unsigned)eintro, eintro / (double)eavg, (unsigned)eavg, (unsigned)our_avg,
           intros ? "素材带 intro" : "无一手 intro ⇒ 整段循环");
    ce_log("trance-bgm: 起播位置 = %#x（素材%s）", (unsigned)so, s_rearr ? "重编曲" : "本体/其他");
    return so;
}

/* ------------------------------------------------------------------ 音量 */

/* 把"当前淡化位置"写进两路音量。**每帧都写**：引擎自己的单流 fade 状态机（`0x453460`，菜单在用）
 * 与音量命令（cmd）都可能把主 BGM 的音量盖回去。 */
static void apply_mix(void)
{
    int32_t main_cb = 0, second_cb = 0;

    trance_bgm_xfade_mix(s_progress, TRANCE_BGM_XFADE_FRAMES, &main_cb, &second_cb);
    /* ★ 我们自己的流**不需要**对象校验：它由我们创建、只有我们引用（引擎换曲动的是 `0x570484`）。
     *   上一版在这里加校验 + "撒手不销毁" ⇒ 泄漏了一个仍在播的流、而且 `s_stream` 变 NULL 后
     *   tick 每帧早退 ⇒ **退出灵界/暂停都再也停不掉它**（用户实跑三条反馈的同一个根因）。
     *   ⇒ 这里只判非空；真要收拾就在 `finish_release()` 里按引擎自己的形状销毁。*/
    if (s_stream) bgm_set_volume(s_stream, (int)second_cb);
    if (s_engine_bgm_seen) {
        if (bgm_obj_usable(s_engine_bgm_seen)) bgm_set_volume(s_engine_bgm_seen, (int)main_cb);
        else drop_engine_bgm("apply_mix 前逐级校验没过");
    }
}

/* ------------------------------------------------------------------ 收尾（唯一一处） */

static void finish_release(void)
{
    void *cur_bgm = engine_bgm();
    /* ★ 只有"真的接管过"才算一次 release：起流失败时也会走这里（那一路什么都没接管），
     *   否则日志里的 `released` 会大于 `started`，把"有没有泄漏"这个判据搅浑。*/
    const int had_stream = (s_stream != NULL);

    /* ① 恢复主 BGM 音量 —— **先认指针还在不在、链还通不通**：引擎换过曲（甚至只是先释放、
     *    几帧后才写新指针）时 `s_engine_bgm_seen` 指向的可能已经是释放掉的对象
     *    ⇒ 只有"同一个对象 **且** 链校验过"才写这一笔（★ 2026-09-14，见 `bgm_chain_ok()`）。*/
    if (s_engine_bgm_seen && cur_bgm == s_engine_bgm_seen && bgm_obj_usable(s_engine_bgm_seen))
        bgm_set_volume(s_engine_bgm_seen, TRANCE_BGM_CB_FULL);
    else if (s_engine_bgm_seen) {
        ++s_stat_engine_lost;
        ce_log("trance-bgm: 收尾时跳过恢复主音量（缓存 %p%s%s）",
               s_engine_bgm_seen,
               cur_bgm == s_engine_bgm_seen ? "" : " 已被引擎换掉",
               bgm_obj_usable(s_engine_bgm_seen) ? "" : " 链校验没过=已释放");
    }

    /* ② 关事件 → ③ 释放第二路（顺序照引擎自己的收尾：`0x477465` 关事件，`0x477481` 释放流）
     *    → ④ 才轮到 PCM 那块内存（流对象还活着的时候绝不能 free 它的数据源）。*/
    if (s_event) { CloseHandle(s_event); s_event = NULL; }
    if (s_stream) { bgm_destroy(s_stream); s_stream = NULL; }
    if (s_pcm) { VirtualFree(s_pcm, 0, MEM_RELEASE); s_pcm = NULL; }
    s_pcm_bytes       = 0;
    s_engine_bgm_seen = NULL;
    s_paused_ours     = 0;
    s_progress        = 0;
    s_phase           = TBGM_IDLE;
    if (had_stream) ++s_stat_release;
}

/* ------------------------------------------------------------------ 对外接口 */

int ce_trance_bgm_available(void)
{
    if (s_selfcheck < 0) s_selfcheck = trance_bgm_core_selfcheck();
    return s_selfcheck == 0 && ds_object() != NULL && engine_bgm() != NULL;
}

int ce_trance_bgm_start(void)
{
    void *ds, *bgm;
    int   hr = 0, out = 0, track_src, rc;
    uint32_t seg;
    char  cur[40], fname[48];

    if (s_phase != TBGM_IDLE) return 1;                  /* 幂等：已经在接管 */
    /* ★ 冷却：刚因为"换曲/对象没了"撤过流 ⇒ 60 帧内不再起（防"起-撤"每帧打转，
     *   2026-09-14 日志里那 440 次 start/release 就是打转的指纹）。*/
    if (s_restart_block > 0) return 0;
    if (s_selfcheck < 0) s_selfcheck = trance_bgm_core_selfcheck();
    if (s_selfcheck != 0) {
        ce_log("trance-bgm: REFUSED - core selfcheck %d (常量与语义脱钩)", s_selfcheck);
        return 0;
    }

    ds  = ds_object();
    bgm = engine_bgm();
    if (!ds || !bgm) {
        ce_log("trance-bgm: REFUSED - engine faces missing (DirectSound %p, BGM %p)", ds, bgm);
        return 0;
    }

    /* ① 当前曲名 → b 变体名（TH13 的规则）。**拿不到名字就整个不接管**（不猜曲目）。*/
    track_src = cur_track_name(cur, sizeof cur);
    if (track_src < 0) {
        ++s_stat_noname;
        if (strcmp(s_last_fail, "<noname>") != 0) {
            snprintf(s_last_fail, sizeof s_last_fail, "<noname>");
            ce_log("trance-bgm: REFUSED - 既读不到当前曲名（0x%06x）也没有关卡表（0x%06x 为空）"
                   " ⇒ 无从判断该放哪首的灵界版", (unsigned)TBGM_ADDR_CUR_NAME,
                   (unsigned)TBGM_ADDR_STAGE_DATA);
        }
        return 0;
    }
    /* ★ 素材模式（`<bgm>\mode.txt`）：music（默认）/ se / both ⇒ 决定用 `b.wav` / `b_se.wav` / `b_both.wav`。
     *   变体文件由 `tools/make_trance_marker.py` 生成；切模式不用重新发布。*/
    {
        char mode[16];
        read_material_mode(mode, sizeof mode);
        if (trance_bgm_variant_name(cur, fname, sizeof fname) != 0) {
            ce_log("trance-bgm: REFUSED - 曲名 \"%s\" 造不出 b 变体（空/带路径/太长？）", cur);
            ++s_stat_noname;
            return 0;
        }
        apply_material_mode(fname, sizeof fname, mode);
    }

    /* ② 读素材（真音频；文件不在 / 坏了 / 太大 ⇒ 不接管）。三条候选目录由 `load_variant`
     *    自己按顺序试，失败时把三条**完整路径**都打进日志。*/
    rc = load_variant(fname, &s_pcm, &s_wav, NULL, 0);
    if (rc != 0) {
        if (rc == -1)      ++s_stat_nofile;
        else if (rc == -3) ++s_stat_toobig;
        else if (rc == -4) ++s_stat_alloc;
        else               ++s_stat_badwav;
        ++s_stat_fail;
        /* 同一种失败只详报一次（一次灵界一行的刷屏没有意义），换曲目再报。*/
        if (strcmp(s_last_fail, fname) != 0) {
            snprintf(s_last_fail, sizeof s_last_fail, "%s", fname);
            ce_log("trance-bgm: REFUSED - 素材 \"%s\" 不可用 rc=%d（当前曲名 \"%s\"，来源 %s）"
                   " ⇒ 本次灵界**不改 BGM**", fname, rc, cur, track_src == 0 ? "引擎当前曲名" : "关卡表兜底");
        }
        return 0;
    }

    /* ③ 我们自己的 fmt 记录：**带 intro**（TH13 的 b 轨口径；intro = 循环回绕点）。
     *    ★ 用的是 **data 载荷**的长度，不是文件长度（44 B 的 RIFF 头不算音频）。
     *    ★★ 2026-10-24 方案 A：intro 段的字节数 = **主曲 fmt 的 `intro_size`（一手值）**
     *       按 avg 比例换算（44100→22050 ⇒ ÷2）。生成器切的 intro 段用的就是同一个一手值
     *       ⇒ 回绕点与主曲的循环点逐采样对齐（实测 lag ≈ 0）。
     *       读不到一手 intro ⇒ 0（= 老口径：素材只有循环体时整段循环）。*/
    s_pcm_bytes = s_wav.pcm_bytes;
    {
        uint32_t our_ba  = (uint32_t)s_wav.channels * (uint32_t)(s_wav.bits / 8u);
        uint32_t our_avg = (uint32_t)s_wav.rate * our_ba;
        trance_bgm_fmt_init(&s_fmt, fname, s_pcm_bytes,
                            engine_intro_bytes(bgm, our_avg, our_ba),
                            s_wav.channels, s_wav.rate, s_wav.bits);
        ce_log("trance-bgm: 素材 fmt：%u/%uch/%ubit，avg %u B/s；intro %u B = %.3f s"
               "（= 主曲 fmt 的 intro_size 按 avg 换算；0 = 整段循环）",
               (unsigned)s_fmt.samples_per_sec, (unsigned)s_fmt.channels,
               (unsigned)s_fmt.bits_per_sample, (unsigned)s_fmt.avg_bytes_per_sec,
               (unsigned)s_fmt.intro_size, s_fmt.intro_size / 88200.0);
    }
    seg = trance_bgm_segment_bytes(s_fmt.avg_bytes_per_sec, s_fmt.block_align);
    if (seg == 0) {
        ce_log("trance-bgm: REFUSED - segment size 0 (avg %u, block %u)",
               (unsigned)s_fmt.avg_bytes_per_sec, (unsigned)s_fmt.block_align);
        finish_release();
        return 0;
    }
    /* 环 = 8 段；素材比环还短的话引擎会在填满整环之前就回卷 ⇒ 直接拒绝（避免半环噪声）。*/
    if (s_pcm_bytes < seg * 8u) {
        ce_log("trance-bgm: REFUSED - 素材 %u B 比引擎的 8 段环 (%u B) 还短",
               (unsigned)s_pcm_bytes, (unsigned)(seg * 8u));
        finish_release();
        return 0;
    }

    /* ④ 事件（引擎自己也是先 CreateEventA 再建流；必须是 **auto-reset** —— 引擎建的时候
     *    `CreateEventA(0, 0, 0, 0)`，第二个参数 0 = auto-reset）。*/
    s_event = CreateEventA(NULL, FALSE, FALSE, NULL);
    if (!s_event) {
        ce_log("trance-bgm: REFUSED - CreateEventA failed (%lu)", (unsigned long)GetLastError());
        finish_release();
        return 0;
    }

    /* ⑤ 第二路内存流。参数逐条照 `th18:0x476e42` 一带的引擎调用点（见文件头 §1）。*/
    hr = ((tbgm_fn_ctor_t)(uintptr_t)TBGM_FN_STREAM_CTOR)(
             ds, &out,
             (uint32_t)(uintptr_t)s_wav.pcm, s_pcm_bytes, (uint32_t)(uintptr_t)&s_fmt,
             0,                                   /* arg5：构造成器不读 */
             0, 0, 0, 0,                           /* arg6..9：DSBUFFERDESC.guid3DAlgorithm = 全 0 */
             0,                                   /* arg10：构造成器不读 */
             (int)seg, (int)(intptr_t)s_event);
    if (hr < 0 || out == 0) {
        ce_log("trance-bgm: REFUSED - stream ctor hr=0x%08x out=%p", (unsigned)hr, (void *)(uintptr_t)out);
        finish_release();
        ++s_stat_fail;
        return 0;
    }
    s_stream          = (void *)(uintptr_t)out;
    s_stream_vtbl     = (void *)(uintptr_t)chain_vtbl(s_stream);   /* 参照物：它一定是好对象 */
    /* 引擎那路也先验一遍再接管（拿不到就"接管空气"还不如不接管）：链不通就不动它。*/
    if (bgm_obj_usable(bgm)) {
        s_engine_bgm_seen = bgm;
    } else {
        s_engine_bgm_seen = NULL;
        {
            uint32_t dp1 = *(uint32_t *)((uint8_t *)bgm + 8);
            uint32_t dp2 = dp1 ? *(uint32_t *)dp1 : 0;
            uint32_t dvt = dp2 ? *(uint32_t *)dp2 : 0;
            ce_log("trance-bgm: 引擎 BGM 链/类别校验没过（bgm=%p → p1=%p → p2=%p → vtbl=%p，"
                   "我方参照物 %p）⇒ 只起第二路、不去动主 BGM 的音量（tick 里还会再试接管）",
                   bgm, (void *)(uintptr_t)dp1, (void *)(uintptr_t)dp2, (void *)(uintptr_t)dvt,
                   s_stream_vtbl);
        }
    }
    s_progress        = 0;
    s_paused_ours     = 0;
    snprintf(s_variant, sizeof s_variant, "%s", fname);
    s_last_fail[0] = 0;                          /* 这次成功了，失败记录作废 */

    /* ⑥ 起播（loop）+ 立刻把淡化起点写下去：`play` 内部会把我们的音量设成"用户满音量"
     *    （一手 `0x48a440` 尾段调 `0x48a5a0(this,0)`）⇒ 不马上覆盖就会有 1 帧的满音量重叠。*/
    /* ★★ 起播与"同时间点"的落地顺序（三件事，缺一件就"接不上"）：
     *   ① **先把游标挪到目标位置**（`bgm_seek_to_engine` 写描述符 `+0x84`）；
     *   ② **临时把 fmt 的循环点设成同一个偏移** ⇒ 万一起播路径里跑了引擎自己的 reset
     *      （`0x48acc0` 的 preload 支：`cur = base + fmt->intro_size`），它落的也是同一处
     *      —— 两条路互为保险，谁先谁后都对；
     *   ③ 循环点**不再回收成 0**（★ 2026-10-24）：素材带 intro 之后，`intro_size` = 素材的
     *      intro 字节数 = **原曲的循环点**，之后的回卷必须回到那里（回到 0 = 重放 intro）。
     *   ⚠ 位置来源用**墙钟**（≈ 听到的位置）：引擎是"读盘跑在播放前面"的，
     *     描述符的 `remaining` 是**读盘位置**，拿它 seek 会偏后一个已缓冲量（最多十几秒）。*/
    bgm_probe("ctor 后", s_stream);
    /* ★★ 2026-09-15 订正（两次）：**重编曲版（`rearr`）也做同时间点 seek**，而且**顺序必须是
     *   play → poke → 整环重填**（上一版写成 poke → play 是**无效**的）。
     *
     *   ① 为什么"重编曲也 seek"：上一版这里写着"重编曲相位对齐没有意义"，依据是 TH13 `03` vs `03b`
     *      的包络相关只有 0.407。但那是**TH13 b 轨**的性质；本 mod 的 `_rearr` 素材是**拿 TH18 原曲的
     *      MIDI 重编的** —— 同一首曲子、同一条时间轴（MIDI 速度实测 = 原曲），实测 chroma 时间序列相关
     *      **0.928**（最佳 lag ≈ 0，AUDIT §CU-4），而生成器现在会把素材起点**细对齐到原曲循环点**上
     *      （`fine_align`：chroma 判据、±0.8 s 窗口，AUDIT §CW）⇒ 相位是对得齐的。
     *      不 seek 的后果正是用户听到的「时间对不上」：主曲此刻在 A 处，我们却从素材 0 秒起播，
     *      交叉淡化的那 20 帧就是"两个时间点同时响"。
     *
     *   ② 为什么顺序不能反过来：`play`（`0x48a440`）内部对描述符调 `reset(desc, rewind=0, pos)`
     *      （`0x48a22c`→`0x48acc0`），而**内存流**走的是预载分支
     *      `cur = base; if (rewind && intro_size > 0) cur = base + intro_size;`
     *      （一手，本仓 `engine/_shared` §8.3）⇒ `rewind=0` ⇒ **它把 `cur` 抹回 `base`**，
     *      于是"先 poke 再 play"写的游标被覆盖掉；而且 `play` 还会按 `cur` 再填一段环。
     *      ⇒ 顺序必须是：**先 play**（引擎按 ctor 预填的环、从环偏移 0 起播），
     *      **再** poke 游标，**再**整环重填。
     *
     *   ③ 为什么必须重填：环偏移 ↔ PCM 字节的对应由"**谁先被填**"决定（§8.4）——
     *      poke 只改"下一次读哪"，已经躺在环里的 8 段（= 16 s）不会变。从 `so` 起连填
     *      `TRANCE_BGM_RING_SEGS` 段（正好绕回环偏移 0）之后，"从环偏移 0 播出"的才是 `so`。
     *      这一步发生在我们 FADE_IN 的起点（我方音量 0、主曲仍满音量）⇒ 听不到。
     *
     *   ④ 位置来源用**帧时钟**（≈ 听到的位置）：引擎"读盘跑在播放前面"，描述符的 `remaining`
     *      是**读盘位置**，拿它 seek 会偏后一个已缓冲量（最多十几秒）。
     *   ⑤ 循环点 = 素材的 intro 字节数（`bgm_seek_to_engine` 里按一手 intro 再写一次，
     *      两边同式同值）；seek 失败 ⇒ 保持 fmt_init 的那份（读不到一手 intro 时就是 0）。*/
    s_seek_pos   = 0;                       /* 清零：seek 失败就老老实实从 0 起播 */
    s_ring_bytes = seg * 8u;                /* 环长（`bgm_seek_to_engine` 只用来打日志）*/
    bgm_play_at(s_stream, 0);               /* ② 先 play（此后 poke 才不会被 reset 抹掉）*/
    if (s_engine_bgm_seen && !s_noseek) {
        bgm_play_at(s_stream, 0);          /* 先 play：否则下面的 poke 会被 play 内部的 reset 抹掉 */
        uint32_t so = bgm_seek_to_engine(s_engine_bgm_seen, s_stream, s_pcm_bytes,
                                         s_fmt.avg_bytes_per_sec, s_fmt.block_align);
        s_seek_pos = so;                                    /* 记下来（日志/诊断用）*/
        if (so && so < s_pcm_bytes) {
            for (int i = 0; i < TRANCE_BGM_RING_SEGS; ++i) (void)bgm_refill(s_stream);   /* ③ */
        } else {
            ce_log("trance-bgm: seek 目标不可用（so=%u / %u B）⇒ 从素材 0 秒起播",
                   (unsigned)so, (unsigned)s_pcm_bytes);
        }
        bgm_probe("poke 后", s_stream);
    }
    bgm_probe("play 后", s_stream);
    s_probe_left = 6;                     /* 前 6 次补 buffer 前后各一行（看清第一段数据从哪开始）*/
    apply_mix();

    s_phase = TBGM_FADE_IN;
    ++s_stat_start;
    ce_log("trance-bgm: OK 第二路起播：素材 \"%s\"（当前曲名 \"%s\"，来源 %s）"
           "PCM %u B = %u ms，%u/%uch/%ubit，seg %u B = %u ms，环 %u ms，引擎 BGM %p - 交叉淡化 %d 帧",
           fname, cur, track_src == 0 ? "引擎当前曲名" : "关卡表兜底",
           (unsigned)s_pcm_bytes,
           (unsigned)((uint64_t)s_pcm_bytes * 1000u / s_fmt.avg_bytes_per_sec),
           (unsigned)s_fmt.samples_per_sec, (unsigned)s_fmt.channels, (unsigned)s_fmt.bits_per_sample,
           (unsigned)seg, (unsigned)((uint64_t)seg * 1000u / s_fmt.avg_bytes_per_sec),
           (unsigned)((uint64_t)seg * 8u * 1000u / s_fmt.avg_bytes_per_sec),
           s_engine_bgm_seen, TRANCE_BGM_XFADE_FRAMES);
    ce_log("trance-bgm: 起播对齐 = %u B = %.2f s（0 = 未 seek）", (unsigned)s_seek_pos, s_seek_pos / 88200.0);
    return 1;
}

void ce_trance_bgm_pause_poll(void)
{
    /* ★★ 2026-09-15 实跑订正（用户：「灵界 bgm 暂停后就没了」）：暂停**不再"停住再续播"**，
     *   而是**释放第二路**、恢复时**按进灵界那条已验证可用的路径重新起流**。
     *
     *   上一版的做法是 `bgm_stop` + 恢复时"poke 游标 + 整环重填 + play"。它有两个坑：
     *     ① `stop`（`th18:0x48a620`）会给每个 DS buffer 调 `SetCurrentPosition(0)`
     *        （vtable `+0x34`），而**同一时刻主曲的位置是保留的** ⇒ 一恢复就错开；
     *     ② 恢复时环里躺着的是旧数据（最多一整圈 16 s），而"poke 游标"只改"下一次读哪"、
     *        改不了已经填好的环 —— 重填判断要依赖"环偏移↔PCM 字节由谁先被填决定"这条**推断**，
     *        一旦不成立就是静音或错位（用户这次听到的正是"暂停后就没了"）。
     *
     *   现在的口径（全部走**已验证可用**的起流路径，不再有暂停专用的对齐逻辑）：
     *     · 暂停（引擎把自己的流停了）⇒ 立刻 `finish_release()` 收干净（不留半死状态）；
     *     · 恢复（引擎又开始播了）⇒ 调 `ce_trance_bgm_start()` —— 它会重新算
     *       "主曲此刻的位置"并对齐起播。**帧时钟在暂停期间不推进**（`tick` 不跑），
     *       主曲位置同样不动 ⇒ 恢复后的对齐目标与暂停前一致 ✓。
     *   代价：恢复时有一次素材重读 + 8 MiB 分配（几十毫秒，且发生在暂停菜单关闭的那一帧）。*/
    if (s_paused_ours) {
        void *bgm = engine_bgm();
        /* 恢复的前提：引擎那条流真的又在播了，而且还是我们当初接管的那一个。*/
        if (bgm && bgm != s_engine_bgm_seen) { s_paused_ours = 0; return; }   /* 换曲了 ⇒ 不恢复 */
        if (bgm && bgm_obj_usable(bgm) && engine_bgm_playing(bgm)) {
            s_paused_ours = 0;
            ce_log("trance-bgm: 引擎 BGM 恢复 ⇒ 按主曲此刻位置重新起流（与进灵界同一条路径）");
            (void)ce_trance_bgm_start();
        }
        return;
    }

    if (s_phase == TBGM_IDLE || !s_stream) return;
    if (engine_bgm() != s_engine_bgm_seen) return;      /* 换曲那条路由 `tick` 处理 */
    if (!bgm_obj_usable(s_engine_bgm_seen)) return;     /* 旧对象已释放：不碰它（`tick` 会撤）*/

    /* 镜像引擎的播放状态（暂停菜单 / 对话期间引擎会把自己的流停掉）。
     * 判据 = 流对象的 `+0x58`：`play` 置 1、`stop` 置 0（一手两个函数各写一次）。*/
    if (!engine_bgm_playing(s_engine_bgm_seen)) {
        ++s_stat_pause;
        s_paused_ours = 1;
        ce_log("trance-bgm: 引擎 BGM 停了（暂停菜单/对话）⇒ 释放第二路，恢复时重新起流");
        finish_release();                 /* 立刻收干净：不留"停住但不释放"的半死状态 */
    }
}

void ce_trance_bgm_tick(void)
{
    void *cur_bgm;

    /* ★ 帧时钟（seek 的兜底位置来源）：**每帧都要走**，所以在下面那些早退之前。
     *   引擎换曲 = 重建流对象（`0x477481` 释放 → `0x4898f0` 新建）⇒ 对象一变就重新计时。
     *   暂停期间本函数不被调用（玩家 tick 停走）⇒ 时钟不推进，与"引擎音乐也停"一致。*/
    cur_bgm = engine_bgm();
    if (cur_bgm != s_watch_obj) { s_watch_obj = cur_bgm; s_watch_frames = 0; }
    else if (cur_bgm) ++s_watch_frames;

    if (s_phase == TBGM_IDLE || !s_stream) return;

    /* ① 引擎换过 BGM 对象（换曲 / 关卡重载）：旧对象可能已被释放 ⇒ 立刻撤、绝不碰它。*/
    cur_bgm = engine_bgm();
    /* ★ 判据两条：指针换了 **或** 旧对象已经不成立了（引擎"先释放、几帧后才写新指针"的窗口里，
     *   指针看着没变，但对象已经没了 —— 2026-09-14 那两次闪退就在这个窗口里）。
     *   ⚠ 但"**我们从没接管过**"（起流时校验没过 ⇒ `s_engine_bgm_seen == NULL`）**不是**"变了"：
     *   上一版把它当"变了" ⇒ 每帧撤一次、卡侧下一帧又起一次（日志 440/440）。这里改成
     *   "没接管过就顺手再试一次接管"，接管不上也不撤。*/
    if (s_engine_bgm_seen == NULL) {
        if (cur_bgm && bgm_obj_usable(cur_bgm)) {
            s_engine_bgm_seen = cur_bgm;
            ce_log("trance-bgm: 补接管引擎 BGM %p（起流那一刻没接上）", cur_bgm);
        }
    } else if (cur_bgm != s_engine_bgm_seen
        || (s_engine_bgm_seen && !bgm_obj_usable(s_engine_bgm_seen))) {
        ++s_stat_swap;
        ce_log("trance-bgm: engine BGM object changed/gone (%p -> %p, 旧对象链校验 %s)"
               " - releasing our stream immediately",
               s_engine_bgm_seen, cur_bgm,
               (s_engine_bgm_seen && bgm_obj_usable(s_engine_bgm_seen)) ? "通过" : "**没过**");
        s_restart_block = 60;              /* 防"起-撤"打转：撤了之后至少 60 帧不再起 */
        finish_release();
        return;
    }
    if (s_restart_block > 0) --s_restart_block;

    /* ② 暂停镜像（同一个函数，绘制路径也会调它）*/
    ce_trance_bgm_pause_poll();
    if (s_paused_ours) return;           /* 暂停期间不推进淡化、也不补 buffer */

    /* ③ 补 buffer：引擎的补 buffer 线程只服务 `0x570484`（见文件头），我们的流得自己问自己。
     *    事件是 auto-reset ⇒ 一次 wait 消费一个信号；每帧问一次，环是 8 段（22050/2ch 下 8×2 s = 16 s），
     *    60 fps 下绝不会问漏。*/
    /* ⚠ 这行早退的前提是「`s_stream` 非空 ⇒ 它一定是个能用的流对象」—— 这个不变量由
     *   `finish_release()`（唯一销毁点，销毁后立刻置 NULL）维持。**任何"撒手但保留对象"的
     *   分支都会打破它**（2026-09-14 踩过：第二路被泄漏、而且每帧从这里早退 ⇒ 退出/暂停
     *   都停不掉、refill 也停了 ⇒ 只剩 16 s 的环在打转）。⇒ 别再往这里加"丢弃指针不销毁"。*/
    if (s_event && WaitForSingleObject(s_event, 0) == WAIT_OBJECT_0) {
        if (s_probe_left > 0) bgm_probe("refill 前", s_stream);
        (void)bgm_refill(s_stream);
        ++s_stat_refill;
        if (s_probe_left > 0) {
            char tag[24];
            snprintf(tag, sizeof tag, "refill #%d 后", (int)s_stat_refill);
            bgm_probe(tag, s_stream);
            --s_probe_left;
        }
    }

    /* ④ 淡化推进 */
    if (s_phase == TBGM_FADE_IN) {
        if (++s_progress >= TRANCE_BGM_XFADE_FRAMES) { s_progress = TRANCE_BGM_XFADE_FRAMES; s_phase = TBGM_HOLD; }
    } else if (s_phase == TBGM_FADE_OUT) {
        if (--s_progress <= 0) { finish_release(); return; }
    }

    /* ⑤ 每帧把两路音量写回去（引擎自己那套 fade/音量命令可能盖掉我们）*/
    apply_mix();
}

void ce_trance_bgm_release(int immediate)
{
    s_paused_ours = 0;                    /* ★ 灵界结束了 ⇒ 别再"恢复后重起"（§CW-7 的坑）*/
    if (s_phase == TBGM_IDLE) return;
    if (immediate || TRANCE_BGM_XFADE_FRAMES <= 0) { finish_release(); return; }
    /* 从**当前位置**往回淡（不是从 0 重来）⇒ 无论在 FADE_IN 还是 HOLD 都不会"跳音量"。*/
    s_phase = TBGM_FADE_OUT;
}

int ce_trance_bgm_active(void) { return s_phase != TBGM_IDLE; }

const char *ce_trance_bgm_state_name(void)
{
    static char buf[200];
    snprintf(buf, sizeof buf, "bgm phase %d prog %d stream %p pcm %u B variant %s started %d released %d "
                              "refill %d pause %d swap %d fail %d engine-lost %d"
                              " (nofile %d badwav %d toobig %d alloc %d noname %d)",
             s_phase, s_progress, s_stream, (unsigned)s_pcm_bytes,
             s_variant[0] ? s_variant : "-",
             (int)s_stat_start, (int)s_stat_release, (int)s_stat_refill,
             (int)s_stat_pause, (int)s_stat_swap, (int)s_stat_fail, (int)s_stat_engine_lost,
             (int)s_stat_nofile, (int)s_stat_badwav, (int)s_stat_toobig, (int)s_stat_alloc,
             (int)s_stat_noname);
    return buf;
}
