#!/usr/bin/env python3
"""风神录（id 110）灵击的一格**火星**切图 —— TH10 `bullet.anm` **entry2** 的 `sprite330`。

    python3 make_mof_bomb_art.py          # 出图 + 打印要 append 到 entries/ORDER.txt 的那几行
    python3 make_mof_bomb_art.py --dump   # 附带把这一格打成 ASCII（核证据用，只读源图）

=====================================================================================
一手考证：TH10 灵击特效的**火星**那一格
=====================================================================================

■ 结论（✅ 一手，逐条可复核）

  | 我们切的名字 | 源 sprite | 源矩形 | 一手 |
  | --- | --- | --- | --- |
  | `MOF_BOMB_SPARK` | **sprite330** | (0, 224, 32, 32) | TH10 `bullet.anm` **script398 / script399** 正文第一行 `sprite(sprite330)` |

  - `sprite330` 的矩形来自 TH10 `bullet.anm` **entry2**（`bullet/etama2.png`，256×256）的 sprite 表本体
    （`thanm -l 10 … -m v4.anmm` 打出的 `sprite330: { x: 0, y: 224, w: 32, h: 32 }`，
    见 `local/th10.v1.00a/anm/bullet/bullet.anm.txt` 第 15480 行；**必须用 `v4.anmm`** ——
    用 v8 的 map 打这份 v4 文件会得到错名字，见下）。
  - script398/399 的全文与逐条解读见 `assets/ability/scripts/280_mof_bomb_spark.anm.txt` 的文件头。
  - 这一格**像素级核对**：全格只有 **261 个** alpha>32 的像素，不透明像素的平均色 = **(255,255,255)**
    ⇒ 它是一颗**纯白的四芒星**（贴图不带色），颜色由脚本的 `color()` 给
    （398 = `color(255,0,0)` 红、399 = `color(0,0,255)` 蓝）。
    `--dump` 把形状打成 ASCII，可直接核「是不是四芒星」。

■ ⚠ 读 TH10 的 anm 必须换 mapfile（这条是本次踩到的坑，记下来给下一个人）

  TH10 的 anm 版本 = **4**，而 `local/thvendor/thpages/static/mapfile/v8.anmm` 是 **v8** 的表。
  两者**同名不同号**（例：v4 `3 sprite` / v8 `3 stop`；v4 `50 scale` / v8 `402 scale`；
  v4 的数学指令 `6..27` 在 v8 是 `100..121`，整体 **+94**）。
  ⇒ 拿 v8 的 map 去 `thanm -l 10`，打出来的会是 `stop(sprite362)` 这种**又对又错**的文本
  （号码是 v4 的、名字是 v8 的）⇒ **一定加 `-m local/vendor/thpages/static/mapfile/v4.anmm`**。

■ 为什么抠图，而不是在脚本里直接写 `sprite(330)`

  同 `make_flan_art.py` 的两条理由：① 本仓 `assets/ability/scripts/` 里**没有任何一条追加脚本
  引用原生 sprite 号**（全部 `@NAME@` 或 `sprite(-1)`）⇒ 跟既有管线一致；
  ② 抠了以后 `anm_ids.h` 里有 `CE_ANM_ABILITY_SPRITE_MOF_BOMB_SPARK` 可给 C 侧用。
  ⚠ **不能**直接写 `sprite(330)`：330 是 **TH10 `bullet.anm`** 的表，我们编的是 **TH18 `ability.anm`**
  —— 两个文件的 sprite 表毫无关系（TH18 的 330 号是另一张图）。这正是必须抠图的原因。

■ 生成器自身的纪律（照 `make_flan_art.py` / `make_signal_hud_art.py` 的既有管线）

  - 只用不带 mask 的 `crop` / `save`：**不缩放、不重采样、不补边**（矩形 = 原图像素尺寸，
    这是 `build_ability.py` 图集硬门②/③要求的）；
  - 幂等（无随机、无时间戳）；缺源图时直接报错并给解包命令（`local/` 是 gitignored 的）。
"""
import sys
from pathlib import Path

from PIL import Image

HERE = Path(__file__).resolve().parent
REPO = HERE.parents[4]
SRC = REPO / "local/th10.v1.00a/anm/bullet/bullet/etama2.png"
SRC_STRIP = REPO / "local/th10.v1.00a/anm/bullet/bullet/etama3.png"
OUT = HERE / "mof_bomb"

# 一手：TH10 `bullet.anm` entry2 的贴图尺寸（`bullet.anm.txt` entry2 的 width/height）
SHEET = (256, 256)

# 一手：script398 / script399 正文第一行的 `sprite(sprite330)`
SP_SPARK = (0, 224, 32, 32)

# ★ 2026-09-16p（用户：「th10 的灵击…**边缘有类似于电流的特效**」）：
#   四支柱环 script394–397 画的是 `sprite(sprite362)` = **etama3.png 里的一条 14×128 竖条**
#   （`bullet.anm.txt` 的 sprite 表：`sprite362: { x: 65, y: 0, w: 14, h: 128 }`），
#   再用 `texCircle(32)` 把它**弯成圈** —— 竖条自带的分段 alpha 带上就是那种「电流边」。
SP_RING_STRIP = (65, 0, 14, 128)


def _load():
    if not SRC.is_file():
        raise SystemExit(
            f"缺原版贴图 {SRC}\n（TH10 的资产先解包，两个命令都在仓库根跑：\n"
            "  mkdir -p /tmp/th10x && ./local/vendor/thtk/build/thdat/thdat -x 10 -C /tmp/th10x "
            "local/th10.v1.00a/th10.dat bullet.anm\n"
            "  cd local/th10.v1.00a/anm/bullet && "
            "../../../../local/vendor/thtk/build/thanm/thanm -x 10 /tmp/th10x/bullet.anm\n"
            "⚠ TH10 的贴图**内嵌在 .anm 里**（entry 的 `hasData: 1`），不在 .dat 里单独成文件）"
        )
    im = Image.open(SRC).convert("RGBA")
    if im.size != SHEET:
        raise SystemExit(f"{SRC} 是 {im.size}，预期 {SHEET}（entry2 的 width/height）")
    return im


def _crop(src, box):
    x, y, w, h = box
    return src.crop((x, y, x + w, y + h)).copy()


def _dump(src):
    x, y, w, h = SP_SPARK
    print(f"\n--- MOF_BOMB_SPARK = sprite330 @{SP_SPARK} ---")
    n = 0
    tot = [0, 0, 0]
    for j in range(h):
        row = ""
        for i in range(w):
            r, g, b, a = src.getpixel((x + i, y + j))
            row += " " if a < 8 else ("." if a < 64 else ("o" if a < 160 else "#"))
            if a > 32:
                n += 1
                tot[0] += r; tot[1] += g; tot[2] += b
        print("  " + row)
    print(f"  alpha>32 的像素 {n} 个；平均色 = "
          f"({tot[0] // max(n, 1)}, {tot[1] // max(n, 1)}, {tot[2] // max(n, 1)})"
          "（纯白 ⇒ 颜色由脚本的 color() 给）")


def main():
    dump = "--dump" in sys.argv
    src = _load()
    OUT.mkdir(exist_ok=True)

    tile = _crop(src, SP_SPARK)
    tile.save(OUT / "MOF_BOMB_SPARK.png")
    if SRC_STRIP.is_file():
        im3 = Image.open(SRC_STRIP).convert("RGBA")
        tile3 = _crop(im3, SP_RING_STRIP)
        tile3.save(OUT / "MOF_BOMB_RING_STRIP.png")
        print(f"mof_bomb: MOF_BOMB_RING_STRIP.png {tile3.width}x{tile3.height} = TH10 bullet.anm "
              f"(etama3 entry3) ({SP_RING_STRIP[0]},{SP_RING_STRIP[1]},{SP_RING_STRIP[2]},{SP_RING_STRIP[3]})")
    else:
        print(f"⚠ 缺 {SRC_STRIP}（竖条那一格没出图；先跑一遍解包命令，见文件头）")
    print(f"mof_bomb: MOF_BOMB_SPARK.png {tile.width}x{tile.height} = TH10 bullet.anm entry2 "
          f"({SP_SPARK[0]},{SP_SPARK[1]},{SP_SPARK[2]},{SP_SPARK[3]})")

    print("\n图集画布 32x32（1 个成员，无补边）；把下面这段 append 到 entries/ORDER.txt：")
    print("ATLAS ATLAS_MOF_BOMB 32x32")
    print("MOF_BOMB_SPARK         ability/mof_bomb/MOF_BOMB_SPARK.png         @0,0,32x32")

    print("\n★ 2026-09-16p 追加：**环的竖条**（`sprite362`，etama3.png，14x128）—— 新开一个图集")
    print("ATLAS ATLAS_MOF_BOMB_RING 14x128")
    print("MOF_BOMB_RING_STRIP    ability/mof_bomb/MOF_BOMB_RING_STRIP.png    @0,0,14x128")

    if dump:
        _dump(src)


if __name__ == "__main__":
    main()
