/* test_mof_green_core.c —— `cards/mof_green_core.c` 的主机单测（`make test-host` 的口径：
 *   cc -std=c11 -Wall -Wextra -Werror -o /tmp/tg tests/test_mof_green_core.c cards/mof_green_core.c && /tmp/tg）。
 *
 * 钉住四件事（都是「改名单 / 换贴图 / 判掉落」这三条线上的硬约束）：
 *   ① **表的不变式**：每一行都必须满足「`dst − src` 的脚本差 ÷ 5 = 变体差，sprite 差 = 变体差 × 12」
 *      （一手：`enemy.anm` 每个变体 = 12 sprite / 5 脚本，见 `mof_green_core.h`）——
 *      这条是「换 sprite 表项 == 换脚本号」的**唯一**依据，写歪了画出来就是错的贴图；
 *   ② **判据**：`mof_green_lookup()` 只认 `anm_src == 2` 且落在**源变体的 5 个脚本**里；
 *      关掉的行（`enabled == 0`）**一律不算**（用户「看情况」⇒ 名单随时改，判据必须跟着走）；
 *   ③ **原语的语义**：swap 备份 + 覆盖、apply 幂等不重取备份、restore 精确还原、is_green 是幂等闩锁；
 *   ④ **运行期同形体检**：宽高逐条相等才放行；有一条不等 ⇒ 拒绝（宁可不改也不画错）。
 *   ★ 「不在场零写入」在卡侧（`cards/mof.c` 的 `mof_green_tick()` 只被 `on_tick_2` 调、
 *     而 `on_tick_2` 只在卡在场时跑），本层不模拟。*/

#include <stdio.h>
#include <string.h>
#include "../cards/mof_green_core.h"

static int s_fail, s_pass;
#define CHECK(cond) do { if (cond) ++s_pass; else { ++s_fail; printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } } while (0)

/* 假的 sprite 表：够大就行（本表最远的 sprite = 267 + 11）。*/
#define FAKE_ENTRIES 320
static uint8_t s_tbl[FAKE_ENTRIES * MOF_GREEN_ENTRY];
static uint8_t s_bak[MOF_GREEN_PAIR_BYTES];

static uint8_t *E(int sprite) { return s_tbl + (size_t)sprite * MOF_GREEN_ENTRY; }

/* 造一个「源变体 = 0x11、绿变体 = 0x22，宽高一致」的假表。*/
static void fake_reset(void)
{
    int i;
    memset(s_tbl, 0, sizeof s_tbl);
    memset(s_bak, 0x5a, sizeof s_bak);
    for (i = 0; i < FAKE_ENTRIES; ++i) {
        memset(E(i), 0x11, MOF_GREEN_ENTRY);
        /* 宽高是**两个 float（8 字节）**，要整段置同一个值 —— 只改首字节的话剩下的 3 字节
         * 仍是 0x11/0x22 的填充 ⇒ memcmp 当然不等（第一版就这么错过，留痕）。*/
        memset(E(i) + MOF_GREEN_WH_OFF, 0x40, 8);
    }
    for (i = 0; i < MOF_GREEN_SPRITES; ++i) {
        memset(E(24 + i), 0x22, MOF_GREEN_ENTRY);       /* A 组绿变体 */
        memset(E(24 + i) + MOF_GREEN_WH_OFF, 0x40, 8);
        memset(E(244 + i), 0x22, MOF_GREEN_ENTRY);      /* B 组绿变体 */
        memset(E(244 + i) + MOF_GREEN_WH_OFF, 0x40, 8);
    }
}

static void test_table_invariants(void)
{
    int i, n = 0;

    for (i = 0; i < MOF_GREEN_PAIR_MAX; ++i) {
        const mof_green_pair_t *p = &mof_green_pairs[i];
        int dv, ds;
        if (!p->who) continue;                  /* 空行 */
        ++n;
        CHECK(p->src_sprite >= 0 && p->dst_sprite >= 0);
        CHECK(p->src_script >= 0 && p->dst_script >= 0);
        /* ⚠ 绝对基址**不是** 12/5 的倍数（一手实测：B 组在 sprite 220 / 脚本 184 起，
         *   220 % 12 = 4、184 % 5 = 4）⇒ 只有**差**才是 12/5 的倍数。*/
        CHECK((p->dst_sprite - p->src_sprite) % MOF_GREEN_SPRITES == 0);
        CHECK((p->dst_script - p->src_script) % 5 == 0);
        CHECK(p->src_sprite % MOF_GREEN_SPRITES == p->dst_sprite % MOF_GREEN_SPRITES);
        CHECK(p->src_script % 5 == p->dst_script % 5);
        CHECK(p->src_sprite != p->dst_sprite);
        CHECK(p->enabled == 0 || p->enabled == 1);
        dv = (p->dst_sprite - p->src_sprite) / MOF_GREEN_SPRITES;    /* 变体差 */
        ds = (p->dst_script - p->src_script) / 5;                    /* 脚本变体差 */
        CHECK(ds == dv);                                             /* ★ 两组必须同步 */
        /* 三组同式的一条推论：`dst − src` 只能是 ±1 / ±2 个变体（= ±5/±10 脚本、±12/±24 sprite）*/
        CHECK(dv >= -2 && dv <= 2);
        CHECK(p->stage_mask == 0 || (p->stage_mask & ~0x7fu) == 0);   /* 只有 st01..st07 七位 */
        /* ★ 硬约束：**同一时刻会同时生效的两行**不许共用同一段 `src_sprite`
         * （`stage_mask` 相交 = 会同时生效；0 = 所有关 = 与谁都相交）。
         * 违反的后果：第二个 swap 把「已经变绿」的内容当原色备份 ⇒ 删卡还原时把绿写回去。*/
        {
            int j;
            for (j = 0; j < MOF_GREEN_PAIR_MAX; ++j) {
                const mof_green_pair_t *q = &mof_green_pairs[j];
                int overlap_stage;
                if (j == i || !q->who || !q->enabled || !p->enabled) continue;
                overlap_stage = (p->stage_mask == 0 || q->stage_mask == 0 ||
                                 (p->stage_mask & q->stage_mask) != 0);
                if (overlap_stage)
                    CHECK(p->src_sprite + MOF_GREEN_SPRITES <= q->src_sprite ||
                          q->src_sprite + MOF_GREEN_SPRITES <= p->src_sprite);
            }
        }
    }
    CHECK(n == MOF_GREEN_PAIR_MAX);             /* 表必须写满（否则 MAX 与本体漂了）*/
}

static void test_stage_and_plan(void)
{
    /* 关掩码（`CE_CURRENT_STAGE()` 是 **1-based**）*/
    CHECK(mof_green_pair_in_stage(&mof_green_pairs[0], 1) == 1);    /* 只 st01 */
    CHECK(mof_green_pair_in_stage(&mof_green_pairs[0], 2) == 0);
    CHECK(mof_green_pair_in_stage(&mof_green_pairs[1], 3) == 1);    /* 只 st03 */
    CHECK(mof_green_pair_in_stage(&mof_green_pairs[1], 2) == 0);
    CHECK(mof_green_pair_in_stage(&mof_green_pairs[3], 2) == 0);    /* st02|st07 但开关关着 ⇒ 不生效 */
    CHECK(mof_green_pair_in_stage(&mof_green_pairs[0], 0) == 0);    /* 读不到关 ⇒ 不生效 */
    CHECK(mof_green_pair_in_stage(&mof_green_pairs[0], 8) == 0);
    CHECK(mof_green_pair_in_stage(0, 1) == 0);

    /* 逐帧计划：st01 ⇒ 只改第 0 行；换到 st03 ⇒ 先还原第 0 行、再改第 1 行 */
    {
        uint32_t ap = 0xdeadbeef, rs = 0xdeadbeef;
        mof_green_plan(1, 0, &ap, &rs);
        CHECK(ap == (1u << 0));
        CHECK(rs == 0);
        mof_green_plan(1, (1u << 0), &ap, &rs);         /* 已改 ⇒ 无动作（幂等）*/
        CHECK(ap == 0 && rs == 0);
        mof_green_plan(3, (1u << 0), &ap, &rs);         /* 换关 ⇒ 还原 0、改 1 */
        CHECK(ap == (1u << 1));
        CHECK(rs == (1u << 0));
        mof_green_plan(3, (1u << 0) | (1u << 1), &ap, &rs);
        /* st03 里第 0 行（只 st01）**不该**在 ⇒ 它必须被还原，而不是「已经改了就算数」*/
        CHECK(ap == 0 && rs == (1u << 0));
        mof_green_plan(3, (1u << 1), &ap, &rs);
        CHECK(ap == 0 && rs == 0);
        mof_green_plan(4, (1u << 1), &ap, &rs);         /* st04 两支都不该在 ⇒ 全还原 */
        CHECK(ap == 0 && rs == (1u << 1));
        mof_green_plan(0, 0xffffffffu, &ap, &rs);       /* 关号读不到 ⇒ 全部还原（安全侧）*/
        CHECK(ap == 0);
        mof_green_plan(1, 0, 0, 0);                     /* NULL 安全 */
        mof_green_plan(1, 0, &ap, 0);
        CHECK(ap == (1u << 0));
    }
}

static void test_lookup(void)
{
    int i;
    const uint32_t applied = (1u << 0) | (1u << 1);     /* 默认名单的两行都「已改」*/

    /* 默认名单 = 蓝妖精两支（开关在 .c 里，改动这里会红 —— 那是**故意**的：
     * 改名单要同时想清楚「谁变绿 = 谁掉绿点」这条绑定）*/
    CHECK(mof_green_active_count() == 2);

    for (i = 0; i < 5; ++i) {                   /* A 组蓝：脚本 0..4 ⇒ 命中；5..9（红）⇒ 不命中 */
        CHECK(mof_green_lookup(applied, 2, 0 + i) == &mof_green_pairs[0]);
        CHECK(mof_green_lookup(applied, 2, 5 + i) == 0);
        CHECK(mof_green_lookup(applied, 2, 184 + i) == &mof_green_pairs[1]);
        CHECK(mof_green_lookup(applied, 2, 189 + i) == 0);
        CHECK(mof_green_lookup(applied, 2, 15 + i) == 0);       /* A 组第四色（开关关着）*/
        CHECK(mof_green_lookup(applied, 2, 199 + i) == 0);      /* B 组第四色（开关关着）*/
    }
    /* ★ 「表里开着」≠「此刻真的改了」：没抹上（applied 位为 0）就不许按宿主给点 ——
     * 否则会出现「看着还是蓝的妖精掉绿点」，违反「变绿 = 掉绿点」这条绑定。*/
    CHECK(mof_green_lookup(0, 2, 0) == 0);
    CHECK(mof_green_lookup(0, 2, 184) == 0);
    CHECK(mof_green_lookup(1u << 1, 2, 0) == 0);        /* 只改了 B 组 ⇒ A 组不算 */
    CHECK(mof_green_lookup(1u << 0, 2, 184) == 0);
    CHECK(mof_green_lookup(1u << 0, 2, 0) == &mof_green_pairs[0]);

    CHECK(mof_green_lookup(applied, 2, 10) == 0);       /* 本来就是绿的那支（宿主表管，不归这里）*/
    CHECK(mof_green_lookup(applied, 2, 194) == 0);
    CHECK(mof_green_lookup(applied, 3, 0) == 0);        /* 不是 enemy.anm ⇒ 一律不算 */
    CHECK(mof_green_lookup(applied, 0, 0) == 0);
    CHECK(mof_green_lookup(applied, 2, -1) == 0);
    CHECK(mof_green_lookup(applied, 2, 99999) == 0);
    CHECK(mof_green_lookup(applied, 2, 1) == &mof_green_pairs[0]);
}

static void test_sane(void)
{
    const mof_green_pair_t *p = &mof_green_pairs[0];
    const mof_green_pair_t *q = &mof_green_pairs[1];

    fake_reset();
    CHECK(mof_green_pair_sane(s_tbl, p) == 1);
    CHECK(mof_green_pair_sane(s_tbl, q) == 1);
    CHECK(mof_green_pair_sane(0, p) == 0);
    CHECK(mof_green_pair_sane(s_tbl, 0) == 0);

    /* 把绿变体第 7 项的宽改掉 ⇒ 体检必须拒绝（这就是「常量与真表对不上就别改」那条）*/
    E(24 + 7)[MOF_GREEN_WH_OFF] = 0x41;         /* 只动绿变体第 7 项的那个 float 的首字节 */
    CHECK(mof_green_pair_sane(s_tbl, p) == 0);
    fake_reset();

    /* 高度那一半也要管（两个 float 都要比）*/
    E(244 + 3)[MOF_GREEN_WH_OFF + 4] = 0x42;
    CHECK(mof_green_pair_sane(s_tbl, q) == 0);
    fake_reset();
    CHECK(mof_green_pair_sane(s_tbl, q) == 1);
}

static void test_swap_apply_restore(void)
{
    const mof_green_pair_t *p = &mof_green_pairs[0];
    int i;

    fake_reset();
    CHECK(mof_green_pair_is_green(s_tbl, p) == 0);

    /* ---- swap：备份原色 + 覆盖成绿色 ---- */
    mof_green_pair_swap(s_tbl, p, s_bak);
    CHECK(mof_green_pair_is_green(s_tbl, p) == 1);
    for (i = 0; i < MOF_GREEN_SPRITES; ++i) {
        CHECK(E(p->src_sprite + i)[0] == 0x22);            /* 源那一格现在是绿的 */
        CHECK(E(p->src_sprite + i)[MOF_GREEN_WH_OFF] == 0x40);
        CHECK(s_bak[i * MOF_GREEN_ENTRY + 0] == 0x11);     /* 备份里是原色 */
    }
    /* 绿变体自己一个字节都没被动（拷贝是**单向**的：源 ← 绿）*/
    CHECK(E(p->dst_sprite)[0] == 0x22);

    /* ---- apply 幂等：再抹一次不改任何东西，也不动备份 ---- */
    {
        uint8_t before[MOF_GREEN_PAIR_BYTES];
        memcpy(before, s_bak, sizeof before);
        mof_green_pair_apply(s_tbl, p);
        CHECK(memcmp(before, s_bak, sizeof before) == 0);
        CHECK(mof_green_pair_is_green(s_tbl, p) == 1);
    }

    /* ---- 重放路径：`anm` 重载把源打回原色 ⇒ apply 复活绿色（**不**动备份）---- */
    for (i = 0; i < MOF_GREEN_SPRITES; ++i)
        memset(E(p->src_sprite + i), 0x11, MOF_GREEN_ENTRY);
    CHECK(mof_green_pair_is_green(s_tbl, p) == 0);
    mof_green_pair_apply(s_tbl, p);
    CHECK(mof_green_pair_is_green(s_tbl, p) == 1);
    CHECK(s_bak[0] == 0x11);

    /* ---- restore：精确还原到原色（删卡 / 换局那条路）---- */
    mof_green_pair_restore(s_tbl, p, s_bak);
    CHECK(mof_green_pair_is_green(s_tbl, p) == 0);
    for (i = 0; i < MOF_GREEN_SPRITES; ++i) {
        CHECK(E(p->src_sprite + i)[0] == 0x11);
        CHECK(E(p->dst_sprite + i)[0] == 0x22);
    }
    /* 还原之后与「从没碰过」逐字节相同（整块比对，防漏格）*/
    {
        static uint8_t fresh[FAKE_ENTRIES * MOF_GREEN_ENTRY];
        memcpy(fresh, s_tbl, sizeof fresh);
        fake_reset();
        CHECK(memcmp(fresh, s_tbl, sizeof fresh) == 0);
    }

    /* ---- NULL 安全（卡侧解析不出表时会传 0）---- */
    mof_green_pair_swap(0, p, s_bak);
    mof_green_pair_apply(0, p);
    mof_green_pair_restore(0, p, s_bak);
    mof_green_pair_swap(s_tbl, 0, s_bak);
    mof_green_pair_swap(s_tbl, p, 0);
    mof_green_pair_restore(s_tbl, p, 0);
    CHECK(mof_green_pair_is_green(0, p) == 0);
    CHECK(mof_green_pair_is_green(s_tbl, 0) == 0);
}

/* ★ 「每波多刷一只绿的」的队形槽位（用户 2026-09-18：「按照几何形状，每波多一对，或者多一只」）*/
static void test_extra_slots(void)
{
    /* ⚠ `out` 必须给到 `MOF_GREEN_EXTRA_SLOT_MAX`：本函数会按传进来的 `out_max` 写满 ——
     * 第一版这里只给 4 个 float 却传 `out_max = 8` ⇒ **写爆栈**，后面几条断言全成垃圾
     * （这就是它当时红的原因）。调用方（卡侧）同样要给够。*/
    float x[20] = {0}, out[MOF_GREEN_EXTRA_SLOT_MAX] = {0};
    int n, i;

    /* 观测不到任何点 ⇒ 不补（这一波还没开始）*/
    CHECK(mof_green_extra_slots(x, 0, 1, out, 4) == 0);
    CHECK(mof_green_extra_slots(0, 3, 1, out, 4) == 0);
    CHECK(mof_green_extra_slots(x, 3, 1, 0, 4) == 0);
    CHECK(mof_green_extra_slots(x, 3, 1, out, 0) == 0);
    CHECK(mof_green_extra_slots(x, 3, 0, out, 4) == 0);          /* count=0 ⇒ 不补 */
    CHECK(mof_green_extra_slots(x, 3, -1, out, 4) == 0);

    /* 对称队形 ±60、±120（间距 60）⇒ 多一只补在右侧 180；多一对 ⇒ ±180 */
    x[0] = 60.0f; x[1] = -60.0f; x[2] = 120.0f; x[3] = -120.0f;
    n = mof_green_extra_slots(x, 4, 1, out, 4);
    CHECK(n == 1);
    CHECK(out[0] == 180.0f);
    n = mof_green_extra_slots(x, 4, 2, out, 4);
    CHECK(n == 2);
    CHECK(out[0] == -180.0f && out[1] == 180.0f);
    /* ★ 整群（TH11 的形状：一波 0 只或 5~80 只）⇒ 由内向外一格格排：±180、±240、±300 … */
    n = mof_green_extra_slots(x, 4, 6, out, MOF_GREEN_EXTRA_SLOT_MAX);
    CHECK(n == 6);
    CHECK(out[0] == -180.0f && out[1] == 180.0f);
    CHECK(out[2] == -240.0f && out[3] == 240.0f);
    CHECK(out[4] == -300.0f && out[5] == 300.0f);
    /* 只数被 `out_max` 与 `MOF_GREEN_EXTRA_SLOT_MAX` 双重夹住 */
    n = mof_green_extra_slots(x, 4, 99, out, MOF_GREEN_EXTRA_SLOT_MAX);
    CHECK(n == MOF_GREEN_EXTRA_SLOT_MAX);         /* 夹到上限 */
    /* 截断：`out_max = 3` 时只写前三格（±180、−240）*/
    n = mof_green_extra_slots(x, 4, 6, out, 3);
    CHECK(n == 3 && out[0] == -180.0f && out[1] == 180.0f && out[2] == -240.0f);

    /* 人少的那一侧优先：只有右侧两点 ⇒ 补到左侧 */
    x[0] = 40.0f; x[1] = 80.0f;
    n = mof_green_extra_slots(x, 2, 1, out, 4);
    CHECK(n == 1 && out[0] == -120.0f);          /* xmax 80 + 间距 40 */

    /* 只观测到一个点 ⇒ 用默认间距（40）—— 否则 `d` 会是 0，补出来的位置与原点重合。
     * ⚠ 侧的规则照旧（右侧已有人 ⇒ 补左侧）⇒ 结果是 −140 而不是 +140。*/
    x[0] = 100.0f;
    n = mof_green_extra_slots(x, 1, 1, out, 4);
    CHECK(n == 1 && out[0] == -140.0f);
    /* 「一对」不受侧影响：±(100+40) */
    n = mof_green_extra_slots(x, 1, 2, out, 4);
    CHECK(n == 2 && out[0] == -140.0f && out[1] == 140.0f);

    /* 全在 x=0（例如从正中刷出）⇒ 默认间距，多一只落在 +40 */
    x[0] = 0.0f; x[1] = 0.0f;
    n = mof_green_extra_slots(x, 2, 1, out, 4);
    CHECK(n == 1 && out[0] == 40.0f);

    /* 非对称三点（-30 / +50 / +90）：|x| 唯一值 {30,50,90} ⇒ **最小**正间距 20（不是 40）
     * ⇒ 补在左侧 -(90+20) = -110（左侧人少优先）*/
    x[0] = -30.0f; x[1] = 50.0f; x[2] = 90.0f;
    n = mof_green_extra_slots(x, 3, 1, out, 4);
    CHECK(n == 1 && out[0] == -110.0f);

    /* `out_max` 截断（多一对但只给一格 ⇒ 只写负侧那一个；`|x|` 只有一个唯一值 ⇒ d=40 默认）*/
    x[0] = 60.0f; x[1] = -60.0f;
    n = mof_green_extra_slots(x, 2, 2, out, 1);
    CHECK(n == 1 && out[0] == -100.0f);

    /* `n > 16` 也要安全（函数内部夹到 16；卡侧最多喂 8 个）：
     * 20 个全正、|x| 间距 10 ⇒ xmax=200（第 16 个之前就夹住了也只是更小）⇒ 补左侧 −(xmax+10)。*/
    for (i = 0; i < 20; ++i) x[i] = (float)(i * 10 + 10);
    n = mof_green_extra_slots(x, 20, 1, out, 4);
    CHECK(n == 1 && out[0] < -160.0f);
}

int main(void)
{
    printf("test_mof_green_core —— 变绿（110）纯逻辑\n");
    test_table_invariants();
    test_stage_and_plan();
    test_lookup();
    test_sane();
    test_swap_apply_restore();
    test_extra_slots();
    printf("test_mof_green_core: %d passed, %d failed\n", s_pass, s_fail);
    return s_fail ? 1 : 0;
}
