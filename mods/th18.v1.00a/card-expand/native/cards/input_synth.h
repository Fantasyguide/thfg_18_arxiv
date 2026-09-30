/* input_synth.h —— 调试用「逐帧合成方向输入」的**纯逻辑**（无引擎依赖、可主机单测）。
 *
 * 用途（用户 2026-09-27 口径）：
 *   「写一个脚本挂在调试键上，**按照帧操作进行移动**，**松开方向键**，然后**平均意义上不要移动**」
 *
 * ⇒ 按住调试键时，逐帧替玩家写方向位；一个周期里**含"不按方向"的帧**；
 *   整周期**净位移期望 = 0**（自机在原地抖，不漂走）。
 *
 * 本文件只放「第 t 帧该按什么」这一件事；写输入位 / 日志 / 调试键在 `input_synth.c`。
 * 拆开的理由与 `season_core.h` / `animal_core.h` 同款：主机上 `make test-host` 能直接跑，
 * 不必起游戏。
 *
 * ============================ 为什么是「左 N / 松 M / 右 N / 松 M」 ============================
 *
 * 一、**周期的一半就是它的镜像**（这是"净位移恰好为 0"的**结构性**理由，不是平均意义上的近似）
 *
 *   令输入序列 `u(t)`，周期 `P = 2(N+M)`：
 *
 *       [0, N)          = 左        (L)
 *       [N, N+M)        = 松        (0)
 *       [N+M, 2N+M)     = 右        (R)
 *       [2N+M, 2N+2M)   = 松        (0)
 *
 *   把时间平移半个周期 `P/2 = N+M` 再看：`L→R`、`R→L`、`0→0`，也就是
 *
 *       u(t + P/2) = -u(t)          （对每个 t 都成立，"−" = 左右互换）
 *
 *   一键核对：`[0,N)` 的 L 平移后落进 `[N+M,2N+M)`=R ✓；`[N,N+M)` 的 0 落进 `[2N+M,P)`=0 ✓。
 *
 *   而自机的运动方程对 `x → -x` 是**等变**的（`g(-x,-v,-u) = -g(x,v,u)`：
 *   代码里 x/y 两轴只差"用哪张方向表"，且 `0x4b7058(左)=(-1,0)` 正是 `0x4b7060(右)=(1,0)` 的取反
 *   —— 一手，见 input_synth.c 顶部「一手依据」第 2 条）。于是
 *   `w(t) := -z(t + P/2)` 也是同一个输入下的解；耗散系统（速度被上界钳住）里周期解唯一
 *   ⇒ `z(t+P/2) = -z(t)` ⇒ **`z(t+P) = z(t)`**：位置逐周期**精确**回到原点，
 *   不是"平均为 0"，是每个周期末尾 `Δx ≡ 0`。
 *
 *   ⚠ 这条对**任意** N、M 都成立 —— 所以 N/M 不是靠"抵消惯性"选的（见第三条）。
 *
 * 二、**TH18 自机没有加速度/惯性**（这条是本仓先前口径 ⚠「松手后有减速」的**订正**）
 *
 *   一手反汇编 `Player__sub_45b170` `0x45b170` 的积分段（`0x45b5a2`–`0x45b608`）逐条：
 *
 *       xmm2 = [player+0x477ec]                       ; 移速倍率（每帧复位 1.0，0x45c702）
 *       edx  = dirX * speed                           ; dirX ∈ {-1,0,1}（0x4b7058/0x4b7060 那张整数表）
 *       edx -= (int)([player+0x477f0] * -128.0)       ; 那是"屏幕偏移"补偿项，见下 ⚠
 *       dx   = (int)((float)edx * xmm2)
 *       [player+0x62c] += dx                          ; 0x62c = x 的 1/128 px 定点（位置权威副本）
 *
 *   —— `speed` 每一帧**只由当前输入选表**得到，公式里**没有任何"上一帧速度"项**；
 *   松开方向那一帧 `dirX = dirY = 0` ⇒ `dx = 0` ⇒ **当帧速度就归零**。
 *   （`player+0x47934` = 上一帧 dx，但它在 `0x45b69f` 被覆盖**之前**只在
 *    `0x45b618`/`0x45b658`/`0x45b750`/`0x45b799` 被读，用途是**转身特效**，不是插值。）
 *   ⇒ 「左 1 帧 / 右 1 帧」在 TH18 **本来就是净零**，不需要靠 N/M 去抵消惯性。
 *
 *   ⚠ 唯一没闭合的项：`player+0x477f0..f8` 每帧从全局 `0x5217dc..e4` 整块拷来
 *     （`0x45c717 movq`），而那一格在文件镜像里还在 .data 的 BSS 尾巴（未初始化）⇒
 *     **运行期常态值 ❓ 未闭合**。它在 dx 里是**与输入方向无关的同一常数**，
 *     左右两半都加同样的量 ⇒ 会破坏上面那条"精确回到原点"（每周期多 `P × 该项`）。
 *     记在报告 §⑤ 待查：跑之前先在 `x` 方向静置几帧，看 `dxq` 是否恒 0。
 *
 * 三、**那 N/M 怎么取？** 由「抖动幅度」与「触发率」定，而不是由惯性定：
 *
 *   · 峰值偏移 = **N 帧的全速位移**（L 块连续走 N 帧才回头）⇒ **N 越小越不漂**。
 *     周期末精确归零，但周期**中途**的 |Δx| 上界就是 `N × V`（V = 一帧位移，
 *     普通 4 px/frame、"文"卡超速 8 px/frame）⇒ **N = 1 是唯一最小解**。
 *   · M 只要 ≥ 1 就满足"含松开帧"，且 M 只影响"静置时长"，不放大 |Δx| 的峰值。
 *     但 **M = 1 还有第二个好处**：文那张卡的判据是
 *     「上一帧方向 == 0 ∧ 这一帧方向 != 0 ∧ 不聚焦」（`engine/card/th18/12-card29-aya-speedqueen.md` §2，
 *     一手 `0x40ca95`–`0x40cab2`）—— `M = 1` 让"0 → 非 0"的跳变**每 2 帧就发生一次**
 *     （周期 4 帧里两次：进 L 一次、进 R 一次）⇒ 无敌 +4/次的触发率拉满（净 +2 帧/帧，与文档 §4 配方 B 同率）。
 *     M 越大触发越稀，只有"想少抖"时才用（本工具默认不图那个）。
 *   · ⇒ **默认 N = 1, M = 1**（周期 4 帧：左 / 松 / 右 / 松）。
 *
 * 四、**默认关闭**：`ce_input_synth_tick()` 在调试键没按下时**一个字节都不写**（见 .c）。
 *
 * 硬门：本文件是纯 C（`static inline`），不含浮点、不含 x87 —— 主机单测直接 include。
 */
#ifndef CE_INPUT_SYNTH_H
#define CE_INPUT_SYNTH_H

#include <stdint.h>

/* ---- 模式参数（改这三个宏 = 改模式；论证见文件头第三条）----
 * `#ifndef` 包着，是为了能用 `-DCE_INPUT_SYNTH_LEFT_FRAMES=k` 在主机上扫参数网格（tools/… 离线模型）。*/
#ifndef CE_INPUT_SYNTH_LEFT_FRAMES
#define CE_INPUT_SYNTH_LEFT_FRAMES      1u   /* N：向左逐帧写多少帧 */
#endif
#ifndef CE_INPUT_SYNTH_RIGHT_FRAMES
#define CE_INPUT_SYNTH_RIGHT_FRAMES     1u   /* N：向右逐帧写多少帧（与左**同值** ⇒ 半个周期是镜像）*/
#endif
#ifndef CE_INPUT_SYNTH_RELEASE_FRAMES
#define CE_INPUT_SYNTH_RELEASE_FRAMES   1u   /* M：「全松方向」多少帧（必须 ≥ 1；判据见文件头）*/
#endif

/* ---- 方向位（一手：engine/input/th18/01-key-mapping.md §2.3；与 engine.h 的 CE_ADDR_INPUT_HELD 同表）----
 * 0x010 = 上 / 0x020 = 下 / 0x040 = 左 / 0x080 = 右。本文件与 .c 都只用值，不用 engine.h，
 * 好让主机单测能单独编译。 */
#define CE_INPUT_SYNTH_DIR_UP     0x010u
#define CE_INPUT_SYNTH_DIR_DOWN   0x020u
#define CE_INPUT_SYNTH_DIR_LEFT   0x040u
#define CE_INPUT_SYNTH_DIR_RIGHT  0x080u
#define CE_INPUT_SYNTH_DIR_MASK   0x0F0u   /* 四个方向位一起；"接管方向"时先抹掉再用 */

/* 一个周期的帧数 = 左 N + 松 M + 右 N + 松 M */
static inline unsigned ce_input_synth_period(void)
{
    return 2u * (CE_INPUT_SYNTH_LEFT_FRAMES + CE_INPUT_SYNTH_RELEASE_FRAMES);
}

/* 第 `phase` 帧（0 = 按下调试键的那一帧）该写哪个方向位；0 = 这一帧**松开方向键**。 */
static inline uint32_t ce_input_synth_mask(unsigned phase)
{
    const unsigned n = CE_INPUT_SYNTH_LEFT_FRAMES;
    const unsigned m = CE_INPUT_SYNTH_RELEASE_FRAMES;
    const unsigned p = phase % ce_input_synth_period();

    if (p < n)            return CE_INPUT_SYNTH_DIR_LEFT;    /* [0, N)        左 */
    if (p < n + m)        return 0;                          /* [N, N+M)      松（★"松开方向键"的帧）*/
    if (p < 2u * n + m)   return CE_INPUT_SYNTH_DIR_RIGHT;   /* [N+M, 2N+M)   右 */
    return 0;                                                /* [2N+M, P)     松 */
}

/* 这一帧是不是"松开方向键"的帧（周期里必须存在；给日志/单测用）。 */
static inline int ce_input_synth_is_release(unsigned phase)
{
    return ce_input_synth_mask(phase) == 0;
}

/* 半周期 = 镜像平移量 P/2（文件头第一条的 `u(t + P/2) = -u(t)`）；单测直接验这条不变量。 */
static inline unsigned ce_input_synth_half_period(void)
{
    return CE_INPUT_SYNTH_LEFT_FRAMES + CE_INPUT_SYNTH_RELEASE_FRAMES;
}

/* ---- 运行件入口（`input_synth.c`）----
 * **每游玩帧调一次**（必须在 Player tick 之前 ⇒ 卡侧 `on_tick_2`（节点 0x16）或常驻帧点
 * `BP_ce_item_frame`（`th18:0x446ee3`）都满足）。返回值无。
 *
 * ⚠ 本件**不自己挂任何站点、也不注册任何卡**：接法（谁调这一句）由 owner 定，
 *   见 input_synth.c 文件尾「接线」。`Makefile` 的 `SRCS` 用 `cards` 目录通配
 *   （`$(wildcard cards/` + `*.c)`）⇒ 本文件**不需要改 Makefile** 就会被编进 DLL；
 *   需要的只有**一行调用**。
 * ⚠ 调试键没按下时本函数一个字节都不写（不按就 return）。 */
void ce_input_synth_tick(void);

#endif /* CE_INPUT_SYNTH_H */
