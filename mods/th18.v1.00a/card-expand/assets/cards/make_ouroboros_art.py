#!/usr/bin/env python3
"""衔尾蛇（id 78）的卡图画面，程序生成（固定种子，重跑逐字节一致）。

    python3 make_ouroboros_art.py
    python3 ../fit_card.py OUROBOROS _src/OUROBOROS_face.png --no-detect --trim 0 --margin 0 --bg '#0e0b14'

一条咬住自己尾巴的蛇盘成一个圆：从头到尾身体渐细，鳞片沿身体铺，头在右上、张口咬住从下方绕回来的尾尖。
画法全是极坐标下的距离场——身体是「到圆环的距离 − 该角度处的半径」，所以渐细、鳞片、明暗都只是角度的函数。
"""
import math
from pathlib import Path

import numpy as np
from PIL import Image

HERE = Path(__file__).resolve().parent
SRC = HERE / "_src"
SEED = 0x1978  # 衔尾蛇 = id 78

BG = np.array([0.055, 0.043, 0.078])          # 深紫黑底
SCALE_LIT = np.array([0.74, 0.86, 0.52])      # 鳞片高光：偏黄的青绿
SCALE_MID = np.array([0.29, 0.52, 0.34])
SCALE_DARK = np.array([0.09, 0.18, 0.14])
BELLY = np.array([0.86, 0.80, 0.52])          # 腹面米黄
EYE = np.array([0.94, 0.76, 0.20])


def to_image(rgb: np.ndarray) -> Image.Image:
    return Image.fromarray(np.clip(rgb * 255.0 + 0.5, 0, 255).astype(np.uint8), "RGB")


def smooth(d: np.ndarray, r: float, soft: float = 1.6) -> np.ndarray:
    return np.clip((r - d) / soft + 0.5, 0.0, 1.0)


def make_face(w: int = 512, h: int = 640) -> Image.Image:
    rng = np.random.default_rng(SEED)
    y, x = np.mgrid[0:h, 0:w].astype(np.float64) + 0.5
    cx, cy = w * 0.5, h * 0.47
    ring_r = min(w, h) * 0.30

    dx, dy = x - cx, y - cy
    rad = np.hypot(dx, dy)
    ang = np.arctan2(dy, dx)                                  # (−π, π]

    # t = 沿蛇身的参数，0 = 头（右上，−0.6 rad 处），逆时针绕一圈回到 1 = 尾尖
    t = ((-0.6 - ang) % (2.0 * math.pi)) / (2.0 * math.pi)

    # 身体半宽：头附近最粗，到尾尖收成一点（末端 8% 快速收细，好让它能被咬住）
    thick = 31.0 * (1.0 - 0.72 * t) * np.clip((1.0 - t) / 0.08, 0.0, 1.0) ** 0.5
    thick = np.maximum(thick, 0.0)
    body_d = np.abs(rad - ring_r)
    body = smooth(body_d, thick)

    # 光照：环的外缘暗、内缘亮一点，再叠一层沿身体的鳞片
    across = np.clip((rad - ring_r) / np.maximum(thick, 1e-9), -1.0, 1.0)
    lam = np.clip(0.55 - across * 0.55, 0.0, 1.0)
    rgb = SCALE_DARK + (SCALE_MID - SCALE_DARK) * lam[..., None]
    rgb = rgb + (SCALE_LIT - rgb) * np.clip((lam - 0.72) / 0.28, 0.0, 1.0)[..., None]

    scales = 0.5 + 0.5 * np.cos(t * 2.0 * math.pi * 46.0 - across * 2.2)
    rgb = rgb * (0.80 + 0.30 * scales)[..., None]

    belly = smooth(np.abs(across + 0.55), 0.30, 0.35)          # 内缘一条腹带
    rgb = rgb + (BELLY - rgb) * (0.75 * belly)[..., None]

    img = BG + (rgb - BG) * body[..., None]

    # 头：在 t≈0 处放一个稍大的椭圆（沿切线拉长），加一只眼和一条口缝
    hx, hy = cx + math.cos(-0.6) * ring_r, cy + math.sin(-0.6) * ring_r
    tang = -0.6 + math.pi / 2.0
    ex = (x - hx) * math.cos(tang) + (y - hy) * math.sin(tang)
    ey = -(x - hx) * math.sin(tang) + (y - hy) * math.cos(tang)
    head = smooth(np.hypot(ex / 1.42, ey / 0.90), 41.0)
    hlam = np.clip(0.5 - ey / 40.0, 0.0, 1.0)
    hrgb = SCALE_DARK + (SCALE_MID - SCALE_DARK) * hlam[..., None]
    hrgb = hrgb + (SCALE_LIT - hrgb) * np.clip((hlam - 0.70) / 0.30, 0.0, 1.0)[..., None]
    img = img + (hrgb - img) * head[..., None]

    eye = smooth(np.hypot((ex - 17.0) / 1.0, (ey + 11.0) / 0.75), 7.5, 1.2)
    img = img + (EYE - img) * eye[..., None]
    pupil = smooth(np.hypot((ex - 17.0) / 0.34, (ey + 11.0) / 0.9), 3.5, 1.0)
    img = img * (1.0 - 0.92 * pupil)[..., None]

    mouth = smooth(np.abs(ey - 15.0), 2.4, 1.0) * smooth(np.abs(ex - 14.0), 26.0, 2.0)
    img = img * (1.0 - 0.72 * mouth)[..., None]

    # 一点星尘，别让底色太死（固定种子）
    dust = rng.random((h, w))
    star = (dust > 0.99965).astype(np.float64) * (1.0 - body) * (1.0 - head)
    img = img + star[..., None] * 0.55
    return to_image(np.clip(img, 0.0, 1.0))


def main():
    SRC.mkdir(parents=True, exist_ok=True)
    out = SRC / "OUROBOROS_face.png"
    make_face().save(out)
    im = Image.open(out)
    print(f"  {out.relative_to(HERE)}  {im.width}×{im.height}")


if __name__ == "__main__":
    main()
