#!/usr/bin/env python3
"""生成 110（风神录）灵击的**柱环 + 火星**脚本 —— 一手 `bullet.anm` **script400/407/441 三套**逐条移植。

    python3 make_mof_bomb_ring_scripts.py            # 写脚本 + 打印台帐
    python3 make_mof_bomb_ring_scripts.py --check    # 只对账（不写盘），不一致就退出 1

■ 为什么要有这个生成器（而不是手抄 16 份）
  一手 TH10 的灵击**按机体/符卡分三套**，三套的**主脚本**（`Bomb::start` 选哪一支，
  本仓 `mof_core.h` 的 `MOF_BOMB_R_*` 一节：灵梦 160 / 魔理沙 112 / 符卡中 64）各自
  `scriptNew` 出**自己那一套子脚本**，子脚本只差几个数（成长帧数 / 目标半径 / 淡出时点 / 火星基准半径）：

  | 变体 | 主脚本 | 白环（∓18°/帧）| 紫环（±6°/帧）| 火星基准半径 | 火星 |
  | --- | --- | --- | --- | --- | --- |
  | 0 灵梦 | 400 | 394 (60,6,32,**160**) / 395 (60,5,16,**160**) | 396/397 (60,4,16,**200**) | **160** | 398(红外)/399(蓝内) |
  | 1 魔理沙 | 407 | 401 (40,6,32,**112**) / 402 (40,5,16,**112**) | 403/404 (40,4,16,**140**) | **112** | 405/406 |
  | 2 符卡中 | 441 | 435 (60,6,32,**64**)  / 436 (60,5,16,**64**)  | 437/438 (60,4,16,**80**)  | **64**  | 439/440 |

  ⚠ 2026-09-24 的教训（owner 实跑「灵击依旧很奇怪」）：我最初**只移植了灵梦那一套（394–397）给全部机体**，
  于是魔理沙（`rise=1.30/f`、半径 112）看到的环/火星全部**按 160/200 px 画** ⇒ 大小与节奏都不对。
  本文件把三套一次性补全，卡侧按 `spell_now`/`reimu` 选变体（与半径曲线同一个判据）。

■ 与一手的两处必改 / 一处换算（都写在每个脚本的文件头里，别只看这里）
  ① `sprite(sprite362)` → `@MOF_BOMB_RING_BENT@`（构建期把 14×128 竖条极坐标弯环、沿圆周铺 32 份
     = 一手 `texCircle(32)`）；`iset($I0,32)/iset($I1,3)/texCircle($I0)` 三行删掉
     （TH18 的 anm VM 是否执行 `texCircle` 没有实跑先例，实跑症状 = 竖条被放大成一条大光晕）。
  ② `scaleTime` 的**第 3/4 参是目标值**（不是起点/终点）—— 铁证 = 本仓 `engine/card/th18/
     13-mallet-clear-and-th17-charge.md` 记的 TH18 原生 script28（`scale(0,2.8)+scaleTime(20,4,1,1)`
     = 「y 从 2.8 拉到 1.0」）。⇒ 一手那两个不等比的数（白环 32/160、紫环 16/200）是**给竖条 + texCircle**
     的；我们的贴图已经是整圈 ⇒ **两轴同值** = `目标半径 / R0`。
  ③ **换算**：烘出来的环基准半径 `R0 = 200 px`（见 `make_mof_bomb_ring_bent.py` 的 512×512 版）
     ⇒ 脚本里 `scale = 半径 / 200`。起手 `scale(16,16)`（一手）⇒ `0.08` = 半径 16 px。
  ④ 偏离：`scaleTime` 的 mode 一手写 6/5/4，我们**统一 4**（mode 6/5 在 TH18 版 anm 里没有一手；
     4 = 本仓在用的线性档，先例 116 季解 / TH16 季解）。

■ 卡侧契约（`bomb10.c`）
  · 柱环：按排期表起，**只写位置**（`entity_pos`），scale/alpha/自删全交给脚本；
  · 火星：出生时写一次位置（视觉锚点），脚本自己按 `%RANDRAD`/`%RANDF` 飞；
  · 脚本号在 `native/anm_ids.h` 里按名字取（`CE_ANM_ABILITY_SCRIPT_MOF_BOMB_RING*` / `_SPARK*`）。
"""
import argparse
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
SCRIPTS = HERE / "scripts"
R0 = 200.0          # 环贴图的基准半径（px）= 脚本里 scale 1.0 的可见半径
FRAME_START = 16.0  # 一手 `scale(16,16)`

# ---- 三套变体（顺序 = 变体下标；编号 = 一手子脚本号，只为注释里对得上）----
RING = [
    # (变体名, 一手主脚本, 本仓脚本号, 一手子脚本号, pad, 旋转 rad/帧, 目标半径 px, 成长帧, 淡出帧, 紫?)
    ("灵梦",   400, 327, 394, "mof_bomb_ring394", -0.31415927, 160.0, 60, 120, False),
    ("灵梦",   400, 328, 395, "mof_bomb_ring395",  0.31415927, 160.0, 60, 120, False),
    ("灵梦",   400, 329, 396, "mof_bomb_ring396",  0.10471976, 200.0, 60,  40, True),
    ("灵梦",   400, 330, 397, "mof_bomb_ring397", -0.10471976, 200.0, 60,  40, True),
    ("魔理沙", 407, 333, 401, "mof_bomb_ring401", -0.31415927, 112.0, 40, 160, False),
    ("魔理沙", 407, 334, 402, "mof_bomb_ring402",  0.31415927, 112.0, 40, 160, False),
    ("魔理沙", 407, 335, 403, "mof_bomb_ring403",  0.10471976, 140.0, 40,  40, True),
    ("魔理沙", 407, 336, 404, "mof_bomb_ring404", -0.10471976, 140.0, 40,  40, True),
    ("符卡中", 441, 337, 435, "mof_bomb_ring435", -0.31415927,  64.0, 60, 120, False),
    ("符卡中", 441, 338, 436, "mof_bomb_ring436",  0.31415927,  64.0, 60, 120, False),
    ("符卡中", 441, 339, 437, "mof_bomb_ring437",  0.10471976,  80.0, 60,  40, True),
    ("符卡中", 441, 340, 438, "mof_bomb_ring438", -0.10471976,  80.0, 60,  40, True),
]

# 火星（枫叶）：一手 398/399（灵梦 160）、405/406（魔理沙 112）、439/440（符卡中 64）
#   红 = 往外（半径 → R+48）、蓝 = 往里（R → R−48）。前两个**已存在**（280/281），后四个本轮新增。
LEAF = [
    ("灵梦",   398, 280, "mof_bomb_spark",       160.0, False),   # 已存在：不重写
    ("灵梦",   399, 281, "mof_bomb_spark_blue",  160.0, True),
    ("魔理沙", 405, 341, "mof_bomb_spark112",    112.0, False),
    ("魔理沙", 406, 342, "mof_bomb_spark_blue112", 112.0, True),
    ("符卡中", 439, 343, "mof_bomb_spark64",      64.0, False),
    ("符卡中", 440, 344, "mof_bomb_spark_blue64", 64.0, True),
]

HEAD = """script script{n} {{
// ★风神录（110）灵击的柱环之一 —— 一手 TH10 `bullet.anm` **script{src}**（{variant}）的逐条移植。
//   ⚠ 本文件由 `assets/ability/make_mof_bomb_ring_scripts.py` **生成**（改数值请改那个脚本的 RING 表）。
//
//   一手原文（v4 `anmmap` 名）：
//       blendMode(1); iset($I0,32); iset($I1,3); texCircle($I0); layer(13); sprite(sprite362);
//       angleVel(0,0,{spin}); scale(16,16); scaleTime({frames}, {mode}, {hand_x}, {hand_y});
//     {color_note}+{fade}: alphaTime(20,0,0); +20: delete();
//   逐条落位：
//     · `sprite(sprite362)` → `@MOF_BOMB_RING_BENT@`：构建期把 14×128 竖条**极坐标弯环**并沿圆周
//       **铺 32 份**（= 一手 `texCircle(32)` 的那个 32），见 `make_mof_bomb_ring_bent.py`
//       （512×512 画布、基准半径 **R0 = 200 px**、厚 28 px）。
//     · `iset($I0,32)/iset($I1,3)/texCircle($I0)` 三行**删掉**：TH18 的 anm VM 是否执行 `texCircle`
//       没有实跑先例（实跑症状 = 竖条被放大成一条大光晕）。
//     · `scaleTime` 的**第 3/4 参是目标值**（不是起点→终点）—— 铁证：本仓
//       `engine/card/th18/13-mallet-clear-and-th17-charge.md` 记的 TH18 原生 `ability.anm` script28
//       `scale(0.0f,2.8f) + scaleTime(20,4,1.0f,1.0f)` 的行为是「y 从 2.8 拉到 **1.0**」。
//       ⇒ 一手那对不等比的数（{hand_x}/{hand_y}）是**给竖条 + texCircle 的**；我们的贴图已经是整圈
//       ⇒ **两轴同值** = 目标半径 / R0 = {target_px:.0f} / 200 = **{target:.2f}**。
//     · 起手 `scale(16,16)`（一手，= 半径 16 px）⇒ `{start:.2f}`；成长 {frames} 帧（一手 `scaleTime` 的帧数）。
//     · 偏离一手①：`scaleTime` 的 mode 一手写 {mode}，我们统一 **4**（mode 6/5 在 TH18 版 anm 里没有一手；
//       4 = 本仓在用的线性档，先例 116 季解 / TH16 季解）。
//     · 偏离一手②：TH18 卡侧起的是**平铺** VM（v8 的 `scriptNew` 才是带父级的子 VM）⇒ 脚本自己的
//       `scale` 就是「多少个 R0（200 px）」；一手「子环 scale 乘在主脚本 scale 上」那层乘法不存在。
//   · 变体 = **{variant}**（一手主脚本 script{master}；卡侧按 `spell_now`/`reimu` 选，与半径曲线同一判据）。
{extra}    blendMode(1);
    layer(13);
    sprite(@MOF_BOMB_RING_BENT@);
    angleVel(0.0f, 0.0f, {spin}f);
    scale({start:.2f}f, {start:.2f}f);
    scaleTime({frames}, 4, {target:.2f}f, {target:.2f}f);
{color}+{fade}:
    alphaTime(20, 0, 0);
+20:
    delete();
}}
"""

LEAF_HEAD = """script script{n} {{
// ★风神录（110）灵击的**火星（枫叶）**之一 —— 一手 TH10 `bullet.anm` **script{src}**（{variant}）的移植。
//   ⚠ 本文件由 `assets/ability/make_mof_bomb_ring_scripts.py` **生成**（改数值请改那个脚本的 LEAF 表）。
//   一手原文与逐条解读见 `280_mof_bomb_spark.anm.txt` 的文件头（同一套配方）；本变体唯一的差别是
//   **基准半径 = {base:.0f} px**（一手 {src} 的字面量，与该变体的灵击半径相同）：
//     起点 = 半径 {base:.0f} px 的随机方向；红往外 ⇒ 终点 {base:.0f}+48 = {out:.0f} px、
//     蓝往里 ⇒ 终点 {base:.0f}−48 = {inn:.0f} px。`posTime($I0,4,…)` 线性、`scaleTime($I0,4,0,0)` 缩到 0、
//     `wait($I0)` 之后 `+8: delete()`；寿命 `$I0 = 20..35` 帧（`isetRand(15)+iadd(20)`）。
//   ⚠ 变体 = **{variant}**（一手主脚本 script{master}）—— 2026-09-24 owner 实跑「灵击依旧很奇怪」的根因
//     就是我最初把**灵梦那套**（半径 160）用在了魔理沙身上（她的灵击半径只有 112）。
{extra}    sprite(@MOF_BOMB_SPARK@);
    layer(13);
    type(1);
    blendMode(1);
    alpha(255);
    color({color});
    isetRand($I0, 15);
    iadd($I0, 20);
    fsetMul(%F0, %RANDF, 1.5f);
    fadd(%F0, 0.5f);
    scale(%F0, %F0);
    rotate(0.0f, 0.0f, %RANDRAD);
    fsetDiv(%F0, %RANDRAD, 20.0f);
    angleVel(0.0f, 0.0f, %F0);
    fset(%F0, %RANDRAD);
    fcos(%F1, %F0);
    fsin(%F2, %F0);
    fsetMul(%F3, %F1, {base:.1f}f);
    fsetMul(%F0, %F2, {base:.1f}f);
    fmul(%F1, 48.0f);
    fmul(%F2, 48.0f);
    {combine1}
    {combine2}
    pos(%F3, %F0, 0.0f);
    posTime($I0, 4, %F1, %F2, 0.0f);
    scaleTime($I0, 4, 0.0f, 0.0f);
    wait($I0);
+8:
    delete();
}}
"""


def ring_text(rec):
    variant, master, n, src, pad, spin, target_px, frames, fade, magenta = rec
    return HEAD.format(
        src=src, variant=variant, master=master, n=n, pad=pad,
        spin=("%.8f" % spin).rstrip("0").rstrip("."),
        frames=frames, mode={60: 6, 40: 5}.get(frames, 4),
        hand_x="32.0f" if (src % 2 == 1) else "16.0f",
        hand_y="%.1f" % (200.0 if magenta else 160.0) if variant == "灵梦"
               else ("%.1f" % (140.0 if magenta else 112.0) if variant == "魔理沙"
                     else "%.1f" % (80.0 if magenta else 64.0)),
        target_px=target_px, target=target_px / R0, start=16.0 / R0,
        color=("    color(255, 0, 255);\n" if magenta else ""),
        color_note=("（紫环：`color(255,0,255)`）" if magenta else "（白环：一手不写 `color`）"),
        extra="",
        fade=fade)


def leaf_text(rec):
    variant, src, n, pad, base, blue = rec
    master = {"灵梦": 400, "魔理沙": 407, "符卡中": 441}[variant]
    return LEAF_HEAD.format(
        src=src, variant=variant, master=master, n=n, pad=pad, base=base,
        out=base + 48.0, inn=base - 48.0,
        color=("0, 0, 255" if blue else "255, 0, 0"),
        combine1=(f"fsetSub(%F1, %F3, %F1);          // ★ 一手：往里收 ⇒ 终点 {base - 48.0:.0f} px"
                  if blue else "fadd(%F1, %F3);"),
        combine2=("fsetSub(%F2, %F0, %F2);" if blue else "fadd(%F2, %F0);"),
        extra="")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--check", action="store_true", help="只对账不写盘")
    a = ap.parse_args()
    bad = 0
    print(f"环贴图基准半径 R0 = {R0:.0f} px ⇒ 脚本里 scale = 半径 / {R0:.0f}\n")
    print("| 脚本 | 变体 | 一手 | 旋转(rad/帧) | 目标半径(px) | scale | 成长(帧) | 淡出 | 紫 |")
    print("| --- | --- | --- | --- | --- | --- | --- | --- | --- |")
    for rec in RING:
        variant, master, n, src, pad, spin, target_px, frames, fade, magenta = rec
        print(f"| {n}_{pad} | {variant} | script{src} | {spin:+.8f} | {target_px:.0f} | "
              f"{target_px / R0:.2f} | {frames} | +{fade} | {'✅' if magenta else '—'} |")
        path = SCRIPTS / f"{n}_{pad}.anm.txt"
        text = ring_text(rec)
        if a.check:
            if not path.exists() or path.read_text() != text:
                print(f"  ✗ 不一致：{path.name}", file=sys.stderr); bad += 1
        else:
            path.write_text(text)
    print("\n| 脚本 | 变体 | 一手 | 基准半径(px) | 红/蓝 |")
    print("| --- | --- | --- | --- | --- |")
    for rec in LEAF:
        variant, src, n, pad, base, blue = rec
        print(f"| {n}_{pad} | {variant} | script{src} | {base:.0f} | {'蓝(往里)' if blue else '红(往外)'} |")
        if n in (280, 281):
            continue                      # 既有的灵梦火星：本轮不动（已实跑确认在画）
        path = SCRIPTS / f"{n}_{pad}.anm.txt"
        text = leaf_text(rec)
        if a.check:
            if not path.exists() or path.read_text() != text:
                print(f"  ✗ 不一致：{path.name}", file=sys.stderr); bad += 1
        else:
            path.write_text(text)
    if a.check and bad:
        sys.exit(1)
    print(f"\n{'对账通过' if a.check else '已写'}：环 {len(RING)} 支（灵梦 4 / 魔理沙 4 / 符卡中 4）"
          f"、火星 {len(LEAF) - 2} 支新增（灵梦那两支是既有的 280/281）")
    return 0


if __name__ == "__main__":
    sys.exit(main())
