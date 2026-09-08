#!/usr/bin/env python3
"""股市（id 80）的卡图画面，程序生成（固定种子，重跑逐字节一致）。

    python3 make_stock_art.py
    python3 ../fit_card.py STOCK _src/STOCK_face.png --no-detect --trim 0 --margin 0 --bg '#0b0e14'

深蓝黑底上一段 K 线：红涨绿跌，最后几根拉出一波急涨，右上角一个亮点收尾。
走势由 stock_core 同款的占位模型（阶段漂移 + 噪声 + 均值回归）在 Python 里复算，不是随手画的。
"""
import random
from pathlib import Path

from PIL import Image, ImageDraw

HERE = Path(__file__).resolve().parent
SRC = HERE / "_src"
SEED = 0x1980 + 581  # 股市 = id 80，偏移量挑出型态清楚的一次
# （原始 0x1980 是全程单调下跌、红绿不分——同一个占位模型在这个种子上撞出了退化解；
#  在附近枚举「涨跌都有 + 收尾拉升」的种子，挑第一个看着顺眼的，落在 +581）

W, H = 256, 320
BG = (11, 14, 20)
UP = (255, 72, 72)
DOWN = (72, 224, 96)
GRID = (28, 34, 48)
GLOW = (255, 220, 120)


def walk(n: int) -> list[tuple[int, int, int, int]]:
    rng = random.Random(SEED)
    price = 10000.0
    regimes = [(0, 40), (15, 60), (-15, 60)]
    reg, left = 1, 0
    candles = []
    for _ in range(n):
        o = price
        hi = lo = price
        for _ in range(16):
            if left == 0:
                reg, left = rng.randrange(3), rng.randrange(80, 241)
            left -= 1
            drift, vol = regimes[reg]
            noise = sum(rng.uniform(-1, 1) for _ in range(3)) / 3
            r = drift + vol * noise + (10000 - price) / 400
            price = max(1000.0, min(999900.0, price + price * r / 10000))
            hi, lo = max(hi, price), min(lo, price)
        candles.append((int(o), int(hi), int(lo), int(price)))
    return candles


def main() -> None:
    SRC.mkdir(exist_ok=True)
    img = Image.new("RGB", (W, H), BG)
    d = ImageDraw.Draw(img)
    for y in range(40, H, 40):
        d.line([(0, y), (W, y)], fill=GRID)
    candles = walk(20)
    lo = min(c[2] for c in candles)
    hi = max(c[1] for c in candles)
    top, bottom = 48, H - 48

    def py(p: int) -> int:
        return bottom - int((p - lo) * (bottom - top) / max(1, hi - lo))

    pitch = W // len(candles)
    for i, (o, h, l, c) in enumerate(candles):
        x = i * pitch + pitch // 2
        color = UP if c >= o else DOWN
        d.line([(x, py(h)), (x, py(l))], fill=color, width=2)
        y0, y1 = sorted((py(o), py(c)))
        d.rectangle([x - pitch // 2 + 2, y0, x + pitch // 2 - 2, max(y1, y0 + 2)], fill=color)
    last = candles[-1]
    gx, gy = (len(candles) - 1) * pitch + pitch // 2, py(last[3])
    for r, a in ((14, 40), (9, 90), (5, 200)):
        d.ellipse([gx - r, gy - r, gx + r, gy + r], fill=tuple(int(BG[k] + (GLOW[k] - BG[k]) * a / 255) for k in range(3)))
    out = SRC / "STOCK_face.png"
    img.save(out)
    print(f"STOCK: {out} {W}x{H}")


if __name__ == "__main__":
    main()
