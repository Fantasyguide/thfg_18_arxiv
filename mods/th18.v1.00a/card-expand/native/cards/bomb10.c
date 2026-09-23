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

/* ---- 视觉：TH10 灵击本体脚本的**排期**（一手 `th10` `bullet.anm` **script400**）----
 *
 * ══ 这是什么 ═══════════════════════════════════════════════════════════════════════════
 *   TH10 的灵击本体 = `bullet.anm` script400（灵梦）/ 407（魔理沙）/ 441（符卡中）。它**自己几乎不画东西**，
 *   只做两件事：① `scale(16,16) + scaleTime(120, 6, 16 → 160)` —— **这个 scale 就是灵击半径**
 *   （`Bomb::on_tick` 每帧 `[bomb+0x3c] = vm.scale.y`，`th10:0x40580e`/`0x405813`）；② `scriptNew` 出一堆子 VM。
 *   子 VM 的排期（一手原文，v4 `anmmap` 的名字）：
 *       script400 { scale(16,16); scaleTime(120,6,16.0f,160.0f);
 *                   scriptNew(394); scriptNew(395); scriptNew(396);        // 第 0 帧
 *                 +10: scriptNew(397);                                      // 第 10 帧
 *                 +10: scriptNew(396);                                      // 第 20 帧
 *                 +10: scriptNew(397);                                      // 第 30 帧
 *                   iset($I0, 80);
 *                 +1: // 31
 *                 offset128: scriptNew(398); scriptNew(399); scriptNew(398); scriptNew(399);
 *                            jmpDec($I0, offset128, 30);
 *                 +20: delete(); }
 *   ⇒ **六支柱环 + 每 30 帧一趟 4 颗火星**（红蓝红蓝，直到灵击结束）。
 *
 * ══ 2026-09-23：为什么把「卡侧摆 32 片」换回一手配方 ═══════════════════════════════════
 *   上一版没有移植贴图，用「卡侧摆 32 片竖条」近似 `texCircle(32)`（TH10 四支柱环写的是
 *   `sprite(sprite362) + texCircle(32)`，把一条 14×128 的渐变竖条弯成圈）。用户实跑给了正确形状：
 *   「**它不是32段**，然后内外还会发射红色和蓝色的枫叶特效，放的时候还会有几个扩大的紫色虚影环」
 *   ⇒ ① 环必须是**连续**的（烘焙成一张环贴图，见 `make_mof_bomb_ring_bent.py`）；
 *     ② 火星（枫叶）**内外都发、红蓝两色**（脚本 398 红往外 160→208 / 399 蓝往里 160→112，本仓已有）；
 *     ③ 起手那几帧要有**几个越来越大的紫色环**（= 一手的 script396/397：`color(255,0,255)`。
 *        扬色 + 半径 16→200 px + 60 帧寿命，在第 0/10/20/30 帧各起一支 ⇒ 画面上就是「几个扩大的紫色虚影环」）。
 *
 * ══ 逐条落位（一手 → 本仓）═════════════════════════════════════════════════════════════
 *   | 一手 | 本仓 | 说明 |
 *   | script394 | 脚本 327 `MOF_BOMB_RING394` | 白/电边，`angleVel(0,0,−0.31415927)` = −18°/帧，半径 32→160 px |
 *   | script395 | 脚本 328 `MOF_BOMB_RING395` | 同上，自转 **+18°/帧**、半径 16→160 px |
 *   | script396 | 脚本 329 `MOF_BOMB_RING396` | **紫**（`color(255,0,255)`），+6°/帧，半径 16→200 px |
 *   | script397 | 脚本 330 `MOF_BOMB_RING397` | 同上，自转 **−6°/帧** |
 *   | script398 | 脚本 280 `MOF_BOMB_SPARK`（已有）| 红枫叶**往外** 160→208 px |
 *   | script399 | 脚本 281 `MOF_BOMB_SPARK_BLUE`（已有）| 蓝枫叶**往里** 160→112 px |
 *   六次 `scriptNew` 的时点由下面的 `s_ring_sched` 表给；火星的时点由 `BOMB10_RING_LEAF_*` 给。
 *
 * ══ 两处**必须**偏离一手的地方（都登记 `TUNING.md`）═══════════════════════════════════
 *   ① **不做父子层级**：TH18 卡侧 `ce_anm_spawn` 起的是**平铺** VM（v8 的 `scriptNew` 才是「带父级的子 VM」）
 *      ⇒ 一手「子环的 scale 乘在主脚本的 scale 上」这层乘法没有了。取法：**把柱环脚本自己的 scale 当 px 半径**
 *      （烘出来的环**基准半径正好 100 px** ⇒ `scale` 就是「多少个 100 px」），于是 394 的 `scaleTime(60,4,0.32→1.60)`
 *      = 半径 32→160 px —— **正好等于灵击半径**（`MOF_BOMB_R_*_CENTI` 的目标值 16000 = 160 px）✓。
 *   ② **`scale` / `alpha` 一律不写**：脚本自己跑 `scaleTime` / `alphaTime`（卡侧每帧写 scale 会被插值器盖掉，
 *      `native/sdk.h:330`）⇒ 卡侧**只写位置**（灵击中心每帧上移，环得跟着走），淡出与自删全交给脚本。
 *      这也让「几个扩大的紫色虚影环」成立：六支柱环各自按**自己的出生帧**长大 ⇒ 半径天然错开。
 *   ③ 已退役：脚本 279（❓ 自定的实心盘占位）与 293（32 片竖条）**不再从这里 spawn** —— 一手没有实心盘，
 *      而「32 段」正是用户否掉的那个形状。两条脚本留在 `assets/ability/scripts/` 里不动（脚本号不许回收：
 *      删文件会让后面的脚本号整体前移，撞到别的会话的脚本）。
 */
#define BOMB10_RING_RADIUS_100 100.0f       /* 环贴图的基准半径(px) = 环脚本 scale 1.0 的可见半径 */

static const char *bomb10_variant_name(int32_t v)
{
    return (v == 0) ? "灵梦" : (v == 1) ? "魔理沙" : "符卡中";
}

/* 排期表：`at` = 出生帧（相对灵击发动；一手主脚本 400/407/441 的标签帧号），
 * `script[变体]` = 柱环脚本号。**变体下标与一手 `Bomb::start` 选主脚本的那三条分支一一对应**
 *   （0 = 灵梦 script400 / 1 = 魔理沙 script407 / 2 = 符卡中 script441），
 *   判据与半径曲线同一个（`spell_now` → `reimu`），见 `bomb10_start()` 里的 `s_run.variant`。
 * ⚠ 三套的子脚本**只差几个数**（成长帧数 / 目标半径 / 淡出时点），一手各有一套；
 *   2026-09-24 owner 实跑「灵击依旧很奇怪」的根因 = 我最初**只移植了灵梦那一套给全部机体**
 *   ⇒ 魔理沙（半径 112）看到的环是照 160/200 px 画的。三套的数值表在
 *   `assets/ability/make_mof_bomb_ring_scripts.py`（唯一出处，脚本由它生成）。*/
static const struct {
    int32_t  at;
    uint32_t script[3];      /* [0] 灵梦 / [1] 魔理沙 / [2] 符卡中 */
    const char *tag;
} s_ring_sched[] = {
    {  0, { CE_ANM_ABILITY_SCRIPT_MOF_BOMB_RING394, CE_ANM_ABILITY_SCRIPT_MOF_BOMB_RING401,
            CE_ANM_ABILITY_SCRIPT_MOF_BOMB_RING435 }, "环1白" },
    {  0, { CE_ANM_ABILITY_SCRIPT_MOF_BOMB_RING395, CE_ANM_ABILITY_SCRIPT_MOF_BOMB_RING402,
            CE_ANM_ABILITY_SCRIPT_MOF_BOMB_RING436 }, "环2白" },
    {  0, { CE_ANM_ABILITY_SCRIPT_MOF_BOMB_RING396, CE_ANM_ABILITY_SCRIPT_MOF_BOMB_RING403,
            CE_ANM_ABILITY_SCRIPT_MOF_BOMB_RING437 }, "环3紫" },
    { 10, { CE_ANM_ABILITY_SCRIPT_MOF_BOMB_RING397, CE_ANM_ABILITY_SCRIPT_MOF_BOMB_RING404,
            CE_ANM_ABILITY_SCRIPT_MOF_BOMB_RING438 }, "环4紫" },
    { 20, { CE_ANM_ABILITY_SCRIPT_MOF_BOMB_RING396, CE_ANM_ABILITY_SCRIPT_MOF_BOMB_RING403,
            CE_ANM_ABILITY_SCRIPT_MOF_BOMB_RING437 }, "环3紫" },
    { 30, { CE_ANM_ABILITY_SCRIPT_MOF_BOMB_RING397, CE_ANM_ABILITY_SCRIPT_MOF_BOMB_RING404,
            CE_ANM_ABILITY_SCRIPT_MOF_BOMB_RING438 }, "环4紫" },
};
/* 火星（枫叶）：一手 398/399（灵梦 半径 160）、405/406（魔理沙 112）、439/440（符卡中 64）。
 * 红 = 往外飞、蓝 = 往里收（一手字面量 +48 / −48）。*/
static const uint32_t s_leaf_script[3][2] = {
    { CE_ANM_ABILITY_SCRIPT_MOF_BOMB_SPARK,     CE_ANM_ABILITY_SCRIPT_MOF_BOMB_SPARK_BLUE     },
    { CE_ANM_ABILITY_SCRIPT_MOF_BOMB_SPARK112,  CE_ANM_ABILITY_SCRIPT_MOF_BOMB_SPARK_BLUE112  },
    { CE_ANM_ABILITY_SCRIPT_MOF_BOMB_SPARK64,   CE_ANM_ABILITY_SCRIPT_MOF_BOMB_SPARK_BLUE64   },
};
#define BOMB10_RING_SCHED_N ((int32_t)(sizeof s_ring_sched / sizeof s_ring_sched[0]))


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
    uint32_t ring_vm[BOMB10_RING_VMS];    /* 六支柱环的 VM 句柄（脚本 327–330；0 = 还没起 / 已自删）。
                                           * ★ 2026-09-23 起**取代**「279 实心盘 + 293 单条带 + 卡侧 32 片」
                                           *   那一套：实心盘是本仓的占位（一手 TH10 没有它），32 片是不认
                                           *   `texCircle` 时的近似（用户口径「**它不是32段**」）—— 两个都不该留。*/
    int32_t  ring_next;    /* 下一根要起的柱环在 `s_ring_sched` 里的下标（排期按帧号升序 ⇒ 顺序推即可）*/
    int32_t  leaf_tick;    /* 诊断：本发一共起了几颗火星（一手：每 30 帧一趟、一趟 4 颗）*/
    int32_t  small;        /* ★★ 2026-09-24：**小灵击档**（一手：特定符卡上的压制，不是「符卡中」）*/
    int32_t  variant;      /* ★★ 2026-09-24：**视觉变体** 0 = 灵梦 / 1 = 魔理沙 / 2 = 符卡中
                            *   （一手 `Bomb::start` 按同一判据选主脚本 400/407/441，三套子脚本不同）*/
    float    vx, vy;       /* ★ 2026-09-24：**视觉锚点** = 发动瞬间的自机坐标（柱环/火星都挂在这里）。
                            *   一手：`Bomb::on_tick` 每帧只动**自己**的 `bomb+0x30..0x38`（消弹/伤害中心），
                            *   **没有**写 anm VM 的位置字段（`th10:0x4057b6`–`0x4057de` 全段只碰炸弹对象）
                            *   ⇒ 环画在 `Bomb::start` 给的那一点，不跟着上升的中心走。
                            *   用户实跑口径：「**你的灵击往上运动速度太快了**」。*/
    uint32_t leaf_probe[BOMB10_RING_LEAF_UNITS];  /* ★ 诊断：最近一趟火星的 VM（用户报「枫叶呢?」）*/
    int32_t  leaf_probe_at;                       /* 诊断：到这一帧就把上面那几个 VM 的字段打出来 */
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
/* ★ 2026-09-24：本发的**视觉变体**（0 灵梦 / 1 魔理沙 / 2 小灵击档）—— 给卡侧的发动日志用
 *   （变体决定用哪一套柱环/火星脚本，见 `s_ring_sched`）。*/
int32_t bomb10_variant(void)      { return s_run.variant; }

/* 救命：`th18:0x45d590`（thiscall(Player*)、无栈参、裸 `ret`）。一手逐条见 `mof_core.h` 的
 * 「灵击 → 决死」块与 `signal_core.h` 的同名段（两个文件都在引用它）。*/
void bomb10_cancel_impending_death(void)
{
    void *p = CE_PLAYER();
    if (!p) return;
    ((void (__attribute__((thiscall)) *)(void *))(uintptr_t)BOMB10_FN_CANCEL_IMPENDING_DEATH)(p);
}

/* ---- 视觉：收干净（柱环脚本自己会 `delete()`，但删卡那一路没人替我们收 ⇒ 句柄必须显式删）---- */
static void bomb10_fx_kill(void)
{
    int i;
    for (i = 0; i < BOMB10_RING_VMS; ++i) {
        if (s_run.ring_vm[i]) { ce_anm_delete(s_run.ring_vm[i]); s_run.ring_vm[i] = 0; }
    }
    s_run.ring_next = 0;
}

/* 诊断一行：把环 VM 的关键字段打出来（用户两次报「起了 VM 却看不见」⇒ 一眼分辨
 * 「字段不对（我们的 bug）」还是「字段都对但不画（图元/层的问题）」）。对照物 = `signal/opt:` 那行。
 * ★ 2026-09-23：环现在由脚本自走 `scale`（卡侧不写）⇒ 这里的 `scale` 读数是**脚本自己跑出来的值**，
 *   正常应当看到它随帧长大（32→160 px 那一档 = 0.32→1.60）。*/
static void bomb10_fx_dump(const char *tag)
{
    uint32_t first = 0;
    uint8_t *vm;
    int i, alive = 0;

    for (i = 0; i < BOMB10_RING_VMS; ++i) {
        if (s_run.ring_vm[i] && ce_anm_get_vm(s_run.ring_vm[i])) { ++alive; if (!first) first = s_run.ring_vm[i]; }
    }
    vm = first ? ce_anm_get_vm(first) : 0;
    if (!vm) {
        ce_log("%s/fx: %s 环 vm=0 **alive=0**（六支柱环都已经不在了；`ring_next`=%d/%d）",
               s_run.host ? s_run.host->log_tag : "bomb10", tag, (int)s_run.ring_next, (int)BOMB10_RING_SCHED_N);
        return;
    }
    ce_log("%s/fx: %s 首个活环 vm=%u 活 %d/%d sprite=%d scale=(%.3f,%.3f) pos=(%.1f,%.1f) c1=%08x c2=%08x frames=%d",
           s_run.host ? s_run.host->log_tag : "bomb10", tag, (unsigned)first, alive,
           (int)BOMB10_RING_VMS,
           (int)*(const int32_t *)(vm + CE_ANM_VM_SPRITE_ID),
           (double)((const float *)(vm + CE_ANM_VM_SCALE))[0],
           (double)((const float *)(vm + CE_ANM_VM_SCALE))[1],
           (double)((const float *)(vm + CE_ANM_VM_POS))[0],
           (double)((const float *)(vm + CE_ANM_VM_POS))[1],
           *(const uint32_t *)(vm + CE_ANM_VM_COLOR1),
           *(const uint32_t *)(vm + CE_ANM_VM_COLOR2),
           (int)s_run.frames);
}

/* ---- 柱环的两件事：到点起（排期表）、每帧跟着灵击中心 ------------------------------------
 * 一手：柱环是主脚本的**子 VM**（`scriptNew`）⇒ 位置天生跟着主脚本走，主脚本又跟着灵击中心。
 * 我们平铺 spawn（见文件头的偏离 ①）⇒ 位置得由卡侧每帧补上。**scale 绝不碰**（偏离 ②）。*/
static void bomb10_ring_spawn_due(void)
{
    while (s_run.ring_next < BOMB10_RING_SCHED_N &&
           s_run.elapsed >= s_ring_sched[s_run.ring_next].at) {
        const int32_t  k  = s_run.ring_next++;
        const uint32_t id = ce_anm_spawn(CE_ABILITY_ANM(),
                                         s_ring_sched[k].script[s_run.variant], 13);
        int i;
        if (!id) {
            ce_log("%s: 柱环 %s 起不来（帧 %d，ANM VM 池满？）",
                   s_run.host ? s_run.host->log_tag : "bomb10", s_ring_sched[k].tag, (int)s_run.elapsed);
            continue;
        }
        for (i = 0; i < BOMB10_RING_VMS; ++i)
            if (!s_run.ring_vm[i]) { s_run.ring_vm[i] = id; break; }
        (void)ce_anm_set_pos(id, s_run.vx, s_run.vy, 0.0f);   /* 视觉锚点（见 bomb10_run_t 的注）*/
        ce_log("%s/fx: 起柱环 %s vm=%u 帧 %d（脚本 %u，变体 %s）",
               s_run.host ? s_run.host->log_tag : "bomb10", s_ring_sched[k].tag, (unsigned)id,
               (int)s_run.elapsed, (unsigned)s_ring_sched[k].script[s_run.variant],
               bomb10_variant_name(s_run.variant));
    }
}

static void bomb10_ring_follow(float cx, float cy)
{
    int i;
    for (i = 0; i < BOMB10_RING_VMS; ++i) {
        if (!s_run.ring_vm[i]) continue;
        if (!ce_anm_get_vm(s_run.ring_vm[i])) { s_run.ring_vm[i] = 0; continue; }  /* 脚本自删 ⇒ 清句柄 */
        (void)ce_anm_set_pos(s_run.ring_vm[i], cx, cy, 0.0f);
    }
}

/* ======================================================================================
 * 发动
 * ======================================================================================
 * 门序（**与改动前两张卡各自的门序一致**，只是合成了一条）：
 *   ① 进行中 ⇒ 不放        一手 `th10:0x42598d` / `th11:0x4311c7`（= bomb10.h 的硬语义 ②）
 *   ② 卡私有闸（`host->gate`）
 *   ③ 自机指针 / 火力门槛（`host->power_spend`；门槛不过 ⇒ **什么都不做**，卡侧自己打日志）
 * 过了三道门之后，公共那一半与前实现逐条同形（顺序照 110）：
 *   写 `CURRENT_POWER` → `ce_repopulate_options()` → 决死救命 → 记曲线参数 → 柱环（一手排期）
 *   → 音效 → RANK −0x80 → 卡侧 `on_start`。*/
int32_t bomb10_start(const bomb10_host_t *host, void *user, int32_t by_hit)
{
    const float *pp;
    int32_t power_before, power_after = 0, cost, reimu, spell_now;

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
    /* ★★★ 小灵击档 = **特定符卡**（一手：`[spellcard+0x3788] ∈ 0x5d..0x60 ∪ {0x6d}` = 风神录 6 面终符
     *   那类「灵击被压制」的场面），**不是「符卡中」**。owner 2026-09-24 口径：「那是剧情和平衡意义，
     *   不能把这个当常态」⇒ 判据走 `mof_core_bomb_suppressed(spell_id)`（名单默认空 ⇒ TH18 侧不启用），
     *   而**不是** `spell_now`。⚠ 伤害表 / `bomb+0x44` 那几处仍然用通用的 `spell_now`（一手 `flags & 1`），
     *   别混用：两者一手就不是同一个判据。
     *   ⚠ `spell_now` 这一行仍然要读（它给伤害/大字灵击用），只是**不再**参与半径与视觉变体。*/
    s_run.small        = mof_core_bomb_suppressed(ce_spell_id());
    /* ★★ 一手 `Bomb::start` 的三条分支（脚本 400/407/441）＝ 灵梦 / 魔理沙 / 小灵击：
     *   判据与下面选半径的那一条**必须同源**（`s_run.small` 优先，再看 `CHARACTER == 0`）⇒
     *   视觉与半径永远一致。三套子脚本只差数值，见 `s_ring_sched` / `s_leaf_script` 的注。*/
    s_run.variant      = s_run.small ? 2 : (reimu ? 0 : 1);
    s_run.target_centi = s_run.small ? MOF_BOMB_R_SPELL_CENTI
                                     : (reimu ? MOF_BOMB_R_REIMU_CENTI : MOF_BOMB_R_MARISA_CENTI);
    s_run.rise_milli   = mof_core_bomb_rise_milli(s_run.small, reimu);
    s_run.reimu        = reimu;
    s_run.x            = pp[0];
    s_run.y            = pp[1];
    s_run.left         = MOF_BOMB_FRAMES;
    s_run.elapsed      = 0;
    s_run.frames       = 0;
    s_run.dmg_frames   = 0;
    s_run.leaf_tick    = 0;
    s_run.ring_next    = 0;
    s_run.vx           = pp[0];
    s_run.vy           = pp[1];
    s_run.leaf_probe_at = 0;
    {
        int i;
        for (i = 0; i < BOMB10_RING_LEAF_UNITS; ++i) s_run.leaf_probe[i] = 0;
    }
    {
        int i;
        for (i = 0; i < BOMB10_RING_VMS; ++i) s_run.ring_vm[i] = 0;
    }

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

    /* 柱环：第 0 帧起 394/395/396（一手 script400 的头三行 `scriptNew`）——
     * 半径/淡出/自删**全在脚本里**（见文件头的「两处必须偏离一手」），卡侧只给出生点。*/
    bomb10_ring_spawn_due();
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

    /* ③ 柱环 + 火星（脚本 327–330 / 280 / 281）—— 排期**逐条照抄一手 script400**：
     *    · 柱环：到点起（第 0/10/20/30 帧共六支），之后每帧只写位置（环跟着上移的灵击中心走）；
     *      半径 / 淡出 / 自删全由脚本自己跑 ⇒ 卡侧**不写 scale、不发 interrupt**。
     *    · 火星（枫叶）：**第 31 帧第一趟**，之后每 30 帧一趟、一趟 4 颗 = 红蓝红蓝
     *      （一手 `+1: offset128: scriptNew(398);(399);(398);(399); jmpDec($I0, offset128, 30)`）。
     *      ⚠ 上界：一手那个循环是 `iset($I0, 80)`（80 趟 = 2400 帧），实际由**灵击寿命**截断
     *      （我们 296 帧 ⇒ 9 趟 36 颗；TH10 亦同，主脚本的 `+20: delete()` 只是兜底）。*/
    bomb10_ring_spawn_due();
    /* ★ 2026-09-24：柱环钉在**发动点**（`vx/vy`），不跟每帧上移的 `center` —— 见 `bomb10_run_t` 的注。
     *   位置每帧写一次是必要的：脚本自己不写 `pos()`（draw = `pos(0x30) + entity_pos(0x5f0) + __pos_2`）。*/
    bomb10_ring_follow(s_run.vx, s_run.vy);
    if (s_run.elapsed >= BOMB10_RING_LEAF_FIRST &&
        (s_run.elapsed - BOMB10_RING_LEAF_FIRST) % BOMB10_RING_LEAF_EVERY == 0) {
        int k;
        for (k = 0; k < BOMB10_RING_LEAF_UNITS; ++k) {
            const uint32_t id = ce_anm_spawn(CE_ABILITY_ANM(),
                                             s_leaf_script[s_run.variant][k & 1], 13);
            if (id) {
                (void)ce_anm_set_pos(id, s_run.vx, s_run.vy, 0.0f);   /* 火星也挂在锚点（一手是主脚本的子 VM）*/
                s_run.leaf_probe[k] = id;                             /* 诊断：5 帧后读它的字段 */
                ++s_run.leaf_tick;
            }
        }
        s_run.leaf_probe_at = s_run.elapsed + 5;
    }
    /* ★ 诊断（用户报「**枫叶呢?**」）：一趟火星出生 5 帧后，把它们在**引擎里**的字段读出来。
     *   能一眼分辨三种情况：VM 已经不在了 / 画在别处（pos 不是锚点+半径方向）/ scale = NaN 或 0。*/
    if (s_run.leaf_probe_at && s_run.elapsed >= s_run.leaf_probe_at) {
        int k;
        s_run.leaf_probe_at = 0;
        for (k = 0; k < BOMB10_RING_LEAF_UNITS; ++k) {
            const uint32_t id = s_run.leaf_probe[k];
            uint8_t *vm = id ? ce_anm_get_vm(id) : 0;
            if (!vm) {
                ce_log("%s/fx: 火星[%d] vm=%u **已经不在了**（出生后第 5 帧）",
                       s_run.host ? s_run.host->log_tag : "bomb10", k, (unsigned)id);
                continue;
            }
            ce_log("%s/fx: 火星[%d] vm=%u sprite=%d scale=(%.3f,%.3f) 脚本pos=(%.1f,%.1f) 实体pos=(%.1f,%.1f) c1=%08x",
                   s_run.host ? s_run.host->log_tag : "bomb10", k, (unsigned)id,
                   (int)*(const int32_t *)(vm + CE_ANM_VM_SPRITE_ID),
                   (double)((const float *)(vm + CE_ANM_VM_SCALE))[0],
                   (double)((const float *)(vm + CE_ANM_VM_SCALE))[1],
                   (double)((const float *)(vm + CE_ANM_VM_SCRIPT_POS))[0],
                   (double)((const float *)(vm + CE_ANM_VM_SCRIPT_POS))[1],
                   (double)((const float *)(vm + CE_ANM_VM_POS))[0],
                   (double)((const float *)(vm + CE_ANM_VM_POS))[1],
                   *(const uint32_t *)(vm + CE_ANM_VM_COLOR1));
            s_run.leaf_probe[k] = 0;
        }
    }

    /* ④ 收尾（还在场上的柱环要显式删 —— 六支里最长的寿命 140 帧 < 灵击 296 帧）*/
    if (--s_run.left <= 0) {
        bomb10_fx_dump("结束前");
        bomb10_fx_kill();
        ce_log("%s: 灵击结束（%d 帧，伤害源 %d 帧，半径 %.1f，中心 y %.1f，火星 %d 颗，变体 %s%s）",
               host->log_tag, (int)s_run.frames, (int)s_run.dmg_frames, (double)r, (double)s_run.y,
               (int)s_run.leaf_tick, bomb10_variant_name(s_run.variant),
               s_run.small ? "（小灵击档）" : "");
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
