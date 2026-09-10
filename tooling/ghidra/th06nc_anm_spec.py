"""Reviewed TH06NC ANM VM symbols for the Ghidra importer.

The TH06 opcode names (0..31) come from the community reconstruction; the
addresses, the 0..35 dispatch bound and the semantics of the four NC-only
opcodes were established first-hand on the Steam New Classic executable
(MD5 b0cc28ec904e9efce2439ad857c938b5).

Evidence and the full derivation live in
``engine/anm/th06nc/01-vm-and-anchor-opcodes.md``.
"""

EXPECTED_MD5 = "b0cc28ec904e9efce2439ad857c938b5"

# `CMP DL,0x23` / `JA` at 0x140006a50 -- the VM accepts opcodes 0..35 and
# routes everything above straight to the advance tail.
MAX_OPCODE = 35
OPCODE_JUMP_TABLE = 0x140007494
ADVANCE_TAIL = 0x1400070E3

# ANM v0 instruction header: {s16 time; u8 opcode; u8 args_size}, args at +4.
INSTR_HEADER_SIZE = 4

# vm-relative field offsets confirmed while reading the interpreter.
VM_STRIDE = 0x120
VM_FIELDS = {
    0x8A: ("pending_interrupt", "s16; 0 means 'no interrupt pending'"),
    0xB8: ("script_time", "current time inside the script"),
    0xC4: ("flags", "u32; bits 8..10 hold the anchor field"),
    0xF0: ("script_start", "base for opcode 5 jumps"),
    0xF8: ("current_instruction", "null once the script exited"),
    0x100: ("sprite", "AnmLoadedSprite pointer used by the draw path"),
}

# The anchor field lives in flags bits 8..10.  TH06 used bit8='x anchored left'
# and bit9='y anchored top'; NC turned it into a value-compared enum, so the
# TH06 bit patterns must NOT be carried over -- only the opcode meanings are.
ANCHOR_FIELD_MASK = 0x700
ANCHOR_FIELD_SHIFT = 8

# field value -> (x alignment, y alignment).  Read off the five draw variants,
# all of which partition the field identically.
ANCHOR_FIELD = {
    0: ("center", "center"),
    1: ("left", "center"),
    2: ("right", "center"),
    3: ("center", "top"),
    4: ("left", "top"),
    5: ("right", "top"),
    6: ("right", "center"),
    7: ("right", "center"),
}

# opcode -> (handler address, AND mask, OR mask, provisional name, field value).
# Every one of bits 8..10 is either force-cleared by the AND or force-set by the
# OR, so these are absolute assignments rather than toggles.
ANCHOR_OPCODES = {
    23: (0x140006FBE, 0xFFFFFCFF, 0x400, "anchorTopLeft", 4),
    32: (0x140007085, 0xFFFFFBFF, 0x300, "anchorTopCenter", 3),
    33: (0x14000709D, 0xFFFFF9FF, 0x100, "anchorLeft", 1),
    34: (0x1400070CD, 0xFFFFFDFF, 0x500, "anchorTopRight", 5),
    35: (0x1400070B5, 0xFFFFFAFF, 0x200, "anchorRight", 2),
}

# Opcodes NC added on top of TH06's 0..31.
NC_ONLY_OPCODES = (32, 33, 34, 35)

# Observed in all 125 shipped .anm files by tooling/anm0_scan.py.  Opcode 33 is
# implemented but never used -- so are TH06's own opcodes 6 and 8, so an unused
# opcode is not by itself a reason to doubt the handler.
SAMPLE_USE_COUNTS = {32: 81, 33: 0, 34: 77, 35: 1}

TH06_OPCODES = tuple(
    """
Exit SetActiveSprite SetScale SetAlpha SetColor Jump Nop FlipX FlipY SetRotation
SetPosition SetScaleSpeed Fade SetBlendAdditive SetBlendDefault ExitHide SetRandomSprite
SetTranslation PosTimeLinear PosTimeDecel PosTimeAccel Stop InterruptLabel Op23 StopHide
Op25 SetAutoRotate Op27 Op28 SetVisibility Op30 Op31
""".split()
)

FUNCTIONS = [
    (
        0x1400069B0,
        "AnmManager__ExecuteScript",
        "[NC first-hand] ANM v0 script interpreter. Instruction header is "
        "{s16 time; u8 opcode; u8 args_size} with args at +4; dispatch is a 36-entry "
        "jump table at 0x140007494 bounded by CMP DL,0x23.",
    ),
    (
        0x1400068F0,
        "AnmManager__SeekInterruptLabel",
        "[NC first-hand] Scans the script for an opcode-22 label matching vm+0x8a, "
        "remembering the -1 (default) label. Opcode 22 is a no-op during execution.",
    ),
    (
        0x140002980,
        "AnmManager__SetActiveSprite",
        "[NC first-hand] Resolves a script-relative sprite index against the manager's "
        "per-anm sprite base table and stores it into vm+0x100.",
    ),
    (
        0x1400020B0,
        "AnmManager__LoadAnm",
        "[NC first-hand] LoadAnm(this, anmIdx, path, spriteIdxOffset). Signature matches "
        "the TH06 community reconstruction parameter for parameter.",
    ),
    (
        0x140002A80,
        "AnmManager__Draw",
        "[NC first-hand] Main draw path. Reads the anchor field (vm+0xc4 & 0x700) and "
        "compares it by value, not by bit -- see the anchor enum.",
    ),
]

GLOBALS = [
    (0x140A6E9B0, "g_AnmManager", "ANM manager instance; confirmed from two independent call sites."),
    (0x140007494, "g_AnmOpcodeJumpTable", "36 RVA entries for ANM opcodes 0..35."),
]

LABELS = [
    (
        0x140006A50,
        "ANM_DISPATCH_BOUND__OPCODE_MAX_35",
        "CMP DL,0x23 / JA: opcodes above 35 fall through to the advance tail. "
        "No shipped .anm uses an opcode >= 36.",
    ),
    (
        0x1400070E3,
        "ANM_ADVANCE_TO_NEXT_INSTR",
        "next = current + 4 + args_size. Also the direct jump-table target for the "
        "execution-time no-ops (opcode 6 nop, opcode 22 interruptLabel).",
    ),
    (
        0x140006D44,
        "ANM_CASE_18_19_20__POS_TIME_INTERP",
        "Shared handler for PosTimeLinear/Decel/Accel; they differ only in flag bits.",
    ),
]

for _op, (_addr, _and, _or, _name, _field) in ANCHOR_OPCODES.items():
    LABELS.append(
        (
            _addr,
            "ANM_CASE_%d__%s" % (_op, _name.upper()),
            "AND [vm+0xc4],0x%08x / OR [vm+0xc4],0x%03x -> anchor field %d = (%s, %s). "
            "Zero-argument, absolute assignment."
            % (_and, _or, _field, ANCHOR_FIELD[_field][0], ANCHOR_FIELD[_field][1]),
        )
    )

STRUCT_DECLS = [
    "struct Th06ncAnmRawInstr { short time; unsigned char opcode; unsigned char args_size; };",
]


def validate():
    errors = []

    if len(TH06_OPCODES) != 32:
        errors.append("TH06 ANM baseline must contain exactly opcodes 0..31")
    if len(set(TH06_OPCODES)) != len(TH06_OPCODES):
        errors.append("duplicate TH06 opcode name")

    if set(NC_ONLY_OPCODES) & set(range(len(TH06_OPCODES))):
        errors.append("NC-only opcodes overlap the TH06 baseline")
    if max(NC_ONLY_OPCODES) != MAX_OPCODE:
        errors.append("the highest NC opcode must equal the dispatch bound")

    for op, (_addr, and_mask, or_mask, _name, field) in ANCHOR_OPCODES.items():
        if or_mask & ~ANCHOR_FIELD_MASK:
            errors.append("opcode %d sets bits outside the anchor field" % op)
        # Each of bits 8..10 must be decided: cleared by AND or set by OR.
        undecided = ANCHOR_FIELD_MASK & and_mask & ~or_mask
        if undecided:
            errors.append("opcode %d leaves anchor bits 0x%x untouched" % (op, undecided))
        if or_mask >> ANCHOR_FIELD_SHIFT != field:
            errors.append("opcode %d field value disagrees with its OR mask" % op)
        if field not in ANCHOR_FIELD:
            errors.append("opcode %d selects an undocumented field value" % op)

    fields = [spec[4] for spec in ANCHOR_OPCODES.values()]
    if len(fields) != len(set(fields)):
        errors.append("two anchor opcodes select the same field value")
    if ANCHOR_FIELD[ANCHOR_OPCODES[23][4]] != ("left", "top"):
        errors.append("opcode 23 must still mean top-left, as in TH06")

    if SAMPLE_USE_COUNTS.get(33) != 0:
        errors.append("opcode 33 is implemented but unused in the shipped data")

    addresses = [address for address, _name, _comment in FUNCTIONS]
    if len(addresses) != len(set(addresses)):
        errors.append("duplicate function address")
    names = [name for _address, name, _comment in FUNCTIONS]
    if len(names) != len(set(names)):
        errors.append("duplicate function name")

    return errors
