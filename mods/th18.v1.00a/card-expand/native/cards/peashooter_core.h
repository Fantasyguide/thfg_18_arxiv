/* peashooter_core.h —— 豌豆射手（id 77）的纯逻辑：开火节奏状态机。
 * 不碰引擎，主机 make test-host 直接测。
 *
 * 两种形态，都由**我们自己的计数器**当 `Player__tick_shooters_for_ability_card` 的 short_timer
 * （引擎那个 `player+0x476d4` 松开射击键会跳，相位不可控）：
 *   平时  —— 每 PEA_INTERVAL 帧一颗大豌豆（PVZ 原版是 1.425 s 一颗；本卡取整 1 秒）
 *   开大  —— 每 PEA_GAT_INTERVAL 帧一排 PEA_GAT_ROWS 颗小豌豆，持续 PEA_GAT_FRAMES 帧
 * 计时只在**射击帧**推进（`+0x1c` 本来就只在按住射击键时广播），所以「开大」不会在不开火时白烧。 */
#pragma once
#include <stdint.h>

#define PEA_INTERVAL      60    /* 帧：1 秒一颗（PVZ 原版是 1.425 s ≈ 85 帧，2026-09-08 用户改成整 1 秒）*/
#define PEA_DMG           90    /* 一颗大豌豆（sht 里的 damage，此处只作对账）→ 90 DPS */
#define PEA_GAT_INTERVAL   5    /* 帧：开大时每 5 帧一排 */
#define PEA_GAT_ROWS       7    /* 一排 7 颗，横向铺开 ±48 px */
#define PEA_GAT_DMG        7    /* 一颗小豌豆；一排同帧 49，稳在最低的每帧上限（Sakuya 60）之下 */
#define PEA_GAT_FRAMES   100    /* 开大持续帧数 → 20 排 × 7 = 140 颗 = 980 伤害 */
#define PEA_WAIT_MIN    1100    /* 帧：两次开大之间至少等这么久（射击帧计）*/
#define PEA_WAIT_SPAN    601    /* wait = MIN + rnd % SPAN ⇒ 周期 1200–1800 帧 = 20–30 s */
#define PEA_CAP          180    /* 有豌豆在途的帧把每敌每帧伤害上限抬到这里 = Reimu 基值 90 + 一颗 90：
                                   90 正好塞满 Reimu 的上限（自机弹一开火就把它挤掉）、又超出 Sakuya 的 60 */
#define PEA_INFLIGHT      40    /* 帧：一颗豌豆（14 px/帧）飞出弹幕区的余量，448/14 ≈ 32 帧 */

enum { PEA_SET_NORMAL = 0, PEA_SET_GATLING = 1 };

typedef struct {
    uint32_t timer;        /* 传给引擎当 short_timer；换形态时清 0，所以切进开大立刻就有第一排 */
    uint32_t wait;         /* 距离下次开大还剩几个射击帧 */
    uint32_t gat_left;     /* > 0 = 开大中，还剩几帧 */
    uint32_t inflight;     /* > 0 = 可能有豌豆在途（每帧 −1；开火那帧置 PEA_INFLIGHT）*/
    uint32_t bursts;       /* 开过几次大 */
    uint32_t peas;         /* 打出去几颗（日志用）*/
    uint8_t  pending_wait; /* 1 = 等调用方给一个随机数（见 pea_set_wait）*/
} pea_state_t;

typedef struct {
    int      set;          /* PEA_SET_NORMAL / PEA_SET_GATLING —— 本帧该用哪一组 shooterset */
    uint32_t timer;        /* 本帧传给引擎的 short_timer */
    int      fires;        /* 1 = 这一帧这组会出膛 */
    int      rows;         /* 本帧出膛几颗 */
    int      burst_start;  /* 1 = 开大就在本帧开始（放光晕 + 音效）*/
    int      burst_end;    /* 1 = 开大在本帧结束（之后要重新抽 wait）*/
} pea_out_t;

void pea_clear(pea_state_t *s);                        /* 局末 / 关店收子机：清干净并要一个新的 wait */
void pea_set_wait(pea_state_t *s, uint32_t rnd);       /* 调用方在 pending_wait 时用游戏 RNG 喂进来 */
void pea_shoot_frame(pea_state_t *s, pea_out_t *o);    /* 每个射击帧一次（要求 pending_wait == 0）*/
void pea_frame(pea_state_t *s);                        /* 每帧一次（含不射击的帧）：inflight 倒数 */
int  pea_needs_cap(const pea_state_t *s);
/* ★★ 2026-10-14（P40，owner 需求「电大哥」协同）：**电弹连锁**的选目标（纯函数 ✓，主机可测 ✓）。
 *   从 (cx, cy) 出发，在 n 个候敌里挑一个：**在半径 r 内**、**id 不在 visited 里**（去过的不再连 ✓）、
 *   且**离 (cx, cy) 最近**的那个 ⇒ 返回下标；没有合格的就返回 -1 ✓。
 *   依据：国服 PvZ2 电能豌豆「发射带有雷电条的豌豆 …… 同时可对**上下行造成电弧连锁伤害**」
 *   （社区攻略：小米游戏中心《电能豌豆综合玩法教学》）＋ owner 口径「**连接自身一个半径范围内的敌人，
 *   敌人还会再连接敌人**」✓ ⇒ 我们这里把"上下行"换成**半径**（TH18 是 360° ✓），逐跳传播 ✓。
 *   ⚠ 这里**只管挑谁**，不碰引擎（伤害/特效在 `peashooter.c` 里 ✓）。*/
/* ★★ P42（owner：「为了防止重复创建VM，从敌人A到敌人B的VM只创建一个」✓）：电弧**按敌人对去重**。
 *   一对敌（A,B）只允许存在一个电弧 VM ✓ —— 同对再次成链时**只刷新**已有那个（位置/角度/缩放 ✓），
 *   不新建 ✓；表满时**宁可不建**也不重复创建 ✓。key 用**无序对**（弧是 A↔B 同一根 ✓）。*/
/* ★★ P43（owner 裁定「**改成真穿透**」✓）：**卡侧电弹** —— 照 89 星星炮那套 ✓
 *   （卡自建 anm 弹 + 自己每帧挪 + 自己出定点伤害源 ⇒ **命中不销毁 = 真穿透** ✓）。
 *   ⚠ 数值必须与 `assets/sht/append_shooterset.py` 的 PEA_NORMAL / PEA_GATLING **两组对齐** ✓
 *     （那边决定"引擎豌豆"的行为 ✓，这边决定"卡侧电弹"的行为 ✓；改一处必须改两处 ✗）。*/
#define PE_MAX            64      /* ❓自定：同时最多几颗卡侧电弹（机枪一组 7 颗 ✓ ⇒ 有余量 ✓）*/
#define PE_SET_NORMAL      0
#define PE_SET_GATLING     1
#define PE_SPD_NORMAL    14.0f    /* = PEA_NORMAL.speed ✓ */
#define PE_SPD_GATLING   18.0f    /* = PEA_GATLING.speed ✓ */
#define PE_DMG_NORMAL     90      /* = PEA_NORMAL.damage ✓ */
#define PE_DMG_GATLING     7      /* = PEA_GATLING.damage ✓ */
#define PE_HIT_W_NORMAL  16.0f    /* = PEA_NORMAL.hit_w/h ✓ */
#define PE_HIT_W_GATLING 12.0f
#define PE_ELEC_SLOW     2.0f     /* ★★ P47（owner 逐字：「电弹的弹速需要比普通豌豆慢一倍」✓）
                                   *   电弹速度 = 该组默认速度 / 它（14→7 ✓、机枪 18→9 ✓）。
                                   *   ⚠ `PE_SPD_NORMAL/GATLING`（镜像 SHT 的 14/18）**不动** ✓ —— 那是引擎豌豆的 ✓。*/
#define PE_HIT_COOLDOWN   10      /* ★ 复刻原版默认 `localNPCHitCooldown = 10`（与 89 的 SC_HIT_COOLDOWN 同口径 ✓）
                                   *   ⇒ 一颗电弹穿过一个敌人是「每 10 帧 1 跳」而不是帧伤 ✓ */
#define PE_VIEW_X       360.0f    /* 出界即收（与 89 的 SC_VIEW_* 同口径 ✓）*/
#define PE_VIEW_Y_MIN   (-80.0f)
#define PE_VIEW_Y_MAX   520.0f

typedef struct {
    float    x, y;        /* 位置（px）*/
    float    vx, vy;      /* 速度（px/帧；屏幕坐标 y 向下 ⇒ 向上是 vy < 0 ✓）。★ P44：开大的扇形弹幕要方向 ✓*/
    int      set;         /* PE_SET_NORMAL / _GATLING ✓ */
    int      dmg;         /* 每次结算的伤害 ✓ */
    int      alive;
    uint32_t cd;          /* 还有几帧才允许再出一次伤害源 ✓ */
} pe_proj_t;

void pe_proj_clear_all(pe_proj_t *a, int n);                       /* 局末 / 删卡：全收 ✓ */
int  pe_proj_spawn(pe_proj_t *a, int n, float x, float y, float vx, float vy, int set, int dmg);
                                                       /* ★ P44：起一颗（显式方向/伤害 ✓）⇒ 下标；满了 = -1 ✓ */
int  pe_proj_new(pe_proj_t *a, int n, float x, float y, int set);
                                                       /* 便捷版：照那一组的默认速度/伤害、正上方 ✓ */
void pe_proj_move(pe_proj_t *p);                                   /* 一帧：挪 + 递减 cd ✓ */
int  pe_proj_hit_now(const pe_proj_t *p);                          /* 1 = 本帧该出伤害源 ✓ */
void pe_proj_fired(pe_proj_t *p);                                  /* 出过源 ⇒ 重置 cd ✓ */
int  pe_proj_out(const pe_proj_t *p);                              /* 1 = 出界 ⇒ 该收 ✓ */

uint32_t pea_arc_key(uint32_t a, uint32_t b);            /* 无序对 key（小 id 在前 ✓；a==b 也稳定 ✓）*/
int      pea_arc_find(const uint32_t *keys, int n, uint32_t key);   /* 找同对的槽 ⇒ 下标；没有 = -1 ✓ */
int      pea_arc_put(uint32_t *keys, uint32_t *vms, const uint8_t *alive, int n,
                     uint32_t key, uint32_t vm);          /* 写进"同对的槽"，否则第一个死的槽 ⇒ 下标；
                                                           * 全满且无同对 = **-1**（调用方据此不新建 ✓）*/
int  pea_chain_pick(const float *xy, const uint32_t *id, int n, float cx, float cy, float r,
                    const uint32_t *visited, int nv);              /* 1 = 本帧要抬每敌每帧伤害上限 */
