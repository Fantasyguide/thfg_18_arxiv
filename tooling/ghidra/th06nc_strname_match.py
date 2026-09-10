#!/usr/bin/env python3
"""离线跑一遍「CC0 字面量 → TH06NC 函数名」的匹配（**不碰 Ghidra 工程锁**）。

只读三样东西：CC0 源码、`local/th06nc/th06nc.exe`、`local/th06nc/th06nc-funcs.json`
（函数边界，由 dump_funcs.py 导出）。PE 段表、字符串定位、RIP 相对 LEA 扫描都在本文件里，
不依赖 capstone。

    python3 tooling/ghidra/th06nc_strname_match.py            # 打印判决
    python3 tooling/ghidra/th06nc_strname_match.py --check    # 与 spec 对账，不一致就非零退出

判据见 th06nc_strname_spec 的模块 docstring。注意 (d) chain 回调结构签名
**不在这里**——那是人工复核，结论落在 spec 的备注与 REJECTED 里。
"""
import argparse
import bisect
import collections
import json
import os
import re
import struct
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
SRC = REPO / "local" / "vendor" / "th06-decomp" / "src"
EXE = REPO / "local" / "th06nc" / "th06nc.exe"
FUNCS = REPO / "local" / "th06nc" / "th06nc-funcs.json"

LITPAT = re.compile(r'"((?:[^"\\\n]|\\.)*)"')
# 函数定义头：可带类限定；结尾必须是 `{`（声明以 `;` 结尾，被 [^;]* 挡掉）
DEFPAT = re.compile(
    r'^[A-Za-z_~][\w:<>,\*&\s]*?\b(?:([A-Za-z_]\w*)::)?([A-Za-z_~]\w*)\s*\([^;]*?\)\s*(?:const\s*)?\{',
    re.M)
NOT_A_FUNCTION = {"if", "for", "while", "switch", "catch", "return", "else", "do"}


def strip_comments(text):
    """把注释换成等长空白（保持偏移），字符串字面量原样留下。"""
    out, i, n = [], 0, len(text)
    while i < n:
        c = text[i]
        if c == "/" and i + 1 < n and text[i + 1] == "/":
            j = text.find("\n", i)
            j = n if j < 0 else j
            out.append(" " * (j - i))
            i = j
        elif c == "/" and i + 1 < n and text[i + 1] == "*":
            j = text.find("*/", i + 2)
            j = n if j < 0 else j + 2
            out.append("".join(ch if ch == "\n" else " " for ch in text[i:j]))
            i = j
        elif c == '"':
            j = i + 1
            while j < n and text[j] != '"':
                j += 2 if text[j] == "\\" else 1
            j = min(j + 1, n)
            out.append(text[i:j])
            i = j
        else:
            out.append(c)
            i += 1
    return "".join(out)


def function_bodies(text):
    """[(start, end, owner)]；owner 形如 `Cls::meth` 或 `<free>name`。"""
    result = []
    for m in DEFPAT.finditer(text):
        cls, meth = m.group(1), m.group(2)
        if meth in NOT_A_FUNCTION:
            continue
        i, n, depth = m.end() - 1, len(text), 0
        while i < n:
            if text[i] == "{":
                depth += 1
            elif text[i] == "}":
                depth -= 1
                if depth == 0:
                    break
            elif text[i] == '"':
                i += 1
                while i < n and text[i] != '"':
                    i += 2 if text[i] == "\\" else 1
            i += 1
        result.append((m.end() - 1, i, "%s::%s" % (cls, meth) if cls else "<free>%s" % meth))
    return result


ESCAPES = {"n": 10, "t": 9, "r": 13, "0": 0, "\\": 92, '"': 34, "'": 39,
           "a": 7, "b": 8, "f": 12, "v": 11}


def unescape(s):
    """C 字面量 → cp932 字节。NC 原样保留了 ZUN 的 Shift-JIS，编码选错就全盘皆空。"""
    out, i = bytearray(), 0
    while i < len(s):
        c = s[i]
        if c == "\\" and i + 1 < len(s):
            nxt = s[i + 1]
            if nxt in ESCAPES:
                out.append(ESCAPES[nxt])
                i += 2
                continue
            if nxt == "x":
                j, hexs = i + 2, ""
                while j < len(s) and s[j] in "0123456789abcdefABCDEF" and len(hexs) < 2:
                    hexs += s[j]
                    j += 1
                out.append(int(hexs, 16))
                i = j
                continue
            out.append(ord(nxt))
            i += 2
            continue
        try:
            out += c.encode("cp932")
        except UnicodeEncodeError:
            return None
        i += 1
    return bytes(out)


def collect_cc0():
    """字面量字节 → 归属集合。i18n.tpl 的宏按**使用点**归属。"""
    owners = collections.defaultdict(set)
    tpl_text = (SRC / "i18n.tpl").read_text(encoding="utf-8", errors="replace")
    # i18n.tpl 定义 TH_JP_<SUFFIX> = "..."，使用点写的是 TH_<SUFFIX>
    # （TH_MAKE_LANG_STR(TH_LANG, _SUFFIX)，TH_LANG = TH_JP）。
    macro_lit = {}
    for m in re.finditer(r"^\s*#define\s+TH_JP_(\w+)\s+(.+)$", tpl_text, re.M):
        lits = LITPAT.findall(m.group(2))
        if len(lits) != 1:
            continue
        raw = unescape(lits[0])
        if raw:
            macro_lit["TH_" + m.group(1)] = raw

    macro_owner = collections.defaultdict(set)
    for name in sorted(os.listdir(SRC)):
        if not name.endswith((".cpp", ".tpl")):
            continue
        text = strip_comments((SRC / name).read_text(encoding="utf-8", errors="replace"))
        bodies = function_bodies(text)

        def enclosing(offset):
            best = None
            for start, end, owner in bodies:
                if start <= offset <= end and (best is None or end - start < best[1]):
                    best = (owner, end - start)
            return best[0] if best else "<data>"

        for m in LITPAT.finditer(text):
            raw = unescape(m.group(1))
            if raw is None or len(raw) < 5:
                continue
            owners[raw].add(enclosing(m.start()))
        if name.endswith(".cpp"):
            for macro in macro_lit:
                for m in re.finditer(r"\b" + macro + r"\b", text):
                    macro_owner[macro].add(enclosing(m.start()))

    for macro, raw in macro_lit.items():
        if raw in owners and owners[raw] == {"<data>"} and macro_owner.get(macro):
            owners[raw] = set(macro_owner[macro])
    return owners


def cc0_same_class_calls():
    """CC0 同类调用图：`Cls::a` -> {`Cls::b`, ...}。判据 (c) 用。"""
    bodies = []
    for name in sorted(os.listdir(SRC)):
        if not name.endswith(".cpp"):
            continue
        text = strip_comments((SRC / name).read_text(encoding="utf-8", errors="replace"))
        for start, end, owner in function_bodies(text):
            if not owner.startswith("<free>"):
                cls, meth = owner.split("::", 1)
                bodies.append((cls, meth, text[start:end]))
    by_class = collections.defaultdict(set)
    for cls, meth, _ in bodies:
        by_class[cls].add(meth)
    edges = collections.defaultdict(set)
    for cls, meth, body in bodies:
        for other in by_class[cls]:
            if other != meth and re.search(r"\b" + re.escape(other) + r"\s*\(", body):
                edges["%s::%s" % (cls, meth)].add("%s::%s" % (cls, other))
    return edges


def load_pe():
    data = EXE.read_bytes()
    pe = struct.unpack_from("<I", data, 0x3C)[0]
    if data[pe:pe + 4] != b"PE\0\0":
        raise SystemExit("not a PE: %s" % EXE)
    nsec = struct.unpack_from("<H", data, pe + 6)[0]
    optsz = struct.unpack_from("<H", data, pe + 20)[0]
    imagebase = struct.unpack_from("<Q", data, pe + 24 + 24)[0]
    sections = []
    for i in range(nsec):
        off = pe + 24 + optsz + i * 40
        name = data[off:off + 8].rstrip(b"\0").decode()
        vsz, va, rsz, ptr = struct.unpack_from("<IIII", data, off + 8)
        sections.append((name, va, vsz, ptr, rsz))
    return data, imagebase, sections


def run():
    owners = collect_cc0()
    edges = cc0_same_class_calls()
    data, imagebase, sections = load_pe()

    def off_to_va(off):
        for _n, va, _vsz, ptr, rsz in sections:
            if ptr <= off < ptr + rsz:
                return imagebase + va + (off - ptr)
        return None

    # 字面量 → exe 里唯一的 VA。必须是完整 C 字符串（前后都是 NUL），
    # 否则 "data/" 会命中 "data/eff01.anm" 的内部。
    lit_va = {}
    for raw in owners:
        hits, start = [], 0
        while True:
            pos = data.find(raw, start)
            if pos < 0:
                break
            start = pos + 1
            if pos > 0 and data[pos - 1] != 0:
                continue
            if pos + len(raw) >= len(data) or data[pos + len(raw)] != 0:
                continue
            va = off_to_va(pos)
            if va:
                hits.append(va)
        if len(hits) == 1:
            lit_va[raw] = hits[0]
    va_lit = {v: k for k, v in lit_va.items()}

    text_sec = next(s for s in sections if s[0] == ".text")
    _n, tva, _tvsz, tptr, trsz = text_sec
    blob, base = data[tptr:tptr + trsz], imagebase + tva
    refs = collections.defaultdict(set)
    i, n = 0, len(blob)
    while i < n - 7:
        c = blob[i]
        if c == 0x8D and (blob[i + 1] & 0xC7) == 0x05:
            tgt = base + i + 6 + struct.unpack_from("<i", blob, i + 2)[0]
            if tgt in va_lit:
                refs[va_lit[tgt]].add(base + i)
            i += 6
            continue
        if 0x40 <= c <= 0x4F and blob[i + 1] == 0x8D and (blob[i + 2] & 0xC7) == 0x05:
            tgt = base + i + 7 + struct.unpack_from("<i", blob, i + 3)[0]
            if tgt in va_lit:
                refs[va_lit[tgt]].add(base + i)
            i += 7
            continue
        i += 1

    entries = json.loads(FUNCS.read_text())
    ranges = sorted((int(f["addr"], 16), int(f["addr"], 16) + f["size"])
                    for f in entries if not f["external"])
    starts = [r[0] for r in ranges]

    def containing(va):
        k = bisect.bisect_right(starts, va) - 1
        if k < 0:
            return None
        s, e = ranges[k]
        return s if s <= va < e else None

    per_func = collections.defaultdict(set)
    for raw, sites in refs.items():
        for va in sites:
            fn = containing(va)
            if fn is not None:
                per_func[fn].add(raw)

    def reaches(node, targets):
        seen, stack = {node}, [node]
        while stack:
            for nxt in edges[stack.pop()]:
                if nxt not in seen:
                    seen.add(nxt)
                    stack.append(nxt)
        return targets <= seen

    def decide(tally):
        real = {o for o in tally if not o.startswith("<")}
        if any(o.startswith("<free>") for o in tally):
            return None, "CC0 side has a free-function literal -- conflict"
        if not real:
            return None, "file-scope data only"
        if len(real) == 1:
            only = next(iter(real))
            if tally[only] <= tally.get("<data>", 0):
                return None, "file-scope data outweighs it"
            return only, "unique"
        if len({o.split("::")[0] for o in real}) == 1:
            inliners = [o for o in real if reaches(o, real - {o})]
            if len(inliners) == 1:
                return inliners[0], "caller-closure"
        ranked = [(o, c) for o, c in tally.most_common() if not o.startswith("<")]
        if len(ranked) > 1 and ranked[0][1] == ranked[1][1]:
            return None, "tied on votes"
        return ranked[0][0], "weighted %d:%d" % (ranked[0][1], ranked[1][1] if len(ranked) > 1 else 0)

    picked, tallies = {}, {}
    for addr, lits in per_func.items():
        tally = collections.Counter()
        for raw in lits:
            if len(owners[raw]) == 1:
                tally[next(iter(owners[raw]))] += 1
        tallies[addr] = tally
        picked[addr] = decide(tally)

    claims = collections.defaultdict(list)
    for addr, (name, _why) in picked.items():
        if name:
            claims[name].append((tallies[addr][name], addr))

    accepted, rejected = {}, {}
    for addr, (name, why) in sorted(picked.items()):
        if name is None:
            rejected[addr] = why
            continue
        rivals = sorted(claims[name], reverse=True)
        if len(rivals) > 1 and rivals[0][0] == rivals[1][0]:
            rejected[addr] = "%s tied between %s" % (name, [hex(r[1]) for r in rivals])
        elif rivals[0][1] != addr:
            rejected[addr] = "%s claimed more strongly by %s" % (name, hex(rivals[0][1]))
        else:
            accepted[addr] = (name, why, tallies[addr][name])
    stats = {"literals": len(owners), "in_exe": len(lit_va), "lea_reached": len(refs),
             "functions_touched": len(per_func)}
    return accepted, rejected, stats


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true",
                        help="与 th06nc_strname_spec 对账，不一致就非零退出")
    args = parser.parse_args()

    for path in (SRC, EXE, FUNCS):
        if not path.exists():
            raise SystemExit("missing (gitignored, see local/README.md): %s" % path)

    accepted, rejected, stats = run()
    print("[cc0] literals >=5 bytes: %(literals)d" % stats)
    print("[nc]  byte-unique in exe: %(in_exe)d   reached by a RIP-relative LEA: %(lea_reached)d"
          % stats)
    print("[nc]  functions touched: %(functions_touched)d" % stats)
    print("\n=== accepted %d ===" % len(accepted))
    for addr, (name, why, votes) in sorted(accepted.items()):
        print("  0x%09x  %-40s n=%-3d [%s]" % (addr, name, votes, why))
    print("\n=== rejected %d ===" % len(rejected))
    for addr, why in sorted(rejected.items()):
        print("  0x%09x  %s" % (addr, why))

    if not args.check:
        return
    sys.path.insert(0, str(Path(__file__).resolve().parent))
    import th06nc_strname_spec as spec

    problems = []
    flat = {a: n for a, n, _c, _m in spec.STR_NAMES}
    for addr, (name, _why, _v) in accepted.items():
        flat_name = name.replace("::", "__")
        if addr in flat:
            if flat[addr] != flat_name:
                problems.append("0x%x: spec says %s, matcher says %s" % (addr, flat[addr], flat_name))
        elif addr in spec.REPRODUCED:
            if spec.REPRODUCED[addr] != flat_name:
                problems.append("0x%x: REPRODUCED says %s, matcher says %s"
                                % (addr, spec.REPRODUCED[addr], flat_name))
        elif addr not in spec.REJECTED:
            problems.append("0x%x %s: matcher accepts it but the spec never mentions it" % (addr, flat_name))
    # 判据 (d) 拆开的平票：spec 收了，匹配器自己判不了 —— 那是人工复核的活。
    for addr in spec.TIEBREAKS:
        if addr in accepted:
            problems.append("0x%x is listed as a manual tiebreak but the matcher decided it "
                            "on its own -- update the note" % addr)
        elif addr not in rejected:
            problems.append("0x%x is listed as a manual tiebreak but the matcher never saw it" % addr)
    print("\n[check] %s" % ("OK" if not problems else "%d problem(s)" % len(problems)))
    for p in problems:
        print("  - %s" % p)
    if problems:
        raise SystemExit(1)


if __name__ == "__main__":
    main()
