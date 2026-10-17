/* 火球术（id 83，致敬炉石传说 Fireball）—— 主动卡：按 C 向**正前方**（画面正上方）打出一颗
 * **炎魔之王同款火球**，碰到第一个敌人就地爆炸。
 *
 * 【为什么它几乎全是现成的件】
 * 火球的贴图 / 拖尾 / 粒子 / 爆炸光环 / 火星、以及爆炸的数值，全部复用炎魔之王（id 72）那一套
 * （`ability.anm` 的 script 92/93/94/95/96 + `firelord_core.h` 的 `FL_BLAST_*`）——用户口径就是「同款火球」。
 * 于是这张卡**没有新美术、不用重编 `ability.anm`**，新的只有一条弹道和一次命中判定。
 *
 * 【爆炸为什么要抬每帧伤害上限】
 * `FL_BLAST_FRAMES` 帧、每帧一个新的 `FL_BLAST_W × FL_BLAST_H` 伤害源（一个源对同一敌人只结算一次，AUDIT U9），
 * 每帧先把 `player+0x47984`（每敌每帧总伤害上限）抬到 `FL_BLAST_CAP`：自机对着 boss 开火时这个配额已被自机弹吃满，
 * 不抬的话火球只能补差额、几乎白打（炎魔 §V20 踩过一次，这里照抄结论）。
 *
 * 【代价】
 * 炉石里火球是 4 费；我们这张**不扣火力也不扣残机**，代价就是 `FB_RECHARGE` 的充能。
 * 主动卡的门控是 SDK 照零售的：不在对话中、且场上有敌人 —— 空场按 C 无效，充能也不走（SDK.md §9）。
 *
 * 【飞出去没打到就是没打到】
 * 飞出弹幕区（**四条边都查**，各留 `FB_EXIT_MARGIN` 让它整颗出画面）即自毁，不在边上炸 —— 这张卡的赌注就是「瞄准」。
 * 直上的火球照理只会从顶边出去，查四条边是「以防万一」：以后加了横向速度 / 追踪，或坐标被别的东西改坏，
 * 它都不会飞在场外无限占着持续态（那样这张卡就再也按不动了）。还有一道 `FB_MAX_FRAMES` 兜底给「坐标被钉住不动」。
 *
 * 纯逻辑（弹道 / 命中判据 / 爆炸倒计时）在 `fireball_core.c`，主机单测；无随机、无浮点输出 ⇒ replay 安全。AUDIT §AM。 */
#include "sdk.h"
#include "fireball_core.h"

#define FB_ANGLE   (-1.5707964f)    /* 火球朝向：弧度，0 = +x，−π/2 = 画面正上方（与引擎撒道具用的那个上抛角同值）*/
#define FB_SE_CAST CE_SE_RELEASE    /* 0x4d 发动音（青眼 / 炎魔 / 反转牌同款）*/
#define FB_SE_BLAST CE_SE_BOMB      /* 0x2c 爆炸，与炎魔的火球落地同一声 */

/* 爆炸演出：照炎魔的 impact_fx —— 光环 script94 + FL_BURST 颗火星 script96 + 小震屏。*/
static void impact_fx(float x, float y, float z)
{
    uint32_t b = ce_anm_spawn(CE_ABILITY_ANM(), CE_ANM_ABILITY_SCRIPT_FIRELORD_BLAST, 13);
    if (b) ce_anm_set_pos(b, x, y, z);
    for (int i = 0; i < FL_BURST; i++) {
        uint32_t q = ce_anm_spawn(CE_ABILITY_ANM(), CE_ANM_ABILITY_SCRIPT_FIRELORD_EMBER, 13);
        if (q) ce_anm_set_pos(q, x, y, z);
    }
    ce_screen_shake(FL_SHAKE_TIME, FL_SHAKE_START, FL_SHAKE_END);
}

/* 火球坐标 FB_HIT_R 内有没有可锁定的敌人（与炎魔选目标同一套过滤：跳过死亡 / 无敌 / 未登场）。
 * 「第一个」按敌人链表顺序取——爆点用的是火球自己的坐标，所以取哪一个都炸在同一处。*/
static int hit_enemy(float bx, float by)
{
    uint8_t *em = CE_ENEMY_MGR(), *e;
    if (!em) return 0;
    CE_ENEMY_FOREACH(em, e) {
        if (!ce_enemy_lockable(e)) continue;
        const float *ep = ce_enemy_pos(e);
        if (fb_in_range(bx, by, ep[0], ep[1])) return 1;
    }
    return 0;
}

static void clear(ce_card_t *c, const char *why)
{
    fb_state_t *s = ce_state(c, fb_state_t);
    if (!s) return;
    if (s->anm_id || s->active || s->blast_left)
        ce_log("fireball: cleared (%s) after %u frames", why, s->frames);
    ce_anm_delete(s->anm_id);
    s->anm_id = 0;
    s->active = 0;
    s->blast_left = 0;
}

static int on_activate(ce_card_t *c)
{
    uint8_t *p = CE_PLAYER();
    if (!p) return ce_refuse("fireball", "no player");
    fb_state_t *s = ce_state(c, fb_state_t);
    if (!s) return ce_refuse("fireball", "no state slot");

    const float *pp = ce_player_pos();
    ce_anm_delete(s->anm_id);                              /* 上一发理论上早没了（飞完 ≤ 56 帧 ≪ 充能）；保险 */
    s->anm_id = 0;
    fb_launch(s, pp[0], pp[1]);
    s->anm_id = ce_anm_spawn(CE_ABILITY_ANM(), CE_ANM_ABILITY_SCRIPT_FIRELORD_FIREBALL, 13);
    if (s->anm_id) {
        ce_anm_set_pos(s->anm_id, s->x, s->y, pp[2]);
        ce_anm_set_rotation(s->anm_id, 0.0f, 0.0f, FB_ANGLE);
    } else {
        ce_log("fireball: ball anm failed (flying invisible)");   /* 判定照走，不因为没图就吞掉这次发动 */
    }
    ce_play_sound(FB_SE_CAST, pp[0]);
    ce_log("fireball: cast at (%.1f, %.1f), speed %.1f/frame up", s->x, s->y, (double)FB_SPEED);
    return 1;                                              /* 进持续态：飞行 + 爆炸都在 on_active_tick 里 */
}

static int on_active_tick(ce_card_t *c, uint32_t elapsed)
{
    fb_state_t *s = ce_state(c, fb_state_t);
    const float *pp = ce_player_pos();
    float z = pp ? pp[2] : 0.0f;
    (void)elapsed;
    if (!s) return 0;

    int hit = s->active ? hit_enemy(s->x, s->y) : 0;
    fb_step_t st = fb_step(s, hit);

    if (s->active) {                                       /* 还在飞：位置归 C，缩放 / 脉动归脚本 */
        float bpos[3] = { s->x, s->y, z };
        if (s->anm_id) ce_anm_set_pos(s->anm_id, s->x, s->y, z);
        s->cancelled += (uint32_t)ce_cancel_radius(bpos, FL_BALL_CANCEL_R, FL_CANCEL_MAX, 0);  /* 飞行开路：弹变点道具 */
        for (int i = 0; i < FL_PARTICLES; i++) {
            uint32_t q = ce_anm_spawn(CE_ABILITY_ANM(), CE_ANM_ABILITY_SCRIPT_FIRELORD_PARTICLE, 13);
            if (q) ce_anm_set_pos(q, s->x, s->y, z);
        }
        if (st.trail) {
            uint32_t t = ce_anm_spawn(CE_ABILITY_ANM(), CE_ANM_ABILITY_SCRIPT_FIRELORD_TRAIL, 13);
            if (t) ce_anm_set_pos(t, s->x, s->y, z);
        }
    }
    if (st.hit) {                                          /* 命中：删火球、落地消弹 + 爆炸演出 + 0x2c */
        float epos[3] = { s->ex, s->ey, z };
        int n = ce_cancel_radius(epos, FL_BLAST_CANCEL_R, FL_CANCEL_MAX, 0);
        ce_anm_delete(s->anm_id); s->anm_id = 0;
        impact_fx(s->ex, s->ey, z);
        ce_play_sound(FB_SE_BLAST, s->ex);
        ce_log("fireball: hit at (%.1f, %.1f) after %u frames: cancelled %d here + %u in flight; %d frames x %d dmg with cap %d",
               s->ex, s->ey, s->frames, n, s->cancelled, FL_BLAST_FRAMES, FL_BLAST_DMG, FL_BLAST_CAP);
    }
    if (st.missed) {
        ce_anm_delete(s->anm_id); s->anm_id = 0;
        ce_log("fireball: missed (left the field) after %u frames, cancelled %u in flight", s->frames, s->cancelled);
    }
    if (st.blast_dmg) {
        float center[3] = { s->ex, s->ey, z };
        ce_raise_damage_cap(FL_BLAST_CAP);                 /* 当帧有效（GameThread 每帧复位）：让这份伤害落在自机弹之上（§V20）*/
        ce_damage_rect(center, 0.0f, 2, st.blast_dmg, FL_BLAST_W, FL_BLAST_H);
    }
    return st.done ? 0 : 1;
}

static void on_stage_start(ce_card_t *c) { clear(c, "stage start"); }
static void on_run_reset(ce_card_t *c)   { clear(c, "run reset"); }

CE_CARD(83, .active_recharge = FB_RECHARGE, .on_activate = on_activate, .on_active_tick = on_active_tick,
            .on_stage_start = on_stage_start, .on_run_reset = on_run_reset);
