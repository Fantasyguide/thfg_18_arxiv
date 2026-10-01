#!/usr/bin/env python3
"""用 ExpHP `th-re-data` 的 `labels.json` 生成 **eclmap**（`thecl -m` 要的那个格式）。

    python3 tooling/eclmap/gen_eclmap.py th09.v1.50a           # 写到 local/vendor/eclmap/eclmap/th09.eclm
    python3 tooling/eclmap/gen_eclmap.py th09.v1.50a --kind anm # → <thNN>.anmm（anm 名表）
    python3 tooling/eclmap/gen_eclmap.py th09.v1.50a --out /tmp/x.eclm
    python3 tooling/eclmap/gen_eclmap.py --list                # 列出哪些版本能生成

## 为什么需要它

`thecl -d9 file.ecl`（反编译）不带 `-m` 时只能给 `ins_54(...)` 这种**数字 opcode**；
带 `-m <eclmap>` 才会写成 `anm_set_main(...)`。仓库自带的那批 eclmap
（`local/vendor/eclmap/eclmap/`）**只有 th08 / th11–th18** —— 想读 **th09** 的 `plNN.ecl`
（= 被送到对面的那个 boss 的符卡本体，见 `engine/card/th09/02-per-character-ecl.md`）就没有名字表。

而 ExpHP 的 `local/vendor/th-re-data/data/<版本>/labels.json` **已经有**每个 opcode 的名字
（键 `ecl` → `[[地址, "54__anm_set_main"], …]`）⇒ 直接转成 eclmap 即可。
（`anm` / `msg` 两个键同理，但那是给 `thanm` / `thmsg` 的，本脚本只做 `ecl`。）

## 一手出处

`local/vendor/th-re-data/`（ExpHP，gitignored）＝本仓 `CLAUDE.md` 说的「命名金矿」；
`thecl` 的 eclmap 语法见 `local/vendor/thtk/`（`thtk` 自带 `!eclmap` / `!ins_names` 两段，
其余行是 `<opcode> <名字>`）。
"""
import argparse
import json
import re
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
REDATA = REPO / "local" / "vendor" / "th-re-data" / "data"
ECLMAP_DIR = REPO / "local" / "vendor" / "eclmap" / "eclmap"


def versions_with_ecl():
    """→ [(版本目录名, opcode 条数)]，只有 labels.json 里真有 `ecl` 键的才算。"""
    out = []
    if not REDATA.is_dir():
        return out
    for d in sorted(REDATA.iterdir()):
        f = d / "labels.json"
        if not f.is_file():
            continue
        try:
            data = json.loads(f.read_text(encoding="utf-8"))
        except Exception:
            continue
        ecl = data.get("ecl")
        if isinstance(ecl, list) and ecl:
            out.append((d.name, len(ecl)))
    return out


def build(version: str, kind: str = "ecl") -> str:
    f = REDATA / version / "labels.json"
    if not f.is_file():
        raise SystemExit(f"找不到 {f}（ExpHP 数据在本机 `local/vendor/th-re-data/`，gitignored）")
    data = json.loads(f.read_text(encoding="utf-8"))
    ecl = data.get(kind)
    if not isinstance(ecl, list) or not ecl:
        raise SystemExit(f"{version} 的 labels.json 里没有 `{kind}` 键 ⇒ 这份数据没带这类名表")
    # ★ 2026-10-01：`kind` 两用 —— `ecl`（指令名，`thecl -m`）与 `anm`（anm 指令名，`thanm -m`）。
    #   两种文件的语法同形（`!<x>map` + `!ins_names` + `NN name`），只是头一行不同
    #   （anmmap 见 `local/vendor/thpages/static/mapfile/v8.anmm`：`!anmmap` / 空行 / 注释 / `!ins_names`）。

    # 条目形如 [地址, "54__anm_set_main"]；名字里 `__` 前是 opcode（可能比 0x 更省事）。
    # ⚠ 有少数条目**只有数字没有名字**（如 "185"）⇒ 名字就用 `ins_<n>` 兜底，保持可读。
    rows = {}
    for addr, name in ecl:
        m = re.match(r"^(\d+)__(.*)$", str(name))
        if m:
            num, nm = int(m.group(1)), m.group(2)
        else:
            m2 = re.match(r"^(\d+)$", str(name))
            if not m2:
                continue
            num, nm = int(m2.group(1)), f"op_{m2.group(1)}"
        # 同一个 opcode 可能出现多次（多个 handler；有的条目只有裸数字、没有 `__名字`）
        # ⇒ **优先取带名字的那条**（裸数字那条只当兜底，别把好名字挤掉）。
        if num not in rows or (rows[num].startswith("op_") and not nm.startswith("op_")):
            rows[num] = nm

    if kind == "anm":
        lines = ["!anmmap", "", f"#  generated from ExpHP labels.json ({version}, kind=anm)", "",
                 "!ins_names"]
    else:
        lines = ["!eclmap", "!ins_names"]
    for num in sorted(rows):
        lines.append(f"{num} {rows[num]}")
    lines.append("")
    return "\n".join(lines)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("version", nargs="?", help="例如 th09.v1.50a")
    ap.add_argument("--out", default=None, help="输出路径（默认 local/vendor/eclmap/eclmap/<thNN>.eclm）")
    ap.add_argument("--kind", choices=("ecl", "anm"), default="ecl",
                    help="ecl = 指令名表（thecl -m，默认）；anm = anm 指令名表（thanm -m）")
    ap.add_argument("--list", action="store_true", help="列出能生成 eclmap 的版本")
    a = ap.parse_args()

    if a.list or not a.version:
        vs = versions_with_ecl()
        if not vs:
            print("没找到任何带 `ecl` 键的 labels.json（先备好 local/vendor/th-re-data/）")
            return 0
        print("能生成名表的版本（版本目录, ecl 条数 / anm 条数）：")
        for name, n in vs:
            anm = 0
            try:
                _d = json.loads((REDATA / name / "labels.json").read_text(encoding="utf-8"))
                anm = len(_d.get("anm") or [])
            except Exception:
                pass
            have = (ECLMAP_DIR / (name.split(".")[0] + ".eclm")).is_file()
            print(f"  {name:20s} ecl {n:4d} / anm {anm:4d}   仓库已有 eclmap: {'是' if have else '否'}")
        return 0

    text = build(a.version, a.kind)
    ext = ".eclm" if a.kind == "ecl" else ".anmm"
    out = Path(a.out) if a.out else ECLMAP_DIR / (a.version.split(".")[0] + ext)
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(text, encoding="utf-8")
    n = len([l for l in text.splitlines() if l and not l.startswith("!")])
    print(f"写入 {out}（{n} 个 opcode）")
    return 0


if __name__ == "__main__":
    sys.exit(main())
