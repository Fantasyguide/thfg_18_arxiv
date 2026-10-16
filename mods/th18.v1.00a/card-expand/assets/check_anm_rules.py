#!/usr/bin/env python3
"""check_anm_rules.py —— anm 脚本的「**看不见**」类静默错误 linter。

## 为什么要有它（owner 2026-10-15：「怎么 109 改起来困难重重」的成因之一）

109（TH09 翻译卡）的观感全压在 anm 脚本上，而这类错误**不报错、只是看不见** ✗：
  · 弧类 opcode（`texArcEven` / `texArcOdd` / …，anmm **601/600** 一族）**缺 `iset($I1, …)`**
    ⇒ `dv = 0` ⇒ 整片只采同一行纹素 ⇒ 像被预乘了 α（`228_ufo_gauge_arc` 注释里记的根因）；
  · 弧类脚本把 `rotate()` 的**弧长写成 0** ⇒ `dθ = 0` ⇒ 顶点全退化 ⇒ **严格什么都不画**
    （`engine/player/th11` 那套一手数学；MEMORY 2026-10-13 定案），而 `rotate(π)` 只是半圆、`±2π` 才是整圈。
于是调试循环变成「猜 → 实跑 → 再看」✗，一轮一次实跑。本脚本把这两条变成**编译期红灯**。

## 规则与严重度

| # | 判据 | 严重度 |
| --- | --- | --- |
| R1 | 用到弧类 opcode 的脚本，必须**同时**出现 `iset($I0, …)` 与 `iset($I1, …)` | **error**（rc=1）|
| R2 | 弧类脚本里 `rotate()` 的第一个实参 |值| < 1e-6 ⇒ 0 弧 = 什么都不画 | **error**（rc=1）|
| R3 | 脚本没有 `layer(…)` | **报告**（不失败）—— 道具 / HUD / 由别的 VM 定层的脚本本来就合法，一刀切会假阳性一片 ✗ |
| R4 | `interruptLabel(n)` 用到 {1,2,3} 之外的值 | **报告** —— TH18 引擎对子机只发 **1/2/3**（`2` 出场 / `3` 进商店 / `1` 销毁）；其它号是"正文保留、不可达"的**故意**做法（见 108 的标签映射），只是值得一眼看见 |

## 用法

    python3 assets/check_anm_rules.py                 # 默认扫 assets/ability/scripts/*.anm.txt
    python3 assets/check_anm_rules.py --dir <目录>    # 扫别处（自测用）
    python3 assets/check_anm_rules.py --quiet         # 只打违规与摘要

R1/R2 命中 ⇒ **退出码 1**（供 `make check` 用）；R3/R4 只打印。
"""
import argparse
import re
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
DEFAULT_DIR = HERE / "ability" / "scripts"

ARC_OPS = ("texArcEven", "texArcOdd")          # anmm 601 / 600 一族
RE_BLOCK_COMMENT = re.compile(r"/\*.*?\*/", re.S)
RE_ROTATE = re.compile(r"rotate\(\s*([-+]?[0-9]*\.?[0-9]+(?:[eE][-+]?[0-9]+)?)")
RE_LAYER = re.compile(r"\blayer\(\s*([0-9]+)")
RE_ILABEL = re.compile(r"\binterruptLabel\(\s*([0-9]+)")
# ★ 豁免注记：脚本里出现这一串 ⇒ R2 不判错（用于「张角由卡侧每帧写」那类**故意**的 rotate(0)）。
ALLOW_ROT0 = "anm-lint: allow-rotate-zero"
# ★ 白名单：`文件名 ⇥ 理由`（# 注释）。用于**不该由 lint 判死**的既有案例 —— 只登记、不改那个脚本。
#   目前唯一一条 = `406_pofv109_aura_sakuya`（它的 `rotate(0.0f,…)` 是"静止初值"，
#   注释明写「张角与朝向由卡侧每帧写」⇒ 是**故意**的，不是漏抄 ✗✅）。
ALLOWLIST = HERE / "anm_rules_allow.txt"


def load_allowlist() -> dict:
    if not ALLOWLIST.is_file():
        return {}
    out = {}
    for ln in ALLOWLIST.read_text(encoding="utf-8").split("\n"):
        ln = ln.split("#")[0].strip()
        if not ln:
            continue
        parts = ln.split(None, 1)
        out[parts[0]] = parts[1] if len(parts) > 1 else ""
    return out


def strip_comments(txt: str) -> str:
    """★ 必须先剥注释：这些脚本里到处是**引一手原文**的 `rotate(0,0,[10010])` 之类，
    第一版没剥 ⇒ 5 支全假阳性 ✗（2026-10-15 自己踩到并修）。"""
    txt = RE_BLOCK_COMMENT.sub(" ", txt)
    out = []
    for ln in txt.split("\n"):
        i = ln.find("//")
        out.append(ln[:i] if i >= 0 else ln)
    return "\n".join(out)


def scan(path: Path) -> dict:
    raw = path.read_text(encoding="utf-8", errors="replace")
    txt = strip_comments(raw)
    has_arc = any(op in txt for op in ARC_OPS)
    return {
        "path": path,
        "has_arc": has_arc,
        "i0": "iset($I0" in txt,
        "i1": "iset($I1" in txt,
        "rots": [float(m) for m in RE_ROTATE.findall(txt)],
        "layers": [int(m) for m in RE_LAYER.findall(txt)],
        "ilabels": sorted({int(m) for m in RE_ILABEL.findall(txt)}),
        "allow_rot0": ALLOW_ROT0 in raw,
    }


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--dir", default=str(DEFAULT_DIR))
    ap.add_argument("--quiet", action="store_true")
    a = ap.parse_args()

    d = Path(a.dir)
    files = sorted(d.glob("*.anm.txt"))
    if not files:
        print("✗ %s 下没有 *.anm.txt" % d, file=sys.stderr)
        return 2

    errs, arcs, no_layer, odd_label, allowed = [], 0, [], [], []
    allow = load_allowlist()
    skipped = []
    for f in files:
        if f.name in allow:
            skipped.append(f.name)
            continue
        r = scan(f)
        if r["has_arc"]:
            arcs += 1
            if not (r["i0"] and r["i1"]):
                errs.append("%s：弧类 opcode 但缺 iset($I0/%s) —— dv=0 ⇒ 会被当成预乘 α，看不见 ✗"
                            % (f.name, "I1" if r["i0"] else "I0"))
            zero = [v for v in r["rots"] if abs(v) < 1e-6]
            if zero and not r["allow_rot0"]:
                errs.append("%s：弧类脚本里 rotate(%g) ⇒ dθ=0 ⇒ 顶点全退化、什么都不画 ✗"
                            "（若这是「张角由卡侧每帧写」那类故意做法，在脚本里写一行 "
                            "`// %s` 即可豁免）" % (f.name, zero[0], ALLOW_ROT0))
            elif zero:
                allowed.append(f.name)
        if not r["layers"]:
            no_layer.append(f.name)
        extra = [v for v in r["ilabels"] if v not in (1, 2, 3)]
        if extra:
            odd_label.append((f.name, extra))

    if not a.quiet:
        print("anm 规则检查：%d 支脚本（弧类 %d 支）" % (len(files), arcs))
        if no_layer:
            print("  · R3 报告：%d 支没有 layer(…)（道具/HUD/由别处定层的属正常，看一眼即可）：" % len(no_layer))
            print("      " + ", ".join(no_layer[:12]) + (" …" if len(no_layer) > 12 else ""))
        if odd_label:
            print("  · R4 报告：%d 支用了 {1,2,3} 之外的 interruptLabel（多半是「故意不可达」的段，见识一下）："
                  % len(odd_label))
            for n, v in odd_label[:8]:
                print("      %s → %s" % (n, v))
        if allowed:
            print("  · R2 豁免（脚本里写了 `%s`）：%s" % (ALLOW_ROT0, ", ".join(allowed)))
        if skipped:
            print("  · 白名单跳过 %d 支（见 assets/anm_rules_allow.txt，登记了理由）：%s"
                  % (len(skipped), ", ".join(skipped)))
    if errs:
        print("\n✗ %d 处硬违规（R1/R2）：" % len(errs))
        for e in errs:
            print("   " + e)
        return 1
    print("✓ R1/R2 全过（弧类脚本 I0/I1 齐、rotate 非 0）")
    return 0


if __name__ == "__main__":
    sys.exit(main())
