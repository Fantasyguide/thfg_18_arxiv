/* season_core.h —— 季节（TH16，card-expand id 116）的**纯逻辑**：档位阈值、五副季节释放档位表、消耗规则。
 * 纯 C11、**不含任何引擎地址**；主机单测 `tests/test_season_core.c`（`make test-host`）。
 *
 * 语义来源（一手，TH16 v1.00a）：`engine/season/th16/01-season-data-and-tables.md`
 *   §2.1 档位阈值 = `Player__init_season_level_deltas` `th16:0x43deb0` + seed 字节 `0x494810`/`0x494790`
 *   §3   10 张释放表 = `.rdata` 直读（`0x491e70`..`0x491fd4`），读点 `BombSubDoyou__begin` `th16:0x40e0f0`
 *   §5   `Bomb__activate_bomb` `th16:0x40db20` 的消耗规则（夏/土用只扣一档，其余清零）
 *
 * 无敌帧数来自社区攻略（THBWiki「东方天空璋/系统」，斜体自承为体感数据）与本仓反编译的交叉：
 * 春 = 释放结束后 60 帧（一手：`BombSubSpring__on_tick` 每帧把无敌刷到 50，效果结束后自然还剩 50 帧）；
 * 夏 9 / 土用 10（一手：`begin` 里置 `+0x1663c = 10`；攻略记 9）/ 秋 30 / 冬 30（攻略；🟡 未逐条反编译）。
 */
#pragma once
#include <stdint.h>

/* 副季节（TH16 `SUBSEASON` 枚举顺序：0 春 / 1 夏 / 2 秋 / 3 冬 / 4 土用）*/
#define SEASON_SUB_SPRING  0
#define SEASON_SUB_SUMMER  1
#define SEASON_SUB_FALL    2
#define SEASON_SUB_WINTER  3
#define SEASON_SUB_DOYOU   4
#define SEASON_SUB_COUNT   5

#define SEASON_LEVEL_MAX   6        /* 档位 0..6（6 个阈值 → 7 个值，与 7 项释放表对应）*/
#define SEASON_POINTS_MAX  1140     /* `MAX_SEASON_POWER`（TH16 `0x4a580c` 运行时 = requirements[6]）*/

typedef struct {
    int32_t duration;               /* 释放持续帧数 */
    float   radius;                 /* 清弹半径（px）*/
    int32_t damage;                 /* 每帧伤害 */
    int32_t invuln;                 /* 无敌帧（从按下起算）*/
    int32_t consume_one_level;      /* 1 = 只扣一档（夏/土用）；0 = 槽清零（春/秋/冬）*/
} season_profile_t;

/* 累计季节点 → 档位 0..6 */
int32_t season_level_for_points(int32_t points);

/* 档位 L 的阈值（L=0 → 0；L=6 → 1140）*/
int32_t season_points_for_level(int32_t level);

/* 取某副季节在某档位下的释放参数；level 会夹到 0..6，level 0 的表项恒为 0 */
void season_profile(int subseason, int level, season_profile_t *out);

/* 副季节名（ASCII，给 HUD / 日志用：游戏 ASCII 字体不吃 UTF-8 中文）*/
/* Spring / Summer / Fall / Winter / Doyou */
const char *season_name(int subseason);

/* 释放后剩余季节点（按 TH16 的消耗规则；越界输入一律夹住）*/
int32_t season_points_after_release(int subseason, int32_t points);

/* —— 解放特效（两圈）的外观参数 —— 一手：TH16 各副季节 anm 的「释放脚本」字节。
 * 每个副季节的 `BombSub*__begin` 建两个 VM，脚本号各不同（`pl00sub/pl02sub/pl03sub/pl04sub` = 3/4，
 * **冬是 `pl01sub` 的 20/21**，`th16:0x410150` 的两处 `AnmLoaded__create_40e5c0` 实参）。
 * 两个脚本都是 `drawPoly(1.0f, N)` + `blendMode(2)`（外圈，减算）/ `blendMode(1)`（内圈，加算）。*/
typedef struct {
    uint32_t outer_c1, outer_c2;    /* 外圈 color / color2（D3DCOLOR 0xAARRGGBB；alpha 由调用方按淡出曲线覆盖）*/
    uint32_t inner_c1, inner_c2;    /* 内圈 color / color2 */
    int32_t  inner_poly;            /* `drawPoly(1.0f, N)` 的 N：春 = 32，其余 = 12 */
    float    spin;                  /* `angleVel` 每帧自转（rad）：外圈 +spin、内圈 −spin；春 = 0 */
    int32_t  alpha_out;             /* 外圈脚本里的 alpha（恒 255）*/
    int32_t  alpha_in;              /* 内圈脚本里的 alpha（春 255、其余 128）*/
    float    drift_y;               /* 土用：`posTime(60, 1, 0, 48, 0)` = 60 帧上浮 48 px；其余 0 */
} season_ring_t;

/* 取某副季节的解放特效外观（subseason 会夹到 0..4）*/
void season_ring(int subseason, season_ring_t *out);

/* 子机阵位：TH16 副 sht 的 option_pos（相对自机的 px 偏移）。
 * 索引 = 三角数[档位-1] + i（TH16 `th16:0x4445fb`：表基址 `th16:0x4a5db0 + SUBSEASON*0x20`，值 {0,1,3,6,10,15,21,28}）
 * ⇒ **每一档一套独立阵型**；TH16 里非聚焦/聚焦两张表数值相同，这里只存一份。
 * 返回 0 = 档位/序号越界。*/
typedef struct { float x, y; } season_opt_pos_t;
int season_option_pos(int subseason, int level, int i, season_opt_pos_t *out);

/* 副季节循环（S 键切季节用）*/
static inline int season_next(int subseason) { return (subseason + 1) % SEASON_SUB_COUNT; }

/* —— 敌人掉落的**计时衰减**（TH16 `dropSeason` + `dropItems`）——
 * 一手 `th16:0x420585`（`dropItems` opcode 509 的 handler，死亡/调用那一刻结算）：
 *     if (bonus_timer.current < 1) n = M;
 *     else n = (N − M) * bonus_timer.current / T + M;
 * 配套 `dropSeason(T,N,M)`（`th16:0x422a26`）把计时器设成 T、`extra_counts[15] = N`、`min_count = M`。
 * ⇒ 出场后 T 帧内击破给满 N 个，之后线性降到 M 个（整数除法，与引擎同）。
 * `alive_frames` = 该敌人已经活了多少帧（TH18 用 `zEnemyData.time_alive.current`，
 * 见 engine.h 的 `CE_ED_TIME_ALIVE`）；负数按 0 算。*/
int32_t season_drop_count_decay(int32_t alive_frames, int32_t time_frames,
                                int32_t max_count, int32_t min_count);

/* —— 逐敌人的**档位**：TH16 的 `(T,N,M)` 是 ZUN 逐敌人手写的（小妖精 `(60,8,1)`、妖精 `(60,10,1)`、
 * 大妖精 `(120,13,1)`/`(120,20,1)`、大只 `(200,30,1)`、st06 巨物 `(600,100,1)`、boss `(1200,200,10)`），
 * TH18 里拿不到这张表 —— 但 TH18 **自己有一套同形的逐敌人档位**：`zEnemyDrop.timed_ex_drop_time`（窗口 T）
 * + `timed_ex_drop[type]`（数量），由 ECL opcode 573/574 写在每个敌人的子程序开头，
 * 一手见 [`engine/season/th18/01`](../../../../../engine/season/th18/01-season-residuals.md) §1b′。
 * 这里把「原版 timed 计数」当**档位键**映射回 TH16 的数量量级（T 直接用原版值）：
 *   1 → 8（TH16 小妖精）/ 2..4 → 10（妖精）/ 5..10 → 13（大妖精）/ ≥11 → 30（大只；含道中 boss 的 15/20）
 * ⚠️ 这个映射本身是**自定**（两边数字都一手，但跨作对应关系没有原版依据）—— 见 CARDS.md 的标注。*/
int32_t season_drop_native_tier(int32_t native_timed_count);
