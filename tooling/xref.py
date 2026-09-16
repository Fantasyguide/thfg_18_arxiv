#!/usr/bin/env python3
"""xref.py —— exe 的调用者/被调用者查询（不依赖 ghidra，直接扫 `e8 rel32`）。

**版本无关**：节布局从 PE 头读（不再写死 th18 的 `.text` RVA/大小），符号表可换。

    python3 tooling/xref.py callers  0x401530 [--exe <exe>] [--symbols <funcs.json>]
    python3 tooling/xref.py callers-any 0x401530      # 连 `ff 15`/`ff 25` 间接调用一起列
    python3 tooling/xref.py callees  0x472e9c
    python3 tooling/xref.py name     0x401530

用途（2026-09-17 定责 TH18 头号崩溃时立过功）：找「谁注册/谁调用」——例如
`th18:0x401530`（`UpdateFunc__operator_new`）的 89 处调用者就是这么扫出来的 ⇒ 拿到「更新链上有谁」。

⚠ 前提与限制：只扫**直接相对调用**（`e8 rel32`）；间接调用（`ff 15 imm32` / `call [reg+off]`）只在
`callers-any` 里以字面量形式列出；跨节的 `e8` 若落在非 `.text` 段会被跳过（本工具的节选择见下）。
符号表：`local/*/*-funcs.json`（社区 th-re-data 风格，list of {addr,name,size}）。
"""
import argparse
import bisect
import json
import os
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, ".."))


def find_default(pattern, subdir=""):
    loc = os.path.join(REPO, "local", subdir) if subdir else os.path.join(REPO, "local")
    if not os.path.isdir(loc):
        return None
    for ver in sorted(os.listdir(loc)):
        d = os.path.join(loc, ver)
        if os.path.isdir(d):
            p = os.path.join(d, pattern)
            if os.path.exists(p):
                return p
    return None


class PE(object):
    """极简 PE32 读法：只取节表（找可执行段）与 imagebase。"""

    def __init__(self, path):
        self.f = open(path, "rb").read()
        self.path = path
        pe = struct.unpack_from("<I", self.f, 0x3C)[0]
        assert self.f[pe:pe + 4] == b"PE\0\0", "不是 PE 文件"
        nsec = struct.unpack_from("<H", self.f, pe + 6)[0]
        optsz = struct.unpack_from("<H", self.f, pe + 20)[0]
        opt = pe + 24
        self.imagebase = struct.unpack_from("<I", self.f, opt + 28)[0]
        self.sections = []
        for i in range(nsec):
            o = opt + optsz + i * 40
            name = self.f[o:o + 8].rstrip(b"\0").decode("latin1")
            vsize, va, rsize, raw = struct.unpack_from("<IIII", self.f, o + 8)
            flags = struct.unpack_from("<I", self.f, o + 36)[0]
            self.sections.append({"name": name, "va": va, "vsize": vsize,
                                  "raw": raw, "rsize": rsize, "exec": bool(flags & 0x20000000)})

    def code_section(self):
        for s in self.sections:
            if s["name"] == ".text":
                return s
        for s in self.sections:
            if s["exec"]:
                return s
        raise SystemExit("找不到可执行节")

    def rva2off(self, rva):
        for s in self.sections:
            if s["va"] <= rva < s["va"] + max(s["vsize"], s["rsize"]):
                if rva - s["va"] >= s["rsize"]:
                    return None
                return s["raw"] + (rva - s["va"])
        return None

    def off2rva(self, off):
        for s in self.sections:
            if s["raw"] <= off < s["raw"] + s["rsize"]:
                return s["va"] + (off - s["raw"])
        return None


def load_symbols(path):
    if not path or not os.path.exists(path):
        return [], {}, {}
    d = json.load(open(path, encoding="utf-8"))
    addrs, names, sizes = [], {}, {}
    if isinstance(d, dict):
        for k, v in d.items():
            try:
                a = int(k, 16)
            except (TypeError, ValueError):
                continue
            addrs.append(a); names[a] = v if isinstance(v, str) else str(v)
    else:
        for e in d:
            a = int(e["addr"], 16)
            addrs.append(a); names[a] = e.get("name", "?"); sizes[a] = e.get("size", 0)
    addrs.sort()
    return addrs, names, sizes


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("mode", choices=("callers", "callers-any", "callees", "name"))
    ap.add_argument("addrs", nargs="+")
    ap.add_argument("--exe", default=None, help="默认 local/*/th*.exe 里第一个")
    ap.add_argument("--symbols", default=None, help="默认 local/*/*-funcs.json")
    a = ap.parse_args()

    exe = a.exe or find_default("th18.exe")
    if not exe:
        raise SystemExit("没找到 exe，用 --exe 指定")
    pe = PE(exe)
    sec = pe.code_section()
    # 符号表默认**按 exe 所在目录**找（否则会挑到别的作品的表 —— 2026-10-01 踩过）；
    # 样本目录里没有就退到社区命名金矿 `local/vendor/th-re-data/data/<版本>/funcs.json`
    # （TH08/09/12 三作的名字都在那儿；样本目录里通常不放符号表）。
    sym = a.symbols
    if not sym:
        d = os.path.dirname(os.path.abspath(exe))
        for fn in sorted(os.listdir(d)):
            if fn.endswith("-funcs.json"):
                sym = os.path.join(d, fn); break
        if not sym:
            v = os.path.join(REPO, "local", "vendor", "th-re-data", "data",
                             os.path.basename(d), "funcs.json")
            sym = v if os.path.exists(v) else None
    addrs, names, sizes = load_symbols(sym)
    ib = pe.imagebase

    def name(va):
        if va in names:
            return names[va]
        i = bisect.bisect_right(addrs, va) - 1
        return "%s+0x%x" % (names[addrs[i]], va - addrs[i]) if i >= 0 else "?"

    def func_at(va):
        i = bisect.bisect_right(addrs, va) - 1
        return addrs[i] if i >= 0 else None

    if a.mode == "name":
        for s in a.addrs:
            va = int(s, 16)
            print("%08x  %s   (in %s)" % (va, name(va), name(func_at(va))))
        return 0

    if a.mode == "callees":
        for s in a.addrs:
            va = int(s, 16)
            print("=== callees of %08x %s ===" % (va, name(va)))
            i = bisect.bisect_right(addrs, va); nxt = addrs[i] if i < len(addrs) else va + 0x200
            off, end = pe.rva2off(va - ib), pe.rva2off(nxt - ib)
            if off is None or end is None:
                print("   （地址不在可执行节内）"); continue
            while off < end - 5:
                if pe.f[off] == 0xE8:
                    rel = struct.unpack_from("<i", pe.f, off + 1)[0]
                    site = pe.off2rva(off) + ib
                    tgt = (site + 5 + rel) & 0xFFFFFFFF
                    print("  %08x  call %08x  %s" % (site, tgt, name(tgt)))
                    off += 5; continue
                off += 1
        return 0

    # callers / callers-any
    lo, hi = sec["raw"], sec["raw"] + sec["rsize"]
    for s in a.addrs:
        tgt_rva = int(s, 16) - ib
        print("=== callers of %08x %s ===" % (int(s, 16), name(int(s, 16))))
        for off in range(lo, hi - 5):
            b = pe.f[off]
            rva = pe.off2rva(off)
            if b == 0xE8:
                rel = struct.unpack_from("<i", pe.f, off + 1)[0]
                if (rva + 5 + rel) & 0xFFFFFFFF == tgt_rva:
                    site = rva + ib
                    print("  %08x  call   (in %s)" % (site, name(func_at(site))))
            elif a.mode == "callers-any" and b == 0xFF and pe.f[off + 1] in (0x15, 0x25):
                print("  %08x  call [%08x]   (in %s)"
                      % (rva + ib, struct.unpack_from("<I", pe.f, off + 2)[0], name(func_at(rva + ib))))
    return 0


if __name__ == "__main__":
    sys.exit(main())
