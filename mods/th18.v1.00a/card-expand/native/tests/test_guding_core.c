/* test_guding_core.c —— guding_core.c 的主机单测（make test-host）。 */
#include <stdio.h>
#include <string.h>
#include "../cards/guding_core.h"

static int s_fail, s_pass;
#define CHECK(cond) do { if (cond) ++s_pass; else { ++s_fail; printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } } while (0)

static void set_slot(uint8_t *slots, unsigned i, int32_t hp, int32_t time, const char *life, const char *timeout)
{
    uint8_t *s = slots + i * GD_SLOT_STRIDE;
    memset(s, 0, GD_SLOT_STRIDE);
    memcpy(s + GD_SLOT_HP, &hp, 4);
    memcpy(s + GD_SLOT_TIME, &time, 4);
    if (life)    strncpy((char *)s + GD_SLOT_SUB_LIFE, life, GD_SLOT_SUB_LEN);
    if (timeout) strncpy((char *)s + GD_SLOT_SUB_TIMEOUT, timeout, GD_SLOT_SUB_LEN);
}

int main(void)
{
    uint8_t slots[8 * GD_SLOT_STRIDE];
    for (unsigned i = 0; i < 8; ++i) set_slot(slots, i, -1, 0, 0, 0);

    /* 活动槽：第一个 hp > -1 && time > 0 */
    CHECK(gd_find_active_slot(slots, 8) == -1);
    set_slot(slots, 0, 2100, 2100, "BossCard1", "BossCard1");             /* st01 Boss1：非符 */
    CHECK(gd_find_active_slot(slots, 8) == 0);
    CHECK(!gd_slot_is_final(slots));
    set_slot(slots, 0, 0, 2100, "Boss2", "Boss2");                        /* BossCard1：非终符 */
    CHECK(gd_find_active_slot(slots, 8) == 0 && !gd_slot_is_final(slots));
    set_slot(slots, 0, 0, 2100, "BossDead", "BossDead");                  /* BossCard2：终符 */
    CHECK(gd_slot_is_final(slots));
    set_slot(slots, 0, -1, 0, "BossDead", "BossDead");                    /* 超时后引擎把 hp 写 -1：不再活动 */
    CHECK(gd_find_active_slot(slots, 8) == -1);
    set_slot(slots, 0, 0, 0, "BossDead", "BossDead");                     /* time 0 = 无超时：不算活动槽 */
    CHECK(gd_find_active_slot(slots, 8) == -1);

    /* Extra 道中：槽 1 是整体计时 "MBossDead"，槽 0 才是当前符卡；第一个活动槽优先 */
    set_slot(slots, 1, 0, 2400, "MBossDead", "MBossDead");
    set_slot(slots, 0, 0, 3600, "MBoss2", "MBoss2");                      /* MBossCard1：非终符 */
    CHECK(gd_find_active_slot(slots, 8) == 0);
    CHECK(!gd_slot_is_final(slots + 0 * GD_SLOT_STRIDE));
    set_slot(slots, 0, 0, 3600, "MBossDead", "MBossDead");                /* MBossCard3：终符 */
    CHECK(gd_slot_is_final(slots));
    set_slot(slots, 0, -1, 0, 0, 0);                                      /* 槽 0 清了 → 槽 1（整体计时）成为活动槽 */
    CHECK(gd_find_active_slot(slots, 8) == 1);

    /* 名字只填一边也认；0x40 满长不越界 */
    set_slot(slots, 0, 0, 100, "", "BossDead");
    CHECK(gd_slot_is_final(slots));
    set_slot(slots, 0, 0, 100, "BossDead", "");
    CHECK(gd_slot_is_final(slots));
    {
        char longname[GD_SLOT_SUB_LEN + 1];
        memset(longname, 'x', GD_SLOT_SUB_LEN); longname[GD_SLOT_SUB_LEN] = 0;
        memcpy(longname + GD_SLOT_SUB_LEN - 4, "Dead", 4);               /* "Dead" 顶到 0x40 末尾、无终止符 */
        set_slot(slots, 0, 0, 100, longname, longname);
        CHECK(gd_slot_is_final(slots));
        memset(longname, 'x', GD_SLOT_SUB_LEN);
        set_slot(slots, 0, 0, 100, longname, longname);
        CHECK(!gd_slot_is_final(slots));
    }
    CHECK(!gd_slot_is_final(slots + 5 * GD_SLOT_STRIDE));               /* 空槽 */

    /* 状态机与倍率 */
    gd_state_t s; memset(&s, 0, sizeof s);
    CHECK(gd_bullet_damage(&s, 8) == 8 && s.bullets == 0);
    CHECK(gd_damage_cap(&s, 90) == 0);
    CHECK(gd_update(&s, 0) == 0 && !s.active);
    CHECK(gd_update(&s, 1) == 1 && s.active && s.frames == 1);
    CHECK(gd_update(&s, 1) == 0 && s.frames == 2);
    CHECK(gd_bullet_damage(&s, 8) == 16 && s.bullets == 1);
    CHECK(gd_bullet_damage(&s, 0) == 0);
    CHECK(gd_damage_cap(&s, 90) == 180 && gd_damage_cap(&s, 160) == 320);
    CHECK(gd_update(&s, 0) == -1 && !s.active && s.frames == 2);
    CHECK(gd_bullet_damage(&s, 8) == 8);
    printf("test_guding_core: %d passed, %d failed\n", s_pass, s_fail);
    return s_fail ? 1 : 0;
}
