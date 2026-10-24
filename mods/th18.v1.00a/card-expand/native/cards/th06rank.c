/* 红魔乡（card-expand id 106，内部 `EOSD_PLAYRANK`）—— **被动卡**，三件：
 *
 *   ⓪ ★ 2026-09-20：**上线收点当且仅当满 P**（用户口径）—— 不满 P 时对**收点线**声明「遮」
 *      （`ce_poc_declare_mask()`；落笔值 = `min(语义高度, 自机 y)` ⇒ 过线不吸，线上满额照给）
 *      + 整局**不画**那条开局指示线（`ce_poc_line_hide()`，§7-A4 定案 (ii)）。
 *      一手与理由全在 `sdk_core.h` 的「遮」声明一节；原实现（接管两条 `ja`）已证明不可用，
 *      见 `AUDIT.md` §BE-EQ / §BE-ER。
 *   ① 低速不显示判定点（th06 独有：别的作聚焦都显示自机受击点）
 *   ② PlayRank（th06 `GameManager.rank/subRank` 1:1 复刻，**影响面搁置** —— 见下）
 *   ③ 冴月麟彩蛋（往一块不参与任何判定的内存里写字符串）
 *
 * 纯逻辑（rank 增减 / 难度表 / 12 条 / 碎片会计）全在 `th06rank_core.c`（可主机单测）；
 * 本文件只做**接线**：引擎地址、事件 → core。
 *
 * 唯一真源：
 *   · spec `docs/superpowers/specs/2026-09-10-th06-playrank-design.md`
 *   · 订正 `docs/superpowers/specs/2026-09-11-system-card-spec-review.md` §2 / §5.1 / §12
 *     （碎片会计「逐颗 + 净减抑制」、出屏 `>= 200.0`、最小 rank 12/10/10/10/14、rank→难度 搁置）
 *   · th06 一手反编译 `/mnt/d/项目/幻想指南/18/th06/src/` 下的 `GameManager.cpp` / `ItemManager.cpp` /
 *     `EnemyManager.cpp` / `Player.cpp`（12 条增减的每一处调用点都逐个读过）
 *
 * ── ② 为什么「影响面搁置」────────────────────────────────────────────────────
 * TH18 的 rank 是**死代码**：全局 `0x4ccd08`（= GlobalsInner+0x2c）全程序只有 2 条读、**没人写**，
 * 22 个关卡 ECL 里 rankF3/F5/F2/I3/I5/I2（ECL 529–534）一次都没用过 ⇒ 写它没有任何效果。
 * 用户 2026-09-11 拍板：**只做「一个没有任何意义的 rank 系统」**，`rank → 难度` 完全搁置
 * （spec §2.3 / spec-review §3.2 / §5.1「剩余」）。所以本卡**不写任何引擎 rank 字段**、
 * 不做难度映射、也**没有任何 HUD**（spec §5：不显示 rank 数字，本卡没有 HUD 元素）。
 *
 * ── 断点依赖（都已在 sites.py 里，本卡不新增）────────────────────────────────
 *   ce_item_frame   0x446ee3  每**正常游玩帧**一次（道具池 body 前；菜单/暂停/商店帧不经过）
 *                             ⇒ 本卡的「关卡时间轴帧」帧源（th06 的 `timelineTime` 对位）
 *   ce_item_collect 0x44625f  type 分派**前**（副作用未发生），逐颗道具
 *   ce_bomb_spent   0x4203bc  每次炸弹消耗（含决死 B）
 *   ce_death        虚表 +0x0c on_death_after_deathbomb（**非**决死；69 / 117 同族）
 */
#include "sdk.h"
#include "th06rank_core.h"

/* ── 引擎地址（engine.h 里没有的，照 sdk.h 既有包装的写法在本文件内自写）─────────── */

/* 判定点指示的 anm id 句柄：`player+0x476b0`（spec §2.1 / IDEAS §「判定点一手」）。
 * 一手 `Player::sub_45b170` `0x45b433`：聚焦且 `+0x476b0 == 0` 时
 * `AnmManager__instantiate_vm_to_world_list_back`（script `0x1a`、layer `0xe`）并把句柄写回
 * `+0x476b0`（`0x45b457`）；`0x45b45d` 每帧对它写 scale、`OR [vm+0x534], 8`。
 * **本卡先从 alpha 入手**（用户 2026-09-11 定：判定点先用 alpha = 0，不动 scale、不打断点）。
 * ⚠ **`+0x476b0` 只是"正转"那一颗**；判定点一共有**两颗**（正转 + 反转），见下面
 *   `T06R_VM_CHILD_NEXT` 的三条一手 —— 只压 `+0x476b0` 会留下一颗反转的（owner 2026-10-25 报）。*/
#define T06R_PLAYER_HITBOX_VM   0x476b0
/* ★ 2026-10-24（owner：可见的那颗说是自机判定点）：引擎还有第二条同族 VM ——
 *   `player+0x476b4`，在 `0x45b91d cmpl $0,0x476bc`（倒计时 > 0）时由
 *   `0x45b98c push $0x1b`（script 27）建；`0x45ba14` 写回句柄。
 *   ⇒ 和判定点同一套 layer/贴图，一并压掉。*/
#define T06R_PLAYER_HITBOX_VM2  0x476b4

/* ── ★★ 判定点的**第二颗 VM** = 父 VM 的**子 VM**（`scriptNew` 造的那颗）────────────────
 * owner 2026-10-25 一手口径：「判定点指示应该是 2 个 VM，正转和反转的两个东西；106 只压了其中转的一个」。
 * 一、两颗 VM 是谁（anm 脚本，一手 `local/th18.v1.00a/anm/effect/effect.anm.txt`）：
 *   · 父 = effect.anm **script26**（= 脚本号 `0x1a`，`0x45b44a push 0x1a`）：
 *     `sprite(sprite23)`（= `eff_sloweffect.png` 那张判定点环）+ `angleVel(+0.05235988)` = **正转 3°/帧**；
 *     第 0 帧 **`scriptNew(script25)`**（`effect.anm.txt:580`）；
 *   · 子 = **script25**（= `0x19`）：**同一个 `sprite23`** + `angleVel(-0.10471976)` = **反转 6°/帧**
 *     （`effect.anm.txt:550-568`）⇒ 正是"正转 + 反转"那两颗。子脚本只有这一处 spawn 点（全 anm 目录
 *     `grep scriptNew(script25)` 在 effect.anm 里只有 script26 一行；`0x19/0x1a` 的其它命中都在别的 anm 自己的编号里）。
 * 二、为什么从父 VM 拿得到子 VM（一手反汇编，th18 v1.00a）：
 *   · `scriptNew` = anm opcode **500**（ExpHP `th-re-data` labels：th18 `0x479a96 = 500__scriptNew`，
 *     th14 同号；**`scriptNewRoot` 是 504** → `0x488a40`，那条才是"孤儿、不挂父"）⇒ handler `0x479a96`
 *     → `AnmLoaded__instantiate_child_vm` **`0x4888f0`**；
 *   · `0x488971 mov [esi+0x5c0],ebx` = **`child+0x5c0`（`zAnmVmSuffix.parent_vm`）= 父 VM**；
 *     `0x48897d`–`0x488988` = `child.root_vm`（`+0x5bc`）= 父的 root（没有则父自己）；
 *     `0x4889e7`–`0x488a11` = 把子结点**头插**进 `父+0x594`（`zAnmVmSuffix.list_of_children`）表头；
 *   · 表结构（ExpHP `type-structs-own.json` 的 `zAnmVmSuffix`：`+0x40 __node_as_child`、`+0x50 list_of_children`
 *     —— 都在 `0x544` 基址上 ⇒ **0x584 / 0x594**）与引擎自己的读法逐条对上（`AnmManager__interrupt_tree`
 *     `0x488c19 mov esi,[esi+0x598]` → `0x488c24 mov edi,[esi]` → `0x488c43 mov esi,[esi+4]`；
 *     父 VM 的 `0x598` = 表头 next = **第一个子结点**，结点 `[0]` = 子 VM 指针（VM init `0x4074b4 mov [edx+0x584],edx`），
 *     结点 `[+4]` = next）。
 *   ⇒ **沿父 VM 的子链表走一层**就能精确拿到那两颗 VM——不用扫 world 链表、不用猜 script 号
 *     （扫链表按脚本号过滤也做得到，但别的 anm 里也有 25/26 号脚本，按父→子关系过滤才是零误伤）。
 * 三、为什么子 VM 不用自己找 spawn 点：它是父 VM 的脚本**第 0 帧**造的，而父 VM 由引擎在**玩家 tick
 *   （0x17）**建（`0x45b450`）⇒ 我们 0x1e 那一笔永远晚于它（同 `on_item_frame` 的相位注）。*/
#define T06R_VM_CHILD_NEXT      0x598   /* 父 VM：`list_of_children.next` = 第一个子结点 */
#define T06R_VM_CHILD_NODE_VM   0x0     /* 子结点：`[0]` = 子 VM 指针（结点 = child+0x584）*/
#define T06R_VM_CHILD_NODE_NEXT 0x4     /* 子结点：`[+4]` = next */
#define T06R_DOT_CHILD_MAX      8u      /* 一趟最多走 8 个（实际恒 1；链表写坏也不许无界）*/

/* 擦弹计数（本 run / 本关累计，上限 99999999）：`0x4ccd0c`。
 * 一手：`0x42497f` 的擦弹路径 `call 0x45e160` → `0x45e179` 读它、`0x45e184 inc`、
 * `0x45e1a1` 写回；清零在 `0x44273a`（每 run 起始）。
 * ⚠ 与 lolk.c（115）/ season.c（116）同址，各自宏名不同；spec §12 待办里说 `sdk.h` 应提炼
 * `CE_ADDR_GRAZE` —— 还没提炼，所以这里自写（**不改 sdk.h**）。
 * `ce_graze` 断点（0x42497f）在 sites.py 里是**注释掉的**（2026-09-10 临时摘除）⇒ 本卡走
 * **每帧差分**，不依赖那个断点。擦弹差分挂在 `on_tick_2`（管理器侧每帧）上。 */
#define T06R_ADDR_GRAZE         0x4ccd0c

/* 当前难度（0 = EASY / 1 = NORMAL / 2 = HARD / 3 = LUNATIC；EXTRA 是独立模式）：
 * `0x4ccd00` = GlobalsInner 首字段旁的难度位（spec §2.3 一手：`0x42aa20` 本体就是
 * `mov eax,[ecx+0x40]`）。本卡**只读**它来选难度表行（min/max 夹取用）。
 * ⚠ **绝不写它**（spec §7：它还被人 `EnemyManager__allocate_new_enemy` / `GameThread__end_stage` /
 * `ScoreFileShotData::add_to_top10_if_worthy` 读 ⇒ 会波及结算与记录）。 */
#define T06R_ADDR_DIFFICULTY    0x4ccd00

/* 道具出屏判据里的两个常量（一手 `ItemManager__on_tick__body` 0x445e38 段）：
 *   `0x445e2b comiss xmm0, [0x4b9340]`  → `0x4b9340` = **200.0f**（判据 = `|pos.x| >= 200.0`：
 *       `0x445e20 movss xmm0,[edi+0xc2c]` + `0x445e28 andps xmm0,xmm6`（清符号位）+
 *       `comiss` + `jb 0x44620a`（小于才继续）⇒ **等于 200.0 也删**，正是订正说的 `>=`）
 *   而 `pos.x` 的偏移就是 `CE_ITEM_POS`（+0xc2c）—— 引擎读的是 `edi+0xc2c`，与 engine.h 的
 *   `CE_ITEM_POS` 逐字节对上 ✓。
 * y 侧的 472.0f（`0x4b93a4`）在 `0x44620a` 之后的共享尾段里判。 */
#define T06R_ITEM_X_LIMIT       200.0f
#define T06R_ITEM_Y_LIMIT       472.0f

static inline int32_t t06r_graze(void)      { return *(volatile int32_t *)T06R_ADDR_GRAZE; }
static inline int32_t t06r_difficulty(void) { return *(volatile int32_t *)T06R_ADDR_DIFFICULTY; }

/* ── ③ 冴月麟：在**本 DLL 自己的 rodata** 里留一份 UTF-8「冴月麟」 ────────────────
 * spec §2.4：「选一块**不被读取、不参与任何判定**的区域……优先 = 本卡 DLL 的静态字符串常量
 * （`static const char k_sakuyue[]`）」⇒ 常量本身就在那里；`on_load` 再往它自己的字节上
 * 写一次（同值 `memcpy`），把「写入」这件事写实。asm 里只出现一次地址引用，
 * 编译器不会折叠掉（`memcpy` 到非 const 别名）。零逻辑影响、不进任何判定、卡面文案不提它。 */
static char k_sakuyue[16] = "\xe5\x86\xb4\xe6\x9c\x88\xe9\xba\x9f";   /* 「冴月麟」9 字节 + NUL */
#define T06R_SAKUYUE_LEN 9

static void sakuyue_write_once(void)
{
    static const char src[T06R_SAKUYUE_LEN + 1] = "\xe5\x86\xb4\xe6\x9c\x88\xe9\xba\x9f";
    if (k_sakuyue[0] == src[0] && __builtin_memcmp(k_sakuyue, src, T06R_SAKUYUE_LEN) == 0) {
        /* 已经是同值：仍然显式写一遍（「写入一次」的语义，不是 no-op 优化掉的分支）*/
    }
    __builtin_memcpy(k_sakuyue, src, T06R_SAKUYUE_LEN + 1);
}

/* ── 状态 ─────────────────────────────────────────────────────────────────── */

/* 出屏去重的「已计过槽号」环形窗（**定长**，见 `off_ring_has` 的注释：
 * 为什么不用每槽一位的位图 —— 4696 槽要 587 字节，卡私有状态只有 496 字节（`ce_state` 上限）。） */
#define T06R_OFF_RING 192   /* 192 槽 × 2 字节 = 384 字节；实际每帧出屏通常 0–2 颗 */

typedef struct {
    t06_state_t r;                        /* rank 四件套 + 帧计数 + 差分窗口（core）*/
    int32_t last_graze;                   /* 擦弹差分基准（< 0 = 还没建立）*/
    int32_t last_rank_logged;             /* 日志用：上次记过的 rank */
    int32_t last_difficulty;              /* 难度表重设用 */
    uint32_t off_counted;                 /* 累计记过的出屏道具数（调试 / 日志用）*/
    uint16_t off_used;                    /* 环形窗里的有效槽号数（帧初清零）*/
    uint16_t off_slot[T06R_OFF_RING];     /* 本帧已记过的道具池槽号 */
    int32_t poc_gate;     /* ★ 2026-09-16za 诊断：本帧有没有声明「未满 P」（1 = 声明了）*/
} t06r_state_t;

/* 卡私有状态只有 496 字节（CE_STATE_BYTES 512 − 块头 16）；超出 `ce_state_user` 直接返回 NULL
 * ⇒ 所有 st() 都会解引用空指针。这里在**编译期**卡住，别等到实跑。 */
typedef char t06r_state_fits[(sizeof(t06r_state_t) <= 496) ? 1 : -1];

static t06r_state_t *st(ce_card_t *c) { return ce_state(c, t06r_state_t); }

/* ① 判定点隐藏（定义在文件下半部的 `on_draw` 之前；`on_item_frame`（0x1e）要先调它 ⇒ 前置声明）。
 * 语义与相位理由见 `t06_hide_hitbox_dot()` 上面那段长注。 */
static int  t06_hide_hitbox_dot(void);
static void t06_dot_probe(int wrote);

/* 引擎库存读数（差分源）。用 volatile 读，防编译器把两次读合并。 */
static inline int32_t lives_now(void)  { return *(volatile int32_t *)CE_ADDR_CURRENT_LIVES; }
static inline int32_t lfrag_now(void)  { return *(volatile int32_t *)CE_ADDR_LIFE_FRAGMENTS; }
static inline int32_t bombs_now(void)  { return *(volatile int32_t *)CE_ADDR_CURRENT_BOMBS; }
static inline int32_t bfrag_now(void)  { return *(volatile int32_t *)CE_ADDR_BOMB_FRAGMENTS; }

/* 帧内去重环形窗：本帧记过的槽号进窗（`off_used` 在帧初清零）。
 * 溢出（同帧 >192 颗出屏）只是**本帧内可能重复计一颗**；下帧自动复位，不累积。
 * 一颗道具不可能同帧被记两次（一帧一次的断点 + 一槽一条扫描），所以这只是防误重入的护栏。 */
static int off_ring_has(const t06r_state_t *s, uint16_t slot)
{
    for (uint16_t k = 0; k < s->off_used; ++k)
        if (s->off_slot[k] == slot) return 1;
    return 0;
}
static void off_ring_add(t06r_state_t *s, uint16_t slot)
{
    if (s->off_used >= T06R_OFF_RING) return;   /* 满：放弃去重（下帧自然复位）*/
    s->off_slot[s->off_used++] = slot;
}

static void seed(ce_card_t *c)
{
    t06r_state_t *s = st(c);
    s->last_graze = t06r_graze();
    s->last_rank_logged = s->r.rank.rank;
    s->last_difficulty = t06r_difficulty();
    s->off_counted = 0;
    s->off_used = 0;
}

/* ── ★ 2026-09-20（`WORK-PACKAGES.md` §7-A4 定案 **(ii)**）：**106 在场的整局都不画那条开局收点指示线**。
 * 理由（owner 口径）：最贴 TH06 —— 原作里根本没有这条视觉线；而且它不随火力闪烁（(i)「线随闸走、满 P 才画」已作废）。
 * 一手：可见性 = `Gui+0x194` 的 bit `0x2000`，**唯一原生写者 = 原生卡 22 的 `on_load`**（`0x40c288`，`or`，
 * **只置不清**），唯一读点 = `0x43af0b`（不置位 ⇒ 那条线的 VM 根本不建）⇒ **我们主动清 = 不画**。
 * 声明是**粘性**的（照 PoC 层其余声明）⇒ 这里只报一次；落笔（清位 + 基准记账 + 撤声明的写回）全在
 * `sdk.h` 的 `ce_poc_apply()` / `ce_poc_release()`（删卡在商店里帧钩子不跑，那条路是**立刻**写）。
 * ⚠ 原生卡 22 每关 `on_load` 都会再置一次 ⇒ 我们每帧的落笔会把它重新清掉（幂等；见 `sdk.c` 的 apply）。*/
static void declare_line_hide(ce_card_t *c)
{
    ce_poc_line_hide(c, 1);
}

static void init_all(ce_card_t *c, const char *why)
{
    t06r_state_t *s = st(c);
    __builtin_memset(s, 0, sizeof *s);
    t06_state_init(&s->r, t06r_difficulty());
    s->last_graze = -1;                       /* -1 = 首帧只对账 */
    declare_line_hide(c);                     /* ★ 「106 在场整局不画那条线」（§7-A4 (ii)；粘性、幂等）*/
    sakuyue_write_once();
    ce_log("th06rank: init on %s (difficulty %d, rank %d, min %d, max %d)", why,
           t06r_difficulty(), s->r.rank.rank, s->r.rank.min_rank, s->r.rank.max_rank);
}

/* 难度变了（练习 / 特别模式切档）⇒ 按新难度表重设 min/max。
 * th06 的 `rank/minRank/maxRank` 只在开新 run 时按表设（GameManager.cpp:345-347），
 * 关卡之间保留；这里加一条「难度变了就重设」的保险，避免 th18 侧切档后 min/max 留在旧档。 */
static void resync_difficulty(t06r_state_t *s)
{
    const int32_t d = t06r_difficulty();
    if (d == s->last_difficulty) return;
    s->last_difficulty = d;
    t06_init(&s->r.rank, d);                  /* 初值 + min/max 重设、subRank 清零（照 th06 开新 run）*/
    ce_log("th06rank: difficulty -> %d, rank reset to %d (min %d, max %d)", d,
           s->r.rank.rank, s->r.rank.min_rank, s->r.rank.max_rank);
}

/* rank 变了就记一行（只在真的变了的时候，不刷屏）。`tag` = 本帧最后一条来源。 */
static void note(t06r_state_t *s, const char *tag)
{
    if (s->r.rank.rank == s->last_rank_logged) return;
    ce_log("th06rank: %s rank %d -> %d (sub %d, t=%d)", tag, s->last_rank_logged,
           s->r.rank.rank, s->r.rank.sub_rank, t06_stage_frame(&s->r));
    s->last_rank_logged = s->r.rank.rank;
}

/* ── 时间轴帧源 ──────────────────────────────────────────────────────────────
 * th06 的帧数条件是 `timelineTime`（**关卡**时间轴，不是全局帧）。TH18 没有对位的东西，
 * 本卡用 `ce_item_frame` 断点（`0x446ee3`）当它：`sites.py` 里那个门卫 CMP 只在
 * **正常游玩帧**到达（菜单 / 暂停 / 商店帧走另一条路），所以每次分派 = 一个游玩帧,
 * 与 th06「对话 / 暂停不计数」的门控同义（115 collect2x 也是这么用的，AUDIT §Z14）。
 * ⚠ 对话（`Gui.msg`）在 TH18 是正常游玩帧、照样会到这个断点 —— th06 那里还有一道
 * `g_Gui.HasCurrentMsgIdx() == 0`（没有对话框时才算），本卡照抄这道门。 */
static void on_item_frame(ce_card_t *c)
{
    t06r_state_t *s = st(c);
    if (CE_GUI_MSG_ACTIVE()) return;          /* th06 的 `HasCurrentMsgIdx() == 0` 门 */
    t06_tick_frame(&s->r);
    /* 帧初清碎片窗口（本帧的碎片事件随后登记、本帧 `on_tick_2` 收口提交）。
     * ⚠ 防御：若某一帧既没有 item_frame 也没有 on_tick_2（理论上不会有），
     * 窗口里残留的碎片净减会漂到下一帧去抑制一次不该抑制的整命增量 —— 这里断掉这条路。 */
    t06_window_item_life_fragment(&s->r, 0 - s->r.life_frag_delta);
    t06_window_item_bomb_fragment(&s->r, 0 - s->r.bomb_frag_delta);

    /* 帧数条件：+1.00（`f % (2400 − 残机数×240) == 0`，f != 0）*/
    if (t06_frame_tick_due(t06_stage_frame(&s->r), CE_CURRENT_LIVES())) {
        t06_increase(&s->r.rank, T06_INC_FRAME);
        note(s, "frame");
    }

    /* 道具出屏 −0.03：spec §4 = **逐帧扫池预测**。一手（本节逐指令核过）：
     *   0x445e13 `comiss xmm0(y), xmm7(=472.0)` + `ja 0x445e38`
     *   0x445e20 `movss xmm0,[edi+0xc2c]` + `andps xmm0,xmm6`（清符号位）+
     *   0x445e2b `comiss xmm0,[0x4b9340](=200.0)` + `jb 0x44620a`（小于才继续）
     * ⇒ 判据 = **`|pos.x| >= 200.0`（含等于）或 `pos.y > 472.0`**；
     * 命中就落到 `0x445e38`：`[edi+0xc74] = 0` + 删 anm + 摘链 —— **引擎同帧就删掉它**。
     * 池内写 state 的地方全库只有两处（`0x445e38` 出界 / `0x446595` 公共收尾），
     * 都是「删除」，没有别的地方清 state ⇒ 「一个槽在 frame N 处于越界且 state != 0」
     * 每颗道具**只可能成立一次**；越界后又飞回场内的（吸附/上线）下回合判据不成立、不再计。
     * 于是本帧只需在**帧内**去重（同一槽不可能同帧被记两次，环形窗只为防误重入）。
     * `ce_item_frame` 断点在池 body **之前** ⇒ 本帧看到的正是「已越界、还没被删」的道具。 */
    {
        uint8_t *mgr = CE_ITEM_MANAGER();
        s->off_used = 0;
        if (mgr) {
            uint8_t *slot = mgr + CE_ITEM_POOL_OFF;
            int hits = 0;
            for (unsigned i = 0; i < CE_ITEM_SLOTS; ++i, slot += CE_ITEM_STRIDE) {
                if (*(volatile int32_t *)(slot + CE_ITEM_STATE) == 0) continue;   /* 空槽 */
                const float ix = *(volatile float *)(slot + CE_ITEM_POS);
                if (ix < T06R_ITEM_X_LIMIT && ix > -T06R_ITEM_X_LIMIT) {
                    const float iy = *(volatile float *)(slot + CE_ITEM_POS + 4);
                    if (!(iy > T06R_ITEM_Y_LIMIT)) continue;   /* 两个判据都不成立 */
                }
                if (off_ring_has(s, (uint16_t)i)) continue;
                off_ring_add(s, (uint16_t)i);
                t06_decrease(&s->r.rank, T06_DEC_OFFSCREEN);
                ++hits;
            }
            if (hits) { note(s, "item offscreen"); s->off_counted += (uint32_t)hits; }
        }
    }

    /* ★★★ 2026-10-24（owner：「106 的判定点隐藏被 108 覆盖」）—— **相位 = 本帧最后一笔**。
     * 本函数挂在 ItemManager tick（UpdateFunc 优先级 **0x1e**）：晚于**所有卡的更新回调**
     * （`on_tick_2` = 0x16，108 的刻符浮字 / 残影束 / 人妖槽每帧写都在这里）与玩家 tick（0x17，
     * 判定点 VM 就是在那儿建/更新的），早于**全部**绘制阶段（玩家 0x1d、层 14 = 0x1f … 层 18 = 0x28）。
     * ⇒ 这一笔落地后，本帧再没有任何卡回调能改那一格 ⇒ 判定点**本帧就**是透明的。
     * ⚠ 与 `on_draw`（0x33）那一笔**都保留**：那边是兜底（绘制相位里若以后有人写），两边共用探针节拍。*/
    t06_dot_probe(t06_hide_hitbox_dot());
}

/* ── 逐颗道具（spec §4.1：**碎片逐颗走这里**，整命 / 整炸走库存增量） ────────────
 * 为什么碎片不能走增量：收第 3 颗时计数器 `2→3→0`（净 −2）**并当场进位 +1 命**，
 * 只认正向增量会丢掉第 3 颗的 +0.70、反把进位算成 +2.00（3 命碎会算成 +3.4）。 */
static void on_item_collect(ce_card_t *c, uint32_t type, const void *item)
{
    t06r_state_t *s = st(c);
    const t06_item_action_t act = t06_item_action(type);
    if (act == T06_ITEM_IGNORE) return;

    switch (act) {
    case T06_ITEM_POWER_SMALL:                                  /* type 1：+0.002（110/111 在场 ×5）*/
        t06_increase(&s->r.rank, t06_power_small_gain(ce_owned(110) || ce_owned(111)));
        break;
    case T06_ITEM_POINT: {                                      /* type 2：蓝点 == 金钱，按回收线分档 */
        /* 用户 2026-09-10 定：把 th06 的 `y < 128.0` 换成 TH18 的**回收线** `player+0x47998`
         *（`CE_PLAYER_POC_HEIGHT`）。低于回收线 ⇒ 走「最大得点」档（÷5 后 +0.06）。
         * ★★ 2026-09-12 对抗审计订正：判据必须是 **自机的 y**，不是道具的 y。
         *   引擎自己的分档点是 `0x446b0c`–`0x446c04`（`ItemManager__on_tick__body` 的金钱处理）：
         *     `446b0c mov esi,ds:0x4cf410`        ← esi = **zPlayer\***
         *     `446b18 movss xmm2,[esi+0x624]`     ← xmm2 = 自机 y（= `CE_PLAYER_Y`，player+0x624）
         *     `446bf9 movss xmm0,[esi+0x47998]`   ← xmm0 = 回收线（= `CE_PLAYER_POC_HEIGHT`）
         *     `446c01 comiss xmm0,xmm2` / `446c04 jae 0x446cb4` ← 线上（y ≤ 线）⇒ 走满额那一支
         *     `446c0a cmp [edi+0xc74],3` / `446c11 je 0x446cb4`  ← **state == 3（真空飞行）也走满额**
         *     `446c1c subss xmm2,xmm0` → 线下按「自机离线多远」递减
         *   ⇒ 三个读点全在**自机**上；原来读道具的 y（`item+CE_ITEM_POS+4`）在两者分离时（70px 磁吸环
         *     内、还没进 30px 的真空飞行）会给出**相反**的分档 ⇒ 已改为自机 y，并补上 state==3 那一支
         *     （`CE_ITEM_STATE_VACUUM` = 3，engine.h:449；此时道具正飞向自机，引擎一律给满额）。
         *   ⚠ deviated-notes：本卡只抄**分档**（它决定 rank 加值），不抄引擎那套递减公式。 */
        const uint8_t *p = CE_PLAYER();
        const uint8_t *it = (const uint8_t *)item;
        int at_poc = 0;
        if (p) {
            /* ★ 2026-09-15：改读**共享件的访问器**（`sdk.h`），不再直读 `*(float*)(p+CE_PLAYER_POC_HEIGHT)`。
             * ★★ 2026-09-20：**改成语义访问器 `ce_poc_height_semantic()`** —— 不满 P 时引擎字段被本卡的
             *   「遮」压在 `min(语义高度, 自机 y)` 上（那是给引擎看的），本卡的 rank 分档要的是
             *   **没有 106 时**那条线（= 别家声明 / 基准 128 / 224）⇒ 两者不能混（用户口径「其它语义零变化」）。*/
            const float poc = ce_poc_height_semantic();
            at_poc = (*(const float *)(p + CE_PLAYER_Y) <= poc);
            if (!at_poc && it && *(const int32_t *)(it + CE_ITEM_STATE) == CE_ITEM_STATE_VACUUM)
                at_poc = 1;                                     /* 引擎同款兜底：真空飞行 = 满额 */
        }
        t06_increase(&s->r.rank, t06_point_gain(at_poc));
        break;
    }
    case T06_ITEM_LIFE_FRAG:                                    /* type 4 / 0x10：命碎 +0.70 */
        t06_increase(&s->r.rank, T06_INC_LIFE_FRAG);
        t06_window_item_life_fragment(&s->r, +1);               /* 本窗口碎片 +1（抑制判据的另一半）*/
        note(s, "life fragment");
        return;                                                 /* 碎片不改整数 rank 时不刷日志 */
    case T06_ITEM_BOMB_FRAG:                                    /* type 6 / 0x11：B 碎 +0.02 */
        t06_increase(&s->r.rank, T06_INC_BOMB_FRAG);
        t06_window_item_bomb_fragment(&s->r, +1);
        note(s, "bomb fragment");
        return;
    default:
        return;
    }
    note(s, "item");
}

/* ── 每帧（管理器侧；AbilityManager tick 优先级 0x16，**先于** Player 0x17 ——
 * 一手注册点 `0x40847f` / `0x45a8b4`（同一注册函数 `0x401180`）⇒ 本卡的差分读的是「上一帧末」的库存）──
 * 这里做三件事：擦弹差分、库存增量（整命 / 整炸 + 同帧碎片净减抑制）、日志。
 * ⚠ 帧窗口：本函数 = 窗口的**收口**。
 * ★ 2026-09-12 审计订正（原注释自相矛盾）：原话写「引擎本帧的 `on_item_frame` / `on_item_collect`
 *   **已经先跑过**（… AbilityManager tick 0x16 在 Player 0x17 之前、**ItemManager 更后**）」——
 *   既然 ItemManager 更后，本帧的 item 事件**还没跑**，两句不可能同时成立。
 *   **实测口径**：ItemManager 的 TH18 UpdateFunc 优先级**全仓无记载**（`LESSONS.md:60` 只列到
 *   AbilityManager 0x16 / Player 0x17 / Bomb 0x19 / Bullet 0x1d）⇒ 相对本卡是**未知**。
 *   **但这不影响正确性**：碎片窗口在**两端**都会清 —— `on_item_frame` 帧初把两个 delta 归零
 *   （见该函数），`t06_commit_frame` 末尾也 `window_clear()`。所以无论 ItemManager tick 在本卡
 *   之前还是之后，「帧初清零 → 本帧登记 → 下一次 commit 收口」这条链都成立：
 *     ① ItemManager 在前 ⇒ 本帧的碎片登记已被收口；
 *     ② ItemManager 在后 ⇒ 登记留在窗口里，由**下一帧**的 commit 收口（窗口跨一帧，语义不变：
 *        碎片那颗自己的 +0.70 是 `on_item_collect` 里当场加的，与窗口无关；窗口只决定
 *        「那一帧的整命 +2.00 抑不抑制」，而碎片进位与 CURRENT_LIVES+1 必然同帧发生）。
 *   ⇒ 结论：正确性不依赖这个未知量；原注释的**结论**（可以安全提交）成立，**理由**写错了，已订正。 */
static int on_tick_2(ce_card_t *c)
{
    t06r_state_t *s = st(c);
    resync_difficulty(s);

    /* ★ 2026-09-19（owner 要求）：**测试键 = 空格 ⇒ 满 P**。
     *   只为验下面那条「上线收点当且仅当满 P」的闸（不然得先刷满火力才能测「满 P 过线照吸」）。
     *   · 边沿检测：`ce_key_down()` 是**电平**（按住恒真）⇒ 配一个 file-local 的 prev，
     *     拍一下设一次、按住不连发（与 117 `cards/animal.c` / 112 `cards/ufo.c` 的调试键同一套）。
     *   · 写的是引擎自己的两个字段：`CURRENT_POWER = [0x4ccd3c]`（`MAX_POWER`，随 sht 变 ——
     *     写死 128 / 400 都是错的，一手见本文件下面「满 P 判据」那条注）。
     *   · 放在**闸声明之前**：拍下空格的那一帧 `gate` 立刻变 0（= 满 P），当场就能验「过线照吸」。
     *   · ⚠ **纯测试件**：不进任何正式数值路径，只在本卡（106）在场上时有效，每次触发打一行日志。
     *   · ⚠ 117（`animal.c`）的调试键也用空格 —— 两张卡同时在卡组里会一起响应；要分开就用
     *     `CE_VK_CONTROL` 做组合键（SDK 已备好那个常量）。*/
    {
        static int dbg_space_prev;                 /* 106 只会有一份 ⇒ file-local 足够 */
        const int down = (CE_DEBUG_SPACE_KEYS && ce_key_down(CE_VK_SPACE)) ? 1 : 0;
        if (down && !dbg_space_prev) {
            const int32_t pmax = *(volatile const int32_t *)CE_ADDR_MAX_POWER;
            if (pmax > 0) {
                CE_CURRENT_POWER() = pmax;
                ce_log("T06R: TEST 空格 ⇒ 满 P（power %d/%d）", (int)pmax, (int)pmax);
            }
        }
        dbg_space_prev = down;
    }

    /* ★★ 2026-09-16za（**用户口径**，两条新设计共用一个来源）：
     *   「红魔乡卡再加一条设计：**上线收点当且仅当满P**」+「**协同效应：除非满P，不加信号强度**」。
     *   一手（TH06 反编译 `ItemManager.cpp` 的道具 tick）：
     *     `if (curItem->state == 1 || (128 <= g_GameManager.currentPower && g_Player.positionCenter.y < 128.0f))`
     *     → 进吸引态（`state = 1`）⇒ **「满 P（128）+ 在回收线（y = 128.0）以上」才是过线收点的充要条件**。
     *   ⇒ 本卡每帧只做一件事：把「现在不是满 P」这个**事实**声明给共享层，两个消费者各自去查：
     *     ① 过线收点 —— ★★ **2026-09-20 换了实现**（原来走两条 `ja` 站点，那两条**已证明不可接管**
     *        也**不可回放**，见 `AUDIT.md` §BE-EQ / §BE-ER，现 park）：
     *        改走 **`ce_poc_declare_mask()`**（PoC 高度层的「遮」声明 = 覆盖位）——
     *        落笔值 = `min(语义高度, 自机 y)`，于是「线 > 自机 y」恒不成立 ⇒ **过线不吸**；
     *        而「线 ≥ 自机 y」那一条（线上满额）在自机过语义线时**恰好相等** ⇒ 照给满额
     *        ⇒ **引擎那两个折额读点看到的 `d` 与「没有 106」时是同一个数**（计分逐位不变）。
     *        一手（字段的 2 写 5 读、两条判据的严格/非严格之差、帧内顺序）写在 `sdk_core.h` 的「遮」一节。
     *     ② 111 的信号推进（关掉「线上未满 ⇒ 补满并钳到 100.00」那一支；擦弹/衰减不受影响）。
     *   ⚠ 声明是**粘性**的（照 `ce_no_hit_declare`）⇒ 每帧都要写一次；删卡 / 局末显式归 0（下面两个收尾函数）。
     *   ⚠ 「满 P」判据 = `CE_CURRENT_POWER() >= [0x4ccd3c]`（`MAX_POWER`，随 sht 变；写死 400 是错的）。*/
    {
        const int32_t pw   = CE_CURRENT_POWER();
        const int32_t pmax = *(volatile const int32_t *)CE_ADDR_MAX_POWER;
        const int     gate = (pmax > 0 && pw < pmax) ? 1 : 0;
        ce_full_p_gate_declare((uint32_t)ce_card_id(c), gate);
        /* ★ 2026-09-20：不满 P ⇒ 对**收点线**声明「遮」（覆盖位）—— 过线收点那一半的新实现。
         *   满 P ⇒ 撤（`ce_poc_declare_mask(c, 0)` = 撤回；落笔那一刻层会把语义高度/基准写回去）。*/
        ce_poc_declare_mask(c, gate);
        if (gate != s->poc_gate) {
            s->poc_gate = gate;
            ce_log("T06R: 上线收点闸 %s（power %d/%d）—— 不满 P ⇒ 声明「遮」把收点线压到自机处"
                   "（过线不吸；线上满额那一支靠 `≥` 与 `>` 的判据差照旧成立，见 sdk_core.h「遮」一节），"
                   "且 111 的「线上补满信号」同时关掉",
                   gate ? "**开**" : "关", (int)pw, (int)pmax);
        }
    }

    /* 擦弹 +0.06（th06:Player.cpp:1394 `IncreaseSubrank(6)` = +0.06，用户 2026-09-11 定「不除」）。
     * 用 0x4ccd0c 的逐帧差分（含 Bomb 状态擦弹 —— th06 那条路也是同一条 `Player::Graze`）。
     * 归零 / 回退（换 run、进练习）时只重设基准，不把负增量当擦弹。 */
    {
        const int32_t g = t06r_graze();
        if (s->last_graze < 0 || g < s->last_graze) {
            s->last_graze = g;
        } else {
            int32_t d = g - s->last_graze;
            s->last_graze = g;
            if (d > 0) {
                if (d > 200) d = 200;            /* 一场对话 / 一帧里挤进太多：钳一下防溢出（幂等）*/
                t06_increase(&s->r.rank, d * T06_INC_GRAZE);
                note(s, "graze");
            }
        }
    }

    /* 整命 +2.00 / 整炸 +0.05：走**库存正向增量**（覆盖道具 type 5/7、商店买 card 1/2、
     * 分数 extend —— 那条 th06 也是 Inc(200)）；同窗口碎片净减时在 core 里抑制。 */
    {
        const int32_t gained = t06_commit_frame(&s->r, lives_now(), lfrag_now(), bombs_now(), bfrag_now());
        if (gained > 0) note(s, "stock+");
        else if (gained < 0) note(s, "stock-");
    }
    return 0;
}

/* ── 放 Bomb −2.00（含决死 B）：th06:Player.cpp:185 `DecreaseSubrank(200) ────────── */
static void on_bomb_spent(ce_card_t *c)
{
    t06r_state_t *s = st(c);
    t06_decrease(&s->r.rank, T06_DEC_BOMB);
    note(s, "bomb");
}

/* ── Miss −16.00（**非决死**）：th06:Player.cpp:225 `DecreaseSubrank(1600) ─────────
 * 挂 +0x0c `on_death_after_deathbomb`（**不是** +0x14）：决死窗口之后仍然死 = 真 Miss；
 * 被决死救下则不到这个槽（double.c / animal.c 同族用法）。决死 B 的 −2.00 已由
 * on_bomb_spent 记过（th06 里决死 B 与普通 B 走同一条 `DecreaseSubrank(200)`）。 */
static int on_death_after_deathbomb(ce_card_t *c, uint32_t acc)
{
    t06r_state_t *s = st(c);
    /* ★★ 2026-09-13（原生卡冲突审计 A 通道）：`acc` = 引擎的累加器（`acc |= 前一张卡的返回值`，
     *   一手 `th18:0x45c116 push edi` / `0x45c121 or edi,eax`）⇒ **非 0 = 这次死亡已被前面的卡取消**，
     *   那就不是真 Miss，不能扣 −16.00 rank。样板 = 原生 `CardEirin__on_player_death_after_deathbomb`
     *   `th18:0x40a4f3`（第 1 条指令就是 `cmp [ebp+8],0; jnz 让位`）。
     *   ⚠ 残余：排在 106 后面的救援者救下时我们仍会记一笔（原生 35 不看 acc）⇒ 见待拍表「死亡结果闩锁」。*/
    if (acc != 0) {
        ce_log("T06R: +0x0c 死亡已被前面的卡救下（acc=%u）⇒ 不算 Miss、不扣 rank", (unsigned)acc);
        return 0;
    }
    t06_decrease(&s->r.rank, T06_DEC_MISS);
    note(s, "miss");
    return 0;
}

/* ── 每关开场：`rank = 难度表初值(16)`、`minRank/maxRank` 按难度、**`subRank = 0`**
 *   （th06:GameManager.cpp:345-347 + 357，spec §2.2「每关开场」；小数部分每关清零、不进 replay）
 *   另外显式补两笔**不经道具**的获得（用户 2026-09-11 定：算获得、给 rank）：
 *     CardNarumi(36) 每关 +1 命碎 ⇒ +0.70；CardPatchouli(37) 每关 +1 符卡 ⇒ +0.02（spec §4 / §8）。 */
static void on_stage_start(ce_card_t *c)
{
    t06r_state_t *s = st(c);
    if (s->r.rank.max_rank <= 0) { init_all(c, "stage start (uninitialised)"); return; }
    t06_state_stage_reset(&s->r);            /* subRank = 0、帧计数 = 0、库存基准撤销 */
    seed(c);
    declare_line_hide(c);                    /* ★ 「106 在场整局不画那条线」（§7-A4 (ii)；幂等）*/

    if (ce_owned(36)) { t06_increase(&s->r.rank, T06_INC_LIFE_FRAG); ce_log("th06rank: stage start, Narumi(36) +0.70"); }
    if (ce_owned(37)) { t06_increase(&s->r.rank, T06_INC_BOMB_FRAG); ce_log("th06rank: stage start, Patchouli(37) +0.02"); }
    s->last_rank_logged = s->r.rank.rank;
    ce_log("th06rank: stage start (rank %d, min %d, max %d, sub %d)", s->r.rank.rank,
           s->r.rank.min_rank, s->r.rank.max_rank, s->r.rank.sub_rank);
}

/* ── ① 低速不显示判定点 ──────────────────────────────────────────────────────
 * 判定点指示 = **两颗** VM（owner 2026-10-25 一手：「正转和反转的两个东西」）：
 *   · **正转**那颗 = `player+0x476b0` 的句柄（script `0x1a` / layer `0xe`，只在**聚焦**时存在）；
 *   · **反转**那颗 = 上面那只 VM 的**子 VM**（script26 第 0 帧 `scriptNew(script25)`；表结构、
 *     地址、为什么从父 VM 走得通 —— 全在文件头 `T06R_VM_CHILD_NEXT` 那段一手注里）。
 *   ⇒ 两颗都要压；只压 `+0x476b0` 时反转那颗照样转（owner 实跑）。
 * 隐藏手段（用户 2026-09-11 定案）：把每颗的 `color_1`（`vm+0x524`）**alpha 写 0**
 *   ⇒ `ce_anm_set_color(id, 0x00FFFFFF)`；外加 `color_2`（`vm+0x528`）的 alpha 字节同时清 0
 *   —— 绘制早退判据是 `color_1.A == 0 && color_2.A == 0`（`th18:0x4812e8`），两路都清才与
 *   `colorMode` 无关（`engine/anm/th18/07-blendmode-...md` §2；某颗脚本哪天写了 `colorMode` 也不怕）。
 * 而且只在**聚焦时**写：不聚焦时引擎自己 `interrupt_tree` 收掉 VM（`0x45b570`，它**会顺着
 *   `list_of_children` 收到子 VM**，`0x488c19`–`0x488c48`）、父句柄清 0，我们碰不到也不该碰。
 *
 * ★★★ 2026-10-24（owner 报「**106 的判定点不显示这个效果，被 108 覆盖了**」）—— **相位错了，不是值错了**：
 *   一手三件（都是别人核过的，这次只用来定位帧内顺序）：
 *     ① 判定点 VM 由**引擎**在**玩家 tick**（UpdateFunc 优先级 **0x17**）里建/更新：
 *        `th18:0x45b433` `cmp [player+0x476b0],0` → 建 VM（`0x45b450` call `0x405bf0`）→ `0x45b457` 写回句柄；
 *     ② 那支 VM 的层 = **脚本自己的 `layer(18)`**（`effect.anm` script `0x1a`；`[0x4cf2c8]+0xc` = EffectManager
 *        的 effect.anm，见 `engine/anm/th18/04-anm-slot-selection.md:101`）⇒ **绘制优先级 0x28**
 *        （`engine/anm/th18/02-render-stages.md` §1：层 18 = 0x28）；
 *     ③ 本卡原来**只在 `on_draw`**（`AbilityManager__on_draw`，优先级 **0x33**）里写 alpha
 *        ⇒ 那一笔**晚于本帧那张图**（0x28 < 0x33）⇒ 只对**下一帧**有效。
 *   ⇒ 只要**任何一帧**在**更新相位**有人把那一格写回非 0（108 的每帧 VM 写：刻符浮字 / 残影束 /
 *     人妖槽数字 —— 它们的句柄是**缓存 id**，而引擎会**回收 VM id**：`ufo.c:2081` 记着同一课，
 *     `ce_anm_get_vm(旧 id)` 在 id 被复用之后**照样返回非 NULL**），那一帧就会把判定点**画出来**，
 *     0x33 再压回 0 ⇒ 实跑表现 = 「装上 108 之后判定点又出现了」。
 *   ⇒ 修法（两条，都是幂等的）：
 *     ★ **在 `on_item_frame`（ItemManager tick，优先级 0x1e）也写一次**。0x1e 晚于**所有卡的更新回调**
 *       （`on_tick_2` = 0x16）与玩家 tick（0x17），早于**全部**绘制阶段（玩家 0x1d、层 14 = 0x1f …
 *       层 18 = 0x28）⇒ 这一笔是**本帧最后一笔**，谁都盖不掉（卡回调里再没有比 0x1e 更晚的更新相位）。
 *     ★ **同时写 alpha 插值器**（`vm+0x140` = `zInterpInt{initial,goal,current}`，`engine.h` 的一手注）：
 *       引擎每帧用 `current` **覆盖** `color_1` 的 A（`th18:0x47c7f2`，`flag = vm+0x168`）⇒
 *       只写 `color_1` 会被它顶掉（`engine.h` 的原话：「只写 color 会被它顶掉」）。三格同写 0 ⇒
 *       即使插值器被启动，收敛值也只有 0。
 *   `on_draw` 那一笔**保留**（幂等、零成本）：它兜住「绘制阶段里才出现的写者」（目前没有）。
 *   ⚠ 每帧重写：脚本若自己每帧设颜色也会被按掉 —— 这正是我们要的效果
 *     （season.c 有同款注记：写死 alpha 会把脚本的 `alphaTime` 按掉）。
 *   ⚠ 删卡 / 换关：这两笔停在那一帧，**不需要还原**（VM 是引擎的，引擎自己维护）；
 *     卡侧只留探针计数，`on_delete`/`on_run_reset` 里归零（见那两个函数）。*/

/* ① 判定点隐藏的**探针记账**（`t06_dot_probe` 用）。放在文件作用域而不是函数内 static：
 * 换关 / 删卡时要能一起归零（否则上一局的行数上限会吃掉这一局的证据）。*/
static uint32_t s_dot_frame, s_dot_c1, s_dot_a1i, s_dot_id, s_dot_fl;
static int      s_dot_wrote;
static unsigned s_dot_lines;
static unsigned s_dot2_lines;          /* ★ 子 VM（反转那颗）那几行单独记数，别被父行吃掉额度 */
static uint32_t s_dot_c_id, s_dot_c_c1, s_dot_c_a1i, s_dot_c_fl;
static unsigned s_dot_c_n;
static void t06_dot_probe_reset(void)
{
    s_dot_frame = 0; s_dot_c1 = 0; s_dot_a1i = 0; s_dot_id = 0; s_dot_fl = 0;
    s_dot_wrote = 0; s_dot_lines = 0; s_dot2_lines = 0;
    s_dot_c_n = 0; s_dot_c_id = 0; s_dot_c_c1 = 0; s_dot_c_a1i = 0; s_dot_c_fl = 0;
}

/* 把**一台** VM 压成全透明（判定点两颗共用同一套写法）：
 *   · `color_1 = 0x00FFFFFF`（alpha 0）、`color_2` 的 A 字节 = 0 —— 绘制早退 `0x4812e8` 要求两路 A 都 0；
 *   · alpha 插值器（`vm+0x140`）三格同写 0 —— 引擎每帧用 `current` 覆盖 `color_1.A`（`th18:0x47c7f2`），
 *     只写 `color_1` 会被顶掉。
 * 幂等；每帧重写是设计的一部分。 */
static void t06_hide_one_vm(uint8_t *vm)
{
    *(uint32_t *)(vm + CE_ANM_VM_COLOR1) = 0x00FFFFFFu;              /* D3DCOLOR 0xAARRGGBB：A = 0 */
    *(uint8_t  *)(vm + CE_ANM_VM_COLOR2 + 3) = 0u;                   /* color_2 只清 A，RGB 原样留着 */
    *(uint32_t *)(vm + CE_ANM_VM_ALPHA1_I + CE_ANM_VM_INTERP_INITIAL) = 0u;
    *(uint32_t *)(vm + CE_ANM_VM_ALPHA1_I + CE_ANM_VM_INTERP_GOAL)    = 0u;
    *(uint32_t *)(vm + CE_ANM_VM_ALPHA1_I + CE_ANM_VM_INTERP_CURRENT) = 0u;
}

/* 写一笔「隐藏判定点」。返回 1 = 这一帧**真的写到了 VM 上**；0 = 有一道门没过
 * （不在游戏线程 / 没有自机 / 不聚焦 / 句柄是 0 / 句柄指向的 VM 已经被引擎回收）。
 * ★ 两颗都写：父 VM（正转）+ 沿父的 `list_of_children` 走到的每一颗子 VM（反转）。
 * 幂等；一帧调两次（0x1e + 0x33）是设计的一部分。 */
static int t06_hide_hitbox_dot(void)
{
    uint8_t *p, *vm;
    uint32_t id;

    /* ★ 门序与理由（都是实跑踩过的坑，别删）：
     *   · `on_draw`（虚表 +0x48）**在菜单 / 标题里也会被调**（animal.c 117 踩过同一个坑），
     *     而那时 `0x4cf410`（= `CE_ADDR_PLAYER_PTR`）可能是上一次 run 留下的**野指针 / 空指针**
     *     ⇒ 先问游戏线程在不在（与 lolk.c（115）/ season.c（116）`on_draw` 开头那一行同款）。
     *   · `CE_PLAYER_FOCUSED()` 展开成 `(CE_PLAYER() != 0) && *(int32_t*)(CE_PLAYER()+0x476cc)`，
     *     `&&` 两侧**各求一次** `*(uint8_t**)0x4cf410` ⇒ 读指针得 0 之后会**再读一次 0 并加偏移**
     *     （实测 SIGSEGV，2026-09-12 审计）⇒ 必须**自己先取指针判空**再走下面的读。*/
    if (!*(volatile uint32_t *)CE_ADDR_GAME_THREAD_PTR) return 0;
    p = CE_PLAYER();
    if (!p) return 0;
    if (!CE_PLAYER_FOCUSED()) return 0;        /* p 已非空 ⇒ 访问器内部那两次读都安全 */
    id = *(volatile uint32_t *)(p + T06R_PLAYER_HITBOX_VM);
    if (!id) return 0;                         /* 不聚焦 / 刚离开聚焦：引擎已收掉 VM */
    vm = ce_anm_get_vm(id);
    if (!vm) return 0;                         /* 句柄作废（脚本自删 / 引擎重建）⇒ 下一帧就是新 VM */
    {   /* ★ 第二条同族 VM（`0x476b4`，script 27）：独立句柄，一起压（含它的子 VM）*/
        uint32_t id2 = *(volatile uint32_t *)(p + T06R_PLAYER_HITBOX_VM2);
        if (id2) {
            uint8_t *vm2 = ce_anm_get_vm(id2);
            uint8_t *n2;
            int g2 = 0;
            if (vm2) {
                t06_hide_one_vm(vm2);
                n2 = *(uint8_t **)(vm2 + T06R_VM_CHILD_NEXT);
                while (n2 && g2++ < T06R_DOT_CHILD_MAX) {
                    uint8_t *c2 = *(uint8_t **)(n2 + T06R_VM_CHILD_NODE_VM);
                    n2 = *(uint8_t **)(n2 + T06R_VM_CHILD_NODE_NEXT);
                    if (c2) t06_hide_one_vm(c2);
                }
            }
        }
    }
    t06_hide_one_vm(vm);                       /* 正转那颗（`player+0x476b0`）*/
    /* ★★ 反转那颗 = 父 VM 的子 VM（script25）。沿 `list_of_children` 走一层 —— 与引擎自己的
     *   `interrupt_tree`（`0x488c19 mov esi,[esi+0x598]` → `0x488c24 mov edi,[esi]` →
     *   `0x488c43 mov esi,[esi+4]`）逐条同形：`父+0x598` = 第一个子结点，`node[0]` = 子 VM，
     *   `node[+4]` = next。实际恒 1 颗；有界（`T06R_DOT_CHILD_MAX`）只是为了链表被写坏时也不无界。*/
    {
        uint8_t *node = *(uint8_t **)(vm + T06R_VM_CHILD_NEXT);
        unsigned guard = 0;
        while (node && guard++ < T06R_DOT_CHILD_MAX) {
            uint8_t *child = *(uint8_t **)(node + T06R_VM_CHILD_NODE_VM);
            uint8_t *next  = *(uint8_t **)(node + T06R_VM_CHILD_NODE_NEXT);   /* 先取 next 再动手 */
            if (child) t06_hide_one_vm(child);
            node = next;
        }
    }
    return 1;
}

/* ── 实跑判据（`CE_DEV_DUMP>=1` 才进二进制；发布档这一整段连参数一起消失）────────────
 * owner 要的证据 =「106 判定点 VM 的 alpha / 可见位，每帧值是多少」。打什么、怎么读：
 *   `id`   = `player+0x476b0`（引擎的句柄；0 = 这一帧没有判定点，例如没聚焦 / 刚重建）
 *   `c1`   = `vm+0x524` 的 `color_1`（我们写 `0x00ffffff` ⇒ 打印出来必须是 `a=00`）
 *   `a1i`  = `vm+0x140+0x10` = alpha 插值器的 `current`（引擎每帧用它覆盖 `color_1.A` ⇒ 必须 0）
 *   `fl`   = `vm+0x534` 的 `flags_lo`（bit1 = 可见位，`engine/anm/th18` 一手；bit3 = 引擎每帧 OR 的 0x8）
 *   `w`    = 本帧这一笔**写进去了没有**（1 = 句柄有效且写到；0 = 门没过）
 * ★ 子 VM（**反转那颗**）另起一行 `T06R-DOT2:`：沿父 VM 的 `list_of_children` 走到的每一颗各一行
 *   （`id` / `script` / `c1` / `a1i` / `fl` 与父同格式，`script` 应为 **25**（0x19）= 反转那颗；
 *    一颗都没有 = 子链表没挂上（那就回到"两颗 VM 到底怎么来的"重查）。
 * 打法：**首帧 / 值有变化 / 每 300 帧心跳**各一行（一帧最多一行，`on_item_frame` 与 `on_draw` 共用计数）。
 * ⚠ 判据：只要 `c1` 的 alpha 恒 00 且 `a1i` 恒 0 ⇒ 隐藏生效；若某帧 `c1` 变成 `ff…` 或 `a1i≠0`
 *   ⇒ **那一帧有别人在写这一格**（把行号附近的信息贴回来即可定位）。*/
static void t06_dot_probe(int wrote)
{
#if CE_DEV_DUMP >= 1
    uint8_t *p = CE_PLAYER();
    uint8_t *vm, *node;
    uint32_t fr, id = 0, c1 = 0, a1i = 0, fl = 0;
    uint32_t c_id = 0, c_c1 = 0, c_a1i = 0, c_fl = 0;
    unsigned c_n = 0;
    int changed;

    if (!p) return;
    fr = ce_sdk_frame();
    if (s_dot_frame == fr) return;                     /* 一帧只判一次 */
    s_dot_frame = fr;
    id = *(volatile uint32_t *)(p + T06R_PLAYER_HITBOX_VM);
    vm = id ? ce_anm_get_vm(id) : 0;
    if (vm) {
        c1  = *(const uint32_t *)(vm + CE_ANM_VM_COLOR1);
        a1i = *(const uint32_t *)(vm + CE_ANM_VM_ALPHA1_I + CE_ANM_VM_INTERP_CURRENT);
        fl  = *(const uint32_t *)(vm + CE_ANM_VM_FLAGS_LO);
        /* 第一颗子 VM 的快照（只用来判"值变了没有"，写日志时再逐颗走一遍）*/
        node = *(uint8_t **)(vm + T06R_VM_CHILD_NEXT);
        if (node) {
            uint8_t *cv = *(uint8_t **)(node + T06R_VM_CHILD_NODE_VM);
            if (cv) {
                c_n   = 1u;
                c_id  = *(const uint32_t *)(cv + CE_ANM_VM_SUFFIX_ID);
                c_c1  = *(const uint32_t *)(cv + CE_ANM_VM_COLOR1);
                c_a1i = *(const uint32_t *)(cv + CE_ANM_VM_ALPHA1_I + CE_ANM_VM_INTERP_CURRENT);
                c_fl  = *(const uint32_t *)(cv + CE_ANM_VM_FLAGS_LO);
            }
        }
    }
    changed = (s_dot_lines == 0u) || (s_dot_wrote != wrote) || (s_dot_c1 != c1) || (s_dot_a1i != a1i) ||
              (s_dot_id != id) || (s_dot_fl != fl) ||
              (s_dot_c_n != c_n) || (s_dot_c_id != c_id) || (s_dot_c_c1 != c_c1) ||
              (s_dot_c_a1i != c_a1i) || (s_dot_c_fl != c_fl);
    if (!changed && (fr % 300u) != 0u) return;
    if (s_dot_lines >= 200u) return;                   /* 收口：一局最多 200 行（父）*/
    ++s_dot_lines;
    s_dot_wrote = wrote; s_dot_c1 = c1; s_dot_a1i = a1i; s_dot_id = id; s_dot_fl = fl;
    s_dot_c_n = c_n; s_dot_c_id = c_id; s_dot_c_c1 = c_c1; s_dot_c_a1i = c_a1i; s_dot_c_fl = c_fl;
    ce_log_key("T06R-DOT: frame %u id=%08x w=%d c1=%08x (alpha %02x) a1i=%08x fl=%08x (visible %d) children=%u",
               (unsigned)fr, (unsigned)id, wrote, (unsigned)c1, (unsigned)((c1 >> 24) & 0xffu),
               (unsigned)a1i, (unsigned)fl, (int)((fl >> 1) & 1u), c_n);
    /* ★ 第二颗（反转）：逐个子 VM 各一行 —— owner 要看的就是"两个都压了吗" */
    if (vm && s_dot2_lines < 200u) {
        unsigned guard = 0, ci = 0;
        for (node = *(uint8_t **)(vm + T06R_VM_CHILD_NEXT); node && guard++ < T06R_DOT_CHILD_MAX; ) {
            uint8_t *cv = *(uint8_t **)(node + T06R_VM_CHILD_NODE_VM);
            uint8_t *next = *(uint8_t **)(node + T06R_VM_CHILD_NODE_NEXT);
            if (cv) {
                const uint32_t cc1 = *(const uint32_t *)(cv + CE_ANM_VM_COLOR1);
                const uint32_t ca1i = *(const uint32_t *)(cv + CE_ANM_VM_ALPHA1_I + CE_ANM_VM_INTERP_CURRENT);
                const uint32_t cfl = *(const uint32_t *)(cv + CE_ANM_VM_FLAGS_LO);
                const uint32_t cid = *(const uint32_t *)(cv + CE_ANM_VM_SUFFIX_ID);
                const int      csc = *(const int32_t *)(cv + CE_ANM_VM_SCRIPT_ID);
                ++s_dot2_lines;
                ++ci;
                ce_log_key("T06R-DOT2: frame %u #%u id=%08x script=%d(0x%x) c1=%08x (alpha %02x) a1i=%08x fl=%08x (visible %d)",
                           (unsigned)fr, ci, (unsigned)cid, csc, (unsigned)csc,
                           (unsigned)cc1, (unsigned)((cc1 >> 24) & 0xffu),
                           (unsigned)ca1i, (unsigned)cfl, (int)((cfl >> 1) & 1u));
            }
            node = next;
        }
    }
#else
    (void)wrote;
#endif
}

static int on_draw(ce_card_t *c)
{
    (void)c;
    /* 见上：这一笔晚于本帧的绘制（0x33 > 0x28），只对**下一帧**有效 ⇒ 它只是兜底，
     * 真正保证「本帧就看不见」的是 `on_item_frame`（0x1e）那一笔。 */
    t06_dot_probe(t06_hide_hitbox_dot());
    return 0;
}

/* ── 生命周期 ─────────────────────────────────────────────────────────────── */
static int on_load(ce_card_t *c)
{
    init_all(c, "load");
    seed(c);
    t06_dot_probe_reset();                   /* ★ 新局：判定点探针的节流 / 行数上限重新开始 */
    declare_line_hide(c);                    /* ★ 「106 在场整局不画那条线」（§7-A4 (ii)；幂等）*/
    return 0;
}

/* 局末（虚表 +0x4c）与删对象（+0x50）都要收干净。
 * ★ 本卡跨帧留下的东西不多：**没有自建 VM、没有子机、没有 HUD、没有全局旋钮**
 *   （判定点那个 VM 是引擎自己的，我们只写它的 alpha，删卡后引擎照旧维护 —— 不需要还原；
 *    冴月麟那块内存不参与任何判定）。真正的风险是**私有状态**：`ce_state` 由 SDK 在
 *    `operator_delete` 桩里释放，所以这里只需把它归零，避免「卡被倒吊人删掉、下局又拿到」
 *    时把旧 rank / 旧差分基准带进新对象（SDK 也留了这个顺序：on_delete 时状态还活着）。 */
static void on_delete(ce_card_t *c)
{
    t06r_state_t *s = st(c);
    ce_full_p_gate_declare((uint32_t)ce_card_id(c), 0);   /* ★ 撤「未满 P」声明（别把闸留在场上）*/
    ce_poc_declare_mask(c, 0);                            /* ★ 撤「遮」（收点线立刻回到基准）*/
    ce_poc_line_hide(c, 0);                               /* ★ 撤「不画线」（并按基准把那一位写回）*/
    t06_dot_probe_reset();                                /* ★ 删卡：判定点探针归零（我们不再写那一格）*/
    if (s) __builtin_memset(s, 0, sizeof *s);
}

static void on_run_reset(ce_card_t *c)
{
    t06r_state_t *s = st(c);
    ce_full_p_gate_declare((uint32_t)ce_card_id(c), 0);   /* ★ 撤「未满 P」声明（局末）*/
    ce_poc_declare_mask(c, 0);                            /* ★ 撤「遮」（换关/局末：字段回到基准）*/
    ce_poc_line_hide(c, 0);                               /* ★ 撤「不画线」（下一关的 on_load 会重新声明）*/
    t06_dot_probe_reset();                                /* ★ 局末：判定点探针归零 */
    if (s) __builtin_memset(s, 0, sizeof *s);   /* 局末：差分基准 / 位图 / 帧计数全清 */
}

CE_CARD(106,
         .on_load = on_load,
         .on_tick_2 = on_tick_2,
         .on_item_frame = on_item_frame,
         .on_item_collect = on_item_collect,
         .on_bomb_spent = on_bomb_spent,
         .on_death_after_deathbomb = on_death_after_deathbomb,
         .on_stage_start = on_stage_start,
         .on_draw = on_draw,
         .on_run_reset = on_run_reset,
         .on_delete = on_delete);
