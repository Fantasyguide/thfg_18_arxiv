/* 神之宣告 —— 主动卡：boss 符卡进行中按 C，消耗一半残机（向上取整），符卡按「超时」立刻结束、不给奖励。
 * 没有残机 / 不在符卡里 / 找不到带超时槽的 boss → 拒绝发动（0x10 无效音，充能退回）。充能 3600 帧（60 s）。
 * 发动同时全屏消弹（弹幕 + 激光，引擎 cancel_all 两个函数，AUDIT O28h）——符卡收场那一帧场上的弹不会白白打死人。
 *
 * 引擎侧（engine/ecl/th18/01-boss-interrupts-and-spellcard.md，AUDIT O28）：
 *   符卡进行中 = SPELLCARD+0x78 bit0；奖励存活 = bit1（收符卡时按它计分、Sannyo 碎片也看它）。
 *   boss 的每段攻击是 zEnemyData.interrupts[i]{hp_value, time, sub_life, sub_timeout}；
 *   步进 0x42ed40 每帧取第一个 hp_value > -1 && time > 0 的槽，slot.time <= time_in_ecl.cur 就走超时子程序、
 *   Spellcard 置「已超时」并清奖励。这里把 time_in_ecl 直接写成 slot.time，下一帧引擎自己按超时收场——
 *   跟真的耗完时间走的是同一条原生路径（C 键在 Player tick 0x17、判定在 EnemyManager tick 0x1b：同一帧收场）。
 *   耐久符卡（bit3）超时本来算收符卡，所以我们额外清 bit1 + 奖励 = 0：一律算失败（限制：sub_timeout 脚本自己的掉落照旧，AUDIT O28b）。
 *   已超时（bit7）但 ECL 还没跑到 523 的窗口拒绝发动；耐久符卡自然超时不置 bit7，那段窗口只靠「活动槽已清」兜底（O28g）。
 *   boss 从 ENEMY_MANAGER.boss_ids[4] 走敌人链表按 enemy_id 找（照 get_boss_enemy_full 0x4237f0）；多 boss 一起超时。 */
#include "sdk.h"

/* 把 boss 当前攻击段的计时推到超时阈值；返回 1 = 推了 */
static int expire_current_attack(uint8_t *enemy)
{
    uint8_t *data = enemy + CE_ENEMY_DATA;
    for (unsigned i = 0; i < CE_ED_INTERRUPT_SLOTS; ++i) {
        uint8_t *slot = data + CE_ED_INTERRUPTS + i * CE_ED_INTERRUPT_STRIDE;
        int32_t hp = *(int32_t *)(slot + 0), t = *(int32_t *)(slot + 4);
        if (hp > -1 && t > 0) {
            ce_ztimer_t *tm = (ce_ztimer_t *)(data + CE_ED_TIME_IN_ECL);
            tm->prev = tm->cur;
            tm->cur = t;
            tm->cur_f = (float)t;
            return 1;
        }
    }
    return 0;
}

static int on_activate(ce_card_t *c)
{
    (void)c;
    uint8_t *spell = CE_SPELLCARD();
    uint8_t *em = CE_ENEMY_MGR();
    if (!spell || !em) return ce_refuse("judgment", "no spellcard/enemy manager");
    uint32_t sf = ce_spell_flags();
    if (!(sf & CE_SPELL_FLAG_ACTIVE)) return ce_refuse("judgment", "no spell card in progress");
    if (sf & CE_SPELL_FLAG_TIMED_OUT) return ce_refuse("judgment", "spell already timed out");   /* 自然超时到 ECL 523 清 bit0 之间的窗口：别误伤下一段攻击 */
    int lives = CE_CURRENT_LIVES();
    int cost = ce_judgment_cost(lives);
    if (!cost) return ce_refuse("judgment", "no lives");

    unsigned expired = 0;
    for (unsigned i = 0; i < CE_EM_BOSS_SLOTS; ++i) {
        int32_t id = *(int32_t *)(em + CE_EM_BOSS_IDS + i * 4);
        if (!id) continue;
        uint8_t *e = ce_enemy_by_id(em, id);
        if (e && expire_current_attack(e)) ++expired;
    }
    if (!expired) return ce_refuse("judgment", "no boss with a timed attack");

    CE_CURRENT_LIVES() = lives - cost;
    ce_gui_update_lives();
    *(uint32_t *)(spell + CE_SPELL_FLAGS) &= ~(uint32_t)CE_SPELL_FLAG_BONUS;   /* 耐久符卡超时本算收符卡：一律按失败 */
    *(int32_t *)(spell + CE_SPELL_BONUS) = 0;
    *(int32_t *)(em + CE_EM_CAN_CAPTURE) = 0;

    ce_cancel_all_bullets();                                                  /* 安全垫：全屏消弹（弹 → 点道具）+ 激光 */
    ce_play_sound(CE_SE_RELEASE, ce_player_x());                              /* 反转牌同款发动音 */
    /* 演出：ability.anm script77（assets/ability/scripts/77_judgment_flash.anm.txt）——卡图副本在场地中央半透明浮现
     * （alpha 140）、铺满弹幕区（scale 1.25 → 1.45，320 高的卡图 × 1.45 ≈ 464 > 区域 448）、75 帧内缓缓放大并上浮 24 px，45 帧后 30 帧淡出。type(1) 二维、层 20，pos y 从区域顶部起算（236 ≈ 正中偏下）。*/
    uint32_t fx = ce_anm_spawn(CE_ABILITY_ANM(), CE_ANM_ABILITY_SCRIPT_JUDGMENT_FLASH, 20);
    ce_log("judgment: lives %d -> %d (cost %d), %u boss attack(s) expired, spell flags %08x, flash anm id %08x",
           lives, lives - cost, cost, expired, *(uint32_t *)(spell + CE_SPELL_FLAGS), fx);
    return 0;
}

CE_CARD(66, .active_recharge = 3600, .on_activate = on_activate);
