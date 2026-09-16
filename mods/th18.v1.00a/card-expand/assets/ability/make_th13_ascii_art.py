#!/usr/bin/env python3
"""从 **TH13 的 `ascii.png`** 切出灵界剩余时间要用的字形（原版原样，不重画）。

    python3 make_th13_ascii_art.py

## 为什么不用 TH18 的字形（用户 2026-09-15 口径）

> 「灵界的字符你不要用 th18 遗留的什么东西，得是 ascii 那张图的 anm，图像已经不遗留在 th18 里面了。
>   得走 th15 卡 UI 的那条路。」

TH18 自带的 `ascii.anm`（font 2/3）**没有** TH13 那套字形（字母 `s` 更是不在同一档尺寸），
而 TH13 的 `ascii/ascii.png` 才是原作那张图 ⇒ **按 115（TH15）卡 UI 的路数移植**：
原版贴图 → `assets/ability/**` → `ability.anm` 的图集条目 → UI 层 VM 画出来。
本脚本只负责第一步（切图），产物进 `assets/ability/th13ascii/`。

## 一手：字形格位置（本脚本自己量，不靠肉眼）

源图 256×512；**大字体**（TH13 的 HUD 用的那档，`TranceManager__on_draw__body` 把
`[esi+0x19178]` 设成 7 = 该字体 id）是图里**唯二 39 像素高**的两行：

| 行 | y | 格距 | 内容（按墨迹高度判定，逐格实测）|
| --- | --- | --- | --- |
| 上 | 264..303 | 32 | `0` `1` `2` `3` `4` `5` `6` `7`（8 格墨迹全高）|
| 下 | 304..343 | 32 | `8` `9` `.` `s`（前两格全高；第 3 格只有底部 8 行 = `.`；第 4 格只有下半 20 行 = `s`，小写落在 x-height）|

⇒ 这套字体**正好 12 个字形 = 数字 + `.` + `s`**，够拼 TH13 那个 `%.2fs`（`th13:0x405940`）。

落盘画布 = **整个字格 32×40**（不按墨迹裁）：于是"格距 = 画布宽"在 C 侧是常数，
字形之间的相对位置与原作者排的完全一致（`.` 的留白也在里面）。
⚠ 与 `make_font2_art.py` 的 16×32 画布**不同**（那是 TH18 font 2 的度量，别混）。

产物只在仓库里（**是切出来的原版像素，不是重画**）；源图 `local/th13.v1.00c/ascii/ascii.png`
本身在 `local/`（gitignored）⇒ 这份 PNG 与源图同版权归属，随 mod 一起发布到本机 modkit。
"""
from pathlib import Path

from PIL import Image

HERE = Path(__file__).resolve().parent
REPO = HERE.parents[4]
SRC = REPO / "local/th13.v1.00c/ascii/ascii.png"
OUT = HERE / "th13ascii"

CELL_W, CELL_H = 32, 40           # 字格（格距 32、行距 40）
ROW_Y = (264, 304)                # 两行的 y 起点
# (名字, 行, 列)
GLYPHS = ([("D%d" % i, 0, i) for i in range(8)]
          + [("D8", 1, 0), ("D9", 1, 1), ("DOT", 1, 2), ("S", 1, 3)])


def main():
    if not SRC.is_file():
        raise SystemExit(
            f"缺源图 {SRC.relative_to(REPO)}（TH13 的 `ascii/ascii.png`；"
            f"从你的 TH13 安装目录把它拷进 local/th13.v1.00c/ascii/ 即可）")
    im = Image.open(SRC).convert("RGBA")
    if im.size != (256, 512):
        raise SystemExit(f"{SRC.name} 尺寸 {im.size}，期望 (256, 512) —— 是别的东西？")
    OUT.mkdir(exist_ok=True)
    ink = im.getchannel("A")
    for name, row, col in GLYPHS:
        x0, y0 = col * CELL_W, ROW_Y[row]
        box = (x0, y0, x0 + CELL_W, y0 + CELL_H)
        cell = im.crop(box)
        cell.save(OUT / f"{name}.png")
        inkpx = sum(1 for p in ink.crop(box).getdata() if p)
        print(f"  {name:<4} 格 ({col},{row}) → 源矩形 {box} 墨迹 {inkpx:>4} px → {name}.png")
    print(f"→ {(OUT.relative_to(REPO))}（{len(GLYPHS)} 张 {CELL_W}×{CELL_H}）")
    print("  下一步：ORDER.txt 追加一个图集条目 + C 侧字形集参数（见 AUDIT §CZ）")


if __name__ == "__main__":
    main()
