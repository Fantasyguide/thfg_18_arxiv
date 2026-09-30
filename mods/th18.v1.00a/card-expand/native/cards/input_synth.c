/* input_synth.c —— 挂在调试键上的「逐帧合成方向输入」运行件。
 *
 * 用户原话（2026-09-27）：
 *   「写一个脚本挂在调试键上，**按照帧操作进行移动**，**松开方向键**，然后**平均意义上不要移动**」
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
 * 3. **帧内顺序：卡侧 tick 晚于引擎算边沿**（一手：engine/_shared/frame-loop.md §4.1/§4.4 +
 *    `0x461825 push 0x11`（ReplayManager）/ `0x408475 push 0x16`（AbilityManager）/
 *    `0x45a8aa push 0x17`（Player），注册表按节点 `+0x00` 升序：`0x11 → 0x16 → 0x17`）
 *    ⇒ 在 `0x16` 里写 `INPUT_HELD` **能**影响同帧的 Player tick（`0x17` 读它）。
 *    ⚠ 所以本件的写点必须是"每游玩帧、且在 Player tick 之前"的那一处（`input_synth.h` 之外的接线，
 *      见文件尾「接线」一节）。
 *
 * 4. **★「写 INPUT_HELD 会进 replay 录制」这条对卡侧 tick 不成立（订正）**
 *      录制读点 = `0x4629f7 mov ecx,[0x4ca428]` → `0x462a0d movzx eax,cx` → `0x462a19 call 0x463060`
 *      （`0x463060` 写 6 字节/帧），全在 **ReplayManager tick（节点 `0x11`）** 体内；
 *      而卡侧 tick / 任何节点 `> 0x11` 的钩子都在它**之后** ⇒ 我们写的位**不在**本帧录像里。
 *      下一帧 `0x46295a movzx eax,WORD PTR ds:0x4ca210`（raw 键盘）会把 `INPUT_HELD` 整个重写
 *      ⇒ 我们写的位**也不会顺延**到下一帧的录像。
 *      唯一会进录像的路径是**挂在 `0x462940` 函数体内**（`0x46295a` 之前写 raw / `0x462966` 之后写 HELD）
 *      —— 那需要一条新断点站点（`sites.py` / patch，共享件；且站点记账已 27 条 > `SITE_BUDGET` 24，见 engine/input/th18/01-key-mapping.md §5.3 的记账）。
 *      ⇒ **结论：本工具的合成输入是"现场有效、不进录像"**；其后果与规避见文件尾「⚠ replay 影响」。
 *
 * 5. **没有残留位**：`INPUT_HELD` 每帧被 `0x46295a`+`0x462961` 从 raw 键盘重写；
 *    `INPUT_PRESSED`/`INPUT_RELEASED` 每帧被 `0x462966 call 0x42abc0` 重算
 *    ⇒ 松开调试键后**不写就是没有**，不需要"清残留"（也不允许清：清了就等于键没按也写字节）。
 *    ⚠ 唯一的副作用：我们这一帧写的 HELD 会在下一帧开头被 `0x462950/0x462955` 存进
 *      `INPUT_HELD_PREV(0x4ca42c)` ⇒ `compute_edges` 会为这些位算出一条 **RELEASED**
 *      （`0x42ac52`–`0x42ac5b`），而它**会被录进录像**（`0x4629fd` 读 `0x4ca438`）。
 *      一手普查（engine/input/th18/01-key-mapping.md §2.3）里 `INPUT_RELEASED` **没有任何读者**
 *      ⇒ 只是录像流里多几个无害的位，不会单独造成失同步（真正的失同步源是"录像缺 held 位"，见下）。
 *
 * 6. **调试键**：`ce_key_down()` 走 `GetKeyboardState`（user32），不是游戏输入掩码
 *    （先例 `cards/animal.c:3875` 一带、`cards/ufo.c:5528`）。⇒ 调试键本身不进 replay 输入流。
 *    ⚠ **不选 Ctrl+Space**：那个组合**已经被占**（`cards/animal.c` 的 `ANIMAL_DEBUG_KEY` 与
 *      `cards/ufo.c` 的 `UFO_DEBUG_KEY` 都拿它当边沿触发）。本件用 **Ctrl+A**
 *      （**空格 `CE_VK_SPACE`** —— 用户口径；原先用 `Ctrl+A` 但实跑没生效；
 *      且 `A` 不在 TH18 的键盘配置表里（一手 engine/input/th18/01-key-mapping.md §1）
 *      也不是方向键 ⇒ 不会与任何原生行为或既有调试键撞车）。
 *
 * ============================== ⚠ replay 影响（必须知道） ==============================
 * 合成的方向**不进录像** ⇒ **用本工具录出来的 replay 会与现场不一致**（回放时自机不动，
 * 位置/擦弹/收点全对不上 ⇒ 若现场有随机数消费就会失同步）。
 * 规避：① 本件只在 **`CE_GT_REPLAY_PLAYING == 0`**（`GameThread+0xd0`，一手）
 *   也就是"不在回放中"时才工作；② 别把带这个工具的录像当证据 —— 它是**开发用**的现场工具。
 *
 * ============================== 接线（本文件不改共享件，接线由 owner 拍） ==============================
 * 本件只暴露一个入口：`ce_input_synth_tick()`（每游玩帧调一次、Player tick 之前）。
 * 两条可选接法（都不需要新断点站点）：
 *   (b) **常驻推荐**：`sdk.c` 的 `BP_ce_item_frame`（`th18:0x446ee3`，站点已存在、
 *       "每个正常游玩帧到一次"、在卡链遍历之前）里加一行 `ce_input_synth_tick();`
 *       （该 .c 顶部再加 `#include "cards/input_synth.h"`）。
 *       ⇒ 不依赖任何卡（原生卡 29 在场也能用，正是用户要测的场景）。
 *       代价：动共享件 `sdk.c`（**一处 include + 一行调用**）⇒ 按仓库纪律先报告、由 owner 落。
 *   (a) **卡侧**：在任一张我方卡的 `on_tick_2` 里加一行。不需要动共享件，
 *       但**只有该卡在卡组里时才生效** ⇒ 测原生卡 29 时要先把那张卡一起编进卡组。
 *       （本件刻意不做成"某张卡的私有状态"：它是每帧一次的全局工具，**没有任何跨帧私有状态**
 *         （只有相位/快照三个标量，按设计随键起落复位）⇒ **没有 `on_delete` 要收的东西**。）
 */
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
 * ⇒ 按住调试键期间，在 HUD 上画一行 ASCII 状态（`ce_hud_text`，**只能在 on_draw 里调**，
 *    所以绘制半边挂在 `sdk.h` 的 `on_draw` 桩上，见 `ce_input_synth_draw()`）。
 * ⚠ 字体是引擎的 ASCII 字体 ⇒ **只能用 ASCII**，别写中文。 */
#define CE_INPUT_SYNTH_HUD        1     /* 0 = 不画（发布版可关）。★ 现在是**常驻**：停着也画 */
#define CE_INPUT_SYNTH_HUD_X      180.0f  /* 640×480 字面量空间（Gui 那套，与 `ce_hud_text` 一致）*/
#define CE_INPUT_SYNTH_HUD_Y       40.0f  /* 压在游玩区上部：一定会被看见（调试件，临时遮挡可接受）*/
#define CE_INPUT_SYNTH_HUD_BGRA   0xff40ff40u  /* BGRA：亮绿（运行中）*/
#define CE_INPUT_SYNTH_HUD_BGRA_IDLE 0xff40c0ffu /* BGRA：淡黄（停着时也画 —— 常驻指示器）*/

/* 刻意**不**做"松开后再清一次位"的收尾：那会在键没按时也写字节（违反"默认一个字节都不写"），
 * 而引擎每帧重写 HELD/PRESSED ⇒ 本来就没有残留（见文件头第 5 条）。 */

#if !CE_INPUT_SYNTH_ENABLED

/* 发布版：整件是空壳（保证"不按调试键时一个字节都不写"是编译期事实，不是运行期判断）。 */
void ce_input_synth_tick(void) { }

#else   /* ------------------------------ 下面是真正的工作体 ------------------------------ */

static unsigned s_phase;      /* 按下调试键后的第几帧（0 = 按下的那一帧）*/
static int      s_active;     /* 上一帧调试键是否按着（用来抓 按下 / 松开 的边沿）*/
static int32_t  s_x0_fixed;   /* 起抖时的 x（1/128 px 定点）；用来算累计 Δx */
static float    s_x0;         /* 起抖时的 x（float, px）；只给日志看 */
static int32_t  s_dxq_max;    /* 起抖以来 |Δx| 的峰值（1/128 px 定点）—— "平均不移动"一眼判据 */

/* 调试键：**空格（Space）** —— 用户 2026-09-13 口径「Ctrl+A 似乎没有生效，就用 Space 就行」。
 * ⚠ 与 106（`th06rank.c`）的 Space 调试键**同键**：同时装备 106 且按空格时两件会一起动
 *   （106 只是读键做别的事，不会抢我们的输入位）。要换键只改这一行。 */
static inline int input_synth_key_down(void)
{
    return ce_key_down(CE_VK_SPACE);
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
    ce_log("isynth: STOP Space after %u frames — net dx = %d/128 px (%.2f px), peak |dx| = %d/128 px, inv=%d",
           s_phase, (int)dxq, (double)dxq / 128.0, (int)s_dxq_max, inv);
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

/* 每帧门：不在回放中、且自机活着。一手：`zGameThread+0xd0` 非 0 = 回放中
 * （`CE_GT_REPLAY_PLAYING`；商店 `0x417cd8` 也这么读）。 */
static int input_synth_blocked(void)
{
    const uint8_t *gt = (const uint8_t *)CE_GAME_THREAD();
    if (!CE_PLAYER()) return 1;
    if (gt && *(const int32_t *)(const void *)(gt + CE_GT_REPLAY_PLAYING) != 0) return 1;
    return 0;
}

/* ============================== 状态字（HUD） ==============================
 * 用户要的「生效时随便 plot 点什么」：按住调试键期间画一行 ASCII：
 *     SYNTH L f=12 dx=-4.0 inv=37
 * 字段 = 本帧方向(L/R/-) · 相位帧号 · 起抖以来累计 Δx(px) · **无敌计时器 cur**（做卡 29 实验时看这个涨）。
 * ⚠ 只画、不写任何引擎状态；没按键（或门挡着）时**什么都不画**。
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

    {
        const uint32_t f = ce_sdk_frame();
        if (s_draw_frame == f) return;            /* 同帧只画一次（每张卡的 on_draw 都会调到这里）*/
        s_draw_frame = f;
    }
    /* ★ 2026-09-13 用户口径：「**那个 UI 改成常驻**吧，**随着时间流逝无敌帧得减小**，做指示那个用」。
     *   ⇒ 不再只在生效时画：**每帧都画**，核心读数就是 `inv`（`player+0x47778`，无敌计时器 cur）
     *     —— 它会随每帧衰减自然往下掉，正好当"还剩多少无敌帧"的指示器。*/
    pl = CE_PLAYER();
    if (pl) inv = *(const int32_t *)(const void *)(pl + IS_OFF_PLAYER_INVULN);

    if (s_active && pl) {
        /* 运行中：多给 方向 / 相位 / 累计 Δx（看"平均不移动"）*/
        uint32_t mask;
        const char *dir;
        dxq  = *(const int32_t *)(const void *)(pl + IS_OFF_PLAYER_X_FIXED) - s_x0_fixed;
        mask = ce_input_synth_mask(s_phase);
        dir  = (mask & CE_INPUT_SYNTH_DIR_LEFT) ? "L"
             : (mask & CE_INPUT_SYNTH_DIR_RIGHT) ? "R" : "-";
        ce_hud_text(CE_INPUT_SYNTH_HUD_X, CE_INPUT_SYNTH_HUD_Y, CE_INPUT_SYNTH_HUD_BGRA,
                    "SYNTH RUN %s f=%u dx=%.1f INV=%d", dir, (unsigned)s_phase,
                    (double)dxq / 128.0, inv);
    } else {
        /* 停着也画（常驻指示器）：只报无敌帧 + 怎么开 */
        ce_hud_text(CE_INPUT_SYNTH_HUD_X, CE_INPUT_SYNTH_HUD_Y, CE_INPUT_SYNTH_HUD_BGRA_IDLE,
                    "SYNTH -- INV=%d   [Space] toggle", inv);
    }
#endif
}

/* ============================== 每帧入口 ============================== */
static int s_armed_logged;     /* ★ 只打一次：证明这件真的被链进 DLL 且每帧被调到 */
static uint32_t s_last_frame;  /* ★ 同帧去重：上一次真正跑过的帧号 */
static int      s_key_prev;    /* ★ 空格的上一次电平（做"按下沿"用 ⇒ 空格是启停开关）*/
static int      s_pending_begin; /* 开关打开时若正好在菜单/回放 ⇒ 等能写了再重取基线 */
void ce_input_synth_tick(void)
{
    /* ★ 同帧去重：本函数挂在**每张卡的 tick 桩**上（见 `sdk.h` 的 `ce_s_##ID##_on_tick_2`），
     * 同帧会被调 N 次（N = 在场的我方卡数）⇒ 用 `ce_sdk_frame()` 只认第一次，
     * 否则相位会在一帧里被推进 N 次（模式全乱）。
     * ⚠ 帧号在 `BP_ce_item_frame`（ItemManager = 0x1e）里自增，**晚于**卡节点（0x16）
     *   ⇒ 同一卡节点内多张卡读到的帧号相同 ✓ 去重成立。*/
    {
        const uint32_t f = ce_sdk_frame();
        if (s_last_frame == f) return;
        s_last_frame = f;
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
    const int down = input_synth_key_down();

    /* ① ★★ **空格 = 启停开关**（用户 2026-09-13：「**空格是启停脚本**」）——
     *    抓**按下沿**（`down && !s_key_prev`）切换 `s_active`，**不再要求一直按住**。
     *    ⚠ `ce_key_down` 只给电平（`GetKeyboardState`）⇒ 边沿由我们自己的 `s_key_prev` 记。*/
    if (down && !s_key_prev) {
        if (s_active) { input_synth_end(); s_active = 0; }
        else          { input_synth_begin(); s_active = 1; s_pending_begin = 0; }
    }
    s_key_prev = down;

    /* ② 关着：**立刻停手、一个字节都不写**（不是"写 0"，是这里根本没有那两句）。 */
    if (!s_active) return;

    /* ③ 门：回放中 / 没有自机（菜单、结算）⇒ 不写，但**保持"运行中"这个状态**（用户是开关语义）。
     *    等能写了再重取基线（否则 dx 会把商店/菜单里的时间也算进去）。*/
    if (input_synth_blocked()) { s_pending_begin = 1; return; }
    if (s_pending_begin) { input_synth_begin(); s_pending_begin = 0; s_phase = 0; }

    /* ④ 这一帧的模式位 */
    const uint32_t mask = ce_input_synth_mask(s_phase);

    /* ⑤ 落笔：**接管方向**（先抹掉四个方向位，再 OR 我们的），其余位（Z/X/Shift/C/D…）原样保留。
     *    · 抹掉是必要的：玩家此刻若真按着方向键，引擎在 `0x462961` 已经把那个位写进 HELD，
     *      不抹就会和我们的模式叠加 ⇒ 结果不确定（工具要的是可复现的输入）。
     *    · HELD 与 PRESSED **两个都写**：引擎的边沿是 `0x462966` 在节点 `0x11` 算的，
     *      我们在节点 `0x16` ⇒ 不写 PRESSED 就没有"这一帧刚按下"的边沿（文件头第 3 条）。
     *      PRESSED 写方向位不会误伤任何消费者：方向位在 PRESSED 里**没有读者**
     *      （一手普查 engine/input/th18/01-key-mapping.md §2.3：读 PRESSED 的只有
     *        `0x45c084`(C,0x400) / `0x45c09c`(D,0x800) / `0x45c048`(X,0x002) / `0x440a6b`(0x80001)，
     *        与 0x0F0 不相交）。 */
    CE_INPUT_HELD() = (CE_INPUT_HELD() & ~(uint32_t)CE_INPUT_SYNTH_DIR_MASK) | mask;
    *(volatile uint32_t *)(uintptr_t)CE_ADDR_INPUT_PRESSED =
        (*(volatile const uint32_t *)(uintptr_t)CE_ADDR_INPUT_PRESSED & ~(uint32_t)CE_INPUT_SYNTH_DIR_MASK) | mask;

    /* ⑥ 可观测：节流日志（含累计 Δx 与峰值 |Δx|，还有文那张卡的无敌计时器 cur）*/
#if CE_INPUT_SYNTH_LOG_EVERY > 0
    if ((s_phase % (unsigned)CE_INPUT_SYNTH_LOG_EVERY) == 0) input_synth_log(mask);
#endif

    ++s_phase;
}

#endif /* CE_INPUT_SYNTH_ENABLED */
