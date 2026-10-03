/* elytra_mace_core.h —— 鞘翅重锤（id 91）的纯逻辑：Undertale 式**时机条**状态机。
 * 不碰引擎（不读按键、不写坐标、不建 VM）⇒ 主机 `make test-host` 能直接测。
 *
 * ## 规则（owner 2026-10-02 逐字）
 * 「发动之后会进入无敌状态，并有一个类似于 Undertale 的时机条；当有敌人可攻击的时候，
 *   时机条会开始动，到中间时机的时候需要再按下 C；让自机瞬移 or 快速直线移动到敌人身边，
 *   重锤砸下，造成高额伤害，然后回到原位。当 3s 没有可攻击的敌人或者时机没卡准的时候，
 *   自动退出，会再给你一点点无敌帧作为缓冲。」
 *
 * ## 状态机
 * ```
 * IDLE ──(on_activate → em_begin)──▶ AIM ──(C 边沿 && 光标在窗口内)──▶ SMASH ──(动画走完)──▶ EXIT ──▶ IDLE
 *                                     │                                                      ▲
 *                                     ├──(C 边沿 && 光标在窗口外)── 判定失败 ─────────────────┤
 *                                     ├──(连续 3s 没有可攻击的敌人)──────────────────────────┤
 *                                     └──(瞄准总预算 3s 到)───────────────────────────────────┘
 * ```
 * · **光标只在有可攻击的敌人时才动**（owner 那句「当有敌人可攻击的时候，时机条会开始动」）；
 *   没有敌人时它**停在原地**，同时 `no_target` 计时器在跑。
 * · `AIM` 里 `armed` 的语义：**激活那一帧的 C 边沿必须丢掉** —— 引擎的 `on_activate` 就是被那一帧的
 *   C 触发的，若不丢，卡片会在同一次按键上"自己确认自己"。
 * · 退出（无论哪种原因）都进 `EXIT`，它只负责**缓冲无敌帧**（owner：「再给你一点点无敌帧作为缓冲」）。
 *
 * ## ❓ 自定（登记 `TUNING.md`）
 * 3s = 180 帧（owner 给了秒数）；光标速度 / 判定窗口 / 砸击动画帧数 / 缓冲帧数都是我拍的。 */
#pragma once
#include <stdint.h>

/* —— owner 给的（秒数换算成帧 @60fps）—— */
#define EM_AIM_FRAMES        180      /* 3s：瞄准总预算（超时自动退出）*/
#define EM_NO_TARGET_FRAMES  180      /* 3s：连续没有可攻击的敌人就退出 */

/* —— 本卡旋钮（❓ 自定） —— */
#define EM_BAR_HALF          80.0f    /* 时机条半宽（px，640×480 空间；贴图 160 宽 ⇒ 与之一致）*/
#define EM_WINDOW            22.0f    /* 判定窗口半宽（px）—— 与贴图中间那条亮带同宽。
                                       * ★ 2026-10-03 owner 实跑：光标离中心 **16 px** 被判 MISS
                                       *   （窗口只有 ±15）⇒「差 1px 就没了」✗ ⇒ 15 → 22
                                       *   （≈0.23 s 的窗口；美术 `make_bar_art.py` 同步加宽到 22）。*/
#define EM_CURSOR_SPEED      3.2f     /* 光标速度（px/帧；单程 160/3.2 = 50 帧）*/
#define EM_SMASH_FRAMES      18       /* 砸下 → 起身的动画帧数（这期间自机在敌人身上）*/
#define EM_EXIT_INVULN       30       /* 退出后的缓冲无敌帧（0.5 s）*/
#define EM_SMASH_DMG         800      /* 那一锤的伤害（❓ 高额；进 TUNING 调）*/
#define EM_SMASH_W           140.0f   /* 判定盒（以目标敌人为中心）*/
#define EM_SMASH_H           140.0f
#define EM_SMASH_OFFSET_Y    (-46.0f) /* 自机停在敌人**上方**多少 px（砸击位）*/
#define EM_TARGET_RANGE      520.0f   /* 「可攻击」的半径（在屏内且够近）❓ 自定 */

typedef enum {
    EM_IDLE  = 0,   /* 不在任何状态（不声明无敌）*/
    EM_AIM   = 1,   /* 时机条在动 / 等确认 */
    EM_SMASH = 2,   /* 已瞬移过去、锤子正在砸 */
    EM_EXIT  = 3    /* 退出后的缓冲无敌帧 */
} em_phase_t;

typedef enum {
    EM_EV_NONE    = 0,
    EM_EV_HIT     = 1,   /* ★ 卡准了：现在瞬移到目标、开砸 */
    EM_EV_MISS    = 2,   /* 按早了/按晚了：进入 EXIT */
    EM_EV_TIMEOUT = 3,   /* 3s（没有敌人 / 总预算）到：进入 EXIT */
    EM_EV_RETURN  = 4,   /* 砸完：把自机送回原位 */
    EM_EV_DONE    = 5    /* 缓冲也走完了：一切都收掉 */
} em_event_t;

typedef struct {
    int32_t  state;        /* em_phase_t */
    uint32_t frames;        /* 当前状态已过帧数 */
    uint32_t no_target;     /* AIM 里连续没有可攻击敌人的帧数 */
    float    cursor;        /* 光标位置（−EM_BAR_HALF..+EM_BAR_HALF；0 = 正中）*/
    float    dir;           /* 光标方向（+1 往右）*/
    uint8_t  armed;         /* 1 = 可以接受 C 确认（激活那一帧之后才置）*/
    uint32_t hits, misses, timeouts;   /* 诊断计数 */
} em_state_t;

void em_reset(em_state_t *s);
/* `on_activate` 里调：进入 AIM、光标从左端出发、丢掉第一帧的 C。*/
void em_begin(em_state_t *s);
/* 每帧一次。`has_target` = 场上有可攻击的敌人；`c_edge` = C 键上升沿（**原始**边沿）。
 * 返回值 = 本帧要处理的事件（EM_EV_NONE = 什么都不用做）。*/
int  em_tick(em_state_t *s, int has_target, int c_edge);
/* 现在还需声明的无敌帧数（卡侧每帧 `ce_invuln_request(id, em_invuln_left(&st))`）。*/
int  em_invuln_left(const em_state_t *s);
/* 光标在贴图上的相对位置（−1..+1，卡侧拿它算屏幕坐标）。
 * ★ **必须 static inline**：i386 ABI 里返回 float 要走 st(0)，写成 .c 里的普通函数就会把 x87
 *   带进我们的目标文件（`make dllx87` 硬门；与 `bc_atan2f` 同一条理由）。*/
static inline float em_cursor_norm(const em_state_t *s)
{
    if (!s || EM_BAR_HALF <= 0.0f) return 0.0f;
    return s->cursor / EM_BAR_HALF;
}
int  em_in_window(const em_state_t *s);      /* 光标此刻在不在判定窗口里（诊断 / 贴图染色用）*/
