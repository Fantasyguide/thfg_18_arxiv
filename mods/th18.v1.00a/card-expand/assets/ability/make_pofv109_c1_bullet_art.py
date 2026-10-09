#!/usr/bin/env python3
"""卡 109 C1 的弹 —— **从一手 TH19 切图**（不是烤的）。
    python3 make_pofv109_c1_bullet_art.py     # 出图 + 打印要抄进 entries/ORDER.txt 的那一行
★★★ 一手出处（本脚本是**切图**，不是自绘）
────────────────────────────────────────
一手：TH19 v1.10c `pl00.anm` 的 **script10**（= 灵梦 C1 那 4 发用的脚本）里写的是
      `sprite(sprite38)`；该 anm 引用的贴图是 **`player/pl00/pl00.png`**，
      而它的 entry 表写着 **`sprite38: { x: 1, y: 81, w: 30, h: 30 }`**。
⇒ 本脚本就是**从那张图切 (1,81,30,30)**，**1:1 原生切图、不改色**。
源（在 gitignored 的 `local/`，构建时取）：
  local/th19.v1.10c/anm/pl00f/player/pl00/pl00.png
  取法（见 HANDOFF-109-C1-C4.md §6.2）：
    thdat -x 19 local/th19.v1.10c/th19.dat pl00.anm
    thanm -x 19 pl00.anm player/pl00/pl00.png      # ⚠ 它会误报 not found，但文件其实已导出
⚠ 已知存疑：切出来**像灵梦的立绘碎片**（红白缎带），不像一颗"弹" —— 但那是**一手自己引用的**，
  照抄即可（见 HANDOFF-109-C1-C4.md §6.2 的记录）。
"""
import pathlib, sys
from PIL import Image

ROOT = pathlib.Path(__file__).resolve().parent          # = assets/ability/
# ★ 向上找仓库根（含 local/ 的那一层）—— 不数 "../" 的层数（数错过两次 ✗）
_root = ROOT
for _ in range(8):
    if (_root / "local").is_dir():
        break
    _root = _root.parent
SRC = _root / "local/th19.v1.10c/anm/pl00f/player/pl00/pl00.png"
OUT = ROOT / "POFV109_C1_BULLET.png"   # ★ 文件名 = ORDER 的 NAME
                                       #   ⇒ anm 内部名 = `ability/POFV109_C1_BULLET.png` ✓
                                       #   （build_ability.py:459 就是这么拼的 ✓）
RECT = (1, 81, 1 + 30, 81 + 30)      # ★ 一手 sprite38

def main() -> int:
    # 脚本可以从仓库根或 assets/ability 下跑 ⇒ 两种相对路径都试
    src = SRC
    if not src.exists():
        alt = _root / "local/th19.v1.10c/anm/pl00f/player/pl00/pl00.png"
        if alt.exists():
            src = alt
        else:
            print(f"缺源图：{SRC}（先按文件头那两条命令取出来）", file=sys.stderr)
            return 1
    im = Image.open(src).convert("RGBA")
    cut = im.crop(RECT)
    OUT.parent.mkdir(parents=True, exist_ok=True)
    cut.save(OUT)
    name = "POFV109_C1_BULLET"
    print(f"出图：{OUT}  {cut.width}x{cut.height}")
    print("\n要抄进 entries/ORDER.txt 末尾的那一行：")
    print(f"{name:<28} ability/{OUT.name:<28} {cut.width}x{cut.height}")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
