#!/usr/bin/env python3
"""把 cards/ 里的新卡图追加进原生 abcard.anm，重编到 native/build/abcard.anm，并校验。

    python3 build_abcard.py                # 构建 + 自检 + 校验 cards.js 索引
    python3 build_abcard.py --verify-only  # 只自检已有的 build/abcard.anm

索引规则：cards/ORDER.txt 第 k 行（0 起）→ sprite_large = base + 2k，sprite_small = base + 2k + 1，
base = 原生 entry 数（th18 = 118；abcard 里 sprite 号 = entry 号）。
新 entry 的字段照抄 BLANK_max / BLANK_min（只换 name 与号）；_min 源图 64×80 垫成 64×128 透明底。
任何校验不过 → exit 1，**不会**去改 JSON。通用部分在 anmlib.py。
"""
import argparse
import json
import sys
from pathlib import Path

import anmlib as L
from anmlib import parse_entries, make_entry, insert_entries  # noqa: F401  —— 单测直接从这里取

HERE, MOD = L.HERE, L.MOD
ORDER = HERE / "cards" / "ORDER.txt"
CARDS_JS = MOD / "patch" / "th18" / "cards.js"
TEX = L.BUILD / "tex-abcard"
OUT_ANM = L.BUILD / "abcard.anm"
TEMPLATES = {"max": "ability/BLANK_max.png", "min": "ability/BLANK_min.png"}
# 引擎硬顶：贴图键 = `(anm 槽号 << 8) | entry 下标`，绑贴图时**只取低 8 位** ⇒ 一个 anm 文件
# 最多 256 个 entry（下标 0..255）。一手：th18:0x486d38（打包）/ th18:0x47e1a4（解包），
# 见 engine/anm/th18/06-entry-limit-and-multi-sprite.md §1.1。与 `build_ability.py` 的 `MAX_ENTRIES` 同一口径。
# ★ 为什么这里必须有这道门（2026-09-13 补）：abcard 每张新卡吃 **2** 条 entry，是卡侧**最先**撞的墙
#   （`CE_CARD_MAX_NEW` 现在是 197，比它紧得多：118 + 2N ≤ 256 ⇒ 只能进 69 张卡）。而越限**不崩不报错** ——
#   第 256 条之后的 entry 绑贴图时下标被截断，实跑里表现为「卡图错位」（AUDIT.md §CC 那个失败模式）。
#   所以它必须是**构建期硬门**，而不是等实跑发现。
MAX_ENTRIES = 256


def expected_sprites(order, base: int):
    return {name: (base + 2 * k, base + 2 * k + 1) for k, name in enumerate(order)}


def check_cards_js(cards: dict, expected: dict, n_entries: int, base: int):
    """→ 错误行列表。规则：索引都 < n_entries；ORDER 里的卡必须等于推出的对；不在 ORDER 的卡只能用原生索引（< base）。"""
    errs = []
    for cid, c in cards.items():
        nm = c.get("internal_name", "?")
        pair = (c.get("sprite_large"), c.get("sprite_small"))
        if any(p is None for p in pair):
            continue
        if any(p >= n_entries or p < 0 for p in pair):
            errs.append(f"卡 {cid} ({nm}): sprite {pair} 越界（entry 数 {n_entries}）")
        if nm in expected:
            if pair != expected[nm]:
                errs.append(f"卡 {cid} ({nm}): JSON 填的是 {pair}，ORDER 推出应为 {expected[nm]}")
        elif any(p >= base for p in pair):
            errs.append(f"卡 {cid} ({nm}): 用了新索引 {pair} 但 ORDER.txt 里没有它")
    return errs


def prepare_textures(order, tpl_max, tpl_min, retail_dir, retail_entries):
    from PIL import Image
    want_max = (int(tpl_max["fields"]["THTXWidth"]), int(tpl_max["fields"]["THTXHeight"]))   # 256×320
    want_min = (int(tpl_min["sprite"]["w"]), int(tpl_min["sprite"]["h"]))                     # 64×80
    pad_min = (int(tpl_min["fields"]["THTXWidth"]), int(tpl_min["fields"]["THTXHeight"]))    # 64×128
    L.fresh_dir(TEX)
    L.link_retail_textures(TEX, retail_dir, retail_entries)
    (TEX / "ability").mkdir(exist_ok=True)
    for name in order:
        big, small = HERE / "cards" / f"{name}_max.png", HERE / "cards" / f"{name}_min.png"
        for p in (big, small):
            if not p.is_file():
                raise SystemExit(f"缺 {p}")
        im = Image.open(big).convert("RGBA")
        if im.size != want_max:
            raise SystemExit(f"{big.name} 是 {im.size}，要 {want_max}")
        im.save(TEX / "ability" / f"{name}_max.png")
        im = Image.open(small).convert("RGBA")
        if im.size != want_min:
            raise SystemExit(f"{small.name} 是 {im.size}，要 {want_min}")
        canvas = Image.new("RGBA", pad_min, (0, 0, 0, 0))
        canvas.paste(im, (0, 0))
        canvas.save(TEX / "ability" / f"{name}_min.png")


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--verify-only", action="store_true")
    a = ap.parse_args()
    th, td, mapdir = L.find_tools()
    tools, anmmap = (th, td), mapdir / "v8.anmm"
    retail_dir, retail_text = L.retail("abcard")
    retail_entries = parse_entries(retail_text)
    base = len(retail_entries)
    by_name = {e["name"]: e for e in retail_entries}
    tpl_max, tpl_min = by_name[TEMPLATES["max"]], by_name[TEMPLATES["min"]]
    # abcard 的 ORDER.txt 是**一列名字**（不是 ability 那套 v2 图集语法）⇒ 用 read_order_names
    order = L.read_order_names(ORDER)
    exp = expected_sprites(order, base)

    # entry 上限硬门（照 build_ability.py 的同名检查）——**先于任何写盘**，
    # 这样越限时既不会产出坏 anm，也不会去改 cards.js。
    n_total = base + 2 * len(order)
    if n_total > MAX_ENTRIES:
        raise SystemExit(
            f"entry 总数 {n_total} > {MAX_ENTRIES}（= 原生 {base} + 2×{len(order)} 张卡）："
            f"贴图键低 8 位放不下，≥256 的 entry 会**绑错贴图**"
            f"（`th18:0x486d38` 打包 / `th18:0x47e1a4` 解包）。\n"
            f"  改法：① 把大图/小图并进**一个 entry**（一个 entry 两个 sprite）⇒ 可容纳的卡数翻倍；"
            f"② 或把新卡图拆到别的空闲 anm 槽（槽号 < 0x21）。\n"
            f"  这一版**没有**产出文件。")

    if not a.verify_only:
        prepare_textures(order, tpl_max, tpl_min, retail_dir, retail_entries)
        blocks = []
        for name, (lg, sm) in exp.items():
            blocks.append(make_entry(tpl_max, lg, f"ability/{name}_max.png"))
            blocks.append(make_entry(tpl_min, sm, f"ability/{name}_min.png"))
        L.compile_anm(insert_entries(retail_text, blocks), TEX, OUT_ANM, tools, anmmap)
        print(f"build: {OUT_ANM.relative_to(MOD)}（{OUT_ANM.stat().st_size:,} B）；原生 entry {base}，追加 {2 * len(order)}")

    _, rebuilt, _ = L.verify_rebuilt(OUT_ANM, retail_text, retail_dir, tools, anmmap)
    if len(rebuilt) != base + 2 * len(order):
        raise SystemExit(f"entry 数 {len(rebuilt)}，应为 {base} + 2×{len(order)}")
    for name, (lg, sm) in exp.items():
        for idx, kind, tpl in ((lg, "max", tpl_max), (sm, "min", tpl_min)):
            e = rebuilt[idx]
            if e["idx"] != idx or e["name"] != f"ability/{name}_{kind}.png" or e["fields"] != tpl["fields"] or e["sprite"] != tpl["sprite"]:
                raise SystemExit(f"新 entry{idx}（{name}_{kind}）字段与 BLANK 模板不一致")
    if not CARDS_JS.is_file():
        raise SystemExit(f"没有 {CARDS_JS}")
    errs = check_cards_js(json.loads(CARDS_JS.read_text(encoding="utf-8")), exp, len(rebuilt), base)
    if errs:
        raise SystemExit("cards.js 索引校验不过：\n  " + "\n  ".join(errs))
    print(f"verify: entry {len(rebuilt)} = {base} + 2×{len(order)}；原生 spec/贴图逐项一致；cards.js 索引一致\n")
    room = MAX_ENTRIES - n_total
    print(f"  ★ entry 余量：用到 {n_total} / 上限 {MAX_ENTRIES}（贴图键低 8 位）⇒ **还剩 {room} 个 entry**"
          f" = **还能进 {room // 2} 张卡**"
          f"（卡侧最先撞的就是这道墙，比 `CE_CARD_MAX_NEW` 的 197 张紧得多；见 CAPACITY.md §5-①）")
    print(f"{'NAME':<16}{'sprite_large':>13}{'sprite_small':>13}")
    for name, (lg, sm) in exp.items():
        print(f"{name:<16}{lg:>13}{sm:>13}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
