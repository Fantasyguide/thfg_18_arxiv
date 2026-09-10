/* 豌豆射手（id 77）—— 装备卡，致敬《植物大战僵尸》。
 *
 * 身边一株豌豆射手子机，**朝正上方**吐豌豆；平时按 PVZ 原版的射速（1.425 s ≈ 85 帧）一颗一颗地打，
 * 每隔 20–30 秒「开大」一次：变成 PVZ 的机枪射手，100 帧里泼出 20 排 × 7 = 140 颗小豌豆。
 *
 * 这是本 mod 第一张真正走 **SHT shooterset** 那条链的卡（`assets/sht/append_shooterset.py` 造好之后
 * 一直没人用：破损核心第一版走过、后来改成定点伤害源，因为它要的是「瞬发单体」）。链路：
 *   ① `on_power_level_change`（+0x18）里 `Player__allocate_option` 要一个子机槽 —— 与破损核心同款，
 *      引擎管它的位置 / 聚焦位移 / 进店收起；指针存 `ce_state()`（对象只有 0x54 字节，+0x54 在对象外）。
 *      认领 / 申请 / 收起三件套是 SDK 的 `ce_option_*`（2026-09-08 从这里与破损核心提炼上去）。
 *   ② `on_tick_shooters`（+0x1c，只在按住射击键时广播）里把子机 + 一组 shooterset 索引交给
 *      `Player__tick_shooters_for_ability_card`，由它按 `timer % fire_rate == start_delay` 出膛。
 *      **short_timer 我们自己数**：引擎那个（`player+0x476d4`）松开射击键会跳，相位不可控。
 *   ③ 弹的贴图也在 `ability.anm`（开火期间引擎把 `player+0x10` 换成它），所以 shooter 的 anm 字段
 *      填 `CE_ANM_ABILITY_SCRIPT_PEA` / `_PEA_SMALL`，四个 `pl0X.anm` 一个字节都不动。
 *
 * 每敌每帧伤害上限：平时那颗 130 装不进 Reimu 的 90，所以有豌豆在途的帧把它抬到 PEA_CAP
 * （零售 `CardRemilia__on_tick_2` `0x40f48e` 就是这么干的）。开大那排 7 × 7 = 49 反而不用抬。
 *
 * 纯逻辑（节奏状态机）在 peashooter_core.c，主机单测。审计 AUDIT §AJ。 */
#include "sdk.h"
#include "peashooter_core.h"

#define PS_OPTION_OFFSET  0x28      /* 子机相对自机的横向偏移；破损核心用 0x18，错开以便两张同时装 */
#define PS_RETRY_FRAMES   60        /* 没拿到子机时的重试间隔（槽满 / 关卡中途买到）*/
#define PS_SE_BURST       0x0d      /* se_powerup：开大起手那一下 */
#define PS_BURST_LAYER    15        /* script103 自己写的层，spawn 时给同一个 */

typedef struct {
    ce_option_t opt;                /* 子机槽（SDK：认领 / 申请 / 收起）*/
    pea_state_t st;
} ps_card_t;

/* +0x18：开局 / 火力档变 / 关店后广播，池刚被重建过 —— 无条件重新申请，与零售装备卡一致。 */
static int on_power_level_change(ce_card_t *c)
{
    ps_card_t *s = ce_state(c, ps_card_t);
    if (s) ce_option_alloc(c, &s->opt, PS_OPTION_OFFSET, CE_ANM_ABILITY_SCRIPT_PEASHOOTER_OPT, "peashooter");
    return 0;
}

/* +0x4c / +0x20：照 `CardReimu1__method_4C` `0x40aad0` 与 `____on_load__2` `0x40aab0`。
 * 节奏一起清零 —— 局末重开一局不该继承上一局攒了多久。 */
static void on_run_reset(ce_card_t *c)
{
    ps_card_t *s = ce_state(c, ps_card_t);
    ce_option_drop(c, s ? &s->opt : 0);      /* 删子机 VM + 清零售在此清的 flags bit1 */
    if (s) pea_clear(&s->st);
}

/* ★ 2026-09-10：卡被局中删掉（倒吊人，商店里成交）时也要**把子机还回池子** ——
 * `on_run_reset` 要等局末，而在商店里 `AbilityManager::on_tick` 冻着，`on_load` 也不会再来。
 * 不收的后果不只是画面：`Player__allocate_option` 的 12 格槽**没有别的回收者**，
 * 每漏一次就少一格（日志会是 `option allocated FAILED (pool full)`）。*/
static void on_delete(ce_card_t *c)
{
    ps_card_t *s = ce_state(c, ps_card_t);
    ce_option_drop(c, s ? &s->opt : 0);
}

static int on_load(ce_card_t *c)
{
    on_run_reset(c);
    return 0;
}

/* +0x2c：AbilityManager 每帧（菜单 / 商店里不跑；优先级 0x16，在 GameThread 复位上限 0x10 之后、
 * 敌人结算 0x1b 之前 —— 所以在这里抬上限只对当帧生效，不用还原）。 */
static int on_tick_2(ce_card_t *c)
{
    ps_card_t *s = ce_state(c, ps_card_t);
    if (!s || !CE_PLAYER()) return 0;
    if (!ce_option_claim(c, &s->opt)) {           /* 关卡中途买到这张卡：广播可能还没来，自己补一个 */
        ce_option_retry(c, &s->opt, PS_OPTION_OFFSET, CE_ANM_ABILITY_SCRIPT_PEASHOOTER_OPT,
                        PS_RETRY_FRAMES, "peashooter");
        return 0;                                /* 补上的那一帧不出膛，节奏从下一帧起算 */
    }
    pea_frame(&s->st);
    if (pea_needs_cap(&s->st)) ce_raise_damage_cap(PEA_CAP);
    return 0;
}

/* +0x1c：`Player__tick_shooting_state` `0x45ea00` 每射击帧对每张卡调。引擎给的两个计时器我们不用
 * （见文件头 ②），节奏全在 pea_shoot_frame 里。 */
static int on_tick_shooters(ce_card_t *c, uint32_t short_timer, uint32_t long_timer)
{
    ps_card_t *s = ce_state(c, ps_card_t);
    uint8_t *o;
    pea_out_t out;

    (void)short_timer;
    (void)long_timer;
    if (!s) return 0;
    o = ce_option_claim(c, &s->opt);
    if (!o) return 0;

    /* 随机只在这两处抽（开局一次 + 每次开完大一次），且都由射击帧驱动 ⇒ replay 里调用序列一致。 */
    if (s->st.pending_wait) {
        pea_set_wait(&s->st, ce_rand());
        ce_log("peashooter: next gatling in %u shooting frames", s->st.wait);
    }
    pea_shoot_frame(&s->st, &out);
    ce_tick_shooters_for_card(o, (int)out.timer, 0,
                              out.set == PEA_SET_GATLING ? CE_SHT_SET_PEA_GATLING : CE_SHT_SET_PEA_NORMAL);

    if (out.burst_start) {
        float ox = ce_option_x(o);
        float oy = ce_option_y(o);
        float pz = ce_player_pos()[2];
        uint32_t fx = ce_anm_spawn(CE_ABILITY_ANM(), CE_ANM_ABILITY_SCRIPT_PEA_BURST, PS_BURST_LAYER);
        if (fx) ce_anm_set_pos(fx, ox, oy, pz);
        ce_play_sound(PS_SE_BURST, ox);
        ce_log("peashooter: gatling #%u start at (%.1f, %.1f): %d frames x %d peas / %d frames = %d peas",
               s->st.bursts, ox, oy, PEA_GAT_FRAMES / PEA_GAT_INTERVAL, PEA_GAT_ROWS,
               PEA_GAT_INTERVAL, PEA_GAT_FRAMES / PEA_GAT_INTERVAL * PEA_GAT_ROWS);
    }
    if (out.burst_end)
        ce_log("peashooter: gatling #%u done, %u peas fired in total", s->st.bursts, s->st.peas);
    return 0;
}

CE_CARD(77, .on_power_level_change = on_power_level_change, .on_tick_shooters = on_tick_shooters,
            .on_tick_2 = on_tick_2, .on_load = on_load, .on_run_reset = on_run_reset,
            .on_delete = on_delete);
