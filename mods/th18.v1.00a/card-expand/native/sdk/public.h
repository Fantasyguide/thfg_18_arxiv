/* Stable card-author surface. TH18 addresses and ABI wrappers stay in ../sdk.h. */
#pragma once
#include <stdint.h>

typedef struct ce_card ce_card_t;
#define CE_ACTIVATE_REFUSED (-1)
void *ce_state_user(const void *card, unsigned size);
#define ce_state(c, T) ((T *)ce_state_user((c), sizeof(T)))

typedef struct {
    int  (*ctor)(ce_card_t *);
    int  (*dtor)(ce_card_t *);
    int  (*on_death_after_deathbomb)(ce_card_t *, uint32_t);
    int  (*on_death_before_deathbomb)(ce_card_t *);
    int  (*on_death_frame2)(ce_card_t *);
    int  (*on_power_level_change)(ce_card_t *);
    int  (*on_tick_shooters)(ce_card_t *, uint32_t, uint32_t);
    int  (*on_load)(ce_card_t *);
    int  (*on_tick)(ce_card_t *);
    int  (*on_bullet_created)(ce_card_t *, void *);
    int  (*on_tick_2)(ce_card_t *);
    int  (*on_enemy_drop)(ce_card_t *, uint32_t, uint32_t);
    void (*on_stage_start)(ce_card_t *);
    int  (*on_hud_anm)(ce_card_t *, uint32_t);
    int  (*on_draw)(ce_card_t *);
    void (*on_run_reset)(ce_card_t *);
    void (*on_item_score)(ce_card_t *, int32_t *);
    void (*on_item_money)(ce_card_t *, int32_t *);
    void (*on_enemy_drop_pre)(ce_card_t *, int32_t *);
    void (*on_bomb_spent)(ce_card_t *);
    void (*on_shop_open)(ce_card_t *);
    uint32_t active_recharge;
    int  (*on_activate)(ce_card_t *);
    int  (*on_active_tick)(ce_card_t *, uint32_t);
    /* collect-2x 四断点（名/址/窗字节一手见 ../sdk.c 头表；AUDIT §Z）*/
    void (*on_item_frame)(ce_card_t *);                    /* BP_ce_item_frame   0x446ee3：每个正常游玩帧、道具池 body 前一次（借门卫位次，不读 CMP）*/
    void (*on_item_collect)(ce_card_t *, uint32_t, const void *); /* BP_ce_item_collect 0x44625f：type 分派前；type = *(uint32_t*)(item+0xc78)，副作用未发生 */
    void (*on_item_commit)(ce_card_t *);                   /* BP_ce_item_commit  0x44658a：本道具分值已入 SCORE、公共收尾前 */
    void (*on_shot_damage)(ce_card_t *, int32_t, const float *);  /* BP_ce_shot_damage 0x45f2a2：本帧本敌可计分命中；(dmg 钳后 ≥1, pos = 敌世界坐标 x,y) */
} ce_hooks_t;

int ce_give_card(uint32_t id, uint32_t mode, uint32_t notify);
int ce_destroy_card(ce_card_t *victim);
ce_card_t *ce_pick_random_card(ce_card_t *exclude);
void ce_log(const char *fmt, ...);
