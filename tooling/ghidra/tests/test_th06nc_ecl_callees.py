"""交叉比对得到的子系统函数名的守门测试。

守两件事：
1. **出处**：每个名字都必须能在 CC0 的 th06-decomp 源码里找到原形。
   `th-re-data` 与 `th06-re` 都没有 LICENSE，它们的名字不许混进来。
2. **证据**：每条都得带支撑它的 opcode 与置信度，否则以后没人知道该信几分。
"""
import importlib.util
import re
import unittest
from pathlib import Path


REPO = Path(__file__).resolve().parents[3]
CC0_SRC = REPO / "local" / "vendor" / "th06-decomp" / "src"


def load(name):
    path = REPO / "tooling" / "ghidra" / f"{name}.py"
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


class Th06ncEclCalleeTests(unittest.TestCase):
    def setUp(self):
        self.spec = load("th06nc_ecl_spec")

    def test_every_entry_carries_its_evidence(self):
        for address, name, ops, confidence in self.spec.ECL_CALLEE_NAMES:
            with self.subTest(name=name):
                self.assertTrue(ops, "%s 没有支撑 opcode" % name)
                self.assertIn(confidence, ("exact", "near"))
                self.assertTrue(0x140000000 <= address < 0x141000000)

    def test_addresses_and_names_are_unique(self):
        addresses = [a for a, _n, _o, _c in self.spec.ECL_CALLEE_NAMES]
        names = [n for _a, n, _o, _c in self.spec.ECL_CALLEE_NAMES]
        self.assertEqual(len(addresses), len(set(addresses)))
        self.assertEqual(len(names), len(set(names)))

    def test_near_matches_stay_labelled_as_near(self):
        # 这两条是人工判定接受的近似匹配，别被悄悄升格成 exact
        near = {n for _a, n, _o, c in self.spec.ECL_CALLEE_NAMES if c == "near"}
        self.assertEqual(
            near, {"EnemyEclInstr__SetVar", "BulletManager__SpawnBulletPattern"}
        )

    def test_the_self_validating_case_is_still_covered(self):
        # EnemyManager__SpawnEnemy 是我们先前独立命名的；算法在不知情下配到了
        # CC0 的同名函数。它留在 FUNCTIONS 里而非 callee 表里，但这条对应关系
        # 是整套方法唯一一次「在已知答案上复现」，值得钉住。
        names = {n for _a, n, _c in self.spec.FUNCTIONS}
        self.assertIn("EnemyManager__SpawnEnemy", names)

    def test_names_come_from_the_cc0_upstream_not_the_unlicensed_ones(self):
        if not CC0_SRC.is_dir():
            self.skipTest("th06-decomp not cloned (gitignored)")
        blob = "\n".join(
            p.read_text(encoding="utf-8", errors="replace")
            for p in list(CC0_SRC.glob("*.cpp")) + list(CC0_SRC.glob("*.hpp"))
        )
        for _address, name, _ops, _confidence in self.spec.ECL_CALLEE_NAMES:
            with self.subTest(name=name):
                cls, _, method = name.partition("__")
                # CC0 里写作 Class::method；我们展平成 Class__method
                self.assertRegex(
                    blob,
                    r"\b%s\s*::\s*%s\b" % (re.escape(cls), re.escape(method)),
                    "%s 在 CC0 源码里找不到原形——它可能来自无 LICENSE 的上游" % name,
                )

    def test_spec_self_check_passes(self):
        self.assertEqual(self.spec.validate(), [])



class Th06ncEnemyFieldTests(unittest.TestCase):
    """结构体偏移表的守门。重点是**别让 weak 悄悄升格**。"""

    def setUp(self):
        self.spec = load("th06nc_ecl_spec")

    def test_tables_are_sorted_and_evidenced(self):
        for table in (self.spec.ENEMY_FIELDS, self.spec.BULLET_PROPS_FIELDS):
            offsets = [o for o, _n, _s, _e in table]
            self.assertEqual(offsets, sorted(offsets))
            self.assertEqual(len(offsets), len(set(offsets)))
            for _offset, name, source, evidence in table:
                with self.subTest(field=name):
                    self.assertIn(source, ("code", "xmatch", "weak"))
                    self.assertTrue(evidence)

    def test_the_directly_read_offsets_stay_directly_read(self):
        # 这四个是从指令里直接读出来的，是整张表的锚。
        got = {n: o for o, n, s, _e in self.spec.ENEMY_FIELDS if s == "code"}
        self.assertEqual(
            got,
            {"position": 0x0B0, "flags": 0x0BC, "bulletProps": 0x26C, "shootOffset": 0xC5C},
        )

    def test_nc_reordered_the_struct_relative_to_th06(self):
        # TH06 的 shootOffset 排在 bulletProps 之前；NC 反过来。这条差异是
        # 「不能照抄 TH06 字段顺序」的直接证据，别让它被悄悄"修正"掉。
        by_name = {n: o for o, n, _s, _e in self.spec.ENEMY_FIELDS}
        self.assertGreater(by_name["shootOffset"], by_name["bulletProps"])

    def test_bullet_props_aim_mode_agrees_with_the_consumer(self):
        # 消费者 0x14000f870 里是 switch(param_2[0xc])，0xc*4 = 0x30
        by_name = {n: o for o, n, _s, _e in self.spec.BULLET_PROPS_FIELDS}
        self.assertEqual(by_name["aimMode"], 0x30)
        self.assertEqual(by_name["aimMode"], self.spec.BULLET_PROPS_AIM_MODE)


if __name__ == "__main__":
    unittest.main()
