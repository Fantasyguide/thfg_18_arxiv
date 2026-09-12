"""TH06NC 敌弹子系统（BulletManager / Bullet / Laser）的复核结果。

来源全部是 MCP 直查 Ghidra 库（`BulletManager__OnUpdate` 的反编译 +
`BulletManager__SpawnLaserPattern` 的反汇编），不是 HTML 导出。

底本对照 CC0 th06-decomp 的 BulletManager.hpp/cpp。**两边结构体都重排过**，
所以偏移一律从指令读，只有「有哪些字段」可以借。
"""

EXPECTED_MD5 = "b0cc28ec904e9efce2439ad857c938b5"

# ─────────────────────────────────────────────────────────────────────────────
# 池的骨架。三方自洽（见 validate()）：
#   g_BulletManager + LASERS_OFFSET == LASERS_ABS
#   BULLETS_OFFSET + BULLET_COUNT * BULLET_SIZE == BULLET_COUNT_FIELD
G_BULLET_MANAGER = 0x1403EC2A0     # ECL 那轮独立得到（engine/ecl/th06nc/04 §2）

BULLETS_OFFSET = 0x08              # pfVar17 = (float *)(param_1 + 2)
BULLET_SIZE = 0x620                # 循环 += 0x188 个 float；回收时 memset(.., 0x620)
BULLET_COUNT = 640                 # while (idx < 0x280)
BULLET_COUNT_FIELD = 0xF5008       # param_1[0x3d402]，每帧清零后按活跃弹累加

LASERS_OFFSET = 0xF5018
LASERS_ABS = 0x1404E12B8           # LEA RBX,[0x1404e12b8]（SpawnLaserPattern）
LASER_SIZE = 0x298                 # ADD RBX,0x298
LASER_COUNT = 64                   # CMP EAX,0x40
LASER_IN_USE = 0x274               # CMP byte [RBX+0x274],0 找空闲槽；占用时写 1

# TH06 侧的同名量，用来钉住"哪些变了、哪些没变"
BULLET_SIZE_TH06 = 0x5C4
BULLET_COUNT_TH06 = 640            # 数量没变
LASER_SIZE_TH06 = 0x270
LASER_COUNT_TH06 = 64              # 数量没变

# ─────────────────────────────────────────────────────────────────────────────
# 弹的状态机。OnUpdate 里 switch(state)，与 CC0 的 BULLET_STATE_* 同构。
BULLET_STATE_UNUSED = 0
BULLET_STATE_FIRED = 1
BULLET_STATES_SPAWNING = (2, 3, 4)   # 三档出现动画，各自用不同的 AnmVm 与速度分母
BULLET_STATE_DESPAWNING = 5

# 三档出现动画各自的 AnmVm 偏移与速度分母（分母来自 .rdata，见 SPAWN_DIVISOR_ADDRS）
SPAWN_VM_OFFSETS = {2: 0x170, 3: 0x290, 4: 0x3B0}
SPAWN_DIVISOR_ADDRS = {2: 0x14030CDD8, 3: 0x14030CDF0, 4: 0x14030CE10}

# ─────────────────────────────────────────────────────────────────────────────
# Bullet 字段（相对 bullets[i]，步长 0x620）。全部 code。
BULLET_FIELDS = [
    (0x000, "speed", "code", "各分支末尾 fVar28 = *pfVar17，再乘 cos/sin 写进 velocity"),
    (0x004, "dirChangeInterval", "code", "与 timer.current 一起算 (numTimes+1)*interval"),
    (0x008, "velocity.x", "code", "= cos(angle) * speed"),
    (0x00C, "velocity.y", "code", "= sin(angle) * speed"),
    (0x010, "velocity.z", "code", "只参与位置累加"),
    (0x014, "exFlags", "code", "u16 位图，测过 0x1/0x10/0x20/0x40/0x80/0x100/0x400/0x800"),
    (0x01C, "dirChangeNumTimes", "code", "每次转向 +1"),
    (0x020, "dirChangeMaxTimes", "code", "到达则清掉对应的 exFlags 位"),
    (0x024, "currentSpeed", "code", "转向时从 speed 刷新，斜坡阶段按比例插值"),
    (0x028, "timer.previous", "code", "循环末尾 previous = current"),
    (0x02C, "timer.current", "code", "循环末尾 current++；ZunTimer 又是 8 字节"),
    (0x030, "pos.x", "code", "pos += velocity；传给 CalcKillBoxCollision 的第二个实参"),
    (0x034, "pos.y", "code", "与 pos.x 同一组累加；出界判定用它与半高比较"),
    (0x038, "pos.z", "code", "只做累加，不参与出界与判定"),
    (0x040, "angle", "code", "cos/sin 的输入；出界反射时按象限折叠"),
    (0x044, "state", "code", "i16，switch 的分支变量"),
    (0x048, "dirChangeEndTime", "code", "与 timer.current 比较，决定是否继续转向"),
    (0x050, "sprite", "code", "AnmVm：AnmManager__ExecuteScript(g_AnmManager, bullet+0x50)"),
    (0x150, "spriteDesc", "code",
     "指针，取 +0x1c/+0x20 当半高/半宽做出界判定（= anm sprite 的 heightPx/widthPx）"),
    (0x170, "spawnVm0", "code", "state 2 的出现动画"),
    (0x290, "spawnVm1", "code", "state 3"),
    (0x3B0, "spawnVm2", "code", "state 4"),
    (0x5F4, "grazeSize.x", "code", "CalcKillBoxCollision 的第三个实参"),
    (0x5F8, "grazeSize.y", "code", "与 .x 相邻；擦弹半径取 min(.x,.y)*0.5 + 20 + hitboxRadius"),
    (0x600, "dirChangeRotation", "code", "每次转向累加到 angle"),
    (0x604, "offscreenFrames", "code", "u16；出界累加，> 0xff 才真正回收"),
    (0x608, "ex4Acceleration.x", "code", "exFlags 位 0x10：velocity += 它"),
    (0x60C, "ex4Acceleration.y", "code", "与 .x 同一分支：velocity.y += 它，再 atan2 更新 angle"),
    (0x610, "ex4Acceleration.z", "code", "velocity.z += 它"),
    (0x614, "angleDelta", "code", "转向时 angle += 它（或直接赋值）"),
    (0x618, "isGrazed", "code", "u8；擦过一次后置 1，之后不再判定"),
]

# ─────────────────────────────────────────────────────────────────────────────
# Laser 字段（相对 lasers[i]，步长 0x298）。
# 偏移来自 SpawnLaserPattern 的写入 + OnUpdate 里以 laser+0x14 为游标的读写。
LASER_FIELDS = [
    (0x004, "width", "code", "算 vm0.scaleY 与 alpha 的分子"),
    (0x008, "timer.previous", "code", "SpawnLaserPattern 写 -999"),
    (0x00C, "timer.current", "code", "写 0；OnUpdate 末尾 previous=current、current++"),
    (0x010, "startOffset", "code", "被 endOffset - startLength 追着走，钳到 >= 0"),
    (0x014, "endOffset", "code", "每帧 += speed"),
    (0x018, "despawnDuration", "code", "state 1/2 阶段与 timer.current 比较"),
    (0x020, "vm0", "code", "AnmVm，ExecuteScript 的对象"),
    (0x140, "vm1", "code", "第二个 AnmVm，脚本号 0x21c"),
    (0x260, "angle", "code", "SpawnLaserPattern 里可加 AngleToPlayer 的结果"),
    (0x264, "flags", "code", "u16，位 0 选「按宽度收缩」还是「按 alpha 淡出」"),
    (0x268, "hitboxStartTime", "code", "state 1 里与 timer.current 比较"),
    (0x26C, "speed", "code", "每帧加到 endOffset"),
    (0x270, "duration", "code", "state 0 阶段的总时长"),
    (0x274, "inUse", "code", "u8；找空闲槽的判据，占用时写 1"),
    (0x278, "pos.x", "code", "SpawnLaserPattern 从 bulletProps+0x20 一次 MOVSD 拷两个 float"),
    (0x27C, "pos.y", "code", "与 pos.x 在同一条 MOVSD 里拷入"),
    (0x280, "pos.z", "code", "单独一条 MOV，来自 bulletProps+0x28"),
    (0x284, "color", "code", "u16，来自 bulletProps+0x0e"),
    (0x288, "startLength", "code", "startOffset 落后 endOffset 不超过它"),
    (0x28C, "startTime", "code", "来自 bulletProps+0x00"),
    (0x290, "state", "code", "u8；0 -> 1 -> 2 的三段"),
]

FUNCTIONS = [
    (
        0x140010870,
        "BulletManager__OnUpdate",
        "[NC first-hand] Bullet + laser per-frame tick. Walks bullets[640] (stride 0x620) "
        "running a switch on state, then lasers[64] (stride 0x298). Graze is inlined here.",
    ),
    (
        0x1400105E0,
        "BulletManager__SpawnLaserPattern",
        "[NC first-hand] Scans lasers[64] for a free slot (inUse == 0 at +0x274) and fills it "
        "from an EnemyLaserShooter. Gives the pool base (0x1404e12b8), stride (0x298) and "
        "count (0x40) directly as immediates.",
    ),
]

# 激光判定（engine/bullet/th06nc/01 §3.1，2026-09-11）：只在 state 1 判；
# 盒 = 激光坐标系里 center=(pos.x+startOffset+len/2, pos.y)、size=(len, width/2) 的 ±size/2 → 半高 width/4；
# 自机绕 pos 旋转 −angle 进坐标系后钳到盒内比 hitboxRadius。
LASER_HITBOX_FN = 0x14006ABA0
LASER_HITBOX_CALLSITE = 0x140011883          # 唯一调用点
LASER_KILL_STATE = 1
LASER_HALF_H_SCALE = 0.25                    # 半高 = width × 0.25
LASER_GRAZE_MARGIN = 48.0                    # 0x14030cf20
LASER_RETIRE_START_OFFSET = 640.0            # 0x14030d03c：startOffset ≥ 它即回收
LASER_STATE0_LEN = 0x270                     # 预警时长（CC0 start_time）
LASER_STATE1_LEN = 0x268                     # 有效时长（CC0 duration）

# 池的绝对地址标签
POOL_LABELS = [
    (0x1403EC2A8, "g_Bullets", "Bullet[640], stride 0x620 (BulletManager + 0x08)."),
    (0x1404E12A8, "g_BulletCount", "i32, recomputed every frame by BulletManager__OnUpdate."),
    (0x1404E12B8, "g_Lasers", "Laser[64], stride 0x298 (BulletManager + 0xf5018)."),
]


def validate():
    errors = []
    if BULLETS_OFFSET + BULLET_COUNT * BULLET_SIZE != BULLET_COUNT_FIELD:
        errors.append("bullets[640] 的尾部必须正好接上 bulletCount —— 步长或数量取错了")
    if G_BULLET_MANAGER + LASERS_OFFSET != LASERS_ABS:
        errors.append("g_BulletManager + LASERS_OFFSET 必须等于 SpawnLaserPattern 里的立即数")
    for table, size, what in ((BULLET_FIELDS, BULLET_SIZE, "Bullet"),
                              (LASER_FIELDS, LASER_SIZE, "Laser")):
        offsets = [o for o, _n, _s, _e in table]
        if offsets != sorted(offsets):
            errors.append("%s 字段表未按偏移排序" % what)
        if len(offsets) != len(set(offsets)):
            errors.append("%s 字段表有重复偏移" % what)
        for offset, name, source, evidence in table:
            if source not in ("code", "code-unk", "weak"):
                errors.append("%s: bad source tag" % name)
            if offset >= size:
                errors.append("%s 的 %s 超出结构体大小" % (what, name))
            if not evidence and not name.startswith("unk_"):
                errors.append("%s 没写证据" % name)
    if LASER_IN_USE != dict((n, o) for o, n, _s, _e in LASER_FIELDS)["inUse"]:
        errors.append("LASER_IN_USE 与字段表不一致")
    return errors
