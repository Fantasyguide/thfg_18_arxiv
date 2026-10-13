#!/usr/bin/env python3
"""卡 109 **C3/C4 的敌方弹幕弹体**贴图 —— **一手原生切图**（TH19 `bullet1.png` ✓）。
    python3 make_pofv109_ball_art.py    # 出图 + 打印要抄进 entries/ORDER.txt 的三行

★★★★ 一手出处（切图，不是烤的 ✓；★ 由独立转写 agent `14bdeb0f` 逐条确证 ✓）
────────────────────────────────────────
★ 一手：TH19 v1.10c 的 **`bullet.anm`** entry0 **`bullet/bullet1.png`**（256×256 ✓）里，
★ 脚本 38 / 327 用 **三帧**画同一颗弹 ✓（`_ext/bullet.txt` 的 sprite 声明 ✓）：
| 帧 | sprite | 声明 | 用途 |
| --- | --- | --- | --- |
| 出生 | `sprite1` | **(8, 240, 8, 8)** | 生成瞬间（C4 槽0 还会先放大 5–6 倍 ✓）|
| 稳定 | `sprite0` | **(0, 240, 8, 8)** | 飞行中（脚本 329/330 = C3 只用这张 ✓）|
| 消失 | `sprite2` | **(16, 240, 8, 8)** | 命中/收掉时（**只有这一支带 `blendMode(1)` 加算** ✓）|
★ 弹体尺寸 = **8×8** ✓（★ **不是** 30×30 的阴阳玉 ✗ —— 那是别的脚本用的环 ✓）。

★★★★ 为什么**可以直接切 TH18 的图**（省掉一切重画 ✓）
────────────────────────────────────────
★ 转写已证：★ **TH18 v1.00a 的 `anm/bullet/bullet/bullet1.png` 与 TH19 v1.10c 的
`dat/bullet/bullet1.png` 逐像素完全相同** ✓（PIL `ImageChops.difference` 全图 bbox = None ✓；
PNG 字节不同、像素同 ✓）⇒ ★ **同坐标、同像素** ✓。
★ ⇒ ★ 本脚本**优先读 TH18 自己的那份**（★ 与 TH19 同像素 ✓，且不依赖 th19 的解包 ✓），
★ 读不到再回退 TH19 的那份 ✓。

★ 源（gitignored 的 `local/`，构建时现取 ✓）：
  `local/th18.v1.00a/anm/bullet/bullet/bullet1.png`   ← 首选（与 TH19 逐像素相同 ✓）
  `local/th19.v1.10c/dat/bullet/bullet1.png`           ← 回退
"""
import pathlib
from PIL import Image

ROOT = pathlib.Path(__file__).resolve().parent
_r = ROOT
for _ in range(8):
    if (_r / "local").is_dir():
        break
    _r = _r.parent
SRC_TH18 = _r / "local/th18.v1.00a/anm/bullet/bullet/bullet1.png"
SRC_TH19 = _r / "local/th19.v1.10c/dat/bullet/bullet1.png"
OUTDIR = ROOT / "bball"

# ★ 三帧的一手 rect（sprite1 / sprite0 / sprite2 ✓）
FRAMES = [
    ("POFV109_BBALL_BORN", "BBALL_BORN.png", (8, 240, 16, 248)),    # sprite1：出生
    ("POFV109_BBALL_IDLE", "BBALL_IDLE.png", (0, 240, 8, 248)),     # sprite0：稳定
    ("POFV109_BBALL_DIE",  "BBALL_DIE.png",  (16, 240, 24, 248)),   # sprite2：消失
]


def main() -> int:
    src = SRC_TH18 if SRC_TH18.is_file() else SRC_TH19
    print(f"# 源 = {src}")
    im = Image.open(src).convert("RGBA")
    OUTDIR.mkdir(parents=True, exist_ok=True)
    for name, fn, rect in FRAMES:
        cut = im.crop(rect)
        cut.save(OUTDIR / fn)
        print(f"{name:<24} ability/bball/{fn} {cut.width}x{cut.height}")
    print("# —— 以上三行追加到 ability/entries/ORDER.txt（只追加）——")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
