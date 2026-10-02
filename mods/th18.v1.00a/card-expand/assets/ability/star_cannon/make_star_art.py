#!/usr/bin/env python3
"""星星炮（id 89）**弹幕贴图**：给原版星星加**蓝色辉光** + 烘一条**水滴形彗尾**。

    python3 assets/ability/star_cannon/make_star_art.py

产物（两张，都由 `entries/ORDER.txt` 引进 `ability.anm`）：
· `_src/STAR_SHOT_BLUE.png` → `STAR_CANNON_SHOT`（星星本体 + 四周蓝晕）；
· `_src/STAR_TAIL.png`      → `STAR_CANNON_TAIL`（**水滴形拖尾**：头端亮、尾端收成尖）。

为什么是烘的：owner 2026-10-02/03 实测「星星周围应当有蓝色」「它还有一个**水滴形状的拖尾**，
像彗星一样 …… 那个就是**动态光照叠起来**的东西，你得烘一个差不多的」。
原版那颗星的贴图（`Projectile_955.png`，22×24）**一个蓝像素都没有**，蓝晕与彗尾都来自
`light = 1f` 的**动态光照**在飞行路径上叠出来的残光；我们引擎没有光照通道
⇒ 只能把这两层**烘成贴图**（= ❓ 自绘近似，不是逐像素复刻）。

⚠ 彗尾**不能烘进星星贴图**：原版的星是**自转**的（`aiStyle 5`：`rotation += speed*0.01*dir`），
尾巴要是画在同一张图上就会跟着转。所以尾巴是**独立的 VM**：贴在星后面、按**速度方向**摆
（不跟自转），一帧一枚、几帧淡掉（见 `cards/star_cannon.c` 与脚本 391）。"""
import os
from PIL import Image, ImageDraw

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.join(HERE, "_src", "Projectile_955.png")
OUT = os.path.join(HERE, "_src", "STAR_SHOT_BLUE.png")
OUT_TAIL = os.path.join(HERE, "_src", "STAR_TAIL.png")

PAD = 10                    # 四周留出辉光的位置（原图 22×24 → 42×44）
GLOW = (104, 176, 255)      # 辉光色（冷蓝，取 Terraria 星光那种偏青的蓝）
GLOW_MAX = 215              # 辉光中心的最大 alpha（别盖过星星本身）
RINGS = 30


# ── 彗尾：水滴形（头端圆亮、尾端收尖），长度 ≈ 原版一颗星飞 2–3 帧的距离（14 px/帧）
TAIL_LEN = 44
TAIL_H = 16
TAIL_HEAD = 10          # 头端圆心离左边缘的位置（贴图左边是**尾尖**、右边是**头**？见下）


def make_tail():
    """水滴 / 彗尾。坐标约定：**右边的头端是星星所在处**，尾巴往**左**（−x）拖 ——
    与 `ce_anm_set_rotation(rot = 速度角)` 配合时（VM 的 +x 指向速度方向），"左"正好是**后方**。"""
    img = Image.new("RGBA", (TAIL_LEN, TAIL_H), (0, 0, 0, 0))
    px = img.load()
    cy = (TAIL_H - 1) / 2.0
    head_x = TAIL_LEN - 1 - 2                     # 头端圆心（留 2 px 余量）
    for x in range(TAIL_LEN):
        # 半径：头端最粗，往尾尖线性收到 0（水滴感）；再乘一个平滑衰减
        t = (head_x - x) / float(head_x) if head_x else 1.0
        if t < 0.0:
            t = 0.0
        r = (TAIL_H / 2.0 - 1.0) * (1.0 - t ** 1.35)
        a_env = (1.0 - t) ** 1.5
        for y in range(TAIL_H):
            d = abs(y - cy)
            if r <= 0.0 or d > r:
                continue
            # 横向羽化 + 纵向包络
            edge = 1.0 - (d / r) ** 2.2
            a = int(210.0 * a_env * edge)
            if a <= 0:
                continue
            # 头端偏白、尾端**明显的蓝**（原版动态光照拖出来的是由白转蓝的渐层；
            # 第一版整条太白，owner 要的是"有蓝色" ⇒ 尾端压到 R70/G140）
            k = 1.0 - t
            col = (int(70 + 145 * k), int(140 + 95 * k), 255)
            px[x, y] = col + (min(a, 255),)
    img.save(OUT_TAIL)
    print(f"STAR_TAIL: {OUT_TAIL} {img.width}x{img.height}（头端在右、尾尖在左）")


def main():
    star = Image.open(SRC).convert("RGBA")
    w, h = star.width + PAD * 2, star.height + PAD * 2
    glow = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    d = ImageDraw.Draw(glow)
    cx, cy = w / 2.0, h / 2.0
    rmax = min(cx, cy)
    for i in range(RINGS, 0, -1):                 # 从外往内铺同心圆，越里面越亮
        r = rmax * i / RINGS
        a = int(GLOW_MAX * (1.0 - i / float(RINGS)) ** 1.6)
        if a <= 0:
            continue
        d.ellipse([cx - r, cy - r, cx + r, cy + r], fill=GLOW + (a,))
    out = glow.copy()
    out.alpha_composite(star, (PAD, PAD))         # 星星压在最上面（不动它一个像素）
    out.save(OUT)
    print(f"STAR_SHOT_BLUE: {OUT} {out.width}x{out.height}（原图 {star.width}x{star.height} + 蓝辉光）")
    make_tail()


if __name__ == "__main__":
    main()
