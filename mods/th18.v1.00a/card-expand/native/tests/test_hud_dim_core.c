/* test_hud_dim_core.c —— `cards/hud_dim.c` 的主机单测（`make test-host`）。
 *
 * 钉三件事（**跨卡统一件**，116/117/107/113/115 + 110/111 共用这一条压暗路）：
 *   ① **表面 px → 自机坐标** 的定式（`自机 = 表面/2 − (320,16)`）——各卡的门限都从块矩形推出来，
 *      换算错了每张卡的门都会偏（`224` 是过时记法，2026-09-15h 订正，见 `hud_dim.h` 文件头）；
 *   ② **统一的那套动态**：**进 = 块矩形外扩 `HUD_DIM_MARGIN`(28)**、**出 = 离开「矩形 + 44」**
 *      （= 28 + `HUD_DIM_BAND` 16 ⇒ **16 px 滞回带**）、终点 alpha 32 / 起始 255 / 每帧 ±56、
 *      `pp == NULL` 的行为、逐帧连续性；
 *   ③ **六块真块的「可触发区」不变式**（§⑩）：每块的「进」区与自机钳位 `x∈[−184,184] / y∈[32,432]`
 *      相交之后必须留下**真的点得亮**的一片（高 ≥ 32 px、宽 ≥ 24 px）—— 这就是 2026-09-15h
 *      把「进 = 矩形之内」改成「矩形 + 边距」要保的那个性质（改回"之内"会让 116/117/107 只剩 4–10 px 高）。
 * **性质 = ❓ 用户口径（UI 统一），不是复刻** —— 见 `hud_dim.h` 文件头。
 *
 * ⚠ 断言里的 `199/143/87/32` 是 `HUD_DIM_STEP = 56` 的**算术结果**（255−56k，k=4 时 31 ⇒ 钳到 32），不是一手值；
 *   改步长宏会红 —— 那正是要的。同理 `28 / 16 / 44` 由 `HUD_DIM_MARGIN` / `HUD_DIM_BAND` 推。*/
#include <stdio.h>
#include <string.h>
#include "../cards/hud_dim.h"

static int s_fail, s_pass;
#define CHECK(cond) do { if (cond) ++s_pass; else { ++s_fail; printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } } while (0)
#define NEAR(a, b) (((a) - (b) < 1e-4f) && ((b) - (a) < 1e-4f))

/* 推一帧并回读状态（确认内部一致）*/
static int32_t tick(hud_dim_t *d, const hud_dim_rect_t *r, float x, float y)
{
    const float pp[2] = { x, y };
    int32_t a = hud_dim_tick(d, r, pp);
    CHECK(a == d->alpha);
    CHECK(d->dim == hud_dim_gate(d->dim, r, pp));
    return a;
}

/* 自机钳位（一手 `th18:0x45b170`）：x ∈ [−184,184]、y ∈ [32,432]。*/
#define PP_X_MIN (-184.0f)
#define PP_X_MAX ( 184.0f)
#define PP_Y_MIN (  32.0f)
#define PP_Y_MAX ( 432.0f)

int main(void)
{
    hud_dim_t d;
    hud_dim_rect_t r;

    /* ① 统一取值本身（用户口径）*/
    CHECK(HUD_DIM_ALPHA_FULL == 255 && HUD_DIM_ALPHA_DIM == 32);
    CHECK(HUD_DIM_STEP == 56);
    CHECK(NEAR(HUD_DIM_MARGIN, 28.0f));        /* ★ 进的边距（❓ 用户口径 2026-09-15h；一手各作 24–32）*/
    CHECK(NEAR(HUD_DIM_BAND, 16.0f));          /* ★ 滞回带（出的一侧 = 28 + 16 = 44）*/

    /* ② 表面 px → 自机坐标（拿各卡的**真实块**对账，见各自文件里的注释）：
     *    116 季节：货架槽 (257, 924) + 104×36 ⇒ x∈[−191.5,−139.5] y∈[428,446] */
    memset(&r, 0xcc, sizeof r);
    hud_dim_rect_surface(&r, 257.0f, 924.0f, 104.0f, 36.0f);
    CHECK(NEAR(r.x0, -191.5f) && NEAR(r.x1, -139.5f));
    CHECK(NEAR(r.y0, 428.0f) && NEAR(r.y1, 446.0f));
    /*    115 绀珠传面板：UI 空间 (575.5, 673.5) 左下 + 320×272 ⇒ x∈[−32.25,127.75] y∈[184.75,320.75] */
    hud_dim_rect_surface(&r, 575.5f, 673.5f, 320.0f, 272.0f);
    CHECK(NEAR(r.x0, -32.25f) && NEAR(r.x1, 127.75f));
    CHECK(NEAR(r.y0, 184.75f) && NEAR(r.y1, 320.75f));
    /*    107 妖妖梦：(257,924) + 200×40 ⇒ x∈[−191.5,−91.5] y∈[426,446]；113 指示槽：(248,928)+144×96 */
    hud_dim_rect_surface(&r, 257.0f, 924.0f, 200.0f, 40.0f);
    CHECK(NEAR(r.x0, -191.5f) && NEAR(r.x1, -91.5f) && NEAR(r.y0, 426.0f) && NEAR(r.y1, 446.0f));
    /*    113 指示槽：(248, **936** 下沿) + 144×96（外框下沿 936 = 基点 928 + 脚本 pos.y 8）*/
    hud_dim_rect_surface(&r, 248.0f, 936.0f, 144.0f, 96.0f);
    CHECK(NEAR(r.x0, -196.0f) && NEAR(r.x1, -124.0f) && NEAR(r.y0, 404.0f) && NEAR(r.y1, 452.0f));
    /*    117 动物灵槽：**可见整排** (64, 928) + 192×48（★ 2026-09-15i；**不是**声明块 (88,924)+193×48）
     *    ⇒ x∈[−288,−192]、y∈[−… ] 上沿 424 / 下沿 448。对照：声明块会算成 x∈[−276,−179.5] y∈[422,446]
     *    —— 差 24 表面 px = 12 自机 px，来源是「88 是首格格心、不是整排左沿」（`cards/animal.c` 的注）。*/
    hud_dim_rect_surface(&r, 64.0f, 928.0f, 192.0f, 48.0f);
    CHECK(NEAR(r.x0, -288.0f) && NEAR(r.x1, -192.0f));
    CHECK(NEAR(r.y0, 424.0f) && NEAR(r.y1, 448.0f));
    /*    110+111 组合 UI：(257,924) + 188×60（= 640 空间的 94×30）⇒ x∈[−191.5,−97.5] y∈[416,446]
     *    ★ 这一条就是 2026-09-15h 的换算订正（**320** 而不是 224）的机器判据：
     *      按 224 会算出 x∈[−95.5,−1.5] —— 偏 96 自机 px（`test_faith_hud_core.c` 里也钉了一遍）。*/
    hud_dim_rect_surface(&r, 257.0f, 924.0f, 188.0f, 60.0f);
    CHECK(NEAR(r.x0, -191.5f) && NEAR(r.x1, -97.5f));
    CHECK(NEAR(r.y0, 416.0f) && NEAR(r.y1, 446.0f));
    hud_dim_rect_surface(NULL, 0.0f, 0.0f, 1.0f, 1.0f);          /* 空指针：不崩 */

    /* ③ 初始化：**满亮**（不是 0）、闩锁关。哨兵填 0xcc 再 init，确认两个字段都被写过 */
    memset(&d, 0xcc, sizeof d);
    hud_dim_init(&d);
    CHECK(d.dim == 0 && d.alpha == HUD_DIM_ALPHA_FULL);
    hud_dim_init(NULL);                                          /* 空指针：不崩 */

    /* ④ 门（进）：方块 r = {0,0,100,100}（自机坐标）。**进 = 落在「矩形 + 28」之内**，四条边各一条判据
     *   （四条各缺一都不进，且 alpha 一个字节不动）*/
    r.x0 = 0.0f; r.y0 = 0.0f; r.x1 = 100.0f; r.y1 = 100.0f;
    memset(&d, 0xcc, sizeof d);
    hud_dim_init(&d);
    CHECK(tick(&d, &r, -28.1f, 50.0f) == 255);       /* x 差 0.1 出「矩形 + 28」⇒ 不进 */
    CHECK(d.dim == 0);
    CHECK(tick(&d, &r, 128.1f, 50.0f) == 255);       /* x > x1 + 28 ⇒ 不进 */
    CHECK(tick(&d, &r, 50.0f, -28.1f) == 255);       /* y 侧同 */
    CHECK(tick(&d, &r, 50.0f, 128.1f) == 255);
    CHECK(d.dim == 0 && d.alpha == 255);
    CHECK(tick(&d, &r, -28.0f, 50.0f) == 199);       /* 恰在「矩形 + 28」的沿上（含边界）⇒ 进，本帧走一步 */
    CHECK(d.dim == 1);
    CHECK(tick(&d, &r, 128.0f, 50.0f) == 143);       /* 恰在右沿 + 28 上也算在内 */
    CHECK(d.dim == 1);
    CHECK(tick(&d, &r, 50.0f, -28.0f) == 87);       /* 上沿 − 28 */
    CHECK(d.dim == 1);
    CHECK(tick(&d, &r, 50.0f, 128.0f) == 32);        /* 下沿 + 28 */
    CHECK(d.dim == 1);
    CHECK(tick(&d, &r, 50.0f, 50.0f) == 32);         /* 矩形正中：仍在区内，钳位到 32 */
    CHECK(tick(&d, &r, 0.0f, 0.0f) == 32);           /* 矩形的角（原本的边界值）当然也在内 */
    CHECK(d.dim == 1);

    /* ⑤ 滞回带 16 px：闩锁开着时，走到「矩形 + 44」（= 28 + 16）之内**保持压暗**，越过 44 才淡回
     *   （注意：带内**不进**新的压暗 —— 「进」必须在「矩形 + 28」里）*/
    memset(&d, 0xcc, sizeof d);
    hud_dim_init(&d);
    CHECK(tick(&d, &r, 128.0f, 50.0f) == 199);       /* 进（贴「矩形 + 28」的右沿）*/
    CHECK(tick(&d, &r, 138.0f, 50.0f) == 143);       /* 138 ∈ (128, 144] ⇒ 迟滞带内：保持 */
    CHECK(d.dim == 1);
    CHECK(tick(&d, &r, 144.0f, 50.0f) == 87);       /* 恰在 100+44：`> 144` 不成立 ⇒ 仍保持 */
    CHECK(d.dim == 1);
    CHECK(tick(&d, &r, 144.1f, 50.0f) == 143);       /* 越过 ⇒ 淡回（从 102 往回走，本帧掉头）*/
    CHECK(d.dim == 0);
    CHECK(tick(&d, &r, 140.0f, 50.0f) == 199);       /* 140 在迟滞带里但**不在「矩形 + 28」内** ⇒ 不再进 */
    CHECK(d.dim == 0);
    CHECK(tick(&d, &r, 128.0f, 50.0f) == 143);       /* 回到「矩形 + 28」的沿上 ⇒ 从 204 往下走 */
    CHECK(d.dim == 1);
    /* y 侧同一条带（上下各 44 = 28 + 16） */
    memset(&d, 0xcc, sizeof d);
    hud_dim_init(&d);
    CHECK(tick(&d, &r, 50.0f, -28.0f) == 199);       /* 贴上沿 − 28 进 */
    CHECK(tick(&d, &r, 50.0f, -44.0f) == 143);       /* −44 ∈ [−44, −28) ⇒ 迟滞带内保持 */
    CHECK(d.dim == 1);
    CHECK(tick(&d, &r, 50.0f, -44.1f) == 199);       /* 越过 ⇒ 出（143 + 56）*/
    CHECK(d.dim == 0);
    CHECK(tick(&d, &r, 50.0f, 140.0f) == 255);       /* 在 (100+28, 100+44] 里：**不算进**，继续淡回到 255 */
    CHECK(d.dim == 0);
    CHECK(tick(&d, &r, 50.0f, 128.0f) == 199);       /* 回到「矩形 + 28」的沿上（贴下沿）⇒ 再进 */
    CHECK(d.dim == 1);
    CHECK(tick(&d, &r, 50.0f, 128.0f) == 143);       /* 确认真的在往下走 */
    CHECK(tick(&d, &r, 50.0f, 100.0f) == 87);       /* 回到矩形内（贴下沿）⇒ 一路压下去 */

    /* ⑥ 步长与钳位：255 → 32 恰好 **4 帧**（255−56×3 = 87，第 4 帧 87−56 = 31 ⇒ 钳到 32），之后钉住；
     *   32 → 255 也是 **4 帧**（32+56×3 = 200 ⇒ 第 4 帧 256 ⇒ 钳到 255）。★ 2026-09-15j：4 帧 = 脚本侧
     *   `alphaTime(4, 0, 32)` / `alphaTime(4, 0, 255)` 的帧数（用户口径「过渡帧得根治」= 两边同帧数）*/
    memset(&d, 0xcc, sizeof d);
    hud_dim_init(&d);
    CHECK(tick(&d, &r, 50.0f, 50.0f) == 199);
    CHECK(tick(&d, &r, 50.0f, 50.0f) == 143);
    CHECK(tick(&d, &r, 50.0f, 50.0f) == 87);
    CHECK(tick(&d, &r, 50.0f, 50.0f) == 32);
    CHECK(tick(&d, &r, 50.0f, 50.0f) == HUD_DIM_ALPHA_DIM);   /* 钳位到 32（不是 0）*/
    CHECK(tick(&d, &r, 50.0f, 50.0f) == 32);                  /* 稳态：不再掉 */
    CHECK(tick(&d, &r, 999.0f, 999.0f) == 88);                /* 离开 ⇒ 83 */
    CHECK(tick(&d, &r, 999.0f, 999.0f) == 144);
    CHECK(tick(&d, &r, 999.0f, 999.0f) == 200);
    CHECK(tick(&d, &r, 999.0f, 999.0f) == 255);
    CHECK(tick(&d, &r, 999.0f, 999.0f) == 255);
    CHECK(tick(&d, &r, 999.0f, 999.0f) == 255);

    /* ⑦ `pp == NULL`（菜单 / 商店 / 结算里没有自机）：关着的闩锁置不上；开着的收掉并朝 255 淡回 */
    CHECK(hud_dim_gate(0, &r, NULL) == 0);
    CHECK(hud_dim_gate(1, &r, NULL) == 0);
    memset(&d, 0xcc, sizeof d);
    hud_dim_init(&d);
    CHECK(hud_dim_tick(&d, &r, NULL) == 255);
    CHECK(d.dim == 0 && d.alpha == 255);
    CHECK(tick(&d, &r, 50.0f, 50.0f) == 199);
    CHECK(tick(&d, &r, 50.0f, 50.0f) == 143);
    CHECK(hud_dim_tick(&d, &r, NULL) == 199);        /* 闩锁被 NULL 收掉，本帧就开始淡回 */
    CHECK(d.dim == 0);
    CHECK(hud_dim_tick(&d, &r, NULL) == 255);
    CHECK(hud_dim_tick(&d, &r, NULL) == 255);

    /* ⑧ 跨帧连续性：走到一半反向 ⇒ 从**当前值**接着走，不跳变、不重置 */
    memset(&d, 0xcc, sizeof d);
    hud_dim_init(&d);
    CHECK(tick(&d, &r, 50.0f, 50.0f) == 199);
    CHECK(tick(&d, &r, 50.0f, 50.0f) == 143);        /* 压暗中途（143）*/
    CHECK(tick(&d, &r, 999.0f, 999.0f) == 199);      /* 离开：143 + 56 = 199（不是回到 255、也不是从 32 起）*/
    CHECK(tick(&d, &r, 50.0f, 50.0f) == 143);        /* 又进去：199 − 56 */
    CHECK(d.dim == 1);

    /* ⑨ 空结构体 / 空矩形：不崩、返回满亮（卡侧 `ce_state` 拿不到时不该画成黑）*/
    CHECK(hud_dim_tick(NULL, &r, NULL) == HUD_DIM_ALPHA_FULL);
    {
        const float pp[2] = { 50.0f, 50.0f };
        memset(&d, 0xcc, sizeof d);
        hud_dim_init(&d);
        CHECK(hud_dim_tick(&d, NULL, pp) == 255);
        CHECK(hud_dim_tick(NULL, NULL, pp) == HUD_DIM_ALPHA_FULL);
        CHECK(hud_dim_gate(1, NULL, pp) == 1);       /* 没给矩形 ⇒ 保持上一次（不乱切）*/
        CHECK(hud_dim_gate(0, NULL, pp) == 0);
    }

    /* ⑩ ★ 六块真块：门限 + **可触发区不变式**（2026-09-15h 的核心判据）
     *   「进」= 矩形 + 28、「出」= 矩形 + 44；可触发区 = 「进」区 ∩ 自机钳位。
     *   ⚠ 不变式：**每块的可触发区都必须真的点得亮**（高 ≥ 32、宽 ≥ 该块的 `minw`，默认 24）。
     *     2026-09-15g 的「进 = 矩形之内」会让左下角那几块只剩 4 / 10 / 6 px 高 —— 那正是被订正掉的东西。*/
    {
        static const struct { const char *name; float x, y, w, h, minw; } blk[] = {
            /* `minw` = 该块可触发区宽度的下限（默认 24）；**117 单独一档 20** —— 见下面那段注 */
            { "116 天空璋", 257.0f, 924.0f, 104.0f,  36.0f, 24.0f },
            { "117 鬼形兽",  64.0f, 928.0f, 192.0f,  48.0f, 20.0f },
            { "107 妖妖梦", 257.0f, 924.0f, 200.0f,  40.0f, 24.0f },
            { "113 神灵庙", 248.0f, 936.0f, 144.0f,  96.0f, 24.0f },
            { "115 绀珠传", 575.5f,673.5f, 320.0f, 272.0f, 24.0f },
            { "110+111",    257.0f, 924.0f, 188.0f,  60.0f, 24.0f },
        };
        unsigned i;

        /* ★★ 2026-09-15i：117 那一行从**声明块** `(88, 924)+193×48` 改成**可见整排** `(64, 928)+192×48`。
         *   为什么：`ANIMAL_HUD_PIN_X 88` 在绘制里是**首格的格心**（`cards/animal.c` 的
         *   `hud_sync: x = s_slot.x + 36·i` / `y = s_slot.y − 20`）⇒ 可见整排 = 64..256（5 格盒各 48 宽、
         *   间距 36 ⇒ 48 + 4×36 = 192）、y 880..928；声明块 88..281 比它**整体右移 24 px**。
         *   压暗的口径是「HUD 画在哪、判据就跟着哪」⇒ 用**可见**那份（`ANIMAL_HUD_VIS_*`、
         *   `hud_fade_tick()`；登记 `AUDIT.md` §DD-8 / `TUNING.md` §7.2）。
         *   ⚠ 它的 `minw` = **20**（不是 24）：本排整排画在游玩区左边缘（256/257）**之外**，
         *     而自机中心最左只能到表面 272 ⇒ 进区 = x ∈ [−184, −164] = **20 px 是几何事实**，
         *     不是回归。旧口径那 32.5 px 里有一半来自声明块那 24 px 的记账偏移（不是本排真能被碰到）。
         *     要它更宽：`HUD_DIM_MARGIN`（一个宏、全卡）或**挪本排**（摆位口径，不归 `cards/hud_dim.*` 这层）。*/

        for (i = 0; i < sizeof blk / sizeof blk[0]; ++i) {
            float ix0, ix1, iy0, iy1, ox0, ox1, oy0, oy1;
            float vx0, vx1, vy0, vy1;

            hud_dim_rect_surface(&r, blk[i].x, blk[i].y, blk[i].w, blk[i].h);
            ix0 = r.x0 - HUD_DIM_MARGIN; ix1 = r.x1 + HUD_DIM_MARGIN;   /* 进 */
            iy0 = r.y0 - HUD_DIM_MARGIN; iy1 = r.y1 + HUD_DIM_MARGIN;
            ox0 = r.x0 - (HUD_DIM_MARGIN + HUD_DIM_BAND);               /* 出 */
            ox1 = r.x1 + (HUD_DIM_MARGIN + HUD_DIM_BAND);
            oy0 = r.y0 - (HUD_DIM_MARGIN + HUD_DIM_BAND);
            oy1 = r.y1 + (HUD_DIM_MARGIN + HUD_DIM_BAND);
            CHECK(ox0 < ix0 && ox1 > ix1 && oy0 < iy0 && oy1 > iy1);    /* 出的一侧必须把进的一侧包住 */

            /* 用真判据验四条沿（不是只看算式）：贴进沿 ⇒ 进；差 0.1 px 出进沿 ⇒ 不进 */
            memset(&d, 0xcc, sizeof d);
            hud_dim_init(&d);
            CHECK(tick(&d, &r, ix0, (iy0 + iy1) * 0.5f) == 199);
            CHECK(d.dim == 1);
            memset(&d, 0xcc, sizeof d);
            hud_dim_init(&d);
            CHECK(tick(&d, &r, ix0 - 0.1f, (iy0 + iy1) * 0.5f) == 255);
            CHECK(d.dim == 0);
            /* 闩锁开着时贴出沿 ⇒ 保持；越 0.1 px ⇒ 出 */
            memset(&d, 0xcc, sizeof d);
            hud_dim_init(&d);
            CHECK(tick(&d, &r, (ix0 + ix1) * 0.5f, (iy0 + iy1) * 0.5f) == 199);
            CHECK(tick(&d, &r, ox1, (iy0 + iy1) * 0.5f) == 143);        /* 贴出沿：保持（dim 仍 1）*/
            CHECK(d.dim == 1);
            CHECK(tick(&d, &r, ox1 + 0.1f, (iy0 + iy1) * 0.5f) == 199); /* 越 ⇒ 出 */
            CHECK(d.dim == 0);

            /* 可触发区 = 「进」∩ 自机钳位（自机够不到的地方不算） */
            vx0 = ix0 > PP_X_MIN ? ix0 : PP_X_MIN;  vx1 = ix1 < PP_X_MAX ? ix1 : PP_X_MAX;
            vy0 = iy0 > PP_Y_MIN ? iy0 : PP_Y_MIN;  vy1 = iy1 < PP_Y_MAX ? iy1 : PP_Y_MAX;
            if (!(vx1 - vx0 >= blk[i].minw))
                printf("  FAIL %s：可触发区宽 %.1f px（要求 ≥ %.0f）\n",
                       blk[i].name, (double)(vx1 - vx0), (double)blk[i].minw);
            CHECK(vx1 - vx0 >= blk[i].minw);
            if (!(vy1 - vy0 >= 32.0f))
                printf("  FAIL %s：可触发区高 %.1f px（要求 ≥ 32）\n", blk[i].name, (double)(vy1 - vy0));
            CHECK(vy1 - vy0 >= 32.0f);
            /* ⚠ 反面记录：这六块里 116/117/110+111 若退回「进 = 矩形之内」，高度只有 4 / 10 / 16 px
             *   ⇒ 上面那条 ≥ 32 的不变式会红（改动前 `hud_dim_gate` 就是那个口径）。*/
        }
    }

    printf("test_hud_dim_core: %d passed, %d failed\n", s_pass, s_fail);
    return s_fail ? 1 : 0;
}
