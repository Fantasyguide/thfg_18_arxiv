#!/usr/bin/env python3
"""神灵（113）**逐型的 anm 脚本**生成器 —— 把「父 + 外环/内芯（普通态）」「父 + 白环/glow（灵界态）」
按 type 0..3 各出一组，写进 `ability/scripts/`。

    python3 make_trance_spirit_scripts.py          # 生成/覆盖（幂等）
    python3 make_trance_spirit_scripts.py --check  # 只对账，不写

## 为什么需要它（而不是手写 24 个文件）

一手（`engine/spirit/th13/01-spirit-and-trance.md` §2.3，`th13:0x43907e`）：
**引擎按 `script = type*6 + 1`（普通）/ `type*6 + 4`（灵界）选脚本** —— 也就是
**每个 type 各有一组父+两个子**，四型的差别在：

| 态 | 差别落在哪 | 一手值 |
| --- | --- | --- |
| 普通态 | **同一对贴图**（`astral.anm` sprite1 外环 / sprite2 内芯），四型靠 **`color()`** 分 | 外环 0 绿(0,127,0) / 1 橄榄(63,63,0) / 2 紫(63,0,63) / 3 灰(63,63,63)；内芯 0 品红(255,32,255) / 1 蓝(80,80,255) / 2 绿(32,255,32) / 3 灰(128,128,128) |
| 灵界态 | **各型各一对贴图**（白环 sprite`3+type`、glow sprite`7+type`），**不染色** | 无 `color()` |

⇒ 24 个文件里只有「贴图名 + 两个颜色的三元组」在变，所以用表驱动生成；
**手抄 24 份一定会漂**（本仓已经为「同一个数抄在 11 个文件里」付过代价，见 `AUDIT` §BJ6）。

## 与已落地那批的关系

`179–184`（type 0 普通态 / type 0 灵界态）是**上一批手写**的、且**已被 `ORDER.txt` 与卡侧引用**，
所以本生成器**不碰它们**（`--check` 会核对它们与表里的 type 0 行一致，不一致就报出来）。

`ORDER.txt` 里 `SPIRIT_TRANCE_0..3` 四个 entry **已经全部就位**（C0 = type0、C1 = type1…），
`SPIRIT_PLAIN_0/1` 是四型**共用**的那一对贴图 ⇒ 本生成器只加脚本，**不动 ORDER**。

## 脚本号

`build_ability.py` 的硬约束：文件名号 = `68 + 序号`、**不许有洞**。所以本生成器
**不自己挑号**，而是取「当前最大号 + 1」往后连续排（表里的顺序 = 号序）。
要重编号请用 `renumber_scripts.py`。
"""
from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
SCRIPTS = HERE / "scripts"

# ---- 一手表（`astral.anm`；`SPIRIT` §2.3）------------------------------------------------
#   sprite 名 = ORDER.txt 的 entry 名（build_ability.py 会把 `@NAME@` 换成 sprite 号）
#   普通态：外环 / 内芯用同一对贴图（SPIRIT_PLAIN_0/1），四型靠 color 分
#   灵界态：白环 = TRANCE_{2*type}、glow = TRANCE_{2*type+1}（ORDER 里 C0R0/C0R1/C1R0… 的排列）
TYPES = [
    # type, 普通外环色,          普通内芯色,         灵界白环 sprite,     灵界 glow sprite
    (0, (0, 127, 0),   (255, 32, 255), "SPIRIT_TRANCE_0", "SPIRIT_TRANCE_1"),
    (1, (63, 63, 0),   (80, 80, 255),  "SPIRIT_TRANCE_2", "SPIRIT_TRANCE_3"),
    (2, (63, 0, 63),   (32, 255, 32),  "SPIRIT_TRANCE_4", "SPIRIT_TRANCE_5"),
    (3, (63, 63, 63),  (128, 128, 128), "SPIRIT_TRANCE_6", "SPIRIT_TRANCE_7"),
]

# 已落地、不许动的手写脚本（type 0 两组）与其「目录号」的对应
FROZEN = {0: (179, 180, 181, 182, 183, 184)}

HEADER = """script script{num} {{
// 自动生成：assets/ability/make_trance_spirit_scripts.py（别手改，改表再重跑）
// 神灵（113）type {t} 的{kind} —— 逐条照抄 TH13 `astral.anm` script{n}（一手：engine/spirit/th13/01 §2.3）
"""

PLAIN_PARENT = HEADER + """//   父（普通态）：-1: alpha(255); scale(1.5,1.5); layer(6); 0: scriptNew(两个子);   ← 一手是 alpha(0)，见下面那条偏差
//                +200: scaleTime(200,0,0.4,0.4); stop(); interruptLabel(1); +240: delete();
//   ⇒ 「随时间不断变小而消失」是**脚本自己**在做（卡侧不许再写 scale，会被插值器盖掉）
// ★★ 2026-09-13 第五轮（TH18 侧的**必需偏差**，用户实跑「按空格只有灵界槽涨、看不见灵」）：
//   一手的父脚本是 `alpha(0)` —— 父是**不带 sprite 的容器**（自己什么都不画），
//   在 TH13 上这句只影响父自己，两个子各有自己的 `color()`/`alpha(255)` ⇒ 看得见。
//   TH18 侧的子 VM 会吃到父的颜色（父子颜色继承那一档）⇒ 父 alpha 0 会把**整支**压成透明
//   ⇒ 神灵看不见，但收集逻辑照跑（槽照涨）= 用户看到的现象。
//   父不画东西 ⇒ 把它写成 255 在任何语义下都不会改变观感（唯一作用是"不再拖累子"）；
//   卡侧还额外在 spawn 之后写一次 `ce_anm_set_color(父, 0xffffffff)` 做第二道保险。
//   🟡 追踪项：父→子到底是"乘 alpha"还是"完全覆盖"，见 AUDIT §CB-5。
-1:
    alpha(255);
    scale(1.5f, 1.5f);
    layer(6);
0:
    scriptNew(@SCRIPT_SPIRIT_PLAIN{RINGSUF}_RING@);
    scriptNew(@SCRIPT_SPIRIT_PLAIN{RINGSUF}_CORE@);
+200:
    scaleTime(200, 0, 0.4f, 0.4f);
    stop();
    interruptLabel(1);
+40:
    delete();
}}
"""

PLAIN_RING = HEADER + """//   外环：呼吸 1.5↔1.6（20+20 帧循环）、blendMode 2、色 = 一手 type {t} 外环 {ring_rgb}
-1:
    type(1);
    sprite(@SPIRIT_PLAIN_0);
    color({r}, {g}, {b});
    alpha(255);
    blendMode(2);
    layer(6);
0:
offset68:
    scaleTime(20, 9, 1.5f, 1.5f);
+20:
    scaleTime(20, 9, 1.6f, 1.6f);
+20:
    jmp(offset68, 0);
    stop();
    interruptLabel(1);
    scaleTime(40, 1, 0.0f, 0.0f);
+40:
    delete();
}}
"""

PLAIN_CORE = HEADER + """//   内芯：呼吸 0.7↔0.9、blendMode 1、色 = 一手 type {t} 内芯 {core_rgb}
-1:
    type(1);
    sprite(@SPIRIT_PLAIN_1);
    color({r}, {g}, {b});
    blendMode(1);
    layer(6);
    alpha(255);
0:
offset80:
    scaleTime(20, 9, 0.7f, 0.7f);
+20:
    scaleTime(20, 9, 0.9f, 0.9f);
+20:
    jmp(offset80, 0);
    stop();
    interruptLabel(1);
    scaleTime(40, 1, 0.0f, 0.0f);
+40:
    delete();
}}
"""

TRANCE_PARENT = HEADER + """//   父（灵界态）：layer(13); scriptNew(白环); scriptNew(glow); stop();
//   ★ 与普通态三点不同（都是一手原文）：**没有** -1 段 / **没有** alpha+scale / **没有**淡出段
//     ⇒ 灵界态神灵不缩不淡，收尾由卡侧负责（`ce_anm_delete`，不是 interrupt —— 本组**没有** interruptLabel）
    layer(13);
    scriptNew(@SCRIPT_SPIRIT_TRANCE{RINGSUF}_RING@);
    scriptNew(@SCRIPT_SPIRIT_TRANCE{RINGSUF}_GLOW@);
    stop();
}}
"""

TRANCE_RING = HEADER + """//   白环：呼吸 0.95↔1.0、blendMode 1、**不染色**（贴图自带白）
-1:
    type(1);
    sprite(@{ring_sprite});
    blendMode(1);
    layer(13);
    color(255, 255, 255);
    alpha(255);
0:
offset48:
    scaleTime(20, 9, 0.95f, 0.95f);
+20:
    scaleTime(20, 9, 1.0f, 1.0f);
+20:
    jmp(offset48, 0);
    stop();
}}
"""

TRANCE_GLOW = HEADER + """//   glow：呼吸 0.95↔1.0、blendMode 1、`angleVel(0,0,3°/帧)` 自转、**不染色**
-1:
    type(1);
    sprite(@{glow_sprite});
    blendMode(1);
    layer(13);
    color(255, 255, 255);
    alpha(255);
    angleVel(0.0f, 0.0f, 0.05235988f);
0:
offset68:
    scaleTime(20, 9, 0.95f, 0.95f);
+20:
    scaleTime(20, 9, 1.0f, 1.0f);
+20:
    jmp(offset68, 0);
    stop();
}}
"""

_FILE = re.compile(r"^(\d+)_(.+)\.anm\.txt$")


def existing() -> dict[int, Path]:
    out = {}
    for p in SCRIPTS.iterdir():
        m = _FILE.match(p.name)
        if m:
            out[int(m.group(1))] = p
    return out


def build_plan(has: dict[int, Path]):
    """返回 [(文件名, 正文)] 与「冻结号 → 应该长什么样」的对账表。

    ★ 2026-09-13（图集合并 PR 顺手修）：**本生成器以前不是幂等的** —— 它按「当前最大号 + 1」发号，
    重跑时**认不出自己上次生成的那 18 个文件**（`233..250`），于是从 262 起**又生成一批同名副本**
    ⇒ `build_ability.py` 的「文件名号必须 = 68 + 序号、不许有洞」直接红（真踩过：`SCRIPT_SPIRIT_PLAIN1`
    与 ORDER 撞名而整次构建失败）。现在改成**按名字认领**：上次生成过的名字 → **覆盖那几个号**，
    不再另开一批；只有全新的名字才从「最大号 + 1」往后排。
    """
    frozen_nums = set()
    for nums in FROZEN.values():
        frozen_nums.update(nums)
    nxt = max(has) + 1 if has else 68
    plan, expect = [], {}
    # 已存在的文件名（不含号）→ 号：本生成器靠它认领自己上次的产物
    own = {}
    for n, p in sorted(has.items()):
        m = _FILE.match(p.name)
        if m:
            own[m.group(2)] = n

    def body_for(t, ring, core, ring_sprite, glow_sprite, nums):
        n_par, n_rng, n_core, n_tpar, n_trng, n_glow = nums
        # ⚠ 占位名由 build_ability.py 按**文件名 stem 大写**生成 ⇒ `233_spirit_plain1_ring` →
        #   `@SCRIPT_SPIRIT_PLAIN1_RING@`（**没有** PLAIN 与 1 之间的下划线）；type 0 那批
        #   （`179_spirit_plain_ring`）同理 → `@SCRIPT_SPIRIT_PLAIN_RING@`。
        suf = "" if t in FROZEN else f"{t}"
        return {
            n_par: PLAIN_PARENT.format(t=t, kind="普通态", num=n_par, n=1, RINGSUF=suf),
            n_rng: PLAIN_RING.format(t=t, kind="普通态外环", num=n_rng, n=2,
                                     ring_rgb=str(ring), r=ring[0], g=ring[1], b=ring[2]),
            n_core: PLAIN_CORE.format(t=t, kind="普通态内芯", num=n_core, n=3,
                                      core_rgb=str(core), r=core[0], g=core[1], b=core[2]),
            n_tpar: TRANCE_PARENT.format(t=t, kind="灵界态", num=n_tpar, n=4, RINGSUF=suf),
            n_trng: TRANCE_RING.format(t=t, kind="灵界态白环", num=n_trng, n=5,
                                       ring_sprite=ring_sprite),
            n_glow: TRANCE_GLOW.format(t=t, kind="灵界态 glow", num=n_glow, n=6,
                                       glow_sprite=glow_sprite),
        }

    names = {
        "par": "spirit_plain{t}", "ring": "spirit_plain{t}_ring", "core": "spirit_plain{t}_core",
        "tpar": "spirit_trance{t}", "tring": "spirit_trance{t}_ring", "tglow": "spirit_trance{t}_glow",
    }
    for (t, ring, core, ring_sprite, glow_sprite) in TYPES:
        if t in FROZEN:
            nums = FROZEN[t]
            expect.update(body_for(t, ring, core, ring_sprite, glow_sprite, nums))
            continue
        keys = ("par", "ring", "core", "tpar", "tring", "tglow")
        fnames = [f"{names[k].format(t=t)}.anm.txt" for k in keys]
        found = [own.get(f[:-len(".anm.txt")]) for f in fnames]
        if any(found) and not all(found):
            raise SystemExit(f"type {t} 的 6 个脚本只找到一部分：{[f for f, n in zip(fnames, found) if n]} —— "
                             f"先跑 renumber_scripts.py 理一遍号")
        if all(found):
            nums = tuple(found)                         # 覆盖上次生成的这几个号（幂等）
            if list(nums) != sorted(nums):
                raise SystemExit(f"type {t} 的 6 个脚本号 {nums} 不是升序（号序 = 顺序）—— 先 renumber_scripts.py")
        else:
            nums = tuple(range(nxt, nxt + 6))
            nxt += 6
            for key, n in zip(keys, nums):
                if n in has:
                    raise SystemExit(f"号 {n} 已被 {has[n].name} 占用 —— 先跑 renumber_scripts.py")
        for key, n in zip(keys, nums):
            plan.append((f"{n}_{names[key].format(t=t)}.anm.txt",
                         body_for(t, ring, core, ring_sprite, glow_sprite, nums)[n]))
    return plan, expect


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--check", action="store_true", help="只对账（核对冻结的那 6 个脚本与表一致）")
    a = ap.parse_args()

    has = existing()
    plan, expect = build_plan(has)

    # ① 冻结脚本与表对账：内容不一致 = 表漂了（表是唯一真源）
    bad = []
    for n, want in sorted(expect.items()):
        p = has.get(n)
        if p is None:
            bad.append(f"号 {n} 缺失（被引用的手写脚本不在了？）")
            continue
        got = p.read_text()
        # 手写的那 6 个带自己的说明段落 ⇒ 只比「指令正文」（去掉注释行与空行）
        def strip(s: str) -> str:
            """比「指令正文」：去掉整行注释、空行，以及行尾的 `// …` 注释。"""
            out = []
            for ln in s.splitlines():
                ln = ln.split("//", 1)[0].rstrip()
                if ln.strip():
                    out.append(ln)
            return "\n".join(out)
        if strip(got) != strip(want):
            bad.append(f"号 {n}（{p.name}）正文与表不一致：\n--- 表里 ---\n{strip(want)}\n--- 文件里 ---\n{strip(got)}")
    if bad:
        print("⛔ 冻结脚本对账失败：", file=sys.stderr)
        for b in bad:
            print("  " + b, file=sys.stderr)
        return 1
    print(f"✅ 冻结脚本对账通过（{len(expect)} 个：type 0 两组）")

    if a.check:
        print(f"（--check）待生成 {len(plan)} 个：{', '.join(n for n, _ in plan) or '无'}")
        return 0

    for name, text in plan:
        (SCRIPTS / name).write_text(text)
        print(f"  + {name}")
    print(f"生成 {len(plan)} 个脚本；接着跑 `make anm anm-verify`（脚本号必须与文件名连续一致）。")
    return 0


if __name__ == "__main__":
    sys.exit(main())
