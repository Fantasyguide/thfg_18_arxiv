#!/usr/bin/env python3
"""文档规范检查器 —— 规范见 DOCSTYLE.md。

    python tooling/check-docs.py              # 全部检查，有违规则 exit 1
    python tooling/check-docs.py --only addr  # 只跑某一项:links/addr/version/tables
    python tooling/check-docs.py --orphans    # 附带报告无入链文档

一条不被脚本检查的规范，半年后就是一份考古材料 —— 所以规范和这个文件必须同步改。
"""
import argparse, hashlib, re, subprocess, sys
from pathlib import Path
from urllib.parse import unquote

REPO = Path(__file__).resolve().parents[1]

# ── 通用 ────────────────────────────────────────────────────────────────
FENCE = re.compile(r"^\s*(```|~~~)")
SPLIT_CODE = re.compile(r"(`[^`\n]*`)")

ADDR = r"0x[0-9a-fA-F]{6,8}"
# 哨兵/常量不是地址，免于版本前缀
SENTINELS = {"0xffffffff", "0x00000000", "0x400000", "0xfffffffe"}
BARE_ADDR = re.compile(r"(?<![\w`./-])(" + ADDR + r")(?![\w`])")
AT_ADDR = re.compile(r"@[ \t]*[`*]*" + ADDR)
TICKED_ADDR = re.compile(r"`(?:(th\d+):)?(" + ADDR + r")`")

VERSION_BANNER = re.compile(r"^>\s*\*\*版本\*\*[:：]\s*(TH\d+|跨版本)", re.M)
# 这三份讲的是「怎么写」，里面的地址是记法示例而非结论，不要求版本声明。
NOTATION_DOCS = {"README.md", "CLAUDE.md", "DOCSTYLE.md"}

LINK = re.compile(r"\[[^\]]*\]\(([^)\s]+)(?:\s+\"[^\"]*\")?\)")
LINK_SKIP = re.compile(r"^(https?:|mailto:|#|<)")
PATHISH = re.compile(r"(/|\.(md|py|sh|json|txt|js|c|h|asm|diff|sht|msg|ecl|anm|toml|yml)$)", re.I)

MAX_CELL = 200          # 表格单元格字符上限
BASELINE = Path(__file__).resolve().parent / "docs-baseline.txt"
BANNER_SCAN_LINES = 20  # 版本声明必须出现在开头这么多行内


def md_files():
    out = subprocess.run(["git", "-C", str(REPO), "ls-files", "-z", "*.md"],
                         capture_output=True, text=True, check=True).stdout
    return [REPO / p for p in out.split("\0") if p]


def prose_lines(text):
    """产出 (行号, 该行去掉行内代码后的散文部分, 原始行)，跳过围栏代码块。"""
    in_fence = False
    for i, line in enumerate(text.split("\n"), 1):
        if FENCE.match(line):
            in_fence = not in_fence
            continue
        if in_fence:
            continue
        parts = SPLIT_CODE.split(line)
        yield i, "".join(parts[0::2]), line


# ── 检查项 ──────────────────────────────────────────────────────────────
def check_links(files):
    bad = []
    for f in files:
        text = f.read_text(encoding="utf-8", errors="replace")
        stripped = "\n".join(p for _, p, _ in prose_lines(text))
        for m in LINK.finditer(stripped):
            raw = m.group(1)
            if LINK_SKIP.match(raw) or not PATHISH.search(raw):
                continue
            target = unquote(raw.split("#", 1)[0])
            if target and not (f.parent / target).resolve().exists():
                bad.append((f, None, f"链接指向不存在的文件: {raw}"))
    return bad


def check_addr(files):
    """地址必须写成 `0xXXXXXX`（可带版本前缀），不许裸写、不许 @ 前缀。"""
    bad = []
    for f in files:
        for ln, prose, raw in prose_lines(f.read_text(encoding="utf-8", errors="replace")):
            for m in BARE_ADDR.finditer(prose):
                bad.append((f, ln, f"地址未加反引号: {m.group(1)}"))
            for m in AT_ADDR.finditer(prose):
                bad.append((f, ln, f"地址带 @ 前缀: {m.group(0)}"))
    return bad


def check_version(files):
    """引用了真实地址的文档必须声明默认版本；跨版本文档的地址必须带版本前缀。"""
    bad = []
    for f in files:
        if str(f.relative_to(REPO)) in NOTATION_DOCS:
            continue
        text = f.read_text(encoding="utf-8", errors="replace")
        addrs = [(ln, m) for ln, _, raw in prose_lines(text)
                 for m in TICKED_ADDR.finditer(raw)
                 if m.group(2).lower() not in SENTINELS]
        if not addrs:
            continue
        head = "\n".join(text.split("\n")[:BANNER_SCAN_LINES])
        m = VERSION_BANNER.search(head)
        if not m:
            bad.append((f, 1, f"引用了 {len(addrs)} 个地址但未在开头 "
                              f"{BANNER_SCAN_LINES} 行内声明 **版本**"))
            continue
        if m.group(1) == "跨版本":
            for ln, am in addrs:
                if not am.group(1):
                    bad.append((f, ln, f"跨版本文档里的地址须带版本前缀: `{am.group(2)}`"))
    return bad


def cell_id(rel, cell):
    """用单元格内容做键 —— 行号会随无关编辑漂移，内容不会。"""
    return hashlib.sha1(f"{rel}\0{cell.strip()}".encode()).hexdigest()[:12]


def load_baseline():
    if not BASELINE.exists():
        return set()
    return {l.split()[0] for l in BASELINE.read_text(encoding="utf-8").splitlines()
            if l.strip() and not l.startswith("#")}


def scan_tables(files):
    """产出 (文件, 行号, 单元格, 键)。表格是索引不是容器:一格 = 一个短句。"""
    for f in files:
        in_fence = False
        for ln, line in enumerate(f.read_text(encoding="utf-8", errors="replace").split("\n"), 1):
            if FENCE.match(line):
                in_fence = not in_fence
                continue
            if in_fence or not line.lstrip().startswith("|"):
                continue
            for cell in line.split("|")[1:-1]:
                if len(cell) > MAX_CELL:
                    yield f, ln, cell, cell_id(f.relative_to(REPO), cell)


def check_tables(files):
    base = load_baseline()
    bad = []
    for f, ln, cell, key in scan_tables(files):
        if key in base:                      # 已登记的历史欠账,不再报错
            continue
        bad.append((f, ln, f"表格单元格 {len(cell)} 字符 > {MAX_CELL}，"
                           f"拆成小节、表格只留链接（key {key}）"))
    return bad


CHECKS = {"links": check_links, "addr": check_addr,
          "version": check_version, "tables": check_tables}


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--only", choices=sorted(CHECKS), help="只跑一项检查")
    ap.add_argument("--orphans", action="store_true", help="附带报告无入链文档")
    ap.add_argument("--update-baseline", action="store_true",
                    help="把当前所有超长表格单元格登记为历史欠账（棘轮:只允许变少）")
    a = ap.parse_args()

    files = md_files()
    if a.update_baseline:
        rows = sorted((k, str(f.relative_to(REPO)), len(c), c.strip()[:56])
                      for f, _, c, k in scan_tables(files))
        BASELINE.write_text(
            "# 超长表格单元格的历史欠账 —— 见 DOCSTYLE.md「表格」。\n"
            "# 键 = sha1(相对路径 + 单元格内容)[:12]，与行号无关。\n"
            "# 只允许变少:改好一处就删一行。新增的违规会直接报错。\n"
            + "".join(f"{k}  {p}  {n}字符  {t}\n" for k, p, n, t in rows),
            encoding="utf-8")
        print(f"已登记 {len(rows)} 处历史欠账 -> {BASELINE.relative_to(REPO)}")
        return 0
    names = [a.only] if a.only else list(CHECKS)
    total = 0
    for name in names:
        bad = CHECKS[name](files)
        total += len(bad)
        if bad:
            print(f"\n✗ [{name}] {len(bad)} 处：")
            for f, ln, msg in bad[:40]:
                loc = f"{f.relative_to(REPO)}" + (f":{ln}" if ln else "")
                print(f"  {loc}\n      {msg}")
            if len(bad) > 40:
                print(f"  … 另有 {len(bad) - 40} 处")
        else:
            print(f"✓ [{name}]")

    if a.orphans:
        inbound = {f: 0 for f in files}
        for f in files:
            for m in LINK.finditer(f.read_text(encoding="utf-8", errors="replace")):
                if not LINK_SKIP.match(m.group(1)) and PATHISH.search(m.group(1)):
                    d = (f.parent / unquote(m.group(1).split("#")[0])).resolve()
                    if d in inbound:
                        inbound[d] += 1
        orph = [f.relative_to(REPO) for f, n in inbound.items() if n == 0
                and f.name not in ("README.md", "CLAUDE.md", "METHOD.md", "DOCSTYLE.md", "INDEX.md")]
        if orph:
            print(f"\n⚠ {len(orph)} 个文档无入链：")
            for o in sorted(orph):
                print(f"  {o}")

    print(f"\n{'✓ 全部通过' if total == 0 else f'✗ 合计 {total} 处违规'}（{len(files)} 个文件）")
    return 1 if total else 0


if __name__ == "__main__":
    sys.exit(main())
