/* 青眼白龙 —— 主动卡：按 C 献祭 1 残机，召唤一条跟着自机的龙。龙有 BE_HP 点生命，替玩家挡子弹（每挡一发 −1，
 * 被挡的弹照炸弹消弹变点道具），每 BE_WAVE_PERIOD 帧向上喷一道矩形伤害光束（BE_WAVE_FRAMES 帧 × BE_WAVE_DMG）。
 * 生命归零 → 死亡动画、结束；过关 / 局末龙消失。残机 0 时拒绝（不许献祭最后一条命）。充能 BE_RECHARGE 帧。
 *
 * 零售原型：跟随 + 挡弹 + 计数 = Tenshi 要石（0x40e8c0）；光束 = Remilia 脉冲（0x40f3a0）。伤害上限 player+0x47984 不动。
 * 纯逻辑在 blue_eyes_core.c（主机单测）；引擎调用是 sdk.h 的薄包装（AUDIT O29）。
 * 设计：docs/superpowers/specs/2026-09-04-blue-eyes-design.md */
#include "sdk.h"
#include "blue_eyes_core.h"

#define BE_COLOR_IDLE 0xffffffffu
#define BE_COLOR_HIT  0xff0080ffu          /* Tenshi 要石有命中那帧的颜色（0x40eb60）*/
#define BE_SE_SUMMON  CE_SE_RELEASE         /* 0x4d 发动音 */
#define BE_SE_BEAM    CE_SE_BOMB            /* 0x2c */
#define BE_SE_DEATH   0x29                  /* Tenshi 要石收场音 */
#define BE_BAR_BG     0xa0181820u           /* D3DCOLOR 0xAARRGGBB：底槽深灰半透明 */
#define BE_BAR_HIGH   0xff40a0ffu           /* > 50％ 蓝 */
#define BE_BAR_MID    0xffffd040u           /* > 20％ 黄 */
#define BE_BAR_LOW    0xffff4040u           /* 红 */

/* 血条：两个根 VM（不做龙的子 VM——子会继承父的 0.45 缩放和死亡缩放）。填充条 drawRect(1,1) 由 C 写 scale = (宽, 高)，
 * 中心随宽度左对齐（不依赖 drawRect 对 anchor 的支持）。零售 HUD 充能条同款做法（0x408a53 写 vm+0x58，AUDIT O29k）。*/
static void bar_update(be_state_t *s)
{
    float ratio = s->hp > 0 ? (float)s->hp / (float)BE_HP : 0.0f;
    float w = BE_BAR_W * ratio, y = s->y + be_bob_dy(s) + BE_BAR_DY;   /* 血条跟着呼吸浮动 */
    ce_anm_set_pos(s->bar_bg_id, s->x, y, s->z);
    ce_anm_set_scale(s->bar_bg_id, BE_BAR_W + 4.0f, BE_BAR_H + 4.0f);
    ce_anm_set_pos(s->bar_id, s->x - (BE_BAR_W - w) * 0.5f, y, s->z);
    ce_anm_set_scale(s->bar_id, w > 0.0f ? w : 0.0f, BE_BAR_H);
    ce_anm_set_color(s->bar_id, ratio > 0.5f ? BE_BAR_HIGH : ratio > 0.2f ? BE_BAR_MID : BE_BAR_LOW);
}

static void bar_destroy(be_state_t *s, int fade)
{
    if (fade) { ce_anm_interrupt(s->bar_bg_id, 1); ce_anm_interrupt(s->bar_id, 1); }
    else      { ce_anm_delete(s->bar_bg_id);       ce_anm_delete(s->bar_id); }
    s->bar_bg_id = 0; s->bar_id = 0;
}

static float player_x(void) { uint8_t *p = CE_PLAYER(); return p ? *(float *)(p + CE_PLAYER_X) : 0.0f; }

static int refuse(const char *why)
{
    ce_play_sound(CE_SE_INVALID, player_x());
    ce_log("blue_eyes: refused (%s)", why);
    return CE_ACTIVATE_REFUSED;
}

static void dismiss(ce_card_t *c, const char *why)
{
    be_state_t *s = ce_state(c, be_state_t);
    if (!s) return;
    if (s->anm_id || s->beam_id || s->bar_id) {
        ce_anm_delete(s->anm_id);
        ce_anm_delete(s->beam_id);
        bar_destroy(s, 0);
        ce_log("blue_eyes: dismissed (%s) hp %d after %u frames, %u waves, %u blocked", why, s->hp, s->frames, s->waves, s->blocked);
    }
    s->anm_id = 0; s->beam_id = 0; s->hp = 0;
}

static int on_activate(ce_card_t *c)
{
    uint8_t *p = CE_PLAYER();
    if (!p) return refuse("no player");
    int lives = CE_CURRENT_LIVES();
    if (lives < 1) return refuse("no lives");
    be_state_t *s = ce_state(c, be_state_t);
    if (!s) return refuse("no state slot");

    const float *pp = (const float *)(p + CE_PLAYER_X);
    be_summon(s, pp[0], pp[1], pp[2]);
    s->anm_id = ce_anm_spawn(CE_ABILITY_ANM(), CE_ANM_ABILITY_SCRIPT_BLUE_EYES_DRAGON, 13);
    if (!s->anm_id) return refuse("dragon anm failed");
    ce_anm_set_pos(s->anm_id, s->x, s->y + be_bob_dy(s), s->z);
    s->bar_bg_id = ce_anm_spawn(CE_ABILITY_ANM(), CE_ANM_ABILITY_SCRIPT_BLUE_EYES_BAR_BG, 13);
    s->bar_id    = ce_anm_spawn(CE_ABILITY_ANM(), CE_ANM_ABILITY_SCRIPT_BLUE_EYES_BAR, 13);
    bar_update(s);

    CE_CURRENT_LIVES() = lives - 1;
    ce_gui_update_lives();
    ce_play_sound(BE_SE_SUMMON, pp[0]);
    ce_log("blue_eyes: summoned, lives %d -> %d, hp %d, anm id %08x, pos (%.1f, %.1f)", lives, lives - 1, s->hp, s->anm_id, s->x, s->y);
    return 1;
}

static int on_active_tick(ce_card_t *c, uint32_t elapsed)
{
    be_state_t *s = ce_state(c, be_state_t);
    uint8_t *p = CE_PLAYER();
    if (!s || !p) return 0;
    if (!ce_anm_get_vm(s->anm_id)) {                       /* 引擎把 VM 收掉了（O29h）：龙没了就结束 */
        ce_log("blue_eyes: dragon vm gone at frame %u", s->frames);
        s->anm_id = 0; s->beam_id = 0;
        bar_destroy(s, 0);
        return 0;
    }
    const float *pp = (const float *)(p + CE_PLAYER_X);
    be_follow(s, pp[0], pp[1], pp[2]);
    float bob = be_bob_dy(s);                              /* 呼吸浮动：画面 / 判定 / 光束起点 / 血条共用同一个值 */
    ce_anm_set_pos(s->anm_id, s->x, s->y + bob, s->z);

    float pos[3] = { s->x, s->y + bob, s->z };
    int blocked = s->hp > 0 ? ce_cancel_radius(pos, BE_RADIUS, s->hp, 0) : 0;
    ce_anm_set_color(s->anm_id, blocked ? BE_COLOR_HIT : BE_COLOR_IDLE);

    be_step_t st = be_step(s, blocked);
    bar_update(s);
    if (st.wave_start) {
        s->beam_id = ce_anm_spawn(CE_ABILITY_ANM(), CE_ANM_ABILITY_SCRIPT_BLUE_EYES_BEAM, 13);
        ce_play_sound(BE_SE_BEAM, s->x);
        ce_log("blue_eyes: wave %u start at frame %u (hp %d, cap %d)", s->waves, s->frames, s->hp, *(int32_t *)(p + CE_PLAYER_DAMAGE_CAP));
    }
    float mouth_y = s->y + bob + BE_MOUTH_DY;              /* 龙头 y（含浮动）*/
    if (st.beam_dmg && mouth_y > 0.0f) {                   /* 光束：从龙头到区域顶边（y = 0）的矩形 */
        float center[3] = { s->x, mouth_y * 0.5f, s->z };
        ce_raise_damage_cap(BE_WAVE_CAP);                  /* 当帧有效：让这 100 落在自机弹之上而不是被它吃掉（V20 / O29e′）*/
        ce_damage_rect(center, 0.0f, 2, st.beam_dmg, BE_BEAM_WIDTH, mouth_y);
        ce_anm_set_pos(s->beam_id, s->x, mouth_y, s->z);   /* 光束父 VM 钉在龙头；子脚本从这里向上画（anchor 左端 + 自转 −90°）*/
    }
    if (elapsed % 60 == 0 && blocked) ce_log("blue_eyes: hp %d (blocked %u, frame %u)", s->hp, s->blocked, s->frames);
    if (st.died) {
        ce_anm_interrupt(s->anm_id, 1);                    /* 脚本 interruptLabel(1)：放大淡出后 delete */
        ce_anm_delete(s->beam_id);
        bar_destroy(s, 1);
        ce_play_sound(BE_SE_DEATH, s->x);
        ce_log("blue_eyes: died after %u frames, %u waves, %u blocked", s->frames, s->waves, s->blocked);
        s->anm_id = 0; s->beam_id = 0;
        return 0;
    }
    return 1;
}

static void on_stage_start(ce_card_t *c) { dismiss(c, "stage start"); }
static void on_run_reset(ce_card_t *c)   { dismiss(c, "run reset"); }

CE_CARD(67, .active_recharge = BE_RECHARGE, .on_activate = on_activate, .on_active_tick = on_active_tick,
            .on_stage_start = on_stage_start, .on_run_reset = on_run_reset);
