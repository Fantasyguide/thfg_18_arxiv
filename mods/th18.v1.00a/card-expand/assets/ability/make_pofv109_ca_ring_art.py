#!/usr/bin/env python3
"""109 的**蓄力消弹圈（红圈）**贴图 —— **把 TH09 原版环纹理「卷」成环**（方案 A）。

    python3 make_pofv109_ca_ring_art.py

产物：`ability/pofv109_storm/CA_RING_WRAP.png`（**256×256** 的环）

## 为什么这么做（一手的机制）

TH09 的蓄力消弹圈 = `etama.anm` **script 91/92/93**（三档一条律）：

```asm
sprite(sprite224);            ; ★ 纹理 = `etama3.png` 里的**竖带**
blendAdditiveSet(1);          ; 加算
pos(16.0f, 0, 0); rotate(0, 0, %RANDRAD);       ; 随机自转
posTime(48/64/128, 0, 192/256/448, 0, 0);       ; ★ 从 16 扩到 192/256/448
```

一手（`etama.anm` 的 sprite 表，entry = `data/etama/etama3.png`）：

| sprite | 矩形 | 说明 |
| --- | --- | --- |
| 222 / **224** / 223 / 225 / 226 | **16 × 768** | 五条**环纹理**（224 = x **32** ✓ 本卡取它）|
| 227 | 16 × 128 | 单条（未平铺）|
| 229 | 256×256 | 图集里的大图 |

★ **关键**：图集本体只有 **128 高**、而 sprite 声明 **768 高** ⇒ **768 = 6 × 128** ✓
  —— 即引擎把 **16×128** 的带子**沿周向平铺 6 圈**贴到环上 ✓（owner：「贴图被**贴到圆环上**」✓✓）。
  ⇒ 所以**摊开看图集怎么看都不像** ✓（环是运行时卷出来的 ✓）。

## 本脚本做的事

把 `sprite224`（x=32 起、**16 宽 × 128 高**的原带）按**同一映射**重投影到环形：
- **径向（环的厚度）** ← 带子的 **x**（16 px ✓）；
- **周向（绕一圈）** ← 带子的 **y**，并按一手的 **6 圈平铺**（768/128 ✓）取样 ✓。

⇒ 纹理**完全是一手的**（只是把运行时卷的一次性做掉 ✓），不是自己画 ✗。

## 纪律

- 1:1 取样（最近邻 ✓）、不缩放原带；
- 原版字节只在 `local/` ⇒ 构建时用 `thanm -x9` 现取 ✓；
- 尺寸不符就报错。
"""
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

from PIL import Image

HERE = Path(__file__).resolve().parent
OUT = HERE / "pofv109_storm"
REPO = HERE.parents[4]

SRC_ANM = REPO / "local/th09.v1.50a/dat/etama.anm"
THANM = REPO / "local/vendor/thtk/build/thanm/thanm"
ANMMAP = REPO / "local/vendor/thpages/static/mapfile/v2.anmm"

STRIP_X = 32          # sprite224 的 x（一手 ✓）
STRIP_W = 16          # 环的厚度（径向 ✓）
STRIP_H = 128         # 图集高度；一手 sprite 声明 768 = 6 × 128 ⇒ **周向平铺 6 圈** ✓
TILES = 6
OUT_SIZE = 256        # 卷出来的环贴图边长（❓ 自定：够大且是 2 的幂）
R_OUT = 120           # 环外半径（px）❓ 自定
#: ★★ 2026-10-01 owner 实跑：「**从左到右第三条**，被以**加模式**画上去了，但是它**被横向拉得还挺宽的**」
#:   ⇒ ① 第三条 = `x = 32` ✓（本脚本 STRIP_X 就是 32 ✓）；② 加算 ✓（脚本 407 已 `blendMode(1)` ✓）；
#:     ③ **横向（= 环的径向厚度）被拉宽** ✓ ⇒ 带子的 16 px 宽按 `THICK_MUL` 倍拉伸 ✓。
#:   ❓ 倍数是自定的（一手那个「卷环」回调 `0x40dc70` 的径向映射没读出来）⇒ 3× 先试，owner 再调。
THICK_MUL = 3
SCALE_NOTE = ""


def main():
    if not SRC_ANM.is_file():
        raise SystemExit(f"缺 TH09 原始样本 {SRC_ANM}")
    tmp = Path(tempfile.mkdtemp(prefix="pofv109_caring_"))
    try:
        r = subprocess.run([str(THANM), "-x9", str(SRC_ANM), "-m", str(ANMMAP)],
                           cwd=tmp, capture_output=True, text=True)
        png = tmp / "data/etama/etama3.png"
        if not png.is_file():
            cand = list(tmp.rglob("etama3.png"))
            if not cand:
                raise SystemExit(f"解包失败：{r.stderr.strip() or r.stdout.strip()}")
            png = cand[0]
        atlas = Image.open(png).convert("RGBA")
        strip = atlas.crop((STRIP_X, 0, STRIP_X + STRIP_W, STRIP_H))
        if strip.size != (STRIP_W, STRIP_H):
            raise SystemExit(f"带子尺寸不对：{strip.size}")

        n = OUT_SIZE
        out = Image.new("RGBA", (n, n), (0, 0, 0, 0))
        px = out.load()
        sp = strip.load()
        c = (n - 1) / 2.0
        thick = STRIP_W * THICK_MUL                # 环的厚度（= 带子宽 × THICK_MUL ✓）
        r_in = R_OUT - thick
        for y in range(n):
            for x in range(n):
                dx = x - c
                dy = y - c
                rad = (dx * dx + dy * dy) ** 0.5
                if rad < r_in or rad >= R_OUT:
                    continue
                # 径向 → 带子的 x（0..15 ✓）
                u = int((rad - r_in) / float(THICK_MUL))   # 径向拉伸 THICK_MUL 倍 ✓
                if u < 0: u = 0
                if u >= STRIP_W: u = STRIP_W - 1
                # 周向 → 带子的 y（按一手的 6 圈平铺 ✓）
                ang = __import__("math").atan2(dy, dx)
                t = (ang + __import__("math").pi) / (2 * __import__("math").pi)   # 0..1
                v = int(t * TILES * STRIP_H) % STRIP_H
                px[x, y] = sp[u, v]
        OUT.mkdir(exist_ok=True)
        out.save(OUT / "CA_RING_WRAP.png")
        print(f"  CA_RING_WRAP.png  {n}x{n}  ← etama3.png 的 x={STRIP_X} 带（{STRIP_W}x{STRIP_H}，"
              f"周向平铺 {TILES} 圈）")
        print(f"→ {OUT}")
    finally:
        shutil.rmtree(tmp, ignore_errors=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())
