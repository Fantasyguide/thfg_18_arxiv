#!/usr/bin/env python3
"""股市（id 80）的卡图画面：把用户原创的那张方形图标铺进 256×320 的卡面。

    python3 make_stock_art.py
    python3 ../fit_card.py STOCK _src/STOCK_face.png --no-detect --trim 0 --margin 0

画面本体是 `_art/stock.png`（用户原创：红底、白色粗蜡烛、白网格、左侧价格标签、几处星芒）。
这里只做两件事，都不改画面内容：

1. **缩到 220 宽**。原生卡框外 3 + 斜面 13 + 内 2 = 每边 18 px，画面区只有 220×284。
   原图内容（实测最左的白色像素）从 x=7 起，直接铺满 256 宽的话「110」那个价格标签
   会有 11 px 压在框底下。缩到 220 居中之后，内容落在 x 24..217，整张都在框内。
2. **上下左右补背景**。原图是一条**纯竖直**渐变（同一行左右同色，实测四角取样一致），
   顶 (252, 38, 17) → 底 (220, 16, 17)。把这条线按同样的斜率外推到整张 256×320，
   于是补出来的背景与图标自身的渐变在接缝处**逐行同色**——图标的圆角透明区落到背景上
   看不出边界，不用再去抠圆角。

确定性：纯像素运算，无随机；同一张源图重跑逐字节一致。
"""
from pathlib import Path

from PIL import Image

HERE = Path(__file__).resolve().parent
SRC_ART = HERE / "_art" / "stock.png"          # 用户原创画面
OUT     = HERE / "_src" / "STOCK_face.png"

W, H = 256, 320          # 卡面
FRAME = 18               # 卡框每边吃掉的像素（cardframe.py 的外 3 + 斜面 13 + 内 2）
ICON = W - 2 * FRAME     # 220：缩放后的图标边长，正好等于画面区宽度
TOP = (H - ICON) // 2    # 竖直居中：50

TOP_RGB    = (252, 38, 17)   # 图标第 0 行
BOTTOM_RGB = (220, 16, 17)   # 图标第 255 行


def main() -> None:
    icon = Image.open(SRC_ART).convert("RGBA").resize((ICON, ICON), Image.LANCZOS)

    # 背景：把图标自身的渐变按同斜率外推到整张卡面。
    # 图标缩放后占 y = TOP .. TOP+ICON-1，所以这两行必须正好等于原图的首尾色。
    bg = Image.new("RGB", (W, H))
    px = bg.load()
    span = ICON - 1
    for y in range(H):
        t = (y - TOP) / span                       # 图标内 0..1，外面自然外推
        row = tuple(
            max(0, min(255, round(TOP_RGB[c] + (BOTTOM_RGB[c] - TOP_RGB[c]) * t)))
            for c in range(3)
        )
        for x in range(W):
            px[x, y] = row

    bg.paste(icon, (FRAME, TOP), icon)             # 用 alpha 合成，圆角落到同色背景上
    OUT.parent.mkdir(exist_ok=True)
    bg.save(OUT)
    print(f"STOCK: {OUT} {W}x{H}（图标 {ICON}×{ICON} @ ({FRAME}, {TOP})，其余为外推渐变）")


if __name__ == "__main__":
    main()
