/* faith.c —— 绿点 / 信仰系统（110 风神录 + 111 地灵殿）**共用**的引擎侧实现：VM 池 + 收集判定。
 *
 * ## 为什么共用一个池（用户要的说明）
 *
 * 用户 2026-09-12 定案：「111 的绿点和 110 的绿点**完完全全就是同一个东西**，它们加的都是分，
 * 只是加的数量不一样」「**一套系统 + 分槽记账**」。所以：
 *   · **实体层共用一个池**（本文件）—— 撒点、飞散、飘落、吸附、收集判定写一遍；
 *   · **记账层按槽分**（`faith_core.c` 的 `faith_slot_t`）—— 110 只写 `Δ_110`、111 只写 `Δ_111`，
 *     两者**互不可见**（「不跨卡耦合」，review §3.1 第 1 条）。
 *   · 池里的每颗点带 `slot_mask`，收集时**只回调认领该 mask 的卡** ⇒ 一张卡装的时候另一槽根本没人写。
 *
 * 代价（已知、用户已知悉）：自建实体**不入原生道具池**（review §7.3）⇒ 原生磁吸 / 回收线 /
 * 114 的批集计数**看不到它**。所以吸引与收集判定在这里自己做，判据照抄原生（读自机字段）。
 *
 * ## 运动学：照原生道具逐条抄（不自己发明）
 *
 * 一手：TH18 `ItemManager__on_tick__body` `0x445a80`（与 TH16 `th16:0x42f4e0` 同款代码同一组常量）——
 * 完整推导与地址见 `native/cards/season.c` 文件头（33/34 轮实跑修出来的那份）。本文件沿用同一套：
 *   · 出生：`spawn` 给角度与初速（引擎 `0x446f40` 的实参角度 −π/2、速度 2.2）；池是我们的，
 *     角度与速度由调用方给（随机散得更自然），**常量与区间逐条照抄**；
 *   · 飞散：`pos += vel`、`r -= 0.03`、`vel = polar(角, r)` ⇒ 沿固定方向减速直线飞行；
 *   · 飘落：`vy += 0.03`（`0x4b90a4`）、`vy > 2.0 → 2.0`、`vy >= 0` 时水平速度清零；
 *   · 吸引：进吸引半径 → 朝自机飞，初速 = 磁速/3，每帧 +0.2 到上限 12.0；过回收线（`player+0x47998`）
 *     → 朝自机飞，初速 = 磁速本身；
 *   · 收集半径 / 吸引半径 / 磁速**读自机自己的字段**（`player+0x4798c` / `+0x47990|94` / `+0x47988`）；
 *   · 出界删：`y > 472`、`|x| >= 200`。
 *   ⇒ 与 season.c 是**同一份代码的第二次出现**（SDK.md §5 的「出现第二次就提炼」还没做，
 *      因为季节点是「按副季节换贴图 + 释放期全收」，绿点是「三档 + 按槽记账」；
 *      两者的**差异**都在调用侧，共同部分已尽量收进本文件的常量里）。⚠ 待办：抽成一个
 *      `item_fly.c` 让 116 / 110 / 111 共用（登记在报告里）。
 *
 * ## ❓ 自定（原版确实没有 / 我们没找到）
 *
 *   · **贴图**：TH18 没有绿点 item type、我们也没有绿点图 ⇒ 借季节点那张 16×16 零售切图
 *     （`CE_ANM_ABILITY_SPRITE_PT_*`）+ `ce_anm_set_color` 染色。见 `faith_sprite_for()`。
 *   · **微档不建实体**：§2.2 的微信仰点有 4 个来源（道中/boss 非符消弹、击破消弹、全避消弹、
 *     登场消弹），其中只有「我们自己放灵击」这条我们现在接得到；消弹每颗 +10 走
 *     `faith_cancel_account()` 直接进账，不撒实体（原作那边是可见的星形碎片）。
 *     登记为待实跑：拿到断点后再补实体版（`faith_spawn(..., FAITH_TIER_MICRO, ...)`）。
 */
#include <stddef.h>          /* sdk.h 的 ce_item_collect_all 用了 size_t（内置 Linux 头不带）*/
#include <string.h>
#include "sdk.h"
#include "faith.h"

#define FAITH_PI              3.14159265358979f
/* —— 运动学常量（出处见文件头；与 season.c 同值，别单独调）—— */
#define FAITH_GRAVITY         0.03f    /* 每帧重力增量（`0x4b90a4`）*/
#define FAITH_VFALL_MAX       2.0f    /* 下落速度上限（`0x494558`）*/
#define FAITH_MAG_MAX         12.0f   /* 吸引速度上限（`0x4945d4`）*/
#define FAITH_MAG_STEP        0.2f    /* 吸引速度每帧增量 */
#define FAITH_OOB_Y           472.0f
#define FAITH_OOB_X           200.0f
#define FAITH_THROW_MIN       0.2f    /* 出生初速区间（`0x446f40` 的实参口径：0.2..2.1）*/
#define FAITH_THROW_SPAN      1.9f
#define FAITH_COLLECT_R_DEF   30.0f
#define FAITH_ATTRACT_R_DEF   70.0f
#define FAITH_MAG_SPEED_DEF   5.0f

/* ❓ 自定染色（原版绿点贴图未找到；TH18 的 `bullet/item.png` 蓝点那一格现被金钱用）。
 * 三个档给三个色，也方便实跑时一眼看出是哪一档在掉。拿到零售绿点贴图后整段删掉。*/
#define FAITH_TINT_BIG    0xff40e060u   /* ❓ 自定：大 = 亮绿 */
#define FAITH_TINT_SMALL  0xff80ffa0u   /* ❓ 自定：小 = 浅绿 */
#define FAITH_TINT_MICRO  0xffffffffu   /* ❓ 自定：微（不建实体，占位）*/

/* 大点的运动学稍微「重」一点（下落更快、吸引起点更远）—— ❓ 自定：原版大/小信仰点的
 * 运动学差异**未查**（th10 那边是同一套 item tick，差异只在半径/分值）。留一个系数方便实跑调。*/
#define FAITH_BIG_MAG_BONUS   1.0f

typedef struct {
    int32_t id;            /* anm VM id；0 = 空槽 */
    float   x, y;
    float   vx, vy;
    float   lang;          /* 飞散角（整个飞散期不变）*/
    float   lr;            /* 飞散剩余速度（每帧 −0.03）*/
    uint8_t state;         /* 0 = 待发射，1 = 飘落，2 = 飞散，3 = 过回收线飞向自机，4 = 吸引半径内飞向自机 */
    uint8_t mag20;         /* 吸引速度 ×20 */
    uint8_t age;
    uint8_t delay;
    uint8_t tier;          /* faith_tier_t */
    int32_t slot_mask;     /* 落进谁的槽 */
    int32_t value;         /* 原始单位分值 */
} faith_dot_t;

/* ★ 池放 DLL 静态区（ce_state 只有 496 字节，装不下几百颗点；同 season.c 的口径）。*/
static int32_t     s_spawn_seq;

static faith_user_t s_user[FAITH_MAX_USERS];
static int          s_n_user;

/* 诊断（静态区）：每 600 帧一条汇总，实跑对账用 */
static int32_t s_stat_pickups, s_stat_points, s_stat_frame, s_stat_dropped;

/* ============================ 注册 ============================ */
void faith_module_reset(void)
{
    faith_clear_entities();
    memset(s_user, 0, sizeof s_user);
    s_n_user = 0;
    s_spawn_seq = 0;
    s_stat_pickups = s_stat_points = s_stat_frame = s_stat_dropped = 0;
}

int faith_register(const faith_user_t *u)
{
    int i;

    if (!u || !u->slot_mask || u->card_id > (uint32_t)CE_SDK_CARD_ID_MAX) return 0;
    for (i = 0; i < s_n_user; ++i) {                    /* 同 id = 更新（幂等：ctor / 每关重进都会调）*/
        if (s_user[i].card_id == u->card_id) { s_user[i] = *u; return 1; }
    }
    if (s_n_user >= FAITH_MAX_USERS) return 0;
    s_user[s_n_user++] = *u;
    return 1;
}

void faith_unregister(uint32_t card_id)
{
    int i;

    for (i = 0; i < s_n_user; ++i) {
        if (s_user[i].card_id != card_id) continue;
        s_user[i] = s_user[--s_n_user];
        return;
    }
}

/* ============================ 池 ============================ */
/* 池**静态分区**（两个区各自紧凑，区内 swap-remove）：
 *   大档 = `s_big[0 .. s_n_big)`，容量 `FAITH_BIG_POOL`；
 *   小档 = `s_small[0 .. s_n_small)`，容量 `FAITH_SMALL_POOL`。
 * 为什么分区：大信仰点一次清场几十颗，不能把小点的位置吃掉（两张卡同装时更是如此）；
 * 而且两个区各自有独立上限 ⇒ 「大点把池挤满导致小点全丢」这种耦合不存在。
 * 统一视图（tick 里顺序推进两个区）用 `dot_at()/dot_n()` 取。*/
static faith_dot_t s_big[FAITH_BIG_POOL];
static faith_dot_t s_small[FAITH_SMALL_POOL];
static int         s_n_big;
static int         s_n_small;

static int dot_n(faith_tier_t tier) { return (tier == FAITH_TIER_BIG) ? s_n_big : s_n_small; }
static faith_dot_t *dot_at(faith_tier_t tier, int i)
{
    return (tier == FAITH_TIER_BIG) ? &s_big[i] : &s_small[i];
}

static void dot_remove(faith_tier_t tier, int i)
{
    if (tier == FAITH_TIER_BIG) s_big[i] = s_big[--s_n_big];
    else                        s_small[i] = s_small[--s_n_small];
}

void faith_clear_entities(void)
{
    faith_clear_entities_mask(~0);          /* 所有槽（模块复位；卡侧删卡请用带 mask 的那一支）*/
}

/* 只收「认领了 `slot_mask` 里任意一位」的实体（`dot.slot_mask & slot_mask != 0`）。
 * ★ 为什么必须有这一支（2026-09-12 审计）：本池是 110（风神录）与 111（地灵殿）**共用**的，
 *   而 `on_delete` 只代表**一张卡**没了 —— 用 `faith_clear_entities()` 会把另一张卡同屏还飘着的
 *   绿点一起删掉（`signal.c` / `mof.c` 原来都这么调，两边注释里都登记了这个偏差）。
 *   每颗点在 `faith_spawn()` 里就带上了 `slot_mask`（进谁的槽），按它筛选即可只收自己家的。
 * 语义与 `dot_remove()` 的 swap-remove 一致：命中就删 VM 并把尾元素换到 `i`（**不 ++i**，尾元素也要过筛）。*/
void faith_clear_entities_mask(int slot_mask)
{
    int i;

    if (!slot_mask) return;
    for (i = 0; i < s_n_big; ) {
        if (!(s_big[i].slot_mask & slot_mask)) { ++i; continue; }
        if (s_big[i].id) ce_anm_delete((uint32_t)s_big[i].id);
        s_big[i] = s_big[--s_n_big];
    }
    for (i = 0; i < s_n_small; ) {
        if (!(s_small[i].slot_mask & slot_mask)) { ++i; continue; }
        if (s_small[i].id) ce_anm_delete((uint32_t)s_small[i].id);
        s_small[i] = s_small[--s_n_small];
    }
}

/* 贴图：❓ 自定占位 —— 借季节点那张 16×16 零售切图（TH16 `bullet/item.png` 右上 2×2），
 * 按档选一格；真正的绿点贴图到位后只改这里。*/
static int faith_sprite_for(faith_tier_t tier)
{
    switch (tier) {
    case FAITH_TIER_BIG:   return CE_ANM_ABILITY_SPRITE_PT_FALL;    /* ❓ 自定：占位单元 */
    case FAITH_TIER_SMALL: return CE_ANM_ABILITY_SPRITE_PT_SPRING;  /* ❓ 自定：占位单元 */
    default:               return CE_ANM_ABILITY_SPRITE_PT_SUMMER;  /* ❓ 自定：占位单元 */
    }
}

static uint32_t faith_tint_for(faith_tier_t tier)
{
    switch (tier) {
    case FAITH_TIER_BIG:   return FAITH_TINT_BIG;
    case FAITH_TIER_SMALL: return FAITH_TINT_SMALL;
    default:               return FAITH_TINT_MICRO;
    }
}

static int dot_spawn_vm(faith_dot_t *p)
{
    uint32_t id = ce_anm_spawn(CE_ABILITY_ANM(), CE_ANM_ABILITY_SCRIPT_SEASON_POINT, FAITH_LAYER);

    if (!id) return 0;
    ce_anm_set_pos(id, p->x, p->y, 0.0f);
    ce_anm_set_sprite(CE_ABILITY_ANM(), id, faith_sprite_for((faith_tier_t)p->tier));
    ce_anm_set_color(id, faith_tint_for((faith_tier_t)p->tier));
    p->id = (int32_t)id;
    return 1;
}

int faith_spawn(float x, float y, faith_tier_t tier, int slot_mask, int32_t value)
{
    faith_dot_t *p;
    float ang, spd, vel[3];

    if (!slot_mask || value <= 0) return 0;
    if (tier == FAITH_TIER_MICRO) return 0;      /* 微档不建实体（见文件头 ❓ 自定）*/
    if (tier != FAITH_TIER_BIG && tier != FAITH_TIER_SMALL) return 0;
    if (dot_n(tier) >= (tier == FAITH_TIER_BIG ? FAITH_BIG_POOL : FAITH_SMALL_POOL)) {
        ++s_stat_dropped;                        /* 池满：丢弃（不做待撒队列 —— 量级不需要）*/
        return 0;
    }

    /* 出生角度/速度：照引擎自己撒主掉落的区间（`0x446f40` 实参角度 −π/2、速度 2.2）。
     * 这里给「随机角 + 0.2..2.1」= 与 season.c 的击破通道同款（一手见其头部注释）。*/
    ang = (float)((int32_t)(ce_rand() % 2001) - 1000) * (FAITH_PI / 1000.0f);
    spd = FAITH_THROW_MIN + (float)(ce_rand() % 1001) * (FAITH_THROW_SPAN / 1000.0f);

    p = dot_at(tier, dot_n(tier));
    memset(p, 0, sizeof *p);
    p->id = 0;                                   /* VM 等延时结束再建（照 TH16：延时里 VM 还没出生）*/
    p->x = x;
    p->y = y;
    p->lang = ang;
    p->lr = spd;
    ce_polar_to_vel(vel, ang, spd);              /* 引擎自己的 FSINCOS，不引 libm */
    p->vx = vel[0];
    p->vy = vel[1];
    p->state = 0;
    p->mag20 = 0;
    p->age = 0;
    p->delay = (uint8_t)((s_spawn_seq & 3) + 1); /* 错帧，让同帧出生的一撮不叠在一起 */
    p->tier = (uint8_t)tier;
    p->slot_mask = slot_mask;
    p->value = value;
    ++s_spawn_seq;
    if (tier == FAITH_TIER_BIG) ++s_n_big; else ++s_n_small;
    return 1;
}

int faith_spawn_big(float x, float y, int32_t value_110)
{
    return faith_spawn(x, y, FAITH_TIER_BIG, 1 << FAITH_SLOT_110, value_110);
}

/* ============================ 收集 ============================ */
static void dot_collect(faith_dot_t *p)
{
    int i;

    ce_anm_interrupt((uint32_t)p->id, 1);        /* 脚本放大淡出后自删（与季节点同一张脚本）*/
    for (i = 0; i < s_n_user; ++i) {
        if (!(s_user[i].slot_mask & p->slot_mask)) continue;      /* 只回调认领该槽的卡 */
        if (s_user[i].on_pickup) s_user[i].on_pickup(s_user[i].user, p->slot_mask,
                                                     (int)p->tier, p->value);
    }
    ++s_stat_pickups;
    s_stat_points += p->value;
}

void faith_cancel_account(int32_t bullets_cancelled)
{
    int i;

    if (bullets_cancelled <= 0) return;
    for (i = 0; i < s_n_user; ++i) {
        if (s_user[i].on_cancel) s_user[i].on_cancel(s_user[i].user, bullets_cancelled);
    }
}

/* ============================ 每帧 ============================ */
/* faith_player_poc_height() 在 faith.h 里（static inline，见那里的注释）。*/

int faith_player_above_line(void)
{
    const float *pp = ce_player_pos();
    uint8_t *pl = CE_PLAYER();
    int pstate;

    if (!pp || !pl) return 0;
    /* 原版的门：自机 state ∈ {2,4}（死亡 / 复活）时不看回收线（`0x445ba3`/`0x445ba8`）。*/
    pstate = *(int32_t *)(pl + CE_PLAYER_STATE);
    if (pstate == 2 || pstate == 4) return 0;
    return faith_player_poc_height() > pp[1];
}

int faith_enemies_present(void)
{
    uint8_t *em = CE_ENEMY_MGR();
    return (em && *(int32_t *)(em + CE_EM_ENEMY_COUNT) > 0) ? 1 : 0;
}

int faith_pool_tick(void)
{
    uint8_t *pl = CE_PLAYER();
    const float *pp = ce_player_pos();
    float mag_speed = FAITH_MAG_SPEED_DEF;
    float collect_r2 = FAITH_COLLECT_R_DEF * FAITH_COLLECT_R_DEF;
    float ring_r2 = FAITH_ATTRACT_R_DEF * FAITH_ATTRACT_R_DEF;
    float slow = 1.0f;
    int collected = 0, t, i;

    {   /* 与季节点同一口径：引擎的道具减速因子（绀珠传卡会写它）⇒ 同一屏上绿点跟着一起慢 */
        uint8_t *im = CE_ITEM_MANAGER();
        if (im) {
            float f = *(float *)(im + CE_ITEM_SLOWDOWN);
            if (f > 0.0f && f < 1.0f) slow = f;
        }
    }
    if (pl) {
        float r;
        mag_speed = *(float *)(pl + CE_PLAYER_ITEM_ATTRACT_SPD);
        if (mag_speed <= 0.0f) mag_speed = FAITH_MAG_SPEED_DEF;
        r = *(float *)(pl + CE_PLAYER_ITEM_COLLECT_R);
        if (r > 0.0f) collect_r2 = r * r;
        r = *(float *)(pl + (CE_PLAYER_FOCUSED() ? CE_PLAYER_ITEM_ATTRACT_RF
                                                 : CE_PLAYER_ITEM_ATTRACT_RU));
        if (r > 0.0f) ring_r2 = r * r;
    }

    /* 每 600 帧一条汇总（诊断；实跑时对「哪些敌人掉了、丢了多少」用）*/
    if (s_stat_frame % 600 == 599 && (s_stat_pickups || s_stat_dropped)) {
        ce_log("faith: pickups %d, points %d, dropped %d (big %d / small %d alive)",
               (int)s_stat_pickups, (int)s_stat_points, (int)s_stat_dropped, s_n_big, s_n_small);
        s_stat_pickups = s_stat_points = s_stat_dropped = 0;
    }
    ++s_stat_frame;

    for (t = 0; t < 2; ++t) {
        faith_tier_t tier = t ? FAITH_TIER_SMALL : FAITH_TIER_BIG;

        for (i = 0; i < dot_n(tier); ) {
            faith_dot_t *p = dot_at(tier, i);
            float x, y, vx, vy;

            if (p->state == 0) {                             /* 待发射：只数延时，VM 都还没建 */
                if (p->delay > 0) { --p->delay; ++i; continue; }
                if (!dot_spawn_vm(p)) { dot_remove(tier, i); continue; }
                p->state = 2;                                /* → 飞散 */
                ++i; continue;                               /* 建完 VM 那一帧不动（照原版）*/
            }
            x = p->x; y = p->y; vx = p->vx; vy = p->vy;

            if (!p->id || !ce_anm_get_vm((uint32_t)p->id)) {  /* 脚本已 delete（超时）→ 回收槽 */
                dot_remove(tier, i);
                continue;
            }
            ++p->age;

            /* 回收线 / 放 B / 对话 ⇒ 真空飞行（初速 = 磁速）*/
            if (p->state == 1 && pl && p->age >= FAITH_INTANGIBLE) {
                if (faith_player_above_line() || CE_BOMB_COLLECTING() || CE_GUI_MSG_ACTIVE()) {
                    p->state = 3;
                    p->mag20 = (uint8_t)(mag_speed * 20.0f);
                    if (p->mag20 > 240) p->mag20 = 240;
                }
            }

            if (p->state == 2) {                             /* 飞散：直线 + 速度每帧 −0.03 */
                x += vx;
                y += vy;
                p->lr -= 0.03f;
                if (p->lr <= 0.0f) {
                    p->lr = 0.0f;
                    vx = 0.0f;
                    vy = 0.0f;
                    p->state = 1;
                } else {
                    float vel[3];
                    ce_polar_to_vel(vel, p->lang, p->lr);
                    vx = vel[0];
                    vy = vel[1];
                }
            } else if (p->state == 1) {                      /* 飘落 */
                float sl = (p->age < FAITH_INTANGIBLE) ? 1.0f : slow;
                vy += FAITH_GRAVITY * sl;
                if (vy > FAITH_VFALL_MAX) vy = FAITH_VFALL_MAX;
                if (vy >= 0.0f) vx = 0.0f;                   /* 原版：下落时水平速度清零 */
                x += vx * sl;
                y += vy * sl;
            } else if (pp) {                                 /* 3/4：朝自机飞 */
                float mag = (float)p->mag20 / 20.0f;
                float dx = pp[0] - x, dy = pp[1] - y;
                float len = __builtin_sqrtf(dx * dx + dy * dy);

                if (tier == FAITH_TIER_BIG) mag *= FAITH_BIG_MAG_BONUS;
                if (len > 0.001f) { vx = dx / len * mag; vy = dy / len * mag; }
                else { vx = 0.0f; vy = mag; }
                x += vx;
                y += vy;
                if (mag < FAITH_MAG_MAX) {
                    int m = (int)p->mag20 + (int)(FAITH_MAG_STEP * 20.0f);
                    p->mag20 = (uint8_t)(m > 240 ? 240 : m);
                }
            }

            p->x = x;
            p->y = y;
            p->vx = vx;
            p->vy = vy;

            if (y > FAITH_OOB_Y || x >= FAITH_OOB_X || x <= -FAITH_OOB_X) {
                if (p->id) ce_anm_delete((uint32_t)p->id);
                dot_remove(tier, i);
                continue;
            }

            if (pp && p->age >= FAITH_INTANGIBLE) {
                float dx = pp[0] - x, dy = pp[1] - y;
                float d2 = dx * dx + dy * dy;

                if (d2 <= collect_r2) {                      /* 收 */
                    dot_collect(p);
                    dot_remove(tier, i);
                    ++collected;
                    continue;
                }
                if (p->state == 1 && d2 <= ring_r2) {        /* 只有飘落态才进吸引半径 */
                    p->state = 4;
                    p->mag20 = (uint8_t)(mag_speed * 20.0f / 3.0f);
                    if (p->mag20 == 0) p->mag20 = 1;
                }
            }

            /* 减速时的抖动（照引擎道具 tick：factor < 1.0 且 age > 31 → 位置写 randf(−1,1)）*/
            if (slow < 1.0f && p->age >= FAITH_INTANGIBLE) {
                float jx = (float)((int32_t)(ce_rand() % 3) - 1);
                float jy = (float)((int32_t)(ce_rand() % 3) - 1);
                ce_anm_set_pos((uint32_t)p->id, x + jx, y + jy, 0.0f);
            } else {
                ce_anm_set_pos((uint32_t)p->id, x, y, 0.0f);
            }
            ++i;
        }
    }
    return collected;
}
