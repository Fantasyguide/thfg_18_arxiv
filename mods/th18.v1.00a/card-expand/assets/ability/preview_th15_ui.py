#!/usr/bin/env python3
"""把 TH15 章节面板的**原样**拼成一张预览图（给人和文档看，不进游戏）。

为什么要它：TH15 的面板是**美术里印好的标签**（`front.anm` / `front02.png` 六条），
标签文字不在任何字符串表里，所以「原来长什么样」只能把原版贴图按 TH15 的坐标摆回去看。
坐标与字号全部来自一手（`engine/chapter/th15/01-chapter-and-bonus.md`）：

    面板父 VM   pos(448,402)、resolutionMode(2)（640×480 下 ×0.5）
    五条标签    子脚本 244..248 = sprite145@(0,0) / 146@(−64,50) / 147@(−64,90)
                / 149@(−64,130) / 148@(−64,170)，每条 192×32（→ 屏幕 96×16，**中心锚**）
    碎片标签    242/243 = sprite150（B 碎片，普通模式）/ 151（命碎）@(0,240)
    文字        五行右对齐（align_h=2）、font 2（10 px 行高 / 7 px 步进）、
                x = 300/308/300/300/300、y = 226/246/266/286/296，
                格式 `"%d"` / `"%3.1f%%"` / `"%3d"` / `"%6d"` / `"  +%d"`，第 5 行色 RGB(128,128,255)
    标题        面板出现时另用 font 4（16 px）在 (224,200) **居中**画一个整数（章号）

产物：`local/_scratch/th15_chapter_ui_preview.png`（640×480）与 `…_2x.png`（1280×960，看得清标签）。
⚠️ 原版贴图只在 `local/`，所以产物也落在 `local/`（gitignored），不进仓库。
"""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

HERE = Path(__file__).resolve().parent
REPO = HERE.parents[4]
SRC = REPO / "local/th15.v1.00b/anm/front/front/front02.png"
OUT = REPO / "local/_scratch"

SCALE = 0.5                      # resolutionMode(2) 在 640×480 下的缩放
PANEL = (448.0, 402.0)           # 面板父 VM 的 anm 坐标（1280×960 空间）

# (sprite y0, 偏移 dx, dy) —— 与 script241 的五个子脚本一一对应
STRIPS = [
    (0,   0.0,   0.0),           # sprite145 标题条
    (32,  -64.0, 50.0),          # sprite146
    (64,  -64.0, 90.0),          # sprite147
    (128, -64.0, 130.0),         # sprite149
    (96,  -64.0, 170.0),         # sprite148
    (160,  0.0, 240.0),          # sprite150 碎片标签（B 碎片）
]
# 五行文字：(x, y, 文本, RGB)
TEXTS = [
    (300.0, 226.0, "1234",      (255, 255, 255)),
    (308.0, 246.0, "87.5%",     (255, 255, 255)),
    (300.0, 266.0, "  0",       (255, 255, 255)),
    (300.0, 286.0, "1234567",   (255, 255, 255)),
    (300.0, 296.0, "  +12345",  (128, 128, 255)),
]


def font(px):
    try:
        return ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", px)
    except OSError:
        return ImageFont.load_default()


def compose(zoom):
    W, H = int(640 * zoom), int(480 * zoom)
    img = Image.new("RGB", (W, H), (56, 56, 64))
    d = ImageDraw.Draw(img)

    # 弹幕区（384×448 @ (32,16)）—— 只为给面板一个位置参照
    d.rectangle([32 * zoom, 16 * zoom, (32 + 384) * zoom - 1, (16 + 448) * zoom - 1],
                outline=(96, 96, 112))

    src = Image.open(SRC).convert("RGBA")
    for y0, dx, dy in STRIPS:
        tile = src.crop((0, y0, 192, y0 + 32))
        w, h = int(192 * SCALE * zoom), int(32 * SCALE * zoom)
        tile = tile.resize((w, h), Image.NEAREST) if zoom != 1.0 else \
            tile.resize((w, h), Image.LANCZOS)
        cx = (PANEL[0] + dx) * SCALE * zoom
        cy = (PANEL[1] + dy) * SCALE * zoom
        img.paste(tile, (int(cx - w / 2), int(cy - h / 2)), tile)   # 中心锚

    # 标题：章号，font 4（16 px）、居中于 (224,200)
    f4 = font(int(16 * zoom))
    title = "43"
    tw = d.textbbox((0, 0), title, font=f4)[2]
    d.text(((224 * zoom) - tw / 2, 200 * zoom - int(9 * zoom)), title,
           font=f4, fill=(238, 255, 136))

    # 五行数值：右对齐，font 2（10 px）
    f2 = font(int(10 * zoom))
    for x, y, txt, rgb in TEXTS:
        tw = d.textbbox((0, 0), txt, font=f2)[2]
        d.text((x * zoom - tw, y * zoom - int(5 * zoom)), txt, font=f2, fill=rgb)

    return img


def main():
    if not SRC.is_file():
        raise SystemExit(f"缺原版贴图 {SRC}\n（先 python3 tooling/thtk/unpack.py th15.v1.00b）")
    OUT.mkdir(parents=True, exist_ok=True)
    for zoom, name in ((1, "th15_chapter_ui_preview.png"),
                       (2, "th15_chapter_ui_preview_2x.png")):
        p = OUT / name
        compose(zoom).save(p)
        print(f"→ {p.relative_to(REPO)}")


if __name__ == "__main__":
    main()
