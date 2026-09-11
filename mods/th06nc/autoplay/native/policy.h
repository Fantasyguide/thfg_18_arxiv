/* policy.h —— 内嵌策略：看一帧世界，给出动作位。纯 C，无平台依赖，宿主可测。
 *
 * 动作位用 stg-agent-proto 的编号（spec §4.4.1），不是游戏的 INPUT_HELD 位；
 * 映射到具体作品是加载器（hook.c）的事。 */
#ifndef AP_POLICY_H
#define AP_POLICY_H

#include <stdint.h>
#include "world.h"

#define BTN_UP     (1u << 0)
#define BTN_DOWN   (1u << 1)
#define BTN_LEFT   (1u << 2)
#define BTN_RIGHT  (1u << 3)
#define BTN_SHOT   (1u << 4)
#define BTN_BOMB   (1u << 5)
#define BTN_SLOW   (1u << 6)

typedef struct {
    int      horizon;          /* 外推帧数 */
    float    safe_margin;      /* 想保持的最小净距（px） */
    float    home_y;           /* 无威胁时想待的 y */
    int      bomb_cooldown;    /* 两次 bomb 之间的最少帧数 */
    /* 运行状态 */
    uint32_t last_bomb_frame;
    uint32_t prev_buttons;
} ap_policy_t;

void     ap_policy_init(ap_policy_t *p);
uint32_t ap_policy_decide(ap_policy_t *p, const ap_world_t *w);

/* 供测试与日志：候选动作评分。返回 best 的净距（负 = 预计被弹）。 */
float    ap_policy_score_move(const ap_world_t *w, float vx, float vy, int horizon, float *first_hit_t);

#endif
