/* 火球术（id 83）纯逻辑单测：弹道 / 命中判据 / 爆炸倒计时 / 出界。
 *   cc -std=c11 -Wall -Wextra -Werror -o t tests/test_fireball_core.c cards/fireball_core.c && ./t */
#include <stdio.h>

#include "../cards/fireball_core.h"

static int fails;
#define CHECK(cond, ...) do { if (!(cond)) { printf("FAIL %s:%d ", __FILE__, __LINE__); \
    printf(__VA_ARGS__); printf("\n"); fails++; } } while (0)

/* 发射：从自机上方 FB_SPAWN_DY 起飞，每帧向上 FB_SPEED */
static void test_launch_and_flight(void)
{
    fb_state_t s = { 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    fb_launch(&s, 10.0f, 400.0f);
    CHECK(s.active, "发射后应处于飞行态");
    CHECK(s.x == 10.0f, "x 应为自机 x，实为 %.1f", (double)s.x);
    CHECK(s.y == 400.0f + FB_SPAWN_DY, "y 应为自机 y + SPAWN_DY，实为 %.1f", (double)s.y);

    float y0 = s.y;
    for (int i = 1; i <= 10; ++i) {
        fb_step_t st = fb_step(&s, 0);
        CHECK(!st.hit && !st.missed && !st.done, "第 %d 帧不该结束", i);
        CHECK(s.y == y0 - FB_SPEED * i, "第 %d 帧 y 应为 %.1f，实为 %.1f", i, (double)(y0 - FB_SPEED * i), (double)s.y);
        CHECK(s.x == 10.0f, "x 不该变（直上）");
    }
    CHECK(s.frames == 10, "帧数应为 10，实为 %u", s.frames);
}

/* 拖尾每 FL_TRAIL_EVERY 帧一个 */
static void test_trail_cadence(void)
{
    fb_state_t s = { 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    fb_launch(&s, 0.0f, 400.0f);
    int trails = 0;
    for (int i = 1; i <= 20; ++i) trails += fb_step(&s, 0).trail;
    CHECK(trails == 20 / FL_TRAIL_EVERY, "20 帧应留 %d 个拖尾，实为 %d", 20 / FL_TRAIL_EVERY, trails);
}

/* 命中：就地爆炸，接着 FL_BLAST_FRAMES 帧各一个伤害源，然后结束 */
static void test_hit_then_blast(void)
{
    fb_state_t s = { 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    fb_launch(&s, 8.0f, 400.0f);
    fb_step(&s, 0);
    float before_y = s.y;

    fb_step_t st = fb_step(&s, 1);
    CHECK(st.hit, "这一帧应判命中");
    CHECK(!st.done, "命中那帧还不算结束（要炸 %d 帧）", FL_BLAST_FRAMES);
    CHECK(!s.active, "命中后火球不再飞");
    CHECK(s.ex == 8.0f && s.ey == before_y - FB_SPEED, "爆点应是命中那帧推进后的火球坐标");
    CHECK(s.blast_left == (uint32_t)FL_BLAST_FRAMES, "爆炸帧数应为 %d，实为 %u", FL_BLAST_FRAMES, s.blast_left);

    int dmg_frames = 0, total = 0;
    for (int i = 0; i < FL_BLAST_FRAMES; ++i) {
        st = fb_step(&s, 0);
        CHECK(st.blast_dmg == FL_BLAST_DMG, "第 %d 个爆炸帧伤害应为 %d，实为 %d", i, FL_BLAST_DMG, st.blast_dmg);
        dmg_frames++; total += st.blast_dmg;
        CHECK(st.done == (i == FL_BLAST_FRAMES - 1), "只有最后一个爆炸帧才 done（i = %d）", i);
    }
    CHECK(dmg_frames == FL_BLAST_FRAMES, "伤害帧数应为 %d，实为 %d", FL_BLAST_FRAMES, dmg_frames);
    CHECK(total == FL_BLAST_FRAMES * FL_BLAST_DMG, "总名义伤害应为 %d，实为 %d", FL_BLAST_FRAMES * FL_BLAST_DMG, total);
    CHECK(fb_step(&s, 0).done, "炸完之后再 step 仍应报 done（不会赖在持续态里）");
}

/* 爆炸期间再判命中也不会重新点火（火球已经没了）*/
static void test_no_double_ignite(void)
{
    fb_state_t s = { 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    fb_launch(&s, 0.0f, 400.0f);
    fb_step(&s, 1);
    for (int i = 0; i < FL_BLAST_FRAMES; ++i) {
        fb_step_t st = fb_step(&s, 1);
        CHECK(!st.hit, "爆炸帧 %d 不该再判一次命中", i);
    }
}

/* 出界：什么都不炸，当帧结束 */
static void test_miss_offscreen(void)
{
    fb_state_t s = { 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    fb_launch(&s, 0.0f, 400.0f);
    fb_step_t st = { 0, 0, 0, 0, 0 };
    int frames = 0;
    while (frames < 1000 && !st.done) { st = fb_step(&s, 0); frames++; }
    CHECK(st.missed, "飞出顶边应报 missed");
    CHECK(st.done && !st.blast_dmg, "空了就结束、不留伤害");
    CHECK(fb_offscreen(s.x, s.y), "结束时应已判出界，实为 y = %.1f", (double)s.y);
    /* 400 + SPAWN_DY 出发、每帧 FB_SPEED，越过上边框外 FB_EXIT_MARGIN 所需帧数 */
    int want = (int)((400.0f + FB_SPAWN_DY + FB_EXIT_MARGIN) / FB_SPEED) + 1;
    CHECK(frames == want, "出界帧数应为 %d，实为 %d", want, frames);
}

/* 出界判据本身：四条边都查（这颗火球只会直上，但以防万一——弹道以后要是改了也不会飞在场外不消失）*/
static void test_offscreen_all_edges(void)
{
    CHECK(!fb_offscreen(0.0f, 224.0f), "场地正中不算出界");
    CHECK(!fb_offscreen(-FB_FIELD_X_HALF, 0.0f), "左上角在界内");
    CHECK(!fb_offscreen(FB_FIELD_X_HALF, FB_FIELD_H), "右下角在界内");
    /* 每条边：margin 之内还活着、之外就死 */
    CHECK(!fb_offscreen(0.0f, -FB_EXIT_MARGIN + 1.0f), "顶边外 margin 之内还活着");
    CHECK(fb_offscreen(0.0f, -FB_EXIT_MARGIN - 1.0f), "越过顶边 margin 应出界");
    CHECK(!fb_offscreen(0.0f, FB_FIELD_H + FB_EXIT_MARGIN - 1.0f), "底边外 margin 之内还活着");
    CHECK(fb_offscreen(0.0f, FB_FIELD_H + FB_EXIT_MARGIN + 1.0f), "越过底边 margin 应出界");
    CHECK(!fb_offscreen(-FB_FIELD_X_HALF - FB_EXIT_MARGIN + 1.0f, 224.0f), "左边外 margin 之内还活着");
    CHECK(fb_offscreen(-FB_FIELD_X_HALF - FB_EXIT_MARGIN - 1.0f, 224.0f), "越过左边 margin 应出界");
    CHECK(!fb_offscreen(FB_FIELD_X_HALF + FB_EXIT_MARGIN - 1.0f, 224.0f), "右边外 margin 之内还活着");
    CHECK(fb_offscreen(FB_FIELD_X_HALF + FB_EXIT_MARGIN + 1.0f, 224.0f), "越过右边 margin 应出界");
}

/* 被外力搬到场外（不管哪一边）也会当帧自毁，不会赖在持续态里 */
static void test_teleported_out_dies(void)
{
    struct { float x, y; const char *where; } spots[] = {
        { -1000.0f, 224.0f, "左" }, { 1000.0f, 224.0f, "右" },
        { 0.0f, -1000.0f, "上" },   { 0.0f, 1000.0f, "下" },
    };
    for (unsigned i = 0; i < sizeof spots / sizeof spots[0]; ++i) {
        fb_state_t s = { 0, 0, 0, 0, 0, 0, 0, 0, 0 };
        fb_launch(&s, 0.0f, 400.0f);
        s.x = spots[i].x; s.y = spots[i].y + FB_SPEED;   /* step 会先推进一格 */
        fb_step_t st = fb_step(&s, 0);
        CHECK(st.missed && st.done, "被搬到%s边外时应当帧自毁", spots[i].where);
        CHECK(!s.active && !st.blast_dmg, "自毁不该留下伤害");
    }
}

/* 保险：坐标被外力弄成不动时，FB_MAX_FRAMES 之后也会收摊 */
static void test_timeout(void)
{
    fb_state_t s = { 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    fb_launch(&s, 0.0f, 400.0f);
    fb_step_t st = { 0, 0, 0, 0, 0 };
    for (int i = 0; i < FB_MAX_FRAMES && !st.done; ++i) {
        st = fb_step(&s, 0);
        s.y = 400.0f;                        /* 每帧把它按回原处：永远出不了界 */
    }
    CHECK(st.done && st.missed, "超时应当 missed + done");
    CHECK(s.frames == (uint32_t)FB_MAX_FRAMES, "应在第 %d 帧收摊，实为 %u", FB_MAX_FRAMES, s.frames);
}

/* 命中判据：圆内 / 圆外 / 边界 */
static void test_in_range(void)
{
    CHECK(fb_in_range(0.0f, 0.0f, 0.0f, 0.0f), "同点应命中");
    CHECK(fb_in_range(0.0f, 0.0f, FB_HIT_R - 1.0f, 0.0f), "半径内应命中");
    CHECK(!fb_in_range(0.0f, 0.0f, FB_HIT_R, 0.0f), "正好在半径上不算（严格小于）");
    CHECK(!fb_in_range(0.0f, 0.0f, FB_HIT_R + 1.0f, 0.0f), "半径外不命中");
    CHECK(!fb_in_range(0.0f, 0.0f, FB_HIT_R * 0.8f, FB_HIT_R * 0.8f), "对角线上 1.13R 不该命中（别写成方框）");
    CHECK(fb_in_range(100.0f, -50.0f, 100.0f + FB_HIT_R * 0.5f, -50.0f - FB_HIT_R * 0.5f), "平移后同样成立");
}

int main(void)
{
    test_launch_and_flight();
    test_trail_cadence();
    test_hit_then_blast();
    test_no_double_ignite();
    test_miss_offscreen();
    test_offscreen_all_edges();
    test_teleported_out_dies();
    test_timeout();
    test_in_range();
    printf("test_fireball_core: %s\n", fails ? "FAIL" : "OK");
    return fails ? 1 : 0;
}
