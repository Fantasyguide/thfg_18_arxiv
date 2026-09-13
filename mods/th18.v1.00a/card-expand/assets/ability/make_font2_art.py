#!/usr/bin/env python3
"""TH18 **font 2** 的字形切图（原生原样，不重画）—— 给「贴图 VM 版数字」用。

    python3 make_font2_art.py

为什么要切它：卡里的数字原来是 ASCII 文字（`AsciiManager__draw_string`），而 ASCII 文字
**永远画不到暂停模糊罩下面**（唯一的早于罩子的绘制趟 group 1 落在 surface 1 + 相机 0，
实跑不可见，见 `cards/season.c` 的组号注释）。改成贴图 VM 才能进 UI 层 → 被罩住。
贴图**必须用原生字形**，所以从原生 `ascii1280.anm` 的 `ascii_1280.png` 直接切。

一手依据（`local/th18.v1.00a/anm/ascii1280/ascii1280.anm.txt` + `ascii.anm.txt` 对照）：

| 字形 | 精灵号 | 映射出处 |
| --- | --- | --- |
| `0`..`9` | 196..205 | font 2/3：`char + 0x94`（`th18:0x41a2b0` switch case 2/3）|
| `-` | 208 | 固定 `0xd0` |
| `%` | 210 | 固定 `0xd2` |
| `.` | 211 | 固定 `0xd3`（**字距 4 而不是 7**）|
| `+` | 212 | 固定 `0xd4` |

每个字形都是 **16×20** 的单元（`y=320` 那一行），**连单元一起切、不要按墨迹裁** ——
`.` 的墨迹在单元第 12..17 行，裁掉留白就等于把它抬到基线以上，`%`/`+` 同理。
落到 `ability.anm` 时声明 **16×32**（2 的幂），构建脚本会把 16×20 居中补透明（上下各 6 px），
于是「精灵中心」正好落在单元的水平中线 —— C 侧按中心定位即可（见 `ce_hud_vm_text_set`）。

分辨率变体：引擎按全局 UI 缩放 `DAT_0056aca0` 选 `ascii1280`(>1.6) / `ascii_960`(>1.1) / `ascii`，
精灵号与映射**三套相同**、尺寸 16×20 / 12×15 / 8×10（`th18:0x419390`）。这里切 **ascii1280**
（1280×960 = 本 mod 的标准分辨率），脚本里 `resolutionMode(2)` 会让它按 `DAT×0.5` 缩放 ⇒
在 960 档自动变成 12×15 大小（重采样，不是引擎那套重画的原图，但尺寸对得上）。
"""
import re
from pathlib import Path

from PIL import Image

HERE = Path(__file__).resolve().parent
REPO = HERE.parents[4]                                # renkolab/
SRC_TXT = REPO / "local/th18.v1.00a/anm/ascii1280/ascii1280.anm.txt"
SRC_PNG = REPO / "local/th18.v1.00a/anm/ascii1280/ascii/ascii_1280.png"
OUT = HERE / "font2"

# 名字 → (精灵号, 注释)
GLYPHS = [("FONT2_" + (str(i) if i >= 0 else ""), 196 + i, f"'{i}'") for i in range(10)] + [
    ("FONT2_MINUS", 208, "'-'"),
    ("FONT2_PERCENT", 210, "'%'"),
    ("FONT2_DOT", 211, "'.'"),
    ("FONT2_PLUS", 212, "'+'"),
]
_SPRITE = re.compile(r"sprite(\d+): \{ x: (\d+), y: (\d+), w: (\d+), h: (\d+) \}")


def main():
    rects = {int(m.group(1)): tuple(int(m.group(i)) for i in (2, 3, 4, 5))
             for m in _SPRITE.finditer(SRC_TXT.read_text(encoding="utf-8"))}
    im = Image.open(SRC_PNG).convert("RGBA")
    OUT.mkdir(exist_ok=True)
    for name, sid, comment in GLYPHS:
        if sid not in rects:
            raise SystemExit(f"{SRC_TXT.name} 里没有 sprite{sid}")
        x, y, w, h = rects[sid]
        cell = im.crop((x, y, x + w, y + h))
        cell.save(OUT / f"{name}.png")
        print(f"{name:<14} sprite{sid:<4} ({x},{y},{w},{h})  {comment}")
    print(f"→ {OUT.relative_to(REPO)}（{len(GLYPHS)} 张，ORDER.txt 里按 16x32 声明）")


if __name__ == "__main__":
    main()
