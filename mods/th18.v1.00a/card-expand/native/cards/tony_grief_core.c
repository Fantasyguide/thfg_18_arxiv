/* tony_grief_core.c —— 东尼的悲恸（id 86）的纯逻辑。判据与出处全部写在 tony_grief_core.h 的文件头。
 * 复用古锭刀（76）的槽原语：`gd_find_active_slot` / `gd_slot_is_final`（同一条一手规则，别再抄一份）。 */
#include <string.h>

#include "tony_grief_core.h"
#include "guding_core.h"

int32_t tg_active_slot(const uint8_t *interrupts, unsigned n, int *out_final)
{
    int i;
    const uint8_t *slot;
    int32_t hp, tm;
    uint32_t w[4];

    if (out_final) *out_final = 0;
    if (!interrupts) return 0;
    i = gd_find_active_slot(interrupts, n);
    if (i < 0) return 0;
    slot = interrupts + (unsigned)i * GD_SLOT_STRIDE;
    memcpy(&hp, slot + GD_SLOT_HP, 4);
    memcpy(&tm, slot + GD_SLOT_TIME, 4);
    /* ★ 名字取**前 16 字节**（不是 `animal.c`/`lolk.c` 的 4 字节）—— 那两处的 4 字节在这里不够用：
     * TH18 的槽名全是 `Boss2`/`BossCard3`/`MBossDead` 这一族，**头 4 字节恒为 "Boss"**
     * ⇒ 两段若正好 hp / time 相同（道中槽的 `setInterrupt(1, 0, t, …)` 就长这样）签名会撞、段边界漏判。
     * 算式仍是同一族（`(i+1)*1000003 ^ hp*31 ^ tm*131 ^ 名字`），只是名字加了权。 */
    memcpy(w, slot + GD_SLOT_SUB_LIFE, sizeof w);
    if (out_final) *out_final = gd_slot_is_final(slot);
    return (int32_t)(((uint32_t)i + 1u) * 1000003u ^ (uint32_t)hp * 31u ^ (uint32_t)tm * 131u
                     ^ w[0] * 7u ^ w[1] * 17u ^ w[2] * 29u ^ w[3] * 43u);
}

void tg_reset(tg_state_t *s)
{
    memset(s, 0, sizeof *s);
}

tg_out_t tg_frame(tg_state_t *s, const tg_input_t *in)
{
    tg_out_t r;
    int boundary = 0;
    int target_was_live;

    memset(&r, 0, sizeof r);
    s->frames++;
    target_was_live = s->target_live;      /* ② 里可能刚锁定新目标（那时快照还没跟上）*/

    /* ---- ① 符卡窗口：每 active 帧锁存（不依赖下降沿那一帧还能读到旧值）---- */
    if (in->spell_active) {
        if (!s->spell_was) s->bonus_seen = 0;                     /* 上升沿 = 新的一张符卡 */
        if (in->spell_bonus) s->bonus_seen = 1;
        s->elapsed_last = in->spell_elapsed;
    }
    /* 下降沿 + 收下 + 16 s 内 ⇒ 武装「等下一个非符段」的窗口。
     * ⚠ 必须同时看**当前** bit1（`in->spell_bonus`）与「亮过」的锁存：中途被清掉的（玩家死亡 /
     *   110/111 灵击 / 神之宣告）在结尾这一刻 bit1 是 0 ⇒ 不算收下（与引擎 `0x42a7ed` 同口径）。
     * ★★ 2026-10-03 owner 口径：「耐久符卡不算收下，**只要是 00.00 结束的符卡，你就当没收下**」
     *   ⇒ 就是这里的 `!in->spell_timed_out`（bit7 = 已超时）。⚠ 不能只看 bit1：超时但 `elapsed < 60`
     *   时引擎**不清** bit1（`0x42efd2 cmp [edx+0x24],0x3c`）⇒ 必须靠 bit7 排除。 */
    if (!in->spell_active && s->spell_was) {
        if (in->spell_bonus && s->bonus_seen && !in->spell_timed_out &&
            s->elapsed_last <= TG_MAX_FRAMES) {
            s->pending = 1;
            s->pending_left = TG_WAIT_FRAMES;
            s->quick++;
            r.captured = 1;
            r.captured_ms = s->elapsed_last;
        }
        s->bonus_seen = 0;                                        /* 这张符卡翻篇 */
    }
    s->spell_was = in->spell_active;

    /* ---- ② 段边界：活动中断槽的签名变化 ---- */
    if (in->have_boss && in->boss_sig != 0) {
        if (!s->sig_init) { s->sig_init = 1; s->sig = in->boss_sig; }
        else if (in->boss_sig != s->sig) { boundary = 1; s->sig = in->boss_sig; }
    }
    if (boundary && s->pending) {
        if (!in->spell_active && !in->boss_final) {
            /* 收下之后的第一段、且不是符卡、也不是死亡脚本 ⇒ 这就是「下一个非符」*/
            s->target_id = in->boss_id;
            s->target_sig = in->boss_sig;
            s->target_live = 1;
            s->pending = 0;
            s->pending_left = 0;
            s->armed_n++;
            r.armed = 1;
        }
        /* 边界是符卡 / 死亡段 ⇒ 窗口留着，等再下一个边界（很少见；TH18 正常是严格交替）*/
    }

    /* ---- ③ 锁定段的存续：签名一变（= 这一段结束）就收手 ----
     * ⚠ 只对**上一帧就已经锁定**的目标查存续：本帧刚锁定的那个，快照里还没有它。 */
    if (s->target_live) {
        if (target_was_live && (!in->target_present || in->target_sig_now != s->target_sig)) {
            s->target_live = 0;
            r.released = 1;
        } else {
            r.want_write = 1;                                     /* 段内每帧幂等补写 */
        }
    }

    /* ---- ④ 等待窗口到点 ---- */
    if (s->pending) {
        if (s->pending_left) {
            if (--s->pending_left == 0) { s->pending = 0; r.expired = 1; }
        } else {
            s->pending = 0;
        }
    }
    return r;
}
