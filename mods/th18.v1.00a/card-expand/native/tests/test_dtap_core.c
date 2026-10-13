/* test_dtap_core.c —— 共享「完整双击」状态机的主机单测（make test-host）。
 *
 * 语义来源（逐条）：`cards/dtap_core.h` 的注释 ✓ —— 原始口径 = owner 2026-09-29 对 116 的原话
 *   「算了，改成**双击 Z** 吧，**必须是完整的双击**：**不按 Z，按 Z，放开，再按**」，
 *   窗口 8 帧 = owner 口径「双击窗口参考 **111 灵梦特技穿墙**」（`th11:0x430e50`）✓。
 *
 * ★ 本文件的 ①–⑧ 是 `tests/test_season_core.c` 原有用例的**逐条对应版** ✓（把界面从
 *   `season_dtap_*` 换成共享的 `ce_dtap_*` ✓）—— 116 的薄包装与 111 的新接线共用这份语义 ✓，
 *   所以这里必须把"完整双击"的边界全部钉住 ✓。*/
#include <stdio.h>
#include <string.h>

#include "../cards/dtap_core.h"

static int s_fail, s_pass;
#define CHECK(cond) do { if (cond) ++s_pass; else { ++s_fail; printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } } while (0)

#define W CE_DTAP_WINDOW_DEFAULT

int main(void)
{
    ce_dtap_t d;
    int fires;

    /* ① 完整双击（按下 → 放开 → 再按下）⇒ 恰好 1 次，且触发帧 = 第二次按下那一帧 */
    ce_dtap_init(&d, 0);
    CHECK(ce_dtap_tick(&d, 1) == 0);          /* 按 */
    CHECK(ce_dtap_tick(&d, 0) == 0);          /* 放 */
    CHECK(ce_dtap_tick(&d, 1) == 1);          /* 再按 → 触发 */
    CHECK(ce_dtap_tick(&d, 1) == 0);          /* 还按着：不连发 */
    CHECK(ce_dtap_tick(&d, 0) == 0);          /* 放（重新武装）*/
    CHECK(ce_dtap_tick(&d, 0) == 0);

    /* ② 只按一下，然后一直放开 ⇒ 0 次 */
    ce_dtap_init(&d, W);
    fires = 0;
    fires += ce_dtap_tick(&d, 1);
    { int i; for (i = 0; i < 60; ++i) fires += ce_dtap_tick(&d, 0); }
    CHECK(fires == 0);

    /* ③ 一直按住 Z（远超窗口）⇒ 0 次（按住不算双击）*/
    ce_dtap_init(&d, W);
    fires = 0;
    { int i; for (i = 0; i < 120; ++i) fires += ce_dtap_tick(&d, 1); }
    CHECK(fires == 0);
    CHECK(ce_dtap_tick(&d, 0) == 0);
    fires += ce_dtap_tick(&d, 1);             /* 新的第一次按下 */
    fires += ce_dtap_tick(&d, 0);             /* 放开 */
    fires += ce_dtap_tick(&d, 1);             /* 第二次按下 ⇒ 触发 */
    CHECK(fires == 1);

    /* ④ 「第一下按下后长时间保持、然后才松手再按」：第一次按下已超窗 ⇒ 那次手势作废 */
    ce_dtap_init(&d, W);
    fires = 0;
    fires += ce_dtap_tick(&d, 1);
    { int i; for (i = 0; i < W + 5; ++i) fires += ce_dtap_tick(&d, 1); }
    fires += ce_dtap_tick(&d, 0);
    fires += ce_dtap_tick(&d, 1);             /* 这只是**新的第一次按下** */
    CHECK(fires == 0);
    fires += ce_dtap_tick(&d, 0);
    fires += ce_dtap_tick(&d, 1);             /* 才凑成完整双击 */
    CHECK(fires == 1);

    /* ⑤ 超出窗口：按下 → 放开 → 等过窗口 → 再按 ⇒ 0 次（窗口内没等到第二次）*/
    ce_dtap_init(&d, W);
    fires = 0;
    fires += ce_dtap_tick(&d, 1);
    fires += ce_dtap_tick(&d, 0);
    { int i; for (i = 0; i < W + 2; ++i) fires += ce_dtap_tick(&d, 0); }
    fires += ce_dtap_tick(&d, 1);             /* 超窗后再按 ⇒ 只是新的第一次按下 */
    CHECK(fires == 0);
    fires += ce_dtap_tick(&d, 0);
    fires += ce_dtap_tick(&d, 1);
    CHECK(fires == 1);

    /* ⑥ 窗口内再次按下 ⇒ 触发（边界：刚好用掉窗口最后一帧的那次按下也算）*/
    ce_dtap_init(&d, W);
    fires = 0;
    fires += ce_dtap_tick(&d, 1);             /* 按（win = W）*/
    fires += ce_dtap_tick(&d, 0);             /* 放（win = W−1）*/
    { int i; for (i = 0; i < W - 2; ++i) fires += ce_dtap_tick(&d, 0); }
    CHECK(d.state == 2 && d.win == 1);        /* 还差 1 帧到期 */
    fires += ce_dtap_tick(&d, 1);             /* 窗口内按下 ⇒ 触发 */
    CHECK(fires == 1);

    /* ⑦ 连续三次按下（按-放-按-放-按）⇒ 同一对只算一次：共 1 次（不是 2 次）*/
    ce_dtap_init(&d, W);
    fires = 0;
    fires += ce_dtap_tick(&d, 1);   /* 按 1 */
    fires += ce_dtap_tick(&d, 0);   /* 放 1 */
    fires += ce_dtap_tick(&d, 1);   /* 按 2 → 触发 */
    fires += ce_dtap_tick(&d, 0);   /* 放 2 → 回空闲 */
    fires += ce_dtap_tick(&d, 1);   /* 按 3（新的第一次按下，不触发）*/
    CHECK(fires == 1);
    fires += ce_dtap_tick(&d, 0);   /* 放 */
    fires += ce_dtap_tick(&d, 1);   /* 按 4 ⇒ 第二个双击 */
    CHECK(fires == 2);

    /* ⑧ 空指针 / 初始化安全 + **零值安全**（111 的卡状态靠 memset ⇒ `window == 0` 必须回落默认 ✓）*/
    ce_dtap_init(0, 0);
    CHECK(ce_dtap_tick(0, 1) == 0);
    CHECK(ce_dtap_tick(0, 0) == 0);
    ce_dtap_init(&d, W);
    CHECK(d.state == 0 && d.win == 0 && d.window == W);
    memset(&d, 0, sizeof d);                  /* ★ 模拟卡侧 memset 后的状态 ✓ */
    CHECK(ce_dtap_tick(&d, 1) == 0);          /* 按（回落默认窗口 ✓）*/
    CHECK(ce_dtap_tick(&d, 0) == 0);          /* 放 */
    CHECK(ce_dtap_tick(&d, 1) == 1);          /* 再按 ⇒ 仍能触发 ✓ */
    /* ⑨ 自定义窗口：2 帧窗口比默认更严（同一手势放慢到 3 帧 ⇒ 不触发）*/
    ce_dtap_init(&d, 2);
    CHECK(d.window == 2);
    CHECK(ce_dtap_tick(&d, 1) == 0);
    CHECK(ce_dtap_tick(&d, 0) == 0);
    { int i; for (i = 0; i < 3; ++i) CHECK(ce_dtap_tick(&d, 0) == 0); }
    CHECK(ce_dtap_tick(&d, 1) == 0);          /* 超窗 ⇒ 只是新的第一次按下 ✓ */
    /* ⑩ 越界窗口回落到默认（防御：`window > 255` 或负数）*/
    ce_dtap_init(&d, 999);
    CHECK(d.window == CE_DTAP_WINDOW_DEFAULT);
    ce_dtap_init(&d, -3);
    CHECK(d.window == CE_DTAP_WINDOW_DEFAULT);

    printf("test_dtap_core: %d passed, %d failed\n", s_pass, s_fail);
    return s_fail ? 1 : 0;
}
