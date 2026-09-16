#!/usr/bin/env python3
"""★ 部署配套性闸：**DLL 与游戏里的 `pl0X.sht` 必须同批**（2026-09-15，AUDIT §CV/§CX）。

    python3 tools/check_deploy_pair.py <待推的 DLL> <modkit 目录>

## 为什么要这道闸（一次真实崩溃）

113 的灵界主炮组把行指针算成 `sht + CE_SHT_TRANCE_DBASE + <编译进 DLL 的偏移>`。
旧设计里那一格是**文件尾的孤儿段**（没有任何偏移项指着它）+ 卡侧临时改偏移项，
于是「新 DLL + 旧 sht」下这个指针会**落到影像之外** ⇒ 引擎把越界字节当子弹回调 `call`
⇒ 一进灵界 `0xC0000005`（崩点 `th18:0x45e8c0` 的 `call eax`，`eax=1`；
链：`AbilityManager__on_tick → Player__on_tick__body → Player__tick_shooting_state →
Player__shoot_one_bullet → PlayerBullet__create`）。

2026-09-15 收尾（NEXT.md §1w 第 5 步）后那一格是**正常追加组**（`CE_SHT_SET_TRANCE` = 0x28），
"指到影像外"这一整类失败模式没有了；但"资产与 DLL 不同批"本身仍然要拦 ——
最轻的后果是主炮组号解析到未分配的 set 0（灵界里主炮没换），重的是别的偏移类耦合。

## 判据（确定性、不靠猜）

1. 游戏里四份 `pl0X.sht` 必须**逐字节等于**当前构建产物 `native/build/sht/pl0X.sht`
   —— 这是"同批"的**充分**判据（生成物与 DLL 都出自当前工作区）；
2. 顺带体检：`CE_SHT_SET_TRANCE` 那一格在四份文件里都**非 0**，且指到的那一行
   符合追加组的形状（首 dword 不是终止符时，`+0x2c..+0x38` 四个 func 字段必须全 0）。

退出码：0 = 配套；1 = 不配套（拒绝同步）；2 = 用法/文件问题。
"""
import os
import re
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
MOD = os.path.dirname(HERE)
sys.path.insert(0, os.path.join(MOD, "assets", "sht"))
import append_shooterset as gen  # noqa: E402  （复用常量，不重复写数字）

DEPLOY_REL = os.path.join("thcrap", "repos", "Renko_1055", "th18_card_expand_255", "th18")
BUILD_DIR = os.path.join(MOD, "native", "build", "sht")


def read_ids_header():
    txt = open(os.path.join(MOD, "native", "sht_ids.h"), encoding="utf-8").read()

    def grab(name):
        m = re.search(rf"#define\s+{name}\s+(0x[0-9a-fA-F]+|\d+)", txt)
        if not m:
            raise SystemExit(f"sht_ids.h 里找不到 {name}")
        return int(m.group(1), 0)
    return grab("CE_SHT_SET_TRANCE"), grab("CE_SHT_TRANCE_DBASE")


def slot_ok(data, slot, dbase):
    """槽偏移项非 0 且指向形状合法的一行。→ (ok, 说明)"""
    if len(data) < gen.OFF_ARRAY + (slot + 1) * 4:
        return False, "文件太短，数组区都没到"
    off = struct.unpack_from("<I", data, gen.OFF_ARRAY + slot * 4)[0]
    if off == 0:
        return False, f"槽 {slot:#x} 的偏移项是 0（这一格没分配）"
    p = dbase + off
    if p + 0x40 > len(data):
        return False, f"槽 {slot:#x} 指向 {p:#x}，超出文件（{len(data):#x}）"
    first = struct.unpack_from("<I", data, p)[0]
    if first == 0xFFFFFFFF:
        return True, "空组（只有终止符）"
    if any(struct.unpack_from("<I", data, p + o)[0] for o in (0x2C, 0x30, 0x34, 0x38)):
        return False, "首行的 func 字段非 0（不像追加组的行）"
    return True, f"行 @ {p:#x}，func 字段全 0 ✔"


def main(argv):
    if len(argv) != 3:
        print(__doc__)
        return 2
    dll_path, modkit = argv[1], argv[2]
    if not os.path.exists(dll_path):
        print(f"找不到 DLL：{dll_path}")
        return 2
    dep_dir = os.path.join(modkit, DEPLOY_REL)
    files = [n for n in sorted(os.listdir(dep_dir)) if re.fullmatch(r"pl0\d\.sht", n)] \
        if os.path.isdir(dep_dir) else []
    if not files:
        print(f"在 {modkit} 里找不到 th18/pl0*.sht —— 不阻断（这套部署里没有 sht 资产）")
        return 0
    slot, dbase = read_ids_header()
    print(f"闸：DLL={os.path.basename(dll_path)}（{os.path.getsize(dll_path)} B）  "
          f"CE_SHT_SET_TRANCE={slot:#x}  DBASE={dbase:#x}（来自 native/sht_ids.h）")

    bad = 0
    for n in files:
        dep = open(os.path.join(dep_dir, n), "rb").read()
        ok, why = slot_ok(dep, slot, dbase)
        print(f"  {'✓' if ok else '✗'} {n}（{len(dep)} B）: 113 的槽 → {why}")
        bad += 0 if ok else 1
        ref_path = os.path.join(BUILD_DIR, n)
        if os.path.exists(ref_path):
            if open(ref_path, "rb").read() != dep:
                print("      ✗ 与当前构建产物不同 ⇒ 资产不是这一版")
                bad += 1
        else:
            print(f"      ⚠ 没有构建产物可对比（{ref_path}）—— 先 `make sht`")
            bad += 1
    if bad:
        print("✗ **不配套**：游戏里的 pl0X.sht 与当前构建不同批。\n"
              "  ⇒ 整批发布：`make release MODKIT_DIR=… NO_GIT=1`（DLL + sht + patch 一起）；\n"
              "  ⇒ 或回滚 DLL：`DLL_SRC=<…/dist/bin/th18_card_expand.dll> bash local/sync_modkit_dll.sh`。")
        return 1
    print("✓ 配套：四份 pl0X.sht = 当前构建产物，且 113 的槽指向形状合法的行")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
