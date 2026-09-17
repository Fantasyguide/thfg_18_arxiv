#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""measure_enemy_variants.py —— 量出 TH18 敌机 anm 的「外观变体（颜色）」与**同形异色**配对。

# 为什么有这个脚本

系统卡 110（风神录）的大信仰点宿主判据是「这敌**长什么样**」（键 = `(anm 源 N, 主脚本 S)`，
见 `native/cards/mof_big_faith_table.h`）。用户 2026-09-18 追加了一个需求方向：
**能不能把「一部分妖精」改成绿妖精**（改精灵 + 改掉落，跟着卡走）—— 要回答它，
先得知道 TH18 自己的贴图里**哪些外观是同形的、只是换了颜色**（那样改脚本号就能换色，
不用新画贴图）。

本脚本只**测量**、不改任何文件（生成/落笔在 `native/cards/*` 与 `assets/*`）。

# 一手口径（全部可重跑）

| 事实 | 来源 |
| --- | --- |
| 脚本 → 贴图：每个变体 5 个脚本共用 12 个连续 sprite | `thanm -l 18 <file>.anm` 的 `scriptN { ins_300(spriteN); }` |
| 变体基址 `b` 的反推规则 | 4 帧脚本 `b = min`；8 帧脚本第一个数是 `b+7`（降序半段）时 `b = min`，是 `b+4` 时 `b = min - 4` |
| 颜色 | `thanm -x 18` 抽出的 PNG，按脚本用到的 sprite 裁切，取 `alpha>200`、`sat>0.45` 像素的**中位色相** |
| 同形 | 逐 pose 比较 alpha 掩码（bbox 内 16×16 覆盖率）的 IoU；**并把「同 pose 的 (w,h) 是否逐条相等」当独立旁证**（只用 `-l`，不需要 PIL）|

# 用法

```bash
# 全部敌机 anm（enemy + st01enm..st07enm + st05enm2）
python3 mods/th18.v1.00a/card-expand/native/tools/measure_enemy_variants.py

# 只看一个文件 / 只跑不需要 PIL 的形状部分
python3 …/measure_enemy_variants.py enemy
python3 …/measure_enemy_variants.py --no-color

# 只打「把非绿变体改成绿变体」的重映射表（脚本号 + sprite 基址）
python3 …/measure_enemy_variants.py enemy --convert
```

★ 颜色部分需要 `PIL` + `numpy`；`thanm` 在 `local/vendor/thtk/build/thanm/thanm`（gitignored，仓库纪律：
不写死机器路径，一律从本文件向上找仓库根）。
"""

from __future__ import annotations

import argparse
import os
import re
import shutil
import subprocess
import sys
import tempfile

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..', '..', '..', '..'))
THANM = os.path.join(ROOT, 'local/vendor/thtk/build/thanm/thanm')
DAT = os.path.join(ROOT, 'local/th18.v1.00a/dat')
ALL_FILES = ['enemy', 'st01enm', 'st02enm', 'st03enm', 'st04enm',
             'st05enm', 'st05enm2', 'st06enm', 'st07enm']

GREEN_LO, GREEN_HI = 85, 165      # 「绿」的色相窗口（校准：GirlBlueA01 hue 228 蓝 / GirlRedA01 hue 12 红）


# ---------------------------------------------------------------- anm 解析

def thanm_list(name):
    out = subprocess.run([THANM, '-l', '18', os.path.join(DAT, f'{name}.anm')],
                         capture_output=True, text=True, check=True).stdout
    entries, scripts = {}, {}
    cur_entry = cur_script = None
    for line in out.splitlines():
        m = re.match(r'^entry (entry\d+) \{', line.strip())
        if m:
            cur_entry = m.group(1)
            entries[cur_entry] = {'name': None, 'sprites': []}
            continue
        m = re.match(r'^\s+name: "(.*)",', line)
        if m and cur_entry:
            entries[cur_entry]['name'] = m.group(1)
        m = re.match(r'^\s+sprite(\d+): \{ x: (-?\d+), y: (-?\d+), w: (\d+), h: (\d+) \}', line)
        if m and cur_entry:
            entries[cur_entry]['sprites'].append(tuple(int(g) for g in m.groups()))
        m = re.match(r'^script (script\d+) \{', line.strip())
        if m:
            cur_script = int(m.group(1)[6:])
            scripts[cur_script] = []
            continue
        m = re.match(r'\s*ins_3\d*\((sprite\d+)\);', line)
        if m and cur_script is not None:
            scripts[cur_script].append(int(m.group(1)[6:]))
    return entries, scripts


def sprite_rects(entries):
    """sprite id → (所属 entry, x, y, w, h)。★ 编号在**整个文件**里连续（entry0 0..119、
    entry1 120..151、entry4 220..327 …，`thanm -l` 实测）⇒ 每个 id 恰好属于一个 entry；
    取色/比形状时**必须只用它自己那一张 PNG**（拿 A 表的矩形去 B 表上裁 = 垃圾像素）。"""
    out = {}
    for key, e in entries.items():
        for sid, x, y, w, h in e['sprites']:
            out.setdefault(sid, (key, x, y, w, h))
    return out


def variant_base(sp):
    """从脚本的 sprite 列表反推变体基址 b；**严格匹配**四种标准 pose，否则返回 None。

    ★ 为什么要严格：TH18 里另有一批杂兵（如 `Cir*`）的脚本只引用一段 8 帧 sprite，
    看着也「像」变体，把它们按 12 窗口分组会把不同敌人并成一个形状组（实测就这么错过一次）。
    妖精类（A 族）的 pose 是**固定四种**（一手 `thanm -l` 逐条核对）：
      · pose0 `[b..b+3]`（4 帧）
      · pose1/2 `[b+4..b+11]`（8 帧升序）
      · pose3/4 `[b+7,b+6,b+5,b+4,b,b+1,b+2,b+3]`（8 帧，前 4 帧逆序）"""
    if not sp:
        return None
    for b in (min(sp), min(sp) - 4):
        if b < 0:
            continue
        if sp in (list(range(b, b + 4)), list(range(b + 4, b + 12)),
                  [b + 7, b + 6, b + 5, b + 4, b, b + 1, b + 2, b + 3]):
            return b
    return None


def variants_of(scripts):
    """b → [脚本号]（只收 12-sprite 变体）。"""
    out = {}
    for s in sorted(scripts):
        b = variant_base(scripts[s])
        if b is not None:
            out.setdefault(b, []).append(s)
    return out


# ---------------------------------------------------------------- 颜色（可选）

def probe_sprites(anm_dir, entries, sprite_ids, rects):
    """→ (hue|None, sat_px, tot_px, [12 个 alpha 掩码|None])。掩码 = bbox 内 alpha>127 的 16×16 覆盖率。"""
    try:
        import numpy as np
        from PIL import Image
    except ImportError:
        return None, 0, 0, None
    hues, sat_px, tot_px = [], 0, 0
    masks = []
    imgs = {}
    for key, e in entries.items():
        if not e['name']:
            continue
        p = os.path.join(anm_dir, e['name'])
        if os.path.exists(p):
            imgs[key] = np.array(Image.open(p).convert('RGBA')).astype('int32')
    for sid in sprite_ids:
        if sid not in rects:
            masks.append(None)
            continue
        owner, x, y, w, h = rects[sid]
        if owner not in imgs:
            masks.append(None)
            continue
        im = imgs[owner]
        sub = im[y:y + h, x:x + w]
        # 掩码（同形判据的主力：同形异色 = 同一张剪影）
        a = sub[:, :, 3] > 127
        if a.any():
            ys, xs = np.where(a)
            m = a[ys.min():ys.max() + 1, xs.min():xs.max() + 1]
            mi = Image.fromarray((m * 255).astype('uint8')).resize((16, 16), Image.BILINEAR)
            masks.append(np.array(mi).astype('float32') / 255.0)
        else:
            masks.append(None)
        px = sub[sub[:, :, 3] > 200][:, :3] / 255.0
        tot_px += len(px)
        if len(px) < 10:
            continue
        mx, mn = px.max(axis=1), px.min(axis=1)
        d = mx - mn
        r, g, b = px[:, 0], px[:, 1], px[:, 2]
        with np.errstate(invalid='ignore', divide='ignore'):
            hr = ((g - b) / d) % 6
            hg = ((b - r) / d) + 2
            hb = ((r - g) / d) + 4
            sat = np.where(mx > 1e-9, d / mx, 0.0)
        hue = np.where(mx == r, hr, np.where(mx == g, hg, hb)) * 60.0
        sel = hue[(d > 1e-9) & (sat > 0.45)]
        sat_px += len(sel)
        if len(sel) >= 5:
            hues.extend(sel.tolist())
    if not hues:
        return None, sat_px, tot_px, masks
    return float(np.median(hues)), sat_px, tot_px, masks


def color_name(h):
    if h is None:
        return '?'
    if GREEN_LO <= h <= GREEN_HI:
        return '绿'
    if h < 25 or h >= 345:
        return '红'
    if h < 60:
        return '橙/茶'
    if h < 260:
        return '青/蓝'
    return '紫/洋红'


# ---------------------------------------------------------------- 分析主体

def analyse(name, with_color=True):
    import numpy as np
    minimum, maximum = np.minimum, np.maximum
    entries, scripts = thanm_list(name)
    rects = sprite_rects(entries)
    variants = variants_of(scripts)

    anm_dir = None
    if with_color:
        anm_dir = tempfile.mkdtemp(prefix='ce-variants-')
        subprocess.run([THANM, '-x', '18', os.path.join(DAT, f'{name}.anm')],
                       cwd=anm_dir, capture_output=True, text=True)

    info = {}
    for b, scr in sorted(variants.items()):
        ids = list(range(b, b + 12))
        hue = sat = tot = masks = None
        if with_color:
            hue, sat, tot, masks = probe_sprites(anm_dir, entries, ids, rects)
        info[b] = {'scripts': scr, 'hue': hue, 'sat': sat, 'tot': tot, 'masks': masks,
                   'rects': [rects.get(i, (None, 0, 0, -1, -1))[1:] for i in ids]}

    # 形状分组：同 pose 的 (w,h) 逐条相等 + 掩码 IoU（有 PIL 时）
    groups = {}
    bases = sorted(info)

    def same_shape(a, b):
        """同形判据 = 主判 **逐 pose alpha 掩码 IoU 中位 ≥ 0.90**（≥ 8 个可比 pose），
        辅判 **12/12 pose 的 (w,h) 逐条相等**（没有 PIL 时只有辅判，见 `--no-color` 的警告）。
        为什么要两判：TH18 里有一批小杂兵（Cir*）的 sprite 恰好与妖精同尺寸，
        单看 (w,h) 会误并——掩码 IoU 立刻分开（实测 IoU ≈ 0）。"""
        ra, rb = info[a]['rects'], info[b]['rects']
        wh = sum(1 for x, y in zip(ra, rb)
                 if x and y and x[2] == y[2] and x[3] == y[3])
        ma, mb = info[a]['masks'], info[b]['masks']
        if ma is None or mb is None:
            return wh == 12
        if wh != 12:
            return False
        ious = []
        for x, y in zip(ma, mb):
            if x is None or y is None:
                continue
            union = float(maximum(x, y).sum())
            if union <= 0:
                continue
            ious.append(float(minimum(x, y).sum()) / union)
        if len(ious) < 8:
            return False
        ious.sort()
        return ious[len(ious) // 2] >= 0.90

    parent = {b: b for b in bases}

    def find(x):
        while parent[x] != x:
            parent[x] = parent[parent[x]]
            x = parent[x]
        return x

    for i, a in enumerate(bases):
        for b in bases[i + 1:]:
            if same_shape(a, b):
                parent[find(b)] = find(a)
    for b in bases:
        groups.setdefault(find(b), []).append(b)

    if anm_dir:
        shutil.rmtree(anm_dir, ignore_errors=True)
    return info, groups


def print_report(name, info, groups, only_convert=False):
    print(f'==== {name}.anm：{len(info)} 个 12-sprite 外观变体，{len(groups)} 个形状组')
    for root, members in sorted(groups.items(), key=lambda kv: min(kv[1])):
        greens = [b for b in members
                  if info[b]['hue'] is not None and GREEN_LO <= info[b]['hue'] <= GREEN_HI]
        if only_convert and not greens:
            continue
        print(f'-- 形状组（基址 {min(members)}）')
        for b in sorted(members):
            v = info[b]
            print('     sprite %-4d..%-4d 脚本 %-24s hue %-6s %s' % (
                b, b + 11, ','.join(str(s) for s in v['scripts']),
                '—' if v['hue'] is None else '%d' % v['hue'], color_name(v['hue'])))
        if not greens:
            print('     ⇒ **没有绿色同形变体**（要变绿只能外供贴图）')
            continue
        g = greens[0]
        print(f'     ⇒ 绿变体 = 基址 {g}（脚本 {info[g]["scripts"]}）；重映射：')
        for b in sorted(members):
            if b == g:
                continue
            ds = info[g]['scripts'][0] - info[b]['scripts'][0]
            print('        sprite %-4d 脚本 %-4d → 脚本 %-4d（脚本 %+d / sprite %+d）' % (
                b, info[b]['scripts'][0], info[g]['scripts'][0], ds, (g - b)))


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument('files', nargs='*', default=None, help=f'anm 名（默认：{" ".join(ALL_FILES)}）')
    ap.add_argument('--no-color', action='store_true', help='不抽图/不算色相（只有形状组与重映射）')
    ap.add_argument('--convert', action='store_true', help='只打含绿变体的形状组')
    a = ap.parse_args()
    if not os.path.exists(THANM):
        sys.exit(f'缺 thanm：{os.path.relpath(THANM, ROOT)}（见 docs/SETUP.md，gitignored）')
    for name in (a.files or ALL_FILES):
        info, groups = analyse(name, with_color=not a.no_color)
        print_report(name, info, groups, only_convert=a.convert)
        print()


if __name__ == '__main__':
    main()
