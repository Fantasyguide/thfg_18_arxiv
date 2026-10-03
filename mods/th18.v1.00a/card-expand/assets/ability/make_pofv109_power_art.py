#!/usr/bin/env python3
"""109 灵力道具（合成卡的「灵力」货币）的外观贴图 —— **从 TH18 自己的原版道具图集 1:1 切**。

    python3 make_pofv109_power_art.py

产物：`ability/pofv109_power/POWER_P.png`（**32×32**）

## 为什么是这一格（一手）

owner 2026-10-01：「**灵力道具得按照类季节道具做，动态什么的一模一样**」＋
「**完全类似与信仰点、P点等**」。
★ 同日实跑反馈：「**灵力道具动态不对，样式不对！**」⇒ 第一版取的 **P 点（`sprite504` = `(0,0,32,32)`）
是错的**：① 尺寸不对（32×32 vs 一手的 16×16）；② 更糟的是它**就是原版 P 道具本身** ⇒
玩家分不清「灵力道具」和普通 P 点。⇒ 本版改取 **`sprite519` = `(224,0,16,16)` = 季节点**。

一手链（全部在 TH18 自己的样本里，**不跨代借图**）：

| 步 | 出处 |
| --- | --- |
| ① `bullet.anm` 的 **entry3** 就是道具图集 | `local/th18.v1.00a/anm/bullet/bullet.anm.txt` 的 `entry entry3 { name: "bullet/item.png" }` |
| ② ★ **季节点 = `sprite519`**，TH18 自己的图集里就有：`sprite519: { x: 224, y: 0, w: 16, h: 16 }` | 同一份 `.txt` 的 sprite 表（第 4171 行）|
| ③ ★ **原版画它的配方** = `script129`（见下）—— 这就是本卡脚本 374 逐条照抄的对象 | `engine/season/th16/01-season-data-and-tables.md` §4（一手逐字节）＋ `bullet.anm` |
| ④ 该图集是 **256×64** | `Image.open(...).size` |

**一手 `script129`（TH16 季节道具的出场动画，逐条）**：

```
-1:  sprite(519); blendMode(1);                     // 加算
     alpha(0); alphaTime(10, 0, 255);                // 10 帧淡入
     scale(2.0, 2.0); scaleTime(16, 0, 1.0, 1.0);    // 16 帧从 2 倍缩到 1 倍
 0:  rotate(0,0,%RANDRAD); fsetMul(%F0,%RANDF2,0.08726646); angleVel(0,0,%F0);   // 随机自转 ±5°/帧
+16: scaleTime(300, 0, 0.7, 0.7); alphaTime(300, 0, 128);   // 之后 300 帧缩到 0.7、淡到 alpha128
+20: blendMode(0); stop();
```

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

#: (产物名, 源图集上的矩形) —— 一手：**`sprite519 = (224,0,16,16)` = 季节点**
#: （TH16 `script129` 就是用它画季节道具；TH18 自己的 `bullet.anm` 里 `sprite519` 同格同号 ✓）
POWER_CELLS = (
    ("POWER_P", 224, 0, 16, 16),
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
