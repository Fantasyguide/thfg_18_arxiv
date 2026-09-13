/* 反转牌 —— 主动卡：C 键发动，场上所有子弹速度方向反向。充能 3600 帧（60 s）。
 *
 * 子弹池：BULLET_MANAGER 里 2000 张 zBullet，扫法照 cancel_all（0x4297a0）：起点 mgr+0xec、stride 0xfa0，
 * 状态（+0xf68）0 = 空槽、3 = 消弹中都跳过。普通子弹每帧 pos += velocity；带 ex 状态的会从 speed/angle
 * 重算 velocity——所以两边都翻：velocity 取反，angle += π（归一到 (-π, π]，虽然 tick 也会归一）。
 * 不动激光（另一套管理器）。瞬发：on_activate 返回 0，SDK 直接进收尾。 */
#include "sdk.h"

#define PI_F 3.14159265f

static int on_activate(ce_card_t *c)
{
    (void)c;
    uint8_t *bm = CE_BULLET_MGR();
    if (!bm) return 0;
    unsigned n = 0;
    uint8_t *b = bm + CE_BM_BULLETS;
    for (unsigned i = 0; i < CE_BM_BULLET_COUNT; ++i, b += CE_BULLET_STRIDE) {
        uint16_t st = *(uint16_t *)(b + CE_BULLET_STATE);
        if (st == 0 || st == 3) continue;
        float *v = (float *)(b + CE_BULLET_VELOCITY);
        v[0] = -v[0]; v[1] = -v[1];
        float *ang = (float *)(b + CE_BULLET_ANGLE);
        float a = *ang + PI_F;
        if (a > PI_F) a -= 2.0f * PI_F;
        *ang = a;
        ++n;
    }
    ce_play_sound(0x4d, ce_player_x());                            /* 0x4d = Tenshi 发动音 */
    /* 亮牌：ability.anm 追加的 script68（assets/ability/scripts/68_reverse_flash.anm.txt），卡图副本 sprite109，
     * 场地中央（脚本 pos(0,224,0)：ECL y 从区域顶部起算）绕 Y 轴转一圈后自灭。层 20：type(8) 走 D3D WORLD 矩阵，不加相机 2 的区域原点偏移，放世界层会偏位；
     * 原生的 type(8) 全在层 20 / 2 / 6（engine/anm/th18/01-vm-instantiate.md §3）。脚本里的 layer() 会覆盖这个参数。*/
    uint32_t fx = ce_anm_spawn(CE_ABILITY_ANM(), CE_ANM_ABILITY_SCRIPT_REVERSE_FLASH, 20);
    ce_log("reverse: %u bullets reversed; flash anm id %08x", n, fx);
    return 0;
}

CE_CARD(64, .active_recharge = 3600, .on_activate = on_activate);
