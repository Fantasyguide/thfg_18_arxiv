/* 绀珠传（card-expand id 115）—— 被动：TH15 章节系统 + 擦弹减速。
 *
 * 一手来源：
 *   engine/chapter/th15/01-chapter-and-bonus.md      （章节奖公式 / 阈值 / 面板）
 *   engine/player/th15/01-graze-and-item-slowdown.md （擦弹 → 掉落物减速 0.3，每帧 +0.1 回 1.0）
 * 设计：docs/superpowers/specs/2026-09-09-lolk-chapter-design.md
 *
 * 两件事：
 *   ① 波次：**一段攻击 = 一波**（用户 2026-09-09 定案）—— 一个非符是一波、一张符卡也是一波。
 *      引擎只给「符卡进行中」一个位（SPELLCARD+0x78 bit0），所以**两个沿都是边界**：
 *      上升沿结算非符波、下降沿结算符卡波；第一波由「boss 出现」开波、boss 消失时收尾（spec §2）。
 *   ② 擦弹减速 + 擦弹圈：**只要擦弹判定框里有敌方子弹就慢**，并且这时自机周围画一圈
 *      （TH15 的 `eff_sloweffect.png`，见 assets/ability/make_lolk_ring_art.py）——
 *      用户 2026-09-09 二次定案；
 *      每帧扫弹池，框内有一颗弹就把 `ITEM_MANAGER+0xe6bb14` 写成 0.3；
 *      引擎自己每帧 +0.1 回 1.0（TH15 原值 + TH18 残留的恢复逻辑，见 engine/player/th18/02）。
 *
 * 本章计数器：擦弹数（原生计数 `0x4ccd0c` 差分）、刷出/消失数（zEnemyManager.enemy_count_real
 * 差分，≈ 击破率的分母/分子，误差见 spec §3.2）。
 *
 * 两个 TH18 接缝集中在文件顶部：`graze_read()` 与 `slow_apply()` —— 都只读/写现成的引擎字段，
 * **不加断点、不改字节**。
 * ★ 2026-09-18 新增第三个接缝（同样是**现成的共享站点**，不是新断点）：`on_item_collect()`
 * （站点 `th18:0x44625f`，本来就在 31 站点表里、110/114 也在用）—— 满 P 时收 F 道具往
 * **共享 PIV 池**写 +100 显示；章节奖那一笔在 `apply_settle()` 里写同一块池。
 * 两份涨源的一手出处 = `engine/faith/th15/01-point-item-value.md`（§4.3 满 P / 「★ 章节奖全链」）。*/
#include "sdk.h"
#include "lolk_core.h"
#include "hud_dim.h"     /* ★ 跨卡统一件：HUD/面板压暗（❓ 用户口径，见文件头）*/
#include <string.h>
#include <stdio.h>      /* snprintf：五行数值的字符串 */

/* ------------------------------------------------------------------ 接缝（TH18 一手，见 engine/chapter/th18/01 + engine/player/th18/02）*/

/* 原生擦弹计数（GLOBALS.inner+0x30）。`Player__do_graze` `0x45e160` 每次擦弹 +1、封顶 99999999；
 * `GameThread__thread_start` 每 run 归零。旁边的 GRAZE_IN_CHAPTER `0x4ccd10` 是**死字段**
 * （只写不读、永不归零）→ 章内擦弹一律用本卡自己的差分，不碰它。*/
#define LOLK_GRAZE_ADDR        0x4ccd0c

/* 道具减速因子：`ITEM_MANAGER + 0xe6bb14`（float）。ExpHP 直接命名为 lolk_item_slowdown_factor；
 * 引擎在 `0x445bf4`–`0x445c7d` 拿它乘 state==1（飘落）道具的位移与重力增量，
 * 每帧尾 `0x446807`–`0x446821` 自己 +0.1 恢复到 1.0（上限 `0x4b9178`），重置写 1.0（`0x42ab27`）。
 * TH18 全 exe **没有任何地方写 0.3** —— 原生从不减速，这就是 TH15 那套代码的残骸。*/
#define LOLK_ITEM_MGR_PTR      0x4cf2ec
#define LOLK_SLOW_FACTOR_OFF   CE_ITEM_SLOWDOWN   /* = itemmgr+0xe6bb14（engine.h）*/

static int32_t graze_read(void)
{
    return *(volatile int32_t *)LOLK_GRAZE_ADDR;
}

/* 擦弹判定（TH15 与 TH18 同一套代码，一手）——**两种模式**，本卡两种都判：
 *   圆形（子弹 `flags & 0x10`）：弹心 vs 自机判定圆，边距 = max(弹半径 / 2.5, **40.0**)
 *       —— TH15 `th15:0x455d00`（常量 `th15:0x4cff28`=2.5、`th15:0x4cffb8`=40.0）、
 *          TH18 `0x45cd90`（`0x4b921c`=2.5、`0x4b92c4`=40.0）——**两作完全相同**。
 *          自机判定圆半径 = `player+0x4799c`（非聚焦）/ `+0x479a0`（聚焦），原生都 3.0。
 *   矩形（其余子弹）：弹**中心** vs 自机判定框外扩 **24.0**
 *       —— TH15 `th15:0x455be0`（`th15:0x4cffa0`）、TH18 `0x45cc20`（`0x4b92a4`）。
 * 选路在子弹自己的分派器 `0x4248a0`：`flags & 0x2` 才参与碰撞、`& 0x10` 走圆、`& 0x40` 先按
 * `bullet+0xf38` 缩放半径。所以「框内有弹」= 圆判定或矩形判定任一命中。
 * 为什么本卡要自己扫：TH18 的擦弹**每颗子弹一生只算一次**（擦后置 `bullet+0x20 |= 4`），
 * 靠擦弹计数差分只在那一帧生效 → 用户要「框内有弹就一直慢」必须每帧自己判。*/
#define LOLK_GRAZE_RECT_MARGIN   24.0f
#define LOLK_GRAZE_CIRCLE_MIN    40.0f
#define LOLK_GRAZE_CIRCLE_DIV    2.5f
/* ★ TH15 的擦弹半径系数：`th15:0x455d00` 在「变大」态里 `r = player_scale × 3.6 × 判定半径`
 *（`Player__i_grow_bigger` `th15:0x457510` 置 `player+0x16294` bit4）。本卡取该系数。*/
#define LOLK_GRAZE_RADIUS_SCALE  3.6f
#define CE_PLAYER_HITBOX_HW      0x479a4    /* float：判定框半宽（原生 1.5）*/
#define CE_PLAYER_HITBOX_HH      0x479a8    /* float：判定框半高（原生 1.5）*/
#define CE_PLAYER_HIT_RADIUS     0x4799c    /* float：判定圆半径（原生 3.0；聚焦 +0x479a0 同值）*/
#define CE_BULLET_RADIUS         0x658      /* float hitbox_radius */
#define CE_BULLET_VM             0x28       /* zAnmVm（颜色在它 +CE_ANM_VM_COLOR）*/
#define CE_BULLET_FLAG_GRAZED    0x4        /* flags & 0x4：已被擦过（0x424984 置，一生一次）*/
/* TH15 `th15:0x419d3d`：B=0x80、R=0xff、A=0xff，G = min((0x68 - 计时器)*2, 0xf0)，
 * 而计时器 < 0x2d(45) 才写 → G 实际只落在 118..208，最亮 G=0xd0(208)（0xf0 够不着）。*/
#define LOLK_GRAZED_COLOR   0xffffd080u     /* BGRA：A=ff R=ff G=d0 B=80 */
#define CE_BULLET_SIZE_SCALE     0xf38      /* float：flags & 0x40 时乘到半径上 */
#define CE_BULLET_FLAG_COLLIDE   0x2        /* flags & 0x2：参与与自机的碰撞 */
#define CE_BULLET_FLAG_CIRCLE    0x10       /* flags & 0x10：圆形判定 */
#define CE_BULLET_FLAG_SCALED    0x40       /* flags & 0x40：半径按 +0xf38 缩放 */

/* 场上敌人个数（zEnemyManager+0x198 enemy_count_real）。*/
static int32_t enemy_count_read(void)
{
    uint8_t *em = CE_ENEMY_MGR();
    return em ? *(int32_t *)(em + 0x198) : -1;
}

/* ---------------------------------------------------------------- 道中波次接缝（TH18 一手）
 *
 * TH15 的道中「一章」= 关卡脚本里一段敌人组（ECL 524 在 `MainSubNN` 开头宣告，见
 * engine/chapter/th15/01 §1b）。TH18 道中不切章，但脚本结构一样（`main → MainFront /
 * MainLatter → MainSubNN`）。
 *
 * **2026-09-13 用户洞察（本卡现行口径）**：`MainSub00 / 01 / 01b / 02 …` 的**编号本身就是
 * 章/波的天然键** —— 去掉尾部字母后的数字是一章，`01` + `01b`（+ `01c`）同章。
 * 所以本节接缝取 **关卡脚本主上下文当前子程序名 → MainSub 编号**（`parse_mainsub_number`），
 * 交给 lolk_core.c 当道中章边界（详见 lolk_core.h 头注）。
 * 完整论证/一手证据见 engine/chapter/th18/03-midstage-waves.md（本次改写该节）。
 *
 * 关卡脚本本身就是一个敌人：引擎在 `EnemyManager__destroy_all` 之后用
 * `allocate_new_enemy(&"main", params)` 建它（`0x444228` / `0x444471`，名字串 `0x4b6634`；
 * 参数结构 `_memset(...,0,0x54)` → parent_enemy_id = 0）。它的子程序名是
 * `main` / `MainFront` / `MainSubNN`…，普通敌人是 `Girl*` / `Cir*` —— 用
 * 「parent == 0 + 子程序名前缀 Main/main」认它（引擎每关重建一次，id 记在 s_stage_id）。
 *
 * 读数（全部只读，不挂钩、不改字节）：
 *   子程序号    `*(zEnemy+0xc)` = 主上下文指针（`Enemy__ecl_run` 收尾 `0x48d4d6` 把
 *               `*(zEnemy+0xc)` 复位成 `zEnemy+0x10`，所以帧末读到的永远是主上下文，
 *               异步子上下文 `MainSubNNb` 不会串进来），再取 `+4` =
 *               `zEclRunContext.cur_location.subroutine_index`（引擎自己在 `0x436a14`
 *               就是这么用的）；`+8` 是字节码偏移。
 *   子程序名    `zEnemy+0x1218` = `zEclVm.file_manager`（`Enemy__constructor` `0x42e439`
 *               从 `EnemyManager+0x188` 拷），`fm+0x8c` = 子程序表（每项 8 字节
 *               {name, bytecode}），`fm+8` = 表长（`Enemy__sub_48d920` `0x48d920` 的二分查找）。
 *   刷出事件    `zEnemyManager+0x90` = next_enemy_id，`Enemy__constructor` `0x42e40b`
 *               每建一个敌人 +1（全 exe 只有它和 manager 构造写）→ 帧间差分 = 本帧刷出数。*/
#define CE_ENEMY_PARENT_ID       0x6834     /* zEnemy.parent_enemy_id（引擎建的关卡脚本 = 0）*/
#define CE_ENEMY_ECL_CTX         0x0c       /* zEnemy.ecl.context.current_context（帧末 = 主上下文）*/
#define CE_ECL_CTX_SUB           0x04       /* zEclRunContext.cur_location.subroutine_index */
#define CE_ENEMY_ECL_FILEMGR     0x1218     /* zEnemy.ecl.file_manager */
#define CE_ECL_FM_SUB_TABLE      0x8c       /* zEclFileManager.subroutines（zEclSubroutinePtrs[]）*/
#define CE_ECL_FM_SUB_COUNT      0x08       /* zEclFileManager 子程序个数 */
#define CE_EM_NEXT_ENEMY_ID      0x90       /* zEnemyManager.next_enemy_id */

static int32_t s_stage_id;                  /* 认出来的关卡脚本 enemy_id；0 = 还没认出来 */

/* 子程序名是不是关卡脚本的（main / Main*）。普通敌人子程序名不以这两个开头。
 * 逐字节比、遇 NUL 停 —— 名字是 ECL 文件里的 C 串，别越读。*/
static int stage_name_is_script(const char *n)
{
    static const char k_upper[] = "Main";
    static const char k_lower[] = "main";
    int i;

    if (!n) return 0;
    for (i = 0; i < 4; ++i) {
        if (n[i] == '\0') return 0;
        if (n[i] != k_upper[i]) break;
        if (i == 3) return 1;                        /* "Main…" */
    }
    for (i = 0; i < 4; ++i) {
        if (n[i] == '\0') return 0;
        if (n[i] != k_lower[i]) return 0;
    }
    return n[4] == '\0';                             /* 正好 "main" */
}

/* 取敌人的「主上下文当前子程序名 + 子程序号」；失败返回 0（*out_sub 置 -1）。*/
static int enemy_ecl_sub(uint8_t *e, int32_t *out_sub)
{
    uint8_t *ctx, *fm, **subs;
    int32_t sub;

    *out_sub = -1;
    if (!e) return 0;
    ctx = *(uint8_t **)(e + CE_ENEMY_ECL_CTX);
    fm = *(uint8_t **)(e + CE_ENEMY_ECL_FILEMGR);
    if (!ctx || !fm) return 0;
    sub = *(int32_t *)(ctx + CE_ECL_CTX_SUB);
    if (sub < 0 || sub >= *(int32_t *)(fm + CE_ECL_FM_SUB_COUNT)) return 0;
    subs = *(uint8_t ***)(fm + CE_ECL_FM_SUB_TABLE);
    if (!subs) return 0;
    *out_sub = sub;
    return 1;
}

static const char *enemy_sub_name(uint8_t *e, int32_t *out_sub)
{
    uint8_t *fm, **subs;

    if (!e || !enemy_ecl_sub(e, out_sub)) return 0;
    fm = *(uint8_t **)(e + CE_ENEMY_ECL_FILEMGR);
    subs = *(uint8_t ***)(fm + CE_ECL_FM_SUB_TABLE);
    return (const char *)subs[*out_sub * 2];
}

/* 关卡脚本：parent == 0 且当前子程序名是 main / Main*。*/
static uint8_t *stage_script_find(uint8_t *em)
{
    uint8_t *e;

    CE_ENEMY_FOREACH(em, e) {
        int32_t sub;
        const char *name;
        if (!e) continue;
        if (*(int32_t *)(e + CE_ENEMY_PARENT_ID) != 0) continue;
        name = enemy_sub_name(e, &sub);
        if (stage_name_is_script(name)) return e;
    }
    return 0;
}

/* ★ 2026-09-13：解析 ECL 子程序名里的 **MainSub 编号（去掉尾部字母）**：
 *   "MainSub00" → 0、"MainSub01" → 1、"MainSub01b" → 1、"MainSub01c" → 1、"MainSub10" → 10；
 *   不是 "MainSub" + 数字（例如 "main" / "MainFront" / "MainBoss"）→ -1。
 * 名字是 ECL 文件里的 C 串（`zEclSubroutinePtrs.name`），只读到非数字 / 6 位为止，
 * 不越读、不依赖数字后面是 NUL（`MainSub01b` 后面就是 'b'）。
 * 大小写敏感 —— 原生 8 关的拼写全是 "MainSub"。*/
#define LOLK_MAINSUB_PREFIX     "MainSub"
#define LOLK_MAINSUB_DIGITS_MAX 6
static int32_t parse_mainsub_number(const char *n)
{
    int32_t v = 0;
    int i;

    if (!n) return -1;
    if (strncmp(n, LOLK_MAINSUB_PREFIX, 7) != 0) return -1;
    n += 7;
    if (*n < '0' || *n > '9') return -1;              /* "MainSubXxx" 不是编号段 */
    for (i = 0; i < LOLK_MAINSUB_DIGITS_MAX && n[i] >= '0' && n[i] <= '9'; ++i) {
        v = v * 10 + (n[i] - '0');
        if (v > 99999) { v = 99999; break; }          /* 防溢出；原生只到 2 位 */
    }
    return v;
}

/* 读关卡脚本（引擎建的 "main" 敌人）**主上下文**当前子程序号 + MainSub 编号。
 *   返回 1 = 找到关卡脚本：*out_sub 有效；*out_chapter = MainSub 编号，< 0 = 非 MainSub 段；
 *   返回 0 = 没找到（*out_sub / *out_chapter 都置 -1）→ 核心退到静默间隔。
 * 读数链与引擎自己一致（`0x436a14`：`zEnemy+0xc` → `ctx+0x4` 子程序号；
 * `zEnemy+0x1218` → `fm+0x8c` 子程序表，每项 8 字节 {name, bytecode}）。*/
static int stage_ecl_read(int32_t *out_sub, int32_t *out_chapter)
{
    uint8_t *em = CE_ENEMY_MGR();
    uint8_t *e;
    int32_t sub = -1;

    *out_sub = -1;
    *out_chapter = -1;
    if (!em) return 0;
    if (s_stage_id) {
        e = ce_enemy_by_id(em, s_stage_id);      /* 缓存命中：按 id 复核，指针不跨帧缓存 */
        if (!e) {
            s_stage_id = 0;                      /* 被回收（换关）→ 重新认 */
        } else if (enemy_ecl_sub(e, &sub)) {
            *out_sub = sub;
            *out_chapter = parse_mainsub_number(enemy_sub_name(e, &sub));
            return 1;
        } else {
            return 0;                            /* 对象在但读不到上下文（换关空窗）*/
        }
    }
    e = stage_script_find(em);
    if (!e) return 0;
    s_stage_id = *(int32_t *)(e + CE_ENEMY_ID);
    if (!enemy_ecl_sub(e, &sub)) return 0;
    *out_sub = sub;
    *out_chapter = parse_mainsub_number(enemy_sub_name(e, &sub));
    return 1;
}

static int32_t spawn_id_read(void)
{
    uint8_t *em = CE_ENEMY_MGR();
    return em ? *(int32_t *)(em + CE_EM_NEXT_ENEMY_ID) : -1;
}

/* ---------------------------------------------------------------- 相位信号（TH18 一手）
 * 详见 engine/chapter/th18/02-wave-signals.md。全部只读，不写、不挂钩：
 *   章号        GLOBALS.inner+0x24（`0x4ccce4`，ECL 524 `setChapter` 写）—— 道中切章
 *   符卡位      SPELLCARD+0x78：bit0 进行中 / bit3 耐久符卡（时符）/ bit7 已超时
 *   相位槽      zEnemy+0x6374 起 8 个 {hp_value, time}（stride 0x88）；活动槽 = 第一个 hp_value > -1 && time > 0
 *   相位判定    `life.current(zEnemy+0x6220) <= 槽.hp_value` = 击破；`槽.time <= time_in_ecl(zEnemy+0x14ec)` = 超时
 *   超时持续位  zEnemy flags_lo(+0x635c) bit24：超时置 1、击破清 0（`0x42f3a0` / `0x42f0b8` 两条路径）*/
#define LOLK_CHAPTER_ADDR        0x4ccce4
#define CE_ENEMY_FLAGS_LO        0x635c
#define CE_ENEMY_LIFE_CUR        0x6220
#define CE_ENEMY_TIME_IN_ECL     0x14ec
#define CE_ENEMY_INTERRUPTS      0x6374
#define CE_ED_INT_STRIDE         0x88
#define CE_ED_INT_COUNT          8
#define LOLK_BOSS_TIMEOUT_BIT    0x1000000u

static uint8_t *boss_enemy(void)
{
    uint8_t *em = CE_ENEMY_MGR();
    unsigned i;

    if (!em) return 0;
    for (i = 0; i < CE_EM_BOSS_SLOTS; ++i) {
        int32_t id = *(int32_t *)(em + CE_EM_BOSS_IDS + i * 4);
        uint8_t *e;
        if (!id) continue;
        e = ce_enemy_by_id(em, id);
        if (e) return e;
    }
    return 0;
}

static void phase_read(lolk_in_t *in)
{
    uint8_t *e = boss_enemy();
    uint32_t fl;
    unsigned i;

    memset(in, 0, sizeof *in);
    in->chapter = *(volatile int32_t *)LOLK_CHAPTER_ADDR;
    fl = ce_spell_flags();
    in->spell_active = (fl & CE_SPELL_FLAG_ACTIVE) ? 1 : 0;
    in->survival = (fl & CE_SPELL_FLAG_SURVIVAL) ? 1 : 0;
    in->spell_timeout = (fl & CE_SPELL_FLAG_TIMED_OUT) ? 1 : 0;
    if (!e) return;
    in->boss_present = 1;
    in->phase_timeout_flag =
        (*(volatile uint32_t *)(e + CE_ENEMY_FLAGS_LO) & LOLK_BOSS_TIMEOUT_BIT) ? 1 : 0;
    for (i = 0; i < CE_ED_INT_COUNT; ++i) {
        uint8_t *slot = e + CE_ENEMY_INTERRUPTS + i * CE_ED_INT_STRIDE;
        int32_t hp = *(volatile int32_t *)slot;
        int32_t tm = *(volatile int32_t *)(slot + 4);
        if (hp > -1 && tm > 0) {
            int32_t cur = *(volatile int32_t *)(e + CE_ENEMY_LIFE_CUR);
            int32_t t = *(volatile int32_t *)(e + CE_ENEMY_TIME_IN_ECL);
            in->phase_sig = (int32_t)((i + 1) * 1000003 ^ hp * 31 ^ tm * 131 ^
                                       *(volatile uint32_t *)(slot + 8));  /* sub_life 前 4 字节 */
            in->phase_broken = (cur <= hp) ? 1 : 0;
            in->phase_timeout = (tm <= t) ? 1 : 0;
            break;
        }
    }
}

/* 减速落地：**框内有弹的每一帧都写 0.3**（松手后交给引擎自己的 +0.1/帧 恢复）。
 * 这样我们只写一个 float，不改任何代码、不加断点，也不与引擎的恢复打架。*/
static void slow_apply(int slow_now)
{
    uint8_t *mgr;
    if (!slow_now) {
        return;                          /* 框里没弹：不动它，让引擎按 +0.1/帧 回 1.0 */
    }
    mgr = *(uint8_t **)LOLK_ITEM_MGR_PTR;
    if (!mgr) {
        return;
    }
    *(volatile float *)(mgr + LOLK_SLOW_FACTOR_OFF) = (float)LOLK_SLOW_START / 100.0f;
}

/* --------------------------------------------------- 消弹小道具：共享 PIV 池（TH15 一手 + TH18 一手）
 *
 * ★★ 2026-09-18（owner 拍板「先撤」）：**原先在这里接的「满 P 时收 type 8 ⇒ PIV +100」已撤掉**。
 *   撤的理由（owner 原话）：「**其他作可能有满 P 时的同语义产物**」⇒ 这属于**跨作口径**，
 *   不该由 115 单独定；而且一手证据显示 **TH15 的 type 8 在原版数据里没有任何生成点**
 *   （12 个 spawn 调用点 + `thecl` 全解 23 个 `.ecl` 零处 8，见
 *   `engine/faith/th15/01-point-item-value.md` §4.6）⇒ TH15 自己不会发生这条。
 *   将来若要接，判据应当是**跨作统一的「满 P 档」口径**（TH16/TH17 的 type 8 也是满 P 档），挂在别的轮次。
 *   下面保留那段一手记录（type 8 的 handler 语义），实现已删 —— 别再照着它接回来。
 *
 * ~~TH15 的涨源：满 P 时收 F 道具（item type 8）⇒ 最大得点 +100 显示。~~
 *   一手（`engine/faith/th15/01-point-item-value.md` §4.1 第 3 行 + §4.3）：
 *   `th15:0x440330` 是 type 8 的 handler，`th15:0x440347` `cmp CURRENT_POWER, __CURRENT_POWER_COPY`
 *   ⇒ **未满走「加火力」那一支**；已满才 `th15:0x44034b` `edx = CURRENT_PIV`、
 *   `th15:0x440357` `add edx,0x2710`（+10000 存储 = **+100 显示**）、`th15:0x440372` 写回。
 *
 * TH18 的对位事件 = **收 type 8 道具（补满火力）**：一手 `th18:0x44625f` 的 type 分派表把 type 8 交给
 * `th18:0x44627f`（`collect_power(MAX_POWER)`）= TH15 type 8 的 F 道具同义（TH18 的 item type 全表
 * 见 `cards/mof_core.h` 的「§满 P 改写」注释块，那张表的一手就是 `th18:0x44625f` / `th18:0x446830`）。
 * 满 P 判据照 TH18 引擎自己的那一条：`CURRENT_POWER >= MAX_POWER`（一手 `th18:0x446981`
 * `cmp eax,[0x4ccd3c]`）。⚠ `engine.h` 只给了 `CE_CURRENT_POWER()`，max power 的读取宏**就地补一个**
 * （与 `cards/mof.c` / `cards/ufo.c` 的同名宏同源，只读，不挂钩）。
 *
 * 接缝 = **现成的共享站点 `ce_item_collect`（`th18:0x44625f`）** —— 它在 SDK 的 31 站点表里本来就有
 * （`sites.py` / `patch` 都已声明，110 / 114 也在用）⇒ 本卡订阅它**不新增断点、不新增入場時刻、不改字节**，
 * 与「本卡不加断点、不改字节」的纪律一致（新增的只是一个回调注册，见文件末 CE_CARD）。
 * 该站点在**本颗道具的副作用之前**（分值还没进 SCORE，`sites.py` 的原话）⇒ 这里读到的 power 正是
 * 「收这颗之前」的值，与 TH15 在 handler 里判的是**同一个时点**。
 * 一颗道具分发一次 ⇒ **一次事件加一笔**；同一帧多颗就多笔（不合并、不漏）。*/

/* ★★ 2026-09-18（owner 拍板「+2 接上」）：**消弹小道具档**。
 * TH15 侧的活口只有 **type 10（消弹掉的小道具）**：`th15:0x41e4b0`
 * （`gen_items_from_cancel`，10 个 caller = 弹消/敌死/相位切换）→ `th15:0x4408d0` → handler `th15:0x43ff4b`
 * → `th15:0x43b200`（实参 `2.0f` @`th15:0x4cff10`）⇒ **PIV += 2 显示**（一手见
 * `engine/faith/th15/01-point-item-value.md` §4.1/§4.6）。
 * TH18 的对位 = **type `9`–`0x0e` 的「分数转换点族」**（同一个 handler `th18:0x446547`：`SCORE += trunc10(MONEY)/10`，
 * 六型同值、内部无档位区分）—— 它们正是 TH18 里**弹消 / 击破的转换产物**，与 TH15 的消弹小道具同源。
 * ⚠ **`+20` 档（TH15 type 11）不接**：一手穷举证明**它在原版没有任何生成点**
 * （12 个 spawn 调用点 + `thecl` 全解的 ECL 里零处 11）⇒ TH18 侧没有对位物，硬接就是自定。
 * 闸照旧由层管（`ce_piv_add` 按本卡可写上限累计夹），本卡不夹。*/
#define LOLK_ITEM_TYPE18_POINT_CONV_LO  9u     /* TH18 type 9  = 分数转换点族最低型 */
#define LOLK_ITEM_TYPE18_POINT_CONV_HI  0x0eu  /* TH18 type 14 = 分数转换点族最高型（六型同 handler）*/
#define LOLK_PIV_CANCEL_ITEM            2      /* TH15 type 10（消弹小道具）= +2 显示（一手 th15:0x43ff4b）*/
static void on_item_collect(ce_card_t *c, uint32_t type, const void *item)
{
    (void)item;
    /* ★★ 消弹小道具档（TH15 type 10 ⇒ +2 显示；见上面那段常量注释的一手链）。
     *   一颗一笔；同一帧多颗就多笔（与一手「每颗小道具 +2」一致）。*/
    if (type >= LOLK_ITEM_TYPE18_POINT_CONV_LO && type <= LOLK_ITEM_TYPE18_POINT_CONV_HI) {
        ce_piv_add(ce_card_id(c), LOLK_PIV_CANCEL_ITEM);
        return;
    }
    /* ★ 2026-09-18：原先这里还有一支「满 P 收 type 8 ⇒ +100」—— 已按 owner 口径**撤掉**
     *   （理见文件上方那段：属跨作「满 P 档」口径，且 TH15 原版根本不产出 type 8）。*/
}

/* ------------------------------------------------------------------ 状态 */

typedef struct {
    lolk_state_t   st;              /* 纯逻辑（lolk_core）*/
    int32_t        last_graze;      /* 上一帧原生擦弹计数；-1 = 未初始化 */
    int32_t        last_enemies;    /* 上一帧敌人数；-1 = 未初始化 */
    int32_t        last_spawn_id;   /* 上一帧 zEnemyManager.next_enemy_id；-1 = 未初始化 */
    lolk_settle_t  last;            /* 最近一次结算（HUD 用）*/
    int32_t        last_age;        /* 结算后经过帧；-1 = 还没结算过 */
    hud_dim_t      hud;             /* ★ 2026-09-15g：面板防遮挡状态 —— **全部走共享件 `hud_dim_tick()`**
                                     * （闩锁 `dim` + alpha；终点 **64（25%）**、过渡 **8 帧线性** ——
                                     * 这两个值 2026-09-21 起是**全卡统一值**，权威口径见 `hud_dim.h` 文件头
                                     * 与 `TUNING.md` §7.1/§7.5；本卡不再自持一份）。
                                     * 原来是 `int32_t panel_alpha`（255 / **76**、每帧 ±8）。*/
    int32_t        fade_t;          /* ★ 2026-09-21：面板退场已走帧数；**-1 = 没在退场**（按场次复位）*/
    int32_t        fade_from;       /* ★ 2026-09-21：退场起点 alpha —— 五行数值从它线性降到 0（20 帧）*/
    int32_t        waves_total;     /* 本 run 已结算波数（日志用）*/
    uint32_t       ring_id;         /* 擦弹圈的 VM（钉在自机上；0 = 还没起）*/
    uint32_t       panel_id[6];     /* 波次面板 UI VM（TH15 script241 的五个标签条 + 碎片标签）*/
    int32_t        roll_left;       /* TH15 gui+0x124：还剩多少「待跳量」（初值 = rate%）*/
    int32_t        roll_step;       /* TH15 gui+0x134：每帧显示值加多少（bonus / rate）*/
    int32_t        roll_shown;      /* TH15 gui+0x130：当前显示值（从 0 跳到 bonus）*/
    int32_t        roll_active;
} lolk_card_t;

/* ⚠️ ce_state 的上限是 CE_STATE_BYTES−16 = 496 字节；超了它**返回 NULL**，
 * 于是每个回调都在 `if (!s) return 0;` 退掉 —— 卡看着绑上了（日志有 bound）却什么也不做。
 * 2026-09-09 就踩过：染色表 tinted[64]+tinted_orig[64] 让它涨到 ~1KB。留个编译期闸。*/
_Static_assert(sizeof(lolk_card_t) <= 496, "lolk_card_t 超过 ce_state 的 496 字节上限");

/* 染色表**放在卡外**（静态数组）：这样不占 ce_state 的 496 字节额度
 * ——2026-09-09 把 768 字节塞进私有状态，卡就被 ce_state 判 NULL 静默失效了。
 * 语义 = 用户口径：进擦弹判定圈 → 染 TH15 的黄；飞出圈 → 还原原色。
 * 每帧开头先把上一帧染过的全部还原，再重染本帧在圈里的（TH18 引擎不会每帧重设子弹颜色）。*/
#define LOLK_TINT_MAX 256
static uint8_t *s_tint_bullet[LOLK_TINT_MAX];
static uint32_t s_tint_orig[LOLK_TINT_MAX];
static unsigned s_tint_n;

static void tint_restore_all(void)
{
    unsigned i;
    for (i = 0; i < s_tint_n; ++i) {
        volatile uint32_t *p = (volatile uint32_t *)(s_tint_bullet[i] + CE_BULLET_VM + CE_ANM_VM_COLOR1);
        /* ★★ 2026-09-10 第三十三轮：**只还原 RGB，绝不碰 alpha**。
         * 用户实测（「敌方的生弹点在擦弹范围里 → 出圈后黄色褪去，但变得极其透明」）：
         * 原来把整条 `color_1`（含 alpha）写回「染色前那一帧的快照」，而弹的 alpha 是脚本
         * `alphaTime`/`colorTime` 逐帧在变的 ⇒ 快照里的 alpha 常常偏低（刚出生淡入时更低），
         * 一还原就把弹钉死在那条低 alpha 上。alpha 归引擎的插值器管，我们只借 RGB 染黄。
         * （旧版还留着「守卫」问题：颜色被别处改过就跳过还原 → 一直黄；无条件还原 + 只动 RGB
         * 两件事一起才干净。）*/
        *p = (*p & 0xff000000u) | (s_tint_orig[i] & 0x00ffffffu);
    }
    s_tint_n = 0;
}

static void tint_mark(uint8_t *b, uint32_t *col)
{
    if (s_tint_n >= LOLK_TINT_MAX) return;      /* 表满就不再染（只影响观感）*/
    s_tint_bullet[s_tint_n] = b;
    s_tint_orig[s_tint_n] = *col;
    s_tint_n++;
    *(volatile uint32_t *)col = (*col & 0xff000000u) | (LOLK_GRAZED_COLOR & 0x00ffffffu);   /* ★ 只改 RGB */
}

static int bullet_in_graze_zone(void)
{
    uint8_t *mgr = CE_BULLET_MGR();
    uint8_t *pl = CE_PLAYER();
    const float *pp;
    float px, py, rect_hw, rect_hh, pr;
    int any = 0;
    unsigned i;

    if (!mgr || !pl) return 0;
    pp = ce_player_pos();
    if (!pp) return 0;
    px = pp[0];
    py = pp[1];
    rect_hw = *(float *)(pl + CE_PLAYER_HITBOX_HW) + LOLK_GRAZE_RECT_MARGIN;
    rect_hh = *(float *)(pl + CE_PLAYER_HITBOX_HH) + LOLK_GRAZE_RECT_MARGIN;
    /* ★ 2026-09-10 第二十六轮：TH15 的擦弹半径比 TH18 的「普通圈」大 ——
     * `th15:0x455d00` 里当 `player+0x16294 & 0x10`（`Player__i_grow_bigger` `th15:0x457510`
     * 置的「变大」态）时：`r = player_scale × 3.6 × hit_radius`（0x3.6 是立即数，一手反编译）。
     * 我们不复刻「变大」动画，取该态的系数 ⇒ 擦弹圈 = `3.6 × 判定半径 + max(br/2.5,40)`，
     * 比 TH18 原生的 `r + …` 大约 10 px（用户口径：绀珠传的圈更大）。*/
    pr = *(float *)(pl + CE_PLAYER_HIT_RADIUS) * LOLK_GRAZE_RADIUS_SCALE;

    tint_restore_all();          /* 先把上一帧染过的弹还原 */

    for (i = 0; i < CE_BM_BULLET_COUNT; ++i) {
        uint8_t *b = mgr + CE_BM_BULLETS + i * CE_BULLET_STRIDE;
        uint32_t flags;
        float br, dx, dy;
        int inside = 0;

        if (*(uint16_t *)(b + CE_BULLET_STATE) == 0) continue;      /* 空槽 */
        flags = *(uint32_t *)(b + CE_BULLET_FLAGS);
        if (!(flags & CE_BULLET_FLAG_COLLIDE)) continue;            /* 不参与碰撞 */
        br = *(float *)(b + CE_BULLET_RADIUS);
        if (br <= 0.0f) continue;
        if (flags & CE_BULLET_FLAG_SCALED) br *= *(float *)(b + CE_BULLET_SIZE_SCALE);

        dx = *(float *)(b + CE_BULLET_POS) - px;
        dy = *(float *)(b + CE_BULLET_POS + 4) - py;

        if (flags & CE_BULLET_FLAG_CIRCLE) {
            float extra = br / LOLK_GRAZE_CIRCLE_DIV;
            float rr;
            if (extra < LOLK_GRAZE_CIRCLE_MIN) extra = LOLK_GRAZE_CIRCLE_MIN;
            rr = pr + extra;
            if (dx * dx + dy * dy < rr * rr + br * br) inside = 1;
        } else if (dx <= rect_hw && -dx <= rect_hw && dy <= rect_hh && -dy <= rect_hh) {
            inside = 1;
        }
        if (inside) {
            any = 1;
            tint_mark(b, (uint32_t *)(b + CE_BULLET_VM + CE_ANM_VM_COLOR1));
        }
    }
    return any;
}

/* 场上还有 boss 吗（boss_ids[4] 里有一个能按 id 找到活的敌人；符卡中一定算）*/


/* 擦弹圈：TH15 `effect.anm` script27 的 1:1 复刻
 *（`assets/ability/scripts/106_lolk_ring.anm.txt`：blendMode(2) 反向减算、
 *  alpha 0→96 用 30 帧淡入、4.3→2.0 用 12 帧由大变小）。
 * 判定区里有弹 → 起 VM 并每帧钉在自机；弹离开 → 触发 interruptLabel(1)，脚本自己 12 帧渐隐后自删。*/
static void ring_sync(lolk_card_t *s, int visible)
{
    const float *pp = ce_player_pos();

    if (visible) {
        if (!s->ring_id || !ce_anm_get_vm(s->ring_id)) {
            s->ring_id = ce_anm_spawn(CE_ABILITY_ANM(), CE_ANM_ABILITY_SCRIPT_LOLK_RING, 12);
            if (!s->ring_id) return;
            ce_log("lolk: graze ring spawned (anm id %u)", s->ring_id);
        }
        if (pp) ce_anm_set_pos(s->ring_id, pp[0], pp[1], 0.0f);
    } else if (s->ring_id) {
        ce_anm_interrupt(s->ring_id, 1);        /* 渐隐交给脚本 */
        s->ring_id = 0;
    }
}


/* HUD 文字位置与字体 = TH15 章节面板逐条照抄（`Gui::on_draw_2e` th15:0x436a6f 起）：
 *   五行右对齐（`align_h = 2`，即 AsciiManager 的「右对齐」模式）、行距 20，
 *   x = 300 / **308** / 300 / 300 / 300，y = 226 / 246 / 266 / 286 / **296**，
 *   font 2 / group 2（TH15 那五行设的就是这两个）。
 *   格式串与取值也一致：`"%d"` 擦弹 / `"%3.1f%%"` 击破率 / `"%3d"` / `"%6d"` 章节奖 / `"  +%d"`。
 *   第 5 行 TH15 印的是 PIV 增量（色 RGB 128,128,255）；本卡没有 PIV，改印分数增量，颜色照抄。
 *   ⚠️ 第 3 行 TH15 印的是 Pointdevice 重开次数，本卡没有 PD → 印本波击破数（标签是 TH15 的，
 *      这一行的语义是本卡自己定的，实机看到不对就改这里）。*/
#define LOLK_HUD_X        300.0f
#define LOLK_HUD_Y        226.0f
#define LOLK_HUD_DY        20.0f
/* ★★ 字体定案（2026-09-10，一手：`th15:0x436990` `Gui::on_draw_2e` 反编译）：
 * TH15 画面板那五行时写的是
 *     pzVar2->group = 2; pzVar2->font_id = 2; alignment_mode_h = 2; alignment_mode_v = 0;
 * （另有顶上那个大号章节奖数字走 `font_id = 4`、align 0/0），画完还原 `font_id = 0 / group = 0`。
 * ⇒ **五行 = 字体 2**。TH18 的 `draw_string` `0x41a2b0` 里字体 2/3 的字形映射与 TH15 逐条相同
 * （`'a'..'z' → char+0x74`、`'A'..'Z' → char+0x94`、`/ : - * % $ . +` → 0xce/0xcf/0xd0/0xd1/0xd2/0x101/0xd3/0xd4`），
 * 且 TH18 `ascii.anm` entry0 里 sprite 196..205（数字）确实是 8×10 那一排 ⇒ 跨作可直接用同一个 id。
 * ⚠️ 之前猜 -1（环境字体 = 0，14×14 小字）和 7/11 都不对；**字体 id 不是「页」**，
 * 页 = `vm+0x20`（`anm_loaded_index_2`，`create_string` 不抄、`draw_string` 现读）。
 * ★★ 2026-09-10 第三十二轮起**不再走 ASCII**：五行改成**贴图 VM**（`ce_hud_vm_text_set`），
 * 字形直接取字体 2 的原生切图（`make_font2_art.py`），所以「字体 2」这套映射现在体现在
 * `sdk.c` 的 `hud_vm_glyph()` 里，而不是环境 `font_id`。原因见下面的行注释：
 * ASCII 文字永远盖不到暂停模糊罩下面（组 1 的趟跑在 surface 1 + 相机 0 上，实跑不可见）。*/
/* 字号：★ **ASCII 文字空间就是 640×480 那一套**（我们的 UI VM 是它的 2×），
 * TH15 画面板文字时用环境 scale = **1.0**（`FUN_00416ae0` 重置成 1.0），
 * 所以这里也必须是 **1.0**；曾经写 2.0 → 实跑「数字被放大到至少 2 倍」（用户 2026-09-10）。*/
#define LOLK_TEXT_SCALE     1.0f
/* 面板 VM 走 UI 空间 = HUD 文字空间 ×2（股市 K 线面板同款）。*/

/* 面板几何照 TH15 `script241`：父 (448,402)、五条偏移 (0,0)/(-64,50)/(-64,90)/(-64,130)/(-64,170)，
 * 都在 resolutionMode(2) 的 2× 空间里 → 我们的 UI 空间就是这套值本身。*/
#define LOLK_PANEL_AX     639.5f       /* ★ 2026-09-10 第三十二轮：用户截图量「右移 191.5」⇒ 448+191.5 */
/* 面板 VM 走 UI 空间（1 单位 = 1 表面 px）*/
/* ★ 2026-09-10 第二十三轮：整体上移 47 UI px（用户实测）；第二十五轮：再下移 16 截图像素
 * （游戏 1280×960、屏幕 100% ⇒ 截图 px = UI px）⇒ 402 − 47 + 16 = **371**。
 * 数值行另外再下移 23 px（见 `LOLK_VAL_Y`）。*/
#define LOLK_PANEL_AY     401.5f       /* ★ 同上：用户「下移 30.5」⇒ 371+30.5 */
#define LOLK_PANEL_N          6
/* 五行数值：TH15 `Gui::on_draw_2e` 的坐标（x 见上、右对齐、行距 20），标签由美术印好。*/
#define LOLK_VAL_X        303.75f      /* ★ 第三十二轮：用户「右移 7.5 表面 px」⇒ +3.75 ASCII */
#define LOLK_VAL_X_RATE   311.75f        /* TH15 第 2 行单独往右 8 px（`%3.1f%%` 更宽）；+3.75 同左列 */
/* 面板下移 16 ⇒ 文字跟着 +8 字面量 px；再额外下移 23 ⇒ +11.5 ⇒ 226 − 23.5 + 8 + 11.5 = 222。*/
#define LOLK_VAL_Y        220.75f      /* ★ 第三十二轮：用户「上移 1.5 表面 px」⇒ −0.75 ASCII；
                                        *   追加亚像素「再上移 1px」⇒ 再 −0.5 ASCII */
#define LOLK_VAL_DY        20.0f
/* ★ TH15 第 5 行（蓝紫色 `+N`）与第 4 行只隔 **10** px（286 → 296），不是 20（一手反汇编 0x436b95/0x436bb8）。*/
#define LOLK_VAL_DY_PIV    10.0f
#define LOLK_HUD_WHITE    0xffffffffu    /* TH15：第 1..4 行都是白（color[0..3] = -1）*/
/* ★ 第 5 行那个蓝紫色 `+N`：TH15 `0x436bbf` 直接写 `[mgr+0x19224] = 0xff8080ff`，
 * 这是 **D3DCOLOR 0xAARRGGBB** ⇒ R=80 G=80 B=FF = 蓝紫。
 * ⚠️ 旧值 0xffff8080 把 R/B 写反了（画出来是粉色）——2026-09-10 订正。*/
#define LOLK_HUD_BLUE     0xff8080ffu
/* 第 5 行的数值 = `(gui+0x130 / 50000) × 10`（一手：`0x436bb3` 的魔数 0x14f8b589 + `SAR EDX,12`
 * 就是「除以 50000」，再 ×10）—— **它是章节奖换到的 PIV（最大得点）增量**，
 * 与「加进总分的 bonus/10」不是一回事（那条在 `0x43ad19`，不显示）。
 * ★ 2026-09-18：这一笔现在**真的写进共享 PIV 池**了 —— `apply_settle()` 里
 * `ce_piv_add(id, (bonus / 50000) * 10)`，与这一行**逐字同一个算式**（TH15 的 HUD 行与写点同源，
 * 见 `engine/faith/th15/01-point-item-value.md` §1.2 与「★ 章节奖全链」）⇒ 面板显示 = 池里记的账。*/
#define LOLK_PIV_DIV      50000
#define LOLK_ROLL_START    90           /* TH15：面板出现 90 帧后才开始跳（gui+0x1a4 >= 0x5a）*/
#define LOLK_SE_TICK     0x27u          /* se_kira01：跳动时每 4 帧一声（th15:0x4354e5）*/
#define LOLK_SE_DONE     0x2fu          /* se_bonus：跳完收尾（th15:0x435514）*/
#define LOLK_HUD_HOLD     300           /* 结算面板保留帧数（~5 s，之后自己消失）*/

static void spawn_item_at(uint32_t type, float x, float y, float speed)
{
    float p3[3] = { x, y, 0.0f };
    if (!ce_spawn_item(type, p3, -1.5707964f, speed))
        ce_log("lolk: spawn frag type 0x%x failed (pool full?)", type);
}

static void apply_settle(ce_card_t *c, lolk_card_t *s, const lolk_settle_t *out)
{
    const float *pp;
    int32_t piv_add;

    if (out->score > 0) {
        ce_add_score_delta(out->score);
    }
    /* ★★ 章节奖 → **共享 PIV 池**（TH15 路径，显示量纲；接缝 = 本卡现成的「一章结算」点，即本函数）。
     *
     * 一手算式（`engine/faith/th15/01-point-item-value.md` 的「★ 章节奖全链」+ §4.1 第 2 行 + §4.2）：
     *   例程 `th15:0x43ab70`（GUI 侧章节结算，调用者 `th15:0x43d0d0`）先算
     *     `base = trunc10(章内擦弹 × 击破率％ × 50)`
     *     （`th15:0x43ac94` 擦弹 × pct → `th15:0x43aca0` ×50.0 → `th15:0x43acac/43acb8` trunc10 → `gui+0x12c`），
     *   再在 `th15:0x43ad1f`–`th15:0x43ad58` 把它换成最大得点：
     *     `÷50000`（魔数 `0x14f8b589` + `sar 12`）→ `×10`（`lea eax,[eax+eax*4]` + `add eax,eax`）
     *     → `×100.0f`（`th15:0x4cffe8`）→ `th15:0x43ad58` 落笔
     *   ⇒ **PIV += 1000 × ⌊base/50000⌋（存储）= (base/50000)×10（显示）**。
     *     ⚠ 文档里那句「`⌊base/5000⌋` 显示」是同一个量的化简写法：引擎做的是「先除 50000、再 ×10」的
     *     **两步取整**，base 不是 5000 的整数倍时两者差 ≤9 显示 ⇒ 这里以指令序列为准（也与本卡第 5 行
     *     HUD 的算式逐字相同：TH15 `th15:0x436bb3` 画的正是 `(gui+0x130/50000)×10`，它就是这一笔 PIV）。
     *   `base` 就是本卡已经算好的 `out->bonus` = `lolk_bonus(out->graze, out->rate)`
     *   （`cards/lolk_core.c`：`trunc10(擦弹 × 击破率％ × 50)`，同一个 TH15 算式）。
     *
     * ⚠ 击破率用的是本卡的**近似**：`out->rate` 由 `enemy_count_real` 差分得到（道中 = 消失/刷出、
     *   boss 相位 = 击破 100 / 超时 0，见 `lolk_core.h` 的「## 分母的近似」）。TH18 没有 TH15 的
     *   `ENEMIES_SPAWNED_IN_CHAPTER` / `ENEMIES_DESTROYED_IN_CHAPTER`（`th15:0x4e7484` / `th15:0x4e7488`）
     *   两个全局 ⇒ **这一项不可能一手精确**；但这个近似与面板的 bonus / 击破率是**同一个数**，
     *   所以面板与池不会互相打脸，也不额外凑第二套计数。
     *
     * ⚠ **闸由层管，本卡不夹**（`ce_piv_add` 按本卡上限 `cap_i − init_i` 累计夹；上限在 `on_tick_2` claim）。*/
    piv_add = (out->bonus / LOLK_PIV_DIV) * 10;      /* LOLK_PIV_DIV = 50000（TH15 的 ÷50000 魔数）*/
    if (piv_add > 0) {
        ce_piv_add(ce_card_id(c), piv_add);
    }
    if (out->frag_type) {
        pp = ce_player_pos();
        spawn_item_at((uint32_t)out->frag_type,
                      pp ? pp[0] : 0.0f, pp ? pp[1] - 76.0f : 0.0f, 1.2f);
    }
    s->last = *out;
    s->last_age = 0;
    s->waves_total++;
    s->fade_t = -1;                     /* ★ 2026-09-21：新的一波 ⇒ 面板重新开一轮（没在退场）*/
    /* TH15 的跳动参数：剩余量 = 击破率％、步长 = bonus/rate、显示值从 0 起跳。*/
    s->roll_left = out->rate > 0 ? out->rate : 1;
    s->roll_step = out->rate > 0 ? out->bonus / out->rate : out->bonus;
    s->roll_shown = 0;
    s->roll_active = 1;
    ce_log("lolk: wave %d (%s %s) settled: graze %d, killed %d/%d = %d%%, bonus %d, score +%d, frag 0x%x, piv +%d",
           out->wave_index, out->kind == LOLK_KIND_BOSS ? "boss" : "mid",
           out->is_spell ? "spell" : "nonspell", out->graze, out->killed,
           out->spawned, out->rate, out->bonus, out->score, out->frag_type, piv_add);
    (void)c;
}

/* 波次面板：TH15 `front.anm` script241 的五个子 VM（五条 192×32 的条，各带 scale(0,1)→(1,1) 展开）。
 * 结算时一起起出来，到点一起 interruptLabel(1) 渐隐自删。*/
static const int LOLK_PANEL_SCRIPT[LOLK_PANEL_N] = {
    CE_ANM_ABILITY_SCRIPT_LOLK_PANEL1, CE_ANM_ABILITY_SCRIPT_LOLK_PANEL2,
    CE_ANM_ABILITY_SCRIPT_LOLK_PANEL3, CE_ANM_ABILITY_SCRIPT_LOLK_PANEL4,
    CE_ANM_ABILITY_SCRIPT_LOLK_PANEL5, CE_ANM_ABILITY_SCRIPT_LOLK_PANEL6,
};
static const float LOLK_PANEL_DX[LOLK_PANEL_N] = { 0.0f, -64.0f, -64.0f, -64.0f, -64.0f, 0.0f };
static const float LOLK_PANEL_DY[LOLK_PANEL_N] = { 0.0f,  50.0f,  90.0f, 130.0f, 170.0f, 240.0f };

/* 面板**可见块**的几何辅助（定义在下面 2026-09-21 那段之后；这里先声明，`panel_show()` 的日志用）。*/
static unsigned panel_bar_count(const lolk_card_t *s);
static int panel_rect(const lolk_card_t *s, hud_dim_rect_t *r);

/* 第 6 条（碎片奖励条）有两张：TH15 `script242` = sprite150 = **绿色（符卡碎片）**、
 * `script243` = sprite151 = **粉色（残机碎片）**（2026-09-09 像素实测坐实；早前把这两条记反了）。
 * 卡按本次奖励类型二选一 —— 否则「掉残机碎片却显示绿色符卡碎片条」。*/
static int panel_script(unsigned i, const lolk_settle_t *last)
{
    if (i != 5) return LOLK_PANEL_SCRIPT[i];
    return (last->frag_type == LOLK_FRAG_LIFE) ? CE_ANM_ABILITY_SCRIPT_LOLK_PANEL7
                                               : CE_ANM_ABILITY_SCRIPT_LOLK_PANEL6;
}

static void panel_show(lolk_card_t *s)
{
    unsigned i;
    for (i = 0; i < LOLK_PANEL_N; ++i) {
        if (i == 5 && s->last.frag_type == 0) continue;   /* 没碎片奖励就不起那条 */
        if (s->panel_id[i] && ce_anm_get_vm(s->panel_id[i])) continue;
        s->panel_id[i] = ce_anm_spawn_ui(CE_ABILITY_ANM(), panel_script(i, &s->last), 0);
        if (!s->panel_id[i]) continue;
        /* ★ 2026-09-10 第三十一轮：面板留在 **UI 表面**（脚本层 24 → 绘制组 37 → 优先级 `0x39`），
         * 但位置挪进**游玩区矩形**（表面 x 256..1024 / y 32..928）。
         * 理由：暂停的模糊罩是 `text.anm` script59（组 38 → `0x3e`），**只盖游玩区矩形**；
         * 组 37 比它早画 ⇒ 暂停时面板被罩住（= 用户要的「整体模糊」）；
         * 而 UI 表面是全分辨率，平时面板不会像画在游玩区 RT（408×472 放大到屏幕）那样发虚。
         * 坐标：相机 2（UI）原点在表面左上角 ⇒ 直接用 TH15 的 `(AX+DX, AY+DY)`。
         * ⚠️ **这个 pos 是该条的「图心」不是左上角**（六支脚本都不写 `anchor` ⇒ `anchor(0,0)` = 居中；
         *    一手与两条旁证见下面 2026-09-21 那段）—— 判据（`panel_rect()`）必须按图心算。*/
        ce_anm_set_pos(s->panel_id[i], LOLK_PANEL_AX + LOLK_PANEL_DX[i],
                       LOLK_PANEL_AY + LOLK_PANEL_DY[i], 0.0f);
        if (i == 0) {
            hud_dim_rect_t r0;
            if (panel_rect(s, &r0)) {
                ce_log("lolk: chapter panel spawned (anm id %u, %u bars, 可见块 表面 x %.1f..%.1f y %.1f..%.1f)",
                       s->panel_id[0], panel_bar_count(s),
                       r0.x0 * 2.0f + 640.0f, r0.x1 * 2.0f + 640.0f,
                       r0.y0 * 2.0f + 32.0f, r0.y1 * 2.0f + 32.0f);
            } else {
                ce_log("lolk: chapter panel spawned (anm id %u)", s->panel_id[0]);
            }
        }
    }
}

/* ★ 面板「玩家贴近就淡」—— 本卡自定行为（TH15 原版没有）：绀珠传把波次拉长，面板会长时间挡在
 * 游玩区左侧，玩家贴上去看不清弹。
 *
 * ★★ 2026-09-15g：这一路按用户口径「UI 统一」改造，**动态统一、区域按本卡自己的几何**：
 *   · 性质 = ❓ **用户口径（UI 统一），不是复刻**（用户 2026-09-15：「压暗这个涉及到 UI 统一，
 *     这个不能复刻。是那个靠近透明吧」；父 agent 2026-09-15g 的读法：「压暗的本质 = 别让 HUD 挡住自机；
 *     HUD 画在哪，判据就跟着哪」）。
 *   · **动态**（统一）：终点 alpha **32**（原 76）、起始 255、每帧 **±56**（原 8）、滞回带 **16 px**。
 *   · **区域**（本卡几何）：面板自己的矩形 —— 面板 VM 走 UI 空间（1 单位 = 1 表面 px），
 *     六条 256×32、x 偏移 `LOLK_PANEL_DX` ∈ {0, −64}、y 偏移 `LOLK_PANEL_DY` ∈ {0, 50, 90, 130, 170, 240}
 *     ⇒ 表面 x 575.5..895.5、y 401.5..673.5（= 320 × 272）
 *     ⇒ 自机坐标（`自机 = 表面/2 − (320, 16)`，见 `hud_dim.h`）= x ∈ [−32.25, 127.75]、y ∈ [184.75, 320.75]
 *   · ★★ 2026-09-15h：**进/出的口径订正**（父 agent：15g 那版「进 = 矩形**之内**」裁错了）——
 *     **进 = 自机落在「矩形 + `HUD_DIM_MARGIN`(28)」之内**、**出 = 离开「矩形 + 44」（28 + 滞回 16）之外**；
 *     理由：一手各作的门本来就落在它们自己那块上沿之上 24–32 px（TH16 的 `400` 在它那块沿之上 28、
 *     TH11 的 `416` = 它那块沿 + 24）⇒ 边距是**原本就有的语义**（本卡这块整片都在自机够得着的范围内，
 *     所以边距只让「靠近」更早成立，不再出现"站不进去"的问题）。
 *     ⇒ **进**：x ∈ [−60.25, 155.75] ∧ y ∈ [156.75, 348.75]（自机钳位 x ∈ [−184, 184]、y ∈ [32, 432] ⇒ 全在范围内 ✓）
 *     ⇒ **出**：x < −76.25 ∨ x > 171.75 ∨ y < 140.75 ∨ y > 364.75（**16 px 滞回带**）
 *   · **面板几何一个字节没动**（`LOLK_PANEL_AX/AY/DX/DY` 与 `panel_show()` 的摆位照旧）。
 *   ⚠️ 父 agent 先前那条「115 判定用左下角那套、与面板画在哪无关」**已作废**（2026-09-15g 撤回）。
 *   ⚠️ 原来的那一套（= **本卡原实现**，TH15 原版没有它、它连一手都不是；留档，不再生效）：
 *     终点 `LOLK_PANEL_FADE_ALPHA 76`（≈0.3）、步长每帧 8、淡出带 `x ∈ (−64, 160)`、
 *     `y ∈ (137.5, 337.5)`（= 面板矩形各放 32 px 余量后跟着面板上下移动改过：153/353 → 137.5/337.5）。
 *
 * ========== ★★ 2026-09-21（P9 之二 · owner 实跑 §th15 #1/#2/#3）三条一起重做 ==========
 *
 * owner 原话（[`SINGLE-CARD-TEST.md`](../../SINGLE-CARD-TEST.md) §th15）：
 *   ①「chapter finish 防挡透明改成 50%」；②「放挡触发框似乎比 UI 位置靠下」；
 *   ③「chapter finish 防挡似乎没有恰当的淡入淡出过渡」（同一条更早的记载在
 *   [`CARD-118-12IN1-REPORT.md`](../../CARD-118-12IN1-REPORT.md) B8：「防遮挡淡入淡出太硬（瞬间）」）。
 * 逐条「一手出处 → 改法」如下（**常量表与实跑口径另见 `TUNING.md` §7.5**）：
 *
 * ② **触发框比 UI 靠下** —— 根因：这块矩形**两处都算错了**（都把 VM pos 当成了左上角）：
 *    · **锚点 = 图心**：TH15 `front.anm` script241 的七个子脚本 244–248 / 242 / 243 **都不写 `anchor`**
 *      （同文件其它 UI 脚本要角锚时会显式写，如 script2 / script6 的 `anchor(1,1)`）⇒ 保持 `wipe`
 *      的清零值 `anchor(0,0)`，而锚点表里 `(0,0)` = **居中**
 *      （[`engine/anm/th18/02-render-stages.md`](../../../engine/anm/th18/02-render-stages.md)）。
 *      ⇒ `ce_anm_set_pos()` 写的 pos 是**该条的中心**。两条旁证：
 *      ⓐ 2026-09-09 实机：192 宽的内容贴在 256 画布**左**侧时用户看到整条**偏左**、居中后就正了
 *        （`assets/ability/make_lolk_ring_art.py` 注释）—— 只有图心锚会有这个症状（左锚应当偏右）；
 *      ⓑ TH15 自己的数据：`Gui::on_draw_2e` 五行文字 y = 226/246/266/286（640 空间）×2 =
 *        452/492/532/572，与四个标签子脚本 245–248 的 `pos` y（402+50/90/130/170）**逐个相等**
 *        ⇒ 文字落在条的**中心线**上（按左上角算的话它们会贴在条的上沿）。
 *    · **可见尺寸 = 192×32**：我们的贴图是 256×32 画布 + **居中的 192×32 原生内容**
 *      （`make_lolk_ring_art.py`，图心锚下可见像素与 TH15 逐像素同位）⇒ 判据要用 192×32；
 *      旧代码按画布 256 算宽（`RECT_W = 256 + 64`）⇒ 又多算了 32。
 *    ⇒ 可见外接盒（表面 px）= 各条 `(AX+DX[i] ± 96, AY+DY[i] ± 16)` 的并集
 *      = **x [479.5, 735.5]、y [385.5, 657.5]**（第 6 条碎片条在场时；不在场则下沿 587.5）。
 *      旧矩形 `(575.5, 673.5) + 320×272` ⇒ **右偏 96–160 px、下偏 16 px**，与 owner 的「靠下」吻合。
 *      ⇒ 现在进 = 矩形 + 28、出 = 矩形 + 44（自机坐标 x [−108.25, 75.75] / y [148.75, 340.75]）。
 *
 * ③ **过渡** —— 原来 C 侧每帧 ±56（**4 帧**）= owner 说的「太硬（瞬间）」。改法 = 借**这块面板自己的**
 *    一手 alpha 过渡值：TH15 script241（面板父 VM）被关掉时走的是
 *      `interruptLabel(1); alphaTime(20, 0, 0); +20: delete();`
 *    （一手：`th15:0x43b0f0` 把 label 1 发给 `gui+0x11c` 的面板树；`th15:0x435561` 在显示计时
 *      `gui+0x1a4 >= gui+0x1b4` 成立时调它 —— mode 0 的 `+0x1b4` = `0x122` = **290 帧**；
 *      `alphaTime` 的 mode 0 = **线性**）；★ 2026-09-22 起按原生一手 = **8 帧线性 / 终点 64**；淡回按对称用同一条。
 *    ★ 同一手还揭出**五行数值的淡出**：TH15 里这五行的 alpha **就是面板父 VM 的 alpha 字节**
 *      （`th15:0x436ae6` `mov al,[eax+0x53b]` → AsciiManager `+0x19227`；`+0x53b` = `color_1.A`，
 *      一手 `AnmVm::step_interpolators` `th15:0x47becf` 写它）= 父 VM 的 `alphaTime(20,0,0)`
 *      ⇒ **六条 10 帧 / 五行走 20 帧**。原实现是「条 10 帧 + 数值**当场 clear**」⇒ 数值根本没有淡出，
 *      这正是 owner 说的「没有恰当的淡入淡出过渡」。现在 20 帧由 C 打（我们没有父 VM）。
 *      ⚠️ **出现（淡入）一侧不动**：TH15 的面板本来就没有 alpha 淡入（六条是 `scale(0,1)` →
 *      `scaleTime(10, 0, 1, 1)` 的横向展开，五行从第一帧就是满 alpha）✓ 我们已照抄。
 *
 * ① **终点 50%** —— 纯数值件：终点 alpha **32 → 128**（❓ owner 口径 2026-09-19）。
 *    ★★ 2026-09-21（同日稍后）**统一收口**：owner 拍定「20 帧 / 50% 是 UI ⇒ 得**统一成一个值**，
 *    以哪个为基准都行」⇒ 父 agent 选**以本卡这套为基准**，**共享件 `hud_dim.{h,c}` 已改成
 *    终点 64 / 8 帧线性**，另外五张卡（116/117/107/113/110+111）跟着一起变。
 *    ⇒ 本卡**不再自持**终点与过渡帧数（`LOLK_PANEL_DIM_ALPHA` / `LOLK_PANEL_DIM_FRAMES` 已删），
 *      改回调共享件的 `hud_dim_tick()`。权威口径 = `hud_dim.h` 文件头 + `TUNING.md` §7.1/§7.5。*/

/* ---- 面板**退场**（关面板）时五行数值的淡出帧数（复刻 TH15 **面板父 VM** 的 `alphaTime(20, 0, 0)`）----
 * ⚠ 这**不是**防遮挡那条统一动态（那条已归共享件 `HUD_DIM_FRAMES`）：它是「面板到点关闭」这条独立路径，
 *   与共享件的 20 帧**同源**（都是 TH15 script241 的 `alphaTime(20,0,0)`）但机制不同 ⇒ 留一个本卡常量。
 *   六条标签条的退场淡出仍归**脚本自己**（`interruptLabel(1); alphaTime(10, 0, 0)`，TH15 原样）。*/
#define LOLK_PANEL_FADE_FRAMES    20

/* 面板可见块：**原生内容的 192×32**（不是 256×32 的画布 —— 见上面 ② 的「可见尺寸」）。*/
#define LOLK_PANEL_ART_W         192.0f
#define LOLK_PANEL_ART_H          32.0f

/* ★ 五行的数值文字（贴图 VM）—— 状态放 DLL 静态区（本卡同时只会有一张在场）*/
#define LOLK_ROW_N 5
static ce_hud_vm_text_t s_rows[LOLK_ROW_N];

/* 本帧面板有几条在场：第 0..4 条（标签/装饰）恒起；第 5 条（碎片奖励条）只在本次结算发了碎片时起
 *（与 `panel_show()` 的 `i == 5 && frag_type == 0 → continue` 同一个判据）。*/
static unsigned panel_bar_count(const lolk_card_t *s)
{
    return (s->last.frag_type != 0) ? (unsigned)LOLK_PANEL_N : (unsigned)(LOLK_PANEL_N - 1);
}

/* 面板**可见块**的外接盒（表面 px）→ 自机坐标矩形（共享件收「左下角 + 宽高」，高往上长）。
 * 每帧现算（碎片条在不在场会改变下沿）——与 `hud_dim.h` 的使用纪律一致（别缓存成常量）。*/
static int panel_rect(const lolk_card_t *s, hud_dim_rect_t *r)
{
    float x0 = 1e9f, x1 = -1e9f, y0 = 1e9f, y1 = -1e9f;
    unsigned i, n = panel_bar_count(s);

    if (!r || n == 0) return 0;
    for (i = 0; i < n; ++i) {
        float cx = LOLK_PANEL_AX + LOLK_PANEL_DX[i];      /* 图心锚 ⇒ 条的中心 */
        float cy = LOLK_PANEL_AY + LOLK_PANEL_DY[i];
        if (cx - LOLK_PANEL_ART_W * 0.5f < x0) x0 = cx - LOLK_PANEL_ART_W * 0.5f;
        if (cx + LOLK_PANEL_ART_W * 0.5f > x1) x1 = cx + LOLK_PANEL_ART_W * 0.5f;
        if (cy - LOLK_PANEL_ART_H * 0.5f < y0) y0 = cy - LOLK_PANEL_ART_H * 0.5f;
        if (cy + LOLK_PANEL_ART_H * 0.5f > y1) y1 = cy + LOLK_PANEL_ART_H * 0.5f;
    }
    hud_dim_rect_surface(r, x0, y1, x1 - x0, y1 - y0);
    return 1;
}

/* ② 一帧一次：防遮挡的**全部动态都归共享件** —— 判据（进 = 矩形 + 28、出 = 矩形 + 44；滞回 16 px）
 * 与过渡（终点 64 / 8 帧线性）都在 `hud_dim_tick()` 里（`hud_dim.h` 文件头 + `TUNING.md` §7.1）。
 * 本卡只负责：每帧现算**自己那块可见块**的矩形、把返回的 alpha 写进六条、并在闩锁翻转时打一行日志。*/
static int32_t panel_alpha_sync(lolk_card_t *s)
{
    hud_dim_rect_t r;
    const float *pp = ce_player_pos();
    int32_t was = s->hud.dim, a;
    uint32_t au;
    unsigned i;

    if (!panel_rect(s, &r)) return s->hud.alpha;
    {   float bx, by, bw, bh;                                  /* ★ 2026-10-03q：**一块 HUD** */
        ce_hud_band_rect(&bx, &by, &bw, &bh);
        if (bw > 0.0f && bh > 0.0f) hud_dim_rect_surface(&r, bx, by, bw, bh); }
    a = hud_dim_tick(&s->hud, &r, pp);
    if (s->hud.dim != was) {                 /* 闩锁翻转（= 玩家进/出触发区）—— 日志用 */
        ce_log("lolk: panel dim %s (alpha %d, rect 表面 x %.1f..%.1f y %.1f..%.1f, 自机 %.1f,%.1f)",
               s->hud.dim ? "ON" : "OFF", (int)a,
               r.x0 * 2.0f + 640.0f, r.x1 * 2.0f + 640.0f,
               r.y0 * 2.0f + 32.0f, r.y1 * 2.0f + 32.0f,
               pp ? pp[0] : 0.0f, pp ? pp[1] : 0.0f);
    }
    au = (uint32_t)a & 0xffu;
    for (i = 0; i < LOLK_PANEL_N; ++i) {
        if (s->panel_id[i]) ce_anm_set_color(s->panel_id[i], (au << 24) | 0x00ffffffu);
    }
    return a;
}

/* 六条：发中断 1 ⇒ **脚本自己**淡出（244–248/242/243 的 `interruptLabel(1); alphaTime(10,0,0);
 * +10: delete()`，TH15 原样、一个字节没动）。发完就把句柄清零 ⇒ 之后不再写它们的颜色
 *（否则 C 会和脚本的 `alphaTime` 抢同一路 `color_1`）。*/
static void panel_interrupt_bars(lolk_card_t *s)
{
    unsigned i;
    for (i = 0; i < LOLK_PANEL_N; ++i) {
        if (s->panel_id[i]) { ce_anm_interrupt(s->panel_id[i], 1); s->panel_id[i] = 0; }
    }
}

static void panel_rows_clear(void)
{
    unsigned i;
    for (i = 0; i < LOLK_ROW_N; ++i) ce_hud_vm_text_clear(&s_rows[i]);
}

/* 立刻收干净（换关 / 局末 / 删卡）：六条发中断（脚本 10 帧后自删）+ 五行当场 clear。*/
static void panel_hide(lolk_card_t *s)
{
    panel_interrupt_bars(s);
    panel_rows_clear();
}

/* 防遮挡状态复位（换关 / 局末）：闩锁归零、alpha 回**满亮 255**、过渡计数清掉。
 * 共享件的 `hud_dim_init()` 做全部四格（dim = 0、alpha = from = 255、t = HUD_DIM_FRAMES 稳态）；
 * ⚠️ 不复位成 0 —— `ce_state` 是 memset 出来的，alpha = 0 会让开场那几帧面板是全黑的。*/
static void panel_dim_reset(lolk_card_t *s)
{
    hud_dim_init(&s->hud);
}

/* ------------------------------------------------------------------ 行为 */

static int on_tick_2(ce_card_t *c)
{
    /* ★ 2026-09-18（用户口径）：向**共享 PIV 池**声明本卡在场 + 本卡的可写上限（`cap_i − init_i`，按难度查表）。
     *   一行、幂等；退场由 `ce_layer_drop_card()` 兜底。擦弹那一项是**共用写入者**（只算一次）⇒ 这里只声明族籍。
     *   口径与表的一手出处：`engine/faith/OVERVIEW.md`「结论：th12–th17 可以共用一个 PIV」+ `sdk_core.h`。*/
    ce_piv_claim_auto(ce_card_id(c), ce_difficulty());
    ce_graze_family_claim(ce_card_id(c));

    lolk_card_t *s = ce_state(c, lolk_card_t);
    lolk_settle_t out;
    lolk_in_t in;
    int32_t g, ec, f, d, spawn_delta = 0;
    int grazed = 0;
    int in_box;
    int ev;

    if (!s) return 0;

    /* ① 擦弹：原生计数差分（同帧多次擦弹只差一个数，但总数不漏）*/
    g = graze_read();
    if (s->last_graze >= 0 && g > s->last_graze) {
        lolk_add_graze(&s->st, g - s->last_graze);
        grazed = 1;
    }
    s->last_graze = g;

    /* ② 刷出 / 消失（enemy_count_real 差分） */
    ec = enemy_count_read();
    if (ec >= 0) {
        if (s->last_enemies >= 0) {
            d = ec - s->last_enemies;
            if (d > 0)      lolk_add_spawned(&s->st, d);
            else if (d < 0) lolk_add_killed(&s->st, -d);
        }
        s->last_enemies = ec;
    }

    /* ②b 道中信号：本帧新建敌人数（next_enemy_id 差分；它只增不减，比 count 差分准）*/
    d = spawn_id_read();
    if (d >= 0) {
        spawn_delta = (s->last_spawn_id >= 0 && d >= s->last_spawn_id) ? d - s->last_spawn_id : 0;
        s->last_spawn_id = d;
    } else {
        spawn_delta = 0;
    }

    /* ③ 减速：擦弹事件 **或** 判定框里有敌方子弹 → 本帧该慢；圈跟着亮 */
    in_box = bullet_in_graze_zone();
    f = lolk_slowdown_step(&s->st, grazed || in_box);
    slow_apply(grazed || in_box);
    ring_sync(s, in_box);

    /* ④ 波边沿（一段攻击 = 一波）：boss 按相位/符卡位翻转；道中按「章 = MainSub 编号」 */
    phase_read(&in);
    in.spawn_delta = spawn_delta;
    {
        int32_t ssub = -1, sch = -1;
        (void)stage_ecl_read(&ssub, &sch);      /* ★ MainSub 编号（去掉尾部字母）*/
        in.stage_sub = ssub;
        in.stage_chapter = sch;
    }
    ev = lolk_tick(&s->st, &in, &out);
    if (ev & LOLK_EV_WAVE_END) {
        apply_settle(c, s, &out);
    }
    if (ev & LOLK_EV_WAVE_START) {
        ce_log("lolk: wave %d start (%s%s, chapter %d, sub %d, mainsub %d [rule %s], sig %d, graze/slow %d%%)",
               s->st.waves_done + 1,
               s->st.wave_kind == LOLK_KIND_BOSS ? "boss " : "mid ",
               s->st.wave_is_spell ? "spell" : "nonspell",
               in.chapter, in.stage_sub, in.stage_chapter, lolk_mid_rule_name(s->st.mid_rule),
               in.phase_sig, f);
    }

    /* 数字跳动（TH15 `Gui::on_tick` th15:0x4354a0 段）：面板出现 90 帧后开始，
     * 每帧显示值 += bonus/rate、剩余量 -= 1，每 4 帧一声 se_kira01；跳完对齐到 bonus + se_bonus。*/
    if (s->roll_active && s->last_age >= LOLK_ROLL_START) {
        if (s->roll_left > 0) {
            s->roll_shown += s->roll_step;
            s->roll_left--;
            if ((s->roll_left & 3) == 0) ce_play_sound(LOLK_SE_TICK, ce_player_x());
        } else {
            s->roll_shown = s->last.bonus;
            s->roll_active = 0;
            ce_play_sound(LOLK_SE_DONE, ce_player_x());
        }
    }

    if (s->last_age >= 0 && s->last_age < 1000000) s->last_age++;
    return 0;
}

/* ⚠️ 2026-09-09 用户实跑：**不要**把收道具算进本章擦弹数。
 * TH15 的 `ItemManager__on_tick__body` `th15:0x43f830` 确实给道具 type 13/14/15 各 add_graze(5/10/25)，
 * 但那是**稀有高分点**；TH18 的击破/消弹会把弹幕成片转成 9..0xe 的转换点，收一片就 +几百
 * （实测本章擦弹数冲到 ~1000）。所以本卡只认原生擦弹计数，不再补道具擦弹。*/

/* 五行数值（**只画数值**：标签在 TH15 的面板美术里 = front02.png 那六条）。
 * 逐条照 TH15 `Gui::on_draw_2e`：`"%d"` / `"%3.1f%%"` / `"%3d"` / `"%6d"` / `"  +%d"`。
 * `a` = 本帧 alpha（正常显示 = 防遮挡 alpha；退场那 20 帧 = C 打的淡出值）。
 * ★ 2026-09-21（③）：TH15 里这五行的 alpha **就是面板父 VM 的 alpha 字节**（一手 `th15:0x436ae6`
 *   读 `vm+0x53b` 写进 AsciiManager 的色字节）⇒ 画面板时它跟面板的防遮挡 alpha 走**同一个值**，
 *   关面板时跟父 VM 的 `alphaTime(20, 0, 0)` 走 —— 也就是这里的两种取值是同一个来源。*/
static void panel_rows_draw(const lolk_card_t *s, uint32_t a)
{
    int32_t shown = s->roll_active ? s->roll_shown : s->last.bonus;
    char buf[16];
    uint32_t A = (a & 0xffu) << 24;

    /* ★★ 2026-09-10 第三十二轮：五行从 **ASCII 文字**改成**贴图 VM**（用户定案）。
     * 理由：ASCII 永远盖不到暂停模糊罩下面（组 1 的趟 `0x419950` 跑在 surface 1 + 相机 0 上，
     * 实跑不可见；其余三趟都晚于罩子的 `0x3e`）—— 面板本体已经会被罩住，字却不会，很扎眼。
     * 现在走 `ce_hud_vm_text_set`：字形 = TH18 font 2 的原生切图（VM 挂脚本 136，UI 层 25）。
     * 排版仍是 TH15 `Gui::on_draw_2e` 那一套（字体 2 = 字距 7、`%3.1f%%` 的 `.` 是 4、右对齐 2）。*/
    #define LOLK_ROW(i, xx, yy, col, fmt, ...)                                      \
        do {                                                                          \
            snprintf(buf, sizeof buf, fmt, __VA_ARGS__);                              \
            ce_hud_vm_text_set(&s_rows[i], (xx), (yy), A | ((col) & 0xffffffu),       \
                               CE_ASCII_ALIGN_RIGHT, LOLK_TEXT_SCALE, buf, "lolk");   \
        } while (0)

    /* 第 1 行：本章擦弹数（TH15 gui+0x120）*/
    LOLK_ROW(0, LOLK_VAL_X, LOLK_VAL_Y, LOLK_HUD_WHITE, "%d", (int)s->last.graze);
    /* 第 2 行：击破率 —— TH15 的跳动就是**这一行往下减**（gui+0x124 从 rate 减到 0），
     * 减完再由 `gui+0x124 = gui+0x128` 弹回原值（th15:0x435519）。*/
    LOLK_ROW(1, LOLK_VAL_X_RATE, LOLK_VAL_Y + LOLK_VAL_DY, LOLK_HUD_WHITE, "%3.1f%%",
             (double)(s->roll_active ? s->roll_left : s->last.rate));
    /* 第 3 行：本波击破数（TH15 这一行是 Pointdevice 重开数）*/
    LOLK_ROW(2, LOLK_VAL_X, LOLK_VAL_Y + LOLK_VAL_DY * 2, LOLK_HUD_WHITE, "%3d",
             (int)s->last.killed);
    /* 第 4 行：章节奖（TH15 gui+0x130，从 0 滚到 bonus）*/
    LOLK_ROW(3, LOLK_VAL_X, LOLK_VAL_Y + LOLK_VAL_DY * 3, LOLK_HUD_WHITE, "%6d", (int)shown);
    /* 第 5 行：**蓝紫色的 PIV 增量** —— TH15 `0x436bb3` 算的是 `(gui+0x130/50000)×10`，
     * 和「加进总分的 bonus/10」不是一回事（后者在 `0x43ad19`，面板上不显示）。
     * 位置只比第 4 行低 10 px（不是 20）。*/
    LOLK_ROW(4, LOLK_VAL_X, LOLK_VAL_Y + LOLK_VAL_DY * 3 + LOLK_VAL_DY_PIV, LOLK_HUD_BLUE,
             "  +%d", (int)((shown / LOLK_PIV_DIV) * 10));
    #undef LOLK_ROW
}

static int on_draw(ce_card_t *c)
{
    lolk_card_t *s = ce_state(c, lolk_card_t);
    int32_t a;

    if (!s) return 0;
    if (!*(volatile uint32_t *)CE_ADDR_GAME_THREAD_PTR) return 0;   /* 主菜单 / 标题：什么都不画 */
    if (s->waves_total == 0 || s->last_age < 0) return 0;           /* 没结算过 = 一块板都不起 */

    /* ★ 2026-09-21（③）：到点后**先淡 20 帧再收**（TH15 关面板的 `alphaTime(20,0,0)`）。
     *   六条由脚本自己淡（各自 `alphaTime(10,0,0)`，`panel_interrupt_bars()` 只发一次中断）；
     *   五行数值没有父 VM ⇒ 这 20 帧由 C 打，淡完才 clear（原来是一到点就 clear = 没有淡出）。*/
    if (s->last_age > LOLK_HUD_HOLD) {
        if (s->fade_t < 0) {                        /* 退场第一帧：发中断 + 记起点 */
            s->fade_t = 0;
            s->fade_from = s->hud.alpha;
            panel_interrupt_bars(s);
            ce_log("lolk: chapter panel closing (bars fade 10f by script, rows fade %df by C)",
                   (int)LOLK_PANEL_FADE_FRAMES);
        }
        if (s->fade_t >= LOLK_PANEL_FADE_FRAMES) {  /* 淡完：收掉五行 */
            panel_rows_clear();
            return 0;
        }
        a = s->fade_from * (LOLK_PANEL_FADE_FRAMES - s->fade_t) / LOLK_PANEL_FADE_FRAMES;
        s->fade_t++;
        panel_rows_draw(s, (uint32_t)a);
        return 0;
    }

    panel_show(s);
    a = panel_alpha_sync(s);        /* ② 触发框按可见块现算；③ 防遮挡 alpha（终点 64 / 8 帧线性）*/
    panel_rows_draw(s, (uint32_t)a);
    return 0;
}

static void on_stage_start(ce_card_t *c)
{
    lolk_card_t *s = ce_state(c, lolk_card_t);
    if (!s) return;
    lolk_reset(&s->st);
    s->last_graze = -1;
    s->last_enemies = -1;
    s->last_spawn_id = -1;
    s->last_age = -1;
    s->fade_t = -1;                     /* ★ 2026-09-21：换关 = 没在退场（面板当场收掉）*/
    panel_dim_reset(s);                 /* ★ 2026-09-21：防遮挡复位（闩锁 0 / alpha 255）*/
    s_stage_id = 0;                     /* 换关 → 关卡脚本对象换人，重新认 */
    if (s->ring_id) { ce_anm_delete(s->ring_id); s->ring_id = 0; }
    panel_hide(s);
}

static void on_delete(ce_card_t *c)
{
    lolk_card_t *s = ce_state(c, lolk_card_t);
    if (!s) return;
    if (s->ring_id) { ce_anm_delete(s->ring_id); s->ring_id = 0; }
    /* ★ 2026-09-10 补：**面板 6 条 VM + 五行数字也要收**。原来只收擦弹圈 ⇒
     * 「波次结算后面板还挂着的那段时间里被倒吊人删卡」（倒吊人是**商店里成交**的即时卡，
     * ctor 里就删）会留下一整块停住的面板 + 五行旧数字，直到下一次换关清 anm。
     * `on_stage_start` / `on_run_reset` 都调了 `panel_hide`，只有这里漏了。*/
    panel_hide(s);
}

static void on_run_reset(ce_card_t *c)
{
    lolk_card_t *s = ce_state(c, lolk_card_t);
    if (!s) return;
    if (s->ring_id) { ce_anm_delete(s->ring_id); s->ring_id = 0; }
    panel_hide(s);
    if (s->waves_total) {
        ce_log("lolk: run reset (%d waves settled, last bonus %d)", s->waves_total, s->last.bonus);
    }
    memset(s, 0, sizeof *s);
    panel_dim_reset(s);                 /* ★ 2026-09-15g/2026-09-21：防遮挡复位 = **满亮 255**（共享件）*/
    lolk_reset(&s->st);                 /* 补 sentinel：last_chapter=-1、wave_chapter=-1、mid_rule=NONE、slow=FULL */
    s->last_graze = -1;
    s->last_enemies = -1;
    s->last_spawn_id = -1;
    s->last_age = -1;
    s->fade_t = -1;                     /* ★ 2026-09-21：memset 会把它写成 0（看着像"正在退场"）*/
    s_stage_id = 0;
}

CE_CARD(115, .on_tick_2 = on_tick_2, .on_draw = on_draw,
              .on_stage_start = on_stage_start, .on_run_reset = on_run_reset,
              .on_delete = on_delete,
              /* ★ 2026-09-18：满 P 涨源（TH15 `th15:0x440357` = +100 显示/颗 F 道具）——
               * 挂**现成的**共享站点 `ce_item_collect`（`th18:0x44625f`，本来就在 31 站点表里），
               * 不是新断点、不改字节；一颗道具一笔，见上面的 `on_item_collect()`。*/
              .on_item_collect = on_item_collect);
