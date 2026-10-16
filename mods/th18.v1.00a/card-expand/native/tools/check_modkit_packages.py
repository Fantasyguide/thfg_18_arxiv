#!/usr/bin/env python3
"""check_modkit_packages.py —— thcrap 多补丁**同开**的护栏（owner 2026-10-15「同开」口径 ✓）。

## 为什么要有它（一手成因 ✓）

`card-expand` 与第三方 **leveledit** 补丁要在**同一份 TH18** 里同时启用 ✓。thcrap 的替换规则是
**按相对路径覆盖、后加载者赢** ✗ ⇒ 只要两个"同时启用"的包里出现**同一个相对路径**，
就会有一方被静默盖掉 ✗（[`assets/mystia/build_st02.py:6`](../assets/mystia/build_st02.py) 记的就是
`th18/st02enm.anm` 这一类：「一方坏掉，『只开一个也能用』就不成立」✓）。

⇒ 本闸就是那句话的机器化 ✓：**同开组合里不得有两包都提供的相对路径** ✓（除非在
`modkit_packages_allow.txt` 里**显式登记为"有意的依赖覆盖"** ✓）。

## 判据（两条，硬软分开 ✓）

| # | 判据 | 严重度 |
| --- | --- | --- |
| **C1** | 同开组合里，两个包提供**同一相对路径** | **error**（rc=1），除非白名单登记 |
| **C2** | 某包目录里的文件**没进它自己的 `files.js`** | **报告**（不失败）—— `mkfiles.py` 会**有意摘掉** gitignored 资产（`ability.anm` / `pl0X.sht` / 语音 wav 等，见其 `:19-21`/`:35-37`）⇒ 一刀切会假阳性 ✓；但"漏了一整个新目录"是真故障 ✓，所以仍打印 |

## 用法

    python3 tools/check_modkit_packages.py --modkit <modkit>                 # 读 enabled.json 的组合
    python3 tools/check_modkit_packages.py --modkit <modkit> --packages A B  # 指定组合
    python3 tools/check_modkit_packages.py --modkit <modkit> --all-pairs     # 两两组合体检（找隐患）

`patch.js` / `files.js` 是**每个包各自必有的元数据** ✓ ⇒ 不参与 C1 ✓（不算冲突 ✓）。
"""
import argparse
import json
import os
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
ALLOW = HERE / "modkit_packages_allow.txt"
REPO_REL = os.path.join("thcrap", "repos")
SKIP_NAMES = {"patch.js", "files.js"}


def load_allow() -> dict:
    """`包A+包B ⇥ 相对路径 ⇥ 理由`（# 注释；包名按字典序拼 ✓）。"""
    out = {}
    if not ALLOW.is_file():
        return out
    for ln in ALLOW.read_text(encoding="utf-8").split("\n"):
        ln = ln.split("#")[0].strip()
        if not ln:
            continue
        parts = [x.strip() for x in ln.split("\t")]
        if len(parts) >= 2:
            pair = "+".join(sorted(parts[0].split("+")))
            out[(pair, parts[1])] = parts[2] if len(parts) > 2 else ""
    return out


def find_pkg(modkit: Path, name: str):
    """在 <modkit>/thcrap/repos/*/<name> 里找包目录（repo 名不写死 ✓）。"""
    root = modkit / REPO_REL
    if not root.is_dir():
        return None
    for repo in sorted(root.iterdir()):
        cand = repo / name
        if cand.is_dir():
            return cand
    return None


def pkg_files(pkg: Path) -> set:
    out = set()
    for p in pkg.rglob("*"):
        if p.is_file() and p.name not in SKIP_NAMES:
            out.add(str(p.relative_to(pkg)).replace(os.sep, "/"))
    return out


def files_js_entries(pkg: Path) -> set:
    f = pkg / "files.js"
    if not f.is_file():
        return set()
    txt = f.read_text(encoding="utf-8", errors="replace")
    out = set()
    for ln in txt.split("\n"):
        ln = ln.strip().rstrip(",")
        if ln.startswith('"') and '":' in ln:
            out.add(ln.split('"')[1])
    return out


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--modkit", required=True)
    ap.add_argument("--packages", nargs="*", default=None,
                    help="要同开的包名（默认读 <modkit>/mods/enabled.json 的 patches）")
    ap.add_argument("--all-pairs", action="store_true", help="对全部包两两组合体检")
    a = ap.parse_args()

    modkit = Path(a.modkit)
    if not modkit.is_dir():
        print("✗ modkit 目录不存在：%s" % modkit, file=sys.stderr)
        return 2

    all_pkgs = []
    root = modkit / REPO_REL
    if root.is_dir():
        for repo in sorted(root.iterdir()):
            for p in sorted(repo.iterdir()):
                if (p / "patch.js").is_file():
                    all_pkgs.append(p.name)

    if a.packages:
        combo = list(a.packages)
    elif a.all_pairs:
        combo = None
    else:
        ej = modkit / "mods" / "enabled.json"
        if not ej.is_file():
            print("✗ 没有 %s ⇒ 请用 --packages 指定同开组合" % ej, file=sys.stderr)
            return 2
        combo = json.loads(ej.read_text(encoding="utf-8")).get("patches", [])
        print("同开组合（读 enabled.json ✓）：%s" % ", ".join(combo))

    combos = []
    if combo is not None:
        combos.append(sorted(combo))
    else:
        for i in range(len(all_pkgs)):
            for j in range(i + 1, len(all_pkgs)):
                combos.append(sorted([all_pkgs[i], all_pkgs[j]]))
        print("体检 %d 个两两组合（现有包：%s）" % (len(combos), ", ".join(all_pkgs)))

    allow = load_allow()
    bad, gaps = 0, 0
    for c in combos:
        files, entries = {}, {}
        missing = []
        for name in c:
            pkg = find_pkg(modkit, name)
            if pkg is None:
                missing.append(name)
                continue
            files[name] = pkg_files(pkg)
            entries[name] = files_js_entries(pkg)
        if missing:
            print("✗ 组合 %s：找不到包 %s" % ("+".join(c), ", ".join(missing)), file=sys.stderr)
            return 2
        for i in range(len(c)):
            for j in range(i + 1, len(c)):
                A, B = c[i], c[j]
                inter = sorted(files[A] & files[B])
                for rel in inter:
                    key = ("+".join(sorted([A, B])), rel)
                    if key in allow:
                        print("  · 白名单放行 %s × %s：%s（%s）" % (A, B, rel, allow[key]))
                    else:
                        print("✗ 同路径冲突：%s 与 %s 都提供 %s ✗"
                              "（thcrap 后加载者赢 ⇒ 一方被静默盖掉；"
                              "若是有意的依赖覆盖，登记到 %s）" % (A, B, rel, ALLOW.name))
                        bad += 1
        for name in c:
            miss = sorted(files[name] - entries[name])
            if miss:
                gaps += len(miss)
                print("  · 报告：%s 有 %d 个文件没进它自己的 files.js（gitignored 资产属正常 ✓）：%s%s"
                      % (name, len(miss), ", ".join(miss[:8]), " …" if len(miss) > 8 else ""))
    if bad:
        print("\n✗ %d 处同路径冲突 ⇒ 同开会有一方被盖掉 ✗" % bad)
        return 1
    print("✓ 同开组合无同路径冲突（C1 全过）")
    return 0


if __name__ == "__main__":
    sys.exit(main())
