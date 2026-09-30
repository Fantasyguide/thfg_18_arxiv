#!/usr/bin/env python3
"""顶部三角指示（画面外指示）的**统一贴图** —— 卡 107 / 110 / 111 共用。

    python3 make_top_indicator_art.py            # 出图 + 打印要 append 到 entries/ORDER.txt 的那一行
    python3 make_top_indicator_art.py --dump     # 附带把这一格打成 ASCII（核证据用，只读源图）

=====================================================================================
一手考证：这个三角形到底是什么、为什么用 TH07 那张
=====================================================================================

■ 结论（✅ 一手，逐条可复核）

  | 项 | 值 | 一手 |
  | --- | --- | --- |
  | 机制 | 「道具在画面顶边之外 ⇒ 顶边画一个三角指示」是**死代码**所在的那套；TH18 的扳机拿不到输入 | `engine/player/th18/06-offscreen-top-indicator-absent.md` |
  | 活的那份 | **TH07 v1.00b**（卡 107 的原作）| `th07:0x433d35` 比**道具自己的 `pos.y`** vs `th07:0x498c70` = `-8.0` |
  | 贴图 | TH07 `etama.anm` entry1 `etama2.png` 的 `(240,64,16,16)` | 逐像素：67 px、行宽 `2,3,4,6,6,8,8,10,10,10`、平均色 **(254,192,253)**（近白偏粉）|
  | 同形 | 与 **TH18 自己** `bullet.anm` 的 `sprite501/503`（`bullet/item.png` 的 `(192,16,16,16)`/`(208,16,16,16)`）**同形** | 行宽同为 `2,3,4,6,6,8,8,10,10,10`；TH18 那两张分别是红/黄，TH07 这张近白 |

■ 为什么用 TH07 这张（而不是 TH18 自己的 red/yellow 两张）

  1. **它是活的那份的原贴图**（107 就是妖妖梦）⇒ 复刻 107 时用的是原作素材；
  2. **近白 ⇒ 可以用顶点色染**：TH18 的 VM `color_1`（`+0x524`）是**调制色**，
     白底染成信仰点的绿（110/111）、或原样当 107 的粉白 ⇒ **一张贴图服务三张卡**（统一修改的要点）；
  3. 尺寸 **16×16** 与 TH07 的 `pos`/alpha 配方（`y = 8.0` + 距离式 alpha）是配套的。

■ 脚本里怎么用

  `assets/ability/scripts/375_top_indicator.anm.txt`：
  `sprite(@TOP_INDICATOR); originMode(1); type(0); static();` —— **逐条照 TH18 原生脚本 134 的正文**
  （TH18 的三角脚本 = `sprite(...); originMode(1); type(0); static();`，静态无动画）。
"""

import argparse
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
REPO = HERE.parents[4]

SRC = REPO / "local/th07.v1.00b/anm/etama/data/etama/etama2.png"
RECT = (240, 64, 16, 16)          # 一手：TH07 etama.anm entry1 的 sprite（(240,64,16,16)）
OUT = HERE / "top_indicator/TOP_INDICATOR.png"
NAME = "TOP_INDICATOR"
ORDER_LINE = f"{NAME}  ability/top_indicator/{NAME}.png  16x16"


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--dump", action="store_true", help="把这一格打成 ASCII（核证据用）")
    a = ap.parse_args()

    if not SRC.is_file():
        print(f"✗ 源图不在：{SRC.relative_to(REPO)}", file=sys.stderr)
        print("  （TH07 的 anm 没有解包时先跑 thdat -x 7 … etama.anm）", file=sys.stderr)
        return 1

    from PIL import Image  # noqa: PLC0415

    im = Image.open(SRC).convert("RGBA")
    x, y, w, h = RECT
    tile = im.crop((x, y, x + w, y + h))

    px = [p for p in tile.getdata() if p[3] > 32]
    rows = [sum(1 for xx in range(w) if tile.getpixel((xx, yy))[3] > 32) for yy in range(h)]
    avg = tuple(round(sum(p[i] for p in px) / len(px)) for i in range(3)) if px else None
    print(f"源      : {SRC.relative_to(REPO)} {im.size}  矩形 {RECT}")
    print(f"三角    : {len(px)} px  行宽 {[r for r in rows if r]}  平均色 {avg}")

    if a.dump:
        for yy in range(h):
            print("".join("#" if tile.getpixel((xx, yy))[3] > 32 else "." for xx in range(w)))

    OUT.parent.mkdir(parents=True, exist_ok=True)
    tile.save(OUT)
    print(f"写出    : {OUT.relative_to(HERE)}")
    print("\n把下面这一行 **append** 到 entries/ORDER.txt（末尾追加，别插队 ⇒ 不动既有 entry/sprite 号）：")
    print("    " + ORDER_LINE)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
