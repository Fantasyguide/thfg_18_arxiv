#!/usr/bin/env python3
r"""
md2wikitext.py — 把 engine/msg/WIKI-DRAFT.md（markdown 母本）转成 THBWiki 可贴的 MediaWiki wikitext。

    python3 engine/msg/tools/md2wikitext.py engine/msg/WIKI-DRAFT.md > engine/msg/WIKI-DRAFT.wikitext

只处理这份稿子用到的子集,不是通用转换器:
- 丢掉文件顶部的 blockquote 说明块、`---` 分隔线、H1 标题(wiki 页名自带)。
- 丢掉每节末尾的「〔依据〕」段(到下一个空行为止)——那是给仓库看的证据指针。
- `## ` / `### ` → `== ==` / `=== ===`;``` 围栏 → <pre>;`**x**` → '''x''';`` `x` `` → <code><nowiki>x</nowiki></code>。
- markdown 管道表 → {| class="wikitable" … |},表头用 !!、行用 || 连接(THBWiki 惯用格式);
  单元格里的字面 `\|` → <nowiki>|</nowiki>。
- `[text](path)` 仓库内链接 → 只留 text。
"""
import re
import sys

INLINE_CODE = re.compile(r"`([^`]+)`")
BOLD = re.compile(r"\*\*(.+?)\*\*")
LINK = re.compile(r"\[([^\]]+)\]\([^)]+\)")


def inline(s: str, in_table: bool = False) -> str:
    s = LINK.sub(r"\1", s)
    s = INLINE_CODE.sub(lambda m: "<code><nowiki>%s</nowiki></code>" % m.group(1).replace("\\|", "|"), s)
    s = BOLD.sub(r"'''\1'''", s)
    s = s.replace("\\|", "<nowiki>|</nowiki>" if in_table else "|")
    return s


def split_row(line: str):
    body = line.strip()
    assert body.startswith("|") and body.endswith("|"), line
    cells, cur, i = [], "", 1
    while i < len(body) - 1:
        c = body[i]
        if c == "\\" and i + 1 < len(body) and body[i + 1] == "|":
            cur += "\\|"
            i += 2
            continue
        if c == "|":
            cells.append(cur.strip())
            cur = ""
        else:
            cur += c
        i += 1
    cells.append(cur.strip())
    return cells


def is_sep(line: str) -> bool:
    return re.fullmatch(r"\s*\|(\s*:?-+:?\s*\|)+\s*", line) is not None


def convert(md: str) -> str:
    lines = md.split("\n")
    out = []
    i = 0
    # 1) 跳过顶部说明块:直到第一条 --- 之后
    while i < len(lines) and lines[i].strip() != "---":
        i += 1
    i += 1
    in_pre = False
    while i < len(lines):
        line = lines[i]
        if line.startswith("```"):
            out.append("</pre>" if in_pre else "<pre>")
            in_pre = not in_pre
            i += 1
            continue
        if in_pre:
            out.append(line.replace("\\|", "|"))
            i += 1
            continue
        if line.strip() == "---":
            i += 1
            continue
        if line.startswith("〔依据〕"):
            while i < len(lines) and lines[i].strip():
                i += 1
            continue
        if line.startswith("# "):
            i += 1
            continue
        m = re.match(r"^(#{2,4}) (.*)$", line)
        if m:
            lvl = len(m.group(1))
            out.append("%s %s %s" % ("=" * lvl, inline(m.group(2)), "=" * lvl))
            i += 1
            continue
        if line.lstrip().startswith("|") and i + 1 < len(lines) and is_sep(lines[i + 1]):
            header = split_row(line)
            i += 2
            rows = []
            while i < len(lines) and lines[i].lstrip().startswith("|"):
                rows.append(split_row(lines[i]))
                i += 1
            out.append('{| class="wikitable"')
            out.append("|-")
            out.append("! " + " !! ".join(inline(c, True) for c in header))
            for r in rows:
                out.append("|-")
                out.append("| " + " || ".join(inline(c, True) for c in r))
            out.append("|}")
            continue
        m = re.match(r"^(\s*)- (.*)$", line)
        if m:
            depth = len(m.group(1)) // 2 + 1
            out.append("*" * depth + " " + inline(m.group(2)))
            i += 1
            continue
        m = re.match(r"^(\s*)\d+\. (.*)$", line)
        if m:
            depth = len(m.group(1)) // 3 + 1
            out.append("#" * depth + " " + inline(m.group(2)))
            i += 1
            continue
        if re.match(r"^\s{2,}\S", line) and out and (out[-1].startswith("*") or out[-1].startswith("#")):
            out[-1] += " " + inline(line.strip())   # 列表项的续行
            i += 1
            continue
        out.append(inline(line))
        i += 1
    # 压掉连续空行
    text = re.sub(r"\n{3,}", "\n\n", "\n".join(out)).strip() + "\n"
    return text


if __name__ == "__main__":
    import argparse
    ap = argparse.ArgumentParser()
    ap.add_argument("src", nargs="?", default="engine/msg/WIKI-DRAFT.md")
    ap.add_argument("--shift", type=int, default=0,
                    help="标题层级整体下沉 N 级(贴进《脚本对照表/其他》的 ===MSG文件=== 之下用 1;独立子页用 0)")
    a = ap.parse_args()
    text = convert(open(a.src, encoding="utf-8").read())
    if a.shift:
        text = re.sub(r"^(=+) (.*?) \1$", lambda m: "=" * (len(m.group(1)) + a.shift) + " " + m.group(2) + " " + "=" * (len(m.group(1)) + a.shift), text, flags=re.M)
    sys.stdout.write(text)
