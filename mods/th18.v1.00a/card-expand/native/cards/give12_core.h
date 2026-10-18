#ifndef GIVE12_CORE_H
#define GIVE12_CORE_H

/* ★★★ 2026-10-19（owner 实跑 118「挑战・十二合一」：**已经拥有的卡还会再发一张** ✗）：
 *   「该发哪些卡」的**纯逻辑**（可离线单测 ✓）。`give12.c` 只负责调 `ce_give_card()` 与打日志。
 *
 * ## 为什么必须自己判「已持有」✗（一手，不是推测）
 * 发卡走的 `ce_give_card(id, CE_MODE_SHOP, 0)`（`sdk.c`）就是一手的
 * `AbilityManager__allocate_new_card` **`0x411460`**（`engine.h:533`）：
 *   · 它**只有两道守卫**：`num_total >= 0x100` → `-1`、`id > 0x38` → `-1`
 *     （`02-lifecycle.md` §2 的逐指令伪码：`0x411469` / `0x411479`）；
 *   · **没有任何 `owned[]` / 「已在链表里」检查** ✗ ⇒ 对一张**已经持有**的卡再调一次
 *     = `operator new` + 写虚表 + `list_insert` **再建一个同 id 的卡对象** ✓
 *     （`0x412CEC`–`0x412D42` 全段无条件执行）⇒ owner 看到的「又发一张」是**真的** ✗。
 *   ⇒ 引擎自己的判据得我们替它先问：`owned[card_id]`（`0x412D42` 置 1、删卡/收尾清零，
 *     `10-extensibility-limits.md` §8.7；SDK 侧现成的读法 = `sdk.h` 的 `ce_owned()` ✓）。
 *   ★ 本条**不新增站点、不改引擎**：只是在调分配器**之前**按游戏自己的字段挡一下 ✓。
 *
 * ## 语义（纯函数，逐条）
 * 1. 区间 = `[CE_GIVE12_FIRST, CE_GIVE12_LAST]` = **106..117**（十二张系统卡）；
 * 2. `CE_GIVE12_SKIP` = **109（花映塚）本版整体摘除** ⇒ 永远不发（owner 2026-10-18 口径：
 *    「118 别摘掉，是只摘掉里面的 109」）✓ —— 与"已持有"是**两种不同的跳过**，日志要分开写；
 * 3. `owned[id] != 0` ⇒ **不发**（这就是本次的修法）；
 * 4. `owned_len` 之外的下标一律当"未持有"（防越界：引擎数组 `int[255]`，`sdk.h:495` 的
 *    `ce_owned()` 也是 `id < 255` 才读 ✓ —— 这里保持同一个上界口径）。*/

#include <stdint.h>

#define CE_GIVE12_FIRST   106u
#define CE_GIVE12_LAST    117u
#define CE_GIVE12_SKIP    109u
#define CE_GIVE12_OWNED_LEN 255u   /* = `zAbilityManager.owned[]` 的长度（`sdk.h:495` / `10-extensibility-limits.md` §8.7）*/
#define CE_GIVE12_PLAN_MAX  16u    /* 区间最多 12 格，留余量 */

/* 这张卡这次该不该发？返回 1 = 发，0 = 不发（已持有 / 被摘除 / 越界）。
 * `owned == NULL` ⇒ 一律当未持有（调用方拿不到管理器时不至于一张都不发 ✗）。*/
int ce_give12_should_give(const int32_t *owned, unsigned owned_len, uint32_t id);

/* 把这次要发的 id 依次写进 `out`（最多 `cap` 个），返回个数。
 * 顺序 = id 升序（与旧循环一致 ✓）；不发的不进 `out`。*/
unsigned ce_give12_plan(const int32_t *owned, unsigned owned_len, uint32_t *out, unsigned cap);

#endif /* GIVE12_CORE_H */
