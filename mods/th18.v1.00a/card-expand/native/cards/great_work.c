/* 伟大之作（id 92）—— **主动卡**，致敬《Noita》的「伟大之作」（The Great Work）。
 *
 * ## owner 2026-10-03 逐字
 * 「noita卡：伟大之作：把所有敌人+弹幕变成金钱道具（类P点的那个）。冷却时间按照转换的弹幕数量和
 *   消灭敌人的血量等比例计算。敌人也按照剩余血量可以转换出多个金钱。如果是 boss 期间就会直接击破一个阶段。」
 *
 * ## 一次发动做四件事（逐条落点）
 * ① **弹幕 → 金钱**：扫引擎的子弹池（`CE_BM_BULLETS` 起 `CE_BM_BULLET_COUNT = 2000` 槽，
 *    `state ∈ {1,2}` = 活动），先记下每颗的位置，再逐颗 `Bullet__cancel(b, 0)` —— **mode 0 = 纯删除**
 *    （一手：0/3 不撒任何实体、1 会撒 type 9、4 会撒 type 9 + type 2），然后由我们自己在那些位置撒
 *    **item type 2 = 金钱道具**（一手 `engine/faith/th18/01-point-item-value.md` §3：收下 = `MONEY += 1`
 *    + `SCORE += 身价/10`；`0x446b00` 那条路就是它）。走 mode 0 还顺带避开两件事：不推那条 RNG、
 *    不搅「本帧消弹颗数」的共用闩锁（见 `engine.h` 的 `CE_FN_BULLET_CANCEL` 注）。
 * ② **敌人 → 金钱**：每个可锁定的敌人按**剩余血量**（`CE_ENEMY_HP_CUR`）换 `gw_money_for_hp()` 枚金钱，
 *    再用一发「等于它剩余血量」的伤害源把它打死（走引擎自己的死亡路径 ⇒ 分数 / 死亡演出 / 掉落照常）。
 * ③ **boss 期间 = 直接击破一个阶段**：带 `CE_ENEMY_FLAG_BOSS`（flags bit23，一手 = ECL `setBoss`）的敌人
 *    不撒金钱、改成吃一发「等于它当前阶段剩余血量」的伤害 ⇒ 正好把当前阶段打掉、进下一段。
 *    ⚠ 伤害要 `ce_raise_damage_cap()` 抬当帧上限，否则被引擎的「每敌每帧上限」削掉。
 * ④ **冷却按转换量算**：`gw_recharge(弹幕数, Σ血量)`（纯逻辑 + 主机单测）⇒ 直接写这张卡的充能三元组
 *    （`card+0x34/+0x38/+0x3c` = zTimer 的 prev/cur/cur_f，`card+0x48` = 总时长；照引擎写充能那一支）。
 *
 * ## ❓ 自定（登记 `TUNING.md` §2.10）
 * 两条公式的系数（每 20 血 1 枚、单敌上限 40 枚、一次最多 256 颗弹、冷却底价/单价/上下限）、
 * 金钱撒落的散开角度、以及「敌人的正常掉落**不压制**」（要压制得走掉落倍率层，见 TUNING 那条）。 */
#include "sdk.h"
#include "great_work_core.h"

#define GW_CARD_SE          0x4d       /* 发动音：0x4d se_release（与杰拉 / 鞘翅重锤同）*/
#define GW_ENEMY_HIT_W      44.0f      /* 打敌人那一发伤害源的判定盒 ❓ 自定 */
#define GW_ENEMY_HIT_H      44.0f
#define GW_LETHAL_SLACK     8         /* 伤害 = 剩余血量 + 这个余量（保证真的过线）❓ 自定 */

typedef struct {
    uint32_t fired;         /* 发动过几次（诊断）*/
    int32_t  last_frames;   /* 上一次算出来的冷却帧数（诊断）*/
    int32_t  last_bullets;
    int64_t  last_hp;
    int32_t  last_money;
    int32_t  last_phase_breaks;
} gw_card_t;

_Static_assert(sizeof(gw_card_t) <= 496, "great_work: private state overflows CE_STATE_BYTES");

typedef void (__attribute__((thiscall)) *gw_fn_bullet_cancel_t)(void *bullet, int mode);

/* 静态缓冲：不占引擎的栈（`on_activate` 跑在引擎的调用栈里）。本卡只在主线程、且 on_activate
 * 不会重入 ⇒ 文件级 static 是安全的（与 108/113 的池同一个口径）。*/
typedef struct { float x, y; int32_t hp; uint32_t boss; } gw_target_t;
static float       s_bullet_pos[GW_BULLET_MAX][3];
static uint32_t    s_bullet_rnd[GW_BULLET_MAX];
static gw_target_t s_target[GW_ENEMY_MAX];

/* 把这张卡的充能**当帧改成** `frames`（照引擎写充能那一支：zTimer 的 prev/cur/cur_f + 总时长）。*/
static void gw_set_recharge(ce_card_t *c, int32_t frames)
{
    uint8_t *p = (uint8_t *)c;
    if (!p || frames < 0) return;
    *(int32_t *)(p + CE_CARD_RECHARGE_TIMER)     = frames - 1;      /* +0x34 prev */
    *(int32_t *)(p + CE_CARD_RECHARGE_TIMER + 4) = frames;          /* +0x38 cur  */
    *(float   *)(p + CE_CARD_RECHARGE_TIMER + 8) = (float)frames;   /* +0x3c cur_f*/
    *(int32_t *)(p + CE_CARD_RECHARGE_TIME)      = frames;          /* +0x48 总时长 */
}

/* ① 弹幕 → 金钱。返回转换掉的弹幕数。*/
static int gw_take_bullets(int32_t *out_money)
{
    uint8_t *bm = CE_BULLET_MGR();
    float    (*pos)[3] = s_bullet_pos;
    uint32_t *rnd = s_bullet_rnd;
    int      n = 0, spawned = 0, i;
    uint32_t seed = ce_rand();

    *out_money = 0;
    if (!bm) return 0;

    for (i = 0; i < CE_BM_BULLET_COUNT; ++i) {
        uint8_t *b = bm + CE_BM_BULLETS + (size_t)i * CE_BULLET_STRIDE;
        int32_t st = (int32_t)*(uint16_t *)(b + CE_BULLET_STATE);
        if (st != 1 && st != 2) continue;                 /* 0 空槽 / 3·4 已在收尾 */
        seed = seed * 1103515245u + 12345u;               /* our LCG（不用引擎那两支走 x87 的）*/
        if (n < GW_BULLET_MAX) {
            pos[n][0] = *(float *)(b + CE_BULLET_POS);
            pos[n][1] = *(float *)(b + CE_BULLET_POS + 4);
            pos[n][2] = *(float *)(b + CE_BULLET_POS + 8);
            rnd[n] = seed;
            ++n;
        }
        ((gw_fn_bullet_cancel_t)CE_FN_BULLET_CANCEL)(b, 0);   /* ★ mode 0 = 纯删除（不撒原生道具）*/
    }

    for (i = 0; i < n; ++i) {                             /* 先全删完再撒，避免撒出来的被自己的扫帚扫掉 */
        float ang = -1.5707964f;
        if (GW_MONEY_ANGLE_SPREAD) {
            /* 用我们自己的 LCG 抽一个 [−π, π) 的方向（replay-safe：不碰引擎 RNG）*/
            uint32_t r = rnd[i];
            ang = ((float)(int32_t)(r & 0xffffu) / 32768.0f - 1.0f) * 3.14159265f;
        }
        if (ce_spawn_item(2u, pos[i], ang, 1.6f)) ++spawned;   /* type 2 = 金钱道具 */
    }
    *out_money = spawned;
    return n;
}

/* ②③ 敌人 → 金钱 / boss → 破一阶段。
 * ⚠ 伤害上限是**本帧总伤害**（`player+0x47984`，`enm_compute_damage_sources 0x45f0f0` 用它钳本帧总和）
 *   ⇒ 必须先统计完再一次性抬，否则逐只抬会互相盖掉（第二只看到的还是第一只的上限）。*/
static int gw_take_enemies(int64_t *hp_sum, int32_t *money, int32_t *breaks)
{
    uint8_t *em = CE_ENEMY_MGR();
    uint8_t *e;
    int n = 0, i;
    int32_t total_dmg = 0;

    *hp_sum = 0;
    *money = 0;
    *breaks = 0;
    if (!em) return 0;

    CE_ENEMY_FOREACH(em, e) {                             /* 第一遍：记账 + 抬上限 */
        int32_t hp;
        uint32_t flags;
        if (n >= GW_ENEMY_MAX) break;
        if (!ce_enemy_lockable(e)) continue;
        hp = *(int32_t *)(e + CE_ENEMY_HP_CUR);
        if (hp <= 0) continue;
        flags = *(uint32_t *)(e + CE_ENEMY_FLAGS);
        s_target[n].x = ce_enemy_pos(e)[0];
        s_target[n].y = ce_enemy_pos(e)[1];
        s_target[n].hp = hp;
        s_target[n].boss = (flags & CE_ENEMY_FLAG_BOSS) ? 1u : 0u;
        *hp_sum += hp;
        total_dmg += hp + GW_LETHAL_SLACK;
        ++n;
    }
    if (n == 0) return 0;
    ce_raise_damage_cap(total_dmg);                       /* ★ 一次性抬到"这一帧要打的总伤害" */

    for (i = 0; i < n; ++i) {                             /* 第二遍：出伤害（+ 非 boss 撒金钱）*/
        float center[3];
        int32_t k, cnt, dmg = s_target[i].hp + GW_LETHAL_SLACK;
        center[0] = s_target[i].x;
        center[1] = s_target[i].y;
        center[2] = 0.0f;
        if (s_target[i].boss) {                           /* ★ boss：直接击破一个阶段 */
            ce_damage_rect(center, 0.0f, 2, dmg, GW_ENEMY_HIT_W, GW_ENEMY_HIT_H);
            ++(*breaks);
            continue;
        }
        cnt = gw_money_for_hp(s_target[i].hp);             /* 普通敌人：按剩余血量换金钱 */
        for (k = 0; k < cnt; ++k) {
            float p[3];
            float ang = -1.5707964f;
            if (GW_MONEY_ANGLE_SPREAD) {
                uint32_t r = ce_rand();
                ang = ((float)(int32_t)(r & 0xffffu) / 32768.0f - 1.0f) * 3.14159265f;
            }
            p[0] = center[0];
            p[1] = center[1];
            p[2] = 0.0f;
            if (ce_spawn_item(2u, p, ang, 1.8f)) ++(*money);
        }
        ce_damage_rect(center, 0.0f, 2, dmg, GW_ENEMY_HIT_W, GW_ENEMY_HIT_H);
    }
    return n;
}

static int on_activate(ce_card_t *c)
{
    gw_card_t *s = ce_state(c, gw_card_t);
    int64_t hp_sum = 0;
    int32_t money = 0, breaks = 0, bullets, frames, enemies;
    float px = ce_player_x();

    if (!s) return 0;

    bullets = gw_take_bullets(&money);
    enemies = gw_take_enemies(&hp_sum, &money, &breaks);

    if (bullets == 0 && enemies == 0)                     /* 空场：不烧充能 */
        return ce_refuse("great_work", "nothing to convert (no live bullet, no lockable enemy)");

    frames = gw_recharge(bullets, hp_sum);
    gw_set_recharge(c, frames);                           /* ④ 冷却按转换量等比例 */

    ++s->fired;
    s->last_frames = frames;
    s->last_bullets = bullets;
    s->last_hp = hp_sum;
    s->last_money = money;
    s->last_phase_breaks = breaks;
    ce_play_sound(GW_CARD_SE, px);
    ce_log("great_work: converted %d bullets + %d enemies (hp %lld) -> %d money items, "
           "%d boss phase(s) broken; recharge %d frames (%.1f s)",
           bullets, enemies, (long long)hp_sum, money, breaks,
           frames, (double)frames / 60.0);
    return 0;
}

CE_CARD(92, .active_recharge = GW_BASE, .on_activate = on_activate);
