#!/usr/bin/env python3
"""旋风斩（id 79）的卡图画面，程序生成（固定种子，重跑逐字节一致）。

    python3 make_whirlwind_art.py
    python3 ../fit_card.py WHIRLWIND _src/WHIRLWIND_face.png --no-detect --trim 0 --margin 0 --bg '#0b0e14'

一道横扫一整圈的刀光：以画面中心为轴，刀刃从 −0.35π 扫到 +1.5π，越靠近「现在」的角度越亮越实，
越往回越淡越散 —— 也就是动态模糊本身。画法全在极坐标里：亮度只是 (角度, 半径) 的函数，所以
拖尾、锋刃、火花全是同一个场的不同截面，没有一笔是贴上去的。

配色照原生炸弹的冷白 + 青，底是深蓝黑；中心留一小团过曝的白，暗示挥刀的人。
"""
import math
from pathlib import Path

import numpy as np
from PIL import Image

HERE = Path(__file__).resolve().parent
SRC = HERE / "_src"
SEED = 0x1979  # 旋风斩 = id 79

BG = np.array([0.035, 0.048, 0.075])          # 深蓝黑底
EDGE = np.array([1.00, 1.00, 1.00])           # 锋刃：过曝白
BODY = np.array([0.55, 0.92, 1.00])           # 刀光主体：冷青
TAIL = np.array([0.13, 0.30, 0.60])           # 拖尾末端：暗蓝

SWEEP = 1.45 * math.pi                        # 扫过的总角度（不到一整圈：留出缺口，起止看得出来）
HEAD = -0.62 * math.pi                        # 刀锋「现在」所在的角度（正上方偏右，顺时针扫下来）


def to_image(rgb: np.ndarray) -> Image.Image:
    return Image.fromarray(np.clip(rgb * 255.0 + 0.5, 0, 255).astype(np.uint8), "RGB")


def make_face(w: int = 512, h: int = 640) -> Image.Image:
    rng = np.random.default_rng(SEED)
    y, x = np.mgrid[0:h, 0:w].astype(np.float64) + 0.5
    cx, cy = w * 0.5, h * 0.47
    r0 = min(w, h) * 0.315                                   # 刀光所在的半径

    dx, dy = x - cx, y - cy
    rad = np.hypot(dx, dy)
    ang = np.arctan2(dy, dx)

    # u = 从刀锋往回退了多少角度（0 = 锋刃，1 = 拖尾末端）；顺时针扫
    u = ((HEAD - ang) % (2.0 * math.pi)) / SWEEP
    tail = np.clip(1.0 - u, 0.0, 1.0)                        # 1 = 锋刃处，0 = 尾端外
    # 尾端**渐隐**而不是切断：硬的角度掩模会在画面上留一道直边（第一版右侧那道）
    live = np.clip((1.0 - u) / 0.26, 0.0, 1.0) ** 1.4

    # 半径向的截面：越靠尾越宽越散（动态模糊）。锋刃处也留够宽度 —— 512 上不到 8 px 的线，
    # 缩到 256 的卡图就没了（第一版就是这么糊掉的）。
    width = 9.5 + 30.0 * u ** 1.25
    across = (rad - r0 + 16.0 * u ** 1.6) / width             # 拖尾同时向外甩开一点
    blade = np.exp(-across ** 2) * live

    # 亮度沿拖尾衰减：锋刃过曝，尾端掉到暗蓝
    glow = blade * (0.10 + 0.90 * tail ** 1.5)

    # 沿刀光的丝缕（同一个场再调一次相位，不是叠贴图）
    strand = 0.80 + 0.20 * np.cos(u * SWEEP * 7.0 + across * 2.6)
    glow = glow * strand

    # 辉光：同一条弧再来一次宽而淡的，让刀光在暗底上「亮」起来而不是只是一条线
    bloom = np.exp(-(across / 3.4) ** 2) * live * (0.05 + 0.95 * tail ** 1.8) * 0.42
    glow = np.clip(glow + bloom, 0.0, 1.4)

    # 刀尖：锋刃那一端再压一个热点，给整条弧一个明确的「现在」
    hx, hy = cx + math.cos(HEAD) * r0, cy + math.sin(HEAD) * r0
    tip = np.exp(-(np.hypot(x - hx, y - hy) / 21.0) ** 2)
    glow = glow + tip * 0.95

    rgb = TAIL + (BODY - TAIL) * np.clip(tail * 1.25, 0.0, 1.0)[..., None]
    rgb = rgb + (EDGE - rgb) * np.clip((glow - 0.55) / 0.45, 0.0, 1.0)[..., None]

    img = BG + (rgb - BG) * np.clip(glow, 0.0, 1.0)[..., None]

    # 外圈一层很淡的气流环，让「旋」有个边界
    halo = np.exp(-((rad - r0 * 1.24) / 24.0) ** 2) * 0.13 * (0.30 + 0.70 * tail) * live
    img = img + (BODY - img) * halo[..., None]

    # 中心的挥刀者：一小团过曝，外面裹一层辉光
    core = np.exp(-(rad / 26.0) ** 2)
    img = img + (EDGE - img) * np.clip(core * 0.85, 0.0, 1.0)[..., None]
    img = img + (BODY - img) * (np.exp(-(rad / 74.0) ** 2) * 0.22)[..., None]

    # 锋刃甩出去的火花：只落在刀光前段，沿切线拉成短线（固定种子）
    spark = np.zeros((h, w))
    for _ in range(140):
        t = rng.random() ** 2.2                               # 集中在锋刃附近
        a = HEAD - t * SWEEP * 0.42
        rr = r0 + rng.normal(0.0, 15.0)
        sx, sy = cx + math.cos(a) * rr, cy + math.sin(a) * rr
        ln = 8.0 + rng.random() * 26.0
        tx, ty = -math.sin(a), math.cos(a)                    # 切线方向
        px = (x - sx) * tx + (y - sy) * ty
        py = -(x - sx) * ty + (y - sy) * tx
        spark += np.exp(-(px / ln) ** 2 - (py / 1.6) ** 2) * (0.60 + 0.40 * (1.0 - t))
    img = img + (EDGE - img) * np.clip(spark, 0.0, 1.0)[..., None]

    # 一点尘埃，别让底色太死
    dust = rng.random((h, w))
    star = (dust > 0.99965).astype(np.float64) * (1.0 - np.clip(glow, 0.0, 1.0))
    img = img + star[..., None] * 0.5
    return to_image(np.clip(img, 0.0, 1.0))


def main():
    SRC.mkdir(parents=True, exist_ok=True)
    out = SRC / "WHIRLWIND_face.png"
    make_face().save(out)
    im = Image.open(out)
    print(f"  {out.relative_to(HERE)}  {im.width}×{im.height}")


if __name__ == "__main__":
    main()
