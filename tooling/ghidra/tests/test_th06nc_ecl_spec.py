import importlib.util
import unittest
from pathlib import Path


SPEC_PATH = Path(__file__).resolve().parents[1] / "th06nc_ecl_spec.py"


def load_spec():
    module_spec = importlib.util.spec_from_file_location("th06nc_ecl_spec", SPEC_PATH)
    module = importlib.util.module_from_spec(module_spec)
    module_spec.loader.exec_module(module)
    return module


class Th06ncEclSpecTests(unittest.TestCase):
    def test_plan_identifies_the_three_proven_core_functions(self):
        spec = load_spec()
        got = {address: name for address, name, _comment in spec.FUNCTIONS}
        self.assertEqual(
            got,
            {
                0x140022060: "EclManager__RunEcl",
                0x1400360D0: "EnemyManager__SpawnEnemy",
                0x140036260: "EnemyManager__RunEclTimeline",
            },
        )

    def test_th06_baseline_has_all_legacy_opcodes_and_keeps_nc_deltas_separate(self):
        spec = load_spec()
        self.assertEqual(len(spec.TH06_OPCODES), 136)
        self.assertEqual(spec.TH06_OPCODES[0], "NOP")
        self.assertEqual(spec.TH06_OPCODES[67], "BULLETFANAIMED")
        self.assertEqual(spec.TH06_OPCODES[135], "SPELLCARDFLAGTIMEOUT")
        self.assertNotIn(200, spec.TH06_OPCODES)
        self.assertEqual(spec.NC_ECL_DELTAS[200][0], "SHOOTOFFSETPOLAR")
        self.assertEqual(spec.NC_ECL_DELTAS[201][0], "BULLETPATTERNMODE134")

    def test_uncertain_nc_semantics_are_explicit_instead_of_presented_as_th06_facts(self):
        spec = load_spec()
        self.assertEqual(spec.NC_TIMELINE_DELTAS[14][0], "MARKER14")
        self.assertIn("no-op", spec.NC_TIMELINE_DELTAS[14][1].lower())
        self.assertEqual(spec.validate(), [])

    def test_opcode_201_is_recorded_as_an_unimplemented_mode_not_a_new_pattern(self):
        spec = load_spec()
        note = spec.NC_ECL_DELTAS[201][1]
        self.assertIn("unimplemented", note.lower())
        self.assertIn("0..8", note)
        # 消费侧的边界是这条结论的支点，别让它悄悄漂走
        self.assertEqual(spec.AIM_MODE_IMPLEMENTED, tuple(range(9)))
        self.assertEqual(spec.SPAWN_BULLET, 0x14000F870)

    def test_dispatch_shape_matches_what_the_jump_table_actually_says(self):
        spec = load_spec()
        self.assertEqual(spec.OPCODE_RANGE, (1, 201))
        self.assertEqual(spec.DISPATCH_BYTE_MAP, 0x140028F5C)
        self.assertEqual(spec.DISPATCH_JUMP_TABLE, 0x140028D78)
        # 121 是含默认出口的去重跳转目标数（TH06 同口径 120），真 handler 是 120
        self.assertEqual(spec.DISTINCT_JUMP_TARGETS, 121)
        self.assertEqual(spec.REAL_HANDLERS, 120)


if __name__ == "__main__":
    unittest.main()
