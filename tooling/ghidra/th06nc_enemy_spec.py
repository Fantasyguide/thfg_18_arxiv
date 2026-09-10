"""TH06NC 敌人子系统：池、生成、ECL 上下文。

来源：MCP 直查 Ghidra 库（`EnemyManager__SpawnEnemy` 的反汇编、`EclManager__RunEcl`
的调用点），以及 **TH06 原版二进制** `th06:0x00411390` 的逐条对照。

⚠️ opcode 交叉比对得到的那 15 个 `Enemy` 字段仍住在 `th06nc_ecl_spec.py`
（那才是它们的出处）。本文件只放这一轮从生成路径读出来的东西。
合并成一张表是待办，见 engine/enemy/th06nc/01 §6。
"""

EXPECTED_MD5 = "b0cc28ec904e9efce2439ad857c938b5"

# ─────────────────────────────────────────────────────────────────────────────
# 池。三个立即数直接来自 SpawnEnemy 的找空闲槽循环。
ENEMIES_OFFSET = 0x08          # LEA RBX,[RCX + 0x8]
ENEMY_SIZE = 0x10B0            # ADD RBX,0x10b0；memcpy 的 R8D 也是它
ENEMY_COUNT = 0x100            # CMP EAX,0x100
ENEMY_TEMPLATE_OFFSET = 0x10C130   # LEA RDX,[RCX + 0x10c130]，memcpy 的源
ENEMY_SLOT_FLAGS = 0xBC        # CMP byte [RBX+0xbc],0 / JGE —— 符号位=占用

# TH06 原版（th06:0x00411390，一手读过全部指令）
ENEMIES_OFFSET_TH06 = 0xED0
ENEMY_SIZE_TH06 = 0xEC8
ENEMY_COUNT_TH06 = 0x100       # 数量没变
ENEMY_TEMPLATE_OFFSET_TH06 = 0x08
ENEMY_SLOT_FLAGS_TH06 = 0xE50  # SHR CL,7 / AND CL,1 —— 同样是 bit7

# ─────────────────────────────────────────────────────────────────────────────
# ★ NC 给 SpawnEnemy 加了第 7 个参数（TH06 的 RET 0x14 证明它只有 5 个栈参）。
#
# 非空时把 arg7+0x08..+0x3c（14 个 dword）拷进 enemy+0x48..+0x7c。
# arg7 在 ECL VM 的调用点是 R13 = parentEnemy + 0x40 = **父敌人的 EclContext**，
# 而 enemy+0x48 正是子敌人 EclContext 的同一位置 —— 即**同偏移整块拷贝**。
# 那 14 个 dword 就是 ECL 变量区（var0-3 / float0-3 / var4-7 / compareRegister）。
#
# 结论：**NC 的 ENEMYCREATE 让子敌人继承父敌人的 ECL 变量；TH06 不会。**
SPAWN_ARG7_IS_PARENT_CONTEXT = True
ECL_VAR_INHERIT_SRC = 0x08     # 相对 EclContext
ECL_VAR_INHERIT_LEN = 14       # dword 个数
ECL_VAR_INHERIT_DST = 0x48     # 相对 Enemy（= EclContext + 0x08）
SPAWN_CALLSITE_WITH_PARENT = 0x14002856A   # EclManager__RunEcl 的 ENEMYCREATE
SPAWN_CALLSITES_TIMELINE = (0x1400364E2, 0x140036518, 0x140036557,
                            0x140036594, 0x140036763, 0x140036B72)  # 都传 0

# ─────────────────────────────────────────────────────────────────────────────
# NC 的 EnemyEclContext（在 Enemy + 0x40）。三处独立命中：
#   +0x00 ZunTimer  <- SpawnEnemy 写 [enemy+0x40]=-999、[enemy+0x44]=0
#   +0x40 subId     <- SpawnEnemy 写 word [enemy+0x80]
#   +0x48 currentInstr <- SpawnEnemy 写 qword [enemy+0x88]
# 与 CC0 EnemyEclContext（0x4c，x86-32）按 x64 指针/ZunTimer 宽度换算后一致。
ECL_CONTEXT_OFFSET = 0x40
ECL_CONTEXT_FIELDS = [
    (0x00, "time.previous", "code", "SpawnEnemy 写 0xfffffc19"),
    (0x04, "time.current", "code", "SpawnEnemy 写 0"),
    (0x08, "vars", "code", "14 个 dword；ENEMYCREATE 时从父敌人整块拷来"),
    (0x40, "subId", "code", "SpawnEnemy 写 word [enemy+0x80] = eclSubId"),
    (0x48, "currentInstr", "code",
     "SpawnEnemy 写 qword [enemy+0x88]，值取自子表指针数组 0x140a6eb80[subId]"),
]

# ─────────────────────────────────────────────────────────────────────────────
# SpawnEnemy 写到的 Enemy 字段（相对 Enemy）。与 CC0 EnemyManager.cpp:92-122 逐条对照。
SPAWN_WRITES = [
    (0x0B0, "position", "code", "MOVSD 拷 x/y，再单独一条拷 z（+0xb8）"),
    (0x1F4, "primaryVm.color", "code", "上一条的源；TH06 里 primaryVm 在 enemy+0，color 在 +0x7c"),
    (0x234, "life", "code", "life >= 0 时写入；CC0 写了两次，NC 也写两次"),
    (0x238, "maxLife", "code",
     "MOV EAX,[enemy+0x234]; MOV [enemy+0x238],EAX —— **从 weak 升为 code**，"
     "原先只有 opcode 111 一个锚"),
    (0x23C, "unk_23c", "code-unk",
     "紧接着 maxLife 又写了一份同样的 life。**TH06 只写一次**（th06:0x004114a1），"
     "所以这是 NC 多出来的一个字段，用途未知"),
    (0xC50, "score", "code", "score >= 0 时写入"),
    (0xF74, "color", "code",
     "= [enemy+0x1f4]，对应 CC0 的 newEnemy->color = newEnemy->primaryVm.color"),
    (0x10A8, "itemDrop", "code", "byte，来自第 5 个参数"),
]

FUNCTIONS = [
    (
        0x1400360D0,
        "EnemyManager__SpawnEnemy",
        "[NC first-hand] Finds a free slot in enemies[256] (stride 0x10b0, bit 7 of +0xbc marks "
        "occupied), memcpy's the template from manager+0x10c130, then fills in life, position, "
        "the ECL sub, colour, itemDrop and score. NC adds a 7th parameter TH06 does not have: "
        "when non-null it copies 14 dwords of ECL variables from the parent enemy's context.",
    ),
]

# EnemyManager 的绝对基址本轮未定位（SpawnEnemy 只用 this 指针），所以不落全局标签。
POOL_LABELS = []


def validate():
    errors = []
    if ENEMY_COUNT != ENEMY_COUNT_TH06:
        errors.append("池容量两边应当相同（都是 256）")
    if ENEMY_SIZE <= ENEMY_SIZE_TH06:
        errors.append("NC 的 Enemy 应当比 TH06 大")
    # 池尾必须在模板之前，且中间留得下已定位的 spellcardInfo（+0x10c0d0）
    pool_end = ENEMIES_OFFSET + ENEMY_COUNT * ENEMY_SIZE
    if pool_end > ENEMY_TEMPLATE_OFFSET:
        errors.append("enemies[] 覆盖到了模板 —— 步长或数量取错了")
    if not (pool_end <= 0x10C0D0 < ENEMY_TEMPLATE_OFFSET):
        errors.append("spellcardInfo(+0x10c0d0) 应当落在池尾与模板之间")
    # ECL 变量继承：源与目标必须是同一个 context 偏移
    if ECL_CONTEXT_OFFSET + ECL_VAR_INHERIT_SRC != ECL_VAR_INHERIT_DST:
        errors.append("父子拷贝必须落在 EclContext 的同一偏移上")
    for table, what in ((ECL_CONTEXT_FIELDS, "EclContext"), (SPAWN_WRITES, "SpawnEnemy")):
        offsets = [o for o, _n, _s, _e in table]
        if offsets != sorted(offsets):
            errors.append("%s 表未按偏移排序" % what)
        for _o, name, source, evidence in table:
            if source not in ("code", "code-unk", "weak"):
                errors.append("%s: bad source tag" % name)
            if source in ("code-unk", "weak") and not name.startswith("unk_"):
                errors.append("%s 语义未定却用了确定的名字" % name)
            if not evidence:
                errors.append("%s 没写证据" % name)
    for _a, name, comment in FUNCTIONS:
        if not comment.isascii():
            errors.append("%s 的注释会写进 Ghidra 并发布，必须 ASCII" % name)
    return errors
