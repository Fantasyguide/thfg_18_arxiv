#!/usr/bin/env python3
"""给 256×320 的卡图加原生同款边框（程序合成，不含原生像素）。

    python3 cardframe.py                 # 给 cards/*_max.png 全部补框（已补过的跳过）；_min 一律从 cards/_art/NAME.png（无框画面）重出
    python3 cardframe.py NAME [NAME…]    # 只处理这几张

原生 _min 是裸画面（64×80，无框）——HUD 卡组图标不带框；只有 _max 带框。所以无框画面要留一份：cards/_art/NAME.png（256×320）。

原生 abcard 的框（117 张逐像素一致，量自 KANAME_max / ALICE_OP_max）：
  外圈 3 px 纯黑 → 13 px 深色斜面 → 2 px 纯黑 → 画面区 220×284（每边内缩 18 px）。
  斜面底色 (35,24,21)；每条边中点有一道高光，沿边按 1 − (偏移/半长)² 衰减（左右半长 100，上下 64），
  越靠内越亮（左右：外 101 → 内 133；上下：外 43 → 内 80），四角斜接。
画面区比例 220:284 与 256:320 不同，用「等比覆盖 + 居中裁切」（宽裁 ~1.4%），不拉伸。
已补框的 PNG 写一个 tEXt `renkolab-frame=1`，重复跑不会套两层。

## ★ `_art` 的两种口径（2026-09-18 加，向后兼容）

1. **满版 256×320**（默认，`fit_card.py` 出的就是这种）：加框走「等比覆盖 + 居中裁切」——上面那条老路。
2. **画面区原尺寸 220×284**，或 256×320 但内容已按画面区排好版（PNG 里带 `renkolab-art=area` 标记）：
   加框 **1:1 贴、不重采样**，`_min` 取**内容外框**等比缩进 64×80 居中。
   为什么要有这条：系统卡那批（106–118）的画面是整数倍最近邻放大的 exe 图标，
   走「覆盖裁切」会被缩成 0.8875 倍（= 178 种像素宽度，糊）；1:1 才对得上设计稿。
   这一路只在 `_art` 带标记 / 尺寸正好 220×284 时启用 ⇒ **其他卡的行为一个像素都没变**。
"""
import sys
from pathlib import Path

from PIL import Image, PngImagePlugin

HERE = Path(__file__).resolve().parent
CARDS = HERE / "cards"
W, H = 256, 320
MIN_W, MIN_H = 64, 80
OUTER, BEVEL, INNER = 3, 13, 2
INSET = OUTER + BEVEL + INNER          # 18
BASE = (35, 24, 21)
SIDE_PEAK, SIDE_W0, SIDE_HALF = (133, 127, 126), 0.67, 100.0
TB_PEAK, TB_W0, TB_HALF = (80, 71, 70), 0.18, 64.0
TAG = "renkolab-frame"
TAG_ART = "renkolab-art"          # `_art` 的口径标记（见文件头 §两种口径）
TAG_ART_AREA = "area"             # 画面区原尺寸：加框 1:1、_min 按内容外框缩
AREA_W, AREA_H = W - 2 * INSET, H - 2 * INSET             # 220×284


def _bevel_color(x, y):
    """(x, y) 落在斜面环里时的颜色：按离哪条外边最近决定属于哪条边（斜接）。"""
    dl, dr, dt, db = x, W - 1 - x, y, H - 1 - y
    d = min(dl, dr, dt, db)
    depth = (d - OUTER) / (BEVEL - 1)                      # 0 外 → 1 内
    if d == dl or d == dr:
        off = abs(y - (H - 1) / 2); peak, w0, half = SIDE_PEAK, SIDE_W0, SIDE_HALF
    else:
        off = abs(x - (W - 1) / 2); peak, w0, half = TB_PEAK, TB_W0, TB_HALF
    f = max(0.0, 1.0 - (off / half) ** 2)
    w = w0 + (1.0 - w0) * depth
    return tuple(int(round(b + f * w * (p - b))) for b, p in zip(BASE, peak))


_FRAME = None


def frame_image():
    """256×320 RGBA：框不透明，画面区透明。只算一次。"""
    global _FRAME
    if _FRAME is None:
        im = Image.new("RGBA", (W, H), (0, 0, 0, 0))
        px = im.load()
        for y in range(H):
            for x in range(W):
                d = min(x, W - 1 - x, y, H - 1 - y)
                if d < OUTER or INSET - INNER <= d < INSET:
                    px[x, y] = (0, 0, 0, 255)
                elif d < INSET:
                    px[x, y] = _bevel_color(x, y) + (255,)
        _FRAME = im
    return _FRAME


def is_framed(path: Path) -> bool:
    try:
        return Image.open(path).info.get(TAG) == "1"
    except Exception:
        return False


def apply_frame(art: Image.Image) -> Image.Image:
    """art：256×320（或任意尺寸）RGBA 满版卡图 → 加框后的 256×320。"""
    iw, ih = W - 2 * INSET, H - 2 * INSET                  # 220×284
    s = max(iw / art.width, ih / art.height)                # 等比覆盖
    art = art.convert("RGBA").resize((max(iw, round(art.width * s)), max(ih, round(art.height * s))), Image.LANCZOS)
    ox, oy = (art.width - iw) // 2, (art.height - ih) // 2
    art = art.crop((ox, oy, ox + iw, oy + ih))
    out = Image.new("RGBA", (W, H), (0, 0, 0, 255))
    out.paste(art, (INSET, INSET))
    out.alpha_composite(frame_image())
    return out


def frame_card(art: Image.Image, area: bool = False) -> Image.Image:
    """无框画面 → 加框的 256×320。`area=True` = 画面区原尺寸口径（1:1，不重采样）。"""
    if not area:
        return apply_frame(art)
    a = art.convert("RGBA")
    if a.size == (W, H):
        a = a.crop((INSET, INSET, W - INSET, H - INSET))        # 内容已按画面区排好版 ⇒ 只裁掉外圈
    elif a.size != (AREA_W, AREA_H):
        a = apply_frame(a)                                       # 尺寸不对就别装作是画面区口径
        return a
    out = Image.new("RGBA", (W, H), (0, 0, 0, 255))
    out.paste(a, (INSET, INSET))
    out.alpha_composite(frame_image())
    return out


def min_image_from_art(art: Image.Image, area: bool = False) -> Image.Image:
    """`_min`（64×80，无框）。默认口径 = `_art` 直接缩成 64×80（256:320 = 64:80，正好 ÷4）。
    `area=True` = 把**内容外框**（非白像素的 bbox）等比缩进 64×80 居中——系统卡那批的
    画面是「白底 + 图标居中」，直接 ÷4 会连白边一起缩、图标显得小。"""
    a = art.convert("RGBA")
    if not area:
        return a.resize((MIN_W, MIN_H), Image.LANCZOS)
    if a.size == (W, H):
        a = a.crop((INSET, INSET, W - INSET, H - INSET))
    bg = (255, 255, 255) if _white_bg(a) else (0, 0, 0, 0)
    box = _content_box(a, bg)
    if box is None:
        return Image.new("RGBA", (MIN_W, MIN_H), bg)
    c = a.crop(box)
    s = min(MIN_W / c.width, MIN_H / c.height)
    tw, th = max(1, round(c.width * s)), max(1, round(c.height * s))
    c = _integer_prefilter(c, tw, th)                          # ★ 见函数注释：像素画先按整数倍降，别直接 LANCZOS
    c = c.resize((tw, th), Image.LANCZOS)
    out = Image.new("RGBA", (MIN_W, MIN_H), bg)
    out.alpha_composite(c, ((MIN_W - c.width) // 2, (MIN_H - c.height) // 2))
    return out


def _integer_prefilter(c: Image.Image, tw: int, th: int) -> Image.Image:
    """缩 `_min` 前先按**能整除内容宽高的最大整数倍**降采样（NEAREST），再用 LANCZOS 缩到目标。

    为什么（2026-09-18，用户口径「不糊」）：系统卡那批 `_art` 是**整数倍最近邻放大**出来的
    （106–117 图标 ×6、118 拼图每格 ×2）。直接 LANCZOS 缩会把已经放大的块再抹一遍 ⇒ 边缘发虚；
    先整除降回「原始像素网格」（×6 → ÷6 回到 32×32，×2 → ÷2 回到 32×32），再一次性缩到 64×80，
    等于从原始图标缩，硬边保住。取「仍不小于目标尺寸」的最大 k，避免先缩过头再放大。"""
    k = 1
    for cand in range(2, 33):
        if c.width % cand == 0 and c.height % cand == 0 \
                and c.width // cand >= tw and c.height // cand >= th:
            k = cand
    return c.resize((c.width // k, c.height // k), Image.NEAREST) if k > 1 else c


def _white_bg(im: Image.Image) -> bool:
    """画面是不是「白底」（四角都接近纯白）⇒ `_min` 的底就用白，否则透明。"""
    px = im.load()
    for x, y in ((0, 0), (im.width - 1, 0), (0, im.height - 1), (im.width - 1, im.height - 1)):
        r, g, b, a = px[x, y]
        if a < 250 or (r, g, b) != (255, 255, 255):
            return False
    return True


def _content_box(im: Image.Image, bg):
    """非底色像素的 bbox；整块都是底色就返回 None。用 getbbox 的快路（白底先做差）。"""
    if bg == (255, 255, 255):
        from PIL import ImageChops
        diff = ImageChops.difference(im.convert("RGB"), Image.new("RGB", im.size, (255, 255, 255)))
        return diff.convert("L").point(lambda v: 255 if v > 6 else 0).getbbox()
    return im.getchannel("A").point(lambda v: 255 if v > 8 else 0).getbbox()


def save_art(im: Image.Image, path: Path):
    """存「无框画面」`_art`，并打上口径标记（`renkolab-art=area` ⇒ 加框 1:1、`_min` 按内容外框缩）。"""
    meta = PngImagePlugin.PngInfo()
    meta.add_text(TAG_ART, TAG_ART_AREA)
    im.save(path, pnginfo=meta)


def save_framed(im: Image.Image, path: Path):
    meta = PngImagePlugin.PngInfo()
    meta.add_text(TAG, "1")
    im.save(path, pnginfo=meta)


ART = CARDS / "_art"


def save_min_from_art(name: str, art: Image.Image, area: bool = False):
    """_min = 无框画面缩到 64×80（原生同款：HUD 图标不带框）。`area` 见 `min_image_from_art()`。"""
    min_image_from_art(art, area).save(CARDS / f"{name}_min.png")


def art_is_area(art: Image.Image, art_path=None) -> bool:
    """`_art` 是不是「画面区原尺寸」口径：尺寸正好 220×284，或 PNG 带 `renkolab-art=area` 标记。"""
    if art.size == (AREA_W, AREA_H):
        return True
    if art_path is not None:
        try:
            return Image.open(art_path).info.get(TAG_ART) == TAG_ART_AREA
        except Exception:
            return False
    return False


def reframe(name: str) -> bool:
    big, art_path = CARDS / f"{name}_max.png", ART / f"{name}.png"
    if not art_path.exists():
        raise SystemExit(f"{name}: 缺 {art_path.relative_to(HERE)}（无框画面）——用 fit_card.py 重出，或从旧版本恢复")
    art = Image.open(art_path)
    area = art_is_area(art, art_path)
    save_min_from_art(name, art, area)
    if is_framed(big):
        print(f"{name}: _max already framed; _min regenerated from _art" + (" (area)" if area else ""))
        return False
    save_framed(frame_card(art, area), big)
    print(f"{name}: _max framed" + (" (art = 画面区原尺寸, 1:1)" if area else " (art 220x284 inside 256x320)")
          + ", _min from _art")
    return True


def main(argv):
    names = argv or sorted(p.name[:-8] for p in CARDS.glob("*_max.png"))
    n = sum(reframe(n) for n in names)
    print(f"{n} card(s) framed")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
