/* 旋风斩（id 79）—— 被动：放一发炸弹时额外再放 X 次，X = ⌊当前火力⌋ − 1，代价是 X 点火力。
 *
 * 一次 4.00 火力的按 B = 四发炸弹连着炸完，火力落回 1.00。把火力换成炸弹输出的卡。
 *
 * 【为什么是 on_bomb_spent + on_tick_2 两段】
 * `do_bomb` `0x420360` 自己有守卫（`+0x30`「正在放」非零就拒绝），所以四发**不可能同帧放**，只能一发接一发：
 *   · `on_bomb_spent`（SDK 事件，断点 `0x4203bc`，`consume_bomb` 刚返回）—— 玩家那一发的时点。
 *     在这里算 X、当场扣火力、记下欠账。此刻 `+0x30` 已经是 1（`do_bomb` 先置标志再扣炸弹），
 *     所以同一帧的 `on_tick_2` 不会抢跑。
 *   · `on_tick_2` —— 每帧看「还在放吗」，不在放且还欠着就调引擎自己的 `do_bomb` 放下一发。
 *     与黄昏（68）接第二发是同一套机制，只是它接一发、我们接 X 发。
 *
 * 【火力的扣法】
 * `ce_spend_power` 是引擎自己的（`0x457480`，AUDIT §V）：火力 ≤ 一档时拒绝不扣、扣完钳到一档。
 * X = ⌊P⌋ − 1 这个定义**天然落在它的界内侧**：X > 0 时扣完必然剩 [1.00, 1.99]，那条拒绝分支永远碰不到
 * （单测 `test_cost_never_refused` 把 0..400 全枚举钉住了）。扣完照 Tsukasa / 炎魔无条件调 `repopulate`。
 *
 * 【为什么会自己停】
 * 扣完火力在 [1.00, 1.99]，⌊·⌋ − 1 = 0 —— 接出来的那几发再算 X 就是 0。`chaining` 那一位是第二道保险。
 *
 * 【额外那几发不花炸弹】
 * `do_bomb` 内部会扣炸弹数（`0x4574d0`，钳 0 不会变负）。这张卡的代价写在火力上，所以每接一发之后
 * 把炸弹数**精确补回**：只在它真的少了的时候补、且补完钳 MAX。这样与腐化（70）共存也不会双重补 ——
 * 腐化自己在 `on_bomb_spent` 里已经把当前数退回去了，我们看到「没少」就不动手。
 * ⚠️ 但腐化的「扣上限」对每一发都生效：旋风斩 + 腐化 = 一次按 B 烧掉 X+1 点炸弹上限。这是设定内的反协同。
 * 真银圣剑（75）同理：接出来的每一发都算一次挥砍。见 AUDIT §AE。
 *
 * 纯逻辑（X 的算法 + 接续状态机）在 whirlwind_core.c，主机单测。无随机、无浮点 ⇒ replay 天然安全。审计 AUDIT §AE。 */
#include "sdk.h"
#include "whirlwind_core.h"

static int bombing(void)
{
    return CE_BOMB_ACTIVE();   /* engine.h 访问器：mgr 判空 + 读 +0x30，同上游常量版语义（合并去重）*/
}

/* SDK 事件：`do_bomb` 里 `consume_bomb` 刚返回。玩家那一发在这里算账。 */
static void on_bomb_spent(ce_card_t *c)
{
    ww_state_t *s = ce_state(c, ww_state_t);
    int per, power, x, cost, changed;

    if (!s) return;
    per = CE_POWER_PER_LEVEL();
    power = CE_CURRENT_POWER();
    x = ww_on_bomb(s, power, per);
    if (!x) {
        if (!s->chaining)
            ce_log("whirlwind: bomb at power %d -> no extra release (need >= %d)", power, 2 * per);
        return;
    }
    cost = x * per;
    changed = ce_spend_power(cost);
    ce_repopulate_options();                   /* 照 Tsukasa `0x410e60` / 炎魔：扣完无条件重建子机并广播 */
    s->spent += (uint32_t)cost;
    ce_log("whirlwind: bomb at power %d -> %d extra release(s), power %d - %d = %d (level changed %d)",
           power, x, power, cost, CE_CURRENT_POWER(), changed);
}

/* +0x2c：上一发放完了就接下一发。调引擎自己的 do_bomb —— 它所有守卫照常生效，被拦下只返回 -1。 */
static int on_tick_2(ce_card_t *c)
{
    ww_state_t *s = ce_state(c, ww_state_t);
    int before, r;

    if (!s || !CE_PLAYER()) return 0;          /* do_bomb 会读 PLAYER+0x620 当声像（本仓踩过的坑）*/
    if (!ww_tick(s, bombing())) return 0;

    before = CE_CURRENT_BOMBS();
    r = ce_do_bomb();
    ww_fired(s, r == 0);
    if (r == 0 && CE_CURRENT_BOMBS() < before) {          /* 只补真的少掉的那一下（腐化已退过就不重复补）*/
        int max = CE_MAX_BOMBS();
        CE_CURRENT_BOMBS() = before > max ? max : before;
        ce_gui_update_bombs();
    }
    ce_log("whirlwind: extra release #%u (do_bomb -> %d), %u still owed, bombs %d",
           s->chained, r, s->left, CE_CURRENT_BOMBS());
    return 0;
}

/* 欠账不跨关、不跨局：带着「还欠 2 发」进下一关会在开场莫名其妙炸两下。 */
static void clear_state(ce_card_t *c)
{
    ww_state_t *s = ce_state(c, ww_state_t);
    if (s) ww_clear(s);
}

static int  on_load(ce_card_t *c)        { clear_state(c); return 0; }
static void on_stage_start(ce_card_t *c) { clear_state(c); }

static void on_run_reset(ce_card_t *c)
{
    ww_state_t *s = ce_state(c, ww_state_t);
    if (!s) return;
    if (s->chained) ce_log("whirlwind: run reset (%u extra releases, %u power spent)", s->chained, s->spent);
    ww_clear(s);
    s->chained = 0;
    s->spent = 0;
}

CE_CARD(79, .on_bomb_spent = on_bomb_spent, .on_tick_2 = on_tick_2,
            .on_load = on_load, .on_stage_start = on_stage_start, .on_run_reset = on_run_reset);
