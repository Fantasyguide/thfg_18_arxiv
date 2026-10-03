#!/usr/bin/env python3
"""109 的「灵」（灵魂）外观贴图 —— **从 TH09 自己的原版幽霊图集 1:1 切**。

    python3 make_pofv109_spirit_art.py

产物：`ability/pofv109_spirit/SPIRIT_0..3.png`（各 **64×64**）

## 为什么是这几格（一手）

owner 2026-10-01 实跑反馈：「**灵魂没有任何贴图**」—— 根因是原来那版是**程序化图元占位**，
且半径写成 `1.0`（`drawPoly(1.0f, 32)`）+ 卡侧 `scale = 1.0` ⇒ **视觉半径 1 px = 看不见**。
⇒ 本次换成**真贴图**：TH09 的**幽霊**（PoFV 里灵/幽灵那一种，白红天使样带翼）。

一手链（全部在 TH09 自己的样本里）：

| 步 | 出处 |
| --- | --- |
| ① TH09 的敌人图集 = `data/stgenm/enemy.png`（512×512）| `th09.dat` 的 `enemy.anm` 的 `entry0 { name: "data/stgenm/enemy.png" }` |
| ② ★ 幽霊那 12 帧 = **sprite 84–95**（各 64×64，位于 `x∈{192,256,320,384}` × `y∈{320,384,448}`）| 同一份 `enemy.anm` 的 sprite 表 |
| ③ 12 帧是**同一条动画**（天使样的白红幽霊，逐帧换翼/手的位置）| 目视核对（裁图放大看） |

⇒ 本脚本取**前 4 帧（sprite 84–87）**做一段循环（`assets/ability/scripts/372_spirit109.anm.txt`）。

## 纪律

- **1:1 切、不缩放、不重绘**（`METHOD.md`：贴图走原生切图，别自己画）。
- 原版字节只在 `local/`（gitignored）⇒ 本脚本**在构建时用 `thanm -x9` 现取**（自包含：
  不依赖 `tooling/thtk/unpack.py` 是否收录了 th09 —— 那个工具目前只列了 th07/08/10–13/15–18）。
- 图集尺寸/格位变了就**直接报错**（不静默出错的图）。
"""
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

from PIL import Image

HERE = Path(__file__).resolve().parent          # assets/ability/
OUT = HERE / "pofv109_spirit"
REPO = HERE.parents[4]                           # 仓库根

#: TH09 v1.50a 的敌人 anm（原始样本，gitignored）与 `thanm`
SRC_ANM = REPO / "local/th09.v1.50a/dat/enemy.anm"
THANM = REPO / "local/vendor/thtk/build/thanm/thanm"
ANMMAP = REPO / "local/vendor/eclmap/anmmap/v2.anmm"   # TH09 的 anm 头版本 = 3 ⇒ v2.anmm

#: 一手：幽霊 12 帧 = sprite 84–95，网格 4 列 × 3 行，起点 (192, 320)，格 64×64。
GHOST_X0, GHOST_Y0, GHOST_CELL = 192, 320, 64
GHOST_FRAMES = 4                                 # 取前 4 帧做循环

#: 期望尺寸（TH09 enemy.png）
EXPECT_SIZE = (512, 512)


def extract_atlas(tmp: Path) -> Path:
    """用 `thanm -x9` 把 `enemy.anm` 里的 png 解到临时目录，返回 png 路径。

    ⚠ `thanm -x` 的输出路径来自 anm 里的 entry name（这里是 `data/stgenm/enemy.png`），
      **相对于 cwd** ⇒ 必须在临时目录里跑（别污染仓库）。"""
    if not SRC_ANM.is_file():
        raise SystemExit(f"缺 TH09 原始样本 {SRC_ANM}")
    if not THANM.is_file():
        raise SystemExit(f"缺 thanm（先编译 tooling/thtk）：{THANM}")
    cmd = [str(THANM), "-x9", str(SRC_ANM)]
    if ANMMAP.is_file():
        cmd += ["-m", str(ANMMAP)]
    r = subprocess.run(cmd, cwd=tmp, capture_output=True, text=True)
    if r.returncode != 0:
        raise SystemExit(f"thanm 解包失败：{r.stderr.strip() or r.stdout.strip()}")
    png = tmp / "data/stgenm/enemy.png"
    if not png.is_file():
        cand = list(tmp.rglob("enemy.png"))
        if not cand:
            raise SystemExit(f"thanm 解包后找不到 enemy.png（tmp={tmp}）")
        png = cand[0]
    return png


def main():
    tmp = Path(tempfile.mkdtemp(prefix="pofv109_spirit_"))
    try:
        png = extract_atlas(tmp)
        atlas = Image.open(png).convert("RGBA")
        if atlas.size != EXPECT_SIZE:
            raise SystemExit(f"TH09 敌人图集尺寸变了：{atlas.size}（期望 {EXPECT_SIZE}；sprite 表随之要重核）")
        OUT.mkdir(exist_ok=True)
        for k in range(GHOST_FRAMES):
            x = GHOST_X0 + k * GHOST_CELL          # 84,85,86,87 = 第一行四格
            y = GHOST_Y0
            cell = atlas.crop((x, y, x + GHOST_CELL, y + GHOST_CELL))
            cell.save(OUT / f"SPIRIT_{k}.png")
            print(f"  SPIRIT_{k}.png  {GHOST_CELL}x{GHOST_CELL}  ← enemy.png ({x},{y})  [sprite {84 + k}]")
        print(f"→ {OUT}")
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


if __name__ == "__main__":
    sys.exit(main())
