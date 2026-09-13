/* lolk_core.h —— 绀珠传（card-expand id 115）纯逻辑：波状态机 + 章节奖公式 + 擦弹减速曲线。
 * 纯 C11、不含任何引擎地址；主机单测 tests/test_lolk_core.c（make test-host）。
 *
 * 语义来源（一手，TH15 v1.00b / TH18 v1.00a）：
 *   engine/chapter/th15/01-chapter-and-bonus.md      —— 公式 / 阈值 / 碎片 / 面板 / 524 分布
 *   engine/chapter/th18/02-wave-signals.md           —— boss 相位结果信号（TH18）
 *   engine/chapter/th18/03-midstage-waves.md         —— 道中波边界（TH15 定义 + TH18 等价信号）
 *   engine/player/th15/01-graze-and-item-slowdown.md —— 减速因子 0.3、每帧 +0.1 回 1.0
 *
 * ## 波次定义（用户 2026-09-09 定案）
 *
 * **一段攻击 = 一波**：一个非符是一波、一张符卡也是一波；进入关底 boss / 道中 boss
 * 也算一波（boss 出现即开新波）。
 *
 * ## 道中波边界：TH15 的「一章」= TH18 的「一个 MainSub 敌人组」
 *
 * TH15 的关卡脚本（`st0X.ecl` 的 `main`/`MainFront`/`MainLatter`）在**每段敌人组开头**
 * 调 ECL 524 `setChapter(n)`：`MainFront` 里 `setChapter(1) → MainSub01 … setChapter(2)
 * → MainSub02 …`（st03/st05 最清楚，见 th15/01 §1b）。所以 TH15 的道中「一章」
 * = 一个 `MainSubNN` 敌人组，**不是**引擎按时间/清场猜的。
 *
 * TH18 的道中**不切章**（全 8 关 `st*.ecl` 只有开场 0、进 boss 前 41 与 boss 段 43/81/2，
 * 道中一个 524 都没有），但**脚本结构一模一样**：`main → MainFront/MainLatter → MainSubNN`。
 *
 * **2026-09-13 用户洞察（本卡现行口径）**：自然键就是 **`MainSub` 编号本身** ——
 * `MainSub00 / MainSub01 / MainSub01b / MainSub02 …` 里，**去掉尾部字母的数字**是一章。
 * 字母后缀是基号的**异步子程序**（`@MainSub01b() async` 等），与基号同章：
 * `01` + `01b`（+ `01c`）= 一章。接线层每帧从关卡脚本的**主上下文子程序名**解析出
 * `stage_chapter`（= MainSub 数字，见 lolk.c 的 `stage_ecl_read()`），纯逻辑侧只用它切章：
 *
 * - **主信号**：`stage_chapter` 变了（或当前没开波）+ 本帧有刷出 = 新章；
 *   同一个编号里的多段 burst / 异步子程序**不切**（旧实现靠 `LOLK_MID_BURST_GAP`
 *   静默窗口猜「一个 MainSub 里排了几波」，已被本条取代并删除）。
 * - `main` / `MainFront` / `MainLatter` 段（`stage_chapter < 0` 但 `stage_sub >= 0`）不是道中章：
 *   这些段里的刷出（例如开场 `MapleEnemy`）不开波，避免多出一个空章。
 * - 认不出关卡脚本（`stage_sub < 0`）才退到「静默 ≥ `LOLK_MID_QUIET_GAP` 帧后再刷出」。
 *
 * 一手静态复核（`thecl -d 18`，8 关全量，2026-09-13）：所有字母后缀 `MainSubNNx` 都是
 * 基号的 `@…() async` 子程序（st01 `MainSub01b/01c`、`MainSub02b`、`MainSub06b`；
 * st02 `MainSub00b/06b`；st03 `MainSub02b/06b`；st05 `MainSub02b`），
 * 所以关卡脚本**主上下文**的子程序名天然就是基号。
 *
 * ## 击破率（用户 2026-09-09 定案，三种来源）
 *
 * | 波种 | 击破率 |
 * | --- | --- |
 * | 道中章 | 击破数 / 本章刷出总数（分母 0 → 100％）|
 * | boss 非符 | 击破 → 100％，超时 → 0％ |
 * | boss 符卡 | 击破（HP 打空）→ 100％，超时 → 0％ |
 * | **时符**（耐久符卡） | **恒 100％**（引擎自己也把它的超时当收卡，见 §信号）|
 *
 * 「击破 / 超时」不用本卡猜：TH18 引擎自己就在判——中断槽 {hp_value, time} 里
 * `life.current <= hp_value` 是击破、`time <= time_in_ecl` 是超时，
 * 超时还会把 `zEnemyData.flags_lo` bit24 置 1（击破时清 0），符卡另把
 * `SPELLCARD+0x78` bit7 置 1。详见 engine/chapter/th18/02-wave-signals.md。
 *
 * ## 分母的近似
 *
 * TH18 没有 TH15 的 ENEMIES_SPAWNED_IN_CHAPTER（死槽）。接线层每帧读
 * `zEnemyManager.enemy_count_real`，正增量记「刷出」、负增量记「消失」；
 * TH15 的 DESTROYED 也是在「敌人被移除」时加的（`th15:0x428b70`），
 * 所以「消失」当分子与 TH15 同义（离场也算，TH15 也这么算）。 */
#pragma once
#include <stdint.h>

/* —— 常量（主机测试可用 -D 覆写）—— */
#define LOLK_BONUS_COEF      50       /* 章节奖系数（TH15 `0x4cffc0` = 50.0）*/
#define LOLK_FRAG_THRESHOLD  1000000  /* bonus ≥ 此值才发碎片（TH15 `0x43ae42`）*/
#define LOLK_SCORE_DIV       10       /* 分数增量 = bonus / 10（TH15 `0x43ad19`）*/
#define LOLK_FRAG_CYCLE      5        /* 每 5 波第 5 波给命碎，其余给符卡碎（本卡新定）*/
#define LOLK_FRAG_BOMB       0x11     /* BOMB2 卡道具 → CardBombFragment（符卡碎）*/
#define LOLK_FRAG_LIFE       0x10     /* EXTEND2 卡道具 → CardLifeFragment（命碎）*/

#define LOLK_SLOW_START      30       /* 擦弹当帧的减速因子 ×100（TH15 = 0.3）*/
#define LOLK_SLOW_STEP       10       /* 每帧恢复 +0.10（TH15 `0x4cfdf0` = 0.1）*/
#define LOLK_SLOW_FULL       100      /* 满速 ×100 */

#define LOLK_WAVE_MAX        999      /* 波号上限（防溢出）*/
#define LOLK_DEBOUNCE        5        /* 波边沿去抖帧数（相位切换时槽清空 + 符卡位翻转会连报）*/

/* 道中波边界（TH18 等价信号，见头注与 engine/chapter/th18/03）*/
#define LOLK_MID_QUIET_GAP   240      /* 退路（stage_sub < 0）：静默 ≥ 此帧数后再刷出算「新章」*/

/* 开道中/ boss 波时用的规则（日志里说明用的是哪条；见 lolk_tick 的 mid_rule）*/
#define LOLK_MID_RULE_NONE     (-1)   /* 还没开过波 */
#define LOLK_MID_RULE_MAINSUB  0      /* 主信号：章 = MainSub 编号（去掉尾部字母）*/
#define LOLK_MID_RULE_QUIET    1      /* 回退：认不出关卡脚本 → 静默间隔 */
#define LOLK_MID_RULE_BOSS     2      /* boss 相位（非道中）*/

/* 事件返回值（位掩码：一次 tick 可以同时「结一波 + 开一波」）*/
#define LOLK_EV_NONE         0
#define LOLK_EV_WAVE_START   1
#define LOLK_EV_WAVE_END     2

/* 波种 */
#define LOLK_KIND_MID        0        /* 道中章：rate = 击破数 / 刷出数 */
#define LOLK_KIND_BOSS       1        /* boss 相位：rate = 100 / 0 */

/* 相位结果 */
#define LOLK_OUT_NONE        0
#define LOLK_OUT_BROKEN      1        /* 击破 */
#define LOLK_OUT_TIMEOUT     2        /* 超时 */

/* 接线层每帧填的相位信号（全部只读引擎现成字段；见 engine/chapter/th18/02）*/
typedef struct {
    int32_t boss_present;    /* 场上有 boss（boss_ids 命中）*/
    int32_t spell_active;    /* SPELLCARD+0x78 bit0：符卡进行中 */
    int32_t survival;        /* SPELLCARD+0x78 bit3：耐久符卡（时符）*/
    int32_t spell_timeout;   /* SPELLCARD+0x78 bit7：符卡已超时 */
    int32_t phase_timeout;   /* 本帧 `中断槽.time <= time_in_ecl`（超时条件成立）*/
    int32_t phase_broken;    /* 本帧 `life.current <= 中断槽.hp_value`（击破条件成立）*/
    int32_t phase_timeout_flag; /* 引擎持续位：zEnemy flags_lo bit24（超时置 1 / 击破清 0）*/
    int32_t phase_sig;       /* 当前相位指纹（0 = 没有活动中断槽）*/
    int32_t chapter;         /* GLOBALS.inner+0x24：ECL 524 写下的章号（TH18 道中不切，只当 boss 兜底）*/
    int32_t stage_sub;       /* 关卡脚本 ECL 主上下文当前子程序号；< 0 = 没认出来（退路判据）*/
    int32_t stage_chapter;   /* ★ 关卡脚本当前 MainSub 编号（去掉尾部字母；01 与 01b 都 = 1）。
                              *   < 0 = 当前不在 MainSub（main/MainFront/MainLatter，或没认出来）*/
    int32_t spawn_delta;     /* 本帧新建敌人数（zEnemyManager.next_enemy_id 差分，< 0 当 0）*/
} lolk_in_t;

typedef struct {
    int32_t boss_present;          /* 上一帧的 boss 在场 */
    int32_t spell_active;          /* 上一帧的符卡位 */
    int32_t wave_open;             /* 本波开着 */
    int32_t wave_kind;             /* LOLK_KIND_* */
    int32_t wave_is_spell;         /* 本波是符卡段（saw_spell 的别名，输出用）*/
    int32_t saw_spell;             /* 本波期间符卡位亮过 */
    int32_t saw_survival;          /* 本波期间耐久符卡位亮过（时符）*/
    int32_t saw_broken;            /* 本波期间见过击破条件成立 */
    int32_t saw_timeout;           /* 本波期间见过超时（直接条件 / 符卡 bit7 / 引擎位上升沿）*/
    int32_t flag_base;             /* 本波开始时的 phase_timeout_flag（判断上升沿用）*/
    int32_t st_base;               /* 本波开始时的 spell_timeout（同上，防上一张符卡的残留）*/
    int32_t graze;                 /* 本波擦弹数 */
    int32_t spawned;               /* 本波刷出数（enemy_count_real 正增量累计）*/
    int32_t killed;                /* 本波消失数（负增量累计；与 TH15 的 DESTROYED 同义）*/
    int32_t waves_done;            /* 已结算波数（碎片循环游标）*/
    int32_t debounce;              /* 边沿去抖倒计时 */
    int32_t last_phase_sig;        /* 上一帧相位指纹 */
    int32_t last_chapter;          /* 上一帧 ECL 524 章号（boss 兜底用）*/
    int32_t last_stage_sub;        /* 上一帧关卡脚本子程序号（诊断/日志用）*/
    int32_t wave_chapter;          /* ★ 当前开着的道中波的 MainSub 编号；-1 = 无 / 未知 / boss */
    int32_t mid_rule;              /* 最近一次开波用的规则 LOLK_MID_RULE_*（日志用）*/
    int32_t quiet;                 /* 连续无刷出帧数（stage_sub < 0 时的退路用）*/
    int32_t slow;                  /* 减速因子 ×100，恒 ∈ [LOLK_SLOW_START, LOLK_SLOW_FULL] */
    int32_t grazed_this_frame;     /* 本帧是否擦到（接线层每帧置，lolk_slowdown_step 消费）*/
} lolk_state_t;

/* 结算输出（仅 LOLK_EV_WAVE_END 时有效）*/
typedef struct {
    int32_t wave_index;   /* 本波序号（从 1 起）*/
    int32_t kind;         /* LOLK_KIND_* */
    int32_t is_spell;     /* 刚结算的这一波是符卡段 */
    int32_t graze;
    int32_t killed;
    int32_t spawned;      /* 分母（近似）*/
    int32_t rate;         /* 击破率 0..100 */
    int32_t bonus;        /* 章节奖 */
    int32_t score;        /* 应加分数 = bonus / LOLK_SCORE_DIV */
    int32_t frag_type;    /* 0 = 不发；否则 LOLK_FRAG_BOMB / LOLK_FRAG_LIFE */
} lolk_settle_t;

/* 每帧调一次，`in` 是本帧采样到的相位信号 + 道中信号
 * （`stage_chapter` = MainSub 编号 / `stage_sub` / `spawn_delta`）。
 * 返回 LOLK_EV_* 位掩码；带 LOLK_EV_WAVE_END 位时填 out（空波不产事件）。*/
int lolk_tick(lolk_state_t *s, const lolk_in_t *in, lolk_settle_t *out);

/* 擦弹 +n（n ≥ 1）。只在本波开着时累计。*/
void lolk_add_graze(lolk_state_t *s, int32_t n);

/* 本波刷出 +n（enemy_count_real 的正增量）。*/
void lolk_add_spawned(lolk_state_t *s, int32_t n);

/* 本波消失 +n（enemy_count_real 的负增量绝对值；与 TH15 的 DESTROYED 同义）。*/
void lolk_add_killed(lolk_state_t *s, int32_t n);

/* 减速曲线：本帧擦弹过 → 因子回落到 LOLK_SLOW_START；否则 +LOLK_SLOW_STEP 到满速。
 * 返回新因子（×100）。每帧调一次（与 lolk_tick 同频）。*/
int32_t lolk_slowdown_step(lolk_state_t *s, int32_t grazed_this_frame);

/* 纯函数：道中击破率（0..100）。spawned ≤ 0 → 100（用户定的边缘情况）。*/
int32_t lolk_rate(int32_t killed, int32_t spawned);

/* 纯函数：boss 相位击破率。时符 → 100；击破 → 100；超时 → 0。*/
int32_t lolk_rate_phase(int32_t is_survival, int32_t outcome);

/* 纯函数：章节奖（TH15 原式，向下取整到 10）。*/
int32_t lolk_bonus(int32_t graze, int32_t rate);

/* 规则名（日志用）：LOLK_MID_RULE_* → "mainsub" / "quiet-fallback" / "boss" / "none"。*/
const char *lolk_mid_rule_name(int32_t rule);

/* 局末 / 开局重置。*/
void lolk_reset(lolk_state_t *s);
