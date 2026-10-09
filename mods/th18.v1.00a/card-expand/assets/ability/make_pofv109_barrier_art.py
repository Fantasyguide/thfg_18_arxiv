#!/usr/bin/env python3
"""卡 109 **罩子**（④）的两层贴图 —— 照 owner 描述烤的（**一手没有这两张图**）。
    python3 make_pofv109_barrier_art.py     # 出图 + 打印要抄进 entries/ORDER.txt 的两行
★★★ 必须写明的两句（别让人以为切了原图）
────────────────────────────────────────
1. **一手没有这两张贴图。** owner 2026-10-10 描述：「这个罩子一手大概是：**一个白色透明圆形**，
   外面是**黄色的"外发光"**」，并指定「**参考咲夜的固灵领域**」。
   一手 TH19 的罩子是 `PlayerInf+0x18` 的**对象**（`state 1/2/3`，anm VM `+0x1c`），
   它的**具体贴图/脚本未取**（详见 `HANDOFF-109-C1-C4.md` §1.4/§8.1）⇒ 本脚本是**照描述烤的**。
2. 为什么不用几何：内层本可用 `texCircle`(600)，但那需要**配一张"绕圆"的条带贴图**（见 §9.2）；
   外层"外发光"本仓也没有现成图。⇒ **直接烤两张径向渐变的圆图**（与 `make_pofv109_chg_mark_art.py`
   同款做法：照描述烤、幂等、1:1 原生尺寸）。
★ 尺寸：内层 96×96、外层 128×128（外层比内层大一圈 ⇒ 叠起来就是"外发光" ✓）。
"""
from pathlib import Path
from PIL import Image

HERE = Path(__file__).resolve().parent
OUT = HERE / "barrier"

DISC = 96      # 内层：白色半透明圆
GLOW = 128     # 外层：黄色外发光


def bake(size: int, rgb: tuple, inner: float, outer: float, a_max: int) -> Image.Image:
    """半径归一化到 1：inner 以内实心，inner→outer 平滑衰减到 0，outer 以外全透明。"""
    im = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    px = im.load()
    c = (size - 1) / 2.0
    for y in range(size):
        for x in range(size):
            dx, dy = (x - c) / c, (y - c) / c
            r = (dx * dx + dy * dy) ** 0.5
            if r >= outer:
                a = 0.0
            elif r <= inner:
                a = 1.0
            else:
                t = (r - inner) / (outer - inner)
                a = (1.0 - t) ** 2                    # 二次衰减 ⇒ 柔边
            if a > 0.0:
                al = int(round(a * a_max))
                px[x, y] = (rgb[0], rgb[1], rgb[2], al)
    return im


def main() -> int:
    OUT.mkdir(parents=True, exist_ok=True)
    arts = [
        # (ORDER 名, 文件名, 尺寸, RGB, 内实心, 外截止, 峰值 alpha)
        ("POFV109_BARRIER_DISC", "BARRIER_DISC.png", DISC, (255, 255, 255), 0.80, 1.00, 150),
        ("POFV109_BARRIER_GLOW", "BARRIER_GLOW.png", GLOW, (255, 230, 90),  0.75, 1.00, 170),
    ]
    for name, fn, size, rgb, inner, outer, amax in arts:
        bake(size, rgb, inner, outer, amax).save(OUT / fn)
        print(f"{name:<24} ability/barrier/{fn:<20} {size}x{size}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
