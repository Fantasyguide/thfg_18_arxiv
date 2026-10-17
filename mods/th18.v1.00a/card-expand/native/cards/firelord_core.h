/* firelord_core.h —— 炎魔之王拉格纳罗斯（id 72）的纯逻辑：生命 / 8 秒一次的随机移动 / 火球飞行与爆炸。
 * 不碰引擎、不产随机数（随机 dword 由调用方从游戏 RNG 取来喂进来），主机 make test-host 直接测。
 * 设计：docs/superpowers/specs/2026-09-06-firelord-design.md。 */
#pragma once
#include <stdint.h>

#define FL_HP             400      /* 9b623ff1 平衡：800 → 400（爆炸仍是 12 × 100 = 1200，与火球术共用）*/
#define FL_POWER_COST     200      /* 2.00 火力 */
#define FL_POWER_GATE     300      /* 门槛 3.00：引擎 spend_power 永远保留 1.00，成本 + 一档才扣得实（原生 Tsukasa 同款：成本 1 档、门槛 2 档）*/
#define FL_RECHARGE       600      /* 10 s，与青眼同 */
#define FL_RADIUS         33.6f    /* 挡弹半径：本体约 64 px 高；2026-09-06 用户：先减半到 28，再 +20％ */
#define FL_PERIOD         420      /* 帧：每 7 s 一个周期（移动 → 到位开火）；2026-09-06 用户：8 s → 7 s */
#define FL_INTRO_FRAMES   60       /* 登场：语音起、目标位置聚 1 s 粒子，然后本体 + 第一圈爆炸 */
#define FL_INTRO_RINGS    3        /* 登场爆炸环次数 */
#define FL_RING_GAP       8        /* 帧：相邻两圈的间隔 */
#define FL_GATHER         6        /* 登场聚气：每帧几颗向中心汇聚的粒子（script98）*/
#define FL_MOVE_FRAMES    60       /* 一次移动滑 60 帧（quintic ease-in-out：起步慢、中段快、落定慢）*/
#define FL_SUMMON_DY      (-64.0f) /* 召唤在自机上方（再钳进落点范围）*/
#define FL_X_MIN          (-160.0f) /* 随机落点范围：弹幕区**下 1/3**（实体坐标：x 居中、y 从顶边起算，区高 448 ⇒ 下 1/3 从 299 起）*/
#define FL_X_MAX          160.0f
#define FL_Y_MIN          300.0f
#define FL_Y_MAX          410.0f    /* 离底边留 38 px：本体半高 32 + 血条 */
#define FL_BOB_PERIOD     96        /* 帧：待命时上下呼吸浮动一个来回 */
#define FL_BOB_AMP        4.0f      /* px：浮动半幅（本体 64 px 高）*/
#define FL_BALL_SPEED     4.0f     /* px / 帧（2026-09-06 用户：放慢）：满场 ~600 px 也在 150 帧内到，仍远小于 480 的周期 ⇒ 场上最多一颗 */
#define FL_BLAST_FRAMES   12       /* 爆炸持续帧：每帧一个新伤害源（一个源对同一敌人只结算一次）。2026-09-06 用户：伤害加大 → 8 → 12 帧 */
#define FL_BLAST_DMG      100      /* 每帧；12 × 100 = 1200（2026-09-06 用户：加强）。★ 只有配合 FL_BLAST_CAP 才是实数：
                                    * 自机弹与我们的源共用「每个敌人每帧」的总伤害上限 player+0x47984（Reimu 90 / Marisa 160 / Sakuya 60 / Sanae 120），
                                    * 自机对着 boss 开火时配额已被吃满，不抬上限火球只能补差额（此前 50 × 12 名义 600 实际接近 0）*/
#define FL_BLAST_CAP      300      /* 爆炸帧把 player+0x47984 抬到它（Remilia 0x40f48e 同款值；GameThread 每帧复位，只对当帧有效）。300 − 任何角色的自机弹上限 ≥ 140 > 100 ⇒ 火球整份落在自机弹之上 */
#define FL_BLAST_CANCEL_R 96.0f    /* 落地那帧消弹半径（= 杀伤框边长；弹变点道具）*/
#define FL_BALL_CANCEL_R  24.0f    /* 飞行中每帧在火球坐标消弹的半径（开路）*/
#define FL_CANCEL_MAX     9999     /* 消弹数量不限（与本体挡弹的 hp 上限无关：各自一次调用、各自读计数器）*/
#define FL_BLAST_W        96.0f    /* 杀伤范围 64 → 96（2026-09-06 用户）*/
#define FL_BLAST_H        96.0f
#define FL_PARTICLES      2        /* 飞行中每帧撒几颗橙白粒子（script95，各自随机飘散）*/
#define FL_BURST          10       /* 落地那帧撒几颗火星（script96）*/
#define FL_AURA           2        /* 常态：本体每帧撒几颗身体火焰（script97，与火球粒子同量）*/
#define FL_SHAKE_TIME     14       /* 震屏帧数（ECL setScreenShake 同款机制；原生 boss 死亡量级更大）*/
#define FL_SHAKE_START    5        /* 起始强度（相机像素偏移）*/
#define FL_SHAKE_END      0
#define FL_TRAIL_EVERY    2        /* 飞行中每 2 帧留一个拖尾 */
#define FL_BAR_DY         40.0f    /* 血条中心相对本体中心的 y（本体下端约 +32）*/
#define FL_BAR_W          40.0f
#define FL_BAR_H          4.0f

typedef struct {
    int32_t  hp;
    uint32_t frames;        /* 召唤以来的帧数 */
    uint32_t blocked;       /* 累计挡弹 */
    uint32_t moves;         /* 已移动次数 */
    uint32_t shots;         /* 已投出火球数 */
    float    x, y, z;       /* 本体坐标 */
    float    sx, sy;        /* 本次移动的起点 */
    float    tx, ty;        /* 本次移动的终点 */
    uint32_t move_left;     /* 本次移动剩余帧；0 = 静止 */
    int      fire_pending;  /* 到位后要开火 */
    /* 火球（最多一颗）*/
    int      ball_active;
    float    bx, by;        /* 火球坐标 */
    float    bvx, bvy;      /* 每帧位移 */
    float    bang;          /* 飞行方向（弧度，0 = +x）*/
    uint32_t ball_left;     /* 剩余飞行帧 */
    float    ex, ey;        /* 落点 = 开火时记下的敌人坐标 */
    uint32_t blast_left;    /* 爆炸剩余帧 */
    uint32_t ball_cancelled, blast_cancelled;   /* 统计：飞行中 / 落地消掉的弹数（引擎侧写，core 不碰）*/
    /* 登场 */
    uint32_t intro_left;    /* 聚气剩余帧；> 0 时本体还没出现，fl_step 只走登场分支 */
    uint32_t rings_left;    /* 还要放几圈爆炸环 */
    uint32_t ring_timer;    /* 距下一圈的帧数 */
    /* ANM ids（引擎侧，core 不碰）*/
    uint32_t anm_id, ball_id, bar_bg_id, bar_id;
} fl_state_t;

typedef struct {
    int gather;         /* 登场聚气中：本帧在 (x, y) 起 FL_GATHER 颗汇聚粒子 */
    int appear;         /* 本帧本体出现：起本体 VM / 血条 + 第一圈爆炸（含火星、震屏）*/
    int ring;           /* 本帧再放一圈爆炸环（第 2、3 圈）*/
    int need_move;      /* 该抽随机落点了：调用方 fl_begin_move(s, ce_rand()) */
    int fire;           /* 到位了：调用方选目标 → fl_launch(s, tx, ty)（没目标就不调）*/
    int trail;          /* 本帧在 (bx, by) 留一个拖尾 */
    int ball_arrived;   /* 火球本帧到点：删火球 VM、起爆炸特效、放 0x2c */
    int blast_dmg;      /* 非 0 = 本帧在 (ex, ey) 放一个 blast_dmg 的 FL_BLAST_W×H 伤害源 */
    int died;
} fl_step_t;

void      fl_summon(fl_state_t *s, float px, float py, float pz);     /* 清零、hp = FL_HP、坐标 = (px, py + FL_SUMMON_DY) 钳进落点范围、intro_left = FL_INTRO_FRAMES */
fl_step_t fl_step(fl_state_t *s, int blocked);                          /* 每帧：登场分支（聚气 → 出现 → 三圈）；之后扣血、计帧、移动插值、火球推进、爆炸计数 */
void      fl_begin_move(fl_state_t *s, uint32_t rnd);                   /* 用一个随机 dword 抽落点、开始滑动、到位后开火 */
void      fl_launch(fl_state_t *s, float tx, float ty);                 /* 朝 (tx, ty) 投火球：直线、FL_BALL_SPEED、到点即爆 */
void      fl_spot_from_rand(uint32_t rnd, float *x, float *y);          /* 低 16 位 → x、高 16 位 → y，均匀落在范围内 */
uint32_t  fl_pick_index(uint32_t rnd, uint32_t n);                      /* n > 0：rnd % n */

/* 呼吸浮动：画面 y = s->y + 它（判定 / 血条一起跟）。三角相位 → smoothstep 折成来回 = 一条 C¹ 连续的近似正弦，不引 libm。
 * frames = 0（召唤那帧）在最低点 −AMP，之后先上浮。**static inline 的理由同 bc_atan2f**：i386 ABI 返回 float 走 st0（flds），
 * 内联掉才守得住 make dllx87 的「零 x87」。*/
static inline float fl_bob_dy(const fl_state_t *s)
{
    uint32_t ph = s->frames % FL_BOB_PERIOD;
    float t = (float)ph / ((float)FL_BOB_PERIOD * 0.5f);   /* [0, 2) */
    if (t > 1.0f) t = 2.0f - t;                            /* 三角波 */
    float k = t * t * (3.0f - 2.0f * t);                   /* smoothstep：两端速度为 0 */
    return (k * 2.0f - 1.0f) * FL_BOB_AMP;                 /* [−AMP, +AMP] */
}
