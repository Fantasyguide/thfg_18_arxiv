"""ECL 主分派表解码的守门测试。

映射是**从 exe 现场解**的，所以这些测试同时也是「exe 还是那一份、表还在那儿」的哨兵。
样本不在时整类跳过。
"""
import hashlib
import importlib.util
import unittest
from pathlib import Path


REPO = Path(__file__).resolve().parents[3]
SAMPLE = REPO / "local" / "th06nc" / "th06nc.exe"
CC0_HEADER = REPO / "local" / "vendor" / "th06-decomp" / "src" / "EclManager.hpp"
EXPECTED_MD5 = "b0cc28ec904e9efce2439ad857c938b5"


def load(name):
    path = REPO / "tooling" / "ghidra" / f"{name}.py"
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


class Th06ncEclOpcodeTableTests(unittest.TestCase):
    def setUp(self):
        if not SAMPLE.exists():
            self.skipTest("sample %s not present (gitignored)" % SAMPLE)
        self.data = SAMPLE.read_bytes()
        if hashlib.md5(self.data).hexdigest() != EXPECTED_MD5:
            self.skipTest("sample hash differs from the recorded build")
        self.ops = load("th06nc_ecl_opcodes")
        self.spec = load("th06nc_ecl_spec")
        self.mapping = self.ops.decode(self.data)

    def test_dispatch_covers_exactly_opcodes_1_to_201(self):
        self.assertEqual(min(self.mapping), 1)
        self.assertEqual(max(self.mapping), 201)
        self.assertEqual(len(self.mapping), 201)

    def test_unimplemented_set_is_127_plus_136_to_199(self):
        # 127 = DEBUGWATCH, which TH06 leaves unimplemented too.
        self.assertEqual(
            self.ops.unimplemented(self.mapping), [127] + list(range(136, 200))
        )

    def test_real_handler_count_matches_the_spec(self):
        groups = self.ops.group(self.mapping, self.spec.TH06_OPCODES, self.spec.NC_ECL_DELTAS)
        self.assertEqual(len(groups), self.spec.REAL_HANDLERS)
        targets = {va for _slot, va in self.mapping.values()}
        self.assertEqual(len(targets), self.spec.DISTINCT_JUMP_TARGETS)

    def test_opcode_201_shares_the_bullet_handler_with_67_through_75(self):
        groups = {tuple(ops): va for va, ops, _l, _c in
                  self.ops.group(self.mapping, self.spec.TH06_OPCODES, self.spec.NC_ECL_DELTAS)}
        self.assertIn(tuple(list(range(67, 76)) + [201]), groups)
        self.assertEqual(groups[tuple(list(range(67, 76)) + [201])], 0x140025371)

    def test_opcode_200_has_a_handler_of_its_own(self):
        self.assertEqual(self.mapping[200][1], 0x140026A41)
        solo = [ops for _va, ops, _l, _c in
                self.ops.group(self.mapping, self.spec.TH06_OPCODES, self.spec.NC_ECL_DELTAS)
                if ops == [200]]
        self.assertEqual(solo, [[200]])

    def test_int_and_float_math_pairs_share_handlers(self):
        # 13/20 ADD, 14/21 SUB, 15/22 MUL, 16/23 DIV, 17/24 MOD —— 这组共用关系
        # 是「NC 与 TH06 同源」的一个结构指纹，漂了就该有人来看一眼。
        for int_op, float_op in ((13, 20), (14, 21), (15, 22), (16, 23), (17, 24)):
            with self.subTest(pair=(int_op, float_op)):
                self.assertEqual(self.mapping[int_op][1], self.mapping[float_op][1])

    def test_labels_are_unique_and_readable(self):
        groups = self.ops.group(self.mapping, self.spec.TH06_OPCODES, self.spec.NC_ECL_DELTAS)
        labels = [label for _va, _ops, label, _c in groups]
        self.assertEqual(len(labels), len(set(labels)))
        for label in labels:
            self.assertRegex(label, r"^ECL_OP_\d{3}_[A-Z0-9]+$")


class Th06ncEclNameProvenanceTests(unittest.TestCase):
    def test_opcode_names_match_the_cc0_upstream_verbatim(self):
        if not CC0_HEADER.is_file():
            self.skipTest("th06-decomp not cloned (gitignored)")
        ops = load("th06nc_ecl_opcodes")
        spec = load("th06nc_ecl_spec")
        errors = ops.validate_names(
            spec.TH06_OPCODES, CC0_HEADER.read_text(encoding="utf-8", errors="replace")
        )
        self.assertEqual(errors, [])


if __name__ == "__main__":
    unittest.main()
