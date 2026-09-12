/* 皇家同花顺 —— 黑桃 10/J/Q/K/A（58–62）共用的 ctor：买到第五张时触发隐藏效果。 */
#pragma once
#include "sdk.h"
int ce_royal_flush_ctor(ce_card_t *c);
void ce_royal_tick(ce_card_t *c);      /* 每帧（on_tick_2）：演出里延时放的音效 */
/* 删卡收尾（虚表 +0x50，五张黑桃共用的 on_delete）：演出还在倒计时的那张黑桃被删掉时，把文件级 static 的
 * 倒计时清掉 —— 理由见 royal.c 的同名函数（不清就会「攒着一响」，等这张卡被重新买到时在局中冒出来）。 */
void ce_royal_dismiss(ce_card_t *c);
