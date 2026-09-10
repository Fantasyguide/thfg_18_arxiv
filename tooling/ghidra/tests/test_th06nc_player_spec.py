"""玩家子系统 spec 的守门测试。

重点守两条**这轮实际踩过的坑**：
- `Player__BombProjectileClipsBox` 是我们自己起的名（CC0 把这段内联了），
  别让它以后被当成上游名字。
- `Player__CheckGraze` 多出来的那个参数是未解之处，别被悄悄写成已解。
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


class Th06ncPlayerSpecTests(unittest.TestCase):
    def setUp(self):
        self.spec = load("th06nc_player_spec")

    def test_the_collision_round_anchors_keep_their_addresses(self):
        # 碰撞那一轮定位的六个锚。后续几轮（射击 / bomb）往 FUNCTIONS 里加了更多，
        # 所以这里查的是「这六个还在且地址没变」，不是精确相等。
        got = {n: a for a, n, _c in self.spec.FUNCTIONS}
        for name, address in {
            "Player__OnUpdate": 0x140068820,
            "Player__CalcKillBoxCollision": 0x14006A980,
            "Player__AddedCallback": 0x140068160,
            "Player__BombProjectileClipsBox": 0x14006A8C0,
            "Player__CalcLaserHitbox": 0x14006ABA0,
            "BulletManager__OnUpdate": 0x140010870,
        }.items():
            with self.subTest(name=name):
                self.assertEqual(got.get(name), address)

    def test_the_function_table_does_not_silently_shrink(self):
        # 上面改成子集查之后，需要另一道闸防止有人删条目而没人发现。
        # 数量变了就该有人来看一眼并改掉这个数。
        self.assertEqual(len(self.spec.FUNCTIONS), 19)
        addresses = [a for a, _n, _c in self.spec.FUNCTIONS]
        names = [n for _a, n, _c in self.spec.FUNCTIONS]
        self.assertEqual(len(addresses), len(set(addresses)))
        self.assertEqual(len(names), len(set(names)))

    def test_our_own_name_stays_marked_as_ours(self):
        by_name = {n: c for _a, n, c in self.spec.FUNCTIONS}
        self.assertIn("our own name", by_name["Player__BombProjectileClipsBox"])

    def test_the_misnaming_correction_is_kept(self):
        # 0x14006a980 一度被我们命名成 CheckGraze。它放的是音效 4（PICHUN，死亡）
        # 而不是 30（GRAZE）。注释里必须留着这条更正，旧名要留在 OUR_OLD_NAMES 里。
        by_name = {n: c for _a, n, c in self.spec.FUNCTIONS}
        self.assertIn("NOT graze", by_name["Player__CalcKillBoxCollision"])
        self.assertIn("Player__CheckGraze", self.spec.OUR_OLD_NAMES)

    def test_the_hitbox_shape_difference_is_not_flattened(self):
        # 这轮最重要的结论：形状变了（盒 -> 圆）而数值没变（都是 1.25）。
        # 两条都容易被后来的人"顺手改正"成错的，所以钉住。
        self.assertEqual(self.spec.HITBOX_SHAPE, "circle")
        self.assertEqual(self.spec.HITBOX_SHAPE_TH06, "aabb")
        self.assertEqual(self.spec.HITBOX_RADIUS, self.spec.HITBOX_HALF_EXTENT_TH06)

    def test_graze_is_also_circular_and_reuses_the_same_radius_field(self):
        self.assertEqual(self.spec.GRAZE_SHAPE, "circle")
        self.assertEqual(self.spec.GRAZE_SHAPE_TH06, "aabb")
        # 外扩 20 两边相同 —— 变的是形状，不是这个数
        self.assertEqual(self.spec.GRAZE_MARGIN, self.spec.GRAZE_MARGIN_TH06)
        by_name = {n: o for o, n, _s, _e in self.spec.PLAYER_FIELDS}
        self.assertEqual(by_name["hitboxRadius"], 0x774C)

    def test_the_only_graze_number_that_changed_is_the_stage_cap(self):
        self.assertNotEqual(self.spec.GRAZE_STAGE_CAP, self.spec.GRAZE_STAGE_CAP_TH06)
        self.assertEqual(self.spec.GRAZE_STAGE_CAP, 99999)
        self.assertEqual(self.spec.GRAZE_TOTAL_CAP, self.spec.GRAZE_TOTAL_CAP_TH06)
        self.assertEqual(self.spec.GRAZE_SCORE, 500)

    def test_player_state_offset_correction_is_kept(self):
        # +0x773c 曾被本仓误标成 playerState；真的在 +0x7898。
        by_name = {n: o for o, n, _s, _e in self.spec.PLAYER_FIELDS}
        self.assertEqual(by_name["playerState"], 0x7898)
        self.assertIn("unk_773c", by_name)

    def test_spawn_positions_match_th06_on_both_paths(self):
        # 曾经把 NC 的复活路径与 CC0 的初始化路径对比，误报出一个不存在的差异。
        self.assertEqual(self.spec.SPAWN_POSITION, (192.0, 384.0, 0.49))
        self.assertEqual(self.spec.RESPAWN_POSITION, (192.0, 384.0, 0.2))

    def test_button_bits_match_the_input_layer(self):
        self.assertEqual(self.spec.BUTTON_BITS[0x01], "SHOOT")
        self.assertEqual(self.spec.BUTTON_BITS[0x02], "BOMB")
        self.assertEqual(self.spec.BUTTON_BITS[0x04], "FOCUS")

    def test_player_fields_are_evidenced(self):
        for _offset, name, source, evidence in self.spec.PLAYER_FIELDS:
            with self.subTest(field=name):
                self.assertIn(source, ("code", "weak"))
                self.assertTrue(evidence)
                # 没定下来的字段必须叫 unk_*，别用一个像模像样的名字掩盖不确定性
                if source == "weak":
                    self.assertTrue(name.startswith("unk_"), name)
                else:
                    self.assertFalse(name.startswith("unk_"), name)

    def test_spec_self_check_passes(self):
        self.assertEqual(self.spec.validate(), [])


if __name__ == "__main__":
    unittest.main()
