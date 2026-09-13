/* animal_core.h —— 鬼形兽（TH17 动物灵 / 咆哮，card-expand id 117）的**纯逻辑**：
 * 5 格队列、就绪判定与时长、就绪窗口计时与延长、霊撃时间轴、结束结算、type 分类。
 * 纯 C11、**不含任何引擎地址**；主机单测 `tests/test_animal_core.c`（`make test-host`）。
 *
 * 语义来源（一手，TH17 v1.00b）：`engine/animal/th17/01-animal-spirits.md`
 *   §5.1 字段表（HYPER_FILL / TOKENS[5] / pending / HYPER_TIME / hyper_type / hyper_extend）
 *   §5.2 就绪 `FUN_0040fb60` `th17:0x40fb60`：数 1/2/3 的出现次数；≥3 → 单一类型 + 时长表；否则混合
 *   §5.3 霊撃 `GoastManager::do_hyper_cancel` `th17:0x40f6d0`：40 帧、半径 `(1 − f⁴) × 200`
 *   §5.4 结束 `FUN_00410640` `th17:0x410640`：把队列里的鱼灵换成道具
 *   §4.3 寿命归零 → `type % 3 + 1`（1→2→3→1）
 *
 * ⚠️ 本卡在 TH18 里是**自建**（TH18 无 GoastManager / zToken / HYPER_*，审计
 * `engine/animal/th18/01-animal-residuals.md`）。数值照抄 TH17，接缝走 card-expand SDK。
 *
 * ★ 2026-09-10 用户定案（Stage 2）：
 *   · 「同类 == 凭依兽灵 → 时长 +480」**已按原版接上**（2026-09-10 二次定案）——TH18 没有凭依选择，
 *     卡侧一律写 `ANIMAL_GOAST_NONE`（= TH17 的「主凭依灵 ≠ 本凭依灵」，任何暴走都不吃 +480）；
 *   · **不做**稀有动物灵（8..14）的爆散 —— 咆哮正常结束只放出 **2 个变色动物灵**；
 *   · 咆哮中收动物灵延长 **90 / 60 / 42 帧**（其后递减，用户口径；TH17 代码是 180 起步 ×2/3
 *     ⇒ 120/80/53，见 `animal_next_extend` 的注释）；收鱼灵**立刻换道具**、不进队列。 */
#pragma once
#include <stdint.h>

#define ANIMAL_SLOT_MAX        5      /* TH17 TOKENS[5]：存 type 的 5 格队列，不是 5 类计数 */
#define ANIMAL_PENDING_MAX     5      /* TH17 pending[5]：霊撃期间的溢出队列 */

#define ANIMAL_TYPE_MIN        1      /* token type 1..14（15..17 是 1..3 的「长寿命/变色」变体）*/
#define ANIMAL_TYPE_MAX        14
#define ANIMAL_TYPE_GOAST_MIN  1      /* 1 オオカミ霊 / 2 カワウソ霊 / 3 オオワシ霊 */
#define ANIMAL_TYPE_GOAST_MAX  3
#define ANIMAL_TYPE_FISH_MIN   4      /* 4 炸弹碎片 / 5 残机碎片 / 6 火力 / 7 点 —— 道具鱼灵 */
#define ANIMAL_TYPE_FISH_MAX   7
#define ANIMAL_TYPE_RARE_MIN   8      /* 8..14 稀有动物（本卡不做，字段保留）*/
#define ANIMAL_TYPE_RARE_MAX   14

#define ANIMAL_HYPER_FRAMES    40     /* 霊撃本体 40 帧（th17:0x40f6d0 置 {39,40,40.0}）*/
#define ANIMAL_GOAST_BONUS     480    /* TH17 `th17:0x40fbe5`–`0x40fbed`：多数类 == 主凭依灵 → +0x1e0 帧 */
/* `core.goast` 的三种取值（主凭依灵 = TH17 每个自机固定绑定的兽灵，如霊夢A = オオカミ霊）：
 *   · 0..2 = 指定主凭依灵（主机单测用）
 *   · ANIMAL_GOAST_NONE = **不存在** —— TH18 没有凭依选择，用户 2026-09-11 定案：按 TH17 的
 *     「主凭依灵 ≠ 本凭依灵」做 ⇒ 任何暴走咆哮都**不吃** +480
 *   · ANIMAL_GOAST_ANY = **恒存在**（彩蛋）—— 持有「弱肉强食之理」(25) 或「鬼杰组长的恐吓」(30, Keiki)
 *     时视为主凭依灵均存在 ⇒ 触发的任何暴走咆哮都算「本凭依灵的暴走咆哮」，吃 +480 */
#define ANIMAL_GOAST_NONE      (-1)
#define ANIMAL_GOAST_ANY       (-2)
#define ANIMAL_RADIUS_MAX      200.0f /* 霊撃半径上限（th17:0x40f61a 的 ×200.0）*/
#define ANIMAL_EXTEND_COUNT    11     /* 咆哮中延长表长度（90/60/42/…）*/

/* 咆哮（就绪态 + 霊撃态）的全部标量状态。 */
typedef struct {
    int32_t slots[ANIMAL_SLOT_MAX]; /* 队列本体：存 token type */
    int32_t fill;                   /* 已收数 0..5（TH17 HYPER_FILL）*/
    int32_t ready;                  /* 1 = 就绪（HYPER_FLAGS bit1）*/
    int32_t hypering;               /* 1 = 霊撃进行中（bit2）*/
    int32_t by_hit;                 /* 1 = 本次霊撃由中弹取消触发（bit3）*/
    int32_t hyper_type;             /* 1..3 = 暴走咆哮（单一兽灵），4 = 混合（TH17 +0xe0）*/
    int32_t duration;               /* 本次就绪窗口总帧数（TH17 hyper_duration +0xdc）*/
    int32_t time_left;              /* 就绪窗口 / 霊撃剩余帧（TH17 HYPER_TIME +0xc8）*/
    int32_t release_level;          /* 队列里最后一个 8..14 的 type−7（本卡不做稀有，恒 0）*/
    int32_t extends_used;           /* 本窗口已用几次延长（animal_next_extend 的游标）*/
    int32_t pending[ANIMAL_PENDING_MAX]; /* 霊撃期间收集的溢出队列 */
    int32_t pending_count;
    int32_t goast;                  /* 主凭依灵：0..2 指定 / ANIMAL_GOAST_NONE 不存在 / ANIMAL_GOAST_ANY 恒存在 */
} animal_core_t;

/* type 分类。 */
static inline int animal_type_is_animal(int32_t t) { return t >= ANIMAL_TYPE_GOAST_MIN && t <= ANIMAL_TYPE_GOAST_MAX; }
static inline int animal_type_is_fish(int32_t t)   { return t >= ANIMAL_TYPE_FISH_MIN && t <= ANIMAL_TYPE_FISH_MAX; }

/* 时长表（TH17 `.rdata` `0x4a13cc`..`0x4a13d8` = 1140/840/1140/1500，读点 `0x4a13c4 + count*4`）：
 * majority_count 3/4/5 → 840/1140/1500；混合（hyper_type 4）恒 1140。
 * **凭依 +480 不在这里**（`animal_ready_compute` 里加，见 `ANIMAL_GOAST_BONUS`）。
 * 越界输入一律夹到合法范围。 */
int32_t animal_hyper_duration(int32_t hyper_type, int32_t majority_count);

/* 就绪判定（TH17 `0x40fb60`）：数队列里 1/2/3 的出现次数，最多的一类 ≥3 → 暴走咆哮（单一类型）；
 * 否则混合。同时算 release_level 并置 ready / time_left=duration / extends_used=0。
 * 返回 1 = 进入就绪（队列满），0 = 队列没满、什么也没做。 */
int animal_ready_compute(animal_core_t *st);

/* 收一颗。返回：
 *   0 = 进队列（可能同时触发就绪）
 *   1 = 队列已满、收的是基本动物灵 → 就绪窗口按 `animal_next_extend` 延长
 *   2 = 霊撃中 → 进 pending（pending 满则丢弃）
 *   3 = 队列已满、收的是鱼灵 → 调用方**立刻**把它换成道具（不进队列、不延长）
 * 非法的 type（不在 1..14）一律丢弃并返回 0。 */
int animal_collect(animal_core_t *st, int32_t type);

/* 下一次延长帧数（用户口径 90 / 60 / 42 / 28 / …；越用越小、用完停在 1）。
 * TH17 代码是 `extend = 180; if (extend > 30) extend = extend*2/3;`（⇒ 120/80/53/…）——
 * 用户 2026-09-10 给的 1.5 s / 1.0 s / 0.7 s 是 wiki 口径，本卡按用户口径实现（🟡）。 */
int32_t animal_next_extend(animal_core_t *st);

/* 就绪窗口每帧 −1（TH17 `0x40ef75` 段；「有存活子弹再 −1」由卡侧决定要不要调两次）。
 * 返回 1 = 本次窗口超时（调用方执行「正常结束」结算）。霊撃中不递减。 */
int animal_ready_tick(animal_core_t *st);

/* 开始霊撃（TH17 `do_hyper_cancel` `0x40f6d0`）：要求 ready 且不在霊撃中。
 * by_hit 0 = 按键发动 / 1 = 中弹取消。返回 1 = 开始，0 = 条件不满足。 */
int animal_hyper_start(animal_core_t *st, int32_t by_hit);

/* 霊撃每帧推进（40 帧）。返回 1 = 本次霊撃结束（调用方清队列并回灌 pending）。 */
int animal_hyper_tick(animal_core_t *st);

/* 霊撃 / 超时结束后：清空队列与状态，再把 pending 逐颗塞回（TH17 §5.4 第 3–4 步）。
 * 返回 1 = 回灌后又满了（调用方应立刻重新就绪），0 = 没满。 */
int animal_clear_and_restore(animal_core_t *st);

/* 霊撃半径：f = time_left / total，r = (1 − f⁴) × 200（TH17 `0x40f61a` 段）。
 * ★ 出参而不是返回值：i386 的 float 返回值走 x87 ST(0)，会破坏 `make dllx87` 的「零 x87」不变式。 */
void animal_hyper_radius(int32_t time_left, int32_t total, float *out);

/* 把队列里的鱼灵 type（4..7）抄进 out（不改变队列）。返回抄到的个数（≤ cap）。 */
int animal_fish_in_queue(const animal_core_t *st, int32_t *out, int32_t cap);

/* 咆哮**正常结束**（超时、没用霊撃）的奖励：2 个「变色动物灵」的 type（1..3）。
 * ★ 2026-09-13 bug-5 用户口径 / 一手对账：
 *   · **一手 TH17** `sub_40e920` `0x40ee03`（release level 0）：`rand()%3 + 15` 连放 2 颗 ——
 *     即 **随机** 15/16/17（= 1..3 的变色版），**不读 `hyper_type`**（`hyper_type` 只在
 *     `Player__do_shooting` 的 set 索引里用；`0x40e920` 从头到尾不碰）。
 *   · **用户口径（本轮）**：暴走咆哮（`hyper_type` 1/2/3）放出的 2 只 = **本次咆哮那一只**的
 *     变色版（狼暴走 → 2 只狼）。⇒ 这是**用户覆盖、非 TH17 一手**。
 *   · 混合（`hyper_type == 4`）TH17 没有「单一类型」，回退到一手行为 = `1 + rand()%3`。
 * rand 为 0 时用确定性轮转（单测）。返回写入 out 的个数（≤ 2）。 */
int animal_end_burst(int32_t *out, int32_t cap, int32_t roar_type, uint32_t (*rand)(void));

/* 寿命归零后的变形：TH17 §4.3 `type % 3 + 1`（1→2→3→1；4..14 不动）。 */
int32_t animal_next_type(int32_t type);
