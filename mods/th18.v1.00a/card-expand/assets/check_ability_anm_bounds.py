#!/usr/bin/env python3
"""卡 117（鬼形兽）的 ability.anm 索引硬校验（独立于生成器内部状态）。

为什么单独做：`ability.anm` 是所有卡共享的追加文件，sprite / script 号由
`assets/ability/entries/ORDER.txt` 的顺序 + `assets/ability/scripts/` 的连续编号决定。
多人并发时只要 ORDER 合并顺序或脚本编号漂移，脚本里的数字 sprite 就会静默指到别人的
sheet（「滚木」类画面）—— 这边不信任 `anm_ids.h` / 生成器，直接把最终 `native/build/ability.anm`
用 `thanm -l 18` 读回来，对卡 117 用到的每个脚本做四项硬检查：

  1. script id 在最终文件里存在；脚本源文件存在；
  2. 脚本里每个 `spriteN` 都在最终 sprite 表里；
  3. 该 `spriteN` 落在**源脚本里 @NAME@ 应指向的 entry**（ORDER 名字 → `ability/<NAME>.png`）；
  4. 脚本里 `scriptNew(scriptN)` 的目标脚本也存在。

另外核对 `native/cards/animal.c` 里用到的每个 `CE_ANM_ABILITY_SPRITE_*`：它由 `anm_ids.h`
给出的 sprite 号必须落在同名 entry 里。这个检查会在 ORDER 漂移 / 构建残留（anm_ids.h 与
ability.anm 不同步）时硬失败。

    python3 assets/check_ability_anm_bounds.py
"""
import re
import sys
from pathlib import Path

import anmlib as L

HERE, MOD = L.HERE, L.MOD
BUILD_ANM = L.BUILD / "ability.anm"
IDS_H = MOD / "native" / "anm_ids.h"
ANIMAL_C = MOD / "native" / "cards" / "animal.c"
ORDER = HERE / "ability" / "entries" / "ORDER.txt"
SCRIPTS_DIR = HERE / "ability" / "scripts"

# 117 的完整脚本段（用户/审计列出的：token 137–143 + HUD 144/145/154–158 + aura 146–148 +
# orbs 149 + strike 150–153 + rainbow 159 + beastap 160–162 + eagle beam 163）。
CARD117_SCRIPTS = list(range(137, 164))

_ENTRY = re.compile(r"^entry entry(\d+) \{\n(.*?)^\}", re.M | re.S)
_SCRIPT = re.compile(r"^script script(\d+) \{\n(.*?)^\}", re.M | re.S)
_ENTRY_NAME = re.compile(r'name:\s*"([^"]+)"')
_SPRITE_DEF = re.compile(r"^\s+sprite(\d+):", re.M)
_SPRITE_REF = re.compile(r"\bsprite\(sprite(\d+)\)")
_SCRIPT_REF = re.compile(r"\bscriptNew\(script(\d+)\)")
_MACRO = re.compile(r"^\s*#define\s+(CE_ANM_ABILITY_(?:SPRITE|SCRIPT)_[A-Z0-9_]+)\s+(\d+)", re.M)
_TOKEN = re.compile(r"@([A-Za-z_][A-Za-z0-9_]*)@?")


def _read_order():
    """ORDER 名字 → build_ability.py 会写进 ability.anm 的 entry 名。"""
    out = {}
    for line in ORDER.read_text(encoding="utf-8").splitlines():
        s = line.strip()
        if not s or s.startswith("#"):
            continue
        parts = s.split()
        if len(parts) >= 2:
            out[parts[0]] = f"ability/{parts[0]}.png"
    return out


def _source_for(script_id):
    hits = sorted(SCRIPTS_DIR.glob(f"{script_id}_*.anm.txt"))
    if len(hits) != 1:
        return None
    return hits[0]


def _code_tokens(path):
    """去掉 `//` 注释后收集 @NAME@；注释里的举例不算引用。"""
    out = set()
    for line in path.read_text(encoding="utf-8").splitlines():
        out.update(_TOKEN.findall(line.split("//", 1)[0]))
    return out


def main():
    if not BUILD_ANM.is_file():
        raise SystemExit(f"缺 {BUILD_ANM}：先 `make anm`（或 `python3 assets/build_ability.py`）")
    if not IDS_H.is_file():
        raise SystemExit(f"缺 {IDS_H}：先 `make anm`")

    th, td, mapdir = L.find_tools()
    text = L.thanm(["-l", L.VERSION, str(BUILD_ANM), "-m", str(mapdir / "v8.anmm")],
                   BUILD_ANM.parent, (th, td))

    entries = {}
    sprite_entries = {}
    for m in _ENTRY.finditer(text):
        name_m = _ENTRY_NAME.search(m.group(2))
        if not name_m:
            continue
        name = name_m.group(1)
        entries[int(m.group(1))] = name
        for sid in _SPRITE_DEF.findall(m.group(2)):
            sprite_entries[int(sid)] = name

    scripts = {int(m.group(1)): m.group(2) for m in _SCRIPT.finditer(text)}
    total_sprites = (max(sprite_entries) + 1) if sprite_entries else 0

    order_map = _read_order()
    macros = {m.group(1): int(m.group(2)) for m in _MACRO.finditer(IDS_H.read_text(encoding="utf-8"))}
    card_src = ANIMAL_C.read_text(encoding="utf-8")
    used_script_macros = set(re.findall(r"CE_ANM_ABILITY_SCRIPT_[A-Z0-9_]+", card_src))
    used_sprite_macros = set(re.findall(r"CE_ANM_ABILITY_SPRITE_[A-Z0-9_]+", card_src))

    bad = []
    print(f"ability.anm: sprites={total_sprites} (max {max(sprite_entries) if sprite_entries else -1}), "
          f"scripts={len(scripts)}; card117 scripts={len(CARD117_SCRIPTS)}")

    # 1) 117 的脚本段：源文件 / 最终 script / sprite 目标 entry / scriptNew 目标。
    for sid in CARD117_SCRIPTS:
        src = _source_for(sid)
        if src is None:
            bad.append(f"script{sid}: 找不到唯一源文件 {sid}_*.anm.txt")
            continue
        body = scripts.get(sid)
        if body is None:
            bad.append(f"script{sid} ({src.name}): 最终 ability.anm 里不存在")
            continue
        expected = {order_map[t] for t in _code_tokens(src) if t in order_map}
        unknown = sorted({t for t in _code_tokens(src) if t not in order_map})
        if unknown:
            bad.append(f"script{sid}: 源文件里的 @{unknown} 不在 ORDER.txt 里")
        refs = sorted({int(x) for x in _SPRITE_REF.findall(body)})
        oob = [r for r in refs if r not in sprite_entries]
        if oob:
            bad.append(f"script{sid}: sprite 引用越界/不存在 {oob}（total={total_sprites}）")
        for r in refs:
            ent = sprite_entries.get(r)
            if expected and ent not in expected:
                bad.append(f"script{sid}: sprite{r} 落在 {ent}，源脚本期望 {sorted(expected)}")
        srefs = sorted({int(x) for x in _SCRIPT_REF.findall(body)})
        for sr in srefs:
            if sr not in scripts:
                bad.append(f"script{sid}: scriptNew(script{sr}) 目标不存在")
        print(f"  script{sid:<3} {src.name:<36} refs={refs} entries={[sprite_entries.get(r) for r in refs]}"
              f" scriptNew={srefs}")

    # 2) 源脚本里存在、但 C 侧没引用的 script macro（如 144 GAUGE_FRAME）也要检查存在性。
    for macro in sorted(used_script_macros):
        if macro not in macros:
            bad.append(f"{macro} 不在 {IDS_H.name} 里")
            continue
        sid = macros[macro]
        if sid not in scripts:
            bad.append(f"{macro}={sid} 在最终 ability.anm 里不存在")

    # 3) C 侧动态换 sprite 用的 macro：名字同名 entry 必须就是它最终指向的 sheet。
    for macro in sorted(used_sprite_macros):
        if macro not in macros:
            bad.append(f"{macro} 不在 {IDS_H.name} 里")
            continue
        sid = macros[macro]
        suffix = macro[len("CE_ANM_ABILITY_SPRITE_"):]
        want = f"ability/{suffix}.png"
        got = sprite_entries.get(sid)
        if got != want:
            bad.append(f"{macro}={sid} 落在 {got}，期望 {want}")
    print(f"  C 侧 sprite macro {len(used_sprite_macros)} 个、script macro {len(used_script_macros)} 个："
          f"{'OK' if not bad else '见下'}")

    if bad:
        print("FAIL:")
        for b in bad:
            print("  " + b)
        return 1
    print("OK: 117 的 script 全部存在；所有 sprite 引用在界内且落在源脚本 @NAME@ 对应的 entry；"
          "scriptNew 目标存在；C 侧 sprite macro 与 entry 同名。")
    return 0


if __name__ == "__main__":
    sys.exit(main())
