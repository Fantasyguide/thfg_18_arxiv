/* 抽取器测试：伪造一块 12 MB 的「进程镜像」，按 th06nc.h 的 RVA 放值，看读出来对不对。
 * 判别式：每个值都非默认、坐标做过 -192 平移、空槽不入表。 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../extract.h"
#include "../th06nc.h"

#define IMG_SIZE 0xC00000
static uint8_t *img;
static void put_f32(uint64_t rva, float v)   { memcpy(img + rva, &v, 4); }
static void put_i32(uint64_t rva, int32_t v) { memcpy(img + rva, &v, 4); }
static void put_i16(uint64_t rva, int16_t v) { memcpy(img + rva, &v, 2); }
static void put_u16(uint64_t rva, uint16_t v){ memcpy(img + rva, &v, 2); }
static void put_u64(uint64_t rva, uint64_t v){ memcpy(img + rva, &v, 8); }

int main(void)
{
    ap_world_t *w = calloc(1, sizeof *w);
    img = calloc(1, IMG_SIZE);
    assert(img && w);

    /* 帧号 = BulletManager 的 ZunTimer.current */
    put_i32(RVA_BULLET_MANAGER + 4, 1234);
    /* 自机：关卡区局部 (192, 384) → 协议 (0, 384) */
    put_f32(RVA_G_PLAYER + OFF_POSITION_X, 192.0f + 30.0f);
    put_f32(RVA_G_PLAYER + OFF_POSITION_Y, 384.0f);
    put_f32(RVA_G_PLAYER + OFF_HITBOX_RADIUS, 1.25f);
    img[RVA_G_PLAYER + OFF_IS_FOCUSED] = 1;
    img[RVA_G_PLAYER + OFF_PLAYER_STATE] = 3;
    put_f32(RVA_G_PLAYER + OFF_CHAR_DATA + 0, 5.0f);
    put_f32(RVA_G_PLAYER + OFF_CHAR_DATA + 4, 2.5f);
    img[RVA_LIVES] = 4; img[RVA_BOMBS] = 2;
    put_u16(RVA_POWER, 96);
    put_u64(RVA_SCORE, 123456789ull);
    put_i32(RVA_GRAZE_IN_STAGE, 77);
    put_i32(RVA_SPELL_ACTIVE, 1);
    img[RVA_BOMB_IN_USE] = 0;
    /* 可动区 (8,16)+(368,416) */
    put_f32(RVA_MOVEMENT_AREA + 0, 8.0f);  put_f32(RVA_MOVEMENT_AREA + 4, 16.0f);
    put_f32(RVA_MOVEMENT_AREA + 8, 368.0f); put_f32(RVA_MOVEMENT_AREA + 12, 416.0f);

    /* 弹：槽 0 飞行中；槽 1 空（state 0，不入表）；槽 639 出现动画（state 3，入表但 state 透传）*/
    {
        uint64_t b0 = RVA_G_BULLETS + 0 * BULLET_SIZE, b639 = RVA_G_BULLETS + 639 * BULLET_SIZE;
        put_i16(b0 + OFF_B_STATE, 1);
        put_f32(b0 + OFF_B_POS, 192.0f + 10.0f); put_f32(b0 + OFF_B_POS + 4, 100.0f);
        put_f32(b0 + OFF_B_VELOCITY, 0.5f);      put_f32(b0 + OFF_B_VELOCITY + 4, 3.0f);
        put_f32(b0 + OFF_B_GRAZE_SIZE, 16.0f);   put_f32(b0 + OFF_B_GRAZE_SIZE + 4, 12.0f);
        img[b0 + OFF_B_IS_GRAZED] = 1;
        put_i16(b639 + OFF_B_STATE, 3);
        put_f32(b639 + OFF_B_POS, 192.0f - 50.0f); put_f32(b639 + OFF_B_POS + 4, 200.0f);
        put_f32(b639 + OFF_B_GRAZE_SIZE, 8.0f);    put_f32(b639 + OFF_B_GRAZE_SIZE + 4, 8.0f);
    }
    /* 敌：槽 2 活着且是 boss；槽 5 活着非 boss；其余空 */
    {
        uint64_t e2 = RVA_G_ENEMIES + 2 * ENEMY_SIZE, e5 = RVA_G_ENEMIES + 5 * ENEMY_SIZE;
        img[e2 + OFF_E_FLAGS] = 0x80 | 0x20; img[e2 + OFF_E_FLAGS2] = 0x03;
        put_f32(e2 + OFF_E_POS, 192.0f); put_f32(e2 + OFF_E_POS + 4, 128.0f);
        put_i32(e2 + OFF_E_BOSS_ID, 0);
        put_i32(e2 + OFF_E_LIFE, 900); put_i32(e2 + OFF_E_MAX_LIFE, 1200);
        put_f32(e2 + OFF_E_HITBOX, 48.0f); put_f32(e2 + OFF_E_HITBOX + 4, 30.0f);
        img[e5 + OFF_E_FLAGS] = 0x80; img[e5 + OFF_E_FLAGS2] = 0x01;   /* 只有 bit0：不碰撞 */
        put_f32(e5 + OFF_E_POS, 192.0f - 100.0f); put_f32(e5 + OFF_E_POS + 4, 64.0f);
        put_i32(e5 + OFF_E_BOSS_ID, -1);
        put_i32(e5 + OFF_E_LIFE, 10); put_i32(e5 + OFF_E_MAX_LIFE, 10);
        put_f32(e5 + OFF_E_HITBOX, 24.0f); put_f32(e5 + OFF_E_HITBOX + 4, 24.0f);
    }

    /* 激光：槽 0 state 1 有效；槽 1 state 0 预警（还差 30 帧）；槽 2 state 2 收缩（不收）；槽 3 未用 */
    {
        uint64_t l0 = RVA_G_LASERS + 0 * LASER_SIZE, l1 = RVA_G_LASERS + 1 * LASER_SIZE, l2 = RVA_G_LASERS + 2 * LASER_SIZE;
        img[l0 + OFF_L_IN_USE] = 1; img[l0 + OFF_L_STATE] = 1;
        put_f32(l0 + OFF_L_POS, 192.0f + 40.0f); put_f32(l0 + OFF_L_POS + 4, 100.0f);
        put_f32(l0 + OFF_L_ANGLE, 1.5f); put_f32(l0 + OFF_L_WIDTH, 16.0f);
        put_f32(l0 + OFF_L_START_OFFSET, 10.0f); put_f32(l0 + OFF_L_END_OFFSET, 250.0f);
        put_f32(l0 + OFF_L_START_LEN, 300.0f); put_f32(l0 + OFF_L_SPEED, 6.0f);
        img[l1 + OFF_L_IN_USE] = 1; img[l1 + OFF_L_STATE] = 0;
        put_i32(l1 + OFF_L_STATE0_LEN, 50); put_i32(l1 + OFF_L_TIMER_CUR, 20);
        put_f32(l1 + OFF_L_WIDTH, 8.0f);
        img[l2 + OFF_L_IN_USE] = 1; img[l2 + OFF_L_STATE] = 2;
    }

    th06nc_extract(img, w);

    assert(w->nlasers == 2);
    assert(w->lasers[0].x == 40.0f && w->lasers[0].y == 100.0f && w->lasers[0].angle == 1.5f);
    assert(w->lasers[0].start == 10.0f && w->lasers[0].end == 250.0f && w->lasers[0].start_len == 300.0f);
    assert(w->lasers[0].speed == 6.0f && w->lasers[0].half_h == 4.0f && w->lasers[0].state == 1);
    assert(w->lasers[0].t_active == 0);
    assert(w->lasers[1].state == 0 && w->lasers[1].t_active == 30 && w->lasers[1].half_h == 2.0f);

    assert(w->frame == 1234);
    assert(w->phase & AP_PHASE_IN_GAME);
    assert(w->phase & AP_PHASE_SPELL_ACTIVE);
    assert(!(w->phase & AP_PHASE_BOMB_ACTIVE));
    assert(w->phase & AP_PHASE_PLAYER_CONTROLLABLE);      /* state 3 = INVULNERABLE 可操作 */
    assert(w->player.x == 30.0f && w->player.y == 384.0f);
    assert(w->player.hit_radius == 1.25f && w->player.focus == 1 && w->player.state == 3);
    assert(w->player.speed == 5.0f && w->player.speed_focus == 2.5f);
    assert(w->player.lives == 4 && w->player.bombs == 2 && w->player.power == 96);
    assert(w->player.score == 123456789u && w->player.graze == 77);
    assert(w->area.xmin == 8.0f - 192.0f && w->area.xmax == 8.0f + 368.0f - 192.0f);
    assert(w->area.ymin == 16.0f && w->area.ymax == 16.0f + 416.0f);

    assert(w->nbullets == 2);
    assert(w->bullets[0].x == 10.0f && w->bullets[0].y == 100.0f);
    assert(w->bullets[0].vx == 0.5f && w->bullets[0].vy == 3.0f);
    assert(w->bullets[0].radius == 6.0f);                /* min(16,12)*0.5 */
    assert(w->bullets[0].state == 1 && w->bullets[0].grazed == 1);
    assert(w->bullets[1].x == -50.0f && w->bullets[1].state == 3 && w->bullets[1].radius == 4.0f);

    assert(w->nenemies == 2);
    assert(w->enemies[0].x == 0.0f && w->enemies[0].y == 128.0f && w->enemies[0].boss == 1);
    assert(w->enemies[0].hp == 900 && w->enemies[0].hp_max == 1200);
    assert(w->enemies[0].hit_w == 16.0f && w->enemies[0].hit_h == 10.0f);   /* hitboxDimensions / 3 */
    assert(w->enemies[0].collidable == 1 && w->enemies[1].collidable == 0);
    assert(w->enemies[1].x == -100.0f && w->enemies[1].boss == 0 && w->enemies[1].hp == 10);

    /* 可动区读到 0 时退回默认 */
    memset(img + RVA_MOVEMENT_AREA, 0, 16);
    th06nc_extract(img, w);
    assert(w->area.xmin == AREA_DEFAULT_X - 192.0f && w->area.ymax == AREA_DEFAULT_Y + AREA_DEFAULT_H);

    /* 出生态不可操作 */
    img[RVA_G_PLAYER + OFF_PLAYER_STATE] = 1;
    th06nc_extract(img, w);
    assert(!(w->phase & AP_PHASE_PLAYER_CONTROLLABLE));

    puts("test_extract ok");
    return 0;
}
