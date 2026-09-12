/* sdk.c —— 行为 SDK 的平台侧：绑定断点、门里的核对与对账、事件断点、trace。机制见 ../SDK.md。
 *
 *   BP_ce_card_bind    0x412cec  mov [esi+4],ebx（分配公共尾段：esi = 卡对象，ebx = id）
 *                      id 有登记的行为 → 对象虚表换成 DLL 里那份；放行原指令。此时 ctor 槽还没被调。
 *   BP_ce_item_score   0x446cf6  lea eax,[edi+0xc2c]（collect_money_item：esi = 道具身价，已钳 ≥ 10；
 *                      后面 push esi 给弹窗、mul esi 进计分）。沿卡链表调 on_item_score(&esi)。
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
 *   BP_ce_enemy_shot_slot 0x433ffa 敌人 ECL etOn(opcode 0x259) 的「发弹三步」汇聚点：沿卡链表问
 *                      on_enemy_shot_slot(ev)，返回非 0 ⇒ retaddr = 0x43402d 本帧不发射这一槽（108 使魔）。
 *   ce_sdk_on_shop_open          不是断点：shop.c 的 BP_ce_shop_reopen 在 MSG 开店那一帧调它，沿卡链表调 on_shop_open()。
 *   ce_destroy_card              局中删一张卡（倒吊人）：照 recount 0x4080e0 的删卡分支，AUDIT §W。
 *   ce_sdk_setup       门里：基类虚表 21 槽与 engine.h 的常量逐一比对（布局假设的运行时守卫）、十二个断点已挂、
 *                      与 cards.js 对账（C 有 JSON 无 → FAIL）、每张行为卡记一行。
 */
#include <string.h>
#include <stdio.h>
#include "card_expand.h"
#include "thcrap_bp.h"
#include <stdarg.h>
#include <stdio.h>
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
#define CE_TRACE_SLOTS 40                  /* 21 个虚表槽 + 事件 0x54..0x88（槽号/4 当数组下标 ⇒ 40 覆盖到 0x9c）*/
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
        /* 伤害层：绑定 = 这张卡在场，把它声明的初值登记进聚合（`+0x50` 删对象时清 0）。AUDIT §AA */
        if (h && h->bullet_damage_permille) ce_dmg_set(regs->ebx, h->bullet_damage_permille);
        if (s_trace) ce_log("trace: card %u object %08x bound to vtable %p%s", regs->ebx, regs->esi, b->vtable,
                            h && h->active_recharge ? " (active)" : equip ? " (equipment)" : "");
    }
    return BP_EXEC_ORIGINAL;
}

/* ---- 自机弹伤害倍率层（AUDIT §AA）----------------------------------------
 * 聚合与「谁落笔」在 sdk_core（纯逻辑、主机单测），这里只做两件带引擎的事。
 *
 * `+0x28`：`PlayerBullet__create` `0x45e320` **先**写 `bullet+0x9c = shooter+0x02`、**再**遍历卡链表调 `+0x28`
 * （一手，AUDIT Z3 的反编译里看得到顺序），所以桩看到的是引擎刚写好的值。只有落笔人乘一次 ⇒ 单次取整。
 * 就地乘而不是「从 shooter 基值重算」：零售 `CardMomoyo__on_bullet_init` `0x411010` 就是重算派
 * （`bullet+0x9c = (1 + 擦弹/N) × shooter+0x02`），重算会把她的加成抹掉；就地乘则与她相乘（顺序依赖是既有性质）。
 *
 * `+0x2c`：每敌每帧总伤害上限 `player+0x47984` 每帧从 `sht+0x28` 复位（GameThread 0x10 早于 AbilityManager 0x16），
 * 不按同一倍率抬起来，加的伤害在贴脸打 boss 时会被配额吃掉（§V20 / §Y5 / §AJ10 是同一件事的三次重犯）。
 * 这一层把它变成自动的：卡只要报倍率，上限跟着走。*/
void ce_sdk_scale_bullet_damage(uint32_t id, void *bullet)
{
    int32_t *dmg;
    if ((int)id != ce_dmg_applier() || ce_dmg_aggregate() == CE_DMG_UNIT || !bullet) return;
    dmg = (int32_t *)((uint8_t *)bullet + CE_BULLET_DAMAGE);
    *dmg = ce_dmg_scale(*dmg);
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

/* HUD 文字。零售 `CardMomoyo__on_draw` `0x4110a0` 的做法逐条复刻（AUDIT §AC）：
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
 * （`PLAYER+0x1570`、512 × 0x9c）、`PlayerBullet__create` `0x45e3ae` 把子机指针写进 `bullet+0xe4`、
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

/* 收起子机：删它的 ANM VM（零售 `CardReimu1__operator_delete` `0x40ab20` 同款）、清零售在同一时机清的
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
            if (*card_vm == vm) *card_vm = 0;      /* 引擎那格也清掉，免得零售析构再删一次 */
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
        if (*vm) { ce_anm_delete(*vm); *vm = 0; }   /* o == NULL：零售单子机写法 */
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

/* 字符 → 零售字形 sprite。映射一手：`AsciiManager__draw_string` `0x41a2b0` 的 font 2/3 分支
 * （数字 `char + 0x94` ⇒ '0' = 0xC4 = 196；`-` = 0xd0、`%` = 0xd2、`.` = 0xd3、`+` = 0xd4）。
 * 切图见 assets/ability/make_font2_art.py，进 ability.anm 后就是 anm_ids.h 里的 FONT2_*。*/
static uint32_t hud_vm_glyph(char ch)
{
    if (ch >= '0' && ch <= '9') return CE_ANM_ABILITY_SPRITE_FONT2_0 + (uint32_t)(ch - '0');
    switch (ch) {
    case '-': return CE_ANM_ABILITY_SPRITE_FONT2_MINUS;
    case '%': return CE_ANM_ABILITY_SPRITE_FONT2_PERCENT;
    case '.': return CE_ANM_ABILITY_SPRITE_FONT2_DOT;
    case '+': return CE_ANM_ABILITY_SPRITE_FONT2_PLUS;
    default:  return 0;                      /* 空格等：按字距跳过 */
    }
}

static float hud_vm_ui_scale(void)
{
    float s = *(const float *)CE_ADDR_UI_SCALE;
    return (s > 0.1f && s < 16.0f) ? s : 2.0f;
}

void ce_hud_vm_text_clear(ce_hud_vm_text_t *t)
{
    int i;
    if (!t) return;
    for (i = 0; i < CE_HUD_VM_TEXT_MAX; ++i) { ce_anm_delete(t->id[i]); t->id[i] = 0; t->shown[i] = 0; }
    t->n = 0;
}

void ce_hud_vm_text_set(ce_hud_vm_text_t *t, float x, float y, uint32_t bgra,
                        int align_h, float scale, const char *s, const char *tag)
{
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

    /* ① 先量总宽（`.` 的字距是 4 而不是 7 —— 引擎的 `fVar11` 就是被它改掉的）*/
    for (p = s; *p; ++p) total += (*p == '.') ? 4.0f * sx * dat : 7.0f * sx * dat;
    pen = x * dat + ((align_h == CE_ASCII_ALIGN_CENTER) ? -total * 0.5f
                   : (align_h == CE_ASCII_ALIGN_RIGHT)  ? -total
                                                        : 0.0f);

    /* ② 字形墨迹上沿（= 引擎的 VM pos.y）：`record+0x134` 三个分支 ——
     *    0 居中（退半个行高 = 5×scale_y）、2 底（退一个行高 = 10×scale_y）、1 顶（不退）。*/
    ytop = y * dat;
    if (mgr) {
        int av = *(int *)(mgr + CE_ASCII_ALIGN_V);
        if (av == 0) ytop -= 5.0f * sy * dat;
        else if (av == 2) ytop -= 10.0f * sy * dat;
    }

    /* ③ 逐字摆：VM 位置 = (笔 x, 墨迹上沿 + 5×scale_y×DAT)。
     *    +5 来自贴图：16×32 的 entry 里 20 行字形上下各补 6 行，精灵中心 = 字形中心往下 1.5 行，
     *    折到「上沿 + 半个字形高」就是 +5（详见脚本 136 与 make_font2_art.py 的注释）。*/
    for (p = s; *p; ++p) {
        uint32_t spr = hud_vm_glyph(*p);
        float adv = (*p == '.') ? 4.0f * sx * dat : 7.0f * sx * dat;

        if (spr && slot < CE_HUD_VM_TEXT_MAX) {
            uint32_t *id = &t->id[slot];
            float pos[3];
            pos[0] = pen; pos[1] = ytop + 5.0f * sy * dat; pos[2] = 0.0f;
            if (*id && !ce_anm_get_vm(*id)) { *id = 0; t->shown[slot] = 0; }
            if (!*id) {
                *id = ce_anm_spawn_ui(anm, CE_ANM_ABILITY_SCRIPT_HUD_GLYPH, pos);
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

/* 两个站点共用一份实现（见 sites.py 的 ce_item_score / ce_item_score_below）：
 *   0x446cf6 = 「自机在回收线上方」或「道具 state == 3（真空飞行）」那一支；
 *   0x446caa = **线下且非真空飞行**那一支（0x446cb2 `jmp 0x446cfc` 绕过前者）。
 * 两支互斥 ⇒ 每次收集仍然只派发一次事件。*/
static int ce_item_score_common(x86_reg_t *regs)
{
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

int __cdecl BP_ce_item_score(x86_reg_t *regs, void *bp_info)
{
    (void)bp_info;
    return ce_item_score_common(regs);
}

int __cdecl BP_ce_item_score_below(x86_reg_t *regs, void *bp_info)
{
    (void)bp_info;
    return ce_item_score_common(regs);
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
            regs->retaddr = 0x45c056;   /* 原版: 0x45c048 test / 0x45c04f je / 0x45c051 call do_bomb */
            return 0;                    /* 不执行原 test，直接从这里继续 */
        }
    }
    return BP_EXEC_ORIGINAL;
}

/* Player__tick_shooting_state 0x45ea00 的查表点 0x45ebfb：原指令 `mov edi,[edi+edx*4+0xe0]`，
 * edx = 本帧从火力/聚焦算出的 set index，eax = 火力档；同帧 0x45ebed/0x45ebf8 已把同一 set index
 * 暂存在 [ebp-8]，0x45ec69 再打包成 `(set<<8)|row` 写进 PlayerBullet+0xac、命中时再回读。
 * 只改这两个位置：edx 管本帧从哪组取 row，[ebp-8] 管弹里记哪一组；不碰 sht 的 0..9 槽。
 * 任一卡的 on_shot_set_index 返回 >=0 时覆盖，否则原版放行。*/
int __cdecl BP_ce_shot_set_index(x86_reg_t *regs, void *bp_info)
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
        if (h && h->on_shot_set_index) {
            int idx = h->on_shot_set_index((ce_card_t *)card, (int)regs->eax);
            if (idx >= 0) {
                if (regs->edx != (uint32_t)idx) {
                    /* 两个副本必须一起改：edx 决定本帧取出哪组 row；[ebp-8] 决定 bullet+0xac 里存的 set。*/
                    regs->edx = (uint32_t)idx;
                    if (regs->ebp) *(uint32_t *)(uintptr_t)(regs->ebp - 8) = (uint32_t)idx;
                }
                ce_sdk_trace(ce_card_id(card), 0x7c, "on_shot_set_index");
                /* ★ 有卡覆写了 set（= 117 咆哮）⇒ **本帧不抑制**（咆哮优先：EVENTS-CONTRACT §5.2）。*/
                return BP_EXEC_ORIGINAL;
            }
        }
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
            regs->retaddr = 0x45eca0;
            return 0;                    /* 不执行原行选择，直接去卡广播 */
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

int __cdecl BP_ce_item_frame(x86_reg_t *regs, void *bp_info)
{
    (void)regs; (void)bp_info;
    uint8_t *mgr = CE_ABILITY_MGR();
    if (!mgr) return BP_EXEC_ORIGINAL;
    /* ★ 帧初：清「本帧已消过弹」闩锁（EVENTS-CONTRACT §5.2 细则 2）。这个断点每个正常游玩帧到一次，
     * 且在道具池 body 之前 ⇒ 是天然的帧边界。107 的结界灵击与 117 的霊撃同帧都触发时，
     * 只有第一个 claim 的负责全屏消弹，第二家让位（避免空池扫荡 / 重复记账）。*/
    ce_cancel_latch_reset();
    /* ★ 帧初：清「这次死亡由谁救」闩锁（用户 2026-09-12 定案：救援层 107 → 117 → 决死 B → 113；
     * 113 只在没人认领时才动手 —— 否则会「别人救了你、113 的窗口结束时又把你杀掉」）。*/
    ce_rescue_note_reset();
    /* ★ 帧初：把「无敌声明层」的聚合值落到 `player+0x47774`（取各卡声明帧数的**最大值**，
     * 只在比当前计时器更长时才写 ⇒ 不截短别人给的无敌、也不动引擎自己的）。
     * 卡只需要 `ce_invuln_request(id, frames)` 声明，落笔由 SDK 统一做（单一落笔，EVENTS-CONTRACT §1）。*/
    ce_invuln_apply();
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
    if (!card_in_list(mgr, victim)) { ce_log("destroy: refused, card %u not in the list", ce_card_id(victim)); return 0; }
    /* 当前选中的主动卡也能删（2026-09-09）。原先一律拒绝，于是「卡组里只剩一张主动卡」时
     * 倒吊人会白买一次。做法照零售 recount 的收尾：先把 mgr+0x38 摘空，删完再让引擎自己重选。
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
        ce_log("destroy: card %u was the selected active card; reselect -> %s", id, ok ? "ok" : "none left");
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

int ce_sdk_setup(uint8_t *base, int trace)
{
    ce_dmg_reset();                                   /* 伤害层：从干净的聚合开始（§AA）*/
    ce_invuln_reset();                                /* 无敌取最大层（EVENTS-CONTRACT §4）*/
    ce_score_reset();                                 /* 计分声明层（EVENTS-CONTRACT §3）*/
    ce_optpose_reset();                               /* 低速子机选举层（§1 选举覆盖）*/
    ce_cancel_latch_reset();                          /* 「本帧已消过弹」闩锁（§5.2 细则 2）*/
    ce_rescue_note_reset();                           /* 「这次死亡由谁救」闩锁（救援层顺序，§5.2）*/
    ce_hud_slots_reset();                             /* HUD 槽位分配器（HUD-LAYOUT §3）*/
    ce_drop_reset();                                  /* 掉落乘法层（EVENTS-CONTRACT §2）*/
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
    /* 2. 十四个断点已挂。聚合检查：任何一个失败都打印现场字节（判断 thcrap 没下断 vs 下错位置），
     * 全部查完再决定成败——避免第一个失败点挡住后面断点的诊断信息。 */
    struct { const char *name; uint32_t rva; unsigned len; } bps[] = {
        { "ce_card_bind",   CE_BP_CARD_BIND_RVA,   6 },
        { "ce_item_score",  CE_BP_ITEM_SCORE_RVA,  6 },
        { "ce_item_score_below", CE_BP_ITEM_SCORE_BELOW_RVA, 8 },
        { "ce_item_money",  CE_BP_ITEM_MONEY_RVA,  6 },
        { "ce_bomb_spent",  CE_BP_BOMB_SPENT_RVA,  5 },
        { "ce_enemy_drop",  CE_BP_ENEMY_DROP_RVA,  6 },
        { "ce_item_frame",  CE_BP_ITEM_FRAME_RVA,  7 },
        { "ce_item_collect",CE_BP_ITEM_COLLECT_RVA,6 },
        { "ce_item_commit", CE_BP_ITEM_COMMIT_RVA, 6 },
        { "ce_shot_damage", CE_BP_SHOT_DAMAGE_RVA, 7 },
        { "ce_bomb_gate",   CE_BP_BOMB_GATE_RVA,   7 },
        { "ce_shot_set_index", CE_BP_SHOT_SET_INDEX_RVA, 7 },
        { "ce_enemy_death", CE_BP_ENEMY_DEATH_RVA, 6 },
        { "ce_enemy_shot_slot", CE_BP_ENEMY_SHOT_SLOT_RVA, 8 },
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
            regs->retaddr = 0x43402du;                                       /* opcode 收尾：本帧不发射 */
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

int __cdecl BP_ce_enemy_death(x86_reg_t *regs, void *bp_info)
{
    (void)bp_info;
    uint8_t *mgr = CE_ABILITY_MGR();
    uint8_t *e = (uint8_t *)(uintptr_t)regs->ecx;      /* zEnemy；唯一可信的寄存器 */
    if (!mgr || !e) return BP_EXEC_ORIGINAL;

    ce_enemy_death_t ev;
    uint32_t flags = *(const uint32_t *)(e + CE_ENEMY_FLAGS);
    ev.id        = *(const uint32_t *)(e + CE_ENEMY_ID);
    ev.kind      = edeath_kind(regs->retaddr);
    ev.flags     = ((flags & 0x31u) == 0 ? CE_EDEATH_F_REAL : 0u);   /* ★ 置位 = 被排除在真敌人之外 */
    ev.drop_main = *(const int32_t *)(e + CE_ENEMY_DROP);
    ev.x         = *(const float *)(e + CE_ENEMY_POS_X);
    ev.y         = *(const float *)(e + CE_ENEMY_POS_X + 4);
    ev.enemy     = e;
    if (ev.drop_main != 0) ev.flags |= CE_EDEATH_F_HAS_DROP;

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
