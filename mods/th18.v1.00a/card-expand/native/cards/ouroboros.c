/* 衔尾蛇（id 78）—— 被动：每 miss X 次，自机弹伤害永久 +20％（加法），然后 X += 1；X 初值 1。
 * 咬住自己尾巴的蛇：每一次死亡都喂大下一次的自己，但下一口越来越难吃到。
 *
 * 三件事，都不新开断点、不改任何字节：
 *   · 计数：`on_death_after_deathbomb`（+0x0c）—— 决死窗口已经过去、这一次是**真的 miss**（加倍 69 用的同一个信号）。
 *     返回 0 = 不救命，只记账（零售 `CardTewi` 就是这么在这个槽里只记账的）。
 *   · 加伤害：报给 **SDK 的自机弹伤害层**（`ce_set_bullet_damage_permille`，AUDIT §AA）。层负责就地乘
 *     `bullet+0x9c`（单次取整、与卡顺序无关）并把每敌每帧总伤害上限 `player+0x47984` 按同一倍率抬起来——
 *     叠到 ×1.6 之后不抬上限基本等于没加。边界：只管自机弹，炸弹与 `ce_damage_rect` 的伤害源不在内。
 *   · HUD：虚表 `+0x44`（`on_hud_anm`）拿到本卡 HUD 图标的根 VM，在它右下角起一个数字 VM 画当前层数
 *     （真银圣剑 script99 / TS_DIGIT_0..9 同一套，AUDIT §X；2026-09-08 起两张卡共用 SDK 的 `ce_hud_digit_*`）。
 *
 * 「永久」= 一整局之内：`on_run_reset`（+0x4c，局末）清零，倍率由 SDK 在同一个桩里回到声明的 ×1。
 * 纯逻辑在 ouroboros_core.c（主机单测）。无随机、无浮点 ⇒ replay 天然安全。审计 AUDIT §AB。 */
#include "sdk.h"
#include "ouroboros_core.h"

#define OB_SE_STACK   0x0d          /* se_powerup：叠一层时那一下 */

typedef struct {
    ob_state_t     st;
    ce_hud_digit_t hud;             /* HUD 小卡上的层数数字（SDK 原语，真银圣剑同一套）*/
} ob_card_t;

/* ctor：每关开场引擎会对卡组每张卡再调一次，所以这里必须幂等——只在还没初始化时铺初值。 */
static int ctor(ce_card_t *c)
{
    ob_card_t *s = ce_state(c, ob_card_t);
    if (s && !s->st.need) ob_reset(&s->st);
    return 0;
}

/* +0x0c：决死窗口过去了、这一次是真的 miss。返回 0 = 不救命。 */
static int on_death_after_deathbomb(ce_card_t *c, uint32_t acc)
{
    ob_card_t *s = ce_state(c, ob_card_t);
    (void)acc;
    if (!s) return 0;
    if (ob_on_miss(&s->st)) {
        ce_set_bullet_damage_permille(c, ob_permille(&s->st));
        ce_play_sound(OB_SE_STACK, 0.0f);
        ce_log("ouroboros: miss #%u -> stack %u (bullet damage x%d.%02d), next stack needs %u more miss(es)",
               s->st.misses, s->st.stacks, ob_permille(&s->st) / 1000, ob_permille(&s->st) % 1000 / 10, s->st.need);
    } else {
        ce_log("ouroboros: miss #%u -> %u/%u toward stack %u", s->st.misses, s->st.progress, s->st.need, s->st.stacks + 1);
    }
    ce_hud_digit_set(&s->hud, (int)s->st.stacks, "ouroboros");
    return 0;
}

/* +0x44：引擎建好本卡的 HUD 图标（也在图标重建时再调一次）。 */
static int on_hud_anm(ce_card_t *c, uint32_t id)
{
    ob_card_t *s = ce_state(c, ob_card_t);
    if (!s) return 0;
    ce_hud_digit_attach(&s->hud, id, (int)s->st.stacks, "ouroboros");
    return 0;
}

/* +0x2c：数字 VM 会随 HUD 重建而消失，每帧对一次账（只有指针变了才真动手）。 */
static int on_tick_2(ce_card_t *c)
{
    ob_card_t *s = ce_state(c, ob_card_t);
    if (s && s->hud.icon_id) ce_hud_digit_set(&s->hud, (int)s->st.stacks, "ouroboros");
    return 0;
}

/* +0x4c：局末。倍率回 ×1 由 SDK 在同一个桩里做（回到 CE_CARD 声明的初值），这里只清自己的账。 */
static void on_run_reset(ce_card_t *c)
{
    ob_card_t *s = ce_state(c, ob_card_t);
    if (!s) return;
    if (s->st.misses) ce_log("ouroboros: run reset (%u misses, %u stacks)", s->st.misses, s->st.stacks);
    ce_hud_digit_clear(&s->hud);
    ob_reset(&s->st);
}

/* ★ 2026-09-10：同真银圣剑 —— 卡被局中删掉（倒吊人，商店里成交）时把 HUD 数字收掉，
 * 否则那串层数会留在屏幕上（数字是独立 VM，HUD 图标重建带不走它）。
 * 倍率不用管：`opdelete` 会 `ce_dmg_set(id, 0)` 把它退出聚合。*/
static void on_delete(ce_card_t *c)
{
    ob_card_t *s = ce_state(c, ob_card_t);
    if (s) ce_hud_digit_clear(&s->hud);
}

CE_CARD(78, .ctor = ctor, .on_death_after_deathbomb = on_death_after_deathbomb,
            .on_hud_anm = on_hud_anm, .on_tick_2 = on_tick_2, .on_run_reset = on_run_reset,
            .on_delete = on_delete,
            .bullet_damage_permille = OB_BASE_PERMILLE);   /* 起手 ×1，每叠一层报一次新值 */
