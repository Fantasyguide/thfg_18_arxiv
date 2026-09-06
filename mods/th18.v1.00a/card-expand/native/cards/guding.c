/* 古锭刀（三国杀・武器）—— 被动：boss 的最后一张符卡（终符）期间，自机弹伤害翻倍。含 Extra 道中 boss。
 *
 * 判定（guding_core.h）：符卡进行中（SPELLCARD+0x78 bit0）&& 某个 boss（ENEMY_MANAGER.boss_ids[4]，照 get_boss_enemy_full 0x4237f0 走链表）
 * 当前活动中断槽（第一个 hp_value > -1 && time > 0，与引擎超时判定 0x42edf5 同一条规则）的子程序名含 "Dead"。
 * th18 七关 boss + Extra 道中的最后一张符卡都是 setInterrupt(0, 0, t, "BossDead"/"MBossDead")，其余是 "Boss2"…"Boss10"
 * （engine/ecl/th18/01 §6）。主线 1–6 关道中 boss 没有符卡，自然不触发。
 *
 * 翻倍两处一起做（AUDIT §Y）：
 *   · on_bullet_created：bullet+0x9c ×2（K♠ / CardMomoyo 同款，PlayerBullet__create 0x45e396 写好之后调槽）；
 *   · on_tick_2：每敌每帧总伤害上限 player+0x47984 抬到 sht+0x28 基值 ×2（GameThread 0x443d3b 每帧从 sht+0x28 复位，
 *     AbilityManager tick 在其后、敌人结算 0x45f0f0 在更后）。贴脸打 boss 时自机弹本来就把上限吃满，只翻弹不抬上限等于白翻。
 * 判定每帧在 on_tick_2 重算并存私有状态，on_bullet_created（Player tick，同帧稍后）只读它。
 * 炸弹与其他卡的伤害源不翻（用户定：只算自机弹）。 */
#include "sdk.h"
#include "guding_core.h"

static uint8_t *find_enemy(uint8_t *em, int32_t id)
{
    unsigned guard = 0;
    for (uint8_t *node = *(uint8_t **)(em + CE_EM_ENEMY_LIST_HEAD); node && guard < 4096;
         node = *(uint8_t **)(node + 4), ++guard) {
        uint8_t *e = *(uint8_t **)node;
        if (e && *(int32_t *)(e + CE_ENEMY_ID) == id) return e;
    }
    return 0;
}

/* 返回 1 = 终符进行中；顺便带出是哪个 boss / 哪个槽（日志用）*/
static int final_spell_now(int32_t *boss_id, int *slot_idx)
{
    uint8_t *spell = CE_SPELLCARD();
    uint8_t *em = CE_ENEMY_MGR();
    if (!spell || !em) return 0;
    if (!(*(uint32_t *)(spell + CE_SPELL_FLAGS) & CE_SPELL_FLAG_ACTIVE)) return 0;
    for (unsigned i = 0; i < CE_EM_BOSS_SLOTS; ++i) {
        int32_t id = *(int32_t *)(em + CE_EM_BOSS_IDS + i * 4);
        if (!id) continue;
        uint8_t *e = find_enemy(em, id);
        if (!e) continue;
        const uint8_t *interrupts = e + CE_ENEMY_DATA + CE_ED_INTERRUPTS;
        int k = gd_find_active_slot(interrupts, CE_ED_INTERRUPT_SLOTS);
        if (k < 0) continue;
        if (gd_slot_is_final(interrupts + k * GD_SLOT_STRIDE)) {
            *boss_id = id; *slot_idx = k;
            return 1;
        }
    }
    return 0;
}

static int base_damage_cap(void)
{
    uint8_t *p = CE_PLAYER();
    if (!p) return 0;
    uint8_t *sht = *(uint8_t **)(p + CE_PLAYER_SHT_PTR);
    return sht ? *(int32_t *)(sht + CE_SHT_MAX_DMG) : 0;
}

static int on_tick_2(ce_card_t *c)
{
    gd_state_t *s = ce_state(c, gd_state_t);
    int32_t boss_id = 0; int slot = -1;
    int now = final_spell_now(&boss_id, &slot);
    int edge = gd_update(s, now);
    if (edge > 0) ce_log("guding: final spell ON (boss enemy_id %d, interrupt slot %d) -> bullet damage x%d, cap %d -> %d",
                         boss_id, slot, GD_MULT, base_damage_cap(), gd_damage_cap(s, base_damage_cap()));
    else if (edge < 0) ce_log("guding: final spell OFF (%u frames, %u bullets doubled so far this run)", s->frames, s->bullets);
    if (now) {
        int cap = gd_damage_cap(s, base_damage_cap());
        if (cap > 0) ce_raise_damage_cap(cap);
    }
    return 0;
}

static int on_bullet_created(ce_card_t *c, void *bullet)
{
    gd_state_t *s = ce_state(c, gd_state_t);
    int32_t *dmg = (int32_t *)((uint8_t *)bullet + CE_BULLET_DAMAGE);
    *dmg = gd_bullet_damage(s, *dmg);
    return 0;
}

static void on_stage_start(ce_card_t *c)
{
    gd_state_t *s = ce_state(c, gd_state_t);
    s->active = 0;                                   /* 上一关末帧的判定不带进新关的第一颗弹 */
}

static void on_run_reset(ce_card_t *c)
{
    gd_state_t *s = ce_state(c, gd_state_t);
    if (s->frames) ce_log("guding: run reset (%u final-spell frames, %u bullets doubled)", s->frames, s->bullets);
    s->active = 0; s->frames = 0; s->bullets = 0;
}

CE_CARD(76, .on_tick_2 = on_tick_2, .on_bullet_created = on_bullet_created,
            .on_stage_start = on_stage_start, .on_run_reset = on_run_reset);
