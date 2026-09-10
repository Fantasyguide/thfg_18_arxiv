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
        0x140069D00,
        "Player__CalcDamageToEnemy",
        "[NC first-hand] Player bullets vs one enemy. Walks bullets[80] doing an AABB "
        "overlap test against (enemyPos, enemyHitboxSize) and accumulates bullet->damage "
        "(only a third of it while a bomb is active), then walks the 32 bomb regions and "
        "adds their damage. Sets *hitWithLazerDuringBomb when a bomb region connects while "
        "bombInfo.isInUse. Called from EnemyManager__OnUpdate right after the body check.",
    ),
    (
        0x1400373B0,
        "EnemyManager__OnUpdate",
        "[NC first-hand] Per-enemy tick. Identified because CC0 TH06 has exactly two callers "
        "of CalcKillBoxCollision (BulletManager::OnUpdate and EnemyManager::OnUpdate) and NC "
        "has exactly two as well, one already independently named BulletManager__OnUpdate. "
        "Content confirms it: hitbox scaled by 1/1.5, life -= 10 on a body hit, damage "
        "clamped to 70, then the spellcard divisors.",
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
# 来源三档：
#   code     —— 偏移由指令直接读出，语义明确
#   code-unk —— 偏移由指令直接读出，但**语义连 CC0 也未知**，沿用其 unk_ 名以便对照
#   weak     —— 偏移本身就不确定，只算线索
PLAYER_FIELDS = [
    (0x0000, "bombRegionPositions", "code",
     "D3DXVECTOR3[32]。CalcDamageToEnemy 第二个循环 LEA RBX,[player+0x184] 步长 0xc、32 次，"
     "位置读 [RBX-0x184] 即 player+0x0 + idx*12"),
    (0x0180, "bombRegionSizes", "code",
     "D3DXVECTOR3[32]，[RBX-0x4]。size.x <= 0 的槽跳过 —— 与 CC0 同一判据"),
    (0x0300, "bombRegionDamages", "code",
     "i32[32]，LEA RDI,[player+0x380] 步长 4，读 [RDI-0x80] 即 player+0x300 + idx*4"),
    (0x0380, "unk_838", "code-unk",
     "i32[32]，[RDI]。命中时 += bombRegionDamages[idx]，与 CC0 的 unk_838 对应"),
    (0x0408, "fireBulletTimer", "code",
     "ZunTimer：读 +0x40c(current)、与 +0x408(previous) 比较判断是否跳过；"
     "生成完一轮后 previous=current、current++，并与 0x1e(30) 比较"),
    (0x0410, "bullets", "code",
     "PlayerBullet[80]，步长 0x170。LEA RBX,[RDI+0x410] 在生成与更新两个循环里都出现；"
     "0x410 + 80*0x170 = 0x7710，正好接上 +0x7720 的回调字段 —— 布局自洽"),
    (0x7718, "unk_9e4", "code-unk",
     "byte 计数器。激光弹命中时 INC 后 TEST 0x7（%8）、炸弹区域命中时 TEST 0x3（%4），"
     "用来给粒子特效降频"),
    (0x7720, "fireBulletCallback", "code",
     "AddedCallback 从 characterData(+0x7870) 拷来；SpawnBullets 在非低速时 CALL 它"),
    (0x7728, "fireBulletFocusCallback", "code", "同上，低速时用（+0x7878 拷来）"),
    (0x7730, "positionCenter", "code",
     "Player__OnUpdate 每帧 ADDSS/MOVSS 读改写 +0x7730/34/38；"
     "绝对地址 0x140506ad0 被 18 个函数读，含 ECL VM 内联的 AngleToPlayer"),
    (0x773C, "respawnTimer", "code",
     "曾被本仓标成 playerState（错），随后降级成 unk_773c（也不够）。**现已解**："
     "三处写入与 CC0 逐条对应 —— AddedCallback 写 8（Player.cpp:138）、"
     "OnUpdate 复活动画里写 0（:281）、进入 INVULNERABLE 时写 6（:290，同段 "
     "invulnerabilityTimer.SetCurrent(240) = 0xf0）；DEAD 期间每帧 -- 并在归零时发道具。"
     "非零期间允许按炸弹 —— 这就是决死窗口"),
    (0x7740, "positionOfLastEnemyHit", "code",
     "每帧先写 (-999,-999,0)（立即数 0xc479c000）；追踪弹判 x > -100 才启用"),
    (0x774C, "hitboxRadius", "code",
     "Player__AddedCallback 的 MOV qword [RBX+0x774c],0x3fa00000 = 1.25f（高 dword 顺带清零 +0x7750）；"
     "CalcKillBoxCollision 用它的平方与距离平方比较"),
    (0x7750, "playerDirection", "code", "Player__OnUpdate 在移动 switch 里写 0..8"),
    (0x7754, "bombProjectiles", "code", "0x14006a8c0 遍历它，跳过 x==0 的项"),
    (0x7854, "invulnerabilityTimer", "code",
     "ZunTimer（8 字节）。Die 里 InitializeForPopup：+0x7858(current)=0、+0x7854(previous)=-999；"
     "复活时 SetCurrent(240) 写 0xf0。**ZunTimer 为 8 字节的第四次独立印证**"),
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
    (0x7880, "orbsPosition", "code",
     "D3DXVECTOR3[2]：+0x7880 与 +0x788c 每帧都被写成 positionCenter；"
     "laser 弹取 orbsPosition[spawnPositionIdx-1]"),
    (0x7898, "playerState", "code",
     "擦弹前查 != 1 && != 2；死亡判定写 2、复活写 1。"
     "与 TH06 的 PLAYER_STATE_ALIVE/SPAWNING/DEAD/INVULNERABLE = 0/1/2/3 逐值相同"),
    (0x79E8, "laserTimer", "code",
     "ZunTimer[2]，步长 8。两处证据：laser 弹用 [RDI + idx*8 + 0x79ec] 读 current 与 70/1 比较；"
     "UpdatePlayerBullets 开头把 CC0 的 2 次递减循环展开成对 +0x79ec 与 +0x79f4 的两段"),
    (0x9EC8, "bombInfo.isInUse", "code",
     "CMP byte [player+0x9ec8],0：为真则子弹伤害 /3，且炸弹区域命中会置 hitWithLazerDuringBomb"),
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

# CharacterData 的完整布局（32 字节），与 CC0 Player.hpp 的同名结构逐字段一致：
#   +0x00 f32 orthogonalMovementSpeed       +0x04 f32 orthogonalMovementSpeedFocus
#   +0x08 f32 diagonalMovementSpeed         +0x0c f32 diagonalMovementSpeedFocus
#   +0x10 FireBulletCallback fireBulletCallback
#   +0x18 FireBulletCallback fireBulletFocusCallback
# ⚠️ 后 16 字节**是两个函数指针，不是浮点**。本仓一度把它们按 f32 读出
# `{2.104, 0, 2.104, 0}` 并记为「含义未知」——那是指针低 dword 0x4006a840
# 被当成 float（恰好 = 2.1041）。四个自机型的表项分别指向 SHOT_TYPES 里的转发桩。
CHAR_DATA_FIELDS = [
    (0x00, "orthogonalMovementSpeed", "f32"),
    (0x04, "orthogonalMovementSpeedFocus", "f32"),
    (0x08, "diagonalMovementSpeed", "f32"),
    (0x0C, "diagonalMovementSpeedFocus", "f32"),
    (0x10, "fireBulletCallback", "ptr"),
    (0x18, "fireBulletFocusCallback", "ptr"),
]
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
# 射击链：SpawnBullets 与 UpdatePlayerBullets **都被内联进 Player__OnUpdate**。
#
# CC0 侧是两个独立函数（Player.cpp:472 UpdatePlayerBullets、:1043 SpawnBullets），
# NC 侧没有对应地址可命名 —— 与 EnemyEclInstr::GetVar 被内联 25 份是同一类现象
# （engine/ecl/th06nc/03 §4）。所以这里登记的是**区间**而不是函数。
INLINED_RANGES = [
    # ⚠️ 第三个字段会被写进 Ghidra 并随 th06nc-re-data 发布 —— **必须 ASCII**。
    # 中文说明写在 engine/player/th06nc/01 §2.12，不要挪到这里。
    (0x140069740, 0x140069B03, "Player__UpdatePlayerBullets",
     "Starts by decrementing laserTimer[0] and [1] (CC0's 2-iteration loop, unrolled), "
     "then walks bullets[80]: branch on bulletType, advance position, "
     "recycle when out of bounds or the anm script ends."),
    (0x140069BD1, 0x140069C6A, "Player__SpawnBullets",
     "Scans bullets[80] for a free slot (bulletState == 0), picks the plain or focus "
     "callback by isFocused and calls it; see FIRE_BULLET_RESULT for the return codes."),
]

# 回调返回值。由内联代码里的 CMP 常量直接读出，语义对照 CC0 的 FireBulletResult：
#   ECX <  0 且 == -2 -> 立即停止本帧的生成（FBR_STOP_SPAWNING）
#   ECX == -1         -> 同一个槽再打一发（FBR_SPAWN_MORE，跳回循环体开头）
#   ECX >  0          -> 停止
#   ECX >= 0          -> 视为已发射：拷 position -> sprite.pos，z=0.495，bulletState=1
FIRE_BULLET_RESULT = {"FBR_STOP_SPAWNING": -2, "FBR_SPAWN_MORE": -1}

PLAYER_BULLET_SIZE = 0x170      # 循环里的 ADD RBX,0x170
PLAYER_BULLET_COUNT = 80        # CMP R14D,0x50；与 CC0 的 bullets[80] 相同
PLAYER_BULLET_STATE_UNUSED = 0
PLAYER_BULLET_STATE_FIRED = 1

# ⚠️ NC 的 ZunTimer 是 **8 字节 {i32 previous; i32 current}**，
# TH06 是 12 字节 {i32 previous; f32 subFrame; i32 current}（CC0 ZunTimer.hpp）。
# 三条独立证据：
#   1. fireBulletTimer 的 previous/current 相差 4（+0x408 / +0x40c）
#   2. laserTimer[] 的索引步长是 8（[RDI + idx*8 + 0x79ec]）
#   3. 若为 12 字节，fireBulletTimer 会与 bullets[0]（+0x410）重叠 —— 排除
ZUNTIMER_SIZE = 8
ZUNTIMER_SIZE_TH06 = 12

# PlayerBullet 字段（相对 bullets[i] 基址）。
# 来源 code = 直接从指令读出；每条都能对上 CC0 UpdatePlayerBullets/SpawnBullets 的某一行。
PLAYER_BULLET_FIELDS = [
    (0x000, "unk_140.previous", "code", "Tick(): MOV [RBX],EAX 把 current 抄进来"),
    (0x004, "unk_140.current", "code", "CMP EAX,0x28 对应 CC0 的 unk_140.AsFrames() < 40"),
    (0x008, "velocity.x", "code", "追踪分支里 ADDSS XMM6,[RBX+0x8]"),
    (0x00C, "velocity.y", "code", "BULLET_TYPE_2 分支 SUBSS [RBX+0xc],0.3 = CC0 的 velocity.y -= 0.3f"),
    (0x010, "bulletState", "code", "i16；==0 视为空闲槽，发射后写 1"),
    (0x012, "bulletType", "code", "i16；switch 的分支变量，1/2/3 三路"),
    (0x018, "sprite", "code",
     "AnmVm 基址：LEA RDX,[RBX+0x18] 传给 AnmManager__ExecuteScript(0x1400069b0)"),
    (0x050, "sprite.anmFileIndex", "code",
     "CalcDamageToEnemy 里 MOVSX word [+0x50] 与 0x461..0x464 比较（魔理沙A 光球四档）；"
     "命中时写回 +0x20 = CC0 的 SetAndExecuteScriptIdx(anmFileIndex + 0x20)。"
     "= sprite(+0x18) + 0x38，与 ANM spec 里「脚本号在 vm+0x38」一致"),
    (0x0A2, "sprite.pendingInterrupt", "code",
     "laser 分支写 1；= sprite(+0x18) + 0x8a，与 ANM spec 里 SeekInterruptLabel 匹配的 vm+0x8a 吻合"),
    (0x0E0, "sprite.pos.x", "code", "SpawnBullets 把 position 拷进来"),
    (0x0E4, "sprite.pos.y", "code",
     "SpawnBullets 里与 .x/.z 同一段拷贝：MOV [RBX+0xe4] <- position.y(+0x140)"),
    (0x0E8, "sprite.pos.z", "code", "新弹写 0.495（0x3efd70a4）"),
    (0x0FC, "sprite.scaleY", "code", "laser 分支 = position.y*2/14.0"),
    (0x138, "unk_152", "code", "i16；laserTimer 的下标（[RDI+idx*8+0x79ec]）"),
    (0x13C, "position.x", "code",
     "追踪分支 SUBSS XMM6,[RBX+0x13c] 求到 positionOfLastEnemyHit 的差；"
     "laser 分支 position.x += sidewaysMotion"),
    (0x140, "position.y", "code", "laser 分支 *= 0.5，对应 CC0 的 position.y /= 2.0f"),
    (0x144, "position.z", "code", "laser 分支写 0.44（0x3ee147ae）"),
    (0x148, "sidewaysMotion", "code", "laser 分支 position.x += 它"),
    (0x14C, "spawnPositionIdx", "code",
     "i16；取 orbsPosition[idx-1]，基址 0x7880-12=0x7874 把 -1 折进了寻址"),
    (0x154, "unk_134.y", "code", "追踪的速度状态：/4 参与转向、结果 MIN(len,10.0)"),
    (0x15C, "damage", "code",
     "i16。CalcDamageToEnemy 命中时累加它（炸弹中则 /3，为 0 取 1）；"
     "BULLET_TYPE_2 命中后自身 /= 4 并写回，为 0 取 1"),
    (0x160, "size.x", "code",
     "AABB 判定的半宽来源；魔理沙A 光球命中后按脚本号写 32/42/48"),
    (0x164, "size.y", "code", "laser 分支 = position.y*2"),
]

# 这一轮把 CC0 的语义常量逐个在 NC 里核对过，**七个全中**（量纲关）。
BULLET_CONSTANTS = [
    (0x14030CD58, 0.5,     "laser: position.y /= 2"),
    (0x14030CD40, 0.3,     "BULLET_TYPE_2: velocity.y -= 0.3"),
    (0x14030D0F0, -100.0,  "追踪前置条件 positionOfLastEnemyHit.x > -100"),
    (0x14030CEA0, 10.0,    "ZUN_MIN(vecLength, 10.0)"),
    (0x14030CEAC, 14.0,    "sprite.scaleY = position.y*2 / 14"),
    (0x14030CD34, 0.25,    "unk_134.y / 4"),
    (0x14030CD9C, 1.0,     "下限钳位"),
]
POSITION_OF_LAST_ENEMY_HIT_RESET = -999.0   # 立即数 0xc479c000，每帧重置

# ─────────────────────────────────────────────────────────────────────────────
# 阶段二：射击 → 敌人的判定链。
#
# EnemyManager__OnUpdate 的顺序，与 CC0 EnemyManager.cpp:583-635 逐条同构：
#   1. hitbox = enemy->hitboxDimensions * (1/1.5)，调 CalcKillBoxCollision
#      —— 体术碰撞，命中且 isInteractable && !isBoss 时 enemy->life -= 10
#   2. CalcDamageToEnemy(&enemy->position, &enemy->hitboxDimensions, &hitWithLazerDuringBomb)
#   3. damage = MIN(damage, 70)；g_Score += (damage/5)*10
#   4. 符卡中：没打中激光 -> damage /= 7；打中了且 usedBomb -> damage /= 3；否则 damage = 0
#   5. isDamageable 才真正 enemy->life -= damage
#   6. enemy->position.y > positionOfLastEnemyHit.y 就更新它
#      —— **追踪弹的数据源**，与阶段一的 UpdatePlayerBullets 闭环
DAMAGE_CAP = 70                       # CMP EAX,0x46 / CMOVGE
BODY_COLLISION_DAMAGE = 10            # ADD dword [enemy+0x234],-0xa
ENEMY_HITBOX_BODY_SCALE = 1.0 / 1.5   # 0x14030cd74
SCORE_PER_DAMAGE = (5, 10)            # (damage / 5) * 10，加到 g_Score(0x1404f2798)
SPELLCARD_DAMAGE_DIVISORS = {"no_laser": 7, "laser_after_bomb": 3}

ENEMY_MANAGER_FIELDS = [
    (0x10C0D0, "spellcardInfo.isActive", "code", "CMP dword [R12+0x10c0d0],0 决定是否走符卡减伤"),
    (0x10C0D4, "spellcardInfo.usedBomb", "code", "CMP byte [R12+0x10c0d4],0 —— 决定 /3 还是清零"),
]

PLAYER_BOMB_REGION_COUNT = 32
PLAYER_BULLET_STATE_COLLIDED = 2
BULLET_TYPE_2 = 2
BULLET_TYPE_LASER = 3
MARISA_A_ORB_ANM_BASE = 0x461         # 四个脚本号 0x461..0x464
MARISA_A_ORB_SIZES = (32.0, 42.0, 48.0, 48.0)
BOMB_ACTIVE_DAMAGE_DIVISOR = 3        # 炸弹期间子弹伤害 /3（结果为 0 则取 1）
TYPE2_DAMAGE_DECAY = 4                # BULLET_TYPE_2 命中后 damage /= 4（为 0 则取 1）
COLLIDED_VELOCITY_SCALE = 0.125       # 0x14030cd1c，命中后 velocity /= 8

# ─────────────────────────────────────────────────────────────────────────────
# 阶段三：死亡链。Die() 与阶段一/二的两段一样被内联，只能登记区间。
#
# 内联点有两处：CalcKillBoxCollision（被弹）与 CalcLaserHitbox（被激光）。
# 两处都以「playerState == ALIVE 才继续」开头，以 laserTimer[0..1].SetCurrent(2) 结尾。
DIE_INLINED_AT = [
    (0x14006AAA5, 0x14006AB6C, "Player__CalcKillBoxCollision"),
    (0x14006AD4B, None, "Player__CalcLaserHitbox"),   # 只定位到 respawnTimer 那一句
]
DEATH_PARTICLES = [
    (12, 1, 0xFF4040FF, "PARTICLE_EFFECT_UNK_12 / COLOR_NEONBLUE"),
    (6, 16, 0xFFFFFFFF, "PARTICLE_EFFECT_UNK_6 / COLOR_WHITE"),
]
DEATH_SOUND = 4                  # SOUND_PICHUN，经由音效队列 0x140509670
DEATH_LASER_TIMER_VALUE = 2      # laserTimer[i].SetCurrent(2)
INVULNERABILITY_FRAMES = 30      # DEAD -> SPAWNING 的门限（复活动画长度）
INVULNERABLE_TIMER_VALUE = 240   # 进入 INVULNERABLE 时 SetCurrent(240) = 0xf0
ZUNTIMER_PREVIOUS_SENTINEL = -999   # 0xfffffc19，InitializeForPopup / SetCurrent 都写它

# 决死（deathbomb）窗口 = 死亡时 respawnTimer 的值。
# ⚠️ 这是 NC 与 TH06 的一处**行为差异**，见 engine/player/th06nc/01 §2.17：
#   TH06：Die() 不碰 respawnTimer -> 首条命 8 帧，之后每次复活写 6 -> 6 帧
#   NC  ：Die() 显式写 8          -> **每次都是 8 帧**
DEATHBOMB_WINDOW = 8
DEATHBOMB_WINDOW_TH06_FIRST_LIFE = 8
DEATHBOMB_WINDOW_TH06_AFTER_RESPAWN = 6

# 死亡结算（respawnTimer 归零那一帧）
DEATH_ITEMS_WITH_LIVES = 6       # POWER_BIG x1 + POWER_SMALL x5
DEATH_ITEMS_GAME_OVER = 5        # FULL_POWER x5
DEATH_POWER_LOSS = 16            # currentPower <= 16 -> 0，否则 -16
ITEM_POWER_BIG, ITEM_POWER_SMALL = 2, 0

# 决死取消死亡：四个 Bomb*Calc 都写 playerState = INVULNERABLE(3)
DEATHBOMB_REVIVE_WRITES = [
    (0x14000D5F6, "BombData__BombReimuACalc"),
    (0x14000E11F, "BombData__BombReimuBCalc"),
    (0x14000E63E, "BombData__BombMarisaACalc"),
    (0x14000EBCD, "BombData__BombMarisaBCalc"),
]

# 资源全局（这一轮顺带定位）
G_LIVES_REMAINING = 0x1404FF0F0     # byte，1UP 上限 8
G_BOMBS_REMAINING = 0x1404FF0F1     # byte，同上限
G_CURRENT_POWER = 0x1404F1E88       # u16
G_POWER_ITEM_COUNT_FOR_SCORE = 0x1404FF0DD

# ❓ NC 特有：一个配置驱动的模式开关，TH06 没有对应物。
# 置位处 0x14004dbe8 SETNZ，源是配置字节 0x140c21dbb（同段还读初始机数/炸弹数）。
# 已观察到两个效果，都在死亡/资源路径上：
#   1. 死亡扣火力后设下限 8（0x140068bb8：currentPower < 8 -> 8）
#   2. 1UP 不加命，改为 DEC 抵扣计数 0x1404f1e60（0x1400433ff）；死亡时 INC 它（0x140068b1a）
# 机制一手确定；**这个开关叫什么、对应界面上哪个选项，未知**。
NC_MODE_FLAG = 0x1404F27C4
NC_DEATH_DEBT_COUNTER = 0x1404F1E60
NC_DEATH_POWER_FLOOR = 8
NC_MODE_FLAG_CONFIG_BYTE = 0x140C21DBB

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

    # 按名字查，不靠表内顺序 —— 早先写成 PLAYER_FIELDS[0]，
    # 后来在 positionCenter 之前插入了 bullets/fireBulletTimer，那个假设就塌了。
    _by_off = {n: o for o, n, _s, _e in PLAYER_FIELDS}
    if "positionCenter" not in _by_off:
        errors.append("positionCenter 不在字段表里")
    elif G_PLAYER + _by_off["positionCenter"] != PLAYER_POSITION_GLOBAL:
        errors.append("g_Player + positionCenter 偏移必须等于记录的绝对地址")
    # bullets[80] 的尾部必须正好接上 fireBulletCallback —— 布局自洽性检查
    if "bullets" in _by_off and "fireBulletCallback" in _by_off:
        if _by_off["bullets"] + PLAYER_BULLET_COUNT * PLAYER_BULLET_SIZE > _by_off["fireBulletCallback"]:
            errors.append("bullets[80] 覆盖到了 fireBulletCallback —— 步长或数量取错了")
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
        if source not in ("code", "code-unk", "xmatch", "weak"):
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
