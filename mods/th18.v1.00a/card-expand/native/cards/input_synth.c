/* input_synth.c —— 挂在调试键上的「逐帧合成方向输入」运行件。
 *
 * 用户原话（2026-09-27）：
 *   「写一个脚本挂在调试键上，**按照帧操作进行移动**，**松开方向键**，然后**平均意义上不要移动**」
 * 用户追加（2026-09-13）：
 *   「**没有进 replay**」⇒ 要合成输入**进录像**（带工具的实验才可复现）；
 *   「**然后在回放的时候，不画 gui**」⇒ 回放中 / 商店冻结帧 / 无自机时**不画**（也不写、也不计数）。
 *
 * 模式（左 N / 松 M / 右 N / 松 M）与 N/M 的取值论证全在 `input_synth.h` 的文件头，这里只讲**落笔**。
 *
 * ============================== 一手依据（本文件用到的每一条） ==============================
 *
 * 1. **输入位与地址**（一手：engine/input/th18/01-key-mapping.md §2.3 + §4.1）
 *      `INPUT_HELD`    = `0x4ca428`（本帧按住的位；`engine.h` 的 `CE_INPUT_HELD()`）
 *      `INPUT_PRESSED` = `0x4ca434`（本帧上升沿，引擎每帧算：`0x42abc0`
 *                        `0x42ac48 xor`+`0x42ac50 and` ⇒ `pressed = (held ^ prev) & held`）
 *      方向位：`0x010` 上 / `0x020` 下 / `0x040` 左 / `0x080` 右（`0x45b2bd` 起八向分支读的就是它们）。
 *
 * 2. **左右镜像**（一手：`0x4b7058` = `(-1, 0)`、`0x4b7060` = `(1, 0)`，INT32 单位向量表；
 *    读点 `0x45b2cc`–`0x45b3ca`）⇒ 动力学对 `x → -x` 等变（`input_synth.h` 文件头第一条的证明前提）。
 *
 * 3. **★ 落点：录制 tick 里的新站点 `ce_input_synth`（`th18:0x462966`）**（一手，逐条反汇编
 *    `ReplayManager__on_tick__record_replay` `0x462940`；完整帧内顺序见
 *    engine/input/th18/01-key-mapping.md §4.1，站点声明见 sites.py 的 `ce_input_synth` 条目）：
 *
 *        0x462950  INPUT_HELD_PREV(0x4ca42c) = INPUT_HELD(0x4ca428)
 *        0x46295a  INPUT_HELD = (uint16)RAW(0x4ca210)      ; ← 用 raw 键盘**整个重写** HELD
 *        ---------------------- 本站点 = 0x462966 ----------------------
 *        0x462966  call 0x42abc0                            ; ← 算 PRESSED/RELEASED 边沿
 *        0x46296b  长按射击自动低速（可能 or HELD,0x8）
 *        0x4629f7  held16 = (uint16)INPUT_HELD              ; ← **录制读点**
 *        0x462a19  call 0x463060(chunk, held16, pressed16, released16)   ; 写 6 字节/帧
 *
 *    ★ 本仓站点机制（`thcrap_bp.h`）：**handler 在原件之前跑**，返回 `BP_EXEC_ORIGINAL` 才回放窗口
 *    那几字节。站点取 `0x462966` ⇒ 进入 handler 时 **`0x462961` 的拷贝已经执行完**
 *    （= owner 要的「拷贝之后」），而 `0x462966` 那条 `call` **还没执行**（= 「边沿之前」）。
 *    ⇒ handler 在这里写 `INPUT_HELD`：引擎随后自己算边沿、`0x4629f7` 又把它录进 replay，
 *      同帧 Player（节点 `0x17`，`0x11 < 0x17`）读到 ⇒ **玩法与录像同时正确**。
 *
 * 4. **★ 订正（2026-09-13，owner 报「没有进 replay」）：旧落点（卡 tick，节点 `0x16`）**不进录像**。
 *      录制读点 `0x4629f7` 在 `ReplayManager` 的节点 **`0x11`** 体内；卡链 tick = AbilityManager
 *      节点 `0x16`（`0x408475 push 0x16`）在它**之后**，且下一帧 `0x46295a`+`0x462961` 又用 raw
 *      键盘把 HELD 整个重写 ⇒ 旧落点写的位**永远赶不上录制读点、也不顺延**。
 *      ⇒ 现在改挂 `0x462966`（上一条）：写点进 `INPUT_HELD` 的**同一帧、边沿与录像之前**。
 *
 * 5. **边沿由引擎算，本件不再手写 `INPUT_PRESSED`**（★ 2026-09-13 订正）：
 *      旧落点在 `0x42abc0` **之后** ⇒ 不手写就没有"这一帧刚按下"的边沿（卡 29 判据吃 0→非 0 跳变）；
 *      新落点在 `0x42abc0` **之前** ⇒ 引擎自己为我们的位算 `INPUT_PRESSED`/`INPUT_RELEASED`，
 *      手写那一半**必须去掉**：否则同一帧写两次（我们写一次、引擎 `0x42abc0` 又按 prev 覆盖一次），
 *      而且与"引擎算"这条唯一真相源打架。方向位在 PRESSED 里**没有读者**（一手普查
 *      engine/input/th18/01-key-mapping.md §2.3：读 PRESSED 的只有 `0x45c084`(C,0x400) /
 *      `0x45c09c`(D,0x800) / `0x45c048`(X,0x002) / `0x440a6b`(0x80001)，与 0x0F0 不相交），
 *      所以"去掉"不会少给任何消费者。
 *
 * 6. **没有残留位**：`INPUT_HELD` 每帧被 `0x46295a`+`0x462961` 从 raw 键盘重写；
 *    `INPUT_PRESSED`/`INPUT_RELEASED` 每帧被 `0x462966 call 0x42abc0` 重算
 *    ⇒ 松开调试键后**不写就是没有**，不需要"清残留"（也不允许清：清了就等于键没按也写字节）。
 *    ⚠ 新落点下这个性质**更强**：我们写的是本帧的最终 `INPUT_HELD`，下一帧开头由 `0x462950`
 *      存进 `INPUT_HELD_PREV`，`0x42abc0` 据它算出的 RELEASED 也**会**进录像（一致，无害）。
 *
 * 7. **调试键**：`ce_key_down()` 走 `GetKeyboardState`（user32），不是游戏输入掩码
 *    （先例 `cards/animal.c:3875` 一带、`cards/ufo.c:5528`）。⇒ 调试键本身不进 replay 输入流。
 *    ⚠ **空格 = 启停开关**（用户口径）：「按一下开、再按一下关」，抓按下沿。
 *    ⚠ 不选 Ctrl+Space：那个组合**已经被占**（`animal.c` / `ufo.c` 的调试键都拿它当边沿触发）。
 *
 * ============================== replay 影响（★ 已订正） ==============================
 * 新落点在录制 tick 内 ⇒ **合成的方向位会进录像**（这正是 owner 2026-09-13 的要求）：
 * 回放时 `0x462a50` 那条 tick 把录下来的 `INPUT_HELD`/`PRESSED`/`RELEASED` 原样喂回
 * （engine/input/th18/01-key-mapping.md §4.2）⇒ 自机在回放里**走出与现场一致的抖动**。
 * 规避/注意事项：
 *   ① 回放**播放**走的是另一个 on_tick（`0x462a50`），本站点根本不命中；handler 另有一道门
 *      `input_synth_blocked()`（`CE_GT_REPLAY_PLAYING` / 商店 / 无自机）⇒ 即使将来命中也不写；
 *   ② 回放中 HUD 不画、计数与检测也不推进（owner 追加口径）：回放里读到的是**回放注入的输入**
 *      产生的状态，算进我们的累计量会污染读数。
 *
 * ============================== 接线 ==============================
 * 本件只暴露一个"要写什么"的入口：`ce_input_synth_frame_mask(int *write_out)`。
 * **谁来调**：`sdk.c` 的站点 handler `BP_ce_input_synth`（站点 `ce_input_synth` = `th18:0x462966`，
 * 声明在 `sites.py`）；handler 负责把返回值写进 `INPUT_HELD`。**不再挂在卡 tick 桩上**
 * （旧接法见 §4 的订正；那样既不进录像、又要求卡组里至少有一张我方卡）。
 * 绘制半边 `ce_input_synth_draw()` 仍挂在 `sdk.h` 的 `on_draw` 桩里（`ce_hud_text` 的契约要求）。
 * （本件刻意不做成"某张卡的私有状态"：它是每帧一次的全局工具，**没有任何跨帧私有状态**
 *   （只有相位/快照几个标量，按设计随键起落复位）⇒ **没有 `on_delete` 要收的东西**。）
 */

/* ★ 2026-09-13 **本件已拆线**（不接入构建：站点/handler/绘制调用都撤了，代码留档）。
 *   它是什么、以及**重挂三步**，见 `input_synth.h` 文件头的状态块与 `DEV-INPUT-SYNTH.md` §0。*/
#include <stdint.h>
#include <stddef.h>              /* size_t：sdk.h 的 static inline 用了它（别的卡靠 <string.h> 顺带带进来，这里写明白）*/

#include "sdk.h"                 /* ce_key_down / ce_log / CE_VK_* / CE_INPUT_HELD() */
#include "engine.h"              /* CE_PLAYER() / CE_ADDR_INPUT_* / CE_GT_REPLAY_PLAYING */
#include "input_synth.h"

/* ---- 本件私有的字段偏移（**不改 engine.h**；出处 engine/player/th18/01-position-and-state-timers.md §1）----
 * player+0x620 / +0x624 = x / y（float, px）；权威副本 = +0x62c / +0x630（1/128 px 定点 int）。
 * 漂移量用**定点**量：整数、无浮点误差 ⇒ 日志里 `dxq` 就是精确到 1/128 px 的净位移。 */
#define IS_OFF_PLAYER_X       0x620
#define IS_OFF_PLAYER_Y       0x624
#define IS_OFF_PLAYER_X_FIXED 0x62c   /* 1/128 px 定点；每帧 += dx（一手 0x45b705/0x45b70b）*/
#define IS_OFF_PLAYER_INVULN  0x47778 /* 无敌计时器 cur（zTimer{prev@+0x47774, cur@+0x47778, f@+0x4777c}）*/

/* ---- 可调旋钮 ---- */
#define CE_INPUT_SYNTH_ENABLED        1     /* 发布版置 0：整件编译成空壳，一个字节都不写（无 static 未用告警）*/
#define CE_INPUT_SYNTH_LOG_EVERY      30u   /* 节流：每 30 帧（≈0.5 s）一行。⚠ 别调成 1（116 那次每帧刷 7419 行的教训）*/
#define CE_INPUT_SYNTH_LOG_START_STOP 1     /* 按下/松开各打一行（一行，不是每帧）*/

/* ---- 状态字（HUD）----
 * 用户 2026-09-13：「**加一个东西：生效的时候随便 plot 点什么 hud**」。
 * ⇒ 在 HUD 上画一行 ASCII 状态（`ce_hud_text`，**只能在 on_draw 里调**，
 *    所以绘制半边挂在 `sdk.h` 的 `on_draw` 桩上，见 `ce_input_synth_draw()`）。
 * ⚠ 字体是引擎的 ASCII 字体 ⇒ **只能用 ASCII**，别写中文。 */
#define CE_INPUT_SYNTH_HUD        1     /* 0 = 不画（发布版可关）。★ 现在是**平时常驻**；回放/商店/无自机不画 */
#define CE_INPUT_SYNTH_HUD_X      180.0f  /* 640×480 字面量空间（Gui 那套，与 `ce_hud_text` 一致）*/
#define CE_INPUT_SYNTH_HUD_Y       40.0f  /* 压在游玩区上部：一定会被看见（调试件，临时遮挡可接受）*/
#define CE_INPUT_SYNTH_HUD_BGRA   0xff40ff40u  /* BGRA：亮绿（运行中）*/
#define CE_INPUT_SYNTH_HUD_BGRA_IDLE 0xff40c0ffu /* BGRA：淡黄（停着时也画 —— 常驻指示器）*/
#define CE_INPUT_SYNTH_HUD_BGRA_SET  0xff4040ffu /* BGRA：红（检测到**引擎重设**无敌帧时）*/

/* 刻意**不**做"松开后再清一次位"的收尾：那会在键没按时也写字节（违反"默认一个字节都不写"），
 * 而引擎每帧重写 HELD/PRESSED ⇒ 本来就没有残留（见文件头第 6 条）。 */

#if !CE_INPUT_SYNTH_ENABLED

/* 发布版：整件是空壳（保证"不按调试键时一个字节都不写"是编译期事实，不是运行期判断）。 */
uint32_t ce_input_synth_frame_mask(int *write_out) { if (write_out) *write_out = 0; return 0; }
void     ce_input_synth_draw(void) { }

#else   /* ------------------------------ 下面是真正的工作体 ------------------------------ */

static unsigned s_phase;      /* 按下调试键后的第几帧（0 = 按下的那一帧）*/
static int      s_active;     /* 上一帧调试键是否按着（用来抓 按下 / 松开 的边沿）*/
static int32_t  s_x0_fixed;   /* 起抖时的 x（1/128 px 定点）；用来算累计 Δx */
static float    s_x0;         /* 起抖时的 x（float, px）；只给日志看 */
static int32_t  s_dxq_max;    /* 起抖以来 |Δx| 的峰值（1/128 px 定点）—— "平均不移动"一眼判据 */
/* ★ 2026-09-13 用户口径：「**重新启停应当帧数累加的**——或者说它应当是无敌帧本身计数的指示」
 *   ⇒ 下面两个计数**跨启停累加**（关掉再开不清零；整个游戏会话内一直累积）：
 *     · `s_frames_total` = 本件真正写过输入的**总帧数**（只有 running 且未被门挡住的帧才 +1）；
 *     · `s_inv_peak`     = 观测到的**无敌帧峰值**（`player+0x47778` 的最大值）——
 *       它就是"这套抖动能攒出多少无敌"的直接读数。
 *   ⚠ 2026-09-13 追加口径：**回放中 / 商店冻结帧 / 无自机不推进这两个数**（见 `ce_input_synth_frame_mask` 的门）。*/
static uint32_t s_frames_total;
static int      s_inv_peak;
/* ★ 2026-09-13 用户口径：「**能不能直接读那个帧数**——似乎**放 B 之后会把无敌帧重设为定值**」。
 *   ⇒ 我们读的就是引擎字段 `player+0x47778`，但它**会被引擎自己覆盖**（一手普查，全是 `mov …, 定值`）：
 *     `Bomb__can_bomb_and_deathbomb_check` `0x42067c`=120 / `0x420879`/`0x4215cc`/`0x4232cb`=40 /
 *     `0x423170`=110；`Card__death_save_*_revive` `0x40a477`/`0x40d9a5`=60；
 *     `Player__commit_death_and_enter_state2` `0x45d280`=180；`GlobalsInner__spend_power` `0x45c35e`=280。
 *     只有卡 29 那处是**加**（`0x40cb2a addss`，+4）。⇒ **攒起来的会被覆盖掉**。
 *   本组状态用来**抓这些覆盖**：每帧比对增量，凡不是「−1（自然衰减）」或「+3（本件触发 +4 再 −1）」
 *   的变化都判为**引擎重设**，在日志与 HUD 上标出来。
 *   ⚠ 回放中不检测（owner 追加口径）：那是回放注入的输入产生的状态。*/
static int      s_inv_prev;        /* 上一帧读到的 inv（-1 = 还没读到过）*/
static int      s_set_mark_frames; /* 检测到引擎重设后，HUD 上标几帧 */
static int      s_set_from, s_set_to;

/* 调试键：**空格（Space）** —— 用户 2026-09-13 口径「Ctrl+A 似乎没有生效，就用 Space 就行」。
 * ⚠ 与 106（`th06rank.c`）的 Space 调试键**同键**：同时装备 106 且按空格时两件会一起动
 *   （106 只是读键做别的事，不会抢我们的输入位）。要换键只改这一行。 */
static inline int input_synth_key_down(void)
{
    return (CE_DEBUG_SPACE_KEYS && ce_key_down(CE_VK_SPACE));
}

/* 调试键按下期间的状态复位 / 快照。只在"按下"那一帧调一次。 */
static void input_synth_begin(void)
{
    uint8_t *pl = CE_PLAYER();
    int     inv0 = 0;
    s_phase = 0;
    s_x0_fixed = 0;
    s_x0 = 0.0f;
    s_dxq_max = 0;
    if (pl) {
        s_x0_fixed = *(const int32_t *)(const void *)(pl + IS_OFF_PLAYER_X_FIXED);
        s_x0       = *(const float   *)(const void *)(pl + IS_OFF_PLAYER_X);
        inv0       = *(const int32_t *)(const void *)(pl + IS_OFF_PLAYER_INVULN);
    }
#if CE_INPUT_SYNTH_LOG_START_STOP
    ce_log("isynth: START Space — pattern 左%u/松%u/右%u/松%u (period %u), x0=%.1f (fixed %d), inv0=%d",
           (unsigned)CE_INPUT_SYNTH_LEFT_FRAMES, (unsigned)CE_INPUT_SYNTH_RELEASE_FRAMES,
           (unsigned)CE_INPUT_SYNTH_RIGHT_FRAMES, (unsigned)CE_INPUT_SYNTH_RELEASE_FRAMES,
           ce_input_synth_period(), (double)s_x0, (int)s_x0_fixed, inv0);
#else
    (void)inv0;
#endif
}

/* 松开调试键：打一行总结（读，不写）。相位归零 ⇒ 下次按下从"左"重新起。 */
static void input_synth_end(void)
{
    uint8_t *pl = CE_PLAYER();
    int32_t dxq = 0;
    int inv = 0;
    if (pl) {
        dxq = *(const int32_t *)(const void *)(pl + IS_OFF_PLAYER_X_FIXED) - s_x0_fixed;
        inv = *(const int32_t *)(const void *)(pl + IS_OFF_PLAYER_INVULN);
    }
#if CE_INPUT_SYNTH_LOG_START_STOP
    ce_log("isynth: STOP — 本次 %u 帧 / 累计 frames=%u ; net dx = %d/128 px (%.2f px), peak |dx| = %d/128 px, "
           "inv=%d (峰值 max=%d)  [累计量跨启停不清零]",
           s_phase, (unsigned)s_frames_total, (int)dxq, (double)dxq / 128.0, (int)s_dxq_max, inv, s_inv_peak);
#else
    (void)dxq; (void)inv;
#endif
    s_phase = 0;
    s_dxq_max = 0;
}

/* 节流日志：周期相位 / 这一帧写的方向位 / 位置 / **累计 Δx** / 峰值 |Δx| / 无敌计时器。 */
static void input_synth_log(uint32_t mask)
{
    uint8_t *pl = CE_PLAYER();
    int32_t dxq = 0;
    float   x = 0.0f, y = 0.0f;
    int     inv = 0;
    if (pl) {
        x   = *(const float   *)(const void *)(pl + IS_OFF_PLAYER_X);
        y   = *(const float   *)(const void *)(pl + IS_OFF_PLAYER_Y);
        inv = *(const int32_t *)(const void *)(pl + IS_OFF_PLAYER_INVULN);
        dxq = *(const int32_t *)(const void *)(pl + IS_OFF_PLAYER_X_FIXED) - s_x0_fixed;
        if (dxq > s_dxq_max)  s_dxq_max = dxq;
        if (-dxq > s_dxq_max) s_dxq_max = -dxq;
    }
    /* `mask` 是人读的方向记号：L / R / -（松开帧）*/
    const char *dir = (mask & CE_INPUT_SYNTH_DIR_LEFT)  ? "L"
                    : (mask & CE_INPUT_SYNTH_DIR_RIGHT) ? "R"
                    : (mask & CE_INPUT_SYNTH_DIR_UP)    ? "U"
                    : (mask & CE_INPUT_SYNTH_DIR_DOWN)  ? "D" : "-";
    ce_log("isynth: f=%u/%u dir=%s pos=(%.1f, %.1f) dx=%d/128px (%.2f) peak=%d/128 inv=%d",
           s_phase % ce_input_synth_period(), ce_input_synth_period(), dir,
           (double)x, (double)y, (int)dxq, (double)dxq / 128.0, (int)s_dxq_max, inv);
}

/* 每帧门：**不在回放中、不在商店冻结帧、且自机活着**。
 * 一手：
 *   · `zGameThread+0xd0` 非 0 = 回放中（`CE_GT_REPLAY_PLAYING`；商店 `0x417cd8` 也这么读）；
 *   · `0x4cf2a4` = `zAbilityShop*`（`engine.h` 的 `CE_ADDR_SHOP_PTR`，「敌人/弹幕/GUI 非 0 即冻结」）。
 *     ★ 2026-09-13 追加这一条的理由（**新落点特有**）：录制 tick 的 raw→HELD 拷贝与
 *     `compute_edges` 在 `0x462966` 一带**商店冻结帧照样跑到**（商店闸 `0x4629a2` 在站点**之后**），
 *     而商店里自机对象**不一定是 NULL** ⇒ 不加这一条，工具开着进商店会往商店菜单灌方向位。
 *     旧接法（卡 tick）天然不含商店帧（AbilityManager/Player 都被 `0x4cf2a4` 挡住）
 *     ⇒ 这里补上，语义与旧接法一致，也满足 owner「商店里不画」的口径。 */
static int input_synth_blocked(void)
{
    const uint8_t *gt = (const uint8_t *)CE_GAME_THREAD();
    if (!CE_PLAYER()) return 1;
    if (gt && *(const int32_t *)(const void *)(gt + CE_GT_REPLAY_PLAYING) != 0) return 1;
    if (*(const int32_t *)(uintptr_t)CE_ADDR_SHOP_PTR != 0) return 1;
    return 0;
}

/* ============================== 状态字（HUD） ==============================
 * 用户要的「生效时随便 plot 点什么」：在 HUD 上画一行 ASCII：
 *     SYNTH RUN L f=12 dx=-4.0 inv=37
 * 字段 = 本帧方向(L/R/-) · 相位帧号 · 起抖以来累计 Δx(px) · **无敌计时器 cur**（做卡 29 实验时看这个涨）。
 * ★ 2026-09-13 追加口径（owner）：「**然后在回放的时候，不画 gui**」⇒ **回放中 / 商店冻结帧 / 无自机不画**；
 *   其余时候**常驻**（停着也画，见下面 else 支）。
 * ⚠ 判据与写入侧、计数侧**同一个** `input_synth_blocked()`（三处语义对齐，见 input_synth.h 的
 *   `ce_input_synth_should_write` 注释）。
 * ⚠ 只画、不写任何引擎状态。
 * ⚠ `ce_hud_text` 的契约：**只能在虚表 +0x48 `on_draw` 里调** ⇒ 本函数只从 sdk.h 的 on_draw 桩进来。*/
void ce_input_synth_draw(void)
{
#if !CE_INPUT_SYNTH_ENABLED || !CE_INPUT_SYNTH_HUD
    /* 空壳：不画 */
#else
    static uint32_t s_draw_frame;
    const uint8_t *pl;
    int     inv = 0;
    int32_t dxq = 0;

    /* ★ 回放中 / 商店冻结帧 / 无自机 ⇒ **一个像素都不画**（owner 追加口径）。放在同帧去重之前：
     *   不画的时候也不必占用"本帧已画"这个名额。*/
    if (input_synth_blocked()) return;

    {
        const uint32_t f = ce_sdk_frame();
        if (s_draw_frame == f) return;            /* 同帧只画一次（每张卡的 on_draw 都会调到这里）*/
        s_draw_frame = f;
    }
    /* ★ 2026-09-13 用户口径：「**那个 UI 改成常驻**吧，**随着时间流逝无敌帧得减小**，做指示那个用」。
     *   ⇒ 不再只在生效时画：**平时每帧都画**，核心读数就是 `inv`（`player+0x47778`，无敌计时器 cur）
     *     —— 它会随每帧衰减自然往下掉，正好当"还剩多少无敌帧"的指示器。
     *   （回放/商店/无自机已在上面收掉。）*/
    pl = CE_PLAYER();
    if (pl) inv = *(const int32_t *)(const void *)(pl + IS_OFF_PLAYER_INVULN);
    if (s_set_mark_frames > 0) --s_set_mark_frames;

    if (s_active && pl) {
        /* 运行中：主读数 = 无敌帧（当前/峰值）+ 累计帧数；再给 方向/相位/累计 Δx */
        uint32_t mask;
        const char *dir;
        int inv_now = inv;
        if (inv_now > s_inv_peak) s_inv_peak = inv_now;   /* 峰值也在绘制侧兜一次（本帧 tick 可能没跑）*/
        dxq  = *(const int32_t *)(const void *)(pl + IS_OFF_PLAYER_X_FIXED) - s_x0_fixed;
        mask = ce_input_synth_mask(s_phase);
        dir  = (mask & CE_INPUT_SYNTH_DIR_LEFT) ? "L"
             : (mask & CE_INPUT_SYNTH_DIR_RIGHT) ? "R" : "-";
        if (s_set_mark_frames > 0)
            ce_hud_text(CE_INPUT_SYNTH_HUD_X, CE_INPUT_SYNTH_HUD_Y, CE_INPUT_SYNTH_HUD_BGRA_SET,
                        "SYNTH RUN %s inv=%d  << SET %d->%d (engine)  max=%d frames=%u",
                        dir, inv_now, s_set_from, s_set_to, s_inv_peak, (unsigned)s_frames_total);
        else
            ce_hud_text(CE_INPUT_SYNTH_HUD_X, CE_INPUT_SYNTH_HUD_Y, CE_INPUT_SYNTH_HUD_BGRA,
                        "SYNTH RUN %s inv=%d max=%d frames=%u dx=%.1f", dir, inv_now,
                        s_inv_peak, (unsigned)s_frames_total, (double)dxq / 128.0);
    } else {
        /* 停着也画（常驻指示器）：报无敌帧（当前/峰值）+ 累计帧数 + 怎么开 */
        if (inv > s_inv_peak) s_inv_peak = inv;
        if (s_set_mark_frames > 0)
            ce_hud_text(CE_INPUT_SYNTH_HUD_X, CE_INPUT_SYNTH_HUD_Y, CE_INPUT_SYNTH_HUD_BGRA_SET,
                        "SYNTH --  inv=%d  << SET %d->%d (engine)  max=%d frames=%u   [Space] toggle",
                        inv, s_set_from, s_set_to, s_inv_peak, (unsigned)s_frames_total);
        else
            ce_hud_text(CE_INPUT_SYNTH_HUD_X, CE_INPUT_SYNTH_HUD_Y, CE_INPUT_SYNTH_HUD_BGRA_IDLE,
                        "SYNTH --  inv=%d max=%d frames=%u   [Space] toggle", inv,
                        s_inv_peak, (unsigned)s_frames_total);
    }
#endif
}

/* ============================== 每帧入口 ==============================
 * 由站点 handler `BP_ce_input_synth`（`sdk.c`，站点 `ce_input_synth` = `th18:0x462966`）每帧调一次。
 * 本函数**不写任何引擎输入**：只回答"这一帧该写什么"（返回值 + `*write_out`），写由 handler 做。
 *
 * ★ 2026-09-13 owner 追加口径：**回放中 / 商店冻结帧 / 无自机 ⇒ 三处一起停**（写输入 / 画 HUD / 计数与检测）——
 *   回放里读到的是**回放注入的输入**产生的状态，算进我们的累计量会污染读数；
 *   判据就是 `input_synth_blocked()` 这一个（语义对齐见 input_synth.h 的 `ce_input_synth_should_write`）。
 */
static int      s_armed_logged;   /* ★ 只打一次：证明这件真的被链进 DLL 且每帧被调到 */
static uint32_t s_last_frame;     /* ★ 同帧去重：上一次真正跑过的帧号（新接法每帧只调一次，纯保险）*/
static int      s_key_prev;       /* ★ 空格的上一次电平（做"按下沿"用 ⇒ 空格是启停开关）*/
static int      s_pending_begin;  /* 开关打开时若正好在菜单/回放 ⇒ 等能写了再重取基线 */

uint32_t ce_input_synth_frame_mask(int *write_out)
{
    int      down, blocked;
    uint32_t mask;

    if (write_out) *write_out = 0;

    /* ★ 同帧去重：本函数由**每帧一次**的站点调；将来若有人再加第二个调用点，
     *   这一条保证相位不会被一帧推两次（模式全乱）。*/
    {
        const uint32_t f = ce_sdk_frame();
        if (s_last_frame == f) return 0;
        s_last_frame = f;
    }

    down    = input_synth_key_down();
    blocked = input_synth_blocked();

    /* ★★ 回放中 / 商店冻结帧 / 无自机 ⇒ **什么都不做**（owner 追加口径）：不写、不计数、不检测、不画
     *   （画那半边在 `ce_input_synth_draw()` 里用同一个门）。调试键电平照记，免得回放
     *   结束那一帧被当成一次"新按下"。*/
    if (blocked) {
        s_key_prev = down;
        if (s_active) s_pending_begin = 1;   /* 等能写了再重取基线（别把回放里的时间算进 dx）*/
        return 0;
    }

    /* ★ 引擎重设检测（**先于 running 判断**：停着时放 B 也要能看到；但回放中不检测，见上）*/
    {
        const uint8_t *pl = CE_PLAYER();
        if (pl) {
            const int inv = *(const int32_t *)(const void *)(pl + IS_OFF_PLAYER_INVULN);
            if (s_inv_prev >= 0) {
                const int d = inv - s_inv_prev;
                /* 允许的两种：−1 = 每帧自然衰减（`0x45c5bc`）；+3 = 本件触发（卡 +4 在玩家之前 ⇒ 本帧净 +3）*/
                if (d != -1 && d != 3) {
                    s_set_from = s_inv_prev;
                    s_set_to   = inv;
                    s_set_mark_frames = 90;      /* HUD 上标约 1.5 s */
                    ce_log("isynth: ★ INV 被**引擎重设**（多半是放 B / 复活 / 死亡结算）：%d -> %d (%+d)  "
                           "[一手：`Bomb__can_bomb_and_deathbomb_check` 0x42067c=120 / 0x420879|0x4215cc|0x4232cb=40 / "
                           "0x423170=110；death_save 60；death_commit 180；spend_power 280 —— 全是 `mov …, 定值`]",
                           s_set_from, inv, d);
                }
            }
            s_inv_prev = inv;
            if (inv > s_inv_peak) s_inv_peak = inv;
        }
    }

    if (!s_armed_logged) {
        s_armed_logged = 1;
        /* ★ 2026-09-13 用户报「Ctrl+A 没生效」⇒ 加这行：**不看按键**，第一帧就打，
         *   用来区分「代码没跑」和「键没被 GetKeyboardState 看到」这两种失败。*/
        ce_log("isynth: armed — 调试键 = 空格(Space)；模式 左%u/松%u/右%u/松%u (period %u)",
               (unsigned)CE_INPUT_SYNTH_LEFT_FRAMES, (unsigned)CE_INPUT_SYNTH_RELEASE_FRAMES,
               (unsigned)CE_INPUT_SYNTH_RIGHT_FRAMES, (unsigned)CE_INPUT_SYNTH_RELEASE_FRAMES,
               ce_input_synth_period());
    }

    /* ① ★★ **空格 = 启停开关**（用户 2026-09-13：「**空格是启停脚本**」）——
     *    抓**按下沿**（`down && !s_key_prev`）切换 `s_active`，**不再要求一直按住**。
     *    ⚠ `ce_key_down` 只给电平（`GetKeyboardState`）⇒ 边沿由我们自己的 `s_key_prev` 记。*/
    if (down && !s_key_prev) {
        if (s_active) { input_synth_end(); s_active = 0; }
        else          { input_synth_begin(); s_active = 1; s_pending_begin = 0; }
    }
    s_key_prev = down;

    /* ② 关着：**立刻停手、不返回要写的东西**（不是"写 0"，是 `write_out` 保持 0）。 */
    if (!s_active) return 0;

    /* ③ 开关打开时若正好在菜单/回放，等能写了再重取基线（否则 dx 会把商店/菜单里的时间也算进去）。*/
    if (s_pending_begin) { input_synth_begin(); s_pending_begin = 0; s_phase = 0; }

    /* ④ 这一帧的模式位 */
    mask = ce_input_synth_mask(s_phase);
    /* ★ 跨启停累加的计数（见 s_frames_total 的注释） */
    {
        const uint8_t *pl = CE_PLAYER();
        ++s_frames_total;
        if (pl) {
            const int inv = *(const int32_t *)(const void *)(pl + IS_OFF_PLAYER_INVULN);
            if (inv > s_inv_peak) s_inv_peak = inv;
        }
    }

    /* ⑤ 可观测：节流日志（含累计 Δx 与峰值 |Δx|，还有文那张卡的无敌计时器 cur）*/
#if CE_INPUT_SYNTH_LOG_EVERY > 0
    if ((s_phase % (unsigned)CE_INPUT_SYNTH_LOG_EVERY) == 0) input_synth_log(mask);
#endif

    ++s_phase;

    /* ⑥ ★ 本帧要写什么：**只有四个方向位**（0 = 这一帧松开方向 ⇒ handler 会把方向位抹掉）。
     *   handler（`sdk.c` 的 `BP_ce_input_synth`）负责"先抹后 OR"写进 `INPUT_HELD`。
     *   **不再手写 `INPUT_PRESSED`**：本站点在 `0x42abc0` 之前 ⇒ 引擎自己算边沿（文件头第 5 条）。*/
    if (write_out) *write_out = 1;
    return mask;
}

#endif /* CE_INPUT_SYNTH_ENABLED */
