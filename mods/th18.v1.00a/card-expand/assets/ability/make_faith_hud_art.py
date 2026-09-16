#!/usr/bin/env python3
"""组合 UI（110 风神录 × 111 地灵殿 共用那一块）行 2 的**「×」字形** —— TH18 自己 font 2 的原版切图。

    python3 make_faith_hud_art.py          # 出图（assets/ability/faith_hud/MUL.png = 16×32）
    python3 make_faith_hud_art.py --dump   # 附带把字形打成 ASCII（核证据用）

## 结论（2026-09-15f 第二轮，用户实跑口径）

**用 TH18 `ascii_1280.png` 的 sprite209 —— 那就是 font 2 的 `'*'`，而它本来就画成一个 `×`。**

★★ 第一版的取舍被用户否掉了，记在这里免得下次再走一遍：
第一版借的是 **TH11 `ascii.anm` entry0 的 sprite10**（`(140,0,14,14)`，墨迹 7×8）—— 它的形状是
**四角菱形（✦）**，用户实跑的原话是「**乘号是叉子不是 × 号**」。⇒ 换成 TH18 自己这一格。

## 一手（三条，互为独立）

1. **字符 → 字形号**（TH18 font 2/3）：`engine/anm/th18/03-ascii-text.md` §0b 的映射 ——
   数字/大写 = `char + 0x94`、小写 = `char + 0x74`、特殊符号 `/ : - * % $ . +` 走固定号。
   ⇒ `'*'`（0x2A）落在那一排特殊符号里。
2. **那一排是哪些格**（本脚本自己量像素，`--dump` 可复核）：`ascii_1280.png` 的 `y=320` 那一行
   16×20 单元里，`196..205` = `0`..`9`、`206` = `/`、`207` = `:`、`208` = `-`、
   **`209` = 一个 X 形（两条粗斜线交叉）**、`210` = `%`、`211` = `.`、`212` = `+`
   —— 与 `make_font2_art.py` 表里的 `-`/`%`/`.`/`+` = 208/210/211/212 逐格对上
   ⇒ **209 只能是 `'*'`**（那排特殊符号里唯一剩下的一个）。
3. **TH11 的一手格式串**是 `"%.6d*%1.2f"`（`th11:0x494198`）—— 字符就是**星号** ✓，
   两个字形都是「`*`」。⇒ **本块没有任何「拿用户口径顶替一手」的取舍**：
   我们画的还是那个字符，只是改用 **TH18 自己那套字形里 `*` 的形状**（它恰好是 ×，也正是用户要的样子）；
   顺带它和同一行倍率的数字**同一套 font 2**（同尺寸、同基线）✓。

## 落盘（与 `make_font2_art.py` 同一套约定）

**画布 16×32、16×20 的单元纵向居中（上下各 6 px 透明）** —— 于是「精灵中心 = 单元水平中线」，
C 侧就能用与 `ce_hud_vm_text_set`（`sdk.c` 的 `+5.0f * sy * dat`）**完全相同**的定位算式，
`×` 与倍率的 4 个字形天然共基线。⚠ 矩形必须保持 16×32（别按墨迹裁）：`scale.x` 乘的是矩形宽。
"""
import argparse
from pathlib import Path

from PIL import Image

HERE = Path(__file__).resolve().parent
REPO = HERE.parents[4]
SRC = REPO / "local/th18.v1.00a/anm/ascii1280/ascii/ascii_1280.png"
OUT = HERE / "faith_hud"

CELL = (208, 320, 224, 340)     # sprite209（font 2 的 `'*'`）的一手矩形：16×20
CANVAS = (16, 32)               # 与 ATLAS_FONT2 / FONT2_* 同约定（上下各 6 px 透明）
EXPECT_INK = (1, 1, 16, 16)     # 单元内的墨迹 bbox（本脚本实测；交叉核对用）


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--dump", action="store_true", help="把切出来的字形打成 ASCII")
    a = ap.parse_args()

    if not SRC.is_file():
        raise SystemExit(
            f"缺源图 {SRC.relative_to(REPO)}。\n"
            "  修法：`python3 tooling/thtk/unpack.py th18.v1.00a`（需要 local/th18.v1.00a/*.dat）。")
    src = Image.open(SRC).convert("RGBA")
    if src.size != (512, 512):
        raise SystemExit(f"{SRC.name} 尺寸 {src.size}，期望 (512, 512) —— 是别的东西？")

    cell = src.crop(CELL)
    ink = cell.getchannel("A").getbbox()
    if ink != EXPECT_INK:
        raise SystemExit(f"单元 {CELL} 的墨迹 bbox {ink} ≠ {EXPECT_INK} —— sprite209 不是那个 × ？"
                         f"（邻居：208 = `-`、210 = `%`、211 = `.`、212 = `+`，可逐格交叉核对）")

    pad = Image.new("RGBA", CANVAS, (0, 0, 0, 0))
    pad.paste(cell, ((CANVAS[0] - cell.width) // 2, (CANVAS[1] - cell.height) // 2))   # 1:1，不重采样
    OUT.mkdir(exist_ok=True)
    dst = OUT / "MUL.png"
    pad.save(dst)
    print(f"{dst.relative_to(REPO)} ← {SRC.relative_to(REPO)} 的 {CELL}"
          f"（TH18 font 2 的 `'*'` = × 形，墨迹 {ink}）→ 画布 {CANVAS[0]}x{CANVAS[1]} 居中")
    if a.dump:
        px = pad.load()
        print(f"  {pad.size[0]}x{pad.size[1]}，墨迹 bbox = {pad.getchannel('A').getbbox()}")
        for y in range(pad.size[1]):
            print("    " + "".join("#" if px[x, y][3] > 40 else "." for x in range(pad.size[0])))
    print("ORDER.txt 的那两行（**只能 append**；本轮改的是自己上一轮追加的那两行 ⇒ 不占新号）：\n"
          "  ATLAS ATLAS_FAITH_HUD 16x32\n"
          "  SIGNAL_MUL             ability/faith_hud/MUL.png                      @0,0,16x32")


if __name__ == "__main__":
    main()
