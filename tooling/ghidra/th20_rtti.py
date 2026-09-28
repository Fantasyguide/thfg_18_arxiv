#!/usr/bin/env python3
"""TH20 的 MSVC RTTI 复原器：类名 → type_info → COL → vftable → 逐槽虚函数。

TH20 没有社区符号（`games/th20.v1.00a/INDEX.md` §2c），但 exe 里保留着完整的
MSVC RTTI 字符串（`.?AVStoneRedReimuInf@@`）。Ghidra 12 的 RTTI 分析器**没有**把这些
还原成 vftable 标签，所以这套链子自己走：

    name 字符串  →  _TypeDescriptor（名字在 +8）
                 →  RTTICompleteObjectLocator（+0xc 指向 type descriptor）
                 →  vftable[-1] 指向 COL  ⇒ vftable 起点 = 找到的引用地址 - 4
                 →  逐槽读指针，直到不再指向 .text

用法：

    python3 tooling/ghidra/th20_rtti.py --grep Stone            # 所有含 Stone 的类
    python3 tooling/ghidra/th20_rtti.py --class StoneRedReimuInf
    python3 tooling/ghidra/th20_rtti.py --grep Stone --slots    # 顺带印槽目标函数名

输出是「类名 → vftable 地址 → 槽位 → 函数地址」。有了这个，
`WeaponStoneInf` 的 `CalcMain` / `CalcSub` / `OnStoneAttack` 就能一一对上 ZUN 的源码结构。
"""
import argparse
import re
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import _driver                                                    # noqa: E402


def read_image(prog):
    """把整个程序内存读成 python bytes（1.8 MB，一次读完，后面全在 python 里扫）。"""
    from jpype import JArray, JByte
    mem = prog.getMemory()
    blocks = []
    for b in mem.getBlocks():
        # ⚠️ **别信 `isInitialized()`**：TH20 的 .rdata/.data 在这个工程里报 False，
        # 但 `getBytes` 读得出来。信它就会把 vftable/COL 全扫掉，只剩 .text，
        # 于是在代码里找到一堆「碰巧等于 type descriptor 地址」的假 COL（踩过）。
        n = int(b.getSize())
        if n == 0:
            continue
        # ⚠️ jpype 会把 python bytearray **拷贝**成 Java 数组，回填读不到 ——
        # 必须自己造 Java 数组再取回来（踩过一次：全零，静默扫不到任何东西）。
        jbuf = JArray(JByte)(n)
        mem.getBytes(b.getStart(), jbuf)
        blocks.append((int(b.getStart().getOffset()),
                       bytes((x & 0xff) for x in jbuf)))
    return blocks


def find_dwords(blocks, value):
    """在镜像里找 4 字节小端 == value 的所有 VA。"""
    needle = struct.pack("<I", value & 0xffffffff)
    hits = []
    for base, data in blocks:
        i = data.find(needle)
        while i != -1:
            hits.append(base + i)
            i = data.find(needle, i + 1)
    return hits


def rd(blocks, va, fmt="<I"):
    size = struct.calcsize(fmt)
    for base, data in blocks:
        if base <= va and va + size <= base + len(data):
            return struct.unpack(fmt, data[va - base:va - base + size])[0]
    return None


def in_text(prog, va):
    a = prog.getAddressFactory().getDefaultAddressSpace().getAddress(va)
    blk = prog.getMemory().getBlock(a)
    return bool(blk) and blk.isExecute()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--version", default="th20")
    ap.add_argument("--class", dest="cls")
    ap.add_argument("--grep")
    ap.add_argument("--slots", action="store_true", help="印每个槽的目标函数名")
    ap.add_argument("--max-slots", type=int, default=40)
    ap.add_argument("--json", help="把结果写成 JSON")
    args = ap.parse_args()

    p = _driver.resolve(args.version)
    _driver.ghidra_install()
    pat = re.compile(args.grep) if args.grep else None

    with _driver.open_program(p["proj_dir"], p["proj_name"], p["program"], tx=None) as prog:
        blocks = read_image(prog)
        fm = prog.getFunctionManager()

        # 1) 收集 .?AV<name>@@ 字符串（type descriptor 的名字）
        names = {}
        for data in prog.getListing().getDefinedData(True):
            v = data.getValue()
            if not isinstance(v, str) or not v.startswith(".?AV"):
                continue
            m = re.match(r"^\.\?AV(.+?)@@$", v)
            if not m:
                continue
            cls = m.group(1)
            if args.cls and cls != args.cls:
                continue
            if pat and not pat.search(cls):
                continue
            names.setdefault(cls, []).append(int(data.getAddress().getOffset()))

        results = []
        for cls, straddrs in sorted(names.items()):
            for straddr in straddrs:
                tdesc = straddr - 8                      # _TypeDescriptor: vfptr, spare, name
                col_cands = [c - 0xc for c in find_dwords(blocks, tdesc)]
                for col in col_cands:
                    if rd(blocks, col) != 0:             # signature
                        continue
                    # vftable 的「元指针」在 vftable-4 处指向 COL ⇒ 找到存 COL 的那一格，
                    # vftable 起点在它**后面** 4 字节（写成 -4 会整体错一格，踩过）。
                    vfptr_cands = [c + 4 for c in find_dwords(blocks, col)]
                    for vft in vfptr_cands:
                        slots = []
                        for i in range(args.max_slots):
                            tgt = rd(blocks, vft + i * 4)
                            if tgt is None or not in_text(prog, tgt):
                                break
                            f = fm.getFunctionAt(
                                prog.getAddressFactory().getDefaultAddressSpace().getAddress(tgt))
                            slots.append((i, tgt, f.getName() if f else None))
                        if not slots:
                            continue
                        results.append(dict(cls=cls, name_str=hex(straddr), tdesc=hex(tdesc),
                                            col=hex(col), vftable=hex(vft), slots=slots))
                        print(f"{cls}\n  name_str={straddr:#x} tdesc={tdesc:#x} COL={col:#x} "
                              f"vftable={vft:#x}  slots={len(slots)}")
                        if args.slots:
                            for i, tgt, fn in slots:
                                print(f"    [{i:2d}] {tgt:#010x}  {fn}")
        if args.json:
            import json
            Path(args.json).write_text(json.dumps(results, ensure_ascii=False, indent=1))
            print(f"[{len(results)} 个 vftable → {args.json}]")


if __name__ == "__main__":
    main()
