#!/usr/bin/env python3
"""卡 109 **C3/C4 的阴阳玉弹体**贴图 —— **一手原生切图**（TH19 `bullet6.png` ✓）。
    python3 make_pofv109_ball_art.py    # 出图 + 打印要抄进 entries/ORDER.txt 的三行

★★★★ 一手出处（切图，不是烤的 ✓）
────────────────────────────────────────
★ 一手：TH19 v1.10c 的 `bullet.anm` **entry9 = `bullet/bullet6.png`** ✓（★ 30×30 的**阴阳玉** ✓）。
★ 它的脚本 327（C4）/ 329·330（C3）正文（`_ext/bullet.txt` ✓）：
| 脚本 | 出生 | 稳定 | 消失 |
| --- | --- | --- | --- |
| `script327`（C4）| `sprite(sprite1)` | `sprite(sprite0)` | （消失支另有）|
| `script329/330`（C3）| — | `sprite(sprite0)` | 同上 |
★ ★★ **编号口径更正（owner 2026-10-13 指出，我先前核错了 ✗）**：
★ 这里 `sprite(N)` 是 ★ **该 entry 内的局部序号** ✓（★ 不是全文件全局号 ✗）——
★ 证据：★ 本仓那条"全局号"的旁证（`engine/versus/th19/02-self-field-systems.md` §4.28）
★ 引的是 **entry0 的脚本**（★ entry0 从 sprite0 起 ⇒ **local == global，区分不了** ✗）。
★ ⇒ ★ entry9 里的 `sprite0/1/2` = ★ **`sprite580/581/582`** ✓（★ 即本文件的 3 个 rect ✓）。
★ 源：`local/th19.v1.10c/dat/bullet/bullet6.png` ✓（★ TH18 也有一份同名文件 ✓，但**先按 TH19 切** ✓）。
"""
import pathlib
from PIL import Image

ROOT = pathlib.Path(__file__).resolve().parent
_r = ROOT
for _ in range(8):
    if (_r / "local").is_dir():
        break
    _r = _r.parent
SRC = _r / "local/th19.v1.10c/dat/bullet/bullet6.png"
OUTDIR = ROOT / "bball"

# ★ entry9 的三张（sprite580/581/582 = 局部 0/1/2 ✓；声明都是 30×30 ✓）
FRAMES = [
    ("POFV109_BBALL_BORN", "BBALL_BORN.png", (33, 1, 63, 31)),    # sprite581（局部 1）：出生
    ("POFV109_BBALL_IDLE", "BBALL_IDLE.png", (1, 1, 31, 31)),     # sprite580（局部 0）：稳定
    ("POFV109_BBALL_DIE",  "BBALL_DIE.png",  (65, 1, 95, 31)),    # sprite582（局部 2）：消失
]


def main() -> int:
    im = Image.open(SRC).convert("RGBA")
    OUTDIR.mkdir(parents=True, exist_ok=True)
    for name, fn, rect in FRAMES:
        cut = im.crop(rect)
        cut.save(OUTDIR / fn)
        print(f"{name:<24} ability/bball/{fn} {cut.width}x{cut.height}")
    print("# —— 以上三行追加到 ability/entries/ORDER.txt（只追加）——")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
