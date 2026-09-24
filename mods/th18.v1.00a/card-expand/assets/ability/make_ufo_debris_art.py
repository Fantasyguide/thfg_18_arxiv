#!/usr/bin/env python3
"""星莲船（系统卡 112）**召唤 UFO 那一刻的闪电碎屑** —— TH18 自己的 `bullet.anm` etbreak 家族 1:1 切图。

## 这一批是给谁用的（owner 2026-09-24 口径）

owner 原话（这场研究的起点，逐句都落到了实装里）：

1. 「我仔细研究了一下 UFO 的动画：召唤动画的**闪电**是随机画的蓝线？」
2. 「闪电是在召唤 UFO **同时**发生的…它的样式就是**类闪电**，并且似乎是**动态**的…
   拿了一个**蓝色的会有残影的像素，随机地让它向外运动**画出来的」
3. 「哦，并且**中间是洋红色**／**后面被 ufo 贴图盖住了**我没看出来」

⇒ 本卡在召唤那一刻撒一批「蓝色碎屑（暗层 + 加算亮层）往外飞」，正中心再压一颗洋红
（层号都**低于本体 VM 的 7** ⇒ 中心会被 UFO 贴图挡住 = owner 第 3 条）。

## 一手依据（可复核）

- **本作的现成资产**（先问「TH18 有没有现成代码」——有，就借全套，不自己画）：
  `local/th18.v1.00a/anm/bullet/bullet.anm.txt` 里的 **etbreak 家族**（`script160..313`）：
  同形、只差**颜色**与**尺寸档**的几十支脚本，素材是同一个 entry
  `bullet/effect/etbreak.png`（256×128）里的 **8 张 64×64 白色碎屑**（sprite540..547）。
- 家族结构（逐条读自 anm 文本）：
  - 子脚本一对 = **暗层**（`layer(20)` / 正常混合 / `alpha(64)`）+ **亮层**（`layer(21)` / `blendMode(1)` /
    `alpha(224)`），8 帧精灵循环（每 3 帧换一张 ⇒ owner 说的「残影」）；
  - 驱动器（`script173/176/179/…/227/…`）= `iset($I4,N)` + 随机等 0..7 帧 + 随机左右翻转
    （`scale(±1,1)`）+ `scriptNew(dim); scriptNew(glow)` + `jmpDec` + 尾部 `wait/delete`。
- 本卡取的那两色（**逐字节抄进脚本**，见 `assets/ability/scripts/346..350_ufo_summon_*.anm.txt`）：

| 用途 | 一手脚本 | 颜色 | 档 | 卡侧脚本 |
| --- | --- | --- | --- | --- |
| 蓝色碎屑（暗） | `script225` | `color(80, 80, 255)` | 第二档（±6 单位 / `scaleTime(24,0,0.7f)`）| 347 |
| 蓝色碎屑（亮） | `script226` | `color(80, 80, 255)` | 同上（`0.75f`、`alpha(224)`）| 348 |
| 中心洋红（暗） | `script219` | `color(224, 8, 224)` | 第二档 | 349 |
| 中心洋红（亮） | `script220` | `color(224, 8, 224)` | 第二档 | 350 |
| 发射器 | `script227` | — | 第二档驱动器（本卡撒 12 轮）| 346 |

- 精灵矩形：同一文件里 `name: "bullet/effect/etbreak.png"` 那个 entry 的 `sprites {}` 里
  `sprite540..sprite547`（**8 张连号**，本生成器逐张断言 = 64×64）。

⚠ 移植改动（层号 20/21 → 5/6、加「向外飞」的 `posTime`、撒的轮数）**全部**记在
`TUNING.md` §2.0zzf 与那五支脚本的文件头里 —— 本文件只负责**切图**，一个字都不改脚本。

## 输出

- `assets/ability/ufo_drop/DEBRIS_0.png … DEBRIS_7.png`（各 64×64，1:1 切图，不缩放不补边）
- ORDER 片段 `entries/ORDER.ufo_debris.txt`（**一个图集 entry、8 个成员**，照 `ORDER.txt` 的图集语法）

跑法（幂等）：
    python3 assets/ability/make_ufo_debris_art.py       # 切图 + 打 ORDER 片段
    # 把片段内容拼进 entries/ORDER.txt 的**末尾**（见文件末尾提示）
    make -C native anm                                  # 重编 ability.anm + anm_ids.h
"""
import re
import sys
from pathlib import Path

from PIL import Image

HERE = Path(__file__).resolve().parent
REPO = HERE.parents[4]
SRC_ANM = REPO / "local/th18.v1.00a/anm/bullet/bullet.anm.txt"
SRC_ENTRY = "effect/etbreak.png"          # anm 文本里的 entry 名（相对 `anm/bullet/`）
OUT_DIR = HERE / "ufo_drop"
ORDER_FRAG = HERE / "entries" / "ORDER.ufo_debris.txt"

FIRST, COUNT = 540, 8                 # 一手：sprite540..sprite547（8 帧连号）
CELL = (64, 64)                       # 一手：每张都是 64×64
ATLAS = "ATLAS_UFO_DEBRIS"
CANVAS = (256, 128)                   # 4×2 格 × 64 = 刚好放下 8 张


def parse_sprite_rects(text: str) -> dict:
    """→ {(entry 名, sprite 号): (x, y, w, h)}：`sprites { spriteN: { x: .., y: .., w: .., h: .. } }`。"""
    out = {}
    cur = None
    for line in text.splitlines():
        m = re.match(r'\s*name:\s*"([^"]+)"', line)
        if m:
            cur = m.group(1)
            continue
        m = re.match(r"\s*sprite(\d+):\s*\{\s*x:\s*(\d+),\s*y:\s*(\d+),\s*w:\s*(\d+),\s*h:\s*(\d+)\s*\}", line)
        if m and cur:
            out[(cur, int(m.group(1)))] = tuple(int(m.group(i)) for i in range(2, 6))
    return out


def main() -> int:
    if not SRC_ANM.exists():
        print(f"✖ 找不到原生 anm 文本：{SRC_ANM}", file=sys.stderr)
        return 1
    rects = parse_sprite_rects(SRC_ANM.read_text(encoding="utf-8", errors="replace"))
    # 原生贴图：`thanm -x` 的那份（`local/th18.v1.00a/anm/` 下按原文件分目录，entry 名直接接在它后面）
    src_png = SRC_ANM.parent / SRC_ENTRY
    if not src_png.exists():
        print(f"✖ 找不到原生贴图：{src_png}", file=sys.stderr)
        return 1
    sheet = Image.open(src_png).convert("RGBA")
    print(f"源：{src_png.relative_to(REPO)}（{sheet.width}×{sheet.height}）")

    OUT_DIR.mkdir(parents=True, exist_ok=True)
    frag = []
    for k in range(COUNT):
        num = FIRST + k
        rect = rects.get((SRC_ENTRY, num))
        if rect is None:
            print(f"✖ 原生 anm 的 entry {SRC_ENTRY} 里没有 sprite{num}", file=sys.stderr)
            return 1
        x, y, w, h = rect
        if (w, h) != CELL:
            print(f"✖ sprite{num} 的矩形是 {w}×{h}，本生成器假定 {CELL[0]}×{CELL[1]}", file=sys.stderr)
            return 1
        # ★ 画布坐标 → 图集成员坐标：本图集就是「原图 4×2 重排成 256×128」，逐格搬，不做任何缩放
        col, row = k % 4, k // 4
        ax, ay = col * CELL[0], row * CELL[1]
        out = OUT_DIR / f"DEBRIS_{k}.png"
        sheet.crop((x, y, x + w, y + h)).save(out)
        got = Image.open(out)
        assert (got.width, got.height) == CELL, (out, got.size)
        print(f"  sprite{num} ({x},{y},{w},{h}) → {out.relative_to(HERE)}（图集格 @{ax},{ay}）")
        frag.append(f"DEBRIS_{k}".ljust(22) + f"ability/ufo_drop/DEBRIS_{k}.png   @{ax},{ay},{CELL[0]}x{CELL[1]}")

    head = [
        "# 卡 112 的召唤闪电碎屑（**TH18 自己的** `bullet.anm` etbreak 家族：`anm/bullet/effect/etbreak.png`",
        f"#   的 sprite{FIRST}..sprite{FIRST + COUNT - 1} 八张 64×64 的 1:1 切图；生成器 =",
        "#   assets/ability/make_ufo_debris_art.py，矩形从原生 anm 文本读 + 逐张断言尺寸）。",
        f"#   用它的五支脚本 = 346..350（一手 = `bullet.anm` script227/225/226/219/220），理由与移植改动见",
        "#   那些脚本的文件头 + TUNING.md §2.0zzf。",
        f"ATLAS {ATLAS} {CANVAS[0]}x{CANVAS[1]}",
    ]
    ORDER_FRAG.write_text("\n".join(head + frag) + "\n", encoding="utf-8")
    print(f"ORDER 片段 → {ORDER_FRAG.relative_to(HERE.parent.parent)}（{ATLAS}，{COUNT} 个成员、一格 {CELL[0]}x{CELL[1]}）")
    print("  ⚠ 记得把这一段拼进 entries/ORDER.txt 的**末尾**（只能 append，见 build_ability.py 的重排检测）")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
