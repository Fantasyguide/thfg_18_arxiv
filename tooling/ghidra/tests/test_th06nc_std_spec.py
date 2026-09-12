"""TH06NC STD 运行时 spec 的守门测试。

守四件事：
1. **spec 自检**通过（地址不重叠、opcode 正好 0..5、注释 ASCII）。
2. **名字出处**：函数名要能在 CC0 的 th06-decomp 里找到原形。
3. **那条更正**：0x1400785c0 曾被日志消息那轮命名为 Stage__LoadStageData，
   实为 Stage::AddedCallback。更正必须留着，且不能与旧 spec 打架。
4. **NC 独有的 opcode 3 魔数**要用**样本数据**复核：stage7.std 里确实有 -1/-2，
   其余六个 std 里一条都没有。数据一变这条就会失败。
"""
import importlib.util
import re
import struct
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parents[3]
CC0_SRC = REPO / "local" / "vendor" / "th06-decomp" / "src"
EXTRACTED = REPO / "local" / "th06nc" / "extracted"


def load(name):
    path = REPO / "tooling" / "ghidra" / f"{name}.py"
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def std_instructions(path):
    """按 NC 的 Stage__OnUpdate 读法解析：记录 0x14 字节，opcode 是 +0x04 的 i16。"""
    data = path.read_bytes()
    _nobj, _nface, _faces, script_offset, _unk = struct.unpack_from("<hhiii", data, 0)
    out, pos = [], script_offset
    while pos + 0x14 <= len(data):
        frame, opcode, size = struct.unpack_from("<ihh", data, pos)
        if not 0 <= opcode < 6:
            break
        out.append((frame, opcode, size, struct.unpack_from("<3i", data, pos + 8)))
        pos += 0x14
    return out


class Th06ncStdSpecTests(unittest.TestCase):
    def setUp(self):
        self.spec = load("th06nc_std_spec")

    def test_spec_self_check_passes(self):
        self.assertEqual(self.spec.validate(), [])

    def test_function_names_come_from_the_cc0_upstream(self):
        if not CC0_SRC.is_dir():
            self.skipTest("th06-decomp not cloned (gitignored)")
        blob = "\n".join(
            p.read_text(encoding="utf-8", errors="replace")
            for p in list(CC0_SRC.glob("*.cpp")) + list(CC0_SRC.glob("*.hpp")))
        names = [n for _a, n, _c in self.spec.FUNCTIONS] + [r[2] for r in self.spec.RENAMES]
        for name in names:
            with self.subTest(name=name):
                cls, _, method = name.partition("__")
                self.assertRegex(
                    blob, r"\b%s\s*::\s*%s\b" % (re.escape(cls), re.escape(method)),
                    "%s 在 CC0 源码里找不到原形" % name)

    def test_the_addedcallback_correction_is_recorded(self):
        by_addr = {r[0]: r for r in self.spec.RENAMES}
        self.assertIn(0x1400785C0, by_addr)
        _a, old, new, memo = by_addr[0x1400785C0]
        self.assertEqual(old, "Stage__LoadStageData")
        self.assertEqual(new, "Stage__AddedCallback")
        self.assertIn("g_StageFiles", memo)
        # 日志消息那轮的 spec 必须已经跟着改过来，否则两个 apply 会互相覆盖。
        logspec = load("th06nc_logname_spec")
        fixed = {a: n for a, n, _c, _k, _m in logspec.LOG_NAMES}
        self.assertEqual(fixed.get(0x1400785C0), "Stage__AddedCallback")
        self.assertIn(0x1400785C0, logspec.CORRECTED)

    def test_opcode_set_matches_the_cc0_enum(self):
        if not CC0_SRC.is_dir():
            self.skipTest("th06-decomp not cloned (gitignored)")
        header = (CC0_SRC / "Stage.hpp").read_text(encoding="utf-8", errors="replace")
        enum = re.search(r"enum StageOpcode\s*\{(.*?)\}", header, re.S).group(1)
        cc0 = [m.strip() for m in re.findall(r"(STDOP_\w+)", enum)]
        ours = ["STDOP_" + n[len("STD_OP_"):] for _o, _a, n, _c in self.spec.OPCODES]
        self.assertEqual(ours, cc0, "NC 的跳表顺序必须与 CC0 的 StageOpcode 枚举一致")

    def test_the_nc_only_loop_opcode_shows_up_in_the_shipped_scripts(self):
        files = sorted(EXTRACTED.glob("*/stage*.std"))
        if not files:
            self.skipTest("stage*.std not extracted (gitignored)")
        self.assertEqual(len(files), 7)
        magic = {}
        for path in files:
            hits = [(i, frame, args[0])
                    for i, (frame, opcode, _size, args) in enumerate(std_instructions(path))
                    if opcode == 3 and args[0] in (-1, -2)]
            if hits:
                magic[path.name] = hits
        # 只有 Extra 关用了它，而且正好一对：先 -1 记点，后 -2 跳回
        self.assertEqual(list(magic), ["stage7.std"])
        (i_save, f_save, a_save), (i_jump, f_jump, a_jump) = magic["stage7.std"]
        self.assertEqual((a_save, a_jump), (-1, -2))
        self.assertLess(i_save, i_jump)
        self.assertLess(f_save, f_jump)

    def test_no_std_script_uses_an_opcode_outside_0_to_5(self):
        files = sorted(EXTRACTED.glob("*/stage*.std"))
        if not files:
            self.skipTest("stage*.std not extracted (gitignored)")
        for path in files:
            rows = std_instructions(path)
            with self.subTest(path=path.name):
                self.assertTrue(rows)
                self.assertTrue(all(0 <= op < 6 for _f, op, _s, _a in rows))
