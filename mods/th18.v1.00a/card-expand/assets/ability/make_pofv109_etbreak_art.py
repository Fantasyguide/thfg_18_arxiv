#!/usr/bin/env python3
"""卡 109 **消弹火花**（`etbreak`）—— **一手原生切图**（不是烤的 ✓）。
    python3 make_pofv109_etbreak_art.py     # 出 8 张图 + 打印要抄进 entries/ORDER.txt 的 8 行
★★★ 一手出处（本脚本是切图，不是自绘 ✓）
────────────────────────────────────────
一手：TH19 v1.10c **`bullet.anm`** 的 entry **`effect/etbreak.png`**（256×128，format 5）——
它的 8 个 sprite 矩形逐条是（`local/th19.v1.10c/anm/bullet/bullet.anm.txt:4866`）：
  sprite540 (1,1,62,62)  sprite541 (65,1,62,62)  sprite542 (129,1,62,62)  sprite543 (193,1,62,62)
  sprite544 (1,65,62,62) sprite545 (65,65,62,62) sprite546 (129,65,62,62) sprite547 (193,65,62,62)
⇒ ★ 本脚本就是**按这 8 个矩形 1:1 切**（不改色、不缩放 ✓）。
★ **谁在播它**：`bullet.anm` 的 **`script162`**（`layer(20)` / `color(192,192,192)` / **无 `blendMode`** /
  `alpha(64)` / `scale 0.3→0.42`（16 帧）/ 每 **2 帧**换一帧 ⇒ 共 16 帧后 `delete()` ✓）
  与 **`script163`**（`layer(21)` / **`blendMode(1)` 加算** / `alpha(224)` / `scale 0.3→0.45` ✓）——
  ★ 两者**播的是同一张 8 帧图** ✓，★ 我们取 **163 那一套（更亮、看得见 ✓）**，见脚本 431 的注释 ✓。
★ 源在 gitignored 的 `local/`，构建时现取 ✓（版权字节不入库 ✓）。
"""
import pathlib
from PIL import Image

ROOT = pathlib.Path(__file__).resolve().parent          # = assets/ability/
_r = ROOT
for _ in range(8):
    if (_r / "local").is_dir():
        break
    _r = _r.parent
SRC = _r / "local/th19.v1.10c/dat/effect/etbreak.png"
OUT = ROOT / "etbreak"
RECTS = [(1, 1), (65, 1), (129, 1), (193, 1), (1, 65), (65, 65), (129, 65), (193, 65)]
W = H = 62


def main() -> int:
    im = Image.open(SRC).convert("RGBA")
    OUT.mkdir(parents=True, exist_ok=True)
    for k, (x, y) in enumerate(RECTS):
        cut = im.crop((x, y, x + W, y + H))
        cut.save(OUT / f"ETBREAK_{k}.png")
        print(f"{'POFV109_ETBREAK_' + str(k):<24} ability/etbreak/ETBREAK_{k}.png {W}x{H}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
