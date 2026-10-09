#!/usr/bin/env python3
"""卡 109 **各档 C 的消弹圈**（扩张的**红色圆环**）—— 照 owner 描述烤的。
    python3 make_pofv109_c_ring_art.py     # 出图 + 打印要抄进 entries/ORDER.txt 的那一行
★★★ 必须写明的两句（别让人以为切了原图）
────────────────────────────────────────
1. **一手没有这张贴图。** owner 2026-10-10 实跑订正：「**C1 C2 包括其他 C 的消弹圈是扩张的
   红色圆环**」—— ★ 这是**观感**描述；一手 TH19 的消弹（`Ecl_EtBreakLv2` / `EXReigekiInf`）
   用的是**程序化圆**与逐命中点的火花，★ 具体那张"红环"贴图**未取**（见
   `HANDOFF-109-C1-C4.md` §5 / §12 的记录）。⇒ 本脚本**照描述烤**。
2. 为什么这么做：本仓 TH18 的 anmm **止于 613**、没有 `ins_617`；`drawPoly` 只吃一路半径 ⇒
   画不出"红环"；而 `texCircle`(600) 需要配一条**绕圆的长条贴图**（§9.2）。⇒ **直接烤一张圆环图**
   （与 `make_pofv109_chg_mark_art.py` / `make_pofv109_barrier_art.py` 同款：照描述烤、幂等）。
★ 尺寸 128×128；环带落在 r≈0.86、带宽 ≈0.10（⇒ 卡侧把它 `scale` 到 8/12/16/20 … 192 都清晰 ✓）。
"""
from pathlib import Path
from PIL import Image

HERE = Path(__file__).resolve().parent
OUT = HERE / "cring"
SIZE = 128
R_MID, R_HALF = 0.86, 0.055        # 环心半径 / 半带宽（归一化）


def main() -> int:
    OUT.mkdir(parents=True, exist_ok=True)
    im = Image.new("RGBA", (SIZE, SIZE), (0, 0, 0, 0))
    px = im.load()
    c = (SIZE - 1) / 2.0
    for y in range(SIZE):
        for x in range(SIZE):
            dx, dy = (x - c) / c, (y - c) / c
            r = (dx * dx + dy * dy) ** 0.5
            d = abs(r - R_MID)
            a = 0.0 if d >= R_HALF else (1.0 - d / R_HALF) ** 2   # 柔边的环
            if a > 0.0:
                px[x, y] = (255, 60, 60, int(round(a * 235)))     # ★ 红色 ✓
    fn = "C_RING.png"
    im.save(OUT / fn)
    print(f"{'POFV109_C_RING':<24} ability/cring/{fn:<24} {SIZE}x{SIZE}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
