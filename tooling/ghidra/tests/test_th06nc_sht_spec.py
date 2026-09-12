"""TH06NC 自机火力表的守门测试。

这套测试**不看 Ghidra**，直接从两个 exe 重读表，所以它守的是**数据结论**：
1. spec 自检。
2. 表的形状：4 自机型 × 9 档，最后一档阈值 999，阈值单调递增。
3. `direction` 全部是整度数（字段映射对了才会这样）。
4. TH06 原版 vs TH06NC 的四类差异**逐条**对上，第五类（其它）必须为空。
   样本一换、结论一改，这里立刻失败。
"""
import importlib.util
import math
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parents[3]


def load(name):
    path = REPO / "tooling" / "ghidra" / f"{name}.py"
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


class Th06ncShtSpecTests(unittest.TestCase):
    def setUp(self):
        self.spec = load("th06nc_sht_spec")
        self.dump = load("th06nc_sht_dump")

    def have(self, *versions):
        for version in versions:
            if not self.dump.TARGETS[version]["exe"].exists():
                self.skipTest("%s 样本未就位（gitignored）" % version)

    def test_spec_self_check_passes(self):
        self.assertEqual(self.spec.validate(), [])

    def test_no_sht_file_ships_with_the_game(self):
        # TH06 期没有 .sht —— 这正是本子系统在这一作要走 exe 内静态表的理由。
        extracted = REPO / "local" / "th06nc" / "extracted"
        if not extracted.is_dir():
            self.skipTest("样本未解包")
        self.assertEqual(list(extracted.rglob("*.sht")), [])

    def test_table_shape(self):
        self.have("th06nc")
        tables = self.dump.read_tables("th06nc")
        self.assertEqual(sorted(tables), sorted(self.dump.CHARACTERS))
        for name, tiers in tables.items():
            with self.subTest(character=name):
                self.assertEqual(len(tiers), 9)
                thresholds = [t["threshold"] for t in tiers]
                self.assertEqual(thresholds[-1], 999, "最后一档必须是满火力那档")
                self.assertEqual(thresholds, sorted(thresholds), "阈值必须单调递增")
                self.assertTrue(all(t["bullets"] for t in tiers))

    def test_power_ranges_are_contiguous_and_cover_0_to_128(self):
        self.have("th06nc")
        for name, tiers in self.dump.read_tables("th06nc").items():
            with self.subTest(character=name):
                spans = [self.dump.power_range(tiers, i) for i in range(9)]
                self.assertEqual(spans[0][0], 0)
                self.assertEqual(spans[-1][1], 128)
                for (_lo, hi), (lo2, _hi2) in zip(spans, spans[1:]):
                    self.assertEqual(hi + 1, lo2, "档与档之间不能有缝")

    def test_every_direction_is_a_whole_number_of_degrees(self):
        # 字段映射错了就不会是整度数 —— 这是最便宜的一道量纲常识闸门。
        self.have("th06nc")
        for name, tiers in self.dump.read_tables("th06nc").items():
            for ti, tier in enumerate(tiers):
                for bi, bullet in enumerate(tier["bullets"]):
                    degrees = math.degrees(bullet["direction"])
                    with self.subTest(character=name, tier=ti, bullet=bi):
                        self.assertAlmostEqual(degrees, round(degrees), places=3)

    def test_bullet_types_seen_match_the_spec(self):
        self.have("th06nc")
        seen = {b["bulletType"]
                for tiers in self.dump.read_tables("th06nc").values()
                for tier in tiers for b in tier["bullets"]}
        self.assertTrue(seen <= set(self.spec.BULLET_TYPES))

    def test_only_one_character_uses_each_special_bullet_type(self):
        # HOMING/RISING/LASER 各自只属于一个自机型 —— 与「霊符/魔符/恋符」的设定对得上。
        self.have("th06nc")
        owners = {}
        for name, tiers in self.dump.read_tables("th06nc").items():
            for tier in tiers:
                for bullet in tier["bullets"]:
                    owners.setdefault(bullet["bulletType"], set()).add(name)
        self.assertEqual(owners.get(1), {"ReimuA"})
        self.assertEqual(owners.get(2), {"MarisaA"})
        self.assertEqual(owners.get(3), {"MarisaB"})

    def test_diff_against_th06_has_exactly_the_four_documented_classes(self):
        self.have("th06", "th06nc")
        records, classes = self.dump.diff()
        expected = self.spec.DIFF_VS_TH06

        self.assertEqual(records, expected["records"])
        self.assertEqual(classes["other"], [], "出现了第五类差异 —— 结论要重写")

        # 1. anm 脚本号：每一条都差，且位移恰好是同一个数
        self.assertEqual(len(classes["anm_shift"]), records)
        self.assertEqual({b - a for _n, _t, _i, a, b in classes["anm_shift"]},
                         {expected["anm_shift"]})

        # 2. 满火力档阈值：四个自机型全部 127 -> 128，且只在第 7 档
        old, new = expected["threshold"]
        self.assertEqual(len(classes["threshold"]), 4)
        for _name, tier, a, b in classes["threshold"]:
            self.assertEqual((tier, a, b), (7, old, new))

        # 3. direction：全部恰好 1 ULP
        import struct
        self.assertEqual(len(classes["direction_ulp"]), expected["direction_ulp"])
        for _name, _tier, _i, a, b in classes["direction_ulp"]:
            ha, hb = (struct.unpack("<I", struct.pack("<f", v))[0] for v in (a, b))
            self.assertEqual(abs(hb - ha), 1)

        # 4. 射击音效：只有那一条
        self.assertEqual(len(classes["sound"]), 1)
        name, tier, index, a, b = classes["sound"][0]
        self.assertEqual((name, tier, index, a, b), expected["sound_fix"])

    def test_th06_really_is_missing_that_shot_sound(self):
        # 上一条的正面表述：原版 ReimuA 火力 16..31 档整档没有射击音，NC 补上了。
        self.have("th06", "th06nc")
        for version, expected in (("th06", False), ("th06nc", True)):
            tier = self.dump.read_tables(version)["ReimuA"][2]
            has_sound = any(b["bulletSoundIdx"] >= 0 for b in tier["bullets"])
            with self.subTest(version=version):
                self.assertEqual(has_sound, expected)
