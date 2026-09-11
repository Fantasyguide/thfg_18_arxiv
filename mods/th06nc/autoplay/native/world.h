/* world.h —— 与作品无关的「一帧世界快照」。抽取器把游戏内存填进来，策略只看它。
 *
 * 坐标系与 stg-agent-proto / stg-engine 一致：x 以关卡区中轴为 0（左负右正），
 * y 以关卡区顶边为 0、向下为正，单位 px。关卡区 384×448。
 * 这份结构就是将来协议编码器的输入（spec §5.7），所以字段名照 spec §4.3.2 起。 */
#ifndef AP_WORLD_H
#define AP_WORLD_H

#include <stdint.h>

#define AP_MAX_BULLETS 640
#define AP_MAX_ENEMIES 256

typedef struct {
    float x, y;            /* 位置 */
    float vx, vy;          /* 每帧位移 */
    float radius;          /* 判定半径 */
    uint8_t state;         /* 原值透传（th06nc：1 飞行） */
    uint8_t grazed;
} ap_bullet_t;

typedef struct {
    float x, y;
    float hit_w, hit_h;    /* 撞自机的框（半宽半高） */
    int32_t hp, hp_max;
    uint8_t boss;          /* 🟡 弱锚 */
} ap_enemy_t;

typedef struct {
    float x, y;
    float hit_radius;
    float speed, speed_focus;   /* 本帧合法步长（正交），低速版 */
    uint8_t state;         /* th06nc：0 ALIVE 1 SPAWNING 2 DEAD 3 INVULNERABLE */
    uint8_t focus;
    uint8_t lives, bombs;
    uint16_t power;
    uint32_t score, graze;
} ap_player_t;

typedef struct {
    float xmin, xmax, ymin, ymax;   /* 自机可动区（协议坐标系） */
} ap_area_t;

typedef struct {
    uint32_t frame;
    uint32_t phase;         /* SA_PHASE_* 同义位，见 policy.h */
    ap_player_t player;
    ap_area_t area;
    int nbullets;
    ap_bullet_t bullets[AP_MAX_BULLETS];
    int nenemies;
    ap_enemy_t enemies[AP_MAX_ENEMIES];
} ap_world_t;

#define AP_PHASE_IN_GAME             (1u << 0)
#define AP_PHASE_BOMB_ACTIVE         (1u << 4)
#define AP_PHASE_SPELL_ACTIVE        (1u << 5)
#define AP_PHASE_PLAYER_CONTROLLABLE (1u << 6)

#endif
