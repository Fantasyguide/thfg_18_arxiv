#!/usr/bin/env python3
"""灵界转场（113）的**离线合成 + 度量** —— 专门用来抓用户看见的"两个蓝色的矩形 / 神秘蓝色块"。

    python3 tools/measure_trance_transition.py                 # 新旧两条流水线各跑一次，打表
    python3 tools/measure_trance_transition.py --json out.json
    python3 tools/measure_trance_transition.py --pipeline new

## 为什么上一版的对账**没抓到**

第七轮的对账是"**角色美术的覆盖率**"：确认 4 台幕布扫过时"astral 被盖住的比例"符合预期。
它全绿，而用户仍然看到不透明色块 —— 因为那个量在**两种流水线里都会到 100%**：
"astral 被盖住"既能是"被**遮罩**（alpha 遮罩，图在下面透出来）"，也能是"被**一块颜色盖住**"。
⇒ 判据必须落在**被盖住的那部分长什么样**。

## 本工具量什么（都在**游玩区** 384×448 内，逐像素，逐帧）

| 记号 | 定义 | 为什么它能抓住"色块" |
| --- | --- | --- |
| `cov(t)`  | 画 astral **之前**那一刻的目的地 alpha 均值（= astral 在 `blendMode(7)` 里的**权重**） | astral 究竟"露了多少"，与颜色无关 |
| `grad(t)` | `cov` 在**垂直于扫过方向**的 8 条带上的 max−min | >0 = 真的**有梯度、在动**；恒 0 = 要么全遮要么全不遮（"一下盖住"） |
| `slab(t)` | 幕布源 alpha ≥ 0.90 的像素里，**最终合成色与纯 astral 差 > 24/255** 的面积占比 | ★ **这就是"色块"的定义**：幕布不透明的地方本该是 astral（一手 `blendMode(7)` 会把它整块换掉），出现别的颜色 = 一块死色 |
| `onsa(t)` | 最终合成**不透明**（A ≥ 0.99）**且**与纯 astral 差 > 24/255 的面积占比 | "不透明的非 astral 区域" —— 用户看到的蓝矩形 |
| `mono`    | `cov` 在转场窗口内是否单调（进：升 / 出：降，容差 0.02） | 转场该是**渐变**，不是"跳一下" |

## 合成模型（与 `engine/anm/th18/*` 的一手结论一致）

· 每帧开始 = 引擎 Clear，`surface 0` 清成 `0xffffffff`（一手 `th18:0x4553dc`）⇒ `Da := 1`；
· 世界层 3 的叠放 = **生成顺序**（尾插）；本卡的顺序由 `native/cards/trance.c` 固定为
  **[清零幕 271] → [4 台幕布] → [填充幕 270] → [astral 251]**；
· `blendMode(0)`：`rgb = S·Sa + D·(1−Sa)`、`a = Sa² + Da·(1−Sa)`；
· `blendMode(2)`（`th18:0x47da20`：SRCALPHA/ONE/**REVSUBTRACT**）：
  `rgb = D − Sa·S`、`a = Da − Sa²`（都 CLAMP 到 [0,1]）—— 脚本 271 用 `color(0,0,0)`+`alpha(255)`
  ⇒ `rgb := D`、`a := 0`（= TH13 `0x4032d0` 那块 `0x00000000` 四边形）；
· `blendMode(7)`（`th18:0x47db06`：DESTALPHA/INVDESTALPHA）：
  `rgb = S·Da + D·(1−Da)`、`a = Da·Sa + Da·(1−Da)` —— **源 alpha 不参与颜色**。

## 两条流水线

* `new` —— 当前脚本（271 清零 / 251 `blendMode(7)` / 幕布与填充幕 `color(64,64,56)`），
  叠放序 **[清零] → [幕布] → [填充] → [astral]**。
* `old` —— **第七轮实跑的那一版**（用户说"两个蓝色的矩形"的那一版），按当时的代码复现：
  astral `blendMode(0)` + 最先画、**没有**清零幕、幕布 `color(32,16,96)` 且**画在 astral 之上**
  （退出幕布层 19）、填充幕不起作用（在 astral 下面被压住）。
  ⇒ 它没有"目的地 alpha"这一步：幕布就是**不透明色块**，`cov ≡ 1`、`grad ≡ 0`。
"""
import argparse
import json
import math
import re
import sys
from pathlib import Path

try:
    from PIL import Image
except ImportError:                                    # pragma: no cover
    sys.exit("需要 Pillow：pip install pillow")

import numpy as np

FIBER = None
ZERO_ALPHA = 0
SPELL = None          # boss 符卡背景（`st0Xenm.anm` script13）：'after' = 我们的幕布在层 3（被盖）；'before' = 层 5（我们盖它）        # 清零幕的顶点 alpha：0 = 第九轮修后（C 侧写死 Sa≈0）；255 = 修前（Da := 1，遮罩失效）

HERE = Path(__file__).resolve().parent
MOD = HERE.parent
SCRIPTS = MOD / "assets" / "ability" / "scripts"
ART = MOD / "assets" / "ability" / "trance"

W, H = 384, 448                    # 游玩区（世界层里 1 单位 = 1 px；x∈[−192,192]、y∈[0,448]）
X0, Y0 = -W / 2, 0.0               # 世界坐标原点（像素 (0,0) 对应世界 (X0, Y0)）
THETA = 24.0 / 255.0               # "与纯 astral 差多少算另一个颜色"（逐通道均值）
SOLID = 0.90                       # 幕布"不透明"的阈值
NFRAMES = 60 + 56                  # 0..59 = 进灵界后 0..59 帧；60..115 = 退出后 0..55 帧
OUT_AT = 60
BANDS = 8

# ---------------------------------------------------------------- 脚本读取
_LABEL = re.compile(r"^\s*(-?\d+|\+\d+)\s*:")
_INS = re.compile(r"^\s*(\w+)\s*\((.*)\)\s*;\s*$")


def parse_script(path):
    """→ {frame: [(name, args…)]}（`-1` = 初始化块；`+N` 展开成绝对帧）。"""
    out, frame = {}, None
    for raw in path.read_text(encoding="utf-8").splitlines():
        line = raw.split("//")[0]
        m = _LABEL.match(line)
        if m:
            tok = m.group(1)
            frame = (frame or 0) + int(tok[1:]) if tok.startswith("+") else int(tok)
            out.setdefault(frame, [])
            continue
        m = _INS.match(line)
        if m and frame is not None:
            args = [a.strip() for a in m.group(2).split(",") if a.strip() != ""]
            out[frame].append((m.group(1), args))
    return out


def _f(s):
    return float(s.rstrip("fF"))


def _rect(name, extra_alpha=False):
    """`drawRect` 常驻 VM（清零幕 / 填充幕）。"""
    sc = parse_script(SCRIPTS / name)
    d = {"blend": 0, "layer": 3, "color": (255, 255, 255), "alpha": 255,
         "rect": None, "pos": (0.0, 0.0), "alpha_time": None}
    for fr in sorted(sc):
        for op, a in sc[fr]:
            if op == "blendMode":
                d["blend"] = int(a[0])
            elif op == "layer":
                d["layer"] = int(a[0])
            elif op == "color":
                d["color"] = tuple(int(float(x)) for x in a[:3])
            elif op == "alpha":
                d["alpha"] = int(float(a[0]))
            elif op == "alphaTime":
                d["alpha_time"] = (fr, int(a[0]), int(float(a[2])))
            elif op == "drawRect":
                d["rect"] = (_f(a[0]), _f(a[1]))
            elif op == "pos":
                d["pos"] = (_f(a[0]), _f(a[1]))
    return d


def load_wipe(name, tag):
    """一支幕布脚本 → 几何/颜色/时间线。tag 只用于打印。"""
    sc = parse_script(SCRIPTS / name)
    d = {"file": name, "tag": tag, "blend": 0, "layer": 3, "color": (255, 255, 255),
         "scale": (1.0, 1.0), "rot": 0.0, "pos": (0.0, 0.0), "pos_time": None, "life": 10 ** 9}
    for fr in sorted(sc):
        for op, a in sc[fr]:
            if fr != -1 and op not in ("posTime", "delete"):
                continue                       # 本卡的幕布只在 -1: 段配置（除了 posTime/delete）
            if op == "blendMode":
                d["blend"] = int(a[0])
            elif op == "layer":
                d["layer"] = int(a[0])
            elif op == "color":
                d["color"] = tuple(int(float(x)) for x in a[:3])
            elif op == "scale":
                d["scale"] = (_f(a[0]), _f(a[1]))
            elif op == "rotate":
                d["rot"] = _f(a[2])
            elif op == "pos":
                d["pos"] = (_f(a[0]), _f(a[1]))
            elif op == "posTime":
                d["pos_time"] = (fr + int(a[0]), int(a[1]), (_f(a[2]), _f(a[3])))
            elif op == "delete":
                d["life"] = fr
    return d


def load_astral():
    sc = parse_script(SCRIPTS / "251_trance_astral.anm.txt")
    d = {"blend": 0, "layer": 3, "scale": (1.0, 1.0), "pos": (0.0, 0.0), "angle_vel": 0.0}
    for fr in sorted(sc):
        for op, a in sc[fr]:
            if op == "blendMode":
                d["blend"] = int(a[0])
            elif op == "layer":
                d["layer"] = int(a[0])
            elif op == "scale":
                d["scale"] = (_f(a[0]), _f(a[1]))
            elif op == "angleVel":
                d["angle_vel"] = _f(a[2])
            elif op == "pos":
                d["pos"] = (_f(a[0]), _f(a[1]))
    return d


# ---------------------------------------------------------------- 采样 / 合成
class Layer:
    """一个 anm 图元：贴图（或 `drawRect` 纯色） + pos/scale/rot/tint/alpha/blendMode。"""

    def __init__(self, img, pos, scale, rot, tint, alpha, blend, rect=None):
        self.img, self.pos, self.scale, self.rot = img, pos, scale, rot
        self.tint, self.alpha, self.blend, self.rect = tint, alpha, blend, rect
        self.solid = img is None
        self._px = None if self.solid else img.load()
        self._size = None if self.solid else img.size

    def sample(self, wx, wy):
        """世界坐标 → (r,g,b,a) ∈ [0,1]^4；None = 这点没被覆盖。"""
        dx, dy = wx - self.pos[0], wy - self.pos[1]
        c, s = math.cos(-self.rot), math.sin(-self.rot)
        lx, ly = dx * c - dy * s, dx * s + dy * c          # 世界 → 本体（先反旋转）
        tw, th = self.rect if self.solid else self._size
        hw, hh = tw * self.scale[0] / 2.0, th * self.scale[1] / 2.0
        if lx < -hw or lx >= hw or ly < -hh or ly >= hh:
            return None
        tr, tg, tb = self.tint
        a = self.alpha / 255.0
        if self.solid:
            return (tr / 255.0, tg / 255.0, tb / 255.0, a)
        u = (lx + hw) / (2.0 * hw)
        v = (ly + hh) / (2.0 * hh)
        x, y = u * tw - 0.5, v * th - 0.5
        x0, y0 = int(math.floor(x)), int(math.floor(y))
        fx, fy = x - x0, y - y0

        def tex(i, j):
            return self._px[min(max(i, 0), tw - 1), min(max(j, 0), th - 1)]

        acc = [0.0] * 4
        for i, j, wgt in ((x0, y0, (1 - fx) * (1 - fy)), (x0 + 1, y0, fx * (1 - fy)),
                          (x0, y0 + 1, (1 - fx) * fy), (x0 + 1, y0 + 1, fx * fy)):
            if wgt <= 0:
                continue
            r, g, b, av = tex(i, j)
            acc[0] += r * wgt
            acc[1] += g * wgt
            acc[2] += b * wgt
            acc[3] += av * wgt
        return (acc[0] * tr / 65025.0, acc[1] * tg / 65025.0,
                acc[2] * tb / 65025.0, acc[3] / 255.0 * a)


# ★★★ alpha 通道的一手口径（2026-09-13 第九轮）：TH18 的 anm `blendMode` 落地函数
# `th18:0x47d937`–`0x47d9a8` 在每条模式分支**之前**统一设 `SEPARATEALPHABLENDENABLE(0xce)←1`、
# `SRCBLENDALPHA(0xcf)←ONE`、`DESTBLENDALPHA(0xd0)←ZERO`、`BLENDOPALPHA(0xd1)←ADD`
# ⇒ **任何 anm sprite 都是 `Da' := Sa`**，`blendMode(n)` 只管 RGB。
# （第一轮/第八轮按 TH13 的"分离 alpha 关闭"算，得到 `Da' = Sa² + Da(1−Sa)` / `Da − Sa²` —— 那是**错的**。）
def blend_pixel(d, s, mode):
    """按 anm 的 blendMode 把一个源像素合到目标上。d/s = [r,g,b,a]。"""
    sr, sg, sb, sa = s
    dr, dg, db, da = d
    if mode == 0:
        nr, ng, nb = (sr * sa + dr * (1 - sa), sg * sa + dg * (1 - sa), sb * sa + db * (1 - sa))
        na = sa                            # ★ 见下：TH18 恒开分离 alpha 混合 ⇒ a := Sa
    elif mode == 2:                       # SRCALPHA / ONE / REVSUBTRACT
        nr, ng, nb = dr - sa * sr, dg - sa * sg, db - sa * sb
        na = sa                            # ★ 见下：TH18 恒开分离 alpha 混合 ⇒ a := Sa
    elif mode == 7:                       # DESTALPHA / INVDESTALPHA
        nr, ng, nb = (sr * da + dr * (1 - da), sg * da + dg * (1 - da), sb * da + db * (1 - da))
        na = sa                            # ★ 同上
    else:
        raise AssertionError(f"blendMode({mode}) 没在本工具里建模（本卡只用 0/2/7）")
    return [min(max(v, 0.0), 1.0) for v in (nr, ng, nb, na)]


def backdrop(i, j):
    """合成用的**舞台背景**（离线替身：横向渐变 + 两个亮块 + 一点高频）。
    本工具只用它回答"这块颜色是不是**原本就有的东西**"。"""
    g = 0.18 + 0.42 * (i / W)
    if 60 <= i < 150 and 300 <= j < 380:
        g = 0.72
    if 250 <= i < 320 and 60 <= j < 140:
        g = 0.55
    g += 0.05 * ((i * 7 + j * 13) % 5) / 4.0
    return [g * 0.85, g * 0.9, g, 1.0]


def wipe_pos(wp, t):
    if not wp["pos_time"]:
        return wp["pos"]
    end, curve, tgt = wp["pos_time"]
    if t >= end:
        return tgt
    start = end - 30
    if t <= start:
        return wp["pos"]
    u = (t - start) / 30.0
    if curve == 1:                        # posTime 的第二参 = 缓动曲线，1 = easeIn2(x)=x²
        u = u * u
    return (wp["pos"][0] + (tgt[0] - wp["pos"][0]) * u,
            wp["pos"][1] + (tgt[1] - wp["pos"][1]) * u)


def build_layers(pipeline, switch_img, wipes, zero, cover, astral_img, cfg, t, in_exit,
                 force_rot0=False):
    """→ ([Layer…], astral 的下标)。流水线顺序见文件头。"""
    layers = []
    if pipeline == "new":                          # ① 清零幕（脚本 271）
        layers.append((Layer(None, zero["pos"], (1, 1), 0.0, zero["color"], ZERO_ALPHA,
                             zero["blend"], rect=zero["rect"]), "zero"))
    # ★★ 第九轮：填充幕（脚本 270）画在 **4 台幕布之前** —— 因为 TH18 的 alpha 是 `Da := Sa`
    # （后画的赢），一手 `effect.anm` script3 那台 VM 也是 4 台里的第 1 台 ⇒ 顺序 = [清零][填充][幕布][astral]。
    if pipeline == "new" and not in_exit:
        ca = cover["alpha"]
        if cover["alpha_time"]:
            f0, _dur, to = cover["alpha_time"]
            ca = cover["alpha"] if t < f0 else to
        layers.append((Layer(None, cover["pos"], (1, 1), 0.0, cover["color"], ca, cover["blend"],
                             rect=cover["rect"]), "cover"))
    for wp in wipes:                               # ② 4 台幕布
        if t > wp["life"]:
            continue
        lay = Layer(switch_img, wipe_pos(wp, t), wp["scale"],
                    0.0 if force_rot0 else wp["rot"], wp["color"], 255, wp["blend"])
        if pipeline == "old":                      # 第七轮那版：幕布 = 不透明蓝紫，画在 astral 之上
            lay.blend = 0
            lay.tint = (32, 16, 96)
        layers.append((lay, "wipe"))
    # ③ 填充幕见上面（挪到幕布之前）
    # ★★★ 第十轮（`AUDIT` §CT-8）：boss 符卡背景 = `st0Xenm.anm` script13
    #   （sprite60 = `cardbg1.png` 的 384×448、`pos(0,224)`、`blendMode(2)`、`alphaTime(60,0,255)`）
    #   + script11（`cardbg0` 512×512、scale 2、`color(192,64,128)`、`blendMode(0)`、alpha→255）。
    #   一手层号 = **`layer(4)`** ⇒ 层 4 那趟（优先级 0xb）在层 3（0xa）之后、层 5（0xd）之前。
    if SPELL is not None:
        sa = min(1.0, t / 60.0)                    # alphaTime(60, 0, 255)
        sb = [
            (Layer(switch_img, (0.0, 224.0), (3.0, 3.5), 0.0, (192, 64, 128), int(255 * sa), 0), "spell0"),
            (Layer(None, (0.0, 224.0), (1, 1), 0.0, (0, 0, 0), int(255 * sa), 2,
                   rect=(384.0, 448.0)), "spell1"),
        ] if SPELL == "after" else []
    alay = Layer(astral_img, cfg["pos"], cfg["scale"], cfg["angle_vel"] * t,
                 (255, 255, 255), 255, 7 if pipeline == "new" else 0)
    if pipeline == "old":                          # 第七轮那版：astral 最先画（不透明背景）
        layers.insert(0, (alay, "astral"))
    else:
        layers.append((alay, "astral"))            # ④ astral 最后画（一手画序 0x403591）
    if SPELL == "before":                          # 我们挪到层 5 ⇒ 符卡背景（层 4）**先**画
        layers = sb + layers
    elif SPELL == "after":                         # 我们还在层 3 ⇒ 符卡背景**后**画 = 盖住我们
        layers = layers + sb
    return layers, [k for k, (_, kind) in enumerate(layers) if kind == "astral"][0]


def astral_ref(astral_img, cfg, t):
    """纯 astral 参照（单独画在黑上）：判断"这块颜色是不是 astral 的图像"。"""
    lay = Layer(astral_img, cfg["pos"], cfg["scale"], cfg["angle_vel"] * t,
                (255, 255, 255), 255, 0)
    ref = [[0.0, 0.0, 0.0] for _ in range(W * H)]
    for j in range(H):
        for i in range(W):
            s = lay.sample(X0 + i + 0.5, Y0 + j + 0.5)
            if s is not None:
                ref[j * W + i] = s[:3]
    return ref


def render_frame(pipeline, astral_img, switch_img, wipes, zero, cover, cfg, t, in_exit,
                 force_rot0=False):
    layers, iast = build_layers(pipeline, switch_img, wipes, zero, cover, astral_img, cfg, t, in_exit,
                                force_rot0)
    ref = astral_ref(astral_img, cfg, t)
    rgb = [None] * (W * H)
    alpa = [0.0] * (W * H)
    dab = [0.0] * (W * H)
    wpa = [0.0] * (W * H)
    for j in range(H):
        for i in range(W):
            wx, wy = X0 + i + 0.5, Y0 + j + 0.5
            d = backdrop(i, j)
            before, cur = None, 0.0
            for k, (lay, kind) in enumerate(layers):
                if k == iast:
                    before = d[3]
                s = lay.sample(wx, wy)
                if s is None:
                    continue
                if kind == "wipe":
                    cur = max(cur, s[3])
                d = blend_pixel(d, s, lay.blend)
            n = j * W + i
            rgb[n], alpa[n], dab[n], wpa[n] = d[:3], d[3], (before if before is not None else d[3]), cur
    return rgb, alpa, dab, wpa, ref


def metrics(rgb, alpa, dab, wpa, ref):
    n = W * H
    cov = sum(dab) / n
    band, cnt = [0.0] * BANDS, [0] * BANDS
    for j in range(H):
        b = min(BANDS - 1, j * BANDS // H)
        for i in range(W):
            band[b] += dab[j * W + i]
            cnt[b] += 1
    band = [band[b] / max(cnt[b], 1) for b in range(BANDS)]
    slab = onsa = 0
    for j in range(H):
        for i in range(W):
            k = j * W + i
            rr, gg, bb = rgb[k]
            pr, pg, pb = ref[k]
            dv = (abs(rr - pr) + abs(gg - pg) + abs(bb - pb)) / 3.0
            if wpa[k] >= SOLID and dv > THETA:
                slab += 1
            if alpa[k] >= 0.99 and dv > THETA:
                onsa += 1
    # ★ astral 可见份额：最终合成仍等于"纯 astral"的面积占比（用户看到的"幕布还在不在"）
    same = 0
    for j in range(H):
        for i in range(W):
            k = j * W + i
            rr, gg, bb = rgb[k]
            pr, pg, pb = ref[k]
            if (abs(rr - pr) + abs(gg - pg) + abs(bb - pb)) / 3.0 <= THETA:
                same += 1
    return {"cov": cov, "grad": max(band) - min(band), "bands": band,
            "slab": slab / n, "onsa": onsa / n, "astral_share": same / n}


# ---------------------------------------------------------------- 纤维方向（用户口径：应当"左右方向"）
def fiber_angle(alpha_field):
    """单台幕布 alpha 场的**纤维方向**（度；0 = 水平/左右，±90 = 竖直）。

    判定贴图时同一套做法：结构张量 → 梯度主轴 θg（alpha 变化最快的方向）→
    **纤维方向 = θg + 90°**（笔触沿着纤维走，是等值线）。
    只在"有梯度"的像素上统计（丢掉 52% 那块 `scrollMode` 的 V=CLAMP 造成的实心区，
    否则实心区会把各向异性稀释掉）。
    """
    a = np.asarray(alpha_field, dtype=float)
    gx = np.zeros_like(a)
    gy = np.zeros_like(a)
    gx[:, 1:-1] = a[:, 2:] - a[:, :-2]
    gy[1:-1, :] = a[2:, :] - a[:-2, :]
    mag = np.hypot(gx, gy)
    m = mag >= max(float(np.percentile(mag, 75)), 1e-6)
    Jxx = float((gx[m] * gx[m]).sum())
    Jyy = float((gy[m] * gy[m]).sum())
    Jxy = float((gx[m] * gy[m]).sum())
    th = 0.5 * math.atan2(2 * Jxy, Jxx - Jyy)
    fib = ((math.degrees(th) + 90.0 + 90.0) % 180.0) - 90.0
    aniso = max(Jxx, Jyy) / max(min(Jxx, Jyy), 1.0)
    return fib, aniso


def fiber_report(switch_img, wipes, force_rot0, t):
    """单独画一台幕布的 alpha（1 世界单位 = 1 px，窗口 1200×1200）→ 纤维方向。"""
    W2 = H2 = 900
    wp = wipes[0]
    rot = 0.0 if force_rot0 else wp["rot"]
    lay = Layer(switch_img, wipe_pos(wp, t), wp["scale"], rot, (255, 255, 255), 255, 0)
    a = np.zeros((H2, W2))
    for j in range(H2):
        wy = -H2 / 2.0 + j + 0.5
        for i in range(W2):
            smp = lay.sample(-W2 / 2.0 + i + 0.5, wy)
            if smp is not None:
                a[j, i] = smp[3]
    return fiber_angle(a)


def run(pipeline, astral_img, switch_img, cfg, wipes_in, wipes_out, zero, cover,
        force_rot0=False):
    rows = []
    for f in range(NFRAMES):
        phase = "in" if f < OUT_AT else "out"
        wipes = wipes_in if phase == "in" else wipes_out
        t = f if phase == "in" else f - OUT_AT
        args = render_frame(pipeline, astral_img, switch_img, wipes, zero, cover, cfg, t,
                            phase == "out", force_rot0)
        m = metrics(*args)
        m.update(frame=f, t=t, phase=phase)
        rows.append(m)
    return rows


def show(rows, label):
    print(f"\n=== pipeline = {label}")
    print("  phase 帧 0..59 = 灵界开始后 0..59 帧；帧 60..115 = 灵界结束后 0..55 帧")
    print("  frame phase  t   cov    grad   slab   onsa   astral |  8 条带 cov")
    print("       (astral = 最终合成仍等于纯 astral 的面积占比；符卡宣言期间它掉下去就是『幕布被取代』)")
    for r in rows:
        if r["frame"] % 5 and r["frame"] not in (45, 59, 60, 70, 80, 90, 100, 110, 115):
            continue
        print(f"  {r['frame']:5d} {r['phase']:5s} {r['t']:3d} {r['cov']:.3f}  {r['grad']:.3f}  "
              f"{r['slab']:.3f}  {r['onsa']:.3f}  {r['astral_share']:.3f} | "
              + " ".join(f"{b:.2f}" for b in r["bands"]))
    fin = [r for r in rows if r["phase"] == "in"]
    fout = [r for r in rows if r["phase"] == "out"]

    def mono(seq, rising):
        return sum(1 for a, b in zip(seq, seq[1:])
                   if (rising and b < a - 0.02) or ((not rising) and b > a + 0.02))

    cov_in = [r["cov"] for r in fin[:46]]
    cov_out = [r["cov"] for r in fout[:56]]
    print(f"  进: cov 0→44→59 = {cov_in[0]:.3f} → {cov_in[44]:.3f} → {fin[59]['cov']:.3f}；"
          f"grad 峰值 {max(r['grad'] for r in fin):.3f}；slab 峰值 {max(r['slab'] for r in fin):.3f}"
          f"；onsa 峰值 {max(r['onsa'] for r in fin):.3f}")
    print(f"  出: cov 0→55 = {cov_out[0]:.3f} → {cov_out[55]:.3f}；"
          f"grad 峰值 {max(r['grad'] for r in fout):.3f}；slab 峰值 {max(r['slab'] for r in fout):.3f}"
          f"；onsa 峰值 {max(r['onsa'] for r in fout):.3f}")
    if SPELL is not None:
        print(f"  astral 可见份额(全程均值): {sum(r['astral_share'] for r in rows)/len(rows):.3f}"
              f"；最低 {min(r['astral_share'] for r in rows):.3f}"
              f"；末 10 帧均值 {sum(r['astral_share'] for r in rows[-10:])/10:.3f}")
    print(f"  单调性: 进 逆序步数 {mono(cov_in, True)}（应 0）；出 逆序步数 {mono(cov_out, False)}（应 0）")
    if FIBER is not None:
        fi, ai = FIBER
        tag = "左右方向 ✅" if abs(fi) <= 35.0 else ("竖直方向 ❌" if abs(fi) >= 55.0 else "斜的 ⚠")
        print(f"  纤维方向: {fi:+.1f}°（0=水平/左右，±90=竖直）各向异性 {ai:.1f} ⇒ {tag}")
    return {"cov_in0": cov_in[0], "cov_in45": fin[45]["cov"], "cov_in59": fin[59]["cov"],
            "cov_out0": cov_out[0], "cov_out55": cov_out[55],
            "slab_in_max": max(r["slab"] for r in fin), "slab_out_max": max(r["slab"] for r in fout),
            "onsa_in_max": max(r["onsa"] for r in fin), "onsa_out_max": max(r["onsa"] for r in fout),
            "grad_in_max": max(r["grad"] for r in fin), "grad_out_max": max(r["grad"] for r in fout),
            "mono_in": mono(cov_in, True), "mono_out": mono(cov_out, False)}


def main():
    global FIBER, ZERO_ALPHA, SPELL
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--pipeline", choices=["new", "old", "both"], default="both")
    ap.add_argument("--spell", choices=["after", "before"], default=None,
                    help="把 boss 符卡背景（st0Xenm script13, layer(4)）也画进来："
                         "after = 我们的幕布在层 3（被它盖住）；before = 层 5（我们盖住它）")
    ap.add_argument("--zero-alpha255", action="store_true",
                    help="清零幕的顶点 alpha 用 255（= 复现第九轮修前那条 bug 的『遮罩失效』）")
    ap.add_argument("--force-rot0", action="store_true",
                    help="把幕布的 z 旋转强制成 0（= 复现用户看到的『未旋转』笔触）")
    ap.add_argument("--json", metavar="PATH")
    a = ap.parse_args()
    ZERO_ALPHA = 255 if a.zero_alpha255 else 0
    SPELL = a.spell

    astral_img = Image.open(ART / "ASTRAL.png").convert("RGBA")
    switch_img = Image.open(ART / "SWITCH.png").convert("RGBA")
    cfg = load_astral()
    wipes_in = [load_wipe(f"26{k}_trance_wipe_in{k-5}.anm.txt", f"effect.anm script{k-3}")
                for k in range(6, 10)]
    wipes_out = [load_wipe(f"26{k}_trance_wipe{k-1}.anm.txt", f"effect.anm script{k+5}")
                 for k in range(2, 6)]
    zero = _rect("271_trance_alpha_zero.anm.txt")
    cover = _rect("270_trance_wipe_cover.anm.txt")

    print(f"脚本: astral(251) blendMode={cfg['blend']} scale={cfg['scale']} pos={cfg['pos']} "
          f"angleVel={cfg['angle_vel']}")
    print(f"      进场幕布 color={[w['color'] for w in wipes_in]} blend={[w['blend'] for w in wipes_in]}"
          f" 寿命={[w['life'] for w in wipes_in]} rot={[round(w['rot'], 4) for w in wipes_in]}")
    print(f"      退出幕布 color={[w['color'] for w in wipes_out]} 寿命={[w['life'] for w in wipes_out]}"
          f" rot={[round(w['rot'], 4) for w in wipes_out]}")
    print(f"      清零幕(271) blend={zero['blend']} color={zero['color']} "
          f"脚本alpha={zero['alpha']} **生效 alpha={ZERO_ALPHA}**(C 侧写死) rect={zero['rect']}")
    print(f"      符卡背景({'层4/在我们的幕布之后' if SPELL == 'after' else ('层4/在我们的幕布之前' if SPELL == 'before' else '不参与')})")
    print(f"      填充幕(270) blend={cover['blend']} color={cover['color']} "
          f"alphaTime={cover['alpha_time']} rect={cover['rect']} pos={cover['pos']}")

    res, todo = {}, (["old", "new"] if a.pipeline == "both" else [a.pipeline])
    for p in todo:
        FIBER = fiber_report(switch_img, wipes_in, a.force_rot0, 25.0)
        rows = run(p, astral_img, switch_img, cfg, wipes_in, wipes_out, zero, cover, a.force_rot0)
        res[p] = show(rows, p)
        res[p]["fiber_deg"], res[p]["fiber_aniso"] = FIBER
    if a.json:
        Path(a.json).write_text(json.dumps(res, indent=2, ensure_ascii=False), encoding="utf-8")
    if len(todo) == 2:
        o, n = res["old"], res["new"]
        print("\n=== 判据（离线）")
        print(f"  slab 峰值   old {max(o['slab_in_max'], o['slab_out_max']):.3f}"
              f"  →  new {max(n['slab_in_max'], n['slab_out_max']):.3f}   （色块面积占比）")
        print(f"  onsa 峰值   old {max(o['onsa_in_max'], o['onsa_out_max']):.3f}"
              f"  →  new {max(n['onsa_in_max'], n['onsa_out_max']):.3f}   （不透明非 astral 面积占比）")
        print(f"  cov 进 0→45 old {o['cov_in0']:.3f}→{o['cov_in45']:.3f}"
              f"  →  new {n['cov_in0']:.3f}→{n['cov_in45']:.3f}")
        print(f"  cov 出 0→55 old {o['cov_out0']:.3f}→{o['cov_out55']:.3f}"
              f"  →  new {n['cov_out0']:.3f}→{n['cov_out55']:.3f}")
        print(f"  grad 峰值   old {max(o['grad_in_max'], o['grad_out_max']):.3f}"
              f"  →  new {max(n['grad_in_max'], n['grad_out_max']):.3f}   （扫过的梯度）")
        ok = (max(n["slab_in_max"], n["slab_out_max"]) < 0.02
              and n["cov_in0"] < 0.05 and n["cov_in45"] > 0.90 and n["cov_out55"] < 0.05
              and n["mono_in"] == 0 and n["mono_out"] == 0
              and max(o["slab_in_max"], o["slab_out_max"]) > 0.05)
        print(f"  ⇒ new 通过「无色块 + 进 0→1 单调 + 出 →0 单调」，且 old 确实被判为有色块："
              f"{'✅' if ok else '❌'}")
        if not ok:
            sys.exit(1)


if __name__ == "__main__":
    main()
