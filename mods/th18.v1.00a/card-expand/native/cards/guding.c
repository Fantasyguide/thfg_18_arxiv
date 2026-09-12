/* 古锭刀（三国杀・武器）—— 被动：boss 的最后一张符卡（终符）期间，自机弹伤害翻倍。含 Extra 道中 boss。
 *
 * 判定（guding_core.h）：符卡进行中（SPELLCARD+0x78 bit0）&& 某个 boss（ENEMY_MANAGER.boss_ids[4]，照 get_boss_enemy_full 0x4237f0 走链表）
 * 当前活动中断槽（第一个 hp_value > -1 && time > 0，与引擎超时判定 0x42edf5 同一条规则）的子程序名含 "Dead"。
 * th18 七关 boss + Extra 道中的最后一张符卡都是 setInterrupt(0, 0, t, "BossDead"/"MBossDead")，其余是 "Boss2"…"Boss10"
 * （engine/ecl/th18/01 §6）。主线 1–6 关道中 boss 没有符卡，自然不触发。
 *
 * 翻倍本身归 **SDK 的自机弹伤害层**（AUDIT §AA，2026-09-08 提炼）：这张卡只在终符的两个沿上报一次倍率
 * （`ce_set_bullet_damage_permille`，1000 ↔ 2000），层负责就地乘 `bullet+0x9c`（单次取整、与卡顺序无关）
 * **并把每敌每帧总伤害上限 `player+0x47984` 按同一倍率抬起来** —— 贴脸打 boss 时自机弹本来就把上限吃满，
 * 只翻弹不抬上限等于白翻，这条以前要每张加伤害的卡自己记得（K♠ 就漏了）。
 * 判定每帧在 on_tick_2 重算；on_bullet_created 现在只做统计。
 * 炸弹与其他卡的伤害源不翻（用户定：只算自机弹，也是层的边界）。 */
#include "sdk.h"
#include "guding_core.h"

/* 返回 1 = 终符进行中；顺便带出是哪个 boss / 哪个槽（日志用）*/
static int final_spell_now(int32_t *boss_id, int *slot_idx)
{
    uint8_t *em = CE_ENEMY_MGR();
    if (!em || !ce_spell_active()) return 0;
    for (unsigned i = 0; i < CE_EM_BOSS_SLOTS; ++i) {
        int32_t id = *(int32_t *)(em + CE_EM_BOSS_IDS + i * 4);
        if (!id) continue;
        uint8_t *e = ce_enemy_by_id(em, id);
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

static int base_damage_cap(void)      /* 只为日志好看：真正抬上限的是伤害层 */
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
    /* ★ 2026-09-12（删卡收尾审计顺手加固）：`ce_state` 在 256 槽耗尽时会返回 NULL
     *（`ce_state_user` 静默失败）⇒ 每个钩子都要判，别直接解引用。同批其它卡都判了。*/
    if (!s) return 0;
    int now = final_spell_now(&boss_id, &slot);
    int edge = gd_update(s, now);
    if (edge) ce_set_bullet_damage_permille(c, now ? CE_DMG_UNIT * GD_MULT : CE_DMG_UNIT);
    if (edge > 0) ce_log("guding: final spell ON (boss enemy_id %d, interrupt slot %d) -> bullet damage x%d, cap %d -> %d",
                         boss_id, slot, GD_MULT, base_damage_cap(), base_damage_cap() * GD_MULT);
    else if (edge < 0) ce_log("guding: final spell OFF (%u frames, %u bullets doubled so far this run)", s->frames, s->bullets);
    return 0;
}

static int on_bullet_created(ce_card_t *c, void *bullet)
{
    (void)bullet;
    gd_state_t *s = ce_state(c, gd_state_t);
    if (!s) return 0;                             /* 同上：NULL 守卫 */
    gd_count_bullet(s);                           /* 只统计：乘法在伤害层里 */
    return 0;
}

static void on_stage_start(ce_card_t *c)
{
    gd_state_t *s = ce_state(c, gd_state_t);
    if (!s) return;
    s->active = 0;                                   /* 上一关末帧的判定不带进新关的第一颗弹 */
    ce_set_bullet_damage_permille(c, CE_DMG_UNIT);
}

static void on_run_reset(ce_card_t *c)
{
    gd_state_t *s = ce_state(c, gd_state_t);
    if (!s) return;
    if (s->frames) ce_log("guding: run reset (%u final-spell frames, %u bullets doubled)", s->frames, s->bullets);
    s->active = 0; s->frames = 0; s->bullets = 0;
    /* 倍率的复位由 SDK 在 `+0x4c` 桩里做（回到声明的初值），这里不用管 */
}

CE_CARD(76, .on_tick_2 = on_tick_2, .on_bullet_created = on_bullet_created,
            .on_stage_start = on_stage_start, .on_run_reset = on_run_reset,
            .bullet_damage_permille = CE_DMG_UNIT);   /* 平时 ×1，终符时改成 ×GD_MULT */
