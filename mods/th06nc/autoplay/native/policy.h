/* policy.h —— 内嵌策略：看一帧世界，给出动作位。纯 C，无平台依赖，宿主可测。
 *
 * 动作位用 stg-agent-proto 的编号（spec §4.4.1），不是游戏的 INPUT_HELD 位；
 * 映射到具体作品是加载器（hook.c）的事。
 *
 * 现行算法 = 速度势场（软层）+ 一帧速度障碍掩码（硬层），决策记录见 policy.c 头部。
 * 旧的采样版在 archive/。 */
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
    /* ---- 软层：速度势场 ---- */
    int   horizon;        /* 最近接近点的搜索窗 T（帧）：t* 钳到 [0, T] */
    float tau;            /* 时间折扣 w(t*) = exp(−t* / tau)（帧） */
    float safe_r;         /* 净距超过它的弹不产生斥力（px） */
    float vortex;         /* 切向分量相对法向的比例 λ */
    float k_att;          /* x 引力系数（对齐 boss；相对斥力满值 1.0，要弱） */
    float k_att_y;        /* y 引力系数（回 home_y） */
    float wall_margin;    /* 离可动区左 / 右 / 上边界多近开始软斥力（px） */
    float wall_margin_bottom; /* 下边界单独一档（自机本来就该待在下面） */
    float wall_k_bottom;  /* 下边界斥力系数（其余边为 1.0） */
    float persist;        /* 上一帧方向的惯性奖励 */
    float f_min;          /* 合力低于它就不动 */
    float slow_dist;      /* 有弹在这个当前距离内就按低速（px） */
    float home_y;         /* 引力的 y 目标 */
    /* ---- 连续 → 八向的去抖（D10）---- */
    float smooth;         /* 合力指数平滑系数：F = smooth·F_now + (1−smooth)·F_prev */
    float switch_hyst;    /* 换方向滞回：新方向得分要超过当前方向 (1+switch_hyst) 倍才换 */
    int   hold_frames;    /* 换向后最少保持的帧数（被硬层禁用或得分转负除外） */
    float f_stop;         /* 停下阈值（< f_min，施密特触发） */
    float slow_exit;      /* 低速退出距离（> slow_dist，施密特触发） */
    int   slow_hold;      /* 低速状态最少保持帧数 */
    /* ---- 硬层：一帧速度障碍掩码 + 应急炸弹 ---- */
    int   vo_frames;      /* 掩码外推帧数：这么多帧内会撞的动作直接禁用 */
    int   bomb_cooldown;  /* 两次 bomb 之间的最少帧数 */
    /* ---- 运行状态 ---- */
    uint32_t last_bomb_frame;
    uint32_t prev_buttons;
    /* ---- 上一帧的合力（日志 / 调试用） ---- */
    float last_fx, last_fy;
    /* ---- 去抖的运行状态 ---- */
    float sm_fx, sm_fy;   /* 平滑后的合力 */
    int   hold_left;      /* 当前方向还要保持几帧 */
    int   slow_on;        /* 低速闩锁 */
    int   slow_hold_left;
    int   moving;         /* 起停闩锁 */
    int   ref_sx, ref_sy; /* 涡旋选边参考点的闩锁：±1；0 = 尚未闩（按位置直接算） */
} ap_policy_t;

void     ap_policy_init(ap_policy_t *p);
uint32_t ap_policy_decide(ap_policy_t *p, const ap_world_t *w);

/* 硬层用的外推：自机按 (vx,vy) 走 horizon 帧，返回时间窗内到最近弹 / 敌的净距（负 = 会撞），
 * first_hit_t = 首次为负的帧（没有则 0）。也是测试的判别工具。 */
float    ap_policy_score_move(const ap_world_t *w, float vx, float vy, int horizon, float *first_hit_t);

/* 软层：算势场合力（不含硬层、不量化）。测试用。 */
void     ap_policy_field(const ap_policy_t *p, const ap_world_t *w, float *fx, float *fy);

#endif
