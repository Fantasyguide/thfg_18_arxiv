#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""伟大之作（卡 92）「全部发亮」特效的**径向亮度渐变**贴图：**内白外黑**、alpha 恒 255。

owner 2026-10-03 给的 AE 配方：「**加和模式**，然后**中心渐变**，**内白外黑**，先是**半径增大**，
再是**黑色变白**」。

★ 为什么 alpha 全 255、靠 **RGB** 承载渐变：加和模式（`blendMode(1)` = `SRCALPHA/ONE`）的合成为
  `dst + srcColor × srcAlpha` ⇒ **黑色（RGB 0）加进去等于没加** ✓ —— 这正是 AE 里"黑 = 不发光"的同一条逻辑。
  若改用 alpha 承载（RGB 恒白、alpha 衰减），边缘是"白×0"也等于没加，两者等价；但**"黑变白"这一步**
  只能在 **RGB** 上做（把黑色提亮），所以贴图必须用 RGB 承载渐变。

输出：`assets/ability/great_work/GREAT_WORK_GLOW.png`（128×128 RGBA：RGB = 中心白 → 边缘黑、A = 255）。
运行：`python3 assets/ability/great_work/make_glow_disc.py`（幂等）。
"""
import math
import os

from PIL import Image

SIZE = 128
CENTER_RGB = (255, 255, 255)     # 圆心：白（最亮）
EDGE_RGB = (0, 0, 0)             # 边缘：黑（加和不贡献 ⇒ 不发光）
FALLOFF = 1.6                    # 亮度 = (1 - r)^FALLOFF；1.6 = 中间还亮、靠边才暗下去

out = os.path.join(os.path.dirname(os.path.abspath(__file__)), "GREAT_WORK_GLOW.png")
img = Image.new("RGBA", (SIZE, SIZE))
px = img.load()
half = (SIZE - 1) / 2.0
for y in range(SIZE):
    for x in range(SIZE):
        dx = (x - half) / half
        dy = (y - half) / half
        r = math.hypot(dx, dy)
        if r > 1.0:
            px[x, y] = (0, 0, 0, 255)     # 圆外：纯黑 + 不透明（加和 ⇒ 不贡献）
            continue
        v = (1.0 - r) ** FALLOFF
        rgb = int(round(CENTER_RGB[0] * v + EDGE_RGB[0] * (1.0 - v)))
        px[x, y] = (rgb, rgb, rgb, 255)
img.save(out)
print("写出", out, img.size, "（RGB 白→黑、alpha 255）")
