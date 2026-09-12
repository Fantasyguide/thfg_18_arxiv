#!/usr/bin/env python3
"""独立硬校验（不依赖生成器内部状态）：

  1. 对 `native/build/pl0X.anm` 用 `thanm -l 18` 回读：**每一个** script 里每个
     `spriteN` 的 N 必须 < 该文件 sprite 总数、且 N 确实存在；
  2. 对 `native/build/sht/pl0X.sht` 回读：每一条 shooter 的 `anm`(+0x22) 与
     `hit_fx`(+0x26) 都必须是同编号 `pl0X.anm` 里存在的 script；
  3. 兽灵脚本（pl00 10/11/8，pl01 13/16/8）的**主飞行路径**不得 `delete()`
     （delete 只允许在 `interruptLabel` 之后）—— 这是「弹不会提前消失」的硬条件。

    python3 assets/check_player_anm_bounds.py
"""
import re
import struct
import sys
from pathlib import Path

import anmlib as L

HERE, MOD = L.HERE, L.MOD
BUILD = L.BUILD
BEAST = {"pl00": (10, 11, 8), "pl01": (13, 16, 8)}
# 只有「会飞的主弹脚本」要求主飞行路径无 delete；hit_fx(8) 是一次性特效，本来就要自删。
FLYING = {"pl00": (10, 11), "pl01": (13, 16)}
FILES = ("pl00", "pl01", "pl02", "pl03")

_SCRIPTS = re.compile(r"^script script(\d+) \{\n(.*?)^\}", re.M | re.S)
_SPRITE_REF = re.compile(r"\bsprite\(sprite(\d+)\)")
_SPRITE_DEF = re.compile(r"\bsprite(\d+):")


def _read_sht(path: Path):
    """-> {set_idx: [(anm, hit_fx), …]}（40 项偏移数组，数据区 +0x180，stride 0x5c）。"""
    b = path.read_bytes()
    offs = list(struct.unpack_from("<40I", b, 0xE0))
    out = {}
    for idx, o in enumerate(offs):
        if o == 0:
            continue
        q = 0x180 + o
        rows = []
        while struct.unpack_from("<i", b, q)[0] != -1:
            anm = struct.unpack_from("<h", b, q + 0x22)[0]
            hfx = struct.unpack_from("<h", b, q + 0x26)[0]
            rows.append((anm, hfx))
            q += 0x5C
        out[idx] = rows
    return out


def main():
    th, td, mapdir = L.find_tools()
    tools, anmmap = (th, td), mapdir / "v8.anmm"
    bad = []
    for player in FILES:
        anm_path = BUILD / f"{player}.anm"
        sht_path = BUILD / "sht" / f"{player}.sht"
        text = L.thanm(["-l", L.VERSION, str(anm_path), "-m", str(anmmap)], anm_path.parent, tools)
        ids = [int(m.group(1)) for m in _SPRITE_DEF.finditer(text)]
        total = (max(ids) + 1) if ids else 0
        scripts = {int(m.group(1)): m.group(2) for m in _SCRIPTS.finditer(text)}
        print(f"{player}.anm: sprites={total} (ids {min(ids)}..{max(ids)}), scripts={len(scripts)}")

        # 1. sprite 越界（全脚本穷举）
        for sid, body in scripts.items():
            for m in _SPRITE_REF.finditer(body):
                r = int(m.group(1))
                if r not in ids or r >= total:
                    bad.append(f"{player}.anm: script{sid} 引用不存在的 sprite{r}（total={total}）")

        # 3. 兽灵脚本主飞行路径无 delete()
        for sid in BEAST.get(player, ()):
            if sid not in scripts:
                bad.append(f"{player}.anm: script{sid} 缺失")
                continue
            body = scripts[sid]
            main = body.split("interruptLabel", 1)[0]
            if sid in FLYING.get(player, ()) and "delete(" in main:
                bad.append(f"{player}.anm: 会飞 script{sid} 的普通飞行路径含 delete()")
            refs = sorted({int(m.group(1)) for m in _SPRITE_REF.finditer(body)})
            oob = [r for r in refs if r not in ids or r >= total]
            print(f"  script{sid}: sprite refs {refs} -> "
                  f"{'OK (< ' + str(total) + ')' if not oob else 'OUT OF RANGE ' + str(oob)}"
                  f"; 主路径 delete={'yes' if 'delete(' in main else 'no'}")

        # 2. sht 引用的 anm / hit_fx 必须是本文件存在的 script
        if not sht_path.is_file():
            raise SystemExit(f"缺 {sht_path}：先跑 python3 assets/sht/append_shooterset.py")
        n_ref = 0
        for idx, rows in _read_sht(sht_path).items():
            if not (0x1E <= idx <= 0x27):
                continue   # 0x17..0x1d 是 PEA/SEASON，anm 指 ability.anm，不归本文件
            for anm, hfx in rows:
                n_ref += 1
                if anm not in scripts:
                    bad.append(f"{player}.sht: set {idx:#x} anm={anm} 不在 {player}.anm（脚本数 {len(scripts)}）")
                if hfx and hfx not in scripts:
                    bad.append(f"{player}.sht: set {idx:#x} hit_fx={hfx} 不在 {player}.anm")
        print(f"  sht(0x1e..0x27): {n_ref} 条 shooter 的 anm/hit_fx 全部落在 {player}.anm 的 script 表内")

    if bad:
        print("FAIL:")
        for b in bad:
            print("  " + b)
        return 1
    print("OK: sprite 全在界内；sht 的 anm/hit_fx 全部存在；兽灵脚本主路径无 delete()。")
    return 0


if __name__ == "__main__":
    sys.exit(main())
