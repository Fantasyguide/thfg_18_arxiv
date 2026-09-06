/* sdk.h —— 写一张行为卡时 include 这个。用法与机制见 ../SDK.md。
 *
 *   #include "sdk.h"
 *   static int on_death_frame2(ce_card_t *c) { ... return 0; }
 *   CE_CARD(62, .on_death_frame2 = on_death_frame2);
 *
 * CE_CARD 展开：这张卡的回调表 → 21 个 __thiscall 桩 → 一份 21 槽虚表 → 登记进 ce_behaviors。
 * 桩按虚表分派（每张卡一套桩），没写的回调桩返回 0（= 基类槽 `xor eax,eax; ret` 的效果）。
 * +0x08（C 键）由 SDK 的主动卡机器接管（非主动卡等价于基类的 ret 0）；+0x38 / +0x3c / +0x40（充能存取，HUD 与 replay 用）直接指基类实现。
 * +0x50 operator_delete：先释放私有状态，再进基类的 delete。
 */
#pragma once
#include <stdint.h>
#include "engine.h"
#include "anm_ids.h"      /* build_ability.py 生成：ability.anm 追加的 sprite / 脚本号 */
#include "sdk_core.h"

typedef struct ce_card ce_card_t;          /* = zCardBaseClass*，只通过下面的取值函数看 */
#define CE_ACTIVATE_REFUSED (-1)           /* on_activate 的返回值：拒绝发动（SDK 退回充能，卡自己放 0x10 无效音）*/

#define ce_card_id(c)     (*(uint32_t *)((uint8_t *)(c) + CE_CARD_ID))
#define ce_card_entry(c)  (*(uint8_t **)((uint8_t *)(c) + CE_CARD_TABLE_ENTRY))
#define ce_card_flags(c)  (*(uint32_t *)((uint8_t *)(c) + CE_CARD_FLAGS))
#define ce_state(c, T)    ((T *)ce_state_user((c), sizeof(T)))      /* 私有状态（SDK §4）：块头 16 字节是 SDK 的（主动卡状态机），卡从其后起 */

typedef struct {
    int  (*ctor)(ce_card_t *);                                   /* +0x00 非 0 = 当场删卡 */
    int  (*dtor)(ce_card_t *);                                   /* +0x04 */
    int  (*on_death_after_deathbomb)(ce_card_t *, uint32_t);     /* +0x0c 非 0 = 救下了 */
    int  (*on_death_before_deathbomb)(ce_card_t *);              /* +0x10 */
    int  (*on_death_frame2)(ce_card_t *);                        /* +0x14 结算之后 */
    int  (*on_power_level_change)(ce_card_t *);                  /* +0x18 */
    int  (*on_tick_shooters)(ce_card_t *, uint32_t, uint32_t);   /* +0x1c */
    int  (*on_load)(ce_card_t *);                                /* +0x20 */
    int  (*on_tick)(ce_card_t *);                                /* +0x24 Player tick 内（在移速倍率复位之前，别在这写移速）*/
    int  (*on_bullet_created)(ce_card_t *, void *bullet);        /* +0x28 */
    int  (*on_tick_2)(ce_card_t *);                              /* +0x2c AbilityManager tick，先于 Player tick；菜单里不跑 */
    int  (*on_enemy_drop)(ce_card_t *, uint32_t, uint32_t);      /* +0x30 */
    void (*on_stage_start)(ce_card_t *);                         /* +0x34 */
    int  (*on_hud_anm)(ce_card_t *, uint32_t);                   /* +0x44 */
    int  (*on_draw)(ce_card_t *);                                /* +0x48 */
    void (*on_run_reset)(ce_card_t *);                           /* +0x4c */
    /* 虚表之外的事件（SDK §6，断点实现）*/
    void (*on_item_score)(ce_card_t *, int32_t *value);          /* 道具身价算完、显示与计分之前 */
    void (*on_item_money)(ce_card_t *, int32_t *bonus);          /* 金钱道具入账（MONEY += 1）之前：*bonus 是额外要加的钱，MONEY 与 MONEY_TOTAL 一起加 */
    void (*on_enemy_drop_pre)(ce_card_t *, int32_t *counts);     /* 敌人撒道具**之前**：counts 是 CE_ENEMY_DROP_TYPES 个 int32，改它就改掉落数 */
    void (*on_bomb_spent)(ce_card_t *);                          /* 炸弹刚扣完（consume_bomb 返回那一条）：改 CURRENT/MAX_BOMBS 的时机，同帧生效不闪 */
    void (*on_shop_open)(ce_card_t *);                           /* 每关过关、MSG 请求开第一家店那一帧（商店对象还没建）：改 MONEY 店里就能看到；重开的第二家不算 */
    /* 主动卡（SDK §9）：active_recharge != 0 就是主动卡（C 键 / 充能 / HUD 由 SDK 与引擎处理）*/
    uint32_t active_recharge;                                    /* 充能帧数（×mgr+0xc58 倍率后装填）*/
    int  (*on_activate)(ce_card_t *);                            /* C 键发动：返回 0 = 瞬发（直接收尾），1 = 进入持续态，CE_ACTIVATE_REFUSED = 条件不满足（充能退回、不算发动）*/
    int  (*on_active_tick)(ce_card_t *, uint32_t elapsed);       /* 持续态每帧（elapsed = 经过帧）；返回 0 = 结束 */
} ce_hooks_t;

/* ---- 引擎调用（薄包装，签名见 engine.h）---- */
typedef int      (__attribute__((thiscall)) *ce_fn_alloc_t)(void *mgr, uint32_t id, uint32_t mode);
typedef void     (__attribute__((fastcall)) *ce_fn_mark_t)(uint32_t id, uint32_t notify);
typedef uint8_t *(__attribute__((fastcall)) *ce_fn_table_get_t)(uint32_t id);
typedef int      (__attribute__((fastcall)) *ce_fn_pick_t)(uint8_t **out, int tier_lo, int tier_hi, uint8_t **exclude, int n);

/* 给玩家一张卡（= 商店成交的两步：allocate_new_card(mode) + mark_obtained）。返回 allocate 的返回值。*/
static inline int ce_give_card(uint32_t id, uint32_t mode, uint32_t notify)
{
    void *mgr = CE_ABILITY_MGR();
    if (!mgr) return -1;
    int r = ((ce_fn_alloc_t)CE_FN_ALLOCATE_NEW_CARD)(mgr, id, mode);
    ((ce_fn_mark_t)CE_FN_MARK_OBTAINED)(id, notify);
    return r;
}
/* 表行（不依赖 card+0x4c：ctor 里它还没写）*/
static inline uint8_t *ce_table_entry(uint32_t id) { return ((ce_fn_table_get_t)CE_FN_TABLE_GET)(id); }
#define ce_entry_id(e)  (*(uint32_t *)((e) + 0x04))
#define ce_entry_tier(e) (*(int32_t *)((e) + 0x10))     /* price_tier（DATA.md §3）*/
/* 商店随机池抽一张（价格档 [lo, hi]，exclude 里的表行不抽）；返回表行或 NULL */
static inline uint8_t *ce_shop_pick_random(int tier_lo, int tier_hi, uint8_t **exclude, int n)
{
    uint8_t *e = 0;
    return ((ce_fn_pick_t)CE_FN_SHOP_PICK_RANDOM)(&e, tier_lo, tier_hi, exclude, n) ? e : 0;
}

/* 日志（card_expand.h 的 ce_log，卡里也能用：th18_card_expand.log 一行）*/
void ce_log(const char *fmt, ...);

/* 音效：play_sound(id) 是 stdcall，声像位置走 xmm2（世界 x）。三行内联汇编，AUDIT O22。*/
static inline void ce_play_sound(uint32_t id, float x)
{
    __asm__ volatile ("movss %[x], %%xmm2\n\t"
                      "pushl %[id]\n\t"
                      "call *%[fn]"
                      : : [x] "m"(x), [id] "r"(id), [fn] "r"((uintptr_t)CE_FN_PLAY_SOUND)
                      : "eax", "ecx", "edx", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7", "memory", "cc");
}

/* 语音：与 SE 完全同构 —— 可叠加、跟随游戏的 SE 音量、不做独占通道、不打断。
 * 只是 id 落在扩展区 0x54..0x73（音效表扩容，AUDIT §Q）。
 * NAME 来自 assets/voice/ORDER.txt，由 assets/build_voice.py 生成 voice_ids.h。
 *   ce_play_voice(SPADE_10_ACTIVATE, player_x());
 * 没登记语音时 voice_ids.h 是空的，用到不存在的 NAME 会在编译期报错 —— 这是想要的。 */
#include "voice_ids.h"
#define ce_play_voice(NAME, x)  ce_play_sound(CE_VOICE_##NAME, (x))

/* HUD 炸弹行刷新（Gui 0x4420e0：thiscall(gui; bombs, fragments, max) ret 0xc，与残机那个完全对称）。
 * 改 CURRENT_BOMBS / MAX_BOMBS 后必须调，零售在 consume_bomb 与每关开场都这么做。AUDIT §T。 */
typedef void (__attribute__((thiscall)) *ce_fn_gui_bombs_t)(void *gui, int bombs, int fragments, int max);
static inline void ce_gui_update_bombs(void)
{
    void *gui = CE_GUI();
    if (gui) ((ce_fn_gui_bombs_t)CE_FN_GUI_UPDATE_BOMBS)(gui, CE_CURRENT_BOMBS(), CE_BOMB_FRAGMENTS(), CE_MAX_BOMBS());
}

/* 炸弹：直接调引擎自己的 do_bomb（无参 cdecl）。返回 0 = 放出去了，-1 = 被它自己的守卫拦下
 * （已经在放 / +0xa0 非零）——**失败是安全的，什么都不会发生**。扣炸弹数由它内部做且钳 0，
 * 所以哪怕 CURRENT_BOMBS 已经是 0 也不会扣成负数，调用方不用碰计数器。AUDIT §R。 */
static inline int ce_do_bomb(void) { return ((int (*)(void))CE_FN_DO_BOMB)(); }

/* ANM：从一个已装载的 anm（CE_ABILITY_ANM() / 取 CE_MGR_ABCARD_ANM）起脚本，挂 world 列表，实体坐标 (0,0,0)
 * = 场地正中（脚本里 originMode(1)）。返回 anm id（0 = 失败）。脚本自己 delete() 的一次性特效不用记 id。
 * 只能在主线程（桩 / 断点里）调；引擎函数 thiscall + ret 0x10，四个栈参由被调方清（AUDIT O24）。*/
#define CE_ABILITY_ANM()  (*(void **)(CE_ABILITY_MGR() + CE_MGR_ABILITY_ANM))
typedef int *(__attribute__((thiscall)) *ce_fn_anm_inst_t)(void *anm, int *out_id, int script, int layer, void **out_vm);
static inline uint32_t ce_anm_spawn(void *anm, int script, int layer)
{
    int id = 0; void *vm = 0;
    if (!anm) return 0;
    ((ce_fn_anm_inst_t)CE_FN_ANM_INSTANTIATE_WORLD_BACK)(anm, &id, script, layer, &vm);
    return (uint32_t)id;
}

/* 残机 / 炸弹：引擎自己的加法（钳上限、音效、特效）。add_bomb 是 thiscall + 一个从不读取的栈参（ret 4），
 * 所以签名里必须带一个 dummy（O23 教训）。上限想一起加照零售：`if (CE_LIVES_MAX() < 7) CE_LIVES_MAX()++`。*/
typedef void (__attribute__((thiscall)) *ce_fn_add_life_t)(void *globals);
typedef void (__attribute__((thiscall)) *ce_fn_add_bomb_t)(void *globals, int unused);
static inline void ce_add_life(void) { ((ce_fn_add_life_t)CE_FN_ADD_LIFE)((void *)CE_ADDR_GLOBALS_INNER); }
static inline void ce_add_bomb(void) { ((ce_fn_add_bomb_t)CE_FN_ADD_BOMB)((void *)CE_ADDR_GLOBALS_INNER, 0); }
/* +1 残机碎片（0x457570，thiscall + 哑栈参 ret 4，与 add_bomb 同形）：残机已满时引擎清零不加、满每命碎片数自动进整命、自带刷 HUD。零售碎片道具 / CardNarumi 同一入口。AUDIT §X */
static inline void ce_add_life_fragment(void) { ((ce_fn_add_bomb_t)CE_FN_ADD_LIFE_FRAGMENT)((void *)CE_ADDR_GLOBALS_INNER, 0); }
/* HUD 残机行刷新（Gui 0x441f10：thiscall(gui; lives, fragments, max) ret 0xc，一手反汇编，AUDIT O28）。改 CURRENT_LIVES 后必须调，零售在死亡 / 商店复原处都这么做。*/
typedef void (__attribute__((thiscall)) *ce_fn_gui_lives_t)(void *gui, int lives, int fragments, int max);
static inline void ce_gui_update_lives(void)
{
    void *gui = CE_GUI();
    if (gui) ((ce_fn_gui_lives_t)CE_FN_GUI_UPDATE_LIVES)(gui, CE_CURRENT_LIVES(), CE_LIFE_FRAGMENTS(), CE_LIVES_MAX());
}
/* 全屏消弹：弹幕（→ 点道具）+ 激光，照 ECL 消弹指令的写法（AUDIT O28h）。*/
/* ★ BulletManager__cancel_all 是 thiscall(BULLET_MANAGER; 一个从不读取的栈参) ret 4——Ghidra 反编译显示 void(void)，尾部却是 `ret 4`
 * （0x429a0e）；零售 ECL 调用现场 0x434d48 `mov ecx,[0x4cf2bc]; push 0; call`。第一版按无参调，多弹 4 字节，on_activate 的 ret 跳飞（O23 同款教训，O28h′）。*/
typedef void (__attribute__((thiscall)) *ce_fn_bullet_cancel_all_t)(void *mgr, int unused);
typedef int  (__attribute__((stdcall)) *ce_fn_laser_cancel_all_t)(int mode, int unused);
static inline void ce_cancel_all_bullets(void)
{
    void *bm = CE_BULLET_MGR();
    if (bm) ((ce_fn_bullet_cancel_all_t)CE_FN_BULLET_CANCEL_ALL)(bm, 0);
    ((ce_fn_laser_cancel_all_t)CE_FN_LASER_CANCEL_ALL)(1, 0);
}
/* 半径消弹（照 Tenshi 要石 0x40eb0c..0x40eb6a）：stdcall 四个栈参 + 半径走 XMM2（AUDIT O29a）。
 * 调前清 BULLET_MANAGER 的计数器、调后读回 = 本次消掉的弹数；max_count 消满即停（拿剩余 HP 当它）。
 * 栈参从右往左压：tag(0)、max_count、mode、pos；被调方 ret 0x10 清栈。*/
static inline int ce_cancel_radius(const float *pos, float r, int max_count, int mode)
{
    uint8_t *bm = CE_BULLET_MGR();
    if (!bm || max_count <= 0) return 0;
    *(int32_t *)(bm + CE_BM_CANCEL_COUNTER) = 0;
    __asm__ volatile ("movss %[r], %%xmm2\n\t"
                      "pushl $0\n\t"
                      "pushl %[max]\n\t"
                      "pushl %[mode]\n\t"
                      "pushl %[pos]\n\t"
                      "call *%[fn]"
                      : : [r] "m"(r), [max] "r"(max_count), [mode] "r"(mode), [pos] "r"(pos),
                          [fn] "r"((uintptr_t)CE_FN_BULLET_CANCEL_RADIUS_AS_BOMB)
                      : "eax", "ecx", "edx", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7", "memory", "cc");
    return *(int32_t *)(bm + CE_BM_CANCEL_COUNTER);
}

/* 矩形伤害源（照 Remilia 0x40f478..0x40f4a6）：stdcall(center*, angle, life, dmg) + 宽 XMM2、高 XMM3，ret 0x10（AUDIT O29b）。
 * 敌人侧 0x45f0f0 每帧按 OBB 判重叠、每 +0x80 帧结算一次 dmg、本帧总量钳 player+0x47984（本 mod 不写它）。返回 1-based 槽号。*/
static inline int ce_damage_rect(const float *center, float angle, int life, int dmg, float w, float h)
{
    int idx;
    uint32_t ang_bits;
    __builtin_memcpy(&ang_bits, &angle, 4);            /* ★ 压栈的栈参一律走寄存器：内联汇编里 push 之后 esp 相对的 "m" 操作数会错位（第一版就错在 angle）*/
    __asm__ volatile ("movss %[w], %%xmm2\n\t"
                      "movss %[h], %%xmm3\n\t"
                      "pushl %[dmg]\n\t"
                      "pushl %[life]\n\t"
                      "pushl %[ang]\n\t"
                      "pushl %[c]\n\t"
                      "call *%[fn]"
                      : "=a"(idx)
                      : [w] "m"(w), [h] "m"(h), [dmg] "r"(dmg), [life] "r"(life), [ang] "r"(ang_bits), [c] "r"(center),
                        [fn] "r"((uintptr_t)CE_FN_PLAYER_DMGSRC_RECT)
                      : "ecx", "edx", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7", "memory", "cc");
    return idx;
}

/* 抬「每个敌人每帧」的总伤害上限 player+0x47984（照 CardRemilia__on_tick_2 0x40f48e：写 300 再放源）。自机弹与卡的伤害源共用它，
 * 自机对着 boss 开火时配额已被自机弹吃满，不抬的话卡的伤害只能补差额。GameThread 0x443d3b（优先级 0x10）每帧从 sht+0x28 复位，
 * AbilityManager tick（0x16）在其后、敌人结算（0x1b）在更后 ⇒ 在 on_tick_2 / on_active_tick 里写只对当帧生效，不用还原。只抬不降。AUDIT V20。*/
static inline void ce_raise_damage_cap(int cap)
{
    uint8_t *p = CE_PLAYER();
    if (p && *(int32_t *)(p + CE_PLAYER_DAMAGE_CAP) < cap) *(int32_t *)(p + CE_PLAYER_DAMAGE_CAP) = cap;
}

/* 装备卡子机（AUDIT §U）。两个函数的调用约定都从零售调用点原样抄来，**别照反编译的形参名改**：
 *
 *   Player__allocate_option —— thiscall + 五个栈参，其中第 1、3 个是 card 自己（`CardReimu1__on_power_level_change`
 *     `0x40aae0`：push script / push off / push ecx / push off / push ecx；调用后调用方不动 esp ⇒ callee ret 0x14）。
 *     返回 zPlayerOption*（12 个槽满了返回 NULL）。最后一参是 **ability.anm** 的脚本号（子机长什么样）。
 *   Player__tick_shooters_for_ability_card —— stdcall(option, short_timer, long_timer, sht_set_index) ret 0x10。
 *     ★ 那组 shooter 的 fire_rate 必须 >= 1：这条路径没有除零保护。
 */
static inline void *ce_allocate_option(void *card, int off, int anm_script)
{
    void *ret;
    __asm__ volatile ("pushl %[script]\n\t"
                      "pushl %[off]\n\t"
                      "pushl %[card]\n\t"
                      "pushl %[off]\n\t"
                      "pushl %[card]\n\t"
                      "movl %[card], %%ecx\n\t"
                      "call *%[fn]"
                      : "=a"(ret)
                      : [script] "ri"(anm_script), [off] "ri"(off), [card] "r"(card),
                        [fn] "r"((uintptr_t)CE_FN_PLAYER_ALLOCATE_OPTION)
                      : "ecx", "edx", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7", "memory", "cc");
    return ret;
}
static inline void ce_tick_shooters_for_card(void *option, int short_timer, int long_timer, int sht_set)
{
    __asm__ volatile ("pushl %[set]\n\t"
                      "pushl %[lt]\n\t"
                      "pushl %[st]\n\t"
                      "pushl %[opt]\n\t"
                      "call *%[fn]"
                      : : [set] "ri"(sht_set), [lt] "ri"(long_timer), [st] "ri"(short_timer), [opt] "r"(option),
                          [fn] "r"((uintptr_t)CE_FN_TICK_SHOOTERS_FOR_CARD)
                      : "eax", "ecx", "edx", "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7", "memory", "cc");
}

/* ANM VM：按 id 找（thiscall(ANM_MANAGER; id) ret 4）、写实体坐标 / 颜色、发中断（stdcall(id, n) ret 8）、删除（stdcall(id) ret 4）。AUDIT O29c。*/
typedef uint8_t *(__attribute__((thiscall)) *ce_fn_anm_get_vm_t)(void *mgr, uint32_t id);
typedef void (__attribute__((stdcall)) *ce_fn_anm_interrupt_t)(uint32_t id, int n);
typedef void (__attribute__((stdcall)) *ce_fn_anm_delete_t)(uint32_t id);
static inline uint8_t *ce_anm_get_vm(uint32_t id)
{
    void *mgr = CE_ANM_MANAGER();
    return (id && mgr) ? ((ce_fn_anm_get_vm_t)CE_FN_ANM_GET_VM_WITH_ID)(mgr, id) : 0;
}
/* UI 列表里起一个 VM（HUD 用；0x409670，世界列表版是 ce_anm_spawn）：pos = 实体坐标（NULL = 0,0,0），脚本里的 layer / resolutionMode 自己写。
 * 第四个栈参被调方不读但 ret 0x10 会清，必须压（AUDIT §X）。返回 anm id（0 = 失败）。*/
typedef int *(__attribute__((thiscall)) *ce_fn_anm_inst_ui_t)(void *anm, int *out_id, int script, const float *pos, int unused);
static inline uint32_t ce_anm_spawn_ui(void *anm, int script, const float *pos)
{
    int id = 0;
    if (!anm) return 0;
    ((ce_fn_anm_inst_ui_t)CE_FN_ANM_INSTANTIATE_UI_BACK)(anm, &id, script, pos, 0);
    return (uint32_t)id;
}
/* 给一个已存在的 VM 换 sprite（同一份 anm 的 sprite 表下标；0x477b00 thiscall(anm; vm, idx) ret 8）。返回 0 = VM 不在了 */
typedef int (__attribute__((thiscall)) *ce_fn_anm_set_sprite_t)(void *anm, void *vm, int sprite);
static inline int ce_anm_set_sprite(void *anm, uint32_t id, int sprite)
{
    uint8_t *vm = ce_anm_get_vm(id);
    if (!vm || !anm) return 0;
    ((ce_fn_anm_set_sprite_t)CE_FN_ANM_SET_SPRITE)(anm, vm, sprite);
    return 1;
}

static inline int ce_anm_set_pos(uint32_t id, float x, float y, float z)
{
    uint8_t *vm = ce_anm_get_vm(id);
    if (!vm) return 0;
    float *p = (float *)(vm + CE_ANM_VM_POS);
    p[0] = x; p[1] = y; p[2] = z;
    return 1;
}
static inline int ce_anm_set_color(uint32_t id, uint32_t rgba)
{
    uint8_t *vm = ce_anm_get_vm(id);
    if (!vm) return 0;
    *(uint32_t *)(vm + CE_ANM_VM_COLOR1) = rgba;
    return 1;
}
static inline int ce_anm_set_fvar(uint32_t id, unsigned i, float v)   /* 脚本变量 %F0–%F3：给脚本传参（破损核心电弧的长度）*/
{
    uint8_t *vm = ce_anm_get_vm(id);
    if (!vm || i > 3) return 0;
    ((float *)(vm + CE_ANM_VM_FVARS))[i] = v;
    return 1;
}
static inline int ce_anm_set_rotation(uint32_t id, float rx, float ry, float rz)   /* 脚本里不能同时跑 rotate 插值（同 scale 的道理）*/
{
    uint8_t *vm = ce_anm_get_vm(id);
    if (!vm) return 0;
    float *r = (float *)(vm + CE_ANM_VM_ROTATION);
    r[0] = rx; r[1] = ry; r[2] = rz;
    return 1;
}
static inline int ce_anm_set_scale(uint32_t id, float sx, float sy)      /* 脚本里没有在跑的 scaleTime 时写入才不被插值器覆盖 */
{
    uint8_t *vm = ce_anm_get_vm(id);
    if (!vm) return 0;
    float *s = (float *)(vm + CE_ANM_VM_SCALE);
    s[0] = sx; s[1] = sy;
    return 1;
}
static inline void ce_anm_interrupt(uint32_t id, int n) { if (id) ((ce_fn_anm_interrupt_t)CE_FN_ANM_INTERRUPT_TREE)(id, n); }
static inline void ce_anm_delete(uint32_t id)           { if (id) ((ce_fn_anm_delete_t)CE_FN_ANM_DELETE_BY_ID)(id); }
/* 火力（AUDIT §V）：照 CardTsukasa__c_press 0x410e60 的三步——门槛 → spend_power → repopulate。
 *   spend_power：thiscall(GlobalsInner; amount) ret 4，返回档数是否变了；CURRENT_POWER <= 一档时拒绝不扣，扣完钳到一档。
 *   repopulate：thiscall(PLAYER+0x620; 哑栈参) ret 4，按新档数重建子机并广播 on_power_level_change。
 *   Tsukasa 扣完**无条件**调 repopulate；我们照做（扣 2 档时档数必变）。调用方自己先保证 CURRENT_POWER >= 成本 + 一档，
 *   否则引擎的「永远保留 1.00」会让实际扣的比名义少。*/
typedef int  (__attribute__((thiscall)) *ce_fn_spend_power_t)(void *globals, int amount);
typedef void (__attribute__((thiscall)) *ce_fn_repopulate_t)(void *inner, int unused);
static inline int  ce_spend_power(int amount) { return ((ce_fn_spend_power_t)CE_FN_SPEND_POWER)((void *)CE_ADDR_GLOBALS_INNER, amount) & 0xff; }
static inline void ce_repopulate_options(void)
{
    uint8_t *p = CE_PLAYER();
    if (p) ((ce_fn_repopulate_t)CE_FN_PLAYER_REPOPULATE_OPTIONS)(p + CE_PLAYER_INNER, 0);
}
/* 游戏自己的 replay 安全 RNG（thiscall(zRng*) 无栈参）：gameplay 里的「随机」一律走它，不引入自己的随机源。*/
typedef uint32_t (__attribute__((thiscall)) *ce_fn_rng_t)(void *rng);
static inline uint32_t ce_rand(void) { return ((ce_fn_rng_t)CE_FN_RNG_RAND_DWORD)((void *)CE_ADDR_REPLAY_SAFE_RNG); }
/* 震屏（AUDIT V17）：照 ECL 517 setScreenShake 的 case 0x434d8d 调 ScreenEffect 工厂 0x476060。fastcall：ecx = type、edx = time，
 * 栈上 start / end / 0 / 0x5b，被调方 ret 0x10。强度是相机像素偏移，time 帧内从 start 线性到 end。UI RNG，不动 replay 流。*/
typedef void *(__attribute__((fastcall)) *ce_fn_screen_effect_t)(int type, int time, int start, int end, int p5, int draw_prio);
static inline void ce_screen_shake(int time, int start, int end)
{
    ((ce_fn_screen_effect_t)CE_FN_SCREEN_EFFECT_NEW)(CE_SCREEN_EFFECT_SHAKE, time, start, end, 0, CE_SCREEN_EFFECT_DRAW_PRIO);
}
static inline int  ce_owned(uint32_t id) { return id < 255 && CE_OWNED_ARRAY()[id] != 0; }
/* ctor 不只在获得时调：每关开始引擎会对卡组里每张卡再调一次 +0x00（实跑 2026-09-04：初始携带的卡 on_stage_start 后紧跟 ctor）。
 * 真正的获得路径（道具 mode 0 / 购买 mode 2）里 owned[自己] 要到 ctor 之后才置 1（0x412d42），关卡开始那次早就是 1 了——
 * 「获得即触发」的效果在 ctor 里先问这个。*/
static inline int  ce_fresh_acquire(ce_card_t *c) { return !ce_owned(ce_card_id(c)); }

/* sdk.c：主动卡机器（桩里调）*/
int  ce_sdk_c_press(void *self, const ce_hooks_t *h);
void ce_sdk_active_tick(void *self, const ce_hooks_t *h);
void ce_sdk_active_reset(void *self, const ce_hooks_t *h, int clear_recharge);

/* sdk.c */
void ce_sdk_trace(uint32_t id, unsigned slot, const char *name);   /* trace 开着才记；每张卡每槽记第一次 */
void ce_sdk_on_shop_open(void);                                   /* shop.c 在 MSG 开店那一帧调：沿链表分派 on_shop_open */
/* 局中从卡组删一张卡（AUDIT §W）：照 recount 0x4080e0 的删卡分支——摘链、按 flags 减类别计数、num_total−1、
 * owned[id] 清零（能再刷出）、虚表 +0x50 删对象、重画 HUD 三行。★ 不能删当前选中的主动卡（mgr+0x38 指向它，
 * 重选要重建 HUD 图标 VM，本函数拒绝并返回 0）。只能在没有人正在遍历链表时调（ctor / 事件断点里都可以，链表遍历桩里不行）。*/
int  ce_destroy_card(ce_card_t *victim);
/* 随机挑一张可删的卡（链表里除 exclude 与选中主动卡以外的任意一张，RNG = 游戏 replay 安全流）；没有返回 NULL */
ce_card_t *ce_pick_random_card(ce_card_t *exclude);
void ce_sdk_register_or_log(const ce_behavior_t *b);

#define CE_TC __attribute__((thiscall))
typedef int  (CE_TC *ce_base_fn0_t)(void *);
typedef void (CE_TC *ce_base_del_t)(void *, uint32_t);

/* ---- 桩生成 ---- */
#define CE_STUB0(ID, NAME, SLOT) \
    static int CE_TC ce_s_##ID##_##NAME(void *self) { \
        ce_sdk_trace(ID, SLOT, #NAME); \
        return ce_hooks_##ID.NAME ? ce_hooks_##ID.NAME((ce_card_t *)self) : 0; }
#define CE_STUB1(ID, NAME, SLOT, T1) \
    static int CE_TC ce_s_##ID##_##NAME(void *self, T1 a) { \
        ce_sdk_trace(ID, SLOT, #NAME); \
        return ce_hooks_##ID.NAME ? ce_hooks_##ID.NAME((ce_card_t *)self, a) : 0; }
#define CE_STUB2(ID, NAME, SLOT) \
    static int CE_TC ce_s_##ID##_##NAME(void *self, uint32_t a, uint32_t b) { \
        ce_sdk_trace(ID, SLOT, #NAME); \
        return ce_hooks_##ID.NAME ? ce_hooks_##ID.NAME((ce_card_t *)self, a, b) : 0; }
#define CE_STUBV(ID, NAME, SLOT) \
    static void CE_TC ce_s_##ID##_##NAME(void *self) { \
        ce_sdk_trace(ID, SLOT, #NAME); \
        if (ce_hooks_##ID.NAME) ce_hooks_##ID.NAME((ce_card_t *)self); }

#define CE_CARD(ID, ...) \
    static const ce_hooks_t ce_hooks_##ID = { __VA_ARGS__ }; \
    CE_STUB0(ID, ctor, 0x00) \
    CE_STUB0(ID, dtor, 0x04) \
    CE_STUB1(ID, on_death_after_deathbomb, 0x0c, uint32_t) \
    CE_STUB0(ID, on_death_before_deathbomb, 0x10) \
    CE_STUB0(ID, on_death_frame2, 0x14) \
    CE_STUB0(ID, on_power_level_change, 0x18) \
    CE_STUB2(ID, on_tick_shooters, 0x1c) \
    CE_STUB0(ID, on_load, 0x20) \
    CE_STUB0(ID, on_tick, 0x24) \
    CE_STUB1(ID, on_bullet_created, 0x28, void *) \
    static int CE_TC ce_s_##ID##_c_press(void *self) { \
        ce_sdk_trace(ID, 0x08, "c_press"); \
        return ce_sdk_c_press(self, &ce_hooks_##ID); } \
    static int CE_TC ce_s_##ID##_on_tick_2(void *self) { \
        ce_sdk_trace(ID, 0x2c, "on_tick_2"); \
        ce_sdk_active_tick(self, &ce_hooks_##ID); \
        return ce_hooks_##ID.on_tick_2 ? ce_hooks_##ID.on_tick_2((ce_card_t *)self) : 0; } \
    CE_STUB2(ID, on_enemy_drop, 0x30) \
    static void CE_TC ce_s_##ID##_on_stage_start(void *self) { \
        ce_sdk_trace(ID, 0x34, "on_stage_start"); \
        ce_sdk_active_reset(self, &ce_hooks_##ID, 0); \
        if (ce_hooks_##ID.on_stage_start) ce_hooks_##ID.on_stage_start((ce_card_t *)self); } \
    CE_STUB1(ID, on_hud_anm, 0x44, uint32_t) \
    CE_STUB0(ID, on_draw, 0x48) \
    static void CE_TC ce_s_##ID##_on_run_reset(void *self) { \
        ce_sdk_trace(ID, 0x4c, "on_run_reset"); \
        ce_sdk_active_reset(self, &ce_hooks_##ID, 1); \
        if (ce_hooks_##ID.on_run_reset) ce_hooks_##ID.on_run_reset((ce_card_t *)self); } \
    static void CE_TC ce_s_##ID##_opdelete(void *self, uint32_t flag) { \
        ce_sdk_trace(ID, 0x50, "operator_delete"); \
        ce_state_free(self); \
        ((ce_base_del_t)CE_BASE_SLOT_OPDELETE)(self, flag); } \
    static const void *const ce_vt_##ID[21] = { \
        (const void *)ce_s_##ID##_ctor,                      /* +0x00 */ \
        (const void *)ce_s_##ID##_dtor,                      /* +0x04 */ \
        (const void *)ce_s_##ID##_c_press,                   /* +0x08 SDK 主动卡机器（非主动卡直接返回 0）*/ \
        (const void *)ce_s_##ID##_on_death_after_deathbomb,  /* +0x0c */ \
        (const void *)ce_s_##ID##_on_death_before_deathbomb, /* +0x10 */ \
        (const void *)ce_s_##ID##_on_death_frame2,           /* +0x14 */ \
        (const void *)ce_s_##ID##_on_power_level_change,     /* +0x18 */ \
        (const void *)ce_s_##ID##_on_tick_shooters,          /* +0x1c */ \
        (const void *)ce_s_##ID##_on_load,                   /* +0x20 */ \
        (const void *)ce_s_##ID##_on_tick,                   /* +0x24 */ \
        (const void *)ce_s_##ID##_on_bullet_created,         /* +0x28 */ \
        (const void *)ce_s_##ID##_on_tick_2,                 /* +0x2c */ \
        (const void *)ce_s_##ID##_on_enemy_drop,             /* +0x30 */ \
        (const void *)ce_s_##ID##_on_stage_start,            /* +0x34 */ \
        (const void *)CE_BASE_SLOT_METHOD_38,                /* +0x38 充能 */ \
        (const void *)CE_BASE_SLOT_METHOD_3C,                /* +0x3c */ \
        (const void *)CE_BASE_SLOT_METHOD_40,                /* +0x40 */ \
        (const void *)ce_s_##ID##_on_hud_anm,                /* +0x44 */ \
        (const void *)ce_s_##ID##_on_draw,                   /* +0x48 */ \
        (const void *)ce_s_##ID##_on_run_reset,              /* +0x4c */ \
        (const void *)ce_s_##ID##_opdelete,                  /* +0x50 */ \
    }; \
    static const ce_behavior_t ce_beh_##ID = { ID, ce_vt_##ID, #__VA_ARGS__, &ce_hooks_##ID }; \
    __attribute__((constructor)) static void ce_reg_##ID(void) { ce_sdk_register_or_log(&ce_beh_##ID); }
