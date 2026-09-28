/* youkai_core.c —— 108「永夜抄」的纯逻辑（主机单测：tests/test_youkai_core.c）。
 * 每条规则的出处写在 youkai_core.h；这里只写实现上的取舍。无引擎依赖、无浮点库依赖（只用 < > 比较）。*/
#include "youkai_core.h"

/* ------------------------------------------------------------------ §3.1 止射闸 */
int yk_should_suppress_native(int player_present, int focused, int character, int power_level)
{
    /* ① 没有自机（标题 / 过场）⇒ **不抑制**：那时原生行循环本来也不会跑，抑制只是让 `retaddr`
     *    改一次控制流（spec §3.1 原文，少一次风险）。② 高速 ⇒ 不抑制（高速 = 原机体，spec §0）。*/
    if (!player_present || !focused) return 0;
    /* ③ ❓**口径（用户 2026-09-12 定）：子机数为 0 ⇒ 不压原生弹。**
     *   **不是一手复刻** —— TH08 妖侧不看 P、一按低速就有子机，原作里没有「低火力 ⇒ 没子机 ⇒ 一发弹都没有」
     *   这个问题；这条门是照着本作的实现约束补的：
     *     妖侧弹的膛口**恒取子机**（`0x40a9c0` 一手，AUDIT §BO-5）⇒ `count == 0` 时妖侧一发都发不出来；
     *     此时若照 spec §3.1 原文（只看「有自机 + 聚焦」）压掉原生弹，那一档就是**什么都不发**。
     *   数量事实来源**只有一处**：`yk_option_wanted` → `yk_option_count` → `YK_OPTION_BY_CHAR`，
     *   与子机声明层（youkai.c `yk_option_declare`）同表同帧 ⇒ 不会出现「声明了却没压 / 压了却没子机」。
     *   `character` / `power_level` 的越界钳位在 `yk_option_count` 里做（引擎读到的值不信任）。
     *   ⇒ 今天只有「早苗（机体 3）+ 火力档 0（P < 100）」会走到这一支（表里 `{0,1,2,3,4}` 的第一格）；
     *     霊夢/魔理沙/咲夜三行恒 1/1/4 ⇒ 任何档（含档 0）都压。*/
    return yk_option_wanted(focused, character, power_level) ? 1 : 0;
}

int yk_suppress_native_shot_spec31_legacy(int player_present, int focused)
{
    /* 旧形状（只看「有自机 + 聚焦」）——**引擎侧不要再用**，真实判据走上面那条四参函数。
     * 保留它是为了让「没有子机数信息」的调用点与既有单测有个明确的入口：默认机体 = 0（霊夢，数量恒 1），
     * 档 = 0 ⇒ 判据退化回 spec §3.1 原文。*/
    return yk_should_suppress_native(player_present, focused, 0, 0);
}

/* --------------------------------------------------------- §3.2 本体外观 / 移动动画状态 */
int yk_body_state(int dx, int prev_dx, int last)
{
    /* 与 `th18:0x45b610`–`0x45b7a8` 的判定**逐条同序**（一手，见 youkai_core.h §3.2）：
     * 引擎那四条 `cmp/jcc` 的次序是「dx<0 → 零 → 正」，四条互斥所以次序不影响结果，
     * 这里照它的读法写，便于逐条对拍：
     *   dx < 0   : 上一帧 >= 0 ⇒ 1（起步向左）；否则落到「dx==0」那一格（不成立）⇒ 不变
     *   dx == 0  : 上一帧 > 0 ⇒ 4（从右侧停）；上一帧 < 0 ⇒ 2（从左侧停）；== 0 ⇒ 不变
     *   dx > 0   : 上一帧 <= 0 ⇒ 3（起步向右）；否则不变 */
    if (last < 0 || last >= YK_BODY_STATES) last = YK_BODY_IDLE;
    if (dx < 0 && prev_dx >= 0) return YK_BODY_MOVE_L;
    if (dx == 0 && prev_dx > 0) return YK_BODY_STOP_R;
    if (dx == 0 && prev_dx < 0) return YK_BODY_STOP_L;
    if (dx > 0 && prev_dx <= 0) return YK_BODY_MOVE_R;
    return last;
}

/* ------------------------------------------------------------------ §3.3 子机 */
/* 数量表的列数必须与 §3.4 的火力档数一致（两处都是「CURRENT_POWER / 100 钳位」的同一组档）。*/
_Static_assert(YK_OPTION_TIERS == YK_SHOT_TIERS, "option tiers and shot tiers must be the same 5 power levels");

/* 数量表在 youkai_core.h（用户定案 + 一手出处）。这里只做钳位与查表。*/
int yk_option_count(int focused, int character, int power_level)
{
    if (!focused) return 0;                               /* 高速 = 原机体（用户定案 5）*/
    if (character < 0) character = 0;
    else if (character >= YK_OPTION_CHARS) character = YK_OPTION_CHARS - 1;
    if (power_level < 0) power_level = 0;
    else if (power_level >= YK_OPTION_TIERS) power_level = YK_OPTION_TIERS - 1;
    return (int)YK_OPTION_BY_CHAR[character][power_level];
}

int yk_option_wanted(int focused, int character, int power_level)
{
    return yk_option_count(focused, character, power_level) > 0;
}

int yk_option_script_changed(int want, int have)
{
    if (have < 0) return 0;                              /* 手里没有子机 ⇒ 没什么可「换」*/
    return want != have;
}

/* ---- 逐格阵型：原生 `pl0X.sht` 的两块（非聚焦 `+0x40` / 聚焦 `+0x90`）逐格照抄 ----
 * 一手来源（`local/th18.v1.00a/dat/pl0X.sht`，本轮用 4 行一读的脚本逐字节核过）：
 *   · 格 `k` 的 (x, y) 就存在 `+0x40 + k*8` 与 `+0x90 + k*8`（引擎读点 `th18:0x45d902`
 *     `movss xmm0,[ecx+edx*8+0x40]` / `th18:0x45d94c` 的 `+0x94`）；
 *   · `k` 由 `T[档−1] + i` 给出（`th18:0x45d8fc`：`mov edx,[eax+edx*4-4]` + `add edx,edi`），
 *     `T` = 三角序列 `{0,1,3,6,10,11,13}`（表基 `th18:0x4cdb5c`，取址用 +1 项 ⇒ 有效列见 `YK_OPT_TRI`）。
 *   ⇒ 声明 N 个时用的就是引擎给 **N 个原生子机**准备的那 N 个位置（同一份数据，零发明）。
 * ⚠ 机体 2（咲夜）的原始两块是 **(弧度, 半径)**、不是像素；下面表里那一行是**离线换算后的像素**
 *   （规则与推导写在 youkai_core.h 的 §3.3 块注释里，含静止 lean = −π/2 与 `0x4b9464 = −128.0f` 两个一手常量）。
 *   ⚠ 换算后的定点值与本卡落笔时的 `(int32_t)(px * 128.0f)` 逐格相等（离线已逐格验过，见报告）。*/
static const float YK_OPT_BLOCK[YK_OPTION_CHARS][10][4] = {
    /* 机体 0 = 霊夢（pl00.sht）：NF k0..9 = (0,−32) (−32,0) (32,0) (−32,−16) (32,−16) (0,−32) (−38,−16) (38,−16) (−16,−32) (16,−32)
     *                            F  k0..9 = (0,−20) (−12,−20) (12,−20) (−20,−20) (20,−20) (0,−30) (−27,−20) (27,−20) (−9,−30) (9,−30) */
    { {  0.0f, -32.0f,   0.0f, -20.0f }, { -32.0f,   0.0f, -12.0f, -20.0f },
      {  32.0f,   0.0f,  12.0f, -20.0f }, { -32.0f, -16.0f, -20.0f, -20.0f },
      {  32.0f, -16.0f,  20.0f, -20.0f }, {   0.0f, -32.0f,   0.0f, -30.0f },
      { -38.0f, -16.0f, -27.0f, -20.0f }, {  38.0f, -16.0f,  27.0f, -20.0f },
      { -16.0f, -32.0f,  -9.0f, -30.0f }, {  16.0f, -32.0f,   9.0f, -30.0f } },
    /* 机体 1 = 魔理沙（pl01.sht）：NF = (0,−32) (−16,−32) (16,−32) (−32,0) (32,0) (0,−32) (−32,0) (32,0) (−16,−32) (16,−32)
     *                              F  = (0,−32) (−6,−32) (6,−32) (−12,−28) (12,−28) (0,−32) (−18,−24) (18,−24) (−6,−32) (6,−32) */
    { {   0.0f, -32.0f,   0.0f, -32.0f }, { -16.0f, -32.0f,  -6.0f, -32.0f },
      {  16.0f, -32.0f,   6.0f, -32.0f }, { -32.0f,   0.0f, -12.0f, -28.0f },
      {  32.0f,   0.0f,  12.0f, -28.0f }, {   0.0f, -32.0f,   0.0f, -32.0f },
      { -32.0f,   0.0f, -18.0f, -24.0f }, {  32.0f,   0.0f,  18.0f, -24.0f },
      { -16.0f, -32.0f,  -6.0f, -32.0f }, {  16.0f, -32.0f,   6.0f, -32.0f } },
    /* 机体 2 = 咲夜（pl02.sht）：原始两块 x = 弧度、y = 半径 → 换算成像素（位移 = (r·cosθ, r·sinθ)、θ = −π/2 + 表值）：
     *   NF 原始 (0,38) (0.34907,38) (−0.34907,38) (0,38) (0.61087,38) (−0.61087,38) (0.29671,38) (−0.29671,38) (1.0472,38) (−1.0472,38)
     *   F  原始 (0,38) (0.34907,38) (−0.34907,38) (0,38) (0.61087,38) (−0.61087,38) (0.34907,38) (−0.34907,38) (1.0472,38) (−1.0472,38)
     *   ⇒ 每格 = (r·sin a, −r·cos a)（离线算，定点值 = (int)(px×128) 与原式逐格一致）*/
    { {   0.0f, -38.0f,    0.0f, -38.0f }, {  12.996765f, -35.708321f,  12.996765f, -35.708321f },
      { -12.996765f, -35.708321f, -12.996765f, -35.708321f }, {  0.0f, -38.0f,   0.0f, -38.0f },
      {  21.795904f, -31.127777f,  21.795904f, -31.127777f },
      { -21.795904f, -31.127777f, -21.795904f, -31.127777f },
      {  11.110125f, -36.339581f,  12.996765f, -35.708321f },
      { -11.110125f, -36.339581f, -12.996765f, -35.708321f },
      {  32.908966f, -18.999998f,  32.908966f, -18.999998f },
      { -32.908966f, -18.999998f, -32.908966f, -18.999998f } },
    /* 机体 3 = 早苗（pl03.sht）：NF = (0,−38) (−20,−38) (20,−38) (−32,−36) (32,−36) (0,−40) (−36,−34) (36,−34) (−16,−38) (16,−38)
     *                             F  = (0,−28) (−24,−28) (24,−28) (−26,−24) (26,−24) (0,−32) (−28,−20) (28,−20) (−8,−32) (8,−32) */
    { {   0.0f, -38.0f,   0.0f, -28.0f }, { -20.0f, -38.0f, -24.0f, -28.0f },
      {  20.0f, -38.0f,  24.0f, -28.0f }, { -32.0f, -36.0f, -26.0f, -24.0f },
      {  32.0f, -36.0f,  26.0f, -24.0f }, {   0.0f, -40.0f,   0.0f, -32.0f },
      { -36.0f, -34.0f, -28.0f, -20.0f }, {  36.0f, -34.0f,  28.0f, -20.0f },
      { -16.0f, -38.0f,  -8.0f, -32.0f }, {  16.0f, -38.0f,   8.0f, -32.0f } },
};

/* `T[count−1]`：三角序列的前 4 项（`th18:0x4cdb5c` 的 {0,0,1,3,6,…} 取 +1 项 ⇒ {0,1,3,6,10,…}）。
 * 声明 N 个时第 i 格用 `T[N−1] + i`：N=1 → k0；N=2 → k1,2；N=3 → k3,4,5；N=4 → k6,7,8,9（正好覆盖 10 格块）。*/
static const unsigned char YK_OPT_TRI[YK_OPTION_CELLS] = { 0, 1, 3, 6 };

int yk_option_offsets(int character, int count, int cell, yk_opt_off_t *out)
{
    int k;
    float home[2];

    if (!out) return 0;
    if (character < 0 || character >= YK_OPTION_CHARS) return 0;
    if (count < 1 || count > YK_OPTION_CELLS) return 0;
    if (cell < 0 || cell >= count) return 0;
    /* ★★ 2026-09-22（th08 #1）：机体 0/1/2 改用手写死的 **TH08 妖侧阵型**（`yk_option_home_xy`，
     * 一手见 youkai_core.h）—— 原来的 TH18 原生阵型表把灵梦的子机摆在自机上方 20 px，
     * 而原作是 **96 px**（且弹簧跟随）⇒ owner 看到的正是这一条。
     * 机体 3（早苗）没有 TH08 一手数据 ⇒ 仍然用 TH18 原生阵型。*/
    if (yk_option_home_xy(character, cell, home)) {
        out->nfx = home[0]; out->nfy = home[1];
        out->fx  = home[0]; out->fy  = home[1];   /* 我们只在低速（聚焦）手里有子机 ⇒ 两块填一样 */
        return 1;
    }
    k = (int)YK_OPT_TRI[count - 1] + cell;
    if (k < 0 || k > 9) return 0;                        /* 块只有 10 格（`+0x40`..`+0x8f`）*/
    out->nfx = YK_OPT_BLOCK[character][k][0];
    out->nfy = YK_OPT_BLOCK[character][k][1];
    out->fx  = YK_OPT_BLOCK[character][k][2];
    out->fy  = YK_OPT_BLOCK[character][k][3];
    return 1;
}

/* ------------------------------------------------ ★★ th08 #1：妖侧子机的家位置与弹簧（一手见 youkai_core.h）
 * 三张表都**逐格写死**（不假设规律）：机体 0 只有 1 格、机体 1 只有 1 格、机体 2 有 4 格。 */
static const float YK_OPT_HOME[3][4][2] = {
    /* 机体 0 霊夢・紫（`th08:0x44e3a0` → `th08:0x44e770`）：1 个，正上方 96 px（`0x4b42c4`）*/
    { { 0.0f, YK_OPT_HOME_Y_REIMU }, { 0.0f, YK_OPT_HOME_Y_REIMU },
      { 0.0f, YK_OPT_HOME_Y_REIMU }, { 0.0f, YK_OPT_HOME_Y_REIMU } },
    /* 机体 1 魔理沙・アリス（`th08:0x44ea40`：`y -= 32.0`）：1 个，正上方 32 px */
    { { 0.0f, YK_OPT_HOME_Y_MARISA }, { 0.0f, YK_OPT_HOME_Y_MARISA },
      { 0.0f, YK_OPT_HOME_Y_MARISA }, { 0.0f, YK_OPT_HOME_Y_MARISA } },
    /* 机体 2 咲夜・レミリア（`th08:0x44eb70` 的跳表 `th08:0x44ee4b` 四支）：
     *   idx0 x-=30 y-=16 ／ idx1 x-=10 y-=32 ／ idx2 x+=10 y-=32 ／ idx3 x+=30 y-=16 */
    { { YK_OPT_HOME_X_SAKUYA_INNER, YK_OPT_HOME_Y_SAKUYA_OUTER },
      { YK_OPT_HOME_X_SAKUYA_OUTER, YK_OPT_HOME_Y_MARISA },
      { -YK_OPT_HOME_X_SAKUYA_OUTER, YK_OPT_HOME_Y_MARISA },
      { -YK_OPT_HOME_X_SAKUYA_INNER, YK_OPT_HOME_Y_SAKUYA_OUTER } },
};

int yk_option_home_xy(int character, int cell, float out[2])
{
    if (!out) return 0;
    if (character < 0 || character >= 3) return 0;        /* 早苗（3）：TH16 文，本批无一手数据 */
    if (cell < 0 || cell >= YK_OPTION_CELLS) return 0;
    out[0] = YK_OPT_HOME[character][cell][0];
    out[1] = YK_OPT_HOME[character][cell][1];
    return 1;
}

int yk_option_uses_spring(int character)
{
    return character == 0;                                 /* 只有 `th08:0x44e3a0`（紫）有速度积分 */
}

void yk_opt_spring_step(float pos[2], float vel[2], const float goal[2])
{
    float dx, dy;
    if (!pos || !vel || !goal) return;
    /* `th08:0x44e770` 逐指令：delta = (goal − pos) × (K / 16.0)；vel += (delta − vel) × 0.2；
     * pos += vel。K = `ds:0x4b4338` = 1.0 ⇒ 每帧把位置差折算成 1/16 的"目标速度"。*/
    dx = (goal[0] - pos[0]) * (YK_OPT_SPRING_K / YK_OPT_SPRING_DIV);
    dy = (goal[1] - pos[1]) * (YK_OPT_SPRING_K / YK_OPT_SPRING_DIV);
    vel[0] += (dx - vel[0]) * YK_OPT_SPRING_DAMP;
    vel[1] += (dy - vel[1]) * YK_OPT_SPRING_DAMP;
    pos[0] += vel[0];
    pos[1] += vel[1];
}

int yk_option_home_from_player(int character, int cell, const float player_xy[2], float out[2])
{
    if (!player_xy) return 0;
    if (!yk_option_home_xy(character, cell, out)) return 0;
    out[0] += player_xy[0];
    out[1] += player_xy[1];
    if (out[1] < YK_OPT_HOME_Y_MIN) out[1] = YK_OPT_HOME_Y_MIN;   /* `th08:0x44e770` 的 `y` 下限 */
    return 1;
}

/* ------------------------------------- ★★★ 咲夜（机体 2）的**放置型**子机 + 自转（owner 2026-09-27 ①②）
 * 一手全文与逐指令对照写在 `youkai_core.h` 的同一段注释里（`th08:0x44eb70` / `0x43edb0` / 跳表）。*/
int yk_option_uses_freeze(int character)
{
    return character == 2;      /* 只有 `th08:0x44eb70`（咲夜・レミリア）的 state 2 不碰位置 */
}

int yk_option_freeze_face0(int cell, float *out)
{
    /* `th08:0x44ec39`（idx0 = 0）/ `0x44ec7b`（idx1 = π = `0x40490fdb`）/ `0x44ecba`（idx2 = 0）/
     * `0x44ecf9`（idx3 = π）：内圈朝 0、外圈朝 π（左右相对）。*/
    if (!out) return 0;
    *out = (cell == 1 || cell == 3) ? 3.14159274101257324f : 0.0f;
    return 1;
}

int yk_option_freeze_spin(int cell, float *out)
{
    /* 跳表 `th08:0x44ee5b` 四支各自的立即数（浮点值照抄）：
     * idx0 `0x3cd67750` = +0.026179939（1.5°）· idx1 `0xbd0efa35` = −0.034906585（−2°）
     * idx2 `0x3d0efa35` = +0.034906585 ·   idx3 `0xbcd67750` = −0.026179939 */
    if (!out) return 0;
    switch (cell) {
    case 0: *out =  YK_OPT_FREEZE_SPIN_OUTER; return 1;
    case 1: *out = -YK_OPT_FREEZE_SPIN_INNER; return 1;
    case 2: *out =  YK_OPT_FREEZE_SPIN_INNER; return 1;
    case 3: *out = -YK_OPT_FREEZE_SPIN_OUTER; return 1;
    default: *out = 0.0f; return 0;
    }
}

void yk_opt_angle_norm(float angle, float step, float *out)
{
    /* `th08:0x43edb0` 逐指令：`f = angle + step`（`fadd [ebp+0xc]`）→
     * `loop: if (f < π) goto exit; f -= 2π; if (++i > 0x10) goto exit; goto loop`
     * （`fcomp ds:0x4b4334` = π、`fsub ds:0x4b4460` = 2π、`cmp eax,0x10`）;
     * `exit: if (f < −π) f += 2π`（`fcomp ds:0x4b4464` = −π）。常量：π = `0x40490fdb`。*/
    const float PI_F = 3.14159265358979f;
    const float TWO_PI = 6.28318530717959f;
    float f;
    int i = 0;
    if (!out) return;
    f = angle + step;
    while (f >= PI_F) { f -= TWO_PI; if (++i > 16) break; }
    if (f < -PI_F) f += TWO_PI;
    *out = f;
}

/* ------------------------------------------- ★★ 追敌（`th08:0x44e8d0`，一手见 youkai_core.h）
 * 与「跟随自机」**共用同一条弹簧**（`yk_opt_spring_step`），只有家位置换了：
 *   `goal = target->final_pos(+0x2d88)`，`goal.y += 32.0`（`0x4b42cc`），`goal.y` 夹到 `>= 32.0`。
 * 逐指令对照（`th08:0x44e8ec`–`0x44e92f`）：读完敌人坐标 → `fadd ds:0x4b42cc`（32.0）→
 * `fcomp 32.0` + `test ah,0x5`/`jp` ⇒ 不小于 32 就写回 32.0（= 下限）。 */
int yk_option_chase_goal(const float target_xy[2], float out[2])
{
    if (!target_xy || !out) return 0;
    out[0] = target_xy[0];
    out[1] = target_xy[1] + YK_OPT_CHASE_GOAL_DY;
    if (out[1] < YK_OPT_HOME_Y_MIN) out[1] = YK_OPT_HOME_Y_MIN;
    return 1;
}

int yk_option_chase_want(int target_present, int age_frames)
{
    if (!target_present) return 0;
    if (age_frames < 0) age_frames = 0;
    return age_frames >= YK_OPT_CHASE_MIN_AGE;
}

/* ------------------------------------------------------------------ §3.4 弹幕 */
/* 档位表 = **不覆盖**（全 `YK_SHOT_DMG_NOT_SET` = 保留 shooter 记录自己写进 `bullet+0x9c` 的 damage）。
 * 档序按 TH08 六档阈值（`8/24/48/80/128/999`，人妖两表共用）压到 TH18 的五个火力档。
 *
 * ★ 2026-09-12：TH08 妖侧的每档伤害**已经抽出来了**（一手：`local/th08.v1.00d/dat/ply03as.sht`，
 * 读点 `th08:0x451805`）—— 逐档「路数 / 每发伤害」＝ thr8: 3 发 48/36/36、thr24: 3 发 48/32/32、
 * thr48: 5 发 28/32/28/32/32、thr80: 7 发 24/30/24/32/22/22/32、thr128: 9 发 24/26/30/26/24/22/24/24/22。
 * **为什么仍不填**：TH08 的单发伤害是**配着「路数」**设计的 —— 单发随 P 大致**下降**、路数随 P
 * 上升（3→9），总 DPS 才是升的。我们的形态是**一档固定**（`YK_SHOT_SET_YOUKAI` 那一组，
 * 满 P 形态），把这串随 P 下降的单发值填进来会让「P 越高越弱」，那是**照抄数据却没有照抄结构**。
 * ⇒ 保持不覆盖 = 一个诚实的「一档固定、伤害不随 P 变」；要不要另设一条上升曲线属数值设计
 * （报告 §4 ❓ 已登记）。*/
static const int32_t yk_shot_damage[YK_SHOT_TIERS] = {
    YK_SHOT_DMG_NOT_SET,   /* 档 0 = P < 100  */
    YK_SHOT_DMG_NOT_SET,   /* 档 1 = P < 200  */
    YK_SHOT_DMG_NOT_SET,   /* 档 2 = P < 300  */
    YK_SHOT_DMG_NOT_SET,   /* 档 3 = P < 400  */
    YK_SHOT_DMG_NOT_SET,   /* 档 4 = P >= 400 */
};

int yk_shot_tier(int power)
{
    int t;
    if (power <= 0) return 0;
    t = power / YK_SHOT_POWER_PER_LEVEL;
    if (t >= YK_SHOT_TIERS) t = YK_SHOT_TIERS - 1;       /* 满 P 以上（含 P 溢出）钳到最高档 */
    return t;
}

int32_t yk_shot_tier_damage(int tier)
{
    if (tier < 0) tier = 0;
    if (tier >= YK_SHOT_TIERS) tier = YK_SHOT_TIERS - 1;
    return yk_shot_damage[tier];
}

int yk_set_is_youkai(uint32_t set, const yk_sets_t *s)
{
    /* ★ 2026-09-22：判据 = `set` 等于 `0x16`，**或**落在逐档组那一片 `[tier_first, +tier_count)`
     * 里。必须**精确**（不能写成一个宽区间）：`0x17..0x27` 是 77/116/117 的组、`0x28..0x47` 是
     * 113/111 的组 —— 宽区间会把它们的弹也认成妖侧弹、套上 TH08 的伤害表（静默改别人的卡）。*/
    if (s == 0) return 0;                                /* 没给集合 ⇒ 什么都**不是**我们的（安全侧）*/
    if (s->tier_count == 0u) return (int)(set == s->single);
    if (set == s->single) return 1;
    return set >= s->tier_first && set < s->tier_first + s->tier_count;
}

int yk_bullet_is_youkai(uint32_t packed_id, const yk_sets_t *s)
{
    /* 只看**打包 id 的 set 字节**（`(packed_id >> 8) & 0xff`）。引擎只为我们那一组里**存在的行**
     * 发弹（`0x40a9c0` 的行循环 + `0x40aa7d or eax,edi` 打包），所以行号不需要再夹
     * `YK_SHOT_SET_YOUKAI_ROWS` —— 换 index 也好、改组内行数也好，这一条判据都自洽。*/
    return yk_set_is_youkai((packed_id >> 8) & 0xffu, s);
}

int yk_bullet_damage_apply(int32_t current, uint32_t packed_id, int power,
                           const yk_sets_t *s, int32_t *out)
{
    int32_t want;
    if (!yk_bullet_is_youkai(packed_id, s)) return 0;    /* 人侧弹 / 别的卡的弹一律不碰 */
    want = yk_shot_tier_damage(yk_shot_tier(power));
    if (want == YK_SHOT_DMG_NOT_SET) return 0;           /* 占位：档位表还是空的 ⇒ 不覆盖 */
    if (want == current) return 0;
    if (out) *out = want;
    return 1;
}

int yk_shotset_usable(int fire_rate)
{
    /* 记录 `+0x00` 是 `fire_rate`（i8）：符号位 = shooterset 终止符，`0` 在装备卡路径上整数除零
     *（`0x40a9c0` 的 `idiv ecx`（`0x40aa4f`）没有 0 分支，engine/sht/th18/02 §2）。
     * ⇒ 「那一格到底装的是我们的记录还是原生那条」这个事实**可以运行期自检**，不用另立开关：
     *   · 原生 `/ build/sht 没重跑` ⇒ 那格还是 `fire_rate = 0` ⇒ 返回 0（**正确地拒绝发射**，
     *     见 youkai.c 的 on_tick_shooters / on_native_shot_suppress；也正因如此，
     *     原生弹被压掉时那一句「一发弹都没有」的日志才有意义）；
     *   · 我们的记录 = TH08 满 P 档那 9 行，`fire_rate ∈ {3,5,6,10}` ⇒ 返回 1。*/
    return fire_rate > 0 && fire_rate < 128;
}

/* ------------------------------------------------------- §3.5 发射口 → 使魔 */
void yk_emit_world_pos(const float enemy_xy[2], const float off3[3], const float org3[3], float out_xy[2])
{
    float ox, oy;
    if (!out_xy) return;
    if (org3 && org3[2] > YK_ORIGIN_ABS_THRESHOLD) {     /* 绝对模式（etOffsetAbs）：原点 + 相对偏移 */
        ox = org3[0]; oy = org3[1];
    } else {                                             /* 相对模式：敌人位置 */
        ox = enemy_xy ? enemy_xy[0] : 0.0f;
        oy = enemy_xy ? enemy_xy[1] : 0.0f;
    }
    if (off3) { ox += off3[0]; oy += off3[1]; }
    out_xy[0] = ox;
    out_xy[1] = oy;
}

int yk_emit_active(uint16_t prop_type) { return prop_type != 0; }

/* ★★★ 2026-09-26（owner #37，两次实跑同一条）：**这条槽的发射点是不是就在敌人本体上**
 *   （一手依据与「为什么严格 == 0」写在 `youkai_core.h` 的声明那段）。*/
int yk_emit_on_body(const float off3[3], const float org3[3])
{
    if (!off3) return 0;
    if (org3 && org3[2] > YK_ORIGIN_ABS_THRESHOLD) return 0;   /* 绝对模式：位置是脚本给的，不算本体 */
    return off3[0] == 0.0f && off3[1] == 0.0f;
}

/* 槽 → 皮肤变体（出处 / 语义在 youkai_core.h）。**纯查表逻辑**，卡侧只把它换成脚本宏。*/
int yk_fam_variant(int slot)
{
    if (slot < 0) return 0;
    return slot & 3;                                      /* 四色一套一槽；槽 >= 4 的兜底见头文件注 */
}

void yk_map_reset(yk_map_t *m)
{
    unsigned i;
    if (!m) return;
    for (i = 0; i < YK_FAM_MAX; ++i) {
        m->e[i].enemy_id = 0;
        m->e[i].fam_id = 0;
        m->e[i].prev_life = YK_LIFE_UNSAMPLED;
        m->e[i].x = 0.0f;
        m->e[i].y = 0.0f;
        m->e[i].slot = 0;
        m->e[i].seen = 0;
        m->e[i].active = 0;
        m->e[i].broken = 0;
        m->e[i].fam_seen = 0;
        m->e[i].no_fam_frames = 0;
        m->e[i].rearm_pending = 0;      /* ★ route A：整表清空（撤卡 / 松低速 / 局末 / 断链）⇒ 没有待处理的「下一波」 */
    }
}

unsigned yk_map_fam_count(const yk_map_t *m)
{
    unsigned i, n = 0;
    if (!m) return 0;
    for (i = 0; i < YK_FAM_MAX; ++i) if (m->e[i].fam_id) ++n;
    return n;
}

unsigned yk_map_heals(const yk_map_t *m)
{
    return m ? m->heals : 0u;
}

/* ★★ route A（2026-09-22）：闸被「下一波 `etOn`」松开的次数。与 `heals` 一样**只增不清**
 * （整表 reset 也不清）—— 卡侧每帧与自己的快照比对后打日志，涨了就说明波次级复位在起作用。 */
unsigned yk_map_rearms(const yk_map_t *m)
{
    return m ? m->rearms : 0u;
}

unsigned yk_map_releases(const yk_map_t *m)
{
    return m ? m->releases : 0u;
}

yk_slot_t *yk_map_find(yk_map_t *m, uint32_t enemy_id, int slot)
{
    unsigned i;
    if (!m || !enemy_id || slot < 0 || slot >= YK_EMIT_SLOTS) return 0;
    for (i = 0; i < YK_FAM_MAX; ++i)
        if (m->e[i].enemy_id == enemy_id && m->e[i].slot == (uint8_t)slot) return &m->e[i];
    return 0;
}

yk_slot_t *yk_map_find_fam(yk_map_t *m, uint32_t fam_id)
{
    unsigned i;
    if (!m || !fam_id) return 0;
    for (i = 0; i < YK_FAM_MAX; ++i) if (m->e[i].fam_id == fam_id) return &m->e[i];
    return 0;
}

/* ★★★ 2026-09-22（P1）：**只读**版判据 —— 「这个 id 是不是我表里活着的使魔」。
 * 与 `yk_fam_broken` 的区别是**它一个字节都不写**（后者是事件：清 `fam_id` + 置 `broken`）。
 * 卡侧每帧的「排除我们自己的使魔」这类判据一律用它；理由与那次真事故（32 格跟随 VM 表泄漏）
 * 见 `youkai_core.h` 里本函数上面那一整段。 */
int yk_map_is_fam(const yk_map_t *m, uint32_t fam_id)
{
    unsigned i;
    if (!m || !fam_id) return 0;
    for (i = 0; i < YK_FAM_MAX; ++i) if (m->e[i].fam_id == fam_id) return 1;
    return 0;
}

void yk_frame_begin(yk_map_t *m)
{
    unsigned i;
    if (!m) return;
    for (i = 0; i < YK_FAM_MAX; ++i) m->e[i].seen = 0;
}

static yk_slot_t *yk_map_alloc(yk_map_t *m, uint32_t enemy_id, int slot)
{
    unsigned i;
    for (i = 0; i < YK_FAM_MAX; ++i) {
        if (m->e[i].enemy_id) continue;
        m->e[i].enemy_id = enemy_id;
        m->e[i].fam_id = 0;
        m->e[i].prev_life = YK_LIFE_UNSAMPLED;
        m->e[i].slot = (uint8_t)slot;
        m->e[i].broken = 0;
        m->e[i].fam_seen = 0;
        m->e[i].no_fam_frames = 0;
        m->e[i].rearm_pending = 0;                        /* ★ route A：新条目没有待处理的「下一波」 */
        return &m->e[i];
    }
    return 0;                                             /* 表满：放弃这个槽（卡侧记日志）*/
}

static void yk_fill_action(yk_action_t *a, int kind, const yk_slot_t *e)
{
    if (!a) return;
    a->kind = kind;
    a->enemy_id = e->enemy_id;
    a->fam_id = e->fam_id;
    a->slot = (int)e->slot;
    a->x = e->x;
    a->y = e->y;
}

int yk_note_slot(yk_map_t *m, uint32_t enemy_id, int slot, int active, const float xy[2],
                 yk_action_t *act)
{
    yk_slot_t *e;
    int was_active;

    if (act) {
        act->kind = YK_ACT_NONE;
        act->enemy_id = enemy_id;
        act->fam_id = 0;
        act->slot = slot;
        act->x = xy ? xy[0] : 0.0f;
        act->y = xy ? xy[1] : 0.0f;
    }
    if (!m || !enemy_id || slot < 0 || slot >= YK_EMIT_SLOTS) return 0;

    e = yk_map_find(m, enemy_id, slot);
    if (!e) {
        /* 不活跃的槽**不建条目**（表小、每帧扫描短）；活跃且无条目 ⇒ 1:1 映射要求一只使魔。*/
        if (!active) return 0;
        e = yk_map_alloc(m, enemy_id, slot);
        if (!e) return 0;
        e->seen = 1;
        e->active = 1;
        e->x = act ? act->x : 0.0f;
        e->y = act ? act->y : 0.0f;
        yk_fill_action(act, YK_ACT_CREATE, e);
        return 1;
    }

    was_active = e->active != 0;
    e->seen = 1;
    e->active = (uint8_t)(active != 0);
    if (xy) { e->x = xy[0]; e->y = xy[1]; }

    /* 发射口被 ECL 重新武装（`etNew(i)` 清零 → `etSprite(i)` 写 type，spec §6.3）⇒ 「打破」复位。
     * 「下一波要重开」这条就是这么自动成立的（review §6.3 末）。*/
    if (active && !was_active) { e->broken = 0; e->fam_seen = 0; e->no_fam_frames = 0; }  /* ★ ECL 重新武装 ⇒ 关闸复位 */

    /* ★★ 2026-09-22（工单 E / route A）：**「下一波发弹」复位**（主路，常量与理由见 `youkai_core.h` §3.5.2）。
     * 闸（`yk_shot_slot_gate`，在 `etOn` 那一刻被调用）夹住这个槽时置了 `rearm_pending`；这里是**同帧消费**：
     * 清掉闸的记忆（`fam_seen`）+ 拆掉「不再重建」的锁（`broken`）⇒ 下面那条 1:1 分支**本次调用就建**。
     * ⚠ 位置必须在 `broken` 判定与 TTL 记账**之前**（否则本帧又会被 TTL/`broken` 挡一次）。
     * ⚠ 与上面那条边沿复位是**两条独立的路**：边沿那条依赖「卡能看见 `type == 0` 的帧」，
     *   而一手实测（22 份原生 ECL、282/282 处）`etNew` 与同槽 `etSprite` **永远同帧** ⇒ 那条基本观测不到；
     *   本路依赖的是引擎自己的发弹事件，实战里必然发生。*/
    if (e->rearm_pending) {
        e->rearm_pending = 0;
        e->broken = 0;
        e->fam_seen = 0;
        e->no_fam_frames = 0;
    }

    /* ★★ 2026-09-22（#7）：**闸的到期自愈**（常量与三条理由见 `youkai_core.h`）。
     * 判据 = 「记忆里有使魔、当前没有」，也就是闸夹着的那个状态本身；连续 `YK_EMIT_BROKEN_TTL`
     * 帧都这样 ⇒ 无条件放开（清 `fam_seen` 让闸放行、清 `broken` 让 1:1 映射把它建回来）。
     * ⚠ 放在**边沿复位之后**：真被重新武装过的那一帧按上面的边沿处理，不记年龄。*/
    if (e->fam_seen && !e->fam_id) {
        if (e->no_fam_frames < YK_EMIT_BROKEN_TTL) ++e->no_fam_frames;
        if (e->no_fam_frames >= YK_EMIT_BROKEN_TTL) {
            e->fam_seen = 0;                              /* 闸放行 */
            e->broken = 0;                                /* 允许重建（下面那条分支本帧就建）*/
            ++m->heals;                                   /* ★ 记账：卡侧每帧读一次打日志（#7 验收）*/
        }
    } else {
        e->no_fam_frames = 0;
    }

    /* 状态差分：不活跃且有使魔 → 撤。*/
    if (!active && e->fam_id) {
        yk_fill_action(act, YK_ACT_WITHDRAW, e);
        e->fam_id = 0;
        e->prev_life = YK_LIFE_UNSAMPLED;
        return 1;
    }
    /* 活跃、没有使魔、也没被打死过 ⇒ 建。*/
    if (active && !e->fam_id && !e->broken) {
        yk_fill_action(act, YK_ACT_CREATE, e);
        return 1;
    }
    return 0;
}

int yk_frame_end(yk_map_t *m, yk_action_t *out, int max)
{
    unsigned i;
    int n = 0;
    if (!m) return 0;
    for (i = 0; i < YK_FAM_MAX; ++i) {
        yk_slot_t *e = &m->e[i];
        if (!e->enemy_id || e->seen) continue;            /* 空条目 / 本帧还在 */
        if (e->fam_id) {
            /* 发射口所属敌人没了（过场全清 / 静默移除）⇒ 它身上的使魔也一起撤。
             * ⚠ 输出数组装不下就**不退休**这个条目（否则会留下一只没人管的真敌人）。*/
            if (out && n >= max) continue;
            yk_fill_action(&out[n], YK_ACT_WITHDRAW, e);
            ++n;
        }
        e->enemy_id = 0;                                  /* `enemy_id` 单调不复用 ⇒ 退休是安全的 */
        e->fam_id = 0;
        e->prev_life = YK_LIFE_UNSAMPLED;
        e->slot = 0;
        e->seen = 0;
        e->active = 0;
        e->broken = 0;
        e->fam_seen = 0;
        e->no_fam_frames = 0;
        e->rearm_pending = 0;                             /* ★ route A：退休的条目不带「下一波」 */
    }
    return n;
}

void yk_fam_created(yk_map_t *m, uint32_t enemy_id, int slot, uint32_t fam_id)
{
    yk_slot_t *e = yk_map_find(m, enemy_id, slot);
    if (!e) return;
    e->fam_id = fam_id;
    e->fam_seen = 1;                                      /* ★ 关闸的「曾经有过」记忆 */
    e->no_fam_frames = 0;                                 /* ★ 建回来了 ⇒ 闸的年龄归零（#7）*/
    e->rearm_pending = 0;                                 /* ★ route A：使魔活着 ⇒ 没有待处理的「下一波」 */
    e->prev_life = YK_LIFE_UNSAMPLED;                     /* 新使魔：下一帧才允许出 delta */
}

int yk_fam_broken(yk_map_t *m, uint32_t fam_id)
{
    yk_slot_t *e = yk_map_find_fam(m, fam_id);
    if (!e) return 0;
    e->fam_id = 0;
    e->no_fam_frames = 0;                                 /* ★ #7：从「它没了的这一刻」开始计闸的年龄 */
    e->prev_life = YK_LIFE_UNSAMPLED;
    /* ★ route A：这里**故意不清** `rearm_pending`。它是「这一槽的下一次 `etOn` 已经来过」的证据；
     *   若正巧与死亡同帧（段首 `enmKillAll` 与 `etOn` 的先后本来就不定），带着它进 tick 只会让
     *   使魔**早一帧**回来 —— 而清掉它就等于把那次 `etOn` 白丢。两种都不会夹死（下面是 TTL 兜底）。*/
    /* ★ 「打死使魔 ⇒ 关掉那个发射口」的**近似**：真正的关槽要 hook `0x434019`（`etOn` 里 `push eax`
     * 前，`esi = enemy + i*0x488` 可反推 `i`）—— 那是**新断点**（sites.py，本卡不能加）。
     * 这里做的两件事替代它：① 使魔不再重建（本标志）② 它带来的转嫁归零（没有使魔就没差分）。
     * ⇒ ⚠ 发射口**本身还在开火**（ECL 直接调 etOn，不经我们），这是已知的近似缺口。*/
    e->broken = 1;
    return 1;
}

void yk_fam_gone(yk_map_t *m, uint32_t fam_id)
{
    yk_slot_t *e = yk_map_find_fam(m, fam_id);
    if (!e) return;
    e->fam_id = 0;
    e->no_fam_frames = 0;                                 /* ★ #7：同上（松手那一刻重新计年龄）*/
    e->prev_life = YK_LIFE_UNSAMPLED;
    /* ★ 2026-09-12：这里**不能**置 `broken`（试过，直接把既有的「静默移除 ⇒ 下一帧补建」断言打红）。
     * 理由：`broken` 唯一的复位口是 `yk_note_slot` 的 inactive→active 边沿，而本函数处理的是
     * **我们自己主动松手**的两条路：
     *   ① 真的静默消失（过场 destroy_all / 被打死但没走到死亡事件）：发射口多半还在，
     *      1:1 映射**下一帧就该补建**（既有单测钉的就是这条）；
     *   ② `yk_run_action` 的 WITHDRAW / `yk_transfer_tick` 的「查不到就松开」：我们刚把它标了删除或
     *      正在收尾，下一帧同一条路会在 `broken == 0` 时重建/重钉 —— 置 broken 就把这条路堵死。
     * ⇒ 结论：**松手不置 `broken`**，`broken` 只留给「真被打死」（`yk_fam_broken`）。
     *   关闸判据改用 `fam_id != 0` 记「这一槽曾经有过我们的使魔」（见下）。
     * ⚠ 由此产生的半帧空窗由发射口闸兜住：`fam_id` 清 0 而敌人还在 ⇒ 该槽当帧不发射，
     *   重建发生在同一帧的 `yk_familiars_tick` 里（AbilityManager tick 早于敌人结算，顺序安全）。 */
}

/* §3.5.1 发射口关闸的判据（纯逻辑；断点/语义、以及「为什么不能只看有没有使魔」的三分
 * 都在 youkai_core.h 的注释里）。**不读 `props[].type`** ⇒ 不受 etOn / etSprite 先后影响。
 *
 * 判据 = **「这一槽曾经有过我们的使魔（`fam_id != 0` 的记忆）而现在回查不到了」**。
 *   `have_fam_id` = 卡侧按 id 回查的结果（0 = 查不到）。
 *   ⚠ 用 `fam_id` 而不是 `broken` 当「曾经有过」的记忆：`yk_fam_gone` 会清 `fam_id`，
 *     所以「清 0 那一刻」正是闸该夹上的那一刻；而下一帧重建回填 `fam_id` 后闸自动放开。 */
int yk_shot_slot_suppressed(const yk_map_t *m, uint32_t enemy_id, int slot, uint32_t have_fam_id)
{
    const yk_slot_t *e;

    if (!m || !enemy_id || slot < 0 || slot >= YK_EMIT_SLOTS) return 0;  /* 不是我们的槽域 ⇒ 绝不掐 */
    e = yk_map_find((yk_map_t *)m, enemy_id, slot);
    if (!e) return 0;                        /* ③ 表里没这个槽 ⇒ 我们**从没管过** ⇒ 放行 */
    if (have_fam_id) return 0;               /* 使魔还在（卡侧已按 id 核过）⇒ 放行 */
    /* ② 曾经有过我们的使魔、现在没了 ⇒ 掐。`fam_seen` 是唯一可靠的「曾经有过」记忆：
     *    `fam_id` 会被 broken / gone 清掉，`broken` 会被重新武装清掉 ⇒ 两个都不能单独用。
     * ① 从没建过（`fam_seen == 0`，含「建被排到下一个 on_tick_2」）⇒ **放行**，
     *    否则第一帧就把槽掐死、永远建不出来。
     * ⚠ 复位口 = `yk_note_slot` 的 inactive→active 边沿（ECL 重新武装）+ `yk_map_reset`
     *    （删卡 / 松开低速 / 局末 / 断链）+ ★ **route A：该槽下一次 `etOn`**
     *    （2026-09-22 主路，见 `yk_shot_slot_gate`）+ `YK_EMIT_BROKEN_TTL` 帧的到期自愈（兜底）。
     *    没有后两条时，一个持续活跃、ECL 不再 `etNew` 的发射口会**永久哑火** —— 那就是 th08 #7 的现象。 */
    return e->fam_seen ? 1 : 0;
}

/* ★★ §3.5.2 route A：闸 + 「下一波发弹」记账（语义 / 一手根因 / 与 TTL 的分工见 `youkai_core.h`）。
 *
 * 卡侧（`yk_on_shot_slot`，断点 `0x433ffa`）只调本函数：返回 1 ⇒ SDK 置 `retaddr = 0x43402d`
 * ⇒ 本帧这一槽不发射；返回 0 ⇒ 照常发。
 * 副作用只有一处：**夹住的槽被问到** ⇒ 置 `rearm_pending`（每个夹持期只记一次账），
 * 卡 tick 的 `yk_note_slot` 同帧消费它 ⇒ 清闸 + 重建使魔。
 *
 * ⚠ 返回 1（不是放行）是**故意的**：让这一条 `etOn` 落空 = 「使魔没了，这一波这个口子就是哑的」
 *   （TH08 里使魔与发射口一体，使魔没了就不会有这一发 —— 见
 *   `engine/enemy/th08/01-familiars-and-anm.md` §3.3）。代价 = **每次使魔死亡最多丢 1 条 `etOn`**
 *   （下一帧使魔已重建 ⇒ 之后的 `etOn` 全放行）。若把这里改成 `return 0`，就变成「这一个口子一帧都不哑」
 *   —— 那是**放行一条没有使魔的发射**，与 owner 保留的语义相反，所以不做。 */
/* ★★★ 2026-09-22（P8-3 / **route A′**）：闸的第二问即放行 —— 把「恢复」从卡 tick 解耦。
 *
 * 原来的 route A 只做两件事：夹住时置 `rearm_pending`，**卡 tick**（`yk_note_slot`）同帧消费它
 * ⇒ 清夹持态 + 重建使魔。缺口（2026-09-22 只读调查查实）：没有任何东西保证卡 tick 会跑到 ——
 * 卡 tick 只在**聚焦**时跑（`on_tick_2` 的 `!focused` 早退），而闸对「未聚焦」不设防
 * （见 `yk_on_shot_slot` 那边的 P8-1）。⇒ 一旦在某个相位夹住，恢复可能被拖到 120 帧 TTL 兜底。
 *
 * **本函数每帧每槽都有全部信息**（断点在 `etOn` 那一刻，`enemy_id`/`slot` 都在手上）⇒ 直接在这里闭环：
 *   · 第一次被问（`rearm_pending == 0`）⇒ 掐（`return 1`，语义不变：使魔没了这一发就是哑的），
 *     记 `rearm_pending = 1` + `rearms++`；
 *   · **第二次被问**（上一次那条 `etOn` 已经被掐掉了）⇒ **放行并当场清夹持态**
 *     （`rearm_pending` / `fam_seen` / `broken` / `no_fam_frames` 全清）⇒ 1:1 映射下一帧就能把它建回来
 *     —— 最快的复位口，不再等卡 tick、也不再等 TTL。
 * ⚠ 与 `yk_note_slot` 的消费是**幂等**的：谁先清都是同一组标志位。
 * ⚠ 计数：`rearms` 记「掐掉过几次」（夹持期数），`releases` 记「route A′ 放过几次」（只增不清）。*/
int yk_shot_slot_gate(yk_map_t *m, uint32_t enemy_id, int slot, uint32_t have_fam_id)
{
    yk_slot_t *e;

    if (!yk_shot_slot_suppressed(m, enemy_id, slot, have_fam_id)) {
        /* 放行。⚠ 放行**不代表**没有待办：夹持期里被放行只可能发生在「使魔活着」那条路上，
         *   而那条路已经在 `yk_fam_created` 里把 `rearm_pending` 清掉了 ⇒ 这里不用管。 */
        return 0;
    }
    e = yk_map_find(m, enemy_id, slot);      /* 上面那支已确认条目存在（否则它返回 0）*/
    if (e && e->rearm_pending) {
        /* ★ route A′：上一次那条 `etOn` 已经哑过一发了 ⇒ 这一次放行 + 当场拆夹持态。*/
        e->rearm_pending = 0;
        e->fam_seen = 0;
        e->broken = 0;
        e->no_fam_frames = 0;
        ++m->releases;
        return 0;
    }
    if (e && !e->rearm_pending) {
        e->rearm_pending = 1;                /* ★ 「下一波到了」：卡 tick 也会消费它（幂等）*/
        ++m->rearms;                         /* ★ 记账（只增不清）：涨了 = 波次级复位在起作用 */
        /* ★ P8-4：年龄按**「被 `etOn` 问过几次」**计（不再只按卡 tick 的聚焦帧计）——
         *   卡 tick 在未聚焦时早退，那样会把兜底 TTL 冻住；断点这条每帧每槽都会到。*/
        if (e->no_fam_frames < 0xffffu) ++e->no_fam_frames;
    }
    return 1;
}

int yk_withdraw_all(yk_map_t *m, yk_action_t *out, int max)
{
    unsigned i;
    int n = 0, truncated = 0;
    if (!m) return 0;
    for (i = 0; i < YK_FAM_MAX; ++i) {
        yk_slot_t *e = &m->e[i];
        if (!e->enemy_id || !e->fam_id) continue;
        if (out && n >= max) { truncated = 1; break; }   /* 缓冲区满：**剩下的先留着** */
        yk_fill_action(&out[n], YK_ACT_WITHDRAW, e);
        ++n;
        e->fam_id = 0;
        e->prev_life = YK_LIFE_UNSAMPLED;
    }
    /* ★ 只有「全部处理完」才清表。原来无条件 `yk_map_reset()` —— 缓冲区不够时会**丢掉还没撤的条目**
     *（`fam_id` 被清零 ⇒ 卡再也不认识那几个真敌人 = 孤儿敌机：既不钉位置也不会被静默撤退）。
     * 现在被截断时表保持原样，下一帧接着撤（不变式：表里有条目 ⇔ 场上那个载体还在）。*/
    if (!truncated) yk_map_reset(m);
    return n;
}

int32_t yk_clamp_spawn_life(int32_t life)
{
    if (life < 1) return 1;                               /* 0 及以下没有意义（引擎拿它算血条 / 死亡判定）*/
    if (life >= YK_LIFE_LIMIT) return YK_LIFE_MAX_ALLOWED;
    return life;
}

/* --------------------------------------------------------------- §3.7 伤害转交 */
void yk_transfer_reset(yk_transfer_t *t)
{
    if (!t) return;
    t->carry = 0;
    t->pending = 0;
}

int32_t yk_half_with_carry(int32_t delta, int32_t *carry)
{
    int32_t sum, half, rem;
    if (delta <= 0) return 0;
    sum = delta + (carry ? *carry : 0);
    half = sum / YK_TRANSFER_DEN;
    rem = sum - half * YK_TRANSFER_DEN;
    if (carry) *carry = rem;
    return half;
}

void yk_transfer_feed(yk_transfer_t *t, int32_t delta)
{
    if (!t || delta <= 0) return;
    t->pending += yk_half_with_carry(delta, &t->carry);
}

int32_t yk_transfer_clamp(int32_t pending, int32_t cap)
{
    if (pending <= 0) return 0;
    if (cap > 0 && pending > cap) return cap;
    return pending;
}

int32_t yk_sample_life(yk_slot_t *e, uint32_t fam_id, int32_t life_now)
{
    int32_t d;
    if (!e) return 0;
    if (e->fam_id != fam_id || e->prev_life == YK_LIFE_UNSAMPLED) {
        e->fam_id = fam_id;
        e->prev_life = life_now;
        return 0;                                         /* 第一次看到这只使魔（或换了对象）*/
    }
    d = e->prev_life - life_now;
    e->prev_life = life_now;
    /* life **变大** = 引擎自己重算（换攻击 / 复活），不是「负数伤害」⇒ 归 0。
     * 我们自造的惰性空壳没有 ECL、没有符卡，理论上不会发生（research §7.4 仍要求对账）。*/
    return d > 0 ? d : 0;
}

int yk_in_transfer_zone(float fx, float fy, float cx, float cy, float w, float h)
{
    float dx = fx - cx, dy = fy - cy;
    if (dx < 0.0f) dx = -dx;
    if (dy < 0.0f) dy = -dy;
    return (dx <= w * 0.5f + YK_FAM_HURTBOX_HALF) && (dy <= h * 0.5f + YK_FAM_HURTBOX_HALF);
}

/* ------------------------------------------------------ §3.10 人妖槽 */
int yk_gauge_clamp(int gauge)
{
    /* `th08:0x43c0fa`/`:0x43c103`（低于下限 ⇒ 写下限）与 `th08:0x43c128`/`:0x43c131`（高于上限 ⇒ 写上限）。
     * 两条都是**有符号 i16 比较**（`movsx` + `cmp` + `jge`/`jle`）⇒ C 的 int 比较同义。*/
    if (gauge < YK_GAUGE_HUMAN_LIMIT) return YK_GAUGE_HUMAN_LIMIT;
    if (gauge > YK_GAUGE_YOUKAI_LIMIT) return YK_GAUGE_YOUKAI_LIMIT;
    return gauge;
}

/* ★★★ 2026-09-22 夜（P-E 单人反向封顶）：一手见 `youkai_core.h` 的同名小节。*/
void yk_gauge_limits_solo(int focused, int32_t *human_limit, int32_t *youkai_limit)
{
    if (focused) {                                   /* 妖侧（低速）：反侧 = 人侧，封在 −2000 */
        if (human_limit)  *human_limit  = -YK_GAUGE_SOLO_OTHER_LIMIT;
        if (youkai_limit) *youkai_limit =  YK_GAUGE_YOUKAI_LIMIT;
    } else {                                         /* 人侧：反侧 = 妖侧，封在 +2000 */
        if (human_limit)  *human_limit  =  YK_GAUGE_HUMAN_LIMIT;
        if (youkai_limit) *youkai_limit =  YK_GAUGE_SOLO_OTHER_LIMIT;
    }
}

int yk_gauge_clamp2(int gauge, int32_t human_limit, int32_t youkai_limit)
{
    if (gauge < human_limit) return (int)human_limit;
    if (gauge > youkai_limit) return (int)youkai_limit;
    return gauge;
}

int yk_gauge_add2(int gauge, int delta, int32_t human_limit, int32_t youkai_limit)
{
    int32_t v = (int32_t)(int16_t)((int32_t)gauge + (int32_t)delta);   /* 先按 i16 截断（同 `yk_gauge_add`）*/
    /* ★★★ 2026-09-27（owner ③：「**松开/按下 shift 不会导致人妖值瞬间变化**，甚至可能瞬间按下松开
     *   or 松开按下 人妖值可能都不变」）：**夹取不许「回收」已经在区间外的值**。
     *
     * 为什么：本卡的限值跟形态走（`yk_gauge_limits_solo`，反侧封 2000），于是按/松 Shift 那一帧
     * **限值本身就换了**；若照旧直接 `clamp2(v)`，一个停在 −5000 的槽会被当帧**拉成 −2000**
     * （+30% 的瞬跳）—— 而一手**没有任何一处**在换形时改槽值：`Player::move` 的两条边沿
     * （`th08:0x44b11f` / `th08:0x44b379`）只写 `player+8 = 0` 与 `player+0xe2ae8.SetCurrent(0)`，
     * 全 exe 8 处 `AddToYoukaiGauge` 调用点一处都不在边沿里（穷举见 `youkai_core.h` 的同一段）。
     * ⇒ 规则：**只有当传入值本来就在 [hl, yl] 内时才夹**；已经在外面时只让它按 `delta` 走。
     *   （安全：本卡所有涨源的方向都由“当前形态”定 ⇒ 区间外的值只会朝区间**回来**，不会跑飞。）*/
    if ((int32_t)gauge >= human_limit && (int32_t)gauge <= youkai_limit)
        return yk_gauge_clamp2((int)v, human_limit, youkai_limit);
    return (int)v;
}

int yk_gauge_add(int gauge, int delta)
{
    /* `th08:0x43c0d9`–`:0x43c0e9`：`gauge = (int16)((int)gauge + delta)`（**先按 i16 截断再夹**）。
     * 这一步照抄：TH08 的中间值是 `eax += delta` 后 `mov WORD [...]`（存 i16）⇒ 溢出会绕回。
     * 本卡的 delta 都是小常数（±111 / ±200 / ±26000），不会绕；按 i16 截断只是为了逐位对齐。*/
    int32_t v = (int32_t)(int16_t)((int32_t)gauge + (int32_t)delta);
    return yk_gauge_clamp((int)v);
}

int yk_gauge_dead_zone(int gauge)
{
    /* `th08:0x44bf4d` `fcomp 9.0` → `:0x44bf61` `set_youkai_gauge(0)`：`|g| <= 9` 时归零。
     * （TH08 那边是 `fabs()` 比较，整数同义。）*/
    int v = gauge < 0 ? -gauge : gauge;
    return (v <= YK_GAUGE_DEAD_ZONE) ? 0 : gauge;
}

int yk_gauge_is_extremely_human(int gauge)  { return gauge <= YK_GAUGE_HUMAN_EFFECTS; }  /* th08:0x406d10 setle */
int yk_gauge_is_moderately_human(int gauge) { return gauge <= YK_GAUGE_BLUE_TINT; }      /* th08:0x406d40 setle */
int yk_gauge_is_extremely_youkai(int gauge) { return gauge >= YK_GAUGE_YOUKAI_EFFECTS; } /* th08:0x406d70 setge */
int yk_gauge_is_moderately_youkai(int gauge){ return gauge >= YK_GAUGE_RED_TINT; }       /* th08:0x406da0 setge */

int yk_gauge_orbs_delta(int focused) { return focused ? YK_GAUGE_PER_ORB : -YK_GAUGE_PER_ORB; }

/* ★★★ 2026-09-22（P3 第二批）：射击中每帧的槽增量（一手 `Player.cpp:445-452`，见 .h 的长注）。
 * `0x6590 / 0x3e7` 是**整数除法**（ZUN 的算式就是 `i32`）⇒ 26000/999 = 26。*/
/* ★★★ 2026-09-22 夜（P-C 订正）：**持续开火推进 = 时间斜坡**（一手 `th08:0x44be67`–`0x44bea8`）。
 * `adv_frames` = `player+0xe2ae8` 的对应物（换形 / 停火 ≥4 帧清零）。*/
int yk_gauge_ramp_delta(int adv_frames)
{
    int f = adv_frames;
    if (f < 0) f = 0;
    if (f > YK_GAUGE_RAMP_FRAMES) f = YK_GAUGE_RAMP_FRAMES;   /* 一手先 fcomp 300.0f 再封顶 21 */
    f = f / YK_GAUGE_RAMP_DIV;                                /* 一手 fdiv 15.0f 后 float→int（向零取整）*/
    if (f > YK_GAUGE_RAMP_CAP) f = YK_GAUGE_RAMP_CAP;         /* 21 是 `f2i` 的转换上界（一手 imm `0x41a80000`）——
                                                              * 帧数先夹到 300 ⇒ 实际最大 20，这一句只是安全界 */
    return f;
}

/* ★★★ 2026-09-22 夜（P-C 订正）：**停火回中**的三档（一手 `th08:0x44bf70`–`0x44bfde`，带符号）。*/
int yk_gauge_center_delta(int gauge)
{
    if (yk_gauge_is_extremely_youkai(gauge))    return -5;     /* `0x44bf70` */
    if (yk_gauge_is_moderately_youkai(gauge))   return -3;     /* `0x44bf87` */
    if (gauge > 0)                              return -2;     /* `0x44bf9e` */
    if (!yk_gauge_is_moderately_human(gauge))   return  2;     /* `0x44bfb0` */
    if (!yk_gauge_is_extremely_human(gauge))    return  3;     /* `0x44bfcc` */
    return 5;                                                  /* `0x44bfde` */
}

int yk_gauge_shoot_delta(int focused)
{
    return focused ? YK_GAUGE_SHOOT_DELTA : -YK_GAUGE_SHOOT_DELTA;
}

/* `AddToYoukaiGauge` 的第一道闸：`*(i32*)0x17d6ed4 != 0 && param_2 == 0 ⇒ 丢掉`
 * （`th08:0x43c0c2`/`:0x43c0cb`；`0x17d6ed4` = `g_Player.shotActive`）。
 * 本函数只回答「现在过不过闸」，条件由卡侧提供（TH18 侧 = 射击键按住）。*/
int yk_gauge_gate_blocks(int shooting)
{
    /* ★★★ 2026-09-27 订正（§BE #110）：本函数**只剩一个调用点** —— `yk_gauge_advance()` 里那道
     * **推进支**的闸，它对应 TH08 `player+0xfdc`（`shotActive`，`th08:0x44be0f` 的 `cmp [edx+0xfdc],0`；
     * 放符卡/炸弹时跳过推进支、只走 `±26/帧`）⇒ 参数就是 `shotActive`，由卡侧用 `yk_bomb_active()`
     * （`[bomb_mgr+0x30] != 0`）喂进来 ✓。
     *
     * ⚠ 另外三笔加数（击破 ±200 / 收刻符 ±111 / 擦弹 +100）**曾经**也走这里 —— 那是错的：
     *   它们对应的是 `AddToYoukaiGauge` 的第一道闸 `th08:0x43c0c2`（`ds:0x17d6ed4` + `param2 == 0`），
     *   而 `0x17d6ed4` **全 exe 没有任何写点**（同页 11 处引用全是读）且落在 BSS ⇒ 恒为 0
     *   ⇒ 那道闸永不生效 ⇒ 三笔加数**无条件照加**。卡侧已改成不做闸。*/
    return shooting ? 1 : 0;
}

uint32_t yk_gauge_color(int gauge)
{
    /* 一手 `AsciiManager.cpp:1035-1058`（顺序即优先级：极人 → 中人 → 极妖 → 中妖 → 白）。
     * 这三通道写的都是 `prefix.color1.r/g/b`，槽条用的是**同一个**颜色（`:1060` 把文字色拷给槽条 VM）。*/
    if (yk_gauge_is_extremely_human(gauge))  return 0x7070ffu;
    if (yk_gauge_is_moderately_human(gauge)) return 0xb0b0ffu;
    if (yk_gauge_is_extremely_youkai(gauge)) return 0xff7070u;
    if (yk_gauge_is_moderately_youkai(gauge))return 0xffb0b0u;
    return 0xffffffu;
}

/* TH08 **游玩区下沿**（640 空间）= 464 —— 相机 0 的视口是 `scale×(32,16,384,448)`
 * （`engine/anm/th18/02-render-stages.md` §2）⇒ `16 + 448 = 464`。y 的基准就是它：
 * 喂给 `yk_gauge_hud()` 的 `anchor` = **游玩区左下角**（TH18 表面 px = (257,928)，用户量过）。*/
#define YK_GAUGE_640_FLOOR 464.0f

void yk_gauge_hud(int gauge, const float anchor[2], float to640, yk_gauge_hud_t *out)
{
    /* 一手（**640 空间**的字面量）：槽条左上 (32,449)、图标 (32,449)/(144,449)、
     * 游标中心 (96 + g·112/20000, 453)、数字 (96 + g·80/20000, 446)、夜点 (80, 460)。
     * ★★★ 2026-09-26（owner #39「参考一下原生左下角框」）：**y 的基准从 481 改成 464**。
     *   一手 `AsciiManager::InitializeVms` 那套 y（449 / 453 / 446 / 460）是 **TH08 的 640 空间**里
     *   的绝对字面量，而 TH08 的**游玩区**正是 `x 32..416 / y 16..464`（`engine/anm/th18/02-render-stages.md`
     *   §2 的相机 0 视口 `scale×(32,16,384,448)`）—— 也就是说：**槽条的上沿 449 比游玩区下沿 464
     *   高 15 个单位**、夜点比它高 4 个（槽条/图标本来就在场地下沿上"骑着边"）。
     *   旧版把「块左下角」记成 640 空间的 **481**（= 449 + 32 的一个自造值）⇒ 折算到 TH18 表面上
     *   整块比原版**高 34 px**（`481 − 464 = 17 × 2`）。现在基准就是**游玩区下沿 464**：
     *   锚点 = 「游玩区左下角」，所有 y = `ay − (464 − y640)·px` ⇒ 与 TH08 逐条同构。
     *   ⚠ 图标 x 用**限值**算：`88 + limit·56/10000`（`AsciiManager::InitializeVms`）——
     *     本卡是 ±10000 ⇒ 恰好落在条的两端（32 / 144），与一手数值一致。
     * 实盘落点（TH18 1280×960、锚点 = 游玩区左下角 `(257,928)`、`px = 2`）：
     *   槽条 `(257,898)` 128×16×2 = 256×32 ⇒ `y 898..930`（跨过场地下沿 928 各 2 px，与 TH08 的
     *   `449..465` 跨 464 同构）；人间图标 `(257,898)`、妖怪图标 `(481,898)`；游标中心 `(385..497, 906)`；
     *   百分比基准 `(385..465, 892)`；夜点 `(353, 920)`。
     *
     * ★★★ 2026-09-22（P2）：**出口是表面 px**（`px = 1/to640` = 2）：本卡那些 VM 的脚本**显式写了**
     *   `resolutionMode(2)`（= `DAT × 0.5` = ×1）⇒ `th18:0x4063d0` 那一步按 1 单位 = 1 表面 px 走，
     *   尺寸那一支同形（`th18:0x4817c6`）⇒ 写 640 空间的数就会「位置高半个屏幕 + 整体缩小 50%」
     *   （owner th08 #14/#15 的实跑原话）。⚠ 那一行 `resolutionMode(2)` **不能删**：`layer(25)` 会经
     *   `set_layer th18:0x4066b0` 把 `flags_hi` bit22 置 1（= `resolutionMode 1` ⇒ **位置 ×DAT**）
     *   —— 少了它整块被画到屏外（owner #39；一手与实跑全文见 `youkai_core.h` 的 `yk_gauge_hud_t`
     *   与 `engine/anm/th18/01-vm-instantiate.md` §3c）。*/
    float k, ax, ay, g;
    int v;
    if (!out) return;
    k = (to640 > 0.0f) ? (1.0f / to640) : 2.0f;   /* 表面 px / 640 空间 px（TH18 = 2）*/
    ax = anchor ? anchor[0] : 0.0f;               /* 锚点本来就是**表面 px** ⇒ 直接用 */
    ay = anchor ? anchor[1] : 0.0f;
    v = yk_gauge_clamp(gauge);
    g = (float)v / (float)YK_GAUGE_SPAN;              /* −1..+1，TH08 是 float 除法（不是整数取整）*/
    out->px = k;
    out->bar_x = ax + (32.0f - 32.0f) * k;
    out->bar_y = ay - (YK_GAUGE_640_FLOOR - 449.0f) * k;   /* ★ 停稳位置 = 449（465 只是滑入起点）*/
    out->human_x = ax + ((88.0f + (float)YK_GAUGE_HUMAN_LIMIT * 56.0f / 10000.0f) - 32.0f) * k;
    out->human_y = ay - (YK_GAUGE_640_FLOOR - 449.0f) * k;
    out->youkai_x = ax + ((88.0f + (float)YK_GAUGE_YOUKAI_LIMIT * 56.0f / 10000.0f) - 32.0f) * k;
    out->youkai_y = out->human_y;
    out->cursor_x = ax + ((96.0f + g * 56.0f) - 32.0f) * k;   /* 112 / 2 = 56 */
    out->cursor_y = ay - (YK_GAUGE_640_FLOOR - 453.0f) * k;   /* 游标锚点 = 心 ⇒ 这是中心 */
    out->text_x = ax + (64.0f + g * 40.0f) * k;               /* 80 / 2 = 40；96 − 32 = 64 */
    out->text_y = ay - (YK_GAUGE_640_FLOOR - 446.0f) * k;     /* 游标 y − 7 = 446（640 空间）*/
    /* ★ 2026-09-22 夜（#27/#28）：**夜点**（一手：y=460、x 从 80 起；两者与百分比上下错开 14 px）。*/
    out->night_x = ax + (80.0f - 32.0f) * k;
    out->night_y = ay - (YK_GAUGE_640_FLOOR - 460.0f) * k;
}

/* ---------------------------------------- 人妖槽的百分比数字：逐字形布局（`th08:0x405e10`）
 * 逐条照抄 `AsciiManager::DrawPercentage`（`AsciiManager.cpp:1095-1225`）：
 *   ① 位数 `n`（只为算居中）：4 +（负号 1）+（≥10000 → 3 / ≥1000 → 2 / 否则 1）；
 *   ② 起始 x 偏移 = `−(n*3.5 − 3.5 − 4.0)`；
 *   ③ 负号（sprite 0x94）步进 7；
 *   ④ 整数部分与 `'.'`（0x93）的步进：整数位各 7、`.` 之后 5；
 *   ⑤ **小数两位是小号**（scale 0.8、`y += 2`），步进 5、5（最后一位后面的 7 只影响下一格 = `'%'`）；
 *   ⑥ 收尾 `'%'`（0x92，scale 复位、`y -= 2`）。
 * 返回字形数（`int`）；`out == NULL` 时只算位数（不崩）。**没有浮点库调用**（只有 `* / +`）。*/
int yk_gauge_text_glyphs(int gauge, yk_gauge_glyph_t *out)
{
    int n, absv, idx = 0;
    float x, y;

    if (gauge > YK_GAUGE_YOUKAI_LIMIT) gauge = YK_GAUGE_YOUKAI_LIMIT;    /* 一手：字段本身已被夹在 ±10000 */
    if (gauge < YK_GAUGE_HUMAN_LIMIT) gauge = YK_GAUGE_HUMAN_LIMIT;
    absv = (gauge < 0) ? -gauge : gauge;

    n = 4 + ((gauge < 0) ? 1 : 0);
    if (absv >= 10000)      n += 3;
    else if (absv >= 1000)  n += 2;
    else                    n += 1;

    /* ① 居中：`xPos = numDigits*3.5 − 3.5 − 4.0`，再 `pos.x -= xPos`。*/
    x = -((float)n * 3.5f - 3.5f - 4.0f);
    y = 0.0f;

/* ⚠ 宏参数必须用**大写**：成员名就叫 `dy`/`scale`，小写参数会被就地替换掉（`out[idx].dy`
 *   会变成 `out[idx].0.0f` —— 编译期会报 expected identifier）。*/
#define YK_EMIT(G, ADV, SC, DY) do {                                        \
        if (out) { out[idx].glyph = (uint8_t)(G); out[idx].dx = x;          \
                   out[idx].dy = y + (DY); out[idx].scale = (SC); }         \
        ++idx; x += (ADV);                                                  \
    } while (0)

    if (gauge < 0) YK_EMIT(YK_GAUGE_GLYPH_MINUS, 7.0f, 1.0f, 0.0f);

    if (absv >= 10000) {
        /* 固定显示 "100.00"（`AsciiManager.cpp:1133-1162`）*/
        YK_EMIT(1, 7.0f, 1.0f, 0.0f);
        YK_EMIT(0, 7.0f, 1.0f, 0.0f);
        YK_EMIT(0, 7.0f, 1.0f, 0.0f);
        YK_EMIT(YK_GAUGE_GLYPH_DOT, 5.0f, 1.0f, 0.0f);
        YK_EMIT(0, 5.0f, 0.8f, 2.0f);       /* 小号两位：scale 0.8、y += 2 */
        YK_EMIT(0, 7.0f, 0.8f, 2.0f);
    } else if (absv >= 1000) {
        int v = absv;
        YK_EMIT(v / 1000, 7.0f, 1.0f, 0.0f); v %= 1000;
        YK_EMIT(v / 100,  7.0f, 1.0f, 0.0f);
        YK_EMIT(YK_GAUGE_GLYPH_DOT, 5.0f, 1.0f, 0.0f);
        YK_EMIT(v / 10 % 10, 5.0f, 0.8f, 2.0f);
        YK_EMIT(v % 10,      7.0f, 0.8f, 2.0f);
    } else {
        int v = absv;
        YK_EMIT(v / 100, 7.0f, 1.0f, 0.0f);
        YK_EMIT(YK_GAUGE_GLYPH_DOT, 5.0f, 1.0f, 0.0f);
        YK_EMIT(v / 10 % 10, 5.0f, 0.8f, 2.0f);
        YK_EMIT(v % 10,      7.0f, 0.8f, 2.0f);
    }
    /* 收尾的 `'%'`：scale 复位、`y -= 2`（对最后一格而言就是回到 y = 0）*/
    YK_EMIT(YK_GAUGE_GLYPH_PERCENT, 7.0f, 1.0f, 0.0f);
#undef YK_EMIT
    return idx;
}

/* ------------------------------------------------------ §3.11 永点 → PIV */
void yk_piv_reset(int32_t *piv, int32_t *orbs)
{
    if (piv) *piv = 0;
    if (orbs) *orbs = 0;
}

void yk_piv_add(int32_t *piv, int32_t *orbs, int32_t delta)
{
    int32_t d, total;
    if (!orbs) return;
    *orbs += delta;                                  /* +0x3c 与 +0x44 同增（本卡只留总量这一个计数）*/
    if (!piv) return;
    if (delta <= 0) return;                          /* `th08:0x418288`/`:0x41828c jle`：负数**不动** PIV */
    total = *orbs;
    d = delta + (total & 1);                         /* ★ 用加完之后的奇偶（`th08:0x418294`）*/
    *piv += (d / 2) * 10;                            /* `th08:0x4182ab`–`:0x4182bc`：C 整除 2 再 ×10 */
}

void yk_piv_spell_bonus(int32_t *piv, int32_t count)
{
    if (!piv || count <= 0) return;
    *piv += count * 10;                              /* `th08:0x41732c` imul ×10 / `:0x417334` add / `:0x41733d` */
}

int32_t yk_piv_pool_step(int32_t extra, int32_t written)
{
    /* ★★★ 2026-09-27（§BE #137，owner：「永点吃了之后**没有加刻符**，左下角**没显示额外最大得点**」）：
     *   共享 PIV 池（`sdk_core.h` 的「额外最大得点」一节）是**累加**语义（`ce_piv_add` 只往池里加、
     *   不回退），而我们的目标量是**绝对值** `extra = piv − piv_base` ⇒ 每帧只能补「还差多少」。
     *   · `extra <= 0`（刚开局 / 还没吃）⇒ 补 0（池初值才 0 ✓ 用户口径「初值是 0」）。
     *   · `written` 已经追平或**超过** `extra`（别的来源把池推高了、或本卡曾经写多过）⇒ 补 0，
     *     **绝不倒扣**（池是「额外最大得点」的读数，不能因为多卡同装就往回走）。
     *   · 一手：PIV 只会往上（`th08:0x418288` 负增量不碰 PIV；miss / 决死不减）⇒ 单调。*/
    if (extra <= 0) return 0;
    if (written < 0) written = 0;
    if (extra <= written) return 0;
    return extra - written;
}

int32_t yk_orb_score(int32_t stage_point_items_in_game, int32_t point_items_in_game)
{
    int32_t v;
    if (stage_point_items_in_game >= YK_ORB_SCORE_STAGE_GATE) return YK_ORB_SCORE_MAX;  /* 0x4412c8 cmp 0x7d0 */
    v = (point_items_in_game / 2) * 10;              /* 0x4412e3 cdq/sub/sar 1 然后 0x4412e8 imul 10 */
    if (v < YK_ORB_SCORE_MIN) v = YK_ORB_SCORE_MIN;  /* 0x4412ee cmp 0x64 / jge ⇒ 下限 100 */
    return v;
}

/* ---- ★★ 刻符的运动常量（一手见 youkai_core.h 的 §3.11 那一段；owner 第 4 条）------------- */
/* ★★★ 2026-09-22（永点两条来源）：一手 `th08:0x42da54` 的或门 ——
 * 「極端人間 或 極端妖怪」时，EnemyManager 的死亡块会再撒**一颗 state 1** 的刻符
 * （`0x42da2a call 0x406d10`(極端人間) 为真就跳过去 spawn，否则 `0x42da38 call 0x406d70`(極端妖怪)，
 * 两者皆非才跳过）⇒ 那一颗**当帧就满速归航**（= owner 说的「自动吸收」）。*/
int yk_orb_instant_absorb(int gauge)
{
    return (yk_gauge_is_extremely_human(gauge) || yk_gauge_is_extremely_youkai(gauge)) ? 1 : 0;
}

/* ★★★ §3.11c 擦弹时点的三道门（一手逐指令、出处与判据全文在 `youkai_core.h` 同名小节）。
 * 这里只落算式：TH08 `Player::sub_44a930`（`th08:0x44a930`）里三处 `spawn_item(pos, 0xa, 1)`。*/
int yk_shottype_is_solo_human(int shottype)
{
    /* `th08:0x42f230`：`cmp ecx,0x4 / jl`（<4 直接假）→ `and eax,1 / jne`（奇数直接假）。*/
    return (shottype >= 4 && (shottype & 1) == 0) ? 1 : 0;
}

int yk_shottype_is_solo_youkai(int shottype)
{
    /* `th08:0x42f270`：同形，`and eax,1` 后 `je`（偶数直接假）⇒ 只认奇数。*/
    return (shottype >= 4 && (shottype & 1) != 0) ? 1 : 0;
}

int yk_graze_field_gate(int enabled, int boss_active, int gauge, int shottype)
{
    /* ❓ 开关关掉 ⇒ 三条门整段不评（回到「聚焦即撒」；门 A/B 仍在 `yk_graze_orbs_per_bullet` 里）。*/
    if (!enabled) return YK_GRAZE_GATE_OFF;
    /* 一手的分支顺序：机体（`th08:0x44aa9e`）→ boss（`:0x44aaac`）→ 槽极妖（`:0x44aaba`）。*/
    if (yk_shottype_is_solo_human(shottype) && shottype != 0xa) return YK_GRAZE_GATE_SHOTTYPE;
    if (!boss_active) return YK_GRAZE_GATE_BOSS;
    if (!yk_gauge_is_extremely_youkai(gauge)) return YK_GRAZE_GATE_GAUGE;
    return YK_GRAZE_GATE_OK;
}

int yk_graze_orbs_per_bullet(int field_gate, int bullet_path, int spell_active, int shottype)
{
    int n;
    /* ★ 2026-09-27（§BE #136）：**入参里没有「形态 / focus」这一维，也不该有** ——
     * 一手里那道 `IsYoukai()` 闸（`th08:0x44aa6c`）只包住 +100，人/妖在 `th08:0x44aa86` 会合，
     * 三处 `spawn_item` 全在会合点之后（逐指令与社区第二源见 `.h` 同名小节）。*/
    /* 第 1 颗前面那三条场上门没过（且不是「开关关掉」那一档）⇒ 这一颗弹**一颗都不给**
     * （一手：三条 `je 0x44ab14` 都跳到函数尾 ⇒ 三处 `spawn_item` 一处都走不到）。*/
    if (field_gate != YK_GRAZE_GATE_OK && field_gate != YK_GRAZE_GATE_OFF) return 0;
    /* 门 1（`th08:0x44aac9`）：门过到这一步就有 1 颗。*/
    n = 1;
    /* 门 A（`th08:0x44aace` + `:0x44aae0`）：只有**弹**路径**且符卡进行中**才给第 2 颗；
     * 注意 `je 0x44ab14` 是跳到函数尾 ⇒ 没在打符卡时**连第 3 颗也没有**（后三道门是嵌套的）。*/
    if (bullet_path && spell_active) {
        n = 2;
        /* 门 B（`th08:0x44ab00`）：「妖単独」机体停在第 2 颗。*/
        if (!yk_shottype_is_solo_youkai(shottype)) n = YK_GRAZE_ORBS_MAX;
    }
    return n;
}

/* ★★★ 2026-09-22（#25 击破符卡的螺旋）：一手角度式（见 .h 的常量与出处）。
 * `θ = ((t − 10) × 2π / 40) − π/2` —— 只用 `* / + -`（不引 libm，x87 门不受影响）。*/
void yk_spiral_angle(int t, float *out)
{
    const float TWO_PI = 6.28318531f;
    const float PI = 3.14159265f;
    float a;
    if (!out) return;
    a = (((float)t - 10.0f) * TWO_PI / 40.0f) - 1.57079633f;
    /* ★ 一手还有一步**角规约**：`0x417590 call 0x43edb0`（= `θ += off` 后 `while (θ > [0x4b4334]) θ -= 2π`，
     *   `off` 第一段 = 0、第二段 = π）。`[0x4b4334]` = π ⇒ 规约到 (−π, π]。
     *   照抄（`cos/sin` 本来也周期性，但规约后与一手逐位可比、单测也好钉）。*/
    while (a > PI) a -= TWO_PI;
    while (a <= -PI) a += TWO_PI;
    *out = a;
}

int yk_spiral_quota_step(int quota, int *first_out, int *second_out)
{
    int k1, k2, q;
    if (first_out)  *first_out  = 0;
    if (second_out) *second_out = 0;
    if (quota <= 0) return 0;
    k1 = (quota > YK_SPIRAL_FIRST_MAX) ? YK_SPIRAL_FIRST_MAX : quota;   /* `0x4175cc`–`0x4175f3` */
    q  = quota - k1;                                                    /* `0x41762d` */
    k2 = (q > YK_SPIRAL_FIRST_MAX) ? YK_SPIRAL_FIRST_MAX : q;           /* `0x417682`–`0x4176a9` */
    if (first_out)  *first_out  = k1;
    if (second_out) *second_out = YK_SPIRAL_SECOND_N;                   /* 固定 6（`0x4176be`）*/
    q -= k2;                                                            /* `0x4176e1` */
    return q > 0 ? q : 0;                                               /* 夹到 0（一手 `0x4176f3`）*/
}

int yk_spiral_live_peak(int per_batch, int batch_frames, int life_frames)
{
    /* 见 `.h` 那一小节：Little 定律 + 最新一批的余量。只做整数运算（主机单测与卡侧同源）。*/
    int steady, tail;
    if (per_batch <= 0 || batch_frames <= 0 || life_frames <= 0) return 0;
    steady = (per_batch * life_frames + batch_frames - 1) / batch_frames;   /* ceil */
    tail = per_batch;
    return steady + tail;
}

int yk_spiral_first_count(int quota)
{
    if (quota <= 0) return 0;
    return (quota > YK_SPIRAL_FIRST_MAX) ? YK_SPIRAL_FIRST_MAX : quota;
}

void yk_orb_motion_params(int character, float *home_speed, float *pick_r)
{
    /* `[ds:0x18b896c]+0x14`（`th08:0x44088e`）= 当前 `.sht` 头的 `itemCollectSpeed`；
     * 抓取盒半宽 = `[.sht+0x18] / 2`（`th08:0x44d83f`–`0x44d873`）+ 刻符自身半宽 8（16×16 贴图）。
     * 本仓 `local/th08.v1.00d/dat/ply0{0,1,2,3}as.sht` 直读：10/12/10/10 与 24/32/24/24。*/
    const int marisa = (character == 1);
    (void)character;
    if (home_speed) *home_speed = marisa ? 12.0f : 10.0f;     /* ply01as = 12.0，其余三队 = 10.0 */
    /* ★★★ 2026-09-22（P5 / owner #16b「永点吸收速度依旧不够快，得是**全屏吸收**的速度」）：
     * **拾取半径改成 TH18 自己那一档（70）**，不再用 TH08 的 20/24。
     *
     * **一手（两条都记着，别混）**：
     *   · TH08 一手值 = **20 / 24**（抓取盒半宽 `[.sht+0x18]/2` = 12/16 加刻符自身半宽 8；
     *     `th08:0x44d83f`–`0x44d873`、`Player::CalcItemBoxCollision th08:0x44a5a0`）；
     *   · **TH18 引擎自己的值 = 70.0**（`th18:0x45ae7b` / `:0x45ae85` 写 `player+0x47990/0x47994`
     *     = 抓取半径；原生 22 号卡 ITEM_LINE 把它抬到 **110.0**，`th18:0x40c1e9`/`:0x40c1f3`）。
     * ⇒ 这条是 **TH18 适配**（owner 2026-09-22 拍：「要的就是全屏吸收那种速度感」，而 70/110
     *   本来就是**本引擎自己的值**），**不是 TH08 复刻** —— 已登记 `TUNING.md` §2.0y。
     * ⚠ 归航**速度**没动：一手 10/12 px/帧本来就是 TH08 与 TH18 22 号卡同一档。
     * ⚠ 与机体无关（TH08 的 20/24 按机体分，TH18 的 70 是全局默认）⇒ 四个机体同一档。*/
    /* ★★★ 2026-09-22 深夜（owner 口径细化）—— **拾取半径回到 TH08 一手 20/24**。
     *
     * 上一版按 owner 的「全屏吸收那种速度感」把它改成 TH18 的 70（登记为「TH18 适配」）；
     * 拿到 owner 的细化口径（「自动吸收」）之后**逐指令复核**，结论是那条适配**解决的不是真问题**：
     *   · 「自动」在一手里 = **state 1（生成即满速归航）**（`th08:0x42da54` push 1）；
     *   · 「吸收」在一手里**仍然要抓取盒相交**（`th08:0x4409f2`–`0x440a1e`：唯一判据 =
     *     `call 0x44a5a0`，state 3 被显式排除）⇒ **没有任何「免半径自动入账」的状态**；
     *   · 本卡原来的「要靠近才收」之所以慢，真因是**所有**刻符都走了 state 3（上抛约 40 帧），
     *     而一手在極端人/妖那一档是 **state 1**（当帧满速归航）。
     * ⇒ 修法 = 两条来源分开（`yk_orb_instant_absorb` + `yk_on_enemy_killed`），半径回一手 20/24。
     * ⚠ **回退/再适配开关**：若 owner 实跑仍嫌慢，`return` 上面那一行改成 `70.0f`
     *   （= TH18 原生档 `th18:0x45ae7b`；22 号卡 ITEM_LINE = 110）即可，不用动别处。*/
    if (pick_r)     *pick_r     = marisa ? 24.0f : 20.0f;     /* 16+8 / 12+8（TH08 一手）*/
}

/* ★★★ 2026-09-26（owner：「打爆敌人爆的永点，就永点出现的**画面位置分布**而言，似乎大了」；
 *   本轮补一句「**从敌机铺出去的范围太大**」）—— 击破批的**落点算式**照一手重写。
 *
 * **一手（`th08:0x42aff0`–`0x42b048` 逐条，本轮新读）**：那是 `Enemy::FUN_0042adb0`
 * （= 敌人死亡处理，0x42adb0）里撒刻符的循环体：
 * ```
 * 42aff0  fild  [ebp-0x2c]              ; = count（本帧要撒几颗）
 * 42aff3  fadd  st(0),st                ; = 2·count  ★
 * 42affe  call  0x40d390                ; Rng::GetRandomF32InRange(rng, 2·count)   → ∈ [0, 2·count)
 * 42b011  call  0x4143c0                ; Rng::GetRandomF32SignedInRange(rng, π)  → ∈ [−π, π)
 * 42b01d  call  0x4286e0                ; Float3::FromAngleMagnitude(&v, angle, magnitude)
 * 42b036  call  0x410a70                ; Float3::operator+=  → v += &enemy->final_pos(+0x2d88)
 * 42b048  call  0x4400a0                ; ItemManager::SpawnItem(&v, type 7, state 3)
 * ```
 * 参数顺序由 `FromAngleMagnitude` 自己的正文定死（`0x4286e0`：`fld [ebp+8]; fsincos;
 * fmul [ebp+0xc] → x; fmul [ebp+0xc] → y` ⇒ **[ebp+8] = 角度、[ebp+0xc] = 模长**），
 * 与符卡螺旋那一处（`0x417593` `push 128; push angle; call 0x4286e0` —— 常量 128 落在**模长**位）
 * 逐位同形 ⇒ **击破批 = 半径上界 `2·count` 的圆盘内均匀撒**，
 * ⚠ **不是**固定半径 128 的环（本卡旧实现写的就是 128 的环 —— owner 报的「范围太大」正是它：
 *   同样的 12 颗，环半径 128 = 圆盘上界 24 的 **5.3 倍**）。
 * ⚠ 符卡螺旋那一支（`0x417593`）**确实**是固定 128 ⇒ 那边的 `YK_SPIRAL_RADIUS` 不动。
 * ⚠ 圆盘 ≠ 均匀面积分布：`radius` 本身就是均匀随机的（不取 √）⇒ 越靠圆心越密，与一手同构。
 *
 * `r1`/`r2` = 卡侧给的两个**独立均匀**随机数（∈ [0,1)）—— 纯逻辑不做 RNG（回放安全与单测都靠这条）。*/
void yk_orb_burst_polar(int count, float r1, float r2, float *out_angle, float *out_radius)
{
    float c = (float)count;
    if (c < 0.0f) c = 0.0f;
    yk_orb_burst_polar_range(2.0f * c, r1, r2, out_angle, out_radius);
}

/* ⚠ 一式两用：一手里**击破批**的半径上界是 `2·N`（`0x42aff0 fild/fadd st(0),st`），
 *   **被杀者自己那一环**却是常量 `128.0f`（`0x42b15d`）—— 两条都走 `GetRandomF32InRange`
 *   这一个函数（`0x40d390` = `GetRandomF32() * arg`）⇒ 差别只在实参。这里把实参开放出来。*/
void yk_orb_burst_polar_range(float radius_range, float r1, float r2,
                              float *out_angle, float *out_radius)
{
#define YK_PI 3.14159265f
    if (radius_range < 0.0f) radius_range = 0.0f;
    if (out_angle)  *out_angle  = (r1 * 2.0f - 1.0f) * YK_PI;   /* rand_signed(π) ∈ [−π, π) */
    if (out_radius) *out_radius = r2 * radius_range;            /* rand(range) ∈ [0, range) */
#undef YK_PI
}

/* ============ §3.11d 击破批的「门」与「颗数」（一手逐条见 `youkai_core.h` 的同名小节）================
 * 一手的结构（`Enemy::FUN_0042adb0`，这一轮读全；地址逐条在头文件的引用块里）：
 *   · **门**：`n = GetSubEnemyChainCount(this)`（`th08:0x42adbc`），`n == 0` ⇒
 *     `0x42adc4 je 0x42b1e5` **跳过整段撒点**（单只无兄弟的敌机一颗都不给）；
 *   · **颗数**（三条按 shottype 的分支，owner 选双人组那一支）：`n >= 8 ? 26 : 2n+10`；
 *   · **被杀者自己那一环**：`subEnemyCount * 2` 颗（`0x42b150 shl eax,1`），state 1、半径 128。
 * ⚠ `n == 0` 返回 **0**（而不是公式在 n=0 时的 10）—— 一手根本不会用 n=0 调这条公式
 *   （门在前），把门写进这个出口就不会有「忘了判 0」的调用点。*/
unsigned yk_kill_burst_count(unsigned chain_n)
{
    if (!chain_n) return 0u;                                   /* 门：无连锁 ⇒ 不给 */
    return (chain_n >= YK_KILL_BURST_CAP_N)
             ? YK_KILL_BURST_CAP
             : (2u * chain_n + 10u);
}

unsigned yk_kill_ring_count(unsigned chain_n)
{
    return 2u * chain_n;
}

/* ============ ★★★ 2026-09-22（owner #26 · 风味选项）决死窗口全屏敌弹变红：三条纯逻辑 ============
 * 依据、一手与「为什么留在卡内」全在 `youkai_core.h` 的同名小节；这里只有算式。*/
int yk_deathbomb_window(int32_t player_state)
{
    return (player_state == YK_DEATHBOMB_STATE) ? 1 : 0;
}

uint32_t yk_tint_apply(uint32_t cur)
{
    return (cur & 0xff000000u) | (YK_TINT_RED & 0x00ffffffu);
}

uint32_t yk_tint_restore(uint32_t cur, uint32_t orig)
{
    return (cur & 0xff000000u) | (orig & 0x00ffffffu);
}

/* ============ ★★★ 2026-09-22 夜（owner 工单 P-A · 夜点）：两条纯逻辑 ==========================
 * 一手、地址与算式全在 `youkai_core.h` 的同名小节（`th08:0x43af1b` 的 switch / `Item::CollectPoint`）。*/
int32_t yk_night_point_init(int32_t difficulty)
{
    switch (difficulty) {
    case 0:  return 60000;      /* `th08:0x43af49`（0xea60）*/
    case 1:  return 100000;     /* `th08:0x43af58`（0x186a0）*/
    case 2:  return 200000;     /* `th08:0x43af67`（0x30d40）*/
    case 3:  return 300000;     /* `th08:0x43af76`（0x493e0）*/
    default: return 300000;     /* `th08:0x43af85`：Extra（4）同 300000；越界也取这一档 */
    }
}

int32_t yk_blue_point_score(int32_t night, int32_t dist, int32_t is_max_value)
{
    int32_t v;
    if (night <= 0) return 0;
    if (is_max_value) v = night;                       /* 一手：`isMaxValue == 1 ⇒ itemScore = 满值` */
    else if (dist <= 0) v = night;                     /* 收点线以上（`y < pocY`）*/
    else {
        v = night / 2 - dist * (night / 1000);         /* 一手：`pointItemValue/2 − (y − pocY) × (…/1000)` */
        if (v < 0) v = 0;                              /* 一手没有夹零；分数不能为负 ⇒ 我们夹（❓ 自定）*/
    }
    v -= v % 10;                                       /* 一手：`itemScore -= itemScore % 10` */
    return v;
}

/* ============ ★★★ 2026-09-22 夜（P-B 逢魔）：门与击破掉落的纯逻辑 ==============================
 * 一手（`th08:0x406d10`/`:0x406d70` 两个谓词、`th08:0x42da41` 那一支、`th08:0x45187f` 那第二颗）
 * 与 owner 的四条冲突裁决（一律按一手）全在 `youkai_core.h` 的同名小节。*/
/* --------------------------------- ★★★ owner ⑥：我们那几条激光的判据（一手见 `youkai_core.h`）*/
int yk_marisa_laser_row(int set_index, int row_index)
{
    return set_index >= YK_LASER_SET_M0 && set_index <= YK_LASER_SET_M4 && row_index == YK_LASER_ROW;
}

/* --------------------------------- ★★★ 第二十六批：卡侧矩形束的四个纯函数（一手见 `youkai_core.h`）--
 * 全部是整数运算：卡侧只把它们的结果乘一个常数换成像素 / 系数（float 的返回一律避开 ——
 * i386 上返回 float 必然过 x87，本仓的 `dllx87` 闸要求我们自己的目标文件里零 x87）。*/
int yk_m_laser_beam_w_px100(int tier)
{
    static const int32_t w[YK_M_LASER_BEAM_TIERS] = YK_M_LASER_BEAM_W_PX100;
    if (tier < 0) tier = 0;
    if (tier >= YK_M_LASER_BEAM_TIERS) tier = YK_M_LASER_BEAM_TIERS - 1;
    return (int)w[tier];
}

int yk_m_laser_beam_damage(int tier)
{
    static const int32_t d[YK_M_LASER_BEAM_TIERS] = YK_M_LASER_BEAM_DAMAGE;
    if (tier < 0) tier = 0;
    if (tier >= YK_M_LASER_BEAM_TIERS) tier = YK_M_LASER_BEAM_TIERS - 1;
    return (int)d[tier];
}

int yk_m_laser_beam_should_settle(unsigned age)
{
    return (age % (unsigned)YK_M_LASER_BEAM_SETTLE) == 0u;
}

int yk_m_laser_beam_grow_permille(unsigned age)
{
    if (age >= (unsigned)YK_M_LASER_BEAM_GROW_FRAMES) return 1000;
    return (int)(age * 1000u / (unsigned)YK_M_LASER_BEAM_GROW_FRAMES);
}

/* ---------------- ★★★ 第二十七批：**残影多束**的五个纯函数（一手逐指令见 `youkai_core.h`）----
 * 全是整数运算 / 就地写数组：**一律不返回 float**（i386 上 float 返回必过 x87，
 * 本仓 `make dllx87` 要求我们自己的目标文件里零 x87）。*/
int yk_m_laser_beam_rate(int tier)
{
    static const int32_t r[YK_M_LASER_BEAM_TIERS] = YK_M_LASER_BEAM_RATE;
    if (tier < 0) tier = 0;
    if (tier >= YK_M_LASER_BEAM_TIERS) tier = YK_M_LASER_BEAM_TIERS - 1;
    return (int)r[tier];
}

int yk_m_laser_beam_bar_live(float x)
{
    return x >= YK_M_LASER_BEAM_TRAIL_MIN;   /* 一手 tick：`fcomp [0x4b6fe0]`（−900）不成立就跳过 */
}

int yk_m_laser_beam_bar_count(const float *trail, int n, int rate)
{
    int k;
    if (!trail || n <= 0 || rate <= 0) return 0;
    if (rate > n) rate = n;                  /* 轨迹只有 32 格：别越界 */
    for (k = 0; k < rate; ++k)
        if (!yk_m_laser_beam_bar_live(trail[k * 2])) break;   /* 一手 draw：遇哨兵 `break`（前缀连续）*/
    return k;
}

int yk_m_laser_beam_bar_alpha(int base, int k, int rate)
{
    int a34, i, a;
    if (base < 0) base = 0;
    if (base > 255) base = 255;
    if (k <= 0 || rate <= 0) return base;    /* 一手：`i == 0` 那一支**不写** alpha（保持原值）*/
    a34 = base * 3 / 4;                      /* `0x450af2 imul eax,3` + `sar 2`（正数 = 向下取整）*/
    i = 2 * k;                               /* 一手循环是 `i += 2`，偏移 `i*0xc` ≡ `k*0x18` */
    a = a34 - (a34 / 2) * i / rate;          /* `0x450ba3 imul [ebp-8]` → `0x450bb2 idiv rate` */
    if (a < 0) a = 0;
    if (a > 255) a = 255;
    return a;
}

int yk_m_laser_beam_bar_damage(int total, int bars, int k)
{
    int base, rem;
    if (bars <= 0 || k < 0 || k >= bars || total <= 0) return 0;
    base = total / bars;
    rem = total % bars;
    return base + (k < rem ? 1 : 0);         /* Σ_k = total（余数给最新的那几条）*/
}

void yk_m_laser_beam_trail_step(float *trail, int n, float mx, float my)
{
    int i;
    if (!trail || n <= 0) return;
    for (i = n - 1; i > 0; --i) {            /* 一手 `0x450992`–`0x4509fa`：i 从 31 递减到 1 */
        trail[i * 2 + 0] = trail[(i - 1) * 2 + 0];
        trail[i * 2 + 1] = trail[(i - 1) * 2 + 1] - YK_M_LASER_BEAM_TRAIL_DY;  /* `0x4b4338` = 1.0 */
    }
    trail[0] = mx;                           /* 一手 `0x4509fc`：`trail[0] = vm+0x2a4`（上一帧的膛口）*/
    trail[1] = my;
}

/* ★★★ 第三十四批：本帧该常驻几支残影 VM —— 见 `youkai_core.h` 同名的长注。
 * 只要这一帧已经有活点（`live_bars > 0`）就**把整档都起满**（一手是同一支 VM 画 `rate` 次 ⇒ 共享相位）。*/
int yk_m_laser_beam_bars_resident(int live_bars, int rate)
{
    if (live_bars <= 0 || rate <= 0) return 0;
    if (rate > (int)YK_M_LASER_BEAM_BARS_MAX) rate = (int)YK_M_LASER_BEAM_BARS_MAX;
    return rate;
}

/* --------------------------------- ★★★ owner ④：逢魔染色的两个纯函数（一手见 `youkai_core.h`）*/
int yk_fengma_tint_set(int set_index)
{
    return (set_index >= YK_FENGMA_SET_R0 && set_index <= YK_FENGMA_SET_R4)
        || (set_index >= YK_FENGMA_SET_M0 && set_index <= YK_FENGMA_SET_M4);
}

uint32_t yk_fengma_tint_dword(uint32_t color_in)
{
    return (color_in & 0xff000000u) | YK_FENGMA_TINT_RGB;   /* 保 alpha（高 8 位），只换 BGR */
}

/* ------------- ★★★ 第二十八批：束根光斑 / 束端命中特效 / 极妖染红的三个纯函数（一手见 `youkai_core.h`）--
 * 整数 + 比较，**不返回 float**（i386 上 float 返回必过 x87；`make dllx87` 要求零 x87）。*/
uint32_t yk_m_laser_beam_bar_color(int alpha, int extreme_youkai)
{
    uint32_t c;
    if (alpha < 0) alpha = 0;                                       /* 与 `yk_m_laser_beam_bar_alpha` 同一口径 */
    if (alpha > 255) alpha = 255;
    c = ((uint32_t)(unsigned)alpha << 24) | 0x00ffffffu;             /* 基准 = 全白 + 逐条 alpha */
    return extreme_youkai ? yk_fengma_tint_dword(c) : c;            /* 极妖 ⇒ 只换低 24 位（一手 `0x450bf2`）*/
}

int yk_m_laser_beam_fx_due(unsigned age)
{
    return (age % (unsigned)YK_M_LASER_BEAM_FX_EVERY) == 0u;
}

int yk_m_laser_beam_fx_on_beam(float ex, float ey, float bx, float by)
{
    float dx = ex - bx;
    if (dx < 0.0f) dx = -dx;
    if (dx > YK_M_LASER_BEAM_FX_HALF_W) return 0;
    if (ey > by) return 0;                                    /* 在膛口下方 = 束的反方向 */
    if (ey < by - YK_M_LASER_BEAM_BAR_H) return 0;            /* 超出束长（= `YK_LASER_LEN`）*/
    return 1;
}

int yk_fengma(int32_t gauge, int32_t human_effects, int32_t youkai_effects)
{
    /* 一手是 `setle` / `setge` ⇒ 两条都**含等号**；取或（`0x42da2a` / `0x42da38` 两问取或）*/
    return (gauge <= human_effects || gauge >= youkai_effects) ? 1 : 0;
}

int yk_fengma_kill_orbs(int fengma, int is_human_fengma, int by_player_bullet)
{
    /* ★★★ 2026-09-27（第三十七批）：**恒 1 颗** —— 那两个「人间逢魔 + 自机弹击杀再多一颗」的
     * 近似分支**已撤**。理由：一手的那 +1 是**另一条来源**（「極人期间**每一发自机弹碰到敌机**」，
     * `th08:0x451860`–`:0x451892`），它有**自己的颗粒度**（每发命中一颗，不是每次击破一颗）
     * ⇒ 本卡按真颗粒度接在 `on_bullet_hit`（新站点 `ce_bullet_hit`）上了；这里再补一颗就是**重复计数**。
     * 一手那条（死亡路径）：`th08:0x42da2a`/`:0x42da38` 两问取或（極人或極妖）⇒ `:0x42da41 push 1` /
     * `:0x42da54 spawn_item(type 7, ...)` —— **无条件、一颗、与形态无关**。
     * ⚠ 两个入参保留（调用点与单测的接口不变），但**不再影响结果**。*/
    (void)is_human_fengma;
    (void)by_player_bullet;
    return fengma ? 1 : 0;
}

int yk_orb_eat_at_toss(int gauge)
{
    /* 只有**極人**那一档「上抛一结束就当场吃掉」（= owner ④：「人逢魔生成的永点应该没有吸收过程……
     * 生成部分原版还是能看见的」）。極妖那一档（擦弹/螺旋常在那里）**保留可见的吸收过程**
     * （owner 第四十三批：「这个的吸收过程得可见的」）。判据 = 既有的極人门（`th08:0x406d10`）。*/
    return yk_gauge_is_extremely_human(gauge) ? 1 : 0;
}

int yk_hit_orb_use_enemy_pos(float dx, float dy)
{
    /* 见 `.h`：源槽中心离敌机太远 ⇒ 那是长光源（激光/束），它的中心在膛口 ⇒ 落点改用敌机位置。*/
    return (dx * dx + dy * dy > YK_HIT_ORB_FAR_DIST * YK_HIT_ORB_FAR_DIST) ? 1 : 0;
}

int yk_human_fengma_bullet_orbs(int32_t gauge, int is_player_bullet)
{
    /* ★★★ 2026-09-27（第三十七批 · owner「3 可以贴一手」）：一手 `Player::compute_damage_to_enemy`
     * `th08:0x451860`–`:0x451892`（逐指令）：
     *   `451860 mov ecx,0x160f508 / 451865 call 0x406d10`  ⇒ **極端人間**问（假 ⇒ 整段跳过）
     *   `451871 mov eax,[bullet+0x480]` / `451877 movsx ecx,WORD [eax+0x1e]` / `45187d jge` ⇒ ≥0 跳过
     *   `45187f push 3` / `451881 push 7` / `451886 add edx,0x2a4`（弹的位置）/ `451892 call 0x4400a0`
     * ⇒ **每一次「極人 + 自机弹命中敌机」掉 1 颗**（在**弹的位置**，type 7 ⇒ 被强制成 state 3）。
     * 判据里**没有**低速位（`focused`）这一维 —— 一手那一段只看槽的極人档。*/
    if (!is_player_bullet) return 0;                 /* 一手这 +1 只属于**自机弹**命中 */
    if (gauge > YK_GAUGE_HUMAN_EFFECTS) return 0;    /* 極人（含等号，`0x406d10` 是 `setle`）*/
    return 1;
}

/* ============ ★★★ 2026-09-22 夜（#27）：弹字的 alpha 与字形（一手见 youkai_core.h 的同名小节）==*/
int yk_popup_alpha(int32_t dx, int32_t dy)
{
    int32_t d = (int32_t)(uint32_t)((uint32_t)dx * (uint32_t)dx + (uint32_t)dy * (uint32_t)dy);
    if (d > 0x1000) return 0xD0;
    if (d > 0x400)  return (int)(((d - 0x400) * 128) / 0xc00) + 0x50;
    return 0x50;
}

int yk_popup_digits(int32_t value, uint8_t *out, int max)
{
    uint8_t tmp[12];
    int n = 0, i, count;
    if (!out || max <= 0) return 0;                  /* NULL / 容量 0：不写、不崩 */
    if (value <= 0) { out[0] = 0; return 1; }
    while (value > 0 && n < (int)sizeof(tmp)) {
        tmp[n++] = (uint8_t)(value % 10);
        value /= 10;
    }
    count = n;
    if (n > max) n = max;                                 /* 容量不够时**留高位**（弹字宁缺低位）*/
    for (i = 0; i < n; ++i) out[i] = tmp[count - 1 - i];  /* 高位在前 */
    return n;
}

/* ★★★ 2026-09-22 夜（P-D 缩水版）：击破魔使扣槽 = −gauge/12（一手 `th08:0x42b213`–`0x42b223`）。*/
int yk_gauge_fam_kill_delta(int32_t gauge)
{
    return (int)(-gauge / YK_FAM_KILL_GAUGE_DIV);     /* C99 `/` 向零截断 = 一手 `idiv` */
}

/* ★★★ 2026-09-22 夜（#27/#28）：夜点数字的字形（一手 `th08:0x405d11`–`0x405e01`，左对齐 7 px/字）。*/
int yk_night_text_glyphs(int32_t night, yk_gauge_glyph_t *out)
{
    uint8_t dg[YK_NIGHT_TEXT_MAX];
    int n, i;
    if (night < 0) night = 0;
    n = yk_popup_digits(night, dg, (int)YK_NIGHT_TEXT_MAX);
    if (n <= 0) {                                  /* 兜底：至少一位 '0' */
        if (out) { out[0].glyph = 0; out[0].dx = 0.0f; out[0].dy = 0.0f; out[0].scale = 1.0f; }
        return 1;
    }
    /* ★★★ 2026-09-27（owner：「人妖槽的那个 100000 东西要**按照额外最大得点的方法 plot**，
     *   也就是说**初始得 plot 000000**」）：**补前导零到 6 位**。
     *   即：与 110/111 那一族「最大得点」的 6 位数字**同一画法**（`faith_hud.c` 的
     *   `faith_hud_digits6()` + 6 个字形位全画）—— 不是原来那套「前导零不画也不占位」。
     *   ⚠ 这是**owner 口径**（不是一手）：TH08 的 `DrawPercentage` 一族对夜点确实是「前导零不占位」，
     *     但面板上那 6 格与最大得点同构 ⇒ 按 owner 的要求对齐最大得点的画法。
     *   实现 = 在低位数字前面把 `'0'` 补满 `YK_NIGHT_TEXT_DIGITS` 位（低位在右，见下面的 dx 递增）。*/
    while (n < (int)YK_NIGHT_TEXT_DIGITS) {
        for (i = n; i > 0; --i) dg[i] = dg[i - 1];
        dg[0] = 0;                                  /* glyph 0 = '0' */
        ++n;
    }
    for (i = 0; i < n; ++i) {
        if (!out) break;
        out[i].glyph = dg[i];
        out[i].dx = (float)i * 7.0f;               /* 一手：每画出一位 `pos.x += 7.0` */
        out[i].dy = 0.0f;
        out[i].scale = 1.0f;
    }
    return n;
}

/* ★★★ 2026-09-22 夜（批次 3）：人侧撞到使魔 = 圆 vs 圆（一手两半径见 `youkai_core.h` 的同名小节）。*/
int yk_fam_hit_player(float dx, float dy, float hit_r)
{
    float r = hit_r + YK_FAM_HIT_REACH;
    if (r <= 0.0f) return 0;
    return (dx * dx + dy * dy) <= r * r;
}

/* ★★★ 2026-09-22 夜（Time 第三道门）：抓取盒的玩家状态白名单（一手/等位映射见 `youkai_core.h`）。*/
int yk_item_box_open(int32_t player_state)
{
    return (player_state == YK_ITEM_BOX_STATE_ALIVE ||
            player_state == YK_ITEM_BOX_STATE_INVULN ||
            player_state == YK_ITEM_BOX_STATE_BORDER) ? 1 : 0;
}
