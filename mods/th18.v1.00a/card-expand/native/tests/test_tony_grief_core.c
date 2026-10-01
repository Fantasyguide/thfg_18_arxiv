/* 东尼的悲恸（id 86）纯逻辑单测：速收判据（收取 vs 超时 / 用时 / 中途作废）、段边界锁定、落笔与释放。
 *   cc -std=c11 -Wall -Wextra -Werror -o t tests/test_tony_grief_core.c cards/tony_grief_core.c cards/guding_core.c && ./t
 * 槽签名那一段自己搭一份假的 interrupts 数组（8 × 0x88），与 cards/animal.c 的算式对拍。 */
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "../cards/tony_grief_core.h"
#include "../cards/guding_core.h"

static int fails;
#define CHECK(cond, ...) do { if (!(cond)) { printf("FAIL %s:%d ", __FILE__, __LINE__); \
    printf(__VA_ARGS__); printf("\n"); fails++; } } while (0)

/* 默认输入：符卡不进行、boss 在跑第 1 段（sig 固定）、目标不存在 */
static tg_input_t base_input(void)
{
    tg_input_t in;
    memset(&in, 0, sizeof in);
    in.have_boss = 1;
    in.boss_id = 7;
    in.boss_sig = 0x1111;
    return in;
}

/* 把状态推到「正在打一张符卡」：喂 active + bonus 的帧 */
static void enter_spell(tg_state_t *s, int32_t elapsed)
{
    tg_input_t in = base_input();
    tg_out_t o;
    in.spell_active = 1;
    in.spell_bonus = 1;
    in.spell_elapsed = elapsed;
    o = tg_frame(s, &in);
    CHECK(!o.captured, "符卡进行中不该判速收");
}

static void test_capture_fast(void)
{
    tg_state_t s;
    tg_input_t in;
    tg_out_t o;

    tg_reset(&s);
    enter_spell(&s, 1);
    enter_spell(&s, 900);

    /* 下降沿：bit0=0、bit1 还在、没超时、用时 900 ≤ 960；同一帧段边界（非符）⇒ 直接锁定 */
    in = base_input();
    in.spell_active = 0;
    in.spell_bonus = 1;
    in.spell_timed_out = 0;
    in.spell_elapsed = 900;
    in.boss_sig = 0x2222;                    /* 新段 */
    o = tg_frame(&s, &in);
    CHECK(o.captured == 1, "16 s 内收下该判速收");
    CHECK(o.captured_ms == 900, "用时该是 900，得到 %d", o.captured_ms);
    CHECK(o.armed == 1, "同帧的段边界该锁定");
    CHECK(o.want_write == 1, "锁定当帧就该落笔");
    CHECK(s.armed_n == 1 && s.quick == 1, "统计 quick=%u armed=%u", s.quick, s.armed_n);

    /* 下一帧：目标还在、签名没变 ⇒ 继续补写 */
    in = base_input();
    in.boss_sig = 0x2222;
    in.target_present = 1;
    in.target_sig_now = 0x2222;
    o = tg_frame(&s, &in);
    CHECK(o.want_write == 1 && !o.released, "段内该继续补写");

    /* 段结束（签名变了）⇒ 收手 */
    in = base_input();
    in.boss_sig = 0x3333;
    in.target_present = 1;
    in.target_sig_now = 0x3333;
    o = tg_frame(&s, &in);
    CHECK(o.released == 1 && o.want_write == 0, "段结束该收手");
    CHECK(s.target_live == 0, "目标该释放");

    /* 再下一帧不该还写 */
    in = base_input();
    o = tg_frame(&s, &in);
    CHECK(o.want_write == 0, "释放后不该再写");
}

static void test_capture_too_slow(void)
{
    tg_state_t s;
    tg_input_t in;
    tg_out_t o;

    tg_reset(&s);
    enter_spell(&s, 5);
    enter_spell(&s, TG_MAX_FRAMES + 1);      /* 超过 16 s */

    in = base_input();
    in.spell_active = 0;
    in.spell_bonus = 1;
    in.spell_elapsed = TG_MAX_FRAMES + 1;
    in.boss_sig = 0x2222;
    o = tg_frame(&s, &in);
    CHECK(!o.captured && !o.armed, "超过 16 s 不该触发");

    /* 正好 960 是含端点的 */
    tg_reset(&s);
    enter_spell(&s, 1);
    enter_spell(&s, TG_MAX_FRAMES);
    in = base_input();
    in.spell_active = 0;
    in.spell_bonus = 1;
    in.spell_elapsed = TG_MAX_FRAMES;
    in.boss_sig = 0x2222;
    o = tg_frame(&s, &in);
    CHECK(o.captured == 1 && o.armed == 1, "正好 960 帧该算进（口径含端点）");
}

static void test_capture_timeout_and_voided(void)
{
    tg_state_t s;
    tg_input_t in;
    tg_out_t o;

    /* 超时：bit7 置（bit1 可能还在，超时 <60 帧时引擎不清它）⇒ 必须靠 bit7 排除 */
    tg_reset(&s);
    enter_spell(&s, 1);
    enter_spell(&s, 300);
    in = base_input();
    in.spell_active = 0;
    in.spell_bonus = 1;                      /* 故意留着 bit1，模拟 elapsed<60 的超时 */
    in.spell_timed_out = 1;
    in.spell_elapsed = 300;
    in.boss_sig = 0x2222;
    o = tg_frame(&s, &in);
    CHECK(!o.captured && !o.armed, "超时不该触发（bit7 置）");

    /* 中途作废（玩家死亡 / 110/111 灵击 / 神之宣告清了 bit1，之后一直没再亮）*/
    tg_reset(&s);
    enter_spell(&s, 1);
    {
        tg_input_t v = base_input();
        v.spell_active = 1;                  /* 还在打，但奖励位被清了 */
        v.spell_bonus = 0;
        v.spell_elapsed = 500;
        tg_frame(&s, &v);
    }
    in = base_input();
    in.spell_active = 0;
    in.spell_bonus = 0;
    in.spell_elapsed = 500;
    in.boss_sig = 0x2222;
    o = tg_frame(&s, &in);
    CHECK(!o.captured && !o.armed, "奖励位中途被清 ⇒ 不算收取");

    /* 新一张符卡重置锁存：上一张作废，这一张收下 ⇒ 触发 */
    tg_reset(&s);
    enter_spell(&s, 1);
    {
        tg_input_t v = base_input();
        v.spell_active = 1;
        v.spell_bonus = 0;
        v.spell_elapsed = 400;
        tg_frame(&s, &v);
    }
    enter_spell(&s, 1);                      /* 上升沿：新符卡 */
    enter_spell(&s, 200);
    in = base_input();
    in.spell_active = 0;
    in.spell_bonus = 1;
    in.spell_elapsed = 200;
    in.boss_sig = 0x2222;
    o = tg_frame(&s, &in);
    CHECK(o.captured == 1 && o.armed == 1, "新一张符卡收下该触发");
}

static void test_boundary_not_spell_and_final(void)
{
    tg_state_t s;
    tg_input_t in;
    tg_out_t o;

    /* 速收之后的下一个段边界是**符卡**（罕见）⇒ 不锁定，窗口留着；再下一个非符边界才锁 */
    tg_reset(&s);
    enter_spell(&s, 1);
    enter_spell(&s, 100);
    in = base_input();
    in.spell_active = 0;
    in.spell_bonus = 1;
    in.spell_elapsed = 100;
    in.boss_sig = 0x2222;
    o = tg_frame(&s, &in);
    CHECK(o.armed == 1, "第一个边界（非符）该锁定");

    /* 换一个：速收那一帧还没有段边界（boss 签名没变）⇒ 只武装窗口；
     * 随后第一个段边界是**符卡**（罕见）⇒ 不锁定、窗口留着；再下一个非符边界才锁。*/
    tg_reset(&s);
    enter_spell(&s, 1);
    enter_spell(&s, 100);
    in = base_input();                       /* boss_sig 仍是 base 的 0x1111 ⇒ 无边界 */
    in.spell_active = 0;
    in.spell_bonus = 1;
    in.spell_elapsed = 100;
    o = tg_frame(&s, &in);
    CHECK(o.captured == 1 && !o.armed, "速收那帧没有段边界时只武装窗口");
    CHECK(s.pending == 1, "窗口该开着");

    in = base_input();
    in.spell_active = 1;                     /* 第一个边界：新段是符卡 */
    in.spell_bonus = 1;
    in.spell_elapsed = 1;
    in.boss_sig = 0x2222;
    o = tg_frame(&s, &in);
    CHECK(!o.armed, "新段是符卡时不该锁定");
    CHECK(s.pending == 1, "窗口该留着等下一个非符");

    /* 符卡打完，下一个边界是非符 ⇒ 锁定 */
    in = base_input();
    in.spell_active = 0;
    in.spell_bonus = 1;
    in.spell_elapsed = 200;
    in.boss_sig = 0x3333;
    o = tg_frame(&s, &in);
    CHECK(o.armed == 1, "再下一个非符边界该锁定");

    /* 死亡段（槽名含 Dead）⇒ 不锁定 */
    tg_reset(&s);
    enter_spell(&s, 1);
    enter_spell(&s, 100);
    in = base_input();
    in.spell_active = 0;
    in.spell_bonus = 1;
    in.spell_elapsed = 100;
    in.boss_sig = 0x2222;
    in.boss_final = 1;                       /* 终符之后是 BossDead 死亡脚本 */
    o = tg_frame(&s, &in);
    CHECK(!o.armed && s.pending == 1, "死亡段不该被写血（窗口留着）");
}

static void test_pending_expiry(void)
{
    tg_state_t s;
    tg_input_t in;
    tg_out_t o;
    int i;

    tg_reset(&s);
    enter_spell(&s, 1);
    enter_spell(&s, 100);
    in = base_input();
    in.spell_active = 0;
    in.spell_bonus = 1;
    in.spell_elapsed = 100;
    /* 没有段边界（boss 签名不变）*/
    o = tg_frame(&s, &in);
    CHECK(o.captured == 1 && !o.armed, "没有段边界时先只武装窗口");
    CHECK(s.pending == 1, "窗口该开着");

    for (i = 0; i < (int)TG_WAIT_FRAMES + 2; ++i) {
        in = base_input();
        o = tg_frame(&s, &in);
        if (o.expired) break;
    }
    CHECK(o.expired == 1, "窗口该到点放弃");
    CHECK(s.pending == 0, "到点后窗口该关");

    /* 窗口关了以后再来的段边界不该被写 */
    in = base_input();
    in.boss_sig = 0x9999;
    o = tg_frame(&s, &in);
    CHECK(!o.armed && !o.want_write, "窗口关了以后不该再锁定");
}

static void test_target_boss_gone(void)
{
    tg_state_t s;
    tg_input_t in;
    tg_out_t o;

    tg_reset(&s);
    enter_spell(&s, 1);
    enter_spell(&s, 100);
    in = base_input();
    in.spell_active = 0;
    in.spell_bonus = 1;
    in.spell_elapsed = 100;
    in.boss_sig = 0x2222;
    o = tg_frame(&s, &in);
    CHECK(o.armed == 1, "先锁定");

    /* boss 没了（target_present = 0）⇒ 释放，不再写 */
    in = base_input();
    in.boss_sig = 0;                         /* 也已经没有活动槽 */
    in.have_boss = 0;
    in.target_present = 0;
    o = tg_frame(&s, &in);
    CHECK(o.released == 1 && !o.want_write, "boss 消失该释放");
}

static void test_active_slot(void)
{
    uint8_t buf[TG_SLOT_COUNT * GD_SLOT_STRIDE];
    int32_t sig_a, sig_b;
    int fin = 0;

    memset(buf, 0, sizeof buf);
    /* 空槽：hp_value = -1（引擎的「空槽」写法）*/
    for (int i = 0; i < TG_SLOT_COUNT; ++i)
        memset(buf + i * GD_SLOT_STRIDE, 0xff, 4);
    CHECK(tg_active_slot(buf, TG_SLOT_COUNT, &fin) == 0, "全空槽该返回 0");

    /* 槽 0：hp=100 time=200 sub_life = "Boss2" */
    {
        uint8_t *sl = buf + 0;
        int32_t hp = 100, tm = 200;
        memcpy(sl + GD_SLOT_HP, &hp, 4);
        memcpy(sl + GD_SLOT_TIME, &tm, 4);
        memcpy(sl + GD_SLOT_SUB_LIFE, "Boss2", 6);
    }
    sig_a = tg_active_slot(buf, TG_SLOT_COUNT, &fin);
    CHECK(sig_a != 0, "活动槽该给出非 0 签名");
    CHECK(fin == 0, "Boss2 不是死亡段");
    CHECK(tg_active_slot(buf, TG_SLOT_COUNT, &fin) == sig_a, "同一槽该是同一个签名");

    /* 换成死亡段名 ⇒ 签名变 + final 置位 */
    memcpy(buf + GD_SLOT_SUB_LIFE, "BossDead", 9);
    sig_b = tg_active_slot(buf, TG_SLOT_COUNT, &fin);
    CHECK(sig_b != sig_a, "换了子程序名该换签名");
    CHECK(fin == 1, "BossDead 该判成死亡段");

    /* only time > 0 counts（time = 0 的槽不算活动）*/
    {
        int32_t tm = 0;
        memcpy(buf + GD_SLOT_TIME, &tm, 4);
    }
    CHECK(tg_active_slot(buf, TG_SLOT_COUNT, &fin) == 0, "time=0 的槽不算活动槽");
}

int main(void)
{
    test_capture_fast();
    test_capture_too_slow();
    test_capture_timeout_and_voided();
    test_boundary_not_spell_and_final();
    test_pending_expiry();
    test_target_boss_gone();
    test_active_slot();
    printf(fails ? "tony_grief_core: %d FAILED\n" : "tony_grief_core: ok\n", fails);
    return fails ? 1 : 0;
}
