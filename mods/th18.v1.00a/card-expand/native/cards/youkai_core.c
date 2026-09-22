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
    e->prev_life = YK_LIFE_UNSAMPLED;                     /* 新使魔：下一帧才允许出 delta */
}

int yk_fam_broken(yk_map_t *m, uint32_t fam_id)
{
    yk_slot_t *e = yk_map_find_fam(m, fam_id);
    if (!e) return 0;
    e->fam_id = 0;
    e->no_fam_frames = 0;                                 /* ★ #7：从「它没了的这一刻」开始计闸的年龄 */
    e->prev_life = YK_LIFE_UNSAMPLED;
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
     *    （删卡 / 松开低速 / 局末 / 断链）+ ★ **`YK_EMIT_BROKEN_TTL` 帧的到期自愈**
     *    （2026-09-22 加，见 `youkai_core.h` 那段：没有它，一个持续活跃、ECL 不再 `etNew` 的发射口
     *    会**永久哑火** —— 那就是 th08 #7 的现象）。 */
    return e->fam_seen ? 1 : 0;
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
