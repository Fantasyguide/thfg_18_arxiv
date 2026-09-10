"""非日志字符串迁名的守门测试。

守五件事：
1. **出处**：每个名字都要能在 CC0 的 th06-decomp 源码里找到原形。
   th-re-data / th06-re 都没有 LICENSE，它们的名字不许混进来。
2. **判据 (b) 的修复**：上一轮放跑 WinMain，是因为"自由函数消息也算冲突"没实现。
   本轮实现了，0x1400454f0 必须留在 REJECTED 里。
3. **判据 (c)**：0x1400479e0 按票数会判成 LoadDiffCharSelect，按 caller-closure
   才是 OnUpdate —— 而上一轮用一条日志消息独立地也判成 OnUpdate。这条自证要钉住。
4. **不与已入库的地址打架**：本轮的新名字不能碰上一轮/别的轮次已经占掉的地址。
5. **注释 ASCII**：会写进 Ghidra 并随 th06nc-re-data 发布。
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


class Th06ncStrNameTests(unittest.TestCase):
    def setUp(self):
        self.spec = load("th06nc_strname_spec")

    def test_spec_self_check_passes(self):
        self.assertEqual(self.spec.validate(), [])

    def test_names_come_from_the_cc0_upstream(self):
        if not CC0_SRC.is_dir():
            self.skipTest("th06-decomp not cloned (gitignored)")
        blob = "\n".join(
            p.read_text(encoding="utf-8", errors="replace")
            for p in list(CC0_SRC.glob("*.cpp")) + list(CC0_SRC.glob("*.hpp")))
        for _a, name, _c, _m in self.spec.STR_NAMES:
            with self.subTest(name=name):
                cls, _, method = name.partition("__")
                self.assertRegex(
                    blob, r"\b%s\s*::\s*%s\b" % (re.escape(cls), re.escape(method)),
                    "%s 在 CC0 源码里找不到原形 —— 可能来自无 LICENSE 的上游" % name)

    def test_the_free_function_conflict_rule_keeps_winmain_out(self):
        # 判据 (b)。上一轮靠人工复核才拦下它；本轮判据自动拒绝。
        self.assertIn(0x1400454F0, self.spec.REJECTED)
        self.assertIn("WinMain", self.spec.REJECTED[0x1400454F0])
        self.assertNotIn(0x1400454F0, [a for a, _n, _c, _m in self.spec.STR_NAMES])

    def test_the_caller_closure_case_is_kept_as_self_validation(self):
        # 判据 (c)。票数规则会判错，closure 规则复现了上一轮的答案。
        self.assertEqual(self.spec.REPRODUCED[0x1400479E0], "MainMenu__OnUpdate")
        logspec = load("th06nc_logname_spec")
        by_addr = {a: n for a, n, _c, _k, _m in logspec.LOG_NAMES}
        self.assertEqual(by_addr[0x1400479E0], "MainMenu__OnUpdate")

    def test_reproduced_addresses_match_the_previous_round(self):
        logspec = load("th06nc_logname_spec")
        by_addr = {a: n for a, n, _c, _k, _m in logspec.LOG_NAMES}
        for addr, name in self.spec.REPRODUCED.items():
            if addr in by_addr:
                with self.subTest(addr=hex(addr)):
                    self.assertEqual(by_addr[addr], name)

    def test_new_names_do_not_collide_with_earlier_rounds(self):
        taken = {}
        logspec = load("th06nc_logname_spec")
        for a, n, _c, _k, _m in logspec.LOG_NAMES:
            taken[a] = n
        taken[logspec.WINMAIN[0]] = logspec.WINMAIN[1]
        for module in ("th06nc_ecl_spec", "th06nc_menu_spec", "th06nc_anm_spec",
                       "th06nc_bullet_spec", "th06nc_enemy_spec", "th06nc_player_spec"):
            mod = load(module)
            for attr in ("FUNCTIONS", "ECL_CALLEE_NAMES"):
                for row in getattr(mod, attr, []):
                    if isinstance(row[0], int):
                        taken[row[0]] = row[1]
        for addr, name, _c, _m in self.spec.STR_NAMES:
            with self.subTest(name=name):
                self.assertNotIn(addr, taken,
                                 "0x%x 已经被 %s 占了" % (addr, taken.get(addr)))

    def test_every_name_carries_a_second_line_of_evidence(self):
        # METHOD:「光有匹配不够」。第二重证据是人工复核出来的调用图/结构事实。
        for _a, name, _c, memo in self.spec.STR_NAMES:
            with self.subTest(name=name):
                self.assertTrue(memo.isascii())
                self.assertGreater(len(memo), 40, "%s 的第二重证据太短" % name)

    def test_tiebreaks_are_registered_with_their_loser(self):
        # 靠 chain 回调结构签名拆开的平票，输的那一边必须登记在 REJECTED 里。
        for winner, memo in self.spec.TIEBREAKS.items():
            with self.subTest(winner=hex(winner)):
                loser = int(memo.split("vs")[1].strip(), 16)
                self.assertIn(loser, self.spec.REJECTED)
