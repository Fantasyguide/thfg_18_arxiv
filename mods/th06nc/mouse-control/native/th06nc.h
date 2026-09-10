/* th06nc.h —— 本 mod 用到的全部 TH06NC 地址与偏移。
 *
 * 每一条都能回指 renkolab 的一手文档，见 TARGET.md 的对照表。
 * 样本 MD5 b0cc28ec904e9efce2439ad857c938b5；DLL 启动时会校验签名字节。 */
#ifndef TH06NC_H
#define TH06NC_H

#include <stdint.h>

#define TH06NC_IMAGEBASE  0x140000000ULL

/* --- RVA（相对 imagebase，运行时加实际基址）--- */
/* engine/player/th06nc/01 §2：玩家每帧 tick，chain 回调 */
#define RVA_PLAYER_ONUPDATE   0x68820
/* engine/menu/th06nc/02 §8：总状态机每帧闩锁的逻辑按钮位图（当前帧 / 上一帧） */
#define RVA_INPUT_HELD        0xA6EC60
#define RVA_INPUT_PREV        0xA6EC64
/* engine/player/th06nc/01 §2.5：玩家对象是全局 */
#define RVA_G_PLAYER          0x4FF3A0
/* engine/player/th06nc/01 §2.11：自机可动区域，Player__OnUpdate 就用它钳位。
 * 4 个 f32：topLeft.x / topLeft.y / size.x / size.y。运行时才填，别读静态值。 */
#define RVA_MOVEMENT_AREA     0x4FF0E0

/* --- Player 结构体偏移（engine/player/th06nc/01 §2.5 / §2.7）--- */
#define OFF_POSITION_X        0x7730   /* f32 x, +4 y, +8 z，关卡区局部坐标 */
#define OFF_POSITION_Y        0x7734
#define OFF_HITBOX_RADIUS     0x774C   /* f32 = 1.25 */
#define OFF_IS_FOCUSED        0x785C   /* bool */
#define OFF_PLAYER_STATE      0x7898   /* 0 ALIVE / 1 SPAWNING / 2 DEAD / 3 INVULNERABLE */
#define OFF_CHAR_DATA         0x7860   /* CharacterData：+0 正交速度 +4 低速速度 */

/* --- 逻辑按钮位（engine/menu/th06nc/02 §8）--- */
#define IN_SHOOT  0x01
#define IN_BOMB   0x02
#define IN_FOCUS  0x04
#define IN_UP     0x10
#define IN_DOWN   0x20
#define IN_LEFT   0x40
#define IN_RIGHT  0x80
#define IN_DIR_MASK (IN_UP | IN_DOWN | IN_LEFT | IN_RIGHT)

/* --- 关卡区几何 ---
 * positionCenter 的坐标系已一手确认与 TH06 相同（出生点硬编码 (192,384)
 * = arcadeRegionSize.x/2, .y-64；可动区实测 (8,16)+(368,416) 也与 TH06 逐值相同）。
 * 可动范围**不硬编码**，每帧从 RVA_MOVEMENT_AREA 读游戏自己的值。
 *
 * 屏幕布局则**与 TH06 不同**，由两次实测标定确认（1280x720 与 1920x1080）：
 *   · 虚拟屏高度固定 480，宽度随窗口宽高比扩展（**不做 4:3 信箱**）
 *   · 缩放 s = 客户区高 / 480      （实测 1.5000 / 2.2500，两组都精确命中）
 *   · 关卡区 384x448 **水平居中**  （实测左边界虚拟 235.33 / 235.11，居中值 234.67）
 *   · 顶边 y = 16（与 TH06 相同）  （实测 16.67 / 14.22）
 * TH06 是左边界固定 x=32、右侧留 HUD；照抄它会偏右约 200px。详见 TARGET.md §6。 */
#define PLAYFIELD_W       384.0f     /* arcadeRegionSize，也是角标定的第二个点 */
#define PLAYFIELD_H       448.0f
#define DEFAULT_ORIGIN_X  (-1.0f)    /* <0 = 水平居中；填 >=0 则当作虚拟单位的左边界 */
#define DEFAULT_ORIGIN_Y   16.0f
#define VIRT_W            640.0f
#define VIRT_H            480.0f

/* Player__OnUpdate 入口的 13 字节，全部位置无关（TARGET.md §2 逐字节核对）：
 *   48 8b c4              MOV RAX,RSP
 *   57                    PUSH RDI
 *   41 54                 PUSH R12
 *   48 81 ec d8 00 00 00  SUB RSP,0xd8 */
#define PROLOGUE_LEN 13
static const uint8_t k_expected_prologue[PROLOGUE_LEN] = {
    0x48, 0x8b, 0xc4, 0x57, 0x41, 0x54, 0x48, 0x81, 0xec, 0xd8, 0x00, 0x00, 0x00
};

#endif
