#!/usr/bin/env python3
"""把一张现成的卡图（任意尺寸/比例）裁切、缩放到 abcard.anm 的 256×320 / 64×80。

    python3 fit_card.py REVERSE reverse_card.png --fill          # 去白边、上下各修 3%、横向拉满 256、白底
    python3 fit_card.py NAME src.png --trim 0 --bg '#000000'     # 等比缩放居中、不修边、黑底
    python3 fit_card.py NAME src.png --no-detect                 # 不做去白边，整图直接放

⑥ 默认再套原生同款边框（cardframe.py，--no-frame 关）。
步骤：① 去白边（非白像素的包围盒）② 上下各修 --trim 比例 ③ 缩到高 320 − 2·margin：默认等比、居中；
--fill 时宽直接拉到 256（不保比例，两侧不留底色）④ 贴到 --bg 底色的 256×320 上 ⑤ _min = 整图缩到 64×80。
"""
import argparse
import sys
from pathlib import Path

from PIL import Image

import cardframe

HERE = Path(__file__).resolve().parent
OUT = HERE / "cards"
MAX_W, MAX_H = 256, 320
MIN_W, MIN_H = 64, 80


def detect_card(im: Image.Image, white=235):
    """非白像素的包围盒（源图外围通常是纯白出血）。"""
    g = im.convert("L").point(lambda v: 0 if v >= white else 255)
    return g.getbbox() or (0, 0, im.width, im.height)


def fit(src: Path, trim: float, bg: str, detect: bool, margin: int, fill: bool = False) -> Image.Image:
    im = Image.open(src).convert("RGBA")
    if detect:
        im = im.crop(detect_card(im))
    if trim:
        t = int(im.height * trim)
        im = im.crop((0, t, im.width, im.height - t))
    h = MAX_H - 2 * margin
    if fill:                              # 不保比例：横向拉满
        w = MAX_W
    else:
        w = round(im.width * h / im.height)
        if w > MAX_W - 2 * margin:        # 太宽就按宽度缩
            w = MAX_W - 2 * margin
            h = round(im.height * w / im.width)
    im = im.resize((w, h), Image.LANCZOS)
    canvas = Image.new("RGBA", (MAX_W, MAX_H), bg)
    canvas.alpha_composite(im, ((MAX_W - w) // 2, (MAX_H - h) // 2))
    return canvas


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("name")
    ap.add_argument("src", type=Path)
    ap.add_argument("--trim", type=float, default=0.03, help="上下各修掉的比例（默认 0.03）")
    ap.add_argument("--bg", default="#ffffff", help="底色（默认白；'#00000000' 透明）")
    ap.add_argument("--margin", type=int, default=8, help="上下留白像素（默认 8）")
    ap.add_argument("--no-detect", action="store_true", help="不做去白边")
    ap.add_argument("--fill", action="store_true", help="横向拉伸到满宽 256（不保比例；上下仍留 margin）")
    ap.add_argument("--no-frame", action="store_true", help="不加原生同款边框（默认加，cardframe.py）")
    ap.add_argument("--out", default=str(OUT))
    a = ap.parse_args()
    out = Path(a.out)
    out.mkdir(parents=True, exist_ok=True)
    big = fit(a.src, a.trim, a.bg, not a.no_detect, a.margin, a.fill)
    art = big
    (out / "_art").mkdir(exist_ok=True)
    art.save(out / "_art" / f"{a.name}.png")                  # 无框画面留档：_min 与以后重出框都从它来
    if a.no_frame:
        big.save(out / f"{a.name}_max.png")
    else:
        big = cardframe.apply_frame(big)                      # 原生同款边框（画面等比覆盖 220×284）
        cardframe.save_framed(big, out / f"{a.name}_max.png")
    art.resize((MIN_W, MIN_H), Image.LANCZOS).save(out / f"{a.name}_min.png")   # _min 不带框（原生同款）
    print(f"{a.name}: {a.name}_max.png {MAX_W}x{MAX_H} · {a.name}_min.png {MIN_W}x{MIN_H}  ← {a.src.name}"
          f"{'  [fill]' if a.fill else ''}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
