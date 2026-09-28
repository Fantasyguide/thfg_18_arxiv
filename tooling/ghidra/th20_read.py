#!/usr/bin/env python3
"""TH20 只读侦察器：反编译 / 反汇编 / 交叉引用 / 符号搜索，一条命令一件事。

TH20 没有社区命名（`games/th20.v1.00a/INDEX.md` §2c），本机也没有 MCP 会话，
所以挖 TH20 全靠这个脚本 + 我们自己的命名回流（`th20_anchors.py`）。

用法（地址写十六进制，可带 `0x`）：

    python3 tooling/ghidra/th20_read.py --decomp 0x45a2b0
    python3 tooling/ghidra/th20_read.py --decomp 0x45a2b0 --callees 3   # 顺带印前 3 层被调函数
    python3 tooling/ghidra/th20_read.py --disasm 0x45a2b0 --len 200
    python3 tooling/ghidra/th20_read.py --xref 0x45a2b0                 # 谁引用了它
    python3 tooling/ghidra/th20_read.py --xref-data 0x574060            # 谁引用了这个数据地址
    python3 tooling/ghidra/th20_read.py --find-name Stone               # 符号名搜
    python3 tooling/ghidra/th20_read.py --funcs-at 0x574060 --count 8    # 地址附近的函数
    python3 tooling/ghidra/th20_read.py --bytes 0x574060 --count 64      # 直接读字节/浮点

`--decomp` 的输出里 `*(int *)(param_1 + 0x3c)` 这种裸偏移是正常现象：
TH20 没有 ExpHP 结构体可绑，offset 只能靠一手读代码认。
"""
import argparse
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import _driver                                                    # noqa: E402


def _addr(prog, s):
    from ghidra.program.model.address import Address
    a = prog.getAddressFactory().getAddress(s if s.startswith("0x") else "0x" + s)
    if a is None:
        raise SystemExit(f"地址解析失败：{s}")
    return a


def _func_at(prog, a):
    fm = prog.getFunctionManager()
    f = fm.getFunctionContaining(a)
    if f is None:
        f = fm.getFunctionAt(a)
    return f


def decompile(prog, entry, depth, seen=None, maxdepth=0, out=None, level=0):
    from ghidra.app.decompiler import DecompInterface
    dec = DecompInterface()
    dec.openProgram(prog)
    seen = seen if seen is not None else set()
    out = out if out is not None else []
    f = _func_at(prog, entry)
    if f is None:
        out.append(f"--- {entry}: 没有函数 ---")
        return out
    ep = str(f.getEntryPoint())
    if ep in seen:
        return out
    seen.add(ep)
    res = dec.decompileFunction(f, 60, None)
    body = res.getDecompiledFunction().getC() if res and res.decompileCompleted() else \
        f"  (反编译失败: {res.getErrorMessage() if res else 'no result'})"
    hdr = f"===== {ep}  {f.getName()}  (深度 {level}) ====="
    out.append(hdr + "\n" + body)
    if level < maxdepth:
        for callee in f.getCalledFunctions(None):
            decompile(prog, callee.getEntryPoint(), depth, seen, maxdepth, out, level + 1)
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--version", default="th20")
    ap.add_argument("--decomp")
    ap.add_argument("--callees", type=int, default=0, help="跟随被调函数的层数")
    ap.add_argument("--disasm")
    ap.add_argument("--len", type=int, default=240)
    ap.add_argument("--xref")
    ap.add_argument("--xref-data")
    ap.add_argument("--find-name")
    ap.add_argument("--funcs-at")
    ap.add_argument("--count", type=int, default=8)
    ap.add_argument("--bytes")
    ap.add_argument("--nbytes", type=int, default=64)
    args = ap.parse_args()

    p = _driver.resolve(args.version)
    _driver.ghidra_install()

    with _driver.open_program(p["proj_dir"], p["proj_name"], p["program"], tx=None) as prog:
        if args.decomp:
            for spec in args.decomp.split(","):
                a = _addr(prog, spec)
                print("\n\n".join(decompile(prog, a, 0, None, args.callees)))
        if args.disasm:
            a = _addr(prog, args.disasm)
            listing = prog.getListing()
            ins = listing.getInstructionAt(a) or listing.getInstructionContaining(a)
            n = 0
            while ins is not None and n < args.len:
                print(f"{ins.getAddress()}  {ins}")
                ins = ins.getNext()
                n += 1
        if args.xref:
          for spec in args.xref.split(","):
            print(f"-- xref to {spec}")
            a = _addr(prog, spec)
            for r in prog.getReferenceManager().getReferencesTo(a):
                fr = r.getFromAddress()
                f = _func_at(prog, fr)
                print(f"{fr}  {r.getReferenceType()}  <- {f.getName() if f else '(无函数)'} "
                      f"[{f.getEntryPoint() if f else ''}]")
        if args.xref_data:
            a = _addr(prog, args.xref_data)
            for r in prog.getReferenceManager().getReferencesTo(a):
                fr = r.getFromAddress()
                f = _func_at(prog, fr)
                print(f"{fr}  {r.getReferenceType()}  <- {f.getName() if f else '(无函数)'} "
                      f"[{f.getEntryPoint() if f else ''}]")
        if args.find_name:
            kw = args.find_name.lower()
            hits = [s for s in prog.getSymbolTable().getAllSymbols(True)
                    if kw in s.getName().lower()]
            for s in hits[:400]:
                print(f"{s.getAddress()}  {s.getSymbolType()}  {s.getSource()}  {s.getName()}")
            print(f"[{len(hits)} 个命中]")
        if args.funcs_at:
            a = _addr(prog, args.funcs_at)
            it = prog.getFunctionManager().getFunctions(a, True)
            for i, f in enumerate(it):
                if i >= args.count:
                    break
                print(f"{f.getEntryPoint()}  {f.getName()}  size={f.getBody().getNumAddresses()}")
        if args.bytes:
            a = _addr(prog, args.bytes)
            import struct
            from jpype import JArray, JByte
            jbuf = JArray(JByte)(args.nbytes)
            prog.getMemory().getBytes(a, jbuf)
            raw = bytes((b & 0xff) for b in jbuf)
            for off in range(0, args.nbytes, 16):
                chunk = raw[off:off + 16]
                hexs = " ".join(f"{b:02x}" for b in chunk)
                f32 = " ".join(f"{struct.unpack('<f', chunk[i:i+4])[0]:.4g}"
                               for i in range(0, len(chunk) - 3, 4))
                print(f"{a.add(off)}  {hexs:<48} | {f32}")


if __name__ == "__main__":
    main()
