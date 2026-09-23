#!/usr/bin/env python3
"""把 TH10 灵击的**竖条**(`sprite362`, 14x128) **极坐标弯成一张环** —— 供 110/111 的灵击环用。

    python3 make_mof_bomb_ring_bent.py          # 出图 + 打印要 append 到 entries/ORDER.txt 的两行
    python3 make_mof_bomb_ring_bent.py --dump   # 附带把结果打成 ASCII（核对几何用）

■ 为什么要烘这张图（用户 2026-09-23 实跑口径，逐字）
  「灵击动画依旧不对，**它不是32段**，然后内外还会发射红色和蓝色的枫叶特效，
    放的时候还会有**几个扩大的紫色虚影环**」

  一手（TH10 `bullet.anm` script394–397，逐字）四条环脚本都是
  `blendMode(1); texCircle(32); layer(13); sprite(sprite362); angleVel(0,0,±…);
   scale(16,16); scaleTime(60, …); [color(255,0,255) 仅 396/397]` ⇒ **连续的一条环**（竖条弯一圈），
  而不是 32 个断片。TH18 的 anm VM 是否执行 `texCircle` 没有先例（`AUDIT.md` 早就这么标，
  实跑症状 = 那条 VM 把竖条按 scale 放大成**一条巨大竖光晕**）⇒ 我们**在构建期把它弯好**：
  极坐标映射 —— 竖条的**长边(128)**铺成圆周、**短边(14)**铺成环的厚度。

■ 几何（唯一约定，卡侧按它算 scale）
  贴图 = 256x256，环心在正中，**基准半径 `R0 = 100 px`、厚度 `14 px`**。
  ⇒ 卡侧要「半径 R px 的环」就写 `scale = R / 100`（脚本 302–305 里的 scale 就是原版的
  16/160/200 ÷ 100）。

■ 源图
  `local/th10.v1.00a/anm/bullet/bullet.png` 的 sprite362 那一格（(65,0,14,128)）——
  与 `MOF_BOMB_RING_STRIP.png` 同源（那个文件是 `make_mof_bomb_art.py` 出的 1:1 切图）。
"""
import argparse
import math
import sys
from pathlib import Path

from PIL import Image

HERE = Path(__file__).resolve().parent
OUT = HERE / "mof_bomb"
STRIP = OUT / "MOF_BOMB_RING_STRIP.png"        # 14x128，由 make_mof_bomb_art.py 生成
NAME = "MOF_BOMB_RING_BENT.png"
CANVAS = 256
R0 = 100.0        # 基准半径（px）
THICK = 14.0      # 环的厚度（px）= 竖条的短边


def bake(strip: Image.Image) -> Image.Image:
    """极坐标弯环：输出 (r, θ) → 源图 (v=径向偏移, u=沿圆周)。"""
    sw, sh = strip.size                      # (14, 128)
    src = strip.load()
    im = Image.new("RGBA", (CANVAS, CANVAS), (0, 0, 0, 0))
    dst = im.load()
    cx = cy = (CANVAS - 1) / 2.0
    r_in, r_out = R0 - THICK / 2.0, R0 + THICK / 2.0
    for y in range(CANVAS):
        for x in range(CANVAS):
            dx, dy = x - cx, y - cy
            rr = math.hypot(dx, dy)
            if rr < r_in or rr >= r_out:
                continue
            ang = math.atan2(dy, dx)
            if ang < 0:
                ang += 2 * math.pi
            u = ang / (2 * math.pi) * sh        # 沿圆周 → 竖条的长边
            v = (rr - r_in) / THICK * sw        # 径向 → 竖条的短边
            sx, sy = min(sw - 1, int(v)), min(sh - 1, int(u))
            dst[x, y] = src[sx, sy]
    return im


def dump(im: Image.Image):
    px = im.load()
    print(f"\n--- {NAME} {im.size} 的 ASCII（每 4x4 取一格）---")
    for y in range(0, CANVAS, 4):
        row = ""
        for x in range(0, CANVAS, 4):
            a = px[x, y][3]
            row += " " if a < 32 else ("." if a < 96 else ("o" if a < 192 else "#"))
        print("  " + row)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--dump", action="store_true")
    a = ap.parse_args()
    if not STRIP.exists():
        sys.exit(f"缺 {STRIP}（先跑 make_mof_bomb_art.py）")
    strip = Image.open(STRIP).convert("RGBA")
    if strip.size != (14, 128):
        sys.exit(f"{STRIP.name} 尺寸 {strip.size} ≠ (14, 128)")
    OUT.mkdir(exist_ok=True)
    ring = bake(strip)
    ring.save(OUT / NAME)
    nz = sum(1 for p in ring.getdata() if p[3] > 0)
    print(f"mof_bomb: {NAME} {CANVAS}x{CANVAS} 不透明 {nz} 像素（基准半径 {R0:.0f} px、厚度 {THICK:.0f} px）")
    print("把下面两行 append 到 entries/ORDER.txt：")
    print(f"ATLAS ATLAS_MOF_BOMB_RING_BENT {CANVAS}x{CANVAS}")
    print(f"MOF_BOMB_RING_BENT     ability/mof_bomb/{NAME}     @0,0,{CANVAS}x{CANVAS}")
    if a.dump:
        dump(ring)
    return 0


if __name__ == "__main__":
    sys.exit(main())
