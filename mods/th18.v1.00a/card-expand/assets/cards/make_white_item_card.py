#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""make_white_item_card.py —— 「白底 + 一件物品撑满」的卡面生成器（2026-10-02）。

    python3 make_white_item_card.py ZENITH --item _src/ZENITH.png     # 87 天顶剑（Terraria 原版物品）
    python3 make_white_item_card.py TONY_GRIEF --text '?'             # 86 占位（美术待 owner 提供）

## 样式出处（不是自创）

= 系统卡 106–118 的**定稿样式**（`tools/make_system_cardart.py --style white`；owner 2026-09-18 口径
「白底 + 图标最大居中」，见那个脚本的文件头）：
纯白底 + 物品按**整数倍最近邻**放大到画面区（220×284）里最大居中，**不画框** ——
加框交给 `assets/cardframe.py`（`frame_card(area=True)` 1:1 贴，不吃重采样 ⇒ 像素画不糊）。
`_art/<NAME>.png` 是唯一真源（带 `renkolab-art=area` 标记），`_max` / `_min` 都从它出。

## 与 `fit_card.py` / `gen_placeholder.py` 的分工

- 有一张**现成的整卡图**（插画 / 扫描卡）⇒ `fit_card.py`（去白边、裁切、套框）。
- 只有一件**像素物品**，要白底撑满 ⇒ 本脚本（整数倍最近邻，不重采样）。
- 美术还没有、只想让卡能进牌组测试 ⇒ `--text '?'`（本脚本的占位路；= 系统卡缺图标时那个问号）。

❓ 自定：`PAD`（物品与画面区边缘的留白，默认 6 px，与系统卡同值）。尺寸口径全部来自
`cardframe.py`（外圈 3 + 斜面 13 + 内圈 2 = 每边内缩 18）。
"""
import argparse
import sys
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

HERE = Path(__file__).resolve().parent
OUT = HERE
sys.path.insert(0, str(HERE.parent))       # assets/ ⇒ import cardframe
import cardframe  # noqa: E402

W, H = cardframe.W, cardframe.H                     # 256×320
AW, AH = cardframe.AREA_W, cardframe.AREA_H         # 220×284（画面区）
M = cardframe.INSET                                 # 18
PAD = 6
WHITE = (255, 255, 255, 255)
LAT_FONT = "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"


def compose_item(item: Image.Image, fill: bool = False) -> Image.Image:
    """纯白底 + 物品放大到画面区里最大居中。

    `fill=False`（默认）= **整数倍最近邻**（像素画不糊；小图会留白边）。
    `fill=True` = 直接缩到画面区上限（LANCZOS）—— 源图本来就大（如 56×56 的泰拉瑞亚物品）时用它「撑满」。"""
    art = Image.new("RGBA", (W, H), WHITE)
    avail = min(AW - 2 * PAD, AH - 2 * PAD)
    if fill:
        k = avail / max(item.width, item.height)
        big = item.resize((max(1, round(item.width * k)), max(1, round(item.height * k))), Image.LANCZOS)
    else:
        k = max(1, avail // max(item.width, item.height))
        big = item.resize((item.width * k, item.height * k), Image.NEAREST)
    art.alpha_composite(big, ((W - big.width) // 2, (H - big.height) // 2))
    return art


def compose_text(text: str, font_path: str = LAT_FONT) -> Image.Image:
    """占位：纯白底 + 一个灰字形（= 系统卡缺图标时同一套画法）。
    `font_path` 可换 —— 例如卢恩字母 ᛃ 只有 FreeMono 有（DejaVu 会渲染成豆腐块）。"""
    art = Image.new("RGBA", (W, H), WHITE)
    d = ImageDraw.Draw(art)
    try:
        f = ImageFont.truetype(font_path, 150)
    except Exception:                       # noqa: BLE001 —— 没字体就退回默认
        f = ImageFont.load_default()
    d.text((W // 2, H // 2), text, font=f, fill=(200, 200, 205, 255), anchor="mm")
    return art


def emit(name: str, art: Image.Image, out: Path):
    out.mkdir(parents=True, exist_ok=True)
    (out / "_art").mkdir(exist_ok=True)
    cardframe.save_art(art, out / "_art" / f"{name}.png")            # 真源（画框前的画面）
    cardframe.save_framed(cardframe.frame_card(art, area=True), out / f"{name}_max.png")
    cardframe.save_min_from_art(name, art, area=True)                # ⚠ 写的是 assets/cards/<NAME>_min.png
    print(f"{name}: {name}_max.png {W}x{H} · {name}_min.png 64x80 · _art/{name}.png（真源）")


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("name")
    g = ap.add_mutually_exclusive_group(required=True)
    g.add_argument("--item", type=Path, help="物品 PNG（透明底像素画；整数倍最近邻放大）")
    g.add_argument("--text", help="占位字形 / 文字（一般为 '?'）")
    ap.add_argument("--font", default=LAT_FONT, help="--text 用的字体（默认 DejaVu Sans Bold）")
    ap.add_argument("--fill", action="store_true",
                    help="物品直接缩到画面区上限（LANCZOS；源图大时用它撑满，默认整数倍最近邻）")
    ap.add_argument("--out", default=str(OUT))
    a = ap.parse_args()
    if a.item:
        art = compose_item(Image.open(a.item).convert("RGBA"), a.fill)
    else:
        art = compose_text(a.text, a.font)
    emit(a.name, art, Path(a.out))
    return 0


if __name__ == "__main__":
    sys.exit(main())
