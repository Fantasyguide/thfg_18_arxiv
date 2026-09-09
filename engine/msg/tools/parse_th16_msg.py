#!/usr/bin/env python3
"""
parse_th16_msg.py — TH16 .msg 解析器(结局/staff/对话通用),验证 msg/02 + msg/04 的结论。

用法:
    python3 parse_th16_msg.py <file.msg> [--kind ending|stage]

- 不含任何版权字节;输入的 .msg 是游戏资产(版权,用户本地提供,勿入库)。
- 指令格式 zMsgRawInstr:{int16 time, uint8 opcode, uint8 args_size, args[args_size]}(见 msg/01)。
- 文件头:u32 count, 然后(对结局 e/staff)u32 script0_offset @+4;脚本从该偏移起(见 msg/04 §1.5)。
- 文本编码:Shift-JIS + 自 TH09 的"加速 XOR 掩码"(init 0x77 / vel 0x07 / acc 0x10,逐字节含 padding;见 msg/02 §1)。

opcode→名 表来自 msg/02(stage)与 msg/04(ending/staff);ending 命名为我们一手提案(无社区对名)。
"""
import struct
import argparse

# ending/staff 指令集(msg/04;命名=我们提案,behavior 一手+e01 实测)
ENDING_OPS = {
    0: "end", 3: "text_line", 4: "text_clear", 5: "wait", 6: "wait_page",
    7: "load_anm_present", 8: "show_image", 9: "set_text_color",
    10: "music_by_name", 11: "music_fade", 12: "goto_staff_roll",
    13: "screen_effect_d", 14: "screen_effect_e",
    15: "show_image_d1", 16: "show_image_d2", 17: "show_image_d3",
}

# stage(关卡对话)指令集(msg/02 + th18/01;名 = ExpHP thpages)
STAGE_OPS = {
    0: "end", 1: "show_player", 2: "show_enemy", 3: "show_textbox", 4: "hide_player",
    5: "hide_enemy", 6: "hide_textbox", 7: "focus_player", 8: "focus_enemy", 9: "focus_none",
    10: "skippable", 11: "pause", 12: "ecl_resume", 13: "face_player", 14: "face_enemy",
    15: "text_1", 16: "text_2", 17: "text_add", 18: "text_clear", 19: "music_boss",
    20: "intro", 21: "stage_end", 22: "music_fade", 23: "shake_player", 24: "shake_enemy",
    25: "y_offset", 26: "modern_26", 27: "music_fade_custom", 28: "callout_pos",
    29: "callout_type", 30: "route_select", 31: "show_enemy_1", 32: "modern_32",
    33: "darken_portrait", 34: "highlight_portrait", 35: "lights_out", 36: "store",
}
STAGE_TEXT_OPS = {15, 16, 17}
STAGE_FLOAT_OPS = {27, 28}

# 哪些 opcode 的实参整体是一条加密 SJIS 文本串(其余按原始字节展示)
ENDING_TEXT_OPS = {3}
# 哪些 opcode 的实参是 {u32, 然后明文 ascii 文件名/路径串}
ENDING_STR_AT8 = {7}          # {idx, filename@+8}
ENDING_STR_AT4 = {10, 12}     # music name / staff filename @+4


def dexor(b: bytes) -> bytes:
    """加速 XOR 掩码解密(init 0x77, vel 0x07, acc 0x10)。"""
    out = bytearray()
    mask, vel = 0x77, 0x07
    for c in b:
        out.append(c ^ mask)
        mask = (mask + vel) & 0xff
        vel = (vel + 0x10) & 0xff
    return bytes(out)


def decode_text(args: bytes) -> str:
    s = dexor(args).split(b"\x00", 1)[0]
    try:
        return s.decode("shift_jis")
    except UnicodeDecodeError:
        return repr(s)


def ascii_str(b: bytes) -> str:
    return b.split(b"\x00", 1)[0].decode("ascii", "replace")


def parse_script(data: bytes, start: int, end: int, kind: str) -> int:
    """从 start 解到 opcode 0 或 end,返回结束位置。"""
    ops = STAGE_OPS if kind == "stage" else ENDING_OPS
    text_ops = STAGE_TEXT_OPS if kind == "stage" else ENDING_TEXT_OPS
    p = start
    n = 0
    while p + 4 <= end:
        time = struct.unpack_from("<H", data, p)[0]   # u16(run 里按 ushort 读,见 th18/01 §1)
        op = data[p + 2]
        size = data[p + 3]
        args = data[p + 4:p + 4 + size]
        name = ops.get(op, f"op{op}")
        detail = ""
        if op in text_ops and size:
            detail = "  text=" + repr(decode_text(args))
        elif kind == "ending" and op in ENDING_STR_AT8 and size > 4:
            idx = struct.unpack_from("<I", args, 0)[0]
            detail = f"  {{idx={idx}, name={ascii_str(args[4:])!r}}}"
        elif kind == "ending" and op in ENDING_STR_AT4 and size:
            detail = f"  name={ascii_str(args)!r}"
        elif kind == "stage" and op in STAGE_FLOAT_OPS and size in (4, 8):
            detail = "  " + repr(struct.unpack_from("<%df" % (size // 4), args, 0))
        elif size in (4, 8, 12) and size:
            ints = struct.unpack_from("<%di" % (size // 4), args, 0)
            detail = "  " + repr(ints)
        print(f"@0x{p:04x} t={time:<4d} {op:<3d} {name:<18s} sz={size:<3d}{detail}")
        p += 4 + size
        n += 1
        if op == 0:
            break
    print(f"# parsed {n} instrs, ended @0x{p:x}/0x{end:x}")
    return p


def parse(path: str, kind: str):
    data = open(path, "rb").read()
    count = struct.unpack_from("<I", data, 0)[0]
    entries = [struct.unpack_from("<II", data, 4 + 8 * i) for i in range(count)]
    print(f"# {path}  size={len(data)}  header: count={count} entries={[(hex(o), f) for o, f in entries]}")
    offsets = sorted({o for o, _ in entries})
    for i, (off, flags) in enumerate(entries):
        later = [o for o in offsets if o > off]
        end = later[0] if later else len(data)
        print(f"## entry {i} ({flags}) @0x{off:x}")
        parse_script(data, off, end, kind)


if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("file")
    ap.add_argument("--kind", default="ending", choices=["ending", "stage"],
                    help="ending: eXX/staffN.msg(msg/04 表);stage: stXXy.msg(msg/02 + th18/01 表)")
    a = ap.parse_args()
    parse(a.file, a.kind)
