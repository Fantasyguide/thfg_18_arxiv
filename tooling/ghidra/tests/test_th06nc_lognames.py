"""日志消息迁名的守门测试。

守三件事：
1. **出处**：每个名字都要能在 CC0 的 th06-decomp 源码里找到原形。
   th-re-data / th06-re 都没有 LICENSE，它们的名字不许混进来。
2. **那条假阳性**：0x1400454f0 曾被匹配器判成 SoundPlayer__InitializeDSound，
   实为 WinMain（内联骗过去的）。这条更正必须留着，别再被"发现"一次。
3. **自证**：AnmManager__LoadAnm 是 ANM 那一轮独立命名的，匹配器不知情地复现了它。
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


class Th06ncLogNameTests(unittest.TestCase):
    def setUp(self):
        self.spec = load("th06nc_logname_spec")

    def test_spec_self_check_passes(self):
        self.assertEqual(self.spec.validate(), [])

    def test_names_come_from_the_cc0_upstream(self):
        if not CC0_SRC.is_dir():
            self.skipTest("th06-decomp not cloned (gitignored)")
        blob = "\n".join(
            p.read_text(encoding="utf-8", errors="replace")
            for p in list(CC0_SRC.glob("*.cpp")) + list(CC0_SRC.glob("*.hpp")))
        for _a, name, _c, _k, _m in self.spec.LOG_NAMES:
            with self.subTest(name=name):
                cls, _, method = name.partition("__")
                self.assertRegex(
                    blob, r"\b%s\s*::\s*%s\b" % (re.escape(cls), re.escape(method)),
                    "%s 在 CC0 源码里找不到原形 —— 可能来自无 LICENSE 的上游" % name)

    def test_winmain_also_exists_upstream(self):
        if not CC0_SRC.is_dir():
            self.skipTest("th06-decomp not cloned (gitignored)")
        main_cpp = (CC0_SRC / "main.cpp").read_text(encoding="utf-8", errors="replace")
        self.assertIn("WinMain", main_cpp)

    def test_the_false_positive_stays_recorded(self):
        # 内联把匹配器骗了：0x1400454f0 带着 InitializeDSound 的消息，
        # 但它的调用者是 CRT 启动代码，是程序入口。别再配一次。
        addr, name, comment = self.spec.WINMAIN
        self.assertEqual(addr, 0x1400454F0)
        self.assertEqual(name, "WinMain")
        self.assertIn(addr, self.spec.REJECTED)
        self.assertIn("InitializeDSound", self.spec.REJECTED[addr])
        # 它不能同时出现在迁名表里
        self.assertNotIn(addr, [a for a, _n, _c, _k, _m in self.spec.LOG_NAMES])

    def test_the_self_validating_case_is_kept(self):
        # 整套手法唯一一次"在已知答案上复现"，值得钉住。
        by = {a: (n, m) for a, n, _c, _k, m in self.spec.LOG_NAMES}
        name, memo = by[0x1400020B0]
        self.assertEqual(name, "AnmManager__LoadAnm")
        self.assertIn("self-validation", memo)

    def test_the_inlined_case_explains_what_got_inlined(self):
        by = {a: (n, k, m) for a, n, _c, k, m in self.spec.LOG_NAMES}
        name, kind, memo = by[0x14003A9C0]
        self.assertEqual(name, "GameManager__AddedCallback")
        self.assertEqual(kind, "inlined")
        self.assertIn("EclManager::Load", memo)

    def test_all_comments_stay_ascii(self):
        # 会写进 Ghidra 并随 th06nc-re-data 发布
        for _a, _n, _c, _k, memo in self.spec.LOG_NAMES:
            self.assertTrue(memo.isascii())
        self.assertTrue(self.spec.WINMAIN[2].isascii())
