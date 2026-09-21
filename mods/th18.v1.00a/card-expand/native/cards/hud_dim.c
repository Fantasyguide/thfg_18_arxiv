/* hud_dim.c —— 「自机贴近 HUD 块 → 压暗」的**唯一实现**（纯逻辑）。取值与判据形状见 `hud_dim.h`。
 * 主机单测：`tests/test_hud_dim_core.c`（`make test-host`）。本文件不进引擎地址、不碰任何 ABI。*/
#include "hud_dim.h"

void hud_dim_init(hud_dim_t *d)
{
    if (!d) return;
    d->dim = 0;
    d->alpha = HUD_DIM_ALPHA_FULL;      /* ★ 255：不复位成 0（否则开场 5 帧从全黑淡上来）*/
    d->from = HUD_DIM_ALPHA_FULL;       /* ★ 2026-09-21：过渡起点也置满亮 */
    d->t = HUD_DIM_FRAMES;              /* ★ 2026-09-21：**稳态**（= FRAMES）⇒ 第一帧不会凭空起斜坡 */
}

/* 表面 px 的块（左下角 + 宽高，高往上长）→ 自机坐标矩形。
 * 定式一手：世界层 `表面 = (640 + x×2, 32 + y×2)` ⇒ `自机 = 表面/2 − (320, 16)`（见 `hud_dim.h`；
 * ⚠ 过时的 `224` 记法 **不要用** —— 它把游玩区放到 640 空间的 32..416，与「左边缘 257」矛盾）。*/
void hud_dim_rect_surface(hud_dim_rect_t *r, float x, float y, float w, float h)
{
    if (!r) return;
    r->x0 = x * 0.5f - HUD_DIM_SURF_DX;
    r->x1 = (x + w) * 0.5f - HUD_DIM_SURF_DX;
    r->y1 = y * 0.5f - HUD_DIM_SURF_DY;              /* 下沿（表面 y 向下、锚点在底边）*/
    r->y0 = (y - h) * 0.5f - HUD_DIM_SURF_DY;        /* 上沿 */
}

int hud_dim_gate(int was_dim, const hud_dim_rect_t *r, const float *pp)
{
    float x, y, m;

    if (!pp) return 0;                                /* 没自机（菜单/商店/结算）⇒ 不在区内 */
    if (!r)  return was_dim ? 1 : 0;                  /* 没给矩形 ⇒ 保持（调用方的错，别乱切）*/
    x = pp[0];
    y = pp[1];
    /* 进 = 落在「块矩形**外扩 MARGIN(28)**」之内（= 玩家靠近就淡：块本身在游玩区下沿之下，站不进去）；
     * 出 = 离开「块矩形外扩 MARGIN + BAND = 44」之外 ⇒ 进/出两沿之差 = BAND(16) = 滞回带。
     * ⚠ 2026-09-15g 那版「进 = 矩形**之内**」已作废（父 agent 2026-09-15h 订正：一手的语义本来就是
     *    「块矩形 + 一个边距」24–32，取 28）。见 `hud_dim.h` 文件头。*/
    m = HUD_DIM_MARGIN + (was_dim ? HUD_DIM_BAND : 0.0f);
    if (x < r->x0 - m || x > r->x1 + m) return 0;
    if (y < r->y0 - m || y > r->y1 + m) return 0;
    return 1;
}

/* ★★ 2026-09-21：斜坡从「每帧固定 ±HUD_DIM_STEP」改成「**20 帧线性**」（`HUD_DIM_FRAMES`）。
 *
 * 为什么改：owner 2026-09-21 拍定「20 帧 / 50% 是 UI ⇒ 得统一成一个值，以哪个为基准都行」，
 * 父 agent 选**以 115 那套为基准**；而 20 帧本身有**一手**出处 —— TH15 `front.anm` script241
 * （章节面板父 VM）关面板时 `alphaTime(20, 0, 0)`（见
 * `engine/chapter/th15/02-chapter-panel-geometry-and-fades.md` §3）。
 *
 * 语义与 anm 的 `alphaTime(t, mode 0, target)` 一致（`engine/anm/th16/07-general.md` 的 op 409）：
 *   · **起步**：方向翻转那一帧记下当前 alpha 作起点（`from`）、帧数清零；
 *   · **每帧**：`alpha = from + (target − from) × t / FRAMES`（t = 1..FRAMES）⇒ 第 FRAMES 帧**精确**落到目标；
 *   · **稳态**：`t == FRAMES` 时恒等于目标（旧实现靠「钳位到目标」达到同一个效果）。
 * ⚠ 旧口径 4 帧（±56）的整数步进**做不到**精确 20 帧（Δ=127 ⇒ 步长 6 = 22 帧、7 = 19 帧）
 *   ⇒ 用「起点 + 帧计数」而不是步长。这也是 `hud_dim_t` 多出 `from` / `t` 两个字段的原因。*/
int32_t hud_dim_tick(hud_dim_t *d, const hud_dim_rect_t *r, const float *pp)
{
    int32_t was, target;

    if (!d) return HUD_DIM_ALPHA_FULL;
    was = d->dim;
    d->dim = hud_dim_gate(was, r, pp);
    target = d->dim ? HUD_DIM_ALPHA_DIM : HUD_DIM_ALPHA_FULL;
    if (d->dim != was) {                    /* 方向翻转：从**本帧** alpha 起算（alphaTime 语义）*/
        d->from = d->alpha;
        d->t = 0;
    }
    if (d->t < HUD_DIM_FRAMES) {
        d->t++;
        d->alpha = d->from + (target - d->from) * d->t / HUD_DIM_FRAMES;
    } else {
        d->alpha = target;                  /* 稳态：钉在目标上 */
    }
    return d->alpha;
}
