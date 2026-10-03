#!/usr/bin/env python3
"""109 的 **hits UI 字形** —— **从 TH19 自己的 `ascii/ascii_norm2.png` 1:1 切**。

    python3 make_pofv109_hits_art.py

产物：`ability/pofv109_hits/` 下 **14 张**（数字 `0..9` + 字母 `H I T S`）

## 为什么（owner 口径）

owner 2026-10-01：「**看字体基本就是这两个之一**」（给了 TH19 的 `ascii_big.png` / `ascii_norm2.png`）。
此前我们的 hits UI 走 `ce_hud_number_set` / `ce_hud_text`，字体是**共享资产**：
- 数字 = `ATLAS_HUD_DIGIT` 的 `ability/truesilver/DIGIT_*.png`（**真银圣剑那套** ✗ 不是本卡的风格）；
- 文字 = TH18 原生 **font 2** 字形。
⇒ 本卡改成**自带一套 TH19 字形**（只影响 109 自己 ✓ 不动共享图集 ✓）。

## 一手（TH19 `ascii.anm` 的 `ascii_norm2.png` entry）

| 项 | 值 |
| --- | --- |
| 图集 | `ascii/ascii_norm2.png`（512×128）|
| 字形排布 | **比例字宽**、**24 px 步距**、高 **21**；sheet 起点 `(1, 2)`，第二行 `y = 26` |
| 字形号 | 该 entry 的 sprite **712 = `!`（ASCII 33）** 起、按 ASCII 顺序排 |
| ⇒ **数字 `0..9`** | ASCII 48..57 ⇒ sprite **727..736** |
| ⇒ 字母 **H/I/T/S** | ASCII 72/73/84/83 ⇒ sprite **751 / 752 / 763 / 762** |

⚠ **步距 = 24**（不是字形宽度）—— 字形本身宽 5..22 不等（比例字体）。

## 纪律

- **1:1 切、不缩放、不重绘**（`METHOD.md`：贴图走原生切图，别自己画）；
- 原版字节只在 `local/`（gitignored）⇒ 本脚本**构建时用 `thdat -x19` + `thanm -x19` 现取**（自包含）；
- 图集尺寸变了就**直接报错**（不静默出错图）。
"""
import subprocess
import sys
import tempfile
from pathlib import Path
import shutil

from PIL import Image

HERE = Path(__file__).resolve().parent          # assets/ability/
OUT = HERE / "pofv109_hits"
REPO = HERE.parents[4]

SRC_DAT = REPO / "local/th19.v1.10c/th19.dat"
SRC_ANM = REPO / "local/th19.v1.10c/dat/ascii.anm"
THDAT = REPO / "local/vendor/thtk/build/thdat/thdat"
THANM = REPO / "local/vendor/thtk/build/thanm/thanm"
ANMMAP = REPO / "local/vendor/eclmap/anmmap/v8.anmm"

EXPECT_SIZE = (512, 288)
#: 一手：字形步距 24、**每行 21 格**（x = 1,25,…,481）、行距 24（y = 2,26,50,…）、高 21；
#:   sheet 从 sprite712 = **空格（ASCII 32）** 起、按 ASCII 顺序排
#:   （★ 我第一版按 `!`=33 算，切出来的 `D0` 是 `/`（47，正好是 `0` 的前一格）⇒ 差一格，
#:     可见的一格 `!` 其实是第二格 ✓）。
#:   （换行点由 sprite 表核准：sprite733 回到 x=1/y=26 ⇒ 第一行 21 格 ✓）
ROW_Y0 = 1
CELL_X = 32
CELL_Y = 40
PER_ROW = 16
GLYPH_H = 37
SPRITE_BASE = 320          # ★ 空格 = ASCII 32（ascii_big）
COL0_X = 1

#: 要切的字形：(产物名, ASCII 码)
GLYPHS = [(f"D{i}", 48 + i) for i in range(10)] + [("H", 72), ("I", 73), ("T", 84), ("S", 83)]


def sprite_rect(ascii_code: int):
    """由 ASCII 码推该字形的矩形（一手：sprite712 = `!` 按顺序排、24 px 步距、两行）。"""
    k = ascii_code - 32                      # ★ 相对**空格**的序号（一手：差一格会整体错位）
    if k < 0:
        raise SystemExit(f"ASCII {ascii_code} 在空格之前，本表不覆盖")
    row, col = divmod(k, PER_ROW)
    y = ROW_Y0 + row * CELL_Y
    x = COL0_X + col * CELL_X
    return x, y


def extract(tmp: Path) -> Path:
    if not SRC_ANM.is_file():
        if not SRC_DAT.is_file():
            raise SystemExit(f"缺 TH19 原始样本 {SRC_DAT}")
        SRC_ANM.parent.mkdir(parents=True, exist_ok=True)
        subprocess.run([str(THDAT), "-x19", str(SRC_DAT), "ascii.anm", "-C", str(SRC_ANM.parent)],
                       capture_output=True, text=True)
        if not SRC_ANM.is_file():
            raise SystemExit("thdat 解 ascii.anm 失败")
    r = subprocess.run([str(THANM), "-x19", str(SRC_ANM), "-m", str(ANMMAP)],
                       cwd=tmp, capture_output=True, text=True)
    if r.returncode != 0:
        raise SystemExit(f"thanm 解包失败：{r.stderr.strip() or r.stdout.strip()}")
    return tmp / "ascii"


def main():
    tmp = Path(tempfile.mkdtemp(prefix="pofv109_hits_"))
    try:
        adir = extract(tmp)
        fill = Image.open(adir / "ascii_big.png").convert("RGBA")
        edge = Image.open(adir / "ascii_bigf.png").convert("RGBA")
        for im, nm in ((fill, "ascii_big"), (edge, "ascii_bigf")):
            if im.size != EXPECT_SIZE:
                raise SystemExit(f"TH19 {nm} 尺寸变了：{im.size}（期望 {EXPECT_SIZE}）")
        # ⚠ 描边版（`bigf`）是**同一布局**（x/y 逐格相同）⇒ 两张图用同一个 `sprite_rect()` ✓
        #   （一手：`ascii_big` 首格 320、`ascii_bigf` 首格 418，换行点/坐标序列完全一致）。
        OUT.mkdir(exist_ok=True)
        for name, code in GLYPHS:
            x, y = sprite_rect(code)
            for im, suffix in ((fill, ""), (edge, "_EDGE")):
                cell = im.crop((x, y, x + CELL_X, y + GLYPH_H))
                cell.save(OUT / f"{name}{suffix}.png")
            print(f"  {name}.png / {name}_EDGE.png  {CELL_X}x{GLYPH_H}  ← ({x},{y})  [ASCII {code}]")
        print(f"→ {OUT}")
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


if __name__ == "__main__":
    sys.exit(main())
