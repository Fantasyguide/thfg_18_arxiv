#!/usr/bin/env python3
"""豌豆射手（id 77）的三张图，全部程序生成（固定种子，重跑逐字节一致）。

    python3 make_peashooter_art.py

  peashooter/PEA.png          32×32   豌豆弹：亮绿小球，左上高光 + 暗边。shooter 的 anm 字段指向的
                                      script100 用它；引擎开火期间把 `player+0x10` 换成 ability.anm，
                                      所以这张图必须在 ability.anm 里（不碰四个 pl0X.anm）。
  peashooter/PEASHOOTER.png   64×64   子机本体：PVZ 味的豌豆射手头，**朝上**（弹也朝上，不用旋转）。
                                      绿球 + 上伸的吹管 + 底下一截茎和两片叶。
  peashooter/_src/PEASHOOTER_face.png 512×640  卡图画面：同一套画法的侧面像（朝右，PVZ 经典姿势）+
                                      三颗飞出去的豌豆，白底 → 再跑
      python3 ../fit_card.py PEASHOOTER ability/peashooter/_src/PEASHOOTER_face.png \
              --no-detect --trim 0 --margin 0 --bg '#ffffff'

画法：全部是「距离场 → 平滑掩码 → 按法线做球面着色」，没有外部素材。
豌豆的绿抄 PVZ 的主色（#7ab648 一族），高光偏黄绿、暗边偏墨绿。
"""
import math
from pathlib import Path

import numpy as np
from PIL import Image

HERE = Path(__file__).resolve().parent
OUT = HERE / "peashooter"
SRC = OUT / "_src"
SEED = 0x1977  # 豌豆射手 = id 77

# PVZ 味的绿：高光 / 主体 / 暗边 / 描边
LIT = np.array([0.83, 0.94, 0.47])
MID = np.array([0.48, 0.71, 0.28])
DARK = np.array([0.24, 0.42, 0.14])
LINE = np.array([0.12, 0.22, 0.07])


def to_image(rgb: np.ndarray, a: np.ndarray) -> Image.Image:
    arr = np.concatenate([rgb, a[..., None]], axis=2)
    return Image.fromarray(np.clip(arr * 255.0 + 0.5, 0, 255).astype(np.uint8), "RGBA")


def grid(h: int, w: int):
    y, x = np.mgrid[0:h, 0:w].astype(np.float64)
    return x + 0.5, y + 0.5


def smooth(d: np.ndarray, r: float, soft: float = 1.2) -> np.ndarray:
    """距离场 d 与半径 r → 0..1 掩码（内部 1），边缘 soft 像素的斜坡。"""
    return np.clip((r - d) / soft + 0.5, 0.0, 1.0)


def sphere_shade(x, y, cx, cy, r, mask):
    """球面着色：按到球心的归一化偏移取一个朝左上的光照，高光 → 主体 → 暗边。"""
    nx = (x - cx) / r
    ny = (y - cy) / r
    n2 = np.clip(1.0 - nx * nx - ny * ny, 0.0, 1.0)
    nz = np.sqrt(n2)
    lam = np.clip(nx * -0.45 + ny * -0.55 + nz * 0.70, 0.0, 1.0)   # 光从左上前方来
    rgb = DARK + (MID - DARK) * np.clip(lam * 1.55, 0.0, 1.0)[..., None]
    rgb = rgb + (LIT - rgb) * np.clip((lam - 0.72) / 0.28, 0.0, 1.0)[..., None]
    rim = np.clip((0.30 - nz) / 0.30, 0.0, 1.0)                     # 边缘压暗一圈
    rgb = rgb * (1.0 - 0.45 * rim)[..., None]
    return rgb * mask[..., None]


def seg_dist(x, y, x0, y0, x1, y1):
    dx, dy = x1 - x0, y1 - y0
    t = np.clip(((x - x0) * dx + (y - y0) * dy) / max(dx * dx + dy * dy, 1e-9), 0.0, 1.0)
    return np.hypot(x - (x0 + t * dx), y - (y0 + t * dy))


def make_pea(size: int = 32) -> Image.Image:
    x, y = grid(size, size)
    c = size / 2.0
    r = size * 0.40
    d = np.hypot(x - c, y - c)
    a = smooth(d, r)
    rgb = sphere_shade(x, y, c, c, r, a)
    # 左上一点镜面高光（小而实，读作「圆」）
    spec = smooth(np.hypot(x - (c - r * 0.34), y - (c - r * 0.36)), r * 0.20, 1.4)
    rgb = rgb + (np.array([1.0, 1.0, 0.92]) - rgb) * (0.80 * spec * a)[..., None]
    return to_image(rgb, a)


def draw_shooter(x, y, cx, cy, scale, angle):
    """豌豆射手的头：球 + 沿 angle 伸出去的吹管（末端喇叭口）。→ (rgb, alpha)。

    angle 是弧度，0 = +x（朝右），-π/2 = 朝上。返回的 rgb 已乘过 alpha。"""
    head_r = 20.0 * scale
    tube_len = 34.0 * scale
    tube_r0 = 8.0 * scale                       # 靠头那端
    tube_r1 = 11.5 * scale                      # 喇叭口
    ux, uy = math.cos(angle), math.sin(angle)

    # 吹管：从头心往外的一段，半径沿轴线性变化 → 用「到线段的距离 − 局部半径」
    x0, y0 = cx + ux * head_r * 0.35, cy + uy * head_r * 0.35
    x1, y1 = x0 + ux * tube_len, y0 + uy * tube_len
    dx, dy = x1 - x0, y1 - y0
    t = np.clip(((x - x0) * dx + (y - y0) * dy) / max(dx * dx + dy * dy, 1e-9), 0.0, 1.0)
    tube_d = np.hypot(x - (x0 + t * dx), y - (y0 + t * dy))
    tube_r = tube_r0 + (tube_r1 - tube_r0) * t
    tube = np.clip((tube_r - tube_d) / 1.2 + 0.5, 0.0, 1.0)

    head_d = np.hypot(x - cx, y - cy)
    head = smooth(head_d, head_r)
    a = np.clip(head + tube, 0.0, 1.0)

    rgb = sphere_shade(x, y, cx, cy, head_r, head)
    # 吹管单独着色：沿垂直轴的横截面当圆柱打光
    px, py = -uy, ux                                            # 轴的法向
    off = ((x - x0) * px + (y - y0) * py) / np.maximum(tube_r, 1e-9)
    nz = np.sqrt(np.clip(1.0 - off * off, 0.0, 1.0))
    lam = np.clip(off * -0.5 + nz * 0.8, 0.0, 1.0)
    trgb = DARK + (MID - DARK) * np.clip(lam * 1.5, 0.0, 1.0)[..., None]
    trgb = trgb + (LIT - trgb) * np.clip((lam - 0.78) / 0.22, 0.0, 1.0)[..., None]
    # 喇叭口内壁：末端压暗成一个「洞」，洞心略微退进管口里
    mx, my = x1 + ux * tube_r1 * 0.42, y1 + uy * tube_r1 * 0.42
    mouth = smooth(np.hypot(x - mx, y - my), tube_r1 * 0.62, 1.2)
    trgb = trgb * (1.0 - 0.70 * mouth)[..., None]
    only_tube = np.clip(tube - head, 0.0, 1.0)
    rgb = rgb + trgb * only_tube[..., None]
    muzzle = (x1 + ux * tube_r1, y1 + uy * tube_r1)     # 炮口尖端：豌豆从这里出去
    return rgb, a, muzzle


def add_stem(x, y, rgb, a, cx, cy, scale, stem_len=40.0):
    """头下面一截细茎 + 茎脚两片朝斜上方的叶（PVZ 的豌豆射手就长这样）。

    stem_len 是茎从头心往下的长度（未乘 scale）；64×64 的子机贴图放不下卡图那么长的茎，压短。"""
    bot = cy + stem_len * scale
    stem = smooth(seg_dist(x, y, cx, cy + 6.0 * scale, cx, bot), 3.0 * scale)
    leaves = np.zeros_like(stem)
    for sgn in (-1.0, 1.0):
        lx, ly = cx + sgn * 13.0 * scale, bot - 4.0 * scale
        th = sgn * 0.62                                   # 叶尖朝斜上外方
        ex = (x - lx) * math.cos(th) + (y - ly) * math.sin(th)
        ey = -(x - lx) * math.sin(th) + (y - ly) * math.cos(th)
        leaves = np.maximum(leaves, smooth(np.hypot(ex / 1.7, ey / 0.75), 7.5 * scale))
    g = np.clip(stem + leaves, 0.0, 1.0)
    only = np.clip(g - a, 0.0, 1.0)
    shade = DARK + (MID - DARK) * 0.55
    return rgb + shade * only[..., None], np.clip(a + g, 0.0, 1.0)


def outline(rgb, a, width: float = 1.6):
    """描边：alpha 的外扩减去自身 → 墨绿轮廓，压在下面。"""
    from PIL import ImageFilter
    m = Image.fromarray(np.clip(a * 255, 0, 255).astype(np.uint8), "L")
    grown = np.asarray(m.filter(ImageFilter.MaxFilter(3)), dtype=np.float64) / 255.0
    if width > 1.5:
        grown = np.maximum(grown, np.asarray(
            Image.fromarray(np.clip(grown * 255, 0, 255).astype(np.uint8), "L")
            .filter(ImageFilter.GaussianBlur(0.6)), dtype=np.float64) / 255.0)
    ring = np.clip(grown - a, 0.0, 1.0)
    return rgb + LINE * ring[..., None], np.clip(a + ring, 0.0, 1.0)


def make_option(size: int = 64) -> Image.Image:
    x, y = grid(size, size)
    scale = size / 64.0 * 0.78
    cx, cy = size * 0.50, size * 0.67
    rgb, a, _ = draw_shooter(x, y, cx, cy, scale, -math.pi / 2)  # 朝上：弹也朝上，脚本不用转
    rgb, a = add_stem(x, y, rgb, a, cx, cy, scale, stem_len=24.0)
    rgb, a = outline(rgb, a)
    return to_image(rgb, a)


def make_card_face(w: int = 512, h: int = 640) -> Image.Image:
    """卡图画面：侧面像斜朝右上（4:5 竖版里这样最不浪费地方），三颗豌豆顺着炮口方向飞出去。"""
    x, y = grid(h, w)
    scale = 4.3
    angle = -0.34                                    # 约 −19°，朝右上
    cx, cy = w * 0.24, h * 0.60
    rgb, a, (mx, my) = draw_shooter(x, y, cx, cy, scale, angle)
    rgb, a = add_stem(x, y, rgb, a, cx, cy, scale)
    ux, uy = math.cos(angle), math.sin(angle)
    for dist, r, al in ((40.0, 25.0, 1.0), (104.0, 20.0, 0.9), (166.0, 15.0, 0.72)):
        px, py = mx + ux * dist, my + uy * dist
        m = smooth(np.hypot(x - px, y - py), r) * al
        prgb = sphere_shade(x, y, px, py, r, m)
        spec = smooth(np.hypot(x - (px - r * 0.34), y - (py - r * 0.36)), r * 0.20, 1.4)
        prgb = prgb + (np.array([1.0, 1.0, 0.92]) - prgb) * (0.75 * spec * m)[..., None]
        only = np.clip(m - a, 0.0, 1.0)
        rgb = rgb + prgb * (only / np.maximum(m, 1e-9))[..., None] * only[..., None]
        a = np.clip(a + only, 0.0, 1.0)
    rgb, a = outline(rgb, a, 2.4)
    card = Image.new("RGBA", (w, h), "#ffffff")
    card.alpha_composite(to_image(rgb, a))
    return card


def main():
    np.random.seed(SEED)
    OUT.mkdir(parents=True, exist_ok=True)
    SRC.mkdir(parents=True, exist_ok=True)
    make_pea().save(OUT / "PEA.png")
    make_option().save(OUT / "PEASHOOTER.png")
    make_card_face().save(SRC / "PEASHOOTER_face.png")
    for p in (OUT / "PEA.png", OUT / "PEASHOOTER.png", SRC / "PEASHOOTER_face.png"):
        im = Image.open(p)
        print(f"  {p.relative_to(HERE)}  {im.width}×{im.height}")


if __name__ == "__main__":
    main()
