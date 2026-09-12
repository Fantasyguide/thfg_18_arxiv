"""敌人子系统 spec 的守门测试。

守两类东西：
- **池的自洽**：池尾必须在模板之前，且中间留得下已独立定位的 spellcardInfo。
  任一个立即数取错都会破坏它。
- **那条跨版本差异**：NC 的 SpawnEnemy 多一个参数、会让子敌人继承父敌人的 ECL 变量。
  TH06 的 RET 0x14 证明它只有 5 个栈参 —— 这条极易被「照抄 TH06」抹平。
"""
import importlib.util
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parents[3]


def load(name):
    path = REPO / "tooling" / "ghidra" / f"{name}.py"
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


class Th06ncEnemyPoolTests(unittest.TestCase):
    def setUp(self):
        self.spec = load("th06nc_enemy_spec")

    def test_spec_self_check_passes(self):
        self.assertEqual(self.spec.validate(), [])

    def test_pool_leaves_room_for_the_independently_located_spellcard_info(self):
        s = self.spec
        pool_end = s.ENEMIES_OFFSET + s.ENEMY_COUNT * s.ENEMY_SIZE
        self.assertEqual(pool_end, 0x10B008)
        # spellcardInfo 与 timeline 指针是别的轮次独立定位的，必须落在这段空档里
        for probe in (0x10C0D0, 0x10C0D4, 0x10C128):
            self.assertLess(pool_end, probe)
            self.assertLess(probe, s.ENEMY_TEMPLATE_OFFSET)

    def test_count_unchanged_size_grew_and_pool_template_swapped(self):
        s = self.spec
        self.assertEqual(s.ENEMY_COUNT, s.ENEMY_COUNT_TH06)
        self.assertEqual(s.ENEMY_SIZE, 0x10B0)
        self.assertEqual(s.ENEMY_SIZE_TH06, 0xEC8)
        # 两边把池与模板对调了 —— 这条形状差异别被写平
        self.assertEqual(s.ENEMIES_OFFSET, s.ENEMY_TEMPLATE_OFFSET_TH06)
        self.assertGreater(s.ENEMY_TEMPLATE_OFFSET, s.ENEMIES_OFFSET)
        self.assertGreater(s.ENEMIES_OFFSET_TH06, s.ENEMY_TEMPLATE_OFFSET_TH06)

    def test_slot_flag_is_bit7_on_both_versions(self):
        # 偏移不同（0xbc vs 0xe50），但都是 bit7 —— 语义没变，位置变了
        s = self.spec
        self.assertNotEqual(s.ENEMY_SLOT_FLAGS, s.ENEMY_SLOT_FLAGS_TH06)

    def test_ecl_variable_inheritance_is_an_nc_addition(self):
        s = self.spec
        self.assertTrue(s.SPAWN_ARG7_IS_PARENT_CONTEXT)
        self.assertEqual(s.ECL_VAR_INHERIT_LEN, 14)
        # 源与目标必须落在 EclContext 的同一偏移上 —— 这是「整块继承」而非「搬家」的判据
        self.assertEqual(s.ECL_CONTEXT_OFFSET + s.ECL_VAR_INHERIT_SRC,
                         s.ECL_VAR_INHERIT_DST)
        # 只有 ECL VM 那一处传父上下文，6 处 timeline 都传 0
        self.assertEqual(len(s.SPAWN_CALLSITES_TIMELINE), 6)
        self.assertNotIn(s.SPAWN_CALLSITE_WITH_PARENT, s.SPAWN_CALLSITES_TIMELINE)

    def test_ecl_context_boundaries_come_from_three_independent_writes(self):
        # +0x00 ZunTimer、+0x40 subId、+0x48 currentInstr —— 三处独立命中才定住了边界。
        by = {n: o for o, n, _s, _e in self.spec.ECL_CONTEXT_FIELDS}
        self.assertEqual(by["time.previous"], 0x00)
        self.assertEqual(by["subId"], 0x40)
        self.assertEqual(by["currentInstr"], 0x48)
        self.assertEqual(by["vars"], 0x08)

    def test_maxlife_was_upgraded_and_the_extra_write_stays_unknown(self):
        s = self.spec
        by = {n: (o, src) for o, n, src, _e in s.SPAWN_WRITES}
        self.assertEqual(by["maxLife"], (0x238, "code"))
        # +0x23c 是 NC 比 TH06 多写的一份 life，语义未知 —— 别给它安名字
        self.assertEqual(by["unk_23c"], (0x23C, "code-unk"))

    def test_applied_comment_stays_ascii(self):
        for _a, name, comment in self.spec.FUNCTIONS:
            self.assertTrue(comment.isascii(), name)
