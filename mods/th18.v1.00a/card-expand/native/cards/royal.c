/* 皇家同花顺 —— 五张黑桃齐了触发：金钱 +800、命 +2、bomb +2。
 *
 * 触发点不另开断点：卡只能从商店买到，成交时 allocate_new_card(mode 2) 会调该卡的 ctor（02-lifecycle §3），
 * 而 owned[id] = 1 写在 ctor 之后（0x412d42）——所以 ctor 里看「其余四张是否已 owned」，齐了就是第五张，正好只触发一次。
 * 初始携带（mode 1）不调 ctor，编成里带满五张不算（用户设定：只有买齐才算）。
 * ★ 但每关开始引擎会对卡组里每张卡再调一次 ctor（实跑：买齐后每关又触发一次，共 6 次）——用 ce_fresh_acquire 挡掉。
 *
 * 演出音乐：ROYAL_RAGTIME 从第 0 帧起（见 ctor 里的注释）。
 * 命 / bomb 照零售 CardLife / CardBomb 的 dtor：先把上限 +1（钳 7），再调引擎的加法（钳上限、放音效、起特效）。
 * 返回 0：黑桃是正常卡，要入卡组。 */
#include "royal.h"

static const uint32_t SPADES[5] = { 58, 59, 60, 61, 62 };
#define ROYAL_GOLD          888
#define ROYAL_TROPHY_FRAME  60      /* 演出时间线：五张牌 0/10/20/30/40 帧逐张弹出，60 帧横幅 + trophy 音效，74 帧「+888 GOLD」，170–194 帧一起淡出 */
static uint32_t s_fx_card;            /* 触发那张卡的 id：只由它的 on_tick_2 走倒计时（五张都挂了 tick，避免一帧减五次）*/
static int      s_fx_countdown;       /* > 0 时每帧 -1，到 0 放 trophy 音效 */

int ce_royal_flush_ctor(ce_card_t *c)
{
    uint32_t self = ce_card_id(c);
    if (!ce_fresh_acquire(c)) return 0;                 /* 每关开始引擎会再调一次 ctor：owned[自己] 已是 1，不是新获得 */
    if (!ce_royal_flush_ready(CE_OWNED_ARRAY(), self, SPADES, 5)) return 0;
    if (!CE_GAME_THREAD()) { ce_log("royal: complete but not in game thread, skip"); return 0; }
    int32_t m = CE_MONEY();
    CE_MONEY() = m + ROYAL_GOLD;
    CE_MONEY_TOTAL() += ROYAL_GOLD;
    for (int i = 0; i < 2; ++i) {
        if (CE_LIVES_MAX() < 7) CE_LIVES_MAX() += 1;     /* CardLife__destructor 0x409b80 的写法 */
        ce_add_life();
    }
    for (int i = 0; i < 2; ++i) {
        if (CE_MAX_BOMBS() < 7) CE_MAX_BOMBS() += 1;     /* CardBomb__destructor 0x409c20 的写法 */
        ce_add_bomb();
    }
    /* 演出：ability.anm script70（assets/ability/scripts/70_royal_show.anm.txt）是父脚本，起五张牌 / 横幅 / 金币文字七个子脚本，层 20；
     * trophy 音效要在横幅弹出那一帧（60）放，C 里做不了延时，交给 ce_royal_tick 倒计时。加命 / 加 bomb 的音效引擎自己放。*/
    uint32_t fx = ce_anm_spawn(CE_ABILITY_ANM(), CE_ANM_ABILITY_SCRIPT_ROYAL_SHOW, 20);
    /* 标志性音乐（assets/voice/make_melodies.py 的 ROYAL_RAGTIME，4.60 s）：拉格泰姆 ——
     * 左手八分 oom-pah、右手 3+3+2 切分、C7→F 副属与 G7→C 终止，「赌场里一把梭哈赢了」。
     * 四分音符 30 帧（120 BPM 的 2/4，一小节 60 帧），与 script70 的时间线对齐：
     * 小节 1 底下走五张牌弹出（帧 0/10/20/30/40），**帧 60 的金色横幅正好是小节 2 的强拍**
     * （重音拉满），帧 180 收在 C 上落进 170–194 的淡出里。
     * 从演出第 0 帧起，所以放在这里而不是 ce_royal_tick 的倒计时里；
     * 帧 60 的 trophy 音效照旧叠在上面（语音就是可叠加的 SE，AUDIT §Q）。 */
    ce_play_voice(ROYAL_RAGTIME, ce_player_x());
    s_fx_card = self; s_fx_countdown = ROYAL_TROPHY_FRAME;
    ce_log("royal: show anm id %08x + ragtime", fx);
    ce_log("royal: ROYAL FLUSH by card %u — money %d -> %d, lives %d (max %d), bombs %d (max %d)",
           self, m, CE_MONEY(), CE_CURRENT_LIVES(), CE_LIVES_MAX(), CE_CURRENT_BOMBS(), CE_MAX_BOMBS());
    return 0;
}

void ce_royal_tick(ce_card_t *c)
{
    if (s_fx_countdown <= 0 || ce_card_id(c) != s_fx_card) return;
    if (--s_fx_countdown == 0) {
        ce_play_sound(CE_SE_TROPHY, ce_player_x());
        ce_log("royal: trophy sound");
    }
}
