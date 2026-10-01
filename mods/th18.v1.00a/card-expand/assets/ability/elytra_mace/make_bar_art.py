#!/usr/bin/env python3
"""鞘翅重锤（id 91）的 **Undertale 式时机条** 贴图（条 + 光标）。

    python3 assets/ability/elytra_mace/make_bar_art.py

产物（都进 `ability.anm`，见 `entries/ORDER.txt`）：
· `_src/BAR.png`    160×14 —— 轨道 + **中间那条亮带 = 判定窗口**（与 C 侧的 `EM_WINDOW` 同宽：
                    窗口 ±15 px / 条半宽 80 px ⇒ 亮带 30 px 居中）
· `_src/CURSOR.png` 6×18  —— 来回扫的光标

样式口径（Undertale 的战斗条 + 本仓 HUD 的观感）都是 ❓ 自定美术：轨道半透黑、亮带暖黄、
光标纯白带黑描边（在深色弹幕上也看得清）。owner 若要换皮，改本文件的常量重烘即可。"""
import os
from PIL import Image, ImageDraw

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "_src")

BAR_W, BAR_H = 160, 14
CUR_W, CUR_H = 6, 18
WINDOW_HALF = 15          # = EM_WINDOW（px）；条半宽 80 ⇒ 亮带 30 px

TRACK      = (14, 14, 18, 190)     # 轨道（半透黑）
TRACK_EDGE = (208, 208, 216, 235)  # 轨道描边（浅灰）
ZONE       = (250, 214, 92, 235)   # 判定窗口（暖黄，Undertale 那一条的观感）
ZONE_EDGE  = (255, 246, 200, 255)
TICK       = (120, 120, 132, 150)  # 等分刻度
CURSOR     = (255, 255, 255, 255)
CURSOR_EDGE = (0, 0, 0, 255)


def main():
    bar = Image.new("RGBA", (BAR_W, BAR_H), (0, 0, 0, 0))
    d = ImageDraw.Draw(bar)
    d.rectangle([0, 0, BAR_W - 1, BAR_H - 1], fill=TRACK, outline=TRACK_EDGE)
    cx = BAR_W // 2
    d.rectangle([cx - WINDOW_HALF, 1, cx + WINDOW_HALF - 1, BAR_H - 2], fill=ZONE, outline=ZONE_EDGE)
    for k in range(1, 8):                      # 等分刻度（8 格），窗口那一段跳过
        x = k * BAR_W // 8
        if abs(x - cx) <= WINDOW_HALF:
            continue
        d.line([x, 3, x, BAR_H - 4], fill=TICK)

    cur = Image.new("RGBA", (CUR_W, CUR_H), (0, 0, 0, 0))
    dc = ImageDraw.Draw(cur)
    dc.rectangle([0, 0, CUR_W - 1, CUR_H - 1], fill=CURSOR, outline=CURSOR_EDGE)

    bar.save(os.path.join(OUT, "BAR.png"))
    cur.save(os.path.join(OUT, "CURSOR.png"))
    print(f"BAR: {BAR_W}x{BAR_H} · CURSOR: {CUR_W}x{CUR_H}（判定窗口 ±{WINDOW_HALF} px）")


if __name__ == "__main__":
    main()
