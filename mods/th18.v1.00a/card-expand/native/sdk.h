/* sdk.h —— 写一张行为卡时 include 这个。用法与机制见 ../SDK.md。
 *
 *   #include "sdk.h"
 *   static int on_death_frame2(ce_card_t *c) { ... return 0; }
 *   CE_CARD(62, .on_death_frame2 = on_death_frame2);
 *
 * CE_CARD 展开：这张卡的回调表 → 21 个 __thiscall 桩 → 一份 21 槽虚表 → 登记进 ce_behaviors。
 * 桩按虚表分派（每张卡一套桩），没写的回调桩返回 0（= 基类槽 `xor eax,eax; ret` 的效果）。
 * +0x08（C 键）由 SDK 的主动卡机器接管（非主动卡等价于基类的 ret 0）；+0x38 / +0x3c / +0x40（充能存取，HUD 与 replay 用）直接指基类实现。
 * +0x50 operator_delete：先调卡的 on_delete（此时私有状态还活着），再释放私有状态，最后进基类的 delete。
 */
#pragma once
#include <stdint.h>
#include "engine.h"
#include "anm_ids.h"      /* build_ability.py 生成：ability.anm 追加的 sprite / 脚本号 */
#include "sht_ids.h"      /* append_shooterset.py 生成：pl0X.sht 追加的 shooterset 索引（§12）*/
#include "sdk/public.h"
#include "sdk_core.h"
#include "cards/hud_dim.h"   /* hud_card_present()：原生「当前卡」UI 在不在（货架让位用）*/

#define ce_card_id(c)     (*(uint32_t *)((uint8_t *)(c) + CE_CARD_ID))
#define ce_card_entry(c)  (*(uint8_t **)((uint8_t *)(c) + CE_CARD_TABLE_ENTRY))
#define ce_card_flags(c)  (*(uint32_t *)((uint8_t *)(c) + CE_CARD_FLAGS))
/* on_enemy_drop_pre 的 counts 是 zEnemyDrop+0x04；主掉落类型在 +0x00（AUDIT S10）*/
#define ce_drop_main(counts) ((int32_t *)((uint8_t *)(counts) - CE_ENEMY_DROP_COUNTS + CE_ENEMY_DROP_MAIN))

/* ---- 引擎调用（薄包装，签名见 engine.h）---- */
typedef int      (__attribute__((thiscall)) *ce_fn_alloc_t)(void *mgr, uint32_t id, uint32_t mode);
typedef void    *( __attribute__((thiscall)) *ce_fn_alloc_enemy_t)(void *mgr, const char *ecl_name, void *spawn, int unused);
typedef void     (__attribute__((fastcall)) *ce_fn_mark_t)(uint32_t id, uint32_t notify);
typedef uint8_t *(__attribute__((fastcall)) *ce_fn_table_get_t)(uint32_t id);
typedef int      (__attribute__((fastcall)) *ce_fn_pick_t)(uint8_t **out, int tier_lo, int tier_hi, uint8_t **exclude, int n);

/* 给玩家一张卡。allocate 失败（-1）时不 mark_obtained，避免只解锁却没拿到。*/
/* ce_give_card 在 sdk/public.h 声明。 */
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

/* 日志 ce_log 由 sdk/public.h 声明；每次调用写入 th18_card_expand.log 一行。*/

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
 * 改 CURRENT_BOMBS / MAX_BOMBS 后必须调，原生在 consume_bomb 与每关开场都这么做。AUDIT §T。 */
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
 * 所以签名里必须带一个 dummy（O23 教训）。上限想一起加照原生：`if (CE_LIVES_MAX() < 7) CE_LIVES_MAX()++`。*/
typedef void (__attribute__((thiscall)) *ce_fn_add_life_t)(void *globals);
typedef void (__attribute__((thiscall)) *ce_fn_add_bomb_t)(void *globals, int unused);
static inline void ce_add_life(void) { ((ce_fn_add_life_t)CE_FN_ADD_LIFE)((void *)CE_ADDR_GLOBALS_INNER); }
static inline void ce_add_bomb(void) { ((ce_fn_add_bomb_t)CE_FN_ADD_BOMB)((void *)CE_ADDR_GLOBALS_INNER, 0); }
/* +1 残机碎片（0x457570，thiscall + 哑栈参 ret 4，与 add_bomb 同形）：残机已满时引擎清零不加、满每命碎片数自动进整命、自带刷 HUD。原生碎片道具 / CardNarumi 同一入口。AUDIT §X */
static inline void ce_add_life_fragment(void) { ((ce_fn_add_bomb_t)CE_FN_ADD_LIFE_FRAGMENT)((void *)CE_ADDR_GLOBALS_INNER, 0); }
/* ★ +1 **炸弹碎片**（2026-09-13 新增；113 的符卡碎奖励用它 —— 用户实跑报「绿灵没有加残机碎片」）。
 * 原生没有独立函数：这段逻辑**内联在道具拾取里**（`ItemManager__on_tick__body`，
 * `th18:0x446460`–`0x4464d0` 一手逐条）：
 *
 *     炸弹已满（CURRENT_BOMBS >= MAX_BOMBS）⇒ 碎片清 0、不加；
 *     否则 ++碎片；碎片 >= **3** ⇒ 碎片清 0 + 调原生 add_bomb（`0x457690`，自带钳上限 / 清碎片 / 音效 0x2e / 刷 HUD）；
 *     最后再刷一次 HUD（`0x4420e0`）。
 *
 * ⇒ 复刻这段（**不是**自己发明一个比例）：3 个碎片 = 1 个炸弹（TH18 的规则；TH13 那边是另一套表）。
 *   ⚠ 这是**共享件新增**（对既有卡零影响：纯新增 inline，没有卡调用过它），登记见 `AUDIT.md` §BE。*/
static inline void ce_add_bomb_fragment(void)
{
    if (CE_CURRENT_BOMBS() >= CE_MAX_BOMBS()) { CE_BOMB_FRAGMENTS() = 0; return; }
    if (++CE_BOMB_FRAGMENTS() >= 3) { CE_BOMB_FRAGMENTS() = 0; ce_add_bomb(); }
    ce_gui_update_bombs();
}
/* HUD 残机行刷新（Gui 0x441f10：thiscall(gui; lives, fragments, max) ret 0xc，一手反汇编，AUDIT O28）。改 CURRENT_LIVES 后必须调，原生在死亡 / 商店复原处都这么做。*/
typedef void (__attribute__((thiscall)) *ce_fn_gui_lives_t)(void *gui, int lives, int fragments, int max);
static inline void ce_gui_update_lives(void)
{
    void *gui = CE_GUI();
    if (gui) ((ce_fn_gui_lives_t)CE_FN_GUI_UPDATE_LIVES)(gui, CE_CURRENT_LIVES(), CE_LIFE_FRAGMENTS(), CE_LIVES_MAX());
}
/* 全屏消弹：弹幕 + 激光，照 ECL 消弹指令的写法（AUDIT O28h）。
 * ★ **2026-09-30 订正**（依据 = `engine/bullet/th18/01-cancel-to-item.md` §1.4/§5（一手））：
 *   旧注写「弹幕（→ 点道具）」是**反的** —— 子弹侧 `BulletManager__cancel_all` 内部 `xor edx,edx`
 *   （`th18:0x429932`）⇒ **mode 0 ⇒ 一颗点道具都不撒**；真正撒的是**激光侧**
 *   （下面 `ce_fn_laser_cancel_all_t` 传的 mode 1 ⇒ 每条激光一颗 type 9）。
 *   ⇒ 正确口径 =「**子弹不撒、激光撒**」（这也是原生 `etClearAll`(613) 的那处不对称）。*/
/* ★ BulletManager__cancel_all 是 thiscall(BULLET_MANAGER; 一个从不读取的栈参) ret 4——Ghidra 反编译显示 void(void)，尾部却是 `ret 4`
 * （0x429a0e）；原生 ECL 调用现场 0x434d48 `mov ecx,[0x4cf2bc]; push 0; call`。第一版按无参调，多弹 4 字节，on_activate 的 ret 跳飞（O23 同款教训，O28h′）。*/
typedef void (__attribute__((thiscall)) *ce_fn_bullet_cancel_all_t)(void *mgr, int unused);
typedef int  (__attribute__((stdcall)) *ce_fn_laser_cancel_all_t)(int mode, int unused);
static inline void ce_cancel_all_bullets(void)
{
    void *bm = CE_BULLET_MGR();
    if (bm) ((ce_fn_bullet_cancel_all_t)CE_FN_BULLET_CANCEL_ALL)(bm, 0);
    ((ce_fn_laser_cancel_all_t)CE_FN_LASER_CANCEL_ALL)(1, 0);
    /* ★ 登记进「本帧消弹记录」（这两条是 void，颗数数不出来 ⇒ 记 -1）。见 `sdk_core.h` 的
     *   `ce_cancel_frame_*` 块注释：记录由 **helper 自动**做，卡不必自己记得 —— 这样
     *   「没记到」永远不会被当成「没消」（原生卡清了池时我们数出来的是 0，那才是真话）。*/
    ce_cancel_frame_note(-1);
}
/* 半径消弹（照 Tenshi 要石 0x40eb0c..0x40eb6a）：stdcall 四个栈参 + 半径走 XMM2（AUDIT O29a）。
 * 调前清 BULLET_MANAGER 的计数器、调后读回 = 本次消掉的弹数；max_count 消满即停（拿剩余 HP 当它）。
 * 栈参从右往左压：tag(0)、max_count、mode、pos；被调方 ret 0x10 清栈。
 * ★ 返回值同时**自动**登记进「本帧消弹记录」（`ce_cancel_frame_count()`）⇒ 卡侧一律以**返回值**算账，
 *   **不要**用「有没有认领到闩锁」当「本帧消过弹」的判据。*/
static inline int ce_cancel_radius(const float *pos, float r, int max_count, int mode)
{
    uint8_t *bm = CE_BULLET_MGR();
    int n;
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
    n = *(int32_t *)(bm + CE_BM_CANCEL_COUNTER);
    ce_cancel_frame_note(n);
    return n;
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

/* 装备卡子机（AUDIT §U）。两个函数的调用约定都从原生调用点原样抄来，**别照反编译的形参名改**：
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
/* 写 VM 的 color_2（`+0x528`，TH18 zAnmVm 字段图；`color2()` 写的就是它）。
 * 渐变图元（`drawRectGrad`）的第二个色只能靠它 —— `ce_anm_set_color` 只写 color_1。*/
static inline int ce_anm_set_color2(uint32_t id, uint32_t rgba)
{
    uint8_t *vm = ce_anm_get_vm(id);
    if (!vm) return 0;
    *(uint32_t *)(vm + CE_ANM_VM_COLOR2) = rgba;
    return 1;
}

/* ★ 2026-09-15c：**把 color_1 原样镜像进 color_2** —— 「两路同色」的安全写法（110 的 9 支 HUD VM、
 * 111 的 3 支原版 VM 用它）。一手（`local/th18.v1.00a/th18.exe`，逐条反汇编复核）：
 *
 *   · 绘制用哪一路由脚本的 `colorMode`（opcode **423** → handler `0x47b69b`，写 `flags_lo`（`+0x534`）
 *     bits17–19）决定，**不是**由「哪一路非零」决定：
 *     `0x47e202: test eax,eax / jne +; mov 0x524(%edi),%eax … mov 0x528(%edi),%eax` ⇒
 *     mode 0 → `+0x524` color_1、mode≠0 → `+0x528` color_2，取中的那一路随后写进 `+0x52c`
 *     （`0x47e2f5 mov %eax,0x52c(%edi)`）供渲染器用。
 *   · 脚本的 `color()`/`alpha()`/`alphaTime()` **全部落在 color_1 上**（指令分派表 `0x47ba4c`/`0x47bb98`
 *     直读：403 `alpha` → handler `0x479efd`，其 `0x479f15` 是 `mov %al,0x527(%ebx)` = color_1.A；
 *     404 `color` → `0x479f44`，写 `0x526/0x525/0x524` = color_1 的 R/G/B；每帧提交插值的例程
 *     `0x47c7f2` 同样写 `+0x527`）。写 **color_2** 的是 `alpha2`（405 → `0x479fa1`，写 `+0x52b`）与
 *     `color2`（406 → `0x479fe8`，写 `+0x528/0x529/0x52a`）。
 *     ⚠️ `engine/anm/th18/07-blendmode-rgb-only-alpha-is-sa.md` §2 与 `AUDIT` §CT-7 把 `+0x52b`
 *     那个 handler 记成了 **alpha**，实为 **alpha2**（本轮订正，见 `NEXT.md` §1y）。
 *   · 早退：`0x4812e8`/`0x48129b`：`color_1.A == 0 && color_2.A == 0` ⇒ 整台 VM 不画。
 *
 * ⇒ **只写 color_1 的脚本（本仓绝大多数原版转写脚本都不写 `color2()`）在 colorMode≠0 时会被画成全透明**，
 *   而 color_2 恒 0 时**看上去一切正常**（日志里 `c1=ffffffff c2=00000000` 就是这么来的）。
 * 镜像（而不是把两路都硬写成某个常数）的理由：脚本的**淡入 / 档位闪色 / 基色**全在 color_1 上逐帧变化
 * （本仓 210/211 = `alpha(0)`+`alphaTime(20,0,255)`；213 = 中断切的 `color(255,255,0)`↔`(255,0,0)`），
 * 硬写会把它们覆盖掉；镜像让「两路同值」**逐帧**成立 ⇒ 与 `colorMode` 无关，又不改任何既有可见行为
 * （color_1 一个字节都不动）。❓ 自定（`TUNING.md` §2.6c）。*/
static inline int ce_anm_mirror_color(uint32_t id)
{
    uint8_t *vm = ce_anm_get_vm(id);
    if (!vm) return 0;
    *(uint32_t *)(vm + CE_ANM_VM_COLOR2) = *(const uint32_t *)(vm + CE_ANM_VM_COLOR1);
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

/* ---- ★★ 进度条/充能条的「活动部分」：**截断**而不是**伸缩**（共享件，2026-09-15 从 113 提炼）----
 * 只写 `scale.x` 是把**整张美术横向压扁/拉长**（用户实跑一眼就看出来：「在奇怪地伸缩」）；
 * 原版的做法是**裁切贴图**，两个字段一起写（一手：原生 HUD 充能条 `th18:0x408a43`
 * `or dword ptr [esi+0x534], 0x18`；TH13 侧写法 `scale.x = uv_scale.x = 比例`，
 * 见 `engine/spirit/th13/01-spirit-and-trance.md` §8、`engine/anm/th16/10-sprite-geometry.md` §4）：
 *   · `zAnmVmPrefix.uv_scale`（`+0x6c`，zFloat2）的 **u = f** ⇒ 只采样 sprite 自己 UV 矩形左边 `f` 那一段
 *     （**收缩恒以左上角为锚**，与 `anchor` 无关；它是 sprite 矩形内的比例，与画布大小无关）；
 *   · `flags_lo`（`+0x534`）`|= 0x18` —— 让引擎按 uv_scale 走这条采样路径。
 * `f` = 进度 0..1（本函数自己钳）。`art_scale_x/y` = 该美术相对 TH18 UI 尺度的整体缩放
 * （113 的美术是 TH13 原尺寸 ⇒ 2.0；111 的信号条是照 TH18 尺度做的 ⇒ 1.0）。
 * ⚠ quad 宽度必须写成 `f × art_scale_x`（**不是** `f`），否则裁切对了、比例还是错的（113 第五轮踩过）。*/
#define CE_ANM_VM_UV_SCALE 0x6c          /* zAnmVmPrefix.uv_scale（zFloat2 u/v；ExpHP th18 type-structs-own）*/
#define CE_ANM_VM_FLAGS_LO 0x534         /* VM 的 flags（低半）；`|= 0x18` 与原生充能条同一套 */
static inline int ce_anm_set_clip_x(uint32_t id, float f, float art_scale_x, float art_scale_y)
{
    uint8_t *vm = ce_anm_get_vm(id);
    if (!vm) return 0;
    if (f < 0.0f) f = 0.0f;
    if (f > 1.0f) f = 1.0f;
    ((float *)(vm + CE_ANM_VM_SCALE))[0] = f * art_scale_x;
    ((float *)(vm + CE_ANM_VM_SCALE))[1] = art_scale_y;
    *(float *)(vm + CE_ANM_VM_UV_SCALE) = f;
    *(uint32_t *)(vm + CE_ANM_VM_FLAGS_LO) |= 0x18u;
    return 1;
}
/* ★★ 2026-09-16m（共享件新增）：**调引擎自己的擦弹函数**（`Player__do_graze`，见 `engine.h` 的
 * `CE_FN_DO_GRAZE` 那一整段：它做什么、不做什么、为什么必须门 NULL）。
 * 语义：**一次调用 = 一次真擦弹** —— 计数 +1、`effect.anm` 火花一颗、`se_graze.wav` 一声。
 * 返回 1 = 真的调了；0 = 门把它挡下了（玩家/EffectManager/effect.anm/AnmManager 有一个为空）。
 * ⚠ 调用约定必须是 **stdcall**（`0x45e290` 是 `ret 4`）—— 千万不要自己再 `add esp`。
 * ⚠ `src_pos` 直接传 `bullet + CE_BULLET_POS`（`+0x638`）即可：callee 只读前两个 float，
 *   而且声像会自然等于子弹 x —— 与原生调用点逐位一致。*/
typedef void (__attribute__((stdcall)) *ce_fn_do_graze_t)(const float *src_pos);
static inline int ce_do_graze(const float *src_pos)
{
    uint8_t *em = *(uint8_t **)CE_ADDR_EFFECT_MGR_PTR;
    uint8_t *an = em ? *(uint8_t **)(em + CE_OFF_EFFECT_MGR_ANM) : 0;

    if (!src_pos || !CE_PLAYER() || !em || !an || !*(void **)CE_ADDR_ANM_MANAGER_PTR) return 0;
    ((ce_fn_do_graze_t)CE_FN_DO_GRAZE)(src_pos);
    return 1;
}

/* ★ 站点自检门（实现见 sdk.c）：不变量不成立时**只记一行**（每个调用点一次）。传字符串常量。*/
void ce_assume_fail(const char *what);

static inline void ce_anm_interrupt(uint32_t id, int n) { if (id) ((ce_fn_anm_interrupt_t)CE_FN_ANM_INTERRUPT_TREE)(id, n); }
/* 同 ce_anm_get_vm 的门槛（M6）：调用方管理器还没起好（比如对象删除路径抢在 ANM 管理器初始化前跑）就别摸它。 */
static inline void ce_anm_delete(uint32_t id)           { if (id && CE_ANM_MANAGER()) ((ce_fn_anm_delete_t)CE_FN_ANM_DELETE_BY_ID)(id); }
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
/* 道具 spawn（collect-2x 辉针城用；AUDIT §Z）—— 0x446f40 ★栈参 8 个、callee ret 0x20（与初版「RET 0x1c/6 参」
 * 记法不同：0x430479 `sub esp,8` 起的尾两槽被调方不读但 ret 会清，必须压）。ECX = CE_ITEM_MANAGER()；
 * 栈参自低向高 (type, pos float3*, junk(原生从不写，传 0), angle, speed, timer(原生撒落传 0 → 出生 state 1),
 * u7, u8)。type ≥ 0x14 → 0；1..0x13 → 槽指针。原生 main-drop 实参 = (type, 敌pos, 垃圾, −π/2, 2.2f, 0)。*/
typedef uint8_t *(__attribute__((thiscall)) *ce_fn_spawn_item_t)(void *mgr, uint32_t type, const float *pos, uint32_t junk,
                                                                float angle, float speed, uint32_t timer, uint32_t u7, uint32_t u8);
static inline uint8_t *ce_spawn_item(uint32_t type, const float *pos, float angle, float speed)
{
    void *mgr = CE_ITEM_MANAGER();
    if (!mgr) return 0;
    return ((ce_fn_spawn_item_t)CE_FN_SPAWN_ITEM)(mgr, type, pos, 0, angle, speed, 0, 0, 0);
}
/* ASCII 的「页」= `mgr+0x2c`（一手：`draw_string` `0x41a2b0` 用
 * `preloaded.anm_10_anms_for_ecl_anim[*(int*)(mgr+0x2c) - 10]` 选字形表 ⇒ 页 = 10 + ascii.anm 的 entry 号：
 * entry0 = ascii.png（小字，320 sprite = 3 套 96 字形）、entry1 = ascii_big（98 字形）、
 * entry3 = ascii_big2（98 字形）、entry2/4 是 big 的「描边」版）。
 * 想用大字体就得切页 + 把字体 id 设 0（大字体表里只有一套字形）。返回旧页（-1 = mgr 空）。*/
static inline int ce_hud_font_page(int page)
{
    uint8_t *mgr = *(uint8_t **)CE_ADDR_ASCII_MANAGER_PTR;
    int old;
    if (!mgr) return -1;
    old = *(int *)(mgr + CE_ASCII_PAGE);
    *(int *)(mgr + CE_ASCII_PAGE) = page;
    return old;
}

/* 擦弹事件（引擎自己的子弹擦弹路径 `0x42497f` 断点记下的）：取「自上次调用以来」的擦弹次数，
 * >0 时把**最后一次被擦到的那颗弹**的坐标写进 x/y。季节点用它撒在弹的位置上。
 * ⚠ **`take` 会清空**累计（「自上次以来」语义）⇒ 同一帧里两张卡都调，后调的那张拿到 0。*/
int ce_graze_take(float *x, float *y);
/* ★ 2026-09-27（第三十九批，卡 108「擦弹刻符撒在弹上」用）：**只读不消费**的版本 ——
 * 返回当前累计的擦弹次数、把**最后一次被擦到的那颗弹**的坐标写进 x/y，但**不清空**。
 * 为什么需要它：108 与 116（季节）会同时在场，两条都读同一个擦弹事件 ⇒ 用 `take` 会互相偷
 * （先调的那张把待处理数据清掉）。108 用本函数（它自己按 `0x4ccd0c` 的累计擦弹差分算颗数，
 * 只需要**位置**）；116 继续用 `take`（它要的是「自上次以来的次数」）。
 * 机制与站点见 `sdk.c` 的 `BP_ce_graze`。*/
int ce_graze_peek(float *x, float *y);

/* 极坐标 → 速度（`0x447430`，thiscall + ret 8：ECX = out，栈 = angle, speed）。
 * 写 out[0] = cos(angle)·speed、out[1] = sin(angle)·speed。角度约定与引擎一致（0 = +x，−π/2 = 上）。*/
typedef void (__attribute__((thiscall)) *ce_fn_polar_to_vel_t)(float *out, float angle, float speed);
static inline void ce_polar_to_vel(float *out, float angle, float speed)
{
    ((ce_fn_polar_to_vel_t)CE_FN_POLAR_TO_VEL)(out, angle, speed);
}
/* SCORE += delta：int64 应用（防中间溢出），钳 [0, 999999999]（SM §1 同口径）。collect-2x 档位补差用——
 * delta 可为负（×0.5/×0.8 档 = 把已入账的 100% 分值扣回倍率，1:1 原义）。AUDIT §Z。*/
static inline void ce_add_score_delta(int64_t delta)
{
    int64_t v = (int64_t)CE_SCORE() + delta;
    if (v < 0) v = 0;
    if (v > 999999999) v = 999999999;
    CE_SCORE() = (int32_t)v;
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

/* ---- 反复出现的卡侧原语（SDK.md §5「先在卡里写，出现第二次就提炼进 sdk.h」）--------------
 * 2026-09-08 提炼：装备卡子机三件套（破损核心 71 / 豌豆射手 77 的实现逐字相同）、HUD 图标数字
 * （真银圣剑 75 / 衔尾蛇 78）、按 enemy_id 找敌人（古锭刀 76 / 神之宣告 66）、主动卡拒绝发动
 * （青眼 67 / 神之宣告 66 / 炎魔 72）、自机 x（三处）。全部是原样搬家，引擎侧调用一个字节没变。*/

/* 自机坐标。`CE_PLAYER_X` 是 float3 的基址 —— 这个假设本来就被青眼 / 豌豆射手 / 炎魔三张卡在用
 * （各自 `(const float *)(p + CE_PLAYER_X)` 再取 `[1]` `[2]`），只是没写在一处。收进这里之后
 * 它变成一条可审计的断言（engine.h 的 `CE_PLAYER_X` 注释）。自机指针为空时返回 NULL。*/
static inline const float *ce_player_pos(void)
{
    uint8_t *p = CE_PLAYER();
    return p ? (const float *)(p + CE_PLAYER_X) : 0;
}
/* 自机世界 x（放音效的声像位置）；自机指针为空时 0。*/
static inline float ce_player_x(void)
{
    const float *pp = ce_player_pos();
    return pp ? pp[0] : 0.0f;
}

/* ==================== ★ PoC 高度层（收点线 / 「回收线」；2026-09-15 抽）====================
 * 为什么有这一层（用户 2026-09-15 口径，逐字）：「有一张**原生卡直接是降低 PoC 高度**，并且 **th14 卡也得追这个高度，
 * 包括信号系统**……**PoC 高度有一个指示线，在开局显示，如果开局 PoC 高度变了，显示也得变**」。
 * 一手（全 exe 穷举，见 `engine/player/th18/05-poc-height-and-indicator.md`）：
 *   · 字段 `player+0x47998` **只有两个写点**（`Player__reset` 128.0 `0x45ae8f` / 原生卡 22 Kanako 的
 *     `on_load` 224.0 `0x40c275`），四个引擎读点全是「自机在线上还是线下」；
 *   · 开局那条指示线（`front.anm` script `0x39`）是**引擎 GUI 自己**建的 VM，位置**现读字段**
 *     （`0x43afe1`：`y640 = poc×2 − 80`），可见性 = `Gui+0x194` bit `0x2000`（唯一写者 = 那张原生卡）。
 * ⇒ 所以「改了高度，显示跟着变」**不用我们画线、也不用改资产**：把字段写对 + 把 `0x2000` 置上就行。
 *
 * ## 卡侧三条纪律
 *   1. **只声明、不直写字段**：`ce_poc_declare(c, 高度)`（声明 = **绝对**高度，单位 px、640 空间）。
 *      声明是**粘性**的（照原生卡 22：开局写一次管到改口）⇒ 每帧都声明、或只声明一次都可以，幂等。
 *      「相对下移」的卡（把线在**当前**高度上再往下挪）自己算好绝对值再声明：先读 `ce_poc_height_semantic()`
 *      （= 「不含**遮**」的生效值，含原生卡 22 的 224）再声明 `读到的值 + 下移量`。
 *   2. **读生效值一律用访问器**（不要 `*(float*)(pl+CE_PLAYER_POC_HEIGHT)`）：
 *      · 我们自己的语义（信号档、计分、卡自己的判据）读 **`ce_poc_height_semantic()`**
 *        —— 它**不含**「遮」（见 `sdk_core.h` 的「遮」声明一节），语义 = **没有 106 时**；
 *      · 非要引擎字段当下的真值（诊断 / 看落笔结果）才读 `ce_poc_height()`。
 *   3. **删卡不用自己写回**：`ce_layer_drop_card()` 会调 `ce_poc_release()` —— 撤声明 + 若没人再声明就
 *      把「我们动手之前」的字段原值写回去（见 `sdk_core.h` 的基准记账）。删卡发生在**商店**里
 *      （AbilityManager / ItemManager 都冻住，帧钩子不跑）⇒ 这一步必须在 `on_delete` 路径上立刻做。
 *
 * ⚠ **没有声明时本层一个字都不写**（否则会把原生卡 22 写进去的 224 顶回 128）。*/
/* 声明 / 更新本卡想要的收点线高度（单位 px、640 空间；`h <= 0` / NaN / Inf = 退出声明）。返回 1 = 记下。*/
static inline int ce_poc_declare_px(ce_card_t *c, float h)
{
    return ce_poc_declare(ce_card_id(c), ce_f32_bits(h));
}
/* 同上，给**拿不到 `ce_card_t*`** 的地方用（例：111 的支持特技函数只拿到私有状态，只有卡 id）。*/
static inline int ce_poc_declare_px_id(uint32_t card_id, float h)
{
    return ce_poc_declare(card_id, ce_f32_bits(h));
}
/* ★★ 2026-09-20（106 的「不满 P 过线不收点」非接管实现，见 `sdk_core.h` 的「遮」声明一节）：
 * 「**遮**」声明 = **覆盖位**。在场时落笔值 = `min(语义高度, 自机 y)`（忽略别家声明的**高度**）：
 *   · 过线收点的门是**严格** `ja`（`线 > y`）⇒ 落笔值恒 ≤ y ⇒ **永不进真空态**（= 不满 P 过线不收点）；
 *   · 线上满额的门是**非严格** `jae`（`线 ≥ y`）⇒ 自机在语义线上方时落笔值 == y ⇒ **照给满额**。
 * 粘性；删卡 / 换关由 `ce_poc_release()` 撤（并立刻写回基准）。*/
static inline int ce_poc_declare_mask(ce_card_t *c, int on)        { return ce_poc_mask_declare(ce_card_id(c), on); }
static inline int ce_poc_declare_mask_id(uint32_t card_id, int on) { return ce_poc_mask_declare(card_id, on); }
/* ★★ 线显示（§7-A4 定案 (ii)）：声明「**不画**开局那条收点指示线」⇒ 落笔时清 `Gui+0x194` 的 bit `0x2000`，
 * 撤完按基准写回（`ce_poc_release()` 里一起撤，删卡那一刻立刻写）。原生那一位**只置不清**
 * （唯一写者 = 原生卡 22 `0x40c288`）⇒ 主动清是本层新增的写点（登记 `AUDIT.md` §BE）。*/
static inline void ce_poc_line_hide(ce_card_t *c, int on)          { ce_poc_line_hide_declare(ce_card_id(c), on); }
/* ★ 只读访问器：**当前生效的收点线高度**（= 引擎字段当下值，含我们落笔的「遮」）。自机指针为空时返回 0。
 *  刻意 `static inline`（float 返回值走 x87 st(0)，独立函数会让 `make dllx87` 变红 —— 同 `faith.h` 那条注）。*/
static inline float ce_poc_height(void)
{
    uint8_t *pl = CE_PLAYER();
    return pl ? *(float *)(pl + CE_PLAYER_POC_HEIGHT) : 0.0f;
}
/* ★★ 只读访问器：**语义高度** —— 「不含『遮』」的生效值（别家声明取最大 → 基准 → 字段实读值）。
 *  **我们自己的读数处一律用它**（106 的分档判据 / 111 的信号档与神奈子线 / SDK 计分层）：
 *  这样它们的语义与「**没有 106**」时逐位相同（用户口径：「106 只做『过线不收点』，其它语义零变化」）。
 *  ⚠ 它**不**等于引擎字段：106 不满 P 时字段被压在 `min(语义高度, 自机 y)` 上（为了让引擎不吸）。*/
static inline float ce_poc_height_semantic(void)
{
    uint8_t *pl = CE_PLAYER();
    uint32_t raw = pl ? *(volatile uint32_t *)(pl + CE_PLAYER_POC_HEIGHT) : 0u;

    return ce_f32_from_bits(ce_poc_semantic_bits(raw));
}
/* 帧界落笔（`sdk.c` 的 `BP_ce_item_frame` 每帧调一次；卡在自己的开局路径里也可以调一次 —— 幂等）。
 * 有声明 ⇒ 写字段（有「遮」⇒ `min(语义高度, 自机 y)`）+ 线显示位；声明全撤 ⇒ 写回基准；
 * 都没事 ⇒ 一个字都不写。*/
void ce_poc_apply(void);
/* 删卡 / 退出声明（`ce_layer_drop_card()` 里自动调；卡不用自己调）。*/
void ce_poc_release(uint32_t id);

/* 敌人坐标（同为 float3 基址；破损核心与炎魔在用）。*/
static inline const float *ce_enemy_pos(const uint8_t *e) { return (const float *)(e + CE_ENEMY_POS_X); }
/* 可锁定 = 非空且不带 NO_LOCK（破损核心瞄准与炎魔投火球是同一套过滤）。*/
static inline int ce_enemy_lockable(const uint8_t *e)
{
    return e && !(*(const uint32_t *)(e + CE_ENEMY_FLAGS) & CE_ENEMY_FLAG_NO_LOCK);
}
/* 沿 EnemyManager 的敌人链表遍历（结点 {entry, next, prev}）。E 每轮拿到一个 zEnemy*（**可能为空**，
 * 用 `ce_enemy_lockable(e)` 一并过滤掉）。guard 与 `ce_enemy_by_id` 同一条不变式：坏链表不会转死。
 *     uint8_t *e;
 *     CE_ENEMY_FOREACH(em, e) { if (!ce_enemy_lockable(e)) continue; … }
 * 只在本帧读，别缓存 enemy 指针。*/
#define CE_ENEMY_FOREACH(EM, E)                                                                     \
    for (struct { uint8_t **n; unsigned g; } ce_it_ =                                               \
             { (EM) ? *(uint8_t ***)((uint8_t *)(EM) + CE_EM_ENEMY_LIST_HEAD) : (uint8_t **)0, 0 }; \
         ce_it_.n && ce_it_.g < CE_ENEMY_LIST_GUARD ? (((E) = ce_it_.n[0]), 1) : 0;                 \
         ce_it_.n = (uint8_t **)ce_it_.n[1], ++ce_it_.g)

/* 符卡状态（古锭刀看「在不在打符卡」、神之宣告还要改奖励位）。管理器为空时 flags 读作 0 = 不在符卡里。*/
static inline uint32_t ce_spell_flags(void)
{
    uint8_t *sp = CE_SPELLCARD();
    return sp ? *(uint32_t *)(sp + CE_SPELL_FLAGS) : 0;
}
static inline int ce_spell_active(void) { return (ce_spell_flags() & CE_SPELL_FLAG_ACTIVE) != 0; }

/* 当前符卡的 **id**（`zSpellcard+0x74`；没有符卡 / 没在符卡中 = 0）。
 * ★ 110 的**小灵击**判据要用它：一手 TH10 `Bomb::start` 那条分支要求的是**特定几张符卡**
 *   （`[spellcard+0x3788] ∈ 0x5d..0x60 ∪ {0x6d}`），不是「任何符卡」——owner 2026-09-24 口径
 *   「风神录 6 面终符那个是剧情/平衡特例，**不能把这个当常态**」。见 `mof_core_bomb_suppressed()`。*/
static inline int32_t ce_spell_id(void)
{
    uint8_t *sp = CE_SPELLCARD();
    return (sp && (ce_spell_flags() & CE_SPELL_FLAG_ACTIVE)) ? *(int32_t *)(sp + CE_SPELL_ID) : 0;
}

/* 按 enemy_id 在 EnemyManager 的敌人链表里找（照 `get_boss_enemy_full` `0x4237f0`：结点 {entry, next, prev}）。
 * 找不到 / em 为空返回 0；guard 防坏链表。要遍历全部敌人用下面的 `CE_ENEMY_FOREACH`。boss 用 `em + CE_EM_BOSS_IDS + i*4` 取 id 再问它。*/
static inline uint8_t *ce_enemy_by_id(uint8_t *em, int32_t id)
{
    unsigned guard = 0;
    if (!em) return 0;
    for (uint8_t *node = *(uint8_t **)(em + CE_EM_ENEMY_LIST_HEAD); node && guard < CE_ENEMY_LIST_GUARD;
         node = *(uint8_t **)(node + 4), ++guard) {
        uint8_t *e = *(uint8_t **)node;
        if (e && *(int32_t *)(e + CE_ENEMY_ID) == id) return e;
    }
    return 0;
}

/* 主动卡拒绝发动：`0x10` 无效音 + 一行 `card: refused (why)` + `CE_ACTIVATE_REFUSED`
 * （c_press 桩看到它就退回刚装填的充能、不进释放态）。在 `on_activate` 里 `return ce_refuse(…)`。*/
int ce_refuse(const char *card, const char *why);

/* ---- 装备卡子机（AUDIT §U）----
 * 槽是引擎的 12 个公共池，会被回收、被别的卡拿走 —— 每次用前认领一次
 * （`option+0x00 != 0 && option+0xd0 == card+0x08`，`Player__allocate_option` `0x40a790` 写的）。
 * 指针存卡的私有状态：新卡对象只有 0x54 字节，原生存的 `card+0x54` 已在对象外。
 *   `on_power_level_change` → `ce_option_alloc`（池刚被重建，无条件再要一个，与原生装备卡一致）
 *   `on_tick_2`            → `ce_option_get`（认领；没有就按 retry 间隔补申请一次）
 *   `on_load` / `on_run_reset` → `ce_option_drop`（删子机的 ANM VM；槽本身由
 *                                `Player__repopulate_options_and_notify_cards` 统一回收）*/
typedef struct {
    uint8_t *ptr;                    /* zPlayerOption*；0 = 还没有 */
    uint32_t retry;                  /* 补申请的倒计时 */
} ce_option_t;

uint8_t *ce_option_claim(ce_card_t *c, ce_option_t *o);
uint8_t *ce_option_alloc(ce_card_t *c, ce_option_t *o, int offset, int anm_script, const char *card);
uint8_t *ce_option_retry(ce_card_t *c, ce_option_t *o, int offset, int anm_script,
                         uint32_t retry_frames, const char *card);
/* 收起子机：删它的 ANM VM + 清 flags 的 `CE_FLAG_EQUIP_RESET_BIT1`（原生 `CardReimu1__method_4C` 两件一起做）。
 * o 可为 NULL：`card+0x1c` 的 VM 与 flags 照样处理。**同时会收掉这个子机发射出去的激光**（见下）。*/
void     ce_option_drop(ce_card_t *c, ce_option_t *o);
/* 把「挂在这个子机上」的自机弹收掉 —— 一手：激光（`mode=2`）的寿命**只跟射击键走**
 * （`tick2` `0x45fc80` 尾部：`shoot_key_short_timer >= 0` 且不在死亡/对话态就继续活着），
 * 子机被收走时束**不会**自己消失 ⇒ 玩家按住 Z 会看到一条钉在旧位置的残束
 * （用户 2026-09-10 实跑：冬子机在季节解放后、不放开 Z 就还有激光）。
 * 收束序列照 `tick2` 自己的自杀段：`bullet+0x8c = 2` + 对束 VM 发 `interrupt 1`；
 * 之后 `Player__tick_bullets` `0x45edb0` 发现束 VM 没了就回收弹槽并清伤害源。
 * 返回收掉的弹数。`ce_option_drop` 已经会调它（不用自己调）。*/
int      ce_option_kill_bullets(void *option);
/* ★ 2026-09-13（117 咆哮「子机还在打普通激光」真身）：把**已经在飞的零售 / 装备子机
 * `mode=2` 激光**收掉。理由是上面同一条一手：激光寿命只跟 Z 走，**fire-time set 重定向
 * （`0x45ebfb`）只挡新弹、挡不住旧束**。只收 `packed set <= 0x15`（0..9 主炮 + 0x0a..0x15
 * 原生装备子机组）且 shooter row `+0x21 == 2` 的活动弹；0x16..0x1d（108/77/116）与
 * 0x1e..0x27（WOLF/EAGLE）都不碰。
 * 收束顺序逐字段对齐 `tick2` 自杀段：`bullet+0x8c = 2`（`0x460296`）→ 束 VM interrupt 1
 * （`0x4602a0`）→ **清 `player+0x476fc+idx*4` 的 mode=2 去重槽**（`0x4602c1`，idx 由
 * `shooter+0x20` 解码）。第三步不能省：`Player__shoot_one_bullet` `0x45e96e` 见该槽非 0
 * 就整条 mode=2 row 早退，旧版漏清导致咆哮后子机永久停火（用户 2026-09-13）。
 * 返回收掉的条数；n>0 时按 set 打证伪日志。*/
int      ce_kill_retail_option_lasers(void);
static inline uint8_t *ce_option_get(ce_card_t *c, ce_option_t *o, int offset, int anm_script,
                                     uint32_t retry_frames, const char *card)
{
    uint8_t *p = ce_option_claim(c, o);
    return p ? p : ce_option_retry(c, o, offset, anm_script, retry_frames, card);
}
/* 子机的世界坐标（`option+0x5c/0x60` 是定点数，× 1/128）*/
static inline float ce_option_x(const uint8_t *o) { return (float)*(const int32_t *)(o + CE_OPT_POS_X) * CE_SUBPIXEL_TO_PIXEL; }
static inline float ce_option_y(const uint8_t *o) { return (float)*(const int32_t *)(o + CE_OPT_POS_Y) * CE_SUBPIXEL_TO_PIXEL; }

/* ---- ★ 低速子机声明层（spec §3.3.1；纯逻辑在 sdk_core.{h,c}，落笔分派在 sdk.c 的 `BP_ce_item_frame`）----
 * 108（妖机体）与 111（SA 子机层）会落在**同一批** zPlayerOption 的同一批字段上 ⇒ 不让两张卡各写各的：
 * 各卡每帧只**声明**（`ce_option_pose_declare`），SDK 选举 **id 最小者落笔**，并且**只调它**的
 * `on_option_pose_apply`（ce_hooks_t 尾槽）。⇒ 「108 覆盖 111」不再依赖帧内 tick 顺序。
 *
 * 卡侧三条纪律：
 *   1. **每帧声明**（层在 `BP_ce_item_frame` 每帧清集合；不声明 = 本帧退出声明）。
 *      门也由卡自己把 —— 108 就是「低速才声明、高速声明 0」（用户定案 5：高速互不影响）。
 *   2. 落笔**只在 `on_option_pose_apply` 里做**：申请（`ce_option_get`）、逐格写偏移、贴图。
 *      回调里若发现 `ce_option_pose_winner() != ce_card_id(c)`（退位通知）⇒ 只收子机、不写字段。
 *   3. 偏移单位 = **像素**（引擎自己 ×128 变原生定点；一手 `th18:0x45d902` + `0x4b9310` = 128.0f）。
 *      `cell[i].fx/fy` = 聚焦（低速）用，`ux/uy` = 非聚焦用；引擎每帧按 `player+0x476cc` 自己选哪一套
 *      （一手 `th18:0x45bce8`–`0x45bd08`），所以两套都给、Shift 切换没有延迟。*/
int  ce_option_pose_declare(ce_card_t *c, const ce_option_pose_t *p);   /* p = NULL / count <= 0 = 退出；返回记下的格数 */
void ce_option_pose_drop(ce_card_t *c);                                  /* 删卡 / 自己退出声明 */
static inline int ce_option_pose_winner(void) { return ce_optpose_winner(); }
static inline const ce_option_pose_t *ce_option_pose_pose(void) { return ce_optpose_pose(); }

/* ---- ★★ 「原生子机（自机本体那 4 格）不许出现」声明层（2026-09-16，111 的魔理沙侧）----
 * 一手结构（两个数组互不相干）见 `engine.h` 的 `CE_NATIVE_OPT_*` 一节；整套机制的长注释在 `sdk.c`
 * 的 `BP_ce_repopulate_native_option` / `BP_ce_native_option_muzzle` 上面（**唯一出处**）。
 * 卡侧用法（111 就是样板）：
 *   · **声明**：`ce_native_option_hide_declare(ce_card_id(c), 1, 机体号)`（**粘性** ⇒ 每帧声明一次也无害）。
 *     机体门在层里：只有「当前机体 == 声明时给的那个」时才真的隐藏 ⇒ 换机体后**立刻**恢复原生行为
 *     （换机体发生在商店/编成里、那一刻卡 tick 冻着 ⇒ 不能指望卡来改口）。
 *   · **收干净**：`ce_layer_drop_card()`（`CE_CARD` 的 on_run_reset / opdelete 自动调）会撤声明；
 *     换关（`on_stage_start`）**不**撤 —— 卡还在卡组里，声明应当继续生效。
 *   · **每帧扫一遍**：`ce_native_option_sweep()` —— 把 4 格 × 2 支原生子机 VM（`+0xb0` 普通形 /
 *     `+0xb4` 满火力形）interrupt 掉并清 id、状态写 0。断点那两条已经覆盖「不建 / 不开火」，
 *     这一步是**自愈兜底**：卡刚装上那一刻场上可能已经有原生子机（例如玩家本来就在满火力），
 *     而 `repopulate` 的档位短路缓存会让引擎「不重建 = 也不清」⇒ 只靠断点会留下幽灵子机。
 *     返回清掉的 VM 数（诊断）。
 *   · ★★ 2026-09-18（111 实跑）：**隐藏子机 ≠ 收掉它已经在打的束** ⇒ 光靠上面这一支不够。
 *     `mode=2` 激光的寿命**只跟射击键（Z）走**、位置每帧从**持有者**读 ⇒ 持有者被清掉之后，
 *     束会**钉在原地**直到松手（症状：一条激光固定不动、随 Z 出没、power up 后换一处）。
 *     ⇒ 声明隐藏的卡要在同一处**每帧**再叫一次 `ce_kill_retail_option_lasers()`
 *     （只收 set ≤ 0x15 且 `row+0x21 == 2` 的活动弹 ⇒ **不会碰**追加组的自机弹）。
 *     ⚠ 每帧叫是必要的：power up 会让引擎 `repopulate` 重建子机，总有一帧「子机刚复活」而
 *     sweep 还没轮到 ⇒ 那一帧发出来的束就是新的一条。*/
void ce_native_option_hide_declare(uint32_t card_id, int on, int character);
int  ce_native_option_hidden(int character);
int  ce_native_option_sweep(void);
/* ★★★ 2026-09-23（108 的「子机不发」修法 B-1）：**只杀显示、保留记录**的 sweep。
 *   背景：引擎的主炮行循环按行里 `+0x20` 选膛口 —— `slot >= 1` 取的是**原生那 4 格**
 *   （一手 `0x45ec4c`）。而 `ce_native_option_sweep()` 会把 `state` 也写 0，跟随逻辑只更新
 *   `state == 2` 的格子 ⇒ 那些行的膛口就「没被点过名」⇒ **只有本体那一列发得出来**（owner 实跑：
 *   `native option slots at low speed - 0/4 live`，四格全是 `state 0`）。
 *   ⇒ 这一支只把 4×2 支 VM `interrupt` 掉并清 id（**显示消失**），`state` 与坐标**一个字不动**
 *     ⇒ 引擎继续跟随它们、行循环继续有活膛口。给「靠原生膛口发弹」的卡用（108）；
 *     声明「原生子机不许出现」的卡（111）继续用原来那支。*/
int  ce_native_option_sweep_keep_state(void);

/* ---- HUD 小卡上的一位数字（AUDIT §X）----
 * 引擎建好本卡的 HUD 图标后经 `+0x44` 把图标根 VM 的 anm id 交来（图标重建时会再交一次）；
 * 在它的实体坐标上起 `ability.anm` 的 `HUD_DIGIT` 脚本，值变了就换 sprite。
 *   `on_hud_anm`           → `ce_hud_digit_attach(&d, id, 当前值, "卡名")`（旧数字先删）
 *   值变了 / `on_tick_2`    → `ce_hud_digit_set(&d, 值, "卡名")`（每帧对账，只有指针变了才真动手）
 *   `on_run_reset`         → `ce_hud_digit_clear(&d)`
 * 值钳在 0..CE_HUD_DIGIT_MAX —— 贴图只有十张。*/
#define CE_HUD_DIGIT_MAX 9
typedef struct {
    uint32_t icon_id;                /* 引擎交来的 HUD 图标根 VM id */
    uint32_t digit_id;               /* 我们的数字 VM id */
    int      shown;                  /* 当前显示的数字（-1 = 还没设）*/
} ce_hud_digit_t;

void ce_hud_digit_attach(ce_hud_digit_t *d, uint32_t icon_anm_id, int value, const char *card);

/* 多位数字，用 ANM 图元画（不是 ascii 文字）。
 * ★ 存在的理由：HUD 文字的绘制优先级（Gui 0x31 / AbilityManager 0x33）**永远早于**任何 UI 层图元（≥ 0x3c），
 *   所以「把数字画在自己的底板上面」用 ce_hud_text 做不到。图元对图元就没这个问题 —— 后起的画在上面。
 * 坐标是 UI 层的 2× 空间；`right_align` 时 x 是右边界。脚本 `HUD_NUMBER` 的 pos 是 (0,0)，
 * 所以位置完全由实体坐标决定。值为负按 0 处理；超过位数按最低几位显示。*/
#define CE_HUD_NUMBER_MAX 6
typedef struct {
    uint32_t id[CE_HUD_NUMBER_MAX];
    int      shown[CE_HUD_NUMBER_MAX];
    int      digits;
} ce_hud_number_t;
void ce_hud_number_set(ce_hud_number_t *n, float x, float y, float spacing,
                       int value, int digits, int right_align, const char *tag);
void ce_hud_number_clear(ce_hud_number_t *n);

/* —— 贴图 VM 版「文字」：TH18 **font 2** 的原生字形（0-9 . % + -），逐字一个 VM ——
 * ★ 存在的理由：**ASCII 文字永远盖不到暂停模糊罩下面**（实跑证伪：唯一早于罩子的组 1 趟
 *   `0x419950` 跑在 surface 1 + 相机 0 上，画出来看不见），而贴图 VM 放 UI 层 25
 *   （折到组 38 → 与罩子同桩 `0x3e`、罩子后起 ⇒ 被罩住）就能做到。
 * 排版**照抄 `AsciiManager__draw_string` `0x41a2b0` 的 font 2/3 分支**：
 *   · 字距 = `scale_x × 7`，`.` 特例 `× 4`（`0x41a487` 段）；坐标空间与 `ce_hud_text_ex` 相同（640×480），
 *     内部 × `DAT_0056aca0` 换表面 px；
 *   · 水平对齐（`align_h`）：0 = 居中（每个字退半个字距）、1 = 左、2 = 右（退满整个字距）
 *     —— 与引擎的 `record+0x130` 三个分支逐条对应；
 *   · 字形是 **16×20** 的单元（`ascii1280` 变体），贴图 entry 声明 16×32、上下各补 6 px 透明，
 *     于是「精灵中心」= 单元水平中线；VM 位置 = `(字笔 x, (y + 5) × DAT)` 就与引擎画出来的像素对齐。
 * 第 0 档只认 15 个字符（其余按字距跳过、不画）：`0-9 . % + -` 与空格；
 * 第 2 档（`CE_HUD_FONT_TH18_FONT2B`）多认 `:` 与 `*`。
 * ★ 2026-09-23 加了 `:` 与 `*`（卡 112 的计时器 / ×N 弹字要从 ASCII 换成贴图 VM 文字，
 *   才能进暂停模糊罩）；两者的一手映射 = 引擎字符表 `th18:0x4b019c` 的 `0x3a`/`0x2a`，
 *   贴图以单图行 append 在 ORDER.txt 末尾（`ATLAS_FONT2` 属已定稿段不许插行）。*/
#define CE_HUD_VM_TEXT_MAX 10
typedef struct {
    uint32_t id[CE_HUD_VM_TEXT_MAX];     /* 每个字形一个 VM；0 = 空 */
    uint32_t shown[CE_HUD_VM_TEXT_MAX];  /* 该 VM 当前挂的 sprite 号（省一次 set_sprite）*/
    int      n;                          /* 上一帧用了几个槽 */
} ce_hud_vm_text_t;
void ce_hud_vm_text_set(ce_hud_vm_text_t *t, float x, float y, uint32_t bgra,
                        int align_h, float scale, const char *s, const char *tag);

/* ★ 2026-09-15：**字形集**参数版（`font` 取下面的枚举）。为什么要它：卡 113 的灵界剩余时间
 * 要 TH13 那张 `ascii.png` 的字形（用户口径：不用 TH18 遗留的字形），而那套字形住在自己的图集
 * `ATLAS_TH13ASCII`（32×40 字格、16 px 步进、`s` 落在 x-height）⇒ 字距/行高/VM 偏移都与 font 2
 * 不同 ⇒ 做成"只换字形表、不换代码"。`ce_hud_vm_text_set` = `…_font(…, CE_HUD_FONT_TH18_FONT2)`，
 * 其它卡的行为一个字节不变。*/
enum { CE_HUD_FONT_TH18_FONT2  = 0,  /* TH18 font 2（16×32 画布、7 px 步进、'.' 4 px）—— 默认 */
       CE_HUD_FONT_TH13_BIG    = 1,  /* TH13 大字体（32×40 字格、16 px 步进、's' 在 x-height）*/
       /* ★ 2026-09-24：TH18 font 2 的**完整字形集**（`ATLAS_FONT2B`，比第 0 档多 `:` 与 `*`）。
        *   度量与第 0 档逐值相同 ⇒ 只是"认得的字符"多两个、贴图来自另一个 entry。
        *   为什么要另一档：字形 VM 绑在固定脚本上，sprite 号按**该脚本所属 entry** 解析 ——
        *   第 0 档那个图集（`ATLAS_FONT2`）在 ORDER 的已定稿段、不能插行，所以另起一套。*/
       CE_HUD_FONT_TH18_FONT2B = 2,
       /* ★★ 2026-09-25 第 4 档：**TH12 `ascii/ascii.png` 的数字字形**（`ATLAS_FONT_TH12`，16×16 字格、
        *   均匀 16 px 步进）。owner 第 27 轮口径：「爆炸后的 **×n.0** 和**点数**的字体完全不对，应该用的是
        *   `…\[th12] 东方星莲船\ascii\ascii.png` 里面**倒数第三行的数字**」—— 那一行（y=216）的
        *   16×16 那族正是 `0-9`（sprite 243..252）+ `.`(254) + `,`(257) + 乘号(256)。
        *   切图/断死见 `assets/ability/make_th12_font_art.py`；脚本 = 362（136/332 的孪生）。*/
       CE_HUD_FONT_TH12_DIGITS = 3 };

void ce_hud_vm_text_set_font(ce_hud_vm_text_t *t, float x, float y, uint32_t bgra,
                             int align_h, float scale, const char *s, const char *tag, int font);

void ce_hud_vm_text_clear(ce_hud_vm_text_t *t);
void ce_hud_digit_set(ce_hud_digit_t *d, int value, const char *card);
void ce_hud_digit_clear(ce_hud_digit_t *d);

/* 原始键盘状态（`GetKeyboardState`，user32）。TH18 自己的输入掩码只认配置里的 6 个键
 * （Shot=Z / Bomb=X / Slow=Shift / Pause=Esc / Item=C / Change=D；配置表 `th18:0x4ca470` 起，
 * 一手见 engine/input/th18/01），**A / S 没被任何动作占用**（D 是 Change）——
 * 卡要用「游戏不认识的键」时读这里。
 * ⚠️ 这些键**不进 replay 输入流**（回放只录 `th18:0x4ca428` 的 16 位掩码）→ 用它触发的效果在回放里不重现。
 * ⚠️ **空格（SPACE）是调试键（owner 2026-09-21 口径）** —— 111 灵界调试、112 令牌调试、106 满 P 测试键都在用它
 * ⇒ **玩法功能禁止占用空格**；玩法用的新键走「掩码空闲位 + 未被占用的键（A/S）」那条路，见 `WORK-PACKAGES.md` §1b/§1c。*/
#define CE_VK_A 0x41
#define CE_VK_S 0x53
#define CE_VK_C 0x43
#define CE_VK_D 0x44
#define CE_VK_SPACE 0x20
#define CE_VK_CONTROL 0x11   /* VK_CONTROL：调试组合键（Ctrl+空格）用 */
#define CE_VK_SHIFT 0x10
int ce_key_down(int vk);

/* 在 HUD 上写一行字（只能在虚表 `+0x48 on_draw` 里调 —— 原生 `CardMomoyo__on_draw` 同款，AUDIT §AC）。
 * 坐标用 Gui 自己那套字面量空间（火力在 (540, 182)）；color 是 BGRA 一个 dword，画完自动还原成白。
 * 自己先 vsnprintf 成一行再交给引擎的 `"%s"`，不转发 varargs。 */
void ce_hud_text(float x, float y, uint32_t bgra, const char *fmt, ...);
/* 同上，但把 AsciiManager 的排版状态全摊开：描边色（0 = 不动）、字体 / 分组 / 水平对齐
 * （各 -1 = 不动）、缩放（≤0 = 不动）。想和金钱那行一样就传
 * `CE_ASCII_FONT_MONEY, CE_ASCII_GROUP_MONEY, CE_ASCII_ALIGN_RIGHT`，想更大再给 scale。
 * 每一项都先存后改、画完还原 —— 引擎后面的绘制全靠这些环境值。*/
void ce_hud_text_ex(float x, float y, uint32_t bgra, uint32_t shadow,
                    int font_id, int group, int align_h, float scale, const char *fmt, ...);

/* sdk.c：主动卡机器（桩里调）*/
int  ce_sdk_c_press(void *self, const ce_hooks_t *h);
void ce_sdk_active_tick(void *self, const ce_hooks_t *h);
void ce_sdk_active_reset(void *self, const ce_hooks_t *h, int clear_recharge);

/* sdk.c */
void ce_sdk_trace(uint32_t id, unsigned slot, const char *name);   /* trace 开着才记；每张卡每槽记第一次 */
/* 伤害层（§AA）：★ 2026-09-13 起**落笔点搬到了卡链遍历之后**（断点 `BP_ce_bullet_damage_final`，
 * `th18:0x45e7ff`；原因与一手见 `BP30-IMPLEMENTATION.md` §1、`CARD-NATIVE-CONFLICT-AUDIT.md` §B-1）：
 * 原来落笔在 `+0x28` 的桩里，与原生 25 Saki / 54 Momoyo（**同槽、从 shooter 重算**）抢同一条链，
 * 谁在链里靠后谁说了算 ⇒ 我们的倍率会被静默抹掉。现在**只有那一个 handler 写** `bullet+0x9c`，
 * 所以卡侧一个字节都不用改，`+0x28` 桩也不再缩放（不存在「同帧施加两次」）。
 * 本函数是那一次落笔的实现：读聚合倍率、就地乘**别人算完的值**（乘 ≠ 覆盖）。*/
void ce_sdk_bullet_damage_final(void *bullet);
void ce_sdk_damage_cap_tick(uint32_t id);
void ce_sdk_damage_hud_draw(uint32_t id);
void ce_sdk_on_shop_open(void);                                   /* shop.c 在 MSG 开店那一帧调：沿链表分派 on_shop_open */
/* collect-2x（辉针城卡）presence 续批谓词（§Z21/§Z22）：池里有没有批集道具（type∈{1,2}）在
 * **真空飞行**（state == CE_ITEM_STATE_VACUUM）—— state 3 = V 门（上线/放 B/对话）放飞的收点道具
 * （state 机模型见 engine.h CE_ITEM_STATE 注）。§Z22 收窄掉 state 4：70px 环 swoosh = 普通游玩磁吸
 * （任何时刻都在发生），不属全屏收点 → 不续批（run-9：普通游玩环收把批撑过 5 → 误弹灰 NO BONUS）。
 * state 1（抛物/坠落）不算：场道具不续批（TH14 原义，0x43898e 只在 state-3 在飞体上重载）。早停。
 * 由 collect2x.c 在批开时每帧调。*/
int ce_item_pool_any_attracting(void);
/* 局中从卡组删一张卡（AUDIT §W）：照 recount 0x4080e0 的删卡分支——摘链、按 flags 减类别计数、num_total−1、
 * owned[id] 清零（能再刷出）、虚表 +0x50 删对象、重画 HUD 三行。★ 不能删当前选中的主动卡（mgr+0x38 指向它，
 * 重选要重建 HUD 图标 VM，本函数拒绝并返回 0）。只能在没有人正在遍历链表时调（ctor / 事件断点里都可以，链表遍历桩里不行）。*/
/* 随机挑一张可删的卡（链表里除 exclude 与选中主动卡以外的任意一张，RNG = 游戏 replay 安全流）；没有返回 NULL */
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
    static int CE_TC ce_s_##ID##_on_bullet_created(void *self, void *b) { \
        ce_sdk_trace(ID, 0x28, "on_bullet_created"); \
        /* ★ 2026-09-13：这里原来调 `ce_sdk_scale_bullet_damage(ID, b)` —— 伤害层的落笔**已搬到** \
         * 卡链遍历之后的断点 `BP_ce_bullet_damage_final`（th18:0x45e7ff）。别在这里再加回来： \
         * 那会与原生 25/54 的重算抢同一条链，且同帧施加两次（见 sdk.h 上方与本行出处）。*/ \
        return ce_hooks_##ID.on_bullet_created ? ce_hooks_##ID.on_bullet_created((ce_card_t *)self, b) : 0; } \
    static int CE_TC ce_s_##ID##_c_press(void *self) { \
        ce_sdk_trace(ID, 0x08, "c_press"); \
        return ce_sdk_c_press(self, &ce_hooks_##ID); } \
    static int CE_TC ce_s_##ID##_on_tick_2(void *self) { \
        ce_sdk_trace(ID, 0x2c, "on_tick_2"); \
        /* ★★ 调试件：「逐帧合成方向输入」（调试键 = 空格）。**必须在卡 tick 这里调** —— \
         *   本桩运行在 AbilityManager 节点（0x16），**早于 Player（0x17）**； \
         *   ItemManager（`ce_item_frame` = 0x1e）在玩家之后，那里写输入玩家读不到。 \
         *   同帧多张卡各调一次 ⇒ 内部用 `ce_sdk_frame()` 去重，只推进一格。 \
         *   不按调试键时一个字节都不写；`CE_INPUT_SYNTH_ENABLED=0` 时是编译期空壳。*/ \
        ce_input_synth_tick(); \
        ce_sdk_active_tick(self, &ce_hooks_##ID); \
        ce_sdk_damage_cap_tick(ID); \
        return ce_hooks_##ID.on_tick_2 ? ce_hooks_##ID.on_tick_2((ce_card_t *)self) : 0; } \
    CE_STUB2(ID, on_enemy_drop, 0x30) \
    static void CE_TC ce_s_##ID##_on_stage_start(void *self) { \
        ce_sdk_trace(ID, 0x34, "on_stage_start"); \
        ce_sdk_active_reset(self, &ce_hooks_##ID, 0); \
        if (ce_hooks_##ID.on_stage_start) ce_hooks_##ID.on_stage_start((ce_card_t *)self); } \
    CE_STUB1(ID, on_hud_anm, 0x44, uint32_t) \
    static int CE_TC ce_s_##ID##_on_draw(void *self) { \
        ce_sdk_trace(ID, 0x48, "on_draw"); \
        /* ★★ 调试件「逐帧合成方向输入」的状态字（生效时才画；同帧去重）。 \
         *   必须在 on_draw 的桩里调 —— `ce_hud_text` 只允许在虚表 +0x48 里用。*/ \
        ce_input_synth_draw(); \
        ce_sdk_damage_hud_draw(ID); \
        return ce_hooks_##ID.on_draw ? ce_hooks_##ID.on_draw((ce_card_t *)self) : 0; } \
    static void CE_TC ce_s_##ID##_on_run_reset(void *self) { \
        ce_sdk_trace(ID, 0x4c, "on_run_reset"); \
        /* ★★★ 2026-09-27（第三十九批 · owner「retry 不清空值的」）：**局级共享层的复位点**。\
         *   TH18 的 retry（gamemode 10/11 → `GameThread__teardown_and_recount_cards` `0x4432c0`\
         *   → `recount_and_recategorize_cards` `0x4080e0`）对**存档卡组的持久卡**就是走虚表 `+0x4c`\
         *   这一条（`0x408151`–`0x408155`；本局买的卡走 `+0x50` 被删对象）；换关走 `+0x34`/`+0x20`\
         *   **不经过这里**。⇒ 共享 PIV 池（`sdk_core.c` 的 `s_piv_pool`，屏幕那个 6 位数读它）\
         *   正好在这一刻归零 —— 它此前**全仓没有任何生产复位点**（`ce_piv_reset()` 只有单测调过，\
         *   `ce_score_reset()` 只在 DLL 装载那一次），所以 retry 之后屏上还是上一局的数。\
         *   幂等（recount 对每张持久卡各调一次 ⇒ 池本来就是 0）；`ce_piv_reset()` 保留「在场声明」。*/ \
        ce_piv_reset();                       /* ★ 共享「额外最大得点」池：局级复位（见上）*/ \
        ce_sdk_active_reset(self, &ce_hooks_##ID, 1); \
        ce_dmg_set(ID, ce_hooks_##ID.bullet_damage_permille); /* 回到声明的初值；声明 0 的卡也要清掉运行时报过的倍率 */ \
        ce_layer_drop_card(ID);               /* 2026-09-12：四个声明层 + HUD 槽位一起退回声明态 */ \
        if (ce_hooks_##ID.on_run_reset) ce_hooks_##ID.on_run_reset((ce_card_t *)self); } \
    static void CE_TC ce_s_##ID##_opdelete(void *self, uint32_t flag) { \
        ce_sdk_trace(ID, 0x50, "operator_delete"); \
        ce_dmg_set(ID, 0);                    /* 退出聚合；无条件，运行时才参与的卡同样要清 */ \
        ce_layer_drop_card(ID);               /* 同上：无敌 / 计分 / 子机声明 / 消弹闩锁 / HUD 槽位 */ \
        if (ce_hooks_##ID.on_delete) ce_hooks_##ID.on_delete((ce_card_t *)self); /* 私有状态还活着 */ \
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
    static ce_behavior_t ce_beh_##ID = { ID, ce_vt_##ID, #__VA_ARGS__, &ce_hooks_##ID, 0 }; \
    __attribute__((constructor)) static void ce_reg_##ID(void) { \
        ce_beh_##ID.is_active = ce_hooks_##ID.active_recharge != 0; \
        ce_sdk_register_or_log(&ce_beh_##ID); \
    }

/* ==================== 共享 helper（EVENTS-CONTRACT.md：四个共享件）====================
 * 这四个是「多张卡都要做同一件事」提炼出来的，别在卡里自己再写一份。
 * 已有件（不用重复造）：消弹计数 = `ce_cancel_radius(pos, r, max_count, mode)` 的**返回值**
 * （它调前清 CE_BM_CANCEL_COUNTER 0x7a41e8、调后读 = 消掉几颗；`ce_cancel_all_bullets()` 是 void、数不出来）。 */

/* ① 全屏收点：把道具池里所有活道具的 state 置 3（真空飞行）⇒ 引擎自己把它们飞向自机、走原生收集
 * （计分 / HUD / 计数 / 磁吸全原生）。用于 **107 森罗结界「自动回收道具」** 与 **111「满槽全收」**。
 * 返回被置 3 的道具数。
 *
 * ★★ 2026-09-16 修（用户实跑：「伊吹特技的信仰点和 P 点金钱吸收动态不一致，**信仰点快了**」）：
 *   **原来只写 state、漏了磁速字段** ⇒ 那些**本来在 state 4（70px 环吸）**的道具会带着
 *   **环吸那一档速度（= 玩家磁速 ÷3，`th18:0x44663b`-`0x446651`）** 进入 state 3，
 *   而我们的自建点一进去就是满磁速（`faith_collect_all()`）⇒ 同屏「绿点飞快、P/金钱慢吞吞」。
 *   一手：引擎自己翻 state 3 的那一处**两个字段一起写** ——
 *     `th18:0x445f63: mov eax,[player+0x47988]` → `0x445f69: mov [item+0xc80],eax` →
 *     `0x445f6f: mov DWORD [item+0xc74],3`
 *   （`+0xc74` = state、`+0xc80` = `CE_ITEM_MAG_SPEED`；随后每帧 `+0.2`、上限 12.0，
 *    见 `0x44611b`/`0x446116`。）
 *   ⇒ 这里照抄：state 与磁速**一起写**（玩家指针为空或磁速 ≤0 时只写 state）。
 *   ⚠ 仍**不动 pos/vel**（视觉依旧是「从原地飞向自机」）。*/
static inline int ce_item_collect_all(void)
{
    uint8_t *mgr = CE_ITEM_MANAGER();
    uint8_t *pl = CE_PLAYER();
    float mag = 0.0f;
    int n = 0;

    if (!mgr) return 0;
    if (pl) mag = *(const float *)(pl + CE_PLAYER_ITEM_ATTRACT_SPD);   /* 一手 `0x445f63` 读的就是它 */
    for (unsigned i = 0; i < CE_ITEM_SLOTS; ++i) {
        uint8_t *it = mgr + CE_ITEM_POOL_OFF + (size_t)i * CE_ITEM_STRIDE;
        int32_t st = *(int32_t *)(it + CE_ITEM_STATE);
        if (st == 0 || st == CE_ITEM_STATE_VACUUM) continue;        /* 空槽 / 已经在飞 */
        if (pl && mag > 0.0f) *(float *)(it + CE_ITEM_MAG_SPEED) = mag;   /* ★ 与 state 同帧写（一手）*/
        *(int32_t *)(it + CE_ITEM_STATE) = CE_ITEM_STATE_VACUUM;
        ++n;
    }
    return n;
}

/* ② 无敌帧请求（声明层）：各卡每帧声明自己想要的帧数，层取**最大值**，由这里落到
 * `player+0x47774`（zTimer）。▸ 只在比当前计时器更长时才写（不截短别人给的无敌，也不缩短引擎自己的）。
 * 用于 **107 结界结束 0.67s**、**109 爆风 C1–C4** 与 **117 霊撃 40 帧**（117 原来的局部 set_invuln 就是这个语义）。
 * ★ 卡只**声明**（`ce_invuln_request(id, frames)`，每帧声明剩余帧数），落笔由 SDK 在每帧的
 * `ce_item_frame` 断点里统一调本函数做 —— 卡不必自己调（自己调也幂等，同一 max）。*/
static inline void ce_invuln_apply(void)
{
    int frames = ce_invuln_frames();
    ce_timer_t *t = CE_PLAYER() ? CE_PLAYER_INVULN() : 0;
    if (!t || frames <= 0) return;
    if (t->cur < frames) { t->prev = t->cur; t->cur = frames; t->cur_f = (float)frames; }
}

/* ③ 「本帧已消过弹」闩锁：同帧多张卡都要全屏消弹时，只有第一个认领的去做。
 * 每帧由 SDK 在 `ce_item_frame` 断点里 reset（见 sdk.c）；卡侧这么用：
 *     if (ce_cancel_latch_claim(ce_card_id(c))) { n = ce_cancel_radius(pos, 4000.0f, 0, 0); ... }
 * 返回非 0 = 本帧由你（这张卡）负责消弹。*/
static inline int ce_shoot_cancel_claim(uint32_t card_id) { return ce_cancel_latch_claim(card_id); }

/* 难度（0 EASY / 1 NORMAL / 2 HARD / 3 LUNATIC / 4 EXTRA）—— 一手 = `engine.h` 的 `CE_ADDR_DIFFICULTY`（`0x4ccd00`）。
 * 用途（2026-09-18）：共享 PIV 池族的卡用它查自己那一档的「可写上限 = cap_i − init_i」（`ce_piv_claim_auto`）。*/
static inline int ce_difficulty(void) { return *(volatile int32_t *)CE_ADDR_DIFFICULTY; }

/* 一次性把 ID 的所有声明层退回「不参与」态（CE_CARD 的 on_run_reset / opdelete 里自动调，卡不用管）。*/
static inline void ce_layer_drop_card(uint32_t id)
{
    ce_dmg_set(id, 0);
    ce_invuln_request(id, 0);
    /* ★ 2026-09-13 计分层重做：**三个加数通道 + 旧接口通道都要退**（少退一个 = 被删的卡
     * 永远留在聚合里，见 SCORE-LAYER-REWORK.md §5「删卡的出口」）。*/
    ce_score_declare_delta(id, 0);
    ce_score_declare_coupled(id, 0);
    ce_score_declare(id, 0);       /* legacy 通道（旧卡用；0 = 不参与）*/
    ce_score_mul_declare(id, 0);   /* 信号强度（耦合通道乘数的一半）*/
    ce_optpose_drop(id);              /* 低速子机声明层（spec §3.3.1）：删卡 = 立刻退出声明 */
    ce_poc_release(id);               /* ★ PoC 高度层（2026-09-15）：撤声明 + 需要时**立刻**把字段写回基准
                                       *   （删卡在商店里 ⇒ 帧钩子不跑，不能等帧界；见本文件上方那一节）*/
    ce_drop_mult_set(id, 0);          /* 掉落乘数（EVENTS-CONTRACT §2）*/
    /* ★ 2026-09-13 补（跨卡互作审计）：**「中弹无伤」声明也要在这里撤**。
     *   `ce_no_hit_declare` 是**粘性**的（任一声明即生效、直到显式撤），
     *   而删卡的路（「弃掉整副卡组」/ 商店里被倒吊人删）**只有 `on_delete` 跑得到** ⇒
     *   漏撤 = 那一格声明永远挂在池里 ⇒ **整个进程的闸门永久开着**。
     *   另一半（`ce_sdk_setup` 的复位）同日已补，见 `sdk.c:1400` 一带。*/
    ce_no_hit_declare(id, 0);
    ce_full_p_gate_declare(id, 0);   /* ★ 「未满 P」声明层（106）*/

    /* ★ 2026-09-18：**共享 PIV 池族 + 擦弹族**也要在这里退（用户口径「多卡同装 = 多加一套系统」，
     *   删卡 = 那套系统退场 ⇒ 它的读数不该再进公式；漏退 = 被删的卡永久留在聚合里）。*/
    ce_piv_release(id);
    ce_graze_family_release(id);
    ce_score_family_declare(id, CE_COEFF_FAMILY_NONE);   /* 系数族（2026-09-18 换版）也要退 */

    /* ★ 2026-09-16（111 魔理沙侧）：「隐藏原生子机」同样是**粘性**声明 ⇒ 也必须在这里撤
     *   （漏撤 = 那台机体的原生子机永远不出现，日志上只看得到"没出现"）。机体号一并清 0。*/
    ce_native_option_hide_declare(id, 0, 0);
    ce_hud_slot_release(id);
}

/* ★★★ 2026-09-24 **实例/组级护栏位（110/111 ↔ 112 的妖精冲突）** —— 只读通报，语义三条：
 *   · **谁写**：110/111 的绿妖精换皮（`mof.c` 的 `mof_green_tick()`）—— 每帧对自己**真的改上的**
 *     那几行（`mof_green_pairs` 的 `src_script`，N 恒 2）重申一次；
 *   · **谁读**：112（星莲船）的令牌宿主候选判断 —— 命中就**当场放弃这只**（不发皮、不掉令牌）；
 *   · **为什么有这个**：owner 口径「**别同时操作同一只妖精的 VM，否则爆了**」；112 的宿主池
 *     `native/cards/ufo_carrier_table.h` 里**含 `(2,0)`**（st01 蓝妖精那一组 = 我们换皮的那组）
 *     ⇒ 必须有个只读位让两边错开。**我们这侧是整组换皮（sprite 表项覆盖）**，所以按「组」通报足够；
 *     若将来要精确到某一只，再用 `ce_fairy_claim_instance_*()` 那一对（同一张表，按 enemy id）。
 *   ⚠ 通报是**帧内有效**：每帧重新声明，过期条目自动失效（`ce_sdk_frame()` 比对）。*/
#define CE_FAIRY_CLAIM_MAX   32
void ce_fairy_claim_group(uint32_t card_id, int32_t anm_src, int32_t anm_script);  /* 每帧重申 */
int  ce_fairy_group_claimed(int32_t anm_src, int32_t anm_script);                   /* 只读 */
void ce_fairy_claim_instance(uint32_t card_id, int32_t enemy_id);                   /* 可选：精确到只 */
int  ce_fairy_instance_claimed(int32_t enemy_id);                                   /* 只读 */
void ce_fairy_claim_clear(uint32_t card_id);                                        /* 删卡/换关收干净 */

/* ---- 自建真敌人载体（108 使魔 / 112 彩虹令牌 / 后续任何「要能被打、能掉东西」的实体）----
 * 一手 ABI 见 `engine.h` 的 `CE_FN_ALLOCATE_NEW_ENEMY`：名字是**加载中的 `default.ecl` 的 ECL 子程序名**
 * （不是 C 函数名），查不到时 `0x48b3a0` 干净退出 ⇒ 占位期可用原版自带的惰性壳（108 用 `"NoEnemyDebug"`）。
 * `spawn` 至少 0x54 字节（字段表 `engine.h` 的 `CE_SPAWN_*`；卡可只填 pos/life/main_drop，其余 0）。
 * ⚠ `spawn+0x14`（life）必须 < 1000；⚠ **禁止在敌人死亡栈里调**（DEATH §2.4：死亡回调中析构/新建会重入）。
 * 返回新敌人指针（NULL = 失败）。调用方自己保证主线程（桩 / 断点 / tick 回调内）。*/
static inline void *ce_spawn_enemy(const char *ecl_name, const void *spawn_params)
{
    void *mgr = (void *)CE_ENEMY_MGR();
    if (!mgr || !ecl_name || !spawn_params) return 0;
    return ((ce_fn_alloc_enemy_t)CE_FN_ALLOCATE_NEW_ENEMY)(mgr, ecl_name, (void *)spawn_params, 0);
}

/* ---- HUD 槽位（HUD-LAYOUT.md §3）：表面 px ↔ 640 空间 ----
 * `ce_hud_vm_text_set` / `ce_hud_number_set` 收的是 640×480 空间的点（引擎内部 × `DAT_0056aca0`
 * 换表面 px；TH18 表面 = 1280×960 ⇒ 系数 2.0，🟡 待实跑确认一次）。*/
#define CE_HUD_SURFACE_TO_640   CE_HUD_640_SCALE

static inline int ce_hud_slot_claim_card(uint32_t card_id, const ce_hud_block_t *blk, ce_hud_slot_t *out)
{
    /* ★ 关卡纪元（HUD-LAYOUT §4.4）：关卡号变了 ⇒ 先把上一关的占位清掉再认领。
     * 只在「认领前」比较 ⇒ 同一关内（局中删卡）一笔都不重排；而删卡留下的占位会在下一关收敛。
     * 卡侧一律用这个入口，别直接 `ce_hud_slot_claim()`（那样不会重排）。
     * ★ 2026-09-22：顺带把「原生那块『当前卡』UI 在不在」推进货架（`sdk_core.h` 的 `ce_hud_card_set()`）
     *   —— 排布只在认领时算一次，所以在这里设一次就够。判据 = `ABILITY_MANAGER->+0x38`（一手，
     *   见 `engine/card/th18/15-selected-card-hud.md` §2/§3）。*/
    ce_hud_stage_begin(CE_CURRENT_STAGE());
    ce_hud_card_set(hud_card_present(*(void *const *)(CE_ABILITY_MGR() + CE_MGR_SELECTED_ACTIVE)));
    return ce_hud_slot_claim(card_id, blk, out);
}

/* ★ 自带锚点认领（`CE_HUD_SIDE_PINNED`）：按用户量过的固定位置登记（117 用）。
 * 与 `_card` 版一样先把关卡号推进纪元 ⇒ 新关卡照样要重新认领。*/
static inline int ce_hud_slot_claim_pinned_card(uint32_t card_id, const ce_hud_block_t *blk,
                                                float anchor_x, float anchor_y, ce_hud_slot_t *out)
{
    ce_hud_stage_begin(CE_CURRENT_STAGE());
    ce_hud_card_set(hud_card_present(*(void *const *)(CE_ABILITY_MGR() + CE_MGR_SELECTED_ACTIVE)));
    return ce_hud_slot_claim_pinned(card_id, blk, anchor_x, anchor_y, out);
}

static inline void ce_hud_slot_pos640(const ce_hud_slot_t *s, float *x640, float *y640)
{
    if (!s) return;
    if (x640) *x640 = s->x * CE_HUD_SURFACE_TO_640;
    if (y640) *y640 = s->y * CE_HUD_SURFACE_TO_640;
}
