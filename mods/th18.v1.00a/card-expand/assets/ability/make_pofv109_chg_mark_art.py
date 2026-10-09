#!/usr/bin/env python3
"""卡 109 蓄力槽的 **C1..C4 段位标（底色菱形）+ 越过光晕** —— 照一手几何形状烤的两组小贴图。

    python3 make_pofv109_chg_mark_art.py       # 出图 + 打印要抄进 entries/ORDER.txt 的 8 行

★★★ 必须写明的两句（别让人以为切了原图）
────────────────────────────────────────
1. **一手没有这两张贴图。** 一手 = TH19 `aura.anm` **script5..12** 的 **`ins_617(1.0f, 4)` 几何**
   （一个 4 边形的**程序图元**，8 支的**首行全是 `sprite(-1)`** ⇒ 不绑任何贴图）。
   ⇒ 本脚本是**照一手的几何比例/配色烤出来的**，**不是**从任何一手贴图里切出来的。
2. 为什么不用几何：TH18 的 anmm **止于 613**、没有 `ins_617`；本仓的 604 `drawPoly`
   **只吃一路半径**（`th16:0x469e00` 每顶点只乘一个 size，**没有径向渐变**）⇒ 画不出
   一手的「白 `color` 圆心 + 段位色 `color2` 圆周」；且 `drawPoly` 在 **HUD 层**
   （`layer(25)` + `resolutionMode(2)`）本仓**无先例**（❓E5，已结案）。
   ⇒ owner 2026-10-11 裁决「**菱形上贴图**」+「**E4 光晕也可以贴图吧**」⇒ 改烤贴图。

配色（一手 `aura.anm` script5/7/9/11 的 `color2`，**4 组段位色**）
────────────────────────────────────────────────────────────
| 索引 | 色 | RGB | 一手出处 |
| :-: | --- | --- | --- |
| 0 | 浅蓝 | `(128,128,255)` | `aura.anm` script5 ✓ |
| 1 | 紫 | `(128,0,255)` | script7 ✓ |
| 2 | 红 | `(255,0,0)` | script9 ✓ |
| 3 | 黄 | `(255,255,0)` | script11 ✓ |

★★ 尺寸（2026-10-11 第二轮：**一手尺寸优先**，见 `TUNING.md` 本轮追加节）
────────────────────────────────────────────────────────────────────
一手那两支的**轴不对称**（本仓 2026-10-11 核完）：脚本 `scale(a, b)` 给的是
**纵半轴 = `a`**、脚本 `scale2(a, b)` 给的是 **横半轴 = `a`**；落到 1280×960 表面时
**只有纵轴乘窗口倍率 ×2**（`scale2.x` **不乘**）⇒ 表面 宽 = `2 × scale2.x`、高 = `2 × scale.x × 2`：

| 一手标签 | 一手数值（`aura.anm` script5） | 表面尺寸 | 本贴图 / 卡侧 |
| --- | --- | --- | --- |
| `interruptLabel(4)` **常显** | `scale(6,6); scale2(2,2)` | **4 × 24** | 底色 `CHG_MARK_*` = **4×24**，卡侧 `scale` 恒 1.0 ✓ |
| `interruptLabel(3)` **未达但可达** | `scale(6,0.2); scale2(0.2,0.2)` | **≈0.4 × 24**（细针） | 同一张底色贴图，卡侧只把 `scale.x` 压到 **0.1** ⇒ 4×0.1 = **0.4** ✓ |
| `interruptLabel(2)` **已达（脉冲）** | `scale.x 9→9→10` / `scale2.x 9→1.8→2.0` | 宽 3.6→4→18、高 36→40，**最大 ≈18 × 36** | 光晕 `CHG_GLOW_*` = **24×48**，脚本 426 在 `0.15 ↔ 0.75` 之间呼吸 ⇒ 3.6×36 ↔ **18×36** ✓ |

* **底色菱形 4×24 是**整张贴图就一个菱形**（拉长的竖菱形，宽:高 = 1:6 ✓）——
  卡侧 `scale` = 1.0 ⇒ 逐 texel 对上（脚本 425 的 `resampleMode(1)` = 最近邻 ✓）。
  ⚠ 没有烤「更宽的高分辨率版本再缩回来」：4 px 宽在**形状归一化**的渐变下已经能画出
  「2 px 白芯 + 两侧 1 px 段位色边」（中心行两枚内像素 `u = 0.25 + 0.5/12` ⇒ 白核；
  外侧两枚 `u = 0.75 + …` ⇒ 0.69 的段位色）；再烤 8×48 让卡侧缩 0.5 只会和
  `resampleMode(1)` 打架（丢一半 texel）⇒ **按一手尺寸直烤**。
* **光晕 24×48** 比底色大（宽高比同为 **1:2**），卡侧/脚本按档位缩放：**最大 18×36** = 24×0.75 / 48×0.75 ✓；
  边缘径向淡出 ⇒ 放大时不会出现硬边。

渐变（**形状归一化的径向渐变**：`u = |dx|/横半轴 + |dy|/纵半轴 ∈ [0,1]`）
────────────────────────────────────────────────────────────────────────
* **底色**：`RGB = lerp(白, 段位色, t)`、`t = clamp((u − 1/3)/(1 − 1/3))` ⇒ **白芯 + 段位色边**
  （一手 `color` 白 / `color2` 段位色的两路顶点色 ✓；`A = 边缘覆盖 × 255`，一手 `alpha(255)` ✓）。
  ★ **订正**：上一轮的 `CORE = HALF/3` 是从「`scale2(2.0)` = 白芯半径」推出来的 —— **那条推断有误**
  （一手 `scale2.x` = **横向半轴本身**，不是内半径；见 `TUNING.md` 本轮 §⑤）。
  ⇒ 白芯占比 **1/3** 现在是 **❓ 自定**（沿用观感），不再是「一手推出的」。
* **光晕**：一手偶数支 `blendMode(1)` 加算 + `alpha(255); alpha2(0);` —— ⚠ `alpha2` 写的是
  **`color_2`**，而 TH18 的 `colorMode` 默认 **0** ⇒ 只读 `color_1`
  （`engine/anm/th18/07-blendmode-rgb-only-alpha-is-sa.md` 的订正节 ✓）⇒ 一手那双 alpha 其实
  **只有一路生效**（= 实心加算菱形）。本脚本按 owner 的「放大淡出好看的那版」烤成**径向淡出**：
  `RGB = 段位色`、`A = (1 − u)² / (1 − u_min)² × 覆盖 × 255`（中心最亮、边界 0）。**❓ 自定**，已登记 `TUNING.md`。
  ⚠ 加算 = `SRCALPHA / ONE`（同文档 §5 mode 1）⇒ 贡献 = `RGB × A` ⇒ **A 的淡出就是"边缘更淡"** ✓。

盖色策略（为什么是 **8 张** 而不是「1 张白 + 卡侧染色」）
──────────────────────────────────────────────────────
* 底色菱形**有白芯** ⇒ 卡侧**不能**染色（会把白芯一起染掉 ✗，owner 2026-10-11 点名）；
* 光晕虽然没有白芯、染色可行，但它和标是**同一支 VM 的两态**、且卡侧**逐帧**写两路顶点色
  ⇒ 让两支**同构**（都写白 `(255,255,255,255)`、颜色全部烘在贴图里）最不容易出错，
  也不依赖「脚本的 `color()` 与卡侧 `ce_anm_set_color()` 谁先执行」这条模糊的时序。
  ⇒ **每色一套（4 标 + 4 光晕 = 8 张）**，与 owner 的「各一张贴图，每色一套」一致 ✓。

幂等：纯确定性绘图（**无随机、不联网、不读任何样本/原图**），可重复运行，输出逐字节一致。
产物：`ability/chgmark/CHG_{MARK,GLOW}_{BLUE,PURPLE,RED,YELLOW}.png`
      （`MARK` 各 **4×24**、`GLOW` 各 **24×48**）。
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

# ★ 两种形状的**表面 px**（= 贴图矩形；卡侧 `scale` 乘的就是矩形 ⇒ 1.0 = 原尺寸 ✓）
MARK_W, MARK_H = 4, 24          # 常显态：一手 label4（`scale(6,6)` + `scale2(2,2)` ⇒ 4×24 表面 px ✓）
GLOW_W, GLOW_H = 24, 48         # 光晕：比底色大，宽高比 1:2（脚本 426 缩到最大 18×36 = ×0.75 ✓）

MARK_HALF_W, MARK_HALF_H = MARK_W / 2.0, MARK_H / 2.0     # 2.0 / 12.0
GLOW_HALF_W, GLOW_HALF_H = GLOW_W / 2.0, GLOW_H / 2.0     # 12.0 / 24.0

CORE_U = 1.0 / 3.0              # ❓ **自定**：白芯占「形状半径」的比例（订正后不再由 `scale2` 推；见文件头）
SS = 8                          # 每像素 8×8 超采样（4 px 宽的形状也要平滑的覆盖/抗锯齿；纯确定性）

# 加算光晕的归一化：贴图**最亮的那枚像素**离中心 `u_min` ⇒ 不归一化的话中心只有 `(1−u_min)²` 亮。
GLOW_PEAK = 1.0 / ((1.0 - (0.5 / GLOW_HALF_W + 0.5 / GLOW_HALF_H)) ** 2)


def _lerp(a, b, t):
    return a + (b - a) * t


def _clamp01(v):
    return 0.0 if v < 0.0 else (1.0 if v > 1.0 else v)


def _u(fx, fy, hw, hh):
    """形状归一化的径向参数（菱形 L1 距离 ÷ 该方向的半轴）⇒ 边界恒为 1。"""
    return abs(fx) / hw + abs(fy) / hh


def mark_pixel(fx, fy, cov, seg):
    """底色菱形：白芯 → 段位色边（一手 `color` 白 / `color2` 段位色 的两路顶点色插值 ✓）。"""
    t = _clamp01((_u(fx, fy, MARK_HALF_W, MARK_HALF_H) - CORE_U) / (1.0 - CORE_U))
    rgb = tuple(int(round(_lerp(255.0, float(c), t))) for c in seg)
    return rgb + (int(round(cov * 255.0)),)


def glow_pixel(fx, fy, cov, seg):
    """加算光晕：RGB = 段位色、A = 径向淡出（`(1−u)²` 归一化到中心 255；❓ 自定，见文件头）。"""
    u = _clamp01(_u(fx, fy, GLOW_HALF_W, GLOW_HALF_H))
    a = min(1.0, ((1.0 - u) ** 2) * GLOW_PEAK)
    return seg + (int(round(a * cov * 255.0)),)


def bake_one(shape, seg, w, h, hw, hh):
    """烤一张 `w`×`h`（RGBA）：`shape` = mark_pixel / glow_pixel、`seg` = 该色段位色。"""
    im = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    px = im.load()
    for y in range(h):
        for x in range(w):
            hits = 0
            for sy in range(SS):
                for sx in range(SS):
                    fx = x + (sx + 0.5) / SS - hw       # 相对贴图中心（半轴坐标系 ✓）
                    fy = y + (sy + 0.5) / SS - hh
                    if abs(fx) / hw + abs(fy) / hh <= 1.0:   # 菱形（L1 球，按半轴归一）内部 ✓
                        hits += 1
            cov = hits / float(SS * SS)
            if cov <= 0.0:
                continue                                  # 菱形外：完全透明（保持 0,0,0,0 ✓）
            fx = x + 0.5 - hw
            fy = y + 0.5 - hh
            px[x, y] = shape(fx, fy, cov, seg)
    return im


# 形状 → (文件名前缀、宽度、高度、半宽、半高、烤图函数、说明)
SHAPES = [
    ("MARK", MARK_W, MARK_H, MARK_HALF_W, MARK_HALF_H, mark_pixel, "白芯 + 色边"),
    ("GLOW", GLOW_W, GLOW_H, GLOW_HALF_W, GLOW_HALF_H, glow_pixel, "径向淡出，加算用"),
]


def main():
    OUT.mkdir(exist_ok=True)
    lines = []
    for name, seg, src in TIERS:
        for kind, w, h, hw, hh, shape, note in SHAPES:
            fn = f"CHG_{kind}_{name}.png"
            bake_one(shape, seg, w, h, hw, hh).save(OUT / fn)
            member = f"POFV109_CHG_{kind}_{name}"
            lines.append((member, f"ability/chgmark/{fn}", f"{w}x{h}"))
            print(f"# {member:<28} {fn:<20} {w}x{h:<4} RGB{seg}  ← {src}（{note}）")
    print("# 下面是 `entries/ORDER.txt` **末尾那 8 行**应有的尺寸（只改这张表的 `WxH` 字段）：")
    for member, rel, size in lines:
        print(f"{member:<28} {rel:<44} {size}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
