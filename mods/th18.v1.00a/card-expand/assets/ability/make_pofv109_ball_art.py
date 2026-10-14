#!/usr/bin/env python3
"""卡 109 **C3/C4 的阴阳玉**（+ C4 第二发射口的 8×8 彩弹）—— **一手原生切图** ✓。
    python3 make_pofv109_ball_art.py    # 出图 + 打印要抄进 entries/ORDER.txt 的行

★★★★ 一手出处（切图，不是烤的 ✓；★ 全部由独立复核 agent `a84cf536` 逐条确证 ✓）
────────────────────────────────────────
★ 机制（★ 这一步是我先前漏掉的整层 ✗）：★ `sprite(N)` 的 N 是**全文件扁平号** ✓，
★ ★ **但弹体 VM（`vm+0x610 == 1`）会先经 `FUN_004f66a0(vm, N)` 换算** ✓ ——
★ 它按 `(type, color)` 返回 `table[type] + 4 + color*0x14 + N*4` ✓
★ ⇒ ★ **N 只是"这张 type 的第几号变体"** ✓（**0 = 实体图 / 1 = 淡版图 / 3 = 消弹脚本** ✓）。

| 用途 | 一手 type/color | 表中变体 | 真正 sprite | 源文件 | rect (PIL) |
| --- | --- | --- | --- | --- | --- |
| ★ C3 阴阳玉（唯一帧 ✓）| `type 46/47, color 0` | `sprite(0)` → `record[0][0]` | **596** | `bullet6.png` | **(1, 33, 63, 95)** = 62×62 大红玉 |
| ★ C4 出生（淡版 ✓）| `type 44, color 3` | `sprite(1)` → `record[3][1]` | **326** | `bullet1.png` | **(97, 209, 127, 239)** = 30×30 半透明蓝玉 |
| ★ C4 稳定（实心 ✓）| `type 44, color 3` | `sprite(0)` → `record[3][0]` | **583** | `bullet6.png` | **(97, 1, 127, 31)** = 30×30 实心蓝玉 |
| ★ C4 第二发射口（8×8 彩弹 ✓）| `type 1, color 2` | `sprite(1)/(0)/(2)` | 324/2/… | `bullet1.png` | **(8,240)/(0,240)/(16,240)** 各 8×8 |

★ 源 = `local/th19.v1.10c/dat/bullet/{bullet1,bullet6}.png` ✓（★ TH18 也有同名文件 ✓，★ 但这两张**只按 TH19 切** ✓
—— ★ `bullet6.png` 的 TH18/TH19 是否同像素**本轮没核** ❓ ⇒ ★ 先按一手的那份 ✓）。
★ 每色变体的含义：★ `[0] 实体图 / [1] 淡版图 / [2] 恒 0 / [3] 消弹脚本 / [4] 色` ✓。
★ ⚠ **消失不换第三张图** ✗ —— ★ 一手的消失是 `alphaTime + scaleTime` 淡出缩掉 ✓。
"""
import pathlib
from PIL import Image

ROOT = pathlib.Path(__file__).resolve().parent
_r = ROOT
for _ in range(8):
    if (_r / "local").is_dir():
        break
    _r = _r.parent
SRC1 = _r / "local/th19.v1.10c/dat/bullet/bullet1.png"
SRC6 = _r / "local/th19.v1.10c/dat/bullet/bullet6.png"
OUTDIR = ROOT / "bball"

# ★ (名字, 文件名, 源, rect) —— 全部一手 ✓
FRAMES = [
    ("POFV109_C3_ORB",       "C3_ORB.png",        SRC6, (1, 33, 63, 95)),      # sprite596：62×62 大红玉
    ("POFV109_C4_ORB_FADE",  "C4_ORB_FADE.png",   SRC1, (97, 209, 127, 239)),  # sprite326：淡版蓝玉
    ("POFV109_C4_ORB_SOLID", "C4_ORB_SOLID.png",  SRC6, (97, 1, 127, 31)),     # sprite583：实心蓝玉
    ("POFV109_BBALL_BORN",   "BBALL_BORN.png",    SRC1, (8, 240, 16, 248)),    # 8×8 彩弹：出生
    ("POFV109_BBALL_IDLE",   "BBALL_IDLE.png",    SRC1, (0, 240, 8, 248)),     # 8×8 彩弹：稳定
    ("POFV109_BBALL_DIE",    "BBALL_DIE.png",     SRC1, (16, 240, 24, 248)),   # 8×8 彩弹：消失
]

# ★★★★★ 2026-10-13（★ 本 goal 待办 1 ✓）：★ **纯灵本体那 8 帧** ✗ ——
#   ★ 一手（★ 复核 `ab9b54e5` ✓ 高置信 ✓）：★ 标准纯灵 = ★ `GhostPure()` → ★ `ins_303(0,106)` → ★ **script106**
#   ★ → ★ **`enemy.anm` entry1 `enemy2.png` 的 sprite 144–151（黄、8 帧、32×32、y=96）** ✓；
#   ★ 固化态 = ★ `GhostPureDisable()` → ★ **同一支 script106** ✓ ⇒ ★「★ 固化不换族、颜色也一样」✓。
#   ★ ★ **为什么必须补** ✗：★ 本卡当年只切了 **144–148 这 5 帧** ✗（★ `ORDER.txt:1168-1172` ✓）
#   ★ ⇒ ★ **"环"的那三帧 149/150/151 从来没切进来** ✗ ⇒ ★ 这就是 owner 报的
#   ★ 「★ **固化态出生时外面那圈蓝环没了**」✗ ✓（★ 一手那个循环是 ★ **球 → … → 空心环** ✓）。
#   ★ 矩形 = ★ **x = 0/32/64/96/128/160/192/224** ✓、★ y = 96 ✓、★ 32×32 ✓（★ 步长 32 ✓）。
# ★ 2026-10-13 订正路径 ✗：`dat/enemy/enemy2.png` **不存在** ❓（`dat/` 要 `thdat -x` 解包才有）
#   ⇒ 用仓库里**已经解出来的**那张 ✓（`glob **/enemy2.png` 查到的 TH19 那份 ✓）。
SRC_SPIRIT = _r / "local/th19.v1.10c/anm/enemy/enemy/enemy2.png"
SPIRIT_DIR = ROOT / "spirit"
SPIRIT_FRAMES = [
    (f"SPIRIT_DISP4_{i}", f"SPIRIT_DISP4_{i}.png", (0 + 32 * i, 96, 32 + 32 * i, 128))
    for i in range(8)
]


def main() -> int:
    OUTDIR.mkdir(parents=True, exist_ok=True)
    for name, fn, src, rect in FRAMES:
        im = Image.open(src).convert("RGBA")
        cut = im.crop(rect)
        cut.save(OUTDIR / fn)
        print(f"{name:<24} ability/bball/{fn} {cut.width}x{cut.height}")
    SPIRIT_DIR.mkdir(parents=True, exist_ok=True)
    for name, fn, rect in SPIRIT_FRAMES:
        im = Image.open(SRC_SPIRIT).convert("RGBA")
        cut = im.crop(rect)
        cut.save(SPIRIT_DIR / fn)
        print(f"{name:<24} ability/spirit/{fn} {cut.width}x{cut.height}")
    print("# —— 以上各行追加/替换到 ability/entries/ORDER.txt ——")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
