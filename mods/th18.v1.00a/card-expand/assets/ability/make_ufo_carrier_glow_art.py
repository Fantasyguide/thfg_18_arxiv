#!/usr/bin/env python3
"""112 星莲船 —— **UFO 令牌宿主：黄辉光妖精**（TH18 原生 `enemy.anm` entry4 `enemy_g.png` 的那一行）切图。

    python3 assets/ability/make_ufo_carrier_glow_art.py     # 出 12 张 + 打印要 append 的 ORDER 片段
    make -C native anm                                       # 把片段拼进 ability.anm

=====================================================================================
为什么有这支脚本（**这是一个真 bug 的收尾**）
=====================================================================================

■ 现象（owner 2026-10-03 报）
  「th12 改的**黄色妖精图出问题了**」——112 的宿主妖精换皮之后贴图不对。

■ 根因（✅ 一手，两套编号空间串了；**不是容量爆了**）
  那三支脚本（`352_ufo_carrier_fairy` / `356_..._side_r` / `357_..._side_l`，都住在
  **`ability.anm`**）在 2026-09-26 被改成写死 `sprite(256..267)`，注释说这些号是
  「TH18 原生 **`enemy.anm`** entry4 = `enemy_g.png` 的 y=416 行」。

  但 `sprite(N)` 是在**本文件**（ability.anm）的 sprite 表里查的，而我们 `ability.anm` 的 256：
  - `entry32 ATLAS_ANIMAL_64`（sprite **222…264**）= **117 鬼形兽的动物灵图集**
  - 265…267 落在 `entry33 ATLAS_ANIMAL_SMALL`（265…279）
  ⇒ 于是「黄辉光妖精」实际画出来是**动物灵**。（同类错误上一轮也犯过：更早写成 `sprite(84..87)`
  = TH12 `enemy.anm` 的号，同样串空间。）

  ⚠ 容量侧是干净的：本次修复前 `ability.anm` = **entry 102/256 · sprite 604**，离墙还远。

■ 修法（本脚本）
  把 TH18 原生那一行**切进我们自己的图集**（`ATLAS_UFO_CARRIER_FAIRY_GLOW`，12 帧 32×32），
  脚本改用 `@CARRIER_FAIRY_GLOW_N@`（构建期由 `build_ability.py` 解析成**本文件**的 sprite 号）
  ⇒ 从此不可能再串空间（宏名/编号由构建期现算，见 `native/anm_ids.h` 的 `CE_ANM_ABILITY_SPRITE_*`）。

  一手（原生 `enemy.anm` 的 sprite 表，`local/th18.v1.00a/anm/enemy/enemy.anm.txt`）：
  `sprite256..267` = `{x: 0/32/…/352, y: 416, w: 32, h: 32}` —— 一行 12 帧：
  **256..259 = 正面 4 帧**（原生 script199 用）、**260..267 = 转身 8 帧**（原生 script202/203 用）。
  本脚本**不重画、不缩放**，矩形从原生 anm 文本读，逐张断言 32×32（与 `make_ufo_carrier_art.py` 同纪律）。
"""
import re
import sys
from pathlib import Path

from PIL import Image

HERE = Path(__file__).resolve().parent
REPO = HERE.parents[4]

SRC_ANM = REPO / "local/th18.v1.00a/anm/enemy/enemy.anm.txt"
SRC_ENTRY = "enemy/enemy_g.png"
OUT_DIR = HERE / "ufo_carrier_glow"
SPRITES = list(range(256, 268))          # 一手：黄辉光妖精那一行（正面 256..259 / 转身 260..267）
NAME = "CARRIER_FAIRY_GLOW"
ATLAS = "ATLAS_UFO_CARRIER_FAIRY_GLOW"


def parse_sprite_rects(text: str) -> dict:
    """从原生 anm 文本里抓 `spriteN: { x: .., y: .., w: .., h: .. }`（按当前 entry 归属）。"""
    out = {}
    entry = None
    for line in text.splitlines():
        m = re.match(r"\s*entry\s+entry(\d+)\s*\{", line)
        if m:
            continue
        m = re.search(r'name:\s*"([^"]+)"', line)
        if m and m.group(1).endswith((".png", ".jpg")):
            entry = m.group(1)
            continue
        m = re.match(r"\s*sprite(\d+):\s*\{\s*x:\s*(-?\d+),\s*y:\s*(-?\d+),\s*w:\s*(\d+),\s*h:\s*(\d+)", line)
        if m and entry:
            out[(entry, int(m.group(1)))] = tuple(int(m.group(i)) for i in range(2, 6))
    return out


def main() -> int:
    if not SRC_ANM.exists():
        print(f"✖ 找不到原生 anm 文本：{SRC_ANM}", file=sys.stderr)
        print("  （TH18 的 anm 没解包时先 `thdat -x 18 … enemy.anm`）", file=sys.stderr)
        return 1

    rects = parse_sprite_rects(SRC_ANM.read_text(encoding="utf-8", errors="replace"))
    src_png = SRC_ANM.parent / SRC_ENTRY
    if not src_png.is_file():
        print(f"✖ 找不到源图：{src_png}", file=sys.stderr)
        return 1
    sheet = Image.open(src_png).convert("RGBA")
    OUT_DIR.mkdir(parents=True, exist_ok=True)

    made = []
    for k, num in enumerate(SPRITES):
        rect = rects.get((SRC_ENTRY, num))
        if not rect:
            print(f"✖ 原生 anm 的 entry {SRC_ENTRY} 里没有 sprite{num}", file=sys.stderr)
            return 1
        x, y, w, h = rect
        if (w, h) != (32, 32):
            print(f"✖ sprite{num} 尺寸 {w}×{h} ≠ 32×32（原生表变了吗？）", file=sys.stderr)
            return 1
        out = OUT_DIR / f"{NAME}_{k}.png"
        sheet.crop((x, y, x + w, y + h)).save(out)
        made.append(out.name)
    print(f"✓ 切出 {len(made)} 张 → {OUT_DIR.relative_to(HERE)}")

    print("\n把下面这段 **append** 到 entries/ORDER.txt（末尾追加；既有 entry/sprite 号零位移）：\n")
    print("# 112 的宿主妖精：TH18 原生 enemy_g.png y=416 那行（黄辉光妖精，12 帧 32×32）")
    print("#   ⚠ 为什么必须切进来：脚本住在 ability.anm，`sprite(N)` 查的是**本文件**的表 ——")
    print("#     写原生 enemy.anm 的号（256..267）会落到 117 的 ATLAS_ANIMAL_64 上（真 bug，已修）。")
    print(f"ATLAS {ATLAS} 384x32")
    for k in range(len(SPRITES)):
        print(f"{NAME}_{k:<19} ability/ufo_carrier_glow/{NAME}_{k}.png   @{32 * k},0,32x32")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
