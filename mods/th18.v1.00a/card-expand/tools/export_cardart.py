#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""export_cardart.py —— 把**非原生卡的卡面美术**导出成一个能直接看的目录（默认导到 Windows 侧）。

    python3 tools/export_cardart.py                      # 导到 modkit 下的 cardart/
    python3 tools/export_cardart.py --out /mnt/d/.../cardart --open-hint

## 为什么要它

卡面美术分两处，肉眼对不上容易误判：
1. **我们自己画的** —— `assets/cards/<NAME>_max.png`（256×320）+ `<NAME>_min.png`（64×80），
   由 `assets/cards/ORDER.txt` 的**行号**决定 sprite 号（第 k 行 → `118+2k` / `119+2k`）；
2. **复用原生图的** —— 没有自己的 PNG，`cards.js` 里直接填原生 `abcard.anm` 的 sprite 号
   （原生 entry 数 = **118**，所以 `< 118` 的都是原生图）。

本脚本把 ① 按「id + 中文名」改名拷出来、把 ② 从**原生 `abcard.anm` 现抽**出来，
再生成一份 `index.html`（表格：id / 卡名 / 内部名 / sprite / 大图 / 小图）⇒ 浏览器直接翻。
**只读 + 拷贝**：不动 `assets/`、不动 `build/`、不动任何 patch。

## 一手（口径都写死，别猜）

- sprite 规则：`assets/build_abcard.py` 头部（`base = 原生 entry 数 = 118`，`sprite 号 = entry 号`）。
- 原生 entry 数 118：`thanm -l 18 local/th18.v1.00a/dat/abcard.anm` 数 `^entry`。
- 每个 id 的 `sprite_large/sprite_small`：`patch/th18/cards.js`（生成物，由卡表决定）。
"""
import argparse
import html
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
MOD = HERE.parent                                   # mods/th18.v1.00a/card-expand
REPO = MOD.parents[2]                               # renkolab
THANM = REPO / "local" / "vendor" / "thtk" / "build" / "thanm" / "thanm"
ABcard = REPO / "local" / "th18.v1.00a" / "dat" / "abcard.anm"
CARDS_JS = MOD / "patch" / "th18" / "cards.js"
ART_DIR = MOD / "assets" / "cards"
ORDER = ART_DIR / "ORDER.txt"
DEFAULT_OUT = REPO / "local" / "vendor" / "th18_modkit" / "cardart"
NATIVE_BASE = 118                                   # 原生 abcard.anm 的 entry 数（= sprite 号上限）


def load_order():
    names = []
    for line in ORDER.read_text(encoding="utf-8").splitlines():
        s = line.strip()
        if s and not s.startswith("#"):
            names.append(s)
    return names


def extract_native(out_dir: Path):
    """把原生 abcard.anm 抽成 PNG，返回 {sprite 号: 文件名}。"""
    if not THANM.is_file():
        sys.exit(f"缺 thanm：{THANM}（见 docs/SETUP.md）")
    if not ABcard.is_file():
        sys.exit(f"缺样本：{ABcard}（gitignored）")
    out_dir.mkdir(parents=True, exist_ok=True)
    subprocess.run([str(THANM), "-x", "18", str(ABcard)], cwd=out_dir,
                   capture_output=True, check=True)
    # 抽出来的文件名就是 entry 的 name（如 ability/empty_max.png）；用 -l 把序号对上
    listing = subprocess.run([str(THANM), "-l", "18", str(ABcard)],
                             capture_output=True, text=True, check=True).stdout
    by_index = {}
    cur = None
    for line in listing.splitlines():
        m = re.match(r"^entry (entry\d+) \{", line.strip())
        if m:
            cur = int(m.group(1)[5:]); continue
        m = re.match(r'^\s+name: "(.*)",', line)
        if m and cur is not None:
            by_index[cur] = m.group(1)
    return by_index, out_dir


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--out", default=str(DEFAULT_OUT), help="导出目录（默认 modkit 下的 cardart/）")
    ap.add_argument("--no-clean", action="store_true", help="不先清空导出目录")
    a = ap.parse_args()

    out = Path(a.out)
    if out.exists() and not a.no_clean:
        shutil.rmtree(out)
    (out / "own").mkdir(parents=True, exist_ok=True)
    (out / "native").mkdir(parents=True, exist_ok=True)

    cards = json.loads(CARDS_JS.read_text(encoding="utf-8"))
    order = load_order()
    native_map, _ = extract_native(out / "native")

    rows = []
    for cid in sorted(cards, key=lambda k: int(k)):
        c = cards[cid]
        nm = c.get("internal_name", "?")
        sl, ss = c.get("sprite_large"), c.get("sprite_small")
        big = small = None
        if nm in order:
            k = order.index(nm)
            assert (sl, ss) == (NATIVE_BASE + 2 * k, NATIVE_BASE + 2 * k + 1), \
                f"卡 {cid} 的 sprite 与 ORDER 行号对不上：{sl}/{ss} vs 行 {k}"
            src_max, src_min = ART_DIR / f"{nm}_max.png", ART_DIR / f"{nm}_min.png"
            big = out / "own" / f"{int(cid):03d}_{nm}_max.png"
            small = out / "own" / f"{int(cid):03d}_{nm}_min.png"
            shutil.copyfile(src_max, big)
            shutil.copyfile(src_min, small)
            rows.append((int(cid), c.get("name", ""), nm, sl, ss, f"own/{big.name}", f"own/{small.name}", "自绘"))
        else:
            big_src = out / "native" / native_map.get(sl, "")
            small_src = out / "native" / native_map.get(ss, "")
            if big_src.is_file():
                big = out / "native" / f"{sl}_{big_src.name.replace('/', '_')}"
                shutil.copyfile(big_src, big)
            if small_src.is_file():
                small = out / "native" / f"{ss}_{small_src.name.replace('/', '_')}"
                shutil.copyfile(small_src, small)
            rows.append((int(cid), c.get("name", ""), nm, sl, ss,
                         f"native/{big.name}" if big else "", f"native/{small.name}" if small else "",
                         "复用原生图" if sl is not None and int(sl) < NATIVE_BASE else "?"))

    # index.html —— 浏览器直接翻（Windows 双击即可）
    h = ['<!doctype html><meta charset="utf-8"><title>非原生卡卡面</title>',
         '<style>body{background:#1b1b1f;color:#eee;font:14px/1.5 sans-serif}',
         'table{border-collapse:collapse}td,th{border:1px solid #444;padding:6px 8px;vertical-align:middle}',
         'img{image-rendering:pixelated;background:#333}th{position:sticky;top:0;background:#26262c}</style>',
         f'<h2>非原生卡卡面（共 {len(rows)} 张）</h2>',
         '<p>大图按 1:1 显示（256×320）、小图 64×80。最后一列标「复用原生图」的表示这张卡没画自己的图、'
         '直接用原生 <code>abcard.anm</code> 的 sprite。</p>',
         '<table><tr><th>id</th><th>卡名</th><th>内部名</th><th>sprite 大/小</th><th>大图</th><th>小图</th><th>来源</th></tr>']
    for cid, name, nm, sl, ss, big, small, src in rows:
        big_cell = ('<img src="%s" height="160">' % big) if big else '—'
        small_cell = ('<img src="%s">' % small) if small else '—'
        h.append('<tr><td>{}</td><td>{}</td><td>{}</td><td>{}/{}</td><td>{}</td><td>{}</td><td>{}</td></tr>'
                 .format(cid, html.escape(name), html.escape(nm), sl, ss, big_cell, small_cell, src))
    h.append('</table>')
    (out / "index.html").write_text("\n".join(h), encoding="utf-8")

    own = sum(1 for r in rows if r[7] == "自绘")
    nat = sum(1 for r in rows if r[7] == "复用原生图")
    print(f"导出 {len(rows)} 张卡 → {out}")
    print(f"  · 自绘卡面 {own} 张（assets/cards/<NAME>_max.png / _min.png）")
    print(f"  · 复用原生图 {nat} 张（sprite < {NATIVE_BASE}）")
    print(f"  · 打开 {out / 'index.html'} 可逐张看")


if __name__ == "__main__":
    main()
