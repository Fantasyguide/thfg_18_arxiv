/* hud_dim.c —— 「自机贴近 HUD 块 → 压暗」的**唯一实现**（纯逻辑）。取值与判据形状见 `hud_dim.h`。
 * 主机单测：`tests/test_hud_dim_core.c`（`make test-host`）。本文件不进引擎地址、不碰任何 ABI。*/
#include <stddef.h>          /* NULL（hud_card_present）*/
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

/* ★★ 2026-09-22：斜坡从「20 帧线性」改成「**8 帧线性 / 终点 64**」（`HUD_DIM_FRAMES` / `HUD_DIM_ALPHA_DIM`）。
 *
 * 为什么改：owner 拍板「th18左下角原生应该有动态吧，**照着那个填**」⇒ 以 **TH18 原生左下角那块「当前卡」UI**
 * 的动态为准。一手（见 `engine/card/th18/15-selected-card-hud.md` §4.2）：
 *   · 闸 = `AbilityManager__on_tick` `0x408640`：自机 `x < −64 ∧ y > 368` ⇒ `FUN_00488c60` `0x488c60` 发 `5`；
 *     离开（`y < 352 ∨ x > −48`）⇒ 同函数发 `4`；
 *   · 脚本 = `abcard.anm` script1–4：`interruptLabel(5): alphaTime(8, 0, 64)` / `interruptLabel(4): alphaTime(8, 0, 255)`。
 * ⇒ **原生帧数 8、原生终点 64**，本件照抄这两个数（C 侧与脚本侧同值）。
 *
 * 语义与 anm 的 `alphaTime(t, mode 0, target)` 一致（`engine/anm/th16/07-general.md` 的 op 409）：
 *   · **起步**：方向翻转那一帧记下当前 alpha 作起点（`from`）、帧数清零；
 *   · **每帧**：`alpha = from + (target − from) × t / FRAMES`（t = 1..FRAMES）⇒ 第 FRAMES 帧**精确**落到目标；
 *   · **稳态**：`t == FRAMES` 时恒等于目标（旧实现靠「钳位到目标」达到同一个效果）。
 * ⚠ 用「起点 + 帧计数」而不是固定步长：Δ = 255−64 = 191 在 8 帧下不是整数步长（191/8 = 23.875）
 *   ⇒ 固定步进写法到不了「恰好 8 帧」。这也是 `hud_dim_t` 有 `from` / `t` 两个字段的原因。*/
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

/* ══════════════════════════════════════════════════════════════════════════════════════════════
 * 左下角那块「当前（选中）主动卡」UI 的一手几何 + 让位（2026-09-22；一手见 `engine/card/th18/15-selected-card-hud.md`）
 * ══════════════════════════════════════════════════════════════════════════════════════════════ */

/* ★ 编译期把「共享件的统一值 = 原生那块的一手值」钉死。
 * 为什么要这一条：2026-09-22 的换基准（owner「照着原生那个填」）就是**从这块 UI 反推**出来的 ——
 *   64 / 8 帧 这两个数的唯一出处就是 `abcard.anm` script1–4。谁把共享件改回别的数，编译就红。*/
_Static_assert(HUD_DIM_ALPHA_DIM == HUD_CARD_ALPHA_DIM,
               "HUD_DIM_ALPHA_DIM 必须 = 原生「当前卡」UI 的压暗终点（abcard.anm script1-4 的 alphaTime(8,0,64)）");
_Static_assert(HUD_DIM_FRAMES == HUD_CARD_DIM_FRAMES,
               "HUD_DIM_FRAMES 必须 = 原生「当前卡」UI 的压暗帧数（abcard.anm script1-4 的 alphaTime(8,...)）");

void hud_card_rect_surface(float *x, float *y, float *w, float *h)
{
    if (x) *x = HUD_CARD_POS_X;
    if (y) *y = HUD_CARD_POS_Y;
    if (w) *w = HUD_CARD_W;
    if (h) *h = HUD_CARD_H;
}

void hud_card_rect_self(hud_dim_rect_t *r)
{
    /* 一手：`abcard.anm` script0 `pos(68, 924)` + `anchor(1, 2)`（左·底）+ `scale(0.75)`
     * ⇒ 块 = 表面 x 68..116 / y 864..924。换算定式同 `hud_dim_rect_surface()`。*/
    hud_dim_rect_surface(r, HUD_CARD_POS_X, HUD_CARD_POS_Y, HUD_CARD_W, HUD_CARD_H);
}

int hud_card_present(const void *selected_active)
{
    return selected_active != NULL;
}

void hud_card_shelf_x0(float *out, float anchor_x, float gap, int present)
{
    float right;
    if (!out) return;
    /* ⚠ **不许写成 `return 一个 float`**：i386 的 float 返回值走 st0（`flds`），而 `make dllx87`
     *   钉着「我们自己的目标文件里**零 x87**」（同 `hud_dim_rect_surface()` 用 out-param 的原因）。*/
    if (!present) { *out = anchor_x; return; }      /* 没有当前卡 ⇒ 那块整体不存在，不占地方 */
    right = HUD_CARD_POS_X + HUD_CARD_W + gap;      /* 有 ⇒ 货架起点不许压到它 */
    if (right <= anchor_x) { *out = anchor_x; return; }
    *out = right;
}
