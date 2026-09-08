#!/usr/bin/env python3
"""往零售 ability.anm 追加贴图 entry 与特效脚本，重编到 native/build/ability.anm，并生成 native/anm_ids.h。

    python3 build_ability.py                # 构建 + 自检 + 写 anm_ids.h
    python3 build_ability.py --verify-only  # 只自检已有的 build/ability.anm

输入：
    ability/entries/ORDER.txt     一行 `NAME  源图路径(相对 assets/)  [WxH]`；只追加不重排。
                                  第 k 行 → entry (7+k)、sprite (109+k)（数字由零售文件算出，不写死）
                                  不写尺寸 = 卡图（256×320，字段抄 abcard BLANK_max）；写 WxH（2 的幂）= 整张贴图一个 sprite
    ability/scripts/NN_name.anm.txt  一个文件一个脚本，NN 从 68 起连续；正文可用 @NAME 引用 ORDER 里的 sprite 号
输出：
    native/build/ability.anm、native/anm_ids.h（CE_ANM_ABILITY_SPRITE_<NAME> / CE_ANM_ABILITY_SCRIPT_<name>）

贴图 entry 字段照抄 abcard 的 BLANK_max（256×512 贴图、THTX 256×320、format 1）——源图必须是 256×320。
"""
import argparse
import re
import sys
from pathlib import Path

import anmlib as L

HERE, MOD = L.HERE, L.MOD
ENTRIES = HERE / "ability" / "entries" / "ORDER.txt"
SCRIPTS = HERE / "ability" / "scripts"
TEX = L.BUILD / "tex-ability"
OUT_ANM = L.BUILD / "ability.anm"
IDS_H = MOD / "native" / "anm_ids.h"
TEMPLATE = ("abcard", "ability/BLANK_max.png")
_FILE = re.compile(r"^(\d+)_([A-Za-z_][A-Za-z0-9_]*)\.anm\.txt$")


def src_path(rel: str):
    """ORDER 里的源图路径：默认相对 assets/；`local/` 开头的相对仓库根（零售贴图搬用——版权字节只在 local/，构建时取，不入库）。"""
    return (L.REPO / rel) if rel.startswith("local/") else (HERE / rel)


def plan(retail_text: str):
    """→ (entries=[(name, src, entry_idx, sprite_idx)], scripts=[(name, path, script_idx)])。"""
    n_entry = len(L.parse_entries(retail_text))
    n_script = len(L.parse_scripts(retail_text))
    sprite0 = L.max_sprite_id(retail_text) + 1
    entries = [(r[0], src_path(r[1]), n_entry + k, sprite0 + k, (r[2] if len(r) > 2 else None)) for k, r in enumerate(L.read_order(ENTRIES))]
    scripts = []
    # 按**编号数值**排（不是文件名字典序）：脚本号过 99 之后 "100_…" 的字典序会排到 "68_…" 前面
    for k, p in enumerate(sorted(SCRIPTS.glob("*.anm.txt"),
                                 key=lambda q: (int(m.group(1)) if (m := _FILE.match(q.name)) else -1, q.name))):
        m = _FILE.match(p.name)
        if not m:
            raise SystemExit(f"{p.name}：文件名要像 68_reverse_flash.anm.txt")
        want = n_script + k
        if int(m.group(1)) != want:
            raise SystemExit(f"{p.name}：编号应为 {want}（零售 {n_script} 个脚本，按文件名顺序连续追加）")
        scripts.append((m.group(2), p, want))
    return entries, scripts


def template_for(size: str, base: dict):
    """`WxH` → 一个整张贴图一个 sprite 的 entry 模板（字段顺序 / 常量抄 base = abcard BLANK_max）。"""
    w, h = (int(x) for x in size.lower().split("x"))
    if w & (w - 1) or h & (h - 1):
        raise SystemExit(f"贴图尺寸要是 2 的幂：{size}")
    fields = dict(base["fields"])
    fields.update({"width": str(w), "height": str(h), "THTXSize": str(w * h * 4), "THTXWidth": str(w), "THTXHeight": str(h)})
    return {"idx": -1, "name": None, "keys": base["keys"], "fields": fields,
            "sprite": {"x": "0", "y": "0", "w": str(w), "h": str(h)}}


def write_ids_h(entries, scripts):
    lines = ["/* 由 assets/build_ability.py 生成 —— ability.anm 里追加的 sprite / 脚本号。别手改。 */",
             "#pragma once", ""]
    for name, _, eidx, sidx, _sz in entries:
        lines.append(f"#define CE_ANM_ABILITY_SPRITE_{name.upper():<20} {sidx:>4}   /* entry{eidx} */")
    for name, _, idx in scripts:
        lines.append(f"#define CE_ANM_ABILITY_SCRIPT_{name.upper():<20} {idx:>4}")
    IDS_H.write_text("\n".join(lines) + "\n")


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--verify-only", action="store_true")
    a = ap.parse_args()
    th, td, mapdir = L.find_tools()
    tools, anmmap = (th, td), mapdir / "v8.anmm"
    retail_dir, retail_text = L.retail("ability")
    retail_entries = L.parse_entries(retail_text)
    tpl = {e["name"]: e for e in L.parse_entries(L.retail(TEMPLATE[0])[1])}[TEMPLATE[1]]
    entries, scripts = plan(retail_text)
    mapping = {name: f"sprite{sidx}" for name, _, _, sidx, _sz in entries}
    tpl_of = {name: (template_for(sz, tpl) if sz else tpl) for name, _, _, _, sz in entries}

    if not a.verify_only:
        from PIL import Image
        L.fresh_dir(TEX)
        L.link_retail_textures(TEX, retail_dir, retail_entries)
        (TEX / "ability").mkdir(exist_ok=True)
        blocks = []
        for name, src, eidx, sidx, _sz in entries:
            t = tpl_of[name]
            want = (int(t["fields"]["THTXWidth"]), int(t["fields"]["THTXHeight"]))
            if not src.is_file():
                raise SystemExit(f"缺 {src}" + ("（零售贴图：先 python3 tooling/thtk/unpack.py th18.v1.00a 解包到 local/）" if "local/" in str(src) else ""))
            im = Image.open(src).convert("RGBA")
            if im.size != want:
                if im.width > want[0] or im.height > want[1]:
                    raise SystemExit(f"{src.name} 是 {im.size}，要 ≤ {want}")
                pad = Image.new("RGBA", want, (0, 0, 0, 0))            # 小图居中补到声明的 2 的幂尺寸（零售 pl01b2 是 96×256）
                pad.paste(im, ((want[0] - im.width) // 2, (want[1] - im.height) // 2))
                im = pad
            im.save(TEX / "ability" / f"{name}.png")
            blocks.append(L.make_entry(t, eidx, f"ability/{name}.png", sprite_idx=sidx))
        text = L.insert_entries(retail_text, blocks) if blocks else retail_text
        sblocks = []
        for name, p, idx in scripts:
            body = L.substitute(p.read_text(), mapping)
            if not body.lstrip().startswith(f"script script{idx} {{"):
                raise SystemExit(f"{p.name}：开头应为 `script script{idx} {{`")
            sblocks.append(body)
        if sblocks:
            text = L.append_scripts(text, sblocks)
        L.compile_anm(text, TEX, OUT_ANM, tools, anmmap)
        print(f"build: {OUT_ANM.relative_to(MOD)}（{OUT_ANM.stat().st_size:,} B）；零售 entry {len(retail_entries)} + {len(entries)}，"
              f"script {len(L.parse_scripts(retail_text))} + {len(scripts)}")

    _, n_e, n_s = L.verify_rebuilt(OUT_ANM, retail_text, retail_dir, tools, anmmap)
    if len(n_e) != len(retail_entries) + len(entries) or len(n_s) != len(L.parse_scripts(retail_text)) + len(scripts):
        raise SystemExit(f"entry {len(n_e)} / script {len(n_s)}，与计划不符")
    for name, _, eidx, sidx, _sz in entries:
        e, t = n_e[eidx], tpl_of[name]
        if e["name"] != f"ability/{name}.png" or e["fields"] != t["fields"] or e["sprite"] != t["sprite"] \
                or f"sprite{sidx}:" not in e["block"]:
            raise SystemExit(f"新 entry{eidx}（{name}）字段与模板不一致")
    write_ids_h(entries, scripts)
    print(f"verify: entry {len(n_e)}、script {len(n_s)}；零售 spec/贴图逐项一致；写 {IDS_H.relative_to(MOD)}\n")
    for name, _, eidx, sidx, sz in entries:
        print(f"  entry{eidx:<4} sprite{sidx:<5} {name}{'  ' + sz if sz else ''}")
    for name, _, idx in scripts:
        print(f"  script{idx:<12} {name}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
