/* tony_grief_core.h —— 东尼的悲恸（id 86）的纯逻辑：符卡「速收」判定 + 下一个非符段的锁定与写血。
 * 纯 C11、不碰引擎（输入是一个快照结构），主机 `make test-host` 直接测。
 *
 * ## 规则（owner 2026-10-01 逐字）
 * 「若用时 16s 之内成功收取一张符卡，则下一个非符血量降为 1。」
 *
 * ## 两侧判据（全部一手；调研见 `AUDIT.md` §BE 的 86 条目 / 本文档头）
 *
 * **①「成功收取」= `zSpellcard+0x78` 的 bit1（奖励存活）在 bit0（进行中）清掉时仍置位。**
 *   一手：`Spellcard__end` `0x42a780` 里 `0x42a7c2 and [edi+0x78],0xfffffffe`（清 bit0）在前，
 *   `0x42a7ed test byte [edi+0x78],2`（拿 bit1 决定计分 / 失败演出）在后；**全程不写 bit1**。
 *   超时则相反：`0x42efc0 or [edx+0x78],0x80`（bit7 已超时）+ `0x42efd8 and ...,0xffffffdd`（清 bit1）。
 *   ⇒ 观测点（下一帧的 AbilityManager tick 0x16）上：`bit0=0 && bit7=0 && bit1=1` = 收下。
 *   ⚠ 超时但 elapsed < 60 帧时 bit1 **保留**（`0x42efd2 cmp [edx+0x24],0x3c`）⇒ 必须靠 bit7 排除，不能只看 bit1。
 *   ⚠ 玩家死亡 / 灵击卡（110/111 的 `bomb10_void_spell_bonus`）/ 神之宣告都会中途清 bit1
 *     ⇒ 本卡用「这张符卡期间 bit1 亮过」的**锁存**（`bonus_seen`），与 `cards/senro.c` 的写法同款。
 *
 * **②「用时 16s」= `zSpellcard+0x24`（每帧 +1 的经过帧）≤ 960。**
 *   一手：`0x42a356` 宣言清零、`0x429f99` 每帧 +1（只有这两个写点），结尾 `0x42a780` 不清它。
 *   宣言帧贡献 0 ⇒ 「帧 D+k 被收掉时读到 k」⇒ `<= 960` 就是「宣言后 960 帧内」。
 *
 * **③「下一个非符段」= 段边界处符卡不在进行中（bit0 = 0）。**
 *   段边界 = 活动中断槽（第一个 `hp_value > -1 && time > 0`）的签名变化 ——
 *   签名 `(槽号, hp_value, time, sub_life 头 4 字节)` 照抄 `cards/animal.c` 的 `animal_phase_sig`
 *   与 `cards/lolk.c` 的 `phase_read`（两处一手同形）。
 *   TH18 的 boss ECL 是「非符 ↔ 符卡」严格交替（`setInterrupt` 与 `spellEx`/`spellEnd` 同一次 ECL run），
 *   而我们的观测点（0x16）早于 Enemy tick（0x1b）⇒ 段边界那一帧读到的 bit0 正是「新段是不是符卡」。
 *
 * **④ 落笔 = `zEnemyData.life.current`（`zEnemy+0x6220`，`CE_ENEMY_HP_CUR`）写 1。**
 *   非符段的伤害路是 `0x42f314 sub [ebx+0x4ff4],esi`，`current` 就是权威值（只有 `is_spell` 位才走 7 倍缩放路，
 *   而本卡只对非符段落笔）；击破判定 `current <= slot.hp_value` 只在**有伤害的帧**跑
 *   ⇒ 写 1 不会当场结束这一段，要等玩家下一发命中（一枪即死）。
 *   `maximum`(0x6224) **不动**：血条 = current / maximum，写 1 会显示见底（正是玩家该看到的）。
 *   段内**每帧幂等补写**（`if (cur > 1) cur = 1`），压过同段内可能更晚的 `lifeSet`；段签名一变立刻停。 */
#pragma once
#include <stdint.h>

#define TG_MAX_FRAMES    960      /* 16 s @60fps（口径含端点：<= 960）*/
#define TG_WAIT_FRAMES   300      /* 速收之后等下一段边界的窗口（帧）❓ 自定：正常情况下一帧就到（同一次 ECL run）*/
#define TG_SLOT_COUNT    8        /* zEnemyData.interrupts[8]（CE_ED_INTERRUPT_SLOTS）*/

/* 每帧喂进来的快照（C 侧负责读引擎字段）*/
typedef struct {
    int      spell_active;     /* zSpellcard+0x78 bit0 */
    int      spell_bonus;      /* bit1（奖励存活）*/
    int      spell_timed_out;  /* bit7（已超时）*/
    int32_t  spell_elapsed;    /* +0x24（经过帧）*/
    int      have_boss;        /* 找到一个「有活动中断槽」的 boss */
    uint32_t boss_id;          /* 那个 boss 的 enemy_id */
    int32_t  boss_sig;         /* 它当前活动槽的签名（0 = 没有活动槽）*/
    int      boss_final;       /* 该段是死亡脚本（槽名含 "Dead"）⇒ 不当「非符」处理 */
    int      target_present;   /* C 侧按 target_id 又查了一次：目标 boss 还在吗 */
    int32_t  target_sig_now;   /* 它的活动槽签名 */
} tg_input_t;

typedef struct {
    int      want_write;       /* 本帧给锁定的那段写 life.current = 1 */
    int      captured;         /* 本帧判到「速收」（日志用）*/
    int      armed;            /* 本帧锁定了一段（日志用）*/
    int      released;         /* 本帧目标段结束 / boss 消失（日志用）*/
    int      expired;          /* 本帧等待窗口到点、放弃（日志用）*/
    int32_t  captured_ms;      /* captured 时的 elapsed（日志用）*/
} tg_out_t;

typedef struct {
    uint32_t frames;
    /* 符卡窗口的锁存 */
    int      spell_was;
    int      bonus_seen;       /* 这张符卡期间 bit1 亮过 */
    int32_t  elapsed_last;     /* 最后一个 active 帧的 +0x24 */
    /* 段边界 */
    int      sig_init;
    int32_t  sig;
    /* 速收 → 等下一段 */
    int      pending;
    uint32_t pending_left;
    /* 锁定的目标段（锁住的是**签名**，不是敌人指针）*/
    uint32_t target_id;
    int32_t  target_sig;
    int      target_live;
    /* 统计 */
    uint32_t quick;            /* 判到的速收次数 */
    uint32_t armed_n;          /* 实际改写过的段数 */
} tg_state_t;

void tg_reset(tg_state_t *s);

/* 活动槽签名（0 = 当前没有活动槽）。`interrupts` = `zEnemyData.interrupts`（stride 0x88）。
 * 算式与 `animal_phase_sig`（`cards/animal.c`）/ `phase_read`（`cards/lolk.c`）同族：
 * `(槽号+1)*1000003 ^ hp*31 ^ time*131 ^ 名字`；**名字取前 16 字节**（那两处只取 4 字节 ——
 * TH18 的槽名全以 "Boss" 开头，4 字节分不开 `Boss2` 与 `BossCard3`，会漏判段边界）。
 * `out_final` 可空；非空时写「槽名含 "Dead"」（复用古锭刀 76 的 `gd_slot_is_final`）。*/
int32_t tg_active_slot(const uint8_t *interrupts, unsigned n, int *out_final);

/* 每帧一次（纯状态机）。 */
tg_out_t tg_frame(tg_state_t *s, const tg_input_t *in);
