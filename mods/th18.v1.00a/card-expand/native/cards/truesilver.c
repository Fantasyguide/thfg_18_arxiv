/* 真银圣剑（炉石传说・Truesilver Champion 4/2）—— 被动「武器」：每关前 TS_DURABILITY 发炸弹是「挥砍」——
 * 炸弹放出的下一帧起连续 TS_BURST_FRAMES 帧、每帧一个盖满弹幕区的矩形伤害源 TS_BURST_DMG（范围内每个敌人各吃 400），
 * 并把每敌每帧伤害上限抬到 TS_BURST_CAP（炎魔 / Remilia 同款，否则被自机弹 + 炸弹自己的源吃满）；每次挥砍回 1 个残机碎片
 * （引擎自己的加碎片函数：满则不加、满 3 进整命、自带刷 HUD）。耐久每关开场回满。决死炸 / 黄昏接出来的第二发也算挥砍。
 *
 * HUD 数字：引擎建好本卡的 HUD 图标时经虚表 +0x44（on_hud_anm）交来图标根 VM 的 anm id；我们读它的实体坐标（vm+0x5f0），
 * 在 UI 列表里起 ability.anm 的 script99（数字贴图 TS_DIGIT_0..9，脚本里 pos = 图标根脚本的 (1056, 537) + 偏移、layer 26、
 * resolutionMode 2 与图标同款），钉在同一实体坐标上；耐久变化时 set_sprite 换数字。每帧核对图标还活着，死了就删数字；
 * 图标被引擎重建（进店 / 拿新卡）时 +0x44 会再来一次，旧数字先删。★ 数字相对图标的偏移是唯一要实跑调的量（日志打图标坐标）。
 * 纯逻辑在 truesilver_core.c（主机单测）。AUDIT §X。 */
#include "sdk.h"
#include "truesilver_core.h"

#define TS_DIGIT_MAX 9

static void digit_kill(ts_state_t *s)
{
    if (s->digit_id) ce_anm_delete(s->digit_id);
    s->digit_id = 0; s->digit_shown = -1;
}

static void digit_sync(ts_state_t *s)
{
    uint8_t *icon = ce_anm_get_vm(s->icon_id);
    if (!icon) { if (s->digit_id) { digit_kill(s); ce_log("truesilver: hud icon gone, digit removed"); } s->icon_id = 0; return; }
    if (s->digit_id && !ce_anm_get_vm(s->digit_id)) { s->digit_id = 0; s->digit_shown = -1; }
    if (!s->digit_id) {
        const float *ip = (const float *)(icon + CE_ANM_VM_POS);
        s->digit_id = ce_anm_spawn_ui(CE_ABILITY_ANM(), CE_ANM_ABILITY_SCRIPT_HUD_DIGIT, ip);
        s->digit_shown = -1;
        ce_log("truesilver: hud digit %s (icon id %u at %.1f, %.1f, %.1f; digit id %u)", s->digit_id ? "spawned" : "FAILED",
               s->icon_id, ip[0], ip[1], ip[2], s->digit_id);
        if (!s->digit_id) return;
    }
    int d = s->durability < 0 ? 0 : s->durability > TS_DIGIT_MAX ? TS_DIGIT_MAX : s->durability;
    if (d != s->digit_shown && ce_anm_set_sprite(CE_ABILITY_ANM(), s->digit_id, CE_ANM_ABILITY_SPRITE_TS_DIGIT_0 + d))
        s->digit_shown = d;
}

static int ctor(ce_card_t *c)
{
    ts_state_t *s = ce_state(c, ts_state_t);
    if (!s) return 0;
    ts_stage_start(s);                                     /* 买到当关就有 2 点；每关开场引擎也会再调一次 ctor，幂等 */
    return 0;
}

static void on_stage_start(ce_card_t *c)
{
    ts_state_t *s = ce_state(c, ts_state_t);
    if (!s) return;
    ts_stage_start(s);
    ce_log("truesilver: stage start, durability %d", s->durability);
}

static void on_bomb_spent(ce_card_t *c)
{
    ts_state_t *s = ce_state(c, ts_state_t);
    if (!s) return;
    if (!ts_on_bomb(s)) { ce_log("truesilver: bomb without durability (0 left this stage)"); return; }
    int lives = CE_CURRENT_LIVES(), frag = CE_LIFE_FRAGMENTS();
    ce_add_life_fragment();
    ce_log("truesilver: swing #%u -> durability %d, +400 over %d frames (cap %d); life fragments %d/%d -> %d/%d",
           s->swings, s->durability, TS_BURST_FRAMES, TS_BURST_CAP, lives, frag, CE_CURRENT_LIVES(), CE_LIFE_FRAGMENTS());
    digit_sync(s);
}

static int on_hud_anm(ce_card_t *c, uint32_t id)
{
    ts_state_t *s = ce_state(c, ts_state_t);
    if (!s) return 0;
    if (s->digit_id) digit_kill(s);                        /* 图标重建：旧数字先删 */
    s->icon_id = id;
    digit_sync(s);
    return 0;
}

static int on_tick_2(ce_card_t *c)
{
    ts_state_t *s = ce_state(c, ts_state_t);
    if (!s) return 0;
    int dmg = ts_tick(s);
    if (dmg) {
        float center[3] = { 0.0f, TS_FIELD_CY, 0.0f };
        ce_raise_damage_cap(TS_BURST_CAP);
        ce_damage_rect(center, 0.0f, 2, dmg, TS_FIELD_W, TS_FIELD_H);
    }
    if (s->icon_id || s->digit_id) digit_sync(s);
    return 0;
}

static void on_run_reset(ce_card_t *c)
{
    ts_state_t *s = ce_state(c, ts_state_t);
    if (!s) return;
    digit_kill(s);
    s->icon_id = 0; s->burst_left = 0;
}

CE_CARD(75, .ctor = ctor, .on_stage_start = on_stage_start, .on_bomb_spent = on_bomb_spent,
            .on_hud_anm = on_hud_anm, .on_tick_2 = on_tick_2, .on_run_reset = on_run_reset);
