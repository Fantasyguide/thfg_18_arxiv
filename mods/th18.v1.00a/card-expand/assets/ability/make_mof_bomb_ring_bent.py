#!/usr/bin/env python3
"""把 TH10 灵击的**竖条**(`sprite362`, 14x128) **极坐标弯成一张环** —— 供 110/111 的灵击环用。

    python3 make_mof_bomb_ring_bent.py          # 出图 + 打印要 append 到 entries/ORDER.txt 的两行
    python3 make_mof_bomb_ring_bent.py --dump   # 附带把结果打成 ASCII（核对几何用）

■ 为什么要烘这张图（用户 2026-09-23 实跑口径，逐字）
  「灵击动画依旧不对，**它不是32段**，然后内外还会发射红色和蓝色的枫叶特效，
    放的时候还会有**几个扩大的紫色虚影环**」

  一手（TH10 `bullet.anm` script394–397，逐字）四条环脚本都是
  `blendMode(1); texCircle(32); layer(13); sprite(sprite362); angleVel(0,0,±…);
   scale(16,16); scaleTime(60, …); [color(255,0,255) 仅 396/397]` ⇒ `texCircle(32)` = 把这条竖条
  **沿圆周无缝铺 32 份**（不是 32 个互相错位的断片）。TH18 的 anm VM 是否执行 `texCircle` 没有先例
  （`AUDIT.md` 早就这么标，实跑症状 = 那条 VM 把竖条按 scale 放大成**一条巨大竖光晕**）
  ⇒ 我们**在构建期把它弯好**：极坐标映射 + **沿圆周铺 `TILES = 32` 份**（= 一手那个 32），
  竖条的**长边(128)**沿圆周、**短边(14)**铺成环的厚度；每份之间首尾相接 ⇒ 一张**连续**的环贴图
  （没有缝隙、没有 32 个独立 VM 的错位 —— 用户否掉的是**后者**）。
  ⚠ 竖条自己的花纹沿长边有 **3 个周期**（峰值行 8 / 58 / 107，周期 ≈ 49.5 px）⇒ 铺 32 份之后
  一圈上共 **96 道电弧**，这才是 TH10 那条「电流边」的密度。

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
TILES = 32        # 沿圆周铺几份（★ 一手 `texCircle(32)` 的那个 32）
SS = 3            # 每个目标像素的超采样边长（3 → 9 个样本，兼作缩小的 box 滤波：竖条 128 px
                  # 要压进 2π·100/32 ≈ 19.6 px 的弧长里，不滤波会走样成噪点）


def bake(strip: Image.Image) -> Image.Image:
    """极坐标弯环：输出 (r, θ) → 源图 (u=沿圆周, v=径向)。

    沿圆周**铺 `TILES` 份**（= 一手 `texCircle(32)`）：`u` 走到竖条长边末尾就从 0 重来。
    每个目标像素在它的 1/SS 网格上取 SS×SS 个样本做平均（预乘 alpha），
    这样「128 px 压进 ≈19.6 px」这一步是带滤波的缩小，而不是抽样噪点。"""
    sw, sh = strip.size                      # (14, 128)
    src = strip.load()
    im = Image.new("RGBA", (CANVAS, CANVAS), (0, 0, 0, 0))
    dst = im.load()
    cx = cy = (CANVAS - 1) / 2.0
    r_in, r_out = R0 - THICK / 2.0, R0 + THICK / 2.0
    span = sh * TILES                        # 一圈上的「竖条长度」总量
    inv = 1.0 / (SS * SS)
    for y in range(CANVAS):
        for x in range(CANVAS):
            acc = [0.0, 0.0, 0.0, 0.0]
            for sy_i in range(SS):
                for sx_i in range(SS):
                    fx = x + (sx_i + 0.5) / SS - cx
                    fy = y + (sy_i + 0.5) / SS - cy
                    rr = math.hypot(fx, fy)
                    if rr < r_in or rr >= r_out:
                        continue
                    ang = math.atan2(fy, fx)
                    if ang < 0:
                        ang += 2 * math.pi
                    u = ang / (2 * math.pi) * span
                    v = (rr - r_in) / THICK * sw
                    sx = min(sw - 1, int(v))
                    sy = int(u) % sh             # ★ 铺 TILES 份：长边走完一圈就回头
                    r_, g_, b_, a_ = src[sx, sy]
                    w = a_ / 255.0
                    acc[0] += r_ * w; acc[1] += g_ * w; acc[2] += b_ * w; acc[3] += w
            if acc[3] <= 0.0:
                continue
            a = acc[3] * inv
            dst[x, y] = (int(acc[0] * inv / a + 0.5), int(acc[1] * inv / a + 0.5),
                         int(acc[2] * inv / a + 0.5), int(a * 255.0 + 0.5))
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
    print(f"mof_bomb: {NAME} {CANVAS}x{CANVAS} 不透明 {nz} 像素（基准半径 {R0:.0f} px、厚度 {THICK:.0f} px、"
          f"沿圆周铺 {TILES} 份 = 一手 `texCircle({TILES})`）")
    print("把下面两行 append 到 entries/ORDER.txt：")
    print(f"ATLAS ATLAS_MOF_BOMB_RING_BENT {CANVAS}x{CANVAS}")
    print(f"MOF_BOMB_RING_BENT     ability/mof_bomb/{NAME}     @0,0,{CANVAS}x{CANVAS}")
    if a.dump:
        dump(ring)
    return 0


if __name__ == "__main__":
    sys.exit(main())
