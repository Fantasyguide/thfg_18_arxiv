/* sdk_core.h —— 行为 SDK 里不依赖平台的部分：行为注册表、与 JSON 的对账、私有状态槽。
 *
 * 纯 C11（只 <stdint.h>），主机上 make test-host 直接测。桩 / 断点 / 引擎访问在 sdk.h / sdk_vtable.c / sdk.c。
 * 设计见 ../SDK.md §3–§4。
 */
#pragma once
#include <stdint.h>

#define CE_SDK_MAX_BEHAVIORS 71        /* = CE_CARD_MAX_NEW：JSON 最多登记这么多，行为不可能更多 */
#define CE_STATE_SLOTS       256       /* 同时存在的带状态卡对象上限（一局最多 256 张卡，0x411469）*/
#define CE_STATE_BYTES       256       /* 每张卡私有状态上限 */

/* 一张卡的行为：id + 它的虚表（21 槽拷贝）+ 一句给日志看的槽清单 */
typedef struct {
    uint32_t    id;
    const void *vtable;
    const char *slots;                 /* 例如 "ctor,on_tick_2" */
    const void *hooks;                 /* ce_hooks_t*（sdk.h）；事件断点按虚表找到它 */
} ce_behavior_t;

/* 登记；返回 0 = 满了或 id 重复（两种都是编程错误，调用方记日志） */
int ce_sdk_register(const ce_behavior_t *b);
unsigned ce_sdk_behavior_count(void);
const ce_behavior_t *ce_sdk_behavior_at(unsigned i);
/* 按 id 找；NULL = 这张卡没有行为（保持基类虚表） */
const ce_behavior_t *ce_sdk_find(uint32_t id);
void ce_sdk_reset_for_test(void);      /* 只给单测用 */

/* 对账：json_ids[0..n) 是 cards.js 登记的 id。
 *   返回 0 且 *bad_id = 第一个「C 有行为、JSON 没登记」的 id（FAIL）；
 *   返回 1；*unbound = 「JSON 有、C 无行为」的张数（允许，开发期正常）。 */
int ce_sdk_bind_check(const uint32_t *json_ids, unsigned n, uint32_t *bad_id, unsigned *unbound);

#define CE_STATE_RESERVED    16        /* 块头留给 SDK（主动卡状态机 ce_active_t）；卡的私有状态从这之后起 */

/* 私有状态槽：键 = 卡对象指针。alloc 幂等（同键返回同块，首次清零）；size > CE_STATE_BYTES 或槽满返回 NULL。 */
void *ce_state_alloc(const void *key, unsigned size);
void *ce_state_get(const void *key);   /* 没分配过返回 NULL */
void  ce_state_free(const void *key);  /* 没分配过是空操作 */
unsigned ce_state_in_use(void);
/* 卡侧私有状态：同一把键、同一块，返回块头 CE_STATE_RESERVED 字节之后的地址；size 超出剩余空间返回 NULL。 */
void *ce_state_user(const void *key, unsigned size);

/* 集卡判定（纯函数）：self 在 set 里，且 set 里其余每张 owned[id] != 0 → 1。owned 是 int[255]。*/
int ce_royal_flush_ready(const int32_t *owned, uint32_t self_id, const uint32_t *set, unsigned n);
/* 神之宣告：消耗一半残机（向上取整）；残机 ≤ 0 返回 0 = 不能发动 */
int ce_judgment_cost(int lives);
/* 黄金小丑：利息 = money 的 percent％ 向下取整；money ≤ 0 返回 0 */
int ce_interest(int money, int percent);
/* 倒吊人：n 个候选里按游戏 RNG 的一个 dword 选下标；n == 0 返回 -1 */
int ce_pick_index(uint32_t rnd, unsigned n);
