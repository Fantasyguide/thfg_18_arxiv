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
落盘时**画布 16×32、字形居中**（上下各 6 px 透明）—— 于是「精灵中心」正好落在单元的水平中线，
C 侧按中心定位即可（见 `ce_hud_vm_text_set`）。
★ 2026-09-13（图集合并）：居中补边从「构建期由 `build_ability.py` 补」搬到**这里**（见 `CANVAS`），
于是 `ATLAS_FONT2` 的成员满足「矩形 = 原图自己的像素尺寸」（`ATLAS-MERGE-AUDIT.md` §4.1 规则①）；
补出来的贴图与合并前**逐字节相同**（旧路径就是 `pad.paste(im, ((16-16)//2, (32-20)//2))` = (0, 6)）。

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
OUT_B = HERE / "font2b"          # ★ 2026-09-24：给 `ATLAS_FONT2B` 用（同一批字形、独立图集）

# 名字 → (精灵号, 注释)
GLYPHS = [("FONT2_" + (str(i) if i >= 0 else ""), 196 + i, f"'{i}'") for i in range(10)] + [
    ("FONT2_MINUS", 208, "'-'"),
    ("FONT2_PERCENT", 210, "'%'"),
    ("FONT2_DOT", 211, "'.'"),
    ("FONT2_PLUS", 212, "'+'"),
    # ★★ 2026-09-23（owner 第二轮：「剩余时间画在模糊层之后，暂停不会模糊」）—— 把计时器从 ASCII
    #   换成**贴图 VM 版文字**，于是需要它用到的两个字形。**精灵号不是猜的**：
    #   引擎自己的「字符码 → 精灵号」表就在 `.rdata`：`th18:0x4b019c` / `0x4b029c` / `0x4b031c` /
    #   `0x4b041c`（各 256 字节，索引 = 字符码、值 = 精灵号；四张同值）。本机直读：
    #     `':'`(0x3a) → **206**、`'*'`(0x2a) → **190**、`'.'`(0x2e) → 194、`'-'`(0x2d) → 193、
    #     `'%'`(0x25) → 185、`'+'`(0x2b) → 191、`' '`(0x20) → 180、`'0'..'9'` → 196..205
    #     （⇒ 后五项与上表逐值相同，等于给上表做了交叉验证）。
    #   ⚠ 两者源矩形**不一样**：sprite206 = (160,320,**16×20**) 和数字同一行 ✓；
    #     sprite190 = (448,208,**16×16**) 在另一行 ⇒ 都按本文件的规矩「连单元一起切、落盘 16×32 居中」，
    #     于是 `*` 的墨迹在画布里比数字低 2 px（数字墨迹 20 行、它 16 行）—— `*` 只用在击破 ×N 弹字上，
    #     2 px 的基线差在观感里看不出来；要较真就得给 SDK 加第二套度量（不值得）。
    # ★★★ 2026-09-24 订正（owner 实跑：「**剩余时间怎么是 xx/yy 格式？这不太对吧**」）：
    #   引擎那张「字符码 → 精灵号」表（`th18:0x4b019c` 等四张同值）在**标点这一段是线性的**
    #   （`sprite = 180 + (char − 0x20)`）：`':'`(0x3a) → **206**、`'*'`(0x2a) → **190**。
    #   但**这张字形表（`ascii_anm` entry0 的 y=320 那一行）的标点顺序不是 ASCII 顺序** ——
    #   逐像素量出来的真身（`local/th18.v1.00a/anm/ascii1280/ascii/ascii_1280.png`）：
    #     · 206 = **`/`**（一条斜线）  ← 按字符表给 ':' 就会画成这个 ⇒ owner 看到的 `11/00` ✓
    #     · 207 = **`:`**（上下两点）  ← 真正的冒号
    #     · 208 = `-`（短横）、209 = **`×`**（乘号）、210 = `%`、211 = `.`（句点）、212 = `+`
    #   ⇒ 本表按**像素真身**取：`':'` = **207**、`'*'`（×N 弹字那个乘号，owner 原话是"大大的 ×一个数"）= **209**。
    #   ⚠ 190（`'*'` 在字符表里的位置）实测是一条**竖线**，不是星号 ⇒ 绝不能用它当乘号。
    ("FONT2_COLON", 207, "':'（像素真身；字符表给的是 206 = **斜线** ✗）"),
    ("FONT2_STAR", 209, "'×'（乘号；字符表给的是 190 = **竖线** ✗）"),
]
_SPRITE = re.compile(r"sprite(\d+): \{ x: (\d+), y: (\d+), w: (\d+), h: (\d+) \}")
# 落盘画布：**16×32**，字形（16×20）**纵向居中**（上下各 6 行透明）。
# ★ 2026-09-13（图集合并）：补边从「构建期由 `build_ability.py` 居中补」搬到**这里**，
#   于是图集成员满足「矩形 = 原图自己的像素尺寸」（`ATLAS-MERGE-AUDIT.md` §4.1 规则①；
#   `build_ability.py` 现在对图集成员**不许补边、尺寸不匹配就报错**）。
#   为什么矩形必须是 16×32 而不是字形的 16×20：`native/sdk.c:690` 的 `+5.0f * sy * dat`
#   是按「32 高的矩形里 20 行字形居中」标定的（§4.1 规则⑥），改了就是行为变化。
CANVAS = (16, 32)


def main():
    rects = {int(m.group(1)): tuple(int(m.group(i)) for i in (2, 3, 4, 5))
             for m in _SPRITE.finditer(SRC_TXT.read_text(encoding="utf-8"))}
    im = Image.open(SRC_PNG).convert("RGBA")
    OUT.mkdir(exist_ok=True)
    OUT_B.mkdir(exist_ok=True)
    for name, sid, comment in GLYPHS:
        if sid not in rects:
            raise SystemExit(f"{SRC_TXT.name} 里没有 sprite{sid}")
        x, y, w, h = rects[sid]
        cell = im.crop((x, y, x + w, y + h))
        pad = Image.new("RGBA", CANVAS, (0, 0, 0, 0))
        pad.paste(cell, ((CANVAS[0] - w) // 2, (CANVAS[1] - h) // 2))   # 不带 mask：原像素 1:1
        pad.save(OUT / f"{name}.png")
        '''★ 2026-09-24：同一张字形再落一份到 `font2b/`（`FONT2B_*`）——
         *   给**独立的图集** `ATLAS_FONT2B` 用：`ce_hud_vm_text_*` 的 VM 绑在哪个 entry 决定
         *   sprite 号往哪张表里查，而 `ATLAS_FONT2` 属 ORDER 的已定稿段（不能插行）
         *   ⇒ 需要 `:`/`*` 的调用点改用这一套（SDK 的 `CE_HUD_FONT_TH18_FONT2B`）。'''
        pad.save(OUT_B / f"FONT2B_{name[len('FONT2_'):]}.png")
        print(f"{name:<14} sprite{sid:<4} 源矩形 ({x},{y},{w},{h}) → 画布 {CANVAS[0]}x{CANVAS[1]}（居中）  {comment}")
    print(f"→ {OUT.relative_to(REPO)} + {OUT_B.relative_to(REPO)}（各 {len(GLYPHS)} 张 {CANVAS[0]}×{CANVAS[1]}；"
          f"ORDER.txt 的 ATLAS_FONT2 / ATLAS_FONT2B 里按同尺寸声明）")


if __name__ == "__main__":
    main()
