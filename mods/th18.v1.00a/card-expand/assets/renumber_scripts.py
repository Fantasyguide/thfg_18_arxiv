#!/usr/bin/env python3
"""把 `ability/scripts/` 的新增脚本编号压紧，补掉并发开发留下的号段空洞。

## 为什么需要它

`build_ability.py` 的 `plan()` 是**硬约束**（原话）：

    want = n_script + k
    if int(m.group(1)) != want:
        raise SystemExit(f"{p.name}：编号应为 {want}（零售 {n_script} 个脚本，按文件名顺序连续追加）")

即脚本号的**数值**必须等于 `68 + 序号`，中间**不许有洞**。多个人（或多个子代理）同时往
`scripts/` 里加东西时，为了不互相抢号，各自会拿一个靠后的号段（例如一人 189+、一人 199+），
落地后中间就空了 —— 这时 `make check` / `build_ability.py` 会直接 raise。

## 它做什么

1. **只动「新增区」**：先把 `68..BASE_MAX`（默认 188，= 收口前最后一个既有号）核对成**严格连续**，
   对不上就拒绝运行（这样绝不会破坏既有脚本号，也就不会让既有文档里的编号引用失效）；
2. 其余文件按**当前编号升序**重排成 `BASE_MAX+1, BASE_MAX+2, …`（保持相对顺序）；
3. `--apply` 时：重命名文件，并把文件里第一行的 `script script<旧号> {` 改成 `script script<新号> {`。

> ⚠️ 文件内的 `script scriptNN {` 里的 `NN` 就是 thtk 编译进归档的脚本 id，
> 必须与文件名号一致 —— 两个都要改，只改文件名会静默指错。
> 文件之间的互相引用走 `scriptNew(@SCRIPT_<名字>@)`（**按名字**，`build_ability.py` 现算号），
> 所以改名号**不会**影响互引；但正文里如果出现**字面量** `scriptNew(199)`，本工具会点名报出来，请自己改。

## 用法

    python3 assets/renumber_scripts.py            # 只打印映射（dry-run，默认）
    python3 assets/renumber_scripts.py --apply    # 真改

改完请跑：`python3 assets/build_ability.py`、`make check`。
"""
from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
SCRIPTS = HERE / "ability" / "scripts"

BASE_MIN = 68      # 零售 ability.anm 的脚本数，我们的追加区从它开始
BASE_MAX = 188     # ★ 收口前的既有上界：本工具保证 68..188 **原样不动**
_FILE = re.compile(r"^(\d+)_(.+)\.anm\.txt$")
_INNER = re.compile(r"^(\s*script\s+script)(\d+)(\s*\{)", re.M)


def scan() -> list[tuple[int, str, Path]]:
    """→ [(num, name, path)]，按数字升序；文件名不合规直接报错。"""
    out = []
    for p in SCRIPTS.glob("*.anm.txt"):
        m = _FILE.match(p.name)
        if not m:
            raise SystemExit(f"{p.name}：文件名要像 68_reverse_flash.anm.txt")
        out.append((int(m.group(1)), m.group(2), p))
    out.sort(key=lambda t: (t[0], t[1]))
    if len({t[0] for t in out}) != len(out):
        dup = [n for n in {t[0] for t in out} if sum(1 for x in out if x[0] == n) > 1]
        raise SystemExit(f"编号重复：{sorted(dup)}")
    return out


def base_ok(items: list[tuple[int, str, Path]]) -> None:
    got = [n for n, _, _ in items if n <= BASE_MAX]
    want = list(range(BASE_MIN, BASE_MAX + 1))
    if got != want:
        missing = sorted(set(want) - set(got))
        extra = sorted(set(got) - set(want))
        raise SystemExit(
            f"既有区 {BASE_MIN}..{BASE_MAX} 不是严格连续，拒绝运行。\n"
            f"  缺：{missing}\n  多：{extra}\n"
            "（先把它修好，或按实际情况改 BASE_MAX 后重跑）"
        )


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--apply", action="store_true", help="真的改文件名与文件内的 script 号")
    a = ap.parse_args()

    items = scan()
    base_ok(items)
    tail = [(n, name, p) for n, name, p in items if n > BASE_MAX]
    if not tail:
        print(f"新增区为空（既有区 {BASE_MIN}..{BASE_MAX} 已连续），无需压缩。")
        return

    moves = [(old, BASE_MAX + 1 + k, name, p) for k, (old, name, p) in enumerate(tail)]

    print(f"既有区 {BASE_MIN}..{BASE_MAX} 连续 ✅；新增区 {len(moves)} 个文件将重排：\n")
    width = max(len(p.name) for _, _, _, p in moves)
    for old, new, name, p in moves:
        flag = "  " if old == new else "→ "
        print(f"  {p.name:<{width}}  {flag}script{new}")
    if all(old == new for old, new, _, _ in moves):
        print("\n号段已经连续，无需改动。")
        return

    # 正文里可能存在的**字面量** scriptNew(数字)：按名字互引不受影响，但字面量会指错。
    lits = []
    for _, _, _, p in moves:
        for m in re.finditer(r"scriptNew\(\s*(\d+)\s*\)", p.read_text(encoding="utf-8")):
            lits.append((p.name, int(m.group(1))))
    if lits:
        print("\n⚠️ 正文里发现**字面量** scriptNew(N)（改名号后会指错，请改成 @SCRIPT_<名字>@）：")
        for name, num in lits:
            print(f"    {name}: scriptNew({num})")

    if not a.apply:
        print("\n（dry-run；加 --apply 才真改）")
        return

    for old, new, name, p in sorted(moves, key=lambda t: t[1], reverse=True):
        text = p.read_text(encoding="utf-8")
        n_inner = len(_INNER.findall(text))
        if n_inner != 1:
            raise SystemExit(f"{p.name}：文件内 `script scriptNN {{` 出现 {n_inner} 次，应为 1 次（拒绝改）")
        inner_old = int(_INNER.search(text).group(2))
        if inner_old != old:
            raise SystemExit(f"{p.name}：文件内 script 号 {inner_old} 与文件名号 {old} 不一致（拒绝改）")
        new_path = SCRIPTS / f"{new}_{name}.anm.txt"
        if new_path.exists():
            raise SystemExit(f"{new_path.name} 已存在（拒绝覆盖）")
        p.write_text(_INNER.sub(lambda m: f"{m.group(1)}{new}{m.group(3)}", text, count=1), encoding="utf-8")
        p.rename(new_path)
        print(f"  {old}_{name} → {new}_{name}")

    print(f"\n压缩完成：新增区现在是 {BASE_MAX + 1}..{BASE_MAX + len(moves)}。")
    print("⚠️ 其它文档/生成器注释里若写死了旧编号，请一并改（跑 `grep -rn` 查一下）。")
    print("⚠️ 接着跑：`python3 assets/build_ability.py`（会重建 ability.anm 与 anm_ids.h）、`make check`。")


if __name__ == "__main__":
    sys.exit(main())
