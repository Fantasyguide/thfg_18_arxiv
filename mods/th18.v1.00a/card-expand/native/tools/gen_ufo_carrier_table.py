#!/usr/bin/env python3
"""生成 `cards/ufo_carrier_table.h` —— 卡 112（星莲船）的**UFO 令牌宿主表**。

# 这张表是什么

owner 2026-09-24 口径：「th12 原版掉落飞碟的怪……只有**大蝴蝶**和**杂鱼妖精**……
**得改谁能飞碟掉落，并且还得给掉的那个改帖图**」。

一手（TH12 v1.00b，全部可复核；逐条见 `TUNING.md` §2.0zzm）：
- **谁掉**：`dropExtra(10/11/12, n)`（ECL op 407，handler `th12:0x4179cc`；表项 = 道具类型、值 = 个数）
  只出现在这几族子程序里：

  | TH12 子程序 | 出现关卡 | 一手值 | 是什么 |
  | --- | --- | --- | --- |
  | `GirlB01` / `GirlB01b` | st01/02/04/07 | `dropExtra(11,1)` / `(10,3)` / `(12,1)` / `(10,1)+(12,1)` | 杂鱼妖精（`anmSelect(2); anmSetMain(0,20/30/35); anmSetSprite(1,85/88/91)`）|
  | `GirlC01` / `GirlC01b/c/d` | st02 | `dropExtra(12,1)` / `(10,1)` / `(10,2)` | 同上（`anmSetMain(0,30); anmSetSprite(1,91)`）|
  | `GirlD01/02/03` | st04/st07 | `dropExtra(12,$I0) + (10,$I1)` / `(11,1)` | **大体型那一族**（`anmSetMain(0,40)` = 64×64 精灵）|
  | `PhantomC1` / `Phantom01c/d` | st03/04/06 | `dropExtra(11,1)` / `(12,1)` / `(10,1)` | 幻影妖精（`anmSetSprite(0,78/80/82)`）|
  | `Boss2..Boss10`（每个相位）/ `MBoss*Dead` | 全部 | 相位 `(10,1)+(12,1)`；道中 boss 死 `(10,2)+(12,2)+(11,3..15)` | boss |

- **怎么认**：死亡事件 `ce_enemy_death_t` **没有**敌人类型号（`id` 是逐实例递增的）⇒ 只能认「它长什么样」：
  运行时读 `enemy+0x1494`（= `anmSelect` 的 `N`）与 `enemy+0x1498`（= `K==0` 那次 `anmSetMain`/`anmSetSprite`
  的 `S`）—— 与 110 的大信仰点表（`mof_big_faith_table.h`）**同一套机器可读身份**。
- **TH18 侧怎么挑宿主**：按**同名子程序**配对（ZUN 各作共用角色命名：TH12 的 `GirlB01/GirlC01/GirlD01-03/Phantom*`
  在 TH18 的每一关里都有同名子程序，且 anm 调用形状同形）。再按**精灵尺寸**分档：
  `S=40/224` 那一族是 **64×64**（= TH12 里 `anmSetMain(0,40)` 的大体型那一族）⇒ 换**蝴蝶**皮；
  其余（32-48 px）⇒ 换**妖精**皮。尺寸是**量出来的**（`thanm -l 18 enemy.anm` 的 sprite 表）。

⚠ `dropExtra(13, n)`（boss 的 20-44）**不当作令牌**：一手那 44 个"彩虹令牌"与社区全局账
（整轮约 55 个红+虹令牌 ⇒ 约 27 次 UFO）**直接矛盾**，owner 2026-09-24 也判「道中 boss 怎么可能掉这么多」。
⇒ 本卡 boss 的令牌数由 owner 第 11 轮口径统一（**2 枚、会变的、随机**）决定；🟡 存疑仍登记在案。

# ★★★ 分档判据 = **血量**（owner 2026-09-24 第 12 轮口径）

> owner 原话：「**我怀疑 GirlC01 不是杂鱼妖精**。**杂鱼妖精是一碰就碎的**，但是我一般说的"**中**"妖精，
>   血量一般**得打一会儿**」。

一手（`enmCreate(sub, x, y, hp, score, item)` 的第 4 参 = hp，见 `engine/ecl/th16/ECL-info.md`）——
**TH12 那几个掉落令牌的宿主子程序，血量恰好分成三档，与 owner 的说法逐条吻合**：

| TH12 子程序 | 创建点 hp | 是什么 |
| --- | --- | --- |
| `Phantom01/01c/01d` | **50..130** | **一碰就碎 = 真·杂鱼妖精** |
| `GirlB01/GirlB01b` | 180..800（中位 300）| 小~中 |
| `GirlC01/GirlC01b/c/d` | **400..800**（中位 700）| **中妖精（得打一会儿）** |
| `GirlD01/02/03` | 400..2000 | 中~大 |
| `PhantomC1` | 500 | 中 |

⇒ 本表按 **hp 阈值**（`UFO_CARRIER_SCRUB_HP`，默认 **400**）分档：
**min(hp) ≤ 400 ⇒ 杂鱼妖精皮（32×32，`sprite84..87`）；否则 ⇒ 大蝴蝶皮（64×64，`sprite125..132`）**。
每行都把量到的 hp 区间写进注释，判据与结果一眼可核；要挪阈值或翻单行 = 改这个脚本再跑一遍。

# 用法（幂等）

    python3 mods/th18.v1.00a/card-expand/native/tools/gen_ufo_carrier_table.py
    make -C native check      # 表一变就会重编 DLL，check 会交叉核对
"""
from __future__ import annotations

import os
import re
import subprocess
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..', '..', '..', '..'))
THECL = os.path.join(ROOT, 'local/vendor/thtk/build/thecl/thecl')
THANM = os.path.join(ROOT, 'local/vendor/thtk/build/thanm/thanm')
ECLMAP = os.path.join(ROOT, 'local/vendor/eclmap/eclmap/th18.eclm')
ENEMY_ANM = os.path.join(ROOT, 'local/th18.v1.00a/dat/enemy.anm')
STAGES = [os.path.join(ROOT, f'local/th18.v1.00a/dat/st0{i}.ecl') for i in range(1, 8)]
OUT = os.path.join(ROOT, 'mods/th18.v1.00a/card-expand/native/cards/ufo_carrier_table.h')

# 宿主子程序名单（TH12 一手掉令牌那几族 + **TH18 里"一碰就碎"的杂鱼妖精族**）
#
# ★★ 为什么有第二类（owner 2026-09-24 第 12 轮）：owner 用 TH18 一面的实机结构点了名 ——
#   「th18 第一面，第一波是三波**红绿阴阳玉**，第二波是**一些杂鱼妖精外加一个中妖精**」。
#   逐条对 TH18 st01 的 ECL：`MainSub00` = `Cir00p/Cir00m`（hp 120）刷 3 波 = **红绿阴阳玉**（`Cir` = 阴阳）；
#   `MainSub01` = 4 只 `GirlRedA01/GirlBlueA01`（hp **100**）+ `MainSub01c` 的 1 只 `GirlC01`（hp **1000**）
#   = **「一些杂鱼妖精 + 一个中妖精」** ✅✅ ⇒ TH18 的**杂鱼妖精 = `GirlRedA0x/GirlBlueA0x` 那一族**
#   （(N,S) = (2,5) / (2,0)，hp 100..300），**中妖精 = `GirlC01`**（hp 600..3200）。
#   ⚠ **阴阳玉（`Cir*`，(2,56/59/65/68/77/80)）不入表**：owner 明说它是"红绿阴阳玉"、不是妖精。
#   ⇒ 加上这一族之后，"杂鱼妖精掉飞碟"这件事在 TH18 里才真的看得见（原先只有 TH12 同名的那几族）。
CARRIER_SUBS = {
    # —— 杂鱼妖精（TH18 一碰就碎的那一族；owner 点名的"第二波"）——
    'GirlRedA01':  'TH18 红妖精（hp 100 = 一碰就碎；owner 点名）',
    'GirlRedA01n': 'TH18 红妖精（同上）',
    'GirlRedA02':  'TH18 红妖精（同上）',
    'GirlRedA03':  'TH18 红妖精（同上）',
    'GirlBlueA01': 'TH18 蓝妖精（hp 100 = 一碰就碎；owner 点名）',
    'GirlBlueA01n':'TH18 蓝妖精（同上）',
    'GirlBlueA02': 'TH18 蓝妖精（同上）',
    'GirlBlueA03': 'TH18 蓝妖精（同上）',
    'GirlA02':     'TH18 妖精（hp 200 = 一碰就碎）',
    'Phantom01b':  'TH18 幻影妖精（hp 280 = 一碰就碎）',
    'Phantom02':   'TH18 幻影妖精（hp 300）',
    'Phantom02b':  'TH18 幻影妖精（hp 300）',
    'Phantom03':   'TH18 幻影妖精（hp 400）',
    # —— TH12 一手点名的令牌宿主（同名子程序；hp 都 > 400 ⇒ 换蝴蝶皮）——
    'GirlB01': 'TH12 GirlB01（st01/02/04/07 掉令牌；hp 500 = 中妖精）',
    'GirlB01b': 'TH12 GirlB01b（st07）',
    'GirlC01': 'TH12 GirlC01（st02 掉令牌；hp 600..3200 = 中妖精）',
    'GirlC01b': 'TH12 GirlC01b（st02）',
    'GirlC01c': 'TH12 GirlC01c（st02）',
    'GirlC01d': 'TH12 GirlC01d（st02）',
    'GirlD01': 'TH12 GirlD01（大体型那一族；st04/st07）',
    'GirlD02': 'TH12 GirlD02（大体型那一族；st04/st07）',
    'GirlD03': 'TH12 GirlD03（大体型那一族；st04）',
    'PhantomC1': 'TH12 PhantomC1（幻影妖精；st03）',
    'Phantom01': 'TH12 Phantom01（st04/st06 的 _at0 掉令牌）',
    'Phantom01c': 'TH12 Phantom01c（st06）',
    'Phantom01d': 'TH12 Phantom01d（st06）',
}


def run(cmd):
    return subprocess.check_output(cmd, cwd=ROOT).decode('utf-8', 'replace')


SCRUB_HP = 400      # 「一碰就碎」的血量上限（owner 口径）；≤ 它 ⇒ 杂鱼妖精皮，否则大蝴蝶皮


def sub_hp(stage_paths, name):
    """→ 这个子程序在**所有创建点**上的 hp 列表（`enmCreate*(name, x, y, hp, …)` 的第 4 个参数）。"""
    out = []
    for p in stage_paths:
        txt = run([THECL, '-d', '18', '-m', ECLMAP, p])
        for m in re.finditer(r'enmCreate\w*\(\s*"%s"\s*,\s*[^,]+,\s*[^,]+,\s*(-?\d+)' % re.escape(name), txt):
            out.append(int(m.group(1)))
    return out


def script_sizes():
    """→ {脚本号: (w,h)}：脚本里第一张 sprite 的尺寸（`thanm -l 18 enemy.anm`）。"""
    text = run([THANM, '-l', '18', ENEMY_ANM])
    sp = {int(m.group(1)): (int(m.group(4)), int(m.group(5)))
          for m in re.finditer(r'sprite(\d+):\s*\{\s*x:\s*(-?\d+),\s*y:\s*(-?\d+),\s*w:\s*(\d+),\s*h:\s*(\d+)', text)}
    out = {}
    for m in re.finditer(r'^script\s+(script\d+)\s*\{(.*?)^\}', text, re.S | re.M):
        num, body = int(m.group(1)[6:]), m.group(2)
        ids = [int(x) for x in re.findall(r'ins_300\(sprite(\d+)\)', body)][:1] or \
              [int(x) for x in re.findall(r'sprite\(sprite(\d+)\)', body)][:1]
        if ids:
            out[num] = sp.get(ids[0], (0, 0))
    return out


def stage_subs(path):
    """→ {子程序名: (正文, 行号)}（thecl 反编译一份就够）。"""
    text = run([THECL, '-d', '18', '-m', ECLMAP, path])
    lines = text.splitlines()
    starts = [(i, m.group(1)) for i, l in enumerate(lines)
              if (m := re.match(r'^void\s+([A-Za-z_]\w*)\s*\(', l))]
    out = {}
    for k, (i, name) in enumerate(starts):
        end = starts[k + 1][0] if k + 1 < len(starts) else len(lines)
        out[name] = ('\n'.join(lines[i:end]), i + 1)
    return out, (starts, lines)


def main() -> int:
    sizes = script_sizes()
    hp_by_sub = {name: sub_hp(STAGES, name) for name in CARRIER_SUBS}
    rows = {}          # (n, s) -> {"kind":…, "who":set, "stages":set, "size":…, "hp":[…]}
    for idx, path in enumerate(STAGES, start=1):
        subs, _ = stage_subs(path)
        for name, (body, _ln) in subs.items():
            if name not in CARRIER_SUBS:
                continue
            m = re.search(r'anmSelect\((\d+)\)', body)
            if not m:
                continue
            n = int(m.group(1))
            s = None
            for mm in re.finditer(r'anmSet(?:Main|Sprite)\(0,\s*(-?\d+)\)', body):
                s = int(mm.group(1))
                break
            if s is None or s < 0:
                continue
            w, h = sizes.get(s, (0, 0))
            key = (n, s)
            r = rows.setdefault(key, {"who": set(), "stages": set(), "size": (w, h), "hp": []})
            r["who"].add(CARRIER_SUBS[name].split('（')[0])
            r["stages"].add(idx)
            r["hp"] += hp_by_sub.get(name, [])
    for key, r in rows.items():
        hps = r["hp"]
        lo = min(hps) if hps else 0
        hi = max(hps) if hps else 0
        r["hp_lo"], r["hp_hi"] = lo, hi
        # ★ owner 第 12 轮口径：**一碰就碎（hp ≤ 阈值）⇒ 杂鱼妖精皮**，其余（中/大，要打一会儿）⇒ 大蝴蝶皮。
        r["kind"] = 'FAIRY' if (hps and lo <= SCRUB_HP) else 'BFLY'
    if not rows:
        print('✖ 一个宿主都没扫到（thecl / eclmap 路径对不上？）', file=sys.stderr)
        return 1
    lines = [
        '/* 由 native/tools/gen_ufo_carrier_table.py 生成 —— 别手改；改表请改那个脚本再跑一遍。',
        ' * 生成命令：python3 mods/th18.v1.00a/card-expand/native/tools/gen_ufo_carrier_table.py',
        ' *',
        ' * 卡 112（星莲船）的 **UFO 令牌宿主表**：键 = 敌人「长什么样」的机器可读身份',
        ' *   `N` = `anmSelect` 的选择子（`enemy+0x1494`）、`S` = `K==0` 那次 `anmSetMain`/`anmSetSprite`',
        ' *   的主脚本（`enemy+0x1498`）—— 与 110 的大信仰点表（`mof_big_faith_table.h`）同一套判据。',
        ' *',
        ' * owner 2026-09-24：「th12 原版掉落飞碟的怪……只有**大蝴蝶**和**杂鱼妖精**（+ boss）；',
        ' *   **得改谁能飞碟掉落，并且还得给掉的那个改帖图**」⇒ 表里的敌人才掉令牌，',
        ' *   并且它们会被 C 侧换上 TH12 的皮肤（脚本 351/352 走位 + 353 头顶旋转飞碟）。',
        ' *',
        ' * 一手的宿主子程序（TH12，逐关）与「换哪张皮」的理由：见生成器文件头 + `TUNING.md` §2.0zzm。',
        ' * 下方每行的注释 = 本行的 (N,S) 在 TH18 里由**哪些同名子程序**使用 + 精灵尺寸（量出来的）。',
        ' */',
        '#pragma once',
        '#ifndef UFO_CARRIER_TABLE_H',
        '#define UFO_CARRIER_TABLE_H',
        '#include <stdint.h>',
        '',
        'typedef enum {',
        '    UFO_CARRIER_KIND_FAIRY = 0,   /* 换「杂鱼妖精」皮：脚本 352（32×32 四帧走位）*/',
        '    UFO_CARRIER_KIND_BFLY  = 1,   /* 换「大蝴蝶」皮：脚本 351（64×64 四帧走位）*/',
        '} ufo_carrier_kind_t;',
        '',
        'typedef struct {',
        '    int16_t  n;        /* anmSelect 选择子（enemy+0x1494）*/',
        '    int16_t  s;        /* 主脚本（enemy+0x1498，K==0 的那次）*/',
        '    uint8_t  kind;     /* ufo_carrier_kind_t */',
        '    uint8_t  sprite_w; /* 主脚本第一张精灵的宽（仅供人读；分档就是靠它）*/',
        '    uint8_t  sprite_h;',
        '    int16_t  hp_min;   /* 创建点 hp 的最小值（owner 第 12 轮的分档判据：≤ 400 = 一碰就碎）*/',
        '    int16_t  hp_max;',
        '    const char *who;   /* 一手出处（TH12 侧的子程序名）*/',
        '    const char *stages;/* 在 TH18 的哪几关出现 */',
        '} ufo_carrier_key_t;',
        '',
        '/* ★ 表（按 (N,S) 升序；一条 = 一个"外观"）*/',
        'static const ufo_carrier_key_t UFO_CARRIER_KEYS[] = {',
    ]
    for (n, s), r in sorted(rows.items()):
        stages = '/'.join(f'st{i}' for i in sorted(r['stages']))
        who = '、'.join(sorted(r['who']))
        lines.append(f'    {{ {n:4d}, {s:4d}, UFO_CARRIER_KIND_{r["kind"]:<5}, '
                     f'{r["size"][0]:3d}, {r["size"][1]:3d}, {r.get("hp_lo", 0):5d}, {r.get("hp_hi", 0):5d}, '
                     f'"{who}", "{stages}" }},')
    lines += [
        '};',
        '#define UFO_CARRIER_KEY_COUNT (int)(sizeof(UFO_CARRIER_KEYS) / sizeof(UFO_CARRIER_KEYS[0]))',
        '',
        '#endif  /* UFO_CARRIER_TABLE_H */',
        '',
    ]
    with open(OUT, 'w', encoding='utf-8') as f:
        f.write('\n'.join(lines))
    print(f'写 {os.path.relpath(OUT, ROOT)}（{len(rows)} 行）：')
    for (n, s), r in sorted(rows.items()):
        print(f'  (N={n}, S={s:3d}) {r["kind"]:5s} sprite {r["size"][0]}x{r["size"][1]}  '
              f'hp {r.get("hp_lo", 0)}..{r.get("hp_hi", 0)}  '
              f'stages={"".join(str(i) for i in sorted(r["stages"]))}  {"/".join(sorted(r["who"]))}')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
