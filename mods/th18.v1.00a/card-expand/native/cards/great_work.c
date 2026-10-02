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

/* ★ 自机也变黄金（owner 2026-10-03 的平衡）：调引擎自己的 `Player__die`。
 *   一手：`Player__die` = `0x45d3a0`（thiscall(Player*)、无栈参、裸 ret；内部干「状态→4 + 死亡特效 +
 *   8 帧决死窗 + 末尾广播卡虚表 +0x10」，**不扣命** —— 扣命在 `commit_death_and_enter_state2`），
 *   调用先例 = `cards/youkai.c` 的 `YK_FN_PLAYER_DIE` / `cards/trance.c` 的 `TRANCE_FN_PLAYER_DIE`。*/
#define GW_FN_PLAYER_DIE 0x45d3a0u
typedef void (__attribute__((thiscall)) *gw_fn_player_die_t)(void *player);
/* `Player__commit_death_and_enter_state2` = `0x45d090`（thiscall(Player*)）：**真正扣命 / 记 miss /
 * 状态→2（死亡动画）**。全 exe 只有 1 个调用点 `0x45c135`（状态 4 的 tick，决死窗走完/没救回时）。
 * owner 2026-10-03：「**我觉得不应该决死**」⇒ 我们走"开窗后**立刻**结算"，等价于窗口当场到期
 * （跳掉那 8 帧 + 跳掉按 X 的机会），但**开窗那一调照走** —— 引擎在 `Player__die` 里会广播卡虚表
 * +0x10（救援层 / 各类保护就挂在那儿）。*/
#define GW_FN_COMMIT_DEATH 0x45d090u
typedef void (__attribute__((thiscall)) *gw_fn_commit_death_t)(void *player);

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

/* ★★ 2026-10-03 owner：「**因为池子上限不能产生的金钱自动进腰包**」——
 *   道具池满时 `ce_spawn_item()` 直接失败，那一份钱原来就**丢了** ✗ ⇒ 现在照**拾取金钱道具那条路**
 *   原样补进腰包（一手：`ItemManager__collect_money_item` `0x446d2e` —— 一枚 type 2 金钱道具
 *   ⇒ `MONEY += 1` 且 `0x4ccd20` / `0x4ccd30` 各 +1，见 `engine/faith/th18/01-point-item-value.md` §3.1 第 1 行）。
 *   三个字段都是 `int32_t`：`0x4ccd34` = `CE_MONEY()`、`0x4ccd30` = `CE_MONEY_TOTAL()`、
 *   `0x4ccd20` = `CE_ADDR_MONEY_ITEMS`（吃到过的金钱道具个数）。*/
static void gw_pocket_money(void)
{
    CE_MONEY() += 1;
    CE_MONEY_TOTAL() += 1;
    *(volatile int32_t *)CE_ADDR_MONEY_ITEMS += 1;
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
    int n = 0, i, skipped_invuln = 0, skipped_dead = 0;
    int32_t total_dmg = 0;

    *hp_sum = 0;
    *money = 0;
    *breaks = 0;
    if (!em) return 0;

    /* ★★ 2026-10-03 owner 实测：「**没有把第一面第二波的中妖精杀死**」——
     *   根因是我们原来用 `ce_enemy_lockable()` 当门，而那个判据是 `CE_ENEMY_FLAG_NO_LOCK`
     *   的 **0x0c000021（任一位置起即"不可锁定"）**：它把 **boss 系（bit26/27）**也算了进去，
     *   于是中妖精 / 中 boss 这一类**能打**的目标被整只过滤掉 ✗。
     *   ⇒ 本卡改判「**活着 + 引擎自己的免伤位没置**」：
     *     · `hp <= 0` = 已经死了/正在收；
     *     · `flags & CE_ENEMY_FLAG_NO_DAMAGE (bit4)` = 引擎自己都不会扣它血（无敌/入场中）
     *       ⇒ 转了也是白转（伤害会被 `0x42f2b9` 那道闸跳过），所以不收。
     *   其余一律可转（含未上场的 —— 本卡的设定就是"全场变黄金"）。*/
    CE_ENEMY_FOREACH(em, e) {                             /* 第一遍：记账 + 抬上限 */
        int32_t hp;
        uint32_t flags;
        if (n >= GW_ENEMY_MAX) break;
        if (!e) continue;
        hp = *(int32_t *)(e + CE_ENEMY_HP_CUR);
        if (hp <= 0) { ++skipped_dead; continue; }
        flags = *(uint32_t *)(e + CE_ENEMY_FLAGS);
        if (flags & CE_ENEMY_FLAG_NO_DAMAGE) { ++skipped_invuln; continue; }
        s_target[n].x = ce_enemy_pos(e)[0];
        s_target[n].y = ce_enemy_pos(e)[1];
        s_target[n].hp = hp;
        s_target[n].boss = (flags & CE_ENEMY_FLAG_BOSS) ? 1u : 0u;
        *hp_sum += hp;
        total_dmg += hp + GW_LETHAL_SLACK;
        ++n;
    }
    if (n == 0) {
        if (skipped_invuln || skipped_dead)
            ce_log("great_work: 没可转的敌人（免伤中 %d / 已死 %d）", skipped_invuln, skipped_dead);
        return 0;
    }
    ce_raise_damage_cap(total_dmg);                       /* ★ 一次性抬到"这一帧要打的总伤害" */

    for (i = 0; i < n; ++i) {                             /* 第二遍：出伤害（+ 非 boss 撒金钱）*/
        float center[3];
        int pocket = 0;
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
            else { gw_pocket_money(); ++(*money); ++pocket; }
        }
        ce_damage_rect(center, 0.0f, 2, dmg, GW_ENEMY_HIT_W, GW_ENEMY_HIT_H);
        if (pocket)
            ce_log("great_work: 池满 ⇒ %d 枚金钱直接进腰包（MONEY +%d）", pocket, pocket);
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

    /* ★★ 2026-10-03 owner 的**平衡**：「伟大之作原作是会把**自机也变成黄金**，那就是**再减少冷却**，
     *   但是**会把我也杀死**」——**除非我处于无敌状态**。
     *   ⇒ 转换做完之后调引擎自己的 `Player__die`（thiscall(Player*)；先例 `youkai.c`/`trance.c`），
     *     走的是**原生 MISS 链**（状态→4 决死窗 → 没救回才 commit：扣命/炸弹归 3/掉一档），
     *     所以还留了 8 帧决死窗（按 X 能救回来 —— owner 没要求秒杀，先按原生的来）。
     *   ⇒ 无敌中（复活 280 帧 / 决死救回 60 / 炸弹各自那段 —— `player+0x47774`）**不杀**：
     *     否则"刚复活就用一次"会被自己的卡秒掉，等于把无敌帧吃掉了。
     *   ⚠ 只在**真的转换了东西**之后才走到这里（空场在上面 `ce_refuse` 早退了）⇒ 拒绝时不自杀。*/
    {
        ce_timer_t *inv = CE_PLAYER_INVULN();
        uint8_t *pl = CE_PLAYER();
        if (pl && (!inv || inv->cur <= 0)) {
            ((gw_fn_player_die_t)(uintptr_t)GW_FN_PLAYER_DIE)(pl);
            /* ★ 不给决死窗：`Player__die` 只是把状态推到 **4（决死窗）** 并广播卡虚表 +0x10（救援/保护
             *   就挂在那里）；**还停在 4** 就立刻结算（= 窗口当场到期，按 X 也来不及）。
             *   若这一调之后状态已经不是 4（被救援层/保护接住了）⇒ **不补刀**，尊重那道保护
             *   —— owner：「那些什么**咆哮状态**之类的**可以挡**」（咆哮的无敌 = `set_invuln(60)`
             *   写的就是 `player+0x47774`，上面那道 `inv->cur` 门已经先挡了一层）。*/
            if (*(int32_t *)(pl + CE_PLAYER_STATE) == 4) {
                ((gw_fn_commit_death_t)(uintptr_t)GW_FN_COMMIT_DEATH)(pl);
                ce_log("great_work: 自机也变成黄金 —— 开窗后**立刻结算**（无决死机会）");
            } else {
                ce_log("great_work: 自机也变成黄金 —— 但被保护/救援接住了（状态 %d）⇒ 不补刀",
                       *(int32_t *)(pl + CE_PLAYER_STATE));
            }
        } else {
            ce_log("great_work: 自机在无敌中 ⇒ 豁免这一死（owner 口径）");
        }
    }

    ce_log("great_work: converted %d bullets + %d enemies (hp %lld) -> %d money items, "
           "%d boss phase(s) broken; recharge %d frames (%.1f s)",
           bullets, enemies, (long long)hp_sum, money, breaks,
           frames, (double)frames / 60.0);
    return 0;
}

CE_CARD(92, .active_recharge = GW_BASE, .on_activate = on_activate);
