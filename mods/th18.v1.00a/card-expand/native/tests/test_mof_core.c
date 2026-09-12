/* test_mof_core.c —— `cards/mof_core.c` 的主机单测（`make test-host` 的口径：
 *   cc -std=c11 -Wall -Wextra -Werror -o /tmp/tm tests/test_mof_core.c cards/mof_core.c && /tmp/tm）。
 *
 * 覆盖（用户点名的部分加粗）：
 *   · **PV 公式三态**（只 110 / 只 111 / 两个都装）+ 落笔者只读聚合值 ⇒ 与声明顺序无关；
 *   · **扣火力的 1.05 下限**（含门槛 ≥ 2.00 的拒绝、原生钳不住的那条）；
 *   · 大信仰点判据（`drop_main == 2` / 同时掉钱与 P / **boss 不掉**）+ 难度表；
 *   · 满 P 时 P 点 → 小信仰点；
 *   · 连击槽的按帧推进（boss 战不衰减）+ ★ **槽空不扣信仰**（一手订正：TH10 无逐帧衰减）；
 *   · ★ 收集事件档 / 连击帧数（大信仰点 +120、小信仰点 +60 —— 一手订正）；
 *   · 蓝点高度系数与分值，含 ★ **生产路径的线性插值**（顶端 / 中间 / 底端 / 线下 1px）；
 *   · 卡内奖残越阈值（20m/40m/80m/150m）。
 *
 * 依据：spec §2.1 / §2.6 / §3 / §8.2 + review §3.2 / §3.2.1 / §5.5.1 / §7.1 / §7.2 / §7.4 / §7.5 /
 *       §7.6 / §7.8 / §7.9 / §11 #47a #48；
 *       ★ 一手（覆盖 spec 被证伪的三处）：`local/_scratch/research-th10-faith-and-graze.md`
 *       §3.1（连击表：大信仰点 +120f）/ §3.2 §3.4（无逐帧信仰衰减）/ §1.1（TH10 无擦弹倍率）。
 */
#include <stdio.h>
#include <string.h>

/* `CE_EDEATH_*`（死亡事件的 kind / flags）来自 SDK；显式包含，**不靠** mof_core.h 的传递包含。*/
#include "../sdk/public.h"
#include "../cards/mof_core.h"

static int s_fail, s_pass;
#define CHECK(cond) do { if (cond) ++s_pass; else { ++s_fail; printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } } while (0)

int main(void)
{
    faith_slot_t slot;
    mof_bonus_t  bonus;
    mof_power_t  pw;
    faith_ev_t   ev;
    int32_t      v;

    /* ============ 大信仰点难度表（th10 一手 0x41b57f）============ */
    CHECK(mof_core_big_value(FAITH_DIFF_EASY) == 5000);
    CHECK(mof_core_big_value(FAITH_DIFF_NORMAL) == 5000);
    CHECK(mof_core_big_value(FAITH_DIFF_HARD) == 8000);
    CHECK(mof_core_big_value(FAITH_DIFF_LUNATIC) == 10000);
    CHECK(mof_core_big_value(FAITH_DIFF_EXTRA) == 10000);

    /* ============ 大信仰点判据（§7.9）============ */
    CHECK(mof_core_wants_big_main(MOF_DROP_TYPE_MONEY, 0) == 1);       /* drop_main == 2 */
    CHECK(mof_core_wants_big_main(1, 0) == 0);                         /* P 点不是 */
    CHECK(mof_core_wants_big_main(0, 0) == 0);                         /* 不掉 */
    CHECK(mof_core_wants_big_main(MOF_DROP_TYPE_MONEY, 1) == 0);       /* ★ boss 不掉 */
    CHECK(mof_core_wants_big_dual(0, 3, 2, 0) == 1);                   /* 同时掉钱与 P */
    CHECK(mof_core_wants_big_dual(0, 3, 0, 0) == 0);                   /* 只有钱 */
    CHECK(mof_core_wants_big_dual(0, 0, 2, 0) == 0);                   /* 只有 P */
    CHECK(mof_core_wants_big_dual(0, 3, 2, 1) == 0);                   /* ★ boss 不掉 */
    CHECK(mof_core_wants_big_dual(MOF_DROP_TYPE_MONEY, 3, 2, 0) == 0); /* 走判据①，不重复计 */

    v = -1;
    CHECK(mof_core_wants_big(MOF_DROP_TYPE_MONEY, 0, 0, 0, FAITH_DIFF_HARD, &v) == 1);
    CHECK(v == 8000);
    CHECK(mof_core_wants_big(MOF_DROP_TYPE_MONEY, 0, 0, 1, FAITH_DIFF_HARD, &v) == 0);
    CHECK(v == 0);
    CHECK(mof_core_wants_big(0, 1, 1, 0, FAITH_DIFF_EASY, &v) == 1);
    CHECK(v == 5000);
    CHECK(mof_core_wants_big(MOF_DROP_TYPE_MONEY, 0, 0, 0, FAITH_DIFF_LUNATIC, 0) == 1); /* NULL 安全 */

    /* ============ 死亡事件的门（KILL_ALL / 真敌人 / 有掉落表）============
     * 断点 0x430240 是**广播**，四类死因都会来 ⇒ 门必须自己过。
     * 依据：shared-enemy-death-event-design §2.4 / review §4.2 的 ⚠ killAll 坑 / §11 #42（极性）。*/
    CHECK(mof_core_death_qualifies(CE_EDEATH_KILLED,    CE_EDEATH_F_REAL | CE_EDEATH_F_HAS_DROP) == 1);
    CHECK(mof_core_death_qualifies(CE_EDEATH_BOMB_KILL, CE_EDEATH_F_REAL | CE_EDEATH_F_HAS_DROP) == 1);
    CHECK(mof_core_death_qualifies(CE_EDEATH_ECL_DIE,   CE_EDEATH_F_REAL | CE_EDEATH_F_HAS_DROP) == 1);
    /* ★ 排除 killAll 一族（0x4307f1 / 0x430a00 / 0x430c08）—— 过场全清会把计数刷满 */
    CHECK(mof_core_death_qualifies(CE_EDEATH_KILL_ALL,  CE_EDEATH_F_REAL | CE_EDEATH_F_HAS_DROP) == 0);
    /* 只有真敌人才算（置位 = 被排除在真敌人之外；极性别弄反）*/
    CHECK(mof_core_death_qualifies(CE_EDEATH_KILLED,    CE_EDEATH_F_HAS_DROP) == 0);
    /* 没有掉落表 ⇒ 谈不上「掉钱」*/
    CHECK(mof_core_death_qualifies(CE_EDEATH_KILLED,    CE_EDEATH_F_REAL) == 0);
    CHECK(mof_core_death_qualifies(CE_EDEATH_KILLED,    0) == 0);

    /* `on_enemy_death` 的推荐入口：门 + 判据① 一起做（只吃事件自带的字段）*/
    v = -1;
    CHECK(mof_core_wants_big_death(CE_EDEATH_KILLED, CE_EDEATH_F_REAL | CE_EDEATH_F_HAS_DROP,
                                   MOF_DROP_TYPE_MONEY, 0, FAITH_DIFF_HARD, &v) == 1);
    CHECK(v == 8000);
    CHECK(mof_core_wants_big_death(CE_EDEATH_KILLED, CE_EDEATH_F_REAL | CE_EDEATH_F_HAS_DROP,
                                   MOF_DROP_TYPE_P, 0, FAITH_DIFF_HARD, &v) == 0);
    CHECK(v == 0);                                                     /* 拒绝时清零 */
    CHECK(mof_core_wants_big_death(CE_EDEATH_KILLED, CE_EDEATH_F_REAL | CE_EDEATH_F_HAS_DROP,
                                   MOF_DROP_TYPE_MONEY, 1, FAITH_DIFF_HARD, &v) == 0);  /* boss */
    CHECK(mof_core_wants_big_death(CE_EDEATH_KILL_ALL, CE_EDEATH_F_REAL | CE_EDEATH_F_HAS_DROP,
                                   MOF_DROP_TYPE_MONEY, 0, FAITH_DIFF_HARD, &v) == 0);
    CHECK(mof_core_wants_big_death(CE_EDEATH_KILLED, CE_EDEATH_F_HAS_DROP,
                                   MOF_DROP_TYPE_MONEY, 0, FAITH_DIFF_HARD, &v) == 0);  /* 非真敌人 */
    CHECK(v == 0);
    CHECK(mof_core_wants_big_death(CE_EDEATH_KILLED, 0,
                                   MOF_DROP_TYPE_MONEY, 0, FAITH_DIFF_HARD, &v) == 0);  /* 无掉落表 */

    /* ============ 满 P 时 P 点 → 小信仰点 ============ */
    CHECK(mof_core_pick_p_item(MOF_DROP_TYPE_P, 0, 400, 400, 0) == MOF_PICK_ADD_SMALL);
    CHECK(mof_core_pick_p_item(0, 1, 400, 400, 0) == MOF_PICK_ADD_SMALL);   /* ex_drop 里的 P */
    CHECK(mof_core_pick_p_item(MOF_DROP_TYPE_P, 0, 399, 400, 0) == MOF_PICK_DROP);   /* 不满 P */
    CHECK(mof_core_pick_p_item(MOF_DROP_TYPE_MONEY, 0, 400, 400, 0) == MOF_PICK_DROP); /* 不是 P 点 */
    CHECK(mof_core_pick_p_item(MOF_DROP_TYPE_P, 0, 400, 400, 1) == MOF_PICK_DROP);   /* boss 不掉 */

    /* ============ 扣火力（§5.5.1 + §3.2.1 的 1.05 下限）============ */
    /* 门槛 = TH18 ≥ 2.00 档（定点 200）：1.99 放不出 */
    CHECK(mof_core_power_can(199) == 0);
    CHECK(mof_core_power_can(200) == 1);
    pw = mof_core_power_spend(199);
    CHECK(pw.ok == 0);
    CHECK(pw.new_power == 199);                       /* 拒绝时不动火力 */
    /* ★ 2.00 → 1.05（不是 1.00！用户「加一个サービス」）*/
    pw = mof_core_power_spend(200);
    CHECK(pw.ok == 1);
    CHECK(pw.new_power == 105);
    /* 2.10 → 1.10（扣 1 档 = 100，还离下限远 —— 这是**整档扣**的正常路径）*/
    pw = mof_core_power_spend(210);
    CHECK(pw.new_power == 110);
    /* 2.04 → 1.05：扣 1 档会到 1.04 < 1.05 ⇒ **下限把最后一次抬回 1.05**
     * （用户原话「最后一个灵击只会扣到 1.05」）*/
    pw = mof_core_power_spend(204);
    CHECK(pw.new_power == 105);
    /* 3.00 → 2.00（正常扣一整档，离下限还远）*/
    pw = mof_core_power_spend(300);
    CHECK(pw.new_power == 200);
    /* 2.05 → 1.05 恰好贴下限 */
    pw = mof_core_power_spend(205);
    CHECK(pw.new_power == 105);
    /* 2.04 → 也是 1.05（扣完 104 < 105 ⇒ 抬回 1.05）—— 「最后一个灵击只会扣到 1.05」*/
    pw = mof_core_power_spend(204);
    CHECK(pw.new_power == 105);
    /* 永远掉不到 1.00 */
    for (int p = 200; p < 400; ++p) {
        mof_power_t q = mof_core_power_spend(p);
        if (q.ok && q.new_power < 105) { CHECK(0); break; }
    }
    CHECK(1);

    /* ============ PV 公式三态（§7.4 / §7.8）============ */
    /* 只装 110：`PV = 信仰_110`（无乘数）。aggregate = 110 声明的信仰。*/
    CHECK(mof_core_pv(1, 0, 50000, 0, 0) == 50000);              /* 基础 */
    CHECK(mof_core_pv(1, 0, 55000, 0, 0) == 55000);              /* +5000 大信仰点 */
    CHECK(mof_core_pv(1, 0, 55000, 8990, 130000) == 55000);      /* ★ 无乘数：擦弹/信号都不乘 */

    /* 擦弹倍率：`min(graze × 0.0001, 8.99)` ⇒ 千分点 = min(graze, 8990) */
    CHECK(mof_core_graze_mult(0) == 0);
    CHECK(mof_core_graze_mult(1) == 1);
    CHECK(mof_core_graze_mult(1000) == 1000);                    /* 1000 graze = ×0.1 */
    CHECK(mof_core_graze_mult(8990) == 8990);                    /* 上限 8.99 */
    CHECK(mof_core_graze_mult(999999) == 8990);
    CHECK(mof_core_graze_mult(-3) == 0);

    /* 只装 111：`PV = 信仰_111 × (擦弹倍率 + 信号强度)`。
     * aggregate = 111 声明的信仰（基础按难度 + Δ_111）。*/
    CHECK(mof_core_pv(0, 1, 50000, 0, 0) == 0);                  /* 乘数为 0 ⇒ 0 */
    CHECK(mof_core_pv(0, 1, 50000, 1000, 0) == 50000);           /* ×1.0 */
    CHECK(mof_core_pv(0, 1, 50000, 1000, 50000) == 2550000);     /* ×(1.0 + 50.0) */

    /* 两个都装：`PV = (基础均值 + Δ_110 + Δ_111) × (擦弹倍率 + 信号强度)`
     * 基础均值 = (50000 + 50000) / 2 = 50000（NORMAL 撞巧同基础，§7.2）。
     * aggregate = 两份声明之和 = 信仰_110 + 信仰_111。*/
    /* ★ 2026-09-12 修：`aggregate` 里**两份基础都要减掉**（原实现把基础记了三遍）：
     *   NORMAL 下 aggregate = 50000 + Δ110 + 50000 + Δ111 ⇒ Δ 和 = aggregate − 100000。*/
    CHECK(mof_core_pv(1, 1, 100000, 1000, 0) == 50000);          /* Δ 都是 0 ⇒ 只剩基础均值 50000 */
    CHECK(mof_core_pv(1, 1, 101000, 1000, 0) == 51000);          /* 多 1000（= 大绿点）也进 */
    CHECK(mof_core_pv(1, 1, 100000, 1000, 50000) == 2550000);    /* 50000 × (1.0 + 50.0) */
    CHECK(mof_core_pv(1, 1, 50000, 1000, 0) == 50000);          /* 只声明了一份基础 ⇒ Δ 按 0 夹住 */

    /* ★ 落笔者只读聚合值 ⇒ 与「谁先声明」无关：交换两卡贡献的和，结果一样 */
    CHECK(mof_core_pv(1, 1, 60000 + 40000, 1000, 0) == mof_core_pv(1, 1, 40000 + 60000, 1000, 0));
    /* 负聚合值按 0 处理（防御；声明层本身不会出负数）*/
    CHECK(mof_core_pv(1, 0, -5, 0, 0) == 0);
    /* 两张都不在 ⇒ 不该有人落笔 */
    CHECK(mof_core_pv(0, 0, 123456, 1000, 0) == 0);

    /* ============ 连击槽的按帧推进（§2.3 boss 战不衰减 + ★ 一手：无逐帧信仰衰减）============
     * ★ 2026-09-12 一手订正（任务 ①）：旧实现在「连击槽空」时按 `faith_slot_drain_empty()`
     *   扣信仰（无敌人 −10800/s、有敌人 −600/s）。一手（`research-th10-faith-and-graze.md`
     *   §3.2 / §3.4 / §1.1-C）证明 **TH10 没有这个机制**：对信仰 `0x474c4c` 的写入点已穷举
     *   （增只有 3 类信仰点道具、减只有 miss / 非决死灵击 / 阶段超时 3 条，**无逐帧衰减**），
     *   连击计时器 `0x474c54` 只被 `0x412ff0` 充能、只被 HUD / 回放读。
     *   ⇒ 下面的断言从「槽空 ⇒ 扣多少」翻转成「槽空 ⇒ **一点都不扣**」。*/
    faith_slot_init_110(&slot);
    (void)faith_slot_add(&slot, FAITH_EV_BIG, 10000);            /* Δ = 10000（原始单位）*/
    (void)faith_combo_set(&slot, 2);
    CHECK(mof_core_combo_tick(&slot, 0) == 0);                   /* 有连击 ⇒ 不扣信仰（恒 0）*/
    CHECK(slot.combo == 1);
    CHECK(mof_core_combo_tick(&slot, 0) == 0);
    CHECK(slot.combo == 0);
    CHECK(slot.delta == 10000 * FAITH_FIXED);                    /* ★ 信仰一点没动 */
    /* 空了：**什么都不发生**（旧实现这里会扣 10 = −600/s 的有敌人档）*/
    CHECK(mof_core_combo_tick(&slot, 0) == 0);
    CHECK(slot.delta == 10000 * FAITH_FIXED);
    CHECK(faith_slot_total(&slot) == 60000);
    /* 连跑 600 帧（10 秒）：旧实现在无敌人档会扣掉 108000 ≫ Δ ⇒ 现在应当**分毫不动** */
    for (int k = 0; k < 600; ++k) CHECK(mof_core_combo_tick(&slot, 0) == 0);
    CHECK(slot.delta == 10000 * FAITH_FIXED);
    CHECK(faith_slot_total(&slot) == 60000);                     /* ★ 不衰减（本轮的证据方向）*/
    /* boss 战：连击槽不衰减（§2.3）、信仰同样不动 */
    (void)faith_combo_set(&slot, 5);
    CHECK(mof_core_combo_tick(&slot, 1) == 0);
    CHECK(slot.combo == 5);
    (void)faith_combo_set(&slot, 0);
    CHECK(mof_core_combo_tick(&slot, 1) == 0);
    CHECK(slot.combo == 0 && slot.delta == 10000 * FAITH_FIXED);
    CHECK(mof_core_combo_tick(0, 0) == 0);                       /* NULL 安全 */

    /* ============ 收集事件档 / 连击帧数（§7.5 表 + 一手 th10 充能立即数）============
     * 一手出处（`research-th10-faith-and-graze.md` §3.1 的 push 立即数）：
     *   大信仰点 +120（`0x41b5c5 push 0x78`）· 蓝点 +100（`0x41b56e push 0x64`）
     *   小信仰点 +60（`0x41b61d push 0x3c`）· 小 P +60（`0x41b46c push 0x3c`）
     *   大 P +20（`0x41b6bd`）· 微 +3（`0x41b5e2 push 3`）· 1UP +0 · 上限 130（`0x413035`）
     * ★ 任务 2：小信仰点原来走 `FAITH_EV_GREEN`（+100f）⇒ 现在走 `FAITH_EV_SMALL_FAITH`（+60f）。*/
    CHECK(MOF_COMBO_BIG == 120);                                 /* ★ 一手：大信仰点 +120（不是 spec 的 130）*/
    CHECK(mof_core_pickup_combo(0 /* FAITH_TIER_BIG */) == 120);
    CHECK(mof_core_pickup_combo(1 /* FAITH_TIER_SMALL */) == 60);
    CHECK(mof_core_pickup_ev(0 /* FAITH_TIER_BIG */) == FAITH_EV_BIG);
    CHECK(mof_core_pickup_ev(1 /* FAITH_TIER_SMALL */) == FAITH_EV_SMALL_FAITH);   /* ★ 不是 GREEN */
    /* 共用件那张表：小信仰点档也确实是 60f（`faith_combo_value(FAITH_EV_SMALL_FAITH)`）*/
    CHECK(faith_combo_value(FAITH_EV_SMALL_FAITH) == 60);
    CHECK(faith_combo_value(FAITH_EV_GREEN) == 100);             /* 蓝点通道仍是 +100f（不能混用）*/
    {
        /* 卡侧 `mof_on_pickup()` 的等价动作：信仰加值走事件档、连击走 110 自己的表 */
        faith_slot_init_110(&slot);
        ev = mof_core_pickup_ev(1 /* FAITH_TIER_SMALL */);
        (void)faith_slot_add(&slot, ev, FAITH_MOF_SMALL);        /* 小信仰点 +100 */
        (void)faith_combo_set(&slot, slot.combo + mof_core_pickup_combo(1 /* FAITH_TIER_SMALL */));
        CHECK(slot.delta == 100 * FAITH_FIXED);                  /* Δ_110 += 100 */
        CHECK(faith_slot_total(&slot) == 50100);
        CHECK(slot.combo == 60);                                 /* ★ 连击 +60f（旧：+100f）*/
        /* 大信仰点：+10000（L）与 +120f */
        faith_slot_init_110(&slot);
        ev = mof_core_pickup_ev(0 /* FAITH_TIER_BIG */);
        (void)faith_slot_add(&slot, ev, mof_core_big_value(FAITH_DIFF_LUNATIC));
        (void)faith_combo_set(&slot, slot.combo + mof_core_pickup_combo(0 /* FAITH_TIER_BIG */));
        CHECK(faith_slot_total(&slot) == 60000);
        CHECK(slot.combo == 120);                                /* ★ 一手 +120f（旧：+130f）*/
        /* 连击槽夹到 130（该槽的满值是 130f，一手 `0x413035`）—— 不是 120 也不是别的 */
        (void)faith_combo_set(&slot, 100);
        (void)faith_combo_set(&slot, slot.combo + mof_core_pickup_combo(0 /* FAITH_TIER_BIG */));
        CHECK(slot.combo == 130);
    }

    /* ============ 符卡「全避」监视器（§2.2「拖到超时 −30000」）============
     * ★ 2026-09-12 审计加：旧判据是「符卡结束且没放灵击」⇒ **每张正常收掉的符卡也扣 30000**。
     *   契约（spec §2.2 / §7.5）：全避 = 这一张**拖到超时**（引擎置 `CE_SPELL_FLAG_TIMED_OUT`）
     *   且本卡没在这一张里放过灵击 ⇒ 才扣。下面逐条钉住。*/
    {
        mof_spell_watch_t w;
        int i;

        /* ① 打掉的符卡：active 全程、一次超时位都没有 ⇒ 下降沿**不扣**（旧实现会扣 1 次）*/
        memset(&w, 0, sizeof w);
        CHECK(mof_core_spell_watch(&w, 1, 0) == 0);      /* 开始 */
        for (i = 0; i < 100; ++i) CHECK(mof_core_spell_watch(&w, 1, 0) == 0);
        CHECK(mof_core_spell_watch(&w, 0, 0) == 0);      /* 下降沿：不扣 */
        CHECK(mof_core_spell_watch(&w, 0, 0) == 0);      /* 结束后继续轮询也不会补扣 */

        /* ② 拖到超时（超时位在符卡中段出现）⇒ 下降沿正好扣**一次** */
        memset(&w, 0, sizeof w);
        CHECK(mof_core_spell_watch(&w, 1, 0) == 0);
        CHECK(mof_core_spell_watch(&w, 1, 0) == 0);
        CHECK(mof_core_spell_watch(&w, 1, 1) == 0);      /* 引擎置超时位（还在符卡里）*/
        CHECK(mof_core_spell_watch(&w, 1, 1) == 0);
        CHECK(mof_core_spell_watch(&w, 0, 1) == 1);      /* ★ 下降沿：扣一次（同帧还看得见位）*/
        CHECK(mof_core_spell_watch(&w, 0, 0) == 0);      /* 不重复扣 */

        /* ③ 超时位与 active **同帧翻转**（锁存的目的）：位只在中段某一帧出现过就够了 */
        memset(&w, 0, sizeof w);
        CHECK(mof_core_spell_watch(&w, 1, 0) == 0);
        CHECK(mof_core_spell_watch(&w, 1, 1) == 0);
        CHECK(mof_core_spell_watch(&w, 0, 0) == 1);      /* 位已消失，但锁存住了 */

        /* ④ 这一张里放过灵击 ⇒ 即使超时也不扣（§2.2）*/
        memset(&w, 0, sizeof w);
        CHECK(mof_core_spell_watch(&w, 1, 0) == 0);
        w.bombed = 1;                                    /* 卡侧在 mof_bomb() 里置位 */
        CHECK(mof_core_spell_watch(&w, 1, 1) == 0);
        CHECK(mof_core_spell_watch(&w, 0, 0) == 0);

        /* ⑤ 新符卡开始清两个标志：上一张的残留超时位**不会误伤**下一张（打掉的）*/
        memset(&w, 0, sizeof w);
        CHECK(mof_core_spell_watch(&w, 1, 0) == 0);
        CHECK(mof_core_spell_watch(&w, 1, 1) == 0);
        CHECK(mof_core_spell_watch(&w, 0, 1) == 1);      /* 第一张：全避 ⇒ 扣 */
        /* 下一张：开始那一帧引擎的位还没清（残留）—— 新符卡第一帧**不读**位 */
        CHECK(mof_core_spell_watch(&w, 1, 1) == 0);
        CHECK(w.timed_out == 0);
        CHECK(mof_core_spell_watch(&w, 1, 0) == 0);
        CHECK(mof_core_spell_watch(&w, 0, 0) == 0);      /* 打掉的 ⇒ 不扣 */

        /* ⑥ NULL 安全 */
        CHECK(mof_core_spell_watch(0, 1, 1) == 0);
    }

    /* ============ 蓝点高度系数与分值（§2.4 / §7.6）============ */
    CHECK(mof_core_height_coeff(100.0f, 128.0f, 448.0f) == MOF_BLUE_COEF_TOP);   /* 线上 */
    CHECK(mof_core_height_coeff(448.0f, 128.0f, 448.0f) == MOF_BLUE_COEF_BOTTOM); /* 板底 */
    CHECK(mof_core_height_coeff(500.0f, 128.0f, 448.0f) == MOF_BLUE_COEF_BOTTOM); /* 出界夹取 */
    {
        int32_t mid = mof_core_height_coeff(288.0f, 128.0f, 448.0f);   /* 正中 */
        CHECK(mid == 770);                                          /* (1000+540)/2 = 770 */
    }
    CHECK(mof_core_blue_value(50000, MOF_BLUE_COEF_TOP) == 50000);       /* 线上满额 */
    CHECK(mof_core_blue_value(50000, MOF_BLUE_COEF_BOTTOM) == 27000);    /* 板底 ≈ 54% */
    CHECK(mof_core_blue_value(50000, 0) == 0);
    CHECK(mof_core_blue_value(0, MOF_BLUE_COEF_TOP) == 0);

    /* ============ 高度系数的**生产路径**（★ 2026-09-12 审计修，任务 1）============
     * 修的是什么：`mof.c` 原来用 `faith_player_above_line() ? TOP : BOTTOM` 二元取值
     * ⇒ **线下 1px 就吃 0.54**，`mof_core_height_coeff()` 的插值是死代码。现在生产路径调
     * `mof_core_pickup_coeff(collect_y, poc_y)`（= 上面那个函数 + `MOF_BOARD_Y` 作板底）。
     * 真实几何（TH18 游玩区高 448、回收线 `player+0x47998` 实跑值 224.0f）：
     *   线上 y ≤ 224 → 1000；板底 y = 448 → 540；跨度 = 224。
     * 手算（线性，向下取整）：
     *   y = 224.0（恰好过线）→ t = 0    → 1000
     *   y = 280.0（1/4）      → t = 0.25 → 1000 − 460×0.25 = 885
     *   y = 336.0（正中）     → t = 0.5  → 1000 − 460×0.5  = 770
     *   y = 392.0（3/4）      → t = 0.75 → 1000 − 460×0.75 = 655
     *   y = 448.0（板底）      → t = 1.0  → 540
     *   y = 225.0（线下 1px）  → t = 1/224 ≈ 0.0044643 → 1000 − 2.0536 = 997.9… → **997**（不是 540！）*/
    CHECK(MOF_BOARD_Y == 448.0f);
    CHECK(mof_core_pickup_coeff(224.0f, 224.0f) == MOF_BLUE_COEF_TOP);           /* 顶端（恰好过线）*/
    CHECK(mof_core_pickup_coeff(100.0f, 224.0f) == MOF_BLUE_COEF_TOP);           /* 线上更高 */
    CHECK(mof_core_pickup_coeff(280.0f, 224.0f) == 885);                         /* 1/4 处 */
    CHECK(mof_core_pickup_coeff(336.0f, 224.0f) == 770);                         /* 中间某点（正中）*/
    CHECK(mof_core_pickup_coeff(392.0f, 224.0f) == 655);                         /* 3/4 处 */
    CHECK(mof_core_pickup_coeff(448.0f, 224.0f) == MOF_BLUE_COEF_BOTTOM);        /* 底端（板底）*/
    CHECK(mof_core_pickup_coeff(600.0f, 224.0f) == MOF_BLUE_COEF_BOTTOM);        /* 板底以下夹取 */
    CHECK(mof_core_pickup_coeff(225.0f, 224.0f) == 997);                         /* ★ 线下 1px ≈ 满额 */
    CHECK(mof_core_pickup_coeff(225.0f, 224.0f) != MOF_BLUE_COEF_BOTTOM);        /* ★ 不再是二元 0.54 */
    CHECK(mof_core_pickup_coeff(300.0f, 448.0f) == MOF_BLUE_COEF_TOP);           /* 跨度非正 + 线上 */
    CHECK(mof_core_pickup_coeff(500.0f, 448.0f) == MOF_BLUE_COEF_BOTTOM);        /* 跨度非正 + 线下 */
    {   /* 单调不增 + 值域夹在 [BOTTOM, TOP]（整条曲线的形状不变式）*/
        int32_t prev = MOF_BLUE_COEF_TOP + 1;
        int ok_mono = 1, ok_range = 1, y;
        for (y = 0; y <= 500; y += 4) {
            int32_t c = mof_core_pickup_coeff((float)y, 224.0f);
            if (c > prev) ok_mono = 0;
            if (c < MOF_BLUE_COEF_BOTTOM || c > MOF_BLUE_COEF_TOP) ok_range = 0;
            prev = c;
        }
        CHECK(ok_mono);
        CHECK(ok_range);
    }
    /* 分值：同一份信仰（50000）随高度线性打折 —— 顶端 50000 / 正中 38500 / 底端 27000 */
    CHECK(mof_core_blue_value(50000, mof_core_pickup_coeff(224.0f, 224.0f)) == 50000);
    CHECK(mof_core_blue_value(50000, mof_core_pickup_coeff(336.0f, 224.0f)) == 38500);  /* 50000×770/1000 */
    CHECK(mof_core_blue_value(50000, mof_core_pickup_coeff(448.0f, 224.0f)) == 27000);  /* 50000×540/1000 */
    CHECK(mof_core_blue_value(50000, mof_core_pickup_coeff(225.0f, 224.0f)) == 49850);  /* 50000×997/1000 */

    /* ============ 卡内奖残（§2.6：20m / 40m / 80m / 150m）============ */
    CHECK(mof_bonus_threshold(0) == MOF_BONUS_M);
    CHECK(mof_bonus_threshold(1) == MOF_BONUS_40M);
    CHECK(mof_bonus_threshold(2) == MOF_BONUS_80M);
    CHECK(mof_bonus_threshold(3) == MOF_BONUS_150M);
    CHECK(mof_bonus_threshold(9) == MOF_BONUS_150M);             /* 越界夹取 */

    mof_bonus_init(&bonus);
    CHECK(mof_bonus_take(&bonus) == 0);
    mof_bonus_add(&bonus, MOF_BONUS_M - 1);
    CHECK(mof_bonus_take(&bonus) == 0);                          /* 差 1 分不给 */
    mof_bonus_add(&bonus, 1);
    CHECK(mof_bonus_take(&bonus) == 1);                          /* 20m ⇒ +1 残 */
    CHECK(mof_bonus_awarded(&bonus) == 1);
    CHECK(mof_bonus_take(&bonus) == 0);                          /* 同一档只发一次 */
    mof_bonus_add(&bonus, MOF_BONUS_150M);                       /* 一口气越 40m/80m/150m */
    CHECK(mof_bonus_take(&bonus) == 3);
    CHECK(mof_bonus_awarded(&bonus) == 4);
    CHECK(mof_bonus_take(&bonus) == 0);                          /* 四档发完就停 */
    mof_bonus_add(&bonus, 99999999999LL);
    CHECK(mof_bonus_take(&bonus) == 0);
    mof_bonus_init(&bonus);
    CHECK(mof_bonus_awarded(&bonus) == 0);
    CHECK(mof_bonus_awarded(0) == 0);                            /* NULL 安全 */

    /* ============ 与 faith_core 的接缝：110 槽的信仰就是 PV 的基础 ============ */
    faith_slot_init_110(&slot);
    (void)faith_slot_add(&slot, FAITH_EV_BIG, 5000);
    (void)faith_slot_reduce(&slot, FAITH_REDUCE_BOMB);            /* 非决死灵击 −3000 */
    CHECK(faith_slot_total(&slot) == 52000);
    CHECK(mof_core_pv(1, 0, faith_slot_total(&slot), 0, 0) == 52000);

    printf("test_mof_core: %d passed, %d failed\n", s_pass, s_fail);
    return s_fail ? 1 : 0;
}
