#!/usr/bin/env python3
"""把 TH06 系的自机火力表从 exe 里读出来（**离线**，不开 Ghidra，不抢工程锁）。

TH06 期还没有 `.sht` 文件（`.sht` 从 TH07 才有），自机数据是**烧在 exe 里的静态表**：

    g_CharData[4]                 每个自机型的移动速度 + 两个发弹回调
      -> g_CharacterPowerData<X>[9]   九档，每档 {发数, 阈值, bullets*}
           -> CharacterPowerBulletData[n]   每发一条 0x24 字节的记录

⚠️ `power` 字段是**开区间上界**，不是「本档的火力值」。运行时走的是
`while (currentPower >= powerData->power) powerData++;`，所以第 i 档覆盖的火力区间是
`[前一档阈值, 本档阈值)`，最后一档阈值 999 = 满火力那档。

用法：
    python3 tooling/ghidra/th06nc_sht_dump.py                 # NC 全表
    python3 tooling/ghidra/th06nc_sht_dump.py --version th06  # 原版全表
    python3 tooling/ghidra/th06nc_sht_dump.py --diff          # 两边逐字段 diff
"""
import argparse
import math
import struct
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]

BULLET_RECORD_SIZE = 0x24
TIER_COUNT = 9
CHARACTERS = ("ReimuA", "ReimuB", "MarisaA", "MarisaB")

# 表的地址。NC 侧由 Player__FireBulletFromTable 的四个 26 字节转发桩给出；
# TH06 侧由 th06:0x4260d0 / 0x426420 / 0x426450 / 0x426480 四个桩 PUSH 的立即数给出。
TARGETS = {
    "th06nc": {
        "exe": REPO / "local" / "th06nc" / "th06nc.exe",
        "char_data": 0x1403DE3B0,
        "char_data_stride": 0x20,
        "tables": (0x1403DCDB0, 0x1403DD530, 0x1403DCBD0, 0x1403DC8A0),
    },
    "th06": {
        "exe": REPO / "local" / "th06.v1.02h" / "th06.exe",
        "char_data": 0x0069D5C8,   # 仅供参考，本工具不读它（32 位布局不同）
        "char_data_stride": 0x18,
        "tables": (0x00476F78, 0x00477770, 0x00477ED8, 0x00478468),
    },
}

# CharacterPowerBulletData，0x24 字节。x86 与 x64 下**相同**（没有指针成员）。
BULLET_FIELDS = (
    (0x00, "waitBetweenBullets", "i16"),
    (0x02, "bulletFrame", "i16"),
    (0x04, "motionX", "f32"),
    (0x08, "motionY", "f32"),
    (0x0C, "sizeX", "f32"),
    (0x10, "sizeY", "f32"),
    (0x14, "direction", "f32"),
    (0x18, "velocity", "f32"),
    (0x1C, "damage", "u16"),
    (0x1E, "spawnPositionIdx", "u8"),
    (0x1F, "bulletType", "u8"),
    (0x20, "anmFileIdx", "i16"),
    (0x22, "bulletSoundIdx", "i16"),
)

# CC0 把 0/1/2 留成了 BULLET_TYPE_0/1/2 —— 没命名。行为是从 CC0 源码读出来的，
# 常量在 NC 里一手复核过（见 engine/sht/th06nc/01 §5）。**名字是我们的命名选择 🟡。**
BULLET_TYPES = {
    0: ("STRAIGHT", "逐帧不改速度，直飞"),
    1: ("HOMING", "开火后 40 帧内朝 positionOfLastEnemyHit 转向，速度钳 10；"
                  "没有目标时每帧 +1/3 加速到 10"),
    2: ("RISING", "每帧 velocity.y -= 0.3f，越飞越快地上冲"),
    3: ("LASER", "CC0 唯一命名的一个：占用 laserTimer[bulletFrame]，"
                 "waitBetweenBullets 是冷却而不是间隔"),
}


class Image:
    """够用的 PE 读取器：VA -> 文件偏移。"""

    def __init__(self, path):
        self.data = Path(path).read_bytes()
        pe = struct.unpack_from("<I", self.data, 0x3C)[0]
        if self.data[pe:pe + 4] != b"PE\0\0":
            raise SystemExit("not a PE: %s" % path)
        magic = struct.unpack_from("<H", self.data, pe + 24)[0]
        self.ptr_size = 4 if magic == 0x10B else 8
        self.imagebase = (struct.unpack_from("<I", self.data, pe + 24 + 28)[0] if magic == 0x10B
                          else struct.unpack_from("<Q", self.data, pe + 24 + 24)[0])
        nsec = struct.unpack_from("<H", self.data, pe + 6)[0]
        optsz = struct.unpack_from("<H", self.data, pe + 20)[0]
        self.sections = []
        for i in range(nsec):
            off = pe + 24 + optsz + i * 40
            name = self.data[off:off + 8].rstrip(b"\0").decode()
            vsz, va, _rsz, ptr = struct.unpack_from("<IIII", self.data, off + 8)
            self.sections.append((name, va, vsz, ptr))

    def offset(self, va):
        for _name, rva, vsz, ptr in self.sections:
            if self.imagebase + rva <= va < self.imagebase + rva + vsz:
                return ptr + (va - self.imagebase - rva)
        raise KeyError("VA 0x%x 不在任何节里" % va)

    def read(self, va, size):
        off = self.offset(va)
        return self.data[off:off + size]

    def pointer(self, va):
        raw = self.read(va, self.ptr_size)
        return struct.unpack("<I" if self.ptr_size == 4 else "<Q", raw)[0]


def decode_bullet(raw):
    codes = {"i16": "<h", "u16": "<H", "u8": "<B", "f32": "<f"}
    return {name: struct.unpack_from(codes[kind], raw, offset)[0]
            for offset, name, kind in BULLET_FIELDS}


def read_tables(version):
    target = TARGETS[version]
    image = Image(target["exe"])
    tier_stride = 8 + image.ptr_size
    out = {}
    for name, table in zip(CHARACTERS, target["tables"]):
        tiers = []
        for index in range(TIER_COUNT):
            head = image.read(table + index * tier_stride, tier_stride)
            count, threshold = struct.unpack_from("<2i", head, 0)
            pointer = struct.unpack("<I" if image.ptr_size == 4 else "<Q",
                                    head[8:8 + image.ptr_size])[0]
            bullets = [decode_bullet(image.read(pointer + i * BULLET_RECORD_SIZE,
                                                BULLET_RECORD_SIZE))
                       for i in range(count)]
            tiers.append({"threshold": threshold, "bullets": bullets})
        out[name] = tiers
    return out


def power_range(tiers, index):
    """第 index 档实际覆盖的自机火力区间（闭区间），按 while(power >= threshold) 推。"""
    low = 0 if index == 0 else tiers[index - 1]["threshold"]
    high = tiers[index]["threshold"] - 1
    return low, min(high, 128)


def print_tables(version):
    tables = read_tables(version)
    for name, tiers in tables.items():
        print("================ %s (%s) ================" % (name, version))
        for index, tier in enumerate(tiers):
            low, high = power_range(tiers, index)
            print("-- 档 %d: 阈值 %-4d -> 火力 %d..%d   %d 发"
                  % (index, tier["threshold"], low, high, len(tier["bullets"])))
            for i, b in enumerate(tier["bullets"]):
                kind = BULLET_TYPES.get(b["bulletType"], ("?", ""))[0]
                print("   [%2d] wait=%-4d frame=%-3d motion=(%g,%g) size=(%g,%g) "
                      "dir=%+8.1f deg v=%-4g dmg=%-4d spawn=%d %-8s anm=%d snd=%d"
                      % (i, b["waitBetweenBullets"], b["bulletFrame"], b["motionX"], b["motionY"],
                         b["sizeX"], b["sizeY"], math.degrees(b["direction"]), b["velocity"],
                         b["damage"], b["spawnPositionIdx"], kind, b["anmFileIdx"],
                         b["bulletSoundIdx"]))
        print()


def diff():
    """TH06 原版 vs TH06NC 逐字段比对，返回 (记录数, 分类字典)。"""
    old, new = read_tables("th06"), read_tables("th06nc")
    records = 0
    classes = {"anm_shift": [], "threshold": [], "direction_ulp": [], "sound": [], "other": []}
    for name in CHARACTERS:
        for index, (a, b) in enumerate(zip(old[name], new[name])):
            if a["threshold"] != b["threshold"]:
                classes["threshold"].append((name, index, a["threshold"], b["threshold"]))
            if len(a["bullets"]) != len(b["bullets"]):
                classes["other"].append((name, index, "numBullets", len(a["bullets"]),
                                         len(b["bullets"])))
                continue
            for i, (x, y) in enumerate(zip(a["bullets"], b["bullets"])):
                records += 1
                for key in x:
                    if x[key] == y[key]:
                        continue
                    where = (name, index, i, x[key], y[key])
                    if key == "anmFileIdx":
                        classes["anm_shift"].append(where)
                    elif key == "direction":
                        classes["direction_ulp"].append(where)
                    elif key == "bulletSoundIdx":
                        classes["sound"].append(where)
                    else:
                        classes["other"].append((name, index, i, key, x[key], y[key]))
    return records, classes


def print_diff():
    records, classes = diff()
    shifts = {b - a for _n, _t, _i, a, b in classes["anm_shift"]}
    print("共比对 %d 条弹记录（4 自机型 x 9 档）\n" % records)
    print("1. anm 脚本号：%d 处，位移集合 = %s   %s"
          % (len(classes["anm_shift"]), sorted(shifts),
             "**全部 +32，纯重编号**" if shifts == {32} else "!! 位移不一致，需要人工看"))
    print("2. 满火力档阈值：%d 处  %s"
          % (len(classes["threshold"]),
             ", ".join("%s 档%d %d->%d" % row for row in classes["threshold"])))
    print("3. direction 位模式：%d 处" % len(classes["direction_ulp"]))
    for name, tier, i, a, b in classes["direction_ulp"]:
        ha = struct.unpack("<I", struct.pack("<f", a))[0]
        hb = struct.unpack("<I", struct.pack("<f", b))[0]
        print("     %s 档%d[%d]  %.1f deg  0x%08x -> 0x%08x  (%d ULP)"
              % (name, tier, i, math.degrees(a), ha, hb, abs(hb - ha)))
    print("4. 射击音效：%d 处  %s"
          % (len(classes["sound"]),
             ", ".join("%s 档%d[%d] %d->%d" % row for row in classes["sound"])))
    print("5. 其它（应为空）：%s" % (classes["other"] or "无"))


def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--version", default="th06nc", choices=sorted(TARGETS))
    parser.add_argument("--diff", action="store_true", help="TH06 原版 vs TH06NC 逐字段比对")
    args = parser.parse_args()

    needed = list(TARGETS) if args.diff else [args.version]
    for version in needed:
        if not TARGETS[version]["exe"].exists():
            raise SystemExit("缺样本（gitignored，见 local/README.md）：%s"
                             % TARGETS[version]["exe"])
    if args.diff:
        print_diff()
    else:
        print_tables(args.version)


if __name__ == "__main__":
    main()
