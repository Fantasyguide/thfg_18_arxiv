/* sdk_core.h —— 行为 SDK 里不依赖平台的部分：行为注册表、与 JSON 的对账、私有状态槽。
 *
 * 纯 C11（只 <stdint.h>），主机上 make test-host 直接测。桩 / 断点 / 引擎访问在 sdk.h / sdk.c。
 * 设计见 ../SDK.md §3–§4。
 */
#pragma once
#include <stdint.h>

#define CE_SDK_MAX_BEHAVIORS 71        /* = CE_CARD_MAX_NEW：JSON 最多登记这么多，行为不可能更多 */
#define CE_SDK_CARD_ID_MIN   58
#define CE_SDK_CARD_ID_MAX   254
#define CE_STATE_SLOTS       256       /* 同时存在的带状态卡对象上限（一局最多 256 张卡，0x411469）*/
#define CE_STATE_BYTES       512       /* 每张卡私有状态上限（2026-09-08 256 → 512：股市 id 80 要 430+ 字节；静态表 256 × 512 = 128 KB）*/

/* 一张卡的行为：id + 它的虚表（21 槽拷贝）+ 一句给日志看的槽清单 */
typedef struct {
    uint32_t    id;
    const void *vtable;
    const char *slots;                 /* 例如 "ctor,on_tick_2" */
    const void *hooks;                 /* ce_hooks_t*（sdk.h）；事件断点按虚表找到它 */
    uint8_t     is_active;             /* active_recharge != 0；供不依赖 ce_hooks_t 的契约检查 */
} ce_behavior_t;

enum {
    CE_SDK_REG_NONE = 0,
    CE_SDK_REG_ID_RANGE,
    CE_SDK_REG_DUPLICATE,
    CE_SDK_REG_CAPACITY,
};

/* 登记；返回 0 = id 越界、重复或容量已满。第一个错误会一直保留到测试 reset。 */
int ce_sdk_register(const ce_behavior_t *b);
int ce_sdk_registration_error(uint32_t *id, int *reason);
unsigned ce_sdk_behavior_count(void);
const ce_behavior_t *ce_sdk_behavior_at(unsigned i);
/* 按 id 找；NULL = 这张卡没有行为（保持基类虚表） */
const ce_behavior_t *ce_sdk_find(uint32_t id);
void ce_sdk_reset_for_test(void);      /* 只给单测用 */

/* 对账：json_ids[0..n) 是 cards.js 登记的 id。
 *   返回 0 且 *bad_id = 第一个「C 有行为、JSON 没登记」的 id（FAIL）；
 *   返回 1；*unbound = 「JSON 有、C 无行为」的张数（允许，开发期正常）。 */
int ce_sdk_bind_check(const uint32_t *json_ids, unsigned n, uint32_t *bad_id, unsigned *unbound);

enum {
    CE_SDK_CONTRACT_NONE = 0,
    CE_SDK_CONTRACT_ACTIVE_BEHAVIOR_REQUIRED,
    CE_SDK_CONTRACT_ACTIVE_CATEGORY_REQUIRED,
};
int ce_sdk_contract_check(const uint32_t *json_ids, const uint32_t *categories, unsigned n,
                          uint32_t *bad_id, int *reason);

#define CE_STATE_RESERVED    16        /* 块头留给 SDK（主动卡状态机 ce_active_t）；卡的私有状态从这之后起 */

/* 私有状态槽：键 = 卡对象指针。alloc 幂等（同键返回同块，首次清零）；size > CE_STATE_BYTES 或槽满返回 NULL。 */
void *ce_state_alloc(const void *key, unsigned size);
void *ce_state_get(const void *key);   /* 没分配过返回 NULL */
void  ce_state_free(const void *key);  /* 没分配过是空操作 */
unsigned ce_state_in_use(void);
/* 卡侧私有状态：同一把键、同一块，返回块头 CE_STATE_RESERVED 字节之后的地址；size 超出剩余空间返回 NULL。 */
void *ce_state_user(const void *key, unsigned size);

/* ---- 自机弹伤害倍率层（AUDIT §AA）----------------------------------------
 * 「给自机弹加伤害」这件事出现了第三次（K♠ 61、古锭刀 76、衔尾蛇 78），提炼成一层。
 * 每张参与的卡报一个千分比（CE_DMG_UNIT = ×1），这里连乘出**聚合倍率**，并指定其中一张卡
 * （id 最小的那张）当「落笔人」——只有它的 `+0x28` 桩真的写 `bullet+0x9c`，所以一颗弹只被乘一次、
 * 只取整一次，与卡在链表里的顺序无关。上限也由落笔人在 `+0x2c` 里按同一个倍率抬。
 * 纯逻辑，无引擎依赖，主机单测。*/
#define CE_DMG_UNIT 1000

void ce_dmg_reset(void);
/* 登记 / 更新一张卡的贡献。permille = 0 表示这张卡不在场（绑定时登记声明值，`+0x50` 删对象时清 0）。*/
void ce_dmg_set(uint32_t id, int permille);
int  ce_dmg_aggregate(void);      /* 千分比连乘；没有贡献者时 = CE_DMG_UNIT */
int  ce_dmg_applier(void);        /* 落笔那张卡的 id；没有贡献者时 -1 */
int  ce_dmg_scale(int damage);    /* damage × 聚合 / UNIT，单次取整；聚合 = UNIT 时原样返回 */

/* 集卡判定（纯函数）：self 在 set 里，且 set 里其余每张 owned[id] != 0 → 1。owned 是 int[255]。*/
int ce_royal_flush_ready(const int32_t *owned, uint32_t self_id, const uint32_t *set, unsigned n);
/* 神之宣告：消耗一半残机（向上取整）；残机 ≤ 0 返回 0 = 不能发动 */
int ce_judgment_cost(int lives);
/* 黄金小丑：利息 = money 的 percent％ 向下取整；money ≤ 0 返回 0 */
int ce_interest(int money, int percent);
/* 倒吊人：n 个候选里按游戏 RNG 的一个 dword 选下标；n == 0 返回 -1 */
int ce_pick_index(uint32_t rnd, unsigned n);
