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
    {     0,      24,       0,      10,     1,  ST1,         "A 组蓝妖精 · 只 st01（GirlBlueA01/A01n，2 站点）" },
    {   220,     244,     184,     194,     1,  ST3,         "B 组蓝妖精 · 只 st03（GirlBlueA02，6 站点；本关原来一只绿妖精都没有）" },
    {     0,      24,       0,      10,     0,  ST5,         "A 组蓝妖精 · 只 st05（GirlBlueA01，2 站点）" },
    {   220,     244,     184,     194,     0,  ST2 | ST7,   "B 组蓝妖精 · 只 st02+st07（GirlBlueA01/A03，4/6 站点）" },
    {    12,      24,       5,      10,     0,  ALL_STAGES,  "A 组红妖精 · 所有关（st01 2 + st06 22 站点）" },
    {   232,     244,     189,     194,     0,  ALL_STAGES,  "B 组红妖精 · 所有关（st02 4 + st03 8 + st07 4 站点）" },
    {    36,      24,      15,      10,     0,  ALL_STAGES,  "A 组第四色（橄榄）· 所有关（st05 6 + st06 4 站点）" },
    {   256,     244,     199,     194,     0,  ALL_STAGES,  "B 组第四色（橙）· ECL 零站点（备用行）" },
};

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

void mof_green_plan(int32_t stage, uint32_t applied, uint32_t *to_apply, uint32_t *to_restore)
{
    uint32_t want = 0, ap = 0, rs = 0;
    int i;
    for (i = 0; i < MOF_GREEN_PAIR_MAX; ++i)
        if (mof_green_pair_in_stage(&mof_green_pairs[i], stage)) want |= 1u << i;
    ap = want & ~applied;                                   /* 该改还没改 ⇒ 去改 */
    rs = applied & ~want;                                   /* 改了但不该改（换关）⇒ 还原 */
    if (to_apply)   *to_apply   = ap;
    if (to_restore) *to_restore = rs;
}

const mof_green_pair_t *mof_green_lookup(uint32_t applied, int32_t anm_src, int32_t anm_script)
{
    int i;
    /* 变体表全部来自 `enemy.anm`（`anmSelect(2)`）⇒ 别的 anm 源一律不算。
     * 一手：`th18:0x436c49`–`0x436c4e`（`N` 取 `[0x4cf2d0]+N*4+0x168`，N=2 ⇒ `+0x170`）。*/
    if (anm_src != 2) return 0;
    for (i = 0; i < MOF_GREEN_PAIR_MAX; ++i) {
        const mof_green_pair_t *p = &mof_green_pairs[i];
        if (!(applied & (1u << i))) continue;               /* ★ 只认「此刻真的改上了」的行 */
        /* 一个变体占 5 个脚本（pose 0..4）—— 只要落在这 5 个里，就是「被我们变绿的那一支」。*/
        if (anm_script >= p->src_script && anm_script < p->src_script + 5) return p;
    }
    return 0;
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
