#!/usr/bin/env python3
"""109 灵力道具（合成卡的「灵力」货币）的外观贴图 —— **从 TH18 自己的原版道具图集 1:1 切**。

    python3 make_pofv109_power_art.py

产物：`ability/pofv109_power/POWER_P.png`（**32×32**）

## 为什么是这一格（一手）

owner 2026-10-01：「**灵力道具得按照类季节道具做，动态什么的一模一样**」＋
「**完全类似与信仰点、P点等**」。⇒ 外观取 **TH18 原生的 P 点**，不自己画。

一手链（全部在 TH18 自己的样本里，**不跨代借图**）：

| 步 | 出处 |
| --- | --- |
| ① `bullet.anm` 的 **entry3** 就是道具图集 | `local/th18.v1.00a/anm/bullet/bullet.anm.txt` 的 `entry entry3 { name: "bullet/item.png" }` |
| ② 它的 sprite 表里 **`sprite504 = {x:0, y:0, w:32, h:32}`** 是那张红 **P** 方块 | 同一份 `.txt` 的 sprite 表（见下） |
| ③ ★ **原版就是用它画 P 点的**：`script114` = `sprite(sprite504); originMode(1); type(1); angleVel(0,0,0.7853982f); scale(0.5f,0.5f); scaleTime(16,0,1.0f,1.0f);` | `bullet.anm.txt` 里 `script114` |
| ④ 该图集是 **256×64**（8 列 × 4 行的 32×32 网格 + 右上角 16×16 的小格族）| `Image.open(...).size` |

⇒ 「P 点 = `(0,0,32,32)`」**不是我挑的**，是 TH18 的 `script114` 自己引用的那格。
⇒ 109 的脚本（`assets/ability/scripts/374_pofv109_power.anm.txt`）据此照抄 `script114` 的**画法**
（`scale 0.5 → 1.0` 用 16 帧 + `angleVel` 自旋），**只改画在哪**（本卡的 VM 跟着道具走）。

## 纪律

- **1:1 切、不缩放、不重绘**（`METHOD.md`：贴图走原生切图，别自己画）。
- 原版字节只在 `local/`（gitignored）⇒ 本脚本**在构建时现取**，产物 PNG 入库。
- 图集尺寸变了就**直接报错**（不静默出一个尺寸不对的图）。
"""
from pathlib import Path

from PIL import Image

HERE = Path(__file__).resolve().parent          # assets/ability/
OUT = HERE / "pofv109_power"
REPO = HERE.parents[4]                           # 仓库根

#: TH18 原版道具图集（由 `tooling/thtk/unpack.py th18.v1.00a` 解出）
SRC_ITEM = REPO / "local/th18.v1.00a/anm/bullet/bullet/item.png"

#: (产物名, 源图集上的矩形) —— 一手：`sprite504 = (0,0,32,32)`，TH18 `script114` 就是用它画 P 点。
POWER_CELLS = (
    ("POWER_P", 0, 0, 32, 32),
)


def main():
    if not SRC_ITEM.is_file():
        raise SystemExit(f"缺 TH18 原版道具图集 {SRC_ITEM}（先 python3 tooling/thtk/unpack.py th18.v1.00a）")
    atlas = Image.open(SRC_ITEM).convert("RGBA")
    if atlas.size != (256, 64):
        raise SystemExit(f"道具图集尺寸变了：{atlas.size}（期望 256x64；sprite 表也随之要重核）")
    OUT.mkdir(exist_ok=True)
    for name, x, y, w, h in POWER_CELLS:
        cell = atlas.crop((x, y, x + w, y + h))
        cell.save(OUT / f"{name}.png")
        print(f"  {name}.png  {w}x{h}  ← item.png ({x},{y})")
    print(f"→ {OUT}")


if __name__ == "__main__":
    main()
