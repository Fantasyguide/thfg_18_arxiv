#!/usr/bin/env python3
"""真银圣剑的 HUD 耐久数字：10 张 32×32 RGBA（`ability/truesilver/DIGIT_0..9.png`），白字 + 2 px 黑描边，DejaVu Serif Bold。
ORDER.txt 里 10 行 TS_DIGIT_0..9（32x32），脚本 99_hud_digit 起一个 VM，C 用 set_sprite 换数字。"""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

HERE = Path(__file__).resolve().parent
OUT = HERE / "truesilver"
FONT = "/usr/share/fonts/truetype/dejavu/DejaVuSerif-Bold.ttf"

def main():
    OUT.mkdir(exist_ok=True)
    font = ImageFont.truetype(FONT, 26)
    for d in range(10):
        im = Image.new("RGBA", (32, 32), (0, 0, 0, 0))
        dr = ImageDraw.Draw(im)
        s = str(d)
        l, t, r, b = dr.textbbox((0, 0), s, font=font)
        x, y = (32 - (r - l)) // 2 - l, (32 - (b - t)) // 2 - t
        dr.text((x, y), s, font=font, fill=(255, 255, 255, 255), stroke_width=2, stroke_fill=(0, 0, 0, 255))
        im.save(OUT / f"DIGIT_{d}.png")
    print(f"truesilver: 10 digits -> {OUT.relative_to(HERE.parent)}")

if __name__ == "__main__":
    main()
