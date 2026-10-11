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
        # ★★★ 2026-10-11 owner：★ 一手那条"渐弱"是 **`alpha`/`alpha2` 端点插值**（脚本用
        #   `ins_300(-1)` = 素色系统贴图 ⇒ 渐变**不是**贴图给的 ✓）；★ 而 **TH18 环的渐变 α 不生效** ✗
        #   ⇒ ★ **把 α 按一手的端点剖面烤进贴图** ✓（owner 口径 ✓）。
        #   一手配方（`effect.anm` ✓）：白心 `alpha(0)` → `alpha2(64)`（中心透 → 边缘 ≈0.25 ✓）；
        #   黄环 `alpha(128)` → `alpha2(0)`（内缘"突然黄" → 向外渐弱 ✓）。
        if r <= R_DISC:                       # 白心：中心 α=0 → 边缘 α=64（线性 ✓）
            u = r / R_DISC                     # 0 = 中心, 1 = 边缘
            a = int(round(64.0 * u))
            # 颜色也从 (255,255,255) 走到 (224,224,255)（一手 rgb → rgb2 ✓）
            b = int(round(255.0 - (255.0 - 255.0) * u))
            g = int(round(255.0 - (255.0 - 224.0) * u))
            px[x, y] = (255, g, b, a)
            continue
        if r < R_IN or r > R_OUT:
            continue
        u = (r - R_IN) / (R_OUT - R_IN)        # 0 = 环内缘, 1 = 环外缘
        a = int(round(128.0 * (1.0 - u)))      # ★ 内缘 128（"突然黄"）→ 外缘 0（渐弱 ✓）
        px[x, y] = (255, 255, 0, a)
out = pathlib.Path(__file__).with_name("POFV109_BARRIER_FULL.png")
im.save(out)
print("已烤:", out.name, im.size, "非透明", sum(1 for p in im.getdata() if p[3] > 0))
