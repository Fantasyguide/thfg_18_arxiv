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
    /* 删对象（局末 recount / 倒吊人 ce_destroy_card）时、SDK 释放私有状态之前调：卡在这里删自己起的 VM 等外部资源。
     * on_run_reset 只覆盖局末那条路，局中被摧毁只走这里。2026-09-08 股市（80）第一个用户。 */
    void (*on_delete)(ce_card_t *);
    uint32_t active_recharge;
    int  (*on_activate)(ce_card_t *);
    int  (*on_active_tick)(ce_card_t *, uint32_t);
    /* 自机弹伤害倍率（千分比，1000 = ×1）。非 0 = 这张卡参与伤害层：SDK 在绑定时登记这个初值，
     * 之后卡可以随时 ce_set_bullet_damage_permille() 改。所有参与者的值连乘成聚合倍率，
     * 由其中一张（id 最小的）在 +0x28 里对 bullet+0x9c 就地乘一次、并在 +0x2c 里抬每帧伤害上限。
     * ★ 别再自己写 bullet+0x9c：那样会各自取整、还得各自记得抬上限。边界：只管自机弹，
     * 炸弹与 ce_damage_rect 的伤害源不在内。AUDIT §AA。*/
    int  bullet_damage_permille;
} ce_hooks_t;

/* 改这张卡当前的自机弹伤害倍率（千分比）。只在值真的变了的时候调（古锭刀在终符沿、衔尾蛇在叠层时）。*/
void ce_set_bullet_damage_permille(ce_card_t *c, int permille);

int ce_give_card(uint32_t id, uint32_t mode, uint32_t notify);
int ce_destroy_card(ce_card_t *victim);
ce_card_t *ce_pick_random_card(ce_card_t *exclude);
void ce_log(const char *fmt, ...);
