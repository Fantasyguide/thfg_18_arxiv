/* test_mof_core.c —— `cards/mof_core.c` 的主机单测（`make test-host` 的口径：
 *   cc -std=c11 -Wall -Wextra -Werror -o /tmp/tm tests/test_mof_core.c cards/mof_core.c && /tmp/tm）。
 *
 * 覆盖（用户点名的部分加粗）：
 *   · **PV 公式三态**（只 110 / 只 111 / 两个都装）+ 落笔者只读聚合值 ⇒ 与声明顺序无关；
 *   · **扣火力的 1.05 下限**（含门槛 ≥ 2.00 的拒绝、原生钳不住的那条）；
 *   · 大信仰点判据（`drop_main == 2` / 同时掉钱与 P / **boss 不掉**）+ 难度表 —— **旧口径**，
 *     2026-09-15 起卡侧改走「宿主表」（下面那一大块：表内给 / 表外不给 / 中绿点不受影响）；
 *   · 满 P 时 P 点 → 小信仰点；
 *   · 连击槽的按帧推进 + ★★ **逐帧信仰衰减**（2026-09-15 **装回**：门 =「无 Boss ∧ 无对话 ∧
 *     关卡帧 ≥ 90」三条全满足，槽空才扣；速率两档（无敌人 10800/s / 有敌人 600/s）；下限 = Δ 不越 0）；
 *   · ★ 收集事件档 / 连击帧数（大信仰点 +120、小信仰点 +60 —— 一手订正）；
 *   · 蓝点高度系数与分值，含 ★ **生产路径的线性插值**（顶端 / 中间 / 底端 / 线下 1px）；
 *   · 卡内奖残越阈值（20m/40m/80m/150m）；
 *   · ★★ **P 点火力 +0.05 档**（2026-09-15，110/111 共用）：补差 = 5−1 = 4（**替换语义、补差实现**）、
 *     **两张卡同时在场只算一次**（`sim_p_commit()` 把两张卡的接线一起模型）、**满火力不越上限**、
 *     跨档收尾「一次不多不少」；一手见 `mof_core.h` 的 §P 点火力（`th18:0x446870` → `0x4573f0`）。
 *   · ★★ **爱丽丝的 P 点减半走共享策略（2026-09-15h）**：`sim_p_commit_policy()` 把「落笔者问
 *     `faith_p_topup_total()` → 用 `mof_core_p_topup_units()`」这条新接线模型出来，钉住
 *     **双卡同装 == 只装 111**（修好了）、**每颗道具策略只被问一次**（相位每颗只推进一次）、
 *     10 颗平均 **2.5 单位 = 0.025 档**、常态逐值不变、满火力那颗不吃相位格。
 *
 * 依据：spec §2.1 / §2.6 / §3 / §8.2 + review §3.2 / §3.2.1 / §5.5.1 / §7.1 / §7.2 / §7.4 / §7.5 /
 *       §7.6 / §7.8 / §7.9 / §11 #47a #48；
 *       ★ 一手（覆盖 spec 被证伪的三处）：`local/_scratch/research-th10-faith-and-graze.md`
 *       §3.1（连击表：大信仰点 +120f）/ §1.1（TH10 无擦弹倍率）；
 *       ★★ 2026-09-15 再推翻上面那份文档的一条负结论（它的 §3.2/§3.4「无逐帧信仰衰减」出自
 *       **绝对地址 grep**，抓不到 `eax` 相对寻址）⇒ 现以 `local/_scratch/research-th10-faith-units-and-drain.md`
 *       §3（门链与衰减主体）为准：**有**逐帧衰减，`th10:0x418a17`–`th10:0x418a3c`。
 */
#include <stdio.h>
#include <string.h>

/* `CE_EDEATH_*`（死亡事件的 kind / flags）来自 SDK；显式包含，**不靠** mof_core.h 的传递包含。*/
#include "../sdk/public.h"
#include "../cards/mof_core.h"
#include "../cards/mof_big_faith_table.h"   /* ★ 宿主表本体（表内给/表外不给的那几条断言要直接看它）*/

static int s_fail, s_pass;
#define CHECK(cond) do { if (cond) ++s_pass; else { ++s_fail; printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } } while (0)

/* ============ ★★ P 点火力 +0.05 档：卡侧接线的**可执行模型** ============
 * SDK 的 `BP_ce_item_commit`（`th18:0x44658a`）沿卡链把**每一张**卡的 `on_item_commit` 都调一遍
 * （`native/sdk.c` 的 `BP_ce_item_commit()`）⇒ 「两张卡同装只算一次」这条**只能落在回调自己身上**，
 * 所以它必须被测。这里把 `cards/mof.c` / `cards/signal.c` 的两个 `on_item_commit` 逐句模型出来
 * （两边都只做三件事：问 owner、`mof_core_p_topup()`、写 `CURRENT_POWER`）：
 *   · `have110` / `have111` = 两张卡在不在场（卡侧 = `ce_owned()`）；
 *   · `power` = **原生那一发已经落地之后**读到的 `CE_CURRENT_POWER`；
 *   · `*appliers` / `*repops` 传出「真的写了火力的张数」/「补了跨档收尾的张数」。*/
static int32_t sim_p_commit(int32_t power, int32_t max_power, int have110, int have111,
                            int *appliers, int *repops)
{
    int32_t p = power;
    int id;

    for (id = 110; id <= 111; ++id) {           /* 卡链遍历（顺序不可预期 ⇒ 结论不许依赖它）*/
        mof_p_topup_t t;
        int present = (id == 110) ? have110 : have111;
        if (!present) continue;                /* 卡不在场 ⇒ 它的回调根本不会被调 */
        /* 在场的那张卡问的是 `mof_core_p_owner(自己的 1, 对方的 ce_owned)`；
         * 对在场的卡来说「自己 = 1」恒成立 ⇒ 两侧算出来的都是同一个值。*/
        if (mof_core_p_owner(have110, have111) != id) continue;   /* id 更大 ⇒ 让位 */
        t = mof_core_p_topup(p, max_power);
        if (t.delta > 0) { p += t.delta; if (appliers) ++*appliers; }
        if (t.repop) { if (repops) ++*repops; }
    }
    return p;
}

/* ============ ★★ 2026-09-15h：P 点补值的**共享策略**接线的可执行模型 ============
 * 与 `sim_p_commit()` 的差别：那一支模型的是「常态 +0.05」；这一支多两件事 ——
 *   ① 落笔者先问**共享策略** `faith_core_p_topup_total()`（会推进相位）；
 *   ② 补值用 `mof_core_p_topup_units()`（总量由策略给）。
 * ⚠ **本模型要钉住的不变式**：策略**只在落笔者那一侧、每颗道具恰好被问一次**
 *   ⇒ 相位每颗只推进一次（双卡同装时 111 在 `mof_core_p_owner()` 那一关就 `continue` 了，
 *   **它连策略都不问** —— 这正是 `cards/signal.c` 里 `!= 111` 那一行早退的模型）。
 *   · `is_marisa` = `CE_ADDR_CHARACTER == 1` 的模型（爱丽丝的机体门）；
 *   · `usable` = 本颗**拿得到火力**（= 卡侧与 `faith_p_topup_take()` 读到的 `power < max`）；
 *   · `*takes` = 策略被问了几次（每颗应为 1）—— 相位计数 `alt->items` 应与它同步。*/
static int32_t sim_p_commit_policy(int32_t power, int32_t max_power, int have110, int have111,
                                   int is_marisa, faith_p_alt_t *alt,
                                   int *appliers, int *takes)
{
    int32_t p = power;
    int id;

    for (id = 110; id <= 111; ++id) {           /* 卡链遍历（顺序不可预期 ⇒ 结论不许依赖它）*/
        mof_p_topup_t t;
        int present = (id == 110) ? have110 : have111;
        if (!present) continue;                /* 卡不在场 ⇒ 它的回调根本不会被调 */
        if (mof_core_p_owner(have110, have111) != id) continue;   /* id 更大 ⇒ 让位（**连策略都不问**）*/
        {
            int usable = (max_power > 0 && p < max_power) ? 1 : 0;
            int32_t total = faith_core_p_topup_total(alt, faith_core_p_alice_on(have111, is_marisa),
                                                     usable);
            if (takes) ++*takes;
            t = mof_core_p_topup_units(p, max_power, total);
        }
        if (t.delta > 0) { p += t.delta; if (appliers) ++*appliers; }
    }
    return p;
}

/* ============ ★★ 「灵击 → 决死」：`cards/mof.c` 里与信仰有关那几步的**可执行模型** ============
 * 用户口径（2026-09-15 逐字）：「**基本语义上灵击==B**」「**所以灵击也得决死**」。
 * 把 `mof_bomb_start()` + `mof_bomb_tick()` 里**碰信仰槽**的三步逐句模型出来（其余步骤与信仰无关，
 * 由 `cards/mof.c` 自己保证）：
 *   ① `mof_bomb_form_start(&s->bomb.form, by_hit)`（记形态）；
 *   ② `faith_slot_reduce(&s->slot, mof_bomb_form_faith_kind(&s->bomb.form))`（主动 −3000 / 决死 0）；
 *   ③ 消 `bullets` 颗：入不入账问 `mof_bomb_form_cancel_accounts()`，入账那一支走
 *      `mof_on_cancel()` 里那一句 `faith_slot_add_cancel(&s->slot, 0, n, 0, 0)`（110 每颗 +10）。
 * 返回该发结束后的信仰总值（显示口径，基础 50000 + Δ）。*/
static int32_t sim_bomb_faith(faith_slot_t *slot, mof_bomb_form_t *f, int32_t by_hit, int32_t bullets)
{
    mof_bomb_form_start(f, by_hit);
    (void)faith_slot_reduce(slot, mof_bomb_form_faith_kind(f));
    if (bullets > 0) {
        mof_bomb_form_cancel_note(f, bullets);
        if (mof_bomb_form_cancel_accounts(f)) (void)faith_slot_add_cancel(slot, 0, bullets, 0, 0);
    }
    return faith_slot_total(slot);
}

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

    /* ============ ★★ 2026-09-15：大信仰点判据 = **宿主表**（NEXT.md §1x.5）============
     * 用户实测「大绿点与金钱完全重合」+ 裁决「**金钱和绿点总之别重合**」⇒ 走「判据收窄」：
     * 判据从「这敌掉的是不是金钱（`drop_main == 2`）」换成「**它在不在宿主表里**」。
     * 表本体 + 全套一手预研（TH10 逐关清单 / 计数 / 映射 / 对不上的清单）在
     * `cards/mof_big_faith_table.h`；键 = `(anm 源 N, 主脚本 S)` = 这敌画的是哪一张贴图。
     * ★ 表里两类宿主：绿妖精族（复刻）+ 大妖精族（**用户 2026-09-15 口径**，见 `TUNING.md` §2.5f #5）
     * （一手 `th18:0x436c85`/`th18:0x436c91`（anmSetSprite）与 `th18:0x431659`/`th18:0x431675`
     * （anmSetMain），运行时绝对偏移 `enemy+0x1494` / `enemy+0x1498`）。*/
    {
        const uint32_t OK_FLAGS = CE_EDEATH_F_REAL | CE_EDEATH_F_HAS_DROP;
        int32_t hv;

        /* ① **表内的敌人给**（6 个视觉身份逐个钉）：
         *   (a) 绿妖精族 `(2,10)` `(2,56)` `(2,68)` `(2,80)` = 复刻（贴图实测为绿）；
         *   (b) 大妖精族 `(2,40)` `(2,224)` = **用户 2026-09-15 口径**（原话「大妖精掉什么都不奇怪，
         *       可以直接大方往里加」）—— TH10 的 `Girl01/02/03` 确实在 11 个创建点上掉 type 3。*/
        CHECK(mof_core_big_faith_host(2, 10) == 1);   /* GirlBlueA03(st05) / GirlA04(st06) */
        CHECK(mof_core_big_faith_host(2, 40) == 1);   /* ★ 大妖精（橙）: GirlD01/D02/D03 多数关 */
        CHECK(mof_core_big_faith_host(2, 56) == 1);   /* Cir01a/02a(st01) Cir00/01(st02) … */
        CHECK(mof_core_big_faith_host(2, 68) == 1);   /* Cir00m(st01) */
        CHECK(mof_core_big_faith_host(2, 80) == 1);   /* Cir00bm(st01) */
        CHECK(mof_core_big_faith_host(2, 224) == 1);  /* ★ 大妖精第二套贴图: st02/st07 */

        /* ② **表外的敌人不给**（含旧口径下会被误给的同类：蓝/红妖精都是「掉钱的」那批）*/
        CHECK(mof_core_big_faith_host(2, 0) == 0);    /* GirlBlueA01：蓝妖精（实测 hue 228）*/
        CHECK(mof_core_big_faith_host(2, 5) == 0);    /* GirlRedA01：红妖精（实测 hue 12）*/
        CHECK(mof_core_big_faith_host(2, 20) == 0);   /* GirlB01 */
        CHECK(mof_core_big_faith_host(2, 35) == 0);   /* GirlC01（青）*/
        CHECK(mof_core_big_faith_host(2, 44) == 0);   /* 大妖精族的**邻近**脚本号也不许误命中 */
        CHECK(mof_core_big_faith_host(2, 41) == 0);
        CHECK(mof_core_big_faith_host(2, 220) == 0);
        CHECK(mof_core_big_faith_host(2, 225) == 0);
        CHECK(mof_core_big_faith_host(3, 0) == 0);    /* st02enm script0：贴图也发绿，但用它的全是 boss */
        CHECK(mof_core_big_faith_host(2, -1) == 0);   /* 没设过主脚本（防御）*/
        CHECK(mof_core_big_faith_host(-1, 10) == 0);  /* anm 源不对 ⇒ 不给（键是两元组）*/
        CHECK(mof_core_big_faith_host(0, 0) == 0);

        /* ③ 完整入口：门 + 表 + **非 boss** + 难度值（`drop_main` **已不是入参**）*/
        hv = -1;
        CHECK(mof_core_wants_big_host(CE_EDEATH_KILLED, OK_FLAGS, 2, 10, 0, FAITH_DIFF_HARD, &hv) == 1);
        CHECK(hv == 8000);
        CHECK(mof_core_wants_big_host(CE_EDEATH_KILLED, OK_FLAGS, 2, 56, 0, FAITH_DIFF_EASY, &hv) == 1);
        CHECK(hv == 5000);
        hv = -1;
        CHECK(mof_core_wants_big_host(CE_EDEATH_KILLED, OK_FLAGS, 2, 10, 1, FAITH_DIFF_HARD, &hv) == 0);
        CHECK(hv == 0);                                                     /* boss：不给，且清零 */
        CHECK(mof_core_wants_big_host(CE_EDEATH_KILLED, OK_FLAGS, 2, 0, 0, FAITH_DIFF_HARD, &hv) == 0);
        CHECK(hv == 0);                                                     /* 表外：不给 */
        CHECK(mof_core_wants_big_host(CE_EDEATH_KILL_ALL, OK_FLAGS, 2, 10, 0, FAITH_DIFF_HARD, &hv) == 0);
        CHECK(mof_core_wants_big_host(CE_EDEATH_KILLED, CE_EDEATH_F_HAS_DROP, 2, 10, 0,
                                      FAITH_DIFF_HARD, &hv) == 0);          /* 非真敌人 */
        CHECK(mof_core_wants_big_host(CE_EDEATH_KILLED, 0, 2, 10, 0, FAITH_DIFF_HARD, &hv) == 0);
        CHECK(mof_core_wants_big_host(CE_EDEATH_KILLED, OK_FLAGS, 2, 10, 0, FAITH_DIFF_LUNATIC, 0) == 1);
        /* ★ **门去掉了「必须有掉落表」那一条**（宿主按外观定：st04 的 Cir02/Cir03 原生 item = 0）
         *   ⇒ `CE_EDEATH_F_HAS_DROP` 缺席也必须能过（对照：旧门 `mof_core_death_qualifies` 会拒绝）。*/
        CHECK(mof_core_death_is_real(CE_EDEATH_KILLED, CE_EDEATH_F_REAL) == 1);
        CHECK(mof_core_death_qualifies(CE_EDEATH_KILLED, CE_EDEATH_F_REAL) == 0);
        CHECK(mof_core_wants_big_host(CE_EDEATH_KILLED, CE_EDEATH_F_REAL, 2, 56, 0,
                                      FAITH_DIFF_EASY, &hv) == 1);
        CHECK(hv == 5000);

        /* ④ **本轮改掉的那个分叉点**（掉钱的蓝妖精）：旧口径给、新判据不给 ——
         *   这正是用户看到的「大绿点与金钱 1:1」的来源 ⇒ 两条断言并排钉，防止有人把旧路接回去。*/
        CHECK(mof_core_wants_big_main(MOF_DROP_TYPE_MONEY, 0) == 1);        /* 旧口径（已退役）*/
        CHECK(mof_core_wants_big_host(CE_EDEATH_KILLED, OK_FLAGS, 2, 0, 0, FAITH_DIFF_HARD, &hv) == 0);
        /* 反过来：宿主**不必**掉钱（`drop_main` 不是入参）⇒ 掉 P 的、不掉东西的宿主一样给 */
        CHECK(mof_core_wants_big_host(CE_EDEATH_KILLED, OK_FLAGS, 2, 68, 0, FAITH_DIFF_HARD, &hv) == 1);

        /* ⑤ **中绿点（满 P 转换）那条路不受本表影响**：它在 `mof_core_full_p_replace()` +
         *   共用件池扫描（`faith_convert_full_p_items()`）上，与宿主表**零交集**。*/
        CHECK(mof_core_full_p_replace(MOF_FULLP_ITEM_SMALL_P, 500, 500) == 1);
        CHECK(mof_core_full_p_replace(MOF_FULLP_ITEM_BIG_P, 500, 500) == 1);
        CHECK(mof_core_full_p_replace(MOF_FULLP_ITEM_SMALL_P, 499, 500) == 0);
        /* 宿主表**不改变**「满 P 转换走哪一档」：大/中绿点的档位仍由 `FAITH_SRC_FULL_P` 决定 */
        CHECK((int)faith_core_tier_for_source(FAITH_SRC_FULL_P) == (int)FAITH_TIER_SMALL);
        CHECK((int)faith_core_tier_for_source(FAITH_SRC_BREAK) == (int)FAITH_TIER_BIG);
        /* 表本身**不含**任何「P 点 / 满 P」的判据（结构上就无从影响）：全部项都只在 N=2 上 */
        {
            int i, all_env = 1;
            for (i = 0; i < MOF_BIG_FAITH_HOSTS_N; ++i)
                if (MOF_BIG_FAITH_HOSTS[i].anm_src != 2) all_env = 0;
            CHECK(all_env);
            CHECK(MOF_BIG_FAITH_HOSTS_N == 6);   /* 4 绿妖精 + 2 大妖精（用户口径）*/
        }
    }

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
    /* ★ 2026-09-15 一手订正（research-th11-faith-and-points.md §2.3）：整数阶梯、每 100 擦弹一步 */
    CHECK(mof_core_graze_mult(0) == 0);
    CHECK(mof_core_graze_mult(1) == 0);                          /* 不足一步 ⇒ 0 */
    CHECK(mof_core_graze_mult(99) == 0);
    CHECK(mof_core_graze_mult(100) == 10);                       /* 1 步 = 0.01 = 千分点 10 */
    CHECK(mof_core_graze_mult(199) == 10);                       /* 零头不计（原版 ⌊graze/100⌋）*/
    CHECK(mof_core_graze_mult(1000) == 100);                     /* 1000 graze = ×0.1 */
    CHECK(mof_core_graze_mult(89899) == 8980);                   /* 上限前一步 */
    CHECK(mof_core_graze_mult(89900) == 8990);                   /* ★ 上限 8.99 的触发点 = 89900 擦弹 */
    CHECK(mof_core_graze_mult(89999) == 8990);
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

    /* ============ ★★ 逐帧信仰衰减 + 连击槽（2026-09-15 从「无」改回「有」）============
     * 背景：2026-09-12 曾按「TH10 没有逐帧信仰衰减」把这一段整段删掉 —— 那条结论出自**绝对地址
     *   grep**，而真正的衰减全是 `eax` 相对寻址（`eax = 0x474c40` ⇒ `[eax+0xc]`/`[eax+0x10]`/
     *   `[eax+0x18]`）⇒ 搜不到。2026-09-15 复核推翻（`research-th10-faith-units-and-drain.md` §3/§6）。
     * 一手（th10 v1.00a）：
     *   门（`th10:0x418740`–`0x41875d`，**三个全满足**才走衰减，任一不满足跳到 skip）：
     *     ① 无 Boss `[ENEMY_MANAGER+0x10] == 0`  ② 无对话 `[GUI+0x9eb8] == 0`
     *     ③ 关卡帧 ≥ 90 `[g_GameThread+0x14] >= 0x5a`
     *   动作（`th10:0x418a00`，唯一调用点 `th10:0x418764` 在三道门之后）：
     *     槽 `[eax+0x18]` > 0 ⇒ 只 −1（`th10:0x418a10`，**不扣信仰**）；
     *     槽 ≤ 0         ⇒ `[eax+0xc] −= [eax+0x10]`（`th10:0x418a26`），速率当场抬到下限 18
     *                       （`th10:0x418a30` 的机器码 `c7 40 10 12 00 00 00` 实测）⇒ **恒 −18 存储/帧**
     *                       = **−180 显示/帧**，结果硬钳存储 5000（`th10:0x418a3c`）。
     *   ⇒ 槽与衰减**同一个门** ⇒ boss 战 / 对话中 / 前 90 帧内，**两者一起冻结**。*/
    {
        mof_faith_gate_t ok;
        /* `ok` = 三条门齐 + **本帧没有击破**（⇒ 快档 18000 = −180 显示/帧；有击破的慢档在下面单独钉）*/
        ok.no_boss = 1; ok.no_dialogue = 1; ok.stage_frames_ok = 1;
        ok.enemies_present = 0; ok.killed_since_last = 0;

        /* ① 一条门一条门地缺 ⇒ 一个字节都不许动（槽也冻结）*/
        faith_slot_init_110(&slot);
        (void)faith_slot_add(&slot, FAITH_EV_BIG, 10000);        /* Δ = 10000 原始单位 */
        (void)faith_combo_set(&slot, 0);                          /* 槽空（衰减的前提）*/
        {
            mof_faith_gate_t g;
            g = ok; g.no_boss = 0;                                     /* 有 Boss */
            CHECK(mof_core_faith_tick(&slot, &g, 1) == 0);
            g = ok; g.no_dialogue = 0;                                 /* 对话中 */
            CHECK(mof_core_faith_tick(&slot, &g, 1) == 0);
            g = ok; g.stage_frames_ok = 0;                             /* 关卡帧 < 90 */
            CHECK(mof_core_faith_tick(&slot, &g, 1) == 0);
        }
        CHECK(slot.delta == 10000 * FAITH_FIXED);                 /* ★ 三门各缺一次 ⇒ 分毫未动 */
        CHECK(faith_slot_total(&slot) == 60000);

        /* ② 三条齐了：槽空 ⇒ 按 18000 定点/帧扣（= 一手 −180 显示/帧）*/
        CHECK(mof_core_faith_tick(&slot, &ok, 1) == 18000);
        CHECK(slot.delta == (10000 * FAITH_FIXED) - 18000);
        CHECK(mof_core_faith_tick(&slot, &ok, 1) == 18000);
        CHECK(slot.delta == (10000 * FAITH_FIXED) - 36000);
        /* 60 帧（= 1 s）⇒ 正好 −10800 显示 = 一手的 −10800/s */
        {
            faith_slot_t t;
            int32_t before;
            faith_slot_init_110(&t);
            (void)faith_slot_add(&t, FAITH_EV_BIG, 20000);
            t.combo = 0;
            before = t.delta;
            CHECK(mof_core_faith_tick(&t, &ok, 60) == FAITH_EMPTY_NO_ENEMY_PER_SEC * FAITH_FIXED);
            CHECK(before - t.delta == 10800 * FAITH_FIXED);       /* ★ 与 FAITH_EMPTY_NO_ENEMY_PER_SEC 同值 */
            CHECK(faith_slot_total(&t) == 50000 + 20000 - 10800);
        }

        /* ②b ★★ **两档，判据 = 这一帧有没有击破**（2026-09-18 逐条复核的定案）：
         *    一手 `th10:0x40e26b`（敌机死亡路径）把速率字段 `0x474c50` 写 1，而衰减帧
         *    `0x418a26 sub ecx,edx` 用的是**读到的原值**、之后 `0x418a30` 才抬到 18
         *    ⇒ 有击破的帧扣 1 存储（−10 显示 = 1000 定点）、没击破的帧扣 18 存储（−180 显示 = 18000 定点）。
         *    ⚠ 中间的两次误判（「一手只有一档」/「撤掉 600 那档」）见 `TUNING.md` §2.5j 的撤回记录。*/
        CHECK(mof_faith_rate_fixed(1) == 1000);                   /* 本帧有击破 ⇒ 慢档 */
        CHECK(mof_faith_rate_fixed(0) == 18000);                  /* 本帧没击破 ⇒ 快档 */
        CHECK(mof_faith_rate_fixed(-1) == 1000);                  /* 非 0/1 ⇒ 按「有击破」保守处理 */
        CHECK(mof_faith_rate_fixed(1) == MOF_FAITH_DRAIN_KILL_FIXED);
        CHECK(mof_faith_rate_fixed(0) == MOF_FAITH_DRAIN_NO_BOSS_FIXED);
        CHECK(mof_faith_rate_fixed(0) == 18 * mof_faith_rate_fixed(1));   /* 18 存储/帧 vs 1 存储/帧 */
        {
            faith_slot_t e;
            mof_faith_gate_t ge = ok;
            faith_slot_init_110(&e);
            (void)faith_slot_add(&e, FAITH_EV_BIG, 20000);
            e.combo = 0;
            ge.killed_since_last = 1;                             /* 本帧击破过 ⇒ 慢档 */
            CHECK(mof_core_faith_tick(&e, &ge, 1) == 1000);
            CHECK(mof_faith_rate_fixed(1) * 60 == FAITH_EMPTY_ENEMY_PER_SEC * FAITH_FIXED);  /* =600/s */
            CHECK(mof_faith_rate_fixed(0) * 60 == FAITH_EMPTY_NO_ENEMY_PER_SEC * FAITH_FIXED); /* =10800/s */
        }

        /* ③ 槽 > 0：只 −1、**不扣信仰**；而门不成立时**槽也冻**（一手：同一组门）*/
        faith_slot_init_110(&slot);
        (void)faith_slot_add(&slot, FAITH_EV_BIG, 10000);
        (void)faith_combo_set(&slot, 2);
        CHECK(mof_core_faith_tick(&slot, &ok, 1) == 0);
        CHECK(slot.combo == 1);
        CHECK(slot.delta == 10000 * FAITH_FIXED);                 /* 有槽 ⇒ 信仰不动 */
        CHECK(mof_core_faith_tick(&slot, &ok, 1) == 0);
        CHECK(slot.combo == 0);
        CHECK(slot.delta == 10000 * FAITH_FIXED);
        /* 槽刚归零的**那一帧**不扣（一手是 `jle` 二选一分叉，不是「先 −1 再判」）*/
        (void)faith_combo_set(&slot, 1);
        CHECK(mof_core_faith_tick(&slot, &ok, 1) == 0);           /* 这一帧把 1 减到 0 */
        CHECK(slot.combo == 0 && slot.delta == 10000 * FAITH_FIXED);
        CHECK(mof_core_faith_tick(&slot, &ok, 1) == 18000);       /* 下一帧才开始衰减 */
        /* 门缺一条 ⇒ 槽不 −1（boss 战 / 对话 / 开场 90 帧三种情况都冻结槽）*/
        {
            mof_faith_gate_t g = ok;
            (void)faith_combo_set(&slot, 5);
            g.no_boss = 0;
            CHECK(mof_core_faith_tick(&slot, &g, 1) == 0);
            CHECK(slot.combo == 5);
            g = ok; g.stage_frames_ok = 0;
            CHECK(mof_core_faith_tick(&slot, &g, 1) == 0);
            CHECK(slot.combo == 5);
        }

        /* ④ 下限 = 「Δ 不越 0」（等价于一手的「钳在存储 5000 = 显示 50000」）*/
        faith_slot_init_110(&slot);
        (void)faith_slot_add(&slot, FAITH_EV_BIG, 100);           /* Δ = 100 显示 */
        (void)faith_combo_set(&slot, 0);
        CHECK(mof_core_faith_tick(&slot, &ok, 600) == 100 * FAITH_FIXED);  /* 只扣得掉这 100 */
        CHECK(slot.delta == 0);
        CHECK(faith_slot_total(&slot) == FAITH_BASE_110);         /* ★ 正好停在基础值上 */
        /* 已经是 0 ⇒ 再跑多少帧都是 0（边界：正好 0 时不再减）*/
        CHECK(mof_core_faith_tick(&slot, &ok, 1) == 0);
        CHECK(mof_core_faith_tick(&slot, &ok, 600) == 0);
        CHECK(slot.delta == 0 && faith_slot_total(&slot) == FAITH_BASE_110);

        /* ⑤ 边界：`stage_frames == 90` 算过（一手 `jl` ⇒ 89 不过、90 过）*/
        faith_slot_init_110(&slot);
        (void)faith_slot_add(&slot, FAITH_EV_BIG, 10000);
        (void)faith_combo_set(&slot, 0);
        {
            mof_faith_gate_t g = ok;
            g.stage_frames_ok = (90 >= 90);                        /* 卡侧判据 = `>= 90` */
            CHECK(mof_core_faith_tick(&slot, &g, 1) == 18000);     /* ★ 正好 90 帧：过 */
            g.stage_frames_ok = (89 >= 90);
            CHECK(mof_core_faith_tick(&slot, &g, 1) == 0);         /* 89 帧：不过 */
        }

        /* ⑥ 旧入口（薄包装）：门只映射了「无 Boss」这一条 ⇒ 语义比一手**弱**（缺两条门）。
         *    保留只为不改既有签名；`in_boss_fight = 1` 时与一手同向（冻结）。*/
        faith_slot_init_110(&slot);
        (void)faith_slot_add(&slot, FAITH_EV_BIG, 10000);
        (void)faith_combo_set(&slot, 0);
        CHECK(mof_core_combo_tick(&slot, 1) == 0);                /* 有 boss ⇒ 冻结 */
        CHECK(slot.delta == 10000 * FAITH_FIXED);
        CHECK(mof_core_combo_tick(&slot, 0) == 18000);            /* 无 boss ⇒ 衰减（另两条门按最宽松）*/
        (void)faith_combo_set(&slot, 3);
        CHECK(mof_core_combo_tick(&slot, 0) == 0);
        CHECK(slot.combo == 2);                                   /* 旧入口仍推进槽 */
        CHECK(slot.delta == (10000 * FAITH_FIXED) - 18000);       /* 有槽 ⇒ 不扣 */
    }
    /* NULL 安全（三个入口各一遍）*/
    CHECK(mof_core_faith_tick(0, 0, 1) == 0);
    CHECK(mof_core_combo_tick(0, 0) == 0);

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

    /* ============ ★★ 2026-09-16 补：**引擎道具与击破**的连击充能（原来一条都没接）============
     * 症状（用户实跑逐字）：「连击值一点也不涨，击破妖精是涨连击值的啊」——
     * 一手 th10 的连击槽有 9 个充能调用点，我们原来只接了「自建点被收」与「消弹微档」两条。
     * 下面把**引擎侧那两条**的映射钉死（表与一手立即数见 `mof_core.h`）。*/
    {
        /* 引擎道具（TH18 item type → 帧数）：1 小 P +60 / 2 金钱（蓝点）+100 / 3 大 P +20 */
        CHECK(mof_core_item_combo(1 /* MOF_P_ITEM_TYPE18 */) == MOF_COMBO_SMALL_P);
        CHECK(mof_core_item_combo(1) == 60);
        CHECK(mof_core_item_combo(MOF_ITEM_TYPE18_MONEY) == MOF_COMBO_BLUE);
        CHECK(mof_core_item_combo(2) == 100);                     /* 一手 `0x41b56e push 0x64` */
        CHECK(mof_core_item_combo(MOF_ITEM_TYPE18_BIG_P) == MOF_COMBO_BIG_P);
        CHECK(mof_core_item_combo(3) == 20);                      /* 一手 `0x41b6bd push 0x14` */
        /* 其余 type 一律 0（命碎 4 / 整命 5 / 符卡碎 6 / 补满火力 8 / 转换点 9–0xe / 卡道具 0x10+）*/
        CHECK(mof_core_item_combo(0) == 0);
        CHECK(mof_core_item_combo(4) == 0);
        CHECK(mof_core_item_combo(5) == 0);                       /* 1UP = +0f（用户口径与一手一致）*/
        CHECK(mof_core_item_combo(6) == 0);
        CHECK(mof_core_item_combo(8) == 0);
        CHECK(mof_core_item_combo(9) == 0);
        CHECK(mof_core_item_combo(0xe) == 0);
        CHECK(mof_core_item_combo(0x10) == 0);
        CHECK(mof_core_item_combo(-1) == 0);

        /* 击破非 boss = +10f（一手 `th10:0x40e681 push 0xa`）；boss 由卡侧不调 ⇒ +0f */
        CHECK(MOF_COMBO_KILL == 10);
        faith_slot_init_110(&slot);
        CHECK(slot.combo == 0);
        (void)faith_combo_set(&slot, slot.combo + MOF_COMBO_KILL);        /* 一只妖精 */
        CHECK(slot.combo == 10);
        (void)faith_combo_set(&slot, slot.combo + mof_core_item_combo(2)); /* 顺路吃一颗钱 */
        CHECK(slot.combo == 110);
        /* 满槽夹取：130 是上限（一手 `0x413035`），多了不溢出 */
        (void)faith_combo_set(&slot, slot.combo + mof_core_pickup_combo(0 /* BIG */));
        CHECK(slot.combo == 130);
        /* 大信仰点宿主的一击 = 拾取 120 + 击破 10 = **用户口径里的 130**（`AUDIT.md` §BC3 的算术）*/
        CHECK(MOF_COMBO_BIG + MOF_COMBO_KILL == 130);
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

    /* ============ 灵击：半径曲线（★ 2026-09-15 一手）============
     * 一手：`th10:0x405813`（`bomb+0x3c` 每帧被灵击 anm VM 的 `scale.y` 覆盖）+ `bullet.anm`
     * script400/407/441 的 `scale(16,16)` + `scaleTime(120, 6, 16.0 → 160/112/64)`
     * ⇒ 帧 0 = 16.00、120 帧长到目标、其后保持。**不是**旧实现的「全屏 4000.0f」。
     * ★★ 2026-09-16o 订正：那条 `scaleTime(120, 6, …)` 的 **6 = 缓动编号**（`1−(1−u)⁴`，ease-out 四次），
     *   **不是线性**（缓动表见 `engine/anm/th16/07-general.md` §4）⇒ 半程已经到 88.3%，早期窜得快。*/
    CHECK(mof_core_bomb_radius_centi(0, MOF_BOMB_R_REIMU_CENTI) == 1600);      /* 帧 0 = 16.00 */
    CHECK(mof_core_bomb_radius_centi(-5, MOF_BOMB_R_REIMU_CENTI) == 1600);     /* 负帧防御 */
    /* 灵梦档 = 16 + 144 × (1 − (1−t/120)⁴)（1/100 px）*/
    CHECK(mof_core_bomb_radius_centi(1, MOF_BOMB_R_REIMU_CENTI) > 1600);      /* 第 1 帧就已经窜出去 */
    CHECK(mof_core_bomb_radius_centi(1, MOF_BOMB_R_REIMU_CENTI) < 2200);      /* ≈ 20.7 px（≈3% 的量程）*/
    CHECK(mof_core_bomb_radius_centi(30, MOF_BOMB_R_REIMU_CENTI) > 1600 + (16000 - 1600) / 2);  /* 30 帧已过半程 */
    CHECK(mof_core_bomb_radius_centi(60, MOF_BOMB_R_REIMU_CENTI) > 1600 + (16000 - 1600) * 8 / 10);
    CHECK(mof_core_bomb_radius_centi(119, MOF_BOMB_R_REIMU_CENTI) > 15900);
    CHECK(mof_core_bomb_radius_centi(120, MOF_BOMB_R_REIMU_CENTI) == 16000);   /* ★ 顶到 160.00 */
    CHECK(mof_core_bomb_radius_centi(121, MOF_BOMB_R_REIMU_CENTI) == 16000);   /* 只增不减、保持 */
    CHECK(mof_core_bomb_radius_centi(99999, MOF_BOMB_R_REIMU_CENTI) == 16000);
    /* 魔理沙档 112.00 与符卡档 64.00：同样缓动、只换目标 */
    CHECK(mof_core_bomb_radius_centi(60, MOF_BOMB_R_MARISA_CENTI) > 1600 + (11200 - 1600) * 8 / 10);
    CHECK(mof_core_bomb_radius_centi(120, MOF_BOMB_R_MARISA_CENTI) == 11200);
    CHECK(mof_core_bomb_radius_centi(60, MOF_BOMB_R_SPELL_CENTI) > 1600 + (6400 - 1600) * 8 / 10);
    CHECK(mof_core_bomb_radius_centi(120, MOF_BOMB_R_SPELL_CENTI) == 6400);
    {   /* 单调不减 + 三个目标值都夹在 [16.00, target]（曲线形状不变式）*/
        static const int32_t targets[3] = { MOF_BOMB_R_REIMU_CENTI, MOF_BOMB_R_MARISA_CENTI,
                                            MOF_BOMB_R_SPELL_CENTI };
        int t, f, mono_ok = 1, range_ok = 1;
        for (t = 0; t < 3; ++t) {
            int32_t prev = 0;
            for (f = 0; f <= 200; ++f) {
                int32_t r = mof_core_bomb_radius_centi(f, targets[t]);
                if (f > 0 && r < prev) mono_ok = 0;
                if (r < MOF_BOMB_R_START_CENTI || r > targets[t]) range_ok = 0;
                prev = r;
            }
        }
        CHECK(mono_ok);
        CHECK(range_ok);
    }
    /* 退化目标（≤ 起点）时直接取目标值，不外推出怪数 */
    CHECK(mof_core_bomb_radius_centi(60, 1000) == 1000);
    CHECK(mof_core_bomb_radius_centi(60, -5) == -5);

    /* 中心每帧上移（一手 `th10:0x4057b6`（符卡 0.5）/ `th10:0x4057cd`（灵梦 1.0）/ `th10:0x4057d5`（魔理沙 1.3））
     * —— 三支**互斥**（不是叠加）。*/
    CHECK(mof_core_bomb_rise_milli(1, 1) == 500);     /* 符卡中：0.5（角色无关）*/
    CHECK(mof_core_bomb_rise_milli(1, 0) == 500);
    CHECK(mof_core_bomb_rise_milli(0, 1) == 1000);    /* 非符卡 + 灵梦：1.0 */
    CHECK(mof_core_bomb_rise_milli(0, 0) == 1300);    /* 非符卡 + 魔理沙：1.3 */

    /* ============ 灵击：伤害表与缩放链（★ 2026-09-15 一手）============
     * 一手 `th10:0x4059f0` 的返回表 + `th10:0x40e141`–`0x40e19e` 的缩放：
     *   raw → /5（player_state ∈ {0,2}）→ 0 则整笔跳过 → 符卡中再 /5、下限 1。*/
    CHECK(MOF_BOMB_DMG_IN_SPELL_NOBIG == 3);          /* 表本身（一手立即数）*/
    CHECK(MOF_BOMB_DMG_IN_SPELL_BIG == 38);
    CHECK(MOF_BOMB_DMG_IN_BOSS == 38);
    CHECK(MOF_BOMB_DMG_IN_PLAIN == 5);
    CHECK(MOF_BOMB_DMG_NONE == 0);
    CHECK(MOF_BOMB_DMG_DIV == 5);
    CHECK(MOF_BOMB_BIG_BOMB_FRAMES == 60);            /* `bomb+0x44` 的门（`th10:0x4059ca cmp …,0x3c`）*/
    /* 圈内 / 无符卡 / 有 boss → 38/5 = 7（一手主用例）*/
    CHECK(mof_core_bomb_damage(1, 1, 0, 0, 1) == 7);
    /* 圈内 / 无符卡 / 无 boss → 5/5 = 1（小怪阶段也有伤害，且不被归零）*/
    CHECK(mof_core_bomb_damage(1, 0, 0, 0, 1) == 1);
    /* 圈内 / 符卡中 / `bomb+0x44==0`（符卡前 60 帧）→ 3/5 = 0 ⇒ **整笔跳过**（一手 `0x40e169` 的行为）*/
    CHECK(mof_core_bomb_damage(1, 1, 1, 0, 1) == 0);
    /* 圈内 / 符卡中 / `bomb+0x44==1` → 38/5 = 7 →boss+符卡 /5 = 1（下限 1）*/
    CHECK(mof_core_bomb_damage(1, 1, 1, 1, 1) == 1);
    /* 圈外 / 无符卡 / 有 boss：灵梦 20/5 = 4、魔理沙 16/5 = 3 */
    CHECK(mof_core_bomb_damage(0, 1, 0, 0, 1) == 4);
    CHECK(mof_core_bomb_damage(0, 1, 0, 0, 0) == 3);
    /* 圈外 / 无符卡 / 无 boss ⇒ 0（**圈外只有有 boss 才有伤害**，一手 `th10:0x405a95`）*/
    CHECK(mof_core_bomb_damage(0, 0, 0, 0, 1) == 0);
    /* 圈外 / 符卡 / 大灵击：灵梦 26/5 = 5 → /5 = 1（下限）；魔理沙 23/5 = 4 → /5 = 0 → 下限 1 */
    CHECK(mof_core_bomb_damage(0, 0, 1, 1, 1) == 1);
    CHECK(mof_core_bomb_damage(0, 0, 1, 1, 0) == 1);
    /* 圈外 / 符卡 / 非大灵击 ⇒ 0（一手 `th10:0x405a76 cmp [ecx+0x44],0x1; jne → 0`）*/
    CHECK(mof_core_bomb_damage(0, 0, 1, 0, 1) == 0);
    /* 值域：任何组合都落在 {0,1,3,4,7}（张表的全部结局）*/
    {
        int in_c, ba, sp, big, re, ok = 1;
        for (in_c = 0; in_c < 2; ++in_c)
        for (ba = 0; ba < 2; ++ba)
        for (sp = 0; sp < 2; ++sp)
        for (big = 0; big < 2; ++big)
        for (re = 0; re < 2; ++re) {
            int32_t d = mof_core_bomb_damage(in_c, ba, sp, big, re);
            if (d != 0 && d != 1 && d != 3 && d != 4 && d != 7) ok = 0;
        }
        CHECK(ok);
    }

    /* ============ ★★ 满 P 转换：**替换**判据（2026-09-15 收口，A + B）============
     * 一手 = **改写道具自己的 type**（`th10:0x41bbf7`–`0x41bc23`：满火力 100 单位 = 5.00 时
     * type 1/4 → 9 小信仰点、type 10/11 → 5 点道具）+ **满的那一帧全场转换**（`th10:0x41ba50`
     * / `th11:0x4240f0`：原位 + 角 −π/2 + 速 2.2）⇒ 原版那份 P 点**根本不存在**。
     * TH18 的 P 类 = type 1（小 P）/ 3（大 P）——旧实现只认 type 1、而且是「额外加撒」。
     * ⚠ 本判据现在只服务 `on_item_collect` 的**同帧竞态网**（主路 = 共用件
     *   `faith_convert_full_p_items()` 的池扫描，卡侧每帧调，见 `faith.h`）。语义 = 「该被替换」。*/
    CHECK(MOF_FULLP_ITEM_SMALL_P == FAITH_TYPE18_P_SMALL);
    CHECK(MOF_FULLP_ITEM_BIG_P == FAITH_TYPE18_P_BIG);
    CHECK(MOF_FULLP_ITEM_SMALL_P == 1);
    CHECK(MOF_FULLP_ITEM_BIG_P == 3);
    CHECK(mof_core_full_p_replace(MOF_FULLP_ITEM_SMALL_P, 500, 500) == 1); /* 满 P + 小 P ⇒ 替换 */
    CHECK(mof_core_full_p_replace(MOF_FULLP_ITEM_BIG_P, 500, 500) == 1);   /* ★ 满 P + 大 P ⇒ 也替换 */
    CHECK(mof_core_full_p_replace(MOF_FULLP_ITEM_SMALL_P, 499, 500) == 0); /* 不满 P ⇒ 不换 */
    CHECK(mof_core_full_p_replace(MOF_FULLP_ITEM_BIG_P, 0, 500) == 0);
    CHECK(mof_core_full_p_replace(2, 500, 500) == 0);                      /* 钱（type 2）不换 */
    CHECK(mof_core_full_p_replace(8, 500, 500) == 0);                      /* ❓ type 8（补满）本卡不接 */
    CHECK(mof_core_full_p_replace(9, 500, 500) == 0);                      /* 点道具不换 */
    CHECK(mof_core_full_p_replace(MOF_FULLP_ITEM_SMALL_P, 500, 0) == 0);   /* max_power 非正 ⇒ 防御性 0 */
    /* ★ 与共用件的类型判据**同源**（共用件扫池要用同一组号 ⇒ 两处不许漂）*/
    CHECK(faith_core_is_p_item_type(MOF_FULLP_ITEM_SMALL_P) == 1);
    CHECK(faith_core_is_p_item_type(MOF_FULLP_ITEM_BIG_P) == 1);
    CHECK(faith_core_is_p_item_type(8) == 0);

    /* ============ ★★ P 点火力 +0.05 档（110 / 111 共用；用户 2026-09-15）============
     * 一手（`local/_scratch/th18_full.asm`）：TH18 原生一颗小 P = `add_power(GI, 1)`
     * （`th18:0x44625f` type1 → `0x4462cc` → `0x446870` → `0x446a7b push 1` → `0x4573f0`），
     * 而一档 = `POWER_PER_LEVEL` = **100 单位**（`th18:0x45a98b` 写、`0x457465` 用它做除法）
     * ⇒ 原生 = **+0.01 档**。用户要的是「P 点**变成** +0.05」⇒ 目标 = 5 单位 ⇒ **补差 4**
     * （写成「再给 5」= 一颗共 +6 = 0.06 档，正是用户点名不许的「叠加成双倍」）。 */
    CHECK(MOF_P_ITEM_TYPE18 == 1);        /* TH18 type 1 = 小 P（大 P = type 3 不在本层）*/
    CHECK(MOF_P_POWER_UNITS == 5);        /* +0.05 档 × 100 单位/档 */
    CHECK(MOF_P_NATIVE_UNITS == 1);       /* 原生那一发（`th18:0x446a7b push 1`）*/
    CHECK(MOF_P_TOPUP_UNITS == 4);        /* 5 − 1 */

    /* ---- 谁落笔：id 最小者（110 < 111）---- */
    CHECK(mof_core_p_owner(1, 1) == 110);   /* 两张都在 ⇒ 110 落笔 */
    CHECK(mof_core_p_owner(1, 0) == 110);
    CHECK(mof_core_p_owner(0, 1) == 111);   /* 只有 111 ⇒ 111 落笔（**任一张在场即生效**）*/
    CHECK(mof_core_p_owner(0, 0) == 0);     /* 都不在 ⇒ 本层不参与（原生逐字节不变）*/

    /* ---- 单颗的算式（入参 = 原生 +1 **已经落地之后**读到的火力）---- */
    {   /* 1.00 档起吃一颗：原生 100 → 101、我们补 4 ⇒ **105 = 1.05**（= +0.05 档）*/
        mof_p_topup_t t = mof_core_p_topup(101, 400);
        CHECK(t.delta == 4);
        CHECK(101 + t.delta == 105);
        CHECK(101 + t.delta != 106);        /* 再给 5 就是 0.06 档（用户明令不许）*/
        CHECK(t.repop == 0);                /* 105 与起点 100 同档 ⇒ 不跨档、不用补收尾 */
    }
    {   /* 真正的升级：起点 195（1.95）⇒ 原生 196、补到 **200（2.00）** ⇒ 必须补一次收尾 */
        mof_p_topup_t t = mof_core_p_topup(196, 400);
        CHECK(t.delta == 4);
        CHECK(196 + t.delta == 200);
        CHECK(t.repop == 1);                /* 原生那一步（195→196）没报 ⇒ 由我们报 */
    }
    {   /* 原生那一步自己就跨了（起点 99 ⇒ 99→100）：它的收尾照旧，我们**不许**再报一次 */
        mof_p_topup_t t = mof_core_p_topup(100, 400);
        CHECK(t.delta == 4);
        CHECK(100 + t.delta == 104);
        CHECK(t.repop == 0);
    }
    {   /* 同一档里随便补（起点 109 ⇒ 110 → 114）：不跨档、不报 */
        mof_p_topup_t t = mof_core_p_topup(110, 400);
        CHECK(t.delta == 4);
        CHECK(t.repop == 0);
    }

    /* ---- ★ 满火力 **不越上限** ---- */
    {
        mof_p_topup_t t;
        t = mof_core_p_topup(400, 400);     /* 已经在满：原生也不给（`th18:0x446981`）⇒ 一个字节都不写 */
        CHECK(t.delta == 0);
        CHECK(t.repop == 0);
        t = mof_core_p_topup(399, 400);     /* 只差 1 单位 ⇒ 夹到上限（补 1，不是 4）*/
        CHECK(t.delta == 1);
        CHECK(399 + t.delta == 400);
        t = mof_core_p_topup(398, 400);
        CHECK(t.delta == 2);
        CHECK(398 + t.delta == 400);
        t = mof_core_p_topup(500, 400);     /* 已经在 max 之上（别人写坏了）⇒ 也不写 */
        CHECK(t.delta == 0);
        t = mof_core_p_topup(0, 0);         /* 引擎还没写 max ⇒ 防御性 0 */
        CHECK(t.delta == 0);
        CHECK(t.repop == 0);
        t = mof_core_p_topup(400, 0);
        CHECK(t.delta == 0);
    }

    /* ---- ★ 两张卡同时在场 **只算一次**（卡侧接线的可执行模型）---- */
    {
        int32_t both, only110, only111, none;
        int appliers = 0, repops = 0;
        int32_t p;
        int ok;

        both = sim_p_commit(101, 400, 1, 1, &appliers, &repops);   /* 两张都装 */
        CHECK(appliers == 1);                                      /* **只有一张真的落笔** */
        CHECK(both == 105);
        only110 = sim_p_commit(101, 400, 1, 0, 0, 0);
        only111 = sim_p_commit(101, 400, 0, 1, 0, 0);              /* 任一张在场即生效 */
        none    = sim_p_commit(101, 400, 0, 0, 0, 0);
        CHECK(only110 == 105);
        CHECK(only111 == 105);
        CHECK(both == only110 && both == only111);                 /* 「只算一次」的定义 */
        CHECK(none == 101);                                        /* 没有卡 ⇒ 只有原生那 +1 */
        CHECK(repops == 0);

        /* 整条火力轴：任何起点都不许出现「双份补差」、也不许越上限 */
        ok = 1;
        for (p = 100; p <= 400; ++p) {
            int32_t b = sim_p_commit(p, 400, 1, 1, 0, 0);
            int32_t o = sim_p_commit(p, 400, 0, 1, 0, 0);
            int32_t want = (p < 400) ? (p + MOF_P_NATIVE_UNITS) : p;   /* 原生那一步 */
            if (b != o) ok = 0;
            if (b > 400) ok = 0;
            if (b < want) ok = 0;                                      /* 只加不减 */
        }
        CHECK(ok);

        /* 整条火力轴：**最终值恰好 = min(起点 + 5, max)**（替换语义）+ 跨档收尾「一次不多不少」 */
        ok = 1;
        {
            int32_t before, n_cross = 0, n_report = 0;
            for (before = 100; before <= 400; ++before) {
                int32_t after_native = (before < 400) ? (before + MOF_P_NATIVE_UNITS) : before;
                int32_t want = before + MOF_P_POWER_UNITS;
                int32_t got = sim_p_commit(after_native, 400, 1, 1, 0, 0);
                int native_reported = (after_native > before) && (((after_native - 1) % 100) == 99);
                mof_p_topup_t t = mof_core_p_topup(after_native, 400);
                if (want > 400) want = 400;
                if (got != want) ok = 0;
                if ((before / 100) != (want / 100)) ++n_cross;          /* 档数真的变了 */
                if (native_reported || t.repop) ++n_report;             /* 谁来报这次收尾 */
            }
            CHECK(ok);
            CHECK(n_cross == n_report);     /* **一次跨档 = 一次收尾**（不许漏、也不许双报）*/
            CHECK(n_cross == 15);           /* 100..400 里恰好 3 个档 × 5 格 */
        }
    }

    /* ============ ★★ 2026-09-15h：爱丽丝的 P 点减半 —— **双卡同装也要生效** ============
     * 用户口径：爱丽丝的两件事（灵击减半 / P 点收益平均 +0.025 档）都算「爱丽丝生效」。
     * 修之前：P 点补值的落笔者恒为 110 ⇒ 111 卡侧那份减半被绕过（只有单装 111 才生效）。
     * 修之后：两张卡都只「问共享策略」⇒ 判据（111 在场 ∧ 机体 == 魔理沙(1)）在 110 那一侧同样成立。
     * 这一块钉五件事：
     *   ① **双卡同装 == 只装 111**（逐值相同）—— 这就是「修好了」的可证形式；
     *   ② **每颗道具策略只被问一次**（`takes == 1`）且相位计数与它同步（`alt.items == takes`）；
     *   ③ 10 颗的平均 = **2.5 单位**（= 0.025 档；`sum == 25`）；
     *   ④ 常态（非魔理沙 / 111 不在场）逐值不变（仍是 +0.05 档，且**相位不动**）；
     *   ⑤ 满火力那一颗**不吃相位格**（否则真正拿到火力的那些颗会被拉偏）。*/
    {
        faith_p_alt_t alt;
        int32_t p;
        int takes, appliers, i;

        /* --- ① 双卡同装（110 落笔）与只装 111：**逐值相同**，且都按爱丽丝的 2,3 走 --- */
        {
            faith_p_alt_t a1, a2;
            int32_t v1, v2;
            memset(&a1, 0, sizeof a1);
            memset(&a2, 0, sizeof a2);
            /* 第一颗：原生 +1 之后 power = 101 ⇒ 总 +2 ⇒ 102 */
            v1 = sim_p_commit_policy(101, 400, 1, 1, 1, &a1, 0, 0);   /* 双卡同装（110 落笔）*/
            v2 = sim_p_commit_policy(101, 400, 0, 1, 1, &a2, 0, 0);   /* 只装 111 */
            CHECK(v1 == 102 && v2 == 102);
            /* 第二颗：原生 +1 之后 = 103 ⇒ 总 +3 ⇒ 补 2 ⇒ 105（= 第一颗结束时的 102 + 3）*/
            v1 = sim_p_commit_policy(103, 400, 1, 1, 1, &a1, 0, 0);
            v2 = sim_p_commit_policy(103, 400, 0, 1, 1, &a2, 0, 0);
            CHECK(v1 == 105 && v2 == 105);
            CHECK(a1.items == 2 && a2.items == 2 && a1.lo == 1 && a1.hi == 1);
        }

        /* --- ② 每颗只问一次策略（相位每颗只推进一次）+ ③ 平均 2.5 --- */
        memset(&alt, 0, sizeof alt);
        takes = appliers = 0;
        p = 100;                                /* 第 1 颗**原生 +1 之前**的火力 */
        for (i = 0; i < 10; ++i) {
            p = sim_p_commit_policy(p + 1, 400, 1, 1, 1, &alt, &appliers, &takes);  /* 原生每颗 +1 */
        }
        CHECK(takes == 10);                     /* 10 颗 ⇒ **恰好 10 次**（不是 20 次）*/
        CHECK(alt.items == takes);              /* 相位计数与问的次数同步（唯一持有者）*/
        CHECK(appliers == 10);                  /* 落笔的也只有一张 */
        CHECK(p == 100 + 25);                   /* 10 颗总共只涨 25 单位（平均 2.5 = 0.025 档）*/
        CHECK(alt.lo == 5 && alt.hi == 5);

        /* --- ④ 常态：非魔理沙 / 111 不在场 ⇒ 逐值等于老模型（+0.05 档）且相位不动 --- */
        memset(&alt, 0, sizeof alt);
        CHECK(sim_p_commit_policy(101, 400, 1, 1, 0, &alt, 0, 0) == sim_p_commit(101, 400, 1, 1, 0, 0));
        CHECK(sim_p_commit_policy(101, 400, 0, 1, 0, &alt, 0, 0) == sim_p_commit(101, 400, 0, 1, 0, 0));
        CHECK(sim_p_commit_policy(101, 400, 1, 0, 1, &alt, 0, 0) == sim_p_commit(101, 400, 1, 0, 0, 0));
        CHECK(alt.items == 0 && alt.alt == 0);  /* 三颗都没推进相位 */
        /* 整条火力轴：常态两侧逐值相同（爱丽丝那条只在 need 时才改数）*/
        {
            int ok = 1;
            faith_p_alt_t ax;
            memset(&ax, 0, sizeof ax);
            for (i = 100; i <= 400; ++i) {
                int32_t a = sim_p_commit_policy(i, 400, 1, 1, 0, &ax, 0, 0);
                int32_t b = sim_p_commit(i, 400, 1, 1, 0, 0);
                if (a != b) ok = 0;
            }
            CHECK(ok);
        }

        /* --- ⑤ 满火力那一颗不吃相位格：序列仍从 LO 起 --- */
        memset(&alt, 0, sizeof alt);
        takes = 0;
        CHECK(sim_p_commit_policy(400, 400, 1, 1, 1, &alt, 0, &takes) == 400);   /* 满格：一个字节都不写 */
        CHECK(takes == 1);                       /* 策略**被问了**（落笔者照常问）… */
        CHECK(alt.items == 0 && alt.alt == 0);   /* …但**没吃相位格**（`usable = 0`）*/
        CHECK(sim_p_commit_policy(101, 400, 1, 1, 1, &alt, 0, 0) == 102);       /* 下一颗仍走 LO */
        CHECK(alt.lo == 1 && alt.hi == 0);
    }

    /* ============ 与 faith_core 的接缝：110 槽的信仰就是 PV 的基础 ============ */
    faith_slot_init_110(&slot);
    (void)faith_slot_add(&slot, FAITH_EV_BIG, 5000);
    (void)faith_slot_reduce(&slot, FAITH_REDUCE_BOMB);            /* 非决死灵击 −3000 */
    CHECK(faith_slot_total(&slot) == 52000);
    CHECK(mof_core_pv(1, 0, faith_slot_total(&slot), 0, 0) == 52000);

    /* ============ ★★ 2026-09-15c「灵击 → 决死」（决死 B 形态）============
     * 三条一起钉：
     *   ① **窗口判据** = `player_state == 4`（一手 `th18:0x45d4e4` —— 卡侧本轮穷举过全 exe 对
     *      `player+0x476ac` 的写点，只有它写 4）；
     *   ② **决死不扣 −3000 信仰**（一手：TH10 的 `Player::on_tick` 里 `call 0x412e70` 只有
     *      `th10:0x425a1f` 一处、在 state 1 支里；决死支通篇没有它）⇒ 同一次灵击、同样的弹数，
     *      决死形态的信仰**恒 ≥** 主动形态；
     *   ③ **消弹入不入账**由 `MOF_BOMB_DEATHBOMB_ACCOUNT` 决定 —— 这一条是 **❓ 自定**（它引的
     *      一手地址在复核里被推翻，见 `mof_core.h`）⇒ 只测「开关被尊重」，**不钉死开关的值**
     *      （翻那一行时本测试必须照旧通过）。*/
    {
        mof_bomb_form_t f;
        faith_slot_t    s_press, s_dbf;
        int32_t         bullets, ok, min_gap;

        /* ① 窗口判据 */
        CHECK(MOF_DEATHBOMB_STATE == 4);                              /* 一手写死的那个值 */
        CHECK(mof_core_deathbomb_window(MOF_DEATHBOMB_STATE) == 1);
        for (int32_t ps = -2; ps <= 10; ++ps)
            if (ps != MOF_DEATHBOMB_STATE) CHECK(mof_core_deathbomb_window(ps) == 0);

        /* ② 形态 → 信仰档 */
        mof_bomb_form_start(&f, MOF_BOMB_BY_PRESS);
        CHECK(mof_bomb_form_is_deathbomb(&f) == 0);
        CHECK(f.cancels == 0);
        CHECK(mof_bomb_form_faith_kind(&f) == FAITH_REDUCE_BOMB);
        mof_bomb_form_start(&f, MOF_BOMB_BY_HIT);
        CHECK(mof_bomb_form_is_deathbomb(&f) == 1);
        CHECK(f.cancels == 0);                                        /* 新一发 ⇒ 颗数清零 */
        CHECK(mof_bomb_form_faith_kind(&f) == FAITH_REDUCE_NONE);     /* ★ 决死不扣 −3000 */
        /* 形态字段只认「是不是决死」这一个二值（传别的值 ⇒ 当主动处理，不许漏成野值）*/
        mof_bomb_form_start(&f, -7);
        CHECK(mof_bomb_form_is_deathbomb(&f) == 0);
        CHECK(mof_bomb_form_faith_kind(&f) == FAITH_REDUCE_BOMB);

        /* 一整发：同样的弹数，两种形态各跑一次。
         * ⚠ 起点必须带一笔 Δ（这里 +5000）才看得见 −3000：`faith_slot_reduce()` 是**从 Δ 里扣**、
         *   Δ 的下限是 0（= 原作「50000 扣不穿」，`faith_slot_reduce()` 的注释）⇒ Δ = 0 时 −3000 扣不动。*/
        faith_slot_init_110(&s_press);
        faith_slot_init_110(&s_dbf);
        (void)faith_slot_add(&s_press, FAITH_EV_BIG, 5000);   /* Δ = 5000 ⇒ 55000 */
        (void)faith_slot_add(&s_dbf,   FAITH_EV_BIG, 5000);
        CHECK(sim_bomb_faith(&s_press, &f, MOF_BOMB_BY_PRESS, 100) == 55000 - 3000 + 100 * FAITH_MOF_WEI);
        {
            int32_t want = 55000 + (MOF_BOMB_DEATHBOMB_ACCOUNT ? 100 * FAITH_MOF_WEI : 0);
            CHECK(sim_bomb_faith(&s_dbf, &f, MOF_BOMB_BY_HIT, 100) == want);
            CHECK(f.cancels == 100);                          /* 颗数**无论入不入账**都记 */
            CHECK(mof_bomb_form_cancel_accounts(&f) == (MOF_BOMB_DEATHBOMB_ACCOUNT ? 1 : 0));
        }

        /* ③ 全弹数轴上的不变式：gap = 决死 − 主动 = 省下的 −3000 − 那一档记账之差（逐弹数精确）*/
        ok = 1; min_gap = 1 << 30;
        for (bullets = 0; bullets <= 300; bullets += 3) {
            faith_slot_t a, b;
            int32_t pa, pb, gap, want_gap;
            faith_slot_init_110(&a);
            faith_slot_init_110(&b);
            (void)faith_slot_add(&a, FAITH_EV_BIG, 5000);
            (void)faith_slot_add(&b, FAITH_EV_BIG, 5000);
            pa = sim_bomb_faith(&a, &f, MOF_BOMB_BY_PRESS, bullets);
            pb = sim_bomb_faith(&b, &f, MOF_BOMB_BY_HIT, bullets);
            if (pb < pa) ok = 0;                              /* ★ 决死不许比主动更亏 */
            gap = pb - pa;
            want_gap = 3000 - (MOF_BOMB_DEATHBOMB_ACCOUNT ? 0 : bullets * FAITH_MOF_WEI);
            if (gap != want_gap) ok = 0;
            if (gap < min_gap) min_gap = gap;
        }
        CHECK(ok);
        CHECK(min_gap == 3000 - (MOF_BOMB_DEATHBOMB_ACCOUNT ? 0 : 300 * FAITH_MOF_WEI));

        /* 入账那一支的颗数是**累加**（一发消 60 + 40 颗 = 记 100 颗）*/
        faith_slot_init_110(&s_press);
        mof_bomb_form_start(&f, MOF_BOMB_BY_PRESS);
        mof_bomb_form_cancel_note(&f, 60);
        mof_bomb_form_cancel_note(&f, 40);
        CHECK(f.cancels == 100);
        mof_bomb_form_cancel_note(&f, -5);                    /* 负数不动 */
        mof_bomb_form_cancel_note(0, 10);                     /* NULL 安全 */
        CHECK(f.cancels == 100);

        /* NULL 安全（卡侧可能在没有卡状态时走诊断支）*/
        mof_bomb_form_start(0, MOF_BOMB_BY_HIT);
        CHECK(mof_bomb_form_is_deathbomb(0) == 0);
        CHECK(mof_bomb_form_faith_kind(0) == FAITH_REDUCE_BOMB);
        CHECK(mof_bomb_form_cancel_accounts(0) == 1);
    }

    printf("test_mof_core: %d passed, %d failed\n", s_pass, s_fail);
    return s_fail ? 1 : 0;
}
