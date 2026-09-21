#!/usr/bin/env python3
"""妖妖梦（TH07 系统卡 107）**HUD 数字 + 森罗结界指示**的贴图：从 TH07 原版 `ascii.anm` 里 1:1 切。

    python3 make_senro_hud_art.py

一手来源（全部在 `local/`，gitignored，构建时取；不入库）
────────────────────────────────────────────────────────
* 原版 anm：`local/th07.v1.00b/dat/ascii.anm`（anm **version 2**，2 个 entry）
    切好的贴图：`local/th07.v1.00b/anm/ascii/data/ascii/ascii.png`（256×256）
* anm 无内嵌贴图 → 用 `thanm -x 7` 兜底解到临时目录（不往 `local/` 写）。

**这两组贴图是谁**（一手 = `AsciiManager::sub_404690` = TH07 画「樱点计数器」的那个函数）：

| 贴图 | 原版 sprite | 用途（一手地址） |
| --- | --- | --- |
| `D0.png`..`D9.png` | `ascii.anm` entry0 **sprite132..141**（各 **8×12**） | HUD 的**数字字形**。画数字时 `sprite = digit + 0x84`（`th07:0x404a41 add edx,0x84` / `0x404d7d` 同一处，两行数字 + +C 行都用它） |
| `BORDER.png` | entry0 **sprite143**（**48×16**） | **森罗结界指示**：结界中才画的那块（VM4 = `ascii.anm` **script5**，`ins_36(30,4,1.2,1.2)` ↔ `(30,1,0.8,0.8)` 无限脉动）。一手判据 = `th07:0x404de1 movsx eax,BYTE PTR ds:0x4bfee5; cmp eax,1; jne` 才画 ⇒ 只在 `ds:0x4bfee5 == 1`（= **结界中**，同址也是 `th07:0x42f5f7` 的 +C 累加闸门）时出现 |

**为什么是 sprite132..141 而不是别的**（三条独立证据）：
1. `AsciiManager::reset` `th07:0x401a00` 给内嵌 VM3（`this+0x6e4`）装 **script3**（`th07:0x401be3`），
   而 `script3` 的正文只有 `ins_3(sprite132); ins_30(1); ins_2();` ⇒ VM3 就是「一个数字」的载体。
2. 画数字那两段循环里 `sprite = 数字 + 0x84`（`0x404a41`/`0x404d7d`），共 6+6+5 位 ⇒ 132..141。
3. 贴图侧：`sprite132..141` = `(0,208)..(72,208)` 十个 **8×12** 等距字形（本脚本从 `thanm -l` 读矩形断死）。

**没切的那张**（本卡这一轮**不用**，留档）：entry0 **sprite142**（96×16）—— 数字簇的「锚」：
`script4`（`th07:0x401b22` 装进 AsciiManager 的 VM2）把它从 `(-96,449)` 用 `posTime(15,0,32,449)`
滑到 **(32,449)** 定住，而 HUD 的数字全部按 `[VM2.pos] + 常数` 摆（`T.x+46/T.y+11` 那两行）。
⇒ 它就是**数字左边那串东西**（原版 HUD 的固定标签）。不切的理由与影响见 `cards/senro.c` 的
`SENRO_HUD_*` 注释（本模型无法目视确认它画的是什么；且它落在数字左侧 ~70 单位，本卡的货架块装不下）。

尺寸 / 缩放（为什么脚本里要 ×2）
─────────────────────────────────
* 原样切、不重采样（纯原生切图）——**8×12 / 48×16 就是原版矩形尺寸**。
* TH07 是 640×480 原生：1 texel = 1 个「640 单位」；TH18 的 UI 层 `resolutionMode(2)` 下
  1 texel = 1 **表面 px**（1280×960），而 640 单位 = 2 表面 px ⇒ 想还原原版观感要在脚本里
  `scale(2.0, 2.0)`（先例 = `scripts/302_ufo_hud_slot.anm.txt` 的 Δ4 同一条换算）。

产物：`ability/senro_hud/D0.png`..`D9.png`、`BORDER.png`；
stdout 打印要追加进 `entries/ORDER.txt` 的行（也写在 `entries/ORDER.senro_hud.txt`）。
幂等：可重复运行，输出逐字节一致。
"""
import re
import subprocess
import sys
import tempfile
from pathlib import Path

from PIL import Image

HERE = Path(__file__).resolve().parent
REPO = HERE.parents[4]

ANM = REPO / "local/th07.v1.00b/dat/ascii.anm"
THANM = REPO / "local/vendor/thtk/build/thanm/thanm"
SRC = REPO / "local/th07.v1.00b/anm/ascii/data/ascii/ascii.png"
OUT = HERE / "senro_hud"

# 切哪几个 sprite：名字 → (原版 sprite 号, 期望尺寸, 含义)
CROPS = ([("D%d" % i, 132 + i, (8, 12), "HUD 数字字形 %d" % i) for i in range(10)]
         + [("BORDER", 143, (48, 16), "森罗结界指示（结界中才画的那块）")])

# 本卡这一轮不切的那张（留档 + 断死它仍在原版里）：数字簇的锚（标签）
UNUSED = [("LABEL", 142, (96, 16), "数字左边那串固定标签（VM2 / script4）—— 见文件头「没切的那张」")]


def sprite_rects() -> dict:
    """从 `thanm -l 7` 的 entry0 读全部 sprite 矩形（**不硬编码**，矩形错了就报错）。"""
    if not (ANM.is_file() and THANM.is_file()):
        raise SystemExit(f"缺 {ANM} 或 {THANM}（TH07 样本 + thtk 工具链）")
    txt = subprocess.run([str(THANM), "-l", "7", str(ANM)],
                         capture_output=True, text=True, check=True).stdout
    rects, entry = {}, None
    for line in txt.split("\n"):
        m = re.match(r"\s*entry entry(\d+)", line)
        if m:
            entry = int(m.group(1))
            continue
        m = re.match(r"\s*sprite(\d+): \{ x: (\d+), y: (\d+), w: (\d+), h: (\d+)", line)
        if m and entry == 0:
            rects[int(m.group(1))] = tuple(int(m.group(i)) for i in (2, 3, 4, 5))
    if len(rects) < 146:
        raise SystemExit(f"entry0 只读到 {len(rects)} 个 sprite，预期 ≥146（ascii.anm 变了？）")
    return rects


def texture() -> Image.Image:
    if SRC.is_file():
        return Image.open(SRC).convert("RGBA")
    with tempfile.TemporaryDirectory() as td:                     # 兜底：直接解 anm，不往 local/ 写
        subprocess.run([str(THANM), "-x", "7", str(ANM)], cwd=td, check=True,
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        p = Path(td) / "data/ascii/ascii.png"
        if not p.is_file():
            raise SystemExit(f"缺 {SRC}，且 thanm 解包没产出 ascii.png")
        return Image.open(p).convert("RGBA").copy()


def pack_rects() -> list:
    """把成员摆进图集画布（显式矩形，行优先；画布 = 各行的最大宽度 × 行高之和）。

    本段成员都是小图 ⇒ 一行放完：十个 8×12 数字 + 一块 48×16。
    产物 `(name, sid, (x, y, w, h), 画布)`。"""
    out, x, row_y, row_h = [], 0, 0, 0
    for name, sid, (w, h), _label in CROPS:
        if x + w > 256:                                     # 画布宽上限（本段用不到，留着防手滑）
            row_y += row_h + 2
            x, row_h = 0, 0
        out.append((name, sid, (x, row_y, w, h)))
        x += w
        row_h = max(row_h, h)
    canvas = (max(x, 1), row_y + max(row_h, 1))
    return out, canvas


def main():
    rects = sprite_rects()
    img = texture()
    if img.size != (256, 256):
        raise SystemExit(f"{'ascii.png'} 是 {img.size}，预期 (256, 256)")
    OUT.mkdir(exist_ok=True)

    layout, canvas = pack_rects()
    order_lines = []
    for name, sid, (dx, dy, w, h) in layout:
        x, y, sw, sh = rects[sid]
        if (sw, sh) != (w, h):
            raise SystemExit(f"{name}: sprite{sid} 矩形是 {(sw, sh)}，预期 {(w, h)}（表变了？）")
        tile = img.crop((x, y, x + sw, y + sh))
        if tile.getbbox() is None:
            raise SystemExit(f"{name}: sprite{sid} {(x, y, sw, sh)} 切出来全透明（矩形抄错了）")
        tile.save(OUT / f"{name}.png")
        ink = sw * sh - tile.getchannel("A").histogram()[0]
        label = [c[3] for c in CROPS if c[0] == name][0]
        print(f"# {name:<7} sprite{sid:<4} 源({x},{y},{sw},{sh}) → 画布 @{dx},{dy},{w}x{h}"
              f"  墨迹 {ink:>4}/{w * h:<4} ← {label}")
        order_lines.append(f"{'SENRO_HUD_' + name:<22} ability/senro_hud/{name}.png"
                           f"{' ' * max(1, 12 - len(name))}@{dx},{dy},{w}x{h}")

    for name, sid, want, label in UNUSED:
        x, y, w, h = rects[sid]
        if (w, h) != want:
            raise SystemExit(f"{name}: sprite{sid} 矩形是 {(w, h)}，预期 {want}（表变了？）")
        print(f"# （不切）{name:<7} sprite{sid:<4} ({x},{y},{w},{h}) ← {label}")

    print(f"senro_hud: {len(CROPS)} 张 -> {OUT.relative_to(HERE.parent)}（画布 {canvas[0]}x{canvas[1]}）")
    print("# ORDER 追加行（也见 entries/ORDER.senro_hud.txt）：")
    print(f"ATLAS ATLAS_SENRO_HUD {canvas[0]}x{canvas[1]}")
    for line in order_lines:
        print(line)
    return 0


if __name__ == "__main__":
    sys.exit(main())
