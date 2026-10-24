/* 黑桃 J —— 移动速度 +10%。
 * player+0x477ec 是每帧的移速倍率：Player tick 末尾复位 1.0，移动函数读它。
 * AbilityManager tick（优先级 0x16）先于 Player tick（0x17），所以在 on_tick_2 里乘才生效（on_tick 在复位之前，白写）。
 * BombMarisa 用同一字段（0.5），多张同类卡相乘叠加。 */
#include "sdk.h"
#include "royal.h"   /* 皇家同花顺：五张黑桃共用 ctor */

/* ★★ 2026-10-24（owner：单装黑桃J 自机闪黄 / 黑桃10 不闪）—— 一手根因：
 *   引擎 `th18:0x45c6c2` 读 `0x477ec`（移速倍率），**> 阈值且 `帧%8 < 4` 时**
 *   就执行 `0x45c6e6 movl $0xffffff00,0x53c(%edi)` = 把自机本体 `color_2` 刷成**黄色**
 *   —— 那是 TH18 自带的「加速中」视觉提示（同段还管决死蓝 / 红），**不是我们染色写错**。
 *   ⇒ 本卡的加速会必然触发它。要保留 +10% 又不闪，就在**引擎之后**把 `color_2` 顶回白：
 *     `on_item_frame`（ItemManager tick，优先级 0x1e）晚于 `Player::on_tick`(0x17)，
 *     是"本帧最后一笔"（同 106 判定点用的相位）。*/
static void on_item_frame(ce_card_t *c)
{
    uint8_t *p = CE_PLAYER();
    (void)c;
    if (!p) return;
    /* ★ 两道条件，缺一不动：
     *   ① **倍率确实 > 1.0**（= 黄闪的前提就是它，见 0x45c6c2）—— 我们没抬倍率时一个字节都不碰，
     *      所以**不可能覆盖撞弹无敌的闪**（那份是 `0xff0000ff` 蓝 / `0xffff0000` 红，一手里根本没有黄色路径）；
     *   ② 当前值**正好是** `0xffffff00`（引擎的加速色）。*/
    if (*(volatile float *)(p + CE_PLAYER_SPEED_MULT) > 1.0f &&
        *(volatile uint32_t *)(p + CE_PLAYER_VM + CE_ANM_VM_COLOR2) == 0xffffff00u)
        *(volatile uint32_t *)(p + CE_PLAYER_VM + CE_ANM_VM_COLOR2) = 0xffffffffu;
}

static int on_tick_2(ce_card_t *c)
{
    ce_royal_tick(c);
    /* ★ 诊断：owner 报黑桃J 玩家闪黄、黑桃10 不闪，关掉 *=1.1 后仍闪 ⇒ 只观察 */
    { uint8_t *pp = CE_PLAYER(); static unsigned s_n;
      if (pp && (++s_n % 60u) == 0u) {
          const uint32_t c1 = *(volatile uint32_t *)(pp + CE_PLAYER_VM + CE_ANM_VM_COLOR1);
          const uint32_t c2 = *(volatile uint32_t *)(pp + CE_PLAYER_VM + CE_ANM_VM_COLOR2);
          ce_log_key("sj: PLAYERCOL c1=%08x c2=%08x invuln_prev=%d invuln_cur=%d", c1, c2,
                           *(int32_t *)(pp + CE_PLAYER_INVULN_TIMER),
                           *(int32_t *)(pp + CE_PLAYER_INVULN_TIMER + 4));
      } }
    uint8_t *p = CE_PLAYER();
    /* ★★ 2026-10-24（owner：单装黑桃J 玩家本体闪黄，黑桃10 不闪；两者唯一差异就是这一行）：
     *   原来是 `*= 1.1f`。若引擎的"每帧复位 1.0"发生在我们写之后/被别的乘者叠加，
     *   就会变成 1.1^n 指数爆炸 ⇒ 位置 Inf/NaN ⇒ 自机 VM 画乱像素 = "闪黄" ✗。
     *   ⇒ 改成**赋值**（每帧写 1.1），无论复位顺序如何都只 +10%，且多张同类卡不再相乘叠加。
     *   （`Player::move` 最后一步才把它当倍率乘，所以每帧显式写 1.1 与"乘一次"等价。）*/
    /* ★★ 2026-10-24：恢复写 1.1，并把「无敌三格 + 自机坐标」每 60 帧打一行。
     *   目的：确认"写移速 ⇒ 每帧 1 帧无敌"是不是因为**加速后每帧都在擦/撞弹**（位置抖 + inv 反复 1）。*/
    if (p) {
        static unsigned s_n2;
        *(float *)(p + CE_PLAYER_SPEED_MULT) = 1.1f;
        if ((++s_n2 % 60u) == 0u) {
            const int32_t ip = *(int32_t *)(p + CE_PLAYER_INVULN_TIMER);
            const int32_t ic = *(int32_t *)(p + CE_PLAYER_INVULN_TIMER + 4);
            const float   i_f = *(volatile float *)(p + CE_PLAYER_INVULN_TIMER + 8);
            const float   px = *(volatile float *)(p + CE_PLAYER_X);
            const float   py = *(volatile float *)(p + CE_PLAYER_Y);
            ce_log_key("sj: SPEEDW pos=(%.1f,%.1f) inv=%d/%d/%.2f mult=%.3f",
                       (double)px, (double)py, ip, ic, (double)i_f,
                       (double)*(volatile float *)(p + CE_PLAYER_SPEED_MULT));
        }
    }
    return 0;
}

/* on_delete：删卡（倒吊人 / 原版空白卡）时收掉皇家同花顺演出的文件级倒计时（见 royal.c 的 ce_royal_dismiss）。
 * 移动速度那一项不用还：`player+0x477ec` 是**每帧**倍率，Player tick 末尾自己复位 1.0。*/
CE_CARD(59, .ctor = ce_royal_flush_ctor, .on_tick_2 = on_tick_2, .on_item_frame = on_item_frame, .on_delete = ce_royal_dismiss);
