/* bp_trace.c —— 测试用断点：记录每次 allocate_new_card(id, mode)。
 *
 * 挂在 0x411469（序言 `cmp [edi+0x28], 0x100`，7 字节），此时 push ebp; mov ebp,esp
 * 已执行：[ebp+8] = card_id，[ebp+0xc] = mode。只记日志、返回 1 照常执行。
 * 只在 patch-test 进栈时才被挂上；正式 patch 不声明这个断点。
 *
 * 战线 D 的验收钩子也在这里：第一次分配到**新 id**（≥57）时，替玩家「获得」它——
 * 调游戏自己的 CardCollection__mark_obtained_and_notify(id, 1)（__fastcall：ecx=id，edx=通知）。
 * 这会走到 0x418e04 的断点 → 影子 + side-car，并弹出「获得卡牌」通知。
 * 新卡还进不了商店（战线 E），没有别的路能触发 mark_obtained(新 id)，所以只能在这里模拟。
 * 该函数无栈参数、自己保存 ebx/esi/edi、不用浮点；从断点里调是安全的。
 *
 * 跑在游戏线程上下文里：不碰 x87（-mfpmath=sse），不调 thcrap API。
 */
#include <stdio.h>
#include "engine.h"
#include "card_expand.h"
#include "thcrap_bp.h"

#define MARK_OBTAINED_RVA 0x018de0          /* CardCollection__mark_obtained_and_notify */
typedef void (__fastcall *mark_obtained_t)(uint32_t id, uint32_t notify);

int __cdecl BP_ce_trace_alloc(x86_reg_t *regs, void *bp_info)
{
    (void)bp_info;
    const uint32_t *frame = (const uint32_t *)(uintptr_t)regs->ebp;
    uint32_t id = frame[2], mode = frame[3];
    if (id >= 57) {
        ce_log("trace: allocate_new_card(id=%u, mode=%u)  <- NEW ID", id, mode);
        if (id < CE_MAX_ROWS) {
            static uint8_t done[CE_MAX_ROWS];
            if (!done[id]) {
                done[id] = 1;
                ce_log("test: calling mark_obtained(id=%u, notify=1) to exercise the unlock path", id);
                ((mark_obtained_t)(uintptr_t)((uint8_t *)GetModuleHandleA(NULL) + MARK_OBTAINED_RVA))(id, 1);
            }
        }
    } else
        ce_log("trace: allocate_new_card(id=%u, mode=%u)", id, mode);
    return BP_EXEC_ORIGINAL;
}

/* 初始卡组钩子（patch-test 的 ce_test_deck，`0x407ee3`，盖住整条 `movzx eax, byte [eax+esi+0x5f608]`）。
 * 现在它只做两件事（起手卡组的**注入**搬到 `\mods\start_deck.js` + start_deck.c 了）：
 *   ① `esi == 0` 时给 `start_deck.c` 一次机会（显式、只改一次地改存档里的初手卡组）；
 *   ② 打一行诊断：引擎此刻真正读到的卡组 + 该机体的编成容量。
 * `regs->eax` 在这一格 = 「本机体初手第 0 格」地址（`scorefile + 0x5f608 + shot*0x10`）。*/
int __cdecl BP_ce_test_deck(x86_reg_t *regs, void *bp_info)
{
    (void)bp_info;
    if (regs->esi != 0) return BP_EXEC_ORIGINAL;       /* 只在每次 reset_cards 的第一格干活 */
    {
        int shot = *(const int32_t *)(uintptr_t)(regs->ebx + CE_DECK_SHOTTYPE_OFF);
        uint8_t *deck0 = (uint8_t *)(uintptr_t)regs->eax;

        /* ★ 显式起手卡组（\mods\start_deck.js）：**只改一次**，见 start_deck.c。放最前面，
         *   这样紧接着的诊断行读到的就是改过之后的存档（实跑一眼能看出这次到底改没改）。*/
        ce_start_deck_tick(deck0, shot);

        /* 每次 reset_cards 都打一行「引擎此刻真正读到」的卡组与编成容量 —— 用来区分
         * 「编成里改的没进存档」(`deck` 与编成设的不一致) 与「编成容量只有 N 格」(`cap` < 3)。*/
        {
            uint8_t *sf = *(uint8_t **)(uintptr_t)CE_ADDR_SCOREFILE_PTR;
            char buf[64];
            int len = 0;
            for (int i = 0; i < CE_DECK_CAP_HARD_MAX; ++i) {
                unsigned v = *(const uint8_t *)(uintptr_t)(deck0 + i + CE_TEST_DECK_SAVE_OFF);
                len += snprintf(buf + len, sizeof buf - (size_t)len, i ? ", %u" : "%u", v);
            }
            ce_log("test: reset_cards — shot %d cap %d deck [%s]", shot,
                   sf ? *(const int32_t *)(sf + shot * 4 + CE_DECK_CAP_OFF) : -1, buf);
        }
    }
    return BP_EXEC_ORIGINAL;
}
