/* card_expand.h —— th18_card_expand.dll 内部共用的声明。
 *
 * 这个 DLL 是 card-expand mod 的唯一 DLL。职责按文件分：
 *   dll_main.c    入口 / 日志文件 / thcrap API 解析 / 开机自检①（零售表签名）
 *   selfcheck.c   开机自检②：BP_ce_gate 里填表（+跳转表）+ 回读站点 + 写结论
 *   unlocked.c    战线 D：unlocked_cards 影子数组 + side-car；三个断点
 *   text.c        战线 E 第一块：id≥57 的文案重定向到 DLL 缓冲（三个断点）
 *   cards_def.c   一张卡的定义：校验 / 表行编码 / 商店容量（纯逻辑，主机单测）
 *   cards.c       战线 E 第 10 段：从 thcrap 栈的 th18/cards.js 装新卡，产出注册表
 *   menu.c        战线 E 第二块：顺序表重排 + 图鉴条目数 + zAbilityMenu 站点核对
 *   bp_trace.c    测试用断点：记录每次 allocate_new_card(id, mode)，新 id 顺手 mark_obtained（只在 patch-test 进栈时挂）
 *   shop_core.c   商店走两遍的状态机（纯逻辑，主机单测）
 *   shop.c        商店走两遍：成交断点 + GameThread 重开断点（AUDIT §P）
 *   sound.c       音效表扩容：SoundManager::init 门里填语音 blob + 新行配置 + I1/I2 自检（AUDIT §Q）
 *   （后续）       数据激活门、新卡注册 …
 */
#pragma once
#include <windows.h>
#include <stdint.h>
#include "sites_gen.h"
#include "cards_def.h"

/* 日志：一律写自己的文件 th18_card_expand.log（游戏目录，写不了退到 %TEMP%）。
 * ce_verdict 额外把那一行镜像进 thcrap 的日志——只镜像结论，不刷屏。*/
void ce_log(const char *fmt, ...);
void ce_verdict(const char *fmt, ...);

/* thcrap 导出（GetProcAddress 取得，可能为 NULL） */
extern uintptr_t (*ce_func_get)(const char *);

/* 日志所在目录（游戏 exe 目录，尾带反斜杠）—— side-car 的兜底位置 */
void ce_log_dir(char *out, size_t cap);

/* 开机自检②（由 BP_ce_gate 调一次）。返回 1 = 通过 */
int ce_selfcheck(uint8_t *module_base);

/* 战线 D（unlocked.c）：找影子 codecave（NULL = 不在栈里）；核对 9 处读 + 3 个断点 */
uint8_t *ce_unlock_init(uint8_t *module_base);
int ce_unlock_check(uint8_t *module_base);
/* 战线 E 第一块（text.c）：核对三个文案重定向断点；装载器覆盖一张新卡的文案 */
int  ce_text_check(uint8_t *module_base);
void ce_text_set(uint32_t id, const char *name, const char (*desc)[CE_CARD_TEXT_LINE], unsigned ndesc);
/* 战线 E 第 10 段（cards.c）：从 thcrap 栈里的 th18/cards.js 装新卡（表行 + 文案 + 注册表）；返回 1 = 通过 */
int      ce_cards_load(uint8_t *module_base, uint8_t *cave, unsigned rows);
unsigned ce_new_card_count(void);
uint32_t ce_new_card_id(unsigned i);
int      ce_new_card_initial_unlocked(unsigned i);
/* 开发配置 th18/cards_dev.js（只在 _test 里）：起手卡组与钩子追踪 */
unsigned ce_dev_deck_count(void);
/* 门禁失败后必须调：把 dev 起手卡组关掉。新卡 id 此时已被 restore_alloc_bound 禁掉，
 * 再往空槽里塞它们只会让 allocate_new_card 返回 -1 —— 空槽本来会分配 NullInf 卡（id 56），
 * 于是 num_total_cards 归零，卡组编成读到没填过的 +0xfec 槽，按 Z 空指针崩溃（AUDIT §AG）。*/
void     ce_dev_deck_disable(void);
uint32_t ce_dev_deck_id(unsigned i);
int      ce_dev_trace(void);
/* 行为 SDK（sdk.c）：门里核对 + 对账；返回 1 = 通过 */
int      ce_sdk_setup(uint8_t *module_base, int trace);
/* 战线 E 第二块（menu.c）：顺序表重排、图鉴条目数、站点核对 */
int ce_menu_setup(uint8_t *module_base);
/* 商店走两遍（shop.c ← shop_core.c 主机单测）：门里核对两个断点；返回 1 = 通过 */
int ce_shop_setup(uint8_t *module_base);
