#!/usr/bin/env python3
"""地灵殿（id 111）**魔理沙子机的弹**贴图 —— TH18 原生 `pl01.anm` entry0 的 `sprite24/25/26` 1:1 切图。

    python3 make_marisa_bullet_art.py          # 出图 + 打印要 append 到 entries/ORDER.txt 的那几行
    python3 make_marisa_bullet_art.py --dump   # 附带把三格打成 ASCII（核证据用，只读源图）

=====================================================================================
为什么需要这个文件（用户 2026-09-16 实跑：「**魔理沙发弹样式不对**」→ 追问后：「**弹的贴图**」）
=====================================================================================

■ 根因（一手，`engine/sht/th18/02-shooter-record.md` §4）

  **装备卡子机的弹，贴图来自 `ability.anm`**：卡侧开火期间引擎把 `player+0x10` 换成
  `ABILITY_MANAGER->ability_anm`（`0x40aa03`，`0x4cf298` = `zAbilityManager*`），循环后才还原成
  `pl0X.anm` ⇒ shooter 记录里 `+0x22`（ANM 脚本号）**在卡子机那条路上是 `ability.anm` 的号段**。
  111 的魔理沙五组（`MARISA_FORMATION_*`，槽 `0x29`–`0x2d`）原来把 `+0x22` 指到
  `CE_ANM_ABILITY_SCRIPT_SEASON_BULLET_DOYOU`（**借 116 的「季节点」弹**）⇒ 打出来的是季节点，不是魔理沙的弹 ✗。

■ 结论（✅ 一手，逐条可复核）

  | 我们切的名字 | 源 sprite | 源矩形 | 一手 |
  | --- | --- | --- | --- |
  | `MARISA_BULLET_0` | **sprite24** | (1, 145, 30, 14) | `pl01.anm` **script5** 正文 `sprite(sprite24)`（自机主炮弹）|
  | `MARISA_BULLET_1` | **sprite25** | (33, 145, 30, 14) | 同 script5 的 `interruptLabel(1)`（命中/淡出那一档）|
  | `MARISA_BULLET_2` | **sprite26** | (65, 145, 30, 14) | 同上（再下一档）|
  | `MARISA_BULLET_3` | **sprite27** | (97, 145, 30, 14) | 同上（第 3 档，之后 `delete()`）|

  - 矩形来自 `local/th18.v1.00a/anm/pl01/pl01.anm.txt` 第 40–42 行的 sprite 表本体（entry0 =
    `player/pl01/pl01.png`，256×256）。
  - `script5` 的静态属性（`layer(13); type(1); alpha(128); zWriteDisable(1); rotateAuto(1);`）逐条抄进
    `assets/ability/scripts/282_marisa_bullet.anm.txt`；`rotateAuto(1)` = **贴图跟着弹的速度方向转**
    （原生就是这么用的 ⇒ 我们那条 30×14 的横弹在任意角度下都朝前）。

■ 生成器纪律（照 `make_flan_art.py` / `make_mof_bomb_art.py`）

  - 只 `crop`/`save`：**不缩放、不重采样、不补边**（矩形 = 原图像素尺寸，`build_ability.py` 图集硬门②/③）；
  - 幂等（无随机、无时间戳）；缺源图时直接报错并给解包命令（`local/` 是 gitignored 的）。
"""
import sys
from pathlib import Path

from PIL import Image

HERE = Path(__file__).resolve().parent
REPO = HERE.parents[4]
SRC = REPO / "local/th18.v1.00a/anm/pl01/player/pl01/pl01.png"
OUT = HERE / "marisa_bullet"

# 一手：TH18 `pl01.anm` entry0 —— **画布 256×512、内嵌图像 256×320**（`pl01.anm.txt` entry0 的
# `width/height` 与 `THTXWidth/THTXHeight`）⇒ 校验内嵌图像那一对（切图用的是内嵌图像）。
SHEET = (256, 320)

# 一手：`pl01.anm` script5 的 `sprite(sprite24)` 与它 `interruptLabel(1)` 里的 25/26
SP0 = (1, 145, 30, 14)
SP1 = (33, 145, 30, 14)
SP2 = (65, 145, 30, 14)
SP3 = (97, 145, 30, 14)


def _load():
    if not SRC.is_file():
        raise SystemExit(
            f"缺原版贴图 {SRC}\n（TH18 的资产先解包：`python3 tooling/thtk/unpack.py th18.v1.00a`；"
            "pl01.anm 的贴图会落在 `local/th18.v1.00a/anm/pl01/player/pl01/`）"
        )
    im = Image.open(SRC).convert("RGBA")
    if im.size != SHEET:
        raise SystemExit(f"{SRC} 是 {im.size}，预期 {SHEET}（entry0 的 width/height）")
    return im


def _crop(src, box):
    x, y, w, h = box
    return src.crop((x, y, x + w, y + h)).copy()


def _dump(src):
    for box, name in ((SP0, "MARISA_BULLET_0"), (SP1, "MARISA_BULLET_1"), (SP2, "MARISA_BULLET_2"),
                      (SP3, "MARISA_BULLET_3")):
        x, y, w, h = box
        print(f"\n--- {name} @{box} ---")
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
              f"({tot[0] // max(n, 1)}, {tot[1] // max(n, 1)}, {tot[2] // max(n, 1)})")


def main():
    dump = "--dump" in sys.argv
    src = _load()
    OUT.mkdir(exist_ok=True)

    for box, name in ((SP0, "MARISA_BULLET_0"), (SP1, "MARISA_BULLET_1"), (SP2, "MARISA_BULLET_2"),
                      (SP3, "MARISA_BULLET_3")):
        tile = _crop(src, box)
        tile.save(OUT / f"{name}.png")
        print(f"marisa_bullet: {name}.png {tile.width}x{tile.height} = TH18 pl01.anm entry0 "
              f"({box[0]},{box[1]},{box[2]},{box[3]})")

    print("\n图集画布 120x14（4 个成员横排，无补边）；把下面这段 append 到 entries/ORDER.txt：")
    print("ATLAS ATLAS_MARISA_BULLET 120x14")
    print("MARISA_BULLET_0        ability/marisa_bullet/MARISA_BULLET_0.png       @0,0,30x14")
    print("MARISA_BULLET_1        ability/marisa_bullet/MARISA_BULLET_1.png       @30,0,30x14")
    print("MARISA_BULLET_2        ability/marisa_bullet/MARISA_BULLET_2.png       @60,0,30x14")
    print("MARISA_BULLET_3        ability/marisa_bullet/MARISA_BULLET_3.png       @90,0,30x14")

    if dump:
        _dump(src)


if __name__ == "__main__":
    main()
