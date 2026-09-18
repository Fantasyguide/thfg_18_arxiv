#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""extract_exe_icons.py —— 从各作 exe 里抽出**图标资源**（最大的那一张），存成 PNG（另附 .ico）。

    python3 tools/extract_exe_icons.py --out <目录>
    python3 tools/extract_exe_icons.py --out <目录> --games th07,th10,th18

## 为什么要它

用户 2026-09-18：给系统卡（106–118）做卡面时「**把每作的 exe 图标做进来图片**」。
图标是商业 exe 里的资源，本仓**不入库**（`local/*` gitignored）⇒ 只做「现抽现用」的搬运工：
读 PE 的 `RT_GROUP_ICON`/`RT_ICON` 资源 → 拼回一个标准 `.ico` → 用 PIL 取**最大尺寸**那帧 → PNG。

一手：PE 资源目录（`.rsrc` 节）里 icon 是**两条**资源 —— `RT_GROUP_ICON`(14) 给「这个图标由哪几张组成」，
`RT_ICON`(3) 给像素数据（DIB 或内嵌 PNG）⇒ 必须先用 group 把散图拼成 `.ico` 才能让 PIL 正确解码。
**抽不到**（exe 里没有 icon 资源）⇒ 该作记 `—` 并跳过，不猜、不外借别作的图。
"""
import argparse
import io
import os
import struct
import sys
from pathlib import Path

import pefile
from PIL import Image

REPO = Path(__file__).resolve().parents[4]          # renkolab/
LOCAL = REPO / "local"
# 系统卡 ↔ 作（`patch/th18/cards.js` 的 id → internal_name；这里是给人看的对照）
GAMES = {
    "th06": ("106", "红魔乡"),
    "th07": ("107", "妖妖梦"),
    "th08": ("108", "永夜抄"),
    "th09": ("109", "花映塚"),
    "th10": ("110", "风神录"),
    "th11": ("111", "地灵殿"),
    "th12": ("112", "星莲船"),
    "th13": ("113", "神灵庙"),
    "th14": ("114", "辉针城"),
    "th15": ("115", "绀珠传"),
    "th16": ("116", "天空璋"),
    "th17": ("117", "鬼形兽"),
    "th18": ("118", "虹龙洞（十二合一挑战卡的底图）"),
}


def find_exe(tag: str) -> Path | None:
    """`local/<tag>.v*/<tag>.exe`（tag 里带点的作如 th095 也用同名 exe）。"""
    for d in sorted(LOCAL.glob(f"{tag}.v*")):
        for exe in sorted(d.glob("*.exe")):
            if exe.stem == tag or exe.stem.startswith(tag):
                return exe
    return None


def ico_from_pe(exe: Path) -> bytes | None:
    """把 PE 里的 icon 资源拼成一个标准 `.ico`（取**最大**那组里的最大帧）。"""
    pe = pefile.PE(str(exe), fast_load=True)
    pe.parse_data_directories(directories=[pefile.DIRECTORY_ENTRY["IMAGE_DIRECTORY_ENTRY_RESOURCE"]])
    groups, images = {}, {}
    if not hasattr(pe, "DIRECTORY_ENTRY_RESOURCE"):
        return None
    for t in pe.DIRECTORY_ENTRY_RESOURCE.entries:
        tid = t.id if t.id is not None else None
        if tid not in (3, 14):                       # 3 = RT_ICON，14 = RT_GROUP_ICON
            continue
        for e in t.directory.entries:
            for lang in e.directory.entries:
                data = pe.get_data(lang.data.struct.OffsetToData, lang.data.struct.Size)
                if tid == 3:
                    images[e.id] = data
                else:
                    groups[e.id] = data
    if not images:
        return None
    # 选「最大帧最大」的那一组
    def group_entries(g: bytes):
        _res, _type, count = struct.unpack_from("<HHH", g, 0)
        out = []
        for i in range(count):
            off = 6 + i * 14
            w, h, colors, _r, planes, bpp, size, iid = struct.unpack_from("<BBBBHHIH", g, off)
            out.append((w or 256, h or 256, bpp, iid))
        return out

    best_gid = max(groups, key=lambda gid: max((w * h, bpp) for w, h, bpp, _ in group_entries(groups[gid])))
    ents = group_entries(groups[best_gid])
    blob = struct.pack("<HHH", 0, 1, len(ents))
    payload = b""
    offset = 6 + 16 * len(ents)
    for w, h, bpp, iid in ents:
        data = images.get(iid)
        if data is None:
            continue
        blob += struct.pack("<BBBBHHII", w if w < 256 else 0, h if h < 256 else 0, 0, 0, 1, bpp,
                            len(data), offset)
        payload += data
        offset += len(data)
    return blob + payload


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--out", required=True, help="输出目录（PNG + .ico）")
    ap.add_argument("--games", default="", help="只处理这些 tag（逗号分隔，默认全部）")
    a = ap.parse_args()
    out = Path(a.out)
    out.mkdir(parents=True, exist_ok=True)
    want = [t for t in (a.games.split(",") if a.games else GAMES) if t]

    ok = miss = 0
    for tag in want:
        card_id, cn = GAMES.get(tag, ("?", "?"))
        exe = find_exe(tag)
        if not exe:
            print(f"  {tag:6s} (卡 {card_id} {cn})：**没有样本** ⇒ 跳过")
            miss += 1
            continue
        ico = ico_from_pe(exe)
        if not ico:
            print(f"  {tag:6s} ({exe.name})：exe 里没有 icon 资源 ⇒ 跳过")
            miss += 1
            continue
        (out / f"{tag}.ico").write_bytes(ico)
        im = Image.open(io.BytesIO(ico))
        im = im.convert("RGBA")
        # 取最大帧
        n = getattr(im, "n_frames", 1)
        best = im
        for i in range(1, n):
            im.seek(i)
            if im.size[0] * im.size[1] > best.size[0] * best.size[1]:
                best = im.copy()
        png = out / f"{card_id}_{tag}.png"
        best.save(png)
        print(f"  {tag:6s} (卡 {card_id} {cn})：{best.size[0]}×{best.size[1]} → {png.name}")
        ok += 1
    print(f"抽出 {ok} 个图标，跳过 {miss} 个 → {out}")


if __name__ == "__main__":
    sys.exit(main())
