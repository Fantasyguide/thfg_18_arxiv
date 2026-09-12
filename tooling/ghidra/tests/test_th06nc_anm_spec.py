import hashlib
import importlib.util
import struct
import subprocess
import sys
import unittest
from pathlib import Path


REPO = Path(__file__).resolve().parents[3]
SPEC_PATH = REPO / "tooling" / "ghidra" / "th06nc_anm_spec.py"
SCANNER = REPO / "tooling" / "anm0_scan.py"
SAMPLE = REPO / "local" / "th06nc" / "th06nc.exe"
EXTRACTED = REPO / "local" / "th06nc" / "extracted"


def load_spec():
    module_spec = importlib.util.spec_from_file_location("th06nc_anm_spec", SPEC_PATH)
    module = importlib.util.module_from_spec(module_spec)
    module_spec.loader.exec_module(module)
    return module


class Th06ncAnmSpecTests(unittest.TestCase):
    def test_baseline_is_th06s_thirty_two_opcodes(self):
        spec = load_spec()
        self.assertEqual(len(spec.TH06_OPCODES), 32)
        self.assertEqual(spec.TH06_OPCODES[0], "Exit")
        self.assertEqual(spec.TH06_OPCODES[15], "ExitHide")
        self.assertEqual(spec.TH06_OPCODES[22], "InterruptLabel")
        self.assertEqual(spec.TH06_OPCODES[31], "Op31")

    def test_nc_adds_exactly_four_opcodes_up_to_the_dispatch_bound(self):
        spec = load_spec()
        self.assertEqual(spec.NC_ONLY_OPCODES, (32, 33, 34, 35))
        self.assertEqual(spec.MAX_OPCODE, 35)

    def test_anchor_opcodes_are_absolute_assignments(self):
        spec = load_spec()
        for op, (_addr, and_mask, or_mask, _name, field) in spec.ANCHOR_OPCODES.items():
            with self.subTest(opcode=op):
                # Every anchor bit is either force-cleared or force-set.
                self.assertEqual(spec.ANCHOR_FIELD_MASK & and_mask & ~or_mask, 0)
                self.assertEqual(or_mask >> spec.ANCHOR_FIELD_SHIFT, field)

    def test_opcode_23_still_means_top_left_despite_the_new_encoding(self):
        spec = load_spec()
        # TH06 wrote bits 8|9 for top-left; NC writes bit 10 instead. The opcode
        # meaning is preserved even though the bit pattern is not.
        self.assertEqual(spec.ANCHOR_OPCODES[23][1:3], (0xFFFFFCFF, 0x400))
        self.assertEqual(spec.ANCHOR_FIELD[spec.ANCHOR_OPCODES[23][4]], ("left", "top"))
        self.assertEqual(spec.ANCHOR_FIELD[3], ("center", "top"))

    def test_spec_self_check_passes(self):
        self.assertEqual(load_spec().validate(), [])


class Th06ncAnmSampleTests(unittest.TestCase):
    """Re-derives the spec straight from the sample. Skipped when it is absent."""

    def setUp(self):
        if not SAMPLE.exists():
            self.skipTest("sample %s not present (gitignored)" % SAMPLE)
        self.data = SAMPLE.read_bytes()
        self.spec = load_spec()

    def va_to_file_offset(self, va):
        data = self.data
        pe = struct.unpack_from("<I", data, 0x3C)[0]
        section_count = struct.unpack_from("<H", data, pe + 6)[0]
        optional_size = struct.unpack_from("<H", data, pe + 20)[0]
        image_base = struct.unpack_from("<Q", data, pe + 24 + 24)[0]
        rva = va - image_base
        for index in range(section_count):
            header = pe + 24 + optional_size + index * 40
            virtual_size, virtual_address, raw_size, raw_pointer = struct.unpack_from(
                "<IIII", data, header + 8
            )
            if virtual_address <= rva < virtual_address + max(virtual_size, raw_size):
                return raw_pointer + (rva - virtual_address)
        raise AssertionError("VA 0x%x is outside every section" % va)

    def test_sample_matches_the_expected_build(self):
        self.assertEqual(hashlib.md5(self.data).hexdigest(), self.spec.EXPECTED_MD5)

    def test_dispatch_is_bounded_at_opcode_35(self):
        # 140006a50  CMP DL,0x23   ->  80 fa 23
        offset = self.va_to_file_offset(0x140006A50)
        self.assertEqual(self.data[offset : offset + 3], b"\x80\xfa\x23")
        self.assertEqual(self.data[offset + 2], self.spec.MAX_OPCODE)

    def test_jump_table_targets_match_the_documented_handlers(self):
        table = self.va_to_file_offset(self.spec.OPCODE_JUMP_TABLE)
        targets = {}
        for op in range(self.spec.MAX_OPCODE + 1):
            rva = struct.unpack_from("<I", self.data, table + op * 4)[0]
            targets[op] = 0x140000000 + rva
        for op, (addr, _a, _o, _n, _f) in self.spec.ANCHOR_OPCODES.items():
            with self.subTest(opcode=op):
                self.assertEqual(targets[op], addr)
        # Execution-time no-ops jump straight to the advance tail.
        self.assertEqual(targets[6], self.spec.ADVANCE_TAIL)
        self.assertEqual(targets[22], self.spec.ADVANCE_TAIL)
        # PosTimeLinear/Decel/Accel share one handler.
        self.assertEqual(targets[18], targets[19])
        self.assertEqual(targets[19], targets[20])

    def test_anchor_handler_bytes_are_the_documented_and_or_pair(self):
        for op, (addr, and_mask, or_mask, _n, _f) in self.spec.ANCHOR_OPCODES.items():
            with self.subTest(opcode=op):
                off = self.va_to_file_offset(addr)
                got = self.data[off : off + 22]
                # 41 81 a3 c4 00 00 00 <imm32>   AND dword ptr [R11+0xc4], imm32
                # 41 81 8b c4 00 00 00 <imm32>   OR  dword ptr [R11+0xc4], imm32
                expected = (
                    b"\x41\x81\xa3\xc4\x00\x00\x00"
                    + struct.pack("<I", and_mask)
                    + b"\x41\x81\x8b\xc4\x00\x00\x00"
                    + struct.pack("<I", or_mask)
                )
                self.assertEqual(got, expected)

    def test_advance_tail_adds_header_size_plus_args(self):
        # 1400070e3  MOVZX ECX,byte ptr [R10+0x3]   41 0f b6 4a 03
        # 1400070e8  ADD RCX,0x4                    48 83 c1 04
        offset = self.va_to_file_offset(self.spec.ADVANCE_TAIL)
        self.assertEqual(self.data[offset : offset + 5], b"\x41\x0f\xb6\x4a\x03")
        self.assertEqual(self.data[offset + 5 : offset + 9], b"\x48\x83\xc1\x04")
        self.assertEqual(self.data[offset + 8], self.spec.INSTR_HEADER_SIZE)


class Th06ncAnmDataTests(unittest.TestCase):
    """The data side: what the shipped .anm files actually contain."""

    def setUp(self):
        if not EXTRACTED.is_dir():
            self.skipTest("extracted archives not present (gitignored)")
        self.spec = load_spec()
        result = subprocess.run(
            [sys.executable, str(SCANNER), str(EXTRACTED)],
            capture_output=True,
            text=True,
            check=True,
        )
        self.out = result.stdout

    def parse_counts(self):
        counts = {}
        for line in self.out.splitlines():
            parts = line.split()
            if len(parts) >= 3 and parts[0].isdigit() and parts[1].isdigit():
                counts[int(parts[0])] = (int(parts[1]), line[line.index("[") :])
        return counts

    def test_no_shipped_script_uses_an_opcode_above_the_dispatch_bound(self):
        counts = self.parse_counts()
        self.assertTrue(counts, "scanner produced no histogram")
        self.assertLessEqual(max(counts), self.spec.MAX_OPCODE)

    def test_new_opcodes_are_argument_free_and_match_recorded_counts(self):
        counts = self.parse_counts()
        for op, expected in self.spec.SAMPLE_USE_COUNTS.items():
            with self.subTest(opcode=op):
                if expected == 0:
                    self.assertNotIn(op, counts)
                    continue
                got, arglens = counts[op]
                self.assertEqual(got, expected)
                self.assertTrue(arglens.startswith("[0]"), arglens)


if __name__ == "__main__":
    unittest.main()
