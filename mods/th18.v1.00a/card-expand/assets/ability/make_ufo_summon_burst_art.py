#!/usr/bin/env python3
"""卡 112 —— UFO **召唤爆闪/雷**（TH12 `enemy.anm` script134 + 子 script112）的原版贴图 1:1 切图。

owner 2026-09-26：「**把 TH12 的 script134/112 原样搬过来**（最忠实一手，工作量中等）」。

一手 `local/th12.v1.00b/anm/enemy/enemy.anm.txt`：entry1 `enemy/enemy2.png` 的
**sprite197..204 = 一行 8 格 32×32**（`(0,96)` 起、步进 32）—— 就是爆闪那 8 张：
  · script134 里 `sprite(sprite197..204)` 逐帧推（+ `spriteRand(sprite197, 8)`）
  · script112 里 `spriteRand(sprite197, 8)` = 从这 8 张里随机取一张当"一根雷"
输出：`assets/ability/ufo_burst/UFO_BURST_0..7.png`（各 32×32，1:1 原样）+ ORDER 片段
`entries/ORDER.ufo_burst.txt`（`ATLAS ATLAS_UFO_BURST 256x32@8x1`，**append 在 ORDER.txt 末尾**）。
跑法：python3 assets/ability/make_ufo_summon_burst_art.py && make -C native anm
"""
import re
import sys
from pathlib import Path
from PIL import Image

HERE = Path(__file__).resolve().parent
REPO = HERE.parents[4]
SRC_ANM = REPO / "local/th12.v1.00b/anm/enemy/enemy.anm.txt"
SRC_PNG = REPO / "local/th12.v1.00b/anm/enemy/enemy/enemy2.png"
ENTRY = "enemy/enemy2.png"
OUT = HERE / "ufo_burst"
ORDER_FRAG = HERE / "entries" / "ORDER.ufo_burst.txt"
N = 8

def main() -> int:
    txt = SRC_ANM.read_text(encoding="utf-8", errors="replace")
    m = re.search(r'name: "%s".*?sprites: \{(.*?)\n    \}' % re.escape(ENTRY), txt, re.S)
    if not m:
        raise SystemExit(f"{SRC_ANM}: 找不到 entry {ENTRY}")
    rects = {int(a): tuple(int(x) for x in b) for a, *b in
             re.findall(r'sprite(\d+): \{ x: (\d+), y: (\d+), w: (\d+), h: (\d+) \}', m.group(1))}
    src = Image.open(SRC_PNG).convert("RGBA")
    OUT.mkdir(parents=True, exist_ok=True)
    lines = []
    for i in range(N):
        sid = 197 + i
        r = rects.get(sid)
        if r is None or r[2:] != (32, 32):
            raise SystemExit(f"sprite{sid} 不是 32×32（实际 {r}）—— 切错族了")
        if r[0] != i * 32 or r[1] != 96:
            raise SystemExit(f"sprite{sid} 的矩形漂了：{r}（期望 x={i*32}, y=96）")
        unit = src.crop((r[0], r[1], r[0] + 32, r[1] + 32))     # ★ 1:1 原样
        if sum(1 for a in unit.getchannel("A").tobytes() if a > 32) < 20:
            raise SystemExit(f"sprite{sid} 几乎全透明 —— 切错位置")
        unit.save(OUT / f"UFO_BURST_{i}.png")
        lines.append(f"UFO_BURST_{i}          ability/ufo_burst/UFO_BURST_{i}.png")
    ORDER_FRAG.write_text(
        "# 卡 112 的 UFO 召唤**爆闪/雷**（TH12 `enemy.anm` entry1 `enemy2.png` sprite197..204 的 1:1 切图）\n"
        "#   生成器 = assets/ability/make_ufo_summon_burst_art.py（矩形从 anm 文本现读 + 断死 32×32/y=96）\n"
        "#   owner 2026-09-26：「把 TH12 的 script134/112 原样搬过来」\n"
        "#   ★ 本块 **append 在 ORDER.txt 最末尾**（行序 = sprite 号）\n"
        f"ATLAS ATLAS_UFO_BURST 256x32@{N}x1\n" + "\n".join(lines) + "\n", encoding="utf-8")
    print(f"切图 {N} 张 → {OUT}\nORDER 片段 → {ORDER_FRAG}")
    return 0

if __name__ == "__main__":
    sys.exit(main())
