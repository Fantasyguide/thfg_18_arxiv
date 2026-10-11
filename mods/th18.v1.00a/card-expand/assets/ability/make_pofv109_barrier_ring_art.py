#!/usr/bin/env python3
"""烤 109 罩子的黄环贴图（owner 2026-10-11：「我十分建议你烤成贴图」）。

为什么要烤：一手 `615` = **mode 29**（= `611 drawRing` 的**半径偏移版**，一侧半径拉开「另一半÷2」，
内外半径不对称、观感"外实内虚"），而 **v8(TH18) 只有 mode 19**（map 止于 613、v8 模式集无 29/30）
⇒ 那个"±拉开"在 TH18 **复刻不出来** ⇒ 直接**烤**下来 ✓。

画法（尽量贴一手观感）：
- 画布 72×72（中心 36,36）；环带 **半径 28–36**（内缘 28 = 白圆半径 ⇒ 相切不重叠 ✓）；
- 沿半径把 alpha 从**外缘实**（≈200）渐到**内缘虚**（≈40）⇒ 近似 mode 29 的"一侧拉开、内侧发虚" ✓；
- 在 28–36 带内再叠一层浅黄渐变，让内缘更"软" ✓。
"""
from PIL import Image
import pathlib
# ★★ 2026-10-11 owner：「如果混合模式没有区别的话，我甚至建议你**整个罩子拷贝成贴图**」
#   ⇒ 白心 + 黄环**一起烤**成一张（一个 VM 画完 ✓，也就不存在"两环叠错/重叠"的问题 ✓）。
SIZE, CTR = 72, 36.0
R_DISC = 28.0                 # 白心半径（一手 604(28,24) ✓）
R_IN, R_OUT = 28.0, 36.0      # 黄环带（内缘 28 = 白圆边 ⇒ 相切不重叠 ✓）
im = Image.new("RGBA", (SIZE, SIZE), (0, 0, 0, 0))
px = im.load()
for y in range(SIZE):
    for x in range(SIZE):
        dx, dy = x + 0.5 - CTR, y + 0.5 - CTR
        r = (dx * dx + dy * dy) ** 0.5
        if r <= R_DISC:                       # 白心：平涂 alpha 64（一手 ins_405(64) ✓）
            px[x, y] = (255, 255, 255, 64)
            continue
        if r < R_IN or r > R_OUT:
            continue
        t = (r - R_IN) / (R_OUT - R_IN)          # 0 = 内缘, 1 = 外缘
        a = int(40 + (200 - 40) * (t * t))       # 外实内虚
        g = int(200 + 55 * t)                    # 外缘偏纯黄、内缘略白
        px[x, y] = (255, min(255, g), int(40 + 60 * (1 - t)), a)
out = pathlib.Path(__file__).with_name("POFV109_BARRIER_FULL.png")
im.save(out)
print("已烤:", out.name, im.size, "非透明", sum(1 for p in im.getdata() if p[3] > 0))
