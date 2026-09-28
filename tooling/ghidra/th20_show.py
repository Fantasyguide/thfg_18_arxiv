#!/usr/bin/env python3
"""从反编译语料库里按地址取函数 —— **不占 Ghidra 锁**，可多人/多 agent 并行。

语料库由 `th20_decomp_corpus.py` 生成（`local/th20.v1.00a/decomp/*.c`）。
工具链的锁是独占的，所以「并行挖同一个 exe」的唯一办法是：一个人生成语料，其余人读文件。

用法：

    python3 tooling/ghidra/th20_show.py 0x5120d0              # 印整个函数
    python3 tooling/ghidra/th20_show.py 0x5120d0 --head 40    # 只印前 40 行
    python3 tooling/ghidra/th20_show.py --grep "StoneAttack"  # 全语料搜（带函数头）
    python3 tooling/ghidra/th20_show.py --callers 0x532330    # 谁调用它（语料内的静态搜索）
    python3 tooling/ghidra/th20_show.py --list --range 0x534000-0x535000
"""
import argparse
import re
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
CORPUS = REPO / "local/th20.v1.00a/decomp"

HDR = re.compile(r"^// ===== (\S+)  (\S+)  size=(\d+) =====$", re.M)


def load(paths):
    funcs = []          # (addr, name, size, file, body)
    for p in paths:
        text = p.read_text(errors="replace")
        marks = list(HDR.finditer(text))
        for i, m in enumerate(marks):
            end = marks[i + 1].start() if i + 1 < len(marks) else len(text)
            funcs.append((m.group(1), m.group(2), int(m.group(3)), p.name,
                          text[m.end():end]))
    return funcs


def norm(a):
    return a.lower().replace("0x", "").lstrip("0").rjust(1, "0")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("addr", nargs="?")
    ap.add_argument("--head", type=int, default=0)
    ap.add_argument("--grep")
    ap.add_argument("--callers", help="印调用这个地址的函数（按语料文本静态匹配）")
    ap.add_argument("--list", action="store_true")
    ap.add_argument("--range", help="0x起-0x止")
    ap.add_argument("--corpus", default=str(CORPUS))
    args = ap.parse_args()

    cdir = Path(args.corpus)
    if not cdir.is_dir():
        sys.exit(f"没有语料库 {cdir} —— 先跑 tooling/ghidra/th20_decomp_corpus.py")
    funcs = load(sorted(cdir.glob("*.c")))

    if args.list:
        lo = hi = None
        if args.range:
            a, _, b = args.range.partition("-")
            lo, hi = int(a, 16), int(b, 16)
        for addr, name, size, fn, _ in funcs:
            if lo is not None and not (lo <= int(addr, 16) <= hi):
                continue
            print(f"{addr}  {name:52s} size={size:5d}  {fn}")
        print(f"[{len(funcs)} 个函数]")
        return

    if args.grep:
        pat = re.compile(args.grep)
        hits = 0
        for addr, name, size, fn, body in funcs:
            if pat.search(body) or pat.search(name):
                print(f"--- {addr} {name} ({fn})")
                for line in body.splitlines():
                    if pat.search(line):
                        print("   ", line.strip()[:200])
                hits += 1
        print(f"[{hits} 个函数命中]")
        return

    if args.callers:
        tgt = int(args.callers, 16)
        var = f"{tgt:#010x}".replace("0x", "")
        pat = re.compile(rf"\b0x0*{var.lstrip('0')}\b", re.I)
        for addr, name, size, fn, body in funcs:
            # 只搜函数体（含间接调用/取地址）
            if pat.search(body):
                calls = [l.strip() for l in body.splitlines() if pat.search(l)]
                print(f"--- {addr} {name} ({fn})")
                for c in calls[:6]:
                    print("   ", c[:180])
        return

    if args.addr:
        want = norm(args.addr)
        for addr, name, size, fn, body in funcs:
            if norm(addr) == want:
                print(f"// {addr} {name} size={size} ({fn})")
                lines = body.splitlines()
                print("\n".join(lines[:args.head] if args.head else lines))
                return
        sys.exit(f"语料库里没有 {args.addr}")


if __name__ == "__main__":
    main()
