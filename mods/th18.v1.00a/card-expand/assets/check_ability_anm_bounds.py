#!/usr/bin/env python3
"""卡 117（鬼形兽）的 ability.anm 索引硬校验（独立于生成器内部状态）。

为什么单独做：`ability.anm` 是所有卡共享的追加文件，sprite / script 号由
`assets/ability/entries/ORDER.txt` 的顺序 + `assets/ability/scripts/` 的连续编号决定。
多人并发时只要 ORDER 合并顺序或脚本编号漂移，脚本里的数字 sprite 就会静默指到别人的
sheet（「滚木」类画面）—— 这边不信任 `anm_ids.h` / 生成器，直接把最终 `native/build/ability.anm`
用 `thanm -l 18` 读回来，对卡 117 用到的每个脚本做四项硬检查：

  1. script id 在最终文件里存在；脚本源文件存在；
  2. 脚本里每个 `spriteN` 都在最终 sprite 表里；
  3. 该 `spriteN` 落在**源脚本里 @NAME@ 应指向的那个 entry**（`ORDER.txt` 里该名字所属的
     entry —— 2026-09-13 图集合并后，多个名字可以同属一个 `ability/atlas/<图集>.png`，
     所以这里比的是"entry 对不对"**外加**"矩形对不对"，不再要求 entry 名 = 名字）；
  4. 脚本里 `scriptNew(scriptN)` 的目标脚本也存在。

另外核对 `native/cards/animal.c` 里用到的每个 `CE_ANM_ABILITY_SPRITE_*`：它由 `anm_ids.h`
给出的 sprite 号必须落在 ORDER 给这个名字指定的那个 entry 的**那个矩形**里。这个检查会在
ORDER 漂移 / 构建残留（anm_ids.h 与 ability.anm 不同步）时硬失败。

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
CARDS_DIR = MOD / "native" / "cards"
ORDER = HERE / "ability" / "entries" / "ORDER.txt"
SCRIPTS_DIR = HERE / "ability" / "scripts"

# 117 的完整脚本段（用户/审计列出的：token 137–143 + HUD 144/145/154–158 + aura 146–148 +
# orbs 149 + strike 150–153 + rainbow 159 + beastap 160–162 + eagle beam 163）。
# ★ 2026-09-13：**不再写死 range(137,164)** —— 那个字面量是"历史上一次性排好的段"，
#   一旦重排脚本/加脚本就会静默指错（合并 PR 之前必须改，见 `ATLAS-MERGE-AUDIT.md` §4.6）。
#   改成**从 anm_ids.h 的宏反查**：`CE_ANM_ABILITY_SCRIPT_<name>` 里 name 命中 117 的族名即可。
# 117 的脚本族名（**不含** bare `GAUGE_`/`HUD_` 这种会和别的卡撞的前缀）。
CARD117_SCRIPT_NAMES = (
    "ANIMAL_", "FISH_", "AURA", "OTTER_ORB", "STRIKE", "SLOT_", "HYPER_BAR_",
    "BEASTAP", "EAGLE_BEAM", "GAUGE_FRAME", "GAUGE_ICON", "SELF_AURA", "TOKEN",
)
# 另外把「117 的 C 源码直接引用的脚本宏」并进来（这才是 117 的权威定义，且免疫重编号）。
CARD117_C_SOURCES = ("animal.c", "youkai.c")


def _card117_scripts(ids_h_text):
    """117 的脚本号 = （C 源码引用的脚本宏）∪（族名命中的宏）。

    为什么不写 `range(137,164)`：那个字面量是"历史上一次性排好的段"，重排/加脚本就会**静默指错**
    （2026-09-13 合并审计 §4.6 就是这条）。改成从 `anm_ids.h` 反查 ⇒ 免疫重编号。
    """
    wanted = set()
    for fn in CARD117_C_SOURCES:
        src = CARDS_DIR / fn
        if src.is_file():
            wanted |= set(re.findall(r"CE_ANM_ABILITY_SCRIPT_[A-Z0-9_]+", src.read_text(encoding="utf-8")))
    out = set()
    for m in _MACRO.finditer(ids_h_text):
        name, val = m.group(1), int(m.group(2))
        if not name.startswith("CE_ANM_ABILITY_SCRIPT_"):
            continue
        if name in wanted or any(k in name for k in CARD117_SCRIPT_NAMES):
            out.add(val)
    return sorted(out)

_ENTRY = re.compile(r"^entry entry(\d+) \{\n(.*?)^\}", re.M | re.S)
_SCRIPT = re.compile(r"^script script(\d+) \{\n(.*?)^\}", re.M | re.S)
_ENTRY_NAME = re.compile(r'name:\s*"([^"]+)"')
_SPRITE_DEF = re.compile(r"^\s+sprite(\d+):", re.M)
_SPRITE_REF = re.compile(r"\bsprite\(sprite(\d+)\)")
_SCRIPT_REF = re.compile(r"\bscriptNew\(script(\d+)\)")
_MACRO = re.compile(r"^\s*#define\s+(CE_ANM_ABILITY_(?:SPRITE|SCRIPT)_[A-Z0-9_]+)\s+(\d+)", re.M)
_TOKEN = re.compile(r"@([A-Za-z_][A-Za-z0-9_]*)@?")


def _read_order():
    """ORDER 名字 → (它所属的 entry 名, 它的矩形 (x,y,w,h))。

    ★ 2026-09-13 图集合并：一个 entry 里有多个 sprite ⇒ 「名字 == entry 名」这条老假设不成立
    （`ICON_WOLF` 与 `AURA_WOLF_0..7` 都住在 `ability/atlas/ATLAS_ANIMAL_64.png` 里）。
    现在按 `anmlib.read_order()` 的解析结果给出**每个名字该落在哪个 entry 的哪个矩形**：
    单图行 → `ability/<NAME>.png` + 整张贴图；图集成员 → `ability/atlas/<图集>.png` + 成员矩形；
    图集名本身 → 第 0 个成员；`REF` → 它指向的目标。
    """
    items, _ = L.read_order(ORDER)
    out, refs = {}, {}
    for it in items:
        if it["kind"] == "single":
            out[it["name"]] = (f"ability/{it['name']}.png", None)      # None = 整张（尺寸未声明时不定）
        elif it["kind"] == "ref":
            refs[it["name"]] = it["target"]
        else:
            entry = f"ability/atlas/{it['name']}.png"
            for m in it["members"]:
                out[m["name"]] = (entry, tuple(m["rect"]))
            out[it["name"]] = (entry, tuple(it["members"][0]["rect"]))
    for name, target in refs.items():
        if target not in out:
            raise SystemExit(f"ORDER.txt：REF {name} 的目标 {target} 不存在")
        out[name] = out[target]
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
    sprite_rects = {}
    for m in _ENTRY.finditer(text):
        name_m = _ENTRY_NAME.search(m.group(2))
        if not name_m:
            continue
        name = name_m.group(1)
        entries[int(m.group(1))] = name
        for sid, rect in L.parse_sprite_rects(m.group(2)).items():
            sprite_entries[sid] = name
            sprite_rects[sid] = rect

    scripts = {int(m.group(1)): m.group(2) for m in _SCRIPT.finditer(text)}
    total_sprites = (max(sprite_entries) + 1) if sprite_entries else 0

    order_map = _read_order()
    ids_h_text = IDS_H.read_text(encoding="utf-8")
    macros = {m.group(1): int(m.group(2)) for m in _MACRO.finditer(ids_h_text)}
    card117_scripts = _card117_scripts(ids_h_text)
    if not card117_scripts:
        raise SystemExit("从 anm_ids.h 里反查不到 117 的脚本（宏名规则变了？见 CARD117_SCRIPT_NAMES）")
    card_src = ANIMAL_C.read_text(encoding="utf-8")
    used_script_macros = set(re.findall(r"CE_ANM_ABILITY_SCRIPT_[A-Z0-9_]+", card_src))
    used_sprite_macros = set(re.findall(r"CE_ANM_ABILITY_SPRITE_[A-Z0-9_]+", card_src))

    bad = []
    print(f"ability.anm: sprites={total_sprites} (max {max(sprite_entries) if sprite_entries else -1}), "
          f"scripts={len(scripts)}; card117 scripts={len(card117_scripts)}")

    # 1) 117 的脚本段：源文件 / 最终 script / sprite 目标 entry / scriptNew 目标。
    for sid in card117_scripts:
        src = _source_for(sid)
        if src is None:
            bad.append(f"script{sid}: 找不到唯一源文件 {sid}_*.anm.txt")
            continue
        body = scripts.get(sid)
        if body is None:
            bad.append(f"script{sid} ({src.name}): 最终 ability.anm 里不存在")
            continue
        expected = {order_map[t][0] for t in _code_tokens(src) if t in order_map}
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
            # 图集合并后还要比**矩形**：entry 对了但矩形挪了，画出来同样是别的东西
            want_rects = {order_map[t][1] for t in _code_tokens(src)
                          if t in order_map and order_map[t][0] == ent and order_map[t][1]}
            if want_rects and sprite_rects.get(r) not in want_rects:
                bad.append(f"script{sid}: sprite{r} 的矩形 {sprite_rects.get(r)} 不在 ORDER 给 {ent} 的 "
                           f"{sorted(want_rects)} 里")
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

    # 3) C 侧动态换 sprite 用的 macro：必须落在 ORDER 给这个名字指定的 entry 的**那个矩形**里。
    #    ⚠ 图集合并后「宏名后缀 == entry 名」这条老假设作废（多个名字住同一个 entry），
    #    改成查 ORDER（= 唯一事实来源）：entry 名 + 矩形都要对上。
    for macro in sorted(used_sprite_macros):
        if macro not in macros:
            bad.append(f"{macro} 不在 {IDS_H.name} 里")
            continue
        sid = macros[macro]
        suffix = macro[len("CE_ANM_ABILITY_SPRITE_"):]
        if suffix not in order_map:
            bad.append(f"{macro}: ORDER.txt 里没有名字 {suffix}")
            continue
        want_entry, want_rect = order_map[suffix]
        got = sprite_entries.get(sid)
        if got != want_entry:
            bad.append(f"{macro}={sid} 落在 {got}，ORDER 期望 {want_entry}")
        elif want_rect and sprite_rects.get(sid) != want_rect:
            bad.append(f"{macro}={sid} 的矩形 {sprite_rects.get(sid)} ≠ ORDER 的 {want_rect}")
    print(f"  C 侧 sprite macro {len(used_sprite_macros)} 个、script macro {len(used_script_macros)} 个："
          f"{'OK' if not bad else '见下'}")

    if bad:
        print("FAIL:")
        for b in bad:
            print("  " + b)
        return 1
    print("OK: 117 的 script 全部存在；所有 sprite 引用在界内、且落在源脚本 @NAME@ 对应 entry 的对应矩形；"
          "scriptNew 目标存在；C 侧 sprite macro 与 ORDER 的 entry/矩形一致。")
    return 0


if __name__ == "__main__":
    sys.exit(main())
