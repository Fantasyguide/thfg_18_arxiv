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

/* 初始卡组钩子（patch-test 的 ce_test_deck，0x407ee3，盖住整条 movzx eax, byte [eax+esi+0x5f608]）。
 * 语义复刻：eax = 那一格的 id。
 *
 * dev 卡组（cards_dev.js start_deck）把槽值**换掉**（只改 eax、不写内存——2026-09-08 写容量域
 * [0x5f678] 触发开局卡死，已回滚）：esi < dev 张数的槽无条件 = dev 卡组 id（档里预装默认卡被顶掉，
 * 引擎 alloc 的是 dev 卡）；reset_cards 循环按容量域走（编成档位 1–3，引擎封顶 3 = CE_DECK_CAP_MAX，
 * 0x4449d4 无 3→4 分支），容量不足时只发前几张——日志可读出发了几张。
 * 返回 0 = 跳过原指令（eax 已经是结果）。原 movzx 不设 flags；紧接着 push eax; call —— 无人读 flags。
 * 无 dev 卡组（cards_dev.js 缺失）= 原语义放行。 */
int __cdecl BP_ce_test_deck(x86_reg_t *regs, void *bp_info)
{
    (void)bp_info;
    const unsigned n = ce_dev_deck_count();
    if (n == 0) return BP_EXEC_ORIGINAL;
    uint32_t id = *(const uint8_t *)(uintptr_t)(regs->eax + regs->esi + CE_TEST_DECK_SAVE_OFF);
    if (regs->esi < n) {
        uint32_t nid = ce_dev_deck_id(regs->esi);
        if (id != nid)
            ce_log("test: deck slot %u: %u -> dev id %u", regs->esi, id, nid);
        regs->eax = nid;
        return 0;
    }
    regs->eax = id;
    return 0;
}
