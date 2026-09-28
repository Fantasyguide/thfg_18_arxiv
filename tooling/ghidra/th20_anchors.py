#!/usr/bin/env python3
"""TH20 锚点提取：RTTI 类名 + ZUN 的 assert 字符串 → 函数地址。

为什么需要这个：TH20 太新，社区金矿 `local/vendor/th-re-data/th20.v1.00a` 还不存在
（见 `games/th20.v1.00a/INDEX.md` §2c），Ghidra 里 7315 个函数只有 C++ 库的 FID 名。

但 TH20 的 exe 里有两样自带的命名金矿，比 th-re-data 还硬（都是 ZUN 自己编译进去的）：

1. **MSVC RTTI**：`.?AVStoneRedReimuInf@@` 这类 type descriptor ⇒ 类名。
2. **assert 字符串**：`D:\\cygwin\\home\\zun\\prog\\th20\\src\\game\\weaponred.cpp:42 StoneRedReimuInf`
   —— 文件 + 行号 + 变量名，每条只被一个函数引用 ⇒ 那个函数就是它的锚。

本脚本只读不写：把「字符串 → 引用它的函数」全部 dump 成 JSON，供后续命名脚本消费。
用法：

    python3 tooling/ghidra/th20_anchors.py [--out FILE] [--grep 关键字]

不给 --grep 就全量导出。
"""
import argparse
import json
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import _driver                                                    # noqa: E402


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--version", default="th20")
    ap.add_argument("--out", default=None)
    ap.add_argument("--grep", default=None, help="只导出名字匹配这个正则的符号")
    args = ap.parse_args()

    p = _driver.resolve(args.version)
    _driver.ghidra_install()
    out = Path(args.out) if args.out else p["vdir"] / f"{args.version}-anchors.json"
    pat = re.compile(args.grep) if args.grep else None

    rows = []
    with _driver.open_program(p["proj_dir"], p["proj_name"], p["program"], tx=None) as prog:
        from ghidra.program.model.symbol import SymbolType

        # 1) 所有字符串 + 引用它的函数
        st = prog.getListing()
        refmgr = prog.getReferenceManager()
        fm = prog.getFunctionManager()
        n_str = 0
        for data in st.getDefinedData(True):
            v = data.getValue()
            if not isinstance(v, str):
                continue
            n_str += 1
            if pat and not pat.search(v):
                continue
            for ref in refmgr.getReferencesTo(data.getAddress()):
                f = fm.getFunctionContaining(ref.getFromAddress())
                rows.append(dict(
                    kind="string",
                    string=v,
                    string_addr=str(data.getAddress()),
                    from_addr=str(ref.getFromAddress()),
                    func=str(f.getEntryPoint()) if f else None,
                    func_name=f.getName() if f else None,
                    ref_type=str(ref.getReferenceType()),
                ))

        # 2) 符号表里的类名痕迹（Ghidra 的 MSVC RTTI 分析器会建 vftable / RTTI 标签）
        syms = []
        for s in prog.getSymbolTable().getAllSymbols(True):
            nm = s.getName()
            if pat and not pat.search(nm):
                continue
            if s.getSymbolType() in (SymbolType.LABEL, SymbolType.FUNCTION,
                                     SymbolType.CLASS, SymbolType.NAMESPACE):
                syms.append(dict(
                    name=nm,
                    addr=str(s.getAddress()),
                    type=str(s.getSymbolType()),
                    source=str(s.getSource()),
                    parent=(s.getParentNamespace().getName()
                            if s.getParentNamespace() else None),
                ))

    blob = dict(version=args.version, exe_md5=_md5(p["exe"]),
                string_refs=rows, symbols=syms)
    out.write_text(json.dumps(blob, ensure_ascii=False, indent=1))
    print(f"[th20] 字符串引用 {len(rows)} 条 / 符号 {len(syms)} 个 → {out}")


def _md5(path):
    import hashlib
    return hashlib.md5(Path(path).read_bytes()).hexdigest()


if __name__ == "__main__":
    main()
