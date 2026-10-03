/* 鞘翅重锤（id 91）—— **主动卡**，致敬《Minecraft》的**鞘翅 + 重锤**
 *（1.21 的那套「滑翔过去一锤砸下」；贴图直接取用户本机 1.21.10 客户端 jar 里的
 * `item/mace.png` 与 `item/elytra.png`，见 `entries/ORDER.txt` 末段与 AUDIT §BE 详录 #215）。
 *
 * ## owner 2026-10-02 逐字
 * 「发动之后会进入无敌状态，并有一个类似于 Undertale 的时机条；当有敌人可攻击的时候，时机条会开始动，
 *   到中间时机的时候需要再按下 C；让自机瞬移 or 快速直线移动到敌人身边，重锤砸下，造成高额伤害，
 *   然后回到原位。当 3s 没有可攻击的敌人或者时机没卡准的时候，自动退出，会再给你一点点无敌帧作为缓冲。」
 *
 * ## 状态机（纯逻辑在 `elytra_mace_core.c`，主机单测 `tests/test_elytra_mace_core.c`）
 * ```
 * IDLE ─(C 发动)→ AIM ─(C 边沿 && 光标在窗口内)→ SMASH ─(18 帧)→ EXIT ─(30 帧缓冲)→ IDLE
 *                   ├─(C 边沿 && 窗口外)→ EXIT（时机没卡准）
 *                   ├─(连续 3s 没有可攻击的敌人)→ EXIT
 *                   └─(瞄准 3s 总预算到)→ EXIT
 * ```
 * 逐条落点：
 * · **无敌**：全程 `ce_invuln_request(id, em_invuln_left())`（声明层，SDK 每帧取最大值落笔）；
 *   `EM_EXIT_INVULN = 30` 帧 = owner 那句「再给你一点点无敌帧作为缓冲」。
 * · **时机条**：`script384`（条，layer 24）+ `script385`（光标，layer 25）；有敌人时光标左右扫，
 *   没有就停住（判据在 core 里）；窗口 = `EM_WINDOW`，与贴图中间那道亮带同宽。
 * · **瞬移**：成功那一帧直接写 `player+0x620/624`（本卡是本仓**第一张**写自机坐标的卡 ——
 *   写的是引擎每帧都会重读的那个 float3，见 `CE_PLAYER_X` 的注）；砸完再写回**发动时记下的原位**。
 * · **高额伤害**：`ce_damage_rect` 一锤 `EM_SMASH_DMG = 800`（同时 `ce_raise_damage_cap` 抬当帧上限，
 *   否则会被引擎的「每敌每帧上限」削到 90）。
 * · 引擎的发动门是 `card->state == 0 && recharge_cur < 1` ⇒ 我们进 AIM 之后充能已经在跑，
 *   第二次 C **不会再触发一次发动**，所以那一下可以安全地当"确认键"读（`INPUT_PRESSED & 0x400`）。
 * · **激活那一帧的 C 边沿要丢掉**（core 的 `armed`）：否则卡片会在同一次按键上自己确认自己。
 *
 * ❓ 自定（进 `TUNING.md` §2.10）：光标速度 / 判定窗口宽度 / 砸击动画帧数 / 缓冲 30 帧 /
 *   伤害 800 / 判定盒 140×140 / 停在敌人上方 46 px / 「可攻击」的半径 520 px。 */
#include "sdk.h"
#include "elytra_mace_core.h"

#define EM_CARD_BAR_LAYER    24         /* UI 表面（早于暂停模糊罩）*/
#define EM_CARD_CUR_LAYER    25         /* 光标：压在条上面 */
#define EM_CARD_HAM_LAYER    22         /* 锤子（❓ 压在自机之上；被盖住就调大）*/
#define EM_CARD_WING_LAYER   19         /* 鞘翅（❓ 走自机之下，当背上的翅膀）*/
#define EM_CARD_BAR_X        320.0f     /* 640×480 空间：游玩区正中 */
#define EM_CARD_BAR_Y        240.0f
#define EM_CARD_C_INPUT      0x400u     /* `INPUT_PRESSED` 的 C 位（一手 `0x45c084`）*/
#define EM_CARD_SE_SLASH     0x2d       /* TH18 音效表 0x2d = se_slash.wav（重锤落下那一声 ❓ 借现成的）*/
#define EM_RECHARGE          2400u      /* ❓自定：40 s（与 `patch/th18/cards.js` 的 active_recharge 同值；
                                         *   契约检查要求 C 侧与卡表两侧都在）*/
#define EM_CARD_HAM_Y0       (-72.0f)   /* 锤子起手：目标上方 72 px */
#define EM_CARD_HAM_Y1       4.0f       /* 落点：目标位置 */
#define EM_CARD_HAM_ROT0     (-0.85f)   /* 起手角（右上）*/
#define EM_CARD_HAM_ROT1     0.45f      /* 落点角（左下）*/

typedef struct {
    em_state_t st;              /* 时机条状态机（纯逻辑）*/
    uint32_t bar_id;            /* 条 VM */
    uint32_t cur_id;            /* 光标 VM */
    uint32_t ham_id;            /* 锤子 VM */
    uint32_t wing_id;           /* 鞘翅 VM */
    float    home_x, home_y;    /* 发动那一刻的自机坐标（砸完写回）*/
    float    tgt_x, tgt_y;      /* 目标敌人（HIT 那一刻锁存）*/
    uint8_t  homed;             /* home_* 有效 */
    uint8_t  switched_away;     /* 本帧发现玩家把卡切走了（D）*/
    uint8_t  dmg_done;          /* 这一锤的伤害只出一次 */
    uint8_t  *self;             /* 本卡对象（on_activate 时记下；用来比对"选中卡是不是我"）*/
} em_card_t;

_Static_assert(sizeof(em_card_t) <= 496, "elytra_mace: private state overflows CE_STATE_BYTES");

/* ---------------------------------------------------------------- 小工具 */

static void em_kill(uint32_t *id)
{
    if (*id) ce_anm_delete(*id);
    *id = 0;
}

/* 把演出用的三个 VM 全收掉（条 / 光标 / 锤子 / 鞘翅）。*/
static void em_kill_fx(em_card_t *s)
{
    em_kill(&s->bar_id);
    em_kill(&s->cur_id);
    em_kill(&s->ham_id);
    em_kill(&s->wing_id);
}

/* 写自机坐标（`CE_PLAYER_X` 是引擎每帧重读的 float3；z 不动）。*/
static void em_set_player(float x, float y)
{
    uint8_t *p = CE_PLAYER();
    if (!p) return;
    *(float *)(p + CE_PLAYER_X) = x;
    *(float *)(p + CE_PLAYER_Y) = y;
}

/* 找一个「可攻击」的敌人（可锁定 + 在 `EM_TARGET_RANGE` 内），返回最近的。1 = 找到。*/
static int em_find_target(float px, float py, float *out_x, float *out_y)
{
    uint8_t *em = CE_ENEMY_MGR();
    uint8_t *e;
    float best = EM_TARGET_RANGE * EM_TARGET_RANGE;
    int found = 0;

    if (!em) return 0;
    CE_ENEMY_FOREACH(em, e) {
        const float *ep;
        float dx, dy, d2;
        if (!ce_enemy_lockable(e)) continue;
        ep = ce_enemy_pos(e);
        dx = ep[0] - px;
        dy = ep[1] - py;
        d2 = dx * dx + dy * dy;
        if (d2 < best) {
            best = d2;
            *out_x = ep[0];
            *out_y = ep[1];
            found = 1;
        }
    }
    return found;
}

/* ---------------------------------------------------------------- 卡片回调 */

static void em_stop(ce_card_t *c)
{
    em_card_t *s = ce_state(c, em_card_t);
    if (!s) return;
    if (s->st.state == EM_SMASH && s->homed) em_set_player(s->home_x, s->home_y);  /* 砸到一半被删 ⇒ 先回位 */
    em_kill_fx(s);
    ce_invuln_request(ce_card_id(c), 0);
    em_reset(&s->st);
    s->homed = 0;
    s->dmg_done = 0;
    s->switched_away = 0;
    s->self = 0;
}

static void on_run_reset(ce_card_t *c) { em_stop(c); }
static void on_delete(ce_card_t *c)    { em_stop(c); }

static int on_load(ce_card_t *c)
{
    em_stop(c);
    return 0;
}

/* ★ 发动（引擎在 C 键 + 该卡选中 + 充能好了时调）：记原位、起时机条、进 AIM。
 * ⚠ 这一帧的 C 边沿归引擎（core 的 `armed` 会把它丢掉）。*/
static int on_activate(ce_card_t *c)
{
    em_card_t *s = ce_state(c, em_card_t);
    const float *pp = ce_player_pos();

    if (!s) return 0;
    if (s->st.state != EM_IDLE) return 0;            /* 已经在演出里（正常到不了这里）*/
    s->home_x = pp ? pp[0] : 0.0f;
    s->home_y = pp ? pp[1] : 0.0f;
    s->homed = 1;
    s->dmg_done = 0;
    s->switched_away = 0;
    s->self = (uint8_t *)c;                          /* 比对"选中卡还是不是我" */
    em_kill_fx(s);
    em_begin(&s->st);                                /* ★ core：进 AIM、丢第一帧 C */

    s->bar_id = ce_anm_spawn(CE_ABILITY_ANM(), CE_ANM_ABILITY_SCRIPT_ELYTRA_MACE_BAR, EM_CARD_BAR_LAYER);
    if (s->bar_id) {
        ce_anm_set_pos(s->bar_id, EM_CARD_BAR_X, EM_CARD_BAR_Y, 0.0f);
        ce_anm_set_color(s->bar_id, 0xffffffffu);
    }
    s->cur_id = ce_anm_spawn(CE_ABILITY_ANM(), CE_ANM_ABILITY_SCRIPT_ELYTRA_MACE_CURSOR, EM_CARD_CUR_LAYER);
    if (s->cur_id) {
        ce_anm_set_pos(s->cur_id, EM_CARD_BAR_X - EM_BAR_HALF, EM_CARD_BAR_Y, 0.0f);
        ce_anm_set_color(s->cur_id, 0xffffffffu);
    }
    ce_play_sound(CE_SE_RELEASE, ce_player_x());
    ce_log("elytra_mace: aim started at (%.1f, %.1f) - press C inside the yellow band",
           (double)s->home_x, (double)s->home_y);
    return 0;
}

/* +0x2c：AbilityManager 每帧（菜单 / 商店 / 暂停里不跑 ⇒ 演出自然冻住）。*/
static int on_tick_2(ce_card_t *c)
{
    em_card_t *s = ce_state(c, em_card_t);
    uint8_t *p = CE_PLAYER();
    const float *pp;
    float px, py, tx = 0.0f, ty = 0.0f;
    int has_target, ev;

    if (!s || !p) return 0;
    if (s->st.state == EM_IDLE) {                    /* 不在演出里：只保证声明退干净 */
        ce_invuln_request(ce_card_id(c), 0);
        return 0;
    }

    pp = ce_player_pos();
    px = pp ? pp[0] : 0.0f;
    py = pp ? pp[1] : 0.0f;
    has_target = em_find_target(px, py, &tx, &ty);

    /* ★ owner 2026-10-02 追加：「此时按 D 会切卡，如果切到了别的卡，退出该状态，
     *   给你一点点无敌帧作为缓冲」⇒ 选中卡不是我了就走**同一条**退出路径（EM_EXIT 的缓冲）。*/
    {
        uint8_t *mgr = CE_ABILITY_MGR();
        uint8_t *sel = mgr ? *(uint8_t **)(mgr + CE_MGR_SELECTED_ACTIVE) : 0;
        if (s->st.state == EM_AIM || s->st.state == EM_SMASH) {
            if (s->self && sel != s->self) {
                if (s->st.state == EM_SMASH && s->homed) em_set_player(s->home_x, s->home_y);
                em_kill(&s->ham_id);
                em_kill(&s->wing_id);
                if (s->bar_id) { ce_anm_delete(s->bar_id); s->bar_id = 0; }
                em_kill(&s->cur_id);
                s->st.state = EM_EXIT;               /* 缓冲无敌帧那一段照走 */
                s->st.frames = 0;
                ce_log("elytra_mace: card switched away (D) - exiting with %d invuln frames", EM_EXIT_INVULN);
                ce_invuln_request(ce_card_id(c), EM_EXIT_INVULN);
                return 0;
            }
        }
    }

    /* 光标：有敌人时左右扫（判据在 core 里）；位置每帧写 */
    ev = em_tick(&s->st, has_target, (*(volatile uint32_t *)CE_ADDR_INPUT_PRESSED & EM_CARD_C_INPUT) != 0);
    if (s->cur_id) {
        ce_anm_set_pos(s->cur_id, EM_CARD_BAR_X + s->st.cursor, EM_CARD_BAR_Y, 0.0f);
        /* 在窗口里染暖黄、外面纯白（和条上那道亮带同色，玩家一眼能对）*/
        ce_anm_set_color(s->cur_id, em_in_window(&s->st) ? 0xff5cd6fau : 0xffffffffu);
    }

    switch (ev) {
    case EM_EV_HIT:
        /* ★ 卡准了：锁存目标、瞬移过去、抡锤、出伤害 */
        s->tgt_x = tx;
        s->tgt_y = ty;
        s->dmg_done = 0;
        em_set_player(tx, ty + EM_SMASH_OFFSET_Y);
        em_kill(&s->cur_id);
        if (s->bar_id) { ce_anm_delete(s->bar_id); s->bar_id = 0; }
        s->ham_id = ce_anm_spawn(CE_ABILITY_ANM(), CE_ANM_ABILITY_SCRIPT_ELYTRA_MACE_HAMMER, EM_CARD_HAM_LAYER);
        s->wing_id = ce_anm_spawn(CE_ABILITY_ANM(), CE_ANM_ABILITY_SCRIPT_ELYTRA_MACE_WING, EM_CARD_WING_LAYER);
        ce_play_sound(EM_CARD_SE_SLASH, tx);          /* ❓ 借 TH18 的斩击音（重锤落下的那一声）*/
        ce_log("elytra_mace: HIT - teleported (%.1f, %.1f) -> target (%.1f, %.1f)",
               (double)px, (double)py, (double)tx, (double)ty);
        break;
    case EM_EV_MISS:
        ce_log("elytra_mace: MISS (cursor %.1f px off center) - backing off with %d invuln frames",
               (double)s->st.cursor, EM_EXIT_INVULN);
        if (s->bar_id) { ce_anm_delete(s->bar_id); s->bar_id = 0; }
        em_kill(&s->cur_id);
        break;
    case EM_EV_TIMEOUT:
        ce_log("elytra_mace: timeout (no attackable enemy for 3s or aim budget over) - exiting");
        if (s->bar_id) { ce_anm_delete(s->bar_id); s->bar_id = 0; }
        em_kill(&s->cur_id);
        break;
    case EM_EV_RETURN:
        em_set_player(s->home_x, s->home_y);         /* ★ 回到原位 */
        em_kill(&s->ham_id);
        em_kill(&s->wing_id);
        /* ★★ 2026-10-03 owner：「鞘翅重锤应当能**连续作用**」⇒ core 砸完回 AIM 连段；
         *   但**时机条 VM 是在 HIT 那一帧被删掉的**（见上面的 EM_EV_HIT）⇒ 这里必须**重建**，
         *   否则第二锤起就没有条可看（玩家只能盲按）✗。光标 VM 不删，core 会把光标位置重置到左端。*/
        if (!s->bar_id) {
            s->bar_id = ce_anm_spawn(CE_ABILITY_ANM(), CE_ANM_ABILITY_SCRIPT_ELYTRA_MACE_BAR, EM_CARD_BAR_LAYER);
            if (s->bar_id) {
                ce_anm_set_pos(s->bar_id, EM_CARD_BAR_X, EM_CARD_BAR_Y, 0.0f);
                ce_anm_set_color(s->bar_id, 0xffffffffu);
            }
        }
        ce_log("elytra_mace: smash done - back home (%.1f, %.1f) ⇒ 回 AIM 连段", (double)s->home_x, (double)s->home_y);
        break;
    case EM_EV_DONE:
        em_kill_fx(s);
        break;
    default:
        break;
    }

    /* 砸击那 18 帧：锤子从右上抡到左下、鞘翅跟着自机；伤害在起手那一帧出一次 */
    if (s->st.state == EM_SMASH) {
        float t = (float)s->st.frames / (float)EM_SMASH_FRAMES;
        if (t > 1.0f) t = 1.0f;
        if (s->ham_id) {
            ce_anm_set_pos(s->ham_id, s->tgt_x, s->tgt_y + EM_CARD_HAM_Y0 + (EM_CARD_HAM_Y1 - EM_CARD_HAM_Y0) * t, 0.0f);
            ce_anm_set_rotation(s->ham_id, 0.0f, 0.0f, EM_CARD_HAM_ROT0 + (EM_CARD_HAM_ROT1 - EM_CARD_HAM_ROT0) * t);
            ce_anm_set_color(s->ham_id, 0xffffffffu);
        }
        if (s->wing_id) {
            ce_anm_set_pos(s->wing_id, px, py, 0.0f);
            ce_anm_set_color(s->wing_id, 0xffffffffu);
        }
        if (!s->dmg_done) {
            float center[3];
            s->dmg_done = 1;
            center[0] = s->tgt_x; center[1] = s->tgt_y; center[2] = 0.0f;
            ce_raise_damage_cap(EM_SMASH_DMG);       /* ★ 不抬就被「每敌每帧上限」（90）削掉 */
            ce_damage_rect(center, 0.0f, 2, EM_SMASH_DMG, EM_SMASH_W, EM_SMASH_H);
        }
    }

    ce_invuln_request(ce_card_id(c), em_invuln_left(&s->st));   /* 全程声明无敌（含退出缓冲）*/
    return 0;
}

CE_CARD(91, .active_recharge = EM_RECHARGE,
            .on_activate = on_activate, .on_tick_2 = on_tick_2,
            .on_load = on_load, .on_run_reset = on_run_reset, .on_delete = on_delete);
