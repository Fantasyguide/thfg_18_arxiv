/* policy.c —— 速度势场（软层）+ 一帧速度障碍掩码（硬层）。
 *
 * ================================================================================
 * 决策记录（2026-09-11；调研见 README「策略：为什么是势场 + VO 掩码」）
 * ================================================================================
 * D1  为什么不是纯 APF：经典人工势场只看位置，四个公认的病——局部极小、目标不可达、
 *     窄缝振荡、不知道障碍在动。弹幕里最致命的是「合力为零但正在被撞」。
 *     → 采用业内通行的两层结构：硬约束（速度障碍掩码）+ 软偏好（势场）。
 * D2  斥力用「最近接近点」而不是当前位置（Ge & Cui 2002 的速度势场 / 预测势场那一支）：
 *     t* = clamp(−r·v / |v|², 0, T)，c = r + v t*，d* = |c| − (r_b + r_p)。
 *     迎面弹在很远处就产生推力，擦身弹几乎不推。
 * D3  时间折扣 w(t*) = exp(−t* / τ)。两个理由合在一个因子里：越远的未来越不紧迫；
 *     线性外推越远越不可信（NC 的弹有 exFlags 转向 / 加速，本策略没读它们）。
 * D4  斥力大小用无量纲的 ((safe_r − d*) / safe_r)²，d* ≥ safe_r 则为 0：
 *     远处的弹完全不参与，避免几百颗弹的 1/d² 尾巴叠成一堵「假墙」。
 * D5  涡旋（切向）项：在法向斥力之外加 λ·|F_n| 的切向分量。纯法向斥力会让自机在弹流前面来回顶；
 *     切向分量让它顺着流缝滑（Vortex APF 的做法）。弹正好穿过自机（|c| ≈ 0）时法向无定义，
 *     此时整个力都取垂直于弹速的方向。**选边参考点是 (0, home_y)，不是可动区中心**：
 *     中心在 y=224、比自机高，横向飞来的弹会把自机一律往上推——实机表现就是「冲到 boss 脸上」
 *     （2026-09-11 反馈）。
 * D6  引力（x 对齐 boss、y 回到 home_y——y 不拉 boss，弹是纵向飞的，x 对齐就够）只在「连续
 *     calm_frames 帧没有斥力」后才起作用，否则自机在斥力边界上会来回进出（APF 的经典振荡）。
 *     引力要弱（k_att 0.12 / 死区 16 px）。上一帧方向的惯性奖励**只在合力本来就偏向该方向时**加：
 *     2026-09-11 实机发现合力为零时惯性仍高于起动阈值，自机会穿过引力死区再被拉回，无弹时在
 *     中轴附近来回振荡——停走判据现在用不含惯性的原始分。
 * D6b 墙：左 / 右 / 上 24 px 起软斥力；下边界单独 12 px、系数 0.5——自机本来就该待在中下部，
 *     底边推得太狠会把它顶离 home_y（实机反馈）。
 * D7  硬层：9 个方向 × 当前速度，用 vo_frames 帧的线性外推判「会不会立刻撞」，撞的禁用；
 *     全部禁用 → 换另一档速度再试 → 仍全禁用 → 有炸弹且过冷却就炸，否则选最不坏的。
 *     这就是旧采样版（archive/）的极简形态，只留它当安全网。
 * D8  敌人本体：距离函数换成「到致死盒的钳位距离 − r_p」（与引擎判定同式，见
 *     engine/player/th06nc/01 §2.7），当静止障碍，不加涡旋项，只对 collidable 的算。
 * D9  激光（engine/bullet/th06nc/01 §3.1）：致死盒 = 激光坐标系里沿线 [start, end]、半高 width/4 的
 *     矩形，自机绕原点旋转 −angle 进坐标系后钳到盒内比半径——与引擎同式。预警态（state 0）
 *     带「还有 t_active 帧开始杀」：硬层在 t < t_active 时不算撞；软层把势放在 t_active 时刻的盒上、
 *     按 w(t_active) 折扣，所以预警线一出现就开始往外推、但远期的推得轻。
 *     伸长中的激光（end += speed）按同一套外推。不加涡旋项（沿着激光滑没有意义）。
 * 不做：道具、弹的 exFlags 外推、激光的转角、符卡时间。
 * ================================================================================ */
#include <math.h>
#include <string.h>
#include "policy.h"

void ap_policy_init(ap_policy_t *p)
{
    memset(p, 0, sizeof *p);
    p->horizon = 60;
    p->tau = 20.0f;
    p->safe_r = 48.0f;
    p->vortex = 0.8f;
    p->k_att = 0.12f;
    p->k_att_y = 0.12f;
    p->wall_margin = 24.0f;
    p->wall_margin_bottom = 12.0f;
    p->wall_k_bottom = 0.5f;
    p->persist = 0.15f;
    p->f_min = 0.05f;
    p->slow_dist = 40.0f;
    p->home_y = 384.0f;
    p->vo_frames = 3;
    p->bomb_cooldown = 90;
    p->last_bomb_frame = 0xFFFFFFFFu;
}

static float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

/* ---- 激光：t 帧后自机在 (px,py) 时到致死盒的净距；未到 t_active 返回 +大；
 *      cpx/cpy 回传盒上最近点的世界坐标（势场的法向用）---- */
static float laser_margin(const ap_laser_t *l, float px, float py, float t, float rp, float *cpx, float *cpy)
{
    float c = cosf(l->angle), sn = sinf(l->angle);
    float end_t = l->end + l->speed * t, start_t;
    float dx = px - l->x, dy = py - l->y, xf, yf, qx, qy, ex, ey;
    if (t < (float)l->t_active) { *cpx = px; *cpy = py; return 1e9f; }
    start_t = end_t - l->start_len; if (start_t < l->start) start_t = l->start; if (start_t < 0.0f) start_t = 0.0f;
    xf = c * dx + sn * dy;                 /* 旋转 −angle 进激光坐标系 */
    yf = c * dy - sn * dx;
    qx = clampf(xf, start_t, end_t);
    qy = clampf(yf, -l->half_h, l->half_h);
    *cpx = l->x + c * qx - sn * qy;        /* 最近点转回世界坐标 */
    *cpy = l->y + sn * qx + c * qy;
    ex = xf - qx; ey = yf - qy;
    return sqrtf(ex * ex + ey * ey) - rp;
}

/* ---- 硬层用的外推（与旧采样版同式，加激光）---- */
float ap_policy_score_move(const ap_world_t *w, float vx, float vy, int horizon, float *first_hit_t)
{
    float px = w->player.x, py = w->player.y, r = w->player.hit_radius;
    float worst = 1e9f;
    int t, i;
    *first_hit_t = 0.0f;
    for (t = 1; t <= horizon; t++) {
        float ft = (float)t;
        px = clampf(px + vx, w->area.xmin, w->area.xmax);
        py = clampf(py + vy, w->area.ymin, w->area.ymax);
        for (i = 0; i < w->nbullets; i++) {
            const ap_bullet_t *b = &w->bullets[i];
            float bx = b->x + b->vx * ft, by = b->y + b->vy * ft;
            float dx = bx - px, dy = by - py;
            float m = sqrtf(dx * dx + dy * dy) - (r + b->radius);
            if (m < worst) { worst = m; if (m < 0.0f && *first_hit_t == 0.0f) *first_hit_t = ft; }
        }
        for (i = 0; i < w->nenemies; i++) {
            const ap_enemy_t *e = &w->enemies[i];
            float cx, cy, dx, dy, m;
            if (!e->collidable) continue;
            cx = clampf(px, e->x - e->hit_w, e->x + e->hit_w);
            cy = clampf(py, e->y - e->hit_h, e->y + e->hit_h);
            dx = px - cx; dy = py - cy;
            m = sqrtf(dx * dx + dy * dy) - r;
            if (m < worst) { worst = m; if (m < 0.0f && *first_hit_t == 0.0f) *first_hit_t = ft; }
        }
        for (i = 0; i < w->nlasers; i++) {
            float qx, qy, m = laser_margin(&w->lasers[i], px, py, ft, r, &qx, &qy);
            if (m < worst) { worst = m; if (m < 0.0f && *first_hit_t == 0.0f) *first_hit_t = ft; }
        }
    }
    return worst;
}

/* ---- 软层：势场 ---- */
static void field_repulsion(const ap_policy_t *p, const ap_world_t *w, float *fx, float *fy)
{
    const float px = w->player.x, py = w->player.y, rp = w->player.hit_radius;
    const float ccx = 0.0f, ccy = p->home_y;   /* 选边参考点（D5）：中轴、中下，不是可动区中心 */
    float F_x = 0.0f, F_y = 0.0f;
    int i;

    for (i = 0; i < w->nbullets; i++) {
        const ap_bullet_t *b = &w->bullets[i];
        float rx = b->x - px, ry = b->y - py, vx = b->vx, vy = b->vy;
        float v2 = vx * vx + vy * vy, ts = 0.0f, cx, cy, cl, ds, wt, mag, nx, ny, tx, ty;
        if (v2 > 1e-6f) ts = clampf(-(rx * vx + ry * vy) / v2, 0.0f, (float)p->horizon);   /* D2 */
        cx = rx + vx * ts; cy = ry + vy * ts;
        cl = sqrtf(cx * cx + cy * cy);
        ds = cl - (b->radius + rp);
        if (ds >= p->safe_r) continue;                                                     /* D4 */
        wt = expf(-ts / p->tau);                                                           /* D3 */
        mag = (p->safe_r - (ds > 0.0f ? ds : 0.0f)) / p->safe_r;
        mag = wt * mag * mag;
        if (cl > 1e-3f) {
            nx = -cx / cl; ny = -cy / cl;                      /* 法向：从接近点指向自机 */
            tx = -ny; ty = nx;                                 /* 切向：先取逆时针 */
            if (tx * (ccx - px) + ty * (ccy - py) < 0.0f) { tx = -tx; ty = -ty; }   /* 朝可动区中心那一侧 */
            F_x += mag * (nx + p->vortex * tx);                                            /* D5 */
            F_y += mag * (ny + p->vortex * ty);
        } else {
            /* 弹正好穿过自机：法向无定义，整个力取垂直于弹速的方向（朝中心一侧） */
            float sv = sqrtf(v2);
            if (sv < 1e-6f) { nx = 0.0f; ny = -1.0f; }
            else { nx = -vy / sv; ny = vx / sv; }
            if (nx * (ccx - px) + ny * (ccy - py) < 0.0f) { nx = -nx; ny = -ny; }
            F_x += mag * nx; F_y += mag * ny;
        }
    }
    for (i = 0; i < w->nenemies; i++) {                                                    /* D8 */
        const ap_enemy_t *e = &w->enemies[i];
        float qx, qy, dx, dy, d, ds, mag;
        if (!e->collidable) continue;
        qx = clampf(px, e->x - e->hit_w, e->x + e->hit_w);
        qy = clampf(py, e->y - e->hit_h, e->y + e->hit_h);
        dx = px - qx; dy = py - qy;
        d = sqrtf(dx * dx + dy * dy);
        ds = d - rp;
        if (ds >= p->safe_r) continue;
        mag = (p->safe_r - (ds > 0.0f ? ds : 0.0f)) / p->safe_r;
        mag *= mag;
        if (d > 1e-3f) { F_x += mag * dx / d; F_y += mag * dy / d; }
        else { /* 在盒内：往盒中心的反方向推 */
            float ex = px - e->x, ey = py - e->y, el = sqrtf(ex * ex + ey * ey);
            if (el < 1e-3f) { ex = 0.0f; ey = 1.0f; el = 1.0f; }
            F_x += mag * ex / el; F_y += mag * ey / el;
        }
    }
    for (i = 0; i < w->nlasers; i++) {                                                     /* D9 */
        const ap_laser_t *l = &w->lasers[i];
        int k;
        float best_m = 1e9f, best_t = 0.0f, bqx = px, bqy = py;
        /* 在 t_active 及其后几个采样点上取最小净距：伸长中的激光要靠后面的采样才看得见 */
        for (k = 0; k < 4; k++) {
            float t = (float)l->t_active + 8.0f * (float)k, qx, qy, m;
            if (t > (float)p->horizon) break;
            m = laser_margin(l, px, py, t, rp, &qx, &qy);
            if (m < best_m) { best_m = m; best_t = t; bqx = qx; bqy = qy; }
        }
        if (best_m >= p->safe_r) continue;
        {
            float wt = expf(-best_t / p->tau);
            float mag = (p->safe_r - (best_m > 0.0f ? best_m : 0.0f)) / p->safe_r;
            float nx = px - bqx, ny = py - bqy, nl = sqrtf(nx * nx + ny * ny);
            mag = wt * mag * mag;
            if (nl > 1e-3f) { nx /= nl; ny /= nl; }
            else {  /* 在盒内：垂直于激光方向、朝可动区中心那一侧 */
                nx = -sinf(l->angle); ny = cosf(l->angle);
                if (nx * (ccx - px) + ny * (ccy - py) < 0.0f) { nx = -nx; ny = -ny; }
            }
            F_x += mag * nx; F_y += mag * ny;
        }
    }
    *fx = F_x; *fy = F_y;
}

static void field_walls(const ap_policy_t *p, const ap_world_t *w, float *fx, float *fy)   /* D6b */
{
    const float m = p->wall_margin, mb = p->wall_margin_bottom;
    float d;
    d = w->player.x - w->area.xmin; if (d < m) { float k = (m - d) / m; *fx += k * k; }
    d = w->area.xmax - w->player.x; if (d < m) { float k = (m - d) / m; *fx -= k * k; }
    d = w->player.y - w->area.ymin; if (d < m) { float k = (m - d) / m; *fy += k * k; }
    d = w->area.ymax - w->player.y; if (d < mb) { float k = (mb - d) / mb; *fy -= p->wall_k_bottom * k * k; }
}

static float target_x_of(const ap_world_t *w)
{
    int i;
    for (i = 0; i < w->nenemies; i++) if (w->enemies[i].boss) return w->enemies[i].x;
    return w->nenemies > 0 ? w->enemies[0].x : 0.0f;
}

static void field_attraction(const ap_policy_t *p, const ap_world_t *w, float *fx, float *fy)
{
    float dx = target_x_of(w) - w->player.x, dy = p->home_y - w->player.y;
    if (dx > -16.0f && dx < 16.0f) dx = 0.0f;
    if (dy > -16.0f && dy < 16.0f) dy = 0.0f;
    /* 死区外近似恒力（k_att ≪ 斥力满值 1.0）：按距离线性衰减会在离目标 40 px 处就低于起动阈值，
     * 自机停在半路；恒力则一直走到死区边缘停下，也不会过头 */
    *fx += p->k_att   * clampf(dx / 16.0f, -1.0f, 1.0f);
    *fy += p->k_att_y * clampf(dy / 16.0f, -1.0f, 1.0f);
}

/* 斥力 + 墙，不含引力（引力受 D6 的安静计数门控，在 decide 里加）。测试与日志用。 */
void ap_policy_field(const ap_policy_t *p, const ap_world_t *w, float *fx, float *fy)
{
    field_repulsion(p, w, fx, fy);
    field_walls(p, w, fx, fy);
}

/* ---- 量化与硬层 ---- */
static const int DIRS[9][2] = { {0,0}, {0,-1}, {0,1}, {-1,0}, {1,0}, {-1,-1}, {1,-1}, {-1,1}, {1,1} };
static const uint32_t DIR_BITS[9] = { 0, BTN_UP, BTN_DOWN, BTN_LEFT, BTN_RIGHT,
                                      BTN_UP|BTN_LEFT, BTN_UP|BTN_RIGHT, BTN_DOWN|BTN_LEFT, BTN_DOWN|BTN_RIGHT };
#define CALM_FRAMES 30

/* calm 计数放在 prev_buttons 的高 16 位，避免改 ap_policy_t 的公开布局 */
#define CALM_GET(p)    ((p)->prev_buttons >> 16)
#define CALM_SET(p, n) ((p)->prev_buttons = ((p)->prev_buttons & 0xFFFFu) | ((uint32_t)(n) << 16))

uint32_t ap_policy_decide(ap_policy_t *p, const ap_world_t *w)
{
    float fx, fy, rx, ry, rmag, best_score, min_d;
    int d, spd, best = -1, forbid[2][9], all_forbidden[2], i, want_slow;
    uint32_t prev_dir = p->prev_buttons & (BTN_UP | BTN_DOWN | BTN_LEFT | BTN_RIGHT), out;

    if (w->player.state != 0 && w->player.state != 3) { CALM_SET(p, 0); return 0; }

    /* --- 软层：合力 --- */
    field_repulsion(p, w, &rx, &ry);
    rmag = sqrtf(rx * rx + ry * ry);
    fx = rx; fy = ry;
    field_walls(p, w, &fx, &fy);
    if (rmag < 0.5f * p->f_min) { if (CALM_GET(p) < 0xFFFF) CALM_SET(p, CALM_GET(p) + 1); }
    else CALM_SET(p, 0);
    if (CALM_GET(p) >= CALM_FRAMES) field_attraction(p, w, &fx, &fy);                     /* D6 */
    p->last_fx = fx; p->last_fy = fy;

    /* --- 低速：有弹已经很近就精细走位 --- */
    min_d = 1e9f;
    for (i = 0; i < w->nbullets; i++) {
        float dx = w->bullets[i].x - w->player.x, dy = w->bullets[i].y - w->player.y;
        float dd = sqrtf(dx * dx + dy * dy) - w->bullets[i].radius;
        if (dd < min_d) min_d = dd;
    }
    want_slow = min_d < p->slow_dist;

    /* --- 硬层：两档速度的 9 向掩码 --- */
    for (spd = 0; spd < 2; spd++) {
        float sp = spd ? w->player.speed_focus : w->player.speed;
        if (sp <= 0.0f) sp = spd ? 2.0f : 4.0f;
        all_forbidden[spd] = 1;
        for (d = 0; d < 9; d++) {
            float vx = (float)DIRS[d][0] * sp, vy = (float)DIRS[d][1] * sp, t;
            if (DIRS[d][0] && DIRS[d][1]) { vx *= 0.70710678f; vy *= 0.70710678f; }
            forbid[spd][d] = ap_policy_score_move(w, vx, vy, p->vo_frames, &t) < 0.0f;
            if (!forbid[spd][d]) all_forbidden[spd] = 0;
        }
    }
    spd = want_slow ? 1 : 0;
    if (all_forbidden[spd] && !all_forbidden[1 - spd]) spd = 1 - spd;

    out = BTN_SHOT | (spd ? BTN_SLOW : 0);

    if (all_forbidden[spd]) {                                                              /* D7 应急 */
        float least = -1e9f, t;
        int cooled = p->last_bomb_frame == 0xFFFFFFFFu
                  || (w->frame - p->last_bomb_frame) > (uint32_t)p->bomb_cooldown;
        if (w->player.bombs > 0 && cooled) { out |= BTN_BOMB; p->last_bomb_frame = w->frame; }
        for (d = 0; d < 9; d++) {
            float sp = spd ? w->player.speed_focus : w->player.speed;
            float vx = (float)DIRS[d][0] * sp, vy = (float)DIRS[d][1] * sp, m;
            if (DIRS[d][0] && DIRS[d][1]) { vx *= 0.70710678f; vy *= 0.70710678f; }
            m = ap_policy_score_move(w, vx, vy, p->vo_frames, &t);
            if (m > least) { least = m; best = d; }
        }
        out |= DIR_BITS[best];
        p->prev_buttons = (p->prev_buttons & 0xFFFF0000u) | (out & 0xFFFFu);
        return out;
    }

    /* --- 量化：合力与 8 向点积 + 惯性 --- */
    best_score = -1e9f;
    {
        float best_raw = -1e9f;
        for (d = 1; d < 9; d++) {
            float dx = (float)DIRS[d][0], dy = (float)DIRS[d][1], raw, s;
            if (forbid[spd][d]) continue;
            if (DIRS[d][0] && DIRS[d][1]) { dx *= 0.70710678f; dy *= 0.70710678f; }
            raw = fx * dx + fy * dy;
            s = raw + ((DIR_BITS[d] == prev_dir && raw > 0.0f) ? p->persist : 0.0f);   /* D6：惯性只锦上添花 */
            if (s > best_score) { best_score = s; best = d; best_raw = raw; }
        }
        if (best_raw < p->f_min && !forbid[spd][0]) best = 0;   /* 没有足够的推力就不动（不看惯性） */
    }
    out |= DIR_BITS[best];
    p->prev_buttons = (p->prev_buttons & 0xFFFF0000u) | (out & 0xFFFFu);
    return out;
}
