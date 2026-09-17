/* collect2x_core.c —— 收点系统（辉针城）纯逻辑。见 collect2x_core.h。 */
#include "collect2x_core.h"

/* 批集 = 引擎道具 type {1(P), 2(钱)}（§Z19：TH14 官方文本「红点与蓝点」→ TH18 无蓝点生态位 =
 * 玩家收点道具 = P 与钱；type 9–0xe 是引擎分数转换点族 —— 0x44625f 分派映射表 0x44685c 里六 type
 * 共指加分 handler 0x446547、只由强制收集（击破转换）产生，DDC 原生转换蓝点不计入收点批 → 不入）。
 * 卡道具 0x10–0x13 等非批集一律不入。*/
static int type_in_batch(uint32_t type)
{
    return type == 1 || type == 2;
}

int c2x_collect_batch(c2x_state_t *s, uint32_t type, int32_t frame)
{
    if (!type_in_batch(type)) return 0;
    s->batch_open = 1;
    s->batch_count++;
    s->last_event_at = frame;
    return 1;
}

int c2x_batch_presence(c2x_state_t *s, int32_t frame)
{
    if (!s->batch_open) return 0;   /* 批未开：无事可续（吸附中的第一枚落地时自会开批）*/
    s->last_event_at = frame;
    return 1;
}

int c2x_collect_virtual(c2x_state_t *s, int32_t n, int32_t frame)
{
    /* 见 collect2x_core.h：与 c2x_collect_batch 只差 type 门（自建实体没有引擎 type），
     * 且**不碰 batch_score**（用户 2026-09-18 口径：「绿点的面值不进 batch_score，它加的不是分数」）。
     * 第一颗自建实体就能**开批**（§Z22 的成员门只针对**原生**道具的真空谓词；自建实体的
     * 「算不算收点」由生产者自己的收集判定给出）。*/
    if (!s || n <= 0) return 0;
    s->batch_open = 1;
    s->batch_count += n;
    s->last_event_at = frame;
    return 1;
}

int c2x_maybe_settle(c2x_state_t *s, int32_t frame, c2x_settle_out_t *out)
{
    out->mult_permille = 0;
    out->fragment_type = 0;
    out->fragment_count = 0;
    out->batch_score = 0;
    if (!s->batch_open) return 0;
    /* 批闭门 = TH14 timer 语义（§Z21 = presence + 原版 8）：last_event_at 由收集（absorb）和
     * 吸附中 presence 双源续期 —— 有批集道具在飞（state 3/4）就不让 idle 走完，批闭只发生在
     * 「吸附空且无新收」的真停段 ~8 帧后；落地休止/抛物中的场道具不续（TH14 原义，0x43898e
     * 只盖在飞）。§Z20 的 8–25 帧波内空洞 = 在飞散布 → presence 盖掉，不再需要 30 帧大窗。 */
    if (frame - s->last_event_at < C2X_BATCH_SETTLE_IDLE) return 0;

    /* —— 批闭：清计数，档位按关批时的 count —— */
    const int32_t count = s->batch_count;
    s->batch_count = 0;
    s->batch_open = 0;
    out->batch_score = s->batch_score;
    s->batch_score = 0;

    int32_t mult;
    if      (count >= C2X_TABLE_T5) mult = C2X_TABLE_M5;
    else if (count >= C2X_TABLE_T4) mult = C2X_TABLE_M4;
    else if (count >= C2X_TABLE_T3) mult = C2X_TABLE_M3;
    else if (count >= C2X_TABLE_T2) mult = C2X_TABLE_M2;
    else if (count >= C2X_TABLE_T1) mult = C2X_TABLE_M1;
    else return 1;                              /* <20：关批、无奖（out 保持全零）*/
    out->mult_permille = mult;

    /* 碎片：<60 走 4:1 循环（第 1–4 次符卡碎、第 5 次命碎归零）；≥60 命碎、不入循环游标。*/
    if (count >= C2X_TABLE_T5) {
        out->fragment_type = C2X_FRAG_LIFE;
    } else if (s->fragment_cycle >= C2X_FRAGMENT_CYCLE) {
        s->fragment_cycle = 0;
        out->fragment_type = C2X_FRAG_LIFE;
    } else {
        s->fragment_cycle++;
        out->fragment_type = C2X_FRAG_BOMB;
    }
    out->fragment_count = 1;
    return 1;
}

int32_t c2x_drop_interval(int32_t power_x100)
{
    /* 96 − power×16/100：power 0.00 → 96，4.00 → 32；截断向零，≤0 钳 1。*/
    int32_t n = C2X_DROP_INTERVAL_BASE - (power_x100 * C2X_DROP_INTERVAL_PER) / 100;
    return n < C2X_DROP_INTERVAL_MIN ? C2X_DROP_INTERVAL_MIN : n;
}

int c2x_maybe_drop(c2x_state_t *s, int32_t frame, int32_t power_x100)
{
    if (frame < s->next_drop_at) return 0;
    s->next_drop_at = frame + c2x_drop_interval(power_x100);
    return 1;
}
