#!/usr/bin/env python3
"""地灵殿（id 111）**魔理沙的元素子机 + 元素子机弹**贴图 —— TH11 原生 `pl01.anm` entry0 的 1:1 切图。

    python3 make_marisa_elem_art.py          # 出图 + 打印要 append 到 entries/ORDER.txt 的那几行
    python3 make_marisa_elem_art.py --dump   # 附带把每一格打成 ASCII + 平均色（核证据用，只读源图）

=====================================================================================
一手考证（用户口径：「**你得用 th11 的魔理沙子机贴图和弹幕样式呀**」+「**切换子机的部分是帕秋莉的特效啊！**」）
=====================================================================================

■ 为什么是这一套（一手链，`local/th11.v1.00a/th11.exe`）

  TH11 的子机 VM 由 `Player::repopulate_options` `th11:0x432cc0` 建，脚本号按**组合号**
  （`3*CHARACTER + SUBSHOT`）选：
    · 组合 3（MarisaA）→ `0x15 + tbl[0x4a7c28]` = 脚本 21..33
    · 组合 4（**MarisaB**）→ **`0x22 + player+0x8bac`（= formation）⇒ 脚本 34..38**（`th11:0x433465`/`0x433470`）
    · 组合 5（MarisaC）→ `0x27` = 脚本 39
  ⇒ **只有 B 这一支是「五档 = 五种脚本」**，而脚本 34..38 画的是 **五个 16×16 纯色球**
  （红/蓝/绿/黄/褐 = **火水木金土**）—— 这就是「**切换子机**」这件事的真身，
  也正是用户说的「**切换子机的部分是帕秋莉的特效**」（五元素系统在 SA 里只有帕秋莉有）。
  ⚠ 「B = 帕秋莉」的一手依据 = **五元素结构 + 图本身**（球色红蓝绿黄褐）；exe 里没有伙伴名的可读字符串，
    「A = 爱丽丝 / C = 荷取」是排除法 + 社区 ⇒ 本仓按 B = 帕秋莉落地（登记在 `TUNING.md`）。

■ 切了什么（逐格，全部来自 `pl01.anm` entry0 = `player/pl01/pl01.png`，256×256）

  | 我们切的名字 | 源 sprite | 源矩形 | 内容（原文依据）|
  | --- | --- | --- | --- |
  | `MARISA_ELEM_FIRE`  | sprite38 | (64, 160, 16, 16)  | 红球 —— `pl01.anm` script34 正文 `sprite(sprite38)` |
  | `MARISA_ELEM_WATER` | sprite39 | (80, 160, 16, 16)  | 蓝球 —— script35 |
  | `MARISA_ELEM_WOOD`  | sprite40 | (96, 160, 16, 16)  | 绿球 —— script36 |
  | `MARISA_ELEM_METAL` | sprite41 | (112, 160, 16, 16) | 黄球 —— script37 |
  | `MARISA_ELEM_EARTH` | sprite42 | (128, 160, 16, 16) | 褐球 —— script38 |
  | `MARISA_ELEM_BULLET_FIRE`  | sprite49 | (0, 208, 32, 16) | formation 0 的子机弹（红）—— shooter `+0x1e = 6` → `create(脚本 6+5=11)` |
  | `MARISA_ELEM_BULLET_WATER` | sprite45 | (0, 192, 32, 16) | formation 1（蓝）—— `+0x1e = 4` → 脚本 9 |
  | `MARISA_ELEM_BULLET_WOOD`  | sprite24 | (0, 144, 32, 16) | formation 2（绿）—— `+0x1e = 0` → 脚本 5 |
  | `MARISA_ELEM_BULLET_METAL` | sprite53 | (0, 224, 32, 16) | formation 3（黄）—— `+0x1e = 8` → 脚本 13 |
  | `MARISA_ELEM_BULLET_EARTH` | sprite57 | (0, 240, 32, 16) | formation 4（橙褐）—— `+0x1e = 10` → 脚本 15 |

  · 子机弹那条链的一手：`Player::shoot_one_bullet` `th11:0x433f90` 的
    `0x4341fb movsx eax,WORD [edi+0x1e]` → `0x4341ff add eax,5` → `0x434203 push ecx(=pl_anm)` →
    `0x434204 call 0x455a00`（**TH11 的子机弹永远走 `pl0X.anm`，号 = shooter[+0x1e] + 5**；
    TH18 相反：卡子机走 `ability.anm`、`+0x22` 直接给号 ⇒ 我们**必须**把这几格搬进 `ability.anm`）。
  · 为什么球和弹要**各搬 5 格**：TH11 里五档是「球一色 + 弹一色」一一对应的
    （红球打红弹…），球色与弹色交叉核对过 ⇒ 一档 = 一格球 + 一格弹。
  · ⚠ TH11 的 **A（爱丽丝）** 用的是另一段 sprite（球 = sprite37、弹 = sprite28 通用彩虹弹）；
    **C（荷取）** = sprite43/44 与 61/62 —— 本轮**不搬**（用户只要魔理沙＝帕秋莉这一套；
    将来要 A/C 就照本文件的写法加两段）。

■ 生成器纪律（照 `make_flan_art.py` / `make_mof_bomb_art.py`）

  - 只 `crop`/`save`：**不缩放、不重采样、不补边**（矩形 = 原图像素尺寸，`build_ability.py` 图集硬门②/③）；
  - 幂等（无随机、无时间戳）；缺源图时直接报错并给解包命令（`local/` 是 gitignored 的）。
"""
import sys
from pathlib import Path

from PIL import Image

HERE = Path(__file__).resolve().parent
REPO = HERE.parents[4]
SRC = REPO / "local/th11.v1.00a/anm/pl01/player/pl01/pl01.png"
OUT = HERE / "marisa_elem"

# 一手：TH11 `pl01.anm` entry0 的贴图尺寸（`pl01.anm.txt` entry0 的 width/height）
SHEET = (256, 256)

# 一手：脚本 34..38 的 `sprite(sprite38..42)`（五元素球）
BALLS = (
    ("MARISA_ELEM_FIRE",  (64, 160, 16, 16)),
    ("MARISA_ELEM_WATER", (80, 160, 16, 16)),
    ("MARISA_ELEM_WOOD",  (96, 160, 16, 16)),
    ("MARISA_ELEM_METAL", (112, 160, 16, 16)),
    ("MARISA_ELEM_EARTH", (128, 160, 16, 16)),
)
# 一手：五档子机弹的 sprite（shooter `+0x1e` + 5 → script5/9/11/13/15 里的 `sprite(...)`）
BULLETS = (
    ("MARISA_ELEM_BULLET_FIRE",  (0, 208, 32, 16)),
    ("MARISA_ELEM_BULLET_WATER", (0, 192, 32, 16)),
    ("MARISA_ELEM_BULLET_WOOD",  (0, 144, 32, 16)),
    ("MARISA_ELEM_BULLET_METAL", (0, 224, 32, 16)),
    ("MARISA_ELEM_BULLET_EARTH", (0, 240, 32, 16)),
)


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
        raise SystemExit(f"{SRC} 是 {im.size}，预期 {SHEET}（entry0 的 width/height）")
    return im


def _crop(src, box):
    x, y, w, h = box
    return src.crop((x, y, x + w, y + h)).copy()


def _dump(src):
    for name, box in BALLS + BULLETS:
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

    for name, box in BALLS + BULLETS:
        tile = _crop(src, box)
        tile.save(OUT / f"{name}.png")
        print(f"marisa_elem: {name}.png {tile.width}x{tile.height} = TH11 pl01.anm entry0 "
              f"({box[0]},{box[1]},{box[2]},{box[3]})")

    print("\n=== 图集 1：五元素球（画布 80x16，5 个成员横排）—— append 到 entries/ORDER.txt ===")
    print("ATLAS ATLAS_MARISA_ELEM 80x16")
    for k, (name, box) in enumerate(BALLS):
        print(f"{name:<22} ability/marisa_elem/{name}.png"
              f"{' ' * max(1, 34 - len(name))}@{k * 16},0,16x16")
    print("\n=== 图集 2：五元素子机弹（画布 160x16，5 个成员横排）—— 同一处 append ===")
    print("ATLAS ATLAS_MARISA_ELEM_BULLET 160x16")
    for k, (name, box) in enumerate(BULLETS):
        print(f"{name:<28} ability/marisa_elem/{name}.png"
              f"{' ' * max(1, 34 - len(name))}@{k * 32},0,32x16")

    if dump:
        _dump(src)


if __name__ == "__main__":
    main()
