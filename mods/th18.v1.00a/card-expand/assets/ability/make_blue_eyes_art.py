#!/usr/bin/env python3
"""青眼白龙的场上贴图：

    python3 make_blue_eyes_art.py

  blue_eyes/DRAGON.png  256×256 RGBA：从 _src/BLUE_EYES_TOPDOWN.png（用户原创，黑底俯视图，头朝上）抠掉黑底，
                        等比缩到 256 高、居中（脚本里再 scale 到场上尺寸）
  光束不在这里：用原生魔理沙的 Master Spark 贴图（pl01b / pl01b2，ORDER.txt 里 `local/` 源），脚本 79–85。

抠图：黑底纯黑（噪声 ≤ 8）。从四角泛洪（阈值 24）得到背景掩码 → 掩码外扩 2 px 的环里按亮度做 alpha 斜坡（8..56），
龙体内部的暗部（不与背景连通）保持不透明。
"""
from pathlib import Path

from PIL import Image, ImageDraw, ImageFilter

HERE = Path(__file__).resolve().parent
OUT = HERE / "blue_eyes"
SRC = OUT / "_src" / "BLUE_EYES_TOPDOWN.png"
SENTINEL = (255, 0, 255)


def key_black(im: Image.Image) -> Image.Image:
    rgb = im.convert("RGB")
    w, h = rgb.size
    flood = rgb.copy()
    for seed in ((0, 0), (w - 1, 0), (0, h - 1), (w - 1, h - 1)):
        ImageDraw.floodfill(flood, seed, SENTINEL, thresh=24)
    fpx = flood.load()
    mask = Image.new("L", (w, h), 0)              # 255 = 背景
    mpx = mask.load()
    for y in range(h):
        for x in range(w):
            if fpx[x, y] == SENTINEL:
                mpx[x, y] = 255
    ring = mask.filter(ImageFilter.MaxFilter(5))   # 背景外扩 2 px：边缘抗锯齿像素在这圈里
    rpx = ring.load()
    px = rgb.load()
    out = Image.new("RGBA", (w, h))
    opx = out.load()
    for y in range(h):
        for x in range(w):
            r, g, b = px[x, y]
            if mpx[x, y]:
                a = 0
            elif rpx[x, y]:
                a = max(0, min(255, (max(r, g, b) - 8) * 255 // 48))
            else:
                a = 255
            opx[x, y] = (r, g, b, a)
    return out


def dragon() -> Image.Image:
    im = key_black(Image.open(SRC))
    im = im.crop(im.getbbox())
    scale = 256 / im.height
    im = im.resize((max(1, round(im.width * scale)), 256), Image.LANCZOS)
    canvas = Image.new("RGBA", (256, 256), (0, 0, 0, 0))
    canvas.paste(im, ((256 - im.width) // 2, 0), im)
    return canvas


def main():
    OUT.mkdir(exist_ok=True)
    d = dragon()
    d.save(OUT / "DRAGON.png")
    print(f"wrote {OUT / 'DRAGON.png'} {d.size} (bbox {d.getbbox()})")


if __name__ == "__main__":
    main()
