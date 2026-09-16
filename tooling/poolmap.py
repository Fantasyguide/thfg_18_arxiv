#!/usr/bin/env python3
"""从游戏日志里的**池页 hex** 还原 thcrap 的断点跳板池（版本无关，纯离线）。

数据来源：`mods/th18.v1.00a/card-expand/native/sdk.c` 的 `ce_mem_map_dump()`
—— 它会把 `0x03c00000..0x03e00000` 里 `PAGE_EXECUTE` 的页**整页打 hex**，
日志里就是一组形如：

    memmap 03d20000..03d21000 COMMIT  PRIVATE prot=0x000010 首 8 字节 = 89 44 24 20 6a 2c e9 f9
      page 03d20000+000: 89 44 24 20 6a 2c e9 f9 e9 71 fc cc cc cc cc cc

每格 `0x10` 字节 = 被偷的原始字节 + `e9 rel32`（跳回原站点）+ `cc` 填充；**1 条声明的断点 = 1 格**。

用法
----
    python3 tooling/poolmap.py <日志> [--text-start 0x400000 --text-end 0x4d0000]
                               [--declared N] [--dump-page 0x03d20000]

判读：`格数` ≈ 该次运行**实际安装的断点条数**（可与 `--declared` 对照）；
`回跳目标` 应全部落在游戏的 `.text`（落在别处 = 那一格不是跳板，或格内容被误读）。
"""
import argparse
import collections
import re

PAGE_RE = re.compile(r"^\s*page ([0-9a-f]{8})\+([0-9a-f]{3}):\s*((?:[0-9a-f]{2} )+)\s*$")
MEM_RE = re.compile(r"^memmap ([0-9a-f]{8})\.\.([0-9a-f]{8})\s+(\S+)\s+(\S+)\s+prot=([0-9a-fx]+)")


def parse(path):
    """→ (pages{base: bytes}, memmap[(base,size,state,type,prot)])"""
    pages = collections.defaultdict(dict)
    mm = []
    with open(path, encoding="utf-8", errors="replace") as f:
        for line in f:
            m = PAGE_RE.match(line)
            if m:
                pages[int(m.group(1), 16)][int(m.group(2), 16)] = bytes.fromhex(
                    m.group(3).replace(" ", ""))
                continue
            m = MEM_RE.match(line)
            if m:
                mm.append((int(m.group(1), 16), int(m.group(2), 16) - int(m.group(1), 16),
                           m.group(3), m.group(4), int(m.group(5), 16)))
    out = {}
    for base, chunks in pages.items():
        buf = bytearray(max(chunks) + 16)
        for off, b in chunks.items():
            buf[off:off + len(b)] = b
        out[base] = bytes(buf)
    return out, mm


def scan(page_base, buf, text_lo, text_hi):
    """逐 0x10 格找跳板；返回 (跳板列表, 首个未写格偏移)。"""
    trams, first_free = [], None
    for i in range(0, len(buf) - 0x10 + 1, 0x10):
        cell = buf[i:i + 0x10]
        j = cell.find(0xe9)
        tgt = None
        if j >= 0 and j + 5 <= len(cell):
            rel = int.from_bytes(cell[j + 1:j + 5], "little", signed=True)
            tgt = page_base + i + j + 5 + rel
        ok = tgt is not None and text_lo <= tgt < text_hi
        if ok:
            trams.append((i, tgt))
        elif first_free is None and not any(cell):
            first_free = i
    return trams, first_free


def main():
    ap = argparse.ArgumentParser(description="还原 thcrap 断点跳板池（离线）")
    ap.add_argument("log")
    ap.add_argument("--text-start", default="0x400000")
    ap.add_argument("--text-end", default="0x4d0000",
                    help="游戏 .text 的范围（判回跳目标是否合法）")
    ap.add_argument("--declared", type=int, default=0, help="该次运行声明的断点条数（对照用）")
    ap.add_argument("--dump-page", default="", help="把这一页（hex 地址）逐格打出来")
    a = ap.parse_args()
    lo, hi = int(a.text_start, 16), int(a.text_end, 16)

    pages, mm = parse(a.log)
    print(f"日志：{a.log}")
    ex = [m for m in mm if (m[4] & 0xff) == 0x10]
    print(f"内存图段落 {len(mm)} 条，其中 PAGE_EXECUTE（会被整页 dump 的）{len(ex)} 条")
    if not pages:
        print("✗ 日志里没有 `page …` 行 —— 该次运行没跑到 ce_mem_map_dump（或日志被截断）")
        return 1
    total = 0
    for base in sorted(pages):
        buf = pages[base]
        trams, first_free = scan(base, buf, lo, hi)
        total += len(trams)
        slots = len(buf) // 0x10
        print(f"\n页 {base:#010x}：dump {len(buf):#x} 字节 = {slots} 格（整页应为 256 格）")
        print(f"  跳板（e9 且回跳落在 .text）= {len(trams)}")
        print(f"  首个全零格 = {'—' if first_free is None else hex(first_free)}")
        if trams:
            tg = [t for _, t in trams]
            print(f"  回跳目标范围 = {min(tg):#x} .. {max(tg):#x}")
            uniq = len(set(tg))
            print(f"  去重后 {uniq} 个目标" + ("（有条目重复 ⇒ 怀疑重复安装）" if uniq < len(tg) else ""))
        if a.dump_page and int(a.dump_page, 16) == base:
            for off in range(0, len(buf), 0x10):
                cell = buf[off:off + 0x10]
                mark = "跳板" if any(off == t for t, _ in trams) else "    "
                print(f"    +{off:03x} {mark} {cell.hex(' ')}")
    print(f"\n合计跳板 {total} 格", end="")
    if a.declared:
        d = "相等 ✓" if total == a.declared else f"≠ 声明 {a.declared}（差 {total - a.declared:+d}）"
        print(f"；与声明条数对照：{d}")
    else:
        print("（用 --declared N 对照该次运行声明的断点条数）")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
