#!/usr/bin/env python3
"""地灵殿（id 111）魔理沙×荷取「河童罩子」的**水泡贴图** —— TH11 `pl01.anm` entry3 的 1:1 切图。

    python3 make_nitori_bubble_art.py          # 出图 + 打印要 append 到 entries/ORDER.txt 的那几行
    python3 make_nitori_bubble_art.py --dump   # 附带把这一格打成 ASCII + 平均色（核证据用，只读源图）

=====================================================================================
一手考证（用户口径：「放B是消弹，给一个**防护罩（有贴图特效）**，只要防护罩不撞认为是 no miss」）
=====================================================================================

■ 结论（✅ 一手，逐条可复核）

  | 我们切的名字 | 源 sprite | 源矩形 | 一手 |
  | --- | --- | --- | --- |
  | `NITORI_BUBBLE` | **sprite76** | (1, 1, 254, 254) | TH11 `pl01.anm` **script58** 正文 `sprite(sprite76)` |

  - `sprite76` 的矩形 = TH11 `pl01.anm` **entry3**（`player/pl01/pl01c.png`，256×256）的 sprite 表本体
    （`thanm -l 11 local/_scratch/th11sht/pl01.anm -m local/vendor/eclmap/anmmap/v4.anmm`
    打出的 `sprite76: { x: 1, y: 1, w: 254, h: 254 }`；entry3 只有这一个 sprite）。
  - **为什么是 pl01.anm 的脚本 58**（一手链，`local/th11.v1.00a/th11.exe`）：
    `Bomb::start__marisa_c` `th11:0x407680` 的 `th11:0x4076e7 push 0x16`(=层 22) /
    `th11:0x4076ec push 0x3a`(=脚本 58) → `th11:0x407705 call 0x455b10`，而
    `th11:0x407700 mov edx,[player+0x10]`（= 已加载的**角色 anm**；`Player::initialize`
    `th11:0x42f8c9` 把 `pl01.anm` 存进 `player+0x10`）⇒ 脚本号是 **pl01.anm 的全局下标**。
    「组合 5 = 魔理沙C（荷取）」的一手见 `assets/ability/make_marisa_elem_art.py` 的文件头。
  - **图是什么**（本文件 `--dump` 可复核）：一张**半透明的青色水泡** ——
    `a=0` 的圆外区域占绝大多数，泡体中心 `(0,84,76,118)`、边缘 `(0,162,147)`、亮部 `(115,202,194)`
    ⇒ 加算（脚本 `blendMode(1)`）+ 每帧 −3°自转（`angleVel(0,0,-0.05235988f)`）= 旋转的水膜球。
  - **它不是** `sprite(-1) + drawPoly/drawRING` 那一族：脚本第一行就是 `sprite(sprite76)`，
    图本身自带渐变的泡壁 ⇒ 必须搬真贴图，不能拿图元凑。

■ 为什么抠图，而不是在脚本里直接写 `sprite(76)`

  同 `make_mof_bomb_art.py` / `make_marisa_elem_art.py` 的两条理由：① 本仓 `assets/ability/scripts/`
  里**没有任何一条追加脚本引用原生 sprite 号**（全部 `@NAME@` 或 `sprite(-1)`）⇒ 跟既有管线一致；
  ② 抠了以后 `anm_ids.h` 里有 `CE_ANM_ABILITY_SPRITE_NITORI_BUBBLE` 可给 C 侧用。
  ⚠ **不能**直接写 `sprite(76)`：76 是 **TH11 `pl01.anm`** 的表，我们编的是 **TH18 `ability.anm`**
  （原生 sprite0..76 的号在我们的文件里完全不是一回事）。

■ 生成器纪律（照 `make_mof_bomb_art.py`）

  - 只 `crop`/`save`：**不缩放、不重采样、不补边**（矩形 = 原图像素尺寸，`build_ability.py` 图集硬门②/③）；
  - 254 不是 2 的幂：`build_ability.py` 的 `template_for()` 已明确放开这条（一手：
    `D3DXCreateTexture` 拿到 entry 头的 w/h **零尺寸检查**，`th18:0x486488`；非 2 次幂的原生 entry 有 354 个）；
  - 幂等（无随机、无时间戳）；缺源图时直接报错并给解包命令（`local/` 是 gitignored 的）。
"""
import sys
from pathlib import Path

from PIL import Image

HERE = Path(__file__).resolve().parent
REPO = HERE.parents[4]
SRC = REPO / "local/th11.v1.00a/anm/pl01/player/pl01/pl01c.png"
OUT = HERE / "signal"

# 一手：TH11 `pl01.anm` entry3 的贴图尺寸（`thanm -l 11 …` 打出的 entry3 width/height）
SHEET = (256, 256)
# 一手：script58 的 `sprite(sprite76)` = (1, 1, 254, 254)
BUBBLE = (1, 1, 254, 254)


def _load():
    if not SRC.is_file():
        raise SystemExit(
            f"缺原版贴图 {SRC}\n（TH11 的资产先解包，两条都在仓库根跑：\n"
            "  mkdir -p /tmp/th11x && ./local/vendor/thtk/build/thdat/thdat -x 11 -C /tmp/th11x "
            "local/th11.v1.00a/th11.dat pl01.anm\n"
            "  cd local/th11.v1.00a/anm/pl01 && "
            "../../../../local/vendor/thtk/build/thanm/thanm -x 11 /tmp/th11x/pl01.anm\n"
            "⚠ TH11 的贴图**内嵌在 .anm 里**（entry 的 `hasData: 1`），不单独成文件；"
            "⚠ 别给 thanm 加 `-o`（会报 mmap failed）)"
        )
    im = Image.open(SRC).convert("RGBA")
    if im.size != SHEET:
        raise SystemExit(f"{SRC} 是 {im.size}，预期 {SHEET}（entry3 的 width/height）")
    return im


def _dump(src):
    x, y, w, h = BUBBLE
    print(f"\n--- NITORI_BUBBLE @{BUBBLE} ---（每 2 像素取 1 点，8×8 降采样成 32×32 的示意）")
    n = 0
    tot = [0, 0, 0]
    for j in range(0, h, 8):
        row = ""
        for i in range(0, w, 8):
            a = max(src.getpixel((x + min(i + k, w - 1), y + min(j + k, h - 1)))[3]
                    for k in range(0, 8, 3))
            row += " " if a < 8 else ("." if a < 64 else ("o" if a < 160 else "#"))
        print("  " + row)
    for j in range(h):
        for i in range(w):
            r, g, b, a = src.getpixel((x + i, y + j))
            if a > 32:
                n += 1
                tot[0] += r; tot[1] += g; tot[2] += b
    print(f"  alpha>32 的像素 {n} 个（占 {100.0 * n / (w * h):.1f}%）；平均色 = "
          f"({tot[0] // max(n, 1)}, {tot[1] // max(n, 1)}, {tot[2] // max(n, 1)})")
    print(f"  中心 {src.getpixel((x + w // 2, y + h // 2))} / 左上角 {src.getpixel((x, y))} / "
          f"四角 {src.getpixel((x + w - 1, y))} {src.getpixel((x, y + h - 1))}")


def main():
    dump = "--dump" in sys.argv
    src = _load()
    OUT.mkdir(exist_ok=True)

    tile = src.crop((BUBBLE[0], BUBBLE[1], BUBBLE[0] + BUBBLE[2], BUBBLE[1] + BUBBLE[3])).copy()
    tile.save(OUT / "NITORI_BUBBLE.png")
    print(f"nitori_bubble: NITORI_BUBBLE.png {tile.width}x{tile.height} = TH11 pl01.anm entry3 "
          f"({BUBBLE[0]},{BUBBLE[1]},{BUBBLE[2]},{BUBBLE[3]})，脚本 58 的 `sprite(sprite76)`")

    print("\n图集画布 254x254（1 个成员，无补边）；把下面这段 append 到 entries/ORDER.txt：")
    print("ATLAS ATLAS_NITORI_BUBBLE 254x254")
    print("NITORI_BUBBLE          ability/signal/NITORI_BUBBLE.png           @0,0,254x254")

    if dump:
        _dump(src)


if __name__ == "__main__":
    main()
