/* 黄昏（id 68）—— 用掉**最后一颗**炸弹时，那一发结束后自动再放一发。
 *
 * 不开断点、不改任何引擎字节：只在 on_tick_2 里盯炸弹管理器的「正在放」标志
 * （`[0x4cf2b8]+0x30`，do_bomb 置 1、can_bomb 查它）的两个边沿：
 *
 *   0→1  炸弹刚开始 —— 若此刻 CURRENT_BOMBS == 0，说明刚用掉的就是最后一颗 → 武装
 *   1→0  炸弹刚结束 —— 已武装就调引擎自己的 do_bomb() 放第二发，并标记「这发是接出来的」
 *   第二发结束 → 清标记。所以最多只接一次，不会无限连。
 *
 * 为什么安全：
 *   · 调的是引擎自己的 do_bomb，它所有守卫照常生效；被拦下只返回 -1，什么都不发生
 *   · 扣炸弹的 0x4574d0 自带钳 0 —— 第二发扣不出负数，我们**完全不碰** CURRENT_BOMBS
 *   · do_bomb 会读 PLAYER+0x620 当声像，所以自机指针为空时不调（本仓踩过的坑，见 LESSONS）
 *
 * 🟡 未验：+0x30 由谁在炸弹结束时清零。本做法对此免疫 —— 真清不了就是永远不接第二发，
 *    不会崩；日志 `dusk: …` 会直接说明。
 */
#include "sdk.h"

typedef struct {
    uint8_t prev;        /* 上一帧的「正在放炸弹」标志 */
    uint8_t armed;       /* 这一发用掉的是最后一颗，结束后要接 */
    uint8_t chained;     /* 正在放的这一发是我们接出来的 */
} dusk_state_t;

static int bombing(void)
{
    return CE_BOMB_ACTIVE() != 0;   /* engine.h 访问器：mgr 空指针安全 */
}

/* 复位：带着卡进入一发**正在放**的炸弹时，别把它当成新炸弹的 0→1 边沿 */
static void reset(ce_card_t *c)
{
    dusk_state_t *st = ce_state(c, dusk_state_t);
    if (!st) return;
    st->prev = (uint8_t)bombing();
    st->armed = 0;
    st->chained = 0;
}

static int on_load(ce_card_t *c) { reset(c); return 0; }
static void on_stage_start(ce_card_t *c) { reset(c); }
static void on_run_reset(ce_card_t *c) { reset(c); }

static int on_tick_2(ce_card_t *c)
{
    dusk_state_t *st = ce_state(c, dusk_state_t);
    if (!st) return 0;
    int b = bombing();

    if (b && !st->prev) {                       /* 炸弹刚开始 */
        if (!st->chained)                       /* 接出来的那发不再武装，否则会无限连 */
            st->armed = (uint8_t)(CE_CURRENT_BOMBS() == 0);
    } else if (!b && st->prev) {                /* 炸弹刚结束 */
        if (st->armed && !st->chained && CE_PLAYER()) {
            st->armed = 0;
            st->chained = 1;
            int r = ce_do_bomb();
            ce_log("dusk: last bomb spent -> chaining a second one (do_bomb -> %d)", r);
            if (r != 0) st->chained = 0;        /* 引擎拦下了：恢复，下次还能接 */
        } else {
            st->armed = 0;
            st->chained = 0;
        }
    }
    st->prev = (uint8_t)b;
    return 0;
}

CE_CARD(68, .on_load = on_load, .on_tick_2 = on_tick_2,
            .on_stage_start = on_stage_start, .on_run_reset = on_run_reset);
