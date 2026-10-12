#!/usr/bin/env python3
"""卡片 117 玩家兽灵弹 / 刀的**可编辑源图**与构建期兜底默认色（AL62/AL63）。

## 语义（AL63 更正）
- **规范输入目录** = `assets/player_art_src/<key>.png`：
  · **存在** ⇒ 构建**原样使用**（**不做任何 color/hue/mono 变换**，用户改完就是最终像素）；
  · **不存在** ⇒ 用「零售原图 + 本文件定义的默认色变换」在**内存里**生成兜底（不写回规范目录）。
- **Windows 可编辑区** `D:\\项目\\幻想指南\\18\\art-src\\`：
  · 根目录 `*.png` = **零售原图**（原样裁切，未调色）—— 给用户自己上色；
  · 子目录 `_auto_default/` = 构建兜底的**已上色默认图**（只是参考，不是原图，别拿它当源）。
- 流程：**原图 → 用户上色（存回 art-src 同名）→ `bash assets/sync_player_art_src.sh` → `make anm`**。

## 默认色变换（只在文件缺失时兜底）
  · `keep` = 零售原样；`hue` = HSV 只转 H（保 S/V）；`mono` = 目标 H、S=1、V=原亮度（纯色带明暗）。
"""
import colorsys
from pathlib import Path
from PIL import Image

HERE = Path(__file__).resolve().parent          # assets/player_art_src
ASSETS = HERE.parent                            # assets/
REPO = HERE.parents[4]                          # renkolab/
TH18_ANM = REPO / "local" / "th18.v1.00a" / "anm"
WIN_ART_SRC = Path("/mnt/d/项目/幻想指南/18/art-src")
AUTO_DIR = "_auto_default"

# key = 规范输入文件名 = 构建查的 key = 零售原图文件名（三者一致）。
# src/rect = 零售图里的原始 sprite 矩形；mode/hue = 缺图时的兜底默认色。
SPECS = [
    # —— 早苗：青蛙 / 蛇 / 爆炸 / 命中闪光 ——
    dict(key="sanae_frog_s34_f0",        src="pl03/player/pl03/pl03.png",  rect=(1, 177, 30, 30), mode="mono", hue=275),
    dict(key="sanae_snake_s30_f0",       src="pl03/player/pl03/pl03.png",  rect=(0, 160, 64, 16), mode="hue", hue=0),
    dict(key="sanae_snake_s31_f1",       src="pl03/player/pl03/pl03.png",  rect=(64, 160, 64, 16), mode="hue", hue=0),
    dict(key="sanae_snake_s32_f2",       src="pl03/player/pl03/pl03.png",  rect=(128, 160, 64, 16), mode="hue", hue=0),
    dict(key="sanae_snake_s33_f3",       src="pl03/player/pl03/pl03.png",  rect=(192, 160, 64, 16), mode="hue", hue=0),
    dict(key="sanae_explosion_s40_f0",   src="pl03/player/pl03/pl03c.png", rect=(0, 0, 128, 128),   mode="mono", hue=275),
    dict(key="sanae_explosion_s41_f1",   src="pl03/player/pl03/pl03c.png", rect=(128, 0, 128, 128), mode="mono", hue=275),
    dict(key="sanae_explosion_s42_f2",   src="pl03/player/pl03/pl03c.png", rect=(256, 0, 128, 128), mode="mono", hue=275),
    dict(key="sanae_explosion_s43_f3",   src="pl03/player/pl03/pl03c.png", rect=(384, 0, 128, 128), mode="mono", hue=275),
    dict(key="sanae_flash_s25_f0",       src="pl03/player/pl03/pl03.png",  rect=(16, 144, 16, 16),   mode="mono", hue=275),
    # 早苗狼命中「旋转小光斑」= 原生 script7 的 sprite25（16×16，单帧；旋转由父蛇 VM 的 rotateAuto 继承）
    # 缺图兜底：mono→红 H0、sat=0.65（比蛇亮一档的浅红；用户实机口径「更亮/偏白的红」）
    dict(key="sanae_wolf_glint_s25_f0",  src="pl03/player/pl03/pl03.png",  rect=(16, 144, 16, 16),   mode="mono", hue=0, sat=0.65),
    # ★ 109：TH19 一手的蛇/青蛙/命中（**成品图**，mode="keep" = 原样用、不套色）
    dict(key="pofv109_snake_f0", src="pl03/player/pl03/pl03.png", rect=(0, 160, 62, 14),   mode="keep", hue=0),
    dict(key="pofv109_snake_f1", src="pl03/player/pl03/pl03.png", rect=(64, 160, 62, 14),  mode="keep", hue=0),
    dict(key="pofv109_snake_f2", src="pl03/player/pl03/pl03.png", rect=(128, 160, 62, 14), mode="keep", hue=0),
    dict(key="pofv109_snake_f3", src="pl03/player/pl03/pl03.png", rect=(192, 160, 62, 14), mode="keep", hue=0),
    dict(key="pofv109_frog_f0",  src="pl03/player/pl03/pl03.png", rect=(1, 177, 30, 30),   mode="keep", hue=0),
    dict(key="pofv109_hit_f0",   src="pl03/player/pl03/pl03.png", rect=(16, 144, 14, 14),  mode="keep", hue=0),
    # —— 咲夜：刀（红=零售；蓝=零售，缺图兜底 → 紫）——
    dict(key="sakuya_knife_red_s24_f0",  src="pl02/player/pl02/pl02.png",  rect=(0, 192, 64, 16),   mode="keep", hue=0),
    dict(key="sakuya_knife_blue_s25_f0", src="pl02/player/pl02/pl02.png",  rect=(64, 192, 64, 16),  mode="hue",  hue=275),
]

SPEC_BY_KEY = {s["key"]: s for s in SPECS}


def _retail_image(spec):
    p = TH18_ANM / spec["src"]
    if not p.is_file():
        raise SystemExit(f"缺零售图 {p}（先 python3 tooling/thtk/unpack.py th18.v1.00a）")
    x, y, w, h = spec["rect"]
    im = Image.open(p).convert("RGBA")
    return im.crop((x, y, x + w, y + h))


def _recolor(im, mode, hue_deg, sat=1.0):
    if mode == "keep":
        return im
    out = im.copy()
    px = out.load()
    for y in range(out.height):
        for x in range(out.width):
            r, g, b, a = px[x, y]
            if a == 0:
                continue
            if mode == "hue":
                hh, ss, vv = colorsys.rgb_to_hsv(r / 255.0, g / 255.0, b / 255.0)
                hh, ss, vv = hue_deg / 360.0, ss, vv
            elif mode == "mono":
                lum = (0.299 * r + 0.587 * g + 0.114 * b) / 255.0
                hh, ss, vv = hue_deg / 360.0, sat, lum
            else:
                raise SystemExit(f"未知 recolor mode {mode}")
            r2, g2, b2 = colorsys.hsv_to_rgb(hh, ss, vv)
            px[x, y] = (int(r2 * 255 + 0.5), int(g2 * 255 + 0.5), int(b2 * 255 + 0.5), a)
    return out


def retail_image(key):
    """零售**原图**（原样裁切，未调色）—— 切给用户改的就是它。"""
    return _retail_image(SPEC_BY_KEY[key])


def default_image(key):
    """缺图兜底：零售原图 + 默认色变换（只在内存里用，不写进规范输入目录）。"""
    spec = SPEC_BY_KEY[key]
    return _recolor(_retail_image(spec), spec["mode"], spec["hue"], spec.get("sat", 1.0))


def ensure(key):
    """构建用图：规范输入目录有就**原样**返回；没有才用默认兜底（内存生成，不落盘）。"""
    if key not in SPEC_BY_KEY:
        raise SystemExit(f"未知 key {key}")
    p = HERE / f"{key}.png"
    if p.is_file():
        return Image.open(p).convert("RGBA")     # ★ 原样使用，绝不套色
    return default_image(key)


def cut_originals(overwrite=False):
    """把零售原图切到 Windows 侧 art-src 根目录（默认不覆盖；`overwrite=True` 恢复原图）。-> (n, path)"""
    if not WIN_ART_SRC.parent.is_dir():
        return 0, "父目录不存在（跳过）"
    WIN_ART_SRC.mkdir(parents=True, exist_ok=True)
    n = 0
    for s in SPECS:
        dst = WIN_ART_SRC / f"{s['key']}.png"
        if dst.is_file() and not overwrite:
            continue
        retail_image(s["key"]).save(dst)
        n += 1
    return n, str(WIN_ART_SRC)


def write_auto_defaults():
    """把「缺图兜底会用的已上色默认图」写到 art-src/_auto_default/ 供对照（每次重写）。-> (n, path)"""
    if not WIN_ART_SRC.parent.is_dir():
        return 0, "父目录不存在（跳过）"
    d = WIN_ART_SRC / AUTO_DIR
    d.mkdir(parents=True, exist_ok=True)
    n = 0
    for s in SPECS:
        default_image(s["key"]).save(d / f"{s['key']}.png")
        n += 1
    return n, str(d)
