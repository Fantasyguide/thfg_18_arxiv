/* faith.h —— 绿点 / 信仰系统的**引擎侧**共享件：一个 VM 池 + 按档 spawn + 收集判定 + 按槽记账。
 *
 * 用户定案（2026-09-12）：110（风神录）与 111（地灵殿）的绿点「完完全全就是同一个东西」，
 * 只做**一套**系统、数值表不同；两者**共用一个池**（= 一次实现），落槽时按 `slot_mask` 分流到
 * 各自的记账槽 `Δ_110` / `Δ_111`（`faith_core.c`）。这是本模块的**核心接口约定**。
 *
 * 依据：
 *   · `docs/superpowers/specs/2026-09-10-mof-faith-design.md` §2.4（自建 VM 池，运动学照原生道具）
 *   · `docs/superpowers/specs/2026-09-11-system-card-spec-review.md` §7.1 / §7.3 / §7.9
 *     （一套系统 + 分槽记账；大 / 小信仰点一律走自建 VM 池；`type 2` 判据）
 *   · 先例 `native/cards/season.c`（TH16 自建 VM 池 + 收集实体最完整的模板）、`animal.c`（117）
 *
 * ⚠ 自建实体的已知代价（review §7.3，用户已知并接受）：**不入原生道具池** ⇒
 *   原生磁吸 / 回收线 / 114 的批集计数都**看不到它**；所以吸引与收集判定由本模块自己做
 *   （判据照抄原生：读自机自己的 `player+0x4798c` 收集半径 / `+0x47990|94` 吸引半径 /
 *    `+0x47988` 磁速 / `+0x47998` 回收线）。
 *
 * ⚠ 美术占位（❓ 自定，见 `METHOD.md`「铁律:UI 与动画一律先从原代码找」）：
 *   TH18 **没有「绿点」这个 item type**（只有 `1..0x13`），我们也没有绿点贴图 ⇒ 现在借
 *   季节点那张 16×16 原生切图（`CE_ANM_ABILITY_SPRITE_PT_*`）+ `ce_anm_set_color` 染绿。
 *   拿到绿点贴图后只改 `faith.c` 的 `faith_sprite_for()`，接口不动。
 */
#pragma once
#ifndef FAITH_H
#define FAITH_H
#include <stdint.h>
#include "faith_core.h"

/* ---- 池（**静态分区**：大档 64 槽 / 小档 256 槽，各自独立上限 —— 大信仰点一次清场几十颗，
 * 不会把小点的位置吃掉）---- */
#define FAITH_BIG_POOL    64     /* 大信仰点 / 大绿点实体上限 */
#define FAITH_SMALL_POOL  256    /* 小信仰点 / 小绿点实体上限 */
#define FAITH_POOL_TOTAL  (FAITH_BIG_POOL + FAITH_SMALL_POOL)
#define FAITH_LAYER       13     /* 世界层 13 = 相机 3（游玩区），与原生道具同层（照 season.c）*/
#define FAITH_INTANGIBLE  32     /* 出生后的「闪白 / 无敌」期（帧）：不吸、不收（照 season.c）*/

/* 三档（与 §7.1 的表一一对应）*/
typedef enum {
    FAITH_TIER_BIG = 0,          /* 大：击破绿妖精 / 同时掉钱与 P 的敌人（boss 不掉，§7.9）*/
    FAITH_TIER_SMALL = 1,        /* 小：满 P 时 P 点转化 / 110 的大信仰点第二形态 */
    FAITH_TIER_MICRO = 2         /* 微：消弹（每颗 +10）。⚠ 微**不建实体**（§2.2 的 4 个来源我们数不出来）*/
} faith_tier_t;

/* ---- 卡侧回调 ----
 * 收集到一颗绿点 / 信仰点时，本模块回调**所有**在场卡；卡自己按 `slot_mask` 认领，
 * 再把值加进自己的记账槽并 `ce_score_declare(ce_card_id(c), 聚合值)`。
 * ⚠ 卡之间**不读对方状态**：本模块只发「谁掉了多少」，聚合由 SDK 的计分层做（EVENTS-CONTRACT §3）。*/
typedef void (*faith_pickup_fn)(void *user, int slot_mask, int tier, int32_t value);

/* 消弹时回调（本模块**不持有任何槽**：槽状态归卡自己的 ce_state —— 这样 110 与 111
 * 各写各的、互不可见，天然满足「不跨卡耦合」）。*/
typedef void (*faith_cancel_fn)(void *user, int32_t bullets_cancelled);

/* ---- 卡注册（每张用绿点的卡调一次；tick 时再由模块做本帧记账）----*/
#define FAITH_MAX_USERS 8
typedef struct {
    uint32_t       card_id;     /* 声明 score 的那张卡的 id（= ce_card_id(c)）*/
    int            slot_mask;   /* 本卡消费哪些槽：1 << FAITH_SLOT_110 / 1 << FAITH_SLOT_111 */
    faith_pickup_fn on_pickup;  /* 收到一颗点（可 NULL）*/
    faith_cancel_fn on_cancel;  /* 消弹入账（可 NULL）*/
    void          *user;        /* 回传给回调的卡指针 */
} faith_user_t;

/* ---- 生命周期 ---- */
/* ⚠ `faith_module_reset()` **没有调用者，也谁都不该调它**（2026-09-12 审计订正；旧注释写「局末与
 * 删卡都调」是错的）：它内部是 `faith_clear_entities()`（清**整个池**）+ `memset` 掉**整张注册表**
 * ⇒ 110 / 111 共用池时，它会把**另一张卡**同屏还飘着的绿点删掉、把另一张卡的注册也抹掉。
 * 两张卡各自在 `on_run_reset` / `on_delete` 里用 `faith_unregister(自己的 id)` +
 * `faith_clear_entities_mask(自己那一槽)`（= 下面这两支），不经过这里。
 * ⚠ 本函数**保留不删**（可能有外部引用），但本仓里它是**死代码**：建议后续直接删掉。*/
void faith_module_reset(void);
int  faith_register(const faith_user_t *u);   /* 0 = 失败（表满 / 参数非法）；重复注册同 id = 更新 */
void faith_unregister(uint32_t card_id);
void faith_clear_entities(void);    /* 收掉池里**所有** VM（模块复位用；卡侧别用它，见下）*/
/* ★ 收掉池里**只认领了 `slot_mask` 那些槽**的 VM —— **删卡路径必须用这一支**（`on_delete` 与
 * `on_run_reset`）。理由（2026-09-12 审计补）：池是 110/111 **共用**的，`faith_clear_entities()`
 * 会把**另一张卡**同行还飘着的绿点一起删掉（「删掉其中一张、另一张的实体凭空消失」）。
 * 每一颗点出生时就带 `slot_mask`（`faith_spawn` 的第 4 参）⇒ 按它筛就只收自己家的。
 * 传 `~0` = 所有槽（`faith_clear_entities()` 就是这么实现的）。*/
void faith_clear_entities_mask(int slot_mask);

/* ---- 撒点 ---- */
/* 在 (x,y) 撒一颗点；`slot_mask` 决定它落进谁的槽、`tier` 决定 entity 池与半径。
 * 池满返回 0（丢弃：本模块**不做待撒队列** —— 大信仰点的量级一次十几颗，不会爆）。
 * 运动学照原生道具（见 faith.c 头注）。*/
int faith_spawn(float x, float y, faith_tier_t tier, int slot_mask, int32_t value);

/* 大信仰点（110 的击破通道；§7.9 的判据由卡侧给，本函数只负责撒）。*/
int faith_spawn_big(float x, float y, int32_t value_110);

/* ---- 每帧 ---- */
/* 推进池（飘落 / 吸引 / 收集 / 出界）+ 收集回调。卡在 `on_tick_2` 里调一次。
 * 返回本帧被收走的颗数（诊断 / 测试用）。*/
int faith_pool_tick(void);

/* 消弹计数入账（**直接吃 `ce_cancel_radius()` 的返回值**，§5.5 订正 3 ——
 * 不要用 `ce_cancel_all_bullets()`，它是 void 数不出来）。
 * 已注册的卡各自从自己的槽拿：110 侧 = 微信仰点每颗 +10；111 侧 = 小绿点每 100 个 +10
 * （余数走 `faith_slot_t.small_rem`，跨关保留）。**不做跨卡耦合**：各槽自己算自己的。*/
void faith_cancel_account(int32_t bullets_cancelled);

/* ---- 便利：读自机几何 / 判定 ---- */
/* player+0x47998（回收线）。★ 刻意写成 `static inline`：i686 Windows ABI 的 **float 返回值走 x87 st(0)**
 * ⇒ 独立函数会让 `make dllx87` 门变红（本仓不变式「我们的 object 里 0 条 x87 指令」）。*/
static inline float faith_player_poc_height(void)
{
    uint8_t *pl = CE_PLAYER();
    return pl ? *(float *)(pl + CE_PLAYER_POC_HEIGHT) : 0.0f;
}
int   faith_player_above_line(void);     /* 自机在回收线上方？*/
int   faith_enemies_present(void);       /* 场上有敌人？（连击槽空时的衰减档位用）*/

#endif /* FAITH_H */
