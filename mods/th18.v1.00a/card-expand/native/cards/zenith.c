/* 天顶剑（id 87）—— 装备卡，致敬《泰拉瑞亚》的天顶剑（Zenith，Item 4956 / ProjectileID.FinalFractal 933）。
 *
 * 身边一把剑形**跟班**（原生装备卡的子机机制，与 71 破损核心 / 77 豌豆射手同一套 `ce_option_*`），
 * 每 ZN_PERIOD 帧朝**最近的敌人**甩出一把剑；剑的运动 / 贴图 / 特效都是**泰拉瑞亚原版复刻**：
 *   · 剑不飞 —— 它**绕着「跟班 + velocity」这个点画椭圆**（`velocity` 是出膛定死的偏移，
 *     原版 `A:182` 的 `UpdatePosition` 对 933 直接早退 ⇒ 从不积分）；一圈 60 相位走完、
 *     相位 120 消失（详见 `zenith_core.h` 的对照表）。
 *   · 贴图 = 原版 `FinalFractalHelper` 的 **21 把剑**随机挑（第 1/4/7… 把固定是天顶剑本体，
 *     其余从另外 20 把里抽 —— 对位 `GetRandomProfileIndex()` 会重掷掉 4956）。
 *   · 特效 = 原版的三件：**残影**（每 2 帧一枚、按这把剑的 `trailColor` 染色、渐隐）、
 *     **命中**（沿刃的判定，冷却 7 帧 ≈ 原版 `localNPCHitCooldown`）、出膛音（`se_slash`）。
 *
 * 三件事各自的出处：
 *   ① 跟班：`Player__allocate_option`（引擎管位置 / 聚焦位移 / 开店收起）—— 见 `broken_core.c` 文件头；
 *      侧向站位自己写 `option+0x64/+0x6c`（原生卡 `0x40b8c1` 同款；allocate 只写 y 偏移）。
 *   ② 瞄准：距离平方取最近（射程 640；原版是「光标 400 px 内最近敌」）+ `bc_atan2f`。
 *   ③ 剑刃：**卡自管的 ANM VM**（炎魔火球 72 的写法，但**多发在途的池**是本卡的新写法）。
 *      椭圆的两次旋转走引擎的 `ce_polar_to_vel()`（FSINCOS）—— 我们自己的目标文件零 sin/cos、零 x87。
 *
 * 收尾（CARD-CHECKLIST §3）：`on_delete` 删光全部剑刃 VM + 最近一枚残影 VM + 还子机槽 ——
 * 商店里删卡时 tick 全停，只有 `on_delete` 跑得到。
 * 纯逻辑（节奏 / 池 / 运动参数）在 `zenith_core.c`，主机单测 `tests/test_zenith_core.c`。 */
#include "sdk.h"
#include "zenith_core.h"
#include "broken_core_core.h"      /* bc_atan2f：确定性 SSE 多项式（禁调引擎的 x87 atan2）*/

#define ZN_OPT_ANM_Y    0x10       /* allocate 的偏移参（引擎把它写进 option+0x68/+0x70 两个 y 偏移）*/
/* ★ 2026-10-03 owner 实测口径：「天顶剑位置放在**自机正中心**，但是是**身下**，甚至可以不画」
 *   ⇒ 站位 (0, 0)（正好压在自机身上）+ 层比自机低 ⇒ 被自机盖住（"身下"）。
 *   ❓「可以不画」我按**保留但被盖住**处理（万一别的路径要用它的坐标）；要彻底不画就把
 *     `ZN_FAM_HIDE` 打开或干脆不建 VM。*/
#define ZN_OPT_NF_X     0.0f
#define ZN_OPT_NF_Y     0.0f
#define ZN_OPT_F_X      0.0f
#define ZN_OPT_F_Y      0.0f
#define ZN_INPUT_Z      0x001u     /* 同 89：按住 Z 才甩剑 */
#define ZN_FAM_CYCLE    0         /* ❓ 0 = 跟班不画（owner 口径）；1 = 恢复 21 把轮播观感 */
#define ZN_RETRY        60         /* 关卡中途买到这张卡：没拿到子机时的重试间隔（槽满 / 广播还没来）*/
/* ★★ 2026-10-03（owner：「87 尽量做吧」）：把原版那几样**挥动特效**补上（一手见 AUDIT §BE #211 追加）：
 *   · 刃尖**星芒** `DrawPrettyStarSparkle`：4 个 `Extra[98]` quad，`t 15→30` 淡入、`30→45` 淡出
 *     ⇒ 每把剑一个 `script388` 的 VM，挂在 `Center + bladeDir·(bladeLength/2)`；
 *   · **×1.25 白色叠加**（原版的辉光第二遍）：每把剑再加一个同 sprite 的 VM，scale ×1.25、白、alpha 半；
 *   · **尘埃**（`DustID 278 FireworksRGB`，1-in-9 × 1–3 颗、沿刃向 +3..7）：`script389/390` 两支自走自灭
 *     的小星点，染色用那把剑的 trailColor；
 *   ⚠ **ribbon 仍然没有**（引擎无顶点条带通道）：那条弧线依旧由 `script380` 的残影顶替。
 *   ⚠ 光照（原版两处 `Lighting.AddLight`）也没法做（引擎无动态光照通道）。*/
#define ZN_SE_EVERY     4          /* ★ 2026-10-24 owner「太吵」：每 4 把剑播一次（1 = 旧行为）*/
#define ZN_SPARK_LAYER  14         /* 星芒 / 尘埃：压在剑刃（13）之上 */
#define ZN_OVERLAY_MUL  1.25f      /* 原版第二遍白叠的放大倍数 */
#define ZN_BLADE_SCALE  0.55f      /* ⚠ 必须与脚本 379 的 `scale(0.55f, 0.55f)` 一致（C 每帧会写 scale 做镜像）*/
#define ZN_SPARK_IN     15.0f      /* 星芒淡入：t 15→30 */
#define ZN_SPARK_FULL   30.0f
#define ZN_SPARK_OUT    45.0f      /* 星芒淡出：t 30→45 */
#define ZN_SPARK_SX0    0.16f      /* 星芒尺寸（起 → 满）*/
#define ZN_SPARK_SX1    0.30f
#define ZN_SPARK_SY0    0.10f
#define ZN_SPARK_SY1    0.18f
#define ZN_DUST_ODDS    16         /* 尘埃 1-in-N（原版是 `Next(9) == 0`；实测嫌杂 ⇒ 调稀一倍）❓ 自定*/
#define ZN_DUST_MAX     3          /* 一次最多几颗（原版 1–3）*/
#define ZN_LAYER        13         /* 世界特效层（破损核心 / 炎魔 / 青眼同层）*/
#define ZN_SE_SLASH     0x2d       /* TH18 音效表 0x2d = se_slash.wav（一手 engine/_shared/th18-sound-table.md）；
                                    * 对位 Terraria 的 `SoundID.Item169`（原版每把出膛放一次）*/
#define ZN_FAM_PERIOD   40         /* ❓自定：跟班**轮播**整组 21 把的节奏（帧/把 ⇒ 一轮 40×21 = 840 帧 = 14 s）*/
#define ZN_RANDOM_N     20         /* 随机池 = 去掉天顶剑本体自己（原版 `GetRandomProfileIndex` 会重掷 4956）*/

typedef struct {
    ce_option_t opt;               /* 跟班槽（SDK：认领 / 申请 / 收起）*/
    zn_state_t  st;                /* 池 + 节奏 + 运动（纯逻辑）*/
    /* ★ 2026-10-02（owner：「跟班也轮流显示整组 21 把」）：跟班的**贴图轮播**状态。
     *   `fam_vm` 记住上一次落笔的那个 VM —— 引擎重建子机（repopulate / 换关）会换成新 id，
     *   发现 id 变了就**立刻**把当前这一把补上（不然那一帧会闪回脚本默认的天顶剑）。*/
    uint8_t  fam_idx;              /* 当前显示的是 21 把里的第几把（0 = 天顶剑本体）*/
    uint32_t fam_timer;
    uint32_t fam_vm;               /* 上一次 `set_sprite` 作用的 VM id（0 = 还没落过笔）*/
} zn_card_t;

_Static_assert(sizeof(zn_card_t) <= 496, "zenith: private state overflows CE_STATE_BYTES");

/* ★ 泰拉瑞亚的剑阵表（`FinalFractalHelper._fractalProfiles`，**顺序照原版**；物品号与 trailColor
 *   都是反 IL 读出来的，见 AUDIT §BE 详录 #211 与 `assets/ability/entries/ORDER.txt` 末段）。
 *   原版结构 = `FinalFractalProfile(ItemID, bladeLength, trailColor)`；我们只用得到
 *   「挑哪张 sprite」与「残影染什么色」两件事（bladeLength 是原版 ribbon 的半宽，我们不做 ribbon）。*/
typedef struct {
    int32_t  sprite;
    uint32_t color;                /* 0x00RRGGBB：strarColor（原版 `FinalFractalProfile.trailColor`）*/
    float    blade_len;            /* ★ 原版 `fullBladeLength`（一手 1.4.5.0 表）—— 只用来算**刃尖**
                                    *   位置与 ribbon 半宽（`trailWidth = bladeLength/2`）：星芒就画在
                                    *   `Center + bladeDir·trailWidth` 上（见 AUDIT §BE #211 追加）*/
} zn_profile_t;

/* 条数 `ZN_PROFILE_N` 在 `zenith_core.h`（core 侧要按 prof 查贴图基准朝向）*/
static const zn_profile_t s_zn_profiles[ZN_PROFILE_N] = {
    { CE_ANM_ABILITY_SPRITE_ZENITH_BLADE_STARFURY,       0x00EC3EC0u , 60.0f },  /* 65   星怒 */
    { CE_ANM_ABILITY_SPRITE_ZENITH_BLADE_BEEKEEPER,      0x00FFE745u , 75.0f },  /* 1123 养蜂人（原版 = Main.OurFavoriteColor）*/
    { CE_ANM_ABILITY_SPRITE_ZENITH_BLADE_LIGHTSBANE,     0x007A42BFu , 65.0f },  /* 46   光之灾 */
    { CE_ANM_ABILITY_SPRITE_ZENITH_BLADE_FIERYGREATSWORD,0x00FE9E23u , 80.0f },  /* 121  炽焰巨剑（🟡 bladeLength **当时没从 IL 解出** ⇒ 取 80，见 TUNING §2.x-87）*/
    { CE_ANM_ABILITY_SPRITE_ZENITH_BLADE_BLADEOFGRASS,   0x006BCB00u , 105.0f },  /* 190  草剑 */
    { CE_ANM_ABILITY_SPRITE_ZENITH_BLADE_EXCALIBUR,      0x00ECC813u , 75.0f },  /* 368  断钢剑 */
    { CE_ANM_ABILITY_SPRITE_ZENITH_BLADE_TRUEEXCALIBUR,  0x00ECC813u , 75.0f },  /* 674  真断钢剑 */
    { CE_ANM_ABILITY_SPRITE_ZENITH_BLADE_NIGHTSEDGE,     0x00B336C9u , 70.0f },  /* 273  暗影焰刃 */
    { CE_ANM_ABILITY_SPRITE_ZENITH_BLADE_TRUENIGHTSEDGE, 0x00B336C9u , 70.0f },  /* 675  真暗影焰刃 */
    { CE_ANM_ABILITY_SPRITE_ZENITH_BLADE_INFLUXWAVER,    0x0054EAF5u , 70.0f },  /* 2880 波涌之刃 */
    { CE_ANM_ABILITY_SPRITE_ZENITH_BLADE_ENCHANTEDSWORD, 0x005B9EE8u , 48.0f },  /* 989  附魔剑 */
    { CE_ANM_ABILITY_SPRITE_ZENITH_BLADE_HORSEMAN,       0x00FC5F04u , 76.0f },  /* 1826 无头骑士剑 */
    { CE_ANM_ABILITY_SPRITE_ZENITH_BLADE_MEOWMERE,       0x00FEC2FAu , 76.0f },  /* 3063 喵刀 */
    { CE_ANM_ABILITY_SPRITE_ZENITH_BLADE_STARWRATH,      0x00ED3F85u , 70.0f },  /* 3065 狂星之怒 */
    { CE_ANM_ABILITY_SPRITE_ZENITH_BLADE_TERRABLADE,     0x0050DE7Au , 70.0f },  /* 757  泰拉刃 */
    { CE_ANM_ABILITY_SPRITE_ZENITH_BLADE_MURAMASA,       0x00384ED2u , 70.0f },  /* 155  村正 */
    { CE_ANM_ABILITY_SPRITE_ZENITH_BLADE_BLOODBUTCHERER, 0x00ED1C24u , 72.0f },  /* 795  嗜血者 */
    { CE_ANM_ABILITY_SPRITE_ZENITH_BLADE_SEEDLER,        0x008FD71Du , 75.0f },  /* 3018 种子弯刀 */
    { CE_ANM_ABILITY_SPRITE_ZENITH_BLADE_TERRAGRIM,      0x00B2FFB4u , 45.0f },  /* 4144 地灵之刃 */
    { CE_ANM_ABILITY_SPRITE_ZENITH_BLADE_COPPERSHORT,    0x00EBA687u , 45.0f },  /* 3507 铜短剑 */
    { CE_ANM_ABILITY_SPRITE_ZENITH_BLADE_ZENITH,         0x00B2FFB4u , 86.0f },  /* 4956 天顶剑本体 */
};

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

/* ★★ 2026-10-03 owner 实测「我所谓的残留虚影是**孤儿虚影**」⇒ 找到真 bug：
 *   原来只有"寿命尽 / 删卡"两条路径收了 `ov_id`/`sp_id`，**另外三条早退路径**（VM 被引擎收掉 /
 *   出界 / 池满顶掉）只清了 `id`/`trail_id` ⇒ ×1.25 那层白叠（**整把剑**！）和刃尖星芒就留在屏上
 *   当孤儿。现在所有路径一律走这个函数：**四个句柄要么一起活、要么一起死**。*/
static void zn_kill_blade(zn_blade_t *b)
{
    if (!b) return;
    if (b->id) ce_anm_delete(b->id);
    if (b->trail_id) ce_anm_delete(b->trail_id);   /* 更早的残影脚本自己 delete，这里只兜底最近一枚 */
    if (b->ov_id) ce_anm_delete(b->ov_id);
    if (b->sp_id) ce_anm_delete(b->sp_id);
    b->id = b->trail_id = b->ov_id = b->sp_id = 0;
    b->active = 0;
}

/* 收掉所有在飞的剑刃 + 它们的残影。**不依赖 tick**（商店里删卡时只有 on_delete 跑得到）。 */
static void zn_kill_blades(ce_card_t *c)
{
    zn_card_t *s = ce_state(c, zn_card_t);
    int i;
    if (!s) return;
    for (i = 0; i < ZN_BLADE_MAX; ++i) zn_kill_blade(&s->st.b[i]);
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

/* +0x50：卡被局中删掉（倒吊人 / 原版空白卡弃整副牌，都在商店里成交）—— 那一刻 tick 全停。 */
static void on_delete(ce_card_t *c)
{
    on_run_reset(c);
}

static int on_load(ce_card_t *c)
{
    on_run_reset(c);
    return 0;
}

/* ★ 跟班贴图轮播（owner 2026-10-02：「跟班也轮流显示整组 21 把」）：
 * 每 `ZN_FAM_PERIOD` 帧换成 21 把里的下一把；子机 VM 被引擎重建时（换关 / 火力档变 / 开店收起再开）
 * 立刻把当前这一把补上。贴图取自同一份 `ATLAS_ZENITH_BLADE`（sprite 表是**文件级**的 ⇒ 脚本 378
 * 用的是单图 entry 的 sprite，也能换成图集里的成员，`ce_anm_set_sprite` 就是这个语义）。*/
static void zn_fam_cycle(zn_card_t *s, uint8_t *o)
{
    uint32_t vm;
    if (!o) return;
    vm = *(uint32_t *)(o + CE_OPT_ANM_ID);
    if (!vm) { s->fam_vm = 0; return; }
    if (vm != s->fam_vm) {                      /* 新 VM（首次 / 被重建）⇒ 立刻补上当前这一把 */
        s->fam_vm = vm;
        s->fam_timer = 0;
        ce_anm_set_sprite(CE_ABILITY_ANM(), vm, s_zn_profiles[s->fam_idx].sprite);
        return;
    }
    if (++s->fam_timer >= ZN_FAM_PERIOD) {
        s->fam_timer = 0;
        s->fam_idx = (uint8_t)((s->fam_idx + 1u) % ZN_PROFILE_N);
        ce_anm_set_sprite(CE_ABILITY_ANM(), vm, s_zn_profiles[s->fam_idx].sprite);
    }
}

/* 把一帧的位姿参数落成世界坐标：两次 `ce_polar_to_vel`（原版的两次 `RotatedBy`）。
 *   offset = (Vector2(1,0).RotatedBy(ang) · Vector2(R, A)).RotatedBy(heading)
 *          = ((R·cos ang, A·sin ang)) 在 heading 的坐标系里再转一次。*/
static void zn_place_blade(const zn_blade_t *b, const zn_pose_t *p, float fx, float fy,
                           float *out_x, float *out_y)
{
    float e[2], h[2], ex, ey;

    ce_polar_to_vel(e, p->ang, 1.0f);          /* (cos ang, sin ang) */
    ex = p->R * e[0];
    ey = p->A * e[1];
    ce_polar_to_vel(h, b->heading, 1.0f);      /* (cos heading, sin heading) */
    *out_x = fx + b->vx + (ex * h[0] - ey * h[1]) + p->fade_x;
    *out_y = fy + b->vy + (ex * h[1] + ey * h[0]) + p->fade_y;
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
    zn_pose_t pose;
    float fx, fy, pz, x, y, center[3];
    const float *pp;
    uint32_t id;
    int i;

    if (!s || !p) return 0;
    pp = ce_player_pos();                       /* 自机坐标（虚影防糊在自机周围要用）*/
    o = ce_option_get(c, &s->opt, ZN_OPT_ANM_Y, CE_ANM_ABILITY_SCRIPT_ZENITH_OPT, ZN_RETRY, "zenith");
    if (!o) return 0;
    zn_place(&s->opt);                          /* 补申请 / 重建过 ⇒ 站位重新写一遍（幂等）*/
    /* ⚠ 跟班轮播（`zn_fam_cycle`）**停用**：option 现在挂的是 1×1 全透明贴图（见脚本 378），
     *   轮播会用 `ce_anm_set_sprite` 把剑的贴图贴回来 ⇒ 又露在自机身上（owner 实测的那个）。
     *   函数留着（`ZN_FAM_CYCLE` 打开就能恢复 21 把轮播的观感）。*/
#if ZN_FAM_CYCLE
    zn_fam_cycle(s, o);
#else
    (void)o;
#endif
    {   /* ★ owner 2026-10-03 实测：「天顶剑位置放在**自机正中心**，但是是**身下**，甚至可以不画」
         *   ⇒ 站位已经在 `zn_place()` 里改成 (0,0)；这里再把 option 那个 VM **整只透明**
         *     （"身下/不画"），剑阵由剑刃自己表现。VM 仍留着当**定位锚**（剑刃从它的坐标出发）。*/
        uint32_t fvm = *(uint32_t *)(o + CE_OPT_ANM_ID);
        if (fvm) ce_anm_set_color(fvm, 0x00000000u);
    }
    pz = ce_player_pos()[2];
    fx = ce_option_x(o);                        /* ★ 原版每帧重读 `player.MountedCenter` ⇒ 剑阵跟着跟班走 */
    fy = ce_option_y(o);

    /* ① 推进在飞的剑刃：算位姿（两次 RotatedBy 由 `ce_polar_to_vel` 完成）+ 命中 + 残影。 */
    for (i = 0; i < ZN_BLADE_MAX; ++i) {
        b = &s->st.b[i];
        if (!b->active) continue;
        if (!ce_anm_get_vm(b->id)) {            /* VM 被引擎收掉了（脚本 delete / 被别的路清）*/
            zn_kill_blade(b);                   /* ★ 连 ov/sp 一起收（孤儿虚影的那个 bug）*/
            continue;
        }
        zn_step(b, &pose);
        if (pose.done) {
            zn_kill_blade(b);
            continue;
        }
        zn_place_blade(b, &pose, fx, fy, &x, &y);
        if (x < -ZN_VIEW_X || x > ZN_VIEW_X || y < ZN_VIEW_Y_MIN || y > ZN_VIEW_Y_MAX) {
            zn_kill_blade(b);
            continue;
        }
        ce_anm_set_pos(b->id, x, y, pz);
        ce_anm_set_rotation(b->id, 0.0f, 0.0f, pose.rot);
        /* ★ 原版 `spriteDirection = −1` 那支要**水平翻转贴图**（owner：「得翻转贴图的，种子弯刀是刀」）：
         *   负的 scaleX 就是镜像（旋转由 rotation 表达，翻转由 scale 符号表达）。*/
        ce_anm_set_scale(b->id, pose.mirror ? -ZN_BLADE_SCALE : ZN_BLADE_SCALE, ZN_BLADE_SCALE);
        ce_anm_set_color(b->id, ((uint32_t)pose.alpha << 24) | 0x00ffffffu);   /* 原版 Opacity */
        if (pose.hit) {                          /* 原版 localNPCHitCooldown：每 7 帧一次（沿刃的判定）*/
            center[0] = x; center[1] = y; center[2] = pz;
            ce_damage_rect(center, pose.rot, ZN_HIT_LIFE, ZN_DMG, ZN_HIT_W, ZN_HIT_H);
        }
        /* ---- ★ 挥动特效三件（2026-10-03 补；一手见 AUDIT §BE #211 追加）---- */
        {
            const zn_profile_t *bp = &s_zn_profiles[(b->prof >= 0 && b->prof < ZN_PROFILE_N) ? b->prof : 0];
            float bd[2];
            float tipx, tipy, t = b->phase;
            /* 原版刃尖方向 = `(rotation − π/2).ToRotationVector2()`（它的贴图正上朝）；
             * 我们的贴图基准是 `ZN_SPRITE_BASE` ⇒ 剑尖方向 = `rot + ZN_SPRITE_BASE`。
             * ⚠ 之前写的是 `rot + π/2`（差了 180°）⇒ 星芒其实挂在**剑柄**那头。*/
            ce_polar_to_vel(bd, pose.tip, 1.0f);
            tipx = x + bd[0] * (bp->blade_len * 0.5f);
            tipy = y + bd[1] * (bp->blade_len * 0.5f);

            if (b->ov_id) {                                  /* ×1.25 白叠（原版辉光第二遍）*/
                if (!ce_anm_get_vm(b->ov_id)) { b->ov_id = 0; }
                else {
                    ce_anm_set_pos(b->ov_id, x, y, pz);
                    ce_anm_set_rotation(b->ov_id, 0.0f, 0.0f, pose.rot);
                    ce_anm_set_scale(b->ov_id,
                                     (pose.mirror ? -1.0f : 1.0f) * ZN_BLADE_SCALE * ZN_OVERLAY_MUL,
                                     ZN_BLADE_SCALE * ZN_OVERLAY_MUL);
                    ce_anm_set_color(b->ov_id, ((uint32_t)(pose.alpha / 2) << 24) | 0x00ffffffu);
                }
            }
            if (b->sp_id) {                                  /* 刃尖星芒：t 15→30 淡入、30→45 淡出 */
                if (!ce_anm_get_vm(b->sp_id)) { b->sp_id = 0; }
                else {
                    float fin = (t - ZN_SPARK_IN) / (ZN_SPARK_FULL - ZN_SPARK_IN);
                    float fout = (ZN_SPARK_OUT - t) / (ZN_SPARK_OUT - ZN_SPARK_FULL);
                    float k = (fin < fout) ? fin : fout;
                    uint32_t cr, cg, cb;
                    int a;
                    if (k < 0.0f) k = 0.0f;
                    if (k > 1.0f) k = 1.0f;
                    a = (int)(255.0f * k);
                    /* 原版两个轴各取一种色（纯白 × 每把剑的 trailColor）—— 我们一层 ⇒ 按 k 插值 */
                    cr = (uint32_t)(255.0f + ((float)((bp->color >> 16) & 0xffu) - 255.0f) * k);
                    cg = (uint32_t)(255.0f + ((float)((bp->color >> 8) & 0xffu) - 255.0f) * k);
                    cb = (uint32_t)(255.0f + ((float)(bp->color & 0xffu) - 255.0f) * k);
                    ce_anm_set_pos(b->sp_id, tipx, tipy, pz);
                    ce_anm_set_rotation(b->sp_id, 0.0f, 0.0f, pose.rot);
                    ce_anm_set_scale(b->sp_id,
                                     (pose.mirror ? -1.0f : 1.0f) * (ZN_SPARK_SX0 + (ZN_SPARK_SX1 - ZN_SPARK_SX0) * k),
                                     ZN_SPARK_SY0 + (ZN_SPARK_SY1 - ZN_SPARK_SY0) * k);
                    ce_anm_set_color(b->sp_id, ((uint32_t)a << 24) | (cr << 16) | (cg << 8) | cb);
                }
            }
            /* 尘埃：1-in-N × 1–3 颗（原版 `Main.rand.Next(9) == 0`），自走自灭、卡侧不记账 */
            if (t < ZN_PHASE_SWING && (ce_rand() % (uint32_t)ZN_DUST_ODDS) == 0u) {
                uint32_t nd = 1u + (ce_rand() % (uint32_t)ZN_DUST_MAX), k2;
                for (k2 = 0; k2 < nd; ++k2) {
                    uint32_t du = ce_anm_spawn(CE_ABILITY_ANM(),
                                               (ce_rand() & 1u) ? CE_ANM_ABILITY_SCRIPT_ZENITH_DUST_L
                                                                : CE_ANM_ABILITY_SCRIPT_ZENITH_DUST_R,
                                               ZN_SPARK_LAYER);
                    if (!du) break;
                    /* 出生点 = 刃上随机一处（原版 `Center + bladeDir·trailWidth·Lerp(0.5,1,rand)`）*/
                    ce_anm_set_pos(du, x + bd[0] * (bp->blade_len * 0.5f * 0.75f),
                                   y + bd[1] * (bp->blade_len * 0.5f * 0.75f), pz);
                    ce_anm_set_color(du, 0xff000000u | bp->color);   /* 染色 = 那把剑的 trailColor */
                }
            }
        }

        /* 离自机太近就不留虚影（owner：「虚影集中在玩家背后十分明显」—— 剑从自机身上出膛，
         * 前半程贴着自机）＋ ★ **按距离发**（owner：「它是因为在收回的时候 v 小，所以虚影密」）：
         * 帧门只给"机会"，真要落笔还得自上一枚起走过 `ZN_TRAIL_SPACING` px ⇒ 回收段走得慢，
         * 残影就稀，不会在那一小段糊成一坨。`trail_last` 打包成 x 高 16 / y 低 16（1/2 px 定点）。*/
        if (pose.emit_trail && zn_trail_far_enough(b, x, y) &&
            zn_dist2(x, y, pp[0], pp[1]) > ZN_TRAIL_MIN_DIST * ZN_TRAIL_MIN_DIST) {
            const zn_profile_t *tp = &s_zn_profiles[(b->prof >= 0 && b->prof < ZN_PROFILE_N) ? b->prof : 0];
            uint32_t tr = ce_anm_spawn(CE_ABILITY_ANM(), CE_ANM_ABILITY_SCRIPT_ZENITH_TRAIL, ZN_LAYER);
            if (tr) {
                ce_anm_set_sprite(CE_ABILITY_ANM(), tr, tp->sprite);          /* 残影用同一把剑 */
                ce_anm_set_color(tr, 0xff000000u | tp->color);                /* 原版 ribbon = 每把剑的 trailColor */
                ce_anm_set_pos(tr, x, y, pz);
                ce_anm_set_rotation(tr, 0.0f, 0.0f, pose.rot);
                ce_anm_set_scale(tr, pose.mirror ? -ZN_BLADE_SCALE : ZN_BLADE_SCALE, ZN_BLADE_SCALE);
                b->trail_last = zn_trail_pack(x, y);
                b->trail_id = tr;      /* 只记最近的（更早的自己 delete，同炎魔拖尾的处理）*/
            }
        }
    }
    if (zn_live(&s->st) > 0) ce_raise_damage_cap(ZN_CAP);   /* 自机弹与卡共用每敌每帧上限：不抬就被自机弹吃满 */

    /* ② 蓄满才甩；没目标 / 池满就一直攒着（蓄满状态保持，敌人一进射程就出手）。*/
    if (!zn_charge(&s->st) || !em) return 0;
    zn_aim_reset(&aim);
    CE_ENEMY_FOREACH(em, enemy) {
        if (!ce_enemy_lockable(enemy)) continue;
        zn_aim_consider(&aim, fx, fy, ce_enemy_pos(enemy)[0], ce_enemy_pos(enemy)[1]);
    }
    if (!zn_aim_ready(&aim)) return 0;
    /* ★ owner 口径：按住 Z 才甩剑；再叠"活着 + 不在对话里"两条 —— 与原生子机射击
     *   （豌豆射手那套 `ce_tick_shooters_for_card` 的引擎门）语义一致。*/
    if ((CE_INPUT_HELD() & ZN_INPUT_Z) == 0u) return 0;
    if (*(int32_t *)(p + CE_PLAYER_STATE) != 1) return 0;
    if (CE_GUI_MSG_ACTIVE()) return 0;
    b = zn_slot(&s->st);
    if (!b) return 0;
    id = ce_anm_spawn(CE_ABILITY_ANM(), CE_ANM_ABILITY_SCRIPT_ZENITH_BLADE, ZN_LAYER);
    if (!id) return 0;
    {
        /* 原版：一挥 3 把，**第 1 把固定是天顶剑本体**，其余从另外 20 把里随机（`GetRandomProfileIndex`
         * 会重掷掉 4956）。我们把它按「每第 3 把 = 本体」对位（跟班是连续出膛，没有"一挥"这个概念）。*/
        int32_t slot = (s->st.fired % 3u == 0u) ? (ZN_PROFILE_N - 1)
                                                : (int32_t)(ce_rand() % ZN_RANDOM_N);
        const zn_profile_t *pr = &s_zn_profiles[slot];
        ce_anm_set_sprite(CE_ABILITY_ANM(), id, pr->sprite);
        zn_launch(b, id, (int32_t)slot, fx, fy, fx + aim.dx, fy + aim.dy,
                  (float)(int32_t)(ce_rand() % (2u * (uint32_t)ZN_AI0_MAX + 1u)) - ZN_AI0_MAX);
    }
    /* ★ 两个挥动特效的 VM 在出膛时一次建好（之后每帧由下面那段写 pos / scale / color，收剑时一起删）*/
    {
    const zn_profile_t *pr = &s_zn_profiles[(b->prof >= 0 && b->prof < ZN_PROFILE_N) ? b->prof : 0];
    b->ov_id = ce_anm_spawn(CE_ABILITY_ANM(), CE_ANM_ABILITY_SCRIPT_ZENITH_BLADE, ZN_LAYER);
    if (b->ov_id) {
        ce_anm_set_sprite(CE_ABILITY_ANM(), b->ov_id, pr->sprite);
        ce_anm_set_scale(b->ov_id, 0.55f * ZN_OVERLAY_MUL, 0.55f * ZN_OVERLAY_MUL);
    }
    b->sp_id = ce_anm_spawn(CE_ABILITY_ANM(), CE_ANM_ABILITY_SCRIPT_ZENITH_SPARKLE, ZN_SPARK_LAYER);
    if (b->sp_id) ce_anm_set_color(b->sp_id, 0x00ffffffu);   /* 先全透明，相位到 15 才亮 */
    /* 日志里那句 sprite 用它 */
    {
        int spr_log = (int)pr->sprite;
        (void)spr_log;
    }
    }
    ce_anm_set_pos(id, fx, fy, pz);           /* 出膛那帧先摆在膛口（下一帧起走椭圆）*/
    ce_anm_set_color(id, 0x00ffffffu);
    zn_did_fire(&s->st);
    /* ★★ 2026-10-24（owner：**天顶剑的声音太吵了**）—— 原来**每一把剑**都播一次 `se_slash`，
     *   多把同帧出鞘就叠成噪音 ⇒ 每 `ZN_SE_EVERY` 把剑才播一次（`ZN_SE_EVERY = 1` 恢复旧行为）。*/
    if (ZN_SE_EVERY <= 1 || ((s->st.fired - 1u) % (uint32_t)ZN_SE_EVERY) == 0u)
        ce_play_sound(ZN_SE_SLASH, fx);
    if (s->st.fired <= 3 || s->st.fired % 20 == 0)
        ce_log("zenith: blade #%u at frame %u, fam (%.1f, %.1f) -> (%.1f, %.1f) v|%.1f| ai0 %.0f sprite %d, live %d",
               s->st.fired, s->st.frames, fx, fy, fx + aim.dx, fy + aim.dy,
               zn_vlen(b), (double)b->ai0,
               (int)s_zn_profiles[(b->prof >= 0 && b->prof < ZN_PROFILE_N) ? b->prof : 0].sprite,
               zn_live(&s->st));
    return 0;
}

CE_CARD(87, .on_power_level_change = on_power_level_change, .on_tick_2 = on_tick_2,
            .on_load = on_load, .on_run_reset = on_run_reset, .on_delete = on_delete);
