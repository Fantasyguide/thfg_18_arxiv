import hashlib
import importlib.util
import struct
import unittest
from pathlib import Path


REPO = Path(__file__).resolve().parents[3]
SPEC_PATH = REPO / "tooling" / "ghidra" / "th06nc_menu_spec.py"
SAMPLE = REPO / "local" / "th06nc" / "th06nc.exe"


def load_spec():
    module_spec = importlib.util.spec_from_file_location("th06nc_menu_spec", SPEC_PATH)
    module = importlib.util.module_from_spec(module_spec)
    module_spec.loader.exec_module(module)
    return module


class Th06ncMenuSpecTests(unittest.TestCase):
    def test_password_and_key_table_agree(self):
        spec = load_spec()
        self.assertEqual(spec.CHEAT_PASSWORD, "classiclager")
        self.assertEqual(len(spec.CHEAT_PASSWORD), 12)
        self.assertEqual(
            [letter for letter, _dik in spec.CHEAT_KEY_BITS],
            ["c", "l", "a", "s", "i", "g", "e", "r"],
        )

    def test_cheat_gates_on_lunatic_not_extra_like_th06(self):
        spec = load_spec()
        self.assertEqual(spec.CHEAT_GATE_STATE, 6)
        self.assertEqual(spec.RESULT_SCREEN_STATES[6], "LUNATIC_RECORDS")
        self.assertEqual(spec.RESULT_SCREEN_STATES[7], "EXTRA_RECORDS")

    def test_pad_cheat_is_a_six_bit_xinput_sequence(self):
        spec = load_spec()
        self.assertEqual([bit for bit, _name in spec.PAD_CHEAT_BITS], [1, 2, 4, 8, 0x10, 0x20])
        self.assertEqual(
            [(advance, abort) for _step, advance, abort in spec.PAD_CHEAT_STEPS],
            [(0x03, 0x00), (0x08, 0x34), (0x04, 0x38), (0x30, 0x0C)],
        )

    def test_unconfirmed_semantics_stay_marked(self):
        spec = load_spec()
        blocked = dict((address, comment) for address, _name, comment in spec.GLOBALS)[0x140509694]
        self.assertIn("unconfirmed", blocked.lower())
        self.assertEqual(spec.validate(), [])


class Th06ncMenuSampleTests(unittest.TestCase):
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
            virtual_size, virtual_address, raw_size, raw_pointer = struct.unpack_from("<IIII", data, header + 8)
            if virtual_address <= rva < virtual_address + max(virtual_size, raw_size):
                return raw_pointer + (rva - virtual_address)
        raise AssertionError("VA 0x%x is outside every section" % va)

    def test_sample_matches_the_expected_build(self):
        self.assertEqual(hashlib.md5(self.data).hexdigest(), self.spec.EXPECTED_MD5)

    def test_password_bytes_are_where_the_spec_says(self):
        offset = self.va_to_file_offset(self.spec.CHEAT_PASSWORD_ADDRESS)
        self.assertEqual(self.data[offset : offset + 13], b"classiclager\x00")

    def test_key_table_bytes_match_the_spec(self):
        offset = self.va_to_file_offset(self.spec.CHEAT_KEY_TABLE_ADDRESS)
        got = []
        for index in range(8):
            ch, dik = struct.unpack_from("<ii", self.data, offset + index * 8)
            got.append((chr(ch), dik))
        self.assertEqual(got, list(self.spec.CHEAT_KEY_BITS))

    def test_only_state_six_reaches_the_cheat_call(self):
        table = self.va_to_file_offset(0x140072824)
        targets = {}
        for state in range(18):
            rva = struct.unpack_from("<I", self.data, table + state * 4)[0]
            targets[state] = 0x140000000 + rva
        self.assertEqual(targets[6], 0x14006FAB4)
        for state in (3, 4, 5, 7):
            self.assertEqual(targets[state], 0x14006FAC3)

    def test_state_six_entry_calls_the_cheat_handler(self):
        offset = self.va_to_file_offset(0x14006FAB4)
        # 48 8b ce             MOV RCX,RSI
        # e8 <rel32>           CALL ResultScreen__UpdateUnlockCheats
        self.assertEqual(self.data[offset : offset + 3], b"\x48\x8b\xce")
        self.assertEqual(self.data[offset + 3], 0xE8)
        rel = struct.unpack_from("<i", self.data, offset + 4)[0]
        self.assertEqual(0x14006FAB4 + 8 + rel, 0x14006E020)


if __name__ == "__main__":
    unittest.main()
