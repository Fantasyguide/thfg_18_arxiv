/* sdk.c —— 行为 SDK 的平台侧：绑定断点、门里的核对与对账、事件断点、trace。机制见 ../SDK.md。
 *
 *   BP_ce_card_bind    0x412cec  mov [esi+4],ebx（分配公共尾段：esi = 卡对象，ebx = id）
 *                      id 有登记的行为 → 对象虚表换成 DLL 里那份；放行原指令。此时 ctor 槽还没被调。
 *   BP_ce_item_score   0x446cf6  lea eax,[edi+0xc2c]（collect_money_item：esi = 道具身价，已钳 ≥ 10；
 *                      后面 push esi 给弹窗、mul esi 进计分）。沿卡链表调 on_item_score(&esi)。
 *   BP_ce_bomb_spent   0x4203bc  do_bomb 里 consume_bomb 刚返回。沿卡链表调 on_bomb_spent()。
 *   BP_ce_enemy_drop   0x430510  Enemy__drop_items_and_notify_cards 入口（ecx = zEnemyDrop = enemy+0x623c，不是敌人）。撒附加表之前
 *                                沿卡链表调 on_enemy_drop_pre(counts)，counts = zEnemyDrop+0x04 起 20 个 int32；主掉落 +0x00 已经撒过（S10）。
 *   BP_ce_item_money   0x446d28  inc [MONEY_TOTAL]（紧接 inc [MONEY]）。沿卡链表调 on_item_money(&bonus)，
 *                      bonus > 0 时 MONEY 与 MONEY_TOTAL_COLLECTED 一起 += bonus；原两条 inc 照常放行。
 *   BP_ce_item_frame   0x446ee3  ItemManager tick 0x446ec0 的门卫 CMP（cmp [0x4cf2a4],0，单条 7 字节）：
 *                      每个正常游玩帧、道具池 body 前到达一次（菜单/暂停帧跳走不经过）；handler 不读 CMP 结果。沿卡链表调 on_item_frame()。
 *   BP_ce_item_collect 0x44625f  collect type 分派前一条 mov edx,[edi+0xc78]（单条 6 字节）：本道具副作用都没发生
 *                      （分值未入 SCORE）。type 由窗内指令装载 → **读内存 *(uint32_t*)(edi+0xc78)**，不是读 EDX。沿卡链表调 on_item_collect(card, type, item)。
 *   BP_ce_item_commit  0x44658a  共享收尾起点 movss xmm2,[esi] + push 0x25（两条，无相对寻址）：本道具分值已按 type 计入
 *                      SCORE、公共收尾（清 state、接 commit tail）之前；线/半径各收集路线在此汇聚。沿卡链表调 on_item_commit()。
 *   BP_ce_shot_damage  0x45f2a2  enm_compute_damage_sources 0x45f0f0 计分路径 mov eax,0xee666667 + imul esi（两条，无窗内跳转目标）：
 *                      esi = 本帧本敌伤害（钳后 ≥1）；守卫失败路 0x45f14e jmp 0x45f2de 天然到不了这里。沿卡链表调
 *                      on_shot_damage(dmg, pos)；弹/激光 bomb 伤害与聚焦射击由 handler 门（CE_BOMB_ACTIVE / CE_PLAYER_FOCUSED）。
 *   ce_sdk_on_shop_open          不是断点：shop.c 的 BP_ce_shop_reopen 在 MSG 开店那一帧调它，沿卡链表调 on_shop_open()。
 *   ce_destroy_card              局中删一张卡（倒吊人）：照 recount 0x4080e0 的删卡分支，AUDIT §W。
 *   ce_sdk_setup       门里：基类虚表 21 槽与 engine.h 的常量逐一比对（布局假设的运行时守卫）、九个断点已挂、
 *                      与 cards.js 对账（C 有 JSON 无 → FAIL）、每张行为卡记一行。
 */
#include <string.h>
#include <stdio.h>
#include "card_expand.h"
#include "thcrap_bp.h"
#include "sdk.h"
#include "sdk_actions.h"

static int give_allocate(void *ctx, uint32_t id, uint32_t mode)
{
    return ((ce_fn_alloc_t)CE_FN_ALLOCATE_NEW_CARD)(ctx, id, mode);
}

static void give_mark(void *ctx, uint32_t id, uint32_t notify)
{
    (void)ctx;
    ((ce_fn_mark_t)CE_FN_MARK_OBTAINED)(id, notify);
}

int ce_give_card(uint32_t id, uint32_t mode, uint32_t notify)
{
    void *mgr = CE_ABILITY_MGR();
    return mgr ? ce_give_card_transaction(id, mode, notify, give_allocate, give_mark, mgr) : -1;
}

/* ---- trace ---- */
static int      s_trace;
#define CE_TRACE_SLOTS 30                  /* 21 个虚表槽 + 事件 0x54 / 0x58 / 0x5c / 0x60 / 0x64 / 0x68 / 0x6c / 0x70 / 0x74 */
                                          /* 后四个 = collect-2x 事件（伪偏移，无虚表槽，仅 trace 记首次命中用）*/
static uint8_t  s_seen[CE_SDK_MAX_BEHAVIORS][CE_TRACE_SLOTS];

void ce_sdk_set_trace(int on) { s_trace = on; }

void ce_sdk_trace(uint32_t id, unsigned slot, const char *name)
{
    if (!s_trace) return;
    unsigned k = 0, n = ce_sdk_behavior_count();
    for (; k < n; ++k) if (ce_sdk_behavior_at(k)->id == id) break;
    if (k >= n || slot / 4 >= CE_TRACE_SLOTS) return;
    if (s_seen[k][slot / 4]) return;
    s_seen[k][slot / 4] = 1;
    ce_log("trace: card %u %s (+0x%02x) first hit", id, name, slot);
}

void ce_sdk_register_or_log(const ce_behavior_t *b)
{
    if (!ce_sdk_register(b))
        ce_verdict("FAIL: sdk: cannot register behavior for card %u (duplicate id or > %u behaviors)", b->id, (unsigned)CE_SDK_MAX_BEHAVIORS);
}

/* ---- 主动卡机器（SDK §9；零售模板 = CardTenshi，04-active-cards.md §3–§5；AUDIT O19–O21）----
 * 对象只有 0x54 字节，零售放 +0x54 的 state 放进私有状态。引擎只看 flags bit3/bit5、+0x34 那组充能计时器、+0x48。*/
typedef struct { uint32_t state; } ce_active_t;             /* 0 空闲 / 1 持续 / 2 收尾 */
_Static_assert(sizeof(ce_active_t) <= CE_STATE_RESERVED, "ce_active_t must fit in the reserved head");

static void timer_init(uint8_t *card, unsigned off)          /* 照 Tenshi case：prev = -1，其余 0，control |= 1 */
{
    ce_ztimer_t *t = (ce_ztimer_t *)(card + off);
    t->prev = -1; t->cur = 0; t->cur_f = 0.0f; t->speed_src = 0; t->control |= 1;
}

/* 绑定时：case 56 走的是被动归一化（& ~0x4a | 4），这里按主动 case 的写法重来（& ~0x46 | 8）*/
static void active_init(uint8_t *card, const ce_hooks_t *h)
{
    uint32_t *flags = (uint32_t *)(card + CE_CARD_FLAGS);
    *flags = (*flags & ~(uint32_t)CE_FLAG_ACTIVE_CLEAR) | CE_FLAG_ACTIVE;
    *(uint32_t *)(card + CE_CARD_RECHARGE_TIME) = h->active_recharge;
    timer_init(card, CE_CARD_ELAPSED_TIMER);
    timer_init(card, CE_CARD_RECHARGE_TIMER);
    ce_active_t *a = ce_state_alloc(card, sizeof *a);
    if (a) a->state = 0;
}

/* 零售主动卡的共同门控（0x45c069..0x45c082 / 0x40ea04..0x40ea26）：没在对话（zGui.msg == 0）且场上有敌人
 * （zEnemyManager.enemy_count_real != 0）。充能递减、经过帧递增、C 键分派都受它。*/
static int game_running(void)
{
    uint8_t *gui = CE_GUI(), *em = CE_ENEMY_MGR();
    return gui && *(int32_t *)(gui + CE_GUI_MSG) == 0 && em && *(int32_t *)(em + CE_EM_ENEMY_COUNT) != 0;
}

/* ★ 两个 Timer 函数尾是 `ret 4`：thiscall + 一个没用到的栈参（Tenshi 调用前 push ecx）。少压这 4 字节会把调用方的栈撕掉——第一版就是这么崩的（AUDIT O23）。*/
typedef void (__attribute__((thiscall)) *ce_fn_timer_t)(void *timer, uint32_t unused);

int ce_sdk_c_press(void *self, const ce_hooks_t *h)
{
    uint8_t *card = (uint8_t *)self;
    if (!h->active_recharge) return 0;
    ce_active_t *a = ce_state_alloc(card, sizeof *a);
    ce_ztimer_t *rc = (ce_ztimer_t *)(card + CE_CARD_RECHARGE_TIMER);
    if (!a || a->state != 0 || rc->cur > 0) return 0;      /* Tenshi 0x40ebf9：state == 0 && +0x38 <= 0 */
    ce_ztimer_t *el = (ce_ztimer_t *)(card + CE_CARD_ELAPSED_TIMER);
    el->prev = -1; el->cur = 0; el->cur_f = 0.0f;           /* 经过帧清零 */
    uint8_t *mgr = CE_ABILITY_MGR();
    float mult = mgr ? *(float *)(mgr + CE_MGR_RECHARGE_MULT) : 1.0f;
    float dur = (float)h->active_recharge * mult;
    rc->cur = (int32_t)dur; rc->cur_f = dur; rc->prev = rc->cur - 1;
    *(uint32_t *)(card + CE_CARD_FLAGS) |= CE_FLAG_FIRING;
    int sustain = h->on_activate ? h->on_activate((ce_card_t *)card) : 0;
    if (sustain == CE_ACTIVATE_REFUSED) {                  /* 条件不满足：把刚装填的充能退回（仍是「已充满」）、不进释放态 */
        rc->prev = -1; rc->cur = 0; rc->cur_f = 0.0f;
        *(uint32_t *)(card + CE_CARD_FLAGS) &= ~(uint32_t)CE_FLAG_FIRING;
        return 0;
    }
    a->state = sustain ? 1 : 2;
    return 0;
}

void ce_sdk_active_tick(void *self, const ce_hooks_t *h)
{
    uint8_t *card = (uint8_t *)self;
    if (!h->active_recharge) return;
    ce_active_t *a = ce_state_alloc(card, sizeof *a);
    if (!a) return;
    ce_ztimer_t *rc = (ce_ztimer_t *)(card + CE_CARD_RECHARGE_TIMER);
    ce_ztimer_t *el = (ce_ztimer_t *)(card + CE_CARD_ELAPSED_TIMER);
    int running = game_running();
    switch (a->state) {
    case 0:
        *(uint32_t *)(card + CE_CARD_FLAGS) &= ~(uint32_t)CE_FLAG_FIRING;
        if (running && rc->cur > 0) ((ce_fn_timer_t)CE_FN_TIMER_DECREMENT)(rc, 0);
        break;
    case 1:
        if (!h->on_active_tick || !h->on_active_tick((ce_card_t *)card, (uint32_t)el->cur)) {
            a->state = 2;
            el->prev = -1; el->cur = 0; el->cur_f = 0.0f;
        }
        break;
    default:
        *(uint32_t *)(card + CE_CARD_FLAGS) &= ~(uint32_t)CE_FLAG_FIRING;
        if (el->cur > 8) a->state = 0;
        break;
    }
    if (running) ((ce_fn_timer_t)CE_FN_TIMER_INCREMENT)(el, 0);
}

void ce_sdk_active_reset(void *self, const ce_hooks_t *h, int clear_recharge)
{
    uint8_t *card = (uint8_t *)self;
    if (!h->active_recharge) return;
    ce_active_t *a = ce_state_alloc(card, sizeof *a);
    if (a) a->state = 0;
    ce_ztimer_t *el = (ce_ztimer_t *)(card + CE_CARD_ELAPSED_TIMER);
    el->prev = -1; el->cur = 0; el->cur_f = 0.0f;
    *(uint32_t *)(card + CE_CARD_FLAGS) &= ~(uint32_t)CE_FLAG_FIRING;
    if (clear_recharge) {                                    /* method_4C（局末）才清充能；__on_load__2（关卡开场）不清 */
        ce_ztimer_t *rc = (ce_ztimer_t *)(card + CE_CARD_RECHARGE_TIMER);
        rc->prev = -1; rc->cur = 0; rc->cur_f = 0.0f;
    }
}

/* ---- 断点 ---- */
int __cdecl BP_ce_card_bind(x86_reg_t *regs, void *bp_info)
{
    (void)bp_info;
    const ce_behavior_t *b = ce_sdk_find(regs->ebx);
    if (b && regs->esi) {
        uint8_t *card = (uint8_t *)(uintptr_t)regs->esi;
        *(const void **)card = b->vtable;
        const ce_hooks_t *h = (const ce_hooks_t *)b->hooks;
        int equip = 0;
        if (h && h->active_recharge) active_init(card, h);
        else {
            /* category 1（子机装备）：case 56 给的是被动 flags（& ~0x4a | 4），改成零售装备 case 的写法（& ~0x0a | 0x44），
             * 引擎据 bit6 把它画进 HUD 装备行、计入 num_equipment（0x408de0 / 0x412d6c）。bit0 由紧接着的原指令按 mode 写，不受影响。AUDIT X11 */
            const uint8_t *e = ce_table_entry(regs->ebx);
            if (e && *(const int32_t *)(e + 0x0c) == 1) {
                uint32_t *flags = (uint32_t *)(card + CE_CARD_FLAGS);
                *flags = (*flags & ~(uint32_t)CE_FLAG_EQUIPMENT_CLEAR) | CE_FLAG_EQUIPMENT_SET;
                equip = 1;
            }
        }
        if (s_trace) ce_log("trace: card %u object %08x bound to vtable %p%s", regs->ebx, regs->esi, b->vtable,
                            h && h->active_recharge ? " (active)" : equip ? " (equipment)" : "");
    }
    return BP_EXEC_ORIGINAL;
}

/* 在场的行为卡：沿 mgr 的卡链表走，按虚表认出我们的卡 */
static const ce_hooks_t *hooks_of(const uint8_t *card)
{
    const void *vt = *(const void *const *)(card + CE_CARD_VTABLE);
    unsigned n = ce_sdk_behavior_count();
    for (unsigned k = 0; k < n; ++k) {
        const ce_behavior_t *b = ce_sdk_behavior_at(k);
        if (b->vtable == vt) return (const ce_hooks_t *)b->hooks;
    }
    return NULL;
}

int __cdecl BP_ce_item_score(x86_reg_t *regs, void *bp_info)
{
    (void)bp_info;
    uint8_t *mgr = CE_ABILITY_MGR();
    if (!mgr) return BP_EXEC_ORIGINAL;
    int32_t value = (int32_t)regs->esi;
    unsigned guard = 0;
    for (const uint8_t *node = *(const uint8_t *const *)(mgr + CE_MGR_CARD_LIST_FIRST);
         node && guard < 256;
         node = *(const uint8_t *const *)(node + CE_NODE_NEXT), ++guard) {
        uint8_t *card = *(uint8_t *const *)(node + CE_NODE_CARD);
        if (!card) continue;
        const ce_hooks_t *h = hooks_of(card);
        if (h && h->on_item_score) {
            h->on_item_score((ce_card_t *)card, &value);
            ce_sdk_trace(ce_card_id(card), 0x54, "on_item_score");
        }
    }
    regs->esi = (uint32_t)value;
    return BP_EXEC_ORIGINAL;
}

int __cdecl BP_ce_item_money(x86_reg_t *regs, void *bp_info)
{
    (void)regs; (void)bp_info;
    uint8_t *mgr = CE_ABILITY_MGR();
    if (!mgr) return BP_EXEC_ORIGINAL;
    int32_t bonus = 0;
    unsigned guard = 0;
    for (const uint8_t *node = *(const uint8_t *const *)(mgr + CE_MGR_CARD_LIST_FIRST);
         node && guard < 256;
         node = *(const uint8_t *const *)(node + CE_NODE_NEXT), ++guard) {
        uint8_t *card = *(uint8_t *const *)(node + CE_NODE_CARD);
        if (!card) continue;
        const ce_hooks_t *h = hooks_of(card);
        if (h && h->on_item_money) {
            h->on_item_money((ce_card_t *)card, &bonus);
            ce_sdk_trace(ce_card_id(card), 0x58, "on_item_money");
        }
    }
    if (bonus > 0) { CE_MONEY() += bonus; CE_MONEY_TOTAL() += bonus; }
    return BP_EXEC_ORIGINAL;
}

/* do_bomb 里 consume_bomb 刚返回那一条（0x4203bc）：炸弹已经扣完、HUD 也刷过了。
 * 想改「这一发花掉什么」就得在这里改 —— 同一帧内，玩家看不到闪动。AUDIT §T。 */
int __cdecl BP_ce_bomb_spent(x86_reg_t *regs, void *bp_info)
{
    (void)regs; (void)bp_info;
    uint8_t *mgr = CE_ABILITY_MGR();
    if (!mgr) return BP_EXEC_ORIGINAL;
    unsigned guard = 0;
    for (const uint8_t *node = *(const uint8_t *const *)(mgr + CE_MGR_CARD_LIST_FIRST);
         node && guard < 256;
         node = *(const uint8_t *const *)(node + CE_NODE_NEXT), ++guard) {
        uint8_t *card = *(uint8_t *const *)(node + CE_NODE_CARD);
        if (!card) continue;
        const ce_hooks_t *h = hooks_of(card);
        if (h && h->on_bomb_spent) {
            h->on_bomb_spent((ce_card_t *)card);
            ce_sdk_trace(ce_card_id(card), 0x60, "on_bomb_spent");
        }
    }
    return BP_EXEC_ORIGINAL;
}

/* Enemy__drop_items_and_notify_cards 0x430510 入口（thiscall，ecx = zEnemyDrop = enemy+0x623c）：在引擎撒**附加表**之前
 * 把掉落数表交给卡改。改完引擎自己按各 type 正确的角度/速度去撒 —— 比自己调
 * ItemManager__spawn_items（8 个栈参、零售调用方还留了没填的槽）稳得多。AUDIT §S。 */
int __cdecl BP_ce_enemy_drop(x86_reg_t *regs, void *bp_info)
{
    (void)bp_info;
    uint8_t *mgr = CE_ABILITY_MGR();
    uint8_t *drop = (uint8_t *)regs->ecx;                       /* zEnemyDrop，不是敌人对象（S10）*/
    if (!mgr || !drop) return BP_EXEC_ORIGINAL;
    int32_t *counts = (int32_t *)(drop + CE_ENEMY_DROP_COUNTS);
    unsigned guard = 0;
    for (const uint8_t *node = *(const uint8_t *const *)(mgr + CE_MGR_CARD_LIST_FIRST);
         node && guard < 256;
         node = *(const uint8_t *const *)(node + CE_NODE_NEXT), ++guard) {
        uint8_t *card = *(uint8_t *const *)(node + CE_NODE_CARD);
        if (!card) continue;
        const ce_hooks_t *h = hooks_of(card);
        if (h && h->on_enemy_drop_pre) {
            h->on_enemy_drop_pre((ce_card_t *)card, counts);
            ce_sdk_trace(ce_card_id(card), 0x5c, "on_enemy_drop_pre");
        }
    }
    return BP_EXEC_ORIGINAL;
}

/* ---- collect-2x（辉针城卡）四断点（AUDIT §Z）。四个 handler 都在**原指令前**触发，读的是窗前值；
 * 与 public.h 四槽位（Task 2 追加在 hooks 表尾，缺槽 = NULL，不动的旧卡不受影响）一一对应。
 * 分派只认声明了该槽的卡；mgr 空 → 放行不触发。 ---- */
int __cdecl BP_ce_item_frame(x86_reg_t *regs, void *bp_info)
{
    (void)regs; (void)bp_info;
    uint8_t *mgr = CE_ABILITY_MGR();
    if (!mgr) return BP_EXEC_ORIGINAL;
    unsigned guard = 0;
    for (const uint8_t *node = *(const uint8_t *const *)(mgr + CE_MGR_CARD_LIST_FIRST);
         node && guard < 256;
         node = *(const uint8_t *const *)(node + CE_NODE_NEXT), ++guard) {
        uint8_t *card = *(uint8_t *const *)(node + CE_NODE_CARD);
        if (!card) continue;
        const ce_hooks_t *h = hooks_of(card);
        if (h && h->on_item_frame) {
            h->on_item_frame((ce_card_t *)card);
            ce_sdk_trace(ce_card_id(card), 0x68, "on_item_frame");
        }
    }
    return BP_EXEC_ORIGINAL;
}

int __cdecl BP_ce_item_collect(x86_reg_t *regs, void *bp_info)
{
    (void)bp_info;
    uint8_t *mgr = CE_ABILITY_MGR();
    uint8_t *item = (uint8_t *)(uintptr_t)regs->edi;
    if (!mgr || !item) return BP_EXEC_ORIGINAL;
    uint32_t type = *(const uint32_t *)(item + CE_ITEM_TYPE);   /* ★ 窗前值：EDX 由窗内指令装载，这里读内存 */
    unsigned guard = 0;
    for (const uint8_t *node = *(const uint8_t *const *)(mgr + CE_MGR_CARD_LIST_FIRST);
         node && guard < 256;
         node = *(const uint8_t *const *)(node + CE_NODE_NEXT), ++guard) {
        uint8_t *card = *(uint8_t *const *)(node + CE_NODE_CARD);
        if (!card) continue;
        const ce_hooks_t *h = hooks_of(card);
        if (h && h->on_item_collect) {
            h->on_item_collect((ce_card_t *)card, type, item);
            ce_sdk_trace(ce_card_id(card), 0x6c, "on_item_collect");
        }
    }
    return BP_EXEC_ORIGINAL;
}

int __cdecl BP_ce_item_commit(x86_reg_t *regs, void *bp_info)
{
    (void)regs; (void)bp_info;
    uint8_t *mgr = CE_ABILITY_MGR();
    if (!mgr) return BP_EXEC_ORIGINAL;
    unsigned guard = 0;
    for (const uint8_t *node = *(const uint8_t *const *)(mgr + CE_MGR_CARD_LIST_FIRST);
         node && guard < 256;
         node = *(const uint8_t *const *)(node + CE_NODE_NEXT), ++guard) {
        uint8_t *card = *(uint8_t *const *)(node + CE_NODE_CARD);
        if (!card) continue;
        const ce_hooks_t *h = hooks_of(card);
        if (h && h->on_item_commit) {
            h->on_item_commit((ce_card_t *)card);
            ce_sdk_trace(ce_card_id(card), 0x70, "on_item_commit");
        }
    }
    return BP_EXEC_ORIGINAL;
}

int __cdecl BP_ce_shot_damage(x86_reg_t *regs, void *bp_info)
{
    (void)bp_info;
    uint8_t *mgr = CE_ABILITY_MGR();
    if (!mgr) return BP_EXEC_ORIGINAL;
    /* 门（放行不触发，AUDIT §Z）：esi==0 / param_6≠0 / 聚焦射击 / 炸弹中 ——
     * 掉 P 只挂「非聚焦非炸弹的射击命中」（辉针城 passive 2 原义）。param_6 与 esi 在窗前的
     * 0x45f298/0x45f29e 已判过、守卫失败路跳 0x45f2de 到不了这里，再门一次是纵深防御。*/
    if (!regs->esi || !regs->ebx) return BP_EXEC_ORIGINAL;
    if (*(const int32_t *)(uintptr_t)(regs->ebx + 0x1c) != 0) return BP_EXEC_ORIGINAL;
    if (CE_PLAYER_FOCUSED() || CE_BOMB_ACTIVE()) return BP_EXEC_ORIGINAL;
    /* ★ pos 源（§Z15 定案，探针已删）：0x45f0f0 是 `push ebx; mov ebx,esp` 栈帧别名，[ebx+8] = arg1
     * **参数槽，槽里是指针**。callee 圆形分支 [ebp-0x40]（=arg1）减各源中心判重叠、矩形分支把 arg1
     * 传 0x404440 —— arg1 全 1024 槽扫描期恒定 = 本帧被结算那个身体（敌人）的 {x,y}；调用方
     * 0x42ee75 压 edi（敌人内偏移）、0x43955b/0x43959d 压自家帧缓冲 lea [esp+0x38]（敌人坐标拷贝）。
     * 早前版本把槽地址当坐标读 = 指针位型 ≈ 亚正规 ≈ 世界原点——这里**解引用**才是坐标。*/
    const float *pos = *(const float *const *)(uintptr_t)(regs->ebx + 8);
    if (!pos) return BP_EXEC_ORIGINAL;      /* arg1 可空防御（引擎侧几何判重叠前已解引用过它，实跑必非空）*/
    unsigned guard = 0;
    for (const uint8_t *node = *(const uint8_t *const *)(mgr + CE_MGR_CARD_LIST_FIRST);
         node && guard < 256;
         node = *(const uint8_t *const *)(node + CE_NODE_NEXT), ++guard) {
        uint8_t *card = *(uint8_t *const *)(node + CE_NODE_CARD);
        if (!card) continue;
        const ce_hooks_t *h = hooks_of(card);
        if (h && h->on_shot_damage) {
            h->on_shot_damage((ce_card_t *)card, (int32_t)regs->esi, pos);
            ce_sdk_trace(ce_card_id(card), 0x74, "on_shot_damage");
        }
    }
    return BP_EXEC_ORIGINAL;
}

/* presence 续批谓词（§Z21，前身 ce_item_pool_any_flying = 池空门 {1,3,4}，§Z18 弃用）：
 * 池里有没有批集道具（type∈{1,2}）在**真空飞行**（state == CE_ITEM_STATE_VACUUM）。
 * ★ state 口径一手（AUDIT §Z21/§Z22，state 机写点普查 0x445b0f/0x445e38/0x445f6f/0x446146/
 * 0x446595/0x446647 + 出生 0x446fac/0x4471d3）：type {1,2} 进入 state 3 的唯一入口 = V 门
 * （0x445b9a 系：上线 / 放 B 前 60 帧（0x4cf2b8+0x30/+0x38）/ 对话管理器 [0x4cf2e0]+0x1b0）
 * 翻 0x445f63/0x445f6f —— state 2→3 的顶点翻转（0x445f59 落穿）只发生在 state-5 出生的 9–0xe
 * 分数转换点族（0x4471d3 = CE_FN_SPAWN_ITEM 的 9–0xe 分支），P/钱不经过 state 2/5 → 分发时
 * state==3 即引擎原生的「飞过来的收点道具」谓词（P/钱 handler 0x44699e/0x446c0a 自己也读它给全额）。
 * §Z22 收窄掉 state 4：4 = 70px 环 swoosh（0x446647）= 普通游玩磁吸，任何时刻都发生、不属全屏
 * 收点 → 不续批（run-9：普通游玩环收把批撑过 5 → 误弹灰 NO BONUS）。TH14 原生 0x43898e 也只在
 * state-3 在飞体上重载批计时（§Z21）。state 1（抛物/坠落）不算：落地残余不挡批（TH14 原义；
 * 池空门的 {1,3,4} 把抛物/休止也算「在飞」→ 躲弹幕挂批 + 急切切批三症状，§Z18）。吸收帧自身由
 * 收集断点续期（c2x_collect_batch），两源合盖从起飞到落地的全程。早停。AUDIT §Z。*/
int ce_item_pool_any_attracting(void)
{
    uint8_t *mgr = CE_ITEM_MANAGER();
    if (!mgr) return 0;
    uint8_t *slot = mgr + CE_ITEM_POOL_OFF;
    for (unsigned i = 0; i < CE_ITEM_SLOTS; ++i, slot += CE_ITEM_STRIDE) {
        uint32_t type = *(const uint32_t *)(slot + CE_ITEM_TYPE);
        if (!(type == 1 || type == 2)) continue;
        uint32_t state = *(const uint32_t *)(slot + CE_ITEM_STATE);
        if (state == CE_ITEM_STATE_VACUUM) return 1;
    }
    return 0;
}

/* GameThread 看到 MSG 的开店位那一帧（shop.c BP_ce_shop_reopen 的 flag_set 分支）：商店对象这一帧稍后才 new，
 * 这里改 MONEY 店里显示的就是改后的值。重开的第二家（同一断点的另一分支）不调。AUDIT §W。 */
void ce_sdk_on_shop_open(void)
{
    uint8_t *mgr = CE_ABILITY_MGR();
    if (!mgr) return;
    unsigned guard = 0;
    for (const uint8_t *node = *(const uint8_t *const *)(mgr + CE_MGR_CARD_LIST_FIRST);
         node && guard < 256;
         node = *(const uint8_t *const *)(node + CE_NODE_NEXT), ++guard) {
        uint8_t *card = *(uint8_t *const *)(node + CE_NODE_CARD);
        if (!card) continue;
        const ce_hooks_t *h = hooks_of(card);
        if (h && h->on_shop_open) {
            h->on_shop_open((ce_card_t *)card);
            ce_sdk_trace(ce_card_id(card), 0x64, "on_shop_open");
        }
    }
}

/* ---- 局中删卡（AUDIT §W）----
 * 零售只在两处删链表里的卡：局末 recount 0x4080e0（逐张：摘链 → vtable+0x50(1) → num_total−1，最后整体重算三计数并重选主动卡）
 * 和 reset_cards 0x407da0（全清）。商店空白卡就是在店里调后者，所以「店里删卡、子机等关店广播再重建」有零售先例。
 * 这里做 recount 那一支的单张版：类别计数按 flags 就地减（recount 是删完重数，结果一样），HUD 用引擎自己的
 * create_all_card_lists_for_hud(mgr, 0) 重画（分配尾段 mode != 1 时调的就是它；它先按 mgr+0x58 删旧图标 VM，不留幽灵）。 */
static int card_in_list(const uint8_t *mgr, const uint8_t *victim)
{
    unsigned guard = 0;
    for (const uint8_t *node = *(const uint8_t *const *)(mgr + CE_MGR_CARD_LIST_FIRST);
         node && guard < 256;
         node = *(const uint8_t *const *)(node + CE_NODE_NEXT), ++guard)
        if (*(const uint8_t *const *)(node + CE_NODE_CARD) == victim) return 1;
    return 0;
}

int ce_destroy_card(ce_card_t *victim_)
{
    uint8_t *mgr = CE_ABILITY_MGR();
    uint8_t *victim = (uint8_t *)victim_;
    if (!mgr || !victim) return 0;
    if (*(uint8_t **)(mgr + CE_MGR_SELECTED_ACTIVE) == victim) {
        ce_log("destroy: refused, card %u is the selected active card", ce_card_id(victim));
        return 0;
    }
    if (!card_in_list(mgr, victim)) { ce_log("destroy: refused, card %u not in the list", ce_card_id(victim)); return 0; }
    uint32_t id = ce_card_id(victim), flags = ce_card_flags(victim);
    uint8_t *node = victim + CE_CARD_LIST_NODE;
    uint8_t *next = *(uint8_t **)(node + CE_NODE_NEXT);
    uint8_t *prev = *(uint8_t **)(node + CE_NODE_PREV);
    if (next) *(uint8_t **)(next + CE_NODE_PREV) = prev;          /* 0x408122..0x408125 */
    if (prev) *(uint8_t **)(prev + CE_NODE_NEXT) = next;          /* 0x40812f..0x408132（prev 可以是表头 mgr+0x18，其 +4 正是 +0x1c）*/
    *(uint8_t **)(node + CE_NODE_NEXT) = 0;
    *(uint8_t **)(node + CE_NODE_PREV) = 0;
    int32_t *cnt = (int32_t *)(mgr + ((flags & CE_FLAG_ACTIVE) ? CE_MGR_NUM_ACTIVE
                                    : (flags & CE_FLAG_EQUIPMENT) ? CE_MGR_NUM_EQUIPMENT : CE_MGR_NUM_PASSIVE));
    if (*cnt > 0) --*cnt;
    if (*(int32_t *)(mgr + CE_MGR_NUM_TOTAL) > 0) --*(int32_t *)(mgr + CE_MGR_NUM_TOTAL);
    if (id < 255) ((int32_t *)(mgr + CE_MGR_OWNED))[id] = 0;     /* 像没拿过一样：商店能再刷出它 */
    void *vt = *(void **)(victim + CE_CARD_VTABLE);
    ((ce_base_del_t)(*(void **)((uint8_t *)vt + 0x50)))(victim, 1);   /* 0x408147..0x408149 push 1; call [vt+0x50]；我们的桩顺手释放 ce_state */
    ((void (CE_TC *)(void *, int))CE_FN_CREATE_HUD_LISTS)(mgr, 0);
    ce_log("destroy: card %u (flags 0x%x) removed; now total %d = %d active + %d equipment + %d passive", id, flags,
           *(int32_t *)(mgr + CE_MGR_NUM_TOTAL), *(int32_t *)(mgr + CE_MGR_NUM_ACTIVE),
           *(int32_t *)(mgr + CE_MGR_NUM_EQUIPMENT), *(int32_t *)(mgr + CE_MGR_NUM_PASSIVE));
    return 1;
}

ce_card_t *ce_pick_random_card(ce_card_t *exclude)
{
    uint8_t *mgr = CE_ABILITY_MGR();
    if (!mgr) return NULL;
    uint8_t *cands[256];
    unsigned n = 0, guard = 0;
    const uint8_t *selected = *(const uint8_t *const *)(mgr + CE_MGR_SELECTED_ACTIVE);
    for (const uint8_t *node = *(const uint8_t *const *)(mgr + CE_MGR_CARD_LIST_FIRST);
         node && guard < 256;
         node = *(const uint8_t *const *)(node + CE_NODE_NEXT), ++guard) {
        uint8_t *card = *(uint8_t *const *)(node + CE_NODE_CARD);
        if (!card || card == (uint8_t *)exclude || card == selected) continue;
        cands[n++] = card;
    }
    int k = ce_pick_index(ce_rand(), n);
    return k < 0 ? NULL : (ce_card_t *)cands[k];
}

/* ---- 门里的核对 ---- */
static int bp_applied(const uint8_t *p, unsigned len)
{
    if (p[0] != 0xe8) return 0;
    for (unsigned i = 5; i < len; ++i) if (p[i] != 0x90) return 0;
    return 1;
}

int ce_sdk_setup(uint8_t *base, int trace)
{
    s_trace = trace;
    memset(s_seen, 0, sizeof s_seen);
    uint32_t reg_bad = 0;
    int reg_reason = 0;
    if (ce_sdk_registration_error(&reg_bad, &reg_reason)) {
        ce_verdict("FAIL: sdk: behavior registration error for card %u (reason %d)", reg_bad, reg_reason);
        return 0;
    }
    /* 1. 基类虚表：21 槽与我们假设的一致（布局守卫；只核对拷贝时会用到的 4 个透传槽 + 首尾）*/
    const uint32_t *vt = (const uint32_t *)CE_ADDR_BASE_VTABLE;
    struct { unsigned i; uint32_t want; } chk[] = {
        { 0, CE_BASE_SLOT_CTOR }, { 2, CE_BASE_SLOT_C_PRESS }, { 14, CE_BASE_SLOT_METHOD_38 },
        { 15, CE_BASE_SLOT_METHOD_3C }, { 16, CE_BASE_SLOT_METHOD_40 }, { 20, CE_BASE_SLOT_OPDELETE },
    };
    for (unsigned i = 0; i < sizeof chk / sizeof chk[0]; ++i)
        if (vt[chk[i].i] != chk[i].want) {
            ce_verdict("FAIL: sdk: base vtable slot %u = %08x, expected %08x — not the layout we know", chk[i].i, vt[chk[i].i], chk[i].want);
            return 0;
        }
    /* 2. 九个断点已挂。聚合检查：任何一个失败都打印现场字节（判断 thcrap 没下断 vs 下错位置），
     * 全部查完再决定成败——避免第一个失败点挡住后面断点的诊断信息。 */
    struct { const char *name; uint32_t rva; unsigned len; } bps[] = {
        { "ce_card_bind",   CE_BP_CARD_BIND_RVA,   6 },
        { "ce_item_score",  CE_BP_ITEM_SCORE_RVA,  6 },
        { "ce_item_money",  CE_BP_ITEM_MONEY_RVA,  6 },
        { "ce_bomb_spent",  CE_BP_BOMB_SPENT_RVA,  5 },
        { "ce_enemy_drop",  CE_BP_ENEMY_DROP_RVA,  6 },
        { "ce_item_frame",  CE_BP_ITEM_FRAME_RVA,  7 },
        { "ce_item_collect",CE_BP_ITEM_COLLECT_RVA,6 },
        { "ce_item_commit", CE_BP_ITEM_COMMIT_RVA, 6 },
        { "ce_shot_damage", CE_BP_SHOT_DAMAGE_RVA, 7 },
    };
    unsigned n_bp_fail = 0;
    for (unsigned i = 0; i < sizeof bps / sizeof bps[0]; ++i) {
        const uint8_t *p = base + bps[i].rva;
        if (bp_applied(p, bps[i].len)) continue;
        char hex[3 * 16];
        char *q = hex;
        for (unsigned k = 0; k < bps[i].len; ++k) q += sprintf(q, "%02x ", p[k]);
        ce_log("FAIL: sdk: breakpoint %s not applied (%u bytes at +%08x: %s)",
               bps[i].name, bps[i].len, bps[i].rva, hex);
        ++n_bp_fail;
    }
    if (n_bp_fail) { ce_verdict("FAIL: sdk: %u/%u behavior breakpoints not applied", n_bp_fail, (unsigned)(sizeof bps / sizeof bps[0])); return 0; }
    /* 3. 对账 */
    uint32_t ids[CE_CARD_MAX_NEW], categories[CE_CARD_MAX_NEW];
    unsigned n = ce_new_card_count();
    for (unsigned i = 0; i < n; ++i) {
        ids[i] = ce_new_card_id(i);
        const uint8_t *entry = ce_table_entry(ids[i]);
        categories[i] = entry ? *(const uint32_t *)(entry + 0x0c) : 4;
    }
    uint32_t bad = 0; unsigned unbound = 0;
    if (!ce_sdk_bind_check(ids, n, &bad, &unbound)) {
        ce_verdict("FAIL: sdk: card %u has a behavior in the DLL but no entry in cards.js", bad);
        return 0;
    }
    int contract_reason = 0;
    if (!ce_sdk_contract_check(ids, categories, n, &bad, &contract_reason)) {
        ce_verdict("FAIL: sdk: card %u violates JSON/behavior contract (reason %d)", bad, contract_reason);
        return 0;
    }
    for (unsigned k = 0; k < ce_sdk_behavior_count(); ++k) {
        const ce_behavior_t *b = ce_sdk_behavior_at(k);
        ce_log("sdk: %u bound (%s)", b->id, b->slots);
    }
    ce_log("sdk: %u behaviors, %u registered cards without behavior; base vtable @ %08x verified; trace=%d",
           ce_sdk_behavior_count(), unbound, (unsigned)CE_ADDR_BASE_VTABLE, s_trace);
    return 1;
}
