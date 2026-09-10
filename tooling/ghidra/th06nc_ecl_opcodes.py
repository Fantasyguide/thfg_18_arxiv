"""从 exe 现场解出 TH06NC 的 ECL 主分派表，生成每个 handler 的标签。

**为什么不把映射写死在 spec 里**：opcode → handler 是二进制里的一张表，
手抄 201 行既容易错也会和 exe 漂移。这里现场解，spec 只留几个表地址常量——
错了会当场对不上，而不是安静地错下去。

分派形状（`0x1400221b1` 起，一手反汇编）：

    MOVSX EAX, word ptr [R14 + 0x4]        ; opcode (s16)
    DEC   EAX                              ; index = opcode - 1
    CMP   EAX, 0xc8                        ; 200
    JA    default
    MOVZX EAX, byte ptr [R10 + RAX + 0x28f5c]      ; 字节映射：index -> 槽位
    MOV   ECX, dword ptr [R10 + RAX*4 + 0x28d78]   ; 跳表：槽位 -> RVA
    JMP   RCX

opcode 名字来自 GensokyoClub/th06(`th06-decomp`)的 `EclRawInstrOpcode`,**CC0**,
可自由使用与再发布(与 truth 的 mapfile 不同)。已用 `validate_names` 逐项核对过。
"""

DISPATCH_BYTE_MAP = 0x140028F5C
DISPATCH_JUMP_TABLE = 0x140028D78
DISPATCH_DEFAULT = 0x140025D7D
OPCODE_MIN = 1
OPCODE_MAX = 201
IMAGE_BASE = 0x140000000


def _sections(data):
    import struct

    pe = struct.unpack_from("<I", data, 0x3C)[0]
    count = struct.unpack_from("<H", data, pe + 6)[0]
    optional = struct.unpack_from("<H", data, pe + 20)[0]
    out = []
    for index in range(count):
        header = pe + 24 + optional + index * 40
        out.append(struct.unpack_from("<IIII", data, header + 8))
    return out


def _reader(data):
    sections = _sections(data)

    def to_offset(va):
        rva = va - IMAGE_BASE
        for virtual_size, virtual_address, raw_size, raw_pointer in sections:
            if virtual_address <= rva < virtual_address + max(virtual_size, raw_size):
                return raw_pointer + (rva - virtual_address)
        raise ValueError("VA 0x%x 不在任何节里" % va)

    return to_offset


def decode(data):
    """-> {opcode: (槽位, handler VA)}，覆盖 1..201。"""
    import struct

    to_offset = _reader(data)
    out = {}
    for opcode in range(OPCODE_MIN, OPCODE_MAX + 1):
        slot = data[to_offset(DISPATCH_BYTE_MAP + (opcode - OPCODE_MIN))]
        rva = struct.unpack_from("<I", data, to_offset(DISPATCH_JUMP_TABLE + slot * 4))[0]
        out[opcode] = (slot, IMAGE_BASE + rva)
    return out


def group(mapping, names, deltas):
    """把 handler 相同的 opcode 归并，生成 (VA, [opcodes], 标签名, 注释)。

    `names` 是 0..135 的 CC0 名字表；`deltas` 是 NC 新增的 {opcode: (name, why)}。
    落到分派默认出口的 opcode **不生成标签**——那不是 handler，是「没实现」。
    """
    buckets = {}
    for opcode, (_slot, va) in sorted(mapping.items()):
        if va == DISPATCH_DEFAULT:
            continue
        buckets.setdefault(va, []).append(opcode)

    def name_of(opcode):
        if opcode < len(names):
            return names[opcode]
        return deltas[opcode][0] if opcode in deltas else "OP%d" % opcode

    out = []
    for va, opcodes in sorted(buckets.items()):
        head = opcodes[0]
        label = "ECL_OP_%03d_%s" % (head, name_of(head))
        if len(opcodes) == 1:
            comment = "ECL opcode %d = %s." % (head, name_of(head))
        else:
            listed = ", ".join("%d %s" % (op, name_of(op)) for op in opcodes)
            comment = "Shared handler for %d opcodes: %s." % (len(opcodes), listed)
        out.append((va, opcodes, label, comment))
    return out


def unimplemented(mapping):
    """落到分派默认出口的 opcode（= 接受但什么都不做）。"""
    return sorted(op for op, (_slot, va) in mapping.items() if va == DISPATCH_DEFAULT)


def validate_names(names, header_text):
    """拿 `th06-decomp/src/EclManager.hpp` 的原文核对名字表，防止悄悄漂移。"""
    import re

    match = re.search(r"enum EclRawInstrOpcode\s*\{(.*?)\};", header_text, re.S)
    if not match:
        return ["在 EclManager.hpp 里找不到 EclRawInstrOpcode"]
    upstream = re.findall(r"ECL_OPCODE_(\w+)", match.group(1))
    if list(upstream) != list(names):
        return ["opcode 名字表与 CC0 上游不一致（上游 %d 项，我们 %d 项）"
                % (len(upstream), len(names))]
    return []
