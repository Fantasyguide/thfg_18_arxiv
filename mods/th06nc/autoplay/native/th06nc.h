/* th06nc.h —— autoplay 用到的全部 TH06NC 地址与偏移。
 *
 * 每一条都能回指 renkolab 的一手文档，见 TARGET.md 的对照表；
 * check.py 会把这里的值与 games/th06nc/symbols.json 和 tooling/ghidra/th06nc_*_spec.py 对账。
 * 样本 MD5 b0cc28ec904e9efce2439ad857c938b5；DLL 启动时会校验签名字节。 */
#ifndef TH06NC_H
#define TH06NC_H

#include <stdint.h>

#define TH06NC_IMAGEBASE  0x140000000ULL

/* --- RVA（相对 imagebase，运行时加实际基址）--- */
/* engine/player/th06nc/01 §2：玩家每帧 tick，chain 回调；hook 点 */
#define RVA_PLAYER_ONUPDATE   0x68820
/* engine/menu/th06nc/02 §8：逻辑按钮位图（当前帧 / 上一帧） */
#define RVA_INPUT_HELD        0xA6EC60
#define RVA_INPUT_PREV        0xA6EC64
/* engine/player/th06nc/01 §2.5 */
#define RVA_G_PLAYER          0x4FF3A0
/* engine/player/th06nc/01 §2.11：可动区 4 个 f32 topLeft.x/y size.x/y，运行时才填 */
#define RVA_MOVEMENT_AREA     0x4FF0E0
/* engine/bullet/th06nc/01 §1：BulletManager 开头是 ZunTimer（+0 prev, +4 current）→ 帧号 */
#define RVA_BULLET_MANAGER    0x3EC2A0
#define RVA_G_BULLETS         0x3EC2A8      /* bullets[640]，步长 0x620 */
#define RVA_G_BULLET_COUNT    0x4E12A8      /* i32，每帧重算的活跃弹数 */
/* engine/enemy/th06nc/01 §1 / §6.3：g_Enemies = EnemyManager + 8 */
#define RVA_G_ENEMIES         0xAA1E98      /* enemies[256]，步长 0x10b0 */
/* engine/player/th06nc/01 §2.9 / §2.16 顺带解出的全局 */
#define RVA_LIVES             0x4FF0F0      /* u8 */
#define RVA_BOMBS             0x4FF0F1      /* u8 */
#define RVA_POWER             0x4F1E88      /* u16 */
#define RVA_SCORE             0x4F2798      /* u64 */
#define RVA_GRAZE_IN_STAGE    0x4FF0CC      /* i32 */
#define RVA_BOMB_IN_USE       0x509268      /* u8 */
#define RVA_SPELL_ACTIVE      0xBADF60      /* i32，EnemyManager+0x10c0d0 spellcardInfo.isActive */

/* --- Player 偏移（engine/player/th06nc/01 §2.5 / §2.7 / §2.10）--- */
#define OFF_POSITION_X        0x7730   /* f32 x, +4 y, +8 z，关卡区局部坐标（原点左上角） */
#define OFF_POSITION_Y        0x7734
#define OFF_HITBOX_RADIUS     0x774C   /* f32 = 1.25，被弹与擦弹共用 */
#define OFF_IS_FOCUSED        0x785C   /* bool */
#define OFF_CHAR_DATA         0x7860   /* CharacterData：+0 正交速度 +4 低速速度 */
#define OFF_PLAYER_STATE      0x7898   /* 0 ALIVE / 1 SPAWNING / 2 DEAD / 3 INVULNERABLE */

/* --- Bullet 偏移（engine/bullet/th06nc/01 §2，全部 code）--- */
#define BULLET_COUNT          640
#define BULLET_SIZE           0x620
#define OFF_B_SPEED           0x000    /* f32 */
#define OFF_B_VELOCITY        0x008    /* f32 x,y,z */
#define OFF_B_POS             0x030    /* f32 x,y,z */
#define OFF_B_ANGLE           0x040    /* f32 */
#define OFF_B_STATE           0x044    /* i16：0 空 1 飞行 2-4 出现动画 5 消失 */
#define OFF_B_GRAZE_SIZE      0x5F4    /* f32 x,y：CalcKillBoxCollision 的第三实参 */
#define OFF_B_IS_GRAZED       0x618    /* u8 */
#define BULLET_STATE_FLYING   1

/* --- Enemy 偏移（engine/enemy/th06nc/01 §1 §5、engine/ecl/th06nc/04 §3）--- */
#define ENEMY_COUNT           256
#define ENEMY_SIZE            0x10B0
#define OFF_E_POS             0x0B0    /* f32 x,y,z（code） */
#define OFF_E_FLAGS           0x0BC    /* byte；bit7 = 槽被占用（code） */
#define OFF_E_BOSS_ID         0x230    /* i32（weak：只有 opcode 101 一个锚） */
#define OFF_E_LIFE            0x234    /* i32（code：SpawnEnemy 直写） */
#define OFF_E_MAX_LIFE        0x238    /* i32（code） */
#define OFF_E_HITBOX          0x2C0    /* f32 x,y,z（code：EnemyManager__OnUpdate × 2/3 传给 CalcKillBoxCollision） */
#define OFF_E_FLAGS2          0x0BD    /* byte；(& 3) == 3 才做本体碰撞（EnemyManager__OnUpdate 0x1400373b0） */
#define ENEMY_FLAG_ALIVE      0x80
#define ENEMY_FLAGS2_COLLIDE  0x03
/* engine/player/th06nc/01 §2.7 完整公式（一手，0x14006a980）：
 *   弹：  R = min(grazeSize.x, grazeSize.y) × 0.5 + hitboxRadius，dist² < R² 即死
 *   敌人：盒半宽 = hitboxDimensions × (2/3) × 0.5 = /3，圆对轴对齐盒 */
#define BULLET_KILL_SIZE_SCALE  0.5f          /* 0x14030cd58 */
#define ENEMY_KILL_BOX_SCALE    (0.6666667f * 0.5f)   /* 0x14030cd74 × 0x14030cd58 */

/* --- 逻辑按钮位（engine/menu/th06nc/02 §8）--- */
#define IN_SHOOT  0x01
#define IN_BOMB   0x02
#define IN_FOCUS  0x04
#define IN_UP     0x10
#define IN_DOWN   0x20
#define IN_LEFT   0x40
#define IN_RIGHT  0x80
#define IN_ALL_MASK (IN_SHOOT | IN_BOMB | IN_FOCUS | IN_UP | IN_DOWN | IN_LEFT | IN_RIGHT)

/* --- 关卡区几何（engine/player/th06nc/01 §2.11，与 TH06 逐值相同）--- */
#define PLAYFIELD_W       384.0f
#define PLAYFIELD_H       448.0f
#define AREA_DEFAULT_X      8.0f     /* 可动区默认值：运行时读到 0 时退回 */
#define AREA_DEFAULT_Y     16.0f
#define AREA_DEFAULT_W    368.0f
#define AREA_DEFAULT_H    416.0f

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
