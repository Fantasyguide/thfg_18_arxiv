#!/usr/bin/env python3
"""Contract tests for the TH18 controller polling patch."""

import hashlib
import json
import struct
import unittest
from pathlib import Path


HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
GAME_EXE = ROOT / "local/th18.v1.00a/th18.exe"
PATCH_DIR = HERE / "patch"
MODKIT = ROOT / "local/vendor/th18_modkit"
PUBLISHED = MODKIT / "thcrap/repos/Renko_1055/th18_disable_controller_polling"

GAME_SHA256 = "5aeb74b19939a29a8cf06e4fe7777aaf9139e93478507e039f43318171020e9f"
XINPUT_TARGET_VA = 0x401860
XINPUT_EXPECTED = bytes.fromhex("55 8b ec")
XINPUT_REPLACEMENT = bytes.fromhex("31 c0 c3")
DIRECTINPUT_TARGET_VA = 0x401650
DIRECTINPUT_EXPECTED = bytes.fromhex("55 8b ec 81 ec 20 01 00 00")
DIRECTINPUT_REPLACEMENT = bytes.fromhex("8b 44 24 04 c2 04 00 90 90")


def load_json(path: Path):
    return json.loads(path.read_text(encoding="utf-8-sig"))


def va_to_file_offset(image: bytes, va: int) -> int:
    pe = struct.unpack_from("<I", image, 0x3C)[0]
    image_base = struct.unpack_from("<I", image, pe + 24 + 28)[0]
    rva = va - image_base
    section_count = struct.unpack_from("<H", image, pe + 6)[0]
    optional_size = struct.unpack_from("<H", image, pe + 20)[0]
    sections = pe + 24 + optional_size
    for index in range(section_count):
        section = sections + index * 40
        virtual_size, virtual_address, raw_size, raw_offset = struct.unpack_from(
            "<IIII", image, section + 8
        )
        if virtual_address <= rva < virtual_address + max(virtual_size, raw_size):
            return raw_offset + rva - virtual_address
    raise ValueError(f"VA {va:#x} is outside the PE sections")


def imports_by_iat(image: bytes):
    pe = struct.unpack_from("<I", image, 0x3C)[0]
    optional = pe + 24
    image_base = struct.unpack_from("<I", image, optional + 28)[0]
    import_rva = struct.unpack_from("<I", image, optional + 96 + 8)[0]

    def rva_offset(rva):
        return va_to_file_offset(image, image_base + rva)

    def c_string(offset):
        end = image.index(0, offset)
        return image[offset:end].decode("ascii")

    result = {}
    descriptor = rva_offset(import_rva)
    while any(struct.unpack_from("<IIIII", image, descriptor)):
        lookup, _, _, name_rva, first_thunk = struct.unpack_from(
            "<IIIII", image, descriptor
        )
        dll = c_string(rva_offset(name_rva))
        lookup = lookup or first_thunk
        index = 0
        while True:
            entry = struct.unpack_from("<I", image, rva_offset(lookup) + index * 4)[0]
            if entry == 0:
                break
            symbol = (
                f"ordinal {entry & 0xFFFF}"
                if entry & 0x80000000
                else c_string(rva_offset(entry) + 2)
            )
            result[image_base + first_thunk + index * 4] = (dll, symbol)
            index += 1
        descriptor += 20
    return result


class PatchContractTests(unittest.TestCase):
    def test_retail_executable_matches_audited_entry_point(self):
        image = GAME_EXE.read_bytes()
        self.assertEqual(hashlib.sha256(image).hexdigest(), GAME_SHA256)
        for va, expected in (
            (XINPUT_TARGET_VA, XINPUT_EXPECTED),
            (DIRECTINPUT_TARGET_VA, DIRECTINPUT_EXPECTED),
        ):
            offset = va_to_file_offset(image, va)
            self.assertEqual(image[offset : offset + len(expected)], expected)
        imports = imports_by_iat(image)
        self.assertEqual(imports[0x4AD280], ("XINPUT1_3.dll", "ordinal 2"))
        self.assertEqual(imports[0x4AD278], ("WINMM.dll", "joyGetPosEx"))
        self.assertEqual(imports[0x4AD1D0], ("USER32.dll", "GetKeyboardState"))

    def test_patch_disables_xinput_and_directinput_polling(self):
        metadata = load_json(PATCH_DIR / "patch.js")
        game_patch = load_json(PATCH_DIR / "th18.v1.00a.js")
        self.assertEqual(metadata["id"], "th18_disable_controller_polling")
        self.assertEqual(metadata["dependencies"], ["nmlgc/base_tsa"])
        self.assertEqual(
            game_patch,
            {
                "binhacks": {
                    "disable_xinput_polling": {
                        "addr": "0x401860",
                        "expected": XINPUT_EXPECTED.hex(),
                        "code": XINPUT_REPLACEMENT.hex(),
                    },
                    "disable_directinput_polling": {
                        "addr": "0x401650",
                        "expected": DIRECTINPUT_EXPECTED.hex(),
                        "code": DIRECTINPUT_REPLACEMENT.hex(),
                    },
                }
            },
        )

    def test_published_patch_is_byte_for_byte_identical(self):
        for name in ("patch.js", "th18.v1.00a.js", "files.js"):
            self.assertEqual((PATCH_DIR / name).read_bytes(), (PUBLISHED / name).read_bytes())

        default_state = load_json(MODKIT / "mods/enabled.default.json")
        self.assertNotIn("th18_disable_controller_polling", default_state["patches"])
        repository = load_json(MODKIT / "thcrap/repos/Renko_1055/repo.js")
        self.assertIn("th18_disable_controller_polling", repository["patches"])


if __name__ == "__main__":
    unittest.main()
