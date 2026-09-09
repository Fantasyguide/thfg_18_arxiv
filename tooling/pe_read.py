#!/usr/bin/env python3
"""从 exe 里按 VA 直读字节——RV / R4–R8 这类「解跳表、常量池、字符串表」的活专用。

    # 读 36 个 dword（ANM 变量跳表）
    python3 tooling/pe_read.py local/th16.v1.00a/th16.exe 0x45f578 --dwords 36

    # 读一段原始字节
    python3 tooling/pe_read.py local/th16.v1.00a/th16.exe 0x45f2d0 --bytes 32

为什么不用 Ghidra：跳表/常量池是**数据**，HTML 快照里没有；而工程锁是独占的，
为几十个字节开一次库不划算（见 task-R3-report.md §3.3）。手解 PE 节表约 20 行，
幂等可复现，不占锁。
"""
import argparse, struct, sys

def load_sections(path):
    d = open(path, "rb").read()
    pe = struct.unpack_from("<I", d, 0x3C)[0]
    if d[pe:pe + 4] != b"PE\0\0":
        sys.exit("不是 PE 文件")
    nsec = struct.unpack_from("<H", d, pe + 6)[0]
    opt = pe + 24
    base = struct.unpack_from("<I", d, opt + 28)[0]
    secoff = opt + struct.unpack_from("<H", d, pe + 20)[0]
    secs = []
    for i in range(nsec):
        o = secoff + 40 * i
        name = d[o:o + 8].rstrip(b"\0").decode()
        vsz, va, rsz, ra = struct.unpack_from("<IIII", d, o + 8)
        secs.append((name, va, vsz, ra, rsz))
    return d, base, secs

def read_va(d, base, secs, addr, n):
    rva = addr - base
    for name, sva, vsz, ra, rsz in secs:
        if sva <= rva < sva + max(vsz, rsz):
            off = ra + (rva - sva)
            return d[off:off + n], name
    raise ValueError(f"{addr:#x} 不在任何节内（imagebase={base:#x}）")

def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("exe")
    ap.add_argument("va", type=lambda s: int(s, 0))
    g = ap.add_mutually_exclusive_group()
    g.add_argument("--bytes", type=int, default=0, help="读原始字节")
    g.add_argument("--dwords", type=int, default=0, help="按 u32 读 n 项")
    ap.add_argument("--base", type=lambda s: int(s, 0), help="变量号基数（配 --dwords 用）")
    a = ap.parse_args()

    d, base, secs = load_sections(a.exe)
    n = (a.dwords or a.bytes or 16) * (4 if a.dwords else 1)
    buf, sec = read_va(d, base, secs, a.va, n)
    print(f"# {a.exe} imagebase={base:#x}  {a.va:#x} 在 {sec}  (file bytes {len(buf)})")
    if a.dwords:
        for i, v in enumerate(struct.unpack(f"<{a.dwords}I", buf)):
            tag = f"{a.base + i} -> " if a.base is not None else ""
            print(f"  [{i:3d}] {tag}{v:#x}")
    else:
        for off in range(0, len(buf), 16):
            chunk = buf[off:off + 16]
            hexs = " ".join(f"{b:02x}" for b in chunk)
            print(f"  {a.va + off:#010x}  {hexs:<47}  {''.join(chr(b) if 32 <= b < 127 else '.' for b in chunk)}")

if __name__ == "__main__":
    main()
