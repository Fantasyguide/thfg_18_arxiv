#!/usr/bin/env python3
"""东尼的悲恸（id 86）的**卡面虚影**贴图：从 owner 给的卡面截图里裁出**主体卡长方形**再缩到场上尺寸。

    python3 assets/ability/tony/make_card_ghost.py

产物：`assets/ability/tony/_src/GRIEF_CARD_GHOST.png`（128×181），由 `entries/ORDER.txt` 的
`TONY_GRIEF_CARD_GHOST` 引进 `ability.anm`（脚本 393：触发时在 BOSS 身上浮一个淡入淡出的卡面虚影）。

★ owner 2026-10-03 两次强调：「这张图**左上角突起**了，真正的**视觉中心应当是主体卡长方形的中心**」——
源图是 Slay the Spire 卡面的截图，**左上那个能量球（"1"）画到了卡框外面**（实测突出去的部分
x ∈ [34,110]、y ∈ [23,150]，而主体卡框是 x ∈ [71,770]、y ∈ [53,1042]）。
⇒ 裁切**只取主体卡框**（700×990），把它当整张图 ⇒ 贴图的几何中心 = 卡面中心（否则按外接框摆会整体偏）。
（同一套口径也用在卡图 `assets/cards/_src/TONY_GRIEF_CARD.png` 上。）"""
import os
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = "/mnt/d/项目/幻想指南/18/th18_modkit/d3bd0a27cffc1e17610100340c90f603738de914.png"
OUT = os.path.join(HERE, "_src", "GRIEF_CARD_GHOST.png")

# 实测的主体卡框（源图坐标，含右/下边界；见上面 owner 口径）
BODY = (71, 53, 771, 1043)      # left, top, right, bottom（半开区间）
W = 128                         # 场上尺寸：128×181（≈ 卡的 0.707 长宽比）


def main():
    if not os.path.exists(SRC):
        raise SystemExit(f"源图不在：{SRC}（owner 给的卡面截图）")
    im = Image.open(SRC).convert("RGBA").crop(BODY)
    h = int(round(W * im.height / im.width))
    im = im.resize((W, h), Image.LANCZOS)
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    im.save(OUT)
    print(f"GRIEF_CARD_GHOST: {OUT} {im.width}x{im.height}（源裁切 {BODY[2]-BODY[0]}x{BODY[3]-BODY[1]} ⇒ ×{W/(BODY[2]-BODY[0]):.3f}）")


if __name__ == "__main__":
    main()
