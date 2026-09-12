/* ufo_core.c —— 112「星莲船 / UFO」纯逻辑层。设计唯一真源与出处见 ufo_core.h。 */
#include "ufo_core.h"

/* 浮点比较容差：本层只需要「是不是同一个颜色 / 是不是在半径内」这类粗判，
 * 不引 libm（主机测试不带 -lm，ufo.c 也不链 libm），所以不用 sqrtf/fabsf。 */
#define UFO_EPS 1e-6f

/* ============================================================ 颜色 / 凑组状态机 */

/* 占位期令牌颜色轮转。**顺序与表长都是设计**，理由见 ufo_core.h 的注：
 * 周期 4 的 `红 蓝 绿 彩虹` 会让连续 3 个 serial 永远互不相同 ⇒ 每次召唤都是彩虹 UFO、
 * 红/绿奖励成死代码。本表周期 7（穷举：周期 ≤ 6 无解）⇒ 三条同色规则 + 彩虹 + 不成组全出现。
 * ⚠ 改这张表之前先看 `tests/test_ufo_core.c` 的「轮转表覆盖」那一段断言 —— 它会红。 */
const int32_t k_ufo_token_color_rotation[UFO_TOKEN_COLOR_ROTATION_N] = {
    UFO_COLOR_GREEN, UFO_COLOR_RAINBOW, UFO_COLOR_RAINBOW, UFO_COLOR_BLUE,
    UFO_COLOR_RED,   UFO_COLOR_RED,     UFO_COLOR_RED
};

/* 金令牌 = 把表里轮到「蓝」的那些、每 `UFO_GOLD_CYCLE` 个周期换成金（★❓ 自定，理由见头文件）。
 * ⚠ 金进槽即归一成蓝（`ufo_color_norm`）⇒ 这条**不改变任何行为**，只决定实机上看得见哪种贴图。 */
int32_t ufo_token_color_for_serial_gold(uint32_t serial)
{
    int32_t c = ufo_token_color_for_serial(serial);
    if (c != UFO_COLOR_BLUE) return c;
    if (UFO_GOLD_CYCLE <= 0) return c;                  /* 关掉金（也防 0 当除数）*/
    if ((int32_t)((serial / (uint32_t)UFO_TOKEN_COLOR_ROTATION_N) % (uint32_t)UFO_GOLD_CYCLE)
        != UFO_GOLD_CYCLE - 1)
        return c;
    return UFO_COLOR_GOLD;
}

int ufo_color_valid(int32_t c)
{
    return c == UFO_COLOR_RED || c == UFO_COLOR_BLUE || c == UFO_COLOR_GREEN ||
           c == UFO_COLOR_RAINBOW || c == UFO_COLOR_GOLD;   /* ★ 金也是合法色（用户裁决 ①）*/
}

int ufo_color_is_gold(int32_t c) { return c == UFO_COLOR_GOLD; }

/* 一手 `0x44a909`：`idx = (color == -1) ? 3 : color - 1` ⇒ 红 0 / 蓝 1 / 绿 2 / 彩虹 3。
 * ★ 用户裁决 ①：**金 = 蓝的调色版** ⇒ 金返回 **1（蓝那一格）** —— 「金 UFO 先用蓝 UFO 的整套贴图」
 *   这件事只有这一个出口：资产线 P 图之后改这一格（或给金单开第 5 件）即可。 */
int ufo_color_art_index(int32_t color)
{
    if (color == UFO_COLOR_RAINBOW) return 3;
    if (color == UFO_COLOR_RED) return 0;
    if (color == UFO_COLOR_BLUE || color == UFO_COLOR_GOLD) return 1;   /* ★ 金 → 蓝那一格（等 P 图）*/
    if (color == UFO_COLOR_GREEN) return 2;
    return -1;
}

void ufo_group_reset(ufo_group_t *g)
{
    int i;
    if (!g) return;
    for (i = 0; i < UFO_SLOT_COUNT; ++i) g->slot[i] = UFO_COLOR_NONE;
    g->filled = 0;
    g->last_group = UFO_COLOR_NONE;
    g->last_slot2 = UFO_COLOR_NONE;
}

int ufo_slots_filled(const ufo_group_t *g)
{
    int i, n = 0;
    if (!g) return 0;
    for (i = 0; i < UFO_SLOT_COUNT; ++i)
        if (g->slot[i] != UFO_COLOR_NONE) ++n;
    return n;
}

int ufo_slot_set(ufo_group_t *g, int32_t color)
{
    int i;
    if (!g || !ufo_color_valid(color)) return 0;
    /* 第一个空槽（不是「按 filled 猜」，坏状态也不会写错格） */
    for (i = 0; i < UFO_SLOT_COUNT; ++i) {
        if (g->slot[i] == UFO_COLOR_NONE) {
            g->slot[i] = color;
            g->filled = ufo_slots_filled(g);
            return 1;
        }
    }
    /* 槽满：**不做任何事**（不丢最旧、不替换、不召唤）—— 规则与理由见 ufo_core.h 的凑组注。 */
    g->filled = ufo_slots_filled(g);
    return 0;
}

/* 两两不同的紧致写法：n 固定为 3 ⇒ 3 次比较就够（不用 O(n²)）。 */
static int ufo_all_pairwise_diff(const int32_t *s, int n)
{
    int i, j;
    for (i = 0; i < n; ++i)
        for (j = i + 1; j < n; ++j)
            if (s[i] == s[j]) return 0;
    return 1;
}

int32_t ufo_group_classify(const ufo_group_t *g)
{
    int32_t s[UFO_SLOT_COUNT];
    int i, all_same = 1, rainbow = 0;
    int32_t fixed = UFO_COLOR_NONE;

    if (!g || g->filled != UFO_SLOT_COUNT) return UFO_COLOR_NONE;
    for (i = 0; i < UFO_SLOT_COUNT; ++i) {
        if (!ufo_color_valid(g->slot[i])) return UFO_COLOR_NONE;   /* 坏状态不合成 */
        /* ★ 唯一的桶归并点：`UFO_GOLD_COUNTS_AS_BLUE` = 1 时「金」在这里变成「蓝」
         *   ⇒ 金+蓝+红 = 两同一异（不是三色各一）、金+金+金 = 蓝。见 ufo_core.h 的裁决①注。 */
        s[i] = ufo_color_norm(g->slot[i]);
        if (s[i] == UFO_COLOR_RAINBOW) rainbow = 1;
        else if (fixed == UFO_COLOR_NONE) fixed = s[i];
    }

    /* 规则 A（spec §0 定案 1）：**3 个互不相同** ⇒ 彩虹 UFO。
     * 彩虹令牌在这里算「第 4 种颜色」⇒ 彩虹 + 红 + 蓝 也满足「互不相同」。
     * ⚠ 桶数不足 3（比如 红 + 红 + 彩虹）会**落在下面两条之外** ⇒ 不召唤。 */
    if (ufo_all_pairwise_diff(s, UFO_SLOT_COUNT)) return UFO_COLOR_RAINBOW;

    /* 规则 B：**3 个同色**。彩虹当任意色补位 ⇒ 红 + 红 + 彩虹 = 红 UFO；
     * 彩虹 + 彩虹 + 红 = 红；彩虹 + 彩虹 + 彩虹 = 彩虹。
     * 一手名字表（`th12:0x4b33e0`）把彩虹记成 `−1` 这一格，没有「彩虹不能补色」的说法 ⇒ 取宽松支。 */
    if (rainbow && fixed == UFO_COLOR_NONE) return UFO_COLOR_RAINBOW;
    for (i = 0; i < UFO_SLOT_COUNT; ++i)
        if (s[i] != UFO_COLOR_RAINBOW && s[i] != fixed) all_same = 0;
    if (all_same) return fixed;

    /* 其余（**两同一异** / 含彩虹但不成组）⇒ 这里返回「不成组」；**处置在 `ufo_group_put`**
     * （一手：第三格填上时当场判，两同一异 ⇒ 丢最旧、保留最近两个，不召唤）。 */
    return UFO_COLOR_NONE;
}

int ufo_group_has_match(const ufo_group_t *g)
{
    return ufo_group_classify(g) != UFO_COLOR_NONE;
}

/* 丢掉最旧的一格并把整条左移 —— **一手 KEEP2 的三条指令逐字**（`th12:0x422ef8`–`0x422f12`）：
 *     mov eax,[esi+4] ; mov [esi],eax        ; slots[0] = slots[1]
 *     mov [esi+8],0                          ; slots[2] = 0
 *     mov [esi+0xc],2 / [esi+0x10],4         ; cnt18 = 2（cnt1c = 4 是「没凑成组」标）
 *     mov [esi+4],edi                        ; slots[1] = 新的这一颗（由调用方接着放）
 * ⇒ 结果 = 「保留最近两个」：`{a,b,c}` → `{b}` + 新的一颗（`ufo_slot_set` 会放进第一个空槽）。 */
static void ufo_group_drop_oldest(ufo_group_t *g, int32_t *out_dropped)
{
    int i;
    for (i = 0; i + 1 < UFO_SLOT_COUNT; ++i) g->slot[i] = g->slot[i + 1];
    g->slot[UFO_SLOT_COUNT - 1] = UFO_COLOR_NONE;
    g->filled = ufo_slots_filled(g);
    if (out_dropped) *out_dropped += 1;
}

/* ★ 字面一手的 KEEP2 条件（`0x422ed4`–`0x422ee9` 的**三条**，2026-09-12 订正）：
 *   ① `0x422ed4 cmp ecx,edi` + `0x422ed6 jne`：`slots[0] == color` 且（`0x422ed8 cmp eax,edi`）
 *      `0x422eda jne 0x422ef3` ⇒ `slots[1] != color` ⇒ **也走 KEEP2**（←旧版漏掉的就是这一条）
 *   ② `0x422ee3 cmp eax,edi` + `0x422ee5 je`：`slots[1] == color` ⇒ KEEP2
 *   ③ `0x422ee7 cmp ecx,eax` + `0x422ee9 je`：`slots[0] == slots[1]` ⇒ KEEP2
 *   三条都不成立（= 三色互不相同）⇒ `0x422eeb or eax,-1` = 彩虹。
 * ⇒ **6 种排列里「两同一异」全走 KEEP2**（含首尾同色 `{A,B,A}`）⇒ 我们的实现（全部丢最旧）
 *   与字面一手**没有差异**（AUDIT §BF4 那条「有意偏离」作废，见报告）。
 * ⚠ 比较前先过 `ufo_color_norm()`：一手没有金色，金 ≡ 蓝 ⇒ 这只是把我们这套颜色映射套进一手的比较式。 */
int ufo_group_keep2_literal_th12(const ufo_group_t *g, int32_t color)
{
    if (!g || g->filled != UFO_SLOT_COUNT) return 0;
    if (!ufo_color_valid(color) || !ufo_color_valid(g->slot[0]) || !ufo_color_valid(g->slot[1]))
        return 0;
    return ufo_color_norm(g->slot[0]) == ufo_color_norm(color) ||
           ufo_color_norm(g->slot[1]) == ufo_color_norm(color) ||
           ufo_color_norm(g->slot[0]) == ufo_color_norm(g->slot[1]);
}

int32_t ufo_group_put(ufo_group_t *g, int32_t color, int32_t *out_dropped)
{
    if (out_dropped) *out_dropped = 0;
    if (!g || !ufo_color_valid(color)) return UFO_GROUP_PUT_NONE;

    if (g->filled == UFO_SLOT_COUNT) {
        /* ① 满格且**已经成组**（上一帧被「场上已有 UFO」挡下的那个）⇒ 原样报成组：
         *    **不收、不丢、不替换**（丢掉 = 玩家白攒一个组）。 */
        if (ufo_group_classify(g) != UFO_COLOR_NONE) return UFO_GROUP_PUT_READY;
        /* ④ 防御支：满格却不成组。新规则下第三格填上时当场就丢最旧（③）⇒ 这个状态**不可达**，
         *    留着是为了「外部直接改 struct / 老存档」也不会把这台状态机锁死（与③同一个口径）。 */
        ufo_group_drop_oldest(g, out_dropped);
    }
    if (!ufo_slot_set(g, color)) return UFO_GROUP_PUT_NONE;   /* 放进第一个空槽（槽满才可能失败）*/
    if (g->filled < UFO_SLOT_COUNT) return UFO_GROUP_PUT_HELD;

    /* ② 第三格刚被填上 —— **一手的判定点就是这里**（`0x422ed4` 起：先判三同色，再判 KEEP2，最后彩虹）。*/
    if (ufo_group_classify(g) != UFO_COLOR_NONE) return UFO_GROUP_PUT_READY;

    /* ③ 不成组 ⇒ **两同一异** ⇒ 照一手 KEEP2：**丢掉最旧的一格、保留最近两个**（不召唤）。
     *    ★ 用户 2026-09-12 裁决 ②：这就是「th12 会删掉第一个 token」；改之前这里什么都不做，
     *      要等**下一颗**令牌进来才脱困（而那时槽里已经满着 3 格）⇒ 手感/槽显示都差一颗令牌。 */
    ufo_group_drop_oldest(g, out_dropped);
    /* ★ 用户要求：**丢完要重判合成**。2 格判不出组（理论不可达），留着是为了
     *   「丢完之后的槽状态」与一手逐字一致，也顺手挡住将来 classify 规则变动。 */
    if (ufo_group_classify(g) != UFO_COLOR_NONE) return UFO_GROUP_PUT_READY;
    return UFO_GROUP_PUT_HELD;
}

/* ---- 五边形框的触发条件（用户裁决 ③；纯逻辑，美术待做）---- */
int ufo_pentagon_hint_for_color(const ufo_group_t *g, int32_t color)
{
    ufo_group_t probe;
    if (!g) return 0;
    if (!ufo_color_valid(color)) return 0;
    if (g->filled != UFO_SLOT_COUNT - 1) return 0;      /* 只有「手里正好 2 枚」才谈得上再来一枚 */
    probe = *g;
    if (!ufo_slot_set(&probe, color)) return 0;
    return ufo_group_classify(&probe) != UFO_COLOR_NONE;
}

int ufo_group_pentagon_hint(const ufo_group_t *g)
{
    static const int32_t k_all[5] = { UFO_COLOR_RED, UFO_COLOR_BLUE, UFO_COLOR_GREEN,
                                      UFO_COLOR_RAINBOW, UFO_COLOR_GOLD };
    int i;
    if (!g || g->filled != UFO_SLOT_COUNT - 1) return 0;
    for (i = 0; i < 5; ++i)
        if (ufo_pentagon_hint_for_color(g, k_all[i])) return 1;
    return 0;
}

int32_t ufo_group_peek_color(const ufo_group_t *g)
{
    return ufo_group_classify(g);
}

int32_t ufo_group_consume(ufo_group_t *g)
{
    int32_t c, s2;
    if (!g) return UFO_COLOR_NONE;
    c = ufo_group_classify(g);
    if (c == UFO_COLOR_NONE) return UFO_COLOR_NONE;    /* 没得合：状态不动（调用方据此不召唤）*/
    s2 = g->slot[UFO_SLOT_COUNT - 1];                  /* ★ 一手 `COLLECTED_UFOS[2]`（`0x4b0c54`）*/
    g->last_group = c;
    ufo_group_reset(g);
    g->last_group = c;                                 /* reset 会把 last_group 清掉，放回 */
    g->last_slot2 = s2;                                /* ★ 同理：彩虹 UFO 的掉落表要读它 */
    return c;
}

/* ============================================================== UFO 生命周期 */
void ufo_reset(ufo_ufo_t *u)
{
    if (!u) return;
    u->ufo_id = 0;
    u->frames = 0;
    u->color = UFO_COLOR_NONE;
    u->cnt_p = 0;
    u->cnt_money = 0;
    u->fill_millionths = 0;
    u->absorbed = 0;
    u->skipped_guard = 0;
}

int ufo_spawn(ufo_ufo_t *u, uint32_t enemy_id, int32_t color)
{
    if (!u || !enemy_id) return 0;
    if (ufo_present(u)) return 0;                      /* 单例（一手管理器 = 一个 `zUfo`）*/
    u->ufo_id = enemy_id;
    u->frames = 0;
    u->color = color;
    u->cnt_p = 0;
    u->cnt_money = 0;
    u->fill_millionths = 0;
    u->absorbed = 0;
    u->skipped_guard = 0;
    return 1;
}

int ufo_present(const ufo_ufo_t *u) { return u && u->ufo_id != 0; }
int ufo_can_spawn(const ufo_ufo_t *u) { return u && u->ufo_id == 0; }

int ufo_tick_advance(ufo_ufo_t *u)
{
    if (!u || !u->ufo_id) return 0;
    if (u->frames < UFO_LIFE_MAX) ++u->frames;
    return u->frames >= UFO_LIFE_MAX;                  /* 到上限：一手 720（`zUfo+0x28`）*/
}

void ufo_finish(ufo_ufo_t *u)
{
    if (!u) return;
    /* 击破 / 飞离共用：清 id + 计数 + fill（照 TH12 `Ufo::on_tick` 的结束分支（`UFO:104-107`）的
     * 「清计数」那半；停循环音 / 删光环 VM / 血条淡出由 ufo.c 的收尾做）。 */
    u->ufo_id = 0;
    u->frames = 0;
    u->color = UFO_COLOR_NONE;
    u->cnt_p = 0;
    u->cnt_money = 0;
    u->fill_millionths = 0;
}

int ufo_can_absorb(const ufo_ufo_t *u)
{
    return ufo_present(u) && u->frames > UFO_ABSORB_PAUSE;   /* 一手：存活帧数 > 59 */
}

/* ================================================================ 吸收（伪磁场） */
int ufo_absorb_type_index(uint32_t type)
{
    if (type == UFO_ITEM_TYPE_P) return 0;
    if (type == UFO_ITEM_TYPE_MONEY) return 1;
    return -1;
}

int ufo_absorbable(uint32_t type, int32_t state, uint32_t ufo_id, int32_t item_type_ok)
{
    if (!ufo_id) return 0;
    if (ufo_absorb_type_index(type) < 0) return 0;
    if (!item_type_ok) return 0;                       /* 槽指针没取到（坏槽）*/
    /* 栅栏③：跳过 state 3（真空飞行）/ 4（70 px 环吸）—— 别抢引擎自己的收点 */
    if (state == 3 || state == 4) return 0;
    /* state 0 = 空槽（或已被别人删掉）*/
    if (state == 0) return 0;
    return 1;
}

int ufo_within_capture(float dx, float dy)
{
    return (dx * dx + dy * dy) < UFO_CAPTURE_DIST_SQ;
}

int ufo_within_capture_sq(float d2) { return d2 < UFO_CAPTURE_DIST_SQ; }

/* ⚠ `ufo_drag_speed` / `ufo_fill_increment` 都是 `static inline` 定义在 ufo_core.h 里
 *（i686 Windows ABI 的 float 返回值走 x87 ⇒ 非 inline 的 float 函数会让 `make dllx87` 变红）。
 * 这里**不重复定义**。 */

int ufo_drag_delta(float ix, float iy, float ux, float uy, float *out_dx, float *out_dy)
{
    float dx = ux - ix, dy = uy - iy;
    if (!out_dx || !out_dy) return 0;
    if (!(dx * dx + dy * dy > UFO_EPS)) return 0;      /* 重合 / 非正距离：没有方向 */
    *out_dx = dx;
    *out_dy = dy;
    return 1;
}

void ufo_drag_step(float *px, float *py, float ux, float uy, float speed)
{
    float dx, dy, d2, step;
    if (!px || !py) return;
    if (!(speed > 0.0f)) return;
    dx = ux - *px;
    dy = uy - *py;
    d2 = dx * dx + dy * dy;
    if (!(d2 > UFO_EPS)) return;                       /* 已经压上了 */
    step = speed * speed;                              /* 与 d² 同量纲（免开方）*/
    if (step >= d2) {                                  /* 这一帧能到 ⇒ **压上去，不越过** */
        *px = ux;
        *py = uy;
        return;
    }
    /* 距离 > speed ⇒ 每帧正好走 speed：比例 = speed / d = √(speed²/d²)… 
     * ⚠ 这里需要 1/d 而不是 1/√d² —— d = √d²。用 float 的双精度路径算这一步：
     * `inv` 由 d2 的平方根倒数平方得来（(1/√d²)² = 1/d²）≠ 1/d ⇒ 不能省那一次开方。
     * 本层不链 libm ⇒ 用位技巧 + 牛顿迭代求 1/√d²，再乘 d2 得 √d2 = d，最后算 speed/d。*/
    {
        union { float f; uint32_t u; } v;
        float rinv, d, k;
        int i;
        v.f = d2;
        v.u = 0x5f3759dfu - (v.u >> 1);
        rinv = v.f;
        for (i = 0; i < 3; ++i) rinv = rinv * (1.5f - 0.5f * d2 * rinv * rinv);
        d = d2 * rinv;                                 /* √d² */
        if (!(d > 0.0f)) { *px = ux; *py = uy; return; }
        k = speed / d;                                 /* 比例步长 */
        *px += dx * k;
        *py += dy * k;
    }
}

/* ============================================================== 围栏三件 */
int ufo_target_on_screen(float x, float y)
{
    /* 一手（本次实装复核）：`0x445e38` 的删除判定 —— `|x| >= 200.0f`（`0x4b9340`）或
     * `y > 472.0f`（`0x4b93a4`）⇒ 引擎当帧把该槽 `state = 0` + 删 anm VM（槽**永久**泄漏，见 ufo.c）。 */
    if (x <= -200.0f || x >= 200.0f) return 0;
    if (y > 472.0f) return 0;
    return 1;
}

int ufo_target_clear_of_player(float x, float y, float px, float py)
{
    float dx = x - px, dy = y - py;
    /* 一手：`0x44625f` 的收点检查半径 = `player+0x4798c`，零售默认 **30.0f**（`0x45ae67` 起初始化；
     * `PLAN` §2.1 的默认 `{5,30,70,70}`）。严格 > ⇒ 命中边界不算安全（引擎用的是 `jbe` 收点）。*/
    return (dx * dx + dy * dy) > (30.0f * 30.0f);
}

int ufo_step_allowed(float nx, float ny, float px, float py)
{
    return ufo_target_on_screen(nx, ny) && ufo_target_clear_of_player(nx, ny, px, py);
}

/* ============================================================ 填充率 / 吸满开奖
 * ⚠ 本段**一个浮点都不用**（含 `double`）：i686 Windows ABI 下 `float`/`double` ↔ `int` 的转换
 *   会生成 x87 指令（`fldl`/`fistpll`/`fldcw`），而 `make dllx87` 断言我们 object 里 0 条 x87。
 *   实装时先写 `float fill` + 一次 `(double)` 转换 ⇒ `ufo_absorb_item` 里当场 6 条 x87。
 *   ⇒ 状态存百万分比整数，增量（`ufo_inc_millionths`）与阈值都整数算，比率只在卡侧转一次。 */
int ufo_items_to_full(int32_t stage, int32_t scale_permille)
{
    int32_t inc = ufo_inc_millionths(stage, scale_permille);
    if (inc <= 0) return -1;                         /* 增量 0 ⇒ 吸不满（不是崩）*/
    /* 严格 `> 1.0` ⇒ 还要再来一颗（一手 `UFO:217`「严格 > 1.0 才吐」）*/
    return (int)(UFO_FILL_UNIT / inc) + 1;
}

int ufo_absorb_item(ufo_ufo_t *u, uint32_t type, int32_t stage, int32_t scale_permille)
{
    int idx;
    if (!ufo_present(u)) return 0;
    idx = ufo_absorb_type_index(type);
    if (idx < 0) return 0;
    /* 一手（`th12:0x44aca0`）：颜色 ≠ −1 时 type 1 → `cntA`、type 2 → `cntB`；彩虹时**反过来**。
     * 本层把两个计数按 TH18 的 type 语义命名（`cnt_p` / `cnt_money`），彩虹的「反过来」照抄语义：
     * 彩虹 UFO 吸 P 道具算到 `cnt_money`、吸金钱算到 `cnt_p`。
     * ⚠ 计数在**满着时也照加**（一手 `0x44acc8` 的 `add [esi+0x48],edi` 在第一个满格检查**之前**）*/
    if (u->color == UFO_COLOR_RAINBOW) {
        if (idx == 0) ++u->cnt_money; else ++u->cnt_p;
    } else {
        if (idx == 0) ++u->cnt_p; else ++u->cnt_money;
    }
    ++u->absorbed;
    /* ★★ 2026-09-12 订正（一手 `0x44acd8 fld1; fcom fill; test ah,0x41; jnp 0x44ae98`）：
     * **fill 已经 >= 1.0 ⇒ 整个收工**（不加 fill、不吐奖励）。
     * 旧版在这里无条件累加并 `> UNIT` 就返回 1 ⇒ 「满着期间每颗都开一次奖」，
     * 而 `ufo_spit_reward(红)` 是**命碎片** ⇒ 那是一条可以刷资源的漏洞。 */
    if (u->fill_millionths >= UFO_FILL_UNIT) return 0;
    u->fill_millionths += ufo_inc_millionths(stage, scale_permille);
    if (u->fill_millionths > UFO_FILL_UNIT) {
        u->fill_millionths = UFO_FILL_UNIT;         /* 一手：满了夹回 1.0（`0x44ad74`）*/
        return 1;                                   /* ★ 一次吸满只开这一次奖 */
    }
    return 0;
}

int32_t ufo_spit_reward(int32_t color)
{
    /* ★ 用户裁决 ①：**金与蓝走同一个 case**（金 = 蓝的调色版）⇒ 两者的返回值逐格相同，
     *   单测把「金/蓝对照表」钉成断言（改这里必须先改那条断言）。 */
    switch (color) {
    case UFO_COLOR_RED:   return UFO_REWARD_LIFE_FRAG;   /* 一手：红 → 残机碎片（type 4 语义）*/
    case UFO_COLOR_GREEN: return UFO_REWARD_BOMB_FRAG;   /* 一手：绿 → Bomb 碎（type 6 语义）*/
    case UFO_COLOR_BLUE:                                 /* 一手：switch **无 case 2** ⇒ 什么都不吐 */
    case UFO_COLOR_GOLD:  return UFO_REWARD_NONE;        /* ★ 金照蓝：也不吐 */
    case UFO_COLOR_RAINBOW: return UFO_REWARD_NONE;      /* 一手：彩虹吐令牌（TH18 无对应 type）⇒ 不吐 */
    default: return UFO_REWARD_NONE;
    }
}

int32_t ufo_break_color_bonus(int32_t color)
{
    switch (color) {
    case UFO_COLOR_RED:   return UFO_REWARD_LIFE_FRAG;   /* 一手：红 UFO 击破额外掉道具 4 */
    case UFO_COLOR_GREEN: return UFO_REWARD_BOMB_FRAG;   /* 一手：绿 UFO 击破额外掉道具 5（→ TH18 6）*/
    case UFO_COLOR_GOLD:                                 /* ★ 金照蓝：无额外 */
    default: return UFO_REWARD_NONE;                     /* 一手：蓝无额外；彩虹另给令牌（见下）*/
    }
}

/* ---- ★★ 令牌掉落：相位表（一手 `th12:0x44af00` 的跳表 `0x44b590` + 四段 spawn）----
 * 相位 0/1/2 = TH12 的道具 13/14/15（= 彩虹令牌的红/蓝/绿起始态，脚本 178/179/180
 * ↔ 我们的 `ability.anm` 219/220/221）。
 * ⚠ **只有这四种颜色 + 三色互不相同才召唤**（`0x44b12d inc eax; cmp eax,4; ja` 之后的跳表），
 *   我们把「金」并进蓝（用户裁决 ①）⇒ 金走蓝那一格（相位 1）。 */
int32_t ufo_break_token_state(int32_t ufo_color, int32_t slot2_color)
{
    switch (ufo_color) {
    case UFO_COLOR_RED:   return 0;                      /* `0x44b23a push 0xd` = 道具 13（红起始）*/
    case UFO_COLOR_BLUE:                                 /* `0x44b32a push 0xe` = 道具 14 */
    case UFO_COLOR_GOLD:  return 1;                      /* ★ 金照蓝（用户裁决 ①：金 = 蓝的调色版）*/
    case UFO_COLOR_GREEN: return 2;                      /* `0x44b409 push 0xf` = 道具 15 */
    case UFO_COLOR_RAINBOW:
        /* `0x44b416 mov eax,[0x4b0c54]; sub eax,1; je → 0xd / sub 1 → 0xe / sub 1 → 0xf`
         * ⇒ **按 `COLLECTED_UFOS[2]`（第 3 枚令牌的颜色，1/2/3）选相位**。
         * ⚠ 一手那里 `slot2` 里存的也是颜色值 1/2/3（`0x422ecb mov [esi+0x8],edi`，edi = 1/2/3）
         *   —— 彩虹令牌本身（颜色 −1）**进不了槽**（槽机 `0x422e81 test edi,edi; jle` 挡掉 ≤0）
         *   ⇒ 这里的 `slot2_color` 只可能是 1/2/3，不会是 UFO_COLOR_RAINBOW。 */
        if (slot2_color == UFO_COLOR_RED)   return 0;
        if (slot2_color == UFO_COLOR_BLUE)  return 1;
        if (slot2_color == UFO_COLOR_GREEN) return 2;
        /* 槽里没记到（老存档 / 外部改 struct）：退回红那一态（`ufo_token_rainbow_script` 的
         * 默认也是红），而不是不掉 —— 一手这条路必掉一枚。 */
        return 0;
    default: return UFO_TOK_DROP_NONE;
    }
}

int32_t ufo_spit_token_state(int32_t ufo_color, int32_t slot2_color)
{
    /* 一手 `th12:0x44aca0`：吸满那条 switch（`0x44ad99`–`0x44ae97`）里
     *   红(`eax==1`) → 道具 4（`0x44adf8`）、绿(`eax==3`) → 道具 7（`0x44adcd`）、
     *   蓝(`eax==2`) → **无 case ⇒ 什么都不吐**、彩虹(`eax==-1`) → `0x44ae03` 按 `[0x4b0c54]` 吐令牌。
     * ⇒ 只有**彩虹**在这条路上吐令牌；其余走道具（`ufo_spit_reward`）或什么都不吐。 */
    if (ufo_color != UFO_COLOR_RAINBOW) return UFO_TOK_DROP_NONE;
    return ufo_break_token_state(UFO_COLOR_RAINBOW, slot2_color);
}

/* ---- ★★ 令牌运动（一手逐条见头文件那一块）----
 * ★ 2026-09-12 换池：位置与速度**都在 `ufo_tok_motion_t` 自己身上**（以前位置在敌机字段里、
 *   这两个函数只吃 `float *x, *y` 指针）。行为逐条不变：位移 → 四边反弹（带朝向守卫）→
 *   1200 帧后不反弹 → 出界即撤。 */
void ufo_tok_motion_init_at(ufo_tok_motion_t *m, uint32_t serial, float x, float y)
{
    if (!m) return;
    /* 一手：`and edi,1` 的奇偶选角度。⚠ 一手的计数在**自增之后**判（`0x427411 inc` 在 `0x4274b1 and` 之前）
     * ⇒ 序号 0（= 本关第 1 枚）落**奇** = 3π/4 = 左下 45°。
     * ⚠ 我们的 `serial` 是**本局**序号（不按关复位）⇒ 换关后奇偶性会与原版差一个（🟡 待实跑，一行可改）。 */
    int leftdown = (int)((serial & 1u) == 0u);            /* 序号 0（本关第 1 枚）⇒ 左下 45° */
    m->x = x;
    m->y = y;
    m->vx = leftdown ? -UFO_TOK_VEL_COMP : UFO_TOK_VEL_COMP; /* cos(3π/4) = −√2/2、cos(π/4) = +√2/2 */
    m->vy = UFO_TOK_VEL_COMP;                             /* 两种角度的 sin 都是 +√2/2（都朝下）*/
    m->age = 0;
}

int ufo_tok_motion_step(ufo_tok_motion_t *m)
{
    if (!m) return 1;
    /* ① 位移（一手 `0x4263a1`–`0x4263cb`：pos += vel；GAME_SPEED 标量按 1.0 取）*/
    m->x += m->vx;
    m->y += m->vy;
    if (m->age < UFO_TOK_NO_BOUNCE_FRAMES) ++m->age;
    /* ② 反弹（一手 `0x4263d1 cmp [edi+0x98c],0x4b0; jge` ⇒ 1200 帧那一段跳过反弹）*/
    if (m->age < UFO_TOK_NO_BOUNCE_FRAMES) {
        float bx = UFO_TOK_BOUNCE_X;
        if (m->x <= -bx && m->vx < 0.0f) m->vx = -m->vx;            /* 反射因子一手 = −1.0（`0x4a4168`）*/
        else if (m->x >= bx && m->vx > 0.0f) m->vx = -m->vx;
        if (m->y <= UFO_TOK_BOUNCE_Y_TOP && m->vy < 0.0f) m->vy = -m->vy;
        else if (m->y >= UFO_TOK_BOUNCE_Y_BOTTOM && m->vy > 0.0f) m->vy = -m->vy;
    }
    /* ③ 出界即撤（一手 `0x42650f`–`0x42655b`：四个外框常量，命中就把 state 写 0）
     * ★ 换池之后「撤」这件事由卡侧执行（`ufo_tok_kill()`：删自己的 VM + 清槽）—— 本层只报判据。 */
    if (m->x <= -UFO_TOK_OUT_X || m->x >= UFO_TOK_OUT_X) return 1;
    if (m->y <= UFO_TOK_OUT_Y_TOP || m->y >= UFO_TOK_OUT_Y_BOTTOM) return 1;
    return 0;
}

int32_t ufo_bounty_item_type(const ufo_ufo_t *u)
{
    if (!u) return 0;
    if (u->cnt_p != 0 && u->cnt_money != 0) return UFO_BOUNTY_TYPE_BOTH;
    if (u->cnt_money != 0) return UFO_BOUNTY_TYPE_MONEY;
    if (u->cnt_p != 0) return UFO_BOUNTY_TYPE_P;
    return 0;                                          /* 两个都 0 ⇒ 一件都不生成（一手）*/
}

int64_t ufo_break_bounty_score(const ufo_ufo_t *u)
{
    int64_t s;
    if (!u) return 0;
    s = (int64_t)u->cnt_p * (int64_t)UFO_BOUNTY_SCORE_PER_P +
        (int64_t)u->cnt_money * (int64_t)UFO_BOUNTY_SCORE_PER_MONEY;
    if (s < 0) s = 0;
    return s;
}

/* ==================================================================== §4 HP */
int32_t ufo_clamp_spawn_life(int32_t life)
{
    if (life >= UFO_LIFE_LIMIT) return UFO_LIFE_MAX_ALLOWED;
    if (life < 1) return 1;                            /* 0 / 负数 = 出生即死（危险）⇒ 抬到 1 */
    return life;
}

/* 血条用的千分比（0..1000）。整数除法 ⇒ 不碰浮点（本层零浮点的理由见 `fill_millionths` 的注）；
 * 边界：`max <= 0`（没记到上限）⇒ 0；`cur < 0` ⇒ 0；`cur >= max` ⇒ 1000。 */
int32_t ufo_hp_permille(int32_t cur, int32_t max)
{
    if (max <= 0) return 0;
    if (cur <= 0) return 0;
    if (cur >= max) return 1000;
    return (int32_t)(((int64_t)cur * 1000) / (int64_t)max);
}

/* ============================================================ 死亡事件过滤
 * ★ 2026-09-12 换池：`ufo_token_kill_counts()` **已删** —— 它只服务「令牌自己被子弹打掉也算收」
 *   那条路（= 真敌人载体的产物）。令牌现在是自建 VM，不产生死亡事件（用户拍板的换池，见头文件 §3.2）。 */
int ufo_body_kill_counts(uint32_t kind)
{
    return kind == UFO_EDEATH_KILLED || kind == UFO_EDEATH_BOMB_KILL;
}
