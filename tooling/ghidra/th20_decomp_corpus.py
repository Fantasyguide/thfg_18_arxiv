#!/usr/bin/env python3
"""TH20 反编译语料库：把指定地址区间里的函数一次性 dump 成文本，供离线分析/并行分工。

为什么要有这个：**Ghidra 工程锁是独占的**（实测两个只读会话也会撞锁，见
`tooling/ghidra/_driver.py` 的 `_LOCK_HINT`）。所以「几个 agent 同时挖同一个 exe」
不能各开一个 Ghidra —— 只能一个人开库、把反编译落成文件，其余人读文件。

落盘位置在 `local/`（gitignored）：反编译原文属于版权字节，不入库。

用法：

    python3 tooling/ghidra/th20_decomp_corpus.py                    # 默认：锦上京异变石相关区间
    python3 tooling/ghidra/th20_decomp_corpus.py --range 0x530000-0x539000:weapon
    python3 tooling/ghidra/th20_decomp_corpus.py --list             # 只列区间，不跑

输出：

    local/th20.v1.00a/decomp/<名字>.c    每个区间一个文件，函数用 `// ===== addr name =====` 分隔
    local/th20.v1.00a/decomp/INDEX.tsv   addr / size / name / 文件  —— 给 grep 用
"""
import argparse
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import _driver                                                    # noqa: E402

# 默认区间按「源文件聚类」切：ZUN 一个 .cpp 编出来的函数基本连在一起，
# 锚点地址（`th20_anchors.py` 那 156 条 assert 串）标出了每个 .cpp 的大致位置。
DEFAULT_RANGES = [
    ("effect_stone", "0x460700-0x468000"),     # efflinestone / efflinestone2 / effstonegaugeline
    ("item", "0x4c4d00-0x4c7000"),             # game/item.cpp（碎片道具）
    ("score", "0x50e900-0x511000"),            # score.cpp + smallscore.cpp（异变值）
    ("stoneenemy", "0x511000-0x515000"),       # game/stoneenemy.cpp
    ("stonemenu", "0x515000-0x51d000"),        # game/stonemenu.cpp + 邻近
    ("weapon", "0x52f000-0x539000"),           # weaponstone + weapon{red,blue,yellow,green}*.cpp
    ("player_hit", "0x4f0000-0x506500"),       # player.cpp / plyshot.cpp（自机、Miss）
    ("bomb", "0x478000-0x47b000"),             # bomb.cpp（炸弹/决死对比）
]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--version", default="th20")
    ap.add_argument("--range", action="append", default=None,
                    help="名字:起-止（可多次），不给就用默认表")
    ap.add_argument("--list", action="store_true")
    ap.add_argument("--outdir", default=None)
    args = ap.parse_args()

    ranges = DEFAULT_RANGES
    if args.range:
        ranges = []
        for spec in args.range:
            name, _, rng = spec.partition(":")
            lo, _, hi = rng.partition("-")
            ranges.append((name, f"{lo}-{hi}"))
    if args.list:
        for n, r in ranges:
            print(f"{n:14s} {r}")
        return

    p = _driver.resolve(args.version)
    _driver.ghidra_install()
    outdir = Path(args.outdir) if args.outdir else p["vdir"] / "decomp"
    outdir.mkdir(parents=True, exist_ok=True)

    with _driver.open_program(p["proj_dir"], p["proj_name"], p["program"], tx=None) as prog:
        from ghidra.app.decompiler import DecompInterface
        dec = DecompInterface()
        dec.openProgram(prog)
        af = prog.getAddressFactory().getDefaultAddressSpace()
        fm = prog.getFunctionManager()
        index = []
        for name, rng in ranges:
            lo, _, hi = rng.partition("-")
            it = fm.getFunctions(af.getAddress(lo.strip()), True)
            hiaddr = af.getAddress(hi.strip())
            lines = []
            n = 0
            for f in it:
                if f.getEntryPoint().compareTo(hiaddr) > 0:
                    break
                res = dec.decompileFunction(f, 60, None)
                body = (res.getDecompiledFunction().getC()
                        if res and res.decompileCompleted() else "// 反编译失败\n")
                lines.append(f"// ===== {f.getEntryPoint()}  {f.getName()}  "
                             f"size={f.getBody().getNumAddresses()} =====\n{body}\n")
                index.append((str(f.getEntryPoint()), str(f.getBody().getNumAddresses()),
                              f.getName(), f"{name}.c"))
                n += 1
            (outdir / f"{name}.c").write_text("".join(lines))
            print(f"[corpus] {name:14s} {rng:22s} {n:5d} 个函数 → {name}.c "
                  f"({(outdir / (name + '.c')).stat().st_size // 1024} KB)")
        with (outdir / "INDEX.tsv").open("w") as fh:
            fh.write("addr\tsize\tname\tfile\n")
            for row in index:
                fh.write("\t".join(row) + "\n")
        print(f"[corpus] 共 {len(index)} 个函数 → {outdir}/INDEX.tsv")


if __name__ == "__main__":
    main()
