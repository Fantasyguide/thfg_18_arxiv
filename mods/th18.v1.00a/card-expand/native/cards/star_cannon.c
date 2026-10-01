/* 星星炮（id 89）—— **装备卡**，致敬《泰拉瑞亚》的星星炮（Star Cannon / `ItemID.StarCannon = 197`）。
 * 卡名那句「桑百颗够吗？」问的就是原版的**坠落之星弹药**（`ItemID.FallenStar = 75`）。
 *
 * ## 两件事
 * ① **主动面（装备）**：身边一门小炮（原生子机机制 `ce_option_*`），每 **12 帧**朝**最近的敌人**
 *    射出一颗星星 —— 节奏与弹速是**原版白板复刻**（`useTime = useAnimation = 12` ⇒ 5 发/秒；
 *    `shootSpeed = 14`；弹幕 `ProjectileID.StarCannonStar = 955`，`aiStyle 5`：
 *    **不加速、不重力**，只有 0.14 rad/帧的自转与 0..200 的闪烁，**无限穿透**）。
 *    数值表与一手出处见 `star_cannon_core.h` 的文件头。
 * ② **被动**：装备着这张卡**撞弹致死**时，游玩区正中显示泰拉瑞亚样的死亡讯息：
 *    「你被杀死了…」＋「掉了591铂金60金15银铜」（文本由 owner 给定，逐字）。
 *    贴图 = 我们按泰拉瑞亚 UI 的样式烘的一张 PNG（`STAR_CANNON_DEATH_MSG`，见 ORDER.txt 与
 *    `assets/ability/star_cannon/make_death_msg.py`）。
 *
 * ## 纪律
 * · 死亡钩子（`on_death_after_deathbomb` = 玩家真正死掉那一刻）里**只置位**，不在死亡栈上建 VM
 *   （照 112 `ufo.c` 的 `pend_drop` 先例）；真正的建 VM 放在下一帧的 `on_tick_2`。
 * · `on_delete` / `on_run_reset` 收光：星星 VM、讯息 VM、子机槽（商店里删卡时 tick 全停，只有它跑得到）。
 * · 音效走**自建语音槽** `CE_VOICE_STAR_CANNON_SHOT`（构建期从用户本机的 `Content/Sounds/Item_9.xnb`
 *   现取 —— 对位原版 `UseSound = SoundID.Item9`，与 112 取 TH12 `se_piyo.wav` 同一套口径）。 */
#include "sdk.h"
#include "star_cannon_core.h"
#include "broken_core_core.h"      /* bc_atan2f：确定性 SSE 多项式（禁调引擎的 x87 atan2）*/

#define SC_OPT_ANM_Y    0x10       /* allocate 的偏移参（引擎写进 option+0x68/+0x70）*/
#define SC_OPT_NF_X     44.0f      /* 非聚焦：自机右前方 ❓ 自定（与天顶剑同款，炮身短所以更贴）*/
#define SC_OPT_NF_Y     (-10.0f)
#define SC_OPT_F_X      34.0f      /* 聚焦：收拢一点 ❓ 自定 */
#define SC_OPT_F_Y      (-14.0f)
#define SC_RETRY        60         /* 关卡中途买到这张卡：没拿到子机时的重试间隔 */
#define SC_LAYER        13         /* 世界特效层（与天顶剑 / 破损核心同层）*/
#define SC_MSG_LAYER    25         /* 讯息贴图：UI 层（世界层是 3–19，HUD 是 24/25）*/
#define SC_MSG_X        320.0f     /* 640×480 空间：(320, 240) 正是**游玩区正中**
                                    *（x：场地中心 320；y：场地上沿 16 + 448/2 = 240）*/
#define SC_MSG_Y        240.0f
#define SC_MSG_FRAMES   300        /* ❓自定：显示 5 秒（原版一直挂到复活；我们的死亡演出更短）*/
#define SC_MSG_FADE     40         /* 最后 40 帧线性淡出 */

typedef struct {
    ce_option_t opt;               /* 跟班槽 */
    sc_state_t  st;                /* 星星池 + 开火节奏（纯逻辑）*/
    uint8_t     death_pending;     /* 死亡钩子里只置位（那一帧不建 VM）*/
    uint32_t    msg_id;            /* 讯息 VM（0 = 没有）*/
    uint32_t    msg_life;          /* 讯息剩余帧 */
} sc_card_t;

_Static_assert(sizeof(sc_card_t) <= 496, "star_cannon: private state overflows CE_STATE_BYTES");

/* 侧向站位：allocate 只写 y 偏移 ⇒ x 的两格自己写（一手原生卡 `th18:0x40b8c1` 同款）。*/
static void sc_place(ce_option_t *o)
{
    uint8_t *p = o ? o->ptr : 0;
    if (!p) return;
    *(int32_t *)(p + CE_OPT_OFF_NF_X) = (int32_t)(SC_OPT_NF_X * 128.0f);
    *(int32_t *)(p + CE_OPT_OFF_NF_Y) = (int32_t)(SC_OPT_NF_Y * 128.0f);
    *(int32_t *)(p + CE_OPT_OFF_F_X)  = (int32_t)(SC_OPT_F_X * 128.0f);
    *(int32_t *)(p + CE_OPT_OFF_F_Y)  = (int32_t)(SC_OPT_F_Y * 128.0f);
}

static void sc_msg_kill(sc_card_t *s)
{
    if (s->msg_id) ce_anm_delete(s->msg_id);
    s->msg_id = 0;
    s->msg_life = 0;
}

/* 收光：星星 + 讯息 + 子机槽（不依赖 tick —— 商店里删卡只有 `on_delete` 跑得到）。*/
static void sc_kill_all(ce_card_t *c)
{
    sc_card_t *s = ce_state(c, sc_card_t);
    int i;
    if (!s) return;
    for (i = 0; i < SC_SHOT_MAX; ++i) {
        if (s->st.s[i].id) ce_anm_delete(s->st.s[i].id);
        s->st.s[i].id = 0;
        s->st.s[i].active = 0;
    }
    sc_msg_kill(s);
    s->death_pending = 0;
}

static void sc_stop(ce_card_t *c)
{
    sc_card_t *s = ce_state(c, sc_card_t);
    sc_kill_all(c);
    ce_option_drop(c, s ? &s->opt : 0);
    if (s) sc_reset(&s->st);
}

/* +0x18：开局 / 火力档变 / 关店后广播 —— 无条件重新申请一个（与原生装备卡一致）。*/
static int on_power_level_change(ce_card_t *c)
{
    sc_card_t *s = ce_state(c, sc_card_t);
    if (s) {
        ce_option_alloc(c, &s->opt, SC_OPT_ANM_Y, CE_ANM_ABILITY_SCRIPT_STAR_CANNON_OPT, "star_cannon");
        sc_place(&s->opt);
    }
    return 0;
}

static void on_run_reset(ce_card_t *c) { sc_stop(c); }
static void on_delete(ce_card_t *c)    { sc_stop(c); }

static int on_load(ce_card_t *c)
{
    sc_stop(c);
    return 0;
}

/* ★★ 被动：**撞弹致死**那一刻（`+0x0c` = 死炸弹窗口结算之后 = 真的死了；`+0x10` 是中弹那一帧，
 * 那时还可能被 107/117/113 的救援救回来 ⇒ 用它当触发会让"救命成功"也弹死亡讯息）。
 * ⚠ 这里**只置位**：死亡栈上不建 VM（纪律见 112 `ufo.c` 的 `pend_drop`）。*/
static int on_death_after_deathbomb(ce_card_t *c, uint32_t acc)
{
    sc_card_t *s = ce_state(c, sc_card_t);
    (void)acc;
    if (s) s->death_pending = 1;
    return 0;
}

/* +0x2c：AbilityManager 每帧（菜单 / 商店 / 暂停里不跑）。*/
static int on_tick_2(ce_card_t *c)
{
    sc_card_t *s = ce_state(c, sc_card_t);
    uint8_t *em = CE_ENEMY_MGR();
    uint8_t *p = CE_PLAYER();
    uint8_t *enemy, *o;
    sc_shot_t *sh;
    sc_aim_t aim;
    float fx, fy, pz, dir[2], x, y, ang, center[3];
    uint32_t id;
    int i;

    if (!s || !p) return 0;

    /* ① 死亡讯息：上一帧的死亡钩子置了位 ⇒ 现在（已离开死亡栈）建 VM 并居中。*/
    if (s->death_pending) {
        s->death_pending = 0;
        sc_msg_kill(s);
        id = ce_anm_spawn(CE_ABILITY_ANM(), CE_ANM_ABILITY_SCRIPT_STAR_CANNON_DEATH_MSG, SC_MSG_LAYER);
        if (id) {
            s->msg_id = id;
            s->msg_life = SC_MSG_FRAMES;
            ce_anm_set_pos(id, SC_MSG_X, SC_MSG_Y, 0.0f);
            ce_anm_set_color(id, 0xffffffffu);
            ce_log("star_cannon: death message shown (590+1 platinum version) at (%.0f, %.0f)",
                   (double)SC_MSG_X, (double)SC_MSG_Y);
        }
    }
    if (s->msg_id) {
        if (!ce_anm_get_vm(s->msg_id) || s->msg_life == 0) {
            sc_msg_kill(s);
        } else {
            int a = 255;
            if (s->msg_life < SC_MSG_FADE) a = (int)(255u * s->msg_life / SC_MSG_FADE);
            ce_anm_set_color(s->msg_id, ((uint32_t)a << 24) | 0x00ffffffu);
            s->msg_life--;
        }
    }

    /* ② 跟班（炮身）：站在自机旁边，**炮口跟着目标转**（rotation.z 由 C 每帧写）。*/
    o = ce_option_get(c, &s->opt, SC_OPT_ANM_Y, CE_ANM_ABILITY_SCRIPT_STAR_CANNON_OPT, SC_RETRY, "star_cannon");
    if (!o) return 0;
    sc_place(&s->opt);
    pz = ce_player_pos()[2];
    fx = ce_option_x(o);
    fy = ce_option_y(o);

    /* 瞄准：最近的敌人；没有就朝正上（❓自定，我们的弹幕区没有光标）*/
    sc_aim_reset(&aim);
    if (em) {
        CE_ENEMY_FOREACH(em, enemy) {
            if (!ce_enemy_lockable(enemy)) continue;
            sc_aim_consider(&aim, fx, fy, ce_enemy_pos(enemy)[0], ce_enemy_pos(enemy)[1]);
        }
    }
    if (sc_aim_ready(&aim)) {
        ang = bc_atan2f(aim.dy, aim.dx);
    } else {
        ang = -1.5707964f;                                  /* −π/2：正上 */
    }
    ce_anm_set_rotation((uint32_t)*(int32_t *)(o + CE_OPT_ANM_ID), 0.0f, 0.0f, ang);

    /* ③ 星星：推进 + 自转 + 闪烁 + 判定；出界就收。*/
    for (i = 0; i < SC_SHOT_MAX; ++i) {
        sh = &s->st.s[i];
        if (!sh->active) continue;
        if (!ce_anm_get_vm(sh->id)) { sh->active = 0; sh->id = 0; continue; }
        if (sc_step(sh)) { ce_anm_delete(sh->id); sh->id = 0; continue; }
        ce_anm_set_pos(sh->id, sh->x, sh->y, pz);
        ce_anm_set_rotation(sh->id, 0.0f, 0.0f, sh->rot);
        ce_anm_set_color(sh->id, ((uint32_t)sc_draw_alpha(sh) << 24) | 0x00ffffffu);
        if (sh->hit) {                                   /* 照原版 localNPCHitCooldown = 10：同一颗星
                                                          * 对同一个敌人每 10 帧才结算一次 */
            center[0] = sh->x; center[1] = sh->y; center[2] = pz;
            ce_damage_rect(center, 0.0f, SC_HIT_LIFE, SC_DMG, SC_HIT_SIZE, SC_HIT_SIZE);
        }
    }
    if (sc_live(&s->st) > 0) ce_raise_damage_cap(SC_CAP);

    /* ④ 开火：蓄满才发（池满就一直攒着，不丢已有星星）。*/
    if (!sc_charge(&s->st)) return 0;
    sh = sc_slot(&s->st);
    if (!sh) return 0;
    id = ce_anm_spawn(CE_ABILITY_ANM(), CE_ANM_ABILITY_SCRIPT_STAR_CANNON_SHOT, SC_LAYER);
    if (!id) return 0;
    ce_polar_to_vel(dir, ang, SC_SPEED);                    /* 方向 × 14（原版 shootSpeed）*/
    x = fx + dir[0] * 0.6f;                                 /* 炮口稍微往前一点 ❓ 自定 */
    y = fy + dir[1] * 0.6f;
    ce_anm_set_pos(id, x, y, pz);
    ce_anm_set_rotation(id, 0.0f, 0.0f, 0.0f);
    ce_anm_set_color(id, 0xffffffffu);
    sc_launch(sh, id, x, y, dir[0], dir[1]);
    sc_did_fire(&s->st);
    ce_play_sound(CE_VOICE_STAR_CANNON_SHOT, fx);           /* 对位原版 `UseSound = SoundID.Item9` */
    if (s->st.fired <= 3 || s->st.fired % 60 == 0)
        ce_log("star_cannon: shot #%u at frame %u, cannon (%.1f, %.1f) ang %.1f deg, live %d",
               s->st.fired, s->st.frames, (double)fx, (double)fy,
               (double)(ang * 57.29578f), sc_live(&s->st));
    return 0;
}

CE_CARD(89, .on_power_level_change = on_power_level_change, .on_tick_2 = on_tick_2,
            .on_death_after_deathbomb = on_death_after_deathbomb,
            .on_load = on_load, .on_run_reset = on_run_reset, .on_delete = on_delete);
