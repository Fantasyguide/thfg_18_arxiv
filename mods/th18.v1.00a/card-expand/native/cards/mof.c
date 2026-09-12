/* 风神录（TH10，card-expand id 110）—— **装备卡**：火力经济 + 信仰 + 连击条 + 灵击。
 *
 * 一手/定案依据（**先读这两篇，本文的注释只指路、不重复推理**）：
 *   · `docs/superpowers/specs/2026-09-10-mof-faith-design.md`（110 的设计原始稿）
 *   · `docs/superpowers/specs/2026-09-11-system-card-spec-review.md` §3.2 / §3.2.1 / §4.6(R9) /
 *     §5.5 + §5.5.1（**三处事实订正 + 扣火力三个坑**）/ §7 全章（绿点 = 信仰，110/111 共用）/
 *     §7.9（大信仰点判据）/ §11 的 #47a #48 / §12 —— **订正覆盖 spec 的旧说法**
 *   · `mods/th18.v1.00a/card-expand/EVENTS-CONTRACT.md` §3（计分层：**声明 → 聚合 → id 最小者落笔**）
 *   · ★ **一手（th10 v1.00a 逐条指令）**：`local/_scratch/research-th10-faith-and-graze.md`
 *     —— 2026-09-12 的三条订正都出自它（§3.2/§3.4 无逐帧信仰衰减、§3.1 连击表的大信仰点 = +120f、
 *     §1.1 TH10 无擦弹倍率、§4.1 灵击门槛/消耗的原版值）。**它覆盖 spec 里被证伪的说法。**
 *
 * ## 分工（这张卡只是「一具身体」，逻辑都在共用件里）
 *
 *   | 件 | 在哪 | 管什么 |
 *   | --- | --- | --- |
 *   | 绿点 / 信仰**纯逻辑** | `faith_core.c` | 按槽加值、上限/下限夹取、连击、信号、擦弹倍率、单位换算 |
 *   | 绿点**实体池**（110/111 共用） | `faith.c` | 撒点、飞散/飘落/吸附、收集判定、消弹入账回调 |
 *   | 110 的**纯逻辑** | `mof_core.c` | 大信仰点判据、蓝点分值、卡内奖残、扣火力、PV 公式 |
 *   | **本文件** | — | 读引擎、调上面三层、`ce_score_declare` |
 *
 * ## 本卡在计分层里的角色（EVENTS-CONTRACT §3）
 *
 * `on_item_score` 的落笔者 = 声明者里 **id 最小**的那张。110 是 108–113 里最小的一批之一
 * ⇒ **110 在场时由 110 落笔**（111 只声明、不落笔；它的 `Δ_111` 与**信号强度**通过聚合值进公式）。
 * 落笔时读**两个聚合值**（`ce_score_aggregate()` 被乘数 + `ce_score_mul_aggregate()` 乘数附加项，
 * 后者是 111 声明进来的信号强度千分点；擦弹倍率是 110 自算的，**不走那条通道**，见 `on_item_score`）：
 *     `PV = (基础均值 + Δ_110 + Δ_111) × (擦弹倍率 + 信号强度)`（两张都在场）
 *     ★ 2026-09-12 一手订正（任务 ③）：这一支里的「擦弹倍率」是 **th11（SA）的机制，TH10 没有**
 *       （`local/_scratch/research-th10-faith-and-graze.md` §1.1）⇒ 只有两卡同装时才该出现。
 *     `PV = 信仰_110`（只有 110，**无乘数**；一手 th10 的蓝点线上式也是「= 信仰」）
 * 然后写 `*value = PV × 10` ⇒ 引擎随后 `SCORE += value / 10` ⇒ **精确得到 PV 分、天然不双计分**（§7.8）。
 * ⚠ 落笔者**读的是聚合值，不读别的卡的状态** —— 这是契约里的硬要求。
 *
 * ## 明确不做（spec §6）
 *
 * 不写 `MONEY` / 不写 max point 全局 / 不做自机 spell card 系统 / 不做续关改动 / 不做原版机体差异。
 *
 * ## 长持续效果的自查（`METHOD.md` 铁律二）
 *
 * 本卡跨帧留下的东西：① 绿点 VM 池（共用件，静态区）；② HUD 的三个文字。
 * **`on_delete` 与 `on_run_reset` 都走同一个 `mof_clear()`**：收池 + 收 HUD + 从计分层撤销声明
 * （`ce_score_declare(id, 0)`）；删卡发生在**商店**里、那一刻 AbilityManager 冻住
 * ⇒ 只能靠 `on_delete`（虚表 `+0x50`），不能等下一帧。
 */
#include <stddef.h>          /* sdk.h 的 ce_item_collect_all 用了 size_t（内置 Linux 头不带）*/
#include <string.h>
#include <stdio.h>
#include "sdk.h"
#include "faith.h"
#include "mof_core.h"

/* ---- 本卡常量 ---- */
#define MOF_ID_NUM            110      /* ⚠ `CE_CARD` 会对 ID 做 ## 拼接 ⇒ 这里必须给**字面量**，
                                         * 不能把 NUM 再套一层宏（`CE_CARD(MOF_ID_NUM, …)` 会拼出
                                         * `ce_hooks_MOF_ID_NUM`）。所以下面 CE_CARD 里写 110。*/
/* engine.h 给了 `CE_CURRENT_POWER()` 但**没有** max power 的读取宏 ⇒ 就地补一个（只读）。
 * `CE_ADDR_MAX_POWER` = `0x4ccd3c` = `GlobalsInner+0x60`（engine.h 一手）。*/
#define CE_MAX_POWER_VAL()     (*(int32_t *)CE_ADDR_MAX_POWER)
#define MOF_GRAZE_ADDR        0x4ccd0c   /* 原生擦弹计数（GLOBALS.inner+0x30；与 lolk/season 同址）*/
#define MOF_ADDR_DIFFICULTY   0x4ccd00   /* 难度（0 EASY / 1 NORMAL / 2 HARD / 3 LUNATIC / **4 EXTRA**；
                                          * 一手：`collect_money_item` 拿它当 switch 选择子，`cmp eax,4` +
                                          * 5 项跳转表 0x446d40；th06rank.c 同址，**只读**）*/
#define MOF_LOG_EVERY         600        /* 每 600 帧一条汇总（实跑对账）*/
#define MOF_HUD_FAITH_X       52.0f      /* HUD：左下角（HUD 字面量空间 640×480）*/
#define MOF_HUD_FAITH_Y       366.0f
#define MOF_HUD_COMBO_Y       378.0f
#define MOF_HUD_COLOR_FAITH   0xffffffffu /* 白字 + 黑描边（`ce_hud_text_ex` 默认描边）*/
#define MOF_HUD_COLOR_COMBO   0xff60ff60u /* ❓ 自定：连击条数值用亮绿（原版 MoF 是同色数字）*/

typedef struct {
    faith_slot_t slot;              /* Δ_110 / 连击槽（130f）/ 基础 50000（共用件）*/
    mof_bonus_t  bonus;             /* 卡内奖残计数（§2.6）*/
    int32_t      last_graze;        /* 上一帧原生擦弹计数；-1 = 未初始化 */
    int32_t      graze_mult;        /* 擦弹倍率（千分点定点）*/
    int32_t      last_lives;        /* 上一帧残机（miss 检测：命数减少）*/
    mof_spell_watch_t spell_watch;  /* 符卡「全避」监视器（状态机在 mof_core.c 的 mof_core_spell_watch()）*/
    int32_t      spell_avoided;     /* 诊断：全避次数 */
    int32_t      frame;
    /* ★ 2026-09-12 审计（本次，任务 1 的收尾）：诊断口径拆成「**收到** / **撒出**」两分法 ——
     *   与 `cards/signal.c:125-128`（111）同款。起因：原来 `stat_big` / `stat_small` **一个字段担两件事**
     *   （撒点处 ++ 一次、`mof_on_pickup` 里又 ++ 一次）⇒ 一颗点记两次，实跑对账时数字永远是两倍。
     *   拆开之后「收到」只由 `mof_on_pickup` 记、「撒出」只由 `faith_spawn` 成功那一支记。*/
    int32_t      stat_big;          /* 诊断：**收到**的大信仰点颗数（`mof_on_pickup`，tier == BIG）*/
    int32_t      stat_small;        /* 诊断：**收到**的小信仰点颗数（`mof_on_pickup`，tier != BIG）*/
    int32_t      stat_big_spawn;    /* 诊断：**撒出**的大信仰点颗数（判据① death + 判据② drop_pre）*/
    int32_t      stat_small_spawn;  /* 诊断：**撒出**的小信仰点颗数（满 P 吃 P 点，**只此一路**）*/
    int32_t      stat_spawn_failed; /* 诊断：共用池满 ⇒ `faith_spawn()` 返回 0 的次数（大小点合计）*/
    int32_t      stat_cancel;       /* 诊断：消弹总数 */
    int32_t      stat_miss;         /* 诊断：miss 次数 */
    int32_t      stat_bomb;         /* 诊断：灵击次数 */
    int32_t      log_frame;
} mof_card_t;

_Static_assert(sizeof(mof_card_t) <= 496, "ce_state 上限 496 字节（超了会静默返回 NULL）");

/* HUD 的文字 VM（DLL 静态区；本卡同时只会有一张在场 —— 与 season.c 的 s_hud_digit 同口径）*/
static ce_hud_vm_text_t s_faith_txt;
static ce_hud_vm_text_t s_combo_txt;

static mof_card_t *st(ce_card_t *c) { return ce_state(c, mof_card_t); }

static int mof_difficulty(void) { return *(volatile int32_t *)MOF_ADDR_DIFFICULTY; }
static int32_t mof_graze(void)  { return *(volatile int32_t *)MOF_GRAZE_ADDR; }

/* 这只敌人是不是 boss / 道中 boss —— 照引擎自己的判据：EnemyManager 的 boss id 表
 * （`get_boss_enemy_full` `0x4237f0` 就是按它走链表；先例 season.c 的 `enemy_is_boss`）。
 * ⚠ §7.9 要求「boss 不掉大信仰点」，但**boss 判据本身还没定**（EDEATH flags 里没有 boss 位）
 * ⇒ 先用这条已实跑过的判据，登记为待实跑（报告「未解」第 1 条）。*/
static int mof_is_boss(const uint8_t *e)
{
    uint8_t *em = CE_ENEMY_MGR();
    int32_t id;
    int i;

    if (!e || !em) return 0;
    id = *(const int32_t *)(e + CE_ENEMY_ID);
    for (i = 0; i < CE_EM_BOSS_SLOTS; ++i) {
        if (*(const int32_t *)(em + CE_EM_BOSS_IDS + i * 4) == id) return 1;
    }
    return 0;
}

/* ---- 计分层：声明自己的贡献（粘性；删卡时清 0 退出聚合）----
 * 声明值 = **本卡这份信仰**（基础 50000 + Δ_110），不是「增量」——
 * 因为 `mof_core_pv()` 的公式要的是「聚合出来的信仰之和」。*/
static void mof_declare_score(mof_card_t *s)
{
    if (!s) return;
    ce_score_declare(MOF_ID_NUM, faith_score_unit(&s->slot));
}

/* ---- 收集回调（faith.c 发出来的）----
 * 只认领 `1 << FAITH_SLOT_110` 的槽；加进自己的记账槽 + 连击槽 + 卡内计数，然后重新声明。*/
static void mof_on_pickup(void *user, int slot_mask, int tier, int32_t value)
{
    ce_card_t *c = (ce_card_t *)user;
    mof_card_t *s = c ? st(c) : 0;
    const float *pp;
    int32_t coeff;
    faith_ev_t ev;

    if (!s || !(slot_mask & (1 << FAITH_SLOT_110))) return;

    /* 事件档（§7.5 的表）：大信仰点 `FAITH_EV_BIG`、**小信仰点 `FAITH_EV_SMALL_FAITH`**。
     * ★ 2026-09-12 审计修（任务 2）：原来两条通道**都**走 `FAITH_EV_GREEN`（连击 +100f）
     *   ⇒ 小信仰点比 §7.5 多 40f（review §7.5 的连击表第 864 行：**小 P / 小信仰点 +60f**；
     *   `faith_core.h` 的 `FAITH_EV_SMALL_FAITH` 本来就是为这一档留的）。
     *   ⚠ 映射本体在 `mof_core_pickup_ev()`（纯逻辑层 ⇒ **有单测**，卡侧这段主机编不出来）。*/
    ev = mof_core_pickup_ev(tier);

    /* 信仰：加值由 `value` 给（大 5000/8000/10000、小 100），事件档只决定「哪一档」。*/
    (void)faith_slot_add(&s->slot, ev, value);

    /* 连击槽：大信仰点 +120f、小信仰点 +60f（§7.5 + 一手 th10 的充能立即数；表在 `mof_core.h`）。
     * ⚠ 这里**不用** `faith_combo_add(ev)`：共用件那张表把「大」档写成 130f（一手实测 120f），
     *   而它同时被 111 的大绿点通道读 ⇒ 110 用自己的表（`mof_core_pickup_combo()`），别改共用件。
     * `faith_combo_set()` 自带 [0, 130] 夹取（130 = 槽满，一手 `0x413035`）。*/
    (void)faith_combo_set(&s->slot, s->slot.combo + mof_core_pickup_combo(tier));

    /* 卡内奖残计数（§2.6）：蓝点分 = 信仰 × 高度系数。
     * ★ 2026-09-12 审计修（任务 1）：高度系数改成**真正的线性插值**（§2.4 / review §7.6
     *   「按到收点线（`player+0x47998`）的高度线性打折：线上 = 1.0，板底 ≈ 0.54」）。
     *   旧实现是二元 —— `faith_player_above_line() ? TOP : BOTTOM` ⇒ **线下 1px 就吃 0.54**
     *   （`mof_core_height_coeff()` 当时是死代码）。
     * ⚠ 高度取**收集那一刻的自机 y**：实体层的回调签名（`faith.h` 的 `faith_pickup_fn`）
     *   **不带这颗点的位置**，而收集判定本身就是「与自机的距离² ≤ 收集半径²（默认 30px）」
     *   ⇒ 收的那一帧自机 y 就是收集高度（误差 ≤ 收集半径 ≈ 30px，占 224px 跨度的 13%）。
     *   要精确到「点自己的 y」得给那个回调签名加一个位置参数（`faith.h`/`faith.c` 本轮不在改动范围）。
     *   ⚠ 一手 th10 用的确实是**道具自己的 y**（`0x41b4aa` `fld [edx+0x3c4]` 比 144.0f）⇒ 这是已知偏差。
     * 自机指针为空时退回旧的保守档 BOTTOM（本回调从 `on_tick_2` → `faith_pool_tick()` 里发出来，
     * 那条路已经挡过 `!CE_PLAYER()`，所以这是纯防御）。*/
    pp = ce_player_pos();
    coeff = pp ? mof_core_pickup_coeff(pp[1], faith_player_poc_height()) : MOF_BLUE_COEF_BOTTOM;
    mof_bonus_add(&s->bonus, mof_core_blue_value(faith_slot_total(&s->slot), coeff));

    /* 「收到」计数（`stat_big` / `stat_small`）：**只在这里**记 —— 撒点处记的是「撒出」
     * （`stat_*_spawn`）。两者分开的理由见结构体注释（旧实现一个字段担两件事 ⇒ 一颗点记两次）。*/
    if (tier == (int)FAITH_TIER_BIG) ++s->stat_big; else ++s->stat_small;
    mof_declare_score(s);
}

/* ---- 消弹回调（faith.c 在 `faith_cancel_account()` 里发）----
 * 「微信仰点：**消弹每颗 +10**」（§2.2）。颗数**直接吃 `ce_cancel_radius()` 的返回值**
 * （§5.5 订正 3：`ce_cancel_all_bullets()` 是 void，数不出来）。*/
static void mof_on_cancel(void *user, int32_t bullets)
{
    ce_card_t *c = (ce_card_t *)user;
    mof_card_t *s = c ? st(c) : 0;

    if (!s || bullets <= 0) return;
    s->stat_cancel += bullets;
    faith_slot_add_cancel(&s->slot, 0, bullets, 0, 0);        /* 110：每颗 +10，落 Δ_110 */
    /* 连击槽：微信仰点 +3f/颗（§7.5）。⚠ 原作是「每颗 +3f」，没有每帧上限 —— 照抄。*/
    {
        int32_t k = bullets;
        while (k-- > 0) faith_combo_add(&s->slot, FAITH_EV_MICRO);
    }
    mof_declare_score(s);
}

/* ---- 击破 → 大信仰点（§7.9）★ **正式钩子 = `on_enemy_death`**（共享死亡断点 `0x430240`）----
 *
 * 为什么是死亡钩子（2026-09-12 SDK owner 修完断点后的正式路径）：
 *   · 断点 `0x430240`（`cavesize 6`、期望字节 `558bec83ec14`）**在引擎撒主掉落之前**
 *     （体内顺序：死亡特效 → `0x430469` 读表 → `0x430493` 撒 → `0x43049f` 触发
 *     `on_enemy_drop_pre` → `0x4304aa` 清表）⇒ 只有这里能拿到**未经消费**的 `drop_main`；
 *   · `ce_hooks_t.on_enemy_death` 是**事件钩子**（不是 21 槽虚表成员），`sdk.c` 沿卡链表直接
 *     `h->on_enemy_death(card, &ev)` 派发 ⇒ 卡侧 `.on_enemy_death =` 本来就能被调到
 *     （`0x84` 只是 trace 槽号，`CE_CARD` 不需要为它生成桩）。
 *     ⚠ 2026-09-12 之前挂不上**不是桩的问题，是断点没装**（`sites.py` 下标取址错位 / 对账表缺条目 /
 *     patch 未重生成）—— 三处已由 SDK owner 修好，现在 `CE_BP_ENEMY_DEATH_RVA = 0x030240`。
 *
 * 本回调的两条**硬禁令**（设计 §2.4 的重入风险）：
 *   · **禁止析构任何敌人**（断点必在敌人链表遍历栈里 ⇒ UAF：`sweep` 先存 next、同帧会删）；
 *   · **禁止缓存 `ev->enemy`**（只在本次回调内有效；要记账只用 `ev->id`）。
 *   本实现两条都遵守：只读 `ev` 的字段、只撒自建 VM、不碰任何敌人对象。
 *
 * 判据（§7.9「不按族挑、按这敌实际掉什么」）：先过**死亡事件的门**
 * `mof_core_death_qualifies(kind, flags)` —— **排除 `CE_EDEATH_KILL_ALL`**（过场全清会把计数刷满）/
 * 要 `CE_EDEATH_F_REAL`（真敌人）/ 要 `CE_EDEATH_F_HAS_DROP`（有掉落表）；
 * 再 `drop_main == 2`（TH18 的 type 2 = 金钱）且非 boss。 */
static void on_enemy_death(ce_card_t *c, const ce_enemy_death_t *ev)
{
    mof_card_t *s = st(c);
    int32_t v = 0;
    int is_boss, want_big;

    if (!s || !ev) return;
    if (!mof_core_death_qualifies(ev->kind, ev->flags)) return;
    if (ev->drop_main != MOF_DROP_TYPE_MONEY) return;   /* 判据② 在 drop_pre 里（那里才有 ex_drop）*/
    /* ❓ 自定 / TODO（**用户未定**）：boss 判据借用 `EnemyManager` 的 boss id 表当占位。
     * 只在门已过、`drop_main == 2` 时才查表（避免每死一只都走一遍 boss 表）。*/
    is_boss = mof_is_boss((const uint8_t *)ev->enemy);
    want_big = mof_core_wants_big_death(ev->kind, ev->flags, ev->drop_main, is_boss,
                                       mof_difficulty(), &v);
    if (!want_big) return;
    if (faith_spawn(ev->x, ev->y, FAITH_TIER_BIG, 1 << FAITH_SLOT_110, v)) {
        ++s->stat_big_spawn;
        ce_log("mof: big faith +%d via on_enemy_death @(%.0f,%.0f) (kind %u, drop_main %d, boss %d, enemy %u)",
               (int)v, (double)ev->x, (double)ev->y, (unsigned)ev->kind, (int)ev->drop_main,
               is_boss, (unsigned)ev->id);
    } else {
        ++s->stat_spawn_failed;
    }
}

/* ---- `on_enemy_drop_pre`：**只剩一件事** —— 「同时掉钱与 P」那条大信仰点（判据②）----
 * ⚠ 为什么判据②**留在这个钩子**：`ce_enemy_death_t` **不携带 ex_drop 计数**（只有 `drop_main`），
 *   而「同时掉钱与 P」必须看 `counts`（本钩子的入参就是 `zEnemyDrop+0x04`）。
 *   两条路**互斥**：`drop_main == 2` 已由死亡钩子处理 ⇒ 这里只在 `drop_main != 2` 时判②。
 * ⚠ 主掉落那一颗在 `0x430469` **已经被引擎撒出去了**（本钩子 `0x43049f` 在它之后）
 *   ⇒ 这里**只能额外再撒**（自建 VM），不能替换（§7.9 的实现注意）。
 *
 * ★★ 2026-09-12 审计裁决（本次任务 1）：小信仰点（满 P 的 P 点转化）**从本钩子删掉了**。
 *   理由 = **同一颗 P 点会被记两次**：本钩子看到的是「这一波**要掉**什么」（撒落前/后的一瞬），
 *   而玩家真吃到那颗 P 点时 `on_item_collect` 也会看到同一颗 ⇒ 两处都判
 *   `mof_core_pick_p_item() == MOF_PICK_ADD_SMALL` 都 `faith_spawn(..., FAITH_MOF_SMALL)`
 *   ⇒ **两颗**小信仰点实体（一颗落在敌人尸体处、一颗落在道具被吃处）⇒ 玩家两颗都吃到
 *   ⇒ `Δ_110` 与连击槽各**双份**（`§7.9` 只定义了「满 P 吃 P 点 → 一颗」，`§11 #48` 只说
 *   「自建 VM 池」没说要撒两颗）。
 *   裁决（SDK owner 定）：**只保留 `on_item_collect`**（= 玩家真吃到才产生信仰，口径最准），
 *   本钩子的这颗**纯删** —— 本卡在这里**没有**「满 P 时把 P 点换成蓝点/别的 type」那种**换种**职责
 *   （`mof_core_pick_p_item()` 的返回值里也没有「改写掉落表」这一档，本卡也从不改写 ex_drop），
 *   所以要删就整段删，不留空壳。
 *   ⚠ 判据函数 `mof_core_pick_p_item()` **没动** —— 111（`cards/signal.c:273`）用同一个函数做同一件事。*/
static void on_enemy_drop_pre(ce_card_t *c, int32_t *counts)
{
    mof_card_t *s = st(c);
    uint8_t *e;
    const float *ep;
    int32_t v = 0, drops = 0, p_count = 0;
    int is_boss, want_big;

    if (!s || !counts) return;

    /* counts = zEnemyDrop+0x04（CE_ENEMY_DROP_COUNTS），zEnemyDrop = enemy+0x623c */
    e = (uint8_t *)counts - CE_ENEMY_DROP_COUNTS - CE_ENEMY_DROP;
    is_boss = mof_is_boss(e);
    {
        int32_t main_drop = *ce_drop_main(counts);
        int i;
        for (i = 0; i < CE_ENEMY_DROP_TYPES; ++i) {
            if (i + 1 == MOF_DROP_TYPE_MONEY) drops = counts[i];
            if (i + 1 == MOF_DROP_TYPE_P)     p_count = counts[i];
        }
        ep = ce_enemy_pos(e);
        if (!ep) return;

        /* 唯一职责：大信仰点 —— 只有判据②（「同时掉钱与 P」）走这里；判据① 已由 `on_enemy_death` 处理。
         * `p_count` 仍然是**判据② 的输入**（dual 要「钱与 P 同时 > 0」），不是「P 点转化」的输入。*/
        want_big = (main_drop != MOF_DROP_TYPE_MONEY)
                 && mof_core_wants_big_dual(main_drop, drops, p_count, is_boss);
        if (want_big) {
            v = mof_core_big_value(mof_difficulty());
            if (faith_spawn(ep[0], ep[1], FAITH_TIER_BIG, 1 << FAITH_SLOT_110, v)) {
                ++s->stat_big_spawn;
                ce_log("mof: big faith +%d via drop_pre (main %d, money %d, P %d, boss %d, enemy %u)",
                       (int)v, (int)main_drop, (int)drops, (int)p_count, is_boss,
                       (unsigned)*(const uint32_t *)(e + CE_ENEMY_ID));
            } else {
                ++s->stat_spawn_failed;
            }
        }
    }
}

/* ---- 满 P 后吃 P 点 → 小信仰点（`on_item_collect`；§7.9 / #48）★ **小信仰点的唯一撒点处** ----
 * ⚠ TH18 **没有「信仰点」这个 item type**（§11 #48）⇒ 只判「这颗是不是 P 点（type 1）」，
 *   撒出来的**一律是我们自建 VM**，不去改原生 type、不动分派表。
 * ★ 2026-09-12 审计裁决：`on_enemy_drop_pre` 里那条**同判据**的撒点已删（理由见那个函数的注释）
 *   ⇒ 一颗 P 点 **只撒一颗**小信仰点、**只加一次** `Δ_110`、**只加一次**连击槽。*/
static void on_item_collect(ce_card_t *c, uint32_t type, const void *item)
{
    mof_card_t *s = st(c);
    const float *ip;
    mof_pick_t pick;

    if (!s || !item) return;
    if (CE_CURRENT_POWER() < CE_MAX_POWER_VAL()) return;     /* 不满 P 不转化 */
    if (type != (uint32_t)MOF_DROP_TYPE_P) return;       /* 只有 P 点（type 1）*/
    pick = mof_core_pick_p_item((int32_t)MOF_DROP_TYPE_P, 1, CE_CURRENT_POWER(), CE_MAX_POWER_VAL(), 0);
    if (pick != MOF_PICK_ADD_SMALL) return;
    ip = (const float *)((const uint8_t *)item + CE_ITEM_POS);
    if (faith_spawn(ip[0], ip[1], FAITH_TIER_SMALL, 1 << FAITH_SLOT_110, FAITH_MOF_SMALL)) {
        ++s->stat_small_spawn;
    } else {
        ++s->stat_spawn_failed;
    }
}

/* ---- 计分层落笔（§7.8）----
 * ⚠ 只在**自己是被选举的落笔者**时动手（`ce_score_applier() == 110`），
 *   且只读**聚合值**（不读任何别的卡的状态）。写 `*value = PV × 10`，
 *   引擎随后 `SCORE += value / 10` ⇒ 精确得到 PV 分、天然不双计分。
 *
 * ★★ 2026-09-12：**第二条通道（乘数附加项）已接**（`sdk_core.h:105-112` / `EVENTS-CONTRACT §3.1`）。
 *   起因：110 与 111 同装时落笔者**永远是 110**（id 最小），而 111 的信号强度只在 111 自己手里；
 *   契约又禁止落笔者读别的卡的状态 ⇒ 110 原来只能把 `signal_value` 写死 0 ⇒
 *   **两卡同装时信号强度整条丢失**（`PV = (基础均值 + ΣΔ) × 擦弹倍率`，少了 `+ 信号强度`）。
 *   现在的链路（**谁声明什么 / 落笔者读哪两个聚合值**，一眼看全）：
 *     · 加数通道 `ce_score_declare(id, …)`：110 声明「基础 50000 + Δ_110」；
 *       111 声明「基础(难度) + Δ_111」⇒ 落笔者读 `ce_score_aggregate()`（Σ）作**被乘数**。
 *     · 乘数通道 `ce_score_mul_declare(id, permille)`：**111 声明自己的信号强度（千分点）**，
 *       110 **不声明**（它没有乘数附加项）⇒ 落笔者读 `ce_score_mul_aggregate()`（Σ 千分点）作**乘数的一部分**。
 *     · **擦弹倍率不进这个通道** —— 它是全局擦弹计数算出来的（`GLOBALS.inner+0x30`），
 *       由 110 自己算（`mof_core_graze_mult()`，本文件 `on_tick_2` ①）、**落笔者自己加**；
 *       若也声明进去就是**同一份擦弹倍率被加两次**（`sdk_core.h:109-110` 的原话）。
 *       ★ 2026-09-12 一手订正（任务 ③）：**这一项是 th11（SA）的机制，TH10 没有擦弹倍率**
 *       （`research-th10-faith-and-graze.md` §1.1）⇒ 它**只在「110 与 111 同装」这一支里**才该出现
 *       （那时它是 111 的一半），110 单装时整项不参与（下面那一支）。
 *   ⇒ 最终式（两张都装）：`PV = (基础均值 + Δ_110 + Δ_111) × (擦弹倍率 + Σ信号强度‰) / 1000`，
 *     由 `mof_core_pv(1, have111, ce_score_aggregate(), s->graze_mult, ce_score_mul_aggregate())` 落笔
 *     （两个乘数在 `mof_core_pv()` 内部相加、再整体 `/1000`，所以这里传的是**千分点**，不是倍数）。
 *   ⚠ 只有 110 单装时 `mof_core_pv()` 走「只装 110」那一支 ⇒ `PV = 信仰_110`、**乘数整项不用**
 *     （MoF 里信仰**就是**最大得点，§7.4；一手 th10 的蓝点线上式 `0x41b4bd`→`0x41b569` 也是
 *     「线上 = 信仰、无乘数」）⇒ 111 不在场时这条通道有没有值都不影响结果。*/
static void on_item_score(ce_card_t *c, int32_t *value)
{
    mof_card_t *s = st(c);
    int have111;
    int32_t pv;

    if (!s || !value) return;
    if (ce_score_applier() != (int)MOF_ID_NUM) return;       /* 不是落笔者 ⇒ 什么都不做 */
    have111 = ce_owned(111) ? 1 : 0;
    /* 被乘数 = 聚合的信仰；乘数 = **110 自算的擦弹倍率 + 111 声明的信号强度**（两个都是千分点）。
     * ⚠ 顺序照 `mof_core_pv()` 的形参：`(have110, have111, aggregate, graze_permille, signal_permille)`。*/
    pv = mof_core_pv(1, have111, ce_score_aggregate(), s->graze_mult, ce_score_mul_aggregate());
    /* ⚠ 2026-09-12 审计：先钳到 int32/10 以内再乘 10 —— `mof_core_pv()` 自己只钳到
     * `0x7fffffff`，直接 `pv * 10` 会**有符号溢出（UB）**。111 侧一直有这个钳（`signal.c`
     * 的 `if (pv > 200000000)`），110 侧漏了 ⇒ 补齐，口径与 111 一致。*/
    if (pv > 200000000) pv = 200000000;
    if (pv <= 0) return;
    *value = pv * 10;                                    /* 引擎会 /10 ⇒ 精确 PV 分 */
    (void)c;
}

/* ---- 灵击（§2.1 / §3.2.1）----
 * 触发：`on_tick_2` 读炸弹键**上升沿**（`INPUT_PRESSED & 0x2`）—— **不是 `on_bomb_spent`**
 * （它在 `0x4203bc`，炸弹**成功花掉之后**才触发，承载不了「库存 == 0」这条分支，§5.5 订正 2）。
 * 库存 == 0 → 本卡自干：自写扣火力（≥2 档、扣 1 档、下限 1.05）+ 全屏消弹 + 伤害源 + 震屏；
 * 库存 > 0 → 交给原生 `do_bomb`（本卡不动）。
 * ⚠ 扣完**必须** `ce_repopulate_options()`（Tsukasa / 商店先例）。*/
static void mof_bomb(ce_card_t *c, mof_card_t *s)
{
    const float *pp = ce_player_pos();
    float pos[3];
    mof_power_t pw = mof_core_power_spend(CE_CURRENT_POWER());
    int n = 0;

    (void)c;                                             /* 本函数不用卡对象（回调签名统一）*/

    if (!pp || !pw.ok) return;                           /* 火力不够 ⇒ 什么都不做（照原作）*/

    *(int32_t *)CE_ADDR_CURRENT_POWER = pw.new_power;    /* ★ 自写扣法（原生会拒绝、且只能整档扣）*/
    ce_repopulate_options();                             /* 子机按新档重建（必须）*/

    pos[0] = pp[0]; pos[1] = pp[1]; pos[2] = pp[2];
    /* 全屏消弹：**用 `ce_cancel_radius` 的返回值**（`ce_cancel_all_bullets()` 是 void、数不出来，
     * §5.5 订正 3）。本照「本帧已消过弹」的闩锁（EVENTS-CONTRACT §5.2 细则 2）。*/
    if (ce_shoot_cancel_claim((uint32_t)MOF_ID_NUM)) {
        n = ce_cancel_radius(pos, MOF_BOMB_CANCEL_R, MOF_BOMB_CANCEL_MAX, 0);
        if (n > 0) {
            faith_cancel_account(n);                     /* → 微信仰点每颗 +10（进 Δ_110）*/
            mof_declare_score(s);
        }
    }
    /* 全场伤害 + 震屏。❓ 参数是占位常量（`mof_core.h` 的 MOF_BOMB_DMG*），TODO 抄 th10 灵击 */
    {
        float d = MOF_BOMB_CANCEL_R;
        ce_raise_damage_cap(MOF_BOMB_DMG);
        ce_damage_rect(pos, 0.0f, MOF_BOMB_DMG_LIFE, MOF_BOMB_DMG, d, d);
    }
    ce_screen_shake(MOF_BOMB_SHAKE_TIME, MOF_BOMB_SHAKE_START, MOF_BOMB_SHAKE_END);

    s->spell_watch.bombed = 1;                           /* 本张符卡期间放过灵击 ⇒ 不算全避 */
    ++s->stat_bomb;
    faith_slot_reduce(&s->slot, FAITH_REDUCE_BOMB);       /* 非决死灵击 −3000（§7.5）*/
    mof_declare_score(s);
    ce_log("mof: 灵击 -1 档 (power %d -> %d), cancelled %d, faith %d, combo %d",
           (int)CE_CURRENT_POWER(), (int)pw.new_power, n, (int)faith_slot_total(&s->slot),
           (int)s->slot.combo);
}

/* ---- 每帧 ---- */
static int on_tick_2(ce_card_t *c)
{
    mof_card_t *s = st(c);
    int32_t g, dg;
    int lives, spell_now;

    if (!s || !CE_PLAYER()) return 0;
    ++s->frame;

    /* ① 擦弹 → 擦弹倍率（§7.5：`min(graze × 0.0001, 8.99)`）
     * ★ 2026-09-12 一手订正（任务 ③）：**这是 th11（SA）的式子，TH10 没有擦弹倍率**
     *   （`research-th10-faith-and-graze.md` §1.1：`0.0001f` 只作 `fpatan` 的 epsilon、
     *   全 exe 无 `8.99f`、13 个计分口无一处读擦弹）⇒ 这里算出来的值**只服务「110 与 111 同装」
     *   那一支**（那时乘数 = 擦弹倍率 + 111 声明进来的信号强度，是契约 §7.4 的式子）；
     *   **110 单装时它整项不参与**（`mof_core_pv()` 的「只装 110」那一支 = `aggregate`）。
     *   实跑对账时看日志里的这条：它代表**111 那一半**，不是 110 的机制。*/
    g = mof_graze();
    if (s->last_graze < 0 || g < s->last_graze) s->last_graze = g;
    dg = g - s->last_graze;
    s->last_graze = g;
    s->graze_mult = mof_core_graze_mult(g);
    if (dg > 0) faith_combo_add(&s->slot, FAITH_EV_MICRO);   /* ❓ 口径待核：擦弹是否续连击（见报告）*/

    /* ② 连击槽推进（★ 2026-09-12 一手订正：**槽空不再扣信仰** —— TH10 没有逐帧信仰衰减，
     *   见 `mof_core.h` 的 `mof_core_combo_tick()` 注释与 `research-th10-faith-and-graze.md` §3.2/§3.4）。*/
    {
        int boss_fight = 0;
        uint8_t *em = CE_ENEMY_MGR();
        int i;
        if (em) {
            for (i = 0; i < CE_EM_BOSS_SLOTS; ++i) {
                if (*(const int32_t *)(em + CE_EM_BOSS_IDS + i * 4) != 0) { boss_fight = 1; break; }
            }
        }
        (void)mof_core_combo_tick(&s->slot, boss_fight);
    }

    /* ③ miss 检测（命数减少）→ `−(信仰−50000)/3`（§7.5）。
     * ⚠ 引擎没有「miss」回调 ⇒ 这里用**命数**差分（比 state 机可靠：决死救回不改命数）。*/
    lives = CE_CURRENT_LIVES();
    if (s->last_lives >= 0 && lives < s->last_lives) {
        faith_slot_reduce(&s->slot, FAITH_REDUCE_MISS);
        faith_combo_set(&s->slot, 0);                    /* miss 清连击槽（§2.3）*/
        ++s->stat_miss;
        mof_declare_score(s);
    }
    s->last_lives = lives;

    /* ④ 全避（boss 非符 / 符卡）−30000（§2.2 / §7.5）
     * ★ 2026-09-12 审计修：「全避」= 这一张**拖到超时**（没打掉、时间到了）⇒ 才扣原版的 −30000；
     *   **打掉的（收了符卡）不扣**。判据三条同时成立才算：
     *     ① 本次符卡期间读到过 `CE_SPELL_FLAG_TIMED_OUT`（`0x80`，engine.h:79）——
     *        引擎在 `0x42efc0` `or [sp+0x78],0x80` 置位（= 计时器走完那一路）；
     *        **耐久符卡不算**：那条在 `0x42efb3 test cl,0x8 / jne 0x42f016` 走「按收符卡算」，
     *        引擎**不置**超时位 ⇒ 只要认这一位，「时符必减 / 耐久符卡按收符卡算」自动成立。
     *     ② 本卡**没在这一张里放过灵击**（放了灵击就不是「靠避」过的，§2.2）。
     *     ③ 只在符卡 active 的**下降沿**结算（= 这一张结束了）。
     *   ⚠ 状态机本身在 `mof_core_spell_watch()`（`mof_core.c`）——**故意搬进 core**，因为
     *     卡侧代码在主机上编不出来（`sdk.h` 的 inline asm）⇒ 留在 tick 里就一条断言都写不了，
     *     而这次修的正是这段判据。锁存语义与「新符卡第一帧不读残留位」的理由见那个函数的注释。
     *   ⚠ 旧实现只判「没放灵击」⇒ **每张符卡正常收掉也扣 30000**（一局 boss 战就能把 Δ_110
     *     打到下限 0、信仰永久钉在 50000）。这是审计发现并修掉的行为差异。*/
    spell_now = ce_spell_active();
    if (mof_core_spell_watch(&s->spell_watch, spell_now,
                             (ce_spell_flags() & CE_SPELL_FLAG_TIMED_OUT) != 0)) {
        faith_slot_reduce(&s->slot, FAITH_REDUCE_AVOID);
        ++s->spell_avoided;
        mof_declare_score(s);
    }

    /* ⑤ 灵击（库存 == 0 且炸弹键上升沿）*/
    if (CE_CURRENT_BOMBS() <= 0 && (*(uint32_t *)CE_ADDR_INPUT_PRESSED & 0x002u)) {
        mof_bomb(c, s);
    }

    /* ⑥ 绿点池推进（共用件：撒点 / 吸附 / 收集 → 回调 `mof_on_pickup`）*/
    faith_pool_tick();

    /* ⑦ 卡内奖残（§2.6：20m / 40m / 80m / 150m → `ce_add_life()`）*/
    {
        int n = mof_bonus_take(&s->bonus);
        while (n-- > 0) {
            ce_add_life();
            ce_log("mof: bonus threshold reached (%d/%d) -> +1 life (bonus %d)",
                   (int)mof_bonus_awarded(&s->bonus), MOF_BONUS_MAX, (int)s->bonus.bonus);
        }
    }

    /* ⑧ 诊断汇总 */
    if (s->frame - s->log_frame >= MOF_LOG_EVERY) {
        s->log_frame = s->frame;
        /* ★ 「收到 / 撒出」两两并排（照 `cards/signal.c:437-438` 的口径）：
         *   big/small = 玩家真吃到的，bigSp/smallSp = 撒出去的。实跑时前者 ≤ 后者；
         *   满 P 吃一颗 P 点 ⇒ `smallSp +1`，吃到后 `small +1`（**各一次**，不再像旧实现那样一颗点两边都 +1）。
         * ⚠ `x%d` 那项 = **th11 的擦弹倍率**（110 单装时不参与 PV，见 `on_item_score` 的注释）。*/
        ce_log("mof: faith %d (Δ %d), combo %d, graze %d (th11 x%d), bonus %d, "
               "big %d/%d small %d/%d (got/spawn), cancel %d miss %d bomb %d avoid %d",
               (int)faith_slot_total(&s->slot), (int)(s->slot.delta / FAITH_FIXED),
               (int)s->slot.combo, (int)s->last_graze, (int)s->graze_mult,
               (int)s->bonus.bonus,
               (int)s->stat_big, (int)s->stat_big_spawn,
               (int)s->stat_small, (int)s->stat_small_spawn,
               (int)s->stat_cancel, (int)s->stat_miss, (int)s->stat_bomb, (int)s->spell_avoided);
        if (s->stat_spawn_failed)
            ce_log("mof: %d faith point(s) dropped (pool full)", (int)s->stat_spawn_failed);
    }
    return 0;
}

/* ---- HUD（左下角：信仰数字 + 连击条数值；111 在场时多一行信号）----
 * 用 **贴图 VM 字形**（`ce_hud_vm_text_set`：TH18 **font 2** 的零售切图，season.c 的档位数字先例）——
 * 不用 ASCII 文字的原因是 `sdk.h` 记下的一手结论：**ASCII 文字永远盖不到暂停模糊罩下面**，
 * 而贴图 VM 走 UI 层 25（折到组 38、与罩子同桩）能被罩住。坐标仍是 HUD 字面量空间（640×480）。
 *
 * ❓ 坐标是**初版**：按「游玩区左下角」的意图给的（季节点槽那套 308,916 是 TH16 原版的表面像素，
 *   两套空间不同、不能照搬）。原版 MoF 的信仰数字位置**我们没抄到**（TH10 的 HUD 脚本未查）
 *   ⇒ 标 `❓ 自定`，实跑按截图挪。连击条的「条」也还没画（只有数字），同属待实跑项。*/
static int on_draw(ce_card_t *c)
{
    mof_card_t *s = st(c);
    char buf[32];

    if (!s) return 0;
    /* ★ 2026-09-12 审计：`on_draw`（虚表 `+0x48`）在**主菜单 / 标题**里也会被调，而 `on_tick_2`
     *   （`+0x2c`）不会 ⇒ 不设门就会把两行 HUD 数字画到菜单上（117 的 bug3 就是这个坑；
     *   115 `lolk.c` / 116 `season.c` / 117 `animal.c` 三张卡都用同款门）。
     *   门关着时**把 VM 收掉**，别只是不画（VM 会留着被别处按旧坐标重画）。*/
    if (!CE_PLAYER() || !CE_GAME_THREAD()) {
        ce_hud_vm_text_clear(&s_faith_txt);
        ce_hud_vm_text_clear(&s_combo_txt);
        return 0;
    }
    snprintf(buf, sizeof buf, "%d", (int)faith_slot_total(&s->slot));
    ce_hud_vm_text_set(&s_faith_txt, MOF_HUD_FAITH_X, MOF_HUD_FAITH_Y, MOF_HUD_COLOR_FAITH,
                       CE_ASCII_ALIGN_LEFT, 0.0f, buf, "mof");
    snprintf(buf, sizeof buf, "%d/130", (int)s->slot.combo);
    ce_hud_vm_text_set(&s_combo_txt, MOF_HUD_FAITH_X, MOF_HUD_COMBO_Y, MOF_HUD_COLOR_COMBO,
                       CE_ASCII_ALIGN_LEFT, 0.0f, buf, "mof");
    /* ★ 2026-09-12：这里原来有一行 111 信号槽的占位（`"sig 0"`）。111 现在有自己的 HUD 与信号实现
     * ⇒ 110 **不再替它画**（两张卡各画各的，避免同一份状态两处显示、也避免跨卡读状态）。*/
    return 0;
}

/* ---- 生命周期 ---- */
static void mof_clear(ce_card_t *c)
{
    mof_card_t *s = st(c);

    /* ★ 顺序要紧：先退出计分层（否则删掉的卡还在往聚合里声明），再收实体与 HUD。*/
    ce_score_declare(MOF_ID_NUM, 0);
    faith_unregister(MOF_ID_NUM);
    /* ★ 只收**自己槽**的绿点（`1 << FAITH_SLOT_110`）。这里原来是 `faith_clear_entities()`
     * （清整个共用池）⇒ 110 在商店里被倒吊人删掉时，**111 同行还飘着的绿点会跟着凭空消失**
     * （两张卡共用一个实体池，`faith.h` 头注）。按 `slot_mask` 筛的那一支是 2026-09-12 审计补的，
     * 下面这一行就是「删卡路径漏在哪」的补丁。*/
    faith_clear_entities_mask(1 << FAITH_SLOT_110);
    ce_hud_vm_text_clear(&s_faith_txt);
    ce_hud_vm_text_clear(&s_combo_txt);
    if (!s) return;
    memset(s, 0, sizeof *s);
    s->last_graze = -1;
    s->last_lives = -1;
}

static int on_load(ce_card_t *c)
{
    mof_card_t *s = st(c);
    faith_user_t u;

    if (!s) return 0;
    s->last_graze = -1;
    s->last_lives = -1;
    memset(&u, 0, sizeof u);
    u.card_id = MOF_ID_NUM;
    u.slot_mask = 1 << FAITH_SLOT_110;       /* 只消费 110 槽 */
    u.on_pickup = mof_on_pickup;
    u.on_cancel = mof_on_cancel;
    u.user = c;
    if (!faith_register(&u)) ce_log("mof: faith_register failed (table full?)");
    mof_declare_score(s);
    return 0;
}

static int ctor(ce_card_t *c)
{
    mof_card_t *s = st(c);

    if (!s) return 0;
    faith_slot_init_110(&s->slot);            /* 基础恒 50000、Δ 从 0 起算、下限 0 */
    mof_bonus_init(&s->bonus);
    s->last_graze = -1;
    s->last_lives = -1;
    s->log_frame = 0;
    mof_declare_score(s);
    return 0;
}

static void on_run_reset(ce_card_t *c) { mof_clear(c); }
static void on_delete(ce_card_t *c)    { mof_clear(c); }

/* 钩子分工（两条路都留，互斥）：
 *   · `on_enemy_death`（断点 `0x430240`，**撒落之前**）→ 大信仰点**判据①**：`drop_main == 2`
 *     且过死亡事件的门（非 KILL_ALL / 真敌人 / 有掉落表）/ 非 boss。这是 §7.9 的主路。
 *   · `on_enemy_drop_pre`（`0x430510`，撒落之后）→ **只剩**大信仰点**判据②**「同时掉钱与 P」
 *     （需要 ex_drop 计数，只有这个钩子有）。
 *     ★ 小信仰点（满 P 时 P 点转化）**已从这里删掉**（2026-09-12 裁决）：它与 `on_item_collect`
 *       判的是**同一颗 P 点** ⇒ 撒两颗实体 ⇒ Δ_110 与连击槽各双份。现在小信仰点**只有**
 *       `on_item_collect` 一处撒点。理由全文见 `on_enemy_drop_pre` 的注释。
 *   · 本卡**不改写掉落表**：两路都是「额外加撒自建 VM」⇒ 不需要死亡钩子「在撒之前」的那个特权，
 *     但那仍是拿到**未消费 `drop_main`** 的唯一位置，所以判据① 放那里是最干净的。
 * 「不做」清单（spec §6）与长持续效果自查（METHOD 铁律二）见文件头。*/
CE_CARD(110,
        .ctor = ctor,
        .on_load = on_load,
        .on_tick_2 = on_tick_2,
        .on_draw = on_draw,
        .on_enemy_death = on_enemy_death,
        .on_enemy_drop_pre = on_enemy_drop_pre,
        .on_item_collect = on_item_collect,
        .on_item_score = on_item_score,
        .on_run_reset = on_run_reset,
        .on_delete = on_delete);
