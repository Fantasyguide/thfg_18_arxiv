#!/usr/bin/env python3
"""星莲船（系统卡 112）**吸收进度环**的那一条竖条带 —— TH12 v1.00b 原版切图（**只切这一张**）。

## 切什么（一手，一处矩形）

`local/th12.v1.00b/anm/bullet/bullet/etama3.png`（**128×128**，一手：PNG 头 + 同一份 anm 文本里
`entry entry3` 的 `width: 128, height: 128, THTXWidth: 128, THTXHeight: 128` 四处对得上）里的
**sprite404 = `{x:49, y:0, w:14, h:128}`**：

- 谁在用：TH12 `bullet.anm` **script206**（= 光环树 script207 的第三个子脚本）的 `sprite(sprite404)`
  —— 它**不是贴图环**，而是「一条 14×128 的**竖条带**被引擎扫成圆弧」：
  `iset($I0,32); iset($I1,2); ins_101($I0); sprite(sprite404); scale(6.0f,59.0f); scrollY(0.025f);
   rotate(3.1415927f, 0.0f, 1.5707964f);`
  ⇒ 外径 = `scale.y + scale.x/2` = **62**、内径 = `scale.y − scale.x/2` = **56**。
- `ins_101` = **原始 opcode 101**（v4/v7 的 anmmap 没有它的助记符名 ⇒ thtk 印成兜底写法 `ins_101(`，
  见 `thanm.c` 的 `anm_stringify_instr`）。它设的是弧的**模式位** `flags_lo>>26`：
  TH18 的 handler `th18:0x47b211` 一手是 `and eax,0xb7ffffff; or eax,0x34000000`
  ⇒ `0x34000000>>26 = 13` ⇒ v8 助记符 = **`texArcEven`**（v8.anmm 的 **601**）；`602 texArc` = mode 14
  （`or 0x38000000`）。参数 = 弧的**段数**（原生 32）。
- 矩形**从 anm 文本的 sprites 表读**（下面 `read_sprite_rect()`），不写死；读不到就 `raise SystemExit`。

## 输出

- `assets/ability/ufo_gauge/UFO_ARC_STRIP.png`（**16×128**）
- ORDER 片段 `entries/ORDER.ufo_gauge.txt`（一行，`16x128` ⇒ 整个 16×128 贴图**一个** sprite）

### 为什么 14×128 要补成 16×128、且**居中**

`assets/ability/README.md` 与 `build_ability.py` 的 ORDER 约定：**贴图尺寸必须是 2 的幂**
（`template_for()` 直接 `w & (w-1) or h & (h-1)` 就 `raise SystemExit`；构建期也要求
`THTX ≤ 256×320` 那类 2 的幂字段自洽）。14 不是 2 的幂 ⇒ 补到 16。
**补的方向是「左右各 1 px 透明」＝居中**，理由是**不改变条带在贴图里的中心**：

- 引擎画 mode 13 的弧时按 sprite 矩形取 uv（`texArcEven` 的 `I0` 段把这条 uv 竖带贴到圆弧上），
  条带的**水平中心**就是「环的半径中心线」；把 14 宽的墨迹放在 16 宽画布的 `x=1..14`
  （两侧各留 1 px 透明）⇒ 墨迹中心 = 7.5 = 画布中心，**与源图里 sprite404 相对它自己 14 px 单元的中心
  完全一致**；若改成左对齐（`x=0..13`）墨迹中心就变成 6.5，环会整体偏半个像素。
  （y 方向 128 已经是 2 的幂 ⇒ 不动、不补、不裁。）

## 幂等 / 缺源

- 幂等：同一输入重复跑，输出**逐字节相同**（`source-over` 直接贴、无随机、无时间戳）。
- 缺源（`bullet.anm.txt` 或 `etama3.png`）⇒ `raise SystemExit` 并打印抽法；**不猜、不画替代品**。

跑法：`python3 make_ufo_gauge_art.py`（可重复运行）。
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

from PIL import Image

HERE = Path(__file__).resolve().parent
REPO = HERE.parents[4]                      # assets/ability → assets → card-expand → th18.v1.00a → mods → repo
SRC_PNG = REPO / "local/th12.v1.00b/anm/bullet/bullet/etama3.png"
SRC_TXT = REPO / "local/th12.v1.00b/anm/bullet/bullet.anm.txt"
OUT = HERE / "ufo_gauge"
ORDER_FRAG = HERE / "entries" / "ORDER.ufo_gauge.txt"

ENTRY_NAME = "bullet/etama3.png"
SPRITE_ID = 404                             # 一手：TH12 `bullet.anm` script206 的 `sprite(sprite404)`
EXPECT_RECT = (49, 0, 14, 128)              # 一手矩形（对账用；不是切图依据 —— 切图读的是 anm 文本）
CANVAS = (16, 128)                          # 2 的幂；见文件头「为什么居中」
NAME = "UFO_ARC_STRIP"


def read_sprite_rect() -> tuple[int, int, int, int]:
    """从 `thanm -l` 的 spec 里读出 `ENTRY_NAME` 那个 entry 的 `sprite404` 矩形 (x, y, w, h)。"""
    if not SRC_TXT.is_file():
        raise SystemExit(
            f"缺 TH12 bullet.anm 的文本 {SRC_TXT}（抽法：cd 到该目录，"
            f"local/vendor/thtk/build/thanm/thanm -l 12 -m local/vendor/thpages/static/mapfile/v4.anmm "
            f"../../dat/bullet.anm > bullet.anm.txt）"
        )
    txt = SRC_TXT.read_text(encoding="utf-8", errors="replace")
    m = re.search(r'name: "%s".*?sprites: \{(.*?)\n    \}' % re.escape(ENTRY_NAME), txt, re.S)
    if not m:
        raise SystemExit(f"{SRC_TXT}: 找不到 entry {ENTRY_NAME}")
    sm = re.search(r"sprite%d: \{ x: (\d+), y: (\d+), w: (\d+), h: (\d+) \}" % SPRITE_ID, m.group(1))
    if not sm:
        raise SystemExit(f"{SRC_TXT}: entry {ENTRY_NAME} 里没有 sprite{SPRITE_ID}")
    return tuple(int(g) for g in sm.groups())     # type: ignore[return-value]


def main() -> int:
    if not SRC_PNG.is_file():
        raise SystemExit(f"缺 TH12 原版贴图 {SRC_PNG}（retail 字节只在 local/，构建时取、不入库）")
    rect = read_sprite_rect()
    if rect != EXPECT_RECT:
        raise SystemExit(
            f"sprite{SPRITE_ID} 矩形从 anm 读出来是 {rect}，与本文件头登记的一手值 {EXPECT_RECT} 不符 —— "
            f"先去复核 `local/th12.v1.00b/anm/bullet/bullet.anm.txt` 再改 EXPECT_RECT（别直接改切图）"
        )
    x, y, w, h = rect
    for v in CANVAS:
        if v & (v - 1):
            raise SystemExit(f"画布尺寸必须是 2 的幂：{CANVAS}")
    cw, ch = CANVAS
    if w > cw or h > ch:
        raise SystemExit(f"画布 {CANVAS} 装不下单元 {w}x{h}")

    src = Image.open(SRC_PNG).convert("RGBA")
    if src.size != (128, 128):
        raise SystemExit(f"{SRC_PNG}: 期望 128×128（一手：PNG 头 + anm `entry entry3` 的 width/height = 128），"
                         f"实际 {src.size}")
    if x + w > src.size[0] or y + h > src.size[1]:
        raise SystemExit(f"sprite{SPRITE_ID} 矩形 {rect} 超出贴图 {src.size} —— anm 与 PNG 不是同一份，别切")
    strip = src.crop((x, y, x + w, y + h))
    vis = sum(1 for a in strip.getchannel("A").tobytes() if a > 32)
    if vis < 500:
        raise SystemExit(f"sprite{SPRITE_ID} @({x},{y},{w},{h}) 可见像素只有 {vis} —— 切错位置了")

    OUT.mkdir(parents=True, exist_ok=True)
    canvas = Image.new("RGBA", CANVAS, (0, 0, 0, 0))
    # 居中：两侧各留 (cw - w)/2 透明列（14→16 就是左右各 1 px；见文件头那张理由）
    canvas.paste(strip, ((cw - w) // 2, (ch - h) // 2))
    canvas.save(OUT / f"{NAME}.png")

    order_line = f"{NAME}  ability/ufo_gauge/{NAME}.png  {cw}x{ch}"
    ORDER_FRAG.write_text(
        "\n".join(
            [
                "# 星莲船（系统卡 112）**吸收进度环**的竖条带 —— TH12 v1.00b `bullet/etama3.png` 切图。",
                "# 本片段由 assets/ability/make_ufo_gauge_art.py 生成（幂等，整段重写）；合并时把下面这行追加进 ORDER.txt。",
                f"# 一处矩形（一手）：sprite{SPRITE_ID} = {{x:{x}, y:{y}, w:{w}, h:{h}}} —— TH12 `bullet.anm` script206 用的就是它。",
                f"# 14×128 不是 2 的幂 ⇒ 补成 {cw}×{ch}（左右各 1 px 透明、**居中**，见生成器文件头「为什么居中」）。",
                f"# 用法：脚本 228 `sprite(@{NAME}@)`（`texArcEven` 把它扫成半径 62/56 的圆弧）。",
                "# ⚠ 血条（脚本 231/232）**不需要贴图**：它是 `sprite(-1)` + `drawRect` ⇒ 本片段只有 1 行。",
            ]
            + [order_line]
        )
        + "\n",
        encoding="utf-8",
    )
    print(f"ufo_gauge: 1 张 {cw}x{ch} PNG -> {(OUT / (NAME + '.png')).relative_to(HERE.parent)}"
          f"（源 sprite{SPRITE_ID} {w}×{h} @({x},{y})，可见像素 {vis}，居中补透明）")
    print("\n# —— 追加到 assets/ability/entries/ORDER.txt（只追加）——")
    print(order_line)
    return 0


if __name__ == "__main__":
    sys.exit(main())
