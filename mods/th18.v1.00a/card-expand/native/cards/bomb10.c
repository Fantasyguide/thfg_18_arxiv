/* bomb10.c —— 110 型灵击的**唯一一份实现**（110 / 111 共用）。设计与一手出处见 `bomb10.h`。
 *
 * 本文件只做三件事：① 按顺序调 `mof_core.*` 的纯函数；② 调引擎原语（VM / 消弹 / 伤害源 / 音效 / RANK）；
 * ③ 在固定的几个点上回调卡侧那一半（`bomb10_host_t`）。**它不认识任何一张卡**。
 *
 * ⚠ 逐帧顺序是**照 110（TH10 一手）那一条**排的，别随手改（两处顺序有一手依据）：
 *   · 中心先上移 → 再用本帧半径消弹 → 再撒伤害源（一手 `th10:0x4057b6`–`0x405816` 的顺序）；
 *   · `elapsed` 在「伤害之后、环之前」自增 ⇒ 环用的是**推进后**的帧号（110 的写法）。
 *   111 原来是 `ring_frames` 计数、帧号在环之后自增 ⇒ 环的淡出时点差 ≤1 帧（纯视觉，登记在 AUDIT §BE-EY）。*/
#include <string.h>
#include "sdk.h"
#include "bomb10.h"

/* ---- 一发灵击的全部状态（**进程里只有这一份**：bomb10.h 的硬语义 ①）---- */
typedef struct {
    int32_t  left;         /* 还剩几帧（0 = 没在灵击）*/
    int32_t  elapsed;      /* 已经过几帧（半径曲线的自变量）*/
    int32_t  frames;       /* 诊断：本发实际走了几帧 */
    int32_t  dmg_frames;   /* 诊断：本发实际撒出伤害源的帧数（说明伤害档没被 `/5` 归零）*/
    int32_t  spell_frames; /* 符卡已过帧数（对位一手 `[spellcard+0x3738]`；≥ 60 = `big_bomb` 那一档）*/
    float    x, y;         /* 消弹/伤害中心（x = 发动瞬间的自机 x，y 每帧上移）*/
    int32_t  target_centi; /* 本发的目标半径（1/100 px）：符卡 6400 / 灵梦 16000 / 其他 11200 */
    int32_t  rise_milli;   /* 每帧上移量（1/1000 px）：0.5 / 1.0 / 1.3 */
    int32_t  reimu;        /* 发动瞬间的 `CHARACTER == 0`（伤害表的圈外档分灵梦/魔理沙）*/
    int32_t  by_hit;       /* 形态：0 = 主动（按 X / C），1 = 决死（救命）—— 一手 `mof_core.h` 的块 */
    int32_t  owner_id;     /* 本发的发动者卡号（诊断：两张同装时看得出是谁发的）*/
    const bomb10_host_t *host;  /* 本发的发动者（卡侧那一半的回调都走它）*/
    void    *user;              /* 本发的发动者的卡私有状态（回调第一参）*/
    uint32_t ring_vm;      /* 范围环的 VM id（脚本 279；0 = 没起 / 已收）*/
    uint32_t strip_vm;     /* 电流边竖条环的 VM id（脚本 293；0 = 没起 / 已收）*/
    int32_t  ring_fading;  /* 环已发过淡出中断（不再写 scale，只在兜底帧数上硬删）*/
    int32_t  spark_tick;   /* 火星出生节流计数（每 `BOMB10_SPARK_EVERY` 帧一红一蓝）*/
    int32_t  invuln_left;  /* ★ 2026-09-22：本发**还剩几帧无敌**（一手 270 / 决死 200；
                            *   见 `bomb10.h` 的「无敌帧」段）。机器每帧声明它、数到 0 声明 0。*/
} bomb10_run_t;

static bomb10_run_t s_run;
/* ★ 安全网（2026-09-21c，本次自己踩过的坑）：两卡的 `driver` 判据若写成「另一张在场就让位」，
 * 两张同装时**谁都不推** ⇒ 这一发永远停在第一帧（半径 16 / 不消弹 / 不掉血），实跑表现为
 * 「放了灵击没反应」。⇒ 记「连续让位次数」，一发**一次都没被推过**却连着让位 ≥`BOMB10_IDLE_WARN`
 * 次就打一行醒目日志（**只打日志、不改行为** —— 正常情形下每个让位后面紧跟驱动者的那一次调用，
 * 计数当帧就归零，绝不会误报）。*/
static int32_t s_idle_calls;
#define BOMB10_IDLE_WARN 5

/* ---- 状态查询 ---- */
int32_t bomb10_busy(void)         { return s_run.left > 0; }
int32_t bomb10_left(void)         { return s_run.left; }
int32_t bomb10_target_centi(void) { return s_run.target_centi; }
int32_t bomb10_spell_frames(void) { return s_run.spell_frames; }
int32_t bomb10_owner_id(void)     { return s_run.owner_id; }
int32_t bomb10_invuln_left(void)  { return s_run.invuln_left; }

/* 救命：`th18:0x45d590`（thiscall(Player*)、无栈参、裸 `ret`）。一手逐条见 `mof_core.h` 的
 * 「灵击 → 决死」块与 `signal_core.h` 的同名段（两个文件都在引用它）。*/
void bomb10_cancel_impending_death(void)
{
    void *p = CE_PLAYER();
    if (!p) return;
    ((void (__attribute__((thiscall)) *)(void *))(uintptr_t)BOMB10_FN_CANCEL_IMPENDING_DEATH)(p);
}

/* ---- 视觉：收干净（脚本 279/293 **自己不会死**，`stop()` 之后只画 ⇒ 必须显式删）---- */
static void bomb10_fx_kill(void)
{
    if (s_run.ring_vm) {
        ce_anm_delete(s_run.ring_vm);
        s_run.ring_vm = 0;
    }
    if (s_run.strip_vm) {
        ce_anm_delete(s_run.strip_vm);
        s_run.strip_vm = 0;
    }
}

/* 诊断一行：把环 VM 的关键字段打出来（用户两次报「起了 VM 却看不见」⇒ 一眼分辨
 * 「字段不对（我们的 bug）」还是「字段都对但不画（图元/层的问题）」）。对照物 = `signal/opt:` 那行。*/
static void bomb10_fx_dump(const char *tag)
{
    uint8_t *vm = s_run.ring_vm ? ce_anm_get_vm(s_run.ring_vm) : 0;

    if (!vm) {
        ce_log("%s/fx: %s ring vm=%u **alive=0**（VM 已经不在了）", s_run.host ? s_run.host->log_tag : "bomb10",
               tag, (unsigned)s_run.ring_vm);
        return;
    }
    ce_log("%s/fx: %s ring vm=%u sprite=%d scale=(%.2f,%.2f) pos=(%.1f,%.1f) c1=%08x c2=%08x frames=%d",
           s_run.host ? s_run.host->log_tag : "bomb10", tag, (unsigned)s_run.ring_vm,
           (int)*(const int32_t *)(vm + CE_ANM_VM_SPRITE_ID),
           (double)((const float *)(vm + CE_ANM_VM_SCALE))[0],
           (double)((const float *)(vm + CE_ANM_VM_SCALE))[1],
           (double)((const float *)(vm + CE_ANM_VM_POS))[0],
           (double)((const float *)(vm + CE_ANM_VM_POS))[1],
           *(const uint32_t *)(vm + CE_ANM_VM_COLOR1),
           *(const uint32_t *)(vm + CE_ANM_VM_COLOR2),
           (int)s_run.frames);
}

/* ======================================================================================
 * 发动
 * ======================================================================================
 * 门序（**与改动前两张卡各自的门序一致**，只是合成了一条）：
 *   ① 进行中 ⇒ 不放        一手 `th10:0x42598d` / `th11:0x4311c7`（= bomb10.h 的硬语义 ②）
 *   ② 卡私有闸（`host->gate`）
 *   ③ 自机指针 / 火力门槛（`host->power_spend`；门槛不过 ⇒ **什么都不做**，卡侧自己打日志）
 * 过了三道门之后，公共那一半与前实现逐条同形（顺序照 110）：
 *   写 `CURRENT_POWER` → `ce_repopulate_options()` → 决死救命 → 记曲线参数 → 环 + 电流边 VM
 *   → 音效 → RANK −0x80 → 卡侧 `on_start`。*/
int32_t bomb10_start(const bomb10_host_t *host, void *user, int32_t by_hit)
{
    const float *pp;
    int32_t power_before, power_after = 0, cost, reimu, spell_now, r;

    if (!host) return 0;
    if (s_run.left > 0) return 0;                       /* ① 一手：灵击进行中 ⇒ 不再发动 */
    if (host->gate && host->gate(user)) return 0;       /* ② 卡私有闸 */

    pp = ce_player_pos();
    power_before = CE_CURRENT_POWER();
    if (!pp || !host->power_spend ||
        !host->power_spend(user, by_hit, power_before, &power_after)) return 0;   /* ③ */
    cost = power_before - power_after;

    /* 自写扣法（原生 `ce_spend_power` 会拒绝、且只能整档扣）—— 两张卡原先都是这一行。*/
    *(int32_t *)CE_ADDR_CURRENT_POWER = power_after;
    ce_repopulate_options();                            /* 子机按新档重建（必须）*/

    spell_now = ce_spell_active();
    reimu = (*(const volatile int32_t *)CE_ADDR_CHARACTER == 0);
    s_run.host         = host;
    s_run.user         = user;
    s_run.owner_id     = (int32_t)host->card_id;
    s_run.by_hit       = by_hit;
    s_run.target_centi = spell_now ? MOF_BOMB_R_SPELL_CENTI
                                   : (reimu ? MOF_BOMB_R_REIMU_CENTI : MOF_BOMB_R_MARISA_CENTI);
    s_run.rise_milli   = mof_core_bomb_rise_milli(spell_now, reimu);
    s_run.reimu        = reimu;
    s_run.x            = pp[0];
    s_run.y            = pp[1];
    s_run.left         = MOF_BOMB_FRAMES;
    s_run.elapsed      = 0;
    s_run.frames       = 0;
    s_run.dmg_frames   = 0;
    s_run.ring_fading  = 0;
    s_run.spark_tick   = 0;
    s_run.ring_vm      = 0;
    s_run.strip_vm     = 0;

    /* 无敌（一手；口径与「为什么不是每帧刷」见 `bomb10.h` 的「无敌帧」段）：
     * 普通灵击 270 帧（`th10:0x4259be push 0x10e`）、决死 200 帧（`th10:0x425c2d push 0xc8`）。
     * 声明层是**粘性**的 ⇒ 这里先声明一次，之后由 `bomb10_tick()` 每帧声明**递减后的剩余值**、数到 0 清。*/
    s_run.invuln_left = (by_hit ? BOMB10_INVULN_FRAMES_DEATHBOMB : BOMB10_INVULN_FRAMES);
    ce_invuln_request(host->card_id, s_run.invuln_left);
    ce_log("%s: 灵击无敌 %d 帧（一手 `th10:%s push 0x%x`；走 SDK 无敌声明层，每帧声明剩余值）",
           host->log_tag, (int)s_run.invuln_left, by_hit ? "0x425c2d" : "0x4259be",
           (unsigned)(by_hit ? 0xc8 : 0x10e));

    /* 决死形态：**救命**（`th18:0x45d590`）—— 位置 = 三道门都过了、火力也扣了之后
     * （门槛不过时上面已经 return ⇒ 不救命，照常死）。两张卡原来都在同一个位置上。*/
    if (by_hit) bomb10_cancel_impending_death();

    /* 范围环（脚本 279；半径逐帧给 px、起点 = TH10 的 16.00）+ 电流边（脚本 293）
     * —— 两点与改动前**逐字相同**（同一脚本号、同一层 13、同一初值）。*/
    r = mof_core_bomb_radius_centi(0, s_run.target_centi);
    s_run.ring_vm = ce_anm_spawn(CE_ABILITY_ANM(), CE_ANM_ABILITY_SCRIPT_BOMB_CLEAR_RING, 13);
    if (s_run.ring_vm) {
        ce_anm_set_pos(s_run.ring_vm, s_run.x, s_run.y, 0.0f);
        ce_anm_set_scale(s_run.ring_vm, (float)r * 0.01f, (float)r * 0.01f);
    }
    s_run.strip_vm = ce_anm_spawn(CE_ABILITY_ANM(), CE_ANM_ABILITY_SCRIPT_MOF_BOMB_RING_STRIP, 13);
    if (s_run.strip_vm) {
        ce_anm_set_pos(s_run.strip_vm, s_run.x, s_run.y, 0.0f);
        ce_anm_set_scale(s_run.strip_vm, (float)r * 0.01f, (float)r * 0.01f);
    }
    ce_log("%s: 灵击范围环起 VM（id %u，脚本 %d，层 13，起点半径 %.2f px）",
           host->log_tag, (unsigned)s_run.ring_vm, (int)CE_ANM_ABILITY_SCRIPT_BOMB_CLEAR_RING,
           (double)((float)r * 0.01f));
    bomb10_fx_dump("起");

    /* 音效（TH10 `se_slash.wav` ↔ TH18 同 wav id 0x2c）*/
    ce_play_sound(BOMB10_SE, pp[0]);

    /* RANK −= 0x80、钳 ±0x400（一手 `th10:0x405987`–`0x4059b8`；⚠ TH18 里是死字段 ⇒ 无实际效果）*/
    {
        int32_t rk = *(volatile int32_t *)BOMB10_RANK_ADDR - BOMB10_RANK_DROP;
        if (rk > BOMB10_RANK_MAX) rk = BOMB10_RANK_MAX;
        if (rk < BOMB10_RANK_MIN) rk = BOMB10_RANK_MIN;
        *(volatile int32_t *)BOMB10_RANK_ADDR = rk;
    }

    if (host->on_start) host->on_start(user, by_hit, cost, power_before, power_after);
    return 1;
}

/* ======================================================================================
 * 每帧推进
 * ======================================================================================
 * `driver` = 本卡是否负责驱动这一帧：**两张卡都调它，但只有一张真的推**。
 * 判据写在调用点（`ce_owned(另一张) ? 0 : 1`）：同装时 110（id 小）推、111 让位。
 * 为什么必须这样：两卡都推 = 一发灵击每帧前进两帧（半径 / 伤害 / 时长全错）。
 * 先例 = 低速子机声明层「id 小者落笔」（108 < 111）、组合 UI「110 优先」。
 * ⚠ 卡侧那一半的回调（`on_cancel`）**永远走本发的发动者**（`s_run.host`），不是驱动者。*/
void bomb10_tick(int32_t driver)
{
    float center[3], r;
    int32_t r_centi, dmg_in, dmg_out, dmg_src, n;
    int32_t boss_alive, spell_now, big_bomb, reimu;
    const bomb10_host_t *host = s_run.host;

    if (!driver) {          /* 本卡让位（另一张 id 更小、由它推）*/
        if (s_run.left > 0 && s_run.frames == 0 && ++s_idle_calls >= BOMB10_IDLE_WARN) {
            s_idle_calls = 0;
            ce_log("bomb10: **这一发连着 %d 帧没人驱动**（owner 卡 %d）—— 两张卡的 `driver` 判据"
                   "是不是都写成了「另一张在场就让位」？（见 bomb10.h 的硬语义 ③）",
                   (int)BOMB10_IDLE_WARN, (int)s_run.owner_id);
        }
        return;
    }
    s_idle_calls = 0;

    /* ★ 2026-09-22 无敌：**每帧声明剩余帧数**（声明层取 max、只写更长值 ⇒ 不会截短别人的无敌）。
     * 一手是「发动瞬间一次性给 270/200」（`Timer::set`），引擎自己逐帧递减 ⇒ 我们这里同步递减并声明，
     * 两者在锁步中只可能**补足**（引擎那侧被消耗掉了才重写），数到 0 就声明 0 把这一格清干净。*/
    if (s_run.invuln_left > 0) {
        --s_run.invuln_left;
        ce_invuln_request((uint32_t)s_run.owner_id, s_run.invuln_left);
    }

    /* 符卡帧计数：**与有没有在灵击无关**（`big_bomb` 要的是「本次攻击已过帧数」）*/
    if (ce_spell_active()) {
        if (s_run.spell_frames < 1000000) ++s_run.spell_frames;
    } else {
        s_run.spell_frames = 0;
    }

    if (s_run.left <= 0) return;
    if (!host) { bomb10_reset(); return; }

    /* 中心先上移，再拿它消弹/放伤害源（= 一手 `th10:0x4057b6`–`0x405816` 的顺序）*/
    s_run.y -= (float)s_run.rise_milli * 0.001f;
    r_centi = mof_core_bomb_radius_centi(s_run.elapsed, s_run.target_centi);
    r = (float)r_centi * 0.01f;
    center[0] = s_run.x;
    center[1] = s_run.y;
    center[2] = 0.0f;

    /* ① 消弹（半径 = 本帧的 r；mode 1 = 引擎自己在每颗弹的位置撒 type 9 消弹点）。
     * 「本帧已消过弹」的闩锁：每帧 claim 一次；一颗都没消就把闩锁交回（原生卡冲突审计那条）——
     * tag = **本发的发动者卡号**（两张卡原来各用自己的号，行为逐条相同）。*/
    if (ce_cancel_latch_claim(host->card_id)) {
        n = ce_cancel_radius(center, r, BOMB10_CANCEL_MAX, BOMB10_CANCEL_MODE);
        if (n > 0) {
            if (host->on_cancel) host->on_cancel(s_run.user, n);
        } else {
            (void)ce_cancel_latch_release(host->card_id);
        }
        if (r > 0.0f && (s_run.elapsed % 30) == 0) {
            ce_log("%s: 灵击消弹圈 r=%.2f 本帧 cancelled %d（mode 1 = 引擎撒消弹点）",
                   host->log_tag, (double)r, (int)n);
        }
    }

    /* ② 伤害：每帧一个新源（圈内档 / 圈外档互斥，用「外圈 = 全场」+「内圈 = 内 − 外」精确表达）。
     * 载体的取舍（为什么是独立伤害源而不是自机弹倍率）写在 `mof_core.h` 与 AUDIT §2.5，别在这里重复推。*/
    boss_alive = 0;
    {
        uint8_t *em = CE_ENEMY_MGR();
        if (em) boss_alive = *(const int32_t *)(em + CE_EM_BOSS_IDS) != 0;
    }
    spell_now = ce_spell_active();
    big_bomb  = spell_now && s_run.spell_frames >= (int32_t)MOF_BOMB_BIG_BOMB_FRAMES;
    reimu     = s_run.reimu;

    dmg_in  = mof_core_bomb_damage(1, boss_alive, spell_now, big_bomb, reimu);
    /* 圈外档只在「有 boss」或「符卡中」时非 0（一手 `th10:0x405a68`–`0x405ab7`）*/
    dmg_out = (boss_alive || spell_now) ? mof_core_bomb_damage(0, boss_alive, spell_now, big_bomb, reimu) : 0;
    if (dmg_in > 0 || dmg_out > 0) ce_raise_damage_cap(BOMB10_DMG_CAP);

    dmg_src = dmg_in - dmg_out;
    if (dmg_src > 0) {
        float side = r * 2.0f;                  /* 外接方形（一手是圆 ⇒ 角落多覆盖，见 AUDIT 的差异表）*/
        if (ce_damage_rect(center, 0.0f, BOMB10_DMG_SRC_LIFE, dmg_src, side, side)) ++s_run.dmg_frames;
    }
    if (dmg_out > 0) {
        float pc[3];
        pc[0] = BOMB10_FIELD_CX; pc[1] = BOMB10_FIELD_CY; pc[2] = center[2];
        if (ce_damage_rect(pc, 0.0f, BOMB10_DMG_SRC_LIFE, dmg_out,
                           BOMB10_FIELD_W, BOMB10_FIELD_H)) ++s_run.dmg_frames;
    }

    ++s_run.elapsed;
    ++s_run.frames;

    /* ③ 范围环：逐帧跟着「本帧的真半径」长；长满那一帧起淡出（= TH10 的 120 帧成长），
     *    淡出后 30 帧还没死就硬删兜底。火星按 `BOMB10_SPARK_EVERY` 帧一红一蓝（脚本 280/281）。*/
    if (s_run.ring_vm) {
        if (!ce_anm_get_vm(s_run.ring_vm)) {
            s_run.ring_vm = 0;                  /* 脚本自己被删了/被打断 ⇒ 清 id */
        } else if (s_run.ring_fading) {
            if (s_run.elapsed > BOMB10_RING_FADE_AT + BOMB10_RING_FADE_EXTRA) {
                ce_anm_delete(s_run.ring_vm);
                s_run.ring_vm = 0;
            }
        } else {
            if (s_run.elapsed >= BOMB10_RING_FADE_AT) {
                ce_anm_interrupt(s_run.ring_vm, 1);          /* 20 帧淡出 → 脚本自删 */
                if (s_run.strip_vm && ce_anm_get_vm(s_run.strip_vm)) {
                    ce_anm_interrupt(s_run.strip_vm, 1);     /* 竖条环一起淡出 */
                }
                s_run.ring_fading = 1;
            }
            ce_anm_set_pos(s_run.ring_vm, center[0], center[1], center[2]);
            (void)ce_anm_set_scale(s_run.ring_vm, r, r);
            if (s_run.strip_vm && ce_anm_get_vm(s_run.strip_vm)) {
                ce_anm_set_pos(s_run.strip_vm, center[0], center[1], center[2]);
                (void)ce_anm_set_scale(s_run.strip_vm, r, r);
            }
            if (s_run.elapsed == BOMB10_RING_FADE_AT + 2) bomb10_fx_dump("长满后");
        }
    }
    if (++s_run.spark_tick >= BOMB10_SPARK_EVERY) {
        uint32_t red  = ce_anm_spawn(CE_ABILITY_ANM(), CE_ANM_ABILITY_SCRIPT_MOF_BOMB_SPARK, 13);
        uint32_t blue = ce_anm_spawn(CE_ABILITY_ANM(), CE_ANM_ABILITY_SCRIPT_MOF_BOMB_SPARK_BLUE, 13);
        s_run.spark_tick = 0;
        if (red)  ce_anm_set_pos(red,  center[0], center[1], 0.0f);
        if (blue) ce_anm_set_pos(blue, center[0], center[1], 0.0f);
    }

    /* ④ 收尾（脚本 279/293 自己不会死 ⇒ 两条环 VM 都要显式删）*/
    if (--s_run.left <= 0) {
        bomb10_fx_dump("结束前");
        bomb10_fx_kill();
        ce_log("%s: 灵击结束（%d 帧，伤害源 %d 帧，半径 %.1f，中心 y %.1f）",
               host->log_tag, (int)s_run.frames, (int)s_run.dmg_frames, (double)r, (double)s_run.y);
        s_run.left = 0;
    }
}

/* ======================================================================================
 * 收干净（删卡 / 局末 / 换关三条路）
 * ======================================================================================
 * 「长持续效果铁律」（`METHOD.md`）：脚本 279/293 自己不会死，删卡又发生在**商店**里
 * （那一刻 AbilityManager 冻住、`bomb10_tick` 不再跑）⇒ 只能在这里收。
 * ⚠ 两张卡**都**有权调它（谁的 on_delete / on_stage_start 先到都算）—— 因为状态是**共用的一份**：
 *   110 被删掉时 111 发的那一发也必须一起收（否则那两支环 VM 永远挂在场上）。
 *   TH10 一手同向：灵击的 anm VM 归**关卡**所有，换关时 VM 被清 ⇒ `Bomb::on_tick` 自己把
 *   `[bomb+0x28]` 置 0（`th10:0x405781`–`0x405786`）。*/
void bomb10_reset(void)
{
    /* ⚠ 顺序：**先用 `s_run.owner_id` 把无敌声明清掉**（声明层是粘性的 ⇒ 不清会一直按最后那个值刷
     * = 永久无敌；卡被删时 `ce_layer_drop_card()` 也会清一次，但换关那条路不清）——再清状态。*/
    if (s_run.owner_id) ce_invuln_request((uint32_t)s_run.owner_id, 0);
    bomb10_fx_kill();
    memset(&s_run, 0, sizeof s_run);
}
