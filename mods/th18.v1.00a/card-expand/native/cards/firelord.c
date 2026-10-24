/* 炎魔之王拉格纳罗斯（id 72）—— 主动卡：按 C 消耗 2.00 火力，召唤一尊不跟随自机的炎魔。FL_HP 点生命替玩家挡子弹
 * （每挡一发 −1，被挡的弹照炸弹消弹变点道具），每 FL_PERIOD 帧抽一个随机落点滑过去，到位那帧向场上**随机一个**敌人
 * 投一颗火球（飞行物，飞向开火时记下的坐标；飞行中每帧在自己坐标消弹 FL_BALL_CANCEL_R 开路），落地爆炸：消弹 FL_BLAST_CANCEL_R，
 * 然后连续 FL_BLAST_FRAMES 帧、每帧一个 FL_BLAST_W×H 的矩形伤害源，且每帧把每敌每帧伤害上限抬到 FL_BLAST_CAP（Remilia 同款，否则被自机弹吃满）。
 * 生命归零 → 死亡动画、结束；过关 / 局末消失。火力 < FL_POWER_GATE 拒绝（引擎永远保留 1.00，成本 + 一档才扣得实）。
 *
 * 原生原型：扣火力三步 = CardTsukasa__c_press（0x410e60：门槛 → spend_power → repopulate）；挡弹 = Tenshi 要石；
 * 伤害源 = Remilia 脉冲 / 青眼 / 破损核心同一个原语。随机 = 游戏自己的 REPLAY_SAFE_RNG。
 * 纯逻辑在 firelord_core.c（主机单测）；引擎调用是 sdk.h 的薄包装（AUDIT §V）。
 * 设计：docs/superpowers/specs/2026-09-06-firelord-design.md */
#include "sdk.h"
#include "firelord_core.h"

#define FL_COLOR_IDLE 0xffffffffu
#define FL_COLOR_HIT  0xffff8040u          /* 挡到弹那帧染橙（青眼是 Tenshi 的蓝 0xff0080ff）*/
#define FL_SE_SUMMON  CE_SE_RELEASE         /* 0x4d 发动音，语音 FIRELORD_SUMMON 叠在上面 */
#define FL_SE_BLAST   CE_SE_BOMB            /* 0x2c 火球落地 */
#define FL_SE_DEATH   0x29                  /* Tenshi 要石收场音（青眼同款）*/
#define FL_BAR_HIGH   0xffff9030u           /* > 50％ 橙 */
#define FL_BAR_MID    0xffffd040u           /* > 20％ 黄 */
#define FL_BAR_LOW    0xffff4040u           /* 红 */

/* 血条：复用青眼的两个 drawRect 根脚本（86 底槽 / 87 填充），C 每帧写 pos / scale / color（AUDIT O29k 同款）。*/
static void bar_update(fl_state_t *s)
{
    float ratio = s->hp > 0 ? (float)s->hp / (float)FL_HP : 0.0f;
    float w = FL_BAR_W * ratio, y = s->y + fl_bob_dy(s) + FL_BAR_DY;   /* 血条跟着呼吸浮动 */
    ce_anm_set_pos(s->bar_bg_id, s->x, y, s->z);
    ce_anm_set_scale(s->bar_bg_id, FL_BAR_W + 4.0f, FL_BAR_H + 4.0f);
    ce_anm_set_pos(s->bar_id, s->x - (FL_BAR_W - w) * 0.5f, y, s->z);
    ce_anm_set_scale(s->bar_id, w > 0.0f ? w : 0.0f, FL_BAR_H);
    ce_anm_set_color(s->bar_id, ratio > 0.5f ? FL_BAR_HIGH : ratio > 0.2f ? FL_BAR_MID : FL_BAR_LOW);
}

static void bar_destroy(fl_state_t *s, int fade)
{
    if (fade) { ce_anm_interrupt(s->bar_bg_id, 1); ce_anm_interrupt(s->bar_id, 1); }
    else      { ce_anm_delete(s->bar_bg_id);       ce_anm_delete(s->bar_id); }
    s->bar_bg_id = 0; s->bar_id = 0;
}

/* 爆炸演出（火球落地 / 登场共用）：script94 光环 + FL_BURST 颗 script96 火星 + 小震屏。音效由调用方放（落地 0x2c、登场 0x4d + 语音）。*/
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

#define FL_SWEEP_MONEY 1   /* ★ 2026-10-24 owner 削弱：3 → 1 张 */
static uint32_t s_fl_sweep_seen;

static void dismiss(ce_card_t *c, const char *why)
{
    fl_state_t *s = ce_state(c, fl_state_t);
    if (!s) return;
    /* 同青眼：这个条件是「有没有东西要清」的判据，必须点名每一个会被删的 VM —— `bar_bg_id` 原来漏在外面
     * （底槽起成功、填充起失败时它非 0 而 `bar_id` 是 0 ⇒ 只按 bar_id 判断会留下底槽）。
     * 2026-09-12 删卡审计补的防线，当前调用顺序下不可复现。*/
    if (s->anm_id || s->ball_id || s->bar_bg_id || s->bar_id || s->intro_left) {
        ce_anm_delete(s->anm_id);
        ce_anm_delete(s->ball_id);
        bar_destroy(s, 0);
        ce_log("firelord: dismissed (%s) hp %d after %u frames, %u moves, %u shots, %u blocked",
               why, s->hp, s->frames, s->moves, s->shots, s->blocked);
    }
    s->anm_id = 0; s->ball_id = 0; s->hp = 0; s->ball_active = 0; s->blast_left = 0; s->intro_left = 0; s->rings_left = 0;
}

/* 场上随机一个可锁定的敌人（与破损核心同一套过滤：EnemyManager+0x18c 链表、跳过 +0x635c & 0xc000021）。
 * 两遍遍历：先数、再按游戏 RNG 抽的下标取坐标。只在本帧读，不缓存 enemy 指针。返回 0 = 没目标。*/
static int pick_random_enemy(float *tx, float *ty, uint32_t *out_n, uint32_t *out_idx)
{
    uint8_t *em = CE_ENEMY_MGR(), *e;
    uint32_t n = 0, idx, i = 0;
    if (!em) return 0;
    CE_ENEMY_FOREACH(em, e) if (ce_enemy_lockable(e)) n++;
    if (!n) return 0;
    idx = fl_pick_index(ce_rand(), n);
    CE_ENEMY_FOREACH(em, e) {
        if (!ce_enemy_lockable(e)) continue;
        if (i++ == idx) {
            *tx = ce_enemy_pos(e)[0];
            *ty = ce_enemy_pos(e)[1];
            *out_n = n; *out_idx = idx;
            return 1;
        }
    }
    return 0;
}

static int on_activate(ce_card_t *c)
{
    /* ★ 2026-10-24：发动时把清扫代数对齐（否则"卡没生效时 92 打过"会留一个旧代数，
     *   本次刚召出来的东西会在下一次 tick 被当成 92 的对象清掉）。*/
    (void)ce_sweep_should_run(&s_fl_sweep_seen);
    uint8_t *p = CE_PLAYER();
    if (!p) return ce_refuse("firelord", "no player");
    int power = CE_CURRENT_POWER();
    if (power < FL_POWER_GATE) return ce_refuse("firelord", "power too low");
    fl_state_t *s = ce_state(c, fl_state_t);
    if (!s) return ce_refuse("firelord", "no state slot");

    const float *pp = ce_player_pos();
    fl_summon(s, pp[0], pp[1], pp[2]);                     /* 本体这帧还不出现：先 FL_INTRO_FRAMES 帧聚气（on_active_tick 的登场分支）*/

    /* 照 Tsukasa：spend_power → 无条件 repopulate（扣 2 档时档数必变；repopulate 顺手广播 on_power_level_change）*/
    int level_changed = ce_spend_power(FL_POWER_COST);
    ce_repopulate_options();
    ce_play_voice(FIRELORD_SUMMON, pp[0]);                 /* 语音先起；发动音 0x4d 留到本体出现那帧 */
    ce_log("firelord: summoning, power %d -> %d (level changed %d), hp %d, target (%.1f, %.1f), intro %u frames",
           power, CE_CURRENT_POWER(), level_changed, s->hp, s->x, s->y, s->intro_left);
    return 1;
}

static int on_active_tick(ce_card_t *c, uint32_t elapsed)
{
    fl_state_t *s = ce_state(c, fl_state_t);
    uint8_t *p = CE_PLAYER();
    if (!s || !p) return 0;
    /* ★★ 2026-10-24（92 清扫，owner：炎魔这类召唤实体也要清、换成 **3 张**金钱卡）*/
    {   /* ★★★ 2026-10-24 修 bug（owner：紧跟着 92 之后放的召唤物直接变钱卡）——
     *   原来 `if (s->anm_id && ce_sweep_should_run(&seen))` 是短路：92 那一刻场上没有召唤物
     *   时代数没被消费 ⇒ 之后任何一次召唤都会看到"旧代数" ⇒ 刚放出来的立刻被清成钱。
     *   ⇒ 代数每帧**无条件消费**，只有"当时真在场"的才清。*/
        const int gw_sweep = ce_sweep_should_run(&s_fl_sweep_seen);
        if (gw_sweep && s->anm_id) {
        float wp[3];
        int k;
        wp[0] = s->x; wp[1] = s->y; wp[2] = s->z;
        for (k = 0; k < FL_SWEEP_MONEY; ++k) {
            if (!ce_spawn_item(0x12u, wp, -1.5707964f, 1.7f)) ce_money_pocket(1);   /* ★ 0x12 = **金钱卡**（不是金钱点 type 2）*/
        }
        ce_log_key("firelord: 92 清扫 —— 炎魔换成金钱（%d 枚）", FL_SWEEP_MONEY);
        dismiss(c, "gw sweep");
        return 0;
    }
    }
    if (s->intro_left > 0) {                               /* 登场聚气：目标位置每帧几颗向中心汇聚的粒子；本体、判定、血条都还没有 */
        fl_step_t st = fl_step(s, 0);
        float pz = ce_player_pos()[2];
        if (st.gather) {
            for (int i = 0; i < FL_GATHER; i++) {
                uint32_t q = ce_anm_spawn(CE_ABILITY_ANM(), CE_ANM_ABILITY_SCRIPT_FIRELORD_GATHER, 13);
                if (q) ce_anm_set_pos(q, s->x, s->y, pz);
            }
        }
        if (st.appear) {                                   /* 本体出现 + 第一圈爆炸（光环 / 火星 / 震屏）+ 发动音 */
            s->anm_id = ce_anm_spawn(CE_ABILITY_ANM(), CE_ANM_ABILITY_SCRIPT_FIRELORD_BODY, 13);
            if (!s->anm_id) { ce_log("firelord: body anm failed at appear"); s->hp = 0; return 0; }
            ce_anm_set_pos(s->anm_id, s->x, s->y + fl_bob_dy(s), s->z);
            s->bar_bg_id = ce_anm_spawn(CE_ABILITY_ANM(), CE_ANM_ABILITY_SCRIPT_BLUE_EYES_BAR_BG, 13);
            s->bar_id    = ce_anm_spawn(CE_ABILITY_ANM(), CE_ANM_ABILITY_SCRIPT_BLUE_EYES_BAR, 13);
            bar_update(s);
            impact_fx(s->x, s->y + fl_bob_dy(s), s->z);
            ce_play_sound(FL_SE_SUMMON, s->x);
            ce_log("firelord: appeared at elapsed %u, anm id %08x, pos (%.1f, %.1f)", elapsed, s->anm_id, s->x, s->y);
        }
        return 1;
    }
    if (!ce_anm_get_vm(s->anm_id)) {                       /* 引擎把 VM 收掉了（O29h 同款）：本体没了就结束 */
        ce_log("firelord: body vm gone at frame %u", s->frames);
        s->anm_id = 0;
        ce_anm_delete(s->ball_id); s->ball_id = 0;
        bar_destroy(s, 0);
        return 0;
    }
    float pz = ce_player_pos()[2];
    float pos[3] = { s->x, s->y + fl_bob_dy(s), s->z };      /* 判定跟画面：呼吸浮动一起算 */
    int blocked = s->hp > 0 ? ce_cancel_radius(pos, FL_RADIUS, s->hp, 0) : 0;
    ce_anm_set_color(s->anm_id, blocked ? FL_COLOR_HIT : FL_COLOR_IDLE);

    fl_step_t st = fl_step(s, blocked);
    ce_anm_set_pos(s->anm_id, s->x, s->y + fl_bob_dy(s), s->z);   /* 逻辑位置 (x, y) + 呼吸浮动 */
    bar_update(s);
    if (st.ring) {                                         /* 登场第 2、3 圈：只有光环 + 轻震，火星只撒第一圈 */
        uint32_t b = ce_anm_spawn(CE_ABILITY_ANM(), CE_ANM_ABILITY_SCRIPT_FIRELORD_BLAST, 13);
        if (b) ce_anm_set_pos(b, s->x, s->y + fl_bob_dy(s), s->z);
        ce_screen_shake(FL_SHAKE_TIME / 2, FL_SHAKE_START - 2, FL_SHAKE_END);
    }
    for (int i = 0; i < FL_AURA; i++) {                    /* 常态身体火焰：脚本自己在身体范围内抽起点、向上飘（UI RNG，不动 replay 流）*/
        uint32_t q = ce_anm_spawn(CE_ABILITY_ANM(), CE_ANM_ABILITY_SCRIPT_FIRELORD_AURA, 13);
        if (q) ce_anm_set_pos(q, s->x, s->y + fl_bob_dy(s), s->z);
    }

    if (st.need_move) {
        fl_begin_move(s, ce_rand());
        ce_log("firelord: move #%u at frame %u: (%.1f, %.1f) -> (%.1f, %.1f)", s->moves, s->frames, s->sx, s->sy, s->tx, s->ty);
    }
    if (st.fire) {
        float tx, ty; uint32_t n, idx;
        if (pick_random_enemy(&tx, &ty, &n, &idx)) {
            fl_launch(s, tx, ty);
            ce_anm_delete(s->ball_id);                     /* 理论上到不了这（周期 ≫ 飞行时间），保险 */
            s->ball_id = ce_anm_spawn(CE_ABILITY_ANM(), CE_ANM_ABILITY_SCRIPT_FIRELORD_FIREBALL, 13);
            ce_play_voice(FIRELORD_ATTACK, s->x);
            ce_log("firelord: shot #%u at frame %u: enemy %u/%u at (%.1f, %.1f), %u frames of flight, angle %.3f",
                   s->shots, s->frames, idx, n, tx, ty, s->ball_left, s->bang);
        } else {
            ce_log("firelord: no target at frame %u (move #%u done)", s->frames, s->moves);
        }
    }
    if (s->ball_active && s->ball_id) {                    /* 火球：pos / rotation 归 C，scale 归脚本（脉动）*/
        ce_anm_set_pos(s->ball_id, s->bx, s->by, pz);
        ce_anm_set_rotation(s->ball_id, 0.0f, 0.0f, s->bang);
    }
    if (st.trail) {
        uint32_t t = ce_anm_spawn(CE_ABILITY_ANM(), CE_ANM_ABILITY_SCRIPT_FIRELORD_TRAIL, 13);
        if (t) ce_anm_set_pos(t, s->bx, s->by, pz);
    }
    if (s->ball_active) {                                  /* 飞行开路：火球坐标周围的弹变点道具（青眼 / 本体挡弹同一函数，数量不限、不动 hp）*/
        float bpos[3] = { s->bx, s->by, pz };
        s->ball_cancelled += (uint32_t)ce_cancel_radius(bpos, FL_BALL_CANCEL_R, FL_CANCEL_MAX, 0);
    }
    if (s->ball_active) {                                  /* 橙白粒子：每帧几颗，钉在火球当前坐标，各自在脚本里用 ANM 自己的随机飘开（UI RNG，不动 replay 流）*/
        for (int i = 0; i < FL_PARTICLES; i++) {
            uint32_t q = ce_anm_spawn(CE_ABILITY_ANM(), CE_ANM_ABILITY_SCRIPT_FIRELORD_PARTICLE, 13);
            if (q) ce_anm_set_pos(q, s->bx, s->by, pz);
        }
    }
    if (st.ball_arrived) {                                 /* 打击感：落地消弹 + 爆炸光环 + 一圈火星 + 小震屏 + 0x2c */
        float epos[3] = { s->ex, s->ey, pz };
        int n = ce_cancel_radius(epos, FL_BLAST_CANCEL_R, FL_CANCEL_MAX, 0);
        s->blast_cancelled += (uint32_t)n;
        ce_anm_delete(s->ball_id); s->ball_id = 0;
        impact_fx(s->ex, s->ey, pz);
        ce_play_sound(FL_SE_BLAST, s->ex);
        ce_log("firelord: blast at (%.1f, %.1f): cancelled %d here + %u in flight; %d frames x %d dmg with cap %d",
               s->ex, s->ey, n, s->ball_cancelled, FL_BLAST_FRAMES, FL_BLAST_DMG, FL_BLAST_CAP);
        s->ball_cancelled = 0;
    }
    if (st.blast_dmg) {                                    /* 每帧一个**新**源：一个源对同一敌人只结算一次（AUDIT U9）*/
        float center[3] = { s->ex, s->ey, pz };
        ce_raise_damage_cap(FL_BLAST_CAP);                 /* 当帧有效（GameThread 每帧复位）：让这 100 落在自机弹之上而不是被它吃掉（V20）*/
        ce_damage_rect(center, 0.0f, 2, st.blast_dmg, FL_BLAST_W, FL_BLAST_H);
    }
    if (elapsed % 60 == 0 && blocked) ce_log("firelord: hp %d (blocked %u, frame %u)", s->hp, s->blocked, s->frames);
    if (st.died) {
        ce_anm_interrupt(s->anm_id, 1);                    /* 脚本 interruptLabel(1)：放大淡出后 delete */
        ce_anm_delete(s->ball_id);
        bar_destroy(s, 1);
        ce_play_sound(FL_SE_DEATH, s->x);
        ce_log("firelord: died after %u frames, %u moves, %u shots, %u blocked", s->frames, s->moves, s->shots, s->blocked);
        s->anm_id = 0; s->ball_id = 0; s->ball_active = 0; s->blast_left = 0;
        return 0;
    }
    return 1;
}

static void on_stage_start(ce_card_t *c) { dismiss(c, "stage start"); }
static void on_run_reset(ce_card_t *c)   { dismiss(c, "run reset"); }

/* ★ 2026-09-10（同 blue_eyes 的边界情况）：倒吊人是**商店里**成交的即时卡（ctor 里删卡），
 * 那时 `AbilityManager::on_tick` 被 `ABILITY_SHOP_PTR` 冻住 ⇒ `on_active_tick` 不会再跑，
 * 只有 `on_delete`（虚表 +0x50）能收尾。不收的话炎魔本体 + 血条 VM 会永久留在场上。*/
static void on_delete(ce_card_t *c) { dismiss(c, "card destroyed"); }

CE_CARD(72, .active_recharge = FL_RECHARGE, .on_activate = on_activate, .on_active_tick = on_active_tick,
            .on_stage_start = on_stage_start, .on_run_reset = on_run_reset, .on_delete = on_delete);
