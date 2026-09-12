#!/usr/bin/env python3
"""从 TH08 v1.00d 的 `th08.dat` 里解条目 —— **THTK 的 thdat 走不通，本脚本是替代品**。

## 为什么需要它

`thdat -x 8 th08.dat` 对 **每一个** 条目都报 `th08_read: incorrect entry magic` 并写出 0 字节产物
（`.anm` / `.sht` / `.ecl` 全中招；`thdat -l` 只走归档索引所以列表正常）。
根因一手（`local/vendor/thtk/thtk/thdat08.c:174-186`）：`th08_read` 在 LZSS 解压之后**强制要求条目
以 `"edz" + 类型字节` 开头**（源码自己写了 `FIXME: ZUN returns contents of raw_entry if magic or
type is incorrect`）。**TH08 v1.00d 的 `th08.dat` 里所有条目都没有这层 `edz` 包装** ——
解压出来就是文件本体（`ply00a.sht` 开头 `00 00 06 00`）。

## 算法（TH08 dat 格式）

1. 头 16 字节 = `"PBGZ"` + 12 字节密文。`th_decrypt(data, size=12, key=0x1b, step=0x37,
   block=12, limit=0x400)` 解出 `count / offset / size`，各自再减去魔数
   `123456 / 345678 / 567891`。
   ⚠ **坑**：`thdat08.c:71` 传的 block 是 `sizeof(header)-4 = 12`，**不是** `0x40`
   （照 `crypt_params` 写成 0x40 时整块解错、count 变成 19 亿）。
2. `seek(offset)`，读 `filesize-offset` 字节 → `th_decrypt(..., 0x3e, 0x9b, 0x80, 0x400)`
   → `unlzss(..., size)` 得索引区（v1.00d = **318** 条）。
3. 索引区 = `{ C 串名; uint32 offset; uint32 size; uint32 extra }` 循环；
   `zsize = next.offset - offset`（最后一条 = `header.offset - prev.offset`）。
4. 每条：`unlzss(dat[offset : offset+zsize], size)` → 若开头是 `edz`+类型字节则再按
   `EDZ_PARAMS` 解一层（本数据集里**没有**这种条目，留着只为对付别的 build），否则**就是文件本体**。

## 用法

```bash
python3 tooling/thtk/extract_th08.py local/th08.v1.00d/th08.dat local/th08.v1.00d/dat          # 全解
python3 tooling/thtk/extract_th08.py <dat> <outdir> ecldata1.ecl player00.anm                  # 挑着解
python3 tooling/thtk/extract_th08.py <dat> <outdir> --list                                     # 只列清单
```

## 一一手依据

- 复现纪律与自证（8 个 `ply*.sht` 的解出结果全部能被 `th08:0x44fb70` 的装载逻辑无损解析）：
  [`local/_scratch/research-th08-youkai-shot-vs-power.md`](../../local/_scratch/research-th08-youkai-shot-vs-power.md) 附录 A。
- `th_decrypt` 逐块「首字节 ^key、次字节 ^(key+step*increment)」的写法照 `thtk/thcrypt.c`。
"""

from __future__ import annotations

import os
import struct
import sys

# `thdat08.c:39` 的 crypt_params：类型字节 → (key, step, block, limit)
EDZ_PARAMS = {
    "M": (0x1B, 0x37, 0x40, 0x2000),
    "T": (0x51, 0xE9, 0x40, 0x3000),
    "A": (0xC1, 0x51, 0x1400, 0x2000),
    "J": (0x03, 0x19, 0x1400, 0x7800),
    "E": (0xAB, 0xCD, 0x200, 0x1000),
    "W": (0x12, 0x34, 0x400, 0x2800),
    "-": (0x35, 0x97, 0x80, 0x2800),
    "*": (0x99, 0x37, 0x400, 0x1000),
}

HEADER_KEY = (0x1B, 0x37, 12, 0x400)  # ⚠ block=12（= sizeof(header)-4），不是 0x40
INDEX_KEY = (0x3E, 0x9B, 0x80, 0x400)


def th_decrypt(data: bytes, size: int, key: int, step: int, block: int, limit: int) -> bytes:
    """`thtk/thcrypt.c` 的 `th_decrypt`：按块交换相邻字节再异或滚动密钥。"""
    data = bytearray(data)
    increment = (block >> 1) + (block & 1)
    if size < block >> 2:
        size = 0
    else:
        size -= (1 if (size % block < block >> 2) else 0) * (size % block) + size % 2
    if limit % block:
        limit = limit + (block - (limit % block))
    end = min(size, limit)
    pos = 0
    while pos < end:
        blk = block
        if end - pos < blk:
            blk = end - pos
            increment = (blk >> 1) + (blk & 1)
        temp = bytearray(blk)
        i = pos
        out = blk - 1
        while out > 0:
            temp[out] = data[i] ^ key
            out -= 1
            temp[out] = data[i + increment] ^ ((key + step * increment) & 0xFF)
            out -= 1
            i += 1
            key = (key + step) & 0xFF
        if blk & 1:
            temp[out] = data[i] ^ key
            key = (key + step) & 0xFF
        key = (key + step * increment) & 0xFF
        data[pos : pos + blk] = temp
        pos += blk
    return bytes(data)


class Bitstream:
    """MSB-first 位流（`thtk/bits.c` 的 `bitstream_read`）。"""

    def __init__(self, data: bytes) -> None:
        self.d = data
        self.i = 0
        self.b = 0
        self.n = 0

    def read(self, bits: int) -> int:
        while bits > self.n:
            c = self.d[self.i] if self.i < len(self.d) else 0
            self.i += 1
            self.b = (self.b << 8) | c
            self.n += 8
        self.n -= bits
        v = (self.b >> self.n) & ((1 << bits) - 1)
        self.b &= (1 << self.n) - 1
        return v


def unlzss(src: bytes, outsize: int) -> bytes:
    """`LZSS_DICTSIZE=0x2000`、`MIN_MATCH=3`、位流 MSB-first。"""
    bs = Bitstream(src)
    dic = bytearray(0x2000)
    dh = 1
    out = bytearray()
    while len(out) < outsize:
        if bs.read(1):
            c = bs.read(8)
            out.append(c)
            dic[dh] = c
            dh = (dh + 1) & 0x1FFF
        else:
            mo = bs.read(13)
            if not mo:
                break
            ml = bs.read(4) + 3
            for i in range(ml):
                c = dic[(mo + i) & 0x1FFF]
                out.append(c)
                dic[dh] = c
                dh = (dh + 1) & 0x1FFF
    return bytes(out)


def parse_index(dat: bytes) -> list[tuple[str, int, int, int, int]]:
    """→ `[(name, offset, size, extra, zsize)]`，顺序与 `thdat -l` 一致。"""
    filesize = len(dat)
    hdr = bytearray(dat[:16])
    if hdr[:4] != b"PBGZ":
        raise SystemExit(f"不是 TH08 dat：头 4 字节 = {bytes(hdr[:4])!r}")
    hdr[4:] = th_decrypt(bytes(hdr[4:]), 12, *HEADER_KEY)
    count, off, size = struct.unpack_from("<III", bytes(hdr), 4)
    count -= 123456
    off -= 345678
    size -= 567891

    z = th_decrypt(dat[off:], filesize - off, *INDEX_KEY)
    idx = unlzss(z, size)

    p = 0
    entries: list[list] = []
    for _ in range(count):
        e = idx.index(b"\0", p)
        name = idx[p:e].decode("latin1")
        p = e + 1
        eo, es, ex = struct.unpack_from("<III", idx, p)
        p += 12
        entries.append([name, eo, es, ex])
    for i, e in enumerate(entries):
        e.append((entries[i + 1][1] - e[1]) if i + 1 < len(entries) else (off - e[1]))
    return [tuple(e) for e in entries]


def extract(dat: bytes, entry: tuple[str, int, int, int, int]) -> bytes:
    name, eo, es, ex, zs = entry
    raw = unlzss(dat[eo : eo + zs], es)
    if raw[:3] != b"edz":
        return raw
    t = raw[3]
    par = EDZ_PARAMS.get(chr(t))
    return th_decrypt(raw[4:], len(raw) - 4, *par) if par else raw[4:]


def main(argv: list[str]) -> int:
    if len(argv) < 2:
        print(__doc__.strip().split("## 用法")[1].strip(), file=sys.stderr)
        return 2
    dat_path = argv[1]
    rest = argv[2:]
    only_list = "--list" in rest
    rest = [a for a in rest if a != "--list"]
    outdir = rest[0] if rest else None
    want = set(rest[1:])

    dat = open(dat_path, "rb").read()
    entries = parse_index(dat)
    print(f"{dat_path}: {len(entries)} 条", file=sys.stderr)

    if only_list:
        for name, eo, es, ex, zs in entries:
            print(f"{name}\t{es}\t{zs}")
        return 0
    if outdir is None:
        print("要写盘就得给 outdir（或只加 --list）", file=sys.stderr)
        return 2

    os.makedirs(outdir, exist_ok=True)
    written = 0
    for e in entries:
        name = e[0]
        if want and name not in want:
            continue
        body = extract(dat, e)
        rel = name.replace("\\", "/")
        dst = os.path.join(outdir, rel)
        os.makedirs(os.path.dirname(dst) or outdir, exist_ok=True)
        with open(dst, "wb") as fp:
            fp.write(body)
        written += 1
        if want:
            print(f"{name}: 声明 {e[2]} → 解出 {len(body)}", file=sys.stderr)
    # 自检：解出的每条都必须与索引声明的 size 一致（不符就是解错了，不是「看起来不对」）
    bad = []
    for e in entries:
        if want and e[0] not in want:
            continue
        body = extract(dat, e)
        if len(body) != e[2]:
            bad.append((e[0], e[2], len(body)))
    if bad:
        print(f"⚠ {len(bad)} 条尺寸不符索引声明（前 5）：{bad[:5]}", file=sys.stderr)
        return 1
    print(f"写出 {written} 条到 {outdir}；全部 {written} 条尺寸与索引声明一致", file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
