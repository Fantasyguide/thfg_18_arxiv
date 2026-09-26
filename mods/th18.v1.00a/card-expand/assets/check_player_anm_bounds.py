#!/usr/bin/env python3
"""独立硬校验（不依赖生成器内部状态）：

  1. 对 `native/build/pl0X.anm` 用 `thanm -l 18` 回读：**每一个** script 里每个
     `spriteN` 的 N 必须 < 该文件 sprite 总数、且 N 确实存在；
  2. 对 `native/build/sht/pl0X.sht` 回读：每一条 shooter 的 `anm`(+0x22) 与
     `hit_fx`(+0x26) 都必须是同编号 `pl0X.anm` 里存在的 script；
  3. 追加脚本（兽灵 / 灵界）的**主飞行路径**不得 `delete()`
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
BEAST = {"pl00": (10, 11, 8, 41, 42), "pl01": (13, 16, 8, 47, 48),
         # ★ AL61：pl02 追加红刀/紫刀（30/31）；pl03 追加大青蛙/大蛇（29/30）。
         # ★ 113：pl00/pl01/pl03 追加 TH13 灵界主弹 + 闪光（41/42 · 47/48 · 32/33）。
         "pl02": (30, 31), "pl03": (29, 30, 31, 32, 33)}
# 只有「会飞的主弹脚本」要求主飞行路径无 delete；hit_fx(8) 与灵界闪光(*_GLINT) 是一次性特效，
# 本来就要自删。
FLYING = {"pl00": (10, 11, 41), "pl01": (13, 16, 47), "pl02": (30, 31), "pl03": (29, 30, 32)}
FILES = ("pl00", "pl01", "pl02", "pl03")

_SCRIPTS = re.compile(r"^script script(\d+) \{\n(.*?)^\}", re.M | re.S)
_SPRITE_REF = re.compile(r"\bsprite\(sprite(\d+)\)")
_SPRITE_DEF = re.compile(r"\bsprite(\d+):")


def _declared_slots() -> int:
    """从生成物 `native/sht_ids.h` 读 `CE_SHT_SLOTS`（= 影像头 `+0x02` 应当等于的那个数）。
    ★ 2026-09-23 起不再硬编码 96：111 的组数会随数值口径变化（`N_SLOTS` 已抬到 128）。"""
    h = L.BUILD.parent / "sht_ids.h"
    if not h.is_file():
        raise SystemExit(f"check_player_anm_bounds: 缺生成物 {h}（先跑 `make sht`）")
    m = re.search(r"^#define\s+CE_SHT_SLOTS\s+(\d+)", h.read_text(), re.M)
    if not m:
        raise SystemExit(f"check_player_anm_bounds: {h} 里没有 `#define CE_SHT_SLOTS`")
    return int(m.group(1))


def _read_sht(path: Path):
    """-> {set_idx: [(anm, hit_fx), …]}

    ★ 2026-09-14（`AUDIT.md` §CQ 扩容）：数组区现在是 **预留 256 格**（`[0xe0,0x4e0)`），
      其中**宣布可用 96 格**（= 头 `+0x02`）、数据区起点 `0x4e0`。这些常量必须与
      `assets/sht/append_shooterset.py` 的 `N_SLOTS` / `N_SLOTS_RESERVED` / `DATA_BASE` 一致
      —— 这里刻意**不 import 生成器**（本脚本的定位是「不依赖生成器内部状态」的独立回读），
      代价是常量有两处；`make check` 里生成器自己的 `verify()` 与 `capcheck` 的三方对账是另外两边。
    """
    b = path.read_bytes()
    # ★ 2026-09-23：**宣布可用格数从生成物 `native/sht_ids.h` 读**（`CE_SHT_SLOTS`），不再硬编码 ——
    #   111 的组数会随数值口径变化（DPS 对齐 TH18 原生后 31 → 43 格），`N_SLOTS` 96 → 128；
    #   硬编码会让这条回读被无关改动打红。**预留格数与数据区起点仍是常量**（它们没变过：
    #   256 / 0x4E0 ⇒ 恒等式自检仍然有效）。读的是**产物**而不是生成器内部状态，故独立性不变。
    slots, reserved, base = _declared_slots(), 256, 0x4E0
    # 结构性自检：**预留**数组区末尾必须正好顶到数据区（与生成器同一条恒等式）
    if 0xE0 + reserved * 4 != base:
        raise SystemExit("check_player_anm_bounds: 扩容常量自相矛盾（数组区末尾 != 数据区起点）")
    n = struct.unpack_from("<H", b, 0x02)[0]
    if n != slots:
        raise SystemExit(f"check_player_anm_bounds: {path.name} 头 +0x02 = {n}，期望 {slots}"
                         f"（生成物没重跑？`make sht`）")
    offs = list(struct.unpack_from(f"<{reserved}I", b, 0xE0))
    out = {}
    for idx, o in enumerate(offs):
        if o == 0:
            continue
        q = base + o
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
        # ★★★ 2026-09-26 第十一批（补闸）：**妖侧的逐档组 + 兜底组也按 `pl0X.anm` 解析**，
        #   必须一起查。这一条缺口是实打实漏过 bug 的：`AUDIT.md` §BE #98 把妖侧行的 `+0x22`
        #   填成了 `ability.anm` 的 364..370，而 `pl0X.anm` 只有 51..60 支脚本 ⇒ 运行期越界、
        #   弹的 VM 拿不到脚本；旧版这里只查 `0x1e..0x27`（咆哮组）⇒ **假绿**。
        #   一手（`th18:0x45e719`–`0x45e76c` + `0x40aa95`–`0x40aa9a`）：主炮路径的弹 anm =
        #   `player+0xc` = `pl0X.anm`（`engine/sht/th18/02-shooter-record.md` §4）。
        yk_idx = set()
        for mm in re.finditer(r"^#define\s+(CE_SHT_SET_YOUKAI\w*)\s+(0[xX][0-9a-fA-F]+|\d+)",
                              (L.BUILD.parent / "sht_ids.h").read_text(), re.M):
            yk_idx.add(int(mm.group(2), 0))
        for idx, rows in _read_sht(sht_path).items():
            if not (0x1E <= idx <= 0x27 or idx in yk_idx):
                continue   # 0x17..0x1d 是 PEA/SEASON，anm 指 ability.anm，不归本文件
            # ⚠ 113 的灵界组**不在这里查**：它借用原生槽 0x0e，但槽里的 40 项偏移数组**没变**
            #   （运行时才指向孤儿段）⇒ 照偏移读出来的是原生早苗子机组，不是灵界行。
            #   灵界行的 anm 号由 `append_shooterset.py` 的 `check_pl_anm_ids()`（宏 ↔ 数交叉核对）
            #   与它自己的 `verify()`（孤儿段逐字节回读）把关；本文件用 `BEAST`/`FLYING`/`PINNED`
            #   三个表管 anm 侧。
            for anm, hfx in rows:
                n_ref += 1
                if anm not in scripts:
                    bad.append(f"{player}.sht: set {idx:#x} anm={anm} 不在 {player}.anm（脚本数 {len(scripts)}）")
                if hfx and hfx not in scripts:
                    bad.append(f"{player}.sht: set {idx:#x} hit_fx={hfx} 不在 {player}.anm")
        print(f"  sht(0x1e..0x27 + 妖侧组): {n_ref} 条 shooter 的 anm/hit_fx 全部落在 "
              f"{player}.anm 的 script 表内")

    if bad:
        print("FAIL:")
        for b in bad:
            print("  " + b)
        return 1
    print("OK: sprite 全在界内；sht 的 anm/hit_fx 全部存在；追加脚本（兽灵/灵界）主路径无 delete()。")
    return 0


if __name__ == "__main__":
    sys.exit(main())
