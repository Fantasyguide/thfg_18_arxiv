/* star_cannon_core.h —— 星星炮（id 89）的纯逻辑：开火节奏 / 瞄准 / 星星弹幕的运动。
 * 不碰引擎（VM id 只当数字转手），也不做三角（方向由调用方的 `ce_polar_to_vel` 落成 vx/vy），
 * 所以主机 `make test-host` 能直接测。
 *
 * ## 数值全部是泰拉瑞亚**原版复刻**（一手：反编译 1.4.4.5 的 `Item.cs` / `Projectile.cs`，
 *    物品号与弹幕号是从用户本机 1.4.5.8 的 exe 常量表读出来的）
 *
 * | 原版 | 值 | 这里 |
 * | --- | --- | --- |
 * | `ItemID.StarCannon` | **197**（`_src/Item_197.png`，`width=50/height=18`）| 跟班贴图 `STAR_CANNON`（script381）|
 * | `useTime` / `useAnimation` | **12** / **12**（`autoReuse = true`）| `SC_PERIOD`（=5 发/秒，**白板无加成**）|
 * | `shoot` | `ProjectileID.StarCannonStar` = **955** | 弹幕贴图 `STAR_CANNON_SHOT`（script382）|
 * | `shootSpeed` | **14** | `SC_SPEED`（px/帧）|
 * | `damage` | 55 | ❓ 按弹幕区尺度缩到 `SC_DMG`（登记 `TUNING.md`）|
 * | `UseSound` | `SoundID.Item9` | 自建语音槽 `CE_VOICE_STAR_CANNON_SHOT`（**从用户本机的 `Item_9.xnb` 现取**）|
 * | 弹幕 `SetDefaults` | `width=height=18`、`aiStyle=5`、`penetrate=-1`（无限穿透）、`alpha=50`、`light=1` | 判定盒 / 穿透 / 闪烁都照它 |
 * | `aiStyle 5` 运动 | 只有 `rotation += (\\|vx\\|+\\|vy\\|)·0.01·direction`（= 14·0.01 = **0.14 rad/帧 ≈ 8°/帧**）| `SC_SPIN` |
 * | `aiStyle 5` 闪烁 | `alpha += 25·localAI[0]`，到 200 翻 −1、到 0 翻 +1（≈16 帧一个来回）| `SC_ALPHA_*`（画的时候取 `255 − alpha`）|
 * | 出界 | `timeLeft` 3600 / 撞方块即消失 | `SC_VIEW_*`（我们的弹幕区没有方块）|
 *
 * ⚠ **原版没有瞄准这回事**（玩家拿鼠标指哪打哪）。我们的弹幕区没有光标 ⇒ ❓自定：
 *   锁**最近的敌人**（`SC_RANGE`），没有敌人时朝**正上**（`−π/2`，与自机射击同向）。
 * ⚠ 原版的星星要**消耗坠落之星弹药**（item 75，卡名那句「桑百颗够吗？」就是在问这个）。
 *   本卡**不设弹药**（❓自定，登记 `TUNING.md`）—— 要加计数就说一声，池与判据都已就位。 */
#pragma once
#include <stdint.h>

/* —— 泰拉瑞亚一手（逐字） —— */
#define SC_PERIOD       12       /* useTime = useAnimation = 12（白板无加成）*/
#define SC_SPEED        14.0f    /* shootSpeed = 14 */
#define SC_SPIN         0.14f    /* (|vx|+|vy|)·0.01 = 14·0.01 */
#define SC_ALPHA_STEP   25       /* alpha += 25·localAI[0] */
#define SC_ALPHA_MAX    200      /* alpha 上界（下界 0）*/
#define SC_HIT_SIZE     14.0f    /* ❓ 判定盒（原版 width=height=18；贴图缩到一半后按同比例收到 14）*/

/* —— 本卡旋钮（❓ 自定） —— */
#define SC_SHOT_MAX     12       /* 同时在飞的星星上限（❓自定；`ce_state` 只有 496 字节）*/
#define SC_RANGE        640.0f   /* 锁敌半径 */
#define SC_RANGE_SQ     (SC_RANGE * SC_RANGE)
#define SC_HIT_LIFE     2        /* 伤害源寿命（一个源对同一敌人只结算一次）*/
/* ★★ 2026-10-03 owner 口径：「星星炮是**无限穿透**的」+「务必**不要做成帧伤**」⇒ 两条都在这里定死：
 *   ① **无限穿透** = 命中不消耗：核心**没有任何 pierce 计数**，`sc_step()` 只在出界时返回 1
 *      （对位原版 `Projectile.SetDefaults` 的 `penetrate = -1`）；
 *   ② **不是帧伤** = 伤害源只每 `SC_HIT_COOLDOWN` 帧出一次（下一条），而**单条**伤害源对同一敌人
 *      只结算一次（`SC_HIT_LIFE` + 引擎侧 `enemy+0x80` 的命中间隔，见 `engine/player/th18/02-damage-sources.md`）
 *      ⇒ 一颗星穿过一个敌人是「每 10 帧 1 跳」而不是每帧一跳。单测把这两条都钉住了。*/
#define SC_HIT_COOLDOWN 10       /* ★ 复刻原版**默认的 `localNPCHitCooldown = 10`**：同一颗星
                                  * 对同一个敌人每 10 帧才结算一次（无限穿透但要吃本地免疫）*/
#define SC_DMG          12       /* 每次命中的伤害（原版 55）*/
#define SC_CAP          200      /* 有星星在飞时把「每敌每帧」上限抬到它 */
#define SC_VIEW_X       360.0f   /* 出界即收 */
#define SC_VIEW_Y_MIN   (-80.0f)
#define SC_VIEW_Y_MAX   520.0f

/* ★ 结构体**压到 32 字节**：`ce_state` 只有 496 字节、池要给 12 格（第一版 40 字节 × 12 超了）。*/
typedef struct {
    uint32_t id;          /* VM 句柄（core 只存不碰）*/
    float    x, y;        /* 位置（世界坐标）*/
    float    vx, vy;      /* 速度（**定死**：原版 aiStyle 5 不加速、不重力）*/
    float    rot;         /* rotation.z（复刻那个 0.14/帧的自转）*/
    int16_t  alpha;       /* 原版那个 alpha（0..200）；画的时候用 255−alpha */
    int8_t   alpha_dir;   /* localAI[0]（±1）*/
    uint8_t  active;
    uint8_t  hit;         /* 本帧该出一次伤害（`sc_step` 写）*/
    uint8_t  hit_timer;
    uint16_t frames;
} sc_shot_t;

typedef struct {
    uint32_t frames;
    uint32_t charge;
    uint32_t fired;
    sc_shot_t s[SC_SHOT_MAX];
} sc_state_t;

void sc_reset(sc_state_t *s);
int  sc_charge(sc_state_t *s);          /* 每帧一次；返回 1 = 该开火了（原版 autoReuse 的节奏）*/
void sc_did_fire(sc_state_t *s);
sc_shot_t *sc_slot(sc_state_t *s);      /* 空槽；满则 0（攒着不发，不丢已有的）*/
int  sc_live(const sc_state_t *s);

/* 出膛：`x/y` = 炮口，`vx/vy` = 方向 × SC_SPEED（调用方用 `ce_polar_to_vel` 算好）。*/
void sc_launch(sc_shot_t *sh, uint32_t id, float x, float y, float vx, float vy);
/* 推进一帧：位移 + 自转 + 闪烁 + 出界判定 + 免疫计时。返回 1 = 该收了（调用方删 VM）。
 * `sh->hit` = 本帧该不该出伤害源（照原版默认的 `localNPCHitCooldown = 10`）。*/
int  sc_step(sc_shot_t *sh);
/* 这一帧该给 VM 写的颜色 alpha（= 255 − 原版 alpha）*/
int  sc_draw_alpha(const sc_shot_t *sh);

typedef struct {
    int   found;
    float best_d2;
    float dx, dy;      /* 目标 − 炮口 */
} sc_aim_t;

void sc_aim_reset(sc_aim_t *a);
void sc_aim_consider(sc_aim_t *a, float ox, float oy, float ex, float ey);
int  sc_aim_ready(const sc_aim_t *a);
