#!/usr/bin/env python3
"""星星炮（id 89）**弹幕贴图**：给原版星星加一圈**蓝色辉光**。

    python3 assets/ability/star_cannon/make_star_art.py

产物：`assets/ability/star_cannon/_src/STAR_SHOT_BLUE.png`，由 `entries/ORDER.txt` 的
`STAR_CANNON_SHOT` 那一行引进 `ability.anm`。

为什么加蓝：owner 2026-10-02 实测「星星周围应当有蓝色」。原版的蓝**不在贴图里** ——
`Projectile.cs` 给这颗星 `light = 1f`（`SetDefaults` type 955），Terraria 的**动态光照**
在星周围铺一圈冷色辉光；我们的引擎没有光照通道 ⇒ 只能把这层辉光**烘进贴图**（= ❓ 自绘近似，
不是逐像素复刻）。原图 22×24（纯黄/棕，一个蓝像素都没有），这里先铺径向蓝辉光再压上星星。"""
import os
from PIL import Image, ImageDraw

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.join(HERE, "_src", "Projectile_955.png")
OUT = os.path.join(HERE, "_src", "STAR_SHOT_BLUE.png")

PAD = 10                    # 四周留出辉光的位置（原图 22×24 → 42×44）
GLOW = (104, 176, 255)      # 辉光色（冷蓝，取 Terraria 星光那种偏青的蓝）
GLOW_MAX = 215              # 辉光中心的最大 alpha（别盖过星星本身）
RINGS = 30


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


if __name__ == "__main__":
    main()
