#!/usr/bin/env python3
"""109 的「灵」（灵魂）外观贴图 —— **从 TH19 自己的原版動物霊图集 1:1 切**。

    python3 make_pofv109_spirit_art.py

产物：`ability/pofv109_spirit/SPIRIT_0..3.png`（各 **64×64**）

## 为什么是这几格（一手）

owner 实跑反馈两轮：
1. 「**灵魂没有任何贴图**」—— 根因是原来那版是**程序化图元占位**，且半径写成 `1.0`
   （`drawPoly(1.0f, 32)`）+ 卡侧 `scale = 1.0` ⇒ **视觉半径 1 px = 看不见**。
2. ★ 「**怎么上来有个莉莉白在靠近，灵不长这样**」—— 我第一次换成 TH09 `enemy.anm` 的
   sprite 84–87，**认错了图**：那 12 帧是**大妖精样的敌人**（看着像莉莉白），**不是灵** ✗。

⇒ 本次改用 **TH19 的 `動物霊`**（獣王園的動物霊；本卡 109 就是「花映塚 × 獣王園」，
   灵这个概念本身来自 TH19）✓。

一手链（全部在 TH19 自己的样本里）：

| 步 | 出处 |
| --- | --- |
| ① TH19 的敵人图集之一 = `enemy/animal_spirits.png`（256×256，6 种動物 × 4 帧）| `th19.dat` 的 `enemy.anm` 的 **entry7** |
| ② ★ 一種動物 = **4 帧 48×32**；第 1 行（红） = **sprite 344–347**（`x = 0/48/96/144`、`y = 0`）| 同一份 `enemy.anm` 的 sprite 表 |
| ③ 依据：TH19 的動物霊是**真的机制**（`engine/card/th19/01-ability-cards.md` 的 `@POWERUP` = 「対動物霊攻撃力UP」）| 仓库文档 |

⇒ 本脚本取**第 1 行那 4 帧（sprite 344–347）**做一段循环（`assets/ability/scripts/372_spirit109.anm.txt`）。

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

#: TH19 v1.10c 的敵人 anm（先由 `thdat -x19` 从 .dat 解出）与 `thanm`
SRC_ANM = REPO / "local/th19.v1.10c/dat/enemy.anm"
THANM = REPO / "local/vendor/thtk/build/thanm/thanm"
ANMMAP = REPO / "local/vendor/eclmap/anmmap/v8.anmm"   # TH19 的 anm 头版本 = 8 ⇒ v8.anmm
THDAT = REPO / "local/vendor/thtk/build/thdat/thdat"
SRC_DAT = REPO / "local/th19.v1.10c/th19.dat"

#: 一手：動物霊**一種 = 一行 4 帧 48×32**；第 1 行从 sprite **344** 起（x 步 48、y 步 32）。
#: ★★ 2026-10-01 owner：「**五种灵都做了吗**」⇒ 没有（此前只切了第 1 行）✗ ⇒ 现在切 **5 行**：
#:   TH19 的 5 种 = 敵人 kind **2..6**（仓库 `engine/versus/th19/04-manual-spec.md` 第 244 行：
#:   「kind 2/3/4 → ghost kind 1；5 → kind 3、6 → kind 4」⇒ 共 5 档 ✓）。
GHOST_X0, GHOST_Y0, GHOST_W, GHOST_H, GHOST_DX = 0, 0, 48, 32, 48
GHOST_FRAMES = 4                                 # 每行 4 帧
GHOST_ROWS = 5                                   # ★ 5 种（kind 0..4）

#: 期望尺寸（TH19 animal_spirits.png）
EXPECT_SIZE = (256, 256)


def extract_atlas(tmp: Path) -> Path:
    """用 `thanm -x9` 把 `enemy.anm` 里的 png 解到临时目录，返回 png 路径。

    ⚠ `thanm -x` 的输出路径来自 anm 里的 entry name（这里是 `data/stgenm/enemy.png`），
      **相对于 cwd** ⇒ 必须在临时目录里跑（别污染仓库）。"""
    if not SRC_ANM.is_file():
        # 解包树里没有就先从 .dat 解出来（`thdat -x19 <dat> enemy.anm -C <dir>`）
        if not SRC_DAT.is_file():
            raise SystemExit(f"缺 TH19 原始样本 {SRC_DAT}")
        SRC_ANM.parent.mkdir(parents=True, exist_ok=True)
        r = subprocess.run([str(THDAT), "-x19", str(SRC_DAT), "enemy.anm", "-C", str(SRC_ANM.parent)],
                           capture_output=True, text=True)
        if not SRC_ANM.is_file():
            raise SystemExit(f"thdat 解 enemy.anm 失败：{r.stderr.strip() or r.stdout.strip()}")
    if not THANM.is_file():
        raise SystemExit(f"缺 thanm（先编译 tooling/thtk）：{THANM}")
    cmd = [str(THANM), "-x19", str(SRC_ANM)]
    if ANMMAP.is_file():
        cmd += ["-m", str(ANMMAP)]
    r = subprocess.run(cmd, cwd=tmp, capture_output=True, text=True)
    if r.returncode != 0:
        raise SystemExit(f"thanm 解包失败：{r.stderr.strip() or r.stdout.strip()}")
    png = tmp / "enemy/animal_spirits.png"
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
        for r in range(GHOST_ROWS):
            for k in range(GHOST_FRAMES):
                x = GHOST_X0 + k * GHOST_DX
                y = GHOST_Y0 + r * GHOST_H
                cell = atlas.crop((x, y, x + GHOST_W, y + GHOST_H))
                cell.save(OUT / f"SPIRIT{r}_{k}.png")
                print(f"  SPIRIT{r}_{k}.png  {GHOST_W}x{GHOST_H}  ← animal_spirits.png ({x},{y})"
                      f"  [sprite {344 + r * 4 + k}]")
        print(f"→ {OUT}")
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


if __name__ == "__main__":
    sys.exit(main())
