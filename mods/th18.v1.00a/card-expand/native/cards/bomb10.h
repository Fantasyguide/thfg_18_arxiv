/* bomb10.h —— **110 型灵击**（TH10 那一套灵击）的**唯一一份实现**，110 风神录 / 111 地灵殿共用。
 *
 * ## 为什么会有这个文件（owner 口径，逐字）
 *   「**110也得是主动卡，因为只要有110 or 111就有灵击，当然，它们完全走同一套灵击系统**」
 *   ⇒ 两张卡各有一份逐帧状态机是**不能**再留的（那正是「两套系统」）：
 *     ① 同装两卡时按 X（库存 == 0）**两张卡会各发一发**（双倍扣火力 / 双倍伤害 / 双份环 VM）；
 *     ② 改一处必须同步另一处（`TUNING.md` §2.5 那条「第二份状态机」的欠账就此清掉）。
 *   ⇒ 抽成这一个模块：**一份状态、一份逐帧推进、一份发动**；两张卡只交「自己那一半」。
 *
 * ## 结构（照本仓「纯逻辑进 core / 引擎层进共享件」的分工）
 *   · **纯数学那一半不在这里**：半径曲线 / 每帧上移 / 伤害表 / 扣火力门槛全部在 `mof_core.*`
 *     （已有一手出处与主机单测 `test_mof_core`）；本模块只**按顺序**调它们。
 *   · 本模块 = **引擎层**（VM、消弹、伤害源、音效、RANK）—— 与 `cards/faith.c`（绿点实体池）
 *     同一层、同一条理由：这些原语只能在实机上跑，主机单测编不出来（`sdk.h` 的 inline asm）。
 *
 * ## 无敌帧（★★ 2026-09-22，owner 口径：「按一手补上」）
 *   TH10 的灵击在**发动那一瞬间**给玩家一段**一次性**无敌（不是每帧刷）：
 *     · 普通灵击 `th10:0x4259be push 0x10e`（**270**）→ `lea eax,[ebp+0x430c]` → `Timer::set` `0x405410`
 *       （该函数体：`[eax+0x4]=N` / `dec` → `[eax+0x0]=N−1` / `fstp [eax+0x8]` ⇒ 定时器 `{prev=N−1, cur=N, cur_f=N.0}`）；
 *     · 决死灵击 `th10:0x425c2d push 0xc8`（**200**）→ 同一个定时器；
 *       （同一条支路另有 `th10:0x425c20 push 0x3c` → `Timer::set(&player[+0x474])` —— 那是**闪烁**计时器，
 *        不是无敌：它被 `th10:0x425ebb cmp [ebp+0x478],[ebp+0x474]` 的「每 3 帧翻一次」逻辑读，而命中判定读的是
 *        `+0x4310`。⇒ owner 工单里那句「TH11 决死 60 帧」同理**不是无敌**，证据见 §「为什么只补 270/200」。）
 *   无敌判据（一手）：`th10:0x426950 mov eax,[ecx+0x4310] / test eax,eax / jg 0x4269bf` ⇒ **cur > 0 即跳过中弹**，
 *     同一个读点还在 `th10:0x426756`（擦弹/命中那条）。
 *   TH18 的同位字段 = `player+0x47774` 的 zTimer（**判据读 `+0x47778`**）：命中判定 `th18:0x45cd1c
 *     cmp DWORD PTR [edx+0x47778],0 / jg 0x45cd2c`（> 0 ⇒ 不调 `Player__die` `0x45d3a0`）；
 *     **原生自己的三张 bomb 卡也直接写它**（例：`th18:0x42067c mov DWORD PTR [eax+0x47778],0x78` = 120 帧）。
 *   ⇒ 本模块**走 SDK 的声明层**（`ce_invuln_request()` 每帧声明**剩余**帧数 + `ce_invuln_apply()` 取 max、
 *     且只写更长值）—— 直接写 `+0x47778` 会**截短别人的长无敌**（一手写点表与理由：
 *     `engine/player/th18/03-player-hit-gate.md` §2 与 §6(b) 风险 1）。
 *     ⚠ 声明层是**粘性**的：不清就一直按最后那个值刷 ⇒ 会变成永久无敌。⇒ 机器自己数「还剩几帧」、
 *     每帧声明、数到 0 就声明 0（`bomb10_reset()` 也显式清）。
 *   ⚠ **一处诚实差异**：TH10 的 270 < 灵击时长 296 ⇒ **最后 26 帧没有无敌**（照一手，不是 bug）；
 *     TH18 原生那三张 bomb 是**每帧刷**（40/110/120，见 `BombReimu/Sanae/Marisa__on_tick`），
 *     比我们更"护"，我们按 TH10 一手取一次性。
 *
 * ### 为什么只补 270/200（不补 TH11 那两个 60）
 *   owner 工单点名了两条：「TH10 灵击自带 **270** 帧」与「TH11 决死窗口 **60** 帧」。一手复核后：
 *   · **TH10 的 270 = 无敌** ✅（上面已证：`push 0x10e` → 命中判定读的那个定时器）；
 *   · **TH11 的 60 不是无敌**：`th11:0x431286 push 0x3c` → `Timer::set(&player[+0x944], 60)`，而 `+0x948`
 *     （= 该定时器的 `cur`）在 state 4 那一支被判成**窗口门槛**：`th11:0x43122d cmp DWORD PTR [esi+0x948],0x8
 *     / jl 0x43124f`（窗口用完 ⇒ 真死）⇒ 它是**决死窗口计时器**（成功决死后重装填 60）。
 *     TH11 的**无敌**是另一样东西：`set_iframes` `th11:0x406860`（写 `[0x4a8eb4]+0x8bb0` 的 zTimer）；
 *     全 exe 的 5 个调用点**全部 `push 0x28`（40 帧）**：`th11:0x4071bb` / `0x407963`（荷取·破罩后）/
 *     `0x407ade` / `0x407d3a` / `0x45de67` ⇒ **TH11 的灵击自带 40 帧无敌**。
 *   ⇒ 结论：按 owner 的口径「**装 th11 的时候也是 th10 灵击**」（2026-09-16m）与「两卡共用同一份实现」，
 *     本模块取 **TH10 那一套：普通 270 / 决死 200**；TH11 的 40 **不复刻**（它是 per-shot bomb 的数，
 *     我们的 bomb 不是 TH11 的 bomb），登记在 `TUNING.md` §2.5 与 `AUDIT.md` §BE-FA 的 2026-09-22b 补遗。
 *   ⇒ 而「决死要有无敌」这件事在一手侧本来就成立：TH10 决死给 **200**（> TH11 那条窗口的 60）⇒
 *     两个数都满足；TH18 侧 `Player__cancel_impending_death` `0x45d590` 另给一个 **60 帧的闪烁计时器**
 *     （`[ecx+0x634]=0x3b / [ecx+0x638]=0x3c / [ecx+0x63c]=60.0f`，**不是**无敌字段 `+0x47774`）。
 *
 * ## 一套系统 ⇒ 三条硬语义
 *   ① **单实例**：进程里只有一个 `s_bomb`（本模块的文件静态）⇒ 任何时刻**最多一发**灵击。
 *   ② **不叠加**：`bomb10_start()` 的头一道门就是「**本发还在进行中 ⇒ 不放**」。这一条**不是新闸**，
 *      是 TH10 一手：`th10:0x42598d mov eax,[esi+0x28] / test / jne 0x425a24`
 *      （`esi` = `BOMB_PTR` `0x4776ec`；`[bomb+0x28] != 0` = 灵击进行中 ⇒ 整条不干）。
 *      TH11 侧同款：`th11:0x4311c7 cmp DWORD PTR [eax+0x3c],0 / jne 0x4311fe`。
 *      ⇒ 两张卡同装时，先到的那一张发动、另一张的这一下**什么都不做**（不扣火力、不消弹、不响音效）。
 *   ③ **不互相抢**：两张卡的 `on_tick_2` 都会每帧调 `bomb10_tick()`，但**只有一张真的推** ——
 *      规则 = **id 小的那张推**（110 恒推；111 传 `ce_owned(110) ? 0 : 1`）⇒ 同装时 110 驱动、111 让位
 *      （先例 = 子机声明层「id 小者落笔」/ 组合 UI「110 优先」）。
 *
 * ## 两个入口（110 与 111 各一套，形状逐字相同）
 *   | 入口 | 触发 | 实现 |
 *   | --- | --- | --- |
 *   | **X**（既有路径，一字未动）| 炸弹库存 == 0 且 X 上升沿（`INPUT_PRESSED & 0x2`）| 卡侧 `on_tick_2` → `bomb10_start()` |
 *   | **C**（2026-09-21 新增，`WORK-PACKAGES.md` §9）| 引擎主动卡分派：C 上升沿 `0x400` + 无对话 + 关卡在跑 + **本卡被 D 选中**（一手 `th18:0x45c048`–`0x45c099`：`mgr+0x38` 那张卡的 `vtable+0x08`）| 卡侧 `on_activate` → **同一个** `bomb10_start()` |
 *   ⇒ 两个入口**只差触发条件**：灵击侧（消耗 / 伤害 / 帧数 / 判定框 / 决死形态 / 音效 / 视觉 / RANK）
 *     逐项相同，因为它们是**同一个函数**。C 的那一帧相位差（第一次 tick 落在下一帧，因为
 *     AbilityManager `0x16` 先于 Player `0x17`）**保留** —— owner 2026-09-21 拍「不动 X」。
 *
 * ## 一手：这张表就是「有 110 或 111 就有灵击」在程序上的意思
 *   · TH18 原生的 X 路径**本来就存在**，缺的只是「库存 == 0」那一支：
 *     `th18:0x45c03f call 0x420420`（`can_bomb`，**第一条指令** `cmp ds:0x4ccd58,0 / jle → 0` ——
 *     库存 0 直接返回 0）→ `0x45c048 test ds:0x4ca434,0x2` → `0x45c051 call 0x420360`（`do_bomb`）。
 *     库存 == 0 时整条不走（`0x45c046 je 0x45c056`）⇒ **这正是两张卡补上的那个缺口**。
 *   · 两张卡都「接在同一个缺口上」：X 路径各自读同一条 `INPUT_PRESSED` 上升沿；
 *     C 路径由引擎**只派发给被选中的那一张**（`0x45c090 mov ecx,[ecx+0x38]` → `0x45c099 call [eax+8]`）
 *     ⇒ C 这一侧天然不会双发，X 那一侧由本模块的单实例 + ①② 保证。
 *
 * ## 常量：一手出处（原来两张卡各抄一份，现在只有这里一份）
 *   · `BOMB10_SE` —— TH10 `se_slash.wav`（`th10:0x405942` 播 id `0x26`）↔ TH18 同一 wav = id `0x2c`
 *     （`engine/_shared/th18-sound-table.md`；❓ 0x2c/0x2d 同文件不同音量，取 0x2c，见 `TUNING.md` §2.5）。
 *   · `BOMB10_RANK_*` —— 一手 `th10:0x405987`–`th10:0x4059b8`（`sub [rank],0x80`、钳 ±`0x400`）。
 *     ⚠ TH18 的 `0x4ccd08` 是**死字段**（只有 ECL global getter 两条读、无人写）⇒ 写了没有实际效果，
 *     仍照一手写（复刻旋钮），理由见 `mof.c` 的旧注与 `TUNING.md` §2.5。
 *   · `BOMB10_DMG_SRC_LIFE` = 2 帧 —— 只为一帧余量：`enm_compute_damage_sources` `th18:0x45f0f0`
 *     的 tag 守卫让**一个源对同一敌人只结算一次** ⇒ 每帧一个新源 = 每帧一笔。
 *   · `BOMB10_FIELD_*` —— TH10/TH18 游玩区同量纲 384×448（世界坐标 −192..192 / 0..448）。
 *   · `BOMB10_RING_VMS` / `BOMB10_RING_LEAF_*` —— 一手 TH10 `bullet.anm` **script400**（灵击本体脚本）
 *     的 `scriptNew` 排期，逐条见 `bomb10.c` 的 `s_ring_sched` 表与文件头：
 *     六支柱环（394/395/396 在第 0 帧、397/396/397 在第 10/20/30 帧）+ 火星第一趟在第 31 帧、
 *     每 30 帧一趟、一趟 4 颗（红蓝红蓝）。★ 2026-09-23 起**不再**用「卡侧摆 32 片」那个近似
 *     （用户口径「**它不是 32 段**」），改回一手的配方（贴图走构建期烘出的**连续环**）。
 *     旧的 `BOMB10_RING_FADE_AT` / `BOMB10_RING_FADE_EXTRA` / `BOMB10_SPARK_EVERY`
 *     三个常量随 32 片方案一起退役（淡出/寿命现在由脚本自己走，见 `bomb10.c`）。
 *   · `BOMB10_CANCEL_MODE` = 1 —— 消弹让**引擎自己**撒消弹点（一手 `engine/bullet/th18/01-cancel-to-item.md`
 *     §2.1：mode 1 撒 type 9、角 `rand×10° − π/2`、速 2.2）；用户 2026-09-16q 口径
 *     「微档点可以完全认为是 TH18 消弹点」。
 *   · `BOMB10_FN_CANCEL_IMPENDING_DEATH` `th18:0x45d590` —— 决死形态的救命调用（thiscall(Player*)、
 *     无栈参、裸 `ret`；体内 `[ecx+0x476ac] = 1` 把状态拉回存活）。与本仓其它救援卡同一个地址
 *     （`senro.c` / `trance.c` / `animal.c` 各有一份同形包装，本节起**三张卡与本模块共用这一个包装**）。*/
#pragma once
#ifndef BOMB10_H
#define BOMB10_H

#include <stdint.h>
#include "mof_core.h"     /* mof_power_t / 半径曲线 / 上移 / 伤害表 / MOF_BOMB_* 常量（纯逻辑层）*/

/* ---- 常量（一手出处见文件头；两张卡都从这里取，别再各写一份）---- */
#define BOMB10_SE              0x2c       /* 音效 id（TH10 `se_slash.wav` 的 TH18 同 wav）*/
#define BOMB10_DMG_SRC_LIFE    2          /* 伤害源寿命（帧）*/
#define BOMB10_RANK_ADDR       0x4ccd08   /* GlobalsInner+0x2c（ExpHP `RANK`）；TH18 里是死字段 */
#define BOMB10_RANK_DROP       0x80       /* 一手 `th10:0x40598c sub eax,0x80` */
#define BOMB10_RANK_MIN        (-0x400)   /* 一手 `th10:0x4059b1` 的下钳 */
#define BOMB10_RANK_MAX        0x400      /* 一手 `th10:0x40599e` 的上钳 */
#define BOMB10_FIELD_W         384.0f     /* 游玩区宽（世界坐标 −192..192）*/
#define BOMB10_FIELD_H         448.0f     /* 游玩区高（世界坐标 0..448）*/
#define BOMB10_FIELD_CX        0.0f
#define BOMB10_FIELD_CY        224.0f
#define BOMB10_RING_VMS        6          /* 柱环 VM 槽数（一手 script400 一共 `scriptNew` 六次）*/
#define BOMB10_RING_LEAF_FIRST 31         /* 火星/枫叶第一趟的帧号（一手 script400 的 `+1: // 31`）*/
#define BOMB10_RING_LEAF_EVERY 30         /* 火星每趟间隔（一手 `interruptLabel($I0, offset128, 30)`）*/
#define BOMB10_RING_LEAF_UNITS 4          /* 每趟颗数 = 红蓝红蓝（一手脚本正文四行 `scriptNew`）*/
#define BOMB10_CANCEL_MAX      99999      /* 消弹颗数上限（一手 TH10 的 `cancel_circle` 也没有上限）*/
#define BOMB10_CANCEL_MODE     1          /* 消弹模式（1 = 引擎撒 type 9 消弹点）*/
/* 消弹诊断的「安静帧」采样间隔（帧）。★ 2026-09-24：从 30（0.5 s）收紧到 **10**（≈0.17 s）——
 * owner 口径「消弹每 0.5s 采样一次吗？事件太短了吧」：0.5 s 会把「连续在消」读成「停了」。
 * 另外**每一帧真的消掉弹都会单独打一行**（带累计），这两条加起来才看得出消弹是连续还是中断。*/
#define BOMB10_CANCEL_LOG_EVERY 10
#define BOMB10_DMG_CAP         MOF_BOMB_DMG_CAP  /* 伤害上限抬升值（❓ 自定，理由见 mof_core.h）*/
#define BOMB10_INVULN_FRAMES           270   /* 普通灵击的无敌帧数（一手 `th10:0x4259be push 0x10e`）*/
#define BOMB10_INVULN_FRAMES_DEATHBOMB 200   /* 决死灵击的无敌帧数（一手 `th10:0x425c2d push 0xc8`）*/
#define BOMB10_FN_CANCEL_IMPENDING_DEATH  0x45d590u
/* 消弹闩锁的 tag 从卡侧的 `host->card_id` 来（`ce_cancel_latch_claim(id)`：同帧只有第一个认领者消弹）。*/

/* ---- 卡片侧要交出来的东西（每张卡一份 `static const` 表；共享机器**不认识任何一张卡**）----
 * 每一条都是「两张卡确实不一样」的那一半 —— 一样的那一半（曲线 / 伤害 / VM / 音效 / RANK / 消弹）
 * 全在本模块里，别再往卡里搬。*/
typedef struct {
    uint32_t    card_id;      /* 消弹闩锁的 tag + 诊断里「谁在放」*/
    const char *log_tag;      /* 日志前缀（"mof" / "signal"；与改动前的行格式保持一致）*/

    /* ① 卡私有前置闸：返回非 0 = 这一下**不放**（机器的「灵击进行中」门在它之前）。
     *    110 = 对话 VM 忙（一手 `th10:0x425971 mov ecx,[eax+0x9eb8]`）；
     *    111 = 河童罩子还没破（一手 `th11:0x40651b`：`[bomb+0x3c] != 0` ⇒ `do_bomb` 返回 −1）。
     *    可空（NULL = 没有这道闸）。*/
    int32_t (*gate)(void *user);

    /* ② 火力扣法：返回 0 = 火力不够门槛（这一下不放；**卡侧自己打日志**，两张卡的行不同），
     *    返回 1 = 够，且把扣完的火力写进 `*power_after`。
     *    110 = `mof_core_power_spend()`（≥2.00 档 / 扣 1 档 / 下限 1.05，**整档**）；
     *    111 = `signal_core_alice_power_spend()`（魔理沙×爱丽丝 = 0.5 档；其余机体原样转发上面那一条）。
     *    `by_hit` 传进来只为「决死形态没发动」那一行日志（两张卡都有）。*/
    int32_t (*power_spend)(void *user, int32_t by_hit, int32_t power_now, int32_t *power_after);

    /* ③ 发动瞬间的**卡私有**那一半（机器已经做完：扣火力 → 写 `CURRENT_POWER` → `ce_repopulate_options()`
     *    → 决死救命 → 范围环 / 电流边 VM → 音效 → RANK）。两张卡在这里做：
     *      110：记形态（`mof_bomb_form_start`）+ 信仰 −3000（非决死）+ 作废符卡奖励 + 全避标记
     *           + `stat_bomb`/`stat_bomb_dbf` + `mof_declare_score()` + 一行日志；
     *      111：记形态（`signal_bomb_form_start`）+ 河童罩子（`signal_shield_arm`）+ 爱丽丝诊断
     *           + `stat_bomb`/`stat_bomb_dbf` + `signal_declare_score()` + 一行日志。
     *    ⚠ 顺序：**在机器那几件之后**。同帧内两组写点互不依赖（消弹/伤害源在下一帧才用这些字段），
     *      所以与原实现里「谁先谁后」的差别不可观察。可空。*/
    void (*on_start)(void *user, int32_t by_hit, int32_t cost, int32_t power_before, int32_t power_after);

    /* ④ 每帧消弹的卡私有记账：机器只报「本帧消掉几颗」，**入不入账由卡自己的形态账本回答**
     *    （110 = `mof_bomb_form_cancel_accounts()`；111 = `signal_bomb_form_cancel_accounts()`）。
     *    ⚠ 两张卡在这里的累计口径**故意不同**（110 记「有消弹的帧数」、111 记「颗数」，见各自旧注）
     *    ⇒ 机器只转发颗数，不做任何合并。可空。*/
    void (*on_cancel)(void *user, int32_t bullets);
} bomb10_host_t;

/* ---- 救命（决死形态）：`Player__cancel_impending_death` `th18:0x45d590` 的唯一包装 ----
 * 111 的河童罩子（`on_death_before_deathbomb`）也调它 —— 同一件事只留一个实现。*/
void bomb10_cancel_impending_death(void);

/* ---- 状态查询（诊断 / 卡侧日志用）---- */
int32_t bomb10_busy(void);           /* 1 = 有一发在进行中（= 一手 `[bomb+0x28] != 0`）*/
int32_t bomb10_left(void);           /* 剩余帧数（0 = 没在灵击）*/
int32_t bomb10_target_centi(void);   /* 本发的目标半径（1/100 px）*/
int32_t bomb10_spell_frames(void);   /* 符卡已过帧数（= 一手 `[spellcard+0x3738]` 的对位）*/
int32_t bomb10_owner_id(void);       /* 本发的发动者卡号（0 = 没有）*/
int32_t bomb10_invuln_left(void);
/* 本发的视觉变体（0 灵梦 / 1 魔理沙 / 2 小灵击档）—— 一手三套子脚本（主脚本 400/407/441）*/
int32_t bomb10_variant(void);    /* 本发还剩几帧无敌（0 = 已给完；诊断用）*/

/* ---- 发动 / 推进 / 收尾 ----
 * `bomb10_start`：返回 1 = **本发由你发动**（公共那一半 + 你的 `on_start` 都做完了）；
 *                 返回 0 = 这一下没放（进行中 / 你的闸 / 火力不够），**没有任何副作用**。
 * `bomb10_tick` ：每帧推进一次。`driver` = 本卡是否负责驱动这一帧 —— 两张卡都调，
 *                 规则 = **id 小的那张推**（110 恒推；111 只在 `!ce_owned(110)` 时推，见文件头 ③）。
 *                 ⚠ 两边都写成「另一张在场就让位」= **谁都不推**（这一发会彻底不动）。
 * `bomb10_reset`：收干净（删卡 / 局末 / 换关三条路都走它）：删两个环 VM + 清全部状态。*/
int32_t bomb10_start(const bomb10_host_t *host, void *user, int32_t by_hit);
void    bomb10_tick(int32_t driver);
void    bomb10_reset(void);

#endif /* BOMB10_H */
