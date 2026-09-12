/* 妖妖梦（TH07 系统卡，card-expand id 107）—— 樱点 / 森罗结界的 TH18 卡侧接线。
 *
 * 纯逻辑在 `cards/senro_core.{h,c}`（主机单测 `tests/test_senro_core.c`，**无引擎依赖、无浮点**）；
 * 本文件只做**引擎面**：消弹调用、无敌声明、道具 / 得分事件、HUD、生命周期清理。
 *
 * 一手依据（th07 侧的逐条地址与逐指令结论全在 `cards/senro_core.h` 头注，这里不重复）：
 *   · 机制记录（用户口径）：`mods/th18.v1.00a/card-expand/IDEAS.md` §「107 妖妖梦」（约 513 行起）
 *   · 体检结论 + 用户裁决：`docs/superpowers/specs/2026-09-11-system-card-spec-review.md` §9（含 §9.5.1 的
 *     VA→offset 勘误、§9.7 森罗结界机制、§9.7.1 断点核对）
 *   · 一手研究（**最新，冲突时以它为准，但本卡逐指令复核了两处，见 senro_core.h 头注的订正 ①②**）：
 *     `local/_scratch/research-107-th07-remaining.md`；反汇编全文 `local/_scratch/th07.asm`
 *   · 多卡同装：`mods/th18.v1.00a/card-expand/EVENTS-CONTRACT.md`（§4 玩家保护层、§5.2 救援层定案）
 *   · SDK 面：`mods/th18.v1.00a/card-expand/SDK.md` §6 / §6.1（事件与声明层）
 *   · 本卡 spec：`docs/superpowers/specs/2026-09-12-senro-107-design.md`（数值表 / 偏差表 / 未定项）
 *
 * ★ 地址写法：本文件的**裸地址 = TH18 v1.00a**（`th18.exe`，imagebase `0x400000`）；
 *   引 th07 一律带前缀 `th07:0x…`。
 *
 * ── 这张卡怎么玩（一句话）─────────────────────────────────────────────────────────────
 * 收道具 / 结界中擦弹 / 自机弹命中 → **樱点**与 **+C** 一起涨；**+C 到 50000 自动开森罗结界**（7 s）。
 * 结界中：**每帧全屏收点**、**擦弹同时涨樱点与最大樱点**、+C 按剩余计时比例**持续下降**。
 * **中弹或按 B ⇒ 灵击**：结界中断 + **全屏弹转小樱点**（每颗 +100 樱点 / +30 +C）+ **0.67 s 无敌**（40 帧）。
 * **自然到期** ⇒ 樱点与最大樱点各 **+10000** + **樱点分**的 Border Bonus。**Miss / Bomb 扣樱点**（不动 +C、不动最大樱点）。
 * 樱点是本卡的「最大得点」：吃一颗点道具（TH18 金钱）的得分 = 樱点（经 `on_item_score` 改写身价）。
 *
 * ── 长持续效果的自查（`METHOD.md` 铁律二 / `SDK.md` §6.1 尾）────────────────────────────
 * 本卡跨帧留下的东西：① 无敌声明层的一格（`ce_invuln_request`）；② 计分声明层的一格（`ce_score_declare`）；
 * ③ HUD 槽位 + 三行贴图 VM 文字。①② 由 `CE_CARD` 的 `on_run_reset` / `opdelete` 自动 `ce_layer_drop_card()` 清，
 * ③ 的槽位也自动 release，但**文字 VM 是本卡自己的**，必须在 `on_delete` / `on_run_reset` 里 `ce_hud_vm_text_clear()`。
 * ⚠ 删卡发生在**商店里**（倒吊人 / 原版空白卡清空整副牌），那一刻 AbilityManager 冻着、卡的 tick 全停
 * ⇒ 只能靠 `on_delete`（虚表 `+0x50`），不能等下一帧。本卡**不建任何世界 VM**（结界的特效需要
 * `ability.anm` 新增脚本 = 资产改动，不在本次范围）⇒ 世界侧没有要收的东西。
 */
#include <stddef.h>          /* sdk.h 的 ce_item_collect_all 用了 size_t（内置 Linux 头不带）*/
#include <string.h>
#include <stdio.h>

#include "sdk.h"
#include "senro_core.h"

#define SENRO_ID                    107

/* ============================ 引擎面：本卡自己补的裸地址 / 常量 ============================
 * 0x4ccd0c  原生擦弹计数（`GLOBALS.inner+0x30`）。engine.h 没有提炼这个地址，先例 = `cards/lolk.c:33`、
 *           `cards/season.c:70`、`cards/mof.c:54`（三处同址）⇒ 本卡就地补一份，值一样。
 * 0x45d590  `Player__cancel_impending_death` —— thiscall(玩家)，取消本次死亡。
 *           先例 = `cards/animal.c:243`（117 咆哮「中弹不扣命」的同一调用）。本卡用它实现
 *           「结界中弹 = 消耗结界救命」（EVENTS-CONTRACT §4.2 的**救援层**）。
 */
#define SENRO_ADDR_GRAZE            0x4ccd0c
#define SENRO_FN_CANCEL_IMPENDING_DEATH 0x45d590
/* 0x4ccd00  难度（0 EASY .. 3 LUNATIC；`GlobalsInner` 首字段区，**只读**）。
 *           先例 = `cards/mof.c:52` 的 `MOF_ADDR_DIFFICULTY`、`cards/th06rank.c` 同址。
 *           本卡只用它查 th07 的最大樱点难度表（`th07:0x42eb7a`–`0x42ec57`）。*/
#define SENRO_ADDR_DIFFICULTY       0x4ccd00

/* X（炸弹）键的上升沿位：`CE_ADDR_INPUT_PRESSED` 0x4ca434 bit 0x002（先例 `cards/animal.c:1830`、`cards/mof.c:412`）。
 * ⚠ 只**读**、绝不写输入位（回放安全：这份掩码就是 replay 录的那一份，review §4.4 / §6.7）。*/
#define SENRO_INPUT_BOMB            0x002u

/* 灵击的全屏消弹：半径 / 上限 / 模式。半径 4000 = 全屏量级（`sdk.h` §6.1 的示例值、`cards/mof.c:179` 同款）。*/
#define SENRO_CANCEL_R              4000.0f
#define SENRO_CANCEL_MAX            9999
#define SENRO_CANCEL_MODE           0        /* 0 = 纯删除（`Bullet__cancel` 0x428e90 的 mode）
                                              * ★ 选 0 不选 1 的理由：mode 1 会让引擎撒出 type 9–0xe 的点道具，
                                              * 而那一族在本卡是「★ 通道」（收一颗再 +20 樱点）⇒ 与小樱点的
                                              * +30/+70 双计。本卡 spec §4.3 偏差表里记了这条取舍。*/

/* 音效：两作的 SE 表 id **不同义** ⇒ 老实标 ❓ 自定（th07 用的是 0x21 与 7）。
 * TH18 侧名字与调校见 `engine/_shared/th18-sound-table.md`。*/
#define SENRO_SE_BORDER_OPEN        0x2f     /* ❓ 自定：`se_bonus.wav`（th18-sound-table.md:204）——
                                              * th07 开结界那一支（`th07:0x441960` 尾段）没找到播音调用。TODO(th07)*/
#define SENRO_SE_BORDER_END         0x2f     /* ❓ 自定：同上。th07 自然结束播 SE 0x21（`th07:0x4416f4`）；
                                              * TH18 的 0x21 = `se_cat00`（th18-sound-table.md:190）语义不符 ⇒ 用 bonus。TODO(th07)*/
#define SENRO_SE_LIBYOU             0x09     /* ❓ 自定：`se_cancel00.wav`（th18-sound-table.md:166）。
                                              * th07 灵击播 SE 7 + 0x21（`th07:0x441e43` / `th07:0x441e51`）。TODO(th07)*/

/* 震屏：th07 的打断在 `th07:0x441d35`/`0x441dd9` 起 32 个环形特效单元（没有全屏震动的一手证据）
 * ⇒ 参数是**表现占位**（与 `cards/mof.c:183` 的灵击同款）。TODO(th07)*/
#define SENRO_SHAKE_TIME            20
#define SENRO_SHAKE_START           12
#define SENRO_SHAKE_END             0

/* HUD：左下角三行（+C / 樱点 / 最大樱点）。块尺寸报给槽位分配器（表面 px，HUD-LAYOUT.md §2/§3）。*/
#define SENRO_HUD_W                 200.0f
#define SENRO_HUD_H                 40.0f
#define SENRO_HUD_ROW_DY            12.0f   /* 行距（640×480 字面量空间；行高 = scale_y × 10）*/
#define SENRO_HUD_DIM               0x40u    /* 自机贴近左下角时整摞的 alpha（HUD-LAYOUT §4.5：64/255）*/

/* th07 的 HUD 颜色（一手 `th07:0x4049a4`，三字节写在 `obj+0x89c..0x89e` = **RGB**）：
 *   ≥ 50000 → `(0x80,0xFF,0xFF)` 亮青；< 50000 → `(0x80,0xD0,0xFF)` 浅蓝。
 * TH18 的 `ce_hud_vm_text_set` 收 BGRA ⇒ 逐字节换序。*/
#define SENRO_HUD_COLOR_PLUS_HOT    0xffffff80u   /* RGB(0x80,0xFF,0xFF) */
#define SENRO_HUD_COLOR_PLUS        0xffffd080u   /* RGB(0x80,0xD0,0xFF) */
#define SENRO_HUD_COLOR_CHERRY      0xffffffffu
/* ❓ 自定：樱点满最大樱点时「数字变金黄」（IDEAS §107 / 用户口径）；th07 的金黄常量本轮没取到。TODO(th07)*/
#define SENRO_HUD_COLOR_GOLD        0xff60d0ffu   /* BGRA = RGB(0xFF,0xD0,0x60) */

/* 诊断日志间隔（游玩帧）。*/
#define SENRO_LOG_EVERY             600
/* 「樱点满值」飘字的最小间隔（th07 每加一次都飘，0x42f679；这里节流防刷屏）。*/
#define SENRO_MAXED_POPUP_GAP       30
/* 倍率弹字的第二行（`BONUS %.1f`）：TH18 只有 `%d` + `BONUS %.1f` 两种格式（engine.h 的
 * `CE_POPUP_VALUE` / `CE_POPUP_MULT` 一手），表达不出 th07 的 `Border Bonus %7d`（`th07:0x4416dd` kind=4）
 * ⇒ Bonus 分放进 `%d` 行、第二行恒 1.0（= ×1，不额外改分）。*/
#define SENRO_POPUP_MULT            1.0f

typedef struct {
    senro_state_t    st;                /* 纯逻辑状态（senro_core.h）*/
    ce_hud_slot_t    slot;              /* HUD 槽（左下角货架；认领在 on_load）*/
    int32_t          slot_ok;           /* 1 = 认领成功 */
    int32_t          frame;             /* 本关游玩帧计数（节流 / 日志）*/
    int32_t          log_frame;
    int32_t          maxed_popup_at;    /* 上次「樱点满值」飘字的帧号 */
    int32_t          spell_was;         /* 上一帧符卡进行中 */
    int32_t          spell_bonus_alive; /* 本张符卡期间奖励位还活着（= 收掉，不是超时）*/
    int32_t          collect_log_at;    /* 结界自动收点的日志节流 */
    int32_t          hit_cancel_pending;/* 中弹灵击的消弹待补做（下一帧 flush；理由见 senro_do_libyou）*/
    int32_t          stat_collect;      /* 诊断：结界期间自动收点总数 */
    int32_t          stat_gain_item;    /* 诊断：道具通道加的樱点总数 */
    int32_t          stat_gain_graze;   /* 诊断：擦弹通道 */
    int32_t          stat_gain_hit;     /* 诊断：命中通道 */
} senro_card_t;

_Static_assert(sizeof(senro_card_t) <= 496, "ce_state 上限 496 字节（超了会静默返回 NULL）");

/* HUD 的三行文字（DLL 静态区；与 season.c / mof.c 同口径 —— 一张卡同时只在场一个对象）。*/
static ce_hud_vm_text_t s_hud_plus;
static ce_hud_vm_text_t s_hud_cherry;
static ce_hud_vm_text_t s_hud_max;

static senro_card_t *st(ce_card_t *c) { return ce_state(c, senro_card_t); }

static int32_t senro_graze_now(void) { return *(volatile int32_t *)SENRO_ADDR_GRAZE; }

static void senro_cancel_impending_death(void)
{
    void *p = CE_PLAYER();
    if (p) ((void (__attribute__((thiscall)) *)(void *))SENRO_FN_CANCEL_IMPENDING_DEATH)(p);
}

/* 难度（0 EASY..3 LUNATIC；= 0x4ccd00，`cards/mof.c` / `cards/th06rank.c` 同址，**只读**）。
 * th07 的最大樱点初值按难度查表（`th07:0x42eb7a`–`0x42ec57`），本卡照搬同一张表。*/
static int senro_difficulty(void) { return *(volatile int32_t *)SENRO_ADDR_DIFFICULTY; }

/* ---- 倍率弹字（TH18 原生 PopupManager 的 rate 槽 k13–17；**逐字段照 `cards/collect2x.c` 的
 * `ce_popup_mult()` 抄**，字段语义一手见 engine.h 的 `CE_POPUP_*` 注）---- */
static int senro_popup(const float *pos, int32_t value, uint32_t color)
{
    uint8_t *mgr = CE_POPUP_MGR();
    int k;

    if (!mgr || !pos) return 0;
    for (k = 0; k < CE_POPUP_RATE_SLOTS; ++k) {
        uint8_t *s2 = mgr + CE_POPUP_RING_RATE + (size_t)k * CE_POPUP_STRIDE;
        if (*(uint8_t *)(s2 + CE_POPUP_BUSY)) continue;              /* 占着：引擎自己会回收 */
        *(int32_t *)(s2 + CE_POPUP_AGE - 4) = -1;
        *(int32_t *)(s2 + CE_POPUP_AGE) = 0;
        *(int32_t *)(s2 + CE_POPUP_AGE + 4) = 0;
        if (!(*(uint32_t *)(s2 + 0x30) & 1u)) {                       /* 动画类标志 bit0（照 adder）*/
            *(uint32_t *)(s2 + 0x30) |= 1u;
            *(int32_t *)(s2 + 0x2c) = 0;
        }
        *(float *)(s2 + CE_POPUP_POS) = pos[0];
        *(float *)(s2 + CE_POPUP_POS + 4) = pos[1];
        *(float *)(s2 + CE_POPUP_POS + 8) = pos[2];
        *(float *)(s2 + 0x18) = 1.0f;                                 /* scale（弹出动画起点）*/
        *(uint32_t *)(s2 + CE_POPUP_COLOR) = color;
        *(int32_t *)(s2 + CE_POPUP_VALUE) = value;
        *(float *)(s2 + CE_POPUP_MULT) = SENRO_POPUP_MULT;
        *(uint8_t *)(s2 + CE_POPUP_BUSY) = 1;
        return 1;
    }
    return 0;
}

static void senro_popup_at_player(int32_t value, uint32_t color)
{
    const float *pp = ce_player_pos();
    if (pp) (void)senro_popup(pp, value, color);
}

/* 把纯逻辑累积的事件变成表现 + 日志（每次入账之后与每帧各调一次；事件取走即清）。*/
static void senro_handle_events(ce_card_t *c, senro_card_t *s, int ev)
{
    (void)c;
    if (!ev) return;
    if (ev & SENRO_EV_BORDER_OPEN) {
        ce_play_sound(SENRO_SE_BORDER_OPEN, ce_player_x());
        ce_log("senro: 森罗结界 OPEN (%d frames, cherry %d/%d, +C %d)",
               (int)SENRO_BORDER_FRAMES, (int)s->st.cherry, (int)s->st.cherry_max, (int)s->st.plus);
    }
    if (ev & SENRO_EV_BORDER_QUEUE)
        ce_log("senro: +C 满但正在放炸弹 → 排队（th07 [player+0x240d]=2 口径）");
    if (ev & SENRO_EV_END_NATURAL) {
        /* 结算表现照一手（`th07:0x441670`）：分数 += (樱点 − 基准)、Border Bonus 飘字、SE。*/
        ce_add_score_delta(s->st.last_bonus);
        ce_play_sound(SENRO_SE_BORDER_END, ce_player_x());
        senro_popup_at_player(s->st.last_bonus, SENRO_HUD_COLOR_GOLD);
        ce_log("senro: 结界自然结束 → 樱点 %d / 最大樱点 %d，Border Bonus +%d",
               (int)s->st.cherry, (int)s->st.cherry_max, (int)s->st.last_bonus);
    }
    if (ev & SENRO_EV_CHERRY_MAXED) {
        if (s->frame - s->maxed_popup_at >= SENRO_MAXED_POPUP_GAP) {
            s->maxed_popup_at = s->frame;
            senro_popup_at_player(s->st.cherry, SENRO_HUD_COLOR_GOLD);
        }
    }
}

/* ---- 计分层（EVENTS-CONTRACT §3：声明 → 聚合 → id 最小者落笔）----
 * 本卡声明 **自己的「最大得点」= 蓝点分值**（`th07:0x4330db`–`0x4331e5` 的一手公式）；
 * 落笔时读**聚合值**（Σ 各卡的「最大得点」载体，不读任何别的卡的状态），写成 `*value = PV × 10`
 * ⇒ 引擎随后 `SCORE += value / 10` ⇒ 精确得到 PV 分、天然不双计分（review §7.8）。
 * ⚠ 现行 `sdk.c` 的 `BP_ce_item_score`（0x446cf6）还是「逐个写指针」的旧形态：它沿卡链调每张卡的
 *   `on_item_score` 并最后写 `regs->esi`。所以落笔者在这里**直接写 `*value`**（今天就能生效）；
 *   层改成「聚合 + 单一落笔」之后这一行仍然正确（落笔者本来就该写它），**不需要**跟着改。 */
static void senro_declare_score(senro_card_t *s)
{
    if (!s) return;
    ce_score_declare(SENRO_ID, senro_point_item_value(&s->st));
}

static void on_item_score(ce_card_t *c, int32_t *value)
{
    senro_card_t *s = st(c);
    int32_t pv, agg;

    if (!s || !value) return;
    if (ce_score_applier() != (int)SENRO_ID) return;      /* 不是落笔者 ⇒ 一个字都不动（契约）*/
    agg = ce_score_aggregate();
    pv = (agg > 0) ? agg : senro_point_item_value(&s->st);
    if (pv < 10) pv = 10;                                /* 引擎自己的身价钳 ≥ 10（review §7.8）*/
    *value = pv * 10;                                    /* 引擎随后 SCORE += value/10 */
}

/* ---- ③ 结界中：自动回收道具（用户口径 + `th07:0x4414xx` 段的结界行为）----
 * 用 SDK 的共享件 `ce_item_collect_all()`（把池里所有活道具 state 置 3 = 真空飞行 ⇒ 引擎原生收点：
 * 计分 / HUD / 计数 / 磁吸全走原生）。⚠ 只动 state、不动 pos/vel ⇒ 视觉上道具朝自机飞。 */
static void senro_border_collect(senro_card_t *s)
{
    int n = ce_item_collect_all();
    if (n) {
        s->stat_collect += n;
        if (s->frame - s->collect_log_at >= SENRO_LOG_EVERY) {
            s->collect_log_at = s->frame;
            ce_log("senro: 结界全收 %d 个道具（累计 %d）", n, (int)s->stat_collect);
        }
    }
}

/* ---- 灵击（中弹 / 按 B）：结束结界 + 全屏消弹转小樱点 + 0.67 s 无敌 ----
 * 执行体 = th07 的 `0x441bd0`（中弹入口 `0x440d1d`、按 B 入口 `0x4419d2`）。
 *
 * ⚠ 全屏消弹走**「本帧已消过弹」闩锁**（EVENTS-CONTRACT §5.2 细则 2：107 的结界灵击与 117 的霊撃
 *   同帧都触发时只有第一家动手）。117 的消弹发生在霊撃的**后续帧**（`animal.c` 的 `hyper_frame`
 *   每帧 `ce_cancel_radius`），`+0x10` 那一帧不 claim ⇒ 「107 拿这笔账」在现况下天然成立；
 *   若将来有卡在 `+0x10` 就消弹，就需要 SDK 提供层内顺序（已登记为跨卡缺口）。
 *
 * ★★ `by_hit` 那一路**不能在死亡栈里扫弹池**（本卡 spec §6.3 的一手依据）：
 *   中弹的调用链是「弹自己的更新函数 `FUN_004248a0` → `0x42490b`/`0x42493f` call `0x45cc20`
 *   （自机受击）→ `Player__die` `0x45d3a0` → `0x45d574 call [vt+0x10]`」——
 *   也就是说我们的 `+0x10` 回调**跑在某颗弹的更新中途**（`esi` = 那颗弹，回调返回后同函数还在用它）。
 *   在这里 `ce_cancel_radius` 会把整池弹标成已消（含当前那颗）⇒ 与调用方的栈帧抢同一块状态。
 *   ⇒ **死亡路只做「不收尾就会丢」的事**（结束结界 / 40 帧无敌 / `cancel_impending_death` 救命），
 *   把全屏消弹+小樱点入账推迟到**下一帧的 `on_tick_2`**（管理器 tick，非弹池循环内，1 帧 ≈ 16 ms，
 *   玩家不可能察觉；`mof.c` / `pofv.c` / `animal.c` 的消弹也都只在 tick 里做）。
 *   实跑要点见本卡 spec §9 第 3 条。 */
static void senro_do_libyou(ce_card_t *c, senro_card_t *s, int by_hit)
{
    const float *pp = ce_player_pos();
    float pos[3];
    int n = 0;

    if (!senro_border_active(&s->st)) return;             /* 没结界：不动作（按 B 照常放原生炸弹）*/
    if (!pp) return;
    pos[0] = pp[0]; pos[1] = pp[1]; pos[2] = pp[2];

    if (by_hit) {
        /* 中弹路：当场收尾（不算弹数），消弹交给下一帧 `senro_tick_2` 的 pending 分支。 */
        (void)senro_libyou(&s->st, 0);
        s->hit_cancel_pending = 1;
    } else {
        /* 按 B 路：tick 上下文，直接扫弹池（先例 = mof.c 的灵击 / animal.c 的霊撃）。 */
        if (ce_cancel_latch_claim(SENRO_ID)) {
            n = ce_cancel_radius(pos, SENRO_CANCEL_R, SENRO_CANCEL_MAX, SENRO_CANCEL_MODE);
        } else {
            ce_log("senro: 本帧消弹已被别的卡认领 → 灵击只收结界（EVENTS-CONTRACT §5.2 细则 2）");
        }
        (void)senro_libyou(&s->st, n);
    }

    ce_invuln_request(SENRO_ID, s->st.invuln);
    ce_invuln_apply();                                   /* 中弹路发生在 Player tick，晚于 SDK 的落笔点 ⇒ 当场补一笔 */
    senro_handle_events(c, s, senro_take_events(&s->st));
    ce_play_sound(SENRO_SE_LIBYOU, pp[0]);
    ce_screen_shake(SENRO_SHAKE_TIME, SENRO_SHAKE_START, SENRO_SHAKE_END);
    if (by_hit) senro_cancel_impending_death();           /* 救援：取消本次死亡（EVENTS-CONTRACT §4.2 ②层）*/
    ce_log("senro: 灵击（%s）消弹 %d 颗 → 樱点 %d / 最大樱点 %d，无敌 %d 帧",
           by_hit ? "中弹" : "按 B", n, (int)s->st.cherry, (int)s->st.cherry_max,
           (int)SENRO_INVULN_FRAMES);
}

/* 中弹那一笔的**下一帧补做**：全屏消弹 + 小樱点入账（理由见 `senro_do_libyou` 的长注）。*/
static void senro_hit_cancel_flush(ce_card_t *c, senro_card_t *s)
{
    const float *pp = ce_player_pos();
    float pos[3];
    int n = 0;
    int32_t add;

    if (!s->hit_cancel_pending) return;
    s->hit_cancel_pending = 0;
    if (!pp) return;
    pos[0] = pp[0]; pos[1] = pp[1]; pos[2] = pp[2];
    if (ce_cancel_latch_claim(SENRO_ID)) {
        n = ce_cancel_radius(pos, SENRO_CANCEL_R, SENRO_CANCEL_MAX, SENRO_CANCEL_MODE);
    } else {
        ce_log("senro: 中弹灵击的补做消弹被别的卡认领（EVENTS-CONTRACT §5.2 细则 2 的层内顺序缺口）");
        return;
    }
    add = senro_small_cherry(&s->st, n);                  /* 小樱点只计进 stat_cancel，不混进道具通道 */
    senro_handle_events(c, s, senro_take_events(&s->st));
    ce_log("senro: 中弹灵击补做消弹 %d 颗 → 小樱点 +%d（樱点 %d / +C %d）",
           n, (int)add, (int)s->st.cherry, (int)s->st.plus);
}

/* ---- 每帧（管理器侧；菜单 / 商店里不跑）---- */
static int on_tick_2(ce_card_t *c)
{
    senro_card_t *s = st(c);
    int32_t g, dg;
    int spell_now;

    if (!s || !CE_PLAYER()) return 0;
    ++s->frame;

    /* ⓿ 中弹灵击的补做（上一帧在死亡栈里记下的）——必须在 tick 之前，消弹与入账同一帧完成。 */
    senro_hit_cancel_flush(c, s);

    /* ① 无敌声明：窗口内每帧声明**剩余**帧数（层取最大值），窗口外退掉声明。 */
    if (s->st.invuln > 0) {
        ce_invuln_request(SENRO_ID, s->st.invuln);
        ce_invuln_apply();
        --s->st.invuln;
    } else {
        ce_invuln_request(SENRO_ID, 0);
    }

    /* ② 结界计时 / +C 比例 / 自然结束。对话中冻结（th07 的结界更新挂在 Player tick 上，
     *    对话里引擎同样不推进游玩；口径见本卡 spec §6.1）。 */
    if (!CE_GUI_MSG_ACTIVE()) {
        senro_handle_events(c, s, senro_tick(&s->st, CE_BOMB_ACTIVE() ? 1 : 0));
        if (senro_border_active(&s->st)) senro_border_collect(s);   /* ③ 结界中自动回收道具 */
    }

    /* ④ 擦弹差分 → 结界中同时涨樱点与最大樱点（th07:0x43ed5f–0x43eda9，高速 80 / 低速 30）。
     *    差分读原生擦弹计数（先例：`cards/lolk.c` / `cards/season.c` / `cards/mof.c` 都这么做；
     *    review §9.7 结论 2 已确认 107 **不需要**逐弹擦弹断点）。 */
    g = senro_graze_now();
    if (s->st.graze_last < 0 || g < s->st.graze_last) s->st.graze_last = g;
    dg = g - s->st.graze_last;
    s->st.graze_last = g;
    if (dg > 0) {
        int32_t add = senro_graze(&s->st, dg, CE_PLAYER_FOCUSED() ? 1 : 0);
        if (add) {
            s->stat_gain_graze += add;
            senro_handle_events(c, s, senro_take_events(&s->st));
        }
    }

    /* ⑤ 按 B（X 上升沿）⇒ 灵击（th07 的按 B 入口 `0x4419d2`）。原生炸弹照放，本卡只做结界那一半。 */
    if ((*(const uint32_t *)CE_ADDR_INPUT_PRESSED & SENRO_INPUT_BOMB) && senro_border_active(&s->st))
        senro_do_libyou(c, s, /*by_hit=*/0);

    /* ⑥ 符卡奖励次数（th07 樱点（大）道具那项的 `[globals+0x1c]`）：
     *    照 `cards/mof.c` 的符卡跟踪写法 —— 奖励位（`CE_SPELL_FLAG_BONUS`，engine.h 一手：
     *    「bit1 奖励存活」）在符卡结束时还活着 = 收掉了这张符卡 ⇒ 记一次。 */
    spell_now = ce_spell_active();
    if (spell_now && !s->spell_was) s->spell_bonus_alive = 0;
    if (spell_now && (ce_spell_flags() & CE_SPELL_FLAG_BONUS)) s->spell_bonus_alive = 1;
    if (!spell_now && s->spell_was && s->spell_bonus_alive) {
        ++s->st.spell_bonuses;
        ce_log("senro: 符卡奖励第 %d 次（樱点（大）道具从此 +%d/颗）",
               (int)s->st.spell_bonuses, (int)(SENRO_ITEM_BIG_BASE + SENRO_ITEM_BIG_PER_BONUS * s->st.spell_bonuses));
    }
    s->spell_was = spell_now;

    /* ⑦ 计分声明（粘性 ⇒ 每帧刷新一次即可；樱点变了当帧就生效）。 */
    senro_declare_score(s);

    /* ⑧ 诊断汇总（实跑对账用）。 */
    if (s->frame - s->log_frame >= SENRO_LOG_EVERY) {
        s->log_frame = s->frame;
        ce_log("senro: cherry %d/%d, +C %d, border %s%s (%d 帧剩余), 开 %d 自然结 %d 灵击 %d 消弹 %d 道具 +%d 擦弹 +%d 命中 +%d",
               (int)s->st.cherry, (int)s->st.cherry_max, (int)s->st.plus,
               s->st.active ? "on" : "off", s->st.queued ? " (queued)" : "",
               (int)s->st.timer, (int)s->st.stat_open, (int)s->st.stat_natural, (int)s->st.stat_libyou,
               (int)s->st.stat_cancel, (int)s->stat_gain_item, (int)s->stat_gain_graze,
               (int)s->stat_gain_hit);
    }
    return 0;
}

/* ---- 道具收集（TH18 item type；映射表见本卡 spec §4.1）---- */
static void on_item_collect(ce_card_t *c, uint32_t type, const void *item)
{
    senro_card_t *s = st(c);
    int32_t d;

    (void)item;
    if (!s) return;
    d = senro_item_cherry(&s->st, type);
    if (!d) return;
    s->stat_gain_item += d;
    senro_handle_events(c, s, senro_take_events(&s->st));
}

/* ---- 自机弹计分命中（断点 0x45f2a2）----
 * ★ 2026-09-12：SDK 补了 `on_shot_damage_any`（`public.h` 的 `ce_hooks_t` 末尾；同一个断点 0x45f2a2、
 *   同一份实参，**不按聚焦过滤**，`focused` 由 SDK 传）⇒ 本卡的**两条通道都在它上面分流**：
 *     高速（!focused）与低速（focused）各走一次 `senro_hit`，「低速 ≈ 高速的 1/3」那半支不再是死路。
 * ⚠ 两个钩子**会各收一次**（public.h 明写）⇒ 这里**只**声明 `on_shot_damage_any` 并从
 *   `CE_CARD` 的挂表里撤掉 `on_shot_damage`，`on_shot_damage` 这个函数体**不再存在** ——
 *   从构造上排除「同一帧同一敌记两次账」。
 * ⚠ 数值语义与迁移前逐字相同：迁移前高速那半支走老钩子、`slow = CE_PLAYER_FOCUSED() ? 1 : 0`
 *   ——而老钩子的策略门恰好保证「到得了它 ⇒ !focused」⇒ 老路上 `slow` 恒 0。现在高速支仍传 0、
 *   低速支传 1（= `SENRO_HIT_SLOW_NUM/SENRO_HIT_FAST_NUM` 那对常量，senro_core.c 未改一个字）。
 * ⚠ 只读 `focused` 这一个入参、不读引擎输入位（回放安全）；bomb 生效帧两个钩子都不发（SDK 边界）。 */
static void on_shot_damage_any(ce_card_t *c, int32_t damage, const float *pos, int focused)
{
    senro_card_t *s = st(c);
    int32_t add;

    (void)pos;
    if (!s) return;
    add = senro_hit(&s->st, damage, focused ? 1 : 0);   /* 1 = 低速（聚焦）⇒ 走慢支；0 = 高速 */
    if (!add) return;
    s->stat_gain_hit += add;
    senro_handle_events(c, s, senro_take_events(&s->st));
}

/* ---- Bomb 用掉（断点 0x4203bc，`consume_bomb` 之后）⇒ 扣樱点（th07:0x440ad9）---- */
static void on_bomb_spent(ce_card_t *c)
{
    senro_card_t *s = st(c);
    int32_t d;

    if (!s) return;
    d = senro_penalty_bomb(&s->st);
    senro_declare_score(s);
    ce_log("senro: Bomb −樱点 %d → %d（+C 与最大樱点不动）", (int)d, (int)s->st.cherry);
}

/* ---- 中弹（虚表 +0x10：死亡已发起、可取消）----
 * 用户 2026-09-12 裁决（EVENTS-CONTRACT §4.1 / §5.2）：森罗结界属**救援层**，
 * 中弹 = 消耗结界救命；一次中弹把当前所有救援型提供者都烧掉（107 结束结界、117 结束咆哮），
 * 动作顺序 107 → 117。本卡只负责自己那一半（117 在自己的 +0x10 里做它那一半）。 */
static int on_death_before_deathbomb(ce_card_t *c)
{
    senro_card_t *s = st(c);
    if (!s) return 0;
    if (!senro_border_active(&s->st)) return 0;
    /* ★ 救援层报到（用户 2026-09-12 定案：顺序 107 → 117 → 决死 B → 113）。
     * ⚠️⚠️ **返回值必须丢弃**（`(void)` 不是手滑）：用户 2026-09-12 重申
     *   「**107 和 117 同时触发的时候，得要 107 和 117 同时破除**」⇒ 两家都必须**无条件**破自己的资源，
     *   谁也不能靠「先 claim 到账」把另一家挡掉。
     * 这个闩锁的**唯一消费者是 113**（它是最后一家，只在 `ce_rescue_note_owner() < 0` 时才动手）。
     * ⇒ 谁把这里改成 `if (ce_rescue_note_claim(...))`，就是让先到的那家独占救援 = 违规。*/
    (void)ce_rescue_note_claim(SENRO_ID);
    senro_do_libyou(c, s, /*by_hit=*/1);
    return 0;   /* 返回值被 Player__die 忽略；救命靠 cancel_impending_death 的副作用 */
}

/* ---- 真死（虚表 +0x14，死亡结算之后）⇒ Miss 扣樱点（th07:0x440e84–0x440ef0）----
 * 走到这里说明灵击没能救（结界本来就没开）⇒ Miss 与 Bomb 的扣减各走各的。 */
static int on_death_frame2(ce_card_t *c)
{
    senro_card_t *s = st(c);
    int32_t d;

    if (!s) return 0;
    d = senro_penalty_miss(&s->st);
    senro_declare_score(s);
    ce_log("senro: Miss −樱点 %d → %d（+C 与最大樱点不动；th07 的 miss 链不夹回基准）",
           (int)d, (int)s->st.cherry);
    return 0;
}

/* ---- HUD（左下角三行：+C / 樱点 / 最大樱点）----
 * 用**贴图 VM 字形**（`ce_hud_vm_text_set`：TH18 font 2 的零售切图，走 UI 层 25 ⇒ 暂停时被模糊罩盖住；
 * 先例 = `cards/season.c` 的档位数字 / `cards/mof.c` 的三行）。坐标是 640×480 字面量空间，
 * 由 HUD 槽位分配器给的 `x640/y640` 推（HUD-LAYOUT.md §1.1 的两套空间）。
 * ★ 坐标**每帧取最新值**（2026-09-12 审计补）：`ce_hud_slot_claim()` 填给 `out` 的是**认领那一刻**的
 *   排布，同一开场批次里后来的认领会按 card_id 重排（`sdk_core.c` 的 `hud_relayout`；`sdk_core.h:180`
 *   原话「卡片应…用本函数取最新值，或每帧取一次」）⇒ 只认领时取一次、之后一直用缓存的那份，会让
 *   三行画到**别人的块**上（比本卡 id 小的卡后认领时本卡的 x 会右移）。111 一直是每帧取的，本卡原来漏了。 */
static int on_draw(ce_card_t *c)
{
    senro_card_t *s = st(c);
    char buf[32];
    uint32_t a, col_plus, col_cherry;
    ce_hud_slot_t cur;
    float x, y;

    /* ★★ 2026-09-12 对抗审计补的门：`on_draw`（虚表 +0x48）**在主菜单 / 标题里也会被调**
     *   （`AbilityManager__on_draw` `0x408ab0` 每帧遍历卡链表调它；`on_tick_2` 才是「菜单/商店不跑」）。
     *   实跑先例：117 `animal.c:1953`「用户在**主菜单**看到 5 个空槽灵魂」、115 `lolk.c:708` 主菜单冒出 `+0`。
     *   本卡原来的第一道门是 `if (!s || !s->slot_ok)` —— 而 `slot_ok` 一旦在 `on_load`/`on_run_reset` 置上
     *   就再也不会清 ⇒ **通关回主菜单后三行会照画**（同 lolk 那个洞）。⇒ 补 `CE_ADDR_GAME_THREAD_PTR`
     *   那道门（与 `lolk.c:708` / `season.c:1347` / `th06rank.c` 同款）。**顺手把三行 VM 收掉**
     *   （`ce_hud_vm_text_clear` 只 delete 已有的 VM，无 VM 时是空操作）—— 不收的话会留着上一帧的字。
     *   ⚠ 只在「不是正常游玩帧」时收：正常帧每帧都会 `ce_hud_vm_text_set` 重画，不必每帧 delete+spawn。 */
    if (!*(volatile uint32_t *)CE_ADDR_GAME_THREAD_PTR) {
        ce_hud_vm_text_clear(&s_hud_plus);
        ce_hud_vm_text_clear(&s_hud_cherry);
        ce_hud_vm_text_clear(&s_hud_max);
        return 0;
    }
    if (!s || !s->slot_ok) return 0;
    a = ce_hud_player_near_corner(ce_player_x(), CE_PLAYER() ? *(float *)(CE_PLAYER() + CE_PLAYER_Y) : 0.0f)
            ? SENRO_HUD_DIM : 0xffu;
    /* ⚠ `ce_hud_slot_get()` **只填 x/y/w/h/side，不填 x640/y640**（`sdk_core.c`）⇒ 拿不到时退回认领时的
     * 缓存副本，换算一律走 `CE_HUD_SURFACE_TO_640`（= 0.5），不读那两格。*/
    cur = s->slot;
    (void)ce_hud_slot_get(SENRO_ID, &cur);
    x = cur.x * CE_HUD_SURFACE_TO_640;
    y = cur.y * CE_HUD_SURFACE_TO_640;
    col_plus = (s->st.plus >= SENRO_PLUS_FULL) ? SENRO_HUD_COLOR_PLUS_HOT : SENRO_HUD_COLOR_PLUS;
    /* 樱点满最大樱点 ⇒ 金黄（用户口径；th07 的常量未取到，见常量注）。 */
    col_cherry = (s->st.cherry >= s->st.cherry_max) ? SENRO_HUD_COLOR_GOLD : SENRO_HUD_COLOR_CHERRY;

    snprintf(buf, sizeof buf, "+%d", (int)s->st.plus);
    ce_hud_vm_text_set(&s_hud_plus, x, y - 2.0f * SENRO_HUD_ROW_DY,
                       (col_plus & 0x00ffffffu) | (a << 24), CE_ASCII_ALIGN_LEFT, 0.0f, buf, "senro");
    snprintf(buf, sizeof buf, "%d", (int)s->st.cherry);
    ce_hud_vm_text_set(&s_hud_cherry, x, y - SENRO_HUD_ROW_DY,
                       (col_cherry & 0x00ffffffu) | (a << 24), CE_ASCII_ALIGN_LEFT, 0.0f, buf, "senro");
    snprintf(buf, sizeof buf, "%d", (int)s->st.cherry_max);
    ce_hud_vm_text_set(&s_hud_max, x, y,
                       (SENRO_HUD_COLOR_CHERRY & 0x00ffffffu) | (a << 24), CE_ASCII_ALIGN_LEFT, 0.0f, buf, "senro");
    return 0;
}

/* ---- 生命周期 ---- */

static void senro_clear(ce_card_t *c, int keep_slot)
{
    senro_card_t *s = st(c);

    /* ★ 顺序要紧：先退出两个声明层（否则被删的卡还在声明无敌 / 得分），再收 HUD 文字。 */
    ce_invuln_request(SENRO_ID, 0);
    ce_score_declare(SENRO_ID, 0);
    ce_hud_vm_text_clear(&s_hud_plus);
    ce_hud_vm_text_clear(&s_hud_cherry);
    ce_hud_vm_text_clear(&s_hud_max);
    if (!s) return;
    memset(s, 0, sizeof *s);
    s->st.graze_last = -1;
    s->spell_was = 0;
    (void)keep_slot;         /* 槽位由 `CE_CARD` 的 on_run_reset / opdelete 自动 release（§6.1）*/
}

static int on_ctor(ce_card_t *c)
{
    senro_card_t *s = st(c);
    if (!s) return 0;
    senro_clear(c, 0);
    senro_init(&s->st, senro_difficulty());
    senro_declare_score(s);
    ce_log("senro: ctor (difficulty %d, cherry_max 初值 %d, 结界 %d 帧, 满槽 %d)",
           senro_difficulty(), (int)s->st.cherry_max, (int)SENRO_BORDER_FRAMES, (int)SENRO_PLUS_FULL);
    return 0;
}

/* HUD 槽认领（`on_load` 与 `on_run_reset` 共用 —— 后者刚 memset 掉整块状态，
 * 而 `on_draw` 要靠 `slot.x640/y640` 定位，不重认领就会把三行画到屏幕左上角）。*/
static void senro_claim_hud(senro_card_t *s)
{
    ce_hud_block_t blk;

    blk.w = SENRO_HUD_W;
    blk.h = SENRO_HUD_H;
    blk.side = 0;                                              /* 左下角（HUD-LAYOUT §2 的货架）*/
    s->slot_ok = ce_hud_slot_claim_card(SENRO_ID, &blk, &s->slot);   /* 幂等：每关都会再调一次（§4.4）。
     * ★ 用 `_card` 入口：它会先把关卡号推进纪元 ⇒ 上一关被删卡留下的占位在**本关开场**收敛（HUD-LAYOUT §4.4）。*/
}

static int on_load(ce_card_t *c)
{
    senro_card_t *s = st(c);

    if (!s) return 0;
    senro_claim_hud(s);
    s->st.graze_last = -1;
    senro_declare_score(s);
    ce_log("senro: on_load (hud slot %s @(%.0f,%.0f) 表面px)", s->slot_ok ? "ok" : "FAIL",
           (double)s->slot.x, (double)s->slot.y);
    return 0;
}

static void on_stage_start(ce_card_t *c)
{
    senro_card_t *s = st(c);
    if (!s) return;
    /* th07 每关重设基准（`th07:0x4043b9` 樱点 = 基准；最大樱点按难度表重取）⇒ 本卡每关从 0 起。 */
    senro_stage_reset(&s->st, senro_difficulty());
    s->frame = 0;
    s->log_frame = 0;
    s->maxed_popup_at = 0;
    s->spell_was = 0;
    s->spell_bonus_alive = 0;
    s->stat_collect = s->stat_gain_item = s->stat_gain_graze = s->stat_gain_hit = 0;
    senro_declare_score(s);
}

static void on_run_reset(ce_card_t *c)
{
    senro_card_t *s = st(c);
    ce_log("senro: run reset");
    senro_clear(c, 0);
    if (s) {
        senro_init(&s->st, senro_difficulty());
        senro_claim_hud(s);                          /* 见 senro_claim_hud 的注：状态被 memset 过，必须重认领 */
    }
}

/* ⚠ 删对象（局末 recount / 局中倒吊人 / 原版空白卡）：SDK 先调它、再释放私有状态。
 *   那一刻 AbilityManager 冻住 ⇒ 只有这里能收 HUD 文字与两个声明层（`CE_CARD` 的 opdelete 会
 *   顺手 `ce_layer_drop_card`，但文字 VM 是本卡自己的）。 */
static void on_delete(ce_card_t *c)
{
    ce_log("senro: on_delete（收 HUD 文字 + 退出无敌 / 计分声明）");
    senro_clear(c, 0);
}

CE_CARD(107,
        .ctor = on_ctor,
        .on_load = on_load,
        .on_stage_start = on_stage_start,
        .on_tick_2 = on_tick_2,
        .on_item_collect = on_item_collect,
        .on_item_score = on_item_score,
        .on_shot_damage_any = on_shot_damage_any,
        .on_bomb_spent = on_bomb_spent,
        .on_death_before_deathbomb = on_death_before_deathbomb,
        .on_death_frame2 = on_death_frame2,
        .on_draw = on_draw,
        .on_run_reset = on_run_reset,
        .on_delete = on_delete);
