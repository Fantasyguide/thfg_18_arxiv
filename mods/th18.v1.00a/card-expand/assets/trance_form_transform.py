#!/usr/bin/env python3
"""把 TH13 的「普通 → 灵界」贴图变换**量出来**，用于给没有 TH13 版的自机造灵界形态。

## 这是什么、为什么需要它

113 的灵界形态（白剪影）复刻的是 TH13 的机制：进灵界时把自机的 24 帧换成另一套**灵界专用贴图**
（`pl00f.png` / `pl01d.png` / `pl02c.png`，见 `AUDIT.md` §DG-2）。三个机体（灵梦 / 魔理沙 / 早苗）
在 TH13 里都有这套成品，**直接切图**即可（1:1、零处理）。

**咲夜是 TH18 才有的自机**（TH13 的四个自机是 灵梦 / 魔理沙 / 早苗 / 妖梦）⇒ 她没有成品可切。
用户 2026-09-16 定案：**用她自己的 TH18 那 24 帧，施加从 TH13 量出来的同一个变换**
（= 本模块）。这条是 **❓自定**（原版没有这个人），登记在 `AUDIT.md` §DG-7 / `TUNING.md` §2.0g；
但**变换本身不是自定的** —— 它是从 TH13 三对原版图里按数据拟合出来的，本模块的 `--report`
会把拟合结果与**验证**数字打出来。

## 变换的模型（逐像素、只动颜色、不动 alpha）

RGB → HSV，三步：

1. **明度直方图规格化**：`V' = F_灵界⁻¹(F_普通(V))`（32 级查表）。
   TH13 的灵界图**没有暗部**：V 的十分位从普通态 `0 … 1.0` 抬到灵界态 `0.53 … 1.0`。
2. **饱和度直方图规格化**：`S' = G_灵界⁻¹(G_普通(S))`。
   灵界图是"亮而少彩"：S 均值 0.41 → 0.22、上限压到 0.50。
3. **色相整体位移** `Δh`：按帧位置配对上"两边都有彩（S>0.25）"的像素取**位移中位**（拟合得 −45°）
   —— 就是灵界那层偏紫的辉光（TH13 灵界的招牌色）。

前两步用**直方图规格化**而不是线性公式，理由：它是非参数、单调、且**能从原版图直接复现**的
（没有任何手工调参）；`--report` 里的验证数字就是它的成绩单。

## 验证（`--report` 每次都会重算并打印）

把变换施加到 TH13 **普通**图、再与 TH13 **灵界**图逐像素比（只算两边都不透明的像素）：

| 机体 | 恒等（什么都不做） | 施加本变换 | 降幅 |
| --- | --- | --- | --- |
| 灵梦 | MAE 69.7 | **20.5** | 70.6% |
| 魔理沙 | MAE 88.4 | **25.5** | 71.2% |
| 早苗 | MAE 82.5 | **26.5** | 67.9% |

残差 ~20 的来源是**清楚的、且不打算补**：TH13 的灵界图是**另一张画**（多一圈辉光：不透明像素
+21…28%，轮廓 IoU 0.80），逐像素变色做不出那圈辉光 ⇒ 我们只复刻"色调变换"这一半，并在
`AUDIT.md` §DG-7 里明写这条差异。

用法：
    python3 assets/trance_form_transform.py --report      # 打印拟合表 + 验证数字
    （构建期：`assets/build_player_anm.py` 里 `transform="trance_form"` 的 entry 会调 `apply()`）
"""
import argparse
import colorsys
import statistics as st
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import anmlib as L  # noqa: E402

TH13_ANM = L.REPO / "local" / "th13.v1.00c" / "anm"

# 24 帧的矩形网格 = TH13 与 TH18 **共同**的自机帧布局（两作同尺寸 30×46、同 32 px 步进；
# 实测 TH18 的自机图就是 TH13 那套：逐帧轮廓 IoU 灵梦 0.947 / 魔理沙 0.959 / **早苗 1.000**）。
RECTS = [(1 + 32 * c, (1, 49, 97)[r], 30, 46) for r in range(3) for c in range(8)]

# 拟合用的三对原版图（TH13）：(普通, 灵界)。**这是变换的唯一依据，别改。**
PAIRS = (
    ("灵梦", "pl00/player/pl00/pl00.png", "pl00/player/pl00/pl00f.png"),
    ("魔理沙", "pl01/player/pl01/pl01.png", "pl01/player/pl01/pl01d.png"),
    ("早苗", "pl02/player/pl02/pl02.png", "pl02/player/pl02/pl02c.png"),
)
NBINS = 32          # 直方图规格化的级数（够细；且比逐像素拟合稳）
HUE_S_MIN = 0.25    # 色相位移只统计"两边都有彩"的像素


def _open(rel: str):
    from PIL import Image
    p = TH13_ANM / rel
    if not p.is_file():
        raise SystemExit(f"缺 TH13 原版贴图 {p}\n（先 python3 tooling/thtk/unpack.py th13.v1.00c）")
    return Image.open(p).convert("RGBA")


def _px(img):
    """→ 若干 (r,g,b,a) 元组（只取 RECTS 那 24 格）。用 `tobytes()` 而不是 `getdata()`：
    Pillow 14 起 `getdata()` 会弃用，仓库别的生成器已经踩过这条告警。"""
    out = []
    for (x, y, w, h) in RECTS:
        raw = img.crop((x, y, x + w, y + h)).tobytes()
        out += [tuple(raw[i:i + 4]) for i in range(0, len(raw), 4)]
    return out


def _hsv_pixels(img):
    return [colorsys.rgb_to_hsv(p[0] / 255, p[1] / 255, p[2] / 255)
            for p in _px(img) if p[3] > 128]


def _qmap(src, dst, nbins=NBINS):
    """直方图规格化表：`src` 的第 k 个分位 → `dst` 的第 k 个分位。"""
    ss, dd = sorted(src), sorted(dst)

    def at(frac, seq):
        return seq[min(len(seq) - 1, int(frac * len(seq)))]

    return [at(i / (nbins - 1), dd) for i in range(nbins)]


def _apply_q(tab, v):
    if v <= 0.0:
        return tab[0]
    if v >= 1.0:
        return tab[-1]
    x = v * (len(tab) - 1)
    i = int(x)
    f = x - i
    return tab[i] * (1 - f) + tab[min(i + 1, len(tab) - 1)] * f


def derive():
    """→ dict(v_tab, s_tab, hue_deg, pairs_used)。确定性：同一批原版图 ⇒ 同一张表。"""
    hn, hf = [], []
    for _name, n, f in PAIRS:
        hn += _hsv_pixels(_open(n))
        hf += _hsv_pixels(_open(f))
    shifts = []
    for _name, n, f in PAIRS:
        for pa, pb in zip(_px(_open(n)), _px(_open(f))):
            if pa[3] > 128 and pb[3] > 128:
                ha, sa, _ = colorsys.rgb_to_hsv(*[v / 255 for v in pa[:3]])
                hb, sb, _ = colorsys.rgb_to_hsv(*[v / 255 for v in pb[:3]])
                if sa > HUE_S_MIN and sb > HUE_S_MIN:
                    shifts.append(((hb - ha) * 360 + 180) % 360 - 180)
    return dict(v_tab=_qmap([a[2] for a in hn], [a[2] for a in hf]),
                s_tab=_qmap([a[1] for a in hn], [a[1] for a in hf]),
                hue_deg=st.median(shifts) if shifts else 0.0,
                pairs_used=len(PAIRS), hue_px=len(shifts))


_CACHE = {}


def transform():
    if "t" not in _CACHE:
        _CACHE["t"] = derive()
    return _CACHE["t"]


def apply(img):
    """把「普通 → 灵界」变换施加到一张 RGBA 图上（**alpha 一个字节都不动**）。"""
    from PIL import Image
    t = transform()
    img = img.convert("RGBA")
    out = Image.new("RGBA", img.size, (0, 0, 0, 0))
    src, dst = img.load(), out.load()
    dh = t["hue_deg"] / 360.0
    for y in range(img.size[1]):
        for x in range(img.size[0]):
            r, g, b, a = src[x, y]
            if a == 0:
                continue                                   # 全透明像素保持全透明（不引入色晕）
            h, s, v = colorsys.rgb_to_hsv(r / 255, g / 255, b / 255)
            h = (h + dh) % 1.0
            s = _apply_q(t["s_tab"], s)
            v = _apply_q(t["v_tab"], v)
            rr, gg, bb = colorsys.hsv_to_rgb(h, s, v)
            dst[x, y] = (int(rr * 255 + 0.5), int(gg * 255 + 0.5), int(bb * 255 + 0.5), a)
    return out


def _mae(a, b):
    tot = n = 0
    for pa, pb in zip(_px(a), _px(b)):
        if pa[3] > 128 and pb[3] > 128:
            tot += abs(pa[0] - pb[0]) + abs(pa[1] - pb[1]) + abs(pa[2] - pb[2])
            n += 3
    return tot / n if n else 0.0


def report():
    t = transform()
    print(f"拟合依据：TH13 的 {t['pairs_used']} 对原版图（{', '.join(p[0] for p in PAIRS)}）")
    print(f"  V 表（{NBINS} 级）：" + " ".join(f"{v:.2f}" for v in t["v_tab"]))
    print(f"  S 表（{NBINS} 级）：" + " ".join(f"{v:.2f}" for v in t["s_tab"]))
    print(f"  色相位移 Δh = {t['hue_deg']:.1f}°（{t['hue_px']} 个配对的有彩像素）")
    print("验证（把变换施加到 TH13 普通图，与真·灵界图逐像素比）：")
    for name, n, f in PAIRS:
        A, B = _open(n), _open(f)
        T = apply(A)
        i0, i1 = _mae(A, B), _mae(T, B)
        print(f"  {name:4s} 恒等 MAE {i0:6.1f} → 变换后 {i1:6.1f}（降低 {(1 - i1 / i0) * 100:4.1f}%）")
    print("  ⚠ 残差来自「TH13 的灵界图是另一张画、多一圈辉光」（不透明像素 +21…28%、轮廓 IoU 0.80）；"
          "本变换只复刻色调那一半，差异已登记 AUDIT §DG-7。")


if __name__ == "__main__":
    ap = argparse.ArgumentParser(description="TH13 灵界形态的色调变换（拟合 + 验证）")
    ap.add_argument("--report", action="store_true", help="打印拟合表与验证数字")
    a = ap.parse_args()
    if a.report:
        report()
    else:
        ap.print_help()
