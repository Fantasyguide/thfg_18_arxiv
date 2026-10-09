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


def _falloff(a: float) -> float:
    return a * a                    # 二次柔边


def bake_disc(size: int, rgb, inner: float, a_max: int):
    """★ **实心半透明圆**：0→inner 满 alpha，inner→1.0 二次衰减到 0（= 白色透明圆的柔边 ✓）。"""
    im = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    px = im.load()
    c = (size - 1) / 2.0
    for y in range(size):
        for x in range(size):
            r = (((x - c) / c) ** 2 + ((y - c) / c) ** 2) ** 0.5
            if r >= 1.0:
                a = 0.0
            elif r <= inner:
                a = 1.0
            else:
                a = _falloff(1.0 - (r - inner) / (1.0 - inner))
            if a > 0.0:
                px[x, y] = (rgb[0], rgb[1], rgb[2], int(round(a * a_max)))
    return im


def bake_ring(size: int, rgb, r_in: float, r_mid: float, a_max: int):
    """★ **空心带**（外发光 ✓）：r < r_in 全透明（**中心是空的** ✗ 不是实心圆 ✓），
       r_in→r_mid 升到满，r_mid→1.0 二次衰减到 0。"""
    im = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    px = im.load()
    c = (size - 1) / 2.0
    for y in range(size):
        for x in range(size):
            r = (((x - c) / c) ** 2 + ((y - c) / c) ** 2) ** 0.5
            if r <= r_in or r >= 1.0:
                a = 0.0
            elif r <= r_mid:
                a = _falloff((r - r_in) / (r_mid - r_in))
            else:
                a = _falloff(1.0 - (r - r_mid) / (1.0 - r_mid))
            if a > 0.0:
                px[x, y] = (rgb[0], rgb[1], rgb[2], int(round(a * a_max)))
    return im


def main() -> int:
    OUT.mkdir(parents=True, exist_ok=True)
    # ★ 内层：**白色、半透明**的实心圆（owner：「一个白色透明圆形」✓）
    #   ★ 峰值 alpha 120（❓旋钮：普通 alpha 混合 ⇒ 120/255 ≈ 47% 透白 ✓）
    bake_disc(DISC, (255, 255, 255), 0.80, 120).save(OUT / "BARRIER_DISC.png")
    print(f"{'POFV109_BARRIER_DISC':<24} ability/barrier/BARRIER_DISC.png {DISC}x{DISC}")
    # ★ 外层：**黄色外发光**（owner）⇒ ★ **空心带** ✓：0.70→0.85 升满、→1.0 衰减 ✓
    #   ★ 峰值 alpha 130（❓旋钮）—— ★ 只在外缘一圈，**中心是空的** ✗ 不再是实心大圆 ✓
    bake_ring(GLOW, (255, 230, 90), 0.70, 0.85, 130).save(OUT / "BARRIER_GLOW.png")
    print(f"{'POFV109_BARRIER_GLOW':<24} ability/barrier/BARRIER_GLOW.png {GLOW}x{GLOW}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
