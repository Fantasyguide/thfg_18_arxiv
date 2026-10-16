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
#define PS_BALL_SCALE    0.22f   /* ★ P50（owner：怎么这么大）0.6 → 0.22 ✓ */         /* ❓自定：电球缩放（71 的 orb 是子机大小 ✓，缩到弹那样 ✓）*/
#define PS_OPT_LAYER      13           /* anm 层（与 71 同一层 ✓）*/
#define PS_CHAIN_HOPS      3           /* ★ P52（owner：一个电球可以连三个人）4 → 3 ✓ */
#define PS_BURST_INVULN    10          /* ★ P54（owner：爆发期间本体无敌 ✓，按 PvZ2 一手）
                                        *   每帧向 SDK **声明** 10 帧无敌 ✓（声明层取最大值 ✓，见 sdk.h ✓）*/
#define PS_RAND_EVERY       5          /* ★ P54：爆发期间每几帧来一发"随机方向"的弹 ✓ */
#define PS_BALL_LINKS      3           /* ★ P52：一个电球最多对**三只敌**各拉一条球→敌的弧 ✓ */
#define PS_CHAIN_RADIUS 150.0f         /* ❓自定：每跳半径（把 PvZ 的"上下行"换成 360° 半径 ✓）*/
#define PS_CHAIN_DMG       2           /* ❓自定（P48 改）：**每帧**每跳的伤害 ✓ —— owner「电链是一帧一下的」✓
                                        *   一帧一下 ⇒ 单次数必须极小 ✓；电弹 7 px/帧 ⇒ 经过一只敌约 7 帧 ⇒ 4 跳 ≈ 28 伤害 ✓ 自限 ✓。*/
#define PS_CHAIN_W      32.0f          /* 判定框 = 与 71 同口径（BC_HIT_W/H ✓）*/
#define PS_CHAIN_H      32.0f
#define PS_CHAIN_LIFE      2           /* 帧：与 71 的 BC_HIT_LIFE 一致 ✓ */
#define PS_CHAIN_CD       12           /* ❓自定：链的节流（开大时 140 颗命中很多 ⇒ 没节流会失控 ✓）*/
#define PS_CHAIN_SE     0x46           /* 与 71 同一个"电流"音效（BC_SE ✓）*/
#define PS_BEAM_TEX_W  256.0f          /* 71 的电弧贴图宽（scale.x = 距离 / 它 ✓）*/
#define PS_CHAIN_TRIG_R 72.0f          /* ★ P50（owner：触发半径再大一些）26 → 72 px ✓ —— 弧长≈它，26 时短到看不见 ✓ */
#define PS_ARC_MAX        32           /* ❓自定：弧表容量（按敌人对去重 ✓，表满宁可不建 ✓）*/
/* ★★★ 2026-10-15（owner 裁定：P44 的"扇形"**整组删除** ✗）：
 *   owner 逐字：「**我是要用 (a) 替换 (b)**」✓「**一手的大招只有一种发弹：向前一个角度范围内的大量随机角度发弹**」✓
 *   ⇒ 大招**只有一种发弹** ✓ = 那条**机枪连发**（`ps_proj_fire` ✓；同装 71 时走卡侧电弹 ✓），
 *     其中**每一颗的朝向**由 `s_rand_spread`（**前方 ±20° 连续随机** ✓）各抽一次 ✓
 *     —— 这正是一手的「向前一个角度范围内的大量随机角度发弹」✓。
 *   ⇒ 原 P44 的"一扇 13 颗普攻电球"（`PS_FAN_N` / `PS_FAN_HALF_DEG` / `PS_FAN_SPEED` / `PS_FAN_EVERY`
 *     ＋ `s_fan[]` ＋ `ps_proj_fan()` ＋ `s_ball_last` / `s_log_fan` / `s_fan_n`）**整组删除** ✗：
 *     它把弹铺到 **±50°** ✗ ⇒ 与"±20° 区间内随机"自相矛盾 ✓，而且一手根本没有第二种发弹 ✗。
 *   ✅ 数量不亏 ✓：机枪那条**单独**就是 `PEA_GAT_FRAMES / PEA_GAT_INTERVAL × PEA_GAT_ROWS`
 *     = 240 / 5 × 7 = **336 颗/次** ✓（卡面那句"一百多颗"是 P54 把时长拉到 240 帧**之前**的旧话 ✓，
 *     属玩家可见文案 ⇒ 另行按 owner 口径处理 ✗）。*/

/* 卡侧电弹的状态（纯逻辑在 `peashooter_core` ✓，这里只放引擎侧的那半：VM id ✓）*/
static pe_proj_t s_proj[PE_MAX];
static uint32_t  s_proj_vm[PE_MAX];
static uint32_t  s_arc_keys[PS_ARC_MAX];   /* (敌A,敌B) 无序对 ✓ */
static uint32_t  s_arc_vms[PS_ARC_MAX];    /* 那一对对应的电弧 VM ✓ */
static uint8_t   s_arc_alive[PS_ARC_MAX];  /* 本帧该 VM 还活着吗（每帧刷 ✓）*/
static uint32_t  s_chain_cd;
static int       s_gate_log = -1;   /* P46：门状态变化时打一行（-1 = 还没打过 ✓）*/
static uint32_t  s_spawn_n, s_respawn_n;            /* P46：诊断计数（起弹 / 补 VM ✓）*/
static unsigned  g_frames_ps_diag;                  /* P46：诊断限频 ✓ */
/* ★★★ 2026-10-15（owner 两条口径 ✓，**改掉 P54 的整圈表** ✗）：
 *   · owner：「**植物大战僵尸里面的电大哥是向前方一个角度范围内随机散射子弹，你目前做成了全向散射**」✓
 *   · owner：「**±20° 之间完全随机**」✓
 *   ⇒ 这张表 = **前方扇面 ±20°、1° 步长共 41 项** ✓（基准 = **正上方** ✓，与 77 的普攻同向 ✓）；
 *     出膛时在表内**均匀抽一项**（`ce_rand() % PS_SPREAD_N` ✓ ⇒ 完全随机、且 replay 确定 ✓）。
 *   ⚠ 卡侧**禁 libm** ✗（本仓不变式）⇒ 常量**离线算好** ✓：生成脚本
 *     `native/tools/gen_pea_spread_table.py` ✓（同 `yk_orbit_sin/cos` 那张 120 项表的先例 ✓）。
 *   字段含义（沿用旧表 ✓）：`vx/vy` = 单位方向（屏幕坐标 ✓，y 向上为负 ✓）；
 *     `spread` = 贴图该转多少（贴图朝上 ⇒ 就是 θ 本身 ✓）；`abs` = 弹的朝向角（θ − π/2 ✓）。*/
#define PS_SPREAD_N       41           /* ★ 2026-10-15：±20° / 1° 步长 ✓（原 16 项整圈表已删 ✗）*/
static const struct { float vx, vy, spread, abs; } s_rand_spread[PS_SPREAD_N] = {
    {    -0.3420f,    -0.9397f,    -0.3491f,    -1.9199f },   /* -20° */
    {    -0.3256f,    -0.9455f,    -0.3316f,    -1.9024f },   /* -19° */
    {    -0.3090f,    -0.9511f,    -0.3142f,    -1.8850f },   /* -18° */
    {    -0.2924f,    -0.9563f,    -0.2967f,    -1.8675f },   /* -17° */
    {    -0.2756f,    -0.9613f,    -0.2793f,    -1.8500f },   /* -16° */
    {    -0.2588f,    -0.9659f,    -0.2618f,    -1.8326f },   /* -15° */
    {    -0.2419f,    -0.9703f,    -0.2443f,    -1.8151f },   /* -14° */
    {    -0.2250f,    -0.9744f,    -0.2269f,    -1.7977f },   /* -13° */
    {    -0.2079f,    -0.9781f,    -0.2094f,    -1.7802f },   /* -12° */
    {    -0.1908f,    -0.9816f,    -0.1920f,    -1.7628f },   /* -11° */
    {    -0.1736f,    -0.9848f,    -0.1745f,    -1.7453f },   /* -10° */
    {    -0.1564f,    -0.9877f,    -0.1571f,    -1.7279f },   /*  -9° */
    {    -0.1392f,    -0.9903f,    -0.1396f,    -1.7104f },   /*  -8° */
    {    -0.1219f,    -0.9925f,    -0.1222f,    -1.6930f },   /*  -7° */
    {    -0.1045f,    -0.9945f,    -0.1047f,    -1.6755f },   /*  -6° */
    {    -0.0872f,    -0.9962f,    -0.0873f,    -1.6581f },   /*  -5° */
    {    -0.0698f,    -0.9976f,    -0.0698f,    -1.6406f },   /*  -4° */
    {    -0.0523f,    -0.9986f,    -0.0524f,    -1.6232f },   /*  -3° */
    {    -0.0349f,    -0.9994f,    -0.0349f,    -1.6057f },   /*  -2° */
    {    -0.0175f,    -0.9998f,    -0.0175f,    -1.5882f },   /*  -1° */
    {     0.0000f,    -1.0000f,     0.0000f,    -1.5708f },   /*  +0° */
    {     0.0175f,    -0.9998f,     0.0175f,    -1.5533f },   /*  +1° */
    {     0.0349f,    -0.9994f,     0.0349f,    -1.5359f },   /*  +2° */
    {     0.0523f,    -0.9986f,     0.0524f,    -1.5184f },   /*  +3° */
    {     0.0698f,    -0.9976f,     0.0698f,    -1.5010f },   /*  +4° */
    {     0.0872f,    -0.9962f,     0.0873f,    -1.4835f },   /*  +5° */
    {     0.1045f,    -0.9945f,     0.1047f,    -1.4661f },   /*  +6° */
    {     0.1219f,    -0.9925f,     0.1222f,    -1.4486f },   /*  +7° */
    {     0.1392f,    -0.9903f,     0.1396f,    -1.4312f },   /*  +8° */
    {     0.1564f,    -0.9877f,     0.1571f,    -1.4137f },   /*  +9° */
    {     0.1736f,    -0.9848f,     0.1745f,    -1.3963f },   /* +10° */
    {     0.1908f,    -0.9816f,     0.1920f,    -1.3788f },   /* +11° */
    {     0.2079f,    -0.9781f,     0.2094f,    -1.3614f },   /* +12° */
    {     0.2250f,    -0.9744f,     0.2269f,    -1.3439f },   /* +13° */
    {     0.2419f,    -0.9703f,     0.2443f,    -1.3265f },   /* +14° */
    {     0.2588f,    -0.9659f,     0.2618f,    -1.3090f },   /* +15° */
    {     0.2756f,    -0.9613f,     0.2793f,    -1.2915f },   /* +16° */
    {     0.2924f,    -0.9563f,     0.2967f,    -1.2741f },   /* +17° */
    {     0.3090f,    -0.9511f,     0.3142f,    -1.2566f },   /* +18° */
    {     0.3256f,    -0.9455f,     0.3316f,    -1.2392f },   /* +19° */
    {     0.3420f,    -0.9397f,     0.3491f,    -1.2217f },   /* +20° */
};

static int       s_log_arc = 1;   /* P50：首条电弧打一行诊断 ✓ */
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
    s_spawn_n = s_respawn_n = 0;
    s_src_fail = 0;
    s_log_arc = 1;
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
    if (s_log_arc) { s_log_arc = 0; ce_log("peashooter: 电大哥 首条电弧 dist=%.1f scale.x=%.3f vm=%u 层=%d", dist, sx, vm, PS_OPT_LAYER); }
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
    /* ★ P52：visited 里**只放"上一只"** ✓（原来放所有连过的 ⇒ 禁止回头 ✗）
     *   每跳开始时把上一只的 id 写进 visited[0] ⇒ A→B→A 这种返程允许 ✓。*/
    for (hop = 0; hop < PS_CHAIN_HOPS; ++hop) {
        visited[0] = (nv > 0) ? visited[nv - 1] : from_id;
        nv = 1;
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
    float spd = (gat ? PE_SPD_GATLING : PE_SPD_NORMAL) / PE_ELEC_SLOW;   /* ★ P47/P50：电弹半速再半速 = 1/4 ✓ */
    for (i = 0; i < n; ++i) {
        float dx = (n == 1) ? 0.0f : (-48.0f + 16.0f * (float)i);
        int rd = gat ? (int)(ce_rand() % PS_SPREAD_N) : 0;      /* ★ P54：**开大**才随机方向 ✓（普攻仍朝正上方 ✓）*/
        int k;
        uint8_t *vm;
        k = gat ? pe_proj_spawn(s_proj, PE_MAX, ox + dx, oy, s_rand_spread[rd].vx * spd, s_rand_spread[rd].vy * spd,
                                PE_SET_GATLING, PE_DMG_GATLING)
                : pe_proj_new(s_proj, PE_MAX, ox + dx, oy, PE_SET_NORMAL);
        if (k < 0) return;                          /* 满了 ⇒ 少几颗（不炸 ✓）*/
        /* ★ P49/P50：用 71 那套**已验证可见**的电球脚本 ✓；★ P50/P54：**这里**必须也缩小 + 上蓝 ✓
         *   （原来只改了大招扇形与补建路径 ✗ ⇒ owner 实跑「太大、不蓝」✓ 就是这一处漏了 ✗）。*/
        s_proj_vm[k] = ce_anm_spawn(CE_ABILITY_ANM(), CE_ANM_ABILITY_SCRIPT_BROKEN_CORE_ORB, PS_OPT_LAYER);
        vm = s_proj_vm[k] ? ce_anm_get_vm(s_proj_vm[k]) : 0;
        if (vm) {
            ce_anm_set_pos(s_proj_vm[k], s_proj[k].x, s_proj[k].y, pz);
            ce_anm_set_scale(s_proj_vm[k], PS_BALL_SCALE, PS_BALL_SCALE);
            if (gat) ce_anm_set_rotation(s_proj_vm[k], 0.0f, 0.0f, s_rand_spread[rd].spread);
            *(volatile uint32_t *)(vm + CE_ANM_VM_COLOR1) = PS_ELEC_RGBA;
            *(volatile uint32_t *)(vm + CE_ANM_VM_COLOR2) = PS_ELEC_RGBA;
        }
        ++s_spawn_n;
    }
}

/* ★ 2026-10-15：原 P44 的 `s_fan[PS_FAN_N]` 扇形方向表 + `ps_proj_fan()` **整组删除** ✗
 *   —— owner 裁定「**用 (a) 替换 (b)**」✓「**一手的大招只有一种发弹**」✓，理由与出处见文件
 *   上方 `PS_SPREAD_N` 前那段注释 ✓。开大期间的发弹现在**只有** `ps_proj_fire()` 一条 ✓。*/

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
            ce_log("peashooter: 电大哥 诊断 在飞弹=%d 已起=%u 补VM=%u 源失败=%u VM色=%08x VM缩放=%.2f",
                   ps_proj_live(), s_spawn_n, s_respawn_n, s_src_fail, vc, vs);
        }
    }
    for (i = 0; i < PE_MAX; ++i) {
        pe_proj_t *p = &s_proj[i];
        if (!p->alive) continue;
        pe_proj_move(p);
        if (!s_proj_vm[i] || !ce_anm_get_vm(s_proj_vm[i])) {   /* ★ P46：VM 没了就补一个（否则弹隐形 ✗）*/
            s_proj_vm[i] = ce_anm_spawn(CE_ABILITY_ANM(), CE_ANM_ABILITY_SCRIPT_BROKEN_CORE_ORB, PS_OPT_LAYER);
            ++s_respawn_n;
        }
        if (s_proj_vm[i] && ce_anm_get_vm(s_proj_vm[i])) {
            uint8_t *vm = ce_anm_get_vm(s_proj_vm[i]);
            ce_anm_set_pos(s_proj_vm[i], p->x, p->y, pz);
            ce_anm_set_scale(s_proj_vm[i], PS_BALL_SCALE, PS_BALL_SCALE);   /* ★ P50：缩小 ✓（原来这里漏了 ✗）*/
            *(volatile uint32_t *)(vm + CE_ANM_VM_COLOR1) = PS_ELEC_RGBA;   /* ★ P50：**两个颜色字段都写** ✓ */
            *(volatile uint32_t *)(vm + CE_ANM_VM_COLOR2) = PS_ELEC_RGBA;
        }
        if (s_proj_vm[i] && ce_anm_get_vm(s_proj_vm[i])) {
            ce_anm_set_pos(s_proj_vm[i], p->x, p->y, pz);
            ce_anm_set_scale(s_proj_vm[i], PS_BALL_SCALE, PS_BALL_SCALE);
            if (ce_anm_get_vm(s_proj_vm[i])) {   /* ★ P50：两个字段都写 ✓ */
                *(volatile uint32_t *)(ce_anm_get_vm(s_proj_vm[i]) + CE_ANM_VM_COLOR1) = PS_ELEC_RGBA;
                *(volatile uint32_t *)(ce_anm_get_vm(s_proj_vm[i]) + CE_ANM_VM_COLOR2) = PS_ELEC_RGBA;
            }
        }
        if (pe_proj_out(p)) { ps_proj_kill(i); continue; }
        if (pe_proj_hit_now(p)) {                   /* 定点伤害源（与 89 同口径 ✓，命中不销毁 ✓）*/
            float center[3] = { p->x, p->y, pz };
            float w = (p->set == PE_SET_GATLING) ? PE_HIT_W_GATLING : PE_HIT_W_NORMAL;
            ce_damage_rect(center, 0.0f, PS_CHAIN_LIFE, p->dmg, w, w);
            pe_proj_fired(p);
        }
        {   /* ★ P53（owner：「一个电球能开三条」「链不能 visited」）：
             *   ① 取半径内**最近的 3 只**（**按距离** ✓ —— 上一版取的是敌人链表前三只 ✗）；
             *   ② **每一只都往下传一条链** ✓（上一版只有第一条传 ✗）⇒ 一个球真正"开三条" ✓；
             *   ③ 链里**没有 visited 名单** ✓ —— 只禁止"原地打自己"（下一跳 ≠ 当前 ✓）⇒ 回头/复用敌人允许 ✓。
             *   小数组 + 插入式取最近 3 ✓（不调 libm ✓、replay 确定 ✓）。*/
            uint8_t *em = CE_ENEMY_MGR(), *e;
            float cx[PS_BALL_LINKS], cy[PS_BALL_LINKS];
            uint32_t ci[PS_BALL_LINKS];
            int cn = 0, k, m;
            if (em) {
                CE_ENEMY_FOREACH(em, e) {
                    const float *pp;
                    float dx, dy, d2;
                    uint32_t id;
                    if (!ce_enemy_lockable(e)) continue;
                    pp = ce_enemy_pos(e);
                    dx = pp[0] - p->x; dy = pp[1] - p->y;
                    d2 = dx * dx + dy * dy;
                    if (d2 > PS_CHAIN_TRIG_R * PS_CHAIN_TRIG_R) continue;
                    id = *(const uint32_t *)(e + CE_ENEMY_ID);
                    if (id == 0) continue;
                    if (cn < PS_BALL_LINKS) {
                        cx[cn] = pp[0]; cy[cn] = pp[1]; ci[cn] = id; ++cn;
                    } else {                                   /* 已有 3 只 ⇒ 若新的更近就顶掉最远那只 ✓ */
                        int far = 0; float fd = -1.0f;
                        for (k = 0; k < cn; ++k) {
                            float ex2 = cx[k] - p->x, ey2 = cy[k] - p->y, d3 = ex2 * ex2 + ey2 * ey2;
                            if (d3 > fd) { fd = d3; far = k; }
                        }
                        if (d2 < fd) { cx[far] = pp[0]; cy[far] = pp[1]; ci[far] = id; }
                    }
                }
            }
            for (m = 0; m < cn; ++m) {
                /* 球→敌那条只是观感 ✓（伤害由弹自己的定点源出 ✓）*/
                ps_arc_show(0x8000u | (uint32_t)i, ci[m], p->x, p->y, cx[m], cy[m], pz);
                /* ★ 每只都往下传 ⇒ 一个球开三条链 ✓ */
                ps_chain_from(ci[m], p->x, p->y, pz);
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
/* ★ P54（owner：「有无联动 …… 随机方向的持续发弹」✓）：**原始爆发**走引擎豌豆 ✓，
 *   不动 SHT 数据（往 APPEND 里加组会挪后面所有组号 ✗）⇒ 改在**出膛那一帧**把弹的**朝向角**写成随机 ✓：
 *   `bullet+0x64` = 一手 `func_on_init=5`（`0x4612d0`）写 `player+0x479cc` 的那个字段 ✓
 *   （`engine/sht/th18/02-shooter-record.md` + `append_shooterset.py` 的注 ✓）。
 *   ⚠ 只在本卡**开大期间**动手 ✗ —— 平时的普通豌豆方向一字不改 ✓。*/
#define PS_BULLET_ANGLE   0x64
/* ★★ 2026-10-15 新增（owner 报：「**电大哥的弹幕怎么把我主机的弹幕也弄成全方向随机的了**」✗）：
 *   本钩子（`on_bullet_created` ✓）= **每颗弹出生都广播** ✓ ⇒ 原来只判"本卡在开大"、
 *   **没判这颗弹是谁的** ✗ ⇒ 开大期间**玩家主机打出的每一颗弹**朝向都被改写 ✗。
 *   ⇒ 先**认弹**：`bullet+0xac` = `(set << 8) | 组内行号` ✓（一手：`Player__shoot_one_bullet` 收的
 *     packed id 一路存进 `bullet+0xac` ✓ —— `engine/sht/th18/02-shooter-record.md:68-70` ✓）
 *     ⇒ **`>> 8` 即 shooterset 组号** ✓，只有 ∈ {`CE_SHT_SET_PEA_NORMAL`(**0x17**),
 *     `CE_SHT_SET_PEA_GATLING`(**0x18**)} 才是本卡的弹 ✓（生成物 `native/sht_ids.h:4-5` ✓；
 *     ⚠ **别**拿卡侧 `PE_SET_NORMAL/GATLING` 的 0/1 来比 ✗ —— 那是卡侧**自建弹**的块内序号 ✓）。
 *   另一条现成判据是 `bullet+0xe4` = **子机指针** ✓（`0x45e3ae` 写 ✓；SDK 的 `ce_option_kill_bullets`
 *   用的就是它 ✓）—— 这里不用：组号已足够精确 ✓，也免去跟踪 option 指针的生命周期 ✓。*/
#define PS_BULLET_SETID   0xac
static int on_bullet_created(ce_card_t *c, void *bullet)
{
    ps_card_t *s = ce_state(c, ps_card_t);
    uint8_t *b = (uint8_t *)bullet;
    if (!b || !s) return 0;
    if (pea_burst_left(&s->st) <= 0) return 0;          /* 不在开大 ⇒ 什么都不做 ✓ */
    if (!pea_bullet_is_ours(*(const uint32_t *)(b + PS_BULLET_SETID),
                            CE_SHT_SET_PEA_NORMAL, CE_SHT_SET_PEA_GATLING))
        return 0;                                        /* ★ 不是本卡的弹 ⇒ 一颗都不许碰 ✗ */
    *(float *)(b + PS_BULLET_ANGLE) = s_rand_spread[(int)(ce_rand() % PS_SPREAD_N)].abs;
    return 0;
}

static int on_tick_2(ce_card_t *c)
{
    ps_proj_tick();
    ps_card_t *s = ce_state(c, ps_card_t);
    if (!s || !CE_PLAYER()) return 0;
    /* ★★★ 2026-10-15（owner 报「电大哥的**无敌一直存在**」✗ 的修）：**先按帧推进爆发** ✓。
     *   原来爆发倒计时只在 `pea_shoot_frame()` 里减 ✗，而那只在 `on_tick_shooters`（**按住射击键才广播** ✗）
     *   里被调 ⇒ 松手 ⇒ 爆发冻结 ⇒ 下面那句无敌声明**一直挂着** ✗。
     *   现在：每帧 `pea_burst_frame()` ✓ ⇒ 松手也照减 ✓ ⇒ 无敌与爆发**同生同灭** ✓。*/
    if (pea_burst_frame(&s->st))                  /* 返回 1 = 开大在本帧结束 */
        ce_log("peashooter: gatling #%u done, %u peas fired in total", s->st.bursts, s->st.peas);
    /* ★ P54（owner：「爆发期间（包括原始）……需要本体无敌」✓，按 PvZ2 一手）：
     *   开大期间每帧**声明** PS_BURST_INVULN 帧无敌 ✓（SDK 声明层取最大值统一落笔 ✓）。*/
    if (pea_burst_left(&s->st) > 0) ce_invuln_request(ce_card_id(c), PS_BURST_INVULN);
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
        /* ★★★ 2026-10-15（owner 裁定，P44 的"一扇 13 颗"**删除** ✗ ⇒ 开大期间只剩上面那一句 ✓）：
         *   owner：「**我是要用 (a) 替换 (b)**」✓「**一手的大招只有一种发弹：向前一个角度范围内的
         *   大量随机角度发弹**」✓「**最好是一帧发一颗，而不是同时发几颗**」✓
         *   ⇒ 每颗的**朝向**由 `s_rand_spread`（±20° 连续随机 ✓）各抽一次 ✓、**一帧一颗** ✓。*/
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
    /* ★ 2026-10-15：原来这里 `if (out.burst_end) ce_log("… gatling #%u done …")` ✓ ——
     *   现在爆发的结束**按帧**判定（可能在**不射击**的帧结束 ✗），所以那行日志搬到了
     *   `on_tick_2` 的 `pea_burst_frame()` 返回处 ✓（`pea_out_t.burst_end` 不再被设置 ✓）。*/
    return 0;
}

CE_CARD(77, .on_power_level_change = on_power_level_change, .on_tick_shooters = on_tick_shooters,
            .on_bullet_created = on_bullet_created,   /* ★ P54：爆发期间把引擎豌豆的朝向写成随机 ✓ */
            .on_tick_2 = on_tick_2, .on_load = on_load, .on_run_reset = on_run_reset,
            .on_delete = on_delete);
