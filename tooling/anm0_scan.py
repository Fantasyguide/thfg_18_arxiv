"""Independent ANM v0 (TH06 / TH06NC) scanner: opcode histogram + argument sizes.

Reimplements the container walk from thtk's ``anm_header06_t`` / ``anm_instr0_t``
and thanm's script-bounds rules, deliberately WITHOUT thanm's per-opcode format
table -- that table is exactly what fails on the opcodes TH06NC added, so using
thanm to count them would be circular.

Usage:  python3 tooling/anm0_scan.py <dir containing .anm files>

Reports every opcode with its observed argument-byte sizes, and lists where the
post-TH06 opcodes (>= 32) occur. See engine/anm/th06nc/01-vm-and-anchor-opcodes.md.
"""
import struct, sys, os, collections

HDR = "<IIIIIIIIIIIIIHBBIHH"
HDR_SIZE = struct.calcsize(HDR)
assert HDR_SIZE == 0x40, HDR_SIZE

def parse_entry(d, base):
    f = struct.unpack_from(HDR, d, base)
    (sprites, scripts, _slot, w, h, fmt, _ck, nameoff, _x, y, version,
     _mp, thtxoff, hasdata, _lrs, _jq, nextoff, _wm, _hm) = f
    return dict(sprites=sprites, scripts=scripts, w=w, h=h, version=version,
                nameoff=nameoff, thtxoff=thtxoff, nextoff=nextoff)

def scan_file(path):
    d = open(path, 'rb').read()
    out = []          # (entry_i, script_id, opcode, arglen, time)
    base = 0
    entry_i = 0
    while True:
        e = parse_entry(d, base)
        if e['version'] != 0:
            raise ValueError(f"{path}: entry {entry_i} version={e['version']} (expected 0)")
        soff = base + HDR_SIZE + e['sprites'] * 4
        offs = [struct.unpack_from("<iI", d, soff + i * 8) for i in range(e['scripts'])]
        for s, (sid, off) in enumerate(offs):
            if s < e['scripts'] - 1:
                limit = base + offs[s + 1][1]
            elif e['thtxoff']:
                limit = base + e['thtxoff']
            elif e['nextoff']:
                limit = base + e['nextoff']
            else:
                limit = len(d)
            p = base + off
            while True:
                if p + 4 > limit:
                    break
                time, typ, ln = struct.unpack_from("<hBB", d, p)
                if p + 4 + ln > limit:
                    break
                if typ == 0 and time == 0:
                    break
                out.append((entry_i, sid, typ, ln, time))
                p += 4 + ln
        if not e['nextoff']:
            break
        base += e['nextoff']
        entry_i += 1
    return out

def main(root):
    paths = []
    for dp, _dn, fn in os.walk(root):
        for f in fn:
            if f.endswith('.anm'):
                paths.append(os.path.join(dp, f))
    paths.sort()
    hist = collections.Counter()
    arglens = collections.defaultdict(set)
    where = collections.defaultdict(list)
    for p in paths:
        try:
            recs = scan_file(p)
        except Exception as ex:
            print(f"!! {p}: {ex}")
            continue
        for entry_i, sid, typ, ln, _t in recs:
            hist[typ] += 1
            arglens[typ].add(ln)
            if typ >= 32:
                where[typ].append((os.path.relpath(p, root), entry_i, sid))
    print(f"scanned {len(paths)} .anm files\n")
    print(f"{'op':>4} {'count':>7}  arg-bytes")
    for op in sorted(hist):
        mark = '   <-- NC-only' if op >= 32 else ''
        print(f"{op:>4} {hist[op]:>7}  {sorted(arglens[op])}{mark}")
    print("\n=== opcode >= 32 的出现位置（文件 / entry / script id） ===")
    for op in sorted(where):
        print(f"\n-- opcode {op}: {len(where[op])} 处")
        agg = collections.Counter(x[0] for x in where[op])
        for f, n in sorted(agg.items()):
            print(f"     {n:>3}x  {f}")

if __name__ == "__main__":
    if len(sys.argv) != 2:
        raise SystemExit(__doc__)
    main(sys.argv[1])
