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

/* ★★★ 2026-10-14（P40/P42/P43，owner 逐字）：
 *   ·「协同效应：当豌豆射手和破损核心同时装备的时候，豌豆射手会进化为「电大哥」；射出的子弹将会变成电弹，
 *      电弹的效果除了本体核心之外，就是闪电链」✓
 *   ·「电子弹的大概效果是，连接自身一个半径范围内的敌人，敌人还会再连接敌人」✓
 *   ·「1、穿透；2、不要麻痹」✓「3、为了防止重复创建VM，从敌人A到敌人B的VM只创建一个」✓
 *   ·「**改成真穿透**」✓（P43 推翻 P42 的"续弹"近似 ✗）
 *
 * 机制依据（**社区攻略 + owner 口径** ✓，标 `复刻/社区`，**不是一手** ✗）：国服 PvZ2 电能豌豆（俗称电大哥）
 *   「发射带有雷电条的豌豆，**穿透**造成伤害，同时可对**上下行造成电弧连锁伤害**」（小米游戏中心攻略 ✓）；
 *   TH18 是 360°（没有行）⇒ 把"上下行"换成**半径** ✓ 并逐跳传播 ✓。
 *
 * ★ P43 的做法 = **照 89 星星炮那套"真穿透"** ✓（我 P42 引错了先例 ✗ —— 89 **没有**续弹：
 *   它是**卡侧自建弹** + **命中不销毁** + 对同一敌每 `SC_HIT_COOLDOWN`(10) 帧才结算一次 ✓，对位原版
 *   `Projectile.SetDefaults` 的 `penetrate = -1` ✓）：
 *     · 同装（`ce_owned(71)` ✓）时**不发引擎豌豆** ✗，改由卡侧 `ce_anm_spawn` 起电弹 ✓；
 *     · 每帧自己挪（`pe_proj_move` ✓）、每 `PE_HIT_COOLDOWN` 帧出一个**定点伤害源**（`ce_damage_rect` ✓）；
 *     · **命中原生就不销毁** ⇒ 真穿透 ✓（一颗弹穿一个敌 = 每 10 帧 1 跳 ✓，**不是帧伤** ✓）；
 *     · 出界即收 ✓；删卡 / 局末把弹与它们的 VM 一并收掉 ✓（铁律「长持续效果必须交代被删掉会怎样」✓）。
 *   · 链：电弹贴近敌（`PS_CHAIN_TRIG_R` ✓）时触发 ✓，逐跳传播 ✓；**弧 VM 按 (敌A,敌B) 无序对去重** ✓
 *     （owner 第 3 点 ✓：同对**只刷新不新建** ✓，表满**宁可不建** ✓）。
 *   · **麻痹不做** ✓（owner：「2、不要麻痹」✓ —— 一行都没写 ✓）。
 *   · **0 条新增站点** ✓（`ce_owned` / `ce_anm_*` / `ce_damage_rect` 全是现成的 ✓，无新 BP ✓）。*/
#define PS_ELEC_CARD      71
#define PS_ELEC_RGB       0x00a0d0ffu  /* 电弹/电弧的 RGB（电光蓝 ✓）*/
#define PS_ELEC_RGBA      0xffa0d0ffu  /* ★ P49：**带 alpha** 的整色（0xff = 不透明 ✓）—— 直接 spawn 的 VM 必须自己写 alpha ✗ */
#define PS_BALL_SCALE     0.6f         /* ❓自定：电球缩放（71 的 orb 是子机大小 ✓，缩到弹那样 ✓）*/
#define PS_OPT_LAYER      13           /* anm 层（与 71 同一层 ✓）*/
#define PS_CHAIN_HOPS      4           /* ❓自定：链最多跳几段（含第一跳 ✓）*/
#define PS_CHAIN_RADIUS 150.0f         /* ❓自定：每跳半径（把 PvZ 的"上下行"换成 360° 半径 ✓）*/
#define PS_CHAIN_DMG       1           /* ❓自定（P48 改）：**每帧**每跳的伤害 ✓ —— owner「电链是一帧一下的」✓
                                        *   一帧一下 ⇒ 单次数必须极小 ✓；电弹 7 px/帧 ⇒ 经过一只敌约 7 帧 ⇒ 4 跳 ≈ 28 伤害 ✓ 自限 ✓。*/
#define PS_CHAIN_W      32.0f          /* 判定框 = 与 71 同口径（BC_HIT_W/H ✓）*/
#define PS_CHAIN_H      32.0f
#define PS_CHAIN_LIFE      2           /* 帧：与 71 的 BC_HIT_LIFE 一致 ✓ */
#define PS_CHAIN_CD       12           /* ❓自定：链的节流（开大时 140 颗命中很多 ⇒ 没节流会失控 ✓）*/
#define PS_CHAIN_SE     0x46           /* 与 71 同一个"电流"音效（BC_SE ✓）*/
#define PS_BEAM_TEX_W  256.0f          /* 71 的电弧贴图宽（scale.x = 距离 / 它 ✓）*/
#define PS_CHAIN_TRIG_R 26.0f          /* ❓自定：电弹离敌多近算"到了身上"（≈ 半个豌豆判定 ✓）*/
#define PS_ARC_MAX        32           /* ❓自定：弧表容量（按敌人对去重 ✓，表满宁可不建 ✓）*/
/* ★★ P44（owner 逐字）：「**大招也得是电版，但就做发一堆普通弹幕即可**」✓ 随后澄清：「**发一堆的普攻电球**」✓
 *   ⇒ 开大时不搞新花样 ✗：就是**把普攻那颗电球一次发一堆** ✓（同 `PE_SET_NORMAL` 的速度/伤害 ✓，横向铺开 ✓）。
 *   ❓自定（等 owner 调）：一轮 `PS_BALL_COLS` 颗、开大期间每 `PS_FAN_EVERY` 帧一轮 ✓；
 *   ⚠ 一颗 = 90 伤害 ✓ ⇒ 一轮 7 颗同帧 = 630 ✗（比开大那排 7 颗小豌豆的 49 大得多 ✓，owner 要调就调这里 ✓）。*/
#define PS_FAN_N          13           /* ❓自定：扇形里几颗普攻电球 ✓（奇数 ⇒ 正中一颗朝正上方 ✓）*/
#define PS_FAN_HALF_DEG 50.0f          /* ❓自定：扇形半角（度 ✓；±50° 的扇面 ✓）*/
#define PS_FAN_SPEED     7.0f          /* ★ P47 = 普攻速度的**一半**（PE_SPD_NORMAL 14 / PE_ELEC_SLOW 2 ✓）*/
#define PS_FAN_EVERY      20           /* ❓自定：开大期间每几帧来一扇 ✓（100 帧的 burst ⇒ 5 扇 ✓）*/

/* 卡侧电弹的状态（纯逻辑在 `peashooter_core` ✓，这里只放引擎侧的那半：VM id ✓）*/
static pe_proj_t s_proj[PE_MAX];
static uint32_t  s_proj_vm[PE_MAX];
static uint32_t  s_arc_keys[PS_ARC_MAX];   /* (敌A,敌B) 无序对 ✓ */
static uint32_t  s_arc_vms[PS_ARC_MAX];    /* 那一对对应的电弧 VM ✓ */
static uint8_t   s_arc_alive[PS_ARC_MAX];  /* 本帧该 VM 还活着吗（每帧刷 ✓）*/
static uint32_t  s_chain_cd;
static int       s_ball_last;   /* P44：上一扇"大招电球"是在哪个射击帧 ✓ */
static int       s_log_fan = 1;  /* P44：第一扇打一行日志（省得每次刷屏 ✓；clear 时复位 ✓）*/
static int       s_gate_log = -1;   /* P46：门状态变化时打一行（-1 = 还没打过 ✓）*/
static uint32_t  s_spawn_n, s_respawn_n, s_fan_n;   /* P46：诊断计数（起弹 / 补 VM / 大招扇数 ✓）*/
static unsigned  g_frames_ps_diag;                  /* P46：诊断限频 ✓ */
static uint32_t  s_src_fail;                        /* P48：伤害源没抢到的次数（池爆报警 ✓）*/

static int ps_elec_on(void) { return ce_owned(PS_ELEC_CARD) ? 1 : 0; }   /* 同装判据 ✓（每帧现算 ✓）*/
static int ps_proj_live(void)   /* P46：现在有几颗弹在飞 ✓ */
{
    int i, n = 0;
    for (i = 0; i < PE_MAX; ++i) if (s_proj[i].alive) ++n;
    return n;
}

/* 收干净：弹 + 弹的 VM + 弧 + 弧的 VM + 节流（删卡 / 局末都走这里 ✓）。*/
static void ps_elec_clear(void)
{
    int i;
    for (i = 0; i < PE_MAX; ++i) {
        if (s_proj_vm[i] && ce_anm_get_vm(s_proj_vm[i])) ce_anm_delete(s_proj_vm[i]);
        s_proj_vm[i] = 0;
    }
    pe_proj_clear_all(s_proj, PE_MAX);
    for (i = 0; i < PS_ARC_MAX; ++i) {
        if (s_arc_vms[i] && ce_anm_get_vm(s_arc_vms[i])) ce_anm_delete(s_arc_vms[i]);
        s_arc_keys[i] = 0; s_arc_vms[i] = 0; s_arc_alive[i] = 0;
    }
    s_chain_cd = 0;
    s_ball_last = -1000000;   /* P44：起手那一下必发 ⇒ 用一个很远的初值 ✓ */
    s_log_fan = 1;
    s_spawn_n = s_respawn_n = s_fan_n = 0;
    s_src_fail = 0;
    g_frames_ps_diag = 0;
}

/* ★ P42 ③：一对敌（A,B）**只建一个**电弧 VM ✓；同对再链**只刷新** ✓；表满且无同对 ⇒ 宁可不建 ✓。*/
static void ps_arc_show(uint32_t ida, uint32_t idb, float px, float py, float tx, float ty, float pz)
{
    uint32_t key = pea_arc_key(ida, idb);
    int slot = pea_arc_find(s_arc_keys, PS_ARC_MAX, key);
    uint32_t vm = (slot >= 0) ? s_arc_vms[slot] : 0;
    float dx = tx - px, dy = ty - py;
    float dist = __builtin_sqrtf(dx * dx + dy * dy);
    float ang = bc_atan2f(dy, dx);
    float sx = dist * (1.0f / PS_BEAM_TEX_W);
    if (vm && ce_anm_get_vm(vm)) {           /* 同对已有活着的弧 ⇒ 只刷新 ✓ */
        ce_anm_set_pos(vm, px, py, pz);
        ce_anm_set_rotation(vm, 0.0f, 0.0f, ang);
        ce_anm_set_scale(vm, sx, 1.0f);
        ce_anm_set_fvar(vm, 0, sx);
        return;
    }
    if (slot < 0) {                           /* 没有同对的槽 ⇒ 先预定一个空槽 ✓ */
        slot = pea_arc_put(s_arc_keys, s_arc_vms, s_arc_alive, PS_ARC_MAX, key, 0);
        if (slot < 0) return;                 /* ★ 表满且无同对 ⇒ 宁可不建 ✓（owner 第 3 点 ✓）*/
    }
    vm = ce_anm_spawn(CE_ABILITY_ANM(), CE_ANM_ABILITY_SCRIPT_BROKEN_CORE_BEAM, PS_OPT_LAYER);
    if (!vm) return;
    ce_anm_set_pos(vm, px, py, pz);
    ce_anm_set_rotation(vm, 0.0f, 0.0f, ang);
    ce_anm_set_scale(vm, sx, 1.0f);
    ce_anm_set_fvar(vm, 0, sx);
    s_arc_keys[slot] = key; s_arc_vms[slot] = vm; s_arc_alive[slot] = 1;
}

/* 从 `from_id` 这只敌出发放一条闪电链 ✓（owner：「连接自身一个半径范围内的敌人，敌人还会再连接敌人」✓）。
 *   ⚠ 拿不到敌指针（`on_bullet_hit` 的 `enemy` 恒 0 ✗；这里本来就只有坐标 + id ✓）⇒ 只用坐标 + id ✓。*/
static void ps_chain_from(uint32_t from_id, float px, float py, float pz)
{
    float cand[2 * PE_MAX];
    uint32_t cid[PE_MAX], visited[PS_CHAIN_HOPS + 2];
    uint8_t *em, *e;
    int n = 0, nv = 0, hop;
    /* ★ P48（owner：电链是一帧一下的）⇒ **没有节流** ✓：贴着敌的每一帧都放 ✓（弧按对去重负责不重复建 VM ✓）。*/
    em = CE_ENEMY_MGR();
    if (em) {
        CE_ENEMY_FOREACH(em, e) {
            const float *pp;
            uint32_t id;
            if (!ce_enemy_lockable(e)) continue;
            if (n >= PE_MAX) break;
            pp = ce_enemy_pos(e);
            id = *(const uint32_t *)(e + CE_ENEMY_ID);
            if (id == 0 || (from_id && id == from_id)) continue;   /* 贴上那只不吃链（它吃弹的伤害 ✓）*/
            cand[2 * n] = pp[0]; cand[2 * n + 1] = pp[1]; cid[n] = id; ++n;
        }
    }
    if (n <= 0) return;
    visited[nv++] = from_id;
    for (hop = 0; hop < PS_CHAIN_HOPS; ++hop) {
        int k = pea_chain_pick(cand, cid, n, px, py, PS_CHAIN_RADIUS, visited, nv);
        float tx, ty;
        uint32_t spark;
        if (k < 0) break;
        tx = cand[2 * k]; ty = cand[2 * k + 1];
        {
            float center[3] = { tx, ty, pz };
            /* ★ P48：一帧一下 ⇒ 每帧都在发源 ✓ ⇒ 记下"源没抢到"的次数（池爆就会猛涨 ✗）*/
            if (ce_damage_rect(center, 0.0f, PS_CHAIN_LIFE, PS_CHAIN_DMG, PS_CHAIN_W, PS_CHAIN_H) <= 0)
                ++s_src_fail;
        }
        ps_arc_show(nv ? visited[nv - 1] : from_id, cid[k], px, py, tx, ty, pz);   /* ★ 按对去重 ✓ */
        spark = ce_anm_spawn(CE_ABILITY_ANM(), CE_ANM_ABILITY_SCRIPT_BROKEN_CORE_SPARK, PS_OPT_LAYER);
        if (spark) ce_anm_set_pos(spark, tx, ty, pz);
        ce_play_sound(PS_CHAIN_SE, tx);
        visited[nv++] = cid[k];
        px = tx; py = ty;                          /* ★ 下一跳从这里出发 ⇒「敌人还会再连接敌人」✓ */
        if (nv >= (int)(sizeof visited / sizeof visited[0])) break;
    }
    if (nv > 1) {
        s_chain_cd = PS_CHAIN_CD;
        ce_log("peashooter: 电大哥 chain %d 跳（起自 id %u）", nv - 1, from_id);
    }
}

/* 起一组卡侧电弹（正常 1 颗 / 开大 7 颗横向铺开 ±48 ✓ —— 与 SHT 的 spawn_dx 同铺法 ✓）。*/
static void ps_proj_fire(void *o, int set)
{
    float ox = ce_option_x(o), oy = ce_option_y(o), pz = ce_player_pos()[2];
    int gat = (set == PEA_SET_GATLING);
    int i, n = gat ? PEA_GAT_ROWS : 1;
    for (i = 0; i < n; ++i) {
        float dx = (n == 1) ? 0.0f : (-48.0f + 16.0f * (float)i);
        int k = pe_proj_new(s_proj, PE_MAX, ox + dx, oy, gat ? PE_SET_GATLING : PE_SET_NORMAL);
        uint32_t vm;
        if (k < 0) return;                          /* 满了 ⇒ 少几颗（不炸 ✓）*/
        /* ★ P49：用 71 那套**已验证可见**的电球脚本 ✓（原来的子弹脚本直接 spawn 会画不出来 ✗）*/
        vm = ce_anm_spawn(CE_ABILITY_ANM(), CE_ANM_ABILITY_SCRIPT_BROKEN_CORE_ORB, PS_OPT_LAYER);
        s_proj_vm[k] = vm;
        if (vm) {
            ce_anm_set_pos(vm, s_proj[k].x, s_proj[k].y, pz);
            ce_anm_set_color_rgb(vm, PS_ELEC_RGB);  /* 电光色 ✓（P14a 抽出来的词汇 ✓）*/
        }
    }
}

/* ★ P44 扇形方向表（**离线算好的常量** ✓ —— 现场不调 libm ✓，x87 闸干净 ✓、replay 确定 ✓）。
 *   角度 a 是从**正上方**往右为正 ✓；vx/vy 是屏幕坐标（y 向下 ✓）。
 *   ⚠ 改 `PS_FAN_N` / `PS_FAN_HALF_DEG` / `PS_FAN_SPEED` 时，这张表要**重新生成** ✗。*/
static const struct { float vx, vy, ang; } s_fan[PS_FAN_N] = {
    {    -5.3623f,    -4.4995f,    -0.8727f },
    {    -4.6536f,    -5.2292f,    -0.7272f },
    {    -3.8466f,    -5.8484f,    -0.5818f },
    {    -2.9583f,    -6.3442f,    -0.4363f },
    {    -2.0076f,    -6.7059f,    -0.2909f },
    {    -1.0145f,    -6.9261f,    -0.1454f },
    {     0.0000f,    -7.0000f,     0.0000f },
    {     1.0145f,    -6.9261f,     0.1454f },
    {     2.0076f,    -6.7059f,     0.2909f },
    {     2.9583f,    -6.3442f,     0.4363f },
    {     3.8466f,    -5.8484f,     0.5818f },
    {     4.6536f,    -5.2292f,     0.7272f },
    {     5.3623f,    -4.4995f,     0.8727f },
};

/* ★ P44：大招 = **发一扇普攻电球** ✓（owner：「**发一堆的普攻电球**」+「**得是扇形**」✓）。
 *   就是普攻那颗（速度 14 / 伤害 90 ✓），按 `s_fan` 的方向往扇面里铺 ✓；
 *   不做"巨球分裂小球" ✗、不做横向一排 ✗。*/
static void ps_proj_fan(void *o)
{
    float ox = ce_option_x(o), oy = ce_option_y(o), pz = ce_player_pos()[2];
    int i;
    for (i = 0; i < PS_FAN_N; ++i) {
        int k = pe_proj_spawn(s_proj, PE_MAX, ox, oy, s_fan[i].vx, s_fan[i].vy,
                              PE_SET_NORMAL, PE_DMG_NORMAL);      /* 普攻那颗 ✓ */
        uint32_t vm;
        if (k < 0) return;
        vm = ce_anm_spawn(CE_ABILITY_ANM(), CE_ANM_ABILITY_SCRIPT_BROKEN_CORE_ORB, PS_OPT_LAYER);   /* ★ P49 电球 ✓ */
        s_proj_vm[k] = vm;
        if (vm) {
            ce_anm_set_pos(vm, s_proj[k].x, s_proj[k].y, pz);
            ce_anm_set_rotation(vm, 0.0f, 0.0f, s_fan[i].ang);   /* 贴图朝上 ⇒ 转 a 就是扇面方向 ✓ */
            ce_anm_set_scale(vm, PS_BALL_SCALE, PS_BALL_SCALE);  /* ★ P49 */
            *(volatile uint32_t *)(ce_anm_get_vm(vm) + CE_ANM_VM_COLOR1) = PS_ELEC_RGBA;   /* ★ P49 带 alpha ✓ */
        }
    }
    ++s_fan_n;
    if (s_log_fan) { s_log_fan = 0; ce_log("peashooter: 电大哥 大招 —— %d 颗普攻电球 扇形 ±%.0f°（共已 %u 扇）", PS_FAN_N, PS_FAN_HALF_DEG, s_fan_n); }
}

static void ps_proj_kill(int i)
{
    if (s_proj_vm[i] && ce_anm_get_vm(s_proj_vm[i])) ce_anm_delete(s_proj_vm[i]);
    s_proj_vm[i] = 0;
    s_proj[i].alive = 0;
}

/* 每帧：刷弧存活 ✓ → 弹挪/画 ✓ → 出界收 ✓ → 定点伤害源 ✓ → 贴近敌触发链 ✓。*/
static void ps_proj_tick(void)
{
    float pz = ce_player_pos()[2];
    int a, i;
    for (a = 0; a < PS_ARC_MAX; ++a)
        s_arc_alive[a] = (s_arc_vms[a] && ce_anm_get_vm(s_arc_vms[a])) ? 1 : 0;
    if (s_chain_cd) --s_chain_cd;
    {   /* ★ P46 诊断：① 门状态一变就打一行 ⇒ 从日志就能看出协同开没开 ✓ */
        int on = ps_elec_on();
        if (on != s_gate_log) {
            s_gate_log = on;
            ce_log("peashooter: 电大哥 门 = %s（ce_owned(71)=%d）", on ? "开" : "关", on);
        }
        /* ② 约每 5 秒一行：在飞弹 / 已起弹 / 补过几次 VM / 大招扇数 ✓ */
        if (on && (g_frames_ps_diag++ % 300u) == 0u) {
            /* ★ P49：顺手把**第一颗在飞弹的 VM** 的颜色/缩放读回来 ✓ —— 隐形时这里会立刻现形 ✓ */
            uint32_t vc = 0; float vs = 0.0f; int vi;
            for (vi = 0; vi < PE_MAX; ++vi) {
                if (s_proj[vi].alive && s_proj_vm[vi]) {
                    uint8_t *vm = ce_anm_get_vm(s_proj_vm[vi]);
                    if (vm) {
                        vc = *(volatile uint32_t *)(vm + CE_ANM_VM_COLOR1);
                        vs = *(const float *)(vm + CE_ANM_VM_SCALE);
                    }
                    break;
                }
            }
            ce_log("peashooter: 电大哥 诊断 在飞弹=%d 已起=%u 补VM=%u 大招扇=%u 源失败=%u VM色=%08x VM缩放=%.2f",
                   ps_proj_live(), s_spawn_n, s_respawn_n, s_fan_n, s_src_fail, vc, vs);
        }
    }
    for (i = 0; i < PE_MAX; ++i) {
        pe_proj_t *p = &s_proj[i];
        if (!p->alive) continue;
        pe_proj_move(p);
        if (!s_proj_vm[i] || !ce_anm_get_vm(s_proj_vm[i])) {   /* ★ P46：VM 没了就补一个（否则弹隐形 ✗）*/
            s_proj_vm[i] = ce_anm_spawn(CE_ABILITY_ANM(),
                                        (p->set == PE_SET_GATLING) ? CE_ANM_ABILITY_SCRIPT_PEA_SMALL
                                                                   : CE_ANM_ABILITY_SCRIPT_PEA, PS_OPT_LAYER);
            ++s_respawn_n;
        }
        if (s_proj_vm[i] && ce_anm_get_vm(s_proj_vm[i])) {
            ce_anm_set_pos(s_proj_vm[i], p->x, p->y, pz);
            ce_anm_set_scale(s_proj_vm[i], PS_BALL_SCALE, PS_BALL_SCALE);
            *(volatile uint32_t *)(ce_anm_get_vm(s_proj_vm[i]) + CE_ANM_VM_COLOR1) = PS_ELEC_RGBA;
        }
        if (pe_proj_out(p)) { ps_proj_kill(i); continue; }
        if (pe_proj_hit_now(p)) {                   /* 定点伤害源（与 89 同口径 ✓，命中不销毁 ✓）*/
            float center[3] = { p->x, p->y, pz };
            float w = (p->set == PE_SET_GATLING) ? PE_HIT_W_GATLING : PE_HIT_W_NORMAL;
            ce_damage_rect(center, 0.0f, PS_CHAIN_LIFE, p->dmg, w, w);
            pe_proj_fired(p);
        }
        {   /* 贴近敌 ⇒ 触发链（链自己带节流 ✓）*/
            uint8_t *em = CE_ENEMY_MGR(), *e;
            if (em) {
                CE_ENEMY_FOREACH(em, e) {
                    const float *pp;
                    float dx, dy;
                    uint32_t id;
                    if (!ce_enemy_lockable(e)) continue;
                    pp = ce_enemy_pos(e);
                    dx = pp[0] - p->x; dy = pp[1] - p->y;
                    if (dx * dx + dy * dy > PS_CHAIN_TRIG_R * PS_CHAIN_TRIG_R) continue;
                    id = *(const uint32_t *)(e + CE_ENEMY_ID);
                    if (id == 0) continue;
                    ps_chain_from(id, p->x, p->y, pz);
                    break;                          /* 一颗弹一帧最多起一条链 ✓ */
                }
            }
        }
    }
}

/* ── 本卡自己的子机槽 + 纯逻辑状态（P43 重写时被误删过 ✗，这里补回 ✓）── */
#define PS_OPTION_OFFSET  0x28      /* 子机相对自机的横向偏移；破损核心用 0x18，错开以便两张同时装 */
#define PS_RETRY_FRAMES   60        /* 没拿到子机时的重试间隔（槽满 / 关卡中途买到）*/
#define PS_SE_BURST       0x0d      /* se_powerup：开大起手那一下 */
#define PS_BURST_LAYER    15        /* script103 自己写的层，spawn 时给同一个 */

typedef struct {
    ce_option_t opt;                /* 子机槽（SDK：认领 / 申请 / 收起）*/
    pea_state_t st;
} ps_card_t;

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
    ps_proj_tick();
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
    if (ps_elec_on()) {
        /* ★ P43：同装 71 ⇒ 走**卡侧电弹**（真穿透 ✓），**不发引擎豌豆** ✗。
         * 发射判定照 SHT 那条：`timer % fire_rate == start_delay`（两组 start_delay 都是 0 ✓）。*/
        int rate = (out.set == PEA_SET_GATLING) ? PEA_GAT_INTERVAL : PEA_INTERVAL;
        if (rate > 0 && ((int)out.timer % rate) == 0) ps_proj_fire(o, out.set);
        /* ★ P44：大招 —— 起手必来一轮，开大期间再每 PS_FAN_EVERY 帧来一轮（都是普攻电球 ✓）*/
        if (out.burst_start) { ps_proj_fan(o); s_ball_last = (int)out.timer; }
        else if (out.set == PEA_SET_GATLING && (int)out.timer - s_ball_last >= PS_FAN_EVERY) {
            ps_proj_fan(o);
            s_ball_last = (int)out.timer;
        }
    } else {
        ce_tick_shooters_for_card(o, (int)out.timer, 0,
                                  out.set == PEA_SET_GATLING ? CE_SHT_SET_PEA_GATLING : CE_SHT_SET_PEA_NORMAL);
    }

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
            .on_tick_2 = on_tick_2, .on_load = on_load, .on_run_reset = on_run_reset,
            .on_delete = on_delete);
