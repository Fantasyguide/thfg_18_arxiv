#!/usr/bin/env python3
"""地灵殿（id 111）咲夜蓄力光圈的**外圈粗环**贴图（owner 口径：粗环 + 满蓄力才彩虹）。

    python3 make_flan_ring_art.py          # 出图 + 打印要 append 到 entries/ORDER.txt 的那几行

## 为什么是贴图（不是 drawPolyBorder）

owner 逐字：「**我不想要细环，我就想要粗环**…内圈来一个淡的白叠加就行，外圈是相对实一点的白环 or 色环。
**然后我想的是满充能才是彩虹变色，充能中是白的**」。
`drawPolyBorder` 的线宽在**局部空间**、不可控（试过，实跑里既细又飘）；贴图环的**粗细就是像素**，
而且整张图是**白**的 ⇒ 卡侧/脚本用 `color` 就能把它染成任意色相（满蓄力的彩虹就是这么来的）。

## 几何（可见尺寸 = 128 × VM scale，环外径 = 64 × scale）

| 项 | 值 |
| --- | --- |
| 画布 | 128×128（透明底）|
| 外径 / 内径 | **62 / 42 px** ⇒ 环带厚 **20 px = 外径的 32%**（粗环）|
| 颜色 | 纯白 `(255,255,255)`，环带内 alpha 255 |
| 边缘 | 1.5 px 抗锯齿（既看得到边界，又不生硬）|

▶ 卡侧契约：`scale = 可见半径 / 64`（外径那一圈正好落在清屏半径上）。
"""
import math
import os
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))

NAME = 'FLAN_RING'
SIZE = 128
R_OUT = 62.0
R_IN = 42.0
FEATHER = 1.5


def main():
    img = Image.new('RGBA', (SIZE, SIZE), (0, 0, 0, 0))
    px = img.load()
    c = (SIZE - 1) / 2.0
    for y in range(SIZE):
        for x in range(SIZE):
            d = math.hypot(x - c, y - c)
            # 环带内 = 255；两侧各 FEATHER px 线性过渡
            a = min((R_OUT - d) / FEATHER, (d - R_IN) / FEATHER, 1.0)
            if a <= 0.0:
                continue
            px[x, y] = (255, 255, 255, int(round(a * 255)))
    out = os.path.join(HERE, 'flan', '%s.png' % NAME)
    img.save(out)
    print('wrote', out, img.size)
    print('append to entries/ORDER.txt:')
    print('ATLAS ATLAS_FLAN_RING %dx%d' % (SIZE, SIZE))
    print('%-22s %-48s @0,0,%dx%d' % (NAME, 'ability/flan/%s.png' % NAME, SIZE, SIZE))


if __name__ == '__main__':
    main()
