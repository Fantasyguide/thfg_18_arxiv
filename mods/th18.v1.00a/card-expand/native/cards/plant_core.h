/* plant_core.h —— 「种下去的植物」共用的纯逻辑：生命 / 挡弹扣血 / 定期产道具。
 * 向日葵（84）与金盏花（85）逐字相同，所以按 SDK.md §5 的约定（出现第二次就提炼）合并到这里；
 * 每张卡自己的数值（血量 / 周期 / 产什么 / 多少个 / 火力成本）由卡在 `plant_def_t` 里给（`plant.h`）。
 * 不碰引擎、不产随机数（产道具的扇形角度是下标算出来的定值 ⇒ replay 安全），主机 `make test-host` 直接测。 */
#pragma once
#include <stdint.h>

#define PLANT_ITEM_SPEED   2.2f           /* 原生 main-drop 的速率 */
#define PLANT_ITEM_UP      (-1.5707964f)  /* −π/2：原生撒落的上抛角 */
#define PLANT_ITEM_SPREAD  0.9f           /* 扇形半角（弧度）：一次撒 25 个也不会叠成一坨 */
#define PLANT_BAR_DY       46.0f          /* 血条中心相对植株中心的 y（植株下缘约 +36）*/
#define PLANT_BAR_W        40.0f
#define PLANT_BAR_H        4.0f

typedef struct {
    int32_t  hp, hp_max;    /* 当前 / 上限（血条按比例画）*/
    uint32_t period;        /* 产出周期（帧）*/
    uint32_t frames;        /* 种下以来的帧数 */
    uint32_t blocked;       /* 累计挡弹数 */
    uint32_t harvests;      /* 已产过几轮 */
    uint32_t timer;         /* 距下一轮还有几帧 */
    float    x, y, z;       /* 钉在种下时的自机位置，之后不动 */
    uint32_t anm_id, bar_bg_id, bar_id;   /* ANM ids（引擎侧，core 不碰）*/
} plant_state_t;

typedef struct {
    int produce;    /* 本帧产一轮：调用方撒 def->items 个道具 */
    int died;       /* 本帧血尽 */
} plant_step_t;

void         plant_sow(plant_state_t *s, int32_t hp, uint32_t period, float px, float py, float pz);
plant_step_t plant_step(plant_state_t *s, int blocked);   /* 每帧：扣血 → 计帧 → 产出计时 */

/* 第 i 个道具（0 ≤ i < n）的抛出角：以 −π/2（正上方）为中心的等分扇形，两端各 PLANT_ITEM_SPREAD。
 * n ≤ 1 时就是正上方。**static inline 的理由同 fl_bob_dy**：i386 ABI 返回 float 走 st0（flds），
 * 内联掉才守得住 `make dllx87` 的「零 x87」。*/
static inline float plant_item_angle_at(int i, int n)
{
    if (n <= 1) return PLANT_ITEM_UP;
    if (i < 0) i = 0;
    if (i >= n) i = n - 1;
    return PLANT_ITEM_UP + PLANT_ITEM_SPREAD * (2.0f * (float)i / (float)(n - 1) - 1.0f);
}
