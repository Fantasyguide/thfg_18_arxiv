#!/usr/bin/env python3
"""生成 110（风神录）灵击的**柱环 + 火星**脚本 —— 一手 `bullet.anm` **script400/407/441 三套**逐条移植。

    python3 make_mof_bomb_ring_scripts.py            # 写脚本 + 打印台帐
    python3 make_mof_bomb_ring_scripts.py --check    # 只对账（不写盘），不一致就退出 1

■ 一手灵击**按机体/符卡分三套**（`Bomb::start` 三选一），三套的**主脚本**各自 `scriptNew` 出自己那套子脚本，
  子脚本只差几个数（成长帧数 / 目标半径 / 淡出时点 / 火星基准半径）：

  | 变体 | 主脚本 | 白环（∓18°/帧）| 紫环（±6°/帧）| 火星基准半径 | 火星 |
  | --- | --- | --- | --- | --- | --- |
  | 0 灵梦 | 400（160）| 394 `(60,6,32,160)` / 395 `(60,5,16,160)` | 396/397 `(60,4,16,200)`，淡出 +40 | 160 | 398(红外)/399(蓝内) |
  | 1 魔理沙 | 407（112）| 401 `(40,6,32,112)` / 402 `(40,5,16,112)`，淡出 +160 | 403/404 `(40,4,16,140)` | 112 | 405/406 |
  | 2 符卡中 | 441（64）| 435 `(60,6,32,64)` / 436 `(60,5,16,64)` | 437/438 `(60,4,16,80)` | 64 | 439/440 |

  ⚠ 2026-09-24 两轮 owner 实跑口径：
    · 「灵击依旧很奇怪」= 我最初**只移植了灵梦那一套给全部机体**（魔理沙的灵击半径只有 112）⇒ 三套补全；
    · 「你的这个**波长**比 th10 原生似乎短」+「灵梦的灵击**没有旋转**」= 我曾把 `texCircle` 那三行删掉、
      改成构建期烘一张「沿圆周铺 **32** 份」的环贴图 ⇒ 一圈 96 道细弧（一手只铺 **3** 份）：
      波长比原生短 ~10 倍，转起来只是闪烁。

■ **不再烘环贴图**：TH18 引擎**原生就在用 `texCircle`**（`local/th18.v1.00a/anm/ability/ability.anm`
  里 5 处，写法与一手同形）：
      script29: `sprite(sprite106); … iset($I0, 32); iset($I1, 10); iset($I2, 1); texCircle($I0); scale(48.0f,24.0f);`
      script43: `sprite(sprite104); … iset($I0, 64); iset($I1,  6);                  texCircle($I0);`
  ⇒ `$I0` = 圆的**分段数**、`$I1` = 贴图沿圆周**铺几份**（一手 = **3**）。所以柱环**逐条照抄一手**：
      `sprite(@MOF_BOMB_RING_STRIP@)`（= 一手 `sprite362` 的 1:1 切图，14×128）
      + `iset($I0,32); iset($I1,3); texCircle($I0);`
      + 一手原样的 `scale`/`scaleTime`（`scale.y` 就是**半径 px** —— `Bomb::on_tick` 每帧
        `[bomb+0x3c] = vm.scale.y`，所以 16→160 就是 16→160 px，**不需要**任何 ÷100/÷200 换算）。
  烘环那套（`make_mof_bomb_ring_bent.py` + `ATLAS_MOF_BOMB_RING_BENT`）**退役保留**（图集 entry 不删，
  删了会让后面所有号前移）；要回退就 `--bent`（只换 sprite；回退时请连同 `TUNING.md` §2.0zzj 一起看）。

■ `scaleTime` 的一个坑（一手原生里核对过）：**第 3/4 个参是「目标」**，不是起点→终点 ——
  铁证 = 本仓 `engine/card/th18/13-mallet-clear-and-th17-charge.md` 记的 TH18 原生 script28
  `scale(0.0f,2.8f) + scaleTime(20,4,1.0f,1.0f)`「y 从 2.8 拉到 **1.0**」。

■ 卡侧契约（`bomb10.c`）
  · 柱环：按排期表起，**只写位置**（`entity_pos`），scale/自转/淡出/自删全交给脚本；
  · 火星：出生时写一次位置（视觉锚点），脚本自己按 `%RANDRAD`/`%RANDF` 飞；
  · 脚本号在 `native/anm_ids.h` 里按名字取。
"""
import argparse
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
SCRIPTS = HERE / "scripts"
SPRITE = "@MOF_BOMB_RING_STRIP@"      # 一手 `sprite362` 的 1:1 切图（14×128；entry 58 / sprite 450）
SPRITE_BENT = "@MOF_BOMB_RING_BENT@"  # 退役的烘环（回退用；见文件头）

# 柱环： (变体, 一手主脚本, 本仓号, 一手子脚本号, 文件名, 自转, 目标半径 px, 成长帧, 淡出帧, 紫, 一手 mode)
RING = [
    ("灵梦",   400, 327, 394, "mof_bomb_ring394", -0.31415927, 160.0, 60, 120, False, 6),
    ("灵梦",   400, 328, 395, "mof_bomb_ring395",  0.31415927, 160.0, 60, 120, False, 5),
    ("灵梦",   400, 329, 396, "mof_bomb_ring396",  0.10471976, 200.0, 60,  40, True,  4),
    ("灵梦",   400, 330, 397, "mof_bomb_ring397", -0.10471976, 200.0, 60,  40, True,  4),
    ("魔理沙", 407, 333, 401, "mof_bomb_ring401", -0.31415927, 112.0, 40, 160, False, 6),
    ("魔理沙", 407, 334, 402, "mof_bomb_ring402",  0.31415927, 112.0, 40, 160, False, 5),
    ("魔理沙", 407, 335, 403, "mof_bomb_ring403",  0.10471976, 140.0, 40,  40, True,  4),
    ("魔理沙", 407, 336, 404, "mof_bomb_ring404", -0.10471976, 140.0, 40,  40, True,  4),
    ("符卡中", 441, 337, 435, "mof_bomb_ring435", -0.31415927,  64.0, 60, 120, False, 6),
    ("符卡中", 441, 338, 436, "mof_bomb_ring436",  0.31415927,  64.0, 60, 120, False, 5),
    ("符卡中", 441, 339, 437, "mof_bomb_ring437",  0.10471976,  80.0, 60,  40, True,  4),
    ("符卡中", 441, 340, 438, "mof_bomb_ring438", -0.10471976,  80.0, 60,  40, True,  4),
]

# 火星（枫叶）： (变体, 一手子脚本号, 本仓号, 文件名, 基准半径 px, 蓝?)
LEAF = [
    ("灵梦",   398, 280, "mof_bomb_spark",        160.0, False),
    ("灵梦",   399, 281, "mof_bomb_spark_blue",   160.0, True),
    ("魔理沙", 405, 341, "mof_bomb_spark112",     112.0, False),
    ("魔理沙", 406, 342, "mof_bomb_spark_blue112", 112.0, True),
    ("符卡中", 439, 343, "mof_bomb_spark64",       64.0, False),
    ("符卡中", 440, 344, "mof_bomb_spark_blue64",  64.0, True),
]

HEAD = """script script{n} {{
// ★风神录（110）灵击的柱环之一 —— 一手 TH10 `bullet.anm` **script{src}**（{variant}）的**逐条**移植。
//   ⚠ 本文件由 `assets/ability/make_mof_bomb_ring_scripts.py` **生成**（改数值请改那个脚本的 RING 表）。
//
//   一手原文（v4 `anmmap` 名）：
//       blendMode(1); iset($I0,32); iset($I1,3); texCircle($I0); layer(13); sprite(sprite362);
//       angleVel(0,0,{spin}); scale({hand_scale}); scaleTime({frames}, {mode}, {hand_target});
//       {color_note}+{fade}: alphaTime(20,0,0); +20: delete();
//
//   逐条落位（**数值一处都没改**，只有 `sprite` 与 `texCircle` 的先后按 TH18 原生习惯挪了一下）：
//     · `sprite(sprite362)` → `sprite({sprite})`：一手 sprite362 = TH10 `etama3.png` 的
//       **(65,0,14,128)** 竖条，本仓有 1:1 切图（entry 58 / sprite 450）。
//     · `iset($I0,32); iset($I1,3); texCircle($I0);` **逐条照抄** —— TH18 引擎**原生**就在用这条指令：
//         `ability.anm` script29 `iset($I0,32); iset($I1,10); iset($I2,1); texCircle($I0); scale(48.0f,24.0f);`
//         `ability.anm` script43 `iset($I0,64); iset($I1, 6);                  texCircle($I0);`
//       ⇒ `$I0` = 圆的**分段数**、`$I1` = 贴图沿圆周**铺几份**；一手是 **3 份**（每条弧占 1/9 圈）。
//       ⚠ 2026-09-24：上一版把这三行删掉、改在构建期烘一张「铺 **32** 份」的环贴图 ⇒ owner 实跑
//         「**波长比 th10 原生似乎短**」+「灵梦的灵击**没有旋转**」（96 道细弧转起来只是闪烁）。
//     · `scale` / `scaleTime` **原样照抄一手**（`{hand_scale}` + {frames} 帧到 `{hand_target}`）：
//       一手 `scale.y` 就是**半径 px**（`Bomb::on_tick` 每帧 `[bomb+0x3c] = vm.scale.y`，
//       见 `engine/player/th10/01-bomb-full-chain.md` §3）⇒ 不需要任何 ÷100/÷200 换算。
//     · `scaleTime` 的第 3/4 参是**目标**（铁证：本仓一手记录里 TH18 原生 script28 的
//       `scale(0.0f,2.8f) + scaleTime(20,4,1.0f,1.0f)` = 「y 从 2.8 拉到 1.0」）；mode 也照抄一手（{mode}）。
//   · 变体 = **{variant}**（一手主脚本 script{master}；卡侧按「小灵击档 / 机体」选，与半径曲线同一判据）。
{extra}    blendMode(1);
    layer(13);
    sprite({sprite});
    iset($I0, 32);                  // 一手：圆的分段数（TH18 原生同形：32 / 64）
    iset($I1, 3);                   // 一手：贴图沿圆周**铺 3 份**（TH18 原生用 10 / 6）
    texCircle($I0);                 // ★ 引擎原生指令（TH18 ability.anm script29/32/43 就是这么用的）
    angleVel(0.0f, 0.0f, {spin}f);
    scale({hand_scale});
    scaleTime({frames}, {mode}, {hand_target});
{color}+{fade}:
    alphaTime(20, 0, 0);
+20:
    delete();
}}
"""

LEAF_HEAD = """script script{n} {{
// ★风神录（110）灵击的**火星（枫叶）**之一 —— 一手 TH10 `bullet.anm` **script{src}**（{variant}）的移植。
//   ⚠ 本文件由 `assets/ability/make_mof_bomb_ring_scripts.py` **生成**（改数值请改那个脚本的 LEAF 表）。
//   配方与逐条解读见 `280_mof_bomb_spark.anm.txt` 的文件头；本变体唯一的差别是
//   **基准半径 = {base:.0f} px**（一手 {src} 的字面量，与该变体的灵击半径相同）：
//     起点 = 半径 {base:.0f} px 的随机方向；红往外 ⇒ 终点 {out:.0f} px、蓝往里 ⇒ 终点 {inn:.0f} px。
//     `posTime($I0,4,…)` 线性、`scaleTime($I0,4,0,0)` 缩到 0、`wait($I0)` 之后 `+8: delete()`；
//     寿命 `$I0 = 20..35` 帧（`isetRand(15)+iadd(20)`）。
//   ⚠ 变体 = **{variant}**（一手主脚本 script{master}）—— owner 实跑「灵击依旧很奇怪」的根因之一
//     就是我最初把灵梦那套（半径 160）用在了魔理沙身上（她的灵击半径只有 112）。
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


def ring_text(rec, sprite):
    variant, master, n, src, _pad, spin, target_px, frames, fade, magenta, mode = rec
    # 一手第 3/4 参：每对里第一支写 32（分段×?），第二支写 16 —— 逐条照抄
    hand_x = 32.0 if (src % 2 == 1) else 16.0
    return HEAD.format(
        n=n, src=src, variant=variant, master=master, sprite=sprite,
        spin=("%.8f" % spin).rstrip("0").rstrip("."),
        hand_scale="{:.1f}f, 16.0f".format(hand_x),
        hand_target="{:.1f}f, {:.1f}f".format(hand_x, target_px),
        frames=frames, mode=mode, fade=fade,
        color_note=("（紫环：`color(255,0,255)`）" if magenta else "（白环：一手不写 `color`）"),
        color=("    color(255, 0, 255);\n" if magenta else ""),
        extra="")


def leaf_text(rec):
    variant, src, n, _pad, base, blue = rec
    master = {"灵梦": 400, "魔理沙": 407, "符卡中": 441}[variant]
    return LEAF_HEAD.format(
        n=n, src=src, variant=variant, master=master, base=base,
        out=base + 48.0, inn=base - 48.0,
        color=("0, 0, 255" if blue else "255, 0, 0"),
        combine1=(f"fsetSub(%F1, %F3, %F1);          // ★ 一手：往里收 ⇒ 终点 {base - 48.0:.0f} px"
                  if blue else "fadd(%F1, %F3);"),
        combine2=("fsetSub(%F2, %F0, %F2);" if blue else "fadd(%F2, %F0);"),
        extra="")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--check", action="store_true", help="只对账不写盘")
    ap.add_argument("--bent", action="store_true", help="用退役的烘环贴图（回退路径；见文件头）")
    a = ap.parse_args()
    sprite = SPRITE_BENT if a.bent else SPRITE
    bad = 0
    print("柱环 = 一手原样：`sprite(切图) + iset($I0,32); iset($I1,3); texCircle($I0)`，"
          f"贴图 = {sprite}（scale/scaleTime 全用一手原值）\n")
    print("| 脚本 | 变体 | 一手 | 自转(rad/帧) | 目标半径(px) | 成长(帧) | mode | 淡出 | 紫 |")
    print("| --- | --- | --- | --- | --- | --- | --- | --- | --- |")
    for rec in RING:
        variant, master, n, src, pad, spin, target_px, frames, fade, magenta, mode = rec
        print(f"| {n}_{pad} | {variant} | script{src} | {spin:+.8f} | {target_px:.0f} | "
              f"{frames} | {mode} | +{fade} | {'✅' if magenta else '—'} |")
        path = SCRIPTS / f"{n}_{pad}.anm.txt"
        text = ring_text(rec, sprite)
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
            continue                      # 既有的灵梦火星：不动（已实跑确认在画）
        path = SCRIPTS / f"{n}_{pad}.anm.txt"
        text = leaf_text(rec)
        if a.check:
            if not path.exists() or path.read_text() != text:
                print(f"  ✗ 不一致：{path.name}", file=sys.stderr); bad += 1
        else:
            path.write_text(text)
    if a.check and bad:
        sys.exit(1)
    print(f"\n{'对账通过' if a.check else '已写'}：柱环 {len(RING)} 支（灵梦 4 / 魔理沙 4 / 符卡中 4）、"
          f"火星 {len(LEAF) - 2} 支新增（灵梦那两支是既有的 280/281）")
    return 0


if __name__ == "__main__":
    sys.exit(main())
