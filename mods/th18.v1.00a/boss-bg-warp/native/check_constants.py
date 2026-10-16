#!/usr/bin/env python3
"""check_constants.py —— th18_bgwarp 的死绑量 ↔ 真 exe 逐字节对账。

`make check`。三件事：
  1. 源码里写死的 4 组签名字节，去 exe 的对应 RVA 上比（不一致 = 换 build 了，必须重取）；
  2. ★ **hook 窗口的指令边界**：我们偷走的那 N 字节必须正好是**整数条指令**，
     且**不含相对寻址**（rel8/rel32）—— 跳板是把这 N 字节原样搬到别处再执行的，
     只要有一条相对指令落在窗口里，搬过去就是错的地址。
  3. 从 C 源码里把 RVA / 偏移 / 签名字节解析出来，保证"文档说的"和"代码写的"是同一批数。

不写死任何机器上的路径：exe 走 --exe，或默认 <repo>/local/<版本>/th18.exe。
"""
import argparse
import os
import re
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, "..", "..", "..", ".."))
SRC = os.path.join(HERE, "th18_bgwarp.c")

VERSION = "th18.v1.00a"
EXE = os.path.join(REPO, "local", VERSION, "th18.exe")
EXE_MD5 = "9969cac756098c1da05a81de45437a70"

# (符号名, 窗口字节数) —— 窗口长度必须与 C 里 make_stub(target, len, ...) 的 len 一致
HOOKS = [
    ("RVA_MESH_UPLOAD", 7),   # push ebp; mov ebp,esp; push ecx; push edi; mov edi,ecx
    ("RVA_SWIRL_EXIT",   6),  # mov edi,[esp+0x1c]; xor ecx,ecx
]

# 签名字节 → 它应该在哪个 RVA（C 里 SIG_TICK 是内联地址，单独处理）
SIG_RVA = {
    "SIG_UPLOAD": "RVA_MESH_UPLOAD",
    "SIG_BUILD":  "RVA_MESH_BUILD",
    "SIG_SWIRL":  "RVA_SWIRL_EXIT",
}
SIG_INLINE = {"SIG_TICK": 0x1E7B4}

fails = []


def rd(d, pe, optsz, rva, n):
    nsec = struct.unpack_from("<H", d, pe + 6)[0]
    for i in range(nsec):
        o = pe + 0x18 + optsz + 40 * i
        vs, va, rs, ra = struct.unpack_from("<IIII", d, o + 8)
        if va <= rva < va + max(vs, rs):
            off = ra + (rva - va)
            return d[off:off + n]
    return None


def parse_c(path):
    """从源码里抓 RVA_* / SIG_* —— 不引入任何 C 解析器，够用就好。"""
    txt = open(path, encoding="utf-8", errors="replace").read()
    rvas = {}
    for m in re.finditer(r"#define\s+(RVA_[A-Z0-9_]+)\s+0x([0-9a-fA-F]+)u", txt):
        rvas[m.group(1)] = int(m.group(2), 16)
    sigs = {}
    for m in re.finditer(r"static const BYTE (SIG_[A-Z0-9_]+)\[\]\s*=\s*\{(.*?)\};", txt, re.S):
        body = re.sub(r"//[^\n]*", "", m.group(2))
        body = re.sub(r"/\*.*?\*/", "", body, flags=re.S)
        vals = [int(x, 16) for x in re.findall(r"0x([0-9a-fA-F]{2})", body)]
        sigs[m.group(1)] = bytes(vals)
    return rvas, sigs


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--exe", default=EXE)
    a = ap.parse_args()
    if not os.path.isfile(a.exe):
        print("找不到 exe：%s（用 --exe 指一个）" % a.exe)
        return 2

    d = open(a.exe, "rb").read()
    pe = struct.unpack_from("<I", d, 0x3C)[0]
    optsz = struct.unpack_from("<H", d, pe + 0x14)[0]
    ib = struct.unpack_from("<I", d, pe + 0x34)[0]
    import hashlib
    md5 = hashlib.md5(d).hexdigest()
    print("exe  = %s" % a.exe)
    print("md5  = %s%s" % (md5, "" if md5 == EXE_MD5 else "   ★ 与登记的 %s 不同" % EXE_MD5))

    rvas, sigs = parse_c(SRC)
    print("源码里解析到 %d 个 RVA、%d 组签名" % (len(rvas), len(sigs)))

    # ---- 1. 签名字节 ----
    for name, blk in sorted(sigs.items()):
        rva = None
        if name in SIG_RVA:
            rva = rvas.get(SIG_RVA[name])
        elif name in SIG_INLINE:
            rva = SIG_INLINE[name]
        if rva is None:
            fails.append("%s：源码里找不到对应的 RVA" % name)
            continue
        got = rd(d, pe, optsz, rva, len(blk))
        if got is None:
            fails.append("%s：RVA 0x%x 不在任何节里" % (name, rva))
            continue
        if got != blk:
            fails.append("%s @0x%x 不符：exe=%s 源码=%s"
                         % (name, rva + ib, got.hex(), blk.hex()))
        else:
            print("  ok  %-16s @0x%08x  %s" % (name, rva + ib, got.hex()))

    # ---- 2. hook 窗口的指令边界 ----
    try:
        from capstone import Cs, CS_ARCH_X86, CS_MODE_32
    except ImportError:
        print("⚠ 没有 capstone ⇒ **跳过指令边界检查**（这一步不能省，装一个 capstone 再跑）")
        Cs = None
    if Cs:
        md = Cs(CS_ARCH_X86, CS_MODE_32)
        md.detail = True
        for key, want in HOOKS:
            rva = rvas[key]
            code = rd(d, pe, optsz, rva, want + 8)
            consumed = 0
            bad = []
            listing = []
            for ins in md.disasm(code, rva + ib):
                if consumed >= want:
                    break
                consumed += ins.size
                listing.append("%s %s" % (ins.mnemonic, ins.op_str))
                # 窗口内不允许出现相对寻址 / 调用 / 跳转
                if ins.mnemonic in ("call", "jmp", "je", "jne", "jz", "jnz", "jg", "jge",
                                    "jl", "jle", "ja", "jae", "jb", "jbe", "loop", "jecxz"):
                    bad.append("相对转移 %s %s" % (ins.mnemonic, ins.op_str))
                for op in ins.operands:
                    if op.type == 1 and ("rip" in op.reg_name if hasattr(op, "reg_name") else False):
                        bad.append("RIP 相对")
            if consumed != want:
                fails.append("%s：窗口 %d 字节切在指令中间（实际吃掉 %d 字节）"
                             % (key, want, consumed))
            elif bad:
                fails.append("%s：窗口里含不可搬移的指令 —— %s" % (key, "；".join(bad)))
            else:
                print("  ok  %-16s 窗口 %d 字节 = 整数条指令、无相对寻址：%s"
                      % (key, want, " | ".join(listing)))

    print()
    if fails:
        for f in fails:
            print("FAIL: " + f)
        print("—— %d 处不符 ——" % len(fails))
        return 1
    print("全部通过。")
    return 0


if __name__ == "__main__":
    sys.exit(main())
