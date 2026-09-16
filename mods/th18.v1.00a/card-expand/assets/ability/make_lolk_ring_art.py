#!/usr/bin/env python3
"""绀珠传（id 118）的擦弹圈：从 TH15 的原版贴图里抠出那个环。

来源（原生字节只在 local/，不入库；构建时取）：
  `local/th15.v1.00b/anm/effect/effect/eff_sloweffect.png`（128×64）
  它在 TH15 的 `effect.anm` entry3 里是两个 64×64 sprite：
    sprite23 = 左半（实心小爆点）、sprite24 = **右半（空心环，半径 ~22 px）**。
  用它的脚本是 `script27`（layer 6 / blendMode 2 / 0.4 秒淡入 / 4.3→2.0 缩放），
  由 `Player::on_tick` 里的 `th15:0x4540e0` 起（条件 = 场上有敌 && 自机动画帧 ≥4 && 低速位）。

面板（波次 UI）同样来自 TH15：`front.anm` 的 `front/front02.png`（192×256，每条 **192×32**），
`script241` 用五条拼出一块面板 —— 244 sprite145@(0,0) / 245 sprite146@(-64,50) /
246 sprite147@(-64,90) / 247 sprite149@(-64,130) / 248 sprite148@(-64,170)，
碎片标签 242 sprite150 / 243 sprite151 同在 (0,240)。
★ 碎片标签的**颜色**坐实了谁是谁（2026-09-09 实测像素）：
**sprite150(y=160) 是绿色 `(34,255,85)` = 符卡碎片条；sprite151(y=192) 是粉色 `(255,119,221)` = 残机碎片条**
（`engine/chapter/th15/01` 早前把这两条写反了，已订正）。
每条存 **256×32**：把 TH15 的 192×32 内容**居中**放（左右各留 32 px 透明）——sprite 是**中心锚**，
内容偏左会整条推左 32 px（2026-09-09 实机就是「偏左」这个症状）。
★ 2026-09-13：**原来这里的理由「构建期必须是 2 的幂」已经作废**（那条门取消了）；
  现在 256×32 是 `ATLAS_LOLK` 里定稿的成员矩形（`ATLAS-MERGE-AUDIT.md` §4.3），
  要改成 192×32 得同时改 ORDER 的矩形 + 实跑确认锚点观感（内容居中 ⇒ 可见像素不动）。

产物：`ability/lolk/RING.png` —— 右半 64×64 用 LANCZOS 放大到 **128×128**
（环半径随之变 ~44 px，正好接近 TH18 圆形擦弹判定的有效半径 ≈ 43 px，脚本 scale 1.0 即可）。

⚠️ 贴图放大 2× ⇒ **脚本里的 scale 要取 TH15 原值的一半**：TH15 script27 是 4.3 → 2.0
（它的贴图是 64×64、亮环半径 ~22 px，落定 44 px）；照抄到 128×128 的贴图上会画成 88 px。
本卡用 **2.15 → 1.0**，落定半径 = 44 px = 判定圈。
"""
from pathlib import Path
from PIL import Image

HERE = Path(__file__).resolve().parent
REPO = HERE.parents[4]
SRC = REPO / "local/th15.v1.00b/anm/effect/effect/eff_sloweffect.png"
OUT = HERE / "lolk"


def main():
    if not SRC.is_file():
        raise SystemExit(f"缺原版贴图 {SRC}\n（先 python3 tooling/thtk/unpack.py th15.v1.00b）")
    im = Image.open(SRC).convert("RGBA")
    if im.size != (128, 64):
        raise SystemExit(f"{SRC} 是 {im.size}，预期 (128, 64)")
    ring = im.crop((64, 0, 128, 64)).resize((128, 128), Image.LANCZOS)
    OUT.mkdir(exist_ok=True)
    ring.save(OUT / "RING.png")

    # 面板：TH15 front.anm script241 的五条 —— **切开各存一张**（TH15 也是五个独立 VM）
    p_src = REPO / "local/th15.v1.00b/anm/front/front/front02.png"
    if not p_src.is_file():
        raise SystemExit(f"缺原生贴图 {p_src}")
    src = Image.open(p_src).convert("RGBA")
    # PANEL_1..5 = 五条标签；PANEL_6 = 绿色（符卡碎片）、PANEL_7 = 粉色（残机碎片）
    rows = [(145, 0), (146, 32), (147, 64), (149, 128), (148, 96), (150, 160), (151, 192)]
    for i, (idx, y0) in enumerate(rows, 1):
        strip = Image.new("RGBA", (256, 32), (0, 0, 0, 0))
        tile = src.crop((0, y0, 192, y0 + 32))
        strip.paste(tile, ((256 - 192) // 2, 0), tile)   # 居中：中心锚下才与 TH15 同位
        strip.save(OUT / f"PANEL_{i}.png")
    print(f"lolk: RING.png 128x128 + PANEL_1..7.png 256x32（内容居中 192）-> {OUT.relative_to(HERE.parent)}")


if __name__ == "__main__":
    main()
