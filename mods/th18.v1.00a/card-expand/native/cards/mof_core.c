/* mof_core.c —— 风神录（110）的纯逻辑实现。接口与依据见 `mof_core.h`。
 * 主机单测：`tests/test_mof_core.c`。
 *
 * ⚠ 本文件包含 `../sdk/public.h` 只为拿 `CE_EDEATH_*`（死亡事件的 `kind` / `flags` 常量）。
 *   那个头**是纯 C**（只有 `<stdint.h>` + 类型 + 宏，**不含任何 TH18 地址或 ABI 包装**）
 *   ⇒ 不破坏「core 层无引擎地址」的规矩，也不需要 `-I.`（相对路径对
 *   `cc tests/test_mof_core.c cards/mof_core.c` 同样成立）。**别再自己镜像一份常量**（会漂）。*/
#include <stdint.h>
#include "../sdk/public.h"
#include "mof_core.h"
#include "mof_big_faith_table.h"   /* ★ 2026-09-15：大信仰点宿主表（TH18 侧的「绿妖精」）*/

/* ---- 大信仰点的难度表（§7.1 / th10 一手 `0x41b57f`：E/N 5000 · H 8000 · L/EX 10000）---- */
int32_t mof_core_big_value(int difficulty) { return faith_big_mof(difficulty); }

/* ---- 大信仰点判据（§7.9：不按族挑，按「这敌实际掉什么」）---- */
int mof_core_death_qualifies(uint32_t kind, uint32_t flags)
{
    /* ★ killAll 一族**不算击破**（review §4.2 的 ⚠ killAll 坑）：那条路在调 `0x430240` 之前
     * 已经 `memset(enemy+0x6240,0,0xc0)` 并清主掉落 ⇒ 清场**杀不掉道具**，却仍会被死亡断点
     * 计到「击破」。过场全清一次就把计数刷满 ⇒ 必须排除。*/
    if (kind == CE_EDEATH_KILL_ALL) return 0;
    /* 只认真敌人（`(flags_lo & 0x31) == 0`；**置位 = 被排除在真敌人之外**，极性见 review §11 #42）*/
    if (!(flags & CE_EDEATH_F_REAL)) return 0;
    /* 没有掉落表就谈不上「掉钱」⇒ 直接出局（`CE_EDEATH_F_HAS_DROP` ⇔ `drop_main != 0`）*/
    if (!(flags & CE_EDEATH_F_HAS_DROP)) return 0;
    return 1;
}

int mof_core_wants_big_main(int32_t drop_main, int is_boss)
{
    if (is_boss) return 0;                       /* boss 不掉（§7.9）*/
    return (drop_main == MOF_DROP_TYPE_MONEY) ? 1 : 0;
}

int mof_core_wants_big_dual(int32_t drop_main, int32_t money_count, int32_t p_count, int is_boss)
{
    if (is_boss) return 0;
    if (drop_main == MOF_DROP_TYPE_MONEY) return 0;   /* 走判据①，别重复计 */
    return (money_count > 0 && p_count > 0) ? 1 : 0;
}

int mof_core_wants_big(int32_t drop_main, int32_t money_count, int32_t p_count, int is_boss,
                       int difficulty, int32_t *value_out)
{
    int want = mof_core_wants_big_main(drop_main, is_boss)
            || mof_core_wants_big_dual(drop_main, money_count, p_count, is_boss);

    if (value_out) *value_out = want ? mof_core_big_value(difficulty) : 0;
    return want;
}

int mof_core_wants_big_death(uint32_t kind, uint32_t flags, int32_t drop_main, int is_boss,
                             int difficulty, int32_t *value_out)
{
    int want;

    if (value_out) *value_out = 0;
    if (!mof_core_death_qualifies(kind, flags)) return 0;
    /* 死亡事件只带 `drop_main`，不带 ex_drop 计数 ⇒ 这里只能走判据①（`drop_main == 2`）。
     * 判据②（「同时掉钱与 P」）在 `on_enemy_drop_pre` 里用 `counts` 做（`mof.c` 的双路说明）。*/
    want = mof_core_wants_big_main(drop_main, is_boss);
    if (value_out && want) *value_out = mof_core_big_value(difficulty);
    return want;
}

/* ==================== ★★ 大信仰点判据换表（2026-09-15，用户裁决「走 1 = 判据收窄」）====================
 * 上面那四个 `mof_core_wants_big*` 是**旧口径**（「这只敌掉的是不是金钱」），已被用户 2026-09-15 的
 * 裁决取代：**「金钱和绿点总之别重合」** ⇒ 大信仰点只给「原作（TH10）真的在 ECL 掉落表里声明了
 * type 3 的那些敌人的对应物」。表与全套预研（逐关清单 / 计数 / 映射 / 不可信处）在
 * `mof_big_faith_table.h` 的头部注释里；这里只放可执行形式（纯函数 ⇒ 有主机单测）。
 *
 * 三个入口的分工：
 *   · `mof_core_death_is_real()` —— 死亡事件的**门**（去掉旧门里「必须有掉落表」那一条：宿主是
 *     按**外观**定的，TH18 里确有原生 item = 0 的宿主，例如 st04 的 `Cir02/Cir03`；
 *     留着旧门会让它们永远不撒点，与「绿妖精 = 宿主」的语义矛盾）。
 *   · `mof_core_big_faith_host()` —— **表本身**（`(anm 源, 主脚本)` → 是/否）。
 *   · `mof_core_wants_big_host()` —— 卡侧唯一入口：门 + 表 + 「非 boss」+ 难度值，一次做完。
 *
 * ⚠ **中绿点（满 P 转换）不走这条路**：那是 `mof_core_full_p_replace()` + 共用件的池扫描
 *   （`faith_convert_full_p_items()`），本表**一个字都不影响它**（单测钉住）。*/

int mof_core_death_is_real(uint32_t kind, uint32_t flags)
{
    /* 与 `mof_core_death_qualifies()` 的前两条逐字相同，只**不要求** `CE_EDEATH_F_HAS_DROP`
     * （理由见上面那段：宿主按外观定，原生掉 0 的绿妖精也算宿主）。*/
    if (kind == CE_EDEATH_KILL_ALL) return 0;
    if (!(flags & CE_EDEATH_F_REAL)) return 0;
    return 1;
}

int mof_core_big_faith_host(int32_t anm_src, int32_t anm_script)
{
    return mof_big_faith_host_lookup(anm_src, anm_script);
}

int mof_core_wants_big_host(uint32_t kind, uint32_t flags, int32_t anm_src, int32_t anm_script,
                           int is_boss, int difficulty, int32_t *value_out)
{
    int want;

    if (value_out) *value_out = 0;
    if (is_boss) return 0;                      /* boss 不掉（一手：TH10 全 exe 无 boss 声明 type 3）*/
    if (!mof_core_death_is_real(kind, flags)) return 0;
    want = mof_core_big_faith_host(anm_src, anm_script);
    if (value_out && want) *value_out = mof_core_big_value(difficulty);
    return want;
}

int mof_core_wants_big_green(uint32_t kind, uint32_t flags, int is_boss, int difficulty,
                             int32_t *value_out)
{
    /* 与 `mof_core_wants_big_host()` **只差最后一件事**：不查宿主表（那批妖精的键由卡侧的
     * 「变绿表」给出）。门与非 boss 逐条相同 —— 因为「变绿」只改**外观与掉落**，
     * 不改「什么算一次击破」这条纪律（KILL_ALL 全清不算、无敌/过场不算）。*/
    if (value_out) *value_out = 0;
    if (is_boss) return 0;
    if (!mof_core_death_is_real(kind, flags)) return 0;
    if (value_out) *value_out = mof_core_big_value(difficulty);
    return 1;
}

mof_pick_t mof_core_pick_p_item(int32_t drop_main, int32_t p_count, int32_t power,
                                int32_t max_power, int is_boss)
{
    /* ⚠ TH18 没有「信仰点」这个 item type（§11 #48）⇒ 这里只判「这颗是不是 P 点」，
     * 撒出来的**一律是我们自建 VM**，不去改原生 type。*/
    if (is_boss) return MOF_PICK_DROP;
    if (p_count <= 0 && drop_main != MOF_DROP_TYPE_P) return MOF_PICK_DROP;
    if (max_power <= 0 || power < max_power) return MOF_PICK_DROP;   /* 不满 P 不转化 */
    return MOF_PICK_ADD_SMALL;
}

/* ★ TH18 类型版的满 P 替换判据（TH10 的表；依据见 `mof_core.h` 的整块注释）。
 * ⚠ 分工：
 *   · **主路**（真正的「替换」）= 共用件 `faith_convert_full_p_items()`（`faith.c` 的池扫描，
 *     满 P 时把 P 类道具**原位删掉** + 原位撒 type 9 档的点）—— 卡侧每帧调，**不经过本函数**；
 *   · **本函数** = `on_item_collect` 那张**同帧竞态网**的判据（引擎的道具 tick 抢在我们扫到之前
 *     把 P 道具喂给了玩家 ⇒ 那颗已拿过原生满 P 身价分，仍要补一颗信仰点）。 */
int mof_core_full_p_replace(int32_t type18, int32_t power, int32_t max_power)
{
    if (max_power <= 0 || power < max_power) return 0;          /* 不满 P：TH10 的跳表根本不进 */
    /* TH10 表里命中「→ 小信仰点」的那两格（type 1 / 4）= TH18 的小 P / 大 P（type 1 / 3）。*/
    return faith_core_is_p_item_type(type18) ? 1 : 0;
}

/* ==================== ★★ P 点火力 +0.05 档（110 / 111 共用）====================
 * 定义、一手链（`th18:0x44625f` → `0x446870` → `0x4573f0`）、以及
 * 「为什么必须落在 `on_item_commit` 而不是 `on_item_collect`」的全套推导 —— 都在 `mof_core.h`
 * 的那一大段里。这里只放可执行形式（可主机单测），不重复推理。 */

int mof_core_p_owner(int have110, int have111)
{
    if (have110) return 110;          /* id 最小者落笔（与 `ce_optpose_*` / `ce_dmg_*` 同一条选举规则）*/
    if (have111) return 111;
    return 0;                         /* 都不在场：本层不参与 */
}

mof_p_topup_t mof_core_p_topup(int32_t power, int32_t max_power)
{
    /* ★ 2026-09-15h：本签名 = 下面「总量由共享策略给」那一支 + **常态**总量。
     * 旧签名的行为**逐字节不变**（`test_mof_core.c` 仍钉着它）；爱丽丝生效时走 `_units()` 那一支。*/
    return mof_core_p_topup_units(power, max_power, MOF_P_POWER_UNITS);
}

mof_p_topup_t mof_core_p_topup_units(int32_t power, int32_t max_power, int32_t total_units)
{
    mof_p_topup_t r;
    int32_t room, lv_before, lv_after;

    r.delta = 0;
    r.repop = 0;
    /* 满火力（或引擎还没写 max）：原生**不给**火力（`th18:0x446981 jl 0x446a7b` 不跳）⇒ 我们也不给。
     * ⇒「满火力时不越上限」在**两条**路上都成立：这里直接不给 + 下面 `room` 的夹取。 */
    if (max_power <= 0 || power >= max_power) return r;
    room = max_power - power;
    r.delta = total_units - MOF_P_NATIVE_UNITS;   /* 原生已经给了 1 单位 ⇒ 我们只补差 */
    if (r.delta < 0) r.delta = 0;                 /* 策略给的量 < 原生那一发 ⇒ 不往回扣（一个字节都不写）*/
    if (r.delta > room) r.delta = room;

    /* 跨档收尾：原生那一步只报「它自己加的那 1 单位」跨没跨档（`th18:0x4573f0` 尾两次
     * `idiv [edi+0x64]`）⇒ 它报的条件 = `(power − 1) % 100 == 99` ⟺ `power % 100 == 0`。
     * 我们这一补若跨了档、而它没报，调用方就得自己补一次 `ce_repopulate_options()`
     * （子机按新火力档重建 + `on_power_level_change` 广播），否则升级静默、子机档位对不上。 */
    lv_before = (power - MOF_P_NATIVE_UNITS) / 100;       /* 原生那一步**之前**的档数 */
    lv_after  = (power + r.delta) / 100;                  /* 本颗 P 点之后的档数 */
    if (lv_before != lv_after && (power % 100) != 0) r.repop = 1;
    return r;
}

/* ---- 蓝点分值（§2.4 / §7.6）---- */
int32_t mof_core_height_coeff(float y, float poc_y, float board_y)
{
    float span, t;

    if (y <= poc_y) return MOF_BLUE_COEF_TOP;         /* 回收线以上（吃黄）= 满额 */
    span = board_y - poc_y;
    if (span <= 0.0f) return MOF_BLUE_COEF_BOTTOM;
    t = (y - poc_y) / span;                           /* 0 = 线上，1 = 板底 */
    if (t > 1.0f) t = 1.0f;                           /* 板底以下夹到 BOTTOM（不外推）*/
    return (int32_t)((float)MOF_BLUE_COEF_TOP
                     + ((float)MOF_BLUE_COEF_BOTTOM - (float)MOF_BLUE_COEF_TOP) * t);
}

/* ★ 生产路径（`mof.c` 的 `mof_on_pickup()`）用的那一支：板底固定 `MOF_BOARD_Y`。
 * 为什么单独一层：把「板底常量 + 收集高度」这一个口径钉在一个**有单测**的函数里，
 * 卡侧只负责把「收集那一刻的高度」喂进来（旧实现把口径写在卡侧 ⇒ 主机编不出来、也测不到，
 * 于是二元 TOP/BOTTOM 一路留到现在，插值函数成了死代码）。*/
int32_t mof_core_pickup_coeff(float collect_y, float poc_y)
{
    return mof_core_height_coeff(collect_y, poc_y, MOF_BOARD_Y);
}

int32_t mof_core_blue_value(int32_t faith, int32_t coeff_permille)
{
    int64_t v;

    if (faith <= 0 || coeff_permille <= 0) return 0;
    v = ((int64_t)faith * (int64_t)coeff_permille) / (int64_t)MOF_BLUE_COEF_TOP;
    if (v > 0x7fffffff) v = 0x7fffffff;
    return (int32_t)v;
}

/* ---- 卡内奖残（§2.6）---- */
void mof_bonus_init(mof_bonus_t *b)
{
    if (!b) return;
    b->bonus = 0;
    b->awarded = 0;
}

void mof_bonus_add(mof_bonus_t *b, int64_t delta)
{
    if (!b || delta <= 0) return;
    b->bonus += delta;
    if (b->bonus > (int64_t)0x7fffffffffffffffLL / 2) b->bonus = (int64_t)0x7fffffffffffffffLL / 2;
}

int32_t mof_bonus_awarded(const mof_bonus_t *b) { return b ? b->awarded : 0; }

int32_t mof_bonus_threshold(int tier)
{
    switch (tier) {
    case 0:  return MOF_BONUS_M;
    case 1:  return MOF_BONUS_40M;
    case 2:  return MOF_BONUS_80M;
    case 3:  return MOF_BONUS_150M;
    default: return MOF_BONUS_150M;
    }
}

int mof_bonus_take(mof_bonus_t *b)
{
    int n = 0;

    if (!b) return 0;
    while (b->awarded < MOF_BONUS_MAX && b->bonus >= mof_bonus_threshold(b->awarded)) {
        ++b->awarded;
        ++n;
    }
    return n;
}

/* ---- 扣火力（§5.5.1 三个坑 + §3.2.1 的 1.05 下限）---- */
int mof_core_power_can(int32_t power)
{
    return power >= MOF_BOMB_POWER_MIN * FAITH_POWER_PER_LEVEL;
}

mof_power_t mof_core_power_spend(int32_t power)
{
    mof_power_t r;
    int32_t floor_p = MOF_BOMB_POWER_FLOOR;          /* 1.05 档（定点 105）*/
    int32_t min_p = MOF_BOMB_POWER_MIN * FAITH_POWER_PER_LEVEL;   /* 2.00 档 = 200 */

    r.ok = 0;
    r.new_power = power;
    if (power < min_p) return r;                     /* 门槛：TH18 ≥ 2.00 档 */
    r.ok = 1;
    r.new_power = power - MOF_BOMB_POWER_COST;       /* 每次扣 1 档 */
    if (r.new_power < floor_p) r.new_power = floor_p; /* ★ 下限 1.05（用户「サービス」）*/
    return r;
}

/* ============================ ★★ 110 的逐帧信仰衰减（2026-09-15 恢复）============================
 * 接口、一手地址链与「门③怎么取」的 🟡 全在 `mof_core.h` 的那一大段里；这里是实现。
 *
 * 实现口径（逐条对一手）：
 *   · 门 = **三个都满足**（一手 `th10:0x418740`–`th10:0x41875d` 全是 `jne/jl` 跳到 skip）；
 *   · 门不成立 ⇒ **整段跳过** ⇒ 连击槽也不 −1（那是同一个函数、同一组门之后的动作）；
 *   · 槽 > 0 ⇒ 只 `−1`，**不扣信仰**（一手 `th10:0x418a06 jle` / `th10:0x418a10`）；
 *   · 槽 == 0 ⇒ `delta -= frames × 18000`，夹 `[0, ∞)`（一手 `th10:0x418a26` + `th10:0x418a3c`）。
 *
 * ⚠ 一手的「槽 > 0 就 −1，否则扣信仰」是**二选一**（`jle` 分叉），不是「先 −1 再判」——
 *   所以「槽刚归零的那一帧」**不扣信仰**，下一帧才扣。本实现照抄这个分叉（`else`）。
 * ⚠ 速率是常量（下限 18 恒定）⇒ 批量 `frames` 与逐帧调用**数值等价**，没有累加顺序问题。*/
/* 本帧的速率档（用户口径 2026-09-15；🟡 一手证据只支持「无敌人」那一档，见 `mof_core.h` 的长注）。
 * 非 0/1 的入参按「有敌人」（慢档）处理 —— 保守：判不出来时宁可少扣信仰。*/
int32_t mof_faith_rate_fixed(int killed_since_last)
{
    /* ★★ 定案（2026-09-18 逐条复核，用户追问「一手是不是只有有敌人分支」逼出来的）：
     *   判据 = **这一帧有没有击破**，不是「有没有敌人」。
     *   一手：`0x40e26b`（敌机死亡路径，紧跟 `or [enemy+0x1444],0x2000`）把速率字段 `0x474c50` 写 **1**；
     *   衰减帧 `0x418a23` 读它、`0x418a26 sub ecx,edx` **用读到的原值**扣，之后 `0x418a30` 才抬到 18。
     *   ⇒ 有击破的帧扣 1 存储（−10 显示 = 慢档）；没击破的帧扣 18 存储（−180 显示 = 快档）。
     * 其它取值按「有击破」处理（保守：宁可少扣）。*/
    return killed_since_last ? MOF_FAITH_DRAIN_KILL_FIXED : MOF_FAITH_DRAIN_NO_BOSS_FIXED;
}

int32_t mof_core_faith_tick(faith_slot_t *s, const mof_faith_gate_t *gate, int frames)
{
    int32_t rate, loss;

    /* 一手那三条门是**守卫**（任一不成立连函数都不调）⇒ 缺门 = 缺参 = 冻结：与「有 Boss」同待遇。*/
    if (!s || !gate || frames <= 0) return 0;
    if (!gate->no_boss || !gate->no_dialogue || !gate->stage_frames_ok) return 0;

    if (s->combo > 0) {                     /* `th10:0x418a06 jle 0x418a17`：槽没走完 ⇒ 只 −1 */
        if (frames >= s->combo) s->combo = 0;   /* 批量补跑时最多减到 0（一手每帧只 −1，不会变负）*/
        else                    s->combo -= frames;
        return 0;
    }

    /* 槽空 ⇒ 衰减（一手 `th10:0x418a17`）：速率按档位取（`th10:0x418a2e`–`0x418a30` 那段）。
     * `delta` 是 ×100 定点 ⇒ 与档位常量同量纲，直接减。*/
    rate = mof_faith_rate_fixed(gate->killed_since_last);
    loss = rate * frames;
    if (loss > s->delta) loss = s->delta;   /* 硬钳在基础（一手存储 5000 = 我们的 `delta` 0）*/
    s->delta -= loss;
    return loss;
}

/* ---- 旧入口：2026-09-15 退役为薄包装（★ 门不完备，卡侧不许再用）----
 * 它原来**恒返回 0**（依据是被推翻的那条负结论），现在转调新入口：
 *   · `no_boss` = `!in_boss_fight`（调用方本来就传这个）；
 *   · 另两条门传 1（**最宽松**）—— 旧签名的入参里根本没有这两个量，无法凭空知道；
 *     速率取「无敌人」快档（旧签名里也没有「场上有没有敌人」）。
 *     ⇒ 走这条路的语义 = 「只按 boss 门 + 快档衰减」，**不等于一手**（缺「对话中」与「前 90 帧」两条门）。
 * ⚠ 保留它只为「不改既有被测试钉住的签名」这条工程规矩；`tests/test_mof_core.c` 里
 *   「恒 0」的旧断言已随本轮一起订正（见那个文件 §连击槽 的注释）。*/
int32_t mof_core_combo_tick(faith_slot_t *s, int in_boss_fight)
{
    mof_faith_gate_t gate;

    gate.no_boss         = !in_boss_fight;
    gate.no_dialogue     = 1;
    gate.stage_frames_ok = 1;
    gate.enemies_present = 0;
    gate.killed_since_last = 0;      /* ★ 旧入口没有「本帧有没有击破」这个输入 ⇒ 取快档（= 它历史上的语义）*/
    return mof_core_faith_tick(s, &gate, 1);
}

/* ---- 收集事件档 / 连击帧数（表与依据见 `mof_core.h`）---- */
int32_t mof_core_pickup_combo(int tier)
{
    /* 0 = `FAITH_TIER_BIG`（大信仰点 +120f，一手 `0x41b5c5 push 0x78`）；
     * 其余实体档 = `FAITH_TIER_SMALL`（110 只有小信仰点 +60f，一手 `0x41b61d push 0x3c`）。
     * ⚠ 数值**不取 `faith_core.h` 的 `FAITH_COMBO_*`**：那张表是共用默认（111 也读），
     *   而一手订正的只有 110 的大信仰点（130 → 120）。见头注。*/
    return (tier == 0) ? MOF_COMBO_BIG : MOF_COMBO_SMALL_FAITH;
}

faith_ev_t mof_core_pickup_ev(int tier)
{
    /* tier 用数值（0 = BIG / 1 = SMALL）—— 不 include `faith.h`（引擎侧头）。 */
    return (tier == 0) ? FAITH_EV_BIG : FAITH_EV_SMALL_FAITH;
}

/* ★★ 2026-09-16 补：**引擎道具**（TH18 item type）→ 连击充能帧数（表与一手出处见 `mof_core.h`）。
 * 这条通道原来一条都没接（症状见头注）⇒ 吃 P / 吃钱 / 吃大 P 都不涨连击。*/
int32_t mof_core_item_combo(int32_t type18)
{
    switch (type18) {
    case MOF_P_ITEM_TYPE18:     return MOF_COMBO_SMALL_P;   /* 1 = 小 P   ← th10 type 1/10 */
    case MOF_ITEM_TYPE18_MONEY: return MOF_COMBO_BLUE;      /* 2 = 金钱    ← th10 type 2（蓝点）*/
    case MOF_ITEM_TYPE18_BIG_P: return MOF_COMBO_BIG_P;     /* 3 = 大 P   ← th10 type 4/11 */
    default:                    return 0;                   /* 命碎 / 整命 / 符卡碎 / 补满 / 转换点 … */
    }
}

/* ---- 符卡「全避」监视器（接口与依据见 mof_core.h）---- */
int mof_core_spell_watch(mof_spell_watch_t *w, int spell_now, int timed_out_now)
{
    int fire = 0;

    if (!w) return 0;
    if (spell_now && !w->was) {                 /* 新符卡开始：两个标志都清 */
        w->bombed = 0;
        w->timed_out = 0;
    }
    /* 只在「上一帧已经在符卡里」时读超时位 ⇒ 新符卡第一帧的残留位不会被读进来 */
    if (w->was && timed_out_now) w->timed_out = 1;
    if (!spell_now && w->was) {                 /* 下降沿：结算这一张 */
        fire = (w->timed_out && !w->bombed);
        w->timed_out = 0;
    }
    w->was = spell_now;
    return fire;
}

/* ---- PV 公式（§7.4 / §7.8）----
 * ★★ 2026-09-15 一手订正（`local/_scratch/research-th11-faith-and-points.md` §2.3）：
 *   TH11 的擦弹项是**整数阶梯**，不是连续浮点 ——
 *       B = min(⌊graze / 100⌋, 899)          （单位 0.01 ⇒ 0.00..8.99；整 100 擦弹才 +0.01）
 *   本函数返回**千分点** ⇒ `B × 10`。
 *   ⚠ 旧实现是 `min(graze, 8990)`（= 1 graze 就算 1 千分点）⇒ **上限提前 10 倍到达**
 *     （8990 擦弹就封顶，原版要 89900），而且**零头被算进去了**（原版不足 100 的擦弹不计）。
 *   一手出处：`th11:0x41f4e0`（`⌊graze/100⌋` 的魔数 `0x51eb851f` + `sar edx,5`）、
 *   `th11:0x41f4e7: 3d 83 03 00 00  cmp eax,0x383` / `th11:0x41f4fe: b8 83 03 00 00  mov eax,0x383`（= 899）。
 *   算式全文与三个现算点见该文 §2.3；**总倍率上限 = 交信度项 1.00 + 擦弹项 8.99 = 9.99**。*/
int32_t mof_core_graze_mult(int32_t graze)
{
    int32_t steps;

    if (graze <= 0) return 0;
    steps = graze / 100;                                     /* ⌊graze/100⌋（C 的整数除法即向零取整，graze>0）*/
    if (steps > FAITH_GRAZE_STEP_MAX) steps = FAITH_GRAZE_STEP_MAX;   /* 899 */
    return steps * 10;                                       /* 0.01 单位 → 千分点 */
}

int32_t mof_core_pv(int have110, int have111, int32_t aggregate, int32_t graze_mult_permille,
                    int32_t signal_value)
{
    int64_t base, pv;
    int64_t mult = (int64_t)graze_mult_permille + (int64_t)signal_value;
    int64_t agg = aggregate;

    if (agg < 0) agg = 0;
    if (mult < 0) mult = 0;

    if (!have110 && !have111) {
        /* 两张都不在：返回 0 只是防御。
         * ⚠ 2026-09-13 计分层重做后本函数**不再参与落笔**（层按族声明 + 一次合成，
         * 见 `../../SCORE-LAYER-REWORK.md`）；这里保留为**回归断言用的旧口径实现**。*/
        return 0;
    }

    if (have110 && !have111) {
        /* 只装 110：`PV = 信仰_110`（**无乘数** —— MoF 里信仰就是最大得点，§7.4）。
         * 而 `aggregate` = 110 声明的信仰（基础 + Δ）⇒ 直接就是答案。*/
        pv = agg;
    } else if (!have110 && have111) {
        /* 只装 111：`PV = 信仰_111 × (擦弹倍率 + 信号强度)`。
         * ⚠ 111 单装时**落笔者是 111 自己**（id 111 > 110，但 110 不在场 ⇒ 聚合里只有 111），
         *   它用自己那份 `base(难度) + Δ_111` 作 `aggregate` 传进来（§7.2：基础按难度）。*/
        pv = agg * mult / 1000;
    } else {
        /* 两个都装：`PV = (基础均值 + Δ_110 + Δ_111) × (擦弹倍率 + 信号强度)`。
         * ⚠ **基础只取一份**（§7.2：绝不能再相加）；而 `aggregate` 是两张卡的**完整**声明之和
         *   = (基础_110 + Δ_110) + (基础_111 + Δ_111) ⇒ 要先把两份基础减掉，才是 Δ 之和。
         *   （2026-09-12 修：原实现写成 `(base + agg) × mult`，把基础记了三遍 ——
         *     NORMAL 下 (50000 + 100000) × 1.0 = 150000，而契约是 50000。111 侧的
         *     `signal_core_pv_111()` 一直是按契约实现的 ⇒ 两卡同装时两边结果不一致，这里对齐到契约。）
         * 🟡 `FAITH_BASE_111_*` 随难度变（EASY 25000 … LUNATIC 200000），本函数拿不到难度 ⇒ 按 NORMAL 反推；
         *    EASY/LUNATIC 下会差 `(基础_111_真实 − 50000)`。需要精确时给本函数加 difficulty 参数（改调用方）。*/
        int64_t delta;
        base  = ((int64_t)FAITH_BASE_110 + FAITH_BASE_111_NORMAL) / 2;
        delta = agg - (int64_t)FAITH_BASE_110 - (int64_t)FAITH_BASE_111_NORMAL;
        if (delta < 0) delta = 0;
        pv = (base + delta) * mult / 1000;
    }
    if (pv > 0x7fffffff) pv = 0x7fffffff;
    return (int32_t)pv;
}

/* ============================ 灵击：半径曲线 / 中心上移 / 伤害表 ============================
 * 依据与推演全文见 `mof_core.h` 的「灵击的收尾」块与 `engine/player/th10/01-bomb-full-chain.md`。
 * 三个函数都**只吃整数**（core 层无浮点 ⇒ 与 `make dllx87` 的「零 x87 指令」不变式无关地安全）。*/

/* 半径曲线：帧 0 = 16.00（`scale(16,16)`），120 帧内线性长到 `target_centi`，其后保持。
 * 一手：`th10:0x405813`（`bomb+0x3c` 每帧被 anm VM 的 `scale.y` 覆盖）+ `bullet.anm` script400/407/441
 * 的原始字节（`scaleTime(120, 6, 16.0 → target)`）。
 * ⚠ 曲线**只增不减**（脚本里 120 帧之后再没有 scale 指令）⇒ 这里也不做任何回退。*/
/* ★★★ 2026-09-16o **订正成 TH10 的缓动**（用户 2026-09-16 逐字：「**你确定你的长大和上移的函数和 th10 一致？**」）
 *
 * 一手（两条）：
 *   ① **TH10 那支脚本自己写的就是缓动**：`bullet.anm` **script400**（灵梦）=
 *      `scale(16.0f, 16.0f)` + **`scaleTime(120, 6, 16.0f, 160.0f)`** —— 第 2 个参数 **6 是缓动编号**，
 *      不是 0（线性）。旁证：同族的三支柱环 script394/395/396 分别用 **mode 5 / 6 / 4**
 *      （`scaleTime(60, 5, …)` / `(60, 6, …)` / `(60, 4, …)`）—— 若 mode 无意义，ZUN 不会给三支
 *      视觉相近的柱子写三个不同的号。
 *   ② **缓动表**（本仓已有一手，`engine/anm/th16/07-general.md` §4：`interp_common_methods`
 *      `0x4033f0` + 31 项跳表 `0x403a10` 逐条解码）：
 *        `mode 0 = u`（线性）、**`mode 6 = 1−(1−u)⁴`（ease-out 四次）**、`mode 4 = 1−(1−u)²`、`mode 5 = 1−(1−u)³`。
 *      ⇒ TH10 的灵击半径 = `16 + (目标−16) × (1 − (1 − t/120)⁴)`：**先窜出去、再慢慢咬住目标**。
 *      ⚠ 这条缓动表的**版本可信度**：表本身是 ✅ TH16 一手；「TH10 的 mode 6 同义」是 🟡 推断
 *        （v4 的跳表没逐条解），依据 = 上面 ① 的三支柱环用法 + 三个版本（TH11/TH17/TH18）脚本里
 *        mode 号与视觉意图一致 ⇒ 按同义落地，登记 `TUNING.md`。
 *   ⚠ 之前的实现在这里是**线性插值**（`(target−16) × t/120`），与一手不符 ⇒ 已换成下面的整数式。
 *     整数式与浮点同值到 1/100 px：`p = 120 − t`，`a = p²/120`，`b = a²/120`，`r = target − (target−16)×b/120`。*/
int32_t mof_core_bomb_radius_centi(int32_t frame, int32_t target_centi)
{
    int32_t p, a, b;

    if (frame <= 0) return MOF_BOMB_R_START_CENTI;
    if (target_centi <= MOF_BOMB_R_START_CENTI) return target_centi;   /* 非法/退化目标：直接取它 */
    if (frame >= MOF_BOMB_R_GROW_FRAMES) return target_centi;          /* 帧 ≥ 120：保持目标 */
    p = MOF_BOMB_R_GROW_FRAMES - frame;                                /* = (1 − u) × 120 */
    a = p * p / MOF_BOMB_R_GROW_FRAMES;                                /* ≈ (1−u)² × 120 */
    b = a * a / MOF_BOMB_R_GROW_FRAMES;                                /* ≈ (1−u)⁴ × 120 */
    return target_centi - (target_centi - MOF_BOMB_R_START_CENTI) * b / MOF_BOMB_R_GROW_FRAMES;
}

int32_t mof_core_bomb_rise_milli(int spell_ish, int reimu)
{
    if (spell_ish) return MOF_BOMB_RISE_SPELL_MILLI;      /* 0.5：`th10:0x4057b6`（符卡中那支）*/
    return reimu ? MOF_BOMB_RISE_REIMU_MILLI              /* 1.0：`th10:0x4057cd`（CHARACTER == 0）*/
                 : MOF_BOMB_RISE_MARISA_MILLI;            /* 1.3：`th10:0x4057d5`（CHARACTER != 0）*/
}

/* 每敌每帧的伤害（已过缩放链）。逐条对位 `th10:0x4059f0` 的返回表 + `th10:0x40e141`–`0x40e19e`：
 *     raw = 表(...)             // 圈内/圈外 × 符卡/无符卡 × 有 boss/无 boss × 灵梦/魔理沙
 *     dmg = raw / 5             // player_state ∈ {0,2}（`th10:0x40e157`；游玩中恒 0）
 *     if (dmg == 0) return 0    // `th10:0x40e169 je 0x40e26b`：0 伤害整笔跳过（**符卡前 60 帧的 3 就是这里消失的**）
 *     if (spell_active) { dmg /= 5; if (dmg <= 0) dmg = 1; }   // `th10:0x40e188`–`0x40e19e`（下限 1）
 * ⚠ TH10 的第二道 `/5` 条件是「**该敌是 boss** 且符卡中」（`th10:0x40e184 test ah,ah`）——TH18 的
 *   矩形伤害源**不区分敌**（谁在框里谁吃同一个数）⇒ 我们退化成「符卡中就对所有目标都 /5」，
 *   差异登记在 `TUNING.md` §2.5（符卡里通常只有 boss 与其召唤物在场，实跑看手感）。*/
int32_t mof_core_bomb_damage(int in_circle, int boss_alive, int spell_active, int big_bomb, int reimu)
{
    int32_t raw, dmg;

    if (in_circle) {
        if (spell_active) raw = big_bomb ? MOF_BOMB_DMG_IN_SPELL_BIG : MOF_BOMB_DMG_IN_SPELL_NOBIG;
        else              raw = boss_alive ? MOF_BOMB_DMG_IN_BOSS : MOF_BOMB_DMG_IN_PLAIN;
    } else {
        if (spell_active) {
            /* 圈外 + 符卡中：只有 `bomb+0x44==1`（大灵击）才有值（`th10:0x405a76`）*/
            raw = big_bomb ? (reimu ? MOF_BOMB_DMG_OUT_SPELL_BIG_REIMU
                                    : MOF_BOMB_DMG_OUT_SPELL_BIG_MARISA)
                           : MOF_BOMB_DMG_NONE;
        } else {
            /* 圈外 + 无符卡：**只有场上有 boss 才有值**（`th10:0x405a95` 的 `bosses[0] != 0` 门）*/
            raw = boss_alive ? (reimu ? MOF_BOMB_DMG_OUT_BOSS_REIMU : MOF_BOMB_DMG_OUT_BOSS_MARISA)
                             : MOF_BOMB_DMG_NONE;
        }
    }
    if (raw <= 0) return 0;
    dmg = raw / MOF_BOMB_DMG_DIV;                 /* /5（魔数 0x66666667 + sar 1 的整数语义）*/
    if (dmg <= 0) return 0;                       /* ★ 归零就整笔跳过（一手 `0x40e169`）*/
    if (spell_active) {                           /* boss + 符卡：再 /5、下限 1（一手 `0x40e188`–`0x40e19e`）*/
        dmg /= MOF_BOMB_DMG_DIV;
        if (dmg <= 0) dmg = 1;
    }
    return dmg;
}

/* ============ ★ 2026-09-15「灵击 → 决死」（决死 B 形态）—— 依据全在 `mof_core.h` 那段注释 ============
 * 这里只有四条判据：**没有浮点、没有引擎地址**（`th18:0x45d4e4` 那个「4」由卡侧读进来，
 * core 只认「窗口 = state 4」这件事）⇒ 卡侧只剩「检测窗口 → 传参」+「照答案落笔」。*/

int32_t mof_core_deathbomb_window(int32_t player_state)
{
    /* 一手 `th18:0x45d4e4`：全 exe 只有这一处把 `player+0x476ac` 写成 4（本轮穷举复核）。*/
    return (player_state == MOF_DEATHBOMB_STATE) ? 1 : 0;
}

void mof_bomb_form_start(mof_bomb_form_t *f, int32_t by_hit)
{
    if (!f) return;
    f->by_hit  = (by_hit == MOF_BOMB_BY_HIT) ? MOF_BOMB_BY_HIT : MOF_BOMB_BY_PRESS;
    f->cancels = 0;
}

void mof_bomb_form_cancel_note(mof_bomb_form_t *f, int32_t bullets)
{
    if (!f || bullets <= 0) return;
    f->cancels += bullets;
}

int32_t mof_bomb_form_faith_kind(const mof_bomb_form_t *f)
{
    /* 主动灵击 = −3000（一手 `th10:0x425a15`）；**决死那一支没有这句**（一手：整个
     * `Player::on_tick` 里 `call 0x412e70` 只有 `0x425a1f` 一处，且它在 state 1 支里）⇒ 决死扣 0。*/
    if (f && f->by_hit == MOF_BOMB_BY_HIT) return FAITH_REDUCE_NONE;
    return FAITH_REDUCE_BOMB;
}

int32_t mof_bomb_form_cancel_accounts(const mof_bomb_form_t *f)
{
    /* 🟡 开关语义（含「引文被推翻」的全过程）见 `mof_core.h`：0 = 决死形态不记「每颗 +10」。*/
    if (f && f->by_hit == MOF_BOMB_BY_HIT) return MOF_BOMB_DEATHBOMB_ACCOUNT ? 1 : 0;
    return 1;
}

int32_t mof_bomb_form_is_deathbomb(const mof_bomb_form_t *f)
{
    return (f && f->by_hit == MOF_BOMB_BY_HIT) ? 1 : 0;
}
