/* 东尼的悲恸（id 86）—— 被动卡，致敬《泰拉瑞亚》的……（❓ 名字出处待 owner 确认，先不写致敬行）。
 *
 * 规则（owner 2026-10-01 逐字）：「若用时 16s 之内成功收取一张符卡，则下一个非符血量降为 1。」
 *
 * 判据全部是一手（详细出处见 `tony_grief_core.h` 的文件头）：
 *   · 成功收取 ⟺ `zSpellcard+0x78` 的 bit1 在 bit0 清掉那一帧仍置位，且 bit7（已超时）未置
 *     （`Spellcard__end` `0x42a780`：`0x42a7c2` 先清 bit0、`0x42a7ed` 再拿 bit1 决定计分/失败；全程不写 bit1）；
 *   · 用时 ⟺ `zSpellcard+0x24`（宣言清零、每帧 +1）≤ 960；
 *   · 下一个非符 ⟺ 段边界（活动中断槽签名变化）处 bit0 = 0，且不是死亡段。
 * 落笔只写 `zEnemyData.life.current`（`zEnemy+0x6220`，非符段的权威血量；`maximum` 不动，血条自然见底），
 * 段内每帧幂等补写（压过同段内可能更晚的 `lifeSet`），段签名一变立刻停。
 *
 * **跨帧外部资源**：只有触发演出那一个**卡面虚影 VM**（脚本 393）—— 它**自淡自毁**（72 帧内 delete），
 * 所以不占卡侧簿记、也不需要 `on_delete` 去收（删卡时它顶多把剩下那几十帧飘完，无害）；
 * 其余无（无子机 / 无 HUD / 无伤害源 / 无全局旋钮）⇒ `on_delete` 只需清自己那份 `ce_state`
 * （真释放由 SDK 的 `ce_state_free` 做）；换关 / 局末也用同一套清零。
 * 纯逻辑在 `tony_grief_core.c`（主机单测 `tests/test_tony_grief_core.c`）；槽原语复用古锭刀（76）。 */
#include "sdk.h"
#include "tony_grief_core.h"

/* ★ 2026-10-03 owner：「东尼的悲恸加一个东西：**触发的时候 BOSS 身上出现一个卡面的虚影**」＋
 *   「然后触发的时候**发一声这个**：`…/th18/data/se_wolf.wav`」⇒ 触发那一刻（= 第一次把 BOSS 血量
 *   写成 1 的那一帧）在 BOSS 坐标起一个脚本 393 的卡面虚影（脚本自淡自毁 ⇒ 卡侧零簿记），
 *   同时播一枚 SE（voice id 0x62 = TH18 自己的 `se_wolf.wav`，逐字节现取，见 `derive_retail_se.py`）。*/
#define TG_GHOST_LAYER   14         /* 与脚本 393 的 `layer(14)` 一致 */

typedef struct {
    tg_state_t st;
} tg_card_t;

_Static_assert(sizeof(tg_card_t) <= 496, "tony_grief: private state overflows CE_STATE_BYTES");

/* 找「第一个有活动中断槽的 boss」，带出它的 enemy_id / 槽签名 / 是否死亡段。
 * 照 `cards/guding.c` 的 final_spell_now 与 `cards/animal.c` 的 animal_boss_enemy（同一套 boss_ids 走法）。 */
static uint8_t *tg_first_active_boss(uint8_t *em, uint32_t *out_id, int32_t *out_sig, int *out_final)
{
    unsigned i;

    *out_id = 0;
    *out_sig = 0;
    *out_final = 0;
    if (!em) return 0;
    for (i = 0; i < CE_EM_BOSS_SLOTS; ++i) {
        int32_t id = *(int32_t *)(em + CE_EM_BOSS_IDS + i * 4);
        uint8_t *e;
        int32_t sig;
        int fin = 0;
        if (!id) continue;
        e = ce_enemy_by_id(em, id);
        if (!e) continue;
        sig = tg_active_slot(e + CE_ENEMY_DATA + CE_ED_INTERRUPTS, CE_ED_INTERRUPT_SLOTS, &fin);
        if (!sig) continue;
        *out_id = (uint32_t)id;
        *out_sig = sig;
        *out_final = fin;
        return e;
    }
    return 0;
}

/* +0x2c：AbilityManager 每帧（菜单 / 商店 / 暂停里不跑）。优先级 0x16，早于 EnemyManager 的 0x1b
 * ⇒ 本帧落笔的血量，敌人本帧的伤害结算就按它算（一枪即死）。 */
static int on_tick_2(ce_card_t *c)
{
    tg_state_t *s = ce_state(c, tg_state_t);
    tg_input_t in;
    tg_out_t out;
    uint8_t *em, *sc;
    uint32_t f;

    if (!s) return 0;                                  /* ce_state 槽耗尽时返回 NULL（同批其它卡都判）*/

    f = ce_spell_flags();
    sc = CE_SPELLCARD();
    in.spell_active = (f & CE_SPELL_FLAG_ACTIVE) != 0;
    in.spell_bonus = (f & CE_SPELL_FLAG_BONUS) != 0;
    in.spell_timed_out = (f & CE_SPELL_FLAG_TIMED_OUT) != 0;
    in.spell_elapsed = sc ? *(int32_t *)(sc + CE_SPELL_ELAPSED) : 0;

    em = CE_ENEMY_MGR();
    in.boss_id = 0;
    in.boss_sig = 0;
    in.boss_final = 0;
    tg_first_active_boss(em, &in.boss_id, &in.boss_sig, &in.boss_final);
    in.have_boss = in.boss_sig != 0;

    in.target_present = 0;
    in.target_sig_now = 0;
    if (s->target_live) {                              /* 目标 boss 的**当前**签名（判段结束用）*/
        uint8_t *e = ce_enemy_by_id(em, (int32_t)s->target_id);
        if (e) {
            int fin = 0;
            int32_t sig = tg_active_slot(e + CE_ENEMY_DATA + CE_ED_INTERRUPTS, CE_ED_INTERRUPT_SLOTS, &fin);
            in.target_present = sig != 0;
            in.target_sig_now = sig;
        }
    }

    out = tg_frame(s, &in);

    if (out.captured)
        ce_log("tony_grief: 速收符卡（用时 %d 帧 ≤ %d）—— 下一个非符血量降为 1",
               out.captured_ms, TG_MAX_FRAMES);
    if (out.armed)
        ce_log("tony_grief: 锁定非符段（boss %u，签名 %08x）", s->target_id, (unsigned)s->target_sig);
    if (out.released)
        ce_log("tony_grief: 目标段结束（本局速收 %u 次 / 改写 %u 段）", s->quick, s->armed_n);
    if (out.expired)
        ce_log("tony_grief: 等了 %d 帧没等到下一段，放弃", TG_WAIT_FRAMES);

    if (out.armed) s->ghost_done = 0;               /* 新锁一段 ⇒ 下一帧的触发演出重新放一次 */

    if (out.want_write) {
        uint8_t *e = ce_enemy_by_id(em, (int32_t)s->target_id);
        if (e) {
            int32_t *hp = (int32_t *)(e + CE_ENEMY_HP_CUR);
            if (*hp > 1) {
                if (!out.armed && *hp != 1)
                    ce_log("tony_grief: 段内补写 boss %u 血量 %d -> 1", s->target_id, *hp);
                *hp = 1;
            }
            if (!s->ghost_done) {                   /* ★ 触发表演：卡面虚影（BOSS 身上）+ 一声 se_wolf */
                const float *ep = ce_enemy_pos(e);
                uint32_t vm = ce_anm_spawn(CE_ABILITY_ANM(), CE_ANM_ABILITY_SCRIPT_TONY_CARD_GHOST,
                                           TG_GHOST_LAYER);
                if (vm) ce_anm_set_pos(vm, ep[0], ep[1], ep[2]);   /* 只写这一次，位移/淡出归脚本 */
                ce_play_sound(CE_VOICE_TONY_GRIEF_TRIGGER, ep[0]);
                s->ghost_done = 1;
                ce_log("tony_grief: 触发演出 —— 卡面虚影 + se_wolf @ boss %u (%.1f, %.1f)",
                       s->target_id, (double)ep[0], (double)ep[1]);
            }
        }
    }
    return 0;
}

static void on_stage_start(ce_card_t *c)
{
    tg_state_t *s = ce_state(c, tg_state_t);
    if (s) tg_reset(s);                                /* 上一关末帧的判定 / 锁定不带进新关 */
}

static void on_run_reset(ce_card_t *c)
{
    tg_state_t *s = ce_state(c, tg_state_t);
    if (s) {
        if (s->quick || s->armed_n)
            ce_log("tony_grief: run reset（速收 %u 次 / 改写 %u 段）", s->quick, s->armed_n);
        tg_reset(s);
    }
}

/* +0x50：删卡（倒吊人 / 原版空白卡弃整副牌，都在商店里成交）。本卡唯一的跨帧外部资源是**自毁**的
 * 卡面虚影 VM（见文件头）⇒ 这里把状态清干净即可 —— 真释放由 SDK 的 `ce_state_free` 做。 */
static void on_delete(ce_card_t *c)
{
    tg_state_t *s = ce_state(c, tg_state_t);
    if (s) tg_reset(s);
}

CE_CARD(86, .on_tick_2 = on_tick_2, .on_stage_start = on_stage_start,
            .on_run_reset = on_run_reset, .on_delete = on_delete);
