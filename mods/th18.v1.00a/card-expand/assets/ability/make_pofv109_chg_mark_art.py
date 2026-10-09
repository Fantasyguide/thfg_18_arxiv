#!/usr/bin/env python3
"""卡 109 蓄力槽的 **C1..C4 段位标（底色菱形）+ 越过光晕** —— 照一手几何形状烤的两组 16×16 小贴图。

    python3 make_pofv109_chg_mark_art.py       # 出图 + 打印要追加进 entries/ORDER.txt 的 8 行

★★★ 必须写明的两句（别让人以为切了原图）
────────────────────────────────────────
1. **一手没有这两张贴图。** 一手 = TH19 `aura.anm` **script5..12** 的 **`ins_617(1.0f, 4)` 几何**
   （一个 4 边形的**程序图元**，8 支的**首行全是 `sprite(-1)`** ⇒ 不绑任何贴图）。
   ⇒ 本脚本是**照一手的几何比例/配色烤出来的**，**不是**从任何一手贴图里切出来的。
2. 为什么不用几何：TH18 的 anmm **止于 613**、没有 `ins_617`；本仓的 604 `drawPoly`
   **只吃一路半径**（`th16:0x469e00` 每顶点只乘一个 size，**没有径向渐变**）⇒ 画不出
   一手的「白 `color` 圆心 + 段位色 `color2` 圆周」；且 `drawPoly` 在 **HUD 层**
   （`layer(25)` + `resolutionMode(2)`）本仓**无先例**（❓E5）。
   ⇒ owner 2026-10-11 裁决「**菱形上贴图**」+「**E4 光晕也可以贴图吧**」⇒ 改烤贴图。

配色（一手 `aura.anm` script5/7/9/11 的 `color2`，**4 组段位色**）
────────────────────────────────────────────────────────────
| 索引 | 色 | RGB | 一手出处 |
| :-: | --- | --- | --- |
| 0 | 浅蓝 | `(128,128,255)` | `aura.anm` script5 ✓ |
| 1 | 紫 | `(128,0,255)` | script7 ✓ |
| 2 | 红 | `(255,0,0)` | script9 ✓ |
| 3 | 黄 | `(255,255,0)` | script11 ✓ |

两形状（一手数值 → 本贴图像素；贴图 = **16×16**，半对角 **8 px** = 卡侧 `POFV_HUD_CHG_DIA_R`）
────────────────────────────────────────────────────────────────────────────
* **底色菱形**（奇数支 5/7/9/11）：一手 `sprite(-1); ins_617(1.0f, 4); scale(6.0f, 6.0f);
  scale2(2.0f, 2.0f); color(255,255,255); alpha(255); color2(<段位色>);`
  ⇒ **白芯 + 段位色边**（内置半径 2 = 外半径 6 的 **1/3**）。
  本脚本：`d = |dx|+|dy|`（菱形 L1 距离）、`t = (d − 8/3) / (8 − 8/3)` 钳到 `0..1`
  ⇒ `RGB = lerp(白, 段位色, t)`；`A = 边缘覆盖 × 255`（一手 `alpha(255)` ⇒ **本体不透明** ✓）。
* **光晕菱形**（偶数支 6/8/10/12）：一手 `blendMode(1)` 加算 + `alpha(255); alpha2(0);`
  —— ⚠ `alpha2` 写的是 **`color_2`**，而 TH18 的 `colorMode` 默认 **0** ⇒ 只读 `color_1`
  （`engine/anm/th18/07-blendmode-rgb-only-alpha-is-sa.md` 的订正节 ✓）⇒ 一手那双 alpha 其实
  **只有一路生效**（= 实心加算菱形）。本脚本按 owner 的「放大淡出好看的那版」烤成**径向淡出**：
  `RGB = 段位色`、`A = (1 − d/8)² × 覆盖 × 255`（中心最亮、菱形边缘 0）。**❓ 自定**，已登记 `TUNING.md`。
  ⚠ 加算 = `SRCALPHA / ONE`（同文档 §5 mode 1）⇒ 贡献 = `RGB × A` ⇒ **A 的淡出就是"边缘更淡"** ✓。

盖色策略（为什么是 **8 张** 而不是「1 张白 + 卡侧染色」）
──────────────────────────────────────────────────────
* 底色菱形**有白芯** ⇒ 卡侧**不能**染色（会把白芯一起染掉 ✗，owner 2026-10-11 点名）；
* 光晕虽然没有白芯、染色可行，但它和标是**同一支 VM 的两态**、且卡侧**逐帧**写两路顶点色
  ⇒ 让两支**同构**（都写白 `(255,255,255,255)`、颜色全部烘在贴图里）最不容易出错，
  也不依赖「脚本的 `color()` 与卡侧 `ce_anm_set_color()` 谁先执行」这条模糊的时序。
  ⇒ **每色一套（4 标 + 4 光晕 = 8 张）**，与 owner 的「各一张贴图，每色一套」一致 ✓。

幂等：纯确定性绘图（**无随机、不联网、不读任何样本/原图**），可重复运行，输出逐字节一致。
产物：`ability/chgmark/CHG_{MARK,GLOW}_{BLUE,PURPLE,RED,YELLOW}.png`（各 **16×16**）。
"""
import sys
from pathlib import Path

from PIL import Image

HERE = Path(__file__).resolve().parent
OUT = HERE / "chgmark"

# 段位色：一手 `aura.anm` script5/7/9/11 的 `color2`（顺序 = 卡侧 POFV_HUD_CHG_TICKS 的索引 ✓）
TIERS = [
    ("BLUE", (128, 128, 255), "script5  浅蓝"),
    ("PURPLE", (128, 0, 255), "script7  紫"),
    ("RED", (255, 0, 0), "script9  红"),
    ("YELLOW", (255, 255, 0), "script11 黄"),
]
TEX = 16                     # 贴图边长（表面 px；半对角 = 8 = 卡侧 DIA_R ✓）
HALF = TEX / 2.0             # 8.0 = 一手 `scale(6.0f)` 的对应半径（本仓 ×2 空间里仍是可见半径 8 ✓）
CORE = HALF / 3.0            # 一手 `scale2(2)/scale(6)` = 1/3 ⇒ 白芯的内半径 ✓
GLOW_PEAK = 1.3              # 光晕中心归一化（见 glow_pixel 的说明 ✓）
SS = 4                       # 每像素 4×4 超采样（只为了 45° 边的抗锯齿；纯确定性）


def _lerp(a, b, t):
    return a + (b - a) * t


def mark_pixel(d, cov, seg):
    """底色菱形：白芯 → 段位色边（一手 `color` 白 / `color2` 段位色 的两路顶点色插值 ✓）。"""
    t = (d - CORE) / (HALF - CORE)
    t = 0.0 if t < 0.0 else (1.0 if t > 1.0 else t)
    rgb = tuple(int(round(_lerp(255.0, float(c), t))) for c in seg)
    return rgb + (int(round(cov * 255.0)),)


def glow_pixel(d, cov, seg):
    """加算光晕：RGB = 段位色、A = 径向淡出（`(1−d/8)²`；❓ 自定，见文件头）。

    `GLOW_PEAK`：菱形**正中心落在像素角上** ⇒ 最亮的那个像素中心离中心 **d = 1.0**
    ⇒ `(1−1/8)² = 0.766` ⇒ 不归一化的话加算光晕的中心只有 77% 亮。乘 1.3 钳回 255 ✓。"""
    t = d / HALF
    t = 0.0 if t < 0.0 else (1.0 if t > 1.0 else t)
    a = min(1.0, ((1.0 - t) ** 2) * GLOW_PEAK)
    return seg + (int(round(a * cov * 255.0)),)


def bake_one(shape, seg):
    """烤一张 16×16（RGBA）：`shape` = mark_pixel / glow_pixel、`seg` = 该色段位色。"""
    im = Image.new("RGBA", (TEX, TEX), (0, 0, 0, 0))
    px = im.load()
    for y in range(TEX):
        for x in range(TEX):
            hits = 0
            for sy in range(SS):
                for sx in range(SS):
                    fx = x + (sx + 0.5) / SS - HALF       # 相对贴图中心（半对角坐标系 ✓）
                    fy = y + (sy + 0.5) / SS - HALF
                    if abs(fx) + abs(fy) <= HALF:        # 菱形（L1 球）内部 ⇒ 计入覆盖 ✓
                        hits += 1
            cov = hits / float(SS * SS)
            if cov <= 0.0:
                continue                                  # 菱形外：完全透明（保持 0,0,0,0 ✓）
            fx = x + 0.5 - HALF
            fy = y + 0.5 - HALF
            px[x, y] = shape(abs(fx) + abs(fy), cov, seg)
    return im


def main():
    OUT.mkdir(exist_ok=True)
    lines = []
    for name, seg, src in TIERS:
        for kind, shape, prefix in (("MARK", mark_pixel, "CHG_MARK"), ("GLOW", glow_pixel, "CHG_GLOW")):
            fn = f"{prefix}_{name}.png"
            bake_one(shape, seg).save(OUT / fn)
            member = f"POFV109_{prefix}_{name}"
            lines.append((member, f"ability/chgmark/{fn}"))
            print(f"# {member:<28} {fn:<20} 16x16  RGB{seg}  ← {src}"
                  f"{'（白芯 + 色边）' if kind == 'MARK' else '（径向淡出，加算用）'}")
    print("# 下面是**追加到 entries/ORDER.txt 末尾**的 8 行（别动既有行）：")
    for member, rel in lines:
        print(f"{member:<28} {rel:<44} 16x16")
    return 0


if __name__ == "__main__":
    sys.exit(main())
