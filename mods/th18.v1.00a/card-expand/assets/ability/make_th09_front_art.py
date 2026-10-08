#!/usr/bin/env python3
"""卡 109 左上角 HUD 用的 **TH09 `front.anm` / `front.png` 素材** —— 1:1 切图（照 `make_th12_font_art.py` 的剧本）。

## owner 口径（2026-10-08，原话）

> 「th09 左上角的都在 `…\\[th09] 东方花映塚…\\data\\ANM\\data\\front\\front.png` 里面，
>   核一下需要哪些，怎么切，放哪」
> 「1:1 抠图是我要的」

## 一手：这些东西在哪

* TH09 的 HUD 框/字形**不在** `ascii.anm`（那只是通用 14×14 字 ✓），也不在 `pl00.anm`（自机立绘 ✓）
  —— 在 **`front.anm`**（本仓样本里就有：`local/th09.v1.50a/th09.dat` → `front.anm`，1194104 B ✓）；
* `front.anm` entry0 = `data/front/front.png`（**512×512** ✓，98 sprite；本脚本只取左上角那 34 个）；
* 一手 sprite 表 = `thanm -l 9 front.anm`；**「框」= sprite7**（`320,0,80,32` =
  「Spell Point」牌 + 一个空心方框 ✓，数字写在框里 ✓）；
* 一手画法（`front.anm` script7/8/9… ✓，详见 `engine/versus/th09/01-versus-loop.md` §2.11）：
  框 `pos(-144,0)`、数字起点 `(-139,13)`、步进 **8**、`color(192,128,128)`、
  `alpha(0)` → `alphaTime(60,0,255)`（60 帧淡入 ✓）。

## 输出

* `assets/ability/th09front/TH09_HUD_*.png`（**34 张、1:1 原样**，不缩放、不重采样）
* ORDER 片段 `assets/ability/entries/ORDER.th09_hud.txt`（**一个图集、34 个成员**，512×512 显式矩形）
  ⇒ 拼到 `ORDER.txt` 的**最末尾**（行序 = sprite 号；插中间会平移全库 ⇒ DLL 与 `ability.anm` 必须同批）

跑法（幂等）：
    python3 assets/ability/make_th09_front_art.py
    # 把 ORDER 片段拼到 entries/ORDER.txt 末尾
    make -C native anm
"""
import re
import subprocess
import sys
import tempfile
from pathlib import Path

from PIL import Image

HERE = Path(__file__).resolve().parent
REPO = HERE.parents[4]
TH09_DAT = REPO / "local/th09.v1.50a/th09.dat"
THDAT = REPO / "local/vendor/thtk/build/thdat/thdat"
THANM = REPO / "local/vendor/thtk/build/thanm/thanm"

ENTRY = "data/front/front.png"
OUT = HERE / "th09front"
ORDER_FRAG = HERE / "entries" / "ORDER.th09_hud.txt"

ATLAS = "ATLAS_TH09_HUD"
ATLAS_W, ATLAS_H = 512, 512

# (输出名, TH09 sprite 号, 一手说明) —— 顺序 = 文件顺序 = ORDER 行序
MEMBERS = [
    ("SIDE_L", 4, "左侧栏竖边 16×448 ✓"),
    ("SIDE_R", 5, "右侧栏竖边 16×448 ✓"),
    ("PANEL", 6, "中间竖栏 32×448 ✓"),
    ("SPELLBOX", 7, "★「Spell Point」牌 + 数字空框 80×32 ✓（= 那个框）"),
    ("DIGIT_G0", 8, "灰白数字 '0' 8×11 ✓"),
    ("DIGIT_G1", 9, "灰白 '1'"), ("DIGIT_G2", 10, "灰白 '2'"), ("DIGIT_G3", 11, "灰白 '3'"),
    ("DIGIT_G4", 12, "灰白 '4'"), ("DIGIT_G5", 13, "灰白 '5'"), ("DIGIT_G6", 14, "灰白 '6'"),
    ("DIGIT_G7", 15, "灰白 '7'"), ("DIGIT_G8", 16, "灰白 '8'"), ("DIGIT_G9", 17, "灰白 '9'"),
    ("DIGIT_R0", 25, "红数字 '0' 8×11 ✓（第二套）"),
    ("DIGIT_R1", 26, "红 '1'"), ("DIGIT_R2", 27, "红 '2'"), ("DIGIT_R3", 28, "红 '3'"),
    ("DIGIT_R4", 29, "红 '4'"), ("DIGIT_R5", 30, "红 '5'"), ("DIGIT_R6", 31, "红 '6'"),
    ("DIGIT_R7", 32, "红 '7'"), ("DIGIT_R8", 33, "红 '8'"), ("DIGIT_R9", 34, "红 '9'"),
    ("SLASH", 18, "数字行尾单位/斜杠 16×10 ✓"),
    ("ICON0", 37, "状态图标（圆点）16×16 ✓"),
    ("ICON1", 36, "状态图标（圆点，另一色）"),
    ("ICON2", 35, "状态图标（空圈）"),
    ("ICON3", 38, "状态图标（×）"),
    ("ICON4", 39, "状态图标（樱）"),
    ("BAR_TOP0", 0, "顶部横条 320×16 ✓"),
    ("BAR_TOP1", 1, "横条 320×16 ✓"),
    ("BAR_TOP2", 2, "横条 320×16 ✓"),
    ("BAR_TOP3", 3, "横条 320×16 ✓"),
]

# 断死：尺寸必须与一手一致（漂了就当场红，不静默切错）
EXPECT = {
    "SPELLBOX": (80, 32), "SLASH": (16, 10),
    "SIDE_L": (16, 448), "SIDE_R": (16, 448), "PANEL": (32, 448),
    "BAR_TOP0": (320, 16),
}
EXPECT.update({f"DIGIT_{s}{i}": (8, 11) for s in "GR" for i in range(10)})
EXPECT.update({f"ICON{i}": (16, 16) for i in range(5)})
# 源 rect 断死（防取样跑偏）
SOURCE_RECTS = {7: (320, 0, 80, 32), 8: (320, 32, 8, 11), 25: (320, 48, 8, 11),
                18: (400, 32, 16, 10), 0: (0, 0, 320, 16)}


def run(cmd, cwd=None):
    p = subprocess.run([str(c) for c in cmd], cwd=cwd, capture_output=True, text=True)
    if p.returncode != 0:
        raise SystemExit(f"命令失败：{' '.join(str(c) for c in cmd)}\n{p.stderr[:400]}")
    return p.stdout


def read_rects():
    """从 TH09 样本现取 `front.anm` 的 sprite 表（不写死任何机器路径 ✓）。"""
    if not TH09_DAT.is_file():
        raise SystemExit(f"缺 TH09 样本 {TH09_DAT}")
    if not (THDAT.is_file() and THANM.is_file()):
        raise SystemExit("缺 thtk 工具（local/vendor/thtk/build/{thdat,thanm}）")
    with tempfile.TemporaryDirectory() as td:
        run([THDAT, "-x", "9", TH09_DAT, "front.anm"], cwd=td)
        txt = run([THANM, "-l", "9", "front.anm"], cwd=td)
    m = re.search(r'name: "%s".*?sprites: \{(.*?)\n    \}' % re.escape(ENTRY), txt, re.S)
    if not m:
        raise SystemExit(f"front.anm: 找不到 entry {ENTRY}")
    rects = {}
    for sm in re.finditer(r"sprite(\d+): \{ x: (\d+), y: (\d+), w: (\d+), h: (\d+) \}", m.group(1)):
        rects[int(sm.group(1))] = tuple(int(g) for g in sm.groups()[1:])
    for sid, want in SOURCE_RECTS.items():
        if rects.get(sid) != want:
            raise SystemExit(f"sprite{sid} 的 rect = {rects.get(sid)}，期望 {want} —— 源不对/版式漂了")
    return rects


def extract_png():
    """导出 entry0 的贴图（`thanm -x -u`）并读进内存。"""
    with tempfile.TemporaryDirectory() as td:
        run([THDAT, "-x", "9", TH09_DAT, "front.anm"], cwd=td)
        run([THANM, "-x", "9", "-u", "front.anm"], cwd=td)
        cand = [p for p in Path(td).rglob("*.png") if "front" in p.name and "mname" not in p.name]
        if not cand:
            raise SystemExit("thanm -x -u 没导出 front 贴图")
        img = Image.open(cand[0]).convert("RGBA")
        name = cand[0].name
    if img.size != (ATLAS_W, ATLAS_H):
        raise SystemExit(f"{name}: 期望 {ATLAS_W}×{ATLAS_H}，实际 {img.size}")
    return img


def layout(i):
    """512×512 内的显式排版（与写出的 ORDER 片段一致；互不重叠 ✓）。"""
    if i < 3:
        return (0 if i == 0 else (16 if i == 1 else 32), 0)
    if i == 3:
        return (384, 0)
    if 4 <= i < 14:
        return (384 + 8 * (i - 4), 32)
    if 14 <= i < 24:
        return (384 + 8 * (i - 14), 48)
    if i == 24:
        return (384, 64)
    if 25 <= i < 30:
        return (400 + 16 * (i - 25), 64)
    return (64, 448 + 16 * (i - 30))


def main():
    rects = read_rects()
    src = extract_png()
    OUT.mkdir(parents=True, exist_ok=True)
    lines = []
    for i, (name, sid, _why) in enumerate(MEMBERS):
        r = rects.get(sid)
        if r is None:
            raise SystemExit(f"front.anm: 没有 sprite{sid}")
        x, y, w, h = r
        if EXPECT.get(name) and (w, h) != EXPECT[name]:
            raise SystemExit(f"{name}（sprite{sid}）尺寸 {w}×{h} ≠ 期望 {EXPECT[name]} —— 切错族了")
        unit = src.crop((x, y, x + w, y + h))          # ★ 1:1 原样，不缩放、不重采样
        vis = sum(1 for a in unit.getchannel("A").tobytes() if a > 32)
        if vis < 8:
            raise SystemExit(f"{name}（sprite{sid}）@({x},{y}) 只有 {vis} 个可见像素 —— 切错位置")
        unit.save(OUT / f"TH09_HUD_{name}.png")
        ax, ay = layout(i)
        lines.append(f"{'TH09_HUD_' + name:<20} ability/th09front/TH09_HUD_{name}.png   @{ax},{ay},{w}x{h}")

    ORDER_FRAG.write_text(
        "# 卡 109 左上角 HUD 的 **TH09 `front.anm` 素材**（`data/front/front.png` 512×512，1:1 直裁）\n"
        "#   生成器 = assets/ability/make_th09_front_art.py（rect 从 `thanm -l 9 front.anm` 现读 + 断死尺寸/关键 rect）\n"
        "#   owner 2026-10-08：「th09 左上角的都在 …\\data\\ANM\\data\\front\\front.png 里面」「1:1 抠图是我要的」\n"
        "#   一手画法（框/数字的位置·步进·颜色·60 帧淡入）见 engine/versus/th09/01-versus-loop.md §2.11\n"
        "#   ★ 本块 **append 在 ORDER.txt 最末尾**（行序 = sprite 号，插中间会平移全库）\n"
        f"ATLAS {ATLAS} {ATLAS_W}x{ATLAS_H}\n" + "\n".join(lines) + "\n",
        encoding="utf-8")

    print(f"切图 {len(MEMBERS)} 张 → {OUT}")
    print(f"ORDER 片段 → {ORDER_FRAG}（{ATLAS} {ATLAS_W}×{ATLAS_H}，34 成员）")
    return 0


if __name__ == "__main__":
    sys.exit(main())
