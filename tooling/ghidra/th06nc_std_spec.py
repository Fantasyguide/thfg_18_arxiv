"""TH06NC 的 STD（关卡背景脚本）运行时。

入口 `Stage__OnUpdate` `0x140077bb0`：指令记录 0x14 字节、opcode 是 +0x04 的 i16、
跳表 6 项在 `0x14007826c`。**指令集与 TH06 逐项相同（0..5），没有新增 opcode**——
但 **opcode 3 多了两个魔数参数 `-1` / `-2`**，见 OP3_* 两个标签。

出处：语义对照 CC0 th06-decomp（CC0 1.0，可转发）的 `Stage.cpp` / `Stage.hpp`；
地址与结构一律在 NC 自己的字节上取。TH06 那侧的证伪用 `th06:0x403810` 一手反汇编。
"""

EXPECTED_MD5 = "b0cc28ec904e9efce2439ad857c938b5"

# ⚠️ 上一轮（日志消息迁名）把 0x1400785c0 命名为 Stage__LoadStageData —— **错的**。
# 它是 Stage::AddedCallback，LoadStageData 被内联了进去，所以那条错误消息落在这里。
# 三条独立证据：
#   1. Stage__RegisterChain 把它装进 calc chain 的 addedCallback 槽（0x140509b30）；
#   2. 它是 g_StageFiles(0x1403de540) 的**唯一**引用者，而 CC0 的 LoadStageData
#      是拿两个路径当参数的，不碰那张表；AddedCallback 才索引它；
#   3. TH06 原版两者是分开的两个函数（th06:0x4044c0 与 th06:0x404690）。
# 这是 name-transfer §4.5 那个坑的又一例：**内联者自己不贡献字面量时判据看不见它**。
RENAMES = [
    (0x1400785C0, "Stage__LoadStageData", "Stage__AddedCallback",
     "[NC first-hand] Calc-chain addedCallback: resets the script timer, instruction index, "
     "camera position and fog interp state, then loads g_StageFiles[stage] - the anm through "
     "AnmManager__LoadAnm and the std through 0x1400387a0. CC0's Stage::LoadStageData is "
     "inlined here, which is why this function carries that function's error message. "
     "It was named Stage__LoadStageData by the log-message round; corrected here."),
]

FUNCTIONS = [
    (0x140077BB0, "Stage__OnUpdate",
     "[NC first-hand] STD interpreter, calc chain prio 6. Instruction records are 0x14 bytes "
     "{i32 frame; i16 opcode; i16 size; i32 args[3]}; the opcode at +0x04 indexes the 6-entry "
     "jump table at 0x14007826c. Opcode set is identical to TH06 (0..5); opcode 3 gained two "
     "NC-only magic arguments, see the OP3_ labels."),
    (0x140078290, "Stage__OnDrawHighPrio",
     "[NC first-hand] Draw chain prio 5. Pushes fog colour and fog start/end through the two "
     "cached render-state setters, then draws object z-levels 0 and 1 via Stage__RenderObjects."),
    (0x140078390, "Stage__OnDrawLowPrio",
     "[NC first-hand] Draw chain prio 6. Draws object z-levels 2 and 3, the stage-to-spellcard "
     "dim square, and the spellcard background vm."),
    (0x1400789A0, "Stage__RegisterChain",
     "[NC first-hand] Builds the three ChainElem structs at 0x140509b20 / 0x140509ae0 / "
     "0x140509aa0 (calc prio 6, draw prio 5, draw prio 6), all with arg = g_Stage. "
     "NB the two draw priorities are 5/6 in NC where TH06 uses 3/4."),
    (0x140078C30, "Stage__DeletedCallback",
     "[NC first-hand] Calc-chain deletedCallback, registered by Stage__RegisterChain."),
    (0x140078CB0, "Stage__RenderObjects",
     "[NC first-hand] Draws every stage object whose zLevel equals the second argument; "
     "called twice by each of the two draw callbacks."),
]

GLOBALS = [
    (0x140509B60, "g_Stage",
     "The Stage instance. Passed as the arg of all three chain elements."),
    (0x1403DE540, "g_StageFiles",
     "8 x {char *anmFile; char *stdFile;}, entry 0 is the CC0 dummy pair. "
     "Indexed by the stage number at 0x1404f1e84."),
    (0x14007826C, "g_StageOpcodeJumpTable",
     "6 RVA entries for STD opcodes 0..5, read by Stage__OnUpdate."),
]

# opcode -> (handler 地址, 标签名, 说明)。CC0 的 StageOpcode 枚举名展平。
OPCODES = [
    (0, 0x140077E0C, "STD_OP_CAMERA_POSITION_KEY",
     "frame == -1 seeds the interpolation; otherwise it commits the key, then scans forward "
     "for the next opcode-0 record to pick up the interpolation end frame and target."),
    (1, 0x140077C4F, "STD_OP_FOG",
     "args are {colour, nearPlane, farPlane} -> Stage +0x40/+0x44/+0x48. When the fog interp "
     "duration at +0x90 is zero they are pushed straight to the renderer."),
    (2, 0x140077D03, "STD_OP_CAMERA_FACING",
     "facingDirInterpInitial <- facingDirInterpFinal, then facingDirInterpFinal <- args."),
    (3, 0x140077D39, "STD_OP_CAMERA_FACING_INTERP_LINEAR",
     "args[0] is the interpolation duration - except for the two NC-only magic values, "
     "see the OP3_ labels below."),
    (4, 0x140077CC3, "STD_OP_FOG_INTERP",
     "skyFogInterpInitial <- skyFog, duration <- args[0] at +0x90, timer reset to "
     "{-999, 0} at +0x94/+0x98."),
    (5, 0x140077DD0, "STD_OP_PAUSE",
     "Blocks until the unpause flag at Stage +0x68 is set, then clears it."),
]

# ★ NC 独有：opcode 3 的两个魔数参数。TH06 的同一 case（th06:0x403810）没有这两条分支。
NC_ONLY_LABELS = [
    (0x140077D52, "STD_OP3_ARG_MINUS1__SAVE_LOOP_POINT",
     "NC-only. args[0] == -1: saves the script timer pair at Stage +0xb0, the camera position "
     "at +0xc0/+0xc8 and the *next* instruction index into +0x208 / +0x214 / +0x21c / +0x210. "
     "TH06's opcode 3 has no such branch (th06:0x403810)."),
    (0x140077D8B, "STD_OP3_ARG_MINUS2__JUMP_TO_LOOP_POINT",
     "NC-only. args[0] == -2: restores those four and sets the instruction index back to the "
     "saved one - an unconditional backward jump. Because the script timer is restored too, "
     "the camera motion replays identically, so the pair forms an endless background loop. "
     "The shipped data uses it once: stage7.std instruction 32 (frame 9345, -1) and "
     "instruction 40 (frame 10945, -2)."),
]

# NC 把 TH06 的三条 SetRenderState(D3DRS_FOG*) 换成了两个带脏检查的缓存写入器。
RENDER_HELPER_COMMENTS = [
    (0x1400969C0,
     "[NC first-hand] Cached fog-colour setter: packs 0xff000000 | rgb, and only forwards to "
     "the renderer when the cached value at 0x14050a488 actually changes. Replaces TH06's "
     "SetRenderState(D3DRS_FOGCOLOR, ...)."),
    (0x140096B00,
     "[NC first-hand] Cached fog start/end setter, two floats in XMM0/XMM1, cache at "
     "0x14050a48c / 0x14050a490. Replaces TH06's SetRenderState(D3DRS_FOGSTART/FOGEND, ...)."),
]

STRUCT_DECLS = [
    # NC 侧一手：Stage__OnUpdate 用 *0x14 步进，opcode 是 +0x04 的 i16。
    "struct Th06ncRawStageInstr { int frame; short opcode; short size; int args[3]; };",
]

# Stage 结构体里本轮确证的字段（字节偏移）。NC 重排过，**不要照抄 TH06 的偏移**。
STAGE_FIELDS = [
    (0x024, "instructionIndex", "i32"),
    (0x040, "skyFog.color", "u32"),
    (0x044, "skyFog.nearPlane", "f32"),
    (0x048, "skyFog.farPlane", "f32"),
    (0x058, "skyFogInterpFinal", "StageCameraSky"),
    (0x068, "unpauseFlag", "u8"),
    (0x078, "facingDirInterpDuration", "i32"),
    (0x07C, "facingDirInterpTimer.previous", "i32"),
    (0x080, "facingDirInterpTimer.current", "i32"),
    (0x090, "skyFogInterpDuration", "i32"),
    (0x094, "skyFogInterpTimer.previous", "i32"),
    (0x098, "skyFogInterpTimer.current", "i32"),
    (0x0A8, "stdData", "RawStageHeader *"),
    (0x0B0, "scriptTime.previous", "i32"),
    (0x0B4, "scriptTime.current", "i32"),
    (0x0C0, "position", "D3DXVECTOR3"),
    (0x1F0, "beginningOfScript", "RawStageInstr *"),
    (0x208, "ncLoopSavedScriptTime", "i32[2]"),
    (0x210, "ncLoopSavedInstructionIndex", "i32"),
    (0x214, "ncLoopSavedPosition", "D3DXVECTOR3"),
]


def validate():
    errors = []
    seen = {}
    rows = ([(a, n) for a, n, _c in FUNCTIONS] + [(a, n) for a, n, _c in GLOBALS] +
            [(a, n) for _o, a, n, _c in OPCODES] + [(a, n) for a, n, _c in NC_ONLY_LABELS])
    for address, name in rows:
        if not (0x140000000 <= address < 0x141000000):
            errors.append("%s 的地址不在映像范围内" % name)
        if address in seen:
            errors.append("0x%x 被 %s 和 %s 同时占用" % (address, seen[address], name))
        seen[address] = name
    if [o for o, _a, _n, _c in OPCODES] != [0, 1, 2, 3, 4, 5]:
        errors.append("OPCODES 必须正好覆盖 0..5 —— NC 的上界检查是 opcode < 6")
    for _o, _a, name, _c in OPCODES:
        if not name.startswith("STD_OP_"):
            errors.append("%s 不是 STD_OP_ 前缀" % name)
    for _a, name, _c in NC_ONLY_LABELS:
        if "NC-only" not in _c:
            errors.append("%s 是 NC 独有的分支，注释里必须写明" % name)
    for old, new, _memo in [(r[1], r[2], r[3]) for r in RENAMES]:
        if old == new:
            errors.append("RENAMES 里 %s 的新旧名相同" % old)
    # 注释会写进 Ghidra 并随 th06nc-re-data 发布 —— 必须 ASCII
    for comment in ([c for _a, _n, c in FUNCTIONS] + [c for _a, _n, c in GLOBALS] +
                    [c for _o, _a, _n, c in OPCODES] + [c for _a, _n, c in NC_ONLY_LABELS] +
                    [c for _a, c in RENDER_HELPER_COMMENTS] + [r[3] for r in RENAMES]):
        if not comment.isascii():
            errors.append("注释必须 ASCII：%s" % comment[:40])
    for offset, name, _kind in STAGE_FIELDS:
        if not 0 <= offset < 0x2f4 * 2:
            errors.append("Stage 字段 %s 的偏移 0x%x 不合理" % (name, offset))
    return errors
