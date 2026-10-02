#!/usr/bin/env python3
"""112 星莲船 —— **UFO 令牌宿主：妖精皮**（普通黄 + 辉光黄，各 12 帧）切图。

    python3 assets/ability/make_ufo_carrier_fairy_art.py    # 出 24 张 + 打印要 append 的 ORDER 片段
    make -C native anm                                       # 把片段拼进 ability.anm

=====================================================================================
为什么有这支脚本（**两轮真 bug 的收尾**）
=====================================================================================

■ 第一轮（2026-10-03，已修）：**两套编号空间串了**，不是容量爆
  脚本住在 `ability.anm`，却写死 `sprite(256..267)` —— 那是**原生 `enemy.anm`** 的号。
  `ability.anm` 里 256..264 实际是 `entry32 ATLAS_ANIMAL_64`（117 的动物灵图集）
  ⇒ 黄妖精画成了动物灵。修法 = 把原生那行**切进本文件图集**，脚本改用 `@宏名@`
  （构建期解析成本文件的号，`anmlib.py` 对未定义占位直接 SystemExit）。见 `AUDIT.md` §BE 详录 #219。

■ 第二轮（owner 2026-10-03 实跑）：「**辉光黄妖精当且仅当辉光妖精**」
  辉光那一套（`enemy_g.png` 的四行）在原版里是**一组**：只有本来就是**辉光妖精**的那种妖精
  （原生脚本 **184/189/194/199** = 蓝/红/绿/黄辉光，宿主表里就有 `S=184/189`）才穿辉光皮。
  我们在所有关卡都给宿主穿辉光黄 ⇒ 出现了「非辉光妖精却发着辉光」的错。
  ⇒ 本脚本改出**两套**皮：**普通黄**（`enemy.png` y=416 行 = 原生 sprite36..47，脚本 15/16..19）
  与**辉光黄**（`enemy_g.png` y=416 行 = 原生 sprite256..267，脚本 199/200..203）；
  C 侧按**宿主自己的那一行**挑（宿主表新加的 `glow` 字段，见 `gen_ufo_carrier_table.py`）。

一手对照（`local/th18.v1.00a/anm/enemy/enemy.anm.txt`，两行**结构逐位平行**）：

| 变体 | 正面 4 帧 | 转身 8 帧 | 侧身 | 出处 |
| --- | --- | --- | --- | --- |
| 普通黄 | `sprite36..39`（script15）| `sprite40..47`（script16/17）| script18/19 | `enemy.png` **y=416** |
| 辉光黄 | `sprite256..259`（script199）| `sprite260..267`（script200/201）| script202/203 | `enemy_g.png` **y=416** |

（辉光 = 普通 **+220** sprite、**+184** script；两支都**不重画不缩放**，矩形从原生 anm 文本读，逐张断言 32×32。）
"""
import re
import sys
from pathlib import Path

from PIL import Image

HERE = Path(__file__).resolve().parent
REPO = HERE.parents[4]

SRC_ANM = REPO / "local/th18.v1.00a/anm/enemy/enemy.anm.txt"

# (皮名前缀, 图集名, 源 sheet, 源 sprite 号 12 个)
SETS = [
    ("CARRIER_FAIRY_PLAIN", "ATLAS_UFO_CARRIER_FAIRY_PLAIN",
     "enemy/enemy.png",   list(range(36, 48)),   "普通黄（非辉光妖精用）"),
    ("CARRIER_FAIRY_GLOW",  "ATLAS_UFO_CARRIER_FAIRY_GLOW",
     "enemy/enemy_g.png", list(range(256, 268)), "辉光黄（本来就辉光的妖精用）"),
]


def parse_sprite_rects(text: str) -> dict:
    """从原生 anm 文本里抓 `spriteN: { x: .., y: .., w: .., h: .. }`（按当前 sheet 归属）。"""
    out = {}
    sheet = None
    for line in text.splitlines():
        m = re.search(r'name:\s*"([^"]+)"', line)
        if m and m.group(1).endswith((".png", ".jpg")):
            sheet = m.group(1)
            continue
        m = re.match(r"\s*sprite(\d+):\s*\{\s*x:\s*(-?\d+),\s*y:\s*(-?\d+),\s*w:\s*(\d+),\s*h:\s*(\d+)", line)
        if m and sheet:
            out[(sheet, int(m.group(1)))] = tuple(int(m.group(i)) for i in range(2, 6))
    return out


def main() -> int:
    if not SRC_ANM.exists():
        print(f"✖ 找不到原生 anm 文本：{SRC_ANM}", file=sys.stderr)
        print("  （TH18 的 anm 没解包时先 `thdat -x 18 … enemy.anm`）", file=sys.stderr)
        return 1

    rects = parse_sprite_rects(SRC_ANM.read_text(encoding="utf-8", errors="replace"))
    frag = []
    for prefix, atlas, sheet, sprites, why in SETS:
        src_png = SRC_ANM.parent / sheet
        if not src_png.is_file():
            print(f"✖ 找不到源图：{src_png}", file=sys.stderr)
            return 1
        out_dir = HERE / ("ufo_carrier_glow" if "GLOW" in prefix else "ufo_carrier_plain")
        out_dir.mkdir(parents=True, exist_ok=True)
        img = Image.open(src_png).convert("RGBA")
        for k, num in enumerate(sprites):
            rect = rects.get((sheet, num))
            if not rect:
                print(f"✖ {sheet} 里没有 sprite{num}", file=sys.stderr)
                return 1
            x, y, w, h = rect
            if (w, h) != (32, 32):
                print(f"✖ sprite{num} 尺寸 {w}×{h} ≠ 32×32（原生表变了吗？）", file=sys.stderr)
                return 1
            img.crop((x, y, x + w, y + h)).save(out_dir / f"{prefix}_{k}.png")
        print(f"✓ {atlas}：切出 {len(sprites)} 张（{why}）→ {out_dir.relative_to(HERE)}")
        frag.append(f"# 112 的宿主妖精皮：{why}；一手 = {sheet} "
                    f"({'/'.join(str(s) for s in sprites[:4])}… = y={rects[(sheet, sprites[0])][1]})")
        frag.append(f"ATLAS {atlas} 384x32")
        rel = "ufo_carrier_glow" if "GLOW" in prefix else "ufo_carrier_plain"
        for k in range(len(sprites)):
            frag.append(f"{prefix}_{k:<19} ability/{rel}/{prefix}_{k}.png   @{32 * k},0,32x32")
        frag.append("")

    print("\n把下面这段 **append** 到 entries/ORDER.txt（末尾追加；既有 entry/sprite 号零位移）：\n")
    print("\n".join(frag))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
