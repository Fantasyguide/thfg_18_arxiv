/* trance_bgm.h —— 灵界版 BGM（卡 113）的引擎侧接线（`cards/trance_bgm.c`）。
 *
 * 路线 A（用户 2026-09-13 拍板）＝ **不碰 `thbgm.dat` / `th18.dat`**，在 DLL 侧用引擎自己的
 * 内存流构造成器再开**第二路 `CStreamingSound`**，与主 BGM 做交叉淡化。
 * 素材 = `tools/make_trance_bgm.py` 生成的 **TH18 自己的曲子 + naive 低音放大** 的 WAV
 * （`th18_NNb.wav`，TH13 的命名法），放在 **DLL 目录下的 `bgm/`**。
 * 方案、TH13 一侧的一手机制、以及三条路线的代价对照见
 * `engine/_shared/th13-th18-trance-bgm-research.md`；实装与失败路径见 `AUDIT.md` §CO。
 */
#pragma once
#include <stdint.h>

/* 卡侧还要用到 core 的旋钮（`TRANCE_BGM_XFADE_FRAMES`）打日志 ⇒ 这里转出它的头。
 * core 全是纯逻辑与常量，带进来不会拖任何引擎依赖。*/
#include "trance_bgm_core.h"

/* 编译期总开关：0 = 整段接管都不做（卡侧只剩"什么都不变"），用于实跑首日出事时一键回退。 */
/* ⚠ 历史：2026-09-13 实跑首日「进灵界后几秒闪退」⇒ 曾置 0 做二分。日志证明它当时**根本没跑**
 *   （`bgm … started 0`）⇒ **闪退不是它**（`AUDIT` §CA-5b 第 6 行）。
 * ★ 2026-09-13 第二轮：素材（`tools/make_trance_bgm.py` 生成的真音频）到位、接线重做之后**打开**。
 *   出问题时的回退顺序：① 这一行改 0（整段不接管，其余功能不受影响）
 *   ② 把 `<DLL 目录>/bgm/` 改名（素材缺失 ⇒ 不接管，主 BGM 照常）。*/
#define TRANCE_BGM_ENABLE       1   /* 保留为语义文档；**实际开关 = 素材存在与否**（找不到 wav ⇒ 不接管）*/

/* 引擎面自检：0 = 可用。缺任何一个全局（DirectSound 没起 / 引擎还没建 BGM 流）就不可用。 */
int  ce_trance_bgm_available(void);

/* 进灵界：建第二路 + 起播 + 从"全主 BGM"开始淡化。
 * 返回 1 = 已接管（`tick` / `release` 必须成对跟上）；0 = 没接管（不产生任何副作用）。
 * ⚠ **幂等**：已在接管中再调 = 原样返回 1，不会重建。
 * ⚠ **绝不在死亡栈上调用**（`+0x0c` 回调里不调）：调用点只在帧点（`on_item_frame`）。*/
int  ce_trance_bgm_start(void);

/* 每帧：① 补 buffer（引擎自己的补 buffer 线程只认它那一条 BGM）② 推进淡化 ③ 镜像暂停。
 * 未接管时是空操作。*/
void ce_trance_bgm_tick(void);

/* **只**镜像引擎的暂停/播放状态（停/起我们的第二路），不推进淡化、不补 buffer。
 * 为什么单独开一个口子：`tick` 只在游玩帧跑，而"暂停时引擎把自己的 BGM 停掉"这件事
 * 恰好发生在游玩帧停摆的时候 ⇒ 从**绘制路径**（`on_draw`，暂停菜单照画）也能调一次。
 * 幂等、幂等、无副作用（未接管时直接返回）。*/
void ce_trance_bgm_pause_poll(void);

/* 出灵界 / 删卡 / 局末。`immediate` = 1 ⇒ 立刻停+释放（**删卡那一路必须用 1**：
 * 商店里 tick 全停，渐变永远走不完 —— `METHOD.md` 铁律二）。幂等。*/
void ce_trance_bgm_release(int immediate);

/* 是否正在接管（诊断/日志用）。 */
int  ce_trance_bgm_active(void);

/* 一行诊断串（状态 / 统计 / 失败计数），给卡的汇总日志用。 */
const char *ce_trance_bgm_state_name(void);
