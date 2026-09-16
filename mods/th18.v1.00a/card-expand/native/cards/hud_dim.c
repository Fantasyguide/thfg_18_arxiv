/* hud_dim.c —— 「自机贴近 HUD 块 → 压暗」的**唯一实现**（纯逻辑）。取值与判据形状见 `hud_dim.h`。
 * 主机单测：`tests/test_hud_dim_core.c`（`make test-host`）。本文件不进引擎地址、不碰任何 ABI。*/
#include "hud_dim.h"

void hud_dim_init(hud_dim_t *d)
{
    if (!d) return;
    d->dim = 0;
    d->alpha = HUD_DIM_ALPHA_FULL;      /* ★ 255：不复位成 0（否则开场 5 帧从全黑淡上来）*/
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

int32_t hud_dim_tick(hud_dim_t *d, const hud_dim_rect_t *r, const float *pp)
{
    int32_t target;

    if (!d) return HUD_DIM_ALPHA_FULL;
    d->dim = hud_dim_gate(d->dim, r, pp);
    target = d->dim ? HUD_DIM_ALPHA_DIM : HUD_DIM_ALPHA_FULL;
    if (d->alpha < target) {
        d->alpha += HUD_DIM_STEP;
        if (d->alpha > target) d->alpha = target;
    } else if (d->alpha > target) {
        d->alpha -= HUD_DIM_STEP;
        if (d->alpha < target) d->alpha = target;
    }
    return d->alpha;
}
