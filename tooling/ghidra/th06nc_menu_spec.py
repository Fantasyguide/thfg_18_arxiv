"""Reviewed TH06NC result-screen unlock-cheat symbols for the Ghidra importer.

Everything here was read first-hand off the Steam New Classic executable
(MD5 b0cc28ec904e9efce2439ad857c938b5).  The TH06 community decompilation was
used only to cross-check semantics (sound ids, CLRD record shape); no address
or structure was carried over from it.

Evidence and the full derivation live in
``engine/menu/th06nc/02-classiclager-unlock-cheat.md``.
"""

EXPECTED_MD5 = "b0cc28ec904e9efce2439ad857c938b5"

# The 12-letter password, and the 8 distinct letters it is built from.  The
# on-disk decode table at 0x14030c520 stores one {int ch; int dik;} pair per
# bit, in this order.
CHEAT_PASSWORD = "classiclager"
CHEAT_KEY_TABLE_ADDRESS = 0x14030C520
CHEAT_PASSWORD_ADDRESS = 0x14030C560

# bit -> (letter, DirectInput scancode).  Verified twice: the literal table
# bytes, and the constant-folded g_KeyRemap addresses inside 0x14006e190
# (0x140c21e60 + dik).
CHEAT_KEY_BITS = (
    ("c", 0x2E),
    ("l", 0x26),
    ("a", 0x1E),
    ("s", 0x1F),
    ("i", 0x17),
    ("g", 0x22),
    ("e", 0x12),
    ("r", 0x13),
)

# 0x140c21e58 is a gamepad bitmap, NOT the keyboard/logical input bitmap.
# Indices are XInput wButtons bit positions; see the doc for the three
# independent confirmations (d-pad, A/B swap flag, trigger threshold).
PAD_CHEAT_BITS = (
    (0x01, "XINPUT_GAMEPAD_LEFT_THUMB"),
    (0x02, "XINPUT_GAMEPAD_RIGHT_THUMB"),
    (0x04, "XINPUT_GAMEPAD_X"),
    (0x08, "XINPUT_GAMEPAD_Y"),
    (0x10, "XINPUT_GAMEPAD_LEFT_SHOULDER"),
    (0x20, "XINPUT_GAMEPAD_RIGHT_SHOULDER"),
)

# (step, advance mask, abort mask).  Steps 0 and 3 additionally require both
# bits of the advance mask to be held down in the same frame.
PAD_CHEAT_STEPS = (
    (0, 0x03, 0x00),
    (1, 0x08, 0x34),
    (2, 0x04, 0x38),
    (3, 0x30, 0x0C),
)

# Result-screen state ids.  state = menu cursor + 3; the menu entry order was
# confirmed by cropping sprite1..sprite5 out of data/result/result00.png.
RESULT_SCREEN_STATES = {
    3: "EASY_RECORDS",
    4: "NORMAL_RECORDS",
    5: "HARD_RECORDS",
    6: "LUNATIC_RECORDS",
    7: "EXTRA_RECORDS",
}
CHEAT_GATE_STATE = 6

FUNCTIONS = [
    (
        0x14006E190,
        "ResultScreen__UpdateKeyboardUnlockCheat",
        "[NC first-hand] Types-the-password cheat. Edge-detects the 8 letters of the "
        "0x14030c520 table, walks \"classiclager\" via +0x4430, and on the 12th letter "
        "fills the CLRD table with 99. Wrong letter resets to 0, 'c' restarts at 1.",
    ),
    (
        0x14006E020,
        "ResultScreen__UpdateUnlockCheats",
        "[NC first-hand] Runs the keyboard cheat, then an inlined 4-step XInput pad "
        "cheat over the 0x140c21e58 bitmap (hold L3+R3, Y, X, hold LB+RB). Either "
        "success sets the score.dat dirty flag at +0x9e99.",
    ),
    (
        0x14006E3A0,
        "ResultScreen__OnUpdate",
        "[NC first-hand] Result-screen per-frame tick. Switches on +0x9e94 through the "
        "18-entry jump table at 0x140072824; only state 6 (Lunatic records) reaches the "
        "cheat call at 0x14006fab4.",
    ),
    (
        0x140012BE0,
        "Input__CollectKeyboard",
        "[NC first-hand] Rebuilds the layout-aware scancode remap table at 0x140c21e60 "
        "via GetKeyboardLayout/MapVirtualKeyExW, then feeds the logical button bitmap "
        "builder. This is why the cheat is typed as letters, not key positions.",
    ),
    (
        0x1400127D0,
        "Input__PublishButtonBitmap",
        "[NC first-hand] Merges keyboard bits with XInput pad state and writes the "
        "6-bit pad bitmap g_PadCheatButtons (0x140c21e58).",
    ),
    (
        0x140074470,
        "ResultScreen__WriteScoreDat",
        "[NC first-hand] Serializes HSCR/CLRD/CATK/PSCR and writes score.dat. The whole "
        "body is gated on the +0x9e99 dirty flag, which the unlock cheats set.",
    ),
]

GLOBALS = [
    (0x14030C520, "g_ResultCheatKeyTable", "8 x {int ch; int dik;} decode table for the classiclager cheat."),
    (0x14030C560, "g_ResultCheatPassword", 'ASCII "classiclager" (13 bytes with NUL).'),
    (0x140C21E60, "g_KeyRemap", "256-byte layout-aware scancode remap table, rebuilt when the HKL changes."),
    (0x140C21F70, "g_KeyState", "256-byte keyboard down-state array."),
    (0x140C21E58, "g_PadCheatButtons", "6-bit XInput button bitmap consumed by the pad unlock cheat."),
    (0x140C21DBA, "g_PadSwapConfirmCancel", "Non-zero swaps the A/B confirm/cancel pad buttons."),
    (0x1403081C0, "g_RemappedScancodes", "The 0x25 scancodes pushed through MapVirtualKeyExW."),
    (0x1404FF0F4, "g_ClrdRecords", "Four 0x18-byte CLRD records; clear flags start at +0x0c."),
    (0x1404FF100, "g_ClrdRecord0Flags", "CLRD[0] clear flags: 5 with-retries + 5 without-retries bytes."),
    (0x140509670, "g_SoundRequestQueue", "Three int slots; negative means free. Cheat enqueues 28 (SOUND_1UP)."),
    (0x140509694, "g_SoundRequestBlocked", "Non-zero skips sound enqueueing; exact meaning still unconfirmed."),
    (0x140072824, "g_ResultScreenStateJumpTable", "18 RVA entries; index is ResultScreen +0x9e94."),
]

LABELS = [
    (
        0x14006FAB4,
        "RESULT_STATE_6_LUNATIC__UNLOCK_CHEAT",
        "Jump-table target for state 6 (Lunatic records). Only this state calls the "
        "unlock cheats; states 3/4/5/7 enter the shared handler directly.",
    ),
    (
        0x14006FAC3,
        "RESULT_STATE_3_TO_7__SHARED_RECORDS",
        "Shared records-page handler for result-screen states 3..7.",
    ),
    (
        0x14006E2DC,
        "RESULT_CHEAT__FILL_CLRD_WITH_99",
        "Writes 10 bytes of 99 at 0x1404ff100 + n*0x18 for n in 0..3.",
    ),
    (
        0x14006E30E,
        "RESULT_CHEAT__ENQUEUE_SOUND_1UP",
        "Enqueues sound id 0x1c (SOUND_1UP in the TH06 numbering) into g_SoundRequestQueue.",
    ),
]

STRUCT_DECLS = [
    "struct Th06ncCheatKey { int ch; int dik; };",
    "struct Th06ncXInputPadState { unsigned char buttons[16]; unsigned char left_trigger; "
    "unsigned char right_trigger; short thumb_lx; short thumb_ly; short thumb_rx; short thumb_ry; };",
]


def validate():
    errors = []

    if len(CHEAT_KEY_BITS) != 8:
        errors.append("cheat key table must have exactly 8 entries (one per bitmap bit)")

    letters = [letter for letter, _dik in CHEAT_KEY_BITS]
    if len(letters) != len(set(letters)):
        errors.append("duplicate letter in the cheat key table")
    if set(letters) != set(CHEAT_PASSWORD):
        errors.append("cheat key table letters must be exactly the distinct letters of the password")

    if CHEAT_GATE_STATE not in RESULT_SCREEN_STATES:
        errors.append("cheat gate state is not a known result-screen state")
    if RESULT_SCREEN_STATES[CHEAT_GATE_STATE] != "LUNATIC_RECORDS":
        errors.append("cheat gates on the Lunatic records page, not Extra as in TH06")

    steps = [step for step, _advance, _abort in PAD_CHEAT_STEPS]
    if steps != [0, 1, 2, 3]:
        errors.append("pad cheat steps must be 0..3 in order")
    pad_mask = 0
    for _bit, _name in PAD_CHEAT_BITS:
        pad_mask |= _bit
    for step, advance, abort in PAD_CHEAT_STEPS:
        if advance & ~pad_mask or abort & ~pad_mask:
            errors.append("pad cheat step %d touches a bit outside the 6-bit pad bitmap" % step)
        if advance & abort:
            errors.append("pad cheat step %d has an overlapping advance/abort mask" % step)

    addresses = [address for address, _name, _comment in FUNCTIONS]
    if len(addresses) != len(set(addresses)):
        errors.append("duplicate function address")
    names = [name for _address, name, _comment in FUNCTIONS]
    if len(names) != len(set(names)):
        errors.append("duplicate function name")

    blocked = dict((address, comment) for address, _name, comment in GLOBALS).get(0x140509694, "")
    if "unconfirmed" not in blocked.lower():
        errors.append("g_SoundRequestBlocked semantics must stay explicitly unconfirmed")

    return errors
