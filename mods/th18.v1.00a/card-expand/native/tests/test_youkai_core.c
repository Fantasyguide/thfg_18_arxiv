/* test_youkai_core.c —— youkai_core.c 的主机单测。
 *
 * 语义来源（TH18 v1.00a）：
 *   `docs/superpowers/specs/2026-09-12-youkai-108-design.md`（§3.1 止射闸 / §3.2 本体贴图 / §3.3 子机 /
 *     §3.4 弹幕 / §3.5 使魔 1:1 映射与状态差分 / §3.6 击破信号 / §3.7 伤害转交 1/2 / §4 数值）
 *   `local/_scratch/research-108-damage-transfer.md`（life 差分、取整、上限、自伤回路）
 *   `local/_scratch/research-108-shooter-tick.md`（打包 set id、0x16 那格 fire_rate = 0）
 *   `assets/sht/append_shooterset.py` 的 `REPLACE`（妖侧 shooterset 的 9 行 = TH08 v1.00d
 *     `ply03as.sht` 满 P 档逐字段翻译；槽号/行数会与本文件的断言对账）
 *   `docs/superpowers/plans/2026-09-10-ufo-card-feasibility.md` §1.1（spawn 参数 +0x14 = life）
 *
 * 覆盖：**止射闸的三条门（含 ❓用户口径「子机 0 ⇒ 不压原生弹」）** / 低速判定 / 子机取得与聚焦边沿 /
 *       档位与妖侧弹识别 / shooterset 自检（槽号·行数·fire_rate）/
 *       发射点世界坐标（相对 vs 绝对）/ 1:1 发射口映射 / 状态差分（建·撤·退休·打破·重新武装）/
 *       表满 / 使魔计数 / 全撤 / 伤害转交的取整与上限 / life 采样 / 自伤保护盒 / spawn life 硬约束。
 *
 * ⚠ §3.1 的判据里**第三条**（子机数 > 0）是**用户口径**（2026-09-12），不是一手复刻 ——
 *   断言里那一段逐格与 `yk_option_count` 对账，钉的就是「门与数量表同源」这件事。
 *
 * ⚠ 档位伤害表是**故意不填**（全 `YK_SHOT_DMG_NOT_SET` = 不覆盖；TH08 的单发伤害随 P 下降，
 *   只搬数字会让「P 越高越弱」，理由见 `cards/youkai_core.c` §3.4）
 *   ⇒ 这里只断言**档位映射**与「不覆盖」这个语义。 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* ★★★ 2026-09-27 第二十八批：本批新加的「**生成物不许与常量脱钩**」那一段闸要遍历 spec 目录
 *   （`opendir`/`readdir` 是 POSIX；`-std=c11` 下必须自己开这个特性宏）。*/
#define _DEFAULT_SOURCE 1
#include <dirent.h>

#include "../cards/youkai_core.h"
/* ★★★ 2026-09-28 第七轮：**固定颜色**那条要核「S53 这支资产真存在」⇒ 直接引生成物 `anm_ids.h`
 *   （`assets/build_ability.py` 生成、不入库手改）。这里只做**资产级**存在性检查；
 *   「变体 v ↔ 脚本号」的匹配由卡侧 `yk_orbit_fixed_variant()` 在**运行期**断言（见那一段的注）。*/
#include "../anm_ids.h"
#ifdef __has_include
#  if __has_include("../sht_ids.h")
#    include "../sht_ids.h"     /* 生成物（make sht）：CE_SHT_SET_* */
#  endif
#endif

static int s_fail, s_pass;
#define CHECK(cond) do { if (cond) ++s_pass; else { ++s_fail; printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } } while (0)

/* 读一个文本文件（cwd = native/，与 `tests/test_faith_core.c` 的 `faith_slurp` 同形）。失败返回 0。*/
static char *yk_slurp(const char *path)
{
    FILE *f = fopen(path, "rb");
    char *buf;
    long n;
    if (!f) return 0;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return 0; }
    n = ftell(f);
    if (n < 0 || fseek(f, 0, SEEK_SET) != 0) { fclose(f); return 0; }
    buf = (char *)malloc((size_t)n + 1u);
    if (!buf) { fclose(f); return 0; }
    if (fread(buf, 1u, (size_t)n, f) != (size_t)n) { free(buf); fclose(f); return 0; }
    buf[n] = '\0';
    fclose(f);
    return buf;
}

/* 抠出 `#define <key> <十进制数>` 的值（只认十进制 —— 本批读的那两个键都是整数）。返回 1 = 找到。*/
static int yk_define_int(const char *text, const char *key, int *out)
{
    const char *p = text;
    size_t kl = strlen(key);
    if (!text) return 0;
    while ((p = strstr(p, "#define")) != 0) {
        const char *q = p + 7;
        while (*q == ' ' || *q == '\t') ++q;
        if (strncmp(q, key, kl) == 0 && (q[kl] == ' ' || q[kl] == '\t')) {
            q += kl;
            while (*q == ' ' || *q == '\t') ++q;
            if (*q >= '0' && *q <= '9') { *out = (int)strtol(q, 0, 10); return 1; }
        }
        p += 7;
    }
    return 0;
}

/* ★★★ 第二十八批闸甲：**束脚本的壳常量必须与卡侧对账**（这条闸原先只在
 * `assets/ability/make_youkai_bullet_art.py` 里，而那个生成器**不在** `make check` 的路径上 ——
 * 2026-09-27 真出过事：`YK_BEAM_ANM_LAYER` 改成 14，生成器没重跑 ⇒ `pl01.anm` 里那 5 支束脚本
 * 还写着 `layer(13)`，而 `make check` **全绿**（没有任何闸看得见它）⇒ 束一直画在与自机弹同层。
 * 这里把它搬进 `make check` 跑得到的宿主机单测；生成器那一份保持不动（两边都拦）。
 * 遍历 spec 目录时**按子串找**（`ykb_m_beam`）而不是硬编码「375 号」—— 生成物的编号会随新脚本平移。*/
/* 束 spec 正文里有没有这个子串 —— **只看非注释行**。
 * ★ 第三十二批：本闸的反向判据是「正文里不许出现 `%RANDF`」，而我们的注释**会写到**它
 * （正是为了讲清「为什么不写它」）⇒ 拿 `strstr` 扫全文会把自己的说明读成违规。正反两向都只认正文，
 * 闸才真的钉在**指令**上（顺带也修掉一个既有的弱点：`scale(1.0f, 0.5f);` 原先能被注释行满足）。*/
static int spec_has_live(const char *t, const char *needle)
{
    char line[1024];
    const char *p = t;
    if (!t || !needle) return 0;
    while (*p) {
        const char *eol = strchr(p, '\n');
        size_t len = eol ? (size_t)(eol - p) : strlen(p);
        size_t k = 0;
        if (len >= sizeof line) len = sizeof line - 1;
        memcpy(line, p, len);
        line[len] = 0;
        while (k < len && (line[k] == ' ' || line[k] == '\t')) ++k;
        if (!(k + 1 < len && line[k] == '/' && line[k + 1] == '/') && strstr(line, needle)) return 1;
        if (!eol) break;
        p = eol + 1;
    }
    return 0;
}

/* ★★★ 第三十三批闸：**束贴图必须独占它那条 entry 行带、且横跨整条行宽**。
 *
 * 这条闸修的是一个**两道既有闸都看不见**的 bug（owner 2026-09-27：「**激光是一缕一缕的，形成脉冲**」）：
 *   · 一手（`engine/anm/th16/10-sprite-geometry.md` §4）：`scrollX` 写 `uv_scroll_pos`，绘制时**加到
 *     sprite 四角的 u 上** ⇒ 采样按**整幅贴图宽**取模（`1.0` = 一整幅图集宽）。
 *     ⇒ 绕回那把尺子是 **entry 的宽**，**不是 sprite 的宽**。我们的束贴在 `pl01.anm entry20`
 *     （`player/yk_bullet.png`，768 宽）里只占 `(96,0,512,32)` ⇒ 每 48 帧（768/16）相位扫过那 256
 *     texel 的 **MAIN/HIT/透明带**：束从膛口那端起变透明 → 整条消失 → 再从膛口长回来。
 *   · 三处参照都是「束 sprite 铺满整条图集行」：一手 TH08 `player01` sprite32 = `(0,224,448,32)`
 *     （图集 256 宽，sprite 宽过整幅）、**本作原生** `pl01` sprite29 = `(0,288,512,32)`（同上）、
 *     117 `ATLAS_ANIMAL_BEAM` sprite284 = `(0,64,512,32)`（entry 512 宽 = sprite 宽）。
 *   ⇒ 闸必须同时钉住**资产侧两处**（`YK_BULLET_MEMBERS` 的尺寸 == `YK_M_LASER_BEAM_SPRITE_W/H`、
 *     `YK_BULLET_CANVAS` 的画布宽 == 束成员宽 且 MAIN/HIT 不在束那 32 行里）—— 只看 C 常量看不见。*/
static void check_beam_member_covers_entry(void)
{
    char *py = yk_slurp("../assets/build_player_anm.py");
    char want_member[128], want_canvas[128], want_lay0[64], want_lay2[64];
    CHECK(py != 0);
    if (!py) return;
    /* 成员表：尺寸必须与卡侧那两个常量逐字相等（`(int)` 转换 —— 这两个宏都是整数像素）*/
    (void)snprintf(want_member, sizeof want_member, "\"YKB_M_OPT\", %d, %d)",
                   (int)YK_M_LASER_BEAM_SPRITE_W, (int)YK_M_LASER_BEAM_SPRITE_H);
    CHECK(spec_has_live(py, want_member) != 0);
    /* 画布：宽 = 束成员宽（**这就是「绕回那把尺子」**），高 ≥ 2×32（束那 32 行之外还要放 MAIN/HIT）*/
    (void)snprintf(want_canvas, sizeof want_canvas, "\"pl01\": ((%d, 64), ",
                   (int)YK_M_LASER_BEAM_SPRITE_W);
    CHECK(spec_has_live(py, want_canvas) != 0);
    /* MAIN/HIT 的落点必须在**第 40 行**（束是 0..31，第 32..39 留空当双线性缓冲）*/
    (void)snprintf(want_lay0, sizeof want_lay0, "((0, 40), (0, 0), (640, 40))");
    (void)snprintf(want_lay2, sizeof want_lay2, "(640, 40))");
    CHECK(spec_has_live(py, want_lay0) != 0);
    CHECK(spec_has_live(py, want_lay2) != 0);
    free(py);
    /* 卡侧那两个常量本身：宽必须 = entry 宽（生成器 `ROW_FILL_MEMBERS` 铺的就是这个数）*/
    CHECK(YK_M_LASER_BEAM_SPRITE_W == 768.0f);      /* ★ 第三十三批：512 → 768 = 整条 entry 行宽 */
    CHECK(YK_M_LASER_BEAM_SPRITE_H == 32.0f);
}

static void check_beam_spec_files(void)
{
    DIR *d = opendir("../assets/yk_bullet/scripts");
    struct dirent *de;
    char want_layer[64];
    char *yc;
    int layer = -1, n = 0;

    yc = yk_slurp("cards/youkai.c");
    CHECK(yc != 0);
    if (!yc) return;
    CHECK(yk_define_int(yc, "YK_BEAM_ANM_LAYER", &layer));
    CHECK(layer == 14);         /* owner 定的层序：自机弹 13 < 束 14 < 妖侧子机 15（`TUNING.md` §2.1ccc-7）*/
    free(yc);
    if (layer < 0) return;
    (void)snprintf(want_layer, sizeof want_layer, "layer(%d);", layer);

    CHECK(d != 0);
    if (!d) return;
    while ((de = readdir(d)) != 0) {
        char path[512];
        char *t;
        if (!strstr(de->d_name, "ykb_m_beam") || !strstr(de->d_name, ".anm.txt")) continue;
        (void)snprintf(path, sizeof path, "../assets/yk_bullet/scripts/%s", de->d_name);
        t = yk_slurp(path);
        CHECK(t != 0);
        if (!t) continue;
        CHECK(spec_has_live(t, want_layer) != 0);       /* ← 上面那场漂移在这儿被拦下 */
        /* ★★ 第三十二批：流速按**一手的 16 texel/帧**重算（我们的 entry 宽 768 ⇒ 16/768 = 1/48）。
         * 为什么不是 163 的 1/21：163 按它自己的图集宽（512）归一化 ⇒ 照字面搬进来会快 2.29 倍
         * （1.75 帧/纹样周期 = 超奈奎斯特的频闪）。换算链见 `youkai_core.h` 的同名长注。*/
        CHECK(spec_has_live(t, "scrollX(-0.02083333f);") != 0);
        CHECK(YK_M_LASER_BEAM_SCROLL == 0.02083333f);   /* …与卡侧那个数（两边一起钉）—— 一手 16 texel/帧 */
        CHECK(spec_has_live(t, "alpha(70);") != 0);
        CHECK(YK_M_LASER_BEAM_ALPHA == 70);             /* ★ 第三十四批：176 → **70** = 一手 `player01.anm`
                                                         * script24..28 的 `alpha(70)`（176 是照 117/TH17 抄的，
                                                         * 实跑报「贼亮看不清纹理」——见 `TUNING.md` §2.1ccc-14）*/
        CHECK(spec_has_live(t, "anchor(1, 0);") != 0);  /* 左端锚（`engine/anm/th16/10` §2：h=1 ⇒ 四角 x=0,+1,0,+1）*/
        CHECK(spec_has_live(t, "rotateAuto(") == 0);    /* 方向只有一个来源 = 卡侧每帧写（注释里提到名字不算）*/
        CHECK(spec_has_live(t, "scaleTime") != 0);      /* 只有收束段 interruptLabel(1) 里那一条 */
        /* ★ 第三十一批：**照抄 117 `script163`** 的那几句逐句钉住 —— `rotate` 那一行正是我们前两批
         * 缺的（`gen_beam` 里从来没有它，而两批都没有闸看得见 ⇒ 这才是本闸扩容的理由）。*/
        CHECK(spec_has_live(t, "rotate(0.0f, 0.0f, -1.5707964f);") != 0);
        /* ★★ 第三十二批（**反向闸**）：`fset(%F0, %RANDF)` 必须**不在正文里** ——
         * 一手的 `rate` 份副本是**同一支 VM**（`th08:0x450ad0` 的循环不重算 UV）⇒ 共享 UV 相位；
         * 我们的副本各是一支 VM，12 个随机相位会互相平均掉 ⇒ 纹样被抹平、看不出贴图在流
         * （owner 第三十二批：「至于贴图本身，我实际看不太清它到底是不是往外发射」）。*/
        CHECK(spec_has_live(t, "%RANDF") == 0);
        CHECK(spec_has_live(t, "scrollX(%F0)") == 0);
        CHECK(spec_has_live(t, "+1: // 1") != 0);       /* 163 的 `+1:` 起每帧定值流速 */
        CHECK(spec_has_live(t, "type(1);") != 0);
        CHECK(spec_has_live(t, "blendMode(1);") != 0);
        ++n;
        free(t);
    }
    closedir(d);
    CHECK(n == YK_M_LASER_BEAM_TIERS);                  /* 逐档一支（5 支）*/
}

static yk_action_t s_acts[YK_FAM_MAX];

/* ★★★ 2026-09-30（owner 实跑：「一面 3 只大蝴蝶生成了两次两个使魔」）：
 *   绕转使魔的**「已给过」账**两个纯函数的单测 —— 覆盖工单点名的三条：
 *     ① 已给过 ⇒ 不给；② 父敌消失后复现 ⇒ 可给；③ 表满 ⇒ 不给（宁可不补）。*/
static void check_orbit_gift_ledger(void)
{
    unsigned absent = 0;
    /* ① 已给过（无论表满不满）⇒ 拒 */
    CHECK(yk_orbit_gift_verdict(1, 0) == YK_ORBIT_GIFT_DONE);
    CHECK(yk_orbit_gift_verdict(1, 1) == YK_ORBIT_GIFT_DONE);   /* 已给过优先于表满 */
    /* 没给过 + 有位 ⇒ 可以补 */
    CHECK(yk_orbit_gift_verdict(0, 0) == YK_ORBIT_GIFT_OK);
    /* ③ 没给过但表满 ⇒ 宁可不补（fail closed：绝不冒「重复补」的险）*/
    CHECK(yk_orbit_gift_verdict(0, 1) == YK_ORBIT_GIFT_FULL);
    /* ② 父敌还在 ⇒ 记录留着（与使魔死活无关）：连续多少次 present 都不清 */
    absent = 7u;
    CHECK(yk_orbit_gift_prune(&absent, 1) == 0);
    CHECK(absent == 0u);
    /* ② 父敌消失：到 `YK_ORBIT_GIFTED_GRACE` 次才清；之前一直不清 */
    absent = 0u;
    CHECK(yk_orbit_gift_prune(&absent, 0) == (YK_ORBIT_GIFTED_GRACE <= 1u ? 1 : 0));
    if (YK_ORBIT_GIFTED_GRACE > 1u) {
        unsigned k;
        int cleared = 0;
        for (k = 2; k <= YK_ORBIT_GIFTED_GRACE; ++k)
            cleared = yk_orbit_gift_prune(&absent, 0);
        CHECK(cleared == 1);
        CHECK(absent == 0u);                     /* 清掉后计数归零 ⇒ 同名 id 复现可重新给 */
    }
    CHECK(yk_orbit_gift_prune(NULL, 0) == 0);    /* 空指针不炸 */
}

/* ★★★ 2026-09-30（owner 口径：13 颗「多绕几圈」）：`yk_orbit_layout()` 的表 —— 一手样本：
 *   8 台 = 3+5（30/60、ω 6/4.5、内外反向）、13 台 = 3+5+5（30/60/90、ω 6/4.5/3、内反向、中/外同向）、
 *   15 台单圈（半径 60、ω 6、360/N 等分）；14–18 台回落单圈（🟡 一手先例的外推）。*/
static void check_orbit_layout(void)
{
    yk_orbit_layout_t a, b;
    int k;
    /* ① ≤4 台（与 5/6/7、9–12、14–18 台一样）⇒ 单圈：半径 60、ω 6°、初角 360/N 等分 */
    CHECK(yk_orbit_layout(4, 0, &a) && a.radius == 60.0f && a.omega_deg == 6.0f && a.ring == 0);
    CHECK(yk_orbit_layout(4, 1, &b) && b.phase_deg == 90.0f);          /* 360/4 */
    /* ★ 2026-09-30（owner 口径「应当是多个环」）：14–18 台不再是单圈 ⇒ 下面 check_orbit_rings_14_18() 专测 */
    /* ② 8 台 = 3 + 5：内圈 30/ω6、外圈 60/ω4.5，且**内外反向** */
    CHECK(yk_orbit_layout(8, 0, &a) && a.ring == 0 && a.radius == 30.0f && a.omega_deg == 6.0f);
    CHECK(yk_orbit_layout(8, 2, &a) && a.radius == 30.0f && a.phase_deg == 120.0f * 2.0f);
    CHECK(yk_orbit_layout(8, 3, &b) && b.ring == 1 && b.radius == 60.0f && b.omega_deg == -4.5f);
    CHECK(yk_orbit_layout(8, 4, &b) && b.phase_deg == 72.0f);          /* 72° 等分 */
    CHECK(yk_orbit_layout(8, 0, &a));
    CHECK(yk_orbit_layout(8, 3, &b));
    CHECK(a.omega_deg * b.omega_deg < 0.0f);                 /* 内外**反向** */
    /* ③ 13 台 = 3 + 5 + 5 */
    CHECK(yk_orbit_layout(13, 0, &a) && a.ring == 0 && a.radius == 30.0f && a.omega_deg == 6.0f);   /* 口径：内圈 + */
    CHECK(yk_orbit_layout(13, 3, &a) && a.ring == 1 && a.radius == 60.0f && a.omega_deg == -4.5f);
    CHECK(yk_orbit_layout(13, 8, &b) && b.ring == 2 && b.radius == 90.0f && b.omega_deg == 3.0f);   /* 中/外不再同向 */
    CHECK(a.omega_deg * b.omega_deg < 0.0f);                 /* ★ 口径：相邻反向 */
    CHECK(yk_orbit_layout(13, 3, &a));
    CHECK(yk_orbit_layout(13, 8, &b));
    CHECK(b.phase_deg == a.phase_deg);                       /* 中/外**共用初角**（一手就是这样）*/
    CHECK(a.omega_deg * b.omega_deg < 0.0f);                 /* ★ 口径：相邻反向（一手是 −3/+3 同向，已偏离）*/
    CHECK(yk_orbit_layout(13, 7, &a) && a.phase_deg == 72.0f * 4.0f);
    /* ④ 参数越界 / NULL 不炸 */
    CHECK(!yk_orbit_layout(0, 0, &a));
    CHECK(!yk_orbit_layout(8, 8, &a));
    CHECK(!yk_orbit_layout(8, -1, &a));
    CHECK(!yk_orbit_layout(8, 0, NULL));
    /* ⑤ 初角不重合：13 台里 3 圈的初角集合两两不撞（中/外那 5+5 是**故意**同初角，按圈分别看）*/
    for (k = 0; k < 3; ++k) {
        yk_orbit_layout_t p, q;
        int j;
        CHECK(yk_orbit_layout(13, k, &p));
        for (j = k + 1; j < 3; ++j) {
            CHECK(yk_orbit_layout(13, j, &q));
            CHECK(p.phase_deg != q.phase_deg);
        }
    }
    /* ⑥ 每台半径（`yk_orbit_radius_at_r`）：按**该台**最终半径外扩，20 帧到位 */
    {   float r = -1.0f;
        CHECK(yk_orbit_radius_at_r(30.0f, 20, &r) && r == 30.0f);
        CHECK(yk_orbit_radius_at_r(90.0f, 10, &r) && r > 44.9f && r < 45.1f);   /* 90/20*10 */
        CHECK(yk_orbit_radius_at_r(60.0f, 0, &r) && r == 0.0f);
        CHECK(!yk_orbit_radius_at_r(60.0f, 5, NULL));
    }
}

/* ★★★ 2026-09-30（owner：「『一堆使魔但是只有一个环』的意思是，**它应当是多个环**」——
 *   这里「环」= **绕转圈数**）：14–18 台那档从「单圈」改成**多圈同心**（❓口径外推，一手无样本）。
 *   规则：圈数 = ceil(N/4) 钳 [3,5]；每圈台数尽量均匀（余数摊外圈）；半径 = 30·(圈+1) 等间隔；
 *   ω = 6/4.5/3/2.25/1.6875（照 5 面递减）；**内圈反向**、其余同向；初角按该圈台数等分。*/
static void check_orbit_rings_14_18(void)
{
    static const unsigned want_rings[5] = { 3, 3, 3, 3, 3 };        /* ★ 14–18 ⇒ **3 圈**（owner：不能到 4 圈，
                                                                    *   内圈 4–6 台、外圈 ≥ 内圈）*/
    unsigned n;
    for (n = 14; n <= 18; ++n) {
        unsigned rings = want_rings[n - 14];
        unsigned r, k;
        unsigned sum[5] = { 0, 0, 0, 0, 0 };
        unsigned prev_phase[5];
        int seen[5] = { 0, 0, 0, 0, 0 };
        float prev_r = 0.0f;
        for (k = 0; k < n; ++k) {
            yk_orbit_layout_t L;
            CHECK(yk_orbit_layout((int)n, (int)k, &L));
            CHECK(L.ring >= 0 && (unsigned)L.ring < rings);
            sum[L.ring]++;
            /* 半径 = 30·(圈+1) 等间隔 */
            CHECK(L.radius == 30.0f * (float)(L.ring + 1));
            /* ω：内圈反向、其余同向，且绝对值不超过 6 */
            /* ★ 口径（owner：相邻反向、内圈 +）：ring0 正、ring1 负、ring2 正 */
            if (L.ring == 0) CHECK(L.omega_deg > 0.0f);
            else if (L.ring == 1) CHECK(L.omega_deg < 0.0f);
            else CHECK(L.omega_deg > 0.0f);
            CHECK(L.omega_deg <= 6.0f && L.omega_deg >= -6.0f);
            /* 同圈初角互不重合 */
            if (seen[L.ring]) CHECK(L.phase_deg != prev_phase[L.ring]);
            else seen[L.ring] = 1;
            prev_phase[L.ring] = L.phase_deg;
            CHECK(L.phase_deg >= 0.0f && L.phase_deg < 360.0f);
        }
        /* 每圈台数之和 == N，且每圈至少 3 台 */
        for (r = 0; r < rings; ++r) {
            unsigned total = sum[0] + sum[1] + sum[2] + sum[3] + sum[4];
            CHECK(total == n);
            CHECK(sum[r] >= 4u);                    /* ★ owner：内圈到 5–6 都可以 ⇒ 每圈 ≥4 台 */
        }
        /* ★ owner 口径：**外圈台数 ≥ 内圈**（半径更大的圈放更多）；14–18 的 ω 序列 = −6/+4.5/+3 */
        CHECK(sum[0] <= sum[1] && sum[1] <= sum[2]);
        {   yk_orbit_layout_t q0, q1, q2;
            int ik0 = -1, ik1 = -1, ik2 = -1;
            for (k = 0; k < n; ++k) {
                yk_orbit_layout_t t;
                yk_orbit_layout((int)n, (int)k, &t);
                if (t.ring == 0 && ik0 < 0) ik0 = (int)k;
                if (t.ring == 1 && ik1 < 0) ik1 = (int)k;
                if (t.ring == 2 && ik2 < 0) ik2 = (int)k;
            }
            CHECK(ik0 >= 0 && ik1 >= 0 && ik2 >= 0);
            CHECK(yk_orbit_layout((int)n, ik0, &q0) && yk_orbit_layout((int)n, ik1, &q1)
                  && yk_orbit_layout((int)n, ik2, &q2));
            CHECK(q0.omega_deg == 6.0f && q1.omega_deg == -4.5f && q2.omega_deg == 3.0f);
            CHECK(q0.omega_deg * q1.omega_deg < 0.0f && q1.omega_deg * q2.omega_deg < 0.0f);
            CHECK(q2.radius == 90.0f);
        }
        /* 半径序列严格递增（等间隔）—— 直接按圈号抽查第 0/最后 圈 */
        {   yk_orbit_layout_t f, l;
            int ki = -1, kl = -1;
            for (k = 0; k < n; ++k) {
                yk_orbit_layout_t t;
                yk_orbit_layout((int)n, (int)k, &t);
                if (t.ring == 0) ki = (int)k;
                if ((unsigned)t.ring == rings - 1u) kl = (int)k;
            }
            CHECK(ki >= 0 && kl >= 0);
            CHECK(yk_orbit_layout((int)n, ki, &f) && yk_orbit_layout((int)n, kl, &l));
            CHECK(l.radius > f.radius);
            prev_r = l.radius;
            CHECK(prev_r == 30.0f * (float)rings);
        }
    }
}

int main(void)
{
    yk_map_t m;
    yk_slot_t *sl;
    yk_action_t a;
    yk_transfer_t t;
    yk_slot_t e;
    float p1[2];
    float ep[2], off[3], org[3], out[2];
    int32_t carry, out_dmg;
    unsigned i;
    /* ★ 2026-09-22（th08 #5）：妖侧组的「集合」描述。旧行为（只有 0x16 一格）= SINGLE_ONLY；
     * 新形态 = 0x16 + 逐角色逐档那一片。两种都测，防止把单格语义改坏。*/
    const yk_sets_t sets_single = YK_SETS_SINGLE_ONLY(YK_SHOT_SET_YOUKAI);
    const yk_sets_t sets_tiers  = { YK_SHOT_SET_YOUKAI, 0x48u, YK_YOUKAI_TIER_SETS };
    (void)sets_tiers;      /* 只在 `#ifdef CE_SHT_SET_YOUKAI_R0` 那一段里用（没跑 make sht 时用不到）*/

    p1[0] = 10.0f; p1[1] = 20.0f;

    /* ★ 2026-09-22（工单 E）：`yk_map_reset` **不**清账本计数（`heals` / `rearms` 是「只增不清」的
     *   计数器，见 `yk_map_rearms` 的注）；卡侧那份表是**文件级静态**（天然 0 初始化），
     *   而这里是栈上的局部 ⇒ 必须先自己清零一次，否则计数断言读到的是垃圾值。*/
    __builtin_memset(&m, 0, sizeof m);

    /* ============================== §3.1 止射闸 */
    CHECK(yk_suppress_native_shot_spec31_legacy(1, 1) == 1);        /* ★ legacy（§3.1 字面）：只说明「有自机 + 聚焦」，不含「0 子机放行」那条用户口径 */
    CHECK(yk_suppress_native_shot_spec31_legacy(1, 0) == 0);        /* 高速 = 原机体 ⇒ 不压 */
    CHECK(yk_suppress_native_shot_spec31_legacy(0, 1) == 0);        /* 标题 / 过场（没有自机）⇒ 不压 */
    CHECK(yk_suppress_native_shot_spec31_legacy(0, 0) == 0);

    /* --- ★★ 新门（2026-09-12）：第三条 = 「本帧妖侧子机数 > 0」-------------------------------
     * ❓**口径（用户 2026-09-12 定）：子机数为 0 ⇒ 放行原生弹**（`count == 0` 时不压）。
     *   **不是一手复刻**：TH08 妖侧不看 P、一按低速就有子机 ⇒ 原作没有「没子机」的档；
     *   本作的约束是「妖侧弹的膛口恒取子机（`0x40a9c0`）」⇒ 没有子机就一发都发不出来，
     *   此时再压原生弹 = 那一档「什么都不发」。
     * 判据顺序（短路）= ① 有自机 ② 聚焦（= spec §3.1 原文）③ 子机数 > 0（= 用户口径）。*/
    {
        int c, lv;
        /* ① 早苗（机体 3）档 0 ⇒ 数量 0 ⇒ **不压**（这条就是用户口径本身）*/
        CHECK(yk_option_count(1, 3, 0) == 0);
        CHECK(yk_should_suppress_native(1, 1, 3, 0) == 0);
        /* ①' 早苗档 1..4（**恒 1 台**，第四十八批 owner 口径）⇒ **压**（口径只放行「一发都出不来」的那一档）*/
        for (lv = 1; lv < 5; ++lv) {
            CHECK(yk_option_count(1, 3, lv) == 1);
            CHECK(yk_should_suppress_native(1, 1, 3, lv) == 1);
        }
        /* ③ 霊夢 / 魔理沙 / 咲夜（常数 1 / 1 / 4）⇒ **任何档都压**（含档 0；与早苗档 0 的对照）*/
        for (c = 0; c < 3; ++c) {
            for (lv = 0; lv < YK_OPTION_TIERS; ++lv) {
                CHECK(yk_option_count(1, c, lv) > 0);
                CHECK(yk_should_suppress_native(1, 1, c, lv) == 1);
            }
        }
        /* ④ 高速 ⇒ 不压（既有行为不变）：与机体 / 档无关 */
        for (c = 0; c < YK_OPTION_CHARS; ++c) {
            for (lv = 0; lv < YK_OPTION_TIERS; ++lv) {
                CHECK(yk_should_suppress_native(1, 0, c, lv) == 0);
                CHECK(yk_should_suppress_native(0, 1, c, lv) == 0);   /* 没有自机（标题 / 过场）*/
            }
        }
        /* ★ 门与数量表**同源**：判据逐格等于「那张表算出来的数量 > 0」——
         * 这一条断言是防「有人回头往门里塞第二份判断」（例如按机体号枚举）的护栏。*/
        for (c = -2; c < YK_OPTION_CHARS + 2; ++c) {
            for (lv = -2; lv < YK_OPTION_TIERS + 2; ++lv) {
                CHECK(yk_should_suppress_native(1, 1, c, lv)
                      == (yk_option_count(1, c, lv) > 0));
            }
        }
        /* 越界钳位（引擎读到的值不信任）：档 99 ⇒ 钳到档 4 ⇒ 早苗压；机体 99 ⇒ 钳到早苗 ⇒ 档 0 不压 */
        CHECK(yk_option_count(1, 99, 0) == 0);
        CHECK(yk_should_suppress_native(1, 1, 3, 99) == 1);
    }

    /* ============================== §3.2 本体外观（移动动画状态机，一手 th18:0x45b610–0x45b7a8）*/
    /* 四条判据 + 「都不成立 ⇒ 保持原状态」，逐条对着引擎的 `cmp/jcc` 写（见 youkai_core.h §3.2）。*/
    CHECK(yk_body_state(-1, 0, YK_BODY_IDLE) == YK_BODY_MOVE_L);    /* 起步向左（th18:0x45b62a）*/
    CHECK(yk_body_state(-5, 2, YK_BODY_IDLE) == YK_BODY_MOVE_L);    /* 上一帧为正也算「起步」*/
    CHECK(yk_body_state(0, -1, YK_BODY_MOVE_L) == YK_BODY_STOP_L);  /* 从左侧停下（th18:0x45b766）*/
    CHECK(yk_body_state(1, 0, YK_BODY_IDLE) == YK_BODY_MOVE_R);     /* 起步向右（th18:0x45b7a6）*/
    CHECK(yk_body_state(3, -2, YK_BODY_IDLE) == YK_BODY_MOVE_R);
    CHECK(yk_body_state(0, 1, YK_BODY_MOVE_R) == YK_BODY_STOP_R);   /* 从右侧停下（th18:0x45b66a）*/
    /* 持续同向移动 / 静止 ⇒ 引擎**不动脚本** ⇒ 我们也保持上一次的状态。*/
    CHECK(yk_body_state(-1, -1, YK_BODY_MOVE_L) == YK_BODY_MOVE_L);
    CHECK(yk_body_state(1, 1, YK_BODY_MOVE_R) == YK_BODY_MOVE_R);
    CHECK(yk_body_state(0, 0, YK_BODY_IDLE) == YK_BODY_IDLE);
    CHECK(yk_body_state(0, 0, YK_BODY_STOP_L) == YK_BODY_STOP_L);   /* 「停下」那支脚本会自己循环 */
    /* `last` 越界（引擎读到的值不信任）⇒ 钳成 idle。*/
    CHECK(yk_body_state(0, 0, 99) == YK_BODY_IDLE);
    CHECK(yk_body_state(0, 0, -7) == YK_BODY_IDLE);
    /* 状态号必须是 0..4 连续（C 侧 `YK_BODY_SCRIPT[4][YK_BODY_STATES]` 的下标 = 状态号，
     * 生成器 `_YK_STATES` 的次序也是这一套 ⇒ 两边靠这三个常量对齐）。*/
    CHECK(YK_BODY_STATES == 5);
    CHECK(YK_BODY_IDLE == 0 && YK_BODY_MOVE_L == 1 && YK_BODY_STOP_L == 2);
    CHECK(YK_BODY_MOVE_R == 3 && YK_BODY_STOP_R == 4);
    /* ============================== §3.3 子机（SDK 的低速子机声明层，spec §3.3.1 / §12.4）*/
    /* --- 数量表：用户定案「看不同机体在永夜抄与天空璋（文文）的数量」+ 一手出处见 youkai_core.h。
     *     行 = 机体 0..3（霊夢/魔理沙/咲夜/早苗），列 = 火力档 0..4（CURRENT_POWER/100 钳位）。*/
    for (i = 0; i < 5; ++i) {
        CHECK(yk_option_count(1, 0, (int)i) == 1);            /* 霊夢 → th08 pl00 妖侧（八雲紫）= 1，与 P 无关 */
        CHECK(yk_option_count(1, 1, (int)i) == 1);            /* 魔理沙 → th08 pl01 妖侧（アリス）= 1，与 P 无关 */
        CHECK(yk_option_count(1, 2, (int)i) == 4);            /* 咲夜 → th08 pl02 妖侧（レミリア）= 4，与 P 无关 */
    }
    /* ★★ 2026-09-27（第四十八批 · owner 口径「高低速状态下文文应该只有一台子机」）：
     *   早苗 = **1 台**（档 0 仍是 0 —— 那是 TH16 档 0 = 0 的一手事实），**不再 ∝ P**。
     *   P 的成长由子机弹承担（`pl03.sht` 低速组 A0..A4：dmg 3/8/16/20 + 弹 anm 逐档放大）。*/
    CHECK(yk_option_count(1, 3, 0) == 0);
    CHECK(yk_option_count(1, 3, 1) == 1);
    CHECK(yk_option_count(1, 3, 2) == 1);
    CHECK(yk_option_count(1, 3, 3) == 1);
    CHECK(yk_option_count(1, 3, 4) == 1);
    CHECK(YK_OPTION_BY_CHAR[3][4] == 1);                      /* 表本体可读（youkai_core.h）*/
    CHECK(YK_OPTION_TIERS == YK_SHOT_TIERS);                  /* 列数 = 五档（同一组火力档）*/
    CHECK(YK_OPTION_CHARS == 4);
    /* 高速：一格槽都不占（用户定案 5）——与机体 / 火力无关 */
    for (i = 0; i < 4; ++i) {
        int lv;
        for (lv = 0; lv < 5; ++lv) {
            CHECK(yk_option_count(0, (int)i, lv) == 0);
            CHECK(!yk_option_wanted(0, (int)i, lv));
        }
    }
    /* wanted = count > 0（膛口 = 子机 ⇒ 它顺带就是「妖侧能不能发弹」的判据）*/
    CHECK(yk_option_wanted(1, 2, 0));                         /* 咲夜档 0 也有 4 个 */
    CHECK(!yk_option_wanted(1, 3, 0));                        /* 早苗档 0 = 0 个（th16:0x4444c1 的清格循环）*/
    CHECK(yk_option_wanted(1, 3, 1));
    /* 越界一律钳进表内（引擎读到的值不信任）：机体 < 0 / > 3、档 < 0 / > 4 */
    CHECK(yk_option_count(1, -5, 2) == yk_option_count(1, 0, 2));
    CHECK(yk_option_count(1, 99, 2) == yk_option_count(1, 3, 2));
    CHECK(yk_option_count(1, 2, -3) == yk_option_count(1, 2, 0));
    CHECK(yk_option_count(1, 3, 99) == yk_option_count(1, 3, 4));

    /* --- 逐格阵型（spec §12.4 的形状缺口）：**「咲夜 = 4」必须真的是 4 个不同的位置** ---
     * 表 = 原生 `pl0X.sht` 的位置块（一手出处见 youkai_core.h §3.3），格号 k = T[count-1] + i。*/
    {
        yk_opt_off_t o1, o2;
        int c, k, j;

        for (c = 0; c < YK_OPTION_CHARS; ++c) {
            /* ★ 2026-09-22（th08 #1）：机体 0/1 的妖侧子机在原作里**只有 1 个**（`YK_OPTION_BY_CHAR`
             *   的第 0/1 行全是 1）⇒ 它们那张表的 4 格是同一个值（同值的"备用格"）。
             *   ★★ 2026-09-27（第四十七批 · owner 口径）：**机体 3（早苗 = th16 文文）也改成同值** ——
             *   「VM 应当堆在正中间，我看到的是一左一右两个子机，只有左边的发弹」⇒ 那一行十格
             *   全部 = (0, −18)。所以「同档内两格不重合」现在只对**咲夜（机体 2）**断言：
             *   她的 TH08 一手阵型确实是左右分开的 4 格。*/
            const int multi = (c == 2);
            for (k = 1; k <= YK_OPTION_CELLS; ++k) {
                for (j = 0; j < k; ++j) {
                    CHECK(yk_option_offsets(c, k, j, &o1));           /* 每格都取得到 */
                    if (!multi) continue;
                    int m;
                    for (m = j + 1; m < k; ++m) {
                        CHECK(yk_option_offsets(c, k, m, &o2));
                        CHECK(!(o1.fx == o2.fx && o1.fy == o2.fy));
                        CHECK(!(o1.nfx == o2.nfx && o1.nfy == o2.nfy));
                    }
                }
                CHECK(!yk_option_offsets(c, k, k, &o1));              /* 第 count 格起 = 无此格 */
            }
        }
        /* ★★ 2026-09-22（th08 #1）：机体 0/1/2 改用 **TH08 妖侧阵型**（一手见 youkai_core.h）。
         *   灵梦 = 1 个、(0, −96)；魔理沙 = 1 个、(0, −32)；咲夜 = 4 个 (±30,−16) / (±10,−32)。*/
        CHECK(yk_option_offsets(0, 1, 0, &o1) && o1.fx == 0.0f && o1.fy == -96.0f);
        CHECK(yk_option_offsets(1, 1, 0, &o1) && o1.fx == 0.0f && o1.fy == -32.0f);
        CHECK(yk_option_offsets(2, 4, 0, &o1) && o1.fx == -30.0f && o1.fy == -16.0f);
        CHECK(yk_option_offsets(2, 4, 1, &o1) && o1.fx == -10.0f && o1.fy == -32.0f);
        CHECK(yk_option_offsets(2, 4, 2, &o1) && o1.fx ==  10.0f && o1.fy == -32.0f);
        CHECK(yk_option_offsets(2, 4, 3, &o1) && o1.fx ==  30.0f && o1.fy == -16.0f);
        /* ★★ 2026-09-27（第四十七批 · owner 口径）：早苗（= th16 文文）**十格同一点 = 正中间（身前）**。
         *   owner 原话：「你的阵型不对，VM 应当堆在正中间，我看到的是一左一右两个子机，只有左边的发弹」。
         *   为什么：文文那一路**只有一个发弹口**（`pl03.sht` 低速组恒 1 条 `slot>=1` 的行），
         *   VM 台数按 P 变、但位置**都叠在一处** —— 不再用 TH18 原生 `pl03.sht` 的左右阵型。
         *   ⚠ 数值 (0, −18) = ❓ 自定（owner 只给了「正中间/身前」）⇒ 一手块一到就换。*/
        for (j = 0; j < YK_OPTION_CELLS; ++j) {
            CHECK(yk_option_offsets(3, 4, j, &o1));
            CHECK(o1.fx == 0.0f && o1.fy == -18.0f);              /* 四格完全重合 */
            CHECK(o1.nfx == 0.0f && o1.nfy == -18.0f);            /* 非聚焦块同值 */
        }
        CHECK(yk_option_offsets(3, 4, 0, &o1) && yk_option_offsets(3, 4, 3, &o2) &&
              o1.fx == o2.fx && o1.fy == o2.fy);                  /* 第 0 格与第 3 格同点 */
        /* ★ 咲夜（机体 2）：TH08 一手 = 4 格全在自机上方（y < 0）、左右分开、且**互不相同**
         *（具体数值在上面那组 CHECK 里逐格钉住 —— 那是 `th08:0x44eb70` 的跳表 `th08:0x44ee4b`）。*/
        for (j = 0; j < 4; ++j) {
            CHECK(yk_option_offsets(2, 4, j, &o1));
            CHECK(o1.fy < 0.0f);                                   /* 4 格都在上方 */
            CHECK(o1.fx != 0.0f);                                  /* 且左右分开（不是一条竖线） */
            for (k = j + 1; k < 4; ++k) {
                CHECK(yk_option_offsets(2, 4, k, &o2));
                CHECK(o1.fx != o2.fx || o1.fy != o2.fy);
            }
        }
        /* 越界参数：机体 / count / cell / 空指针 */
        CHECK(!yk_option_offsets(4, 1, 0, &o1));
        CHECK(!yk_option_offsets(-1, 1, 0, &o1));
        CHECK(!yk_option_offsets(0, 0, 0, &o1));
        CHECK(!yk_option_offsets(0, YK_OPTION_CELLS + 1, 0, &o1));
        CHECK(!yk_option_offsets(0, 1, -1, &o1));
        CHECK(!yk_option_offsets(0, 1, 0, NULL));
    }

    CHECK(yk_option_script_changed(50, 50) == 0);             /* 一致 ⇒ 不用重申请 */
    CHECK(yk_option_script_changed(50, 51) == 1);             /* 声明的脚本和手里那个不一样 ⇒ 重申请 */
    CHECK(yk_option_script_changed(50, -1) == 0);             /* 手里还没有子机 ⇒ 不算「变了」 */
    CHECK(yk_option_script_changed(50, 0) == 1);

    /* ===== ★★ 2026-09-22（th08 #1）：妖侧子机的**家位置**与弹簧（TH08 一手，见 youkai_core.h）=====
     * owner 原话：「灵梦机体妖子机本身应当追踪上去」。一手 = TH08 的妖侧子机位置是逐机体写死的：
     *   机体 0 霊夢・紫 = (0,−96) + **弹簧跟随**（`th08:0x44e770`）；机体 1 アリス = (0,−32) 吸附；
     *   机体 2 レミリア = 4 格 (±30,−16) / (±10,−32)（`th08:0x44eb70` 的跳表 `th08:0x44ee4b`）。
     * ⚠ 同时钉住那条**一手负结论**：追敌人的分支（`th08:0x44e8d0`）在 v1.00d 够不到
     *   （`player+0xe2abc` 全 exe 只被写 0）⇒ 本卡**不做**追敌人，别把它当漏做。 */
    {
        float home[2], goal[2], pos[2], vel[2];
        const float player[2] = { 200.0f, 300.0f };
        /* 机体 0：1 格、正上方 96 px（`th08:0x4b42c4`）*/
        CHECK(yk_option_home_xy(0, 0, home) && home[0] == 0.0f && home[1] == -96.0f);
        CHECK(yk_option_home_xy(0, 1, home) && home[1] == -96.0f);       /* 只有 1 格 ⇒ 各格同值 */
        /* 机体 1：正上方 32 px（`th08:0x44ea40` 的 `y -= 32.0`）*/
        CHECK(yk_option_home_xy(1, 0, home) && home[0] == 0.0f && home[1] == -32.0f);
        /* 机体 2：4 格，逐格不同（顺序 = 子机序号 0..3）*/
        CHECK(yk_option_home_xy(2, 0, home) && home[0] == -30.0f && home[1] == -16.0f);
        CHECK(yk_option_home_xy(2, 1, home) && home[0] == -10.0f && home[1] == -32.0f);
        CHECK(yk_option_home_xy(2, 2, home) && home[0] ==  10.0f && home[1] == -32.0f);
        CHECK(yk_option_home_xy(2, 3, home) && home[0] ==  30.0f && home[1] == -16.0f);
        /* 机体 3（早苗 = TH16 文）本批**没有**一手数据 ⇒ 返回 0（调用者回落到 TH18 原生阵型）*/
        CHECK(!yk_option_home_xy(3, 0, home));
        CHECK(!yk_option_home_xy(-1, 0, home));
        CHECK(!yk_option_home_xy(0, YK_OPTION_CELLS, home));
        CHECK(!yk_option_home_xy(0, 0, 0));
        /* 只有机体 0 走弹簧 */
        CHECK(yk_option_uses_spring(0) == 1);
        CHECK(yk_option_uses_spring(1) == 0);
        CHECK(yk_option_uses_spring(2) == 0);
        CHECK(yk_option_uses_spring(3) == 0);
        /* 家位置 = 自机 + 偏移，且 `y` 夹到 `>= 32`（`th08:0x44e770` 那一步）*/
        CHECK(yk_option_home_from_player(0, 0, player, goal));
        CHECK(goal[0] == 200.0f && goal[1] == 204.0f);                    /* 300 − 96 */
        {   /* 自机贴到屏上沿（y = 100）⇒ 300−96 那一套不成立，夹到 32 */
            const float hi[2] = { 100.0f, 20.0f };
            CHECK(yk_option_home_from_player(0, 0, hi, goal));
            CHECK(goal[1] == YK_OPT_HOME_Y_MIN);                          /* 20 − 96 < 32 ⇒ 夹成 32 */
        }
        CHECK(!yk_option_home_from_player(3, 0, player, goal));           /* 没数据 ⇒ 不动手 */
        CHECK(!yk_option_home_from_player(0, 0, 0, goal));
        /* 弹簧一步（`th08:0x44e770` 逐步式）：pos/vel 都被就地更新，且**方向朝目标** */
        pos[0] = player[0]; pos[1] = player[1]; vel[0] = 0.0f; vel[1] = 0.0f;
        goal[0] = player[0]; goal[1] = player[1] - 96.0f;                 /* 不夹（300−96 > 32）*/
        yk_opt_spring_step(pos, vel, goal);
        CHECK(vel[1] < 0.0f);                                             /* 向上加速 */
        CHECK(pos[1] < player[1] && pos[1] > goal[1]);                    /* 动了一步但没到 */
        CHECK(pos[0] == player[0] && vel[0] == 0.0f);                     /* x 方向没有差 ⇒ 不动 */
        /* 收敛性：连续积分 600 帧后必须停在家位置上（±0.5 px）*/
        {
            int f;
            for (f = 0; f < 600; ++f) yk_opt_spring_step(pos, vel, goal);
            CHECK(pos[0] > goal[0] - 0.5f && pos[0] < goal[0] + 0.5f);
            CHECK(pos[1] > goal[1] - 0.5f && pos[1] < goal[1] + 0.5f);
        }
        /* 空指针安全 */
        yk_opt_spring_step(0, vel, goal);
        yk_opt_spring_step(pos, 0, goal);
        yk_opt_spring_step(pos, vel, 0);

        /* ★★★ 2026-09-27（owner ①② 咲夜）：**放置型**（`th08:0x44eb70` 的 state 2 不碰位置
         * + 共尾的 `face = norm(face + step)` 自转）。逐条断言照抄那一手的常量。*/
        {
            float face = -1.0f, spin = 0.0f, ang = 0.0f;
            CHECK(yk_option_uses_freeze(2) == 1);              /* 只有咲夜那一支 */
            CHECK(yk_option_uses_freeze(0) == 0);
            CHECK(yk_option_uses_freeze(1) == 0);
            CHECK(yk_option_uses_freeze(3) == 0);
            /* 出生朝向 `+0x2d8`：idx 0/2 = 0、idx 1/3 = π（`0x40490fdb`）*/
            CHECK(yk_option_freeze_face0(0, &face) && face == 0.0f);
            CHECK(yk_option_freeze_face0(1, &face) && face == 3.14159274101257324f);
            CHECK(yk_option_freeze_face0(2, &face) && face == 0.0f);
            CHECK(yk_option_freeze_face0(3, &face) && face == 3.14159274101257324f);
            CHECK(!yk_option_freeze_face0(0, 0));
            /* 每帧增量（跳表 `th08:0x44ee5b`）：+1.5° / −2° / +2° / −1.5°（内外圈反向）*/
            CHECK(yk_option_freeze_spin(0, &spin) && spin > 0.0261799f && spin < 0.0261800f);
            CHECK(yk_option_freeze_spin(1, &spin) && spin > -0.0349066f && spin < -0.0349065f);
            CHECK(yk_option_freeze_spin(2, &spin) && spin > 0.0349065f && spin < 0.0349066f);
            CHECK(yk_option_freeze_spin(3, &spin) && spin > -0.0261800f && spin < -0.0261799f);
            CHECK(!yk_option_freeze_spin(4, &spin) && spin == 0.0f);   /* 越界 ⇒ 不转 */
            CHECK(!yk_option_freeze_spin(0, 0));
            /* `th08:0x43edb0` = `f = angle + step` 再绕回 `[−π, π)` */
            yk_opt_angle_norm(0.0f, 0.5f, &ang);
            CHECK(ang == 0.5f);                                          /* 没越界 ⇒ 原样 */
            yk_opt_angle_norm(3.0f, 0.5f, &ang);
            CHECK(ang < 3.14159274101257324f && ang > -3.14159274101257324f);
            CHECK(ang > 3.0f - 6.2831855f + 0.49f);                      /* = 3.5 − 2π ≈ −2.783 */
            yk_opt_angle_norm(0.0f, -4.0f, &ang);                        /* 负侧只加一次 2π */
            CHECK(ang > 2.28f && ang < 2.29f);                           /* = −4 + 2π ≈ 2.2832 */
            yk_opt_angle_norm(0.0f, 100.0f, &ang);                       /* 迭代上限 16（一手 `cmp eax,0x10`）*/
            CHECK(ang < 0.0f && ang > -1.0f);
            yk_opt_angle_norm(0.0f, 0.5f, 0);                            /* 空指针安全 */
        }

        /* ★★ 2026-09-22 晚（owner 第 2 条）：**追敌**那一段（一手 `th08:0x44e8d0`）。
         * 家位置 = 目标 `final_pos` + (0, +32)，`y` 夹到 `>= YK_OPT_HOME_Y_MIN`。*/
        {
            const float tgt[2] = { 240.0f, 120.0f };
            CHECK(yk_option_chase_goal(tgt, goal));
            CHECK(goal[0] == 240.0f && goal[1] == 120.0f + YK_OPT_CHASE_GOAL_DY);
            {   /* 目标贴到屏上沿 ⇒ 夹到 32（`th08:0x44e918`–`0x44e92f` 的 fcomp/写回）*/
                const float hi2[2] = { 240.0f, -40.0f };
                CHECK(yk_option_chase_goal(hi2, goal));
                CHECK(goal[1] == YK_OPT_HOME_Y_MIN);
            }
            CHECK(!yk_option_chase_goal(0, goal));
            CHECK(!yk_option_chase_goal(tgt, 0));
            /* 进门条件（`th08:0x44e770` 尾巴）：目标存在 **且** 出场满 10 帧 */
            CHECK(yk_option_chase_want(1, YK_OPT_CHASE_MIN_AGE));
            CHECK(yk_option_chase_want(1, YK_OPT_CHASE_MIN_AGE + 1));
            CHECK(!yk_option_chase_want(1, YK_OPT_CHASE_MIN_AGE - 1));
            CHECK(!yk_option_chase_want(1, 0));
            CHECK(!yk_option_chase_want(1, -5));                          /* 越界当 0 */
            CHECK(!yk_option_chase_want(0, 1000));                        /* 没目标 ⇒ 永不进 */
            /* 旋转/待机标签号：脚本生成器（`make_youkai_option_art.py` 的 LABEL_MAP）里
             * TH08 原号 3→5（旋转）、1→2（待机循环 A）—— 两处**必须**一致，改一处就编译期红。*/
            CHECK(YK_OPT_SPIN_LABEL == 5);
            CHECK(YK_OPT_IDLE_LABEL == 2);
            /* 追踪用的弹簧与跟随自机**同一条**：从自机位出发、600 帧后收敛到目标上方 32 */
            pos[0] = 200.0f; pos[1] = 300.0f; vel[0] = 0.0f; vel[1] = 0.0f;
            {   int f; for (f = 0; f < 600; ++f) yk_opt_spring_step(pos, vel, goal); }
            CHECK(pos[0] > goal[0] - 0.5f && pos[0] < goal[0] + 0.5f);
            CHECK(pos[1] > goal[1] - 0.5f && pos[1] < goal[1] + 0.5f);
        }
    }

    /* ============================== §3.4 弹幕（档位 / 妖侧弹 / shooterset 自检）*/
    CHECK(yk_shot_tier(0) == 0);
    CHECK(yk_shot_tier(99) == 0);
    CHECK(yk_shot_tier(100) == 1);
    CHECK(yk_shot_tier(250) == 2);
    CHECK(yk_shot_tier(399) == 3);
    CHECK(yk_shot_tier(400) == 4);
    CHECK(yk_shot_tier(99999) == YK_SHOT_TIERS - 1);          /* 溢出钳最高档 */
    CHECK(yk_shot_tier_damage(-1) == yk_shot_tier_damage(0)); /* 越界钳位 */
    CHECK(yk_shot_tier_damage(99) == yk_shot_tier_damage(YK_SHOT_TIERS - 1));
    CHECK(yk_shot_tier_damage(0) == YK_SHOT_DMG_NOT_SET);     /* ★ 故意不填（见文件头注释 / youkai_core.c §3.4）*/
    CHECK(yk_shot_tier_damage(4) == YK_SHOT_DMG_NOT_SET);

    /* 打包 id = (set << 8) | 组内行号（`0x45ec69` 打包 / `0x45ee10` 回读）*/
    CHECK(yk_bullet_is_youkai(((uint32_t)YK_SHOT_SET_YOUKAI << 8) | 3, &sets_single));
    CHECK(yk_bullet_is_youkai(((uint32_t)YK_SHOT_SET_YOUKAI << 8) | 0, &sets_single));
    CHECK(!yk_bullet_is_youkai(((uint32_t)(YK_SHOT_SET_YOUKAI + 1) << 8) | 3, &sets_single));
    CHECK(!yk_bullet_is_youkai((uint32_t)YK_SHOT_SET_YOUKAI, &sets_single));             /* 只带 row 不带 set */
    CHECK(!yk_bullet_is_youkai(0x0503u, &sets_single));                                  /* 人侧 0x05 组 */
    /* 占位（表 = 全 0 = 不覆盖）⇒ 妖侧弹也原样保留 shooter 记录的 damage */
    out_dmg = -1;
    CHECK(yk_bullet_damage_apply(50, ((uint32_t)YK_SHOT_SET_YOUKAI << 8) | 1, 400, &sets_single, &out_dmg) == 0);
    CHECK(out_dmg == -1);
    CHECK(yk_bullet_damage_apply(50, 0x0501u, 400, &sets_single, &out_dmg) == 0);         /* 不是妖侧弹 ⇒ 不碰 */
    CHECK(yk_bullet_damage_apply(50, ((uint32_t)YK_SHOT_SET_YOUKAI << 8) | 1, 400, &sets_single, 0) == 0); /* out=NULL 安全 */

    /* shooterset 自检：那一格必须是**我们写的合法记录**才敢调 `ce_tick_shooters_for_card`
     *（原生原值是 `fire_rate = 0` ⇒ 装备卡路径整数除零，`0x40aa4f` 没有零分支）。
     * 原生那格长什么样、我们写进去的 9 行长什么样、逐字段出处 —— 全在
     * `assets/sht/append_shooterset.py` 的 `REPLACE` 注释里（一手：TH08 v1.00d `ply03as.sht` 满 P 档）。*/
    CHECK(!yk_shotset_usable(0));        /* ★ 原生那一格的值 = 自检必须拒绝（这条就是除零护栏本身）*/
    CHECK(!yk_shotset_usable(-1));       /* 负 = 终止符 */
    CHECK(!yk_shotset_usable(128));
    CHECK(yk_shotset_usable(1));
    CHECK(yk_shotset_usable(127));
    /* 我们那 9 行的 fire_rate 逐行 = TH08 `rec[+0x00]`（10/5/3/5/10/6/5/5/6）⇒ 每一行都必须判「可用」。
     * ⚠ 这一组数改了要**同时**改 `append_shooterset.py` 的 `REPLACE`（生成器会核对 header 常量）。*/
    CHECK(yk_shotset_usable(3));
    CHECK(yk_shotset_usable(5));
    CHECK(yk_shotset_usable(6));
    CHECK(yk_shotset_usable(10));

    /* 槽号与行数：生成器（`make sht`）会回来核对这两个常量，对不上就失败（防静默漂移）*/
    CHECK(YK_SHOT_SET_YOUKAI == 0x16);          /* 原生唯一无卡映射的那一格（见 youkai_core.h 的出处）*/
    CHECK(YK_SHOT_SET_YOUKAI < 0x17);           /* ★ 必须落在**原生区**：0x17–0x27 是本 mod 追加区，
                                                 *   动了会让别的卡（77/116/117）的索引漂移 */
    CHECK(YK_SHOT_SET_YOUKAI_ROWS == 9);        /* 9 行 = `REPLACE` 里的行数 */
    CHECK(YK_SHOT_SET_YOUKAI_ROWS <= 256);      /* 行号进 `packed_id` 低 8 位 */
    /* 打包 id 往返：9 行都在我们的 set 字节下；别的 set 一律不认（人侧 0x05 / 追加区 0x17）*/
    for (i = 0; i < (unsigned)YK_SHOT_SET_YOUKAI_ROWS; ++i)
        CHECK(yk_bullet_is_youkai(((uint32_t)YK_SHOT_SET_YOUKAI << 8) | i, &sets_single));
    CHECK(!yk_bullet_is_youkai(((uint32_t)(YK_SHOT_SET_YOUKAI - 1) << 8) | 0, &sets_single));
    CHECK(!yk_bullet_is_youkai(((uint32_t)(YK_SHOT_SET_YOUKAI + 1) << 8) | 0, &sets_single));
    CHECK(!yk_bullet_is_youkai(0x1700u, &sets_single));       /* 追加区第一格（77 的豌豆）不是妖侧弹 */
    /* ★ 判据**只看 set 字节**（不夹行号）：行号越界的打包 id 也算「我们的」——
     * 因为引擎不会造出这种弹（`0x40a9c0` 的行循环在终止符处就停，`0x40aa8f`）。
     * 钉住这条语义，免得以后误以为这里有行号闸。*/
    CHECK(yk_bullet_is_youkai(((uint32_t)YK_SHOT_SET_YOUKAI << 8) | YK_SHOT_SET_YOUKAI_ROWS, &sets_single));
    CHECK(yk_bullet_is_youkai(((uint32_t)YK_SHOT_SET_YOUKAI << 8) | 0xffu, &sets_single));
#ifdef CE_SHT_SET_YOUKAI
    /* `native/sht_ids.h` 是生成物：跑过 `make sht` 之后它就会有这一条 ⇒ 两边必须相等。
     * （没跑过 `make sht` 时这个宏不存在，上面那条硬编码 0x16 仍然钉着槽号。）*/
    CHECK(CE_SHT_SET_YOUKAI == YK_SHOT_SET_YOUKAI);
#endif
#ifdef CE_SHT_SET_YOUKAI_R0
    /* ===== ★★ 2026-09-22（th08 #5）：妖侧不再只有一格 =====
     * 逐角色 × 逐火力档那 15 格**必须**是连续的、且顺序 = R0..R4, M0..M4, S0..S4
     *（`youkai.c` 的 `_Static_assert` 钉同一件事；这里再钉一次是为了让**纯逻辑单测**也能跑）。 */
    /* ★ 2026-09-23：**不再钉绝对值** —— 111 的组数会随数值口径变化（DPS 对齐 TH18 原生后
     *   31 → 43 格），硬编码会把这条纯逻辑单测被**无关改动**打红。只钉意图：
     *   ① 排在 111 那一段之后；② **20 格**（★ 2026-09-26 由 15 扩到 20：早苗那 5 格是 TH16
     *   `pl03sub`（秋/文），见 `YOUKAI_TIERS_A`）都在「宣布可用」的范围内。*/
    CHECK(CE_SHT_SET_YOUKAI_R0 > CE_SHT_SET_MARISA_MAIN_00);
    CHECK(CE_SHT_SET_YOUKAI_R0 + YK_SHOT_TIERS * 4 <= CE_SHT_SLOTS);
    CHECK(CE_SHT_SET_YOUKAI_M0 == CE_SHT_SET_YOUKAI_R0 + YK_SHOT_TIERS);
    CHECK(CE_SHT_SET_YOUKAI_S0 == CE_SHT_SET_YOUKAI_M0 + YK_SHOT_TIERS);
    CHECK(CE_SHT_SET_YOUKAI_A0 == CE_SHT_SET_YOUKAI_S0 + YK_SHOT_TIERS);
    CHECK(YK_YOUKAI_TIER_SETS == 20);
    {
        const yk_sets_t real = { YK_SHOT_SET_YOUKAI, CE_SHT_SET_YOUKAI_R0, YK_YOUKAI_TIER_SETS };
        unsigned k;
        /* ① 保留格 + 15 格逐档格 ⇒ 全部认（判据**只看 set 字节**，与行号无关）*/
        CHECK(yk_set_is_youkai(YK_SHOT_SET_YOUKAI, &real));
        for (k = 0; k < YK_YOUKAI_TIER_SETS; ++k)
            CHECK(yk_set_is_youkai(CE_SHT_SET_YOUKAI_R0 + k, &real));
        /* ② **紧邻的两格都不认** —— 这是"必须精确、不能写宽区间"的关键断言：
         *    0x47 = 111 的主炮组最后一格、0x57 = 下一张卡的组。写成宽区间会把它们的弹套上
         *    TH08 的伤害表（静默改别人的卡）。*/
        CHECK(!yk_set_is_youkai(CE_SHT_SET_YOUKAI_R0 - 1u, &real));
        CHECK(!yk_set_is_youkai(CE_SHT_SET_YOUKAI_R0 + YK_YOUKAI_TIER_SETS, &real));
        CHECK(!yk_set_is_youkai(0x17u, &real));                     /* 77 豌豆 */
        CHECK(!yk_set_is_youkai(0x28u, &real));                     /* 113 灵界 */
        CHECK(!yk_set_is_youkai(0x05u, &real));                     /* 人侧主炮 */
        /* ③ 单格形态（旧行为）：逐档格**不**认、保留格认 */
        CHECK(yk_set_is_youkai(YK_SHOT_SET_YOUKAI, &sets_single));
        CHECK(!yk_set_is_youkai(CE_SHT_SET_YOUKAI_R0, &sets_single));
        /* ④ NULL = 什么都不认（安全侧：宁可不改也不误改别人的弹）*/
        CHECK(!yk_set_is_youkai(YK_SHOT_SET_YOUKAI, 0));
        CHECK(!yk_set_is_youkai(CE_SHT_SET_YOUKAI_R0, 0));
        /* ⑤ 打包 id 往返（各角色各档各取一行）*/
        for (k = 0; k < YK_YOUKAI_TIER_SETS; ++k)
            CHECK(yk_bullet_is_youkai(((CE_SHT_SET_YOUKAI_R0 + k) << 8) | 4u, &real));
        CHECK(!yk_bullet_is_youkai((0x4700u), &real));
        /* ⑥ 伤害层：档位表还是占位 ⇒ 对逐档格也**不覆盖**（与单格同口径）*/
        out_dmg = -1;
        CHECK(yk_bullet_damage_apply(50, ((CE_SHT_SET_YOUKAI_R0 + 3) << 8) | 1, 400, &real, &out_dmg) == 0);
        CHECK(out_dmg == -1);
    }
#endif

    /* ============================== §3.5 发射点世界坐标（etOn 的算法）*/
    ep[0] = 5.0f; ep[1] = 6.0f;
    off[0] = 10.0f; off[1] = -4.0f; off[2] = 0.0f;
    org[0] = 100.0f; org[1] = 200.0f; org[2] = 0.0f;
    yk_emit_world_pos(ep, off, org, out);            /* 相对模式：敌人位置 + 偏移 */
    CHECK(out[0] == 15.0f && out[1] == 2.0f);
    org[2] = 1.0f;                                   /* 绝对模式（etOffsetAbs 把 z 置 1.0）*/
    yk_emit_world_pos(ep, off, org, out);
    CHECK(out[0] == 110.0f && out[1] == 196.0f);
    org[2] = YK_ORIGIN_ABS_THRESHOLD;                /* 阈值本身不算绝对（判据是 > 0.9）*/
    yk_emit_world_pos(ep, off, org, out);
    CHECK(out[0] == 15.0f && out[1] == 2.0f);
    yk_emit_world_pos(0, off, org, out);             /* 敌人坐标指针为空也安全 */
    CHECK(out[0] == 10.0f && out[1] == -4.0f);
    CHECK(yk_emit_active(0) == 0);
    CHECK(yk_emit_active(1) == 1);
    CHECK(yk_emit_active(0x1234) == 1);              /* `props[i].type != 0` = 活跃 */

    /* ★★★ 2026-09-26（owner #37「发射口与敌人重合不应该建立使魔」）：体上炮口的判据。
     *   相对模式 + 偏移恰好 (0,0) ⇒ 1（跳过这一格、不建使魔）；绝对模式（org.z > 阈值）恒 0
     *   （位置是脚本 `etOffsetAbs` 给的，与本体无关）；任何非 0 偏移 ⇒ 0（那是**有意摆远**的发射口，
     *   原生语料里最小是 16 px）。*/
    off[0] = 0.0f; off[1] = 0.0f; off[2] = 0.0f;
    org[0] = 100.0f; org[1] = 200.0f; org[2] = 0.0f;
    CHECK(yk_emit_on_body(off, org) == 1);           /* 相对 + (0,0) = 本体 */
    org[2] = 1.0f;                                   /* etOffsetAbs ⇒ 脚本给的位置，不算本体 */
    CHECK(yk_emit_on_body(off, org) == 0);
    org[2] = YK_ORIGIN_ABS_THRESHOLD;                /* 阈值本身仍算相对（判据是 > 0.9）*/
    CHECK(yk_emit_on_body(off, org) == 1);
    off[1] = -16.0f;                                 /* 原生最小显式偏移 = 16 px ⇒ 保留使魔 */
    CHECK(yk_emit_on_body(off, org) == 0);
    off[1] = 0.0f; off[0] = 32.0f;
    CHECK(yk_emit_on_body(off, org) == 0);
    off[0] = 0.0f;
    CHECK(yk_emit_on_body(off, 0) == 1);             /* 没有 origins 段（全 0）也按相对处理 */
    CHECK(yk_emit_on_body(0, org) == 0);             /* 空指针安全 */

    /* 槽 → 皮肤变体（四色一套一槽，用户定案；脚本宏在 cards/youkai.c，这里只钉映射）：
     * 槽 0..3 ⇒ 变体 0..3（= S53..S56，红/绿/蓝白/黄）；槽 >= 4 走 `& 3` 兜底（规格说走不到）。*/
    CHECK(yk_fam_variant(0) == 0);
    CHECK(yk_fam_variant(1) == 1);
    CHECK(yk_fam_variant(2) == 2);
    CHECK(yk_fam_variant(3) == 3);
    CHECK(yk_fam_variant(4) == 0);                   /* 兜底：四色循环（❓ 自定，非第 5 色）*/
    CHECK(yk_fam_variant(5) == 1);
    CHECK(yk_fam_variant(YK_EMIT_SLOTS - 1) < 4);    /* 变体恒在四色域内 ⇒ 卡侧查表不会越界 */
    CHECK(yk_fam_variant(-1) == 0);                  /* 越界入参也安全 */
    for (i = 0; i < (unsigned)YK_EMIT_SLOTS; ++i) CHECK(yk_fam_variant((int)i) >= 0 && yk_fam_variant((int)i) < 4);

    /* ============================== §3.5 1:1 映射 + 状态差分 */
    yk_map_reset(&m);
    CHECK(yk_map_fam_count(&m) == 0);

    /* 一个敌人一个活跃槽 ⇒ 恰好 1 只使魔（实测以 1 为主）*/
    yk_frame_begin(&m);
    CHECK(yk_note_slot(&m, 100, 0, 1, p1, &a) == 1);
    CHECK(a.kind == YK_ACT_CREATE && a.enemy_id == 100 && a.slot == 0 && a.fam_id == 0);
    CHECK(a.x == 10.0f && a.y == 20.0f);             /* 动作里带发射点坐标 */
    yk_fam_created(&m, 100, 0, 7001);
    CHECK(yk_map_fam_count(&m) == 1);
    CHECK(yk_note_slot(&m, 100, 0, 1, p1, &a) == 0); /* 已有使魔 ⇒ 不重复建（1:1）*/
    CHECK(yk_note_slot(&m, 100, 5, 0, p1, &a) == 0); /* 不活跃的槽连条目都不建 */
    CHECK(yk_map_find(&m, 100, 5) == 0);
    CHECK(yk_frame_end(&m, s_acts, (int)YK_FAM_MAX) == 0);   /* 本帧扫到了 ⇒ 谁也不撤 */

    /* boss 4 个槽 ⇒ 4 只使魔（实测上限，review §6.3）；敌人 100 这一帧还在 ⇒ 它的使魔留着 */
    yk_frame_begin(&m);
    CHECK(yk_note_slot(&m, 100, 0, 1, p1, &a) == 0);  /* 已有使魔 ⇒ 保持 */
    for (i = 0; i < 4; ++i) {
        CHECK(yk_note_slot(&m, 200, (int)i, 1, p1, &a) == 1 && a.kind == YK_ACT_CREATE);
        yk_fam_created(&m, 200, (int)i, 8000u + i);
    }
    CHECK(yk_map_fam_count(&m) == 5);                /* 4 + 前面那只 */
    CHECK(yk_frame_end(&m, s_acts, (int)YK_FAM_MAX) == 0);

    /* 状态差分①：发射口变不活跃且有使魔 ⇒ 撤（动作带回 fam_id）*/
    yk_frame_begin(&m);
    CHECK(yk_note_slot(&m, 100, 0, 1, p1, &a) == 0);
    CHECK(yk_note_slot(&m, 200, 0, 1, p1, &a) == 0);
    CHECK(yk_note_slot(&m, 200, 1, 1, p1, &a) == 0);
    CHECK(yk_note_slot(&m, 200, 2, 0, p1, &a) == 1);
    CHECK(a.kind == YK_ACT_WITHDRAW && a.fam_id == 8002u && a.enemy_id == 200 && a.slot == 2);
    CHECK(yk_map_fam_count(&m) == 4);
    CHECK(yk_note_slot(&m, 200, 3, 1, p1, &a) == 0);
    CHECK(yk_note_slot(&m, 200, 2, 0, p1, &a) == 0); /* 已撤 ⇒ 不再重复撤 */
    CHECK(yk_frame_end(&m, s_acts, (int)YK_FAM_MAX) == 0);

    /* 状态差分②：那个槽又变活跃（ECL 重新武装）⇒ 建一只新的 */
    yk_frame_begin(&m);
    CHECK(yk_note_slot(&m, 100, 0, 1, p1, &a) == 0);
    CHECK(yk_note_slot(&m, 200, 0, 1, p1, &a) == 0);
    CHECK(yk_note_slot(&m, 200, 1, 1, p1, &a) == 0);
    CHECK(yk_note_slot(&m, 200, 3, 1, p1, &a) == 0);
    CHECK(yk_note_slot(&m, 200, 2, 1, p1, &a) == 1 && a.kind == YK_ACT_CREATE);
    yk_fam_created(&m, 200, 2, 8009);
    CHECK(yk_map_fam_count(&m) == 5);
    CHECK(yk_frame_end(&m, s_acts, (int)YK_FAM_MAX) == 0);

    /* 状态差分③：敌人 100 没了（本帧一个槽都没扫到）⇒ 撤 + 退休条目 */
    yk_frame_begin(&m);
    for (i = 0; i < 4; ++i) CHECK(yk_note_slot(&m, 200, (int)i, 1, p1, &a) == 0);
    CHECK(yk_frame_end(&m, s_acts, (int)YK_FAM_MAX) == 1);
    CHECK(s_acts[0].kind == YK_ACT_WITHDRAW && s_acts[0].fam_id == 7001u && s_acts[0].enemy_id == 100);
    CHECK(yk_map_find(&m, 100, 0) == 0);             /* 条目退休（`enemy_id` 单调不复用）*/
    CHECK(yk_map_fam_count(&m) == 4);                /* 200 的 8000 / 8001 / 8009 / 8003 */

    /* §3.6 击破信号：使魔被打死 ⇒ 那个槽「被打破」*/
    CHECK(yk_fam_broken(&m, 8001u) == 1);
    CHECK(yk_fam_broken(&m, 999999u) == 0);          /* 不是我们的使魔 */
    CHECK(yk_map_fam_count(&m) == 3);
    yk_frame_begin(&m);
    CHECK(yk_note_slot(&m, 200, 0, 1, p1, &a) == 0);
    CHECK(yk_note_slot(&m, 200, 1, 1, p1, &a) == 0); /* 活跃、但「被打破」⇒ 不重建 */
    CHECK(yk_note_slot(&m, 200, 2, 1, p1, &a) == 0);
    CHECK(yk_note_slot(&m, 200, 3, 1, p1, &a) == 0);
    CHECK(yk_note_slot(&m, 200, 1, 0, p1, &a) == 0); /* 不活跃、没有使魔 ⇒ 无动作 */
    CHECK(yk_note_slot(&m, 200, 1, 1, p1, &a) == 1 && a.kind == YK_ACT_CREATE);  /* inactive→active = 下一波 */
    yk_fam_created(&m, 200, 1, 8010);
    CHECK(yk_map_fam_count(&m) == 4);
    CHECK(yk_frame_end(&m, s_acts, (int)YK_FAM_MAX) == 0);
    CHECK(yk_fam_broken(&m, 8010u) == 1);            /* 再打死一次照样打破 */
    CHECK(yk_map_fam_count(&m) == 3);

    /* 使魔查不到（过场 destroy_all 静默移除，不走死亡事件）⇒ 松开，但**不算打破** ⇒ 下一帧照 1:1 补回来 */
    sl = yk_map_find(&m, 200, 3);
    CHECK(sl != 0 && sl->fam_id == 8003u);
    yk_fam_gone(&m, 8003u);
    CHECK(yk_map_fam_count(&m) == 2);
    yk_frame_begin(&m);
    CHECK(yk_note_slot(&m, 200, 0, 1, p1, &a) == 0);
    CHECK(yk_note_slot(&m, 200, 1, 1, p1, &a) == 0); /* 槽 1 仍「被打破」：还是那只 8010 刚被打死 */
    CHECK(yk_note_slot(&m, 200, 2, 1, p1, &a) == 0);
    CHECK(yk_note_slot(&m, 200, 3, 1, p1, &a) == 1 && a.kind == YK_ACT_CREATE);
    yk_fam_created(&m, 200, 3, 8011);
    CHECK(yk_map_fam_count(&m) == 3);
    CHECK(yk_frame_end(&m, s_acts, (int)YK_FAM_MAX) == 0);

    /* ★★★ 2026-09-22（P1，实跑日志直证）：**判据必须是只读的**。
     * 这是那次「跟随 VM 表满 32 格 ⇒ 之后造的使魔 `stays invisible` + 敌方使魔滞留」的根因：
     * 卡侧每帧的「排除我们自己的使魔」原来调的是**事件** `yk_fam_broken`（它会清 `fam_id` + 置
     * `broken`），于是地图侧记录被清（那只真敌人变孤儿）、VM 侧记录再没人收。
     * 这里把两件事钉死：① `yk_map_is_fam` 命中时 `yk_map_t` **一个字节都不变**（含 `broken` /
     * `no_fam_frames` / 位置）；② 连问两次结果一致（幂等）；③ 对照 `yk_fam_broken` 确实会改表
     * （两者的语义差别是**故意的**，不是可以互相替换的）。*/
    {
        yk_map_t snapshot;
        yk_slot_t *cur;
        float p2[2];
        p2[0] = 1.0f; p2[1] = 2.0f;
        yk_map_reset(&m);
        yk_frame_begin(&m);
        CHECK(yk_note_slot(&m, 500, 0, 1, p2, &a) == 1 && a.kind == YK_ACT_CREATE);
        yk_fam_created(&m, 500, 0, 5555u);
        yk_frame_end(&m, s_acts, (int)YK_FAM_MAX);
        __builtin_memcpy(&snapshot, &m, sizeof m);
        cur = yk_map_find(&m, 500, 0);
        CHECK(cur != 0 && cur->fam_id == 5555u && cur->fam_seen == 1 && cur->broken == 0);
        CHECK(yk_map_is_fam(&m, 5555u) == 1);                    /* 命中 */
        CHECK(yk_map_is_fam(&m, 5555u) == 1);                    /* 幂等（连问两次）*/
        CHECK(yk_map_is_fam(&m, 0u) == 0);                       /* fam_id 0 = 空 ⇒ 恒 0 */
        CHECK(yk_map_is_fam(&m, 12345u) == 0);                   /* 不是我们的使魔 */
        CHECK(yk_map_is_fam(0, 5555u) == 0);                     /* NULL 表不崩 */
        CHECK(__builtin_memcmp(&snapshot, &m, sizeof m) == 0);   /* ★ 纯读：整张表逐字节不变 */
        CHECK(yk_fam_broken(&m, 5555u) == 1);                    /* 对照：事件版**会**改表 */
        CHECK(__builtin_memcmp(&snapshot, &m, sizeof m) != 0);
        CHECK(yk_map_is_fam(&m, 5555u) == 0);                    /* 事件之后它不再是我的使魔 */
        CHECK(yk_map_find(&m, 500, 0) != 0 && yk_map_find(&m, 500, 0)->broken == 1);
    }

    /* 表满：一个敌人的 16 槽 + 另一个敌人的 16 槽 = YK_FAM_MAX ⇒ 第 33 个槽放弃（不崩、不排队）*/
    yk_map_reset(&m);
    yk_frame_begin(&m);
    for (i = 0; i < YK_EMIT_SLOTS; ++i) {
        CHECK(yk_note_slot(&m, 300, (int)i, 1, p1, &a) == 1);
        yk_fam_created(&m, 300, (int)i, 30000u + i);
        CHECK(yk_note_slot(&m, 301, (int)i, 1, p1, &a) == 1);
        yk_fam_created(&m, 301, (int)i, 31000u + i);
    }
    CHECK(yk_map_fam_count(&m) == YK_FAM_MAX);
    CHECK(yk_note_slot(&m, 302, 0, 1, p1, &a) == 0);  /* 表满 ⇒ 放弃这个槽 */
    CHECK(yk_map_find(&m, 302, 0) == 0);
    CHECK(yk_frame_end(&m, s_acts, (int)YK_FAM_MAX) == 0);

    /* 全撤（高速 / miss / 局末 / 删卡）*/
    CHECK(yk_withdraw_all(&m, s_acts, (int)YK_FAM_MAX) == (int)YK_FAM_MAX);
    CHECK(yk_map_fam_count(&m) == 0);
    CHECK(s_acts[0].kind == YK_ACT_WITHDRAW);
    CHECK(yk_map_find_fam(&m, 30000u) == 0);
    CHECK(yk_withdraw_all(&m, s_acts, (int)YK_FAM_MAX) == 0);   /* 空表再撤是空操作 */

    /* ★ 缓冲区不够时**不许丢条目**（原来无条件 yk_map_reset ⇒ 剩下的敌人变孤儿）：
     *   表里放 2 个条目、只给 1 格缓冲 ⇒ 撤 1 个、**表里还剩 1 个**（下一帧接着撤）。*/
    CHECK(yk_note_slot(&m, 400, 0, 1, p1, &a) == 1 && a.kind == YK_ACT_CREATE);
    yk_fam_created(&m, 400, 0, 9100);
    CHECK(yk_note_slot(&m, 401, 0, 1, p1, &a) == 1 && a.kind == YK_ACT_CREATE);
    yk_fam_created(&m, 401, 0, 9101);
    CHECK(yk_map_fam_count(&m) == 2);
    CHECK(yk_withdraw_all(&m, s_acts, 1) == 1);          /* 只撤得下 1 个 */
    CHECK(yk_map_fam_count(&m) == 1);                    /* ⇒ 另一个**还在表里**（不变式不破） */
    CHECK(s_acts[0].kind == YK_ACT_WITHDRAW);
    CHECK(yk_withdraw_all(&m, s_acts, (int)YK_FAM_MAX) == 1);
    CHECK(yk_map_fam_count(&m) == 0);                    /* 处理完 ⇒ 表清空 */
    CHECK(yk_withdraw_all(&m, s_acts, 1) == 0);
    CHECK(sizeof(yk_map_t) <= 1100u);                 /* 静态表预算（现 32×32 + 16 = 1040 B；原来 ≤1024）*/
    CHECK(sizeof(yk_slot_t) == 32u);                   /* ★ owner ② 的 `eton_gap` 把 28 → 30 → 32 B（对齐）*/

    /* ============================== §3.7 伤害转交 1/2：取整 */
    yk_transfer_reset(&t);
    CHECK(t.carry == 0 && t.pending == 0);
    carry = 0;
    CHECK(yk_half_with_carry(5, &carry) == 2 && carry == 1);      /* 5 / 2 = 2 余 1 */
    CHECK(yk_half_with_carry(3, &carry) == 2 && carry == 0);      /* 5+3 = 8 ⇒ 合计 4 = 8/2，半分不丢 */
    CHECK(yk_half_with_carry(0, &carry) == 0 && carry == 0);
    CHECK(yk_half_with_carry(-7, &carry) == 0 && carry == 0);
    CHECK(yk_half_with_carry(7, 0) == 3);                         /* carry = NULL 安全 */
    CHECK(yk_half_with_carry(1, &carry) == 0 && carry == 1);
    CHECK(yk_half_with_carry(1, &carry) == 1 && carry == 0);      /* 两次 1 点 ⇒ 转 1 */

    /* ★ 2026-10-01：这条**与 `YK_TRANSFER_PCT` 绑定** —— 100 = owner 口径（全额转、不丢余数）；
     *   50 = 一手 1/2（半整数进位器）。旋钮切回 50 时这组断言仍成立。*/
    yk_transfer_reset(&t);
    yk_transfer_feed(&t, 5);
    yk_transfer_feed(&t, 3);
#if YK_TRANSFER_PCT == 100
    CHECK(t.pending == 8 && t.carry == 0);                        /* 全额：5+3 */
#elif YK_TRANSFER_PCT == 50
    CHECK(t.pending == 4 && t.carry == 0);                        /* 1/2：（5+3)/2 = 4 */
#else
    CHECK(t.pending == (int32_t)((5 + 3) * YK_TRANSFER_PCT / 100));
#endif
    yk_transfer_feed(&t, 0);                                      /* 没有伤害就不加 */
    yk_transfer_feed(&t, -9);                                     /* 负数（life 变大）也不加 */
#if YK_TRANSFER_PCT == 100
    CHECK(t.pending == 8);
#elif YK_TRANSFER_PCT == 50
    CHECK(t.pending == 4);
#else
    CHECK(t.pending == (int32_t)(8 * YK_TRANSFER_PCT / 100));
#endif
    yk_transfer_reset(&t);
    yk_transfer_feed(&t, 1);
#if YK_TRANSFER_PCT == 100
    CHECK(t.pending == 1 && t.carry == 0);
    yk_transfer_feed(&t, 1);
    CHECK(t.pending == 2 && t.carry == 0);
#elif YK_TRANSFER_PCT == 50
    CHECK(t.pending == 0 && t.carry == 1);
    yk_transfer_feed(&t, 1);
    CHECK(t.pending == 1 && t.carry == 0);
#else
    CHECK(t.pending == 1 * YK_TRANSFER_PCT / 100);
#endif

    /* 单帧上限：cap <= 0 = 无上限（占位默认，TODO 待定）*/
    CHECK(yk_transfer_clamp(4, 0) == 4);
    CHECK(yk_transfer_clamp(4, -1) == 4);
    CHECK(yk_transfer_clamp(4, 60) == 4);
    CHECK(yk_transfer_clamp(100, 60) == 60);
    CHECK(yk_transfer_clamp(0, 60) == 0);
    CHECK(yk_transfer_clamp(-5, 60) == 0);
    CHECK(YK_TRANSFER_DEN == 2);                                  /* 用户 2026-09-12 定案 1/2 */
    CHECK(YK_TRANSFER_CAP_PER_FRAME == 0);                        /* ★ 占位：还没封顶 */

    /* life 采样（逐帧差分的边界）*/
    memset(&e, 0, sizeof e);
    e.fam_id = 7;
    e.prev_life = YK_LIFE_UNSAMPLED;
    CHECK(yk_sample_life(&e, 7, 100) == 0);                       /* 第一次采样不算伤害 */
    CHECK(yk_sample_life(&e, 7, 90) == 10);
    CHECK(yk_sample_life(&e, 7, 90) == 0);                        /* 没掉血 */
    CHECK(yk_sample_life(&e, 7, 95) == 0);                        /* life 变大 = 引擎自己重算 */
    CHECK(yk_sample_life(&e, 7, 90) == 5);
    CHECK(yk_sample_life(&e, 8, 10) == 0);                        /* 换了使魔 ⇒ 重新采样 */
    CHECK(yk_sample_life(&e, 8, 4) == 6);
    CHECK(yk_sample_life(0, 1, 1) == 0);                          /* 空条目安全 */

    /* 自伤保护盒（转嫁源打 boss，别把压在 boss 身上的使魔算进来 ⇒ 防每帧自激）*/
    CHECK(yk_in_transfer_zone(0.0f, 0.0f, 0.0f, 0.0f, 16.0f, 16.0f));
    CHECK(yk_in_transfer_zone(20.0f, 0.0f, 0.0f, 0.0f, 16.0f, 16.0f));       /* 20 = 8 + 12 */
    CHECK(!yk_in_transfer_zone(20.1f, 0.0f, 0.0f, 0.0f, 16.0f, 16.0f));
    CHECK(yk_in_transfer_zone(-120.0f, 5.0f, -100.0f, 0.0f, 16.0f, 16.0f));  /* 相对中心 */
    CHECK(!yk_in_transfer_zone(-200.0f, 0.0f, -100.0f, 0.0f, 16.0f, 16.0f));

    /* ============================== §3.5 硬约束：spawn 的 life < 1000 */
    CHECK(YK_LIFE_MAX_ALLOWED < YK_LIFE_LIMIT);
    CHECK(yk_clamp_spawn_life(1) == 1);
    CHECK(yk_clamp_spawn_life(999) == 999);
    CHECK(yk_clamp_spawn_life(YK_LIFE_LIMIT) == YK_LIFE_MAX_ALLOWED);        /* 1000 ⇒ 999 */
    CHECK(yk_clamp_spawn_life(4000) == YK_LIFE_MAX_ALLOWED);                /* 原生 Lily 那种 4000 会被钳 */
    CHECK(yk_clamp_spawn_life(0) == 1);
    CHECK(yk_clamp_spawn_life(-5) == 1);
    CHECK(YK_FAM_LIFE < YK_LIFE_LIMIT);   /* ★ 占位（1，一发即破）；换成 >= 1000 的值这条会红 */

    /* ============ §3.5.1 发射口精确闸（on_enemy_shot_slot → yk_shot_slot_suppressed）============
     * 断点 0x433ffa；返回 1 ⇒ SDK 置 retaddr = 0x43402d ⇒ 本帧这一槽不发射。
     * 判据三分：① 从没管过 ⇒ 放行；② 建过又没了 ⇒ 掐；③ 表里有但还没建 ⇒ 放行。 */
    yk_map_reset(&m);
    /* ① 表里没有这个槽（我们从没管过）⇒ 一律放行 —— 否则第一帧就把敌人的发射口掐死 */
    CHECK(yk_shot_slot_suppressed(&m, 500, 0, 0) == 0);
    CHECK(yk_shot_slot_suppressed(&m, 500, 3, 0) == 0);
    /* 越界 / 空入参：绝不掐（这是「不吞正常发射」的第一道保险）*/
    CHECK(yk_shot_slot_suppressed(&m, 0, 0, 0) == 0);            /* enemy_id = 0 不是敌人 */
    CHECK(yk_shot_slot_suppressed(&m, 500, -1, 0) == 0);
    CHECK(yk_shot_slot_suppressed(&m, 500, YK_EMIT_SLOTS, 0) == 0);
    CHECK(yk_shot_slot_suppressed(0, 500, 0, 0) == 0);
    /* ③ 活跃、条目已建、但使魔还没建出来（broken == 0）⇒ 放行（建被排到下一个 on_tick_2）*/
    yk_frame_begin(&m);
    CHECK(yk_note_slot(&m, 500, 0, 1, p1, &a) == 1 && a.kind == YK_ACT_CREATE);
    CHECK(yk_shot_slot_suppressed(&m, 500, 0, 0) == 0);          /* ★ 关键：不能掐，否则永远建不出来 */
    /* 建好了、活着 ⇒ 放行 */
    yk_fam_created(&m, 500, 0, 9001);
    CHECK(yk_shot_slot_suppressed(&m, 500, 0, 9001) == 0);
    /* ② 被打死 ⇒ **掐**（= spec §9.6「打掉使魔 ⇒ 那个发射口停火」）*/
    CHECK(yk_fam_broken(&m, 9001) == 1);
    CHECK(yk_shot_slot_suppressed(&m, 500, 0, 0) == 1);
    /* 建完又活着 ⇒ 放行（确认上面那条不是恒 1）*/
    yk_fam_created(&m, 500, 0, 9001);
    CHECK(yk_shot_slot_suppressed(&m, 500, 0, 9001) == 0);
    CHECK(yk_fam_broken(&m, 9001) == 1);
    /* 别的槽不受影响（同敌人的另一槽 / 别的敌人）⇒ 不误伤 */
    CHECK(yk_shot_slot_suppressed(&m, 500, 1, 0) == 0);
    CHECK(yk_shot_slot_suppressed(&m, 501, 0, 0) == 0);
    /* ECL 重新武装（inactive → active 边沿）复位 broken ⇒ 闸放开 */
    yk_frame_begin(&m);
    CHECK(yk_note_slot(&m, 500, 0, 0, p1, &a) == 0);             /* 变不活跃：已打破 ⇒ 不撤也不建 */
    CHECK(yk_note_slot(&m, 500, 0, 1, p1, &a) == 1 && a.kind == YK_ACT_CREATE);
    CHECK(yk_shot_slot_suppressed(&m, 500, 0, 0) == 0);          /* ★ 边沿复位 ⇒ 放行，准备重建 */
    yk_fam_created(&m, 500, 0, 9002);
    CHECK(yk_shot_slot_suppressed(&m, 500, 0, 9002) == 0);
    /* 静默消失（过场 destroy_all，不走死亡事件）：`yk_fam_gone` 清 `fam_id` ⇒ 闸**当帧就夹上**
     *   （比走死亡事件更快），而下一帧的 1:1 映射会重建它、闸随之放开
     *   —— 这一段与既有的「槽 3 静默移除 ⇒ 下一帧重建」那条断言是同一个口径。 */
    yk_fam_gone(&m, 9002);
    CHECK(yk_shot_slot_suppressed(&m, 500, 0, 0) == 1);
    yk_frame_begin(&m);
    CHECK(yk_note_slot(&m, 500, 0, 1, p1, &a) == 1 && a.kind == YK_ACT_CREATE); /* 1:1 补建 */
    yk_fam_created(&m, 500, 0, 9003);
    CHECK(yk_shot_slot_suppressed(&m, 500, 0, 9003) == 0);       /* ★ 补建回来 ⇒ 闸放开 */
    /* ★ 删卡 / 松开低速 / 局末 / 断链的**唯一复位口**：yk_drop_all → yk_withdraw_all → yk_map_reset
     *   ⇒ 整表清空后闸必须**全部放开**（否则钩子还返回 1，敌人就永远哑火了）。 */
    CHECK(yk_withdraw_all(&m, s_acts, (int)YK_FAM_MAX) == 1);    /* 撤掉 9003 那一条 */
    CHECK(s_acts[0].kind == YK_ACT_WITHDRAW);
    CHECK(yk_shot_slot_suppressed(&m, 500, 0, 0) == 0);          /* ★ 闸放开 */
    CHECK(yk_map_fam_count(&m) == 0);

    /* ===== ★★ 2026-09-22（th08 #7）闸的**到期自愈**：ECL 不再重新武装时不许永久哑火 =====
     * 现象（owner 单卡实跑）：「击破 boss 一个阶段后 boss 几乎不再发弹」——boss 的发射口在
     * 整个阶段里**持续活跃**（反复 `etOn`、中间不调 `etNew`），而 `broken` 的唯一复位口是
     * inactive→active 边沿 ⇒ 闸夹死 + 使魔不再重建 ⇒ 那个槽**永远**不发弹。
     * 修法 = 连续 `YK_EMIT_REBUILD_TTL` 帧「记忆里有使魔、当前没有」⇒ 无条件放开。 */
    CHECK(YK_EMIT_REBUILD_TTL > 0);
    yk_map_reset(&m);
    yk_frame_begin(&m);
    CHECK(yk_note_slot(&m, 700, 2, 1, p1, &a) == 1 && a.kind == YK_ACT_CREATE);
    yk_fam_created(&m, 700, 2, 7001);
    CHECK(yk_fam_broken(&m, 7001) == 1);                         /* 使魔被打死 */
    CHECK(yk_shot_slot_suppressed(&m, 700, 2, 0) == 1);          /* 闸夹上（设计如此）*/
    {
        int f;
        unsigned base = yk_map_heals(&m);
        /* TTL−1 帧：仍然夹着（发射口**一直活跃**，没有边沿）*/
        for (f = 1; f < YK_EMIT_REBUILD_TTL; ++f) {
            yk_frame_begin(&m);
            CHECK(yk_note_slot(&m, 700, 2, 1, p1, &a) == 0);     /* broken ⇒ 不重建、不撤 */
            CHECK(yk_shot_slot_suppressed(&m, 700, 2, 0) == 1);
        }
        CHECK(yk_map_heals(&m) == base);                         /* 还没到期 ⇒ 没自愈 */
        /* 第 TTL 帧：到期 ⇒ 闸放开 + 本帧就重建（CREATE 动作）*/
        yk_frame_begin(&m);
        CHECK(yk_note_slot(&m, 700, 2, 1, p1, &a) == 1 && a.kind == YK_ACT_CREATE);
        CHECK(yk_shot_slot_suppressed(&m, 700, 2, 0) == 0);      /* ★ 闸放开（关键断言）*/
        CHECK(yk_map_heals(&m) == base + 1);                     /* ★ 记了一次账 */
        yk_fam_created(&m, 700, 2, 7002);
        CHECK(yk_shot_slot_suppressed(&m, 700, 2, 7002) == 0);   /* 使魔回来了 ⇒ 照旧放行 */
        /* 重建之后再打死 ⇒ 年龄**重新**从 0 起算（不会立刻又自愈）*/
        CHECK(yk_fam_broken(&m, 7002) == 1);
        yk_frame_begin(&m);
        CHECK(yk_note_slot(&m, 700, 2, 1, p1, &a) == 0);
        CHECK(yk_map_heals(&m) == base + 1);                     /* 只过了 1 帧 ⇒ 没自愈 */
        CHECK(yk_shot_slot_suppressed(&m, 700, 2, 0) == 1);
    }
    /* 到期之前 ECL 若重新武装过 ⇒ 立刻复位、**不计**自愈账（走的是边沿那条路）*/
    yk_map_reset(&m);
    yk_frame_begin(&m);
    CHECK(yk_note_slot(&m, 701, 0, 1, p1, &a) == 1 && a.kind == YK_ACT_CREATE);
    yk_fam_created(&m, 701, 0, 7101);
    CHECK(yk_fam_broken(&m, 7101) == 1);
    {
        unsigned base = yk_map_heals(&m);
        yk_frame_begin(&m);
        CHECK(yk_note_slot(&m, 701, 0, 0, p1, &a) == 0);         /* etNew：清零 type */
        CHECK(yk_note_slot(&m, 701, 0, 1, p1, &a) == 1 && a.kind == YK_ACT_CREATE); /* etSprite：写回 */
        CHECK(yk_shot_slot_suppressed(&m, 701, 0, 0) == 0);
        CHECK(yk_map_heals(&m) == base);                         /* 边沿复位**不**记账 */
    }

    /* ============ ★★★ 2026-10-01（**P1，owner 拍板**）= 闸「停到主复位口 / 长期 TTL」============
     * owner 逐字：「使魔是符卡的有机组成部分，由它来向你发弹」＋「有些**终符卡是持续发弹的**，
     * 我**高速把发弹口全清掉了**，我终符直接变成**奶奶来也能过了**」。
     * ⇒ ① **删掉** route A（卡 tick 消费 `rearm_pending`）与 route A′（第二次 `etOn` 放行）：
     *      一波之内**不再自己复活**（`rearm_pending` 降级为纯记账）；
     *    ② 主复位口 = ECL 重新武装边沿 / **波次边沿**（`yk_fam_wave_clear`）/ 相位撤壳 / 整表清；
     *    ③ **新增长期 TTL**（`YK_EMIT_REBUILD_TTL` = 600 帧 ≈ 10 s）到期自动放行 + 重建。 */
    /* ★★★ 2026-10-01（owner 实跑 ②）：加了 `eton_gap`（`etOn` 间隔 ⇒ 波次边沿）⇒ 28 → 30 → 32 B
     *   （对齐）；`yk_map_t` = 32×32+16 = 1040 B（静态表，不进 496 B 的卡状态预算）。*/
    CHECK(sizeof(yk_slot_t) == 32u);
    CHECK(YK_EMIT_REBUILD_TTL == 600u);               /* ★ owner 2026-10-01：≈10 s 的自动重建 */
    yk_map_reset(&m);
    yk_frame_begin(&m);
    CHECK(yk_note_slot(&m, 800, 0, 1, p1, &a) == 1 && a.kind == YK_ACT_CREATE);
    yk_fam_created(&m, 800, 0, 8001);
    CHECK(yk_shot_slot_gate(&m, 800, 0, 8001) == 0);            /* 使魔活着 ⇒ 放行，不记账 */
    CHECK(yk_map_rearms(&m) == 0);
    CHECK(yk_fam_broken(&m, 8001) == 1);                        /* 人侧打死了它（妖侧无敌，打不死）*/
    {
        unsigned base = yk_map_rearms(&m);
        unsigned hbase = yk_map_heals(&m);
        unsigned lbase = yk_map_releases(&m);
        CHECK(yk_shot_slot_gate(&m, 800, 0, 0) == 1);           /* 第一次 etOn：掐 + 记一次夹持账 */
        CHECK(yk_map_rearms(&m) == base + 1);
        /* ★★★ P1 回归断言：**第二次（以及之后每一次）被问都照旧掐** —— 一波之内不再自己复活。*/
        CHECK(yk_shot_slot_gate(&m, 800, 0, 0) == 1);
        CHECK(yk_shot_slot_gate(&m, 800, 0, 0) == 1);
        CHECK(yk_map_rearms(&m) == base + 1);                   /* 不重复记「掐」的账 */
        CHECK(yk_map_releases(&m) == lbase);                    /* ★ route A′ 的「第二问放行」已删 */
        CHECK(yk_map_heals(&m) == hbase);
        CHECK(yk_shot_slot_suppressed(&m, 800, 0, 0) == 1);     /* ★ 闸仍然夹着 */
        /* ★ P1 回归断言：**卡 tick 不再消费 `rearm_pending`** ⇒ 跑几帧也不放行、不重建。*/
        CHECK(yk_frame_end(&m, s_acts, (int)YK_FAM_MAX) == 0);
        {   int f;
            for (f = 0; f < 5; ++f) {
                yk_frame_begin(&m);
                CHECK(yk_note_slot(&m, 800, 0, 1, p1, &a) == 0);   /* broken ⇒ 不重建、不撤 */
                CHECK(yk_shot_slot_suppressed(&m, 800, 0, 0) == 1);
            }
        }
        CHECK(yk_map_releases(&m) == lbase);
        /* ★ 主复位口 ②（**波次边沿**）：收到 `enmKillAll` 广播 ⇒ `yk_fam_wave_clear` 放行 + 重建。*/
        CHECK(yk_fam_wave_clear(&m, 999999u) == 0);             /* 不是我们的使魔 ⇒ 什么都不做 */
        CHECK(yk_map_releases(&m) == lbase);
        CHECK(yk_fam_wave_clear(&m, 8001u) == 0);               /* 已经被 broken 清掉 fam_id ⇒ 查不到 */
        /* 造一只新的、再让它被清场打死 */
        yk_frame_begin(&m);
        CHECK(yk_note_slot(&m, 800, 0, 1, p1, &a) == 0);        /* 仍夹着 ⇒ 不建 */
        yk_map_reset(&m);
        yk_frame_begin(&m);
        CHECK(yk_note_slot(&m, 800, 0, 1, p1, &a) == 1 && a.kind == YK_ACT_CREATE);
        yk_fam_created(&m, 800, 0, 8002);
        CHECK(yk_fam_wave_clear(&m, 8002u) == 1);               /* ★ 波次边沿：放行 */
        CHECK(yk_map_releases(&m) == lbase + 1);                /* ★ 记一笔「波次边沿放行」 */
        CHECK(yk_shot_slot_suppressed(&m, 800, 0, 0) == 0);     /* ★ 闸已放开（关键断言）*/
        CHECK(yk_frame_end(&m, s_acts, (int)YK_FAM_MAX) == 0);
        yk_frame_begin(&m);
        CHECK(yk_note_slot(&m, 800, 0, 1, p1, &a) == 1 && a.kind == YK_ACT_CREATE);  /* 1:1 映射重建 */
        CHECK(yk_map_heals(&m) == hbase);                       /* ★ 没走长期 TTL 兜底 */
        yk_fam_created(&m, 800, 0, 8003);
        CHECK(yk_shot_slot_gate(&m, 800, 0, 8003) == 0);        /* 下一次 etOn：使魔已回来 ⇒ 放行 */
        /* 再来一轮：证明 wave_clear 是可重复的（建 → 清场打死 → 波次边沿放行 → 重建）。
         * ⚠ 清场那条路**只调 `yk_fam_wave_clear`**（不像玩家击破那样先走 `yk_fam_broken`）——
         *   后者会清 `fam_id` ⇒ 再按 id 找就找不到了。*/
        CHECK(yk_fam_wave_clear(&m, 8003u) == 1);               /* 清场直接命中（fam_id 还在）*/
        CHECK(yk_map_releases(&m) == lbase + 2);
        yk_frame_begin(&m);
        CHECK(yk_note_slot(&m, 800, 0, 1, p1, &a) == 1 && a.kind == YK_ACT_CREATE);
        yk_fam_created(&m, 800, 0, 8004);
    }
    /* 没被夹住的槽：闸恒放行、一次都不记账（route A 绝不误伤）。
     * ⚠ `rearms`/`heals` 都是**只增不清**的（`yk_map_reset` 不动它们，卡侧靠快照比对打日志）
     *   ⇒ 这里一律与快照比，别写绝对值。*/
    {
    unsigned rbase = yk_map_rearms(&m);
    yk_map_reset(&m);
    yk_frame_begin(&m);
    CHECK(yk_note_slot(&m, 801, 0, 1, p1, &a) == 1 && a.kind == YK_ACT_CREATE);
    CHECK(yk_shot_slot_gate(&m, 801, 0, 0) == 0);               /* 还没建完：放行（第一道保险） */
    CHECK(yk_shot_slot_gate(&m, 801, 1, 0) == 0);               /* 别的槽 / 从没管过 */
    CHECK(yk_shot_slot_gate(&m, 0, 0, 0) == 0);
    CHECK(yk_shot_slot_gate(0, 801, 0, 0) == 0);
    CHECK(yk_map_rearms(&m) == rbase);
    /* 整表复位（撤卡 / 松低速 / 局末）⇒ 待处理的「下一波」也一起清掉，不留悬案 */
    yk_fam_created(&m, 801, 0, 8101);
    CHECK(yk_fam_broken(&m, 8101) == 1);
    CHECK(yk_shot_slot_gate(&m, 801, 0, 0) == 1);
    yk_map_reset(&m);
    yk_frame_begin(&m);
    CHECK(yk_note_slot(&m, 801, 0, 1, p1, &a) == 1 && a.kind == YK_ACT_CREATE);  /* 新条目、无残留 */
    CHECK(yk_shot_slot_suppressed(&m, 801, 0, 0) == 0);
    }

    /* ============ ★★★ 2026-10-01（owner 实跑 ②）：**波次边沿 = `etOn` 的间隔** ============
     * 一手 ECL（`local/_scratch/st05mbs.asm` 的 `MBoss1`）：一波 = 一次 `MBoss1_at`（`etNew(0/1)`
     * + 配置 + 30 × (`etOn`×4 + `wait(12)`）），波间 = `wait(30)`×2 + moveRand ≥ 60 帧
     * ⇒ 阈值 `YK_EMIT_WAVE_GAP`(24) 能把「波内 12 帧」与「波间 ≥60 帧」分开。*/
    CHECK(YK_EMIT_WAVE_GAP == 24);
    CHECK(YK_EMIT_WAVE_GAP > 12 && YK_EMIT_WAVE_GAP < 60);   /* 一手两个间隔之间 */
    /* ★★★ 2026-10-01（#209 · owner 主需求「打死使魔 ⇒ 关闭发射口」）：**自适应判据**
     *   `yk_emit_gap_is_new_wave(eton_gap, gap_ref)` = 间隔 ≥ 24 **且** ≥ `4 × gap_ref`
     *   ⇒ **缓发敌人（波内间隔就有 30/60 帧）不会被误判成「新一波」而白放行**。*/
    CHECK(yk_emit_gap_is_new_wave(10, 5) == 0);      /* 波内（短间隔）*/
    CHECK(yk_emit_gap_is_new_wave(24, 5) == 1);      /* 24 ≥ max(24, 20) ⇒ 新波 */
    CHECK(yk_emit_gap_is_new_wave(24, 12) == 0);     /* ★ st4 类：波内 12 帧 ⇒ 24 **不算**新波 */
    CHECK(yk_emit_gap_is_new_wave(60, 12) == 1);     /* 真波间（60 ≥ 48）*/
    CHECK(yk_emit_gap_is_new_wave(30, 30) == 0);     /* ★ 缓发敌人：30 帧节奏 ⇒ 30 不放行 */
    CHECK(yk_emit_gap_is_new_wave(200, 30) == 1);    /* 缓发敌人的真波间 */
    CHECK(yk_emit_gap_is_new_wave(300, 0) == 1);     /* 还没观测到节奏 ⇒ 只用绝对阈值 */
    yk_map_reset(&m);
    yk_frame_begin(&m);
    CHECK(yk_note_slot(&m, 850, 0, 1, p1, &a) == 1 && a.kind == YK_ACT_CREATE);
    yk_fam_created(&m, 850, 0, 8501);
    CHECK(yk_fam_broken(&m, 8501) == 1);                        /* 玩家打死了它（本波从此哑火）*/
    {
        unsigned gbase = yk_map_gap_releases(&m);
        unsigned i;
        CHECK(yk_shot_slot_gate(&m, 850, 0, 0) == 1);           /* 本波第一条 etOn：照掐 */
        /* 先在**波内**打两发（间隔 5 帧）⇒ 让闸学到「这一槽的常规节奏 = 5 帧」*/
        for (i = 0; i < 2; ++i) {
            int f;
            for (f = 0; f < 5; ++f) { yk_frame_begin(&m); CHECK(yk_note_slot(&m, 850, 0, 1, p1, &a) == 0); }
            CHECK(yk_shot_slot_gate(&m, 850, 0, 0) == 1);       /* 5 帧间隔 ⇒ 波内 ⇒ 仍掐 */
        }
        CHECK(yk_map_gap_releases(&m) == gbase);
        /* 波内长间隔（24 帧 < 4×5=20? 不成立 ⇒ 24 ≥ 20 会放行；这里用 23 帧验证「不够长不放行」）*/
        for (i = 0; i < (unsigned)(YK_EMIT_WAVE_GAP - 1); ++i) {
            yk_frame_begin(&m);
            (void)yk_note_slot(&m, 850, 0, 1, p1, &a);
        }
        CHECK(yk_shot_slot_gate(&m, 850, 0, 0) == 1);           /* 间隔 < 阈值 ⇒ 仍掐 */
        CHECK(yk_map_gap_releases(&m) == gbase);
        /* 跨过阈值（≥24 且 ≥ 4×5=20）⇒ 判定「新的一波」⇒ **放行**（这一发照发）+ 记账 */
        for (i = 0; i < (unsigned)YK_EMIT_WAVE_GAP; ++i) {
            yk_frame_begin(&m);
            (void)yk_note_slot(&m, 850, 0, 1, p1, &a);          /* 每帧的卡 tick（年龄在这里涨）*/
        }
        CHECK(yk_shot_slot_gate(&m, 850, 0, 0) == 0);           /* ★ 放行（关键断言）*/
        CHECK(yk_map_gap_releases(&m) == gbase + 1);            /* ★ 记一笔「间隔 ⇒ 波次」*/
        /* ★ #209 端到端账：#209 的判据行 = 「这一槽放行前被否决过几条 etOn」*/
        CHECK(yk_map_last_release_slot(&m) == 0);
        CHECK(yk_map_last_release_enemy(&m) == 850);
        CHECK(yk_map_last_release_vetoed(&m) >= 4);             /* 夹持期里确实被否决过多条 */
        CHECK(yk_shot_slot_suppressed(&m, 850, 0, 0) == 0);     /* 夹持态已清 ⇒ 闸恒放 */
        /* 使魔在下一帧被 1:1 映射重建；重建后再打死 ⇒ 新一波照旧从 0 起算 */
        CHECK(yk_note_slot(&m, 850, 0, 1, p1, &a) == 1 && a.kind == YK_ACT_CREATE);
        yk_fam_created(&m, 850, 0, 8502);
        CHECK(yk_fam_broken(&m, 8502) == 1);
        CHECK(yk_shot_slot_gate(&m, 850, 0, 0) == 1);
        yk_frame_begin(&m);
        CHECK(yk_shot_slot_gate(&m, 850, 0, 0) == 1);           /* 才 1 帧 ⇒ 仍掐（不会立刻又放）*/
        CHECK(yk_map_gap_releases(&m) == gbase + 1);
    }
    /* 没有使魔的槽 / 从没建过：间隔那条路**绝不**误放（判据要求 `fam_seen`）*/
    {
        unsigned gbase = yk_map_gap_releases(&m);
        yk_map_reset(&m);
        yk_frame_begin(&m);
        CHECK(yk_note_slot(&m, 851, 0, 1, p1, &a) == 1 && a.kind == YK_ACT_CREATE);
        yk_fam_created(&m, 851, 0, 8511);
        {   int i; for (i = 0; i < YK_EMIT_WAVE_GAP + 5; ++i) yk_frame_begin(&m); }
        CHECK(yk_shot_slot_gate(&m, 851, 0, 8511) == 0);        /* 使魔活着 ⇒ 放行、不记账 */
        CHECK(yk_map_gap_releases(&m) == gbase);
        CHECK(yk_shot_slot_gate(&m, 851, 1, 0) == 0);           /* 从没管过的槽 ⇒ 放行 */
        CHECK(yk_map_gap_releases(&m) == gbase);
    }

    /* ============ ★★★ 2026-10-01（owner 口径 ④）：**锁形态**的两个纯判据 ============
     * owner 逐字：「当以**人形态放 B** 的时候，**B 的持续时间内应当固定为人形态**；**妖形态同理**」
     * ＋「**灵击不算 B**」＋「灵界 or 暴走咆哮状态 ⇒ **shift 别把妖子机放出来**（主机贴图不动）」。*/
    CHECK(yk_is_real_bomb(0, 0) == 0);          /* 没在放 B */
    CHECK(yk_is_real_bomb(0, 1) == 0);
    CHECK(yk_is_real_bomb(1, 0) == 1);          /* 真 B ⇒ 要锁 */
    CHECK(yk_is_real_bomb(1, 1) == 0);          /* ★ 灵击（起手在决死窗口里）⇒ **不算 B**、不锁 */
    CHECK(yk_side_switch_allowed(0, 0, 0) == 1);    /* 三个都没有 ⇒ 照常可切 */
    CHECK(yk_side_switch_allowed(1, 0, 0) == 0);    /* 灵界 ⇒ 锁 */
    CHECK(yk_side_switch_allowed(0, 1, 0) == 0);    /* 暴走咆哮 ⇒ 锁 */
    CHECK(yk_side_switch_allowed(0, 0, 1) == 0);    /* 真 B ⇒ 锁 */
    CHECK(yk_side_switch_allowed(1, 1, 1) == 0);    /* 取或 */

    /* ============ ★★★ 2026-10-01（owner 拍板 **D2 = 插值跟随**）：`yk_follow_step()` ============
     * 一手量级 = TH08 `moveRandTime(60, 4, 1.0f)`（60 帧飞过去）；语义见 `youkai_core.h`。*/
    /* ★ 2026-10-01 晚：owner 拍「那还是瞬移吧」⇒ 发布默认 = **0 = 硬钉**（D2 插值代码保留，旋钮调 >0 即启用）。*/
    CHECK(YK_EMIT_FOLLOW_FRAMES == 0);
    {
        float p[2];
        /* ① `frames = 0` ⇒ 硬钉（旧行为）*/
        p[0] = 5.0f; p[1] = 7.0f;
        yk_follow_step(p, 0.0f, 0.0f, 100.0f, -50.0f, 0, 0);
        CHECK(p[0] == 100.0f && p[1] == -50.0f);
        /* ② 首帧不动（age = 0）：留在起点 */
        p[0] = 5.0f; p[1] = 7.0f;
        yk_follow_step(p, 5.0f, 7.0f, 65.0f, 7.0f, 0, 60);
        CHECK(p[0] == 5.0f && p[1] == 7.0f);
        /* ③ 60 帧内收敛：单调、不回弹、第 60 帧精确到位 */
        {
            float prev = 5.0f;
            int i;
            int ok_mono = 1, ok_range = 1;
            p[0] = 5.0f; p[1] = 7.0f;
            for (i = 1; i <= 60; ++i) {
                yk_follow_step(p, 5.0f, 7.0f, 65.0f, 7.0f, i, 60);
                if (p[0] < prev - 0.0001f) ok_mono = 0;          /* 不回弹（超调也在这里被抓住）*/
                if (p[0] > 65.0001f) ok_range = 0;               /* 不越过目标 */
                prev = p[0];
            }
            CHECK(ok_mono == 1);
            CHECK(ok_range == 1);
            CHECK(p[0] == 65.0f && p[1] == 7.0f);                /* 第 60 帧精确到位 */
        }
        /* ④ 目标不变时吸附（剩余 < 1 px ⇒ 直接贴到目标，不再抖）*/
        p[0] = 0.0f; p[1] = 0.0f;
        yk_follow_step(p, 64.5f, 7.0f, 65.0f, 7.0f, 30, 60);   /* 起点离目标仅 0.5 px */
        CHECK(p[0] == 65.0f && p[1] == 7.0f);
        /* ⑤ 斜向也收敛（x/y 同步推进）*/
        p[0] = 0.0f; p[1] = 0.0f;
        yk_follow_step(p, 0.0f, 0.0f, 30.0f, 40.0f, 60, 60);
        CHECK(p[0] == 30.0f && p[1] == 40.0f);
        /* ⑥ 空指针安全 */
        yk_follow_step(0, 0.0f, 0.0f, 1.0f, 1.0f, 1, 60);
    }

    /* ============ ★★★ 2026-10-01（owner：「瞬移不会重建血量吧」）：重建判据与位置无关 ============
     * `yk_shell_needs_rebuild(have_shell, broken)` **入参里没有位置** —— 这就是「瞬移不重建」的结构性证明；
     * 位置那条路（`yk_pin_follow_step`/`yk_pin_familiar`）只写 `CE_ENEMY_POS_X/Y` 与 `YK_ENEMY_ABS_POS`。*/
    CHECK(yk_shell_needs_rebuild(0, 0) == 1);      /* 没壳、没被打死 ⇒ 建 */
    CHECK(yk_shell_needs_rebuild(1, 0) == 0);      /* 有活壳 ⇒ **绝不**重建（无论它挪到哪）*/
    CHECK(yk_shell_needs_rebuild(1, 1) == 0);
    CHECK(yk_shell_needs_rebuild(0, 1) == 0);      /* 被打死 ⇒ 由闸/波次边沿决定，不在这里重建 */
    /* 位置变化不影响任何寿命/重建判据：`yk_follow_step` 只写 xy[0]/xy[1]（结构上无别的出口）。*/
    {
        float q[2];
        q[0] = 3.0f; q[1] = 4.0f;
        yk_follow_step(q, 0.0f, 0.0f, 60.0f, 0.0f, 30, 60);
        CHECK(q[0] == 30.0f && q[1] == 0.0f);       /* 只有位置被写 */
    }

    /* ============ ★★★ 2026-10-01（owner 硬约束 ②）：时符名判据 ============
     * 符卡名 = `zSpellcard+0x34`（`Spellcard__start` 逐字节拷入）⇒ 按字节判「時符/时符」开头。*/
    {
        static const uint8_t u16_t[8] = { 0x42, 0x66, 0x26, 0x7B, 0x00, 0x00, 0x00, 0x00 }; /* 時符 */
        static const uint8_t u16_s[8] = { 0xF6, 0x65, 0x26, 0x7B, 0x00, 0x00, 0x00, 0x00 }; /* 时符 */
        static const uint8_t u8_t[8]  = { 0xE6, 0x99, 0x82, 0xE7, 0xAC, 0xA6, 0x00, 0x00 };
        static const uint8_t sjis[8]  = { 0x8E, 0x9E, 0x95, 0x84, 0x00, 0x00, 0x00, 0x00 };
        static const uint8_t other[8] = { 0xE7, 0xAC, 0xA6, 0x00, 0x00, 0x00, 0x00, 0x00 }; /* 「符」开头 */
        CHECK(yk_spell_name_is_time(u16_t, 8) == 1);
        CHECK(yk_spell_name_is_time(u16_s, 8) == 1);
        CHECK(yk_spell_name_is_time(u8_t, 8) == 1);
        CHECK(yk_spell_name_is_time(sjis, 8) == 1);
        CHECK(yk_spell_name_is_time(other, 8) == 0);
        CHECK(yk_spell_name_is_time(0, 8) == 0);
        CHECK(yk_spell_name_is_time(u16_t, 3) == 0);      /* 太短 ⇒ 不判 */
        /* ★★★ 2026-10-01（owner 真判据）：「**超时不算 bonus failed**」= `zSpellcard+0x78` **bit3**
         *   （耐久符卡；一手 `0x42efb0`–`0x42f039`：bit3 ⇒ 不置 bit7、不清 `+0x7c`）⇒ 优先于名字。*/
        CHECK(yk_spell_xfer_block(0, 0) == YK_XFER_BLOCK_NONE);
        CHECK(yk_spell_xfer_block(0, 1) == YK_XFER_BLOCK_NAME);       /* 只有名字命中 ⇒ 兜底 */
        CHECK(yk_spell_xfer_block(YK_SPELL_FLAG_SURVIVAL, 0) == YK_XFER_BLOCK_FLAG);   /* ★ 真判据 */
        CHECK(yk_spell_xfer_block(YK_SPELL_FLAG_SURVIVAL, 1) == YK_XFER_BLOCK_FLAG);   /* flag 优先 */
        CHECK(yk_spell_xfer_block(1, 0) == YK_XFER_BLOCK_NONE);       /* bit0(进行中) 本身不抑制 */
    }

    /* ============ §3.10 人妖槽（TH08 `youkaiGauge`；限值/判定/几何全部一手）============ */
    /* 夹紧：`th08:0x43c0fa`（下限）/`th08:0x43c128`（上限），两条都**取等号**。*/
    CHECK(yk_gauge_clamp(0) == 0);
    CHECK(yk_gauge_clamp(9999) == 9999);
    CHECK(yk_gauge_clamp(10000) == YK_GAUGE_YOUKAI_LIMIT);
    CHECK(yk_gauge_clamp(12345) == YK_GAUGE_YOUKAI_LIMIT);
    CHECK(yk_gauge_clamp(-12345) == YK_GAUGE_HUMAN_LIMIT);
    /* 加法先按 i16 截断再夹（`th08:0x43c0e9` 的 `mov WORD`）——小增量下与直接加等价。*/
    CHECK(yk_gauge_add(9000, 500) == 9500);
    CHECK(yk_gauge_add(9900, 500) == YK_GAUGE_YOUKAI_LIMIT);
    CHECK(yk_gauge_add(-9900, -500) == YK_GAUGE_HUMAN_LIMIT);
    CHECK(yk_gauge_add(0, 200) == 200);
    /* 死区：`|g| <= 9 ⇒ 0`（`th08:0x44bf4d` 的 fcomp 9.0 → `:0x44bf61`）。*/
    CHECK(yk_gauge_dead_zone(9) == 0);
    CHECK(yk_gauge_dead_zone(-9) == 0);
    CHECK(yk_gauge_dead_zone(0) == 0);
    CHECK(yk_gauge_dead_zone(10) == 10);
    CHECK(yk_gauge_dead_zone(-10) == -10);
    /* 四档判定（`th08:0x406d10/40/70/a0`；极人/中人 ≤、极妖/中妖 ≥，**取等号**）。*/
    CHECK(yk_gauge_is_extremely_human(-8000) == 1);
    CHECK(yk_gauge_is_extremely_human(-7999) == 0);
    CHECK(yk_gauge_is_moderately_human(-2000) == 1);
    CHECK(yk_gauge_is_moderately_human(-1999) == 0);
    CHECK(yk_gauge_is_extremely_youkai(8000) == 1);
    CHECK(yk_gauge_is_extremely_youkai(7999) == 0);
    CHECK(yk_gauge_is_moderately_youkai(2000) == 1);
    CHECK(yk_gauge_is_moderately_youkai(1999) == 0);
    CHECK(yk_gauge_is_extremely_human(0) == 0 && yk_gauge_is_extremely_youkai(0) == 0);
    /* 收刻符的增量：聚焦 = +111、否则 −111（`th08:0x44139e`/`:0x4413b0`）。*/
    CHECK(yk_gauge_orbs_delta(1) == 111 && yk_gauge_orbs_delta(0) == -111);
    /* ★★★ 2026-09-22（P3 第二批）：射击中每帧 ±26（一手 `Player.cpp:445-452` + `:541`
     * `0x6590 / 0x3e7` = 26000/999 = 26，方向 = `shotType & 1` = isYoukaiMode ⇒ 本卡口径的低速位）。
     * ★ 2026-09-27 订正（§BE #110）：`yk_gauge_gate_blocks` 现在**只**给推进支用（参数 = `shotActive`
     *   = TH08 `player+0xfdc`）；击破 ±200 / 收刻符 ±111 / 擦弹 +100 三笔**不再过闸** ——
     *   它们对应的是 `AddToYoukaiGauge` 的第一道闸 `th08:0x43c0c2`，而它读的 `ds:0x17d6ed4`
     *   全 exe 无写点又是 BSS ⇒ 恒为 0 ⇒ 那道闸永不生效。*/
    CHECK(YK_GAUGE_SHOOT_DELTA == 26);
    CHECK(yk_gauge_shoot_delta(1) == 26 && yk_gauge_shoot_delta(0) == -26);
    CHECK(yk_gauge_gate_blocks(1) == 1 && yk_gauge_gate_blocks(0) == 0);
    /* ★★★ 2026-09-22（永点三来源之一）：一手 `th08:0x42da54` 的**或门** ——
     * 「極端人間 **或** 極端妖怪」时那一颗刻符是 **state 1**（生成即满速归航 = 自动吸收）。*/
    CHECK(yk_orb_instant_absorb(-8000) == 1);      /* 極端人間（`<= −8000`）*/
    CHECK(yk_orb_instant_absorb(8000) == 1);       /* 極端妖怪（`>= +8000`）*/
    CHECK(yk_orb_instant_absorb(-3000) == 0);      /* 中人：不成立 */
    CHECK(yk_orb_instant_absorb(0) == 0);
    /* ★★★ 2026-09-22（P3 第二批）：射击中每帧 ±26（一手 `Player.cpp:445-452` + `:541`
     * `0x6590 / 0x3e7` = 26000/999 = 26，方向 = `shotType & 1` = isYoukaiMode ⇒ 本卡口径的低速位）。
     * ★ 2026-09-27 订正（§BE #110）：`yk_gauge_gate_blocks` 现在**只**给推进支用（参数 = `shotActive`
     *   = TH08 `player+0xfdc`）；击破 ±200 / 收刻符 ±111 / 擦弹 +100 三笔**不再过闸** ——
     *   它们对应的是 `AddToYoukaiGauge` 的第一道闸 `th08:0x43c0c2`，而它读的 `ds:0x17d6ed4`
     *   全 exe 无写点又是 BSS ⇒ 恒为 0 ⇒ 那道闸永不生效。*/
    CHECK(YK_GAUGE_SHOOT_DELTA == 26);
    CHECK(yk_gauge_shoot_delta(1) == 26 && yk_gauge_shoot_delta(0) == -26);
    CHECK(yk_gauge_gate_blocks(1) == 1 && yk_gauge_gate_blocks(0) == 0);
    /* 染色五档（`AsciiManager.cpp:1035-1058`，顺序即优先级）。*/
    CHECK(yk_gauge_color(-9000) == 0x7070ffu);
    CHECK(yk_gauge_color(-3000) == 0xb0b0ffu);
    CHECK(yk_gauge_color(9000) == 0xff7070u);
    CHECK(yk_gauge_color(3000) == 0xffb0b0u);
    CHECK(yk_gauge_color(0) == 0xffffffu);
    CHECK(yk_gauge_color(-8000) == 0x7070ffu);      /* 极人优先于中人 */
    CHECK(yk_gauge_color(8000) == 0xff7070u);       /* 极妖优先于中妖 */
    /* HUD 几何（一手 `AsciiManager`，**640 空间的字面量**）：**输出一律表面 px** ——
     * `px = 1/to640 = 2`（TH18）。2026-09-22（P2）实跑定案：那批 VM 的脚本**显式写 `resolutionMode(2)`**
     * ⇒ `th18:0x4063d0` 的 mode 2 支 = `DAT × 0.5` = **×1**（= 1 单位 = 1 表面 px），写 640 空间的数会
     * 「位置高半个屏幕 + 整体缩小 50%」（owner th08 #14/#15）。全文见 `youkai_core.h` 的注。
     * 锚点 = **游玩区左下角**（表面 px）——2026-09-26（#39）改成一手换算的 `(257,928)`，
     * 所有 y = `ay − (464 − y640)·k`（464 = TH08 游玩区的下沿，见 `YK_GAUGE_640_FLOOR`）。*/
    {
        yk_gauge_hud_t h;
        float a[2];
        const float ax = 257.0f, ay = 928.0f;                       /* 表面 px（锚点直接用）*/
        const float k  = 2.0f;                                      /* 640 空间 → 表面 px */
        a[0] = 257.0f; a[1] = 928.0f;
        yk_gauge_hud(0, a, 0.5f, &h);
        CHECK(h.px == k);                                           /* ★ P2：倍数出口 = 2 */
        /* ★ 2026-09-22 晚订正：槽条的停稳 y = **449**（`ascii.anm` script5 的 `posTime` 终点），
         * 465 只是滑入动画的起点 ⇒ 与两个图标同高（原来写 465 ⇒ 条比图标低 16 px）。
         * ★ 2026-09-26（#39）：基准从 481 改成 464（TH08 游玩区下沿）⇒ 整块下移 34 px。*/
        CHECK(h.bar_x == ax && h.bar_y == ay - 15.0f * k);          /* 槽条 128×16，左上 ⇒ 表面 (257,898) */
        CHECK(h.bar_y == h.human_y);                                /* ★ 条与图标同高（一手 449）*/
        CHECK(h.human_x == ax && h.human_y == ay - 15.0f * k);      /* 人间图标在条左端（88−56−32=0） */
        CHECK(h.youkai_x == ax + 112.0f * k);                       /* 妖怪图标在条右端（88+56−32=112） */
        CHECK(h.cursor_x == ax + 64.0f * k);                        /* g=0 ⇒ 96−32=64 */
        CHECK(h.cursor_y == ay - 11.0f * k);
        CHECK(h.text_x == ax + 64.0f * k);
        CHECK(h.text_y == ay - 18.0f * k);
        CHECK(h.night_x == ax + 48.0f * k);                         /* 一手 x = 80 ⇒ 80 − 32 = 48 */
        CHECK(h.night_y == ay - 4.0f * k);
        /* 满妖侧：游标走到条右端（96+56−32 = 120）。*/
        yk_gauge_hud(10000, a, 0.5f, &h);
        CHECK(h.cursor_x == ax + 120.0f * k);
        CHECK(h.text_x == ax + 64.0f * k + 40.0f * k);
        /* 满人侧：游标走到条左端。*/
        yk_gauge_hud(-10000, a, 0.5f, &h);
        CHECK(h.cursor_x == ax + 8.0f * k);                         /* 96−56−32 = 8 */
        /* 越界输入按限值夹（引擎/卡状态给脏值时不跑到条外）。*/
        yk_gauge_hud(999999, a, 0.5f, &h);
        CHECK(h.cursor_x == ax + 120.0f * k);
        /* 锚点 NULL / to640 非法 ⇒ 锚点当 0、比例回默认 2（不崩）。*/
        yk_gauge_hud(0, 0, 0.5f, &h);
        CHECK(h.bar_x == 0.0f && h.bar_y == -(464.0f - 449.0f) * k);
        yk_gauge_hud(0, a, 0.0f, &h);
        CHECK(h.px == 2.0f && h.bar_x == 257.0f);                   /* to640 = 0 ⇒ 倍数回 2，锚点仍是表面 px */
        yk_gauge_hud(0, a, 0.5f, 0);                                /* out = NULL：什么都不做 */
    }

    /* ============ ★★ 人妖槽百分比数字的字形布局（一手 `th08:0x405e10`）============ */
    {
        yk_gauge_glyph_t g[YK_GAUGE_TEXT_MAX];
        int n, i;

        /* 0 ⇒ "0.00%"：5 个字形（1 整数 + '.' + 2 小号 + '%'）。*/
        n = yk_gauge_text_glyphs(0, g);
        CHECK(n == 5);
        CHECK(g[0].glyph == 0 && g[1].glyph == YK_GAUGE_GLYPH_DOT);
        CHECK(g[4].glyph == YK_GAUGE_GLYPH_PERCENT);
        CHECK(g[2].glyph == 0 && g[3].glyph == 0);
        CHECK(g[2].scale == 0.8f && g[3].scale == 0.8f);            /* 小数两位 = 小号 */
        CHECK(g[2].dy == 2.0f && g[3].dy == 2.0f);                  /* 且 y += 2（一手）*/
        CHECK(g[0].scale == 1.0f && g[4].scale == 1.0f && g[4].dy == 0.0f);
        /* 位数只影响居中：n = 4 + 1（<1000）⇒ xPos = 5*3.5 − 3.5 − 4 = 10 ⇒ dx0 = −10。*/
        CHECK(g[0].dx == -10.0f);
        /* 步进：整数 7、'.' 之后 5、小号 5、收尾 7（逐条抄 `DrawPercentage`）。*/
        CHECK(g[1].dx == g[0].dx + 7.0f);
        CHECK(g[2].dx == g[1].dx + 5.0f);
        CHECK(g[3].dx == g[2].dx + 5.0f);
        CHECK(g[4].dx == g[3].dx + 7.0f);

        /* 1234（= 12.34%）⇒ "12.34%"：6 个字形，整数两位。*/
        n = yk_gauge_text_glyphs(1234, g);
        CHECK(n == 6);
        CHECK(g[0].glyph == 1 && g[1].glyph == 2 && g[2].glyph == YK_GAUGE_GLYPH_DOT);
        CHECK(g[3].glyph == 3 && g[4].glyph == 4 && g[5].glyph == YK_GAUGE_GLYPH_PERCENT);
        CHECK(g[3].scale == 0.8f && g[4].scale == 0.8f);
        CHECK(g[0].dx == -((6 * 3.5f - 3.5f - 4.0f)));              /* n = 4 + 2 */

        /* 负值 ⇒ 前面多一个 '-'（步进 7），位数 +1。*/
        n = yk_gauge_text_glyphs(-1234, g);
        CHECK(n == 7);
        CHECK(g[0].glyph == YK_GAUGE_GLYPH_MINUS && g[0].scale == 1.0f && g[0].dy == 0.0f);
        CHECK(g[1].glyph == 1 && g[1].dx == g[0].dx + 7.0f);
        CHECK(g[6].glyph == YK_GAUGE_GLYPH_PERCENT);
        CHECK(g[0].dx == -((7 * 3.5f - 3.5f - 4.0f)));

        /* ±10000 ⇒ 固定 "100.00" / "-100.00"（一手：≥10000 那一支）。*/
        n = yk_gauge_text_glyphs(10000, g);
        CHECK(n == 7);                                              /* "100.00" + '%' */
        CHECK(g[0].glyph == 1 && g[1].glyph == 0 && g[2].glyph == 0);
        CHECK(g[3].glyph == YK_GAUGE_GLYPH_DOT);
        CHECK(g[4].glyph == 0 && g[5].glyph == 0);
        CHECK(g[4].scale == 0.8f && g[5].scale == 0.8f && g[5].dy == 2.0f);
        CHECK(g[6].glyph == YK_GAUGE_GLYPH_PERCENT);
        n = yk_gauge_text_glyphs(-10000, g);
        CHECK(n == YK_GAUGE_TEXT_MAX);                              /* 8 = '-' + "100.00" + '%' */
        CHECK(g[0].glyph == YK_GAUGE_GLYPH_MINUS && g[7].glyph == YK_GAUGE_GLYPH_PERCENT);
        /* 越界值按 ±10000 夹（引擎给脏值时不会算到数组外）。*/
        CHECK(yk_gauge_text_glyphs(999999, g) == 7);
        CHECK(yk_gauge_text_glyphs(-999999, g) == YK_GAUGE_TEXT_MAX);
        /* NULL 出口：只算位数、不崩。*/
        CHECK(yk_gauge_text_glyphs(0, 0) == 5);
        CHECK(yk_gauge_text_glyphs(12345, 0) == 7);
        /* 全定义域：返回 1..MAX 且 dx 单调、字形号在域内。*/
        for (i = -11000; i <= 11000; i += 137) {
            int k = yk_gauge_text_glyphs(i, g);
            int j;
            CHECK(k >= 1 && k <= YK_GAUGE_TEXT_MAX);
            for (j = 0; j < k; ++j) {
                CHECK(g[j].glyph <= YK_GAUGE_GLYPH_MINUS);
                CHECK(g[j].scale == 1.0f || g[j].scale == 0.8f);
                if (j) CHECK(g[j].dx > g[j - 1].dx);
            }
            CHECK(g[k - 1].glyph == YK_GAUGE_GLYPH_PERCENT);
        }
    }

    /* ============ §3.11 永点（刻符）→ PIV（一手 `th08:0x418220`）============ */
    {
        int32_t piv, orbs;
        /* 逐颗：+10 / 0 / +10 / 0 …（总数为奇数那颗 +10）⇒ 平均 +5/颗。*/
        yk_piv_reset(&piv, &orbs);
        CHECK(piv == 0 && orbs == 0);
        yk_piv_add(&piv, &orbs, 1);  CHECK(orbs == 1 && piv == 10);
        yk_piv_add(&piv, &orbs, 1);  CHECK(orbs == 2 && piv == 10);
        yk_piv_add(&piv, &orbs, 1);  CHECK(orbs == 3 && piv == 20);
        yk_piv_add(&piv, &orbs, 1);  CHECK(orbs == 4 && piv == 20);
        /* 一次给 2 颗：total = 6（偶）⇒ (2+0)/2×10 = 10。*/
        yk_piv_add(&piv, &orbs, 2);  CHECK(orbs == 6 && piv == 30);
        /* **负数不动 PIV**（`th08:0x418288`/`:0x41828c jle`），只动计数。*/
        yk_piv_add(&piv, &orbs, -15);
        CHECK(orbs == -9 && piv == 30);
        /* 再收一颗：total = −8（偶）⇒ (1+0)/2 = 0 ⇒ 不加。*/
        yk_piv_add(&piv, &orbs, 1);  CHECK(orbs == -8 && piv == 30);
        yk_piv_add(&piv, &orbs, 1);  CHECK(orbs == -7 && piv == 40);  /* total 奇 ⇒ +10 */
        /* SC 捕获那份额外账：`count × 10`，独立加（`th08:0x41732c`）。*/
        yk_piv_spell_bonus(&piv, 7); CHECK(piv == 110);
        yk_piv_spell_bonus(&piv, 0); CHECK(piv == 110);               /* 非正不加 */
        yk_piv_spell_bonus(&piv, -3);CHECK(piv == 110);
        /* ★★★ 2026-09-27（§BE #137，owner：「永点吃了之后没有加刻符，左下角没显示额外最大得点」）：
         * **永点 → 共享「额外最大得点」池**的步进（池是累加语义 ⇒ 每帧只补差额）。
         * 目标量 = `piv − piv_base`（本卡那笔；`piv_base` = 难度初值 6W/10W/20W/30W/30W）：
         *   · 开局（extra = 0）⇒ 补 0（池初值 0 ✓「初值是 0」这条用户口径）；
         *   · 吃 2 颗永点 ⇒ extra = 10 ⇒ 补 10（左下角那个数跟着 +10）；
         *   · 追平之后 ⇒ 每帧补 0（不会重复加）； */
        CHECK(yk_piv_pool_step(0, 0) == 0);
        CHECK(yk_piv_pool_step(-5, 0) == 0);            /* 负数目标（不该出现，防御）⇒ 不写 */
        CHECK(yk_piv_pool_step(10, 0) == 10);           /* 第一笔：整笔补 */
        CHECK(yk_piv_pool_step(10, 10) == 0);           /* 追平：不再加（每帧重申也幂等）*/
        CHECK(yk_piv_pool_step(470, 400) == 70);        /* 后续增量：只补差额 */
        CHECK(yk_piv_pool_step(400, 470) == 0);         /* 已写**超过**目标 ⇒ 不倒扣（池只涨）*/
        CHECK(yk_piv_pool_step(470, -3) == 470);        /* 已写账被清（新局 `ce_piv_reset`）⇒ 重新补 */
        CHECK(YK_PIV_POOL_CAP == 0x7ffffffe);           /* 一手 PIV 无上限 ⇒ int32 安全上界 */
        /* 刻符的道具分值（`th08:0x4412c3`–`0x4412ee`）：本关蓝点 ≥ 2000 ⇒ 满值 10000。*/
        CHECK(yk_orb_score(2000, 0) == 10000);
        CHECK(yk_orb_score(5000, 10) == 10000);
        CHECK(yk_orb_score(0, 0) == 100);            /* (0/2)*10 = 0 ⇒ 下限 100 */
        CHECK(yk_orb_score(0, 100) == 500);          /* (100/2)*10 */
        CHECK(yk_orb_score(1999, 4) == 100);         /* 没到门槛 + 值太小 ⇒ 下限 */
        CHECK(yk_orb_score(0, 20000) == 100000);     /* 门槛按**本关**算，本局计数照给 */

        /* ★★ 2026-09-22 晚（owner 第 4 条）+ ★★★ 2026-09-22（P5 #16b）：刻符的**运动常量**。
         * 归航速度 = `[.sht+0x14]`（TH08 一手 10/12，**未改**）；
         * 拾取半径 = **TH18 自己那一档 70**（`th18:0x45ae7b`/`:0x45ae85`；原生 22 号卡 ITEM_LINE = 110）
         * —— owner 2026-09-22 拍的 **TH18 适配**（要「全屏吸收那种速度感」），
         * **不是** TH08 一手值（一手 = `[.sht+0x18]/2 + 8` = 20/24，见 `youkai_core.c` 的注）。*/
        {
            float sp = 0.0f, r = 0.0f;
            int c;
            yk_orb_motion_params(0, &sp, &r);
            CHECK(sp == 10.0f && r == 20.0f);                     /* pl00as：10.0 / 24/2+8（一手）*/
            yk_orb_motion_params(1, &sp, &r);
            CHECK(sp == 12.0f && r == 24.0f);                     /* pl01as：12.0 / 32/2+8（一手）*/
            yk_orb_motion_params(2, &sp, &r);
            CHECK(sp == 10.0f && r == 20.0f);                     /* pl02as */
            yk_orb_motion_params(3, &sp, &r);
            CHECK(sp == 10.0f && r == 20.0f);                     /* pl03as */
            yk_orb_motion_params(-1, &sp, &r);                    /* 越界 ⇒ 0 号那一档 */
            CHECK(sp == 10.0f && r == 20.0f);
            for (c = -2; c <= 6; ++c) {                           /* 全定义域不崩、值恒在 {10,12}/{70} */
                yk_orb_motion_params(c, &sp, &r);
                CHECK((sp == 10.0f || sp == 12.0f) && (r == 20.0f || r == 24.0f));
            }
            sp = 7.0f; r = 7.0f;
            yk_orb_motion_params(0, &sp, 0);                      /* 单出口 */
            CHECK(sp == 10.0f && r == 7.0f);
            yk_orb_motion_params(0, 0, &r);
            CHECK(r == 20.0f);
            yk_orb_motion_params(0, 0, 0);                        /* 双 NULL：什么都不做、不崩 */
            /* ★★★ 2026-09-26（#41「从敌机铺出去的范围太大」）：**击破批的落点算式**
             * 一手（`th08:0x42aff0`–`0x42b048`）= 圆盘内均匀撒：
             *   `angle = rand_signed(π) ∈ [−π, π)`、`radius = rand(2·count) ∈ [0, 2·count)`。
             * 旧实现是**固定半径 128 的环** ⇒ 12 颗时是圆盘上界（24）的 5.3 倍。*/
            {
                float a = 9.0f, rad = 9.0f;
                const float PI = 3.14159265f;
                yk_orb_burst_polar(12, 0.0f, 0.0f, &a, &rad);     /* r=0 ⇒ 角 = −π、半径 0 */
                CHECK(a == -PI && rad == 0.0f);
                yk_orb_burst_polar(12, 0.5f, 0.5f, &a, &rad);     /* 中值 ⇒ 角 0、半径 = count */
                CHECK(a == 0.0f && rad == 12.0f);
                yk_orb_burst_polar(12, 1.0f, 1.0f, &a, &rad);     /* 上界 ⇒ 角 π、半径 2·count */
                CHECK(a == PI && rad == 24.0f);
                yk_orb_burst_polar(12, 0.25f, 0.1f, &a, &rad);    /* 角 = −π/2（正下方）*/
                CHECK(rad == 0.1f * 24.0f);
                CHECK(a < 0.0f && a > -PI);                       /* ★ 恒在 [−π, π) 内 */
                yk_orb_burst_polar(0, 0.9f, 1.0f, &a, &rad);      /* count = 0 ⇒ 半径 0（不崩）*/
                CHECK(rad == 0.0f);
                yk_orb_burst_polar(-3, 0.9f, 1.0f, &a, &rad);     /* 负 count 也夹到 0 */
                CHECK(rad == 0.0f);
                yk_orb_burst_polar(12, 0.3f, 0.7f, &a, 0);        /* 单出口 */
                CHECK(a < 0.0f && a > -PI);
                yk_orb_burst_polar(12, 0.3f, 0.7f, 0, &rad);
                CHECK(rad > 0.0f);
                yk_orb_burst_polar(12, 0.3f, 0.7f, 0, 0);         /* 双 NULL：不崩 */
                for (a = 0.0f; a < 1.0f; a += 0.125f) {           /* 半径上界恒 = 2·count */
                    float t = 0.0f;
                    yk_orb_burst_polar(26, 0.5f, a, 0, &t);
                    CHECK(t >= 0.0f && t < 52.0f);
                }
            }
            /* ★★★ 2026-09-22（#25 击破符卡的螺旋）：一手参数（`engine/item/th08/01-time-orbs.md` §5.3）
             * 角度闭式 `θ(t) = ((t − 10) × 2π/40) − π/2`：`t = 10` 正上方（−π/2）、每 +1 帧 +9°（2π/40）。*/
            {
                float a = 0.0f;
                const float D = 0.15707963268f;            /* 2π/40 = 9° */
                yk_spiral_angle(10, &a);
                CHECK(a > -1.5708f - 1e-4f && a < -1.5708f + 1e-4f);      /* t = 10 ⇒ 正上方 */
                yk_spiral_angle(11, &a);
                CHECK(a > (-1.5708f + D) - 1e-3f && a < (-1.5708f + D) + 1e-3f);
                yk_spiral_angle(50, &a);                                   /* 40 帧一圈 ⇒ 回到正上方 */
                CHECK(a > -1.5708f - 1e-3f && a < -1.5708f + 1e-3f);
                yk_spiral_angle(0, 0);                                     /* NULL 出口不崩 */
                /* ★★★ 2026-09-27（第三十九批 · 逐指令复核订正）：`push 8` **不是批次周期**，
                 *   是 `Timer::current_is_gt` 的一次性门槛「帧龄 > 8」⇒ 螺旋**每帧都喷**。*/
                CHECK(YK_SPIRAL_START_AGE == 8 && YK_SPIRAL_FIRST_MAX == 7 &&
                      YK_SPIRAL_SECOND_N == 6 && YK_SPIRAL_RADIUS == 128.0f);
            /* ★★★ 2026-09-27（第四十三批 · owner「螺旋改成画面中心」）：锚点 = 游玩区正中 (0, 224)
             *   （TH18 的 ECL 坐标；一手那条绕的是符卡圈 effect，我们按 owner 口径取画面中心）。*/
            CHECK(YK_SPIRAL_CENTER_X == 0.0f && YK_SPIRAL_CENTER_Y == 224.0f);
            /* ★★★ 第四十三批（owner「这个的吸收过程得可见的」）：**只有極人**那一档
             *   「上抛结束直接吃掉」（= 不走飞向自机那段）；極妖（擦弹/螺旋常在那一档）必须**可见**。*/
            CHECK(yk_orb_eat_at_toss(-10000) == 1);      /* 極人（含等号，`setle`）*/
            CHECK(yk_orb_eat_at_toss(-8000) == 1);
            CHECK(yk_orb_eat_at_toss(-7999) == 0);
            CHECK(yk_orb_eat_at_toss(0) == 0);
            CHECK(yk_orb_eat_at_toss(8000) == 0);        /* ★ 極妖 ⇒ **不吃**（吸收要看得见）*/
            CHECK(yk_orb_eat_at_toss(10000) == 0);
                CHECK(yk_spiral_first_count(1000) == 7);                   /* min(额度, 7) */
                CHECK(yk_spiral_first_count(7) == 7);
                CHECK(yk_spiral_first_count(3) == 3);
                CHECK(yk_spiral_first_count(0) == 0 && yk_spiral_first_count(-5) == 0);
                /* ★★★ 2026-09-27（第三十八批 · owner：「扩永点池到 ~1e2，至少把击破 boss 的螺旋
                 * 渲染出来」）：**在飞峰值**算式 —— 卡侧的 `YK_ORB_MAX` = 128 就是按它定的。
                 * `per_batch` = 7 + 6 = 13、`batch_frames` = 8、`life_frames` = 65（❓自定估算）。*/
                /* ★★★ 订正后的节奏：**每帧一批**（`batch_frames = 1`）⇒ 13 颗/帧 × 在飞 75 帧 */
                CHECK(yk_spiral_live_peak(13, 1, 75) == 988);          /* 13×75 + 13（池 1024 够了）*/
                CHECK(yk_spiral_live_peak(13, 1, 65) == 858);          /* 旧寿命估算下的下界 */
                CHECK(yk_spiral_live_peak(1, 1, 75) == 76);
                CHECK(yk_spiral_live_peak(13, 8, 75) == 135);          /* 旧读法（每 8 帧一批）对照 */
                CHECK(yk_spiral_live_peak(0, 1, 75) == 0);             /* 防御：非正入参 ⇒ 0 */
                CHECK(yk_spiral_live_peak(13, 0, 75) == 0);
                CHECK(yk_spiral_live_peak(13, 1, 0) == 0);
                CHECK(YK_SPIRAL_LIVE_LIFE_FRAMES == 75);
                /* ★★★ 一帧的吐出与账面（一手 `0x4175cc`–`0x4176e1`）：第二段**固定 6**（与额度无关）
                 *   但**账面扣 `min(额度,7)` 两次** ⇒ 额度 ≥14 时每帧扣 14、吐 13。*/
                {
                    int f = -1, sc = -1;
                    CHECK(yk_spiral_quota_step(1000, &f, &sc) == 986 && f == 7 && sc == 6);
                    CHECK(yk_spiral_quota_step(14, &f, &sc) == 0 && f == 7 && sc == 6);
                    CHECK(yk_spiral_quota_step(13, &f, &sc) == 0 && f == 7 && sc == 6);  /* 扣 7 + 扣 6 */
                    CHECK(yk_spiral_quota_step(9, &f, &sc) == 0 && f == 7 && sc == 6);   /* 扣 7 + 扣 2 */
                    CHECK(yk_spiral_quota_step(5, &f, &sc) == 0 && f == 5 && sc == 6);   /* ★ 额度 5 也吐 6 */
                    CHECK(yk_spiral_quota_step(1, &f, &sc) == 0 && f == 1 && sc == 6);
                    CHECK(yk_spiral_quota_step(0, &f, &sc) == 0 && f == 0 && sc == 0);
                    CHECK(yk_spiral_quota_step(-3, &f, &sc) == 0 && f == 0 && sc == 0);
                    /* ★ 总额度 1000 的时长（一手：72 帧 / ~935 颗）—— 用账面扣减直接数一遍。*/
                    {
                        int q = 1000, frames = 0, orbs = 0;
                        while (q > 0 && frames < 1000) {
                            int a = 0, b = 0;
                            q = yk_spiral_quota_step(q, &a, &b);
                            orbs += a + b;
                            ++frames;
                        }
                        CHECK(frames == 72);          /* ⇒ 1.20 s @60fps */
                        CHECK(orbs == 935);           /* 一手复算：71×13 + 12 */
                    }
                }
                /* ★ 池必须装得下螺旋：`YK_ORB_MAX`(**1024**) ≥ 订正后的峰值(**988**) —— 这条把
                 *   「池够不够」钉成回归：改小池、或把「每帧一批」误改回批次制，都会在这里红。
                 *   （池的演进：64 → 128 → 256（owner「扩到 ~1e2」）→ **1024**（订正成每帧 13 颗之后
                 *   实算 988；256 那版会把螺旋削掉七成以上）。）*/
                CHECK(yk_spiral_live_peak(YK_SPIRAL_FIRST_MAX + YK_SPIRAL_SECOND_N,
                                          1,                             /* ★ 每帧一批 */
                                          YK_SPIRAL_LIVE_LIFE_FRAMES) <= 1024);
            }
            /* 一手的状态号与常量（改了要在注释里同步改一手出处）*/
            CHECK(YK_ORB_STATE_FALL == 0 && YK_ORB_STATE_HOMING == 1 && YK_ORB_STATE_SPAWN == 3);
            CHECK(YK_ORB_FALL_GRAVITY == 0.03f);                  /* `ds:0x4b44c0` */
            CHECK(YK_ORB_FALL_TERMINAL == 3.0f);                  /* `0x40400000` */
            CHECK(YK_ORB_FALL_VY_MIN == -2.2f);                   /* `ds:0x4b5b2c` */
            CHECK(YK_ORB_LOST_Y == 464.0f);                       /* `arcadeRegionSize.y (448) + 16` */
            /* ★★★ 2026-09-22（owner #26 · 风味选项）：决死窗口 + 染色的两条不变量。
             * 判据与一手见 `youkai_core.h` 的同名小节（窗口 = `player+0x476ac == 4`，
             * 一手 `th18:0x45d4e4` 全 exe 唯一写点）。*/
            CHECK(YK_DEATHBOMB_STATE == 4);
            CHECK(yk_deathbomb_window(4) == 1);
            CHECK(yk_deathbomb_window(0) == 0 && yk_deathbomb_window(1) == 0 &&
                  yk_deathbomb_window(3) == 0 && yk_deathbomb_window(5) == 0);
            CHECK(yk_deathbomb_window(-1) == 0);
            /* 染色的两条不变量：只换 RGB + alpha 原样；还原 = 当前 alpha + 快照 RGB。*/
            CHECK(YK_TINT_RED == 0xffff0000u);
            CHECK(yk_tint_apply(0xffffffffu) == 0xffff0000u);   /* 纯红 */
            CHECK(yk_tint_apply(0x80aabbccu) == 0x80ff0000u);        /* alpha 0x80 原样保留 */
            CHECK(yk_tint_apply(0x00112233u) == 0x00ff0000u);
            CHECK(yk_tint_restore(0x40ff0000u, 0x80aabbccu) == 0x40aabbccu);  /* 取当前 alpha */
            CHECK(yk_tint_restore(0xffffffffu, 0x80aabbccu) == 0xffaabbccu);
            /* 染了再还原 = 回到原色（除 alpha 用的是「当前」那一份 —— 引擎每帧会重设颜色）。*/
            CHECK(yk_tint_restore(yk_tint_apply(0xffffffffu), 0xffffffffu) == 0xffffffffu);
            /* ★★★ 2026-09-22 夜（owner 工单 P-A · 夜点）：初值四档 + 蓝点分（TH08 一手算式）。
             * 一手：`th08:0x43af1b` 的 switch（60000/100000/200000/300000/300000）；
             *       `Item::CollectPoint`（PoC 线以上 = 满值；以下 = 夜点/2 − 距离×(夜点/1000)，十位取整）。*/
            CHECK(YK_NIGHT_POINT_GRADES == 5);
            CHECK(yk_night_point_init(0) == 60000);
            CHECK(yk_night_point_init(1) == 100000);
            CHECK(yk_night_point_init(2) == 200000);
            CHECK(yk_night_point_init(3) == 300000);
            CHECK(yk_night_point_init(4) == 300000);          /* Extra（`th08:0x43af85`）*/
            CHECK(yk_night_point_init(-1) == 300000 && yk_night_point_init(99) == 300000);  /* 越界取最高档 */
            /* 蓝点分：线以上 / isMaxValue ⇒ 满值（十位取整不改它，因为四档初值都是 10 的倍数）*/
            CHECK(yk_blue_point_score(100000, 0, 0) == 100000);
            CHECK(yk_blue_point_score(100000, -5, 0) == 100000);
            CHECK(yk_blue_point_score(100000, 123, 1) == 100000);
            /* 线以下：100000/2 − 123×(100000/1000) = 50000 − 12300 = 37700（十位取整后同值）*/
            CHECK(yk_blue_point_score(100000, 123, 0) == 37700);
            /* 距离大到把值压成负 ⇒ 我们夹 0（一手没有夹零，登记 ❓ 自定）*/
            CHECK(yk_blue_point_score(100000, 100000, 0) == 0);
            CHECK(yk_blue_point_score(0, 10, 0) == 0);
            /* 十位取整：60000/2 − 7×(60000/1000) = 30000 − 420 = 29580（已是 10 的倍数）*/
            CHECK(yk_blue_point_score(60000, 7, 0) == 29580);
            /* ★★★ 2026-09-22 夜（P-B 逢魔）：门（两条都含等号、取或）+ 击破掉几颗刻符
             * 一手：`th08:0x406d10`（极人 `g <= −8000`，`setle`）/ `th08:0x406d70`（极妖 `g >= +8000`，`setge`）；
             *       `th08:0x42da41`（逢魔 ⇒ 1 颗）；`th08:0x45187f`（人间逢魔 + 自机弹击杀 ⇒ 再 1 颗）。*/
            CHECK(YK_FENGMA_SCORE_PER_FRAME == 100);
            CHECK(yk_fengma(-8000, YK_GAUGE_HUMAN_EFFECTS, YK_GAUGE_YOUKAI_EFFECTS) == 1);   /* 含等号 */
            CHECK(yk_fengma(8000, YK_GAUGE_HUMAN_EFFECTS, YK_GAUGE_YOUKAI_EFFECTS) == 1);
            CHECK(yk_fengma(-8001, YK_GAUGE_HUMAN_EFFECTS, YK_GAUGE_YOUKAI_EFFECTS) == 1);   /* 更极端也算 */
            CHECK(yk_fengma(-7999, YK_GAUGE_HUMAN_EFFECTS, YK_GAUGE_YOUKAI_EFFECTS) == 0);
            CHECK(yk_fengma(7999, YK_GAUGE_HUMAN_EFFECTS, YK_GAUGE_YOUKAI_EFFECTS) == 0);
            CHECK(yk_fengma(0, YK_GAUGE_HUMAN_EFFECTS, YK_GAUGE_YOUKAI_EFFECTS) == 0);
            /* 妖梦人型那种「人侧只要 −3000」的阈值也走同一个函数（阈值是参数）*/
            CHECK(yk_fengma(-3000, -3000, 8000) == 1 && yk_fengma(-2999, -3000, 8000) == 0);
            /* 击破掉几颗：非逢魔 0；逢魔 **恒 1** —— ★ 2026-09-27（第三十七批）撤掉了
             * 「人间逢魔 + 自机弹击杀 ⇒ 2」那条近似：一手那 +1 属**另一条来源**
             * （極人期间**每一发自机弹命中**），现在按真颗粒度接在 `on_bullet_hit` 上。*/
            CHECK(yk_fengma_kill_orbs(0, 1, 1) == 0);
            CHECK(yk_fengma_kill_orbs(1, 0, 0) == 1);
            CHECK(yk_fengma_kill_orbs(1, 0, 1) == 1);
            CHECK(yk_fengma_kill_orbs(1, 1, 0) == 1);
            CHECK(yk_fengma_kill_orbs(1, 1, 1) == 1);      /* ★ 从 2 改成 1（不再重复计数）*/
            /* ★★★ 2026-09-27（第三十七批）：**極人 + 一发自机弹命中** ⇒ 1 颗（一手
             * `th08:0x451860`–`:0x451892`）。判据 = 槽的極人档 + 「是自机弹」两条，**没有低速位**。*/
            CHECK(yk_human_fengma_bullet_orbs(-8000, 1) == 1);    /* 極人（含等号，`setle`）*/
            CHECK(yk_human_fengma_bullet_orbs(-10000, 1) == 1);
            CHECK(yk_human_fengma_bullet_orbs(-7999, 1) == 0);    /* 差 1 就不算極人 */
            CHECK(yk_human_fengma_bullet_orbs(0, 1) == 0);
            CHECK(yk_human_fengma_bullet_orbs(10000, 1) == 0);    /* 極妖：这 +1 只属極人 */
            CHECK(yk_human_fengma_bullet_orbs(-10000, 0) == 0);   /* 不是自机弹（卡侧伤害源/原生卡）*/
            /* ★★★ 2026-09-27（第三十九批 · owner：「魔理沙人激光的永点生成位置明显偏离 hit 位置」）：
             * 落点判据 —— 源槽中心离敌机 > 64 px ⇒ 认为那是**长光源**（激光/束，中心在膛口）
             * ⇒ 落点改用**敌机位置**。普通弹（两者差 < 30 px）一律保持「撒在弹上」的一手口径。*/
            CHECK(YK_HIT_ORB_FAR_DIST == 64.0f);
            CHECK(yk_hit_orb_use_enemy_pos(0.0f, 0.0f) == 0);      /* 正中命中：用源中心 */
            CHECK(yk_hit_orb_use_enemy_pos(20.0f, 0.0f) == 0);     /* 普通弹那一档 */
            CHECK(yk_hit_orb_use_enemy_pos(0.0f, -30.0f) == 0);
            CHECK(yk_hit_orb_use_enemy_pos(63.9f, 0.0f) == 0);     /* 阈值内 */
            CHECK(yk_hit_orb_use_enemy_pos(64.1f, 0.0f) == 1);     /* 越过阈值 ⇒ 换敌机位置 */
            CHECK(yk_hit_orb_use_enemy_pos(200.0f, 0.0f) == 1);    /* 激光：膛口离敌机几百 px */
            CHECK(yk_hit_orb_use_enemy_pos(50.0f, 50.0f) == 1);    /* 斜向：50√2 ≈ 70.7 > 64 */
            CHECK(yk_hit_orb_use_enemy_pos(-300.0f, 40.0f) == 1);
            /* ★★★ 2026-09-22 夜（P-E 单人反向封顶）：一手 `Player::on_registration`
             * —— 单人（`th08:0x44da99` / `:0x44dac4` 的两个门）把**反侧 limit** 收成 ±2000
             * （`th08:0x44daa2` = +0x7d0 / `th08:0x44dacd` = −0xf830），effects 不动 ⇒ 反侧逢魔不可达。*/
            {
                int32_t hl = 0, yl = 0;
                yk_gauge_limits_solo(1, &hl, &yl);            /* 妖侧（低速）⇒ 人侧封在 −2000 */
                CHECK(hl == -YK_GAUGE_SOLO_OTHER_LIMIT && hl == -2000);
                CHECK(yl == YK_GAUGE_YOUKAI_LIMIT);
                yk_gauge_limits_solo(0, &hl, &yl);            /* 人侧 ⇒ 妖侧封在 +2000 */
                CHECK(hl == YK_GAUGE_HUMAN_LIMIT);
                CHECK(yl == YK_GAUGE_SOLO_OTHER_LIMIT && yl == 2000);
                yk_gauge_limits_solo(1, 0, 0);                /* NULL 安全 */
                /* 反侧真的到不了逢魔线：妖侧形态下，往人侧推到底也只能到 −2000 */
                yk_gauge_limits_solo(1, &hl, &yl);
                CHECK(yk_gauge_add2(-1900, -200, hl, yl) == -2000);       /* 夹在反侧 limit */
                CHECK(yk_gauge_add2(-2000, -100, hl, yl) == -2000);
                CHECK(yk_fengma(yk_gauge_add2(-2000, -100, hl, yl),
                                YK_GAUGE_HUMAN_EFFECTS, YK_GAUGE_YOUKAI_EFFECTS) == 0);  /* 反侧逢魔不可达 */
                CHECK(yk_gauge_add2(9900, 500, hl, yl) == 10000);         /* 正侧仍到 100% */
                CHECK(yk_fengma(yk_gauge_add2(7900, 200, hl, yl),
                                YK_GAUGE_HUMAN_EFFECTS, YK_GAUGE_YOUKAI_EFFECTS) == 1);  /* 正侧可逢魔 */
                /* `clamp2` 与旧的 `clamp` 在满量程时逐点一致（回归保护）*/
                CHECK(yk_gauge_clamp2(-99999, YK_GAUGE_HUMAN_LIMIT, YK_GAUGE_YOUKAI_LIMIT) ==
                      yk_gauge_clamp(-99999));
                CHECK(yk_gauge_clamp2(99999, YK_GAUGE_HUMAN_LIMIT, YK_GAUGE_YOUKAI_LIMIT) ==
                      yk_gauge_clamp(99999));
                CHECK(yk_gauge_clamp2(1234, YK_GAUGE_HUMAN_LIMIT, YK_GAUGE_YOUKAI_LIMIT) == 1234);
                /* ★★★ 2026-09-27（owner ③「按/松 shift 不得让人妖值瞬间变化」）：
                 * **夹取不许回收已经在区间外的值** —— 换形那一帧限值本身换了，若照旧直接夹，
                 * 停在 −5000 的槽会被当帧拉成 −2000（+30% 的瞬跳）；一手没有任何一处这么做。*/
                yk_gauge_limits_solo(1, &hl, &yl);        /* 妖侧：区间 = [−2000, +10000] */
                CHECK(yk_gauge_add2(-5000, 0, hl, yl) == -5000);          /* ★ 换形当帧：一点不动 */
                CHECK(yk_gauge_add2(-5000, +20, hl, yl) == -4980);        /* 只按 delta 走，不弹回 −2000 */
                CHECK(yk_gauge_add2(-1900, -200, hl, yl) == -2000);       /* 区间内照旧夹（回归）*/
                CHECK(yk_gauge_add2(-2100, -200, hl, yl) == -2300);       /* 区间外 ⇒ 不回收（同上）*/
                yk_gauge_limits_solo(0, &hl, &yl);        /* 人侧：区间 = [−10000, +2000] */
                CHECK(yk_gauge_add2(5000, 0, hl, yl) == 5000);            /* ★ 松 Shift 也不瞬跳 */
                CHECK(yk_gauge_add2(5000, -20, hl, yl) == 4980);
                CHECK(yk_gauge_add2(1900, 200, hl, yl) == 2000);          /* 区间内照旧夹 */
                /* 换形空窗常量（一手 `th08:0x44be05` 的 `cmp …,0x1e`）*/
                CHECK(YK_GAUGE_SHIFT_BLANK == 30);
            }
            /* ★★★ 2026-09-27（owner ⑥）：**我们自己那条激光行**的判据。
             * ★ 第二十六批起它**不再用于收束**（引擎不发这一行了，见下）—— 留着当**运行期判据**：
             *   探针用它数「这一行的引擎弹有几发」，必须恒 0。*/
            CHECK(yk_marisa_laser_row(0x59, 0) == 1);
            CHECK(yk_marisa_laser_row(0x5d, 0) == 1);
            CHECK(yk_marisa_laser_row(0x59, 1) == 0);      /* 组内第 1 行 = 本体行（不是激光）*/
            CHECK(yk_marisa_laser_row(0x58, 0) == 0);      /* 霊夢的组：不是我们的激光 */
            CHECK(yk_marisa_laser_row(0x5e, 0) == 0);      /* 咲夜 */
            CHECK(yk_marisa_laser_row(0x63, 0) == 0);      /* 早苗 */
            CHECK(yk_marisa_laser_row(0x16, 0) == 0);      /* 兜底组 */
            /* ★ 第三十一批：**不再照抄一手的 448**（owner：「th08 长 448 能保证激光射出画面 …
             *   你有必要完全复刻吗？」+「两边坐标系统又不一样」）⇒ 取 117 的口径 2048
             *   （`ANIMAL_EAGLE_BEAM_LEN = 512 × 4.0`）；一手值 448 的出处 = `th08:0x450a4f`。*/
            CHECK(YK_LASER_LEN == 2048.0f);
            CHECK(YK_CHAR_MARISA == 1);                    /* 机体表（`CE_ADDR_CHARACTER` 一手）*/
            /* ★★★ 2026-09-27 第二十六批：**卡侧矩形束**的四个纯函数（一手见 `youkai_core.h`）。
             * 宽度 = 一手 TH08 `player01.anm` script24..28 的 `scale.y` × 贴图高 32
             *   （16 / 16 / 19.2 / 22.4 / 25.6 px；×100 存整数避免 float 表）。*/
            CHECK(yk_m_laser_beam_w_px100(0) == 1600);
            CHECK(yk_m_laser_beam_w_px100(1) == 1600);
            CHECK(yk_m_laser_beam_w_px100(2) == 1920);
            CHECK(yk_m_laser_beam_w_px100(3) == 2240);
            CHECK(yk_m_laser_beam_w_px100(4) == 2560);
            CHECK(yk_m_laser_beam_w_px100(-1) == 1600);           /* 越界钳进表内（不读表外）*/
            CHECK(yk_m_laser_beam_w_px100(99) == 2560);
            /* 伤害 = `round(2 × 该档对齐后的激光行伤害 / 该行的 rate)`（= 本卡既有 DPS 模型里那一行的份额 × 2）
             * ⇒ 平均每帧 = 7.0 / 4.5 / 3.5 / 3.0 / 2.5（生成器逐档算 + 回读对账，见 `append_shooterset.py`）。*/
            CHECK(yk_m_laser_beam_damage(0) == 14);
            CHECK(yk_m_laser_beam_damage(1) == 9);
            CHECK(yk_m_laser_beam_damage(2) == 7);
            CHECK(yk_m_laser_beam_damage(3) == 6);
            CHECK(yk_m_laser_beam_damage(4) == 5);
            CHECK(yk_m_laser_beam_damage(-5) == 14);
            CHECK(yk_m_laser_beam_damage(5) == 5);
            /* 结算节拍 = 每 2 帧一笔（一手 TH08 `th08:0x4517a1` 每 2 帧结算一次）*/
            CHECK(YK_M_LASER_BEAM_SETTLE == 2);
            CHECK(yk_m_laser_beam_should_settle(0) == 1);         /* 出生那一帧就结算（age 从 0 起）*/
            CHECK(yk_m_laser_beam_should_settle(1) == 0);
            CHECK(yk_m_laser_beam_should_settle(2) == 1);
            CHECK(yk_m_laser_beam_should_settle(3) == 0);
            CHECK(yk_m_laser_beam_should_settle(120) == 1);
            /* 展开系数：头 8 帧 0→1000 线性，之后恒 1000（117 的「如收回般倒放展开」，
             * 帧数 = 束脚本 `interruptLabel(1)` 的 `scaleTime(8, 1, 1.0, 0.0)`）。*/
            CHECK(YK_M_LASER_BEAM_GROW_FRAMES == 8);
            CHECK(yk_m_laser_beam_grow_permille(0) == 0);
            CHECK(yk_m_laser_beam_grow_permille(1) == 125);
            CHECK(yk_m_laser_beam_grow_permille(4) == 500);
            CHECK(yk_m_laser_beam_grow_permille(7) == 875);
            CHECK(yk_m_laser_beam_grow_permille(8) == 1000);
            CHECK(yk_m_laser_beam_grow_permille(9999) == 1000);   /* 不溢出、不回头 */
            /* 束的条数上限 = 每台我们的子机一条 + 回退本体的那一条（117 的同款形状）*/
            CHECK(YK_M_LASER_BEAM_UNITS == YK_OPTION_CELLS + 1);
            /* 贴图逻辑尺寸（★ 第三十三批：`YKB_M_OPT.png` = **768×32** —— 那条 256 texel 源行
             * 原样 ×3，**铺满 entry20 的整条行宽**；旧值 512 = 镜像平铺 —— 镜像那一圈压根不是
             * UV 绕回的那一圈，`scrollX` 的绕回按 **entry 宽（768）** 取模 ⇒ 会把 MAIN/HIT/透明带
             * 扫进束里（owner 报的「一缕一缕/脉冲」）。完整链条见 `youkai_core.h` 的同名长注）。*/
            CHECK(YK_M_LASER_BEAM_SPRITE_W == 768.0f);
            CHECK(YK_M_LASER_BEAM_SPRITE_H == 32.0f);
            /* 满长系数 = 2048/768（脚本初值那一行只写 y，x 每帧由卡侧写）*/
            CHECK(YK_LASER_LEN / YK_M_LASER_BEAM_SPRITE_W > 2.66f);
            CHECK(YK_LASER_LEN / YK_M_LASER_BEAM_SPRITE_W < 2.67f);

            /* ★★★ 2026-09-27 第二十七批：**残影多束**的五个纯函数（一手逐指令见 `youkai_core.h`）。
             * 一手三支回调：init `th08:0x44fe20` / tick `th08:0x450840` / draw `th08:0x450ad0`。*/
            /* ① 条数 = 一手行里的 `+0x00`（`0x44ff2b` 搬进 `vm+0x46a`）= ply01as 的 2/4/6/8/12。*/
            CHECK(yk_m_laser_beam_rate(0) == 2);
            CHECK(yk_m_laser_beam_rate(1) == 4);
            CHECK(yk_m_laser_beam_rate(2) == 6);
            CHECK(yk_m_laser_beam_rate(3) == 8);
            CHECK(yk_m_laser_beam_rate(4) == 12);
            CHECK(yk_m_laser_beam_rate(-1) == 2);        /* 越界钳进表内 */
            CHECK(yk_m_laser_beam_rate(99) == 12);
            /* 数组第二维必须容得下逐档最大条数（否则 `s_beam_vm[u][k]` 越界）*/
            CHECK(YK_M_LASER_BEAM_BARS_MAX == 12);
            CHECK(YK_M_LASER_BEAM_BARS_MAX <= YK_M_LASER_BEAM_TRAIL_MAX);
            {
                int t, mx = 0;
                for (t = 0; t < 5; ++t) if (yk_m_laser_beam_rate(t) > mx) mx = yk_m_laser_beam_rate(t);
                CHECK(mx == YK_M_LASER_BEAM_BARS_MAX);   /* 表里的最大值 == 上限常数 */
            }
            /* ② 轨迹常量：32 格、每步 y −1、哨兵 −999、门 −900（一手 `0x4b4c7c`/`0x4b6fe0`/`0x4b4338`）*/
            CHECK(YK_M_LASER_BEAM_TRAIL_MAX == 32);
            CHECK(YK_M_LASER_BEAM_TRAIL_DY == 1.0f);
            CHECK(YK_M_LASER_BEAM_TRAIL_NONE == -999.0f);
            CHECK(YK_M_LASER_BEAM_TRAIL_MIN == -900.0f);
            CHECK(yk_m_laser_beam_bar_live(0.0f) == 1);
            CHECK(yk_m_laser_beam_bar_live(-900.0f) == 1);        /* 门是 `>=`（一手 `fcomp` 只跳「小于」）*/
            CHECK(yk_m_laser_beam_bar_live(-900.5f) == 0);
            CHECK(yk_m_laser_beam_bar_live(YK_M_LASER_BEAM_TRAIL_NONE) == 0);   /* 哨兵要被门挡掉 */
            /* ③ 每条残影的判定盒：宽 = 一手 `SpawnShot(…, 16.0, 448.0, …)` 的 16（半尺寸约定 ⇒ 屏上 16 px）；
             *    长 = 跟着视觉走（`YK_LASER_LEN`）—— 第三十一批起不再是硬写的 448。*/
            CHECK(YK_M_LASER_BEAM_BAR_W == 16.0f);
            CHECK(YK_M_LASER_BEAM_BAR_H == 2048.0f);
            CHECK(YK_M_LASER_BEAM_BAR_H == YK_LASER_LEN);
            /* ④ alpha 递减（一手 `0x450af2` 起：`a34 = a0*3/4`、`k != 0` ⇒ `a34 − (a34/2)·2k/rate`）。
             * `k = 0` 那一支一手**不写** alpha ⇒ 返回原值（全场最亮的就是最新的那一条）。
             * ⚠ 下面这几条传的是**字面量 160**（纯函数的输入），与本卡的基准宏值无关。
             * ★★★ 第三十四批：基准宏 **176 → 70**（一手 TH08 `player01.anm` script24..28 的 `alpha(70)`；
             *   176 是第三十一批照 **117/TH17** 的 `script163` 抄来的，两批实跑都报「贼亮、看不清纹理」）。*/
            CHECK(YK_M_LASER_BEAM_ALPHA == 70);              /* ★ 一手值（`alpha(70)`），脚本正文也钉着它 */
            CHECK(yk_m_laser_beam_bar_alpha(70, 0, 4) == 70);         /* 档 0/1（rate 2）：本条 + 26 */
            CHECK(yk_m_laser_beam_bar_alpha(70, 1, 2) == 52 - 26 * 2 / 2);
            CHECK(yk_m_laser_beam_bar_alpha(70, 0, 12) == 70);        /* 档 4：70/48/44/39/35/31/26/22/18/13/9/5 */
            CHECK(yk_m_laser_beam_bar_alpha(70, 1, 12) == 52 - 26 * 2 / 12);
            CHECK(yk_m_laser_beam_bar_alpha(70, 11, 12) == 52 - 26 * 22 / 12);
            /* ★ 加算下「12 条重叠处」的等效亮度 = Σalpha/255（**加算**：`blendMode(1)` = `SRCALPHA/ONE/ADD`）
             *   —— 近似闭式 `a0·(3·rate+5)/8`（整数截断会差几个点，故这里钉**逐档实测值**）：
             *   176 @ rate 12 = 902 = **3.54×**（束心 82% 的像素被钳成纯白 = owner 的「贼 jb 亮」），
             *   70 @ rate 12 = **360 = 1.41×**。逐条裁定见 `TUNING.md` §2.1ccc-14 与 `youkai_core.h`。*/
            {
                static const int sum70[5] = { 96, 148, 202, 254, 360 };   /* 档 0..4（rate 2/4/6/8/12）*/
                int tt, kk, sum;
                for (tt = 0; tt < 5; ++tt) {
                    const int r = yk_m_laser_beam_rate(tt);
                    sum = 0;
                    for (kk = 0; kk < r; ++kk) sum += yk_m_laser_beam_bar_alpha(70, kk, r);
                    CHECK(sum == sum70[tt]);
                    CHECK(sum * 255 >= 96 * 255 / 2);     /* 任何一档都不再是「一条压满 255」*/
                }
                sum = 0;
                for (kk = 0; kk < 12; ++kk) sum += yk_m_laser_beam_bar_alpha(176, kk, 12);
                CHECK(sum == 902);                        /* 改前那一档的叠加总量（= 3.54 × 255）*/
                CHECK(sum > 255 * 3);                     /* ★ 改前：三条满亮叠在一起 = 必然钳白 */
                CHECK(sum70[4] < 255 * 2);                /* ★ 改后：连 2 倍都不到 */
            }
            CHECK(yk_m_laser_beam_bar_alpha(160, 0, 4) == 160);
            CHECK(yk_m_laser_beam_bar_alpha(160, 1, 4) == 90);        /* a34 = 120；k=1 ⇒ 120 − 60·2/4 */
            CHECK(yk_m_laser_beam_bar_alpha(160, 3, 4) == 120 - 60 * 6 / 4);
            CHECK(yk_m_laser_beam_bar_alpha(160, 0, 12) == 160);
            CHECK(yk_m_laser_beam_bar_alpha(160, 11, 12) == 120 - 60 * 22 / 12);
            CHECK(yk_m_laser_beam_bar_alpha(160, 11, 12) > 0);        /* 最旧那条也要看得见（> 0）*/
            CHECK(yk_m_laser_beam_bar_alpha(70, 11, 12) > 0);         /* ★ 基准 70 时最旧那条也 > 0（5）*/
            CHECK(yk_m_laser_beam_bar_alpha(160, 1, 0) == 160);       /* rate = 0 ⇒ 不递减（防除零）*/
            CHECK(yk_m_laser_beam_bar_alpha(255, 1, 4) == 191 - 95 * 2 / 4);
            CHECK(yk_m_laser_beam_bar_alpha(-5, 2, 4) == 0);          /* 基准钳进 0..255 */
            CHECK(yk_m_laser_beam_bar_alpha(300, 0, 4) == 255);
            /* ★★★ 第三十四批：**副本必须同帧起**（一手是同一支 VM 画 rate 次 ⇒ 共享 UV 相位）。
             * 上一版「历史填到几条就起几条」会让相邻副本的相位差 16 texel = ¼ 纹样周期 ⇒ 纹样被平均掉
             * （逐像素模拟：沿束纹样对比 0.384 → 0.085）。判据 = 只要开局那一帧（bars = 1）就起满整档。*/
            CHECK(yk_m_laser_beam_bars_resident(0, 12) == 0);   /* 还没有活点 ⇒ 一支都不起 */
            CHECK(yk_m_laser_beam_bars_resident(0, 2) == 0);
            CHECK(yk_m_laser_beam_bars_resident(1, 2) == 2);    /* ★ 第一个活点就起满整档 */
            CHECK(yk_m_laser_beam_bars_resident(1, 12) == 12);
            CHECK(yk_m_laser_beam_bars_resident(4, 8) == 8);
            CHECK(yk_m_laser_beam_bars_resident(99, 12) == 12); /* 条数比 rate 多也不多起 */
            CHECK(yk_m_laser_beam_bars_resident(-1, 12) == 0);
            CHECK(yk_m_laser_beam_bars_resident(3, 0) == 0);    /* rate = 0 ⇒ 不起（防除零/防越界）*/
            CHECK(yk_m_laser_beam_bars_resident(3, 99) == (int)YK_M_LASER_BEAM_BARS_MAX);  /* 钳进数组第二维 */
            /* alpha 单调不增（越旧越淡）——逐档全扫一遍，防将来手滑写反 */
            {
                int tt, kk, prev;
                for (tt = 0; tt < 5; ++tt) {
                    const int r = yk_m_laser_beam_rate(tt);
                    prev = 999;
                    for (kk = 0; kk < r; ++kk) {
                        const int a = yk_m_laser_beam_bar_alpha(160, kk, r);
                        CHECK(a <= prev);
                        prev = a;
                    }
                    CHECK(prev > 0);
                }
            }
            /* ⑤ 伤害**分摊**：Σ_k = total（一手是「每条残影各 1 点」，本卡要保 DPS ⇒ 摊不改总额）。
             * 与上一版逐档同额：每 2 帧 14 / 9 / 7 / 6 / 5 点。*/
            {
                int tt, kk, sum;
                for (tt = 0; tt < 5; ++tt) {
                    const int tot = yk_m_laser_beam_damage(tt);
                    const int r = yk_m_laser_beam_rate(tt);
                    sum = 0;
                    for (kk = 0; kk < r; ++kk) {
                        const int d = yk_m_laser_beam_bar_damage(tot, r, kk);
                        CHECK(d >= 0);
                        sum += d;
                    }
                    CHECK(sum == tot);                 /* ★ 残影**不是** N 倍伤害 */
                }
                CHECK(yk_m_laser_beam_bar_damage(14, 2, 0) == 7);
                CHECK(yk_m_laser_beam_bar_damage(14, 2, 1) == 7);
                CHECK(yk_m_laser_beam_bar_damage(9, 4, 0) == 3);    /* 9 = 3+2+2+2：余数给最新的两条 */
                CHECK(yk_m_laser_beam_bar_damage(9, 4, 1) == 2);
                CHECK(yk_m_laser_beam_bar_damage(9, 4, 3) == 2);
                CHECK(yk_m_laser_beam_bar_damage(5, 12, 4) == 1);   /* 5 < 12 ⇒ 只有最新 5 条各 1 点 */
                CHECK(yk_m_laser_beam_bar_damage(5, 12, 5) == 0);
                CHECK(yk_m_laser_beam_bar_damage(5, 12, 11) == 0);
                CHECK(yk_m_laser_beam_bar_damage(0, 4, 0) == 0);
                CHECK(yk_m_laser_beam_bar_damage(9, 0, 0) == 0);    /* 没条数 ⇒ 0（不崩、不除零）*/
                CHECK(yk_m_laser_beam_bar_damage(9, 4, 4) == 0);    /* k 越界 ⇒ 0 */
                CHECK(yk_m_laser_beam_bar_damage(9, 4, -1) == 0);
            }
            /* ⑥ 轨迹推进（一手 `0x450840` 的移位 + `0x44fe20` 的清表）*/
            {
                float tr[YK_M_LASER_BEAM_TRAIL_MAX][2];
                int k;
                for (k = 0; k < (int)YK_M_LASER_BEAM_TRAIL_MAX; ++k) {
                    tr[k][0] = YK_M_LASER_BEAM_TRAIL_NONE;
                    tr[k][1] = 0.0f;
                }
                CHECK(yk_m_laser_beam_bar_count(&tr[0][0], 32, 4) == 0);   /* 全哨兵 ⇒ 一条都不出 */
                yk_m_laser_beam_trail_step(&tr[0][0], 32, 100.0f, 200.0f);
                CHECK(tr[0][0] == 100.0f && tr[0][1] == 200.0f);
                CHECK(yk_m_laser_beam_bar_count(&tr[0][0], 32, 4) == 1);   /* 一手：一帧多一条 */
                yk_m_laser_beam_trail_step(&tr[0][0], 32, 110.0f, 200.0f);
                CHECK(tr[0][0] == 110.0f && tr[0][1] == 200.0f);
                CHECK(tr[1][0] == 100.0f && tr[1][1] == 199.0f);           /* 老的点上移一格且 y −1 */
                CHECK(yk_m_laser_beam_bar_count(&tr[0][0], 32, 4) == 2);
                /* 填到第 5 格：条数被 rate 夹住（rate = 4 ⇒ 只出 4 条，第 5 格不参与）*/
                for (k = 0; k < 6; ++k) yk_m_laser_beam_trail_step(&tr[0][0], 32, 100.0f + (float)k, 200.0f);
                CHECK(yk_m_laser_beam_bar_count(&tr[0][0], 32, 4) == 4);
                CHECK(yk_m_laser_beam_bar_count(&tr[0][0], 32, 12) == 8);  /* 现在有 8 格是活的 */
                /* 32 格填满之后仍被 rate 夹住（rate = 12 < 32）*/
                for (k = 0; k < 40; ++k) yk_m_laser_beam_trail_step(&tr[0][0], 32, 100.0f, 200.0f);
                CHECK(yk_m_laser_beam_bar_count(&tr[0][0], 32, 12) == 12);
                CHECK(yk_m_laser_beam_bar_count(&tr[0][0], 32, 32) == 32);
                /* 自机静止 ⇒ 各点 x 全同、y 逐格 −1（横向不动时残影**重合**成一条）*/
                CHECK(tr[0][0] == 100.0f && tr[5][0] == 100.0f);
                CHECK(tr[5][1] == 200.0f - 5.0f);
                /* 横向移动 ⇒ 各点 x 不同（这就是 owner 要的「多束」）*/
                tr[0][0] = 300.0f;
                CHECK(tr[0][0] != tr[5][0]);
                /* 边界：n = 0 / NULL 不崩 */
                yk_m_laser_beam_trail_step(0, 32, 1.0f, 2.0f);
                yk_m_laser_beam_trail_step(&tr[0][0], 0, 1.0f, 2.0f);
                CHECK(yk_m_laser_beam_bar_count(0, 32, 4) == 0);
                CHECK(yk_m_laser_beam_bar_count(&tr[0][0], 32, 0) == 0);
                CHECK(yk_m_laser_beam_bar_count(&tr[0][0], 0, 4) == 0);
            }
            /* ⑦ ★★★ 第三十四批：**跟随**（owner 第 4 条「本体以及伤害源完全跟着子机定死」）。
             * 逐帧模拟「子机每帧横移 6 px」的 tick 顺序（先 `trail[i]=trail[i-1]` 再写本帧膛口）：
             *   · 一手 `0x450840` 写的是 `vm+0x2a4` = **上一帧**膛口 ⇒ 本体慢一帧；
             *   · 本卡写**本帧**膛口 ⇒ `trail[0] == 本帧膛口`（差 0），残影 `k >= 1` 才是历史点。
             * 判据 = 下面那条「差 = 0」（实跑侧的同款判据 = 探针行 `d(trail0,muzzle) 0.00`）。*/
            {
                float tr[YK_M_LASER_BEAM_TRAIL_MAX][2];
                float mx, my;
                int k, f, bars;
                for (k = 0; k < (int)YK_M_LASER_BEAM_TRAIL_MAX; ++k) {
                    tr[k][0] = YK_M_LASER_BEAM_TRAIL_NONE;
                    tr[k][1] = 0.0f;
                }
                for (f = 0; f < 8; ++f) {
                    mx = 100.0f + 6.0f * (float)f;      /* 子机这一帧的膛口（横移 6 px/帧）*/
                    my = 300.0f;
                    yk_m_laser_beam_trail_step(&tr[0][0], 32, mx, my);
                    CHECK(tr[0][0] == mx && tr[0][1] == my);       /* ★ 本帧膛口 = 本体锚（差 0）*/
                    if (f >= 1)
                        CHECK(tr[1][0] == mx - 6.0f && tr[1][1] == my - 1.0f);  /* 残影 = 上一帧 + y−1 */
                    if (f >= 2)
                        CHECK(tr[2][0] == mx - 12.0f && tr[2][1] == my - 2.0f);
                }
                /* 条数照旧（`bars` 只由轨迹决定 ⇒ 与「同帧起满整档」那条例外无关）*/
                bars = yk_m_laser_beam_bar_count(&tr[0][0], 32, 12);
                CHECK(bars == 8);
                CHECK(yk_m_laser_beam_bars_resident(bars, 12) == 12);   /* 常驻 12、可见 8 */
            }
            /* ★★★ 2026-09-27（owner ④）：**妖逢魔染色**的两个纯函数。
             * 一手：只有霊夢（`ply00as`）与魔理沙（`ply01as`）的行 `+0x1e > 0` ⇒ 只有这两位的妖侧组染红。*/
            CHECK(yk_fengma_tint_set(0x54) == 1);            /* 霊夢 A0..A4 */
            CHECK(yk_fengma_tint_set(0x58) == 1);
            CHECK(yk_fengma_tint_set(0x59) == 1);            /* 魔理沙 M0..M4 */
            CHECK(yk_fengma_tint_set(0x5d) == 1);
            CHECK(yk_fengma_tint_set(0x5e) == 0);            /* 咲夜：一手全是 0 ⇒ 不染 */
            CHECK(yk_fengma_tint_set(0x62) == 0);
            CHECK(yk_fengma_tint_set(0x63) == 0);            /* 早苗（TH16 数据）⇒ 一手口径下不染 */
            CHECK(yk_fengma_tint_set(0x67) == 0);
            CHECK(yk_fengma_tint_set(0x16) == 0);            /* 兜底组（幽々子）⇒ 不染 */
            /* 染色 dword：只换低 24 位（BGR），**alpha 必须原样**（一手只写 `+0x1f0/1/2` 三个字节）*/
            CHECK(yk_fengma_tint_dword(0x80000000u) == 0x80ff4040u);
            CHECK(yk_fengma_tint_dword(0x00abcdefu) == 0x00ff4040u);
            CHECK(yk_fengma_tint_dword(0xff123456u) == 0xffff4040u);
            /* TH18 的 dword 是 `0xAARRGGBB` ⇒ 低字节 B=0x40、中 G=0x40、高 R=0xff = RGB(255,64,64) 红 */
            CHECK((YK_FENGMA_TINT_RGB & 0xffu) == 0x40u);
            CHECK(((YK_FENGMA_TINT_RGB >> 8) & 0xffu) == 0x40u);
            CHECK(((YK_FENGMA_TINT_RGB >> 16) & 0xffu) == 0xffu);
            /* ★★★ 2026-09-22 夜（#27 弹字）：距离 alpha 三段 + 十进制字形。
             * 一手：`th08:0x4054e3`-`0x405545` / `:0x405983`-`0x4059f1`（`d = dx²+dy²` 整数）。*/
            CHECK(YK_POPUP_SCORE_LIFE == 60 && YK_POPUP_TIME_LIFE == 90);
            CHECK(YK_POPUP_GLYPH_PLUS == 13);
            CHECK(yk_popup_alpha(0, 0) == 0x50);                  /* d = 0 ⇒ 0x50 */
            CHECK(yk_popup_alpha(32, 0) == 0x50);                 /* d = 0x400 ⇒ 仍 0x50（不 > 0x400）*/
            CHECK(yk_popup_alpha(33, 0) == 0x50 + (0x41 * 128) / 0xc00);   /* 刚过 0x400 */
            CHECK(yk_popup_alpha(64, 0) == 0xD0);                 /* d = 0x1000 ⇒ > 0x400 支；0x1000 不 > 0x1000 */
            CHECK(yk_popup_alpha(65, 0) == 0xD0 || yk_popup_alpha(65, 0) > 0x50);
            CHECK(yk_popup_alpha(1000, 1000) == 0xD0);            /* 远处 ⇒ 满 alpha 上限 */
            {
                uint8_t g[6];
                CHECK(yk_popup_digits(1, g, 6) == 1 && g[0] == 1);
                CHECK(yk_popup_digits(10000, g, 6) == 5 && g[0] == 1 && g[4] == 0);
                CHECK(yk_popup_digits(12345, g, 3) == 3 && g[0] == 1 && g[1] == 2 && g[2] == 3);
                CHECK(yk_popup_digits(0, g, 6) == 1 && g[0] == 0);
                CHECK(yk_popup_digits(-5, g, 6) == 1 && g[0] == 0);
                CHECK(yk_popup_digits(7, 0, 6) == 0);              /* NULL 出口：不写、不崩 */
                CHECK(yk_popup_digits(7, g, 0) == 0);              /* 容量 0：不写 */
            /* ★★★ 2026-09-22 夜（P-C 订正）：**时间斜坡**（一手 `th08:0x44be67`–`0x44bea8`）
             * 与**回中三档**（一手 `th08:0x44bf70`–`0x44bfde`）。*/
            CHECK(YK_GAUGE_RAMP_DIV == 15 && YK_GAUGE_RAMP_CAP == 21 && YK_GAUGE_RAMP_FRAMES == 300);
            CHECK(YK_GAUGE_IDLE_MAX == 30 && YK_GAUGE_STOP_CLEAR == 4);
            CHECK(yk_gauge_ramp_delta(0) == 0);
            CHECK(yk_gauge_ramp_delta(14) == 0);
            CHECK(yk_gauge_ramp_delta(15) == 1);
            CHECK(yk_gauge_ramp_delta(29) == 1);
            CHECK(yk_gauge_ramp_delta(30) == 2);
            CHECK(yk_gauge_ramp_delta(300) == 20);
            CHECK(yk_gauge_ramp_delta(301) == 20);            /* 一手：先夹到 300 再 /15 */
            CHECK(yk_gauge_ramp_delta(315) == 20);          /* 帧数先夹 300 ⇒ 最大 20（21 那个 imm 是
                                                              * float→int 的转换上界，实达不到）*/
            CHECK(yk_gauge_ramp_delta(99999) == 20);          /* 帧数上限 300 ⇒ 实际峰值 20 */
            CHECK(yk_gauge_ramp_delta(-5) == 0);              /* 负帧数按 0 */
            /* 回中三档（带符号）：极妖 ⇒ −5、中妖 ⇒ −3、正侧 ⇒ −2、负侧 ⇒ +2/+3/+5 */
            CHECK(yk_gauge_center_delta(9000) == -5);
            CHECK(yk_gauge_center_delta(8000) == -5);         /* 含等号（setge）*/
            CHECK(yk_gauge_center_delta(2500) == -3);
            CHECK(yk_gauge_center_delta(2000) == -3);
            CHECK(yk_gauge_center_delta(500) == -2);
            CHECK(yk_gauge_center_delta(-500) == 2);
            CHECK(yk_gauge_center_delta(-2500) == 3);
            CHECK(yk_gauge_center_delta(-9000) == 5);
            /* ★★★ 2026-09-22 夜（P-D 缩水版）：击破魔使 = −gauge/12（一手 `th08:0x42b216 mov ecx,0xc`）
             * 与 50 帧静默常量（一手 `th08:0x42b240`）。*/
            CHECK(YK_FAM_KILL_GAUGE_DIV == 12 && YK_ORB_SILENCE_FRAMES == 50);
            CHECK(yk_gauge_fam_kill_delta(12000) == -1000);   /* 夹紧后的量程内：−12000/12 */
            CHECK(yk_gauge_fam_kill_delta(10000) == -833);    /* 一手 `idiv` 向零截断（不是 −834）*/
            CHECK(yk_gauge_fam_kill_delta(8000) == -666);
            CHECK(yk_gauge_fam_kill_delta(2000) == -166);
            CHECK(yk_gauge_fam_kill_delta(12) == -1);
            CHECK(yk_gauge_fam_kill_delta(11) == 0);          /* 向零截断 ⇒ 11/12 = 0 */
            CHECK(yk_gauge_fam_kill_delta(-10000) == 833);    /* 人侧反向同式 */
            CHECK(yk_gauge_fam_kill_delta(0) == 0);
            /* ★★★ 2026-09-22 夜（#27/#28）：**夜点数字的字形**（一手 `th08:0x405d11`–`0x405e01`：
             * 左对齐、7 px/字、**前导零不画也不占位**）。*/
            {
                yk_gauge_glyph_t ng[YK_NIGHT_TEXT_MAX];
                int n2;
                CHECK(YK_NIGHT_TEXT_MAX == 8);
                n2 = yk_night_text_glyphs(60000, ng);
                /* ★ 2026-09-27（§BE #121）：补前导零到 6 位 ⇒ 60000 变成 `060000`（旧断言「5 位」作废）*/
                CHECK(n2 == (int)YK_NIGHT_TEXT_DIGITS);
                CHECK(ng[0].glyph == 0 && ng[1].glyph == 6 && ng[5].glyph == 0);
                CHECK(ng[1].dx == 7.0f && ng[5].dx == 35.0f);             /* 每字 +7（一手 `pos.x += 7.0`）*/
                CHECK(ng[0].dy == 0.0f && ng[0].scale == 1.0f);
                n2 = yk_night_text_glyphs(300000, ng);
                CHECK(n2 == 6 && ng[0].glyph == 3);
                /* ★ 2026-09-27（§BE #121，owner 口径）：**改成补前导零到 6 位**（按「额外最大得点」
                 *   的画法）⇒ 下面三条旧断言（前导零不占位 / 0 只画一位）已作废并改掉。*/
                n2 = yk_night_text_glyphs(123456789, ng);                 /* 超过 8 位 ⇒ 留高位、截 8 位 */
                CHECK(n2 == 8 && ng[0].glyph == 1 && ng[7].glyph == 8);
                n2 = yk_night_text_glyphs(0, ng);
                CHECK(n2 == (int)YK_NIGHT_TEXT_DIGITS && ng[0].glyph == 0 && ng[5].glyph == 0);
                n2 = yk_night_text_glyphs(-5, ng);
                CHECK(n2 == (int)YK_NIGHT_TEXT_DIGITS && ng[0].glyph == 0 && ng[5].glyph == 0);
                CHECK(yk_night_text_glyphs(60000, 0) >= 1);               /* NULL 出口不崩 */
            /* ★★★ 2026-09-22 夜（批次 3）：人侧撞使魔 = 圆 vs 圆
             * （一手：自机判定半径 3.0 @`player+0x4799c`；使魔 hurtbox 半宽 12.0 = 默认 24 的一半）。*/
            CHECK(YK_FAM_HIT_REACH == YK_FAM_HURTBOX_HALF && YK_FAM_HIT_REACH == 12.0f);
            CHECK(yk_fam_hit_player(0.0f, 0.0f, 3.0f) == 1);           /* 正中 */
            CHECK(yk_fam_hit_player(14.9f, 0.0f, 3.0f) == 1);          /* 3.0 + 12.0 = 15.0 内 */
            CHECK(yk_fam_hit_player(15.1f, 0.0f, 3.0f) == 0);          /* 刚好出界 */
            CHECK(yk_fam_hit_player(10.0f, 10.0f, 3.0f) == 1);         /* √200 ≈ 14.1 < 15 */
            CHECK(yk_fam_hit_player(11.0f, 11.0f, 3.0f) == 0);         /* √242 ≈ 15.6 > 15 */
            CHECK(yk_fam_hit_player(1.0f, 1.0f, 0.0f) == 1);           /* 自机半径 0 也按 hurtbox 判 */
            CHECK(yk_fam_hit_player(20.0f, 0.0f, 0.0f) == 0);
            CHECK(yk_fam_hit_player(1.0f, 0.0f, -5.0f) == 1);          /* 负半径：仍按 hurtbox（r>0）*/
            /* ★★★ 2026-09-22 夜（Time 第三道门）：抓取盒的玩家状态白名单。
             * 一手 TH08 = {0 ALIVE, 3 INVULNERABLE, 4 BORDER} 放行（`th08:0x44a5c1`/`:0x44a5c9`/`:0x44a5d4`）；
             * TH18 等位映射 = {1 存活, 3 无敌/复活, 4 决死窗口}（移动只在 case 1 `th18:0x45c0cc`）。*/
            CHECK(YK_ITEM_BOX_STATE_INIT == 0 && YK_ITEM_BOX_STATE_ALIVE == 1 &&
                  YK_ITEM_BOX_STATE_DEAD == 2 && YK_ITEM_BOX_STATE_INVULN == 3 &&
                  YK_ITEM_BOX_STATE_BORDER == 4);
            CHECK(yk_item_box_open(1) == 1);                      /* 存活 */
            CHECK(yk_item_box_open(3) == 1);                      /* 无敌/复活 */
            CHECK(yk_item_box_open(4) == 1);                      /* 决死窗口 */
            CHECK(yk_item_box_open(0) == 0);                      /* 初生/未就位 ⇒ 关 */
            CHECK(yk_item_box_open(2) == 0);                      /* 死亡段 ⇒ 关 */
            CHECK(yk_item_box_open(-1) == 0 && yk_item_box_open(99) == 0);   /* 野值一律关 */
            /* ★★★ 2026-09-22 夜（P-F · 擦弹的**颗数**）：一手 `Player::sub_44a930`（`th08:0x44a930`）
             * 三处 `spawn_item(pos, type 0xa, state 1)`（`th08:0x44aac9` / `:0x44aaef` / `:0x44ab0f`），
             * 各道门**嵌套** ⇒ 每擦到一颗弹 **0 / 1 / 2 / 3** 颗。逐指令与门语义全文 = `youkai_core.h` §3.11c。
             * 这里**每道门各测正 / 反两个方向**（入参正交地把门打开 / 关上）。*/
            /* 两条 shottype 谓词（一手 `th08:0x42f230` / `:0x42f270`：`>= 4` 才算单人，再取奇偶）。*/
            CHECK(yk_shottype_is_solo_human(0) == 0 && yk_shottype_is_solo_human(3) == 0);
            CHECK(yk_shottype_is_solo_human(4) == 1 && yk_shottype_is_solo_human(10) == 1);
            CHECK(yk_shottype_is_solo_human(5) == 0 && yk_shottype_is_solo_human(11) == 0);
            CHECK(yk_shottype_is_solo_youkai(5) == 1 && yk_shottype_is_solo_youkai(11) == 1);
            CHECK(yk_shottype_is_solo_youkai(4) == 0 && yk_shottype_is_solo_youkai(10) == 0);
            CHECK(yk_shottype_is_solo_youkai(0) == 0 && yk_shottype_is_solo_youkai(3) == 0);
            CHECK(yk_shottype_is_solo_human(4) != yk_shottype_is_solo_youkai(4));   /* 互斥 */
            /* ---- 第 1 颗前面那三条**场上门**（★ 2026-09-23 统筹拍：按一手接上，具名开关可回退）----
             * 一手分支顺序 = 机体（`th08:0x44aa9e`）→ boss（`th08:0x44aaac`）→ 槽极妖（`th08:0x44aaba`）。*/
            CHECK(YK_GRAZE_GATE_OK == 0 && YK_GRAZE_GATE_SHOTTYPE == 1 && YK_GRAZE_GATE_BOSS == 2 &&
                  YK_GRAZE_GATE_GAUGE == 3 && YK_GRAZE_GATE_OFF == 4);
            /* 门「机体」（`th08:0x44aa94` / `:0x44aa9e`）：**人単独且不是妖夢（0xa）**。
             *   正（挡）：4 / 6 / 8；反（放）：3（组）、5（妖単独）、0xa（妖夢例外）。*/
            CHECK(yk_graze_field_gate(1, 1, 8000, 4) == YK_GRAZE_GATE_SHOTTYPE);
            CHECK(yk_graze_field_gate(1, 1, 8000, 8) == YK_GRAZE_GATE_SHOTTYPE);
            CHECK(yk_graze_field_gate(1, 1, 8000, 3) == YK_GRAZE_GATE_OK);
            CHECK(yk_graze_field_gate(1, 1, 8000, 5) == YK_GRAZE_GATE_OK);
            CHECK(yk_graze_field_gate(1, 1, 8000, 10) == YK_GRAZE_GATE_OK);   /* 妖夢例外 */
            /* 门「boss」（`th08:0x44aaac`）：正（放）= 场上有 boss；反（挡）= 没有。
             *   挡住时**先于**槽门被报（一手顺序：boss 在槽之前判）。*/
            CHECK(yk_graze_field_gate(1, 1, 8000, 0) == YK_GRAZE_GATE_OK);
            CHECK(yk_graze_field_gate(1, 0, 8000, 0) == YK_GRAZE_GATE_BOSS);
            CHECK(yk_graze_field_gate(1, 0, 0, 0) == YK_GRAZE_GATE_BOSS);     /* 两条都不过 ⇒ 报 boss */
            /* 门「槽极妖」（`th08:0x44aaba`，阈值 +8000 = 80%）：正（放）= 8000 及以上；反（挡）= 7999 及以下。*/
            CHECK(yk_graze_field_gate(1, 1, 8000, 0) == YK_GRAZE_GATE_OK);
            CHECK(yk_graze_field_gate(1, 1, 7999, 0) == YK_GRAZE_GATE_GAUGE);
            CHECK(yk_graze_field_gate(1, 1, 0, 0) == YK_GRAZE_GATE_GAUGE);
            CHECK(yk_graze_field_gate(1, 1, 10000, 0) == YK_GRAZE_GATE_OK);   /* 满槽 */
            CHECK(yk_graze_field_gate(1, 1, -10000, 0) == YK_GRAZE_GATE_GAUGE); /* 极人侧：也不算极妖 */
            /* ❓ 开关关掉（`YK_GRAZE_FIELD_GATES = 0`）：三条门整段不评 ⇒ 恒 `OFF`
             *   （与三个入参无关）⇒ 这就是「聚焦即撒」老行为。*/
            CHECK(yk_graze_field_gate(0, 0, 0, 4) == YK_GRAZE_GATE_OFF);
            CHECK(yk_graze_field_gate(0, 1, 8000, 0) == YK_GRAZE_GATE_OFF);
            /* ---- 颗数：门没过 ⇒ **0 颗**（一手三条 `je 0x44ab14` 都跳到函数尾）---- */
            CHECK(yk_graze_orbs_per_bullet(YK_GRAZE_GATE_BOSS, 1, 1, 0) == 0);
            CHECK(yk_graze_orbs_per_bullet(YK_GRAZE_GATE_GAUGE, 1, 1, 0) == 0);
            CHECK(yk_graze_orbs_per_bullet(YK_GRAZE_GATE_SHOTTYPE, 1, 1, 4) == 0);
            /* 门 A = flag（`th08:0x44aace`）**且** 符卡进行中（`th08:0x44aad9` = `SPELLCARD.flags & 1`）：
             *   正：弹路径 + 符卡中 ⇒ 第 2/3 颗；反：符卡不在 ⇒ 停在 1 颗；激光路径（flag=1）⇒ 也停在 1 颗。*/
            CHECK(yk_graze_orbs_per_bullet(YK_GRAZE_GATE_OK, 1, 1, 0) == 3);   /* 门开 */
            CHECK(yk_graze_orbs_per_bullet(YK_GRAZE_GATE_OK, 1, 0, 0) == 1);   /* 门关（没在打符卡）*/
            CHECK(yk_graze_orbs_per_bullet(YK_GRAZE_GATE_OK, 0, 1, 0) == 1);   /* 门关（激光擦弹，flag != 0）*/
            /* 门 B（`th08:0x44ab00`）：**妖単独**机体停在第 2 颗。
             *   正：`!solo_youkai`（组 0..3）⇒ 3 颗；反：`solo_youkai`（5 / 7 / 9 / 11）⇒ 2 颗。*/
            CHECK(yk_graze_orbs_per_bullet(YK_GRAZE_GATE_OK, 1, 1, 0) == 3);
            CHECK(yk_graze_orbs_per_bullet(YK_GRAZE_GATE_OK, 1, 1, 5) == 2);
            CHECK(yk_graze_orbs_per_bullet(YK_GRAZE_GATE_OK, 1, 1, 11) == 2);
            /* 门 B 只在前两颗都给出来的前提下才被求值（嵌套）：妖単独 + 没在打符卡 ⇒ 仍是 1 颗。*/
            CHECK(yk_graze_orbs_per_bullet(YK_GRAZE_GATE_OK, 1, 0, 5) == 1);
            /* 本卡的映射（TH18 机体号 0..3 = TH08 的「组」）下能取到的两档：平擦 1 / 符卡中 3。*/
            for (i = 0; i < 4u; ++i) {
                CHECK(yk_graze_orbs_per_bullet(YK_GRAZE_GATE_OK, 1, 0, (int)i) == 1);
                CHECK(yk_graze_orbs_per_bullet(YK_GRAZE_GATE_OK, 1, 1, (int)i) == 3);
            }
            /* ★★★ 2026-09-27（§BE #136，owner 实跑「**妖逢魔现在擦弹似乎不掉永点**」）：
             *   **这一支的两个纯函数都没有「形态 / focus / 低速」入参 —— 这是照抄一手，不是漏写。**
             *   ① 一手逐指令：那道 `IsYoukai()` 闸（`th08:0x44aa6c call 0x40bc40` → `:0x44aa76 je
             *      th08:0x44aa86`）**只包住 +100 那一笔**，人 / 妖两侧在 `th08:0x44aa86` **会合**，
             *      三处刻符生成（`th08:0x44aac9` / `:0x44aaef` / `:0x44ab0f`）全在会合点**之后**；
             *   ② 社区第二源（スコアアタックまとめ wiki「刻符出現条件」）逐字：「妖怪逢魔が時に
             *      敵弾にかする（**ボス戦のみ**）」＋小ネタ「…**低速であることは関係ない**」。
             *   ⇒ 判据（妖逢魔 + 有 boss）下**必须给 ≥ 1 颗**；卡侧若在调用点再包一层 `if (focused)`
             *     就正好把「妖逢魔 + 高速擦弹」整段吞掉 —— 旧代码就是那样（本轮已删）。*/
            CHECK(YK_GRAZE_ORBS_MAX == 3);
            for (i = 0; i < 4u; ++i) {                    /* 本卡四个机体各来一遍 */
                CHECK(yk_graze_field_gate(1, 1, 10000, (int)i) == YK_GRAZE_GATE_OK);   /* 妖逢魔满槽 */
                CHECK(yk_graze_field_gate(1, 1, 8000, (int)i) == YK_GRAZE_GATE_OK);    /* 门线含等号 */
                CHECK(yk_graze_orbs_per_bullet(YK_GRAZE_GATE_OK, 1, 0, (int)i) == 1);
                CHECK(yk_graze_orbs_per_bullet(YK_GRAZE_GATE_OK, 1, 1, (int)i) == YK_GRAZE_ORBS_MAX);
            }
            /* 边界：槽 7999（= 79%）仍不算极妖 ⇒ 过不了门 ⇒ 0 颗；且 0 **只能**来自没门过。*/
            CHECK(yk_graze_orbs_per_bullet(yk_graze_field_gate(1, 1, 7999, 0), 1, 1, 0) == 0);
            CHECK(yk_graze_orbs_per_bullet(YK_GRAZE_GATE_OK, 0, 0, 0) == 1);
            /* ★ 开关关掉（`OFF`）⇒ **回到老行为**：连「人単独非妖夢」也不再拦（只会被门 A/B 影响）。*/
            CHECK(yk_graze_orbs_per_bullet(YK_GRAZE_GATE_OFF, 1, 1, 4) == 3);
            CHECK(yk_graze_orbs_per_bullet(YK_GRAZE_GATE_OFF, 1, 0, 4) == 1);
            CHECK(yk_graze_orbs_per_bullet(YK_GRAZE_GATE_OFF, 1, 1, 10) == 3);
            /* 野值（不该出现的判据码）⇒ 按「没过」处理（防御性；不崩、不给）。*/
            CHECK(yk_graze_orbs_per_bullet(7, 1, 1, 0) == 0);
            CHECK(yk_graze_orbs_per_bullet(-1, 1, 1, 0) == 0);
            }
            }
        }
    }

    /* ★★★ 2026-09-27（§BE #121）：夜点数字**补前导零到 6 位**（owner：按额外最大得点的画法，
     *   初始要看得见 `000000`）。`glyph 0` = '0'、dx = i×7（一手：每位 `pos.x += 7.0`）。*/
    {
        yk_gauge_glyph_t gg[YK_NIGHT_TEXT_MAX];
        int n = yk_night_text_glyphs(0, gg);
        CHECK(n == (int)YK_NIGHT_TEXT_DIGITS);
        CHECK(gg[0].glyph == 0 && gg[5].glyph == 0);
        CHECK(gg[5].dx > gg[0].dx);                       /* 低位在右 */
        n = yk_night_text_glyphs(1234, gg);
        CHECK(n == (int)YK_NIGHT_TEXT_DIGITS);
        CHECK(gg[0].glyph == 0 && gg[1].glyph == 0);       /* 前导零补满 */
        CHECK(gg[2].glyph == 1 && gg[3].glyph == 2 && gg[4].glyph == 3 && gg[5].glyph == 4);
        n = yk_night_text_glyphs(-5, gg);
        CHECK(n == (int)YK_NIGHT_TEXT_DIGITS && gg[5].glyph == 0);   /* 负数按 0 */
        n = yk_night_text_glyphs(999999, gg);
        CHECK(n == (int)YK_NIGHT_TEXT_DIGITS && gg[0].glyph == 9);
        n = yk_night_text_glyphs(1234567, gg);             /* 7 位（补零只在不足 6 位时）*/
        CHECK(n == 7 && gg[0].glyph == 1 && gg[6].glyph == 7);
    }

    /* ★★★ 2026-09-27（§BE #114）：击破批的**门**与**颗数**（一手 `Enemy::FUN_0042adb0`）。
     * 门的地址：`th08:0x42adbc call 0x41fd40`（数连锁）→ `0x42adc4 cmp [ebp-0x4],0 je 0x42b1e5`
     * （连锁为空 ⇒ **整段撒点跳过**）⇒ `n == 0` 必须给 **0 颗**（不是公式在 n=0 时的 10）。*/
    {
        unsigned k;
        yk_map_t t;
        memset(&t, 0, sizeof t);
        CHECK(yk_kill_burst_count(0) == 0);
        CHECK(yk_kill_ring_count(0) == 0);
        /* 双人组那一支（owner 2026-09-27 拍板）：`n >= 8 ? 26 : 2n+10`。
         * 一手边界两条：`0x42af29 cmp [ebp-0x4],0x8` / `0x42af2f mov [ebp-0x68],0x1a`。*/
        CHECK(yk_kill_burst_count(1) == 12);       /* ← 旧版写死的 12（这一档正好与一手同值）*/
        CHECK(yk_kill_burst_count(2) == 14);
        CHECK(yk_kill_burst_count(7) == 24);
        CHECK(yk_kill_burst_count(8) == 26);       /* 封顶那一档 */
        CHECK(yk_kill_burst_count(9) == 26);
        CHECK(yk_kill_burst_count(9999u) == 26);
        /* 被杀者自己那一环：`0x42b150 shl eax,1` ⇒ `2 · subEnemyCount`。*/
        CHECK(yk_kill_ring_count(1) == 2);
        CHECK(yk_kill_ring_count(4) == 8);
        CHECK(yk_kill_ring_count(9999u) == 19998u);
        /* ★★★ 2026-10-01（**记忆值**，owner 口径 —— 修「死亡 +N 不出现」）：
         *   `yk_kill_display_count(live, remembered)` = `max(live, remembered)`；
         *   口径：显示值与爆点公式用这个数，**位置循环仍按活子敌**（有几个位置画几个）。
         *   边界四条（owner 实跑的两种局面 + 两个退化）：*/
        CHECK(yk_kill_display_count(0u, 13u) == 13u);   /* 人侧：先清光使魔再打死母机 ⇒ n 仍 = 13 */
        CHECK(yk_kill_display_count(5u, 13u) == 13u);   /* 活子敌少于当初发的台数 ⇒ 取记忆值 */
        CHECK(yk_kill_display_count(13u, 5u) == 13u);   /* 活的比记录多（后补过/记录被清过）⇒ 取活数 */
        CHECK(yk_kill_display_count(0u, 0u) == 0u);     /* 都没使魔 ⇒ 0（门照旧拦住，一颗不给）*/
        CHECK(yk_kill_display_count(1u, 1u) == 1u);
        CHECK(yk_kill_display_count(9999u, 0u) == 9999u);
        CHECK(yk_kill_display_count(0u, 9999u) == 9999u);
        /* 记忆值只**放大**门与颗数（单调不减 ⇒ 不会把原本会发生的击破批吃掉）。*/
        for (k = 0u; k < 40u; ++k) {
            CHECK(yk_kill_display_count(k, k) == k);
            CHECK(yk_kill_display_count(k, 7u) >= k);
            CHECK(yk_kill_display_count(k, 7u) >= 7u);
            CHECK(yk_kill_burst_count(yk_kill_display_count(0u, k))
                  == yk_kill_burst_count(k));
        }
        /* 单调不减（防将来把常数写成 +6 / +16 那两支时手滑）。*/
        for (k = 1u; k < 40u; ++k) {
            CHECK(yk_kill_burst_count(k) <= yk_kill_burst_count(k + 1u));
            CHECK(yk_kill_burst_count(k) >= 12u);
        }
        /* ★ 半径上界可换（一手尾巴那一支把 `2N` 换成常量 128 的实参）：
         *   `yk_orb_burst_polar_range(range, …)` ≡ `yk_orb_burst_polar(range/2, …)`；
         *   `0x42b15d push 0x43000000` 是 `GetRandomF32InRange(128.0f)` 的**实参** ⇒ 圆盘上界。*/
        {
            float a1, r1, a2, r2;
            yk_orb_burst_polar(64, 0.25f, 0.5f, &a1, &r1);
            yk_orb_burst_polar_range(128.0f, 0.25f, 0.5f, &a2, &r2);
            CHECK(a1 == a2);
            CHECK(r1 == 64.0f);                    /* 0.5 × 128 */
            CHECK(r2 == 64.0f);
            yk_orb_burst_polar_range(128.0f, 0.0f, 1.0f, &a2, &r2);
            CHECK(r2 == 128.0f);                   /* 上界（真实 RNG 的 r2 < 1 ⇒ 取不到，这里喂 1.0）*/
            yk_orb_burst_polar_range(128.0f, 1.0f, 0.0f, &a2, &r2);
            CHECK(a2 > 3.14159f - 1e-4f && a2 < 3.14159f + 1e-4f);   /* 角上界 = π（≈ 满一圈）*/
            CHECK(r2 == 0.0f);
            yk_orb_burst_polar_range(-5.0f, 0.5f, 1.0f, &a2, &r2);    /* 负上界夹到 0 */
            CHECK(r2 == 0.0f);
            yk_orb_burst_polar_range(128.0f, 0.5f, 0.5f, 0, 0);       /* 双 NULL：不崩 */
        }
        (void)t;
    }

    /* ★★★ 2026-09-27 第二十八批（owner ⑥ 遗留 ❓ 收口）：**束根光斑 / 束端命中特效 / 极妖染红**。
     * 一手链条（脚本全文、逐指令、TH08 查不到什么）写在 `cards/youkai_core.h` 的同一节。*/
    {
        /* ① **极妖 ⇒ 残影束整条染红**（一手 `th08:0x450ad0` 的 `0x450bf2`：RGB(255,64,64)、alpha 不动）。
         *   我们复用 owner ④ 那套（`yk_fengma_tint_dword` / `YK_FENGMA_TINT_RGB`）。*/
        CHECK(yk_m_laser_beam_bar_color(160, 0) == 0xa0ffffffu);
        CHECK(yk_m_laser_beam_bar_color(160, 1) == 0xa0ff4040u);
        CHECK((yk_m_laser_beam_bar_color(160, 1) & 0xff000000u) == (uint32_t)(160u << 24));
        CHECK((yk_m_laser_beam_bar_color(160, 1) & 0x00ffffffu) == (YK_FENGMA_TINT_RGB & 0x00ffffffu));
        CHECK(yk_m_laser_beam_bar_color(0, 1) == YK_FENGMA_TINT_RGB);        /* alpha 0 也保 */
        CHECK(yk_m_laser_beam_bar_color(255, 0) == 0xffffffffu);
        CHECK(yk_m_laser_beam_bar_color(255, 1) == 0xffff4040u);
        CHECK(yk_m_laser_beam_bar_color(300, 0) == 0xffffffffu);             /* 越界钳到 0xff */
        CHECK(yk_m_laser_beam_bar_color(-5, 0) == 0x00ffffffu);
        /* 逐条 alpha（一手 `0x450ad0` 的递减结果）也照样染色，且**只换低 24 位**。*/
        CHECK(yk_m_laser_beam_bar_color(yk_m_laser_beam_bar_alpha(160, 5, 12), 1) >> 24
              == (uint32_t)yk_m_laser_beam_bar_alpha(160, 5, 12));

        /* ② **束端命中特效的节拍**：一手两条不同的数（TH18 `hit2` 每 2 帧 / TH08 `0x450ee0` 约每 8 帧），
         *   取 2 —— 它是**帧计数**判据，且与伤害源同相。*/
        CHECK(YK_M_LASER_BEAM_FX_EVERY == 2);
        CHECK(YK_M_LASER_BEAM_FX_EVERY == YK_M_LASER_BEAM_SETTLE);
        CHECK(yk_m_laser_beam_fx_due(0));
        CHECK(yk_m_laser_beam_fx_due(2));
        CHECK(yk_m_laser_beam_fx_due(120));
        CHECK(!yk_m_laser_beam_fx_due(1));
        CHECK(!yk_m_laser_beam_fx_due(3));
        CHECK(!yk_m_laser_beam_fx_due(121));

        /* ③ **命中判据那条带**：横向 ±（束半宽 8 + 特效半宽 16）；纵向 = 束那一列，
         *   远端 = `by − YK_M_LASER_BEAM_BAR_H`（= `YK_LASER_LEN`，**函数读的是宏、不是第 4 个实参**）。
         *   ★ 第三十一批：束长 448 → 2048 ⇒ 下面的远端边界从 0 挪到 `448 − 2048 = −1600`。
         *   实参 `(ex, ey, bx, by)` 里的 `by` = **膛口 y**（调用点传的是轨迹点）。*/
        CHECK(YK_M_LASER_BEAM_FX_HALF_W == 24.0f);
        CHECK(YK_M_LASER_BEAM_FX_HALF_W == YK_M_LASER_BEAM_BAR_W * 0.5f + 16.0f);
        CHECK(yk_m_laser_beam_fx_on_beam(100.0f, 200.0f, 100.0f, 448.0f));      /* 正中轴上 */
        CHECK(yk_m_laser_beam_fx_on_beam(124.0f, 200.0f, 100.0f, 448.0f));      /* 边界 |dx| = 24 */
        CHECK(!yk_m_laser_beam_fx_on_beam(125.0f, 200.0f, 100.0f, 448.0f));
        CHECK(!yk_m_laser_beam_fx_on_beam(75.0f, 200.0f, 100.0f, 448.0f));      /* 另一侧同样（|dx| = 25）*/
        CHECK(!yk_m_laser_beam_fx_on_beam(100.0f, 449.0f, 100.0f, 448.0f));     /* 在膛口**下方** = 束反方向 */
        CHECK(yk_m_laser_beam_fx_on_beam(100.0f, 448.0f, 100.0f, 448.0f));      /* 近端边界（含）*/
        CHECK(yk_m_laser_beam_fx_on_beam(100.0f, -1600.0f, 100.0f, 448.0f));    /* 远端边界（含）= 448 − 2048 */
        CHECK(!yk_m_laser_beam_fx_on_beam(100.0f, -1601.0f, 100.0f, 448.0f));   /* 超出 2048 */
        CHECK(yk_m_laser_beam_fx_on_beam(124.0f, 0.0f, 100.0f, 448.0f));

        /* ④ 束根光斑 / 命中特效用的**原生脚本号与层号**（一手见 `youkai_core.h`）：
         *   两支脚本互不相同；光斑 13 < 束 14 < 子机 15 这条 owner 定的层序，光斑在束**之下**。*/
        CHECK(YK_BEAM_CORE_SCRIPT == 4);
        CHECK(YK_BEAM_HIT_FX_SCRIPT == 5);
        CHECK(YK_BEAM_CORE_SCRIPT != YK_BEAM_HIT_FX_SCRIPT);
        CHECK(YK_BEAM_CORE_ANM_LAYER == 13);
        CHECK(YK_BEAM_HIT_FX_LAYER == 15);
        CHECK(YK_BEAM_CORE_ANM_LAYER < YK_BEAM_HIT_FX_LAYER);
        CHECK(YK_BEAM_HIT_FX_LAYER == YK_BEAM_CORE_ANM_LAYER + 2);   /* 原生那一对（15 与 13）的相对位置 */
        CHECK(YK_BEAM_CORE_ANM_LAYER == 13);                        /* = 自机弹那一层（原生 script4 自己写的值）*/
    }
    /* ===== ★★★ 2026-09-28（第四十九批）：**惰性绕转使魔**的纯逻辑（大怪判据 / 相位 / 查表）=====
     * 语义来源：`youkai_core.h` 那一节（owner 口径 + 117 的同源判据）；场景侧在 `youkai.c`。*/
    {
        float dx, dy;
        unsigned i;

        /* ① 「大怪」判据的边界（63×64 / 64×63 / 64×64）—— 与 117 `cards/animal.c` 的
         *    `ANIMAL_BIG_SPRITE_MIN_W/H` 同值（**2026-10-01 起两边都是 48**），
         *    所以这一段同时钉住「两张卡用的是同一个数」。*/
        CHECK(YK_BIG_SPRITE_MIN_W == 64 && YK_BIG_SPRITE_MIN_H == 64);   /* ★ 2026-10-01：尺寸门回 64，48×48 走白名单 */
        /* ★ 2026-09-30：阈值 64 → **48**（4 面那两种「大妖精」= 48×48；小妖精 32×32 / 48×32 仍排除）*/
        CHECK(!yk_big_by_sprite(47, 48));
        CHECK(!yk_big_by_sprite(48, 47));
        /* ★ 2026-10-01 白名单：48×48 只有 72..87 / 292..307 算大怪 */
        CHECK(!yk_big_by_sprite_id(48, 48, 88));              /* #88（6 面那批）**不算** */
        CHECK(!yk_big_by_sprite_id(48, 48, 95));
        CHECK(!yk_big_by_sprite_id(48, 48, 308));             /* entry4 的对应排除段 */
        CHECK(yk_big_by_sprite_id(48, 48, 72));               /* 大妖精第一个 */
        CHECK(yk_big_by_sprite_id(48, 48, 87));
        CHECK(yk_big_by_sprite_id(48, 48, 292));              /* entry4 的大妖精段 */
        CHECK(yk_big_by_sprite_id(48, 48, 307));
        CHECK(yk_big_by_sprite_id(64, 64, 0));                /* 64×64 不限号 */
        CHECK(!yk_big_by_sprite(48, 48));                     /* ★ 尺寸门回 64 ⇒ 旧入口对 48×48 说「不」*/
        CHECK(yk_big_by_sprite(64, 64));                      /* 大蝴蝶 */
        CHECK(yk_big_by_sprite(65, 64));
        CHECK(yk_big_by_sprite(64, 65));
        CHECK(yk_big_by_sprite(96, 96));                      /* 实测的 64×64 那一批 sprite */
        CHECK(!yk_big_by_sprite(0, 0));
        CHECK(!yk_big_by_sprite(-64, 100));                   /* 读到的值不信任：负数不算大怪 */
        CHECK(!yk_big_by_sprite(100, -64));

        /* ② 相位推进：每帧 +3°、满 120 回绕；走满一圈**正好**回到起点 */
        CHECK(YK_ORBIT_STEP_DEG == 3);
        CHECK(YK_ORBIT_STEPS == 120);
        CHECK(YK_ORBIT_STEPS == 360u / YK_ORBIT_STEP_DEG);
        CHECK(YK_ORBIT_PHASE_STRIDE == 17u);                  /* 每敌错相的乘子 */
        /* ★★★ 第七轮引入、2026-09-30 **订正**的**半步累加器**：表仍是 3°/格，实际角速度 =
         *   `YK_ORBIT_STEP_DEG / YK_ORBIT_STEP_DIV`。**当前 DIV = 1 ⇒ 一手值 3.0°/帧**
         *  （owner 那句「旋转速度太快」的根因是**半径读错**，不是角速度；一手另一档 1.5°/帧
         *    = 把 DIV 改 2 ⇒ 这两条一起核）。下列断言对 DIV 的**两个取值都成立**。*/
        CHECK(YK_ORBIT_STEP_DIV == 1);                        /* 一手角速度 3.0°/帧 */
        CHECK(YK_ORBIT_SUBSTEPS == YK_ORBIT_STEPS * YK_ORBIT_STEP_DIV);
        CHECK(YK_ORBIT_SUBSTEPS == 120);                      /* DIV = 1 ⇒ 120 帧 = 2 s 一圈 */
        CHECK((double)YK_ORBIT_STEP_DEG / (double)YK_ORBIT_STEP_DIV == 3.0);
        /* 一圈的帧数 = 360° ÷ 有效角速度（= `STEPS × DIV`，两条独立算法给出同一个数）*/
        CHECK((unsigned)(360u / (YK_ORBIT_STEP_DEG / YK_ORBIT_STEP_DIV)) == YK_ORBIT_SUBSTEPS);
        /* ① substep → 格号 = `(substep / DIV) % STEPS`（DIV 个 substep 走一格）*/
        CHECK(yk_orbit_step_of_substep(0) == 0u);
        CHECK(yk_orbit_step_of_substep(YK_ORBIT_STEP_DIV - 1u) == 0u);
        CHECK(yk_orbit_step_of_substep(YK_ORBIT_STEP_DIV) == 1u);
        CHECK(yk_orbit_step_of_substep(YK_ORBIT_STEP_DIV * 4u) == 4u);
        CHECK(yk_orbit_step_of_substep(YK_ORBIT_SUBSTEPS - 1u) == YK_ORBIT_STEPS - 1u);
        CHECK(yk_orbit_step_of_substep(YK_ORBIT_SUBSTEPS) == 0u);          /* 越界也回绕（mod）*/
        CHECK(yk_orbit_step_of_substep(1000000000u) < YK_ORBIT_STEPS);
        /* ② 累加：每帧 +1、满 `SUBSTEPS` 回绕 */
        CHECK(yk_orbit_substep_advance(0) == 1u);
        CHECK(yk_orbit_substep_advance(YK_ORBIT_SUBSTEPS - 2u) == YK_ORBIT_SUBSTEPS - 1u);
        CHECK(yk_orbit_substep_advance(YK_ORBIT_SUBSTEPS - 1u) == 0u);
        CHECK(yk_orbit_substep_advance(1000000000u) < YK_ORBIT_SUBSTEPS);
        /* ③ 20 帧 = 20 个 substep = `20/DIV` 格 = **60°**（20 × 3.0°）**/
        {
            unsigned sub = 0;
            for (i = 0; i < 20u; ++i) sub = yk_orbit_substep_advance(sub);
            CHECK(sub == 20u);
            CHECK(yk_orbit_step_of_substep(sub) == (unsigned)(20u / YK_ORBIT_STEP_DIV));
            CHECK(yk_orbit_step_of_substep(sub) * YK_ORBIT_STEP_DEG == 60u);   /* 度数自检 */
            {   /* 每一格横跨 **DIV** 个 substep ⇒ 逐格核一遍映射（DIV 两个取值都过）*/
                unsigned c, j;
                for (c = 0; c < YK_ORBIT_STEPS; ++c)
                    for (j = 0; j < YK_ORBIT_STEP_DIV; ++j)
                        CHECK(yk_orbit_step_of_substep(c * YK_ORBIT_STEP_DIV + j) == c);
            }
        }
        /* ④ 走满一圈回到起点；格 0 在一圈里只被经过一次（DIV 个 substep 算同一格）*/
        {
            unsigned sub = 0, hits = 0;
            for (i = 0; i < YK_ORBIT_SUBSTEPS; ++i) {
                sub = yk_orbit_substep_advance(sub);
                CHECK(sub < YK_ORBIT_SUBSTEPS);
                if (yk_orbit_step_of_substep(sub) == 0u) ++hits;
            }
            CHECK(sub == 0u);
            CHECK(yk_orbit_step_of_substep(sub) == 0u);
            CHECK(hits == 1u);            /* DIV=1 ⇒ 只有 substep 0 落格 0；DIV=2 时是 2 */
        }

        /* ⑤ 四色**语义表**（2026-09-30 owner 口径）：两轴 = 会不会体术 × 高低速弹是否一样。
         *   四个 kind 必须**正好覆盖 2×2 四种组合**（每个 (体术, 弹) 组合恰好一个 kind）。*/
        CHECK(YK_FAM_KIND_COUNT == 4);
        CHECK(YK_FAM_KIND_MELEE_SHOT_SAME == 0);
        CHECK(YK_FAM_KIND_MELEE_SHOT_DIFF == 1);
        CHECK(YK_FAM_KIND_NO_MELEE_SHOT_SAME == 2);
        CHECK(YK_FAM_KIND_NO_MELEE_SHOT_DIFF == 3);
        CHECK(yk_fam_kind_valid(0) && yk_fam_kind_valid(1) && yk_fam_kind_valid(2) && yk_fam_kind_valid(3));
        CHECK(!yk_fam_kind_valid(-1));
        CHECK(!yk_fam_kind_valid(4));
        CHECK(yk_fam_kind_melee(YK_FAM_KIND_MELEE_SHOT_SAME) == 1);      /* 发弹口使魔：会体术 */
        CHECK(yk_fam_kind_shot_same(YK_FAM_KIND_MELEE_SHOT_SAME) == 1);  /* 且弹一样（不改符卡）*/
        CHECK(yk_fam_kind_melee(YK_FAM_KIND_MELEE_SHOT_DIFF) == 1);
        CHECK(yk_fam_kind_shot_same(YK_FAM_KIND_MELEE_SHOT_DIFF) == 0);
        CHECK(yk_fam_kind_melee(YK_FAM_KIND_NO_MELEE_SHOT_SAME) == 0);   /* 额外使魔：不会体术 */
        CHECK(yk_fam_kind_shot_same(YK_FAM_KIND_NO_MELEE_SHOT_SAME) == 1);
        CHECK(yk_fam_kind_melee(YK_FAM_KIND_NO_MELEE_SHOT_DIFF) == 0);
        CHECK(yk_fam_kind_shot_same(YK_FAM_KIND_NO_MELEE_SHOT_DIFF) == 0);
        {   /* 2×2 全覆盖：四种 (melee, shot_same) 组合各出现且只出现一次 */
            int seen[2][2] = { { 0, 0 }, { 0, 0 } };
            int k;
            for (k = 0; k < YK_FAM_KIND_COUNT; ++k)
                seen[yk_fam_kind_melee(k)][yk_fam_kind_shot_same(k)] += 1;
            for (k = 0; k < 2; ++k) {
                CHECK(seen[k][0] == 1);
                CHECK(seen[k][1] == 1);
            }
        }
        /* 越界 kind ⇒ 按最保守那一档（会体术 + 弹一样）*/
        CHECK(yk_fam_kind_melee(99) == yk_fam_kind_melee(YK_FAM_KIND_MELEE_SHOT_SAME));
        CHECK(yk_fam_kind_shot_same(-5) == yk_fam_kind_shot_same(YK_FAM_KIND_MELEE_SHOT_SAME));
        /* ⑥ 四色的**资产级**存在性：四支脚本互不相同（卡侧另有 `_Static_assert` 逐对钉死）。
         *   ⚠ 2×2 语义 ↔ 53..56 的**配色本身 = ❓自定**（无一手依据）⇒ 这里只核「有四支不同的」。*/
        CHECK(CE_ANM_ABILITY_SCRIPT_YOUKAI_FAMILIAR_S53 != CE_ANM_ABILITY_SCRIPT_YOUKAI_FAMILIAR_S54);
        CHECK(CE_ANM_ABILITY_SCRIPT_YOUKAI_FAMILIAR_S54 != CE_ANM_ABILITY_SCRIPT_YOUKAI_FAMILIAR_S55);
        CHECK(CE_ANM_ABILITY_SCRIPT_YOUKAI_FAMILIAR_S55 != CE_ANM_ABILITY_SCRIPT_YOUKAI_FAMILIAR_S56);
        CHECK(CE_ANM_ABILITY_SCRIPT_YOUKAI_FAMILIAR_S53 != CE_ANM_ABILITY_SCRIPT_YOUKAI_FAMILIAR_S56);

        /* ③ 四个象限的代表值（**y 轴向下**：0 = 正右、30 = 正下、60 = 正左、90 = 正上）*/
        CHECK(yk_orbit_offset(0, 48.0f, &dx, &dy) && dx > 47.9f && dx < 48.1f && dy > -0.1f && dy < 0.1f);
        CHECK(yk_orbit_offset(30, 48.0f, &dx, &dy) && dx > -0.1f && dx < 0.1f && dy > 47.9f && dy < 48.1f);
        CHECK(yk_orbit_offset(60, 48.0f, &dx, &dy) && dx > -48.1f && dx < -47.9f && dy > -0.1f && dy < 0.1f);
        CHECK(yk_orbit_offset(90, 48.0f, &dx, &dy) && dx > -0.1f && dx < 0.1f && dy > -48.1f && dy < -47.9f);

        /* ④ 模长守恒（**不引 libm**：只用乘加与比较）—— 全部 120 项都在 1% 容差内 */
        for (i = 0; i < YK_ORBIT_STEPS; ++i) {
            const float r = 48.0f, r2 = r * r;
            float m2;
            CHECK(yk_orbit_offset(i, r, &dx, &dy));
            m2 = dx * dx + dy * dy;
            CHECK(m2 > r2 * 0.99f && m2 < r2 * 1.01f);
        }
        CHECK(yk_orbit_offset(17, 0.0f, &dx, &dy) && dx == 0.0f && dy == 0.0f);   /* 半径 0 ⇒ 原地 */

        /* ⑤ 越界 step 一律回绕进表内；空指针返回 0（调用方的编程错，不是数据问题）*/
        CHECK(yk_orbit_offset(120, 48.0f, &dx, &dy) && dx > 47.9f && dx < 48.1f);   /* 120 ≡ 0 */
        CHECK(yk_orbit_offset(121, 48.0f, &dx, &dy));          /* ≡ 1，不崩即可 */
        CHECK(yk_orbit_offset(1000000000u, 48.0f, &dx, &dy));
        CHECK(!yk_orbit_offset(0, 48.0f, NULL, &dy));
        CHECK(!yk_orbit_offset(0, 48.0f, &dx, NULL));

        /* ⑥ 台数 = **父敌最大血量的函数**，表 = **TH08 一手统计**（★★★ 第三轮校准；出处见
         *   `youkai_core.c` 的表头注 + `engine/enemy/th08/03-familiar-count-vs-hp.md`）。
         *   分档（**闭区间**）：`<=200 ⇒ 2` / `<=1000 ⇒ 4` / `<=2000 ⇒ 8` / 其余 ⇒ 13。
         *   这一段钉的是**函数与表的关系**（边界含/不含、单调不降、钳进池上限），不是拟合曲线。*/
        CHECK(YK_ORBIT_HP_TIER_STEPS == 3);
        CHECK(YK_ORBIT_MAX_PER_ENEMY == 18);                  /* = 13（TH08 最大档）+ 5（面加成上限）*/
        CHECK(YK_ORBIT_STAGE_BONUS_MAX == 5);
        CHECK(yk_orbit_count_for_hp(0) == 2);                 /* 读不到血量 ⇒ 第一档（不放弃补）*/
        CHECK(yk_orbit_count_for_hp(-12345) == 2);
        CHECK(yk_orbit_count_for_hp(1) == 2);
        CHECK(yk_orbit_count_for_hp(200) == 2);               /* 阈值是**闭区间**：200 还在第一档 */
        CHECK(yk_orbit_count_for_hp(201) == 4);
        CHECK(yk_orbit_count_for_hp(1000) == 4);
        CHECK(yk_orbit_count_for_hp(1001) == 8);
        CHECK(yk_orbit_count_for_hp(2000) == 8);
        CHECK(yk_orbit_count_for_hp(2001) == 13);
        CHECK(yk_orbit_count_for_hp(999999) == 13);
        CHECK(yk_orbit_count_for_hp(2001) == YK_ORBIT_HP_MAX_COUNT);   /* HP 档只管到 13（不含面加成）*/
        {   /* 单调不降 + 恒在 [1, 池上限] 内（表被替换后这两条也必须成立）*/
            int prev = 0, hp, v;
            for (hp = -10; hp <= 5000; hp += 37) {
                v = yk_orbit_count_for_hp(hp);
                CHECK(v >= 1 && v <= (int)YK_ORBIT_MAX);
                CHECK(v >= prev);
                prev = v;
            }
        }

        /* ⑥bis ★★★★ 2026-10-01（**owner 定案口径**）**使魔血量 = 母机最大血量 / 圈数**：
         *   「档 0（母机 HP ≤ 200）恒 **100**、其它 **`母机HP / 圈数`**」（纯函数 `yk_orbit_fam_hp`，
         *   圈数 = `yk_orbit_rings_for_count()` = `s_orbit_tab[]` 的 `nrings`；**不再封顶 999**）。
         *   口径、推导例子与「一手 `familiarCreate` 的 100/100/300/600 作对照」见 `youkai_core.h`。
         *   ⚠ 这一批**不再**走 `yk_orbit_familiar_hp(台数档)` 那条旧式（旧函数留给对照/单测）。*/
        CHECK(YK_FAM_HP_TIER0_MAX == 200);
        /* owner 给的五个例子（逐字核过）：*/
        CHECK(yk_orbit_fam_hp(3200, 3) == 1066);   /* 3200/3 = 1066.67 ⇒ 1066 */
        CHECK(yk_orbit_fam_hp(1800, 3) == 600);
        CHECK(yk_orbit_fam_hp(3200, 1) == 3200);   /* ★ >999 合法（spawn 侧两步走）*/
        CHECK(yk_orbit_fam_hp(200, 2) == 100);     /* 档 0（圈数不参与）*/
        CHECK(yk_orbit_fam_hp(0, 1) == 100);
        /* 边界与退化：*/
        CHECK(yk_orbit_fam_hp(-5, 3) == 100);      /* 负数 ⇒ 档 0（不崩、不返回负数）*/
        CHECK(yk_orbit_fam_hp(200, 3) == 100);     /* 档 0 上界（含）*/
        CHECK(yk_orbit_fam_hp(201, 1) == 201);     /* 刚出档 0：/1 */
        CHECK(yk_orbit_fam_hp(201, 2) == 100);     /* 201/2 = 100 */
        CHECK(yk_orbit_fam_hp(201, 3) == 67);      /* 201/3 = 67 */
        CHECK(yk_orbit_fam_hp(900, 1) == 900);
        CHECK(yk_orbit_fam_hp(900, 2) == 450);
        CHECK(yk_orbit_fam_hp(900, 3) == 300);
        CHECK(yk_orbit_fam_hp(1998, 2) == 999);    /* 1998/2 = 999 ⇒ 正好不越门 */
        CHECK(yk_orbit_fam_hp(2000, 2) == 1000);   /* ★ 1000 > 999 也照给（spawn 侧两步走）*/
        CHECK(yk_orbit_fam_hp(999999, 3) == 333333);
        CHECK(yk_orbit_fam_hp(4000, 0) == 4000);   /* rings < 1 当 1 */
        CHECK(yk_orbit_fam_hp(4000, -7) == 4000);
        CHECK(yk_orbit_fam_hp(4000, 99) == 800);   /* rings > 5 钳 5（表最多 5 圈）*/
        {   /* 不变量：恒 ≥ 1、**无 999 上限**；`hp > 200` 段对 hp 单调不降、对 rings 单调不增。
             * ⚠ **不**断言「恒 ≥ 100」：档 0 的 100 只在 `hp <= 200`；`hp = 201` 且圈数 3 时
             *   `201/3 = 67`（口径边界的小坑，已在报告里向 owner 提，未擅自改）。*/
            int hp, r, prev;
            for (r = 0; r <= 6; ++r) {
                prev = 0;
                for (hp = 201; hp <= 5000; hp += 41) {
                    int v = yk_orbit_fam_hp(hp, r);
                    CHECK(v >= 1);
                    CHECK(v >= prev);
                    prev = v;
                }
                CHECK(yk_orbit_fam_hp(0, r) >= 1);       /* 档 0 恒 100（0/负数/读不到）*/
                CHECK(yk_orbit_fam_hp(200, r) == 100);
            }
            for (hp = 500; hp <= 5000; hp += 250) {      /* 圈数越多、每台越少（同一个 HP）*/
                CHECK(yk_orbit_fam_hp(hp, 1) >= yk_orbit_fam_hp(hp, 2));
                CHECK(yk_orbit_fam_hp(hp, 2) >= yk_orbit_fam_hp(hp, 3));
            }
        }
        /* 圈数出口 = 与 `s_orbit_tab[]` 同一张表（表外/未定义 ⇒ 1，与 `yk_orbit_layout` 的单圈兜底一致）。*/
        CHECK(yk_orbit_rings_for_count(1) == 1);
        CHECK(yk_orbit_rings_for_count(5) == 1);
        CHECK(yk_orbit_rings_for_count(6) == 2);
        CHECK(yk_orbit_rings_for_count(8) == 2);
        CHECK(yk_orbit_rings_for_count(9) == 3);
        CHECK(yk_orbit_rings_for_count(13) == 3);
        CHECK(yk_orbit_rings_for_count(18) == 3);
        CHECK(yk_orbit_rings_for_count(0) == 1);           /* 表外 */
        CHECK(yk_orbit_rings_for_count(-3) == 1);
        CHECK(yk_orbit_rings_for_count(19) == 1);
        /* 卡侧入口 = `fam_hp × YK_ORBIT_HP_SCALE`（**只保底 1、不钳 999**；SCALE 变了也成立）。*/
        {
            static const int probe_hp[] = { 0, 200, 201, 1000, 1800, 2000, 3200, 999999 };
            static const int probe_r[]  = { 1, 2, 3 };
            unsigned pi, ri;
            for (pi = 0; pi < sizeof probe_hp / sizeof probe_hp[0]; ++pi)
                for (ri = 0; ri < sizeof probe_r / sizeof probe_r[0]; ++ri) {
                    int want = yk_orbit_fam_hp(probe_hp[pi], probe_r[ri]) * YK_ORBIT_HP_SCALE;
                    if (want < 1) want = 1;
                    CHECK(yk_orbit_hp_for_enemy(probe_hp[pi], probe_r[ri], YK_CHAR_REIMU) == want);
                }
            /* 「母机 3200 血、3 圈」= 卡侧真实会走的那条（>999 ⇒ 由 spawn 两步走承接）。*/
            CHECK(yk_orbit_hp_for_enemy(3200, 3, YK_CHAR_REIMU) == 1066 * YK_ORBIT_HP_SCALE);
            CHECK(yk_orbit_hp_for_enemy(3200, 3, YK_CHAR_REIMU) > YK_LIFE_MAX_ALLOWED);   /* 证明「不钳 999」*/
        }
        /* ⑥ter ★★★★ 2026-10-01（**owner 口径 ② 第二轮定案**）**映射（发射口）使魔血量 = 母机 HP / 3**：
         *   「发射口使魔血量理论上应当是一个**在一波里面或许能打死**的值，但这个判定就很不好说了」
         *   ⇒ `/3` 是**启发式**（不是精确判定）；档 0 与绕转共用（≤200 ⇒ 100，owner「0 还是 100」）；
         *   **不钳 999**（>999 由 spawn 路径两步写血承接）。动机 + 数值依据（5/6 面 13000/14000、
         *   单波 360 f、灵梦 1280 dps ⇒ 2560–3840 是目标）见 `youkai_core.h` 的同名小节。*/
        CHECK(yk_map_fam_hp(13000, YK_CHAR_REIMU) == 4333);       /* ★ owner 的锚点：5 面道中 ⇒ 3.39 s @1280 dps */
        CHECK(yk_map_fam_hp(14000, YK_CHAR_REIMU) == 4666);       /* ★ 6 面道中 ⇒ 3.65 s */
        CHECK(yk_map_fam_hp(3200, YK_CHAR_REIMU) == 1066);
        CHECK(yk_map_fam_hp(1800, YK_CHAR_REIMU) == 600);
        CHECK(yk_map_fam_hp(200, YK_CHAR_REIMU) == 100);          /* 档 0 上界（含）*/
        CHECK(yk_map_fam_hp(0, YK_CHAR_REIMU) == 100);            /* 读不到 ⇒ 档 0 */
        CHECK(yk_map_fam_hp(-9, YK_CHAR_REIMU) == 100);           /* 负数 ⇒ 档 0 */
        CHECK(yk_map_fam_hp(201, YK_CHAR_REIMU) == 67);           /* 201/3 = 67（小坑：比档 0 的 100 少）*/
        CHECK(yk_map_fam_hp(300, YK_CHAR_REIMU) == 100);
        CHECK(yk_map_fam_hp(301, YK_CHAR_REIMU) == 100);          /* 301/3 = 100 */
        CHECK(yk_map_fam_hp(302, YK_CHAR_REIMU) == 100);          /* 302/3 = 100（整数除法）*/
        CHECK(yk_map_fam_hp(303, YK_CHAR_REIMU) == 101);
        CHECK(yk_map_fam_hp(1000, YK_CHAR_REIMU) == 333);
        CHECK(yk_map_fam_hp(2000, YK_CHAR_REIMU) == 666);         /* ★ >999 只在 3000 以上出现（见下）*/
        CHECK(yk_map_fam_hp(3000, YK_CHAR_REIMU) == 1000);        /* ★ >999：不再封顶 */
        CHECK(yk_map_fam_hp(999999, YK_CHAR_REIMU) == 333333);
        CHECK(yk_map_fam_hp(3000, YK_CHAR_REIMU) > YK_LIFE_MAX_ALLOWED);
        {   /* 不变量（分两段，别跨 200 那格比单调 —— 档 0 的 100 会「跳低」到 67）：
             *   · `hp <= 200` ⇒ **恒 100**（含 0 / 负数 / 读不到）；
             *   · `hp >= 201` ⇒ `hp/3`、恒 ≥ 1、恒 ≤ hp、对 HP 单调不降。
             * ⚠ 全部按**灵梦（基准 1000‰）**核 —— 机体倍率在下面单独测。*/
            int prev = 0, hp;
            for (hp = -20; hp <= 200; hp += 7) CHECK(yk_map_fam_hp(hp, YK_CHAR_REIMU) == 100);
            for (hp = 201; hp <= 8000; hp += 37) {
                int v = yk_map_fam_hp(hp, YK_CHAR_REIMU);
                CHECK(v >= 1);
                CHECK(v <= hp);
                CHECK(v == hp / 3);
                CHECK(v >= prev);
                prev = v;
            }
        }
        /* ⑥quater ★★★★ 2026-10-01（**owner 口径 ③ · 按机体的使魔血量倍率**，平衡项）：
         *   owner 逐字：「我觉得 **616dps 可以减小**，并且**她高速是扩散机体，打使魔命中不太了**；
         *   对于**咲夜和魔理沙也可以适当减小**，**灵梦高速能追踪比较 OP**」。
         *   模型 = `有效 = 标称 × 命中系数`、倍率 = `有效_i / 有效_灵梦`（只乘血量）；表与推导见
         *   `youkai_core.h` 的同名小节。⚠ **平衡口径、不是复刻**；命中系数是估计值（待实测校准）。*/
        CHECK(yk_fam_hp_char_scale(YK_CHAR_REIMU) == 1000);   /* 基准 = ×1.0 */
        CHECK(yk_fam_hp_char_scale(YK_CHAR_MARISA) == 350);   /* ★ owner 第三轮：一档下调 0.85 ⇒ ≈0.59 */
        CHECK(yk_fam_hp_char_scale(YK_CHAR_SAKUYA) == 880);   /* 1880 × 0.60 = 1128 ⇒ 1128/1280 */
        CHECK(yk_fam_hp_char_scale(YK_CHAR_SANAE) == 240);    /* 616 × 0.50 = 308 ⇒ 308/1280 */
        CHECK(yk_fam_hp_char_scale(-1) == 1000);              /* 越界/未知 ⇒ 缺省 1.0（缺省值即缺省）*/
        CHECK(yk_fam_hp_char_scale(4) == 1000);
        CHECK(yk_fam_hp_char_scale(999) == 1000);
        /* 单调性（owner 的期望方向）：早苗 < 魔理沙/咲夜 < 灵梦 */
        CHECK(yk_fam_hp_char_scale(YK_CHAR_SANAE) < yk_fam_hp_char_scale(YK_CHAR_MARISA));
        CHECK(yk_fam_hp_char_scale(YK_CHAR_MARISA) < yk_fam_hp_char_scale(YK_CHAR_SAKUYA));
        CHECK(yk_fam_hp_char_scale(YK_CHAR_SAKUYA) < yk_fam_hp_char_scale(YK_CHAR_REIMU));
        /* 倍率乘在**最后**：`HP_i = (母机HP/3) × 倍率_i`（档 0 的 100 也乘）。*/
        CHECK(yk_map_fam_hp(13000, YK_CHAR_REIMU) == 4333);
        CHECK(yk_map_fam_hp(13000, YK_CHAR_SAKUYA) == 4333 * 880 / 1000);   /* 3813 */
        CHECK(yk_map_fam_hp(13000, YK_CHAR_MARISA) == 4333 * 350 / 1000);   /* 1516 */
        CHECK(yk_map_fam_hp(13000, YK_CHAR_SANAE)  == 4333 * 240 / 1000);   /* 1039 */
        CHECK(yk_map_fam_hp(200, YK_CHAR_SANAE) == 24);                     /* 档 0 也乘（100×0.24）*/
        /* ★ 口径目标：四机体在 13000 血母机上**打死一只的耗时相同**（≈3.4 s @各自的有效 DPS）
         *   —— 用整数近似核一遍（`HP_i × 1000 / 有效DPS_i` 应与灵梦同量级）。*/
        {
            static const int eff[4] = { 1280, 448, 1128, 308 };             /* 有效 DPS（表；魔理沙 350‰）*/
            static const int chr[4] = { YK_CHAR_REIMU, YK_CHAR_MARISA, YK_CHAR_SAKUYA, YK_CHAR_SANAE };
            int i, t0;
            t0 = yk_map_fam_hp(13000, YK_CHAR_REIMU) * 1000 / eff[0];
            for (i = 0; i < 4; ++i) {
                int t = yk_map_fam_hp(13000, chr[i]) * 1000 / eff[i];
                /* `t` = 毫秒（HP × 1000 / dps 的整数近似）⇒ 等时（±10%）且都 < 6 s 波。*/
                CHECK(t > t0 * 90 / 100 && t < t0 * 110 / 100);
                CHECK(t < 6000);
            }
        }
        /* 旧式（台数档一手表）仍在、仍可用作对照 —— 但**不再是**卡侧口径。*/
        CHECK(YK_ORBIT_HP_ONEHAND[0] == 100 && YK_ORBIT_HP_ONEHAND[1] == 100);
        CHECK(YK_ORBIT_HP_ONEHAND[2] == 300 && YK_ORBIT_HP_ONEHAND[3] == 600);
        CHECK(yk_orbit_onehand_hp(1) == 100);      /* 档 0（台数 ≤2）*/
        CHECK(yk_orbit_onehand_hp(2) == 100);
        CHECK(yk_orbit_onehand_hp(3) == 100);      /* 档 1（台数 ≤5）*/
        CHECK(yk_orbit_onehand_hp(5) == 100);
        CHECK(yk_orbit_onehand_hp(6) == 300);      /* 档 2（台数 ≤8）*/
        CHECK(yk_orbit_onehand_hp(8) == 300);
        CHECK(yk_orbit_onehand_hp(9) == 600);      /* 档 3（其余，含 13）*/
        {   /* 旧式 = 一手表 × SCALE，再过同一个 999 / 1 钳（SCALE 变了也成立）。*/
            int want = (int)YK_ORBIT_HP_ONEHAND[3] * YK_ORBIT_HP_SCALE;
            if (want > YK_LIFE_MAX_ALLOWED) want = YK_LIFE_MAX_ALLOWED;
            if (want < 1) want = 1;
            CHECK(yk_orbit_familiar_hp(13) == want);
        }

        /* ⑦ ★★★★ 绕转半径 = **随帧龄线性外扩到 60**（2026-09-30 订正：一手 `moveCircle` 的
         *   第 1 实参是**时长**不是半径 ⇒ 形状是「贴本体 → 1.7 s 螺旋外扩到位 → 冻结」）。*/
        CHECK(YK_ORBIT_FINAL_RADIUS == 60.0f);                 /* 一手实算 50–64 的中间值 */
        CHECK(YK_ORBIT_EXPAND_FRAMES == 20u);                  /* ★ 2026-09-30：统一到一手分圈样本的 20 帧 */
        CHECK(YK_ORBIT_RADIAL_SPEED > 2.99f && YK_ORBIT_RADIAL_SPEED < 3.01f);   /* = 60/20 */
        {
            float r = -1.0f;
            CHECK(yk_orbit_radius_at(0, &r) && r == 0.0f);     /* 出生：贴在本体身上 */
            CHECK(yk_orbit_radius_at(-5, &r) && r == 0.0f);    /* 负数当 0 */
            CHECK(yk_orbit_radius_at(1, &r) && r > 2.99f && r < 3.01f);
            CHECK(yk_orbit_radius_at(10, &r) && r > 29.9f && r < 30.1f);
            CHECK(yk_orbit_radius_at(19, &r) && r < YK_ORBIT_FINAL_RADIUS);
            CHECK(yk_orbit_radius_at(20, &r) && r == YK_ORBIT_FINAL_RADIUS);       /* 到位 */
            CHECK(yk_orbit_radius_at(21, &r) && r == YK_ORBIT_FINAL_RADIUS);       /* 冻结 */
            CHECK(yk_orbit_radius_at(1000000, &r) && r == YK_ORBIT_FINAL_RADIUS);
            CHECK(!yk_orbit_radius_at(10, NULL));
            {   /* 单调不降 + 恒在 [0, FINAL] 内 */
                float prev = -1.0f;
                int age;
                for (age = 0; age <= 220; ++age) {
                    CHECK(yk_orbit_radius_at(age, &r));
                    CHECK(r >= 0.0f && r <= YK_ORBIT_FINAL_RADIUS);
                    CHECK(r >= prev);
                    prev = r;
                }
            }
        }

        /* ⑪ 相位：`k * 120 / n` 均匀铺开（n >= 2 互不重合、n=2 正好 180° 相对）+ 每敌错相。*/
        CHECK(yk_orbit_phase_id(0) == 0u);
        CHECK(yk_orbit_phase_id(1) == 17u);                   /* (1*17) % 120 */
        CHECK(yk_orbit_phase_id(2) == 34u);
        CHECK(yk_orbit_phase_id(7) == 119u);                  /* (119) % 120 */
        CHECK(yk_orbit_phase_id(8) == 16u);                   /* 136 % 120：相邻两只大怪错 17 步 */
        CHECK(yk_orbit_phase_id(0xffffffffu) < YK_ORBIT_STEPS);
        CHECK(yk_orbit_phase_step(4, 0) == 0u);
        CHECK(yk_orbit_phase_step(4, 1) == 30u);
        CHECK(yk_orbit_phase_step(4, 2) == 60u);
        CHECK(yk_orbit_phase_step(4, 3) == 90u);
        CHECK(yk_orbit_phase_step(2, 1) == 60u);              /* 两台 = 正好 180° 相对 */
        CHECK(yk_orbit_phase_step(8, 1) == 15u);
        CHECK(yk_orbit_phase_step(13, 0) == 0u);
        CHECK(yk_orbit_phase_step(13, 12) == (unsigned)(12 * 120 / 13));
        CHECK(yk_orbit_phase_step(1, 0) == 0u);
        CHECK(yk_orbit_phase_step(0, 0) == 0u);               /* 越界：n <= 0 / k < 0 / k >= n ⇒ 0 */
        CHECK(yk_orbit_phase_step(4, -1) == 0u);
        CHECK(yk_orbit_phase_step(4, 4) == 0u);
        CHECK(yk_orbit_phase_step(4, 99) == 0u);
        {   /* n = 1..13：全部在表内，且 n >= 2 时两两不重合、首尾不重合 */
            int n, k, j;
            for (n = 1; n <= (int)YK_ORBIT_MAX_PER_ENEMY; ++n) {
                unsigned st[24];
                CHECK(n <= 24);
                for (k = 0; k < n; ++k) {
                    st[k] = yk_orbit_phase_step(n, k);
                    CHECK(st[k] < YK_ORBIT_STEPS);
                    if (k) CHECK(st[k] > st[k - 1]);          /* 严格递增 ⇒ 均匀且互不重合 */
                }
                CHECK(st[0] == 0u);
                if (n >= 2) {
                    CHECK(st[n - 1] != st[0]);
                    for (k = 0; k < n; ++k)
                        for (j = k + 1; j < n; ++j) CHECK(st[k] != st[j]);
                }
            }
        }
        {   /* 同父的两台一定相对（n=2 的 {0,60}）；不同父（id 不同）初相不同 */
            unsigned a = (yk_orbit_phase_id(101) + yk_orbit_phase_step(2, 0)) % YK_ORBIT_STEPS;
            unsigned b = (yk_orbit_phase_id(101) + yk_orbit_phase_step(2, 1)) % YK_ORBIT_STEPS;
            unsigned c = (yk_orbit_phase_id(102) + yk_orbit_phase_step(2, 0)) % YK_ORBIT_STEPS;
            CHECK(a < YK_ORBIT_STEPS && b < YK_ORBIT_STEPS && c < YK_ORBIT_STEPS);
            CHECK(b != a);
            CHECK(c != a);
        }
    }
    check_orbit_layout();                   /* ★ 2026-09-30：按台数分圈（8=3+5 / 13=3+5+5 / 其余单圈）*/
    check_orbit_rings_14_18();              /* ★ 2026-09-30：14–18 台 ⇒ 多圈（owner 口径，一手无样本）*/
    check_beam_member_covers_entry();       /* ★ 第三十三批：束贴图铺满整条 entry 行宽（「一缕一缕」的闸）*/
    check_beam_spec_files();
    check_orbit_gift_ledger();              /* ★ 2026-09-30：绕转使魔「一只敌人只补一次」的判定 */

    printf("youkai_core: %d checks, %d failed\n", s_pass + s_fail, s_fail);
    return s_fail ? 1 : 0;
}
