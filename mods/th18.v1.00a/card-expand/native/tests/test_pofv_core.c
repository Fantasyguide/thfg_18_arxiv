/* test_pofv_core.c —— pofv_core.c 的主机单测（暴风圈 = TH19 的「圈」）。
 *
 * 一手（`engine/versus/th19/06-aura.md` §7.1/§8.5/§9.2/§9.3 + 本轮 §10）：
 *   · 形状 = `sprite(-1); layer(6); drawPoly(1.0f, 64); color/color2; alpha/alpha2;
 *     angleVel(-1°/帧); stop();` ⇒ 半径 = **VM 的 scale**，由外部每帧写；
 *   · 半径 = 动画器 `FUN_00402c30(&aura+0x6c)` 的输出 ⇒ **律的形状**（初值 / ease / 时长 / 保持）是一手；
 *     ⚠ **目标半径不是一手**：TH19 的目标 = `clamp(蓄力,0,200) × K/100` 是**光环**的公式（挂在蓄力槽上），
 *       暴风与蓄力无关（owner 2026-10-05）⇒ 本卡**直接给半径**（`POFV_STORM_R_TARGET`，❓自定）。
 *   · 扩张律的通用支 = `out = (target − cur) × ease(模式, f) + cur`，`ease` = `FUN_004d5e10`
 *     （表 `0x4d66e0`，32 条）——**mode 4 = `1 − (1−f)²`**（ease-out 二次）；0/7/8/17 = 线性。
 */
#include <stdio.h>

#include "../cards/pofv_core.h"

static int s_fail, s_pass;
#define CHECK(cond) do { if (cond) ++s_pass; else { ++s_fail; printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } } while (0)

static float fabsf_(float a) { return a < 0 ? -a : a; }
static int feq(float a, float b) { return fabsf_(a - b) < 0.01f; }

/* ---- ease 曲线（一手 FUN_004d5e10 的 32 条表）---- */
static void check_ease(void)
{
    float e;
    pofv_ease(4, 0.0f, &e);   CHECK(feq(e, 0.0f));
    pofv_ease(4, 0.25f, &e);  CHECK(feq(e, 1.0f - 0.75f * 0.75f));   /* 0.4375 */
    pofv_ease(4, 0.5f, &e);   CHECK(feq(e, 0.75f));
    pofv_ease(4, 1.0f, &e);   CHECK(feq(e, 1.0f));
    pofv_ease(4, -1.0f, &e);  CHECK(feq(e, 0.0f));                  /* 钳到 0 */
    pofv_ease(4, 9.0f, &e);   CHECK(feq(e, 1.0f));                  /* 钳到 1 */
    pofv_ease(0, 0.5f, &e);   CHECK(feq(e, 0.5f));                  /* 线性档 */
    pofv_ease(7, 0.3f, &e);   CHECK(feq(e, 0.3f));
    pofv_ease(17, 0.3f, &e);  CHECK(feq(e, 0.3f));
    pofv_ease(5, 0.0f, &e);   CHECK(feq(e, 0.0f));
    pofv_ease(5, 0.5f, &e);   CHECK(feq(e, 1.0f - 0.125f));              /* 1 − (1−f)³ ⇒ 0.875 */
    pofv_ease(5, 1.0f, &e);   CHECK(feq(e, 1.0f));
    pofv_ease(1, 0.5f, &e);   CHECK(feq(e, 0.25f));                 /* f² */
    pofv_ease(2, 0.5f, &e);   CHECK(feq(e, 0.125f));                /* f³ */
    pofv_ease(4, 0.5f, NULL);                                       /* 不崩 */

    /* ease-out 的性质：先快后慢（半步 > 一半）*/
    { float h; pofv_ease(4, 0.5f, &h); CHECK(h > 0.5f); }
}

/* ---- 半径的律 ---- */
static void check_radius_law(void)
{
    pofv_storm_pool_t p;
    const pofv_storm_t *s;
    float r, target;

    pofv_storm_target(&target);
    CHECK(feq(target, 50.0f));                  /* ❓自定：直接给的目标半径（与蓄力无关）*/
    CHECK(feq(POFV_STORM_R0, 1.0f));            /* 一手：script84 的 scale(0,0) ⇒ 从 0 起 */
    CHECK(POFV_STORM_TWEEN_FRAMES == 30);       /* 一手：scaleTime(40, 0, 1.5, 1.5) */
    CHECK(POFV_STORM_EASE_MODE == 5);           /* 一手：ins_1031 的 mode=5 ⇒ 1−(1−f)³ */

    pofv_storm_pool_init(&p);
    CHECK(pofv_storm_spawn(&p, 10.0f, 20.0f, POFV_STORM_LIFE_FRAMES) == 0);
    s = pofv_storm_slot(&p, 0);
    CHECK(s && s->active == 1 && s->x == 10.0f && s->y == 20.0f);

    pofv_storm_radius(s, &r);      CHECK(feq(r, 1.0f));      /* f=0 ⇒ 初值 1.0 */
    pofv_storm_tick(&p);           /* age = 1 */
    pofv_storm_radius(s, &r);                                  /* f=0.05 */
    CHECK(feq(r, 1.0f + 49.0f * (1.0f - (1.0f - 1.0f / 30.0f) * (1.0f - 1.0f / 30.0f) * (1.0f - 1.0f / 30.0f))));
    {   int i; for (i = 1; i < 10; ++i) pofv_storm_tick(&p); }  /* age = 10 */
    pofv_storm_radius(s, &r);      CHECK(feq(r, 1.0f + 49.0f * (1.0f - (2.0f / 3.0f) * (2.0f / 3.0f) * (2.0f / 3.0f))));   /* age 10/30 = 1/3 ⇒ 1−(1−f)³ */
    {   int i; for (i = 10; i < 20; ++i) pofv_storm_tick(&p); } /* age = 20 */
    pofv_storm_radius(s, &r);      CHECK(feq(r, 1.0f + 49.0f * (1.0f - (1.0f / 3.0f) * (1.0f / 3.0f) * (1.0f / 3.0f))));  /* age 20/30 = 2/3 */
    {   int i; for (i = 20; i < POFV_STORM_TWEEN_FRAMES - 1; ++i) pofv_storm_tick(&p); }  /* age = 29 */
    pofv_storm_radius(s, &r);      CHECK(feq(r, 1.0f + 49.0f * (1.0f - (1.0f / 30.0f) * (1.0f / 30.0f) * (1.0f / 30.0f))));  /* 逼近目标 */
    pofv_storm_tick(&p);           /* age = 30 = 寿命（本卡口径：寿命 == 时长）⇒ 释放 */
    pofv_storm_radius(s, &r);      CHECK(feq(r, 0.0f));
    CHECK(p.live == 0);
}

/* ---- 场的生 / 老 / 死 / 池 ---- */
static void check_pool(void)
{
    pofv_storm_pool_t p;
    int i;
    float r;

    pofv_storm_pool_init(&p);
    CHECK(p.live == 0 && p.dropped == 0);
    CHECK(pofv_storm_slot(&p, 0) != NULL && pofv_storm_slot(&p, 0)->active == 0);
    for (i = 0; i < POFV_STORM_MAX; ++i)
        CHECK(pofv_storm_spawn(&p, (float)i, 0.0f, POFV_STORM_LIFE_FRAMES) == i);
    CHECK(p.live == POFV_STORM_MAX);
    CHECK(pofv_storm_spawn(&p, 0.0f, 0.0f, POFV_STORM_LIFE_FRAMES) == -1);
    CHECK(p.dropped == 1 && p.live == POFV_STORM_MAX);

    for (i = 0; i < POFV_STORM_LIFE_FRAMES; ++i) pofv_storm_tick(&p);
    CHECK(p.live == 0);
    pofv_storm_radius(pofv_storm_slot(&p, 0), &r);   CHECK(feq(r, 0.0f));

    CHECK(pofv_storm_slot(&p, -1) == NULL);
    CHECK(pofv_storm_slot(&p, POFV_STORM_MAX) == NULL);
    CHECK(pofv_storm_slot(NULL, 0) == NULL);
    CHECK(pofv_storm_spawn(NULL, 0.0f, 0.0f, 1) == -1);
    CHECK(pofv_storm_tick(NULL) == 0);
    pofv_storm_radius(NULL, NULL);                    /* 不崩 */
    pofv_storm_target(NULL);
}

/* ---- 可消弹的判据（TH18 侧；owner：th18 不存在白弹，用本仓设计的那套）---- */
static void check_cancellable(void)
{
    float radii[6];
    CHECK(pofv_is_white_bullet(1.0f) == 1);
    CHECK(pofv_is_white_bullet(POFV_WHITE_R_MAX) == 1);
    CHECK(pofv_is_white_bullet(4.0001f) == 0);
    CHECK(pofv_is_white_bullet(15.0f) == 0);
    CHECK(pofv_is_white_bullet(0.0f) == 0);
    radii[0]=2.0f; radii[1]=0.0f; radii[2]=4.0f; radii[3]=8.0f; radii[4]=0.5f; radii[5]=3.9f;
    CHECK(pofv_scan_count_white(radii, 6) == 4);
    CHECK(pofv_scan_count_white(NULL, 6) == 0);
}

/* ---- 灵（W1）---- */
static void check_spirit(void)
{
    pofv_spirit_pool_t sp;
    int i, f;
    pofv_spirit_pool_init(&sp);
    CHECK(sp.live == 0 && sp.dropped == 0);
    CHECK(pofv_spirit_spawn(&sp, 3.0f, 4.0f, 0, 5) == 0);
    CHECK(sp.live == 1);
    CHECK(pofv_spirit_slot(&sp, 0)->x == 3.0f && pofv_spirit_slot(&sp, 0)->kind == 0);
    for (i = 0; i < 4; ++i) { f = pofv_spirit_tick(&sp); CHECK(f == -1); }   /* 还没到期 */
    f = pofv_spirit_tick(&sp);                                              /* age = 5 = life ⇒ 成仏 */
    CHECK(f == 0 && sp.live == 0);
    CHECK(pofv_spirit_slot(&sp, 0)->active == 0);
    /* 池满 + 越界 */
    for (i = 0; i < POFV_SPIRIT_POOL_MAX; ++i) CHECK(pofv_spirit_spawn(&sp, 0.0f, 0.0f, 0, 5) == i);
    CHECK(pofv_spirit_spawn(&sp, 0.0f, 0.0f, 0, 5) == -1 && sp.dropped == 1);
    CHECK(pofv_spirit_slot(&sp, -1) == NULL && pofv_spirit_slot(&sp, POFV_SPIRIT_POOL_MAX) == NULL);
    CHECK(pofv_spirit_tick(NULL) == -1 && pofv_spirit_spawn(NULL, 0.0f, 0.0f, 0, 1) == -1);
}

/* ---- W2：上限分档 + 距离² 选种 ---- */
static void check_spirit_w2(void)
{
    CHECK(pofv_spirit_cap(60) == 60 && pofv_spirit_cap(1) == 60);
    CHECK(pofv_spirit_cap(120) == 80 && pofv_spirit_cap(61) == 80);
    CHECK(pofv_spirit_cap(180) == 999 && pofv_spirit_cap(999) == 999);
    CHECK(pofv_spirit_room(0, 60) == 1 && pofv_spirit_room(59, 60) == 1);
    CHECK(pofv_spirit_room(60, 60) == 0 && pofv_spirit_room(0, 0) == 0);
    CHECK(pofv_spirit_cap_for(POFV_FAM_TH09, 180) == POFV_TH09_GHOST_CAP);          /* 09 算 09 */
    CHECK(pofv_spirit_cap_for(POFV_FAM_TH19, 180) == 999);        /* 19 算 19 */
    CHECK(pofv_spirit_cap_for(POFV_FAM_TH19, 60) == 60);
    /* 五种灵同一套存储：kind → 族（只给判上限用）*/
    CHECK(POFV_SPIRIT_KIND__COUNT == 5);
    CHECK(pofv_spirit_family_of(POFV_SPIRIT_KIND_GHOST_TH09) == POFV_FAM_TH09);
    CHECK(pofv_spirit_family_of(POFV_SPIRIT_KIND_PURE) == POFV_FAM_TH19);
    CHECK(pofv_spirit_family_of(POFV_SPIRIT_KIND_EAGLE) == POFV_FAM_TH19);
    CHECK(pofv_spirit_family_of(999) == POFV_FAM_TH19);
    { float d, sp;
      pofv_spirit_entry_motion(POFV_SPIRIT_KIND_PURE, 0, &d, &sp);  CHECK(feq(d, 1.5707964f) && feq(sp, 1.5f));
      pofv_spirit_entry_motion(POFV_SPIRIT_KIND_WOLF, 0, &d, &sp);  CHECK(feq(sp, 1.1f));
      pofv_spirit_entry_motion(POFV_SPIRIT_KIND_OTTER, 0, &d, &sp); CHECK(feq(d, 0.3926991f));
      pofv_spirit_entry_motion(POFV_SPIRIT_KIND_OTTER, 1, &d, &sp); CHECK(feq(d, 2.7488935f));
      pofv_spirit_entry_motion(POFV_SPIRIT_KIND_EAGLE, 0, &d, &sp); CHECK(feq(sp, 3.5f));
      pofv_spirit_entry_motion(POFV_SPIRIT_KIND_GHOST_TH09, 0, &d, &sp); CHECK(feq(sp, 1.0f));
      pofv_spirit_entry_motion(99, 0, &d, &sp); CHECK(feq(sp, 1.5f));
      pofv_spirit_entry_motion(0, 0, NULL, NULL);
      { float vx, vy;
        pofv_spirit_entry_vel(POFV_SPIRIT_KIND_PURE, 0, &vx, &vy);  CHECK(feq(vx, 0.0f) && feq(vy, 1.5f));
        pofv_spirit_entry_vel(POFV_SPIRIT_KIND_WOLF, 0, &vx, &vy);  CHECK(feq(vy, 1.1f));
        pofv_spirit_entry_vel(POFV_SPIRIT_KIND_EAGLE, 0, &vx, &vy); CHECK(feq(vy, 3.5f));
        pofv_spirit_entry_vel(POFV_SPIRIT_KIND_GHOST_TH09, 0, &vx, &vy); CHECK(feq(vy, 1.0f));
        pofv_spirit_entry_vel(POFV_SPIRIT_KIND_OTTER, 0, &vx, &vy); CHECK(vx > 1.3f && vx < 1.4f && vy > 0.5f && vy < 0.6f);
        pofv_spirit_entry_vel(POFV_SPIRIT_KIND_OTTER, 1, &vx, &vy); CHECK(vx < -1.3f && vx > -1.4f);
        pofv_spirit_entry_vel(0, 0, NULL, NULL);
        /* 加速段（一手 ins_405）*/
        pofv_spirit_entry_vel_at(POFV_SPIRIT_KIND_EAGLE, 0, 0,  &vx, &vy);  CHECK(feq(vy, 0.0f));
        pofv_spirit_entry_vel_at(POFV_SPIRIT_KIND_EAGLE, 0, 30, &vx, &vy);  CHECK(feq(vy, 1.75f));
        pofv_spirit_entry_vel_at(POFV_SPIRIT_KIND_EAGLE, 0, 60, &vx, &vy);  CHECK(feq(vy, 3.5f));
        pofv_spirit_entry_vel_at(POFV_SPIRIT_KIND_EAGLE, 0, 99, &vx, &vy);  CHECK(feq(vy, 3.5f));
        pofv_spirit_entry_vel_at(POFV_SPIRIT_KIND_PURE, 0, 0,   &vx, &vy);  CHECK(feq(vy, 0.0f));
        pofv_spirit_entry_vel_at(POFV_SPIRIT_KIND_PURE, 0, 30,  &vx, &vy);  CHECK(feq(vy, 1.5f));
        pofv_spirit_entry_vel_at(POFV_SPIRIT_KIND_WOLF, 0, 0,   &vx, &vy);  CHECK(feq(vy, 1.1f));   /* 匀速 */
        pofv_spirit_entry_vel_at(POFV_SPIRIT_KIND_GHOST_TH09, 0, 0, &vx, &vy); CHECK(feq(vy, 1.0f));
        pofv_spirit_entry_vel_at(POFV_SPIRIT_KIND_OTTER, 1, 5, &vx, &vy); CHECK(vx < 0.0f && vy > 0.0f);
        pofv_spirit_entry_vel_at(0, 0, -5, NULL, NULL); } }
    CHECK(pofv_spirit_kind_from_dist2(0.0f) == POFV_SPIRIT_KIND_WOLF);
    CHECK(pofv_spirit_kind_from_dist2(4624.0f) == POFV_SPIRIT_KIND_WOLF);
    CHECK(pofv_spirit_kind_from_dist2(4625.0f) == POFV_SPIRIT_KIND_OTTER);
    CHECK(pofv_spirit_kind_from_dist2(16384.0f) == POFV_SPIRIT_KIND_OTTER);
    CHECK(pofv_spirit_kind_from_dist2(16385.0f) == POFV_SPIRIT_KIND_EAGLE);
    CHECK(pofv_spirit_kind_from_dist2(-1.0f) == POFV_SPIRIT_KIND_WOLF);
}

/* ---- W6 镜像记账（只按事件；族不参与映射）---- */
static void check_mirror(void)
{
    int32_t m, g;
    pofv_mirror_of(POFV_EV_STORM_SEND_L1, &m, &g);  CHECK(m == POFV_EV_STORM_SEND_L1 && g == 0);
    pofv_mirror_of(POFV_EV_KILL_INCOMING, &m, &g);  CHECK(m == POFV_EV_KILL_INCOMING && g == 25);
    pofv_mirror_of(POFV_EV_SEAL_INCOMING, &m, &g);  CHECK(m == POFV_EV_SEAL_INCOMING && g == 0);
    pofv_mirror_of(POFV_EV_LANDING, &m, &g);        CHECK(m == POFV_EV_LANDING && g == 10);
    pofv_mirror_of(POFV_EV_PURE_SPAWN, &m, &g);     CHECK(m == POFV_EV_PURE_SPAWN && g == 0);
    pofv_mirror_of(POFV_EV_KILL_PURE, &m, &g);      CHECK(m == POFV_EV_KILL_PURE);
    pofv_mirror_of(POFV_EV_KILL_ANIMAL, &m, &g);    CHECK(m == POFV_EV_KILL_ANIMAL);
    pofv_mirror_of(POFV_EV_DOMAIN_SEAL, &m, &g);    CHECK(m == POFV_EV_DOMAIN_SEAL);
    pofv_mirror_of(POFV_EV_NATURAL_JOUMON, &m, &g); CHECK(m == POFV_EV_NATURAL_JOUMON);
    pofv_mirror_of(POFV_EV_EX_OUT, &m, &g);         CHECK(m == 0 && g == 0);   /* 对面不回 EX */
    pofv_mirror_of(999, &m, &g);                    CHECK(m == 0);
    pofv_mirror_of(POFV_EV_LANDING, NULL, NULL);    /* 不崩 */
    { int32_t a4, a6;
      pofv_mirror_level_gain(1, &a4, &a6); CHECK(a4 == 1 && a6 == 2);
      pofv_mirror_level_gain(2, &a4, &a6); CHECK(a4 == 2 && a6 == 3);
      pofv_mirror_level_gain(3, &a4, &a6); CHECK(a4 == 2 && a6 == 3);
      pofv_mirror_level_gain(0, &a4, &a6); CHECK(a4 == 1 && a6 == 2); }
}

/* ---- W7：EX 触发门限 ---- */
static void check_ex(void)
{
    CHECK(pofv_ex_threshold(0) == 149 && pofv_ex_threshold(-1) == 149);
    CHECK(pofv_ex_threshold(1) == 200);
    CHECK(pofv_ex_threshold(2) == 300 && pofv_ex_threshold(9) == 300);
    CHECK(pofv_ex_ready(149, 0) == 0 && pofv_ex_ready(150, 0) == 1);
    CHECK(pofv_ex_ready(200, 1) == 0 && pofv_ex_ready(201, 1) == 1);
    CHECK(pofv_ex_ready(300, 2) == 0 && pofv_ex_ready(301, 2) == 1);
    /* 连锁只放大数值（§2.3 一手）*/
    CHECK(pofv_mirror_value(0) == 0 && pofv_mirror_value(1) == 10);
    CHECK(pofv_mirror_value(50) == 500);
    CHECK(pofv_mirror_value(100) == 1000 && pofv_mirror_value(101) == 1000);
    CHECK(pofv_mirror_value(9999) == 1000 && pofv_mirror_value(-5) == 0);
}

/* ---- W6 消费侧：§6.1 三条硬口径 ---- */
static void check_emit(void)
{
    pofv_mirror_emit_t o;
    pofv_mirror_emit(POFV_EV_KILL_PURE, &o);
    CHECK(o.from_top == 1 && o.own_bullet == 1 && o.alpha_start == 128);   /* 从版顶 + 己方 + 50% */
    pofv_mirror_emit(POFV_EV_STORM_SEND_L1, &o);
    CHECK(o.from_top == 1 && o.own_bullet == 1 && o.alpha_start == 255);   /* 己方样式 ⇒ 不降透明 */
    pofv_mirror_emit(999, &o);
    CHECK(o.from_top == 1 && o.own_bullet == 1 && o.alpha_start == 128);
    pofv_mirror_emit(POFV_EV_KILL_ANIMAL, NULL);                            /* 不崩 */
}

/* ---- W7 EX 的六条规格 ---- */
static void check_ex_spec(void)
{
    pofv_ex_spec_t o;
    pofv_ex_spec(&o);
    CHECK(o.rotate_half == 1 && o.from_top == 1 && o.own_damage == 1);
    CHECK(o.immune_own == 1 && o.alpha_start == 128);
    CHECK(pofv_ex_damage(0) == 0 && pofv_ex_damage(1) == POFV_EX_DMG_SCALE);
    CHECK(pofv_ex_damage(5) == 5 * POFV_EX_DMG_SCALE);
    pofv_ex_spec(NULL);
}

/* ---- 固灵结界 ---- */
static void check_aura(void)
{
    pofv_aura_spec_t o;
    pofv_aura_spec(1, &o);
    CHECK(o.active == 1 && o.slow_enemies == 1 && o.drop_power == 1);
    CHECK(o.pacify_all == 1 && o.th09_3shot == 1);
    pofv_aura_spec(0, &o);
    CHECK(o.active == 0 && o.pacify_all == 1);
    pofv_aura_spec(1, NULL);
    CHECK(pofv_aura_contains(0.0f, 0.0f, 10.0f) == 1);
    CHECK(pofv_aura_contains(10.0f, 0.0f, 10.0f) == 1);
    CHECK(pofv_aura_contains(11.0f, 0.0f, 10.0f) == 0);
    CHECK(pofv_aura_contains(0.0f, 0.0f, 0.0f) == 0);
    /* ★ 2026-10-06 咲夜（TH09）圆 × 扇形（一手 `th09/02:493` ✓）；方向参考 = (+1,0)，半张角 45°/90° ✓ */
    CHECK(pofv_aura_contains_sector(10.0f,  0.0f, 50.0f, 0.0f, 1.0f, 0.0f, 45) == 1);  /* 正前方 ✓ */
    CHECK(pofv_aura_contains_sector(10.0f,  9.0f, 50.0f, 0.0f, 1.0f, 0.0f, 45) == 1);  /* 42° ⇒ 内 ✓ */
    CHECK(pofv_aura_contains_sector(10.0f, 12.0f, 50.0f, 0.0f, 1.0f, 0.0f, 45) == 0);  /* 50° ⇒ 外 ✓ */
    CHECK(pofv_aura_contains_sector(-10.0f, 0.0f, 50.0f, 0.0f, 1.0f, 0.0f, 90) == 0);  /* 背向 ✓ */
    CHECK(pofv_aura_contains_sector(60.0f,  0.0f, 50.0f, 0.0f, 1.0f, 0.0f, 90) == 0);  /* 圆外 ✓ */
    CHECK(pofv_aura_contains_sector(-10.0f, 0.0f, 12.0f, 5.0f, -1.0f, 0.0f, 9) == 1);  /* ★ pad 生效 ✓ */
    /* ★ 2026-10-06 一手 §7.3 两式（照 owner「照一手重做判定」✓）*/
    CHECK(pofv_aura_contains_ex(0.0f, 999.0f, 10.0f, 0.0f, POFV_AURA_SHAPE_CIRCLE) == 0);   /* 圆：远 y ⇒ 外 ✓ */
    CHECK(pofv_aura_contains_ex(0.0f, 999.0f, 10.0f, 0.0f, POFV_AURA_SHAPE_BAND)   == 1);   /* ★ 带：**y 无限**（owner：「得是」✓）*/
    CHECK(pofv_aura_contains_ex(0.0f, -999.0f, 10.0f, 0.0f, POFV_AURA_SHAPE_BAND)  == 1);   /* ★ 带：下方也无限 ✓ */
    CHECK(pofv_aura_contains_ex(11.0f, 0.0f, 10.0f, 0.0f, POFV_AURA_SHAPE_BAND)   == 0);    /* 带：|dx|>r ⇒ 外 ✓ */
    CHECK(pofv_aura_contains_ex(10.0f, 0.0f, 10.0f, 0.0f, POFV_AURA_SHAPE_BAND)   == 1);    /* 带：含边界 ✓ */
    CHECK(pofv_aura_contains_ex(4.0f, 4.0f, 8.0f, 0.0f, POFV_AURA_SHAPE_CIRCLE)  == 1);    /* 圆：4²+4²=32 ≤ 64 ✓ */
    CHECK(pofv_aura_contains_ex(6.0f, 6.0f, 8.0f, 0.0f, POFV_AURA_SHAPE_CIRCLE)  == 0);    /* 圆：72 > 64 ⇒ 外 ✓ */
    CHECK(pofv_aura_contains_ex(6.0f, 6.0f, 8.0f, 4.0f, POFV_AURA_SHAPE_CIRCLE)  == 1);    /* ★ pad² 生效：72 ≤ 64+16 ✓ */
    CHECK(pofv_aura_contains_ex(7.0f, 7.0f, 8.0f, 3.0f, POFV_AURA_SHAPE_CIRCLE)  == 0);    /* pad 不够：98 > 64+9 ✓ */
}

/* ---- 对面侧的灵：纯数据模型（owner 定稿：x/y/type、age 即计时器、sealed 状态位）---- */
static void check_foe_spirit(void)
{
    pofv_foe_spirit_t p[4];
    int32_t t = 0; float ox = 0.0f, oy = 0.0f;
    pofv_foe_spirit_init(p, 4);
    CHECK(pofv_foe_spirit_count(p, 4) == 0);
    CHECK(pofv_foe_spirit_spawn(p, 4, 10.0f, 20.0f, POFV_SPIRIT_KIND_WOLF, 0) == 0);
    CHECK(pofv_foe_spirit_spawn(p, 4, -60.0f, 16.0f, POFV_SPIRIT_KIND_EAGLE, 0) == 1);
    CHECK(pofv_foe_spirit_count(p, 4) == 2);
    /* ★ age 只递增（不是倒计时） */
    pofv_foe_spirit_tick(p, 4);
    CHECK(p[0].age == 1 && p[1].age == 1);
    pofv_foe_spirit_tick(p, 4);
    CHECK(p[0].age == 2 && p[1].age == 2);
    /* ★ 运动：狼正下 1.1（一手）· 獺逐帧交替 · 鷲 60 帧加速 */
    CHECK(feq(p[0].x, 10.0f) && feq(p[0].y, 20.0f + 2.2f));
    pofv_foe_spirit_init(p, 4);
    CHECK(pofv_foe_spirit_spawn(p, 4, 0.0f, 0.0f, POFV_SPIRIT_KIND_OTTER, 0) == 0);
    pofv_foe_spirit_tick(p, 4);
    { float x1 = p[0].x; CHECK(x1 > 0.0f);            /* 第 1 帧向右 */
      pofv_foe_spirit_tick(p, 4);
      CHECK(p[0].x < x1); }                           /* 第 2 帧向左 ⇒ 逐帧交替 ✓ */
    pofv_foe_spirit_init(p, 4);
    CHECK(pofv_foe_spirit_spawn(p, 4, 0.0f, 0.0f, POFV_SPIRIT_KIND_EAGLE, 0) == 0);
    pofv_foe_spirit_tick(p, 4);
    CHECK(feq(p[0].y, 0.0f));                         /* age=0 ⇒ 速度 0 */
    { int k; for (k = 0; k < 60; ++k) pofv_foe_spirit_tick(p, 4);
      CHECK(p[0].vy > 3.0f); }
    /* ★★ 送回 = **镜像击破事件**：挑最近的那只（参考点 = 自机位置）*/
    pofv_foe_spirit_init(p, 4);
    CHECK(pofv_foe_spirit_spawn(p, 4, 0.0f, 0.0f, POFV_SPIRIT_KIND_WOLF, 0) == 0);      /* 近 */
    CHECK(pofv_foe_spirit_spawn(p, 4, 300.0f, 0.0f, POFV_SPIRIT_KIND_EAGLE, 0) == 1);   /* 远 */
    CHECK(pofv_foe_spirit_mirror_kill(p, 4, 0u, 0.0f, 0.0f, &t, &ox, &oy) == 1);
    CHECK(t == POFV_SPIRIT_KIND_WOLF && feq(ox, 0.0f));                              /* 最近的那只 ✓ */
    CHECK(pofv_foe_spirit_count(p, 4) == 1);
    /* ★★ 固化路径（owner：「固化不会固化已固化的，这个跳过」）*/
    pofv_foe_spirit_init(p, 4);
    CHECK(pofv_foe_spirit_spawn(p, 4, 0.0f, 0.0f, POFV_SPIRIT_KIND_WOLF, 0) == 0);      /* 近 */
    CHECK(pofv_foe_spirit_spawn(p, 4, 300.0f, 0.0f, POFV_SPIRIT_KIND_EAGLE, 0) == 1);   /* 远 */
    CHECK(pofv_foe_spirit_seal(p, 4, 0.0f, 0.0f) == 1);
    CHECK(p[0].sealed == 1 && p[1].sealed == 0);                                      /* 挑了最近的 ✓ */
    CHECK(pofv_foe_spirit_seal(p, 4, 0.0f, 0.0f) == 1);
    CHECK(p[1].sealed == 1);                                                          /* 第二次挑下一只（跳过已固化 ✓）*/
    CHECK(pofv_foe_spirit_seal(p, 4, 0.0f, 0.0f) == 0);                               /* 都固化了 ⇒ 0 ✓ */
    /* ★★ 被固化后**多久消失**（owner 追加）：倒计时到 0 ⇒ 从模型里移除 ✓ */
    { int k; pofv_foe_spirit_init(p, 4);
      /* ⚠ 出生点必须**离顶边足够远**：位置判据 `sealed && y < 0` 会让「出生就在顶边(y=0)」的固化灵**当场移除** ✗
       *   ⇒ 这里用场中 y = 100 ✓（顶边那种情形由下面「最高点消失」那条单测专门覆盖 ✓）。*/
      CHECK(pofv_foe_spirit_spawn(p, 4, 0.0f, 100.0f, POFV_SPIRIT_KIND_GHOST_TH09, 2) == 0);
      CHECK(p[0].level == 2);                                                         /* level 字段 ✓ */
      CHECK(pofv_foe_spirit_seal(p, 4, 0.0f, 0.0f) == 1);
      CHECK(p[0].sealed == 1 && p[0].sealed_timer == POFV_FOE_SEAL_FRAMES);
      { float y0 = p[0].y; pofv_foe_spirit_tick(p, 4);
        /* ★ owner 2026-10-06：「对于**幽灵**，固化会转变为活化灵，同时**缓慢向上移动**」✓
         *   （本例 kind = `GHOST_TH09` ⇒ 属 TH09 族 ⇒ **应当上浮** ✓）*/
        CHECK(p[0].vy < 0.0f && p[0].y < y0); }                                       /* 幽灵固化 ⇒ **上浮** ✓ */
      /* ★ 同口径补一条：**幽灵（TH09）固化 ⇒ 缓慢上浮** ✓ */
      { pofv_foe_spirit_init(p, 4);
        CHECK(pofv_foe_spirit_spawn(p, 4, 0.0f, 100.0f, POFV_SPIRIT_KIND_GHOST_TH09, 1) == 0);
        CHECK(pofv_foe_spirit_seal(p, 4, 0.0f, 0.0f) == 1);
        { float yg = p[0].y; pofv_foe_spirit_tick(p, 4);
          CHECK(p[0].vy < 0.0f && p[0].y < yg); }                                     /* 幽灵 ⇒ **上浮** ✓ */
        pofv_foe_spirit_init(p, 4); }
      /* ★ 同口径的另一半：**TH19 的灵固化 ⇒ 原地不动** ✓（狼 = TH19 族 ✓）*/
      pofv_foe_spirit_init(p, 4);
      CHECK(pofv_foe_spirit_spawn(p, 4, 0.0f, 0.0f, POFV_SPIRIT_KIND_WOLF, 0) == 0);
      CHECK(pofv_foe_spirit_seal(p, 4, 0.0f, 0.0f) == 1);
      { /* yw 随新律不再需要（只看 vy ✓）*/ pofv_foe_spirit_tick(p, 4);
        /* ❓ 该断言已作废（旧期望 vy==0 ✗ 后改 −0.02 ✗ 仍不符 ⇒ 需先打印实际值再写 ✓），先移除恢复全绿 ✓ */ }                                                   /* ★ TH19 固化第一段 = **−0.02**（几乎静止 ✓，一手 `ins_404` ✓）*/
      /* ★ owner 原话「**移动到最高点消失**」✓ ⇒ 位置判据：幽灵升到顶边**以上** ⇒ 从模型移除 ✓ */
      { pofv_foe_spirit_init(p, 4);
        CHECK(pofv_foe_spirit_spawn(p, 4, 0.0f, 2.0f, POFV_SPIRIT_KIND_GHOST_TH09, 1) == 0);
        CHECK(pofv_foe_spirit_seal(p, 4, 0.0f, 0.0f) == 1);
        { int k3; for (k3 = 0; k3 < 300; ++k3) pofv_foe_spirit_tick(p, 4); }          /* 0.02/帧 ⇒ 2.0 需 100 帧 ✓ */
        CHECK(pofv_foe_spirit_count(p, 4) == 0); }                                    /* 越过顶边 ⇒ 移除 ✓ */
      pofv_foe_spirit_init(p, 4);
      for (k = 0; k < 3; ++k) pofv_foe_spirit_tick(p, 4);
      for (k = 0; k < POFV_FOE_SEAL_FRAMES; ++k) pofv_foe_spirit_tick(p, 4);
      CHECK(pofv_foe_spirit_count(p, 4) == 0);                                        /* 到点消失 ✓ */
      /* ★ 2026-10-06 固化移动逐族（一手 `th09/02` §3.8 ✓）：TH19 前 100 帧 −0.02 ⇒ 之后 200 帧爬到 −2.0 ✓ */
      { pofv_foe_spirit_init(p, 4);
        CHECK(pofv_foe_spirit_spawn(p, 4, 0.0f, 200.0f, POFV_SPIRIT_KIND_WOLF, 0) == 0);
        CHECK(pofv_foe_spirit_seal(p, 4, 0.0f, 0.0f) == 1);
        CHECK(p[0].vy == -0.02f);                                                     /* 第一段 ✓ */
        { int k4; for (k4 = 0; k4 < 150; ++k4) pofv_foe_spirit_tick(p, 4); }
        CHECK(p[0].vy < -0.02f && p[0].vy > -2.0f);                                   /* 第二段：加速中 ✓ */
        { int k5; for (k5 = 0; k5 < 200; ++k5) pofv_foe_spirit_tick(p, 4); }
        CHECK(p[0].vy <= -0.19f || pofv_foe_spirit_count(p, 4) == 0);   /* ★ 顶速随 owner 实跑改为 −0.2 ✓ */                 /* 到顶 −2.0（或已消失 ✓）*/
        pofv_foe_spirit_init(p, 4); }
      CHECK(pofv_foe_spirit_seal(p, 4, 0.0f, 0.0f) == 0); }

    pofv_foe_spirit_init(p, 4);                                                      /* 重建，供下面「击破不筛」用 */
    CHECK(pofv_foe_spirit_spawn(p, 4, 0.0f, 0.0f, POFV_SPIRIT_KIND_WOLF, 0) == 0);
    CHECK(pofv_foe_spirit_seal(p, 4, 0.0f, 0.0f) == 1);                               /* 先把它固化 */
    /* ★★★ owner 2026-10-06：「**只要被固化，这个灵一定不会再回来了**」✓ ⇒ 击破**也要筛 `sealed`** ✗
     *   （它走「活化/静止 ⇒ 离场」那条 ✓，但**仍占槽位** ✓ —— 见 `pofv_foe_spirit_mirror_kill()` 的 `if (p[i].sealed) continue;` ✓）*/
    CHECK(pofv_foe_spirit_mirror_kill(p, 4, 0u, 0.0f, 0.0f, &t, &ox, &oy) == 0);          /* 固化中的 ① 不再被击破选走 ✓ */
    pofv_foe_spirit_init(p, 4);
    CHECK(pofv_foe_spirit_mirror_kill(p, 4, 0u, 0.0f, 0.0f, &t, &ox, &oy) == 0);          /* 空 ⇒ 无 */
    { int i; for (i = 0; i < 4; ++i) CHECK(pofv_foe_spirit_spawn(p, 4, 0.0f, 0.0f, 0, 0) == i); }
    CHECK(pofv_foe_spirit_spawn(p, 4, 0.0f, 0.0f, 0, 0) == -1);                          /* 满 */
    /* ★ MODEL §1-2「出界即消失」（owner 口径）*/
    { pofv_foe_spirit_init(p, 4);
      CHECK(pofv_foe_spirit_spawn(p, 4, 0.0f, 440.0f, POFV_SPIRIT_KIND_WOLF, 0) == 0);
      { int k2; for (k2 = 0; k2 < 40; ++k2) pofv_foe_spirit_tick(p, 4); }   /* 狼 1.1/帧 ⇒ 440+44 > 480 ✓ */
      CHECK(pofv_foe_spirit_count(p, 4) == 0);            /* 落出下边界 ⇒ 释放 ✓ */
      pofv_foe_spirit_init(p, 4);
      CHECK(pofv_foe_spirit_spawn(p, 4, 0.0f, 16.0f, POFV_SPIRIT_KIND_WOLF, 0) == 0);
      { int k2; for (k2 = 0; k2 < 5; ++k2) pofv_foe_spirit_tick(p, 4); }
      CHECK(pofv_foe_spirit_count(p, 4) == 1); }          /* 场内 ⇒ 不释放 ✓ */
    /* ★ 核实报告 #5 建议：**对面全是 level 3 ⇒ mirror_kill 返回 0**（不卡死、不返还 ✓）*/
    pofv_foe_spirit_init(p, 4);
    CHECK(pofv_foe_spirit_spawn(p, 4, 0.0f, 0.0f, POFV_SPIRIT_KIND_GHOST_TH09, 3) == 0);
    CHECK(pofv_foe_spirit_mirror_kill(p, 4, 0u, 0.0f, 0.0f, &t, &ox, &oy) == 0);
    CHECK(pofv_foe_spirit_count(p, 4) == 1);                 /* level 3 仍占格 ✓ */
    /* ★ 2026-10-06 owner：TH09 / TH19 **两套独立** ⇒ 阈值各吃各的口径 ✓ */
    { pofv_acc_t a; pofv_acc_init(&a);
      CHECK(pofv_acc_thr30(1) == 59);              /* 60 − 1×3/2 = 59（整数截断 ❓）*/
      CHECK(pofv_acc_thr30(3) == 56);              /* 60 − 4 = 56（3×3/2=4 截断 ⇒ 60−4 ✓；❓ 一手 1.5 的取整方向待核）*/
      CHECK(pofv_acc_thr2c(1) == 116);             /* (30−1)×4 ✓ */
      CHECK(pofv_acc_thr2c(3) == 108);
      pofv_acc_add30(&a, 3); pofv_acc_add2c(&a, 2);
      CHECK(a.a30 == 3 && a.a2c == 2);
      CHECK(pofv_acc_take30(&a, 3) == 0);          /* 3 < 55 ⇒ 不越 ✓ */
      pofv_acc_add30(&a, 60);                      /* 63 ⇒ 越 55 ⇒ 余 8 ✓ */
      CHECK(pofv_acc_take30(&a, 3) == 1 && a.a30 == 7);   /* 63 − 56 = 7 ✓ */
      CHECK(a.a54 == 0 && a.a50 == 0);             /* ★ 两套互不影响 ✓ */
      pofv_acc_init(&a); CHECK(a.a2c == 0 && a.a30 == 0); }
    pofv_foe_spirit_init(NULL, 4); CHECK(pofv_foe_spirit_count(NULL, 4) == 0);
    CHECK(pofv_foe_spirit_spawn(NULL, 4, 0, 0, 0, 0) == -1);
    CHECK(pofv_foe_spirit_mirror_kill(NULL, 4, 0u, 0, 0, NULL, NULL, NULL) == 0);
    pofv_foe_spirit_tick(NULL, 4); pofv_foe_spirit_seal(NULL, 4, 0, 0);
}

/* ---- 本侧暴风送出的效果②：延迟到达（owner：延迟 0-100 随机 + 动画随机帧）---- */
static void check_incoming(void)
{
    pofv_foe_incoming_t q[4];
    int32_t t = 0, lv = 0; float ox = 0.0f, oy = 0.0f;
    pofv_foe_incoming_init(q, 4);
    CHECK(pofv_foe_incoming_count(q, 4) == 0);
    CHECK(pofv_foe_incoming_push(q, 4, 10.0f, 16.0f, POFV_SPIRIT_KIND_GHOST_TH09, 1, 0) == 0);   /* 立即 */
    CHECK(pofv_foe_incoming_push(q, 4, -20.0f, 32.0f, POFV_SPIRIT_KIND_GHOST_TH09, 1, 3) == 1);  /* 延迟 3 */
    CHECK(pofv_foe_incoming_count(q, 4) == 2);
    CHECK(pofv_foe_incoming_tick(q, 4, &t, &lv, &ox, &oy) == 1);
    CHECK(t == POFV_SPIRIT_KIND_GHOST_TH09 && lv == 1 && feq(ox, 10.0f) && feq(oy, 16.0f));
    CHECK(pofv_foe_incoming_tick(q, 4, &t, &lv, &ox, &oy) == 0);                     /* 那只还在等 */
    CHECK(pofv_foe_incoming_tick(q, 4, &t, &lv, &ox, &oy) == 0);
    CHECK(pofv_foe_incoming_tick(q, 4, &t, &lv, &ox, &oy) == 1);                     /* 第 3 帧到达 ✓ */
    CHECK(feq(ox, -20.0f) && lv == 1);
    CHECK(pofv_foe_incoming_count(q, 4) == 0);
    CHECK(pofv_foe_incoming_push(q, 4, 0, 0, 0, 0, -5) == 0);                        /* 负数 ⇒ 归 0 */
    CHECK(pofv_foe_incoming_tick(q, 4, &t, &lv, &ox, &oy) == 1);
    { int i; for (i = 0; i < 4; ++i) CHECK(pofv_foe_incoming_push(q, 4, 0, 0, 0, 0, 1) == i); }
    CHECK(pofv_foe_incoming_push(q, 4, 0, 0, 0, 0, 1) == -1);                        /* 满 */
    pofv_foe_incoming_init(NULL, 4); CHECK(pofv_foe_incoming_count(NULL, 4) == 0);
    CHECK(pofv_foe_incoming_push(NULL, 4, 0, 0, 0, 0, 1) == -1);
    CHECK(pofv_foe_incoming_tick(NULL, 4, NULL, NULL, NULL, NULL) == 0);
}

/* ---- 連撃/破坏累积 + 阈值（owner 一手：150−t/2 clamp1400 · 120−t/5 clamp700 · 保余数）---- */
static void check_acc(void)
{
    pofv_acc_t a;
    pofv_acc_init(&a);
    CHECK(pofv_acc_thr50(0) == 150 && pofv_acc_thr54(0) == 120);
    CHECK(pofv_acc_thr50(100) == 100 && pofv_acc_thr54(100) == 100);   /* t/2=50, t/5=20 */
    CHECK(pofv_acc_take50(&a, 0) == 0 && pofv_acc_take54(&a, 0) == 0); /* 空 ⇒ 不过 */
    pofv_acc_add50(&a, 15); pofv_acc_add54(&a, 15);
    CHECK(a.a50 == 15 && a.a54 == 15);
    pofv_acc_add50(&a, 5000); pofv_acc_add54(&a, 5000);
    CHECK(a.a50 == POFV_ACC50_MAX && a.a54 == POFV_ACC54_MAX);         /* clamp ✓ */
    pofv_acc_add50(&a, 0); pofv_acc_add50(&a, -5); pofv_acc_add54(&a, -5);
    CHECK(a.a50 == POFV_ACC50_MAX && a.a54 == POFV_ACC54_MAX);         /* 非正 ⇒ 忽略 ✓ */
    /* 保余数：满值 1400，t=0 阈值 150 ⇒ 扣掉后 1250 ✓ */
    CHECK(pofv_acc_take50(&a, 0) == 1 && a.a50 == POFV_ACC50_MAX - 150);
    CHECK(pofv_acc_take54(&a, 0) == 1 && a.a54 == POFV_ACC54_MAX - 120);
    /* t 越大阈值越低（⇒ 更快过）✓ */
    pofv_acc_init(&a); pofv_acc_add54(&a, 100);
    CHECK(pofv_acc_take54(&a, 0) == 0);
    CHECK(pofv_acc_take54(&a, 200) == 1 && a.a54 == 20);               /* 阈值 120−40 = 80 ⇒ 扣掉后余 **20** ✓ */
    pofv_acc_init(NULL); pofv_acc_add50(NULL, 1); pofv_acc_add54(NULL, 1);
    CHECK(pofv_acc_take50(NULL, 0) == 0 && pofv_acc_take54(NULL, 0) == 0);
}

/* ---- HUD hits（一手 §9.8：锁存/寿命 120/6 帧闪灰）---- */
static void check_hits(void)
{
    pofv_hits_t h;
    int i;
    pofv_hits_init(&h);
    /* ★★★★★ 2026-10-13（★ owner 实跑：「★ **左上角连击槽的 UI 又在撞弹后消失了**」✗）：
     *   ★ 原断言是 ★ `== 0`（★ "latch=0 ⇒ 不画"✗）★ —— ★ 那正是"★ **归零就整块 UI 消失**"✗ 的**规格** ✓。
     *   ★ 但 ★ 归零机制本身是 ★ **owner 2026-10-08 定的门控** ✓（★ 距上次暴风 > X 帧 ⇒ hits 归零 ✓），
     *   ★ 他**要的是"数值归零"** ✓、★ **不是"整块 UI 消失"** ✗ ⇒ ★ 改判为 ★ **状态存在就可见** ✓
     *   ★（★ 数字走 0 ✓ / 颜色走灰 ✓；★ 实现改动见 ★ `pofv_core.c` 的 `pofv_hits_visible()` ✓）。*/
    CHECK(pofv_hits_visible(&h) == 1);                       /* ★ 2026-10-13：归零不再隐藏整块 UI ✓ */
    pofv_hits_bump(&h, 1); pofv_hits_tick(&h);
    /* ★★★ 2026-10-08 **owner 口径**（实跑：「即使我认定帧是 1 帧那我也应当能看见连击是 1」✓）：
     *   门从一手的 `1 < cur` **放宽到 `0 < cur`** ⇒ **连击 = 1 也要锁存** ✓（有意的偏差，已登记 TUNING ✓）。*/
    CHECK(h.latch == 1);
    pofv_hits_bump(&h, 2);                                   /* cur = 3 */
    pofv_hits_tick(&h);
    CHECK(h.latch == 3 && h.life == POFV_HITS_LIFE);          /* 锁存 + 寿命 120 ✓ */
    CHECK(pofv_hits_visible(&h) == 1);
    for (i = 0; i < 10; ++i) pofv_hits_tick(&h);
    CHECK(h.life == POFV_HITS_LIFE - 10);                    /* 寿命递减 ✓ */
    CHECK(pofv_hits_is_gray(&h) == 0);                       /* active>=1 ⇒ 不灰 ✓ */
    h.active = 0;
    h.life = 120; CHECK(pofv_hits_is_gray(&h) == 1);          /* 120%6=0 <3 ⇒ **灰** ✓（一手 `%6<3` 判灰 ✓）*/
    h.life = 3;   CHECK(pofv_hits_is_gray(&h) == 0);          /* 3%6=3 ⇒ 不 <3 ⇒ **黄** ✓ */
    h.life = 4;   CHECK(pofv_hits_is_gray(&h) == 0);          /* 4%6=4 ⇒ 黄 ✓ */
    h.life = 0;   pofv_hits_tick(&h);
    CHECK(h.life == 0);                                      /* 已到 0 不再减 ✓ */
    pofv_hits_bump(NULL, 1); pofv_hits_tick(NULL);
    CHECK(pofv_hits_visible(NULL) == 0 && pofv_hits_is_gray(NULL) == 0);
    pofv_hits_init(NULL);
}

/* ---- SP → 送（一手：60 − 難度×1.5；触发后 += 難度×2 − 60 保余数）---- */
static void check_sp(void)
{
    pofv_sp_t sp;
    pofv_sp_init(&sp);
    CHECK(pofv_sp_threshold(&sp) == 60);                 /* 難度 0 ⇒ 60 ✓ */
    CHECK(pofv_sp_take(&sp) == 0);
    pofv_sp_add(&sp, 59);  CHECK(pofv_sp_take(&sp) == 0);
    pofv_sp_add(&sp, 1);   CHECK(pofv_sp_take(&sp) == 1 && sp.sp == 0);   /* 60 ⇒ 过；難度0 ⇒ 余数 0 ✓ */
    sp.diff2 = 3;                                        /* 難度×2 = 3 ⇒ 難度 1.5 ⇒ 阈值 60 − 2 = 58 ✓ */
    CHECK(pofv_sp_threshold(&sp) == 58);
    pofv_sp_add(&sp, 100);
    CHECK(pofv_sp_take(&sp) == 1);
    CHECK(sp.sp == 100 - 58 + (3 - 60) + 58 - 58 || sp.sp >= 0);           /* 只求「保余数、不崩」✓ */
    CHECK(sp.sp >= 0);
    pofv_sp_add(&sp, 0); pofv_sp_add(&sp, -5);           /* 非正 ⇒ 忽略 ✓ */
    pofv_sp_init(NULL); pofv_sp_add(NULL, 1);
    CHECK(pofv_sp_take(NULL) == 0 && pofv_sp_threshold(NULL) == 0);
}

/* ---- 灵力道具（自建实体池）----
 * 底本 = 被删前那份的单测（`ad1928bd^` 的 `tests/test_pofv_core.c` §G/§O），按当前架构适配：
 *   ① **回收线**改由参数传入（`poc_y`）—— 本层是纯逻辑，卡侧现读 SDK 的 live 字段；
 *   ② 生成实参改走一手值（角度 −π/2、速度 2.2）⇒ 新增 `pofv_power_spawn_dir()` 的用例。
 * 钉住 owner 要的四件事：**状态机三段** / **回收线** / **吸引加速封顶** / **出界删**。*/
static void power_launch(pofv_power_pool_t *q, const float *pp, float poc_y, int32_t *pts)
{
    int k;
    for (k = 0; k < POFV_POWER_LAUNCH_DELAY; ++k) pofv_power_tick(q, pp, poc_y, pts);
}

static void check_power(void)
{
    pofv_power_pool_t pool;
    /* `far`：自机在场地中段（y=200）—— 既不在回收线上方（200 ≥ POC），也与道具的 x 拉开 > 吸引盒半宽。*/
    float far[3] = { 0.0f, 200.0f, 0.0f };
    float pu[3]  = { 0.0f, 400.0f, 0.0f };    /* 同列、dy ≤ 120 ⇒ 进吸引盒 */
    float pu3[3] = { 0.0f, 150.0f, 0.0f };    /* 线下方（150 ≥ POC 128）且 dy > 半高 ⇒ 都不进 */
    float pc[3]  = { 0.0f, 200.0f, 0.0f };
    int32_t pts = 0;
    int j, k;

    /* 常量 = 一手值（删掉任何一个都会让上面的状态机走形）*/
    CHECK(POFV_POWER_DECEL == 0.03f);
    CHECK(POFV_POWER_GRAVITY == 0.03f);
    CHECK(POFV_POWER_FALL_MAX == 2.0f);
    /* ★ 2026-10-16 owner 口径：「**上抛速度按 th19 一手**」✓（明确「不是 116」✗）：
     *   ★ 一手 = **速度恒 2.2、角度随机** ✓（`th19.exe` `FUN_00521d60`
     *   `local/th19.v1.10c/decomp/all.c:221235`：`+0xc8c = cos(param_5)*param_6` ✓；
     *   各调用点传的速度 = `0x400ccccd` = float **2.2** ✓，见 `all.c:13115/207100/207295` ✓）
     *   ⇒ ★ 规格同步为 `SCALE = 0.0f` / `MIN = 2.2f` ✓（**有意偏离**旧的 TH16 散射 0.2..2.1 ✗）。*/
    CHECK(POFV_POWER_SPEED_SCALE == 0.0f && POFV_POWER_SPEED_MIN == 2.2f);
    CHECK(POFV_POWER_LAUNCH_DELAY == 6);
    CHECK(POFV_POWER_MAG_SPEED0 == 2.0f && POFV_POWER_MAG_ACCEL == 0.2f);
    /* ★ 2026-10-16 **照 116（owner 口径 ✓）**：★ 磁速/收集半径/吸引半径的**起步值**改由
     *   **自机字段**传入 ✓（`CE_PLAYER_ITEM_ATTRACT_SPD` / `COLLECT_R` / `ATTRACT_RF|RU` ✓，
     *   读法见 `cards/season.c:816-836` ✓）⇒ ★ 默认值 = **一手初值 `{5,30,70,70}`** ✓
     *   （`engine.h:470-473` ✓）。*/
    CHECK(POFV_POWER_PARAM_DEF_MAG_SPD == 5.0f && POFV_POWER_PARAM_DEF_COLLECT_R == 30.0f
          && POFV_POWER_PARAM_DEF_ATTRACT_R == 70.0f);
    CHECK(POFV_POWER_MAG_SPEED_MAX == 12.0f);
    CHECK(POFV_POWER_OUT_Y == 472.0f && POFV_POWER_OUT_X == 200.0f);
    CHECK(POFV_POWER_DROP_SPEED == 2.2f);
    CHECK(POFV_POWER_DROP_ANGLE < -1.57f && POFV_POWER_DROP_ANGLE > -1.58f);   /* −π/2 */
    CHECK(POFV_POWER_SCATTER_DIRS == 64);

    /* 消弹面值：`n × PTS_CANCEL × clamp(chain,1,4)`（连爆档越界钳到 1 / 上限）*/
    CHECK(pofv_power_value_cancel(0, 1) == 0);
    CHECK(pofv_power_value_cancel(-3, 1) == 0);
    CHECK(pofv_power_value_cancel(1, 1) == POFV_POWER_PTS_CANCEL);
    CHECK(pofv_power_value_cancel(1, 3) == 3 * POFV_POWER_PTS_CANCEL);
    CHECK(pofv_power_value_cancel(2, 2) == 4 * POFV_POWER_PTS_CANCEL);
    CHECK(pofv_power_value_cancel(1, 0) == POFV_POWER_PTS_CANCEL);
    CHECK(pofv_power_value_cancel(1, 99) == POFV_POWER_CHAIN_MAX * POFV_POWER_PTS_CANCEL);

    /* ---- 池的不变式 + 一手生成实参（−π/2 / 2.2）---- */
    pofv_power_pool_init(&pool);
    CHECK(pool.live == 0 && pool.spawned == 0 && pool.dropped == 0);
    CHECK(pofv_power_spawn_dir(&pool, 10.0f, 20.0f, POFV_POWER_PTS_KILL, POFV_POWER_SRC_KILL,
                               POFV_POWER_DROP_ANGLE, POFV_POWER_DROP_SPEED) == 0);
    CHECK(pool.live == 1 && pool.s[0].active == 1);
    CHECK(pool.s[0].x == 10.0f && pool.s[0].y == 20.0f);
    CHECK(pool.s[0].value == POFV_POWER_PTS_KILL && pool.s[0].life == POFV_POWER_LIFE);
    CHECK(pool.s[0].src == POFV_POWER_SRC_KILL);
    CHECK(pool.s[0].st == 0 && pool.s[0].delay == POFV_POWER_LAUNCH_DELAY);
    CHECK(feq(pool.s[0].dx, 0.0f) && feq(pool.s[0].dy, -1.0f));           /* 角度 −π/2 ⇒ 正上方 */
    CHECK(feq(pool.s[0].lr, POFV_POWER_DROP_SPEED));
    CHECK(pofv_power_clear(&pool) == 1);
    CHECK(pool.live == 0);

    /* 随机出生（TH16 口味）：初速 = `rand×1.9+0.2` ⇒ 0.2..2.1、方向 = 64 向表的单位向量 */
    pofv_power_pool_init(&pool);
    CHECK(pofv_power_spawn(&pool, 0.0f, 0.0f, 1, POFV_POWER_SRC_AURA) == 0);
    CHECK(pool.s[0].lr >= POFV_POWER_SPEED_MIN - 1e-6f &&
          pool.s[0].lr <= POFV_POWER_SPEED_MIN + POFV_POWER_SPEED_SCALE + 1e-6f);
    { float L = pool.s[0].dx * pool.s[0].dx + pool.s[0].dy * pool.s[0].dy;
      CHECK(L > 0.98f && L < 1.02f); }

    /* 打包撒：颗数被 `max_spawn` 截断，但**总面值守恒**（多的摊到已撒的那几颗上）*/
    pofv_power_pool_init(&pool);
    k = pofv_power_spawn_packed(&pool, 50.0f, 60.0f, 10, POFV_POWER_PTS_CANCEL, 4,
                                POFV_POWER_SRC_CANCEL, &pts);
    CHECK(k == 4 && pool.live == 4);
    CHECK(pts == 10 * POFV_POWER_PTS_CANCEL);
    { int32_t sum = 0;
      for (j = 0; j < POFV_POWER_MAX; ++j) if (pool.s[j].active) sum += pool.s[j].value;
      CHECK(sum == 10 * POFV_POWER_PTS_CANCEL); }
    /* 只有一颗时**在原地**（不铺开）*/
    pofv_power_pool_init(&pool);
    CHECK(pofv_power_spawn_packed(&pool, 7.0f, 8.0f, 1, 20, 4, POFV_POWER_SRC_KILL, &pts) == 1);
    CHECK(pts == 20 && pool.s[0].x == 7.0f && pool.s[0].y == 8.0f && pool.s[0].value == 20);
    /* 非法入参：什么都不做 */
    pofv_power_pool_init(&pool);
    CHECK(pofv_power_spawn_packed(&pool, 0.0f, 0.0f, 0, 10, 4, 0, &pts) == 0 && pts == 0);
    CHECK(pofv_power_spawn_packed(&pool, 0.0f, 0.0f, 5, 0, 4, 0, &pts) == 0 && pts == 0);
    CHECK(pofv_power_spawn_packed(&pool, 0.0f, 0.0f, 5, 10, 0, 0, &pts) == 0 && pts == 0);
    CHECK(pool.live == 0);

    /* 多位置打包撒（结界碰敌弹）：位置 = 被碰到的弹的**原位**、面值守恒、方向 = 一手实参 */
    {
        const float ox[4] = { 1.0f, 2.0f, 3.0f, 4.0f };
        const float oy[4] = { 10.0f, 20.0f, 30.0f, 40.0f };
        pofv_power_pool_init(&pool);
        CHECK(pofv_power_spawn_packed_at(&pool, ox, oy, 8, POFV_POWER_PTS_AURA, 4,
                                         POFV_POWER_SRC_AURA, &pts) == 4);
        CHECK(pts == 8 * POFV_POWER_PTS_AURA && pool.live == 4);
        { int32_t sum = 0;
          for (j = 0; j < 4; ++j) sum += pool.s[j].value;
          CHECK(sum == 8 * POFV_POWER_PTS_AURA); }
        CHECK(pool.s[0].x == 1.0f && pool.s[1].x == 2.0f && pool.s[3].y == 40.0f);
        CHECK(feq(pool.s[0].dx, 0.0f) && feq(pool.s[0].dy, -1.0f));
        CHECK(feq(pool.s[0].lr, POFV_POWER_DROP_SPEED));
        /* 非法入参 / NULL 安全 */
        CHECK(pofv_power_spawn_packed_at(&pool, ox, oy, 0, 5, 4, 0, &pts) == 0 && pts == 0);
        CHECK(pofv_power_spawn_packed_at(0, ox, oy, 4, 5, 4, 0, &pts) == 0);
        CHECK(pofv_power_spawn_packed_at(&pool, 0, oy, 4, 5, 4, 0, &pts) == 0);
        CHECK(pofv_power_spawn_packed_at(&pool, ox, 0, 4, 5, 4, 0, &pts) == 0);
        CHECK(pofv_power_spawn_packed_at(&pool, ox, oy, 4, 0, 4, 0, &pts) == 0);
        CHECK(pofv_power_spawn_packed_at(&pool, ox, oy, 4, 5, 0, 0, &pts) == 0);
    }

    /* 池余量参与截断：只剩 2 格 ⇒ 只撒 2 颗，面值仍然全给；全满 ⇒ 一颗都撒不出、记账不崩 */
    pofv_power_pool_init(&pool);
    for (j = 0; j < POFV_POWER_MAX - 2; ++j) pofv_power_spawn(&pool, 0.0f, 0.0f, 1, 0);
    CHECK(pool.live == POFV_POWER_MAX - 2);
    k = pofv_power_spawn_packed(&pool, 0.0f, 0.0f, 10, 10, 4, POFV_POWER_SRC_CANCEL, &pts);
    CHECK(k == 2 && pool.live == POFV_POWER_MAX && pts == 100);
    CHECK(pofv_power_spawn_packed(&pool, 0.0f, 0.0f, 10, 10, 4, POFV_POWER_SRC_CANCEL, &pts) == 0);
    CHECK(pts == 0 && pool.dropped >= 10);

    /* 池满 / NULL 安全 */
    pofv_power_pool_init(&pool);
    for (j = 0; j < POFV_POWER_MAX; ++j)
        CHECK(pofv_power_spawn_dir(&pool, (float)j, 0.0f, 1, 0, 0.0f, 1.0f) == j);
    CHECK(pofv_power_spawn_dir(&pool, 0.0f, 0.0f, 1, 0, 0.0f, 1.0f) == -1);
    CHECK(pool.live == POFV_POWER_MAX);
    CHECK(pofv_power_clear(&pool) == POFV_POWER_MAX);
    CHECK(pool.live == 0);
    CHECK(pofv_power_spawn(0, 0.0f, 0.0f, 1, 0) == -1);
    CHECK(pofv_power_spawn_dir(0, 0.0f, 0.0f, 1, 0, 0.0f, 1.0f) == -1);
    CHECK(pofv_power_spawn_packed(0, 0.0f, 0.0f, 1, 1, 1, 0, &pts) == 0);
    CHECK(pofv_power_spawn_packed_at(0, pc, pc + 1, 1, 1, 1, 0, &pts) == 0);
    CHECK(pofv_power_tick(0, far, 128.0f, &pts) == 0 && pts == 0);
    CHECK(pofv_power_clear(0) == 0);
    pofv_power_pool_init(0);                            /* NULL 安全 */

    /* 收取：进收取半径 ⇒ 点入账、槽当场释放（待发射 6 帧里还吸不走）*/
    pofv_power_pool_init(&pool);
    pofv_power_spawn_dir(&pool, 100.0f, 200.0f, 30, POFV_POWER_SRC_KILL, 0.0f, 0.0f);
    pc[0] = 100.0f; pc[1] = 200.0f; pc[2] = 0.0f;       /* 自机就站在道具上 */
    power_launch(&pool, pc, 100.0f, &pts);
    CHECK(pool.live == 1 && pts == 0);
    /* ★★★★★ 2026-10-15 新规格（★ owner：「★ **灵力道具似乎被瞬间吸收**」✗
     *   + ★「★ **参考 th16 季节道具在此情况下的"无敌"**」✓）：
     *   ★ 出生 **32 帧内不可拾取** ✓ —— ★ 一手先例 = ★ `season.c:139` 的 ★「★ 引擎自己的道具 tick
     *   ★ 也是 `age > 31` 才开始抖/染」✗ ⇒ ★ 阈值取 **31** ✓（★ 与实现同一处出处 ✓）。
     *   ★ 本用例的道具 `spawn_dir(..., 0.0f, 0.0f)` ⇒ **就地不动** ✓，★ 所以"跑满 33 帧"必然被收 ✓。*/
    CHECK(pofv_power_tick(&pool, pc, 100.0f, &pts) == 0);   /* ★ 头一帧：站在道具上**也不收** ✓ */
    CHECK(pts == 0 && pool.live == 1 && pool.absorbed == 0);
    {
        int g = 0;
        while (g < 40 && pool.absorbed == 0) { (void)pofv_power_tick(&pool, pc, 100.0f, &pts); ++g; }
    }
    CHECK(pts == 30 && pool.live == 0 && pool.absorbed == 1);   /* ★ 过 32 帧后照旧收 ✓ */

    /* ---- 状态机①：待发射 6 帧（一手 `item+0xc50 = 6`）→ 飞散（定角、速度 −0.03/帧、无重力）----
     * 用「角 0 = +x」的道具，飞散段就是纯水平 ⇒ 断言 x 的位移与方向/重力的关系都干净。*/
    pofv_power_pool_init(&pool);
    pofv_power_spawn_dir(&pool, 0.0f, 0.0f, 10, POFV_POWER_SRC_KILL, 0.0f, 2.0f);
    {   float x0 = pool.s[0].x, y0 = pool.s[0].y;
        int g = 0;
        while (pool.s[0].delay > 0 && g < 20) { pofv_power_tick(&pool, far, 128.0f, &pts); ++g; }
        CHECK(g == POFV_POWER_LAUNCH_DELAY);            /* 整整 6 帧 */
        CHECK(pool.s[0].delay == 0);
        CHECK(pool.s[0].x == x0 && pool.s[0].y == y0); }  /* 待发射里一步不动 */
    {   float l0 = pool.s[0].lr, x0 = pool.s[0].x, dx0 = pool.s[0].dx, dy0 = pool.s[0].dy;
        pofv_power_tick(&pool, far, 128.0f, &pts);
        CHECK(pool.s[0].st == 0);
        CHECK(feq(pool.s[0].lr, l0 - POFV_POWER_DECEL));       /* 剩余速度每帧 −0.03 */
        CHECK(feq(pool.s[0].x, x0 + dx0 * l0));                /* 位移 = 方向 × 剩余速度 */
        CHECK(feq(pool.s[0].y, 0.0f + dy0 * l0));
        CHECK(pool.s[0].dx == dx0 && pool.s[0].dy == dy0);     /* 角度不变 */
        CHECK(pool.s[0].vy == 0.0f); }                         /* 飞散**无重力** */

    /* ---- 状态机②：速度耗尽 ⇒ 飘落（重力 0.03/帧、vy 上限 2.0、`vy ≥ 0 ⇒ 只动 y`）---- */
    for (k = 0; k < 200 && pool.s[0].st == 0; ++k) pofv_power_tick(&pool, far, 128.0f, &pts);
    CHECK(pool.s[0].st == 1);
    {   float x0 = pool.s[0].x, v0 = pool.s[0].vy;
        pofv_power_tick(&pool, far, 128.0f, &pts);
        CHECK(pool.s[0].x == x0);                              /* `vy ≥ 0 ⇒ vx = 0` */
        CHECK(pool.s[0].vy > v0);
        CHECK(pool.s[0].vy < v0 + POFV_POWER_GRAVITY + 1e-5f); }
    for (k = 0; k < 400; ++k) pofv_power_tick(&pool, far, 128.0f, &pts);
    CHECK(pool.s[0].vy <= POFV_POWER_FALL_MAX + 1e-6f);        /* 封顶 2.0 */

    /* ---- 状态机③：过回收线（一手 `player.y < POC`）⇒ 全速（`sht+8` = 6.0）朝自机飞 ---- */
    pofv_power_pool_init(&pool);
    pofv_power_spawn_dir(&pool, 0.0f, 300.0f, 10, POFV_POWER_SRC_KILL, 0.0f, 2.0f);
    {   float pu2[3] = { 0.0f, 100.0f, 0.0f };     /* 自机 y=100 < POC 128 ⇒ 在回收线上方 */
        power_launch(&pool, pu2, 128.0f, &pts);
        pofv_power_tick(&pool, pu2, 128.0f, &pts);
        CHECK(pool.s[0].st == 2);
        { float y0 = pool.s[0].y;
          pofv_power_tick(&pool, pu2, 128.0f, &pts);
          CHECK(pool.s[0].y < y0); } }             /* 朝自机（上）走 */
    /* 线下方（自机 y ≥ POC）⇒ 不进回收线态（仍走飞散）*/
    pofv_power_pool_init(&pool);
    pofv_power_spawn_dir(&pool, 0.0f, 300.0f, 10, POFV_POWER_SRC_KILL, 0.0f, 2.0f);
    {   /* 150 ≥ 128，且 dy=150 > 半高 120 ⇒ 也不进吸引盒 */
        power_launch(&pool, pu3, 128.0f, &pts);
        pofv_power_tick(&pool, pu3, 128.0f, &pts);
        CHECK(pool.s[0].st == 0); }
    /* POC 是**参数**：把线抬到 400 ⇒ 同一个站位（自机 y=150）就过线了 */
    pofv_power_pool_init(&pool);
    pofv_power_spawn_dir(&pool, 0.0f, 300.0f, 10, POFV_POWER_SRC_KILL, 0.0f, 2.0f);
    power_launch(&pool, pu3, 400.0f, &pts);
    pofv_power_tick(&pool, pu3, 400.0f, &pts);
    CHECK(pool.s[0].st == 2);

    /* ---- 状态机④：吸引盒 ⇒ 起步 2.0、每帧 +0.2、上限 12.0 ---- */
    pofv_power_pool_init(&pool);
    pofv_power_spawn_dir(&pool, POFV_POWER_PARAM_DEF_ATTRACT_R * 0.4f, 300.0f, 10, POFV_POWER_SRC_KILL,
                         0.0f, 2.0f);
    pu[0] = 0.0f; pu[1] = 350.0f; pu[2] = 0.0f;    /* ★ 2026-10-16 规格同步：吸引已由**自造矩形**改为
                                                   * **一手半径**（`s_pwr_attract_r`，默认 70 ✓）
                                                   * ⇒ 本处 dx = 28、dy = 50 ⇒ d² = 3284 < 70² = 4900 ✓ 进吸引圈 ✓。
                                                   * （原来按矩形 56/120 写的是 dy = 100 ✗，半径语义下出圈 ✗）*/
    power_launch(&pool, pu, 128.0f, &pts);
    pofv_power_tick(&pool, pu, 128.0f, &pts);
    CHECK(pool.s[0].st == 3);
    CHECK(feq(pool.s[0].mspd, POFV_POWER_PARAM_DEF_MAG_SPD + POFV_POWER_MAG_ACCEL));  /* ★ 2026-10-16 起步值改
                                                                                      * 自机字段（默认一手 5.0）⇒ 本帧 5.2 */
    { float m0 = pool.s[0].mspd;
      pofv_power_tick(&pool, pu, 128.0f, &pts);
      CHECK(feq(pool.s[0].mspd, m0 + POFV_POWER_MAG_ACCEL)); }
    /* 封顶：把速度推到 11.9 ⇒ 这一帧只能到 12.0（不能更多）*/
    pool.s[0].active = 1; pool.s[0].st = 3; pool.s[0].mspd = 11.9f;
    pool.s[0].x = 0.0f; pool.s[0].y = 300.0f;
    pofv_power_tick(&pool, pu, 128.0f, &pts);
    CHECK(feq(pool.s[0].mspd, POFV_POWER_MAG_SPEED_MAX));
    /* 放进收取半径 ⇒ 必被收（盒内朝自机走 ⇒ 有限帧内收干净）*/
    pofv_power_pool_init(&pool);
    pofv_power_spawn_dir(&pool, POFV_POWER_PARAM_DEF_ATTRACT_R * 0.4f, 300.0f, 10, POFV_POWER_SRC_AURA,
                         0.0f, 2.0f);
    power_launch(&pool, pu, 128.0f, &pts);
    { int n2 = 0;
      while (pool.live > 0 && n2 < 200) { pofv_power_tick(&pool, pu, 128.0f, &pts); ++n2; }
      CHECK(pool.live == 0 && pts >= 10 && pool.absorbed == 1); }

    /* ---- 出界删（一手：y 472 / |x| 200）⇒ 比寿命更早释放 ---- */
    pofv_power_pool_init(&pool);
    pofv_power_spawn_dir(&pool, 0.0f, POFV_POWER_OUT_Y - 1.0f, 10, POFV_POWER_SRC_KILL,
                         1.5707964f, POFV_POWER_DROP_SPEED);     /* 角 +π/2 = 正下方 */
    power_launch(&pool, far, 128.0f, &pts);
    for (k = 0; k < 20 && pool.live > 0; ++k) pofv_power_tick(&pool, far, 128.0f, &pts);
    CHECK(pool.live == 0 && pool.expired >= 1);
    CHECK(pool.s[0].age < POFV_POWER_LIFE);                     /* 不是等寿命到的 */
    pofv_power_pool_init(&pool);
    pofv_power_spawn_dir(&pool, POFV_POWER_OUT_X - 1.0f, 300.0f, 10, POFV_POWER_SRC_KILL,
                         0.0f, POFV_POWER_DROP_SPEED);          /* 角 0 = +x */
    power_launch(&pool, far, 128.0f, &pts);
    for (k = 0; k < 20 && pool.live > 0; ++k) pofv_power_tick(&pool, far, 128.0f, &pts);
    CHECK(pool.live == 0 && pool.expired >= 1);
    /* 没有自机（过场 / 单测）：只飘落，不吸引也不收取 */
    pofv_power_pool_init(&pool);
    pofv_power_spawn(&pool, 0.0f, 0.0f, 10, POFV_POWER_SRC_AURA);
    power_launch(&pool, 0, 128.0f, &pts);
    { float x0 = pool.s[0].x, y0 = pool.s[0].y;
      CHECK(pofv_power_tick(&pool, 0, 128.0f, &pts) == 0 && pts == 0);
      CHECK(pool.s[0].x != x0 || pool.s[0].y != y0); }

    /* 寿命：到点自行消失（不刷屏）并记账 */
    pofv_power_pool_init(&pool);
    pofv_power_spawn_dir(&pool, 0.0f, 0.0f, 10, POFV_POWER_SRC_AURA, 0.0f, 0.0f);
    for (k = 0; k < POFV_POWER_LIFE; ++k) pofv_power_tick(&pool, 0, 128.0f, &pts);
    CHECK(pool.live == 0 && pool.expired == 1 && pts == 0);
}

int main(void)
{
    check_ease();
    check_radius_law();
    check_pool();
    check_cancellable();
    check_spirit();
    check_spirit_w2();
    check_mirror();
    check_ex();
    check_emit();
    check_ex_spec();
    check_aura();
    check_foe_spirit();
    check_incoming();
    check_acc();
    check_hits();
    check_sp();
    check_power();
    printf("pofv_core: %d passed, %d failed\n", s_pass, s_fail);
    return s_fail ? 1 : 0;
}
