/* sdk.c —— 行为 SDK 的平台侧：绑定断点、门里的核对与对账、事件断点、trace。机制见 ../SDK.md。
 *
 *   BP_ce_card_bind    0x412cec  mov [esi+4],ebx（分配公共尾段：esi = 卡对象，ebx = id）
 *                      id 有登记的行为 → 对象虚表换成 DLL 里那份；放行原指令。此时 ctor 槽还没被调。
 *   BP_ce_item_score_in 0x446bf9 movss xmm0,[esi+0x47998]（collect_money_item 的**单一汇合点**；
 *                      三条难度支与 `ja` 那条都到这里。此刻 ecx = 引擎钳位后的身价（单位 1/1000 点）、
 *                      esi = PLAYER、edi = ITEM）。沿卡链表**收集声明**，再把 regs->ecx 改成
 *                      `1000 × (G_0 + ΣG_i)` ⇒ **引擎自己那两段高度系数乘在整笔上**（计分层重做，
 *                      见 SCORE-LAYER-REWORK.md；卡侧回调仍叫 on_item_score，语义是「声明自己的 G_i」）。
 *                      无卡声明时一个字都不写 ⇒ 与原生逐字节相同。
 *   BP_ce_item_score   0x446cf6 / BP_ce_item_score_below 0x446caa —— 引擎算完身价的**两条出口**，
 *                      2026-09-13 起是**纯观察者**（不写 regs->esi、不调卡回调），只打 `score-out:` 那行，
 *                      用来在实跑日志里看见引擎的高度系数（满额支 engine == ours，线下支 engine < ours）。
 *   BP_ce_bomb_spent   0x4203bc  do_bomb 里 consume_bomb 刚返回。沿卡链表调 on_bomb_spent()。
 *   BP_ce_enemy_drop   0x430510  Enemy__drop_items_and_notify_cards 入口（ecx = zEnemyDrop = enemy+0x623c，不是敌人）。撒附加表之前
 *                                沿卡链表调 on_enemy_drop_pre(counts)，counts = zEnemyDrop+0x04 起 20 个 int32；主掉落 +0x00 已经撒过（S10）。
 *                                卡循环之后施加**乘法层** ce_drop_apply()（EVENTS-CONTRACT §2）。
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
 *                      同一断点还发 **on_shot_damage_any**（trace 0x88）：非 bomb 帧的**全部**自机弹命中（含聚焦），
 *                      给需要高速/低速两条通道的卡（107）；老事件的策略门不变。
 *   BP_ce_shot_set_index 0x45ebfb 主炮查 set 前的 mov edi,[edi+edx*4+0xe0]；handler 问 on_shot_set_index(card, eax)，
 *                      返回 >=0 时同时改 edx 与 [ebp-8]（弹里也记 beast set），不碰 sht 0..9。
 *   BP_ce_option_shot_gate 0x40a9c3 Player__tick_shooters_for_ability_card 0x40a9c0 的序言后
 *                      `mov edx,[ebp+8]; sub esp,0xc`（6 字节，两条完整指令、无相对寻址）：
 *                      对原生装备子机组 0x0a..0x15 **与 117 咆哮组 0x1e..0x27** 问
 *                      on_option_shot_gate(card, set)，返回 1 ⇒
 *                      retaddr = 0x40aa9e（该函数自己的空路径收尾）本组子机弹不发（117 咆哮用）。
 *                      ★ **2026-09-12 十四修：已开（`CE_OPTION_SHOT_GATE_ENABLE` 1）** —— 用户 bug 5
 *                      「魔理沙的咆哮依旧会发射子机的激光」⇒ 功能按原设计生效。六字节站点已由部署日志
 *                      证过（门回读通过、无 `mitigation: allocator bound restored`），并新增
 *                      `ce_integrity_check()` 每关自检卡对象 ⇒ 若再出现 id 腐烂会当场打 `BAD card`。
 *                      不能选 `je rel32`：cave_fix 只重定位开头的 `e8/e9`，`0f 84` 复制进 cave
 *                      后会按原位移跳飞。
 *   BP_ce_enemy_shot_slot 0x433ffa 敌人 ECL etOn(opcode 0x259) 的「发弹三步」汇聚点：沿卡链表问
 *                      on_enemy_shot_slot(ev)，返回非 0 ⇒ retaddr = 0x43402d 本帧不发射这一槽（108 使魔）。
 *   ce_sdk_on_shop_open          不是断点：shop.c 的 BP_ce_shop_reopen 在 MSG 开店那一帧调它，沿卡链表调 on_shop_open()。
 *   BP_ce_bullet_damage_final 0x45e7ff PlayerBullet__create 0x45e320 的**卡链遍历出口**（edi = bullet）：
 *                      伤害层的**唯一**落笔点 —— 五家（含原生 25/54 的重算）跑完之后才乘我们的倍率
 *                      ⇒ 乘法叠加、与卡链顺序无关。见 BP30-IMPLEMENTATION.md §1。
 *   BP_ce_death_result 0x45c127 Player__on_tick__body case 4 里五家救援者跑完后的**唯一判定**（edi = acc）：
 *                      把「这次死亡最终有没有被取消」写进 sdk_core 的结果闩锁（只读 getter 给卡侧）；
 *                      同一站点也把结果打进日志。见 BP30-IMPLEMENTATION.md §2。
 *   BP_ce_input_synth 0x462966 ReplayManager__on_tick__record_replay 0x462940 里的 `call Input__compute_edges`
 *                      （**在 `0x462961` 的 raw→INPUT_HELD 拷贝之后**、边沿与录制读点 `0x4629f7` 之前）：
 *                      调试件「逐帧合成方向输入」在这里写 INPUT_HELD 的四个方向位 ⇒ **进录像** +
 *                      同帧被引擎算成边沿、被 Player（0x17）读到。默认（不按调试键/回放中/商店/无自机）不写。
 *                      ⚠ 回放播放走另一条 on_tick（0x462a50）⇒ 站点根本不命中；门是双保险。
 *   ce_destroy_card              局中删一张卡（倒吊人）：照 recount 0x4080e0 的删卡分支，AUDIT §W。
 *   ce_sdk_setup       门里：基类虚表 21 槽与 engine.h 的常量逐一比对（布局假设的运行时守卫）、全部断点已挂、
 *                      与 cards.js 对账（C 有 JSON 无 → FAIL）、每张行为卡记一行。
 */
#include <string.h>
#include <stdio.h>
#include "card_expand.h"
#include "thcrap_bp.h"
#include <stdarg.h>
#include <stdio.h>
#include "sdk.h"

uint32_t ce_input_z_frozen;   /* ★ 2026-10-11 owner 定的 Z 快照槽 ✓（长注见 sdk.h）；★ 必须在条件编译之外 ✓ */
#include "sdk_actions.h"
/* ★ 2026-09-13 已拆线：#include "cards/input_synth.h" —— 调试件「逐帧合成方向输入」不再接入
 *   （代码保留在 cards/input_synth.{h,c}；重挂见 DEV-INPUT-SYNTH.md §0）。*/
#if 0
#include "cards/input_synth.h"
#endif

/* ★ 2026-09-12 十三修：`ce_option_shot_gate` 因 3 字节站点被 thcrap 拒绝，门回读失败会触发
 *   `restore_alloc_bound` 降级路径（新 id 禁用），而存档/编成里已有新 id 时 `allocate_new_card`
 *   会返回 -1，后续卡表/HUD 槽可能被写坏。
 *   ★ 2026-09-12 十四修：站点改成 6 字节后门回读已过（部署日志无 `FAIL` / 无 `mitigation`），
 *   且新加的 `ce_integrity_check()` 每关自检卡对象 ⇒ **按用户 bug 5 重新打开**。
 *   （历史：十三修期间为排障临时置 0。） */
#define CE_OPTION_SHOT_GATE_ENABLE 1

static int give_allocate(void *ctx, uint32_t id, uint32_t mode)
{
    return ((ce_fn_alloc_t)CE_FN_ALLOCATE_NEW_CARD)(ctx, id, mode);
}

static void give_mark(void *ctx, uint32_t id, uint32_t notify)
{
    (void)ctx;
    ((ce_fn_mark_t)CE_FN_MARK_OBTAINED)(id, notify);
}

/* ★★ 2026-09-22：左下角那块「当前（选中）主动卡」UI 的刷新入口。
 *
 * 一手（`engine/card/th18/15-selected-card-hud.md` §2/§3）：那块 UI = `abcard.anm` script0 的 VM 树，
 * 显隐与换图**只有一条实现** = `AbilityManager__update_selected_active_card_hud` `0x408c30`
 * （thiscall(mgr)，**无栈参、`ret`**；一手 `0x408cfa`–`0x408cfd`）：`mgr+0x38 == 0` ⇒ 递归清整棵树的
 * `visible`（`vm+0x534` bit1，anm opcode 310，绘制前 `0x481256` 读它）⇒ **一个像素都不画**；
 * 非 0 ⇒ 置位并把卡表的 `sprite_small`（`table_entry+0x30`）写进 script3/script4 两颗子 VM。
 *
 * ⚠ **引擎不会替我们刷**：`set_selected_active_card` `0x408b00` 只在**找到卡**时调它（`0x408b74`），
 *   「找不到」那条路直接返回 0（**不清 HUD**）；分配器尾段只写 `mgr+0x38`（`0x412d5d`）然后调
 *   `create_all_card_lists_for_hud`（那只重画**卡组图标行**，不碰 `mgr+0x3c` 那棵树）。
 * ⇒ 我们这两条改 `mgr+0x38` 的路（买到主动卡 / 删掉最后一张主动卡）**必须自己补这一下**，
 *   否则现象正是 owner 描述的那两条：买了主动卡左下角不出现、删光主动卡那块卡图留在屏幕上。*/
#define CE_FN_UPDATE_SELECTED_HUD 0x408c30
typedef void (__attribute__((thiscall)) *ce_fn_update_selected_hud_t)(void *mgr);

static void hud_card_refresh(void *mgr)
{
    if (mgr) ((ce_fn_update_selected_hud_t)CE_FN_UPDATE_SELECTED_HUD)(mgr);
}

int ce_give_card(uint32_t id, uint32_t mode, uint32_t notify)
{
    void *mgr = CE_ABILITY_MGR();
    if (!mgr) return -1;
    int r = ce_give_card_transaction(id, mode, notify, give_allocate, give_mark, mgr);
    if (r >= 0) {
        /* 分配尾段把**新卡**写进了 `mgr+0x38` 当选中卡（`0x412d5d`，一手：`test al,8` 门内的
         * `mov [edi+0x38],esi`）⇒ 这里补一次 HUD 刷新，让左下角当场出现/换成它。
         * 给的是被动卡时 `mgr+0x38` 没变，这一步是幂等的空转（`0x408c30` 只读不改选中对象）。*/
        hud_card_refresh(mgr);
        ce_log("hud-card: refreshed after giving card %u (selected=%p)", id,
               *(void **)((uint8_t *)mgr + CE_MGR_SELECTED_ACTIVE));
    }
    return r;
}

/* ---- trace ---- */
static int      s_trace;
#define CE_TRACE_SLOTS 40                  /* 21 个虚表槽 + 事件 0x54..0x88（槽号/4 当数组下标 ⇒ 40 覆盖到 0x9c）*/
                                          /* 后四个 = collect-2x 事件（伪偏移，无虚表槽，仅 trace 记首次命中用）*/
                                          /* 空槽（可继续分配）：0x98 / 0x9c —— 0x98 已给「主炮组号同帧争用」用 */
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

/* ---- 主动卡机器（SDK §9；原生模板 = CardTenshi，04-active-cards.md §3–§5；AUDIT O19–O21）----
 * 对象只有 0x54 字节，原生放 +0x54 的 state 放进私有状态。引擎只看 flags bit3/bit5、+0x34 那组充能计时器、+0x48。*/
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

/* 原生主动卡的共同门控（0x45c069..0x45c082 / 0x40ea04..0x40ea26）：没在对话（zGui.msg == 0）且场上有敌人
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
            /* category 1（子机装备）：case 56 给的是被动 flags（& ~0x4a | 4），改成原生装备 case 的写法（& ~0x0a | 0x44），
             * 引擎据 bit6 把它画进 HUD 装备行、计入 num_equipment（0x408de0 / 0x412d6c）。bit0 由紧接着的原指令按 mode 写，不受影响。AUDIT X11 */
            const uint8_t *e = ce_table_entry(regs->ebx);
            if (e && *(const int32_t *)(e + 0x0c) == 1) {
                uint32_t *flags = (uint32_t *)(card + CE_CARD_FLAGS);
                *flags = (*flags & ~(uint32_t)CE_FLAG_EQUIPMENT_CLEAR) | CE_FLAG_EQUIPMENT_SET;
                equip = 1;
            }
        }
        /* 伤害层：绑定 = 这张卡在场，把它声明的初值登记进聚合（`+0x50` 删对象时清 0）。AUDIT §AA */
        if (h && h->bullet_damage_permille) ce_dmg_set(regs->ebx, h->bullet_damage_permille);
        if (s_trace) ce_log("trace: card %u object %08x bound to vtable %p%s", regs->ebx, regs->esi, b->vtable,
                            h && h->active_recharge ? " (active)" : equip ? " (equipment)" : "");
    }
    return BP_EXEC_ORIGINAL;
}

/* ---- 自机弹伤害倍率层（AUDIT §AA；落笔点 2026-09-13 搬到卡链之后）----------------
 * 聚合与「谁声明」在 sdk_core（纯逻辑、主机单测）；这里只有**一个**带引擎的落笔点。
 *
 * ★★ 落笔点 = 断点 `BP_ce_bullet_damage_final`（`th18:0x45e7ff`，卡链遍历**出口**）。
 * 一手：`PlayerBullet__create` `0x45e320` **先**写 `bullet+0x9c = shooter+0x02`（`0x45e392`/`0x45e396`）→
 * `0x45e7dc mov eax,[0x4cf298]`（ABILITY_MGR）/ `0x45e7e4 mov esi,[eax+0x1c]`（卡链头）→
 * 循环 `0x45e7f0`–`0x45e7fd`（`PUSH EDI`(bullet) / `CALL [vtbl+0x28]` / `ESI=[ESI+4]`）→ **本站点** →
 * `0x45e837 push [edi+0x9c]` 才把伤害交给伤害源（`0x45e861 call 0x45dfa0`）。
 *
 * **为什么必须放在遍历之后**（`CARD-NATIVE-CONFLICT-AUDIT.md` §B-1，用户 2026-09-13 拍「按语义叠加」）：
 * 原生两张装备卡与我们**同槽**（`+0x28`），而且都是**从 shooter 基值重算**派：
 *   · `CardMomoyo__on_bullet_init` `0x411010` = `min(1.8, 1 + 击杀/N) × shooter+0x02`（54 暴食蜈蚣，×1.0→1.8）；
 *   · `CardSaki____on_shoot_2` `0x40c310` = `shot type7 时 shooter+0x02 × 1.4`（另把尺寸/速参 ×1.5）。
 * ⇒ 原来落笔在桩里时是**顺序依赖**：它们在链里**之后**就从基值重算，把我们那条 ×1.25 整条抹掉（静默）；
 *   卡链顺序 = 获得/存档顺序，玩家不可控。放到出口上 ⇒ 我们永远乘在**别人算完的最终值**上：
 *   ×1.25 × 1.4 = ×1.75（乘法叠加，不是覆盖），与链序无关。
 *
 * **为什么不重复施加**（选「取代」而不是「标记去重」，理由）：本站点是全 exe**唯一**的落笔点 ——
 * 普查（`local/_scratch/scan_bp_window.py`）显示 `call [reg+0x28]` 共 8 处，只有 `0x45e7f5` 在**卡链**上
 * （其余 7 处是别的对象类型的虚表调用）⇒ 桩里那次缩放已删除（`sdk.h` 的 `CE_CARD`）。
 * 因此**不需要**任何「本帧是否已施加」的状态：没有状态就没有「状态过期 / 漏清 / 嵌套创建」这一类失效模式，
 * 而单次落笔由「直线码只执行一次」直接保证。⚠ 反过来说：**别把缩放加回桩里**，那会变成同帧施加两次。
 * 单次取整的性质也保住了（一颗弹只乘一次）。
 *
 * `+0x2c`（`on_tick_2` 桩）：每敌每帧总伤害上限 `player+0x47984` 每帧从 `sht+0x28` 复位
 * （GameThread 0x10 早于 AbilityManager 0x16），不按同一倍率抬起来，加的伤害在贴脸打 boss 时会被配额吃掉
 * （§V20 / §Y5 / §AJ10 是同一件事的三次重犯）。这一层把它变成自动的：卡只要报倍率，上限跟着走。
 * ⚠ 上限仍由**声明者里 id 最小那张**（`ce_dmg_applier()`）在自己的 `on_tick_2` 里抬 —— 与伤害落笔点无关。*/
void ce_sdk_bullet_damage_final(void *bullet)
{
    int32_t *dmg;
    int agg = ce_dmg_aggregate();
    if (agg == CE_DMG_UNIT || !bullet) return;      /* 没有加伤害的卡 = 中性：一个字节都不动 */
    dmg = (int32_t *)((uint8_t *)bullet + CE_BULLET_DAMAGE);
    *dmg = ce_dmg_scale(*dmg);                      /* 就地乘「别人的最终值」= 乘法叠加 */
}

void ce_sdk_damage_cap_tick(uint32_t id)
{
    const uint8_t *p = CE_PLAYER();
    const uint8_t *sht;
    int base;
    if ((int)id != ce_dmg_applier() || ce_dmg_aggregate() == CE_DMG_UNIT || !p) return;
    sht = *(const uint8_t *const *)(p + CE_PLAYER_SHT_PTR);
    if (!sht) return;
    base = *(const int32_t *)(sht + CE_SHT_MAX_DMG);
    if (base > 0) ce_raise_damage_cap(ce_dmg_scale(base));
}

/* HUD 文字。原生 `CardMomoyo__on_draw` `0x4110a0` 的做法逐条复刻（AUDIT §AC）：
 * 写 `ASCII_MANAGER_PTR + 0x19228` 的字色 → 建字符串 → 还原成白。
 * ★★ 2026-09-10 修正：**必须用 `AsciiManager__create_string` `0x419b90`**，不能用 `drawf_debug`
 * `0x41a110` —— 后者在 create_string 之后还会写 `mgr + 0xc0c + num_strings*0x138 = 1`，
 * 而那个地址就是**刚建那条记录的 `+0x120`（font_id）** ⇒ 我们设的字体被强行改成 **1**，
 * 于是「字体扫描」五行全长一个样、换字体毫无效果（反汇编：`0x41a14e`/`0x41a15d`）。
 * `create_string` 是 thiscall(mgr, pos[3], text)，文本我们已 vsnprintf 好，直接传指针。*/
typedef void (__attribute__((thiscall)) *ce_fn_ascii_create_t)(void *mgr, const float *pos, const char *text);

/* 真正干活的那层。font/group/align 传 -1 表示不动。 */
static void ce_hud_text_raw(float x, float y, uint32_t bgra, uint32_t shadow,
                            int font_id, int group, int align_h, float scale, const char *text)
{
    uint8_t *mgr = *(uint8_t **)CE_ADDR_ASCII_MANAGER_PTR;
    float pos[3], *sc, old_sc[2];
    uint32_t old_color, old_shadow = 0;
    int old_font = 0, old_group = 0, old_align = 0, old_shadows = 0;

    if (!mgr) return;
    pos[0] = x; pos[1] = y; pos[2] = 0.0f;
    sc = (float *)(mgr + CE_ASCII_SCALE);
    old_color = *(uint32_t *)(mgr + CE_ASCII_COLOR);
    *(uint32_t *)(mgr + CE_ASCII_COLOR) = bgra;
    if (shadow) {
        old_shadow = *(uint32_t *)(mgr + CE_ASCII_COLOR2);
        *(uint32_t *)(mgr + CE_ASCII_COLOR2) = shadow;
        old_shadows = *(int *)(mgr + CE_ASCII_DRAW_SHADOWS);
        *(int *)(mgr + CE_ASCII_DRAW_SHADOWS) = 1;          /* 打开描边遍 */
    }
    if (font_id >= 0) { old_font  = *(int *)(mgr + CE_ASCII_FONT);    *(int *)(mgr + CE_ASCII_FONT)    = font_id; }
    if (group   >= 0) { old_group = *(int *)(mgr + CE_ASCII_GROUP);   *(int *)(mgr + CE_ASCII_GROUP)   = group; }
    if (align_h >= 0) { old_align = *(int *)(mgr + CE_ASCII_ALIGN_H); *(int *)(mgr + CE_ASCII_ALIGN_H) = align_h; }
    if (scale > 0.0f) { old_sc[0] = sc[0]; old_sc[1] = sc[1]; sc[0] = scale; sc[1] = scale; }
    ((ce_fn_ascii_create_t)CE_FN_ASCII_CREATE)(mgr, pos, text);
    if (scale > 0.0f) { sc[0] = old_sc[0]; sc[1] = old_sc[1]; }
    if (align_h >= 0) *(int *)(mgr + CE_ASCII_ALIGN_H) = old_align;
    if (group   >= 0) *(int *)(mgr + CE_ASCII_GROUP)   = old_group;
    if (font_id >= 0) *(int *)(mgr + CE_ASCII_FONT)    = old_font;
    if (shadow) {
        *(int *)(mgr + CE_ASCII_DRAW_SHADOWS) = old_shadows;
        *(uint32_t *)(mgr + CE_ASCII_COLOR2) = old_shadow;
    }
    *(uint32_t *)(mgr + CE_ASCII_COLOR) = old_color;
}

void ce_hud_text(float x, float y, uint32_t bgra, const char *fmt, ...)
{
    char buf[128];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    ce_hud_text_raw(x, y, bgra, 0, -1, -1, -1, 0.0f, buf);
}

/* 同上，但指定字体 / 分组 / 水平对齐（三者传 -1 = 用环境当前值）。
 * 金钱那行是 font 10 / group 3 / 右对齐，比默认的 font 0 大一圈 —— 想和它看着是一套就照抄。
 * 每一项都**先存后改、画完还原**：引擎后面的绘制全靠这些环境值，改了不还等于给别人埋雷。*/
/* 原始键盘状态。窗口没焦点 / 线程没泵消息时 GetKeyboardState 给的是旧状态 —— 游戏主线程两者都满足。
 * 256 字节栈缓冲，按虚拟键码索引，高位（0x80）置位 = 按下。*/
int ce_key_down(int vk)
{
    unsigned char st[256];
    if (vk < 0 || vk > 255) return 0;
    if (!GetKeyboardState(st)) return 0;
    return (st[vk] & 0x80) != 0;
}

void ce_hud_text_ex(float x, float y, uint32_t bgra, uint32_t shadow,
                    int font_id, int group, int align_h, float scale, const char *fmt, ...)
{
    char buf[128];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    ce_hud_text_raw(x, y, bgra, shadow, font_id, group, align_h, scale, buf);
}

/* `+0x48`：`AbilityManager__on_draw` `0x408ab0` 每帧遍历卡链表调它。把聚合倍率写在火力值左边，
 * 只有落笔人画、且只在倍率不是 ×1 时画（没有加伤害的卡时 HUD 干干净净）。 */
void ce_sdk_damage_hud_draw(uint32_t id)
{
    int agg = ce_dmg_aggregate();
    if ((int)id != ce_dmg_applier() || agg == CE_DMG_UNIT) return;
    ce_hud_text(CE_HUD_DMG_X, CE_HUD_POWER_Y, CE_HUD_POWER_COLOR,
                "x%d.%02d", agg / CE_DMG_UNIT, agg % CE_DMG_UNIT / 10);
}

void ce_set_bullet_damage_permille(ce_card_t *c, int permille)
{
    if (!c) return;
    ce_dmg_set(ce_card_id(c), permille);
}

/* ---- 卡侧原语的实现（声明、用法与出处见 sdk.h）------------------------------------ */

int ce_refuse(const char *card, const char *why)
{
    ce_play_sound(CE_SE_INVALID, ce_player_x());
    ce_log("%s: refused (%s)", card, why);
    return CE_ACTIVATE_REFUSED;
}

uint8_t *ce_option_claim(ce_card_t *c, ce_option_t *o)
{
    uint8_t *p = o ? o->ptr : 0;
    if (!p) return 0;
    if (*(int32_t *)(p + CE_OPT_IN_USE) == 0 ||
        *(uint32_t *)(p + CE_OPT_OWNER_INDEX) != *(uint32_t *)((uint8_t *)c + CE_CARD_ARRAY_INDEX)) {
        o->ptr = 0;                                  /* 槽被回收 / 被别的卡拿走了 */
        return 0;
    }
    return p;
}

/* ---- 低速子机声明层：卡侧两个入口（层本身在 sdk_core.c；落笔分派见本文件的 `ce_option_pose_frame_tick`）---- */
int ce_option_pose_declare(ce_card_t *c, const ce_option_pose_t *p)
{
    if (!c) return 0;
    return ce_optpose_declare(ce_card_id(c), p);
}

void ce_option_pose_drop(ce_card_t *c)
{
    if (!c) return;
    ce_optpose_drop(ce_card_id(c));
}

/* ---- ★★ 2026-09-16：「原生子机」自愈兜底（声明/断点见上面那一整段与 `sdk.h`）----
 * 每帧把 4 格 × 2 支（`+0xb0` 普通形 / `+0xb4` 满火力形）清掉：非 0 才动，正常帧零开销。
 * ⚠ 为什么断点之外还要这一支：`repopulate` 的档位短路缓存（`inner+0x470d8`）会让引擎在
 *   「档位没变」时**既不重建也不清理** ⇒ 卡刚装上 / 换机体回来那一刻场上已有的原生子机
 *   会一直留着（幽灵子机 + 出膛点仍在）。这一支把「不管谁建的、只要我在就收掉」变成硬保证。*/
/* ★ 2026-09-23：**这一轮隐藏期内真的收掉过原生子机 VM** 的旗子（由 sweep 置、由可见化补重建清）。
 *   `BP_ce_item_frame` 只在它立着时才补 `repopulate` ⇒ **没杀过就一次都不重建**（快速高低切时不会
 *   按帧触发重建 / 广播；见那里的长注）。*/
/* 判定「扫描停手了」的静默帧数：必须 > 0（同帧内卡 tick 与这里的先后不保证）且足够小
 * （松 Shift 之后原生子机要立刻回来；4 帧 = 66 ms，肉眼看不见延迟）*/
#define SDK_NATOPT_SWEEP_QUIET 4u
static int s_natopt_swept;
/* ★★★ 2026-09-26（owner 实跑：「咲夜妖机体的子机一直在重建」）：**本帧有没有卡在主动收原生子机**。
 *   两个 sweep（老的 `ce_native_option_sweep()` 与 108 用的 `..._keep_state()`）在**被调用**时都盖一个
 *   帧号戳（不只是「真收掉了」才盖）—— 因为「卡在躲这个」这件事本身才是判据（收干净之后每帧返回 0，
 *   只看「收掉过」就看不见它还在扫）。用途见下面 restore 那一段。*/
static uint32_t s_natopt_sweep_frame;

int ce_native_option_sweep(void)
{
    uint8_t *pl = CE_PLAYER();
    int i, n = 0;

    if (!pl) return 0;
    for (i = 0; i < CE_NATIVE_OPT_COUNT; ++i) {
        uint8_t *slot = pl + CE_NATIVE_OPT_PLAYER_MUZ0 + (size_t)i * CE_NATIVE_OPT_STRIDE;
        uint32_t *vm[2];
        int k;

        vm[0] = (uint32_t *)(slot + CE_NATOPT_VM_MAIN);
        vm[1] = (uint32_t *)(slot + CE_NATOPT_VM_MAX);
        for (k = 0; k < 2; ++k) {
            if (*vm[k]) { ce_anm_interrupt(*vm[k], 1); *vm[k] = 0; ++n; }
        }
        if (*(int32_t *)(slot + CE_NATOPT_STATE) != 0) *(int32_t *)(slot + CE_NATOPT_STATE) = 0;
    }
    if (n) {
        static int s_logged;
        s_natopt_swept = 1;        /* ★ 见 `BP_ce_item_frame` 的「可见化补重建」：真收掉过才需要补 */
        s_natopt_sweep_frame = ce_sdk_frame();   /* ★ 2026-09-26：本帧有人在扫（见 restore 那一段）*/
        if (!s_logged) {
            s_logged = 1;
            ce_log("sdk: 原生子机自愈清理：本帧收掉 %d 支 VM（声明者 id %d）",
                   n, ce_native_option_hide_owner());
        }
    }
    return n;
}

int ce_native_option_sweep_keep_state(void)
{
    uint8_t *pl = CE_PLAYER();
    int i, n = 0;

    if (!pl) return 0;
    for (i = 0; i < CE_NATIVE_OPT_COUNT; ++i) {
        uint8_t *slot = pl + CE_NATIVE_OPT_PLAYER_MUZ0 + (size_t)i * CE_NATIVE_OPT_STRIDE;
        uint32_t *vm[2];
        int k;
        vm[0] = (uint32_t *)(slot + CE_NATOPT_VM_MAIN);
        vm[1] = (uint32_t *)(slot + CE_NATOPT_VM_MAX);
        for (k = 0; k < 2; ++k)
            if (*vm[k]) {
                /* ★★★ 2026-09-27（§BE #119，owner：「**灵梦变回来的人子机变虚了，并且似乎大了 1.5-2 倍**」）：
                 *   这里原来发的是 `interrupt(1)` + 把 id 写 0。**`interrupt(1)` 不是「删掉 / 藏起来」**，
                 *   而是让那支 VM 去跑它的**中断支脚本**（TH18 原生子机脚本的 1 号支 = 一段缩放/淡出动画）
                 *   —— 而 id 已经被写 0 ⇒ **引擎再也不会回收这支 VM，也没人会把它恢复成正常形态**
                 *   ⇒ 它就停在那段动画的状态里：**半透明 + 放大 1.5–2 倍**，正是 owner 看到的那一台。
                 *   ⇒ 改成 **标记删除**（一手 `0x488cf0`：置 `vm+0x538` bit7，随后由 anm 管理器真正回收）
                 *   —— 显示立刻没了、记录（`state`/坐标）照旧保留（= 本函数存在的全部意义），
                 *   松 Shift 之后仍由 `ce_repopulate_options()` 把两支 VM 重建回来（那条路已经在跑，
                 *   见本文件上面的「可见化补重建」那一段）。
                 *   ⚠ 与老 `ce_native_option_sweep()` 的差别因此仍然只有一条：**不碰 `state` 与坐标**。*/
                ce_anm_delete(*vm[k]);
                *vm[k] = 0;
                ++n;
            }
        /* ★ 与 `ce_native_option_sweep()` 的**唯一区别**：`state`（`+0x00`）与坐标**不动** */
    }
    if (n) s_natopt_swept = 1;      /* 与老 sweep 共用同一个旗子（可见化时补 repopulate） */
    s_natopt_sweep_frame = ce_sdk_frame();       /* ★ 2026-09-26：本帧有人在扫（**无条件**盖戳，见 restore）*/
    return n;
}

uint8_t *ce_option_alloc(ce_card_t *c, ce_option_t *o, int offset, int anm_script, const char *card)
{
    if (!o || !CE_PLAYER()) return 0;
    o->ptr = (uint8_t *)ce_allocate_option(c, offset, anm_script);
    ce_log("%s: option %s (ptr %08x, anm id %08x)", card, o->ptr ? "allocated" : "FAILED (pool full)",
           (unsigned)(uintptr_t)o->ptr, *(uint32_t *)((uint8_t *)c + CE_CARD_OPTION_ANM_ID));
    return o->ptr;
}

uint8_t *ce_option_retry(ce_card_t *c, ce_option_t *o, int offset, int anm_script,
                         uint32_t retry_frames, const char *card)
{
    if (!o) return 0;
    if (o->retry) { o->retry--; return 0; }
    o->retry = retry_frames;
    return ce_option_alloc(c, o, offset, anm_script, card);
}

/* 收掉挂在某个子机上的自机弹（激光）。一手：`Player__tick_bullets` `0x45edb0` 的池
 * （`PLAYER+0x1570`、512 槽 × `0xf8`；`0x45edd5 LEA EDI,[ECX+0x1570]`、`0x45f0b0 ADD EDI,0xf8`）、
 * `PlayerBullet__create` `0x45e3ae` 把子机指针写进 `bullet+0xe4`、
 * `tick2` `0x45fc80` 尾部的自杀段（`bullet+0x8c = 2` + 对 `bullet+8` 的束 VM 发 interrupt 1）。*/
int ce_option_kill_bullets(void *option)
{
    uint8_t *pl = CE_PLAYER();
    int i, n = 0;

    if (!pl || !option) return 0;
    for (i = 0; i < CE_PB_COUNT; ++i) {
        uint8_t *b = pl + CE_PB_POOL + i * CE_PB_STRIDE;
        if (*(int32_t *)(b + CE_PB_STATE) != 1) continue;              /* 只收「活动」的弹 */
        if (*(void **)(b + CE_PB_OPTION) != option) continue;
        *(int32_t *)(b + CE_PB_STATE) = 2;                             /* tick2 自杀段第一步 */
        ce_anm_interrupt(*(uint32_t *)(b + CE_PB_VM_ID), 1);           /* 束脚本 interrupt 1 = 收束淡出 */
        ++n;
    }
    if (n > 0) ce_log("option: killed %d lasers attached to %08x", n, (unsigned)(uintptr_t)option);
    return n;
}

/* 清 `player+0x476fc+idx*4` 的 mode=2 去重槽。引擎自己的自杀段在 `0x4602c1` 做同一件事；
 * 不镜像它，`Player__shoot_one_bullet` `0x45e96e` 会一直看到该槽非 0，整条 mode=2 row 永久早退
 *（用户 2026-09-13：咆哮后 2p/4p 某些子机不再发激光；重开一局才恢复）。
 * 槽位解码逐指令对齐 `0x4602a8`：byte `shooter+0x20`，0 跳过；<0x10 取低 4 位，否则取高 4 位。*/
static void ce_mode2_lock_clear(uint8_t *pl, const uint8_t *sr)
{
    int slot, idx;

    if (!pl || !sr) return;
    slot = (int)*(const int8_t *)(sr + CE_SHT_ROW_OPT_SLOT);
    if (slot == 0) return;                                  /* 0x460227 `test al,al; je 0x46030e` */
    if (slot < 0x10) idx = slot & 0xf; else idx = slot >> 4;
    if (idx < 0 || idx >= CE_PLAYER_MODE2_LOCK_SLOTS) return;
    *(int32_t *)(pl + CE_PLAYER_MODE2_LOCK_BASE + idx * 4) = 0;
}

/* ★ 2026-09-13：咆哮期间清掉「零售 / 装备子机」的 `mode=2` 激光。
 * 真身（一手）：`mode=2` 激光的寿命**不跟 set 走** —— `tick2` `0x45fc80` 的存活分支
 * `0x4601e0 cmp [player+0x476d4],0 / jl 自杀` 只看**射击短计时**（松开 Z 才 <0）；
 * `fire-time set` 重定向 `0x45ebfb` 只改本条之后取哪组 row，**挡不住已经在飞的旧束**
 * ⇒ 按住 Z 进咆哮：旧束一直钉在原 sub-machine 上（用户 2026-09-13「子机还在打普通激光」）。
 * 过滤：packed set（`bullet+0xac >> 8`）≤ `0x15`（0..9 主炮 + 0x0a..0x15 原生装备子机组）
 * 且 shooter row `+0x21 == 2`；`0x16..0x1d`（108 使魔 / 77 豌豆 / 116 季节）与
 * `0x1e..0x27`（117 WOLF/EAGLE）都不碰。
 *
 * ★ 2026-09-13 十九修（回归真身）：上一版只写 `state=2` + interrupt 1，漏了引擎自杀段第三步
 *   `0x4602c1 mov [player+edx*4+0x476fc],0`（`edx` 由 `shooter+0x20` 解码）。`+0x476fc`
 *   是「该 mode=2 槽正在持有一发激光」的去重表：`Player__shoot_one_bullet` `0x45e96e`
 *   看到非 0 就直接 `ret 0x10` 不发新弹。我们收掉了旧弹却没解锁 ⇒ 咆哮结束后那些子机永远
 *   发不出激光，只有重开一局才清空。现在逐字段对齐引擎：state=2 → interrupt 1 → 清锁。*/
int ce_kill_retail_option_lasers(void)
{
    uint8_t *pl = CE_PLAYER();
    uint8_t *sht;
    uint32_t by_set[CE_SHT_SET_OPTION_MAX + 1];
    int i, s, n = 0;

    for (s = 0; s <= CE_SHT_SET_OPTION_MAX; ++s) by_set[s] = 0;
    if (!pl) return 0;
    sht = *(uint8_t **)(pl + CE_PLAYER_SHT_PTR);   /* zPlayer+0x47940 */
    if (!sht) return 0;
    for (i = 0; i < CE_PB_COUNT; ++i) {
        uint8_t *b = pl + CE_PB_POOL + i * CE_PB_STRIDE;
        uint32_t packed, set, row;
        uint8_t *sr;

        if (*(int32_t *)(b + CE_PB_STATE) != 1) continue;     /* 只收活动弹 */
        packed = *(uint32_t *)(b + CE_PB_PACKED_ID);
        set = packed >> 8;
        if (set > CE_SHT_SET_OPTION_MAX) continue;            /* ≤ 0x15：挡在 0x16..0x1d / 0x1e..0x27 之外 */
        row = packed & 0xffu;
        /* 引擎的 set 表在内存里是**绝对指针**（装载器已 +buf+0x180）；用旧 set 照样解到旧 row。*/
        sr = *(uint8_t **)(sht + 0xe0 + set * 4);
        if (!sr) continue;
        if (*(const int8_t *)(sr + row * 0x5c + CE_SHT_ROW_MODE) != 2) continue; /* 只收 mode=2 激光 */
        *(int32_t *)(b + CE_PB_STATE) = 2;                    /* `tick2` 自杀段：0x460296 */
        ce_anm_interrupt(*(uint32_t *)(b + CE_PB_VM_ID), 1);  /* 束脚本 interrupt 1：0x4602a0 */
        ce_mode2_lock_clear(pl, sr + row * 0x5c);             /* 去重槽清 0：0x4602c1 */
        ++by_set[set];
        ++n;
    }
    if (n > 0) {
        for (s = 0; s <= CE_SHT_SET_OPTION_MAX; ++s) {
            if (by_set[s])
                ce_log("wba_c: retail option laser kill: set 0x%x x%u", (unsigned)s, by_set[s]);
        }
        ce_log("wba_c: retail option laser kill: total n=%d (state=2 + interrupt 1 + shot lock cleared)", n);
    }
    return n;
}

/* 收起子机：删它的 ANM VM（原生 `CardReimu1__operator_delete` `0x40ab20` 同款）、清原生在同一时机清的
 * flags bit1（`CardReimu1__method_4C` `0x40aad0`）、**收掉它发射出去的激光**，松开我们记的指针。
 * 槽本身由引擎统一回收。*/
void ce_option_drop(ce_card_t *c, ce_option_t *o)
{
    ce_card_flags(c) &= ~(uint32_t)CE_FLAG_EQUIP_RESET_BIT1;
    if (o && o->ptr) {
        /* ★ 按**这个槽自己**的 VM id 删（option+0xb0）——不能一律删 card+0x1c：
         * 引擎的 `Player__allocate_option` 每次分配都覆盖 card+0x1c（`th18:0x40a906`），
         * 多子机卡里 card+0x1c 只记得**最后一个**；而且「收起没分配过的槽」时若按 card+0x1c 删，
         * 会把**刚分配好的那颗**一起删掉（2026-09-09 季节卡实机：子机一直不出现就是这么来的）。
         * 槽里的 VM id 在 option+0xb0（`th18:0x40a909`），分配器自己也按它清旧 VM（`th18:0x40a7d2`）。*/
        uint32_t vm = *(uint32_t *)(o->ptr + CE_OPT_ANM_ID);
        ce_option_kill_bullets(o->ptr);            /* ★ 先收激光：束只认射击键，子机没了它不会自己走 */
        if (vm) {
            uint32_t *card_vm = (uint32_t *)((uint8_t *)c + CE_CARD_OPTION_ANM_ID);
            ce_anm_delete(vm);
            *(uint32_t *)(o->ptr + CE_OPT_ANM_ID) = 0;
            if (*card_vm == vm) *card_vm = 0;      /* 引擎那格也清掉，免得原生析构再删一次 */
        }
        /* ★ 槽也要真的松开：`Player__allocate_option` `0x40a790` 找空位的判据就是
         * `equipment[i].in_use == 0`（`while (*(int *)pzVar11->field_0 != 0)`），**没有别的回收者**
         * （全程序只有它自己写 in_use 与 option+0xb0）。不置 0 就是每收一次子机漏一格 ——
         * 12 格用完后新子机再也申请不到（日志会是 `option allocated FAILED (pool full)`）。*/
        *(int32_t *)(o->ptr + CE_OPT_IN_USE) = 0;
        o->ptr = 0;
        o->retry = 0;
    } else {
        uint32_t *vm = (uint32_t *)((uint8_t *)c + CE_CARD_OPTION_ANM_ID);
        if (*vm) { ce_anm_delete(*vm); *vm = 0; }   /* o == NULL：原生单子机写法 */
    }
}

static void hud_digit_kill(ce_hud_digit_t *d)
{
    if (d->digit_id) ce_anm_delete(d->digit_id);
    d->digit_id = 0;
    d->shown = -1;
}

void ce_hud_digit_clear(ce_hud_digit_t *d)
{
    if (!d) return;
    hud_digit_kill(d);
    d->icon_id = 0;
}

void ce_hud_digit_attach(ce_hud_digit_t *d, uint32_t icon_anm_id, int value, const char *card)
{
    if (!d) return;
    if (d->digit_id) hud_digit_kill(d);                    /* 图标重建：旧数字先删 */
    d->icon_id = icon_anm_id;
    ce_hud_digit_set(d, value, card);
}

void ce_hud_digit_set(ce_hud_digit_t *d, int value, const char *card)
{
    uint8_t *icon;
    int v;

    if (!d) return;
    icon = ce_anm_get_vm(d->icon_id);
    if (!icon) {                                           /* 图标没了（进店 / 卡被删）：数字跟着走 */
        if (d->digit_id) { hud_digit_kill(d); ce_log("%s: hud icon gone, digit removed", card); }
        d->icon_id = 0;
        return;
    }
    if (d->digit_id && !ce_anm_get_vm(d->digit_id)) { d->digit_id = 0; d->shown = -1; }
    if (!d->digit_id) {
        const float *ip = (const float *)(icon + CE_ANM_VM_POS);
        d->digit_id = ce_anm_spawn_ui(CE_ABILITY_ANM(), CE_ANM_ABILITY_SCRIPT_HUD_DIGIT, ip);
        d->shown = -1;
        ce_log("%s: hud digit %s (icon id %u at %.1f, %.1f, %.1f; digit id %u)", card,
               d->digit_id ? "spawned" : "FAILED", d->icon_id, ip[0], ip[1], ip[2], d->digit_id);
        if (!d->digit_id) return;
    }
    v = value < 0 ? 0 : value > CE_HUD_DIGIT_MAX ? CE_HUD_DIGIT_MAX : value;
    if (v != d->shown && ce_anm_set_sprite(CE_ABILITY_ANM(), d->digit_id, CE_ANM_ABILITY_SPRITE_TS_DIGIT_0 + v))
        d->shown = v;
}

void ce_hud_number_clear(ce_hud_number_t *n)
{
    if (!n) return;
    for (int i = 0; i < CE_HUD_NUMBER_MAX; ++i) { ce_anm_delete(n->id[i]); n->id[i] = 0; n->shown[i] = -1; }
    n->digits = 0;
}

void ce_hud_number_set(ce_hud_number_t *n, float x, float y, float spacing,
                       int value, int digits, int right_align, const char *tag)
{
    int d[CE_HUD_NUMBER_MAX], i;
    void *anm = CE_ABILITY_ANM();

    if (!n || !anm) return;
    if (digits < 1) digits = 1;
    if (digits > CE_HUD_NUMBER_MAX) digits = CE_HUD_NUMBER_MAX;
    if (value < 0) value = 0;
    for (i = digits - 1; i >= 0; --i) { d[i] = value % 10; value /= 10; }

    if (n->digits != digits) { ce_hud_number_clear(n); n->digits = digits; }
    for (i = 0; i < digits; ++i) {
        float px = right_align ? x - spacing * (digits - 1 - i) : x + spacing * i;
        float pos[3];
        pos[0] = px; pos[1] = y; pos[2] = 0.0f;
        if (n->id[i] && !ce_anm_get_vm(n->id[i])) { n->id[i] = 0; n->shown[i] = -1; }
        if (!n->id[i]) {
            n->id[i] = ce_anm_spawn_ui(anm, CE_ANM_ABILITY_SCRIPT_HUD_NUMBER, pos);
            n->shown[i] = -1;
            if (!n->id[i]) { ce_log("%s: hud number digit %d FAILED to spawn", tag, i); return; }
        } else {
            ce_anm_set_pos(n->id[i], px, y, 0.0f);
        }
        if (d[i] != n->shown[i] && ce_anm_set_sprite(anm, n->id[i], CE_ANM_ABILITY_SPRITE_TS_DIGIT_0 + d[i]))
            n->shown[i] = d[i];
    }
}

/* ============================ 贴图 VM 版文字 ============================ */

/* 字符 → 原生字形 sprite。映射一手 = **`.rdata` 里 4 张 256 字节的字符→字形表**
 * （`th18:0x4b019c` / `0x4b029c` / `0x4b031c` / `0x4b041c`，索引 = 字符码、值 = 精灵号；
 *  数字 `'0'..'9'` → 196..205 与下面这张表逐值相同 ⇒ 不是巧合）：
 *   `' '`=180、`'-'`=193、`'%'`=185、`'+'`=191、`'.'`=194、**小写字母 = 245..255 + 128..142**
 *   ⇒ `'s'` = 精灵 **135**（16×16 那一档；见 AUDIT §CY-3）。
 * 切图见 assets/ability/make_font2_art.py，进 ability.anm 后就是 anm_ids.h 里的 FONT2_*。*/
static uint32_t hud_vm_glyph(char ch)
{
    if (ch >= '0' && ch <= '9') return CE_ANM_ABILITY_SPRITE_FONT2_0 + (uint32_t)(ch - '0');
    switch (ch) {
    case '-': return CE_ANM_ABILITY_SPRITE_FONT2_MINUS;
    case '%': return CE_ANM_ABILITY_SPRITE_FONT2_PERCENT;
    case '.': return CE_ANM_ABILITY_SPRITE_FONT2_DOT;
    case '+': return CE_ANM_ABILITY_SPRITE_FONT2_PLUS;
    /* ⚠ `':'` / `'*'` **不放这一档**：它们不在 `ATLAS_FONT2` 里，而这个字形表绑的就是那个 entry
     *   （引擎按 entry 的精灵表解析 sprite 号）⇒ 要用它们请用下一档 `hud_vm_glyph_font2b`。*/
    default:  return 0;                      /* 空格等：按字距跳过 */
    }
}

static float hud_vm_ui_scale(void)
{
    float s = *(const float *)CE_ADDR_UI_SCALE;
    return (s > 0.1f && s < 16.0f) ? s : 2.0f;
}

/* ★ 2026-09-15：第二套字形 = **TH13 大字体**（用户口径：灵界的字符要用 TH13 那张 `ascii.png`，
 * 不用 TH18 遗留的）。字形来自 `ATLAS_TH13ASCII`（切图脚本 assets/ability/make_th13_ascii_art.py）：
 * 图里唯二 39 px 高的两行、格距 32、行距 40 ⇒ 12 个字形 = `0`..`9` `.` `s`。
 * 它在 640 空间是 16×20（脚本里 resolutionMode(2) ⇒ 1280→640 减半），所以步进 16、半行高 10。*/
/* ★ 2026-09-24（卡 112）：**`ATLAS_FONT2B` 版**字形表 —— `ATLAS_FONT2` 的完整复制，
 *   只多了 `':'` 与 `'*'`（乘号）两个字形。为什么要独立一档见 `assets/ability/scripts/332_*.anm.txt` 的头注。
 *   ⚠ **字形真身 ≠ 字符表**：引擎那张「字符码 → 精灵号」表（`th18:0x4b019c` 等四张同值）在标点段是
 *     线性的（`180 + (char − 0x20)`）⇒ `':'` → 206、`'*'` → 190；但那张字形表（`ascii_1280.png`
 *     的 y=320 行）的标点顺序**不是** ASCII 顺序，逐像素量出来：**206 = `/`（斜线）、207 = `:`（两点）**、
 *     208 = `-`、**209 = `×`**、210 = `%`、211 = `.`、212 = `+`，而 **190 = 一条竖线**。
 *     ⇒ 本档的 `':'` / `'*'` 取的是**207 / 209**（owner 实跑「剩余时间是 xx/yy」就是 206 那条斜线）。
 *   详见 `assets/ability/make_font2_art.py` 的 `GLYPHS` 注。*/
static uint32_t hud_vm_glyph_font2b(char ch)
{
    if (ch >= '0' && ch <= '9') return CE_ANM_ABILITY_SPRITE_FONT2B_0 + (uint32_t)(ch - '0');
    switch (ch) {
    case '-': return CE_ANM_ABILITY_SPRITE_FONT2B_MINUS;
    case '%': return CE_ANM_ABILITY_SPRITE_FONT2B_PERCENT;
    case '.': return CE_ANM_ABILITY_SPRITE_FONT2B_DOT;
    case '+': return CE_ANM_ABILITY_SPRITE_FONT2B_PLUS;
    case ':': return CE_ANM_ABILITY_SPRITE_FONT2B_COLON;
    case '*': return CE_ANM_ABILITY_SPRITE_FONT2B_STAR;
    default:  return 0;
    }
}

/* ★★ 2026-09-25 第 4 档：TH12 的数字字形（16×16 那一族，见 `CE_HUD_FONT_TH12_DIGITS`）。
 *   字符→sprite 的对应 = 逐格渲染核对过的**图形**（不是猜）：243..252 = `0..9`、254 = `.`、
 *   257 = `,`、256 = 乘号字形（一手 TH12 `on_draw_16` 的格式串 `"*%1.1f"` 里的 `*`）。*/
static uint32_t hud_vm_glyph_th12(char ch)
{
    if (ch >= '0' && ch <= '9') return CE_ANM_ABILITY_SPRITE_F12_D0 + (uint32_t)(ch - '0');
    switch (ch) {
    case '.': return CE_ANM_ABILITY_SPRITE_F12_DOT;
    case ',': return CE_ANM_ABILITY_SPRITE_F12_COMMA;
    case '*': return CE_ANM_ABILITY_SPRITE_F12_X;
    default:  return 0;                      /* 空格等：按字距跳过（这套只有 13 个字形）*/
    }
}

static uint32_t hud_vm_glyph_th13(char ch)
{
    if (ch >= '0' && ch <= '9') return CE_ANM_ABILITY_SPRITE_TH13ASCII_D0 + (uint32_t)(ch - '0');
    switch (ch) {
    case '.': return CE_ANM_ABILITY_SPRITE_TH13ASCII_DOT;
    case 's': return CE_ANM_ABILITY_SPRITE_TH13ASCII_S;
    default:  return 0;                      /* 空格等：按字距跳过（TH13 这套只有 12 个字形）*/
    }
}

/* 字体描述：字形表 / 字距 / 生成脚本 / 半行高（VM 的 y 补偿）/ VM 到墨迹上沿的距离（640 空间）*/
typedef struct {
    uint32_t (*glyph)(char);
    float    adv;          /* 默认步进（640 空间）*/
    float    adv_dot;      /* '.' 的步进（TH18 font 2 是 4，TH13 那套与其它字形同宽）*/
    uint32_t script;       /* 生成 VM 用的 anm 脚本（它绑定图集 ⇒ 决定了字形从哪个 entry 取）*/
    float    half_line;    /* 垂直对齐用（居中退半个行高）*/
    float    full_line;    /* 底对齐用（退一个行高）*/
    float    vm_dy;        /* VM 中心相对"墨迹上沿"的偏移 */
    float    size;         /* ★ 整体尺寸倍率（字形 VM 的 scale × 它；字距/行高/偏移都跟着它走）*/
} hud_font_t;

/* ★★ TH13 那行的度量 = **用户 2026-09-15 在 1280 档、屏幕缩放 100% 下逐字比对截图量出来的**
 *   像素差反推（用户口径：「整体需要往上移 1px / `s` 向右 29px / …每往左一个字形再少 4px /
 *   点需要 12px」）。换算：本表是 **640 空间**的字面量，最终到 1280 缓冲要 ×`size` × `dat`(=2)
 *   ⇒ 即 ×4（size=2 时）。于是：
 *     · 普通字形每格差 4 px(1280) ⇒ `adv` = 64/4 + 1 = **17**（1280 下 68 = 原 64 + 4）
 *     · `.` 到下一个数字差 9 px(1280) ⇒ `adv_dot` = 32/4 + 2.25 = **10.25**（1280 下 41）
 *     · 整体上移 1 px(1280) ⇒ `vm_dy` = 10 − 0.5 = **9.5**
 *     · 用户给的"最左边那个只需 +8"由**居中自动重算**给出（总宽变大 ⇒ 左边距同步变大）。
 *   ⚠ 这三个数就是"原版长啥样"的唯一可核对来源（TH13 的字符→步进表没能在 exe 里找到；
 *     它按原生像素画、scale=1.0，见 AUDIT §CZ-7）⇒ 想再微调只动这三个数，不用重切图。*/
static const hud_font_t HUD_FONTS[] = {
    { hud_vm_glyph,      7.0f, 4.0f, CE_ANM_ABILITY_SCRIPT_HUD_GLYPH,      5.0f, 10.0f,  5.0f, 1.0f },
    /* `size` = 2.0：原版按 `ascii.png` 的**原生像素**画（TH13 字符串记录 scale x=y=1.0，
     *   `th13:0x4059cf`），而本仓美术走 `resolutionMode(2)` 会被 DAT×0.5 折半 ⇒ ×2 才等于原版
     *   （32×40 字格在 640 空间 = 32×40 像素；字距/半行高/VM 偏移都跟着 size 走）。*/
    /* ⚠ `adv_dot` = `adv`（**均匀步进**）：2026-09-15 用户明确「s 向右 29px、最左边向右 4」
     *   ⇒ 各字形的位移是**均匀递增**的（点自己 12 ≈ 第 3 格、s 29 ≈ 第 7 格）⇒ 不存在"点后步进
     *   特别大/特别小"。点的字形在 32×40 字格里**自带留白**（见 make_th13_ascii_art.py），
     *   所以"挨着"是**美术本身**给的，不是靠砍步进。上一版把它设成 10.25 是理解错了。*/
    /* ★★ 2026-09-15 第二轮订正（用户给了具体字形：「C 向右 33、s 向右 29、C 在 29 基础上再向右 3」）：
     *   **相邻步进每格大了 4 px(1280)** ⇒ 步进要**收窄**：64 → 60 ⇒ `adv` = 60/4 = **15**
     *   （上一版我按"左多右少"读成了要变宽，属理解反了）。
     *   `adv_dot` 暂回 **8**（= 原来那个收紧值）：用户最早口径「小数点不算一个字符的大小，是挨着的」
     *   ⇒ 点那一格**比数字窄**；它的精确值等"逐字形位移表"（见 AUDIT §CZ-8）再定，先不动。
     *   `vm_dy` = 9.5（整体上移 1 px(1280)）保持不变。*/
    /* ★★ 2026-09-15 第三轮：用户给出**完整逐字位移表**（`A.BCs`，1280 档 100%）：
     *     A 53 / . 49 / B 37 / C 33 / s 29 ⇒ 相邻差 = 每段步进的修正量：
     *       A→. −4、.→B **−12**（点的格子特别窄）、B→C −4、C→s −4。
     *   ⇒ 普通格 64 → 60 px(1280) ⇒ `adv` = 60/4 = **15**
     *      点那一格 32 → 20 px(1280) ⇒ `adv_dot` = 20/4 = **5**
     *   ⇒ 剩下的 `A = 53` 是**锚点**差（整串往右 53），其中 12 来自"步进收窄后居中左缘右移"，
     *      其余 41 = 锚点残差 ⇒ 见 `trance.c` 的 `TRANCE_HUD_X_OFFSET`（单独一个旋钮，等下一张表定）。*/
    { hud_vm_glyph_th13, 15.0f, 5.0f, CE_ANM_ABILITY_SCRIPT_TH13_BIG_GLYPH, 10.0f, 20.0f, 9.5f, 2.0f },
    /* ★ 2026-09-24 第三档：TH18 font 2 的**完整字形集**（`ATLAS_FONT2B`）+ `:` `*`；
     *   度量与第 0 档逐值相同（7 / 4 / 10 / 5 / size 1.0）⇒ 换过去字号不变。*/
    { hud_vm_glyph_font2b, 7.0f, 4.0f, CE_ANM_ABILITY_SCRIPT_FONT2B_GLYPH, 5.0f, 10.0f, 5.0f, 1.0f },
    /* ★ 2026-09-25 第 4 档：TH12 数字字形（`ATLAS_FONT_TH12`，16×16 字格）。
     *   度量：字格与墨迹同宽 ⇒ **均匀 16 px 步进**（`.`/`,` 也一样，不像 TH18 font 2 那样收窄）；
     *   半行高 8 / 整行高 16；`vm_dy` = 8（贴图画布就是 16×16，精灵中心 = 墨迹中心）；
     *   `size` = 1.0（与 TH18 font 2 同档；调用点的 `scale` 决定绝对大小 —— 卡 112 的 ×N 用 2.0、
     *   底下那行点数用 1.0，正是 owner 说的「×n.0 是 scale 了一倍」）。*/
    /* ★★★ 2026-09-26 owner 第 29 轮：「**两个的大小都得大至少 1 倍多**」+「**字符间距极宽**」——
     *   两个数原来都错了，一手 = TH12 `AsciiManager::draw_string`（`th12:0x4018f0`）逐指令：
     *     · font 4（= `on_draw_16` 用的那一档）的**字距** = `scale.x × 12.0`
     *       （`4019e4 or [ebp+0x494],2` → `4019d0 fld [edi+0x110]; fmul st,st(2)`，st(2) = 常量
     *       `0x4a4128` = **12.0**）—— **不是**字格宽 16 ⇒ 上一版用 16（整格）⇒ 每字之间空 4 px = "极宽"；
     *     · `.` 与 `,` 的宽度 = `scale.x × 4.0`（`401a73`/`401aa8` 两条 `cmp [eax],0x2e/0x2c` 支，乘的是
     *       常量 `0x4a4108` = **4.0**）⇒ 点与逗号只占 1/3 格。
     *   `size`：本仓 UI 字形美术走 `resolutionMode(2)` 会被 DAT×0.5 折半（第 1 档 TH13 的先例）
     *   ⇒ 要让 16×16 texel 的字形在 **640 空间**里就是 16 单位（= TH12 原生大小），`size` 必须 **2.0**
     *   （上一版 1.0 ⇒ 字形只有一半大 = "得大至少 1 倍多"）。*/
    /* ★★★ 2026-09-26 第 30 轮（owner 复报「**间距依旧很宽**」）—— 上一版把 TH12 的 12 直接抄进来，
     *   忘了本表的度量会**再乘一遍 `size`**（`adv_screen = adv × size × scale × dat`，dat = 2）：
     *     · 上一版：adv 12 × size 2 × dat 2 = **48 px**，而字形自己只有 16 texel × size 2 = **32 px**
     *       ⇒ 每字之间空 16 px（半个字宽）= owner 说的"依旧很宽" ✗；
     *     · 本轮：TH12 的原值 12 px（640 空间）要落在屏幕上就是 12 × dat 2 = **24 px**
     *       ⇒ `adv = 24 / (size × dat) = 24 / 4 =` **6.0**；`.`/`,` 同 4 → **2.0**。
     *   ⚠ 行高/VM 偏移同理都要除以 (size) 这一层：`half_line` 8 → **4**、`full_line` 16 → **8**、
     *     `vm_dy` 8 → **4**（它们也是 × sy 用的）。
     *   ⇒ 结果：字形 32 px 宽、步进 24 px ⇒ **互相压 8 px**（TH12 就是 16 与 12 的关系，同一比例）。*/
    /* ★★★ 2026-09-26 第 31 轮（owner：「**times 2.0 的 0 偏左，明确地压在了 `.` 上面**」）——
     *   上一版 `adv` 6（屏显 24 px）而字形是 **32 px**，而且 `.` 只有 2（屏显 8 px）⇒ 每个字都往前压
     *   8 px、`. ` 之后那个 `0` 更是直接压在点上。
     *   本仓字形的**墨迹几乎占满整格**（16×16 的格子里 13~14 px）⇒ 步进**不能小于字格宽**：
     *     `adv × size(2) × dat(2) = 屏显步进` ⇒ 要等于字形宽 32 px，`adv` 必须 = **8.0**；
     *     `.` / `,` 也用 **8.0**（TH12 那个 4/12 的窄步进是配它自己**带留白**的小字形用的，我们的
     *     16×16 切图没有那份留白 ⇒ 照抄就会出现"点被数字压住"）。
     *   ⇒ 现在：字形 32 px、步进 32 px ⇒ 相邻字符**边贴边、不重叠**（`.` 后面那个数字也就正常落在右边）。*/
    { hud_vm_glyph_th12, 6.0f, 2.0f, CE_ANM_ABILITY_SCRIPT_FONT_TH12_GLYPH, 4.0f, 8.0f, 4.0f, 2.0f },   /* ★ 2026-09-26 第 51 轮（owner：「击破后 UI 还是间距宽」）——按一手 `AsciiManager::draw_string`
      *   `th12:0x4018f0` 的比例收紧：font 4 的数字步进 = `scale×12.0`（`0x4a4128`）、
      *   **`.` 与 `,` = `scale×4.0`**（`0x4a4108`）⇒ 相对字形格（16）是 **0.75 倍**、点/逗号是**数字的 1/3**。
      *   ⇒ `adv` 8.0 → **6.0**（0.75×）、`adv_dot` 8.0 → **2.0**（= 6.0/3）；
      *   并把 `hud_text_width/fill` 里只认 `'.'` 的窄步进**补上 `','`**（一手两者同宽）。*/
};

/* 默认字形集 = TH18 font 2（所有既有调用点走这里，行为一个字节不变）。*/
/* ★★★ 2026-10-03r（owner：「你加一下 log，因为**十二合一是测试的终极形态**」）——
 *   **HUD 台账整表**：谁认领了、块多大、在哪一行、PINNED 还是货架（降级）、带的矩形与淡化状态。
 *   118（挑战・十二合一）一次把十二张系统卡发齐 ⇒ 这份日志就是那次排布的**唯一现场**。*/
/* ★★★ 2026-10-03z15（owner 反复：「**log 和实际 HUD 的展现之间肯定有问题**」；
 *   「113 和 117 重合」「108 声明的矩形可能比 UI 本身靠左」）——
 *   **墨迹探针**：把一支 HUD VM 的**真实绘制依据**一次打全 ——
 *   · 有效落点 = 脚本 pos(+0x30) + pos2(+0x4e4) + 实体 pos(+0x5f0)（引擎就是这么算的）；
 *   · **贴图尺寸** `+0x74/+0x78`（zAnmVmPrefix.sprite_size）+ 缩放 `+0x54` + 层 `+0x18`。
 *   ⇒ 有了"落点 + 贴图尺寸×缩放"，**声明块该多大就是算出来的**，不再靠推。*/
void ce_hud_vm_probe(const char *tag, uint32_t id)
{
    const uint8_t *vm = id ? (const uint8_t *)ce_anm_get_vm(id) : 0;
    if (!vm) { ce_log("hud.vm %s id %u: (no VM)", tag ? tag : "?", (unsigned)id); return; }
    {
        const float *sp = (const float *)(vm + CE_ANM_VM_SCRIPT_POS);
        const float *p2 = (const float *)(vm + CE_ANM_VM_POS_2);
        const float *ep = (const float *)(vm + CE_ANM_VM_POS);
        const float *sz = (const float *)(vm + CE_ANM_VM_SPRITE_SIZE);
        const float *sc = (const float *)(vm + CE_ANM_VM_SCALE);
        /* ★ z22：顺带把**算出来的墨迹盒**打出来（按"贴图以 VM 位置为中心"= `anchor(1,2)` 的实测口径）
         *   ⇒ 日志里就能直接和"声明块"对比（差额一目了然）。*/
        {
            const float cx = sp[0] + p2[0] + ep[0], cy = sp[1] + p2[1] + ep[1];
            const float hw = sz[0] * sc[0] * 0.5f, hh = sz[1] * sc[1] * 0.5f;
            ce_log("hud.vm ink %s id %u ⇒ x %.0f..%.0f y %.0f..%.0f（居中锚）",
                   tag ? tag : "?", (unsigned)id,
                   (double)(cx - hw), (double)(cx + hw), (double)(cy - hh), (double)(cy + hh));
        }
        ce_log("hud.vm %s id %u script %d layer %d sprite %d size %.0fx%.0f scale %.2fx%.2f "
               "draw 表面 (%.0f,%.0f)",
               tag ? tag : "?", (unsigned)id,
               (int)*(const int32_t *)(vm + CE_ANM_VM_SCRIPT_ID),
               (int)*(const int32_t *)(vm + CE_ANM_VM_LAYER),
               (int)*(const int32_t *)(vm + CE_ANM_VM_SPRITE_ID),
               (double)sz[0], (double)sz[1], (double)sc[0], (double)sc[1],
               (double)(sp[0] + p2[0] + ep[0]), (double)(sp[1] + p2[1] + ep[1]));
    }
}

/* ★★★★ 2026-10-03z17（owner：「**你能不能把声明块画出来？**」）——
 *   **把分配器"以为的"块直接画在屏幕上**（4 条边 = 4 支 `stock_rect` VM）：这样
 *   「声明块 vs 屏幕上真正画出来的 UI」可以**一眼对照**（这一轮所有"重合/偏左/错位"都是这两者的差）。
 *
 *   画法：`assets/ability/scripts/104_stock_rect.anm.txt` = `drawRect(1.0f,1.0f)` 的 1×1 白块
 *   （layer 25 + `resolutionMode(2)` ⇒ **1 单位 = 1 表面 px**）⇒ 给 `scale` 就能画出任意矩形 ✓。
 *   颜色：`LEFT`（货架）= 绿、`PINNED`（自带锚点）= 黄、`RIGHT` = 青；半透明（A=0xB0）。
 *   ⚠ 生命周期：**固定池**（每个块 4 支，懒惰建、之后只改位置/缩放/颜色）——不 churn、
 *     块消失时把它的边**透明隐藏**（不删）⇒ 不会泄漏也不会每帧新建。
 *   ⚠ 关掉它：把下面这个宏改成 0（调试完成后一行）。*/
/* ★★★ 2026-10-03z57（owner：「**然后你可以拆框了**」）—— 声明块调试覆盖层**关掉**。
 *   它已完成使命（113 的 100+px 偏差、110/111 的墨迹溢出、108 的底部溢出都是靠它定位的 ✓）。
 *   要再用就把这里改成 1（其余代码原样保留 ✓）。*/
#ifndef CE_HUD_DEBUG_BLOCKS
#define CE_HUD_DEBUG_BLOCKS 0
#endif

#if CE_HUD_DEBUG_BLOCKS
#define CE_HUD_DBG_MAX  16                       /* 同时可视化的**块数**上限（16×4 = 64 支 VM）*/
static uint32_t s_dbg_edge[CE_HUD_DBG_MAX][4];   /* 每块 4 条边：0=上 1=下 2=左 3=右 */
static int      s_dbg_rows;                      /* 已经建好的行数（按**实际块数**增长，不预开 257 行）*/
static int32_t  s_dbg_stage = -1;               /* ★ z25：池子对应的关卡号（换关要整池重建）*/
static int32_t  s_dbg_retry[CE_HUD_DBG_MAX];    /* ★ z32：每行每 30 帧最多重建一次（不洪水）*/
static int32_t  s_dbg_last_id[CE_HUD_DBG_MAX]; /* ★ z32：每行上次打日志时的卡 id（换卡就再打一次）*/

static uint32_t hud_dbg_spawn_edge(const float *p)
{
    if (!CE_ABILITY_ANM()) return 0;
    return ce_anm_spawn_ui(CE_ABILITY_ANM(), CE_ANM_ABILITY_SCRIPT_STOCK_RECT, p);
}

void ce_hud_debug_blocks_tick(void)
{
    float bx, by, bw, bh;
    uint32_t id;
    ce_hud_slot_t sl;
    int n = ce_hud_slot_count(), i, e;

    if (n > CE_HUD_DBG_MAX) n = CE_HUD_DBG_MAX;
    /* ★★★ 2026-10-03z25（owner：「113 **依旧框画在右边**」）——**真因**：
     *   引擎**换关时会清掉 UI 列表的 VM**，而本池只在"第一帧"建过一次 ⇒
     *   旧的边 VM 句柄全成了**陈旧/失效**的 id：`ce_anm_get_vm()` 查不到、**但屏幕上仍留着上一关的框**
     *   （上一关 12 张卡时确实把块排到过右侧）⇒ 看起来就像"113 的框在右边" ✗。
     *   ⇒ 改成**按关卡重建整池**（`CE_CURRENT_STAGE()` 变化 ⇒ 全部句柄作废、重新建），
     *     并**去掉"每帧自愈重建"**（查不到就每帧新建 = VM 洪水 ✗）。*/
    if (CE_CURRENT_STAGE() != s_dbg_stage) {
        s_dbg_stage = CE_CURRENT_STAGE();
        for (i = 0; i < CE_HUD_DBG_MAX; ++i)
            for (e = 0; e < 4; ++e) s_dbg_edge[i][e] = 0;   /* 引擎已清 ⇒ 只作废句柄 */
        s_dbg_rows = 0;
    }
    if (n > s_dbg_rows) {                        /* 按**实际块数**懒惰加行（不预开 257 行）*/
        const float org[3] = { 0.0f, 0.0f, 0.0f };
        int r;
        for (r = s_dbg_rows; r < n; ++r)
            for (e = 0; e < 4; ++e) s_dbg_edge[r][e] = hud_dbg_spawn_edge(org);
        ce_log("hud: debug block overlay rows %d -> %d（4 条边/块；绿=货架 黄=PINNED 青=RIGHT）",
               s_dbg_rows, n);
        /* ★ z23：**覆盖层自探针** —— owner 报「黄框画在右侧」，而台账里 113 的块在 248..392
         *   ⇒ 先证明"我画的框到底在哪"（用同一支墨迹探针读它自己的 VM）。*/
        if (s_dbg_edge[0][0] && s_dbg_edge[0][2]) {
            ce_hud_vm_probe("dbg.edge_top", s_dbg_edge[0][0]);
            ce_hud_vm_probe("dbg.edge_left", s_dbg_edge[0][2]);
        }
        s_dbg_rows = n;
    }
    if (!s_dbg_rows) return;

    for (i = 0; i < s_dbg_rows; ++i) {
        uint32_t col;
        /* ★★★ 2026-10-03z32（owner：「**不可能被画面盖住，因为它大体是透明的**」）——
         *   那条 z30 的"被 109 自己的画面盖住"的推断**不成立**（画面透明）⇒ 真凶是**边根本就没建起来**：
         *   `ce_anm_spawn_ui` 在帧里可能失败（尤其一帧内要建很多支时）⇒ 那几行就是空的 ✗
         *   （日志里 `hud.vm dbg.edge_* …: (no VM)` 正是这个）。
         *   ⇒ 加**有节流的重试**：某支查不到就每 **30 帧**最多重试一次（补得上、又不会 VM 洪水）。*/
        for (e = 0; e < 4; ++e) {
            if (s_dbg_edge[i][e] && ce_anm_get_vm(s_dbg_edge[i][e])) continue;
            if (s_dbg_retry[i] > 0) { --s_dbg_retry[i]; continue; }
            {
                const float org[3] = { 0.0f, 0.0f, 0.0f };
                s_dbg_edge[i][e] = hud_dbg_spawn_edge(org);
                s_dbg_retry[i] = 30;
                if (s_dbg_edge[i][e])
                    ce_log("hud: debug overlay 重建 row %d 的边（边 %d）", i, e);
            }
        }
        float x, y, w, h, t = 2.0f;             /* 边宽 2 表面 px */
        if (i >= n || !ce_hud_slot_enum(i, &id, &sl)) {
            for (e = 0; e < 4; ++e)
                if (s_dbg_edge[i][e]) ce_anm_set_color(s_dbg_edge[i][e], 0x00ffffffu);
            continue;
        }
        x = sl.x; y = sl.y; w = sl.w; h = sl.h;
        col = (sl.side == CE_HUD_SIDE_PINNED) ? 0xB0FFFF00u
            : (sl.side == CE_HUD_SIDE_RIGHT)  ? 0xB000FFFFu : 0xB000FF00u;
        if (s_dbg_last_id[i] != (int32_t)id) {   /* ★ z32：每行的实际几何（换卡/首见时打一次）*/
            s_dbg_last_id[i] = (int32_t)id;
            ce_log("hud: debug overlay row %d = card %u 块 x %.0f..%.0f y %.0f..%.0f "
                   "(边 VM 上%d 下%d 左%d 右%d)", i, (unsigned)id,
                   (double)x, (double)(x + w), (double)(y - h), (double)y,
                   s_dbg_edge[i][0] ? 1 : 0, s_dbg_edge[i][1] ? 1 : 0,
                   s_dbg_edge[i][2] ? 1 : 0, s_dbg_edge[i][3] ? 1 : 0);
        }
        /* ⚠ `drawRect` 是**以 VM 位置为中心**画的（先例：116 的外框四条就传"框心"）
         *   ⇒ 四条边各自给**边心**（否则整体会偏半条边）。*/
        if (s_dbg_edge[i][0]) {                  /* 上边：中心 = (x+w/2, y−h) */
            ce_anm_set_pos(s_dbg_edge[i][0], x + w * 0.5f, y - h, 0.0f);
            ce_anm_set_scale(s_dbg_edge[i][0], w, t);
            ce_anm_set_color(s_dbg_edge[i][0], col);
        }
        if (s_dbg_edge[i][1]) {                  /* 下边：中心 = (x+w/2, y) */
            ce_anm_set_pos(s_dbg_edge[i][1], x + w * 0.5f, y, 0.0f);
            ce_anm_set_scale(s_dbg_edge[i][1], w, t);
            ce_anm_set_color(s_dbg_edge[i][1], col);
        }
        if (s_dbg_edge[i][2]) {                  /* 左边：中心 = (x, y−h/2) */
            ce_anm_set_pos(s_dbg_edge[i][2], x, y - h * 0.5f, 0.0f);
            ce_anm_set_scale(s_dbg_edge[i][2], t, h);
            ce_anm_set_color(s_dbg_edge[i][2], col);
        }
        if (s_dbg_edge[i][3]) {                  /* 右边：中心 = (x+w, y−h/2) */
            ce_anm_set_pos(s_dbg_edge[i][3], x + w, y - h * 0.5f, 0.0f);
            ce_anm_set_scale(s_dbg_edge[i][3], t, h);
            ce_anm_set_color(s_dbg_edge[i][3], col);
        }
    }
    (void)bx; (void)by; (void)bw; (void)bh;
}
#else
void ce_hud_debug_blocks_tick(void) { }
#endif

/* ★★★★ 2026-10-03z50（owner：「**并没有一起变暗，还是解耦**」）——
 *   **根因**：z49 的写被我放在了「树根变化才跑」的探针块里 ⇒ 一关**只写一次** ✗
 *   （实跑日志里 `耦合写入` **只出现 1 次**：写的时候 band alpha=232，之后再没更新 ✗）。
 *   ⇒ 本函数**每帧**跑一次，结构完全按**只读探针实测**的那棵树来（见 AUDIT #273）：
 *     · `mgr+0x3c` = **树根 id**（不是指针 ✓）⇒ `ce_anm_get_vm()` 取根；
 *     · 子链 = `zAnmVmSuffix.list_of_children`（**VM+0x594** = 0x544+0x50）的 `head.next`；
 *     · 子 VM 的有效 `color_1` 在 **prefix +0x524** ✓（实测四支 = 脚本 2/1/4/3 ✓）。
 *   ⚠ 安全铁律（#272 的教训）：**每一个指针解引用前都过 `IsBadReadPtr`**、
 *     循环上限 8 防环、只写已验证的 `color_1`、**只改 A 保留 RGB**（乘性 ⇒ 与引擎的闸只叠加）。
 *   ⚠ 日志按「band alpha 或支数变化」才打一行 ⇒ 不刷屏 ✓。*/
void ce_hud_native_face_dim_tick(void)
{
    uint8_t *mgr = CE_ABILITY_MGR();
    int32_t root;
    uint8_t *vm, *head, *node;
    uint32_t a;
    int guard = 0, nw = 0;
    static uint32_t s_wa = 0xffffffffu;
    static int s_wn = -1;

    if (!mgr) return;
    root = *(const int32_t *)(mgr + CE_MGR_SEL_CARD_VM);
    if (!root) return;
    vm = (uint8_t *)ce_anm_get_vm((uint32_t)root);
    if (!vm) return;
    /* ★★★★ 2026-10-03z57（**第二次开游戏崩溃**：`th18.exe.84392.dmp` 16:31）——
     *   日志把坑指出来了：`IsBadReadPtr()` **只证明"可读"，不能证明"这是个 VM"** ✗
     *   （实跑：`mgr+0x3c=9764778` 被当成指针，读出 layer=283705345 / script_id=226164736 全是垃圾，
     *     却一路"可读"⇒ 我**往只读页/随机内存写** ⇒ 访问违例 ✓）。
     *   ⇒ 两道硬校验（缺一不可）：
     *     ① **身份**：该 VM 的 `id`（`suffix+0x544`）必须**等于**我们要找的 `root` ✓
     *        （实测好状态下两者相等 ✓；不等就是垃圾 ⇒ 立刻退出 ✓）；
     *     ② **写**权限：写之前用 **`IsBadWritePtr`**（不是 ReadPtr ✗）✓。
     *   ⚠ 只读探针那一版之所以安全，是因为它**不写**；写版本必须多这两道 ✓。*/
    if (IsBadReadPtr(vm + CE_ANM_VM_SUFFIX_ID, 4)) return;
    if (*(const int32_t *)(vm + CE_ANM_VM_SUFFIX_ID) != root) return;
    if (IsBadReadPtr(vm + CE_ANM_VM_CHILD_LIST, 12)) return;
    head = vm + CE_ANM_VM_CHILD_LIST;
    node = *(uint8_t *const *)(head + 0x4);
    a = (uint32_t)ce_hud_band_dim_alpha() & 0xffu;
    while (node && node != head && guard++ < 8) {
        uint8_t *ch;
        if (IsBadReadPtr(node, 12)) break;
        ch = *(uint8_t *const *)node;
        if (ch && !IsBadReadPtr(ch, CE_ANM_VM_ALPHA2_I + CE_ANM_VM_INTERP_CURRENT + 4) &&
            !IsBadWritePtr(ch + CE_ANM_VM_COLOR1, 8) &&
            !IsBadWritePtr(ch + CE_ANM_VM_ALPHA1_I, 0x20) &&
            !IsBadWritePtr(ch + CE_ANM_VM_ALPHA2_I, 0x20)) {
            const uint32_t old = *(const uint32_t *)(ch + CE_ANM_VM_COLOR1);
            const uint32_t old2 = *(const uint32_t *)(ch + CE_ANM_VM_COLOR2);
            int32_t *i1, *i2;
            *(uint32_t *)(ch + CE_ANM_VM_COLOR1) = (a << 24) | (old & 0x00ffffffu);
            *(uint32_t *)(ch + CE_ANM_VM_COLOR2) = (a << 24) | (old2 & 0x00ffffffu);
            /* ★ z51：**连插值器一起写**（`initial/goal/current` 三处）——否则引擎每帧用
             *   它算出的 `current` 覆盖颜色 A ⇒ 我们和引擎**交替胜出 = 闪烁** ✓（owner 实跑）。*/
            i1 = (int32_t *)(ch + CE_ANM_VM_ALPHA1_I);
            i2 = (int32_t *)(ch + CE_ANM_VM_ALPHA2_I);
            i1[CE_ANM_VM_INTERP_INITIAL / 4] = (int32_t)a;
            i1[CE_ANM_VM_INTERP_GOAL / 4]    = (int32_t)a;
            i1[CE_ANM_VM_INTERP_CURRENT / 4] = (int32_t)a;
            i2[CE_ANM_VM_INTERP_INITIAL / 4] = (int32_t)a;
            i2[CE_ANM_VM_INTERP_GOAL / 4]    = (int32_t)a;
            i2[CE_ANM_VM_INTERP_CURRENT / 4] = (int32_t)a;
            ++nw;
        }
        node = *(uint8_t *const *)(node + 0x4);
    }
    if (s_wa != a || s_wn != nw) {
        s_wa = a; s_wn = nw;
        ce_log("hud: native face 耦合(每帧) 写 %d 支 band alpha=%u", nw, (unsigned)a);
    }
}

/* ★★★★ 2026-10-03z58（owner 第 1 步：**只加 API + 只读探针，不动任何卡的判据**）——
 *   **把"扫描全部敌弹"这件事抽离 + 加速**的验证件。抽离的**只是"怎么走"**：
 *   判据（半径 <10 / 自机身边 / 只染某色 …）**仍然各卡自己写** ✓（owner 口径）。
 *
 *   两条路：
 *     · **活弹链表**：`mgr+0xbc`（`tick_list_head`）的 `head.next` 起走，
 *       `node.entry` = `zBullet*` ⇒ **只摸活弹** ✓（O(活弹)）；
 *     · **数组扫描**：`mgr+CE_BM_BULLETS + i*CE_BULLET_STRIDE`（i = 1..2000）⇒ 现在的写法
 *       （**2000 个分散地址、每个 4KB 步长** ✗）。
 *   ⇒ 本函数**只读**、把两者的活弹数**对上账**，并把**状态直方图**打出来（第 2 步要的一手数据）：
 *     状态直方图正是"什么算有效弹"的原始素材，各卡判据可以据此写对 ✓。
 *   ⚠ 安全：每一步 `IsBadReadPtr`、上限 4096 防环、**不写任何内存** ✓（#272/#278 的教训）。*/
int ce_ptr_readable(const void *p, unsigned n)
{
    return p && !IsBadReadPtr(p, n);
}

void ce_bullet_scan_probe(void)
{
    static unsigned s_tick;
    uint8_t *mgr;
    uint8_t *head, *node;
    int n_list = 0, guard = 0, bad = 0, i;
    unsigned hist[8];
    int idx_first[4];
    int sp_idx[6], sp_state[6], sp_arr[6], i_sp;
    unsigned sp_flags[6], sp_vm[6];
    int n_sp = 0;
    if (++s_tick % 60u) return;                     /* 一秒一次（数组扫描贵：2000×4KB 步长）*/
    mgr = CE_BULLET_MGR();
    if (!mgr) return;
    for (i = 0; i < 8; ++i) hist[i] = 0;
    for (i = 0; i < 4; ++i) idx_first[i] = -1;

    /* ① 活弹链表（快路）*/
    head = mgr + CE_BM_TICK_LIST_HEAD;
    if (!IsBadReadPtr(head, 16)) {
        node = *(uint8_t *const *)(head + CE_BULLET_LIST_NEXT);
        while (node && node != head && guard++ < 4096) {
            uint8_t *b;
            if (IsBadReadPtr(node, 16)) { bad = 1; break; }
            b = *(uint8_t *const *)node;
            if (!b || IsBadReadPtr(b, CE_BULLET_STATE + 2)) { bad = 1; break; }
            if (n_list < 4) idx_first[n_list] = *(const int32_t *)(b + CE_BULLET_INDEX_IN_ARRAY);
            ++n_list;
            node = *(uint8_t *const *)(node + CE_BULLET_LIST_NEXT);
        }
    } else {
        bad = 1;
    }

    /* ② 数组扫描（老路）+ 状态直方图（第 2 步的一手素材）*/
    for (i = 1; i <= CE_BM_BULLET_COUNT; ++i) {
        const uint8_t *b = mgr + CE_BM_BULLETS + (size_t)i * CE_BULLET_STRIDE;
        const unsigned st = *(const uint16_t *)(b + CE_BULLET_STATE);
        if (st < 8) ++hist[st];
        /* ★ z59（第 2 步）：**非 0/1 的槽**（疑似哨兵）逐个打明细 —— 下标、flags(+0x20)、
         *   index_in_bullet_array(+0x660)、以及它的 VM id：哨兵应当是**位置固定、内容恒定**的假弹 ✓。*/
        if (st != 0 && st != 1 && n_sp < 6) {
            sp_idx[n_sp] = i;
            sp_state[n_sp] = st;
            sp_flags[n_sp] = *(const uint32_t *)(b + 0x20);
            sp_arr[n_sp] = *(const int32_t *)(b + CE_BULLET_INDEX_IN_ARRAY);
            sp_vm[n_sp] = *(const uint32_t *)(b + 0x28);
            ++n_sp;
        }
    }
    if (n_sp) {
        for (i_sp = 0; i_sp < n_sp; ++i_sp)
            ce_log("bullet: 非 0/1 槽：下标 %d state=%d flags=0x%x index_in_array=%d vm=0x%x",
                   sp_idx[i_sp], sp_state[i_sp], sp_flags[i_sp], sp_arr[i_sp], sp_vm[i_sp]);
    }
    (void)i_sp;
    ce_log("bullet: 活弹链表 %d 发%s vs 数组非零槽 %u 发 | 状态直方图 "
           "[0]=%u [1]=%u [2]=%u [3]=%u [4]=%u [5]=%u [6]=%u [7]=%u | 链表前 4 发的数组下标 %d %d %d %d",
           n_list, bad ? "（走到坏指针就停 ✗）" : "",
           (unsigned)(hist[1] + hist[2] + hist[3] + hist[4] + hist[5] + hist[6] + hist[7]),
           (unsigned)hist[0], (unsigned)hist[1], (unsigned)hist[2], (unsigned)hist[3],
           (unsigned)hist[4], (unsigned)hist[5], (unsigned)hist[6], (unsigned)hist[7],
           idx_first[0], idx_first[1], idx_first[2], idx_first[3]);
}

void ce_hud_slot_log_all(const char *tag)
{
    int i, n = ce_hud_slot_count();
    float bx, by, bw, bh;
    const int card_face = hud_card_present(*(void *const *)(CE_ABILITY_MGR() + CE_MGR_SELECTED_ACTIVE));
    ce_log("hud: ===== %s —— %d slot(s) claimed  (native active-card face %s) =====",
           tag ? tag : "hud", n, card_face ? "PRESENT @68..116" : "absent");
    for (i = 0; i < n; ++i) {
        uint32_t id = 0;
        ce_hud_slot_t sl;
        if (!ce_hud_slot_enum(i, &id, &sl)) break;
        ce_log("hud:   #%-2d card %-3u %-6s surface (%.0f,%.0f) %.0fx%.0f  =>  x %.0f..%.0f  y %.0f..%.0f",
               i, (unsigned)id,
               sl.side == CE_HUD_SIDE_PINNED ? "PINNED" :
               sl.side == CE_HUD_SIDE_RIGHT  ? "RIGHT" : "LEFT",
               (double)sl.x, (double)sl.y, (double)sl.w, (double)sl.h,
               (double)sl.x, (double)(sl.x + sl.w),
               (double)(sl.y - sl.h), (double)sl.y);
    }
    ce_hud_band_rect(&bx, &by, &bw, &bh);
    ce_log("hud:   band x %.0f..%.0f  y %.0f..%.0f  (%.0fx%.0f)  dim=%d alpha=%d  fallbacks=%d",
           (double)bx, (double)(bx + bw), (double)(by - bh), (double)by, (double)bw, (double)bh,
           ce_hud_band_dim_dimmed(), (int)ce_hud_band_dim_alpha(), ce_hud_slot_fallbacks());
    /* ★★★ 2026-10-03z11（owner：「我感觉你的 log 和实际 HUD 的展现之间肯定有问题」）——
     *   **坐标空间探针**：本仓 HUD 的一切常量都按 **1280×960 设计空间**写（`resolutionMode(2)`
     *   的 VM 实际缩放 = `ui_scale × 0.5`；1280 时 ui_scale = 2.0 ⇒ 1 单位 = 1 表面 px ✓）。
     *   若这台机器的 `ui_scale` ≠ 2.0（游戏表面可以是 640/960/1280 三档），
     *   那"日志说的表面 px"与"屏幕上看到的位置"就会**整体差一个比例** ⇒ 先量出来再谈排布。*/
    {
        const float ui = *(const float *)CE_ADDR_UI_SCALE;
        /* BMP 尺寸 = **表面尺寸**（`engine/anm/th18/05-snapshot-and-surfaces.md:105`）*/
        const int32_t sw = *(const volatile int32_t *)0x56ac80;
        const int32_t sh = *(const volatile int32_t *)0x56ac84;
        ce_log("hud: coord-space ui_scale=%.3f (2.000 = 1280x960 设计空间 ✓) surface=%dx%d "
               "(三档 640/960/1280) ⇒ 本仓常量按 1280 写",
               (double)ui, (int)sw, (int)sh);
    }
    ce_log("hud: ===== end =====");
}

void ce_hud_vm_text_set(ce_hud_vm_text_t *t, float x, float y, uint32_t bgra,
                        int align_h, float scale, const char *s, const char *tag)
{
    ce_hud_vm_text_set_font(t, x, y, bgra, align_h, scale, s, tag, CE_HUD_FONT_TH18_FONT2);
}

void ce_hud_vm_text_clear(ce_hud_vm_text_t *t)
{
    int i;
    if (!t) return;
    for (i = 0; i < CE_HUD_VM_TEXT_MAX; ++i) { ce_anm_delete(t->id[i]); t->id[i] = 0; t->shown[i] = 0; }
    t->n = 0;
}

void ce_hud_vm_text_set_font(ce_hud_vm_text_t *t, float x, float y, uint32_t bgra,
                             int align_h, float scale, const char *s, const char *tag, int font)
{
    const hud_font_t *f = (font == CE_HUD_FONT_TH13_BIG)      ? &HUD_FONTS[1]
                        : (font == CE_HUD_FONT_TH18_FONT2B)   ? &HUD_FONTS[2]
                        : (font == CE_HUD_FONT_TH12_DIGITS)   ? &HUD_FONTS[3]
                                                              : &HUD_FONTS[0];
    const char *p;
    uint8_t *mgr = *(uint8_t **)CE_ADDR_ASCII_MANAGER_PTR;
    void *anm = CE_ABILITY_ANM();
    float dat = hud_vm_ui_scale(), sx = 1.0f, sy = 1.0f, total = 0.0f, pen, ytop;
    int slot = 0, i;

    if (!t || !anm || !s) return;

    /* 字距/行高都乘 ASCII 环境的缩放（`ce_hud_text_ex` 的 scale 参数同款语义：0 = 用环境值）*/
    if (scale > 0.0f) {
        sx = sy = scale;
    } else if (mgr) {
        sx = *(float *)(mgr + CE_ASCII_SCALE);
        sy = *(float *)(mgr + CE_ASCII_SCALE + 4);
        if (!(sx > 0.01f && sx < 8.0f)) sx = 1.0f;
        if (!(sy > 0.01f && sy < 8.0f)) sy = 1.0f;
    }

    /* ★ 2026-09-15：把字体的整体尺寸折进 sx/sy —— 之后所有度量（总宽/步进/对上沿的偏移）
     *   都跟着它走，不用逐个字段乘。原有字形集 size = 1.0 ⇒ 既有调用点不变。*/
    sx *= f->size;
    sy *= f->size;

    /* ① 先量总宽（`.` 的字距是 4 而不是 7 —— 引擎的 `fVar11` 就是被它改掉的）*/
    for (p = s; *p; ++p) total += ((*p == '.' || *p == ',') ? f->adv_dot : f->adv) * sx * dat;   /* ★ 第 51 轮：逗号也走窄步进（一手同宽）*/
    pen = x * dat + ((align_h == CE_ASCII_ALIGN_CENTER) ? -total * 0.5f
                   : (align_h == CE_ASCII_ALIGN_RIGHT)  ? -total
                                                        : 0.0f);

    /* ② 字形墨迹上沿（= 引擎的 VM pos.y）：`record+0x134` 三个分支 ——
     *    0 居中（退半个行高 = 5×scale_y）、2 底（退一个行高 = 10×scale_y）、1 顶（不退）。*/
    ytop = y * dat;
    if (mgr) {
        int av = *(int *)(mgr + CE_ASCII_ALIGN_V);
        if (av == 0) ytop -= f->half_line * sy * dat;
        else if (av == 2) ytop -= f->full_line * sy * dat;
    }

    /* ③ 逐字摆：VM 位置 = (笔 x, 墨迹上沿 + 5×scale_y×DAT)。
     *    +5 来自贴图：16×32 的 entry 里 20 行字形上下各补 6 行，精灵中心 = 字形中心往下 1.5 行，
     *    折到「上沿 + 半个字形高」就是 +5（详见脚本 136 与 make_font2_art.py 的注释）。*/
    for (p = s; *p; ++p) {
        uint32_t spr = f->glyph(*p);
        float adv = ((*p == '.' || *p == ',') ? f->adv_dot : f->adv) * sx * dat;   /* ★ 第 51 轮：逗号同窄步进（一手 `0x4a4108` 两者同宽）*/

        if (spr && slot < CE_HUD_VM_TEXT_MAX) {
            uint32_t *id = &t->id[slot];
            float pos[3];
            /* ★ 第 52 轮（owner：「`.` 得往左移动，其他字符不要移动」）：**只把点这一格往左挪**
             *   （贴图里点的墨迹居中，字格又宽 ⇒ 看着离前一个数字太远）。步进 `adv` 不动 ⇒
             *   后面的字符位置一个都不变。挪动量 = 1.5 × `adv_dot` 的屏幕量（≈12 px @1280）。*/
            pos[0] = pen - ((*p == '.') ? f->adv_dot * 1.0f * sx * dat : 0.0f);   /* ★ 第 54 轮：1.5 → 1.0（owner：「有点过左了」）*/
            pos[1] = ytop + f->vm_dy * sy * dat; pos[2] = 0.0f;
            if (*id && !ce_anm_get_vm(*id)) { *id = 0; t->shown[slot] = 0; }
            if (!*id) {
                *id = ce_anm_spawn_ui(anm, f->script, pos);
                t->shown[slot] = 0;
                if (!*id) { ce_log("%s: hud glyph %d FAILED to spawn", tag, slot); return; }
            } else {
                ce_anm_set_pos(*id, pos[0], pos[1], 0.0f);
            }
            if (t->shown[slot] != spr) {
                if (ce_anm_set_sprite(anm, *id, spr)) t->shown[slot] = spr;
            }
            ce_anm_set_color(*id, bgra);
            ce_anm_set_scale(*id, sx, sy);
            ++slot;
        }
        pen += adv;
    }

    /* ③ 串变短了 → 多出来的 VM 收掉 */
    for (i = slot; i < t->n && i < CE_HUD_VM_TEXT_MAX; ++i) {
        ce_anm_delete(t->id[i]);
        t->id[i] = 0;
        t->shown[i] = 0;
    }
    t->n = slot;
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

/* ---- Q16.16：把引擎里的 float 位模式搬成定点（**不产生 x87**）----
 * `dllx87` 门的三个来源里有「float ↔ int 转换」，所以这里手搓 IEEE 754 拆位：
 *   · |x| < 1 ⇒ 取 `1.m × 2^(e-127)`，右移 `16-(e-127)` 位后四舍五入到 1/65536；
 *   · 负数（游戏里的 y 可能有）⇒ 取绝对值再变号。
 * 用途只有一个：`ce_item_score_in_common` 判「自机在回收线线上还是线下」这个**诊断位**
 * （★ 2026-09-13 订正：读的是**自机**的 y，不是道具的高度 —— 引擎两条例程都是
 *  `xmm2/xmm0 = [PLAYER+0x624]`、回收线 = `[PLAYER+0x47998]`，一手见 §`BP_ce_item_score_in`）。
 * 精度 1/65536 px，远小于引擎自己 `cvttss2si` 的整数截断误差。*/
static int32_t ce_q16_from_bits(uint32_t bits)
{
    uint32_t sign = bits >> 31, exp = (bits >> 23) & 0xffu, frac = bits & 0x7fffffu;
    uint32_t mant;
    int e;

    if (exp == 0) return 0;                       /* 0 / 非规格化：取 0（坐标不会是这种值）*/
    mant = frac | 0x800000u;                      /* 1.m */
    e = (int)exp - 127;
    if (e >= 16) {
        int32_t r = (int32_t)(mant << (e - 16));
        return sign ? -r : r;
    }
    if (e >= -16) {
        uint32_t rem = mant & ((1u << (16 - e)) - 1u);
        uint32_t r = mant >> (16 - e);
        if (rem >= (1u << (15 - e))) ++r;         /* 四舍五入 */
        return sign ? -(int32_t)r : (int32_t)r;
    }
    return 0;                                     /* 太小 ⇒ 0 */
}

/* ★★ 2026-09-13 第二次修订：**落笔点从「出口」搬到「入口」**（推导 / 一手证据 / 单位表见
 *   ../SCORE-LAYER-REWORK.md §2 / §4）。
 *
 * 旧写法（本次改掉）挂在 `0x446cf6` / `0x446caa` —— 那两条都在**引擎算完身价之后**
 * （`esi` = 引擎自己的最终值 = 10 × 这一颗的 SCORE 增量，满额支），handler 却把 `regs->esi`
 * 一写 ⇒ 引擎刚做的高度系数**整个被我们丢掉**：实际结构 = `coeff × G_0 + ΣG_i`，
 * 而用户规格是 `coeff × (G_0 + ΣG_i)`。
 *
 * （老注释保留在这里备查：`0x446cf6` = 「自机在回收线上方」或「道具 state == 3（真空飞行）」那一支；
 *   `0x446caa` = **线下且非真空飞行**那一支，`0x446cb2 jmp 0x446cfc` 绕过前者；两支互斥。
 *   现在这一对身份是**纯观察者**，见下面的 `ce_item_score_out_common`。）
 *
 * 新站点 `0x446bf9`（= sites.py 的 `ce_item_score_in`，8 字节 `movss xmm0,[esi+0x47998]`）是**单一汇合点**：
 *   0x446b0c  esi = PLAYER（[0x4cf410]）            ← 这一段全在用 esi 当自机
 *   0x446b13  edi = ITEM（= ecx 参数，另存 [ebp-8]）
 *   0x446b18  xmm2 = [PLAYER+0x624]                 ← ★ **自机的 y**（不是道具的高度！见 §2 订正）
 *   0x446b20–0x446bf1  三条难度支算身价并钳进 [0x4ccd24]（`ja` 那条 0x446bf3 又读回来）
 *   ---> 我们的站点：**ecx = 钳位后的身价**（恒等于 [0x4ccd24]，站点自检用它）
 *   0x446bf9  xmm0 = [PLAYER+0x47998]（回收线）
 *   0x446c01  comiss xmm0,xmm2 / jae 0x446cb4 ⇒ 自机在线上（或 0x446c0a 道具 state==3 真空飞行）
 *             ⇒ 满额支：esi = 10 × floor(ecx/1000)，弹窗用它、`SCORE += esi/10`
 *   0x446c17  线下支：同一条整数链再乘一个高度折扣（🟡 形状未钉死，我们不复制）
 *
 * 单位链（一手，逐条在 exe 上核过）：
 *   ecx（1/1000 点） --满额支--> esi（弹窗值）= 10 × floor(ecx/1000) --`mul esi`+`shr edx,3`--> SCORE += floor(ecx/1000)
 *   我们写 `ecx = 1000 × G` ⇒ 引擎精确给 G 分；且 ecx 是 1000 的倍数 ⇒ 引擎那步「量化到 10 的倍数」是恒等，
 *   不引入任何舍入损失。
 *   ⇒ 桥给层的**身价** = `ecx / 100`（= (ecx/1000) 点 × 10 = 引擎的弹窗值）⇒ `ce_score_base_points()` == ecx/1000。
 *
 * 无卡声明时**一个字都不写**（连 `regs->ecx` 都不碰）⇒ 与原生逐字节相同（原生钳位、原生系数、
 * 原生弹窗值全保留）。*/
#define CE_ITEM_SCORE_BASE_UNIT  1000u   /* 站点处 ecx 的单位：1 点 = 1000（一手 0x446cb4 起的 ÷100、÷10 链）*/

static unsigned s_score_in_n;            /* 本次进程里到过本站点的次数（日志限流用）*/
static int      s_score_in_log;          /* 这一颗要不要打日志（in/out 两行配对）*/
static int      s_score_in_mismatch;     /* 层的 G_0 与引擎基础不等：只报一次 */
static int      s_score_in_sat;          /* ecx 溢出饱和：只报一次 */
static int      s_score_fold_n;          /* ★ 折额兜底生效过几次（首次打一行诊断；正常应当恒为 0）*/
static int32_t  s_score_in_total;        /* 这一颗我们真正落下去的 G（点；与 `regs->ecx` 同源）*/

/* ★★ 2026-09-16zl：**站点命中轨迹**（头号崩溃的下一把刀）。
 * 每次命中的站点与上一次不同就打一行 —— `ce_log` 每行都 open/write/close（必 flush），
 * 所以**崩溃前最后一行 = 直接点名是哪条站点命中之后跳错的**，不用再靠时序夹逼。
 * 只读、零内存写；命中的站点名是编译期字符串常量 ⇒ 指针比较就够。*/
static const char *s_site_last;
static unsigned  s_site_hits;
static void ce_site_trail(const char *name)
{
    ++s_site_hits;
    if (s_site_last == name) return;
    s_site_last = name;
    ce_log("site: %s（累计第 %u 次命中）", name, s_site_hits);
}

/* ★★ 站点自检门（2026-09-18）：把「一手不变量」在**运行期**查一遍，对不上就记一行（每个调用点只记一次）。
 * 它是**证据门**，不是拦截器：默认不改任何行为（少数几处另加 fail-safe，见各自注释）。
 * 为什么要有：§BE-DV（muzzle 的 esp 契约）与 §BE-DW（窗口里那条 ja）两次事故的共同点是
 *   「我们以为的站点语义/契约已经变了」，而代码静默跑了几周 ⇒ 之后任何一次怪现象，日志里先有现场。*/
void ce_assume_fail(const char *what)
{
    static const char *s_seen[32];
    static unsigned s_n;
    unsigned i;
    for (i = 0; i < s_n; ++i) if (s_seen[i] == what) return;
    if (s_n < 32u) {
        s_seen[s_n++] = what;
        ce_log("★ assume FAIL: %s（站点自检不成立；只记一次、行为不变）", what);
    }
}

static int ce_addr_exec(uint32_t a);              /* 定义在后面（`VirtualQuery` 判页保护）*/

/* ★★ 2026-09-17/18（本次会话）：`regs->retaddr` 的**唯一**写入口。
 * 一手依据 = `engine/_shared/thcrap-breakpoint-pools.md` §2/§8（thcrap 2024-11-06 源码 + 本机桩字节）：
 *   · 桩**直接** `call BP_*`，返回非 0 时才自己把 cave 写进 `[ebp+0x24]`；
 *     **没有 `breakpoint_process`、没有 `esp_diff`** ⇒ 改 `regs->esp` 没人消费（要弹栈参只能用声明里的
 *     `stack_clear_size`，走 `s` 变体的 `ret imm16`，且 `imm = 4 + 函数自身清理量`）；
 *   · `regs->retaddr` 就是桩 `ret` 的落点 ⇒ **写错一次 = 控制流跳到别处**；本仓头号崩溃就是
 *     「跳进 thcrap 池页没写过的那一段」（`00 00` + `eax=1` ⇒ 写 `0x1` 的访问违例）。
 *
 * 所以所有落点都过这一道门，**两条判据缺一不可**：
 *   ① 页是「已提交且可执行」（`ce_addr_exec`）；
 *   ② ★ **页必须是模块映像（`MEM_IMAGE`）** —— thcrap 给断点分的那两页池子是 `VirtualAlloc` 出来的
 *      `MEM_PRIVATE` + `PAGE_EXECUTE`，**可执行但绝不该成为落点**。只查 ① 挡不住头号崩溃那一族
 *      （池地址本身是可执行的✓），加上 ② 才挡得住：真有人把池地址当落点时，这里记一行并**保持原返回地址**。
 *      合法落点都是 exe/DLL 里的地址（我们 6 个常量 + `Player__shoot_one_bullet` 的调用者返回地址）
 *      ⇒ 都是 `MEM_IMAGE`✓。命中时只读判定，不写任何内存。*/
static void ce_retaddr_set(x86_reg_t *regs, uint32_t target)
{
    MEMORY_BASIC_INFORMATION mbi;
    int image_exec = 0;
    if (regs && target && ce_addr_exec(target)
        && VirtualQuery((const void *)(uintptr_t)target, &mbi, sizeof mbi) == sizeof mbi
        && mbi.Type == MEM_IMAGE) {
        image_exec = 1;
        regs->retaddr = target;
    }
    if (image_exec) return;
    {
        static unsigned s_bad_n;
        if (s_bad_n < 8u) {
            ++s_bad_n;
            ce_log("★ retaddr 拒绝：0x%08x 不是「模块映像内的可执行页」（第 %u 次）—— 保持原返回地址"
                   "（池页/MEM_PRIVATE 一律拒收：那是 thcrap 断点池，跳进去就是头号崩溃）",
                   (unsigned)target, s_bad_n);
        }
    }
}


static int ce_item_score_in_common(x86_reg_t *regs)
{
    uint8_t *mgr = CE_ABILITY_MGR();
    uint8_t *item = (uint8_t *)regs->edi;          /* 道具对象（0x446b13 mov edi,ecx）*/
    uint8_t *pl = CE_PLAYER();
    uint32_t base = regs->ecx;                     /* 引擎钳位后的身价，单位 1/1000 点 */
    int32_t unused = 0;                            /* 旧签名要求一个 int32_t*；卡不再读 / 写它 */
    int32_t g0, lay, total;
    unsigned guard = 0;

    s_score_in_log = 0;
    if (!mgr) return BP_EXEC_ORIGINAL;

    /* ★ 站点自检（一手不变量）：三条难度支都在本站点**之前**把钳位结果写回 [0x4ccd24]，
     * 而 `ja` 那条更是直接把它读进 ecx ⇒ 到这里 `ecx` 必然 == [0x4ccd24]。
     * 对不上 = 我们对站点的假设过时了 ⇒ 保守：一个字都不写（退回原生行为）。*/
    if (base != *(volatile uint32_t *)CE_ADDR_MONEY_BASE) return BP_EXEC_ORIGINAL;

    /* ① 桥：把**引擎钳位后**的身价换成层要的单位（身价 = 点 × 10）交给层。
     *   `ecx / 1000 × 10 == ecx / 100`（两级整除与一级等价，见 §2 单位表）：ecx 是 1000 的倍数，
     *   所以这里没有任何截断。难度一并给层（引擎自己已经钳过界，层那次钳只是兜底）。*/
    ce_score_bridge(base / (CE_ITEM_SCORE_BASE_UNIT / CE_SCORE_ENGINE_UNIT),
                    *(volatile int32_t *)CE_ADDR_DIFFICULTY);

    /* ①b ★ 擦弹 ⇒ 最大得点（`ΔG_擦弹`）的输入：**普通内存读**，不是断点 ⇒ 不新增站点。
     *   一手：th12–th17 四作同形「每 +1 擦弹 ⇒ 最大得点 +1 显示」，所以这一项 = 本局擦弹数；
     *   层按「状态的函数」处理 ⇒ 多卡同装也只算一次（用户口径：别加一张卡擦弹数的作用就 +1 次）。
     *   `CE_ADDR_GRAZE` 见 engine.h（与耦合通道读的是同一个全局，`0x4ccd0c`）。*/
    ce_graze_bridge((uint32_t)*(volatile int32_t *)CE_ADDR_GRAZE);

    /* ② coeff_06（高度门）**只判不打折**：引擎自己在 0x446bf9–0x446cf3 把它乘在（我们写进去的）ecx 上，
     * 所以它现在作用在**整笔** `G_0 + ΣG_i` 上 —— 这正是用户规格要的结构。
     * 我们只把「这一颗是不是线上 / 真空飞行」这个**已确认的判据**记进诊断字段
     * （线下那条折扣曲线 🟡 未钉死，见 sdk_core.h 与 SCORE-LAYER-REWORK.md §4）。
     * ★ 判据的两个量都从**自机**取（pl+0x624 与 pl+0x47998）—— 2026-09-13 订正：旧代码读的是道具自己的
     *   `+0xc2c`，那是错的（引擎读的是 PLAYER+0x624）。
     * ★ 2026-09-15：回收线改读**共享件的访问器** `ce_poc_height()`（PoC 高度层；`sdk.h`）——
     *   本层落笔之后它与字段同值，但访问器是唯一真值来源（别在这里再直读那个偏移）。
     * ★★ 2026-09-20（106 的「遮」）：**改读 `ce_poc_height_semantic()`**（= 不含「遮」的生效值）——
     *   本层的 `coeff_0` 必须与「没有 106」时一致；而引擎套在 `ecx` 上的那个系数本来也是语义系数
     *   （落笔值 = `min(语义高度, 自机 y)` 把语义几何原样喂给了引擎，见 `sdk_core.h` 那一节）。
     * 两个 float 用整数搬运：本文件在 i686 上编，x87 门不许引入 float↔int 转换。*/
    if (pl && item) {
        int32_t y_q = ce_q16_from_bits(*(volatile uint32_t *)(pl + CE_PLAYER_Y));
        int32_t poc_q = ce_q16_from_bits(ce_f32_bits(ce_poc_height_semantic()));
        int above = (y_q >= poc_q)
                 || (*(volatile int32_t *)(item + CE_ITEM_STATE) == CE_ITEM_STATE_VACUUM);
        ce_score_set_coeff(above ? 1000u : 0u);        /* 0 = 线下（引擎会打折；值我们不猜）*/
        /* ★ 2026-09-18（计分层换版，路线 A）：把**几何**交给层 —— 层据此自算 `coeff_0`（整数、
         *   `900·(450−d)/450`）并按「每项走自己原作的曲线」归一（`ΔG_i × coeff_i/coeff_0`）。
         *   两个 Q16 是本段上面已经算好的（整数搬运，无浮点）。*/
        /* 道具自己的 y 也要给层：th06/th07/th08/th10 那几族的曲线吃**道具 y**（一手口径见 sdk_core.h）。*/
        ce_score_bridge_geom2(y_q,
                              ce_q16_from_bits(*(volatile uint32_t *)(item + CE_ITEM_POS + 4)),
                              poc_q);
    } else {
        ce_score_set_coeff(1000u);
        ce_score_bridge_geom(0, 0);                    /* 没有自机 ⇒ 当成线上（满额），与上面一致 */
    }

    /* ③ 收集声明（卡只声明自己那一份 G_i）。**每次收集只在这里收一次**
     * （老的两个出口站点已改成观察者，不再调卡回调 ⇒ 不会双计）。*/
    for (const uint8_t *node = *(const uint8_t *const *)(mgr + CE_MGR_CARD_LIST_FIRST);
         node && guard < 256;
         node = *(const uint8_t *const *)(node + CE_NODE_NEXT), ++guard) {
        uint8_t *card = *(uint8_t *const *)(node + CE_NODE_CARD);
        if (!card) continue;
        const ce_hooks_t *h = hooks_of(card);
        if (h && h->on_item_score) {
            h->on_item_score((ce_card_t *)card, &unused);
            ce_sdk_trace(ce_card_id(card), 0x54, "on_item_score");
        }
    }

    /* ④ 落笔：`ecx = 1000 × G_total`。引擎随后自己乘系数、弹窗、`SCORE += floor(ecx/1000)`。守两条：
     *   · **没有卡声明就一个字都不写** ⇒ 原生路径逐字节不变（原生上下限与原生弹窗值都保留）；
     *   · 层的 G_0 与引擎基础（base/1000）不一致时**以引擎的为准** —— 层那次钳是给界外桥值兜底的，
     *     万一它漂了也不该把原生身价带偏（只报一次日志）。*/
    g0  = (int32_t)(base / CE_ITEM_SCORE_BASE_UNIT);
    lay = ce_score_base_points();
    if (lay != g0) {
        total = g0 + (ce_score_total() - lay);
        if (!s_score_in_mismatch) {
            s_score_in_mismatch = 1;
            ce_log("score-in: layer G0=%d != engine base=%d (using the engine base; layer guard clamp drifted?)",
                   (int)lay, (int)g0);
        }
    } else {
        total = ce_score_total();
    }
    if (ce_score_participants() > 0) {
        int64_t want = (int64_t)total * (int64_t)CE_ITEM_SCORE_BASE_UNIT;
        /* ★★ 2026-09-20「折额兜底」：引擎在**本站点之后**（`0x446c01`–`0x446cf3`）用
         *   `d = ⌊自机 y − 字段⌋` 算 `0.9·(1−d/450)` 再乘到我们刚写进去的 `ecx` 上（一手逐指令）。
         *   正常情形下这不是问题：字段就是 PoC 层「遮」的落笔值 `min(语义高度, 自机 y)`
         *   ⇒ 那个 `d` 与「没有 106」时**同一个数** ⇒ 倍率 = 1000（一个字都不动，见 `sdk_core.h`）。
         *   这里留一道兜底：万一字段被第三方改过 / 帧内时序变过 ⇒ 按**语义高度**把差额补回来
         *   （倍率夹在 [1, 4000]，越界就不动 —— 宁可少给也不放大 32 倍）。钳位限制见 `sdk_core.h`。*/
        {
            const uint32_t poc_field_bits = pl ? *(volatile uint32_t *)(pl + CE_PLAYER_POC_HEIGHT) : 0u;
            const int32_t  y_q   = pl ? ce_q16_from_bits(*(volatile uint32_t *)(pl + CE_PLAYER_Y)) : 0;
            const int32_t  pf_q  = pl ? ce_q16_from_bits(poc_field_bits) : 0;
            const int32_t  ps_q  = pl ? ce_q16_from_bits(ce_f32_bits(ce_poc_height_semantic())) : 0;
            const int vacuum = (item && *(volatile int32_t *)(item + CE_ITEM_STATE) == CE_ITEM_STATE_VACUUM) ? 1 : 0;
            const int ratio  = pl ? ce_poc_fold_ratio_permille(y_q, pf_q, ps_q, vacuum) : 1000;

            if (ratio != 1000) {
                want = want * (int64_t)ratio / 1000;
                if (!s_score_fold_n) {
                    ce_log("score-in: ★ 折额兜底生效 ×%d‰（字段 %u ≠ 语义遮值 —— 见 sdk_core.h「遮」一节）",
                           ratio, (unsigned)poc_field_bits);
                }
                ++s_score_fold_n;
            }
        }
        if (want > 0x7fffffff) {               /* 32 位 ecx 装不下 ⇒ 饱和 + 报一次（G_total ≈ 2,147,483 点以上）*/
            want = 0x7fffffff;
            if (!s_score_in_sat) {
                s_score_in_sat = 1;
                ce_log("score-in: G_total=%d overflows the engine's ecx (saturating at 0x7fffffff)", (int)total);
            }
        }
        if ((uint32_t)want != base) regs->ecx = (uint32_t)want;
    }
    s_score_in_total = total;              /* 给下面的 `score-out:` 用（= 我们真正落下去的那个 G）*/

    /* ⑤ 实跑线索：一行把整条链摊开（同一颗道具后面还会跟一行 `score-out:`）。
     *   限流照 `BP_ce_bullet_damage_final` 的规矩：头 30 颗 + 之后每 50 颗；
     *   in/out 两行共用同一个判据 ⇒ 日志里永远成对、可逐颗对照。*/
    ++s_score_in_n;
    s_score_in_log = (s_score_in_n <= 30) || (s_score_in_n % 50 == 0);
    if (s_trace && s_score_in_log) {
        ce_log("score-in: base=%u G0=%d lay=%d sum=%d -> ecx=%u coeff=%d (#%u)",
               (unsigned)base, (int)g0, (int)lay, (int)ce_score_aggregate(),
               (unsigned)regs->ecx, (int)ce_score_coeff_permille(), s_score_in_n);
    }
    return BP_EXEC_ORIGINAL;
}

/* 两个老出口站点：**纯观察者**（见文件头与 sites.py 的说明）。
 *   0x446cf6 = 「自机在线上」或「道具 state == 3（真空飞行）」那一支的出口；
 *   0x446caa = **线下且非真空飞行**那一支的出口（0x446cb2 `jmp 0x446cfc` 绕过前者）。
 * 它们只做一件事：把引擎算好的最终值（`regs->esi` = 弹窗值 = 10 × 这一颗的 SCORE 增量）与我们的
 * 预测（`10 × G_total`）比一行 —— 满额支应当**完全相等**，线下支应当更小（差额就是引擎的高度折扣）。
 * 这就是「系数确实乘在整笔 `G_0 + ΣG_i` 上」的实跑证据，也是把注入点搬到入口之后**唯一**的观测手段。
 * ⚠ 一个字都不许写：写 `regs->esi` 会把引擎的值覆盖掉（那正是本次要修的 bug），调卡回调会让声明**双计**。
 *   实跑确认「满额支 delta == 0」之后，这一对站点连同本函数一起删。*/
static int ce_item_score_out_common(x86_reg_t *regs, int below)
{
    if (s_trace && s_score_in_log) {
        int32_t engine = (int32_t)regs->esi;
        int32_t ours = s_score_in_total * CE_SCORE_ENGINE_UNIT;   /* = 我们落下去的 G × 10（满额支应当相等）*/
        ce_log("score-out: engine=%d ours=%d delta=%d (%s)", (int)engine, (int)ours,
               (int)(ours - engine), below ? "below-line => coefficient" : "at/above line => must be 0");
    }
    return BP_EXEC_ORIGINAL;
}

int __cdecl BP_ce_item_score_in(x86_reg_t *regs, void *bp_info)
{
    ce_site_trail("ce_item_score_in");
    (void)bp_info;
    return ce_item_score_in_common(regs);
}

int __cdecl BP_ce_item_score(x86_reg_t *regs, void *bp_info)
{
    ce_site_trail("ce_item_score");
    (void)bp_info;
    return ce_item_score_out_common(regs, 0);
}

int __cdecl BP_ce_item_score_below(x86_reg_t *regs, void *bp_info)
{
    ce_site_trail("ce_item_score_below");
    (void)bp_info;
    return ce_item_score_out_common(regs, 1);
}

int __cdecl BP_ce_item_money(x86_reg_t *regs, void *bp_info)
{
    ce_site_trail("ce_item_money");
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

/* Player__on_tick__body 0x45c048 的普通炸弹分支：原指令 `test BYTE PTR ds:0x4ca434,0x2`。
 * 任一在场行为卡的 `on_bomb_gate` 返回 1 时，跳过原指令并把 retaddr 指到 0x45c056（**跳过 0x45c051
 * 的 `call do_bomb`**；该调用前后只有 test/je/call 三条）。没有卡要求拦截时返回 1 放行，原版炸弹不受影响。*/
int __cdecl BP_ce_bomb_gate(x86_reg_t *regs, void *bp_info)
{
    (void)bp_info;
    uint8_t *mgr = CE_ABILITY_MGR();
    if (!mgr) return BP_EXEC_ORIGINAL;
    unsigned guard = 0;
    for (const uint8_t *node = *(const uint8_t *const *)(mgr + CE_MGR_CARD_LIST_FIRST);
         node && guard < 256;
         node = *(const uint8_t *const *)(node + CE_NODE_NEXT), ++guard) {
        uint8_t *card = *(uint8_t *const *)(node + CE_NODE_CARD);
        if (!card) continue;
        const ce_hooks_t *h = hooks_of(card);
        if (h && h->on_bomb_gate && h->on_bomb_gate((ce_card_t *)card)) {
            ce_sdk_trace(ce_card_id(card), 0x78, "on_bomb_gate");
            ce_retaddr_set(regs, 0x45c056);   /* 原版: 0x45c048 test / 0x45c04f je / 0x45c051 call do_bomb */
            return 0;                    /* 不执行原 test，直接从这里继续 */
        }
    }
    return BP_EXEC_ORIGINAL;
}

/* Player__tick_shooting_state 0x45ea00 的查表点 0x45ebfb：原指令 `mov edi,[edi+edx*4+0xe0]`，
 * edx = 本帧从火力/聚焦算出的 set index，eax = 火力档；同帧 0x45ebed/0x45ebf8 已把同一 set index
 * 暂存在 [ebp-8]，0x45ec69 再打包成 `(set<<8)|row` 写进 PlayerBullet+0xac、命中时再回读。
 * 只改这两个位置：edx 管本帧从哪组取 row，[ebp-8] 管弹里记哪一组；不碰 sht 的 0..9 槽。
 * 任一卡的 on_shot_set_index 返回 >=0 时覆盖，否则原版放行。
 *
 * ★ 2026-09-13（卡 113 灵界）：**从「先到先得」改成按 `shot_set_index_priority` 仲裁**。
 * ★★ 2026-09-16z（用户口径「改子机和改主体发弹的覆盖原则」）：阶梯 **117(20) > 113(10) > 108 > 111**
 *   —— 原则「特殊大于常态 / 专用大于通用」，其中 **117 > 113 是人为规定**；
 *   108/111 **不参与这一层**（111 的主炮一个字都不写；108 只是「本帧不发原生弹」，见下面那段
 *   `on_native_shot_suppress`），所以「有卡覆盖 set ⇒ 本帧不抑制」（本函数末尾那条早退）就已经
 *   把 117/113 排在 108 前面了。完整阶梯表见 `sdk/public.h`。
 *   起因：117 咆哮与 113 灵界都往这个断点返回组号，谁先被问到取决于卡链表顺序（拿卡/装卡顺序），
 *   同一局里可能变 ⇒ 行为不可预期。规则（用户 2026-09-13 定）：**灵界赢**（113 的短窗口，
 *   它把主炮整组换掉）。实现：扫完整条卡链表，取「返回 >= 0 且优先级最大」的那张；
 *   同优先级取链表里靠前的（= 旧行为，对只声明 117 一家的现况零影响）。
 *   两张以上同帧都要覆盖时打一条 trace（槽 `0x98`，未占用的空槽，见 sdk.c 顶部的槽表）—— 现场日志能直接看出谁赢了。*/
/* ★★★ 2026-09-23（108 的「子机不发」修法第 2 步，见 `HANDOFF-108.md` §4）：
 *   **本帧有没有卡认领过 set** —— 认领了，说明**行是那张卡的**，那么膛口落在原生子机数组里的弹
 *   就该让它飞（`BP_ce_native_option_muzzle` 的「不发弹」只该管**原生那一组**的行）。
 *   记帧号而不是 bool：投弹与选 set 在同一个玩家 tick 内，用帧号比较天然只在同一帧内有效。*/
static uint32_t s_set_claimed_frame;
static int      s_set_claimed_seen;

int __cdecl BP_ce_shot_set_index(x86_reg_t *regs, void *bp_info)
{
    (void)bp_info;
    uint8_t *mgr = CE_ABILITY_MGR();
    /* ▼ 站点自检门 ①（2026-09-18，**只记账**）：本站点窗口 = `mov edi,[edi+edx*4+0xe0]`
     *   ⇒ 站点处的 `edi` 必须是**本机体的 sht 表基址**（一手：engine.h `CE_PLAYER_SHT_PTR` = player+0x47940，
     *   取自 `th18:0x443d32 mov eax,[ecx+0x47940]`）。对不上 = 我们在别的上下文里命中了这条站点
     *   （本卡 109 的「拦 Z」与 117/113 的 set 覆写都建立在这个前提上）⇒ 记一行，**照常放行**。*/
    /* ★★★ 2026-09-23 订正（这条自检原来**自己写错了** ⇒ 一直是假警报）：
     *   `CE_PLAYER_SHT_PTR`（`0x47940`）是 zPlayer 里的**指针字段**（一手：`th18:0x443d32
     *   mov eax,[ecx+0x47940]`），全仓其它 6 处消费者都写 `*(u8**)(p + 0x47940)`；而这里原来比的是
     *   **字段的地址** `player + 0x47940` ⇒ 永远不等 ⇒ 每次开局都打一行 `assume FAIL`，
     *   **信息量为零**，还把它误当成了「妖侧不出弹」的第一嫌疑。
     *   现在按值比（`edi` 应在站点处 = sht 基址），并且在**第一次有卡认领**时把四个真值打进日志 ——
     *   那一行才是判「覆写有没有落在本机体行表那条循环上」的证据。*/
    {
        const uint32_t sht_now = CE_PLAYER()
            ? *(const uint32_t *)(uintptr_t)(CE_PLAYER() + CE_PLAYER_SHT_PTR) : 0u;
        if (sht_now && regs->edi != sht_now)
            ce_assume_fail("shot-set-index: 站点处 edi 不是本机体 sht 表基址（拦Z/set 覆写的前提）");
    }
    if (!mgr) return BP_EXEC_ORIGINAL;
    unsigned guard = 0;
    uint8_t *winner = NULL;
    int winner_idx = -1, winner_prio = 0, n_claim = 0;
    for (const uint8_t *node = *(const uint8_t *const *)(mgr + CE_MGR_CARD_LIST_FIRST);
         node && guard < 256;
         node = *(const uint8_t *const *)(node + CE_NODE_NEXT), ++guard) {
        uint8_t *card = *(uint8_t *const *)(node + CE_NODE_CARD);
        if (!card) continue;
        const ce_hooks_t *h = hooks_of(card);
        if (h && h->on_shot_set_index) {
            int idx = h->on_shot_set_index((ce_card_t *)card, (int)regs->eax);
            if (idx < 0) continue;
            ++n_claim;
            /* 严格大于：同优先级保持「链表里靠前的先赢」（= 加这个字段之前的语义）。*/
            if (!winner || h->shot_set_index_priority > winner_prio) {
                winner = card;
                winner_idx = idx;
                winner_prio = h->shot_set_index_priority;
            }
        }
    }
    if (winner) {
        int idx = winner_idx;
        /* ★ 2026-09-23：**第一次认领**时打一行真值（one-shot，不随帧刷）——
         *   判据：`edi` 是否 = `[player+0x47940]`（sht 基址）、引擎给的档 `eax`、
         *   本帧 set `edx` 与栈上的 `[ebp-8]` 副本。若 `edi != sht` ⇒ 我们的 set 号被塞进了
         *   **别的表**（那正是「妖侧一行都不发」的机制）。*/
        {
            static int s_first_logged;
            if (!s_first_logged) {
                s_first_logged = 1;
                ce_log("ce: shot-set-index first claim (frame %u): edi=%#x [player+0x47940]=%#x "
                       "eax(tier)=%d edx(set before)=%d stack_slot=%#x winner id=%u prio=%d set=%#x",
                       (unsigned)ce_sdk_frame(), (unsigned)regs->edi,
                       CE_PLAYER() ? (unsigned)*(const uint32_t *)(uintptr_t)(CE_PLAYER() + CE_PLAYER_SHT_PTR) : 0u,
                       (int)regs->eax, (int)regs->edx,
                       regs->ebp ? (unsigned)*(const uint32_t *)(uintptr_t)(regs->ebp - 8) : 0u,
                       (unsigned)ce_card_id(winner), winner_prio, (unsigned)idx);
            }
        }
        if (n_claim > 1) {
            /* 同帧争用：记一条（含胜者的优先级与卡号），现场靠它区分「灵界赢」还是「咆哮赢」。*/
            ce_sdk_trace(ce_card_id(winner), 0x98, "on_shot_set_index contested");
            ce_log("ce: shot set contested: %d claimers, winner id=%u prio=%d set=%#x",
                   n_claim, (unsigned)ce_card_id(winner), winner_prio, (unsigned)idx);
        }
        if (regs->edx != (uint32_t)idx) {
            /* 两个副本必须一起改：edx 决定本帧取出哪组 row；[ebp-8] 决定 bullet+0xac 里存的 set。*/
            regs->edx = (uint32_t)idx;
            if (regs->ebp) *(uint32_t *)(uintptr_t)(regs->ebp - 8) = (uint32_t)idx;
        }
        s_set_claimed_frame = ce_sdk_frame();
        s_set_claimed_seen = 1;
        ce_sdk_trace(ce_card_id(winner), 0x7c, "on_shot_set_index");
        /* ★ 有卡覆写了 set（= 117 咆哮 / 113 灵界）⇒ **本帧不抑制**。
         *   用户口径 2026-09-16z 的阶梯：**117 > 113 > 108 > 111** ⇒ 这一条早退同时实现了
         *   「117 / 113 都排在 108 前面」（108 只压原生弹，不换 set）。*/
        return BP_EXEC_ORIGINAL;
    }
    /* 没有任何卡覆写 set ⇒ 再问「本帧跳过原生行循环」。
     * 108（低速妖机体）与 109（蓄力拦 Z）挂同一个槽；两者语义都是「本帧不发原生弹」⇒ OR，无需优先序。
     * 做法：return 0 + retaddr 跳到 0x45eca0（**引擎自己的空行落点** `0x45ec06 js 0x45eca0`）——
     * 跳过 0x45ebfb..0x45ec9f 的原生行循环，但**保留 0x45eca0 起的卡广播**（77 / 116 不受影响）。
     * 寄存器安全性：0x45eca0 起只用 eax/edi（就地重写）与 ebx/esi（从 [ebp-0xc]/[ebp-0x10] 重载，
     * 而那两个槽在行循环**之前**就写好了）—— 见 AUDIT。*/
    guard = 0;
    for (const uint8_t *node = *(const uint8_t *const *)(mgr + CE_MGR_CARD_LIST_FIRST);
         node && guard < 256;
         node = *(const uint8_t *const *)(node + CE_NODE_NEXT), ++guard) {
        uint8_t *card = *(uint8_t *const *)(node + CE_NODE_CARD);
        if (!card) continue;
        const ce_hooks_t *h = hooks_of(card);
        if (h && h->on_native_shot_suppress && h->on_native_shot_suppress((ce_card_t *)card)) {
            ce_sdk_trace(ce_card_id(card), 0x80, "on_native_shot_suppress");
            ce_retaddr_set(regs, 0x45eca0);
            return 0;                    /* 不执行原行选择，直接去卡广播 */
        }
    }
    return BP_EXEC_ORIGINAL;
}

/* ======================================================================================
 * ★★ 2026-09-16：**隐藏自机原生那 4 格子机**（两条断点；声明层在 `sdk_core.h`）
 * ======================================================================================
 * 背景（用户口径 2026-09-16：「魔理沙原生子机需要弄掉」——装了 111 且机体 = 魔理沙时，
 * 自机原生那 4 格子机不许再出现）。一手结构见 `engine.h` 的 `CE_NATIVE_OPT_*` 一节：
 * 引擎里**两个子机数组**互不相干 —— 原生的 `inner+0x50`（4 格，按火力档重建）与
 * 卡申请的池 `inner+0x410`（12 格）。本层只动前者；后者归 `ce_optpose_*` 选举层。
 *
 * 为什么**必须两条**断点（少一条都会露馅，逐条给一手）：
 *   ① `BP_ce_repopulate_native_option`（`0x45d610`）= **不建 + 收掉**：
 *      原生子机的 VM 只在 `Player__repopulate_options_and_notify_cards` `0x45d5e0` 里建
 *      （普通形 `+0xb0`、满火力形 `+0xb4`），而它按 `lvl = CURRENT_POWER/100` 建 ⇒ 把 `lvl` 压成 0
 *      就"永远建不出来"；已建的两支 VM 由 handler 自己 `interrupt` 掉（照引擎自己那两段清理的写法：
 *      `0x45d823` 的 `push 1 / push [esi] / call 0x488be0 / [esi]=0`）。
 *   ② `BP_ce_native_option_muzzle`（`0x45e930`）= **不发弹**：
 *      主炮行循环选组只看 `CURRENT_POWER/100`（`0x45ebcb idiv`），**不看子机存不存在** ——
 *      `row[0x20] != 0` 的行一律拿 `PLAYER+0x580+(slot&0xf)*0xf0` 当膛口出弹（`0x45ec4c`–`0x45ec5d`）
 *      ⇒ 只做①的话，子弹会**从看不见的子机位置**飞出来（而且那部分输出本来是要被 111 的子机层接管的）。
 *      在 `Player__shoot_one_bullet` 入口拦「膛口指针落在原生数组区间」的那些发。
 *      ⚠ 卡路径（`0x40aa80`）传的膛口是**卡池**指针（`inner+0x410` = `PLAYER+0xa30`）⇒ 区间不相交，
 *        117 / 116 / 108 那些卡申请的装备子机一发都不会被误伤（实测判据 = 指针区间，不是行字段）。
 *
 * ⚠ 两条都只在 `ce_native_option_hidden(当前机体)` 为真时动手 ⇒ 灵梦 / 咲夜 / 早苗机体、
 *   以及没装 111 的魔理沙，行为逐字节不变。
 * ⚠ 站点与字节（`sites.py` 里逐条核过）：`0x45d610` 7 字节 `53 89 81 dc 71 04 00`
 *   （`push ebx` + `mov [ecx+0x471dc],eax`；**该 mov 的 imm32 是 0x471dc，不是相对寻址**）、
 *   `0x45e930` **5 字节** `55 8b ec 51 53`（`push ebp` / `mov ebp,esp` / `push ecx` / `push ebx` ——
 *   恰好落在指令边界上；⚠ 取 6 会从 `mov ebx,[ebp+8]`（3 字节，0x45e935 起）中间切开 ⇒ cave 回放半条指令）。
 * 详见 `NEXT.md` 的「111 魔理沙侧」一节与 `AUDIT.md` §BE。*/

/* ---- ① repopulate：把本次的 lvl 压成 0，并收掉已建的原生子机 VM ----
 * 站点：`0x45d610`（`push ebx` + `mov [ecx+0x471dc],eax`），此刻
 *   · **ecx = `&zPlayer.inner`**（thiscall 的 this = `PLAYER+0x620`；一手 `0x45d5fc mov [esp+0x18],ecx`）、
 *   · **eax = 本次算出来的 `lvl = CURRENT_POWER / POWER_PER_LEVEL`**（`0x45d604`–`0x45d60a`）。
 * handler 把 eax 改成 0 后**放行原字节**（cave 重放那 7 字节）：于是
 *   · `[inner+0x471dc] = 0`（该字段全 exe 只在这一处写、无人读 ⇒ 无副作用）、
 *   · `0x45d622` 把 eax 存进 `[esp+0x10]`、`0x45d843` 再读回来当档位
 *     ⇒ 后面两段（满火力形 VM 的建/清、普通形 4 格的建/清）拿到的都是 0：
 *     满火力形的建循环 `if (0 < lvl)` 直接不进；普通形走 `lvl < 1` 的「清空 4 格」那条路
 *     （interrupt `+0xb0` + 状态写 0）—— 正是我们要的。
 * ⚠ **不改** `inner+0x470d8`（"上次建过哪一档"的短路缓存）：引擎自己会把它写成 0，
 *   下一次带 lvl=0 进来时短路到"不用清"，而那时确实已经没有东西可清（我们每帧都收干净了）；
 *   卡被删掉之后，真实档位 != 0 ⇒ 短路条件不成立 ⇒ 引擎自己重建，**不需要我们写回任何东西**。
 * ⚠ 满火力形（`+0xb4`）那两支 VM 在「引擎认为 lvl == 0」时**不会**被引擎清（那条 `if` 分支
 *   只在 `CURRENT_POWER < MAX_POWER` 时跑）⇒ handler 自己按 `0x45d818` 那段同款写法收掉 4 格的两支。*/
int __cdecl BP_ce_repopulate_native_option(x86_reg_t *regs, void *bp_info)
{
    uint8_t *inner = (uint8_t *)(uintptr_t)regs->ecx;
    int32_t character;
    int i;

    (void)bp_info;
    if (!inner) return BP_EXEC_ORIGINAL;
    character = *(const int32_t *)CE_ADDR_CHARACTER;
    if (!ce_native_option_hidden(character)) return BP_EXEC_ORIGINAL;
    /* ★★★ 2026-09-23（修法第 1 步）：**有卡请求保留记录 ⇒ 本支什么都不做**（既不压 `lvl` 也不清 `state`/VM）
     *   ⇒ 引擎照常建记录、每帧跟随坐标 ⇒ 行循环有活膛口；而**显示**由那张卡自己的
     *   `ce_native_option_sweep_keep_state()` 每帧收掉（这一步必须留给**每帧**做，因为引擎建记录的那一刻
     *   就同时建了 VM ⇒ 想在**这一支**里「只留记录不留 VM」是不可能的）。*/
    if (ce_native_option_records_wanted()) return BP_EXEC_ORIGINAL;

    /* 收掉 4 格 × 2 支原生子机 VM（普通形 / 满火力形）。
     * ⚠ id 读出来就置 0 再 interrupt（引擎自己也是这个顺序：先 interrupt 再写 0 —— 顺序无所谓，
     *   但一定要写 0，否则 `FUN_0045bc90` 每帧还会拿这个 id 去摆位置）。*/
    for (i = 0; i < CE_NATIVE_OPT_COUNT; ++i) {
        uint8_t *slot = inner + CE_NATIVE_OPT_INNER_BASE + (size_t)i * CE_NATIVE_OPT_STRIDE;
        uint32_t *vm[2];
        int k;

        vm[0] = (uint32_t *)(slot + CE_NATOPT_VM_MAIN);
        vm[1] = (uint32_t *)(slot + CE_NATOPT_VM_MAX);
        for (k = 0; k < 2; ++k) {
            if (*vm[k]) {
                /* ▼ 站点自检门 ②（2026-09-18，**fail-safe**）：破坏/重建发弹点要拿这个 id 去调引擎的
                 *   `AnmManager::interrupt_tree`。id 的合法域是「引擎给的 VM 句柄」⇒ 明显垃圾
                 *   （小整数 / 内核段）时**不调引擎**（避免引擎往垃圾地址写），只把字段清 0 + 记账
                 *   —— 字段清 0 = 引擎下次重建的入口条件，这一步照做。*/
                if (*vm[k] < 0x10000u || *vm[k] >= 0x80000000u)
                    ce_assume_fail("repopulate: 原生子机 VM id 不在合理域（拒绝调引擎，只清字段）");
                else
                    ce_anm_interrupt(*vm[k], 1);
                *vm[k] = 0;
            }
        }
        *(int32_t *)(slot + CE_NATOPT_STATE) = 0;
    }
    /* 本次的档位 = 0（见上面那段：两段建的/清的都看它）*/
    regs->eax = 0;
    ce_sdk_trace((unsigned)ce_native_option_hide_owner(), 0x9c, "native option hidden (repopulate)");
    return BP_EXEC_ORIGINAL;
}

/* ---- ② Player__shoot_one_bullet 0x45e930 入口：膛口落在原生子机数组里 ⇒ 整发跳过 ----
 * 站点 = 函数**第一条**指令起的 5 字节（`55 8b ec 51 53`），此刻
 *   · **ecx = zPlayer\***（thiscall；一手调用点 `0x45ec66 mov ecx,esi`，esi = PLAYER）、
 *   · 栈（低 → 高）= **thcrap 蹦床返回地址（站点+5）**、调用者返回地址、packed id、timer、pos*、**option\***
 *     （四个栈参由调用方压，一手 `0x45ec5f`–`0x45ec72`：先 push 膛口、再 push pos、push timer、push packed id）。
 *
 * ★★ 2026-09-17 订正（本次会话，一手 = `engine/_shared/thcrap-breakpoint-pools.md` §2）：
 *   本版 thcrap 的桩**直接 `call BP_*`**，**没有 `breakpoint_process` / `esp_diff`** ⇒
 *   ① `regs->esp` 写了没人消费（老注释说的 `add esp, diff` 是 2020 老源码的写法，本机桩是 `mov esp,ebp`）；
 *   ② `regs->esp` 指的**不是**调用者返回地址，而是 thcrap 自己那条 `call <桩>` 压进去的**站点+5**
 *      （本文件 §「实跑订正②」在 `0x430240` 处实测到 `0x430245`，同一形态）；
 *   ③ 因此膛口也**不是** `sp[4]`（那是 pos*）⇒ 已改成 `sp[5]`。
 *   ⇒ 上一版（`retaddr = sp[0]` + `esp += 0x14`）实际会**落到函数体内、且序言不重放、栈参不弹** ⇒
 *     帧错位之后某个 `ret` 会弹到栈上的陈旧值（其中就有 thcrap 池地址）——正是头号崩溃那一类。
 *
 * 现在的写法 = **只用 thcrap 原生机制**：站点在 `sites.py` 里声明 `stack_clear_size: 0x10`
 *   （⇒ 用 `bp_entry0s` 变体，桩末尾是 `ret imm16`），handler 只做两件事：
 *   设 `retaddr = sp[1]`（真正的调用者返回地址）+ 返回 0（不重放原字节）⇒ 桩执行 `ret 0x10`：
 *   **回到调用者 + 弹掉 4 个栈参**，与注释里想要的「等价 `ret 0x10`」逐字一致，且不依赖任何被忽略的字段。
 *   （该函数的清理在**被调方**：调用点 `0x45ec78` 之后没有 `add esp,0x10`，一手核过 ⇒ stdcall 形态。）
 * ⚠ 返回值不设（调用方 `0x45ec78` 紧接着读行字段，**不检查 eax**，一手逐指令核过）。
 * ⚠ 判据用**指针区间**而不是行字段：卡路径同样走这个函数，但它的膛口是卡池指针
 *   （`PLAYER+0xa30` 起）⇒ 区间不相交，天然只命中原生 4 格。*/
int __cdecl BP_ce_native_option_muzzle(x86_reg_t *regs, void *bp_info)
{
    uint32_t *sp = (uint32_t *)(uintptr_t)regs->esp;
    uint8_t *player = (uint8_t *)(uintptr_t)regs->ecx;
    uint8_t *muzzle;

    (void)bp_info;
    if (!player || !sp) return BP_EXEC_ORIGINAL;
    if (!ce_native_option_hidden(*(const int32_t *)CE_ADDR_CHARACTER)) return BP_EXEC_ORIGINAL;
    /* ★ 2026-09-23（第 2 步）：**本帧已有卡认领 set ⇒ 放行**（理由见 `s_set_claimed_frame` 的注）。
     *   影响面：今天只在「111 声明了隐藏（= 魔理沙机体）+ 有卡认领 set（= 108 的低速）」这一种组合下
     *   改变行为，而那种组合下这些弹此前本来就发不出来（记录被隐藏断点压掉了）⇒ 现在只是**把门先打开**。*/
    if (s_set_claimed_seen && s_set_claimed_frame == ce_sdk_frame()) return BP_EXEC_ORIGINAL;
    muzzle = (uint8_t *)(uintptr_t)sp[5];          /* 第 5 个栈参 = 膛口（0 = 自机本体）；sp[0] 是蹦床返回地址 */
    if (!muzzle) return BP_EXEC_ORIGINAL;
    if (muzzle < player + CE_NATIVE_OPT_PLAYER_MUZ0 || muzzle >= player + CE_NATIVE_OPT_PLAYER_END)
        return BP_EXEC_ORIGINAL;                   /* 不是原生子机（自机本体 / 卡池子机）⇒ 放行 */
    /* 诊断（限流：前 3 发 + 之后每 600 发一行）：一发不发的时候**唯一**能看出「这条路在跑」的行。*/
    {
        static uint32_t s_n;
        if (s_n < 3u || (s_n % 600u) == 0u)
            ce_log("sdk: 原生子机弹已拦（第 %u 发，膛口 = PLAYER+0x%x，机体 %d）",
                   (unsigned)(s_n + 1u),
                   (unsigned)(muzzle - player),
                   *(const int32_t *)CE_ADDR_CHARACTER);
        ++s_n;
    }
    ce_retaddr_set(regs, sp[1]);                   /* 回到调用者（sp[0] = thcrap 蹦床的返回地址 = 站点+5）*/
    return 0;                                      /* 不重放原字节（= 整发不发）；栈参由 `stack_clear_size` 的 ret 0x10 弹掉 */
}


/* Player__tick_shooters_for_ability_card 0x40a9c0 的序言后（0x40a9c3）：
 *   40a9c0 push ebp / 40a9c1 mov ebp,esp /
 *   40a9c3 mov edx,[ebp+8] / 40a9c6 sub esp,0xc / 40a9c9 test edx,edx / 40a9cb je 0x40aa9e
 *   ← 站点 = 前两条完整指令，共 6 字节；**无相对寻址**（thcrap 的 cave_fix 不重定位 `0f 84`）。
 * 站点处 ebp 已就位；栈参 [ebp+8]=option、[ebp+0xc]=short、[ebp+0x10]=long、[ebp+0x14]=sht set。
 * handler 返回 1 = 执行这两条原指令（mov + sub），回到 0x40a9c9 原样继续；
 * 返回 0 且 retaddr=0x40aa9e 才跳过整段（`mov esp,ebp` 重设栈顶）。
 * 上一版只取 0x40a9c3 的 3 字节 `mov edx,[ebp+8]`：低于 thcrap sourcecave 下限 5，断点被拒；
 * SDK 门回读失败后 `restore_alloc_bound` 把新 id 禁掉 —— 存档/编成里已有新 id 时就是用户看到的
 * 「卡 id 变垃圾 / HUD 槽消失」。**2026-09-12 十四修已按用户 bug 5 打开**（站点 6 字节、门回读已过）。 */
int __cdecl BP_ce_option_shot_gate(x86_reg_t *regs, void *bp_info)
{
#if !CE_OPTION_SHOT_GATE_ENABLE
    (void)regs;
    (void)bp_info;
    return BP_EXEC_ORIGINAL;
#else
    (void)bp_info;
    uint8_t *mgr = CE_ABILITY_MGR();
    int32_t set;
    unsigned guard = 0;

    if (!mgr || !regs->ebp) return BP_EXEC_ORIGINAL;
    set = *(const int32_t *)(uintptr_t)(regs->ebp + 0x14);
    /* ★ 2026-09-13 十八修（诊断，一次运行最多记 8 个不同值）：用户报「按 Z 进魔理沙狼咆哮子机还发射激光」，
     *   而日志里**没有** `option shot suppressed` 行 ⇒ 先证明这条路径到底跑没跑、拿的是哪些 set：
     *   出现 `option shot path set 0x..` ⇒ 就是这条路径（再看是否被下面的区间漏掉）；
     *   一整局一次都没有 ⇒ 那些激光**不是**这条路径出的（去查主炮路径 / 别的函数）。*/
    {
        static int32_t s_seen[8];
        static int s_seen_n;
        int i, dup = 0;
        for (i = 0; i < s_seen_n; ++i) if (s_seen[i] == set) { dup = 1; break; }
        if (!dup && s_seen_n < 8) {
            s_seen[s_seen_n++] = set;
            ce_log("wba_c: option shot path set 0x%x (#%d, %s)", (unsigned)set, s_seen_n,
                   (set >= CE_SHT_SET_OPTION_MIN && set <= CE_SHT_SET_OPTION_MAX) ? "native option group" :
                   (set >= CE_SHT_SET_BEAST_MIN && set <= CE_SHT_SET_BEAST_MAX)   ? "117 beast group" :
                                                                                    "OTHER (not in range!)");
        }
    }
    /* ★ 2026-09-13 十七修：原来只拦原生装备子机组 0x0a..0x15，但这条路径在咆哮期间会拿
     *   **咆哮组**当 set 继续开火（魔理沙狼咆哮 = WOLF 组 0x1e..0x22，非空 ⇒ 用户看到的
     *   「子机还在发射激光」；鹫的 EAGLE 组 0x23..0x27 是空组，所以那次看起来是好的）。*/
    if (!((set >= CE_SHT_SET_OPTION_MIN && set <= CE_SHT_SET_OPTION_MAX) ||
          (set >= CE_SHT_SET_BEAST_MIN   && set <= CE_SHT_SET_BEAST_MAX)))
        return BP_EXEC_ORIGINAL;
    for (const uint8_t *node = *(const uint8_t *const *)(mgr + CE_MGR_CARD_LIST_FIRST);
         node && guard < 256;
         node = *(const uint8_t *const *)(node + CE_NODE_NEXT), ++guard) {
        uint8_t *card = *(uint8_t *const *)(node + CE_NODE_CARD);
        if (!card) continue;
        const ce_hooks_t *h = hooks_of(card);
        if (h && h->on_option_shot_gate && h->on_option_shot_gate((ce_card_t *)card, (int)set)) {
            ce_sdk_trace(ce_card_id(card), 0x94, "on_option_shot_gate");
            ce_retaddr_set(regs, 0x40aa9e);   /* 该函数 option==NULL 的空路径收尾 */
            return 0;
        }
    }
    return BP_EXEC_ORIGINAL;
#endif
}

/* Enemy__drop_items_and_notify_cards 0x430510 入口（thiscall，ecx = zEnemyDrop = enemy+0x623c）：在引擎撒**附加表**之前
 * 把掉落数表交给卡改。改完引擎自己按各 type 正确的角度/速度去撒 —— 比自己调
 * ItemManager__spawn_items（8 个栈参、原生调用方还留了没填的槽）稳得多。AUDIT §S。 */
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
    /* ★ 乘法层（EVENTS-CONTRACT §2）：各卡只做**加法**（照现状直接改 counts），倍数在这里一次性
     * 作用在「最终总数」上 ⇒ 「别的卡/附加表加的那颗也被翻倍」（用户 2026-09-12 裁决）。
     * 必须在卡循环**之后**、引擎按 counts 撒之前（本断点就在撒附加表之前）。*/
    if (ce_drop_mult_aggregate() != CE_DROP_MULT_UNIT) {
        /* ★ 传的是 zEnemyDrop **基址**（regs->ecx），不是 counts —— counts 是基址 +0x04，
         * 传错会整体错开一个 type 格（2026-09-12 审计抓到的 off-by-one）。*/
        ce_drop_apply((int32_t *)drop);
    }
    return BP_EXEC_ORIGINAL;
}

/* ---- collect-2x（辉针城卡）四断点（AUDIT §Z）。四个 handler 都在**原指令前**触发，读的是窗前值；
 * 与 public.h 四槽位（Task 2 追加在 hooks 表尾，缺槽 = NULL，不动的旧卡不受影响）一一对应。
 * 分派只认声明了该槽的卡；mgr 空 → 放行不触发。 ---- */
/* ★ 擦弹（2026-09-10，天空璋卡）：`0x42497f` = 引擎自己算完一次**子弹擦弹**之后那一条。
 * ESI = 刚被擦到的弹（`0x42496f` 已判过 `[esi+0x20]&4` 没擦过、`0x424984` 才置位），位置在 `esi+0x638`。
 * 卡用 `ce_graze_take(&x,&y)` 取「自上次以来」的擦弹次数 + 最后一次的弹坐标（季节点从弹上撒）。*/
static float   s_graze_x, s_graze_y;
static int32_t s_graze_pending;

int __cdecl BP_ce_graze(x86_reg_t *regs, void *bp_info)
{
    uint8_t *b = (uint8_t *)(uintptr_t)regs->esi;
    (void)bp_info;
    if (b) {
        s_graze_x = *(const float *)(b + CE_BULLET_POS);
        s_graze_y = *(const float *)(b + CE_BULLET_POS + 4);
        if (s_graze_pending < 64) ++s_graze_pending;
    }
    return BP_EXEC_ORIGINAL;
}

int ce_graze_take(float *x, float *y)
{
    int n = s_graze_pending;
    s_graze_pending = 0;
    if (n > 0) { if (x) *x = s_graze_x; if (y) *y = s_graze_y; }
    return n;
}

/* ★ 2026-09-27（第三十九批）：**只读**版（不消费）—— 见 `sdk.h` 的同名注释：
 * 108（擦弹刻符的落点）与 116（季节点）会在同一帧读同一个事件，用 `take` 会互相偷数据。*/
int ce_graze_peek(float *x, float *y)
{
    int n = s_graze_pending;
    if (n > 0) { if (x) *x = s_graze_x; if (y) *y = s_graze_y; }
    return n;
}

/* ★★★ 2026-09-13（owner 口径：「**没有进 replay**」→「要它进录像」）：
 *   **调试件「逐帧合成方向输入」的落点**（`cards/input_synth.{h,c}`；机制/一手见 `DEV-INPUT-SYNTH.md` §4）。
 *   站点 = `th18:0x462966`，在 `ReplayManager__on_tick__record_replay`（`0x462940`）体内：
 *
 *       0x462950  INPUT_HELD_PREV = INPUT_HELD
 *       0x46295a  INPUT_HELD = (uint16)RAW(0x4ca210)        ; raw 键盘整体重写
 *       ------------------- handler 在这里进来 -------------------
 *       0x462966  call 0x42abc0                              ; ← 窗口那条被挪走的原指令
 *       0x4629f7  held16 = (uint16)INPUT_HELD                ; ← 录制读点
 *       0x462a19  call 0x463060(chunk, held16, pressed16, released16)
 *
 *   ★ 为什么这样能保证「**拷贝已完成、我们写在其后**」：本仓的站点机制是**handler 在原件之前跑**
 *   （`thcrap_bp.h`：handler 返回 `BP_EXEC_ORIGINAL` 之后，thcrap 才从跳板池回放窗口那几字节）。
 *   站点取 `0x462966`（= `0x462961` 那条 `mov` 的**下一条**）⇒ 进入 handler 时拷贝**已经执行完**
 *   （`INPUT_HELD` 里是本帧 raw），而 `call 0x42abc0` **还没执行**。我们此刻把四个方向位写进
 *   `INPUT_HELD` ⇒ ① `0x42abc0` 为这些位算 `INPUT_PRESSED`/`INPUT_RELEASED`（卡 29 吃这个边沿）；
 *   ② `0x4629f7` 把它录进 replay；③ 同帧 Player（节点 `0x17` > `0x11`）读到 ⇒ 玩法与录像同时正确。
 *   （若改挂 `0x462961`：handler 在那条 `mov` **之前**跑 ⇒ 无法"拷贝之后写"；那条只能靠改
 *    `regs->eax` 让原件自己搬，效果等价但不是 owner 要的落点口径。）
 *   ⚠ 窗内唯一一条 `e8 rel32` **正是窗口首条** ⇒ thcrap 的 `cave_fix` 会重定位（`sitecheck.py
 *   0x462966 5` 判 "fixable"），**不需要 `no_replay`**；同地址先例 = `mods/th18.v1.00a/mouse-control`
 *   的 hook 2（同一个 exe 上已实跑，其 `AUDIT.md` §B 记「录 replay 的 0x463060 在注入点之后」）。
 *
 *   · 写什么：**只动四个方向位**（`~0x0F0` 保留 Z/X/Shift/C/D…），先抹后 OR ⇒ 结果只由模式决定
 *     （现场若真按着方向键，会被抹掉 ⇒ 实验可复现）。
 *   · **默认不写**：`ce_input_synth_frame_mask()` 的 `*write_out == 0`（没按调试键 / 回放中 /
 *     商店冻结帧 / 无自机）⇒ 直接返回，**一个字节都不写**（回放中写会覆盖回放注入的输入 ⇒ 回放必坏；
 *     商店冻结帧同理 —— 录制 tick 在商店里照样跑到本站点，而自机不一定是 NULL）。
 *   · 轻：一次标量调用 + 一次 RMW，没有循环/分配/字符串（handler 在每帧的输入节点里跑）。
 *   · **不返回 `BP_EXEC_ORIGINAL` 以外的东西**：那条 `e8 rel32` 正是我们要引擎执行的 `compute_edges`。*/
/* ★★★ 2026-09-13 **已拆线**（owner：「那个无敌脚本空格的都拆掉，代码保留，注释一下这是什么」）：
 *   本 handler 与它的站点 `ce_input_synth`（`th18:0x462966`）**已不再安装** ——
 *   这是一件**调试用**的「逐帧合成方向输入」工具（按空格开关，替玩家逐帧写方向位、
 *   原地抖动、净位移为 0），原本用来实测**原生卡 29「疾風の下駄」的起步无敌能不能刷成永久**。
 *   ⇒ 代码保留在这里与 `cards/input_synth.{h,c}`，**重挂三步**见 `DEV-INPUT-SYNTH.md` §0。*/
#if 0   /* ← 取消注释（连同上面对应的 sites.py 条目）即可重新装上 */
int __cdecl BP_ce_input_synth(x86_reg_t *regs, void *bp_info)
{
    int      write = 0;
    uint32_t mask;
    (void)regs; (void)bp_info;
    mask = ce_input_synth_frame_mask(&write);
    if (write)
        CE_INPUT_HELD() = (CE_INPUT_HELD() & ~(uint32_t)CE_INPUT_SYNTH_DIR_MASK) | mask;
    return BP_EXEC_ORIGINAL;
}
#endif  /* 0：ce_input_synth 已拆线 */

/* ---- 低速子机声明层的落笔分派（spec §3.3.1；层 = sdk_core 的 `ce_optpose_*`）----
 * 每帧恰好跑一次：① 问层「本帧谁落笔、上一帧的落笔者在不在」；② **退位通知**（上一帧落笔者这一帧没赢
 * ⇒ 叫它一次，让它把子机收干净）；③ 叫本帧落笔者一次（申请 / 逐格写偏移 / 贴图）。
 * ⚠ 为什么退位也要叫一次：层是**每帧**声明的 —— 108 松开 Shift 就声明 0、立刻不再是落笔者；
 *   若没人叫它，它手里那几个 zPlayerOption 会一直留在池里（高速还看得见妖侧子机 = 用户定案 5 的反面）。
 * ⚠ 时序（一手）：卡的 tick 在 AbilityManager（优先级 **0x16**），本断点 = ItemManager tick（**0x1e**，
 *   `th18:0x4458a0 push 0x446ec0` + `0x4458ac push 0x1e` + `0x401180` 注册）⇒ **同一帧内卡先声明、
 *   帧界再落笔**，落笔看到的就是本帧的声明。即使某天顺序反过来，也只是「声明晚一帧生效」——
 *   门式（每帧重写偏移、引擎每帧自己按聚焦位取用，`th18:0x45bce8`–`0x45bd08`）⇒ 无可见差异。*/
static void optpose_call_one(uint8_t *mgr, int id)
{
    unsigned guard = 0;
    for (const uint8_t *node = *(const uint8_t *const *)(mgr + CE_MGR_CARD_LIST_FIRST);
         node && guard < 256;
         node = *(const uint8_t *const *)(node + CE_NODE_NEXT), ++guard) {
        uint8_t *card = *(uint8_t *const *)(node + CE_NODE_CARD);
        if (!card || (int)ce_card_id(card) != id) continue;     /* 退位者可能已经删了 ⇒ 找不到就跳过 */
        const ce_hooks_t *h = hooks_of(card);
        if (h && h->on_option_pose_apply) {
            h->on_option_pose_apply((ce_card_t *)card);
            ce_sdk_trace((uint32_t)id, 0x90, "on_option_pose_apply");
        }
        return;
    }
}

static void ce_option_pose_frame_tick(void)
{
    uint8_t *mgr = CE_ABILITY_MGR();
    int prev = -1, win;

    if (!mgr) return;
    win = ce_optpose_frame(&prev);
    if (prev >= 0 && prev != win) optpose_call_one(mgr, prev);   /* ① 退位通知（只收不写）*/
    if (win >= 0) optpose_call_one(mgr, win);                    /* ② 本帧落笔 */
}

/* ---- ★ PoC 高度层（收点线）：落笔与删卡写回（决策全在 sdk_core.c 的 `ce_poc_frame*` / `ce_poc_drop_take_base`）
 * 语义 / 一手见 `sdk_core.h` 与 `sdk.h` 的 PoC 层块注释。这里只做三件**引擎侧**的事：
 *   ① 写 `player+0x47998`；② 置/清 `Gui+0x194` 的 bit `0x2000`（=「画开局那条指示线」；置照原生卡 22 的
 *      `0x40c288`，清是「遮」声明层新增的写点，见 §7-A4）；③ 读自机 y 当「遮」的钳位上限。
 *
 * ★★ 2026-09-20「遮」（106 的「不满 P 过线不收点」非接管实现）：
 *   · **钳位上限 = 自机 y，且必须在**这里**读**（不是卡 tick 里）—— 一手帧内顺序：
 *     AbilityManager（卡 tick，优先级 `0x16`）→ **Player `0x17`** → ItemManager **`0x1e`**，
 *     而本函数 = `BP_ce_item_frame`（`0x446ee3`，ItemManager tick 里、body `0x445a80` 之前）
 *     ⇒ 此刻的 y 才是引擎那两条 `ja` 与两个折额读点将看到的值（卡 tick 里读到的是**上一帧**的 y）。
 *   · **线显示与高度落笔解耦**：满 P 时没有高度声明（`ce_poc_frame*` 返回 0），但指示线照样不许画
 *     ⇒ 那一段必须在写字段之外**单独**跑。
 * ⚠ **没有声明时不写字段**：原生卡 22 会在 on_load 把线写成 224，本层无权把它顶回 128。*/
void ce_poc_apply(void)
{
    uint8_t *pl = CE_PLAYER();
    uint8_t *gui;
    uint32_t out = 0;
    uint32_t ceiling = 0;

    if (!pl) return;
    if (ce_poc_mask_active()) {
        /* 「遮」在场 ⇒ 钳位上限 = 自机 y（合理性闸：读不到就传 0 ⇒ 层退回 `CE_POC_TINY_BITS`）。*/
        const uint32_t ybits = *(volatile uint32_t *)(pl + CE_PLAYER_Y);
        const float    y     = ce_f32_from_bits(ybits);
        if (ybits != 0u && y > 0.0f && y <= 448.0f) ceiling = ybits;
    }
    if (ce_poc_frame_masked(*(volatile uint32_t *)(pl + CE_PLAYER_POC_HEIGHT), ceiling, &out)) {
        *(float *)(pl + CE_PLAYER_POC_HEIGHT) = ce_f32_from_bits(out);
    }
    /* 线显示（`Gui+0x194` bit `0x2000`）：决策在 sdk_core（含「遮」的基准记账 + 「有人声明高度 ⇒ 照原生
     * 卡 22 打开」那条旧行为），这里只写内存。*/
    gui = CE_GUI();
    if (gui) {
        uint32_t *w = (uint32_t *)(gui + CE_GUI_FLAG_WORD);
        int now = (*w & CE_GUI_FLAG_POC_LINE) ? 1 : 0;
        int target = now;

        if (ce_poc_line_apply(now, ce_poc_has_declaration() ? 1 : 0, &target)) {
            if (target) *w |= CE_GUI_FLAG_POC_LINE;
            else        *w &= ~(uint32_t)CE_GUI_FLAG_POC_LINE;
        }
    }
}

/* 删卡 / 退出声明（`sdk.h` 的 `ce_layer_drop_card()` 调）：撤声明；若撤完**没人再声明**且记过基准
 * ⇒ **立刻**把基准写回字段（删卡发生在商店里、AbilityManager 与 ItemManager 都冻住，帧钩子不跑）。
 * ★ 2026-09-20：这里一并撤「遮」声明与「不画线」声明（前者同属 PoC 层，后者是它的显示面）——
 *   两条都是**粘性**的，漏撤 = 那条线在本局剩下的时间里永远不画 / 字段永远被压着。*/
void ce_poc_release(uint32_t id)
{
    uint8_t *pl = CE_PLAYER();
    uint8_t *gui;
    uint32_t back = 0;
    int bit = 0;

    if (ce_poc_drop_take_base(id, &back) && pl) {
        *(float *)(pl + CE_PLAYER_POC_HEIGHT) = ce_f32_from_bits(back);
    }
    gui = CE_GUI();
    if (gui && ce_poc_line_release(id, &bit)) {
        uint32_t *w = (uint32_t *)(gui + CE_GUI_FLAG_WORD);

        if (bit) *w |= CE_GUI_FLAG_POC_LINE;
        else     *w &= ~(uint32_t)CE_GUI_FLAG_POC_LINE;
    }
}

/* ★★ 2026-09-16zf：**thcrap codecave 取证**（头号崩溃第二阶段：EIP 落在一张全 0 的页上）。
 *
 * ## 一手事实（`crashdumps/` 解析，全文见 `AUDIT.md` §DN）
 *   · 崩溃 dump 里 **EIP = `0x03c1065a` / `0x03c2065a`**（两次运行都在 `0x03c?065a`，页基不同），
 *     **不在任何模块内**；dump 把该页 256 字节捕了下来 ⇒ **全是 0**。
 *   · `00 00` 解码 = `add byte ptr [eax], al` ⇒ 现场 `Eax = 1` ⇒ **写到地址 `0x1`** ⇒
 *     这就是那条「`0xC0000005` **写** `0x1` + `Eax = 1` + EIP 在堆」的签名**逐字解释**（不是"堆数据当指令"，
 *     是**执行一张零页**）。页是已提交且可读可执行的（否则会先取指失败），只是内容为 0。
 *   · 第二次（`34076`）改崩在 `do_bomb+0x8c`（`th18:0x4203ec` `call [eax+4]`，`eax = 0`），
 *     而该帧的**返回地址 = `0x03c10345`** —— 同一个 `0x03c1xxxx` 区。⇒ 这个区是**被当作代码用的**。
 * ## 这个区是什么
 *   `0x03c?xxxx` 页对齐、低地址、不属于任何模块 = **thcrap 的 codecave 区**（codecave 用 `VirtualAlloc`）。
 *   本 mod 栈里用 codecave 的：**我们自己的 211 条 binhack 里那 11 个 codecave**（`th18_card_table`
 *   0x33cc 字节的大表拷贝等），以及 **`nmlgc/base_tsa` 的 `th18_centipede_fix`**（其站点 = `th18:0x412c76`，
 *   `cavesize 10`，正好是 `AbilityManager::allocate_new_card` 里的 `mov [0x4cf2d4],0` —— **和我们的卡表搬迁
 *   改的是同一个函数**）。
 * ## 于是要回答两个问题（本段代码就是回答它们的）
 *   ① 我们每个站点的 cave 在哪、**首字节是不是 0**（thcrap 没写进去 = 一命中就执行零页）；
 *   ② 运行期这些 cave 的头部**会不会被改写/清零**（谁在写 = 下一轮要抓的人）；外加两两重叠检测。
 * 结论口径：`cave[名字]: 站点 -> cave 首 8 字节 = …`（首见一行/站点）；运行期变化打 `★★ CAVE CLOBBERED`。*/

static int ce_addr_exec(uint32_t a);              /* 定义在下面（`VirtualQuery` 判页保护）；这里先声明 */

/* `nmlgc/base_tsa` 的 `th18_centipede_fix` codecave 站点（RVA；一手 = 该 patch 的 js：
 * `"breakpoints": { "codecave:th18_centipede_fix": {"addr": "Rx12c76", "cavesize": 10} }`）。*/
#define CE_BS_CAVE_SITE_RVA 0x12c76u

#define CE_CAVE_SLOTS 48
#define CE_CAVE_WATCH 8

typedef struct {
    const char *name;
    uint32_t    site, cave;
    uint8_t     head[CE_CAVE_WATCH];
    uint8_t     live;
} ce_cave_t;

static ce_cave_t s_caves[CE_CAVE_SLOTS];
static unsigned  s_cave_n;
static unsigned  s_cave_zero;       /* 首见就全 0 的 cave 数（thcrap 没写进去）*/
static unsigned  s_cave_ovl;        /* 两两重叠的 cave 对数 */
static unsigned  s_cave_clobber;    /* 运行期首 8 字节被改写的次数 */
static unsigned  s_cave_ticks;      /* 看门狗跑过的帧数（日志里当帧号用）*/

/* thcrap 把站点改成 `e8/e9 rel32`（跳/调到 cave）⇒ cave = site + 5 + rel32。
 * 站点上不是 e8/e9 = 没打上或形态不同 ⇒ 返回 0（不猜）。*/
static uint32_t ce_cave_from_site(uint32_t site_va)
{
    const uint8_t *p = (const uint8_t *)(uintptr_t)site_va;
    uint32_t rel;

    if (!site_va || IsBadReadPtr((const void *)p, 5)) return 0;
    if (p[0] != 0xe8 && p[0] != 0xe9) return 0;
    rel = (uint32_t)p[1] | ((uint32_t)p[2] << 8) | ((uint32_t)p[3] << 16) | ((uint32_t)p[4] << 24);
    return (uint32_t)(site_va + 5u + rel);
}

static void ce_cave_note(const char *name, uint32_t site_va)
{
    ce_cave_t *c;
    unsigned i, zeros = 1;
    char hex[3 * CE_CAVE_WATCH + 1];
    char *q = hex;

    if (s_cave_n >= CE_CAVE_SLOTS) return;
    c = &s_caves[s_cave_n++];
    c->name = name; c->site = site_va; c->cave = ce_cave_from_site(site_va); c->live = 0;
    if (!c->cave) {
        ce_log("cave[%s]: 站点 %08x 上不是 e8/e9 ⇒ 没解出 cave（thcrap 没打/形态不同）", name, site_va);
        return;
    }
    if (IsBadReadPtr((const void *)(uintptr_t)c->cave, CE_CAVE_WATCH)) {
        ce_log("cave[%s]: 站点 %08x -> cave %08x ★ 首 %u 字节读不了（页没提交？）",
               name, site_va, c->cave, CE_CAVE_WATCH);
        return;
    }
    for (i = 0; i < CE_CAVE_WATCH; ++i) {
        c->head[i] = *(const uint8_t *)(uintptr_t)(c->cave + i);
        if (c->head[i]) zeros = 0;
        q += sprintf(q, "%02x ", c->head[i]);
    }
    c->live = 1;
    if (zeros) ++s_cave_zero;
    ce_log("cave[%s]: 站点 %08x -> cave %08x 首 %u 字节 = %s%s%s",
           name, site_va, c->cave, CE_CAVE_WATCH, hex,
           zeros ? "★★★ 全 0（命中即执行零页！）" : "",
           ce_addr_exec(c->cave) ? "" : " ★ cave 页不可执行");
}

/* 两两重叠检测。⚠ 2026-09-16zg 订正：thcrap 给每个 cave **固定 0x30 字节**（本次实测：
 * `0x03d30540 / 570 / 5a0 / 5d0 / 600 / 630 / 660 / 690 / 6c0 / 6f0 / 720 …` 恰好 0x30 一格），
 * 所以「相差 0x30/0x60…」是**正常紧密排布**，不是重叠 —— 判据必须比 0x30 更严。
 * 真正要抓的只有「两个 cave 落进同一个 0x30 槽」这一种。*/
static void ce_cave_overlap(void)
{
    unsigned i, k;
    for (i = 0; i < s_cave_n; ++i)
        for (k = i + 1; k < s_cave_n; ++k) {
            uint32_t a = s_caves[i].cave, b = s_caves[k].cave;
            if (!a || !b) continue;
            if (b > a && b < a + 0x30u) {
                ++s_cave_ovl;
                if (s_cave_ovl <= 10)
                    ce_log("★★ CAVE OVERLAP #%u: [%s] %08x 与 [%s] %08x 落进同一格（差 %d 字节）",
                           s_cave_ovl, s_caves[i].name, a, s_caves[k].name, b, (int)(b - a));
            }
        }
}

/* 每帧看门狗（`BP_ce_item_frame` 里调）：首 8 字节与首见快照不同 ⇒ 打一行（含帧号）。*/
static void ce_cave_watch(void)
{
    unsigned i, k;

    ++s_cave_ticks;
    for (i = 0; i < s_cave_n; ++i) {
        ce_cave_t *c = &s_caves[i];
        if (!c->live || IsBadReadPtr((const void *)(uintptr_t)c->cave, CE_CAVE_WATCH)) continue;
        for (k = 0; k < CE_CAVE_WATCH; ++k)
            if (*(const uint8_t *)(uintptr_t)(c->cave + k) != c->head[k]) {
                ++s_cave_clobber;
                if (s_cave_clobber <= 20)
                    ce_log("★★ CAVE CLOBBERED #%u: [%s] cave %08x 第 %u 字节 %02x -> %02x（帧 %u）"
                           "—— 这个 cave 的代码被谁改了/清零了",
                           s_cave_clobber, c->name, c->cave, k, c->head[k],
                           *(const uint8_t *)(uintptr_t)(c->cave + k), s_cave_ticks);
                for (k = 0; k < CE_CAVE_WATCH; ++k)
                    c->head[k] = *(const uint8_t *)(uintptr_t)(c->cave + k);
                break;
            }
    }
}

/* `0x03c00000`..`0x03e00000` 的内存地图（一手 `VirtualQuery` 走一遍；只读、不碰任何保护）。
 * 目的：给「EIP 落进去的那张零页」**定性** —— 三次崩溃的地址都落在这个窗口
 * （`0x03c1065a` / `0x03c2065a` / `0x03c10345`）。判读：
 *   · `MEM_PRIVATE` + `PAGE_EXECUTE*` ⇒ thcrap 的 codecave（那就要看它的首字节是不是 0）；
 *   · `MEM_PRIVATE` + `RW` ⇒ 堆 / 线程栈（未用到的栈页天然是 0）；
 *   · `MEM_FREE` ⇒ 根本没提交（那 EIP 落进去会先取指失败，与本签名不符）。*/
/* ★ 2026-09-17（用户口径：把站点预算闸定下来）—— 把"闸的上限"变成**每次运行可测**：
 * 数一遍跳板池里**实际装了多少格**。判据与 `tooling/poolmap.py` 完全一致：
 *   · 格 = `0x10` 步长；格内含 `e9`；且 `rel32` 目标**落在游戏影像内**（回跳）。
 * 只读、无副作用；开机自检里打一行。离线可用 poolmap.py 复核同一份日志，两边应当吻合。*/
static void ce_pool_count(uint8_t *base)
{
    uint32_t a = 0x03c00000u, end = 0x03e00000u;
    uint32_t lo = (uint32_t)(uintptr_t)base, hi = lo + 0x100000u;
    unsigned pages = 0, used = 0, first_zero = 0;
    MEMORY_BASIC_INFORMATION mbi;

    while (a < end) {
        uint32_t b, sz, o;
        if (VirtualQuery((LPCVOID)(uintptr_t)a, &mbi, sizeof mbi) != sizeof mbi) break;
        b = (uint32_t)(uintptr_t)mbi.BaseAddress;
        sz = (uint32_t)mbi.RegionSize;
        if (!sz) break;
        if (mbi.State == MEM_COMMIT && (mbi.Protect & 0xffu) == PAGE_EXECUTE
            && !IsBadReadPtr((const void *)(uintptr_t)b, 16)) {
            ++pages;
            for (o = 0; o + 16u <= sz; o += 16u) {
                const uint8_t *c = (const uint8_t *)(uintptr_t)(b + o);
                int j, hit = 0;
                for (j = 0; j + 5 <= 16; ++j) {
                    if (c[j] == 0xe9) {
                        int32_t rel;
                        uint32_t tgt;
                        memcpy(&rel, c + j + 1, 4);
                        tgt = (uint32_t)(b + o + (uint32_t)j + 5u) + (uint32_t)rel;
                        if (tgt >= lo && tgt < hi) { hit = 1; break; }
                    }
                }
                if (hit) ++used;
                else if (!first_zero) {
                    unsigned k, nz = 0;
                    for (k = 0; k < 16u; ++k) nz |= c[k];
                    if (!nz) first_zero = b + o;
                }
            }
        }
        a = b + sz;
    }
    ce_log("pool: 可执行页 %u、跳板格 %u、首个全零格 @%08x"
           "（扫 0x03c00000..0x03e00000；判据 = 0x10/格 + e9 回跳落在影像内，与 tooling/poolmap.py 同）",
           pages, used, first_zero);
}

static void ce_mem_map_dump(void)
{
    uint32_t a = 0x03c00000u;
    const uint32_t end = 0x03e00000u;
    MEMORY_BASIC_INFORMATION mbi;
    unsigned n = 0;

    while (a < end && n < 64) {
        uint32_t b, sz;
        const char *state, *type;
        char hex[3 * 8 + 1];
        char *q = hex;
        unsigned i;

        if (VirtualQuery((LPCVOID)(uintptr_t)a, &mbi, sizeof mbi) != sizeof mbi) break;
        b  = (uint32_t)(uintptr_t)mbi.BaseAddress;
        sz = (uint32_t)mbi.RegionSize;
        if (!sz) break;
        state = mbi.State == MEM_COMMIT ? "COMMIT" : (mbi.State == MEM_FREE ? "FREE" : "RESERVE");
        type  = mbi.Type == MEM_IMAGE ? "IMAGE" : (mbi.Type == MEM_MAPPED ? "MAPPED" : "PRIVATE");
        hex[0] = '\0';
        if (mbi.State == MEM_COMMIT && !IsBadReadPtr((const void *)(uintptr_t)b, 8)) {
            for (i = 0; i < 8; ++i) q += sprintf(q, "%02x ", *(const uint8_t *)(uintptr_t)(b + i));
        } else {
            strcpy(hex, "-");
        }
        ce_log("memmap %08x..%08x %-7s %-7s prot=%#08x 首 8 字节 = %s",
               b, b + sz, state, type, (unsigned)mbi.Protect, hex);
        /* ★ 2026-09-16zg：**把可执行页整段 dump 出来**。三次崩溃都落在这一带的一张
         * `PAGE_EXECUTE` 页上、且**偏移恒为 `+0x65a`**（页基每次不同：`0x03c10000` / `0x03c20000` /
         * `0x03d20000`）⇒ 那张页是**定长布局的代码块**，其中 `+0x65a` 处是 0（未写）。
         * 崩溃帧的**返回地址 = 同一张页的 `+0x345`** ⇒ **是这张页里的代码 `call` 到了它自己的零尾巴**。
         * 把页内容（前 `0x800` 字节）原样打进日志 ⇒ 离线用 capstone 反汇编就能看到 `+0x340` 那条 `call`
         * 的 rel32 到底指向哪、以及这张页是谁生成的（首字节 `89 44 24 20 6a 2c e9 …` =
         * `mov [esp+0x20],eax; push 0x2c; jmp …` = thcrap 的**内部钩子 thunk** 形态）。*/
        if (mbi.State == MEM_COMMIT
            && (mbi.Protect & 0xffu) == PAGE_EXECUTE
            && !IsBadReadPtr((const void *)(uintptr_t)b, 16)) {
            uint32_t lim = sz < 0x800u ? sz : 0x800u, o;
            for (o = 0; o < lim; o += 16) {
                char row[3 * 16 + 1];
                char *qq = row;
                unsigned j;
                for (j = 0; j < 16 && o + j < lim; ++j)
                    qq += sprintf(qq, "%02x ", *(const uint8_t *)(uintptr_t)(b + o + j));
                ce_log("  page %08x+%03x: %s", b, o, row);
            }
        }
        a = b + sz;
        ++n;
    }
    ce_log("memmap: 0x03c00000..0x03e00000 走了 %u 段", n);
}

/* ★ 2026-09-12 十三修：卡对象 / AbilityManager 完整性探针。
 *
 * 用户报告的「卡又成滚木 / 撞到槽里 slot 消失」与更早 id=254（0xfe，有符号字节为负）症状同型，
 * 说明不是纯美术索引，而是卡对象或 manager 计数被写坏。这个探针在**每个正常游玩关卡开场**
 * 沿卡链表 dump 每张卡的 id / flags，并核对 `num_total` 与 `owned[128..254]`；之后每帧只在
 * 发现异常（id 越界、链表长度 != num_total、owned 高位非零）时再打一行。下一份用户日志可直接
 * 证明是「id 污染 / manager 坏」还是「纯美术」。零写入、纯只读；只在 ItemManager tick 帧界跑。 */
static int32_t s_integrity_stage = (-0x7fffffff - 1);
static void ce_integrity_check(void)
{
    uint8_t *mgr = CE_ABILITY_MGR();
    const int32_t *owned;
    int32_t stage, total, active, equip, passive;
    unsigned list_n = 0, guard = 0, id;
    int bad = 0, owned_hi = 0, first;

    if (!mgr) return;
    stage = CE_CURRENT_STAGE();
    first = (stage != s_integrity_stage);
    if (first) s_integrity_stage = stage;

    total   = *(const int32_t *)(mgr + CE_MGR_NUM_TOTAL);
    active  = *(const int32_t *)(mgr + CE_MGR_NUM_ACTIVE);
    equip   = *(const int32_t *)(mgr + CE_MGR_NUM_EQUIPMENT);
    passive = *(const int32_t *)(mgr + CE_MGR_NUM_PASSIVE);
    owned   = CE_OWNED_ARRAY();
    for (id = 128; id <= CE_SDK_CARD_ID_MAX; ++id)
        if (owned[id] != 0) ++owned_hi;

    for (const uint8_t *node = *(const uint8_t *const *)(mgr + CE_MGR_CARD_LIST_FIRST);
         node && guard < 256;
         node = *(const uint8_t *const *)(node + CE_NODE_NEXT), ++guard) {
        uint8_t *card = *(uint8_t *const *)(node + CE_NODE_CARD);
        uint32_t cid, flags;
        if (!card) continue;
        cid = ce_card_id(card);
        flags = ce_card_flags(card);
        if (first)
            ce_log("integrity[stage %d]: card[%u] id=%u flags=%08x", stage, list_n, cid, flags);
        else if (cid > 127u)
            ce_log("integrity[stage %d]: HIGH-ID card[%u] id=%u flags=%08x", stage, list_n, cid, flags);
        if (cid == 0 || cid > CE_SDK_CARD_ID_MAX) {
            bad = 1;
            ce_log("integrity[stage %d]: BAD card[%u] id=%u flags=%08x", stage, list_n, cid, flags);
        }
        ++list_n;
    }
    if ((int32_t)list_n != total) {
        bad = 1;
        ce_log("integrity[stage %d]: list length %u != CE_MGR_NUM_TOTAL %d", stage, list_n, total);
    }
    if (first || owned_hi || bad)
        ce_log("integrity[stage %d]: list=%u total=%d active=%d equip=%d passive=%d owned[128..254]=%u bad=%d",
               stage, list_n, total, active, equip, passive, owned_hi, bad);
    if (bad)
        ce_log("integrity[stage %d]: CARD MANAGER INTEGRITY FAILURE — 请把以上行贴给维护者", stage);
}

int __cdecl BP_ce_item_frame(x86_reg_t *regs, void *bp_info)
{
    ce_site_trail("ce_item_frame");
    /* ★ 2026-09-13：本帧点也是 **SDK 帧号**的唯一递增处（跨卡共用模块的「每帧只推进一次」守卫用它，
     *   例 `faith_pool_tick()` 被 110/111 各调一次；见 `sdk_core.h` 的 `ce_sdk_frame`）。*/
    ce_site_trail("if/ce_sdk_frame_bump");
    ce_sdk_frame_bump();
    ce_site_trail("if/ce_cave_watch");
    ce_cave_watch();                          /* ★ 2026-09-16zf：codecave 看门狗（每帧一次，只读）*/
    /* ★ 站点自检门 ⑤（2026-09-18，**只记账**）：炸弹管理器指针的「有 → 无」边沿。
     *   `CE_ADDR_BOMB_MGR` = `th18:0x4cf2b8`（一手：`Bomb::operator new` @ `th18:0x41ff73` 是唯一写点、
     *   `+0x30` = 正在放炸弹）。历史上有一次崩在 `do_bomb+0x8c` 的 `call [eax+4]`、`[BOMB_PTR]` 的虚表为 0
     *   （`GATES.md` §8.2 / `local/MEMORY.md` §1.3t-u 的待办：**每帧窥视首 dword，变 0 就当场打行**）。
     *   本门就是那条待办：只在「本来非 0、这一帧变 0」时记一行（含帧号），行为不变。*/
    {
        static uint32_t s_bomb_prev;
        uint32_t bomb_now = *(volatile uint32_t *)(uintptr_t)CE_ADDR_BOMB_MGR;
        if (s_bomb_prev && !bomb_now)
            ce_log("★ bomb ptr 归零（帧 %u）：BOMB_PTR 0x%08x -> 0（历史崩点 do_bomb+0x8c 的前置条件）",
                   (unsigned)ce_sdk_frame(), (unsigned)s_bomb_prev);
        s_bomb_prev = bomb_now;
    }
    /* ★★★ 2026-09-23（108 #33/#38「退出 shift 后原生子机在进 shift 的位置发弹、本体消失」）：
     *   **隐藏期收掉过原生子机 VM ⇒ 可见化之后必须补一次 `repopulate`**，否则它们永远回不来。机制逐条：
     *     · 隐藏那一侧（`BP_ce_repopulate_native_option` `0x45d610` + 卡侧每帧的 `ce_native_option_sweep()`）
     *       把 `lvl` 压成 0（**不建**）**并**把已建的那对 VM `interrupt` 掉、id 写 0、状态写 0；
     *     · 引擎**只在火力档变化时**才叫 `Player__repopulate_options_and_notify_cards` `0x45d5e0`
     *       （就是 sdk.h 里那句「档位短路缓存」）⇒ 松开 Shift 只是「不再隐藏」，**没有任何人叫重建**
     *       ⇒ 4 格子机的记录还在、主炮行循环照旧从**看不见的膛口**出弹（`BP_ce_native_option_muzzle`
     *       只在隐藏期拦），但 VM 一个都回不来 = owner 报的「**发弹但不显示、停在进 shift 那一处**」。
     *   · 修法 = 在可见化之后替引擎补一次 `ce_repopulate_options()`（按当前档重建 + 广播
     *     `on_power_level_change`，与「卡扣完火力必须补的那一次」同一个调用）。
     *
     * ★★ 2026-09-23（owner：**快速高低切不能出显著的 bug**）—— 这一段必须对「高频切换」稳，故三条约束：
     *   ① **真杀过才补**：判据是 `s_natopt_swept`（由 `ce_native_option_sweep()` 在**真的收掉 VM** 时立起）。
     *      快速连按时若这一轮 hide 没来得及收掉任何 VM，就**一次都不重建** ⇒ 不产生任何抖动；
     *   ② **冷却 = 20 帧**（≈0.33 s）：连按时重建最多 3 次/秒，绝不按帧触发 —— 重建会**广播
     *      `on_power_level_change`**，而 116（`season.c`）等卡在那个回调里会**重分子机**
     *      ⇒ 按帧广播会把别人的子机池搅乱（那才是「显著的 bug」）；冷却 + 真杀过两条一起把这条路封住；
     *   ③ **按机体判**（`ce_native_option_hidden(当前机体)`）+ 只在**可见态**补 ⇒ 与层本身的机体门口径一致，
     *      换机体当帧就能自愈。
     *   ⚠ 写 `CE_OFF_OPT_LAST_LEVEL = -1` 是**必要的**：引擎那个「上次按哪一档建过」的缓存会让
     *     `repopulate` 直接短路返回（一手读点 `0x45d84d` `cmp [esi+0x476f8],eax`；写 -1 一定重走重建/清空）。
     *   ⚠ 与卡池无关：原生 4 格 = `inner+0x50`（`PLAYER+0x670` 那一族），卡申请的是 `inner+0x410`
     *     ⇒ 这次重建**不动** 108/111/116/117 手里已申请的子机（两个数组互不相干，一手见上段）。*/
    {
        static uint32_t s_natopt_fix_frame;
        const int ch_now = *(const int32_t *)CE_ADDR_CHARACTER;
        const int hid_now = ce_native_option_hidden(ch_now);
        const uint32_t f_now = ce_sdk_frame();
        /* ★★★ 2026-09-26：**还在扫就别补**（这是「子机一直在重建」的真根因）。
         *   实跑日志（本机 `game/th18_card_expand.log`，char 2 = 咲夜）：
         *     `native options restored (char 2, …, frame 13078/13754/13834/13888/13908/13928/…)`
         *     —— 从 13908 起**每 20 帧一次**（= 下面那个冷却常数），每次后面紧跟四行
         *     `youkai: option allocated (ptr …, anm id …)`（同一个 ptr、**新的** anm id ⇒ VM 被删重建）。
         *   机制：108 在低速每帧调 `ce_native_option_sweep_keep_state()`（杀原生子机的显示，
         *   但 `state`/坐标保留）⇒ 真收掉过 ⇒ `s_natopt_swept = 1`；而 `hid_now` 对**没声明隐藏的机体**
         *   （咲夜/灵梦/早苗）恒为 0 ⇒ 这一段在**仍然按着 Shift**的时候就判「隐藏期结束了」⇒
         *   `ce_repopulate_options()` 把原生那 4 格重建（VM 又长出来）⇒ 我们下一帧再扫掉 ⇒ 旗子又立起
         *   ⇒ **20 帧一轮的自持循环**；而那次重建会**广播 `on_power_level_change`**（引擎的语义），
         *   108 在那个回调里会把手里子机的指针清掉 ⇒ 四台子机被重建 ⇒ owner 看到的「一直在重建」。
         *   （附带伤害：这个循环每 20 帧给**所有**卡广播一次 `on_power_level_change`，116 等卡会在里面
         *     重分母机 ⇒ 不止 108 受影响。）
         *   ⇒ 判据补一条：**本帧（或最近几帧）还有人在扫 ⇒ 不补**。停手之后（松开 Shift）才补，
         *     语义与原来的「隐藏期结束」完全一致，只是不再被自己的 keep_state 扫描误触发。*/
        if (!hid_now && s_natopt_swept &&
            (s_natopt_sweep_frame == 0u || f_now - s_natopt_sweep_frame >= SDK_NATOPT_SWEEP_QUIET) &&
            (s_natopt_fix_frame == 0u || f_now - s_natopt_fix_frame >= 20u)) {
            s_natopt_swept = 0;
            s_natopt_fix_frame = f_now ? f_now : 1u;
            /* ★★★ 2026-09-27（§BE #120）：**原来这里写错地址了** —— 引擎的短路缓存是
             *   `player+0x476f8`（一手 `0x45d84d cmp [esi+0x476f8],eax / je`，`esi = [0x4cf410]`），
             *   而旧代码写的是 `GlobalsInner + 0x470d8`（0x514db4，引擎根本不读）⇒ 这次
             *   `ce_repopulate_options()` **直接短路返回**，原生子机的 VM 一个都没重建回来
             *   ⇒ owner 的「人子机消失、发弹点正常」（记录在、所以膛口照发；VM 没了、所以看不见）。*/
            {
                uint8_t *pl2 = CE_PLAYER();
                if (pl2) *(volatile int32_t *)(pl2 + CE_PLAYER_OPT_LAST_LEVEL) = -1;
            }
            ce_repopulate_options();
            ce_log("sdk: native options restored (char %d, owner card %d, frame %u) - swept VMs were "
                   "rebuilt (last-level cache reset first); this fires only after a hide that really "
                   "swept something, at most once per 20 frames",
                   ch_now, ce_native_option_hide_owner(), (unsigned)f_now);
        }
    }
    (void)regs; (void)bp_info;
    uint8_t *mgr = CE_ABILITY_MGR();
    if (!mgr) return BP_EXEC_ORIGINAL;
    ce_site_trail("if/ce_integrity_check");
    ce_integrity_check();
    /* ★ 帧界：低速子机声明层的落笔 + 清集合（spec §3.3.1）。顺序 **先落笔、后清**：
     * 落笔用的是「上一次清集合以来」的全部声明（= 本帧各卡 tick 里刚发的），清完之后本帧的声明从零收集。*/
    ce_site_trail("if/ce_option_pose_frame_tick");
    /* ⚗ 2026-09-16zo 二分（**临时**）：把「阵型几何落笔」这一步停掉 —— 它是本 handler 里**唯一会写引擎侧字段**
     * 的一步，而崩点被夹在「这一步之后、ItemManager tick 返回之前」。停掉后若不崩 ⇒ 就是它写坏字段；
     * 若照旧崩 ⇒ 这一步无罪，把刀口挪到别的写点。
     * ⚠ 副作用：111 的子机不再按 TH11 阵型几何逐帧落笔（位置不跟随阵型）——只用于定责。*/
    ce_option_pose_frame_tick();   /* ✅ 2026-09-16zq 恢复：上一刀已证明它无罪（停用后照旧崩）*/
    ce_site_trail("if/ce_optpose_frame_reset");
    ce_optpose_frame_reset();
    /* ★ 帧初：清「本帧已消过弹」闩锁（EVENTS-CONTRACT §5.2 细则 2）。这个断点每个正常游玩帧到一次，
     * 且在道具池 body 之前 ⇒ 是天然的帧边界。107 的结界灵击与 117 的霊撃同帧都触发时，
     * 只有第一个 claim 的负责全屏消弹，第二家让位（避免空池扫荡 / 重复记账）。*/
    ce_cancel_latch_reset();
    /* ★★★ 2026-10-03q（owner：「所有左下角 HUD 应当是**一块 HUD**」）：整条带的**共享淡化**
     *   每帧算一次（矩形 = 所有已认领块的并集 ∪ 原生卡面）⇒ 各卡读 `ce_hud_band_dim_alpha()`。*/
    ce_site_trail("if/ce_hud_band_dim_tick");
    {   /* ★ 2026-10-03r：带的淡化**翻转**时打一行（不刷屏；配合 `ce_hud_slot_log_all` 看整表）*/
        static int s_dim_was = -1;
        const int dim_before = ce_hud_band_dim_dimmed();
        int dim_now;
        /* ★★★★ 2026-10-03z35（owner：「**bug：hud 布局跨游玩记忆**，你看 log 就知道了」）——
     *   症结：槽位表的唯一重置判据是 **`ce_hud_stage_begin()` 的"关卡号变化"**
     *   （`sdk_core.c`：`if (stage == s_hud_stage) return;`）⇒
     *   **重开同一关（关卡号还是 1）不会重置** ⇒ 上一局整张占位表被新一局沿用 ✗
     *   （各卡又是"`ce_hud_slot_claimed(id)` 为真就沿用缓存槽"⇒ 一并沿用 ✗）。
     *   ⇒ 补一个**真正的一局/一关起点信号**：**自机对象从"无"变"有"**
     *     （`CE_PLAYER()` 在关卡里非空、菜单/结算里为空 ✓，而且是**引擎自己**的状态，
     *      不依赖关卡号）⇒ 那一刻把纪元打回 **−1**，下一帧各卡认领时按新一局重排 ✓。
     *   ⚠ 只认"无→有"这一条边沿：同一关里换面/暂停/商店都不会触发 ✓（自机一直在）。*/
    ce_hud_stage_reset_end_frame();   /* ★ z44：帧末重新武装"每关一次"的重置闸 */
    ce_hud_band_dim_tick(ce_player_pos());
    ce_hud_native_face_dim_tick();   /* ★ z50：原生主动卡与共享带**每帧**同步（安全写，见函数头）*/
    ce_bullet_scan_probe();          /* ★ z58：只读比对"活弹链表 vs 数组扫描"（第 1 步验证件）*/
    ce_hud_debug_blocks_tick();   /* ★ z17：把声明块画出来（debug；宏可关）*/
        dim_now = ce_hud_band_dim_dimmed();
        if (s_dim_was != dim_now) {
            ce_log("hud: band dim %s (alpha %d, was %d)", dim_now ? "ON" : "OFF",
                   (int)ce_hud_band_dim_alpha(), dim_before);
            s_dim_was = dim_now;
        }
    }
    /* ★★★ 2026-10-03z13（owner：「**看看能不能把原生的也统一一下压暗**，这个得调查一下了」）——
     *   **调查结论（一手，`engine/card/th18/15-selected-card-hud.md` §0/§4.2/§6）**：
     *   · 原生那块「当前主动卡」= `abcard.anm` **script0 的 5-VM 树**（根 + script1..4），
     *     **树根 id 在 `mgr+0x3c`**；子 VM 由脚本 `scriptNew` 紧跟着建 ⇒ id 连续（根+1..根+4）。
     *   · 引擎自己在 `AbilityManager__on_tick` `0x408640` 用**闸**压它：
     *     进 = `player.x < −64 ∧ player.y > 368` ⇒ `interruptLabel(5)`、出 = 反向 ⇒ `(4)`，
     *     **8 帧线性到 alpha 64**，闩锁 = `mgr+0x40`。
     *   ⚠ **不能只学它发 5/4**：它的闸每帧都会重新判，自机在我们带里但不在它那个小矩形里时
     *     它会**反向发 4** ⇒ 两边打架 = **每帧抖**（这条是调查里最容易踩的坑）。
     *   ⇒ **正解 = 直接写颜色 alpha**：那块 UI 的绘制是 **color_1 的 A × 脚本 alpha**（乘性）
     *     ⇒ 我们每帧把 5 个 VM 的 color A 写成**共享带**的 alpha ⇒ **跟着淡**，
     *     而引擎的闸**只会把它压得更暗、不会变亮** ⇒ **结构性不打架** ✓（动画/终点仍是一手的）。
     *   ⚠ 顺序无关（乘性），所以不需要抢在引擎 tick 之后写 ✓。*/
    {
        uint8_t *mgr = CE_ABILITY_MGR();
        if (mgr) {
            const int32_t root = *(const int32_t *)(mgr + CE_MGR_SEL_CARD_VM);
            /* ★ z19：直读诊断（每关一行）—— 上一版只在找到 VM 时才打，结果一条都没有，
             *   分不清"字段为 0"还是"VM 查不到"。实测：root 有效、get_vm(root) 非空 ✓
             *   ⇒ 所以问题在**写错了哪几支**（见下）。*/
            /* ★★★★ 2026-10-03z48（**两起崩溃之后，按正确姿势重做**：先只读、全程校验）——
             *   owner 确认「关掉它**不再崩**」⇒ 负对照成立 ⇒ 原扫描代码就是崩溃源 ✓。
             *   这次**一个字节都不写**，只把真相量出来：
             *     · `mgr+0x3c` 到底是 **id** 还是 **VM 指针**（两种解释都试：直接当指针 / `get_vm(id)`）；
             *     · 若那个 VM 可读，打它的 `layer(+0x18) / script_id(+0x28) / sprite_id(+0x24) /
             *       id(suffix +0x544) / flags`；
             *     · 再按命名金矿的 `zAnmVmSuffix.list_of_children`（**+0x50**）走一遍子链表，
             *       每个节点 `entry(=子 VM*)` 与子 VM 的脚本号都打出来。
             *   ⚠ **每一次解引用前都过 `IsBadReadPtr`**（本文件别处已在用 ✓）；循环上限 8 防环。*/
            {
                static int32_t s_nat_root_logged = -2;
                if (s_nat_root_logged != root) {
                    uint8_t *vm = 0;
                    s_nat_root_logged = root;
                    ce_log("hud: native face 只读探针 —— mgr+0x3c=%d, mgr+0x38=%d",
                           (int)root, (int)*(const int32_t *)(mgr + CE_MGR_SELECTED_ACTIVE));
                    /* ★ z57：**删掉"解释①（把 id 当指针）"** —— 实跑证明它会把垃圾地址
                     *   判成"可读"（`layer=283705345` 那种）⇒ 只会误导 ✓。
                     *   只保留正解：`get_vm(id)` + **身份校验**（id 必须相等 ✓）。*/
                    vm = (uint8_t *)ce_anm_get_vm((uint32_t)root);
                    if (vm && (IsBadReadPtr(vm + CE_ANM_VM_SUFFIX_ID, 4) ||
                               *(const int32_t *)(vm + CE_ANM_VM_SUFFIX_ID) != root))
                        vm = 0;
                    ce_log("hud:   解释②(get_vm)=%p%s", (void *)vm,
                           (vm && !IsBadReadPtr(vm, 0x60)) ? "（可读 ✓、id 校验通过 ✓）" : "");
                    if (vm) {
                        const int32_t sid = *(const int32_t *)(vm + CE_ANM_VM_SUFFIX_ID);
                        uint8_t *head = vm + CE_ANM_VM_CHILD_LIST;
                        int guard = 0;
                        ce_log("hud:   树根 VM=%p id(suffix+0x0)=%d parent(+0x7c)=%p", (void *)vm,
                               (int)sid,
                               IsBadReadPtr(vm + CE_ANM_VM_PARENT, 4)
                                   ? (void *)0 : (void *)*(uint8_t *const *)(vm + CE_ANM_VM_PARENT));
                        if (!IsBadReadPtr(head, 12)) {
                            uint8_t *node = *(uint8_t *const *)(head + 0x4);
                            while (node && node != head && guard++ < 8) {
                                if (IsBadReadPtr(node, 12)) { ce_log("hud:   子链节点 %p 不可读 ✗ 停", (void *)node); break; }
                                {
                                    uint8_t *child = *(uint8_t *const *)node;
                                    ce_log("hud:   子链[%d] node=%p entry=%p%s", guard - 1, (void *)node,
                                           (void *)child,
                                           (child && !IsBadReadPtr(child, 0x60))
                                               ? " 可读" : " 不可读/空");
                                    if (child && !IsBadReadPtr(child, 0x60))
                                        ce_log("hud:     子 VM script_id=%d sprite_id=%d layer=%d id=%d",
                                               (int)*(const int32_t *)(child + CE_ANM_VM_SCRIPT_ID),
                                               (int)*(const int32_t *)(child + CE_ANM_VM_SPRITE_ID),
                                               (int)*(const int32_t *)(child + CE_ANM_VM_LAYER),
                                               (int)*(const int32_t *)(child + CE_ANM_VM_SUFFIX_ID));
                                    node = *(uint8_t *const *)(node + 0x4);
                                }
                            }
                            ce_log("hud:   子链结束：共走 %d 个节点%s", guard,
                                   (guard >= 8) ? "（达上限 8，可能还有）" : "");
                        } else {
                            ce_log("hud:   子链表头 %p 不可读 ✗（说明 +0x50 不是子链表）", (void *)head);
                        }
                        /* ★★★★ 2026-10-03z49（**只读探针实测通过后，才动写**）——
                         *   实跑数据（`hud: native face 只读探针`）：
                         *     树根 id 与 `mgr+0x3c` **一致** ✓、`parent=0` ✓（它是根 ✓）、
                         *     子链 **4 个节点全可读**，`script_id` = **2 / 1 / 4 / 3**、`layer` 24/25 ✓
                         *     ⇒ **正是脚本 1..4 那四支** ✓；且它们 id **既不连续也不等差**
                         *     （13139815 / 13107048 / 13074281 / 13041514）⇒ 这就是前两次"猜步长"必败的原因 ✓。
                         *   ⇒ 写：走**同一条**子链，**每一步都先 `IsBadReadPtr` 校验**，
                         *     直接写**已验证的** `child + CE_ANM_VM_COLOR1`（`color_1` 是 **prefix** 字段 ✓），
                         *     只改 A、保留 RGB ⇒ 乘性 ⇒ 与引擎自己的闸只叠加、不互相取消 ✓。
                         *     上限 8 个节点、不追子节点的子节点（那棵树就一层 ✓）。*/
                        {
                            uint8_t *head2 = vm + CE_ANM_VM_CHILD_LIST;
                            if (!IsBadReadPtr(head2, 12)) {
                                uint8_t *node2 = *(uint8_t *const *)(head2 + 0x4);
                                int guard2 = 0, nw = 0;
                                const uint32_t a2 = (uint32_t)ce_hud_band_dim_alpha() & 0xffu;
                                while (node2 && node2 != head2 && guard2++ < 8) {
                                    uint8_t *ch;
                                    if (IsBadReadPtr(node2, 12)) break;
                                    ch = *(uint8_t *const *)node2;
                                    if (ch && !IsBadReadPtr(ch, CE_ANM_VM_COLOR1 + 4)) {
                                        const uint32_t old = *(const uint32_t *)(ch + CE_ANM_VM_COLOR1);
                                        *(uint32_t *)(ch + CE_ANM_VM_COLOR1) =
                                            (a2 << 24) | (old & 0x00ffffffu);
                                        ++nw;
                                    }
                                    node2 = *(uint8_t *const *)(node2 + 0x4);
                                }
                                {
                                    static int32_t s_nat_wr_logged = -2;
                                    if (s_nat_wr_logged != root) {
                                        s_nat_wr_logged = root;
                                        ce_log("hud: native face 耦合写入 %d 支（band alpha=%u；"
                                               "只改 A、保留 RGB，乘性叠加）", nw, (unsigned)a2);
                                    }
                                }
                            }
                        }
                    }
                }
            }
            /* ★★★★ 2026-10-03z47（**两起崩溃**：`th18.exe.86452.dmp`(16:27) / `.88004.dmp`(19:38)）——
             *   两次日志都**恰好停在上面那行探针之后**（日志到此为止），而"耦合"那行**一次都没打出来**
             *   ⇒ 崩在**扫描 / 子链表遍历**里（读/写任意地址）✗。
             *   时间也对得上：16:27 = stride 版（`root ± k×32767`）、19:38 = 子链表版（`rvm + 0x50`）
             *   ⇒ **两次崩溃都是这一段**。
             *   ⇒ **本轮先止血：整段写路径删掉**（只留上面那行探针 —— 它已证明是安全的）。
             *   下次调查的正确姿势（写在这里免得再犯）：
             *     · 每次解引用前先校验可读（`IsBadReadPtr(ptr, len)` / SEH 包裹）；
             *     · **先只读不改**，把链表的实际形态（节点数 / `entry` / 脚本号）打出来，确认无误再谈写；
             *     · 并且把"关掉它不崩"当作**负对照**先验收 ✓。
             *   ⚠ 教训：这两版都是"推理 → 直接用"（`mgr+0x3c` 是 id 还是指针、子节点在哪都没证实）
             *     ⇒ 涉及**引擎指针**的新代码必须先做只读探针。*/

        }
    }
    /* ★ 帧初：清「这次死亡由谁救」闩锁（用户 2026-09-12 定案：救援层 107 → 117 → 决死 B → 113；
     * 113 只在没人认领时才动手 —— 否则会「别人救了你、113 的窗口结束时又把你杀掉」）。*/
    ce_rescue_note_reset();
    /* ★ 帧初：把「无敌声明层」的聚合值落到 `player+0x47774`（取各卡声明帧数的**最大值**，
     * 只在比当前计时器更长时才写 ⇒ 不截短别人给的无敌、也不动引擎自己的）。
     * 卡只需要 `ce_invuln_request(id, frames)` 声明，落笔由 SDK 统一做（单一落笔，EVENTS-CONTRACT §1）。*/
    ce_invuln_apply();
    /* ★ 2026-09-15：把「PoC 高度层」（收点线 `player+0x47998`）的聚合值落到字段上（每帧一次）。
     *   有声明 ⇒ 写 + 置 `Gui+0x194` bit `0x2000`（开那条开局指示线）；声明全撤 ⇒ 写回基准；都没有 ⇒ 一个字不写。
     *   ★ 本帧点**在道具池 body 之前**（见 `sites.py` 的 `ce_item_frame`）⇒ 引擎自己那四条
     *   「线上/线下」判据（`0x445baa` / `0x44698d` / `0x446bf9` 系）当帧读到的就是我们写进去的值。*/
    ce_poc_apply();
    /* ★ 帧初：推进「玩家贴近左下角 → HUD 整摞淡出」的迟滞（每帧**只推一次**；
     * 卡片读 `ce_hud_near_corner()` 即可，别自己调 —— 多张卡同帧各调会反复推进状态）。*/
    {
        const float *pp = ce_player_pos();
        if (pp) ce_hud_near_corner_update(pp[0], pp[1]);
    }
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
    ce_site_trail("ce_item_collect");
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
    ce_site_trail("ce_item_commit");
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
    /* ★ 2026-09-12：**先**发「不过滤聚焦」的那一份（on_shot_damage_any），再走老事件的策略门。
     *   老门（非聚焦 + 非 bomb）保持原样 ⇒ 114 的「非聚焦命中掉 P」语义零变化。*/
    {
        const int focused_any = CE_PLAYER_FOCUSED();
        const int bomb_any    = CE_BOMB_ACTIVE();
        if (!bomb_any) {
            const float *pos_any = *(const float *const *)(uintptr_t)(regs->ebx + 8);
            if (pos_any) {
                unsigned g2 = 0;
                for (const uint8_t *node = *(const uint8_t *const *)(mgr + CE_MGR_CARD_LIST_FIRST);
                     node && g2 < 256;
                     node = *(const uint8_t *const *)(node + CE_NODE_NEXT), ++g2) {
                    uint8_t *card = *(uint8_t *const *)(node + CE_NODE_CARD);
                    if (!card) continue;
                    const ce_hooks_t *h2 = hooks_of(card);
                    if (h2 && h2->on_shot_damage_any) {
                        h2->on_shot_damage_any((ce_card_t *)card, (int32_t)regs->esi, pos_any, focused_any);
                        ce_sdk_trace(ce_card_id(card), 0x88, "on_shot_damage_any");
                    }
                }
            }
        }
    }
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

/* ==========================================================================================
 * ★★★ 2026-09-27（卡 108 · owner「3 可以贴一手，**关键在站点写法**」）
 *   `BP_ce_bullet_hit` —— **「一发自机弹 × 一个敌机」的命中那一刻**（一手语义见下）
 * ==========================================================================================
 *
 * **一手（TH08 v1.00d，逐指令）**：那条规定写在**玩家侧**的伤害函数里 ——
 * `Player::compute_damage_to_enemy` 的 `th08:0x451860`–`:0x451892`：
 *   `451860 mov ecx,0x160f508 / 451865 call 0x406d10`  ⇒ **極端人間**问（假就整段跳过）
 *   `451871 mov eax,[bullet+0x480]` / `451877 movsx ecx,WORD [eax+0x1e]` / `45187d jge`（≥0 跳过）
 *   `45187f push 3` / `451881 push 7` / `451886 add edx,0x2a4`（= **弹的位置**）
 *   `451892 call 0x4400a0`（`ItemManager::spawn_item`；type 7 被 `0x440124` 强制成 state 3）
 * ⇒ **極人期间，每一发自机弹碰到敌机**就在**那颗弹的位置**掉 1 颗刻符（不是「击破」，是「命中」）。
 *
 * **TH18 为什么不能照抄**：TH18 的伤害不在弹里算 —— 弹只是往 `player+0x204d8` 的
 * 1024 槽**伤害源池**里放一条记录（`PlayerBullet__create` `0x45e861 call 0x45dfa0`，
 * 下标存 `bullet+0xb0`），由**敌人侧**每帧遍历（`enm_compute_damage_sources` `0x45f0f0`）。
 * ⇒ 等价事件 = 「**某个伤害源第一次命中某个敌人**」，本站点就钉在那个瞬间。
 *
 * **站点（`sites.py` 的 `ce_bullet_hit`，0x45f466 / 8 字节）为什么选这里**：
 *   · 走到 0x45f466 之前已经过 `0x45f45d je 0x45f25f`（**这个源这一辈子已经打过这个敌**）
 *     与 `0x45f437 jg 0x45f25f`（该源累计上限）⇒ 落在这里 = **这个源第一次命中这个敌**
 *     = 一发弹命中一次 ✓（正是一手的颗粒度）。
 *   · 0x45f458 的 `je 0x45f466` 只在 `esi == 0`（这次没带敌指针）时跳 ⇒ 用 `regs->esi != 0` 排除。
 *   · `esi` = 敌指针（本迭代 `0x45f254 mov esi,[ebx+0x20]` 刚重载，之后到本站点无人写）；
 *     `edi` = 伤害源槽 + 0x20（循环 `lea edi,[eax+0x20594]`、每轮 `add edi,0x9c`）⇒ 槽 = edi − 0x20。
 *   · **落点不需要另找弹**：弹的 tick 每帧把弹的当前坐标同步进它的源
 *     （`0x45efd2 movq xmm0,[bullet+0x48]` → `0x45efe3 movq [PLAYER+0x204f4+idx*0x9c],xmm0`）
 *     ⇒ 槽的 `+0x1c/+0x20` 就是**弹的位置**（一手撒的正是这里）。
 *
 * **「这一发是不是自机弹」怎么判**：只有弹把自己的源下标写在 `bullet+0xb0`（`CE_PB_DAMAGE_IDX`）
 * ⇒ 扫弹池（`PLAYER+0x1570`、`CE_PB_COUNT` × `CE_PB_STRIDE`、`+0x8c == 1` 活动）找 `+0xb0 == 本槽下标`。
 * ⚠ **不用**槽自己的 `+0x98`（特殊 handler 索引）判：弹把它写成 1（`0x45e890`），但两个 create
 * （`0x45de40` / `0x45dfa0`）**都不清它** ⇒ 槽被回收后可能残留、会把卡侧来源误判成自机弹。
 *
 * **开销**：两遍卡链表 —— 第一遍只看有没有任何卡声明 `on_bullet_hit`，没有就**整段早退**
 * （没装 108 的牌组：零开销，一次指针比较）；有声明才反推下标 + 扫弹池。
 * 返回 `BP_EXEC_ORIGINAL`：三条原指令在 cave 里原样回放，本站点**不改控制流、不写内存**。
 * ========================================================================================== */
int __cdecl BP_ce_bullet_hit(x86_reg_t *regs, void *bp_info)
{
    (void)bp_info;
    uint8_t *mgr = CE_ABILITY_MGR();
    if (!mgr) return BP_EXEC_ORIGINAL;
    if (!regs->esi) return BP_EXEC_ORIGINAL;        /* esi == 0 = 走的 0x45f458 那条「没敌」路 */
    if (!regs->edi || !CE_PLAYER()) return BP_EXEC_ORIGINAL;
    /* 第一遍：有没有人声明？（没声明就一个字都不读、不扫）*/
    {
        int want = 0;
        unsigned g0 = 0;
        for (const uint8_t *node = *(const uint8_t *const *)(mgr + CE_MGR_CARD_LIST_FIRST);
             node && g0 < 256;
             node = *(const uint8_t *const *)(node + CE_NODE_NEXT), ++g0) {
            uint8_t *card = *(uint8_t *const *)(node + CE_NODE_CARD);
            if (!card) continue;
            const ce_hooks_t *h = hooks_of(card);
            if (h && h->on_bullet_hit) { want = 1; break; }
        }
        if (!want) return BP_EXEC_ORIGINAL;
    }
    {
        const uint8_t *pl   = CE_PLAYER();
        const uint8_t *src  = (const uint8_t *)(uintptr_t)(regs->edi - 0x20);   /* 槽首 */
        const uint8_t *base = pl + CE_PLAYER_DMGSRC_BASE;                       /* = PLAYER+0x204d8 */
        ce_bullet_hit_t ev;
        size_t span = (size_t)CE_PLAYER_DMGSRC_SLOTS * CE_PLAYER_DMGSRC_STRIDE;
        if (src < base || src >= base + span) return BP_EXEC_ORIGINAL;          /* 纵深防御：不在池里 */
        {
            int32_t idx = (int32_t)((src - base) / CE_PLAYER_DMGSRC_STRIDE);    /* 1-based（槽 0 不用）*/
            const uint8_t *bpool = pl + CE_PB_POOL;
            int is_bullet = 0, hit_bullet = 0;
            if (idx > 0) {
                for (unsigned i = 0; i < CE_PB_COUNT; ++i) {
                    const uint8_t *b = bpool + (size_t)i * CE_PB_STRIDE;
                    if (*(const uint32_t *)(b + CE_PB_STATE) != 1u) continue;   /* 1 = 活动 */
                    if (*(const int32_t *)(b + CE_PB_DAMAGE_IDX) == idx) { is_bullet = 1; hit_bullet = i; break; }
                }
            }
            ev = (ce_bullet_hit_t){0};
            ev.index      = idx;
            ev.is_player_bullet = is_bullet;
            ev.bullet_i   = is_bullet ? (int32_t)hit_bullet : -1;   /* 第几槽（诊断用；-1 = 不是弹）*/
            ev.flags      = *(const uint32_t *)(src + 0x00);
            ev.damage     = *(const int32_t *)(src + 0x74);          /* 这一源的「每次结算伤害」*/
            ev.special    = *(const int32_t *)(src + 0x98);          /* 特殊 handler 索引（**别当判据**）*/
            ev.x          = *(const float *)(src + 0x1c);
            ev.y          = *(const float *)(src + 0x20);
            /* ★ 2026-09-27（第三十九批）：敌机位置 —— 长光源（激光/束）的源中心在**膛口**，
             *   卡侧要用它把落点换到真正的命中处（见 `public.h` 同名字段与 `yk_hit_orb_use_enemy_pos`）。
             *   ⚠ `esi` 是本迭代的敌指针（已在入口判非 0），这里只在回调内读一次。*/
            /* ★★★★ 2026-09-27 **崩溃修复**（第三十九批上线当天实跑崩在 `read 0x1274`，
             *   dump = `th18.exe.74956.dmp`：EIP = 本 DLL+0xfd1b = `movhps 0x1270(%eax),%xmm0`，
             *   `eax = 4`、params = `[0, 0x1274]`）：
             *   ⚠⚠ **`esi` 在这个站点不是敌指针，是「敌人 ID」** —— 调用方 `0x42f137`
             *   `push eax`（`eax = [[enemy+0x5588]+0x6830]`）= 第 7 个实参 = 那个单调递增的 id
             *   （= SDK 里 `CE_ENEMY_ID` 那一格）；`esi == 0` = 没有 id（一手 `0x45f456 test esi,esi /
             *   `:0x45f458 je` 就是「没 id 就不打这个「一源一敌」标记」）⇒ **绝不能拿它当指针解引用**。
             *   我上一版把它当敌指针读 `+0x1270` ⇒ 命中第 4 个敌人时读到地址 `0x1274` ⇒ 访问违例 ✓。
             *   ⇒ 敌机坐标改走 **arg1**（`[ebx+8]`，与 `BP_ce_shot_damage` 的 `pos` 同一个源：
             *     调用方 `0x42f12e lea edi,[ebx+0x44]` → `0x42f16d push edi`；函数内
             *     `0x45f138 mov [ebp-0x40],edx(=arg1)` ⇒ 几何判定就是拿它当 {x,y} 读的）。*/
            {
                uintptr_t fr = (uintptr_t)regs->ebx;                 /* ebx = 帧基（`mov ebx,esp`）*/
                const float *epos = 0;
                if (fr >= 0x10000u) epos = *(const float *const *)(fr + 8);   /* arg1 = &{x,y} */
                if (epos && (uintptr_t)epos >= 0x10000u) {
                    ev.ex = epos[0];
                    ev.ey = epos[1];
                } else {
                    ev.ex = ev.ey = 0.0f;                            /* 拿不到 ⇒ has_enemy_pos = 0 */
                }
                ev.has_enemy_pos = (epos && (uintptr_t)epos >= 0x10000u) ? 1 : 0;
            }
            ev.enemy_id   = (uint32_t)regs->esi;                     /* ★ 敌人 ID（**不是指针**）*/
            ev.enemy      = 0;   /* 本站点拿不到敌指针（只有 id）⇒ 一律 0，别当指针用 */
        }
        unsigned guard = 0;
        for (const uint8_t *node = *(const uint8_t *const *)(mgr + CE_MGR_CARD_LIST_FIRST);
             node && guard < 256;
             node = *(const uint8_t *const *)(node + CE_NODE_NEXT), ++guard) {
            uint8_t *card = *(uint8_t *const *)(node + CE_NODE_CARD);
            if (!card) continue;
            const ce_hooks_t *h = hooks_of(card);
            if (h && h->on_bullet_hit) {
                h->on_bullet_hit((ce_card_t *)card, &ev);
                ce_sdk_trace(ce_card_id(card), 0x8a, "on_bullet_hit");
            }
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
 * 收点 → 不续批（run-9：普通游玩环收把批撑过 5 → 误弹灰 NO BONUS）。TH14 原版 0x43898e 也只在
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
 * 原生只在两处删链表里的卡：局末 recount 0x4080e0（逐张：摘链 → vtable+0x50(1) → num_total−1，最后整体重算三计数并重选主动卡）
 * 和 reset_cards 0x407da0（全清）。商店空白卡就是在店里调后者，所以「店里删卡、子机等关店广播再重建」有原生先例。
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
    if (!card_in_list(mgr, victim)) { ce_log("destroy: refused, card %u not in the list", ce_card_id(victim)); return 0; }
    /* 当前选中的主动卡也能删（2026-09-09）。原先一律拒绝，于是「卡组里只剩一张主动卡」时
     * 倒吊人会白买一次。做法照原生 recount 的收尾：先把 mgr+0x38 摘空，删完再让引擎自己重选。
     * 顺序不能反 —— 指针留到对象被释放之后就是悬垂的。*/
    uint8_t **selected = (uint8_t **)(mgr + CE_MGR_SELECTED_ACTIVE);
    int was_selected = (*selected == victim);
    if (was_selected) *selected = NULL;
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
    if (was_selected) {
        int ok = ((int (CE_TC *)(void *, int))CE_FN_SET_SELECTED_ACTIVE)(mgr, -1);
        /* ★ 2026-09-22：`set_selected_active_card` **只在选中成功时**刷 HUD（`0x408b74`）⇒
         * 「删掉的正好是最后一张主动卡」这条路上 `mgr+0x38` 已经是 NULL 而那棵 VM 树的 `visible`
         * 还挂着 ⇒ 旧卡图会**永久留在左下角**（要到下一关开场 `FUN_00407f10` 重建才消失）。
         * 这就是 owner 说的「没有主动卡的时候……整个 UI 是不存在的」那一半，必须在这里补一刀。*/
        hud_card_refresh(mgr);
        ce_log("destroy: card %u was the selected active card; reselect -> %s; hud card -> %s", id,
               ok ? "ok" : "none left",
               *(void **)((uint8_t *)mgr + CE_MGR_SELECTED_ACTIVE) ? "shown" : "hidden");
    }
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
    /* 2026-09-09：不再排除「当前选中的主动卡」—— ce_destroy_card 现在能删它并让引擎重选。
     * 原来排除它，导致卡组里只剩一张主动卡时倒吊人抽不到候选、白买一次（实跑发现）。*/
    for (const uint8_t *node = *(const uint8_t *const *)(mgr + CE_MGR_CARD_LIST_FIRST);
         node && guard < 256;
         node = *(const uint8_t *const *)(node + CE_NODE_NEXT), ++guard) {
        uint8_t *card = *(uint8_t *const *)(node + CE_NODE_CARD);
        if (!card || card == (uint8_t *)exclude) continue;
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

/* ⚗ 2026-09-17a 二分（临时）：只校验这 6 条站点；其余在 patch 里没声明 ⇒ 跳过（避免拿 RVA 0 去核对）*/
static int ce_bp_kept(const char *name)
{
    static const char *k[] = { "ce_enemy_damage", "ce_card_bind", "ce_item_score", "ce_item_score_below", "ce_item_money", "ce_enemy_drop" };
    unsigned i;
    for (i = 0; i < sizeof k / sizeof k[0]; ++i) if (!strcmp(k[i], name)) return 1;
    return 0;
}

int ce_sdk_setup(uint8_t *base, int trace)
{
    ce_dmg_reset();                                   /* 伤害层：从干净的聚合开始（§AA）*/
    ce_invuln_reset();                                /* 无敌取最大层（EVENTS-CONTRACT §4）*/
    ce_score_reset();                                 /* 计分声明层（EVENTS-CONTRACT §3）*/
    ce_optpose_reset();                               /* 低速子机选举层（§1 选举覆盖）*/
    ce_poc_reset();                                   /* ★ PoC 高度层（收点线；2026-09-15）：连基准一起清 */
    ce_poc_line_reset();                              /* ★ 同层的「不画那条线」声明（2026-09-20）：连基准一起清 */
    ce_cancel_latch_reset();                          /* 「本帧已消过弹」闩锁（§5.2 细则 2）*/
    ce_rescue_note_reset();                           /* 「这次死亡由谁救」闩锁（救援层顺序，§5.2）*/
    ce_forced_death_reset();                          /* ★ 「卡自己造的死」通报位（2026-09-16u）*/
    ce_full_p_gate_reset();                           /* ★ 「未满 P」声明层（2026-09-16za，106）*/
    ce_hud_slots_reset();                             /* HUD 槽位分配器（HUD-LAYOUT §3）*/
    ce_drop_reset();                                  /* 掉落乘法层（EVENTS-CONTRACT §2）*/
    /* ★ 2026-09-13 补（跨卡互作审计 §「sdk_core 漏复位」）：**中弹无伤声明层漏了复位**。
     *   `ce_no_hit_declare` 是**粘性**的（任一声明即生效、直到显式撤），
     *   而 `ce_sdk_setup` 原来只复位了伤害/无敌/计分/子机/闩锁/HUD/掉落 —— 漏了它 ⇒
     *   一旦开机时残留一次非 0 声明（异常退出、热重载、或某张卡在 `on_delete` 之前就崩），
     *   **整个进程的「中弹无伤」闸门会永久开着**（每张卡的 tick 都停在那条粘性值上）。
     *   ⇒ 与上面那批一样，从干净的聚合开始。*/
    ce_no_hit_reset();                                /* 「中弹无伤」声明层（§CD-4）*/
    /* ★ 2026-09-16：「隐藏原生子机」声明层（111 魔理沙侧）—— 与上面那批一样**从干净的聚合开始**：
     *   它是粘性的，残留一次非 0 声明 = 那台机体的原生子机永远不出现（且没人能看出为什么）。*/
    ce_native_option_hide_reset();
    /* ★ 2026-10-01：跨卡只读标志层（`HANDOFF-108-CROSSCARD-SHIFT.md`）—— 同上，**粘性** ⇒ 从干净起步；
     *   残留一次 = 「灵界/咆哮」标志挂在进程里，108 的 shift 闸门永久打不开。*/
    ce_card_flag_reset();
    /* ★ 2026-09-13：死亡结果闩锁（`BP_ce_death_result` / `BP30-IMPLEMENTATION.md` §2）也要从这里干净起步。
     *   它与上面那批不同：**不是**每帧闩锁，而是「一次死亡的判定结果」，正常由 `BP_ce_player_die` 复位；
     *   这里再复位一次是防「异常退出 / 热重载」把上一次进程的 1 带进来。*/
    ce_death_result_reset();                          /* 死亡结果闩锁（§A-2）*/
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
    /* 2. 全部行为断点已挂（19 条）。聚合检查：任何一个失败都打印现场字节（判断 thcrap 没下断 vs 下错位置），
     * 全部查完再决定成败——避免第一个失败点挡住后面断点的诊断信息。
     * ⚠ 2026-09-13 夜补进 `ce_player_die`：它（2026-09-13 新增的灵界「中弹无伤」闸门）原来**不在本对账表里**
     *   ⇒ 断点没挂上时是**静默失效**（闸门看着"在"，实际一行都没跳）。这里补上后，它和其余 15 条一样
     *   「挂不上就直接报 FAIL」。零新增风险：本表其余 15 条与它同出一个 patch 的 `breakpoints`，
     *   谁的前 15 条能过，它必然也已挂上。
     * ★ 2026-09-13 再补两条（17 → 18 条，`BP30-IMPLEMENTATION.md`）：`ce_bullet_damage_final`（伤害层唯一
     *   落笔点）与 `ce_death_result`（死亡结果闩锁）。这两条**必须**在这里：前者挂了才有伤害倍率，
     *   后者挂了才有结果闩锁 —— 都是「不挂就静默失效」的那一类，所以一律「挂不上直接 FAIL」。
     * ★ 2026-09-13 第四次补（19 → 20 条）：`ce_enemy_damage`（`0x42f2cb`，「Boss 随血量掉灵」的事件源；
     *   不挂 = 113 的通道 A 静默失效）。
     * ★ 2026-09-13 第三次补（18 → 19 条）：`ce_item_score_in`（计分层**新的**落笔点，`0x446bf9`，
     *   `SCORE-LAYER-REWORK.md` §2）。它不挂 = 107/110/111 的声明**全部静默失效**（分值回到原生），
     *   而两个老出口站点仍会打 `score-out:` ⇒ 日志上看起来「有一个 delta」却查不出原因，必须在这里拦。*/
    struct { const char *name; uint32_t rva; unsigned len; } bps[] = {
        { "ce_card_bind",   CE_BP_CARD_BIND_RVA,   6 },
        /* ⚗ 2026-09-16zr 二分临时：本站点已从 patch 摘掉，对账行同步注释 */
        /* 名额回收：本行停用（站点已从 patch 摘除，功能=纯观察者）*/
        /* 名额回收：本行停用（站点已从 patch 摘除，功能=纯观察者）*/
        { "ce_item_money",  CE_BP_ITEM_MONEY_RVA,  6 },
        /* ★ 2026-09-22 恢复（同 `ce_bomb_gate`/`ce_option_shot_gate`）：`ce_bomb_spent` 有**六个**
         *   卡的 `.on_bomb_spent` 消费方；`ce_item_score_in` 是**计分层唯一的落笔点**（`score-in:` 那套）。
         *   两条都是 2026-09-16 的「二分（临时）」摘的、**从没回填** ⇒ 静默失效。sitecheck 四项全过。*/
        { "ce_item_score_in", CE_BP_ITEM_SCORE_IN_RVA, 8 },
        { "ce_bomb_spent",  CE_BP_BOMB_SPENT_RVA,  5 },
        /* ⛔ ce_death_result / ce_poc_gate_a / ce_poc_gate_b / ce_chain_node_guard：仍 park（见 §BE-FE）*/
        { "ce_enemy_drop",  CE_BP_ENEMY_DROP_RVA,  6 },
        /* ★★ 2026-09-22 恢复（本表曾被 2026-09-16/17 的「二分」整片注释掉，但那些站点后来
         *   已在 `sites.py` 的 `_SDK_KEEP_NAMED` 里按名字放回 patch ⇒ **注释没跟着回来** =
         *   这些站点的「挂不上直接 FAIL」护栏整整 5 天是关着的（静默失效那一类风险）。
         *   恢复前逐条核过：① 每个名字都在当前 patch 的断点表里（25 条）；
         *   ② 每条的 `expected` 与 `th18.exe` 那 6/7/8/9 个字节**逐字节相符**（写了个一次性脚本按
         *      PE 段表做 RVA→文件偏移映射后比对，25 条全符）⇒ thcrap 会全部挂上，不会误报 FAIL；
         *   ③ `_sdk_rva` = 0 的（仍在 park 的）宏是坏值（`0x-400000`）⇒ **编译器会拦住**误启用。*/
        { "ce_item_frame",  CE_BP_ITEM_FRAME_RVA,  7 },
        { "ce_item_collect", CE_BP_ITEM_COLLECT_RVA, 6 },
        { "ce_item_commit", CE_BP_ITEM_COMMIT_RVA, 6 },
        { "ce_shot_damage", CE_BP_SHOT_DAMAGE_RVA, 7 },
        { "ce_bomb_gate",   CE_BP_BOMB_GATE_RVA,   7 },
        { "ce_shot_set_index", CE_BP_SHOT_SET_INDEX_RVA, 7 },
        /* ★ 2026-09-22 恢复（同 `ce_bomb_gate`）：`CE_OPTION_SHOT_GATE_ENABLE` = 1 而站点被「名额回收」
         *   摘掉 ⇒ 113 的「灵界期间停掉装备卡子机弹」与 117 的咆哮同款闸**静默失效**。sitecheck 四项全过。*/
        { "ce_option_shot_gate", CE_BP_OPTION_SHOT_GATE_RVA, 6 },
        { "ce_enemy_damage", CE_BP_ENEMY_DAMAGE_RVA, 6 },
        { "ce_enemy_death", CE_BP_ENEMY_DEATH_RVA, 6 },
        { "ce_enemy_shot_slot", CE_BP_ENEMY_SHOT_SLOT_RVA, 8 },
        { "ce_player_die",  CE_BP_PLAYER_DIE_RVA,  6 },
        { "ce_bullet_damage_final", CE_BP_BULLET_DAMAGE_FINAL_RVA, 9 },
        /* ★ 2026-09-27（108 · owner「3 可以贴一手，关键在站点写法」）：一发自机弹 × 一个敌机的命中。
         *   机制全文 = `BP_ce_bullet_hit` 的注释（唯一出处）。*/
        { "ce_bullet_hit",  CE_BP_BULLET_HIT_RVA,  8 },
        /* ★ 2026-09-27（第三十九批）：**放回**（原 2026-09-10 的 park 理由已被别处的定案取代，
         *   五项窗口检查全绿）—— 108 的擦弹刻符要撒在**被擦到的那颗弹**上，116 的季节点同。*/
        { "ce_graze",       CE_BP_GRAZE_RVA,       5 },
        /* ★★★ 2026-09-13（owner：合成输入要进 replay）：调试件「逐帧合成方向输入」的落点
         *   （机制全文 = 上面 `BP_ce_input_synth` 的长注）。列在这里是为了把站点一起登记进
         *   `ce_cave_note`/每帧看门狗与启动日志。
         *   ⚠ 本条的 `bp_applied()` 判据**是退化的**：窗口首字节原本就是 `e8`（`call rel32`），
         *   站点打没打上都满足 `p[0]==0xe8` ⇒ 这一行**不构成**"栏挂了没有"的真护栏
         *   （真验靠 `sitecheck.py 0x462966 5` + 实跑；它绝不会误报 FAIL，也不会挡住别人）。*/
        /* { "ce_input_synth", CE_BP_INPUT_SYNTH_RVA, 5 },  ★ 2026-09-13 已拆线（见文件头与 DEV-INPUT-SYNTH.md §0）*/
        /* ⛔ ce_death_result：站点仍在 park */
        /* ★ 2026-09-16（111 魔理沙侧「原生子机不许出现」）：`sdk.c` 上面那一大段。
         *   两条都**必须**在这里：少一条 = 静默失效（①不拦 ⇒ 原生子机照建照画；②不拦 ⇒
         *   子弹从看不见的子机位置飞出来，且那部分输出与 111 的子机层**叠加**）。*/
        { "ce_repopulate_native_option", CE_BP_REPOP_NATIVE_OPT_RVA, 7 },
        { "ce_native_option_muzzle", CE_BP_NATIVE_OPT_MUZZLE_RVA, 5 },
        /* ⛔ ce_poc_gate_a / ce_poc_gate_b：**故意** park（接管那条 `ja` 有害，见 `sites.py` 的 ⛔ 段）——
         *   宏同样只是坏值，想放回先读 `AUDIT.md` §BE-EN。*/
        /* ⛔ ce_chain_node_guard：站点仍在 park */
    };
    unsigned n_bp_fail = 0;
    for (unsigned i = 0; i < sizeof bps / sizeof bps[0]; ++i) {
        if (!bps[i].rva) continue;   /* ⚗ 二分临时：被过滤掉的站点 RVA 是 0 ⇒ 跳过核对 */
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
    /* 2a-bis. ★ 2026-09-16zf：**codecave 取证**（头号崩溃第二阶段；机制全文 = 本文件上面的
     *   `ce_cave_note` 那一段）。两件事：
     *     ① 把**我们每个站点**的 cave 解出来并记下首 8 字节（thcrap 若没把代码写进去 = 全 0 ⇒
     *        该站点一命中就执行零页 —— 与 dump 里那个「写 0x1」的签名逐字相符）；
     *     ② 把 **`nmlgc/base_tsa` 的 `th18_centipede_fix`** 站点（`th18:0x412c76`，它和我们的卡表搬迁
     *        改的是同一个 `AbilityManager::allocate_new_card`）也解出来并记下首 16 字节。
     *   之后每帧由 `ce_cave_watch()` 复查首 8 字节是否被改写。*/
    {
        unsigned i;
        for (i = 0; i < sizeof bps / sizeof bps[0]; ++i)
            ce_cave_note(bps[i].name, (uint32_t)(uintptr_t)(base + bps[i].rva));
        ce_cave_note("base_tsa:th18_centipede_fix", (uint32_t)(uintptr_t)(base + CE_BS_CAVE_SITE_RVA));
        ce_cave_overlap();
        ce_log("cave: 共登记 %u 个站点（全 0 的 %u 个、重叠 %u 对）；每帧复查首 %u 字节",
               s_cave_n, s_cave_zero, s_cave_ovl, (unsigned)CE_CAVE_WATCH);
        ce_mem_map_dump();
        ce_pool_count(base);      /* ★ 闸上限的实测读数（用户口径：把闸定下来）*/
    }
    /* 2b. **binhack 落点回读**（★ 2026-09-14 新增，AUDIT §CQ）：断点有 `bp_applied()` 兜着，
     *     而 binhack 一条都没有 —— 它的 `expected` 对不上时 thcrap **只打一行日志就跳过**
     *     （binhack.cpp:398，等价于没打）。目前只有一条 binhack 属于「不打就整个弹幕系统错位」：
     *     战线 F 的 `pl0X.sht` 组数扩容（数据区基址 `0x180 → 0x260`）。它没打上 =
     *     影像里 96 格偏移数组与数据区**错位 0xe0 字节** ⇒ 每一条零售 shooterset 都指到
     *     别的组/垃圾上（主炮先崩），而日志上什么都不会说。
     *     判据不是「字节相等」而是**读回来的立即数**：`lea eax,[edx+imm32]` 的 imm32 ==
     *     `CE_SHT_DATA_BASE`（生成物 sht_ids.h，与生成器 `append_shooterset.py` 同源）。
     *     FAIL 的后果与断点那批一致（本局禁用新卡），但这条**本来就没法玩**：
     *     补丁没打上时装不装 DLL 主炮都是错的 —— 早 FAIL 早看见。*/
    {
        const uint8_t *p = base + CE_BH_SHT_DBASE_RVA;
        uint32_t got = (uint32_t)p[2] | ((uint32_t)p[3] << 8) | ((uint32_t)p[4] << 16) | ((uint32_t)p[5] << 24);
        if (p[0] != 0x8d || p[1] != 0x82 || got != (uint32_t)CE_SHT_DATA_BASE) {
            ce_log("FAIL: sdk: pl0X.sht 96-slot expansion binhack not applied "
                   "(+%08x: %02x %02x imm32=%#x, expected 8d 82 imm32=%#x) — pl0X.sht 是 %d 格影像，"
                   "基点还是 %#x 的话主炮会按错位 0x%x 字节读组",
                   CE_BH_SHT_DBASE_RVA, p[0], p[1], got, (unsigned)CE_SHT_DATA_BASE,
                   CE_SHT_SLOTS, 0x180, (unsigned)(CE_SHT_DATA_BASE - 0x180));
            ce_verdict("FAIL: sdk: pl0X.sht 96-slot expansion binhack not applied");
            return 0;
        }
    }
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

/* ============================ 敌人发射口（共享事件，108） ============================
 * 断点 0x433ffa（敌人 ECL opcode 0x259 = etOn，handler 0x433f34 内「发弹三步」的汇聚点）：
 *   433ff4  mov [esi+0x5a8],eax          ; 槽记录写瞄准值（在我们的站点**之前**，照常执行）
 *   433ffa  movss xmm1,[edi+0x5144]      ; ★ 站点（8 字节、无相对寻址）
 *   434002  mov ecx,ds:0x4cf2bc / call 0x42cca0   ; et_protect_range 置位 / 取值
 *   434013  lea eax,[esi+0x598] / push / call 0x427810   ; __fire(props)（唯一的发射点）
 *   434025  xorps xmm1,xmm1 / call 0x42cca0      ; 复位
 *   43402d  jmp 0x436417                          ; opcode 收尾
 * ⇒ handler 返回非 0 时 `retaddr = 0x43402d`：**本帧这一槽不发射**，其余（瞄准值、ECL 状态机）照旧。
 * 寄存器（一手）：edi = &enemy->data（= enemy+0x122c）、槽号 i = [ebp-0x56c]、esi = &data + i*0x488。
 * 不加任何内存写、不碰敌人生命周期 ⇒ 与 BP_ce_shot_set_index 同风险级。*/
#define CE_ESHOT_SLOT_STACK   0x56c      /* [ebp-0x56c] = 槽号（0x433f3d 写；x86 栈向下增长）*/
#define CE_ESHOT_DATA_OFF     0x122c     /* zEnemy → zEnemyData 的偏移（0x430d30 add ecx,0x122c）*/
int __cdecl BP_ce_enemy_shot_slot(x86_reg_t *regs, void *bp_info)
{
    (void)bp_info;
    uint8_t *mgr = CE_ABILITY_MGR();
    uint8_t *data = (uint8_t *)(uintptr_t)regs->edi;
    if (!mgr || !data || !regs->ebp) return BP_EXEC_ORIGINAL;

    int32_t slot = *(const int32_t *)(uintptr_t)(regs->ebp - CE_ESHOT_SLOT_STACK);
    if (slot < 0 || slot > 15) return BP_EXEC_ORIGINAL;                     /* 界外 ⇒ 不动（防御）*/
    /* 自检：站点处 esi 必须正好是 &data + slot*0x488（对不上说明我们记的寄存器语义过时了）*/
    if (regs->esi != (uint32_t)(uintptr_t)(data + (size_t)slot * 0x488)) return BP_EXEC_ORIGINAL;

    uint8_t *enemy = data - CE_ESHOT_DATA_OFF;                               /* 回到 zEnemy* */
    ce_enemy_shot_t ev;
    ev.enemy_id = *(const uint32_t *)(enemy + CE_ENEMY_ID);
    ev.slot     = slot;
    ev.enemy    = enemy;

    unsigned guard = 0;
    for (const uint8_t *node = *(const uint8_t *const *)(mgr + CE_MGR_CARD_LIST_FIRST);
         node && guard < 256;
         node = *(const uint8_t *const *)(node + CE_NODE_NEXT), ++guard) {
        uint8_t *card = *(uint8_t *const *)(node + CE_NODE_CARD);
        if (!card) continue;
        const ce_hooks_t *h = hooks_of(card);
        if (h && h->on_enemy_shot_slot && h->on_enemy_shot_slot((ce_card_t *)card, &ev)) {
            ce_sdk_trace(ce_card_id(card), 0x8c, "on_enemy_shot_slot");
            ce_retaddr_set(regs, 0x43402du);                                 /* opcode 收尾：本帧不发射 */
            return 0;
        }
    }
    return BP_EXEC_ORIGINAL;
}

/* ============================ 敌人死亡 / 击破（共享事件） ============================
 * 断点 0x430240（FUN_00430240，SDK 内部叫 Enemy__die）入口：
 *   thiscall，ecx = zEnemy；**在引擎撒主掉落之前**（0x430469 读表 → 0x430493 call 0x446f40 撒 →
 *   0x43049f call 0x430510 通知卡 → 0x4304aa 清表）；恒定 `mov eax,1` 返回 ⇒ 调用方同帧 sweep 删，
 *   **一敌一生正好一次**。
 * 死因用调用点的返回地址分（见 sdk/public.h 的 CE_EDEATH_*）：
 *   0x42f3b7 = life ≤ 0 的主路（= 四张卡要的「击破」）；0x42ee90 = 炸弹秒杀路；
 *   0x430f59 = ECL die opcode；0x4307f1 / 0x430a00 / 0x430c08 = killAll 三兄弟；其余 = OTHER。
 * 设计：docs/superpowers/specs/2026-09-12-shared-enemy-death-event-design.md（AUDIT 对应节）。*/
/* ★ 2026-09-13 实跑订正①：thcrap `x86_reg_t.retaddr` 是断点后的 cave 续跑 EIP，不是调用点返回地址。
 * ★ 实跑订正②：`[regs->esp]` 也不行 —— 实测读到 `0x430245`（= 断点站点 `0x430240`+5），是 thcrap
 *   自己的蹦床/续跑地址。真实调用点返回地址在栈更深处 ⇒ 从 `regs->esp` 起扫前 **12 个 dword**
 *   （`[esp]`, `[esp+4]`, … `[esp+0x2c]`），匹配已知七条 call site 就返回它，并记录命中格号。
 *   扫描不到时返回 0 / `ret_slot=-1`，由卡侧按「HP ≤ 0 + real_bit + 非 Cir/ECir」兜底（宁可多掉）。*/
static int edeath_is_known_call_site(uint32_t w)
{
    switch (w) {
    case 0x42f3b7u:  /* KILLED（life ≤ 0 主路）*/
    case 0x42ee90u:  /* BOMB_KILL */
    case 0x42f3ddu:  /* OTHER（标志驱动移除）*/
    case 0x4307f1u:  /* KILL_ALL */
    case 0x430a00u:  /* KILL_ALL */
    case 0x430c08u:  /* KILL_ALL */
    case 0x430f59u:  /* ECL_DIE */
        return 1;
    default:
        return 0;
    }
}

static uint32_t edeath_call_site(const x86_reg_t *regs, int32_t *out_slot)
{
    int i;
    if (out_slot) *out_slot = -1;
    if (!regs || !regs->esp) return 0;
    for (i = 0; i < 12; ++i) {
        uint32_t w = *(const uint32_t *)(uintptr_t)(regs->esp + (uint32_t)i * 4u);
        if (edeath_is_known_call_site(w)) {
            if (out_slot) *out_slot = i;
            return w;
        }
    }
    return 0;
}

static uint32_t edeath_kind(uint32_t retaddr)
{
    switch (retaddr) {
    case 0x42f3b7u: return CE_EDEATH_KILLED;
    case 0x42ee90u: return CE_EDEATH_BOMB_KILL;
    case 0x430f59u: return CE_EDEATH_ECL_DIE;
    case 0x4307f1u:
    case 0x430a00u:
    case 0x430c08u: return CE_EDEATH_KILL_ALL;
    default:        return CE_EDEATH_OTHER;
    }
}

/* ★★ `BP_ce_bullet_damage_final` —— 伤害层的**唯一**落笔点（一手：`BP30-IMPLEMENTATION.md` §1）。
 *
 * 站点 = `PlayerBullet__create` `th18:0x45e320` 的**卡链遍历出口**（9 字节，exe 里
 * `ff 77 08 8b 0d 5c f6 51 00` = `push [edi+8]` / `mov ecx,[0x51f65c]`(= ANM_MANAGER_PTR)）：
 *   `0x45e7dc mov eax,[0x4cf298]`（ABILITY_MGR）→ `0x45e7e4 mov esi,[eax+0x1c]`（卡链头）→
 *   `0x45e7e9 je 0x45e7ff`（**没卡也落在本站点**）→ 循环 `0x45e7f0`–`0x45e7fd`
 *   （`mov ecx,[esi]` / `push edi`(bullet) / `call [eax+0x28]` / `esi=[esi+4]` / `jne`）→ **本站点**
 *   → `0x45e808 call 0x488b40`（AnmManager__get_vm_with_id）→ … → `0x45e837 push [edi+0x9c]`
 *   才把伤害交给伤害源（`0x45e861 call 0x45dfa0`）。
 * ⇒ 五家（含原生 25 Saki `0x40c310` / 54 Momoyo `0x411010` 的**重算**）全跑完之后，我们才乘一次
 *   ⇒ 与卡链顺序无关的**乘法叠加**（×1.25 × 1.4 = ×1.75）；不放在 `+0x28` 桩里的原因见上面那一大段。
 *
 * 寄存器：**EDI = bullet**（`0x45e7e1 mov [edi+8],esi` 与 `0x45e837 push [edi+0x9c]` 两处独立印证）；
 * ESI/EDX 此时已被引擎换用（esi = 卡链节点游标）⇒ **只读 EDI**，别的都不可信。
 * 站点 9 字节 = `push imm` + 绝对寻址 mov，无相对寻址、窗内无跳转靶（`0x45e7e9` 的 je 恰好落在站点
 * **本身**，落在站点 = 走断点，合法）；返回 BP_EXEC_ORIGINAL ⇒ 两条原指令在 cave 里原样回放
 * （thcrap 的 cave 跑在**同一个栈**上，`push` 的效果照旧带回去）。
 *
 * 诊断日志：自机弹是**每发都有**的高频路径 ⇒ 只在「真的改了值」时计数，前 3 次 + 之后每 5000 次一条
 * （照 `BP_ce_player_die` 的限流风格）。判据是 `before -> after`、不是「被调用次数」——
 * 中性（没有加伤害的卡）时 handler 一个字都不写，也就一条日志都不打。*/
static unsigned s_dmg_final_logs;

/* ★★ 2026-09-16zd：**自机弹 `+0xe8` 回调闸**（崩溃取证 + 防"跳飞到 1"）。
 *
 * 一手链条 + dump 证据 = `engine.h` 的 `CE_BULLET_CALLBACK`（唯一出处）。这里只做两件事：
 *   ① **普查**：头 8 个"非 0 的 `+0xe8`"各打一行（我们要知道引擎合法时会放什么进去）；
 *   ② **闸**：值非 0 且 < 0x10000（32 位进程里不可能是指向模块的代码地址）⇒ 判为坏、清 0
 *      （引擎自己就会跳过 ⇒ 不再 `call 1`），并计数打日志。
 * ⚠ 只读 `edi`（本站点在卡链遍历出口，esi/edx 已被引擎换用，见上面那段注释）。*/
static unsigned s_bullet_cb_census;
static unsigned s_bullet_cb_guarded;

int __cdecl BP_ce_bullet_damage_final(x86_reg_t *regs, void *bp_info)
{
    (void)bp_info;
    uint8_t *bullet = (uint8_t *)(uintptr_t)regs->edi;
    if (bullet) {
        int32_t *cb = (int32_t *)(bullet + CE_BULLET_CALLBACK);
        int32_t cbv = *cb;
        if (cbv != 0 && s_bullet_cb_census < 8) {
            ++s_bullet_cb_census;
            ce_log("bullet +0xe8 census #%u: bullet %08x row/set %08x +0xe8 = %08x（引擎会在 0x45e8ad 读它 call）",
                   s_bullet_cb_census, (unsigned)regs->edi,
                   (unsigned)*(const int32_t *)(bullet + CE_BULLET_SET_ROW), (unsigned)cbv);
        }
        if (cbv != 0 && (cbv < 0x10000)) {
            *cb = 0;
            ++s_bullet_cb_guarded;
            if (s_bullet_cb_guarded <= 5 || (s_bullet_cb_guarded % 500) == 0)
                ce_log("★ bullet +0xe8 guard: 弹 %08x 的 +0xe8 = %08x 不是合法代码地址 ⇒ 清 0"
                       "（否则 0x45e8c0 call eax ⇒ 0xC0000005，见 crashdumps）（#%u）",
                       (unsigned)regs->edi, (unsigned)cbv, s_bullet_cb_guarded);
        }
        int32_t *dmg = (int32_t *)(bullet + CE_BULLET_DAMAGE);
        int32_t before = *dmg;
        ce_sdk_bullet_damage_final(bullet);
        if (*dmg != before) {
            ++s_dmg_final_logs;
            if (s_dmg_final_logs <= 3 || (s_dmg_final_logs % 5000) == 0)
                ce_log("bullet damage final: %d -> %d (agg %d permille, bullet %08x, #%u, after card chain)",
                       (int)before, (int)*dmg, ce_dmg_aggregate(), (unsigned)regs->edi, s_dmg_final_logs);
        }
    }
    return BP_EXEC_ORIGINAL;
}

/* ★★ 2026-09-16ze：**更新链节点闸 + 取证**（头号崩溃的止血点，`local/MEMORY.md` §1）。
 *
 * ## 一、机制（一手，`local/_scratch/th18xref.py` + `local/_scratch/thdis.py`）
 * 站点 `th18:0x401352` 在 `UpdateFuncRegistry__run_all_on_tick` `th18:0x4012e0` 的派发上：
 *   `0x40131f mov edi,[ecx]`（**节点**）→ `0x401323 cmp [edi+8],0 / je`（没有 on_tick 就跳过）
 *   → `0x401329 test byte [edi+4],2 / je 0x4013ba`（暂停位）→ …→
 *   `0x40134f mov ecx,[edi+0x24]`（**this**）→ `0x401352 mov eax,[edi+8]` → `0x401355 call eax`。
 * 节点的类型 = `UpdateFunc__operator_new` `th18:0x401530`（`push 0x28` → `operator new`），布局一手坐实：
 *   `+0x00` 排序键 int（`register__on_tick` `0x401180` 的 `0x4011cc mov [edi],esi` 写它）｜
 *   `+0x04` flags（`0x401546 and [ecx+4],~1` + `0x40157d or [ecx+4],1`；bit1 = 暂停，见 `0x401329`）｜
 *   `+0x08` **on_tick**（`0x401581 mov [ecx+8],eax`，eax = operator_new 的**唯一参数**）｜
 *   `+0x0c` 注册期一次性回调（`0x401193` 有值就 `call`，`0x4011a1` 随后清 0）｜
 *   `+0x10` 第二个回调（`0x4013ab` 非 0 就 `call`，与 `+8` 同一 this）｜
 *   `+0x14` **链表格**（`0x401565 mov [ecx+0x14],ecx` 自指；格 = `+0x14` 节点自指 / `+0x18` next / `+0x1c` prev，
 *   `register` 的 `0x4011ef`–`0x401204` 就是按 `[node+0]` **升序插入**这条双向链）｜
 *   `+0x18/+0x1c/+0x20` 归零｜`+0x24` **this**（`0x40134f` 读它；operator_new 与 register 都不写 ⇒ 由调用者写）。
 * 谁注册（`0x401530` 的 89 处 `call`，按所在函数归类）：`Supervisor__initialize`(11)、`AsciiManager`、
 *   `AbilityManager`、`AbilityMenu`/`AbilityShop`、`Stage`、`Player__initialize`、`BulletManager`、
 *   `EnemyManager`、`ItemManager`、`EffectManager`、`Bomb*`（**运行期动态注册**）、`ScreenEffect`、
 *   `ReplayManager`、`PopupManager`、`MainMenu`、`TrophyNotice`、`CardCollection` 等。
 * 谁**摘链**（`0x4015a0` 的调用者）= 各管理器自己的析构：`Player__operator_delete`、`Stage__destructor`、
 *   `BulletManager__operator_delete`、`EnemyManager__operator_delete`、`Bomb__operator_delete`、
 *   `AbilityMenu__operator_delete`、`ReplayManager__destructor`… ⇒ 「释放前先摘链」是引擎自己的规矩。
 *
 * ## 二、崩溃签名（`crashdumps/` 的 `*.dmp`，5 次全同）与这条闸要回答的问题
 *   `0xC0000005` **写** `0x1`、`Eax = 1`、`Ebp/Esp` 每次同址、`EIP ≈ 0x03c?065a`（堆里、且不在任何模块）
 *   ⇒ 进程没有 DEP ⇒ **把堆数据当指令执行**了 ⇒ `[edi+8]` 是悬垂/被改写的垃圾指针。
 *   触发条件（用户实测）：**射击中 + 上线收点那一刻**（停枪不崩；打枪不越线不崩；换机体也崩）。
 * 这条闸做三件事：
 *   ① **链地图**：每个节点第一次被访问时打一行（node/order/flags/on_tick/this/vtable + 各自落在哪个模块）
 *      ⇒ 事故现场直接点名「受害者是谁」（是我们动过的对象还是引擎子系统）。
 *   ② **写监视**：节点的 `+8`/`+0x24` 与首次记录不同 ⇒ 打 `★ CHANGED` 行。节点是启动期一次分配的，
 *      正常运行时**永不改变** ⇒ 这一行就是「谁在什么时候把哪个节点写坏了」的直接证据。
 *   ③ **闸**：`on_tick` 不是可执行地址 ⇒ **跳过这次 `call`**（`eax = 1`、`retaddr = 0x401357`）：
 *      跳转表 `0x4013f8[1] = 0x4013b7` 是「本节点本帧什么都不做、链继续」的**纯空转**路径
 *      （不碰 edi、不摘链；对比 `[0] = 0x401397` 会去 `0x4015a0` 摘链 ⇒ 对垃圾节点更危险）。
 *      返回 0 = 不回放被搬进 cave 的两条原指令，栈上没有 `call` 的压栈 ⇒ 与「调用后」的栈形一致。
 * ⚠ 站点窗口 5 字节 = `8b 47 08` + `ff d0`：两条都无相对寻址、窗内无跳转靶（`0x401357` 之后才分派）。
 * ⚠ 只读 `edi`（本站点 `eax` 还没被装载、`ecx` 已是 this）⇒ 与 `BP_ce_bullet_damage_final` 同一条纪律。*/

#define CE_UF_SIZE        0x28u
#define CE_UF_ORDER       0x00u
#define CE_UF_FLAGS       0x04u
#define CE_UF_TICK        0x08u
#define CE_UF_THIS        0x24u
#define CE_CHAIN_SLOTS    64u
#define CE_CHAIN_RESUME   0x401357u   /* `0x401355 call eax` 的下一条 */
#define CE_CHAIN_SKIP_EAX 1u          /* 跳转表 `0x4013f8[1] = 0x4013b7`：空转、链继续 */

typedef struct {
    uint32_t node, tick, thisp;
    uint8_t  known, tick_ok;
} ce_chain_slot_t;

static ce_chain_slot_t s_chain[CE_CHAIN_SLOTS];
static unsigned s_chain_nodes;        /* 见过的不同节点数（前 CE_CHAIN_SLOTS 个各打一行地图）*/
static unsigned s_chain_visits;       /* 本站点命中次数（= Σ 每帧节点数）*/
static unsigned s_chain_changed;      /* ② 抓到「已知节点的 +8 / +0x24 被改写」的次数 */
static unsigned s_chain_bad;          /* ③ 抓到「on_tick 不是可执行地址 ⇒ 跳过 call」的次数 */

/* 32 位 Windows 的模块表走法（自包含，不引 winternl.h；offsets 错了只会打错名字，不会崩）：
 *   `fs:[0x30]` = PEB → `+0x0c` = PEB_LDR_DATA → `+0x0c` = InLoadOrderModuleList（嵌在 Ldr 里的头）
 *   LDR_DATA_TABLE_ENTRY：`+0x18` DllBase、`+0x20` SizeOfImage、`+0x2c` BaseDllName（UNICODE_STRING）
 * 用途只有一个：把 on_tick / this / vtable 三个裸地址翻成「哪个模块+偏移」。找不到就写 "?"。*/
typedef struct { uint16_t len, maxlen; uint32_t buf; } ce_unistr_t;

static void ce_mod_desc(uint32_t addr, char *out, unsigned cap)
{
    const uint8_t *peb = (const uint8_t *)(uintptr_t)__readfsdword(0x30);
    uint32_t ldr, head, cur;
    unsigned guard = 0;

    if (!out || cap < 8) return;
    out[0] = '?'; out[1] = '\0';
    if (!peb || IsBadReadPtr(peb + 0x0c, 4)) return;
    ldr = *(const uint32_t *)(const void *)(peb + 0x0c);
    if (!ldr || IsBadReadPtr((const void *)(uintptr_t)ldr, 0x10)) return;
    head = ldr + 0x0c;
    cur  = *(const uint32_t *)(const void *)(uintptr_t)head;
    while (cur && cur != head && guard++ < 256) {
        uint32_t base, size, nbuf;
        const ce_unistr_t *un;
        unsigned i, n;
        if (IsBadReadPtr((const void *)(uintptr_t)cur, 0x30)) return;
        base = *(const uint32_t *)(const void *)(uintptr_t)(cur + 0x18);
        size = *(const uint32_t *)(const void *)(uintptr_t)(cur + 0x20);
        if (base && size && addr >= base && addr < base + size) {
            un = (const ce_unistr_t *)(const void *)(uintptr_t)(cur + 0x2c);
            nbuf = un->buf;
            n = (unsigned)(un->len / 2);
            if (nbuf && !IsBadReadPtr((const void *)(uintptr_t)nbuf, un->len ? un->len : 2)) {
                if (n > cap - 12) n = cap - 12;
                for (i = 0; i < n; ++i) {
                    uint16_t ch = *(const uint16_t *)(const void *)(uintptr_t)(nbuf + i * 2);
                    out[i] = (ch >= 0x20 && ch < 0x7f) ? (char)ch : '_';
                }
                out[n] = '\0';
            } else {
                strcpy(out, "mod");
            }
            sprintf(out + strlen(out), "+%x", (unsigned)(addr - base));
            return;
        }
        cur = *(const uint32_t *)(const void *)(uintptr_t)cur;    /* Flink */
    }
}

/* 目标是不是「已提交且可执行」的页。用 `VirtualQuery`（不改保护、不动内存）——
 * 比 `cbv < 0x10000` 那种范围猜法可靠：本次的坏指针是**堆地址**（`0x03c?xxxx`），范围猜法抓不到。*/
static int ce_addr_exec(uint32_t a)
{
    MEMORY_BASIC_INFORMATION mbi;
    if (a < 0x10000) return 0;
    if (VirtualQuery((LPCVOID)(uintptr_t)a, &mbi, sizeof mbi) != sizeof mbi) return 0;
    if (mbi.State != MEM_COMMIT) return 0;
    switch (mbi.Protect & 0xffu) {
    case PAGE_EXECUTE:
    case PAGE_EXECUTE_READ:
    case PAGE_EXECUTE_READWRITE:
    case PAGE_EXECUTE_WRITECOPY:
        return 1;
    default:
        return 0;
    }
}

int __cdecl BP_ce_chain_node_guard(x86_reg_t *regs, void *bp_info)
{
    uint32_t node = regs->edi;
    uint32_t tick, thisp, vt = 0;
    ce_chain_slot_t *sl = NULL;
    char m_tick[64], m_this[64], m_vt[64];
    unsigned i;
    int verify = 0, is_new = 0;

    (void)bp_info;
    ++s_chain_visits;
    /* 节点自己都读不了 ⇒ 判不了，放行（原指令照跑，行为与不装 DLL 一致）*/
    if (!node || IsBadReadPtr((const void *)(uintptr_t)node, CE_UF_SIZE)) return BP_EXEC_ORIGINAL;

    tick  = *(const uint32_t *)(const void *)(uintptr_t)(node + CE_UF_TICK);
    thisp = *(const uint32_t *)(const void *)(uintptr_t)(node + CE_UF_THIS);
    if (!tick) return BP_EXEC_ORIGINAL;          /* 引擎自己在 0x401323 已排除；出现就是它先坏了 */

    for (i = 0; i < CE_CHAIN_SLOTS; ++i)
        if (s_chain[i].known && s_chain[i].node == node) { sl = &s_chain[i]; break; }
    /* ★ 2026-09-16zm：**节点派发轨迹**（与站点轨迹同法）：当前派发的节点与上次不同就打一行。
     * 崩前最后一行 = 正在跑谁的回调 ⇒ 直接点名是哪个子系统的 tick 之后跳错的。*/
    {
        static uint32_t s_node_last;
        if (node != s_node_last) {
            s_node_last = node;
            ce_log("node: %08x order=%d flags=%#x on_tick=%08x this=%08x ret=%08x",
                   node, (int)*(const int32_t *)(const void *)(uintptr_t)(node + CE_UF_ORDER),
                   *(const uint32_t *)(const void *)(uintptr_t)(node + CE_UF_FLAGS),
                   tick, thisp, (unsigned)regs->eax);
        }
    }

    if (!sl) {
        for (i = 0; i < CE_CHAIN_SLOTS; ++i)
            if (!s_chain[i].known) { sl = &s_chain[i]; break; }
        if (sl) {
            sl->known = 1; sl->node = node; sl->tick = tick; sl->thisp = thisp;
            ++s_chain_nodes;
            verify = 1; is_new = 1;
        }
    } else if (sl->tick != tick || sl->thisp != thisp) {
        /* ★ ② 节点是启动期一次分配的，正常运行时这两个字段永不改变 ⇒ 走到这里 = 有人在写它。
         * （⚠ 另一种解释：同址的旧节点**被正确摘链+释放**后，新节点复用了这个地址 —— 运行期动态注册的
         *   只有炸弹/屏幕特效/奖杯通知那几家；对照上下文一行就能区分。）*/
        ++s_chain_changed;
        if (s_chain_changed <= 20)
            ce_log("★ chain CHANGED #%u: node %08x on_tick %08x -> %08x , this %08x -> %08x",
                   s_chain_changed, node, sl->tick, tick, sl->thisp, thisp);
        sl->tick = tick; sl->thisp = thisp;
        verify = 1;
    }
    /* ⚠ 槽位表满（>64 个不同节点：炸弹/屏幕特效等运行期动态注册的家数多时可能）⇒ 只放弃**监视**，
     *   绝不放弃闸本身：这里判一次 NULL 再往下走，闸照旧（`sl->tick_ok` 那两行是唯一的解引用点）。*/
    if (!sl) {
        if (!ce_addr_exec(tick)) {
            ++s_chain_bad;
            if (s_chain_bad <= 10 || (s_chain_bad % 100) == 0)
                ce_log("★★ chain guard #%u（槽位表已满、无监视）: node %08x 的 on_tick = %08x 不可执行 ⇒ 跳过 call",
                       s_chain_bad, node, tick);
            regs->eax = CE_CHAIN_SKIP_EAX;
            ce_retaddr_set(regs, CE_CHAIN_RESUME);
            return 0;
        }
        return BP_EXEC_ORIGINAL;
    }
    if (verify) sl->tick_ok = (uint8_t)ce_addr_exec(tick);
    if (!sl->tick_ok) sl->tick_ok = (uint8_t)ce_addr_exec(tick);   /* 记过坏的每次复核（页可能才提交）*/
    /* ① 链地图：只在首见时打（≤ 64 行、一次性），四个地址各翻一次模块名 */
    if (sl && is_new && s_chain_nodes <= CE_CHAIN_SLOTS) {
        if (thisp && !IsBadReadPtr((const void *)(uintptr_t)thisp, 4))
            vt = *(const uint32_t *)(const void *)(uintptr_t)thisp;
        ce_mod_desc(tick, m_tick, sizeof m_tick);
        ce_mod_desc(thisp, m_this, sizeof m_this);
        ce_mod_desc(vt, m_vt, sizeof m_vt);
        ce_log("chain node #%u: %08x order=%d flags=%#x on_tick=%08x (%s) this=%08x (%s) vt=%08x (%s)",
               s_chain_nodes, node, (int)*(const int32_t *)(const void *)(uintptr_t)(node + CE_UF_ORDER),
               *(const uint32_t *)(const void *)(uintptr_t)(node + CE_UF_FLAGS),
               tick, m_tick, thisp, m_this, vt, m_vt);
    }

    /* ③ 闸：不是可执行地址 ⇒ 跳过 `call eax`（空转 = 跳转表 [1]），并把现场留一行 */
    if (!sl->tick_ok) {
        ++s_chain_bad;
        if (s_chain_bad <= 10 || (s_chain_bad % 100) == 0) {
            ce_mod_desc(tick, m_tick, sizeof m_tick);
            ce_mod_desc(thisp, m_this, sizeof m_this);
            ce_log("★★ chain guard #%u: node %08x 的 on_tick = %08x (%s) 不是可执行地址 ⇒ 本帧跳过这次 call"
                   "（否则 0x401355 call eax = 0xC0000005；返回 1 走 0x4013b7 空转）"
                   " order=%d flags=%#x this=%08x (%s) visit=%u changed=%u",
                   s_chain_bad, node, tick, m_tick,
                   (int)*(const int32_t *)(const void *)(uintptr_t)(node + CE_UF_ORDER),
                   *(const uint32_t *)(const void *)(uintptr_t)(node + CE_UF_FLAGS),
                   thisp, m_this, s_chain_visits, s_chain_changed);
        }
        regs->eax = CE_CHAIN_SKIP_EAX;
        ce_retaddr_set(regs, CE_CHAIN_RESUME);
        return 0;                        /* 0 = 不回放 cave 里的两条原指令 */
    }
    return BP_EXEC_ORIGINAL;
}

/* ★★ `BP_ce_death_result` —— 「这次死亡**最终**有没有被取消」结果闩锁（一手：`BP30-IMPLEMENTATION.md` §2）。
 *
 * 站点 = `Player__on_tick__body` `th18:0x45be90` 的 case 4 里**唯一那次判定**（6 字节，
 * exe 里 `85 ff 8b 7c 24 14` = `test edi,edi` / `mov edi,[esp+0x14]`）：
 *   `0x45c106 mov eax,[0x4cf298]` → `0x45c10b mov esi,[eax+0x1c]` → `0x45c110 je 0x45c133`（没卡）；
 *   `0x45c112 xor edi,edi`（acc=0）→ `0x45c116 push edi`（当参数）→ `0x45c11c call [eax+0xc]` →
 *   `0x45c121 or edi,eax` → `0x45c123/0x45c125` 循环**把五家全跑完** → **本站点** →
 *   `0x45c129 mov edi,[esp+0x14]`（还原 EDI = player）→ `0x45c12d jne 0x45c3d1`（acc≠0 ⇒ 取消死亡）→
 *   `0x45c135 call 0x45d090`（acc=0 ⇒ 扣命）。
 * ⇒ **EDI = acc**：非 0 = 这次死亡已被取消、0 = 会扣命。handler 只把结果**原样播进 sdk_core**
 *   （`ce_death_result_note`），供排在后面的救援者（原生 35 不看 acc、别人的卡 69 无条件扣命）在
 *   记账侧查询；**不改任何控制流** —— 返回 BP_EXEC_ORIGINAL，原两条指令回放，`test` 设的 ZF 正好给
 *   `0x45c12d` 的 `jne` 用（所以**不要**在这里读 eflags 下结论）。
 *
 * ★ 为什么不是 0x45c3d1（取消臂）/ 0x45c135（扣命臂）：前者是 case 3 与 case 4 的**共用尾**
 *   （Ghidra 反编译直接把它标成 `LAB_0045c3d1`，`case 3` 也 `goto` 它）⇒ 会把非死亡帧记成「取消」；
 *   后者是 `call rel32`（窗内相对寻址，纪律禁止）。
 * ⚠ 本站点**在没卡的 deck 上不执行**（`0x45c110 je 0x45c133` 绕开它）⇒ 那种情况闩锁停在
 *   `CE_DEATH_RESULT_UNKNOWN`；而「没卡」= 没有卡能读它，所以这一档不会被人误读成 0。*/
static unsigned s_death_result_logs;

int __cdecl BP_ce_death_result(x86_reg_t *regs, void *bp_info)
{
    (void)bp_info;
    int cancelled = (regs->edi != 0);
    ce_death_result_note(cancelled);
    ++s_death_result_logs;
    /* 限流打印（照 BP_ce_player_die 的写法）：前 3 次 + 之后每 20 次一条。死亡本来就稀有。*/
    if (s_death_result_logs <= 3 || (s_death_result_logs % 20) == 0)
        ce_log("death result: %s (acc=%u, five rescuers done, #%u) - life %s",
               cancelled ? "CANCELLED" : "not cancelled", (unsigned)regs->edi, s_death_result_logs,
               cancelled ? "kept" : "will be lost (0x45c135 -> commit)");
    return BP_EXEC_ORIGINAL;
}

/* ======================================================================================
 * ★★ 2026-09-16za **「上线收点当且仅当满 P」**（卡 106 红魔乡的新设计；两条断点）
 * ======================================================================================
 * 用户口径（逐字）：「红魔乡卡再加一条设计：**上线收点当且仅当满P**」。
 * 一手（TH06 反编译 `ItemManager.cpp` 的道具 tick）：
 *     `if (curItem->state == 1 || (128 <= g_GameManager.currentPower && g_Player.positionCenter.y < 128.0f))`
 *     → 进吸引态（`state = 1`、`sincosmul(…, 8.0f)`）
 *   ⇒ **「满 P（128）+ 自机在回收线（y = 128.0）以上」是过线收点的充要条件**；TH06 里没有第二条路。
 * TH18 侧的同一语义点 = `ItemManager__on_tick__body` 的两条 `ja 0x445f63`
 * （`th18:0x445bb9` / `th18:0x446003`；0x445f63 = `mov [item+0xc74],3` = 道具进真空态）。
 * ★★ 2026-09-18 **改法（这次不再依赖 cave 回放）**：handler **恒返回 0**，由它自己把
 * `regs->retaddr` 设成「那条 `ja` 的两种结果」：
 *   声明层开着（= 106 说「现在不是满 P」）⇒ 条件不成立 ⇒ 落点 = 站点后一条（A `th18:0x445bbf` /
 *   B `th18:0x446009`）；否则 ⇒ 条件成立 ⇒ 落点 = 真空态入口 `th18:0x445f63`。
 * **为什么不能「返回 1 让 cave 回放那条 ja」**（这就是头号崩溃，一手见 `AUDIT.md` §BE-DW）：
 *   窗口 = **一条 `0f 8x rel32`**，而 thcrap 的 `cave_fix` **只重定位窗口首条的 `e8`/`e9`**
 *   ⇒ 回放时那条 `ja` 会跳到「本格 + (`0x445f63` − 站点)」= 本格 **+0x3aa**（A）/ 本格 − 0xa0（B）
 *   ⇒ 实测崩点（池基 + 0x65a/0x64a/0x62a）减 0x3aa 正好是那几局池子的**最后一格**
 *   ⇒ 落到池页没写过的那一段 ⇒ `00 00`、写 `0x1` 的访问违例。
 *   恒返回 0 ⇒ **cave 永不执行** ⇒ 窗口里有什么分支都无所谓（本窗口仍保留，因为它是「哪条支路」的
 *   唯一判据：`th18:0x445f63` 是**四条**支路的汇聚点，挂在那里分不出「过线」）。
 * ⚠ 落空支**不看 flags**（一手：A 的 `0x445bbf` / B 的 `0x446009` 都是 `mov eax,[0x4cf2b8]` 起头、自带 `cmp`）
 *   ⇒ 落点本身是整指令边界、且都在 exe 映像内（过 `ce_retaddr_set`）。
 * ★★★ **2026-09-19 晚修（一次实跑事故，`AUDIT.md` §BE-EN）**：上面那版 handler **只按声明层选落点、从不复现
 *   那条 `ja` 的比较** ⇒ 没人声明（deck 里没 106）时**恒落真空入口** ⇒ 道具**每帧无条件真空**
 *   （owner 实跑「同装 th14 和 15 的时候，一直在全屏吸收」）。
 *   ⇒ 现在**先按 `regs->eflags` 复现 `ja`**（`ja` = `CF==0 && ZF==0`，即 `!(eflags & 0x41)`；
 *     前一条 `comiss xmm0,[esi+0x624]` 的结果就在那儿），**再**与「满 P 闸」与一下。
 *   为什么 `eflags` 可用：thcrap 的桩在**任何改标志的指令之前**就 `lahf`/`seto al`/`push eax` 存了它
 *   （一手桩模板见 `engine/_shared/thcrap-breakpoint-pools.md` §2；`x86_reg_t` 的第一个字段就是 `eflags`）。
 * ⚠ **两条都要挂**：只挂一条 = 另一条支路照旧真空（静默半失效）。
 * ⚠ 回收线本身照旧可见（我们不碰 `player+0x47998`）——这正是用户选的「精确门」而不是「把线抬走」。*/
/* ★★★ 一次性**复现用** handler（默认不声明、默认不导出；只有 `CE_CANARY_POC_GATE=1` 时才由
 * `sites.py` 的**测试归档**把两个站点声明出来，见 `emit_test_patch()` 与 `AUDIT.md` §BE-DW 的验证配方）。
 *
 * 用途：把「窗口 = 一条 `0f 8x rel32`」的站点按**原样**声明回去，验证 §BE-DW 的算术：
 *   恒返回 1 ⇒ thcrap 让 cave **原样回放**窗口 ⇒ 那条 `ja` 在新位置跳到「本格 + (0x445f63 − 站点)」
 *   ⇒ 预期崩点 = **首个全零格 − 0x10 + 0x3aa**（例：首个全零格 0x2d0 ⇒ 池基 + 0x69a）。
 * 零副作用：不读任何状态、不写任何内存、不改 regs（`regs` 都没用）。
 * ⚠ 跑完必须按配方复原（撤 env ⇒ gen ⇒ gen_def ⇒ deploy），别把它留在发布面。*/
int __cdecl BP_ce_canary_a(x86_reg_t *regs, void *bp_info)
{
    (void)regs; (void)bp_info;
    return BP_EXEC_ORIGINAL;      /* 1 = 回放窗口（正是我们要复现的那条路）*/
}

int __cdecl BP_ce_canary_b(x86_reg_t *regs, void *bp_info)
{
    (void)regs; (void)bp_info;
    return BP_EXEC_ORIGINAL;
}

#define CE_POC_VACUUM   0x445f63u   /* 两条 `ja` 的共同目标 = 道具进真空态（一手：mov eax,[esi+0x47988] 起头）*/

static int ce_poc_gate_common(x86_reg_t *regs, uint32_t fall_through)
{
    /* ① 复现窗口里那条 `ja`：一手指令对（两个站点同形）
     *      `movss xmm0,[esi+0x47998]`（回收线）→ `comiss xmm0,[esi+0x624]`（自机 y）→ `ja …`
     *    ⇒ 条件 = **回收线 > 自机 y**（`ja` = CF==0 && ZF==0；`comiss` 的无序情形两边都为 1 ⇒ 不成立，
     *      正好与浮点比较 `>` 对 NaN 的行为一致）。
     *    ★ **不读 `regs->eflags`**：thcrap 的桩是用 `lahf / seto al / push eax` 存标志的，位布局与标准
     *      EFLAGS **不同**（CF 在位 8、ZF 在位 14、OF 在位 0）⇒ 按 `!(eflags & 0x41)` 去读实际读的是
     *      OF 与 AL 的残留位 ⇒ **恒真** ⇒ 又变回"无条件真空"。本仓 §BE-EO 第一版就栽在这上面
     *      （owner 实跑「依旧全吸」）。⇒ 改用**重算操作数**：`esi` 在桩里被 `pushad` 可靠保存。
     *      ⚠ 站点上 `esi` = 自机对象（两个偏移都是自机字段，见上面的一手对）。*/
    const uint8_t *pl = (const uint8_t *)(uintptr_t)regs->esi;
    float line = 0.0f, py = 0.0f;
    int take = 0;
    if (pl) {
        memcpy(&line, pl + 0x47998, sizeof line);          /* 回收线（不碰它，只读）*/
        memcpy(&py,   pl + 0x624,   sizeof py);            /* 自机 y */
        /* 合理性闸：两个值都在游玩区量级内才敢下结论；读不到就**不吸**（宁可回到"不吸"，
         * 也不要重演"每帧无条件真空"那次事故）。只在第一次异常时记一行。*/
        if (line > -2000.0f && line < 2000.0f && py > -2000.0f && py < 2000.0f) {
            take = (line > py);
        } else {
            static int warned;
            if (!warned) { warned = 1;
                ce_log("WARN: poc-gate: 可疑操作数 (line %.1f, player.y %.1f) @esi=%p — 本次按"
                       "「不吸」处理（见 AUDIT §BE-EP）", (double)line, (double)py, (const void *)pl); }
        }
    }
    /* ★★ 2026-09-20（统筹会话要求，出处 = `AUDIT.md` §BE-ER）：**删掉了原来那句
     *     `if (ce_full_p_gate_owner() < 0) return BP_EXEC_ORIGINAL;`
     *   它是 2026-09-19 那次崩溃的直接元凶：返回 1 ⇒ thcrap 从**跳板池**回放本站点窗口的原始字节
     *   （`0f 87 a4 03 00 00` = 一条裸 `ja rel32`），而 `cave_fix` **只重定位窗口首条的 `e8/e9`**
     *   ⇒ 那条 `ja` 按**原相对位移**从**池的地址**跳 ⇒ 落到池的未写区（`00 00` = `add [eax],al`，
     *   此刻 `Eax = 1`）⇒ **写 `0x1` 的访问违例**。逐字节复算见 §BE-ER（dump 的 `Eip = 0x03c205fa`
     *   与「池格 `0x03c20250` + 6 + 0x3a4」逐位对上）。
     *   ⇒ 本函数的**唯一合法形态是「恒返回 0 + 自己给落点」**（与两条闸 park 的前提一致）：
     *   `sitecheck` 的 W6（2026-09-20 立）把这条写成硬规则 —— 声明了 `no_replay` 的站点，
     *   handler 里出现 `BP_EXEC_ORIGINAL` 就是红。
     *   ⚠ 这两条站点**保持 park**（106 的闸走 PoC 高度 + 语义高度那条不接管的路，见 §BE-EQ / §7-A）。*/
    ce_retaddr_set(regs, (take && !ce_full_p_gate_active()) ? CE_POC_VACUUM : fall_through);
    return 0;                        /* 恒 0：不回放，也就不进 cave（窗口是 0f 8x，进 cave 会跳飞）*/
}

int __cdecl BP_ce_poc_gate_a(x86_reg_t *regs, void *bp_info)
{
    (void)bp_info;
    return ce_poc_gate_common(regs, 0x445bbfu);
}

int __cdecl BP_ce_poc_gate_b(x86_reg_t *regs, void *bp_info)
{
    (void)bp_info;
    return ce_poc_gate_common(regs, 0x446009u);
}

/* ★ `BP_ce_player_die` —— 「中弹无伤」闸门（一手链：engine/player/th18/03-player-hit-gate.md）。
 *
 * 站点 = `Player__die` `th18:0x45d3a0` 的入口序言（6 字节，exe 里 `55 8b ec 83 ec 14`）。
 * **闸门开着 ⇒ 整段函数体不执行**：返回 0（不回放原指令）+ 把 `retaddr` 改成该函数唯一的 `c3`
 * （`th18:0x45d584`；逐字节核过：`0x45d3a0`–`0x45d583` 区间只有这一条 `ret`，是**裸 `c3`**、
 * 不是 `ret N`，前面就是函数自己的 `pop edi / pop esi / pop ebx / mov esp,ebp / pop ebp`）。
 *
 * ★ **这个站点就是 TH13 那道闸门的同一语义槽位**（`th13:0x444ca4` 的 `test dword [0x4be830],0x100`
 *   = `Player::on_hit` 体的第一件事）⇒ 闸门行为与一手 TH13 等价。
 *   ⚠ **别被函数名骗了**：`0x45d3a0` 在 th-re-data 里叫 `Player__die`，但**按内容它是 TH13 的 `on_hit`**
 *   （音效 / 特效 / 状态 4 / 无敌 6 全在这一个函数里）；TH13 的 `Player::die`（扣命那半段）在 TH18 的
 *   对应物是 `0x45d090` `Player__commit_death_and_enter_state2`。2026-09-13 就是这条命名陷阱让
 *   `AUDIT` §CD-3 误判成「我们的闸门在 die 上、音效/miss 先发生」—— 实际反了。
 *
 * 一手：本站在**函数入口**，下面这些**全部在站点之后**（都在被跳过的函数体里）⇒ 开着闸门时一个字节都不发生：
 *   ① 中弹音 `0x45d3b4`（`push 2`；`0x45d3b7 call 0x476be0`）；
 *   ② 中弹特效 VM 脚本 `0x1d` `0x45d3f7`；
 *   ③ 状态 → 4（决死窗口）`0x45d4e4`；无敌 6 帧 `0x45d50d`；
 *   ④ 决死窗口帧数 `player+0x47908 = 8` `0x45d557`；卡虚表 `+0x10` 救援广播 `0x45d574`。
 *   **记 miss / 扣命更晚、且只能从状态 4 走到**：`0x45d090` 的 `[0x4ccd18]++`（`0x45d38b`）与命数 `--`
 *   （`0x45d19b`），而 `0x45d090` 全 exe 只有 1 个调用点 `0x45c135`（`Player__on_tick__body` 的 case 4：
 *   `0x45c0f4` 拿窗口计数器 `player+0x638` 比 `player+0x47908`、`0x45c12d` 有卡认领就跳走）；
 *   进 case 4 的唯一途径是状态 4，而状态 4 全 exe**只在 `0x45d4e4` 被写过**（`find_struct_writes`）。
 *
 * ⚠ 两个必须知道的边界：
 *   ① **擦弹不受影响**：擦弹与击中在判定函数里是**返回值互斥**的（1 = 击中 / 2 = 擦弹），
 *      本站在分流之后；判定函数仍返回 1 ⇒ 调用侧照原样把弹吃掉（`th18:0x4249be`，与 TH13
 *      `th13:0x40dd0c` 逐条同形）；
 *   ② **尾部的卡虚表 `+0x10` 救援广播（`th18:0x45d574`）也一并被跳过**
 *      —— 靠本闸门自救的那张卡，**补死（补调 `Player__die`）之前必须先撤声明**。
 *
 * 声明层在 `sdk_core.c`（`ce_no_hit_declare`，粘性、任一声明即生效）。默认无人声明 ⇒ 逐字节放行。
 *
 * ★ 2026-09-13 追加一件**只记账**的事：本站点同时是**死亡结果闩锁的复位点**
 *   （`ce_death_result_reset()` —— 一次死亡开始了，最终结果还不知道；判定处见 `BP_ce_death_result`）。
 *   复位**在闸门检查之前**：闸门开着时这次「死亡」根本没发生 ⇒ 结果保持 UNKNOWN（正确），
 *   而绝不能带着上一次死亡的 `1` 过去。除了这里，`ce_sdk_setup` 也复位一次。*/
static int s_no_hit_gate_hits;

int __cdecl BP_ce_player_die(x86_reg_t *regs, void *bp_info)
{
    (void)bp_info;
    ce_death_result_reset();                   /* ★ 一次死亡开始 ⇒ 最终结果未知（见上）*/
    if (!ce_no_hit_active()) return BP_EXEC_ORIGINAL;

    ce_retaddr_set(regs, CE_FN_PLAYER_DIE_RET); /* ★ 落闸：跳到唯一的 ret，函数体一行都不跑 */
    ++s_no_hit_gate_hits;
    if (s_no_hit_gate_hits <= 3 || (s_no_hit_gate_hits % 60) == 0)
        ce_log("no-hit gate: Player__die (= TH13 on_hit body) skipped (#%d, declared by card %d) - "
               "no miss / no sound / no effect / no deathbomb window; "
               "bullet still eaten, graze unaffected", s_no_hit_gate_hits, ce_no_hit_owner());
    return 0;                                  /* 0 = 不回放被覆盖的原指令 */
}

/* ★★ 2026-09-13 第五轮：**敌人吃到伤害**（共享事件，113 的「Boss 随血量掉灵」= TH13 通道 A）。
 *   站点 `0x42f2cb`（`EnemyData::step_game_logic` `0x42ed40` 内），被偷的指令 = 引擎自己的
 *   `add [ebx+0x5008],esi`（累计受伤记账）⇒ **handler 只读、原样放行**。
 *   一手（`REPORT-BOSS-SPIRIT-DROP.md` §2–§4）：
 *     · `ebx` = `zEnemyData*`（= `zEnemy*+0x122c`）、`esi` = 本帧最终伤害；
 *     · 站点在两道免伤闸之后 ⇒ "HP 即将减少"；`0x42ed40` 每敌每帧一次（唯一调用方 `0x42fff0`）；
 *     · boss 判据 = `flags_lo`（`enemy+0x635c`）**bit23**（ECL opcode 512 `setBoss`；
 *       22 个 .ecl 普查：只有 st0Xbs/st0Xbs 各一次 ⇒ 该位 ⟺ boss/道中 boss）。
 *   ⚠ 卡侧纪律：回调里**只记标量**（id/damage/x/y），任何建 VM 都留到帧末。*/
static int s_enemy_damage_selfcheck;
int __cdecl BP_ce_enemy_damage(x86_reg_t *regs, void *bp_info)
{
    (void)bp_info;
    uint8_t *mgr = CE_ABILITY_MGR();
    uint8_t *data = (uint8_t *)(uintptr_t)regs->ebx;         /* zEnemyData* = zEnemy*+0x122c */
    if (!mgr || !data) return BP_EXEC_ORIGINAL;

    ce_enemy_damage_t ev;
    /* ★★ 2026-09-13 第五轮修（用户实跑「掉灵和距离反相关」⇒ 查日志发现全是垃圾值）：
     *   **`ebx` 就是 `zEnemyData*`，不是 `zEnemy*`** —— 序言 `0x42ed4f mov ebx,ecx` 收的 this 是
     *   `zEnemy+0x122c`（调用方 `0x42fed7 lea ecx,[ebx+0x122c]` 一手）。上一版我在这里写成了
     *   `e = data + 0x122c`（**符号写反**）⇒ 之后所有 zEnemy 口径的读都偏了 0x2458 字节，
     *   读出来的是浮点区 ⇒ 日志里 `id 1063116254 / hp -1073990863 / pos (0,0)` 全是这个原因
     *   （合理性闸把它全挡了 ⇒ `boss hits 0(drops 0, rejected 4)`）。
     *   ⇒ 正确做法：**直接用 data 口径的偏移**（这些才是引擎自己在这段代码里用的那些）：
     *     生命 `+0x4ff4`（`0x42f314 sub [ebx+0x4ff4],esi`、`0x42f39d cmp [ebx+0x4ff4],0` = 死亡判据）；
     *     坐标 `+0x44`（`0x42ee31 lea edi,[ebx+0x44]` = 判定用的位置）；
     *     flags `+0x5130`（序言 `0x42ed53 mov ecx,[ebx+0x5130]`）；
     *     id `+0x5604`（= `zEnemy+0x6830`，`data = zEnemy+0x122c`）。*/
    uint32_t flags = *(const uint32_t *)(data + 0x5130);
    ev.id     = *(const uint32_t *)(data + 0x5604);
    ev.damage = (int32_t)regs->esi;
    ev.hp_cur = *(const int32_t *)(data + 0x4ff4);
    ev.hp_max = *(const int32_t *)(data + 0x5004);
    ev.flags  = (flags & CE_ENEMY_FLAG_BOSS) ? CE_EDAMAGE_F_BOSS : 0u;
    ev.x      = *(const float *)(data + 0x44);
    ev.y      = *(const float *)(data + 0x48);
    ev.enemy  = *(void **)(data + 0x5588);                   /* zEnemyData.full 回指针（诊断用，卡侧不碰）*/
    /* 一次性自检：回指针应当 = data − 0x122c；不等只记一行（不拦功能），留给下一次实跑判读。*/
    if (!s_enemy_damage_selfcheck) {
        s_enemy_damage_selfcheck = 1;
        if (ev.enemy != (void *)(data - 0x122c))
            ce_log("WARN: sdk: ce_enemy_damage selfcheck: full=%p data=%p (expect full = data - 0x122c)",
                   ev.enemy, (void *)data);
    }

    unsigned guard = 0;
    for (const uint8_t *node = *(const uint8_t *const *)(mgr + CE_MGR_CARD_LIST_FIRST);
         node && guard < 256;
         node = *(const uint8_t *const *)(node + CE_NODE_NEXT), ++guard) {
        uint8_t *card = *(uint8_t *const *)(node + CE_NODE_CARD);
        if (!card) continue;
        const ce_hooks_t *h = hooks_of(card);
        if (h && h->on_enemy_damage) {
            h->on_enemy_damage((ce_card_t *)card, &ev);
            ce_sdk_trace(ce_card_id(card), 0x85, "on_enemy_damage");
        }
    }
    return BP_EXEC_ORIGINAL;                                 /* ★ 引擎的 `add` 必须照跑 */
}

int __cdecl BP_ce_enemy_death(x86_reg_t *regs, void *bp_info)
{
    (void)bp_info;
    uint8_t *mgr = CE_ABILITY_MGR();
    uint8_t *e = (uint8_t *)(uintptr_t)regs->ecx;      /* zEnemy；唯一可信的寄存器 */
    if (!mgr || !e) return BP_EXEC_ORIGINAL;

    ce_enemy_death_t ev;
    uint32_t flags = *(const uint32_t *)(e + CE_ENEMY_FLAGS);
    ev.id        = *(const uint32_t *)(e + CE_ENEMY_ID);
    ev.esp       = regs->esp;
    ev.retaddr   = edeath_call_site(regs, &ev.ret_slot);
    ev.kind      = edeath_kind(ev.retaddr);
    ev.flags     = ((flags & 0x31u) == 0 ? CE_EDEATH_F_REAL : 0u);   /* ★ 置位 = 被排除在真敌人之外 */
    ev.drop_main = *(const int32_t *)(e + CE_ENEMY_DROP);
    /* ★ 2026-09-22 夜：`by_player_bullet`（108 的 P-D 用）—— 取「主路被打死」这一档，见 `public.h` 的注。*/
    ev.by_player_bullet = (ev.kind == CE_EDEATH_KILLED) ? 1 : 0;
    ev.x         = *(const float *)(e + CE_ENEMY_POS_X);
    ev.y         = *(const float *)(e + CE_ENEMY_POS_X + 4);
    ev.enemy     = e;
    if (ev.drop_main != 0) ev.flags |= CE_EDEATH_F_HAS_DROP;
    /* ★ 2026-09-22 夜：`by_player_bullet`（108 的 P-D 用）—— 取「主路被打死」这一档，见 `public.h` 的注。*/
    ev.by_player_bullet = (ev.kind == CE_EDEATH_KILLED) ? 1 : 0;

    unsigned guard = 0;
    for (const uint8_t *node = *(const uint8_t *const *)(mgr + CE_MGR_CARD_LIST_FIRST);
         node && guard < 256;
         node = *(const uint8_t *const *)(node + CE_NODE_NEXT), ++guard) {
        uint8_t *card = *(uint8_t *const *)(node + CE_NODE_CARD);
        if (!card) continue;
        const ce_hooks_t *h = hooks_of(card);
        if (h && h->on_enemy_death) {
            h->on_enemy_death((ce_card_t *)card, &ev);
            ce_sdk_trace(ce_card_id(card), 0x84, "on_enemy_death");
        }
    }
    return BP_EXEC_ORIGINAL;
}

/* ============================ ★★★ 2026-09-24 妖精认领通报位（110/111 ↔ 112）============================
 * 语义与三条纪律写在 `sdk.h` 的声明处（唯一出处）。实现要点：
 *   · 两条小表（组 / 实例），条目 = {卡号, 键, 帧号}；**帧号不等于当前帧 ⇒ 视为过期**（无需清理钩子）；
 *   · 每帧重申 = 覆盖同键条目的帧号；满了就丢（诊断计数），不覆盖别人的条目。*/
typedef struct { uint32_t card; int32_t key, key2, frame; } ce_fairy_claim_t;
static ce_fairy_claim_t s_gclaim[CE_FAIRY_CLAIM_MAX];
static ce_fairy_claim_t s_iclaim[CE_FAIRY_CLAIM_MAX];

void ce_fairy_claim_group(uint32_t card_id, int32_t anm_src, int32_t anm_script)
{
    int i, free_i = -1;
    int32_t fr = (int32_t)ce_sdk_frame();
    if (!card_id) return;
    for (i = 0; i < CE_FAIRY_CLAIM_MAX; ++i) {
        if (s_gclaim[i].card == card_id && s_gclaim[i].key == anm_src && s_gclaim[i].key2 == anm_script) {
            s_gclaim[i].frame = fr;
            return;
        }
        if (free_i < 0 && (s_gclaim[i].card == 0 || s_gclaim[i].frame != fr)) free_i = i;
    }
    if (free_i < 0) { ce_log("fairyclaim: 组通报表满（%d 条）—— 这一帧没通报上", (int)CE_FAIRY_CLAIM_MAX); return; }
    s_gclaim[free_i].card = card_id; s_gclaim[free_i].key = anm_src;
    s_gclaim[free_i].key2 = anm_script; s_gclaim[free_i].frame = fr;
}

int ce_fairy_group_claimed(int32_t anm_src, int32_t anm_script)
{
    int i;
    int32_t fr = (int32_t)ce_sdk_frame();
    for (i = 0; i < CE_FAIRY_CLAIM_MAX; ++i)
        if (s_gclaim[i].card && s_gclaim[i].frame == fr
            && s_gclaim[i].key == anm_src && s_gclaim[i].key2 == anm_script) return 1;
    return 0;
}

void ce_fairy_claim_instance(uint32_t card_id, int32_t enemy_id)
{
    int i, free_i = -1;
    int32_t fr = (int32_t)ce_sdk_frame();
    if (!card_id || !enemy_id) return;
    for (i = 0; i < CE_FAIRY_CLAIM_MAX; ++i) {
        if (s_iclaim[i].card == card_id && s_iclaim[i].key == enemy_id) { s_iclaim[i].frame = fr; return; }
        if (free_i < 0 && (s_iclaim[i].card == 0 || s_iclaim[i].frame != fr)) free_i = i;
    }
    if (free_i < 0) return;
    s_iclaim[free_i].card = card_id; s_iclaim[free_i].key = enemy_id; s_iclaim[free_i].frame = fr;
}

int ce_fairy_instance_claimed(int32_t enemy_id)
{
    int i;
    int32_t fr = (int32_t)ce_sdk_frame();
    for (i = 0; i < CE_FAIRY_CLAIM_MAX; ++i)
        if (s_iclaim[i].card && s_iclaim[i].frame == fr && s_iclaim[i].key == enemy_id) return 1;
    return 0;
}

void ce_fairy_claim_clear(uint32_t card_id)
{
    int i;
    if (!card_id) return;
    for (i = 0; i < CE_FAIRY_CLAIM_MAX; ++i) {
        if (s_gclaim[i].card == card_id) { s_gclaim[i].card = 0; s_gclaim[i].frame = 0; }
        if (s_iclaim[i].card == card_id) { s_iclaim[i].card = 0; s_iclaim[i].frame = 0; }
    }
}
