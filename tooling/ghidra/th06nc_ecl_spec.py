"""Reviewed TH06NC ECL symbols and types used by the Ghidra importer.

The 0..135 opcode names come from the CC0 GensokyoClub/th06 reconstruction,
but the addresses and NC deltas below were independently checked against the
Steam launch executable (MD5 b0cc28ec904e9efce2439ad857c938b5).
"""

EXPECTED_MD5 = "b0cc28ec904e9efce2439ad857c938b5"

FUNCTIONS = [
    (
        0x140022060,
        "EclManager__RunEcl",
        "[TH06 baseline + NC verified] Per-enemy ECL interpreter. The record layout, "
        "difficulty mask, variable encoding, time gate, and offset-to-next loop match TH06.",
    ),
    (
        0x1400360D0,
        "EnemyManager__SpawnEnemy",
        "[TH06 baseline + NC verified] Allocates an enemy slot, initializes its ECL context, "
        "selects a subroutine from g_EclSubTable, and runs its first ECL frame.",
    ),
    (
        0x140036260,
        "EnemyManager__RunEclTimeline",
        "[TH06 baseline + NC verified] Stage timeline interpreter. Runtime dispatch remains "
        "limited to opcodes 0..12 and advances by the int16 size field at +0x06.",
    ),
]

GLOBALS = [
    (0x140A6EB70, "g_EclManager", "TH06NC ECL manager, verified from loader and VM references."),
    (0x140A6EB78, "g_EclFile", "Loaded ECL file buffer (g_EclManager + 0x08)."),
    (0x140A6EB80, "g_EclSubTable", "Relocated ECL subroutine pointer table (+0x10)."),
    (0x140A6EB88, "g_EclTimelineCheckpoint", "Opcode-13 timeline checkpoint selected by the loader (+0x18)."),
    (0x140A6EB90, "g_EclTimelineCheckpointPending", "Requests use of the saved timeline checkpoint (+0x20)."),
]

# 主分派：index = opcode - 1，上界 CMP EAX,0xc8；字节映射 + 跳表。
DISPATCH_BYTE_MAP = 0x140028F5C
DISPATCH_JUMP_TABLE = 0x140028D78
OPCODE_RANGE = (1, 201)
DISTINCT_JUMP_TARGETS = 121      # 含默认出口。TH06 同口径是 120，多的那个就是 opcode 200 的
REAL_HANDLERS = 120              # 去掉默认出口后真正有实现的 handler 数

# 消费 aim_mode 的一侧。
BULLET_PROPS_OFFSET = 0x26C      # enemy + 0x26c
BULLET_PROPS_SPRITE = 0x2C       # bullet_props + 0x2c
BULLET_PROPS_AIM_MODE = 0x30     # bullet_props + 0x30，u16
SPAWN_BULLET = 0x14000F870       # switch(aim_mode) 只实现 0..8，无 default
AIM_MODE_IMPLEMENTED = tuple(range(9))
ENEMY_SHOOT_OFFSET = 0xC5C       # enemy + 0xc5c，opcode 200 写这里

LABELS = [
    (0x1400221B1, "ECL_OPCODE_DISPATCH", "Main opcode dispatch; encoded table accepts IDs 1..201."),
    (
        0x140025371,
        "ECL_CASE_67_75_201__BULLET_PATTERN",
        "Shared bullet-pattern handler for opcodes 67..75 and 201 (jump-table slot 60). "
        "Stores aim_mode = opcode - 67 into bullet_props+0x30, so opcode 201 selects 134 -- "
        "a mode the consumer at 0x14000f870 does not implement (its switch covers 0..8 with "
        "no default), leaving the angle at its pre-switch 0.0. Settled, not provisional.",
    ),
    (
        0x140026A41,
        "ECL_CASE_200__SHOOT_OFFSET_POLAR",
        "NC opcode 200, the one genuinely new ECL instruction: a polar form of TH06's "
        "SHOOTOFFSET (opcode 81). Writes enemy+0xc5c = (cos(arg0)*arg1, sin(arg0)*arg1, 0); "
        "the z zero comes from R12D, cleared by XOR R12D,R12D at 0x14002213f. arg2 unused.",
    ),
    (0x140036492, "ECL_TIMELINE_OPCODE_DISPATCH", "Timeline dispatch accepts only opcodes 0..12."),
    (0x14003B490, "ECL_SCAN_TIMELINE_OPCODE_13", "Loader scan for an opcode-13 timeline checkpoint."),
]

TH06_OPCODES = tuple(
    """
NOP UNIMP JUMP JUMPDEC SETINT SETFLOAT SETINTRAND SETINTRANDMIN SETFLOATRAND SETFLOATRANDMIN
SETVARSELFX SETVARSELFY SETVARSELFZ MATHINTADD MATHINTSUB MATHINTMUL MATHINTDIV MATHINTMOD MATHINC MATHDEC
MATHFLOATADD MATHFLOATSUB MATHFLOATMUL MATHFLOATDIV MATHFLOATMOD MATHATAN2 MATHNORMANGLE CMPINT CMPFLOAT JUMPLSS
JUMPLEQ JUMPEQU JUMPGRE JUMPGEQ JUMPNEQ CALL RET CALLLSS CALLLEQ CALLEQU CALLGRE CALLGEQ CALLNEQ
MOVEPOSITION MOVEAXISVELOCITY MOVEVELOCITY MOVEANGULARVELOCITY MOVESPEED MOVEACCELERATION MOVERAND MOVERANDINBOUND
MOVEATPLAYER MOVEDIRTIMEDECELERATE MOVEDIRTIMEDECELERATEFAST MOVEDIRTIMEACCELERATE MOVEDIRTIMEACCELERATEFAST
MOVEPOSITIONTIMELINEAR MOVEPOSITIONTIMEDECELERATE MOVEPOSITIONTIMEDECELERATEFAST MOVEPOSITIONTIMEACCELERATE
MOVEPOSITIONTIMEACCELERATEFAST MOVETIMEDECELERATE MOVETIMEDECELERATEFAST MOVETIMEACCELERATE MOVETIMEACCELERATEFAST
MOVEBOUNDSSET MOVEBOUNDSDISABLE BULLETFANAIMED BULLETFAN BULLETCIRCLEAIMED BULLETCIRCLE BULLETOFFSETCIRCLEAIMED
BULLETOFFSETCIRCLE BULLETRANDOMANGLE BULLETRANDOMSPEED BULLETRANDOM SHOOTINTERVAL SHOOTINTERVALDELAYED SHOOTDISABLED
SHOOTENABLED SHOOTNOW SHOOTOFFSET BULLETEFFECTS BULLETCANCEL BULLETSOUND LASERCREATE LASERCREATEAIMED LASERINDEX
LASERROTATE LASERROTATEFROMPLAYER LASEROFFSET LASERTEST LASERCANCEL SPELLCARDSTART SPELLCARDEND ENEMYCREATE ENEMYKILLALL
ANMSETMAIN ANMSETPOSES ANMSETSLOT ANMSETDEATH BOSSSET SPELLCARDEFFECT ENEMYSETHITBOX ENEMYFLAGCOLLISION
ENEMYFLAGCANTAKEDAMAGE EFFECTSOUND ENEMYFLAGDEATH DEATHCALLBACKSUB ENEMYINTERRUPTSET ENEMYINTERRUPT ENEMYLIFESET
BOSSTIMERSET LIFECALLBACKTHRESHOLD LIFECALLBACKSUB TIMERCALLBACKTHRESHOLD TIMERCALLBACKSUB ENEMYFLAGINTERACTABLE
EFFECTPARTICLE DROPITEMS ANMFLAGROTATION EXINSCALL EXINSREPEAT TIMESET DROPITEMID STDUNPAUSE BOSSSETLIFECOUNT DEBUGWATCH
ANMINTERRUPTMAIN ANMINTERRUPTSLOT ENEMYFLAGDISABLECALLSTACK BULLETRANKINFLUENCE ENEMYFLAGINVISIBLE BOSSTIMERCLEAR
LASERCLEARALL SPELLCARDFLAGTIMEOUT
""".split()
)

NC_ECL_DELTAS = {
    200: (
        "SHOOTOFFSETPOLAR",
        "Verified math and destination fields; name is provisional until all call-site semantics are documented.",
    ),
    201: (
        "BULLETPATTERNMODE134",
        "Settled: NOT a new pattern. Shares the 67..75 handler (jump-table slot 60) and stores "
        "aim_mode = opcode - 67 = 134 into bullet_props+0x30, but the consumer's switch in "
        "0x14000f870 only implements modes 0..8 and has no default, so the angle stays at its "
        "pre-switch 0.0. Nothing anywhere compares that field against 134. The mode is unimplemented.",
    ),
}

NC_TIMELINE_DELTAS = {
    13: ("CHECKPOINT", "Consumed by the ECL loader, outside the runtime timeline switch."),
    14: ("MARKER14", "Argument-free no-op marker in the current runtime; observed once in ecldata7.ecl."),
}

# ─────────────────────────────────────────────────────────────────────────────
# 通过 ECL opcode 交叉比对定位到的子系统函数。
#
# 方法：CC0 的 th06-decomp 源码给出「每个 opcode 调用哪些函数」；我们从 NC 二进制
# 解出「每个 opcode 的 handler 调用哪些地址」。两边用 opcode 号当共同坐标系，
# 取**双向唯一**的匹配。全程不经过 th-re-data / th06-re（两者都没有 LICENSE），
# 名字直接采 CC0 上游的写法，`::` 展平成 `__`。
#
# 自证：EnemyManager__SpawnEnemy 是我们先前独立命名的，算法在不知情下把它配到
# CC0 的同名函数——这是方法在已知答案上的一次正确复现。
#
# ⚠️ 局限：NC 内联得比 TH06 激进（`EnemyEclInstr::GetVar` 在 NC 里没有独立函数，
# 被内联进 25 个 handler），所以召回率天然偏低。**配上的可信，配不上不代表不同。**
#
# 每项 = (地址, 名字, 触发它的 opcode, 置信度)
ECL_CALLEE_NAMES = [
    (0x14002E260, "EnemyEclInstr__GetVarFloat",
     (8, 9, 28, 43, 44, 45, 46, 47, 48, 51, 67, 68, 69, 70, 71, 72, 73, 74, 75,
      78, 82, 87, 88, 89, 90, 118), "exact"),
    (0x14002E620, "EnemyEclInstr__SetVar",
     (3, 4, 5, 6, 7, 9, 10, 11, 12, 26), "near"),      # CC0 还含 opcode 8，NC 那条疑似内联
    (0x14002E0A0, "EnemyEclInstr__MovePosTime", (56, 57, 58, 59, 60), "exact"),
    (0x14002DFC0, "EnemyEclInstr__MoveDirTime", (52, 53, 54, 55), "exact"),
    (0x14002E180, "EnemyEclInstr__MoveTime", (61, 62, 63, 64), "exact"),
    (0x14002ECF0, "EnemyEclInstr__MathAdd", (13, 20), "exact"),
    (0x14002F770, "EnemyEclInstr__MathSub", (14, 21), "exact"),
    (0x1400301F0, "EnemyEclInstr__MathMul", (15, 22), "exact"),
    (0x1400310F0, "EnemyEclInstr__MathDiv", (16, 23), "exact"),
    (0x140031B70, "EnemyEclInstr__MathMod", (17, 24), "exact"),
    (0x1400325F0, "EnemyEclInstr__MathAtan2", (25,), "exact"),
    (0x14000F870, "BulletManager__SpawnBulletPattern",
     (67, 68, 69, 70, 71, 72, 73, 74, 75, 80), "near"),  # NC 侧多了个 opcode 1，疑似区间归属误差
    (0x1400105E0, "BulletManager__SpawnLaserPattern", (85, 86), "exact"),
    (0x14000EF00, "BulletManager__TurnAllBulletsIntoPoints", (83, 93), "exact"),
    (0x14002B0D0, "EffectManager__SpawnParticles", (102, 118), "exact"),
    (0x1400426E0, "ItemManager__SpawnItem", (119, 124), "exact"),
]

# ─────────────────────────────────────────────────────────────────────────────
# TH06NC 的 Enemy 结构体字段偏移（部分）。
#
# ⚠️ **NC 重排了这个结构体**：CC0 的 TH06 `Enemy` 把 9 个 AnmVm 放在最前面
# （primaryVm + vms[8]），position 排在很后面；NC 的 position 在 +0xb0，
# 而 bulletProps 在 +0x26c。**字段顺序不能从 TH06 推断，必须逐个验。**
#
# 两种来源，置信度不同：
#   "code" —— 直接从指令读出来的（最硬）
#   "xmatch" —— opcode 交叉比对得到、且有 ≥2 个 opcode 支撑
#   "weak"  —— 只有 1 个 opcode 支撑，**仅供线索，别当结论**
#
# 每项 = (偏移, 字段名(CC0 写法), 来源, 证据)
ENEMY_FIELDS = [
    (0x0A0, "lifeCallbackThreshold", "weak", "opcode 113"),
    (0x0A4, "lifeCallbackSub", "weak", "opcode 114"),
    (0x0B0, "position", "code", "ADDSS XMM0,[RDI+0xb0] 与 +0xb4/+0xb8，三个 f32"),
    (0x0BC, "flags", "code", "TEST byte ptr [RDI+0xbc],0x20"),
    (0x0C0, "laserProps", "xmatch", "opcodes 85, 86；+0xc0 起一片连续偏移"),
    (0x230, "bossId", "weak", "opcode 101"),
    (0x238, "maxLife", "weak", "opcode 111"),
    (0x26C, "bulletProps", "code", "LEA R12,[RDI+0x26c] 后作为实参传给 SpawnBulletPattern"),
    (0x2C0, "hitboxDimensions", "weak", "opcode 103；+0x2c0/+0x2c4/+0x2c8"),
    (0xBD8, "laserStore", "xmatch", "opcodes 85, 86, 87"),
    (0xBE0, "deathCallbackSub", "xmatch", "opcodes 108, 133"),
    (0xC54, "timerCallbackThreshold", "weak", "opcode 115"),
    (0xC58, "timerCallbackSub", "xmatch", "opcodes 116, 133"),
    (0xC5C, "shootOffset", "code", "三个 f32 加到 position 上写进 bulletProps.position"),
    (0xF70, "runInterrupt", "weak", "opcode 110"),
]

# NC 的 EnemyBulletShooter 也和 TH06 不同（TH06: sprite +0x00 / aimMode +0x48）。
# 这些是相对 bulletProps 基址的偏移。
BULLET_PROPS_FIELDS = [
    (0x0C, "position", "code", "写 +0x0c/+0x10/+0x14 = enemy.position + shootOffset"),
    (0x18, "count1", "code", "MOV word ptr [R12+0x18]，rank 缩放后写入"),
    (0x2C, "sprite", "code", "指令首个 u16 参数"),
    (0x30, "aimMode", "code", "SUB AX,0x43 的结果；消费者 switch 的变量"),
]

STRUCT_DECLS = [
    "struct Th06ncEclRawHeader { short sub_count; short main_count; unsigned int timeline_offsets[3]; };",
    "struct Th06ncEclRawInstrHeader { int time; short opcode; short offset_to_next; "
    "unsigned char unk_08; unsigned char skip_for_difficulty; unsigned char unk_0a; unsigned char unk_0b; };",
    "struct Th06ncEclTimelineInstr { short time; short arg0; short opcode; short size; };",
    "struct Th06ncEclManager { struct Th06ncEclTimelineInstr *timeline; struct Th06ncEclRawHeader *ecl_file; "
    "struct Th06ncEclRawInstrHeader **sub_table; struct Th06ncEclTimelineInstr *timeline_checkpoint; "
    "unsigned char timeline_checkpoint_pending; unsigned char _pad_21[7]; };",
]


def validate():
    errors = []
    if len(TH06_OPCODES) != 136:
        errors.append("TH06 opcode baseline must contain exactly IDs 0..135")
    function_addresses = [address for address, _name, _comment in FUNCTIONS]
    if len(function_addresses) != len(set(function_addresses)):
        errors.append("duplicate function address")
    names = [name for _address, name, _comment in FUNCTIONS]
    if len(names) != len(set(names)):
        errors.append("duplicate function name")
    if set(NC_ECL_DELTAS) & set(range(len(TH06_OPCODES))):
        errors.append("NC ECL deltas overlap the TH06 baseline")
    # 201 曾长期挂着「provisional」；现在已收口成「模式未实现」。这条闸门跟着换成
    # 更具体的要求：别把它写回「新弹型」。
    note201 = NC_ECL_DELTAS[201][1].lower()
    if "unimplemented" not in note201:
        errors.append("opcode 201 must state that its aim_mode is unimplemented")
    if "0..8" not in NC_ECL_DELTAS[201][1]:
        errors.append("opcode 201 must record the 0..8 bound of the consuming switch")

    # 交叉比对得到的子系统函数：每条都要带支撑它的 opcode 和置信度，
    # 否则以后没人能判断该信它几分。
    seen = {}
    for address, name, ops_, confidence in ECL_CALLEE_NAMES:
        if confidence not in ("exact", "near"):
            errors.append("%s: confidence must be 'exact' or 'near'" % name)
        if not ops_:
            errors.append("%s: no supporting opcodes recorded" % name)
        if any(op not in range(1, 202) for op in ops_):
            errors.append("%s: opcode out of the 1..201 dispatch range" % name)
        if address in seen:
            errors.append("duplicate callee address 0x%x" % address)
        seen[address] = name
    if len(set(seen.values())) != len(seen):
        errors.append("duplicate callee name")

    # 结构体偏移：每条都要标来源，weak 的不许悄悄升格
    for table, label in ((ENEMY_FIELDS, "Enemy"), (BULLET_PROPS_FIELDS, "EnemyBulletShooter")):
        offsets = [o for o, _n, _s, _e in table]
        if offsets != sorted(offsets):
            errors.append("%s 字段表必须按偏移升序" % label)
        if len(offsets) != len(set(offsets)):
            errors.append("%s 字段表有重复偏移" % label)
        for offset, name, source, evidence in table:
            if source not in ("code", "xmatch", "weak"):
                errors.append("%s.%s: 来源必须是 code/xmatch/weak" % (label, name))
            if not evidence:
                errors.append("%s.%s: 没有记证据" % (label, name))

    return errors
