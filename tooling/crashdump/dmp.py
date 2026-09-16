#!/usr/bin/env python3
"""dmp.py —— Windows minidump（`MDMP`）解析：异常 / 寄存器 / ebp 链 / 模块表 / 已捕获内存段。

**版本无关**（TH08/TH09/TH12 同样适用）：符号表与样本路径都由参数给，默认按 th18 找。

    python3 tooling/crashdump/dmp.py <file.dmp> [--frames 24] [--symbols <funcs.json>]

配方（一手，2026-09-17 实测定下来的，改动前先读这里）：
  ① 头 `MDMP`：`Signature@0`、`NumberOfStreams@8`、`StreamDirectoryRva@0xc`；流目录项 = `{Type,Size,Rva}`；
  ② 异常流(6)：`ThreadId@0`、`ExceptionCode@8`、`ExceptionAddress@0x18`(u64)、`NumberParameters@0x20`、
     `ExceptionInformation@0x28`；`ThreadContext` 描述符在 `+0xa0`（DataSize@+0xa0, Rva@+0xa4）；
  ③ i386 `CONTEXT`：`Edi@0x9c Esi@0xa0 Ebx@0xa4 Edx@0xa8 Ecx@0xac Eax@0xb0 Ebp@0xb4 Eip@0xb8 Esp@0xc4`；
  ④ 线程流(3)：`{ThreadId@0, Stack{Start@0x18(u64), Size@0x20, Rva@0x24}, Context@0x2c}`；
  ⑤ 模块流(4)：`{Base@0(u64), Size@8, NameRva@0x14}`，**名字前有 u32 长度前缀**（UTF-16）；
  ⑥ 内存段（流 5 = MemoryList / 9 = Memory64List）——**堆页通常不在里面**，所以对象内容读不到，只有栈。

判读要点（2026-09-17 TH18 头号崩溃就是靠它定的责）：
  · `Eip` 不在任何模块里 ⇒ 多半是「跳到 thcrap 跳板池/被释放的代码」；
  · 若那一页被 dump 捕了下来（段表里找得到），**直接看字节**：全 0 就说明执行的是 `00 00`（`add [eax],al`），
    配合 `Eax` 就能解释「`0xC0000005` 写某个小地址」这种签名；
  · `ebp` 链往上走两级就能看出「是从哪个引擎循环里跳出去的」。
"""
import argparse
import bisect
import json
import os
import re
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.abspath(os.path.join(HERE, "..", ".."))


def find_symbols():
    """默认符号表：local/<ver>/<name>-funcs.json —— 取第一个存在的（社区符号表）。"""
    loc = os.path.join(REPO, "local")
    if not os.path.isdir(loc):
        return None
    for ver in sorted(os.listdir(loc)):
        d = os.path.join(loc, ver)
        if not os.path.isdir(d):
            continue
        for fn in sorted(os.listdir(d)):
            if fn.endswith("-funcs.json"):
                return os.path.join(d, fn)
    return None


def load_symbols(path):
    """支持两种形态：list of {addr,name,size}（th-re-data 风格）/ dict{addr: name}。"""
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
            addrs.append(a)
            names[a] = v if isinstance(v, str) else str(v)
    else:
        for e in d:
            a = int(e["addr"], 16)
            addrs.append(a)
            names[a] = e.get("name", "?")
            sizes[a] = e.get("size", 0)
    addrs.sort()
    return addrs, names, sizes


class Dump(object):
    def __init__(self, path):
        self.f = open(path, "rb").read()
        sig, ver, n, dira, chk, tds, flags = struct.unpack_from("<IIIIIIQ", self.f, 0)
        assert sig == 0x504D444D, "not a minidump"
        self.streams = {}
        for i in range(n):
            t, size, rva = struct.unpack_from("<III", self.f, dira + i * 12)
            self.streams[t] = (size, rva)

    def u32(self, off):
        return struct.unpack_from("<I", self.f, off)[0]

    def u64(self, off):
        return struct.unpack_from("<Q", self.f, off)[0]

    def exception(self):
        size, rva = self.streams[6]
        nparam = self.u32(rva + 0x20)
        return {"thread": self.u32(rva), "code": self.u32(rva + 8),
                "addr": self.u64(rva + 0x18),
                "params": [self.u64(rva + 0x28 + i * 8) for i in range(min(nparam, 15))],
                "ctx_rva": self.u32(rva + 0xa0 + 4)}

    def context(self, rva):
        g = lambda o: self.u32(rva + o)
        return {"edi": g(0x9c), "esi": g(0xa0), "ebx": g(0xa4), "edx": g(0xa8), "ecx": g(0xac),
                "eax": g(0xb0), "ebp": g(0xb4), "eip": g(0xb8), "esp": g(0xc4)}

    def threads(self):
        size, rva = self.streams[3]
        out = []
        for i in range(self.u32(rva)):
            e = rva + 4 + i * 48
            out.append({"tid": self.u32(e), "stack_start": self.u64(e + 0x18),
                        "stack_size": self.u32(e + 0x20), "stack_rva": self.u32(e + 0x24),
                        "ctx_rva": self.u32(e + 0x2c)})
        return out

    def modules(self):
        size, rva = self.streams[4]
        out = []
        for i in range(self.u32(rva)):
            e = rva + 4 + i * 108
            base, imgsize, namerva = self.u64(e), self.u32(e + 8), self.u32(e + 0x14)
            ln = self.u32(namerva)
            nm = self.f[namerva + 4:namerva + 4 + ln].decode("utf-16-le", "replace")
            out.append((base, imgsize, nm))
        return out

    def ranges(self):
        """已捕获的内存段（流 5 / 9）—— 用来判断「崩点那一页有没有被捕获」。"""
        out = []
        if 5 in self.streams:
            size, rva = self.streams[5]
            for i in range(self.u32(rva)):
                e = rva + 4 + i * 16
                start, dsz, drva = struct.unpack_from("<QII", self.f, e)
                out.append((start, dsz, drva))
        if 9 in self.streams:
            size, rva = self.streams[9]
            n, base = self.u64(rva), self.u64(rva + 8)
            off = base
            for i in range(n):
                start, dsz = struct.unpack_from("<QQ", self.f, rva + 16 + i * 16)
                out.append((start, dsz, off))
                off += dsz
        return out

    def mem(self, rva, size):
        return self.f[rva:rva + size]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("dump")
    ap.add_argument("--frames", type=int, default=20)
    ap.add_argument("--symbols", default=None, help="funcs.json（默认自动找 local/*/*-funcs.json）")
    a = ap.parse_args()

    addrs, names, _ = load_symbols(a.symbols or find_symbols())

    def name(va):
        if va in names:
            return names[va]
        i = bisect.bisect_right(addrs, va) - 1
        return "%s+0x%x" % (names[addrs[i]], va - addrs[i]) if i >= 0 else "?"

    d = Dump(a.dump)
    ex = d.exception()
    ths = d.threads()
    th = next((t for t in ths if t["tid"] == ex["thread"]), ths[0])
    ctx = d.context(ex["ctx_rva"])
    stk = d.mem(th["stack_rva"], th["stack_size"])
    mods = d.modules()

    def inmod(va):
        for base, size, nm in mods:
            if base <= va < base + size:
                return "%s+%x" % (os.path.basename(nm), va - base)
        return "?（不在任何模块内）"

    print("文件   : %s" % os.path.basename(a.dump))
    print("异常   : code=%#x addr=%#x params=%s" % (ex["code"], ex["addr"], [hex(p) for p in ex["params"]]))
    print("寄存器 : eip=%08x esp=%08x ebp=%08x eax=%08x ebx=%08x ecx=%08x edx=%08x esi=%08x edi=%08x"
          % tuple(ctx[k] for k in ("eip", "esp", "ebp", "eax", "ebx", "ecx", "edx", "esi", "edi")))
    print("EIP 在 : %s" % inmod(ctx["eip"]))
    if ex["params"]:
        kind = {0: "读", 1: "写", 8: "执行(DEP)"}.get(ex["params"][1] if len(ex["params"]) > 1 else -1, "?")
        print("访问   : %s @ %#x" % (kind, ex["params"][0]))
        hit = [r for r in d.ranges() if r[0] <= ex["addr"] < r[0] + r[1]]
        if hit:
            start, size, rva = hit[0]
            off = rva + (ex["addr"] - start)
            print("         崩点那一页**已被捕获**：%#x..%#x，该处 32 字节 = %s"
                  % (start, start + size, d.f[off:off + 32].hex(" ")))
        else:
            print("         崩点那一页**未被捕获**（堆页通常不捕 ⇒ 只能靠栈与寄存器推断）")
    print("模块   : %d 个（%s…）" % (len(mods), ", ".join(os.path.basename(m[2]) for m in mods[:4])))
    print("\n-- ebp 链（返回地址 → 符号）--")
    ebp = ctx["ebp"]
    seen = set()
    for i in range(a.frames):
        rel = ebp - th["stack_start"]
        if rel < 0 or rel + 8 > len(stk):
            print("   [%2d] ebp=%08x 不在该线程栈窗口内" % (i, ebp))
            break
        nxt, ret = struct.unpack_from("<II", stk, rel)
        print("   [%2d] ebp=%08x ret=%08x  %s" % (i, ebp, ret, name(ret)))
        if nxt <= ebp or nxt in seen:
            break
        seen.add(ebp)
        ebp = nxt
    return 0


if __name__ == "__main__":
    sys.exit(main())
