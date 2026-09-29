#!/usr/bin/env python3
"""季节（系统卡 116）**季节子机**的逐季节动画帧 —— 从 TH16 原版副自机贴图切（本卡私有资产）。

    python3 make_season_opt_anim_art.py

产出（`assets/ability/season/`）：

    OPT_SUMMER_2/3/4.png   16×16   夏：TH16 `pl02sub.anm` entry1 sprite5/6/7（script1 的 4 帧循环）
    OPT_FALL_2..8.png      16×16   秋：TH16 `pl03sub.anm` entry1 sprite5..11（script1 的 8 帧循环）

★ 一手依据（**脚本号 + 帧数 + 关键指令 + 层**，全部落在 TH16 原版 anm 文本上）：

  TH16 `PlayerInner__repopulate_options` 的季节子机脚本号 = `th16:0x492be0[SUBSEASON]` = **{1, 1, 1, 0x12, 1}**
  （春/夏/秋/土用 = script1，**冬 = `pl01sub.anm` 的 script18**；`th16:0x44477b`）；
  副 anm 文件 = `th16:0x492cdc[SUBSEASON]` = **{pl00sub, pl02sub, pl03sub, pl01sub, pl04sub}**。
  脚本本体（逐条照抄，见 `assets/ability/scripts/364..368_*.anm.txt`）：
    · 春 `pl00sub` script1  ：layer(13) blendMode(1) type(1)；scaleTime(10,4, 0.8,0.8) → (6,1, 0.6,0.6)；
                              sprite14 单帧；angleVel(0,0,+0.31415927) → (0,0,−0.10471976)
    · 夏 `pl02sub` script1  ：layer(13) blendMode(1) type(3)；scaleTime(10,4, 1.0,1.0) → (6,1, 0.6,0.6)；
                              **sprite4→5→6→7 每 4 帧一换**（`jmp(offset136, 16)`）
    · 秋 `pl03sub` script1  ：layer(13) type(0) resampleMode(1)；scaleTime(10,4, 1.0,1.0)（**之后不缩**）；
                              **sprite4..sprite11 每 4 帧一换**（8 帧，`jmp(offset88, 0)`）
    · 冬 `pl01sub` script18 ：layer(13) blendMode(1) type(2)；scaleTime(10,4, 1.0,1.0) → (6,1, 0.6,0.6)；
                              sprite3 单帧；之后 scaleTime(15,9, 0.5,0.5) ↔ (15,9, 0.6,0.6) 呼吸
    · 土用 `pl04sub` script1：同春（sprite6 单帧）
  ⇒ **夏/秋两季的「各自动画」就是多帧 sprite 循环**，春/冬/土用是单帧 + 角度/缩放动画
  （它们的动画靠脚本的 angleVel / scaleTime，不需要新贴图）。

★ 切图规矩（与 `make_season_art.py` 的 `make_opts()` 同一套，2026-09-10 实跑踩出来的坑）：
  TH16 的 anm 是**同页多 texture**，每个 entry 有自己的 `xOffset/yOffset`，sprite 的 (x,y) 是
  **相对该 entry 的** ⇒ 必须加偏移，否则切到的是空白区。
  ★★ 本生成器的矩形**从 anm 的 sprites 表读**（不是手抄坐标）：解析
  `local/th16.v1.00a/anm/<副>/<副>.anm.txt` 的 `entry entry1` 块，取 xOffset/yOffset + 目标 sprite 的
  `{x,y,w,h}`，再断言 `w,h == 16`（= 2 的幂、且与原图**同尺寸** ⇒ **不需要补边/居中**；
  土用那种 30×14 需要补边的特例不在这里 —— 本文件的 10 帧全是 16×16）。

  ⚠ 生成器**只写这 10 张新帧**；第 1 帧（`OPT_SUMMER.png` / `OPT_FALL.png`）是既有的
  `make_season_art.py:make_opts()` 产出的，这里**只核对**（同矩形、逐像素相同）不重写，
  避免动到既有定稿资产。
"""
import hashlib
import re
import sys
from pathlib import Path

import numpy as np
from PIL import Image

HERE = Path(__file__).resolve().parent
OUT = HERE / "season"
REPO = HERE.parents[4]                      # assets/ability -> assets -> card-expand -> th18.v1.00a -> mods -> repo
TH16 = REPO / "local/th16.v1.00a/anm"

# —— 要切的帧：TH16 副 anm 文件 + entry + sprite 名 + 目标文件名 ——
# entry1 就是「副自机的小道具/子机」那一页（entry0 = 副 sht 的弹贴图页）。
# ★ 目标文件名里的序号 = **TH16 脚本里的帧序**（第 1 帧沿用既有 OPT_*.png）。
WANT = (
    # (副 anm 目录, entry, sprite, 输出名, 该副季节的帧序说明)
    ("pl02sub", "entry1", "sprite5",  "OPT_SUMMER_2", "夏 script1 第 2 帧"),
    ("pl02sub", "entry1", "sprite6",  "OPT_SUMMER_3", "夏 script1 第 3 帧"),
    ("pl02sub", "entry1", "sprite7",  "OPT_SUMMER_4", "夏 script1 第 4 帧"),
    ("pl03sub", "entry1", "sprite5",  "OPT_FALL_2",   "秋 script1 第 2 帧"),
    ("pl03sub", "entry1", "sprite6",  "OPT_FALL_3",   "秋 script1 第 3 帧"),
    ("pl03sub", "entry1", "sprite7",  "OPT_FALL_4",   "秋 script1 第 4 帧"),
    ("pl03sub", "entry1", "sprite8",  "OPT_FALL_5",   "秋 script1 第 5 帧"),
    ("pl03sub", "entry1", "sprite9",  "OPT_FALL_6",   "秋 script1 第 6 帧"),
    ("pl03sub", "entry1", "sprite10", "OPT_FALL_7",   "秋 script1 第 7 帧"),
    ("pl03sub", "entry1", "sprite11", "OPT_FALL_8",   "秋 script1 第 8 帧"),
)

# 第 1 帧（既有资产）→ 复核用：同 entry/sprite
FIRST_FRAME = (
    ("pl02sub", "entry1", "sprite4", "OPT_SUMMER.png", "夏 script1 第 1 帧"),
    ("pl03sub", "entry1", "sprite4", "OPT_FALL.png",   "秋 script1 第 1 帧"),
)

_SRC = {                       # 副 anm 目录 → 贴图（相对仓库根；原版字节只在 local/，不入库）
    "pl02sub": "local/th16.v1.00a/anm/pl02sub/player/pl02/pl02.png",
    "pl03sub": "local/th16.v1.00a/anm/pl03sub/player/pl03/pl03.png",
}
_ENTRY = re.compile(r"^entry\s+(\w+)\s*\{", re.M)
_OFF = re.compile(r"^\s*(xOffset|yOffset):\s*(\d+)\s*,", re.M)
_SPR = re.compile(r"^\s*(sprite\d+):\s*\{\s*x:\s*(\d+),\s*y:\s*(\d+),\s*w:\s*(\d+),\s*h:\s*(\d+)\s*\}", re.M)


class AnmEntry:
    """一个 anm 文本里的 entry：xOffset/yOffset + sprite 矩形表（**矩形从这里读，不手抄**）。"""

    def __init__(self, sub: str, entry: str):
        txt = TH16 / sub / f"{sub}.anm.txt"
        if not txt.is_file():
            raise SystemExit(f"缺 TH16 原版 anm 文本 {txt}（见 README 的 unpack 说明）")
        text = txt.read_text(encoding="utf-8", errors="replace")
        # 按 entry 切块（块 = `entry X {` 到下一个 `entry` 或文件尾）
        marks = [(m.group(1), m.start()) for m in _ENTRY.finditer(text)]
        body = None
        for i, (name, pos) in enumerate(marks):
            if name == entry:
                end = marks[i + 1][1] if i + 1 < len(marks) else len(text)
                body = text[pos:end]
                break
        if body is None:
            raise SystemExit(f"{txt.name} 里没有 {entry}")
        off = dict((k, int(v)) for k, v in _OFF.findall(body))
        if "xOffset" not in off:
            off["xOffset"] = 0                       # entry0 常省略（= 0）
        if "yOffset" not in off:
            off["yOffset"] = 0
        self.xoff, self.yoff = off["xOffset"], off["yOffset"]
        self.sprites = {n: (int(x), int(y), int(w), int(h))
                        for n, x, y, w, h in _SPR.findall(body)}
        if not self.sprites:
            raise SystemExit(f"{txt.name} 的 {entry} 没有 sprite 表")

    def rect(self, sprite: str, use_offset: bool):
        if sprite not in self.sprites:
            raise SystemExit(f"sprite 表里没有 {sprite}（有 {sorted(self.sprites)}）")
        x, y, w, h = self.sprites[sprite]
        if use_offset:
            x, y = x + self.xoff, y + self.yoff
        return (x, y, w, h)


def _atlas(sub: str) -> Image.Image:
    src = REPO / _SRC[sub]
    if not src.is_file():
        raise SystemExit(f"缺 TH16 原版自机贴图 {src}")
    return Image.open(src).convert("RGBA")


def _crop(sub: str, ent: "AnmEntry", sprite: str, use_offset: bool):
    atlas = _atlas(sub)
    x, y, w, h = ent.rect(sprite, use_offset)
    if (w, h) != (16, 16):
        # 本文件只处理 16×16 的帧；别的尺寸要谈「补边/居中」策略，不在这里默默补
        raise SystemExit(f"{sub} {sprite} 的矩形是 {w}×{h}，期望 16×16（脚本的 scale 按 16 算的）")
    if x < 0 or y < 0 or x + w > atlas.width or y + h > atlas.height:
        return None                                   # 这种读法越界 ⇒ 不是它
    return atlas.crop((x, y, x + w, y + h))


def pick_convention(sub: str, ent: "AnmEntry", first_sprite: str, first_png: Path) -> bool:
    """定「这个副 anm 的 entry1，sprite 坐标要不要再加 xOffset/yOffset」。

    ⚠️ ZUN 的 anm 里两种写法都有（同一个仓的两支副 anm 就不一样）—— 实测：
      · `pl00sub/pl01sub/pl03sub/pl04sub` 的 entry1 **要加**偏移；
      · `pl02sub` 的 entry1 **不能加**（它的 sprite 坐标已经是整页绝对坐标；
        加了就读到页外：`pl02.png` 只有 128×160，而 (80,144)+偏移 = (144,288) 越界）。
    判据不靠猜：拿**既有的、已定稿的第 1 帧 PNG**（`OPT_SUMMER.png` / `OPT_FALL.png`，
    由 `make_season_art.py:make_opts()` 切、`ATLAS-MERGE-AUDIT` §2.11 登记）逐像素比对 ——
    哪个读法与它完全相同就用哪个；两个都不行就直接报错（说明第 1 帧本身错了）。"""
    if not first_png.is_file():
        raise SystemExit(f"缺既有第 1 帧 {first_png}（应由 make_season_art.py 生成）")
    want = np.asarray(Image.open(first_png).convert("RGBA"))
    ok = []
    for use_off in (False, True):
        piece = _crop(sub, ent, first_sprite, use_off)
        ok.append(piece is not None and np.array_equal(np.asarray(piece), want))
    if ok[0] == ok[1]:
        raise SystemExit(f"{first_png.name} 无法判定 {sub} 的偏移约定（两种读法结果一样或都不行）")
    return ok[1]


def _visible(im: Image.Image) -> int:
    """可见像素数（alpha > 0）—— 硬门：产出不许全透明。"""
    a = np.asarray(im.convert("RGBA"))[..., 3]
    return int((a > 0).sum())


def main() -> None:
    OUT.mkdir(exist_ok=True)
    # 先按各副 anm 的**第 1 帧**定下「要不要加 xOffset/yOffset」的约定（逐像素对既有资产）
    conv = {}
    firsts = {}
    for sub, entry, sprite, name, why in FIRST_FRAME:
        ent = AnmEntry(sub, entry)
        # 只核对该 entry 的 sprite 名是第一个 sprite（entry 里最小号那个）
        firsts[sub] = ent
        conv[sub] = pick_convention(sub, ent, sprite, OUT / name)

    report = []
    for sub, entry, sprite, name, why in WANT:
        ent = firsts[sub]
        piece = _crop(sub, ent, sprite, conv[sub])
        if piece is None:
            raise SystemExit(f"{name}：{sub} {entry} {sprite} 按约定（加偏移={conv[sub]}）越界")
        if _visible(piece) == 0:
            raise SystemExit(f"{name} 切出来**全透明** ⇒ 矩形/偏移错了：{sub} {entry} {sprite}")
        piece.save(OUT / f"{name}.png")
        md5 = hashlib.md5((OUT / f"{name}.png").read_bytes()).hexdigest()
        report.append((name, ent.rect(sprite, conv[sub]), _visible(piece), md5, why))

    # —— 既有第 1 帧的复核（不写文件）：同矩形 + 逐像素相同 ——
    check = []
    for sub, entry, sprite, name, why in FIRST_FRAME:
        ent = firsts[sub]
        piece = _crop(sub, ent, sprite, conv[sub])
        same = np.array_equal(np.asarray(piece), np.asarray(Image.open(OUT / name).convert("RGBA")))
        check.append((name, ent.rect(sprite, conv[sub]), _visible(piece), same, why))

    print(f"season opt anim: {len(report)} 帧 → {OUT.relative_to(HERE.parent)}（16×16，2 的幂，无补边）")
    for sub in sorted(conv):
        print(f"  偏移约定 {sub} entry1：{'加' if conv[sub] else '不加'} xOffset/yOffset（按既有第 1 帧逐像素定）")
    for name, rect, vis, md5, why in report:
        print(f"  {name:14s} {rect}  可见像素 {vis:4d}  md5 {md5}  # {why}")
    print("复核既有第 1 帧（未改写）：")
    for name, rect, vis, same, why in check:
        print(f"  {name:14s} {rect}  可见像素 {vis:4d}  逐像素相同 {same}  # {why}")


if __name__ == "__main__":
    main()
