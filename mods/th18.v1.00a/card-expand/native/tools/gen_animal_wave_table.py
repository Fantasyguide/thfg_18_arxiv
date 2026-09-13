#!/usr/bin/env python3
"""Generate cards/animal_wave_table.h from TH18 stage ECL + enemy.anm sprite sizes.

Usage (repo root):
    python3 mods/th18.v1.00a/card-expand/native/tools/gen_animal_wave_table.py

The table is a static, per-stage/per-MainSub-group total used by card 117 AL58c.
It expands simple `$A--` loops (ZUN's common `deci([x]); jmpNeq(label,0)` idiom)
so that e.g. st01 MainSub01's `$A=8` loop contributes 8 red + 8 blue, not 1+1.
"""
import collections
import os
import re
import subprocess
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..', '..', '..', '..'))
THECL = os.path.join(ROOT, 'local/vendor/thtk/build/thecl/thecl')
THANM = os.path.join(ROOT, 'local/vendor/thtk/build/thanm/thanm')
ECLMAP = os.path.join(ROOT, 'local/vendor/eclmap/eclmap/th18.eclm')
STAGE_ECLS = [os.path.join(ROOT, f'local/th18.v1.00a/dat/st0{i}.ecl') for i in range(1, 8)]
ENEMY_ANM = os.path.join(ROOT, 'local/th18.v1.00a/dat/enemy.anm')
OUT = os.path.join(ROOT, 'mods/th18.v1.00a/card-expand/native/cards/animal_wave_table.h')


def run(cmd):
    return subprocess.check_output(cmd, cwd=ROOT).decode('utf-8', 'replace')


def parse_anm_sizes():
    """Return sprite_id -> (w,h) and flat script_id -> (w,h)."""
    text = run([THANM, '-l', '18', ENEMY_ANM])
    sprite = {}
    for m in re.finditer(r'sprite(\d+):\s*\{\s*x:\s*(-?\d+),\s*y:\s*(-?\d+),\s*w:\s*(\d+),\s*h:\s*(\d+)', text):
        sprite[int(m.group(1))] = (int(m.group(4)), int(m.group(5)))
    script = {}
    for m in re.finditer(r'^script\s+script(\d+)\s*\{', text, re.M):
        num = int(m.group(1))
        start = m.end() - 1
        depth = 0
        j = start
        while j < len(text):
            if text[j] == '{':
                depth += 1
            elif text[j] == '}':
                depth -= 1
                if depth == 0:
                    break
            j += 1
        body = text[start + 1:j]
        ids = [int(x) for x in re.findall(r'\bsprite(\d+)\b', body)]
        sizes = {sprite[i] for i in ids if i in sprite}
        if len(sizes) == 1:
            script[num] = sizes.pop()
    return script


def parse_functions(text):
    funcs = {}
    for m in re.finditer(r'^void\s+(\w+)\s*\(\)\s*\{', text, re.M):
        name = m.group(1)
        start = m.end()
        depth = 1
        i = start
        while i < len(text) and depth:
            if text[i] == '{':
                depth += 1
            elif text[i] == '}':
                depth -= 1
            i += 1
        funcs[name] = text[start:i - 1]
    return funcs


def parse_stmts(body):
    stmts = []
    for raw in body.splitlines():
        line = raw.split('//')[0].strip()
        if not line:
            continue
        stmts.append(line)
    labels = {}
    for idx, st in enumerate(stmts):
        if re.match(r'^\w+:\s*$', st):
            labels[st[:-1]] = idx
    return stmts, labels


def run_wave(funcs, root, spawn_counts, stack=None, step_limit=300000):
    if stack is None:
        stack = []
    if root in stack or root not in funcs:
        return
    stmts, labels = parse_stmts(funcs[root])
    pc = 0
    var = {}
    steps = 0
    last_spawn_step = 0
    last_spawn_total = sum(spawn_counts.values())
    while pc < len(stmts) and steps < step_limit:
        steps += 1
        st = stmts[pc]
        m = re.match(r'^enmCreate\w*\("([^"]+)"', st)
        if m:
            spawn_counts[m.group(1)] += 1
            last_spawn_step = steps
            last_spawn_total = sum(spawn_counts.values())
            pc += 1
            continue
        m = re.match(r'^@(\w+)\s*\((.*)\)\s*(async)?\s*;', st)
        if m:
            callee = m.group(1)
            if callee.startswith('MainSub'):
                run_wave(funcs, callee, spawn_counts, stack + [root], step_limit)
            pc += 1
            continue
        m = re.match(r'^\$(\w+)\s*=\s*(\d+)\s*;', st)
        if m:
            var[m.group(1)] = int(m.group(2))
            pc += 1
            continue
        m = re.match(r'^if\s*\(\$(\w+)--\)\s*goto\s+(\w+)\s*@', st)
        if m:
            key, lab = m.group(1), m.group(2)
            old = var.get(key, 0)
            var[key] = old - 1
            pc = labels.get(lab, pc + 1) if old != 0 else pc + 1
            continue
        m = re.match(r'^if\s*\((\d+)\)\s*goto\s+(\w+)\s*@', st)
        if m:
            pc = labels.get(m.group(2), pc + 1) if int(m.group(1)) != 0 else pc + 1
            continue
        m = re.match(r'^goto\s+(\w+)\s*@', st)
        if m:
            pc = labels.get(m.group(1), pc + 1)
            continue
        if st.startswith('return'):
            return
        # no new spawns for a long while -> assume a post-spawn infinite loop; stop
        if steps - last_spawn_step > 20000:
            return
        pc += 1


def spawn_identity(funcs, name, script_sizes):
    body = funcs.get(name)
    if not body:
        return None
    m = re.search(r'anmSetMain\(0,\s*(-?\d+)\)', body)
    if m:
        type_id = int(m.group(1))
        script_id = type_id
    else:
        m = re.search(r'anmSetSprite\(0,\s*(-?\d+)\)', body)
        if not m:
            return None
        type_id = 0
        script_id = int(m.group(1))
    size = script_sizes.get(script_id)
    if not size:
        return None
    return type_id, size[0], size[1]


def main():
    script_sizes = parse_anm_sizes()
    entries = collections.Counter()  # (stage, wave, type_id, w, h) -> total
    detail = {}
    for stage, path in enumerate(STAGE_ECLS, start=1):
        if not os.path.exists(path):
            continue
        text = run([THECL, '-j', '-d', '18', '-m', ECLMAP, path, '/tmp/animal_gen_stage.txt'])
        text = open('/tmp/animal_gen_stage.txt', encoding='utf-8', errors='replace').read()
        funcs = parse_functions(text)
        nums = set()
        for name in funcs:
            m = re.match(r'^MainSub(\d+)', name)
            if m:
                nums.add(int(m.group(1)))
        for n in sorted(nums):
            root = f'MainSub{n:02d}'
            if root not in funcs:
                candidates = [k for k in funcs if re.match(r'^MainSub%02d' % n, k)]
                if not candidates:
                    continue
                root = candidates[0]
            spawn = collections.Counter()
            run_wave(funcs, root, spawn)
            for name, cnt in spawn.items():
                ident = spawn_identity(funcs, name, script_sizes)
                if ident is None:
                    continue
                type_id, w, h = ident
                entries[(stage, n, type_id, w, h)] += cnt
                detail.setdefault((stage, n, type_id, w, h), collections.Counter())[name] += cnt
    lines = []
    lines.append('/* Auto-generated by native/tools/gen_animal_wave_table.py. DO NOT EDIT BY HAND. */')
    lines.append('#pragma once')
    lines.append('#include <stdint.h>')
    lines.append('')
    lines.append('typedef struct {')
    lines.append('    int32_t stage;    /* CE_CURRENT_STAGE(), 1-based */')
    lines.append('    int32_t wave;     /* MainSub 数字（去尾字母）*/')
    lines.append('    int32_t type_id;  /* anm_main 外观脚本号（anmSetSprite 路径为 0）*/')
    lines.append('    int32_t w, h;     /* sprite 原始尺寸 */')
    lines.append('    int32_t total;    /* 本波该组动态总数（已展开 $A--/deci 循环）*/')
    lines.append('} animal_wave_group_total_t;')
    lines.append('')
    lines.append('static const animal_wave_group_total_t animal_wave_group_totals[] = {')
    for (stage, wave, type_id, w, h), total in sorted(entries.items()):
        lines.append(f'    {{ {stage}, {wave}, {type_id}, {w}, {h}, {total} }},')
    lines.append('};')
    lines.append('')
    lines.append(f'#define ANIMAL_WAVE_GROUP_TOTAL_COUNT {len(entries)}')
    lines.append('')
    with open(OUT, 'w', encoding='utf-8') as f:
        f.write('\n'.join(lines))
    # print stage1 summary to stdout for review
    for (stage, wave, type_id, w, h), total in sorted(entries.items()):
        if stage == 1:
            names = ','.join(f'{k}x{v}' for k, v in sorted(detail[(stage, wave, type_id, w, h)].items()))
            print(f'stage1 wave{wave} type{type_id} {w}x{h} total{total}: {names}')
    print(f'wrote {OUT}: {len(entries)} entries')


if __name__ == '__main__':
    sys.exit(main())
