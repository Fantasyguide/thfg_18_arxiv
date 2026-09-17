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
        if (p->enabled) {
            /* 源与绿各自 12 个 sprite 不许和别的行重叠（否则两行会互相踩）*/
            int j;
            for (j = 0; j < MOF_GREEN_PAIR_MAX; ++j) {
                const mof_green_pair_t *q = &mof_green_pairs[j];
                if (j == i || !q->who || !q->enabled) continue;
                CHECK(p->src_sprite + MOF_GREEN_SPRITES <= q->src_sprite ||
                      q->src_sprite + MOF_GREEN_SPRITES <= p->src_sprite);
            }
        }
    }
    CHECK(n == MOF_GREEN_PAIR_MAX);             /* 表必须写满（否则 MAX 与本体漂了）*/
}

static void test_lookup(void)
{
    int i;

    /* 默认名单 = 蓝妖精两支（开关在 .c 里，改动这里会红 —— 那是**故意**的：改名单要同时想清楚下面这些）*/
    CHECK(mof_green_active_count() == 2);

    for (i = 0; i < 5; ++i) {                   /* A 组蓝：脚本 0..4 ⇒ 命中；5..9（红）⇒ 不命中 */
        CHECK(mof_green_lookup(2, 0 + i) == &mof_green_pairs[0]);
        CHECK(mof_green_lookup(2, 5 + i) == 0);
        CHECK(mof_green_lookup(2, 184 + i) == &mof_green_pairs[1]);
        CHECK(mof_green_lookup(2, 189 + i) == 0);
        CHECK(mof_green_lookup(2, 15 + i) == 0);        /* A 组第四色（开关关着）*/
        CHECK(mof_green_lookup(2, 199 + i) == 0);       /* B 组第四色（开关关着）*/
    }
    CHECK(mof_green_lookup(2, 10) == 0);        /* 本来就是绿的那支（宿主表管，不归这里）*/
    CHECK(mof_green_lookup(2, 194) == 0);
    CHECK(mof_green_lookup(3, 0) == 0);         /* 不是 enemy.anm ⇒ 一律不算 */
    CHECK(mof_green_lookup(0, 0) == 0);
    CHECK(mof_green_lookup(2, -1) == 0);
    CHECK(mof_green_lookup(2, 99999) == 0);
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

int main(void)
{
    printf("test_mof_green_core —— 变绿（110）纯逻辑\n");
    test_table_invariants();
    test_lookup();
    test_sane();
    test_swap_apply_restore();
    printf("test_mof_green_core: %d passed, %d failed\n", s_pass, s_fail);
    return s_fail ? 1 : 0;
}
