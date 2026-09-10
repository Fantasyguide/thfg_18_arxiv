#!/usr/bin/env python3
"""把 th06-re 的 Ghidra XML 导出当符号源，叠进我们自己的 TH06 库。

`local/vendor/th06-re-xml/th06_102h.exe.xml` 是社区工程的 Ghidra XML 导出，
里面有 **1520 个函数名**、718 条签名注释和 1230 条注释——比 ExpHP 的
`th-re-data`（TH06 只有 274 个函数）丰富得多。

XML 引用的 `th06_102h.exe.bytes` 我们没有，所以**不能**直接 import 成一个程序；
本脚本只把它当**符号/注释来源**，套到已经用真 exe 建好的库上。

⚠️ 我们手上的 exe 与 th06-decomp README 记的 SHA256 不同（发行包装不同），
但代码布局一致。脚本因此不比对整文件哈希，而是**验地址对齐**：
XML 的函数入口必须有足够高的比例落在库里已识别的函数上，否则直接中止——
对不齐还硬套 = `_driver.py` 开头列的第 4 号坑。

用法：

    source tooling/env.sh
    python3 tooling/ghidra/import_th06_re_xml.py --dry-run
    python3 tooling/ghidra/import_th06_re_xml.py

安全默认：只填 Ghidra 的默认名（`FUN_*` / 无符号），**不动**你或 ExpHP 已经命名的东西；
要覆盖加 `--overwrite`。
"""
import argparse
import os
import re
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

from _driver import REPO, open_program, resolve  # noqa: E402

VERSION = "th06.v1.02h"
XML_PATH = REPO / "local/vendor/th06-re-xml/th06_102h.exe.xml"
EXPECTED_MD5 = "99920041bc37e3ad4dc0201af84244c1"

# 对齐闸门：XML 函数入口命中库中已识别函数的最低比例。
MIN_ALIGNMENT = 0.70

# Ghidra 自动生成的符号，套过来没有任何价值，只会把库弄脏。
NOISE = re.compile(r"^(switchD|caseD|prt_|LAB_|DAT_|FUN_|SUB_|s_|u_|PTR_|__|\?)")

_FUNC = re.compile(r'<FUNCTION ENTRY_POINT="([0-9a-fA-F]+)" NAME="([^"]*)"')
_TYPEINFO = re.compile(r"<TYPEINFO_CMT>(.*?)</TYPEINFO_CMT>", re.S)
_SYMBOL = re.compile(
    r'<SYMBOL ADDRESS="([0-9a-fA-F]+)" NAME="([^"]*)" NAMESPACE="([^"]*)"'
)
_COMMENT = re.compile(
    r'<COMMENT ADDRESS="([0-9a-fA-F]+)" TYPE="([a-z-]+)">(.*?)</COMMENT>', re.S
)


def unescape(text):
    for a, b in (("&lt;", "<"), ("&gt;", ">"), ("&quot;", '"'), ("&apos;", "'"), ("&amp;", "&")):
        text = text.replace(a, b)
    return re.sub(r"&#x([0-9a-fA-F]+);", lambda m: chr(int(m.group(1), 16)), text)


def flatten(name):
    """th06::AsciiManager::OnUpdate -> AsciiManager__OnUpdate（与仓库现有命名一致）。"""
    if name.startswith("th06::"):
        name = name[len("th06::") :]
    return re.sub(r"[^A-Za-z0-9_]", "_", name.replace("::", "__"))


def parse(xml_text):
    """返回 (functions, data_symbols, comments)。"""
    functions = []
    for m in _FUNC.finditer(xml_text):
        address = int(m.group(1), 16)
        name = unescape(m.group(2))
        # TYPEINFO_CMT 紧跟在 FUNCTION 之后（同一条记录内）
        window = xml_text[m.end() : m.end() + 600]
        sig = _TYPEINFO.search(window)
        functions.append((address, name, unescape(sig.group(1)).strip() if sig else None))

    entry_points = set(a for a, _n, _s in functions)

    data_symbols = []
    for m in _SYMBOL.finditer(xml_text):
        address = int(m.group(1), 16)
        if address in entry_points:
            continue                       # 函数名走 functions 那条路
        name, namespace = unescape(m.group(2)), unescape(m.group(3))
        if NOISE.match(name) or NOISE.match(namespace.split("::")[0]):
            continue
        data_symbols.append((address, (namespace + name) if namespace else name))

    comments = []
    for m in _COMMENT.finditer(xml_text):
        text = unescape(m.group(3)).strip()
        # 段名那几条（".text" / ".rdata" …）纯噪音
        if not text or (len(text) < 8 and text.startswith(".")):
            continue
        comments.append((int(m.group(1), 16), m.group(2), text))

    return functions, data_symbols, comments


def apply(program, functions, data_symbols, comments, dry, overwrite, want_comments):
    from ghidra.program.model.listing import CodeUnit
    from ghidra.program.model.symbol import SourceType

    user, default = SourceType.USER_DEFINED, SourceType.DEFAULT
    to_addr = program.getAddressFactory().getDefaultAddressSpace().getAddress
    fm = program.getFunctionManager()
    st = program.getSymbolTable()
    listing = program.getListing()

    stats = dict(
        funcs_applied=0, funcs_overwritten=0, funcs_skipped=0, funcs_missing=0,
        data_applied=0, data_skipped=0, comments=0, sigs=0,
    )

    # ---- 对齐闸门：先数，不写 ----
    present = sum(1 for a, _n, _s in functions if fm.getFunctionAt(to_addr(a)) is not None)
    ratio = present / len(functions) if functions else 0.0
    print("[align] XML 函数 %d，库中已识别 %d（%.1f%%）" % (len(functions), present, ratio * 100))
    if ratio < MIN_ALIGNMENT:
        raise RuntimeError(
            "地址对不齐（命中率 %.1f%% < %.0f%%）——exe 与 XML 很可能不是同一 build，已中止"
            % (ratio * 100, MIN_ALIGNMENT * 100)
        )

    for address, name, signature in functions:
        location = to_addr(address)
        function = fm.getFunctionAt(location)
        if function is None:
            stats["funcs_missing"] += 1
            continue
        symbol = function.getSymbol()
        named = symbol is not None and symbol.getSource() != default
        if named and not overwrite:
            stats["funcs_skipped"] += 1
        else:
            if not dry:
                function.setName(flatten(name), user)
            stats["funcs_overwritten" if named else "funcs_applied"] += 1
        if signature:
            existing = listing.getComment(CodeUnit.PLATE_COMMENT, location)
            if existing is None or overwrite:
                if not dry:
                    listing.setComment(
                        location, CodeUnit.PLATE_COMMENT, "[th06-re] " + signature
                    )
                stats["sigs"] += 1

    for address, name in data_symbols:
        location = to_addr(address)
        primary = st.getPrimarySymbol(location)
        named = primary is not None and not primary.isDynamic() and primary.getSource() != default
        if named and not overwrite:
            stats["data_skipped"] += 1
            continue
        if not dry:
            st.createLabel(location, flatten(name), user)
        stats["data_applied"] += 1

    if want_comments:
        kinds = {
            "plate": CodeUnit.PLATE_COMMENT,
            "pre": CodeUnit.PRE_COMMENT,
            "post": CodeUnit.POST_COMMENT,
            "end-of-line": CodeUnit.EOL_COMMENT,
            "repeatable": CodeUnit.REPEATABLE_COMMENT,
        }
        for address, kind, text in comments:
            ctype = kinds.get(kind)
            if ctype is None:
                continue
            location = to_addr(address)
            if listing.getComment(ctype, location) is not None and not overwrite:
                continue
            if not dry:
                listing.setComment(location, ctype, "[th06-re] " + text)
            stats["comments"] += 1

    return stats


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--dry-run", action="store_true", help="只统计，不落盘")
    parser.add_argument("--overwrite", action="store_true", help="覆盖已有名字/注释")
    parser.add_argument("--no-comments", action="store_true", help="不导入注释")
    parser.add_argument("--xml", default=str(XML_PATH), help="覆盖 XML 路径")
    args = parser.parse_args()

    xml = Path(args.xml)
    if not xml.is_file():
        raise SystemExit(
            "找不到 %s —— 它在 local/vendor/（gitignored），"
            "见 engine/_shared/community-sources.md" % xml
        )

    text = xml.read_text(encoding="utf-8", errors="replace")
    functions, data_symbols, comments = parse(text)
    print(
        "[xml] 函数 %d · 数据符号 %d（已滤噪音）· 注释 %d"
        % (len(functions), len(data_symbols), len(comments))
    )

    resolved = resolve(VERSION)
    with open_program(
        resolved["proj_dir"],
        resolved["proj_name"],
        resolved["program"],
        tx="import th06-re XML symbols",
        commit=not args.dry_run,
        expect_md5=EXPECTED_MD5,
    ) as program:
        stats = apply(
            program, functions, data_symbols, comments,
            dry=args.dry_run, overwrite=args.overwrite,
            want_comments=not args.no_comments,
        )

    mode = "dry-run/rolled-back" if args.dry_run else "saved"
    print(
        "[th06-re-xml] %s funcs applied=%d overwritten=%d skipped=%d missing=%d "
        "| sigs=%d | data applied=%d skipped=%d | comments=%d"
        % (
            mode, stats["funcs_applied"], stats["funcs_overwritten"],
            stats["funcs_skipped"], stats["funcs_missing"], stats["sigs"],
            stats["data_applied"], stats["data_skipped"], stats["comments"],
        )
    )


if __name__ == "__main__":
    main()
