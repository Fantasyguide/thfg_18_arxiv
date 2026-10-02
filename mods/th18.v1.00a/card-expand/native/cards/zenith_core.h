/* zenith_core.h —— 天顶剑（id 87）的纯逻辑：开火节奏 / 挑最近的敌人 / **剑刃的运动**。
 * 不碰引擎（VM id 与 sprite 下标只当数字转手），也不做三角（旋转交给调用方的 `ce_polar_to_vel`），
 * 所以主机 `make test-host` 能直接测。
 *
 * ## 运动是**泰拉瑞亚原版复刻**（不是我们编的）
 *
 * 一手：`Terraria.exe`（用户本机 （本机 Steam 版实际 **1.4.5.8**；公式/表另用 1.4.4.5 的社区 decompile 对照，两版这块一致 —— 见 AUDIT §BE #213 追加 B））的 `Terraria.Projectile.AI_182_FinalFractal`
 * （Zenith 弹幕 = `ProjectileID.FinalFractal` 933）+ `Terraria.Graphics.FinalFractalHelper`，
 * 用 `dnfile` + `dncil` 反 IL 逐行读出（过程与对照见 `AUDIT.md` §BE 详录 #211）。逐条对位：
 *
 * | 原版（AI_182_FinalFractal） | 这里 |
 * | --- | --- |
 * | `localAI[0] += Lerp(0.7f, 2f, GetLerpValue(900f, 0f, velocity.Length()*2f, true))` | `rate`（`zn_launch` 算好）+ `phase += rate` |
 * | `if (localAI[0] >= 120f) Kill()` | `done` |
 * | `L = GetLerpValue(0,1, localAI[0]/60)` | `L = min(phase/60, 1)` |
 * | `ang = π + sign(velocity.X)·L·2π` | `out->ang` |
 * | `R = max(60, |velocity| + GetLerpValue(0.5,1,L)·40)` | `out->R` |
 * | `A = ai[0] · Lerp(2, 1, fast)`（`ai[0] ∈ [−100,100]`）| `out->A`（`ai0` 由调用方用 `ce_rand()` 抽）|
 * | `offset = (Vector2(1,0).RotatedBy(ang) · Vector2(R, A)).RotatedBy(velocity.ToRotation())` | 调用方用两次 `ce_polar_to_vel` 落成 `(R·cos ang, A·sin ang)` 再按 `heading` 旋转 |
 * | `fade = (1 − GetLerpValue(0,0.5,L)) · Vector2(sign·−R·0.1, −ai[0]·0.3)` | `out->fade_x / fade_y` |
 * | `Center = player.MountedCenter + velocity + offset + fade` | 调用方：`跟班当前位置 + (vx,vy) + offset + fade` |
 * | `rotation = ang + velocity.ToRotation() + π/2` | `out->rot` |
 * | `Opacity = GetLerpValue(0,5,t) · GetLerpValue(120,115,t)` | `out->alpha` |
 * | 命中：沿刃 80×20 的胶囊（`localNPCHitCooldown` 7）| `out->hit`（每 `ZN_HIT_COOLDOWN` 帧一次），判定盒 40×40 |
 * | 拖尾：4 个残影 + ribbon + 尘埃 | `out->emit_trail`（每 `ZN_TRAIL_EVERY` 帧一个残影 VM，颜色 = 这把剑的 trailColor）|
 *
 * ⚠ 关键性质（照原版，容易写错）：**`velocity` 不是每帧位移，而是出膛时定死的偏移**
 * （`Player.ItemCheck_Shoot`：`velocity = (目标 − 自机) / 2`，`UpdatePosition` 对 933 直接早退 ⇒ 从不积分）。
 * 所以剑既不飞也不减速：它**绕着「自机 + velocity」这个点画椭圆**，一圈正好在相位 60 走完，
 * 之后停在固定位置直到相位 120 消失 —— 这就是 wiki 说的 "in an arc returning to the player"。
 * 而且 `Center` 里那一项是**每帧重读的自机位置** ⇒ 剑阵跟着自机走。
 *
 * ## 本卡自己的旋钮（❓ 自定，登记在 `TUNING.md` §2.10）
 * 出膛周期 / 射程 / 弧长上限 / 池大小 / 伤害 / 命中冷却 / 残影间隔 / 视野边界。 */
#pragma once
#include <stdint.h>

/* —— 泰拉瑞亚一手（`AI_182_FinalFractal` / `SetDefaults`，逐字） —— */
#define ZN_PHASE_END      120.0f   /* localAI[0] >= 120 ⇒ Kill() */
#define ZN_PHASE_SWING     60.0f   /* 旋转相位用 localAI[0]/60 */
#define ZN_SPIN_MIN         0.7f   /* localAI[0] += Lerp(0.7f, 2f, fast) */
#define ZN_SPIN_MAX         2.0f
#define ZN_SPEED_REF      900.0f   /* fast = GetLerpValue(900f, 0f, |velocity|·2f, true) */
#define ZN_RADIUS_MIN      60.0f   /* R = max(60, |velocity| + GetLerpValue(0.5,1,L)·40) */
#define ZN_RADIUS_ADD      40.0f
#define ZN_FADE_IN          5.0f   /* Opacity = GetLerpValue(0,5,t) · GetLerpValue(120,115,t) */
#define ZN_FADE_OUT         5.0f
#define ZN_AI0_MAX        100.0f   /* ai[0] = Main.rand.Next(−100, 101) */

/* —— 本卡旋钮（❓ 自定） —— */
#define ZN_BLADE_MAX        8      /* 同时在飞的剑刃上限（池满就攒着不发，不丢已有的）*/
#define ZN_PERIOD          10      /* 帧：跟班两发之间（原版 = 一挥 3 把、每 10 tick 一把 ⇒ 6 把/秒）*/
#define ZN_RANGE          640.0f   /* 锁敌半径（原版 = 光标 400 px 内最近敌；我们的"光标"=跟班）*/
#define ZN_RANGE_SQ       (ZN_RANGE * ZN_RANGE)
#define ZN_ARC_MAX        640.0f   /* ★ 出膛点到目标的**最大距离**（= 2·|velocity|）。2026-10-03 owner：
                                    *   「天顶剑应当是能**打全图**的」—— 原来拍的 200 把甩距锁在 200 px 内，
                                    *   屏幕上大半的敌人都够不着。游玩区 384×448 的对角 ≈ **590** ⇒ 取 640
                                    *   （= `ZN_RANGE`，锁敌半径内一律够得着）。⚠ `velocity = 偏移/2` ⇒
                                    *   `|v| = 目标距离/2`，椭圆中心也在 `自机 + velocity` ⇒ 甩距**正好**等于
                                    *   目标距离（原版就是这么定的），所以这里只需放开这道钳位 */
#define ZN_TRAIL_EVERY      2      /* ★ 2026-10-03 owner 连提两次：「虚影密度可以再高一些」→「虚影可以再密」
                                    *   ⇒ 8 → 4 → **2 帧**（每把剑同时约 12 枚）。⚠ 更早那条「残留大量虚影」是
                                    *   **孤儿 VM 的 bug**（已修），修干净之后加密才是纯观感问题 */
/* ★★ 2026-10-03 owner：「刀的甩出去的**旋转和运动方向不太一致**，最后收到背后的虚影应当是**竖直朝下**的」。
 *   根因：原版 `AI_182` 的 `Projectile.rotation = radians + heading + π/2` 是**按弹幕贴图**推的 ——
 *   原版那把剑的弹幕贴图**正上朝**（base = −π/2：引擎 rotation=0 时剑尖指向 local −y），
 *   而我们的 21 把用的是 Terraria 的**物品贴图**，量出来**全是 45° 斜的**（剑尖在右上 ⇒ base ≈ −π/4）。
 *   ⇒ 画的时候要减掉这 45°：`rotation = radians + heading + π/2 − π/4`。
 *   ✅ 同时解释了"收尾为什么该竖直朝下"：L=1 时 `radians = π`（模 2π）、剑回到自机**背后 40 px**，
 *   正上目标（heading = −π/2）时 rotation = π − π/2 + π/2 = **π = 竖直朝下** ✓（这正是 owner 描述的观感；
 *   减 45° 之前是 135°，斜着 —— 就是"旋转和运动方向不一致"的那 45°）。*/
#define ZN_SPRITE_BASE      (-0.7853982f)   /* 贴图基准朝向（引擎 rotation=0 时剑尖的指向）= 右上 45° */
#define ZN_TRAIL_SPACING    9.0f  /* ★ 残影**按距离**发：每走过这么多 px 才留一枚（帧门 `ZN_TRAIL_EVERY`
                                    *   只是上限）—— 回收段 `|v|` 小也不会糊成一坨（owner 2026-10-03）*/
#define ZN_TRAIL_MIN_DIST   40.0f  /* ★ 同一条实测：「虚影集中在玩家背后十分明显」⇒ **离自机 40 px 内不留**
                                    *   （剑从自机身上出膛，前半程贴着自机 ⇒ 虚影糊在自机周围）*/
#define ZN_HIT_COOLDOWN     7      /* 帧：同一把剑两次伤害之间的最小间隔（原版 localNPCHitCooldown = 7）*/
#define ZN_HIT_LIFE         2      /* 伤害源寿命（一个源对同一敌人只结算一次）*/
#define ZN_HIT_W           40.0f   /* 判定盒：盖住刃（贴图 32–70 px × scale 0.55 ⇒ 18–39 px）❓ 自定 */
#define ZN_HIT_H           40.0f
#define ZN_DMG             20       /* 每次命中的伤害（原版 190，我们按弹幕区尺度缩放）❓ 自定 */
#define ZN_CAP            200       /* 有剑刃在飞时把「每敌每帧」上限抬到它（自机弹共用同一个上限）*/
#define ZN_VIEW_X         360.0f   /* 出界即收 */
#define ZN_VIEW_Y_MIN     (-80.0f)
#define ZN_VIEW_Y_MAX     520.0f

typedef struct {
    int32_t  active;      /* 1 = 这个槽在飞 */
    uint32_t id;          /* 剑刃 VM id（引擎侧句柄；core 只存不碰）*/
    uint32_t trail_id;    /* 最近一个残影 VM id（残影脚本自己 delete；这个只用于删卡时兜底）*/
    uint32_t ov_id;       /* ★ ×1.25 白色叠加 VM（原版辉光第二遍；随剑刃一起收）*/
    uint32_t sp_id;       /* ★ 刃尖星芒 VM（随剑刃一起收）*/

    int16_t  prof;        /* ★ 剑阵表下标 0..20（sprite / trailColor / bladeLength 都查表拿 ——
                           *   省下每格 6 字节：8 格 + 特效 VM 句柄刚好卡进 ce_state 的 496）*/
    uint8_t  sign_pos;    /* 1 = velocity.X > 0（原版 direction / 椭圆往哪边转）*/
    uint32_t trail_last;  /* ★ **上一枚残影的落点**（打包：x 高 16 / y 低 16，1/2 px 定点）——
                           *   残影改成**按距离**发（见 `ZN_TRAIL_SPACING`）：原来按帧发，回收段
                           *   `|v|` 小、同样帧数只走一点点 ⇒ 虚影在那一段糊成一坨（owner 2026-10-03）*/
    uint8_t  hit_timer;
    uint8_t  _pad;
    float    vx, vy;      /* 原版 velocity：出膛时定死的偏移（= (目标 − 出膛点)/2）*/
    float    heading;     /* v.angle（弧度，0 = +x）*/
    float    ai0;         /* 原版 ai[0] ∈ [−100, 100]（椭圆第二半轴）*/
    float    phase;       /* 原版 localAI[0] */
    uint16_t frames;      /* 只用于日志 */
} zn_blade_t;

/* 剑阵表的条数 = 原版 `FinalFractalHelper._fractalProfiles` 的条数（一手，见 AUDIT §BE #211）。
 * ★ 放这里是因为 **core 侧也要按 prof 查"这把剑的贴图基准朝向"**（`s_zn_base[]`）；
 *   卡侧的 `s_zn_profiles[]` 用同一个宏。*/
#ifndef ZN_PROFILE_N
#define ZN_PROFILE_N 21
#endif

/* 由 vx/vy 现算的派生量（不占字段：8 格 × 3 个 float 会把 ce_state 撑爆 ⇒ 卡状态上限 496 字节）。
 * **static inline 的理由同 `bc_atan2f`**：i386 ABI 返回 float 走 st0，内联掉才守得住零 x87。*/
/* 残影落点的打包 / 距离判定（1/2 px 定点，见 `trail_last`）。
 * **static inline 的理由同 `bc_atan2f`**：i386 下返回 float 的非内联函数会走 x87。*/
#define ZN_TRAIL_FIX     2.0f
static inline uint32_t zn_trail_pack(float x, float y)
{
    int32_t ix = (int32_t)(x * ZN_TRAIL_FIX), iy = (int32_t)(y * ZN_TRAIL_FIX);
    if (ix < -32768) ix = -32768;
    if (ix > 32767) ix = 32767;
    if (iy < -32768) iy = -32768;
    if (iy > 32767) iy = 32767;
    return ((uint32_t)(uint16_t)ix << 16) | (uint16_t)iy;
}
static inline int zn_trail_far_enough(const zn_blade_t *b, float x, float y)
{
    float lx, ly, dx, dy;
    if (b->trail_last == 0xffffffffu) return 1;              /* 第一枚 */
    lx = (float)(int16_t)(b->trail_last >> 16) / ZN_TRAIL_FIX;
    ly = (float)(int16_t)(b->trail_last & 0xffffu) / ZN_TRAIL_FIX;
    dx = x - lx; dy = y - ly;
    return (dx * dx + dy * dy) >= (ZN_TRAIL_SPACING * ZN_TRAIL_SPACING);
}

static inline float zn_dist2(float ax, float ay, float bx, float by)
{
    float dx = ax - bx, dy = ay - by;
    return dx * dx + dy * dy;
}
static inline float zn_vlen(const zn_blade_t *b) { return __builtin_sqrtf(b->vx * b->vx + b->vy * b->vy); }
static inline float zn_fast(const zn_blade_t *b)   /* clamp(GetLerpValue(900, 0, |v|·2)) */
{
    float t = (ZN_SPEED_REF - 2.0f * zn_vlen(b)) / ZN_SPEED_REF;
    return t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
}
static inline float zn_rate(const zn_blade_t *b)   /* Lerp(0.7, 2, fast) */
{
    return ZN_SPIN_MIN + (ZN_SPIN_MAX - ZN_SPIN_MIN) * zn_fast(b);
}
static inline float zn_sign(const zn_blade_t *b) { return b->sign_pos ? 1.0f : -1.0f; }


typedef struct {
    uint32_t  frames;
    uint32_t  charge;
    uint32_t  fired;
    zn_blade_t b[ZN_BLADE_MAX];
} zn_state_t;

/* 一帧的位姿参数。**三角不在这里** —— 调用方用 `ce_polar_to_vel` 落成世界坐标：
 *     ce_polar_to_vel(e, out->ang, 1.0f);   ex = out->R * e[0];  ey = out->A * e[1];
 *     ce_polar_to_vel(ch, b->heading, 1.0f);
 *     x = 跟班x + b->vx + (ex*ch[0] - ey*ch[1]) + out->fade_x;
 *     y = 跟班y + b->vy + (ex*ch[1] + ey*ch[0]) + out->fade_y; */
typedef struct {
    float ang;          /* 椭圆第一半轴的角（原版 num9）*/
    float R, A;         /* 两个半轴（原版 R / ai[0]·Lerp(2,1,fast)）*/
    float fade_x, fade_y; /* 原版的起始偏移（随 L 衰减到 0）*/
    float rot;          /* VM 的 rotation.z = ang + heading − base（已减掉这把剑的贴图基准）*/
    float tip;          /* ★ 剑尖指向（引擎角度 = ang + heading）—— 星芒/尘埃挂这一头 */
    int   mirror;       /* ★ 1 = 本帧要**水平翻转**贴图（原版 `spriteDirection = −1` 那支）*/
    int   alpha;        /* 0..255（原版 Opacity × 255）*/
    int   emit_trail;   /* 本帧该在当前位置留一个残影 */
    int   hit;          /* 本帧该出一次伤害（原版 localNPCHitCooldown）*/
    int   done;         /* 寿命尽（调用方收 VM）*/
} zn_pose_t;

/* 挑最近的（reset → 逐个 consider → ready）。距离平方比较，不开方。角度由调用方
 * `bc_atan2f(aim.dy, aim.dx)` 或直接喂给 `zn_launch`。 */
typedef struct {
    int   found;
    float best_d2;
    float dx, dy;      /* 目标 − 出膛点 */
} zn_aim_t;

void zn_reset(zn_state_t *s);
int  zn_charge(zn_state_t *s);          /* 每帧一次：frames++、充能；返回 1 = 蓄满 */
void zn_did_fire(zn_state_t *s);        /* 蓄满后真发了一把：清充能、计数 */
zn_blade_t *zn_slot(zn_state_t *s);     /* 找一个空槽；全满返回 0（调用方攒着）*/

/* 朝 (tx,ty) 甩一把：算 v / heading / sign / fast / rate（原版 AI 序言那几行）。
 * `ai0` 由调用方用 `ce_rand()` 抽（core 不产随机数，保持可单测）。*/
void zn_launch(zn_blade_t *b, uint32_t id, int32_t prof,
               float ox, float oy, float tx, float ty, float ai0);
/* 推进一帧：写一帧的位姿参数；寿命尽把 active 清 0（调用方据此收 VM）。*/
void zn_step(zn_blade_t *b, zn_pose_t *out);
int  zn_live(const zn_state_t *s);      /* 在飞数量（> 0 时调用方抬每帧伤害上限）*/

void zn_aim_reset(zn_aim_t *a);
void zn_aim_consider(zn_aim_t *a, float ox, float oy, float ex, float ey);
int  zn_aim_ready(const zn_aim_t *a);
