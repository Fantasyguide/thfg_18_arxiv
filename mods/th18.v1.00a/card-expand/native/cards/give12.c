/* 118「挑战：十二合一」—— 主动卡（D 切到本卡后按 C 发动）：一次把 106–117 十二张系统卡发到手上。
 *
 * 定位（用户 2026-09-17 口径）：正式构建也带，它是一个**挑战**卡 —— 挑战来自你自己决定
 * 把哪些系统机制叠在一起打。它同时是"106–117 全装"的总验收载体。
 *
 * ★ 语义克制（用户 2026-09-17 更正后的口径）：**只发卡，不改存档、不改卡组容量**。
 *   「初手卡组只有 3 格」是**初手那一张表**的格数，卡组本身不受它限制 ⇒ 玩家拿到卡后
 *   自己在编成里装上即可。（早先那条"必须写存档容量"的推论已被更正，见 COMPAT-MATRIX §7.1。）
 *
 * 发卡走**已有路径**（一手）：`cards/pot.c` 的 `ce_give_card(id, CE_MODE_SHOP, 0)`
 * —— 与商店刷出来、强欲之壶发出来的是同一条路（`sdk/public.h:208` 声明）。
 *
 * 幂等：一局只发一次（`s_run` 记住本局的纪元；`on_run_reset` 清零 ⇒ 重开一局可以再发）。
 * 收尾：本卡**不留跨帧资源**（没有自建池/VM/HUD 槽）⇒ `on_delete` 只需把本局标记清掉，
 * 满足 METHOD 的长持续效果铁律。
 *
 * 站点：**0 条新增**（主动卡走既有的 `on_activate` / `on_active_tick` 回调与 `ce_give_card` 函数调用）。
 */
#include "sdk.h"

#define GIVE12_FIRST 106u
#define GIVE12_LAST  117u

static int s_given_run;      /* 本局是否已发过（幂等）；on_run_reset/on_delete 清零 */
static int s_epoch = 1;      /* 简单纪元：run_reset 时 ++，用来区分"新的一局" */

static int ctor(ce_card_t *c)
{
    if (!c) return 0;
    ce_log("give12: ctor — 主动卡（D 切到本卡后按 C 发动，一次发 %u..%u 十二张）",
           GIVE12_FIRST, GIVE12_LAST);
    return 0;
}

static int on_activate(ce_card_t *c)
{
    unsigned id, ok = 0, fail = 0;

    if (!c) return 0;
    if (s_given_run == s_epoch) {
        ce_log("give12: 本局已经发过了（幂等）—— 不重复发");
        return 0;                            /* 发动成功但不重复发 */
    }
    s_given_run = s_epoch;
    for (id = GIVE12_FIRST; id <= GIVE12_LAST; ++id) {
        int r = ce_give_card(id, CE_MODE_SHOP, 0);
        if (r) ++ok; else ++fail;
        ce_log("give12: gave card %u -> %d", id, r);
    }
    ce_log("give12: 发动完成 —— %u 张成功 / %u 张失败（%u..%u）；拿到后请到编成里自行装上",
           ok, fail, GIVE12_FIRST, GIVE12_LAST);
    return 0;
}

static void on_run_reset(ce_card_t *c)
{
    (void)c;
    s_given_run = 0;
    ++s_epoch;                               /* 新的一局 ⇒ 允许再发一次 */
}

static void on_delete(ce_card_t *c)
{
    (void)c;
    /* 本卡不留跨帧资源；只把本局标记清掉（删卡在本局中途发生时，重挂后可再发）。*/
    s_given_run = 0;
    ce_log("give12: deleted — 本卡不收任何跨帧资源（无池 / 无 VM / 无 HUD 槽）");
}

CE_CARD(118, .active_recharge = 1,           /* 主动卡：D 切换 + C 发动 */
             .ctor = ctor, .on_activate = on_activate,
             .on_run_reset = on_run_reset, .on_delete = on_delete);
