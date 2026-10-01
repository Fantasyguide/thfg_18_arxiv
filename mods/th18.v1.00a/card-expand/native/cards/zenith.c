/* 天顶剑（id 87）—— 装备卡，致敬《泰拉瑞亚》的天顶剑（Zenith，物品 4144）。
 *
 * 身边一把剑形**跟班**（原生装备卡的子机机制，与 71 破损核心 / 77 豌豆射手同一套 `ce_option_*`），
 * 每 ZN_PERIOD 帧朝**最近的敌人**掷出一把**自旋的剑刃**；剑刃直线飞一段（ZN_LIFE 帧）后消失。
 *
 * 三件事各自的出处（都能在仓里找到现成写法，没有自创机制）：
 *   ① 跟班：`Player__allocate_option`（引擎管位置 / 聚焦位移 / 开店收起）—— 见 `broken_core.c` 文件头；
 *      指针存本卡私有状态（我们的对象只有 0x54 字节，原生的 `card+0x54` 在对象外）。
 *      侧向站位自己写 `option+0x64/+0x6c`（原生卡 `th18:0x40b8c1` 同款；allocate 只写 y 偏移）。
 *   ② 挑最近的敌人：距离平方比较（照 `broken_core_core.c` 的 `bc_aim_*`，本卡自带一份是为了改射程）；
 *      角度用 `bc_atan2f`（确定性 SSE 多项式，不引 libm、零 x87）。
 *   ③ 会飞的伤害弹：**卡自管 ANM VM + 每帧一个小矩形伤害源** —— 炎魔火球（72）的写法：
 *      `ce_anm_spawn` 起 VM → 每帧 `ce_anm_set_pos` / `set_rotation` → 每帧在剑刃位置 `ce_damage_rect`
 *      （一个源对同一敌人只结算一次 ⇒ 要连续伤害就得每帧放新源）。`on_delete` 里逐个删 VM。
 *      ❓ 与炎魔唯一的差别：这是**多发在途的池**（炎魔只有一颗火球）—— 池的写法是新东西，
 *      数值（周期 / 速度 / 伤害 / 射程）一律 `❓ 自定`，登记在 `TUNING.md` 的 87 条目。
 *
 * 为什么不用 SHT shooterset 那条链（豌豆射手的写法）：`+0x1c` 只在**按住射击键**时广播，
 * 而这张卡的剑刃要跟班自己定时掷（不按键）—— 那条路要自己在 `on_tick_2` 里调 `0x40a9c0`，
 * 反而比 ANM 弹更绕，而且要把弹的贴图绑到 ability.anm 的 shooter 记录上。
 *
 * 纯逻辑（节奏 / 池 / 瞄准）在 zenith_core.c，主机单测 tests/test_zenith_core.c。 */
#include "sdk.h"
#include "zenith_core.h"
#include "broken_core_core.h"      /* bc_atan2f：确定性 SSE 多项式（见那个头；禁止调引擎的 x87 atan2）*/

#define ZN_OPT_ANM_Y    0x10       /* allocate 的偏移参（引擎把它写进 option+0x68/+0x70 两个 y 偏移）*/
#define ZN_OPT_NF_X     46.0f      /* 非聚焦：自机右前方一点 ❓ 自定（allocate 只给 y，x 自己写）*/
#define ZN_OPT_NF_Y     (-14.0f)
#define ZN_OPT_F_X      36.0f      /* 聚焦：收拢一点 ❓ 自定 */
#define ZN_OPT_F_Y      (-20.0f)
#define ZN_RETRY        60         /* 关卡中途买到这张卡：没拿到子机时的重试间隔（槽满 / 广播还没来）*/
#define ZN_LAYER        13         /* 世界特效层（破损核心 / 炎魔 / 青眼同层）*/
#define ZN_SE_SLASH     0x2d       /* TH18 音效表 0x2d = se_slash.wav（一手 engine/_shared/th18-sound-table.md）*/

typedef struct {
    ce_option_t opt;               /* 跟班槽（SDK：认领 / 申请 / 收起）*/
    zn_state_t  st;                /* 池 + 节奏（纯逻辑）*/
} zn_card_t;

_Static_assert(sizeof(zn_card_t) <= 496, "zenith: private state overflows CE_STATE_BYTES");

/* 侧向站位：allocate 只会写 y 偏移（第 2 / 第 5 个栈参 ⇒ option+0x68 / +0x70），x 的两格留给卡自己写
 *（一手 `th18:0x40a80a` 写 0、原生卡 `th18:0x40b8c1` 自己摆阵型）。单位 = 像素，引擎每帧 ×128 用。 */
static void zn_place(ce_option_t *o)
{
    uint8_t *p = o ? o->ptr : 0;
    if (!p) return;
    *(int32_t *)(p + CE_OPT_OFF_NF_X) = (int32_t)(ZN_OPT_NF_X * 128.0f);
    *(int32_t *)(p + CE_OPT_OFF_NF_Y) = (int32_t)(ZN_OPT_NF_Y * 128.0f);
    *(int32_t *)(p + CE_OPT_OFF_F_X)  = (int32_t)(ZN_OPT_F_X * 128.0f);
    *(int32_t *)(p + CE_OPT_OFF_F_Y)  = (int32_t)(ZN_OPT_F_Y * 128.0f);
}

/* 收掉所有在飞的剑刃 VM。**不依赖 tick**（商店里删卡时只有 on_delete 跑得到）。 */
static void zn_kill_blades(ce_card_t *c)
{
    zn_card_t *s = ce_state(c, zn_card_t);
    int i;
    if (!s) return;
    for (i = 0; i < ZN_BLADE_MAX; ++i) {
        if (s->st.b[i].id) ce_anm_delete(s->st.b[i].id);
        s->st.b[i].id = 0;
        s->st.b[i].active = 0;
    }
}

/* +0x18：开局 / 火力档变 / 关店后广播（池刚被重建过）—— 无条件重新申请一个，与原生装备卡一致。 */
static int on_power_level_change(ce_card_t *c)
{
    zn_card_t *s = ce_state(c, zn_card_t);
    if (s) {
        ce_option_alloc(c, &s->opt, ZN_OPT_ANM_Y, CE_ANM_ABILITY_SCRIPT_ZENITH_OPT, "zenith");
        zn_place(&s->opt);
    }
    return 0;
}

/* +0x4c（局末）/ +0x20（每关开场）—— 两条路共用：收剑刃 + 还子机槽 + 清节奏。 */
static void on_run_reset(ce_card_t *c)
{
    zn_card_t *s = ce_state(c, zn_card_t);
    zn_kill_blades(c);
    ce_option_drop(c, s ? &s->opt : 0);
    if (s) zn_reset(&s->st);
}

/* +0x50：卡被局中删掉（倒吊人 / 原版空白卡弃整副牌，都在商店里成交）—— 那一刻 tick 全停、
 * on_load 不会再来，只有这里跑得到。子机槽没有别的回收者（12 格漏完就申请不到），剑刃 VM 也一样。 */
static void on_delete(ce_card_t *c)
{
    on_run_reset(c);
}

static int on_load(ce_card_t *c)
{
    on_run_reset(c);
    return 0;
}

/* +0x2c：AbilityManager 每帧（菜单 / 商店 / 暂停里不跑；优先级 0x16，在 GameThread 复位伤害上限 0x10 之后、
 * 敌人结算 0x1b 之前 ⇒ 在这里抬上限只对当帧生效）。 */
static int on_tick_2(ce_card_t *c)
{
    zn_card_t *s = ce_state(c, zn_card_t);
    uint8_t *p = CE_PLAYER();
    uint8_t *em = CE_ENEMY_MGR();
    uint8_t *o, *enemy;
    zn_aim_t aim;
    zn_blade_t *b;
    float ox, oy, pz;
    uint32_t id;
    int i;

    if (!s || !p) return 0;
    o = ce_option_get(c, &s->opt, ZN_OPT_ANM_Y, CE_ANM_ABILITY_SCRIPT_ZENITH_OPT, ZN_RETRY, "zenith");
    if (!o) return 0;
    zn_place(&s->opt);                          /* 补申请 / 重建过 ⇒ 站位重新写一遍（幂等）*/
    pz = ce_player_pos()[2];

    /* ① 推进在飞的剑刃：写位姿 + 每帧在剑刃位置放一个小伤害源。
     *    一个源对同一敌人只结算一次（src+0x84 tag 守卫）⇒ 连续伤害必须逐帧放新源（炎魔同款）。*/
    for (i = 0; i < ZN_BLADE_MAX; ++i) {
        b = &s->st.b[i];
        if (!b->active) continue;
        if (!ce_anm_get_vm(b->id)) {            /* VM 被引擎收掉了（脚本 delete / 被别的路清）*/
            b->active = 0; b->id = 0;
            continue;
        }
        zn_advance(b);
        if (!b->active) {                       /* 寿命尽 / 出界：收 VM、放空槽 */
            ce_anm_delete(b->id);
            b->id = 0;
            continue;
        }
        ce_anm_set_pos(b->id, b->x, b->y, pz);
        ce_anm_set_rotation(b->id, 0.0f, 0.0f, b->rot);
        {
            float center[3] = { b->x, b->y, pz };
            ce_damage_rect(center, 0.0f, ZN_HIT_LIFE, ZN_DMG, ZN_HIT_W, ZN_HIT_H);
        }
    }
    if (zn_live(&s->st) > 0) ce_raise_damage_cap(ZN_CAP);   /* 自机弹与卡共用每敌每帧上限：不抬就被自机弹吃满 */

    /* ② 蓄满才掷；没目标 / 池满就一直攒着（蓄满状态保持，敌人一进射程就出手）。*/
    if (!zn_charge(&s->st) || !em) return 0;
    ox = ce_option_x(o);
    oy = ce_option_y(o);
    zn_aim_reset(&aim);
    CE_ENEMY_FOREACH(em, enemy) {
        if (!ce_enemy_lockable(enemy)) continue;
        zn_aim_consider(&aim, ox, oy, ce_enemy_pos(enemy)[0], ce_enemy_pos(enemy)[1]);
    }
    if (!zn_aim_ready(&aim)) return 0;
    b = zn_slot(&s->st);
    if (!b) return 0;
    id = ce_anm_spawn(CE_ABILITY_ANM(), CE_ANM_ABILITY_SCRIPT_ZENITH_BLADE, ZN_LAYER);
    if (!id) return 0;
    zn_launch(b, id, ox, oy, aim.dx, aim.dy);
    ce_anm_set_pos(id, b->x, b->y, pz);
    ce_anm_set_rotation(id, 0.0f, 0.0f, b->rot);
    zn_did_fire(&s->st);
    ce_play_sound(ZN_SE_SLASH, ox);
    if (s->st.fired <= 3 || s->st.fired % 20 == 0)
        ce_log("zenith: blade #%u at frame %u, fam (%.1f, %.1f) -> (%.1f, %.1f) angle %.3f, live %d",
               s->st.fired, s->st.frames, ox, oy, ox + aim.dx, oy + aim.dy,
               bc_atan2f(aim.dy, aim.dx), zn_live(&s->st));
    return 0;
}

CE_CARD(87, .on_power_level_change = on_power_level_change, .on_tick_2 = on_tick_2,
            .on_load = on_load, .on_run_reset = on_run_reset, .on_delete = on_delete);
