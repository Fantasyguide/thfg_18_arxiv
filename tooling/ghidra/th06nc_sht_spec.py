"""TH06NC 的「SHT」——自机射击数据。

**TH06 期没有 `.sht` 文件**（那个格式从 TH07 才有；`sht-webedit` 的 struct 表最早是
`struct_07.js`，`thtk` / `truth` 更是完全不支持 sht）。TH06 与 TH06NC 的自机数据是
**烧在 exe 里的静态表**：

    g_CharData[4]                        每自机型：移动速度 + 两个发弹回调
      -> g_CharacterPowerData<X>[9]      九档：{发数, 阈值, bullets*}
           -> CharacterPowerBulletData[] 每发一条 0x24 字节

⚠️ **`power` 字段是开区间上界，不是「本档的火力值」。** 运行时是
`while (currentPower >= powerData->power) powerData++;`（NC 侧一手：
`0x14006a5ae` 的 `CMP/JGE/ADD RBX,0x10`），所以第 i 档覆盖 `[前一档阈值, 本档阈值)`，
最后一档阈值 999 就是满火力那档。仓库里 `th06nc_player_spec.POWER_LEVELS`
一度只列了 8 个阈值并叫它「档位」，两处都不对，已更正。

出处：结构与语义对照 CC0 th06-decomp（CC0 1.0，可转发）的 `Player.hpp` / `Player.cpp`；
地址、字节与常量一律在各自 exe 上取。表的内容由
[`th06nc_sht_dump.py`](th06nc_sht_dump.py) **离线**读出，不依赖 Ghidra。
"""

EXPECTED_MD5 = "b0cc28ec904e9efce2439ad857c938b5"

GLOBALS = [
    (0x1403DE3B0, "g_CharData",
     "4 x CharacterData, stride 0x20 (x64: 4 floats + 2 function pointers). "
     "Index is shotType + character * 2. Reimu moves 4.0/2.0, Marisa 5.0/2.5; "
     "both callbacks of a row point at the same 26-byte thunk, i.e. focus does not "
     "change the shot table - same as TH06."),
    (0x1403DCDB0, "g_CharacterPowerDataReimuA",
     "9 x CharacterPowerData {i32 numBullets; i32 powerThreshold; ptr bullets}, stride 0x10. "
     "powerThreshold is an EXCLUSIVE upper bound; the last one is 999 = full power."),
    (0x1403DD530, "g_CharacterPowerDataReimuB", "9 x CharacterPowerData, stride 0x10."),
    (0x1403DCBD0, "g_CharacterPowerDataMarisaA", "9 x CharacterPowerData, stride 0x10."),
    (0x1403DC8A0, "g_CharacterPowerDataMarisaB", "9 x CharacterPowerData, stride 0x10."),
    (0x1404F1E88, "g_GameManager__currentPower",
     "Player power, read as a WORD by Player__FireBulletFromTable at 0x14006a5a1. "
     "A GameManager field, not a standalone global: 0x1404f1e80 is the character, "
     "0x1404f1e81 the shot type and 0x1404f1e84 the stage number. The struct base "
     "itself is not located yet."),
]

FUNCTION_COMMENTS = [
    (0x14006A580,
     "[NC first-hand] Walks the CharacterPowerData table with "
     "while (currentPower >= tier->powerThreshold) tier++ (0x14006a5ae), so the "
     "threshold is an EXCLUSIVE upper bound and the 999 tier is full power. "
     "There is no bounds check, exactly as in TH06. Then indexes "
     "tier->bullets by bulletIdx * 0x24 (LEA RDI,[R12+R12*8] then RDI*4) and "
     "branches on bulletType == 3 (laser) at +0x1f."),
]

# 一条 0x24 字节的弹记录。x86 与 x64 下**相同**（无指针成员）。
BULLET_FIELDS = [
    (0x00, "waitBetweenBullets", "i16", "非激光：发射间隔；激光：冷却帧数"),
    (0x02, "bulletFrame", "i16", "非激光：在间隔里的相位；激光：laserTimer 槽号（0/1）"),
    (0x04, "motion", "f32[2]", "相对生成点的偏移"),
    (0x0C, "size", "f32[2]", "判定/贴图尺寸；激光的 y 是 480 = 一屏高"),
    (0x14, "direction", "f32", "弧度。样本里**全部是整度数**"),
    (0x18, "velocity", "f32", ""),
    (0x1C, "damage", "u16", ""),
    (0x1E, "spawnPositionIdx", "u8", "0 = 自机中心；1/2 = orbsPosition[idx-1]，即两个子机"),
    (0x1F, "bulletType", "u8", "见 BULLET_TYPES"),
    (0x20, "anmFileIdx", "i16", ""),
    (0x22, "bulletSoundIdx", "i16", "-1 = 不发声"),
]

# ⚠️ CC0 把 0/1/2 留成了 BULLET_TYPE_0/1/2 —— **没命名**。
# 行为是从 CC0 源码读出来的，四个常量都在 NC 里一手复核过（地址见备注）。
# **名字是我们的命名选择，可信度 🟡**；行为本身 ✅。
BULLET_TYPES = {
    0: ("STRAIGHT", "逐帧不改速度，直飞。CC0 的 switch 里没有它的 case。", None),
    1: ("HOMING",
        "开火后 40 帧内朝 positionOfLastEnemyHit 转向，速度钳 10，下限 1；"
        "没有目标时每帧 +1/3 加速到 10。只有 ReimuA 用。",
        "NC 一手：0x1400698c4 的 COMISS 拿 -100.0f（0x14030d0f0）比 "
        "lastEnemyHit.x，0x1400698d1 的 CMP EAX,0x28 就是那 40 帧，"
        "钳位常量 10.0f 在 0x14030cea0。"),
    2: ("RISING",
        "每帧 velocity.y -= 0.3f，越飞越快地上冲。只有 MarisaA 用。",
        "NC 一手：0x1400698a3 的 SUBSS XMM0,XMM12，XMM12 = 0.3f（0x14030cd40）。"),
    3: ("LASER",
        "CC0 唯一命名的一个。占用 laserTimer[bulletFrame]（Player +0x79e8，步长 8），"
        "waitBetweenBullets 是冷却而非间隔。只有 MarisaB 用。",
        "NC 一手：0x14006a5c9 的 CMP byte ptr [RSI+RDI*4+0x1f],0x3。"),
}

# ── TH06 原版 vs TH06NC：193 条弹记录逐字段比对的结论 ────────────────────────
# 由 th06nc_sht_dump.py --diff 产出，测试会重跑一遍。四类差异之外**逐字节相同**。
DIFF_VS_TH06 = {
    "records": 193,
    "anm_shift": 32,          # 193/193 条，位移集合恰为 {32}，纯 ANM 脚本重编号
    "threshold": (127, 128),  # 四个自机型的第 7 档阈值，唯一的玩法差异
    "direction_ulp": 46,      # 全部 1 ULP，编译期浮点求值路径不同，两边都是整度数
    "sound_fix": ("ReimuA", 2, 0, -1, 0),   # 原版火力 16..31 档没有射击音，NC 补上了
}

TH06_TABLES = {   # 供 diff 复现用；由 th06:0x4260d0/0x426420/0x426450/0x426480 四个桩给出
    "ReimuA": 0x00476F78, "ReimuB": 0x00477770,
    "MarisaA": 0x00477ED8, "MarisaB": 0x00478468,
}


def validate():
    errors = []
    seen = {}
    for address, name, comment in GLOBALS:
        if not (0x140000000 <= address < 0x141000000):
            errors.append("%s 的地址不在映像范围内" % name)
        if address in seen:
            errors.append("0x%x 被 %s 和 %s 同时占用" % (address, seen[address], name))
        seen[address] = name
        if not comment.isascii():
            errors.append("%s 的注释会写进 Ghidra 并发布，必须 ASCII" % name)
    for address, comment in FUNCTION_COMMENTS:
        if not comment.isascii():
            errors.append("0x%x 的注释必须 ASCII" % address)
    offsets = [o for o, _n, _k, _c in BULLET_FIELDS]
    if offsets != sorted(offsets):
        errors.append("BULLET_FIELDS 没按偏移排序")
    if offsets[-1] + 2 != 0x24:
        errors.append("BULLET_FIELDS 没铺满 0x24 字节")
    for code, (name, behaviour, evidence) in BULLET_TYPES.items():
        if not name.isupper():
            errors.append("bulletType %d 的名字要大写" % code)
        if code != 0 and not evidence:
            errors.append("bulletType %d (%s) 没写 NC 侧的一手证据" % (code, name))
    if 3 not in BULLET_TYPES or BULLET_TYPES[3][0] != "LASER":
        errors.append("3 必须是 LASER —— 那是 CC0 唯一命名的一个，不能改")
    if DIFF_VS_TH06["threshold"] != (127, 128):
        errors.append("满火力档阈值那条结论被改了，改之前先重跑 --diff")
    return errors
