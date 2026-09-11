/* extract.c —— th06nc：游戏内存 → ap_world_t。只读。
 *
 * 坐标：th06nc 的自机 / 弹 / 敌坐标都是关卡区局部、原点左上角（engine/player/th06nc/01 §2.11），
 * 这里统一减去 PLAYFIELD_W/2 平移到中轴原点，y 不动。
 * 弹的判定半径：min(grazeSize.x, grazeSize.y) × 0.5 —— 🟡 按擦弹公式的同一项取，
 * 死亡判定（CalcKillBoxCollision 第四参 = 1 圆对圆）的确切半径待对一遍 0x14006a980。 */
#include <string.h>
#include "extract.h"
#include "th06nc.h"

static float rd_f32(const uint8_t *p) { float v; memcpy(&v, p, 4); return v; }
static int32_t rd_i32(const uint8_t *p) { int32_t v; memcpy(&v, p, 4); return v; }
static int16_t rd_i16(const uint8_t *p) { int16_t v; memcpy(&v, p, 2); return v; }
static uint16_t rd_u16(const uint8_t *p) { uint16_t v; memcpy(&v, p, 2); return v; }
static uint64_t rd_u64(const uint8_t *p) { uint64_t v; memcpy(&v, p, 8); return v; }

#define HALF_W (PLAYFIELD_W * 0.5f)

void th06nc_extract(const uint8_t *base, ap_world_t *w)
{
    const uint8_t *pl = base + RVA_G_PLAYER;
    const uint8_t *area = base + RVA_MOVEMENT_AREA;
    int i;

    w->frame = (uint32_t)rd_i32(base + RVA_BULLET_MANAGER + 4);

    w->player.x = rd_f32(pl + OFF_POSITION_X) - HALF_W;
    w->player.y = rd_f32(pl + OFF_POSITION_Y);
    w->player.hit_radius = rd_f32(pl + OFF_HITBOX_RADIUS);
    w->player.speed = rd_f32(pl + OFF_CHAR_DATA + 0);
    w->player.speed_focus = rd_f32(pl + OFF_CHAR_DATA + 4);
    w->player.state = pl[OFF_PLAYER_STATE];
    w->player.focus = pl[OFF_IS_FOCUSED] ? 1 : 0;
    w->player.lives = base[RVA_LIVES];
    w->player.bombs = base[RVA_BOMBS];
    w->player.power = rd_u16(base + RVA_POWER);
    w->player.score = (uint32_t)rd_u64(base + RVA_SCORE);
    w->player.graze = (uint32_t)rd_i32(base + RVA_GRAZE_IN_STAGE);

    {
        float ax = rd_f32(area), ay = rd_f32(area + 4), aw = rd_f32(area + 8), ah = rd_f32(area + 12);
        if (aw <= 0.0f || ah <= 0.0f) { ax = AREA_DEFAULT_X; ay = AREA_DEFAULT_Y; aw = AREA_DEFAULT_W; ah = AREA_DEFAULT_H; }
        w->area.xmin = ax - HALF_W; w->area.xmax = ax + aw - HALF_W;
        w->area.ymin = ay;          w->area.ymax = ay + ah;
    }

    w->phase = AP_PHASE_IN_GAME;
    if (rd_i32(base + RVA_SPELL_ACTIVE)) w->phase |= AP_PHASE_SPELL_ACTIVE;
    if (base[RVA_BOMB_IN_USE]) w->phase |= AP_PHASE_BOMB_ACTIVE;
    if (w->player.state == 0 || w->player.state == 3) w->phase |= AP_PHASE_PLAYER_CONTROLLABLE;

    w->nbullets = 0;
    for (i = 0; i < BULLET_COUNT && w->nbullets < AP_MAX_BULLETS; i++) {
        const uint8_t *b = base + RVA_G_BULLETS + (uint64_t)i * BULLET_SIZE;
        int16_t st = rd_i16(b + OFF_B_STATE);
        ap_bullet_t *o;
        float sx, sy;
        if (st == 0) continue;
        o = &w->bullets[w->nbullets++];
        o->x = rd_f32(b + OFF_B_POS) - HALF_W;
        o->y = rd_f32(b + OFF_B_POS + 4);
        o->vx = rd_f32(b + OFF_B_VELOCITY);
        o->vy = rd_f32(b + OFF_B_VELOCITY + 4);
        sx = rd_f32(b + OFF_B_GRAZE_SIZE); sy = rd_f32(b + OFF_B_GRAZE_SIZE + 4);
        o->radius = (sx < sy ? sx : sy) * 0.5f;
        o->state = (uint8_t)st;
        o->grazed = b[OFF_B_IS_GRAZED] ? 1 : 0;
    }

    w->nenemies = 0;
    for (i = 0; i < ENEMY_COUNT && w->nenemies < AP_MAX_ENEMIES; i++) {
        const uint8_t *e = base + RVA_G_ENEMIES + (uint64_t)i * ENEMY_SIZE;
        ap_enemy_t *o;
        if (!(e[OFF_E_FLAGS] & ENEMY_FLAG_ALIVE)) continue;
        o = &w->enemies[w->nenemies++];
        o->x = rd_f32(e + OFF_E_POS) - HALF_W;
        o->y = rd_f32(e + OFF_E_POS + 4);
        o->hit_w = rd_f32(e + OFF_E_HITBOX) * 0.5f;
        o->hit_h = rd_f32(e + OFF_E_HITBOX + 4) * 0.5f;
        o->hp = rd_i32(e + OFF_E_LIFE);
        o->hp_max = rd_i32(e + OFF_E_MAX_LIFE);
        o->boss = rd_i32(e + OFF_E_BOSS_ID) >= 0 ? 1 : 0;
    }
}
