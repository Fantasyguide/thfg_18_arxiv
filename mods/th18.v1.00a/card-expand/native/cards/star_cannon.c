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
/* ★ 2026-10-03 owner 实测口径：「星星炮位置放在**子机正前方**」⇒ 炮身摆在自机正上方居中
 *   （原生子机那一排在自机两侧偏下，"正前方"就是那一排的前面 = 再往上、回中）。❓ 数值仍是我定的。*/
#define SC_OPT_NF_X     0.0f       /* 非聚焦：正前方（居中）*/
#define SC_OPT_NF_Y     (-40.0f)
#define SC_OPT_F_X      0.0f       /* 聚焦：收拢一点 */
#define SC_OPT_F_Y      (-34.0f)
#define SC_RETRY        60         /* 关卡中途买到这张卡：没拿到子机时的重试间隔 */
/* ★★ 2026-10-03 owner：「89 这也涉及到社区梗，计数，打到 300 颗的时候会变成灰色装填一小段时间」
 *   ⇒ 弹匣 300 发（对位原版的**坠落之星弹药** —— 卡名那句「桑百颗够吗？」问的就是它）、
 *     打满 300 ⇒ 子机**变灰** + 装填 `SC_RELOAD_FRAMES` 帧（这期间不开火），装完计数归零。
 *     ❓ 装填时长是我拍的（owner 只说"一小段时间"）。
 *   ★ 2026-10-03 owner：「不需要 on_draw」⇒ **不显示任何数字**，计数只在内部（诊断日志里报）。*/
#define SC_INPUT_Z      0x001u     /* ★ owner 实测：「星星炮和天顶剑**是否开火得看 Z**」
                                    *   ⇒ 只有按住 Z（射击键）时才开火；松手就停（充能照跑，随时能续）*/
#define SC_MAGAZINE     300        /* 一个弹匣 = 300 颗 */
#define SC_SE_MUZZLE    0x2d       /* ❓ 出膛音借 TH18 的 se_slash（与天顶剑同一枚）*/
#define SC_TAIL_BACK    9.0f       /* ❓ 彗尾 VM 相对星位再往后挪多少 px（贴图 44×16、scale 0.5 ⇒
                                    *   半长 11；挪 9 让**头端**正好压在星上、尾巴拖在身后）*/
#define SC_RELOAD_FRAMES 180       /* ❓自定：装填 3 s（这期间灰着、不开火）*/
#define SC_LAYER        13         /* 世界特效层（与天顶剑 / 破损核心同层）*/
#define SC_MSG_LAYER    25         /* 讯息贴图：UI 层（世界层是 3–19，HUD 是 24/25）*/
#define SC_MSG_X        320.0f     /* 640×480 空间：(320, 240) 正是**游玩区正中**
                                    *（x：场地中心 320；y：场地上沿 16 + 448/2 = 240）*/
#define SC_MSG_Y        234.0f     /* ★ 贴图中心：一手是「第一行**字形顶** = 屏幕中心 − 60（1080p 的 px）」——
                                    *   按屏高换算到我们 480 高的空间是 **−27** ⇒ 字形顶 ≈ 213；贴图高 46（已裁
                                    *   上下留白、字形顶离贴图上沿 2）⇒ 中心 = 211 + 23 = **234**。
                                    *   ⚠ 字号/贴图尺寸一改（`make_death_msg.py`），这里与下面两个半宽一起改 */
/* ★ 2026-10-02（一手，AUDIT §BE 详录 #213 追加 B）：原版 `Main.DrawInterface_35_YouDied()` 的行为是
 *   「死亡瞬间出现、**无淡入无淡出**、复活下一帧直接消失」。
 * ★ 2026-10-03 owner 定案：**显示 15 秒**（原版死亡界面底下那行就是**复活倒计时** ——
 *   `Game.RespawnInSuffix` 从 15 秒起数，所以"挂着 15 秒"才是那段界面的实际时长）。
 *   ⇒ 这里改成**固定 15 s**（900 帧 @60fps）、仍然**不淡入不淡出**（淡出这条原版没有）。*/
#define SC_MSG_FRAMES   900        /* 15 s（owner 口径；原版倒计时也是 15 起步）*/
/* ★ 2026-10-03 owner：「你被杀死了**太大了**」⇒ 字号按屏高换算重烘（52/21 → 27/11，见
 *   `make_death_msg.py` 的注）：贴图 372×80 → **320×46**，位置与碰触盒同步。*/
/* ★★ 2026-10-03 owner：「仁慈一些，类似于 115 chapter finish 的淡出，当第一次碰触到的时候，
 *   死亡讯息直接隐藏」⇒ 照 115（`lolk.c`）那套**防遮挡**做：
 *     · 判据 = 自机（世界坐标换算到 640 空间）**碰进讯息贴图的矩形**（外扩 `SC_MSG_TOUCH_PAD`）；
 *     · 动作 = 起一次 `SC_MSG_FADE` 帧的线性淡出再收（**帧数直接取 115 的 `LOLK_PANEL_FADE_FRAMES = 20`**），
 *       **一次性**（淡掉就不再回来 —— owner 说「直接隐藏」，不是 115 那种"离开又回来"的连续抗遮挡）；
 *     · 只在自机**活着**（`player+0x476ac == 1`）时判 —— 死亡演出里那具"尸体"不算碰触
 *       （否则死在屏幕中央时会当场把自己的讯息点掉）。*/
#define SC_MSG_HALF_W   160.0f     /* 讯息贴图 320×46 的一半（尺寸改了就跟着改）*/
#define SC_MSG_HALF_H   23.0f
#define SC_MSG_TOUCH_PAD 6.0f      /* 外扩一点算"碰到"（自机自己也有体积）❓ 自定 */
#define SC_MSG_FADE     20         /* ★ 退场淡出帧数 = 115 的 LOLK_PANEL_FADE_FRAMES */

typedef struct {
    ce_option_t opt;               /* 跟班槽 */
    sc_state_t  st;                /* 星星池 + 开火节奏（纯逻辑）*/
    uint8_t     death_pending;     /* 死亡钩子里只置位（那一帧不建 VM）*/
    uint32_t    msg_id;            /* 讯息 VM（0 = 没有）*/
    uint32_t    msg_life;          /* 讯息剩余帧 */
    uint32_t    msg_fade_t;        /* 退场淡出已走帧数（0 = 没在退场）*/
    uint32_t    cannon_id;         /* ★ 我们**自绘**的炮身 VM（owner 实测：「炮体也得跟着旋转」）。
                                    *   为什么不直接转 option 自己那个 VM：它的旋转不归我们管
                                    *   （引擎的 option tick 可能每帧覆写）⇒ 自己画一个，位置读 option 的坐标、
                                    *   旋转与灰/白全由我们写；**option 那个 VM 置成透明**当"定位锚 + 收起语义"用。*/
    float       aim;               /* 当前瞄准角（没有目标时保持上一次，免得炮口乱甩）*/
    int32_t     shots;             /* 这一匣已经打了几颗（0..SC_MAGAZINE）*/
    int32_t     reload_left;       /* 装填剩余帧（> 0 = 灰着 + 不开火）*/
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

/* 自机碰到讯息贴图没有（世界 → 640 空间：x 以场地中心 320 为 0、y 以场地上沿 16 起算，与 112 的换算同源）。*/
static int sc_msg_touched(const float *pp)
{
    float x640, y640;
    if (!pp) return 0;
    x640 = 320.0f + pp[0];
    y640 = 16.0f + pp[1];
    if (x640 < SC_MSG_X - SC_MSG_HALF_W - SC_MSG_TOUCH_PAD) return 0;
    if (x640 > SC_MSG_X + SC_MSG_HALF_W + SC_MSG_TOUCH_PAD) return 0;
    if (y640 < SC_MSG_Y - SC_MSG_HALF_H - SC_MSG_TOUCH_PAD) return 0;
    if (y640 > SC_MSG_Y + SC_MSG_HALF_H + SC_MSG_TOUCH_PAD) return 0;
    return 1;
}

static void sc_msg_kill(sc_card_t *s)
{
    if (s->msg_id) ce_anm_delete(s->msg_id);
    s->msg_id = 0;
    s->msg_life = 0;
    s->msg_fade_t = 0;
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
    if (s->cannon_id) ce_anm_delete(s->cannon_id);
    s->cannon_id = 0;
    s->aim = 0.0f;
    sc_msg_kill(s);
    s->death_pending = 0;
    s->shots = 0;
    s->reload_left = 0;
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
        /* ⚠ 这里必须与 `on_tick_2` 的 `ce_option_get` 用**同一个锚脚本 392** —— 引擎开局
         *   （`on_power_level_change`）就是在这里建 option 的 VM 的；只改 tick 那处不够，
         *   否则 option 的 VM 依旧挂炮身贴图（owner 两次实测：「一直有一个闪烁的黑色虚影」→
         *   「依旧有一个炮身贴图」，两次都是这一处）。*/
        ce_option_alloc(c, &s->opt, SC_OPT_ANM_Y, CE_ANM_ABILITY_SCRIPT_STAR_CANNON_ANCHOR, "star_cannon");
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
            s->msg_fade_t = 0;
            ce_anm_set_pos(id, SC_MSG_X, SC_MSG_Y, 0.0f);
            ce_anm_set_color(id, 0xffffffffu);   /* 颜色烘在贴图里（一手：暖红 229,127,127、不描边）*/
            ce_log("star_cannon: death message shown (591 platinum version) at (%.0f, %.0f)",
                   (double)SC_MSG_X, (double)SC_MSG_Y);
        }
    }
    if (s->msg_id) {                     /* 挂满 15 s 再收（不再跟"复活了"走 —— owner 2026-10-03 定案）*/
        int alive = (*(int32_t *)(p + CE_PLAYER_STATE) == 1);
        if (!ce_anm_get_vm(s->msg_id) || s->msg_life == 0) {
            sc_msg_kill(s);
        } else if (s->msg_fade_t == 0 && alive && sc_msg_touched(ce_player_pos())) {   /* ★ 第一次碰到 ⇒ 起淡出 */
            s->msg_fade_t = 1;
            ce_log("star_cannon: death message dismissed (player touched it) - fading %d frames",
                   SC_MSG_FADE);
        } else if (s->msg_fade_t > 0) {                                   /* 115 式退场淡出（一次性）*/
            int a = (int)(255u * (SC_MSG_FADE - s->msg_fade_t) / SC_MSG_FADE);
            if (a < 0) a = 0;
            ce_anm_set_color(s->msg_id, ((uint32_t)a << 24) | 0x00ffffffu);
            if (s->msg_fade_t >= SC_MSG_FADE) {
                sc_msg_kill(s);
            } else {
                s->msg_fade_t++;
            }
        } else {
            s->msg_life--;
        }
    }

    /* ② 跟班（炮身）：占一个原生子机槽当**定位锚**（收起/展开语义白送），
     *   但**画面上的炮身是我们自绘的 VM** —— 这样炮口的旋转与灰/白完全归我们写。*/
    /* ⚠ option 挂的是**空白锚脚本 392**（不画东西）—— 早先挂 381（炮身贴图）再写 `color = 0` 想藏它，
     *   结果引擎只让 alpha 生效、RGB 留 0 ⇒ 屏上一个**黑色剪影**在闪（owner 实测的那个"黑色虚影"）。
     *   画面上的炮身是下面那个**自绘 VM**（脚本 381），旋转/灰白全归我们写。*/
    o = ce_option_get(c, &s->opt, SC_OPT_ANM_Y, CE_ANM_ABILITY_SCRIPT_STAR_CANNON_ANCHOR, SC_RETRY, "star_cannon");
    if (!o) {
        if (s->cannon_id) { ce_anm_delete(s->cannon_id); s->cannon_id = 0; }   /* 收起了 ⇒ 自绘的也收 */
        return 0;
    }
    sc_place(&s->opt);
    pz = ce_player_pos()[2];
    fx = ce_option_x(o);
    fy = ce_option_y(o);
    if (s->reload_left > 0) {                        /* ★ 装填中：灰着、不开火 */
        if (--s->reload_left == 0) {
            s->shots = 0;                            /* 装满了 ⇒ 计数归零 */
            ce_log("star_cannon: reload done - %d rounds ready", SC_MAGAZINE);
        }
    }
    if (s->cannon_id && !ce_anm_get_vm(s->cannon_id)) s->cannon_id = 0;   /* 被引擎收掉 ⇒ 当没有（防写进回收后的槽）*/
    if (!s->cannon_id) {                             /* 自绘炮身：第一次 tick 建出来 */
        s->cannon_id = ce_anm_spawn(CE_ABILITY_ANM(), CE_ANM_ABILITY_SCRIPT_STAR_CANNON_OPT, SC_LAYER);
        if (s->cannon_id && s->aim == 0.0f) s->aim = -1.5707964f;
    }

    /* 瞄准：最近的敌人；没有就朝正上（❓自定，我们的弹幕区没有光标）*/
    sc_aim_reset(&aim);
    if (em) {
        CE_ENEMY_FOREACH(em, enemy) {
            if (!ce_enemy_lockable(enemy)) continue;
            sc_aim_consider(&aim, fx, fy, ce_enemy_pos(enemy)[0], ce_enemy_pos(enemy)[1]);
        }
    }
    if (sc_aim_ready(&aim)) ang = bc_atan2f(aim.dy, aim.dx);  /* 有目标：算瞄准角 */
    else ang = s->aim;                                       /* 没目标：保持上一次（炮口不甩）*/
    s->aim = ang;
    if (s->cannon_id) {                                      /* ★ 炮体跟着目标转（自绘 VM）*/
        ce_anm_set_pos(s->cannon_id, fx, fy, pz);
        ce_anm_set_rotation(s->cannon_id, 0.0f, 0.0f, s->aim);
        ce_anm_set_color(s->cannon_id, s->reload_left > 0 ? 0xff909090u : 0xffffffffu);
    }

    /* ③ 星星：推进 + 自转 + 闪烁 + 判定；出界就收。*/
    for (i = 0; i < SC_SHOT_MAX; ++i) {
        sh = &s->st.s[i];
        if (!sh->active) continue;
        if (!ce_anm_get_vm(sh->id)) { sh->active = 0; sh->id = 0; continue; }
        if (sc_step(sh)) { ce_anm_delete(sh->id); sh->id = 0; continue; }
        ce_anm_set_pos(sh->id, sh->x, sh->y, pz);
        ce_anm_set_rotation(sh->id, 0.0f, 0.0f, sh->rot);
        ce_anm_set_color(sh->id, ((uint32_t)sc_draw_alpha(sh) << 24) | 0x00ffffffu);
        /* ★★ 2026-10-03（owner：「它还有一个**水滴形状的拖尾**，像彗星一样 …… 动态光照叠起来的东西，
         *   你得烘一个差不多的」）：在星星**身后**补一枚烘好的水滴形尾（脚本 391 自淡自毁，卡侧零簿记）。
         *   ⚠ 尾巴的 rotation 用**速度角**（`sh->vx/vy`），**不能用 `sh->rot`** —— 星是自转的
         *     （`aiStyle 5` 的 `rotation += speed*0.01*dir`），尾巴跟着转会拧成麻花。
         *   位置再往后让半个尾长（贴图右端是头 ⇒ 头正好落在星上）。*/
        {
            float va = bc_atan2f(sh->vy, sh->vx);
            float vd[2];
            uint32_t tl;
            ce_polar_to_vel(vd, va, 1.0f);
            tl = ce_anm_spawn(CE_ABILITY_ANM(), CE_ANM_ABILITY_SCRIPT_STAR_CANNON_TAIL, SC_LAYER);
            if (tl) {
                ce_anm_set_pos(tl, sh->x - vd[0] * SC_TAIL_BACK, sh->y - vd[1] * SC_TAIL_BACK, pz);
                ce_anm_set_rotation(tl, 0.0f, 0.0f, va);
                ce_anm_set_color(tl, ((uint32_t)sc_draw_alpha(sh) << 24) | 0x00ffffffu);
            }
        }
        if (sh->hit) {                                   /* 照原版 localNPCHitCooldown = 10：同一颗星
                                                          * 对同一个敌人每 10 帧才结算一次 */
            center[0] = sh->x; center[1] = sh->y; center[2] = pz;
            ce_damage_rect(center, 0.0f, SC_HIT_LIFE, SC_DMG, SC_HIT_SIZE, SC_HIT_SIZE);
        }
    }
    if (sc_live(&s->st) > 0) ce_raise_damage_cap(SC_CAP);

    /* ④ 开火：蓄满才发（池满就一直攒着，不丢已有星星）；**装填中不发**、**没按 Z 不发**。*/
    if (!sc_charge(&s->st)) return 0;
    if (s->reload_left > 0) return 0;
    /* ★ owner 实测口径：「星星炮要**瞄准敌人**开火」「关于 Z 参考豌豆射手」——豌豆射手那套开火门
     *   是引擎的原生子机射击（`ce_tick_shooters_for_card`）：**按住 Z + 活着 + 不在对话里**才发。
     *   我们的星星是卡自管的（要瞄准 / 要 Terraria 的弹道），所以这三条自己判，语义与之一致；
     *   再叠一条「**必须有可攻击的敌人**」—— 没目标不开火（原来朝正上乱打一梭子，owner 不要）。*/
    if ((CE_INPUT_HELD() & SC_INPUT_Z) == 0u) return 0;
    if (*(int32_t *)(p + CE_PLAYER_STATE) != 1) return 0;
    if (CE_GUI_MSG_ACTIVE()) return 0;
    if (!sc_aim_ready(&aim)) return 0;
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
    if (++s->shots >= SC_MAGAZINE) {                        /* ★ 打满一匣 ⇒ 变灰装填 */
        s->shots = SC_MAGAZINE;
        s->reload_left = SC_RELOAD_FRAMES;
        ce_log("star_cannon: magazine empty (%d rounds) - reloading %d frames",
               SC_MAGAZINE, SC_RELOAD_FRAMES);
    }
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
