/* mof_green_core.c —— 见 `mof_green_core.h` 的长注（为什么能只换贴图表项就变绿、一手在哪）。
 * 本层**只有数据与内存原语**：不包含 `engine.h`、不认任何 TH18 地址（`tbl` 由卡侧解析后传进来）。*/
#include <string.h>
#include "mof_green_core.h"

/* ============================ 表本体（名单） ============================
 * ★★ 用户 2026-09-18 的两句口径定死了这张表的样子：
 *   「**固定几只**……**我不知道，得看情况**」+「我预期改是**一波改一两只** —— 似乎 110 111 原作
 *   就是这样，**纯信仰点掉落的没那么多**」
 * ⇒ 名单 = **表 + 每行两个开关**（`enabled` / `stage_mask`），改名单 = 改这一行（机制一个字不动）；
 *   默认只开**两支**，且都收窄到**单个关**（因为同一支妖精的外观是**跨关共用**的：`S=184`
 *   同时被 st02/st03/st07 用 ⇒ 不按关收窄就会一次改掉三关，与「一波一两只」冲突）。
 *
 * **默认开的**
 *   ① B 组蓝妖精 **只在 st03**：st03 是**唯一一个现在没有绿妖精的关**（它自己的 `Cir00` 是紫 292°）
 *      ⇒ 这一改把「每关都有绿妖精」补齐（6 个 `enmCreate` 站点 = `GirlBlueA02` 一支）。
 *   ② A 组蓝妖精 **只在 st01**：st01 的蓝妖精是**最稀疏**的一支（`GirlBlueA01` + `GirlBlueA01n`
 *      共 2 个站点 ⇒ 正好「一波一两只」），而且蓝妖精掉的正是**金钱** ⇒ 一并满足
 *      「金钱和绿点总之别重合」。
 *
 * **默认关的**（想加就近改 `enabled` / `stage_mask`）：st05/st02/st07 的蓝妖精、以及红/第四色各支。
 * ⚠ 硬约束（单测钉住）：**同一时刻处于生效状态的两行不许共用同一段 `src_sprite`** ——
 *   两行若 `stage_mask` 有交（0 = 所有关 = 与谁都交）就必须各用各的源变体；否则第二个 swap
 *   会把「已经是绿的」内容当原色备份下来 ⇒ 还原时把绿当原色写回去。
 *
 * 一手的色相、站点数与「谁在用」（`native/tools/measure_enemy_variants.py` + 全量 ECL 花名册）：
 *   A 组：sprite 0 蓝 228（st01 `GirlBlueA01/A01n` 2 站点 + st05 `GirlBlueA01` 2，掉 2/1）
 *         sprite 12 红 12（st01 `GirlRedA01/01n` 2 + st06 红系 22，掉 1）
 *         sprite 36 橄榄 60（st05 `GirlA02/GirlRedA03` 6 + st06 `GirlA05` 4，掉 1）
 *   B 组：sprite 220 蓝 196（st02 4 + st03 6 + st07 6，掉 2/1）
 *         sprite 232 红 9（st02 4 + st03 8 + st07 4，掉 1/2）
 *         sprite 256 橙 57（**ECL 零站点**）
 *   C 组：sprite 160/172/196（**ECL 零站点**）；绿那支 = 184
 * ⚠ 大妖精 `GirlD*`（sprite 96 那支）、`GirlB01/02`、`GirlC01`、`Phantom*` **没有绿色同形**
 *   ⇒ 不在本表里也不该在（要它们也变绿只能外供 TH10 贴图，见报告 §5）。
 * ✅ 本来就是绿的、已在宿主表里的：`(2,10)`（st05 `GirlBlueA03` / st06 `GirlA04`）、`(2,56)`、
 *   `(2,68)`、`(2,80)` —— ★ 用户点名的「一面开头的红绿阴阳玉」= `Cir00p`(红 0°) / `Cir00m`(绿 127°)，
 *   **绿的那只早就是宿主**（10 个站点），不需要走本表。*/
#define ST1 (1u << 0)
#define ST2 (1u << 1)
#define ST3 (1u << 2)
#define ST4 (1u << 3)
#define ST5 (1u << 4)
#define ST6 (1u << 5)
#define ST7 (1u << 6)
#define ALL_STAGES 0u

const mof_green_pair_t mof_green_pairs[MOF_GREEN_PAIR_MAX] = {
    /*  src_spr  dst_spr  src_scr  dst_scr   on   关掩码        who */
    /* ★★ 2026-09-18c（用户实跑后）：**默认全部关掉** —— 实跑看到「第一面第二波的蓝妖精全变绿妖精了」
     *    并明确选了路线②（「我们自己多刷一只绿的」＝**不碰原版妖精**）⇒ 变色这条路留作**备用口味**：
     *    想让某一支妖精整支变绿，把对应行的 `enabled` 改 1（机制、判据、掉落替换都已经在）。
     *    `mof_green_swap` 那套（sprite 表项覆盖 + 按关掩码 + 删卡还原）一个字都没删。*/
    /* ★★★ 2026-09-24 owner 口径（逐字）：「绿妖精我放弃了，**走改妖精路线**吧，改的数量就是
     *   **只有杂鱼绿色妖精**，记得和 th12 卡得**错开改，别改成同一只**」+「数量就按照**改一只（对）**
     *   就行，和 th12 基本一致」⇒ **只开这一行**（蓝妖精整支换绿皮、只 st01、32×32 杂鱼限定）。
     *   ⚠ **为什么挑这一行**：112（TH12 星莲船）的令牌载体皮用的是 **sprite36..47**（他们自己的日志：
     *     `carrier skin ON … -> FAIRY (sprite36..47 妖精)`）⇒ 我们换的是 **sprite0 那一组**（另一支妖精），
     *     两边**不碰同一只**（也不碰他们 `sprite125..132` 的蝴蝶皮）。换色机制 = 覆盖 sprite 表项 +
     *     关掩码 + 删卡还原（`mof_green_swap`），一行一个「对」。*/
    {     0,      24,       0,      10,     1,  ST1,         "A 组蓝妖精 · 只 st01（GirlBlueA01/A01n，2 站点）· ★owner 2026-09-24 开启" },
    {   220,     244,     184,     194,     0,  ST3,         "B 组蓝妖精 · 只 st03（GirlBlueA02，6 站点；本关原来一只绿妖精都没有）" },
    {     0,      24,       0,      10,     0,  ST5,         "A 组蓝妖精 · 只 st05（GirlBlueA01，2 站点）" },
    {   220,     244,     184,     194,     0,  ST2 | ST7,   "B 组蓝妖精 · 只 st02+st07（GirlBlueA01/A03，4/6 站点）" },
    {    12,      24,       5,      10,     0,  ALL_STAGES,  "A 组红妖精 · 所有关（st01 2 + st06 22 站点）" },
    {   232,     244,     189,     194,     0,  ALL_STAGES,  "B 组红妖精 · 所有关（st02 4 + st03 8 + st07 4 站点）" },
    {    36,      24,      15,      10,     0,  ALL_STAGES,  "A 组第四色（橄榄）· 所有关（st05 6 + st06 4 站点）" },
    {   256,     244,     199,     194,     0,  ALL_STAGES,  "B 组第四色（橙）· ECL 零站点（备用行）" },
};

/* ★★★ 2026-09-24（owner 口径，逐字）：「你会称之为**中妖精**的妖精——第一面第二波里面单独一只的那个，
 *   也对应地建了绿色妖精；但是**有色妖精是杂鱼妖精**，而这一波根本没有杂鱼妖精。
 *   而我说的加绿色妖精，**是只有杂鱼妖精**」。
 *   ⇒ 「补绿妖精」只认**杂鱼妖精**（小号有色妖精）。判据落到**精灵尺寸**上（一手 `enemy.anm` 的 sprite 表）：
 *     · `script 0..19`  → 32×32  ⇒ **杂鱼** ✅
 *     · `script 20..39` → 48×32 / 48×48 ⇒ **中妖精** ✗（第一面第二波那只单独的就是这一类）
 *     · `script 159..178` / `184..203` → 32×32 ⇒ 杂鱼 ✅
 *   （64×64 的 `script 40` / `224` = 大妖精，本来就在白名单外。）尺寸表可复现：
 *   `python3 -c` 扫 `local/_scratch/th18anm/enemy.anm.txt` 的 `script→sprite→rect`，见 `TUNING.md` §2.0zzi。*/
int mof_green_is_fairy(int32_t anm_src, int32_t anm_script)
{
    if (anm_src != 2) return 0;                       /* 只认 `enemy.anm`（`anmSelect(2)`）的外观 */
    if (anm_script >= 0 && anm_script <= 19) return 1;    /* 32×32：A 组杂鱼（4 色）*/
    if (anm_script >= 159 && anm_script <= 178) return 1; /* 32×32：B 组杂鱼 */
    if (anm_script >= 184 && anm_script <= 203) return 1; /* 32×32：B 组杂鱼（另一批外观）*/
    /* 20..39 = 48×32 / 48×48 的**中妖精**：owner 明确否掉（「这一波根本没有杂鱼妖精」）⇒ 不补。 */
    return 0;
}

int mof_green_extra_slots(const float *x, int n, int count, float *out_x, int out_max)
{
    float xmin = 0.0f, xmax = 0.0f, d = 0.0f;
    int neg = 0, pos = 0, i, j, cnt = 0;
    float uniq[16];
    int nu = 0;
    int side;

    if (!x || !out_x || out_max <= 0 || n <= 0 || count <= 0) return 0;
    if (n > 16) n = 16;
    if (count > MOF_GREEN_EXTRA_SLOT_MAX) count = MOF_GREEN_EXTRA_SLOT_MAX;
    xmin = xmax = x[0];
    for (i = 0; i < n; ++i) {
        if (x[i] < xmin) xmin = x[i];
        if (x[i] > xmax) xmax = x[i];
        if (x[i] < 0.0f) ++neg; else if (x[i] > 0.0f) ++pos;
    }
    /* 唯一的 |x| 值（排序后取最小正间距）—— 队形的「一格」有多宽 */
    for (i = 0; i < n; ++i) {
        float a = x[i] < 0.0f ? -x[i] : x[i];
        int seen = 0;
        for (j = 0; j < nu; ++j) if (uniq[j] == a) { seen = 1; break; }
        if (!seen && nu < 16) uniq[nu++] = a;
    }
    for (i = 1; i < nu; ++i) {                 /* 插入排序（nu 很小）*/
        float v = uniq[i];
        for (j = i - 1; j >= 0 && uniq[j] > v; --j) uniq[j + 1] = uniq[j];
        uniq[j + 1] = v;
    }
    for (i = 1; i < nu; ++i) {
        float gap = uniq[i] - uniq[i - 1];
        if (gap > 0.5f && (d == 0.0f || gap < d)) d = gap;
    }
    if (d == 0.0f) d = MOF_GREEN_EXTRA_SLOT_MIN;      /* 只观测到一个点 / 全在 x=0 ⇒ 用默认间距 */

    /* ★★ 2026-09-22（th10 #2「绿色妖精队列错误」）：槽位**紧贴观测到的那一端**，不再用 `|x|max`。
     *   旧写法 = `side × (|x|max + d)`：只要这一波的观测点**都偏在一侧**（TH18 一面道中的「竖直列」
     *   就是这种 —— `st01.ecl` 的 `MainSub00` 里 `%A` 整波不变、3 只一直刷同一个 x），
     *   `|x|max` 取的是**离屏幕中心最远那一端**的绝对值 ⇒ 补出来的一只会落到**屏幕另一侧**
     *   （实测：一面第二波的列在 x ≈ −224..−160，补刷落在 **+256** → 夹到 +176 = 右边缘）。
     *   一手 TH10 的形状是「**排在循环的下一格**」（`stage01.ecl` `main_572` 后紧跟一条
     *   `ins_257("Ecl_GGirl00", %B, …)`，`%B` = 循环最后一格 **+d**）⇒ 相邻才是复刻，
     *   照它改：**取「人少那一侧的端点 ± 一格」**（对称队形与旧规则逐值相同，见单测）。*/
    if (count == 1) {
        side = (neg < pos) ? -1 : 1;
        out_x[cnt++] = (side < 0) ? (xmin - d) : (xmax + d);
        return cnt;
    }
    /* 一对 / 多对：由内向外一格格排（先负后正），`count` 是**总只数** */
    for (i = 0; i < count && cnt < out_max; ++i) {
        float r = (float)(i / 2 + 1) * d;
        out_x[cnt++] = ((i % 2) == 0) ? (xmin - r) : (xmax + r);
    }
    return cnt;
}

int mof_green_active_count(void)
{
    int i, n = 0;
    for (i = 0; i < MOF_GREEN_PAIR_MAX; ++i)
        if (mof_green_pairs[i].enabled) ++n;
    return n;
}

int mof_green_pair_in_stage(const mof_green_pair_t *p, int32_t stage)
{
    if (!p || !p->enabled) return 0;
    if (p->stage_mask == 0) return 1;                       /* 0 = 所有关 */
    if (stage < 1 || stage > 7) return 0;
    return (p->stage_mask & (1u << (stage - 1))) != 0;
}

void mof_green_plan_tbl(const mof_green_pair_t *tbl, int n, int32_t stage, uint32_t applied,
                        uint32_t *to_apply, uint32_t *to_restore)
{
    uint32_t want = 0, ap = 0, rs = 0;
    int i;
    if (!tbl || n > 32) n = 0;
    for (i = 0; i < n; ++i)
        if (mof_green_pair_in_stage(&tbl[i], stage)) want |= 1u << i;
    ap = want & ~applied;                                   /* 该改还没改 ⇒ 去改 */
    rs = applied & ~want;                                   /* 改了但不该改（换关）⇒ 还原 */
    if (to_apply)   *to_apply   = ap;
    if (to_restore) *to_restore = rs;
}

void mof_green_plan(int32_t stage, uint32_t applied, uint32_t *to_apply, uint32_t *to_restore)
{
    mof_green_plan_tbl(mof_green_pairs, MOF_GREEN_PAIR_MAX, stage, applied, to_apply, to_restore);
}

const mof_green_pair_t *mof_green_lookup_tbl(const mof_green_pair_t *tbl, int n, uint32_t applied,
                                             int32_t anm_src, int32_t anm_script)
{
    int i;
    /* 变体表全部来自 `enemy.anm`（`anmSelect(2)`）⇒ 别的 anm 源一律不算。
     * 一手：`th18:0x436c49`–`0x436c4e`（`N` 取 `[0x4cf2d0]+N*4+0x168`，N=2 ⇒ `+0x170`）。*/
    if (anm_src != 2 || !tbl || n > 32) return 0;
    for (i = 0; i < n; ++i) {
        const mof_green_pair_t *p = &tbl[i];
        if (!p->enabled) continue;                          /* 关着的行**永远**不算（纵深防御：
                                                             * 哪怕调用方递来一个全 1 的 `applied`）*/
        if (!(applied & (1u << i))) continue;               /* ★ 只认「此刻真的改上了」的行 */
        /* 一个变体占 5 个脚本（pose 0..4）—— 只要落在这 5 个里，就是「被我们变绿的那一支」。*/
        if (anm_script >= p->src_script && anm_script < p->src_script + 5) return p;
    }
    return 0;
}

const mof_green_pair_t *mof_green_lookup(uint32_t applied, int32_t anm_src, int32_t anm_script)
{
    return mof_green_lookup_tbl(mof_green_pairs, MOF_GREEN_PAIR_MAX, applied, anm_src, anm_script);
}

static const uint8_t *entry_at(const uint8_t *tbl, int sprite)
{
    return tbl + (size_t)sprite * MOF_GREEN_ENTRY;
}

int mof_green_pair_sane(const uint8_t *tbl, const mof_green_pair_t *p)
{
    int i;
    if (!tbl || !p) return 0;
    if (p->src_sprite < 0 || p->dst_sprite < 0) return 0;
    for (i = 0; i < MOF_GREEN_SPRITES; ++i) {
        const uint8_t *a = entry_at(tbl, p->src_sprite + i);
        const uint8_t *b = entry_at(tbl, p->dst_sprite + i);
        /* 宽高（两个 float）必须逐条相等：这是「同形异色」在**运行期**的唯一判据。
         * 不等 ⇒ 我们记的常量与这一版真表的布局对不上 ⇒ 宁可不改（调用方只打一行日志）。*/
        if (memcmp(a + MOF_GREEN_WH_OFF, b + MOF_GREEN_WH_OFF, 8) != 0) return 0;
    }
    return 1;
}

int mof_green_pair_is_green(const uint8_t *tbl, const mof_green_pair_t *p)
{
    int i;
    if (!tbl || !p) return 0;
    for (i = 0; i < MOF_GREEN_SPRITES; ++i)
        if (memcmp(entry_at(tbl, p->src_sprite + i), entry_at(tbl, p->dst_sprite + i),
                   MOF_GREEN_ENTRY) != 0)
            return 0;
    return 1;
}

void mof_green_pair_apply(uint8_t *tbl, const mof_green_pair_t *p)
{
    int i;
    if (!tbl || !p) return;
    for (i = 0; i < MOF_GREEN_SPRITES; ++i)
        memcpy(tbl + (size_t)(p->src_sprite + i) * MOF_GREEN_ENTRY,
               tbl + (size_t)(p->dst_sprite + i) * MOF_GREEN_ENTRY, MOF_GREEN_ENTRY);
}

void mof_green_pair_swap(uint8_t *tbl, const mof_green_pair_t *p, uint8_t *backup)
{
    int i;
    if (!tbl || !p || !backup) return;
    for (i = 0; i < MOF_GREEN_SPRITES; ++i) {
        uint8_t *src = tbl + (size_t)(p->src_sprite + i) * MOF_GREEN_ENTRY;
        memcpy(backup + (size_t)i * MOF_GREEN_ENTRY, src, MOF_GREEN_ENTRY);      /* 先备份原色 */
        memcpy(src, tbl + (size_t)(p->dst_sprite + i) * MOF_GREEN_ENTRY, MOF_GREEN_ENTRY);
    }
}

void mof_green_pair_restore(uint8_t *tbl, const mof_green_pair_t *p, const uint8_t *backup)
{
    int i;
    if (!tbl || !p || !backup) return;
    for (i = 0; i < MOF_GREEN_SPRITES; ++i)
        memcpy(tbl + (size_t)(p->src_sprite + i) * MOF_GREEN_ENTRY,
               backup + (size_t)i * MOF_GREEN_ENTRY, MOF_GREEN_ENTRY);
}
