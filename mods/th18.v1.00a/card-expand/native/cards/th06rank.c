/* 红魔乡（card-expand id 106，内部 `EOSD_PLAYRANK`）—— **被动卡**，三件：
 *
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
 * **本卡先从 alpha 入手**（用户 2026-09-11 定：判定点先用 alpha = 0，不动 scale、不打断点）。 */
#define T06R_PLAYER_HITBOX_VM   0x476b0

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
} t06r_state_t;

/* 卡私有状态只有 496 字节（CE_STATE_BYTES 512 − 块头 16）；超出 `ce_state_user` 直接返回 NULL
 * ⇒ 所有 st() 都会解引用空指针。这里在**编译期**卡住，别等到实跑。 */
typedef char t06r_state_fits[(sizeof(t06r_state_t) <= 496) ? 1 : -1];

static t06r_state_t *st(ce_card_t *c) { return ce_state(c, t06r_state_t); }

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

static void init_all(ce_card_t *c, const char *why)
{
    t06r_state_t *s = st(c);
    __builtin_memset(s, 0, sizeof *s);
    t06_state_init(&s->r, t06r_difficulty());
    s->last_graze = -1;                       /* -1 = 首帧只对账 */
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
            const float poc = *(const float *)(p + CE_PLAYER_POC_HEIGHT);
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
    (void)acc;
    t06r_state_t *s = st(c);
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

    if (ce_owned(36)) { t06_increase(&s->r.rank, T06_INC_LIFE_FRAG); ce_log("th06rank: stage start, Narumi(36) +0.70"); }
    if (ce_owned(37)) { t06_increase(&s->r.rank, T06_INC_BOMB_FRAG); ce_log("th06rank: stage start, Patchouli(37) +0.02"); }
    s->last_rank_logged = s->r.rank.rank;
    ce_log("th06rank: stage start (rank %d, min %d, max %d, sub %d)", s->r.rank.rank,
           s->r.rank.min_rank, s->r.rank.max_rank, s->r.rank.sub_rank);
}

/* ── ① 低速不显示判定点 ──────────────────────────────────────────────────────
 * 判定点指示 = `player+0x476b0` 的 VM（script `0x1a` / layer `0xe`，只在**聚焦**时存在）。
 * 隐藏手段（用户 2026-09-11 定案）：把它的 `color_1`（`vm+0x524`）**alpha 写 0**
 *   ⇒ `ce_anm_set_color(id, 0x00FFFFFF)`。
 * ⚠ 时机：SDK 文档写明 AbilityManager（`on_tick_2`，优先级 0x16）**先于** Player（0x17），
 *   `on_tick` 也在 Player 复位前 ⇒ **只有 `on_draw` 晚于 Player tick**，隐藏必须放这里
 *   （spec §2.1 的时序注）。而且只在**聚焦时**写：不聚焦时引擎自己 `interrupt_tree` 收掉 VM、
 *   句柄清 0，我们碰不到也不该碰。
 * ⚠ 每帧重写：脚本若自己每帧设颜色也会被按掉 —— 这正是我们要的效果
 *   （season.c 有同款注记：写死 alpha 会把脚本的 `alphaTime` 按掉）。 */
static int on_draw(ce_card_t *c)
{
    (void)c;
    uint8_t *p;
    uint32_t id;

    /* ★ on_draw（虚表 +0x48）**在菜单 / 标题里也会被调**（animal.c 117 实跑踩过同一个坑），
     * 而那时 `0x4cf410`（= `CE_ADDR_PLAYER_PTR`）可能是上一次 run 留下的**野指针 / 空指针**
     * —— 先问游戏线程在不在（这道门与 lolk.c（115）/ season.c（116）`on_draw` 开头那一行同款、同理由）。
     * ★★ 2026-09-12 审计订正：原先这道门**不够** —— 紧接着的 `CE_PLAYER_FOCUSED()` 展开成
     *   `(CE_PLAYER() != 0) && *(int32_t*)(CE_PLAYER()+0x476cc)`，`&&` 的两侧**各求一次**
     *   `*(uint8_t**)0x4cf410`，于是「读指针得 0」之后会**再读一次 0 并加偏移** ⇒ 空指针解引用
     *   （实测 `cc -I. … -o /tmp/ck` 跑那一行 = SIGSEGV）。契约上「游戏线程在 ⇒ `0x4cf410` 非空」
     *   未必成立（标题 → 首次进入关卡之间存在窗口、on_load 时它也可能还是空）
     *   ⇒ 必须**自己先把指针取回来判空**，再走下面全部读。 */
    if (!*(volatile uint32_t *)CE_ADDR_GAME_THREAD_PTR) return 0;
    p = CE_PLAYER();
    if (!p) return 0;
    if (!CE_PLAYER_FOCUSED()) return 0;      /* p 已非空 ⇒ 访问器内部那两次读都安全 */
    id = *(volatile uint32_t *)(p + T06R_PLAYER_HITBOX_VM);
    if (!id) return 0;
    ce_anm_set_color(id, 0x00FFFFFFu);       /* D3DCOLOR 0xAARRGGBB：alpha 0 = 全透明 */
    return 0;
}

/* ── 生命周期 ─────────────────────────────────────────────────────────────── */
static int on_load(ce_card_t *c)
{
    init_all(c, "load");
    seed(c);
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
    if (s) __builtin_memset(s, 0, sizeof *s);
}

static void on_run_reset(ce_card_t *c)
{
    t06r_state_t *s = st(c);
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
