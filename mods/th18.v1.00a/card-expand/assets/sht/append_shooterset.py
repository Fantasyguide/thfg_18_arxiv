#!/usr/bin/env python3
"""往零售的四个 `pl0X.sht` 追加「新卡子机」的 shooterset，输出到 native/build/sht/，并生成 native/sht_ids.h。

    python3 append_shooterset.py                # 构建 + 自检 + 写 sht_ids.h
    python3 append_shooterset.py --verify-only  # 只自检已有的 build/sht/*.sht

## 为什么能纯追加

`pl0X.sht` 的 `+0xe0` 是一张 **40 项**的 shooterset 偏移数组（数据区起点 `+0x180` 是解析器里的硬编码
常量），零售只用了 23 项（`0x00`–`0x09` 主炮、`0x0a`–`0x16` 装备卡子机），**尾部 17 项是 0**。
所以「填一项 + 文件尾接一段」不动前面任何一个字节。
一手：`engine/sht/th18/01-file-layout-and-shooterset-index.md`。

## 三条必须守住的不变式（脚本每次都回读校验）

1. 头部 `+0x02` 仍是 40 —— 它是解析循环的上界。
2. **shooterset 0 的四个 func 字段仍全是 0**。剩下的空位仍指向 set 0，会被重复解析；
   靠 `0 → 表[0] = NULL → 0` 幂等才不炸。改坏它 = 开局崩。
3. 前 23 组 shooterset 的字节逐字节不变。

另外 `fire_rate` 必须 1–127：`0` 在装备卡路径上会整数除零（`0x40a9c0` 没有零分支），
`< 0` 是 shooterset 的终止符。
"""
import argparse
import struct
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent          # …/assets/sht
MOD = HERE.parent.parent                        # …/card-expand
REPO = MOD.parent.parent.parent
RETAIL = REPO / "local" / "th18.v1.00a" / "dat"
OUT = MOD / "native" / "build" / "sht"
IDS_H = MOD / "native" / "sht_ids.h"
ANM_IDS_H = MOD / "native" / "anm_ids.h"
FILES = ("pl00.sht", "pl01.sht", "pl02.sht", "pl03.sht")

OFF_ARRAY = 0xE0        # 偏移数组
DATA_BASE = 0x180       # 解析器里的硬编码常量
N_SLOTS = 40            # 偏移数组长度（= 头部 +0x02）
N_RETAIL = 23           # 零售用掉的项（0x00–0x16）
STRIDE = 0x5C
TERMINATOR = b"\xff\xff\xff\xff"

# ---- 要追加的 shooterset ----------------------------------------------------
# 一项 = (NAME, [shooter, …])；索引按顺序从 N_RETAIL 起分配。字段图见
# engine/sht/th18/02-shooter-record.md §1。这里只列非 0 字段，其余补 0。
APPEND = [
    # 豌豆射手（id 77）。两组：平时一颗大豌豆，「开大」时一排 7 颗小豌豆。
    # 节奏**不**交给引擎的射击计时器 —— peashooter.c 自带计数器当 short_timer 传进
    # `Player__tick_shooters_for_ability_card`，所以这里的 fire_rate 就是「每 N 个我们的计数发一次」。
    # 字段图见 engine/sht/th18/02-shooter-record.md §1；发射判定是 `timer % fire_rate == start_delay`。
    # opt_slot 必须 0（用 tick_shooters 传进来的子机坐标）；angle −π/2 = 正上方；func_* 全 0 = 直飞不瞄准。

    # 平时：1 秒一颗、一颗 90 = 90 DPS（PVZ 原版射速是 1.425 s ≈ 85 帧，2026-09-08 用户改成整 1 秒；
    # DPS 与改之前的 85/130 基本相同，但节奏更密）。90 塞满 Reimu 的每帧上限、超出 Sakuya 的 60，
    # 所以卡在豌豆在途的帧把上限抬到 180。
    ("PEA_NORMAL", [dict(fire_rate=60, start_delay=0, damage=90, hit_w=16.0, hit_h=16.0,
                         angle=-1.5707964, speed=14.0, opt_slot=0, mode=0,
                         anm="CE_ANM_ABILITY_SCRIPT_PEA", sfx=0x20, func_on_init=0)]),

    # 开大（PVZ 的机枪射手）：每 5 帧一排，一排 7 颗横向铺开 ±48 px；持续 100 帧 = 20 排 = 140 颗。
    # 单颗只有 7（= PVZ 原版豌豆伤害的量级），一排 7 颗同帧也只有 49，稳在所有角色的每帧上限
    # （最低 Sakuya 60）之下 —— 一口气打出去的是**数量**，不是单发。
    ("PEA_GATLING", [dict(fire_rate=5, start_delay=0, damage=7, hit_w=12.0, hit_h=12.0,
                          spawn_dx=dx, angle=-1.5707964, speed=18.0, opt_slot=0, mode=0,
                          anm="CE_ANM_ABILITY_SCRIPT_PEA_SMALL", sfx=-1, func_on_init=0)
                     for dx in (-48.0, -32.0, -16.0, 0.0, 16.0, 32.0, 48.0)]),
]


def pack_shooter(s: dict, anm_ids: dict) -> bytes:
    rate = s["fire_rate"]
    if not 1 <= rate <= 127:
        raise SystemExit(f"fire_rate={rate} 不在 1..127：0 会在装备卡路径上除零，负数是终止符")
    anm = s["anm"]
    anm = anm_ids[anm] if isinstance(anm, str) else anm
    b = bytearray(STRIDE)
    struct.pack_into("<bbh", b, 0x00, rate, s["start_delay"], s["damage"])
    struct.pack_into("<ff", b, 0x04, s.get("spawn_dx", 0.0), s.get("spawn_dy", 0.0))
    struct.pack_into("<ff", b, 0x0C, s["hit_w"], s["hit_h"])
    struct.pack_into("<ff", b, 0x14, s["angle"], s["speed"])
    struct.pack_into("<bb", b, 0x20, s["opt_slot"], s["mode"])
    struct.pack_into("<hh", b, 0x22, anm, s["sfx"])
    struct.pack_into("<bb", b, 0x2A, s.get("fire_rate2", 0), s.get("start_delay2", 0))
    struct.pack_into("<4i", b, 0x2C, s["func_on_init"], s.get("func_on_tick", 0),
                     s.get("func_on_draw", 0), s.get("func_on_hit", 0))
    return bytes(b)


def read_anm_ids() -> dict:
    if not ANM_IDS_H.is_file():
        raise SystemExit(f"缺 {ANM_IDS_H}：先跑 make anm")
    out = {}
    for line in ANM_IDS_H.read_text(encoding="utf-8").splitlines():
        parts = line.split()
        if len(parts) >= 3 and parts[0] == "#define" and parts[2].rstrip("/*").strip().isdigit():
            out[parts[1]] = int(parts[2])
    return out


def offsets(data: bytes) -> list:
    return list(struct.unpack_from(f"<{N_SLOTS}I", data, OFF_ARRAY))


def walk_set(data: bytes, off: int) -> int:
    """→ 该 shooterset 的字节长度（含 4 字节终止符）。"""
    p = DATA_BASE + off
    while struct.unpack_from("<i", data, p)[0] != -1:
        p += STRIDE
        if p > len(data):
            raise SystemExit("shooterset 没有终止符，文件坏了")
    return p + 4 - (DATA_BASE + off)


def check_retail_shape(name: str, data: bytes):
    n = struct.unpack_from("<H", data, 0x02)[0]
    if n != N_SLOTS:
        raise SystemExit(f"{name}: 头部 +0x02 = {n}，期望 {N_SLOTS}")
    offs = offsets(data)
    if offs[0] != 0:
        raise SystemExit(f"{name}: 第 0 项偏移不是 0")
    # set 0 的 func 字段必须全 0（空位重复解析靠它幂等）
    p = DATA_BASE
    while struct.unpack_from("<i", data, p)[0] != -1:
        if any(struct.unpack_from("<4i", data, p + 0x2C)):
            raise SystemExit(f"{name}: shooterset 0 的 func 字段不全为 0 —— 空位会被二次解析成野指针")
        p += STRIDE
    return offs


def build_one(name: str, retail: bytes, sets: list) -> bytes:
    offs = check_retail_shape(name, retail)
    used = sum(1 for i, o in enumerate(offs) if i == 0 or o != 0)
    if used != N_RETAIL:
        raise SystemExit(f"{name}: 零售用了 {used} 项，期望 {N_RETAIL}")
    if N_RETAIL + len(sets) > N_SLOTS:
        raise SystemExit(f"偏移数组只有 {N_SLOTS} 项，零售占 {N_RETAIL}，放不下 {len(sets)} 组")

    out = bytearray(retail)
    for k, (_, blob) in enumerate(sets):
        idx = N_RETAIL + k
        struct.pack_into("<I", out, OFF_ARRAY + idx * 4, len(out) - DATA_BASE)
        out += blob
    return bytes(out)


def verify(name: str, retail: bytes, built: bytes, sets: list):
    offs_r, offs_b = offsets(retail), offsets(built)
    check_retail_shape(name, built)
    if offs_b[:N_RETAIL] != offs_r[:N_RETAIL]:
        raise SystemExit(f"{name}: 前 {N_RETAIL} 项偏移被动过")
    for i in range(N_RETAIL):
        n = walk_set(retail, offs_r[i])
        a, b = DATA_BASE + offs_r[i], DATA_BASE + offs_b[i]
        if retail[a:a + n] != built[b:b + n]:
            raise SystemExit(f"{name}: shooterset {i:#x} 的字节变了")
    for k, (nm, blob) in enumerate(sets):
        idx = N_RETAIL + k
        o = offs_b[idx]
        if o == 0:
            raise SystemExit(f"{name}: 第 {idx:#x} 项没写偏移")
        if built[DATA_BASE + o:DATA_BASE + o + len(blob)] != blob:
            raise SystemExit(f"{name}: {nm} 的数据对不上")
        if walk_set(built, o) != len(blob):
            raise SystemExit(f"{name}: {nm} 的终止符位置不对")
    for i in range(N_RETAIL + len(sets), N_SLOTS):
        if offs_b[i] != 0:
            raise SystemExit(f"{name}: 第 {i:#x} 项本该留空")


def write_ids_h(sets: list):
    lines = ["/* 由 assets/sht/append_shooterset.py 生成 —— 追加进 pl0X.sht 的 shooterset 索引。别手改。 */",
             "#pragma once", ""]
    for k, (nm, blob) in enumerate(sets):
        lines.append(f"#define CE_SHT_SET_{nm:<24} {N_RETAIL + k:#04x}   "
                     f"/* {len(blob) // STRIDE} shooter */")
    IDS_H.write_text("\n".join(lines) + "\n", encoding="utf-8")


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--verify-only", action="store_true")
    a = ap.parse_args()

    anm_ids = read_anm_ids()
    sets = [(nm, b"".join(pack_shooter(s, anm_ids) for s in rows) + TERMINATOR) for nm, rows in APPEND]

    if not sets:                                   # 没有要追加的：不产出（别发一份和零售一模一样的 .sht）
        if not a.verify_only:
            for name in FILES:
                (OUT / name).unlink(missing_ok=True)
            write_ids_h(sets)
        print("  没有登记任何 shooterset：不产出 .sht（build/sht/ 已清空）")
        return 0

    OUT.mkdir(parents=True, exist_ok=True)
    for name in FILES:
        src = RETAIL / name
        if not src.is_file():
            raise SystemExit(f"缺零售样本 {src}（用户放进 local/，见 local/README.md）")
        retail = src.read_bytes()
        dst = OUT / name
        if a.verify_only:
            if not dst.is_file():
                raise SystemExit(f"缺 {dst}：先跑 make sht")
            built = dst.read_bytes()
        else:
            built = build_one(name, retail, sets)
            dst.write_bytes(built)
        verify(name, retail, built, sets)
        print(f"  {name}: {len(retail)} → {len(built)} 字节，追加 {len(sets)} 组"
              f"（索引 {N_RETAIL:#04x}..{N_RETAIL + len(sets) - 1:#04x}）")

    if not a.verify_only:
        write_ids_h(sets)
        print(f"wrote {IDS_H.relative_to(MOD)}")
    for k, (nm, blob) in enumerate(sets):
        print(f"  CE_SHT_SET_{nm} = {N_RETAIL + k:#04x}  ({len(blob) // STRIDE} shooter)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
