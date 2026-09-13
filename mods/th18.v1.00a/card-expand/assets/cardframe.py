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


def save_framed(im: Image.Image, path: Path):
    meta = PngImagePlugin.PngInfo()
    meta.add_text(TAG, "1")
    im.save(path, pnginfo=meta)


ART = CARDS / "_art"


def save_min_from_art(name: str, art: Image.Image):
    """_min = 无框画面缩到 64×80（原生同款：HUD 图标不带框）。"""
    art.convert("RGBA").resize((MIN_W, MIN_H), Image.LANCZOS).save(CARDS / f"{name}_min.png")


def reframe(name: str) -> bool:
    big, art_path = CARDS / f"{name}_max.png", ART / f"{name}.png"
    if not art_path.exists():
        raise SystemExit(f"{name}: 缺 {art_path.relative_to(HERE)}（无框画面）——用 fit_card.py 重出，或从旧版本恢复")
    art = Image.open(art_path)
    save_min_from_art(name, art)
    if is_framed(big):
        print(f"{name}: _max already framed; _min regenerated from _art")
        return False
    save_framed(apply_frame(art), big)
    print(f"{name}: _max framed (art 220x284 inside 256x320), _min from _art")
    return True


def main(argv):
    names = argv or sorted(p.name[:-8] for p in CARDS.glob("*_max.png"))
    n = sum(reframe(n) for n in names)
    print(f"{n} card(s) framed")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
