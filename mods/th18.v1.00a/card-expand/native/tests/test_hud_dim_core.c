/* test_hud_dim_core.c —— `cards/hud_dim.c` 的主机单测（`make test-host`）。
 *
 * 钉三件事（**跨卡统一件**，116/117/107/113/115 + 110/111 共用这一条压暗路）：
 *   ① **表面 px → 自机坐标** 的定式（`自机 = 表面/2 − (320,16)`）——各卡的门限都从块矩形推出来，
 *      换算错了每张卡的门都会偏（`224` 是过时记法，2026-09-15h 订正，见 `hud_dim.h` 文件头）；
 *   ② **统一的那套动态**：**进 = 块矩形外扩 `HUD_DIM_MARGIN`(28)**、**出 = 离开「矩形 + 44」**
 *      （= 28 + `HUD_DIM_BAND` 16 ⇒ **16 px 滞回带**）、终点 alpha **128**（50% 透明）/ 起始 255 /
 *      **过渡 20 帧线性**、`pp == NULL` 的行为、逐帧连续性；
 *   ③ **六块真块的「可触发区」不变式**（§⑩）：每块的「进」区与自机钳位 `x∈[−184,184] / y∈[32,432]`
 *      相交之后必须留下**真的点得亮**的一片（高 ≥ 32 px、宽 ≥ 24 px）—— 这就是 2026-09-15h
 *      把「进 = 矩形之内」改成「矩形 + 边距」要保的那个性质（改回"之内"会让 116/117/107 只剩 4–10 px 高）。
 * **性质 = ❓ 用户口径（UI 统一），不是复刻** —— 见 `hud_dim.h` 文件头。
 *
 * ⚠ **取值口径（2026-09-21 换基准）**：终点 **128** / 过渡 **20 帧线性**（`HUD_DIM_ALPHA_DIM` /
 *   `HUD_DIM_FRAMES`；owner 拍定「20 帧 / 50% 是 UI ⇒ 统一成一个值，以 115 那套为基准」；
 *   20 帧的一手 = TH15 `front.anm` script241 的 `alphaTime(20, 0, 0)`）。旧口径 = 32 / 每帧 ±56（4 帧）。
 * ⚠ 断言里的 `249/243/…/128` 是 `HUD_DIM_ALPHA_DIM` 与 `HUD_DIM_FRAMES` 的**算术结果**
 *   （`from + Δ·t/20`，Δ = −127），不是一手值；改这两个宏会红 —— 那正是要的。
 *   同理 `28 / 16 / 44` 由 `HUD_DIM_MARGIN` / `HUD_DIM_BAND` 推。
 * ⚠ 与**脚本侧**同帧数同终点：`assets/ability/scripts/123–127,175–178` 的 label 4/5 写成
 *   `alphaTime(20, 0, 255)` / `alphaTime(20, 0, 128)`，机器判据 = `native/tools/anm_hud_alpha_check.py`
 *   （`make check` 里跑）—— 那边改了三处要一起改（`hud_dim.h` 文件头的「旧值留档」列了清单）。*/
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

/* `alphaTime(HUD_DIM_FRAMES, 0, target)` 的逐帧值：第 t 帧 = `from + (target−from)·t/FRAMES`（t = 1..FRAMES）。
 * 一次过渡的**完整**帧表；`out[i]` = 第 i+1 帧。*/
static void ramp_table(int32_t from, int32_t target, int32_t *out)
{
    int t;
    for (t = 0; t < HUD_DIM_FRAMES; ++t)
        out[t] = from + (target - from) * (t + 1) / HUD_DIM_FRAMES;
}

/* 斜坡的**第一帧**：从满亮起步、朝 128 走一帧（= `HUD_DIM_ALPHA_FULL − 127/20` = 249）。
 * 用它而不是字面量，是为了让「换了终点/帧数宏」时只有一处要跟。*/
#define DIM_FRAME1 (HUD_DIM_ALPHA_FULL + (HUD_DIM_ALPHA_DIM - HUD_DIM_ALPHA_FULL) / HUD_DIM_FRAMES)

/* 自机钳位（一手 `th18:0x45b170`）：x ∈ [−184,184]、y ∈ [32,432]。*/
#define PP_X_MIN (-184.0f)
#define PP_X_MAX ( 184.0f)
#define PP_Y_MIN (  32.0f)
#define PP_Y_MAX ( 432.0f)

int main(void)
{
    hud_dim_t d;
    hud_dim_rect_t r;
    int32_t a1, a2, a3, k;

    /* ① 统一取值本身（用户口径）*/
    CHECK(HUD_DIM_ALPHA_FULL == 255 && HUD_DIM_ALPHA_DIM == 128);   /* ★ 2026-09-21：终点 128 = 50% 透明 */
    CHECK(HUD_DIM_FRAMES == 20);                                    /* ★ 2026-09-21：20 帧线性（旧 ±56 = 4 帧）*/
    CHECK(DIM_FRAME1 == 249);                                       /* 斜坡首帧（Δ = −127，取整向零）*/
    CHECK(NEAR(HUD_DIM_MARGIN, 28.0f));        /* ★ 进的边距（❓ 用户口径 2026-09-15h；一手各作 24–32）*/
    CHECK(NEAR(HUD_DIM_BAND, 16.0f));          /* ★ 滞回带（出的一侧 = 28 + 16 = 44）*/

    /* ② 表面 px → 自机坐标（拿各卡的**真实块**对账，见各自文件里的注释）：
     *    116 季节：货架槽 (257, 924) + 104×36 ⇒ x∈[−191.5,−139.5] y∈[428,446] */
    memset(&r, 0xcc, sizeof r);
    hud_dim_rect_surface(&r, 257.0f, 924.0f, 104.0f, 36.0f);
    CHECK(NEAR(r.x0, -191.5f) && NEAR(r.x1, -139.5f));
    CHECK(NEAR(r.y0, 428.0f) && NEAR(r.y1, 446.0f));
    /*    115 绀珠传面板：⚠️ **2026-09-21 订正** —— 可见块 = 表面 (479.5, 657.5) 左下 + **256**×272
     *    （各条 `pos ± (96,16)` 的并集；`pos` 是**图心**不是左上角）⇒ x∈[−80.25,47.75] y∈[176.75,312.75]。
     *    旧值 (575.5,673.5)+320×272 是按左上角 + 256 画布算的 ⇒ 右偏 96–160、下偏 16 表面 px。
     *    一手与改法：`TUNING.md` §7.5 / `engine/chapter/th15/02-chapter-panel-geometry-and-fades.md`。*/
    hud_dim_rect_surface(&r, 479.5f, 657.5f, 256.0f, 272.0f);
    CHECK(NEAR(r.x0, -80.25f) && NEAR(r.x1, 47.75f));
    CHECK(NEAR(r.y0, 176.75f) && NEAR(r.y1, 312.75f));
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

    /* ③ 初始化：**满亮**（不是 0）、闩锁关、帧计数 = 稳态。哨兵填 0xcc 再 init，确认四格都被写过 */
    memset(&d, 0xcc, sizeof d);
    hud_dim_init(&d);
    CHECK(d.dim == 0 && d.alpha == HUD_DIM_ALPHA_FULL);
    CHECK(d.from == HUD_DIM_ALPHA_FULL && d.t == HUD_DIM_FRAMES);   /* ★ 2026-09-21：稳态 ⇒ 第一帧不起斜坡 */
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
    CHECK(tick(&d, &r, -28.0f, 50.0f) == DIM_FRAME1);   /* 恰在「矩形 + 28」的沿上（含边界）⇒ 进，当帧起斜坡 */
    CHECK(d.dim == 1 && d.t == 1);
    CHECK(tick(&d, &r, 128.0f, 50.0f) == 243);       /* 恰在右沿 + 28 上也算在内（斜坡第 2 帧）*/
    CHECK(d.dim == 1);
    CHECK(tick(&d, &r, 50.0f, -28.0f) == 236);       /* 上沿 − 28（第 3 帧）*/
    CHECK(d.dim == 1);
    CHECK(tick(&d, &r, 50.0f, 128.0f) == 230);       /* 下沿 + 28（第 4 帧）*/
    CHECK(d.dim == 1);
    CHECK(tick(&d, &r, 50.0f, 50.0f) == 224);        /* 矩形正中：仍在区内，斜坡继续 */
    CHECK(tick(&d, &r, 0.0f, 0.0f) == 217);          /* 矩形的角（原本的边界值）当然也在内 */
    CHECK(d.dim == 1);

    /* ⑤ 滞回带 16 px：闩锁开着时，走到「矩形 + 44」（= 28 + 16）之内**保持压暗**，越过 44 才淡回
     *   （注意：带内**不进**新的压暗 —— 「进」必须在「矩形 + 28」里）*/
    memset(&d, 0xcc, sizeof d);
    hud_dim_init(&d);
    CHECK(tick(&d, &r, 128.0f, 50.0f) == DIM_FRAME1);   /* 进（贴「矩形 + 28」的右沿）*/
    CHECK(tick(&d, &r, 138.0f, 50.0f) == 243);       /* 138 ∈ (128, 144] ⇒ 迟滞带内：保持（斜坡继续）*/
    CHECK(d.dim == 1);
    CHECK(tick(&d, &r, 144.0f, 50.0f) == 236);       /* 恰在 100+44：`> 144` 不成立 ⇒ 仍保持 */
    CHECK(d.dim == 1);
    a1 = tick(&d, &r, 144.1f, 50.0f);                /* 越过 ⇒ 淡回（当帧从 236 起算，不跳变）*/
    CHECK(d.dim == 0);
    CHECK(a1 >= 236 && a1 < 255);
    a2 = tick(&d, &r, 140.0f, 50.0f);                /* 140 在迟滞带里但**不在「矩形 + 28」内** ⇒ 继续淡回 */
    CHECK(d.dim == 0);
    CHECK(a2 >= a1 && a2 < 255);
    a3 = tick(&d, &r, 128.0f, 50.0f);                /* 回到「矩形 + 28」的沿上 ⇒ 再进（且不是瞬间到 128）*/
    CHECK(d.dim == 1);
    CHECK(a3 < 255 && a3 > HUD_DIM_ALPHA_DIM);
    /* y 侧同一条带（上下各 44 = 28 + 16） */
    memset(&d, 0xcc, sizeof d);
    hud_dim_init(&d);
    CHECK(tick(&d, &r, 50.0f, -28.0f) == DIM_FRAME1);   /* 贴上沿 − 28 进 */
    CHECK(tick(&d, &r, 50.0f, -44.0f) == 243);       /* −44 ∈ [−44, −28) ⇒ 迟滞带内保持 */
    CHECK(d.dim == 1);
    a1 = tick(&d, &r, 50.0f, -44.1f);                /* 越过 ⇒ 出（当帧起淡回）*/
    CHECK(d.dim == 0);
    CHECK(a1 >= 236);
    a2 = tick(&d, &r, 50.0f, 140.0f);                /* 在 (100+28, 100+44] 里：**不算进**，继续淡回 */
    CHECK(d.dim == 0);
    CHECK(a2 >= a1);
    a3 = tick(&d, &r, 50.0f, 128.0f);                /* 回到「矩形 + 28」的沿上（贴下沿）⇒ 再进 */
    CHECK(d.dim == 1);
    CHECK(a3 > HUD_DIM_ALPHA_DIM && a3 <= a2);

    /* ⑥ ★ 过渡 = **20 帧线性**（逐帧对表；`alphaTime(HUD_DIM_FRAMES, 0, target)` 的语义）：
     *   255 → 128 走满 20 帧、第 20 帧**精确**到位、之后钉住；反向同长。
     *   ⚠ 旧口径是 ±56（4 帧）：整数步进做不到精确 20 帧（Δ = 127 ⇒ 6/帧 = 22 帧、7/帧 = 19 帧）
     *     ⇒ 现在是「起点 + 帧计数」（`hud_dim_t` 多了 `from` / `t`）。这一节就是那条性质的机器判据。*/
    {
        int32_t want[HUD_DIM_FRAMES];
        ramp_table(HUD_DIM_ALPHA_FULL, HUD_DIM_ALPHA_DIM, want);
        CHECK(want[0] == 249 && want[HUD_DIM_FRAMES - 1] == HUD_DIM_ALPHA_DIM);
        CHECK(want[0] != HUD_DIM_ALPHA_DIM);                 /* **不是**瞬间到位 */
        for (k = 1; k < HUD_DIM_FRAMES; ++k)
            CHECK(want[k] <= want[k - 1]);                   /* 单调下降 */
        memset(&d, 0xcc, sizeof d);
        hud_dim_init(&d);
        for (k = 0; k < HUD_DIM_FRAMES; ++k)
            CHECK(tick(&d, &r, 50.0f, 50.0f) == want[k]);    /* 逐帧对表 */
        CHECK(d.alpha == HUD_DIM_ALPHA_DIM && d.t == HUD_DIM_FRAMES);
        CHECK(tick(&d, &r, 50.0f, 50.0f) == HUD_DIM_ALPHA_DIM);   /* 稳态：不再掉 */
        CHECK(tick(&d, &r, 50.0f, 50.0f) == HUD_DIM_ALPHA_DIM);
        ramp_table(HUD_DIM_ALPHA_DIM, HUD_DIM_ALPHA_FULL, want);
        CHECK(want[0] == 134 && want[HUD_DIM_FRAMES - 1] == HUD_DIM_ALPHA_FULL);
        for (k = 1; k < HUD_DIM_FRAMES; ++k)
            CHECK(want[k] >= want[k - 1]);                   /* 单调上升 */
        for (k = 0; k < HUD_DIM_FRAMES; ++k)
            CHECK(tick(&d, &r, 999.0f, 999.0f) == want[k]);  /* 反向同长 */
        CHECK(tick(&d, &r, 999.0f, 999.0f) == HUD_DIM_ALPHA_FULL);
    }

    /* ⑦ `pp == NULL`（菜单 / 商店 / 结算里没有自机）：关着的闩锁置不上；开着的收掉并朝 255 淡回 */
    CHECK(hud_dim_gate(0, &r, NULL) == 0);
    CHECK(hud_dim_gate(1, &r, NULL) == 0);
    memset(&d, 0xcc, sizeof d);
    hud_dim_init(&d);
    CHECK(hud_dim_tick(&d, &r, NULL) == 255);
    CHECK(d.dim == 0 && d.alpha == 255);
    CHECK(tick(&d, &r, 50.0f, 50.0f) == 249);
    CHECK(tick(&d, &r, 50.0f, 50.0f) == 243);
    a1 = hud_dim_tick(&d, &r, NULL);                 /* 闩锁被 NULL 收掉，本帧就开始淡回（从 243 起算）*/
    CHECK(d.dim == 0);
    CHECK(a1 >= 243 && a1 < 255);
    for (k = 0; k < HUD_DIM_FRAMES; ++k) (void)hud_dim_tick(&d, &r, NULL);
    CHECK(d.alpha == HUD_DIM_ALPHA_FULL);            /* 20 帧内回到满亮 */
    CHECK(hud_dim_tick(&d, &r, NULL) == HUD_DIM_ALPHA_FULL);

    /* ⑧ 跨帧连续性：走到一半反向 ⇒ 从**当前值**接着走，不跳变、不重置、不瞬间到位 */
    memset(&d, 0xcc, sizeof d);
    hud_dim_init(&d);
    CHECK(tick(&d, &r, 50.0f, 50.0f) == 249);
    CHECK(tick(&d, &r, 50.0f, 50.0f) == 243);        /* 压暗中途（243）*/
    a1 = tick(&d, &r, 999.0f, 999.0f);               /* 离开：从 243 起淡回（不是跳回 255、也不是从 128 起）*/
    CHECK(d.dim == 0);
    CHECK(a1 >= 243 && a1 < 255);
    CHECK(a1 - 243 <= 127 / HUD_DIM_FRAMES + 1);     /* 一帧最多走 Δ/FRAMES（+1 取整余量）⇒ 不跳变 */
    a2 = tick(&d, &r, 50.0f, 50.0f);                 /* 又进去：从当前值往下走 */
    CHECK(d.dim == 1);
    CHECK(a2 <= a1 && a1 - a2 <= 127 / HUD_DIM_FRAMES + 1);

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
            { "115 绀珠传", 479.5f,657.5f, 256.0f, 272.0f, 24.0f },   /* ★ 2026-09-21 订正（图心锚；见下面那段注）*/
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

        /* ★★ 2026-09-21：115 那一行从 `(575.5,673.5)+320×272` 改成**可见块** `(479.5,657.5)+256×272`。
         *   为什么：面板六条子 VM 的 `pos` 是**图心**（TH15 `front.anm` script241 的七个子脚本都不写
         *   `anchor` ⇒ `anchor(0,0)` = 居中），旧值却按「左上角 + 256 画布」算 ⇒ 右偏 96–160、下偏 16 表面 px
         *   （owner 实跑：「放挡触发框似乎比 UI 位置靠下」）。可见块 = 各条 `pos ± (96,16)` 的并集。
         *   一手证据（锚点三条 + 关面板的两段淡出）：`engine/chapter/th15/02-chapter-panel-geometry-and-fades.md`；
         *   常量与实跑口径：`TUNING.md` §7.5。★ 同日 owner 拍定「20 帧 / 50% 统一成一个值、以 115 为基准」
         *   ⇒ **共享件 `hud_dim.{h,c}` 已改成 128 / 20 帧**，本表六块一起走这一套（本文件 §①/§⑥ 钉的就是它）。*/

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
            CHECK(tick(&d, &r, ix0, (iy0 + iy1) * 0.5f) == DIM_FRAME1);
            CHECK(d.dim == 1);
            memset(&d, 0xcc, sizeof d);
            hud_dim_init(&d);
            CHECK(tick(&d, &r, ix0 - 0.1f, (iy0 + iy1) * 0.5f) == 255);
            CHECK(d.dim == 0);
            /* 闩锁开着时贴出沿 ⇒ 保持（且斜坡继续走）；越 0.1 px ⇒ 出（当帧起淡回，不跳变） */
            memset(&d, 0xcc, sizeof d);
            hud_dim_init(&d);
            CHECK(tick(&d, &r, (ix0 + ix1) * 0.5f, (iy0 + iy1) * 0.5f) == DIM_FRAME1);
            CHECK(tick(&d, &r, ox1, (iy0 + iy1) * 0.5f) == 243);        /* 贴出沿：保持（dim 仍 1）*/
            CHECK(d.dim == 1);
            a1 = tick(&d, &r, ox1 + 0.1f, (iy0 + iy1) * 0.5f);          /* 越 ⇒ 出 */
            CHECK(d.dim == 0);
            CHECK(a1 >= 243 && a1 < 255);

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
