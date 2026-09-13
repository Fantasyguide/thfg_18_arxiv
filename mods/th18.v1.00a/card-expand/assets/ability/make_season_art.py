#!/usr/bin/env python3
"""季节（id 119）的子机贴图，程序生成（固定种子，重跑逐字节一致）。

    python3 make_season_art.py

  season/SEASON_OPT.png   64×64   季节子机本体：一颗「季节点」——白芯 → 金 → 绿 的球，
                                  外面一圈细环（槽），右上角一片小叶子。
                                  由 `Player__allocate_option` 起，脚本 113 把它缩到场上约 27 px。
  season/SEASON_SEED.png  32×32   子机打出去的小种子（shooter 的 anm 字段指向的脚本 114 用它）。
  season/PT_{SPRING,SUMMER,FALL,WINTER,DOYOU}.png  16×16   ★季节点本身（脚本 115）——
                                  直接从 TH16 原版 `bullet/item.png` 切：**右上 2×2 = 春/夏(上行)、秋/冬(下行)**，
                                  再加一行 (224,32) = 土用，切片 16×16 与 P 点 / 蓝点同尺寸
                                  （一手：TH16 `0x430cb7 add eax,0x81` → 脚本号 = 129 + SUBSEASON）。

画法：距离场 → 平滑掩码 → 按法线做球面着色，无外部素材（与 make_peashooter_art.py 同一套）。
"""
import math
from pathlib import Path

import numpy as np
from PIL import Image

HERE = Path(__file__).resolve().parent
OUT = HERE / "season"
SEED = 0x1977 + 119

# 季节色：芯白 → 金 → 叶绿；环用金
CORE = (255, 255, 232)
GOLD = (255, 206, 92)
LEAF = (110, 196, 84)
LEAF_DARK = (58, 132, 52)


def _grid(w, h):
    y, x = np.mgrid[0:h, 0:w]
    return x.astype(np.float64), y.astype(np.float64)


def _smooth(edge0, edge1, v):
    t = np.clip((v - edge0) / (edge1 - edge0), 0.0, 1.0)
    return t * t * (3.0 - 2.0 * t)


def _save(name, rgb, alpha):
    img = np.dstack([np.clip(rgb, 0, 255), np.clip(alpha * 255.0, 0, 255)]).astype(np.uint8)
    OUT.mkdir(exist_ok=True)
    Image.fromarray(img, "RGBA").save(OUT / name)


def make_opt():
    w = h = 64
    x, y = _grid(w, h)
    cx = cy = 31.5
    dx, dy = (x - cx) / 31.5, (y - cy) / 31.5
    r = np.hypot(dx, dy)

    orb = 1.0 - _smooth(0.86, 0.98, r)            # 球体
    ring = _smooth(0.97, 1.0, r) - _smooth(1.0, 1.06, r)   # 细环
    ring = np.clip(ring, 0.0, 1.0)

    # 球面法线（用于着色）
    nz = np.sqrt(np.clip(1.0 - np.clip(r, 0, 1) ** 2, 0.0, 1.0))
    nx, ny = dx / np.maximum(r, 1e-6) * nz, dy / np.maximum(r, 1e-6) * nz

    # 芯 → 金 → 绿的同心色带
    t = np.clip(r / 0.9, 0, 1)
    base = np.zeros((h, w, 3))
    for c in range(3):
        a = np.where(t < 0.45, CORE[c], GOLD[c])
        b = LEAF[c]
        wgt = np.clip((t - 0.45) / 0.55, 0, 1)
        base[..., c] = a * (1 - wgt) + b * wgt
    lam = np.clip(nx * -0.55 + ny * -0.75, 0, 1)
    rgb = base * (0.62 + 0.38 * lam[..., None])

    # 右上角的叶子
    lx, ly = (x - 44.0) / 16.0, (y - 17.0) / 8.0
    ang = math.radians(-28)
    lx2 = lx * math.cos(ang) - ly * math.sin(ang)
    ly2 = lx * math.sin(ang) + ly * math.cos(ang)
    leaf = 1.0 - _smooth(0.85, 1.05, np.hypot(lx2, ly2 * 1.6))
    leaf = np.clip(leaf - orb, 0, 1)              # 球体盖住的部分不画
    leaf_rgb = np.zeros((h, w, 3))
    for c in range(3):
        leaf_rgb[..., c] = LEAF_DARK[c] * 0.55 + LEAF[c] * 0.45
    # 叶脉
    vein = (1.0 - _smooth(0.05, 0.12, np.abs(ly2))) * leaf
    leaf_rgb = leaf_rgb * (1 - 0.45 * vein[..., None])

    alpha = np.clip(orb + ring * 0.9 + leaf, 0, 1)
    rgb = np.where((leaf > 0)[..., None], leaf_rgb, rgb)
    ring_rgb = np.zeros((h, w, 3))
    for c in range(3):
        ring_rgb[..., c] = GOLD[c]
    rgb = np.where((ring > 0)[..., None], ring_rgb * (0.55 + 0.45 * nz[..., None]), rgb)
    _save("SEASON_OPT.png", rgb, alpha)


def make_seed():
    w = h = 32
    x, y = _grid(w, h)
    dx, dy = (x - 15.5) / 15.0, (y - 15.5) / 15.0
    r = np.hypot(dx, dy)
    body = 1.0 - _smooth(0.62, 0.80, r)
    nz = np.sqrt(np.clip(1.0 - np.clip(r, 0, 1) ** 2, 0.0, 1.0))
    nx, ny = dx / np.maximum(r, 1e-6) * nz, dy / np.maximum(r, 1e-6) * nz
    lam = np.clip(nx * -0.55 + ny * -0.75, 0, 1)
    base = np.zeros((h, w, 3))
    t = np.clip(r / 0.8, 0, 1)
    for c in range(3):
        base[..., c] = CORE[c] * (1 - t) + LEAF[c] * t
    rgb = base * (0.6 + 0.4 * lam[..., None])
    _save("SEASON_SEED.png", rgb, body)


# —— 季节点：从 TH16 原版道具图集切 5 个 16×16 单元（原版字节只在 local/，不入库；构建时取）——
# 对应关系（一手）：sprite519=(224,0) 春 / 520=(240,0) 夏 / 521=(224,16) 秋 / 522=(240,16) 冬 /
# 523=(224,32) 土用；TH16 用 `脚本号 = 129 + SUBSEASON` 选（`0x430cb7 add eax,0x81`）。
REPO = HERE.parents[4]
SRC_ITEM = REPO / "local/th16.v1.00a/anm/bullet/bullet/item.png"
SRC_FRONT = REPO / "local/th16.v1.00a/anm/front/front/front00.png"
POINT_CELLS = (
    ("PT_SPRING", 224, 0),
    ("PT_SUMMER", 240, 0),
    ("PT_FALL",   224, 16),
    ("PT_WINTER", 240, 16),
    ("PT_DOYOU",  224, 32),
)


def make_points():
    if not SRC_ITEM.is_file():
        raise SystemExit(f"缺 TH16 原版道具图集 {SRC_ITEM}（unpack 自 th16.dat，见 local/README.md）")
    atlas = Image.open(SRC_ITEM).convert("RGBA")
    if atlas.size != (256, 64):
        raise SystemExit(f"道具图集尺寸变了：{atlas.size}（期望 256x64）")
    OUT.mkdir(exist_ok=True)
    for name, x, y in POINT_CELLS:
        atlas.crop((x, y, x + 16, y + 16)).save(OUT / f"{name}.png")


# —— 左下角计量槽的季节字（TH16 `front.anm` entry0 的 sprite82..86，y=80 一行 5 个 32×32）——
# 一手：TH16 用 `sprite = 82 + SUBSEASON` 选字（`th16:0x4276e1 add edx,0x52`）。
# —— 季节子机本体（TH16 的「小球」）—— 在**自机贴图**里，副 anm entry1 的 xOffset/yOffset + sprite 矩形：
#   spring pl00sub sprite14 / summer pl02sub sprite4 / fall pl03sub sprite4 / winter pl01sub sprite3（script18）/
#   doyou pl04sub sprite6。一手：`PlayerInner__repopulate_options` 按 `DAT_00492be0[SUBSEASON]` 选脚本。
OPT_CELLS = (
    ("OPT_SPRING", "local/th16.v1.00a/anm/pl00sub/player/pl00/pl00.png", 96, 144),
    ("OPT_SUMMER", "local/th16.v1.00a/anm/pl02sub/player/pl02/pl02.png", 64, 144),
    ("OPT_FALL",   "local/th16.v1.00a/anm/pl03sub/player/pl03/pl03.png", 128, 144),
    ("OPT_WINTER", "local/th16.v1.00a/anm/pl01sub/player/pl01/pl01.png", 80, 160),
    ("OPT_DOYOU",  "local/th16.v1.00a/anm/pl04sub/player/pl04/pl00.png", 144, 144),
)


def make_opts():
    OUT.mkdir(exist_ok=True)
    for name, rel, x, y in OPT_CELLS:
        src = REPO / rel
        if not src.is_file():
            raise SystemExit(f"缺 TH16 原版自机贴图 {src}")
        im = Image.open(src).convert("RGBA")
        im.crop((x, y, x + 16, y + 16)).save(OUT / f"{name}.png")


GLYPH_CELLS = (
    ("GLYPH_SPRING", 832, 80),
    ("GLYPH_SUMMER", 864, 80),
    ("GLYPH_FALL",   896, 80),
    ("GLYPH_WINTER", 928, 80),
    ("GLYPH_DOYOU",  960, 80),
)


def make_glyphs():
    if not SRC_FRONT.is_file():
        raise SystemExit(f"缺 TH16 原版 front00.png {SRC_FRONT}")
    im = Image.open(SRC_FRONT).convert("RGBA")
    OUT.mkdir(exist_ok=True)
    for name, x, y in GLYPH_CELLS:
        im.crop((x, y, x + 32, y + 32)).save(OUT / f"{name}.png")




# —— 冬（魔理沙）季节子机的**激光**贴图 —— TH16 `pl01sub.anm` entry0 sprite0 = (0,1,512,30)，
# 实际贴图 `player/pl01/pl01.png` 只有 256 宽，取 (0,1,256,31) 铺进 256×32 的 entry。
# 一手：`pl01sub.sht` 的 option 弹 anm 脚本 = 0/3/6/9/12/15（tag=2 激光），
# 而激光的可见形状就是这条**横向长条**（TH18 的激光 tick 会把贴图沿束向拉伸）。
def make_laser():
    src = REPO / "local/th16.v1.00a/anm/pl01sub/player/pl01/pl01.png"
    if not src.is_file():
        raise SystemExit(f"缺 TH16 原版自机贴图 {src}")
    im = Image.open(src).convert("RGBA")
    # ★ entry0 的 yOffset = 176（同页多 texture），sprite0 = (0,1,512,30) ⇒ 实际在 y=177 起
    strip = im.crop((0, 177, 256, 207))
    out = Image.new("RGBA", (256, 32), (0, 0, 0, 0))
    out.paste(strip, (0, 1))
    OUT.mkdir(exist_ok=True)
    out.save(OUT / "SEASON_LASER.png")
    # 激光的光斑（子脚本用 sprite1）与命中特效（sprite2）：都在 entry0，同样 +yOffset 176
    im.crop((0, 224, 32, 256)).save(OUT / "LASER_CHILD.png")
    im.crop((32, 224, 64, 256)).save(OUT / "LASER_HIT.png")




# —— ★ 2026-09-10 第二十六轮：**五副季节子机弹的真正贴图**
# 一手来源：TH16 `pl0Xsub.anm`（副自机 anm）。`PlayerBullet__create` `th16:0x444e10` 里
#   副季节那条路走的是 `player->subseason_anm_file`（= pl0Xsub.anm）+ 脚本号 = 副 .sht 的 `+0x22`。
# 各副脚本用的 sprite（`thanm spec` 出来的 sprite 表）：
#   春 pl00sub script0：主体 sprite9/10 (96,32)/(128,32) 32×32 每 2 帧循环；
#                       命中 sprite1..4 (32,0)/(64,0)/(96,0)/(128,0) 32×32 每 3 帧
#   夏 pl02sub script0：主体 sprite0 (0,0) 16×16；命中 sprite0..3 (0,0)/(16,0)/(32,0)/(48,0)
#   秋 pl03sub script0：主体 sprite0 (0,0) 64×64；命中 sprite1..3 (64,0)/(128,0)/(192,0)
#   土用 pl04sub script0：主体 sprite0 (1,1) 30×14；命中 sprite1..3 (33,1)/(65,1)/(97,1)
# 贴图都是 2 的幂 → 直接按原尺寸存（32/16/64），30×14 的土用补到 32×16（居中）。
# ★★ 关键：TH16 的 anm 贴图是**同页多 texture**，每个 entry 有自己的 `xOffset/yOffset`，
# sprite 的 (x,y) 是**相对该 entry 的** ⇒ 切图必须加偏移，否则切到的是白色空白区
# （2026-09-10 实跑踩过：不加偏移 ⇒ 满屏「一系列长方形」）。
# 偏移取自各 pl0Xsub.anm 的 entry0 头：pl00sub (64,192)、pl01sub (0,176)、pl02sub (0,144)、
# pl03sub (0,176)、pl04sub (0,144)。
SUBS = {
    "SPRING": ("pl00sub/player/pl00/pl00.png", (64, 192), [
        ("BULLET_SPRING_A", (96, 32, 128, 64)), ("BULLET_SPRING_B", (128, 32, 160, 64)),
    ] + [(f"HIT_SPRING_{i}", ((i - 1) % 5 * 32, (i - 1) // 5 * 32,
                              (i - 1) % 5 * 32 + 32, (i - 1) // 5 * 32 + 32)) for i in range(1, 9)]),
    "SUMMER": ("pl02sub/player/pl02/pl02.png", (0, 144), [
        ("BULLET_SUMMER", (0, 0, 16, 16)),
        ("HIT_SUMMER_1", (0, 0, 16, 16)), ("HIT_SUMMER_2", (16, 0, 32, 16)),
        ("HIT_SUMMER_3", (32, 0, 48, 16)),
    ]),
    "FALL": ("pl03sub/player/pl03/pl03.png", (0, 176), [
        ("BULLET_FALL", (0, 0, 64, 64)),
        ("HIT_FALL_1", (64, 0, 128, 64)), ("HIT_FALL_2", (128, 0, 192, 64)),
        ("HIT_FALL_3", (192, 0, 256, 64)),
    ]),
    "DOYOU": ("pl04sub/player/pl04/pl00.png", (0, 144), [
        ("BULLET_DOYOU", (1, 1, 31, 15)),
        ("HIT_DOYOU_1", (33, 1, 63, 15)), ("HIT_DOYOU_2", (65, 1, 95, 15)),
        ("HIT_DOYOU_3", (97, 1, 127, 15)),
    ]),
}


def make_sub_bullets():
    OUT.mkdir(exist_ok=True)
    for season, (rel, (xoff, yoff), items) in SUBS.items():
        src = REPO / "local/th16.v1.00a/anm" / rel
        if not src.is_file():
            raise SystemExit(f"缺 TH16 原版自机贴图 {src}")
        im = Image.open(src).convert("RGBA")
        for name, (x0, y0, x1, y1) in items:
            piece = im.crop((x0 + xoff, y0 + yoff, x1 + xoff, y1 + yoff))
            w, h = piece.size
            # 非 2 的幂（土用 30×14）→ 补到 32×16，居中
            tw, th = (1 << (w - 1).bit_length()), (1 << (h - 1).bit_length())
            if (tw, th) != (w, h):
                canvas = Image.new("RGBA", (tw, th), (0, 0, 0, 0))
                canvas.paste(piece, ((tw - w) // 2, (th - h) // 2), piece)
                piece = canvas
            piece.save(OUT / f"{name}.png")


# —— 季节槽的「releaseable / 等级」条：TH16 `front.anm` script118 的 `sprite87`
# = `front00.png` (832,112,64,16)（一手：`front.anm` 的 sprite87 定义）。
def make_digit_strip():
    if not SRC_FRONT.is_file():
        raise SystemExit(f"缺 TH16 原版 front00.png {SRC_FRONT}")
    im = Image.open(SRC_FRONT).convert("RGBA")
    OUT.mkdir(exist_ok=True)
    im.crop((832, 112, 896, 128)).save(OUT / "SEASON_STRIP.png")


def main():
    make_opt()
    make_seed()
    make_points()
    make_glyphs()
    make_opts()
    make_laser()
    make_digit_strip()
    make_sub_bullets()
    print(f"season: SEASON_OPT.png 64x64 + SEASON_SEED.png 32x32 + "
          f"PT_{{SPRING,SUMMER,FALL,WINTER,DOYOU}}.png 16x16 + "
          f"GLYPH_*.png 32x32 + OPT_*.png 16x16 + SEASON_LASER.png 256x32 + "
          f"BULLET_*/HIT_*.png（TH16 pl0Xsub.anm 切图）-> {OUT.relative_to(HERE.parent)}")


if __name__ == "__main__":
    main()
