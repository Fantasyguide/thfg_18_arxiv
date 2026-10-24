/*
 * bgwarp_core.h —— 「局域背景扭曲」的可移植核心（TH18 v1.00a）
 *
 * 用途：把"自绘背景扭曲"这套东西**并进卡侧**（card-expand / 109 C4）。
 * 试验台（route_b.c）只是它的一个宿主；合并时**只需把本 .c/.h 搬进卡侧**，卡里按下面三行接：
 *
 *     bgwarp_install(card_log);                       // DLL 加载后一次（或首次激活时）
 *     bgwarp_set(1, 160.0f, 1.0f, 255.0f);            // C4 发动
 *     bgwarp_set(0, 0, 0, 0);                         // C4 结束 / on_delete
 *
 * 生命周期与一手 boss 扭曲对齐（一手：效果结束时引擎直接把 stage+0x3410 网格指针清零 ⇒ 瞬间消失）：
 *   C4 发动 ⇒ 开始出现；C4 结束 ⇒ 立刻消失。**不做淡出**（要淡出是自定行为，请自行登记）。
 *
 * 硬约束（血泪换来的，改代码时不要去掉）：
 *   ① 本实现里**所有间接调用**都必须先过内部 `rb_code()`（MEM_IMAGE + 可执行页）；
 *   ② 设备指针一变（Reset/重建）⇒ 必须清空所有纹理/面缓存。
 */
#ifndef BGWARP_CORE_H
#define BGWARP_CORE_H

#ifdef __cplusplus
extern "C" {
#endif

/* 日志回调：宿主提供（试验台写文件；卡侧可接自己的 log）。允许为 NULL。 */
typedef void (*bgwarp_log_fn)(const char *fmt, ...);

/* 安装（一次）。返回 1=成功；0=签名不符/失败（此时功能保持关闭，绝不影响游戏）。 */
int  bgwarp_install(bgwarp_log_fn logf);

/* 开/关 + 参数。on=0 ⇒ 立刻停止绘制（与一手"指针清零"同观感）。
 * radius：扭曲半径（世界单位；一手那个盘默认阈值 112 / 当前 192，实测反推 R≈180）
 * power ：位移倍数（1.0 = 复刻引擎的系数 32/8）
 * alpha ：圆内不透明度（0..255） */
void bgwarp_set(int on, float radius, float power, float alpha);

/* 可选微调（不调就用默认值）。 */
void bgwarp_tune(int grid_n, float fade, int overlap, float blend);

/* 诊断/试验模式：0 关 / 1 M1 平移 / 2 扭曲网格 / 3 光晕[屏幕UV] / 4 平面网格 / 5 光晕[整图UV]。
 * 生产用不到；试验台用它按键切换。 */
void bgwarp_set_mode(int mode);
int  bgwarp_mode(void);

/* 试验台专用旋钮（生产不需要）：采样源面 / 第二份 / M1 平移量 / 第二份偏移 / 半径跟敌方。 */
void bgwarp_set_test(int surf, int two, float shift, float two_off, int radius_auto);

/* 宿主的每帧回调（在"确认在舞台里"之后调用；试验台用它轮询热键）。允许为 NULL。 */
void bgwarp_set_tick(void (*fn)(void));

/* 还原层 6 桩（可选；仅试验台/禁用时用）。 */
void bgwarp_restore(void);

#ifdef __cplusplus
}
#endif
#endif /* BGWARP_CORE_H */
