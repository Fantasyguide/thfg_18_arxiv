/* shop_core.h —— 「商店走两遍」的纯逻辑：什么时候该让 GameThread 再开一家店。
 *
 * 纯 C11（只 <stdint.h>），主机上 make test-host 直接测。断点 / 引擎访问在 shop.c。
 *
 * 引擎侧的事实（engine/card/th18/05-shop-and-money.md §3.5，一手）：
 *   开店 = 关末 MSG opcode 36 置 GameThread+0xb0 的 0x20000 位；GameThread__on_tick 0x443b05 看到就 new 一家店；
 *   关店 = 商店自己在 state 5（成交后 30 帧）析构，把全局商店指针清零；
 *   冻结 = 敌人 / 弹幕 / GUI(MSG) 都是「商店指针非 0 就跳过本帧」；
 *   优先级 商店 0xc → GameThread 0x10 → 敌人 0x1b → GUI 0x21：商店在自己 tick 里关门，同一帧 GameThread 就能重开，无空档帧。
 *
 * 状态机只看两件事：成交断点（0x4183ea）记「本次进店买了」；GameThread 断点每帧问一次「要不要重开」。
 */
#pragma once
#include <stdint.h>

#define CE_SHOP_VISITS_DEFAULT 2      /* 每关进店次数 */
#define CE_SHOP_REOPEN_WINDOW  300    /* 成交到关店最多隔这么多帧（原生 30 帧；留余量，防止陈旧状态跨局误开）*/

typedef struct {
    int visits;          /* 每关次数（≥1）*/
    int visits_left;     /* 本关还能再开几家 */
    int bought;          /* 本次进店已成交（成交断点置 1，重开或作废时清 0）*/
    int bought_stage;    /* 成交时的 CURRENT_STAGE */
    int bought_frame;    /* 成交时的 TIME_IN_STAGE */
    int last_bought_frame; /* 最近一次成交帧（重开后仍保留，给日志算间隔）*/
} ce_shop_state_t;

void ce_shop_reset(ce_shop_state_t *s, int visits);

/* 成交断点：记下关卡与帧 */
void ce_shop_on_bought(ce_shop_state_t *s, int stage, int frame);

/* GameThread 断点（每帧一次，在 test eax,0x20000 之前）。
 *   flag_set  = eax 已带 0x20000（MSG 刚要求开第一家店）
 *   shop_open = 商店指针非 0
 *   blocked   = 练习模式 / replay 回放（这两种里商店 30 帧自动退、不成交，本来也不会重开；显式挡一道）
 * 返回 1 = 该把 0x20000 位加回 eax，让 GameThread 这一帧再开一家。*/
int ce_shop_on_gamethread(ce_shop_state_t *s, int flag_set, int shop_open, int stage, int frame, int blocked);
