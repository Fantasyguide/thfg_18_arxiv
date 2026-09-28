#!/usr/bin/env python3
"""TH20 命名回流：清 FID 假阳性 + 套 RTTI 类名/vftable + 钉 assert 锚点注释。

TH20 太新，`local/vendor/th-re-data/th20.v1.00a` 不存在（`games/th20.v1.00a/INDEX.md` §2c），
所以 Ghidra 里 7315 个函数本来只有 `FUN_xxxx`。好在 exe 自带两样 ZUN 编译进去的线索：

1. **MSVC RTTI**：类名 + `vftable`（`th20_rtti.py` 复原链子）。
2. **assert 字符串**：`...\\src\\game\\weaponred.cpp:42 StoneRedReimuInf`，
   被哪个函数引用，那个函数就是这条断言的归属（`th20_anchors.py` 已 dump 成 JSON）。

同时要做一件**减法**：Ghidra 的 FID 分析器在 ZUN 的代码上打了 73 处假阳性
（`FID_conflict:_Func_impl_no_alloc<...Concurrency::target_block...>`），
这些名字**不但错，还会让反编译器把真函数体前面糊上 7 KB 的库函数说明**。
本脚本把它们清成 `FUN_xxxx` + 去掉库注释。

名字一律标 `SourceType.USER_DEFINED`，且能落盘（`symbols.py export` 收得走）。
「谁是 CalcMain」这种还没坐实的，**只写进注释，不起名** —— 宁可少宣称。

用法：

    python3 tooling/ghidra/th20_names.py --dry-run      # 只印计划
    python3 tooling/ghidra/th20_names.py --apply        # 写库
"""
import argparse
import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import _driver                                                    # noqa: E402


def plan(prog, anchors):
    from ghidra.program.model.symbol import SourceType
    fm = prog.getFunctionManager()
    st = prog.getSymbolTable()
    acts = []
    for f in fm.getFunctions(True):
        nm = f.getName()
        if nm.startswith("FID_conflict"):
            acts.append(("fid", str(f.getEntryPoint()), nm))
    # assert 注释：字符串 → 引用它的函数
    comments = {}
    for row in anchors["string_refs"]:
        if "th20\\src" not in row["string"] or not row["func"]:
            continue
        txt = row["string"]
        i = txt.find("src\\")
        comments.setdefault(row["func"], set()).add(txt[i:] if i >= 0 else txt)
    return acts, comments


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--version", default="th20")
    ap.add_argument("--apply", action="store_true")
    ap.add_argument("--dry-run", action="store_true")
    ap.add_argument("--rtti", default=None, help="th20-rtti.json（默认按约定找）")
    ap.add_argument("--anchors", default=None, help="th20-anchors.json")
    args = ap.parse_args()

    p = _driver.resolve(args.version)
    _driver.ghidra_install()
    rtti_path = Path(args.rtti) if args.rtti else p["vdir"] / f"{args.version}-rtti.json"
    anc_path = Path(args.anchors) if args.anchors else p["vdir"] / f"{args.version}-anchors.json"

    apply = args.apply and not args.dry_run
    tx = "TH20 命名回流" if apply else None

    with _driver.open_program(p["proj_dir"], p["proj_name"], p["program"],
                              tx=tx, commit=apply) as prog:
        from ghidra.program.model.symbol import SourceType
        fm = prog.getFunctionManager()
        st = prog.getSymbolTable()
        af = prog.getAddressFactory().getDefaultAddressSpace()

        # ── 1) FID 假阳性 → FUN_xxxx，并清掉「Library Function」板注释
        fid = [f for f in fm.getFunctions(True) if f.getName().startswith("FID_conflict")]
        print(f"[names] FID 假阳性 {len(fid)} 个")
        for f in fid:
            if apply:
                f.setComment(None)
                for s in list(st.getSymbols(f.getEntryPoint())):
                    if s.getName().startswith("FID_conflict"):
                        st.removeSymbolSpecial(s)
                f.setName(f"FUN_{f.getEntryPoint()}", SourceType.USER_DEFINED)

        # ── 2) RTTI：`Class::vftable` 标签（多继承会有多个 vftable，用 _N 区分）
        rt = json.loads(rtti_path.read_text()) if rtti_path.is_file() else []
        seen = {}
        n_vf = 0
        for x in rt:
            cls = x["cls"]
            if cls.startswith("?") or "@std" in cls or "@" in cls:
                continue                       # 只给 ZUN 自己的类起名，标准库/编译器内部一律放过
            va = int(x["vftable"], 16)
            seen[cls] = seen.get(cls, 0) + 1
            name = f"{cls}::vftable" if seen[cls] == 1 else f"{cls}::vftable_{seen[cls]}"
            a = af.getAddress(va)
            if apply:
                st.createLabel(a, name, SourceType.USER_DEFINED)
            n_vf += 1
        print(f"[names] RTTI vftable 标签 {n_vf} 个（{len(seen)} 个类）")

        # ── 3) assert 锚点：把 `file:line token` 写成板注释（**不起名**，避免过度宣称）
        anc = json.loads(anc_path.read_text()) if anc_path.is_file() else {"string_refs": []}
        by_func = {}
        for row in anc["string_refs"]:
            if "th20\\src" not in row["string"] or not row["func"]:
                continue
            txt = row["string"]
            i = txt.find("src\\")
            by_func.setdefault(row["func"], []).append(txt[i:] if i >= 0 else txt)
        n_c = 0
        for faddr, items in by_func.items():
            a = af.getAddress(faddr)
            f = fm.getFunctionAt(a)
            if not f:
                continue
            body = "\n".join(sorted(set(items)))
            if apply:
                old = f.getComment() or ""
                new = (old + "\n" if old else "") + "assert 锚点:\n" + body
                f.setComment(new)
            n_c += 1
        print(f"[names] assert 锚点注释 {n_c} 个函数")
        if not apply:
            print("[names] dry-run：没有写库（加 --apply 才写）")


if __name__ == "__main__":
    main()
