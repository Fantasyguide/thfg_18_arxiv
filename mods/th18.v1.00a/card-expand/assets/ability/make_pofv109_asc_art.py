#!/usr/bin/env python3
"""从 **TH19 的 `ascii_1280.png`** 切出 HUD 文字要用的字形（原版原样，不重画）。

    python3 make_pofv109_asc_art.py            # 切图 + 写预览
    python3 make_pofv109_asc_art.py --dry-run  # 只量几何、打印对照表，不落盘

## 为什么是这张图、这一段（用户 2026-10-23 口径）

> 「`D:\\SteamLibrary\\steamapps\\common\\th19\\data\\ANM\\ascii\\ascii_1280.png`
>   里面从上到下的第二个字体区域里面的字体」
> 「3 行 … 32 个一格」 → 再确认「你切出来的是对的」（2026-10-24 定稿）

`ascii_1280.png`（512×512）自上而下：**大号衬线体**（顶部 ~168px）→ **本脚本这三行细字** →
三套粗数字 → `Bonus History Failed` + 蝴蝶。这三行细字才是 TH19 HUD 用的那一档。

⚠ 关键教训（错了一整轮）：这三行字是**亮字**（alpha>0 且 RGB 高），
   而 `ascii_1280.anm` 的 sprite 表只覆盖顶部 504×168 那片**衬线体** ⇒
   ① 原版里这三行**没有 sprite**，必须自己切；
   ② 扫墨迹若用「暗墨」（`R<128`）只会圈到描边，行带表会整体错位。

## 几何（本脚本自己量，不靠肉眼；量不到就报错，不许硬跑）

三行**行距 16px、格宽 16px、每行 32 格**；整块 = `512×48`，一格 16×16：

| 行 | 内容 | 格位 |
| --- | --- | --- |
| 0 | `! " # $ % & ' ( ) * + , - . /` `0-9` `: ; < = > ?` | 第 0 格是**空格**，字在第 1..31 格 |
| 1 | `@` `A-Z` `[ \\ ] ^ _` | 第 0..31 格 |
| 2 | `` ` `` `a-z` `{ | } ~` | 第 0 格是**空格**，字在第 1..30 格，第 31 格空 |

→ 96 格里 **93 个字符**（3 格空：0 / 64 / 95）。

## 产物

`assets/ability/ascii/POFV109_ASC_C00.png … C95.png`（**96 张 16×16，原始像素、不缩放、不补边**）
⇒ `ORDER.txt` 里排成 **一个** 图集段 `ATLAS ATLAS_POFV109_ASC 512x48@32x3`
（= **1 个 entry + 96 个 sprite**，sprite 号连续：`C00 + 格号`）。

为什么切**整格**而不是按墨迹裁：格宽 16 是原版的**字距**，C 侧推进一个字符就 `x += 16`，
不需要每个字一张宽度表；`.`/`,` 的留白也原样保留。

源图在 `local/`（gitignored，与 TH19 同版权归属），本脚本只做「切 + 落盘」。
"""
import argparse
import re
import sys
from pathlib import Path

from PIL import Image, ImageDraw

HERE = Path(__file__).resolve().parent            # assets/ability/
REPO = HERE.parents[4]                            # 仓库根
OUT = HERE / "ascii"
SHEET = HERE / "POFV109_ASC_sheet.png"            # 人看的对照图（×3 + 字符标签）

COLS = 32
PITCH = 16                                        # 行距 = 格宽 = 16（实测）
CELL = 16

# ── 字符表：格号 → 字符（空格 = None）─────────────────────────────────────
# 三行的排法不是普通的「从第 0 格起按 ASCII 铺」：第 0 行和第 2 行的**第 0 格是空的**
# （空出来的是 `space` 与一个空位），所以逐格查表，别用 `0x20 + 格号` 这种想当然的公式。
def char_of(row: int, col: int):
    idx = row * COLS + col
    if row == 0:
        if col == 0:
            return " "
        if col <= 15:
            return chr(0x20 + col)                # ! " # … /
        if col <= 25:
            return chr(ord("0") + col - 16)       # 0-9
        return chr(0x3A + col - 26)               # : ; < = > ?
    if row == 1:
        return chr(0x40 + col)                    # @ A-Z [ \ ] ^ _
    if col == 0:
        return " "
    if col <= 26:
        return chr(0x60 + col)                    # a-z
    if col <= 30:
        return chr(0x7B + col - 27)               # { | } ~
    return " "


def find_source() -> Path:
    """TH19 原生 ascii 图：`local/th19*/data/ANM/ascii/ascii_1280.png`（可多个，取第一个）。"""
    hits = sorted(REPO.glob("local/th19*/data/ANM/ascii/ascii_1280.png"))
    if not hits:
        raise SystemExit(
            "找不到 TH19 的 `ascii_1280.png`。把零售样本放进 `local/th19.<ver>/data/ANM/ascii/`"
            "（整份 `data/` 拷过来即可），或用 `--src` 指一个副本。")
    return hits[0]


def ink_mask(im: Image.Image):
    """墨迹 = alpha>8 且 RGB 高（这三行是**亮字**；用暗墨只会圈到描边）。"""
    px = im.load()
    w, h = im.size
    rows = []
    for y in range(h):
        n = 0
        for x in range(w):
            r, g, b, a = px[x, y]
            if a > 8 and max(r, g, b) >= 96:
                n += 1
        rows.append(n)
    return rows


def measure(im: Image.Image):
    """量出三行细字：返回 (y0, 行数, 格宽, 每行每格的墨迹 bbox 列表)。

    判据（不写死 y）：整图逐行数墨迹 → 连续非零行 = 一个「行组」→
    找**行距恰好 16 且每行高度 ≤ 16** 的连续 3 组（衬线体那几行高 25~28、行距 29，量不进来）。
    """
    rows = ink_mask(im)
    groups, cur = [], None
    for y, n in enumerate(rows):
        if n:
            if cur is None:
                cur = [y, y]
            else:
                cur[1] = y
        elif cur is not None:
            groups.append(tuple(cur))
            cur = None
    if cur is not None:
        groups.append(tuple(cur))

    # 行带 = 3 个**连续**的细行组：每行高度 ≤ 16、行距按「墨迹底边」恰好 16（基线间距）
    cand = []
    for i in range(len(groups) - 2):
        g = groups[i:i + 3]
        hs = [b - a + 1 for a, b in g]
        bases = [b for _, b in g]
        if max(hs) <= PITCH and bases[1] - bases[0] == PITCH and bases[2] - bases[1] == PITCH:
            cand.append((i, g, hs))
    if len(cand) != 1:
        raise SystemExit(f"三行细字的定位不唯一/没找到（候选 {cand}）；源图换了？"
                         f"行组：{groups}")
    i, g, hs = cand[0]

    # 格顶 y0：块内只含这三组墨迹，且**首行墨迹上下留白尽量相等**（确定性规则；空白格/描边都不影响）
    prev_end = groups[i - 1][1] if i > 0 else -1
    next_top = groups[i + 3][0] if i + 3 < len(groups) else 10 ** 9
    best = None
    for y in range(max(0, prev_end + 1), min(g[0][0], next_top - 3 * PITCH) + 1):
        if y + 3 * PITCH - 1 >= next_top:
            continue
        top_pad = g[0][0] - y
        bot_pad = y + PITCH - 1 - g[0][1]
        key = (abs(top_pad - bot_pad), y)
        if best is None or key < best[0]:
            best = (key, y, top_pad, bot_pad)
    if best is None:
        raise SystemExit(f"找不到合法的格顶（行组 {g}）")
    _, y0, top_pad, bot_pad = best
    print(f"几何：三行细字 y={y0}..{y0 + 3 * PITCH - 1}（行高 {hs}，基线间距 {PITCH}，"
          f"首行留白 上{top_pad}/下{bot_pad}）")

    w, h = im.size
    if w % COLS or PITCH * 3 > h:
        raise SystemExit(f"源图 {w}×{h} 与 32 格/行 不相容")
    cell = w // COLS
    if cell != CELL:
        raise SystemExit(f"格宽实测 {cell} ≠ 常量 {CELL}（改常量之前先确认是不是换了源图）")

    # 逐格墨迹 bbox：自检用（空格必须没有墨迹、有字的格墨迹必须落在格里）
    px = im.load()
    boxes = []
    for r in range(3):
        line = []
        for c in range(COLS):
            x0, y0c = c * cell, y0 + r * PITCH
            bx = None
            for y in range(y0c, y0c + cell):
                for x in range(x0, x0 + cell):
                    rr, gg, bb, aa = px[x, y]
                    if aa > 8 and max(rr, gg, bb) >= 96:
                        bx = (x - x0, y - y0c) if bx is None else (
                            min(bx[0], x - x0), min(bx[1], y - y0c))
                        bx = (bx[0], bx[1], x - x0, y - y0c)
            line.append(bx)
        boxes.append(line)
    return y0, cell, boxes


def self_check(boxes):
    """格号 ↔ 字符 的逐格核对：空格的格必须全透明，有字的格必须有墨迹。"""
    bad = []
    for r in range(3):
        for c in range(COLS):
            ch, bx = char_of(r, c), boxes[r][c]
            if ch == " " and bx is not None:
                bad.append(f"格 {r * COLS + c:02d} 应为空格但有墨迹 {bx}")
            if ch != " " and bx is None:
                bad.append(f"格 {r * COLS + c:02d} 应为 {ch!r} 但没有墨迹")
    if bad:
        raise SystemExit("逐格核对不过：\n  " + "\n  ".join(bad))


def write_cells(im: Image.Image, y0: int, cell: int):
    OUT.mkdir(parents=True, exist_ok=True)
    n = 0
    for r in range(3):
        for c in range(COLS):
            box = (c * cell, y0 + r * PITCH, (c + 1) * cell, y0 + r * PITCH + cell)
            im.crop(box).save(OUT / f"POFV109_ASC_C{r * COLS + c:02d}.png")
            n += 1
    return n


def write_sheet(im: Image.Image, y0: int, cell: int, scale: int = 3):
    """对照图：6 段 × 16 格，×3 放大 + 格线 + 字符标签（人一眼能核）。"""
    pad, lab = 2, 12
    per_row = 16
    cw = cell * scale
    sheet = Image.new("RGBA", (per_row * (cw + pad) + pad,
                               6 * (cw + lab + pad) + pad), (18, 18, 22, 255))
    d = ImageDraw.Draw(sheet)
    for idx in range(96):
        r, c = divmod(idx, COLS)
        sx, sy = c * cell, y0 + r * PITCH
        cut = im.crop((sx, sy, sx + cell, sy + cell)).resize((cw, cw), Image.NEAREST)
        px = (idx % per_row) * (cw + pad) + pad
        py = (idx // per_row) * (cw + lab + pad) + pad
        sheet.alpha_composite(cut, (px, py))
        ch = char_of(r, c)
        d.text((px, py + cw + 1), '-' if ch == ' ' else ch, fill=(150, 220, 150, 255))
        d.rectangle([px, py, px + cw - 1, py + cw - 1], outline=(70, 70, 80, 255))
    sheet.convert("RGB").save(SHEET)
    return SHEET


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--src", help="ascii_1280.png 的副本（默认自动找 local/th19*/…）")
    ap.add_argument("--dry-run", action="store_true", help="只量几何 + 打印对照表，不落盘")
    a = ap.parse_args()

    src = Path(a.src) if a.src else find_source()
    print(f"源图：{src}")
    im = Image.open(src).convert("RGBA")
    print(f"尺寸：{im.size[0]}×{im.size[1]}")
    y0, cell, boxes = measure(im)
    self_check(boxes)

    rows = []
    for r in range(3):
        rows.append("".join(char_of(r, c) for c in range(COLS)).rstrip())
    for r, line in enumerate(rows):
        print(f"  第 {r} 行（{len(line):2d} 字）：{line}")

    if a.dry_run:
        print("--dry-run：不落盘")
        return 0
    n = write_cells(im, y0, cell)
    sheet = write_sheet(im, y0, cell)
    print(f"写 {OUT.relative_to(REPO)}：{n} 张 {cell}×{cell}（格号 C00..C95）")
    print(f"写 {sheet.relative_to(REPO)}：对照图（×3）")
    print(f"⇒ ORDER.txt 图集段应为：ATLAS ATLAS_POFV109_ASC {im.size[0]}x{PITCH * 3}@{COLS}x3"
          f"（{n} 个成员，每个 {cell}x{cell}）")
    return 0


if __name__ == "__main__":
    sys.exit(main())
