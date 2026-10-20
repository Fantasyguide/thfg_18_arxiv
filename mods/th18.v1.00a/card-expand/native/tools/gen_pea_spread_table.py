#!/usr/bin/env python3
"""gen_pea_spread_table.py —— 生成 77「豌豆射手」电弹的**前方散射表**（owner 口径 ✓）。

## 为什么离线生成（本仓惯例 ✓）

owner 2026-10-15：「**植物大战僵尸里面的电大哥是向前方一个角度范围内随机散射子弹，
你目前做成了全向散射**」✓ ＋「**±20° 之间完全随机**」✓
⇒ 电弹的朝向必须是**正上方 ±20° 内的连续随机** ✓，而卡侧**禁 libm** ✗（本仓不变式：
`make dllx87` 要求我们的 object 里 0 条 x87 指令）⇒ 角度/方向**离线算成常量表** ✓，
运行时只做「`ce_rand() % N` 抽一项」 ✓。同先例：`youkai_core.c` 的 `yk_orbit_cos/sin`（120 项 ✓）。

## 用法

    python3 native/tools/gen_pea_spread_table.py            # 打印 C 表（默认 ±20° / 1° 步长）
    python3 native/tools/gen_pea_spread_table.py --half-deg 15 --step 0.5

表项语义（与 `cards/peashooter.c` 里的 `s_rand_spread[]` 逐字对应 ✓）：
    `vx/vy` = 单位方向（屏幕坐标 ✓，y 向上为负 ✓）
    `spread` = 贴图该转多少（贴图朝上 ⇒ 就是 θ 本身 ✓）
    `abs`    = 弹的朝向角（θ − π/2 ✓）
"""
import argparse
import math


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--half-deg", type=float, default=20.0, help="半角（度）；默认 20 ✓")
    ap.add_argument("--step", type=float, default=1.0, help="步长（度）；默认 1 ✓")
    a = ap.parse_args()
    n = int(round(2 * a.half_deg / a.step)) + 1
    print("/* 生成：python3 native/tools/gen_pea_spread_table.py --half-deg %g --step %g"
          "  ⇒ %d 项（±%g°）*/" % (a.half_deg, a.step, n, a.half_deg))
    print("static const struct { float vx, vy, spread, abs; } s_rand_spread[PS_SPREAD_N] = {")
    for i in range(n):
        deg = -a.half_deg + i * a.step
        r = math.radians(deg)
        print("    { %10.4ff, %10.4ff, %10.4ff, %10.4ff },   /* %+3d° */"
              % (math.sin(r), -math.cos(r), r, r - math.pi / 2, int(round(deg))))
    print("};")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
