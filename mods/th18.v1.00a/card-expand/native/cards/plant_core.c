/* 「种下去的植物」（向日葵 84 / 金盏花 85）共用的纯逻辑。见 plant_core.h。 */
#include "plant_core.h"

void plant_sow(plant_state_t *s, int32_t hp, uint32_t period, float px, float py, float pz)
{
    plant_state_t z = { 0, 0, 0, 0, 0, 0, 0, 0.0f, 0.0f, 0.0f, 0, 0, 0 };
    *s = z;
    s->hp = s->hp_max = hp;
    s->period = period;
    s->timer = period;
    s->x = px; s->y = py; s->z = pz;
}

plant_step_t plant_step(plant_state_t *s, int blocked)
{
    plant_step_t out = { 0, 0 };

    if (blocked > 0) {                    /* 挡下的每一发弹扣 1 点（引擎那边同一帧已经把它们变成点道具）*/
        s->blocked += (uint32_t)blocked;
        s->hp -= blocked;
    }
    if (s->hp <= 0) {                     /* 血尽：这一帧不产（先死后产不合逻辑）*/
        s->hp = 0;
        out.died = 1;
        return out;
    }
    s->frames++;
    if (s->timer > 0 && --s->timer == 0) {
        s->timer = s->period;
        s->harvests++;
        out.produce = 1;
    }
    return out;
}
