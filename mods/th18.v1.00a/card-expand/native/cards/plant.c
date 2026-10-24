/* 「种下去的植物」共用的引擎层。见 plant.h。AUDIT §AN / §AO。 */
#include "plant.h"

#define PLANT_COLOR_IDLE 0xffffffffu
#define PLANT_COLOR_HIT  0xffff8080u       /* 挡到弹那帧染红（青眼是蓝、炎魔是橙）*/
#define PLANT_SE_SOW     CE_SE_RELEASE     /* 0x4d 发动音 */
#define PLANT_SE_HARVEST CE_SE_CARDGET     /* 0x2e 产出那一下 */
#define PLANT_SE_WITHER  0x29              /* 要石收场音（青眼 / 炎魔死亡同款）*/
#define PLANT_BAR_HIGH   0xffffe040u       /* > 50％ 金 */
#define PLANT_BAR_MID    0xffffa030u       /* > 20％ 橙 */
#define PLANT_BAR_LOW    0xffff4040u       /* 红 */

/* 血条：复用青眼的两个 drawRect 根脚本（86 底槽 / 87 填充），C 每帧写 pos / scale / color（炎魔同款）。*/
static void bar_update(plant_state_t *s)
{
    float ratio = (s->hp > 0 && s->hp_max > 0) ? (float)s->hp / (float)s->hp_max : 0.0f;
    float w = PLANT_BAR_W * ratio, y = s->y + PLANT_BAR_DY;
    ce_anm_set_pos(s->bar_bg_id, s->x, y, s->z);
    ce_anm_set_scale(s->bar_bg_id, PLANT_BAR_W + 4.0f, PLANT_BAR_H + 4.0f);
    ce_anm_set_pos(s->bar_id, s->x - (PLANT_BAR_W - w) * 0.5f, y, s->z);
    ce_anm_set_scale(s->bar_id, w > 0.0f ? w : 0.0f, PLANT_BAR_H);
    ce_anm_set_color(s->bar_id, ratio > 0.5f ? PLANT_BAR_HIGH : ratio > 0.2f ? PLANT_BAR_MID : PLANT_BAR_LOW);
}

static void bar_destroy(plant_state_t *s, int fade)
{
    if (fade) { ce_anm_interrupt(s->bar_bg_id, 1); ce_anm_interrupt(s->bar_id, 1); }
    else      { ce_anm_delete(s->bar_bg_id);       ce_anm_delete(s->bar_id); }
    s->bar_bg_id = 0; s->bar_id = 0;
}

/* 撒一轮：def->items 个道具，扇形铺开（角度按下标算，不取随机数）。池满时 spawn 返回 0，记一行日志。*/
static void harvest(plant_state_t *s, const plant_def_t *def)
{
    float pos[3] = { s->x, s->y, s->z };
    int ok = 0;
    for (uint32_t i = 0; i < def->items; i++)
        if (ce_spawn_item(def->item_type, pos, plant_item_angle_at((int)i, (int)def->items), PLANT_ITEM_SPEED)) ok++;
    ce_play_sound(PLANT_SE_HARVEST, s->x);
    ce_log("%s: harvest #%u at frame %u -> %d/%u items of type %u (hp %d)",
           def->tag, s->harvests, s->frames, ok, def->items, def->item_type, s->hp);
}

#define PLANT_SWEEP_MONEY 1
static uint32_t s_plant_sweep_seen;   /* 92 清扫代数（每次发动只清一次）*/

void plant_clear(ce_card_t *c, const plant_def_t *def, const char *why)
{
    plant_state_t *s = ce_state(c, plant_state_t);
    if (!s) return;
    if (s->anm_id || s->hp > 0) {
        ce_anm_delete(s->anm_id);
        bar_destroy(s, 0);
        ce_log("%s: uprooted (%s) hp %d after %u frames, %u blocked, %u harvests",
               def->tag, why, s->hp, s->frames, s->blocked, s->harvests);
    }
    s->anm_id = 0; s->hp = 0;
}

int plant_activate(ce_card_t *c, const plant_def_t *def)
{
    /* ★ 2026-10-24：发动时把清扫代数对齐（否则"卡没生效时 92 打过"会留一个旧代数，
     *   本次刚召出来的东西会在下一次 tick 被当成 92 的对象清掉）。*/
    (void)ce_sweep_should_run(&s_plant_sweep_seen);
    uint8_t *p = CE_PLAYER();
    if (!p) return ce_refuse(def->tag, "no player");
    int power = CE_CURRENT_POWER();
    if (power < def->power_gate) return ce_refuse(def->tag, "power too low");
    plant_state_t *s = ce_state(c, plant_state_t);
    if (!s) return ce_refuse(def->tag, "no state slot");

    const float *pp = ce_player_pos();
    ce_anm_delete(s->anm_id);                              /* 上一株理论上早没了（同一时刻只有一株）；保险 */
    bar_destroy(s, 0);
    plant_sow(s, def->hp, def->period, pp[0], pp[1], pp[2]);
    s->anm_id = ce_anm_spawn(CE_ABILITY_ANM(), def->script, 13);
    if (!s->anm_id) {                                      /* 起不来就当没发动：此时**还没扣火力** */
        ce_log("%s: body anm failed", def->tag);
        s->hp = 0;
        return ce_refuse(def->tag, "anm failed");
    }
    ce_anm_set_pos(s->anm_id, s->x, s->y, s->z);
    s->bar_bg_id = ce_anm_spawn(CE_ABILITY_ANM(), CE_ANM_ABILITY_SCRIPT_BLUE_EYES_BAR_BG, 13);
    s->bar_id    = ce_anm_spawn(CE_ABILITY_ANM(), CE_ANM_ABILITY_SCRIPT_BLUE_EYES_BAR, 13);
    bar_update(s);

    /* 照 Tsukasa `0x410e60` / 炎魔：spend_power → 无条件 repopulate（顺手广播 on_power_level_change）*/
    int level_changed = ce_spend_power(def->power_cost);
    ce_repopulate_options();
    ce_play_sound(PLANT_SE_SOW, s->x);
    ce_log("%s: planted at (%.1f, %.1f), power %d -> %d (level changed %d), hp %d, first harvest in %u frames",
           def->tag, s->x, s->y, power, CE_CURRENT_POWER(), level_changed, s->hp, s->timer);
    return 1;                                              /* 进持续态 */
}

int plant_tick(ce_card_t *c, const plant_def_t *def, uint32_t elapsed)
{
    plant_state_t *s = ce_state(c, plant_state_t);
    if (!s) return 0;
    /* ★★ 2026-10-24（92 清扫）：种着的植物 → 换金钱 + 清 VM（植物本体是纯 VM，敌人扫描看不见它）*/
    {   /* ★★★ 2026-10-24 修 bug（owner：紧跟着 92 之后放的召唤物直接变钱卡）——
     *     原来 `if (s->anm_id && ce_sweep_should_run(&seen))` 是**短路**：92 那一刻场上没有召唤物
     *     时代数根本没被消费 ⇒ 之后任何一次召唤都会看到"旧代数" ⇒ 刚放出来的那只立刻被清成钱。
     *     ⇒ 代数每帧**无条件消费**，只有"当时真在场"的才清。*/
        const int gw_sweep = ce_sweep_should_run(&s_plant_sweep_seen);
        if (gw_sweep && s->anm_id) {
        float wp[3];
        int k;
        wp[0] = s->x; wp[1] = s->y; wp[2] = s->z;
        for (k = 0; k < PLANT_SWEEP_MONEY; ++k) {
            if (!ce_spawn_item(0x12u, wp, -1.5707964f, 1.7f)) ce_money_pocket(1);   /* ★ 0x12 = **金钱卡**（不是金钱点 type 2）*/
        }
        ce_log_key("plant: 92 清扫 —— %s 的植物换成金钱（%d 枚）", def->tag, PLANT_SWEEP_MONEY);
        plant_clear(c, def, "gw sweep");
        return 0;
    }
        }
    if (!ce_anm_get_vm(s->anm_id)) {                       /* 引擎把 VM 收掉了（O29h 同款）：本体没了就结束 */
        ce_log("%s: body vm gone at frame %u", def->tag, s->frames);
        s->anm_id = 0;
        bar_destroy(s, 0);
        return 0;
    }

    float pos[3] = { s->x, s->y, s->z };
    int blocked = s->hp > 0 ? ce_cancel_radius(pos, def->radius, s->hp, 0) : 0;   /* max = hp：不会超额挡弹 */
    ce_anm_set_color(s->anm_id, blocked ? PLANT_COLOR_HIT : PLANT_COLOR_IDLE);

    plant_step_t st = plant_step(s, blocked);
    ce_anm_set_pos(s->anm_id, s->x, s->y, s->z);           /* 种在哪儿就一直在哪儿 */
    bar_update(s);

    if (st.produce) harvest(s, def);
    if (elapsed % 60 == 0 && blocked)
        ce_log("%s: hp %d (blocked %u, frame %u)", def->tag, s->hp, s->blocked, s->frames);
    if (st.died) {
        ce_anm_interrupt(s->anm_id, 1);                    /* 脚本 interruptLabel(1)：缩小淡出后 delete */
        bar_destroy(s, 1);
        ce_play_sound(PLANT_SE_WITHER, s->x);
        ce_log("%s: withered after %u frames, %u blocked, %u harvests",
               def->tag, s->frames, s->blocked, s->harvests);
        s->anm_id = 0;
        return 0;
    }
    return 1;
}
