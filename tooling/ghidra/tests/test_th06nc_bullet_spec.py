"""敌弹子系统 spec 的守门测试。

重点守**三条互相独立的自洽关系**——它们是整套偏移的地基，
任何一个数取错都会立刻塌，比逐个字段断言有效得多。
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


class Th06ncBulletPoolTests(unittest.TestCase):
    def setUp(self):
        self.spec = load("th06nc_bullet_spec")

    def test_spec_self_check_passes(self):
        self.assertEqual(self.spec.validate(), [])

    def test_bullet_array_ends_exactly_at_the_count_field(self):
        s = self.spec
        self.assertEqual(s.BULLETS_OFFSET + s.BULLET_COUNT * s.BULLET_SIZE,
                         s.BULLET_COUNT_FIELD)

    def test_laser_base_agrees_with_the_immediate_in_spawnlaserpattern(self):
        s = self.spec
        self.assertEqual(s.G_BULLET_MANAGER + s.LASERS_OFFSET, s.LASERS_ABS)

    def test_the_onupdate_cursor_lands_on_the_same_inuse_byte(self):
        # OnUpdate 用 mgr+0xf502c 当游标，SpawnLaserPattern 用 laser+0x274 判占用。
        # 两处独立，必须指向同一字节 —— 这是激光池基址最硬的一条交叉证据。
        s = self.spec
        cursor = 0xF502C - s.LASERS_OFFSET          # = 0x14
        self.assertEqual(cursor + 0x260, s.LASER_IN_USE)

    def test_counts_did_not_change_but_sizes_did(self):
        # NC 把两个结构体都撑大了，但池的容量与 TH06 相同。
        # 谁要是"顺手"把 size 改回 TH06 的值，这条就炸。
        s = self.spec
        self.assertEqual(s.BULLET_COUNT, s.BULLET_COUNT_TH06)
        self.assertEqual(s.LASER_COUNT, s.LASER_COUNT_TH06)
        self.assertGreater(s.BULLET_SIZE, s.BULLET_SIZE_TH06)
        self.assertGreater(s.LASER_SIZE, s.LASER_SIZE_TH06)
        self.assertEqual(s.BULLET_SIZE, 0x620)
        self.assertEqual(s.LASER_SIZE, 0x298)

    def test_every_field_fits_inside_its_struct(self):
        s = self.spec
        for table, size in ((s.BULLET_FIELDS, s.BULLET_SIZE),
                            (s.LASER_FIELDS, s.LASER_SIZE)):
            for offset, name, _src, _ev in table:
                with self.subTest(field=name):
                    self.assertLess(offset, size)

    def test_bullet_states_cover_the_switch(self):
        s = self.spec
        self.assertEqual(s.BULLET_STATE_UNUSED, 0)
        self.assertEqual(s.BULLET_STATE_FIRED, 1)
        self.assertEqual(s.BULLET_STATES_SPAWNING, (2, 3, 4))
        self.assertEqual(s.BULLET_STATE_DESPAWNING, 5)
        # 三档出现动画各有自己的 AnmVm 与速度分母，别把它们合并成一档
        self.assertEqual(sorted(s.SPAWN_VM_OFFSETS), list(s.BULLET_STATES_SPAWNING))
        self.assertEqual(sorted(s.SPAWN_DIVISOR_ADDRS), list(s.BULLET_STATES_SPAWNING))
        self.assertEqual(len(set(s.SPAWN_VM_OFFSETS.values())), 3)
        self.assertEqual(len(set(s.SPAWN_DIVISOR_ADDRS.values())), 3)

    def test_laser_offset_triplet_is_coherent(self):
        # startOffset 被 endOffset - startLength 追着走 —— 三者必须都在表里
        by = {n: o for o, n, _s, _e in self.spec.LASER_FIELDS}
        for name in ("startOffset", "endOffset", "startLength"):
            self.assertIn(name, by)
        self.assertEqual(by["startOffset"] + 4, by["endOffset"])

    def test_pool_labels_match_the_offsets(self):
        s = self.spec
        by = {n: a for a, n, _c in s.POOL_LABELS}
        self.assertEqual(by["g_Bullets"], s.G_BULLET_MANAGER + s.BULLETS_OFFSET)
        self.assertEqual(by["g_BulletCount"], s.G_BULLET_MANAGER + s.BULLET_COUNT_FIELD)
        self.assertEqual(by["g_Lasers"], s.LASERS_ABS)

    def test_applied_comments_stay_ascii(self):
        # 会写进 Ghidra 并随 th06nc-re-data 发布 —— 非 ASCII 会被发布安全测试拦下。
        for _a, name, comment in self.spec.FUNCTIONS:
            self.assertTrue(comment.isascii(), name)
        for _a, name, comment in self.spec.POOL_LABELS:
            self.assertTrue(comment.isascii(), name)


if __name__ == "__main__":
    unittest.main()
