/* ice_cream_core.h —— 冰淇淋（id 90）的纯逻辑：**生前火力/符卡的快照**与「miss 之后该写什么」。
 * 不碰引擎 ⇒ 主机 `make test-host` 能直接测。
 *
 * ## 一手：TH18 一次 miss 到底做了什么（decompile 结论，证据在 AUDIT §BE 详录 #214）
 * `Player__on_tick__body` 的死亡分支（`th18:0x45c133`–`0x45c154`）：
 * ```
 * 0x45c135  call 0x45d090   ; Player__commit_death_and_enter_state2
 *                           ;   ⇒ CURRENT_BOMBS = clamp(PER_LIFE_BOMBS = 3, 0, MAX_BOMBS)（0x45d1cb–0x45d1e8）
 *                           ;   ⇒ CURRENT_LIVES -= 1（0x45d19b）、状态 → 2、刷 HUD
 * 0x45c140  cmp [edi+0x638], 3      ; 决死窗口计数器还等于 3（= 这一帧刚死）
 * 0x45c149  push [0x4ccd40]         ; 参数 = 一档火力（POWER_PER_LEVEL）
 * 0x45c154  call 0x457480           ; GlobalsInner__spend_power ⇒ **CURRENT_POWER -= 一档**（钳到一档）
 * ```
 * 而卡的回调 `+0x14`（`on_death_frame2`）在 **`0x45c255`** 被调 —— **正好在扣完火力之后** ⇒
 * 卡在这时把两个值写回去就是「不损失火力 + 符卡 += 3」。
 *
 * ## 本卡的两条口径
 * ① **只在「活着的正常态」采快照**（`player+0x476ac == 1`）：死亡帧的结算可能发生在我们的 tick
 *    之前/之后，若在死亡态里也刷快照，就可能把**已经被扣过**的值当成"生前值"写回去（= 反而没保住）。
 * ② **符卡 = 生前 + 3**（钳 `MAX_BOMBS`），不是"重设成 6" —— owner 原话「bomb 直接 +3」。 */
#pragma once
#include <stdint.h>

#define IC_BOMB_BONUS 3      /* owner 逐字：miss 时符卡 +3（正常 miss 是重设成 3）*/

typedef struct {
    int32_t power;       /* 生前火力（CURRENT_POWER）*/
    int32_t bombs;       /* 生前符卡（CURRENT_BOMBS）*/
    uint8_t have;        /* 采到过快照没有 */
    uint8_t miss;        /* 本帧真死（+0x0c 置、+0x14 消费）*/
} ic_state_t;

void ic_reset(ic_state_t *s);
/* 每帧一次：`alive_normal` = 玩家处于正常态（`+0x476ac == 1`）。返回 1 = 这一帧刷新了快照。*/
int  ic_cache(ic_state_t *s, int alive_normal, int32_t power, int32_t bombs);
/* 真死那一刻（`+0x0c`）：只置位（死亡栈上不做任何写）。*/
void ic_note_miss(ic_state_t *s);
/* 死亡结算之后（`+0x14`）：算出该写回去的两个值。返回 1 = 应该写（调用方写完后调 HUD 刷新）。
 * ⚠ 一次性：调用完就把 `miss` 清掉（同一次 miss 只写一遍）。*/
int  ic_apply_after_miss(ic_state_t *s, int32_t max_bombs,
                         int32_t *out_power, int32_t *out_bombs);
