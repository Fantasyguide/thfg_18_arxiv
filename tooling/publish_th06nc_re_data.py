#!/usr/bin/env python3
"""把 TH06NC 的符号成果导成一个可公开发布的 `th06nc-re-data` 仓库内容。

**为什么单向**：`games/th06nc/symbols.json` 是我们那层的**唯一源**（双向、入库，
见 `tooling/ghidra/README.md` 的「两层符号」）。公开仓库是**下游**，只接收不回流——
和 `renkolab 开发 / th18_modkit 发布` 同一套路。所以本脚本只写目标目录，
永远不读它、不往回改 renkolab 里的任何东西。

**为什么值得发**：TH06NC 目前社区零覆盖——truth 的 `v0.anmm` 只到 opcode 31、
ExpHP 的 `th-re-data` 没有 NC、`th06-re` 是原版。我们这批是第一份。

**不发什么**（发布前的硬规矩）：
- 不含任何游戏字节：没有 exe、没有 DAT、没有贴图，只有地址 + 我们起的名字 + 我们写的注释。
- 不转发上游：ExpHP 的 `th-re-data` 与 `th06-re` 的 XML 都没有 LICENSE，
  它们的名字不进这里。TH06NC 那层本来也没有上游可借。
- 不抄 truth 的 mapfile：`truth/map/README.md` 写的是「personal use」，
  所以 `.anmm` 只发**我们自己查出来的那几条**，做叠加层用。

用法：

    python3 tooling/publish_th06nc_re_data.py <输出目录>
    python3 tooling/publish_th06nc_re_data.py <输出目录> --check   # 只比对，不写
"""
import argparse
import json
import re
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
SYMBOLS = REPO / "games/th06nc/symbols.json"
SPEC_DIR = REPO / "tooling/ghidra"

EXPECTED_MD5 = "b0cc28ec904e9efce2439ad857c938b5"

# Ghidra 的 PE 导入分析器在 IAT 上写的桩注释，形如 "405  GetObjectA  <<not bound>>"。
# 它们不是我们的笔记，别往公开仓库里倒。
# （`symbols.py` 那边不滤是为了不让既有版本的 symbols.json 报假漂移，
#   所以这道过滤放在发布这一侧。）
IAT_STUB = re.compile(r"^\d+\s+\S+\s+<<not bound>>$")

LICENSE_TEXT = """CC0 1.0 Universal

The person who associated a work with this deed has dedicated the work to the
public domain by waiving all of their rights to the work worldwide under
copyright law, including all related and neighboring rights, to the extent
allowed by law.

You can copy, modify, distribute and perform the work, even for commercial
purposes, all without asking permission.

Full text: https://creativecommons.org/publicdomain/zero/1.0/legalcode

----

This dedication covers the symbol names, addresses, annotations and mapfiles in
this repository -- that is, our own analysis notes. It does not and cannot cover
the game itself, which remains the copyright of its authors. No game code, game
data or game assets are included here.
"""


def load_spec(name):
    import importlib.util

    path = SPEC_DIR / f"{name}.py"
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def build(symbols, anm_spec, ecl_spec):
    """symbols.json + 两份 spec -> {相对路径: 文本}。纯函数，好测。"""
    if symbols.get("exe_md5", "").lower() != EXPECTED_MD5:
        raise SystemExit(
            "symbols.json 的 exe_md5 是 %r，不是预期的 %r —— 样本换了，先确认再发"
            % (symbols.get("exe_md5"), EXPECTED_MD5)
        )

    comments = [c for c in symbols.get("comments", []) if not IAT_STUB.match(c["text"])]
    by_addr = {}
    for c in comments:
        # 一个地址可能有多种 kind 的注释；plate 优先当作「这一项的说明」
        slot = by_addr.setdefault(c["addr"], {})
        slot[c["kind"]] = c["text"]

    def note(addr):
        slot = by_addr.get(addr, {})
        return slot.get("plate") or slot.get("pre") or slot.get("eol")

    funcs = [
        {"addr": r["addr"], "name": r["name"], "comment": note(r["addr"])}
        for r in sorted(symbols.get("funcs", []), key=lambda r: int(r["addr"], 16))
        if r.get("name")
    ]
    statics = [
        {"addr": r["addr"], "name": r["name"], "type": r.get("type"), "comment": note(r["addr"])}
        for r in sorted(symbols.get("statics", []), key=lambda r: int(r["addr"], 16))
    ]
    labels = [
        {"addr": r["addr"], "name": r["name"], "comment": note(r["addr"])}
        for r in sorted(symbols.get("labels", []), key=lambda r: int(r["addr"], 16))
    ]

    used = {r["addr"] for r in funcs} | {r["addr"] for r in statics} | {r["addr"] for r in labels}
    loose = [c for c in comments if c["addr"] not in used]

    def dump(obj):
        return json.dumps(obj, ensure_ascii=False, indent=1, sort_keys=False) + "\n"

    out = {
        "data/th06nc/funcs.json": dump(funcs),
        "data/th06nc/statics.json": dump(statics),
        "data/th06nc/labels.json": dump(labels),
        "data/th06nc/comments.json": dump(
            sorted(loose, key=lambda c: (int(c["addr"], 16), c["kind"]))
        ),
        "mapfiles/th06nc.anmm": anmm(anm_spec),
        "mapfiles/th06nc.eclm": eclm(ecl_spec),
        "LICENSE": LICENSE_TEXT,
        "README.md": readme(symbols, anm_spec, funcs, statics, labels, loose),
    }
    return out


def anmm(spec):
    lines = [
        "!anmmap",
        "",
        "# TH06NC (Touhou Koumakyou: New Classic) -- ANM v0 additions.",
        "#",
        "# THIS IS AN OVERLAY, NOT A COMPLETE MAPFILE. It intentionally contains only the",
        "# opcodes TH06NC added on top of TH06's 0..31, because those 32 are documented",
        "# elsewhere by the community and we did not re-derive them. Load it alongside",
        "# whatever v0 mapfile you already use.",
        "#",
        "# All four are zero-argument anchor setters writing a 3-bit field in vm flags",
        "# bits 8..10. TH06 used bit8='x anchored left' + bit9='y anchored top' as two",
        "# independent switches; NC replaced that with a value-compared enum, so the bit",
        "# patterns are NOT interchangeable with TH06 even though opcode 23 still means",
        "# top-left. There is no bottom row: the grid is 3x2, not 3x3.",
        "",
        "!ins_names",
    ]
    for op in spec.NC_ONLY_OPCODES:
        _addr, _and, _or, name, field = spec.ANCHOR_OPCODES[op]
        x, y = spec.ANCHOR_FIELD[field]
        lines.append("%d %s  # anchor field %d = (x:%s, y:%s)" % (op, name, field, x, y))
    lines += ["", "!gvar_names", "# TH06NC ANM has no variables, same as TH06.", ""]
    return "\n".join(lines)


def _brief(text, limit=90):
    """mapfile 的行内注释取第一句就够；完整论证在 README 与 renkolab 的文档里。"""
    first = text.split(". ")[0].rstrip(".")
    # 用 ASCII 省略号：mapfile 产物必须全 ASCII（见发布测试）
    return first if len(first) <= limit else first[: limit - 3].rstrip() + "..."


def eclm(spec):
    lines = [
        "!eclmap",
        "",
        "# TH06NC -- ECL additions over the TH06 instruction set.",
        "#",
        "# OVERLAY, same caveat as the .anmm: only what we derived first-hand.",
        "#",
        "# NC raised the opcode ceiling from 135 to 201, but only 200 and 201 do anything:",
        "# 136..199 all land on the dispatch default, and so does the pre-existing 127",
        "# (DEBUGWATCH) -- which TH06 leaves unimplemented too. Within 1..135 the sets of",
        "# implemented opcodes are identical between the two builds, in both directions.",
        "#",
        "# The NAMES below are ours and provisional. The semantics are not: 200's math and",
        "# 201's dead aim_mode are both verified at byte level. See the repo README.",
        "",
        "!ins_names",
    ]
    for op, (name, why) in sorted(spec.NC_ECL_DELTAS.items()):
        lines.append("%d %s  # %s" % (op, name, _brief(why)))
    lines += ["", "!timeline_ins_names"]
    for op, (name, why) in sorted(spec.NC_TIMELINE_DELTAS.items()):
        lines.append("%d %s  # %s" % (op, name, _brief(why)))
    lines.append("")
    return "\n".join(lines)


def readme(symbols, anm_spec, funcs, statics, labels, loose):
    rows = []
    for op in anm_spec.NC_ONLY_OPCODES:
        _a, _n, _o, name, field = anm_spec.ANCHOR_OPCODES[op]
        x, y = anm_spec.ANCHOR_FIELD[field]
        rows.append("| %d | `%s` | %d | %s | %s |" % (op, name, field, x, y))
    anchor_table = "\n".join(rows)

    return f"""# th06nc-re-data

Reverse-engineering symbols for **Touhou Koumakyou: New Classic** (the 2026 Steam
remake of Embodiment of Scarlet Devil), in the shape of ExpHP's
[`th-re-data`](https://github.com/exphp-share/th-re-data).

Nobody had published anything for this build: truth's `v0.anmm` stops at ANM
opcode 31, `th-re-data` has no NC entry, and `th06-re` targets the 2002 original.
This is a first pass.

**Sample**: `th06nc.exe`, MD5 `{symbols['exe_md5']}` (x86-64, imagebase
`0x140000000`). Addresses are meaningless against any other build — check the
hash first.

## What's here

| Path | Contents |
| --- | --- |
| `data/th06nc/funcs.json` | {len(funcs)} named functions |
| `data/th06nc/statics.json` | {len(statics)} named globals |
| `data/th06nc/labels.json` | {len(labels)} named code labels, incl. all 120 ECL opcode handlers |
| `data/th06nc/comments.json` | {len(loose)} annotations not attached to the above |
| `mapfiles/th06nc.anmm` | the ANM opcodes NC added (overlay) |
| `mapfiles/th06nc.eclm` | the ECL opcodes NC added (overlay) |

`funcs.json` / `statics.json` use `th-re-data`'s schema verbatim. `labels.json`
deviates: `th-re-data` stores `{{group: [[addr, suffix], ...]}}` and builds names
from the pair, while ours are standalone names, so it is a flat
`[{{addr, name, comment}}]` list instead.

## The headline finding: four new ANM opcodes

TH06's ANM VM dispatches opcodes 0..31. NC's dispatches **0..35** — the bound is
a literal `CMP DL,0x23 / JA` at `0x140006a50`, and the jump table at
`0x140007494` has 36 entries. Scanning all 125 shipped `.anm` files turns up no
opcode >= 36, so the range is exactly 0..35.

All four additions are zero-argument anchor setters. Each is a two-instruction
`AND`/`OR` pair on the VM flags word at `vm+0xc4`, writing a 3-bit field at bits
8..10. Every one of those bits is either force-cleared by the AND or force-set by
the OR, so they are absolute assignments, not toggles.

| opcode | provisional name | field | x align | y align |
| --- | --- | --- | --- | --- |
{anchor_table}

For reference, the pre-existing opcode 23 (`anchorTopLeft`) sets field 4, and the
default (no opcode) is field 0 = centered on both axes. **There is no bottom
row** — the grid is 3x2, not the 3x3 that modern ANM's parameterised `anchor`
instruction gives you.

### Don't carry TH06's bit patterns over

TH06 encodes the anchor as two independent switches: bit 8 = "x anchored left",
bit 9 = "y anchored top", and its opcode 23 is a plain `OR flags, 0x300`. NC
turned the same three bits into an enum compared by value, and rewrote opcode 23
as `AND ~0x300` + `OR 0x400`. So opcode 23 still means top-left in both, but the
bit pattern `0x300` means *top-left* in TH06 and *top-center* in NC. Opcode-level
semantics carry over; bit patterns do not.

Opcode 33 is implemented but unused in the shipped data — as are TH06's own
opcodes 6 (`nop`) and 8 (`flipY`), so that is not a reason to doubt it.

## The ECL side: one real addition, one dead alias

NC's ECL dispatch takes `opcode - 1`, bounds it with `CMP EAX,0xc8`, and goes
through a byte map at `0x140028f5c` into a jump table at `0x140028d78` — so it
accepts opcodes **1..201**, against TH06's **1..135** (a direct 135-entry table
at `0x40a9a4`, no byte map).

That extra range is mostly empty. Decoding both tables entry by entry:

- **136..199 all land on the dispatch default.** So do TH06's and NC's opcode
  127 (`DEBUGWATCH`) — the original leaves it unimplemented too.
- Within 1..135 the implemented/unimplemented sets are **identical in both
  directions** — the diff is empty each way.
- TH06 has 120 distinct handlers, NC has 121. The one extra is opcode 200's.

`labels.json` names **all 120 real handlers**, as `ECL_OP_<opcode>_<NAME>`, with a
comment on each listing every opcode that reaches it. The mapping is decoded from
the binary's own byte map and jump table rather than transcribed, so it cannot
drift from the sample. Sharing is informative in itself: opcodes 13/20, 14/21,
15/22, 16/23 and 17/24 pair the integer and float forms of add/sub/mul/div/mod
onto one handler each, 4/5 share, 85/86 share, and 67..75 share with 201.

**Opcode 200** is a genuine new instruction: a polar form of TH06's
`SHOOTOFFSET`, computing `shoot_offset = (cos(a)*r, sin(a)*r, 0)` at
`0x140026a41` and writing `enemy+0xc5c`.

**Opcode 201 is not a new bullet pattern.** It shares jump-table slot 60 with
opcodes 67..75, and that handler stores `aim_mode = opcode - 67` — so 134 for
opcode 201. The consumer, `0x14000f870`, switches on `bullet_props+0x30` with
cases 0..8 and **no default**, leaving the angle at its pre-switch `0.0`.
Searching the whole binary for a comparison against 134 turns up 18 hits, all
unrelated loop and array bounds. Nothing special-cases it.

Caveat: that is a static result. Opcode 201 appears 13 times in the shipped
scripts, which is a lot of uses for an instruction that looks degenerate, so it
has not been confirmed on a running game.

## Confidence

Names are ours and provisional where marked. The addresses, dispatch bound,
instruction bytes and file-side statistics are first-hand off the binary. The
*motivation* for the new opcodes is not established: they appear almost only in
the localized `text*.anm` files, whose entries are runtime-created blank text
slots, but neither TH06 nor NC resizes a sprite to the measured text width, so
the obvious "because translations have different widths" story does not hold as
stated.

## Where the names come from

The ECL opcode names (`JUMPDEC`, `BULLETFANAIMED`, `SHOOTOFFSET`, …) are taken
verbatim from the `EclRawInstrOpcode` enum in
[GensokyoClub/th06](https://github.com/GensokyoClub/th06), which is released
under **CC0** — so they can be reused and redistributed freely. Our copy is
checked against that header on every test run, so it cannot silently drift.

Everything else — the addresses, the opcode-to-handler mapping, the anchor
analysis, the annotations — is ours.

## What is NOT here

No game code, no game data, no assets — only addresses, names we invented, and
notes we wrote. Nothing is forwarded from upstream projects: ExpHP's
`th-re-data` and the `th06-re` XML both ship without a license, and truth's
mapfiles are marked personal-use, so none of their content appears here. The
mapfiles are deliberately overlays containing only what we derived ourselves.

## License

[CC0-1.0](LICENSE). ExpHP's repo has no license, which makes it awkward to build
on; we would rather not repeat that.
"""


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("out", help="目标仓库目录")
    parser.add_argument("--check", action="store_true", help="只比对是否有差异，不写文件")
    args = parser.parse_args()

    symbols = json.loads(SYMBOLS.read_text(encoding="utf-8"))
    files = build(symbols, load_spec("th06nc_anm_spec"), load_spec("th06nc_ecl_spec"))

    out = Path(args.out)
    changed = []
    for rel, text in sorted(files.items()):
        path = out / rel
        old = path.read_text(encoding="utf-8") if path.is_file() else None
        if old == text:
            continue
        changed.append(rel)
        if not args.check:
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(text, encoding="utf-8")

    verb = "有差异" if args.check else "已写"
    if changed:
        print("[publish] %s %d 个文件 -> %s" % (verb, len(changed), out))
        for rel in changed:
            print("    " + rel)
    else:
        print("[publish] 无差异 -> %s" % out)
    if args.check and changed:
        sys.exit(1)


if __name__ == "__main__":
    main()
