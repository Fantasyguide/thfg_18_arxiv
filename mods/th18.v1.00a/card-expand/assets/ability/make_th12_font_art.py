#!/usr/bin/env python3
"""卡 112（星莲船）击破弹字用的 **TH12 数字字形** —— 从原版 `ascii/ascii.png` 1:1 切图。

## owner 口径（2026-09-25 第 27 轮，原话）

> 「爆炸后的 **×n.0** 和**可能是点数**的字体完全不对，应该用的是：
>   `…\\[th12] 东方星莲船 (汉化版+日文版)\\ascii\\ascii.png` 里面**倒数第三行的数字**」
> 「点数的具体格式应该是 **400,000** 这样的，其中 **×n.0 应该是 scale 了一倍**」

## 一手：那一行是哪几个 sprite

`ascii.anm.txt`（TH12 v4 anm 的文本 dump）里 entry `ascii/ascii.png` 的精灵表：**16×16** 那一族
全部落在 **y = 216** 这一行（= 从底下数第三行：248 / 240 / 232 / **216**），`x = 0,16,…,224` 共 **15 格**：

| sprite | 内容 |
| --- | --- |
| **243..252** | `0 1 2 3 4 5 6 7 8 9`（逐格渲染核对过）|
| 253 | `/` |
| **254** | `.`（点） |
| 255 | 重复的 `5`（原图如此，不用） |
| **256** | 乘号字形（画成粗 `x`）—— 一手 `on_draw_16` 的格式串 `"*%1.1f"` 里的 `*` 就是它 |
| **257** | `,`（逗号，带下伸的小尾） |

⇒ 本图集只取**用得到的 13 格**：`D0..D9` + `DOT` + `COMMA` + `X`（乘号）。

## 输出

- `assets/ability/font_th12/{F12_D0..F12_D9,F12_DOT,F12_COMMA,F12_X}.png`（各 16×16，1:1 原样）
- ORDER 片段 `assets/ability/entries/ORDER.font_th12.txt`（**一个图集、13 个成员**，128×32@8x2）
  ⇒ 拼到 `ORDER.txt` 的**最末尾**（行序 = sprite 号，插中间会平移全库）
- 字形脚本 `assets/ability/scripts/362_font_th12_glyph.anm.txt` **由本脚本写**（另有 332 是 TH18 那套的孪生）

跑法（幂等）：
    python3 assets/ability/make_th12_font_art.py
    # 把 ORDER 片段拼到 entries/ORDER.txt 末尾
    make -C native anm
"""
import re
import sys
from pathlib import Path

from PIL import Image

HERE = Path(__file__).resolve().parent
REPO = HERE.parents[4]
SRC_ANM = REPO / "local/th12.v1.00b/anm/ascii/ascii.anm.txt"
SRC_PNG = REPO / "local/th12.v1.00b/anm/ascii/ascii/ascii.png"
ENTRY = "ascii/ascii.png"
OUT = HERE / "font_th12"
SCRIPTS = HERE / "scripts"
ORDER_FRAG = HERE / "entries" / "ORDER.font_th12.txt"

ATLAS = "ATLAS_FONT_TH12"
ATLAS_W, ATLAS_H, COLS = 128, 32, 8

# (输出名, TH12 sprite 号, 一手说明)  —— 顺序 = ORDER 行序 = sprite 号顺序
GLYPHS = [
    ("F12_D0", 243, "'0'"), ("F12_D1", 244, "'1'"), ("F12_D2", 245, "'2'"),
    ("F12_D3", 246, "'3'"), ("F12_D4", 247, "'4'"), ("F12_D5", 248, "'5'"),
    ("F12_D6", 249, "'6'"), ("F12_D7", 250, "'7'"), ("F12_D8", 251, "'8'"),
    ("F12_D9", 252, "'9'"),
    ("F12_DOT", 254, "'.'"), ("F12_COMMA", 257, "','"),
    ("F12_X", 256, "乘号（一手 `\"*%1.1f\"` 的 `*`）"),
]
SCRIPT_NUM = 361
SCRIPT_NAME = "font_th12_glyph"


def read_rects() -> dict[int, tuple[int, int, int, int]]:
    if not SRC_ANM.is_file():
        raise SystemExit(f"缺 TH12 ascii.anm 的文本 {SRC_ANM}")
    txt = SRC_ANM.read_text(encoding="utf-8", errors="replace")
    m = re.search(r'name: "%s".*?sprites: \{(.*?)\n    \}' % re.escape(ENTRY), txt, re.S)
    if not m:
        raise SystemExit(f"{SRC_ANM}: 找不到 entry {ENTRY}")
    out = {}
    for sm in re.finditer(r"sprite(\d+): \{ x: (\d+), y: (\d+), w: (\d+), h: (\d+) \}", m.group(1)):
        out[int(sm.group(1))] = tuple(int(g) for g in sm.groups()[1:])
    return out


def main() -> int:
    rects = read_rects()
    # 断死一手：这一族必须全是 16×16、且在同一行 y=216（"倒数第三行"）——漂了就当场红
    for _name, sid, _why in GLYPHS:
        r = rects.get(sid)
        if r is None:
            raise SystemExit(f"{SRC_ANM}: 没有 sprite{sid}")
        if r[2:] != (16, 16):
            raise SystemExit(f"sprite{sid} 不是 16×16（实际 {r[2]}×{r[3]}）—— 切错族了")
        if r[1] != 216:
            raise SystemExit(f"sprite{sid} 的 y = {r[1]}（期望 216 = 倒数第三行）—— 行漂了")
    if rects[243][0] != 0 or rects[252][0] != 144:
        raise SystemExit("数字那一排在 x 上不是从 0 起、步进 16 —— 版式漂了")

    src = Image.open(SRC_PNG).convert("RGBA")
    if src.size != (256, 256):
        raise SystemExit(f"{SRC_PNG}: 期望 256×256，实际 {src.size}")
    OUT.mkdir(parents=True, exist_ok=True)
    lines = []
    for i, (name, sid, _why) in enumerate(GLYPHS):
        x, y, w, h = rects[sid]
        unit = src.crop((x, y, x + w, y + h))          # ★ 1:1 原样，不缩放、不重采样
        vis = sum(1 for a in unit.getchannel("A").tobytes() if a > 32)
        if vis < 20:
            raise SystemExit(f"sprite{sid} @({x},{y}) 只有 {vis} 个可见像素 —— 切错位置")
        unit.save(OUT / f"{name}.png")
        # ⚠ 网格段（段头带 `@列x行`）里**不许**再写 `@x,y,WxH`（build_ability 会报错）—— 位置由网格定
        lines.append(f"{name:<18} ability/font_th12/{name}.png")
    ORDER_FRAG.write_text(
        "# 卡 112 击破弹字的 **TH12 数字字形**（TH12 `ascii/ascii.png` 倒数第三行 = y=216 的 16×16 那族）\n"
        "#   生成器 = assets/ability/make_th12_font_art.py（矩形从 `ascii.anm.txt` 现读 + 断死 16×16 / y=216）\n"
        "#   owner 2026-09-25 第 27 轮：「×n.0 和点数的字体应该用 ascii.png 里面倒数第三行的数字」\n"
        "#   ★ 本块 **append 在 ORDER.txt 最末尾**（行序 = sprite 号，插中间会平移全库）\n"
        f"ATLAS {ATLAS} {ATLAS_W}x{ATLAS_H}@{COLS}x{ATLAS_H // 16}\n" + "\n".join(lines) + "\n",
        encoding="utf-8")

    # 字形脚本（TH18 那两套 = 136 / 332 的孪生：只有 sprite 名与图集不同）
    SCRIPTS.mkdir(parents=True, exist_ok=True)
    (SCRIPTS / f"{SCRIPT_NUM}_{SCRIPT_NAME}.anm.txt").write_text(
        f"script script{SCRIPT_NUM} {{\n"
        f"    // 卡 112 击破弹字的字形 VM（**TH12 ascii 数字**版，图集 `{ATLAS}`）——\n"
        f"    // 脚本 136（`ATLAS_FONT2`）/ 332（`ATLAS_FONT2B`）的孪生：只有 sprite 名与图集不同。\n"
        f"    // ★ 为什么必须每套字形各一支脚本：`ce_hud_vm_text_*` 的字形 VM 绑在固定脚本上，\n"
        f"    //   引擎按**该脚本所属 entry 的精灵表**解析 `vm->sprite` ⇒ 跨 entry 指号会越界。\n"
        f"    // 由 `assets/ability/make_th12_font_art.py` 生成（改就改那里再跑一遍）。\n"
        f"    sprite(@F12_D0@);\n"
        f"    ins_431(1);\n"
        f"    type(1);\n"
        f"    layer(25);          // 与另外两套字形同层：UI 层 25 → 折到绘制桩 38（暂停时被模糊罩住）\n"
        f"    resolutionMode(2);\n"
        f"    pos(0.0f, 0.0f, 0.0f);\n"
        f"    alpha(255);\n"
        f"    stop();\n"
        f"    interruptLabel(1);\n"
        f"    delete();\n"
        f"}}\n", encoding="utf-8")

    print(f"切图 {len(GLYPHS)} 张 → {OUT}")
    print(f"ORDER 片段 → {ORDER_FRAG}（{ATLAS} {ATLAS_W}x{ATLAS_H}）")
    print(f"字形脚本 → {SCRIPTS / (str(SCRIPT_NUM) + '_' + SCRIPT_NAME + '.anm.txt')}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
