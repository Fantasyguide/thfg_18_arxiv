"""Reviewed TH06NC player-subsystem symbols for the Ghidra importer.

定位手法与 ECL 那批不同：玩家主循环是 chain 回调（没有直接 xref），
碰撞/擦弹是从弹幕主循环里调的，ECL 的 opcode 坐标系够不着。用的是三个锚：

1. **输入位图**：`0x1400127d0` 生成的逻辑按钮位图里
   bit0=射击 / bit1=炸弹 / bit2=低速 / bit4-7=方向。同时测这几位的函数只有一个。
2. **调用侧画像**：TH06 的 `BulletManager::OnUpdate` 调 `CheckGraze` /
   `CalcKillBoxCollision` / `CalcLaserHitbox`；NC 的对应函数也正好调三个同邻域函数。
3. **参数与内容**：`CalcLaserHitbox` 的 6 个参数逐个吻合；炸弹裁剪循环的
   「跳过 x==0 项 + 半宽 AABB」与 CC0 逐行同构。

⚠️ **顺序推断在这里翻过车**：一开始按「地址顺序 = 源码顺序」把三个函数对上去，
内容一验就发现 `0x14006a8c0` 根本不是 `CalcKillBoxCollision`，而是 CC0 内联在
`CheckGraze` 里的炸弹裁剪循环。**别用顺序猜，用内容验。**

名字取自 CC0 的 GensokyoClub/th06；NC 独有的那个用我们自己的描述性命名。
"""

EXPECTED_MD5 = "b0cc28ec904e9efce2439ad857c938b5"

# 逻辑按钮位图的位分配（`0x1400127d0` 一手解出，见 engine/menu/th06nc/02 §8）
BUTTON_BITS = {
    0x01: "SHOOT",     # Z / J
    0x02: "BOMB",      # X / K
    0x04: "FOCUS",     # LShift / Space / L
    0x08: "SKIP",      # LCtrl / ;
    0x10: "UP",
    0x20: "DOWN",
    0x40: "LEFT",
    0x80: "RIGHT",
}

# 我们自己起错过、需要被覆盖掉的旧名。留在这里是为了让改名可复现，
# 也是为了记住这次教训：0x14006a980 曾被命名成 Player__CheckGraze，
# 但它放的是音效 4（PICHUN，死亡）而不是 30（GRAZE）。
OUR_OLD_NAMES = {"Player__CheckGraze"}

FUNCTIONS = [
    (
        0x140068820,
        "Player__OnUpdate",
        "[NC first-hand] Player per-frame tick, registered as a chain callback (hence no "
        "direct xrefs). Tests the logical button bitmap at 0x140a6ec60: bomb on a press "
        "edge of bit 1, focus flag stored to player+0x785c from bit 2, an 8-case movement "
        "switch on the direction bits, shooting on bit 0. TH06 splits this into "
        "OnUpdate + HandlePlayerInputs (2282 + 3482 bytes); NC inlined them (5309).",
    ),
    (
        0x140068160,
        "Player__AddedCallback",
        "[NC first-hand] Player init: loads the player ANM, sets the spawn position "
        "(192, 384, 0.49), the hitbox RADIUS 1.25 at +0x774c and grabItemSize (12, 12, 5) "
        "at +0x9f28. All of those match TH06 numerically.",
    ),
    (
        0x14006A980,
        "Player__CalcKillBoxCollision",
        "[NC first-hand] Death collision against a bullet box. Runs the bomb-projectile "
        "clip test first (0x14006a8c0), and on a hit spawns the death particles, sets the "
        "death state and enqueues sound 4 (SOUND_PICHUN) -- TH06's Player::Die is inlined "
        "here. NOT graze: the only function that enqueues sound 30 (SOUND_GRAZE) for "
        "bullets is BulletManager__OnUpdate, so CheckGraze/ScoreGraze are inlined there. "
        "The extra char parameter selects circle-vs-AABB (0) or circle-vs-circle (1).",
    ),
    (
        0x14006A8C0,
        "Player__BombProjectileClipsBox",
        "[NC first-hand, our own name] Walks the bomb-projectile array at player+0x7754, "
        "skips entries whose x extent is 0, and AABB-tests each against the caller's box "
        "using half extents. CC0 has this logic INLINED inside Player::CheckGraze, so "
        "there is no upstream name to borrow -- note this is the opposite of NC's usual "
        "direction, which inlines more than TH06 rather than less.",
    ),
    (
        0x14006ABA0,
        "Player__CalcLaserHitbox",
        "[NC first-hand] Laser collision. The signature matches CC0's "
        "CalcLaserHitbox(laserCenter, laserSize, rotation, angle, canGraze) parameter for "
        "parameter, and it calls cosf/sinf for the rotation plus the graze particle effect.",
    ),
    (
        0x14006A580,
        "Player__FireBulletFromTable",
        "[NC first-hand, our own name] The one data-driven fire-bullet implementation. "
        "TH06 has four hand-written FireBullet{ReimuA,ReimuB,MarisaA,MarisaB}; NC replaced "
        "them with 26-byte thunks that each pass a CharacterPowerData table to this. "
        "No upstream name to borrow, because upstream has no such function.",
    ),
    (
        0x140010870,
        "BulletManager__OnUpdate",
        "[NC first-hand] Bullet per-frame tick. Identified by its callee profile: it calls "
        "all three player collision entry points plus AnmManager__ExecuteScript and "
        "EffectManager__SpawnParticles -- the same shape as TH06's BulletManager tick.",
    ),
    (
        0x14006a840,
        "Player__FireBulletReimuA",
        "[NC first-hand] 26-byte thunk: passes the ReimuA CharacterPowerData table to "
        "Player__FireBulletFromTable. Selected via g_CharData[0].",
    ),
    (
        0x14006a860,
        "Player__FireBulletReimuB",
        "[NC first-hand] 26-byte thunk: passes the ReimuB CharacterPowerData table to "
        "Player__FireBulletFromTable. Selected via g_CharData[1].",
    ),
    (
        0x14006a880,
        "Player__FireBulletMarisaA",
        "[NC first-hand] 26-byte thunk: passes the MarisaA CharacterPowerData table to "
        "Player__FireBulletFromTable. Selected via g_CharData[2].",
    ),
    (
        0x14006a8a0,
        "Player__FireBulletMarisaB",
        "[NC first-hand] 26-byte thunk: passes the MarisaB CharacterPowerData table to "
        "Player__FireBulletFromTable. Selected via g_CharData[3].",
    ),
    (
        0x14000d1a0,
        "BombData__BombReimuACalc",
        "[NC first-hand] Bomb logic for ReimuA, from g_BombData. Called through player+0x9ed0.",
    ),
    (
        0x14000d9c0,
        "BombData__BombReimuADraw",
        "[NC first-hand] Bomb draw for ReimuA, from g_BombData. Stored at player+0x9ed8.",
    ),
    (
        0x14000db50,
        "BombData__BombReimuBCalc",
        "[NC first-hand] Bomb logic for ReimuB, from g_BombData. Called through player+0x9ed0.",
    ),
    (
        0x14000e130,
        "BombData__BombReimuBDraw",
        "[NC first-hand] Bomb draw for ReimuB, from g_BombData. Stored at player+0x9ed8.",
    ),
    (
        0x14000e270,
        "BombData__BombMarisaACalc",
        "[NC first-hand] Bomb logic for MarisaA, from g_BombData. Called through player+0x9ed0.",
    ),
    (
        0x14000e650,
        "BombData__BombMarisaADraw",
        "[NC first-hand] Bomb draw for MarisaA, from g_BombData. Stored at player+0x9ed8.",
    ),
    (
        0x14000e7f0,
        "BombData__BombMarisaBCalc",
        "[NC first-hand] Bomb logic for MarisaB, from g_BombData. Called through player+0x9ed0.",
    ),
    (
        0x14000ebe0,
        "BombData__BombMarisaBDraw",
        "[NC first-hand] Bomb draw for MarisaB, from g_BombData. Stored at player+0x9ed8.",
    ),
]

# 玩家对象是个全局。基址由弹幕 tick 的调用点给出：
#   140011411  LEA RCX,[0x1404ff3a0]
#   140011418  CALL Player__CheckGraze
G_PLAYER = 0x1404FF3A0

# 已定位的玩家结构体偏移。分档同 ECL 那边：code = 直接从指令读出。
PLAYER_FIELDS = [
    (0x7730, "positionCenter", "code",
     "Player__OnUpdate 每帧 ADDSS/MOVSS 读改写 +0x7730/34/38；"
     "绝对地址 0x140506ad0 被 18 个函数读，含 ECL VM 内联的 AngleToPlayer"),
    (0x773C, "unk_773c", "weak",
     "init 与死亡判定都写 8。曾被本仓标成 playerState —— **错的**，"
     "真正的 playerState 在 +0x7898（枚举值与 TH06 相同）。这里究竟是什么仍未知"),
    (0x774C, "hitboxRadius", "code",
     "Player__AddedCallback 的 MOV qword [RBX+0x774c],0x3fa00000 = 1.25f（高 dword 顺带清零 +0x7750）；"
     "CalcKillBoxCollision 用它的平方与距离平方比较"),
    (0x7750, "playerDirection", "code", "Player__OnUpdate 在移动 switch 里写 0..8"),
    (0x7754, "bombProjectiles", "code", "0x14006a8c0 遍历它，跳过 x==0 的项"),
    (0x785C, "isFocused", "code", "Player__OnUpdate 里 *(bool*)(p+0x785c) = (input & 4) != 0"),
    (0x7860, "characterData_orthogonalMovementSpeed", "code",
     "移动 switch 在 focus 位为 0 的分支读它（0x140069143/68/94/1b9）；"
     "init 0x14006835e 从速度表 0x1403de3b0 一次 MOVUPS 读 16 字节写进 +0x7860"),
    (0x7864, "characterData_orthogonalMovementSpeedFocus", "code",
     "同一组分支在 focus 位为 1 时读它（0x140069136/54/80/1ac）"),
    (0x7868, "characterData_diagonalMovementSpeed", "code",
     "四个斜向分支经 CMOVZ 在 +0x7868/+0x786c 间二选一；"
     "0x14006840c 写入 = +0x7860 / sqrtf(2)，与 CC0 Player.cpp:115 同式"),
    (0x786C, "characterData_diagonalMovementSpeedFocus", "code",
     "0x140068414 写入 = +0x7864 / sqrtf(2)，对应 CC0 Player.cpp:116"),
    (0x7898, "playerState", "code",
     "擦弹前查 != 1 && != 2；死亡判定写 2、复活写 1。"
     "与 TH06 的 PLAYER_STATE_ALIVE/SPAWNING/DEAD/INVULNERABLE = 0/1/2/3 逐值相同"),
    (0x9F28, "grabItemSize", "code", "init 写 (12.0, 12.0, 5.0)，与 TH06 逐值相同"),
]

# 擦弹相关的全局（不在 Player 结构体里）
GRAZE_GLOBALS = [
    (0x1404FF0CC, "g_GrazeInStage", "关卡内擦弹数，上限 99999"),
    (0x1404FF0D0, "g_GrazeInTotal", "总擦弹数，上限 999999"),
    (0x1404F2798, "g_Score", "每次擦弹 +500"),
    (0x140509268, "g_BombInUse", "非 0 时擦弹不计数"),
]

# 供应用器直接建标签用（名字 + 注释）
GRAZE_GLOBALS_LABELS = [
    (0x1404FF0CC, "g_GrazeInStage",
     "[NC first-hand] Per-stage graze counter, capped at 99999. TH06 caps it at 9999 "
     "(CC0 Player::ScoreGraze) -- this is the one graze number NC actually changed."),
    (0x1404FF0D0, "g_GrazeInTotal", "[NC first-hand] Total graze counter, capped at 999999 (same as TH06)."),
    (0x1404F2798, "g_Score", "[NC first-hand] Score. A graze adds 500, same as TH06."),
    (0x140509268, "g_BombInUse", "[NC first-hand] Non-zero suppresses graze counting, same as TH06."),
]

# ★ 擦弹判定：同样是圆，而且复用了被弹判定那个半径字段。
#
#   r = min(弹宽, 弹高) * 0.5 + 20.0 + player[+0x774c]
#   命中条件：(py - cy)² + (px - cx)² < r²
#
# TH06 的 CheckGraze 是把弹幕盒各向外扩 20 之后做 AABB 对 AABB。
# **外扩 20 这个数两边相同**，变的还是形状。
GRAZE_MARGIN = 20.0
GRAZE_MARGIN_TH06 = 20.0
GRAZE_SHAPE = "circle"
GRAZE_SHAPE_TH06 = "aabb"

# 唯一查到的数值差异：关卡内擦弹上限被抬高了 10 倍。
GRAZE_STAGE_CAP = 99999
GRAZE_STAGE_CAP_TH06 = 9999      # CC0 Player::ScoreGraze
GRAZE_TOTAL_CAP = 999999
GRAZE_TOTAL_CAP_TH06 = 999999
GRAZE_SCORE = 500

# ★ 这轮最重要的结论：判定形状变了，数值没变。
#
# TH06 的 Player 有 hitboxTopLeft / hitboxBottomRight / hitboxSize 三个 D3DXVECTOR3
# （Player.hpp:213-217），碰撞是轴对齐盒对盒。
# NC 没有这三个字段，也没有任何 positionCenter ± hitboxSize 的计算，
# 取而代之的是 +0x774c 一个标量半径，比较写成 r² 与距离² ——**圆形判定**。
#
# 半径 1.25 与 TH06 的半宽 1.25 相同，所以 NC 的圆**内接**于 TH06 的方，四角被削掉：
#   TH06 面积 (2*1.25)^2 = 6.25   NC 面积 π*1.25² ≈ 4.909   小约 21%
#
# 注意范围：只有**被弹判定**变圆了。道具吸取仍是 AABB
# （+0xa2ac = +0x9f2c + +0x7734，即 grabItemSize 与 position 相加）。
HITBOX_SHAPE = "circle"
HITBOX_SHAPE_TH06 = "aabb"
HITBOX_RADIUS = 1.25
HITBOX_HALF_EXTENT_TH06 = 1.25
GRAB_ITEM_SIZE = (12.0, 12.0, 5.0)

# 出生坐标。两条路径，值都与 TH06 相同：
#   初次出生 Player__AddedCallback  -> (192, 384, 0.49)   CC0 Player.cpp:98-100
#   死亡复活 Player__OnUpdate 0x140068dc5 -> (192, 384, 0.2) CC0 Player.cpp:241-243
#
# ⚠️ 曾经把 NC 的**复活**路径与 CC0 的**初始化**路径对比，误报「NC 把 z 从 0.49 改成 0.2」。
# 两边都是初始化 0.49、复活 0.2，**没有差异**。周边也逐条吻合：
# 复活的 30 帧无敌判定（NC 的 CMP ECX,0x1e）与 scaleX/Y = 3.0 都在。
SPAWN_POSITION = (192.0, 384.0, 0.49)
RESPAWN_POSITION = (192.0, 384.0, 0.2)

# 玩家坐标的绝对地址 = G_PLAYER + positionCenter 偏移。ECL 把它当伪变量读，
# 三个分量各有一个 case（0x1400222a4 / b0 / bc 的 LEA）。
PLAYER_POSITION_GLOBAL = 0x140506AD0


# ─────────────────────────────────────────────────────────────────────────────
# 射击：NC 把 TH06 手写的四个 FireBullet* 改成了**一个数据驱动的实现**。
#
# g_CharData[shotType + character*2]（步长 32）里的回调全是 26 字节的转发桩，
# 各自把一张 CharacterPowerData 表的地址传给共同实现 0x14006a580。
#
# 速度值与 CC0 的 g_CharData 逐项相同：灵梦 4.0/2.0、魔理沙 5.0/2.5，
# 且 focus 与非 focus 用同一个回调 —— 这点也和 TH06 一样。
G_CHAR_DATA = 0x1403DE3B0
CHAR_DATA_STRIDE = 32
CHAR_SELECT_GLOBALS = (0x1404F1E80, 0x1404F1E81)   # 角色, 机型
FIRE_BULLET_IMPL = 0x14006A580

# (index, 名字, 转发桩, 那张 CharacterPowerData 表, 正交速度, 低速速度)
SHOT_TYPES = [
    (0, "ReimuA",  0x14006A840, 0x1403DCDB0, 4.0, 2.0),
    (1, "ReimuB",  0x14006A860, 0x1403DD530, 4.0, 2.0),
    (2, "MarisaA", 0x14006A880, 0x1403DCBD0, 5.0, 2.5),
    (3, "MarisaB", 0x14006A8A0, 0x1403DC8A0, 5.0, 2.5),
]

# 每档火力一项：{i32 numBullets; i32 power; CharacterPowerBulletData *bullets;}（x64 下 16 字节）。
# 档位与 TH06 相同。
POWER_LEVELS = (8, 16, 32, 48, 64, 80, 96, 128)
# CharacterPowerBulletData 的布局与 CC0 相同（ZUN_ASSERT_SIZE 0x24）：
#   +0x00 i16 waitBetweenBullets  +0x02 i16 bulletFrame  +0x04 f32[2] motion
#   +0x0c f32[2] size             +0x14 f32 direction    +0x18 f32 velocity
#   +0x1c u16 damage              +0x1e u8 spawnPositionIdx  +0x1f u8 bulletType
#   +0x20 i16 anmFileIdx          +0x22 i16 bulletSoundIdx
POWER_BULLET_STRIDE = 0x24

# ─────────────────────────────────────────────────────────────────────────────
# 炸弹：同样是按 (机型, 角色) 索引的函数指针表，每项 {Calc, Draw}。
# 玩家对象里 +0x9ed0 = Calc、+0x9ed8 = Draw，按下炸弹键时调 Calc。
#
# 表的第 1、4 项由音效枚举名独立确认：0x14000d1a0 放 SOUND_BOMB_REIMU_A(13)、
# 0x14000e7f0 放 SOUND_BOMB_MARISA_B(19) —— 名字里就写着是谁的炸弹。
G_BOMB_DATA = 0x1403DC260
BOMB_TABLE = [
    ("ReimuA",  0x14000D1A0, 0x14000D9C0),
    ("ReimuB",  0x14000DB50, 0x14000E130),
    ("MarisaA", 0x14000E270, 0x14000E650),
    ("MarisaB", 0x14000E7F0, 0x14000EBE0),
]

def validate():
    errors = []

    addresses = [address for address, _name, _comment in FUNCTIONS]
    if len(addresses) != len(set(addresses)):
        errors.append("duplicate function address")
    names = [name for _address, name, _comment in FUNCTIONS]
    if len(names) != len(set(names)):
        errors.append("duplicate function name")

    by_name = dict((name, comment) for _address, name, comment in FUNCTIONS)

    # NC 独有、没有上游名字的那个必须写明是我们自己起的，否则以后会被误当成 CC0 名字
    if "our own name" not in by_name.get("Player__BombProjectileClipsBox", ""):
        errors.append("Player__BombProjectileClipsBox must be marked as our own name")

    # 这个函数一度被我们错命名成 CheckGraze；注释里必须留着「不是擦弹」的更正
    if "NOT graze" not in by_name.get("Player__CalcKillBoxCollision", ""):
        errors.append("Player__CalcKillBoxCollision must keep the 'not graze' correction")

    if G_PLAYER + PLAYER_FIELDS[0][0] != PLAYER_POSITION_GLOBAL:
        errors.append("g_Player + positionCenter 偏移必须等于记录的绝对地址")
    if SPAWN_POSITION != (192.0, 384.0, 0.49) or RESPAWN_POSITION != (192.0, 384.0, 0.2):
        errors.append("出生/复活坐标与一手读到的立即数不符")
    # 真正的差异在形状不在数值——别让这两条被写反
    if HITBOX_SHAPE == HITBOX_SHAPE_TH06:
        errors.append("NC 的被弹判定是圆、TH06 是轴对齐盒，这条差异不许抹平")
    if HITBOX_RADIUS != HITBOX_HALF_EXTENT_TH06:
        errors.append("半径与 TH06 半宽同为 1.25，数值没变——别写成数值变了")
    if GRAZE_SHAPE == GRAZE_SHAPE_TH06:
        errors.append("擦弹判定也从盒变成了圆，这条差异不许抹平")
    if GRAZE_MARGIN != GRAZE_MARGIN_TH06:
        errors.append("外扩 20 两边相同——变的是形状不是这个数")
    if GRAZE_STAGE_CAP == GRAZE_STAGE_CAP_TH06:
        errors.append("关卡内擦弹上限 9999 -> 99999 是一手结论，别抹平")
    if GRAZE_TOTAL_CAP != GRAZE_TOTAL_CAP_TH06:
        errors.append("总擦弹上限两边相同")

    offsets = [o for o, _n, _s, _e in PLAYER_FIELDS]
    if offsets != sorted(offsets):
        errors.append("player field table must be sorted by offset")
    for _offset, name, source, evidence in PLAYER_FIELDS:
        if source not in ("code", "xmatch", "weak"):
            errors.append("%s: bad source tag" % name)
        if not evidence:
            errors.append("%s: no evidence recorded" % name)

    if len(SHOT_TYPES) != 4 or [r[0] for r in SHOT_TYPES] != [0, 1, 2, 3]:
        errors.append("四个自机型必须按 index 0..3 排列")
    for _i, name, thunk, table, ortho, focus in SHOT_TYPES:
        if focus * 2 != ortho:
            errors.append("%s 的低速速度应是正交速度的一半" % name)
        if not (0x140000000 <= thunk < 0x141000000 and 0x140000000 <= table < 0x141000000):
            errors.append("%s 的地址不在 imagebase 范围内" % name)
    if POWER_LEVELS != (8, 16, 32, 48, 64, 80, 96, 128):
        errors.append("火力档位与 TH06 相同，别改")
    if len(BOMB_TABLE) != 4:
        errors.append("炸弹表必须是四项")
    if BOMB_TABLE[0][1] != 0x14000D1A0 or BOMB_TABLE[3][1] != 0x14000E7F0:
        errors.append("第 1/4 项由音效枚举名确认，别动")

    if set(BUTTON_BITS) != {1, 2, 4, 8, 0x10, 0x20, 0x40, 0x80}:
        errors.append("button bit map should cover bits 0..7")

    return errors
