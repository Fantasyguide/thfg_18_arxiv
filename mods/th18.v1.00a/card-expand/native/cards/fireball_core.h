/* fireball_core.h —— 火球术（id 83，致敬炉石传说）的纯逻辑：一颗直上的火球 + 命中即爆。
 * 不碰引擎、不产随机数、无随机，主机 `make test-host` 直接测。
 *
 * ★ **火球本体与爆炸的数值一律复用炎魔之王的**（用户口径「炎魔之王同款火球」）：这里直接 include
 * `firelord_core.h` 用它的 `FL_BLAST_*` / `FL_BALL_CANCEL_R` / `FL_PARTICLES` / `FL_BURST` / `FL_TRAIL_EVERY`，
 * 不另抄一份常数 —— 一处改，两张卡一起动。特效脚本也是炎魔那几个（`ability.anm` 92/93/94/95/96），
 * 所以这张卡**不需要新美术、不用重编 `ability.anm`**。
 * 只有「怎么飞」是这张卡自己的：从自机正上方出发、直线向上、碰到第一个敌人就地爆炸。 */
#pragma once
#include <stdint.h>

#include "firelord_core.h"

#define FB_RECHARGE     480      /* 8 s 充能 = 这张卡唯一的代价（炉石里是 4 费；我们不扣火力也不扣命）*/
#define FB_SPEED        8.0f     /* px / 帧，向上。炎魔的 4.0 是「慢慢飘向敌人」的演出节奏，
                                  * 自机正前方打出去要跟手：448 px 高的弹幕区约 56 帧穿完 */
#define FB_SPAWN_DY     (-24.0f) /* 出生点在自机上方一点，免得第一帧就压在自机身上 */
#define FB_HIT_R        32.0f    /* 命中判定：火球中心与敌人中心的距离。🟡 敌人碰撞箱尺寸本仓还没查，
                                  * 先用固定半径近似（炎魔选目标也只用敌人中心坐标）*/
/* 出界自毁：**四条边都查**。这颗火球只会直上（x 不变、y 只减），照理只可能从顶边出去 ——
 * 但「以防万一」值这几行：以后要是给它加了横向速度 / 追踪 / 反弹，或者坐标被别的东西改坏，
 * 它都不会飞在场外无限占着主动态（那样这张卡就再也按不动了）。
 * 坐标系：弹幕区 384 × 448，x = 0 在上边框中点、x ∈ [−192, 192]，y 从顶边 0 起算向下为正
 * （`engine/anm/th18/01-vm-instantiate.md` §「场地正中 = (0, 224)」）。*/
#define FB_FIELD_X_HALF 192.0f   /* 弹幕区半宽 */
#define FB_FIELD_H      448.0f   /* 弹幕区高 */
#define FB_EXIT_MARGIN  40.0f    /* 越过边框这么多才算飞出去（让它整颗飞出画面再消失，不是贴着边就没）*/
#define FB_MAX_FRAMES   240      /* 保险的保险：4 s 还没命中也没出界就收摊（坐标被钉住不动时不至于永远占着主动态）*/

typedef struct {
    int      active;         /* 火球在飞 */
    float    x, y;           /* 火球坐标 */
    uint32_t frames;         /* 发射以来的帧数 */
    float    ex, ey;         /* 爆点 = 命中那一刻的火球坐标 */
    uint32_t blast_left;     /* 爆炸剩余帧 */
    uint32_t cancelled;      /* 统计：飞行中消掉的弹数（引擎侧写，core 不碰）*/
    uint32_t anm_id;         /* 火球 VM（引擎侧，core 不碰）*/
} fb_state_t;

typedef struct {
    int trail;       /* 本帧在 (x, y) 留一个拖尾 */
    int hit;         /* 本帧命中：删火球 VM、起爆炸特效 + 音效、消弹 */
    int missed;      /* 本帧飞出界 / 超时：删火球 VM，什么都不炸 */
    int blast_dmg;   /* 非 0 = 本帧在 (ex, ey) 放一个 blast_dmg 的伤害源 */
    int done;        /* 本帧之后整张卡的持续态结束 */
} fb_step_t;

void      fb_launch(fb_state_t *s, float px, float py);
/* 每帧推进一步。`hit` = 引擎侧这一帧判出的命中（火球坐标 FB_HIT_R 内有可锁定敌人）。
 * 顺序：先走爆炸倒计时（命中之后火球已经没了），再推进飞行。*/
fb_step_t fb_step(fb_state_t *s, int hit);
/* 命中判据（纯数学，引擎侧对每个敌人调一次）：中心距 < FB_HIT_R。*/
int       fb_in_range(float bx, float by, float ex, float ey);
/* 出界判据：四条边任一越过 FB_EXIT_MARGIN。*/
int       fb_offscreen(float x, float y);
