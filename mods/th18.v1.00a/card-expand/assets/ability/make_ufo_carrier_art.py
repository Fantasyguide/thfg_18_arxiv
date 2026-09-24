#!/usr/bin/env python3
"""星莲船（系统卡 112）**UFO 令牌宿主的"皮肤"** —— TH12 `enemy.anm`（`enemy/enemy.png`）1:1 切图。

## 为什么有这一套（owner 2026-09-24 口径，原话）

> 「th12 原版掉落飞碟的怪……只有**大蝴蝶**和**杂鱼妖精**。其中，掉飞碟的大蝴蝶的贴图是
>  **`enemy.png` 的第一二行**；掉飞碟的杂鱼妖精是 **`enemy.png` 妖精贴图的第 4 行**；
>  并且她们头上会有 **`enemy.png` 第二行的右半部分的旋转飞碟特效**。
>  **得改谁能飞碟掉落，并且还得给掉的那个改帖图**」

⇒ 本卡把**指定的 TH18 原生敌人换皮成上面这三样**（TH18 自己没有蝴蝶/这种妖精），
并且只有它们掉令牌。三样东西逐条对到 TH12 的原始资料：

| 输出 | 一手 sprite | 一手脚本 | 说明 |
| --- | --- | --- | --- |
| `CARRIER_BFLY_0..7` | `enemy.anm` **sprite125..132** | **script45**（4 帧走位 + 左右翻转两套） | 64×64 的蝴蝶（白翅 + 粉花身体）= owner 说的「第一二行」 |
| `CARRIER_FAIRY_0..11` | **`sprite36..47`** | **script15**（36..39）+ **script16..19**（40..47）| ★ 32×32 的**完整**小妖精 = owner 说的「妖精贴图第 4 行」；**一行 12 帧 = 正面走位 + 转身/侧身**（owner：「左右的动态没有复刻」= 这 8 帧转身动画也要用上）|
| `CARRIER_MARK_0..5` | `sprite167..172` | **script98**（+ 子 100/101 两层灰影） | 64×32 的**旋转飞碟**（每 3 帧换一张），挂在头顶 `pos(0, -32)` |

## ★★ 2026-09-24 第 16 轮：妖精那三张图**切错了**（owner 实跑：「sprite 切分有问题」）

上一版把 `sprite84..87` 当成"妖精贴图第 4 行"切了 —— 实跑一看，那**不是妖精**，是
**大蝴蝶那张 64×64 图的四个 32×32 象限碎片**（`sprite84` = 左下、`sprite85` = 右下…），
所以换皮之后妖精看着像"只有头和尾没变"的一块残片。

一手复核（`enemy/enemy.png` 512×512 的精灵表 + 逐格渲染）：

| 区域 | 内容 |
| --- | --- |
| `sprite0..47`（y=256..383）| **完整的小妖精**，**4 行 × 12 帧 = 4 个颜色族**：0-11 蓝 / 12-23 红 / 24-35 绿 / 36-47 黄 |
| `sprite48..107`（y=0..127）| **不是独立精灵** —— 大蝴蝶/飞碟那些 64×64 图的象限碎片（125..136 才是整图）|
| `sprite125..132`（y=0，64×64）| 大蝴蝶（owner 的「第一二行」）|
| `sprite167..172`（64×32）| 头顶旋转飞碟（owner 的「第二行右半」）|

一手脚本（TH12 `enemy.anm`）：`script0` = 蓝妖精走位（sprite0..3）、`script5` = **红**（12..15）、
`script10` = **绿**（24..27）、**`script15` = 黄（36..39）** —— **第 4 行就是这一族** ✓
（旁证：TH12 的 `GirlRedA01` 写的就是 `anmSetMain(0, 5)`、`GirlBlueA01` 是 `anmSetMain(0, 0)`，
颜色与名字对得上；而我们上一版那个 `script35`（sprite84..87）落在碎片区，所以切出来是残片）。

⚠ 精灵矩形**从原生 anm 文本读**（`sprites {}` 表），不写死坐标；逐张断言尺寸，
与 `make_ufo_debris_art.py` / `make_ufo_drop_art.py` 同一套纪律。**不重画、不缩放**。

## 输出

- `assets/ability/ufo_carrier/{CARRIER_BFLY_0..7,CARRIER_FAIRY_0..3,CARRIER_MARK_0..5}.png`
- ORDER 片段 `entries/ORDER.ufo_carrier.txt`（**三个图集 entry**）

跑法（幂等）：
    python3 assets/ability/make_ufo_carrier_art.py
    # 把片段拼进 entries/ORDER.txt 的**末尾**
    make -C native anm
"""
import re
import sys
from pathlib import Path

from PIL import Image

HERE = Path(__file__).resolve().parent
REPO = HERE.parents[4]
SRC_ANM = REPO / "local/th12.v1.00b/anm/enemy/enemy.anm.txt"
SRC_ENTRY = "enemy/enemy.png"
OUT_DIR = HERE / "ufo_carrier"
ORDER_FRAG = HERE / "entries" / "ORDER.ufo_carrier.txt"

# (图集名, 输出名前缀, 一手 sprite 起号, 张数, 期望尺寸, 说明)
GROUPS = [
    ("ATLAS_UFO_CARRIER_BFLY",  "CARRIER_BFLY",  125, 8, (64, 64),
     "64×64 蝴蝶 8 张（一手 enemy.anm sprite125..132；走位用前 4 张 = script45）"),
    ("ATLAS_UFO_CARRIER_FAIRY", "CARRIER_FAIRY",  36, 12, (32, 32),
     "32×32 完整小妖精 12 帧（一手 sprite36..47 = 妖精贴图**第 4 行 / 黄族**整行："
     "36..39 = 正面走位（一手 script15）、40..47 = 转身/侧身（一手 script16..19））"),
    ("ATLAS_UFO_CARRIER_MARK",  "CARRIER_MARK",  167, 6, (64, 32),
     "64×32 头顶旋转飞碟 6 张（一手 sprite167..172；script98 每 3 帧换一张）"),
]
COLS = 8          # 每行放几张（行优先排布）


def parse_sprite_rects(text: str) -> dict:
    out, cur = {}, None
    for line in text.splitlines():
        m = re.match(r'\s*name:\s*"([^"]+)"', line)
        if m:
            cur = m.group(1)
            continue
        m = re.match(r"\s*sprite(\d+):\s*\{\s*x:\s*(-?\d+),\s*y:\s*(-?\d+),\s*w:\s*(\d+),\s*h:\s*(\d+)\s*\}", line)
        if m and cur:
            out[(cur, int(m.group(1)))] = tuple(int(m.group(i)) for i in range(2, 6))
    return out


def main() -> int:
    if not SRC_ANM.exists():
        print(f"✖ 找不到原生 anm 文本：{SRC_ANM}", file=sys.stderr)
        return 1
    rects = parse_sprite_rects(SRC_ANM.read_text(encoding="utf-8", errors="replace"))
    src_png = SRC_ANM.parent / SRC_ENTRY
    if not src_png.exists():
        print(f"✖ 找不到原生贴图：{src_png}", file=sys.stderr)
        return 1
    sheet = Image.open(src_png).convert("RGBA")
    print(f"源：{src_png.relative_to(REPO)}（{sheet.width}×{sheet.height}）")

    OUT_DIR.mkdir(parents=True, exist_ok=True)
    frag_head = [
        "# 卡 112 的**令牌宿主皮肤**（TH12 `enemy.anm` 的 1:1 切图；生成器 =",
        "#   assets/ability/make_ufo_carrier_art.py，矩形从原生 anm 文本读 + 逐张断言尺寸）：",
        "#   · ATLAS_UFO_CARRIER_BFLY  = 64×64 蝴蝶 8 张（sprite125..132；一手 script45 走位）",
        "#   · ATLAS_UFO_CARRIER_FAIRY = 32×32 小妖精 4 张（sprite84..87 = 妖精贴图第 4 行；script35）",
        "#   · ATLAS_UFO_CARRIER_MARK  = 64×32 头顶旋转飞碟 6 张（sprite167..172；script98）",
        "#   owner 口径与三样东西的一手对照见生成器文件头；用它们的三支脚本 = ability.anm 351..355。",
    ]
    frag_body = []
    for atlas, prefix, first, count, cell, note in GROUPS:
        cols = min(count, COLS)
        rows = (count + cols - 1) // cols
        canvas = (cols * cell[0], rows * cell[1])
        frag_body += ["", f"# {note}", f"ATLAS {atlas} {canvas[0]}x{canvas[1]}"]
        for k in range(count):
            num = first + k
            rect = rects.get((SRC_ENTRY, num))
            if rect is None:
                print(f"✖ 原生 anm 的 entry {SRC_ENTRY} 里没有 sprite{num}", file=sys.stderr)
                return 1
            x, y, w, h = rect
            if (w, h) != cell:
                print(f"✖ sprite{num} 的矩形是 {w}×{h}，本组假定 {cell}", file=sys.stderr)
                return 1
            col, row = k % cols, k // cols
            ax, ay = col * cell[0], row * cell[1]
            name = f"{prefix}_{k}"
            out = OUT_DIR / f"{name}.png"
            sheet.crop((x, y, x + w, y + h)).save(out)
            got = Image.open(out)
            assert (got.width, got.height) == cell, (out, got.size)
            print(f"  sprite{num} ({x},{y},{w},{h}) → {out.relative_to(HERE)}（图集格 @{ax},{ay}）")
            frag_body.append(f"{name}".ljust(26) + f"ability/ufo_carrier/{name}.png   @{ax},{ay},{cell[0]}x{cell[1]}")
    ORDER_FRAG.write_text("\n".join(frag_head + frag_body) + "\n", encoding="utf-8")
    print(f"ORDER 片段 → {ORDER_FRAG.relative_to(HERE.parent.parent)}（3 个图集 entry）")
    print("  ⚠ 记得把这一段拼进 entries/ORDER.txt 的**末尾**（只能 append）")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
