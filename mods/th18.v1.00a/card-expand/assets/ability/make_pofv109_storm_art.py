#!/usr/bin/env python3
"""109 的**暴风**贴图 —— **从 TH09 自己的 `data/etama/etama2.png` 1:1 切**。

    python3 make_pofv109_storm_art.py

产物：`ability/pofv109_storm/STORM.png`（**62×62**）

## 为什么是这一格（owner 指认 + 我核实）

owner：「我看见的 **th09 暴风**是贴图 `[th09]\\data\\ANM\\data\\etama\\etama2.png`
       **倒数第二行最右侧**」＋「如果 th18 里面没有对应函数，那就**换成 th09 的暴风样式**」。

一手（`th09.dat` 的 `etama.anm`，entry1 = `data/etama/etama2.png`）：
该 entry 的 sprite 表里，那一行最右侧的一格 = **`sprite192 = { x: 193, y: 177, w: 62, h: 62 }`** ✓
（x 193+62 = 255 = 图集右边缘 ⇒ 确实是「最右侧」✓；图集 256×256）。
目视核对（合成到深底）＝**蓝色刺环** ✓ —— 与卡里一直叫的「**蓝圈**」对得上 ✓✓。

## 为什么改用贴图（而不是程序化图元）

1. **TH19 那条画法本仓没找到**：我扫过 `effect.anm`（187 脚本）、`bullet/enemy/pl00/aura/front.anm`，
   **没有**「暗色（或亮色）+ `blendMode(2)` + 大 `drawPoly`」那一条 ⇒ 很可能与 hits 计数器一样是
   **代码画的** ✗（那我们就没法「照抄」）。
2. owner 给了**退路**：「**如果 th18 里面没有对应函数，那就换成 th09 的暴风样式**」✓
3. ⇒ 直接用 **TH09 的原版贴图**（一个 sprite）—— 比任何程序化近似都更「一手」✓。

## 纪律

- **1:1 切、不缩放、不重绘**（`METHOD.md`：贴图走原生切图，别自己画）；
- 原版字节只在 `local/`（gitignored）⇒ 本脚本**构建时用 `thanm -x9` 现取**（自包含）；
- 图集尺寸变了就**直接报错**。
"""
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

from PIL import Image

HERE = Path(__file__).resolve().parent          # assets/ability/
OUT = HERE / "pofv109_storm"
REPO = HERE.parents[4]

SRC_ANM = REPO / "local/th09.v1.50a/dat/etama.anm"
THANM = REPO / "local/vendor/thtk/build/thanm/thanm"
ANMMAP = REPO / "local/vendor/eclmap/anmmap/v2.anmm"   # TH09 的 anm 头版本 = 3 ⇒ v2.anmm

#: 一手：`sprite192 = { x: 193, y: 177, w: 62, h: 62 }`（`etama2.png` 那一行最右）
CELL = ("STORM", 193, 177, 62, 62)
EXPECT_SIZE = (256, 256)


def main():
    if not SRC_ANM.is_file():
        raise SystemExit(f"缺 TH09 原始样本 {SRC_ANM}")
    tmp = Path(tempfile.mkdtemp(prefix="pofv109_storm_"))
    try:
        r = subprocess.run([str(THANM), "-x9", str(SRC_ANM), "-m", str(ANMMAP)],
                           cwd=tmp, capture_output=True, text=True)
        if r.returncode != 0:
            raise SystemExit(f"thanm 解包失败：{r.stderr.strip() or r.stdout.strip()}")
        png = tmp / "data/etama/etama2.png"
        if not png.is_file():
            cand = list(tmp.rglob("etama2.png"))
            if not cand:
                raise SystemExit("解包后找不到 etama2.png")
            png = cand[0]
        atlas = Image.open(png).convert("RGBA")
        if atlas.size != EXPECT_SIZE:
            raise SystemExit(f"TH09 etama2 尺寸变了：{atlas.size}（期望 {EXPECT_SIZE}）")
        name, x, y, w, h = CELL
        OUT.mkdir(exist_ok=True)
        atlas.crop((x, y, x + w, y + h)).save(OUT / f"{name}.png")
        print(f"  {name}.png  {w}x{h}  ← etama2.png ({x},{y})  [TH09 sprite192 = 蓝刺环]")
        print(f"→ {OUT}")
    finally:
        shutil.rmtree(tmp, ignore_errors=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())
