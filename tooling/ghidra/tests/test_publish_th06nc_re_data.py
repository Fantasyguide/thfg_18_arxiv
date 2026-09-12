"""发布器的守门测试。

放在 `tooling/ghidra/tests/` 是因为 `publish_th06nc_re_data.py` 直接吃
`tooling/ghidra/th06nc_*_spec.py`，跟着 spec 一起跑最省事。

这里守的主要是**发布安全**：一旦有人往 mapfile 里塞了上游的名字、
或者让本机路径 / 游戏文本漏进公开产物，测试要当场炸。
"""
import importlib.util
import json
import re
import unittest
from pathlib import Path


REPO = Path(__file__).resolve().parents[3]
PUBLISHER = REPO / "tooling" / "publish_th06nc_re_data.py"
SYMBOLS = REPO / "games" / "th06nc" / "symbols.json"


def load(path, name):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def build_payload():
    pub = load(PUBLISHER, "publish_th06nc_re_data")
    symbols = json.loads(SYMBOLS.read_text(encoding="utf-8"))
    return pub, symbols, pub.build(
        symbols,
        load(REPO / "tooling/ghidra/th06nc_anm_spec.py", "th06nc_anm_spec"),
        load(REPO / "tooling/ghidra/th06nc_ecl_spec.py", "th06nc_ecl_spec"),
    )


class PublishTh06ncReDataTests(unittest.TestCase):
    def setUp(self):
        if not SYMBOLS.is_file():
            self.skipTest("games/th06nc/symbols.json not present")
        self.pub, self.symbols, self.files = build_payload()

    def test_build_is_deterministic(self):
        _pub, _symbols, again = build_payload()
        self.assertEqual(self.files, again)

    def test_expected_files_are_produced(self):
        self.assertEqual(
            set(self.files),
            {
                "LICENSE",
                "README.md",
                "data/th06nc/funcs.json",
                "data/th06nc/statics.json",
                "data/th06nc/labels.json",
                "data/th06nc/comments.json",
                "mapfiles/th06nc.anmm",
                "mapfiles/th06nc.eclm",
            },
        )

    def test_iat_stub_comments_never_reach_the_public_payload(self):
        # Ghidra's PE import analyser writes "405  GetObjectA  <<not bound>>" all over
        # the IAT. They are not our notes and must not be published.
        for rel, text in self.files.items():
            with self.subTest(file=rel):
                self.assertNotIn("<<not bound>>", text)

    def test_anm_mapfile_only_carries_opcodes_we_derived_ourselves(self):
        # truth's mapfiles are marked personal-use, so the TH06 baseline names
        # (0..31) must NOT be copied in. Only NC's own additions belong here.
        anmm = self.files["mapfiles/th06nc.anmm"]
        ids = [int(m) for m in re.findall(r"^(\d+) \w+", anmm, re.M)]
        self.assertEqual(sorted(ids), [32, 33, 34, 35])
        for stolen in ("anchorTopLeft", "posTimeDecel", "interruptLabel", "scaleGrowth"):
            self.assertNotIn("\n%s" % stolen, anmm)

    def test_ecl_mapfile_carries_only_the_nc_deltas_and_stays_provisional(self):
        eclm = self.files["mapfiles/th06nc.eclm"]
        self.assertIn("!timeline_ins_names", eclm)
        self.assertIn("200 SHOOTOFFSETPOLAR", eclm)
        self.assertIn("201 BULLETPATTERNMODE134", eclm)
        self.assertIn("PROVISIONAL", eclm.upper())

    def test_payload_leaks_no_local_paths_or_non_ascii_game_text(self):
        for rel, text in self.files.items():
            if rel in ("README.md", "LICENSE"):
                continue          # 这两份是我们手写的英文散文
            with self.subTest(file=rel):
                self.assertTrue(text.isascii(), "非 ASCII 内容可能是游戏文本")
                for needle in ("/data/", "/home/", "sunyunbo", "C:\\"):
                    self.assertNotIn(needle, text)


    def test_th06nc_symbols_carry_no_unlicensed_upstream_names(self):
        """th06nc 的符号层必须**零**上游名。

        ⚠️ 这**不是**合规要求。用一个短名字（EnemyManager__SpawnEnemy 之类）
        不构成侵权，且 community-sources.md 早已记下「用户确认可研究/移植，
        发布时注明出处」。受保护的是**整份数据集**那种汇编形态，而迁名到 NC
        根本不搬 TH06 的地址。

        这条守的是**工程卫生**：保持「哪些是我们在 NC 字节上读出来的、
        哪些是从别处借的」可分。混进去之后事后再拆成本极高。
        另外那批名字若靠纯结构迁移而来，按 METHOD.md 只能算线索。

        策略见 engine/_shared/community-sources.md 的同名小节。
        """
        import json
        path = REPO / "games" / "th06nc" / "symbols.json"
        data = json.loads(path.read_text(encoding="utf-8"))
        tags = ("[th-re-data]", "[th06-re]")
        offenders = []
        for key, rows in data.items():
            if not isinstance(rows, list):
                continue
            for row in rows:
                if not isinstance(row, dict):
                    continue
                for field in ("comment", "text", "name"):
                    value = str(row.get(field, "") or "")
                    if value.startswith(tags):
                        offenders.append((key, row.get("addr"), value[:60]))
        self.assertEqual(
            offenders, [],
            "th06nc 的符号层混进了无 LICENSE 的上游名：%r" % (offenders[:5],))

    def test_wrong_sample_hash_is_refused(self):
        bad = dict(self.symbols, exe_md5="0" * 32)
        with self.assertRaises(SystemExit):
            self.pub.build(
                bad,
                load(REPO / "tooling/ghidra/th06nc_anm_spec.py", "th06nc_anm_spec"),
                load(REPO / "tooling/ghidra/th06nc_ecl_spec.py", "th06nc_ecl_spec"),
            )

    def test_license_is_cc0_and_disclaims_the_game_itself(self):
        text = self.files["LICENSE"]
        self.assertIn("CC0 1.0 Universal", text)
        self.assertIn("No game code, game", text)


if __name__ == "__main__":
    unittest.main()
