/* 豌豆射手（id 77）—— 装备卡，致敬《植物大战僵尸》。
 *
 * 身边一株豌豆射手子机，**朝正上方**吐豌豆；平时按 PVZ 原版的射速（1.425 s ≈ 85 帧）一颗一颗地打，
 * 每隔 20–30 秒「开大」一次：变成 PVZ 的机枪射手，100 帧里泼出 20 排 × 7 = 140 颗小豌豆。
 *
 * 这是本 mod 第一张真正走 **SHT shooterset** 那条链的卡（`assets/sht/append_shooterset.py` 造好之后
 * 一直没人用：破损核心第一版走过、后来改成定点伤害源，因为它要的是「瞬发单体」）。链路：
 *   ① `on_power_level_change`（+0x18）里 `Player__allocate_option` 要一个子机槽 —— 与破损核心同款，
 *      引擎管它的位置 / 聚焦位移 / 进店收起；指针存 `ce_state()`（对象只有 0x54 字节，+0x54 在对象外）。
 *      认领 / 申请 / 收起三件套是 SDK 的 `ce_option_*`（2026-09-08 从这里与破损核心提炼上去）。
 *   ② `on_tick_shooters`（+0x1c，只在按住射击键时广播）里把子机 + 一组 shooterset 索引交给
 *      `Player__tick_shooters_for_ability_card`，由它按 `timer % fire_rate == start_delay` 出膛。
 *      **short_timer 我们自己数**：引擎那个（`player+0x476d4`）松开射击键会跳，相位不可控。
 *   ③ 弹的贴图也在 `ability.anm`（开火期间引擎把 `player+0x10` 换成它），所以 shooter 的 anm 字段
 *      填 `CE_ANM_ABILITY_SCRIPT_PEA` / `_PEA_SMALL`，四个 `pl0X.anm` 一个字节都不动。
 *
 * 每敌每帧伤害上限：平时那颗 130 装不进 Reimu 的 90，所以有豌豆在途的帧把它抬到 PEA_CAP
 * （原生 `CardRemilia__on_tick_2` `0x40f48e` 就是这么干的）。开大那排 7 × 7 = 49 反而不用抬。
 *
 * 纯逻辑（节奏状态机）在 peashooter_core.c，主机单测。审计 AUDIT §AJ。 */
#include "sdk.h"
#include "peashooter_core.h"
#include "broken_core_core.h"   /* ★ P40：只为 bc_atan2f（纯 SSE 确定性 atan2 ✓，与 71/animal.c 同一先例 ✓）*/

/* ★★★ 2026-10-14（P40，owner 需求逐字）：「协同效应：当豌豆射手和破损核心同时装备的时候，豌豆射手
 *   会进化为「电大哥」；射出的子弹将会变成电弹，电弹的效果除了本体核心之外，就是闪电链」✓
 *
 * 机制依据（**社区攻略 + owner 口径** ✓，按仓库口径标 `复刻/社区`，**不是一手** ✗）：
 *   · 国服 PvZ2 电能豌豆：「发射带有雷电条的豌豆，**穿透**造成伤害，同时可对**上下行造成电弧连锁伤害**」
 *     （小米游戏中心《电能豌豆综合玩法教学》✓）；大招 = 三个最近目标 + 巨型电能球分裂 ✓。
 *   · owner：「**连接自身一个半径范围内的敌人，敌人还会再连接敌人**」✓
 *   ⇒ TH18 是 360°（没有行）⇒ 把"上下行"换成**半径**、并**逐跳传播** ✓。
 *   ⚠ 与攻略的**已知差异**（诚实标 ✓）：攻略里的豌豆**穿透**，我们这套 SHT 豌豆不穿透 ✗
 *     ⇒ 本版**不做穿透**（要做就得加一套 shooterset ⇒ 另开一单 ✓）。
 *
 * 实现（**0 条新增站点** ✓ R5）：同装判据 = `ce_owned(71)` ✓；命中钩子 = `on_bullet_hit` ✓；
 *   伤害 = `ce_damage_rect` ✓；特效 = 复用 71 的 BEAM(89)/SPARK(90) + 电流音(0x46) ✓；
 *   染色 = P14a 刚抽的 `ce_color_set_rgb()` ✓（只改 RGB、保 alpha ✓）。*/
/* 弹壳里的 anm VM 偏移：**本地定义**（与 `cards/lolk.c`（115 绀珠传）同一个值 `0x28` ✓ —— 同为"自机弹"
 * 这一型对象 ✓；共享 `sdk.h` 里没有导它，而按 R2 我不擅自改共享件 ✗ ⇒ 照 lolk.c 的先例本地写 ✓）。
 * 颜色字段 = `vm + CE_ANM_VM_COLOR1` ✓（CE_ANM_VM_COLOR1 是共享 SDK 里的 ✓）。*/
#define PS_BULLET_VM    0x28
/* ★★ P42（owner 逐字）：「**1、穿透；2、不要麻痹**」+「**3、为了防止重复创建VM，从敌人A到敌人B的VM
 *   只创建一个**」✓
 *   ① **穿透 = 卡侧续弹**：豌豆命中时，在**原位往前 PS_PIERCE_ADV** 再发一颗**同组同行**的弹
 *      （`Player__shoot_one_bullet` `th18:0x45e930`，thiscall(player; packed_id, timer, pos*, option) ✓
 *       —— 与 89 星星炮/动物灵同一先例 ✓，**不是 hook、无新 BP / 无 sites.py 改动** ✓）⇒ 视觉上穿过去 ✓。
 *      认弹用 `bullet+0xac` 的打包 id（`(set<<8)|row` ✓，先例 animal.c/youkai.c ✓）。
 *   ② **不做麻痹** ✓（owner 说不要 ✓ —— 本节一行都没写 ✓）。
 *   ③ **弧按敌人对去重**：一张 `(敌A,敌B) 无序对 → anm id` 的表 ✓；同对再链**只刷新**已有 VM ✓
 *      （位置/角度/缩放重算 ✓）**不新建** ✓；表满时**宁可不建**也不重复创建 ✓。*/
#define PS_BULLET_PACKED_ID 0xac   /* 弹里的打包 id = `(set<<8)|row`（本地定义 ✓，与 animal.c/youkai.c 同源 ✓）*/
#define PS_FN_PLAYER_SHOOT_ONE_BULLET 0x45e930u   /* 卡侧发一颗弹（无新 BP ✓）*/
#define PS_PIERCE_ADV   24.0f      /* ❓自定：续弹落在命中点**往前**多少 px（避开同一只敌 ✓）*/
#define PS_PIERCE_CD       2       /* ❓自定：至少隔几帧才续一次（防一帧多敌时连环续 ✗）*/
#define PS_PIERCE_CAP   4000u      /* 保险：一局最多续这么多颗（防失控 ✓）*/
#define PS_ARC_MAX        32       /* ❓自定：同时记住多少条"按对去重"的电弧 ✓*/
#define PS_ELEC_CARD     71            /* 同装的另一张 = 破损核心 ✓ */
#define PS_ELEC_RGB      0x00a0d0ffu   /* 电弹颜色（电光蓝 ✓）：`0x00RRGGBB` ✓ */
#define PS_ELEC_MAX      128           /* 同时追踪的电弹上限 ✓（开大在途的豌豆 ≤ 约 50 ✓）*/
#define PS_CHAIN_HOPS      4           /* 闪电链最多跳几段 ✓（含第一跳 ✓："敌人再连敌人" ⇒ 逐跳 ✓）*/
#define PS_CHAIN_RADIUS 150.0f         /* 每跳半径（px ✓，把 PvZ 的"上下行"换成 360° 半径 ✓）*/
#define PS_CHAIN_DMG       8           /* ❓自定：每段伤害（71 一道闪电 = 30×4 = 120 ✓ ⇒ 链只给零头 ✓，等 owner 调 ✓）*/
#define PS_CHAIN_W      32.0f          /* 判定框 = 与 71 同口径（BC_HIT_W/H ✓）*/
#define PS_CHAIN_H      32.0f
#define PS_CHAIN_LIFE      2           /* 帧：与 71 的 BC_HIT_LIFE 一致 ✓ */
#define PS_CHAIN_CD       12           /* ❓自定：**节流** —— 至少隔这么多帧才放一次链（开大 140 颗会命中很多次 ✓，
                                        *   没有节流链会失控 ✓；等 owner 调 ✓）*/
#define PS_CHAIN_SE     0x46           /* 与 71 同一个"电流"音效 ✓（BC_SE ✓）*/
#define PS_BEAM_TEX_W  256.0f          /* 71 的电弧贴图宽（scale.x = 距离 / 它 ✓）*/

typedef struct { uint8_t *b; uint32_t orig; } ps_elec_t;
static ps_elec_t s_elec[PS_ELEC_MAX];
static uint32_t  s_arc_keys[PS_ARC_MAX];   /* (敌A,敌B) 无序对 ✓ */
static uint32_t  s_arc_vms[PS_ARC_MAX];    /* 那一对的电弧 VM（0 = 还没建 ✓）*/
static uint8_t   s_arc_alive[PS_ARC_MAX];  /* 本帧该 VM 还活着吗（每帧刷 ✓）*/
static uint32_t  s_pierce_cd, s_pierce_n;
typedef int (__attribute__((thiscall)) *ps_shoot_one_fn_t)(void *player, int packed_id, int timer,
                                                           const float *pos, void *option);
static int       s_elec_n;
static uint32_t  s_chain_cd;

static int ps_elec_on(void) { return ce_owned(PS_ELEC_CARD) ? 1 : 0; }   /* 同装判据（每帧现算 ⇒ 中途买/删都跟得上 ✓）*/
static void ps_elec_clear(void)
{
    int i;
    s_elec_n = 0;
    s_chain_cd = 0;
    s_pierce_cd = 0;
    s_pierce_n = 0;
    for (i = 0; i < PS_ARC_MAX; ++i) {                 /* 把还记得的电弧 VM 删掉（不留东西 ✓）*/
        if (s_arc_vms[i] && ce_anm_get_vm(s_arc_vms[i])) ce_anm_delete(s_arc_vms[i]);
        s_arc_keys[i] = 0; s_arc_vms[i] = 0; s_arc_alive[i] = 0;
    }
}

/* 每帧：死弹**还原颜色**（⚠ 不还原的话，弹槽被新弹复用时它一出生就是电光色 ✗ —— P29 的教训 ✓），
 *   同时递减链的节流 ✓。判活弹用 `CE_BULLET_STATE != 0`（与 109/115 同一口径 ✓）。*/
static void ps_elec_tick(void)
{
    int i, k = 0;
    int a;
    if (s_chain_cd) --s_chain_cd;
    if (s_pierce_cd) --s_pierce_cd;
    for (a = 0; a < PS_ARC_MAX; ++a)                    /* 弧 VM 存活（去重要用它 ✓）*/
        s_arc_alive[a] = (s_arc_vms[a] && ce_anm_get_vm(s_arc_vms[a])) ? 1 : 0;
    for (i = 0; i < s_elec_n; ++i) {
        uint8_t *b = s_elec[i].b;
        if (!b || *(const uint16_t *)(b + CE_BULLET_STATE) == 0) {
            if (b) ce_color_set_rgb((volatile uint32_t *)(b + PS_BULLET_VM + CE_ANM_VM_COLOR1), s_elec[i].orig);
            continue;
        }
        s_elec[k++] = s_elec[i];
    }
    s_elec_n = k;
}

/* +0x?? 出膛：同装时把刚出生的豌豆染成电弹并登记 ✓ */
static int on_bullet_created(ce_card_t *c, void *bullet)
{
    uint8_t *b = (uint8_t *)bullet;
    volatile uint32_t *col;
    (void)c;
    if (!b || !ps_elec_on() || s_elec_n >= PS_ELEC_MAX) return 0;
    col = (volatile uint32_t *)(b + PS_BULLET_VM + CE_ANM_VM_COLOR1);
    s_elec[s_elec_n].b = b;
    s_elec[s_elec_n].orig = *col;
    ++s_elec_n;
    ce_color_set_rgb(col, PS_ELEC_RGB);
    return 0;
}

/* 命中 ⇒ **闪电链** ✓。⚠ 钩子拿不到敌指针（`hit->enemy` 恒 0 ✗）⇒ 只用坐标 + id ✓。*/
/* ★ P42：一对敌（A,B）**只建一个**电弧 VM ✓（owner 第 3 点）；同对再链**只刷新** ✓；
 *   表满且无同对 ⇒ **宁可不建** ✓。（VM 本体是引擎的 ✓，我们只记 id ✓，删卡时在 ps_elec_clear 里删掉 ✓。）*/
static void ps_arc_show(uint32_t ida, uint32_t idb, float px, float py, float tx, float ty, float pz)
{
    uint32_t key = pea_arc_key(ida, idb);
    int slot = pea_arc_find(s_arc_keys, PS_ARC_MAX, key);
    uint32_t vm = (slot >= 0) ? s_arc_vms[slot] : 0;
    float dx = tx - px, dy = ty - py;
    float dist = __builtin_sqrtf(dx * dx + dy * dy);
    float ang = bc_atan2f(dy, dx);
    float sx = dist * (1.0f / PS_BEAM_TEX_W);
    if (vm && ce_anm_get_vm(vm)) {          /* ★ 同对已经有活着的弧 ⇒ 只刷新，不新建 ✓ */
        ce_anm_set_pos(vm, px, py, pz);
        ce_anm_set_rotation(vm, 0.0f, 0.0f, ang);
        ce_anm_set_scale(vm, sx, 1.0f);
        ce_anm_set_fvar(vm, 0, sx);
        return;
    }
    if (slot < 0) {                          /* 没有同对的槽 ⇒ 先"预定"一个空槽 ✓ */
        slot = pea_arc_put(s_arc_keys, s_arc_vms, s_arc_alive, PS_ARC_MAX, key, 0);
        if (slot < 0) return;                /* ★ 表满且无同对 ⇒ 宁可不建 ✓ */
    }
    vm = ce_anm_spawn(CE_ABILITY_ANM(), CE_ANM_ABILITY_SCRIPT_BROKEN_CORE_BEAM, 13);
    if (!vm) return;
    ce_anm_set_pos(vm, px, py, pz);
    ce_anm_set_rotation(vm, 0.0f, 0.0f, ang);
    ce_anm_set_scale(vm, sx, 1.0f);
    ce_anm_set_fvar(vm, 0, sx);
    s_arc_keys[slot] = key; s_arc_vms[slot] = vm; s_arc_alive[slot] = 1;
}

/* ★ P42 ① 穿透 = 卡侧续弹：命中时在**原位往前 PS_PIERCE_ADV** 再发一颗**同组同行**的弹 ✓
 *   （`Player__shoot_one_bullet` ✓，与 89 星星炮/动物灵同一先例 ✓，**无新 BP** ✓）。*/
static void ps_pierce_try(const ce_bullet_hit_t *hit, float hx, float hy, float pz)
{
    uint8_t *bm, *pb;
    int packed, set;
    void *pl;
    float pos[3];
    if (s_pierce_cd || s_pierce_n >= PS_PIERCE_CAP) return;
    bm = CE_BULLET_MGR();
    if (!bm || hit->bullet_i <= 0) return;
    pb = bm + CE_BM_BULLETS + (size_t)hit->bullet_i * CE_BULLET_STRIDE;
    packed = *(const int *)(pb + PS_BULLET_PACKED_ID);        /* (set<<8)|row ✓ */
    set = (packed >> 8) & 0xff;
    if (set != (CE_SHT_SET_PEA_NORMAL & 0xff) && set != (CE_SHT_SET_PEA_GATLING & 0xff)) return;
    pl = CE_PLAYER();
    if (!pl) return;
    pos[0] = hx; pos[1] = hy - PS_PIERCE_ADV; pos[2] = pz;    /* PEA 两组都是正上方直飞 ✓ ⇒ "往前" = −y ✓ */
    if (((ps_shoot_one_fn_t)PS_FN_PLAYER_SHOOT_ONE_BULLET)(pl, packed, 0, pos, 0)) {
        ++s_pierce_n;
        s_pierce_cd = PS_PIERCE_CD;
    }
}

/* 命中 ⇒ **闪电链** ✓ + **穿透** ✓（两者的节流各自独立 ✓）。
 * ⚠ 钩子拿不到敌指针（`hit->enemy` 恒 0 ✗）⇒ 只用坐标 + id ✓。*/
static void on_bullet_hit(ce_card_t *c, const ce_bullet_hit_t *hit)
{
    float cand[2 * PS_ELEC_MAX];
    uint32_t cid[PS_ELEC_MAX], visited[PS_CHAIN_HOPS + 2];
    float px, py, hx, hy, pz;
    uint32_t prev_id;
    uint8_t *em, *e;
    int n = 0, nv = 0, hop;
    (void)c;
    if (!hit || !ps_elec_on()) return;
    px = hit->has_enemy_pos ? hit->ex : hit->x;
    py = hit->has_enemy_pos ? hit->ey : hit->y;
    hx = px; hy = py;
    pz = ce_player_pos()[2];

    if (!s_chain_cd) {                       /* ── 链（自己的节流 ✓）── */
        em = CE_ENEMY_MGR();
        if (em) {
            CE_ENEMY_FOREACH(em, e) {
                const float *pp;
                uint32_t id;
                if (!ce_enemy_lockable(e)) continue;
                if (n >= PS_ELEC_MAX) break;
                pp = ce_enemy_pos(e);
                id = *(const uint32_t *)(e + CE_ENEMY_ID);
                if (id == 0 || (hit->enemy_id && id == hit->enemy_id)) continue;   /* 命中那只已吃过豌豆伤害 ✓ */
                cand[2 * n] = pp[0]; cand[2 * n + 1] = pp[1]; cid[n] = id; ++n;
            }
        }
        if (n > 0) {
            prev_id = hit->enemy_id;
            visited[nv++] = hit->enemy_id;             /* 连过的记下来 ⇒ 防成环 ✓ */
            for (hop = 0; hop < PS_CHAIN_HOPS; ++hop) {
                int k = pea_chain_pick(cand, cid, n, px, py, PS_CHAIN_RADIUS, visited, nv);
                float tx, ty;
                uint32_t spark;
                if (k < 0) break;                      /* 半径内没有新目标 ⇒ 链到此为止 ✓ */
                tx = cand[2 * k]; ty = cand[2 * k + 1];
                {   /* 伤害：定点小判定，与 71 同一套（life/dmg/w/h ✓）*/
                    float center[3] = { tx, ty, pz };
                    ce_damage_rect(center, 0.0f, PS_CHAIN_LIFE, PS_CHAIN_DMG, PS_CHAIN_W, PS_CHAIN_H);
                }
                ps_arc_show(prev_id, cid[k], px, py, tx, ty, pz);   /* ★ 弧：按对去重 ✓ */
                spark = ce_anm_spawn(CE_ABILITY_ANM(), CE_ANM_ABILITY_SCRIPT_BROKEN_CORE_SPARK, 13);
                if (spark) ce_anm_set_pos(spark, tx, ty, pz);
                ce_play_sound(PS_CHAIN_SE, tx);
                visited[nv++] = cid[k];
                prev_id = cid[k];
                px = tx; py = ty;                      /* ★ 下一跳从这里出发 ⇒「敌人还会再连接敌人」✓ */
                if (nv >= (int)(sizeof visited / sizeof visited[0])) break;
            }
            if (nv > 1) {
                s_chain_cd = PS_CHAIN_CD;
                ce_log("peashooter: 电大哥 chain %d 跳（命中 id %u）", nv - 1, hit->enemy_id);
            }
        }
    }
    ps_pierce_try(hit, hx, hy, pz);            /* ── 穿透（自己的节流 ✓）── */
}

#define PS_OPTION_OFFSET  0x28      /* 子机相对自机的横向偏移；破损核心用 0x18，错开以便两张同时装 */
#define PS_RETRY_FRAMES   60        /* 没拿到子机时的重试间隔（槽满 / 关卡中途买到）*/
#define PS_SE_BURST       0x0d      /* se_powerup：开大起手那一下 */
#define PS_BURST_LAYER    15        /* script103 自己写的层，spawn 时给同一个 */

typedef struct {
    ce_option_t opt;                /* 子机槽（SDK：认领 / 申请 / 收起）*/
    pea_state_t st;
} ps_card_t;

/* +0x18：开局 / 火力档变 / 关店后广播，池刚被重建过 —— 无条件重新申请，与原生装备卡一致。 */
static int on_power_level_change(ce_card_t *c)
{
    ps_card_t *s = ce_state(c, ps_card_t);
    if (s) ce_option_alloc(c, &s->opt, PS_OPTION_OFFSET, CE_ANM_ABILITY_SCRIPT_PEASHOOTER_OPT, "peashooter");
    return 0;
}

/* +0x4c / +0x20：照 `CardReimu1__method_4C` `0x40aad0` 与 `____on_load__2` `0x40aab0`。
 * 节奏一起清零 —— 局末重开一局不该继承上一局攒了多久。 */
static void on_run_reset(ce_card_t *c)
{
    ps_elec_clear();
    ps_card_t *s = ce_state(c, ps_card_t);
    ce_option_drop(c, s ? &s->opt : 0);      /* 删子机 VM + 清原生在此清的 flags bit1 */
    if (s) pea_clear(&s->st);
}

/* ★ 2026-09-10：卡被局中删掉（倒吊人，商店里成交）时也要**把子机还回池子** ——
 * `on_run_reset` 要等局末，而在商店里 `AbilityManager::on_tick` 冻着，`on_load` 也不会再来。
 * 不收的后果不只是画面：`Player__allocate_option` 的 12 格槽**没有别的回收者**，
 * 每漏一次就少一格（日志会是 `option allocated FAILED (pool full)`）。*/
static void on_delete(ce_card_t *c)
{
    ps_elec_clear();
    ps_card_t *s = ce_state(c, ps_card_t);
    ce_option_drop(c, s ? &s->opt : 0);
}

static int on_load(ce_card_t *c)
{
    on_run_reset(c);
    return 0;
}

/* +0x2c：AbilityManager 每帧（菜单 / 商店里不跑；优先级 0x16，在 GameThread 复位上限 0x10 之后、
 * 敌人结算 0x1b 之前 —— 所以在这里抬上限只对当帧生效，不用还原）。 */
static int on_tick_2(ce_card_t *c)
{
    ps_elec_tick();
    ps_card_t *s = ce_state(c, ps_card_t);
    if (!s || !CE_PLAYER()) return 0;
    if (!ce_option_claim(c, &s->opt)) {           /* 关卡中途买到这张卡：广播可能还没来，自己补一个 */
        ce_option_retry(c, &s->opt, PS_OPTION_OFFSET, CE_ANM_ABILITY_SCRIPT_PEASHOOTER_OPT,
                        PS_RETRY_FRAMES, "peashooter");
        return 0;                                /* 补上的那一帧不出膛，节奏从下一帧起算 */
    }
    pea_frame(&s->st);
    if (pea_needs_cap(&s->st)) ce_raise_damage_cap(PEA_CAP);
    return 0;
}

/* +0x1c：`Player__tick_shooting_state` `0x45ea00` 每射击帧对每张卡调。引擎给的两个计时器我们不用
 * （见文件头 ②），节奏全在 pea_shoot_frame 里。 */
static int on_tick_shooters(ce_card_t *c, uint32_t short_timer, uint32_t long_timer)
{
    ps_card_t *s = ce_state(c, ps_card_t);
    uint8_t *o;
    pea_out_t out;

    (void)short_timer;
    (void)long_timer;
    if (!s) return 0;
    o = ce_option_claim(c, &s->opt);
    if (!o) return 0;

    /* 随机只在这两处抽（开局一次 + 每次开完大一次），且都由射击帧驱动 ⇒ replay 里调用序列一致。 */
    if (s->st.pending_wait) {
        pea_set_wait(&s->st, ce_rand());
        ce_log("peashooter: next gatling in %u shooting frames", s->st.wait);
    }
    pea_shoot_frame(&s->st, &out);
    ce_tick_shooters_for_card(o, (int)out.timer, 0,
                              out.set == PEA_SET_GATLING ? CE_SHT_SET_PEA_GATLING : CE_SHT_SET_PEA_NORMAL);

    if (out.burst_start) {
        float ox = ce_option_x(o);
        float oy = ce_option_y(o);
        float pz = ce_player_pos()[2];
        uint32_t fx = ce_anm_spawn(CE_ABILITY_ANM(), CE_ANM_ABILITY_SCRIPT_PEA_BURST, PS_BURST_LAYER);
        if (fx) ce_anm_set_pos(fx, ox, oy, pz);
        ce_play_sound(PS_SE_BURST, ox);
        ce_log("peashooter: gatling #%u start at (%.1f, %.1f): %d frames x %d peas / %d frames = %d peas",
               s->st.bursts, ox, oy, PEA_GAT_FRAMES / PEA_GAT_INTERVAL, PEA_GAT_ROWS,
               PEA_GAT_INTERVAL, PEA_GAT_FRAMES / PEA_GAT_INTERVAL * PEA_GAT_ROWS);
    }
    if (out.burst_end)
        ce_log("peashooter: gatling #%u done, %u peas fired in total", s->st.bursts, s->st.peas);
    return 0;
}

CE_CARD(77, .on_power_level_change = on_power_level_change, .on_tick_shooters = on_tick_shooters,
            .on_bullet_created = on_bullet_created, .on_bullet_hit = on_bullet_hit,   /* ★ P40：电大哥协同 ✓ */
            .on_tick_2 = on_tick_2, .on_load = on_load, .on_run_reset = on_run_reset,
            .on_delete = on_delete);
