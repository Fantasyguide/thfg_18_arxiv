#!/usr/bin/env python3
"""卡 109 **消弹范围圈**的环带贴图 —— **一手原生切图**（TH09 `etama3.png` 第 3 列 ✓）。
    python3 make_pofv109_ring_strip_art.py    # 出图 + 打印要抄进 entries/ORDER.txt 的那一行
★★★ 一手出处（切图，不是烤的 ✓）
────────────────────────────────────────
一手：TH09 v1.50a **`etama.anm`** 的 entry **`data/etama/etama3.png`**（**128×128** 的图 ✓）里
声明的 **`sprite224: { x: 32, y: 0, w: 16, h: 768 }`** —— ⚠ **h=768 > 贴图高 128** ⇒
★ 那是**竖向回绕/平铺**（TH09 的长条带就是这么拉的 ✓，与 TH18 的 `scrollY` 同类 ✓）
⇒ ★ **要切的源 = 那 16 px 宽的一整列**：**`(32, 0, 16, 128)`** ✓。
★ 颜色 = **贴图自带**（第 3 列是红 ✓）⇒ ★ **不用染色** ✓（一手 `script86` 也没写色 ✓）。
★ 源（gitignored 的 `local/`，构建时现取 ✓）：
  `local/th09.v1.50a/anm/etama/data/etama/etama3.png`
  取法：`thdat -x 9 th09.dat etama.anm` → `thanm -x 9 etama.anm data/etama/etama3.png`
  （⚠ `thanm -x` 会**误报 not found**，但文件其实已导出 ✓ —— 与 §6.2 记的坑同一条 ✓）。
"""
import pathlib
from PIL import Image

ROOT = pathlib.Path(__file__).resolve().parent
_r = ROOT
for _ in range(8):
    if (_r / "local").is_dir():
        break
    _r = _r.parent
SRC = _r / "local/th09.v1.50a/anm/etama/data/etama/etama3.png"
OUT = ROOT / "cring" / "C_RING_STRIP.png"
RECT = (32, 0, 48, 128)          # ★ 一手 sprite224 的那一列（h 取贴图实际高 128 ✓ 由 scrollY 回绕 ✓）


def main() -> int:
    im = Image.open(SRC).convert("RGBA")
    cut = im.crop(RECT)
    OUT.parent.mkdir(parents=True, exist_ok=True)
    cut.save(OUT)
    print(f"{'POFV109_C_RING_STRIP':<24} ability/cring/C_RING_STRIP.png {cut.width}x{cut.height}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
