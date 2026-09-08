/* ouroboros_core.h —— 衔尾蛇（id 78）的纯逻辑：miss 计数 → 层数 → 伤害倍率。
 * 纯 C11、无引擎依赖，主机单测 tests/test_ouroboros_core.c。
 *
 * 规则（用户定）：每 miss X 次，自机弹伤害永久 +OB_STEP_PERMILLE（加法叠），然后 X += 1。X 初值 1。
 * ⇒ 第 N 层要累计 N(N+1)/2 次 miss：1 / 3 / 6 / 10 / 15 …；残机有限，一局现实里到 2–3 层、极限 4 层。
 * 「永久」= 一整局（run）之内；局末 on_run_reset 清零。无随机、无浮点 ⇒ replay 天然安全。 */
#pragma once
#include <stdint.h>

#define OB_STEP_PERMILLE  200      /* 每层 +20 个百分点（加法：1.2 / 1.4 / 1.6 …）*/
#define OB_BASE_PERMILLE  1000     /* = CE_DMG_UNIT，此处独立定义以保持本文件不依赖 SDK */
#define OB_MAX_STACKS     9        /* HUD 上只有一位数字；到顶就不再涨（现实里够不着）*/

typedef struct {
    uint32_t stacks;      /* 已叠了几层 */
    uint32_t need;        /* 当前的 X：再 miss 这么多次升下一层 */
    uint32_t progress;    /* 本层已经 miss 了几次 */
    uint32_t misses;      /* 统计：本局总 miss 次数 */
} ob_state_t;

void ob_reset(ob_state_t *s);         /* stacks = 0、need = 1、progress = 0 */
/* 记一次 miss。返回 1 = 这一次让层数 +1（调用方去改倍率 / 放音效）。*/
int  ob_on_miss(ob_state_t *s);
int  ob_permille(const ob_state_t *s); /* OB_BASE + stacks × OB_STEP */
uint32_t ob_remaining(const ob_state_t *s);  /* 距下一层还差几次 miss */
